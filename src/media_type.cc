// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_type.h"
#include "utils.h"

namespace easymedia {

Type StringToDataType(const char *data_type) {
  if (string_start_withs(data_type, AUDIO_PREFIX))
    return Type::Audio;
  else if (string_start_withs(data_type, IMAGE_PREFIX))
    return Type::Image;
  else if (string_start_withs(data_type, VIDEO_PREFIX))
    return Type::Video;
  else if (string_start_withs(data_type, TEXT_PREFIX))
    return Type::Text;
  return Type::None;
}

} // namespace easymedia