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

private:
  bool send_auth_failure_;
  rockface_pixel_format pixel_fmt_;
  rockface_handle_t face_handle_;
  bool detect_track_;
  bool detect_align_;
  bool detect_landmark_;
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

  std::string license_path = DEFAULT_LIC_PATH;
  if (params[KEY_PATH].empty()) {
    LOG("use default license file path:%s\n", license_path.c_str());
  } else {
    license_path = params[KEY_PATH];
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

  if (params[KEY_ROCKFACE_DETECT_TRACK].empty()) {
    detect_track_ = false;
  } else {
    detect_track_ = atoi(params[KEY_ROCKFACE_DETECT_TRACK].c_str());
  }

  if (params[KEY_ROCKFACE_DETECT_ALIGN].empty()) {
    detect_align_ = false;
  } else {
    detect_align_ = atoi(params[KEY_ROCKFACE_DETECT_ALIGN].c_str());
  }

  if (params[KEY_ROCKFACE_DETECT_LANDMARK].empty()) {
    detect_landmark_ = false;
  } else {
    detect_landmark_ = atoi(params[KEY_ROCKFACE_DETECT_LANDMARK].c_str());
  }

  face_handle_ = rockface_create_handle();

  int ret = rockface_set_licence(face_handle_, license_path.c_str());
  if (ret < 0)
    LOG("Error: authorization error %d!", ret);

  rockface_init_detector(face_handle_);
  send_auth_failure_ = false;
}

int RockFaceDetect::Process(std::shared_ptr<MediaBuffer> input,
                        std::shared_ptr<MediaBuffer> &output) {
  if (!input)
    return 1;

  auto img_buffer = std::static_pointer_cast<easymedia::ImageBuffer>(input);

  AutoDuration ad;

  int ret = -1;
  auto &nn_result = img_buffer->GetRknnResult();
  rockface_image_t input_image;

  input_image.width = img_buffer->GetWidth();
  input_image.height = img_buffer->GetHeight();
  input_image.pixel_format = pixel_fmt_;
  input_image.is_prealloc_buf = 1;
  input_image.data = (uint8_t *)img_buffer->GetPtr();
  input_image.size = img_buffer->GetValidSize();

  RknnResult result_item;
  memset(&result_item, 0, sizeof(RknnResult));
  result_item.type = NNRESULT_TYPE_FACE;

  rockface_det_array_t * det_array;
  rockface_det_array_t face_array;
  rockface_det_array_t tracked_face_array;
  memset(&face_array, 0, sizeof(rockface_det_array_t));

  if (send_auth_failure_)
    return -1;

  ret = rockface_detect(face_handle_, &input_image, &face_array);
  if (ret != ROCKFACE_RET_SUCCESS) {
    if (ret == ROCKFACE_RET_AUTH_FAIL) {
      RknnResult result;
      result.type = NNRESULT_TYPE_AUTHORIZED_STATUS;
      result.status = FAILURE;
      callback_(this, NNRESULT_TYPE_AUTHORIZED_STATUS, &result, 1);
      send_auth_failure_  = true;
    }
    LOG("rockface_face_detect error %d\n", ret);
    return 1;
  }
  det_array = &face_array;

  if (detect_track_) {
    LOG("RockFaceDetect detect_track_\n");
    rockface_track(face_handle_, &input_image, 1, &face_array,
                   &tracked_face_array);
    det_array = &tracked_face_array;
  }

  for (int i = 0; i < det_array->count; i++) {
    rockface_det_t *det_face = &(det_array->face[i]);

    if (detect_align_) {
      LOG("RockFaceDetect detect_align_\n");
      rockface_image_t aligned_img;
      memset(&aligned_img, 0, sizeof(rockface_image_t));
      ret = rockface_align(face_handle_, &input_image, &(det_face->box),
                           NULL, &aligned_img);
      if (ret != ROCKFACE_RET_SUCCESS) {
        LOG("error align face %d\n", ret);
        continue;
      }
      rockface_image_release(&aligned_img);
    }

    if (detect_landmark_) {
      LOG("RockFaceDetect detect_landmark_\n");
      rockface_landmark_t face_landmark;
      ret = rockface_landmark5(face_handle_, &input_image, &(det_face->box),
                               &face_landmark);
      if (ret != ROCKFACE_RET_SUCCESS) {
        LOG("error rockface_landmarke %d\n", ret);
        continue;
      }
      result_item.face_info.landmark = face_landmark;
    }

    result_item.face_info.base = *det_face;
    nn_result.push_back(result_item);

    LOGD("RockFaceDetect detect id %d\n", result_item.face_info.base.id);

  }

  if (nn_result.size()) {
    RknnResult infos[nn_result.size()];
    if (infos) {
      int i = 0;
      for (auto &iter : nn_result) {
        infos[i].timeval = img_buffer->GetAtomicClock();
        infos[i].img_w = img_buffer->GetWidth();
        infos[i].img_h = img_buffer->GetHeight();
        infos[i].face_info.base = iter.face_info.base;
        infos[i].type = NNRESULT_TYPE_FACE;
        i++;
      }
      AutoLockMutex _rw_mtx(cb_mtx_);
      callback_(this, NNRESULT_TYPE_FACE, infos, nn_result.size());
    }
    LOGD("RockFaceDetect %lld ms %lld us\n", ad.Get() / 1000, ad.Get() % 1000);
  }
  output = input;


  return 0;
}

int RockFaceDetect::IoCtrl(unsigned long int request, ...) {
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

DEFINE_COMMON_FILTER_FACTORY(RockFaceDetect)
const char *FACTORY(RockFaceDetect)::ExpectedInputDataType() {
  return TYPE_ANYTHING;
}
const char *FACTORY(RockFaceDetect)::OutPutDataType() { return TYPE_ANYTHING; }
} // namespace easymedia
