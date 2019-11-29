// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "message.h"

#include <assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include "key_string.h"
#include "utils.h"

namespace easymedia {

bool operator==(const EventMessage &left, const EventMessage &right)
{
  return (left.sender_ == right.sender_)
          && (left.id_ == right.id_)
          && (left.param_ == right.param_)
          && (left.params_ == right.params_)
          && (left.type_ == right.type_);
}

bool operator!=(const EventMessage &left, const EventMessage &right)
{
  return !(left == right);
}

void EventHandler::RegisterEventHook(std::shared_ptr<easymedia::Flow> flow, EventHook proc)
{
  process_ = proc;
  event_thread_loop_ = true;
  event_thread_ = new std::thread(process_, flow, std::ref(event_thread_loop_));
}

void EventHandler::UnRegisterEventHook()
{
  if (event_thread_) {
    event_thread_loop_ = false;
    event_cond_mtx_.lock();
    event_cond_mtx_.notify();
    event_cond_mtx_.unlock();
    event_thread_->join();
    delete event_thread_;
    event_thread_ = nullptr;
  }
}

void EventHandler::EventHookWait()
{
  AutoLockMutex _signal_mtx(event_cond_mtx_);
  event_cond_mtx_.wait();
}

void EventHandler::SignalEventHook()
{
  AutoLockMutex _signal_mtx(event_cond_mtx_);
  event_cond_mtx_.notify();
}

EventMessage * EventHandler::GetEventMessages()
{
  EventMessage * msg = nullptr;
  AutoLockMutex _rw_mtx(event_queue_mtx_);
  if (process_) {
    if (event_msgs_.empty()) {
      return nullptr;
    }
    msg = event_msgs_.front();
    event_msgs_.erase(event_msgs_.begin());
  }
  return msg;
}

void EventHandler::CleanRepeatMessage(EventMessage *msg)
{
  EventMessage *tmp;
  for (auto iter = event_msgs_.cbegin();
       iter != event_msgs_.cend();) {
    tmp = (EventMessage *)*iter;
    if (msg->GetId() == tmp->GetId()) {
      iter = event_msgs_.erase(iter);
      delete tmp;
    } else {
      iter++;
    }
  }
}

void EventHandler::InsertMessage(EventMessage *msg, bool front)
{
  if (front) {
    auto iter = event_msgs_.begin();
    iter = event_msgs_.insert(iter, msg);
  } else {
    event_msgs_.push_back(msg);
  }
}

void EventHandler::NotifyToEventHandler(EventMessage *msg)
{
  bool inser_front = false;
  AutoLockMutex _rw_mtx(event_queue_mtx_);
  if (process_) {
    if (msg->GetType() == MESSAGE_TYPE_UNIQUE) {
      CleanRepeatMessage(msg);
    } else if (msg->GetType() == MESSAGE_TYPE_LIFO) {
      inser_front = true;;
    }
    InsertMessage(msg, inser_front);
  }
}

} // namespace easymedia
