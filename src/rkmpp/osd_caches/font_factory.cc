// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <wchar.h>

#include "color_table.h"
#include "font_factory.h"

#define RGB2PIXEL888(r, g, b) (((r) << 16) | ((g) << 8) | (b))

void inline FillBitmapToBuf(FT_Face face, FT_Glyph glyph, int font_size,
                            uint color, uint *buffer, int buffer_w,
                            int buffer_h, int x, int y, bool is_char) {
  FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
  FT_Bitmap *bitmap = &bitmap_glyph->bitmap;
  unsigned char bR, bG, bB;
  unsigned char cR, cG, cB;

  int font_w = font_size;
  int font_h = font_size;
  if (is_char)
    font_w = font_size >> 1;

  int bitmap_top =
      (face->size->metrics.ascender >> 6) - face->glyph->bitmap_top;
  int bitmap_bottom = bitmap_top + bitmap->rows;
  int bitmap_left = face->glyph->bitmap_left;
  int bitmap_right = bitmap_left + bitmap->width;
  if (bitmap_bottom > buffer_h)
    bitmap_bottom = buffer_h;
  if (bitmap_right > buffer_w)
    bitmap_right = buffer_w;

  int resion_top = y;
  int resion_bottom = resion_top + font_h;
  int resion_left = x;
  int resion_right = resion_left + font_w;
  if (resion_bottom > buffer_h)
    resion_bottom = buffer_h;
  if (resion_right > buffer_w)
    resion_right = buffer_w;

  cR = color >> 16 & 0xFF;
  cG = color >> 8 & 0xFF;
  cB = color >> 0 & 0xFF;

  for (int j = resion_top, p = 0; j < resion_bottom; j++, p++) {
    for (int i = resion_left, q = 0; i < resion_right; i++, q++) {
      int area = buffer_w * j;
      if (p >= bitmap_top && p < bitmap_bottom && q >= bitmap_left &&
          q < bitmap_right) {
        bR = buffer[area + i] >> 16 & 0xFF;
        bG = buffer[area + i] >> 8 & 0xFF;
        bB = buffer[area + i] >> 0 & 0xFF;
        unsigned char gray =
            bitmap
                ->buffer[(p - bitmap_top) * bitmap->width + (q - bitmap_left)];
        buffer[area + i] = RGB2PIXEL888((cR * gray + bR * (255 - gray)) >> 8,
                                        (cG * gray + bG * (255 - gray)) >> 8,
                                        (cB * gray + bB * (255 - gray)) >> 8);
      }
    }
  }
}

void inline FillYuvMapToBuf(FT_Face face, FT_Glyph glyph, int font_size,
                            uint color, uint8_t *buffer, int buffer_w,
                            int buffer_h, int x, int y, bool is_char) {
  FT_BitmapGlyph bitmap_glyph = (FT_BitmapGlyph)glyph;
  FT_Bitmap *bitmap = &bitmap_glyph->bitmap;
  unsigned char cR, cG, cB;

  int font_w = font_size;
  int font_h = font_size;
  if (is_char)
    font_w = font_w >> 1;

  int bitmap_top =
      (face->size->metrics.ascender >> 6) - face->glyph->bitmap_top;
  int bitmap_bottom = bitmap_top + bitmap->rows;
  int bitmap_left = face->glyph->bitmap_left;
  int bitmap_right = bitmap_left + bitmap->width;
  if (bitmap_bottom > font_h)
    bitmap_bottom = font_h;
  if (bitmap_right > font_w)
    bitmap_right = font_w;

  int resion_top = y;
  int resion_bottom = resion_top + font_h;
  int resion_left = x;
  int resion_right = resion_left + font_w;
  if (resion_bottom > buffer_h)
    resion_bottom = buffer_h;
  if (resion_right > buffer_w)
    resion_right = buffer_w;

  cR = color >> 16 & 0xFF;
  cG = color >> 8 & 0xFF;
  cB = color >> 0 & 0xFF;

  for (int j = resion_top, p = 0; j < resion_bottom; j++, p++) {
    for (int i = resion_left, q = 0; i < resion_right; i++, q++) {
      int area = buffer_w * j;
      if (p >= bitmap_top && p < bitmap_bottom && q >= bitmap_left &&
          q < bitmap_right) {
        unsigned char gray =
            bitmap
                ->buffer[(p - bitmap_top) * bitmap->width + (q - bitmap_left)];
        if (gray) {
          buffer[area + i] =
              find_color(rgb888_palette_table, PALETTE_TABLE_LEN, cR, cG, cB);
          LOG_DEBUG("*%3d", buffer[area + i]);
          // LOG_DEBUG("*");
        } else {
          buffer[area + i] = PALETTE_TABLE_LEN - 1;
          LOG_DEBUG("-%3d", buffer[area + i]);
          // LOG_DEBUG("-");
        }
      } else {
        buffer[area + i] = PALETTE_TABLE_LEN - 1;
        LOG_DEBUG("-%3d", buffer[area + i]);
        // LOG_DEBUG("-");
      }
    }
    LOG_DEBUG("\n");
  }
  LOG_DEBUG("\n\n");
}

int FontFactory::CreateFont(const char *font_path, int font_size) {
  FT_Init_FreeType(&library_);
  FT_New_Face(library_, font_path, 0, &face_);
  if (!face_) {
    printf("please check font_path %s\n", font_path);
    return -1;
  }
  FT_Set_Char_Size(face_, font_size * 64, font_size * 64, 0, 0);
  font_size_ = font_size;
  font_path_ = font_path;
  return 0;
}

int FontFactory::DestoryFont() {
  if (face_) {
    FT_Done_Face(face_);
    face_ = NULL;
  }
  if (library_) {
    FT_Done_FreeType(library_);
    library_ = NULL;
  }
  return 0;
}

int FontFactory::SetFontSize(int font_size) {
  FT_Set_Char_Size(face_, font_size * 64, font_size * 64, 0, 0);
  font_size_ = font_size;
  return 0;
}

int FontFactory::GetFontSize() { return font_size_; }

uint FontFactory::SetFontColor(uint font_color) {
  font_color_ = font_color;
  return 0;
}

uint FontFactory::GetFontColor() { return font_color_; }

int FontFactory::DrawWChar(uint *buffer, int buf_w, int buf_h, wchar_t wch,
                           int x, int y) {
  FT_Glyph glyph;
  FT_UInt charIdx = FT_Get_Char_Index(face_, wch);
  bool is_char = false;
  FT_Load_Glyph(face_, charIdx, FT_LOAD_DEFAULT);
  FT_Get_Glyph(face_->glyph, &glyph);
  FT_Glyph_To_Bitmap(&glyph, ft_render_mode_normal, 0, 1);
  (wch < 255) ? is_char = true : is_char = false;
  FillBitmapToBuf(face_, glyph, font_size_, font_color_, (uint *)buffer, buf_w,
                  buf_h, x, y, is_char);
  FT_Done_Glyph(glyph);
  glyph = NULL;
  return 0;
}

int FontFactory::DrawText(uint *buffer, int buf_w, int buf_h,
                          const wchar_t *wstr, int x, int y) {
  int x_offset = 0;
  int len = wcslen(wstr);
  for (int i = 0; i < len; i++) {
    DrawWChar(buffer, buf_w, buf_h, wstr[i], x + x_offset, y);
    x_offset += (face_->glyph->advance.x >> 6);
  }
  return 0;
}

int FontFactory::DrawWCharYuvMap(uint8_t *buffer, int buf_w, int buf_h,
                                 wchar_t wch, int x, int y) {
  FT_Glyph glyph;
  FT_UInt charIdx = FT_Get_Char_Index(face_, wch);
  bool is_char = false;
  FT_Load_Glyph(face_, charIdx, FT_LOAD_DEFAULT);
  FT_Get_Glyph(face_->glyph, &glyph);
  FT_Glyph_To_Bitmap(&glyph, ft_render_mode_normal, 0, 1);
  (wch < 255) ? is_char = true : is_char = false;
  LOG_DEBUG("DrawWCharYuvMap wch %c\n", wch);
  FillYuvMapToBuf(face_, glyph, font_size_, font_color_, buffer, buf_w, buf_h,
                  x, y, is_char);
  FT_Done_Glyph(glyph);
  glyph = NULL;
  return 0;
}

int FontFactory::DrawTextYuvMap(uint8_t *buffer, int buf_w, int buf_h,
                                const wchar_t *wstr, int x, int y) {
  int x_offset = 0;
  int len = wcslen(wstr);
  for (int i = 0; i < len; i++) {
    DrawWCharYuvMap(buffer, buf_w, buf_h, wstr[i], x + x_offset, y);
    x_offset += (face_->glyph->advance.x >> 6);
  }
  return 0;
}
