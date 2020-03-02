// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osd_cache.h"

OSDCache::OSDCache() {
  // initCommon(path, size, color, "0123456789:-/*.");
}

int OSDCache::initCommonYuvMap(std::string path, uint32_t size,
                               uint32_t color) {
  LOG_INFO("initCommonYuvMap\n");
  // std::unique_ptr<FontFactory> ft_factory;
  // ft_factory.reset(new FontFactory());
  auto ft_factory = std::make_shared<FontFactory>();
  if (!ft_factory)
    return -1;
  int ret = ft_factory->CreateFont(path.c_str(), size);
  if (ret)
    return -1;
  ft_factory->SetFontColor(color);
  const char *str = COMMON_STRING;
  int font_w, font_h;
  for (size_t i = 0; i < strlen(str); i++) {
    auto font_case = std::make_shared<FontCache>();
    font_case->font_color = color;
    font_case->font_size = size;
    font_case->font_path = path;
    font_case->wch = (wchar_t)str[i];
    font_w = size;
    font_h = size;
    if (font_case->wch < 256)
      font_w = size >> 1;
    font_case->font_width = font_w;
    font_case->font_height = font_h;
    font_case->yuvamap_buffer_size = font_w * font_h;
    font_case->yuvamap_buffer =
        (uint8_t *)malloc(font_case->yuvamap_buffer_size);
    if (!font_case->yuvamap_buffer) {
      printf("initCommonCaches malloc yuvamap_buffer faild\n");
    }
    ft_factory->DrawWCharYuvMap(font_case->yuvamap_buffer, font_w, font_h,
                                font_case->wch, 0, 0);
    caches_.emplace_back(font_case->wch, font_case);
  }
  // ft_factory.reset();
  return 0;
}

int OSDCache::deInitCommonYuvMap() {
  for (auto iter : caches_) {
    auto font_case = iter.second;
    if (font_case->yuvamap_buffer) {
      free(font_case->yuvamap_buffer);
      font_case->yuvamap_buffer = NULL;
    }
  }
  return 0;
}

std::shared_ptr<FontCache> OSDCache::GetCommonFontCache(wchar_t wch) {
  LOG_INFO("GetCommonFontCache\n");
  for (auto iter : caches_) {
    if (iter.first == wch) {
      return iter.second;
    }
  }
  return nullptr;
}
