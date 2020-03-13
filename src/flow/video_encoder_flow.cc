// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include "encoder.h"
#include "flow.h"

#include "buffer.h"
#include "media_type.h"

#ifdef RK_MOVE_DETECTION
#include "move_detection_flow.h"
#endif

namespace easymedia {

static bool encode(Flow *f, MediaBufferVector &input_vector);

class VideoEncoderFlow : public Flow {
public:
  VideoEncoderFlow(const char *param);
  virtual ~VideoEncoderFlow() {
    AutoPrintLine apl(__func__);
    StopAllThread();
  }
  static const char *GetFlowName() { return "video_enc"; }
  int Control(unsigned long int request, ...);

private:
  std::shared_ptr<VideoEncoder> enc;
  bool extra_output;
  bool extra_merge;
  std::list<std::shared_ptr<MediaBuffer>> extra_buffer_list;
#ifdef RK_MOVE_DETECTION
  MoveDetectionFlow *md_flow;
#endif //RK_MOVE_DETECTION
  friend bool encode(Flow *f, MediaBufferVector &input_vector);
};

bool encode(Flow *f, MediaBufferVector &input_vector) {
  VideoEncoderFlow *vf = (VideoEncoderFlow *)f;
  std::shared_ptr<VideoEncoder> enc = vf->enc;
  std::shared_ptr<MediaBuffer> &src = input_vector[0];
  std::shared_ptr<MediaBuffer> dst, extra_dst;
  dst = std::make_shared<MediaBuffer>(); // TODO: buffer pool
  if (!dst) {
    LOG_NO_MEMORY();
    return false;
  }
  if (vf->extra_output) {
    extra_dst = std::make_shared<MediaBuffer>();
    if (!extra_dst) {
      LOG_NO_MEMORY();
      return false;
    }
  }

#ifdef RK_MOVE_DETECTION
  std::shared_ptr<MediaBuffer> md_info;
  if (vf->md_flow) {
    LOGD("[VEnc Flow]: LookForMdResult start!\n");
    int time_cost = 0;
    int last_time = 0;
    while(time_cost < 21) {
      if (time_cost >= 18)
        last_time = 1;
      md_info = vf->md_flow->LookForMdResult(
        src->GetAtomicClock(), last_time);
      if (md_info)
        break;

      time_cost += 3;
      msleep(3); //3ms
    }
    if (md_info && md_info->GetValidSize()) {
#ifndef NDEBUG
      LOGD("[VEnc Flow]: get md info(cnt=%d): %p, %zuBytes\n",
         md_info->GetValidSize() / sizeof(INFO_LIST),
         md_info.get(), md_info->GetValidSize());
      INFO_LIST *info = (INFO_LIST *)md_info->GetPtr();
      LOGD("[VEnc Flow]: mdinfo: flag:%d, upleft:<%d, %d>, downright:<%d, %d>\n",
        info->flag, info->up_left[0], info->up_left[1],
        info->down_right[0], info->down_right[1]);
#endif
      src->SetRelatedSPtr(md_info);
    } else
      LOG("ERROR: VEnc Flow: fate error get null md result\n");

    LOGD("[VEnc Flow]: LookForMdResult end!\n\n");
  }
#endif //RK_MOVE_DETECTION

  if (0 != enc->Process(src, dst, extra_dst)) {
    LOG("encoder failed\n");
    return false;
  }

  bool ret = true;
  // when output fps less len input fps, enc->Proccess() may
  // return a empty mediabuff.
  if (dst->GetValidSize() > 0) {
    ret = vf->SetOutput(dst, 0);
    if (vf->extra_output)
      ret &= vf->SetOutput(extra_dst, 1);
  }

  return ret;
}

VideoEncoderFlow::VideoEncoderFlow(const char *param) : extra_output(false),
    extra_merge(false)
#ifdef  RK_MOVE_DETECTION
, md_flow(nullptr)
#endif
{
  std::list<std::string> separate_list;
  std::map<std::string, std::string> params;
  if (!ParseWrapFlowParams(param, params, separate_list)) {
    SetError(-EINVAL);
    return;
  }
  std::string &codec_name = params[KEY_NAME];
  if (codec_name.empty()) {
    LOG("missing codec name\n");
    SetError(-EINVAL);
    return;
  }

  std::string &extra_merge_value = params[KEY_NEED_EXTRA_MERGE];
  if (!extra_merge_value.empty()) {
    extra_merge = !!std::stoi(extra_merge_value);
  }

  const char *ccodec_name = codec_name.c_str();
  // check input/output type
  std::string &&rule = gen_datatype_rule(params);
  if (rule.empty()) {
    SetError(-EINVAL);
    return;
  }
  if (!REFLECTOR(Encoder)::IsMatch(ccodec_name, rule.c_str())) {
    LOG("Unsupport for video encoder %s : [%s]\n", ccodec_name, rule.c_str());
    SetError(-EINVAL);
    return;
  }

  const std::string &enc_param_str = separate_list.back();
  std::map<std::string, std::string> enc_params;
  if (!parse_media_param_map(enc_param_str.c_str(), enc_params)) {
    SetError(-EINVAL);
    return;
  }
  MediaConfig mc;
  if (!ParseMediaConfigFromMap(enc_params, mc)) {
    SetError(-EINVAL);
    return;
  }

  auto encoder = REFLECTOR(Encoder)::Create<VideoEncoder>(
      ccodec_name, enc_param_str.c_str());
  if (!encoder) {
    LOG("Fail to create video encoder %s<%s>\n", ccodec_name,
        enc_param_str.c_str());
    SetError(-EINVAL);
    return;
  }

  if (!encoder->InitConfig(mc)) {
    LOG("Fail to init config, %s\n", ccodec_name);
    SetError(-EINVAL);
    return;
  }

  void *extra_data = nullptr;
  size_t extra_data_size = 0;
  encoder->GetExtraData(&extra_data, &extra_data_size);
  // TODO: if not h264
  const std::string &output_dt = enc_params[KEY_OUTPUTDATATYPE];

  enc = encoder;

  SlotMap sm;
  sm.input_slots.push_back(0);
  sm.output_slots.push_back(0);
  if (params[KEY_NEED_EXTRA_OUTPUT] == "y") {
    extra_output = true;
    sm.output_slots.push_back(1);
  }
  sm.process = encode;
  sm.thread_model = Model::ASYNCCOMMON;
  sm.mode_when_full = InputMode::DROPFRONT;
  sm.input_maxcachenum.push_back(3);
  if (!InstallSlotMap(sm, codec_name, 40)) {
    LOG("Fail to InstallSlotMap, %s\n", ccodec_name);
    SetError(-EINVAL);
    return;
  }

  if (extra_data && extra_data_size > 0 &&
      (output_dt == VIDEO_H264 || output_dt == VIDEO_H265)) {

    if (extra_merge) {
      std::shared_ptr<MediaBuffer> extra_buf = std::make_shared<MediaBuffer>();
      extra_buf->SetPtr(extra_data);
      extra_buf->SetValidSize(extra_data_size);
      extra_buf->SetUserFlag(MediaBuffer::kExtraIntra);
      SetOutput(extra_buf, 0);
    } else {
      if (output_dt == VIDEO_H264)
        extra_buffer_list = split_h264_separate((const uint8_t *)extra_data,
                                              extra_data_size, gettimeofday());
      else
        extra_buffer_list = split_h265_separate((const uint8_t *)extra_data,
                                              extra_data_size, gettimeofday());
      for (auto &extra_buffer : extra_buffer_list) {
        assert(extra_buffer->GetUserFlag() & MediaBuffer::kExtraIntra);
        SetOutput(extra_buffer, 0);
      }
    }

    if (extra_output) {
      std::shared_ptr<MediaBuffer> nullbuffer;
      SetOutput(nullbuffer, 1);
    }
  }
}

int VideoEncoderFlow::Control(unsigned long int request, ...) {
  va_list ap;
  va_start(ap, request);
  auto value = va_arg(ap, std::shared_ptr<ParameterBuffer>);
  va_end(ap);
  assert(value);

#ifdef RK_MOVE_DETECTION
  if (request == VideoEncoder::kMoveDetectionFlow) {
    if (value->GetSize() != sizeof(void **)) {
      LOG("ERROR: VEnc Flow: move detect config falied!\n");
      return -1;
    }
    md_flow = *((MoveDetectionFlow **)value->GetPtr());
    LOGD("[VEnc Flow]: md_flow:%p, flow name:%s\n",
      md_flow, md_flow->GetFlowName());
  } else
#endif //RK_MOVE_DETECTION
    enc->RequestChange(request, value);
  return 0;
}

DEFINE_FLOW_FACTORY(VideoEncoderFlow, Flow)
// type depends on encoder
const char *FACTORY(VideoEncoderFlow)::ExpectedInputDataType() { return ""; }
const char *FACTORY(VideoEncoderFlow)::OutPutDataType() { return ""; }

} // namespace easymedia
