// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <assert.h>

#include "buffer.h"

#include "media_type.h"
#include "mpp_encoder.h"

namespace easymedia {

class MPPConfig {
public:
  virtual ~MPPConfig() = default;
  virtual bool InitConfig(MPPEncoder &mpp_enc, const MediaConfig &cfg) = 0;
  virtual bool CheckConfigChange(MPPEncoder &mpp_enc, uint32_t change,
                                 std::shared_ptr<ParameterBuffer> val) = 0;
};

class MPPMJPEGConfig : public MPPConfig {
public:
  virtual bool InitConfig(MPPEncoder &mpp_enc, const MediaConfig &cfg) override;
  virtual bool CheckConfigChange(MPPEncoder &mpp_enc, uint32_t change,
                                 std::shared_ptr<ParameterBuffer> val) override;
};

static bool MppEncPrepConfig(MppEncPrepCfg &prep_cfg,
                             const ImageInfo &image_info) {
  MppFrameFormat pic_type = ConvertToMppPixFmt(image_info.pix_fmt);
  if (pic_type == -1) {
    LOG("error input pixel format\n");
    return false;
  }
  memset(&prep_cfg, 0, sizeof(prep_cfg));
  prep_cfg.change =
      MPP_ENC_PREP_CFG_CHANGE_INPUT | MPP_ENC_PREP_CFG_CHANGE_FORMAT;
  prep_cfg.width = image_info.width;
  prep_cfg.height = image_info.height;
  if (pic_type == MPP_FMT_YUV422_YUYV || pic_type == MPP_FMT_YUV422_UYVY)
    prep_cfg.hor_stride = image_info.vir_width * 2;
  else
    prep_cfg.hor_stride = image_info.vir_width;
  prep_cfg.ver_stride = image_info.vir_height;
  prep_cfg.format = pic_type;
  LOG("encode w x h(%d[%d] x %d[%d])\n", prep_cfg.width, prep_cfg.hor_stride,
      prep_cfg.height, prep_cfg.ver_stride);
  return true;
}

bool MPPMJPEGConfig::InitConfig(MPPEncoder &mpp_enc, const MediaConfig &cfg) {
  const ImageConfig &img_cfg = cfg.img_cfg;
  MppEncPrepCfg prep_cfg;

  if (!MppEncPrepConfig(prep_cfg, img_cfg.image_info))
    return false;
  int ret = mpp_enc.EncodeControl(MPP_ENC_SET_PREP_CFG, &prep_cfg);
  if (ret) {
    LOG("mpi control enc set cfg failed\n");
    return false;
  }
  mpp_enc.GetConfig().img_cfg.image_info = img_cfg.image_info;
  mpp_enc.GetConfig().type = Type::Image;
  std::shared_ptr<ParameterBuffer> change =
      std::make_shared<ParameterBuffer>(0);
  change->SetValue(img_cfg.qp_init);
  return CheckConfigChange(mpp_enc, VideoEncoder::kQPChange, change);
}

bool MPPMJPEGConfig::CheckConfigChange(MPPEncoder &mpp_enc, uint32_t change,
                                       std::shared_ptr<ParameterBuffer> val) {
  ImageConfig &iconfig = mpp_enc.GetConfig().img_cfg;
  if (change & VideoEncoder::kQPChange) {
    int quant = val->GetValue();
    MppEncCodecCfg codec_cfg;
    memset(&codec_cfg, 0, sizeof(codec_cfg));
    codec_cfg.coding = MPP_VIDEO_CodingMJPEG;
    codec_cfg.jpeg.change = MPP_ENC_JPEG_CFG_CHANGE_QP;
    quant = std::max(1, std::min(quant, 10));
    codec_cfg.jpeg.quant = quant;
    int ret = mpp_enc.EncodeControl(MPP_ENC_SET_CODEC_CFG, &codec_cfg);
    if (ret) {
      LOG("mpi control enc change codec cfg failed, ret=%d\n", ret);
      return false;
    }
    LOG("mjpeg quant = %d\n", quant);
    iconfig.qp_init = quant;
  } else {
    LOG("Unsupport request change 0x%08x!\n", change);
    return false;
  }

  return true;
}

class MPPCommonConfig : public MPPConfig {
public:
  static const int kMPPMinBps = 2 * 1000;
  static const int kMPPMaxBps = 98 * 1000 * 1000;

  MPPCommonConfig(MppCodingType type) : code_type(type) {}
  virtual bool InitConfig(MPPEncoder &mpp_enc, const MediaConfig &cfg) override;
  virtual bool CheckConfigChange(MPPEncoder &mpp_enc, uint32_t change,
                                 std::shared_ptr<ParameterBuffer> val) override;

private:
  MppCodingType code_type;
};

static int CalcMppBps(MppEncRcCfg *rc_cfg, int bps_max) {
  if ((bps_max > MPPCommonConfig::kMPPMaxBps) ||
    (bps_max < MPPCommonConfig::kMPPMinBps)) {
    LOG("ERROR: MPP Encoder: bps <%d> is not valid!\n", bps_max);
    return -1;
  }

  switch (rc_cfg->rc_mode) {
  case MPP_ENC_RC_MODE_CBR:
    // constant bitrate has very small bps range of 1/16 bps
    rc_cfg->bps_max = bps_max;
    rc_cfg->bps_target = bps_max * 16 / 17;
    rc_cfg->bps_min = bps_max * 15 / 17;
    break;
  case MPP_ENC_RC_MODE_VBR:
    if (rc_cfg->quality == MPP_ENC_RC_QUALITY_CQP) {
      /* constant QP does not have bps */
      rc_cfg->bps_target   = -1;
      rc_cfg->bps_max      = -1;
      rc_cfg->bps_min      = -1;
    } else {
      // variable bitrate has large bps range
      rc_cfg->bps_target = bps_max * 2 / 3;
      rc_cfg->bps_max = bps_max;
      rc_cfg->bps_min = bps_max * 1 / 3;
    }
    break;
  default:
    // TODO
    LOG("right now rc_mode=%d is untested\n", rc_cfg->rc_mode);
    return -1;
  }

  if (rc_cfg->bps_max > MPPCommonConfig::kMPPMaxBps)
    rc_cfg->bps_max = MPPCommonConfig::kMPPMaxBps;
  if (rc_cfg->bps_min < MPPCommonConfig::kMPPMinBps)
    rc_cfg->bps_min = MPPCommonConfig::kMPPMinBps;

  return 0;
}

bool MPPCommonConfig::InitConfig(MPPEncoder &mpp_enc, const MediaConfig &cfg) {
  VideoConfig vconfig = cfg.vid_cfg;
  const ImageConfig &img_cfg = vconfig.image_cfg;
  MpiCmd mpi_cmd = MPP_CMD_BASE;
  MppParam param = NULL;

  MppEncRcCfg rc_cfg;
  MppEncPrepCfg prep_cfg;
  MppEncCodecCfg codec_cfg;
  unsigned int need_block = 1;
  int dummy = 1;
  memset(&rc_cfg, 0, sizeof(rc_cfg));
  memset(&prep_cfg, 0, sizeof(prep_cfg));
  memset(&codec_cfg, 0, sizeof(codec_cfg));

  mpi_cmd = MPP_SET_INPUT_BLOCK;
  param = &need_block;
  int ret = mpp_enc.EncodeControl(mpi_cmd, param);
  if (ret != 0) {
    LOG("mpp control set input block failed ret %d\n", ret);
    return false;
  }

  if (!MppEncPrepConfig(prep_cfg, img_cfg.image_info))
    return false;
  ret = mpp_enc.EncodeControl(MPP_ENC_SET_PREP_CFG, &prep_cfg);
  if (ret) {
    LOG("mpi control enc set prep cfg failed ret %d\n", ret);
    return false;
  }

  param = &dummy;
  ret = mpp_enc.EncodeControl(MPP_ENC_PRE_ALLOC_BUFF, param);
  if (ret) {
    LOG("mpi control pre alloc buff failed ret %d\n", ret);
    return false;
  }

  rc_cfg.change = MPP_ENC_RC_CFG_CHANGE_ALL;

  rc_cfg.rc_mode = GetMPPRCMode(vconfig.rc_mode);
  if (rc_cfg.rc_mode == MPP_ENC_RC_MODE_BUTT) {
    LOG("Invalid rc mode %s\n", vconfig.rc_mode);
    return false;
  }
  rc_cfg.quality = GetMPPRCQuality(vconfig.rc_quality);
  if (rc_cfg.quality == MPP_ENC_RC_QUALITY_BUTT) {
    LOG("Invalid rc quality %s\n", vconfig.rc_quality);
    return false;
  }

  int bps = vconfig.bit_rate;
  int fps = std::max(1, std::min(vconfig.frame_rate, (1 << 16) - 1));
  int gop = vconfig.gop_size;

  if (CalcMppBps(&rc_cfg, bps) < 0)
    return false;
  // fix input / output frame rate
  rc_cfg.fps_in_flex = 0;
  rc_cfg.fps_in_num = fps;
  rc_cfg.fps_in_denorm = 1;
  rc_cfg.fps_out_flex = 0;
  rc_cfg.fps_out_num = fps;
  rc_cfg.fps_out_denorm = 1;

  rc_cfg.gop = gop;
  rc_cfg.skip_cnt = 0;

  vconfig.bit_rate = rc_cfg.bps_target;
  vconfig.frame_rate = fps;
  LOG("encode bps %d fps %d gop %d\n", bps, fps, gop);
  ret = mpp_enc.EncodeControl(MPP_ENC_SET_RC_CFG, &rc_cfg);
  if (ret) {
    LOG("mpi control enc set rc cfg failed ret %d\n", ret);
    return false;
  }

  int profile = vconfig.profile;
  if (profile != 66 && profile != 77)
    vconfig.profile = profile = 100; // default PROFILE_HIGH 100

  codec_cfg.coding = code_type;
  switch (code_type) {
  case MPP_VIDEO_CodingAVC:
    codec_cfg.h264.change =
        MPP_ENC_H264_CFG_CHANGE_PROFILE | MPP_ENC_H264_CFG_CHANGE_ENTROPY;
    codec_cfg.h264.profile = profile;
    codec_cfg.h264.level = vconfig.level;
    codec_cfg.h264.entropy_coding_mode =
        (profile == 66 || profile == 77) ? (0) : (1);
    codec_cfg.h264.cabac_init_idc = 0;

#ifdef RK_MPP_VERSION_NEW
    if (vconfig.trans_8x8) {
      if (profile == 100) {
        codec_cfg.h264.change |= MPP_ENC_H264_CFG_CHANGE_TRANS_8x8;
        codec_cfg.h264.transform8x8_mode = vconfig.trans_8x8;
      } else {
        LOG("the profile must be greater than 100"
          "to enable h264 enc trans_8x8.\n");
      }
    }
#endif
    // setup QP on CQP mode
    codec_cfg.h264.change |= MPP_ENC_H264_CFG_CHANGE_QP_LIMIT;
    codec_cfg.h264.qp_init = img_cfg.qp_init;
    codec_cfg.h264.qp_min = vconfig.qp_min;
    codec_cfg.h264.qp_max = vconfig.qp_max;
    codec_cfg.h264.qp_max_step = vconfig.qp_step;
    break;
  case MPP_VIDEO_CodingHEVC:
#ifdef RK_MPP_VERSION_NEW
    codec_cfg.h265.change =
      MPP_ENC_H265_CFG_INTRA_QP_CHANGE | MPP_ENC_H265_CFG_RC_QP_CHANGE;
    codec_cfg.h265.qp_init = img_cfg.qp_init;
    codec_cfg.h265.max_i_qp = vconfig.max_i_qp;
    codec_cfg.h265.min_i_qp = vconfig.min_i_qp;
    codec_cfg.h265.max_qp = vconfig.qp_max;
    codec_cfg.h265.min_qp = vconfig.qp_min;
#else
    codec_cfg.h265.change = MPP_ENC_H265_CFG_INTRA_QP_CHANGE;
    codec_cfg.h265.intra_qp = img_cfg.qp_init;
#endif
    break;
  default:
    // will never go here, avoid gcc warning
    return false;
  }
  LOG("encode profile %d level %d init_qp %d\n", profile, vconfig.level,
      img_cfg.qp_init);
  ret = mpp_enc.EncodeControl(MPP_ENC_SET_CODEC_CFG, &codec_cfg);
  if (ret) {
    LOG("mpi control enc set codec cfg failed ret %d\n", ret);
    return false;
  }

  if (bps >= 50000000) {
    RK_U32 qp_scale = 2; // 1 or 2
    ret = mpp_enc.EncodeControl(MPP_ENC_SET_QP_RANGE, &qp_scale);
    if (ret) {
      LOG("mpi control enc set qp scale failed ret %d\n", ret);
      return false;
    }
    LOG("qp_scale:%d\n", qp_scale);
  }

  MppPacket packet = nullptr;
  ret = mpp_enc.EncodeControl(MPP_ENC_GET_EXTRA_INFO, &packet);
  if (ret) {
    LOG("mpi control enc get extra info failed\n");
    return false;
  }

  // Get and write sps/pps for H.264/5
  if (packet) {
    void *ptr = mpp_packet_get_pos(packet);
    size_t len = mpp_packet_get_length(packet);
    if (!mpp_enc.SetExtraData(ptr, len)) {
      LOG("SetExtraData failed\n");
      return false;
    }
    mpp_enc.GetExtraData()->SetUserFlag(MediaBuffer::kExtraIntra);
    packet = NULL;
  }

  mpp_enc.GetConfig().vid_cfg = vconfig;
  mpp_enc.GetConfig().type = Type::Video;
  return true;
}

bool MPPCommonConfig::CheckConfigChange(MPPEncoder &mpp_enc, uint32_t change,
                                        std::shared_ptr<ParameterBuffer> val) {
  VideoConfig &vconfig = mpp_enc.GetConfig().vid_cfg;
  MppEncRcCfg rc_cfg;

  // reset all values.
  memset(&rc_cfg, 0, sizeof(rc_cfg));

  if (change & VideoEncoder::kFrameRateChange) {
    uint8_t *values = (uint8_t *)val->GetPtr();
    if (val->GetSize() < 2) {
      LOG("ERROR: MPP Encoder: fps should be array[2Byte]={num, den}\n");
      return false;
    }
    uint8_t new_fps_num = values[0];
    uint8_t new_fps_den = values[1];
    rc_cfg.change = MPP_ENC_RC_CFG_CHANGE_FPS_OUT;
    rc_cfg.fps_out_flex = 0;
    rc_cfg.fps_out_num = new_fps_num;
    rc_cfg.fps_out_denorm = new_fps_den;
    if (mpp_enc.EncodeControl(MPP_ENC_SET_RC_CFG, &rc_cfg) != 0) {
      LOG("ERROR: MPP Encoder: fps control failed!\n");
      return false;
    }
    vconfig.frame_rate = new_fps_num;
  } else if (change & VideoEncoder::kBitRateChange) {
    int new_bit_rate = val->GetValue();
    LOGD("MPP Encoder: new bpsmax:%d\n", new_bit_rate);
    if((new_bit_rate < 0) ||(new_bit_rate > 60 * 1000 * 1000)) {
      LOG("ERROR: MPP Encoder: bps should within (0, 60Mb]\n");
      return false;
    }
    rc_cfg.change = MPP_ENC_RC_CFG_CHANGE_BPS;
    rc_cfg.rc_mode = GetMPPRCMode(vconfig.rc_mode);
    rc_cfg.quality = GetMPPRCQuality(vconfig.rc_quality);
    if (CalcMppBps(&rc_cfg, new_bit_rate) < 0)
      return false;
    if (mpp_enc.EncodeControl(MPP_ENC_SET_RC_CFG, &rc_cfg) != 0) {
      LOG("ERROR: MPP Encoder: bpsmax control failed!\n");
      return false;
    }
    vconfig.bit_rate = new_bit_rate;
  } else if (change & VideoEncoder::kRcModeChange) {
    char *new_mode = (char *)val->GetPtr();
    LOGD("MPP Encoder: new rc_mode:%s\n", new_mode);
    rc_cfg.rc_mode = GetMPPRCMode(new_mode);
    if (rc_cfg.rc_mode == MPP_ENC_RC_MODE_BUTT) {
      LOG("ERROR: MPP Encoder: rc_mode is invalid!"
        "should be KEY_CBR/KEY_VBR/KEY_FIXQP.\n");
      return false;
    }
    rc_cfg.quality = GetMPPRCQuality(vconfig.rc_quality);
    rc_cfg.change =
      MPP_ENC_RC_CFG_CHANGE_RC_MODE | MPP_ENC_RC_CFG_CHANGE_BPS;
    //Recalculate bps
    if (CalcMppBps(&rc_cfg, vconfig.bit_rate) < 0)
      return false;
    if (mpp_enc.EncodeControl(MPP_ENC_SET_RC_CFG, &rc_cfg) != 0) {
      LOG("ERROR: MPP Encoder: rc_mode control failed!\n");
      return false;
    }
    // save new value to encoder->vconfig.
    if (rc_cfg.rc_mode == MPP_ENC_RC_MODE_VBR)
      vconfig.rc_mode = KEY_VBR;
    else
      vconfig.rc_mode = KEY_CBR;
  } else if (change & VideoEncoder::kRcQualityChange) {
    char *new_quality = (char *)val->GetPtr();
    LOGD("MPP Encoder: new rc_quality:%s\n", new_quality);
    rc_cfg.quality = GetMPPRCQuality(new_quality);
    if (rc_cfg.quality == MPP_ENC_RC_QUALITY_BUTT) {
      LOG("ERROR: MPP Encoder: rc_quality is invalid!"
        "should be [KEY_WORST, KEY_BEST].\n");
      return false;
    }
    LOGD("MPP Encoder: current rc_mode:%s\n", vconfig.rc_mode);
    rc_cfg.rc_mode = GetMPPRCMode(vconfig.rc_mode);
    rc_cfg.change =
      MPP_ENC_RC_CFG_CHANGE_QUALITY | MPP_ENC_RC_CFG_CHANGE_BPS;
    //Recalculate bps
    if (CalcMppBps(&rc_cfg, vconfig.bit_rate) < 0)
      return false;
    if (mpp_enc.EncodeControl(MPP_ENC_SET_RC_CFG, &rc_cfg) != 0) {
      LOG("ERROR: MPP Encoder: rc_quality control failed!\n");
      return false;
    }
    // save new value to encoder->vconfig.
    if (!strcmp(new_quality, KEY_BEST))
      vconfig.rc_quality = KEY_BEST;
    else if (!strcmp(new_quality, KEY_BETTER))
      vconfig.rc_quality = KEY_BETTER;
    else if (!strcmp(new_quality, KEY_MEDIUM))
      vconfig.rc_quality = KEY_MEDIUM;
    else if (!strcmp(new_quality, KEY_WORSE))
      vconfig.rc_quality = KEY_WORSE;
    else if (!strcmp(new_quality, KEY_WORST))
      vconfig.rc_quality = KEY_WORST;
  } else if (change & VideoEncoder::kForceIdrFrame) {
    LOGD("MPP Encoder: force idr frame...\n");
    if (mpp_enc.EncodeControl(MPP_ENC_SET_IDR_FRAME, nullptr) != 0) {
      LOG("ERROR: MPP Encoder: force idr frame control failed!\n");
      return false;
    }
  } else if (change & VideoEncoder::kQPChange) {
    VideoEncoderQp *qps = (VideoEncoderQp *)val->GetPtr();
    if (val->GetSize() < sizeof(VideoEncoderQp)) {
      LOG("ERROR: MPP Encoder: Incomplete VideoEncoderQp information\n");
      return false;
    }
    LOGD("MPP Encoder: new qp:(%d, %d, %d, %d, %d, %d)\n",
      qps->qp_init, qps->qp_step, qps->qp_min, qps->qp_max,
      qps->min_i_qp, qps->max_i_qp);
    MppEncCodecCfg codec_cfg;
    if (code_type == MPP_VIDEO_CodingAVC) {
      codec_cfg.h264.change = MPP_ENC_H264_CFG_CHANGE_QP_LIMIT;
      codec_cfg.h264.qp_init = qps->qp_init;
      codec_cfg.h264.qp_max_step = qps->qp_step;
      codec_cfg.h264.qp_min = qps->qp_min;
      codec_cfg.h264.qp_max = qps->qp_max;
    } else if (code_type == MPP_VIDEO_CodingHEVC) {
      codec_cfg.h265.change =
        MPP_ENC_H265_CFG_INTRA_QP_CHANGE | MPP_ENC_H265_CFG_RC_QP_CHANGE;
      codec_cfg.h265.qp_init = qps->qp_init;
      codec_cfg.h265.qp_max_step = qps->qp_step;
      codec_cfg.h265.min_qp = qps->qp_min;
      codec_cfg.h265.max_qp = qps->qp_max;
      codec_cfg.h265.max_i_qp = qps->max_i_qp;
      codec_cfg.h265.min_i_qp = qps->min_i_qp;
    } else {
      LOG("ERROR: MPP Encoder: Encoder:%d not support this change\n",
        code_type);
      return false;
    }
    if (mpp_enc.EncodeControl(MPP_ENC_SET_CODEC_CFG, &codec_cfg) != 0) {
      LOG("ERROR: MPP Encoder: qp control failed!\n");
      return false;
    }
    vconfig.image_cfg.qp_init = qps->qp_init;
    vconfig.qp_min = qps->qp_min;
    vconfig.qp_max = qps->qp_max;
    vconfig.qp_step = qps->qp_step;
    vconfig.max_i_qp = qps->max_i_qp;
    vconfig.min_i_qp = qps->min_i_qp;
  } else if (change & VideoEncoder::kROICfgChange) {
    EncROIRegion *regions = (EncROIRegion *)val->GetPtr();
    if (val->GetSize() && (val->GetSize() < sizeof(EncROIRegion))) {
      LOG("ERROR: MPP Encoder: ParameterBuffer size is invalid!\n");
      return false;
    }
    int region_cnt = val->GetSize() / sizeof(EncROIRegion);
    mpp_enc.RoiUpdateRegions(regions, region_cnt);
  }
#ifdef MPP_SUPPORT_HW_OSD
  else if (change & VideoEncoder::kOSDDataChange) {
    // type: OsdRegionData*
    LOGD("MPP Encoder: config osd regions\n");
    if (val->GetSize() < sizeof(OsdRegionData)) {
      LOG("ERROR: MPP Encoder: palette buff should be OsdRegionData type\n");
      return false;
    }
    OsdRegionData *param = (OsdRegionData *)val->GetPtr();
    if (mpp_enc.OsdRegionSet(param)) {
      LOG("ERROR: MPP Encoder: set osd regions error!\n");
      return false;
    }
  } else if (change & VideoEncoder::kOSDPltChange) {
    // type: 265 * U32 array.
    LOGD("MPP Encoder: config osd palette\n");
    if (val->GetSize() < (sizeof(int) * 4)) {
      LOG("ERROR: MPP Encoder: palette buff should be U32 * 256\n");
      return false;
    }
    uint32_t *param = (uint32_t *)val->GetPtr();
    if (mpp_enc.OsdPaletteSet(param)) {
      LOG("ERROR: MPP Encoder: set Palette error!\n");
      return false;
    }
  }
#endif
  else {
    LOG("Unsupport request change 0x%08x!\n", change);
    return false;
  }

  return true;
}

class MPPFinalEncoder : public MPPEncoder {
public:
  MPPFinalEncoder(const char *param);
  virtual ~MPPFinalEncoder() {
    if (mpp_config)
      delete mpp_config;
  }

  static const char *GetCodecName() { return "rkmpp"; }
  virtual bool InitConfig(const MediaConfig &cfg) override;

protected:
  // Change configs which are not contained in sps/pps.
  virtual bool CheckConfigChange(
      std::pair<uint32_t, std::shared_ptr<ParameterBuffer>>) override;

  MPPConfig *mpp_config;
};

MPPFinalEncoder::MPPFinalEncoder(const char *param) : mpp_config(nullptr) {
  std::string output_data_type =
      get_media_value_by_key(param, KEY_OUTPUTDATATYPE);
  SetMppCodeingType(output_data_type.empty()
                        ? MPP_VIDEO_CodingUnused
                        : GetMPPCodingType(output_data_type));
}

bool MPPFinalEncoder::InitConfig(const MediaConfig &cfg) {
  assert(!mpp_config);
  MediaConfig new_cfg = cfg;
  switch (coding_type) {
  case MPP_VIDEO_CodingMJPEG:
    mpp_config = new MPPMJPEGConfig();
    new_cfg.img_cfg.codec_type = codec_type;
    break;
  case MPP_VIDEO_CodingAVC:
  case MPP_VIDEO_CodingHEVC:
    new_cfg.vid_cfg.image_cfg.codec_type = codec_type;
    mpp_config = new MPPCommonConfig(coding_type);
    break;
  default:
    LOG("Unsupport mpp encode type: %d\n", coding_type);
    return false;
  }
  if (!mpp_config) {
    LOG_NO_MEMORY();
    return false;
  }
  return mpp_config->InitConfig(*this, new_cfg);
}

bool MPPFinalEncoder::CheckConfigChange(
    std::pair<uint32_t, std::shared_ptr<ParameterBuffer>> change_pair) {
  //common ConfigChange process
  if (change_pair.first & VideoEncoder::kEnableStatistics) {
    bool value = (change_pair.second->GetValue())?true:false;
    set_statistics_switch(value);
    return true;
  }

  assert(mpp_config);
  if (!mpp_config)
    return false;
  return mpp_config->CheckConfigChange(*this, change_pair.first,
                                       change_pair.second);
}

DEFINE_VIDEO_ENCODER_FACTORY(MPPFinalEncoder)
const char *FACTORY(MPPFinalEncoder)::ExpectedInputDataType() {
  return MppAcceptImageFmts();
}

#define IMAGE_JPEG "image:jpeg"
#define VIDEO_H264 "video:h264"
#define VIDEO_H265 "video:h265"

#define VIDEO_ENC_OUTPUT                     \
  TYPENEAR(IMAGE_JPEG) TYPENEAR(VIDEO_H264)  \
  TYPENEAR(VIDEO_H265)

const char *FACTORY(MPPFinalEncoder)::OutPutDataType() { return VIDEO_ENC_OUTPUT; }

} // namespace easymedia
