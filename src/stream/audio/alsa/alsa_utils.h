// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_ALSA_UTILS_H_
#define EASYMEDIA_ALSA_UTILS_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <alsa/asoundlib.h>

#ifdef __cplusplus
}
#endif

#include <map>
#include <string>

#include "sound.h"

snd_pcm_format_t SampleFormatToAlsaFormat(SampleFormat fmt);
void ShowAlsaAvailableFormats(snd_pcm_t *handle, snd_pcm_hw_params_t *params);
int ParseAlsaParams(const char *param,
                    std::map<std::string, std::string> &params,
                    std::string &device, SampleInfo &sample_info);

snd_pcm_t *AlsaCommonOpenSetHwParams(const char *device,
                                     snd_pcm_stream_t stream, int mode,
                                     SampleInfo &sample_info,
                                     snd_pcm_hw_params_t *hwparams);

#endif // EASYMEDIA_ALSA_UTILS_H_
