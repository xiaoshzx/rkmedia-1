// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "rkmedia_api.h"

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

void occlusion_detection_cb(EVENT_S *pstEvent) {
  if (pstEvent) {
    printf(
        "@@@ OD: ModeID:%d, EventType:%x, Get movement info[%d]: ORI:%dx%d\n",
        pstEvent->mode_id, pstEvent->type, pstEvent->stOdEvent.u16Cnt,
        pstEvent->stOdEvent.u32Width, pstEvent->stOdEvent.u32Height);
    for (int i = 0; i < pstEvent->stOdEvent.u16Cnt; i++) {
      printf("--> %d rect:(%d, %d, %d, %d), Occlusion:%d\n", i,
             pstEvent->stOdEvent.stRects[i].s32X,
             pstEvent->stOdEvent.stRects[i].s32Y,
             pstEvent->stOdEvent.stRects[i].u32Width,
             pstEvent->stOdEvent.stRects[i].u32Height,
             pstEvent->stOdEvent.u16Occlusion[i]);
    }
  }
}

int main() {
  RK_S32 ret = 0;

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
    printf("ERROR: Vi Create failed! ret=%d\n", ret);
    exit(0);
  }

  ALGO_OD_ATTR_S stOdChnAttr;
  stOdChnAttr.enImageType = IMAGE_TYPE_NV12;
  stOdChnAttr.u32Width = 1920;
  stOdChnAttr.u32Height = 1080;
  stOdChnAttr.u16RoiCnt = 1;
  stOdChnAttr.stRoiRects[0].s32X = 0;
  stOdChnAttr.stRoiRects[0].s32Y = 0;
  stOdChnAttr.stRoiRects[0].u32Width = 1920;
  stOdChnAttr.stRoiRects[0].u32Height = 1080;
  stOdChnAttr.u16Sensitivity = 30;
  ret = RK_MPI_ALGO_OD_CreateChn(0, &stOdChnAttr);
  if (ret) {
    printf("ERROR: OcclusionDetection Create failed! ret=%d\n", ret);
    exit(0);
  }

  MPP_CHN_S stEncChn;
  stEncChn.enModId = RK_ID_ALGO_OD;
  stEncChn.s32DevId = 0;
  stEncChn.s32ChnId = 0;
  ret = RK_MPI_SYS_RegisterEventCb(&stEncChn, occlusion_detection_cb);
  if (ret) {
    printf("ERROR: MoveDetection register event failed! ret=%d\n", ret);
    exit(0);
  }

  MPP_CHN_S stSrcChn;
  MPP_CHN_S stDestChn;

  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = 0;

  stDestChn.enModId = RK_ID_ALGO_OD;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = 0;
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("ERROR: Bind vi and od failed! ret=%d\n", ret);
    exit(0);
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  while (!quit) {
    usleep(100);
  }

  printf("%s exit!\n", __func__);
  RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
  RK_MPI_VI_DisableChn(0, 0);
  RK_MPI_ALGO_OD_DestroyChn(0);

  return 0;
}
