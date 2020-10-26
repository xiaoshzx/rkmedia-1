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
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rkmedia_api.h"

static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

#define AAC_PROFILE_LOW 1
static void GetAdtsHeader(RK_U8 *pu8AdtsHdr, RK_S32 u32SmpRate, RK_U8 u8Channel,
                          RK_U32 u32DataLen) {
  RK_U8 u8FreqIdx = 0;
  switch (u32SmpRate) {
  case 96000:
    u8FreqIdx = 0;
    break;
  case 88200:
    u8FreqIdx = 1;
    break;
  case 64000:
    u8FreqIdx = 2;
    break;
  case 48000:
    u8FreqIdx = 3;
    break;
  case 44100:
    u8FreqIdx = 4;
    break;
  case 32000:
    u8FreqIdx = 5;
    break;
  case 24000:
    u8FreqIdx = 6;
    break;
  case 22050:
    u8FreqIdx = 7;
    break;
  case 16000:
    u8FreqIdx = 8;
    break;
  case 12000:
    u8FreqIdx = 9;
    break;
  case 11025:
    u8FreqIdx = 10;
    break;
  case 8000:
    u8FreqIdx = 11;
    break;
  case 7350:
    u8FreqIdx = 12;
    break;
  default:
    u8FreqIdx = 4;
    break;
  }
  RK_U32 u32PacketLen = u32DataLen + 7;
  pu8AdtsHdr[0] = 0xFF;
  pu8AdtsHdr[1] = 0xF1;
  pu8AdtsHdr[2] =
      ((AAC_PROFILE_LOW) << 6) + (u8FreqIdx << 2) + (u8Channel >> 2);
  pu8AdtsHdr[3] = (((u8Channel & 3) << 6) + (u32PacketLen >> 11));
  pu8AdtsHdr[4] = ((u32PacketLen & 0x7FF) >> 3);
  pu8AdtsHdr[5] = (((u32PacketLen & 7) << 5) + 0x1F);
  pu8AdtsHdr[6] = 0xFC;
}

static void *GetMediaBuffer(void *path) {
  char *save_path = (char *)path;
  printf("#Start %s thread, arg:%s\n", __func__, save_path);
  FILE *save_file = fopen(save_path, "w");
  if (!save_file)
    printf("ERROR: Open %s failed!\n", save_path);

  RK_U8 aac_header[7];
  MEDIA_BUFFER mb = NULL;
  int cnt = 0;
  while (!quit) {
    mb = RK_MPI_SYS_GetMediaBuffer(RK_ID_AENC, 0, -1);
    if (!mb) {
      printf("RK_MPI_SYS_GetMediaBuffer get null buffer!\n");
      break;
    }

    printf("#%d Get Frame:ptr:%p, size:%zu, mode:%d, channel:%d, "
           "timestamp:%lld\n",
           cnt++, RK_MPI_MB_GetPtr(mb), RK_MPI_MB_GetSize(mb),
           RK_MPI_MB_GetModeID(mb), RK_MPI_MB_GetChannelID(mb),
           RK_MPI_MB_GetTimestamp(mb));

    if (save_file) {
      GetAdtsHeader(aac_header, 48000, 2, RK_MPI_MB_GetSize(mb));
      fwrite(aac_header, 1, 7, save_file);
      fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), save_file);
    }
    RK_MPI_MB_ReleaseBuffer(mb);
  }

  if (save_file)
    fclose(save_file);

  return NULL;
}

static RK_CHAR optstr[] = "?:d:c:r:o:";
static void print_usage(const RK_CHAR *name) {
  printf("usage example:\n");
  printf("\t%s [-d default] [-r 16000] [-c 2] -o /tmp/aenc.aac\n", name);
  printf("\t-d: device name, Default:\"default\"\n");
  printf("\t-r: sample rate, Default:16000\n");
  printf("\t-c: channel count, Default:2\n");
  printf("\t-o: output path, Default:\"/tmp/aenc.aac\"\n");
  printf("Notice: fmt always be s16_le\n");
}

int main(int argc, char *argv[]) {
  RK_U32 u32SampleRate = 16000;
  RK_U32 u32ChnCnt = 2;
  RK_U32 u32FrameCnt = 1024;
  // default:CARD=rockchiprk809co
  RK_CHAR *pDeviceName = "default";
  RK_CHAR *pOutPath = "/tmp/aenc.aac";
  int ret = 0;
  int c;

  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
    case 'd':
      pDeviceName = optarg;
      break;
    case 'r':
      u32SampleRate = atoi(optarg);
      break;
    case 'c':
      u32ChnCnt = atoi(optarg);
      break;
    case 'o':
      pOutPath = optarg;
      break;
    case '?':
    default:
      print_usage(argv[0]);
      return 0;
    }
  }

  printf("#Device: %s\n", pDeviceName);
  printf("#SampleRate: %d\n", u32SampleRate);
  printf("#Channel Count: %d\n", u32ChnCnt);
  printf("#Frame Count: %d\n", u32FrameCnt);
  printf("#Output Path: %s\n", pOutPath);

  RK_MPI_SYS_Init();

  MPP_CHN_S mpp_chn_ai, mpp_chn_aenc;
  mpp_chn_ai.enModId = RK_ID_AI;
  mpp_chn_ai.s32ChnId = 0;
  mpp_chn_aenc.enModId = RK_ID_AENC;
  mpp_chn_aenc.s32ChnId = 0;

  // 1. create AI
  AI_CHN_ATTR_S ai_attr;
  ai_attr.pcAudioNode = pDeviceName;
  ai_attr.enSampleFormat = RK_SAMPLE_FMT_S16;
  ai_attr.u32NbSamples = u32FrameCnt;
  ai_attr.u32SampleRate = u32SampleRate;
  ai_attr.u32Channels = u32ChnCnt;
  ai_attr.enAiLayout = AI_LAYOUT_NORMAL;
  ret = RK_MPI_AI_SetChnAttr(mpp_chn_ai.s32ChnId, &ai_attr);
  ret |= RK_MPI_AI_EnableChn(mpp_chn_ai.s32ChnId);
  if (ret) {
    printf("Create AI[0] failed! ret=%d\n", ret);
    return -1;
  }

  // 2. create AENC
  AENC_CHN_ATTR_S aenc_attr;
  aenc_attr.enCodecType = RK_CODEC_TYPE_AAC;
  aenc_attr.u32Bitrate = u32SampleRate;
  aenc_attr.u32Quality = 1;
  aenc_attr.stAencAAC.u32Channels = u32ChnCnt;
  aenc_attr.stAencAAC.u32SampleRate = u32SampleRate;
  ret = RK_MPI_AENC_CreateChn(mpp_chn_aenc.s32ChnId, &aenc_attr);
  if (ret) {
    printf("Create AENC[0] failed! ret=%d\n", ret);
    return -1;
  }

  pthread_t read_thread;
  pthread_create(&read_thread, NULL, GetMediaBuffer, pOutPath);

  // 3. bind AI-AENC
  ret = RK_MPI_SYS_Bind(&mpp_chn_ai, &mpp_chn_aenc);
  if (ret) {
    printf("Bind AI[0] to AENC[0] failed! ret=%d\n", ret);
    return -1;
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);
  while (!quit) {
    usleep(100);
  }

  RK_MPI_SYS_UnBind(&mpp_chn_ai, &mpp_chn_aenc);
  RK_MPI_AI_DisableChn(mpp_chn_ai.s32ChnId);
  RK_MPI_AENC_DestroyChn(mpp_chn_aenc.s32ChnId);

  return 0;
}
