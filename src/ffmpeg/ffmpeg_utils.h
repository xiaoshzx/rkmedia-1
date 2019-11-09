// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_FFMPEG_UTILS_H_
#define EASYMEDIA_FFMPEG_UTILS_H_

extern "C" {
#define __STDC_CONSTANT_MACROS
#include <libavformat/avformat.h>
}

#include "image.h"
#include "sound.h"

namespace easymedia {

enum AVPixelFormat PixFmtToAVPixFmt(PixelFormat fmt);
enum AVCodecID PixFmtToAVCodecID(PixelFormat fmt);

enum AVCodecID SampleFmtToAVCodecID(SampleFormat fmt);
enum AVSampleFormat SampleFmtToAVSamFmt(SampleFormat sfmt);

void PrintAVError(int err, const char *log, const char *mark);

} // namespace easymedia

#endif // #ifndef EASYMEDIA_FFMPEG_UTILS_H_
