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
#include "easymedia/control.h"

static bool quit = false;

static void sigterm_handler(int sig) {
  LOG("signal %d\n", sig);
  quit = true;
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

std::shared_ptr<easymedia::Flow> create_alsa_flow(std::string aud_in_path,
                                                  SampleInfo &info,
                                                  bool capture) {
  std::string flow_name;
  std::string flow_param;
  std::string sub_param;
  std::string stream_name;

  if (capture) {
    //default sync mode
    flow_name = "source_stream";
    stream_name = "alsa_capture_stream";
  } else {
    flow_name = "output_stream";
    stream_name = "alsa_playback_stream";
    PARAM_STRING_APPEND(flow_param, KEK_THREAD_SYNC_MODEL, KEY_ASYNCCOMMON);
    PARAM_STRING_APPEND(flow_param, KEK_INPUT_MODEL, KEY_DROPFRONT);
    PARAM_STRING_APPEND_TO(flow_param, KEY_INPUT_CACHE_NUM, 5);
  }
  flow_param = "";
  sub_param = "";

  PARAM_STRING_APPEND(flow_param, KEY_NAME, stream_name);
  PARAM_STRING_APPEND(sub_param, KEY_DEVICE, aud_in_path);
  PARAM_STRING_APPEND(sub_param, KEY_SAMPLE_FMT, SampleFmtToString(info.fmt));
  PARAM_STRING_APPEND_TO(sub_param, KEY_CHANNELS, info.channels);
  PARAM_STRING_APPEND_TO(sub_param, KEY_FRAMES, info.nb_samples);
  PARAM_STRING_APPEND_TO(sub_param, KEY_SAMPLE_RATE, info.sample_rate);

  auto audio_source_flow = create_flow(flow_name, flow_param, sub_param);
  if (!audio_source_flow) {
    printf("Create flow %s failed\n", flow_name.c_str());
    exit(EXIT_FAILURE);
  } else {
    printf("%s flow ready!\n", flow_name.c_str());
  }
  return audio_source_flow;
}

std::shared_ptr<easymedia::Flow> create_resample_flow(SampleInfo &info) {
  std::string flow_name;
  std::string flow_param;
  std::string sub_param;

  flow_name = "filter";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "ffmpeg_resample");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE,
                      SampleFmtToString(info.fmt));
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE,
                      SampleFmtToString(info.fmt));
  sub_param = "";
  PARAM_STRING_APPEND(sub_param, KEY_SAMPLE_FMT, SampleFmtToString(info.fmt));
  PARAM_STRING_APPEND_TO(sub_param, KEY_CHANNELS, info.channels);
  PARAM_STRING_APPEND_TO(sub_param, KEY_SAMPLE_RATE, info.sample_rate);
  flow_param = easymedia::JoinFlowParam(flow_param, 1, sub_param);
  auto flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!flow) {
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
    exit(EXIT_FAILURE);
  }
  return flow;
}


std::shared_ptr<easymedia::Flow> create_audio_fifo_flow(SampleInfo &info) {
  std::string flow_name;
  std::string flow_param;
  std::string sub_param;

  flow_name = "filter";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "ffmpeg_audio_fifo");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE,
                      SampleFmtToString(info.fmt));
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE,
                      SampleFmtToString(info.fmt));
  sub_param = "";
  PARAM_STRING_APPEND(sub_param, KEY_SAMPLE_FMT, SampleFmtToString(info.fmt));
  PARAM_STRING_APPEND_TO(sub_param, KEY_CHANNELS, info.channels);
  PARAM_STRING_APPEND_TO(sub_param, KEY_SAMPLE_RATE, info.sample_rate);
  PARAM_STRING_APPEND_TO(sub_param, KEY_FRAMES, info.nb_samples);
  flow_param = easymedia::JoinFlowParam(flow_param, 1, sub_param);
  auto flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!flow) {
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
    exit(EXIT_FAILURE);
  }
  return flow;
}

void usage(char *name) {
  LOG("\nUsage: \t%s -a default -o default -f S16 -r 16000 -c 2 -F FLTP -R 48000 -C 1\n",name);
  LOG("\tNOTICE: format: -f -F [U8 S16 S32 FLT FLTP]\n");
  LOG("\tNOTICE: channels: -c -C [1 2]\n");
  LOG("\tNOTICE: samplerate: -r -R [8000 16000 24000 32000 441000 48000]\n");
  LOG("\tNOTICE: capture params: -f -c -r\n");
  LOG("\tNOTICE: resample params: -F -C -R\n");
  exit(EXIT_FAILURE);
}

SampleFormat parseFormat(std::string args) {
  if (!args.compare("S16"))
    return SAMPLE_FMT_S16;
  if (!args.compare("S32"))
    return SAMPLE_FMT_S32;
  if (!args.compare("U8"))
    return SAMPLE_FMT_U8;
  if (!args.compare("FLT"))
    return SAMPLE_FMT_FLT;
  if (!args.compare("FLTP"))
    return SAMPLE_FMT_FLTP;
  else
    return SAMPLE_FMT_NONE;
}

static char optstr[] = "?a:o:f:r:c:F:R:C:";

int main(int argc, char** argv)
{
  SampleFormat fmt = SAMPLE_FMT_S16;
  int channels = 1;
  int sample_rate = 8000;
  int nb_samples;
  SampleFormat res_fmt = SAMPLE_FMT_S16;;
  int res_channels = 1;
  int res_sample_rate = 8000;
  int res_nb_samples;
  int c;
  std::string aud_in_path = "default";
  std::string output_path = "default";
  std::string flow_name;
  std::string flow_param;
  std::string sub_param;
  std::string stream_name;

  easymedia::REFLECTOR(Stream)::DumpFactories();
  easymedia::REFLECTOR(Flow)::DumpFactories();

  opterr = 1;
  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
    case 'a':
      aud_in_path = optarg;
      LOG("audio device path: %s\n", aud_in_path.c_str());
      break;
    case 'o':
      output_path = optarg;
      LOG("output device path: %s\n", output_path.c_str());
      break;
    case 'f':
      fmt = parseFormat(optarg);
      if (fmt == SAMPLE_FMT_NONE)
        usage(argv[0]);
      break;
    case 'r':
      sample_rate = atoi(optarg);
      if (sample_rate != 8000 && sample_rate != 16000 && sample_rate != 24000 &&
          sample_rate != 32000 && sample_rate != 44100 &&
          sample_rate != 48000) {
        LOG("sorry, sample_rate %d not supported\n", sample_rate);
        usage(argv[0]);
      }
      break;
    case 'c':
      channels = atoi(optarg);
      if (channels < 1 || channels > 2)
        usage(argv[0]);
      break;
    case 'F':
      res_fmt = parseFormat(optarg);
      if (res_fmt == SAMPLE_FMT_NONE)
        usage(argv[0]);
      break;
    case 'R':
      res_sample_rate = atoi(optarg);
      if (res_sample_rate != 8000 && res_sample_rate != 16000 && res_sample_rate != 24000 &&
          res_sample_rate != 32000 && res_sample_rate != 44100 &&
          res_sample_rate != 48000) {
        LOG("sorry, sample_rate %d not supported\n", res_sample_rate);
        usage(argv[0]);
      }
      break;
    case 'C':
      res_channels = atoi(optarg);
      if (res_channels < 1 || res_channels > 2)
        usage(argv[0]);
      break;
    case '?':
    default:
      usage(argv[0]);
      break;
    }
  }

  nb_samples = sample_rate * 25 / 1000;//25ms
  res_nb_samples = res_sample_rate * 25 / 1000;//25ms
  SampleInfo sample_info = {fmt, channels, sample_rate, nb_samples};
  SampleInfo res_sample_info = {res_fmt, res_channels, res_sample_rate, res_nb_samples};

  // 1. alsa capture flow
  std::shared_ptr<easymedia::Flow> audio_source_flow =
      create_alsa_flow(aud_in_path, sample_info, true);
  if (!audio_source_flow) {
    LOG("Create flow alsa_capture_flow failed\n");
    exit(EXIT_FAILURE);
  }
  int volume = 70;
  audio_source_flow->Control(easymedia::S_ALSA_VOLUME, &volume);

  // 2. alsa resample
  std::shared_ptr<easymedia::Flow> audio_resample_flow =
      create_resample_flow(res_sample_info);
  if (!audio_resample_flow) {
    LOG("Create flow audio_resample_flow failed\n");
    exit(EXIT_FAILURE);
  }

  // 3. alsa resample back
  std::shared_ptr<easymedia::Flow> audio_resample_back_flow =
      create_resample_flow(sample_info);
  if (!audio_resample_back_flow) {
    LOG("Create flow audio_resample_flow failed\n");
    exit(EXIT_FAILURE);
  }

  // 4. audio fifo to fixed output samples
  sample_info.nb_samples *= 2;
  std::shared_ptr<easymedia::Flow> audio_fifo_flow =
      create_audio_fifo_flow(sample_info);
  if (!audio_fifo_flow) {
    LOG("Create flow audio_fifo_flow failed\n");
    exit(EXIT_FAILURE);
  }

  // 5. alsa playback flow
  std::shared_ptr<easymedia::Flow> audio_sink_flow =
      create_alsa_flow(output_path, sample_info, false);
  if (!audio_sink_flow) {
    LOG("Create flow alsa_capture_flow failed\n");
    exit(EXIT_FAILURE);
  }
  volume = 60;
  audio_sink_flow->Control(easymedia::S_ALSA_VOLUME, &volume);

  audio_fifo_flow->AddDownFlow(audio_sink_flow, 0, 0);
  audio_resample_back_flow->AddDownFlow(audio_fifo_flow, 0, 0);
  audio_resample_flow->AddDownFlow(audio_resample_back_flow, 0, 0);
  audio_source_flow->AddDownFlow(audio_resample_flow, 0, 0);

  signal(SIGINT, sigterm_handler);

  while (!quit) {
    easymedia::msleep(100);
  }

  audio_fifo_flow->RemoveDownFlow(audio_sink_flow);
  audio_resample_back_flow->RemoveDownFlow(audio_fifo_flow);
  audio_resample_flow->RemoveDownFlow(audio_resample_back_flow);
  audio_source_flow->RemoveDownFlow(audio_resample_flow);
  audio_resample_back_flow.reset();
  audio_resample_flow.reset();
  audio_source_flow.reset();
  audio_fifo_flow.reset();
  audio_sink_flow.reset();

  return 0;
}
