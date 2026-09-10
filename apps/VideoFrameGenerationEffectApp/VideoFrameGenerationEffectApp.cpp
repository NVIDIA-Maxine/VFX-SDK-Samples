/*
 * SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <stdio.h>
#include <string.h>

#include <chrono>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "nvCVOpenCV.h"
#include "nvVFXVideoFrameGeneration.h"
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

static constexpr unsigned kDefaultMultiplier = 2u;

bool FLAG_verbose = false, FLAG_show = false, FLAG_progress = false, FLAG_usePinnedMemory = false;
bool FLAG_automaticShotChangeDetectionEnabled = true;
int FLAG_logLevel = NVCV_LOG_ERROR;
unsigned FLAG_multiplier = kDefaultMultiplier;
bool FLAG_multiplierSet = false;
float FLAG_targetFps = 0.0f;
std::string FLAG_codec = DEFAULT_CODEC, FLAG_inFile, FLAG_outFile, FLAG_log = "stderr", FLAG_mode = "medium";
std::string FLAG_inputFramePrev, FLAG_inputFrameCurrent, FLAG_outputFramePath;
std::vector<float> FLAG_timesteps;

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
  const char* val_str;
  if (!GetFlagArgVal(flag, arg, &val_str)) return false;
  val->assign(val_str ? val_str : "");
  return true;
}

static bool GetFlagArgVal(const char* flag, const char* arg, bool* val) {
  const char* val_str;
  bool success = GetFlagArgVal(flag, arg, &val_str);
  if (success) {
    *val = (val_str == NULL || strcasecmp(val_str, "true") == 0 || strcasecmp(val_str, "on") == 0 ||
            strcasecmp(val_str, "yes") == 0 || strcasecmp(val_str, "1") == 0);
  }
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, int* val) {
  const char* val_str;
  bool success = GetFlagArgVal(flag, arg, &val_str);
  if (success && val_str) *val = (int)strtol(val_str, NULL, 10);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, unsigned* val) {
  const char* val_str;
  bool success = GetFlagArgVal(flag, arg, &val_str);
  if (success && val_str) *val = (unsigned)strtoul(val_str, NULL, 10);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, float* val) {
  const char* val_str;
  bool success = GetFlagArgVal(flag, arg, &val_str);
  if (success && val_str) *val = strtof(val_str, NULL);
  return success;
}

static bool GetFlagArgValAndSplit(const char* flag, const char* arg, std::vector<float>* vals) {
  const char* val_str;
  if (!GetFlagArgVal(flag, arg, &val_str)) return false;
  vals->clear();
  if (!val_str) return true;

  std::istringstream iss(val_str);
  std::string part;
  size_t index = 0;
  while (std::getline(iss, part, ',')) {
    ++index;
    const size_t start = part.find_first_not_of(" \t");
    if (start == std::string::npos) continue;  // empty or whitespace-only token
    const size_t finish = part.find_last_not_of(" \t");
    const std::string token = part.substr(start, finish - start + 1);

    char* end = nullptr;
    const float v = strtof(token.c_str(), &end);
    if (end != token.c_str() + token.size()) {
      printf("Error: --%s value #%zu (\"%s\") is not a number\n", flag, index, token.c_str());
      return false;
    }
    vals->push_back(v);
  }
  return true;
}

static void Usage() {
  printf(
      "VideoFrameGenerationEffectApp [args ...]\n"
      "  where args is:\n"
      "  --in_file=<path>                          input video file to be processed\n"
      "  --out_file=<path>                         output video file to be written\n"
      "  --input_frame_prev=<path>                 previous 16-bit PNG/TIFF input frame (packed RGB10A2 image)\n"
      "                                            When this path is enabled, input video files should not be provided\n"
      "  --input_frame_current=<path>              current 16-bit PNG/TIFF input frame (packed RGB10A2 image mode)\n"
      "  --output_frame_path=<path>                output PNG/TIFF path for generated RGB10A2 image(s). With\n"
      "                                            --multiplier=N, writes N-1 intermediates; multiple outputs\n"
      "                                            are suffixed _1, _2, ... before the extension.\n"
      "  --show                                    display the results in a window\n"
      "  --multiplier=<N>                          multiply the input frame rate by N (default 2). For each\n"
      "                                            pair of consecutive input frames the SDK inserts N-1 frames\n"
      "                                            evenly spaced between them; e.g., N=4 inserts 3 frames per\n"
      "                                            pair, turning a 30 fps input into a 120 fps output.\n"
      "                                            Out-of-range values are rejected with an error listing the\n"
      "                                            allowed range. Use either --multiplier or --timesteps,\n"
      "                                            not both.\n"
      "  --timesteps=t1,t2,...                     generate frames at the listed positions between each input\n"
      "                                            pair instead of evenly. Each value is in (0.0, 1.0) where\n"
      "                                            0 is the previous input frame and 1 is the current input\n"
      "                                            frame (e.g., 0.5 = halfway, 0.25 = a quarter of the way).\n"
      "                                            Useful for non-uniform spacing. Use either --multiplier or\n"
      "                                            --timesteps, not both.\n"
      "  --target_fps=<float>                      fps written to the output video. Defaults to the input fps\n"
      "                                            scaled by the number of frames generated per input pair, i.e.\n"
      "                                            input_fps * multiplier in multiplier mode, or\n"
      "                                            input_fps * (1 + number_of_timesteps) in timestep mode. \n"
      "                                            Setting a lower value slows playback.\n"
      "                                            (e.g. 24 fps source + --multiplier=4 +\n"
      "                                            --target_fps=24 yields 4x slow motion at 24 fps).\n"
      "  --automatic_shot_change_detection=<bool>\n"
      "                                            enable automatic shot-change detection (default true)\n"
      "  --mode=(low|medium|high)                  mode to use (default medium)\n"
      "  --codec=<fourcc>                          the fourcc code for the desired codec (default " DEFAULT_CODEC
      ")\n"
      "  --progress                                show progress\n"
      "  --log=<file>                              log SDK errors to a file, \"stderr\" or \"\" (default stderr)\n"
      "  --log_level=<N>                           the desired log level: {0, 1, 2, 3} = {FATAL, ERROR, WARNING, INFO} "
      "(default 1)\n"
      "  --use_pinned_memory[=(true|false)]        use NVCV_CPU_PINNED memory for the input and output image.\n"
      "  --verbose                                 verbose output\n");
}

static int ParseMyArgs(int argc, char** argv) {
  int errs = 0;
  for (--argc, ++argv; argc--; ++argv) {
    bool help;
    const char* arg = *argv;
    if (arg[0] != '-') {
      continue;
    } else if ((arg[1] == '-') && GetFlagArgVal("multiplier", arg, &FLAG_multiplier)) {
      FLAG_multiplierSet = true;
      continue;
    } else if ((arg[1] == '-') &&                                                  //
                (GetFlagArgVal("verbose", arg, &FLAG_verbose) ||                    //
                GetFlagArgVal("in", arg, &FLAG_inFile) ||                          //
                GetFlagArgVal("in_file", arg, &FLAG_inFile) ||                     //
                GetFlagArgVal("out", arg, &FLAG_outFile) ||                        //
                GetFlagArgVal("out_file", arg, &FLAG_outFile) ||                   //
                GetFlagArgVal("input_frame_prev", arg, &FLAG_inputFramePrev) ||    //
                GetFlagArgVal("input_frame_current", arg, &FLAG_inputFrameCurrent) ||  //
                GetFlagArgVal("output_frame_path", arg, &FLAG_outputFramePath) ||  //
                GetFlagArgVal("show", arg, &FLAG_show) ||                          //
                GetFlagArgVal("automatic_shot_change_detection", arg,
                              &FLAG_automaticShotChangeDetectionEnabled) ||         //
                GetFlagArgValAndSplit("timesteps", arg, &FLAG_timesteps) ||        //
                GetFlagArgVal("target_fps", arg, &FLAG_targetFps) ||               //
                GetFlagArgVal("mode", arg, &FLAG_mode) ||                          //
                GetFlagArgVal("codec", arg, &FLAG_codec) ||                        //
                GetFlagArgVal("progress", arg, &FLAG_progress) ||                  //
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

static void GetVideoInfo(cv::VideoCapture& reader, const char* file_name, VideoInfo* info) {
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
        file_name, (char*)&info->codec, info->width, info->height, info->frameRate, info->frameCount,
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

static bool IsRgb10ImageOutputPath(const std::string& path) {
  const size_t slash = path.find_last_of("\\\\/");
  const size_t dot = path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return false;
  const char* extension = path.c_str() + dot;
  return strcasecmp(extension, ".png") == 0 || strcasecmp(extension, ".tif") == 0 ||
         strcasecmp(extension, ".tiff") == 0;
}

static std::string OutputFramePath(const std::string& output_path, unsigned frame_index, unsigned frame_count) {
  if (frame_count == 1u) return output_path;

  const size_t slash = output_path.find_last_of("\\\\/");
  const size_t dot = output_path.find_last_of('.');
  if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) {
    return output_path + "_" + std::to_string(frame_index);
  }
  return output_path.substr(0, dot) + "_" + std::to_string(frame_index) + output_path.substr(dot);
}

struct VideoFrameGenerationApp {
  enum Err {
    errQuit = +1,
    errFlag = +2,
    errRead = NVCV_ERR_READ,
    errWrite = NVCV_ERR_WRITE,
    errNone = NVCV_SUCCESS,
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
    errCudaMemory = NVCV_ERR_CUDA_MEMORY,
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

  VideoFrameGenerationApp() {
    m_eff = nullptr;
    m_inited = false;
    m_showFPS = false;
    m_progress = false;
    m_show = false;
    m_enableEffect = true;
    m_framePeriod = 0.f;
  }
  ~VideoFrameGenerationApp() { NvVFX_DestroyEffect(m_eff); }

  void setShow(bool show) { m_show = show; }
  Err CreateEffect();
  void DestroyEffect();
  NvCV_Status AllocBuffers(unsigned width, unsigned height);
  NvCV_Status AllocImagePairBuffers(unsigned width, unsigned height);
  NvCV_Status AllocTempBuffers();
  Err ProcessMovie(const char* in_file, const char* out_file);
  Err ProcessImagePair(const char* prev_frame_path, const char* curr_frame_path, const char* output_frame_path);
  Err ProcessKey(int key);
  void DrawFrameRate(cv::Mat& img);
  Err AppErrFromVfxStatus(NvCV_Status status) { return (Err)status; }
  const char* ErrorStringFromCode(Err code);

  NvVFX_Handle m_eff;
  cv::Mat m_prevImg;
  cv::Mat m_currImg;
  cv::Mat m_dstImg;
  NvCVImage m_prevGpuBuf;
  NvCVImage m_currGpuBuf;
  NvCVImage m_dstGpuBuf;
  NvCVImage m_prevVFX;
  NvCVImage m_currVFX;
  NvCVImage m_dstVFX;
  NvCVImage m_tmpVFX;
  bool m_show;
  bool m_inited;
  bool m_showFPS;
  bool m_progress;
  bool m_enableEffect;
  float m_framePeriod;
  std::chrono::high_resolution_clock::time_point m_lastTime;
};

const char* VideoFrameGenerationApp::ErrorStringFromCode(Err code) {
  struct LutEntry {
    Err code;
    const char* str;
  };
  static const LutEntry lut[] = {
      {errQuit, "The user chose to quit the application"},
      {errFlag, "There was a problem with the command-line arguments"},
  };
  if ((int)code <= 0) return NvCV_GetErrorStringFromCode((NvCV_Status)code);
  for (const LutEntry* p = lut; p != &lut[sizeof(lut) / sizeof(lut[0])]; ++p)
    if (p->code == code) return p->str;
  return "UNKNOWN ERROR";
}

void VideoFrameGenerationApp::DrawFrameRate(cv::Mat& img) {
  const float timeConstant = 16.f;
  std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> dur = std::chrono::duration_cast<std::chrono::duration<float>>(now - m_lastTime);
  float t = dur.count();
  if (0.f < t && t < 100.f) {
    if (m_framePeriod)
      m_framePeriod += (t - m_framePeriod) * (1.f / timeConstant);
    else
      m_framePeriod = t;
    if (m_showFPS) {
      char buf[32];
      snprintf(buf, sizeof(buf), "%.1f", 1. / m_framePeriod);
      cv::putText(img, buf, cv::Point(10, img.rows - 10), cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 1);
    }
  } else {
    m_framePeriod = 0.f;
  }
  m_lastTime = now;
}

VideoFrameGenerationApp::Err VideoFrameGenerationApp::ProcessKey(int key) {
  static const int ESC_KEY = 27;
  switch (key) {
    case 'Q':
    case 'q':
    case ESC_KEY:
      return errQuit;
    case 'f':
    case 'F':
      m_showFPS = !m_showFPS;
      break;
    case 'p':
    case 'P':
    case '%':
      m_progress = !m_progress;
      break;
    case 'e':
    case 'E':
      m_enableEffect = !m_enableEffect;
      fprintf(stderr, "Effect %s\n", m_enableEffect ? "enabled" : "disabled");
      break;
    default:
      break;
  }
  return errNone;
}

VideoFrameGenerationApp::Err VideoFrameGenerationApp::CreateEffect() {
  NvCV_Status vfx_err;

  BAIL_IF_ERR(vfx_err = NvVFX_CreateEffect(NVVFX_FX_VIDEO_FRAME_GENERATION, &m_eff));

bail:
  return AppErrFromVfxStatus(vfx_err);
}

void VideoFrameGenerationApp::DestroyEffect() {
  NvVFX_DestroyEffect(m_eff);
  m_eff = nullptr;
}

NvCV_Status VideoFrameGenerationApp::AllocTempBuffers() {
  NvCV_Status vfx_err;
  BAIL_IF_ERR(vfx_err = NvCVImage_Alloc(&m_tmpVFX, m_dstVFX.width, m_dstVFX.height, m_dstVFX.pixelFormat,
                                        m_dstVFX.componentType, m_dstVFX.planar, NVCV_GPU, 0));
  BAIL_IF_ERR(vfx_err = NvCVImage_Realloc(&m_tmpVFX, m_prevVFX.width, m_prevVFX.height, m_prevVFX.pixelFormat,
                                          m_prevVFX.componentType, m_prevVFX.planar, NVCV_GPU, 0));
bail:
  return vfx_err;
}

NvCV_Status VideoFrameGenerationApp::AllocBuffers(unsigned width, unsigned height) {
  NvCV_Status vfx_err = NVCV_SUCCESS;

  if (m_inited) return NVCV_SUCCESS;

  const unsigned io_mem_space = FLAG_usePinnedMemory ? NVCV_CPU_PINNED : NVCV_GPU;

  m_prevImg.create(height, width, CV_8UC3);
  BAIL_IF_NULL(m_prevImg.data, vfx_err, NVCV_ERR_MEMORY);

  m_currImg.create(height, width, CV_8UC3);
  BAIL_IF_NULL(m_currImg.data, vfx_err, NVCV_ERR_MEMORY);

  m_dstImg.create(height, width, CV_8UC3);
  BAIL_IF_NULL(m_dstImg.data, vfx_err, NVCV_ERR_MEMORY);

  BAIL_IF_ERR(
      vfx_err = NvCVImage_Alloc(&m_prevGpuBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, io_mem_space, 32));
  BAIL_IF_ERR(
      vfx_err = NvCVImage_Alloc(&m_currGpuBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, io_mem_space, 32));
  BAIL_IF_ERR(vfx_err =
                  NvCVImage_Alloc(&m_dstGpuBuf, width, height, NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, io_mem_space, 32));

  NVWrapperForCVMat(&m_prevImg, &m_prevVFX);
  NVWrapperForCVMat(&m_currImg, &m_currVFX);
  NVWrapperForCVMat(&m_dstImg, &m_dstVFX);

#ifndef ALLOC_TEMP_BUFFERS_AT_RUN_TIME
  BAIL_IF_ERR(vfx_err = AllocTempBuffers());
#endif

  m_inited = true;

bail:
  return vfx_err;
}

NvCV_Status VideoFrameGenerationApp::AllocImagePairBuffers(unsigned width, unsigned height) {
  NvCV_Status vfx_err = NVCV_SUCCESS;

  if (m_inited) return NVCV_SUCCESS;

  const unsigned io_mem_space = FLAG_usePinnedMemory ? NVCV_CPU_PINNED : NVCV_GPU;

  m_prevImg.create(height, width, CV_16UC3);
  BAIL_IF_NULL(m_prevImg.data, vfx_err, NVCV_ERR_MEMORY);
  m_currImg.create(height, width, CV_16UC3);
  BAIL_IF_NULL(m_currImg.data, vfx_err, NVCV_ERR_MEMORY);
  m_dstImg.create(height, width, CV_16UC3);
  BAIL_IF_NULL(m_dstImg.data, vfx_err, NVCV_ERR_MEMORY);


  BAIL_IF_ERR(vfx_err =
                  NvCVImage_Alloc(&m_prevGpuBuf, width, height, NVCV_RGB10A2, NVCV_P32, NVCV_CHUNKY, io_mem_space, 32));
  BAIL_IF_ERR(vfx_err =
                  NvCVImage_Alloc(&m_currGpuBuf, width, height, NVCV_RGB10A2, NVCV_P32, NVCV_CHUNKY, io_mem_space, 32));
  BAIL_IF_ERR(vfx_err =
                  NvCVImage_Alloc(&m_dstGpuBuf, width, height, NVCV_RGB10A2, NVCV_P32, NVCV_CHUNKY, io_mem_space, 32));

  NVWrapperForCVMat(&m_prevImg, &m_prevVFX);
  NVWrapperForCVMat(&m_currImg, &m_currVFX);
  NVWrapperForCVMat(&m_dstImg, &m_dstVFX);

#ifndef ALLOC_TEMP_BUFFERS_AT_RUN_TIME
  BAIL_IF_ERR(vfx_err = AllocTempBuffers());
#endif

  m_inited = true;

bail:
  return vfx_err;
}

VideoFrameGenerationApp::Err VideoFrameGenerationApp::ProcessMovie(const char* in_file, const char* out_file) {
  CUstream stream = 0;
  VideoFrameGenerationApp::Err app_err = errNone;
  bool ok = false;
  cv::VideoCapture reader;
  cv::VideoWriter writer;
  NvCV_Status vfx_err = NVCV_SUCCESS;
  unsigned frame_num = 0;
  VideoInfo info{};
  double writer_fps = 0.0;

  // Two ways to drive Video Frame Generation:
  //   - Timestep mode    (--timesteps=t0,t1,...): caller supplies explicit t-values in (0, 1).
  //   - Multiplier mode  (default):               emit M-1 uniformly spaced intermediates per pair,
  //                                               where M = FLAG_multiplier.
  const bool timestep_mode = !FLAG_timesteps.empty();

  // Number of frames the feature generates between each (prev, curr) input pair.
  unsigned num_intermediates = 0;
  if (timestep_mode) {
    num_intermediates = static_cast<unsigned>(FLAG_timesteps.size());
  } else if (FLAG_multiplier > 0u) {
    num_intermediates = FLAG_multiplier - 1u;  // e.g. M=4 -> 3 intermediates per pair
  }

  // Total frames written per pair = generated intermediates + the original curr frame
  // (curr is written once after all intermediates to close out the pair).
  const unsigned frames_per_pair = num_intermediates + 1u;

  if (in_file && !in_file[0]) in_file = nullptr;

  if (in_file) {
    reader.open(in_file);
  }

  if (!reader.isOpened()) {
    printf("Error: Could not open video: \"%s\"\n", in_file ? in_file : "(null)");
    return errRead;
  }

  GetVideoInfo(reader, in_file, &info);

  BAIL_IF_ERR(vfx_err = AllocBuffers(info.width, info.height));

  // Pick the writer FPS: explicit --target_fps if set, else input_fps * frames_per_pair.
  writer_fps = (FLAG_targetFps > 0.0f) ? (double)FLAG_targetFps : info.frameRate * (double)frames_per_pair;

  if (out_file && !out_file[0]) out_file = nullptr;
  if (out_file) {
    ok = writer.open(out_file, StringToFourcc(FLAG_codec), writer_fps, cv::Size(m_dstVFX.width, m_dstVFX.height));
    if (!ok) {
      printf("Cannot open \"%s\" for video writing\n", out_file);
      out_file = nullptr;
      if (!m_show) return errWrite;
    }
  }

  if (FLAG_verbose && out_file) {
    printf(
        "\n[Output]\n"
        "       file \"%s\"\n"
        "        fps %.3f\n",
        out_file, writer_fps);
    if (info.frameCount > 0) {
      const long long est_frames = 1 + (info.frameCount - 1) * static_cast<long long>(frames_per_pair);
      printf(
          "frame count %4lld\n"
          "   duration %s\n",
          est_frames, DurationString(static_cast<double>(est_frames) / writer_fps));
    }
  }

  BAIL_IF_ERR(vfx_err = NvVFX_SetCudaStream(m_eff, NVVFX_CUDA_STREAM, stream));
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFX_INPUT_WIDTH, static_cast<unsigned>(info.width)));
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFX_INPUT_HEIGHT, static_cast<unsigned>(info.height)));
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_MODE,
                                     (FLAG_mode == "low") ? NVVFXVIDEOFRAMEGENERATION_MODE_LOW
                                                          : (FLAG_mode == "medium") ? NVVFXVIDEOFRAMEGENERATION_MODE_MEDIUM
                                                                                    : NVVFXVIDEOFRAMEGENERATION_MODE_HIGH));
  // Disable automatic shot change detection if needed (enabled by default)
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_AUTOMATIC_SHOT_CHANGE_DETECTION_ENABLED,
                                     FLAG_automaticShotChangeDetectionEnabled ? 1u : 0u));
  BAIL_IF_ERR(vfx_err = NvVFX_Load(m_eff));
  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_OUTPUT_IMAGE, &m_dstGpuBuf));

  if (!timestep_mode) {
    BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_FRAME_MULTIPLIER, FLAG_multiplier));
  } else {
    // Timestep mode: set multiplier to 0 to tell the SDK we are supplying explicit timesteps.
    BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_FRAME_MULTIPLIER, 0u));
  }

  // Read the first frame into m_prevImg
  if (!reader.read(m_prevImg) || m_prevImg.empty()) {
    printf("Error: Could not read first frame\n");
    return errRead;
  }

  // Write the first original frame
  if (out_file) writer.write(m_prevImg);

  for (frame_num = 1; reader.read(m_currImg); ++frame_num) {
    if (m_currImg.empty()) {
      printf("Frame %u is empty\n", frame_num);
      continue;
    }

    if (m_enableEffect) {
      // Upload previous and current frames to GPU
      BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&m_prevVFX, &m_prevGpuBuf, 1.f, stream, &m_tmpVFX));
      BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&m_currVFX, &m_currGpuBuf, 1.f, stream, &m_tmpVFX));
      // IMPORTANT: Video Frame Generation requires SetImage to be called on BOTH inputs
      // at the start of every new (prev, curr) pair, even when the GPU buffer pointers are
      // unchanged from the previous pair.
      BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_INPUT_IMAGE_0, &m_prevGpuBuf));
      BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_INPUT_IMAGE_1, &m_currGpuBuf));
      BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_SHOT_CHANGE, 0u));
      // Emit num_intermediates generated frames per (prev, curr) pair.
      for (unsigned k = 0; k < num_intermediates; ++k) {
        if (timestep_mode) {
          // To switch from multiplier mode to timestep mode, set NVVFXVIDEOFRAMEGENERATION_FRAME_MULTIPLIER to 0 first.
          BAIL_IF_ERR(vfx_err = NvVFX_SetF32(m_eff, NVVFXVIDEOFRAMEGENERATION_TIMESTEP, FLAG_timesteps[k]));
        } else {
          BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_FRAME_INDEX, k + 1u));
        }
        BAIL_IF_ERR(vfx_err = NvVFX_Run(m_eff, 0));
        BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&m_dstGpuBuf, &m_dstVFX, 1.f, stream, &m_tmpVFX));

        if (out_file) writer.write(m_dstImg);
        if (m_show) {
          DrawFrameRate(m_dstImg);
          cv::imshow("Generated Frame", m_dstImg);
          int key = cv::waitKey(1);
          if (key > 0) {
            app_err = ProcessKey(key);
            if (errQuit == app_err) break;  // exit inner loop; outer break propagates below
          }
        }
      }
      if (errQuit == app_err) break;
      // After all intermediates, emit the original curr frame.
      if (out_file) writer.write(m_currImg);
      if (m_show) cv::imshow("Original", m_currImg);
    } else {
      // Effect disabled: just write the current frame (no interpolation)
      if (out_file) writer.write(m_currImg);

      if (m_show) {
        DrawFrameRate(m_currImg);
        cv::imshow("Original", m_currImg);
      }
    }

    if (m_show) {
      int key = cv::waitKey(1);
      if (key > 0) {
        app_err = ProcessKey(key);
        if (errQuit == app_err) break;
      }
    }

    // Current frame becomes previous frame for the next iteration
    std::swap(m_prevImg, m_currImg);
    NVWrapperForCVMat(&m_prevImg, &m_prevVFX);
    NVWrapperForCVMat(&m_currImg, &m_currVFX);

    if (m_progress) fprintf(stderr, "\b\b\b\b%3.0f%%", 100.f * frame_num / info.frameCount);
  }

  if (m_progress) fprintf(stderr, "\n");
  reader.release();
  if (out_file) writer.release();
bail:
  return AppErrFromVfxStatus(vfx_err);
}

VideoFrameGenerationApp::Err VideoFrameGenerationApp::ProcessImagePair(const char* prev_frame_path,
                                                                         const char* curr_frame_path,
                                                                         const char* output_frame_path) {
  CUstream stream = 0;
  NvCV_Status vfx_err = NVCV_SUCCESS;
  const bool timestep_mode = !FLAG_timesteps.empty();
  const unsigned num_intermediates =
      timestep_mode ? static_cast<unsigned>(FLAG_timesteps.size()) : (FLAG_multiplier > 0u ? FLAG_multiplier - 1u : 0);

  const cv::Mat prev_input = cv::imread(prev_frame_path, cv::IMREAD_ANYDEPTH | cv::IMREAD_COLOR);
  const cv::Mat curr_input = cv::imread(curr_frame_path, cv::IMREAD_ANYDEPTH | cv::IMREAD_COLOR);
  if (prev_input.empty() || curr_input.empty()) {
    printf("Error: Could not read 16-bit input frame pair\n");
    return errRead;
  }
  if (prev_input.type() != CV_16UC3 || curr_input.type() != CV_16UC3) {
    printf("Error: --input_frame_prev and --input_frame_current must be 16-bit, 3-channel images\n");
    return errPixelFormat;
  }
  if (prev_input.size() != curr_input.size()) {
    printf("Error: Input image dimensions must match\n");
    return errMismatch;
  }
  if (num_intermediates == 0u) {
    printf("Error: Image-pair mode requires at least one generated frame\n");
    return errParameter;
  }

  BAIL_IF_ERR(vfx_err = AllocImagePairBuffers(static_cast<unsigned>(prev_input.cols),
                                               static_cast<unsigned>(prev_input.rows)));
  prev_input.copyTo(m_prevImg);
  curr_input.copyTo(m_currImg);

  BAIL_IF_ERR(vfx_err = NvVFX_SetCudaStream(m_eff, NVVFX_CUDA_STREAM, stream));
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFX_INPUT_WIDTH, static_cast<unsigned>(m_prevImg.cols)));
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFX_INPUT_HEIGHT, static_cast<unsigned>(m_prevImg.rows)));
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_MODE,
                                     (FLAG_mode == "low") ? NVVFXVIDEOFRAMEGENERATION_MODE_LOW
                                                          : (FLAG_mode == "medium") ? NVVFXVIDEOFRAMEGENERATION_MODE_MEDIUM
                                                                                    : NVVFXVIDEOFRAMEGENERATION_MODE_HIGH));
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_AUTOMATIC_SHOT_CHANGE_DETECTION_ENABLED,
                                     FLAG_automaticShotChangeDetectionEnabled ? 1u : 0u));
  BAIL_IF_ERR(vfx_err = NvVFX_Load(m_eff));
  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_OUTPUT_IMAGE, &m_dstGpuBuf));
  if (timestep_mode) {
    BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_FRAME_MULTIPLIER, 0u));
  } else {
    BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_FRAME_MULTIPLIER, FLAG_multiplier));
  }

  // RGB10A2 uses fixed-integer transfer, so no normalization scale is applied.
  BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&m_prevVFX, &m_prevGpuBuf, 1.f, stream, &m_tmpVFX));
  BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&m_currVFX, &m_currGpuBuf, 1.f, stream, &m_tmpVFX));
  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_INPUT_IMAGE_0, &m_prevGpuBuf));
  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_INPUT_IMAGE_1, &m_currGpuBuf));
  BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_SHOT_CHANGE, 0u));

  for (unsigned k = 0; k < num_intermediates; ++k) {
    if (timestep_mode) {
      BAIL_IF_ERR(vfx_err = NvVFX_SetF32(m_eff, NVVFXVIDEOFRAMEGENERATION_TIMESTEP, FLAG_timesteps[k]));
    } else {
      BAIL_IF_ERR(vfx_err = NvVFX_SetU32(m_eff, NVVFXVIDEOFRAMEGENERATION_FRAME_INDEX, k + 1u));
    }
    BAIL_IF_ERR(vfx_err = NvVFX_Run(m_eff, 0));
    BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&m_dstGpuBuf, &m_dstVFX, 1.f, stream, &m_tmpVFX));

    const std::string frame_path = OutputFramePath(output_frame_path, k + 1u, num_intermediates);
    if (!cv::imwrite(frame_path, m_dstImg)) {
      printf("Error writing output image: \"%s\"\n", frame_path.c_str());
      return errWrite;
    }
    if (FLAG_verbose) printf("Wrote generated frame: \"%s\"\n", frame_path.c_str());
    if (m_show) {
      cv::imshow("Generated Frame", m_dstImg);
      cv::waitKey(1);
    }
  }

bail:
  return AppErrFromVfxStatus(vfx_err);
}

int main(int argc, char** argv) {
  VideoFrameGenerationApp::Err fx_err = VideoFrameGenerationApp::errNone;
  int n_errs;
  VideoFrameGenerationApp app;

  n_errs = ParseMyArgs(argc, argv);
  if (n_errs == NVCV_ERR_HELP) {
    Usage();
    return 0;
  }
  if (n_errs) std::cerr << n_errs << " command line syntax problems\n";

  NvCV_Status vfx_err = NvVFX_ConfigureLogger(FLAG_logLevel, FLAG_log.c_str(), nullptr, nullptr);
  if (NVCV_SUCCESS != vfx_err)
    printf("%s: while configuring logger to \"%s\"\n", NvCV_GetErrorStringFromCode(vfx_err), FLAG_log.c_str());

  const bool image_pair_supplied = !FLAG_inputFramePrev.empty() || !FLAG_inputFrameCurrent.empty() || !FLAG_outputFramePath.empty();
  if (image_pair_supplied) {
    if (FLAG_inputFramePrev.empty() || FLAG_inputFrameCurrent.empty() || FLAG_outputFramePath.empty()) {
      std::cerr << "Please specify --input_frame_prev, --input_frame_current and --output_frame_path together\n";
      ++n_errs;
    }
    if (!FLAG_inFile.empty() || !FLAG_outFile.empty()) {
      std::cerr << "Image-pair mode cannot be combined with --in_file or --out_file\n";
      ++n_errs;
    }
    if (!FLAG_outputFramePath.empty() && !IsRgb10ImageOutputPath(FLAG_outputFramePath)) {
      std::cerr << "--output_frame_path must use a 16-bit-capable PNG or TIFF extension\n";
      ++n_errs;
    }
  } else {
    if (FLAG_inFile.empty()) {
      std::cerr << "Please specify --in_file=XXX\n";
      ++n_errs;
    }
    if (FLAG_outFile.empty() && !FLAG_show) {
      std::cerr << "Please specify --out_file=XXX or --show\n";
      ++n_errs;
    }
  }
  if (FLAG_multiplierSet && !FLAG_timesteps.empty()) {
    std::cerr << "Please use either --multiplier or --timesteps, not both.\n";
    ++n_errs;
  }
  if (FLAG_multiplierSet && FLAG_multiplier == 0u && FLAG_timesteps.empty()) {
    std::cerr << "Please specify a non-zero --multiplier (or use --timesteps instead).\n";
    ++n_errs;
  }

  for (size_t i = 0; i < FLAG_timesteps.size(); ++i) {
    if (!(FLAG_timesteps[i] > 0.0f && FLAG_timesteps[i] < 1.0f)) {
      std::cerr << "--timesteps value #" << (i + 1) << " (" << FLAG_timesteps[i]
                << ") must be in the open interval (0.0, 1.0)\n";
      ++n_errs;
    }
  }
  if (FLAG_targetFps < 0.0f) {
    std::cerr << "--target_fps must be non-negative (got " << FLAG_targetFps << ")\n";
    ++n_errs;
  }
  if (FLAG_mode != "low" && FLAG_mode != "medium" && FLAG_mode != "high") {
    std::cerr << "--mode must be low, medium or high (got " << FLAG_mode << ")\n";
    ++n_errs;
  }
  app.m_progress = FLAG_progress;
  app.setShow(FLAG_show);

  if (n_errs) {
    Usage();
    fx_err = VideoFrameGenerationApp::errFlag;
  } else {
    fx_err = app.CreateEffect();
    if (VideoFrameGenerationApp::errNone != fx_err) {
      std::cerr << "Error creating VideoFrameGeneration effect\n";
    } else {
      fx_err = image_pair_supplied
                   ? app.ProcessImagePair(FLAG_inputFramePrev.c_str(), FLAG_inputFrameCurrent.c_str(),
                                          FLAG_outputFramePath.c_str())
                   : app.ProcessMovie(FLAG_inFile.c_str(), FLAG_outFile.c_str());
    }
  }

  if (fx_err) std::cerr << "Error: " << app.ErrorStringFromCode(fx_err) << std::endl;
  return (int)fx_err;
}
