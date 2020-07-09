// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_type.h"
#include "rkmedia_common.h"
#include "utils.h"

std::string ImageTypeToString(IMAGE_TYPE_E type) {
  switch (type) {
  case IMAGE_TYPE_GRAY8:
    return IMAGE_GRAY8;
  case IMAGE_TYPE_GRAY16:
    return IMAGE_GRAY16;
  case IMAGE_TYPE_YUV420P:
    return IMAGE_YUV420P;
  case IMAGE_TYPE_NV12:
    return IMAGE_NV12;
  case IMAGE_TYPE_NV21:
    return IMAGE_NV21;
  case IMAGE_TYPE_YV12:
    return IMAGE_YV12;
  case IMAGE_TYPE_FBC2:
    return IMAGE_FBC2;
  case IMAGE_TYPE_FBC0:
    return IMAGE_FBC0;
  case IMAGE_TYPE_YUV422P:
    return IMAGE_YUV422P;
  case IMAGE_TYPE_NV16:
    return IMAGE_NV16;
  case IMAGE_TYPE_NV61:
    return IMAGE_NV61;
  case IMAGE_TYPE_YV16:
    return IMAGE_YV16;
  case IMAGE_TYPE_YUYV422:
    return IMAGE_YUYV422;
  case IMAGE_TYPE_UYVY422:
    return IMAGE_UYVY422;
  case IMAGE_TYPE_RGB332:
    return IMAGE_RGB332;
  case IMAGE_TYPE_RGB565:
    return IMAGE_RGB565;
  case IMAGE_TYPE_BGR565:
    return IMAGE_BGR565;
  case IMAGE_TYPE_RGB888:
    return IMAGE_RGB888;
  case IMAGE_TYPE_BGR888:
    return IMAGE_BGR888;
  case IMAGE_TYPE_ARGB8888:
    return IMAGE_ARGB8888;
  case IMAGE_TYPE_ABGR8888:
    return IMAGE_ABGR8888;
  case IMAGE_TYPE_JPEG:
    return IMAGE_JPEG;
  default:
    LOG("no found image type:%d", type);
    return "";
  }
}

std::string CodecToString(CodecType type) {
  switch (type) {
  case CODEC_TYPE_AAC:
    return AUDIO_AAC;
  case CODEC_TYPE_MP2:
    return AUDIO_MP2;
  case CODEC_TYPE_VORBIS:
    return AUDIO_VORBIS;
  case CODEC_TYPE_G711A:
    return AUDIO_G711A;
  case CODEC_TYPE_G711U:
    return AUDIO_G711U;
  case CODEC_TYPE_G726:
    return AUDIO_G726;
  case CODEC_TYPE_H264:
    return VIDEO_H264;
  case CODEC_TYPE_H265:
    return VIDEO_H265;
  default:
    return "";
  }
}
