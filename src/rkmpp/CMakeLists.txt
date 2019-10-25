#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

pkg_check_modules(ROCKCHIP_MPP REQUIRED rockchip_mpp)

set(EASY_MEDIA_RKMPP_SOURCE_FILES rkmpp/mpp_inc.cc)

option(RKMPP_ENCODER "compile: rkmpp encode wrapper" OFF)
if(RKMPP_ENCODER)
  set(EASY_MEDIA_RKMPP_SOURCE_FILES ${EASY_MEDIA_RKMPP_SOURCE_FILES}
                                    rkmpp/mpp_encoder.cc
                                    rkmpp/mpp_final_encoder.cc)
endif()

option(RKMPP_DECODER "compile: rkmpp decode wrapper" OFF)
if(RKMPP_DECODER)
  set(EASY_MEDIA_RKMPP_SOURCE_FILES ${EASY_MEDIA_RKMPP_SOURCE_FILES}
                                    rkmpp/mpp_decoder.cc)
endif()

set(EASY_MEDIA_SOURCE_FILES ${EASY_MEDIA_SOURCE_FILES}
                            ${EASY_MEDIA_RKMPP_SOURCE_FILES} PARENT_SCOPE)

set(EASY_MEDIA_DEPENDENT_LIBS
    ${EASY_MEDIA_DEPENDENT_LIBS} ${RKMPP_LIB_NAME}
    PARENT_SCOPE)

option(RKMPP_TEST "compile: rkmpp wrapper test" ON)
if(RKMPP_TEST)
  add_subdirectory(test)
endif()
