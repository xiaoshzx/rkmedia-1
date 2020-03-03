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
#include "aac_server_media_subsession.hh"
#include "live555_media_input.hh"
#include "mp2_server_media_subsession.hh"
#include "simple_server_media_subsession.hh"

#include "buffer.h"
#include "codec.h"
#include "media_config.h"
#include "media_reflector.h"
#include "media_type.h"

namespace easymedia {

static bool SendMediaToServer(Flow *f, MediaBufferVector &input_vector);

class RtspConnection {
public:
  static std::shared_ptr<RtspConnection>
  getInstance(int port, std::string username, std::string userpwd) {
    kMutex.lock();
    if (m_rtspConnection == nullptr) {
      struct make_shared_enabler : public RtspConnection {
        make_shared_enabler(int port, std::string username, std::string userpwd)
            : RtspConnection(port, username, userpwd){};
      };
      m_rtspConnection =
          std::make_shared<make_shared_enabler>(port, username, userpwd);
      if (!init_ok) {
        m_rtspConnection = nullptr;
      }
    }
    kMutex.unlock();
    return m_rtspConnection;
  }
  void addSubsession(ServerMediaSubsession *subsession,
                     std::string channel_name);
  void deleteSession(std::string channel_name);
  UsageEnvironment *getEnv() { return env; };

  ~RtspConnection();

private:
  static volatile bool init_ok;
  static volatile char out_loop_cond;

  RtspConnection(int port, std::string username, std::string userpwd);

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

RtspConnection::RtspConnection(int port, std::string username,
                               std::string userpwd)
    : scheduler(nullptr), env(nullptr), authDB(nullptr), rtspServer(nullptr),
      session_thread(nullptr) {
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

  rtspServer = RTSPServer::createNew(*env, port, authDB, 1000);

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

void RtspConnection::deleteSession(std::string channel_name) {
  kMutex.lock();
  if (rtspServer != nullptr) {
    rtspServer->deleteServerMediaSession(channel_name.c_str());
    LOG("RtspConnection delete %s.\n", channel_name.c_str());
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

  std::string channel_name;
  std::string video_type;
  friend bool SendMediaToServer(Flow *f, MediaBufferVector &input_vector);
};

bool SendMediaToServer(Flow *f, MediaBufferVector &input_vector) {
  RtspServerFlow *rtsp_flow = (RtspServerFlow *)f;

  for (auto &buffer : input_vector) {
    if (!buffer)
      continue;
    if (buffer && buffer->IsHwBuffer()) {
      // hardware buffer is limited, copy it
      auto new_buffer = MediaBuffer::Clone(*buffer.get());
      new_buffer->SetType(buffer->GetType());
      buffer = new_buffer;
    }

    if ((buffer->GetUserFlag() & MediaBuffer::kExtraIntra)) {
      std::list<std::shared_ptr<easymedia::MediaBuffer>> spspps;
      if (rtsp_flow->video_type == VIDEO_H264) {
        spspps = split_h264_separate((const uint8_t *)buffer->GetPtr(),
                                     buffer->GetValidSize(),
                                     easymedia::gettimeofday());
      } else if (rtsp_flow->video_type == VIDEO_H265) {
        spspps = split_h265_separate((const uint8_t *)buffer->GetPtr(),
                                     buffer->GetValidSize(),
                                     easymedia::gettimeofday());
      }
      for (auto &buf : spspps) {
        rtsp_flow->server_input->PushNewVideo(buf);
      }
    } else if (buffer->GetType() == Type::Audio)
      rtsp_flow->server_input->PushNewAudio(buffer);
    else if (buffer->GetType() == Type::Video)
      rtsp_flow->server_input->PushNewVideo(buffer);
    else
      LOG("#ERROR: Unknown buffer type(%d)\n", (int)buffer->GetType());
  }

  return true;
}

RtspServerFlow::RtspServerFlow(const char *param) {
  std::list<std::string> input_data_types;
  std::map<std::string, std::string> params;
  if (!parse_media_param_map(param, params)) {
    SetError(-EINVAL);
    return;
  }
  std::string value;
  CHECK_EMPTY_SETERRNO(value, params, KEY_INPUTDATATYPE, EINVAL)
  parse_media_param_list(value.c_str(), input_data_types, ',');
  CHECK_EMPTY_SETERRNO(channel_name, params, KEY_CHANNEL_NAME, EINVAL)

  value = params[KEY_PORT_NUM];
  int port = std::stoi(value);
  std::string &username = params[KEY_USERNAME];
  std::string &userpwd = params[KEY_USERPASSWORD];
  rtspConnection = RtspConnection::getInstance(port, username, userpwd);
  if (rtspConnection) {
    int in_idx = 0;
    std::string markname;
    SlotMap sm;

    server_input = Live555MediaInput::createNew(*(rtspConnection->getEnv()));
    if (!server_input)
      goto err;

    // rtspServer->addServerMediaSession(sms);
    for (auto &type : input_data_types) {
      ServerMediaSubsession *subsession = nullptr;
      if (type == VIDEO_H264) {
#ifdef LIVE555_SERVER_H264
        subsession = H264ServerMediaSubsession::createNew(
            *(rtspConnection->getEnv()), *server_input);
        video_type = VIDEO_H264;
#endif
      } else if (type == VIDEO_H265) {
#ifdef LIVE555_SERVER_H265
        subsession = H265ServerMediaSubsession::createNew(
            *(rtspConnection->getEnv()), *server_input);
        video_type = VIDEO_H265;
#endif
      } else if (type == AUDIO_AAC) {
        int sample_rate = 0, channels = 0, profiles = 0;
        value = params[KEY_SAMPLE_RATE];
        if (!value.empty())
          sample_rate = std::stoi(value);

        value = params[KEY_CHANNELS];
        if (!value.empty())
          channels = std::stoi(value);

        value = params[KEY_PROFILE];
        if (!value.empty())
          profiles = std::stoi(value);

        subsession = AACServerMediaSubsession::createNew(
            *(rtspConnection->getEnv()), *server_input, sample_rate, channels,
            profiles);
      } else if (type == AUDIO_G711A || type == AUDIO_G711U ||
                 type == AUDIO_G726) {
        int sample_rate = 0, channels = 0;
        unsigned bitrate = 0;
        value = params[KEY_SAMPLE_RATE];
        if (!value.empty())
          sample_rate = std::stoi(value);

        value = params[KEY_CHANNELS];
        if (!value.empty())
          channels = std::stoi(value);

        value = params[KEY_SAMPLE_FMT];
        if (!value.empty())
          bitrate = std::stoi(value);
        subsession = SIMPLEServerMediaSubsession::createNew(
            *(rtspConnection->getEnv()), *server_input, sample_rate, channels,
            type, bitrate);
      } else if (type == AUDIO_MP2) {
        subsession = MP2ServerMediaSubsession::createNew(
            *(rtspConnection->getEnv()), *server_input);

      } else if (string_start_withs(type, AUDIO_PREFIX)) {
        // pcm or vorbis
        LOG_TODO();
        goto err;
      } else {
        LOG("TODO, unsupport type : %s\n", type.c_str());
        // goto err;
      }
      if (subsession) {
        // goto err;
        // sms->addSubsession(subsession);
        rtspConnection->addSubsession(subsession, channel_name);
      }
      sm.input_slots.push_back(in_idx);
      in_idx++;
    }
    // sm.thread_model = Model::SYNC;
    sm.process = SendMediaToServer;
    sm.thread_model = Model::ASYNCCOMMON;
    sm.mode_when_full = InputMode::BLOCKING;
    sm.input_maxcachenum.push_back(0); // no limit
    markname = "rtsp " + channel_name + std::to_string(in_idx);
    if (!InstallSlotMap(sm, markname, 0)) {
      LOG("Fail to InstallSlotMap, %s\n", markname.c_str());
      goto err;
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
  if (rtspConnection) {
    rtspConnection->deleteSession(channel_name);
  }
  if (server_input) {
    delete server_input;
    server_input = nullptr;
  }
}

DEFINE_FLOW_FACTORY(RtspServerFlow, Flow)
const char *FACTORY(RtspServerFlow)::ExpectedInputDataType() { return ""; }
const char *FACTORY(RtspServerFlow)::OutPutDataType() { return ""; }

} // namespace easymedia
