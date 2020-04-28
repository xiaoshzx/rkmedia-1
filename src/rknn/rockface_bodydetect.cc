// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <rockface/rockface.h>


#include "buffer.h"
#include "filter.h"
#include "lock.h"
#include "rknn_utils.h"
#include "rknn_user.h"

namespace easymedia {

#define DEFAULT_LIC_PATH "/userdata/key.lic"

class BodyDetect : public Filter {
public:
  BodyDetect(const char *param);
  virtual ~BodyDetect();
  static const char *GetFilterName() { return "rockface_bodydetect"; }
  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> &output) override;
  virtual int IoCtrl(unsigned long int request, ...) override;

protected:
  bool RoiFilter(rockface_det_t* objects, int width, int height);

private:
  bool send_auth_failure_;
  int interval_;
  ImageRect roi_rect_;
  std::string input_type_;
  rockface_handle_t body_handle_;
  RknnCallBack callback_;
  ReadWriteLockMutex cb_mtx_;
};

BodyDetect::BodyDetect(const char *param)
    : callback_(nullptr) {
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }
  input_type_ = params[KEY_INPUTDATATYPE];
  if (input_type_.empty()) {
    LOG("bodydetect lost input type.\n");
    return;
  }
  const std::string &interval = params[KEY_FRAME_INTERVAL];
  if (!interval.empty())
    interval_ = std::stoi(interval);
  auto &&rects = StringToImageRect(params[KEY_BUFFER_RECT]);
  if (rects.empty()) {
    LOG("missing rects\n");
    SetError(-EINVAL);
    return;
  }
  roi_rect_ = rects[0];

  std::string license_path = DEFAULT_LIC_PATH;
  if (!params[KEY_PATH].empty())
    license_path = params[KEY_PATH];

  rockface_ret_t ret;
  body_handle_ = rockface_create_handle();
  ret = rockface_set_licence(body_handle_, license_path.c_str());
  if (ret != ROCKFACE_RET_SUCCESS)
    LOG("rockface_set_licence failed, ret = %d.\n", ret);

  ret = rockface_init_person_detector(body_handle_);
  if (ret != ROCKFACE_RET_SUCCESS) {
    LOG("rockface_init_person_detector failed, ret = %d\n", ret);
    return;
  }
  send_auth_failure_ = false;
}

BodyDetect::~BodyDetect() {
  if (body_handle_)
    rockface_release_handle(body_handle_);
}

int BodyDetect::Process(std::shared_ptr<MediaBuffer> input,
                        std::shared_ptr<MediaBuffer> &output) {
  static int frame_index = 0;
  if (frame_index++ < interval_)
    return 0;
  if (send_auth_failure_)
    return -1;

  auto input_buffer = std::static_pointer_cast<easymedia::ImageBuffer>(input);
  rockface_image_t input_img;
  input_img.width = input_buffer->GetWidth();
  input_img.height = input_buffer->GetHeight();
  input_img.pixel_format = StrToRockFacePixelFMT(input_type_.c_str());
  input_img.data = (uint8_t *)input_buffer->GetPtr();

  rockface_ret_t ret;
  rockface_det_person_array_t body_array;
  memset(&body_array, 0, sizeof(rockface_det_person_array_t));

  AutoDuration ad;
  ret = rockface_person_detect(body_handle_, &input_img, &body_array);
  if (ret != ROCKFACE_RET_SUCCESS) {
    if (ret == ROCKFACE_RET_AUTH_FAIL) {
      RknnResult result;
      result.type = NNRESULT_TYPE_AUTHORIZED_STATUS;
      result.status = FAILURE;
      callback_(this, NNRESULT_TYPE_AUTHORIZED_STATUS, &result, 1);
      send_auth_failure_ = true;
    }
    LOG("rockface_person_detect failed.\n");
    return 0;
  }
  LOG("rockface_person_detect %lldus\n", ad.Get());

  RknnResult result_item;
  memset(&result_item, 0, sizeof(RknnResult));
  result_item.type = NNRESULT_TYPE_BODY;
  auto &nn_result = input_buffer->GetRknnResult();

  int count = body_array.count;
  RknnResult result[count];
  for (int i = 0; i < count; i++) {
    rockface_det_t *body = &body_array.person[i];
    if (!RoiFilter(body, input_img.width, input_img.height))
      continue;

    result[i].img_w = input_img.width;
    result[i].img_h = input_img.height;
    LOG("body[%d], position:[%d, %d, %d, %d]\n", i,
        body->box.left, body->box.top,
        body->box.right, body->box.bottom);

    memcpy(&result[i].face_info.base, body, sizeof(rockface_det_t));
    memcpy(&result_item.body_info.base, body, sizeof(rockface_det_t));
    nn_result.push_back(result_item);
  }

  if (count > 0 && callback_) {
    AutoLockMutex _rw_mtx(cb_mtx_);
    callback_(this, NNRESULT_TYPE_BODY, result, count);
  }
  frame_index = 0;
  output = input;
  return 0;
}

int BodyDetect::IoCtrl(unsigned long int request, ...) {
  va_list vl;
  va_start(vl, request);
  void *arg = va_arg(vl, void *);
  va_end(vl);

  if (!arg)
    return -1;

  int ret = 0;
  switch (request) {
  case S_NN_CALLBACK: {
    AutoLockMutex _rw_mtx(cb_mtx_);
    callback_ = (RknnCallBack)arg;
  } break;
  case G_NN_CALLBACK: {
    AutoLockMutex _rw_mtx(cb_mtx_);
    arg = (void *)callback_;
  } break;
  default:
    ret = -1;
    break;
  }
  return ret;
}

bool BodyDetect::RoiFilter(rockface_det_t* body, int width, int height) {
  if (!body)
    return false;

  int left = body->box.left;
  int top = body->box.top;
  int right = body->box.right;
  int bottom = body->box.bottom;

  float factor_w = 0.2;
  float factor_h = 0.1;
  float threshold_w = (right - left) * factor_w;
  float threshold_h = (bottom - top) * factor_h;

  int roi_left = roi_rect_.x - threshold_w;
  int roi_top = roi_rect_.y  - threshold_h;
  int roi_right = roi_rect_.x + roi_rect_.w + threshold_w;
  int roi_bottom = roi_rect_.y + roi_rect_.h + threshold_h;

  roi_left = (roi_left > 0) ? roi_left : 0;
  roi_top = (roi_top > 0) ? roi_top : 0;
  roi_right = (roi_right > width) ? width : roi_right;
  roi_bottom = (roi_bottom > height) ? height : roi_bottom;

  return (roi_left < left && right < roi_right &&
          roi_top < top && bottom < roi_bottom) ? true : false;
}

DEFINE_COMMON_FILTER_FACTORY(BodyDetect)
const char *FACTORY(BodyDetect)::ExpectedInputDataType() {
  return TYPE_ANYTHING;
}
const char *FACTORY(BodyDetect)::OutPutDataType() { return TYPE_ANYTHING; }

} // namespace easymedia
