/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <string>
#include <vector>

#include "batchUtilities.h"
#include "nvCVOpenCV.h"
#include "nvVFXGreenScreen.h"
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
#define BAIL_IF_FALSE(x, err, code) \
  do {                              \
    if (!(x)) {                     \
      err = code;                   \
      goto bail;                    \
    }                               \
  } while (0)
#define BAIL(err, code) \
  do {                  \
    err = code;         \
    goto bail;          \
  } while (0)

#define DEFAULT_CODEC "avc1"

bool FLAG_verbose = false;
int FLAG_mode = 0;
int FLAG_logLevel = NVCV_LOG_ERROR;
std::string FLAG_log = "stderr";
std::string FLAG_outFile;
std::string FLAG_modelDir;
std::string FLAG_codec = DEFAULT_CODEC;
std::vector<const char*> FLAG_inFiles;

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

static bool GetFlagArgVal(const char* flag, const char* arg, long* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) *val = strtol(valStr, NULL, 10);
  return success;
}

static bool GetFlagArgVal(const char* flag, const char* arg, int* val) {
  long longVal;
  bool success = GetFlagArgVal(flag, arg, &longVal);
  if (success) *val = (int)longVal;
  return success;
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

static void Usage() {
  printf(
      "BatchAigsEffectApp [ flags ... ] inFile1 [ inFileN ... ]\n"
      "  where flags is:\n"
      "  --out_file=<path>     output video files to be written (a pattern with one %%u or %%d), default "
      "\"BatchOut_%%02u.mp4\"\n"
      "  --model_dir=<path>    the path to the directory that contains the models\n"
      "  --mode=<value>        which model to pick for processing (default: 0)\n"
      "  --verbose             verbose output\n"
      "  --codec=<fourcc>      the fourcc code for the desired codec (default " DEFAULT_CODEC
      ")\n"
      "  --log=<file>          log SDK errors to a file, \"stderr\" or \"\" (default stderr)\n"
      "  --log_level=<N>       the desired log level: {0, 1, 2, 3} = {FATAL, ERROR, WARNING, INFO}, respectively "
      "(default 1)\n"
      "  and inFile1 ... are identically sized video files\n");
}

static int ParseMyArgs(int argc, char** argv) {
  int errs = 0;
  for (--argc, ++argv; argc--; ++argv) {
    bool help;
    const char* arg = *argv;
    if (arg[0] == '-') {
      if (arg[1] == '-') {                                      // double-dash
        if (GetFlagArgVal("verbose", arg, &FLAG_verbose) ||     //
            GetFlagArgVal("mode", arg, &FLAG_mode) ||           //
            GetFlagArgVal("model_dir", arg, &FLAG_modelDir) ||  //
            GetFlagArgVal("out_file", arg, &FLAG_outFile) ||    //
            GetFlagArgVal("log", arg, &FLAG_log) ||             //
            GetFlagArgVal("log_level", arg, &FLAG_logLevel) ||  //
            GetFlagArgVal("codec", arg, &FLAG_codec)) {
          continue;
        } else if (GetFlagArgVal("help", arg, &help)) {  // --help
          Usage();
          errs = 1;
        }
      } else {  // single dash
        for (++arg; *arg; ++arg) {
          if (*arg == 'v') {
            FLAG_verbose = true;
          } else {
            printf("Unknown flag ignored: \"-%c\"\n", *arg);
          }
        }
        continue;
      }
    } else {  // no dash
      FLAG_inFiles.push_back(arg);
    }
  }
  return errs;
}

class App {
 public:
  NvVFX_Handle _eff;
  NvCVImage _src, _stg, _dst;
  CUstream _stream;
  unsigned _batchSize;
  App() : _eff(nullptr), _stream(0), _batchSize(0) {}
  ~App() {
    NvVFX_DestroyEffect(_eff);
    if (_stream) NvVFX_CudaStreamDestroy(_stream);
  }

  NvCV_Status init(const char* effectName, unsigned batchSize, unsigned int mode, const NvCVImage* srcImg) {
    NvCV_Status err = NVCV_ERR_UNIMPLEMENTED;

    _batchSize = batchSize;
    BAIL_IF_ERR(err = NvVFX_CreateEffect(effectName, &_eff));
    BAIL_IF_ERR(err = AllocateBatchBuffer(&_src, _batchSize, srcImg->width, srcImg->height, NVCV_BGR, NVCV_U8,
                                          NVCV_CHUNKY, NVCV_GPU, 1));
    BAIL_IF_ERR(err = AllocateBatchBuffer(&_dst, _batchSize, srcImg->width, srcImg->height, NVCV_A, NVCV_U8,
                                          NVCV_CHUNKY, NVCV_GPU, 1));
    BAIL_IF_ERR(err = NvVFX_SetString(_eff, NVVFX_MODEL_DIRECTORY, FLAG_modelDir.c_str()));

    {  // Set parameters.
      NvCVImage nth;
      BAIL_IF_ERR(err = NvVFX_SetImage(
                      _eff, NVVFX_INPUT_IMAGE,
                      NthImage(0, srcImg->height, &_src, &nth)));  // Set the first of the batched images in ...
      BAIL_IF_ERR(err = NvVFX_SetImage(_eff, NVVFX_OUTPUT_IMAGE,
                                       NthImage(0, _dst.height / _batchSize, &_dst, &nth)));  // ... and out
      BAIL_IF_ERR(err = NvVFX_CudaStreamCreate(&_stream));
      BAIL_IF_ERR(err = NvVFX_SetCudaStream(_eff, NVVFX_CUDA_STREAM, _stream));
      BAIL_IF_ERR(err = NvVFX_SetU32(_eff, NVVFX_MODE, mode));
    }

  bail:
    return err;
  }
};

NvCV_Status BatchProcess(const char* effectName, unsigned int mode, const std::vector<const char*>& srcVideos,
                         const char* outfilePattern, std::string codec) {
  NvCV_Status err = NVCV_SUCCESS;
  App app;
  cv::Mat ocv1, ocv2;
  NvCVImage nvx1, nvx2;
  unsigned srcWidth, srcHeight, dstHeight;

  std::vector<NvVFX_StateObjectHandle> arrayOfStates;
  NvVFX_StateObjectHandle* batchOfStates = nullptr;

  unsigned int numOfVideoStreams = static_cast<unsigned int>(srcVideos.size());

  // If valid states are passed for inference, then -
  // 1. The largest batch this effect can process is equal to maximum number of video streams
  // 2. Multiple frames from the same video stream should not be present in the same batch
  unsigned maxBatchSize = numOfVideoStreams;
  std::vector<unsigned> batchIndices(maxBatchSize);  // store video index order in a batch

  std::vector<cv::VideoCapture> srcCaptures(numOfVideoStreams);
  std::vector<cv::VideoWriter> dstWriters(numOfVideoStreams);
  for (unsigned int i = 0; i < numOfVideoStreams; i++) {
    srcCaptures[i].open(srcVideos[i]);
    if (srcCaptures[i].isOpened() == false) BAIL(err, NVCV_ERR_READ);

    int width, height;
    double fps;
    width = (int)srcCaptures[i].get(cv::CAP_PROP_FRAME_WIDTH);
    height = (int)srcCaptures[i].get(cv::CAP_PROP_FRAME_HEIGHT);
    fps = srcCaptures[i].get(cv::CAP_PROP_FPS);

    const int fourcc = StringToFourcc(codec);
    char fileName[1024];
    snprintf(fileName, sizeof(fileName), outfilePattern, i);
    dstWriters[i].open(fileName, fourcc, fps, cv::Size2i(width, height), false);
    if (dstWriters[i].isOpened() == false) BAIL(err, NVCV_ERR_WRITE);
  }

  // Read in the first image, to determine the resolution for init()
  BAIL_IF_FALSE(srcVideos.size() > 0, err, NVCV_ERR_MISSINGINPUT);
  srcCaptures[0] >> ocv1;
  srcCaptures[0].set(cv::CAP_PROP_POS_FRAMES, 0);  // resetting to first frame
  if (!ocv1.data) {
    printf("Cannot read video file \"%s\"\n", srcVideos[0]);
    BAIL(err, NVCV_ERR_READ);
  }
  NVWrapperForCVMat(&ocv1, &nvx1);
  srcWidth = nvx1.width;
  srcHeight = nvx1.height;

  BAIL_IF_ERR(err = app.init(effectName, maxBatchSize, mode, &nvx1));  // Init effect and buffers
  BAIL_IF_ERR(err = NvVFX_SetU32(app._eff, NVVFX_MAX_NUMBER_STREAMS, numOfVideoStreams));
  BAIL_IF_ERR(err = NvVFX_SetU32(app._eff, NVVFX_MODEL_BATCH, numOfVideoStreams > 1 ? 8 : 1));
  BAIL_IF_ERR(err = NvVFX_Load(app._eff));

  // Creating state objects, one per stream.
  for (unsigned int i = 0; i < numOfVideoStreams; i++) {
    NvVFX_StateObjectHandle state = nullptr;
    BAIL_IF_ERR(err = NvVFX_AllocateState(app._eff, &state));
    arrayOfStates.push_back(state);
  }

  // Creating batch array to hold states
  batchOfStates = (NvVFX_StateObjectHandle*)malloc(sizeof(NvVFX_StateObjectHandle) * maxBatchSize);
  if (batchOfStates == nullptr) {
    err = NVCV_ERR_MEMORY;
    goto bail;
  }

  dstHeight = app._dst.height / maxBatchSize;
  BAIL_IF_ERR(err = NvCVImage_Alloc(&nvx2, app._dst.width, dstHeight, NVCV_A, NVCV_U8, NVCV_CHUNKY, NVCV_CPU, 0));
  CVWrapperForNvCVImage(&nvx2, &ocv2);
  for (int j = 0;; j++) {
    unsigned activeVideoCount =
        0;  // counter to keep track of how many videos have not ended and still need to be processed
    for (unsigned int i = 0; i < maxBatchSize; i++) {
      int capIdx = i % numOfVideoStreams;  // interlacing frames from different video stream, but can in any order
      if (!srcCaptures[capIdx].isOpened()) continue;  // if video ended, we do not count
      srcCaptures[capIdx] >> ocv1;
      if (ocv1.empty()) {
        srcCaptures[capIdx].release();  // if video ended, we close it and do not count
        continue;
      }
      batchIndices[activeVideoCount] = capIdx;
      batchOfStates[activeVideoCount] = arrayOfStates[capIdx];

      NVWrapperForCVMat(&ocv1, &nvx1);
      if (!(nvx1.width == srcWidth && nvx1.height == srcHeight)) {
        printf(
            "Input video file \"%s\" %ux%u does not match %ux%u\n"
            "Batching requires all video frames to be of the same size\n",
            srcVideos[activeVideoCount], nvx1.width, nvx1.height, srcWidth, srcHeight);
        BAIL(err, NVCV_ERR_MISMATCH);
      }
      BAIL_IF_ERR(err = TransferToNthImage(activeVideoCount, &nvx1, &app._src, 1.f, app._stream, NULL));
      ocv1.release();
      activeVideoCount++;
    }
    if (activeVideoCount == 0) goto bail;  // all videos have been processed

    // Run batch
    unsigned batchSize = activeVideoCount;  // processing a batch consisting of 1 frame from each of the active videos
    BAIL_IF_ERR(
        err = NvVFX_SetU32(app._eff, NVVFX_BATCH_SIZE, (unsigned)batchSize));  // The batchSize can change every Run
    BAIL_IF_ERR(err = NvVFX_SetStateObjectHandleArray(app._eff, NVVFX_STATE,
                                                      batchOfStates));  // The batch of states can change every Run
    BAIL_IF_ERR(err = NvVFX_Run(app._eff, 0));

    for (unsigned int i = 0; i < batchSize; ++i) {
      int writerIdx = batchIndices[i];
      BAIL_IF_ERR(err = TransferFromNthImage(i, &app._dst, &nvx2, 1.0f, app._stream, NULL));
      dstWriters[writerIdx] << ocv2;
    }
    // NvCVImage_Dealloc() is called in the destructors
  }
bail:
  // If DeallocateState fails, all memory allocated in the SDK returns to the heap when the effect handle is destroyed.
  for (unsigned int i = 0; i < arrayOfStates.size(); i++) {
    NvVFX_DeallocateState(app._eff, arrayOfStates[i]);
  }
  arrayOfStates.clear();

  if (batchOfStates) {
    free(batchOfStates);
    batchOfStates = nullptr;
  }

  for (auto& cap : srcCaptures) {
    if (cap.isOpened()) cap.release();
  }

  for (auto& writer : dstWriters) {
    if (writer.isOpened()) writer.release();
  }

  return err;
}

int main(int argc, char** argv) {
  int nErrs;
  NvCV_Status vfxErr;

  nErrs = ParseMyArgs(argc, argv);
  if (nErrs) return nErrs;

  vfxErr = NvVFX_ConfigureLogger(FLAG_logLevel, FLAG_log.c_str(), nullptr, nullptr);
  if (NVCV_SUCCESS != vfxErr)
    printf("%s: while configuring logger to \"%s\"\n", NvCV_GetErrorStringFromCode(vfxErr), FLAG_log.c_str());

  // If the outFile is missing a stream index
  // insert one, assuming a period followed by a three-character extension
  if (FLAG_outFile.empty())
    FLAG_outFile = "BatchOut_%02u.mp4";
  else if (std::string::npos == FLAG_outFile.find_first_of('%'))
    FLAG_outFile.insert(FLAG_outFile.size() - 4, "_%02u");

  vfxErr = BatchProcess(NVVFX_FX_GREEN_SCREEN, FLAG_mode, FLAG_inFiles, FLAG_outFile.c_str(), FLAG_codec);

  if (NVCV_SUCCESS != vfxErr) {
    Usage();
    printf("Error: %s\n", NvCV_GetErrorStringFromCode(vfxErr));
    nErrs = (int)vfxErr;
  }

  return nErrs;
}
