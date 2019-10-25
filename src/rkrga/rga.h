// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_RGA_H_
#define EASYMEDIA_RGA_H_

#include "image.h"
namespace easymedia {

class ImageBuffer;
int rga_blit(std::shared_ptr<ImageBuffer> src, std::shared_ptr<ImageBuffer> dst,
             ImageRect *src_rect = nullptr, ImageRect *dst_rect = nullptr,
             int rotate = 0);

} // namespace easymedia

#endif // #ifndef EASYMEDIA_RGA_H_
