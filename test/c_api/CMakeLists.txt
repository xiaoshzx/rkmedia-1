#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

project(easymedia_c_api_test)

add_definitions(-g -O0 -ggdb -gdwarf -funwind-tables -rdynamic)

add_definitions(-DDEBUG)

#--------------------------
# rkmedia_vi_venc_test
#--------------------------
add_executable(rkmedia_vi_venc_test rkmedia_vi_venc_test.c)
add_dependencies(rkmedia_vi_venc_test easymedia)
target_link_libraries(rkmedia_vi_venc_test easymedia)
target_include_directories(rkmedia_vi_venc_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_features(rkmedia_vi_venc_test PRIVATE cxx_std_11)
install(TARGETS rkmedia_vi_venc_test RUNTIME DESTINATION "bin")
