// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flow.h"

#include <time.h>

#include <mutex>

#include <BasicUsageEnvironment/BasicUsageEnvironment.hh>
#ifndef _RTSP_SERVER_HH
#include <liveMedia/RTSPServer.hh>
#endif

#if !defined(LIVE555_SERVER_H264) && !defined(LIVE555_SERVER_H265)
#error                                                                         \
    "This RTSP !VIDEO! implementation currently only support at least one of h264 and h265!!!"
#endif

#ifdef LIVE555_SERVER_H264
#include "h264_server_media_subsession.hh"
#endif
#ifdef LIVE555_SERVER_H265
#include "h265_server_media_subsession.hh"
#endif
#include "live555_media_input.hh"

#include "buffer.h"
#include "media_config.h"
#include "media_reflector.h"
#include "media_type.h"

namespace easymedia {

static bool SendVideoToServer(Flow *f, MediaBufferVector &input_vector);
// static bool SendAudioToServer(Flow *f, MediaBufferVector &input_vector);

class RtspConnection {
public:
  static std::shared_ptr<RtspConnection>
  getInstance(int ports[], std::string username, std::string userpwd) {
    kMutex.lock();
    if (m_rtspConnection == nullptr) {
      struct make_shared_enabler : public RtspConnection {
        make_shared_enabler(int ports[], std::string username,
                            std::string userpwd)
            : RtspConnection(ports, username, userpwd){};
      };
      m_rtspConnection =
          std::make_shared<make_shared_enabler>(ports, username, userpwd);
      if (!init_ok) {
        m_rtspConnection = nullptr;
      }
    }
    kMutex.unlock();
    return m_rtspConnection;
  }
  void addSubsession(ServerMediaSubsession *subsession,
                     std::string channel_name);
  UsageEnvironment *getEnv() { return env; };

  ~RtspConnection();

private:
  static volatile bool init_ok;
  static volatile char out_loop_cond;

  RtspConnection(int ports[], std::string username, std::string userpwd);

  void service_session_run();
  static std::mutex kMutex;
  static std::shared_ptr<RtspConnection> m_rtspConnection;

  TaskScheduler *scheduler;
  UsageEnvironment *env;
  UserAuthenticationDatabase *authDB;
  RTSPServer *rtspServer;
  std::thread *session_thread;
};

std::mutex RtspConnection::kMutex;
std::shared_ptr<RtspConnection> RtspConnection::m_rtspConnection = nullptr;
volatile bool RtspConnection::init_ok = false;
volatile char RtspConnection::out_loop_cond = 1;

RtspConnection::RtspConnection(int ports[], std::string username,
                               std::string userpwd)
    : scheduler(nullptr), env(nullptr), authDB(nullptr), rtspServer(nullptr),
      session_thread(nullptr) {
  int idx = 0;
  if (!username.empty() && !userpwd.empty()) {
    authDB = new UserAuthenticationDatabase;
    if (!authDB) {
      goto err;
    }
    authDB->addUserRecord(username.c_str(), userpwd.c_str());
  }
  scheduler = BasicTaskScheduler::createNew();
  if (!scheduler) {
    goto err;
  }
  env = BasicUsageEnvironment::createNew(*scheduler);
  if (!env) {
    goto err;
  }
  while (idx < 3 && !rtspServer) {
    int port = ports[idx++];
    if (port <= 0)
      continue;
    rtspServer = RTSPServer::createNew(*env, port, authDB, 1000);
  }
  if (!rtspServer) {
    goto err;
  }
  out_loop_cond = 0;
  session_thread = new std::thread(&RtspConnection::service_session_run, this);
  if (!session_thread) {
    LOG_NO_MEMORY();
    goto err;
  }
  init_ok = true;
  return;
err:
  LOG("=============== RtspConnection error. =================\n");
  init_ok = false;
}

void RtspConnection::service_session_run() {
  AutoPrintLine apl(__func__);
  LOG("================ service_session_run =================\n");
  env->taskScheduler().doEventLoop(&out_loop_cond);
}

void RtspConnection::addSubsession(ServerMediaSubsession *subsession,
                                   std::string channel_name) {
  kMutex.lock();
  ServerMediaSession *sms;
  sms = rtspServer->lookupServerMediaSession(channel_name.c_str());

  if (!sms) {
    time_t t;
    t = time(&t);
    sms =
        ServerMediaSession::createNew(*(env), channel_name.c_str(), ctime(&t),
                                      "rtsp stream server", False /*UNICAST*/);
    if (rtspServer != nullptr && sms != nullptr) {
      char *url = nullptr;
      rtspServer->addServerMediaSession(sms);
      url = rtspServer->rtspURL(sms);
      *env << "Play this stream using the URL:\n\t" << url << "\n";
      if (url)
        delete[] url;
    }
  }

  if (!sms) {
    *(env) << "Error: Failed to create ServerMediaSession: "
           << env->getResultMsg() << "\n";
  } else {
    sms->addSubsession(subsession);
  }

  kMutex.unlock();
}

RtspConnection::~RtspConnection() {
  out_loop_cond = 1;
  if (session_thread) {
    session_thread->join();
    delete session_thread;
    session_thread = nullptr;
  }
  if (rtspServer) {
    // will also reclaim ServerMediaSession and ServerMediaSubsessions
    Medium::close(rtspServer);
    rtspServer = nullptr;
  }
  if (authDB) {
    delete authDB;
    authDB = nullptr;
  }
  if (env && env->reclaim() == True)
    env = nullptr;
  if (scheduler) {
    delete scheduler;
    scheduler = nullptr;
  }
}

class RtspServerFlow : public Flow {
public:
  RtspServerFlow(const char *param);
  virtual ~RtspServerFlow();
  static const char *GetFlowName() { return "live555_rtsp_server"; }

private:
  Live555MediaInput *server_input;
  std::shared_ptr<RtspConnection> rtspConnection;

  friend bool SendVideoToServer(Flow *f, MediaBufferVector &input_vector);
  // friend bool SendAudioToServer(Flow *f, MediaBufferVector &input_vector);
};

bool SendVideoToServer(Flow *f, MediaBufferVector &input_vector) {
  RtspServerFlow *rtsp_flow = (RtspServerFlow *)f;
  auto &buffer = input_vector[0];
  if (buffer && buffer->IsHwBuffer()) {
    // hardware buffer is limited, copy it
    auto new_buffer = MediaBuffer::Clone(*buffer.get());
    buffer = new_buffer;
  }
  rtsp_flow->server_input->PushNewVideo(buffer);
  return true;
}

#if 0
bool SendAudioToServer(Flow *f, MediaBufferVector &input_vector) {
  RtspServerFlow *rtsp_flow = (RtspServerFlow *)f;
  rtsp_flow->server_input->PushNewAudio(input_vector[1]);
  return true;
}
#endif

RtspServerFlow::RtspServerFlow(const char *param) {
  std::list<std::string> input_data_types;
  std::string channel_name;
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }
  std::string value;
  CHECK_EMPTY_SETERRNO(value, params, KEY_INPUTDATATYPE, EINVAL)
  parse_media_param_list(value.c_str(), input_data_types, ',');
  CHECK_EMPTY_SETERRNO(channel_name, params, KEY_CHANNEL_NAME, EINVAL)
  int ports[3] = {0, 554, 8554};
  value = params[KEY_PORT_NUM];
  if (!value.empty()) {
    int port = std::stoi(value);
    if (port != 554 && port != 8554)
      ports[0] = port;
  }
  std::string &username = params[KEY_USERNAME];
  std::string &userpwd = params[KEY_USERPASSWORD];
  rtspConnection = RtspConnection::getInstance(ports, username, userpwd);

  server_input = Live555MediaInput::createNew(*(rtspConnection->getEnv()));
  if (!server_input)
    goto err;

  if (rtspConnection) {
    int in_idx = 0;
    std::string markname;

    // rtspServer->addServerMediaSession(sms);
    for (auto &type : input_data_types) {
      SlotMap sm;
      ServerMediaSubsession *subsession = nullptr;
      if (type == VIDEO_H264) {
#ifdef LIVE555_SERVER_H264
        subsession = H264ServerMediaSubsession::createNew(
            *(rtspConnection->getEnv()), *server_input);
#endif
        sm.process = SendVideoToServer;
      } else if (type == VIDEO_H265) {
#ifdef LIVE555_SERVER_H265
        subsession = H265ServerMediaSubsession::createNew(
            *(rtspConnection->getEnv()), *server_input);
#endif
        sm.process = SendVideoToServer;
      } else if (string_start_withs(type, AUDIO_PREFIX)) {
        // pcm or vorbis
        LOG_TODO();
        goto err;
      } else {
        LOG("TODO, unsupport type : %s\n", type.c_str());
        goto err;
      }
      if (!subsession)
        goto err;
      sm.input_slots.push_back(in_idx); // video
      sm.thread_model = Model::SYNC;
      sm.mode_when_full = InputMode::BLOCKING;
      sm.input_maxcachenum.push_back(0); // no limit
      markname = "rtsp " + channel_name + std::to_string(in_idx);
      if (!InstallSlotMap(sm, markname, 0)) {
        LOG("Fail to InstallSlotMap, %s\n", markname.c_str());
        goto err;
      }
      // sms->addSubsession(subsession);
      rtspConnection->addSubsession(subsession, channel_name);
      in_idx++;
    }
  } else {
    goto err;
  }

  *(rtspConnection->getEnv()) << "...rtsp done initializing\n";

  return;
err:
  SetError(-EINVAL);
}

RtspServerFlow::~RtspServerFlow() {
  AutoPrintLine apl(__func__);
  StopAllThread();
  SetDisable();
  if (server_input) {
    delete server_input;
    server_input = nullptr;
  }
}

DEFINE_FLOW_FACTORY(RtspServerFlow, Flow)
const char *FACTORY(RtspServerFlow)::ExpectedInputDataType() { return ""; }
const char *FACTORY(RtspServerFlow)::OutPutDataType() { return ""; }

} // namespace easymedia
