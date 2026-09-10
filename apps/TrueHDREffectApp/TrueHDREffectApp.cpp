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

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "nvCVOpenCV.h"
#include "nvVFXTrueHDR.h"
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

bool FLAG_verbose = false;
bool FLAG_progress = false;
bool FLAG_usePinnedMemory = false;
int FLAG_logLevel = NVCV_LOG_ERROR;
std::string FLAG_inFile;
std::string FLAG_outFile;
std::string FLAG_log = "stderr";
std::string FLAG_inRawDims;

// HDR tunables
uint32_t FLAG_hdrContrast = 100;
uint32_t FLAG_hdrSaturation = 100;
uint32_t FLAG_hdrMiddleGray = 50;
uint32_t FLAG_hdrMaxLuminance = 650;
uint32_t FLAG_hdrDebandingOff = 0;

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

static bool GetFlagArgVal(const char* flag, const char* arg, uint32_t* val) {
  const char* val_str;
  bool success = GetFlagArgVal(flag, arg, &val_str);
  if (success && val_str) *val = (uint32_t)strtoul(val_str, NULL, 10);
  return success;
}

static void Usage() {
  printf(
      "TrueHDREffectApp [args ...]\n"
      "  where args is:\n"
      "  --in_file=<path>           input 8-bit SDR file to be processed (image, any video container\n"
      "                             OpenCV can decode, or - with --in_raw_rgba8 - a concatenated raw\n"
      "                             RGBA8 byte stream)\n"
      "  --out_file=<path>          path (including extension) for the headerless raw HDR10 output;\n"
      "                             written verbatim. The stream is packed RGB10A2 (4 bytes/pixel,\n"
      "                             no row padding, no container header). Dimensions match --in_file.\n"
      "  --in_raw_rgba8=<W>x<H>     interpret --in_file as a raw RGBA8 byte stream of <W>x<H>\n"
      "                             interleaved frames (4 bytes/pixel, no header, frame count\n"
      "                             inferred from file size). Bypasses OpenCV image/video decode.\n"
      "  --use_pinned_memory[=(true|false)]  use NVCV_CPU_PINNED memory for the input and output\n"
      "                             GPU buffers. With pinned-host output the raw HDR10 frames\n"
      "                             are fwrite'd directly from the (host-visible) snippet target,\n"
      "                             skipping one GPU->host transfer per frame.\n"
      "  --progress                 print percent-complete progress to stderr\n"
      "  --log=<file>               log SDK errors to a file, \"stderr\" or \"\" (default stderr)\n"
      "  --log_level=<N>            log level: 0=FATAL, 1=ERROR, 2=WARNING, 3=INFO (default 1)\n"
      "  --verbose                  verbose output\n"
      "\n"
      "HDR tunables (per-Run, applied before NvVFX_Load on the effect handle):\n"
      "  --hdr_contrast=<N>         HDR contrast,        range 0..200,     default 100\n"
      "  --hdr_saturation=<N>       HDR saturation,      range 0..200,     default 100\n"
      "  --hdr_middle_gray=<N>      HDR middle-grey,     range 10..100,    default 50\n"
      "  --hdr_max_luminance=<N>    HDR target peak nits,range 400..2000,  default 650\n"
      "  --hdr_debanding_off=<0|1>  Skip DL-Debander pass for clean sources, default 0\n");
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
                GetFlagArgVal("progress", arg, &FLAG_progress) ||                  //
                GetFlagArgVal("use_pinned_memory", arg, &FLAG_usePinnedMemory) ||  //
                GetFlagArgVal("in_raw_rgba8", arg, &FLAG_inRawDims) ||             //
                GetFlagArgVal("log", arg, &FLAG_log) ||                            //
                GetFlagArgVal("log_level", arg, &FLAG_logLevel) ||                 //
                GetFlagArgVal("hdr_contrast", arg, &FLAG_hdrContrast) ||           //
                GetFlagArgVal("hdr_saturation", arg, &FLAG_hdrSaturation) ||       //
                GetFlagArgVal("hdr_middle_gray", arg, &FLAG_hdrMiddleGray) ||      //
                GetFlagArgVal("hdr_max_luminance", arg, &FLAG_hdrMaxLuminance) ||  //
                GetFlagArgVal("hdr_debanding_off", arg, &FLAG_hdrDebandingOff))) {
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
  size_t str_size = strlen(str), suf_size = strlen(suf);
  if (str_size < suf_size) return false;
  return (0 == strcasecmp(suf, str + str_size - suf_size));
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
  return HasOneOfTheseSuffixes(str, ".bmp", ".jpg", ".jpeg", ".png", nullptr);
}

// Returns true if the file exists and is readable as a regular file.
static bool FileExists(const char* path) {
  if (!path || !*path) return false;
  std::ifstream probe(path, std::ios::binary);
  return probe.is_open();
}

// Parse "WxH" from --in_raw_rgba8 into width/height. Returns false on any parse failure.
static bool ParseRawDims(const char* dims, int* width, int* height) {
  if (!dims || !*dims) return false;
  char* end = nullptr;
  long w = strtol(dims, &end, 10);
  if (!end || (*end != 'x' && *end != 'X') || w <= 0) return false;
  long h = strtol(end + 1, &end, 10);
  if (!end || (*end != '\0' && *end != ' ') || h <= 0) return false;
  *width = static_cast<int>(w);
  *height = static_cast<int>(h);
  return true;
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
        file_name, reinterpret_cast<char*>(&info->codec), info->width, info->height, info->frameRate, info->frameCount,
        info->frameRate > 0.0 ? DurationString(info->frameCount / info->frameRate) : "??:??:??.???");
}

struct TrueHDRApp {
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

  TrueHDRApp() = default;
  ~TrueHDRApp() { NvVFX_DestroyEffect(m_eff); }

  Err CreateEffect();
  void DestroyEffect();
  NvCV_Status AllocBuffers(unsigned width, unsigned height);
  NvCV_Status AllocTempBuffers();
  Err ProcessImage(const char* in_file, const char* out_file);
  Err ProcessMovie(const char* in_file, const char* out_file);
  Err ProcessRaw(const char* in_file, const char* out_file);
  Err AppErrFromVfxStatus(NvCV_Status status) { return (Err)status; }
  const char* ErrorStringFromCode(Err code);

  // Open the .r10 stream at the exact user-supplied --out_file path (no filename mangling).
  // The raw stream is headerless packed RGB10A2; the caller is responsible for propagating
  // dimensions to any downstream tool (e.g. the bundled ffmpeg packaging helper).
  std::string OpenHDR10Stream(const char* out_path, std::ofstream& fp) const;
  NvCV_Status WriteRawHDR10Frame(std::ofstream& fp, CUstream stream);

  NvVFX_Handle m_eff = nullptr;
  cv::Mat m_srcImg;        // CPU BGR U8 (cv::imread / cv::VideoCapture target).
  NvCVImage m_srcGpuBuf;   // GPU RGBA U8 interleaved - snippet input.
  NvCVImage m_dstGpuBuf;   // GPU (or pinned) RGB10A2 P32 - snippet output.
  NvCVImage m_srcVFX;      // NvCVImage alias for m_srcImg.
  NvCVImage m_tmpVFX;      // NvCVImage_Transfer staging.
  NvCVImage m_dstHDRHost;  // Host staging for fwrite (unused with --use_pinned_memory).
  bool m_inited = false;
  bool m_progress = false;
};

const char* TrueHDRApp::ErrorStringFromCode(Err code) {
  struct LutEntry {
    Err code;
    const char* str;
  };
  static const LutEntry kLut[] = {
      {errRead, "There was a problem reading a file"},
      {errWrite, "There was a problem writing a file"},
      {errQuit, "The user chose to quit the application"},
      {errFlag, "There was a problem with the command-line arguments"},
  };
  if ((int)code <= 0) return NvCV_GetErrorStringFromCode((NvCV_Status)code);
  for (const LutEntry* p = kLut; p != &kLut[sizeof(kLut) / sizeof(kLut[0])]; ++p)
    if (p->code == code) return p->str;
  return "UNKNOWN ERROR";
}

TrueHDRApp::Err TrueHDRApp::CreateEffect() {
  NvCV_Status vfx_err;
  BAIL_IF_ERR(vfx_err = NvVFX_CreateEffect(NVVFX_FX_TRUE_HDR, &m_eff));
bail:
  return AppErrFromVfxStatus(vfx_err);
}

// Push --hdr_* values into the effect. Can be called before or after NvVFX_Load.
static NvCV_Status ApplyHdrTunables(NvVFX_Handle eff) {
  NvCV_Status err;
  if (NVCV_SUCCESS != (err = NvVFX_SetU32(eff, NVVFXTRUEHDR_CONTRAST, FLAG_hdrContrast))) return err;
  if (NVCV_SUCCESS != (err = NvVFX_SetU32(eff, NVVFXTRUEHDR_SATURATION, FLAG_hdrSaturation))) return err;
  if (NVCV_SUCCESS != (err = NvVFX_SetU32(eff, NVVFXTRUEHDR_MIDDLE_GRAY, FLAG_hdrMiddleGray))) return err;
  if (NVCV_SUCCESS != (err = NvVFX_SetU32(eff, NVVFXTRUEHDR_LUMINANCE, FLAG_hdrMaxLuminance))) return err;
  if (NVCV_SUCCESS != (err = NvVFX_SetU32(eff, NVVFXTRUEHDR_DEBANDING_OFF, FLAG_hdrDebandingOff))) return err;
  return NVCV_SUCCESS;
}

void TrueHDRApp::DestroyEffect() {
  NvVFX_DestroyEffect(m_eff);
  m_eff = nullptr;
}

NvCV_Status TrueHDRApp::AllocTempBuffers() {
  NvCV_Status vfx_err;
  BAIL_IF_ERR(vfx_err = NvCVImage_Alloc(&m_tmpVFX, m_srcVFX.width, m_srcVFX.height, m_srcVFX.pixelFormat,
                                        m_srcVFX.componentType, m_srcVFX.planar, NVCV_GPU, 0));
bail:
  return vfx_err;
}

NvCV_Status TrueHDRApp::AllocBuffers(unsigned width, unsigned height) {
  NvCV_Status vfx_err = NVCV_SUCCESS;

  if (m_inited) return NVCV_SUCCESS;

  const unsigned io_mem_space = FLAG_usePinnedMemory ? NVCV_CPU_PINNED : NVCV_GPU;

  if (!m_srcImg.data) {
    m_srcImg.create(height, width, CV_8UC3);
    BAIL_IF_NULL(m_srcImg.data, vfx_err, NVCV_ERR_MEMORY);
  }

  // Output dims must match input. RGB10A2 P32 has no cv::Mat equivalent, so we
  // write it directly to disk instead of round-tripping through OpenCV.
  BAIL_IF_ERR(vfx_err = NvCVImage_Alloc(&m_srcGpuBuf, m_srcImg.cols, m_srcImg.rows, NVCV_RGBA, NVCV_U8,
                                        NVCV_INTERLEAVED, io_mem_space, 32));
  BAIL_IF_ERR(vfx_err = NvCVImage_Alloc(&m_dstGpuBuf, m_srcImg.cols, m_srcImg.rows, NVCV_RGB10A2, NVCV_P32,
                                        NVCV_INTERLEAVED, io_mem_space, 32));
  if (io_mem_space != NVCV_CPU_PINNED) {
    BAIL_IF_ERR(vfx_err = NvCVImage_Alloc(&m_dstHDRHost, m_srcImg.cols, m_srcImg.rows, NVCV_RGB10A2, NVCV_P32,
                                          NVCV_INTERLEAVED, NVCV_CPU, 32));
  }

  NVWrapperForCVMat(&m_srcImg, &m_srcVFX);

#ifndef ALLOC_TEMP_BUFFERS_AT_RUN_TIME
  BAIL_IF_ERR(vfx_err = AllocTempBuffers());
#endif

  m_inited = true;

bail:
  return vfx_err;
}

std::string TrueHDRApp::OpenHDR10Stream(const char* out_path, std::ofstream& fp) const {
  std::string fname = (out_path && *out_path) ? out_path : "output.r10";
  fp.open(fname, std::ios::out | std::ios::binary | std::ios::trunc);
  return fname;
}

NvCV_Status TrueHDRApp::WriteRawHDR10Frame(std::ofstream& fp, CUstream stream) {
  if (!fp.is_open()) return NVCV_ERR_PARAMETER;

  const NvCVImage* host_img = nullptr;
  if (m_dstGpuBuf.gpuMem == NVCV_CPU_PINNED) {
    NvCV_Status sync_err = NvVFX_CudaStreamSynchronize(stream);
    if (sync_err != NVCV_SUCCESS) return sync_err;
    host_img = &m_dstGpuBuf;
  } else {
    if (m_dstHDRHost.pixels == nullptr) return NVCV_ERR_INITIALIZATION;
    NvCV_Status xfer_err = NvCVImage_Transfer(&m_dstGpuBuf, &m_dstHDRHost, 1.f, stream, nullptr);
    if (xfer_err != NVCV_SUCCESS) return xfer_err;
    host_img = &m_dstHDRHost;
  }

  const size_t row_bytes = static_cast<size_t>(host_img->width) * host_img->pixelBytes;
  const char* row = static_cast<const char*>(host_img->pixels);
  for (unsigned y = 0; y < host_img->height; ++y) {
    fp.write(row, static_cast<std::streamsize>(row_bytes));
    row += host_img->pitch;
  }
  return NVCV_SUCCESS;
}

TrueHDRApp::Err TrueHDRApp::ProcessImage(const char* in_file, const char* out_file) {
  constexpr float kIdentityScale = 1.f;
  constexpr int kAsyncRun = 1;
  CUstream stream = 0;
  NvCV_Status vfx_err;
  if (!m_eff) return errEffect;

  m_srcImg = cv::imread(in_file);
  if (!m_srcImg.data) {
    if (!FileExists(in_file)) {
      printf(
          "Error: could not open file: \"%s\"\n"
          "  File not found or not accessible. Check the path and permissions.\n",
          in_file);
    } else {
      printf(
          "Error: could not decode \"%s\" as an image.\n"
          "  File exists but its format is not a recognized image or the file is corrupt.\n"
          "  Supported image extensions: .bmp, .jpg, .jpeg, .png.\n"
          "  For video, use a container OpenCV can decode via its selected backend\n"
          "  (e.g., .mp4, .avi, .mov, .mkv).\n"
          "  For raw RGBA8 streams, add --in_raw_rgba8=WxH.\n",
          in_file);
    }
    return errRead;
  }

  BAIL_IF_ERR(vfx_err = AllocBuffers(m_srcImg.cols, m_srcImg.rows));

  BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&m_srcVFX, &m_srcGpuBuf, kIdentityScale, stream, &m_tmpVFX));
  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_INPUT_IMAGE, &m_srcGpuBuf));
  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_OUTPUT_IMAGE, &m_dstGpuBuf));
  BAIL_IF_ERR(vfx_err = NvVFX_SetCudaStream(m_eff, NVVFX_CUDA_STREAM, stream));
  BAIL_IF_ERR(vfx_err = ApplyHdrTunables(m_eff));
  BAIL_IF_ERR(vfx_err = NvVFX_Load(m_eff));
  BAIL_IF_ERR(vfx_err = NvVFX_Run(m_eff, kAsyncRun));

  {
    std::ofstream fp;
    std::string fname = OpenHDR10Stream(out_file, fp);
    if (!fp.is_open()) {
      printf("Error opening \"%s\" for raw HDR10 write\n", fname.c_str());
      return errWrite;
    }
    BAIL_IF_ERR(vfx_err = WriteRawHDR10Frame(fp, stream));
    fp.close();
    fprintf(stderr, "Wrote HDR10 output: %s\n", fname.c_str());
  }

bail:
  return AppErrFromVfxStatus(vfx_err);
}

TrueHDRApp::Err TrueHDRApp::ProcessMovie(const char* in_file, const char* out_file) {
  constexpr float kIdentityScale = 1.f;
  constexpr int kAsyncRun = 1;
  CUstream stream = 0;
  cv::VideoCapture reader;
  // Declared up-front: goto bail must not cross std::ofstream's non-trivial init.
  std::ofstream hdr_fp;
  std::string hdr_fname;
  NvCV_Status vfx_err = NVCV_SUCCESS;
  unsigned frame_num;
  VideoInfo info{};
  if (in_file && !in_file[0]) in_file = nullptr;

  if (in_file) {
    reader.open(in_file);
  } else {
    std::cerr << "Please specify --in_file=<path>\n";
    return errFlag;
  }

  if (!reader.isOpened()) {
    if (!FileExists(in_file)) {
      printf(
          "Error: could not open file: \"%s\"\n"
          "  File not found or not accessible. Check the path and permissions.\n",
          in_file ? in_file : "(null)");
    } else {
      printf(
          "Error: could not open \"%s\" as a video.\n"
          "  File exists but its format is not a recognized video container.\n"
          "  Supported input:\n"
          "    - Video containers OpenCV can decode via its selected backend\n"
          "      (e.g., .mp4, .avi, .mov, .mkv)\n"
          "    - Still images (extension-based): .bmp, .jpg, .jpeg, .png\n"
          "    - Raw RGBA8 stream: add --in_raw_rgba8=WxH\n"
          "  Formats like .gif and .webp are not supported by this app.\n",
          in_file);
    }
    return errRead;
  }
  GetVideoInfo(reader, in_file, &info);
  if (info.width <= 0 || info.height <= 0) {
    printf(
        "Error: \"%s\" opened but reports %dx%d frame dimensions.\n"
        "  For a raw RGBA8 stream, add --in_raw_rgba8=WxH.\n",
        in_file, info.width, info.height);
    reader.release();
    return errRead;
  }
  BAIL_IF_ERR(vfx_err = AllocBuffers(info.width, info.height));

  if (out_file && !out_file[0]) out_file = nullptr;
  if (out_file) {
    hdr_fname = OpenHDR10Stream(out_file, hdr_fp);
    if (!hdr_fp.is_open()) {
      printf("Cannot open \"%s\" for raw HDR10 write\n", hdr_fname.c_str());
      return errWrite;
    }
  }

  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_INPUT_IMAGE, &m_srcGpuBuf));
  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_OUTPUT_IMAGE, &m_dstGpuBuf));
  BAIL_IF_ERR(vfx_err = NvVFX_SetCudaStream(m_eff, NVVFX_CUDA_STREAM, stream));
  BAIL_IF_ERR(vfx_err = ApplyHdrTunables(m_eff));
  BAIL_IF_ERR(vfx_err = NvVFX_Load(m_eff));

  frame_num = 0;
  for (;;) {
    if (!reader.read(m_srcImg)) break;
    if (m_srcImg.empty()) {
      printf("Frame %u is empty\n", frame_num);
      continue;
    }

    if (frame_num == 0 && out_file) {
      fprintf(stderr, "Writing HDR10 output: %s\n", hdr_fname.c_str());
    }

    BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&m_srcVFX, &m_srcGpuBuf, kIdentityScale, stream, &m_tmpVFX));
    BAIL_IF_ERR(vfx_err = NvVFX_Run(m_eff, kAsyncRun));
    if (out_file) {
      BAIL_IF_ERR(vfx_err = WriteRawHDR10Frame(hdr_fp, stream));
    }

    if (m_progress && info.frameCount > 0) {
      fprintf(stderr, "\b\b\b\b%3.0f%%", 100.f * frame_num / info.frameCount);
    }
    ++frame_num;
  }

  if (m_progress) fprintf(stderr, "\n");
  reader.release();
  if (hdr_fp.is_open()) hdr_fp.close();

  if (frame_num == 0) {
    fprintf(stderr,
            "Error: read 0 frames from \"%s\" (container reports %lld frames).\n"
            "  Container opened, but the decoder returned no frames. Common cause:\n"
            "  the video codec/profile is not supported by the platform decoder.\n",
            in_file, info.frameCount);
    if (out_file) std::remove(out_file);  // don't leave a 0-byte artifact
    return errRead;
  }

bail:
  return AppErrFromVfxStatus(vfx_err);
}

TrueHDRApp::Err TrueHDRApp::ProcessRaw(const char* in_file, const char* out_file) {
  constexpr float kIdentityScale = 1.f;
  constexpr int kAsyncRun = 1;
  CUstream stream = 0;
  std::ifstream in_fp;
  std::ofstream hdr_fp;
  std::string hdr_fname;
  std::vector<uint8_t> staging;
  NvCVImage host_view{};  // Host wrapper over staging; Init once, refill each frame.
  NvCV_Status vfx_err = NVCV_SUCCESS;
  int width = 0;
  int height = 0;
  long long frame_count = 0;
  size_t frame_bytes = 0;
  unsigned frame_num;

  if (!in_file || !*in_file) {
    std::cerr << "Please specify --in_file=<path>\n";
    return errFlag;
  }
  if (!ParseRawDims(FLAG_inRawDims.c_str(), &width, &height)) {
    std::cerr << "Invalid --in_raw_rgba8 value: \"" << FLAG_inRawDims << "\" (expected WxH, e.g. 1280x720)\n";
    return errFlag;
  }

  in_fp.open(in_file, std::ios::in | std::ios::binary);
  if (!in_fp.is_open()) {
    printf("Error: Could not open raw input: \"%s\"\n", in_file);
    return errRead;
  }
  // Infer frame count from file size / (4 * W * H); raw stream has no header.
  in_fp.seekg(0, std::ios::end);
  const long long size_bytes = static_cast<long long>(in_fp.tellg());
  in_fp.seekg(0, std::ios::beg);
  frame_bytes = static_cast<size_t>(4) * static_cast<size_t>(width) * static_cast<size_t>(height);
  frame_count = (frame_bytes > 0) ? size_bytes / static_cast<long long>(frame_bytes) : 0;
  if (frame_count <= 0) {
    printf("Error: raw file too small for one %dx%d RGBA8 frame (size=%lld, frame=%zu)\n",  //
           width, height, size_bytes, frame_bytes);
    return errRead;
  }
  if (FLAG_verbose) {
    printf("       file \"%s\"\n      width %4d\n     height %4d\nframe count %4lld\n",  //
           in_file, width, height, frame_count);
  }

  BAIL_IF_ERR(vfx_err = AllocBuffers(static_cast<unsigned>(width), static_cast<unsigned>(height)));
  staging.resize(frame_bytes);

  // Init the host wrapper once; staging.data() is stable for the lifetime of `staging`.
  BAIL_IF_ERR(vfx_err = NvCVImage_Init(&host_view, static_cast<unsigned>(width), static_cast<unsigned>(height),
                                       width * 4, staging.data(), NVCV_RGBA, NVCV_U8, NVCV_INTERLEAVED, NVCV_CPU));

  if (out_file && !out_file[0]) out_file = nullptr;
  if (out_file) {
    hdr_fname = OpenHDR10Stream(out_file, hdr_fp);
    if (!hdr_fp.is_open()) {
      printf("Cannot open \"%s\" for raw HDR10 write\n", hdr_fname.c_str());
      return errWrite;
    }
    fprintf(stderr, "Writing HDR10 output: %s\n", hdr_fname.c_str());
  }

  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_INPUT_IMAGE, &m_srcGpuBuf));
  BAIL_IF_ERR(vfx_err = NvVFX_SetImage(m_eff, NVVFX_OUTPUT_IMAGE, &m_dstGpuBuf));
  BAIL_IF_ERR(vfx_err = NvVFX_SetCudaStream(m_eff, NVVFX_CUDA_STREAM, stream));
  BAIL_IF_ERR(vfx_err = ApplyHdrTunables(m_eff));
  BAIL_IF_ERR(vfx_err = NvVFX_Load(m_eff));

  for (frame_num = 0; frame_num < static_cast<unsigned>(frame_count); ++frame_num) {
    in_fp.read(reinterpret_cast<char*>(staging.data()), static_cast<std::streamsize>(frame_bytes));
    if (!in_fp) {
      printf("Frame %u: short read on raw input (wanted %zu bytes)\n", frame_num, frame_bytes);
      break;
    }
    BAIL_IF_ERR(vfx_err = NvCVImage_Transfer(&host_view, &m_srcGpuBuf, kIdentityScale, stream, nullptr));
    BAIL_IF_ERR(vfx_err = NvVFX_Run(m_eff, kAsyncRun));
    if (out_file) {
      BAIL_IF_ERR(vfx_err = WriteRawHDR10Frame(hdr_fp, stream));
    }
    if (m_progress && frame_count > 0) {
      fprintf(stderr, "\b\b\b\b%3.0f%%", 100.f * frame_num / frame_count);
    }
  }
  if (m_progress) fprintf(stderr, "\n");
  in_fp.close();
  if (hdr_fp.is_open()) hdr_fp.close();

bail:
  return AppErrFromVfxStatus(vfx_err);
}

int main(int argc, char** argv) {
  TrueHDRApp::Err fx_err = TrueHDRApp::errNone;
  int n_errs;
  TrueHDRApp app;

  n_errs = ParseMyArgs(argc, argv);
  if (n_errs == NVCV_ERR_HELP) {
    Usage();
    return 0;
  }
  if (n_errs) std::cerr << n_errs << " command line syntax problems\n";

  NvCV_Status vfx_err = NvVFX_ConfigureLogger(FLAG_logLevel, FLAG_log.c_str(), nullptr, nullptr);
  if (NVCV_SUCCESS != vfx_err)
    printf("%s: while configuring logger to \"%s\"\n", NvCV_GetErrorStringFromCode(vfx_err), FLAG_log.c_str());

  if (FLAG_inFile.empty()) {
    std::cerr << "Please specify --in_file=<path>\n";
    ++n_errs;
  }
  if (FLAG_outFile.empty()) {
    std::cerr << "Please specify --out_file=<path>\n";
    ++n_errs;
  }
  app.m_progress = FLAG_progress;

  if (n_errs) {
    Usage();
    fx_err = TrueHDRApp::errFlag;
  } else {
    fx_err = app.CreateEffect();
    if (TrueHDRApp::errNone != fx_err) {
      std::cerr << "Error creating TrueHDR effect\n";
    } else if (!FLAG_inRawDims.empty()) {
      fx_err = app.ProcessRaw(FLAG_inFile.c_str(), FLAG_outFile.c_str());
    } else if (IsImageFile(FLAG_inFile.c_str())) {
      fx_err = app.ProcessImage(FLAG_inFile.c_str(), FLAG_outFile.c_str());
    } else {
      fx_err = app.ProcessMovie(FLAG_inFile.c_str(), FLAG_outFile.c_str());
    }
  }

  if (fx_err) std::cerr << "Error: " << app.ErrorStringFromCode(fx_err) << std::endl;
  return (int)fx_err;
}
