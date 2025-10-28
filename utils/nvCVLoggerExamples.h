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

#ifndef __NVCVLOGGER_EXAMPLES__
#define __NVCVLOGGER_EXAMPLES__

#include <stdio.h>

#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>

#include "nvCVStatus.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A logger class that records all log records in a C++ string.                                 ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

class MemLogger {
 public:
  /// Default constructor.
  MemLogger() {}

  /// Access to the log string.
  /// @return a reference to the mutable log string.
  std::string& log() { return m_log; }

  /// C-style callback function, for the logger.
  /// @param[in,out]  userData  a pointer that will point to this instantiation.
  /// @param[in]      msg       the message to be appended to the log.
  static void Callback(void* userData, const char* msg);

 private:
  std::mutex m_mutex;  ///< The mutex to avoid collisions from different threads.
  std::string m_log;   ///< The accumulated log string.
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A logger class that records all log records to stderr.                                       ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

class StderrLogger {
 public:
  StderrLogger() {}
  static void Callback(void* userData, const char* msg);

 private:
  std::mutex m_mutex;  ///< The mutex to avoid collisions from different threads.
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A logger class that records all log records to a file.                                       ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

class FileLogger {
 public:
  /// Destructor
  ~FileLogger();

  /// Default constructor
  FileLogger();

  /// File initialization constructor
  /// @param[in]  file  the file to use for logging.
  /// @param[in]  mode  "w" or "a" (NULL implies "w").
  FileLogger(const char* file, const char* mode = nullptr);

  /// Initialization
  /// This can be called more than once, in which case it should flush the old one thej open the new one
  /// @param[in]  file  the file to use for logging.
  /// @param[in]  mode  "w" or "a" (NULL implies "w").
  /// @return NVCV_SUCCESS if successful, NVCV_ERR_FILE if not.
  NvCV_Status init(const char* file, const char* mode = nullptr);

  /// C-style callback function, for the logger.
  /// @param[in,out]  userData  a pointer that will point to this instantiation.
  /// @param[in]      msg       the message to be appended to the log.
  static void Callback(void* userData, const char* msg);

 private:
  std::mutex m_mutex;  ///< The mutex to avoid collisions from different threads.
  FILE* m_fd;          ///< The file descriptor.
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A more complex file logger callback, one that does the actual writing in a separate thread.  ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

class FileThreadLogger {
 public:
  /// Destructor
  ~FileThreadLogger();

  /// Default constructor
  FileThreadLogger();

  /// File initialization constructor
  /// @param[in]  file  the file to use for logging.
  /// @param[in]  mode  "w" or "a" (NULL implies "w").
  FileThreadLogger(const char* file, const char* mode = nullptr);

  /// Initialization
  /// This can be called more than once, in which case it should flush the old one then open the new one
  /// @param[in]  file  the file to use for logging.
  /// @param[in]  mode  "w" or "a" (NULL implies "w").
  /// @return NVCV_SUCCESS if successful, NVCV_ERR_FILE if not.
  NvCV_Status init(const char* file, const char* mode = nullptr);

  /// Log method for this C++ class.
  /// @param[in]  msg   The message to be appended to the log.
  void log(const char* msg);

  /// C-style callback function, for the logger.
  /// @param[in,out]  userData  a pointer that will point to this instantiation.
  /// @param[in]      msg       the message to be appended to the log.
  static void Callback(void* userData, const char* msg) {
    FileThreadLogger* ftl = (FileThreadLogger*)userData;
    ftl->log(msg);
  }

 private:
  /// Worker to be spawned off to another thread.
  void worker();

  /// C-style worker, to be employed by the thread.
  /// @param[in,out]  userData  a pointer that will point to this instantiation.
  static void Worker(void* userData) {
    FileThreadLogger* ftl = (FileThreadLogger*)userData;
    ftl->worker();
  }

  FILE* m_fd;                      ///< The file descriptor.
  std::string m_fileName;          ///< The name of the currently file open for logging.
  std::string m_buf[2];            ///< Buffer 0 is used by the clients, buffer 1 is used by the worker thread.
  std::thread m_thread;            ///< The thread of the worker.
  std::mutex m_mutex;              ///< The mutex.
  std::condition_variable m_cond;  ///< The condition variable.
  bool m_run;                      ///< A signal to tell the worker thread when to stop.
};

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Multifile logger.                                                                            ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

class MultifileLogger {
 public:
  /// Destructor
  ~MultifileLogger();

  /// Default constructor
  MultifileLogger();

  /// File initialization constructor
  /// @param[in]  proto     the prototype file to use for logging.
  /// @param[in]  max_size  the maximum size per each file.
  /// @param[in]  num_files the number of files to be used for the log.
  /// @param[in]  first     the index of the first file to be written.
  MultifileLogger(const char* proto, size_t max_size, unsigned num_files, unsigned first = 0);

  /// Initialization
  /// This can be called more than once, in which case it should flush the old one the open the new one
  /// @param[in]  proto     the prototype file to use for logging.
  /// @param[in]  max_size  the maximum size per each file.
  /// @param[in]  num_files the number of files to be used for the log.
  /// @param[in]  first     the index of the first file to be written.
  /// @return NVCV_SUCCESS if successful, NVCV_ERR_FILE if not.
  NvCV_Status init(const char* proto, size_t max_size, unsigned num_files, unsigned first = 0);

  /// Log method for this C++ class.
  /// @param[in]  msg   The message to be appended to the log.
  void log(const char* msg);

  /// C-style callback function, for the logger.
  /// @param[in,out]  userData  a pointer that will point to this instantiation.
  /// @param[in]      msg       the message to be appended to the log.
  static void Callback(void* userData, const char* msg) {
    MultifileLogger* mfl = (MultifileLogger*)userData;
    mfl->log(msg);
  }

 private:
  /// @param[in]  index  Open the log file with the specified index.
  NvCV_Status openLogFile(unsigned index);

  /// Write buffer
  /// @param[in]  buf   The buffer.
  /// @param[in]  size  The number of bytes in the buffer to write.
  void writeBuffer(const char* buf, size_t size);

  /// Worker to be spawned off to another thread.
  void worker();

  /// C-style worker, to be employed by the thread.
  /// @param[in,out]  userData  a pointer that will point to this instantiation.
  static void Worker(void* userData) {
    MultifileLogger* mfl = (MultifileLogger*)userData;
    mfl->worker();
  }

  FILE* m_fd;                      ///< The file descriptor.
  std::string m_fileProto;         ///< The prototype for the log files.
  std::string m_buf[2];            ///< Buffer 0 is used by the clients, buffer 1 is used by the worker thread.
  std::thread m_thread;            ///< The thread of the worker.
  std::mutex m_mutex;              ///< The mutex.
  std::condition_variable m_cond;  ///< The condition variable.
  unsigned m_numFiles;             ///< The number of file to be used in the log.
  size_t m_maxSize;                ///< The maximum size of each file.
  size_t m_currSize;               ///< The current size of the current file.
  unsigned m_currIndex;            ///< The index of the current file.
  bool m_run;                      ///< A signal to tell the worker thread when to stop.
};

#endif  // __NVCVLOGGER_EXAMPLES__
