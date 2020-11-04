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

#include <rga/RgaApi.h>

#include "rkmedia_api.h"
#include "rkmedia_venc.h"

#define CROP_TARGET_WIDTH 640
#define CROP_TARGET_HEIGHT 640

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

static void *GetMediaBuffer(void *arg) {
  printf("#Start %s thread, arg:%p\n", __func__, arg);
  rga_info_t src;
  rga_info_t dst;
  MEDIA_BUFFER src_mb = NULL;
  MEDIA_BUFFER dst_mb = NULL;

  int ret = c_RkRgaInit();
  if (ret) {
    printf("ERROR: c_RkRgaInit() failed! ret = %d\n", ret);
    return NULL;
  }

  while (!quit) {
    src_mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, 0, -1);
    if (!src_mb) {
      printf("ERROR: RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
      break;
    }

    MB_IMAGE_INFO_S stImageInfo = {CROP_TARGET_WIDTH, CROP_TARGET_HEIGHT,
                                   CROP_TARGET_WIDTH, CROP_TARGET_HEIGHT,
                                   IMAGE_TYPE_NV12};
    dst_mb = RK_MPI_MB_CreateImageBuffer(&stImageInfo, RK_TRUE, 0);
    if (!dst_mb) {
      printf("ERROR: RK_MPI_MB_CreateImageBuffer get null buffer!\n");
      break;
    }

    memset(&src, 0, sizeof(rga_info_t));
    memset(&dst, 0, sizeof(rga_info_t));

    src.fd = RK_MPI_MB_GetFD(src_mb);
    src.mmuFlag = 1;
    dst.fd = RK_MPI_MB_GetFD(dst_mb);
    dst.mmuFlag = 1;
    rga_set_rect(&src.rect, 0, 0, 1920, 1080, 1920, 1080,
                 RK_FORMAT_YCbCr_420_SP);
    rga_set_rect(&dst.rect, 0, 0, CROP_TARGET_WIDTH, CROP_TARGET_HEIGHT,
                 CROP_TARGET_WIDTH, CROP_TARGET_HEIGHT, RK_FORMAT_YCbCr_420_SP);
    ret = c_RkRgaBlit(&src, &dst, NULL);
    if (ret) {
      printf("ERROR: RkRgaBlit failed! ret = %d\n", ret);
      break;
    }

    RK_MPI_SYS_SendMediaBuffer(RK_ID_VO, 0, dst_mb);
    RK_MPI_MB_ReleaseBuffer(src_mb);
    RK_MPI_MB_ReleaseBuffer(dst_mb);
    src_mb = NULL;
    dst_mb = NULL;
  }

  if (src_mb)
    RK_MPI_MB_ReleaseBuffer(src_mb);
  if (dst_mb)
    RK_MPI_MB_ReleaseBuffer(dst_mb);

  return NULL;
}

int main() {
  int ret = 0;
  RK_MPI_SYS_Init();

  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = "rkispp_scale0";
  vi_chn_attr.u32BufCnt = 4;
  vi_chn_attr.u32Width = 1920;
  vi_chn_attr.u32Height = 1080;
  vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(0, 0, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, 0);
  if (ret) {
    printf("ERROR: Create vi[0] failed! ret=%d\n", ret);
    return -1;
  }

  VO_CHN_ATTR_S stVoAttr = {0};
  stVoAttr.pcDevNode = "/dev/dri/card0";
  stVoAttr.emPlaneType = VO_PLANE_OVERLAY;
  stVoAttr.enImgType = IMAGE_TYPE_NV12;
  stVoAttr.u16Zpos = 1;
  stVoAttr.stImgRect.s32X = 0;
  stVoAttr.stImgRect.s32Y = 0;
  stVoAttr.stImgRect.u32Width = CROP_TARGET_WIDTH;
  stVoAttr.stImgRect.u32Height = CROP_TARGET_HEIGHT;
  stVoAttr.stDispRect.s32X = 0;
  stVoAttr.stDispRect.s32Y = 0;
  stVoAttr.stDispRect.u32Width = CROP_TARGET_WIDTH;
  stVoAttr.stDispRect.u32Height = CROP_TARGET_HEIGHT;
  ret = RK_MPI_VO_CreateChn(0, &stVoAttr);
  if (ret) {
    printf("Create vo[0] failed! ret=%d\n", ret);
    return -1;
  }

  pthread_t read_thread;
  pthread_create(&read_thread, NULL, GetMediaBuffer, NULL);

  usleep(1000); // waite for thread ready.
  ret = RK_MPI_VI_StartStream(0, 0);
  if (ret) {
    printf("ERROR: Start Vi[0] failed! ret=%d\n", ret);
    return -1;
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);
  while (!quit) {
    usleep(100);
  }

  printf("%s exit!\n", __func__);
  RK_MPI_VI_DisableChn(0, 0);
  RK_MPI_VO_DestroyChn(0);

  return 0;
}
