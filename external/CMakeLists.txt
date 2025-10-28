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

#######################
# Interface to OpenCV #
#######################
if(NOT TARGET opencv346)
  if(MSVC)
    if(CMAKE_CL_64)
      set(OpenCV_ARCH x64)
    elseif((CMAKE_GENERATOR MATCHES "ARM") OR ("${arch_hint}" STREQUAL "ARM") OR (CMAKE_VS_EFFECTIVE_PLATFORMS MATCHES "ARM|arm"))
      # see Modules/CmakeGenericSystem.cmake
      set(OpenCV_ARCH ARM)
    else()
      set(OpenCV_ARCH x86)
    endif()
    if(MSVC_VERSION GREATER_EQUAL 1920)
      #set(OpenCV_RUNTIME vc16)
      message("No Visual Studio 2019 OpenCV library available; trying 2017 library instead")
      set(OpenCV_RUNTIME vc15)
    elseif(MSVC_VERSION GREATER_EQUAL 1910)
      set(OpenCV_RUNTIME vc15)
    elseif(MSVC_VERSION GREATER_EQUAL 1900)
      set(OpenCV_RUNTIME vc14)
    else()
      message("MSVC_VERSION ${MSVC_VERSION} is not accommodated")
    endif()

    add_library(opencv346 INTERFACE)
    set(OpenCV_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/opencv/include ${CMAKE_CURRENT_SOURCE_DIR}/opencv/include/opencv2)
    target_link_libraries(opencv346 INTERFACE
      $<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/opencv/lib/opencv_world346d.lib>
      $<$<CONFIG:Release>:${CMAKE_CURRENT_SOURCE_DIR}/opencv/lib/opencv_world346.lib>
      $<$<CONFIG:RelWithDebInfo>:${CMAKE_CURRENT_SOURCE_DIR}/opencv/lib/opencv_world346.lib>
    )
    set(OpenCV_INCLUDE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/opencv/include ${CMAKE_CURRENT_SOURCE_DIR}/opencv/include/opencv2)
    target_include_directories(opencv346 INTERFACE ${OpenCV_INCLUDE_DIRS})
    set_target_properties(opencv346 PROPERTIES
      DYNAMIC_LIBRARY_DIR "${CMAKE_CURRENT_SOURCE_DIR}/opencv/bin")
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
