// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_RK_AUDIO_H_
#define EASYMEDIA_RK_AUDIO_H_

#include <cstddef>

#include "sound.h"

// This file contains rk particular audio functions
inline bool rk_aec_agc_anr_algorithm_support();
int rk_voice_init(const SampleInfo &sample_info, short int ashw_para[500]);
void rk_voice_handle(void *buffer, int bytes);
void rk_voice_deinit();

#endif // EASYMEDIA_RK_AUDIO_H_