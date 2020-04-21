// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer.h"
#include "encoder.h"
#include "filter.h"
#include "lock.h"
#include "media_config.h"

#define YUV_PIXEL_RED ((0x4C << 16) | (0x54 << 8) | 0xFF)

namespace easymedia {

static void draw_rect(std::shared_ptr<ImageBuffer> &buffer, Rect &rect,
                      int thick);
static int draw_nv12_rect(uint8_t *data, int img_w, int img_h, Rect &rect,
                          int thick, int yuv_color);
static Rect combine_rect(std::vector<Rect> &rect);
static void hw_draw_rect(uint8_t *data, int img_w, Rect &rect, int thick,
                         int palette_index);

class DrawFilter : public Filter {
public:
  DrawFilter(const char *param);
  virtual ~DrawFilter() = default;
  static const char *GetFilterName() { return "draw_filter"; }
  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> &output) override;
  virtual int IoCtrl(unsigned long int request, ...) override;

  void DoDrawRect(std::shared_ptr<ImageBuffer> &buffer, Rect &rect);
  void DoDraw(std::shared_ptr<ImageBuffer> &buffer,
              std::list<RknnResult> &nn_result);

  void DoHwDrawRect(OsdRegionData *region_data, int enable = 1);
  void DoHwDraw(std::shared_ptr<ImageBuffer> &buffer,
                std::list<RknnResult> &nn_result);

private:
  bool need_async_draw_;
  std::list<FaceInfo> face_infos_;
  ReadWriteLockMutex face_det_mtx_;
  RknnHandler handler_;
  int empty_detect_cnt_;
  int allow_empty_cnt_;
};

DrawFilter::DrawFilter(const char *param)
    : need_async_draw_(false), handler_(nullptr), empty_detect_cnt_(0),
      allow_empty_cnt_(3) {
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

void DrawFilter::DoDrawRect(std::shared_ptr<ImageBuffer> &buffer, Rect &rect) {
  int thick = 4;
  draw_rect(buffer, rect, thick);
}

void DrawFilter::DoHwDrawRect(OsdRegionData *region_data, int enable) {
  Flow *flow = (Flow *)handler_;
  if (region_data->enable &&
      ((region_data->width % 16) || (region_data->height % 16))) {
    LOG("ERROR: osd region size must be a multiple of 16x16.");
    return;
  }
  if (enable) {
    int buffer_size = region_data->width * region_data->height;
    OsdRegionData *rdata =
        (OsdRegionData *)malloc(sizeof(OsdRegionData) + buffer_size);
    memcpy((void *)rdata, (void *)region_data, sizeof(OsdRegionData));
    rdata->buffer = (uint8_t *)rdata + sizeof(OsdRegionData);
    memcpy(rdata->buffer, region_data->buffer, buffer_size);
    auto pbuff = std::make_shared<ParameterBuffer>(0);
    pbuff->SetPtr(rdata, sizeof(OsdRegionData) + buffer_size);
    flow->Control(VideoEncoder::kOSDDataChange, pbuff);
  } else {
    region_data->enable = enable;
    OsdRegionData *rdata = (OsdRegionData *)malloc(sizeof(OsdRegionData));
    memcpy((void *)rdata, (void *)region_data, sizeof(OsdRegionData));
    auto pbuff = std::make_shared<ParameterBuffer>(0);
    pbuff->SetPtr(rdata, sizeof(OsdRegionData));
    flow->Control(VideoEncoder::kOSDDataChange, pbuff);
  }
}

void DrawFilter::DoHwDraw(std::shared_ptr<ImageBuffer> &buffer,
                          std::list<RknnResult> &nn_result) {
  int thick = 4;
  int color_index = 0x23;
  OsdRegionData osd_region_data;
  memset(&osd_region_data, 0, sizeof(OsdRegionData));
  osd_region_data.enable = 1;
  osd_region_data.region_id = 7;
  if (!need_async_draw_) {
    face_infos_.clear();
    for (auto iter : nn_result)
      face_infos_.push_back(iter.face_info);
  }

  int size = face_infos_.size();
  if (size <= 0) {
    if (empty_detect_cnt_ >= allow_empty_cnt_)
      DoHwDrawRect(&osd_region_data, 0);
    if (empty_detect_cnt_ < allow_empty_cnt_)
      empty_detect_cnt_++;
    return;
  }
  empty_detect_cnt_ = 0;

  std::vector<Rect> rects;

  if (buffer->GetWidth() == 1080)
    thick *= 2;
  else if (buffer->GetWidth() > 1080)
    thick *= 4;

  for (int i = 0; i < size; i++) {
    Rect rect;
    auto face_det = face_infos_.front();
    face_infos_.pop_front();
    rect.left = UPALIGNTO16(face_det.base.box.left);
    rect.right = DOWNALIGNTO16(face_det.base.box.right);
    rect.top = UPALIGNTO16(face_det.base.box.top);
    rect.bottom = DOWNALIGNTO16(face_det.base.box.bottom);
    rects.push_back(rect);
  }
  Rect combine = combine_rect(rects);
  for (int i = 0; i < size; i++) {
    rects[i].left = rects[i].left - combine.left;
    rects[i].right = rects[i].right - combine.left;
    rects[i].top = rects[i].top - combine.top;
    rects[i].bottom = rects[i].bottom - combine.top;
  }

  osd_region_data.pos_x = combine.left;
  osd_region_data.pos_y = combine.top;
  osd_region_data.width = combine.right - combine.left;
  osd_region_data.height = combine.bottom - combine.top;
  int buffer_size = osd_region_data.width * osd_region_data.height;
#ifdef DRAW_HW_BUFFER
  auto mb = easymedia::MediaBuffer::Alloc(
      buffer_size, easymedia::MediaBuffer::MemType::MEM_HARD_WARE);
  osd_region_data.buffer = mb->GetPtr();
#else
  osd_region_data.buffer = static_cast<uint8_t *>(malloc(buffer_size));
#endif
  if (!osd_region_data.buffer) {
    return;
  }
  memset(osd_region_data.buffer, 0xFF, buffer_size);
  for (int i = 0; i < size; i++) {
    hw_draw_rect(osd_region_data.buffer, osd_region_data.width, rects[i], thick,
                 color_index);
  }
  DoHwDrawRect(&osd_region_data);

#ifndef DRAW_HW_BUFFER
  free(osd_region_data.buffer);
#endif
}

void DrawFilter::DoDraw(std::shared_ptr<ImageBuffer> &buffer,
                        std::list<RknnResult> &nn_result) {
  if (!need_async_draw_) {
    for (auto info : nn_result) {
      if (info.type == NNRESULT_TYPE_FACE) {
        rockface_det_t face_det = info.face_info.base;
        Rect rect = {face_det.box.left, face_det.box.top, face_det.box.right,
                     face_det.box.bottom};
        DoDrawRect(buffer, rect);
      }
    }
  } else {
    int cnt = face_infos_.size();
    for (int i = 0; i < cnt; i++) {
      auto face_info = face_infos_.front();
      face_infos_.pop_front();
      Rect rect = {face_info.base.box.left, face_info.base.box.top,
                   face_info.base.box.right, face_info.base.box.bottom};
      DoDrawRect(buffer, rect);
    }
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
  auto &nn_result = src->GetRknnResult();
  auto dst = std::static_pointer_cast<easymedia::ImageBuffer>(output);

  input->BeginCPUAccess(false);
  AutoLockMutex rw_mtx(face_det_mtx_);
  if (handler_)
    DoHwDraw(dst, nn_result);
  else
    DoDraw(dst, nn_result);

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
  case S_NN_HANDLER: {
    AutoLockMutex rw_mtx(face_det_mtx_);
    handler_ = (RknnHandler)arg;
  } break;
  case G_NN_HANDLER: {
    AutoLockMutex rw_mtx(face_det_mtx_);
    arg = (void *)handler_;
  } break;

  case S_SUB_REQUEST: {
    AutoLockMutex rw_mtx(face_det_mtx_);
    SubRequest *req = (SubRequest *)arg;
    if (S_NN_INFO == req->sub_request && need_async_draw_) {
      int size = req->size;
      FaceInfo *infos = (FaceInfo *)req->arg;
      if (!infos)
        break;
      memcpy(infos, req->arg, size * sizeof(FaceInfo));
      for (int i = 0; i < size; i++) {
        FaceInfo info = infos[i];
        face_infos_.push_back(info);
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

void draw_rect(std::shared_ptr<ImageBuffer> &buffer, Rect &rect, int thick) {
  ImageInfo info = buffer->GetImageInfo();
  uint8_t *img_data = (uint8_t *)buffer->GetPtr();
  int img_w = buffer->GetWidth();
  int img_h = buffer->GetHeight();

  if (img_w >= 1080)
    thick *= 4;

  if (rect.right >= img_w - thick) {
    LOG("draw_rect right > img_w\n");
    rect.left = img_w - thick - 1;
  }
  if (rect.left < 0) {
    LOG("draw_rect letf < 0\n");
    rect.left = 0;
  }
  if (rect.bottom >= img_h - thick) {
    LOG("draw_rect bottom > img_h\n");
    rect.bottom = img_h - thick - 1;
  }
  if (rect.top < 0) {
    LOG("draw_rect top < 0\n");
    rect.top = 0;
  }

  if (info.pix_fmt == PIX_FMT_NV12) {
    draw_nv12_rect(img_data, img_w, img_h, rect, thick, YUV_PIXEL_RED);
  } else {
    LOG("RockFaceDebug:can't draw rect on this format yet!\n");
  }
}

int draw_nv12_rect(uint8_t *data, int img_w, int img_h, Rect &rect, int thick,
                   int yuv_color) {
  int j, k;
  int uv_offset = img_w * img_h;
  int y_offset, u_offset, v_offset;
  int rect_x, rect_y, rect_w, rect_h;
  rect_x = rect.left;
  rect_y = rect.top;
  rect_w = rect.right - rect.left;
  rect_h = rect.bottom - rect.top;

  int y = (yuv_color >> 16) & 0xFF;
  int u = (yuv_color >> 8) & 0xFF;
  int v = (yuv_color >> 0) & 0xFF;

  for (j = rect_y; j < rect_y + rect_h; j++) {
    for (k = rect_x; k < rect_x + rect_w; k++) {
      if (k < (rect_x + thick) || k > (rect_x + rect_w - thick) ||
          j < (rect_y + thick) || j > (rect_y + rect_h - thick)) {
        y_offset = j * img_w + k;
        if (!(k & 0x1) && !(j & 0x1)) {
          u_offset = (j >> 1) * img_w + uv_offset + k;
          v_offset = u_offset + 1;
          data[y_offset] = y;
          data[u_offset] = u;
          data[v_offset] = v;
        }
      }
    }
  }
  return 0;
}

Rect combine_rect(std::vector<Rect> &rect) {
  Rect combine;
  int size = rect.size();
  memset(&combine, 0, sizeof(Rect));
  if (!size)
    return combine;
  combine.left = rect[0].left;
  combine.right = rect[0].right;
  combine.top = rect[0].top;
  combine.bottom = rect[0].bottom;
  for (int i = 1; i < size; i++) {
    combine.left = VALUE_MIN(combine.left, rect[i].left);
    combine.right = VALUE_MAX(combine.right, rect[i].right);
    combine.top = VALUE_MIN(combine.top, rect[i].top);
    combine.bottom = VALUE_MAX(combine.bottom, rect[i].bottom);
  }
  return combine;
}

void hw_draw_rect(uint8_t *data, int img_w, Rect &rect, int thick, int index) {
  int j, k, offset;
  for (j = rect.top; j < rect.bottom; j++) {
    for (k = rect.left; k < rect.right; k++) {
      if (k < (rect.left + thick) || k > (rect.right - thick) ||
          j < (rect.top + thick) || j > (rect.bottom - thick)) {
        offset = j * img_w + k;
        data[offset] = index;
      }
    }
  }
}

} // namespace easymedia
