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

std::string get_video_encoder_config_string (
  const ImageInfo &info, const VideoEncoderCfg &cfg) {
  if (!info.width || !info.height ||
      (info.pix_fmt >= PIX_FMT_NB) ||
      (info.pix_fmt <= PIX_FMT_NONE)) {
    LOG("ERROR: %s image info is wrong!\n", __func__);
    return NULL;
  }

  if (StringToCodecType(cfg.type) < 0) {
    LOG("ERROR: %s not support enc type:%s!\n",
      __func__, cfg.type);
    return NULL;
  }

  if (cfg.rc_quality &&
    strcmp(cfg.rc_quality, KEY_BEST) &&
    strcmp(cfg.rc_quality, KEY_BETTER) &&
    strcmp(cfg.rc_quality, KEY_MEDIUM) &&
    strcmp(cfg.rc_quality, KEY_WORSE) &&
    strcmp(cfg.rc_quality, KEY_WORST)) {
    LOG("ERROR: %s rc_quality is invalid!"
      "should be [KEY_WORST, KEY_BEST]\n", __func__);
    return NULL;
  }

  if (cfg.rc_mode && strcmp(cfg.rc_mode, KEY_VBR) &&
    strcmp(cfg.rc_mode, KEY_CBR)) {
    LOG("ERROR: %s rc_mode is invalid! should be KEY_VBR/KEY_VBR\n",
      __func__);
    return NULL;
  }

  MediaConfig enc_config;
  memset(&enc_config, 0, sizeof(enc_config));
  VideoConfig &vid_cfg = enc_config.vid_cfg;
  ImageConfig &img_cfg = vid_cfg.image_cfg;
  img_cfg.image_info.pix_fmt = info.pix_fmt;
  img_cfg.image_info.width = info.width;
  img_cfg.image_info.height = info.height;
  img_cfg.image_info.vir_width = info.vir_width;
  img_cfg.image_info.vir_height = info.vir_height;
  img_cfg.codec_type = StringToCodecType(cfg.type);

  if (cfg.fps)
    vid_cfg.frame_rate = cfg.fps;
  else {
    vid_cfg.frame_rate = 30;
    LOG("INFO: VideoEnc: frame rate use defalut value:30\n");
  }

  if (cfg.gop)
    vid_cfg.gop_size = cfg.gop;
  else {
    vid_cfg.gop_size = 30;
    LOG("INFO: VideoEnc: GOP use defalut value:30\n");
  }

  if (cfg.max_bps) {
    vid_cfg.bit_rate = cfg.max_bps;
  } else {
    int den, num;
    GetPixFmtNumDen(info.pix_fmt, num, den);
    int wh_product = info.width * info.height;
    if (wh_product > 2073600)
      vid_cfg.bit_rate = wh_product * vid_cfg.frame_rate * num / den / 20;
    else if (wh_product > 921600)
      vid_cfg.bit_rate = wh_product * vid_cfg.frame_rate * num / den / 17;
    else if (wh_product > 101376)
      vid_cfg.bit_rate = wh_product * vid_cfg.frame_rate * num / den / 15;
    else
      vid_cfg.bit_rate = wh_product * vid_cfg.frame_rate * num / den / 8;
    LOG("INFO: VideoEnc: maxbps use defalut value:%d\n", vid_cfg.bit_rate);
  }

  if (img_cfg.codec_type == CODEC_TYPE_H264) {
    /*
     * H.264 profile_idc parameter
     * 66  - Baseline profile
     * 77  - Main profile
     * 100 - High profile
     */
    if ((cfg.profile == 66) || (cfg.profile == 77) ||
      (cfg.profile == 100))
      vid_cfg.profile = cfg.profile;
    else {
      vid_cfg.profile = 100;
      LOG("INFO: VideoEnc: AVC profile use defalut value:100\n");
    }
    /*
     * H.264 level_idc parameter
     * 10 / 11 / 12 / 13    - qcif@15fps / cif@7.5fps / cif@15fps / cif@30fps
     * 20 / 21 / 22         - cif@30fps / half-D1@@25fps / D1@12.5fps
     * 30 / 31 / 32         - D1@25fps / 720p@30fps / 720p@60fps
     * 40 / 41 / 42         - 1080p@30fps / 1080p@30fps / 1080p@60fps
     * 50 / 51 / 52         - 4K@30fps
     */
    if (((cfg.enc_levle >= 10) && (cfg.enc_levle <= 13)) ||
      ((cfg.enc_levle >= 20) && (cfg.enc_levle <= 22)) ||
      ((cfg.enc_levle >= 30) && (cfg.enc_levle <= 32)) ||
      ((cfg.enc_levle >= 40) && (cfg.enc_levle <= 42)) ||
      ((cfg.enc_levle >= 50) && (cfg.enc_levle <= 52)))
      vid_cfg.level = cfg.enc_levle;
    else {
      vid_cfg.level = 40;
      LOG("INFO: VideoEnc: AVC level use defalut value:40\n");
    }
  }

  if (cfg.rc_quality)
    vid_cfg.rc_quality = cfg.rc_quality;
  else {
    vid_cfg.rc_quality = KEY_MEDIUM;
    LOG("INFO: VideoEnc: rc_quality use defalut value: Middle\n");
  }

  // for jpeg
  if (img_cfg.codec_type == CODEC_TYPE_JPEG) {
    if (!strcmp(vid_cfg.rc_quality, KEY_BEST))
      img_cfg.qp_init = 10;
    else if (!strcmp(vid_cfg.rc_quality, KEY_BETTER))
      img_cfg.qp_init = 8;
    else if (!strcmp(vid_cfg.rc_quality, KEY_MEDIUM))
      img_cfg.qp_init = 6;
    else if (!strcmp(vid_cfg.rc_quality, KEY_WORSE))
      img_cfg.qp_init = 3;
    else if (!strcmp(vid_cfg.rc_quality, KEY_WORST))
      img_cfg.qp_init = 1;
  } else {
    //defalut qp config for h264/h265
    img_cfg.qp_init = 26;
    vid_cfg.max_i_qp = 46;
    vid_cfg.min_i_qp = 10;
    vid_cfg.qp_max = 51;
    vid_cfg.qp_min = 1;
    vid_cfg.qp_step = 4;
  }

  if (cfg.rc_mode)
    vid_cfg.rc_mode = cfg.rc_mode;
  else {
    vid_cfg.rc_mode = KEY_VBR;
    LOG("INFO: VideoEnc: rc_mode use defalut value: KEY_VBR\n");
  }

  std::string enc_param = "";
  enc_param.append(easymedia::to_param_string(enc_config, cfg.type));
  return enc_param;
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

int video_encoder_set_rc_quality(
  std::shared_ptr<Flow> &enc_flow, const char *rc_quality) {
  if (!enc_flow || !rc_quality)
    return -EINVAL;

  if (strcmp(rc_quality, KEY_BEST) && strcmp(rc_quality, KEY_BETTER) &&
    strcmp(rc_quality, KEY_MEDIUM) && strcmp(rc_quality, KEY_WORSE) &&
    strcmp(rc_quality, KEY_WORST)) {
    LOG("ERROR: %s rc_quality:%s is invalid! should be [KEY_WORST, KEY_BEST]\n",
      __func__, rc_quality);
    return -EINVAL;
  }

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  int str_len = strlen(rc_quality);
  char *quality = (char *)malloc(str_len + 1);
  memcpy(quality, rc_quality, strlen(rc_quality));
  quality[str_len] = '\0';
  pbuff->SetPtr(quality, strlen(rc_quality));
  enc_flow->Control(VideoEncoder::kRcQualityChange, pbuff);

  return 0;
}

int video_encoder_set_rc_mode(
  std::shared_ptr<Flow> &enc_flow, const char *rc_mode) {
  if (!enc_flow || !rc_mode)
    return -EINVAL;

  if (strcmp(rc_mode, KEY_VBR) && strcmp(rc_mode, KEY_CBR)) {
    LOG("ERROR: %s rc_mode is invalid! should be KEY_VBR/KEY_VBR\n",
      __func__);
    return -EINVAL;
  }

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  int str_len = strlen(rc_mode);
  char *mode = (char *)malloc(str_len + 1);
  memcpy(mode, rc_mode, strlen(rc_mode));
  mode[str_len] = '\0';
  pbuff->SetPtr(mode, strlen(rc_mode));
  enc_flow->Control(VideoEncoder::kRcModeChange, pbuff);

  return 0;
}

int video_encoder_set_qp(
  std::shared_ptr<Flow> &enc_flow, VideoEncoderQp &qps) {
  if (!enc_flow)
    return -EINVAL;

  // qp_max       - 8 ~ 51
  // qp_min       - 0 ~ 48
  if ((qps.qp_max && ((qps.qp_max > 51) || (qps.qp_max < 8))) ||
    (qps.max_i_qp && ((qps.max_i_qp > 51) || (qps.max_i_qp < 8))) ||
    (qps.qp_min < 0) || (qps.qp_min > 48) ||
    (qps.min_i_qp < 0) || (qps.min_i_qp > 48) ||
    (qps.qp_min > qps.qp_max) || (qps.min_i_qp > qps.max_i_qp)) {
    LOG("ERROR: qp range error. qp_min:[0, 48]; qp_max:[8, 51]\n");
    return -EINVAL;
  }

  if ((qps.qp_init > qps.qp_max) || (qps.qp_init < qps.qp_min)) {
    LOG("ERROR: qp_init should be within [qp_min, qp_max]\n");
    return -EINVAL;
  }

  if (!qps.qp_step || (qps.qp_step > (qps.qp_max - qps.qp_min))) {
    LOG("ERROR: qp_step should be within (0, qp_max - qp_min]\n");
    return -EINVAL;
  }

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  VideoEncoderQp *qp_struct =
    (VideoEncoderQp *)malloc(sizeof(VideoEncoderQp));
  memcpy(qp_struct, &qps, sizeof(VideoEncoderQp));
  pbuff->SetPtr(qp_struct, sizeof(VideoEncoderQp));
  enc_flow->Control(VideoEncoder::kQPChange, pbuff);

  return 0;
}

int video_encoder_force_idr(std::shared_ptr<Flow> &enc_flow) {
  if (!enc_flow)
    return -EINVAL;

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  enc_flow->Control(VideoEncoder::kForceIdrFrame, pbuff);

  return 0;
}

int video_encoder_set_fps(
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

  if (region_data->enable &&
    ((region_data->width % 16) || (region_data->height % 16))) {
    LOG("ERROR: osd region size must be a multiple of 16x16.");
    return -EINVAL;
  }

  int buffer_size = region_data->width * region_data->height;
  OsdRegionData *rdata =
    (OsdRegionData *)malloc(sizeof(OsdRegionData) + buffer_size);
  memcpy((void *)rdata, (void *)region_data, sizeof(OsdRegionData));
  rdata->buffer = (uint8_t *)rdata + sizeof(OsdRegionData);
  memcpy(rdata->buffer, region_data->buffer, buffer_size);

  auto pbuff = std::make_shared<ParameterBuffer>(0);
  pbuff->SetPtr(rdata, sizeof(OsdRegionData) + buffer_size);
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

int video_encoder_set_split(
  std::shared_ptr<Flow> &enc_flow, unsigned int mode, unsigned int size) {
  if (!enc_flow)
    return -EINVAL;

  uint32_t *param = (uint32_t *)malloc(2 * sizeof(uint32_t));
  *param = mode;
  *(param + 1) = size;
  auto pbuff = std::make_shared<ParameterBuffer>(0);
  pbuff->SetPtr(param, 2 * sizeof(uint32_t));
  enc_flow->Control(VideoEncoder::kSplitChange, pbuff);

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
