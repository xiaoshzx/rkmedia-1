// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_MESSAGE_H_
#define EASYMEDIA_MESSAGE_H_

#include <stdint.h>
#include <string.h>
#include <sys/time.h>
#include <vector>
#include <thread>

#include "lock.h"

namespace easymedia {


enum EventMessageType {
  MESSAGE_TYPE_FIFO = 0,
  MESSAGE_TYPE_LIFO,
  MESSAGE_TYPE_UNIQUE
};

class EventMessage;
void EventMsgAssign(EventMessage &, const EventMessage &);
bool operator==(const EventMessage &, const EventMessage &);
bool operator!=(const EventMessage &, const EventMessage &);

class Flow;
class EventMessage {
public:
  EventMessage()
    : sender_(nullptr), id_(0), param_(0), params_(nullptr), type_(0) {}
  EventMessage(void *sender, int id, int param,
                     void *params = nullptr, int type = 0)
    : sender_(sender), id_(id), param_(param)
    , params_(params), type_(type) {}
  ~EventMessage(){}

  friend void EventMsgAssign(EventMessage &, const EventMessage &);
  friend bool operator==(const EventMessage &, const EventMessage &);
  friend bool operator!=(const EventMessage &, const EventMessage &);

  void * GetSender() { return sender_; }
  int GetId() { return id_; }
  int GetParam() { return param_; }
  void * GetParams() { return params_; }
  int GetType() { return type_; }

private:
  void *sender_;
  int id_;
  int param_;
  void *params_;
  int type_;
};

typedef int (* EventHook)(std::shared_ptr<Flow>flow, bool &loop);
typedef std::vector<EventMessage> MessageQueue;
typedef std::vector<EventMessage *> PMessageQueue;

class EventHandler {
public:
  EventHandler() {}
  virtual ~EventHandler() {}

  void RegisterEventHook(std::shared_ptr<Flow>flow, EventHook proc);
  void UnRegisterEventHook();
  void EventHookWait();
  void SignalEventHook();

  void CleanRepeatMessage(EventMessage *msg);
  void InsertMessage(EventMessage *msg, bool front = false);
  EventMessage * GetEventMessages();
  void NotifyToEventHandler(EventMessage *msg);

public:

private:
  EventHook process_;
  bool event_thread_loop_;
  std::thread *event_thread_;
  PMessageQueue event_msgs_;
  ConditionLockMutex event_cond_mtx_;
  ReadWriteLockMutex event_queue_mtx_;
};

class AutoMessage {
public:
  AutoMessage() = delete;
  AutoMessage(EventMessage *msg) : msg_(msg) { }
  ~AutoMessage()
  {
    if(msg_ != nullptr) {
      delete msg_;
      msg_ = nullptr;
    }
  }
  EventMessage *GetMessage(){ return msg_;}
private:
  EventMessage *msg_;
};

} // namespace easymedia

#endif // #ifndef EASYMEDIA_FLOW_H_

