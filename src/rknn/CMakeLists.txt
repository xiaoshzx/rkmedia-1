#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

set(EASY_MEDIA_NN_SOURCE_FILES rknn/rknn_utils.cc)
set(EASY_MEDIA_NN_DEPENDENT_LIBS)

if(RKNN)
  include_directories(${RKNPU_HEADER_DIR})
  set(EASY_MEDIA_NN_SOURCE_FILES ${EASY_MEDIA_NN_SOURCE_FILES}
                                   rknn/rknn.cc)
  set(EASY_MEDIA_NN_DEPENDENT_LIBS ${EASY_MEDIA_NN_DEPENDENT_LIBS}
                                     rknn_runtim)
endif()

if(ROCKFACE)
  set(EASY_MEDIA_NN_SOURCE_FILES ${EASY_MEDIA_NN_SOURCE_FILES}
                                   rknn/draw_filter.cc
                                   rknn/rockface_detect.cc
                                   rknn/rockface_bodydetect.cc)
  set(EASY_MEDIA_NN_DEPENDENT_LIBS ${EASY_MEDIA_NN_DEPENDENT_LIBS}
                                     rknn_api rockface)
endif()

if(ROCKX)
    set(EASY_MEDIA_ROCKX_SOURCE_FILES rknn/rockx_filter.cc)
    set(EASY_MEDIA_SOURCE_FILES ${EASY_MEDIA_SOURCE_FILES}
                                ${EASY_MEDIA_ROCKX_SOURCE_FILES})
endif()

set(EASY_MEDIA_SOURCE_FILES ${EASY_MEDIA_SOURCE_FILES}
                            ${EASY_MEDIA_NN_SOURCE_FILES} PARENT_SCOPE)
set(EASY_MEDIA_DEPENDENT_LIBS ${EASY_MEDIA_DEPENDENT_LIBS}
                              ${EASY_MEDIA_NN_DEPENDENT_LIBS} PARENT_SCOPE)
