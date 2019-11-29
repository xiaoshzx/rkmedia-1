#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

set(EASY_MEDIA_FLOW_SOURCE_FILES
    flow/video_encoder_flow.cc
    flow/decoder_flow.cc
    flow/file_flow.cc
    flow/filter_flow.cc
    flow/source_stream_flow.cc
    flow/output_stream_flow.cc)

set(EASY_MEDIA_SOURCE_FILES ${EASY_MEDIA_SOURCE_FILES}
                            ${EASY_MEDIA_FLOW_SOURCE_FILES} PARENT_SCOPE)

option(FLOW_TEST "compile: flow wrapper test" ON)
if(FLOW_TEST)
  add_subdirectory(test)
endif()
