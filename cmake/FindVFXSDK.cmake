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

# Finds the NVVideoEffects library and its features
#
# And the following targets:
#   NVVideoEffects          - The main VFXSDK library
#   nvVFX<FeatureName>      - For every feature found in the lib directory

if(NOT TARGET NVVideoEffects)
  # Allow the user to specify where to find VFXSDK
  set(VFXSDK_ROOT "" CACHE PATH "Root directory of VFX SDK installation")
  get_filename_component(VFXSDK_ROOT "${VFXSDK_ROOT}" ABSOLUTE)

  # Find the dynamic library directory
  find_path(NVVideoEffects_LIBRARY_DIR
    NAMES NVVideoEffects.dll libVideoFX.so
    PATHS
    ${VFXSDK_ROOT}/bin # If explicitly set
    ${CMAKE_INSTALL_PREFIX}/bin # If installed
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin # If local
    ${VFXSDK_ROOT}/lib # Linux, if explicitly set
    ${CMAKE_INSTALL_PREFIX}/lib # Linux, if installed
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib # Linux, if local
    /usr/local/VideoFX/lib/ # Linux, if installed separately
    NO_DEFAULT_PATH
  )

  # Find the model directory
  find_path(NVVideoEffects_MODEL_DIR
    NAMES models
    PATHS
    ${VFXSDK_ROOT}/bin # If explicitly set
    ${CMAKE_INSTALL_PREFIX}/bin # If installed
    ${CMAKE_CURRENT_SOURCE_DIR}/../bin # If local
    ${VFXSDK_ROOT}/lib # Linux, if explicitly set
    ${CMAKE_INSTALL_PREFIX}/lib # Linux, if installed
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib # Linux, if local
    /usr/local/VideoFX/lib/ # Linux, if installed separately
    NO_DEFAULT_PATH
  )
  set(NVVideoEffects_MODEL_DIR ${NVVideoEffects_MODEL_DIR}/models)

  if(NOT ${VFXSDK_ROOT} AND DEFINED NVVideoEffects_LIBRARY_DIR AND EXISTS ${NVVideoEffects_LIBRARY_DIR})
    set(VFXSDK_ROOT "${NVVideoEffects_LIBRARY_DIR}/../")
    get_filename_component(VFXSDK_ROOT "${VFXSDK_ROOT}" ABSOLUTE)
    set(VFXSDK_ROOT "${VFXSDK_ROOT}" CACHE PATH "Root directory of VFX SDK installation" FORCE)
  endif()

  message(STATUS "VFXSDK_ROOT: ${VFXSDK_ROOT}")

  # Find the import library file for NVVideoEffects
  find_library(NVVideoEffects_IMPORT_LIBRARY
    NAMES VideoFX.lib libVideoFX.so
    PATHS
    ${VFXSDK_ROOT}/lib
    ${VFXSDK_ROOT}/bin
    NO_DEFAULT_PATH
  )

  # Find the import library file for NVCVImage
  find_library(NVCVImage_IMPORT_LIBRARY
    NAMES NVCVImage.lib libNVCVImage.so
    PATHS
    ${VFXSDK_ROOT}/lib
    ${VFXSDK_ROOT}/bin
    NO_DEFAULT_PATH
  )

  if(NOT NVVideoEffects_IMPORT_LIBRARY)
    add_library(NVVideoEffects STATIC
      ${VFXSDK_ROOT}/nvvfx/include/nvVideoEffects.h
      ${VFXSDK_ROOT}/nvvfx/include/nvCVStatus.h)
    target_include_directories(NVVideoEffects PUBLIC ${VFXSDK_ROOT}/nvvfx/include)
    target_sources(NVVideoEffects PRIVATE ${VFXSDK_ROOT}/nvvfx/src/nvVideoEffectsProxy.cpp)
    set(NVVideoEffects_IMPORT_LIBRARY NVVideoEffects)
  endif()

  if(NOT NVCVImage_IMPORT_LIBRARY)
    add_library(NVCVImage STATIC
      ${VFXSDK_ROOT}/nvvfx/src/nvCVImageProxy.cpp
      ${VFXSDK_ROOT}/nvvfx/include/nvCVImage.h
      ${VFXSDK_ROOT}/nvvfx/include/nvCVStatus.h)
    target_include_directories(NVCVImage PUBLIC ${VFXSDK_ROOT}/nvvfx/include)
    set(NVCVImage_IMPORT_LIBRARY NVCVImage)
  endif()

  # Find the include directory
  find_path(NVVideoEffects_INCLUDE_DIR
    NAMES nvVideoEffects.h
    PATHS
    ${VFXSDK_ROOT}/include
    ${VFXSDK_ROOT}/nvvfx/include
    NO_DEFAULT_PATH
  )

  # Set version from header if available
  if(NVVideoEffects_INCLUDE_DIR)
    # Find all version*.h files
    file(GLOB VERSION_FILES "${VFXSDK_ROOT}/videofx_version.h")
    
    # On Linux, also check in share directory if not found in root
    if(NOT VERSION_FILES AND UNIX)
      file(GLOB VERSION_FILES "${VFXSDK_ROOT}/share/videofx_version.h")
    endif()
    
    if(VERSION_FILES)
      list(GET VERSION_FILES 0 VERSION_FILE)
      file(READ "${VERSION_FILES}" VERSION_HEADER)

      string(REGEX MATCH "NVIDIA_VIDEOEFFECTS_SDK_VERSION_MAJOR[ \t]+([0-9]+)" _ "${VERSION_HEADER}")
      set(NVVideoEffects_VERSION_VERSION_MAJOR "${CMAKE_MATCH_1}")
      string(REGEX MATCH "NVIDIA_VIDEOEFFECTS_SDK_VERSION_MINOR[ \t]+([0-9]+)" _ "${VERSION_HEADER}")
      set(NVVideoEffects_VERSION_VERSION_MINOR "${CMAKE_MATCH_1}")
      string(REGEX MATCH "NVIDIA_VIDEOEFFECTS_SDK_VERSION_RELEASE[ \t]+([0-9]+)" _ "${VERSION_HEADER}")
      set(NVVideoEffects_VERSION_VERSION_RELEASE "${CMAKE_MATCH_1}")
      string(REGEX MATCH "NVIDIA_VIDEOEFFECTS_SDK_VERSION_BUILD[ \t]+([0-9]+)" _ "${VERSION_HEADER}")
      set(NVVideoEffects_VERSION_VERSION_BUILD "${CMAKE_MATCH_1}")
      set(NVVideoEffects_VERSION_STRING "${NVVideoEffects_VERSION_VERSION_MAJOR}.${NVVideoEffects_VERSION_VERSION_MINOR}.${NVVideoEffects_VERSION_VERSION_RELEASE}.${NVVideoEffects_VERSION_VERSION_BUILD}")
    endif()
  endif()

  if (NOT NVVideoEffects_VERSION_STRING)
    message(WARNING "Unable to deduce SDK version")
    set(NVVideoEffects_VERSION_STRING "")
  endif()

  # Create targets
  include(FindPackageHandleStandardArgs)
  find_package_handle_standard_args(VFXSDK
    FAIL_MESSAGE
    "VFXSDK not found. Please set VFXSDK_ROOT to the root directory of the VFX SDK installation."
    REQUIRED_VARS
    NVVideoEffects_IMPORT_LIBRARY
    NVCVImage_IMPORT_LIBRARY
    NVVideoEffects_INCLUDE_DIR
    VERSION_VAR NVVideoEffects_VERSION_STRING
  )

  if(VFXSDK_FOUND)
    # Main library target
    if(NOT TARGET NVVideoEffects)
      add_library(NVVideoEffects SHARED IMPORTED)
    endif()
    set_target_properties(NVVideoEffects PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${NVVideoEffects_INCLUDE_DIR}"
      IMPORTED_IMPLIB "${NVVideoEffects_IMPORT_LIBRARY}"
      IMPORTED_LOCATION "${NVVideoEffects_IMPORT_LIBRARY}"
      DYNAMIC_LIBRARY_DIR "${NVVideoEffects_LIBRARY_DIR}"
      MODEL_DIR "${NVVideoEffects_MODEL_DIR}"
    )

    # NvCVImage library target
    if(NOT TARGET NVCVImage)
      add_library(NVCVImage SHARED IMPORTED)
    endif()
    set_target_properties(NVCVImage PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${NVVideoEffects_INCLUDE_DIR}"
      IMPORTED_IMPLIB "${NVCVImage_IMPORT_LIBRARY}"
      IMPORTED_LOCATION "${NVCVImage_IMPORT_LIBRARY}"
      DYNAMIC_LIBRARY_DIR "${NVVideoEffects_LIBRARY_DIR}"
    )

    # Dynamically discover feature libraries
    file(GLOB DIR_LIST LIST_DIRECTORIES true "${VFXSDK_ROOT}/features/*")
    set(FEATURE_FOLDERS "")
    foreach(DIR_OBJ ${DIR_LIST})
      if(IS_DIRECTORY ${DIR_OBJ})
        list(APPEND FEATURE_FOLDERS ${DIR_OBJ})
      endif()
    endforeach()
    foreach(FEATURE_FOLDER ${FEATURE_FOLDERS})
      file(GLOB HEADERS "${FEATURE_FOLDER}/include/*.h")
      foreach(HEADER ${HEADERS})
        get_filename_component(HEADER_NAME ${HEADER} NAME_WE)
        get_filename_component(FEATURE_FOLDER_NAME ${FEATURE_FOLDER} NAME)
        string(TOLOWER "${HEADER_NAME}" HEADER_NAME_LOWER)
        if("${HEADER_NAME_LOWER}" STREQUAL "${FEATURE_FOLDER_NAME}")
          set(FEATURE_NAME ${HEADER_NAME})
        endif()
      endforeach()
      if(NOT FEATURE_NAME)
        message(FATAL_ERROR "Invalid feature. Header not matching feature name: ${FEATURE_FOLDER}")
      endif()
      # Create interface library for the feature
      add_library(${FEATURE_NAME} INTERFACE)

      # Read the version string from the header file and set the version property
      file(READ "${FEATURE_FOLDER}/include/${FEATURE_NAME}.h" FEATURE_HEADER)
      string(TOUPPER "${FEATURE_NAME}" PROJECT_NAME_UPPER)
      string(REGEX MATCH "#define[ \t]+${PROJECT_NAME_UPPER}_VERSION[ \t]+\"([^\"]+)\"" VERSION_MATCH "${FEATURE_HEADER}")
      set(FEATURE_VERSION_STRING "${CMAKE_MATCH_1}")
      set_target_properties(${FEATURE_NAME} PROPERTIES
        INTERFACE_VERSION ${FEATURE_VERSION_STRING}
      )
      
      target_include_directories(${FEATURE_NAME} INTERFACE ${FEATURE_FOLDER}/include)
      if(UNIX)
        set_target_properties(${FEATURE_NAME} PROPERTIES INTERFACE_DYNAMIC_LIBRARY_DIRECTORY ${FEATURE_FOLDER}/lib)
      else()
        set_target_properties(${FEATURE_NAME} PROPERTIES INTERFACE_DYNAMIC_LIBRARY_DIRECTORY ${FEATURE_FOLDER}/bin)
      endif()
    endforeach()
  endif()
endif()
