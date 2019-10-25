// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_CONTROL_H_
#define EASYMEDIA_CONTROL_H_

#include <stdint.h>

namespace easymedia {

typedef struct {
  const char *name;
  uint64_t value;
} DRMPropertyArg;

typedef struct {
  unsigned long int sub_request;
  void *arg;
} SubRequest;

enum {
  S_FIRST_CONTROL = 10000,
  S_SUB_REQUEST, // many devices have their kernel controls
  // ImageRect
  S_SOURCE_RECT,
  S_DESTINATION_RECT,
  S_SRC_DST_RECT,
  // ImageInfo
  G_PLANE_IMAGE_INFO,
  // int
  G_PLANE_SUPPORT_SCALE,
  // DRMPropertyArg
  S_CRTC_PROPERTY,
  S_CONNECTOR_PROPERTY,
  // any type
  S_STREAM_OFF,
};

} // namespace easymedia

#endif // #ifndef EASYMEDIA_CONTROL_H_