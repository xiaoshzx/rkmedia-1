// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <assert.h>
#include <chrono>             // std::chrono::seconds
#include <condition_variable> // std::condition_variable, std::cv_status
#include <math.h>
#include <mutex> // std::mutex, std::unique_lock

#include <move_detect/move_detection.h>

#include "buffer.h"
#include "flow.h"
#include "image.h"
#include "media_reflector.h"
#include "media_type.h"
#include "message.h"
#include "move_detection_flow.h"

/* Upper limit of the result stored in the list */
#define MD_RESULT_MAX_CNT 10

enum {
  MD_UPDATE_NONE = 0x00,
  MD_UPDATE_SENSITIVITY = 0x01,
  MD_UPDATE_ROI_RECTS = 0x02,
};

namespace easymedia {

bool md_process(Flow *f, MediaBufferVector &input_vector) {
  MoveDetectionFlow *mdf = (MoveDetectionFlow *)f;
  std::shared_ptr<MediaBuffer> &src = input_vector[0];
  std::shared_ptr<MediaBuffer> dst;
  static INFO_LIST info_list[4096];
  int result_size = 0;
  int info_cnt = 0;
#ifndef NDEBUG
  static struct timeval tv0;
  struct timeval tv1, tv2;
  gettimeofday(&tv1, NULL);
#endif

  if (!src)
    return false;

  if (!mdf->roi_in) {
    LOG("ERROR: MD: process invalid arguments\n");
    return false;
  }

  if (mdf->update_mask & MD_UPDATE_SENSITIVITY) {
    LOG("MD: Applying new sensitivity....\n");
    move_detection_set_sensitivity(mdf->md_ctx, mdf->Sensitivity);
    mdf->update_mask &= (~MD_UPDATE_SENSITIVITY);
  } else if (mdf->update_mask & MD_UPDATE_ROI_RECTS) {
    LOG("MD: Applying new roi rects...\n");
    if (mdf->roi_in) {
      LOG("MD: free old roi info.\n");
      free(mdf->roi_in);
    }

    for (int i = 0; i < (int)mdf->new_roi.size(); i++) {
      LOG("MD: New ROI RECT[%d]:(%d,%d,%d,%d)\n", i, mdf->new_roi[i].x,
          mdf->new_roi[i].y, mdf->new_roi[i].w, mdf->new_roi[i].h);
    }
    mdf->roi_cnt = (int)mdf->new_roi.size();
    // We need to create roi_cnt + 1 ROI_IN, and the last one sets
    // the flag to 0 to tell the motion detection interface that
    // this is the end marker.
    mdf->roi_in = (ROI_INFO *)malloc((mdf->roi_cnt + 1) * sizeof(ROI_INFO));
    memset(mdf->roi_in, 0, (mdf->roi_cnt + 1) * sizeof(ROI_INFO));
    for (int i = 0; i < mdf->roi_cnt; i++) {
      mdf->roi_in[i].up_left[0] = mdf->new_roi[i].y;                        // y
      mdf->roi_in[i].up_left[1] = mdf->new_roi[i].x;                        // x
      mdf->roi_in[i].down_right[0] = mdf->new_roi[i].y + mdf->new_roi[i].h; // y
      mdf->roi_in[i].down_right[1] = mdf->new_roi[i].x + mdf->new_roi[i].w; // x
    }
    mdf->update_mask &= (~MD_UPDATE_ROI_RECTS);
    mdf->new_roi.clear();
  }

  for (int i = 0; i < mdf->roi_cnt; i++) {
    mdf->roi_in[i].flag = mdf->roi_enable ? 1 : 0;
    mdf->roi_in[i].is_move = 0;
  }
  mdf->roi_in[mdf->roi_cnt].flag = 0;

  memset(info_list, 0, sizeof(info_list));
  move_detection(mdf->md_ctx, src->GetPtr(), (mdf->roi_in), info_list);
#ifndef NDEBUG
  gettimeofday(&tv2, NULL);
#endif

  for (int i = 0; i < mdf->roi_cnt; i++)
    if (mdf->roi_in[i].flag && mdf->roi_in[i].is_move)
      info_cnt++;

  if (info_cnt) {
    LOGD("[MoveDetection]: Detected movement in %d areas, Total areas cnt: %d\n",
         info_cnt, mdf->roi_cnt);
    {
      EventParamPtr param =
          std::make_shared<EventParam>(MSG_FLOW_EVENT_INFO_MOVEDETECTION, 0);
      int mdevent_size = sizeof(MoveDetectEvent);
      MoveDetectEvent *mdevent = (MoveDetectEvent *)malloc(mdevent_size);
      if (!mdevent) {
        LOG_NO_MEMORY();
        return false;
      }
      MoveDetecInfo *mdinfo = mdevent->data;
      mdevent->info_cnt = info_cnt;
      mdevent->ori_height = mdf->ori_height;
      mdevent->ori_width = mdf->ori_width;
      mdevent->ds_width = mdf->ds_width;
      mdevent->ds_height = mdf->ds_height;
      int info_id = 0;
      for (int i = 0; (i < mdf->roi_cnt) && (info_id < info_cnt); i++) {
        if (mdf->roi_in[i].flag && mdf->roi_in[i].is_move) {
          mdinfo[info_id].x = mdf->roi_in[i].up_left[1];
          mdinfo[info_id].y = mdf->roi_in[i].up_left[0];
          mdinfo[info_id].w =
              mdf->roi_in[i].down_right[1] - mdf->roi_in[i].up_left[1];
          mdinfo[info_id].h =
              mdf->roi_in[i].down_right[0] - mdf->roi_in[i].up_left[0];
          info_id++;
        }
      }
      param->SetParams(mdevent, mdevent_size);
      mdf->NotifyToEventHandler(param, MESSAGE_TYPE_FIFO);
    }

    if (mdf->event_callback_) {
      MoveDetectEvent mdevent;
      MoveDetecInfo *mdinfo = mdevent.data;
      mdevent.info_cnt = info_cnt;
      mdevent.ori_height = mdf->ori_height;
      mdevent.ori_width = mdf->ori_width;
      mdevent.ds_width = mdf->ds_width;
      mdevent.ds_height = mdf->ds_height;
      int info_id = 0;
      for (int i = 0; (i < mdf->roi_cnt) && (info_id < info_cnt); i++) {
        if (mdf->roi_in[i].flag && mdf->roi_in[i].is_move) {
          mdinfo[info_id].x = mdf->roi_in[i].up_left[1];
          mdinfo[info_id].y = mdf->roi_in[i].up_left[0];
          mdinfo[info_id].w =
              mdf->roi_in[i].down_right[1] - mdf->roi_in[i].up_left[1];
          mdinfo[info_id].h =
              mdf->roi_in[i].down_right[0] - mdf->roi_in[i].up_left[0];
          info_id++;
        }
      }
      mdf->event_callback_(mdf->event_handler2_, &mdevent);
    }
  }

  for (int i = 0; i < 4096; i++) {
    if (!info_list[i].flag)
      break;
    result_size += sizeof(INFO_LIST);
  }

  if (result_size) {
    // We need to create result_cnt + 1 INFO_LIST, and the last one sets
    // the flag to 0 to tell the librocchip_mpp.so that this is the end marker.
    result_size += sizeof(INFO_LIST);
    dst = MediaBuffer::Alloc(result_size, MediaBuffer::MemType::MEM_HARD_WARE);
    if (!dst) {
      LOG_NO_MEMORY();
      return false;
    }
    memcpy(dst->GetPtr(), info_list, result_size);
  } else {
    dst = MediaBuffer::Alloc(4, MediaBuffer::MemType::MEM_COMMON);
    if (!dst) {
      LOG_NO_MEMORY();
      return false;
    }
  }

  LOGD("[MoveDetection]: insert list time:%lldms\n",
       src->GetAtomicClock() / 1000);

  dst->SetValidSize(result_size);
  dst->SetAtomicClock(src->GetAtomicClock());

  mdf->InsertMdResult(dst);

#ifndef NDEBUG
  LOGD("[MoveDetection]: get result cnt:%02d, process call delta:%ld ms, "
       "elapse %ld ms\n",
       result_size / sizeof(INFO_LIST),
       tv0.tv_sec ? ((tv1.tv_sec - tv0.tv_sec) * 1000 +
                     (tv1.tv_usec - tv0.tv_usec) / 1000)
                  : 0,
       (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec) / 1000);

  tv0.tv_sec = tv1.tv_sec;
  tv0.tv_usec = tv1.tv_usec;
#endif

  return true;
}

std::shared_ptr<MediaBuffer>
MoveDetectionFlow::LookForMdResult(int64_t atomic_clock, int timeout_us) {
  std::shared_ptr<MediaBuffer> right_result = NULL;
  std::shared_ptr<MediaBuffer> last_restult = NULL;
  int clk_delta = 0;
  int clk_delta_min = 0;
  int64_t start_ts = gettimeofday();
  int left_time = 0;
#ifndef NDBUEG
  AutoDuration ad;
#endif

  LOGD("#LookForMdResult, target:%.1f, timeout:%.1f\n", atomic_clock / 1000.0,
       timeout_us / 1000.0);
  do {
    // Step1: Lookfor mdinfo first.
    md_results_mtx.lock();
    for (auto &tmp : md_results) {
      clk_delta = abs(atomic_clock - tmp->GetAtomicClock());
      LOGD("...Compare vs :%.1f\n", tmp->GetAtomicClock() / 1000.0);
      // The time stamps of images acquired by multiple image
      // acquisition channels at the same time cannot exceed 1 ms.
      if (clk_delta <= 1000) {
        LOGD(">>> MD get right result\n");
        right_result = tmp;
        break;
      }
      // Find the closest value.
      if ((!clk_delta_min || (clk_delta <= clk_delta_min))) {
        clk_delta_min = clk_delta;
        last_restult = tmp;
      }
    }
    md_results_mtx.unlock();

    // Step2: wait for new mdinfo, then lookup again.
    // If no new mdinfo is received within the remaining time,
    // timeout processing: directly use the closest mdinfo as
    // the result.
    left_time = timeout_us - (gettimeofday() - start_ts);
    if ((md_results.size() > 0) && !right_result && (left_time > 0)) {
      std::unique_lock<std::mutex> lck(md_results_mtx);
      if (con_var.wait_for(lck, std::chrono::microseconds(left_time)) ==
          std::cv_status::timeout) {
        LOG("WARN:MD get closest result, deltaTime=%.1fms.\n",
            clk_delta_min / 1000.0);
        right_result = last_restult;
        break; // stop while
      }
#ifndef NDEBUG
      else {
        LOGD("#RetryLookForMdResult target:%.1f\n", atomic_clock / 1000.0);
      }
#endif
    } else
      break; // stop while
  } while (1);

#ifndef NDBUEG
  LOGD("#%s cost:%dms\n", __func__, (int)(ad.Get() / 1000));
#endif

  return right_result;
}

void MoveDetectionFlow::InsertMdResult(std::shared_ptr<MediaBuffer> &buffer) {
  md_results_mtx.lock();
  while (md_results.size() > MD_RESULT_MAX_CNT)
    md_results.pop_front();

  md_results.push_back(buffer);
  md_results_mtx.unlock();
  con_var.notify_all();
}

MoveDetectionFlow::MoveDetectionFlow(const char *param) {
  std::list<std::string> separate_list;
  std::map<std::string, std::string> params;
  if (!ParseWrapFlowParams(param, params, separate_list)) {
    LOG("ERROR: MD: flow param error!\n");
    SetError(-EINVAL);
    return;
  }

  std::string key_name = params[KEY_NAME];
  // check input/output type
  std::string &&rule = gen_datatype_rule(params);
  if (rule.empty()) {
    SetError(-EINVAL);
    return;
  }

  if (!REFLECTOR(Flow)::IsMatch("move_detec", rule.c_str())) {
    LOG("ERROR: Unsupport for move_detec : [%s]\n", rule.c_str());
    SetError(-EINVAL);
    return;
  }

  const std::string &md_param_str = separate_list.back();
  std::map<std::string, std::string> md_params;
  if (!parse_media_param_map(md_param_str.c_str(), md_params)) {
    LOG("ERROR: MD: md param error!\n");
    SetError(-EINVAL);
    return;
  }

  std::string value;
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_SINGLE_REF, 0)
  is_single_ref = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_ORI_WIDTH, 0)
  ori_width = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_ORI_HEIGHT, 0)
  ori_height = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_DS_WIDTH, 0)
  ds_width = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_DS_HEIGHT, 0)
  ds_height = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_ROI_CNT, 0)
  roi_cnt = std::stoi(value);
  value = md_params[KEY_MD_SENSITIVITY];
  if (value.empty())
    Sensitivity = 0;
  else
    Sensitivity = std::stoi(value);

  std::vector<ImageRect> rects;
  if (roi_cnt > 0) {
    CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_ROI_RECT, 0)
    rects = StringToImageRect(value);
    if (rects.empty()) {
      LOG("ERROR: MD: param missing rects\n");
      SetError(-EINVAL);
      return;
    }

    if ((int)rects.size() != roi_cnt) {
      LOG("ERROR: MD: rects cnt != roi cnt.\n");
      SetError(-EINVAL);
      return;
    }
  }

  LOGD("MD: param: sensitivity=%d\n", Sensitivity);
  LOGD("MD: param: is_single_ref=%d\n", is_single_ref);
  LOGD("MD: param: orignale width=%d\n", ori_width);
  LOGD("MD: param: orignale height=%d\n", ori_height);
  LOGD("MD: param: down scale width=%d\n", ds_width);
  LOGD("MD: param: down scale height=%d\n", ds_height);
  LOGD("MD: param: roi_cnt=%d\n", roi_cnt);

  // We need to create roi_cnt + 1 ROI_IN, and the last one sets
  // the flag to 0 to tell the motion detection interface that
  // this is the end marker.
  roi_in = (ROI_INFO *)malloc((roi_cnt + 1) * sizeof(ROI_INFO));
  memset(roi_in, 0, (roi_cnt + 1) * sizeof(ROI_INFO));
  for (int i = 0; i < roi_cnt; i++) {
    LOGD("### ROI RECT[i]:(%d,%d,%d,%d)\n", rects[i].x, rects[i].y, rects[i].w,
         rects[i].h);
    roi_in[i].flag = 1;
    roi_in[i].up_left[0] = rects[i].y;                 // y
    roi_in[i].up_left[1] = rects[i].x;                 // x
    roi_in[i].down_right[0] = rects[i].y + rects[i].h; // y
    roi_in[i].down_right[1] = rects[i].x + rects[i].w; // x
  }

  roi_enable = 1;
  update_mask = MD_UPDATE_NONE;

  md_ctx = move_detection_init(ori_width, ori_height, ds_width, ds_height,
                               is_single_ref);

  if ((Sensitivity > 0) && (Sensitivity <= 100)) {
    if (move_detection_set_sensitivity(md_ctx, Sensitivity))
      LOG("ERROR: MD: cfg sensitivity(%d) failed!\n", Sensitivity);
    else
      LOG("OD: init ctx with sensitivity(%d)...\n", Sensitivity);
  }

  SlotMap sm;
  sm.input_slots.push_back(0);
  sm.process = md_process;
  sm.thread_model = Model::ASYNCCOMMON;
  sm.mode_when_full = InputMode::DROPFRONT;
  sm.input_maxcachenum.push_back(3);
  if (!InstallSlotMap(sm, "MDFlow", 20)) {
    LOG("Fail to InstallSlotMap for MDFlow\n");
    SetError(-EINVAL);
    return;
  }
  SetFlowTag("MDFlow");
}

MoveDetectionFlow::~MoveDetectionFlow() {
  AutoPrintLine apl(__func__);
  StopAllThread();

  if (md_ctx)
    move_detection_deinit(md_ctx);

  if (roi_in)
    free(roi_in);

  std::list<std::shared_ptr<MediaBuffer>>::iterator it;
  for (it = md_results.begin(); it != md_results.end();)
    it = md_results.erase(it);
}

int MoveDetectionFlow::Control(unsigned long int request, ...) {
  int ret = 0;
  va_list ap;
  va_start(ap, request);

  switch (request) {
  case S_MD_ROI_ENABLE: {
    auto value = va_arg(ap, int);
    roi_enable = value ? 1 : 0;
    LOG("MD: %s roi function!\n", roi_enable ? "Enable" : "Disable");
    break;
  }
  case S_MD_SENSITIVITY: {
    Sensitivity = va_arg(ap, int);
    if ((Sensitivity < 1) || (Sensitivity > 100)) {
      LOG("ERROR: MD: invalid sensitivity value!\n");
      break;
    }

    update_mask |= MD_UPDATE_SENSITIVITY;
    break;
  }
  case S_MD_ROI_RECTS: {
    ImageRect *new_rects = va_arg(ap, ImageRect *);
    int new_rects_cnt = va_arg(ap, int);
    assert(new_rects && (new_rects_cnt > 0));
    LOG("MD: new roi image rects cnt:%d\n", new_rects_cnt);
    for (int i = 0; i < new_rects_cnt; i++) {
      LOG("MD: ROI RECT[%d]:(%d,%d,%d,%d)\n", i, new_rects[i].x, new_rects[i].y,
          new_rects[i].w, new_rects[i].h);
      new_roi.push_back(std::move(new_rects[i]));
    }

    update_mask |= MD_UPDATE_ROI_RECTS;
    break;
  }
  default:
    ret = -1;
    LOG("ERROR: MD: not support type:%d\n", request);
    break;
  }

  va_end(ap);
  return ret;
}

DEFINE_FLOW_FACTORY(MoveDetectionFlow, Flow)
// type depends on encoder
const char *FACTORY(MoveDetectionFlow)::ExpectedInputDataType() { return ""; }
const char *FACTORY(MoveDetectionFlow)::OutPutDataType() { return ""; }

} // namespace easymedia
