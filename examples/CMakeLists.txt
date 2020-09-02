#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

project(rkmedia_examples)

add_definitions(-g -O0 -ggdb -gdwarf -funwind-tables -rdynamic)

add_definitions(-DDEBUG)

aux_source_directory(common COMMON_SRC)
#--------------------------
# rkmedia_vi_venc_test
#--------------------------
add_executable(rkmedia_vi_venc_test rkmedia_vi_venc_test.c ${COMMON_SRC})
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
#  rkmedia_vi_od_test
#--------------------------
add_executable(rkmedia_vi_od_test rkmedia_vi_od_test.c)
add_dependencies(rkmedia_vi_od_test easymedia)
target_link_libraries(rkmedia_vi_od_test easymedia)
target_include_directories(rkmedia_vi_od_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_vi_od_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_venc_jpeg_test
#--------------------------
add_executable(rkmedia_venc_jpeg_test rkmedia_venc_jpeg_test.c)
add_dependencies(rkmedia_venc_jpeg_test easymedia)
target_link_libraries(rkmedia_venc_jpeg_test easymedia)
target_include_directories(rkmedia_venc_jpeg_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_venc_jpeg_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_venc_mjpeg_test
#--------------------------
add_executable(rkmedia_venc_mjpeg_test rkmedia_venc_mjpeg_test.c)
add_dependencies(rkmedia_venc_mjpeg_test easymedia)
target_link_libraries(rkmedia_venc_mjpeg_test easymedia)
target_include_directories(rkmedia_venc_mjpeg_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_venc_mjpeg_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_vi_multi_bind_test
#--------------------------
add_executable(rkmedia_vi_multi_bind_test rkmedia_vi_multi_bind_test.c)
add_dependencies(rkmedia_vi_multi_bind_test easymedia)
target_link_libraries(rkmedia_vi_multi_bind_test easymedia)
target_include_directories(rkmedia_vi_multi_bind_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_vi_multi_bind_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_venc_cover_test
#--------------------------
add_executable(rkmedia_venc_cover_test rkmedia_venc_cover_test.c)
add_dependencies(rkmedia_venc_cover_test easymedia)
target_link_libraries(rkmedia_venc_cover_test easymedia)
target_include_directories(rkmedia_venc_cover_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_venc_cover_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_venc_offline_test
#--------------------------
add_executable(rkmedia_venc_offline_test rkmedia_venc_offline_test.c)
add_dependencies(rkmedia_venc_offline_test easymedia)
target_link_libraries(rkmedia_venc_offline_test easymedia)
target_include_directories(rkmedia_venc_offline_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_venc_offline_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_venc_smartp_test
#--------------------------
add_executable(rkmedia_venc_smartp_test rkmedia_venc_smartp_test.c)
add_dependencies(rkmedia_venc_smartp_test easymedia)
target_link_libraries(rkmedia_venc_smartp_test easymedia)
target_include_directories(rkmedia_venc_smartp_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_venc_smartp_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_vi_get_frame_test
#--------------------------
add_executable(rkmedia_vi_get_frame_test rkmedia_vi_get_frame_test.c)
add_dependencies(rkmedia_vi_get_frame_test easymedia)
target_link_libraries(rkmedia_vi_get_frame_test easymedia)
target_include_directories(rkmedia_vi_get_frame_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_vi_get_frame_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_vi_rga_test
#--------------------------
add_executable(rkmedia_vi_rga_test rkmedia_vi_rga_test.c)
add_dependencies(rkmedia_vi_rga_test easymedia)
target_link_libraries(rkmedia_vi_rga_test easymedia)
target_include_directories(rkmedia_vi_rga_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_vi_rga_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_vi_work_mode_test
#--------------------------
add_executable(rkmedia_vi_work_mode_test rkmedia_vi_work_mode_test.c)
add_dependencies(rkmedia_vi_work_mode_test easymedia)
target_link_libraries(rkmedia_vi_work_mode_test easymedia)
target_include_directories(rkmedia_vi_work_mode_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_vi_work_mode_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_vi_venc_change_resolution_test
#--------------------------
add_executable(rkmedia_vi_venc_change_resolution_test rkmedia_vi_venc_change_resolution_test.c)
add_dependencies(rkmedia_vi_venc_change_resolution_test easymedia)
target_link_libraries(rkmedia_vi_venc_change_resolution_test easymedia)
target_include_directories(rkmedia_vi_venc_change_resolution_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_vi_venc_change_resolution_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_venc_roi_osd_test
#--------------------------
add_executable(rkmedia_venc_roi_osd_test rkmedia_venc_roi_osd_test.c)
add_dependencies(rkmedia_venc_roi_osd_test easymedia)
target_link_libraries(rkmedia_venc_roi_osd_test easymedia)
target_include_directories(rkmedia_venc_roi_osd_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_venc_roi_osd_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_vi_vo_test
#--------------------------
add_executable(rkmedia_vi_vo_test rkmedia_vi_vo_test.c)
add_dependencies(rkmedia_vi_vo_test easymedia)
target_link_libraries(rkmedia_vi_vo_test easymedia)
target_include_directories(rkmedia_vi_vo_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_vi_vo_test RUNTIME DESTINATION "bin")

#--------------------------
#  rkmedia_main_stream_with_jpeg_test
#--------------------------
add_executable(rkmedia_main_stream_with_jpeg_test rkmedia_main_stream_with_jpeg_test.c)
add_dependencies(rkmedia_main_stream_with_jpeg_test easymedia)
target_link_libraries(rkmedia_main_stream_with_jpeg_test easymedia)
target_include_directories(rkmedia_main_stream_with_jpeg_test PRIVATE ${CMAKE_SOURCE_DIR}/include)
install(TARGETS rkmedia_main_stream_with_jpeg_test RUNTIME DESTINATION "bin")

option(COMPILES_UINT_TEST "Enable unit tests" OFF)
if(COMPILES_UINT_TEST)
  add_subdirectory(uintTest)
endif()
