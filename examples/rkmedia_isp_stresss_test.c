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
#include "librtsp/rtsp_demo.h"
#include "rkmedia_api.h"
#include "rkmedia_venc.h"

// static bool g_enable_rtsp = true;
rtsp_demo_handle g_rtsplive = NULL;
rtsp_session_handle g_session;
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
  rtsp_tx_video(g_session, RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb),
                RK_MPI_MB_GetTimestamp(mb));
  RK_MPI_MB_ReleaseBuffer(mb);
  rtsp_do_event(g_rtsplive);
}

RK_U32 g_width = 1920;
RK_U32 g_height = 1080;
char *g_video_node = "rkispp_scale0";
IMAGE_TYPE_E g_enPixFmt = IMAGE_TYPE_NV12;
RK_S32 g_S32Rotation = 0;

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
    venc_chn_attr.stVencAttr.enRotation = g_S32Rotation;

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
  while (quit) {
    count++;
    if (g_S32Rotation > 270) {
      g_S32Rotation = 0;
    } else {
      g_S32Rotation += 90;
    }
    printf("######### %d : count = %u #############.\n", g_S32Rotation, count);
    startISP(RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, iq_file_dir);
    SAMPLE_COMM_ISP_SET_BypassStreamRotation(g_S32Rotation);
    streamOn();
    usleep(50 * 100 * 1000);
    SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
    streamOff();
    usleep(100 * 1000);
  }
}

static RK_VOID testDefog(char *iq_file_dir) {
  quit = true;
  // g_width = 3840;
  // g_height = 2160;
  // g_video_node = "rkispp_m_bypass";
  // g_enPixFmt = IMAGE_TYPE_FBC0;
  RK_U32 count = 0;
  RK_U32 u32Mode;
  while (quit) {
    printf("######## disable count = %u ############.\n", count++);
    startISP(RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, iq_file_dir);
    u32Mode = 0;
    SAMPLE_COMM_ISP_SET_DefogEnable(u32Mode);
    streamOn();
    SAMPLE_COMM_ISP_Stop();
    streamOff();

    printf("######## enable manaul count = %u ############.\n", count);
    u32Mode = 1; // manual
    startISP(RK_AIQ_WORKING_MODE_NORMAL, RK_FALSE, iq_file_dir);
    SAMPLE_COMM_ISP_SET_DefogEnable(u32Mode);
    streamOn();
    for (int i = 0; i < 255; i++) {
      SAMPLE_COMM_ISP_SET_DefogStrength(u32Mode, i);
      usleep(100 * 1000);
    }

    printf("######## enable auto count = %u ############.\n", count);
    u32Mode = 2;
    SAMPLE_COMM_ISP_SET_DefogStrength(u32Mode, 0);
    usleep(5 * 1000 * 1000);
    SAMPLE_COMM_ISP_Stop();
    streamOff();
  }
}

static RK_VOID RKMEDIA_ISP_Usage() {
  printf("\n\n/Usage:./rkmdia_audio index --aiq iqfiles_dir /\n");
  printf("\tindex and its function list below\n");
  printf("\t0:  normal --> hdr2\n");
  printf("\t1:  normal --> hdr2 --> hdr3\n");
  printf("\t2:  FBC rotating 0-->90-->270.\n");
  printf("\t3:  Defog: close --> manual -> auto.\n");
  printf("\tyou can play rtsp://xxx.xxx.xxx.xxx/live/main_stream\n");
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
  // init rtsp
  g_rtsplive = create_rtsp_demo(554);
  g_session = rtsp_new_session(g_rtsplive, "/live/main_stream");
  rtsp_set_video(g_session, RTSP_CODEC_ID_VIDEO_H264, NULL, 0);
  rtsp_sync_video_ts(g_session, rtsp_get_reltime(), rtsp_get_ntptime());

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
  case 3:
    testDefog(iq_file_dir);
  default:
    break;
  }

  // release rtsp
  rtsp_del_demo(g_rtsplive);
  return 0;
}
