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

#include "common/sample_common.h"
#include "librtsp/rtsp_demo.h"
#include "rkmedia_api.h"
#include "rkmedia_venc.h"

#define CROP_TARGET_WIDTH 640
#define CROP_TARGET_HEIGHT 640

static bool quit = false;
RK_U32 g_width = 1920;
RK_U32 g_height = 1080;
char *g_video_node = "rkispp_scale0";
IMAGE_TYPE_E g_enPixFmt = IMAGE_TYPE_NV12;
RK_S32 g_S32Rotation = 0;
MPP_CHN_S g_stViChn;
MPP_CHN_S g_stVencChn;

static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

static void *GetVencBuffer(void *arg) {
  printf("#Start %s thread, arg:%p\n", __func__, arg);
  // init rtsp
  rtsp_demo_handle rtsplive = NULL;
  rtsp_session_handle session;
  rtsplive = create_rtsp_demo(554);
  session = rtsp_new_session(rtsplive, "/live/main_stream");
  rtsp_set_video(session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
  rtsp_sync_video_ts(session, rtsp_get_reltime(), rtsp_get_ntptime());

  MEDIA_BUFFER mb = NULL;
  while (!quit) {
    mb = RK_MPI_SYS_GetMediaBuffer(g_stVencChn.enModId, g_stVencChn.s32ChnId,
                                   -1);
    printf("-Get Video Encoded packet():ptr:%p, fd:%d, size:%zu, mode:%d, time "
           "= %llu.\n",
           RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb), RK_MPI_MB_GetSize(mb),
           RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetTimestamp(mb));
    rtsp_tx_video(session, RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb),
                  RK_MPI_MB_GetTimestamp(mb));
    RK_MPI_MB_ReleaseBuffer(mb);
    rtsp_do_event(rtsplive);
  }
  // release rtsp
  rtsp_del_demo(rtsplive);
  return NULL;
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
    src_mb =
        RK_MPI_SYS_GetMediaBuffer(g_stViChn.enModId, g_stViChn.s32ChnId, -1);
    if (!src_mb) {
      printf("ERROR: RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
      break;
    }

    MB_IMAGE_INFO_S stImageInfo = {CROP_TARGET_WIDTH, CROP_TARGET_HEIGHT,
                                   CROP_TARGET_WIDTH, CROP_TARGET_HEIGHT,
                                   IMAGE_TYPE_NV12};
    dst_mb = RK_MPI_MB_CreateImageBuffer(&stImageInfo, RK_TRUE);
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
    RK_MPI_MB_SetTimestamp(dst_mb, RK_MPI_MB_GetTimestamp(src_mb));
    rga_set_rect(&src.rect, 0, 0, 1920, 1080, 1920, 1080,
                 RK_FORMAT_YCbCr_420_SP);
    rga_set_rect(&dst.rect, 0, 0, CROP_TARGET_WIDTH, CROP_TARGET_HEIGHT,
                 CROP_TARGET_WIDTH, CROP_TARGET_HEIGHT, RK_FORMAT_YCbCr_420_SP);
    ret = c_RkRgaBlit(&src, &dst, NULL);
    if (ret) {
      printf("ERROR: RkRgaBlit failed! ret = %d\n", ret);
      break;
    }

    VENC_RESOLUTION_PARAM_S stResolution;
    stResolution.u32Width = CROP_TARGET_WIDTH;
    stResolution.u32Height = CROP_TARGET_HEIGHT;
    stResolution.u32VirWidht = CROP_TARGET_WIDTH;
    stResolution.u32VirHeight = CROP_TARGET_HEIGHT;

    RK_MPI_VENC_SetResolution(g_stVencChn.s32ChnId, stResolution);
    RK_MPI_SYS_SendMediaBuffer(g_stVencChn.enModId, g_stVencChn.s32ChnId,
                               dst_mb);
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

int main(int argc, char *argv[]) {
  int ret = 0;
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

  RK_MPI_SYS_Init();
  g_stViChn.enModId = RK_ID_VI;
  g_stViChn.s32DevId = 0;
  g_stViChn.s32ChnId = 1;
  g_stVencChn.enModId = RK_ID_VENC;
  g_stVencChn.s32DevId = 0;
  g_stVencChn.s32ChnId = 0;

  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = g_video_node;
  vi_chn_attr.u32BufCnt = 4;
  vi_chn_attr.u32Width = g_width;
  vi_chn_attr.u32Height = g_height;
  vi_chn_attr.enPixFmt = g_enPixFmt;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(g_stViChn.s32DevId, g_stViChn.s32ChnId,
                             &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(g_stViChn.s32DevId, g_stViChn.s32ChnId);
  if (ret) {
    printf("ERROR: Create vi[0] failed! ret=%d\n", ret);
    return -1;
  }

  VENC_CHN_ATTR_S venc_chn_attr;
  venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
  venc_chn_attr.stVencAttr.imageType = g_enPixFmt;
  venc_chn_attr.stVencAttr.u32PicWidth = g_width;
  venc_chn_attr.stVencAttr.u32PicHeight = g_height;
  venc_chn_attr.stVencAttr.u32VirWidth = g_width;
  venc_chn_attr.stVencAttr.u32VirHeight = g_height;
  venc_chn_attr.stVencAttr.u32Profile = 77;
  venc_chn_attr.stVencAttr.enRotation = g_S32Rotation;

  venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;

  venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 30;
  venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = g_width * g_height * 30 / 14;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;

  RK_MPI_VENC_CreateChn(g_stVencChn.s32ChnId, &venc_chn_attr);

  pthread_t read_thread;
  pthread_create(&read_thread, NULL, GetMediaBuffer, NULL);
  pthread_t venc_thread;
  pthread_create(&venc_thread, NULL, GetVencBuffer, NULL);

  usleep(1000); // waite for thread ready.
  ret = RK_MPI_VI_StartStream(g_stViChn.s32DevId, g_stViChn.s32ChnId);
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
  pthread_join(read_thread, NULL);
  pthread_join(venc_thread, NULL);
  RK_MPI_VI_DisableChn(g_stViChn.s32DevId, g_stViChn.s32ChnId);
  RK_MPI_VENC_DestroyChn(g_stVencChn.s32ChnId);

#ifdef RKAIQ
  SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
#endif
  return 0;
}
