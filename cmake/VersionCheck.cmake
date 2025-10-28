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

# Compares two version strings to determine compatibility using semantic versioning rules.
# - Major and minor versions must match exactly
# - Release version must be greater than or equal to the required version
# - Build version is ignored in the comparison
# 
# Parameters:
#   FEATURE_VERSION_STRING_REQUIRED (string) - Required version string in format "major.minor.release.build"
#   FEATURE_VERSION_STRING (string)          - Actual version string to check in format "major.minor.release.build" 
#   RESULT_VAR (variable name)               - Output variable name to store the comparison result
# 
# Output:
#   Sets ${RESULT_VAR} to TRUE if:
#     - Major versions match exactly, and
#     - Minor versions match exactly, and
#     - Release version is greater than or equal to required release version
#   Sets ${RESULT_VAR} to FALSE if:
#     - Major versions do not match, or
#     - Minor versions do not match, or
#     - Release version is less than required release version
function(check_feature_version FEATURE_VERSION_STRING_REQUIRED FEATURE_VERSION_STRING RESULT_VAR)
  string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${FEATURE_VERSION_STRING}")
  set(FEATURE_VERSION_MAJOR "${CMAKE_MATCH_1}")
  set(FEATURE_VERSION_MINOR "${CMAKE_MATCH_2}")
  set(FEATURE_VERSION_RELEASE "${CMAKE_MATCH_3}")
  set(FEATURE_VERSION_BUILD "${CMAKE_MATCH_4}")
  string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)\\.([0-9]+)" _ "${FEATURE_VERSION_STRING_REQUIRED}")
  set(FEATURE_VERSION_MAJOR_REQUIRED "${CMAKE_MATCH_1}")
  set(FEATURE_VERSION_MINOR_REQUIRED "${CMAKE_MATCH_2}")
  set(FEATURE_VERSION_RELEASE_REQUIRED "${CMAKE_MATCH_3}")
  set(FEATURE_VERSION_BUILD_REQUIRED "${CMAKE_MATCH_4}")
  if (FEATURE_VERSION_MAJOR STREQUAL FEATURE_VERSION_MAJOR_REQUIRED AND
      FEATURE_VERSION_MINOR STREQUAL FEATURE_VERSION_MINOR_REQUIRED AND
      FEATURE_VERSION_RELEASE GREATER_EQUAL FEATURE_VERSION_RELEASE_REQUIRED)
    set(${RESULT_VAR} TRUE PARENT_SCOPE)
  else()
    set(${RESULT_VAR} FALSE PARENT_SCOPE)
  endif()
endfunction()

# Verifies that a required feature target exists and meets the minimum version requirement.
# - Prints WARNING messages if verification fails
# - Messages include details about missing features or version mismatches
# 
# Parameters:
#   REQUIRED_FEATURE_NAME_AND_VERSION (string) - Name and version of the CMake target representing the required feature.
#                                                Of the form "<feature_name>:<version>"
#   RESULT_VAR (variable name)                 - Output variable name to store the verification result
# 
# Output:
#   Sets ${RESULT_VAR} to TRUE if:
#     - The feature target exists
#     - The feature version meets or exceeds the required version
#   Sets ${RESULT_VAR} to FALSE if:
#     - The feature target does not exist
#     - The feature version is below the required minimum
function(verify_app_feature_dependency REQUIRED_FEATURE_AND_VERSION RESULT_VAR)  
  string(REGEX MATCH "^([^:]+):([^:]+)" _ "${REQUIRED_FEATURE_AND_VERSION}")
  set(REQUIRED_FEATURE_NAME ${CMAKE_MATCH_1})
  set(REQUIRED_FEATURE_VERSION_STRING ${CMAKE_MATCH_2})
  if (NOT TARGET ${REQUIRED_FEATURE_NAME})
    message(WARNING "Required feature ${REQUIRED_FEATURE_NAME} is not available.")
    set(${RESULT_VAR} FALSE PARENT_SCOPE)
    return()
  endif()
  get_target_property(FEATURE_VERSION_STRING ${REQUIRED_FEATURE_NAME} INTERFACE_VERSION)
  check_feature_version(${REQUIRED_FEATURE_VERSION_STRING} ${FEATURE_VERSION_STRING} VERSION_CHECK_RESULT)
  if (NOT VERSION_CHECK_RESULT)
    message(WARNING "REQUIRED FEATURE: ${REQUIRED_FEATURE_NAME} VERSION: ${REQUIRED_FEATURE_VERSION_STRING} is not available.")
    set(${RESULT_VAR} FALSE PARENT_SCOPE)
    return()
  endif()
  set(${RESULT_VAR} TRUE PARENT_SCOPE)
endfunction()
