// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer.h"
#include "filter.h"
#include "lock.h"

#define YUV_PIXEL_RED 76, 84, 255

namespace easymedia {

static void draw_face_rect(std::shared_ptr<ImageBuffer> &buffer,
                               int x, int y, int w, int h, int pixel);
static int draw_nv12_rect(uint8_t *data, int img_w, int img_h, int rect_x, int rect_y,
                       int rect_w, int rect_h, int pixel, int Y, int U, int V);

class DrawFilter : public Filter {
public:
   DrawFilter(const char *param);
  virtual ~ DrawFilter() = default;
  static const char *GetFilterName() { return "draw_filter"; }
  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> &output) override;
  virtual int IoCtrl(unsigned long int request, ...) override;

private:
  bool need_async_draw_;
  std::list<FaceInfo> face_det_;
  int64_t ghost_us_;
  AutoDuration ad_;
  std::list<FaceInfo> last_face_det_;
  ReadWriteLockMutex face_det_mtx_;
};

DrawFilter::DrawFilter(const char *param)
    : need_async_draw_(false), ghost_us_(50000) {
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }

  if (params[KEY_NEED_ASYNC_DRAW].empty()) {
    need_async_draw_ = false;
  } else {
    need_async_draw_ = atoi(params[KEY_NEED_ASYNC_DRAW].c_str());
  }
}

int DrawFilter::Process(std::shared_ptr<MediaBuffer> input,
                        std::shared_ptr<MediaBuffer> &output) {
  if (!input || input->GetType() != Type::Image)
    return -EINVAL;
  if (!output || output->GetType() != Type::Image)
    return -EINVAL;


  output = input;
  auto src = std::static_pointer_cast<easymedia::ImageBuffer>(input);
  auto dst = std::static_pointer_cast<easymedia::ImageBuffer>(output);

  input->BeginCPUAccess(false);

  if (!need_async_draw_) {
    auto &nn_result = src->GetRknnResult();
    for (auto info : nn_result) {
      if (info.type == NNRESULT_TYPE_FACE) {
        rockface_det_t face_det = info.face_info.base;
        draw_face_rect(dst, face_det.box.left, face_det.box.right,
                       face_det.box.top, face_det.box.bottom, 6);
      }
    }
  } else {
    AutoLockMutex _rw_mtx(face_det_mtx_);
    int cnt = face_det_.size();
    int last_cnt = last_face_det_.size();

    if (!cnt && last_cnt && ghost_us_ > ad_.Get()) {
      for(int i = 0; i < last_cnt; i++) {
        auto face_det = last_face_det_.front();
        last_face_det_.pop_front();
        draw_face_rect(dst, face_det.base.box.left, face_det.base.box.right,
                       face_det.base.box.top, face_det.base.box.bottom, 6);
      }
    } else if (cnt) {
      last_face_det_ = face_det_;
    }

    for(int i = 0; i < cnt; i++) {
      auto face_det = face_det_.front();
      face_det_.pop_front();
      draw_face_rect(dst, face_det.base.box.left, face_det.base.box.right,
                     face_det.base.box.top, face_det.base.box.bottom, 6);
      ad_.Reset();
    }
  }
  input->EndCPUAccess(false);

  return 0;
}

int DrawFilter::IoCtrl(unsigned long int request, ...) {
  va_list vl;
  va_start(vl, request);
  void *arg = va_arg(vl, void *);
  va_end(vl);

  int ret = 0;
  switch (request) {
  case S_SUB_REQUEST: {
    SubRequest *req = (SubRequest *)arg;
    if (S_DETECT_INFO == req->sub_request && need_async_draw_) {
      FaceInfo *infos = (FaceInfo *)req->arg;
      int size = req->size;
      AutoLockMutex _rw_mtx(face_det_mtx_);
      for(int i = 0; i < size; i++) {
        face_det_.push_back(infos[i]);
      }
    }
  } break;
  default:
    ret = -1;
    break;
  }

  return ret;
}

DEFINE_COMMON_FILTER_FACTORY(DrawFilter)
const char *FACTORY(DrawFilter)::ExpectedInputDataType() {
  return TYPE_ANYTHING;
}
const char *FACTORY(DrawFilter)::OutPutDataType() { return TYPE_ANYTHING; }

void draw_face_rect(std::shared_ptr<ImageBuffer> &buffer, int letf, int right,
                        int top, int bottom, int pixel) {
  ImageInfo info = buffer->GetImageInfo();
  uint8_t *img_data = (uint8_t *)buffer->GetPtr();
  int img_w = buffer->GetWidth();
  int img_h = buffer->GetHeight();

  if (img_w >= 1080)
    pixel *= 4;

  if (right >= img_w - pixel) {
    LOG("draw_face_rect right > img_w\n");
    letf = img_w - pixel -1;
  }
  if (letf < 0) {
    LOG("draw_face_rect letf < 0\n");
    letf = 0;
  }
  if (bottom >= img_h - pixel) {
    LOG("draw_face_rect bottom > img_h\n");
    bottom = img_h - pixel -1;
  }
  if (top < 0) {
    LOG("draw_face_rect top < 0\n");
    top = 0;
  }

  if (info.pix_fmt == PIX_FMT_NV12) {
    draw_nv12_rect(img_data, img_w, img_h, letf,
                   right, top, bottom, pixel, YUV_PIXEL_RED);
  } else {
    LOG("RockFaceDebug:can't draw rect on this format yet!\n");
  }
}

int draw_nv12_rect(uint8_t *data, int img_w, int img_h, int letf, int right,
                        int top, int bottom, int pixel, int Y, int U, int V) {
  int j, k;
  int uv_offset = img_w * img_h;
  int y_offset, u_offset, v_offset;
  int  rect_x, rect_y, rect_w, rect_h;
  rect_x = letf;
  rect_y = top;
  rect_w = right - letf;
  rect_h = bottom - top;
  for (j = rect_y; j < rect_y + rect_h; j++) {
    for (k = rect_x; k < rect_x + rect_w; k++) {
      if (k < (rect_x + pixel) || k > (rect_x + rect_w - pixel) ||
        j < (rect_y + pixel) || j > (rect_y + rect_h - pixel)) {
        y_offset = j * img_w + k;
        if (!(k & 0x1) && !(j & 0x1)) {
          u_offset = (j >> 1) * img_w + uv_offset + k;
          v_offset = u_offset + 1;
          data[y_offset] = Y;
          data[u_offset] = U;
          data[v_offset] = V;
        }
      }
    }
  }
  return 0;
}

} // namespace easymedia

