# SPDX-FileCopyrightText: Copyright (c) 2025-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#######################
# Interface to OpenCV #
#######################
if(NOT TARGET OpenCV AND NOT TARGET opencv346 AND NOT TARGET opencv4120)
  if(MSVC)
    # Prebuilt OpenCV is vendored per target architecture as opencv/<arch>/{include,lib,bin}.
    # The target name carries the OpenCV version, which differs between architectures.
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "ARM64|arm64|aarch64")
      set(OpenCV_ARCH arm64)
      set(OpenCV_TARGET opencv4120)
      set(OpenCV_WORLD_LIB opencv_world4120)
    else()
      set(OpenCV_ARCH x64)
      set(OpenCV_TARGET opencv346)
      set(OpenCV_WORLD_LIB opencv_world346)
    endif()

    set(OpenCV_ARCH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/opencv/${OpenCV_ARCH})
    set(OpenCV_INCLUDE_DIRS ${OpenCV_ARCH_DIR}/include ${OpenCV_ARCH_DIR}/include/opencv2)
    set(OpenCV_LIBRARY_DIR ${OpenCV_ARCH_DIR}/lib)
    set(OpenCV_BINARY_DIR ${OpenCV_ARCH_DIR}/bin)

    if(NOT EXISTS ${OpenCV_LIBRARY_DIR}/${OpenCV_WORLD_LIB}.lib)
      message(FATAL_ERROR "No ${OpenCV_ARCH} OpenCV found at ${OpenCV_LIBRARY_DIR}/${OpenCV_WORLD_LIB}.lib")
    endif()

    add_library(${OpenCV_TARGET} INTERFACE)
    target_include_directories(${OpenCV_TARGET} INTERFACE ${OpenCV_INCLUDE_DIRS})
    target_link_libraries(${OpenCV_TARGET} INTERFACE
      $<$<CONFIG:Debug>:${OpenCV_LIBRARY_DIR}/${OpenCV_WORLD_LIB}d.lib>
      $<$<NOT:$<CONFIG:Debug>>:${OpenCV_LIBRARY_DIR}/${OpenCV_WORLD_LIB}.lib>
    )
    set_target_properties(${OpenCV_TARGET} PROPERTIES
      DYNAMIC_LIBRARY_DIR "${OpenCV_BINARY_DIR}")

    add_library(OpenCV INTERFACE)
    target_link_libraries(OpenCV INTERFACE ${OpenCV_TARGET})
    set_target_properties(OpenCV PROPERTIES
      DYNAMIC_LIBRARY_DIR "${OpenCV_BINARY_DIR}")

    message(STATUS "OpenCV (${OpenCV_ARCH}): ${OpenCV_ARCH_DIR}")
  else()
    find_package(OpenCV REQUIRED
      PATHS /usr/local /usr
      PATH_SUFFIXES share/OpenCV share/opencv4)
    add_library(OpenCV INTERFACE)
    target_link_libraries(OpenCV INTERFACE ${OpenCV_LIBRARIES})

    message("OpenCV_INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS}")
    message("OpenCV_LIBRARIES ${OpenCV_LIBRARIES}")
    message("OpenCV_LIBS ${OpenCV_LIBS}")
  endif()

endif()
