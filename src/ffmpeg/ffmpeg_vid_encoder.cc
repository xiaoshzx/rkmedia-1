// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "ffmpeg_vid_encoder.h"

#include <unordered_map>

#include "buffer.h"
#include "encoder.h"
#include "ffmpeg_utils.h"
#include "image.h"

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

namespace easymedia {

FFMpegVideoEncoder::FFMpegVideoEncoder(const char *param) {
  std::map<std::string, std::string> params;
  std::list<std::pair<const std::string, std::string &>> req_list;
  req_list.push_back(std::pair<const std::string, std::string &>(
      KEY_OUTPUTDATATYPE, OutputFormat_));
  req_list.push_back(
      std::pair<const std::string, std::string &>(KEY_NAME, CodecName_));
  parse_media_param_match(param, params, req_list);
}

FFMpegVideoEncoder::~FFMpegVideoEncoder() {
  avcodec_free_context(&Context_);
  av_frame_free(&frame);
  av_packet_free(&pkt);
}

bool FFMpegVideoEncoder::InitConfig(const MediaConfig &cfg) {
  GetConfig().vid_cfg = cfg.vid_cfg;
  GetConfig().type = Type::Video;

  Context_->pix_fmt =
      PixFmtToAVPixFmt(cfg.vid_cfg.image_cfg.image_info.pix_fmt);
  Context_->width = cfg.vid_cfg.image_cfg.image_info.width;
  Context_->height = cfg.vid_cfg.image_cfg.image_info.height;

  Context_->profile = cfg.vid_cfg.profile;
  Context_->level = cfg.vid_cfg.level;

  Context_->bit_rate = cfg.vid_cfg.bit_rate;
  if (!strncmp("cbr", cfg.vid_cfg.rc_mode, 3)) {
    Context_->rc_max_rate = cfg.vid_cfg.bit_rate;
    Context_->rc_min_rate = cfg.vid_cfg.bit_rate;
  }
  Context_->time_base = AVRational{1, cfg.vid_cfg.frame_rate};
  Context_->framerate = AVRational{cfg.vid_cfg.frame_rate, 1};
  Context_->gop_size = cfg.vid_cfg.gop_size;
  Context_->max_b_frames = 2;

  Context_->qmax = cfg.vid_cfg.qp_max;
  Context_->qmin = cfg.vid_cfg.qp_min;
  Context_->max_qdiff = cfg.vid_cfg.qp_step;
  // Context_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

  if (avcodec_open2(Context_, Codec_, NULL) < 0) {
    fprintf(stderr, "Codec cannot found\n");
    return false;
  }

  frame = av_frame_alloc();
  pkt = av_packet_alloc();

  frame->format = Context_->pix_fmt;
  frame->width = Context_->width;
  frame->height = Context_->height;

  return true;
}

bool FFMpegVideoEncoder::FFMpegVideoEncoder::Init() {
  if (OutputFormat_.empty()) {
    LOG("missing %s\n", KEY_OUTPUTDATATYPE);
    return false;
  }
  output_fmt = StringToPixFmt(OutputFormat_.c_str());
  if (!CodecName_.empty()) {
    if (CodecName_ == "h264_rkmpp") {
      LOG("EasyMedia using FFmpeg rkmpp hwaccel api is not implemented yet");
      return false;
    }
    Codec_ = avcodec_find_encoder_by_name(CodecName_.c_str());
  } else {
    CodecId_ = PixFmtToAVCodecID(output_fmt);
    Codec_ = avcodec_find_encoder(CodecId_);
  }
  if (!Codec_) {
    LOG("Fail to find ffmpeg codec, request codec name=%s, or format=%s\n",
        CodecName_.c_str(), OutputFormat_.c_str());
    return false;
  }
  Context_ = avcodec_alloc_context3(Codec_);
  if (!Context_) {
    LOG("Fail to avcodec_alloc_context3\n");
    return false;
  }

  return true;
}

int FFMpegVideoEncoder::Process(const std::shared_ptr<MediaBuffer> &input,
                                std::shared_ptr<MediaBuffer> &output,
                                std::shared_ptr<MediaBuffer> extra_output) {
  UNUSED(input);
  UNUSED(output);
  UNUSED(extra_output);
  return -1;
}

int FFMpegVideoEncoder::SendInput(const std::shared_ptr<MediaBuffer> &input) {
  int ret = 0;
#if 0
  ret = av_frame_make_writable(frame);
  if (ret < 0) {
    fprintf(stderr, "Could not mark the video frame data writable\n");
    return -1;
  }
#endif

  if (input->GetValidSize() > 0) {
#if 1
    frame->data[0] = (uint8_t *)input->GetPtr();
    frame->data[1] =
        (uint8_t *)(input->GetPtr()) + frame->width * frame->height;
    frame->data[2] = (uint8_t *)(input->GetPtr()) +
                     frame->width * frame->height +
                     frame->width * frame->height / 4;
#else
    memcpy(frame->data[0], (uint8_t *)input->GetPtr(),
           frame->width * frame->height);
    memcpy(frame->data[1],
           (uint8_t *)(input->GetPtr()) + frame->width * frame->height,
           frame->width * frame->height / 4);
    memcpy(frame->data[2],
           (uint8_t *)(input->GetPtr()) + frame->width * frame->height +
               frame->width * frame->height / 4,
           frame->width * frame->height / 4);
#endif
    // On ffmpeg 4.1.3, linesizes need to be manual configured.
    frame->linesize[0] = frame->width;
    frame->linesize[1] = frame->width / 2;
    frame->linesize[2] = frame->width / 2;
    frame->pts = input->GetUSTimeStamp();

    frame->format = Context_->pix_fmt;
    frame->width = Context_->width;
    frame->height = Context_->height;

    ret = avcodec_send_frame(Context_, frame);
  } else {
    ret = avcodec_send_frame(Context_, NULL);
  }
  if (ret < 0) {
    fprintf(stderr, "Error sending a frame for encoding\n");
    return -1;
  }

  return 0;
}

std::shared_ptr<MediaBuffer> FFMpegVideoEncoder::FetchOutput() {
  int ret = 0;
  ret = avcodec_receive_packet(Context_, pkt);
  if (ret == AVERROR(EAGAIN)) {
    errno = EAGAIN;
    return nullptr;
  } else if (ret == AVERROR_EOF) {
    auto buffer = MediaBuffer::Alloc(1);
    buffer->SetValidSize(0);
    buffer->SetEOF(true);
    return buffer;
  } else if (ret < 0) {
    errno = EFAULT;
    fprintf(stderr, "Error during encoding\n");
    return nullptr;
  }
#if 0
  static int i = 0;
  fprintf(stderr, "Write %s packet %3" PRId64 " (size=%5d)\n",
          ((pkt->flags & AV_PKT_FLAG_KEY) ? "I" : "P"), pkt->pts, pkt->size);
  char name[255] = "";
  snprintf(name, 255, "debug-%d.h264", i++);
  FILE *f = fopen(name, "wb+");
  fwrite(pkt->data, 1, pkt->size, f);
  fclose(f);
#endif
  auto buffer = MediaBuffer::Alloc(pkt->size);
  buffer->SetValidSize(pkt->size);
  memcpy(buffer->GetPtr(), pkt->data, pkt->size);
  buffer->SetUSTimeStamp(pkt->pts);
  buffer->SetUserFlag((pkt->flags & AV_PKT_FLAG_KEY) ? MediaBuffer::kIntra
                                                     : MediaBuffer::kPredicted);
  av_packet_unref(pkt);
  return buffer;
}

bool FFMpegVideoEncoder::CheckConfigChange(
    std::pair<uint32_t, std::shared_ptr<ParameterBuffer>>) {
  return true;
}
int FFMpegVideoEncoder::EncodeControl(int cmd, void *param) {
  UNUSED(cmd);
  UNUSED(param);
  return -1;
}

DEFINE_VIDEO_ENCODER_FACTORY(FFMpegVideoEncoder)
const char *FACTORY(FFMpegVideoEncoder)::ExpectedInputDataType() {
  return nullptr;
}
const char *FACTORY(FFMpegVideoEncoder)::OutPutDataType() { return VIDEO_H264; }

} // namespace easymedia
