#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
find_package(GTest REQUIRED)

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