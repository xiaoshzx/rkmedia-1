#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>

#include <string>

#include "buffer.h"
#include "encoder.h"
#include "flow.h"
#include "key_string.h"
#include "media_config.h"
#include "media_type.h"
#include "message.h"
#include "stream.h"
#include "utils.h"
#include "image.h"
#include "rkmedia_api.h"

/********************************************************************
 * SYS Ctrl api
 ********************************************************************/

enum {
  CHN_STATUS_CLOSED,
  CHN_STATUS_OPEN,
  CHN_STATUS_BIND,
  //ToDo...
};

typedef struct _ChannelTable {
  int status; //CHN_STATUS_OPEN/CHN_STATUS_CLOSED/....
  std::shared_ptr<easymedia::Flow> rkmedia_flow;
} ChannelTable;

ChannelTable g_vi_chns[VI_MAX_DEV_NUM];
std::mutex g_vi_mtx;

ChannelTable g_venc_chns[VENC_MAX_CHN_NUM];
std::mutex g_venc_mtx;

RK_S32  RK_MPI_SYS_Bind(const MPP_CHN_S *pstSrcChn, const MPP_CHN_S *pstDestChn) {
  std::shared_ptr<easymedia::Flow> src;
  std::shared_ptr<easymedia::Flow> sink;
  ChannelTable *src_chn = NULL;
  ChannelTable *dst_chn = NULL;

  switch (pstSrcChn->enModId) {
    case RK_ID_VI:
      if (g_vi_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_OPEN)
        return RK_ERR_SYS_NOTREADY;
      src = g_vi_chns[pstSrcChn->s32ChnId].rkmedia_flow;
      src_chn = &g_vi_chns[pstSrcChn->s32ChnId];
      break;
    case RK_ID_VENC:
      if (g_venc_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_OPEN)
        return RK_ERR_SYS_NOTREADY;
      src = g_venc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
      src_chn = &g_venc_chns[pstSrcChn->s32ChnId];
      break;
    default:
      return RK_ERR_SYS_NOT_SUPPORT;
  }

  switch (pstDestChn->enModId) {
    case RK_ID_VI:
      if (g_vi_chns[pstDestChn->s32ChnId].status != CHN_STATUS_OPEN)
        return RK_ERR_SYS_NOTREADY;
      sink = g_vi_chns[pstDestChn->s32ChnId].rkmedia_flow;
      dst_chn = &g_vi_chns[pstDestChn->s32ChnId];
      break;
    case RK_ID_VENC:
      if (g_venc_chns[pstDestChn->s32ChnId].status != CHN_STATUS_OPEN)
        return RK_ERR_SYS_NOTREADY;
      sink = g_venc_chns[pstDestChn->s32ChnId].rkmedia_flow;
      dst_chn = &g_venc_chns[pstDestChn->s32ChnId];
      break;
    default:
      return RK_ERR_SYS_NOT_SUPPORT;
  }

  if (!src) {
    LOG("ERROR: %s Src Chn[%d] is not ready!\n", __func__, pstSrcChn->s32ChnId);
    return RK_ERR_SYS_NOTREADY;
  }

  if (!sink) {
    LOG("ERROR: %s Dst Chn[%d] is not ready!\n", __func__, pstDestChn->s32ChnId);
    return RK_ERR_SYS_NOTREADY;
  }

  //Rkmedia flow bind
  src->AddDownFlow(sink, 0, 0);

  // change status frome OPEN to BIND.
  src_chn->status = CHN_STATUS_BIND;
  dst_chn->status = CHN_STATUS_BIND;

  return 0;
}

//RK_S32 RK_MPI_SYS_UnBind(const MPP_CHN_S *pstSrcChn, const MPP_CHN_S *pstDestChn)

/********************************************************************
 * Vi api
 ********************************************************************/
RK_S32 RK_MPI_VI_EnableChn(VI_PIPE ViPipe, VI_CHN ViChn) {
  if ((ViPipe < 0) || (ViChn < 0) || (ViChn > VI_MAX_CHN_NUM))
    return -1;

  g_vi_mtx.lock();
  if (g_vi_chns[ViChn].status == CHN_STATUS_OPEN) {
    g_vi_mtx.unlock();
    return -1;
  }

  //Get size with ViChn, For Example:
  //  CH0: 2688x1520, NV12/FBC0
  //  CH1: 1920x1080, NV12
  //  CH2: 1280x720,  NV12
  //  CH3: 640x480,   NV12

  //Reading yuv from camera
  std::string flow_name = "source_stream";
  std::string flow_param;
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "v4l2_capture_stream");
  PARAM_STRING_APPEND(flow_param, KEK_THREAD_SYNC_MODEL, KEY_SYNC);
  PARAM_STRING_APPEND(flow_param, KEK_INPUT_MODEL, KEY_DROPFRONT);
  PARAM_STRING_APPEND_TO(flow_param, KEY_INPUT_CACHE_NUM, 5);
  std::string stream_param;
  PARAM_STRING_APPEND_TO(stream_param, KEY_USE_LIBV4L2, 1);
  PARAM_STRING_APPEND(stream_param, KEY_DEVICE, "/dev/video14");
  PARAM_STRING_APPEND(stream_param, KEY_V4L2_CAP_TYPE, KEY_V4L2_C_TYPE(VIDEO_CAPTURE));
  PARAM_STRING_APPEND(stream_param, KEY_V4L2_MEM_TYPE, KEY_V4L2_M_TYPE(MEMORY_DMABUF));
  PARAM_STRING_APPEND_TO(stream_param, KEY_FRAMES, 4); // if not set, default is 2
  PARAM_STRING_APPEND(stream_param, KEY_OUTPUTDATATYPE, IMAGE_NV12);
  PARAM_STRING_APPEND_TO(stream_param, KEY_BUFFER_WIDTH, 1920);
  PARAM_STRING_APPEND_TO(stream_param, KEY_BUFFER_HEIGHT, 1080);
  flow_param = easymedia::JoinFlowParam(flow_param, 1, stream_param);

  g_vi_chns[ViChn].rkmedia_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  g_vi_chns[ViChn].status = CHN_STATUS_OPEN;

  g_vi_mtx.unlock();
  return 0;
}

RK_S32 RK_MPI_VI_DisableChn(VI_PIPE ViPipe, VI_CHN ViChn) {
  if ((ViPipe < 0) || (ViChn < 0) || (ViChn > VI_MAX_CHN_NUM))
    return -1;

  g_vi_mtx.lock();
  if (g_vi_chns[ViChn].status == CHN_STATUS_BIND) {
    g_vi_mtx.unlock();
    return -1;
  }

  g_vi_chns[ViChn].rkmedia_flow.reset();
  g_vi_chns[ViChn].status = CHN_STATUS_CLOSED;
  g_vi_mtx.unlock();
  return 0;
}


/********************************************************************
 * Venc api
 ********************************************************************/
RK_S32 RK_MPI_VENC_CreateChn(VENC_CHN VeChn, VENC_CHN_ATTR_S *stVencChnAttr) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID; 

  g_venc_mtx.lock();
  if (g_venc_chns[VeChn].status == CHN_STATUS_OPEN) {
    g_venc_mtx.unlock();
    return RK_ERR_VENC_EXIST;
  }

  //ToDo...
  //std::string flow_param = ConvertEncCfgToString(stVencChnAttr);
  UNUSED(stVencChnAttr);

  std::string flow_name = "video_enc";
  std::string flow_param;
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "rkmpp");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, IMAGE_NV12);
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, VIDEO_H264);

  int bps = 1920 * 1080 * 30 / 14;
  std::string enc_param;
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_WIDTH, 1920);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_HEIGHT, 1080);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_VIR_WIDTH, 1920);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_VIR_HEIGHT, 1080);
  PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE, bps);
  PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE_MAX, bps * 17 / 16);
  PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE_MIN, bps / 16);
  PARAM_STRING_APPEND(enc_param, KEY_FPS, "30/0");
  PARAM_STRING_APPEND(enc_param, KEY_FPS_IN, "30/0");
  PARAM_STRING_APPEND_TO(enc_param, KEY_FULL_RANGE, 0);

  flow_param = easymedia::JoinFlowParam(flow_param, 1, enc_param);
  g_venc_chns[VeChn].rkmedia_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      "video_enc", flow_param.c_str());

  g_venc_chns[VeChn].status = CHN_STATUS_OPEN;

  g_venc_mtx.unlock();
  return 0;
}

RK_S32 RK_MPI_VENC_DestroyChn(VENC_CHN VeChn) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID; //对应HI_ERR_VENC_INVALID_CHNID 

  g_venc_mtx.lock();
  if (g_venc_chns[VeChn].status == CHN_STATUS_BIND) {
    g_venc_mtx.unlock();
    return RK_ERR_VENC_BUSY;
  }

  g_venc_chns[VeChn].rkmedia_flow.reset();
  g_venc_chns[VeChn].status = CHN_STATUS_CLOSED;
  g_venc_mtx.unlock();

  return 0;
}

