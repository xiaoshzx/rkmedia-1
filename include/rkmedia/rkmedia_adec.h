// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef __RKMEDIA_ADEC_
#define __RKMEDIA_ADEC_
#ifdef __cplusplus
extern "C" {
#endif
#include "rkmedia_common.h"

typedef struct rkADEC_ATTR_AAC_S {
  // reserved
} ADEC_ATTR_AAC_S;

typedef struct rkADEC_ATTR_MP2_S {
  // reserved
} ADEC_ATTR_MP2_S;

typedef struct rkADEC_ATTR_G711A_S {
  RK_U32 u32Channels;
  RK_U32 u32SampleRate;
} ADEC_ATTR_G711A_S;

typedef struct rkADEC_ATTR_G711U_S {
  RK_U32 u32Channels;
  RK_U32 u32SampleRate;
} ADEC_ATTR_G711U_S;

typedef struct rkADEC_ATTR_G726_S {
  // reserved
} ADEC_ATTR_G726_S;

typedef struct rkADEC_CHN_ATTR_S {
  CODEC_TYPE_E enType;
  union {
    ADEC_ATTR_AAC_S aac_attr;
    ADEC_ATTR_MP2_S mp2_attr;
    ADEC_ATTR_G711A_S g711a_attr;
    ADEC_ATTR_G711U_S g711u_attr;
    ADEC_ATTR_G726_S g726_attr;
  };
} ADEC_CHN_ATTR_S;

#ifdef __cplusplus
}
#endif
#endif // #ifndef __RKMEDIA_ADEC_
