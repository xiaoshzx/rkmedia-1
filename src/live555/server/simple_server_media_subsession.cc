// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "simple_server_media_subsession.hh"
#include "SimpleRTPSink.hh"

#include "media_type.h"
#include "utils.h"

namespace easymedia {

SIMPLEServerMediaSubsession *SIMPLEServerMediaSubsession::createNew(
    UsageEnvironment &env, Live555MediaInput &wisInput,
    unsigned samplingFrequency, unsigned numChannels, std::string audioFormat,
    unsigned bitrate) {
  return new SIMPLEServerMediaSubsession(env, wisInput, samplingFrequency,
                                         numChannels, audioFormat, bitrate);
}

SIMPLEServerMediaSubsession::SIMPLEServerMediaSubsession(
    UsageEnvironment &env, Live555MediaInput &mediaInput,
    unsigned samplingFrequency, unsigned numChannels, std::string audioFormat,
    unsigned bitrate)
    : OnDemandServerMediaSubsession(env, True /*reuse the first source*/),
      fMediaInput(mediaInput), fSamplingFrequency(samplingFrequency),
      fNumChannels(numChannels), fAudioFormat(audioFormat), fbitrate(bitrate) {}

SIMPLEServerMediaSubsession::~SIMPLEServerMediaSubsession() {
  LOG_FILE_FUNC_LINE();
}

FramedSource *
SIMPLEServerMediaSubsession::createNewStreamSource(unsigned clientSessionId,
                                                   unsigned &estBitrate) {
  LOG_FILE_FUNC_LINE();
  if (!fSDPLines && clientSessionId != 0) {
    LOG("%s:%s:%d --- you must get sdp first.\n", __FILE__, __func__, __LINE__);
    return NULL;
  }
  estBitrate = (fbitrate + 500) / 1000; // kbps
  return fMediaInput.audioSource();
}

RTPSink *SIMPLEServerMediaSubsession::createNewRTPSink(
    Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
    FramedSource *inputSource) {
  if (!inputSource) {
    LOG("inputSource is not ready, can not create new rtp sink\n");
    return NULL;
  }
  char const *mimeType;
  unsigned char payloadFormatCode =
      rtpPayloadTypeIfDynamic; // by default, unless a static RTP payload type
                               // can be used
  if (fAudioFormat == AUDIO_G711U) {
    mimeType = "PCMU";
    if (fSamplingFrequency == 8000 && fNumChannels == 1) {
      payloadFormatCode = 0; // a static RTP payload type
    }
  } else if (fAudioFormat == AUDIO_G711A) {
    mimeType = "PCMA";
    if (fSamplingFrequency == 8000 && fNumChannels == 1) {
      payloadFormatCode = 8; // a static RTP payload type
    }
  } else if (fAudioFormat == AUDIO_G726) {
    if (fbitrate / 1000 == 16) {
      mimeType = "G726-16";
    } else if (fbitrate / 1000 == 24) {
      mimeType = "G726-24";
    } else if (fbitrate / 1000 == 32) {
      mimeType = "G726-32";
    } else if (fbitrate / 1000 == 40) {
      mimeType = "G726-40";
    } else {
      return nullptr;
    }
  } else {
    return nullptr;
  }
  return SimpleRTPSink::createNew(envir(), rtpGroupsock, payloadFormatCode,
                                  fSamplingFrequency, "audio", mimeType,
                                  fNumChannels);
}

// std::mutex SIMPLEServerMediaSubsession::kMutex;
void SIMPLEServerMediaSubsession::startStream(
    unsigned clientSessionId, void *streamToken, TaskFunc *rtcpRRHandler,
    void *rtcpRRHandlerClientData, unsigned short &rtpSeqNum,
    unsigned &rtpTimestamp,
    ServerRequestAlternativeByteHandler *serverRequestAlternativeByteHandler,
    void *serverRequestAlternativeByteHandlerClientData) {
  OnDemandServerMediaSubsession::startStream(
      clientSessionId, streamToken, rtcpRRHandler, rtcpRRHandlerClientData,
      rtpSeqNum, rtpTimestamp, serverRequestAlternativeByteHandler,
      serverRequestAlternativeByteHandlerClientData);
  // kMutex.lock();
  if (kSessionIdList.empty())
    fMediaInput.Start(envir());
  LOG("%s - clientSessionId: 0x%08x\n", __func__, clientSessionId);
  kSessionIdList.push_back(clientSessionId);
  // kMutex.unlock();
}
void SIMPLEServerMediaSubsession::deleteStream(unsigned clientSessionId,
                                               void *&streamToken) {
  // kMutex.lock();
  LOG("%s - clientSessionId: 0x%08x\n", __func__, clientSessionId);
  kSessionIdList.remove(clientSessionId);
  if (kSessionIdList.empty())
    fMediaInput.Stop(envir());
  // kMutex.unlock();
  OnDemandServerMediaSubsession::deleteStream(clientSessionId, streamToken);
}

} // namespace easymedia
