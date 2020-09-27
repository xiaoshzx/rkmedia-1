// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef RKAIQ

#include "sample_common.h"
#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

static rk_aiq_sys_ctx_t *g_aiq_ctx;

RK_S32 SAMPLE_COMM_ISP_Init(rk_aiq_working_mode_t WDRMode, RK_BOOL bFECEnable,
                            const char *iq_file_dir) {
  // char *iq_file_dir = "iqfiles/";
  setlinebuf(stdout);
  if (iq_file_dir == NULL) {
    printf("SAMPLE_COMM_ISP_Init : not start.\n");
    g_aiq_ctx = NULL;
    return 0;
  }

  rk_aiq_sys_ctx_t *aiq_ctx;
  rk_aiq_static_info_t aiq_static_info;
  rk_aiq_uapi_sysctl_enumStaticMetas(0, &aiq_static_info);

  printf("sensor_name is %s, iqfiles is %s\n",
         aiq_static_info.sensor_info.sensor_name, iq_file_dir);

  aiq_ctx = rk_aiq_uapi_sysctl_init(aiq_static_info.sensor_info.sensor_name,
                                    iq_file_dir, NULL, NULL);

  printf("rk_aiq_uapi_sysctl_init bFECEnable %d\n", bFECEnable);

  rk_aiq_uapi_setFecEn(aiq_ctx, bFECEnable);

  printf("rk_aiq_uapi_setFecEn\n");
  if (rk_aiq_uapi_sysctl_prepare(aiq_ctx, 0, 0, WDRMode)) {
    printf("rkaiq engine prepare failed !\n");
    return -1;
  }
  printf("rk_aiq_uapi_sysctl_init/prepare succeed\n");
  g_aiq_ctx = aiq_ctx;
  return 0;
}

RK_VOID SAMPLE_COMM_ISP_Stop(void) {
  if (!g_aiq_ctx)
    return;

  printf("rk_aiq_uapi_sysctl_stop enter\n");
  rk_aiq_uapi_sysctl_stop(g_aiq_ctx);
  printf("rk_aiq_uapi_sysctl_deinit enter\n");
  rk_aiq_uapi_sysctl_deinit(g_aiq_ctx);
  printf("rk_aiq_uapi_sysctl_deinit exit\n");
  g_aiq_ctx = NULL;
}

RK_S32 SAMPLE_COMM_ISP_Run(void) {
  if (!g_aiq_ctx)
    return -1;

  if (rk_aiq_uapi_sysctl_start(g_aiq_ctx)) {
    printf("rk_aiq_uapi_sysctl_start  failed\n");
    return -1;
  }
  printf("rk_aiq_uapi_sysctl_start succeed\n");
  return 0;
}

RK_VOID SAMPLE_COMM_ISP_DumpExpInfo(rk_aiq_working_mode_t WDRMode) {
  char aStr[128] = {'\0'};
  Uapi_ExpQueryInfo_t stExpInfo;
  rk_aiq_wb_cct_t stCCT;

  rk_aiq_user_api_ae_queryExpResInfo(g_aiq_ctx, &stExpInfo);
  rk_aiq_user_api_awb_GetCCT(g_aiq_ctx, &stCCT);

  if (WDRMode == RK_AIQ_WORKING_MODE_NORMAL) {
    sprintf(aStr, "M:%.0f-%.1f LM:%.1f CT:%.1f",
            stExpInfo.CurExpInfo.LinearExp.exp_real_params.integration_time *
                1000 * 1000,
            stExpInfo.CurExpInfo.LinearExp.exp_real_params.analog_gain,
            stExpInfo.MeanLuma, stCCT.CCT);
  } else {
    sprintf(aStr, "S:%.0f-%.1f M:%.0f-%.1f L:%.0f-%.1f SLM:%.1f MLM:%.1f "
                  "LLM:%.1f CT:%.1f",
            stExpInfo.CurExpInfo.HdrExp[0].exp_real_params.integration_time *
                1000 * 1000,
            stExpInfo.CurExpInfo.HdrExp[0].exp_real_params.analog_gain,
            stExpInfo.CurExpInfo.HdrExp[1].exp_real_params.integration_time *
                1000 * 1000,
            stExpInfo.CurExpInfo.HdrExp[1].exp_real_params.analog_gain,
            stExpInfo.CurExpInfo.HdrExp[2].exp_real_params.integration_time *
                1000 * 1000,
            stExpInfo.CurExpInfo.HdrExp[2].exp_real_params.analog_gain,
            stExpInfo.HdrMeanLuma[0], stExpInfo.HdrMeanLuma[1],
            stExpInfo.HdrMeanLuma[2], stCCT.CCT);
  }
  printf("isp exp dump: %s\n", aStr);
}

RK_VOID SAMPLE_COMM_ISP_SetFrameRate(RK_U32 uFps) {
  if (!g_aiq_ctx)
    return;

  printf("SAMPLE_COMM_ISP_SetFrameRate start %d\n", uFps);

  frameRateInfo_t info;
  info.mode = OP_MANUAL;
  info.fps = uFps;
  rk_aiq_uapi_setFrameRate(g_aiq_ctx, info);

  printf("SAMPLE_COMM_ISP_SetFrameRate %d\n", uFps);
}

RK_VOID SAMPLE_COMM_ISP_SetLDCHLevel(RK_U32 level) {
  if (!g_aiq_ctx)
    return;
  rk_aiq_uapi_setLdchEn(g_aiq_ctx, level > 0);
  if (level > 0 && level <= 255)
    rk_aiq_uapi_setLdchCorrectLevel(g_aiq_ctx, level);
}

/*only support switch between HDR and normal*/
RK_VOID SAMPLE_COMM_ISP_SetWDRModeDyn(rk_aiq_working_mode_t WDRMode) {
  rk_aiq_uapi_sysctl_swWorkingModeDyn(g_aiq_ctx, WDRMode);
}

#endif
