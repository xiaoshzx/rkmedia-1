// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include "buffer.h"
#include "decoder.h"
#include "ffmpeg_utils.h"
#include "media_type.h"

namespace easymedia {

// A decoder which call the ffmpeg interface.
class FFMPEGAudioDecoder : public AudioDecoder {
public:
  FFMPEGAudioDecoder(const char *param);
  virtual ~FFMPEGAudioDecoder();
  static const char *GetCodecName() { return "ffmpeg_aud"; }
  virtual bool Init() override;
  virtual bool InitConfig(const MediaConfig &cfg) override;
  virtual int Process(const std::shared_ptr<MediaBuffer> &,
                      std::shared_ptr<MediaBuffer> &,
                      std::shared_ptr<MediaBuffer>) override {
    errno = ENOSYS;
    return -1;
  }
  virtual int SendInput(const std::shared_ptr<MediaBuffer> &input) override;
  virtual std::shared_ptr<MediaBuffer> FetchOutput() override;

private:
  AVCodec *av_codec;
  AVCodecContext *avctx;
  AVCodecParserContext *parser;
  AVPacket *avpkt;
  enum AVSampleFormat output_fmt;
  std::string input_data_type;
  std::string ff_codec_name;
  bool need_parser;
};

FFMPEGAudioDecoder::FFMPEGAudioDecoder(const char *param)
    : av_codec(nullptr), avctx(nullptr), parser(nullptr),
      output_fmt(AV_SAMPLE_FMT_NONE), need_parser(true) {
  printf("%s:%d.\n", __func__, __LINE__);
  std::map<std::string, std::string> params;
  std::list<std::pair<const std::string, std::string &>> req_list;
  req_list.push_back(std::pair<const std::string, std::string &>(
      KEY_INPUTDATATYPE, input_data_type));
  req_list.push_back(
      std::pair<const std::string, std::string &>(KEY_NAME, ff_codec_name));
  parse_media_param_match(param, params, req_list);
}

FFMPEGAudioDecoder::~FFMPEGAudioDecoder() {
  if (avpkt) {
    av_packet_free(&avpkt);
  }
  if (avctx) {
    avcodec_free_context(&avctx);
  }
  if (parser) {
    av_parser_close(parser);
  }
}

bool FFMPEGAudioDecoder::Init() {
  if (input_data_type.empty()) {
    LOG("missing %s\n", KEY_INPUTDATATYPE);
    return false;
  }
  codec_type = StringToCodecType(input_data_type.c_str());
  if (codec_type == CODEC_TYPE_G711A || codec_type == CODEC_TYPE_G711U)
    need_parser = false;
  if (!ff_codec_name.empty()) {
    av_codec = avcodec_find_decoder_by_name(ff_codec_name.c_str());
  } else {
    AVCodecID id = CodecTypeToAVCodecID(codec_type);
    av_codec = avcodec_find_decoder(id);
  }
  if (!av_codec) {
    LOG("Fail to find ffmpeg codec, request codec name=%s, or format=%s\n",
        ff_codec_name.c_str(), input_data_type.c_str());
    return false;
  }

  if (need_parser) {
    parser = av_parser_init(av_codec->id);
    if (!parser) {
      LOG("Parser not found\n");
      return false;
    }
  }

  avctx = avcodec_alloc_context3(av_codec);
  if (!avctx) {
    LOG("Fail to avcodec_alloc_context3\n");
    return false;
  }
  LOG("av codec name=%s\n",
      av_codec->long_name ? av_codec->long_name : av_codec->name);
  return true;
}

bool FFMPEGAudioDecoder::InitConfig(const MediaConfig &cfg) {
  if (!need_parser) {
    avctx->channels = cfg.aud_cfg.sample_info.channels;
    avctx->sample_rate = cfg.aud_cfg.sample_info.sample_rate;
  }
  int av_ret = avcodec_open2(avctx, av_codec, NULL);
  LOG("InitConfig channels = %d, sample_rate = %d. sample_fmt = %d.\n",
      avctx->channels, avctx->sample_rate, avctx->sample_fmt);
  if (av_ret < 0) {
    PrintAVError(av_ret, "Fail to avcodec_open2", av_codec->long_name);
    return false;
  }
  auto mc = cfg;
  mc.type = Type::Audio;
  mc.aud_cfg.codec_type = codec_type;

  avpkt = av_packet_alloc();
  if (!avpkt) {
    fprintf(stderr, "Could not allocate audio pkt\n");
    return false;
  }

  return AudioDecoder::InitConfig(mc);
}

int FFMPEGAudioDecoder::SendInput(const std::shared_ptr<MediaBuffer> &input) {
  int ret = -1;
  if (input->IsValid()) {
    assert(input && input->GetType() == Type::Audio);
    uint8_t *data = (uint8_t *)input->GetPtr();
    uint64_t data_size = input->GetValidSize();
    if (data_size > 0) {
      if (need_parser) {
        ret = av_parser_parse2(parser, avctx, &avpkt->data, &avpkt->size, data,
                               data_size, AV_NOPTS_VALUE, AV_NOPTS_VALUE, 0);
        if (ret < 0) {
          fprintf(stderr, "Error while parsing\n");
          return -1;
        }
        data += ret;
        data_size -= ret;
        input->SetValidSize(data_size);
        input->SetPtr(data);
      } else {
        avpkt->data = data;
        avpkt->size = data_size;
        input->SetValidSize(0);
        input->SetPtr(data);
      }

      if (avpkt->size) {
        ret = avcodec_send_packet(avctx, avpkt);
        if (ret < 0) {
          LOG("Audio data lost!, ret = %d.\n", ret);
        }
      }
    }
  } else {
    LOG(" will use avcodec_send_packet send nullptr to fflush decoder.\n");
    ret = avcodec_send_packet(avctx, nullptr);
  }
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN))
      return -EAGAIN;
    PrintAVError(ret, "Error submitting the packet to the decoder",
                 av_codec->long_name);
    return -1;
  }
  return 0;
}

static int __ffmpeg_frame_free(void *arg) {
  auto frame = (AVFrame *)arg;
  av_frame_free(&frame);
  return 0;
}

std::shared_ptr<MediaBuffer> FFMPEGAudioDecoder::FetchOutput() {
  int ret = -1;
  auto frame = av_frame_alloc();
  if (!frame)
    return nullptr;
  std::shared_ptr<MediaBuffer> buffer = std::make_shared<MediaBuffer>(
      frame->data, 0, -1, frame, __ffmpeg_frame_free);
  ret = avcodec_receive_frame(avctx, frame);
  if (ret < 0) {
    if (ret == AVERROR(EAGAIN)) {
      errno = EAGAIN;
      return nullptr;
    } else if (ret == AVERROR_EOF) {
      buffer->SetEOF(true);
      return buffer;
    }
    errno = EFAULT;
    PrintAVError(ret, "Fail to receive frame from decoder",
                 av_codec->long_name);
    return nullptr;
  }

  auto data_size = av_get_bytes_per_sample(avctx->sample_fmt);
  if (data_size < 0) {
    /* This should not occur, checking just for paranoia */
    fprintf(stderr, "Failed to calculate data size\n");
    return nullptr;
  }
  LOGD("decode [%d]-[%d]-[%d]-[%d]\n", ret, frame->nb_samples, data_size,
       avctx->channels);
  buffer->SetPtr(frame->data[0]);
  buffer->SetValidSize(data_size * frame->nb_samples * avctx->channels);
  buffer->SetUSTimeStamp(frame->pts);
  buffer->SetType(Type::Audio);

  if (av_codec->id == AV_CODEC_ID_AAC) {
    // from FLTP to S16P
    int buffer_size = avctx->channels *
                      av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) *
                      avctx->frame_size;
    std::shared_ptr<MediaBuffer> buffer_s16p =
        std::make_shared<MediaBuffer>(MediaBuffer::Alloc2(buffer_size));
    uint8_t *pi = (uint8_t *)buffer->GetPtr();
    uint8_t *po = (uint8_t *)buffer_s16p->GetPtr();
    int os = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
    int is = av_get_bytes_per_sample(avctx->sample_fmt);
    conv_AV_SAMPLE_FMT_FLT_to_AV_SAMPLE_FMT_S16(po, pi, is, os,
                                                po + buffer_size);
    // from S16P to S16
    if (avctx->channels > 1) {
      SampleInfo sampleinfo;
      sampleinfo.fmt = SAMPLE_FMT_S16;
      sampleinfo.channels = avctx->channels;
      sampleinfo.nb_samples = avctx->frame_size;
      std::shared_ptr<MediaBuffer> buffer_s16 =
          std::make_shared<MediaBuffer>(MediaBuffer::Alloc2(buffer_size));
      conv_planar_to_package((uint8_t *)buffer_s16->GetPtr(),
                             (uint8_t *)buffer_s16p->GetPtr(), sampleinfo);
      buffer_s16p = buffer_s16;
    }
    buffer_s16p->SetValidSize(buffer_size);
    buffer_s16p->SetUSTimeStamp(buffer->GetUSTimeStamp());
    buffer_s16p->SetType(Type::Audio);
    buffer = buffer_s16p;
  }
  return buffer;
}

DEFINE_AUDIO_DECODER_FACTORY(FFMPEGAudioDecoder)
const char *FACTORY(FFMPEGAudioDecoder)::ExpectedInputDataType() {
  return AUDIO_PCM;
}
const char *FACTORY(FFMPEGAudioDecoder)::OutPutDataType() {
  return TYPE_ANYTHING;
}

} // namespace easymedia
