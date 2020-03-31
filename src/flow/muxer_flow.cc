// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <inttypes.h>
#include <sys/time.h>

#include "buffer.h"
#include "codec.h"
#include "flow.h"
#include "muxer.h"
#include "muxer_flow.h"
#include "utils.h"

#include "fcntl.h"
#include "stdint.h"
#include "stdio.h"
#include "unistd.h"

#include <sstream>

namespace easymedia {

MuxerFlow::MuxerFlow(const char *param)
    : video_recorder(nullptr), video_in(false), audio_in(false),
      file_duration(-1), file_index(-1), file_time_en(false) {
  std::list<std::string> separate_list;
  std::map<std::string, std::string> params;

  if (!ParseWrapFlowParams(param, params, separate_list)) {
    SetError(-EINVAL);
    return;
  }

  std::string &muxer_name = params[KEY_NAME];
  if (muxer_name.empty()) {
    LOG("missing muxer name\n");
    SetError(-EINVAL);
    return;
  }

  file_path = params[KEY_PATH];
  if (file_path.empty()) {
    LOG("Muxer will use internal path\n");
  }

  file_prefix = params[KEY_FILE_PREFIX];
  if (file_prefix.empty()) {
    LOG("Muxer will use default prefix\n");
  }

  std::string time_str = params[KEY_FILE_TIME];
  if (!time_str.empty()) {
    file_time_en = !!std::stoi(time_str);
    LOG("Muxer will record video end with time\n");
  }

  std::string index_str = params[KEY_FILE_INDEX];
  if (!index_str.empty()) {
    file_index = std::stoi(index_str);
    LOG("Muxer will record video start with index %" PRId64"\n", file_index);
  }

  std::string &duration_str = params[KEY_FILE_DURATION];
  if (!duration_str.empty()) {
    file_duration = std::stoi(duration_str);
    LOG("Muxer will save video file per %" PRId64"sec\n", file_duration);
  }

  for (auto param_str : separate_list) {
    MediaConfig enc_config;
    std::map<std::string, std::string> enc_params;
    if (!parse_media_param_map(param_str.c_str(), enc_params)) {
      continue;
    }

    if (!ParseMediaConfigFromMap(enc_params, enc_config)) {
      continue;
    }

    if (enc_config.type == Type::Video) {
      vid_enc_config = enc_config;
      video_in = true;
      LOG("Found video encode config!\n");
    } else if (enc_config.type == Type::Audio) {
      aud_enc_config = enc_config;
      audio_in = true;
      LOG("Found audio encode config!\n");
    }
  }

  std::string token;
  std::istringstream tokenStream(param);
  std::getline(tokenStream, token, FLOW_PARAM_SEPARATE_CHAR);
  muxer_param = token;

  SlotMap sm;
  sm.input_slots.push_back(0);
  sm.input_slots.push_back(1);
  sm.thread_model = Model::ASYNCCOMMON;
  sm.mode_when_full = InputMode::DROPFRONT;
  sm.input_maxcachenum.push_back(10);
  sm.input_maxcachenum.push_back(20);
  sm.fetch_block.push_back(false);
  sm.fetch_block.push_back(false);
  sm.process = save_buffer;

  if (!InstallSlotMap(sm, "MuxerFlow", 0)) {
    LOG("Fail to InstallSlotMap for MuxerFlow\n");
    return;
  }
}

MuxerFlow::~MuxerFlow() { StopAllThread(); }

static std::shared_ptr<MediaBuffer>
prepare_h264_intra_frame(std::shared_ptr<MediaBuffer> &buf,
  int64_t timestamp, int offset_only) {
  const uint8_t *p = (uint8_t *)buf->GetPtr();
  const uint8_t *end = p + buf->GetValidSize();
  const uint8_t *nal_start = nullptr, *nal_end = nullptr;
  const uint8_t *sps_start = nullptr;

  nal_start = find_nalu_startcode(p, end);
  // 00 00 01 or 00 00 00 01
  size_t start_len = (nal_start[2] == 1 ? 3 : 4);
  for (;;) {
    if (nal_start == end)
      break;
    nal_start += start_len;
    nal_end = find_nalu_startcode(nal_start, end);
    uint8_t nal_type = (*nal_start) & 0x1F;
    uint32_t flag;
    switch (nal_type) {
    case 7: //sps
      sps_start = nal_start - start_len;
      nal_start = nal_end;
      continue;
    case 8: //pps
      flag = MediaBuffer::kExtraIntra;
      break;
    default:
      flag = 0;
    }

    // not extraIntra?
    if (!flag || !sps_start)
      break;

    size_t size = nal_end - sps_start;
    if (offset_only) {
      buf->SetPtr((uint8_t *)buf->GetPtr() + size);
      buf->SetValidSize(buf->GetValidSize() - size);
      break; // break for loop.
    }

    auto sub_buffer = MediaBuffer::Alloc(size);
    if (!sub_buffer) {
      LOG_NO_MEMORY(); // fatal error
      return nullptr;
    }
    memcpy(sub_buffer->GetPtr(), sps_start, size);
    sub_buffer->SetValidSize(size);
    sub_buffer->SetUserFlag(flag);
    sub_buffer->SetUSTimeStamp(timestamp);
    sub_buffer->SetType(Type::Video);

    buf->SetPtr((uint8_t *)buf->GetPtr() + size);
    buf->SetValidSize(buf->GetValidSize() - size);

    return sub_buffer;
  }

  return nullptr;
}

static std::shared_ptr<MediaBuffer>
prepare_h265_intra_frame(std::shared_ptr<MediaBuffer> &buf,
  int64_t timestamp, int offset_only) {
  const uint8_t *p = (uint8_t *)buf->GetPtr();
  const uint8_t *end = p + buf->GetValidSize();
  const uint8_t *nal_start = nullptr, *nal_end = nullptr;
  const uint8_t *vps_start = nullptr;

  nal_start = find_nalu_startcode(p, end);
  // 00 00 01 or 00 00 00 01
  size_t start_len = (nal_start[2] == 1 ? 3 : 4);
  for (;;) {
    if (nal_start == end)
      break;
    nal_start += start_len;
    nal_end = find_nalu_startcode(nal_start, end);
    uint8_t nal_type = ((*nal_start) & 0x7E) >> 1;
    uint32_t flag;
    switch (nal_type) {
    case 32: //vps
      vps_start = nal_start - start_len;
      nal_start = nal_end;
      continue;
    case 33: //sps
      nal_start = nal_end;
      continue;
    case 34: //pps
      flag = MediaBuffer::kExtraIntra;
      break;
    default:
      flag = 0;
    }

    // not extraIntra?
    if (!flag || !vps_start)
      break;

    size_t size = nal_end - vps_start;
    if (offset_only) {
      buf->SetPtr((uint8_t *)buf->GetPtr() + size);
      buf->SetValidSize(buf->GetValidSize() - size);
      break; // break for loop.
    }

    auto sub_buffer = MediaBuffer::Alloc(size);
    if (!sub_buffer) {
      LOG_NO_MEMORY(); // fatal error
      return nullptr;
    }
    memcpy(sub_buffer->GetPtr(), vps_start, size);
    sub_buffer->SetValidSize(size);
    sub_buffer->SetUserFlag(flag);
    sub_buffer->SetUSTimeStamp(timestamp);
    sub_buffer->SetType(Type::Video);

    buf->SetPtr((uint8_t *)buf->GetPtr() + size);
    buf->SetValidSize(buf->GetValidSize() - size);

    return sub_buffer;
  }

  return nullptr;
}

std::shared_ptr<VideoRecorder> MuxerFlow::NewRecoder(const char *path) {
  std::string param = std::string(muxer_param);
  PARAM_STRING_APPEND(param, KEY_PATH, path);
  auto vrecorder = std::make_shared<VideoRecorder>(param.c_str());

  if (!vrecorder) {
    LOG("Create video recoder failed, path:[%s]\n", path);
    return nullptr;
  } else {
    LOG("Ready to recod new video file path:[%s]\n", path);
  }

  return vrecorder;
}

std::string MuxerFlow::GenFilePath() {
  std::ostringstream ostr;

  // if user special a file path then use it.
  if (!file_path.empty()) {
    return file_path;
  }

  if (!file_prefix.empty()) {
    ostr << file_prefix;
  }

  if (file_time_en) {
    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    char time_str[128] = {0};

    snprintf(time_str, 128, "_%d%02d%02d%02d%02d%02d", tm.tm_year + 1900,
             tm.tm_mon + 1, tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);

    ostr << time_str;
  }

  if (file_index > 0) {
    ostr << "_" << file_index;
    file_index++;
  }

  ostr << ".mp4";

  return ostr.str();
}

bool save_buffer(Flow *f, MediaBufferVector &input_vector) {
  MuxerFlow *flow = static_cast<MuxerFlow *>(f);
  auto &&recoder = flow->video_recorder;
  int64_t duration_us = flow->file_duration;

  if (recoder == nullptr) {
    recoder = flow->NewRecoder(flow->GenFilePath().c_str());
    flow->last_ts = 0;
  }

  // process audio stream here
  do {
    if (!flow->audio_in) {
      break;
    }

    auto &aud_buffer = input_vector[1];

    if (aud_buffer == nullptr) {
      break;
    }

    if (!recoder->Write(flow, aud_buffer)) {
      recoder.reset();
      return true;
    }
  } while (0);

  // process video stream here
  do {
    if (!flow->video_in) {
      break;
    }

    auto &vid_buffer = input_vector[0];

    if (vid_buffer == nullptr) {
      break;
    }

    if ((vid_buffer->GetUserFlag() & MediaBuffer::kIntra)) {
      CodecType codec_t =
        flow->vid_enc_config.vid_cfg.image_cfg.codec_type;
      if (!flow->video_extra) {
        if (codec_t == CODEC_TYPE_H264) {
          flow->video_extra = prepare_h264_intra_frame(vid_buffer,
            easymedia::gettimeofday(), 0);
        } else if (codec_t == CODEC_TYPE_H265) {
          flow->video_extra = prepare_h265_intra_frame(vid_buffer,
            easymedia::gettimeofday(), 0);
        }
        if (!flow->video_extra) {
          LOG("ERROR: Muxer Flow: Intra Frame without sps pps\n");
        }
      } else {
        if (codec_t == CODEC_TYPE_H264)
          prepare_h264_intra_frame(vid_buffer, easymedia::gettimeofday(), 1);
        else if (codec_t == CODEC_TYPE_H265)
          prepare_h265_intra_frame(vid_buffer, easymedia::gettimeofday(), 1);
      }
    }

    if (!recoder->Write(flow, vid_buffer)) {
      recoder.reset();
      return true;
    }

    if (flow->last_ts == 0 || vid_buffer->GetUSTimeStamp() < flow->last_ts) {
      flow->last_ts = vid_buffer->GetUSTimeStamp();
    }

    if (duration_us <= 0) {
      break;
    }

    if (vid_buffer->GetUSTimeStamp() - flow->last_ts >= duration_us * 1000000) {
      recoder.reset();
      recoder = nullptr;
    }
  } while (0);

  return true;
}

DEFINE_FLOW_FACTORY(MuxerFlow, Flow)
const char *FACTORY(MuxerFlow)::ExpectedInputDataType() { return nullptr; }
const char *FACTORY(MuxerFlow)::OutPutDataType() { return ""; }

VideoRecorder::VideoRecorder(const char *param)
    : vid_stream_id(-1), aud_stream_id(-1) {
  muxer = easymedia::REFLECTOR(Muxer)::Create<easymedia::Muxer>(
      "ffmpeg", param);

  if (!muxer) {
    LOG("Create muxer ffmpeg failed\n");
    exit(EXIT_FAILURE);
  }
}

VideoRecorder::~VideoRecorder() {
  if (vid_stream_id != -1) {
    auto buffer = easymedia::MediaBuffer::Alloc(1);
    buffer->SetEOF(true);
    buffer->SetValidSize(0);
    muxer->Write(buffer, vid_stream_id);
  }

  if (muxer) {
    muxer.reset();
  }
}

void VideoRecorder::ClearStream() {
  vid_stream_id = -1;
  aud_stream_id = -1;
}

bool VideoRecorder::Write(MuxerFlow *f, std::shared_ptr<MediaBuffer> buffer) {
  MuxerFlow *flow = static_cast<MuxerFlow *>(f);

  if (flow->video_in && flow->video_extra && vid_stream_id == -1) {
    if (!muxer->NewMuxerStream(flow->vid_enc_config, flow->video_extra,
                               vid_stream_id)) {
      LOG("NewMuxerStream failed for video\n");
    } else {
      LOG("Video: create video stream finished!\n");
    }

    if (flow->audio_in) {
      if (!muxer->NewMuxerStream(flow->aud_enc_config, nullptr,
                                 aud_stream_id)) {
        LOG("NewMuxerStream failed for audio\n");
      } else {
        LOG("Audio: create audio stream finished!\n");
      }
    }

    auto header = muxer->WriteHeader(vid_stream_id);
    if (!header) {
      LOG("WriteHeader on video stream return nullptr\n");
      ClearStream();
      return false;
    }
  }

  if (buffer->GetType() == Type::Video && vid_stream_id != -1) {
    if (nullptr == muxer->Write(buffer, vid_stream_id)) {
      LOG("Write on video stream return nullptr\n");
      ClearStream();
      return false;
    }
  } else if (buffer->GetType() == Type::Audio && aud_stream_id != -1) {
    if (nullptr == muxer->Write(buffer, aud_stream_id)) {
      LOG("Write on audio stream return nullptr\n");
      ClearStream();
      return false;
    }
  }

  return true;
}

} // namespace easymedia
