// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __MUXER_FLOW_H__
#define __MUXER_FLOW_H__

#include <sys/time.h>

#include "buffer.h"
#include "flow.h"
#include "muxer.h"
#include "utils.h"

#include "fcntl.h"
#include "stdint.h"
#include "stdio.h"
#include "unistd.h"

namespace easymedia {

class VideoRecorder;

static bool save_buffer(Flow *f, MediaBufferVector &input_vector);
static int muxer_buffer_callback(void *handler, uint8_t *buf, int buf_size);

class MuxerFlow : public Flow {
  friend VideoRecorder;

public:
  MuxerFlow(const char *param);
  virtual ~MuxerFlow();
  static const char *GetFlowName() { return "muxer_flow"; }

  virtual int Control(unsigned long int request, ...) final;

  void StartStream();
  void StopStream();

private:
  std::shared_ptr<VideoRecorder> NewRecoder(const char *path);
  friend bool save_buffer(Flow *f, MediaBufferVector &input_vector);
  friend int muxer_buffer_callback(void *handler, uint8_t *buf, int buf_size);

private:
  std::shared_ptr<MediaBuffer> video_extra;
  std::string muxer_param;
  std::string file_prefix;
  std::string file_path;
  std::string output_format;       // ffmpeg customio output format.
  std::string ffmpeg_avdictionary; // examples: key1-value,key2-value,key3-value
  std::shared_ptr<VideoRecorder> video_recorder;
  MediaConfig vid_enc_config;
  MediaConfig aud_enc_config;
  bool video_in;
  bool audio_in;
  int64_t file_duration;
  int64_t file_index;
  int64_t last_ts;
  bool file_time_en;
  bool is_use_customio;
  std::string GenFilePath();
  bool enable_streaming;
};

class VideoRecorder {
public:
  VideoRecorder(const char *param, Flow *f);
  ~VideoRecorder();

  bool Write(MuxerFlow *f, std::shared_ptr<MediaBuffer> buffer);

private:
  std::shared_ptr<Muxer> muxer;
  int vid_stream_id;
  int aud_stream_id;
  void ClearStream();
  Flow *muxer_flow;
};
} // namespace easymedia

#endif
