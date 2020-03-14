// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_RKNN_USER_H_
#define EASYMEDIA_RKNN_USER_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
  int id;
  int left;
  int top;
  int right;
  int bottom;
  float score;
  int age;
  int gender;
  float pitch;
  float roll;
  float yaw;
} FaceInfo;

#ifdef __cplusplus
}
#endif

#endif // #ifndef EASYMEDIA_RKNN_USER_H_
