// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <assert.h>
#include <chrono>             // std::chrono::seconds
#include <condition_variable> // std::condition_variable, std::cv_status
#include <math.h>
#include <mutex> // std::mutex, std::unique_lock

#include <occlusion_detect/occlusion_detection.h>

#include "buffer.h"
#include "flow.h"
#include "image.h"
#include "media_reflector.h"
#include "media_type.h"
#include "message.h"

/* Upper limit of the result stored in the list */
#define MD_RESULT_MAX_CNT 10

enum {
  OD_UPDATE_NONE = 0x00,
  OD_UPDATE_ROI_RECTS = 0x01,
  OD_UPDATE_SENSITIVITY = 0x02,
};

namespace easymedia {

bool od_process(Flow *f, MediaBufferVector &input_vector);

class OcclusionDetectionFlow : public Flow {
public:
  OcclusionDetectionFlow(const char *param);
  virtual ~OcclusionDetectionFlow();
  static const char *GetFlowName() { return "occlusion_detec"; }
  int Control(unsigned long int request, ...);

protected:
  OD_ROI_INFO *roi_in;
  int roi_cnt;
  od_ctx detection_ctx;
  int roi_enable;
  int update_mask;
  int init_bg_interval;
  int init_bg_sucess;
  std::vector<ImageRect> new_roi;
  int sensitivity;

private:
  int img_width, img_height;
  friend bool od_process(Flow *f, MediaBufferVector &input_vector);
};

bool od_process(Flow *f, MediaBufferVector &input_vector) {
  OcclusionDetectionFlow *odf = (OcclusionDetectionFlow *)f;
  std::shared_ptr<MediaBuffer> &src = input_vector[0];
  std::shared_ptr<MediaBuffer> dst;
  int info_cnt = 0;
#ifndef NDEBUG
  static struct timeval tv0;
  struct timeval tv1, tv2;
  gettimeofday(&tv1, NULL);
#endif

  if (!src)
    return false;

  if (!odf->roi_in) {
    LOG("ERROR: OD: process invalid arguments\n");
    return false;
  }

  if (odf->update_mask & OD_UPDATE_ROI_RECTS) {
    LOG("OD: Applying new roi rects...\n");
    if (odf->roi_in) {
      LOG("OD: free old roi info.\n");
      free(odf->roi_in);
    }

    for (int i = 0; i < (int)odf->new_roi.size(); i++) {
      LOG("OD: New ROI RECT[%d]:(%d,%d,%d,%d)\n", i, odf->new_roi[i].x,
          odf->new_roi[i].y, odf->new_roi[i].w, odf->new_roi[i].h);
    }
    odf->roi_cnt = (int)odf->new_roi.size();

    odf->roi_in = (OD_ROI_INFO *)malloc(odf->roi_cnt * sizeof(OD_ROI_INFO));
    memset(odf->roi_in, 0, odf->roi_cnt * sizeof(OD_ROI_INFO));
    for (int i = 0; i < odf->roi_cnt; i++) {
      odf->roi_in[i].up_left[0] = odf->new_roi[i].y;                        // y
      odf->roi_in[i].up_left[1] = odf->new_roi[i].x;                        // x
      odf->roi_in[i].down_right[0] = odf->new_roi[i].y + odf->new_roi[i].h; // y
      odf->roi_in[i].down_right[1] = odf->new_roi[i].x + odf->new_roi[i].w; // x
    }
    odf->update_mask &= (~OD_UPDATE_ROI_RECTS);
    odf->new_roi.clear();
  }

  if (odf->update_mask & OD_UPDATE_SENSITIVITY) {
    LOG("OD: Applying new sensitivity(%d)\n", odf->sensitivity);
    if (occlusion_set_sensitivity(odf->detection_ctx, odf->sensitivity))
      LOG("ERROR: OD: update sensitivity(%d) failed!\n", odf->sensitivity);
    odf->update_mask &= (~OD_UPDATE_SENSITIVITY);
  }

  for (int i = 0; i < odf->roi_cnt; i++) {
    odf->roi_in[i].flag = odf->roi_enable ? 1 : 0;
    odf->roi_in[i].occlusion = 0;
  }

  if(occlusion_detection(odf->detection_ctx, src->GetPtr(), odf->roi_in, odf->roi_cnt)) {
    LOG("ERROR: OD: occlusion detection process failed!\n");
    return false;
  }

#ifndef NDEBUG
  gettimeofday(&tv2, NULL);
#endif

  for (int i = 0; i < odf->roi_cnt; i++)
    if (odf->roi_in[i].flag && odf->roi_in[i].occlusion)
      info_cnt++;

  if (!odf->init_bg_sucess) {
    LOGD("OD: Background frame negotiation:%d(should > 30)\n", odf->init_bg_interval);
    if (!info_cnt)
      odf->init_bg_interval++;
    else
      odf->init_bg_interval = 0;

    if (odf->init_bg_interval > 30) {
      odf->init_bg_sucess = 1;
      LOG("OD: Background frame negotiation sucess!\n");
    } else {
      if (!odf->init_bg_interval)
        occlusion_refresh_bg(odf->detection_ctx);
      return true; //not ready yet.
    }
  }

  if (info_cnt) {
    LOGD("[OcclusionDetection]: Detected occlusion in %d areas, Total areas cnt: %d\n",
         info_cnt, odf->roi_cnt);
    {
      EventParamPtr param =
          std::make_shared<EventParam>(MSG_FLOW_EVENT_INFO_OCCLUSIONDETECTION, 0);
      int odevent_size = sizeof(OcclusionDetectEvent);
      OcclusionDetectEvent *odevent = (OcclusionDetectEvent *)malloc(odevent_size);
      if (!odevent) {
        LOG_NO_MEMORY();
        return false;
      }
      OcclusionDetecInfo *odinfo = odevent->data;
      odevent->info_cnt = info_cnt;
      odevent->img_height = odf->img_height;
      odevent->img_width = odf->img_width;
      int info_id = 0;
      for (int i = 0; i < odf->roi_cnt; i++) {
        if (odf->roi_in[i].flag && odf->roi_in[i].occlusion) {
          odinfo[info_id].x = odf->roi_in[i].up_left[1];
          odinfo[info_id].y = odf->roi_in[i].up_left[0];
          odinfo[info_id].w =
              odf->roi_in[i].down_right[1] - odf->roi_in[i].up_left[1];
          odinfo[info_id].h =
              odf->roi_in[i].down_right[0] - odf->roi_in[i].up_left[0];
          odinfo[info_id].occlusion = 1;
          info_id++;
        }
      }
      param->SetParams(odevent, odevent_size);
      odf->NotifyToEventHandler(param, MESSAGE_TYPE_FIFO);
    }

    if (odf->event_callback_) {
      OcclusionDetectEvent odevent;
      OcclusionDetecInfo *odinfo = odevent.data;
      odevent.info_cnt = info_cnt;
      odevent.img_height = odf->img_height;
      odevent.img_width = odf->img_width;
      int info_id = 0;
      for (int i = 0; i < odf->roi_cnt; i++) {
        if (odf->roi_in[i].flag && odf->roi_in[i].occlusion) {
          odinfo[info_id].x = odf->roi_in[i].up_left[1];
          odinfo[info_id].y = odf->roi_in[i].up_left[0];
          odinfo[info_id].w =
              odf->roi_in[i].down_right[1] - odf->roi_in[i].up_left[1];
          odinfo[info_id].h =
              odf->roi_in[i].down_right[0] - odf->roi_in[i].up_left[0];
          odinfo[info_id].occlusion = 1;
          info_id++;
        }
      }
      odf->event_callback_(odf->event_handler2_, &odevent);
    }
  }

#ifndef NDEBUG
  LOGD("[OcclusionDetection]: get info cnt:%02d, process call delta:%ld ms, "
       "elapse %ld ms\n",
       info_cnt,
       tv0.tv_sec ? ((tv1.tv_sec - tv0.tv_sec) * 1000 +
                     (tv1.tv_usec - tv0.tv_usec) / 1000)
                  : 0,
       (tv2.tv_sec - tv1.tv_sec) * 1000 + (tv2.tv_usec - tv1.tv_usec) / 1000);

  tv0.tv_sec = tv1.tv_sec;
  tv0.tv_usec = tv1.tv_usec;
#endif

  return true;
}

OcclusionDetectionFlow::OcclusionDetectionFlow(const char *param) {
  std::list<std::string> separate_list;
  std::map<std::string, std::string> params;
  if (!ParseWrapFlowParams(param, params, separate_list)) {
    LOG("ERROR: OD: flow param error!\n");
    SetError(-EINVAL);
    return;
  }

  std::string key_name = params[KEY_NAME];
  if (key_name != "occlusion_detec") {
    LOG("ERROR: OD: KEY_NAME:%s not match \"occlusion_detec\"!\n");
    SetError(-EINVAL);
    return;
  }

  // check input/output type
  std::string &&rule = gen_datatype_rule(params);
  if (rule.empty()) {
    SetError(-EINVAL);
    return;
  }

  if (!REFLECTOR(Flow)::IsMatch("occlusion_detec", rule.c_str())) {
    LOG("ERROR: Unsupport for occlusion_detec : [%s]\n", rule.c_str());
    SetError(-EINVAL);
    return;
  }

  const std::string &md_param_str = separate_list.back();
  std::map<std::string, std::string> od_params;
  if (!parse_media_param_map(md_param_str.c_str(), od_params)) {
    LOG("ERROR: OD: md param error!\n");
    SetError(-EINVAL);
    return;
  }

  std::string value;
  CHECK_EMPTY_SETERRNO(value, od_params, KEY_OD_WIDTH, 0)
  img_width = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, od_params, KEY_OD_HEIGHT, 0)
  img_height = std::stoi(value);
  CHECK_EMPTY_SETERRNO(value, od_params, KEY_OD_ROI_CNT, 0)
  roi_cnt = std::stoi(value);
  value = od_params[KEY_OD_SENSITIVITY];
  if (value.empty())
    sensitivity = 0;
  else
    sensitivity = std::stoi(value);
  init_bg_interval = 0;
  init_bg_sucess = 0;

  std::vector<ImageRect> rects;
  if (roi_cnt > 0) {
    CHECK_EMPTY_SETERRNO(value, od_params, KEY_OD_ROI_RECT, 0)
    rects = StringToImageRect(value);
    if (rects.empty()) {
      LOG("ERROR: OD: param missing rects\n");
      SetError(-EINVAL);
      return;
    }

    if ((int)rects.size() != roi_cnt) {
      LOG("ERROR: OD: rects cnt != roi cnt.\n");
      SetError(-EINVAL);
      return;
    }
  }

  LOGD("OD: param: sensitivity=%d\n", sensitivity);
  LOGD("OD: param: orignale width=%d\n", img_width);
  LOGD("OD: param: orignale height=%d\n", img_height);
  LOGD("OD: param: roi_cnt=%d\n", roi_cnt);

  roi_in = (OD_ROI_INFO *)malloc(roi_cnt * sizeof(OD_ROI_INFO));
  memset(roi_in, 0, roi_cnt * sizeof(OD_ROI_INFO));
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
  update_mask = OD_UPDATE_NONE;

  detection_ctx = occlusion_detection_init(img_width, img_height);
  if (!detection_ctx) {
    LOGD("ERROR: OD: od ctx init failed!\n");
    SetError(-EINVAL);
    return;
  }

  if ((sensitivity > 0) && (sensitivity <= 100)) {
    if (occlusion_set_sensitivity(detection_ctx, sensitivity))
      LOG("ERROR: OD: cfg sensitivity(%d) failed!\n", sensitivity);
    else
      LOG("OD: init ctx with sensitivity(%d)...\n", sensitivity);
  }

  SlotMap sm;
  sm.input_slots.push_back(0);
  sm.process = od_process;
  sm.thread_model = Model::ASYNCCOMMON;
  sm.mode_when_full = InputMode::DROPFRONT;
  sm.input_maxcachenum.push_back(3);
  if (!InstallSlotMap(sm, "ODFlow", 20)) {
    LOG("Fail to InstallSlotMap for ODFlow\n");
    SetError(-EINVAL);
    return;
  }
  SetFlowTag("ODFlow");
}

OcclusionDetectionFlow::~OcclusionDetectionFlow() {
  AutoPrintLine apl(__func__);
  StopAllThread();

  if (detection_ctx)
    occlusion_detection_deinit(detection_ctx);

  if (roi_in)
    free(roi_in);
}

int OcclusionDetectionFlow::Control(unsigned long int request, ...) {
  int ret = 0;
  va_list ap;
  va_start(ap, request);

  switch (request) {
  case S_OD_ROI_ENABLE: {
    auto value = va_arg(ap, int);
    roi_enable = value ? 1 : 0;
    LOG("OD: %s roi function!\n", roi_enable ? "Enable" : "Disable");
    break;
  }
  case S_OD_ROI_RECTS: {
    ImageRect *new_rects = va_arg(ap, ImageRect *);
    int new_rects_cnt = va_arg(ap, int);
    assert(new_rects && (new_rects_cnt > 0));
    LOG("OD: new roi image rects cnt:%d\n", new_rects_cnt);
    for (int i = 0; i < new_rects_cnt; i++) {
      LOG("OD: ROI RECT[%d]:(%d,%d,%d,%d)\n", i, new_rects[i].x, new_rects[i].y,
          new_rects[i].w, new_rects[i].h);
      new_roi.push_back(std::move(new_rects[i]));
    }

    update_mask |= OD_UPDATE_ROI_RECTS;
    break;
  }
  case S_OD_SENSITIVITY: {
    sensitivity = va_arg(ap, int);
    LOG("OD: new sensitivity=%d!\n", sensitivity);
    break;
  }
  default:
    ret = -1;
    LOG("ERROR: OD: not support type:%d\n", request);
    break;
  }

  va_end(ap);
  return ret;
}

DEFINE_FLOW_FACTORY(OcclusionDetectionFlow, Flow)
// type depends on encoder
const char *FACTORY(OcclusionDetectionFlow)::ExpectedInputDataType() { return ""; }
const char *FACTORY(OcclusionDetectionFlow)::OutPutDataType() { return ""; }

} // namespace easymedia
