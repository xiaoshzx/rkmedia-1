// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_LIVE555_MEDIA_INPUT_HH_
#define EASYMEDIA_LIVE555_MEDIA_INPUT_HH_

#include <list>
#include <memory>
#include <type_traits>

#include <liveMedia/MediaSink.hh>

#include "lock.h"

namespace easymedia {

class MediaBuffer;
class VideoFramedSource;
class AudioFramedSource;
using ListReductionPtr = std::add_pointer<void(
    std::list<std::shared_ptr<MediaBuffer>> &mb_list)>::type;
class Live555MediaInput : public Medium {
public:
  static Live555MediaInput *createNew(UsageEnvironment &env);
  virtual ~Live555MediaInput();
  FramedSource *videoSource();
  FramedSource *audioSource();
  void Start(UsageEnvironment &env);
  void Stop(UsageEnvironment &env);

  void PushNewVideo(std::shared_ptr<MediaBuffer> &buffer);
  void PushNewAudio(std::shared_ptr<MediaBuffer> &buffer);

private:
  Live555MediaInput(UsageEnvironment &env);
  Boolean initialize(UsageEnvironment &env);
  Boolean initAudio(UsageEnvironment &env);
  Boolean initVideo(UsageEnvironment &env);

  class Source {
  public:
    Source();
    ~Source();
    bool Init(ListReductionPtr func = nullptr);
    void Push(std::shared_ptr<easymedia::MediaBuffer> &);
    std::shared_ptr<MediaBuffer> Pop();
    int GetReadFd() { return wakeFds[0]; }
    int GetWriteFd() { return wakeFds[1]; }

  private:
    std::list<std::shared_ptr<MediaBuffer>> cached_buffers;
    ConditionLockMutex mtx;
    ListReductionPtr reduction;
    int wakeFds[2]; // Live555's EventTrigger is poor for multithread, use fds
  };
  std::shared_ptr<Source> vs, as;

  volatile bool connecting;
  VideoFramedSource *video_source;
  AudioFramedSource *audio_source;

  friend class VideoFramedSource;
  friend class AudioFramedSource;
};

// Functions to set the optimal buffer size for RTP sink objects.
// These should be called before each RTPSink is created.
#define AUDIO_MAX_FRAME_SIZE 204800
#define VIDEO_MAX_FRAME_SIZE (1920 * 1080 * 2)
inline void setAudioRTPSinkBufferSize() {
  OutPacketBuffer::maxSize = AUDIO_MAX_FRAME_SIZE;
}
inline void setVideoRTPSinkBufferSize() {
  OutPacketBuffer::maxSize = VIDEO_MAX_FRAME_SIZE;
}

} // namespace easymedia

#endif // #ifndef EASYMEDIA_LIVE555_MEDIA_INPUT_HH_
