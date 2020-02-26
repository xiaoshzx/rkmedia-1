// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "g711_server_media_subsession.hh"
#include "SimpleRTPSink.hh"

#include "utils.h"

namespace easymedia {

G711ServerMediaSubsession *G711ServerMediaSubsession::createNew(
    UsageEnvironment &env, Live555MediaInput &wisInput,
    unsigned samplingFrequency, unsigned numChannels, unsigned char audioFormat,
    unsigned char bitsPerSample) {
  return new G711ServerMediaSubsession(env, wisInput, samplingFrequency,
                                       numChannels, audioFormat, bitsPerSample);
}

G711ServerMediaSubsession::G711ServerMediaSubsession(
    UsageEnvironment &env, Live555MediaInput &mediaInput,
    unsigned samplingFrequency, unsigned numChannels, unsigned char audioFormat,
    unsigned char bitsPerSample)
    : OnDemandServerMediaSubsession(env, True /*reuse the first source*/),
      fMediaInput(mediaInput), fSamplingFrequency(samplingFrequency),
      fNumChannels(numChannels), fAudioFormat(audioFormat),
      fBitsPerSample(bitsPerSample) {}

G711ServerMediaSubsession::~G711ServerMediaSubsession() {
  LOG_FILE_FUNC_LINE();
}

FramedSource *
G711ServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/,
                                                 unsigned &estBitrate) {
  unsigned bitsPerSecond = fSamplingFrequency * fBitsPerSample * fNumChannels;
  estBitrate = (bitsPerSecond + 500) / 1000; // kbps
  // estBitrate = 96; // kbps, estimate
  return fMediaInput.audioSource();
}

RTPSink *G711ServerMediaSubsession::createNewRTPSink(
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
  if (fAudioFormat == WA_PCMU) {
    mimeType = "PCMU";
    if (fSamplingFrequency == 8000 && fNumChannels == 1) {
      payloadFormatCode = 0; // a static RTP payload type
    }
  } else if (fAudioFormat == WA_PCMA) {
    mimeType = "PCMA";
    if (fSamplingFrequency == 8000 && fNumChannels == 1) {
      payloadFormatCode = 8; // a static RTP payload type
    }
  } else {
    return nullptr;
  }
  return SimpleRTPSink::createNew(envir(), rtpGroupsock, payloadFormatCode,
                                  fSamplingFrequency, "audio", mimeType,
                                  fNumChannels);
}

// std::mutex G711ServerMediaSubsession::kMutex;
void G711ServerMediaSubsession::startStream(
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
void G711ServerMediaSubsession::deleteStream(unsigned clientSessionId,
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
