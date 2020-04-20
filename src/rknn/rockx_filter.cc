// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer.h"
#include "utils.h"
#include "filter.h"
#include "rknn_utils.h"
#include <rockx/rockx.h>
#include <assert.h>

namespace easymedia {

class ROCKXFilter : public Filter {
public:
  ROCKXFilter(const char *param);
  virtual ~ROCKXFilter();
  static const char *GetFilterName() { return "rockx_filter"; }
  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> &output) override;
  virtual int IoCtrl(unsigned long int request, ...) override;

private:
  std::string model_name_;
  std::vector<rockx_handle_t> rockx_handles_;
  std::shared_ptr<easymedia::MediaBuffer> tmp_image_;
  std::string input_type_;
  RknnCallBack callback_;
};

ROCKXFilter::ROCKXFilter(const char *param)
  : model_name_(""){
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }

  if (params[KEY_ROCKX_MODEL].empty()) {
    LOG("lost rockx model info!\n");
    return;
  } else {
    model_name_ = params[KEY_ROCKX_MODEL];
  }

  if (params[KEY_INPUTDATATYPE].empty()) {
    LOG("rockx lost input type.\n");
    return;
  } else {
    input_type_ = params[KEY_INPUTDATATYPE];
  }

  std::vector<rockx_module_t> models;
  void *config = nullptr;
  size_t config_size = 0;
  if (model_name_ == "rockx_face_gender_age") {
    models.push_back(ROCKX_MODULE_FACE_DETECTION);
    models.push_back(ROCKX_MODULE_FACE_LANDMARK_5);
    models.push_back(ROCKX_MODULE_FACE_ANALYZE);
  } else if (model_name_ == "rockx_face_detect") {
    models.push_back(ROCKX_MODULE_FACE_DETECTION);
  } else if (model_name_ == "rockx_face_landmark"){
    models.push_back(ROCKX_MODULE_FACE_DETECTION);
    models.push_back(ROCKX_MODULE_FACE_LANDMARK_68);
  } else if (model_name_ == "rockx_pose_body"){
    models.push_back(ROCKX_MODULE_POSE_BODY);
  } else if (model_name_ == "rockx_pose_finger"){
    models.push_back(ROCKX_MODULE_POSE_FINGER_21);
  } else{
    assert(0);
  }
  for (size_t i = 0; i < models.size(); i++) {
    rockx_handle_t npu_handle = nullptr;
    rockx_module_t &model = models[i];
    rockx_ret_t ret = rockx_create(&npu_handle, model, config, config_size);
    if (ret != ROCKX_RET_SUCCESS) {
      fprintf(stderr, "init rockx module %d error=%d\n", model, ret);
      SetError(-EINVAL);
      return;
    }
    rockx_handles_.push_back(npu_handle);
  }
}

ROCKXFilter::~ROCKXFilter() {
  for (auto handle : rockx_handles_)
      rockx_destroy(handle);
}

int ROCKXFilter::Process(std::shared_ptr<MediaBuffer> input,
                        std::shared_ptr<MediaBuffer> &output) {
  auto input_buffer = std::static_pointer_cast<easymedia::ImageBuffer>(input);
  rockx_image_t input_img;
  input_img.width = input_buffer->GetWidth();
  input_img.height = input_buffer->GetHeight();
  input_img.pixel_format = StrToRockxPixelFMT(input_type_.c_str());
  input_img.data = (uint8_t *)input_buffer->GetPtr();

  auto &name = model_name_;
  auto &handles = rockx_handles_;
  if (name == "rockx_face_detect") {
    rockx_handle_t &face_det_handle = handles[0];
    rockx_object_array_t face_array;
    memset(&face_array, 0, sizeof(rockx_object_array_t));
    rockx_ret_t ret =
        rockx_face_detect(face_det_handle, &input_img, &face_array, nullptr);
    if (ret != ROCKX_RET_SUCCESS) {
      fprintf(stderr, "rockx_face_detect error %d\n", ret);
      return -1;
    }
    if (face_array.count <= 0)
      return -1;
    RknnResult result_item;
    memset(&result_item, 0, sizeof(RknnResult));
    result_item.type = NNRESULT_TYPE_FACE;
    auto &nn_result = input_buffer->GetRknnResult();
    for (int i = 0; i < face_array.count; i++) {
      rockx_object_t *object = &face_array.object[i];
      memcpy(&result_item.face_info.object, object, sizeof(rockx_object_t));
      nn_result.push_back(result_item);
      if (callback_)
        callback_(this, NNRESULT_TYPE_FACE, object, sizeof(rockx_object_t));
    }
    output = input;
    return 0;
  } else if (name == "rockx_face_landmark"){
      rockx_handle_t &face_det_handle = handles[0];
      rockx_handle_t &face_landmark_handle = handles[1];
      rockx_object_array_t face_array;
      memset(&face_array, 0, sizeof(rockx_object_array_t));
      rockx_ret_t ret =
          rockx_face_detect(face_det_handle, &input_img, &face_array, nullptr);
      if (ret != ROCKX_RET_SUCCESS) {
        fprintf(stderr, "rockx_face_detect error %d\n", ret);
        return -1;
      }
      if (face_array.count <= 0)
        return -1;

      RknnResult result_item;
      memset(&result_item, 0, sizeof(RknnResult));
      result_item.type = NNRESULT_TYPE_LANDMARK;
      auto &nn_result = input_buffer->GetRknnResult();
      for (int i = 0; i < face_array.count; i++) {
        rockx_face_landmark_t out_landmark;
        memset(&out_landmark, 0, sizeof(rockx_face_landmark_t));
        ret = rockx_face_landmark(face_landmark_handle, &input_img, &face_array.object[i].box, &out_landmark);
        if (ret != ROCKX_RET_SUCCESS && ret != -2) {
          fprintf(stderr, "rockx_face_landmark error %d\n", ret);
          return false;
        }
        memcpy(&result_item.landmark_info.object, &out_landmark, sizeof(rockx_face_landmark_t));
        nn_result.push_back(result_item);
        if (callback_)
          callback_(this, NNRESULT_TYPE_LANDMARK, &out_landmark, sizeof(rockx_face_landmark_t));
      }
      output = input;
      return 0;
  }else{
    assert(0);
  }
  return 0;

}

int ROCKXFilter::IoCtrl(unsigned long int request, ...) {
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

DEFINE_COMMON_FILTER_FACTORY(ROCKXFilter)
const char *FACTORY(ROCKXFilter)::ExpectedInputDataType() { return TYPE_ANYTHING; }
const char *FACTORY(ROCKXFilter)::OutPutDataType() { return TYPE_ANYTHING; }

} // namespace easymedia
