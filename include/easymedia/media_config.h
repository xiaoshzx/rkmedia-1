// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EASYMEDIA_MEDIA_CONFIG_H_
#define EASYMEDIA_MEDIA_CONFIG_H_

#include "image.h"
#include "media_type.h"
#include "sound.h"
#include "flow.h"

typedef struct {
  ImageInfo image_info;
  CodecType codec_type;
  int qp_init; // h264 : 0 - 48, higher value means higher compress
               //        but lower quality
               // jpeg (quantization coefficient): 1 - 10,
               // higher value means lower compress but higher quality,
               // contrary to h264
} ImageConfig;

typedef struct {
  ImageConfig image_cfg;
  int qp_step;
  int qp_min;
  int qp_max;
  int max_i_qp; // h265 encoder.
  int min_i_qp; // h265 encoder.
  int bit_rate;
  int frame_rate;
  int trans_8x8; // h264 encoder.
  int level;
  int gop_size;
  int profile;
  // quality - quality parameter
  //    (extra CQP level means special constant-qp (CQP) mode)
  //    (extra AQ_ONLY means special aq only mode)
  // "worst", "worse", "medium", "better", "best", "cqp", "aq_only"
  const char *rc_quality;
  // rc_mode - rate control mode
  // "vbr", "cbr"
  const char *rc_mode;
} VideoConfig;

typedef struct {
  SampleInfo sample_info;
  CodecType codec_type;
  // uint64_t channel_layout;
  int bit_rate;
  float quality; // vorbis: 0.0 ~ 1.0;
} AudioConfig;

typedef struct {
  union {
    VideoConfig vid_cfg;
    ImageConfig img_cfg;
    AudioConfig aud_cfg;
  };
  Type type;
} MediaConfig;

#define OSD_REGIONS_CNT 8

typedef struct  {
  uint8_t *buffer; //Content: ID of palette
  uint32_t pos_x;
  uint32_t pos_y;
  uint32_t width;
  uint32_t height;
  uint32_t inverse;
  uint32_t region_id; // max = 8.
  uint8_t enable;
} OsdRegionData;

typedef struct  {
  uint16_t x;            /**< horizontal position of top left corner */
  uint16_t y;            /**< vertical position of top left corner */
  uint16_t w;            /**< width of ROI rectangle */
  uint16_t h;            /**< height of ROI rectangle */
  uint16_t intra;        /**< flag of forced intra macroblock */
  uint16_t quality;      /**<  qp of macroblock */
  uint16_t qp_area_idx;  /**< qp min max area select*/
  uint8_t  area_map_en;  /**< enable area map */
  uint8_t  abs_qp_en;    /**< absolute qp enable flag*/
} EncROIRegion;

typedef struct {
  char *type;
  uint32_t max_bps;
  //KEY_WORST/KEY_WORSE/KEY_MEDIUM/KEY_BETTER/KEY_BEST
  const char *rc_quality;
  //KEY_VBR/KEY_CBR
  const char *rc_mode;
  uint16_t fps;
  uint16_t gop;
  // For AVC
  uint8_t profile;
  uint8_t enc_levle;
} VideoEncoderCfg;

typedef struct {
  int qp_init;
  int qp_step;
  int qp_min; //0~48
  int qp_max; //8-51
  int min_i_qp;
  int max_i_qp;
} VideoEncoderQp;

#include <map>

namespace easymedia {
extern const char *rc_quality_strings[7];
extern const char *rc_mode_strings[2];
bool ParseMediaConfigFromMap(std::map<std::string, std::string> &params,
                             MediaConfig &mc);
_API std::vector<EncROIRegion> StringToRoiRegions(
  const std::string &str_regions);
_API std::string to_param_string(const ImageConfig &img_cfg);
_API std::string to_param_string(const VideoConfig &vid_cfg);
_API std::string to_param_string(const AudioConfig &aud_cfg);
_API std::string to_param_string(const MediaConfig &mc,
                                 const std::string &out_type);
_API std::string get_video_encoder_config_string (
  const ImageInfo &info, const VideoEncoderCfg &cfg);
_API int video_encoder_set_maxbps(
  std::shared_ptr<Flow> &enc_flow, unsigned int bpsmax);
// rc_quality Ranges:
//   KEY_WORST/KEY_WORSE/KEY_MEDIUM/KEY_BETTER/KEY_BEST
_API int video_encoder_set_rc_quality(
  std::shared_ptr<Flow> &enc_flow, const char *rc_quality);
// rc_mode Ranges:KEY_VBR/KEY_CBR
_API int video_encoder_set_rc_mode(
  std::shared_ptr<Flow> &enc_flow, const char *rc_mode);
_API int video_encoder_set_qp(
  std::shared_ptr<Flow> &enc_flow, VideoEncoderQp &qps);
_API int video_encoder_force_idr(std::shared_ptr<Flow> &enc_flow);
_API int video_encoder_set_fps(
  std::shared_ptr<Flow> &enc_flow, uint8_t num, uint8_t den);
_API int video_encoder_set_osd_plt(
  std::shared_ptr<Flow> &enc_flow, uint32_t *yuv_plt);
_API int video_encoder_set_osd_region(
  std::shared_ptr<Flow> &enc_flow, OsdRegionData *region_data);
_API int video_encoder_set_move_detection(std::shared_ptr<Flow> &enc_flow,
  std::shared_ptr<Flow> &md_flow);
_API int video_encoder_set_roi_regions(std::shared_ptr<Flow> &enc_flow,
  EncROIRegion *regions, int region_cnt);
_API int video_encoder_set_roi_regions(std::shared_ptr<Flow> &enc_flow,
  std::string roi_param);
_API int video_encoder_set_gop_size(std::shared_ptr<Flow> &enc_flow,
  int gop);
// mode: slice split mode
// 0 - No slice is split
// 1 - Slice is split by byte number
// 2 - Slice is split by macroblock / ctu number
//
// szie: slice split size parameter
// When split by byte number this value is the max byte number for each slice.
// When split by macroblock / ctu number this value is the MB/CTU number
// for each slice.
_API int video_encoder_set_split(
  std::shared_ptr<Flow> &enc_flow, unsigned int mode, unsigned int size);
_API int video_encoder_enable_statistics(
  std::shared_ptr<Flow> &enc_flow, int enable);
} // namespace easymedia

#endif // #ifndef EASYMEDIA_MEDIA_CONFIG_H_
