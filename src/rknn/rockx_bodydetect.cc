// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <rockx/rockx.h>

#include "buffer.h"
#include "filter.h"

namespace easymedia {

static const struct PixelFmtEntry {
  rockx_pixel_format fmt;
  const char *fmt_str;
} pixel_fmt_string_map[] = {
  {ROCKX_PIXEL_FORMAT_GRAY8, "image:gray8"},
  {ROCKX_PIXEL_FORMAT_RGB888, IMAGE_RGB888},
  {ROCKX_PIXEL_FORMAT_BGR888, IMAGE_BGR888},
  {ROCKX_PIXEL_FORMAT_RGBA8888, IMAGE_ARGB8888},
  {ROCKX_PIXEL_FORMAT_BGRA8888, IMAGE_ABGR8888},
  {ROCKX_PIXEL_FORMAT_YUV420P_YU12, IMAGE_YUV420P},
  {ROCKX_PIXEL_FORMAT_YUV420P_YV12, "image:yv12"},
  {ROCKX_PIXEL_FORMAT_YUV420SP_NV12, IMAGE_NV12},
  {ROCKX_PIXEL_FORMAT_YUV420SP_NV21, IMAGE_NV21},
  {ROCKX_PIXEL_FORMAT_YUV422P_YU16, IMAGE_UYVY422},
  {ROCKX_PIXEL_FORMAT_YUV422P_YV16, "image:yv16"},
  {ROCKX_PIXEL_FORMAT_YUV422SP_NV16, IMAGE_NV16},
  {ROCKX_PIXEL_FORMAT_YUV422SP_NV61, IMAGE_NV61},
  {ROCKX_PIXEL_FORMAT_GRAY16, "image:gray16"}
};

static rockx_pixel_format StrToRockxPixelFMT(const char *fmt_str) {
  FIND_ENTRY_TARGET_BY_STRCMP(fmt_str, pixel_fmt_string_map, fmt_str, fmt)
  return ROCKX_PIXEL_FORMAT_MAX;
}

class BodyDetect : public Filter {
public:
  BodyDetect(const char *param);
  virtual ~BodyDetect();
  static const char *GetFilterName() { return "rockx_bodydetect"; }
  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> &output) override;
  virtual int IoCtrl(unsigned long int request, ...) override;

protected:
  bool RoiFilter(rockx_object_t* objects, int width, int height);

private:
  ImageRect roi_rect_;
  std::string input_type_;
  rockx_handle_t body_handle_;
  RknnCallBack callback_;
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
  auto &&rects = StringToImageRect(params[KEY_BUFFER_RECT]);
  if (rects.empty()) {
    LOG("missing rects\n");
    SetError(-EINVAL);
    return;
  }
  roi_rect_ = rects[0];
  rockx_ret_t ret =
      rockx_create(&body_handle_, ROCKX_MODULE_OBJECT_DETECTION, nullptr, 0);
  if (ret != ROCKX_RET_SUCCESS) {
    LOG("create body handle failed, ret = %d\n", ret);
    return;
  }
}

BodyDetect::~BodyDetect() {
  if (body_handle_)
    rockx_destroy(body_handle_);
}

int BodyDetect::Process(std::shared_ptr<MediaBuffer> input,
                        std::shared_ptr<MediaBuffer> &output) {
  auto input_buffer = std::static_pointer_cast<easymedia::ImageBuffer>(input);

  rockx_image_t input_img;
  input_img.width = input_buffer->GetWidth();
  input_img.height = input_buffer->GetHeight();
  input_img.pixel_format = StrToRockxPixelFMT(input_type_.c_str());
  input_img.data = (uint8_t *)input_buffer->GetPtr();

  rockx_ret_t ret;
  rockx_object_array_t body_array;
  memset(&body_array, 0, sizeof(rockx_object_array_t));

  AutoDuration ad;
  ret = rockx_object_detect(body_handle_, &input_img, &body_array, nullptr);
  if (ret != ROCKX_RET_SUCCESS) {
    LOG("rockx_object_detect failed.\n");
    return -1;
  }
  LOG("rockx_object_detect %lldus\n", ad.Get());

  RknnResult result_item;
  memset(&result_item, 0, sizeof(RknnResult));
  result_item.type = NNRESULT_TYPE_BODY;
  auto &nn_result = input_buffer->GetRknnResult();

  for (int i = 0; i < body_array.count; i++) {
    rockx_object_t *object = &body_array.object[i];
    if (!RoiFilter(object, input_img.width, input_img.height))
      continue;

    LOG("body[%d], position:[%d, %d, %d, %d]\n", i,
        object->box.left, object->box.top,
        object->box.right, object->box.bottom);

    memcpy(&result_item.body_info.object, object, sizeof(rockx_object_t));
    nn_result.push_back(result_item);

    if (callback_)
      callback_(this, NNRESULT_TYPE_FACE, object, sizeof(rockx_object_t));
  }
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
  case S_CALLBACK_HANDLER:
    callback_ = (RknnCallBack)arg;
    break;
  case G_CALLBACK_HANDLER:
    arg = (void *)callback_;
    break;
  default:
    ret = -1;
    break;
  }
  return ret;
}

bool BodyDetect::RoiFilter(rockx_object_t* object, int width, int height) {
  // the cls_ids is 1 for the human body.
  if (object->cls_idx != 1)
    return false;

  int left = object->box.left;
  int top = object->box.top;
  int right = object->box.right;
  int bottom = object->box.bottom;

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
