// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "osd_producer.h"

OSDProducer::OSDProducer() {}

OSDProducer::~OSDProducer() { DestoryFontCaches(); }

std::shared_ptr<OSDCache> OSDProducer::CreateFontCaches(std::string font_path,
                                                        uint32_t font_size,
                                                        uint32_t color) {
  LOG_INFO("CreateFontCaches\n");
  auto osd_cache = std::make_shared<OSDCache>();
  if (!osd_cache)
    return nullptr;
  int ret = osd_cache->initCommonYuvMap(font_path, font_size, color);
  if (ret)
    return nullptr;
  osd_caches_.emplace_back(osd_cache);
  return osd_caches_[osd_caches_.size() - 1];
}

int OSDProducer::DestoryFontCaches() {
  for (auto iter : osd_caches_) {
    iter->deInitCommonYuvMap();
  }
  osd_caches_.clear();
  return 0;
}

std::shared_ptr<OSDCache> OSDProducer::FindOSDCaches(std::string font_path,
                                                     uint32_t font_size,
                                                     uint32_t color) {
  LOG_INFO("FindOSDCache\n");
  for (auto iter : osd_caches_) {
    auto caches = iter->GetCaches();
    for (auto it : caches) {
      auto font_cache = it.second;
      if (font_cache->font_color == color &&
          font_cache->font_size == font_size &&
          font_cache->font_path == font_path) {
        return iter;
      }
    }
  }
  return CreateFontCaches(font_path, font_size, color);
}

int OSDProducer::FillDate(osd_data_s *data) {
  LOG_INFO("FillDate\n");
  auto osd_caches = FindOSDCaches(data->text.font_path, data->text.font_size,
                                  data->text.font_color);
  if (!osd_caches)
    return -1;
  int len = wcslen(data->text.wch);
  int left, right, top, bottom = 0;
  int font_w, font_h;

  bool replace_empty = false;
  AutoDuration ad;

  left = right = top = bottom = 0;
  for (int index = 0; index < len; index++) {
    replace_empty = false;
    auto font_case = osd_caches->GetCommonFontCache(data->text.wch[index]);
    if (!font_case) {
      replace_empty = true;
      font_h = data->text.font_size;
      (data->text.wch[index] > 256) ? (font_w = data->text.font_size)
                                    : (font_w = (data->text.font_size >> 1));
    } else {
      font_w = font_case->font_width;
      font_h = font_case->font_height;
    }

    if (replace_empty) {
      left += font_w;
      continue;
    }
    right = left + font_w;
    if (right > data->width)
      right = data->width;
    bottom = top + font_h;
    if (bottom > data->height)
      bottom = data->height;

    uint8_t *font_addr = font_case->yuvamap_buffer;
    uint32_t font_offset = 0;
    uint32_t data_offset = 0;
    for (int j = top; j < bottom; j++) {
      for (int i = left; i < right; i++) {
        data_offset = j * data->width + i;
        font_offset = j * (font_w) + (i - left);
        data->buffer[data_offset] = font_addr[font_offset];
      }
    }
    left += font_w;
  }
  LOG_INFO("FillDateTime %lld ms %lld us\n", ad.Get() / 1000, ad.Get() % 1000);
  return 0;
}

int OSDProducer::GetImageInfo(osd_data_s *data) {
  LOG_INFO("GetImageinfo\n");
  AutoDuration ad;
  std::unique_ptr<BMPReader> bmp_reader_;
  bmp_reader_.reset(new BMPReader());
  int ret = bmp_reader_->GetBmpInfo(data);
  if (ret)
    return ret;
  return 0;
}

int OSDProducer::FillImage(osd_data_s *data) {
  LOG_INFO("FillImage\n");
  AutoDuration ad;
  std::unique_ptr<BMPReader> bmp_reader_;
  bmp_reader_.reset(new BMPReader());
  int ret = bmp_reader_->LoadYuvaMapFromFile(data);
  if (ret)
    return ret;
  LOG_INFO("FillImage w %d h %d %lld ms %lld us\n", data->width, data->height,
           ad.Get() / 1000, ad.Get() % 1000);
  return 0;
}

int OSDProducer::FillText(osd_data_s *data) {
  LOG_INFO("FillText\n");
  AutoDuration ad;
  std::unique_ptr<FontFactory> ft_factory;
  ft_factory.reset(new FontFactory());
  int ret = ft_factory->CreateFont(data->text.font_path, data->text.font_size);
  if (ret)
    return -1;
  ft_factory->SetFontColor(data->text.font_color);
  ft_factory->DrawTextYuvMap(data->buffer, data->width, data->height,
                             data->text.wch, 0, 0);
  ft_factory.reset();
  LOG_INFO("FillText %lld ms %lld us\n", ad.Get() / 1000, ad.Get() % 1000);
  return 0;
}

int OSDProducer::FillYuvMap(osd_data_s *data) {
  int ret = 0;
  if (data->type == OSD_TYPE_DATE) {
    ret = FillDate(data);
  } else if (data->type == OSD_TYPE_IMAGE) {
    ret = FillImage(data);
  } else if (data->type == OSD_TYPE_TEXT) {
    ret = FillText(data);
  }
  return ret;
}
