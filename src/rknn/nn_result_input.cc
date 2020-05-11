// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <queue>

#include "buffer.h"
#include "encoder.h"
#include "filter.h"
#include "lock.h"
#include "media_config.h"

namespace easymedia {

class NNResultInput : public Filter {
public:
  NNResultInput(const char *param);
  virtual ~NNResultInput() = default;
  static const char *GetFilterName() { return "nn_result_input"; }

  void PushResult(std::list<RknnResult> &results);
  std::list<RknnResult> PopResult(void);

  bool Wait(std::unique_lock<std::mutex> &lock, uint32_t milliseconds);
  void Signal(void);

  virtual int Process(std::shared_ptr<MediaBuffer> input,
                      std::shared_ptr<MediaBuffer> &output) override;
  virtual int IoCtrl(unsigned long int request, ...) override;

private:
  unsigned int frame_caches_;
  int frame_rate_;
  RknnHandler handler_;
  ReadWriteLockMutex result_mutex_;
  std::mutex mutex_;
  std::condition_variable cond_;
  std::queue<std::list<RknnResult>> nn_results_list_;
};

NNResultInput::NNResultInput(const char *param) {
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }

  if (params[KEY_FRAME_RATE].empty()) {
    frame_rate_ = 30;
  } else {
    frame_rate_ = atoi(params[KEY_FRAME_RATE].c_str());
  }

  if (params[KEY_FRAME_CACHES].empty()) {
    frame_caches_ = 1;
  } else {
    frame_caches_ = atoi(params[KEY_FRAME_CACHES].c_str());
  }
}

bool NNResultInput::Wait(std::unique_lock<std::mutex> &lock,
                         uint32_t milliseconds) {
  if (cond_.wait_for(lock, std::chrono::milliseconds(milliseconds)) ==
      std::cv_status::timeout)
    return false;
  return true;
}
void NNResultInput::Signal() { cond_.notify_all(); }

void NNResultInput::PushResult(std::list<RknnResult> &results) {
  std::lock_guard<std::mutex> lock(mutex_);
  while (nn_results_list_.size() > frame_caches_)
    nn_results_list_.pop();
  nn_results_list_.push(results);
  Signal();
}

std::list<RknnResult> NNResultInput::PopResult(void) {
  std::unique_lock<std::mutex> lock(mutex_);
  std::list<RknnResult> list;
  if (nn_results_list_.empty()) {
    uint32_t milliseconds = 1000 / frame_rate_;
    if (Wait(lock, milliseconds) == false) {
      return std::move(list);
    }
  }
  list = nn_results_list_.front();
  nn_results_list_.pop();
  return std::move(list);
}

int NNResultInput::Process(std::shared_ptr<MediaBuffer> input,
                           std::shared_ptr<MediaBuffer> &output) {
  if (!input || input->GetType() != Type::Image)
    return -EINVAL;
  if (!output || output->GetType() != Type::Image)
    return -EINVAL;
  AutoLockMutex rw_mtx(result_mutex_);
  auto src = std::static_pointer_cast<easymedia::ImageBuffer>(input);
  auto &nn_results = src->GetRknnResult();
  auto input_nn_results = PopResult();
  nn_results.clear();
  for (auto &iter : input_nn_results) {
    nn_results.push_back(iter);
  }
  output = input;
  return 0;
}

int NNResultInput::IoCtrl(unsigned long int request, ...) {
  va_list vl;
  va_start(vl, request);
  void *arg = va_arg(vl, void *);
  va_end(vl);

  int ret = 0;
  AutoLockMutex rw_mtx(result_mutex_);
  switch (request) {
  case S_SUB_REQUEST: {
    SubRequest *req = (SubRequest *)arg;
    if (S_NN_INFO == req->sub_request) {
      int size = req->size;
      std::list<RknnResult> infos_list;
      RknnResult *infos = (RknnResult *)req->arg;
      if (infos) {
        for (int i = 0; i < size; i++)
          infos_list.push_back(infos[i]);
      }
      PushResult(infos_list);
    }
  } break;
  default:
    ret = -1;
    break;
  }
  return ret;
}

DEFINE_COMMON_FILTER_FACTORY(NNResultInput)
const char *FACTORY(NNResultInput)::ExpectedInputDataType() {
  return TYPE_ANYTHING;
}
const char *FACTORY(NNResultInput)::OutPutDataType() { return TYPE_ANYTHING; }

} // namespace easymedia
