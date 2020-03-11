// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <assert.h>
#include <math.h>

#include <move_detect/move_detection.h>

#include "flow.h"
#include "buffer.h"
#include "image.h"
#include "message.h"
#include "media_type.h"
#include "media_reflector.h"
#include "move_detection_flow.h"

/* Upper limit of the result stored in the list */
#define MD_RESULT_MAX_CNT 10

namespace easymedia {

bool md_process(Flow *f, MediaBufferVector &input_vector) {
  MoveDetectionFlow *mdf = (MoveDetectionFlow *)f;
  std::shared_ptr<MediaBuffer> &src = input_vector[0];
  std::shared_ptr<MediaBuffer> dst;
  static INFO_LIST info_list[4096];
  int result_size = 0;
  int result_cnt = 0;
  int info_cnt = 0;
#ifndef NDEBUG
  static struct timeval tv0;
  struct timeval tv1, tv2;
  gettimeofday(&tv1, NULL);
#endif

  if (!mdf->roi_cnt || !mdf->roi_in) {
    LOG("ERROR: MD: process invalid arguments\n");
    return false;
  }

  for (int i = 0; i < mdf->roi_cnt; i++) {
    mdf->roi_in[i].flag = 1;
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
    EventParamPtr param = std::make_shared<EventParam>(MSG_FLOW_EVENT_INFO_MOVEDETECTION, 0);
    int mdevent_size = sizeof(MoveDetectEvent) + info_cnt * sizeof(MoveDetecInfo);
    MoveDetectEvent *mdevent = (MoveDetectEvent * )malloc(mdevent_size);
    if (!mdevent) {
      LOG_NO_MEMORY();
      return false;
    }
    MoveDetecInfo *mdinfo = (MoveDetecInfo *)((char *)mdevent + sizeof(MoveDetectEvent));
    mdevent->info_cnt = info_cnt;
    mdevent->ori_height = mdf->ori_height;
    mdevent->ori_width = mdf->ori_width;
    mdevent->ds_width = mdf->ds_width;
    mdevent->ds_height = mdf->ds_height;
    mdevent->data = mdinfo;
    for (int i = 0; i < mdf->roi_cnt; i++) {
      if (mdf->roi_in[i].flag && mdf->roi_in[i].is_move) {
        mdinfo[i].x = mdf->roi_in[i].up_left[1];
        mdinfo[i].y = mdf->roi_in[i].up_left[0];
        mdinfo[i].w = mdf->roi_in[i].down_right[1];
        mdinfo[i].h = mdf->roi_in[i].down_right[0];
      }
    }
    param->SetParams(mdevent, mdevent_size);
    mdf->NotifyToEventHandler(param, MESSAGE_TYPE_FIFO);
  }

  for (int i = 0; i < 4096; i++) {
    if (!info_list[i].flag)
      break;
    result_cnt++;
    result_size += sizeof(INFO_LIST);
  }

  if (result_size) {
    dst =  MediaBuffer::Alloc(result_size, MediaBuffer::MemType::MEM_HARD_WARE);
    if (!dst) {
      LOG_NO_MEMORY();
      return false;
    }
    memcpy(dst->GetPtr(), info_list, result_size);
  } else {
    dst =  MediaBuffer::Alloc(4, MediaBuffer::MemType::MEM_COMMON);
    if (!dst) {
      LOG_NO_MEMORY();
      return false;
    }
  }

  LOGD("[MoveDetection]: insert list time:%lldms\n", src->GetAtomicClock() / 1000);

  dst->SetValidSize(result_size);
  dst->SetAtomicClock(src->GetAtomicClock());

  mdf->InsertMdResult(dst);

#ifndef NDEBUG
  LOGD("[MoveDetection]: get result cnt:%02d, process call delta:%ld ms, elapse %ld ms\n",
    result_size / sizeof(INFO_LIST),
    tv0.tv_sec?((tv1.tv_sec - tv0.tv_sec) * 1000 + (tv1.tv_usec - tv0.tv_usec) / 1000) : 0,
    (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec) / 1000);

  tv0.tv_sec = tv1.tv_sec;
  tv0.tv_usec = tv1.tv_usec;
#endif

  return true;
}

std::shared_ptr<MediaBuffer> MoveDetectionFlow::
LookForMdResult(int64_t atomic_clock, int approximation) {
  std::shared_ptr<MediaBuffer> right_result = NULL;
  std::shared_ptr<MediaBuffer> last_restult = NULL;
  int clk_delta = 0;
  int clk_delta_min = 0;

  md_results_mtx.read_lock();
  LOGD("#LookForMdResult, target:%.1f\n", atomic_clock / 1000.0);
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
    if (approximation &&
        (!clk_delta_min || (clk_delta <=  clk_delta_min))) {
      clk_delta_min = clk_delta;
      last_restult = tmp;
    }
  }
  md_results_mtx.unlock();

  if (approximation && !right_result && last_restult) {
    LOG("WARN:MD get closest result, deltaTime=%.1fms.\n",
      clk_delta_min / 1000.0);
    right_result = last_restult;
  }

  return right_result;
}

void MoveDetectionFlow::
InsertMdResult(std::shared_ptr<MediaBuffer> &buffer) {
  md_results_mtx.lock();
  while (md_results.size() > MD_RESULT_MAX_CNT)
    md_results.pop_front();

  md_results.push_back(buffer);
  md_results_mtx.unlock();
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
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_ORI_HEIGHT, 0)
  ori_width = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_ORI_WIDTH, 0)
  ori_height = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_DS_WIDTH, 0)
  ds_width = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_DS_HEIGHT, 0)
  ds_height = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_ROI_CNT, 0)
  roi_cnt = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, md_params, KEY_MD_ROI_RECT, 0)
  auto rects = StringToImageRect(value);
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
    LOGD("### ROI RECT[i]:(%d,%d,%d,%d)\n", rects[i].x,
      rects[i].y, rects[i].w, rects[i].h);
    roi_in[i].flag = 1;
    roi_in[i].up_left[0] = rects[i].y; // y
    roi_in[i].up_left[1] = rects[i].x; // x
    roi_in[i].down_right[0] = rects[i].h; // y
    roi_in[i].down_right[1] = rects[i].w; // x
  }

  md_ctx = move_detection_init(ori_width, ori_height,
    ds_width, ds_height, is_single_ref);

  SlotMap sm;
  sm.input_slots.push_back(0);
  sm.process = md_process;
  sm.thread_model = Model::ASYNCCOMMON;
  sm.mode_when_full = InputMode::DROPFRONT;
  sm.input_maxcachenum.push_back(3);
  if (!InstallSlotMap(sm, key_name, 40)) {
    LOG("Fail to InstallSlotMap, md_detection\n");
    SetError(-EINVAL);
    return;
  }
}

MoveDetectionFlow:: ~MoveDetectionFlow() {
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
  va_list ap;
  va_start(ap, request);
  //auto value = va_arg(ap, std::shared_ptr<ParameterBuffer>);
  va_end(ap);
  //assert(value);
  return 0;
}

DEFINE_FLOW_FACTORY(MoveDetectionFlow, Flow)
// type depends on encoder
const char *FACTORY(MoveDetectionFlow)::ExpectedInputDataType() { return ""; }
const char *FACTORY(MoveDetectionFlow)::OutPutDataType() { return ""; }

} // namespace easymedia
