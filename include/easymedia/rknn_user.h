// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_RKNN_USER_H_
#define EASYMEDIA_RKNN_USER_H_

#ifdef USE_ROCKFACE
#include "rockface/rockface.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
#ifdef USE_ROCKFACE
  rockface_det_t base;
  rockface_attribute_t attr;
  rockface_landmark_t landmark;
  rockface_angle_t angle;
  rockface_feature_t feature;
#endif
} FaceInfo;

typedef enum {
  NNRESULT_TYPE_NONE = -1,
  NNRESULT_TYPE_FACE = 0,
  NNRESULT_TYPE_BODY,
  NNRESULT_TYPE_FINGER,
} RknnResultType;

typedef struct {
  RknnResultType type;
  union {
    FaceInfo face_info;
  };
} RknnResult;

#ifdef __cplusplus
}
#endif

#endif // #ifndef EASYMEDIA_RKNN_USER_H_
