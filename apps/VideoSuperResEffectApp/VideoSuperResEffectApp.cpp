/*
 * SPDX-FileCopyrightText: Copyright (c) 2020-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
#include <vector>

#include "nvCVOpenCV.h"
#include "unicodeUtf8Utils.h"
#include "nvVFXVideoSuperRes.h"
#include "nvVideoEffects.h"
#include "opencv2/opencv.hpp"

#ifdef _MSC_VER
#define strcasecmp _stricmp
#include <Windows.h>
#else  // !_MSC_VER
#include <sys/stat.h>
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

#ifdef _WIN32
#define DEFAULT_CODEC "avc1"
#else  // !_WIN32
#define DEFAULT_CODEC "H264"
#endif  // _WIN32

struct VSRModeInfo {
  unsigned int mode;
  const char* name;
};

static const VSRModeInfo kVSRModes[] = {
    {0, "VSR Bicubic"},          //
    {1, "VSR Low"},              //
    {2, "VSR Medium"},           //
    {3, "VSR High"},             //
    {4, "VSR Ultra"},            //
    {8, "Denoise Low"},          //
    {9, "Denoise Medium"},       //
    {10, "Denoise High"},        //
    {11, "Denoise Ultra"},       //
    {12, "Deblur Low"},          //
    {13, "Deblur Medium"},       //
    {14, "Deblur High"},         //
    {15, "Deblur Ultra"},        //
    {16, "HighBitrate Low"},     //
    {17, "HighBitrate Medium"},  //
    {18, "HighBitrate High"},    //
    {19, "HighBitrate Ultra"},   //
    {21, "Streaming Medium"},    //
    {23, "Streaming Ultra"},     //
};
static constexpr int kVSRModesCount = sizeof(kVSRModes) / sizeof(kVSRModes[0]);

static const char* GetVSRModeName(unsigned int mode) {
  for (int i = 0; i < kVSRModesCount; ++i) {
    if (kVSRModes[i].mode == mode) {
      return kVSRModes[i].name;
    }
  }
  return "Unknown";
}

bool FLAG_debug = false, FLAG_verbose = false, FLAG_show = false, FLAG_progress = false, FLAG_webcam = false,
     FLAG_usePinnedMemory = false;
int FLAG_mode = 0, FLAG_logLevel = NVCV_LOG_ERROR;
float FLAG_strength = 1.0f;
std::string FLAG_codec = DEFAULT_CODEC, FLAG_camRes = "1280x720", FLAG_resolution, FLAG_inFile, FLAG_outFile,
            FLAG_outDir, FLAG_modelDir, FLAG_log = "stderr";

// Set this when using OTA Updates
// This path is used by nvVideoEffectsProxy.cpp to load the SDK dll
// when using  OTA Updates
char* g_nvVFXSDKPath = NULL;

static bool GetFlagArgVal(const char* flag, const char* arg, const char** val) {
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

static bool GetFlagArgVal(const char* flag, const char* arg, std::string* val) {
  const char* valStr;
  if (!GetFlagArgVal(flag, arg, &valStr)) return false;
  val->assign(valStr ? valStr : "");
  return true;
}

static bool GetFlagArgVal(const char* flag, const char* arg, bool* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) {
    *val = (valStr == NULL || strcasecmp(valStr, "true") == 0 || strcasecmp(valStr, "on") == 0 ||
            strcasecmp(valStr, "yes") == 0 || strcasecmp(valStr, "1") == 0);
  }
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, float* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success && valStr) *val = strtof(valStr, NULL);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, long* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success && valStr) *val = strtol(valStr, NULL, 10);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, int* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success && valStr) *val = (int)strtol(valStr, NULL, 10);
  return success;
}

static void Usage() {
  printf(
      "VideoSuperResEffectApp [args ...]\n"
      "  where args is:\n"
      "  --in_file=<path>           input file to be processed\n"
      "  --webcam                   use a webcam as the input\n"
      "  --out_file=<path>          output file to be written\n"
      "  --show                     display the results in a window (for webcam, it is always true)\n"
      "  --mode=<value>             for VideoSuperRes: quality level (default: 0)\n"
      "                             (0=VSR_Bicubic, 1=VSR_Low, 2=VSR_Medium, 3=VSR_High, 4=VSR_Ultra,\n"
      "                              8=Denoise_Low, 9=Denoise_Medium, 10=Denoise_High, 11=Denoise_Ultra,\n"
      "                              12=Deblur_Low, 13=Deblur_Medium, 14=Deblur_High, 15=Deblur_Ultra,\n"
      "                              16=HighBitrate_Low, 17=HighBitrate_Medium, 18=HighBitrate_High, "
      "                              19=HighBitrate_Ultra, 21=Streaming_Medium, 23=Streaming_Ultra)\n"
      "  --strength=<value>         for VideoSuperRes: strength in [0.0, 1.0] (default: 1.0)\n"
      "                             8-bit input uses RGBA+U8; 16-bit TIFF/PNG input uses RGB10A2+P32 automatically\n"
      "  --cam_res=[WWWx]HHH        specify camera resolution as height or width x height\n"
      "                             (default \"1280x720\") \n"
      "  --resolution=[WWWx]HHH     specify output resolution as height (e.g., 1080) or width x height (e.g., "
      "1920x1080). Input and output resolution should match in denoise and deblur modes\n"
      "  --model_dir=<path>         the path to the directory that contains the models\n"
      "  --codec=<fourcc>           the fourcc code for the desired codec (default " DEFAULT_CODEC
      ")\n"
      "  --progress                 show progress\n"
      "  --log=<file>               log SDK errors to a file, \"stderr\" or \"\" (default stderr)\n"
      "  --log_level=<N>            the desired log level: {0, 1, 2, 3} = {FATAL, ERROR, WARNING, INFO}, respectively "
      "(default 1)\n"
      "  --use_pinned_memory[=(true|false)]  use NVCV_CPU_PINNED memory for the input and output image.\n"
      "  --verbose                  verbose output\n"
      "  --debug                    print extra debugging information\n");
}

static int ParseMyArgs(int argc, char** argv) {
  int errs = 0;
  for (--argc, ++argv; argc--; ++argv) {
    bool help;
    const char* arg = *argv;
    if (arg[0] != '-') {
      continue;
    } else if ((arg[1] == '-') &&                                                  //
               (GetFlagArgVal("verbose", arg, &FLAG_verbose) ||                    //
                GetFlagArgVal("in", arg, &FLAG_inFile) ||                          //
                GetFlagArgVal("in_file", arg, &FLAG_inFile) ||                     //
                GetFlagArgVal("out", arg, &FLAG_outFile) ||                        //
                GetFlagArgVal("out_file", arg, &FLAG_outFile) ||                   //
                GetFlagArgVal("show", arg, &FLAG_show) ||                          //
                GetFlagArgVal("webcam", arg, &FLAG_webcam) ||                      //
                GetFlagArgVal("cam_res", arg, &FLAG_camRes) ||                     //
                GetFlagArgVal("mode", arg, &FLAG_mode) ||                          //
                GetFlagArgVal("strength", arg, &FLAG_strength) ||                  //
                GetFlagArgVal("resolution", arg, &FLAG_resolution) ||              //
                GetFlagArgVal("model_dir", arg, &FLAG_modelDir) ||                 //
                GetFlagArgVal("codec", arg, &FLAG_codec) ||                        //
                GetFlagArgVal("progress", arg, &FLAG_progress) ||                  //
                GetFlagArgVal("debug", arg, &FLAG_debug) ||                        //
                GetFlagArgVal("use_pinned_memory", arg, &FLAG_usePinnedMemory) ||  //
                GetFlagArgVal("log", arg, &FLAG_log) ||                            //
                GetFlagArgVal("log_level", arg, &FLAG_logLevel))) {
      continue;
    } else if (GetFlagArgVal("help", arg, &help)) {
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

static bool HasSuffix(const char* str, const char* suf) {
  size_t strSize = strlen(str), sufSize = strlen(suf);
  if (strSize < sufSize) return false;
  return (0 == strcasecmp(suf, str + strSize - sufSize));
}

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

static bool IsImageFile(const char* str) {
  return HasOneOfTheseSuffixes(str, ".bmp", ".jpg", ".jpeg", ".png", ".tif", ".tiff", nullptr);
}

static bool IsLossyImageFile(const char* str) { return HasOneOfTheseSuffixes(str, ".jpg", ".jpeg", nullptr); }

// Returns the next VSR mode, skipping denoise/deblur modes if upscaling
static unsigned int GetNextVSRMode(unsigned int currentMode, bool isUpscale) {
  // Find current mode index
  int currentIdx = -1;
  for (int i = 0; i < kVSRModesCount; ++i) {
    if (kVSRModes[i].mode == currentMode) {
      currentIdx = i;
      break;
    }
  }
  int nextIdx = (currentIdx + 1) % kVSRModesCount;
  // Find next valid mode
  for (int i = 0; i < kVSRModesCount; ++i) {
    unsigned int mode = kVSRModes[nextIdx].mode;
    // Skip denoise and deblur modes if upscaling
    bool isDenoiseOrDeblur = (mode >= 8 && mode <= 15);
    if (!isUpscale || !isDenoiseOrDeblur) {
      return mode;
    }
    nextIdx = (nextIdx + 1) % kVSRModesCount;
  }
  // Fallback to Bicubic if no valid mode found
  return 0;
}

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

struct VideoInfo {
  int codec;
  int width;
  int height;
  double frameRate;
  long long frameCount;
};

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

static int StringToFourcc(const std::string& str) {
  union chint {
    int i;
    char c[4];
  };
  chint x = {0};
  for (int n = (str.size() < 4) ? (int)str.size() : 4; n--;) x.c[n] = str[n];
  return x.i;
}

struct FXApp {
  enum Err {
    errQuit = +1,  // Application errors
    errFlag = +2,
    errRead = +3,
    errWrite = +4,
    errNone = NVCV_SUCCESS,  // Video Effects SDK errors
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
    errCudaMemory = NVCV_ERR_CUDA_MEMORY,  // CUDA errors
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

  FXApp() {
    _eff = nullptr;
    _inited = false;
    _showFPS = false;
    _progress = false;
    _show = false;
    _nextModeRequested = false;
    _toggleEffectRequested = false;
    _enableEffect = true, _drawVisualization = true, _framePeriod = 0.f;
    _currentMode = 0;
    _srcGpuBuf = {};
    _dstGpuBuf = {};
    _tmpVFX    = {};
  }
  ~FXApp() {
    NvVFX_DestroyEffect(_eff);
    if (_srcGpuBuf.pixels) NvCVImage_Dealloc(&_srcGpuBuf);
    if (_dstGpuBuf.pixels) NvCVImage_Dealloc(&_dstGpuBuf);
    if (_tmpVFX.pixels)    NvCVImage_Dealloc(&_tmpVFX);
  }

  void setShow(bool show) { _show = show; }
  Err createEffect(const char* modelDir);
  void destroyEffect();
  NvCV_Status allocBuffers(unsigned width, unsigned height);
  NvCV_Status allocTempBuffers();
  Err processImage(const char* inFile, const char* outFile);
  Err processMovie(const char* inFile, const char* outFile);
  Err initCamera(cv::VideoCapture& cap);
  Err processKey(int key);
  void drawFrameRate(cv::Mat& img);
  void drawEffectStatus(cv::Mat& img);
  void drawVSRMode(cv::Mat& img);
  Err appErrFromVfxStatus(NvCV_Status status) { return (Err)status; }
  const char* errorStringFromCode(Err code);

  NvVFX_Handle _eff;
  cv::Mat _srcImg;
  cv::Mat _dstImg;
  NvCVImage _srcGpuBuf;
  NvCVImage _dstGpuBuf;
  NvCVImage _srcVFX;
  NvCVImage _dstVFX;
  NvCVImage _tmpVFX;  // We use the same temporary buffer for source and dst, since it auto-shapes as needed
  bool _show;
  bool _inited;
  bool _showFPS;
  bool _progress;
  bool _enableEffect;
  bool _drawVisualization;
  bool _nextModeRequested;
  bool _toggleEffectRequested;
  float _framePeriod;
  unsigned int _currentMode;
  std::chrono::high_resolution_clock::time_point _lastTime;
};

const char* FXApp::errorStringFromCode(Err code) {
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

// Returns white at the correct scale for the image bit depth (8-bit or 16-bit).
static cv::Scalar TextColor(const cv::Mat& img) {
  double v = (img.depth() == CV_16U) ? 65535.0 : 255.0;
  return cv::Scalar(v, v, v);
}

void FXApp::drawFrameRate(cv::Mat& img) {
  const float timeConstant = 16.f;
  std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> dur = std::chrono::duration_cast<std::chrono::duration<float>>(now - _lastTime);
  float t = dur.count();
  if (0.f < t && t < 100.f) {
    if (_framePeriod)
      _framePeriod += (t - _framePeriod) * (1.f / timeConstant);  // 1 pole IIR filter
    else
      _framePeriod = t;
    if (_showFPS) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%.1f", 1. / _framePeriod);
      cv::putText(img, buf, cv::Point(10, img.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 1, TextColor(img), 1);
    }
  } else {               // Ludicrous time interval; reset
    _framePeriod = 0.f;  // WAKE UP
  }
  _lastTime = now;
}

FXApp::Err FXApp::processKey(int key) {
  static const int ESC_KEY = 27;
  switch (key) {
    case 'Q':
    case 'q':
    case ESC_KEY:
      return errQuit;
    case 'f':
    case 'F':
      _showFPS = !_showFPS;
      break;
    case 'p':
    case 'P':
    case '%':
      _progress = !_progress;
    case 'n':
    case 'N':
      _nextModeRequested = true;
      break;
    case 'e':
    case 'E':
      _toggleEffectRequested = true;
      break;
    case 'd':
    case 'D':
      if (FLAG_webcam) _drawVisualization = !_drawVisualization;
      break;
    default:
      break;
  }
  return errNone;
}

FXApp::Err FXApp::initCamera(cv::VideoCapture& cap) {
  const int camIndex = 0;
#ifdef _WIN32
  cap.open(camIndex, cv::CAP_DSHOW);
#else
  cap.open(camIndex);
#endif
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
    if (camWidth != cap.get(cv::CAP_PROP_FRAME_WIDTH) || camHeight != cap.get(cv::CAP_PROP_FRAME_HEIGHT)) {
      printf("Error: Camera does not support %d x %d resolution\n", camWidth, camHeight);
      return errGeneral;
    }
  }
  return errNone;
}

void FXApp::drawEffectStatus(cv::Mat& img) {
  char buf[32];
  snprintf(buf, sizeof(buf), "Effect: %s", _enableEffect ? "on" : "off");
  cv::putText(img, buf, cv::Point(10, img.rows - 40), cv::FONT_HERSHEY_SIMPLEX, 1, TextColor(img), 1);
}

void FXApp::drawVSRMode(cv::Mat& img) {
  char buf[64];
  snprintf(buf, sizeof(buf), "Mode: %s", GetVSRModeName(_currentMode));
  cv::putText(img, buf, cv::Point(10, 30), cv::FONT_HERSHEY_SIMPLEX, 1, TextColor(img), 1);
}

FXApp::Err FXApp::createEffect(const char* modelDir) {
  NvCV_Status vfxErr;
  BAIL_IF_ERR(vfxErr = NvVFX_CreateEffect(NVVFX_FX_VIDEO_SUPER_RES, &_eff));
bail:
  return appErrFromVfxStatus(vfxErr);
}

void FXApp::destroyEffect() {
  NvVFX_DestroyEffect(_eff);
  _eff = nullptr;
}

// Allocate one temp buffer to be used for input and output. Reshaping of the temp buffer in NvCVImage_Transfer() is
// done automatically, and is very low overhead. We expect the destination to be largest, so we allocate that first to
// minimize reallocs probablistically. Then we Realloc for the source to get the union of the two. This could
// alternately be done at runtime by feeding in an empty temp NvCVImage, but there are advantages to allocating all
// memory at load time.
NvCV_Status FXApp::allocTempBuffers() {
  NvCV_Status vfxErr;
  BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_tmpVFX, _dstVFX.width, _dstVFX.height, _dstVFX.pixelFormat,
                                       _dstVFX.componentType, _dstVFX.planar, NVCV_GPU, 0));
  BAIL_IF_ERR(vfxErr = NvCVImage_Realloc(&_tmpVFX, _srcVFX.width, _srcVFX.height, _srcVFX.pixelFormat,
                                         _srcVFX.componentType, _srcVFX.planar, NVCV_GPU, 0));
bail:
  return vfxErr;
}

// Transfer normalizes BGR cv::Mat to BGR F32 on GPU; RGB10A2 uses scale 1 (fixed integer conversion).
static void GetTransferScales(const NvCVImage* gpuBuf, int srcDepth, float* inputScale, float* outputScale) {
  if (gpuBuf->pixelFormat == NVCV_BGR && gpuBuf->componentType == NVCV_F32) {
    const float maxVal = (srcDepth == CV_16U) ? 65535.f : 255.f;
    *inputScale = 1.f / maxVal;
    *outputScale = maxVal;
  } else {
    *inputScale = 1.f;
    *outputScale = 1.f;
  }
}

NvCV_Status FXApp::allocBuffers(unsigned width, unsigned height) {
  NvCV_Status vfxErr = NVCV_SUCCESS;

  if (_inited) return NVCV_SUCCESS;

  const unsigned io_mem_space = FLAG_usePinnedMemory ? NVCV_CPU_PINNED : NVCV_GPU;
  if (!_srcImg.data) {
    _srcImg.create(height, width, CV_8UC3);  // src CPU
    BAIL_IF_NULL(_srcImg.data, vfxErr, NVCV_ERR_MEMORY);
  }
  if (FLAG_resolution.empty()) {
    printf("--resolution has not been specified\n");
    return NVCV_ERR_PARAMETER;
  }
  int dstWidth, dstHeight;
  if (2 != sscanf(FLAG_resolution.c_str(), "%d%*[xX]%d", &dstWidth, &dstHeight)) {
    dstHeight = std::stoi(FLAG_resolution);
    dstWidth = _srcImg.cols * dstHeight / _srcImg.rows;
  }
  _dstImg.create(dstHeight, dstWidth, _srcImg.type());  // dst CPU
  BAIL_IF_NULL(_dstImg.data, vfxErr, NVCV_ERR_MEMORY);
  if (_srcImg.depth() == CV_16U) {
    if (FLAG_verbose) printf("Info: 16-bit source detected, using 10-bit (RGB10A2) VSR path.\n");
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_RGB10A2, NVCV_P32,
                                         NVCV_CHUNKY, io_mem_space, 32));
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_dstGpuBuf, _dstImg.cols, _dstImg.rows, NVCV_RGB10A2, NVCV_P32,
                                         NVCV_CHUNKY, io_mem_space, 32));
  } else if (_srcImg.depth() == CV_8U) {
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_srcGpuBuf, _srcImg.cols, _srcImg.rows, NVCV_RGBA, NVCV_U8,
                                         NVCV_INTERLEAVED, io_mem_space, 32));
    BAIL_IF_ERR(vfxErr = NvCVImage_Alloc(&_dstGpuBuf, _dstImg.cols, _dstImg.rows, NVCV_RGBA, NVCV_U8,
                                         NVCV_INTERLEAVED, io_mem_space, 32));
  } else {
    printf("Unsupported source depth %d for VideoSuperRes (expected 8- or 16-bit)\n", _srcImg.depth());
    return NVCV_ERR_PIXELFORMAT;
  }
  NVWrapperForCVMat(&_srcImg, &_srcVFX);  // _srcVFX is an alias for _srcImg
  NVWrapperForCVMat(&_dstImg, &_dstVFX);  // _dstVFX is an alias for _dstImg

// #define ALLOC_TEMP_BUFFERS_AT_RUN_TIME    // Deferring temp buffer allocation is easier
#ifndef ALLOC_TEMP_BUFFERS_AT_RUN_TIME       // Allocating temp buffers at load time avoids run time hiccups
  BAIL_IF_ERR(vfxErr = allocTempBuffers());
#endif                                       // ALLOC_TEMP_BUFFERS_AT_RUN_TIME

  _inited = true;

bail:
  return vfxErr;
}

FXApp::Err FXApp::processImage(const char* inFile, const char* outFile) {
  CUstream stream = 0;
  NvCV_Status vfxErr;
  float inputScale, outputScale;
  _currentMode = FLAG_mode;

  if (!_eff) return errEffect;
  // Read with ANYDEPTH so 16-bit TIFF/PNG sources are preserved as CV_16UC3.
  _srcImg = cv::imread(inFile, cv::IMREAD_ANYDEPTH | cv::IMREAD_COLOR);
  if (!_srcImg.data) return errRead;

  BAIL_IF_ERR(vfxErr = allocBuffers(_srcImg.cols, _srcImg.rows));
  GetTransferScales(&_srcGpuBuf, _srcImg.depth(), &inputScale, &outputScale);

  BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(&_srcVFX, &_srcGpuBuf, inputScale, stream, &_tmpVFX));

  vfxErr = NvVFX_SetU32(_eff, NVVFX_IMAGE_ENCODING_MODE,
      (_srcGpuBuf.pixelFormat == NVCV_RGB10A2) ? NVVFX_IMAGE_ENCODING_RGB10A2 : NVVFX_IMAGE_ENCODING_RGB8);
  // Not all features support this selector; silently continue if it is not available.
  if (vfxErr != NVCV_SUCCESS && vfxErr != NVCV_ERR_SELECTOR) goto bail;
  BAIL_IF_ERR(vfxErr = NvVFX_SetImage(_eff, NVVFX_INPUT_IMAGE, &_srcGpuBuf));
  BAIL_IF_ERR(vfxErr = NvVFX_SetImage(_eff, NVVFX_OUTPUT_IMAGE, &_dstGpuBuf));
  BAIL_IF_ERR(vfxErr = NvVFX_SetCudaStream(_eff, NVVFX_CUDA_STREAM, stream));

  vfxErr = NvVFX_SetU32(_eff, NVVFX_QUALITY_LEVEL, (unsigned int)FLAG_mode);
  // Not all features support this selector; silently continue if it is not available.
  if (vfxErr != NVCV_SUCCESS && vfxErr != NVCV_ERR_SELECTOR) goto bail;

  vfxErr = NvVFX_SetF32(_eff, NVVFX_STRENGTH, FLAG_strength);
  if (vfxErr != NVCV_SUCCESS && vfxErr != NVCV_ERR_SELECTOR) goto bail;

  BAIL_IF_ERR(vfxErr = NvVFX_Load(_eff));
  BAIL_IF_ERR(vfxErr = NvVFX_Run(_eff, 0));
  BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(&_dstGpuBuf, &_dstVFX, outputScale, stream, &_tmpVFX));

  if (outFile && outFile[0]) {
    if (IsLossyImageFile(outFile)) {
      if (_dstImg.depth() == CV_16U)
        fprintf(stderr, "WARNING: JPEG does not support 16-bit; output will be silently truncated to 8-bit\n");
      else
        fprintf(stderr, "WARNING: JPEG output file format will reduce image quality\n");
    }
    if (!cv::imwrite(outFile, _dstImg)) {
      printf("Error writing: \"%s\"\n", outFile);
      return errWrite;
    }
  }
  if (_show) {
    drawVSRMode(_dstImg);
    cv::imshow("Output", _dstImg);
    cv::waitKey(3000);
  }
bail:
  return appErrFromVfxStatus(vfxErr);
}

FXApp::Err FXApp::processMovie(const char* inFile, const char* outFile) {
  CUstream stream = 0;
  FXApp::Err appErr = errNone;
  bool ok;
  cv::VideoCapture reader;
  cv::VideoWriter writer;
  NvCV_Status vfxErr;
  unsigned frameNum;
  _currentMode = FLAG_mode;
  VideoInfo info;
  float inputScale, outputScale;
  bool isUpscale = false;
  if (inFile && !inFile[0]) inFile = nullptr;  // Set file paths to NULL if zero length

  if (!FLAG_webcam && inFile) {
    reader.open(inFile);
  } else {
    appErr = initCamera(reader);
    if (appErr != errNone) return appErr;
  }

  if (!reader.isOpened()) {
    if (!FLAG_webcam)
      printf("Error: Could not open video: \"%s\"\n", inFile);
    else
      printf("Error: Webcam not found\n");
    return errRead;
  }

  GetVideoInfo(reader, (inFile ? inFile : "webcam"), &info);

  // NOTE: cv::VideoCapture always decodes frames to 8-bit BGR regardless of source bit
  // depth. The 10-bit VSR path (RGB10A2) is therefore not available for video input.
  // To use 10-bit VSR, supply a 16-bit still image (TIFF or PNG) as input instead.
  BAIL_IF_ERR(vfxErr = allocBuffers(info.width, info.height));

  if (outFile && !outFile[0]) outFile = nullptr;
  if (outFile) {
    // Output is always 8-bit compressed video (default codec: H.264). Even if a 10-bit
    // source were decoded correctly, cv::VideoWriter does not support 10-bit output.
    ok = writer.open(outFile, StringToFourcc(FLAG_codec), info.frameRate, cv::Size(_dstVFX.width, _dstVFX.height));
    if (!ok) {
      printf("Cannot open \"%s\" for video writing\n", outFile);
      outFile = nullptr;
      if (!_show) return errWrite;
    }
  }

  isUpscale = !(_srcVFX.width == _dstVFX.width && _srcVFX.height == _dstVFX.height);
  BAIL_IF_ERR(vfxErr = NvVFX_SetImage(_eff, NVVFX_INPUT_IMAGE, &_srcGpuBuf));
  BAIL_IF_ERR(vfxErr = NvVFX_SetImage(_eff, NVVFX_OUTPUT_IMAGE, &_dstGpuBuf));
  BAIL_IF_ERR(vfxErr = NvVFX_SetCudaStream(_eff, NVVFX_CUDA_STREAM, stream));

  vfxErr = NvVFX_SetU32(_eff, NVVFX_QUALITY_LEVEL, (unsigned int)_currentMode);
  // Not all features support this selector; silently continue if it is not available.
  if (vfxErr != NVCV_SUCCESS && vfxErr != NVCV_ERR_SELECTOR) goto bail;

  vfxErr = NvVFX_SetF32(_eff, NVVFX_STRENGTH, FLAG_strength);
  if (vfxErr != NVCV_SUCCESS && vfxErr != NVCV_ERR_SELECTOR) goto bail;

  BAIL_IF_ERR(vfxErr = NvVFX_Load(_eff));

  GetTransferScales(&_srcGpuBuf, _srcImg.depth(), &inputScale, &outputScale);

  for (frameNum = 0; reader.read(_srcImg); ++frameNum) {
    if (_srcImg.empty()) {
      printf("Frame %u is empty\n", frameNum);
    }

    if (_enableEffect) {
      BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(&_srcVFX, &_srcGpuBuf, inputScale, stream, &_tmpVFX));
      BAIL_IF_ERR(vfxErr = NvVFX_Run(_eff, 0));
      BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(&_dstGpuBuf, &_dstVFX, outputScale, stream, &_tmpVFX));
    } else {
      BAIL_IF_ERR(vfxErr = NvCVImage_Transfer(&_srcVFX, &_dstVFX, 1.f / 255.f, stream, &_tmpVFX));
    }

    if (outFile) writer.write(_dstImg);

    if (_show) {
      drawFrameRate(_dstImg);
      drawVSRMode(_dstImg);
      drawEffectStatus(_dstImg);
      cv::imshow("Output", _dstImg);
      int key = cv::waitKey(1);
      if (key > 0) {
        appErr = processKey(key);
        if (errQuit == appErr) break;
      }
    }
    if (_nextModeRequested) {
      if (_enableEffect) {
        unsigned int newMode = GetNextVSRMode(_currentMode, isUpscale);
        if (newMode != _currentMode) {
          _currentMode = newMode;
          BAIL_IF_ERR(vfxErr = NvVFX_SetU32(_eff, NVVFX_QUALITY_LEVEL, _currentMode));
          fprintf(stderr, "Switched to Video Super Resolution Quality Level: %u (%s)\n", _currentMode,
                  GetVSRModeName(_currentMode));
        }
      } else {
        fprintf(stderr, "Cannot switch quality level while effect is disabled\n");
      }
      _nextModeRequested = false;
    }
    if (_toggleEffectRequested) {
      if (!isUpscale) {
        _enableEffect = !_enableEffect;
        fprintf(stderr, "Effect %s\n", _enableEffect ? "enabled" : "disabled");
      } else {
        fprintf(stderr, "Cannot disable effect while upscaling\n");
      }
      _toggleEffectRequested = false;
    }
    if (_progress) fprintf(stderr, "\b\b\b\b%3.0f%%", 100.f * frameNum / info.frameCount);
  }

  if (_progress) fprintf(stderr, "\n");
  reader.release();
  if (outFile) writer.release();
bail:
  return appErrFromVfxStatus(vfxErr);
}

static int SamplesMain(int argc, char** argv) {
  FXApp::Err fxErr = FXApp::errNone;
  int nErrs;
  FXApp app;

  nErrs = ParseMyArgs(argc, argv);
  if (nErrs == NVCV_ERR_HELP) {
    Usage();
    return 0;
  }
  if (nErrs) std::cerr << nErrs << " command line syntax problems\n";

  NvCV_Status vfxErr = NvVFX_ConfigureLogger(FLAG_logLevel, FLAG_log.c_str(), nullptr, nullptr);
  if (NVCV_SUCCESS != vfxErr)
    printf("%s: while configuring logger to \"%s\"\n", NvCV_GetErrorStringFromCode(vfxErr), FLAG_log.c_str());

  if (FLAG_verbose) {
  }
  if (FLAG_webcam) {
    // If webcam is on, enable showing the results and turn off displaying the progress
    if (FLAG_progress) FLAG_progress = !FLAG_progress;
    if (!FLAG_show) FLAG_show = !FLAG_show;
  }
  if (FLAG_inFile.empty() && !FLAG_webcam) {
    std::cerr << "Please specify --in_file=XXX or --webcam=true\n";
    ++nErrs;
  }
  if (FLAG_outFile.empty() && !FLAG_show) {
    std::cerr << "Please specify --out_file=XXX or --show\n";
    ++nErrs;
  }
  if (!(FLAG_strength >= 0.0f && FLAG_strength <= 1.0f)) {
    std::cerr << "--strength must be in the range [0.0, 1.0]\n";
    ++nErrs;
  }
  app._progress = FLAG_progress;
  app.setShow(FLAG_show);

  if (nErrs) {
    Usage();
    fxErr = FXApp::errFlag;
  } else {
    fxErr = app.createEffect(FLAG_modelDir.c_str());
    if (FXApp::errNone != fxErr) {
      std::cerr << "Error creating VideoSuperRes effect\n";
    } else {
      if (IsImageFile(FLAG_inFile.c_str()))
        fxErr = app.processImage(FLAG_inFile.c_str(), FLAG_outFile.c_str());
      else
        fxErr = app.processMovie(FLAG_inFile.c_str(), FLAG_outFile.c_str());
    }
  }

  if (fxErr) std::cerr << "Error: " << app.errorStringFromCode(fxErr) << std::endl;
  return (int)fxErr;
}

#ifdef _WIN32
int wmain(int argc, wchar_t* wargv[]) {
  std::vector<std::string> u8args(argc);
  std::vector<char*> u8argv(argc);
  for (int i = 0; i < argc; ++i) {
    u8args[i] = WideToUtf8(wargv[i]);
    u8argv[i] = &u8args[i][0];
  }
  return SamplesMain(argc, u8argv.data());
}
#else
int main(int argc, char** argv) {
  return SamplesMain(argc, argv);
}
#endif
