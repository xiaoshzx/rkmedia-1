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

#include "common/sample_common.h"
#include "rkmedia_api.h"
#include "rkmedia_move_detection.h"

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

void move_detection_cb(EVENT_S *pstEvent) {
  if (pstEvent) {
    printf(
        "@@@ MD: ModeID:%d, EventType:%x, Get movement info[%d]: ORI:%dx%d\n",
        pstEvent->mode_id, pstEvent->type, pstEvent->md_event.u16Cnt,
        pstEvent->md_event.u32Width, pstEvent->md_event.u32Height);
    for (int i = 0; i < pstEvent->md_event.u16Cnt; i++) {
      printf("--> %d rect:(%d, %d, %d, %d)\n", i,
             pstEvent->md_event.stRects[i].s32X,
             pstEvent->md_event.stRects[i].s32Y,
             pstEvent->md_event.stRects[i].u32Width,
             pstEvent->md_event.stRects[i].u32Height);
    }
  }
}

int main(int argc, char *argv[]) {
  RK_S32 ret = 0;

  RK_MPI_SYS_Init();
#ifdef RKAIQ
  rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
  RK_BOOL fec_enable = RK_FALSE;
  int fps = 30;
  char *iq_file_dir = NULL;
  if ((argc > 1) && !strcmp(argv[1], "-h")) {
    printf("\n\n/Usage:./%s [--aiq iq_file_dir]\n", argv[0]);
    printf("\t --aiq iq_file_dir : init isp\n");
    return -1;
  }
  if (argc == 3) {
    if (strcmp(argv[1], "--aiq") == 0) {
      iq_file_dir = argv[2];
    }
  }
  SAMPLE_COMM_ISP_Init(hdr_mode, fec_enable, iq_file_dir);
  SAMPLE_COMM_ISP_Run();
  SAMPLE_COMM_ISP_SetFrameRate(fps);
#else
  (void)argc;
  (void)argv;
#endif

  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = "rkispp_scale0";
  vi_chn_attr.u32BufCnt = 4;
  vi_chn_attr.u32Width = 640;
  vi_chn_attr.u32Height = 480;
  vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(0, 1, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, 1);
  if (ret) {
    printf("ERROR: Vi Create failed!\n");
    exit(0);
  }

  ALGO_MD_ATTR_S md_chn_attr;
  md_chn_attr.imageType = IMAGE_TYPE_NV12;
  md_chn_attr.u16Sensitivity = 70;
  md_chn_attr.u32Width = 640;
  md_chn_attr.u32Height = 480;
  md_chn_attr.u16RoiCnt = 2;
  md_chn_attr.stRoiRects[0].s32X = 0;
  md_chn_attr.stRoiRects[0].s32Y = 0;
  md_chn_attr.stRoiRects[0].u32Width = 160;
  md_chn_attr.stRoiRects[0].u32Height = 120;
  md_chn_attr.stRoiRects[1].s32X = 160;
  md_chn_attr.stRoiRects[1].s32Y = 120;
  md_chn_attr.stRoiRects[1].u32Width = 160;
  md_chn_attr.stRoiRects[1].u32Height = 120;
  ret = RK_MPI_ALGO_MD_CreateChn(0, &md_chn_attr);
  if (ret) {
    printf("ERROR: MoveDetection Create failed!\n");
    exit(0);
  }

  MPP_CHN_S stEncChn;
  stEncChn.enModId = RK_ID_ALGO_MD;
  stEncChn.s32DevId = 0;
  stEncChn.s32ChnId = 0;
  ret = RK_MPI_SYS_RegisterEventCb(&stEncChn, move_detection_cb);
  if (ret) {
    printf("ERROR: MoveDetection register event failed!\n");
    exit(0);
  }

  MPP_CHN_S stSrcChn;
  MPP_CHN_S stDestChn;

  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = 1;

  stDestChn.enModId = RK_ID_ALGO_MD;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = 0;
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("ERROR: Bind vi and md failed!\n");
    exit(0);
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  printf("Keep MoveDetection enable for 5s...\n");
  sleep(5);
  printf("Keep MoveDetection disable for 5s...\n");
  ret = RK_MPI_ALGO_MD_EnableSwitch(0, RK_FALSE);
  if (ret) {
    printf("ERROR: Disable MD failed!\n");
    exit(0);
  }
  sleep(5);
  printf("Reset MoveDetection to enable...\n");
  ret = RK_MPI_ALGO_MD_EnableSwitch(0, RK_TRUE);
  if (ret) {
    printf("ERROR: Enable MD failed!\n");
    exit(0);
  }

  while (!quit) {
    usleep(100);
  }

#ifdef RKAIQ
  SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
#endif
  printf("%s exit!\n", __func__);
  RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
  RK_MPI_VI_DisableChn(0, 1);
  RK_MPI_ALGO_MD_DestroyChn(0);

  return 0;
}
