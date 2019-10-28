#
# Copyright 2019 Fuzhou Rockchip Electronics Co., Ltd. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#

# vi: set noexpandtab syntax=cmake:

message(STATUS "cmake version ${CMAKE_VERSION}")
if(NOT CMAKE_VERSION VERSION_LESS "2.8.12.20131121")
  cmake_policy(SET CMP0025 OLD) # report Apple's Clang as just Clang
  cmake_policy(SET CMP0042 OLD) # do not require MACOSX_RPATH
endif()

# Search packages for host system instead of packages for target system in case
# of cross compilation these macro should be defined by toolchain file
if(NOT COMMAND find_host_package)
  macro(find_host_package)
    find_package(${ARGN})
  endmacro()
endif()
if(NOT COMMAND find_host_program)
  macro(find_host_program)
    find_program(${ARGN})
  endmacro()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

project(easymedia)

cmake_minimum_required(VERSION 3.1.0 FATAL_ERROR)
include(CheckIncludeFiles)
include(CheckFunctionExists)
include(CheckSymbolExists)
include(CheckCXXCompilerFlag)

# ----------------------------------------------------------------------------
# set property to classify library kinds
# ----------------------------------------------------------------------------
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
set_property(GLOBAL PROPERTY PREDEFINED_TARGETS_FOLDER "CMakeTargets")
# ----------------------------------------------------------------------------
# enable test in this project
# ----------------------------------------------------------------------------
if(NOT DEFINED BUILD_TESTING)
  option(BUILD_TESTING "Build easymedia with testing." OFF)
  mark_as_advanced(BUILD_TESTING)
endif()
include(CTest)
if(BUILD_TESTING)
  enable_testing()
endif()

# ----------------------------------------------------------------------------
# Compiler detection
# ----------------------------------------------------------------------------
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
  set(CLANG true)
endif()
if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
  set(GCC true)
endif()

if(CLANG)
  # treat clang roughly like gcc
  set(GCC true)
  add_definitions(-Wall
                  -Wextra
                  -Wshadow
                  -ffast-math)
elseif(CMAKE_COMPILER_IS_GNUCXX)
  add_definitions(-Wall
                  -Wextra
                  -Wshadow
                  -ffast-math)
  check_cxx_compiler_flag(-Wno-narrowing GCC_HAS_NO_NARROWING)
  check_cxx_compiler_flag(-mstackrealign GCC_HAS_STACK_REALIGN)
  if(GCC_HAS_STACK_REALIGN)
    add_definitions(-mstackrealign)
  endif()
  execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion
                  OUTPUT_VARIABLE GCC_VERSION)
endif()

if($(CMAKE_BUILD_TYPE) MATCHES "Release")
  set(CMAKE_C_VISIBILITY_PRESET hidden)
  set(CMAKE_CXX_VISIBILITY_PRESET hidden)
endif()

# set c++11
set(CMAKE_CXX_STANDARD 11)
# rtti may be expensive on Embedded Platform, obey Google C++ Style Guide
add_definitions(-fno-rtti)

# add PIC flag
add_definitions(-fPIC)
# set (CMAKE_SHARED_LINKER_FLAGS ${CMAKE_EXE_LINKER_FLAGS})

add_definitions(-fvisibility=hidden -fvisibility-inlines-hidden)

include_directories(./)

# ----------------------------------------------------------------------------
# Build options
# ----------------------------------------------------------------------------
find_package(PkgConfig)
include(GNUInstallDirs)

# ----------------------------------------------------------------------------
# Set Warning as Error
# ----------------------------------------------------------------------------
option(WARNINGS_AS_ERRORS "Stop compiles on first warning" OFF)
if(WARNINGS_AS_ERRORS)
  if(GCC)
    add_definitions(-Werror)
  elseif(MSVC)
    add_definitions(/WX)
  endif()
endif(WARNINGS_AS_ERRORS)

option(SANITIZER "compile with sanitizer" OFF)
option(FILTER "compile: filter" ON)
option(ENCODER "compile: encoder" ON)
option(DECODER "compile: decoder" ON)
option(MUXER "compile: muxer" ON)
option(DEMUXER "compile: demuxer" ON)

pkg_check_modules(LIBDRM libdrm)
if(LIBDRM_FOUND)
  add_definitions(-DLIBDRM)
  include_directories(${LIBDRM_INCLUDE_DIRS})
  set(EASY_MEDIA_DEPENDENT_LIBS drm)
else()
pkg_check_modules(LIBION libion)
if(LIBION_FOUND)
  add_definitions(-DLIBION)
  include_directories(${LIBION_INCLUDE_DIRS})
  set(EASY_MEDIA_DEPENDENT_LIBS ion)
endif()
endif()

include_directories(include/easymedia)
add_subdirectory(src)
if(BUILD_TESTING)
  add_subdirectory(test)
endif()
