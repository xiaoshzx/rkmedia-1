#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

project(easymedia_c_api_test)

set(RKMPP_TEST_DEPENDENT_LIBS easymedia)

add_definitions(-DDEBUG)

#--------------------------
# rkmedia c api test
#--------------------------

add_executable(rkmedia_c_api_test rkmedia_c_api_test.cc)
add_dependencies(rkmedia_c_api_test easymedia)
target_link_libraries(rkmedia_c_api_test easymedia)
target_include_directories(rkmedia_c_api_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(rkmedia_c_api_test PRIVATE cxx_std_11)
install(TARGETS rkmedia_c_api_test RUNTIME DESTINATION "bin")
