// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_IMAGE_H_
#define EASYMEDIA_IMAGE_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
  PIX_FMT_NONE = -1,
  PIX_FMT_YUV420P,
  PIX_FMT_NV12,
  PIX_FMT_NV21,
  PIX_FMT_YUV422P,
  PIX_FMT_NV16,
  PIX_FMT_NV61,
  PIX_FMT_YUYV422,
  PIX_FMT_UYVY422,
  PIX_FMT_RGB332,
  PIX_FMT_RGB565,
  PIX_FMT_BGR565,
  PIX_FMT_RGB888,
  PIX_FMT_BGR888,
  PIX_FMT_ARGB8888,
  PIX_FMT_ABGR8888,
  PIX_FMT_JPEG,
  PIX_FMT_H264,
  PIX_FMT_H265,
  PIX_FMT_NB
} PixelFormat;

typedef struct {
  PixelFormat pix_fmt;
  int width;      // valid pixel width
  int height;     // valid pixel height
  int vir_width;  // stride width, same to buffer_width, must greater than
                  // width, often set vir_width=(width+15)&(~15)
  int vir_height; // stride height, same to buffer_height, must greater than
                  // height, often set vir_height=(height+15)&(~15)
} ImageInfo;

typedef struct {
  int x, y; // left, top
  int w, h; // width, height
} ImageRect;

#ifdef __cplusplus
}
#endif

#include "utils.h"

_API void GetPixFmtNumDen(const PixelFormat &fmt, int &num, int &den);
_API int CalPixFmtSize(const PixelFormat &fmt, const int width,
                       const int height, int align = 16);
_API inline int CalPixFmtSize(const ImageInfo &ii, int align = 16) {
  return CalPixFmtSize(ii.pix_fmt, ii.width, ii.height, align);
}
_API PixelFormat StringToPixFmt(const char *type);
_API const char *PixFmtToString(PixelFormat fmt);

#include <map>
#include <string>
#include <vector>

namespace easymedia {
bool ParseImageInfoFromMap(std::map<std::string, std::string> &params,
                           ImageInfo &ii, bool input = true);
_API std::string to_param_string(const ImageInfo &ii, bool input = true);

_API std::string TwoImageRectToString(const std::vector<ImageRect> &src_dst);
std::vector<ImageRect> StringToTwoImageRect(const std::string &str_rect);

} // namespace easymedia

#endif // #ifndef EASYMEDIA_IMAGE_H_
