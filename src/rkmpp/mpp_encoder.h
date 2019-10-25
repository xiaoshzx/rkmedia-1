// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_MPP_ENCODER_H
#define EASYMEDIA_MPP_ENCODER_H

#include "encoder.h"
#include "mpp_inc.h"

namespace easymedia {

// A encoder which call the mpp interface directly.
// Mpp is always video process module.
class MPPEncoder : public VideoEncoder {
public:
  MPPEncoder();
  virtual ~MPPEncoder() = default;

  virtual bool Init() override;

  // sync encode the raw input buffer to output buffer
  virtual int Process(const std::shared_ptr<MediaBuffer> &input,
                      std::shared_ptr<MediaBuffer> &output,
                      std::shared_ptr<MediaBuffer> extra_output) override;

  virtual int SendInput(const std::shared_ptr<MediaBuffer> &) override;
  virtual std::shared_ptr<MediaBuffer> FetchOutput() override;

protected:
  MppCodingType coding_type;
  uint32_t output_mb_flags;
  // call before Init()
  void SetMppCodeingType(MppCodingType type);
  virtual bool
  CheckConfigChange(std::pair<uint32_t, std::shared_ptr<ParameterBuffer>>) {
    return true;
  }
  // Control before encoding.
  int EncodeControl(int cmd, void *param);

  virtual int PrepareMppFrame(const std::shared_ptr<MediaBuffer> &input,
                              MppFrame &frame);
  virtual int PrepareMppPacket(std::shared_ptr<MediaBuffer> &output,
                               MppPacket &packet);
  virtual int PrepareMppExtraBuffer(std::shared_ptr<MediaBuffer> extra_output,
                                    MppBuffer &buffer);
  int Process(MppFrame frame, MppPacket &packet, MppBuffer &mv_buf);

private:
  std::shared_ptr<MPPContext> mpp_ctx;

  friend class MPPMJPEGConfig;
  friend class MPPCommonConfig;
};

} // namespace easymedia

#endif // EASYMEDIA_MPP_ENCODER_H
