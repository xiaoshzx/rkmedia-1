// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef NDEBUG
#undef NDEBUG
#endif
#ifndef DEBUG
#define DEBUG
#endif

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <cmath>
#include <string>
#include <unordered_map>

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavformat/avformat.h>
}

#include "buffer.h"
#include "encoder.h"
#include "key_string.h"
#include "muxer.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846 /* pi */
#endif

static const std::unordered_map<SampleFormat, AVSampleFormat> SampleFormatMap = {
    {SAMPLE_FMT_U8, AV_SAMPLE_FMT_U8},   {SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16},
    {SAMPLE_FMT_S32, AV_SAMPLE_FMT_S32}, {SAMPLE_FMT_AAC, AV_SAMPLE_FMT_FLTP},
    {SAMPLE_FMT_MP2, AV_SAMPLE_FMT_S16},
};

static void
fill_audio_sin_data(std::shared_ptr<easymedia::SampleBuffer> &buffer) {
  static float phase = 0;
  float tincr = 2 * M_PI * 440.0f / buffer->GetSampleInfo().sample_rate;

  // TODO: Add common sine wave generator 
  if (buffer->GetSampleFormat() == SAMPLE_FMT_S16 ||
  buffer->GetSampleFormat() == SAMPLE_FMT_MP2) {
    // short
    uint16_t *samples = (uint16_t *)buffer->GetPtr();
    for (int i = 0; i < buffer->GetSamples(); i++) {
      samples[2 * i] = (int)(std::sin(phase) * 10000);
      for (int k = 1; k < buffer->GetSampleInfo().channels; k++)
        samples[2 * i + k] = samples[2 * i];
      phase += tincr;
    }
  } else if (buffer->GetSampleFormat() == SAMPLE_FMT_S32 ||
             buffer->GetSampleFormat() == SAMPLE_FMT_AAC ||
             buffer->GetSampleFormat() == SAMPLE_FMT_VORBIS) {
    // float
    float *samples = (float *)buffer->GetPtr();
    for (int frame = 0; frame < buffer->GetSamples(); ++frame) {
      for (int channel = 0; channel < buffer->GetSampleInfo().channels;
           ++channel) {
        if (av_sample_fmt_is_planar(
               SampleFormatMap.at(buffer->GetSampleFormat()))) {
          samples[channel * buffer->GetSamples() + frame] = 0.2f * std::sin(phase);
        } else {
         samples[channel + buffer->GetSampleInfo().channels * frame] =
             0.2f * std::sin(phase);
        }
      }
      phase += tincr;
    }
  } else {
    assert(0);
  }
}

template <typename Encoder>
int encode(std::shared_ptr<easymedia::Muxer> mux,
           std::shared_ptr<Encoder> encoder,
           std::shared_ptr<easymedia::MediaBuffer> src,
           int stream_no) {
  auto enc = encoder;
  int ret = enc->SendInput(src);
  if (ret < 0) {
    fprintf(stderr, "[%d]: frame encode failed, ret=%d\n", stream_no, ret);
    return -1;
  }

  while (ret >= 0) {
    auto out = enc->FetchOutput();
    if (!out) {
      if (errno != EAGAIN) {
        fprintf(stderr, "[%d]: frame fetch failed, ret=%d\n", stream_no, errno);
        ret = errno;
      }
      break;
    }
    size_t out_len = out->GetValidSize();
    if (out_len == 0)
      break;
    fprintf(stderr, "[%d]: frame encoded, out %zu bytes\n\n", stream_no,
            out_len);
    mux->Write(out, stream_no);
  }

  return ret;
}

std::shared_ptr<easymedia::AudioEncoder>
initAudioEncoder(std::string OutFormat) {
  std::string param;
  PARAM_STRING_APPEND(param, KEY_OUTPUTDATATYPE, OutFormat);
  auto aud_enc = easymedia::REFLECTOR(Encoder)::Create<easymedia::AudioEncoder>(
      "ffmpeg_aud", param.c_str());
  if (!aud_enc) {
    fprintf(stderr, "Create ffmpeg_aud encoder failed\n");
    exit(EXIT_FAILURE);
  }

  // s16 2ch stereo, 1024 nb_samples
  SampleInfo aud_info = {SAMPLE_FMT_NONE, 2, 48000, 1024};
  aud_info.fmt = StringToSampleFmt(OutFormat.c_str());
  MediaConfig aud_enc_config;
  auto &ac = aud_enc_config.aud_cfg;
  ac.sample_info = aud_info;
  ac.bit_rate = 64000; // 64kbps
  aud_enc_config.type = Type::Audio;
  if (!aud_enc->InitConfig(aud_enc_config)) {
    fprintf(stderr, "Init config of ffmpeg_aud encoder failed\n");
    exit(EXIT_FAILURE);
  }

  return aud_enc;
}

std::shared_ptr<easymedia::VideoEncoder> initVideoEncoder(std::string EncoderName,
                                                          std::string SrcFormat,
                                                          std::string OutFormat,
                                                          int w, int h) {
  std::string param;
  PARAM_STRING_APPEND(param, KEY_OUTPUTDATATYPE, OutFormat);
  // If not rkmpp, then it is ffmpeg
  if (EncoderName == "ffmpeg_vid") {
    if (OutFormat == "video:h264") {
      PARAM_STRING_APPEND(param, KEY_NAME, "libx264");
    } else if (OutFormat == "video:h265") {
      PARAM_STRING_APPEND(param, KEY_NAME, "libx265");
    } else {
      exit(EXIT_FAILURE);
    }
  }
  auto vid_enc = easymedia::REFLECTOR(Encoder)::Create<easymedia::VideoEncoder>(
      EncoderName.c_str(), param.c_str());

  if (!vid_enc) {
    fprintf(stderr, "Create encoder %s failed\n", EncoderName.c_str());
    exit(EXIT_FAILURE);
  }

  PixelFormat fmt = PIX_FMT_NONE;
  if (SrcFormat == "nv12") {
    fmt = PIX_FMT_NV12;
  } else if (SrcFormat == "yuv420p") {
    fmt = PIX_FMT_YUV420P;
  } else {
    fprintf(stderr, "TO BE TESTED <%s:%s,%d>\n", __FILE__, __FUNCTION__,
            __LINE__);
    exit(EXIT_FAILURE);
  }

  // TODO SrcFormat and OutFormat use the same variable
  ImageInfo vid_info = {fmt, w, h, w, h};
  if (EncoderName == "rkmpp") {
    vid_info.vir_width = UPALIGNTO16(w);
    vid_info.vir_height = UPALIGNTO16(h);
  }
  MediaConfig vid_enc_config;
  if (OutFormat == VIDEO_H264 || OutFormat == VIDEO_H265) {
    VideoConfig &vid_cfg = vid_enc_config.vid_cfg;
    ImageConfig &img_cfg = vid_cfg.image_cfg;
    img_cfg.image_info = vid_info;
    img_cfg.qp_init = 24;
    vid_cfg.qp_step = 4;
    vid_cfg.qp_min = 12;
    vid_cfg.qp_max = 48;
    vid_cfg.bit_rate = w * h * 7;
    if (vid_cfg.bit_rate > 1000000) {
      vid_cfg.bit_rate /= 1000000;
      vid_cfg.bit_rate *= 1000000;
    }
    vid_cfg.frame_rate = 30;
    vid_cfg.level = 52;
    vid_cfg.gop_size = 10; // vid_cfg.frame_rate;
    vid_cfg.profile = 100;
    // vid_cfg.rc_quality = "aq_only"; vid_cfg.rc_mode = "vbr";
    vid_cfg.rc_quality = KEY_BEST;
    vid_cfg.rc_mode = KEY_CBR;
    vid_enc_config.type = Type::Video;
  } else {
    // TODO
    assert(0);
  }

  if (!vid_enc->InitConfig(vid_enc_config)) {
    fprintf(stderr, "Init config of encoder %s failed\n", EncoderName.c_str());
    exit(EXIT_FAILURE);
  }

  return vid_enc;
}

std::shared_ptr<easymedia::SampleBuffer> initAudioBuffer(MediaConfig &cfg) {
  auto &audio_info = cfg.aud_cfg.sample_info;
  fprintf(stderr, "sample number=%d\n", audio_info.nb_samples);
  int aud_size = GetSampleSize(audio_info) * audio_info.nb_samples;
  auto aud_mb = easymedia::MediaBuffer::Alloc2(aud_size);
  auto aud_buffer =
      std::make_shared<easymedia::SampleBuffer>(aud_mb, audio_info);
  aud_buffer->SetValidSize(aud_size);
  assert(aud_buffer && (int)aud_buffer->GetSize() >= aud_size);
  return aud_buffer;
}

std::shared_ptr<easymedia::MediaBuffer>
initVideoBuffer(std::string &EncoderName, ImageInfo &image_info) {
  // The vir_width/vir_height have aligned when init video encoder
  size_t len = CalPixFmtSize(image_info.pix_fmt, image_info.vir_width,
                             image_info.vir_height, 0);
  fprintf(stderr, "video buffer len %zu\n", len);
  // Just treat all aligned memory to be hardware memory
  // need to know rkmpp needs DRM managed memory,
  // but ffmpeg software encoder doesn't need.
  easymedia::MediaBuffer::MemType MemType =
      EncoderName == "rkmpp"
          ? easymedia::MediaBuffer::MemType::MEM_HARD_WARE
          : easymedia::MediaBuffer::MemType::MEM_COMMON;
  auto &&src_mb = easymedia::MediaBuffer::Alloc2(
      len, MemType);
  assert(src_mb.GetSize() > 0);
  auto src_buffer =
      std::make_shared<easymedia::ImageBuffer>(src_mb, image_info);
  assert(src_buffer && src_buffer->GetSize() >= len);

  return src_buffer;
}

std::shared_ptr<easymedia::Muxer>
initMuxer(std::shared_ptr<easymedia::VideoEncoder> vid_enc,
          std::shared_ptr<easymedia::AudioEncoder> aud_enc, int &vid_stream_no,
          int &aud_stream_no, std::string &output_path) {
  easymedia::REFLECTOR(Muxer)::DumpFactories();
  std::string param;
  PARAM_STRING_APPEND(param, KEY_PATH, output_path);
  auto mux = easymedia::REFLECTOR(Muxer)::Create<easymedia::Muxer>(
      "ffmpeg", param.c_str());
  if (!mux) {
    fprintf(stderr, "Create muxer ffmpeg failed\n");
    exit(EXIT_FAILURE);
  }

  if (!mux->NewMuxerStream(vid_enc->GetConfig(), vid_enc->GetExtraData(),
                           vid_stream_no)) {
    fprintf(stderr, "NewMuxerStream failed for video\n");
    exit(EXIT_FAILURE);
  }

  if (!mux->NewMuxerStream(aud_enc->GetConfig(), aud_enc->GetExtraData(),
                           aud_stream_no)) {
    fprintf(stderr, "NewMuxerStream failed for audio\n");
    exit(EXIT_FAILURE);
  }

  return mux;
}

static char optstr[] = "?i:o:w:h:e:c:";

int main(int argc, char **argv) {
  int c;
  std::string input_path;
  std::string output_path;
  int w = 0, h = 0;
  std::string input_format;
  std::string enc_format;
  std::string enc_codec_name = "rkmpp";   // rkmpp, ffmpeg_vid
  std::string aud_enc_format = AUDIO_MP2; // test mp2, aac

  opterr = 1;
  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
    case 'i':
      input_path = optarg;
      printf("input file path: %s\n", input_path.c_str());
      break;
    case 'o':
      output_path = optarg;
      printf("output file path: %s\n", output_path.c_str());
      break;
    case 'w':
      w = atoi(optarg);
      break;
    case 'h':
      h = atoi(optarg);
      break;
    case 'e': {
      char *cut = strchr(optarg, '_');
      if (!cut) {
        fprintf(stderr, "input/output format must be cut by \'_\'\n");
        exit(EXIT_FAILURE);
      }
      cut[0] = 0;
      input_format = optarg;
      enc_format = cut + 1;
      if (enc_format == "h264")
        enc_format = VIDEO_H264;
      else if (enc_format == "h265")
        enc_format = VIDEO_H265;
      printf("input format: %s\n", input_format.c_str());
      printf("enc format: %s\n", enc_format.c_str());
    } break;
    case 'c': {
      char *cut = strchr(optarg, ':');
      if (cut) {
        cut[0] = 0;
        std::string ff = optarg;
        if (ff == "ffmpeg")
          enc_codec_name = cut + 1;
      } else {
        enc_codec_name = optarg;
      }
    } break;
    case '?':
    default:
      printf("usage example: \n");
      printf("ffmpeg_enc_mux_test -i input.yuv -o output.mp4 -w 320 -h 240 -e "
             "nv12_h264 -c rkmpp\n");
      exit(0);
    }
  }
  if (input_path.empty() || output_path.empty())
    exit(EXIT_FAILURE);
  if (!w || !h)
    exit(EXIT_FAILURE);
  if (input_format.empty() || enc_format.empty())
    exit(EXIT_FAILURE);
  fprintf(stderr, "width, height: %d, %d\n", w, h);

  int input_file_fd = open(input_path.c_str(), O_RDONLY | O_CLOEXEC);
  assert(input_file_fd >= 0);
  unlink(output_path.c_str());

  // 1. encoder
  easymedia::REFLECTOR(Encoder)::DumpFactories();
  auto vid_enc =
      initVideoEncoder(enc_codec_name, input_format, enc_format, w, h);
  auto aud_enc = initAudioEncoder(aud_enc_format);
  auto aud_buffer = initAudioBuffer(aud_enc->GetConfig());
  auto src_buffer = initVideoBuffer(enc_codec_name, vid_enc->GetConfig().img_cfg.image_info);
  std::shared_ptr<easymedia::MediaBuffer> dst_buffer;
  if (enc_codec_name == "rkmpp") {
    size_t dst_len = CalPixFmtSize(vid_enc->GetConfig().img_cfg.image_info, 16);
    dst_buffer = easymedia::MediaBuffer::Alloc(
        dst_len, easymedia::MediaBuffer::MemType::MEM_HARD_WARE);
    assert(dst_buffer && dst_buffer->GetSize() >= dst_len);
  }

  // 2. muxer
  int vid_stream_no = -1;
  int aud_stream_no = -1;
  // TODO SrcFormat and OutFormat use the same variable
  vid_enc->GetConfig().img_cfg.image_info.pix_fmt =
      StringToPixFmt(enc_format.c_str());
  auto mux =
      initMuxer(vid_enc, aud_enc, vid_stream_no, aud_stream_no, output_path);

  auto header = mux->WriteHeader(vid_stream_no);
  if (!header) {
    fprintf(stderr, "WriteHeader on stream index %d return nullptr\n",
            vid_stream_no);
    exit(EXIT_FAILURE);
  }
  // for ffmpeg, WriteHeader once, this call only dump info
  //mux->WriteHeader(aud_stream_no);

  int64_t vinterval_per_frame =
      1000000LL /* us */ / vid_enc->GetConfig().vid_cfg.frame_rate;
  int64_t ainterval_per_frame =
      1000000LL /* us */ * aud_enc->GetConfig().aud_cfg.sample_info.nb_samples /
      aud_enc->GetConfig().aud_cfg.sample_info.sample_rate;

  ssize_t read_len;
  // TODO SrcFormat and OutFormat use the same variable
  vid_enc->GetConfig().vid_cfg.image_cfg.image_info.pix_fmt =
      StringToPixFmt(std::string("image:").append(input_format).c_str());
  // Since the input is packed yuv images, no padding buffer,
  // we want to read actual pixel size
  size_t len =
      CalPixFmtSize(vid_enc->GetConfig().vid_cfg.image_cfg.image_info, 0);

  int vid_index = 0;
  int first_video_time = 0;
  int aud_index = 0;
  int64_t first_audio_time = 0;
  while (true) {
    if (vid_index * vinterval_per_frame < aud_index * ainterval_per_frame) {
      // video
      read_len = read(input_file_fd, src_buffer->GetPtr(), len);
      if (read_len < 0) {
        // if 0 Continue to drain all encoded buffer
        fprintf(stderr, "%s read len %zu\n", enc_codec_name.c_str(), read_len);
        break;
      } else if (read_len == 0 && enc_codec_name == "rkmpp") {
        // rkmpp process does not accept empty buffer
        // it will treat the result of nullptr input as normal
        // though it is ugly, but we cannot change it by now
        fprintf(stderr, "%s read len 0\n", enc_codec_name.c_str());
        break;
      }
      if (first_video_time == 0) {
        first_video_time = easymedia::gettimeofday();
      }

      // feed video buffer
      src_buffer->SetValidSize(read_len); // important
      src_buffer->SetUSTimeStamp(first_video_time +
                                 vid_index * vinterval_per_frame); // important
      vid_index++;
      if (enc_codec_name == "rkmpp") {
        dst_buffer->SetValidSize(dst_buffer->GetSize());
        if (0 != vid_enc->Process(src_buffer, dst_buffer, nullptr)) {
          continue;
        }
        size_t out_len = dst_buffer->GetValidSize();
        fprintf(stderr, "vframe %d encoded, type %s, out %zu bytes\n", vid_index,
                dst_buffer->GetUserFlag() & easymedia::MediaBuffer::kIntra
                    ? "I frame"
                    : "P frame",
                out_len);
        mux->Write(dst_buffer, vid_stream_no);
      } else if (enc_codec_name == "ffmpeg_vid") {
        if (0 > encode<easymedia::VideoEncoder>(mux, vid_enc, src_buffer,
                                                vid_stream_no)) {
          fprintf(stderr, "Encode video frame %d failed\n", vid_index);
          break;
        }
      }
    } else {
      // audio
      fill_audio_sin_data(aud_buffer);
      if (first_audio_time == 0)
        first_audio_time = easymedia::gettimeofday();
      aud_buffer->SetUSTimeStamp(first_audio_time +
                                 aud_index * ainterval_per_frame); // important
      aud_index++;
      if (0 > encode<easymedia::AudioEncoder>(mux, aud_enc, aud_buffer, aud_stream_no)) {
        fprintf(stderr, "Encode audio frame %d failed\n", aud_index);
        break;
      }
    }
  }

  src_buffer->SetValidSize(0);
  if (0 > encode<easymedia::VideoEncoder>(mux, vid_enc, src_buffer, vid_stream_no)) {
    fprintf(stderr, "Drain video frame %d failed\n", vid_index);
  }
  aud_buffer->SetSamples(0);
  if (0 > encode<easymedia::AudioEncoder>(mux, aud_enc, aud_buffer, aud_stream_no)) {
    fprintf(stderr, "Drain audio frame %d failed\n", aud_index);
  }
  auto buffer = easymedia::MediaBuffer::Alloc(1);
  buffer->SetEOF(true);
  buffer->SetValidSize(0);
  mux->Write(buffer, vid_stream_no);

  close(input_file_fd);
  mux = nullptr;
  vid_enc = nullptr;
  aud_enc = nullptr;

  return 0;
}
