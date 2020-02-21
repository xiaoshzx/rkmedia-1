// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "stdint.h"
#include "stdio.h"
#include "unistd.h"
#include "assert.h"
#include "signal.h"

#include <iostream>
#include <string>
#include <memory>

#include "easymedia/key_string.h"
#include "easymedia/media_type.h"
#include "easymedia/utils.h"
#include "easymedia/reflector.h"
#include "easymedia/buffer.h"
#include "easymedia/image.h"
#include "easymedia/flow.h"
#include "easymedia/stream.h"
#include "easymedia/media_config.h"

std::shared_ptr<easymedia::Flow> create_flow(const std::string &flow_name,
                                             const std::string &flow_param,
                                             const std::string &elem_param);

static bool quit = false;

static void sigterm_handler(int sig) {
  LOG("signal %d\n", sig);
  quit = true;
}

static char optstr[] = "?a:v:o:";

int main(int argc, char** argv)
{
  int in_w = 1280;
  int in_h = 720;
  int fps = 25;
  int w_align = UPALIGNTO16(in_w);
  int h_align = UPALIGNTO16(in_h);
  PixelFormat enc_in_fmt = PIX_FMT_YUYV422;
  ImageInfo info = {enc_in_fmt, in_w, in_h, w_align, h_align};

  std::string vid_in_path = "/dev/video0";
  std::string aud_in_path = "default:CARD=rockchiprk809co";
  std::string output_path;
  std::string input_format = "image:yuyv422";
  std::string flow_name;
  std::string flow_param;
  std::string sub_param;
  std::string video_enc_param;
  std::string audio_enc_param;
  std::string muxer_param;
  std::string stream_name;
  int c;

  easymedia::REFLECTOR(Stream)::DumpFactories();
  easymedia::REFLECTOR(Flow)::DumpFactories();

  opterr = 1;
  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
    case 'a':
      aud_in_path = optarg;
      LOG("audio device path: %s\n", aud_in_path.c_str());
      break;
    case 'v':
      vid_in_path = optarg;
      LOG("video device path: %s\n", vid_in_path.c_str());
      break;
    case 'o':
      output_path = optarg;
      LOG("output file path: %s\n", output_path.c_str());
      break;
    case '?':
    default:
      LOG("usage example: \n");
      LOG("\t%s -a default:CARD=rockchiprk809co -v /dev/video0 -o /tmp/out.mp4\n",
          argv[0]);
      break;
    }
  }

  if (vid_in_path.empty() || output_path.empty())
    exit(EXIT_FAILURE);

  if (vid_in_path.empty() || aud_in_path.empty()) {
    LOG("use default video device and audio device!\n");
  }

  flow_name = "source_stream";
  stream_name = "v4l2_capture_stream";

  PARAM_STRING_APPEND(flow_param, KEY_NAME, stream_name);
  PARAM_STRING_APPEND_TO(sub_param, KEY_USE_LIBV4L2, 1);
  PARAM_STRING_APPEND(sub_param, KEY_DEVICE, vid_in_path);
  PARAM_STRING_APPEND(sub_param, KEY_V4L2_CAP_TYPE,
                      KEY_V4L2_C_TYPE(VIDEO_CAPTURE));
  PARAM_STRING_APPEND(sub_param, KEY_V4L2_MEM_TYPE,
                      KEY_V4L2_M_TYPE(MEMORY_MMAP));
  PARAM_STRING_APPEND_TO(sub_param, KEY_FRAMES, 8);
  PARAM_STRING_APPEND(sub_param, KEY_OUTPUTDATATYPE, input_format);
  PARAM_STRING_APPEND_TO(sub_param, KEY_BUFFER_WIDTH, in_w);
  PARAM_STRING_APPEND_TO(sub_param, KEY_BUFFER_HEIGHT, in_h);
  auto video_source_flow = create_flow(flow_name, flow_param, sub_param);

  if (!video_source_flow) {
    printf("Create flow %s failed\n", flow_name.c_str());
    exit(EXIT_FAILURE);
  } else {
    printf("%s flow ready!\n", flow_name.c_str());
  }

  flow_name = "source_stream";
  flow_param = "";
  sub_param = "";
  stream_name = "alsa_capture_stream";

  PARAM_STRING_APPEND(flow_param, KEY_NAME, stream_name);
  PARAM_STRING_APPEND(sub_param, KEY_DEVICE, aud_in_path);
  PARAM_STRING_APPEND(sub_param, KEY_SAMPLE_FMT, SampleFmtToString(SAMPLE_FMT_S16));
  PARAM_STRING_APPEND_TO(sub_param, KEY_CHANNELS, 2);
  PARAM_STRING_APPEND_TO(sub_param, KEY_FRAMES, 1152);
  PARAM_STRING_APPEND_TO(sub_param, KEY_SAMPLE_RATE, 44100);

  auto audio_source_flow = create_flow(flow_name, flow_param, sub_param);
  if (!audio_source_flow) {
    printf("Create flow %s failed\n", flow_name.c_str());
    exit(EXIT_FAILURE);
  } else {
    printf("%s flow ready!\n", flow_name.c_str());
  }

  flow_param = "";
  flow_name = "video_enc";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "rkmpp");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, input_format);
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, VIDEO_H264);
  PARAM_STRING_APPEND_TO(flow_param, KEY_NEED_EXTRA_MERGE, 1);
  MediaConfig video_enc_config;
  VideoConfig &vid_cfg = video_enc_config.vid_cfg;
  ImageConfig &img_cfg = vid_cfg.image_cfg;
  img_cfg.image_info = info;
  img_cfg.qp_init = 24;
  vid_cfg.qp_step = 4;
  vid_cfg.qp_min = 12;
  vid_cfg.qp_max = 48;
  vid_cfg.bit_rate = in_w * in_h * 7;
  if (vid_cfg.bit_rate > 1000000) {
    vid_cfg.bit_rate /= 1000000;
    vid_cfg.bit_rate *= 1000000;
  }
  vid_cfg.frame_rate = fps;
  vid_cfg.level = 52;
  vid_cfg.gop_size = fps;
  vid_cfg.profile = 100;
  // vid_cfg.rc_quality = "aq_only"; vid_cfg.rc_mode = "vbr";
  vid_cfg.rc_quality = KEY_BEST;
  vid_cfg.rc_mode = KEY_CBR;

  video_enc_param.append(easymedia::to_param_string(video_enc_config, VIDEO_H264));
  auto video_enc_flow = create_flow(flow_name, flow_param, video_enc_param);

  if (!video_enc_flow) {
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
    exit(EXIT_FAILURE);
  } else {
    LOG("%s flow ready!\n", flow_name.c_str());
  }

  flow_name = "audio_enc";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "ffmpeg_aud");
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, AUDIO_MP2);
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, AUDIO_PCM_S16);
  MediaConfig audio_enc_config;
  // s16 2ch stereo, 1152 nb_samples
  SampleInfo aud_info = {SAMPLE_FMT_S16, 2, 44100, 1152};
  auto &ac = audio_enc_config.aud_cfg;
  ac.sample_info = aud_info;
  ac.bit_rate = 64000; // 64kbps
  audio_enc_config.type = Type::Audio;

  audio_enc_config.aud_cfg.codec_type = CODEC_TYPE_MP2;
  audio_enc_param.append(easymedia::to_param_string(audio_enc_config, AUDIO_MP2));
  flow_param = easymedia::JoinFlowParam(flow_param, 1, audio_enc_param);
  auto audio_enc_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!audio_enc_flow) {
    LOG("Create flow %s failed\n", flow_name.c_str());
  } else {
    LOG("%s flow ready!\n", flow_name.c_str());
  }

  flow_param = "";
  flow_name = "muxer_flow";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "muxer_flow");
  PARAM_STRING_APPEND_TO(flow_param, KEY_FILE_DURATION, 30);
  PARAM_STRING_APPEND_TO(flow_param, KEY_FILE_TIME, 1);
  PARAM_STRING_APPEND_TO(flow_param, KEY_FILE_INDEX, 18);
  // PARAM_STRING_APPEND(flow_param, KEY_FILE_PREFIX, "/tmp/RKMEDIA");
  PARAM_STRING_APPEND(flow_param, KEY_PATH, output_path);
  // PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, "flv");
  video_enc_config.vid_cfg.image_cfg.codec_type = CODEC_TYPE_H264;
  muxer_param.append(easymedia::to_param_string(video_enc_config, VIDEO_H264));

  auto &&param = easymedia::JoinFlowParam(flow_param, 2, audio_enc_param, muxer_param);
  auto muxer_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), param.c_str());
  if (!muxer_flow) {
    exit(EXIT_FAILURE);
  } else {
    LOG("%s flow ready!\n", flow_name.c_str());
  }

  video_enc_flow->AddDownFlow(muxer_flow, 0, 0);
  audio_enc_flow->AddDownFlow(muxer_flow, 0, 1);
  video_source_flow->AddDownFlow(video_enc_flow, 0, 0);
  audio_source_flow->AddDownFlow(audio_enc_flow, 0, 0);

  signal(SIGINT, sigterm_handler);

  while (!quit) {
    easymedia::msleep(100);
  }

  video_source_flow->RemoveDownFlow(video_enc_flow);
  audio_source_flow->RemoveDownFlow(audio_enc_flow);
  video_enc_flow->RemoveDownFlow(muxer_flow);
  audio_enc_flow->RemoveDownFlow(muxer_flow);

  audio_source_flow.reset();
  video_source_flow.reset();
  video_enc_flow.reset();
  audio_enc_flow.reset();
  muxer_flow.reset();

  return 0;
}

std::shared_ptr<easymedia::Flow> create_flow(const std::string &flow_name,
                                             const std::string &flow_param,
                                             const std::string &elem_param) {
  auto &&param = easymedia::JoinFlowParam(flow_param, 1, elem_param);
  auto ret = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), param.c_str());
  if (!ret)
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
  return ret;
}

