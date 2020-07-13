// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "color_table.h"

/* Match an RGB value to a particular palette index */
RK_U8 find_color(const RK_U32 *pal, RK_U32 len, RK_U8 r,
                          RK_U8 g, RK_U8 b) {
  RK_U32 i = 0;
  RK_U8 pixel = 0;
  RK_U32 smallest = 0;
  RK_U32 distance = 0;
  RK_S32 rd, gd, bd;
  RK_U8 rp, gp, bp;

  smallest = ~0;

  // LOG_DEBUG("find_color rgba_value %8x", (0xFF << 24 | r << 16 | g <<8 | b
  // <<0));

  for (i = 0; i < len; ++i) {
    bp = (pal[i] & 0xff000000) >> 24;
    gp = (pal[i] & 0x00ff0000) >> 16;
    rp = (pal[i] & 0x0000ff00) >> 8;

    rd = rp - r;
    gd = gp - g;
    bd = bp - b;

    distance = (rd * rd) + (gd * gd) + (bd * bd);
    if (distance < smallest) {
      pixel = i;

      /* Perfect match! */
      if (distance == 0)
        break;

      smallest = distance;
    }
  }

  // LOG_DEBUG("find_color pixel %d pal[%d][%d] %8x", pixel, pixel/6, pixel%6,
  // pal[pixel]);

  return pixel;
}
