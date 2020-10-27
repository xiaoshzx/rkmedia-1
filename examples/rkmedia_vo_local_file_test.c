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

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

static RK_CHAR optstr[] = "?:d:t:w:h:z:i:";
static void print_usage(const RK_CHAR *name) {
  printf("#Usage example:\n");
  printf("\t%s [-d /dev/dri/card0] [-t Primary] [-w 720] [-w 1280] [-z 0] -i "
         "input.yuv\n",
         name);
  printf("\t-d: display card node, Default:\"/dev/dri/card0\"\n");
  printf("\t-w: display width, Default:720\n");
  printf("\t-h: display height, Default:1280\n");
  printf("\t-z: plane zpos, Default:0, value[0, 2]\n");
  printf("\t-i: input path, Default:NULL\n");
  printf("#Note: \n\tPrimary plane format: RGB888\n");
  printf("\n\tOverlay plane format: NV12\n");
}

int main(int argc, char *argv[]) {
  RK_CHAR *pDeviceName = "/dev/dri/card0";
  RK_CHAR *u32PlaneType = "Primary";
  RK_U32 u32Zpos = 0;
  RK_U32 u32DispWidth = 720;
  RK_U32 u32DispHeight = 1280;
  RK_CHAR *pInPath = NULL;
  RK_U32 u32FrameSize = 0;
  int ret = 0;
  int c;

  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
    case 'd':
      pDeviceName = optarg;
      break;
    case 't':
      u32PlaneType = optarg;
      break;
    case 'w':
      u32DispWidth = (RK_U32)atoi(optarg);
      break;
    case 'h':
      u32DispHeight = (RK_U32)atoi(optarg);
      break;
    case 'z':
      u32Zpos = (RK_U32)atoi(optarg);
      break;
    case 'i':
      pInPath = optarg;
      break;
    case '?':
    default:
      print_usage(argv[0]);
      return 0;
    }
  }

  printf("#Device: %s\n", pDeviceName);
  printf("#PlaneType: %s\n", u32PlaneType);
  printf("#Display Width: %d\n", u32DispWidth);
  printf("#Display Height: %d\n", u32DispHeight);
  printf("#Display Zpos: %d\n", u32Zpos);
  printf("#Input Path: %s\n", pInPath);

  FILE *read_file = fopen(pInPath, "r");
  if (!read_file) {
    printf("ERROR: Open %s failed!\n", pInPath);
    return -1;
  }

  RK_MPI_SYS_Init();

  VO_CHN_ATTR_S stVoAttr = {0};
  stVoAttr.pcDevNode = "/dev/dri/card0";
  stVoAttr.u16Fps = 60;
  stVoAttr.u16Zpos = u32Zpos;
  stVoAttr.u32Width = u32DispWidth;
  stVoAttr.u32Height = u32DispHeight;
  stVoAttr.u32HorStride = u32DispWidth;
  stVoAttr.u32VerStride = u32DispHeight;
  if (!strcmp(u32PlaneType, "Primary")) {
    // VO[0] for primary plane
    stVoAttr.emPlaneType = VO_PLANE_PRIMARY;
    stVoAttr.enImgType = IMAGE_TYPE_RGB888;
    u32FrameSize = u32DispWidth * u32DispHeight * 3;
  } else if (!strcmp(u32PlaneType, "Overlay")) {
    // VO[0] for overlay plane
    stVoAttr.emPlaneType = VO_PLANE_OVERLAY;
    stVoAttr.enImgType = IMAGE_TYPE_NV12;
    u32FrameSize = u32DispWidth * u32DispHeight * 3 / 2;
  } else {
    printf("ERROR: Unsupport plane type:%s\n", u32PlaneType);
    return -1;
  }

  ret = RK_MPI_VO_CreateChn(0, &stVoAttr);
  if (ret) {
    printf("Create vo[0] failed! ret=%d\n", ret);
    return -1;
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);
  RK_U32 u32FrameId = 0;
  RK_U32 u32ReadSize = 0;
  RK_U64 u64TimePeriod = 33333; // us
  MB_IMAGE_INFO_S stImageInfo = {u32DispWidth, u32DispHeight, u32DispWidth,
                                 u32DispHeight, stVoAttr.enImgType};
  while (!quit) {
    // Create dma buffer. Note that VO only support dma buffer.
    MEDIA_BUFFER mb = RK_MPI_MB_CreateImageBuffer(&stImageInfo, RK_TRUE);
    if (!mb) {
      printf("ERROR: no space left!\n");
      break;
    }

    u32ReadSize = fread(RK_MPI_MB_GetPtr(mb), 1, u32FrameSize, read_file);
    if (u32ReadSize != u32FrameSize) {
      printf("#Get end of file!\n");
      RK_MPI_MB_ReleaseBuffer(mb);
      break;
    }
    RK_MPI_MB_SetSzie(mb, u32ReadSize);
    printf("#Send frame[%d] fd=%d to VO[0]...\n", u32FrameId++,
           RK_MPI_MB_GetFD(mb));
    ret = RK_MPI_SYS_SendMediaBuffer(RK_ID_VO, 0, mb);
    if (ret) {
      printf("ERROR: RK_MPI_SYS_SendMediaBuffer to VO[0] failed! ret=%d\n",
             ret);
      RK_MPI_MB_ReleaseBuffer(mb);
      break;
    }
    // mb must be release. The encoder has internal references to the data sent
    // in. Therefore, mb cannot be reused directly
    RK_MPI_MB_ReleaseBuffer(mb);
    usleep(u64TimePeriod);
  }

  RK_MPI_VO_DestroyChn(0);
  return 0;
}
