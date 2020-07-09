// Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef __RKMEDIA_VENC_
#define __RKMEDIA_VENC_

#include "media_type.h"
#include "rkmedia_common.h"

typedef RK_U32 RK_FR32;

/* rc mode */
typedef enum rkVENC_RC_MODE_E {
  VENC_RC_MODE_H264CBR = 1,
  VENC_RC_MODE_H264VBR,

  VENC_RC_MODE_MJPEGCBR,

  VENC_RC_MODE_H265CBR,
  VENC_RC_MODE_H265VBR,
  VENC_RC_MODE_BUTT,
} VENC_RC_MODE_E;

/* the attribute of h264e cbr*/
typedef struct rkVENC_H264_CBR_S {

  RK_U32 u32Gop; // RW; Range:[1, 65536]; the interval of I Frame.
  RK_U32 u32SrcFrameRateNum;
  RK_U32 u32SrcFrameRateDen;
  RK_FR32 fr32DstFrameRateNum;
  RK_FR32 fr32DstFrameRateDen;
  RK_U32 u32BitRate; // RW; Range:[2, 614400]; average bitrate
} VENC_H264_CBR_S;

/* the attribute of h264e vbr*/
typedef struct rkVENC_H264_VBR_S {
  RK_U32 u32Gop; // RW; Range:[1, 65536]; the interval of ISLICE.
  RK_U32 u32SrcFrameRateNum;
  RK_U32 u32SrcFrameRateDen;
  RK_FR32 fr32DstFrameRateNum;
  RK_FR32 fr32DstFrameRateDen;
  RK_U32 u32MaxBitRate; // RW; Range:[2, 614400];the max bitrate
  RK_U32 u32MinBitRate;
} VENC_H264_VBR_S;

/* the attribute of mjpege cbr*/
typedef struct hiVENC_MJPEG_CBR_S {
  RK_U32 u32SrcFrameRateNum;
  RK_U32 u32SrcFrameRateDen;
  RK_FR32 fr32DstFrameRateNum;
  RK_FR32 fr32DstFrameRateDen;
  RK_U32 u32BitRate; // RW; Range:[2, 614400]; average bitrate
} VENC_MJPEG_CBR_S;

typedef struct rkVENC_H264_CBR_S VENC_H265_CBR_S;
typedef struct rkVENC_H264_VBR_S VENC_H265_VBR_S;

/* the attribute of rc*/
typedef struct rkVENC_RC_ATTR_S {
  /* RW; the type of rc*/
  VENC_RC_MODE_E enRcMode;
  union {
    VENC_H264_CBR_S stH264Cbr;
    VENC_H264_VBR_S stH264Vbr;

    VENC_MJPEG_CBR_S stMjpegCbr;

    VENC_H265_CBR_S stH265Cbr;
    VENC_H265_VBR_S stH265Vbr;
  };
} VENC_RC_ATTR_S;

/* the gop mode */
typedef enum rkVENC_GOP_MODE_E {
  VENC_GOPMODE_SVG = 0,
  VENC_GOPMODE_TSVG = 1,
  VENC_GOPMODE_BUTT,
} VENC_GOP_MODE_E;

/*the attribute of jpege*/
typedef struct rkVENC_ATTR_JPEG_S {
  // reserved
} VENC_ATTR_JPEG_S;

/*the attribute of mjpege*/
typedef struct rkVENC_ATTR_MJPEG_S {
  // reserved
} VENC_ATTR_MJPEG_S;

/*the attribute of h264e*/
typedef struct rkVENC_ATTR_H264_S {

  // reserved
} VENC_ATTR_H264_S;

/*the attribute of h265e*/
typedef struct rkVENC_ATTR_H265_S {

  // reserved
} VENC_ATTR_H265_S;

/* the attribute of the Venc*/
typedef struct rkVENC_ATTR_S {

  CodecType enType;       // RW; the type of encodec
  IMAGE_TYPE_E imageType; // the type of input image
  RK_U32 u32VirWidth;  // stride width, same to buffer_width, must greater than
                       // width, often set vir_width=(width+15)&(~15)
  RK_U32 u32VirHeight; // stride height, same to buffer_height, must greater
                       // than height, often set vir_height=(height+15)&(~15)
  RK_U32 u32Profile;   // RW; Range:[0,100];
                       // H.264:   66: baseline; 77:MP; 100:HP;
                       // H.265:   default:Main;
                       // Jpege/MJpege:   default:Baseline
  RK_BOOL bByFrame;    // RW; Range:[0,1];
                       // get stream mode is slice mode or frame mode
  RK_U32 u32PicWidth;  // RW; Range:[0,16384];width of a picture to be encoded,
                       // in pixel
  RK_U32 u32PicHeight; // RW; Range:[0,16384];height of a picture to be encoded,
                       // in pixel
  union {
    VENC_ATTR_H264_S stAttrH264e;   // attributes of H264e
    VENC_ATTR_H265_S stAttrH265e;   // attributes of H265e
    VENC_ATTR_MJPEG_S stAttrMjpege; // attributes of Mjpeg
    VENC_ATTR_JPEG_S stAttrJpege;   // attributes of jpeg
  };
} VENC_ATTR_S;

/* the attribute of the gop*/
typedef struct rkVENC_GOP_ATTR_S {
  VENC_GOP_MODE_E enGopMode; // RW; Encoding GOP type
} VENC_GOP_ATTR_S;

/* the attribute of the venc chnl*/
typedef struct rkVENC_CHN_ATTR_S {
  VENC_ATTR_S stVencAttr;    // the attribute of video encoder
  VENC_RC_ATTR_S stRcAttr;   // the attribute of rate  ctrl
  VENC_GOP_ATTR_S stGopAttr; // the attribute of gop
} VENC_CHN_ATTR_S;

#endif // #ifndef __RKMEDIA_VENC_