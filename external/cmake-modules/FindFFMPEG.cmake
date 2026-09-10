# Minimal FFMPEG module for OpenCV's modules/videoio/cmake/detect_ffmpeg.cmake.
# Enable with -DOPENCV_FFMPEG_USE_FIND_PACKAGE=FFMPEG and point -DFFMPEG_ROOT at
# the unpacked FFmpeg package. Replaces the pkg-config code path.

set(_ffmpeg_components avcodec avformat avutil swscale)

find_path(FFMPEG_INCLUDE_DIR
  NAMES libavcodec/avcodec.h
  HINTS "${FFMPEG_ROOT}/include"
  NO_DEFAULT_PATH)

set(FFMPEG_LIBRARIES "")
set(FFMPEG_FOUND TRUE)

if(NOT FFMPEG_INCLUDE_DIR)
  set(FFMPEG_FOUND FALSE)
endif()

foreach(_c ${_ffmpeg_components})
  find_library(FFMPEG_${_c}_LIBRARY
    NAMES ${_c}
    HINTS "${FFMPEG_ROOT}/lib"
    NO_DEFAULT_PATH)

  if(NOT FFMPEG_${_c}_LIBRARY OR NOT FFMPEG_INCLUDE_DIR)
    set(FFMPEG_FOUND FALSE)
    continue()
  endif()
  list(APPEND FFMPEG_LIBRARIES "${FFMPEG_${_c}_LIBRARY}")

  # detect_ffmpeg.cmake VERSION_LESS-compares these against minimums. An
  # undefined variable compares as 0, which silently disables FFmpeg, so every
  # component version has to be populated here.
  string(TOUPPER "${_c}" _u)
  file(GLOB _hdrs "${FFMPEG_INCLUDE_DIR}/lib${_c}/version*.h")  # 5.0+ split out version_major.h
  set(_ver "")
  foreach(_part MAJOR MINOR MICRO)
    set(_v 0)
    foreach(_h ${_hdrs})
      file(STRINGS "${_h}" _line REGEX "^#define +LIB${_u}_VERSION_${_part} +[0-9]+")
      if(_line)
        string(REGEX MATCH "[0-9]+" _v "${_line}")
      endif()
    endforeach()
    list(APPEND _ver "${_v}")
  endforeach()
  string(REPLACE ";" "." _ver "${_ver}")
  set(FFMPEG_lib${_c}_VERSION "${_ver}")
endforeach()

set(FFMPEG_INCLUDE_DIRS "${FFMPEG_INCLUDE_DIR}")
