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

  // DRM Display controls
  // ImageRect
  S_SOURCE_RECT = 10100,
  S_DESTINATION_RECT,
  S_SRC_DST_RECT,
  // ImageInfo
  G_PLANE_IMAGE_INFO,
  // int
  G_PLANE_SUPPORT_SCALE,
  // DRMPropertyArg
  S_CRTC_PROPERTY,
  S_CONNECTOR_PROPERTY,

  // V4L2 controls
  // any type
  S_STREAM_OFF = 10200,

  // ALSA controls
  // int
  S_ALSA_VOLUME = 10300,
  G_ALSA_VOLUME,

  // Through Guard controls
  // int
  S_ALLOW_THROUGH_COUNT = 10400,

  // ANR controls
  // int
  S_ANR_ON = 10500,
  G_ANR_ON,

  // RKNN controls
  // any type
  S_CALLBACK_HANDLER = 10600,
  G_CALLBACK_HANDLER,
};

} // namespace easymedia

#endif // #ifndef EASYMEDIA_CONTROL_H_
