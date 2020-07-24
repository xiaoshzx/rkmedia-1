// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rkmedia_buffer.h"
#include "rkmedia_buffer_impl.h"

void *RK_MPI_MB_GetPtr(MEDIA_BUFFER mb) {
  if (!mb)
    return NULL;

  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  return mb_impl->ptr;
}

int RK_MPI_MB_GetFD(MEDIA_BUFFER mb) {
  if (!mb)
    return 0;

  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  return mb_impl->fd;
}

size_t RK_MPI_MB_GetSize(MEDIA_BUFFER mb) {
  if (!mb)
    return 0;

  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  return mb_impl->size;
}

MOD_ID_E RK_MPI_MB_GetModeID(MEDIA_BUFFER mb) {
  if (!mb)
    return RK_ID_UNKNOW;

  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  return mb_impl->mode_id;
}

RK_U16 RK_MPI_MB_GetChannelID(MEDIA_BUFFER mb) {
  if (!mb)
    return RK_ID_UNKNOW;

  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  return mb_impl->chn_id;
}

RK_S32 RK_MPI_MB_ReleaseBuffer(MEDIA_BUFFER mb) {
  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  if (!mb)
    return -RK_ERR_SYS_ILLEGAL_PARAM;

  if (mb_impl->rkmedia_mb)
    mb_impl->rkmedia_mb.reset();

  delete mb_impl;
  return RK_ERR_SYS_OK;
}
