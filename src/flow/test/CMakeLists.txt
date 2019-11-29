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


option(FLOW_EVENT_TEST "compile: flow wrapper test" ON)

if(FLOW_EVENT_TEST)
  add_definitions(-DFLOW_EVENT_TEST)
  set(FLOW_EVENT_TEST_SRC_FILES flow_event_test.cc)
  add_executable(flow_event_test ${FLOW_EVENT_TEST_SRC_FILES})
  target_link_libraries(flow_event_test easymedia)
  target_include_directories(flow_event_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
  target_compile_features(flow_event_test PRIVATE cxx_std_11)
  install(TARGETS flow_event_test RUNTIME DESTINATION "bin")
endif()
