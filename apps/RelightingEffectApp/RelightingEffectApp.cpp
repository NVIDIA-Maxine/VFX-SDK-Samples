/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <string>

#include "nvCVOpenCV.h"
#include "nvVFXBackgroundBlur.h"
#include "nvVFXGreenScreen.h"
#include "nvVFXRelighting.h"
#include "nvVideoEffects.h"
#include "opencv2/opencv.hpp"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#endif  // _MSC_VER

#define BAIL_IF_ERR(err) \
  do {                   \
    if (0 != (err)) {    \
      goto bail;         \
    }                    \
  } while (0)
#define BAIL_IF_NULL(x, err, code) \
  do {                             \
    if ((void*)(x) == NULL) {      \
      err = code;                  \
      goto bail;                   \
    }                              \
  } while (0)

#define NVCV_ERR_HELP 411
#define DEFAULT_CODEC "avc1"
#define ESC 0x1b

#define SHOW_OUTPUT 0x0
#define SHOW_INPUT 0x1

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif  // M_PI
#ifndef M_2PI
#define M_2PI 6.28318530717958647693
#endif  // M_2PI
#define F_PI static_cast<float>(M_PI)
#define F_2PI static_cast<float>(M_2PI)
#define F_RADIANS_PER_DEGREE static_cast<float>(M_PI / 180.)
#define F_DEGREES_PER_RADIAN static_cast<float>(180. / M_PI)

#define BGMODE_SRC 0
#define BGMODE_SRC_BLURRED 1
#define BGMODE_HDR 2
#define BGMODE_BG 3
#define BGMODE_BG_BLURRED 4

bool FLAG_debug = false, FLAG_verbose = false, FLAG_show = false, FLAG_webcam = false, FLAG_autorotate = false;
int FLAG_logLevel = NVCV_LOG_ERROR,
    FLAG_bgMode = BGMODE_SRC,
    FLAG_aigs_mode = 2;        // 0=FG Qual Model 1=FG Perf Model 2=BG Qual Model 3=BG Perf Model\n"
float FLAG_pan = -90.f,        // Degrees in the command-line arguments, radians in the SDK
    FLAG_rotationRate = 20.f,  // degrees per second.
      FLAG_vfov = 60.f;  // Degrees in the command-line arguments, radians in the SDK
std::string FLAG_codec = DEFAULT_CODEC, FLAG_inFile, FLAG_outFile, FLAG_outDir, FLAG_modelsDir,
            FLAG_showMode = "output", FLAG_camRes, FLAG_inBg, FLAG_inHDR, FLAG_inMat, FLAG_log = "stderr";

// Set this when using OTA Updates
// This path is used by nvVideoEffectsProxy.cpp to load the SDK dll
// when using  OTA Updates
char* g_nvVFXSDKPath = NULL;

static bool GetFlagArgVal(const char* flag, const char* arg, const char** val) {  // C-string
  if (*arg != '-') return false;
  while (*++arg == '-') continue;
  const char* s = strchr(arg, '=');
  if (s == NULL) {
    if (strcmp(flag, arg) != 0) return false;
    *val = NULL;
    return true;
  }
  size_t n = s - arg;
  if ((strlen(flag) != n) || (strncmp(flag, arg, n) != 0)) return false;
  *val = s + 1;
  return true;
}

static bool GetFlagArgVal(const char* flag, const char* arg, std::string* val) {  // std:: string
  const char* valStr;
  if (!GetFlagArgVal(flag, arg, &valStr)) return false;
  val->assign(valStr ? valStr : "");
  return true;
}

static bool GetFlagArgVal(const char* flag, const char* arg, bool* val) {  // bool
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) {
    *val = (valStr == NULL || strcasecmp(valStr, "true") == 0 || strcasecmp(valStr, "on") == 0 ||
            strcasecmp(valStr, "yes") == 0 || strcasecmp(valStr, "1") == 0);
  }
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, float* val) {  // float
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) *val = strtof(valStr, NULL);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, long* val) {  // long
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) *val = strtol(valStr, NULL, 10);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, int* val) {  // int
  long longVal;
  bool success = GetFlagArgVal(flag, arg, &longVal);
  if (success) *val = (int)longVal;
  return success;
}

static void Usage() {
  printf(
      "RelightingEffectApp [args ...]\n"
      "  where args is:\n"
      "  --autorotate[=(true|false)] automatically rotate the environment\n"
      "  --bg_mode=<n>               0=src 1=srcBlur 2=HDR 3=bgImg 4=bgImgBlur\n"
      "  --cam_res=[WWWx]HHH         specify resolution as height or width x height\n"
      "  --codec=<fourcc>            the fourcc code for the desired codec (default " DEFAULT_CODEC
      ")\n"
      "  --debug[=(true|false)]      print extra debugging information\n"
      "  --help[=(true|false)]       print this help message\n"
      "  --in_bg=(file|color)        use the specified file (png or jpg) or color (gray or 0xRRGGBB) for the "
      "background\n"
      "  --in_file=<file>            specify input source file (image or video)\n"
      "  --in_hdr=<file>             specify input HDR file (hdr or exr) or directory, for illumination.\n"
      "  --in_mat=<file>             specify input matte file. Only supported when processing images.\n"
      "                              If not specified, or when processing videos/webcam, AIGS is run\n"
      "  --log=<file>                log SDK errors to a file, \"stderr\" or \"\" (default stderr)\n"
      "  --log_level=<N>             the desired log level: {0, 1, 2, 3} = {FATAL, ERROR, WARNING, INFO}, respectively "
      "(default 1)\n"
      "  --model_dir=<path>          the path to the directory that contains the .trtmodel files\n"
      "  --out_dir=<dir>             set the output directory. Must use in conjunction with --out_file to create an output file \n"
      "  --out_file=<file>           specify an output video file\n"
      "  --pan=<num>                 set the initial pan angle, in degrees (default -90)\n"
      "  --rotation_rate=<N>         the auto-rotation rate, in degrees per second\n"
      "  --show[=(true|false)]       display images on-screen\n"
      "  --show_mode=<mode>          Options - output, input\n"
      "  --verbose[=(true|false)]    verbose output\n"
      "  --vfov=<num>                set the initial vertical field of view, in degrees (default 60)\n"
      "  --webcam[=(true|false)]     use a webcam as the input, rather than a file\n");
}

static void PrintKeyboardControlLegend() {
  printf(
      "\nKeyboard Control Legend\n"
      "  ESC or q   quit\n"
      "  , (comma)  adjust pan  by  -1 degree\n"
      "  . (period) adjust pan  by  +1 degree\n"
      "  <          adjust pan  by -10 degrees\n"
      "  >          adjust pan  by +10 degrees\n"
      "  v (lower)  adjust vfov by -10 degrees\n"
      "  V (upper)  adjust vfov by +10 degrees\n"
      "  r          auto-rotate\n"
      "  p or space pause video\n"
      "  f          toggle between showing and not showing the frame rate\n"
      "  i          toggle between showing output and showing input\n"
      "  n          advance to the next HDR for illumination\n"
      "  b          cycle through background mode\n"
      "  z          reset to studio lighting\n"
      "  h          print this help message\n");
}

static int ParseMyArgs(int argc, char** argv) {
  int errs = 0;
  for (--argc, ++argv; argc--; ++argv) {
    bool help;
    const char* arg = *argv;
    if (arg[0] != '-') {
      continue;
    } else if ((arg[1] == '-') &&                                              //
               (                                                               //
                   GetFlagArgVal("autorotate", arg, &FLAG_autorotate) ||       //
                   GetFlagArgVal("bg_mode", arg, &FLAG_bgMode) ||              //
                   GetFlagArgVal("cam_res", arg, &FLAG_camRes) ||              //
                   GetFlagArgVal("codec", arg, &FLAG_codec) ||                 //
                   GetFlagArgVal("debug", arg, &FLAG_debug) ||                 //
                   GetFlagArgVal("in_bg", arg, &FLAG_inBg) ||                  //
                   GetFlagArgVal("in_file", arg, &FLAG_inFile) ||              //
                   GetFlagArgVal("in_hdr", arg, &FLAG_inHDR) ||                //
                   GetFlagArgVal("in_mask", arg, &FLAG_inMat) ||               //
                   GetFlagArgVal("in_mat", arg, &FLAG_inMat) ||                //
                   GetFlagArgVal("in_src", arg, &FLAG_inFile) ||               //
                   GetFlagArgVal("log", arg, &FLAG_log) ||                     //
                   GetFlagArgVal("log_level", arg, &FLAG_logLevel) ||          //
                   GetFlagArgVal("model_dir", arg, &FLAG_modelsDir) ||         //
                   GetFlagArgVal("models_dir", arg, &FLAG_modelsDir) ||        //
                   GetFlagArgVal("out_dir", arg, &FLAG_outDir) ||              //
                   GetFlagArgVal("out_file", arg, &FLAG_outFile) ||            //
                   GetFlagArgVal("pan", arg, &FLAG_pan) ||                     //
                   GetFlagArgVal("rotation_rate", arg, &FLAG_rotationRate) ||  //
                   GetFlagArgVal("show", arg, &FLAG_show) ||                   //
                   GetFlagArgVal("show_mode", arg, &FLAG_showMode) ||          //
                   GetFlagArgVal("verbose", arg, &FLAG_verbose) ||             //
                   GetFlagArgVal("vfov", arg, &FLAG_vfov) ||                   //
                   GetFlagArgVal("webcam", arg, &FLAG_webcam))) {
      continue;
    } else if (GetFlagArgVal("help", arg, &help)) {
      Usage();
      return NVCV_ERR_HELP;
    } else if (arg[1] != '-') {
      for (++arg; *arg; ++arg) {
        if (*arg == 'v') {
          FLAG_verbose = true;
        } else {
          printf("Unknown flag ignored: \"-%c\"\n", *arg);
        }
      }
      continue;
    } else {
      printf("Unknown flag ignored: \"%s\"\n", arg);
    }
  }
  return errs;
}

/// Determine whether the given string has the specified suffix.
/// @param[in] str  the string to be queried.
/// @param[in] str  the suffix to be tested against.
/// @return true if the file matches the suffix, false otherwise.
static bool HasSuffix(const char* str, const char* suf) {
  size_t strSize = strlen(str), sufSize = strlen(suf);
  if (strSize < sufSize) return false;
  return (0 == strcasecmp(suf, str + strSize - sufSize));
}

/// Determine whether the given string has any of the specified suffixes.
/// @param[in] str  the string to be queried, followed by a list of suffixes, terminated by a NULL pointer.
/// @return true if the file matches any of the suffixes, false otherwise.
static bool HasOneOfTheseSuffixes(const char* str, ...) {
  bool matches = false;
  const char* suf;
  va_list ap;
  va_start(ap, str);
  while (nullptr != (suf = va_arg(ap, const char*))) {
    if (HasSuffix(str, suf)) {
      matches = true;
      break;
    }
  }
  va_end(ap);
  return matches;
}

/// Determine whether a given file contains an image, by looking at the file's extension.
/// @param[in]  str the file name.
/// @return     true if the file contains an image, false otherwise.
static bool IsImageFile(const char* str) {
  return HasOneOfTheseSuffixes(str, ".bmp", ".jpg", ".jpeg", ".png", nullptr);
}

/// Return a human-meaningful string that describes the duration of the input stream.
/// @param[in]  sc  the duration of the input stream, in seconds.
/// @return     the descriptive string.
static const char* DurationString(double sc) {
  static char buf[16];
  int hr, mn;
  hr = (int)(sc / 3600.);
  sc -= hr * 3600.;
  mn = (int)(sc / 60.);
  sc -= mn * 60.;
  snprintf(buf, sizeof(buf), "%02d:%02d:%06.3f", hr, mn, sc);
  return buf;
}

/// Information about a video stream.
struct VideoInfo {
  int codec;             ///< The FOURCC code that describes the input codec.
  int width;             ///< The width of the frames in the video stream.
  int height;            ///< The height of the frames in the video stream.
  double frameRate;      ///< The frame rate of the video stream, in frames per second.
  long long frameCount;  ///< The number of frames in the video stream. Undefined for a webcam input stream.
};

/// Get information about the input video stream.
/// @param[in]  reader    the input video stream reader to be queried.
/// @param[in]  fileName  the file name associated with the video stream.
/// @param[out] info      the requested information about the video stream.
static void GetVideoInfo(cv::VideoCapture& reader, const char* fileName, VideoInfo* info) {
  info->codec = (int)reader.get(cv::CAP_PROP_FOURCC);
  info->width = (int)reader.get(cv::CAP_PROP_FRAME_WIDTH);
  info->height = (int)reader.get(cv::CAP_PROP_FRAME_HEIGHT);
  info->frameRate = (double)reader.get(cv::CAP_PROP_FPS);
  info->frameCount = (long long)reader.get(cv::CAP_PROP_FRAME_COUNT);
  if (FLAG_verbose)
    printf(
        "       file \"%s\"\n"
        "      codec %.4s\n"
        "      width %4d\n"
        "     height %4d\n"
        " frame rate %.3f\n"
        "frame count %4lld\n"
        "   duration %s\n",
        fileName, (char*)&info->codec, info->width, info->height, info->frameRate, info->frameCount,
        DurationString(info->frameCount / info->frameRate));
}

/// Convert a string to a FOURRCC code.
/// @param[in] str the string to be parsed.
/// @return the resultant FOURCC code.
static int StringToFourcc(const std::string& str) {
  union chint {
    int i;
    char c[4];
  };
  chint x = {0};
  for (int n = (str.size() < 4) ? (int)str.size() : 4; n--;) x.c[n] = str[n];
  return x.i;
}

/// Read any image that OpenCV can read, including HDR and EXR.
/// @param[in]   file      the file to read.
/// @param[in]   fmt       the desired pixel format.
/// @param[in]   typ       the desired component type.
/// @param[in]   layout    the desired layout.
/// @param[in]   memspace  the desired memory space.
/// @param[in]   align     the desired alignment.
/// @param[out]  im        the image to reallocate and fill with the specified image.
/// @return  NVCV_SUCCES   if successful,
///          NVCV_ERR_READ if there were problems reading the image.
/// @note    sometimes OpenCV has problems with certain types of paths.
static NvCV_Status ReadImage(const char* file, NvCVImage_PixelFormat fmt, NvCVImage_ComponentType typ, unsigned layout,
                             unsigned memspace, unsigned align, NvCVImage* im) {
  NvCV_Status err;
  NvCVImage nvc;
  cv::Mat ocv;
  int flags = 0;
  if (fmt > NVCV_YA) flags |= cv::IMREAD_COLOR;
  if (typ != NVCV_U8) flags |= cv::IMREAD_ANYDEPTH;
  if (fmt >= NVCV_RGBA) flags |= cv::IMREAD_ANYCOLOR;
  ocv = cv::imread(file, flags);
  BAIL_IF_NULL(ocv.data, err, NVCV_ERR_READ);
  NVWrapperForCVMat(&ocv, &nvc);
  if (fmt == NVCV_FORMAT_UNKNOWN) fmt = nvc.pixelFormat;
  if (typ == NVCV_TYPE_UNKNOWN) typ = nvc.componentType;
  BAIL_IF_ERR(err = NvCVImage_Realloc(im, nvc.width, nvc.height, fmt, typ, layout, memspace, align));
  err = NvCVImage_Transfer(&nvc, im, 1.f, nullptr, nullptr);

bail:
  return err;
}

/// Read a color specified in the form 0xRRGGBB or "gray".
/// @param[in]  str     the string to be parsed.
/// @param[out] color   the color, stored as { r, g, b } in a little-endian int.
/// @return     true    if the parsing was successful, false otherwise.
static bool ReadColor(const char* str, int* color) {
  *color = -1;
  if (!strcasecmp("gray", str)) {
    *color = 0x00808080;
    return true;
  } else {
    int n = sscanf(str, "0x%x", color);
    return n > 0;
  }
}

/// Clear a CPU image.
/// @param[in]  color the color, specified as {r, g, b} in a little-endian int.
/// @param[out] im    the image to be cleared to the specified color.
/// @return NVCV_SUCCESS if successful.
static NvCV_Status ClearImage(int color, NvCVImage* im) {
  NvCVImage fr;  // NVCV_BGR assumes little-endian
  NvCVImage_Init(&fr, im->width, im->height, 0, &color, NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CPU);
  fr.pixelBytes = 0;
  fr.pitch = 0;
  return NvCVImage_Transfer(&fr, im, 1.f, 0, 0);
}

/// Iterate through a directory
class DirectoryIterator {
 public:
  enum { typeFile = 1, typeDirectory = 2, typeSpecial = 4, typeAll = (typeFile | typeDirectory | typeSpecial) };

  /// Constructor.
  DirectoryIterator();

  /// Initializing Constructor.
  /// @param[in] path         the path to the directory.
  /// @param[in] iterateWhat  the type of file to iterate.
  DirectoryIterator(const char* path, unsigned iterateWhat);

  /// Destructor.
  ~DirectoryIterator();

  /// Initializer.
  /// @param[in]  path                the path to the directory.
  /// @param[in]  iterateWhat         the type of file to iterate.
  /// @return     NVCV_SUCCESS        if the operation was successful.
  ///             NVCV_ERR_PARAMETER  if there was no directory at the specified path.
  ///             NVCV_ERR_FILE       if there was nothing at the specified path.
  NvCV_Status init(const char* path, unsigned iterateWhat);

  /// Get the next file.
  /// @param[out pName]     a place to store a pointer to the file name.
  /// @param[out] type      a place to store the type of the file.
  /// @return NVCV_SUCCESS  if the operation was successful.
  ///         NVCV_ERR_EOF  if there are no more files.
  NvCV_Status next(const char** pName, unsigned* type);

 private:
  struct Impl;
  Impl* pimpl;
};

#ifdef _MSC_VER  //////////////////////////////////////// WINDOWS ////////////////////////////////////////
#include <Windows.h>
struct DirectoryIterator::Impl {
  HANDLE h;
  unsigned which;
  bool first;
  WIN32_FIND_DATAA data;
};
DirectoryIterator::DirectoryIterator() {
  pimpl = new DirectoryIterator::Impl;
  pimpl->h = nullptr;
}
DirectoryIterator::DirectoryIterator(const char* path, unsigned iterateWhat) : DirectoryIterator() {
  (void)init(path, iterateWhat);
}
DirectoryIterator::~DirectoryIterator() {
  if (pimpl) {
    if (pimpl->h) FindClose(pimpl->h);
    delete pimpl;
  }
}
NvCV_Status DirectoryIterator::init(const char* path, unsigned iterateWhat) {
  DWORD attributes = GetFileAttributesA(path);
  if (INVALID_FILE_ATTRIBUTES == attributes) return NVCV_ERR_FILE;
  if (!(FILE_ATTRIBUTE_DIRECTORY & attributes)) return NVCV_ERR_PARAMETER;
  std::string pathStar = path;
  pathStar += "\\*";
  if (nullptr == (pimpl->h = FindFirstFileA(pathStar.c_str(), &pimpl->data))) return NVCV_ERR_FILE;
  pimpl->which = iterateWhat ? iterateWhat : typeAll;
  pimpl->first = true;
  return NVCV_SUCCESS;
}
NvCV_Status DirectoryIterator::next(const char** pName, unsigned* type) {
  if (!pName) return NVCV_ERR_PARAMETER;
  while (1) {
    if (pimpl->first) {
      pimpl->first = false;
    } else if (!FindNextFileA(pimpl->h, &pimpl->data)) {
      *pName = nullptr;
      if (type) *type = 0;
      return NVCV_ERR_EOF;
    }
    *pName = pimpl->data.cFileName;
    if (0 != (pimpl->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
      if (pimpl->which & typeDirectory) {
        if (type) *type = typeDirectory;
        break;
      }
    } else if (0 != (pimpl->data.dwFileAttributes &
                     (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_COMPRESSED |
                      FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_TEMPORARY))) {
      if (pimpl->which & typeFile) {
        if (type) *type = typeFile;
        break;
      }
    } else {
      if (pimpl->which & typeSpecial) {
        if (type) *type = typeSpecial;
        break;
      }
    }
  }
  return NVCV_SUCCESS;
}
#else  //////////////////////////////////////// UNIX ////////////////////////////////////////
#include <dirent.h>
#include <sys/stat.h>

struct DirectoryIterator::Impl {
  DIR* dp;
  unsigned which;
};
DirectoryIterator::DirectoryIterator() {
  pimpl = new DirectoryIterator::Impl;
  pimpl->dp = nullptr;
}
DirectoryIterator::DirectoryIterator(const char* path, unsigned iterateWhat) : DirectoryIterator() {
  (void)init(path, iterateWhat);
}
DirectoryIterator::~DirectoryIterator() {
  if (pimpl) {
    if (pimpl->dp) closedir(pimpl->dp);
    delete pimpl;
  }
}
NvCV_Status DirectoryIterator::init(const char* path, unsigned iterateWhat) {
  if (nullptr == (pimpl->dp = opendir(path))) {
    struct stat statBuf;
    return stat(path, &statBuf) ? NVCV_ERR_FILE : NVCV_ERR_PARAMETER;
  }
  pimpl->which = iterateWhat ? iterateWhat : typeAll;
  return NVCV_SUCCESS;
}
NvCV_Status DirectoryIterator::next(const char** pName, unsigned* type) {
  struct dirent* entry;
  if (type) *type = 0;
  if (!pName) return NVCV_ERR_PARAMETER;
  while (nullptr != (entry = readdir(pimpl->dp))) {
    *pName = entry->d_name;
    switch (entry->d_type) {
      case DT_REG:
        if (pimpl->which & typeFile) {
          if (type) *type = typeFile;
          return NVCV_SUCCESS;
        }
        break;
      case DT_DIR:
        if (pimpl->which & typeDirectory) {
          if (type) *type = typeDirectory;
          return NVCV_SUCCESS;
        }
        break;
      default:
        if (pimpl->which & typeSpecial) {
          if (type) *type = typeSpecial;
          return NVCV_SUCCESS;
        }
        break;
    }
  }
  *pName = nullptr;
  return NVCV_ERR_EOF;
}
#endif /* _MSC_VER */

/// Determine the type of the given file
/// @param[in]  file  the file to be queried.
/// @return {0, 1, 2} for no {error, file, directory}
static int FileType(const char* file) {
#ifdef _MSC_VER
  DWORD attr = GetFileAttributesA(file);
  if (attr == INVALID_FILE_ATTRIBUTES) return 0;
  if (attr & FILE_ATTRIBUTE_DIRECTORY) return 2;
  if (attr & (FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_COMPRESSED |
              FILE_ATTRIBUTE_REPARSE_POINT | FILE_ATTRIBUTE_SPARSE_FILE | FILE_ATTRIBUTE_TEMPORARY))
    return 1;
  return 0;
#else
  struct stat statBuf;
  if (0 != stat(file, &statBuf)) return 0;
  if (S_IFDIR & statBuf.st_mode) return 2;
  if (S_IFREG & statBuf.st_mode) return 1;
  return 0;
#endif
}

static std::string basenamequote(const std::string& full) {
  std::string base;
  size_t n = full.find_last_of(
#ifdef _MSC_VER
      "/\\"
#else  /* _MSC_VER */
      '/'
#endif /* _MSC_VER */
  );
  base = " \"";
  if (std::string::npos == n)
    base.append(full);
  else
    base.append(full, n + 1, std::string::npos);
  base += '"';
  return base;
}

/// The application class.
struct RelightApp {
  /// Error codes.
  enum Err {
    errQuit = +1,  // Application errors are positive
    errFlag = +2,
    errRead = +3,
    errWrite = +4,
    errNone = NVCV_SUCCESS,  // Video Effects SDK errors are non-positive
    errGeneral = NVCV_ERR_GENERAL,
    errUnimplemented = NVCV_ERR_UNIMPLEMENTED,
    errMemory = NVCV_ERR_MEMORY,
    errEffect = NVCV_ERR_EFFECT,
    errSelector = NVCV_ERR_SELECTOR,
    errBuffer = NVCV_ERR_BUFFER,
    errParameter = NVCV_ERR_PARAMETER,
    errMismatch = NVCV_ERR_MISMATCH,
    errPixelFormat = NVCV_ERR_PIXELFORMAT,
    errModel = NVCV_ERR_MODEL,
    errLibrary = NVCV_ERR_LIBRARY,
    errInitialization = NVCV_ERR_INITIALIZATION,
    errFileNotFound = NVCV_ERR_FILE,
    errFeatureNotFound = NVCV_ERR_FEATURENOTFOUND,
    errMissingInput = NVCV_ERR_MISSINGINPUT,
    errResolution = NVCV_ERR_RESOLUTION,
    errUnsupportedGPU = NVCV_ERR_UNSUPPORTEDGPU,
    errWrongGPU = NVCV_ERR_WRONGGPU,
    errUnsupportedDriver = NVCV_ERR_UNSUPPORTEDDRIVER,
    errCudaMemory = NVCV_ERR_CUDA_MEMORY,  // CUDA errors are also encoded to be negative
    errCudaValue = NVCV_ERR_CUDA_VALUE,
    errCudaPitch = NVCV_ERR_CUDA_PITCH,
    errCudaInit = NVCV_ERR_CUDA_INIT,
    errCudaLaunch = NVCV_ERR_CUDA_LAUNCH,
    errCudaKernel = NVCV_ERR_CUDA_KERNEL,
    errCudaDriver = NVCV_ERR_CUDA_DRIVER,
    errCudaUnsupported = NVCV_ERR_CUDA_UNSUPPORTED,
    errCudaIllegalAddress = NVCV_ERR_CUDA_ILLEGAL_ADDRESS,
    errCuda = NVCV_ERR_CUDA,
  };

  RelightApp()
      : m_relightEff(nullptr),
        m_aigsEff(nullptr),
        m_stream(0),
        m_pan(-90.f * F_RADIANS_PER_DEGREE),
        m_vfov(60.f * F_RADIANS_PER_DEGREE),
        m_showMode(SHOW_OUTPUT),
        m_showFPS(false),
        m_framePeriod(0.f),
        m_autorotate(false),
        m_rotationRate(20.f * F_RADIANS_PER_DEGREE),
        m_autoDelta(2.f * F_RADIANS_PER_DEGREE),
        m_pauseFrame(false) {
  }
  ~RelightApp() { cleanup(); }

  Err initCamera(cv::VideoCapture& cap);
  Err processImage(const std::string& in_file, const std::string& out_file);
  Err processMovie(const std::string& in_file, const std::string& out_file);
  Err processKey(int key);
  void drawFrameRate(cv::Mat& img);
  Err app_errFromVfxStatus(NvCV_Status status) { return (Err)status; }
  const char* errorStringFromCode(Err code);
  void cleanup();
  NvCV_Status readBackground(const std::string& file);
  NvCV_Status readHDRlist();

  void setPan(float pan) { m_pan = pan * F_RADIANS_PER_DEGREE; }
  void setVFOV(float vfov) { m_vfov = vfov * F_RADIANS_PER_DEGREE; }
  void setShowMode(unsigned show_mode) { m_showMode = show_mode; }
  void setAutorotate(bool yes) { m_autorotate = yes; }
  void setRotationRate(float rate) { m_rotationRate = rate * F_RADIANS_PER_DEGREE; }

  NvVFX_Handle m_relightEff;  ///< Relighting effect handle.
  NvVFX_Handle m_aigsEff;     ///< AIGS effect handle.
  CUstream m_stream;          ///< CUDA stream.

  NvCVImage m_cSrc,    ///< CPU src image.
      m_cMat,          ///< CPU matte.
      m_cDst,          ///< CPU dst image.
      m_cHdr,          ///< CPU HDR image.
      m_cBkg,          ///< CPU background image.
      m_gSrc,          ///< GPU src image.
      m_gMat,          ///< GPU matte.
      m_gDst,          ///< GPU dst image.
      m_gBkg,          ///< GPU background image.
      m_gPrj,          ///< GPU projected HDR image.
      m_gBlr,          ///< GPU blurred background.
      m_tmp;           ///< temporary CUDA working and staging image.
  cv::Mat m_cvInput;   ///< OpenCV alias for m_cSrc.
  cv::Mat m_cvOutput;  ///< OpenCV alias for m_cDst.

  float m_pan;  ///< Current pan angle.
  float m_vfov;  ///< Current vertical field of view.

  unsigned m_showMode;   ///< Whether to show the output (default) or the input.
  bool m_showFPS;        ///< Whether to show the frame rate.
  bool m_pauseFrame;     ///< Whether to stop on the current movie frame.
  bool m_autorotate;     ///< Autorotate.
  float m_autoDelta;     ///< Delta angle for the auto-rotate.
  float m_framePeriod;   ///< The frame period, in seconds.
  float m_rotationRate;  ///< The rotation rate in radians per second.

  unsigned m_hdrIndex;
  std::vector<std::string> m_hdrFiles;

  std::chrono::high_resolution_clock::time_point m_lastTime;  ///< The last frame period measurement.
  unsigned m_backgroundMode =
      BGMODE_SRC;  // 0 = src, 1 = blurred src, 2 = HDR, 3 = background if supplied, 4 = blurred background if supplied
  NvVFX_Handle m_bgBlurEff = nullptr;
};

const char* RelightApp::errorStringFromCode(Err code) {
  struct LutEntry {
    Err code;
    const char* str;
  };
  static const LutEntry lut[] = {
      {errRead, "There was a problem reading a file"},
      {errWrite, "There was a problem writing a file"},
      {errQuit, "The user chose to quit the application"},
      {errFlag, "There was a problem with the command-line arguments"},
  };
  if ((int)code <= 0) return NvCV_GetErrorStringFromCode((NvCV_Status)code);
  for (const LutEntry* p = lut; p != &lut[sizeof(lut) / sizeof(lut[0])]; ++p)
    if (p->code == code) return p->str;
  return "UNKNOWN ERROR";
}

void RelightApp::cleanup() {
  if (m_bgBlurEff) {
    NvVFX_DestroyEffect(m_bgBlurEff);
    m_bgBlurEff = nullptr;
  }
  if (m_relightEff) {
    NvVFX_DestroyEffect(m_relightEff);
    m_relightEff = nullptr;
  }
  if (m_aigsEff) {
    NvVFX_DestroyEffect(m_aigsEff);
    m_aigsEff = nullptr;
  }
  if (m_stream) {
    NvVFX_CudaStreamDestroy(m_stream);
    m_stream = 0;
  }
}

NvCV_Status RelightApp::readBackground(const std::string& file) {
  NvCV_Status err;
  if (HasOneOfTheseSuffixes(file.c_str(), ".png", ".jpg", ".jpeg", ".tif", ".tiff", nullptr))
    if (NVCV_SUCCESS != (err = ReadImage(FLAG_inBg.c_str(), NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CPU, 0, &m_cBkg)))
      printf("Cannot read background file \"%s\"\n", FLAG_inBg.c_str());
  if (m_cBkg.pixels) {  // Resize bkg to match destination (or maybe crop?)
    NvCVImage mid(m_gBkg.width, m_gBkg.height, NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CPU, 0);
    cv::Mat ocSrc, ocDst;
    CVWrapperForNvCVImage(&m_cBkg, &ocSrc);
    CVWrapperForNvCVImage(&mid, &ocDst);
    cv::resize(ocSrc, ocDst, ocDst.size(), 0, 0, cv::INTER_LINEAR);  // INTER_CUBIC or INTER_AREA are probably better
    err = NvCVImage_Transfer(&mid, &m_gBkg, 1.f, m_stream, &m_tmp);
  } else {
    int color;
    if (!ReadColor(FLAG_inBg.c_str(), &color)) color = 0x808080;  // gray background image
    ClearImage(color, &m_gBkg);
  }
  return NVCV_SUCCESS;
}

NvCV_Status RelightApp::readHDRlist() {
  if (FLAG_inHDR.empty()) return NVCV_ERR_FILE;
  switch (FileType(FLAG_inHDR.c_str())) {
    case 0:
      return NVCV_ERR_FILE;
    case 1:
      m_hdrFiles.push_back(FLAG_inHDR);
      break;
    case 2: {
      DirectoryIterator dit;
      NvCV_Status err = dit.init(FLAG_inHDR.c_str(), DirectoryIterator::typeFile);
      if (NVCV_SUCCESS != err) return err;
      const char* name;
      unsigned type;
      while (NVCV_ERR_EOF != dit.next(&name, &type))
        if (HasOneOfTheseSuffixes(name, ".hdr", ".exr", nullptr)) m_hdrFiles.push_back(FLAG_inHDR + '/' + name);
    }
  }
  return NVCV_SUCCESS;
}

RelightApp::Err RelightApp::initCamera(cv::VideoCapture& cap) {
  const int camIndex = 0;
  cap.open(camIndex);
  if (!FLAG_camRes.empty()) {
    int camWidth, camHeight, n;
    n = sscanf(FLAG_camRes.c_str(), "%d%*[xX]%d", &camWidth, &camHeight);
    switch (n) {
      case 2:
        break;  // We have read both width and height
      case 1:
        camHeight = camWidth;
        camWidth = (int)(camHeight * (16. / 9.) + .5);
        break;
      default:
        camHeight = 0;
        camWidth = 0;
        break;
    }

    if (camWidth) cap.set(cv::CAP_PROP_FRAME_WIDTH, camWidth);
    if (camHeight) cap.set(cv::CAP_PROP_FRAME_HEIGHT, camHeight);
    if (camWidth != cap.get(cv::CAP_PROP_FRAME_WIDTH) || camHeight != cap.get(cv::CAP_PROP_FRAME_HEIGHT))
      printf("Error: Camera does not support %d x %d resolution; using %.0f x %.0f instead\n", camWidth, camHeight,
             cap.get(cv::CAP_PROP_FRAME_WIDTH), cap.get(cv::CAP_PROP_FRAME_HEIGHT));
  }
  return errNone;
}

void RelightApp::drawFrameRate(cv::Mat& img) {
  const float timeConstant = 16.f;
  std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> dur = std::chrono::duration_cast<std::chrono::duration<float>>(now - m_lastTime);
  float t = dur.count();
  if (0.f < t && t < 100.f) {
    if (m_framePeriod)
      m_framePeriod += (t - m_framePeriod) * (1.f / timeConstant);  // 1 pole IIR filter
    else
      m_framePeriod = t;
    if (m_showFPS) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%.1f", 1. / m_framePeriod);
      cv::putText(img, buf, cv::Point(10, img.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 1);
    }
  } else {                // Ludicrous time interval; reset
    m_framePeriod = 0.f;  // WAKE UP
  }
  m_lastTime = now;
}

RelightApp::Err RelightApp::processKey(int key) {
  switch (key) {
    case 'q':  // fall through
    case ESC:
      goto bail;  // q or ESC   quit
    case '?':
    case 'h':
      PrintKeyboardControlLegend();  // h or ?   print key legend
      break;
    case ',':
      m_pan -= 1.f * F_RADIANS_PER_DEGREE;  // ,        pan left  1
      goto adjust_pan;
    case '.':
      m_pan += 1.f * F_RADIANS_PER_DEGREE;  // .        pan right 1
      goto adjust_pan;
    case '<':
      m_pan -= 10.f * F_RADIANS_PER_DEGREE;  // <        pan left  10
      goto adjust_pan;
    case '>':
      m_pan += 10.f * F_RADIANS_PER_DEGREE;  // >        pan right 10
      goto adjust_pan;
    adjust_pan:
      m_pan = fmodf(m_pan, F_2PI);
      if (m_pan <= -F_PI)
        m_pan += F_2PI;
      else if (m_pan > F_PI)
        m_pan -= F_2PI;
      (void)NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_PAN, m_pan);
      break;
    case 'V':
      m_vfov += 10.f * F_RADIANS_PER_DEGREE;  // V        10 degrees wider FOV
      goto adjust_fov;
    case 'v':
      m_vfov -= 10.f * F_RADIANS_PER_DEGREE;  // v        10 degrees narrower FOV
      goto adjust_fov;
    adjust_fov:
      (void)NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_VFOV, m_vfov);
      break;
    case 'r':
      m_autorotate = !m_autorotate;  // r        auto-rotate
      break;
    case ' ':  // space or
    case 'p':
      m_pauseFrame = !m_pauseFrame;  // p        pause video
      break;
    case 'i':
      m_showMode = ~m_showMode & SHOW_INPUT;  // i        toggle show input
      break;
    case 'o':
      m_showMode = SHOW_OUTPUT;  // o        show output
      break;
    case 'f':
      m_showFPS = !m_showFPS;  // f        show frame rate
      break;
    case 'b':
      m_backgroundMode = (m_backgroundMode + 1u) % 5u;
      break;
    case 'z':
      for (size_t i = 0; i < m_hdrFiles.size(); ++i) {  // z jump to studio relighting
        if (std::string::npos != m_hdrFiles[i].rfind("vkl_mid.hdr")) {
          m_hdrIndex = (unsigned)((i ? i : m_hdrIndex) - 1);
          (void)NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_PAN, m_pan = -90.f * F_RADIANS_PER_DEGREE);
          (void)NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_VFOV, m_vfov = 60.f * F_RADIANS_PER_DEGREE);
          break;
        }
      }
      // Fall through
    case 'n':
      (void)NvCVImage_Dealloc(&m_cHdr);  // n        next HDR
      m_hdrIndex = (m_hdrIndex + 1) % unsigned(m_hdrFiles.size());
      while (!m_hdrFiles.empty() && NVCV_SUCCESS != ReadImage(m_hdrFiles[m_hdrIndex].c_str(), NVCV_RGB, NVCV_F32,
                                                              NVCV_CHUNKY, NVCV_CPU, 0, &m_cHdr)) {
        printf("Cannot read \"%s\"\n", m_hdrFiles[m_hdrIndex].c_str());
        m_hdrFiles.erase(m_hdrFiles.begin() + m_hdrIndex);  // Erase it from the list
        m_hdrIndex %= unsigned(m_hdrFiles.size());          // Make sure we point to a valid image
      }
      if (m_hdrFiles.empty()) goto bail;  // No HDR files left
      (void)NvVFX_SetImage(m_relightEff, NVVFX_INPUT_IMAGE_2, &m_cHdr);
      break;
    default:
      break;
  }
  if (key > 0 && key < 255) {
    std::string status = (SHOW_INPUT == m_showMode) ? "input" : "output";
    if (m_pauseFrame) status += " paused";
    if (m_autorotate) status += " autorotate";
    if ('n' == key) status += basenamequote(m_hdrFiles[m_hdrIndex]);
    static const char* bkstr[] = {"src", "src-blurred", "env", "bg-img", "bg-img-blurred"};
    status += ' ';
    if (m_backgroundMode < 5) status += bkstr[m_backgroundMode];
    if (' ' <= key && key <= '~')
      printf("%c", key);
    else
      printf("ctl-%c (%02x)", key + '@', key);

    printf(": pan=%.3g vfov=%.3g %s\n", m_pan * F_DEGREES_PER_RADIAN, m_vfov * F_DEGREES_PER_RADIAN, status.c_str());
  }
  return errNone;
bail:
  return errQuit;
}

RelightApp::Err RelightApp::processImage(const std::string& in_file, const std::string& out_file) {
  const char win_name[] = "OutputWindow";
  NvCV_Status err;
  cv::Mat cv_img;
  NvCVImage cv_img_wrapper;

  // Create a CUDA stream and the relighting effect
  BAIL_IF_ERR(err = NvVFX_CudaStreamCreate(&m_stream));
  BAIL_IF_ERR(err = NvVFX_CreateEffect(NVVFX_FX_RELIGHTING, &m_relightEff));

  // Read input source image
  cv_img = cv::imread(in_file);
  if (!cv_img.data) {
    printf("Cannot read input file \"%s\"\n", in_file.c_str());
    return errRead;
  }
  NVWrapperForCVMat(&cv_img, &cv_img_wrapper);

  // Read HDR image
  if (NVCV_SUCCESS != (err = ReadImage(FLAG_inHDR.c_str(), NVCV_RGB, NVCV_F32, NVCV_CHUNKY, NVCV_CPU, 0, &m_cHdr))) {
    printf("Cannot read HDR file \"%s\"\n", FLAG_inHDR.c_str());
    return app_errFromVfxStatus(err);
  }

  // Allocate CPU and GPU buffers as necessary
  BAIL_IF_ERR(err = NvCVImage_Alloc(&m_gSrc, cv_img_wrapper.width, cv_img_wrapper.height, NVCV_BGR, NVCV_U8,
                                    NVCV_CHUNKY, NVCV_CUDA, 1));
  BAIL_IF_ERR(err = NvCVImage_Alloc(&m_gMat, m_gSrc.width, m_gSrc.height, NVCV_A, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 1));
  BAIL_IF_ERR(err =
                  NvCVImage_Alloc(&m_gDst, m_gSrc.width, m_gSrc.height, NVCV_RGB, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 1));
  BAIL_IF_ERR(err =
                  NvCVImage_Alloc(&m_gPrj, m_gSrc.width, m_gSrc.height, NVCV_RGB, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 0));
  BAIL_IF_ERR(err =
                  NvCVImage_Alloc(&m_gBkg, m_gSrc.width, m_gSrc.height, NVCV_RGB, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 0));
  BAIL_IF_ERR(err =
                  NvCVImage_Alloc(&m_gBlr, m_gSrc.width, m_gSrc.height, NVCV_RGB, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 0));

  // Open the background image if given, or set to gray
  BAIL_IF_ERR(err = readBackground(FLAG_inBg));

  // Set input and output parameters
  BAIL_IF_ERR(err = NvVFX_SetCudaStream(m_relightEff, NVVFX_CUDA_STREAM, m_stream));
  BAIL_IF_ERR(err = NvVFX_SetString(m_relightEff, NVVFX_MODEL_DIRECTORY, FLAG_modelsDir.c_str()));
  BAIL_IF_ERR(err = NvVFX_SetImage(m_relightEff, NVVFX_INPUT_IMAGE_0, &m_gSrc));   // src in
  BAIL_IF_ERR(err = NvVFX_SetImage(m_relightEff, NVVFX_INPUT_IMAGE_1, &m_gMat));   // mat in
  err = NvVFX_SetImage(m_relightEff, NVVFX_INPUT_IMAGE_2, &m_cHdr);                // hdr in
  if (NVCV_ERR_WRONGSIZE != err) BAIL_IF_ERR(err);                                 // don't exit if HDR != 2:1
  BAIL_IF_ERR(err = NvVFX_SetImage(m_relightEff, NVVFX_OUTPUT_IMAGE_0, &m_gDst));  // dst out
  BAIL_IF_ERR(err = NvVFX_SetImage(m_relightEff, NVVFX_OUTPUT_IMAGE_1, &m_gPrj));  // projected HDR out
  BAIL_IF_ERR(err = NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_PAN, m_pan));
  BAIL_IF_ERR(err = NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_VFOV, m_vfov));

  // Load relighting effect
  err = NvVFX_Load(m_relightEff);
  if (NVCV_SUCCESS != err) {
    printf("Cannot load model from \"%s\"\n", FLAG_modelsDir.c_str());
    BAIL_IF_ERR(err);
  }

  // Initialize AIGS filter for generating mattes
  if (FLAG_inMat.empty()) {
    BAIL_IF_ERR(err = NvVFX_CreateEffect(NVVFX_FX_GREEN_SCREEN, &m_aigsEff));
    BAIL_IF_ERR(err = NvVFX_SetCudaStream(m_aigsEff, NVVFX_CUDA_STREAM, m_stream));
    BAIL_IF_ERR(err = NvVFX_SetString(m_aigsEff, NVVFX_MODEL_DIRECTORY, FLAG_modelsDir.c_str()));
    BAIL_IF_ERR(err = NvVFX_SetImage(m_aigsEff, NVVFX_INPUT_IMAGE, &m_gSrc));
    BAIL_IF_ERR(err = NvVFX_SetImage(m_aigsEff, NVVFX_OUTPUT_IMAGE, &m_gMat));
    BAIL_IF_ERR(err = NvVFX_SetU32(m_aigsEff, NVVFX_CUDA_GRAPH, 1u));
    BAIL_IF_ERR(err = NvVFX_SetU32(m_aigsEff, NVVFX_MODE, 2u));  // Default AIGS mode is BG - Qual
    BAIL_IF_ERR(err = NvVFX_Load(m_aigsEff));
  } else {
    err = ReadImage(FLAG_inMat.c_str(), NVCV_A, NVCV_U8, NVCV_CHUNKY, NVCV_CPU, 0, &m_cMat);
    if (NVCV_SUCCESS != err) {
      printf("Cannot open matte file \"%s\"\n", FLAG_inMat.c_str());
      BAIL_IF_ERR(err);
    }
    if (!(m_cMat.width == m_gSrc.width && m_cMat.height == m_gSrc.height)) {
      printf("Error: Matte image resolution (%dx%d) does not match input image resolution (%ux%u)\n", m_cMat.width,
             m_cMat.height, m_gSrc.width, m_gSrc.height);
      return errResolution;
    }
    // Copy CPU matte to GPU
    BAIL_IF_ERR(err = NvCVImage_Transfer(&m_cMat, &m_gMat, 1.f, m_stream, &m_tmp));
  }

  if (BGMODE_SRC_BLURRED == FLAG_bgMode || BGMODE_BG_BLURRED == FLAG_bgMode) {  // Blur requested
    BAIL_IF_ERR(err = NvVFX_CreateEffect(NVVFX_FX_BGBLUR, &m_bgBlurEff));
    BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_OUTPUT_IMAGE, &m_gBlr));
    BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_0, &m_gSrc));
    BAIL_IF_ERR(err = NvVFX_Load(m_bgBlurEff));  // Load needs an input and output
  }

  if (FLAG_show) cv::namedWindow(win_name);

  BAIL_IF_ERR(err = NvCVImage_Transfer(&cv_img_wrapper, &m_gSrc, 1.f, m_stream, &m_tmp));
  if (FLAG_inMat.empty())  // Run AIGS to segment the input, unless a matte has been explicitly specified
    BAIL_IF_ERR(err = NvVFX_Run(m_aigsEff, 1));
  BAIL_IF_ERR(err = NvVFX_Run(m_relightEff, 1));
  switch (FLAG_bgMode) {
    case BGMODE_SRC:
      BAIL_IF_ERR(err = NvCVImage_Composite(&m_gDst, &m_gSrc, &m_gMat, &m_gDst, m_stream));
      break;
    case BGMODE_SRC_BLURRED:
      BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_0, &m_gSrc));
      BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_1, &m_gMat));
      BAIL_IF_ERR(err = NvVFX_Run(m_bgBlurEff, 1));  // Background blur the src
      BAIL_IF_ERR(err = NvCVImage_Composite(&m_gDst, &m_gBlr, &m_gMat, &m_gDst, m_stream));
      break;
    case BGMODE_HDR:
      BAIL_IF_ERR(err = NvCVImage_Composite(&m_gDst, &m_gPrj, &m_gMat, &m_gDst, m_stream));
      break;
    case BGMODE_BG:
      BAIL_IF_ERR(err = NvCVImage_Composite(&m_gDst, &m_gBkg, &m_gMat, &m_gDst, m_stream));
      break;
    case BGMODE_BG_BLURRED:
      BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_0, &m_gBkg));
      // BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_1, nullptr));   // Already NULL
      BAIL_IF_ERR(err = NvVFX_Run(m_bgBlurEff, 1));  // Blur the background image
      BAIL_IF_ERR(err = NvCVImage_Composite(&m_gDst, &m_gBlr, &m_gMat, &m_gDst, m_stream));
      break;
  }
  BAIL_IF_ERR(err = NvCVImage_Transfer(&m_gDst, &cv_img_wrapper, 1.f, m_stream, &m_tmp));
  NvVFX_CudaStreamSynchronize(m_stream);

  if (FLAG_show) {
    cv::imshow(win_name, cv_img);
    (void)cv::waitKey(3000);
  }

  if (!out_file.empty() && !cv::imwrite(out_file, cv_img)) {
    printf("Error writing: \"%s\"\n", out_file.c_str());
    return errWrite;
  }

bail:
  if (FLAG_show) cv::destroyWindow(win_name);
  cleanup();
  return app_errFromVfxStatus(err);
}

RelightApp::Err RelightApp::processMovie(const std::string& in_file, const std::string& out_file) {
  const char win_name[] = "OutputWindow";
  RelightApp::Err app_err = errNone;
  bool use_nominal_framerate = false;  // used for autorotate
  cv::VideoCapture src_reader;
  cv::VideoWriter writer;
  NvCV_Status err;
  unsigned frame_num;
  VideoInfo src_info;

  // Create a CUDA stream and the relighting effect
  BAIL_IF_ERR(err = NvVFX_CudaStreamCreate(&m_stream));
  BAIL_IF_ERR(err = NvVFX_CreateEffect(NVVFX_FX_RELIGHTING, &m_relightEff));

  // Read input source image/webcam
  if (!FLAG_webcam && !in_file.empty()) {
    src_reader.open(in_file);
  } else {
    app_err = initCamera(src_reader);
    if (app_err != errNone) return app_err;
  }

  if (!src_reader.isOpened()) {
    printf("Error: Could not open video: \"%s\"\n", in_file.c_str());
    return errRead;
  }

  // Read source video
  GetVideoInfo(src_reader, (in_file.empty() ? "webcam" : in_file.c_str()), &src_info);
  if (src_info.frameCount > 0 && src_info.frameRate > 0 && !out_file.empty()) use_nominal_framerate = true;

  // Read HDR image
  if (m_hdrFiles.empty() && NVCV_SUCCESS != (err = readHDRlist())) {
    printf("Cannot get HDR file list from \"%s\"\n", FLAG_inHDR.c_str());
    return app_errFromVfxStatus(err);
  }
  m_hdrIndex = 0;
  while (!m_hdrFiles.empty() && NVCV_SUCCESS != (err = ReadImage(m_hdrFiles[m_hdrIndex].c_str(), NVCV_RGB, NVCV_F32,
                                                                 NVCV_CHUNKY, NVCV_CPU, 0, &m_cHdr))) {
    printf("Cannot read HDR file \"%s\"\n", m_hdrFiles[m_hdrIndex].c_str());
    m_hdrFiles.erase(m_hdrFiles.begin());
  }
  if (NVCV_SUCCESS != err) return app_errFromVfxStatus(err);

  // Allocate CPU and GPU buffers as necessary
  BAIL_IF_ERR(err = NvCVImage_Alloc(&m_cSrc, src_info.width, src_info.height, NVCV_BGR, NVCV_U8, NVCV_CHUNKY,
                                    NVCV_CPU_PINNED, 0));
  BAIL_IF_ERR(err =
                  NvCVImage_Alloc(&m_gSrc, m_cSrc.width, m_cSrc.height, NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 1));
  BAIL_IF_ERR(err = NvCVImage_Alloc(&m_gMat, m_gSrc.width, m_gSrc.height, NVCV_A, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 1));
  BAIL_IF_ERR(err =
                  NvCVImage_Alloc(&m_gDst, m_gSrc.width, m_gSrc.height, NVCV_RGB, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 1));
  BAIL_IF_ERR(
      err = NvCVImage_Alloc(&m_cDst, m_gDst.width, m_gDst.height, NVCV_BGR, NVCV_U8, NVCV_CHUNKY, NVCV_CPU_PINNED, 0));
  BAIL_IF_ERR(err =
                  NvCVImage_Alloc(&m_gPrj, m_gSrc.width, m_gSrc.height, NVCV_RGB, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 0));
  BAIL_IF_ERR(err =
                  NvCVImage_Alloc(&m_gBkg, m_gSrc.width, m_gSrc.height, NVCV_RGB, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA, 0));
  // BAIL_IF_ERR(err = NvCVImage_Alloc(&m_gBlr, m_gSrc.width, m_gSrc.height, NVCV_RGB, NVCV_U8, NVCV_CHUNKY, NVCV_CUDA,
  // 0));

  // Open the background image if given
  BAIL_IF_ERR(err = readBackground(FLAG_inBg));

  CVWrapperForNvCVImage(&m_cSrc, &m_cvInput);   // m_cvInput  is an alias for m_cSrc
  CVWrapperForNvCVImage(&m_cDst, &m_cvOutput);  // m_cvOutput is an alias for m_cDst

  // If output file is specified, open a writer for writing the output video
  if (!out_file.empty() && !writer.open(out_file, StringToFourcc(FLAG_codec), src_info.frameRate,
                                        cv::Size(src_info.width, src_info.height))) {
    printf("Cannot open \"%s\" for video writing\n", out_file.c_str());
    if (!FLAG_show) return errWrite;
  }

  // Set input and output parameters
  BAIL_IF_ERR(err = NvVFX_SetCudaStream(m_relightEff, NVVFX_CUDA_STREAM, m_stream));
  BAIL_IF_ERR(err = NvVFX_SetString(m_relightEff, NVVFX_MODEL_DIRECTORY, FLAG_modelsDir.c_str()));
  BAIL_IF_ERR(err = NvVFX_SetImage(m_relightEff, NVVFX_INPUT_IMAGE_0, &m_gSrc));   // src in
  BAIL_IF_ERR(err = NvVFX_SetImage(m_relightEff, NVVFX_INPUT_IMAGE_1, &m_gMat));   // mat in
  err = NvVFX_SetImage(m_relightEff, NVVFX_INPUT_IMAGE_2, &m_cHdr);                // hdr in
  if (NVCV_ERR_WRONGSIZE != err) BAIL_IF_ERR(err);                                 // don't exit if not 2:1
  BAIL_IF_ERR(err = NvVFX_SetImage(m_relightEff, NVVFX_OUTPUT_IMAGE_0, &m_gDst));  // dst out
  BAIL_IF_ERR(err = NvVFX_SetImage(m_relightEff, NVVFX_OUTPUT_IMAGE_1, &m_gPrj));  // projected HDR out
  BAIL_IF_ERR(err = NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_PAN, m_pan));
  BAIL_IF_ERR(err = NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_VFOV, m_vfov));
  if (FLAG_show) PrintKeyboardControlLegend();

  // Load relighting effect
  err = NvVFX_Load(m_relightEff);
  if (NVCV_SUCCESS != err) {
    printf("Cannot load model from \"%s\"\n", FLAG_modelsDir.c_str());
    BAIL_IF_ERR(err);
  }

  // Initialize AIGS filter for generating mattes
  BAIL_IF_ERR(err = NvVFX_CreateEffect(NVVFX_FX_GREEN_SCREEN, &m_aigsEff));
  BAIL_IF_ERR(err = NvVFX_SetCudaStream(m_aigsEff, NVVFX_CUDA_STREAM, m_stream));
  BAIL_IF_ERR(err = NvVFX_SetString(m_aigsEff, NVVFX_MODEL_DIRECTORY, FLAG_modelsDir.c_str()));
  BAIL_IF_ERR(err = NvVFX_SetImage(m_aigsEff, NVVFX_INPUT_IMAGE, &m_gSrc));
  BAIL_IF_ERR(err = NvVFX_SetImage(m_aigsEff, NVVFX_OUTPUT_IMAGE, &m_gMat));
  BAIL_IF_ERR(err = NvVFX_SetU32(m_aigsEff, NVVFX_CUDA_GRAPH, 1u));
  BAIL_IF_ERR(err = NvVFX_SetU32(m_aigsEff, NVVFX_MODE, 2u));  // Default AIGS mode is BG - Qual

  BAIL_IF_ERR(err = NvVFX_Load(m_aigsEff));

  // You can get better quality and potentially higher speed by blurring before compositing.
  // BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_OUTPUT_IMAGE, &m_gBlr));
  // switch (m_backgroundMode) {
  // case 2: err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_0, &m_gSrc);
  //         err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_1, &m_gMat);
  //         break;
  // case 4: err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_0, &m_gBkg);
  //         err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_1, nullptr);
  //         break;
  // }
  // src blur can occur in parallel with relighting.
  // bkg blur is faster with no matte, and only needs be done once for a static image.
  // The blured result is then composited thusly:
  // err = NvCVImage_Composite(&m_gDst, &m_gBlr, &m_gMat, &m_gDst, m_stream);
  // The code below requires no reconfiguring when changing background mode.
  m_backgroundMode = FLAG_bgMode;
  BAIL_IF_ERR(err = NvVFX_CreateEffect(NVVFX_FX_BGBLUR, &m_bgBlurEff));
  BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_0, &m_gDst));
  BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_INPUT_IMAGE_1, &m_gMat));
  BAIL_IF_ERR(err = NvVFX_SetImage(m_bgBlurEff, NVVFX_OUTPUT_IMAGE, &m_gDst));
  BAIL_IF_ERR(err = NvVFX_Load(m_bgBlurEff));

  if (FLAG_show) cv::namedWindow(win_name);

  for (frame_num = 0; m_pauseFrame || src_reader.read(m_cvInput); m_pauseFrame || ++frame_num) {
    if (m_cvInput.empty()) printf("Frame %u is empty\n", frame_num);

    BAIL_IF_ERR(err = NvCVImage_Transfer(&m_cSrc, &m_gSrc, 1.f, m_stream, &m_tmp));
    BAIL_IF_ERR(err = NvVFX_Run(m_aigsEff, 1));
    BAIL_IF_ERR(err = NvVFX_Run(m_relightEff, 1));
    switch (m_backgroundMode) {
      case BGMODE_HDR:
        err = NvCVImage_Composite(&m_gDst, &m_gPrj, &m_gMat, &m_gDst, m_stream);
        if (err) std::cerr << NvCV_GetErrorStringFromCode(err);
        break;
      case BGMODE_SRC:
        err = NvCVImage_Composite(&m_gDst, &m_gSrc, &m_gMat, &m_gDst, m_stream);
        if (err) std::cerr << NvCV_GetErrorStringFromCode(err);
        break;
      case BGMODE_SRC_BLURRED:
        err = NvCVImage_Composite(&m_gDst, &m_gSrc, &m_gMat, &m_gDst, m_stream);
        if (err) std::cerr << NvCV_GetErrorStringFromCode(err);
        err = NvVFX_Run(m_bgBlurEff, 1);  // TODO: better to blur before composite
        if (err) std::cerr << NvCV_GetErrorStringFromCode(err);
        break;
      case BGMODE_BG:
        err = NvCVImage_Composite(&m_gDst, &m_gBkg, &m_gMat, &m_gDst, m_stream);
        if (err) std::cerr << NvCV_GetErrorStringFromCode(err);
        break;
      case BGMODE_BG_BLURRED:
        err = NvCVImage_Composite(&m_gDst, &m_gBkg, &m_gMat, &m_gDst, m_stream);
        if (err) std::cerr << NvCV_GetErrorStringFromCode(err);
        err = NvVFX_Run(m_bgBlurEff, 1);  // TODO: better to blur before composite
        if (err) std::cerr << NvCV_GetErrorStringFromCode(err);
        break;
    }
    BAIL_IF_ERR(err = NvCVImage_Transfer(&m_gDst, &m_cDst, 1.f, m_stream, &m_tmp));
    BAIL_IF_ERR(err = NvVFX_CudaStreamSynchronize(m_stream));

    cv::Mat* show_output;
    switch (m_showMode) {
      default:
      case SHOW_OUTPUT:
        show_output = &m_cvOutput;
        break;
      case SHOW_INPUT:
        show_output = &m_cvInput;
        break;
    }
    if (writer.isOpened()) writer.write(*show_output);
    drawFrameRate(*show_output);
    if (FLAG_show) {
      cv::imshow(win_name, *show_output);

      int key = cv::waitKey(1);
      if (key > 0) {
        app_err = processKey(key);
        if (errQuit == app_err) break;
      }
    }
    if (m_autorotate) {  // 20 degrees per second. When processing videos, we use the nominal framerate
      if (m_framePeriod)
        m_autoDelta =
            use_nominal_framerate ? float(m_rotationRate / src_info.frameRate) : (m_rotationRate * m_framePeriod);
      (void)NvVFX_SetF32(m_relightEff, NVVFX_ANGLE_PAN, m_pan = fmodf(m_pan + m_autoDelta, F_2PI));
    }
  }

bail:
  src_reader.release();
  if (writer.isOpened()) writer.release();
  if (FLAG_show) cv::destroyWindow(win_name);
  cleanup();
  return app_errFromVfxStatus(err);
}

int main(int argc, char** argv) {
  int nErrs = 0;
  RelightApp::Err err = RelightApp::errNone;
  RelightApp app;

  nErrs = ParseMyArgs(argc, argv);
  if (nErrs) {
    if (NVCV_ERR_HELP == nErrs) return nErrs;
    std::cerr << nErrs << " command line syntax problems\n";
  }

  NvCV_Status vfxErr = NvVFX_ConfigureLogger(FLAG_logLevel, FLAG_log.c_str(), nullptr, nullptr);
  if (NVCV_SUCCESS != vfxErr)
    printf("%s: while configuring logger to \"%s\"\n", NvCV_GetErrorStringFromCode(vfxErr), FLAG_log.c_str());

  if (FLAG_webcam)     // If the input is a webcam, ...
    FLAG_show = true;  // ... force show to bring up a window that can respond to key commands in order to quit.
  if (FLAG_modelsDir.empty()) {
    std::cerr << "Please specify --model_dir=/path/to/trtpkg_directory\n";
    ++nErrs;
  }
  if (FLAG_inFile.empty() && !FLAG_webcam) {
    std::cerr << "Please specify --in_file=XXX or --webcam=true\n";
    ++nErrs;
  }
  if (FLAG_inHDR.empty()) {
    std::cerr << "Please specify --in_hdr=XXX\n";
    ++nErrs;
  }
  if (FLAG_outFile.empty() && !FLAG_show) {
    std::cerr << "Please specify --out_file=XXX or --show\n";
    ++nErrs;
  }

  // Set app initial values
  app.setPan(FLAG_pan);
  app.setVFOV(FLAG_vfov);
  app.setAutorotate(FLAG_autorotate);
  app.setRotationRate(FLAG_rotationRate);
  app.setShowMode(("input" == FLAG_showMode) ? SHOW_INPUT : SHOW_OUTPUT);

  if (nErrs) {
    Usage();
    err = RelightApp::errFlag;
  } else {
    if (IsImageFile(FLAG_inFile.c_str()))
      err = app.processImage(FLAG_inFile.c_str(), FLAG_outFile.c_str());
    else
      err = app.processMovie(FLAG_inFile.c_str(), FLAG_outFile.c_str());
  }

  if (err) std::cerr << "Error: " << app.errorStringFromCode(err) << std::endl;
  return int(err);
}
