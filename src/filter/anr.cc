// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer.h"
#include "filter.h"
#include <assert.h>
extern "C" {
#include <ANR/anr_provider.h>
}

#define DEBUG_FILE 0
#if DEBUG_FILE
#include <fstream>
#endif

namespace easymedia {

class ANRFilter : public Filter {
public:
  ANRFilter(const char *param);
  virtual ~ANRFilter();
  static const char *GetFilterName() { return "ANR"; }
  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> &output) override;
  virtual int IoCtrl(unsigned long int request, ...) override;

private:
  int channels;
  int sample_rate;
  SampleFormat format;
  int nb_samples;
  std::string param_path;

  bool anr_on;
  APHandleType anr_handle;

#if DEBUG_FILE
  std::ofstream infile;
  std::ofstream outfile;
#endif
};

ANRFilter::ANRFilter(const char *param) : anr_on(true), anr_handle(nullptr) {
  std::string s_format;
  std::string s_channels;
  std::string s_sample_rate;
  std::string s_nb_samples;
  std::map<std::string, std::string> params;
  std::list<std::pair<const std::string, std::string &>> req_list;
  req_list.push_back(
      std::pair<const std::string, std::string &>(KEY_SAMPLE_FMT, s_format));
  req_list.push_back(
      std::pair<const std::string, std::string &>(KEY_CHANNELS, s_channels));
  req_list.push_back(std::pair<const std::string, std::string &>(
      KEY_SAMPLE_RATE, s_sample_rate));
  req_list.push_back(
      std::pair<const std::string, std::string &>(KEY_FRAMES, s_nb_samples));
  parse_media_param_match(param, params, req_list);
  if (!s_channels.empty())
    channels = std::atoi(s_channels.c_str());
  if (!s_sample_rate.empty())
    sample_rate = std::atoi(s_sample_rate.c_str());
  if (!s_format.empty())
    format = StringToSampleFmt(s_format.c_str());
  if (!s_nb_samples.empty())
    nb_samples = std::atoi(s_nb_samples.c_str());

  /* support 8k~48k S16 format and 1 channels only */
  assert(format == SAMPLE_FMT_S16P);
  assert(sample_rate >= 8000 && sample_rate <= 48000);
  assert(channels == 1);
  int frame_time = nb_samples * 1000 / sample_rate;
  LOG("ANR: frame time %d\n", frame_time);
  assert(frame_time == 10 || frame_time == 16 || frame_time == 20);

  ANRProcessState state;
  state.anr_enabled = 1;
  state.sampling_rate = sample_rate; // 8k~48k
  state.frame_size = nb_samples;     // only 10ms|16ms|20ms

  anr_handle = ANR_Init(&state);
  assert(anr_handle);

#if DEBUG_FILE
  static int id = 0;
  id++;
  std::string file_in =
      std::string("/tmp/anr_in") + std::to_string(id) + std::string(".pcm");
  std::string file_out =
      std::string("/tmp/anr_out") + std::to_string(id) + std::string(".pcm");
  infile.open(file_in.c_str(),
              std::ios::out | std::ios::trunc | std::ios::binary);
  outfile.open(file_out.c_str(),
               std::ios::out | std::ios::trunc | std::ios::binary);
  assert(infile.is_open() && outfile.is_open());
#endif
}

ANRFilter::~ANRFilter() {
  ANR_Destroy(anr_handle);
#if DEBUG_FILE
  infile.close();
  outfile.close();
#endif
}

int ANRFilter::Process(std::shared_ptr<MediaBuffer> input,
                       std::shared_ptr<MediaBuffer> &output) {
  if (!input || input->GetType() != Type::Audio)
    return -EINVAL;
  if (!output)
    return -EINVAL;

  auto src = std::static_pointer_cast<easymedia::SampleBuffer>(input);
  SampleInfo src_info = src->GetSampleInfo();
  if (src_info.fmt != format || src_info.channels != channels ||
      src_info.sample_rate != sample_rate ||
      src_info.nb_samples != nb_samples) {
    return -1;
  }
  if (anr_on) {
    SampleInfo dst_info = {format, 1, sample_rate, nb_samples};
    int size = GetSampleSize(dst_info) * nb_samples;
    auto dst = std::make_shared<easymedia::SampleBuffer>(
        MediaBuffer::Alloc2(size), dst_info);
    assert(dst);
    ANR_Process(anr_handle, (short int *)src->GetPtr(),
                (short int *)dst->GetPtr());

#if DEBUG_FILE
    infile.write((const char *)src->GetPtr(), src->GetValidSize());
    outfile.write((const char *)dst->GetPtr(), dst->GetValidSize());
#endif
  } else {
    output = input;
  }
  return 0;
}

int ANRFilter::IoCtrl(unsigned long int request, ...) {
  va_list vl;
  va_start(vl, request);
  void *arg = va_arg(vl, void *);
  va_end(vl);

  if (!arg)
    return -1;

  int ret = 0;
  switch (request) {
  case S_ANR_ON:
    anr_on = *((int *)arg);
    break;
  case G_ANR_ON:
    *((int *)arg) = anr_on;
    break;
  default:
    ret = -1;
    break;
  }
  return ret;
}

DEFINE_COMMON_FILTER_FACTORY(ANRFilter)
const char *FACTORY(ANRFilter)::ExpectedInputDataType() {
  return AUDIO_PCM_S16;
}

const char *FACTORY(ANRFilter)::OutPutDataType() { return AUDIO_PCM_S16; }
} // namespace easymedia
