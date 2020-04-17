// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

 #include <string.h>

#include "utils.h"
#include "rknn_utils.h"
#include "media_type.h"

namespace easymedia {

#ifdef USE_ROCKFACE
static const struct PixelFmtEntry {
  rockface_pixel_format fmt;
  const char *fmt_str;
} pixel_fmt_string_map[] = {
  {ROCKFACE_PIXEL_FORMAT_GRAY8, "image:gray8"},
  {ROCKFACE_PIXEL_FORMAT_RGB888, IMAGE_RGB888},
  {ROCKFACE_PIXEL_FORMAT_BGR888, IMAGE_BGR888},
  {ROCKFACE_PIXEL_FORMAT_RGBA8888, IMAGE_ARGB8888},
  {ROCKFACE_PIXEL_FORMAT_BGRA8888, IMAGE_ABGR8888},
  {ROCKFACE_PIXEL_FORMAT_YUV420P_YU12, IMAGE_YUV420P},
  {ROCKFACE_PIXEL_FORMAT_YUV420P_YV12, "image:yv12"},
  {ROCKFACE_PIXEL_FORMAT_YUV420SP_NV12, IMAGE_NV12},
  {ROCKFACE_PIXEL_FORMAT_YUV420SP_NV21, IMAGE_NV21},
  {ROCKFACE_PIXEL_FORMAT_YUV422P_YU16, IMAGE_UYVY422},
  {ROCKFACE_PIXEL_FORMAT_YUV422P_YV16, "image:yv16"},
  {ROCKFACE_PIXEL_FORMAT_YUV422SP_NV16, IMAGE_NV16},
  {ROCKFACE_PIXEL_FORMAT_YUV422SP_NV61, IMAGE_NV61}
};

rockface_pixel_format StrToRockFacePixelFMT(const char *fmt_str) {
  FIND_ENTRY_TARGET_BY_STRCMP(fmt_str, pixel_fmt_string_map, fmt_str, fmt)
  return ROCKFACE_PIXEL_FORMAT_MAX;
}
#endif // USE_ROCKFACE

} // namespace easymedia