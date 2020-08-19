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

static FILE *g_save_file;
static FILE *g_save_file_sub0;
static FILE *g_save_file_sub1;
static bool quit = false;
static void sigterm_handler(int sig) {
  fprintf(stderr, "signal %d\n", sig);
  quit = true;
}

void video_packet_cb(MEDIA_BUFFER mb) {
#if 0
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
#endif

  if (g_save_file)
    fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), g_save_file);

  RK_MPI_MB_ReleaseBuffer(mb);
}

void video_packet_cb_sub0(MEDIA_BUFFER mb) {
  static int packet_cnt_sub0 = 0;
  if (g_save_file_sub0 && (packet_cnt_sub0++ < 150))
    fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), g_save_file_sub0);
  RK_MPI_MB_ReleaseBuffer(mb);
}

void video_packet_cb_sub1(MEDIA_BUFFER mb) {
  static int packet_cnt_sub1 = 0;
  if (g_save_file_sub0 && (packet_cnt_sub1++ < 150))
    fwrite(RK_MPI_MB_GetPtr(mb), 1, RK_MPI_MB_GetSize(mb), g_save_file_sub1);
  RK_MPI_MB_ReleaseBuffer(mb);
}

int StreamOn(int width, int height, IMAGE_TYPE_E img_type, const char* video_node, int sec) {
  static int stream_on_cnt = 0;
  int ret = 0;

  CODEC_TYPE_E codec_type;
  if (stream_on_cnt % 2)
    codec_type = RK_CODEC_TYPE_H264;
  else
    codec_type = RK_CODEC_TYPE_H265;

  printf("\n### %04d, wxh: %dx%d, CodeType: %s, ImgType: %s Start........\n\n",
    stream_on_cnt++, width, height, (codec_type == RK_CODEC_TYPE_H264)?"H264":"H265",
    (img_type == IMAGE_TYPE_FBC0)?"FBC0":"NV12");

  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = video_node;
  vi_chn_attr.u32BufCnt = 4;
  vi_chn_attr.u32Width = width;
  vi_chn_attr.u32Height = height;
  vi_chn_attr.enPixFmt = img_type;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(0, 0, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, 0);
  if (ret) {
    printf("Create Vi failed! ret=%d\n", ret);
    return -1;
  }

  if (img_type == IMAGE_TYPE_FBC0) {
    printf("TEST: INFO: FBC0 use rkispp_scale0 for luma caculation!\n");
    vi_chn_attr.pcVideoNode = "rkispp_scale0";
    vi_chn_attr.u32BufCnt = 4;
    vi_chn_attr.u32Width = 1280;
    vi_chn_attr.u32Height = 720;
    vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
    vi_chn_attr.enWorkMode = VI_WORK_MODE_LUMA_ONLY;
    ret = RK_MPI_VI_SetChnAttr(0, 3, &vi_chn_attr);
    ret |= RK_MPI_VI_EnableChn(0, 3);
    if (ret) {
      printf("Create Vi[3] for luma failed! ret=%d\n", ret);
      return -1;
    }
  }

  VENC_CHN_ATTR_S venc_chn_attr;
  venc_chn_attr.stVencAttr.enType = codec_type;
  venc_chn_attr.stVencAttr.imageType = img_type;
  venc_chn_attr.stVencAttr.u32PicWidth = width;
  venc_chn_attr.stVencAttr.u32PicHeight = height;
  venc_chn_attr.stVencAttr.u32VirWidth = width;
  venc_chn_attr.stVencAttr.u32VirHeight = height;
  venc_chn_attr.stVencAttr.u32Profile = 77;
  venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
  venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 75;
  venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = width * height * 30 / 14;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 25;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
  ret = RK_MPI_VENC_CreateChn(0, &venc_chn_attr);
  if (ret) {
    printf("Create avc failed! ret=%d\n", ret);
    return -1;
  }

  memset(&venc_chn_attr, 0, sizeof(venc_chn_attr));
  venc_chn_attr.stVencAttr.enType = RK_CODEC_TYPE_JPEG;
  venc_chn_attr.stVencAttr.imageType = img_type;
  venc_chn_attr.stVencAttr.u32PicWidth = width;
  venc_chn_attr.stVencAttr.u32PicHeight = height;
  venc_chn_attr.stVencAttr.u32VirWidth = width;
  venc_chn_attr.stVencAttr.u32VirHeight = height;
  // venc_chn_attr.stVencAttr.enRotation = VENC_ROTATION_90;
  ret = RK_MPI_VENC_CreateChn(3, &venc_chn_attr);
  if (ret) {
    printf("Create jpeg failed! ret=%d\n", ret);
    return -1;
  }

  // The encoder defaults to continuously receiving frames from the previous
  // stage. Before performing the bind operation, set s32RecvPicNum to 0 to
  // make the encoding enter the pause state.
  VENC_RECV_PIC_PARAM_S stRecvParam;
  stRecvParam.s32RecvPicNum = 0;
  RK_MPI_VENC_StartRecvFrame(3, &stRecvParam);

  MPP_CHN_S stEncChn;
  stEncChn.enModId = RK_ID_VENC;
  stEncChn.s32DevId = 0;
  stEncChn.s32ChnId = 0;
  RK_MPI_SYS_RegisterOutCb(&stEncChn, video_packet_cb);

  MPP_CHN_S stSrcChn;
  stSrcChn.enModId = RK_ID_VI;
  stSrcChn.s32DevId = 0;
  stSrcChn.s32ChnId = 0;
  MPP_CHN_S stDestChn;
  stDestChn.enModId = RK_ID_VENC;
  stDestChn.s32DevId = 0;
  stDestChn.s32ChnId = 0;
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("Create RK_MPI_SYS_Bind0 failed! ret=%d\n", ret);
    return -1;
  }

  stDestChn.s32ChnId = 3;
  ret = RK_MPI_SYS_Bind(&stSrcChn, &stDestChn);
  if (ret) {
    printf("Create RK_MPI_SYS_Bind1 failed! ret=%d\n", ret);
    return -1;
  }

  printf("%s initial finish\n", __func__);
  signal(SIGINT, sigterm_handler);

  int loop_cnt = sec * 2;
  while (!quit) {
    usleep(500000);
    loop_cnt--;
    if (loop_cnt < 0)
      break;
  }

  printf("%s exit!\n", __func__);
  stDestChn.s32ChnId = 0;
  RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);
  stDestChn.s32ChnId = 3;
  RK_MPI_SYS_UnBind(&stSrcChn, &stDestChn);

  RK_MPI_VI_DisableChn(0, 0);
  RK_MPI_VENC_DestroyChn(0); // avc/hevc
  RK_MPI_VENC_DestroyChn(3); // jpeg
  if (img_type == IMAGE_TYPE_FBC0) {
      printf("TEST: INFO: Disable luma caculation vi[3]!\n");
      RK_MPI_VI_DisableChn(0, 3);
  }
  printf("\n-------------------END----------------------------\n");

  return 0;
}

int SubStreamOn(int width, int height, const char* video_node, int vi_chn, int venc_chn) {
  static int sub_stream_cnt = 0;
  int ret = 0;

  printf("*** SubStreamOn[%d]: VideoNode:%s, wxh:%dx%d, vi:%d, venc:%d START....\n",
    sub_stream_cnt, video_node, width, height, vi_chn, venc_chn);

  CODEC_TYPE_E codec_type = RK_CODEC_TYPE_H264;
  VI_CHN_ATTR_S vi_chn_attr;
  vi_chn_attr.pcVideoNode = video_node;
  vi_chn_attr.u32BufCnt = 4;
  vi_chn_attr.u32Width = width;
  vi_chn_attr.u32Height = height;
  vi_chn_attr.enPixFmt = IMAGE_TYPE_NV12;
  vi_chn_attr.enWorkMode = VI_WORK_MODE_NORMAL;
  ret = RK_MPI_VI_SetChnAttr(0, vi_chn, &vi_chn_attr);
  ret |= RK_MPI_VI_EnableChn(0, vi_chn);
  if (ret) {
    printf("Create Vi failed! ret=%d\n", ret);
    return -1;
  }

  VENC_CHN_ATTR_S venc_chn_attr;
  venc_chn_attr.stVencAttr.enType = codec_type;
  venc_chn_attr.stVencAttr.imageType = IMAGE_TYPE_NV12;
  venc_chn_attr.stVencAttr.u32PicWidth = width;
  venc_chn_attr.stVencAttr.u32PicHeight = height;
  venc_chn_attr.stVencAttr.u32VirWidth = width;
  venc_chn_attr.stVencAttr.u32VirHeight = height;
  venc_chn_attr.stVencAttr.u32Profile = 77;
  venc_chn_attr.stRcAttr.enRcMode = VENC_RC_MODE_H264CBR;
  venc_chn_attr.stRcAttr.stH264Cbr.u32Gop = 75;
  venc_chn_attr.stRcAttr.stH264Cbr.u32BitRate = width * height * 30 / 14;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.fr32DstFrameRateNum = 25;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateDen = 0;
  venc_chn_attr.stRcAttr.stH264Cbr.u32SrcFrameRateNum = 30;
  ret = RK_MPI_VENC_CreateChn(venc_chn, &venc_chn_attr);
  if (ret) {
    printf("Create avc failed! ret=%d\n", ret);
    return -1;
  }

  MPP_CHN_S stEncChn;
  stEncChn.enModId = RK_ID_VENC;
  stEncChn.s32DevId = 0;
  stEncChn.s32ChnId = venc_chn;
  if (sub_stream_cnt == 0)
    RK_MPI_SYS_RegisterOutCb(&stEncChn, video_packet_cb_sub0);
  else
    RK_MPI_SYS_RegisterOutCb(&stEncChn, video_packet_cb_sub1);

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
    printf("Create RK_MPI_SYS_Bind0 failed! ret=%d\n", ret);
    return -1;
  }

  sub_stream_cnt++;
  printf("*** SubStreamOn: END....\n");

  return 0;
}

int SubStreamOff(int vi_chn, int venc_chn) {
  int ret = 0;

  printf("*** SubStreamOff: vi:%d, venc:%d START....\n", vi_chn, venc_chn);

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
    printf("ERROR: unbind vi[%d] -> venc[%d] failed!\n", vi_chn, venc_chn);
    return -1;
  }

  RK_MPI_VI_DisableChn(0, vi_chn);
  RK_MPI_VENC_DestroyChn(venc_chn);
  printf("*** SubStreamOff: END....\n");

  return 0;
}

static char optstr[] = "?:c:s:w:h:";

static void print_usage(char *name) {
  printf("#Function description:\n");
  printf("There are three streams in total:MainStream/SubStream0/SubStream.\n"
         "The sub-stream remains unchanged, and the resolution of the main \n"
         "stream is constantly switched.\n");
  printf("  SubStream0: rkispp_scale1: 720x480 NV12 -> /userdata/sub0.h264(150 frames)\n");
  printf("  SubStream1: rkispp_scale2: 1280x720 NV12 -> /userdata/sub1.h264(150 frames)\n");
  printf("  MainStream: case1: rkispp_m_bypass: widthxheight FBC0\n");
  printf("                     rkispp_scale0: 1280x720 for luma caculation.\n");
  printf("  MainStream: case2: rkispp_scale0: 1280x720 NV12\n");
  printf("#Usage Example: \n");
  printf("  %s [-c 20] [-s 5] [-w 3840] [-h 2160]\n", name);
  printf("  @[-c] Main stream switching times. defalut:20\n");
  printf("  @[-s] The duration of the main stream. default:5s\n");
  printf("  @[-w] img width for rkispp_m_bypass. default: 3840\n");
  printf("  @[-h] img height for rkispp_m_bypass. default: 2160\n");
}

int main(int argc, char *argv[]) {
  int loop_cnt = 20;
  int loop_seconds = 5; // 5s
  int width = 3840;
  int height = 2160;

  int c = 0;
  opterr = 1;
  while ((c = getopt(argc, argv, optstr)) != -1) {
    switch (c) {
    case 'c':
      loop_cnt = atoi(optarg);
      printf("#IN ARGS: loop_cnt: %d\n", loop_cnt);
      break;
    case 's':
      loop_seconds = atoi(optarg);
      printf("#IN ARGS: loop_seconds: %d\n", loop_seconds);
      break;
    case 'w':
      width = atoi(optarg);;
      printf("#IN ARGS: bypass width: %d\n", width);
      break;
    case 'h':
      height = atoi(optarg);;
      printf("#IN ARGS: bypass height: %d\n", height);
      break;
    case '?':
    default:
      print_usage(argv[0]);
      exit(0);
    }
  }

  printf(">>>>>>>>>>>>>>> Test START <<<<<<<<<<<<<<<<<<<<<<\n");
  printf("-->LoopCnt:%d\n", loop_cnt);
  printf("-->LoopSeconds:%d\n", loop_seconds);
  printf("-->BypassWidth:%d\n", width);
  printf("-->BypassHeight:%d\n", height);

  RK_MPI_SYS_Init();

  g_save_file_sub0 = fopen("/data/sub0.h264", "w");
  if (SubStreamOn(720, 480, "rkispp_scale1", 1, 1)) {
    printf("ERROR: SubStreamOn failed!\n");
    return -1;
  }

  g_save_file_sub1 = fopen("/data/sub1.h264", "w");
  if (SubStreamOn(1280, 720, "rkispp_scale2", 2, 2)) {
    printf("ERROR: SubStreamOn failed!\n");
    return -1;
  }

  for (int i = 0; !quit && (i < loop_cnt); i++ ) {
    g_save_file = fopen("/data/fbc0.h264", "w");
    if (StreamOn(width, height, IMAGE_TYPE_FBC0, "rkispp_m_bypass", loop_seconds)) {
      printf("ERROR: StreamOn 2k failed!\n");
      break;
    }
    fclose(g_save_file);
    g_save_file = NULL;

    g_save_file = fopen("/data/720p.h264", "w");
    if (StreamOn(1280, 720, IMAGE_TYPE_NV12, "rkispp_scale0", loop_seconds)) {
      printf("ERROR: StreamOn 720p failed!\n");
      break;
    }
    fclose(g_save_file);
    g_save_file = NULL;
  }

  if (g_save_file)
    fclose(g_save_file);

  SubStreamOff(1, 1);
  SubStreamOff(2, 2);
  fclose(g_save_file_sub0);
  fclose(g_save_file_sub1);
  printf(">>>>>>>>>>>>>>> Test END <<<<<<<<<<<<<<<<<<<<<<\n");
  return 0;
}
