// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer.h"
#include "flow.h"
#include "stream.h"
#include "utils.h"
#include "rockface/rockface.h"

#define DEFAULT_LIC_PATH "/userdata/key.lic"

namespace easymedia {

static bool image_process(Flow *f, MediaBufferVector &input_vector);

static const struct PixelFmtEntry {
  rockface_pixel_format fmt;
  const char *fmt_str;
} pixel_fmt_string_map[] = {
  {ROCKFACE_PIXEL_FORMAT_GRAY8, "image:gray8"},
  {ROCKFACE_PIXEL_FORMAT_RGB888, IMAGE_RGB888},
  {ROCKFACE_PIXEL_FORMAT_BGR888, IMAGE_BGR888},
  {ROCKFACE_PIXEL_FORMAT_RGBA8888, IMAGE_ARGB8888},
  {ROCKFACE_PIXEL_FORMAT_BGRA8888, IMAGE_ABGR8888},
  {ROCKFACE_PIXEL_FORMAT_YUV420P_YU12, IMAGE_YUV420P},
  {ROCKFACE_PIXEL_FORMAT_YUV420P_YV12, "image:yv12"},
  {ROCKFACE_PIXEL_FORMAT_YUV420SP_NV12, IMAGE_NV12},
  {ROCKFACE_PIXEL_FORMAT_YUV420SP_NV21, IMAGE_NV21},
  {ROCKFACE_PIXEL_FORMAT_YUV422P_YU16, IMAGE_UYVY422},
  {ROCKFACE_PIXEL_FORMAT_YUV422P_YV16, "image:yv16"},
  {ROCKFACE_PIXEL_FORMAT_YUV422SP_NV16, IMAGE_NV16},
  {ROCKFACE_PIXEL_FORMAT_YUV422SP_NV61, IMAGE_NV61}
};

static rockface_pixel_format StrToRockFacePixelFMT(const char *fmt_str) {
  FIND_ENTRY_TARGET_BY_STRCMP(fmt_str, pixel_fmt_string_map, fmt_str, fmt)
  return ROCKFACE_PIXEL_FORMAT_MAX;
}

class RockFaceFlow : public Flow {
public:
  RockFaceFlow(const char *param);
  virtual ~RockFaceFlow();
  static const char *GetFlowName() { return "rockface_flow"; }

private:
  friend bool image_process(Flow *f, MediaBufferVector &input_vector);

private:
  rockface_pixel_format pixel_fmt;
  rockface_handle_t face_handle;
};

RockFaceFlow::RockFaceFlow(const char *param) {
  int ret = -1;
  std::string license_path = DEFAULT_LIC_PATH;
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }

  if (params[KEY_PATH].empty()) {
    LOG("use default license file path:%s\n", license_path.c_str());
  } else {
    license_path = params[KEY_PATH];
  }

  if (params[KEY_INPUTDATATYPE].empty()) {
    LOG("lost input pixel format!\n");
    return;
  } else {
    pixel_fmt = StrToRockFacePixelFMT(params[KEY_INPUTDATATYPE].c_str());
    if (pixel_fmt >= ROCKFACE_PIXEL_FORMAT_MAX) {
      LOG("input pixel format not support yet!\n");
      return;
    }
  }

  face_handle = rockface_create_handle();

  ret = rockface_set_licence(face_handle, license_path.c_str());
  if (ret < 0) {
    LOG("Error: authorization error %d!", ret);
    return;
  }

  rockface_init_detector(face_handle);
  rockface_init_analyzer(face_handle);

  SlotMap sm;
  sm.input_slots.push_back(0);
  sm.output_slots.push_back(0);
  sm.thread_model = Model::ASYNCCOMMON;
  sm.mode_when_full = InputMode::DROPFRONT;
  sm.input_maxcachenum.push_back(0);
  sm.process = image_process;

  std::string &name = params[KEY_NAME];
  if (!InstallSlotMap(sm, name, 0)) {
    LOG("Fail to InstallSlotMap, %s\n", name.c_str());
    return;
  }
}

RockFaceFlow::~RockFaceFlow() {
  StopAllThread();
  rockface_release_handle(face_handle);
}

bool image_process(Flow *f, MediaBufferVector &input_vector) {
  int ret = -1;
  RockFaceFlow *flow = static_cast<RockFaceFlow *>(f);
  auto &buffer = input_vector[0];
  if (!buffer)
    return true;

  auto img_buffer = std::static_pointer_cast<easymedia::ImageBuffer>(buffer);
  auto &nn_result = img_buffer->GetRknnResult();

  rockface_image_t input_image;

  input_image.width = img_buffer->GetWidth();
  input_image.height = img_buffer->GetHeight();
  input_image.pixel_format = flow->pixel_fmt;
  input_image.is_prealloc_buf = 1;
  input_image.data = (uint8_t *)buffer->GetPtr();
  input_image.size = buffer->GetValidSize();

  rockface_det_array_t face_array;
  memset(&face_array, 0, sizeof(rockface_det_array_t));

  ret = rockface_detect(flow->face_handle, &input_image, &face_array);
  if (ret != ROCKFACE_RET_SUCCESS) {
    LOG("rockface_face_detect error %d\n", ret);
    return false;
  }

  for (int i = 0; i < face_array.count; i++) {
    rockface_det_t *det_face = &(face_array.face[i]);
    // face align
    rockface_image_t aligned_img;
    memset(&aligned_img, 0, sizeof(rockface_image_t));
    ret = rockface_align(flow->face_handle, &input_image, &(det_face->box),
                         NULL, &aligned_img);
    if (ret != ROCKFACE_RET_SUCCESS) {
      LOG("error align face %d\n", ret);
      continue;
    }
    // face attribute
    rockface_attribute_t face_attr;
    ret = rockface_attribute(flow->face_handle, &aligned_img, &face_attr);

    rockface_image_release(&aligned_img);

    if (ret != ROCKFACE_RET_SUCCESS) {
      LOG("error rockface_attribute %d\n", ret);
      continue;
    }
    // face landmark
    rockface_landmark_t face_landmark;
    ret = rockface_landmark5(flow->face_handle, &input_image, &(det_face->box),
                             &face_landmark);
    if (ret != ROCKFACE_RET_SUCCESS) {
      LOG("error rockface_landmarke %d\n", ret);
      continue;
    }
    // face angle
    rockface_angle_t face_angle;
    ret = rockface_angle(flow->face_handle, &face_landmark, &face_angle);

    RknnResult result_item;
    memset(&result_item, 0, sizeof(RknnResult));
    result_item.type = NNRESULT_TYPE_FACE;
    result_item.face_info.base = *det_face;
    result_item.face_info.attr = face_attr;
    result_item.face_info.landmark = face_landmark;
    result_item.face_info.angle = face_angle;
    nn_result.push_back(result_item);
  }

  flow->SetOutput(img_buffer, 0);

  return true;
}

DEFINE_FLOW_FACTORY(RockFaceFlow, Flow)
const char *FACTORY(RockFaceFlow)::ExpectedInputDataType() { return nullptr; }
const char *FACTORY(RockFaceFlow)::OutPutDataType() { return ""; }

} // namespace easymedia
