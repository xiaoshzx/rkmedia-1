// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_SOUND_H_
#define EASYMEDIA_SOUND_H_

#include <stddef.h>

#include "utils.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  SAMPLE_FMT_NONE = -1,
  SAMPLE_FMT_U8,
  SAMPLE_FMT_S16,
  SAMPLE_FMT_S32,
  SAMPLE_FMT_VORBIS,
  SAMPLE_FMT_AAC,
  SAMPLE_FMT_MP2,
  SAMPLE_FMT_NB
} SampleFormat;

typedef struct {
  SampleFormat fmt;
  int channels;
  int sample_rate;
  int nb_samples;
} SampleInfo;

#ifdef __cplusplus
}
#endif

_API const char *SampleFmtToString(SampleFormat fmt);
_API SampleFormat StringToSampleFmt(const char *fmt_str);
_API bool SampleInfoIsValid(const SampleInfo &sample_info);
_API size_t GetSampleSize(const SampleInfo &sample_info);

#include <map>
#include <string>
namespace easymedia {
bool ParseSampleInfoFromMap(std::map<std::string, std::string> &params,
                            SampleInfo &si);
std::string _API to_param_string(const SampleInfo &si);
} // namespace easymedia

#endif // #ifndef EASYMEDIA_SOUND_H_
