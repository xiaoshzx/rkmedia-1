#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

set(EASY_MEDIA_RKMPP_SOURCE_FILES rkmpp/mpp_inc.cc)

option(RKMPP_ENCODER "compile: rkmpp encode wrapper" OFF)
if(RKMPP_ENCODER)
  set(EASY_MEDIA_RKMPP_SOURCE_FILES ${EASY_MEDIA_RKMPP_SOURCE_FILES}
                                    rkmpp/mpp_encoder.cc
                                    rkmpp/mpp_final_encoder.cc)
  if (RKMPP_ENCODER_OSD)
    set(EASY_MEDIA_RKMPP_SOURCE_FILES ${EASY_MEDIA_RKMPP_SOURCE_FILES}
                                      rkmpp/osd_caches/osd_producer.cc
                                      rkmpp/osd_caches/osd_cache.cc
                                      rkmpp/osd_caches/font_factory.cc
                                      rkmpp/osd_caches/color_table.cc
                                      rkmpp/osd_caches/bmp_reader.cc)
  endif()
endif()

option(RKMPP_DECODER "compile: rkmpp decode wrapper" OFF)
if(RKMPP_DECODER)
  set(EASY_MEDIA_RKMPP_SOURCE_FILES ${EASY_MEDIA_RKMPP_SOURCE_FILES}
                                    rkmpp/mpp_decoder.cc)
endif()

set(EASY_MEDIA_SOURCE_FILES ${EASY_MEDIA_SOURCE_FILES}
                            ${EASY_MEDIA_RKMPP_SOURCE_FILES} PARENT_SCOPE)

option(RKMPP_TEST "compile: rkmpp wrapper test" ON)
if(RKMPP_TEST)
  add_subdirectory(test)
endif()
