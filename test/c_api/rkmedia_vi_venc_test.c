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

int main() {

#ifdef RKAIQ
  SAMPLE_COMM_ISP_Init(RK_AIQ_WORKING_MODE_NORMAL);
  SAMPLE_COMM_ISP_Run();
#endif

  VENC_CHN_ATTR_S venc_chn_attr;
  venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_H264;
  venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
  venc_chn_attr.stVencAttr.u32PicWidth = 1920;
  venc_chn_attr.stVencAttr.u32PicHeight = 1080;
  venc_chn_attr.stVencAttr.u32VirWidth = 1920;
  venc_chn_attr.stVencAttr.u32VirHeight = 1080;
  venc_chn_attr.stVencAttr.u32Profile = 77;

  venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;

  venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 30;
  venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = 1920 * 1080 * 30 / 14;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 30;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;

  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.buffer_cnt = 4;
  vi_chn_attr.width = 1920;
  vi_chn_attr.height = 1080;
  vi_chn_attr.pix_fmt = IMAGE_TYPE_NV12;

  RK_MPI_SYS_Init();

  RK_MPI_VI_SetChnAttr(0, 1, &vi_chn_attr);
  RK_MPI_VI_EnableChn(0, 1);
  RK_MPI_VENC_CreateChn(0, &venc_chn_attr);

  MPP_CHN_S stEncChn;
  stEncChn.enModId = RK_ID_VENC;
  stEncChn.s32DevId = 0;
  stEncChn.s32ChnId = 0;
  RK_MPI_SYS_RegisterOutCb(&stEncChn, video_packet_cb);

  MPP_CHN_S stSrcChn;
  MPP_CHN_S stDestChn;

  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = 1;

  stDestChn.enModId = RK_ID_VENC;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = 0;

  RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

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

  while (!quit) {
    usleep(100);
  }

#ifdef RKAIQ
  SAMPLE_COMM_ISP_Stop(); // isp aiq stop before vi streamoff
#endif

  printf("%s exit!\n", __func__);
  RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
  RK_MPI_VI_DisableChn(0, 1);
  RK_MPI_VENC_DestroyChn(0);

  return 0;
}
