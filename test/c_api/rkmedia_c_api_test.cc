// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include "rkmedia_api.h"

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

int main() {

  RK_MPI_VI_EnableChn(0, 0);
  RK_MPI_VENC_CreateChn(0, NULL);

  MPP_CHN_S stSrcChn;
  MPP_CHN_S stDestChn;

  stSrcChn.enModId   = RK_ID_VI;
  stSrcChn.s32DevId  = 0;
  stSrcChn.s32ChnId  = 0;

  stDestChn.enModId  = RK_ID_VENC;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = 0;

  RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  while(!quit) {
    usleep(100);
  }

  return 0;
}

