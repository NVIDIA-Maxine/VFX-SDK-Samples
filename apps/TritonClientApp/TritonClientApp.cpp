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

#include <memory>
#include <string>
#include <vector>

#include "batchUtilities.h"
#include "nvCVOpenCV.h"
#include "nvVFXGreenScreen.h"
#include "nvVideoEffects.h"
#include "opencv2/opencv.hpp"

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

std::string FLAG_effect;
bool FLAG_verbose = false;
bool FLAG_useTritonGRPC = false;
std::string FLAG_tritonURL = "localhost:8001";
int FLAG_mode = 0;
int FLAG_logLevel = NVCV_LOG_ERROR;
std::string FLAG_log = "stderr";
std::string FLAG_outFile;
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

static bool GetFlagArgVal(const char* flag, const char* arg, float* val) {
  const char* valStr;
  bool success = GetFlagArgVal(flag, arg, &valStr);
  if (success) *val = strtof(valStr, NULL);
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
      "TritonClientApp [ flags ... ] inFile1 [ inFileN ... ]\n"
      "  where flags is:\n"
      "  --effect=<effect name>        name of the effect to run (Supported: " NVVFX_FX_GREEN_SCREEN
      ").\n"
      "  --url=<URL>                   URL to the Triton server\n"
      "  --grpc[=(true|false)]         use gRPC for data transfer to the Triton server instead of CUDA shared memory.\n"
      "  --out_file=<path>             output video files to be written (a pattern with one %%u or %%d), default "
      "\"BatchOut_%%02u.mp4\"\n"
      "  --model_dir=<path>            the path to the directory that contains the models\n"
      "  --mode=<value>                which model to pick for processing (default: 0)\n"
      "  --verbose                     verbose output\n"
      "  --codec=<fourcc>              the fourcc code for the desired codec (default " DEFAULT_CODEC
      ")\n"
      "  --log=<file>                  log SDK errors to a file, \"stderr\" or \"\" (default stderr)\n"
      "  --log_level=<N>               the desired log level: {0, 1, 2} = {FATAL, ERROR, WARNING}, respectively "
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
        if (GetFlagArgVal("effect", arg, &FLAG_effect) ||       //
            GetFlagArgVal("verbose", arg, &FLAG_verbose) ||     //
            GetFlagArgVal("url", arg, &FLAG_tritonURL) ||       //
            GetFlagArgVal("grpc", arg, &FLAG_useTritonGRPC) ||  //
            GetFlagArgVal("mode", arg, &FLAG_mode) ||           //
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

class BaseApp {
 public:
  NvVFX_Handle m_eff;
  NvCVImage m_src, m_stg, m_dst;
  NvCVImage m_nvTempResult, m_nthImg;
  CUstream m_stream;
  unsigned m_numVideoStreams;
  NvVFX_TritonServer m_triton;
  std::string m_effectName;
  std::vector<NvVFX_StateObjectHandle> m_arrayOfStates;
  std::vector<NvVFX_StateObjectHandle> m_batchOfStates;

  static BaseApp* Create(const char* effect_name);
  virtual ~BaseApp() {
    if (m_eff) NvVFX_DestroyEffect(m_eff);
    if (m_stream) NvVFX_CudaStreamDestroy(m_stream);
    if (m_triton) NvVFX_DisconnectTritonServer(m_triton);
  }
  virtual NvCV_Status Init(unsigned num_video_streams) {
    NvCV_Status err = NVCV_ERR_UNIMPLEMENTED;
    m_numVideoStreams = num_video_streams;
    err = NvVFX_ConnectTritonServer(FLAG_tritonURL.c_str(), &m_triton);
    if (err != NVCV_SUCCESS) printf("Error connecting to the server at %s.\n", FLAG_tritonURL.c_str());
    BAIL_IF_ERR(err);
    err = NvVFX_CreateEffectTriton(m_effectName.c_str(), &m_eff);
    if (err != NVCV_SUCCESS)
      printf("Error creating the %s feature on the server at %s.\n", m_effectName.c_str(), FLAG_tritonURL.c_str());
    BAIL_IF_ERR(err);
    err = NvVFX_SetTritonServer(m_eff, m_triton);
    if (err != NVCV_SUCCESS)
      printf("Error creating the %s feature on the server at %s.\n", m_effectName.c_str(), FLAG_tritonURL.c_str());
    BAIL_IF_ERR(err);
    m_arrayOfStates.resize(m_numVideoStreams, nullptr);
    m_batchOfStates.resize(m_numVideoStreams, nullptr);
    if (FLAG_verbose) {
      unsigned int using_triton = 0;
      err = NvVFX_IsUsingTriton(m_eff, &using_triton);
      if (NVCV_SUCCESS != err) {
        printf("Error: %s\n", NvCV_GetErrorStringFromCode(err));
      }
      if (using_triton) printf("Using triton server\n");
    }
  bail:
    return err;
  }
  virtual NvCV_Status Load() { return NvVFX_Load(m_eff); }
  virtual NvCV_Status Run(const unsigned* batch_indices, unsigned batchsize) {
    NvCV_Status err = NVCV_SUCCESS;
    for (int i = 0; i < batchsize; i++) {
      m_batchOfStates[i] = m_arrayOfStates[batch_indices[i]];
    }
    BAIL_IF_ERR(err = NvVFX_SetU32(m_eff, NVVFX_BATCH_SIZE, batchsize));  // The batchSize can change every Run
    BAIL_IF_ERR(err = NvVFX_SetStateObjectHandleArray(
                    m_eff, NVVFX_STATE, m_batchOfStates.data()));  // The batch of states can change every Run
    BAIL_IF_ERR(err = NvVFX_Run(m_eff, 0));
  bail:
    return err;
  }
  virtual NvCV_Status InitVideoStream(unsigned n) { return NvVFX_AllocateState(m_eff, &m_arrayOfStates[n]); }
  virtual NvCV_Status ReleaseVideoStream(unsigned n) { return NvVFX_DeallocateState(m_eff, m_arrayOfStates[n]); }
  virtual NvCV_Status AllocateBuffers(unsigned width, unsigned height) = 0;
  virtual NvCV_Status SetParameters() = 0;
  virtual NvCV_Status GenerateNthOutputVizImage(unsigned n, const cv::Mat& input, cv::Mat& result) = 0;

 protected:
  BaseApp() : m_eff(nullptr), m_stream(0), m_numVideoStreams(0), m_triton(nullptr) {}
};

class AIGSApp : public BaseApp {
 public:
  NvCV_Status AllocateBuffers(unsigned width, unsigned height) {
    NvCV_Status err = NVCV_SUCCESS;
    BAIL_IF_ERR(err = AllocateBatchBuffer(&m_src, m_numVideoStreams, width, height, NVCV_BGR, NVCV_U8, NVCV_CHUNKY,
                                          FLAG_useTritonGRPC ? NVCV_CPU : NVCV_GPU, 1));
    BAIL_IF_ERR(err = AllocateBatchBuffer(&m_dst, m_numVideoStreams, width, height, NVCV_A, NVCV_U8, NVCV_CHUNKY,
                                          FLAG_useTritonGRPC ? NVCV_CPU : NVCV_GPU, 1));
  bail:
    return err;
  }
  NvCV_Status SetParameters() {
    NvCV_Status err = NVCV_SUCCESS;
    BAIL_IF_ERR(err = NvVFX_SetImage(m_eff, NVVFX_INPUT_IMAGE,
                                     NthImage(0, m_src.height / m_numVideoStreams, &m_src,
                                              &m_stg)));  // Set the first of the batched images in ...
    BAIL_IF_ERR(err = NvVFX_SetImage(m_eff, NVVFX_OUTPUT_IMAGE,
                                     NthImage(0, m_dst.height / m_numVideoStreams, &m_dst, &m_stg)));  // ... and out
    BAIL_IF_ERR(err = NvVFX_SetU32(m_eff, NVVFX_MODE, FLAG_mode));
  bail:
    return err;
  }
  NvCV_Status GenerateNthOutputVizImage(unsigned n, const cv::Mat& input, cv::Mat& result) {
    NvCV_Status err = NVCV_SUCCESS;
    result = cv::Mat(m_dst.height / m_numVideoStreams, m_dst.width, CV_8UC1);
    NVWrapperForCVMat(&result, &m_nvTempResult);
    BAIL_IF_ERR(err = NvCVImage_Transfer(NthImage(n, m_src.height / m_numVideoStreams, &m_dst, &m_nthImg),
                                         &m_nvTempResult, 1, m_stream, &m_stg));
  bail:
    return err;
  }
};

BaseApp* BaseApp::Create(const char* effect_name) {
  BaseApp* obj = nullptr;
  if (!strcasecmp(effect_name, NVVFX_FX_GREEN_SCREEN))
    obj = new AIGSApp;
  else
    return nullptr;
  obj->m_effectName = effect_name;
  return obj;
}

NvCV_Status BatchProcess(const char* effectName, const std::vector<const char*>& srcVideos, const char* outfilePattern,
                         std::string codec) {
  NvCV_Status err = NVCV_SUCCESS;
  std::unique_ptr<BaseApp> app(BaseApp::Create(effectName));
  cv::Mat ocv_cpu;
  NvCVImage nvcv_cpu;
  unsigned src_width, src_height;

  // 1. The largest batch this effect can process is equal to maximum number of video streams
  // 2. Multiple frames from the same video stream should not be present in the same batch
  unsigned int num_video_streams = static_cast<unsigned int>(srcVideos.size());
  std::vector<unsigned> batch_indices(num_video_streams);  // store video index order in a batch
  std::vector<cv::VideoCapture> src_captures(num_video_streams);
  std::vector<cv::VideoWriter> dst_writers(num_video_streams);
  std::vector<cv::Mat> frames(num_video_streams), frames_t_1(num_video_streams);

  // Open video file readers and writers
  for (unsigned int i = 0; i < num_video_streams; i++) {
    src_captures[i].open(srcVideos[i]);
    if (src_captures[i].isOpened() == false) BAIL(err, NVCV_ERR_READ);

    int width, height;
    double fps;
    width = (int)src_captures[i].get(cv::CAP_PROP_FRAME_WIDTH);
    height = (int)src_captures[i].get(cv::CAP_PROP_FRAME_HEIGHT);
    fps = src_captures[i].get(cv::CAP_PROP_FPS);

    const int fourcc = StringToFourcc(codec);
    char fileName[1024];
    snprintf(fileName, sizeof(fileName), outfilePattern, i);
    dst_writers[i].open(fileName, fourcc, fps, cv::Size2i(width, height), false);
    if (dst_writers[i].isOpened() == false) BAIL(err, NVCV_ERR_WRITE);
  }

  // Read in the first image, to determine the resolution for init()
  BAIL_IF_FALSE(srcVideos.size() > 0, err, NVCV_ERR_MISSINGINPUT);
  src_captures[0] >> ocv_cpu;
  src_captures[0].set(cv::CAP_PROP_POS_FRAMES, 0);  // resetting to first frame
  if (!ocv_cpu.data) {
    printf("Cannot read video file \"%s\"\n", srcVideos[0]);
    BAIL(err, NVCV_ERR_READ);
  }
  NVWrapperForCVMat(&ocv_cpu, &nvcv_cpu);
  src_width = nvcv_cpu.width;
  src_height = nvcv_cpu.height;

  BAIL_IF_ERR(err = app->Init(num_video_streams));                 // Init effect
  BAIL_IF_ERR(err = app->AllocateBuffers(src_width, src_height));  // Allocate buffers
  BAIL_IF_ERR(err = app->SetParameters());                         // Set IO and config
  BAIL_IF_ERR(err = app->Load());                                  // Load the feature

  for (unsigned i = 0; i < num_video_streams; i++) {
    if (!src_captures[i].isOpened()) continue;  // if video is not opened, we skip
    src_captures[i] >> frames[i];
    if (frames[i].empty())                   // if nothing read
      src_captures[i].release();             // closing the video
    else                                     // if a frame is read
      BAIL_IF_ERR(app->InitVideoStream(i));  // initialize video stream
  }

  while (1) {
    unsigned active_video_count =
        0;  // counter to keep track of how many videos have not ended and still need to be processed
    for (unsigned i = 0; i < num_video_streams; i++) {
      if (src_captures[i].isOpened()) {
        src_captures[i] >> frames_t_1[i];  // Reading the next frame to know if the video has ended
                                           // as it is not possible to know if current frame is last
                                           // without reading the next frame
        if (frames_t_1[i].empty()) {
          BAIL_IF_ERR(app->ReleaseVideoStream(i));  // Trition requires NvVFX_DeallocateState() is called just before
                                                    // the last inference for that video stream
          src_captures[i].release();                // closing the video
        }
      }
      if (frames[i].empty()) continue;
      batch_indices[active_video_count] = i;

      NVWrapperForCVMat(&frames[i], &nvcv_cpu);
      if (!(nvcv_cpu.width == src_width && nvcv_cpu.height == src_height)) {
        printf(
            "Input video file \"%s\" %ux%u does not match %ux%u\n"
            "Batching requires all video frames to be of the same size\n",
            srcVideos[active_video_count], nvcv_cpu.width, nvcv_cpu.height, src_width, src_height);
        BAIL(err, NVCV_ERR_MISMATCH);
      }
      BAIL_IF_ERR(err = TransferToNthImage(active_video_count, &nvcv_cpu, &app->m_src, 1.f, app->m_stream, NULL));
      active_video_count++;
    }
    if (active_video_count == 0) goto bail;  // all videos have been processed

    // Run batch
    unsigned batchsize = active_video_count;  // processing a batch consisting of 1 frame from each of the active videos
    BAIL_IF_ERR(err = app->Run(batch_indices.data(), batchsize));
    // NvCVImage_Dealloc() is called in the destructors

    for (unsigned int i = 0; i < batchsize; ++i) {
      int video_idx = batch_indices[i];
      cv::Mat display_frame;
      BAIL_IF_ERR(err = app->GenerateNthOutputVizImage(video_idx, frames[video_idx], display_frame));
      dst_writers[video_idx] << display_frame;
    }

    // Update current frame
    for (unsigned i = 0; i < batchsize; i++) {
      unsigned video_idx = batch_indices[i];
      frames[video_idx] = frames_t_1[video_idx].clone();  // copying the t+1 frame to current frame
                                                          // for the videos that were processed
    }
  }
bail:
  for (auto& cap : src_captures) {
    if (cap.isOpened()) cap.release();
  }
  for (auto& writer : dst_writers) {
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

  vfxErr = BatchProcess(FLAG_effect.c_str(), FLAG_inFiles, FLAG_outFile.c_str(), FLAG_codec);

  if (NVCV_SUCCESS != vfxErr) {
    Usage();
    printf("Error: %s\n", NvCV_GetErrorStringFromCode(vfxErr));
    nErrs = (int)vfxErr;
  }

  return nErrs;
}
