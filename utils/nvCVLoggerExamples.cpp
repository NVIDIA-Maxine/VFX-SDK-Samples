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

#include "nvCVLoggerExamples.h"

#include <string.h>

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A logger class that records all log records in a C++ string.                                 ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

void MemLogger::Callback(void* userData, const char* msg) {
  MemLogger* ml = (MemLogger*)userData;
  if (msg) {  // if msg is NULL, it is requested to flush
    std::unique_lock<std::mutex> lock(ml->m_mutex);
    ml->m_log += msg;
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A logger class that records all log records to stderr.                                       ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

void StderrLogger::Callback(void* user_data, const char* msg) {
  StderrLogger* logger = (StderrLogger*)user_data;
  if (msg) {
    std::unique_lock<std::mutex> lock(logger->m_mutex);
    fputs(msg, stderr);  // It should be obvious how to log to stdout instead of stderr
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A logger class that records all log records to a file.                                       ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

FileLogger::~FileLogger() {
  if (m_fd) fclose(m_fd);
}

FileLogger::FileLogger() : m_fd(nullptr) {}

FileLogger::FileLogger(const char* file, const char* mode) : m_fd(nullptr) { init(file, mode); }

/// Initialization
/// This can be called more than once, in which case it should flush the old one thej open the new one
/// @param[in]  file  the file to use for logging.
/// @param[in]  mode  "w" or "a" (NULL implies "w").
/// @return NVCV_SUCCESS if successful, NVCV_ERR_FILE if not.
NvCV_Status FileLogger::init(const char* file, const char* mode) {
  NvCV_Status err = NVCV_SUCCESS;
  if (m_fd) {        // If a file was already open, ...
    fclose(m_fd);    // ... close it ..
    m_fd = nullptr;  // ... and forget it
  }
  if (file) {  // If we want to start logging to a regular file, ...
    if (!mode) mode = "w";
#ifndef _MSC_VER
    m_fd = fopen(file, mode);  // ... open it
#else                          // _MSC_VER
    fopen_s(&m_fd, file, mode);  // ... open it
#endif                         // _MSC_VER
    if (!m_fd)                 // If we were not successful opening the file, ...
      err = NVCV_ERR_FILE;     // ... we indicate an error
  }
  return err;
}

void FileLogger::Callback(void* user_data, const char* msg) {
  FileLogger* logger = (FileLogger*)user_data;
  std::unique_lock<std::mutex> lock(logger->m_mutex);
  if (logger->m_fd) {
    if (msg) {
      fputs(msg, logger->m_fd);
    } else {  // if msg is NULL, it is requested to flush and close
      fflush(logger->m_fd);
      fclose(logger->m_fd);
      logger->m_fd = nullptr;
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// A more complex file logger callback, one that does the actual writing in a separate thread.  ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

FileThreadLogger::~FileThreadLogger() {
  log(nullptr);  // A NULL log message is a signal to shut down
}

NvCV_Status FileThreadLogger::init(const char* file, const char* mode) {
  NvCV_Status err = NVCV_SUCCESS;
  std::unique_lock<std::mutex> lock(m_mutex);         // Require exclusive access while we reconfigure
  if (m_fd) {                                         // If a file was already open, ...
    fflush(m_fd);                                     // ... flush any unwritten data
    if (m_fd != stderr) {                             // For normal file output (not stderr), ...
      if (file && !strcmp(file, m_fileName.c_str()))  // ... if the currently open file is the same as the new one
        return NVCV_SUCCESS;  // ... no need to close and reopen (so different loggers can share)
      fclose(m_fd);           // Otherwise it is a different file so we close it ..
    }
    m_fd = nullptr;  // ... and forget it
  }
  m_fileName.clear();  // Forget any previously opened file
  if (file) {          // If we want to start logging to a regular file, ...
#ifndef _MSC_VER
    m_fd = fopen(file, (mode ? mode : "w"));  // ... open it
#else                                         // _MSC_VER
    fopen_s(&m_fd, file, (mode ? mode : "w"));  // ... open it
#endif                                        // _MSC_VER
    if (m_fd) {                               // If we were successful opening the file, ...
      m_fileName = file;                      // ... remember it
    } else {                                  // Otherwise, we failed to open the file
      err = NVCV_ERR_FILE;                    // We indicate an error
      m_fd = stderr;                          // Use the stderr instead, so we can report somewhere
    }
  } else {          // Otherwise, we want to start logging to stderr
    m_fd = stderr;  // So we make it happen!
  }
  return err;
}

FileThreadLogger::FileThreadLogger() : m_fd(stderr), m_run(true) {
  m_buf[0].reserve(2000);  // Pre-allocate buffer space so the threads don't need to
  m_buf[1].reserve(2000);
  m_thread = std::thread(&Worker, this);
}

FileThreadLogger::FileThreadLogger(const char* file, const char* mode) : m_fd(nullptr), m_run(true) {
  m_buf[0].reserve(2000);  // Pre-allocate buffer space so the threads don't need to
  m_buf[1].reserve(2000);
  (void)init(file, mode);  // init will have already squawked if the file could not be opened
  m_thread = std::thread(&Worker, this);
}

void FileThreadLogger::worker() {
  while (1) {  // Keep looking for work until asked to stop
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      if (!m_run) return;             // Exit if no longer running
      m_cond.wait(lock);              // Wait around until there is something to do
      std::swap(m_buf[0], m_buf[1]);  // The worker gets exclusive access to m_buf[1]
    }
    if (m_buf[1].size()) {                                      // This might be 0 when asked to stop
      (void)fwrite(m_buf[1].data(), 1, m_buf[1].size(), m_fd);  // TODO: check the returned value
      m_buf[1].clear();                                         // Empty the buffer so the clients can use it
    }
  }
}

void FileThreadLogger::log(const char* msg) {
  if (msg) {
    {
      std::unique_lock<std::mutex> lock(m_mutex);  // Assure exclusive access to the buffer
      m_buf[0] += msg;                             // Add the new data
    }
    m_cond.notify_one();  // Wake up the thread to print it
  } else {                // NULL msg is a signal to finish
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_run = 0;  // Tell the thread to quit
    }
    m_cond.notify_all();                       // Wake up the thread
    if (m_thread.joinable()) m_thread.join();  // and wait until it exits
    if (m_fd) {
      if (m_buf[0].size())                                  // If there is any residual unwritten data, ...
        fwrite(m_buf[0].data(), 1, m_buf[0].size(), m_fd);  // ... write it
      m_buf[0].clear();
      fflush(m_fd);                      // Flush it
      if (m_fd != stderr) fclose(m_fd);  // Close the file as long as it is not stderr
      m_fd = nullptr;
      m_fileName.clear();
    }
  }
}

////////////////////////////////////////////////////////////////////////////////////////////////////
/// Multifile Logger.                                                                            ///
/// This can be instantiated once and supplied several times as a callback to several SDKs.      ///
////////////////////////////////////////////////////////////////////////////////////////////////////

MultifileLogger::~MultifileLogger() {
  log(nullptr);  // A NULL log message is a signal to shut down
}

NvCV_Status MultifileLogger::openLogFile(unsigned index) {
  std::string file;
  std::unique_lock<std::mutex> lock(m_mutex);
  m_currIndex = index % m_numFiles;
  file.resize(1024);
  int n = snprintf(&file[0], 1, m_fileProto.c_str(), m_currIndex) + 1;
  file.resize(n);
  n = snprintf(&file[0], n, m_fileProto.c_str(), m_currIndex);
  file.resize(n);  // This should be 1 character smaller now
  if (m_fd) {      // If a file was already open, ...
    fflush(m_fd);  // ... flush any unwritten data
    fclose(m_fd);
    m_fd = nullptr;
  }
#ifndef _MSC_VER
  m_fd = fopen(file.c_str(), "wb");  // ... open it
#else                                // _MSC_VER
  n = fopen_s(&m_fd, file.c_str(), "wb");  // ... open it as binary to prevent Windows from adding CR
#endif                               // _MSC_VER
  m_currSize = 0;
  // if (!m_fd) fprintf(stderr, "Failed to open logfile \"%s\"\n", file.c_str());
  return m_fd ? NVCV_SUCCESS : NVCV_ERR_FILE;
}

NvCV_Status MultifileLogger::init(const char* proto, size_t max_size, unsigned num_files, unsigned first) {
  m_fileProto = proto;  // TODO: assure that this has one %d, %i or %u
  m_maxSize = max_size;
  m_numFiles = num_files;
  return openLogFile(first);
}

MultifileLogger::MultifileLogger()
    : m_fd(nullptr), m_run(true), m_maxSize(0), m_numFiles(0), m_currIndex(0), m_currSize(0) {
  m_buf[0].reserve(2000);  // Pre-allocate buffer space so the threads don't need to
  m_buf[1].reserve(2000);
  m_thread = std::thread(&Worker, this);
}

MultifileLogger::MultifileLogger(const char* proto, size_t max_size, unsigned num_files, unsigned first)
    : m_fd(nullptr), m_run(true), m_maxSize(0), m_numFiles(0), m_currIndex(0), m_currSize(0) {
  m_buf[0].reserve(2000);  // Pre-allocate buffer space so the threads don't need to
  m_buf[1].reserve(2000);
  NvCV_Status err =
      init(proto, max_size, num_files, first);  // init will have already squawked if the file could not be opened
  if (NVCV_SUCCESS != err) return;
  m_thread = std::thread(&Worker, this);
}

void MultifileLogger::writeBuffer(const char* buf, size_t size) {
  NvCV_Status err;
  if (m_currSize >= m_maxSize) {
    err = openLogFile(m_currIndex + 1);
    if (NVCV_SUCCESS != err) return;
  }
  while ((m_currSize + size) > m_maxSize) {
    const char *s, *send;
    for (s = (send = buf - 1) + m_maxSize - m_currSize; s != send; --s) {
      if ('\n' == *s) {  // Make sure to write complete lines
        size_t z = s - send;
        (void)fwrite(buf, 1, z, m_fd);  // TODO: check the returned value -- but what can we do?
        buf += z;
        size -= z;
        m_currSize += z;
        err = openLogFile(m_currIndex + 1);  // TODO: check the returned value -- but what could we do?
        if (NVCV_SUCCESS != err) return;
        break;
      }
    }
    if (send != s) continue;               // Continue splitting until less than m_maxSize to write.
    if (m_currSize) {                      // Can't find a line short enough to write, ...
      err = openLogFile(m_currIndex + 1);  // ... so open up a new empty file ...
      if (NVCV_SUCCESS != err) return;
      continue;  // ... and write at least one line the next time through
    } else {     // Can't write even one line into an empty file without exceeding the file size (uncommon)
      (void)fwrite(buf, 1, size, m_fd);  // Write the whole buffer anyway
      m_currSize += size;
      // openLogFile(m_currIndex + 1);       // This is covered by the first statement in this function
      return;
    }
  }
  if (size) {                          // We can write the whole buffer without exceeding the size
    (void)fwrite(buf, 1, size, m_fd);  // TODO: check the returned value
    m_currSize += size;
  }
}

void MultifileLogger::worker() {
  while (1) {  // Keep looking for work until asked to stop
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      if (!m_run) return;             // Exit if no longer running
      m_cond.wait(lock);              // Wait around until there is something to do
      std::swap(m_buf[0], m_buf[1]);  // The worker gets exclusive access to m_buf[1]
    }
    if (m_buf[1].size()) {                            // This might be 0 when asked to stop
      writeBuffer(m_buf[1].data(), m_buf[1].size());  // TODO: check the returned value
      m_buf[1].clear();                               // Empty the buffer so the clients can use it
    }
  }
}

void MultifileLogger::log(const char* msg) {
  if (msg) {
    {
      std::unique_lock<std::mutex> lock(m_mutex);  // Assure exclusive access to the buffer
      m_buf[0] += msg;                             // Add the new data
    }
    m_cond.notify_one();  // Wake up the thread to print it
  } else {                // NULL msg is a signal to finish
    {
      std::unique_lock<std::mutex> lock(m_mutex);
      m_run = 0;  // Tell the thread to quit
    }
    m_cond.notify_all();                       // Wake up the thread
    if (m_thread.joinable()) m_thread.join();  // and wait until it exits
    if (m_fd) {
      if (m_buf[0].size())                              // If there is any residual unwritten data, ...
        writeBuffer(m_buf[0].data(), m_buf[0].size());  // ... write it
      fflush(m_fd);                                     // Flush it
      if (m_fd != stderr) fclose(m_fd);                 // Close the file as long as it is not stderr
      m_fd = nullptr;
    }
  }
}
