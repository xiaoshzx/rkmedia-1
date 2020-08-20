// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __RKMEDIA_VO_
#define __RKMEDIA_VO_
#ifdef __cplusplus
extern "C" {
#endif
#include "rkmedia_common.h"

typedef struct rkVO_CHN_ATTR_S {
  RK_U32 u32Width;
  RK_U32 u32Height;
  RK_U32 u32VerStride;
  RK_U32 u32HorStride;
  IMAGE_TYPE_E enImgType;
  RK_U16 u16Fps;
  RK_U16 u16Zpos;
} VO_CHN_ATTR_S;

#ifdef __cplusplus
}
#endif
#endif // #ifndef __RKMEDIA_VI_