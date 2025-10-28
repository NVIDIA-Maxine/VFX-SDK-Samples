# SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
# SPDX-License-Identifier: MIT
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

# This CMakeLists.txt builds all VFXSDK sample applications
# Apps are automatically discovered based on directory naming convention (*App)
# Triton client applications are only supported and built on Unix/Linux platforms

cmake_minimum_required(VERSION 3.9)

# Triton client support is only available on Unix/Linux platforms
# On Unix, ENABLE_TRITON defaults to ON; on other platforms it defaults to OFF
if(UNIX AND NOT DEFINED ENABLE_TRITON)
  option(ENABLE_TRITON "Enable Triton client applications (Linux only)" ON)
elseif(NOT DEFINED ENABLE_TRITON)
  option(ENABLE_TRITON "Enable Triton client applications (Linux only)" OFF)
endif()

# By default, we report a warning if a sample app has missing dependencies, but we
# allow for an option to hard fail
set(SAMPLES_DEPS_ERROR_LEVEL WARNING)
option(SAMPLES_HARD_FAIL "Report fatal error when sample apps have missing dependencies" OFF)
if(SAMPLES_HARD_FAIL)
  set(SAMPLES_DEPS_ERROR_LEVEL FATAL_ERROR)
endif()

# Required dependency versions for building sample applications
set(REQUIRED_CUDA_VER "12.8" CACHE STRING "CUDA version for samples")
set(REQUIRED_TENSORRT_VER "10.9.0.34" CACHE STRING "TRT version for samples")
set(REQUIRED_CUDNN_VER "9.7.1" CACHE STRING "CUDNN version for samples")

# Automatically discover all sample apps by finding directories ending with "App"
file(GLOB APP_DIRS RELATIVE ${CMAKE_CURRENT_SOURCE_DIR} "*App")

# Build all discovered sample applications
# Triton client apps are only built if ENABLE_TRITON is ON (Unix/Linux only)
foreach(APP_DIR ${APP_DIRS})
  if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/${APP_DIR})
    string(TOLOWER ${APP_DIR} APP_DIR_LOWER)
    if(APP_DIR_LOWER MATCHES "triton")
      if(ENABLE_TRITON)
        add_subdirectory(${APP_DIR})
      endif()
    else()
      add_subdirectory(${APP_DIR})
    endif()
  endif()
endforeach()
