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

#include "osd/color_table.h"
#include "rkmedia_api.h"
#include "rkmedia_buffer_impl.h"
#include "rkmedia_utils.h"

using namespace easymedia;

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

typedef struct _RkmediaAIAttr { AI_CHN_ATTR_S attr; } RkmediaAIAttr;

typedef struct _RkmediaAOAttr { AO_CHN_ATTR_S attr; } RkmediaAOAttr;

typedef struct _RkmediaAENCAttr { AENC_CHN_ATTR_S attr; } RkmediaAENCAttr;

typedef ALGO_MD_ATTR_S RkmediaMDAttr;

typedef struct _RkmediaChannel {
  MOD_ID_E mode_id;
  CHN_STATUS status;
  std::shared_ptr<easymedia::Flow> rkmedia_flow;
  OutCbFunc cb;
  EventCbFunc event_cb;
  union {
    RkmediaVIAttr vi_attr;
    RkmediaVencAttr venc_attr;
    RkmediaAIAttr ai_attr;
    RkmediaAOAttr ao_attr;
    RkmediaAENCAttr aenc_attr;
    RkmediaMDAttr md_attr;
  };
} RkmediaChannel;

RkmediaChannel g_vi_chns[VI_MAX_CHN_NUM];
std::mutex g_vi_mtx;

RkmediaChannel g_venc_chns[VENC_MAX_CHN_NUM];
std::mutex g_venc_mtx;

RkmediaChannel g_ai_chns[AI_MAX_CHN_NUM];
std::mutex g_ai_mtx;

RkmediaChannel g_ao_chns[AO_MAX_CHN_NUM];
std::mutex g_ao_mtx;

RkmediaChannel g_aenc_chns[AENC_MAX_CHN_NUM];
std::mutex g_aenc_mtx;

RkmediaChannel g_algo_md_chns[ALGO_MD_MAX_CHN_NUM];
std::mutex g_algo_md_mtx;

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
  Reset_Channel_Table(g_ai_chns, AI_MAX_CHN_NUM, RK_ID_AI);
  Reset_Channel_Table(g_aenc_chns, AENC_MAX_CHN_NUM, RK_ID_AENC);
  Reset_Channel_Table(g_ao_chns, AO_MAX_CHN_NUM, RK_ID_AO);
  Reset_Channel_Table(g_algo_md_chns, ALGO_MD_MAX_CHN_NUM, RK_ID_ALGO_MD);

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
  case RK_ID_AI:
    if (g_ai_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    src = g_ai_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_ai_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_AENC:
    if (g_aenc_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    src = g_aenc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_aenc_chns[pstSrcChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  switch (pstDestChn->enModId) {
  case RK_ID_VENC:
    if (g_venc_chns[pstDestChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_venc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_venc_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AO:
    if (g_ao_chns[pstDestChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_ao_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_ao_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AENC:
    if (g_aenc_chns[pstDestChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_aenc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_aenc_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_ALGO_MD:
    if (g_algo_md_chns[pstDestChn->s32ChnId].status != CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_algo_md_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_algo_md_chns[pstDestChn->s32ChnId];
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
  case RK_ID_AI:
    if (g_ai_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    src = g_ai_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_ai_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_AO:
    if (g_ao_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    src = g_ao_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_ao_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_AENC:
    if (g_aenc_chns[pstSrcChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    src = g_aenc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_aenc_chns[pstSrcChn->s32ChnId];
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
  case RK_ID_AI:
    if (g_ai_chns[pstDestChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_ai_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_ai_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AO:
    if (g_ao_chns[pstDestChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_ao_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_ao_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AENC:
    if (g_aenc_chns[pstDestChn->s32ChnId].status != CHN_STATUS_BIND)
      return -RK_ERR_SYS_NOTREADY;
    sink = g_aenc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_aenc_chns[pstDestChn->s32ChnId];
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
  case RK_ID_AI:
    if (g_ai_chns[pstChn->s32ChnId].status < CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    flow = g_ai_chns[pstChn->s32ChnId].rkmedia_flow;
    target_chn = &g_ai_chns[pstChn->s32ChnId];
    break;
  case RK_ID_AENC:
    if (g_aenc_chns[pstChn->s32ChnId].status < CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    flow = g_aenc_chns[pstChn->s32ChnId].rkmedia_flow;
    target_chn = &g_aenc_chns[pstChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  target_chn->cb = cb;
  flow->SetOutputCallBack(target_chn, FlowOutputCallback);

  return RK_ERR_SYS_OK;
}

static void FlowEventCallback(void *handle, void *data) {
  if (!data)
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

  if (!target_chn->event_cb) {
    LOG("ERROR: %s chn[mode:%d] in has no callback!\n", __func__,
        target_chn->mode_id);
    return;
  }

  switch (target_chn->mode_id) {
  case RK_ID_ALGO_MD: {
    MoveDetectEvent *rkmedia_md_event = (MoveDetectEvent *)data;
    MoveDetecInfo *rkmedia_md_info = rkmedia_md_event->data;
    EVENT_S stEvent;

    stEvent.mode_id = RK_ID_ALGO_MD;
    stEvent.type = RK_EVENT_MD;
    stEvent.md_event.u16Cnt = rkmedia_md_event->info_cnt;
    stEvent.md_event.u32Width = rkmedia_md_event->ori_width;
    stEvent.md_event.u32Height = rkmedia_md_event->ori_height;
    for (int i = 0; i < rkmedia_md_event->info_cnt; i++) {
      stEvent.md_event.stRects[i].s32X = (RK_S32)rkmedia_md_info[i].x;
      stEvent.md_event.stRects[i].s32Y = (RK_S32)rkmedia_md_info[i].y;
      stEvent.md_event.stRects[i].u32Width = (RK_S32)rkmedia_md_info[i].w;
      stEvent.md_event.stRects[i].u32Width = (RK_S32)rkmedia_md_info[i].h;
    }
    target_chn->event_cb(&stEvent);
  } break;
  default:
    LOG("ERROR: Channle Mode ID:%d not support event callback!\n",
        target_chn->mode_id);
    break;
  }
}

RK_S32 RK_MPI_SYS_RegisterEventCb(const MPP_CHN_S *pstChn, EventCbFunc cb) {
  std::shared_ptr<easymedia::Flow> flow;
  RkmediaChannel *target_chn = NULL;

  switch (pstChn->enModId) {
  case RK_ID_ALGO_MD:
    if (g_algo_md_chns[pstChn->s32ChnId].status < CHN_STATUS_OPEN)
      return -RK_ERR_SYS_NOTREADY;
    flow = g_algo_md_chns[pstChn->s32ChnId].rkmedia_flow;
    target_chn = &g_algo_md_chns[pstChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  target_chn->event_cb = cb;
  flow->SetEventCallBack(target_chn, FlowEventCallback);

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
    return -RK_ERR_VENC_INVALID_CHNID;

  g_venc_mtx.lock();
  if (g_venc_chns[VeChn].status != CHN_STATUS_CLOSED) {
    g_venc_mtx.unlock();
    return -RK_ERR_VENC_EXIST;
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
  PARAM_STRING_APPEND_TO(enc_param, KEY_REF_FRM_CFG,
                         stVencChnAttr->stGopAttr.enGopMode);

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
    return -RK_ERR_VENC_INVALID_CHNID; //对应HI_ERR_VENC_INVALID_CHNID
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

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
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

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
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

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
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status != CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_bps(g_venc_chns[VeChn].rkmedia_flow, u32BitRate,
                        u32MinBitRate, u32MaxBitRate);
  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_RequestIDR(VENC_CHN VeChn, RK_BOOL bInstant _UNUSED) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_force_idr(g_venc_chns[VeChn].rkmedia_flow);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetFps(VENC_CHN VeChn, RK_U8 u8OutNum, RK_U8 u8OutDen,
                          RK_U8 u8InNum, RK_U8 u8InDen) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_fps(g_venc_chns[VeChn].rkmedia_flow, u8OutNum, u8OutDen,
                        u8InNum, u8InDen);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_SetGop(VENC_CHN VeChn, RK_U32 u32Gop) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_gop_size(g_venc_chns[VeChn].rkmedia_flow, u32Gop);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_SetAvcProfile(VENC_CHN VeChn, RK_U32 u32Profile,
                                 RK_U32 u32Level) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_avc_profile(g_venc_chns[VeChn].rkmedia_flow, u32Profile,
                                u32Level);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}
RK_S32 RK_MPI_VENC_InsertUserData(VENC_CHN VeChn, RK_U8 *pu8Data,
                                  RK_U32 u32Len) {

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

  g_venc_mtx.lock();
  video_encoder_set_userdata(g_venc_chns[VeChn].rkmedia_flow, pu8Data, u32Len);

  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetRoiAttr(VENC_CHN VeChn,
                              const VENC_ROI_ATTR_S *pstRoiAttr) {

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

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
    return -RK_ERR_VENC_INVALID_CHNID;

  g_venc_mtx.lock();
  if (g_venc_chns[VeChn].status == CHN_STATUS_BIND) {
    g_venc_mtx.unlock();
    return -RK_ERR_VENC_BUSY;
  }

  g_venc_chns[VeChn].rkmedia_flow.reset();
  g_venc_chns[VeChn].status = CHN_STATUS_CLOSED;
  g_venc_mtx.unlock();

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetGopMode(VENC_CHN VeChn, VENC_GOP_MODE_E GopMode) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;
  g_venc_mtx.lock();
  video_encoder_set_ref_frm_cfg(g_venc_chns[VeChn].rkmedia_flow, GopMode);
  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_InitOsd(VENC_CHN VeChn) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;

  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN) {
    LOG("ERROR: Venc[%d] should be opened before init osd!\n");
    return -RK_ERR_VENC_NOTREADY;
  }

  return easymedia::video_encoder_set_osd_plt(g_venc_chns[VeChn].rkmedia_flow,
                                              yuv444_palette_table);
}

static RK_VOID Argb1555_To_Region_Data(const BITMAP_S *pstBitmap, RK_U8 *data,
                                       RK_U32 canvasWidth,
                                       RK_U32 canvasHeight) {
  RK_U8 value_r, value_g, value_b, value_a;
  RK_U32 TargetWidth, TargetHeight;
  RK_U32 ColorValue;
  RK_U32 *BitmapLineStart;
  RK_U8 *CanvasLineStart;

  TargetWidth =
      (pstBitmap->u32Width > canvasWidth) ? canvasWidth : pstBitmap->u32Width;
  TargetHeight = (pstBitmap->u32Height > canvasHeight) ? canvasHeight
                                                       : pstBitmap->u32Height;

  LOGD("%s Bitmap[%d, %d] -> Canvas[%d, %d], target=<%d, %d>\n", __func__,
       pstBitmap->u32Width, pstBitmap->u32Height, canvasWidth, canvasHeight,
       TargetWidth, TargetHeight);

  // Initialize all pixels to transparent color
  if ((canvasWidth > pstBitmap->u32Width) ||
      (canvasHeight > pstBitmap->u32Height))
    memset(data, 0xFF, canvasWidth * canvasHeight);

  for (RK_U32 i = 0; i < TargetHeight; i++) {
    BitmapLineStart = (RK_U32 *)pstBitmap->pData + i * pstBitmap->u32Width;
    CanvasLineStart = data + i * canvasWidth;
    for (RK_U32 j = 0; j < TargetWidth; j++) {
      ColorValue = *(BitmapLineStart + j);
      value_a = (ColorValue & 0x8000) >> 15;
      value_r = (ColorValue & 0x7C00) >> 10;
      value_g = (ColorValue & 0x03E0) >> 5;
      value_b = (ColorValue & 0x001F);
      if (value_a == 0)
        *(CanvasLineStart + j) = PALETTE_TABLE_LEN - 1; // Transparent
      else
        *(CanvasLineStart + j) =
            find_color(bgra8888_palette_table, PALETTE_TABLE_LEN, value_r,
                       value_g, value_b);
    }
  }
}

static RK_VOID Argb8888_To_Region_Data(const BITMAP_S *pstBitmap, RK_U8 *data,
                                       RK_U32 canvasWidth,
                                       RK_U32 canvasHeight) {
  RK_U8 value_r, value_g, value_b, value_a;
  RK_U32 TargetWidth, TargetHeight;
  RK_U32 ColorValue;
  RK_U32 *BitmapLineStart;
  RK_U8 *CanvasLineStart;

  TargetWidth =
      (pstBitmap->u32Width > canvasWidth) ? canvasWidth : pstBitmap->u32Width;
  TargetHeight = (pstBitmap->u32Height > canvasHeight) ? canvasHeight
                                                       : pstBitmap->u32Height;

  LOGD("%s Bitmap[%d, %d] -> Canvas[%d, %d], target=<%d, %d>\n", __func__,
       pstBitmap->u32Width, pstBitmap->u32Height, canvasWidth, canvasHeight,
       TargetWidth, TargetHeight);

  // Initialize all pixels to transparent color
  if ((canvasWidth > pstBitmap->u32Width) ||
      (canvasHeight > pstBitmap->u32Height))
    memset(data, 0xFF, canvasWidth * canvasHeight);

  for (RK_U32 i = 0; i < TargetHeight; i++) {
    BitmapLineStart = (RK_U32 *)pstBitmap->pData + i * pstBitmap->u32Width;
    CanvasLineStart = data + i * canvasWidth;
    for (RK_U32 j = 0; j < TargetWidth; j++) {
      ColorValue = *(BitmapLineStart + j);
      value_a = (ColorValue & 0xF0000000) >> 24;
      value_r = (ColorValue & 0x00FF0000) >> 16;
      value_g = (ColorValue & 0x0000FF00) >> 8;
      value_b = (ColorValue & 0x000000FF);
      if (value_a == 0)
        *(CanvasLineStart + j) = PALETTE_TABLE_LEN - 1; // Transparent
      else
        *(CanvasLineStart + j) =
            find_color(bgra8888_palette_table, PALETTE_TABLE_LEN, value_r,
                       value_g, value_b);
    }
  }
}

RK_S32 RK_MPI_VENC_SetBitMap(VENC_CHN VeChn,
                             const OSD_REGION_INFO_S *pstRgnInfo,
                             const BITMAP_S *pstBitmap) {
  RK_U8 rkmedia_osd_data[OSD_PIX_NUM_MAX] = {0xFF};
  RK_U32 total_pix_num = 0;
  RK_S32 ret = RK_ERR_SYS_OK;

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;

  if (!pstBitmap || !pstBitmap->pData || !pstBitmap->u32Width ||
      !pstBitmap->u32Height)
    return -RK_ERR_VENC_ILLEGAL_PARAM;

  if (!pstRgnInfo || !pstRgnInfo->u32Width || !pstRgnInfo->u32Height)
    return -RK_ERR_VENC_ILLEGAL_PARAM;

  if ((pstRgnInfo->u32PosX % 16) || (pstRgnInfo->u32PosY % 16) ||
      (pstRgnInfo->u32Width % 16) || (pstRgnInfo->u32Height % 16)) {
    LOG("ERROR: <x, y, w, h> = <%d, %d, %d, %d> must be 16 aligned!\n",
        pstRgnInfo->u32PosX, pstRgnInfo->u32PosY, pstRgnInfo->u32Width,
        pstRgnInfo->u32Height);
    return -RK_ERR_VENC_ILLEGAL_PARAM;
  }

  total_pix_num = pstRgnInfo->u32Width * pstRgnInfo->u32Height;
  if (total_pix_num > OSD_PIX_NUM_MAX) {
    LOG("ERROR: RgnInfo pixels(%d) exceed the maximum number of osd "
        "pixels(%d)\n",
        total_pix_num, OSD_PIX_NUM_MAX);
    return -RK_ERR_VENC_ILLEGAL_PARAM;
  }

  switch (pstBitmap->enPixelFormat) {
  case PIXEL_FORMAT_ARGB_1555:
    Argb1555_To_Region_Data(pstBitmap, rkmedia_osd_data, pstRgnInfo->u32Width,
                            pstRgnInfo->u32Height);
    break;
  case PIXEL_FORMAT_ARGB_8888:
    Argb8888_To_Region_Data(pstBitmap, rkmedia_osd_data, pstRgnInfo->u32Width,
                            pstRgnInfo->u32Height);
    break;
  default:
    LOG("ERROR: Not support bitmap pixel format:%d\n",
        pstBitmap->enPixelFormat);
    ret = RK_ERR_VENC_NOT_SUPPORT;
    break;
  }

  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN) {
    LOG("ERROR: Venc[%d] should be opened before set bitmap!\n");
    return -RK_ERR_VENC_NOTREADY;
  }

  OsdRegionData rkmedia_osd_rgn;
  rkmedia_osd_rgn.buffer = rkmedia_osd_data;
  rkmedia_osd_rgn.region_id = pstRgnInfo->enRegionId;
  rkmedia_osd_rgn.pos_x = pstRgnInfo->u32PosX;
  rkmedia_osd_rgn.pos_y = pstRgnInfo->u32PosY;
  rkmedia_osd_rgn.width = pstRgnInfo->u32Width;
  rkmedia_osd_rgn.height = pstRgnInfo->u32Height;
  rkmedia_osd_rgn.inverse = pstRgnInfo->u8Inverse;
  rkmedia_osd_rgn.enable = pstRgnInfo->u8Enable;
  easymedia::video_encoder_set_osd_region(g_venc_chns[VeChn].rkmedia_flow,
                                          &rkmedia_osd_rgn);

  return ret;
}

/********************************************************************
 * Ai api
 ********************************************************************/
static std::shared_ptr<easymedia::Flow>
create_flow(const std::string &flow_name, const std::string &flow_param,
            const std::string &elem_param) {
  auto &&param = easymedia::JoinFlowParam(flow_param, 1, elem_param);
  auto ret = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), param.c_str());
  if (!ret)
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
  return ret;
}

static std::shared_ptr<easymedia::Flow>
create_alsa_flow(std::string aud_in_path, SampleInfo &info, bool capture) {
  std::string flow_name;
  std::string flow_param;
  std::string sub_param;
  std::string stream_name;

  if (capture) {
    // default sync mode
    flow_name = "source_stream";
    stream_name = "alsa_capture_stream";
  } else {
    flow_name = "output_stream";
    stream_name = "alsa_playback_stream";
    PARAM_STRING_APPEND(flow_param, KEK_THREAD_SYNC_MODEL, KEY_ASYNCCOMMON);
    PARAM_STRING_APPEND(flow_param, KEK_INPUT_MODEL, KEY_DROPFRONT);
    PARAM_STRING_APPEND_TO(flow_param, KEY_INPUT_CACHE_NUM, 5);
  }
  flow_param = "";
  sub_param = "";

  PARAM_STRING_APPEND(flow_param, KEY_NAME, stream_name);
  PARAM_STRING_APPEND(sub_param, KEY_DEVICE, aud_in_path);
  PARAM_STRING_APPEND(sub_param, KEY_SAMPLE_FMT, SampleFmtToString(info.fmt));
  PARAM_STRING_APPEND_TO(sub_param, KEY_CHANNELS, info.channels);
  PARAM_STRING_APPEND_TO(sub_param, KEY_FRAMES, info.nb_samples);
  PARAM_STRING_APPEND_TO(sub_param, KEY_SAMPLE_RATE, info.sample_rate);

  auto audio_source_flow = create_flow(flow_name, flow_param, sub_param);
  if (!audio_source_flow) {
    printf("Create flow %s failed\n", flow_name.c_str());
    exit(EXIT_FAILURE);
  } else {
    printf("%s flow ready!\n", flow_name.c_str());
  }
  return audio_source_flow;
}

RK_S32 RK_MPI_AI_SetChnAttr(AI_CHN AiChn, const AI_CHN_ATTR_S *pstAttr) {
  if ((AiChn < 0) || (AiChn >= AI_MAX_CHN_NUM))
    return -RK_ERR_AI_INVALID_DEVID;

  g_ai_mtx.lock();
  if (!pstAttr || !pstAttr->path)
    return -RK_ERR_SYS_NOT_PERM;

  if (g_ai_chns[AiChn].status != CHN_STATUS_CLOSED) {
    g_ai_mtx.unlock();
    return -RK_ERR_AI_BUSY;
  }

  memcpy(&g_ai_chns[AiChn].ai_attr.attr, pstAttr, sizeof(AI_CHN_ATTR_S));
  g_ai_chns[AiChn].status = CHN_STATUS_READY;

  g_ai_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AI_EnableChn(AI_CHN AiChn) {
  if ((AiChn < 0) || (AiChn >= AI_MAX_CHN_NUM))
    return RK_ERR_AI_INVALID_DEVID;
  g_ai_mtx.lock();
  if (g_ai_chns[AiChn].status != CHN_STATUS_READY) {
    g_ai_mtx.unlock();
    return (g_ai_chns[AiChn].status > CHN_STATUS_READY) ? -RK_ERR_AI_EXIST
                                                        : -RK_ERR_AI_NOT_CONFIG;
  }
  SampleInfo info;
  info.channels = g_ai_chns[AiChn].ai_attr.attr.channels;
  info.fmt = (SampleFormat)g_ai_chns[AiChn].ai_attr.attr.fmt;
  info.nb_samples = g_ai_chns[AiChn].ai_attr.attr.nb_samples;
  info.sample_rate = g_ai_chns[AiChn].ai_attr.attr.sample_rate;
  g_ai_chns[AiChn].rkmedia_flow =
      create_alsa_flow(g_ai_chns[AiChn].ai_attr.attr.path, info, RK_TRUE);
  g_ai_chns[AiChn].status = CHN_STATUS_OPEN;
  g_ai_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AI_DisableChn(AI_CHN AiChn) {
  if ((AiChn < 0) || (AiChn > AI_MAX_CHN_NUM))
    return RK_ERR_AI_INVALID_DEVID;

  g_ai_mtx.lock();
  if (g_ai_chns[AiChn].status == CHN_STATUS_BIND) {
    g_ai_mtx.unlock();
    return -RK_ERR_AI_BUSY;
  }

  g_ai_chns[AiChn].rkmedia_flow.reset();
  g_ai_chns[AiChn].status = CHN_STATUS_CLOSED;
  g_ai_mtx.unlock();

  return RK_ERR_SYS_OK;
}

/********************************************************************
 * Ao api
 ********************************************************************/
RK_S32 RK_MPI_AO_SetChnAttr(AO_CHN AoChn, const AO_CHN_ATTR_S *pstAttr) {
  if ((AoChn < 0) || (AoChn >= AO_MAX_CHN_NUM))
    return -RK_ERR_AO_INVALID_DEVID;

  g_ao_mtx.lock();
  if (!pstAttr || !pstAttr->path)
    return -RK_ERR_SYS_NOT_PERM;

  if (g_ao_chns[AoChn].status != CHN_STATUS_CLOSED) {
    g_ao_mtx.unlock();
    return -RK_ERR_AI_BUSY;
  }

  memcpy(&g_ao_chns[AoChn].ao_attr.attr, pstAttr, sizeof(AO_CHN_ATTR_S));
  g_ao_chns[AoChn].status = CHN_STATUS_READY;

  g_ao_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AO_EnableChn(AO_CHN AoChn) {
  if ((AoChn < 0) || (AoChn >= AO_MAX_CHN_NUM))
    return RK_ERR_AO_INVALID_DEVID;
  g_ao_mtx.lock();
  if (g_ao_chns[AoChn].status != CHN_STATUS_READY) {
    g_ao_mtx.unlock();
    return (g_ao_chns[AoChn].status > CHN_STATUS_READY) ? -RK_ERR_VO_EXIST
                                                        : -RK_ERR_VO_NOT_CONFIG;
  }
  SampleInfo info;
  info.channels = g_ao_chns[AoChn].ao_attr.attr.channels;
  info.fmt = (SampleFormat)g_ao_chns[AoChn].ao_attr.attr.fmt;
  info.nb_samples = g_ao_chns[AoChn].ao_attr.attr.nb_samples;
  info.sample_rate = g_ao_chns[AoChn].ao_attr.attr.sample_rate;
  g_ao_chns[AoChn].rkmedia_flow =
      create_alsa_flow(g_ao_chns[AoChn].ao_attr.attr.path, info, RK_FALSE);
  g_ao_chns[AoChn].status = CHN_STATUS_OPEN;
  g_ao_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AO_DisableChn(AO_CHN AoChn) {
  if ((AoChn < 0) || (AoChn > AO_MAX_CHN_NUM))
    return RK_ERR_AO_INVALID_DEVID;

  g_ao_mtx.lock();
  if (g_ao_chns[AoChn].status == CHN_STATUS_BIND) {
    g_ao_mtx.unlock();
    return -RK_ERR_AI_BUSY;
  }

  g_ao_chns[AoChn].rkmedia_flow.reset();
  g_ao_chns[AoChn].status = CHN_STATUS_CLOSED;
  g_ao_mtx.unlock();

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AO_SetVolume(AO_CHN AoChn, RK_S32 s32Volume) {
  if ((AoChn < 0) || (AoChn > AO_MAX_CHN_NUM))
    return RK_ERR_AO_INVALID_DEVID;
  g_ao_mtx.lock();
  if (g_ao_chns[AoChn].status <= CHN_STATUS_READY) {
    g_ao_mtx.unlock();
    return -RK_ERR_AO_NOTOPEN;
  }
  g_ao_chns[AoChn].rkmedia_flow->Control(easymedia::S_ALSA_VOLUME, &s32Volume);
  g_ao_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AO_GetVolume(AO_CHN AoChn, RK_S32 *ps32Volume) {
  if ((AoChn < 0) || (AoChn > AO_MAX_CHN_NUM))
    return RK_ERR_AO_INVALID_DEVID;
  g_ao_mtx.lock();
  if (g_ao_chns[AoChn].status <= CHN_STATUS_READY) {
    g_ao_mtx.unlock();
    return -RK_ERR_AO_NOTOPEN;
  }
  g_ao_chns[AoChn].rkmedia_flow->Control(easymedia::G_ALSA_VOLUME, ps32Volume);
  g_ao_mtx.unlock();
  return RK_ERR_SYS_OK;
}

/********************************************************************
 * Aenc api
 ********************************************************************/
RK_S32 RK_MPI_AENC_CreateChn(AENC_CHN AencChn, const AENC_CHN_ATTR_S *pstAttr) {
  if ((AencChn < 0) || (AencChn >= AENC_MAX_CHN_NUM))
    return -RK_ERR_AENC_INVALID_DEVID;

  g_aenc_mtx.lock();
  if (!pstAttr)
    return -RK_ERR_SYS_NOT_PERM;

  if (g_aenc_chns[AencChn].status != CHN_STATUS_CLOSED) {
    g_aenc_mtx.unlock();
    return -RK_ERR_AI_BUSY;
  }

  memcpy(&g_aenc_chns[AencChn].aenc_attr.attr, pstAttr,
         sizeof(AENC_CHN_ATTR_S));
  g_aenc_chns[AencChn].status = CHN_STATUS_READY;

  std::string flow_name;
  std::string param;
  flow_name = "audio_enc";
  param = "";
  CODEC_TYPE_E codec_type = g_aenc_chns[AencChn].aenc_attr.attr.enType;
  PARAM_STRING_APPEND(param, KEY_NAME, "ffmpeg_aud");
  PARAM_STRING_APPEND(param, KEY_OUTPUTDATATYPE, CodecToString(codec_type));
  RK_S32 nb_sample = 0;
  RK_S32 channels = 0;
  RK_S32 sample_rate = 0;
  Sample_Format_E sample_format;
  switch (codec_type) {
  case RK_CODEC_TYPE_G711A:
    sample_format = RK_SAMPLE_FMT_S16;
    nb_sample = g_aenc_chns[AencChn].aenc_attr.attr.g711a_attr.u32NbSample;
    channels = g_aenc_chns[AencChn].aenc_attr.attr.g711a_attr.u32Channels;
    sample_rate = g_aenc_chns[AencChn].aenc_attr.attr.g711a_attr.u32SampleRate;
    break;
  case RK_CODEC_TYPE_G711U:
    sample_format = RK_SAMPLE_FMT_S16;
    nb_sample = g_aenc_chns[AencChn].aenc_attr.attr.g711u_attr.u32NbSample;
    channels = g_aenc_chns[AencChn].aenc_attr.attr.g711u_attr.u32Channels;
    sample_rate = g_aenc_chns[AencChn].aenc_attr.attr.g711u_attr.u32SampleRate;
    break;
  case RK_CODEC_TYPE_MP2:
    sample_format = RK_SAMPLE_FMT_S16;
    channels = g_aenc_chns[AencChn].aenc_attr.attr.mp2_attr.u32Channels;
    sample_rate = g_aenc_chns[AencChn].aenc_attr.attr.mp2_attr.u32SampleRate;
    break;
  case RK_CODEC_TYPE_AAC:
    sample_format = RK_SAMPLE_FMT_FLTP;
    channels = g_aenc_chns[AencChn].aenc_attr.attr.aac_attr.u32Channels;
    sample_rate = g_aenc_chns[AencChn].aenc_attr.attr.aac_attr.u32SampleRate;
    break;
  case RK_CODEC_TYPE_G726:
    sample_format = RK_SAMPLE_FMT_S16;
    channels = g_aenc_chns[AencChn].aenc_attr.attr.g726_attr.u32Channels;
    sample_rate = g_aenc_chns[AencChn].aenc_attr.attr.g726_attr.u32SampleRate;
    break;
  default:
    g_aenc_mtx.unlock();
    return -RK_ERR_AENC_CODEC_NOT_SUPPORT;
  }
  PARAM_STRING_APPEND(param, KEY_INPUTDATATYPE,
                      SampleFormatToString(sample_format));

  MediaConfig enc_config;
  SampleInfo aud_info = {(SampleFormat)sample_format, channels, sample_rate,
                         nb_sample};
  auto &ac = enc_config.aud_cfg;
  ac.sample_info = aud_info;
  ac.bit_rate = g_aenc_chns[AencChn].aenc_attr.attr.u32Bitrate;
  enc_config.type = Type::Audio;

  std::string enc_param;
  enc_param.append(
      easymedia::to_param_string(enc_config, CodecToString(codec_type)));
  param = easymedia::JoinFlowParam(param, 1, enc_param);
  g_aenc_chns[AencChn].rkmedia_flow = easymedia::REFLECTOR(
      Flow)::Create<easymedia::Flow>(flow_name.c_str(), param.c_str());

  g_aenc_chns[AencChn].status = CHN_STATUS_OPEN;
  g_aenc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AENC_DestroyChn(AENC_CHN AencChn) {
  if ((AencChn < 0) || (AencChn > AENC_MAX_CHN_NUM))
    return RK_ERR_AENC_INVALID_DEVID;

  g_aenc_mtx.lock();
  if (g_aenc_chns[AencChn].status == CHN_STATUS_BIND) {
    g_aenc_mtx.unlock();
    return -RK_ERR_AENC_BUSY;
  }

  g_aenc_chns[AencChn].rkmedia_flow.reset();
  g_aenc_chns[AencChn].status = CHN_STATUS_CLOSED;
  g_aenc_mtx.unlock();

  return RK_ERR_SYS_OK;
}

/********************************************************************
 * Algorithm::Move Detection api
 ********************************************************************/
RK_S32 RK_MPI_ALGO_MD_SetChnAttr(ALGO_MD_CHN MdChn,
                                 const ALGO_MD_ATTR_S *pstChnAttr) {
  if ((MdChn < 0) || (MdChn > ALGO_MD_MAX_CHN_NUM))
    return -RK_ERR_ALGO_MD_INVALID_CHNID;

  if (!pstChnAttr)
    return -RK_ERR_ALGO_MD_ILLEGAL_PARAM;

  g_algo_md_mtx.lock();
  memcpy(&g_algo_md_chns[MdChn].md_attr, pstChnAttr, sizeof(ALGO_MD_ATTR_S));
  g_algo_md_chns[MdChn].status = CHN_STATUS_READY;
  g_algo_md_mtx.unlock();

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_ALGO_MD_CreateChn(ALGO_MD_CHN MdChn) {
  if ((MdChn < 0) || (MdChn > ALGO_MD_MAX_CHN_NUM))
    return -RK_ERR_ALGO_MD_INVALID_CHNID;

  g_algo_md_mtx.lock();
  if (g_algo_md_chns[MdChn].status != CHN_STATUS_READY) {
    g_algo_md_mtx.unlock();
    return (g_vi_chns[MdChn].status > CHN_STATUS_READY)
               ? -RK_ERR_ALGO_MD_EXIST
               : -RK_ERR_ALGO_MD_NOT_CONFIG;
  }

  ALGO_MD_ATTR_S *pstMDAttr = &g_algo_md_chns[MdChn].md_attr;

  std::string flow_name = "move_detec";
  std::string flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "move_detec");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE,
                      ImageTypeToString(pstMDAttr->imageType));
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, "NULL");
  std::string md_param = "";
  PARAM_STRING_APPEND_TO(md_param, KEY_MD_SINGLE_REF, 1);
  PARAM_STRING_APPEND_TO(md_param, KEY_MD_ORI_WIDTH, pstMDAttr->u32Width);
  PARAM_STRING_APPEND_TO(md_param, KEY_MD_ORI_HEIGHT, pstMDAttr->u32Height);
  PARAM_STRING_APPEND_TO(md_param, KEY_MD_DS_WIDTH, pstMDAttr->u32Width);
  PARAM_STRING_APPEND_TO(md_param, KEY_MD_DS_HEIGHT, pstMDAttr->u32Height);
  PARAM_STRING_APPEND_TO(md_param, KEY_MD_ROI_CNT, pstMDAttr->u16RoiCnt);
  std::string strRects;
  for (int i = 0; i < pstMDAttr->u16RoiCnt; i++) {
    strRects.append("(");
    strRects.append(std::to_string(pstMDAttr->stRoiRects[i].s32X));
    strRects.append(",");
    strRects.append(std::to_string(pstMDAttr->stRoiRects[i].s32Y));
    strRects.append(",");
    strRects.append(std::to_string(pstMDAttr->stRoiRects[i].u32Width));
    strRects.append(",");
    strRects.append(std::to_string(pstMDAttr->stRoiRects[i].u32Height));
    strRects.append(")");
  }
  PARAM_STRING_APPEND(md_param, KEY_MD_ROI_RECT, strRects.c_str());
  flow_param = easymedia::JoinFlowParam(flow_param, 1, md_param);
  LOGD("#MoveDetection flow param:\n%s\n", flow_param.c_str());
  g_algo_md_chns[MdChn].rkmedia_flow = easymedia::REFLECTOR(
      Flow)::Create<easymedia::Flow>(flow_name.c_str(), flow_param.c_str());
  g_algo_md_chns[MdChn].status = CHN_STATUS_OPEN;

  g_algo_md_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_ALGO_MD_DestroyChn(ALGO_MD_CHN MdChn) {
  if ((MdChn < 0) || (MdChn > ALGO_MD_MAX_CHN_NUM))
    return -RK_ERR_ALGO_MD_INVALID_CHNID;

  g_algo_md_mtx.lock();
  if (g_algo_md_chns[MdChn].status == CHN_STATUS_BIND) {
    g_algo_md_mtx.unlock();
    return -1;
  }

  g_algo_md_chns[MdChn].rkmedia_flow.reset();
  g_algo_md_chns[MdChn].status = CHN_STATUS_CLOSED;
  g_algo_md_mtx.unlock();

  return RK_ERR_SYS_OK;
}
