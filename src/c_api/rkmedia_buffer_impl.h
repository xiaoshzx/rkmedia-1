// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __RK_BUFFER_IMPL_
#define __RK_BUFFER_IMPL_

#include "buffer.h"
#include "flow.h"

#include "rkmedia_common.h"

typedef struct _rkMEDIA_BUFFER_S {
  MB_TYPE_E type;
  void *ptr;        // Virtual address of buffer
  int fd;           // dma buffer fd
  size_t size;      // The size of the buffer
  MOD_ID_E mode_id; // The module to which the buffer belongs
  RK_U16 chn_id;    // The channel to which the buffer belongs
  RK_U64 timestamp; // buffer timesatmp
  std::shared_ptr<easymedia::MediaBuffer> rkmedia_mb;
  union {
    MB_IMAGE_INFO_S stImageInfo;
  };

} MEDIA_BUFFER_IMPLE;

#endif // __RK_BUFFER_IMPL_