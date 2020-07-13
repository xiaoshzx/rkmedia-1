// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "encoder.h"
#include "image.h"
#include "key_string.h"
#include "media_config.h"
#include "media_type.h"
#include "message.h"
#include "stream.h"
#include "utils.h"

#include "rkmedia_api.h"
#include "rkmedia_buffer.h"
#include "rkmedia_buffer_impl.h"
#include "rkmedia_utils.h"

typedef enum rkCHN_STATUS {
  CHN_STATUS_CLOSED,
  CHN_STATUS_READY, // params is confirmed.
  CHN_STATUS_OPEN,
  CHN_STATUS_BIND,
  // ToDo...
} CHN_STATUS;

typedef struct _RkmediaVencAttr { VENC_ATTR_S attr; } RkmediaVencAttr;

typedef struct _RkmediaVIAttr {
  char *path;
  VI_CHN_ATTR_S attr;
} RkmediaVIAttr;

typedef struct _RkmediaChannel {
  MOD_ID_E mode_id;
  CHN_STATUS status;
  std::shared_ptr<easymedia::Flow> rkmedia_flow;
  OutCbFunc cb;
  union {
    RkmediaVIAttr vi_attr;
    RkmediaVencAttr venc_attr;
  };
} RkmediaChannel;

RkmediaChannel g_vi_chns[VI_MAX_CHN_NUM];
std::mutex g_vi_mtx;

RkmediaChannel g_venc_chns[VENC_MAX_CHN_NUM];
std::mutex g_venc_mtx;

/********************************************************************
 * SYS Ctrl api
 ********************************************************************/
static void Reset_Channel_Table(RkmediaChannel *tbl, int cnt, MOD_ID_E mid) {
  for (int i = 0; i < cnt; i++) {
    tbl[i].mode_id = mid;
    tbl[i].status = CHN_STATUS_CLOSED;
    tbl[i].cb = nullptr;
  }
}

RK_S32 RK_MPI_SYS_Init() {
  LOG_INIT();

  // memset(g_vi_dev, 0, VI_MAX_DEV_NUM * sizeof(RkmediaVideoDev));
  g_vi_chns[0].vi_attr.path = (char *)"/dev/video13"; // rkispp_bypass
  g_vi_chns[1].vi_attr.path = (char *)"/dev/video14"; // rkispp_scal0
  g_vi_chns[2].vi_attr.path = (char *)"/dev/video15"; // rkispp_scal1
  g_vi_chns[3].vi_attr.path = (char *)"/dev/video16"; // rkispp_scal2

  Reset_Channel_Table(g_vi_chns, VI_MAX_CHN_NUM, RK_ID_VI);
  Reset_Channel_Table(g_venc_chns, VENC_MAX_CHN_NUM, RK_ID_VENC);

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_SYS_Bind(const MPP_CHN_S *pstSrcChn,
                       const MPP_CHN_S *pstDestChn) {
  std::shared_ptr<easymedia::Flow> src;
  std::shared_ptr<easymedia::Flow> sink;
  RkmediaChannel *src_chn = NULL;
  RkmediaChannel *dst_chn = NULL;

  switch (pstSrcChn->enModId) {
  case RK_ID_VI:
    if (g_vi_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    src = g_vi_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_vi_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_VENC:
    if (g_venc_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    src = g_venc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_venc_chns[pstSrcChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  switch (pstDestChn->enModId) {
  case RK_ID_VI:
    if (g_vi_chns[pstDestChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_vi_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_vi_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_VENC:
    if (g_venc_chns[pstDestChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_venc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_venc_chns[pstDestChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  if (!src) {
    LOG("ERROR: %s Src Chn[%d] is not ready!\n", __func__, pstSrcChn->s32ChnId);
    return -RK_ERR_SYS_NOTREADY;
  }

  if (!sink) {
    LOG("ERROR: %s Dst Chn[%d] is not ready!\n", __func__,
        pstDestChn->s32ChnId);
    return -RK_ERR_SYS_NOTREADY;
  }

  // Rkmedia flow bind
  src->AddDownFlow(sink, 0, 0);

  // change status frome OPEN to BIND.
  src_chn->status = CHN_STATUS_BIND;
  dst_chn->status = CHN_STATUS_BIND;

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_SYS_UnBind(const MPP_CHN_S *pstSrcChn,
                         const MPP_CHN_S *pstDestChn) {
  std::shared_ptr<easymedia::Flow> src;
  std::shared_ptr<easymedia::Flow> sink;
  RkmediaChannel *src_chn = NULL;
  RkmediaChannel *dst_chn = NULL;

  switch (pstSrcChn->enModId) {
  case RK_ID_VI:
    if (g_vi_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    src = g_vi_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_vi_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_VENC:
    if (g_venc_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    src = g_venc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_venc_chns[pstSrcChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  switch (pstDestChn->enModId) {
  case RK_ID_VI:
    if (g_vi_chns[pstDestChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_vi_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_vi_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_VENC:
    if (g_venc_chns[pstDestChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_venc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_venc_chns[pstDestChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  if (!src) {
    LOG("ERROR: %s Src Chn[%d] is not ready!\n", __func__, pstSrcChn->s32ChnId);
    return -RK_ERR_SYS_NOTREADY;
  }

  if (!sink) {
    LOG("ERROR: %s Dst Chn[%d] is not ready!\n", __func__,
        pstDestChn->s32ChnId);
    return -RK_ERR_SYS_NOTREADY;
  }

  // Rkmedia flow unbind
  src->RemoveDownFlow(sink);

  // change status frome BIND to OPEN.
  src_chn->status = CHN_STATUS_OPEN;
  dst_chn->status = CHN_STATUS_OPEN;

  return RK_ERR_SYS_OK;
}

static void
FlowOutputCallback(void *handle,
                   std::shared_ptr<easymedia::MediaBuffer> rkmedia_mb) {
  if (!rkmedia_mb)
    return;

  if (!handle) {
    LOG("ERROR: %s without handle!\n", __func__);
    return;
  }

  RkmediaChannel *target_chn = (RkmediaChannel *)handle;
  if (target_chn->status < CHN_STATUS_OPEN) {
    LOG("ERROR: %s chn[mode:%d] in status[%d] should not call output "
        "callback!\n",
        __func__, target_chn->mode_id, target_chn->status);
    return;
  }

  if (!target_chn->cb) {
    LOG("ERROR: %s chn[mode:%d] in has no callback!\n", __func__,
        target_chn->mode_id);
    return;
  }

  MEDIA_BUFFER_IMPLE *mb = new MEDIA_BUFFER_IMPLE;
  mb->ptr = rkmedia_mb->GetPtr();
  mb->fd = rkmedia_mb->GetFD();
  mb->size = rkmedia_mb->GetValidSize();
  mb->rkmedia_mb = rkmedia_mb;
  mb->mode_id = target_chn->mode_id;
  target_chn->cb(mb);
}

RK_S32 RK_MPI_SYS_RegisterOutCb(const MPP_CHN_S *pstChn, OutCbFunc cb) {
  std::shared_ptr<easymedia::Flow> flow;
  RkmediaChannel *target_chn = NULL;

  switch (pstChn->enModId) {
  case RK_ID_VI:
    if (g_vi_chns[pstChn->s32ChnId].status < CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    flow = g_vi_chns[pstChn->s32ChnId].rkmedia_flow;
    target_chn = &g_vi_chns[pstChn->s32ChnId];
    break;
  case RK_ID_VENC:
    if (g_venc_chns[pstChn->s32ChnId].status < CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    flow = g_venc_chns[pstChn->s32ChnId].rkmedia_flow;
    target_chn = &g_venc_chns[pstChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  target_chn->cb = cb;
  flow->SetOutputCallBack(target_chn, FlowOutputCallback);

  return RK_ERR_SYS_OK;
}

/********************************************************************
 * Vi api
 ********************************************************************/
_CAPI RK_S32 RK_MPI_VI_SetChnAttr(VI_PIPE ViPipe, VI_CHN ViChn,
                                  const VI_CHN_ATTR_S *pstChnAttr) {
  if ((ViPipe < 0) || (ViChn < 0) || (ViChn > VI_MAX_CHN_NUM))
    return -RK_ERR_VI_INVALID_CHNID;

  if (!pstChnAttr)
    return -RK_ERR_SYS_NOT_PERM;

  g_vi_mtx.lock();
  if (g_vi_chns[ViChn].status != CHN_STATUS_CLOSED) {
    g_vi_mtx.unlock();
    return -RK_ERR_VI_BUSY;
  }

  memcpy(&g_vi_chns[ViChn].vi_attr.attr, pstChnAttr, sizeof(VI_CHN_ATTR_S));
  g_vi_chns[ViChn].status = CHN_STATUS_READY;
  g_vi_mtx.unlock();

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VI_EnableChn(VI_PIPE ViPipe, VI_CHN ViChn) {
  if ((ViPipe < 0) || (ViChn < 0) || (ViChn > VI_MAX_CHN_NUM))
    return -RK_ERR_VI_INVALID_CHNID;

  g_vi_mtx.lock();
  if (g_vi_chns[ViChn].status != CHN_STATUS_READY) {
    g_vi_mtx.unlock();
    return (g_vi_chns[ViChn].status > CHN_STATUS_READY) ? -RK_ERR_VI_EXIST
                                                        : -RK_ERR_VI_NOT_CONFIG;
  }

  // Reading yuv from camera
  std::string flow_name = "source_stream";
  std::string flow_param;
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "v4l2_capture_stream");
  std::string stream_param;
  PARAM_STRING_APPEND_TO(stream_param, KEY_USE_LIBV4L2, 1);
  PARAM_STRING_APPEND(stream_param, KEY_DEVICE, g_vi_chns[ViChn].vi_attr.path);
  PARAM_STRING_APPEND(stream_param, KEY_V4L2_CAP_TYPE,
                      KEY_V4L2_C_TYPE(VIDEO_CAPTURE));
  PARAM_STRING_APPEND(stream_param, KEY_V4L2_MEM_TYPE,
                      KEY_V4L2_M_TYPE(MEMORY_DMABUF));
  PARAM_STRING_APPEND_TO(stream_param, KEY_FRAMES,
                         g_vi_chns[ViChn].vi_attr.attr.buffer_cnt);
  PARAM_STRING_APPEND(stream_param, KEY_OUTPUTDATATYPE,
                      ImageTypeToString(g_vi_chns[ViChn].vi_attr.attr.pix_fmt));
  PARAM_STRING_APPEND_TO(stream_param, KEY_BUFFER_WIDTH,
                         g_vi_chns[ViChn].vi_attr.attr.width);
  PARAM_STRING_APPEND_TO(stream_param, KEY_BUFFER_HEIGHT,
                         g_vi_chns[ViChn].vi_attr.attr.height);
  flow_param = easymedia::JoinFlowParam(flow_param, 1, stream_param);

  g_vi_chns[ViChn].rkmedia_flow = easymedia::REFLECTOR(
      Flow)::Create<easymedia::Flow>(flow_name.c_str(), flow_param.c_str());
  g_vi_chns[ViChn].status = CHN_STATUS_OPEN;

  g_vi_mtx.unlock();
  return RK_ERR_SYS_OK;
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

  return RK_ERR_SYS_OK;
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

  std::string flow_name = "video_enc";
  std::string flow_param;
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "rkmpp");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE,
                      ImageTypeToString(stVencChnAttr->stVencAttr.imageType));
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE,
                      CodecToString(stVencChnAttr->stVencAttr.enType));

  std::string enc_param;
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_WIDTH,
                         stVencChnAttr->stVencAttr.u32PicWidth);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_HEIGHT,
                         stVencChnAttr->stVencAttr.u32PicHeight);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_VIR_WIDTH,
                         stVencChnAttr->stVencAttr.u32VirWidth);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_VIR_HEIGHT,
                         stVencChnAttr->stVencAttr.u32VirHeight);
  switch (stVencChnAttr->stVencAttr.enType) {
  case CODEC_TYPE_H264:
    PARAM_STRING_APPEND_TO(enc_param, KEY_PROFILE,
                           stVencChnAttr->stVencAttr.u32Profile);
    break;
  default:
    break;
  }

  memcpy(&g_venc_chns[VeChn].venc_attr, stVencChnAttr,
         sizeof(g_venc_chns[VeChn].venc_attr));

  std::string str_fps_in, str_fsp;
  switch (stVencChnAttr->stRcAttr.enRcMode) {
  case VENC_RC_MODE_H264CBR:
    PARAM_STRING_APPEND(enc_param, KEY_COMPRESS_RC_MODE, KEY_CBR);
    PARAM_STRING_APPEND_TO(enc_param, KEY_VIDEO_GOP,
                           stVencChnAttr->stRcAttr.stH264Cbr.u32Gop);
    PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE,
                           stVencChnAttr->stRcAttr.stH264Cbr.u32BitRate);
    str_fps_in
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Cbr.u32SrcFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Cbr.u32SrcFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS_IN, str_fps_in);

    str_fsp
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Cbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Cbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fsp);
    break;
  case VENC_RC_MODE_H264VBR:
    PARAM_STRING_APPEND(enc_param, KEY_COMPRESS_RC_MODE, KEY_VBR);
    PARAM_STRING_APPEND_TO(enc_param, KEY_VIDEO_GOP,
                           stVencChnAttr->stRcAttr.stH264Vbr.u32Gop);
    PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE_MAX,
                           stVencChnAttr->stRcAttr.stH264Vbr.u32MaxBitRate);
    PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE_MAX,
                           stVencChnAttr->stRcAttr.stH264Vbr.u32MinBitRate);
    str_fps_in
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Vbr.u32SrcFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Vbr.u32SrcFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS_IN, str_fps_in);

    str_fsp
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Vbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Vbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fsp);
    break;
  case VENC_RC_MODE_H265CBR:
    PARAM_STRING_APPEND(enc_param, KEY_COMPRESS_RC_MODE, KEY_CBR);
    PARAM_STRING_APPEND_TO(enc_param, KEY_VIDEO_GOP,
                           stVencChnAttr->stRcAttr.stH265Cbr.u32Gop);
    PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE,
                           stVencChnAttr->stRcAttr.stH265Cbr.u32BitRate);
    str_fps_in
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Cbr.u32SrcFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Cbr.u32SrcFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS_IN, str_fps_in);

    str_fsp
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Cbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Cbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fsp);
    break;
  case VENC_RC_MODE_H265VBR:
    PARAM_STRING_APPEND(enc_param, KEY_COMPRESS_RC_MODE, KEY_VBR);
    PARAM_STRING_APPEND_TO(enc_param, KEY_VIDEO_GOP,
                           stVencChnAttr->stRcAttr.stH265Vbr.u32Gop);
    PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE_MAX,
                           stVencChnAttr->stRcAttr.stH265Vbr.u32MaxBitRate);
    PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE_MAX,
                           stVencChnAttr->stRcAttr.stH265Vbr.u32MinBitRate);

    str_fps_in
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Vbr.u32SrcFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Vbr.u32SrcFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS_IN, str_fps_in);

    str_fsp
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Vbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Vbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fsp);
    break;
  case VENC_RC_MODE_MJPEGCBR:
    PARAM_STRING_APPEND(enc_param, KEY_COMPRESS_RC_MODE, KEY_CBR);
    PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE,
                           stVencChnAttr->stRcAttr.stMjpegCbr.u32BitRate);
    str_fps_in
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stMjpegCbr.u32SrcFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stMjpegCbr.u32SrcFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS_IN, str_fps_in);

    str_fsp
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stMjpegCbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stMjpegCbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fsp);
    break;
  default:
    break;
  }

  PARAM_STRING_APPEND_TO(enc_param, KEY_FULL_RANGE, 0);

  flow_param = easymedia::JoinFlowParam(flow_param, 1, enc_param);
  g_venc_chns[VeChn].rkmedia_flow = easymedia::REFLECTOR(
      Flow)::Create<easymedia::Flow>("video_enc", flow_param.c_str());

  g_venc_chns[VeChn].status = CHN_STATUS_OPEN;

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetRcParam(VENC_CHN VeChn,
                              const VENC_RC_PARAM_S *pstRcParam) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID; //对应HI_ERR_VENC_INVALID_CHNID
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();

  VideoEncoderQp qp;

  qp.qp_init = pstRcParam->s32FirstFrameStartQp;
  switch (g_venc_chns[VeChn].venc_attr.attr.enType) {
  case CODEC_TYPE_H264:
    qp.qp_step = pstRcParam->stParamH264.u32StepQp;
    qp.qp_max = pstRcParam->stParamH264.u32MaxQp;
    qp.qp_min = pstRcParam->stParamH264.u32MinQp;
    qp.qp_max_i = pstRcParam->stParamH264.u32MaxIQp;
    qp.qp_min_i = pstRcParam->stParamH264.u32MinIQp;
    break;
  case CODEC_TYPE_H265:
    qp.qp_step = pstRcParam->stParamH265.u32StepQp;
    qp.qp_max = pstRcParam->stParamH265.u32MaxQp;
    qp.qp_min = pstRcParam->stParamH265.u32MinQp;
    qp.qp_max_i = pstRcParam->stParamH265.u32MaxIQp;
    qp.qp_min_i = pstRcParam->stParamH265.u32MinIQp;
    break;
  case CODEC_TYPE_JPEG:
    break;
  default:
    break;
  }
  video_encoder_set_qp(g_venc_chns[VeChn].rkmedia_flow, qp);
  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetRcMode(VENC_CHN VeChn, VENC_RC_MODE_E RcMode) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  switch (RcMode) {
  case VENC_RC_MODE_H264CBR:
    video_encoder_set_rc_mode(g_venc_chns[VeChn].rkmedia_flow, KEY_CBR);
    break;
  case VENC_RC_MODE_H264VBR:
    video_encoder_set_rc_mode(g_venc_chns[VeChn].rkmedia_flow, KEY_VBR);
    break;
  case VENC_RC_MODE_H265CBR:
    video_encoder_set_rc_mode(g_venc_chns[VeChn].rkmedia_flow, KEY_CBR);
    break;
  case VENC_RC_MODE_H265VBR:
    video_encoder_set_rc_mode(g_venc_chns[VeChn].rkmedia_flow, KEY_VBR);
    break;
  case VENC_RC_MODE_MJPEGCBR:
    video_encoder_set_rc_mode(g_venc_chns[VeChn].rkmedia_flow, KEY_CBR);
    break;
  default:
    break;
  }
  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetRcQuality(VENC_CHN VeChn, VENC_RC_QUALITY_E RcQuality) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  switch (RcQuality) {
  case VENC_RC_QUALITY_HIGHEST:
    video_encoder_set_rc_quality(g_venc_chns[VeChn].rkmedia_flow, KEY_HIGHEST);
    break;
  case VENC_RC_QUALITY_HIGHER:
    video_encoder_set_rc_quality(g_venc_chns[VeChn].rkmedia_flow, KEY_HIGHER);
    break;
  case VENC_RC_QUALITY_HIGH:
    video_encoder_set_rc_quality(g_venc_chns[VeChn].rkmedia_flow, KEY_HIGH);
    break;
  case VENC_RC_QUALITY_MEDIUM:
    video_encoder_set_rc_quality(g_venc_chns[VeChn].rkmedia_flow, KEY_MEDIUM);
    break;
  case VENC_RC_QUALITY_LOW:
    video_encoder_set_rc_quality(g_venc_chns[VeChn].rkmedia_flow, KEY_LOW);
    break;
  case VENC_RC_QUALITY_LOWER:
    video_encoder_set_rc_quality(g_venc_chns[VeChn].rkmedia_flow, KEY_LOWER);
    break;
  case VENC_RC_QUALITY_LOWEST:
    video_encoder_set_rc_quality(g_venc_chns[VeChn].rkmedia_flow, KEY_LOWEST);
    break;
  default:
    break;
  }
  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_SetBitrate(VENC_CHN VeChn, RK_U32 u32BitRate,
                              RK_U32 u32MinBitRate, RK_U32 u32MaxBitRate) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_bps(g_venc_chns[VeChn].rkmedia_flow, u32BitRate,
                        u32MinBitRate, u32MaxBitRate);
  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_RequestIDR(VENC_CHN VeChn, RK_BOOL bInstant _UNUSED) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_force_idr(g_venc_chns[VeChn].rkmedia_flow);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetFps(VENC_CHN VeChn, RK_U8 u8OutNum, RK_U8 u8OutDen,
                          RK_U8 u8InNum, RK_U8 u8InDen) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_fps(g_venc_chns[VeChn].rkmedia_flow, u8OutNum, u8OutDen,
                        u8InNum, u8InDen);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_SetGop(VENC_CHN VeChn, RK_U32 u32Gop) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_gop_size(g_venc_chns[VeChn].rkmedia_flow, u32Gop);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_SetAvcProfile(VENC_CHN VeChn, RK_U32 u32Profile,
                                 RK_U32 u32Level) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_avc_profile(g_venc_chns[VeChn].rkmedia_flow, u32Profile,
                                u32Level);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_InsertUserData(VENC_CHN VeChn, RK_U8 *pu8Data,
                                  RK_U32 u32Len) {

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_userdata(g_venc_chns[VeChn].rkmedia_flow, pu8Data, u32Len);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetRoiAttr(VENC_CHN VeChn,
                              const VENC_ROI_ATTR_S *pstRoiAttr) {

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  EncROIRegion regions;
  regions.x = pstRoiAttr->stRect.s32X;
  regions.y = pstRoiAttr->stRect.s32Y;
  regions.w = pstRoiAttr->stRect.u32Width;
  regions.h = pstRoiAttr->stRect.u32Height;

  regions.area_map_en = pstRoiAttr->bEnable;
  regions.abs_qp_en = pstRoiAttr->bAbsQp;
  regions.qp_area_idx = pstRoiAttr->u32Index;
  regions.quality = pstRoiAttr->s32Qp;

  video_encoder_set_roi_regions(g_venc_chns[VeChn].rkmedia_flow, &regions, 1);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_DestroyChn(VENC_CHN VeChn) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return RK_ERR_VENC_INVALID_CHNID;

  g_venc_mtx.lock();
  if (g_venc_chns[VeChn].status == CHN_STATUS_BIND) {
    g_venc_mtx.unlock();
    return RK_ERR_VENC_BUSY;
  }

  g_venc_chns[VeChn].rkmedia_flow.reset();
  g_venc_chns[VeChn].status = CHN_STATUS_CLOSED;
  g_venc_mtx.unlock();

  return RK_ERR_SYS_OK;
}
