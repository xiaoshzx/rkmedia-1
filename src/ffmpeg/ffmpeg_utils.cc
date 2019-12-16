// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ffmpeg_utils.h"

namespace easymedia {

enum AVPixelFormat PixFmtToAVPixFmt(PixelFormat fmt) {
  static const struct PixFmtAVPFEntry {
    PixelFormat fmt;
    enum AVPixelFormat av_fmt;
  } pix_fmt_av_pixfmt_map[] = {
      {PIX_FMT_YUV420P, AV_PIX_FMT_YUV420P},
      {PIX_FMT_NV12, AV_PIX_FMT_NV12},
      {PIX_FMT_NV21, AV_PIX_FMT_NV21},
      {PIX_FMT_YUV422P, AV_PIX_FMT_YUV422P16LE},
      {PIX_FMT_NV16, AV_PIX_FMT_NV16},
      {PIX_FMT_NV61, AV_PIX_FMT_NONE},
      {PIX_FMT_YUYV422, AV_PIX_FMT_YUYV422},
      {PIX_FMT_UYVY422, AV_PIX_FMT_UYVY422},
      {PIX_FMT_RGB332, AV_PIX_FMT_RGB8},
      {PIX_FMT_RGB565, AV_PIX_FMT_RGB565LE},
      {PIX_FMT_BGR565, AV_PIX_FMT_BGR565LE},
      {PIX_FMT_RGB888, AV_PIX_FMT_RGB24},
      {PIX_FMT_BGR888, AV_PIX_FMT_BGR24},
      {PIX_FMT_ARGB8888, AV_PIX_FMT_ARGB},
      {PIX_FMT_ABGR8888, AV_PIX_FMT_ABGR},
  };
  FIND_ENTRY_TARGET(fmt, pix_fmt_av_pixfmt_map, fmt, av_fmt)
  return AV_PIX_FMT_NONE;
}

enum AVCodecID SampleFmtToAVCodecID(SampleFormat fmt) {
  static const struct SampleFmtAVCodecIDEntry {
    SampleFormat fmt;
    enum AVCodecID av_codecid;
  } sample_fmt_av_codecid_map[] = {
      {SAMPLE_FMT_U8, AV_CODEC_ID_PCM_U8},
      {SAMPLE_FMT_S16, AV_CODEC_ID_PCM_S16LE},
      {SAMPLE_FMT_S32, AV_CODEC_ID_PCM_S32LE},
  };
  FIND_ENTRY_TARGET(fmt, sample_fmt_av_codecid_map, fmt, av_codecid)
  return AV_CODEC_ID_NONE;
}

enum AVCodecID CodecTypeToAVCodecID(CodecType type) {
  static const struct SampleFmtAVCodecIDEntry {
    CodecType type;
    enum AVCodecID av_codecid;
  } codec_type_av_codecid_map[] = {
      {CODEC_TYPE_AAC, AV_CODEC_ID_AAC},
      {CODEC_TYPE_MP2, AV_CODEC_ID_MP2},
      {CODEC_TYPE_VORBIS, AV_CODEC_ID_VORBIS},
      {CODEC_TYPE_G711A, AV_CODEC_ID_PCM_ALAW},
      {CODEC_TYPE_G711U, AV_CODEC_ID_PCM_MULAW},
      {CODEC_TYPE_G726, AV_CODEC_ID_ADPCM_G726},
      {CODEC_TYPE_H264, AV_CODEC_ID_H264},
      {CODEC_TYPE_H265, AV_CODEC_ID_H265},
  };
  FIND_ENTRY_TARGET(type, codec_type_av_codecid_map, type, av_codecid)
  return AV_CODEC_ID_NONE;
}

enum AVSampleFormat SampleFmtToAVSamFmt(SampleFormat sfmt) {
  static const struct SampleFmtAVSFEntry {
    SampleFormat fmt;
    enum AVSampleFormat av_sfmt;
  } sample_fmt_av_sfmt_map[] = {
      {SAMPLE_FMT_U8, AV_SAMPLE_FMT_U8},
      {SAMPLE_FMT_S16, AV_SAMPLE_FMT_S16},
      {SAMPLE_FMT_S32, AV_SAMPLE_FMT_S32},
      {SAMPLE_FMT_FLTP, AV_SAMPLE_FMT_FLTP},
  };
  FIND_ENTRY_TARGET(sfmt, sample_fmt_av_sfmt_map, fmt, av_sfmt)
  return AV_SAMPLE_FMT_NONE;
}

void PrintAVError(int err, const char *log, const char *mark) {
  char str[AV_ERROR_MAX_STRING_SIZE] = {0};
  av_strerror(err, str, sizeof(str));
  if (mark)
    LOG("%s '%s': %s\n", log, mark, str);
  else
    LOG("%s: %s\n", log, str);
}

} // namespace easymedia