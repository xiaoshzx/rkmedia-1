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


option(FLOW_EVENT_TEST "compile: flow event test" ON)

if(FLOW_EVENT_TEST)
  add_definitions(-DFLOW_EVENT_TEST)
  set(FLOW_EVENT_TEST_SRC_FILES flow_event_test.cc)
  add_executable(flow_event_test ${FLOW_EVENT_TEST_SRC_FILES})
  target_link_libraries(flow_event_test easymedia)
  target_include_directories(flow_event_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(flow_event_test PRIVATE cxx_std_11)
  install(TARGETS flow_event_test RUNTIME DESTINATION "bin")
endif()

option(AUDIO_ENCODER_FLOW_TEST "compile: audio encoder flow test" ON)

if(AUDIO_ENCODER_FLOW_TEST)
  set(AUDIO_ENCODER_FLOW_TEST_SRC_FILES audio_encoder_flow_test.cc)
  add_executable(audio_encoder_flow_test ${AUDIO_ENCODER_FLOW_TEST_SRC_FILES})
  target_link_libraries(audio_encoder_flow_test easymedia)
  target_include_directories(audio_encoder_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(audio_encoder_flow_test PRIVATE cxx_std_11)
  install(TARGETS audio_encoder_flow_test RUNTIME DESTINATION "bin")
endif()

option(LINK_FLOW_TEST "compile: link flow test" ON)

if(LINK_FLOW_TEST)
  set(LINK_FLOW_TEST_SRC_FILES link_flow_test.cc)
  add_executable(link_flow_test ${LINK_FLOW_TEST_SRC_FILES})
  target_link_libraries(link_flow_test easymedia)
  target_include_directories(link_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(link_flow_test PRIVATE cxx_std_11)
  install(TARGETS link_flow_test RUNTIME DESTINATION "bin")
endif()

option(MUXER_FLOW_TEST "compile: muxer flow test" ON)

if(MUXER_FLOW_TEST)
  set(MUXER_FLOW_TEST_SRC_FILES muxer_flow_test.cc)
  add_executable(muxer_flow_test ${MUXER_FLOW_TEST_SRC_FILES})
  target_link_libraries(muxer_flow_test easymedia)
  target_include_directories(muxer_flow_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(muxer_flow_test PRIVATE cxx_std_11)
  install(TARGETS muxer_flow_test RUNTIME DESTINATION "bin")
endif()
