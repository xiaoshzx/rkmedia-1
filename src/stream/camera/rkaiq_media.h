// Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef _TOOL_RKAIQ_MEDIA_H_
#define _TOOL_RKAIQ_MEDIA_H_

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <string>
#include <linux/videodev2.h>

#include "rkaiq/common/mediactl/mediactl-priv.h"
#include "rkaiq/common/mediactl/mediactl.h"
#include "rkaiq/common/mediactl/v4l2subdev.h"

#define MAX_MEDIA_NUM 10
#define MAX_CAM_NUM 4

typedef struct {
  int model_idx;
  int linked_sensor;
  std::string sensor_name;
  std::string media_dev_path;
  std::string isp_dev_path;
  std::string csi_dev_path;
  std::string mpfbc_dev_path;
  std::string main_path;
  std::string self_path;
  std::string rawwr0_path;
  std::string rawwr1_path;
  std::string rawwr2_path;
  std::string rawwr3_path;
  std::string dma_path;
  std::string rawrd0_m_path;
  std::string rawrd1_l_path;
  std::string rawrd2_s_path;
  std::string stats_path;
  std::string input_params_path;
  std::string mipi_luma_path;
  std::string mipi_dphy_rx_path;
} isp_info_t;

typedef struct {
  int model_idx;
  std::string media_dev_path;
  std::string pp_input_image_path;
  std::string pp_m_bypass_path;
  std::string pp_scale0_path;
  std::string pp_scale1_path;
  std::string pp_scale2_path;
  std::string pp_input_params_path;
  std::string pp_stats_path;
  std::string pp_dev_path;
} ispp_info_t;

typedef struct {
  int model_idx;
  int linked_sensor;
  std::string sensor_name;
  std::string media_dev_path;
  std::string mipi_id0;
  std::string mipi_id1;
  std::string mipi_id2;
  std::string mipi_id3;
  std::string mipi_dphy_rx_path;
  std::string mipi_csi2_sd_path;
  std::string lvds_sd_path;
  std::string mipi_luma_path;
} cif_info_t;

typedef struct {
  isp_info_t isp;
  ispp_info_t ispp;
  cif_info_t cif;
} media_info_t;

class RKAiqMedia {
public:
  RKAiqMedia();
  virtual ~RKAiqMedia() = default;
  int IsLinkSensor(struct media_device *device);
  std::string GetSensorName(struct media_device *device);
  void GetIsppSubDevs(int id, struct media_device *device, const char *devpath);
  void GetIspSubDevs(int id, struct media_device *device, const char *devpath);
  void GetCifSubDevs(int id, struct media_device *device, const char *devpath);
  int GetMediaInfo();
  int DumpMediaInfo();
  int LinkToIsp(bool enable = false);

public:
  std::string GetSensorName(int id) {
    if (id >= MAX_CAM_NUM)
      return "";

    if (media_info[id].isp.linked_sensor)
      return media_info[id].isp.sensor_name;
    else if (media_info[id].cif.linked_sensor)
      return media_info[id].cif.sensor_name;
    return "";
  }

  std::string GetVideoNode(int camera_id, const char *chn_name) {
    if (camera_id >= MAX_CAM_NUM)
      return "";

    if (strstr(chn_name, "bypass"))
      return media_info[camera_id].ispp.pp_m_bypass_path;
    else if (strstr(chn_name, "scale0"))
      return media_info[camera_id].ispp.pp_scale0_path;
    else if (strstr(chn_name, "scale1"))
      return media_info[camera_id].ispp.pp_scale1_path;
    else if (strstr(chn_name, "scale2"))
      return media_info[camera_id].ispp.pp_scale2_path;

    return "";
  }

private:
  media_info_t media_info[MAX_CAM_NUM];
};

#endif // _TOOL_RKAIQ_MEDIA_H_
