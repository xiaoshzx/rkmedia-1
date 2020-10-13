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
  quit = false;
}

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
  printf("Get Video Encoded packet(%s):ptr:%p, fd:%d, size:%zu, mode:%d\n",
         nalu_type, RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetFD(mb),
         RK_MPI_MB_GetSize(mb), RK_MPI_MB_GetModeID(mb));
  RK_MPI_MB_ReleaseBuffer(mb);
}

RK_U32 g_width = 1920;
RK_U32 g_height = 1080;
char *g_video_node = "rkispp_scale0";
IMAGE_TYPE_E g_enPixFmt = IMAGE_TYPE_NV12;

static void StreamOnOff(RK_BOOL start) {
  MPP_CHN_S stSrcChn;
  MPP_CHN_S stDestChn;
  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = 1;

  stDestChn.enModId = RK_ID_VENC;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = 0;
  if (start) {
    VENC_CHN_ATTR_S venc_chn_attr;
    venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
    venc_chn_attr.stVencAttr.imageType = g_enPixFmt;
    venc_chn_attr.stVencAttr.u32PicWidth = g_width;
    venc_chn_attr.stVencAttr.u32PicHeight = g_height;
    venc_chn_attr.stVencAttr.u32VirWidth = g_width;
    venc_chn_attr.stVencAttr.u32VirHeight = g_height;
    venc_chn_attr.stVencAttr.u32Profile = 77;

    venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;

    venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 30;
    venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = g_width * g_height * 30 / 14;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 0;
    venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 0;
    venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;

    VI_CHN_ATTR_S vi_chn_attr;
    vi_chn_attr.pcVideoNode = g_video_node;
    vi_chn_attr.u32BufCnt = 4;
    vi_chn_attr.u32Width = g_width;
    vi_chn_attr.u32Height = g_height;
    vi_chn_attr.enPixFmt = g_enPixFmt;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;

    RK_MPI_VI_SetChnAttr(stSrcChn.s32DevId, stSrcChn.s32ChnId, &vi_chn_attr);
    RK_MPI_VI_EnableChn(stSrcChn.s32DevId, stSrcChn.s32ChnId);
    RK_MPI_VENC_CreateChn(stDestChn.s32ChnId, &venc_chn_attr);

    RK_MPI_SYS_RegisterOutCb(&stDestChn, video_packet_cb);

    RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);

    VENC_RC_PARAM_S venc_rc_param;
    venc_rc_param.s32FirstFrameStartQp = 30;
    venc_rc_param.stParamH264.u32StepQp = 6;
    venc_rc_param.stParamH264.u32MinQp = 20;
    venc_rc_param.stParamH264.u32MaxQp = 51;
    venc_rc_param.stParamH264.u32MinIQp = 24;
    venc_rc_param.stParamH264.u32MaxIQp = 51;
    sleep(3);
    printf("%s: start set qp.\n", __func__);
    RK_MPI_VENC_SetRcParam(stDestChn.s32ChnId, &venc_rc_param);
    printf("%s: after set qp.\n", __func__);
    printf("%s exit!\n", __func__);
  } else {
    RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
    RK_MPI_VENC_DestroyChn(stSrcChn.s32ChnId);
    RK_MPI_VI_DisableChn(stDestChn.s32DevId, stDestChn.s32ChnId);
  }
}

static void streamOn() { StreamOnOff(RK_TRUE); }
static void streamOff() { StreamOnOff(RK_FALSE); }
static void startISP(rk_aiq_working_mode_t hdr_mode, RK_BOOL fec_enable,
                     char *iq_file_dir) {
  // hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
  // fec_enable = RK_FALSE;
  int fps = 30;
  printf("hdr mode %d, fec mode %d, fps %d\n", hdr_mode, fec_enable, fps);
  SAMPLE_COMM_ISP_Init(hdr_mode, fec_enable, iq_file_dir);
  SAMPLE_COMM_ISP_Run();
  SAMPLE_COMM_ISP_SetFrameRate(fps);
}

static void testNormalToHdr2(char *iq_file_dir) {
  quit = true;
  RK_U32 count = 0;
  rk_aiq_working_mode_t hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
  startISP(hdr_mode, RK_FALSE, iq_file_dir); // isp aiq start before vi streamon
  streamOn();
  while (quit) {
    count++;
    if (hdr_mode == RK_AIQ_WORKING_MODE_NORMAL) {
      hdr_mode = RK_AIQ_WORKING_MODE_ISP_HDR2;
      printf("set hdr mode to hdr2 count = %u.\n", count);
    } else {
      hdr_mode = RK_AIQ_WORKING_MODE_NORMAL;
      printf("set hdr mode to normal count = %u.\n", count);
    }

    SAMPLE_COMM_ISP_SET_HDR(hdr_mode);
    usleep(100 * 1000); // sleep 30 ms.
  }
  SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
  streamOff();
}

static void testNormalToHdr2ToHdr3(char *iq_file_dir) {
  quit = true;
  RK_U32 count = 0;
  while (quit) {
    count++;
    // normal
    printf("######### normal : count = %u #############.\n", count);
    startISP(RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, iq_file_dir);
    streamOn();
    usleep(100 * 10000);

    // hdr2
    printf("############ hdr2 : count = %u ###############.\n", count);
    SAMPLE_COMM_ISP_SET_HDR(RK_AIQ_WORKING_MODE_ISP_HDR2);
    usleep(100 * 10000);

    // hdr3:  go or leave hdr3, must restart aiq
    printf("############ hdr3 : count = %u ##############.\n", count);
    SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
    streamOff();
    startISP(RK_AIQ_WORKING_MODE_ISP_HDR3, RK_FALSE, iq_file_dir);
    streamOn();
    usleep(100 * 10000);
    SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
    streamOff();
  }
}

static RK_VOID testFBCRotation(char *iq_file_dir) {
  quit = true;
  g_width = 3840;
  g_height = 2160;
  g_video_node = "rkispp_m_bypass";
  g_enPixFmt = IMAGE_TYPE_FBC0;
  RK_U32 count = 0;
  RK_S32 S32Rotation = 0;
  while (quit) {
    count++;
    if (S32Rotation > 270) {
      S32Rotation = 0;
    } else {
      S32Rotation += 90;
    }
    printf("######### %d : count = %u #############.\n", S32Rotation, count);
    startISP(RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, iq_file_dir);
    streamOn();
    SAMPLE_COMM_ISP_SET_BypassStreamRotation(S32Rotation);
    SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
    streamOff();
    usleep(300 * 1000);
  }
}

static RK_VOID RKMEDIA_ISP_Usage() {
  printf("\n\n/Usage:./rkmdia_audio index --aiq iqfiles_dir /\n");
  printf("\tindex and its function list below\n");
  printf("\t0:  normal --> hdr2\n");
  printf("\t1:  normal --> hdr2 --> hdr3\n");
  printf("\t2:  FBC rotating 0-->90-->270.\n");
}

int main(int argc, char *argv[]) {
  signal(SIGINT, sigterm_handler);

  RK_U32 u32Index;

  if (strcmp(argv[1], "-h") == 0) {
    RKMEDIA_ISP_Usage();
    return -1;
  }
  char *iq_file_dir = NULL;
  if (argc == 4) {
    if (strcmp(argv[2], "--aiq") == 0) {
      iq_file_dir = argv[3];
    }
    u32Index = atoi(argv[1]);
  } else {
    RKMEDIA_ISP_Usage();
    return -1;
  }

  RK_MPI_SYS_Init();
  switch (u32Index) {
  case 0:
    testNormalToHdr2(iq_file_dir);
    break;
  case 1:
    testNormalToHdr2ToHdr3(iq_file_dir);
    break;
  case 2:
    testFBCRotation(iq_file_dir);
    break;
  default:
    break;
  }

  return 0;
}
