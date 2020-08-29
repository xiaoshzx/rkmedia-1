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

RK_S32 SAMPLE_COMM_ISP_Init(rk_aiq_working_mode_t WDRMode) {
  char *iq_file_dir = "iqfiles/";
  setlinebuf(stdout);

  rk_aiq_sys_ctx_t *aiq_ctx;
  rk_aiq_static_info_t aiq_static_info;
  rk_aiq_uapi_sysctl_enumStaticMetas(0, &aiq_static_info);

  printf("sensor_name is %s, iqfiles is %s\n",
         aiq_static_info.sensor_info.sensor_name, iq_file_dir);

  aiq_ctx = rk_aiq_uapi_sysctl_init(aiq_static_info.sensor_info.sensor_name,
                                    iq_file_dir, NULL, NULL);

  if (rk_aiq_uapi_sysctl_prepare(aiq_ctx, 0, 0, WDRMode)) {
    printf("rkaiq engine prepare failed !\n");
    return -1;
  }
  printf("rk_aiq_uapi_sysctl_init/prepare succeed\n");
  g_aiq_ctx = aiq_ctx;
  return 0;
}

RK_VOID SAMPLE_COMM_ISP_FEC_Set(RK_BOOL open) {
  if (!g_aiq_ctx)
    return;

  printf("%s, mode is %d\n", __func__, open);
  rk_aiq_uapi_sysctl_setModuleCtl(g_aiq_ctx, RK_MODULE_FEC, open);
}

RK_VOID SAMPLE_COMM_ISP_Stop(void) {
  if (!g_aiq_ctx)
    return;

  printf("rk_aiq_uapi_sysctl_stop\n");
  rk_aiq_uapi_sysctl_stop(g_aiq_ctx);
  rk_aiq_uapi_sysctl_deinit(g_aiq_ctx);
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
#endif
