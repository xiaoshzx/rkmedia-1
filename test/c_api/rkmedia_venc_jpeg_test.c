// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rkmedia_api.h"
#include "rkmedia_venc.h"

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

void video_packet_cb(MEDIA_BUFFER mb) {
  static RK_U32 jpeg_id = 0;
  printf("Get JPEG packet[%d]:ptr:%p, fd:%d, size:%zu, mode:%d, channel:%d, timestamp:%lld\n", jpeg_id,
         RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
         RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb), RK_MPI_MB_GetTimestamp(mb));

  char jpeg_path[64];
  sprintf(jpeg_path, "/tmp/test_jpeg%d.jpeg", jpeg_id);
  FILE *file = fopen(jpeg_path, "w");
  if (file) {
    fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), file);
    fclose(file);
  }

  RK_MPI_MB_ReleaseBuffer(mb);
  jpeg_id++;
}

#define TEST_ARGB32_YELLOW 0xFFFFFF00
#define TEST_ARGB32_RED 0xFFFF0033
#define TEST_ARGB32_BLUE 0xFF003399
#define TEST_ARGB32_TRANS 0x00FFFFFF

static void set_argb8888_buffer(RK_U32 *buf, RK_U32 size, RK_U32 color) {
  for (RK_U32 i = 0; buf && (i < size); i++)
    *(buf + i) = color;
}

int main() {
  RK_S32 ret;

  ret = RK_MPI_SYS_Init();
  if (ret) {
    printf("Sys Init failed! ret=%d\n", ret);
    return -1;
  }

  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.buffer_cnt = 4;
  vi_chn_attr.width = 1920;
  vi_chn_attr.height = 1080;
  vi_chn_attr.pix_fmt = IMAGE_TYPE_NV12;
  ret = RK_MPI_VI_SetChnAttr(0, 1, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, 1);
  if (ret) {
    printf("Create Vi failed! ret=%d\n", ret);
    return -1;
  }

  VENC_CHN_ATTR_S venc_chn_attr;
  memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));
  venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_JPEG;
  venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
  venc_chn_attr.stVencAttr.u32PicWidth = 1920;
  venc_chn_attr.stVencAttr.u32PicHeight = 1080;
  venc_chn_attr.stVencAttr.u32VirWidth = 1920;
  venc_chn_attr.stVencAttr.u32VirHeight = 1080;
  ret = RK_MPI_VENC_CreateChn(0, &venc_chn_attr);
  if (ret) {
    printf("Create Venc failed! ret=%d\n", ret);
    return -1;
  }

  MPP_CHN_S stEncChn;
  stEncChn.enModId = RK_ID_VENC;
  stEncChn.s32ChnId = 0;
  ret = RK_MPI_SYS_RegisterOutCb(&stEncChn, video_packet_cb);
  if (ret) {
    printf("Register Output callback failed! ret=%d\n", ret);
    return -1;
  }

  // The encoder defaults to continuously receiving frames from the previous
  // stage. Before performing the bind operation, set s32RecvPicNum to 0 to
  // make the encoding enter the pause state.
  VENC_RECV_PIC_PARAM_S stRecvParam;
  stRecvParam.s32RecvPicNum = 0;
  RK_MPI_VENC_StartRecvFrame(0, &stRecvParam);

  MPP_CHN_S stSrcChn;
  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32ChnId = 1;
  MPP_CHN_S stDestChn;
  stDestChn.enModId = RK_ID_VENC;
  stDestChn.s32ChnId = 0;
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("Register Output callback failed! ret=%d\n", ret);
    return -1;
  }

  RK_MPI_VENC_InitOsd(0);

  BITMAP_S BitMap;
  BitMap.enPixelFormat = PIXEL_FORMAT_ARGB_8888;
  BitMap.u32Width = 64;
  BitMap.u32Height = 256;
  BitMap.pData = malloc(BitMap.u32Width * 4 * BitMap.u32Height);
  RK_U8 *ColorData = (RK_U8 *)BitMap.pData;
  RK_U16 ColorBlockSize = BitMap.u32Height * BitMap.u32Width;
  set_argb8888_buffer((RK_U32 *)ColorData, ColorBlockSize / 4,
                      TEST_ARGB32_YELLOW);
  set_argb8888_buffer((RK_U32 *)(ColorData + ColorBlockSize),
                      ColorBlockSize / 4, TEST_ARGB32_TRANS);
  set_argb8888_buffer((RK_U32 *)(ColorData + 2 * ColorBlockSize),
                      ColorBlockSize / 4, TEST_ARGB32_RED);
  set_argb8888_buffer((RK_U32 *)(ColorData + 3 * ColorBlockSize),
                      ColorBlockSize / 4, TEST_ARGB32_BLUE);

  // Case 1: Canvas and bitmap are equal in size
  OSD_REGION_INFO_S RngInfo;
  RngInfo.enRegionId = REGION_ID_0;
  RngInfo.u32PosX = 0;
  RngInfo.u32PosY = 0;
  RngInfo.u32Width = 64;
  RngInfo.u32Height = 256;
  RngInfo.u8Enable = 1;
  RngInfo.u8Inverse = 0;
  RK_MPI_VENC_SetBitMap(0, &RngInfo, &BitMap);

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  char cmd[64];
  printf("#Usage: input 'quit' to exit programe!\n"
         "peress any other key to capture one picture to file\n");
  while (!quit) {
    fgets(cmd, sizeof(cmd), stdin);
    printf("#Input cmd:%s\n", cmd);
    if (strstr(cmd, "quit")) {
      printf("#Get 'quit' cmd!\n");
      break;
    }
    stRecvParam.s32RecvPicNum = 1;
    ret = RK_MPI_VENC_StartRecvFrame(0, &stRecvParam);
    if (ret) {
      printf("RK_MPI_VENC_StartRecvFrame failed!\n");
      break;
    }
    usleep(30000); // sleep 30 ms.
  }

  printf("%s exit!\n", __func__);
  RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
  RK_MPI_VI_DisableChn(0, 1);
  RK_MPI_VENC_DestroyChn(0);

  return 0;
}
