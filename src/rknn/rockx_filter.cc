// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer.h"
#include "filter.h"
#include "rknn_utils.h"
#include "utils.h"
#include <assert.h>
#include <rockx/rockx.h>

namespace easymedia {

void SavePoseBodyImg(rockx_image_t input_image,
                     rockx_keypoints_array_t *body_array) {
  static int frame_num = 0;
  const std::vector<std::pair<int, int>> pose_pairs = {
      {2, 3},   {3, 4},   {5, 6},  {6, 7},  {8, 9},   {9, 10},
      {11, 12}, {12, 13}, {1, 0},  {0, 14}, {14, 16}, {0, 15},
      {15, 17}, {2, 5},   {8, 11}, {2, 8},  {5, 11}};
  // process result
  for (int i = 0; i < body_array->count; i++) {
    printf("asx   person %d:\n", i);

    for (int j = 0; j < body_array->keypoints[i].count; j++) {
      int x = body_array->keypoints[i].points[j].x;
      int y = body_array->keypoints[i].points[j].y;
      float score = body_array->keypoints[i].score[j];
      printf("  %s [%d, %d] %f\n", ROCKX_POSE_BODY_KEYPOINTS_NAME[j], x, y,
             score);
      rockx_image_draw_circle(&input_image, {x, y}, 3, {255, 0, 0}, -1);
    }

    for (int j = 0; j < (int)pose_pairs.size(); j++) {
      const std::pair<int, int> &posePair = pose_pairs[j];
      int x0 = body_array->keypoints[i].points[posePair.first].x;
      int y0 = body_array->keypoints[i].points[posePair.first].y;
      int x1 = body_array->keypoints[i].points[posePair.second].x;
      int y1 = body_array->keypoints[i].points[posePair.second].y;

      if (x0 > 0 && y0 > 0 && x1 > 0 && y1 > 0)
        rockx_image_draw_line(&input_image, {x0, y0}, {x1, y1}, {0, 255, 0}, 1);
    }
  }
  if (body_array != NULL) {
    char path[64];
    snprintf(path, 64, "/data/%d.jpg", frame_num++);
    rockx_image_write(path, &input_image);
  } else {
    char path[64];
    snprintf(path, 64, "/data/err_%d.rgb", frame_num++);
    FILE *fp = fopen(path, "wb");
    if (fp) {
      int stride =
          (input_image.pixel_format == ROCKX_PIXEL_FORMAT_RGB888) ? 3 : 2;
      int size = input_image.width * input_image.height * stride;
      fwrite(input_image.data, size, 1, fp);
      fclose(fp);
    }
  }
}

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
  int ProcessRockxFaceDetect(
      std::shared_ptr<easymedia::ImageBuffer> input_buffer,
      rockx_image_t input_img, std::vector<rockx_handle_t> handles);
  int ProcessRockxFaceLandmark(
      std::shared_ptr<easymedia::ImageBuffer> input_buffer,
      rockx_image_t input_img, std::vector<rockx_handle_t> handles);
  int ProcessRockxPoseBody(std::shared_ptr<easymedia::ImageBuffer> input_buffer,
                           rockx_image_t input_img,
                           std::vector<rockx_handle_t> handles);
};

ROCKXFilter::ROCKXFilter(const char *param) : model_name_("") {
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
  } else if (model_name_ == "rockx_face_landmark") {
    models.push_back(ROCKX_MODULE_FACE_DETECTION);
    models.push_back(ROCKX_MODULE_FACE_LANDMARK_68);
  } else if (model_name_ == "rockx_pose_body") {
    models.push_back(ROCKX_MODULE_POSE_BODY_V2);
  } else if (model_name_ == "rockx_pose_finger") {
    models.push_back(ROCKX_MODULE_POSE_FINGER_21);
  } else {
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

int ROCKXFilter::ProcessRockxFaceDetect(
    std::shared_ptr<easymedia::ImageBuffer> input_buffer,
    rockx_image_t input_img, std::vector<rockx_handle_t> handles) {
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
    result_item.img_w = input_img.width;
    result_item.img_h = input_img.height;
    nn_result.push_back(result_item);
    if (callback_)
      callback_(this, NNRESULT_TYPE_FACE, object, sizeof(rockx_object_t));
  }
  return 0;
}

int ROCKXFilter::ProcessRockxFaceLandmark(
    std::shared_ptr<easymedia::ImageBuffer> input_buffer,
    rockx_image_t input_img, std::vector<rockx_handle_t> handles) {
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
    ret = rockx_face_landmark(face_landmark_handle, &input_img,
                              &face_array.object[i].box, &out_landmark);
    if (ret != ROCKX_RET_SUCCESS && ret != -2) {
      fprintf(stderr, "rockx_face_landmark error %d\n", ret);
      return false;
    }
    memcpy(&result_item.landmark_info.object, &out_landmark,
           sizeof(rockx_face_landmark_t));
    result_item.img_w = input_img.width;
    result_item.img_h = input_img.height;
    nn_result.push_back(result_item);
    if (callback_)
      callback_(this, NNRESULT_TYPE_LANDMARK, &out_landmark,
                sizeof(rockx_face_landmark_t));
  }
  return 0;
}

int ROCKXFilter::ProcessRockxPoseBody(
    std::shared_ptr<easymedia::ImageBuffer> input_buffer,
    rockx_image_t input_img, std::vector<rockx_handle_t> handles) {
  fprintf(stderr, "%s %d \n", __FUNCTION__, __LINE__);
  rockx_handle_t &pose_body_handle = handles[0];
  rockx_keypoints_array_t key_points_array;
  memset(&key_points_array, 0, sizeof(rockx_keypoints_array_t));
  rockx_ret_t ret =
      rockx_pose_body(pose_body_handle, &input_img, &key_points_array, nullptr);
  if (ret != ROCKX_RET_SUCCESS) {
    fprintf(stderr, "rockx_face_detect error %d\n", ret);
    return -1;
  }
  if (key_points_array.count <= 0) {
#ifdef DEBUG_POSE_BODY
    SavePoseBodyImg(input_img, NULL);
#endif
    return -1;
  }

  RknnResult result_item;
  memset(&result_item, 0, sizeof(RknnResult));
  result_item.type = NNRESULT_TYPE_BODY;
  auto &nn_result = input_buffer->GetRknnResult();
  for (int i = 0; i < key_points_array.count; i++) {
    rockx_keypoints_t *object = &key_points_array.keypoints[i];
    memcpy(&result_item.body_info.object, object, sizeof(rockx_keypoints_t));
    result_item.img_w = input_img.width;
    result_item.img_h = input_img.height;
    nn_result.push_back(result_item);
    if (callback_)
      callback_(this, NNRESULT_TYPE_BODY, object, sizeof(rockx_keypoints_t));
  }
#ifdef DEBUG_POSE_BODY
  savePoseBodyImg(input_img, &key_points_array);
#endif
  return 0;
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
    ProcessRockxFaceDetect(input_buffer, input_img, handles);
    output = input;
    return 0;
  } else if (name == "rockx_face_landmark") {
    ProcessRockxFaceLandmark(input_buffer, input_img, handles);
    output = input;
    return 0;
  } else if (name == "rockx_pose_body") {
    ProcessRockxPoseBody(input_buffer, input_img, handles);
    output = input;
    return 0;
  } else {
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
  case S_NN_CALLBACK:
    callback_ = (RknnCallBack)arg;
    break;
  case G_NN_CALLBACK:
    arg = (void *)callback_;
    break;
  default:
    ret = -1;
    break;
  }
  return ret;
}

DEFINE_COMMON_FILTER_FACTORY(ROCKXFilter)
const char *FACTORY(ROCKXFilter)::ExpectedInputDataType() {
  return TYPE_ANYTHING;
}
const char *FACTORY(ROCKXFilter)::OutPutDataType() { return TYPE_ANYTHING; }

} // namespace easymedia
