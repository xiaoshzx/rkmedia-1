// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "buffer.h"
#include "flow.h"
#include "stream.h"
#include "utils.h"
#include "link_config.h"

namespace easymedia {

static bool process_buffer(Flow *f, MediaBufferVector &input_vector);

class _API LinkFlow : public Flow {
public:
  LinkFlow(const char *param);
  virtual ~LinkFlow();
  static const char *GetFlowName() { return "link_flow"; }
private:
  friend bool process_buffer(Flow *f, MediaBufferVector &input_vector);

private:
  LinkType link_type_;
};

LinkFlow::LinkFlow(const char *param)
{
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }

  SetVideoHandler(nullptr);
  SetAudioHandler(nullptr);
  SetCaptureHandler(nullptr);

  SlotMap sm;
  sm.input_slots.push_back(0);
  if (sm.thread_model == Model::NONE)
    sm.thread_model =
        !params[KEY_FPS].empty() ? Model::ASYNCATOMIC : Model::ASYNCCOMMON;
  if (sm.mode_when_full == InputMode::NONE)
    sm.mode_when_full = InputMode::DROPCURRENT;

  sm.input_maxcachenum.push_back(0);
  sm.process = process_buffer;

  if (!InstallSlotMap(sm, "LinkFLow", 0)) {
    LOG("Fail to InstallSlotMap for LinkFLow\n");
    return;
  }

  std::string &type = params[KEY_INPUTDATATYPE];
  link_type_ = LINK_NONE;
  if (type.find(VIDEO_PREFIX) != std::string::npos) {
    link_type_ = LINK_VIDEO;
  } else if (type.find(AUDIO_PREFIX) != std::string::npos) {
    link_type_ = LINK_AUDIO;
  } else if (type.find(IMAGE_PREFIX) != std::string::npos) {
    link_type_ = LINK_PICTURE;
  }
}

LinkFlow::~LinkFlow() {
  StopAllThread();
}

bool process_buffer(Flow *f, MediaBufferVector &input_vector) {
  LinkFlow *flow = static_cast<LinkFlow *>(f);
  auto &buffer = input_vector[0];
  if (!buffer && !flow)
    return true;

  if (flow->link_type_ == LINK_VIDEO) {
    auto link_handler = flow->GetVideoHandler();
    auto nal_type = (buffer->GetUserFlag() & MediaBuffer::kIntra) ? 1 : 0;
    auto timestamp = easymedia::gettimeofday() / 1000;
    if (link_handler)
      link_handler((unsigned char *)buffer->GetPtr(), buffer->GetValidSize(),
                   timestamp, nal_type);
  } else if (flow->link_type_ == LINK_AUDIO) {
    auto link_audio_handler = flow->GetAudioHandler();
    auto timestamp = easymedia::gettimeofday() / 1000;
    if (link_audio_handler)
      link_audio_handler((unsigned char *)buffer->GetPtr(), buffer->GetValidSize(),
                         timestamp);
  }

  return 0;
}

DEFINE_FLOW_FACTORY(LinkFlow, Flow)
const char *FACTORY(LinkFlow)::ExpectedInputDataType() { return nullptr; }
const char *FACTORY(LinkFlow)::OutPutDataType() { return ""; }

} // namespace easymedia

