#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

project(easymedia_flow_test)

set(FLOW_TEST_DEPENDENT_LIBS easymedia)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DDEBUG)

#--------------------------
# FlowTest
#--------------------------
find_package(GTest)

if(GTest_FOUND)
add_executable(FlowTest FlowTest.cc)

target_link_libraries(FlowTest
    PRIVATE
    GTest::GTest
    GTest::Main
    easymedia
)

target_include_directories(FlowTest
    PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
target_compile_features(FlowTest PRIVATE cxx_std_11)

add_test(AllTestsInEasyMedia FlowTest)
endif()

#--------------------------
# flow_event_test
#--------------------------
add_definitions(-DFLOW_EVENT_TEST)
set(FLOW_EVENT_TEST_SRC_FILES flow_event_test.cc)
add_executable(flow_event_test ${FLOW_EVENT_TEST_SRC_FILES})
target_link_libraries(flow_event_test easymedia)
target_include_directories(flow_event_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(flow_event_test PRIVATE cxx_std_11)
install(TARGETS flow_event_test RUNTIME DESTINATION "bin")

#--------------------------
# link_flow_test
#--------------------------
set(LINK_FLOW_TEST_SRC_FILES link_flow_test.cc)
add_executable(link_flow_test ${LINK_FLOW_TEST_SRC_FILES})
target_link_libraries(link_flow_test easymedia)
target_include_directories(link_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(link_flow_test PRIVATE cxx_std_11)
install(TARGETS link_flow_test RUNTIME DESTINATION "bin")

if(FFMPEG)
#--------------------------
# audio_encoder_flow_test
#--------------------------
if(ENCODER)
  set(AUDIO_ENCODER_FLOW_TEST_SRC_FILES audio_encoder_flow_test.cc)
  add_executable(audio_encoder_flow_test ${AUDIO_ENCODER_FLOW_TEST_SRC_FILES})
  target_link_libraries(audio_encoder_flow_test easymedia)
  target_include_directories(audio_encoder_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(audio_encoder_flow_test PRIVATE cxx_std_11)
  install(TARGETS audio_encoder_flow_test RUNTIME DESTINATION "bin")
endif()#ENCODER

#--------------------------
# audio_encoder_flow_test
#--------------------------
if(MUXER)
  set(MUXER_FLOW_TEST_SRC_FILES muxer_flow_test.cc)
  add_executable(muxer_flow_test ${MUXER_FLOW_TEST_SRC_FILES})
  target_link_libraries(muxer_flow_test easymedia)
  target_include_directories(muxer_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(muxer_flow_test PRIVATE cxx_std_11)
  install(TARGETS muxer_flow_test RUNTIME DESTINATION "bin")
endif()#MUXER
endif()#FFMPEG

if(RKMPP)
if(RKMPP_ENCODER)
#--------------------------
# video_encoder_flow_test
#--------------------------
  set(VIDEO_ENCODER_FLOW_TEST_SRC_FILES video_encoder_flow_test.cc)
  add_executable(video_encoder_flow_test ${VIDEO_ENCODER_FLOW_TEST_SRC_FILES})
  target_link_libraries(video_encoder_flow_test easymedia)
  target_include_directories(video_encoder_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(video_encoder_flow_test PRIVATE cxx_std_11)
  install(TARGETS video_encoder_flow_test RUNTIME DESTINATION "bin")

#--------------------------
# video_encoder_bps_test
#--------------------------
  set(VIDEO_ENCODER_BPS_TEST_SRC_FILES video_encoder_bps_test.cc)
  add_executable(video_encoder_bps_test ${VIDEO_ENCODER_BPS_TEST_SRC_FILES})
  target_link_libraries(video_encoder_bps_test easymedia)
  target_include_directories(video_encoder_bps_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(video_encoder_bps_test PRIVATE cxx_std_11)
  install(TARGETS video_encoder_bps_test RUNTIME DESTINATION "bin")

#--------------------------
# video_encoder_roi_test
#--------------------------
  set(VIDEO_ENCODER_ROI_TEST_SRC_FILES video_encoder_roi_test.cc)
  add_executable(video_encoder_roi_test ${VIDEO_ENCODER_ROI_TEST_SRC_FILES})
  target_link_libraries(video_encoder_roi_test easymedia)
  target_include_directories(video_encoder_roi_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(video_encoder_roi_test PRIVATE cxx_std_11)
  install(TARGETS video_encoder_roi_test RUNTIME DESTINATION "bin")

#--------------------------
# video_encoder_osd_test
#--------------------------
if(RKMPP_ENCODER_OSD)
  set(VIDEO_ENCODER_OSD_TEST_SRC_FILES video_encoder_osd_test.cc)
  add_executable(video_encoder_osd_test ${VIDEO_ENCODER_OSD_TEST_SRC_FILES})
  target_link_libraries(video_encoder_osd_test easymedia)
  target_include_directories(video_encoder_osd_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(video_encoder_osd_test PRIVATE cxx_std_11)
  install(TARGETS video_encoder_osd_test RUNTIME DESTINATION "bin")
endif()#RKMPP_ENCODER_OSD
endif()#RKMPP_ENCODER
endif()#RKMPP

if(FILTER)
#--------------------------
# rga_filter_flow_test
#--------------------------
  set(RGA_FILTER_FLOW_TEST_SRC_FILES rga_filter_flow_test.cc)
  add_executable(rga_filter_flow_test ${RGA_FILTER_FLOW_TEST_SRC_FILES})
  target_link_libraries(rga_filter_flow_test easymedia)
  target_include_directories(rga_filter_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(rga_filter_flow_test PRIVATE cxx_std_11)
  install(TARGETS rga_filter_flow_test RUNTIME DESTINATION "bin")
endif()#FILTER

if(ALSA_PLAYBACK AND ALSA_CAPTURE)
#--------------------------
# audio_loop_test
#--------------------------
set(AUDIO_LOOP_TEST_SRC_FILES audio_loop_test.cc)
add_executable(audio_loop_test ${AUDIO_LOOP_TEST_SRC_FILES})
target_link_libraries(audio_loop_test easymedia)
target_include_directories(audio_loop_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(audio_loop_test PRIVATE cxx_std_11)
install(TARGETS audio_loop_test RUNTIME DESTINATION "bin")
endif()#ALSA_PLAYBACK AND ALSA_CAPTURE

if(MOVE_DETECTION)
#--------------------------
# move_detection_flow_test
#--------------------------
set(MOVE_DETECTION_FLOW_TEST_SRC_FILES move_detection_flow_test.cc)
add_executable(move_detection_flow_test ${MOVE_DETECTION_FLOW_TEST_SRC_FILES})
target_link_libraries(move_detection_flow_test easymedia)
target_include_directories(move_detection_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(move_detection_flow_test PRIVATE cxx_std_11)
install(TARGETS move_detection_flow_test RUNTIME DESTINATION "bin")

#--------------------------
# video_smart_encoder_test
#--------------------------
if(RKMPP_ENCODER)
  set(SMART_ENCODER_TEST_SRC_FILES video_smart_encoder_test.cc)
  add_executable(video_smart_encoder_test ${SMART_ENCODER_TEST_SRC_FILES})
  target_link_libraries(video_smart_encoder_test easymedia)
  target_include_directories(video_smart_encoder_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(video_smart_encoder_test PRIVATE cxx_std_11)
  install(TARGETS video_smart_encoder_test RUNTIME DESTINATION "bin")
endif()
endif()#RKMPP_ENCODER_OSD
