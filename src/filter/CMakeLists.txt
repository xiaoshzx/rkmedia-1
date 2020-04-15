#
#Copyright 2020 Fuzhou Rockchip Electronics Co., Ltd.All rights reserved.
#Use of this source code is governed by a BSD - style license that can be
#found in the LICENSE file.
#

#vi : set noexpandtab syntax = cmake:

option(ANR "compile: ANR filter" OFF)

if (ANR)
  set(EASY_MEDIA_ANR_SOURCE_FILES
       filter/anr.cc)

  set(EASY_MEDIA_SOURCE_FILES
       ${EASY_MEDIA_SOURCE_FILES}
       ${EASY_MEDIA_ANR_SOURCE_FILES}
       PARENT_SCOPE)

  set(EASY_MEDIA_DEPENDENT_LIBS
       ${EASY_MEDIA_DEPENDENT_LIBS}
       rkap_anr
       rkap_common
       PARENT_SCOPE)

endif()

