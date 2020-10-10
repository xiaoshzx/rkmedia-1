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
#include "rkmedia_venc.h"

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

int main(int argc, char *argv[]) {
  int ret = 0;

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
  vi_chn_attr.u32Width = 1920;
  vi_chn_attr.u32Height = 1080;
  vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_LUMA_ONLY;
  ret = RK_MPI_VI_SetChnAttr(0, 1, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, 1);
  ret |= RK_MPI_VI_StartStream(0, 1);
  if (ret) {
    printf("ERROR: create VI[0] error! ret=%d\n", ret);
    return 0;
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);
  sleep(3);

  RECT_S stRects[2] = {{0, 0, 256, 256}, {256, 256, 256, 256}};
  VIDEO_REGION_INFO_S stVideoRgn;
  stVideoRgn.pstRegion = stRects;
  stVideoRgn.u32RegionNum = 2;
  RK_U64 u64LumaData[2];
  while (!quit) {
    ret = RK_MPI_VI_GetChnRegionLuma(0, 1, &stVideoRgn, u64LumaData, 100);
    if (ret) {
      printf("ERROR: get luma from VI[0] error! ret=%d\n", ret);
      break;
    }
    printf("Rect[0] {0, 0, 256, 256} -> luma:%lld\n", u64LumaData[0]);
    printf("Rect[1] {256, 256, 256, 256} -> luma:%lld\n", u64LumaData[1]);
    usleep(100000); // 100ms
  }
#ifdef RKAIQ
  SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
#endif
  printf("%s exit!\n", __func__);
  RK_MPI_VI_DisableChn(0, 1);

  return 0;
}
