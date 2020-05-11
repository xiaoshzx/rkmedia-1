// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer.h"
#include "filter.h"
#include "rknn_utils.h"
#include "lock.h"
#include "rockface/rockface.h"

#define DEFAULT_LIC_PATH "/userdata/key.lic"

namespace easymedia {

class RockFaceDetect : public Filter {
public:
   RockFaceDetect(const char *param);
  virtual ~ RockFaceDetect() = default;
  static const char *GetFilterName() { return "rockface_detect"; }
  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> &output) override;
  virtual int IoCtrl(unsigned long int request, ...) override;

  void SendNNResult(std::list<RknnResult>& list, std::shared_ptr<ImageBuffer> image);

private:
  bool enable_track_;
  bool enable_landmark_;
  float score_threshod_;
  AuthorizedStatus auth_status_;
  rockface_pixel_format pixel_fmt_;
  rockface_handle_t face_handle_;
  RknnCallBack callback_;
  ReadWriteLockMutex cb_mtx_;
};

RockFaceDetect::RockFaceDetect(const char *param)
    : callback_(nullptr) {
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }
  if (params[KEY_INPUTDATATYPE].empty()) {
    LOG("lost input pixel format!\n");
    return;
  } else {
    pixel_fmt_ = StrToRockFacePixelFMT(params[KEY_INPUTDATATYPE].c_str());
    if (pixel_fmt_ >= ROCKFACE_PIXEL_FORMAT_MAX) {
      LOG("input pixel format not support yet!\n");
      return;
    }
  }
  std::string license_path = DEFAULT_LIC_PATH;
  if (params[KEY_PATH].empty()) {
    LOG("use default license file path:%s\n", license_path.c_str());
  } else {
    license_path = params[KEY_PATH];
  }

  score_threshod_ = 0.0;
  const std::string &score_threshod = params[KEY_SCORE_THRESHOD];
  if (!score_threshod.empty())
    score_threshod_ = std::stof(score_threshod);

  enable_track_ = false;
  const std::string &enable_track = params[KEY_FACE_DETECT_TRACK];
  if (!enable_track.empty())
    enable_track_ = atoi(enable_track.c_str());

  enable_landmark_ = false;
  const std::string &enable_landmark = params[KEY_FACE_DETECT_LANDMARK];
  if (!enable_landmark.empty())
    enable_landmark_ = atoi(enable_landmark.c_str());

  rockface_ret_t ret;
  face_handle_ = rockface_create_handle();
  ret = rockface_set_licence(face_handle_, license_path.c_str());
  if (ret != ROCKFACE_RET_SUCCESS)
    auth_status_ = FAILURE;
  else
    auth_status_ = SUCCESS;

  ret = rockface_init_detector(face_handle_);
  if (ret != ROCKFACE_RET_SUCCESS) {
    LOG("rockface_init_detector failed. ret = %d\n", ret);
    return;
  }
  if (auth_status_ != SUCCESS)
    LOG("rockface detect authorize failed.\n");
}

int RockFaceDetect::Process(std::shared_ptr<MediaBuffer> input,
                            std::shared_ptr<MediaBuffer> &output) {
  auto image = std::static_pointer_cast<easymedia::ImageBuffer>(input);
  if (!image)
    return -1;

  AutoDuration duration;
  rockface_image_t input_image;
  input_image.width = image->GetVirWidth();
  input_image.height = image->GetVirHeight();
  input_image.pixel_format = pixel_fmt_;
  input_image.is_prealloc_buf = 1;
  input_image.data = (uint8_t *)image->GetPtr();
  input_image.size = image->GetValidSize();

  RknnResult nn_result;
  memset(&nn_result, 0, sizeof(RknnResult));
  nn_result.type = NNRESULT_TYPE_FACE;
  auto &nn_list = image->GetRknnResult();
  if (auth_status_ == TIMEOUT)
    goto exit;

  rockface_ret_t ret;
  rockface_det_array_t det_array;
  rockface_det_array_t track_array;
  rockface_det_array_t* face_array;
  ret = rockface_detect(face_handle_, &input_image, &det_array);
  if (ret != ROCKFACE_RET_SUCCESS) {
    if (ret == ROCKFACE_RET_AUTH_FAIL) {
      RknnResult result;
      result.type = NNRESULT_TYPE_AUTHORIZED_STATUS;
      result.status = FAILURE;
      auth_status_ = TIMEOUT;
      callback_(this, NNRESULT_TYPE_AUTHORIZED_STATUS, &result, 1);
    }
    LOG("rockface_face_detect failed, ret = %d\n", ret);
    return -1;
  }
  face_array = &det_array;
  if (enable_track_) {
    ret = rockface_track(face_handle_, &input_image, 4, &det_array, &track_array);
    if (ret == ROCKFACE_RET_SUCCESS)
      face_array = &track_array;
  }

  for (int i = 0; i < face_array->count; i++) {
    rockface_det_t *face = &(face_array->face[i]);
    if (face->score - score_threshod_ < 0) {
      LOG("Drop the face, score = %f\n", face->score);
      continue;
    }
    if (enable_landmark_) {
      rockface_landmark_t* landmark = &nn_result.face_info.landmark;
      ret = rockface_landmark5(face_handle_, &input_image, &(face->box), landmark);
      if (ret != ROCKFACE_RET_SUCCESS) {
        LOG("rockface_landmark5 failed, ret = %d\n", ret);
        continue;
      }
    }
    nn_result.face_info.base = *face;
    nn_list.push_back(nn_result);
  }
  LOGD("RockFaceDetect cost time %lld us\n", duration.Get());
exit:
  SendNNResult(nn_list, image);
  output = input;
  return 0;
}

void RockFaceDetect::SendNNResult(std::list<RknnResult>& list,
                                  std::shared_ptr<ImageBuffer> image) {
  AutoLockMutex lock(cb_mtx_);
  if (!callback_)
    return;
  if (list.empty()) {
    callback_(this, NNRESULT_TYPE_FACE, nullptr, 0);
  } else {
    int count = 0;
    int size = list.size();
    RknnResult nn_array[size];
    for (auto &iter : list) {
      if (iter.type != NNRESULT_TYPE_FACE)
        continue;
      nn_array[count].timeval = image->GetAtomicClock();
      nn_array[count].img_w = image->GetWidth();
      nn_array[count].img_h = image->GetHeight();
      nn_array[count].face_info.base = iter.face_info.base;
      nn_array[count].type = NNRESULT_TYPE_FACE;
      count++;
    }
    callback_(this, NNRESULT_TYPE_FACE, nn_array, count);
  }
}

int RockFaceDetect::IoCtrl(unsigned long int request, ...) {
  va_list vl;
  va_start(vl, request);
  void *arg = va_arg(vl, void *);
  va_end(vl);

  if (!arg)
    return -1;

  int ret = 0;
  AutoLockMutex lock(cb_mtx_);
  switch (request) {
  case S_NN_CALLBACK: {
      callback_ = (RknnCallBack)arg;
    } break;
  case G_NN_CALLBACK: {
      arg = (void *)callback_;
    } break;
  default:
      ret = -1;
    break;
  }
  return ret;
}

DEFINE_COMMON_FILTER_FACTORY(RockFaceDetect)
const char *FACTORY(RockFaceDetect)::ExpectedInputDataType() {
  return TYPE_ANYTHING;
}
const char *FACTORY(RockFaceDetect)::OutPutDataType() { return TYPE_ANYTHING; }
} // namespace easymedia
