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

if(WIN32)
  set(RUN_FILES run_batcheffectapp.bat)
  set(RUN_FILES_DENOISE run_batchdenoiseeffectapp.bat)
  set(RUN_FILES_AIGS run_batchaigseffectapp.bat)
else()
  set(RUN_FILES
    run_batcheffectapp.sh
    setup_env.sh)
  set(RUN_FILES_DENOISE run_batchdenoiseeffectapp.sh)
  set(RUN_FILES_AIGS run_batchaigseffectapp.sh)
endif()

##################
# BatchEffectApp #
##################

set(REQUIRED_FEATURES nvVFXArtifactReduction:1.0.0.0 nvVFXSuperRes:1.0.0.0 nvVFXUpscale:1.0.0.0 nvVFXTransfer:1.0.0.0)
foreach(REQUIRED_FEATURE_AND_VERSION IN LISTS REQUIRED_FEATURES)
  verify_app_feature_dependency(${REQUIRED_FEATURE_AND_VERSION} RESULT)
  if(NOT RESULT)
    message(${SAMPLES_DEPS_ERROR_LEVEL} "Skipping BatchEffectApp.")
    return()
  endif()
endforeach()

set(BATCHEFFECTAPP_SRCS
  BatchEffectApp.cpp
  ${VFXSDKSampleApps_UTILS_DIR}/batchUtilities.cpp
  ${VFXSDKSampleApps_UTILS_DIR}/batchUtilities.h)

add_executable(BatchEffectApp README.md ${BATCHEFFECTAPP_SRCS})

if(TARGET OpenCV)
  set(OPENCV OpenCV)
else()
  set(OPENCV opencv346)
endif()
target_include_directories(BatchEffectApp PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${VFXSDKSampleApps_UTILS_DIR})

target_link_libraries(BatchEffectApp PRIVATE
  ${OPENCV}
  NVVideoEffects
  NVCVImage
)

get_target_property(NVVFX_DYNAMIC_LIBRARY_DIR NVVideoEffects DYNAMIC_LIBRARY_DIR)
set(NVVFX_DYNAMIC_LIBRARY_DIRS ${NVVFX_DYNAMIC_LIBRARY_DIR})

foreach(REQUIRED_FEATURE_AND_VERSION IN LISTS REQUIRED_FEATURES)
  string(REGEX MATCH "^([^:]+):([^:]+)" _ "${REQUIRED_FEATURE_AND_VERSION}")
  set(REQUIRED_FEATURE_NAME ${CMAKE_MATCH_1})
  target_link_libraries(BatchEffectApp PRIVATE ${REQUIRED_FEATURE_NAME})
  get_target_property(FEATURE_DYNAMIC_LIBRARY_DIR ${REQUIRED_FEATURE_NAME} INTERFACE_DYNAMIC_LIBRARY_DIRECTORY)
  if(FEATURE_DYNAMIC_LIBRARY_DIR)
    list(APPEND NVVFX_DYNAMIC_LIBRARY_DIRS ${FEATURE_DYNAMIC_LIBRARY_DIR})
  endif()
endforeach()

get_target_property(NVVFX_MODEL_DIR NVVideoEffects MODEL_DIR)

if(MSVC)
  get_target_property(OPENCV_PATH_STR ${OPENCV} DYNAMIC_LIBRARY_DIR)
  set(PATH_STR "PATH=%PATH%" ${OPENCV_PATH_STR};${NVVFX_DYNAMIC_LIBRARY_DIRS})
  set(CMD_ARG_STR "--model_dir=\"${NVVFX_MODEL_DIR}\"")
  set_target_properties(BatchEffectApp PROPERTIES
    FOLDER SampleApps
    VS_DEBUGGER_ENVIRONMENT "${PATH_STR}"
    VS_DEBUGGER_COMMAND_ARGUMENTS "${CMD_ARG_STR} --webcam --show"
  )
  # https://stackoverflow.com/questions/28178978/how-to-generate-pdb-files-for-release-build-with-cmake-flags
  target_compile_options(BatchEffectApp PRIVATE $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:/Zi>) # Keep flag specific to C++ to avoid Cuda build errors
  target_link_options(BatchEffectApp PRIVATE $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:/INCREMENTAL:NO;/DEBUG;/OPT:REF;/OPT:ICF>)
endif(MSVC)

# Configure run-files
foreach(RUN_FILE IN LISTS RUN_FILES)
  # Note: Separate configure_file from file copy for older CMake version to support file permissions
  configure_file(${RUN_FILE}.in ${CMAKE_CURRENT_BINARY_DIR}/tmp/${RUN_FILE} @ONLY)
  file(COPY ${CMAKE_CURRENT_BINARY_DIR}/tmp/${RUN_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_files FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE)
  file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/tmp/)
  add_custom_command(TARGET BatchEffectApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/run_files/${RUN_FILE}
    $<TARGET_FILE_DIR:BatchEffectApp>)
endforeach()

#########################
# BatchDenoiseEffectApp #
#########################

set(REQUIRED_FEATURES nvVFXDenoising:1.0.0.0)
foreach(REQUIRED_FEATURE_AND_VERSION IN LISTS REQUIRED_FEATURES)
  verify_app_feature_dependency(${REQUIRED_FEATURE_AND_VERSION} RESULT)
  if(NOT RESULT)
    message(${SAMPLES_DEPS_ERROR_LEVEL} "Skipping BatchDenoiseEffectApp.")
    return()
  endif()
endforeach()

set(BATCHDENOISEEFFECTAPP_SRCS
  BatchDenoiseEffectApp.cpp
  ${VFXSDKSampleApps_UTILS_DIR}/batchUtilities.cpp
  ${VFXSDKSampleApps_UTILS_DIR}/batchUtilities.h)

add_executable(BatchDenoiseEffectApp README.md ${BATCHDENOISEEFFECTAPP_SRCS})

target_include_directories(BatchDenoiseEffectApp PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${VFXSDKSampleApps_UTILS_DIR}
)
target_link_libraries(BatchDenoiseEffectApp PRIVATE
  ${OPENCV}
  NVVideoEffects
  NVCVImage
)

set(NVVFX_DYNAMIC_LIBRARY_DIRS ${NVVFX_DYNAMIC_LIBRARY_DIR})

foreach(REQUIRED_FEATURE_AND_VERSION IN LISTS REQUIRED_FEATURES)
  string(REGEX MATCH "^([^:]+):([^:]+)" _ "${REQUIRED_FEATURE_AND_VERSION}")
  set(REQUIRED_FEATURE_NAME ${CMAKE_MATCH_1})
  target_link_libraries(BatchDenoiseEffectApp PRIVATE ${REQUIRED_FEATURE_NAME})
  get_target_property(FEATURE_DYNAMIC_LIBRARY_DIR ${REQUIRED_FEATURE_NAME} INTERFACE_DYNAMIC_LIBRARY_DIRECTORY)
  if(FEATURE_DYNAMIC_LIBRARY_DIR)
    list(APPEND NVVFX_DYNAMIC_LIBRARY_DIRS ${FEATURE_DYNAMIC_LIBRARY_DIR})
  endif()
endforeach()

if(MSVC)
  get_target_property(OPENCV_PATH_STR ${OPENCV} DYNAMIC_LIBRARY_DIR)
  set(PATH_STR "PATH=%PATH%" ${OPENCV_PATH_STR};${NVVFX_DYNAMIC_LIBRARY_DIRS})
  set(CMD_ARG_STR "--model_dir=\"${NVVFX_MODEL_DIR}\"")
  set_target_properties(BatchDenoiseEffectApp PROPERTIES
    FOLDER SampleApps
    VS_DEBUGGER_ENVIRONMENT "${PATH_STR}"
    VS_DEBUGGER_COMMAND_ARGUMENTS "${CMD_ARG_STR}"
  )
  # https://stackoverflow.com/questions/28178978/how-to-generate-pdb-files-for-release-build-with-cmake-flags
  target_compile_options(BatchDenoiseEffectApp PRIVATE $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:/Zi>) # Keep flag specific to C++ to avoid Cuda build errors
  target_link_options(BatchDenoiseEffectApp PRIVATE $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:/INCREMENTAL:NO;/DEBUG;/OPT:REF;/OPT:ICF>)
endif(MSVC)

# Configure run-files
foreach(RUN_FILE IN LISTS RUN_FILES_DENOISE)
  # Note: Separate configure_file from file copy for older CMake version to support file permissions
  configure_file(${RUN_FILE}.in ${CMAKE_CURRENT_BINARY_DIR}/tmp/${RUN_FILE} @ONLY)
  file(COPY ${CMAKE_CURRENT_BINARY_DIR}/tmp/${RUN_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_files FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE)
  file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/tmp/)
  add_custom_command(TARGET BatchDenoiseEffectApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/run_files/${RUN_FILE}
    $<TARGET_FILE_DIR:BatchDenoiseEffectApp>)
endforeach()

######################
# BatchAigsEffectApp #
######################

set(REQUIRED_FEATURES nvVFXGreenScreen:1.0.0.0)
foreach(REQUIRED_FEATURE_AND_VERSION IN LISTS REQUIRED_FEATURES)
  verify_app_feature_dependency(${REQUIRED_FEATURE_AND_VERSION} RESULT)
  if(NOT RESULT)
    message(${SAMPLES_DEPS_ERROR_LEVEL} "Skipping BatchAigsEffectApp.")
    return()
  endif()
endforeach()

set(BATCHAIGSEFFECTAPP_SRCS
  BatchAigsEffectApp.cpp
  ${VFXSDKSampleApps_UTILS_DIR}/batchUtilities.cpp
  ${VFXSDKSampleApps_UTILS_DIR}/batchUtilities.h)

add_executable(BatchAigsEffectApp README.md ${BATCHAIGSEFFECTAPP_SRCS})

target_include_directories(BatchAigsEffectApp PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${VFXSDKSampleApps_UTILS_DIR}
)
target_link_libraries(BatchAigsEffectApp PRIVATE
  ${OPENCV}
  NVVideoEffects
  NVCVImage
)

set(NVVFX_DYNAMIC_LIBRARY_DIRS ${NVVFX_DYNAMIC_LIBRARY_DIR})

foreach(REQUIRED_FEATURE_AND_VERSION IN LISTS REQUIRED_FEATURES)
  string(REGEX MATCH "^([^:]+):([^:]+)" _ "${REQUIRED_FEATURE_AND_VERSION}")
  set(REQUIRED_FEATURE_NAME ${CMAKE_MATCH_1})
  target_link_libraries(BatchAigsEffectApp PRIVATE ${REQUIRED_FEATURE_NAME})
  get_target_property(FEATURE_DYNAMIC_LIBRARY_DIR ${REQUIRED_FEATURE_NAME} INTERFACE_DYNAMIC_LIBRARY_DIRECTORY)
  if(FEATURE_DYNAMIC_LIBRARY_DIR)
    list(APPEND NVVFX_DYNAMIC_LIBRARY_DIRS ${FEATURE_DYNAMIC_LIBRARY_DIR})
  endif()
endforeach()

if(MSVC)
  get_target_property(OPENCV_PATH_STR ${OPENCV} DYNAMIC_LIBRARY_DIR)
  set(PATH_STR "PATH=%PATH%" ${OPENCV_PATH_STR};${NVVFX_DYNAMIC_LIBRARY_DIRS})
  set(CMD_ARG_STR "--model_dir=\"${NVVFX_MODEL_DIR}\"")
  set_target_properties(BatchAigsEffectApp PROPERTIES
    FOLDER SampleApps
    VS_DEBUGGER_ENVIRONMENT "${PATH_STR}"
    VS_DEBUGGER_COMMAND_ARGUMENTS "${CMD_ARG_STR}"
  )
  # https://stackoverflow.com/questions/28178978/how-to-generate-pdb-files-for-release-build-with-cmake-flags
  target_compile_options(BatchAigsEffectApp PRIVATE $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:/Zi>) # Keep flag specific to C++ to avoid Cuda build errors
  target_link_options(BatchAigsEffectApp PRIVATE $<$<AND:$<CONFIG:Release>,$<COMPILE_LANGUAGE:CXX>>:/INCREMENTAL:NO;/DEBUG;/OPT:REF;/OPT:ICF>)
endif(MSVC)

# Configure run-files
foreach(RUN_FILE IN LISTS RUN_FILES_AIGS)
  # Note: Separate configure_file from file copy for older CMake version to support file permissions
  configure_file(${RUN_FILE}.in ${CMAKE_CURRENT_BINARY_DIR}/tmp/${RUN_FILE} @ONLY)
  file(COPY ${CMAKE_CURRENT_BINARY_DIR}/tmp/${RUN_FILE} DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/run_files FILE_PERMISSIONS OWNER_EXECUTE OWNER_READ OWNER_WRITE)
  file(REMOVE_RECURSE ${CMAKE_CURRENT_BINARY_DIR}/tmp/)
  add_custom_command(TARGET BatchAigsEffectApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    ${CMAKE_CURRENT_BINARY_DIR}/run_files/${RUN_FILE}
    $<TARGET_FILE_DIR:BatchAigsEffectApp>)
endforeach()

# Add resources and README.md file
add_custom_command(TARGET BatchEffectApp POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
  ${CMAKE_CURRENT_SOURCE_DIR}/README.md
  ${VFXSDKSampleApps_RESOURCES_DIR}/input_0_100_frames.mp4
  ${VFXSDKSampleApps_RESOURCES_DIR}/input_100_250_frames.mp4
  ${VFXSDKSampleApps_RESOURCES_DIR}/LeFret_000900.jpg
  ${VFXSDKSampleApps_RESOURCES_DIR}/LeFret_001400.jpg
  ${VFXSDKSampleApps_RESOURCES_DIR}/LeFret_003400.jpg
  ${VFXSDKSampleApps_RESOURCES_DIR}/LeFret_012300.jpg
  $<TARGET_FILE_DIR:BatchEffectApp>
)
