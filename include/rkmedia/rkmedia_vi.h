// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __RKMEDIA_VI_
#define __RKMEDIA_VI_
#ifdef __cplusplus
extern "C" {
#endif
#include "rkmedia_common.h"

typedef struct rkVI_CHN_ATTR_S {
  RK_U32 width;
  RK_U32 height;
  IMAGE_TYPE_E pix_fmt;
  RK_U32 buffer_cnt;
} VI_CHN_ATTR_S;
#ifdef __cplusplus
}
#endif
#endif // #ifndef __RKMEDIA_VI_