#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

project(easymedia_drm_display_test)

set(DRM_DISPLAY_TEST_DEPENDENT_LIBS easymedia)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DDEBUG)

set(DRM_DISPLAY_TEST_SRC_FILES drm_display_test.cc)
add_executable(drm_display_test ${DRM_DISPLAY_TEST_SRC_FILES})
add_dependencies(drm_display_test easymedia)
target_link_libraries(drm_display_test ${DRM_DISPLAY_TEST_DEPENDENT_LIBS})
install(TARGETS drm_display_test RUNTIME DESTINATION "bin")
