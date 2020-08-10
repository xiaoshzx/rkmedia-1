// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <condition_variable>
#include <mutex>
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
#include "rkmedia_buffer.h"
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

typedef struct _RkmediaVencAttr { VENC_CHN_ATTR_S attr; } RkmediaVencAttr;

typedef struct _RkmediaVIAttr {
  const char *path;
  VI_CHN_ATTR_S attr;
} RkmediaVIAttr;

typedef struct _RkmediaAIAttr { AI_CHN_ATTR_S attr; } RkmediaAIAttr;

typedef struct _RkmediaAOAttr { AO_CHN_ATTR_S attr; } RkmediaAOAttr;

typedef struct _RkmediaAENCAttr { AENC_CHN_ATTR_S attr; } RkmediaAENCAttr;

typedef ALGO_MD_ATTR_S RkmediaMDAttr;

#define RKMEDIA_CHNNAL_BUFFER_LIMIT 3

typedef struct _RkmediaChannel {
  MOD_ID_E mode_id;
  RK_U16 chn_id;
  CHN_STATUS status;
  std::shared_ptr<easymedia::Flow> rkmedia_flow;
  // Some functions need a pipeline to complete,
  // the first Flow is placed in rkmedia_flow,
  // and other flows are placed in rkmedia_flow_list for management.
  // For example:
  // vi flow --> venc flow --> file save flow
  // rkmedia_flow : vi flow
  // rkmedia_flow_list : venc flow : file save flow.
  std::list<std::shared_ptr<easymedia::Flow>> rkmedia_flow_list;
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
  RK_U16 bind_ref;
  std::mutex buffer_mtx;
  std::condition_variable buffer_cond;
  std::list<MEDIA_BUFFER> buffer_list;

  // used for region luma.
  std::mutex luma_buf_mtx;
  std::condition_variable luma_buf_cond;
  std::shared_ptr<easymedia::MediaBuffer> luma_rkmedia_buf;
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

RkmediaChannel g_rga_chns[RGA_MAX_CHN_NUM];
std::mutex g_rga_mtx;

static int RkmediaChnPushBuffer(RkmediaChannel *ptrChn, MEDIA_BUFFER buffer) {
  if (!ptrChn || !buffer)
    return -1;

  ptrChn->buffer_mtx.lock();
  if (ptrChn->buffer_list.size() >= RKMEDIA_CHNNAL_BUFFER_LIMIT) {
    LOG("WARN: Mode[%d]:Chn[%d] drop buffer, Please get buffer in time!\n",
        ptrChn->mode_id, ptrChn->chn_id);
    MEDIA_BUFFER mb = ptrChn->buffer_list.front();
    ptrChn->buffer_list.pop_front();
    RK_MPI_MB_ReleaseBuffer(mb);
  }
  ptrChn->buffer_list.push_back(buffer);
  ptrChn->buffer_cond.notify_all();
  ptrChn->buffer_mtx.unlock();
  easymedia::msleep(3);

  return 0;
}

static MEDIA_BUFFER RkmediaChnPopBuffer(RkmediaChannel *ptrChn,
                                        RK_S32 s32MilliSec) {
  if (!ptrChn)
    return NULL;

  std::unique_lock<std::mutex> lck(ptrChn->buffer_mtx);
  if (ptrChn->buffer_list.empty()) {
    if (s32MilliSec < 0) {
      ptrChn->buffer_cond.wait(lck);
    } else if (s32MilliSec > 0) {
      if (ptrChn->buffer_cond.wait_for(
              lck, std::chrono::milliseconds(s32MilliSec)) ==
          std::cv_status::timeout) {
        LOG("INFO: %s: Mode[%d]:Chn[%d] get mediabuffer timeout!\n", __func__,
            ptrChn->mode_id, ptrChn->chn_id);
        return NULL;
      }
    } else {
      return NULL;
    }
  }

  MEDIA_BUFFER mb = NULL;
  if (!ptrChn->buffer_list.empty()) {
    mb = ptrChn->buffer_list.front();
    ptrChn->buffer_list.pop_front();
  }

  return mb;
}

static void RkmediaChnClearBuffer(RkmediaChannel *ptrChn) {
  if (!ptrChn)
    return;

  LOGD("#%p Mode[%d]:Chn[%d] clear media buffer start...\n", ptrChn,
       ptrChn->mode_id, ptrChn->chn_id);
  MEDIA_BUFFER mb = NULL;
  ptrChn->buffer_mtx.lock();
  while (!ptrChn->buffer_list.empty()) {
    mb = ptrChn->buffer_list.front();
    ptrChn->buffer_list.pop_front();
    RK_MPI_MB_ReleaseBuffer(mb);
  }
  ptrChn->buffer_cond.notify_all();
  ptrChn->buffer_mtx.unlock();
  LOGD("#%p Mode[%d]:Chn[%d] clear media buffer end...\n", ptrChn,
       ptrChn->mode_id, ptrChn->chn_id);
}

/********************************************************************
 * SYS Ctrl api
 ********************************************************************/
static void Reset_Channel_Table(RkmediaChannel *tbl, int cnt, MOD_ID_E mid) {
  for (int i = 0; i < cnt; i++) {
    tbl[i].mode_id = mid;
    tbl[i].chn_id = i;
    tbl[i].status = CHN_STATUS_CLOSED;
    tbl[i].cb = nullptr;
    tbl[i].event_cb = nullptr;
    tbl[i].bind_ref = 0;
  }
}

RK_S32 RK_MPI_SYS_Init() {
  LOG_INIT();

  // memset(g_vi_dev, 0, VI_MAX_DEV_NUM * sizeof(RkmediaVideoDev));
  g_vi_chns[0].vi_attr.path = "rkispp_m_bypass"; // rkispp_bypass
  g_vi_chns[1].vi_attr.path = "rkispp_scale0";   // rkispp_scal0
  g_vi_chns[2].vi_attr.path = "rkispp_scale1";   // rkispp_scal1
  g_vi_chns[3].vi_attr.path = "rkispp_scale2";   // rkispp_scal2

  Reset_Channel_Table(g_vi_chns, VI_MAX_CHN_NUM, RK_ID_VI);
  Reset_Channel_Table(g_venc_chns, VENC_MAX_CHN_NUM, RK_ID_VENC);
  Reset_Channel_Table(g_ai_chns, AI_MAX_CHN_NUM, RK_ID_AI);
  Reset_Channel_Table(g_aenc_chns, AENC_MAX_CHN_NUM, RK_ID_AENC);
  Reset_Channel_Table(g_ao_chns, AO_MAX_CHN_NUM, RK_ID_AO);
  Reset_Channel_Table(g_algo_md_chns, ALGO_MD_MAX_CHN_NUM, RK_ID_ALGO_MD);
  Reset_Channel_Table(g_rga_chns, RGA_MAX_CHN_NUM, RK_ID_RGA);

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
    src = g_vi_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_vi_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_VENC:
    src = g_venc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_venc_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_AI:
    src = g_ai_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_ai_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_AENC:
    src = g_aenc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_aenc_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_RGA:
    src = g_rga_chns[pstDestChn->s32ChnId].rkmedia_flow;
    src_chn = &g_rga_chns[pstDestChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  if ((src_chn->status < CHN_STATUS_OPEN) || (!src)) {
    LOG("ERROR: %s Src Mode[%d]:Chn[%d] is not ready!\n", __func__,
        pstSrcChn->enModId, pstSrcChn->s32ChnId);
    return -RK_ERR_SYS_NOTREADY;
  }

  switch (pstDestChn->enModId) {
  case RK_ID_VENC:
    sink = g_venc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_venc_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AO:
    sink = g_ao_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_ao_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AENC:
    sink = g_aenc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_aenc_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_ALGO_MD:
    sink = g_algo_md_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_algo_md_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_RGA:
    sink = g_rga_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_rga_chns[pstDestChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  if ((dst_chn->status < CHN_STATUS_OPEN) || (!sink)) {
    LOG("ERROR: %s Dst Mode[%d]:Chn[%d] is not ready!\n", __func__,
        pstDestChn->enModId, pstDestChn->s32ChnId);
    return -RK_ERR_SYS_NOTREADY;
  }

  // Rkmedia flow bind
  src->AddDownFlow(sink, 0, 0);
  // Generally, after the previous Chn is bound to the next stage,
  // FlowOutputCallback will be disabled.Because the VI needs to calculate
  // the brightness, the VI still retains the FlowOutputCallback after
  // binding the lower-level Chn.
  if (src_chn->mode_id != RK_ID_VI)
    src->SetOutputCallBack(NULL, NULL);

  // change status frome OPEN to BIND.
  src_chn->status = CHN_STATUS_BIND;
  src_chn->bind_ref++;
  dst_chn->status = CHN_STATUS_BIND;
  dst_chn->bind_ref++;

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
    src = g_vi_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_vi_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_VENC:
    src = g_venc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_venc_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_AI:
    src = g_ai_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_ai_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_AO:
    src = g_ao_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_ao_chns[pstSrcChn->s32ChnId];
    break;
  case RK_ID_AENC:
    src = g_aenc_chns[pstSrcChn->s32ChnId].rkmedia_flow;
    src_chn = &g_aenc_chns[pstSrcChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  if ((src_chn->status != CHN_STATUS_BIND))
    return -RK_ERR_SYS_NOT_PERM;

  if ((src_chn->bind_ref <= 0) || (!src)) {
    LOG("ERROR: %s Src Mode[%d]:Chn[%d]'s parameter does not match the "
        "status!\n",
        __func__, pstSrcChn->enModId, pstSrcChn->s32ChnId);
    return -RK_ERR_SYS_NOT_PERM;
  }

  switch (pstDestChn->enModId) {
  case RK_ID_VI:
    sink = g_vi_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_vi_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_VENC:
    sink = g_venc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_venc_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AI:
    sink = g_ai_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_ai_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AO:
    sink = g_ao_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_ao_chns[pstDestChn->s32ChnId];
    break;
  case RK_ID_AENC:
    sink = g_aenc_chns[pstDestChn->s32ChnId].rkmedia_flow;
    dst_chn = &g_aenc_chns[pstDestChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  if ((dst_chn->status != CHN_STATUS_BIND))
    return -RK_ERR_SYS_NOT_PERM;

  if ((dst_chn->bind_ref <= 0) || (!sink)) {
    LOG("ERROR: %s Dst Mode[%d]:Chn[%d]'s parameter does not match the "
        "status!\n",
        __func__, pstDestChn->enModId, pstDestChn->s32ChnId);
    return -RK_ERR_SYS_NOT_PERM;
  }

  // Rkmedia flow unbind
  src->RemoveDownFlow(sink);

  src_chn->bind_ref--;
  dst_chn->bind_ref--;
  // change status frome BIND to OPEN.
  if (src_chn->bind_ref <= 0) {
    src_chn->status = CHN_STATUS_OPEN;
    src_chn->bind_ref = 0;
  }
  if (dst_chn->bind_ref == 0) {
    dst_chn->status = CHN_STATUS_OPEN;
    dst_chn->bind_ref = 0;
  }

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

  if (target_chn->mode_id == RK_ID_VI) {
    std::unique_lock<std::mutex> lck(target_chn->luma_buf_mtx);
    target_chn->luma_rkmedia_buf = rkmedia_mb;
    target_chn->luma_buf_cond.notify_all();
    // Generally, after the previous Chn is bound to the next stage,
    // FlowOutputCallback will be disabled. Because the VI needs to
    // calculate the brightness, the VI still retains the FlowOutputCallback
    // after binding the lower-level Chn. It is judged here that if Chn is VI,
    // and the VI status is BIND, return immediately.
    if (target_chn->status == CHN_STATUS_BIND)
      return;
  }

  // RK_MPI_SYS_GetMediaBuffer and output callback function,
  // can only choose one.
  if (target_chn->cb) {
    MEDIA_BUFFER_IMPLE *mb0 = new MEDIA_BUFFER_IMPLE;
    if (!mb0) {
      LOG("ERROR: %s mode[%d]:chn[%d] no space left for new mb!\n", __func__,
          target_chn->mode_id, target_chn->chn_id);
      return;
    }
    mb0->ptr = rkmedia_mb->GetPtr();
    mb0->fd = rkmedia_mb->GetFD();
    mb0->size = rkmedia_mb->GetValidSize();
    mb0->rkmedia_mb = rkmedia_mb;
    mb0->mode_id = target_chn->mode_id;
    mb0->chn_id = target_chn->chn_id;
    mb0->timestamp = (RK_U64)rkmedia_mb->GetUSTimeStamp();
    target_chn->cb(mb0);
  } else {
    MEDIA_BUFFER_IMPLE *mb = new MEDIA_BUFFER_IMPLE;
    if (!mb) {
      LOG("ERROR: %s mode[%d]:chn[%d] no space left for new mb!\n", __func__,
          target_chn->mode_id, target_chn->chn_id);
      return;
    }
    mb->ptr = rkmedia_mb->GetPtr();
    mb->fd = rkmedia_mb->GetFD();
    mb->size = rkmedia_mb->GetValidSize();
    mb->rkmedia_mb = rkmedia_mb;
    mb->mode_id = target_chn->mode_id;
    mb->chn_id = target_chn->chn_id;
    mb->timestamp = (RK_U64)rkmedia_mb->GetUSTimeStamp();
    RkmediaChnPushBuffer(target_chn, mb);
  }
}

RK_S32 RK_MPI_SYS_RegisterOutCb(const MPP_CHN_S *pstChn, OutCbFunc cb) {
  std::shared_ptr<easymedia::Flow> flow;
  RkmediaChannel *target_chn = NULL;

  switch (pstChn->enModId) {
  case RK_ID_VI:
    target_chn = &g_vi_chns[pstChn->s32ChnId];
    break;
  case RK_ID_VENC:
    target_chn = &g_venc_chns[pstChn->s32ChnId];
    break;
  case RK_ID_AI:
    target_chn = &g_ai_chns[pstChn->s32ChnId];
    break;
  case RK_ID_AENC:
    target_chn = &g_aenc_chns[pstChn->s32ChnId];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  if (target_chn->status < CHN_STATUS_OPEN)
    return -RK_ERR_SYS_NOTREADY;

  if (!target_chn->rkmedia_flow_list.empty())
    flow = target_chn->rkmedia_flow_list.back();
  else if (target_chn->rkmedia_flow)
    flow = target_chn->rkmedia_flow;

  if (!flow) {
    LOG("ERROR: <ModeID:%d ChnID:%d> fatal error!"
        "Status does not match the resource\n");
    return -RK_ERR_SYS_NOT_PERM;
  }

  target_chn->cb = cb;
  // flow->SetOutputCallBack(target_chn, FlowOutputCallback);

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

MEDIA_BUFFER RK_MPI_SYS_GetMediaBuffer(MOD_ID_E enModID, RK_S32 s32ChnID,
                                       RK_S32 s32MilliSec) {
  RkmediaChannel *target_chn = NULL;

  switch (enModID) {
  case RK_ID_VI:
    if (s32ChnID < 0 || s32ChnID >= VI_MAX_CHN_NUM) {
      LOG("ERROR: %s invalid VI ChnID[%d]\n", __func__, s32ChnID);
      return NULL;
    }
    target_chn = &g_vi_chns[s32ChnID];
    break;
  case RK_ID_VENC:
    if (s32ChnID < 0 || s32ChnID >= VENC_MAX_CHN_NUM) {
      LOG("ERROR: %s invalid AENC ChnID[%d]\n", __func__, s32ChnID);
      return NULL;
    }
    target_chn = &g_venc_chns[s32ChnID];
    break;
  case RK_ID_AI:
    if (s32ChnID < 0 || s32ChnID >= AI_MAX_CHN_NUM) {
      LOG("ERROR: %s invalid AI ChnID[%d]\n", __func__, s32ChnID);
      return NULL;
    }
    target_chn = &g_ai_chns[s32ChnID];
    break;
  case RK_ID_AENC:
    if (s32ChnID < 0 || s32ChnID > AENC_MAX_CHN_NUM) {
      LOG("ERROR: %s invalid AENC ChnID[%d]\n", __func__, s32ChnID);
      return NULL;
    }
    target_chn = &g_aenc_chns[s32ChnID];
    break;
  case RK_ID_RGA:
    if (s32ChnID < 0 || s32ChnID > RGA_MAX_CHN_NUM) {
      LOG("ERROR: %s invalid RGA ChnID[%d]\n", __func__, s32ChnID);
      return NULL;
    }
    target_chn = &g_rga_chns[s32ChnID];
    break;
  default:
    LOG("ERROR: %s invalid modeID[%d]\n", __func__, enModID);
    return NULL;
  }

  if (target_chn->status < CHN_STATUS_OPEN) {
    LOG("ERROR: %s Mode[%d]:Chn[%d] in status[%d], "
        "this operation is not allowed!\n",
        __func__, enModID, s32ChnID, target_chn->status);
    return NULL;
  }

  return RkmediaChnPopBuffer(target_chn, s32MilliSec);
}

RK_S32 RK_MPI_SYS_SendMediaBuffer(MOD_ID_E enModID, RK_S32 s32ChnID,
                                  MEDIA_BUFFER buffer) {
  RkmediaChannel *target_chn = NULL;

  switch (enModID) {
  case RK_ID_VENC:
    if (s32ChnID < 0 || s32ChnID >= VENC_MAX_CHN_NUM)
      return -RK_ERR_SYS_ILLEGAL_PARAM;
    target_chn = &g_venc_chns[s32ChnID];
    break;
  case RK_ID_AENC:
    if (s32ChnID < 0 || s32ChnID > AENC_MAX_CHN_NUM)
      return -RK_ERR_SYS_ILLEGAL_PARAM;
    target_chn = &g_aenc_chns[s32ChnID];
    break;
  case RK_ID_ALGO_MD:
    if (s32ChnID < 0 || s32ChnID > ALGO_MD_MAX_CHN_NUM)
      return -RK_ERR_SYS_ILLEGAL_PARAM;
    target_chn = &g_algo_md_chns[s32ChnID];
    break;
  default:
    return -RK_ERR_SYS_NOT_SUPPORT;
  }

  MEDIA_BUFFER_IMPLE *mb = (MEDIA_BUFFER_IMPLE *)buffer;
  target_chn->rkmedia_flow->SendInput(mb->rkmedia_mb, 0);

  return RK_ERR_SYS_OK;
}

/********************************************************************
 * Vi api
 ********************************************************************/
RK_S32 RK_MPI_VI_SetChnAttr(VI_PIPE ViPipe, VI_CHN ViChn,
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
  g_vi_chns[ViChn].rkmedia_flow->SetOutputCallBack(&g_vi_chns[ViChn],
                                                   FlowOutputCallback);
  g_vi_chns[ViChn].status = CHN_STATUS_OPEN;

  g_vi_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VI_DisableChn(VI_PIPE ViPipe, VI_CHN ViChn) {
  if ((ViPipe < 0) || (ViChn < 0) || (ViChn > VI_MAX_CHN_NUM))
    return -RK_ERR_SYS_ILLEGAL_PARAM;

  g_vi_mtx.lock();
  if (g_vi_chns[ViChn].status == CHN_STATUS_BIND) {
    g_vi_mtx.unlock();
    return -RK_ERR_SYS_NOT_PERM;
  }

  RkmediaChnClearBuffer(&g_vi_chns[ViChn]);
  g_vi_chns[ViChn].rkmedia_flow.reset();
  g_vi_chns[ViChn].status = CHN_STATUS_CLOSED;
  g_vi_chns[ViChn].luma_buf_mtx.lock();
  g_vi_chns[ViChn].luma_rkmedia_buf.reset();
  g_vi_chns[ViChn].luma_buf_cond.notify_all();
  g_vi_chns[ViChn].luma_buf_mtx.unlock();
  g_vi_mtx.unlock();

  return RK_ERR_SYS_OK;
}

static RK_U64
rkmediaCalculateRegionLuma(std::shared_ptr<easymedia::ImageBuffer> &rkmedia_mb,
                           const RECT_S *ptrRect) {
  RK_U64 sum = 0;
  ImageInfo &imgInfo = rkmedia_mb->GetImageInfo();

  if ((imgInfo.pix_fmt != PIX_FMT_YUV420P) &&
      (imgInfo.pix_fmt != PIX_FMT_NV12) && (imgInfo.pix_fmt != PIX_FMT_NV21) &&
      (imgInfo.pix_fmt != PIX_FMT_YUV422P) &&
      (imgInfo.pix_fmt != PIX_FMT_NV16) && (imgInfo.pix_fmt != PIX_FMT_NV61)) {
    LOG("ERROR: %s not support image type!\n", __func__);
    return 0;
  }

  if (((RK_S32)(ptrRect->s32X + ptrRect->u32Width) > imgInfo.width) ||
      ((RK_S32)(ptrRect->s32Y + ptrRect->u32Height) > imgInfo.height)) {
    LOG("ERROR: %s rect[%d,%d,%u,%u] out of image wxh[%d, %d]\n", __func__,
        ptrRect->s32X, ptrRect->s32Y, ptrRect->u32Width, ptrRect->u32Height,
        imgInfo.width, imgInfo.height);
    return 0;
  }

  RK_U32 line_size = imgInfo.vir_width;
  RK_U8 *rect_start =
      (RK_U8 *)rkmedia_mb->GetPtr() + ptrRect->s32Y * line_size + ptrRect->s32X;
  for (RK_U32 i = 0; i < ptrRect->u32Height; i++) {
    RK_U8 *line_start = rect_start + i * line_size;
    for (RK_U32 j = 0; j < ptrRect->u32Width; j++) {
      sum += *(line_start + j);
    }
  }

  return sum;
}

RK_S32 RK_MPI_VI_GetChnRegionLuma(VI_PIPE ViPipe, VI_CHN ViChn,
                                  const VIDEO_REGION_INFO_S *pstRegionInfo,
                                  RK_U64 *pu64LumaData, RK_S32 s32MilliSec) {
  if ((ViPipe < 0) || (ViChn < 0) || (ViChn > VI_MAX_CHN_NUM))
    return -RK_ERR_VI_INVALID_CHNID;

  if (!pstRegionInfo || !pstRegionInfo->u32RegionNum || !pu64LumaData)
    return -RK_ERR_VI_ILLEGAL_PARAM;

  std::shared_ptr<easymedia::ImageBuffer> rkmedia_mb;
  RkmediaChannel *target_chn = &g_vi_chns[ViChn];

  if (target_chn->status < CHN_STATUS_OPEN)
    return -RK_ERR_VI_NOTREADY;

  {
    // The {} here is to limit the scope of locking. The lock is only
    // used to find the buffer, and the accumulation of the buffer is
    // outside the lock range. This is good for frame rate.
    std::unique_lock<std::mutex> lck(target_chn->luma_buf_mtx);
    if (!target_chn->luma_rkmedia_buf) {
      if (s32MilliSec < 0) {
        target_chn->luma_buf_cond.wait(lck);
      } else if (s32MilliSec > 0) {
        if (target_chn->luma_buf_cond.wait_for(
                lck, std::chrono::milliseconds(s32MilliSec)) ==
            std::cv_status::timeout)
          return -RK_ERR_VI_TIMEOUT;
      } else {
        return -RK_ERR_VI_BUF_EMPTY;
      }
    }
    if (target_chn->luma_rkmedia_buf)
      rkmedia_mb = std::static_pointer_cast<easymedia::ImageBuffer>(
          target_chn->luma_rkmedia_buf);

    target_chn->luma_rkmedia_buf.reset();
  }

  if (!rkmedia_mb)
    return -RK_ERR_VI_BUF_EMPTY;

  for (RK_U32 i = 0; i < pstRegionInfo->u32RegionNum; i++)
    *(pu64LumaData + i) =
        rkmediaCalculateRegionLuma(rkmedia_mb, (pstRegionInfo->pstRegion + i));

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VI_StartStream(VI_PIPE ViPipe, VI_CHN ViChn) {
  if ((ViPipe < 0) || (ViChn < 0) || (ViChn > VI_MAX_CHN_NUM))
    return -RK_ERR_VI_INVALID_CHNID;

  g_vi_mtx.lock();
  if (g_vi_chns[ViChn].status < CHN_STATUS_OPEN) {
    g_vi_mtx.unlock();
    return -RK_ERR_VI_BUSY;
  }

  if (!g_vi_chns[ViChn].rkmedia_flow) {
    g_vi_mtx.unlock();
    return -RK_ERR_VI_NOTREADY;
  }

  g_vi_chns[ViChn].rkmedia_flow->StartStream();
  g_vi_mtx.unlock();

  return RK_ERR_SYS_OK;
}

/********************************************************************
 * Venc api
 ********************************************************************/
static RK_S32 RkmediaCreateJpegSnapPipeline(RkmediaChannel *VenChn) {
  std::shared_ptr<easymedia::Flow> video_encoder_flow;
  std::shared_ptr<easymedia::Flow> video_decoder_flow;
  std::shared_ptr<easymedia::Flow> video_jpeg_flow;

  VENC_CHN_ATTR_S *stVencChnAttr = &VenChn->venc_attr.attr;
  VENC_ROTATION_E enRotation = stVencChnAttr->stVencAttr.enRotation;
  RK_S32 bps = 2000000;
  RK_S32 video_width = stVencChnAttr->stVencAttr.u32PicWidth;
  RK_S32 video_height = stVencChnAttr->stVencAttr.u32PicHeight;
  RK_S32 vir_width = stVencChnAttr->stVencAttr.u32VirWidth;
  RK_S32 vir_height = stVencChnAttr->stVencAttr.u32VirHeight;
  std::string pixel_format =
      ImageTypeToString(stVencChnAttr->stVencAttr.imageType);
  std::string flow_name = "video_enc";
  std::string flow_param = "";
  std::string enc_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "rkmpp");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, pixel_format);
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, VIDEO_H265);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_WIDTH, video_width);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_HEIGHT, video_height);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_VIR_WIDTH, vir_width);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_VIR_HEIGHT, vir_height);
  PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE, bps);
  PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE_MAX, bps);
  PARAM_STRING_APPEND_TO(enc_param, KEY_COMPRESS_BITRATE_MIN, bps);
  PARAM_STRING_APPEND(enc_param, KEY_VIDEO_GOP, "1");
  // set output fps
  std::string str_fps;
  RK_U32 u32FpsNum = stVencChnAttr->stRcAttr.stMjpegCbr.u32SrcFrameRateNum;
  RK_U32 u32FpsDen = stVencChnAttr->stRcAttr.stMjpegCbr.u32SrcFrameRateDen;
  if (!u32FpsNum)
    u32FpsNum = u32FpsDen = 1;
  str_fps.append(std::to_string(u32FpsNum))
      .append("/")
      .append(std::to_string(u32FpsDen));
  PARAM_STRING_APPEND(enc_param, KEY_FPS_IN, str_fps);
  // set input fps
  str_fps = "";
  u32FpsNum = stVencChnAttr->stRcAttr.stMjpegCbr.fr32DstFrameRateNum;
  u32FpsDen = stVencChnAttr->stRcAttr.stMjpegCbr.fr32DstFrameRateDen;
  if (!u32FpsNum)
    u32FpsNum = u32FpsDen = 1;
  str_fps.append(std::to_string(u32FpsNum))
      .append("/")
      .append(std::to_string(u32FpsDen));
  PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fps);
  // jpeg pre encoder work in fixqp mode
  PARAM_STRING_APPEND(enc_param, KEY_COMPRESS_RC_MODE, KEY_FIXQP);
  PARAM_STRING_APPEND(enc_param, KEY_COMPRESS_QP_INIT, "20");
  PARAM_STRING_APPEND_TO(enc_param, KEY_ROTATION, enRotation);

  flow_param = easymedia::JoinFlowParam(flow_param, 1, enc_param);
  printf("\n#VideoEncoder flow param:\n%s\n", flow_param.c_str());
  video_encoder_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!video_encoder_flow) {
    LOG("ERROR: [%s]: Create flow %s failed\n", __func__, flow_name.c_str());
    return -RK_ERR_VENC_ILLEGAL_PARAM;
  }

  if (pixel_format == IMAGE_FBC0)
    pixel_format = IMAGE_NV12;
  else if (pixel_format == IMAGE_FBC2)
    pixel_format = IMAGE_NV16;

  flow_name = "video_dec";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "rkmpp");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, VIDEO_H265);
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, pixel_format);
  std::string dec_param = "";
  PARAM_STRING_APPEND(dec_param, KEY_INPUTDATATYPE, VIDEO_H265);
  PARAM_STRING_APPEND_TO(dec_param, KEY_MPP_SPLIT_MODE, 0);
  PARAM_STRING_APPEND_TO(dec_param, KEY_OUTPUT_TIMEOUT, -1);

  flow_param = easymedia::JoinFlowParam(flow_param, 1, dec_param);
  printf("\n#VideoDecoder flow param:\n%s\n", flow_param.c_str());
  video_decoder_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!video_decoder_flow) {
    LOG("ERROR: [%s]: Create flow %s failed\n", __func__, flow_name.c_str());
    return -RK_ERR_VENC_ILLEGAL_PARAM;
  }

  flow_name = "video_enc";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "rkmpp");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, pixel_format);
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, IMAGE_JPEG);

  RK_S32 jpeg_width = video_width;
  RK_S32 jpeg_heigth = video_height;
  if ((enRotation == VENC_ROTATION_90) || (enRotation == VENC_ROTATION_270)) {
    jpeg_width = video_height;
    jpeg_heigth = video_width;
  }

  enc_param = "";
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_WIDTH, jpeg_width);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_HEIGHT, jpeg_heigth);

  RK_S32 new_width = UPALIGNTO(jpeg_width, 256);
  if (((new_width / 256) % 2) == 0)
    new_width += 256;
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_VIR_WIDTH, new_width);
  PARAM_STRING_APPEND_TO(enc_param, KEY_BUFFER_VIR_HEIGHT,
                         UPALIGNTO(jpeg_heigth, 8));

  flow_param = easymedia::JoinFlowParam(flow_param, 1, enc_param);
  printf("\n#VideoEncoder:JPEG: flow param:\n%s\n", flow_param.c_str());
  video_jpeg_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!video_jpeg_flow) {
    LOG("ERROR: [%s]: Create flow %s failed\n", __func__, flow_name.c_str());
    return -RK_ERR_VENC_ILLEGAL_PARAM;
  }

#if DEBUG_JPEG_SAVE_H265
  std::shared_ptr<easymedia::Flow> video_save_flow;
  flow_name = "file_write_flow";
  flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_PATH, "/tmp/jpeg.h265");
  PARAM_STRING_APPEND(flow_param, KEY_OPEN_MODE, "w+");
  printf("\n#FileWrite:\n%s\n", flow_param.c_str());
  video_save_flow = easymedia::REFLECTOR(Flow)::Create<easymedia::Flow>(
      flow_name.c_str(), flow_param.c_str());
  if (!video_save_flow) {
    fprintf(stderr, "Create flow %s failed\n", flow_name.c_str());
    exit(EXIT_FAILURE);
  }
  video_encoder_flow->AddDownFlow(video_save_flow, 0, 0);
#endif // DEBUG_JPEG_SAVE_H265

  video_encoder_flow->SetFlowTag("JpegPreEncoder");
  video_decoder_flow->SetFlowTag("JpegPreDecoder");
  video_jpeg_flow->SetFlowTag("JpegEncoder");

  // rkmedia flow bind.
  video_decoder_flow->AddDownFlow(video_jpeg_flow, 0, 0);
  video_encoder_flow->AddDownFlow(video_decoder_flow, 0, 0);
  video_jpeg_flow->SetOutputCallBack(VenChn, FlowOutputCallback);

  VenChn->rkmedia_flow = video_encoder_flow;
  VenChn->rkmedia_flow_list.push_back(video_decoder_flow);
  VenChn->rkmedia_flow_list.push_back(video_jpeg_flow);

  VenChn->status = CHN_STATUS_OPEN;

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_CreateChn(VENC_CHN VeChn, VENC_CHN_ATTR_S *stVencChnAttr) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;

  if (!stVencChnAttr)
    return -RK_ERR_VENC_NULL_PTR;

  g_venc_mtx.lock();
  if (g_venc_chns[VeChn].status != CHN_STATUS_CLOSED) {
    g_venc_mtx.unlock();
    return -RK_ERR_VENC_EXIST;
  }

  if ((stVencChnAttr->stVencAttr.enRotation != VENC_ROTATION_0) &&
      (stVencChnAttr->stVencAttr.enRotation != VENC_ROTATION_90) &&
      (stVencChnAttr->stVencAttr.enRotation != VENC_ROTATION_180) &&
      (stVencChnAttr->stVencAttr.enRotation != VENC_ROTATION_270)) {
    LOG("WARN: Venc[%d]: rotation invalid! use default 0\n", VeChn);
    stVencChnAttr->stVencAttr.enRotation = VENC_ROTATION_0;
  }

  // save venc_attr to venc chn.
  memcpy(&g_venc_chns[VeChn].venc_attr, stVencChnAttr, sizeof(RkmediaVencAttr));

  if (stVencChnAttr->stVencAttr.enType == RK_CODEC_TYPE_JPEG) {
    RK_S32 ret = RkmediaCreateJpegSnapPipeline(&g_venc_chns[VeChn]);
    g_venc_mtx.unlock();
    return ret;
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
  PARAM_STRING_APPEND_TO(enc_param, KEY_ROTATION,
                         stVencChnAttr->stVencAttr.enRotation);
  switch (stVencChnAttr->stVencAttr.enType) {
  case RK_CODEC_TYPE_H264:
    PARAM_STRING_APPEND_TO(enc_param, KEY_PROFILE,
                           stVencChnAttr->stVencAttr.u32Profile);
    break;
  default:
    break;
  }

  std::string str_fps_in, str_fps;
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

    str_fps
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Cbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Cbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fps);
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

    str_fps
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Vbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH264Vbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fps);
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

    str_fps
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Cbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Cbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fps);
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

    str_fps
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Vbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stH265Vbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fps);
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

    str_fps
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stMjpegCbr.fr32DstFrameRateNum))
        .append("/")
        .append(std::to_string(
            stVencChnAttr->stRcAttr.stMjpegCbr.fr32DstFrameRateDen));
    PARAM_STRING_APPEND(enc_param, KEY_FPS, str_fps);
    break;
  default:
    break;
  }

  PARAM_STRING_APPEND_TO(enc_param, KEY_FULL_RANGE, 0);
  // PARAM_STRING_APPEND_TO(enc_param, KEY_REF_FRM_CFG,
  //                       stVencChnAttr->stGopAttr.enGopMode);

  flow_param = easymedia::JoinFlowParam(flow_param, 1, enc_param);
  g_venc_chns[VeChn].rkmedia_flow = easymedia::REFLECTOR(
      Flow)::Create<easymedia::Flow>("video_enc", flow_param.c_str());
  g_venc_chns[VeChn].rkmedia_flow->SetOutputCallBack(&g_venc_chns[VeChn],
                                                     FlowOutputCallback);
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
  switch (g_venc_chns[VeChn].venc_attr.attr.stVencAttr.enType) {
  case RK_CODEC_TYPE_H264:
    qp.qp_step = pstRcParam->stParamH264.u32StepQp;
    qp.qp_max = pstRcParam->stParamH264.u32MaxQp;
    qp.qp_min = pstRcParam->stParamH264.u32MinQp;
    qp.qp_max_i = pstRcParam->stParamH264.u32MaxIQp;
    qp.qp_min_i = pstRcParam->stParamH264.u32MinIQp;
    break;
  case RK_CODEC_TYPE_H265:
    qp.qp_step = pstRcParam->stParamH265.u32StepQp;
    qp.qp_max = pstRcParam->stParamH265.u32MaxQp;
    qp.qp_min = pstRcParam->stParamH265.u32MinQp;
    qp.qp_max_i = pstRcParam->stParamH265.u32MaxIQp;
    qp.qp_min_i = pstRcParam->stParamH265.u32MinIQp;
    break;
  case RK_CODEC_TYPE_JPEG:
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

RK_S32 RK_MPI_VENC_SetRoiAttr(VENC_CHN VeChn, const VENC_ROI_ATTR_S *pstRoiAttr,
                              RK_S32 region_cnt) {

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;
  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;
  if (pstRoiAttr == nullptr && region_cnt > 0)
    return -RK_ERR_VENC_ILLEGAL_PARAM;

  g_venc_mtx.lock();
  EncROIRegion regions[region_cnt];
  for (int i = 0; i < region_cnt; i++) {
    regions[i].x = pstRoiAttr[i].stRect.s32X;
    regions[i].y = pstRoiAttr[i].stRect.s32Y;
    regions[i].w = pstRoiAttr[i].stRect.u32Width;
    regions[i].h = pstRoiAttr[i].stRect.u32Height;

    regions[i].intra = pstRoiAttr[i].bIntra;
    regions[i].area_map_en = pstRoiAttr[i].bEnable;
    regions[i].abs_qp_en = pstRoiAttr[i].bAbsQp;
    regions[i].qp_area_idx = pstRoiAttr[i].u32Index;
    regions[i].quality = pstRoiAttr[i].s32Qp;
  }

  video_encoder_set_roi_regions(g_venc_chns[VeChn].rkmedia_flow, regions,
                                region_cnt);

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

  RkmediaChnClearBuffer(&g_venc_chns[VeChn]);

  if (g_venc_chns[VeChn].rkmedia_flow) {
    if (!g_venc_chns[VeChn].rkmedia_flow_list.empty()) {
      auto ptrRkmediaFlow = g_venc_chns[VeChn].rkmedia_flow_list.front();
      g_venc_chns[VeChn].rkmedia_flow->RemoveDownFlow(ptrRkmediaFlow);
    }
    g_venc_chns[VeChn].rkmedia_flow.reset();
  }

  while (!g_venc_chns[VeChn].rkmedia_flow_list.empty()) {
    auto ptrRkmediaFlow0 = g_venc_chns[VeChn].rkmedia_flow_list.front();
    g_venc_chns[VeChn].rkmedia_flow_list.pop_front();
    if (!g_venc_chns[VeChn].rkmedia_flow_list.empty()) {
      auto ptrRkmediaFlow1 = g_venc_chns[VeChn].rkmedia_flow_list.front();
      ptrRkmediaFlow0->RemoveDownFlow(ptrRkmediaFlow1);
    }
    ptrRkmediaFlow0.reset();
  }

  g_venc_chns[VeChn].status = CHN_STATUS_CLOSED;
  g_venc_mtx.unlock();

  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_SetGopMode(VENC_CHN VeChn, VENC_GOP_ATTR_S *pstGopModeAttr) {
  if (!pstGopModeAttr)
    return -RK_ERR_VENC_ILLEGAL_PARAM;

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;

  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN)
    return -RK_ERR_VENC_NOTREADY;

  EncGopModeParam rkmedia_param;
  memset(&rkmedia_param, 0, sizeof(rkmedia_param));
  switch (pstGopModeAttr->enGopMode) {
  case VENC_GOPMODE_NORMALP:
    rkmedia_param.mode = GOP_MODE_NORMALP;
    rkmedia_param.ip_qp_delta = pstGopModeAttr->s32IPQpDelta;
    break;
  case VENC_GOPMODE_TSVC:
    rkmedia_param.mode = GOP_MODE_TSVC3;
    break;
  case VENC_GOPMODE_SMARTP:
    rkmedia_param.mode = GOP_MODE_SMARTP;
    rkmedia_param.ip_qp_delta = pstGopModeAttr->s32IPQpDelta;
    rkmedia_param.interval = pstGopModeAttr->u32BgInterval;
    rkmedia_param.gop_size = pstGopModeAttr->u32GopSize;
    break;
  default:
    LOG("ERROR: %s invalid gop mode(%d)!\n", pstGopModeAttr->enGopMode);
    return -RK_ERR_VENC_ILLEGAL_PARAM;
  }

  g_venc_mtx.lock();
  easymedia::video_encoder_set_gop_mode(g_venc_chns[VeChn].rkmedia_flow,
                                        &rkmedia_param);
  g_venc_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_VENC_RGN_Init(VENC_CHN VeChn) {
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

RK_S32 RK_MPI_VENC_RGN_SetBitMap(VENC_CHN VeChn,
                                 const OSD_REGION_INFO_S *pstRgnInfo,
                                 const BITMAP_S *pstBitmap) {
  RK_U8 *rkmedia_osd_data;
  RK_U32 total_pix_num = 0;
  RK_S32 ret = RK_ERR_SYS_OK;

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;

  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN) {
    LOG("ERROR: Venc[%d] should be opened before set bitmap!\n");
    return -RK_ERR_VENC_NOTREADY;
  }

  if (pstRgnInfo && !pstRgnInfo->u8Enable) {
    OsdRegionData rkmedia_osd_rgn;
    memset(&rkmedia_osd_rgn, 0, sizeof(rkmedia_osd_rgn));
    rkmedia_osd_rgn.region_id = pstRgnInfo->enRegionId;
    rkmedia_osd_rgn.enable = pstRgnInfo->u8Enable;
    ret = easymedia::video_encoder_set_osd_region(
        g_venc_chns[VeChn].rkmedia_flow, &rkmedia_osd_rgn);
    if (ret)
      ret = -RK_ERR_VENC_NOT_PERM;
    return ret;
  }

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
  rkmedia_osd_data = (RK_U8 *)malloc(total_pix_num);
  if (!rkmedia_osd_data) {
    LOG("ERROR: No space left! RgnInfo pixels(%d)\n", total_pix_num);
    return -RK_ERR_VENC_NOMEM;
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
    ret = -RK_ERR_VENC_NOT_SUPPORT;
    break;
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
  ret = easymedia::video_encoder_set_osd_region(g_venc_chns[VeChn].rkmedia_flow,
                                                &rkmedia_osd_rgn);
  if (ret)
    ret = -RK_ERR_VENC_NOT_PERM;

  if (rkmedia_osd_data)
    free(rkmedia_osd_data);

  return ret;
}

RK_S32 RK_MPI_VENC_RGN_SetCover(VENC_CHN VeChn,
                                const OSD_REGION_INFO_S *pstRgnInfo,
                                const COVER_INFO_S *pstCoverInfo) {
  RK_U8 *rkmedia_cover_data;
  RK_U32 total_pix_num = 0;
  RK_S32 ret = RK_ERR_SYS_OK;
  RK_U8 color_id = 0xFF;

  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;

  if (g_venc_chns[VeChn].status < CHN_STATUS_OPEN) {
    LOG("ERROR: Venc[%d] should be opened before set cover!\n");
    return -RK_ERR_VENC_NOTREADY;
  }

  if (pstRgnInfo && !pstRgnInfo->u8Enable) {
    OsdRegionData rkmedia_osd_rgn;
    memset(&rkmedia_osd_rgn, 0, sizeof(rkmedia_osd_rgn));
    rkmedia_osd_rgn.region_id = pstRgnInfo->enRegionId;
    rkmedia_osd_rgn.enable = pstRgnInfo->u8Enable;
    ret = easymedia::video_encoder_set_osd_region(
        g_venc_chns[VeChn].rkmedia_flow, &rkmedia_osd_rgn);
    if (ret)
      ret = -RK_ERR_VENC_NOT_PERM;
    return ret;
  }

  if (!pstCoverInfo)
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
  rkmedia_cover_data = (RK_U8 *)malloc(total_pix_num);
  if (!rkmedia_cover_data) {
    LOG("ERROR: No space left! RgnInfo pixels(%d)\n", total_pix_num);
    return -RK_ERR_VENC_NOMEM;
  }

  RK_U8 value_r, value_g, value_b, value_a;
  switch (pstCoverInfo->enPixelFormat) {
  case PIXEL_FORMAT_ARGB_1555:
    value_a = (RK_U8)((pstCoverInfo->u32Color & 0x00008000) >> 15);
    value_r = (RK_U8)((pstCoverInfo->u32Color & 0x00007C00) >> 10);
    value_g = (RK_U8)((pstCoverInfo->u32Color & 0x000003E0) >> 5);
    value_b = (RK_U8)((pstCoverInfo->u32Color & 0x0000001F));
    break;
  case PIXEL_FORMAT_ARGB_8888:
    value_a = (RK_U8)((pstCoverInfo->u32Color & 0xF0000000) >> 24);
    value_r = (RK_U8)((pstCoverInfo->u32Color & 0x00FF0000) >> 16);
    value_g = (RK_U8)((pstCoverInfo->u32Color & 0x0000FF00) >> 8);
    value_b = (RK_U8)((pstCoverInfo->u32Color & 0x000000FF));
    break;
  default:
    LOG("ERROR: Not support cover pixel format:%d\n",
        pstCoverInfo->enPixelFormat);
    return -RK_ERR_VENC_NOT_SUPPORT;
  }

  // find and fill color
  if (value_a == 0x00)
    color_id = PALETTE_TABLE_LEN - 1;
  else
    color_id = find_color(bgra8888_palette_table, PALETTE_TABLE_LEN, value_r,
                          value_g, value_b);
  memset(rkmedia_cover_data, color_id, total_pix_num);

  OsdRegionData rkmedia_osd_rgn;
  rkmedia_osd_rgn.buffer = rkmedia_cover_data;
  rkmedia_osd_rgn.region_id = pstRgnInfo->enRegionId;
  rkmedia_osd_rgn.pos_x = pstRgnInfo->u32PosX;
  rkmedia_osd_rgn.pos_y = pstRgnInfo->u32PosY;
  rkmedia_osd_rgn.width = pstRgnInfo->u32Width;
  rkmedia_osd_rgn.height = pstRgnInfo->u32Height;
  rkmedia_osd_rgn.inverse = pstRgnInfo->u8Inverse;
  rkmedia_osd_rgn.enable = pstRgnInfo->u8Enable;
  ret = easymedia::video_encoder_set_osd_region(g_venc_chns[VeChn].rkmedia_flow,
                                                &rkmedia_osd_rgn);
  if (ret)
    ret = -RK_ERR_VENC_NOT_PERM;

  if (rkmedia_cover_data)
    free(rkmedia_cover_data);

  return ret;
}

RK_S32 RK_MPI_VENC_StartRecvFrame(VENC_CHN VeChn,
                                  const VENC_RECV_PIC_PARAM_S *pstRecvParam) {
  if ((VeChn < 0) || (VeChn >= VENC_MAX_CHN_NUM))
    return -RK_ERR_VENC_INVALID_CHNID;

  if (!pstRecvParam)
    return -RK_ERR_VENC_ILLEGAL_PARAM;

  if (!g_venc_chns[VeChn].rkmedia_flow)
    return -RK_ERR_VENC_NOTREADY;

  g_venc_chns[VeChn].rkmedia_flow->SetRunTimes(pstRecvParam->s32RecvPicNum);

  return RK_ERR_SYS_OK;
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
  g_ai_chns[AiChn].rkmedia_flow->SetOutputCallBack(&g_ai_chns[AiChn],
                                                   FlowOutputCallback);
  g_ai_chns[AiChn].status = CHN_STATUS_OPEN;
  g_ai_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AI_SetVolume(AI_CHN AiChn, RK_S32 s32Volume) {
  if ((AiChn < 0) || (AiChn > AI_MAX_CHN_NUM))
    return RK_ERR_AI_INVALID_DEVID;
  g_ai_mtx.lock();
  if (g_ai_chns[AiChn].status <= CHN_STATUS_READY) {
    g_ai_mtx.unlock();
    return -RK_ERR_AI_NOTOPEN;
  }
  g_ai_chns[AiChn].rkmedia_flow->Control(easymedia::S_ALSA_VOLUME, &s32Volume);
  g_ai_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_AI_GetVolume(AI_CHN AiChn, RK_S32 *ps32Volume) {
  if ((AiChn < 0) || (AiChn > AI_MAX_CHN_NUM))
    return RK_ERR_AI_INVALID_DEVID;
  g_ai_mtx.lock();
  if (g_ai_chns[AiChn].status <= CHN_STATUS_READY) {
    g_ai_mtx.unlock();
    return -RK_ERR_AI_NOTOPEN;
  }
  g_ai_chns[AiChn].rkmedia_flow->Control(easymedia::G_ALSA_VOLUME, ps32Volume);
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

  RkmediaChnClearBuffer(&g_ai_chns[AiChn]);

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
  g_ao_chns[AoChn].rkmedia_flow->SetOutputCallBack(&g_ao_chns[AoChn],
                                                   FlowOutputCallback);
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
  RkmediaChnClearBuffer(&g_ao_chns[AoChn]);
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

  g_aenc_chns[AencChn].rkmedia_flow->SetOutputCallBack(&g_aenc_chns[AencChn],
                                                       FlowOutputCallback);

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

  RkmediaChnClearBuffer(&g_aenc_chns[AencChn]);

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
    return -RK_ERR_ALGO_MD_BUSY;
  }

  g_algo_md_chns[MdChn].rkmedia_flow.reset();
  g_algo_md_chns[MdChn].status = CHN_STATUS_CLOSED;
  g_algo_md_mtx.unlock();

  return RK_ERR_SYS_OK;
}

/********************************************************************
 * Rga api
 ********************************************************************/
RK_S32 RK_MPI_RGA_CreateChn(RGA_CHN RgaChn, RGA_ATTR_S *pstRgaAttr) {
  if ((RgaChn < 0) || (RgaChn > RGA_MAX_CHN_NUM))
    return -RK_ERR_RGA_INVALID_CHNID;

  if (!pstRgaAttr)
    return -RK_ERR_RGA_ILLEGAL_PARAM;

  RK_U32 u32InX = pstRgaAttr->stImgIn.u32X;
  RK_U32 u32InY = pstRgaAttr->stImgIn.u32Y;
  RK_U32 u32InWidth = pstRgaAttr->stImgIn.u32Width;
  RK_U32 u32InHeight = pstRgaAttr->stImgIn.u32Height;
  //  RK_U32 u32InVirWidth = pstRgaAttr->stImgIn.u32HorStride;
  //  RK_U32 u32InVirHeight = pstRgaAttr->stImgIn.u32VirStride;
  std::string InPixelFmt = ImageTypeToString(pstRgaAttr->stImgIn.imgType);

  RK_U32 u32OutX = pstRgaAttr->stImgOut.u32X;
  RK_U32 u32OutY = pstRgaAttr->stImgOut.u32Y;
  RK_U32 u32OutWidth = pstRgaAttr->stImgOut.u32Width;
  RK_U32 u32OutHeight = pstRgaAttr->stImgOut.u32Height;
  RK_U32 u32OutVirWidth = pstRgaAttr->stImgOut.u32HorStride;
  RK_U32 u32OutVirHeight = pstRgaAttr->stImgOut.u32VirStride;
  std::string OutPixelFmt = ImageTypeToString(pstRgaAttr->stImgOut.imgType);
  RK_BOOL bEnableBp = pstRgaAttr->bEnBufPool;
  RK_U16 u16BufPoolCnt = pstRgaAttr->u16BufPoolCnt;
  RK_U16 u16Rotaion = pstRgaAttr->u16Rotaion;
  if ((u16Rotaion != 0) && (u16Rotaion != 90) && (u16Rotaion != 180) &&
      (u16Rotaion != 270)) {
    LOG("ERROR: %s rotation only support: 0/90/180/270!\n", __func__);
    return -RK_ERR_RGA_ILLEGAL_PARAM;
  }

  g_rga_mtx.lock();
  if (g_rga_chns[RgaChn].status != CHN_STATUS_CLOSED) {
    g_rga_mtx.unlock();
    return -RK_ERR_RGA_EXIST;
  }

  std::string flow_name = "filter";
  std::string flow_param = "";
  PARAM_STRING_APPEND(flow_param, KEY_NAME, "rkrga");
  PARAM_STRING_APPEND(flow_param, KEY_INPUTDATATYPE, InPixelFmt);
  // Set output buffer type.
  PARAM_STRING_APPEND(flow_param, KEY_OUTPUTDATATYPE, OutPixelFmt);
  // Set output buffer size.
  PARAM_STRING_APPEND_TO(flow_param, KEY_BUFFER_WIDTH, u32OutWidth);
  PARAM_STRING_APPEND_TO(flow_param, KEY_BUFFER_HEIGHT, u32OutHeight);
  PARAM_STRING_APPEND_TO(flow_param, KEY_BUFFER_VIR_WIDTH, u32OutVirWidth);
  PARAM_STRING_APPEND_TO(flow_param, KEY_BUFFER_VIR_HEIGHT, u32OutVirHeight);
  // enable buffer pool?
  if (bEnableBp) {
    PARAM_STRING_APPEND(flow_param, KEY_MEM_TYPE, KEY_MEM_HARDWARE);
    PARAM_STRING_APPEND_TO(flow_param, KEY_MEM_CNT, u16BufPoolCnt);
  }

  std::string filter_param = "";
  ImageRect src_rect = {(RK_S32)u32InX, (RK_S32)u32InY, (RK_S32)u32InWidth,
                        (RK_S32)u32InHeight};
  ImageRect dst_rect = {(RK_S32)u32OutX, (RK_S32)u32OutY, (RK_S32)u32OutWidth,
                        (RK_S32)u32OutHeight};
  std::vector<ImageRect> rect_vect;
  rect_vect.push_back(src_rect);
  rect_vect.push_back(dst_rect);
  PARAM_STRING_APPEND(filter_param, KEY_BUFFER_RECT,
                      easymedia::TwoImageRectToString(rect_vect).c_str());
  PARAM_STRING_APPEND_TO(filter_param, KEY_BUFFER_ROTATE, 0);
  flow_param = easymedia::JoinFlowParam(flow_param, 1, filter_param);
  LOG("\n#Rkrga Filter flow param:\n%s\n", flow_param.c_str());
  g_rga_chns[RgaChn].rkmedia_flow = easymedia::REFLECTOR(
      Flow)::Create<easymedia::Flow>(flow_name.c_str(), flow_param.c_str());
  g_rga_chns[RgaChn].rkmedia_flow->SetOutputCallBack(&g_rga_chns[RgaChn],
                                                     FlowOutputCallback);
  g_rga_chns[RgaChn].status = CHN_STATUS_OPEN;

  g_rga_mtx.unlock();
  return RK_ERR_SYS_OK;
}

RK_S32 RK_MPI_RGA_DestroyChn(RGA_CHN RgaChn) {
  if ((RgaChn < 0) || (RgaChn > RGA_MAX_CHN_NUM))
    return -RK_ERR_RGA_INVALID_CHNID;

  g_rga_mtx.lock();
  if (g_rga_chns[RgaChn].status == CHN_STATUS_BIND) {
    g_rga_mtx.unlock();
    return -RK_ERR_RGA_BUSY;
  }

  g_rga_chns[RgaChn].rkmedia_flow.reset();
  g_rga_chns[RgaChn].status = CHN_STATUS_CLOSED;
  g_rga_mtx.unlock();

  return RK_ERR_SYS_OK;
}
