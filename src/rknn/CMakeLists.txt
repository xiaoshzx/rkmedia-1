#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

option(RKNN "compile: rknn wrapper" OFF)
if(RKNN)
  set(EASY_MEDIA_RKNN_SOURCE_FILES rknn/rknn.cc)
  set(EASY_MEDIA_SOURCE_FILES ${EASY_MEDIA_SOURCE_FILES}
                              ${EASY_MEDIA_RKNN_SOURCE_FILES} PARENT_SCOPE)
  set(EASY_MEDIA_DEPENDENT_LIBS
      ${EASY_MEDIA_DEPENDENT_LIBS} rknn_runtime
      PARENT_SCOPE)
endif()
