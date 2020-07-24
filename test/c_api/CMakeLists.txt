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
install(TARGETS rkmedia_vi_venc_test RUNTIME DESTINATION "bin")

#--------------------------
# rkmedia_venc_osd_test
#--------------------------
add_executable(rkmedia_venc_osd_test rkmedia_venc_osd_test.c)
add_dependencies(rkmedia_venc_osd_test easymedia)
target_link_libraries(rkmedia_venc_osd_test easymedia)
target_include_directories(rkmedia_venc_osd_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_venc_osd_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_audio_test
#--------------------------
add_executable(rkmedia_audio_test rkmedia_audio_test.c)
add_dependencies(rkmedia_audio_test easymedia)
target_link_libraries(rkmedia_audio_test easymedia)
target_include_directories(rkmedia_audio_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_audio_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_vi_md_test
#--------------------------
add_executable(rkmedia_vi_md_test rkmedia_vi_md_test.c)
add_dependencies(rkmedia_vi_md_test easymedia)
target_link_libraries(rkmedia_vi_md_test easymedia)
target_include_directories(rkmedia_vi_md_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_vi_md_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_venc_jpeg_test
#--------------------------
add_executable(rkmedia_venc_jpeg_test rkmedia_venc_jpeg_test.c)
add_dependencies(rkmedia_venc_jpeg_test easymedia)
target_link_libraries(rkmedia_venc_jpeg_test easymedia)
target_include_directories(rkmedia_venc_jpeg_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_venc_jpeg_test RUNTIME DESTINATION "bin")