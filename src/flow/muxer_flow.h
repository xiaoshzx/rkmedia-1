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

class MuxerFlow : public Flow {
  friend VideoRecorder;

public:
  MuxerFlow(const char *param);
  virtual ~MuxerFlow();
  static const char *GetFlowName() { return "muxer_flow"; }

private:
  std::shared_ptr<VideoRecorder> NewRecoder(const char *path);
  friend bool save_buffer(Flow *f, MediaBufferVector &input_vector);

private:
  std::shared_ptr<MediaBuffer> video_extra;
  std::string muxer_param;
  std::string file_prefix;
  std::string file_path;
  std::shared_ptr<VideoRecorder> video_recorder;
  MediaConfig vid_enc_config;
  MediaConfig aud_enc_config;
  bool video_in;
  bool audio_in;
  int64_t file_duration;
  int64_t file_index;
  int64_t last_ts;
  bool file_time_en;
  std::string GenFilePath();
};

class VideoRecorder {
public:
  VideoRecorder(const char *param);
  ~VideoRecorder();

  bool Write(MuxerFlow *f, std::shared_ptr<MediaBuffer> buffer);

private:
  std::shared_ptr<Muxer> muxer;
  int vid_stream_id;
  int aud_stream_id;
  void ClearStream();
};

} // namespace easymedia

#endif
