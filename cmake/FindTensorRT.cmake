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

if(NOT TARGET TensorRT::nvinfer)
  find_path(TensorRT_DIR
    NAMES include/NvInfer.h
    PATHS
    /usr/local/ARSDK/external/tensorrt
    /usr/local/TensorRT-10.9.0.34
    /usr
  )
  set(TensorRT_INCLUDE_DIRS ${TensorRT_DIR}/include)
  foreach(lib
    nvinfer
    nvinfer_dispatch
    nvinfer_plugin
    nvonnxparser
  )
    # XXX - tensorrt hardcoded
    find_library(TensorRT_${lib}_LIBRARY
      ${lib}
      lib${lib}.so.10.9.0
      lib${lib}.so.10
      PATHS
      ${TensorRT_DIR}/lib
      ${TensorRT_DIR}/lib/x86_64-linux-gnu
      ${TensorRT_DIR}/lib64
    )
    add_library(TensorRT::${lib} IMPORTED INTERFACE)
    set_target_properties(TensorRT::${lib} PROPERTIES
      INTERFACE_LINK_LIBRARIES
      ${TensorRT_${lib}_LIBRARY}
      INTERFACE_INCLUDE_DIRECTORIES
      ${TensorRT_INCLUDE_DIRS}
    )
    list(APPEND TensorRT_LIBRARIES ${TensorRT_${lib}_LIBRARY})
  endforeach()
  message("TensorRT_DIR: ${TensorRT_DIR}")
  message("TensorRT_INCLUDE_DIRS: ${TensorRT_INCLUDE_DIRS}")
  message("TensorRT_LIBRARIES: ${TensorRT_LIBRARIES}")
endif()
