// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <fcntl.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "rkmedia_api.h"
#include "rkmedia_venc.h"

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

static void *GetMediaBuffer(void *arg) {
  printf("#Start %s thread, arg:%p\n", __func__, arg);
  FILE *save_file = fopen("/userdata/output.h264", "w");
  if (!save_file)
    printf("ERROR: Open /userdata/output.h264 failed!\n");

  MEDIA_BUFFER mb = NULL;
  while (!quit) {
    mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VENC, 0, -1);
    if (!mb) {
      printf("RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
      break;
    }

    printf("Get packet:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
           "timestamp:%lld\n",
           RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
           RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb),
           RK_MPI_MB_GetTimestamp(mb));

    if (save_file)
      fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), save_file);
    RK_MPI_MB_ReleaseBuffer(mb);
  }

  return NULL;
}

int main() {
  RK_S32 ret = 0;
  RK_MPI_SYS_Init();

  VENC_CHN_ATTR_S venc_chn_attr;
  venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
  venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
  venc_chn_attr.stVencAttr.u32PicWidth = 1920;
  venc_chn_attr.stVencAttr.u32PicHeight = 1080;
  venc_chn_attr.stVencAttr.u32VirWidth = 1920;
  venc_chn_attr.stVencAttr.u32VirHeight = 1080;
  venc_chn_attr.stVencAttr.u32Profile = 77;
  venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
  venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 30;
  venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = 1920 * 1080 * 30 / 14;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
  ret = RK_MPI_VENC_CreateChn(0, &venc_chn_attr);
  if (ret) {
    printf("ERROR: Create venc failed!\n");
    exit(0);
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  FILE *read_file = fopen("/userdata/1080p.nv12", "r");
  if (!read_file) {
    printf("ERROR: open /userdata/1080p.nv12 failed!\n");
    exit(0);
  }

  MB_IMAGE_INFO_S stImageInfo = {1920, 1080, 1920, 1080, IMAGE_TYPE_NV12};

  pthread_t read_thread;
  pthread_create(&read_thread, NULL, GetMediaBuffer, NULL);

  RK_U32 u32FrameId = 0;
  RK_S32 s32ReadSize = 0;
  RK_U64 u64TimePeriod = 33333; // us
  while (!quit) {
    // Create dma buffer. Note that mpp encoder only support dma buffer.
    MEDIA_BUFFER mb = RK_MPI_MB_CreateImageBuffer(&stImageInfo, RK_TRUE);
    if (!mb) {
      printf("ERROR: no space left!\n");
      break;
    }

    // One frame size for nv12 image.
    // 3110400 = 1920 * 1080 * 3 / 2;
    s32ReadSize = fread(RK_MPI_MB_GetPtr(mb), 1, 3110400, read_file);
    if (s32ReadSize != 3110400) {
      printf("Get end of file!\n");
      break;
    }
    RK_MPI_MB_SetSzie(mb, 3110400);
    RK_MPI_MB_SetTimestamp(mb, u32FrameId * u64TimePeriod);
    printf("#Send frame[%d] fd=%d to venc[0]...\n", u32FrameId++,
           RK_MPI_MB_GetFD(mb));
    RK_MPI_SYS_SendMediaBuffer(RK_ID_VENC, 0, mb);
    // mb must be release. The encoder has internal references to the data sent
    // in. Therefore, mb cannot be reused directly
    RK_MPI_MB_ReleaseBuffer(mb);
    usleep(u64TimePeriod);
  }

  while (!quit) {
    usleep(100000);
  }

  printf("%s exit!\n", __func__);
  RK_MPI_VENC_DestroyChn(0);

  return 0;
}
