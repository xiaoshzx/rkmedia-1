// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef __RKMEDIA_AI_
#define __RKMEDIA_AI_
#ifdef __cplusplus
extern "C" {
#endif
#include "rkmedia_common.h"
typedef struct rkAI_CHN_ATTR_S {
  RK_CHAR *path;
  Sample_Format_E fmt;
  RK_U32 channels;
  RK_U32 sample_rate;
  RK_U32 nb_samples;
} AI_CHN_ATTR_S;
#ifdef __cplusplus
}
#endif
#endif // #ifndef __RKMEDIA_AI_
