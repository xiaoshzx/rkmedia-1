// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media_config.h"

#include <strings.h>

#include "key_string.h"
#include "media_type.h"
#include "utils.h"
#include "encoder.h"

namespace easymedia {

const char *rc_quality_strings[7] = {KEY_WORST,  KEY_WORSE, KEY_MEDIUM,
                                     KEY_BETTER, KEY_BEST,  KEY_CQP,
                                     KEY_AQ_ONLY};
const char *rc_mode_strings[2] = {KEY_VBR, KEY_CBR};

static const char *convert2constchar(const std::string &s, const char *array[],
                                     size_t array_len) {
  for (size_t i = 0; i < array_len; i++)
    if (!strcasecmp(s.c_str(), array[i]))
      return array[i];
  return nullptr;
}

static const char *ConvertRcQuality(const std::string &s) {
  return convert2constchar(s, rc_quality_strings,
                           ARRAY_ELEMS(rc_quality_strings));
}

static const char *ConvertRcMode(const std::string &s) {
  return convert2constchar(s, rc_mode_strings, ARRAY_ELEMS(rc_mode_strings));
}

bool ParseMediaConfigFromMap(std::map<std::string, std::string> &params,
                             MediaConfig &mc) {
  std::string value = params[KEY_OUTPUTDATATYPE];
  if (value.empty()) {
    LOG("miss %s\n", KEY_OUTPUTDATATYPE);
    return false;
  }
  bool image_in = string_start_withs(value, IMAGE_PREFIX);
  bool video_in = string_start_withs(value, VIDEO_PREFIX);
  bool audio_in = string_start_withs(value, AUDIO_PREFIX);
  if (!image_in && !video_in && !audio_in) {
    LOG("unsupport outtype %s\n", value.c_str());
    return false;
  }
  ImageInfo info;
  int qp_init;
  CodecType codec_type;
  if (image_in || video_in) {
    if (!ParseImageInfoFromMap(params, info))
      return false;
    CHECK_EMPTY(value, params, KEY_COMPRESS_QP_INIT)
    qp_init = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_CODECTYPE)
    codec_type = (CodecType)std::stoi(value);
  } else {
    // audio
    AudioConfig &aud_cfg = mc.aud_cfg;
    if (!ParseSampleInfoFromMap(params, aud_cfg.sample_info))
      return false;
    CHECK_EMPTY(value, params, KEY_COMPRESS_BITRATE)
    aud_cfg.bit_rate = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_FLOAT_QUALITY)
    aud_cfg.quality = std::stof(value);
    CHECK_EMPTY(value, params, KEY_CODECTYPE)
    aud_cfg.codec_type = (CodecType)std::stoi(value);
    mc.type = Type::Audio;
    return true;
  }
  if (image_in) {
    ImageConfig &img_cfg = mc.img_cfg;
    img_cfg.image_info = info;
    img_cfg.qp_init = qp_init;
    img_cfg.codec_type = codec_type;
    mc.type = Type::Image;
  } else if (video_in) {
    VideoConfig &vid_cfg = mc.vid_cfg;
    ImageConfig &img_cfg = vid_cfg.image_cfg;
    img_cfg.image_info = info;
    img_cfg.qp_init = qp_init;
    img_cfg.codec_type = codec_type;
    CHECK_EMPTY(value, params, KEY_COMPRESS_QP_STEP)
    vid_cfg.qp_step = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_COMPRESS_QP_MIN)
    vid_cfg.qp_min = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_COMPRESS_QP_MAX)
    vid_cfg.qp_max = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_COMPRESS_BITRATE)
    vid_cfg.bit_rate = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_FPS)
    vid_cfg.frame_rate = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_LEVEL)
    vid_cfg.level = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_VIDEO_GOP)
    vid_cfg.gop_size = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_PROFILE)
    vid_cfg.profile = std::stoi(value);
    CHECK_EMPTY_WITH_DECLARE(const std::string &, rc_q, params,
                             KEY_COMPRESS_RC_QUALITY)
    vid_cfg.rc_quality = ConvertRcQuality(rc_q);
    CHECK_EMPTY_WITH_DECLARE(const std::string &, rc_m, params,
                             KEY_COMPRESS_RC_MODE)
    vid_cfg.rc_mode = ConvertRcMode(rc_m);
    CHECK_EMPTY(value, params, KEY_H265_MAX_I_QP)
    vid_cfg.max_i_qp = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_H265_MIN_I_QP)
    vid_cfg.min_i_qp = std::stoi(value);
    CHECK_EMPTY(value, params, KEY_H264_TRANS_8x8)
    vid_cfg.trans_8x8 = std::stoi(value);
    mc.type = Type::Video;
  }
  return true;
}

std::string to_param_string(const ImageConfig &img_cfg) {
  std::string ret = to_param_string(img_cfg.image_info);
  PARAM_STRING_APPEND_TO(ret, KEY_COMPRESS_QP_INIT, img_cfg.qp_init);
  PARAM_STRING_APPEND_TO(ret, KEY_CODECTYPE, img_cfg.codec_type);
  return ret;
}

std::string to_param_string(const VideoConfig &vid_cfg) {
  const ImageConfig &img_cfg = vid_cfg.image_cfg;
  std::string ret = to_param_string(img_cfg);
  PARAM_STRING_APPEND_TO(ret, KEY_COMPRESS_QP_STEP, vid_cfg.qp_step);
  PARAM_STRING_APPEND_TO(ret, KEY_COMPRESS_QP_MIN, vid_cfg.qp_min);
  PARAM_STRING_APPEND_TO(ret, KEY_COMPRESS_QP_MAX, vid_cfg.qp_max);
  PARAM_STRING_APPEND_TO(ret, KEY_COMPRESS_BITRATE, vid_cfg.bit_rate);
  PARAM_STRING_APPEND_TO(ret, KEY_FPS, vid_cfg.frame_rate);
  PARAM_STRING_APPEND_TO(ret, KEY_LEVEL, vid_cfg.level);
  PARAM_STRING_APPEND_TO(ret, KEY_VIDEO_GOP, vid_cfg.gop_size);
  PARAM_STRING_APPEND_TO(ret, KEY_PROFILE, vid_cfg.profile);
  PARAM_STRING_APPEND(ret, KEY_COMPRESS_RC_QUALITY, vid_cfg.rc_quality);
  PARAM_STRING_APPEND(ret, KEY_COMPRESS_RC_MODE, vid_cfg.rc_mode);
  PARAM_STRING_APPEND_TO(ret, KEY_H265_MAX_I_QP, vid_cfg.max_i_qp);
  PARAM_STRING_APPEND_TO(ret, KEY_H265_MIN_I_QP, vid_cfg.min_i_qp);
  PARAM_STRING_APPEND_TO(ret, KEY_H264_TRANS_8x8, vid_cfg.trans_8x8);
  return ret;
}

std::string to_param_string(const AudioConfig &aud_cfg) {
  std::string ret = to_param_string(aud_cfg.sample_info);
  PARAM_STRING_APPEND_TO(ret, KEY_COMPRESS_BITRATE, aud_cfg.bit_rate);
  PARAM_STRING_APPEND_TO(ret, KEY_FLOAT_QUALITY, aud_cfg.quality);
  PARAM_STRING_APPEND_TO(ret, KEY_CODECTYPE, aud_cfg.codec_type);
  return ret;
}

std::string to_param_string(const MediaConfig &mc,
                            const std::string &out_type) {
  std::string ret;
  MediaConfig mc_temp = mc;
  bool image_in = string_start_withs(out_type, IMAGE_PREFIX);
  bool video_in = string_start_withs(out_type, VIDEO_PREFIX);
  bool audio_in = string_start_withs(out_type, AUDIO_PREFIX);
  if (!image_in && !video_in && !audio_in) {
    LOG("unsupport outtype %s\n", out_type.c_str());
    return ret;
  }

  PARAM_STRING_APPEND(ret, KEY_OUTPUTDATATYPE, out_type);
  if (image_in) {
    mc_temp.img_cfg.codec_type = StringToCodecType(out_type.c_str());
    ret.append(to_param_string(mc_temp.img_cfg));
  }

  if (video_in) {
    mc_temp.vid_cfg.image_cfg.codec_type = StringToCodecType(out_type.c_str());
    ret.append(to_param_string(mc_temp.vid_cfg));
  }

  if (audio_in) {
    mc_temp.aud_cfg.codec_type = StringToCodecType(out_type.c_str());
    ret.append(to_param_string(mc_temp.aud_cfg));
  }

  return ret;
}

int video_encoder_set_maxbps(
  std::shared_ptr<Flow> &enc_flow, unsigned int bpsmax) {
  if (!enc_flow)
    return -EINVAL;

  if (bpsmax >= 98 * 1000 * 1000) {
    LOG("ERROR: bpsmax should be less then 98Mb\n");
    return -EINVAL;
  }

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  pbuff->SetValue(bpsmax);
  enc_flow->Control(VideoEncoder::kBitRateChange, pbuff);

  return 0;
}

_API int video_encoder_set_fps(
  std::shared_ptr<Flow> &enc_flow, uint8_t num, uint8_t den) {
  if (!enc_flow)
    return -EINVAL;

  if (!den || !num || (den > 16) || (num > 120)) {
    LOG("ERROR: fps(%d/%d) is invalid! num:[1,120], den:[1, 16].\n",
      num, den);
    return -EINVAL;
  }

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  uint8_t *fps_array = (uint8_t *)malloc(2 * sizeof(uint8_t));
  fps_array[0] = num;
  fps_array[1] = den;
  pbuff->SetPtr(fps_array, 2);
  enc_flow->Control(VideoEncoder::kFrameRateChange, pbuff);

  return 0;
}

// input palette should be yuva formate.
int video_encoder_set_osd_plt(
  std::shared_ptr<Flow> &enc_flow, uint32_t *yuv_plt) {
  if (!enc_flow)
    return -EINVAL;

  uint32_t *plt = (uint32_t *)malloc(256 * sizeof(uint32_t));
  memcpy(plt, yuv_plt, 256 * sizeof(uint32_t));

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  pbuff->SetPtr(plt, 256 * sizeof(uint32_t));
  enc_flow->Control(VideoEncoder::kOSDPltChange, pbuff);

  return 0;
}

int video_encoder_set_osd_region(
  std::shared_ptr<Flow> &enc_flow, OsdRegionData *region_data) {
  if (!enc_flow || !region_data)
    return -EINVAL;

  void *rdata = (void *)malloc(sizeof(OsdRegionData));
  memcpy(rdata, (void *)region_data, sizeof(OsdRegionData));

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  pbuff->SetPtr(rdata, sizeof(OsdRegionData));
  enc_flow->Control(VideoEncoder::kOSDDataChange, pbuff);

  return 0;
}

int video_encoder_set_move_detection(std::shared_ptr<Flow> &enc_flow,
  std::shared_ptr<Flow> &md_flow) {
  int ret = 0;
  void **rdata = (void **)malloc(sizeof(void *));
  *rdata = md_flow.get();

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  pbuff->SetPtr(rdata, sizeof(sizeof(void *)));
  ret = enc_flow->Control(easymedia::VideoEncoder::kMoveDetectionFlow, pbuff);

  return ret;
}

int video_encoder_set_roi_regions(std::shared_ptr<Flow> &enc_flow,
  EncROIRegion *regions, int region_cnt) {
  if (!enc_flow)
    return -EINVAL;

  int rsize = 0;
  void *rdata = NULL;
  if (regions && region_cnt) {
    rsize = sizeof(EncROIRegion) * region_cnt;
    rdata = (void *)malloc(rsize);
    memcpy(rdata, (void *)regions, rsize);
  }
  auto pbuff = std::make_shared<ParameterBuffer>(0);
  pbuff->SetPtr(rdata, rsize);
  enc_flow->Control(VideoEncoder::kROICfgChange, pbuff);
  return 0;
}

int video_encoder_enable_statistics(
  std::shared_ptr<Flow> &enc_flow, int enable) {
  if (!enc_flow)
    return -EINVAL;

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  pbuff->SetValue(enable);
  enc_flow->Control(VideoEncoder::kEnableStatistics, pbuff);

  return 0;
}

} // namespace easymedia
