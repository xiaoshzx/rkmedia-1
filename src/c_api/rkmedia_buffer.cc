// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "rkmedia_buffer.h"
#include "image.h"
#include "rkmedia_buffer_impl.h"
#include "rkmedia_utils.h"

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

RK_S16 RK_MPI_MB_GetChannelID(MEDIA_BUFFER mb) {
  if (!mb)
    return -RK_ERR_SYS_ILLEGAL_PARAM;

  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  return mb_impl->chn_id;
}

RK_U64 RK_MPI_MB_GetTimestamp(MEDIA_BUFFER mb) {
  if (!mb)
    return 0;

  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  return mb_impl->timestamp;
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

MEDIA_BUFFER RK_MPI_MB_CreateImageBuffer(MB_IMAGE_INFO_S *pstImageInfo,
                                         RK_BOOL boolHardWare) {
  if (!pstImageInfo || !pstImageInfo->u32Height || !pstImageInfo->u32Width ||
      !pstImageInfo->u32VerStride || !pstImageInfo->u32HorStride)
    return NULL;

  std::string strPixFormat = ImageTypeToString(pstImageInfo->enImgType);
  PixelFormat rkmediaPixFormat = StringToPixFmt(strPixFormat.c_str());
  if (rkmediaPixFormat == PIX_FMT_NONE) {
    LOG("ERROR: %s: unsupport pixformat!\n", __func__);
    return NULL;
  }
  RK_U32 buf_size = CalPixFmtSize(rkmediaPixFormat, pstImageInfo->u32HorStride,
                                  pstImageInfo->u32VerStride, 16);
  if (buf_size == 0)
    return NULL;

  MEDIA_BUFFER_IMPLE *mb = new MEDIA_BUFFER_IMPLE;
  if (!mb) {
    LOG("ERROR: %s: no space left!\n", __func__);
    return NULL;
  }

  auto &&rkmedia_mb = easymedia::MediaBuffer::Alloc(
      buf_size, boolHardWare ? easymedia::MediaBuffer::MemType::MEM_HARD_WARE
                             : easymedia::MediaBuffer::MemType::MEM_COMMON);
  if (!rkmedia_mb) {
    delete mb;
    LOG("ERROR: %s: no space left!\n", __func__);
    return NULL;
  }

  ImageInfo rkmediaImageInfo = {rkmediaPixFormat, (int)pstImageInfo->u32Width,
                                (int)pstImageInfo->u32Height,
                                (int)pstImageInfo->u32HorStride,
                                (int)pstImageInfo->u32VerStride};
  mb->rkmedia_mb = std::make_shared<easymedia::ImageBuffer>(*(rkmedia_mb.get()),
                                                            rkmediaImageInfo);
  mb->ptr = mb->rkmedia_mb->GetPtr();
  mb->fd = mb->rkmedia_mb->GetFD();
  mb->size = 0;
  mb->type = MB_TYPE_IMAGE;
  mb->stImageInfo = *pstImageInfo;
  mb->timestamp = 0;
  mb->mode_id = RK_ID_UNKNOW;
  mb->chn_id = 0;

  return mb;
}

RK_S32 RK_MPI_MB_SetSzie(MEDIA_BUFFER mb, RK_U32 size) {
  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  if (!mb_impl || !mb_impl->rkmedia_mb)
    return -RK_ERR_SYS_ILLEGAL_PARAM;

  mb_impl->size = size;
  if (mb_impl->rkmedia_mb)
    mb_impl->rkmedia_mb->SetValidSize(size);

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_MB_SetTimestamp(MEDIA_BUFFER mb, RK_U64 timestamp) {
  if (!mb)
    return -RK_ERR_SYS_ILLEGAL_PARAM;

  MEDIA_BUFFER_IMPLE *mb_impl = (MEDIA_BUFFER_IMPLE *)mb;
  mb_impl->timestamp = timestamp;
  if (mb_impl->rkmedia_mb)
    mb_impl->rkmedia_mb->SetUSTimeStamp(timestamp);

  return RK_ERR_SYS_OK;
}
