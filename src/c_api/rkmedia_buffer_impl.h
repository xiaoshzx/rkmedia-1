// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __RK_BUFFER_IMPL_
#define __RK_BUFFER_IMPL_

#include "buffer.h"
#include "flow.h"

#include "rkmedia_common.h"

typedef struct _rkMEDIA_BUFFER_S {
  void *ptr;
  int fd;
  size_t size;
  MOD_ID_E mode_id;
  std::shared_ptr<easymedia::MediaBuffer> rkmedia_mb;
} MEDIA_BUFFER_IMPLE;

#endif // __RK_BUFFER_IMPL_