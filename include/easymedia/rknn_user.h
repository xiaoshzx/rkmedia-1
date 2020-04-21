// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_RKNN_USER_H_
#define EASYMEDIA_RKNN_USER_H_

#ifdef USE_ROCKFACE
#include "rockface/rockface.h"
#endif
#ifdef USE_ROCKX
#include <rockx/rockx.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

using RknnCallBack = std::add_pointer<void(void* handler,
    int type, void *ptr, int size)>::type;
using RknnHandler = std::add_pointer<void*>::type;

typedef struct Rect {
  int left;
  int top;
  int right;
  int bottom;
} Rect;

typedef struct {
  int img_w;
  int img_h;
#ifdef USE_ROCKFACE
  rockface_det_t base;
  rockface_attribute_t attr;
  rockface_landmark_t landmark;
  rockface_angle_t angle;
  rockface_feature_t feature;
#endif
#ifdef USE_ROCKX
  rockx_object_t object;
#endif
} FaceInfo;

typedef struct {
  int img_w;
  int img_h;
#ifdef USE_ROCKX
  rockx_face_landmark_t object;
#endif
} LandmarkInfo;

typedef struct {
  int img_w;
  int img_h;
#ifdef USE_ROCKFACE
  rockface_det_t base;
#endif
} BodyInfo;

typedef enum {
  NNRESULT_TYPE_NONE = -1,
  NNRESULT_TYPE_FACE = 0,
  NNRESULT_TYPE_BODY,
  NNRESULT_TYPE_FINGER,
  NNRESULT_TYPE_LANDMARK,
} RknnResultType;

typedef struct {
  RknnResultType type;
  union {
    BodyInfo body_info;
    FaceInfo face_info;
    LandmarkInfo landmark_info;
  };
} RknnResult;

#ifdef __cplusplus
}
#endif

#endif // #ifndef EASYMEDIA_RKNN_USER_H_
