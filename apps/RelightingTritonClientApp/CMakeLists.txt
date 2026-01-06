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

#############################
# RelightingTritonClientApp #
#############################
set(RUN_FILES
  run_relightingtritonclientapp.sh
  setup_env.sh)

set(SOURCE_FILES
  RelightingTritonClientApp.cpp
  ${VFXSDKSampleApps_UTILS_DIR}/batchUtilities.cpp)

add_executable(RelightingTritonClientApp
  README.md
  ${SOURCE_FILES})

if(TARGET OpenCV)
  set(OPENCV OpenCV)
else()
  set(OPENCV opencv346)
endif()

target_include_directories(RelightingTritonClientApp PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${VFXSDKSampleApps_UTILS_DIR}
)
target_link_libraries(RelightingTritonClientApp PRIVATE
  ${OPENCV}
  NVVideoEffects
  NVCVImage
)

target_link_libraries(RelightingTritonClientApp PRIVATE nvVFXRelighting)

get_target_property(NVVFX_DYNAMIC_LIBRARY_DIR NVVideoEffects DYNAMIC_LIBRARY_DIR)
get_target_property(NVVFX_MODEL_DIR NVVideoEffects MODEL_DIR)

# Configure run-files
foreach(RUN_FILE IN LISTS RUN_FILES)
  # Note: Separate configure_file from file copy for older CMake version to support file permissions
  configure_file(${RUN_FILE}.in ${CMAKE_CURRENT_BINARY_DIR}/tmp/${RUN_FILE} @ONLY)
  file(COPY ${CMAKE_CURRENT_BINARY_DIR}/tmp/${RUN_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_files FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE)
  file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/tmp/)
  add_custom_command(TARGET RelightingTritonClientApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/run_files/${RUN_FILE}
    $<TARGET_FILE_DIR:RelightingTritonClientApp>)
endforeach()

# Add resources and README.md file
add_custom_command(TARGET RelightingTritonClientApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  ${VFXSDKSampleApps_RESOURCES_DIR}/relighting_video.mp4
  ${VFXSDKSampleApps_RESOURCES_DIR}/Warmer.hdr
  ${VFXSDKSampleApps_RESOURCES_DIR}/Warm.hdr
  ${VFXSDKSampleApps_RESOURCES_DIR}/Default.hdr
  ${VFXSDKSampleApps_RESOURCES_DIR}/Cool.hdr
  ${VFXSDKSampleApps_RESOURCES_DIR}/Cooler.hdr
  ${VFXSDKSampleApps_RESOURCES_DIR}/input1.jpg
  $<TARGET_FILE_DIR:RelightingTritonClientApp>
)
