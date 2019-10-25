// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "h265_server_media_subsession.hh"

#include <liveMedia/H265VideoRTPSink.hh>
#include <liveMedia/H265VideoStreamDiscreteFramer.hh>

#include "utils.h"

namespace easymedia {
H265ServerMediaSubsession *
H265ServerMediaSubsession::createNew(UsageEnvironment &env,
                                     Live555MediaInput &wisInput) {
  return new H265ServerMediaSubsession(env, wisInput);
}

H265ServerMediaSubsession::H265ServerMediaSubsession(
    UsageEnvironment &env, Live555MediaInput &mediaInput)
    : OnDemandServerMediaSubsession(env, True /*reuse the first source*/),
      fMediaInput(mediaInput), fEstimatedKbps(1000), fDoneFlag(0),
      fDummyRTPSink(NULL), fGetSdpTimeOut(1000 * 10), sdpState(INITIAL) {}

H265ServerMediaSubsession::~H265ServerMediaSubsession() {
  LOG_FILE_FUNC_LINE();
}

std::mutex H265ServerMediaSubsession::kMutex;
std::list<unsigned int> H265ServerMediaSubsession::kSessionIdList;

void H265ServerMediaSubsession::startStream(
    unsigned clientSessionId, void *streamToken, TaskFunc *rtcpRRHandler,
    void *rtcpRRHandlerClientData, unsigned short &rtpSeqNum,
    unsigned &rtpTimestamp,
    ServerRequestAlternativeByteHandler *serverRequestAlternativeByteHandler,
    void *serverRequestAlternativeByteHandlerClientData) {
  OnDemandServerMediaSubsession::startStream(
      clientSessionId, streamToken, rtcpRRHandler, rtcpRRHandlerClientData,
      rtpSeqNum, rtpTimestamp, serverRequestAlternativeByteHandler,
      serverRequestAlternativeByteHandlerClientData);
  kMutex.lock();
  if (kSessionIdList.empty())
    fMediaInput.Start(envir());
  LOG("%s - clientSessionId: 0x%08x\n", __func__, clientSessionId);
  kSessionIdList.push_back(clientSessionId);
  kMutex.unlock();
}
void H265ServerMediaSubsession::deleteStream(unsigned clientSessionId,
                                             void *&streamToken) {
  kMutex.lock();
  LOG("%s - clientSessionId: 0x%08x\n", __func__, clientSessionId);
  kSessionIdList.remove(clientSessionId);
  if (kSessionIdList.empty())
    fMediaInput.Stop(envir());
  kMutex.unlock();
  OnDemandServerMediaSubsession::deleteStream(clientSessionId, streamToken);
}

static void afterPlayingDummy(void *clientData) {
  H265ServerMediaSubsession *subsess = (H265ServerMediaSubsession *)clientData;
  LOG("%s, set done.\n", __func__);
  // Signal the event loop that we're done:
  subsess->setDoneFlag();
}

static void checkForAuxSDPLine(void *clientData) {
  H265ServerMediaSubsession *subsess = (H265ServerMediaSubsession *)clientData;
  subsess->checkForAuxSDPLine1();
}

void H265ServerMediaSubsession::checkForAuxSDPLine1() {
  LOG("** fDoneFlag: %d, fGetSdpTimeOut: %d **\n", fDoneFlag, fGetSdpTimeOut);
  if (fDummyRTPSink->auxSDPLine() != NULL) {
    // Signal the event loop that we're done:
    LOG("%s, set done.\n", __func__);
    setDoneFlag();
  } else {
    if (fGetSdpTimeOut <= 0) {
      LOG("warning: get sdp time out, set done.\n");
      sdpState = GET_SDP_LINES_TIMEOUT;
      setDoneFlag();
    } else {
      // try again after a brief delay:
      int uSecsToDelay = 100000; // 100 ms
      nextTask() = envir().taskScheduler().scheduleDelayedTask(
          uSecsToDelay, (TaskFunc *)checkForAuxSDPLine, this);
      fGetSdpTimeOut -= uSecsToDelay;
    }
  }
}

char const *
H265ServerMediaSubsession::getAuxSDPLine(RTPSink *rtpSink,
                                         FramedSource *inputSource) {
  // Note: For MPEG-4 video buffer, the 'config' information isn't known
  // until we start reading the Buffer.  This means that "rtpSink"s
  // "auxSDPLine()" will be NULL initially, and we need to start reading
  // data from our buffer until this changes.
  sdpState = GETTING_SDP_LINES;
  fDoneFlag = 0;
  fDummyRTPSink = rtpSink;
  fGetSdpTimeOut = 100000 * 10;
  // Start reading the buffer:
  fDummyRTPSink->startPlaying(*inputSource, afterPlayingDummy, this);
  LOG_FILE_FUNC_LINE();
  // Check whether the sink's 'auxSDPLine()' is ready:
  checkForAuxSDPLine(this);

  envir().taskScheduler().doEventLoop(&fDoneFlag);
  LOG_FILE_FUNC_LINE();
  char const *auxSDPLine = fDummyRTPSink->auxSDPLine();
  LOG("++ auxSDPLine: %p\n", auxSDPLine);
  if (auxSDPLine)
    sdpState = GOT_SDP_LINES;
  return auxSDPLine;
}

char const *H265ServerMediaSubsession::sdpLines() {
  if (!fSDPLines)
    sdpState = INITIAL;
  char const *ret = OnDemandServerMediaSubsession::sdpLines();
  if (sdpState == GET_SDP_LINES_TIMEOUT) {
    if (fSDPLines) {
      delete[] fSDPLines;
      fSDPLines = NULL;
    }
    ret = NULL;
  }
  return ret;
}

FramedSource *
H265ServerMediaSubsession::createNewStreamSource(unsigned /*clientSessionId*/,
                                                 unsigned &estBitrate) {
  estBitrate = fEstimatedKbps;
  if (sdpState == GETTING_SDP_LINES || sdpState == GET_SDP_LINES_TIMEOUT) {
    LOG("sdpline is not ready, can not create new stream source\n");
    return NULL;
  }
  LOG_FILE_FUNC_LINE();
  // Create a framer for the Video Elementary Stream:
  FramedSource *source = H265VideoStreamDiscreteFramer::createNew(
      envir(), fMediaInput.videoSource());
  LOG("h265 framedsource : %p\n", source);
  return source;
}

RTPSink *H265ServerMediaSubsession::createNewRTPSink(
    Groupsock *rtpGroupsock, unsigned char rtpPayloadTypeIfDynamic,
    FramedSource *inputSource) {
  if (!inputSource) {
    LOG("inputSource is not ready, can not create new rtp sink\n");
    return NULL;
  }
  setVideoRTPSinkBufferSize();
  LOG_FILE_FUNC_LINE();
  RTPSink *rtp_sink = H265VideoRTPSink::createNew(envir(), rtpGroupsock,
                                                  rtpPayloadTypeIfDynamic);
  LOG("h265 rtp sink : %p\n", rtp_sink);
  return rtp_sink;
}
} // namespace easymedia
