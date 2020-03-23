// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "live555_media_input.hh"

#include <assert.h>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

#include "buffer.h"
#include "codec.h"
#include "utils.h"

namespace easymedia {
// A common "FramedSource" subclass, used for reading from a cached buffer list:

Live555MediaInput::Live555MediaInput(UsageEnvironment &env)
    : Medium(env), connecting(false), video_source(nullptr),
      audio_source(nullptr), video_callback(nullptr), audio_callback(nullptr) {}

Live555MediaInput::~Live555MediaInput() {
  LOG_FILE_FUNC_LINE();
  video_list.remove_if([](Source *s) {
    if (s->GetReadFd() < 0) {
      delete s;
      return true;
    } else {
      return false;
    }
  });

  audio_list.remove_if([](Source *s) {
    if (s->GetReadFd() < 0) {
      delete s;
      return true;
    } else {
      return false;
    }
  });
  connecting = false;
}

void Live555MediaInput::Start(UsageEnvironment &env _UNUSED) {
  connecting = true;
  LOG_FILE_FUNC_LINE();
}

void Live555MediaInput::Stop(UsageEnvironment &env _UNUSED) {
  connecting = false;
  // TODO: tell main to stop all up flow resource
  LOG_FILE_FUNC_LINE();
}

Live555MediaInput *Live555MediaInput::createNew(UsageEnvironment &env) {
  return new Live555MediaInput(env);
}

#define MAX_CACHE_NUMBER 60
static void common_reduction(std::list<std::shared_ptr<MediaBuffer>> &mb_list) {
  if (mb_list.size() > MAX_CACHE_NUMBER) {
    for (int i = 0; i < MAX_CACHE_NUMBER / 2; i++)
      mb_list.pop_front();
    LOG("call common_reduction.\n");
  }
}

static void
h264_packet_reduction(std::list<std::shared_ptr<MediaBuffer>> &mb_list) {
  if (mb_list.size() < MAX_CACHE_NUMBER)
    return;
  // only remain one I frame
  auto i = mb_list.rbegin();
  for (; i != mb_list.rend(); ++i) {
    auto &b = *i;
    if (b->GetUserFlag() & MediaBuffer::kIntra)
      break;
  }
  if (i == mb_list.rend())
    return;
  auto iter = mb_list.begin();
  for (; iter != mb_list.end(); ++iter) {
    auto &b = *iter;
    if (!(b->GetUserFlag() & MediaBuffer::kExtraIntra))
      break;
  }
  LOG("h264 reduction before, num: %d \n", (int)mb_list.size());
  mb_list.erase(iter, (++i).base());
  LOG("h264 reduction after, num: %d \n", (int)mb_list.size());
}

FramedSource *Live555MediaInput::videoSource() {
  // if (!video_source)
  Source *source = new Source();
  if (!source)
    return nullptr;
  if (!source->Init(h264_packet_reduction)) {
    delete source;
    return nullptr;
  }
  video_list.push_back(source);
  video_source = new VideoFramedSource(envir(), *source);
  return video_source;
}

FramedSource *Live555MediaInput::audioSource() {
  Source *source = new Source();
  if (!source)
    return nullptr;
  if (!source->Init(common_reduction)) {
    delete source;
    return nullptr;
  }
  audio_list.push_back(source);
  audio_source = new AudioFramedSource(envir(), *source);
  return audio_source;
}

#if 0
static void printErr(UsageEnvironment& env, char const* str = NULL) {
  if (str != NULL)
    env << str;
  env << ": " << strerror(env.getErrno()) << "\n";
}
#endif

void Live555MediaInput::PushNewVideo(std::shared_ptr<MediaBuffer> &buffer) {
  if (!buffer)
    return;
  /*
  if ((connecting && video_source != nullptr) ||
      (buffer->GetUserFlag() & MediaBuffer::kExtraIntra))
    vs->Push(buffer);*/
  video_list.remove_if([](Source *s) {
    if (s->GetReadFd() < 0) {
      delete s;
      return true;
    } else {
      return false;
    }
  });

  for (auto video : video_list) {
    if (video) {
      if (video->GetReadFd() >= 0) {
        video->Push(buffer);
      }
    }
  }
}

void Live555MediaInput::PushNewAudio(std::shared_ptr<MediaBuffer> &buffer) {
  if (!buffer)
    return;
  /*
  if (connecting && audio_source != nullptr)
    as->Push(buffer);
    */
  audio_list.remove_if([](Source *s) {
    if (s->GetReadFd() < 0) {
      delete s;
      return true;
    } else {
      return false;
    }
  });
  for (auto audio : audio_list) {
    if (audio) {
      if (audio->GetReadFd() >= 0) {
        audio->Push(buffer);
      }
    }
  }
}

void Live555MediaInput::SetStartVideoStreamCallback(
    const StartStreamCallback &cb) {
  AutoLockMutex _alm(video_callback_mtx);
  video_callback = cb;
}
StartStreamCallback Live555MediaInput::GetStartVideoStreamCallback() {
  AutoLockMutex _alm(video_callback_mtx);
  return video_callback;
}
void Live555MediaInput::SetStartAudioStreamCallback(
    const StartStreamCallback &cb) {
  AutoLockMutex _alm(audio_callback_mtx);
  audio_callback = cb;
}
StartStreamCallback Live555MediaInput::GetStartAudioStreamCallback() {
  AutoLockMutex _alm(audio_callback_mtx);
  return audio_callback;
}

Source::Source() : reduction(nullptr) {
  wakeFds[0] = wakeFds[1] = -1;
  LOG("Source :: %p.\n", this);
}

void Source::CloseReadFd() {
  if (wakeFds[0] >= 0) {
    ::close(wakeFds[0]);
    wakeFds[0] = -1;
  }
}

Source::~Source() {
  if (wakeFds[0] >= 0) {
    ::close(wakeFds[0]);
    wakeFds[0] = -1;
  }
  if (wakeFds[1] >= 0) {
    ::close(wakeFds[1]);
    wakeFds[1] = -1;
  }
  LOG("~Source::%p remain %d buffers, will auto release\n", this,
      (int)cached_buffers.size());
}

bool Source::Init(ListReductionPtr func) {
  // create pipe fds
  int ret = pipe2(wakeFds, O_CLOEXEC);
  if (ret) {
    LOG("pipe2 failed: %m\n");
    return false;
  }
  assert(wakeFds[0] >= 0 && wakeFds[1] >= 0);
  reduction = func;
  return true;
}

void Source::Push(std::shared_ptr<MediaBuffer> &buffer) {
  AutoLockMutex _alm(mtx);
  if (reduction)
    reduction(cached_buffers);
  cached_buffers.push_back(buffer);
  // mtx.notify();
  int i = 0;
  write(wakeFds[1], &i, sizeof(i));
}

std::shared_ptr<MediaBuffer> Source::Pop() {
  AutoLockMutex _alm(mtx);
  if (cached_buffers.empty())
    return nullptr;
  auto buffer = cached_buffers.front();
  cached_buffers.pop_front();
  return std::move(buffer);
}

void ListSource::doGetNextFrame() {
  assert(fSource.GetReadFd() >= 0);
  // Await the next incoming data on our FID:
  envir().taskScheduler().turnOnBackgroundReadHandling(
      fSource.GetReadFd(),
      (TaskScheduler::BackgroundHandlerProc *)&incomingDataHandler, this);
}

void ListSource::doStopGettingFrames() {
  LOG_FILE_FUNC_LINE();
  FramedSource::doStopGettingFrames();
}

void ListSource::incomingDataHandler(ListSource *source, int /*mask*/) {
  source->incomingDataHandler1();
}

void ListSource::incomingDataHandler1() {
  // Read the data from our file into the client's buffer:
  readFromList();

  assert(fSource.GetReadFd() >= 0);
  // Stop handling any more input, until we're ready again:
  envir().taskScheduler().turnOffBackgroundReadHandling(fSource.GetReadFd());

  // Tell our client that we have new data:
  afterGetting(this);
}

bool ListSource::readFromList(bool flush _UNUSED) { return false; }

void ListSource::flush() {
  readFromList(true);
  fFrameSize = 0;
  fNumTruncatedBytes = 0;
}

VideoFramedSource::VideoFramedSource(UsageEnvironment &env, Source &source)
    : ListSource(env, source), got_iframe(false) {
  // fReadFd = input.vs->GetReadFd();
}

VideoFramedSource::~VideoFramedSource() {
  LOG_FILE_FUNC_LINE();
  // fInput.video_source = NULL;
}

bool VideoFramedSource::readFromList(bool flush _UNUSED) {
#ifdef DEBUG_SEND
  fprintf(stderr, "$$$$ %s, %d\n", __func__, __LINE__);
#endif
  std::shared_ptr<MediaBuffer> buffer;
  int i = 0;
  ssize_t read_size = (ssize_t)sizeof(i);
  ssize_t ret = read(fSource.GetReadFd(), &i, sizeof(i));
  if (ret != read_size) {
    LOG("%s:%d, read from pipe error, %m\n", __func__, __LINE__);
    envir() << __LINE__ << " read from pipe error: " << errno << "\n";
    goto err;
  }

  buffer = fSource.Pop();
  if (buffer) {
    if (!got_iframe) {
      got_iframe = buffer->GetUserFlag() & MediaBuffer::kIntra;
      if (!got_iframe && !(buffer->GetUserFlag() & MediaBuffer::kExtraIntra))
        goto err;
    }
    fPresentationTime = buffer->GetTimeVal();
// gettimeofday(&fPresentationTime, NULL);
#ifdef DEBUG_SEND
    fprintf(stderr, "video frame time: %ld, %ld.\n", fPresentationTime.tv_sec,
            fPresentationTime.tv_usec);
#endif
    fFrameSize = buffer->GetValidSize();
#ifdef DEBUG_SEND
    envir() << "video frame size: " << fFrameSize << "\n";
#endif
    assert(fFrameSize > 0);
    uint8_t *p = (uint8_t *)buffer->GetPtr();
    assert(p[0] == 0);
    assert(p[1] == 0);
    if (p[2] == 0) {
      assert(p[3] == 1);
      read_size = 4;
    } else {
      assert(p[2] == 1);
      read_size = 3;
    }
    fFrameSize -= read_size;
    if (fFrameSize > fMaxSize) {
      LOG("%s : %d, fFrameSize(%d) > fMaxSize(%d)\n", __func__, __LINE__,
          fFrameSize, fMaxSize);
      fNumTruncatedBytes = fFrameSize - fMaxSize;
      fFrameSize = fMaxSize;
    } else {
      fNumTruncatedBytes = 0;
    }
    memcpy(fTo, p + read_size, fFrameSize);
    return true;
  }

err:
  fFrameSize = 0;
  fNumTruncatedBytes = 0;
  return false;
}

AudioFramedSource::AudioFramedSource(UsageEnvironment &env, Source &source)
    : ListSource(env, source) {
  // fReadFd = input.as->GetReadFd();
}

AudioFramedSource::~AudioFramedSource() {
  LOG_FILE_FUNC_LINE();
  // fInput.audio_source = NULL;
}

bool AudioFramedSource::readFromList(bool flush _UNUSED) {
#ifdef DEBUG_SEND
  fprintf(stderr, "$$$$ %s, %d\n", __func__, __LINE__);
#endif
  std::shared_ptr<MediaBuffer> buffer;
  uint8_t *p;

  int i = 0;
  ssize_t read_size = (ssize_t)sizeof(i);
  ssize_t ret = read(fSource.GetReadFd(), &i, sizeof(i));
  if (ret != read_size) {
    LOG("%s:%d, read from pipe error, %m\n", __func__, __LINE__);
    envir() << __LINE__ << " read from pipe error: " << errno << "\n";
    goto err;
  }

  buffer = fSource.Pop();
  if (buffer) {
    p = (uint8_t *)buffer->GetPtr();
    fPresentationTime = buffer->GetTimeVal();
#ifdef DEBUG_SEND
    fprintf(stderr, "audio frame time: %ld, %ld.\n", fPresentationTime.tv_sec,
            fPresentationTime.tv_usec);
#endif
    fFrameSize = buffer->GetValidSize();
#ifdef DEBUG_SEND
    envir() << "audio frame size: " << fFrameSize << "\n";
#endif
    assert(fFrameSize > 0);
    if (fFrameSize > fMaxSize) {
      LOG("%s : %d, fFrameSize(%d) > fMaxSize(%d)\n", __func__, __LINE__,
          fFrameSize, fMaxSize);
      fNumTruncatedBytes = fFrameSize - fMaxSize;
      fFrameSize = fMaxSize;
    } else {
      fNumTruncatedBytes = 0;
    }
    memcpy(fTo, p, fFrameSize);
    return true;
  }

err:
  fFrameSize = 0;
  fNumTruncatedBytes = 0;

#if 0
  assert(fFileNo > 0);
#ifdef DEBUG_SEND
  fprintf(stderr, "$$$$ %s, %d\n", __func__, __LINE__);
#endif
  do {
    // Note the timestamp and size:
    ssize_t read_size = (ssize_t)sizeof(struct timeval);
    ssize_t ret = read(fFileNo, &fPresentationTime, read_size);
    if (ret != read_size) {
      envir() << __LINE__ << "read from pipe error: " << errno << "\n";
      break;
    }
#ifdef DEBUG_SEND
    envir() << "audio read pipe frame time: " << (int)fPresentationTime.tv_sec
            << "s, " << (int)fPresentationTime.tv_usec << "us. \n";
#endif
    assert(sizeof(fFrameSize) >= sizeof(unsigned int));
    read_size = sizeof(unsigned int);
    ret = read(fFileNo, &fFrameSize, read_size);
    if (ret != read_size) {
      envir() << __LINE__ << "read from pipe error: " << errno << "\n";
      break;
    }
    assert(fFrameSize > 0);
#ifdef DEBUG_SEND
    envir() << "audio read pipe size: " << fFrameSize << "\n";
#endif
    if (flush) {
      char tmp[512];
      while (fFrameSize > 0) {
        read_size = std::min(sizeof(tmp), fFrameSize);
        ret = read(fFileNo, tmp, read_size);
        if (ret != read_size && errno != EAGAIN) {
          envir() << __LINE__ << "read from pipe error: " << errno << "\n";
          break;
        }
        fFrameSize -= ret;
      }
      break;
    }
    if (fFrameSize > fMaxSize) {
      fNumTruncatedBytes = fFrameSize - fMaxSize;
      fFrameSize = fMaxSize;
    } else {
      fNumTruncatedBytes = 0;
    }
    ret = read(fFileNo, fTo, fFrameSize);
    if ((unsigned)ret != fFrameSize) {
      envir() << __LINE__ << "read from pipe error: " << errno << "\n";
      break;
    }
#ifdef DEBUG_SEND
    fprintf(stderr, "$$$$ %s, %d\n", __func__, __LINE__);
#endif
    return true;
  } while (0);
  fFrameSize = 0;
  fNumTruncatedBytes = 0;
#endif
  return false;
}

} // namespace easymedia
