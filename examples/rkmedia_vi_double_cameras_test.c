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

static FILE *g_file0;
static FILE *g_file1;

void video_packet_cb(MEDIA_BUFFER mb) {
  const char *nalu_type = "Unknow";
  switch (RK_MPI_MB_GetFlag(mb)) {
  case VENC_NALU_IDRSLICE:
    nalu_type = "IDR Slice";
    break;
  case VENC_NALU_PSLICE:
    nalu_type = "P Slice";
    break;
  default:
    break;
  }
  printf("Camera-%d::Get Video Encoded packet(%s):ptr:%p, fd:%d, size:%zu\n",
         RK_MPI_MB_GetChannelID(mb), nalu_type, RK_MPI_MB_GetPtr(mb),
         RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb));

  FILE *file = RK_MPI_MB_GetChannelID(mb) ? g_file0 : g_file1;
  if (file)
    fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), file);

  RK_MPI_MB_ReleaseBuffer(mb);
}

static int StreamOn(const char *video_node, int width, int height,
                    IMAGE_TYPE_E img_type, int vi_chn, int venc_chn) {
  int ret = 0;

  printf("===> StreamOn: Start: video_node:%s, wxh:%dx%d, vi_chn:%d, "
         "venc_chn:%d <===\n",
         video_node, width, height, vi_chn, venc_chn);

  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = video_node;
  vi_chn_attr.u32BufCnt = 4;
  vi_chn_attr.u32Width = width;
  vi_chn_attr.u32Height = height;
  vi_chn_attr.enPixFmt = img_type;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(0, vi_chn, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, vi_chn);
  if (ret) {
    printf("ERROR: Create VI[%d]:%s failed!\n", vi_chn, video_node);
    return -1;
  }

  VENC_CHN_ATTR_S venc_chn_attr;
  venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
  venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
  venc_chn_attr.stVencAttr.u32PicWidth = width;
  venc_chn_attr.stVencAttr.u32PicHeight = height;
  venc_chn_attr.stVencAttr.u32VirWidth = width;
  venc_chn_attr.stVencAttr.u32VirHeight = height;
  venc_chn_attr.stVencAttr.u32Profile = 77;
  venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
  venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 30;
  venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = width * height * 30 / 14;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 1;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 1;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
  ret = RK_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
  if (ret) {
    printf("ERROR: Create Venc[%d] failed! ret=%d\n", venc_chn, ret);
    return -1;
  }

  MPP_CHN_S stEncChn;
  stEncChn.enModId = RK_ID_VENC;
  stEncChn.s32DevId = 0;
  stEncChn.s32ChnId = venc_chn;
  ret = RK_MPI_SYS_RegisterOutCb(&stEncChn, video_packet_cb);
  if (ret) {
    printf("ERROR: RegisterOutCb for Venc[%d] failed! ret = %d\n", venc_chn,
           ret);
    return -1;
  }

  MPP_CHN_S stSrcChn;
  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = vi_chn;
  MPP_CHN_S stDestChn;
  stDestChn.enModId = RK_ID_VENC;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = venc_chn;
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("ERROR: Bind Vi[%d] and Venc[%d] failed! ret = %d\n", vi_chn,
           venc_chn, ret);
    return -1;
  }

  return 0;
}

static int StreamOff(int vi_chn, int venc_chn) {
  int ret = 0;

  MPP_CHN_S stSrcChn;
  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = vi_chn;
  MPP_CHN_S stDestChn;
  stDestChn.enModId = RK_ID_VENC;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = venc_chn;
  ret = RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("ERROR: UnBind Vi[%d] and Venc[%d] failed! ret = %d\n", vi_chn,
           venc_chn, ret);
    return -1;
  }
  ret = RK_MPI_VI_DisableChn(vi_chn, 1);
  if (ret) {
    printf("ERROR: Destroy Vi[%d] failed! ret = %d\n", vi_chn, ret);
    return -1;
  }
  ret = RK_MPI_VENC_DestroyChn(venc_chn);
  if (ret) {
    printf("ERROR: Destroy Venc[%d] failed! ret = %d\n", venc_chn, ret);
    return -1;
  }

  return 0;
}

static void print_usage(char *name) {
  printf("#Usage: \n");
  printf("  %s VideoNode0 VideoNode1\n", name);
  printf("  VideoNode0: video node for camera0, such as:\"/dev/video31\"\n");
  printf("  VideoNode1: video node for camera1, such as:\"/dev/video38\"\n");
}

int main(int argc, char *argv[]) {
  int ret = 0;

  if (argc != 3) {
    print_usage(argv[0]);
    return -1;
  }

  printf("#Camera0:%s, wxh:1080p\n", argv[1]);
  printf("#Camera1:%s, wxh:720p\n", argv[2]);

  RK_MPI_SYS_Init();

  g_file0 = fopen("/userdata/camera0.h264", "w");
  g_file1 = fopen("/userdata/camera1.h264", "w");

  ret = StreamOn("/dev/video32", 1920, 1080, IMAGE_TYPE_NV12, 0, 0);
  if (ret)
    exit(0);

  ret = StreamOn("/dev/video38", 1280, 720, IMAGE_TYPE_NV12, 1, 1);
  if (ret)
    exit(0);

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  while (!quit) {
    usleep(100);
  }

  StreamOff(0, 0);
  StreamOff(1, 1);

  if (g_file0)
    fclose(g_file0);
  if (g_file1)
    fclose(g_file1);

  return 0;
}
