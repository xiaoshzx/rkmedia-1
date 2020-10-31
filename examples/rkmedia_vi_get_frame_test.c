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

#include "common/sample_common.h"
#include "rkmedia_api.h"
#include "rkmedia_venc.h"

typedef struct {
  char *file_path;
  int frame_cnt;
} OutputArgs;

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

static void *GetMediaBuffer(void *arg) {
  OutputArgs *outArgs = (OutputArgs *)arg;
  char *save_path = outArgs->file_path;
  int save_cnt = outArgs->frame_cnt;
  FILE *save_file = fopen(save_path, "w");
  if (!save_file)
    printf("ERROR: Open %s failed!\n", save_path);

  MEDIA_BUFFER mb = NULL;
  while (!quit) {
    mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_VI, 0, -1);
    if (!mb) {
      printf("RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
      break;
    }

    printf("Get Frame:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, "
           "timestamp:%lld\n",
           RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
           RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb),
           RK_MPI_MB_GetTimestamp(mb));

    if (save_file && (save_cnt-- > 0)) {
      fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), save_file);
      printf("#Save frame-%d to %s\n", save_cnt, save_path);
    }

    RK_MPI_MB_ReleaseBuffer(mb);
  }

  if (save_file)
    fclose(save_file);

  return NULL;
}

static RK_CHAR optstr[] = "?:d:w:h:c:o:a:x:";
static void print_usage(const RK_CHAR *name) {
  printf("usage example:\n");
  printf("\t%s -d rkispp_scale0 -w 1920 -h 1080 -c 10 -o test.yuv\n",
         name);
  printf("\t-d: device node, Default: NULL\n");
  printf("\t-w: width, Default:1920\n");
  printf("\t-h: height, Default:1080\n");
  printf("\t-c: frames cnt, Default:10\n");
  printf("\t-o: output path, Default:NULL\n");
  printf("\t-a: enable aiq api, Default:NO, Value:0,NO;1,YES.\n");
  printf("\t-x: iqfile Path, Default:/oem/etc/iqfiles\n");
  printf("Notice: fmt always NV12\n");
}

int main(int argc, char *argv[]) {
  RK_U32 u32Width = 1920;
  RK_U32 u32Height = 1080;
  RK_U32 u32FrameCnt = 10;
  RK_CHAR *pDeviceName = "rkispp_scale0";
  RK_CHAR *pOutPath = NULL;
  RK_CHAR *pIqfilesPath = "/oem/etc/iqfiles";
  RK_U32 u32AiqEnable = 0;
  int c;
  int ret = 0;

  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
    case 'd':
      pDeviceName = optarg;
      break;
    case 'w':
      u32Width = atoi(optarg);
      break;
    case 'h':
      u32Height = atoi(optarg);
      break;
    case 'c':
      u32FrameCnt = atoi(optarg);
      break;
    case 'o':
      pOutPath = optarg;
      break;
    case 'a':
      u32AiqEnable = atoi(optarg);
      break;
    case 'x':
      pIqfilesPath = optarg;
      break;
    case '?':
    default:
      print_usage(argv[0]);
      return 0;
    }
  }

  printf("\n#############################\n");
  printf("#Device: %s\n", pDeviceName);
  printf("#Resolution: %dx%d\n", u32Width, u32Height);
  printf("#Frame Count to save: %d\n", u32FrameCnt);
  printf("#Output Path: %s\n", pOutPath);
  if (u32AiqEnable) {
    printf("#Enable Aiq: %s\n", u32AiqEnable ? "TRUE" : "FALSE");
    printf("#Aiq xml path: %s\n\n", pIqfilesPath);
  }

  RK_MPI_SYS_Init();
  if (u32AiqEnable) {
#ifdef RKAIQ
    rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
    RK_BOOL fec_enable = RK_FALSE;
    int fps = 30;
    SAMPLE_COMM_ISP_Init(hdr_mode, fec_enable, pIqfilesPath);
    SAMPLE_COMM_ISP_Run();
    SAMPLE_COMM_ISP_SetFrameRate(fps);
#endif
  }

  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = "rkispp_scale0";
  vi_chn_attr.u32BufCnt = 4;
  vi_chn_attr.u32Width = u32Width;
  vi_chn_attr.u32Height = u32Height;
  vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(0, 0, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, 0);
  if (ret) {
    printf("Create VI[0] failed! ret=%d\n", ret);
    return -1;
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  pthread_t read_thread;
  OutputArgs outArgs = { pOutPath, u32FrameCnt };
  pthread_create(&read_thread, NULL, GetMediaBuffer, &outArgs);
  ret = RK_MPI_VI_StartStream(0, 0);
  if (ret) {
    printf("Start VI[0] failed! ret=%d\n", ret);
    return -1;
  }

  while (!quit) {
    usleep(100);
  }

#ifdef RKAIQ
  if (u32AiqEnable)
    SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
#endif

  printf("%s exit!\n", __func__);
  RK_MPI_VI_DisableChn(0, 0);

  return 0;
}
