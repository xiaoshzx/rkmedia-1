// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _RK_OSD_COMMON_H_
#define _RK_OSD_COMMON_H_

#include <chrono>
#include <stdint.h>
#include <string>

//#define INFO_EN
//#define DEBUG_EN

#ifdef INFO_EN
#define LOG_INFO(format, ...) fprintf(stderr, format "", ##__VA_ARGS__)
#else
#define LOG_INFO(format, ...)
#endif

#ifdef DEBUG_EN
#define LOG_DEBUG(format, ...) fprintf(stderr, format "", ##__VA_ARGS__)
#else
#define LOG_DEBUG(format, ...)
#endif

inline int64_t gettimeofday() {
  std::chrono::microseconds us =
      std::chrono::duration_cast<std::chrono::microseconds>(
          std::chrono::system_clock::now().time_since_epoch());
  return us.count();
}

class AutoDuration {
public:
  AutoDuration() { Reset(); }
  int64_t Get() { return gettimeofday() - start; }
  void Reset() { start = gettimeofday(); }
  int64_t GetAndReset() {
    int64_t now = gettimeofday();
    int64_t pretime = start;
    start = now;
    return now - pretime;
  }

private:
  int64_t start;
};

enum {
  OSD_TYPE_DATE = 0,
  OSD_TYPE_IMAGE = 1,
  OSD_TYPE_TEXT = 2,
};

typedef struct text_data {
  const wchar_t *wch;
  uint32_t font_size;
  uint32_t font_color;
  const char *font_path;
} text_data_s;

typedef struct osd_data {
  int type;
  union {
    const char *image;
    text_data_s text;
  };
  int width;
  int height;
  uint8_t *buffer;
  uint32_t size;
} osd_data_s;

#endif // _RK_OSD_COMMON_H_
