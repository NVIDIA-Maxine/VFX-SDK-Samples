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

#include "unicodeUtf8Utils.h"

#ifdef _WIN32
#include <Windows.h>

std::wstring Utf8ToWide(const char* utf8) {
  int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, nullptr, 0);
  if (len <= 0) return {};
  std::wstring wide(len - 1, L'\0');
  MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, utf8, -1, &wide[0], len);
  return wide;
}

std::string WideToUtf8(const wchar_t* wide) {
  int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, nullptr, 0, nullptr, nullptr);
  if (len <= 0) return {};
  std::string utf8(len - 1, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide, -1, &utf8[0], len, nullptr, nullptr);
  return utf8;
}

FILE* Utf8FileOpen(const char* path, const char* mode) {
  std::wstring wpath = Utf8ToWide(path);
  std::wstring wmode = Utf8ToWide(mode);
  if ((wpath.empty() && path && *path) || (wmode.empty() && mode && *mode)) {
    errno = EILSEQ;
    return nullptr;
  }
  return _wfopen(wpath.c_str(), wmode.c_str());
}

#else  // !_WIN32

FILE* Utf8FileOpen(const char* path, const char* mode) { return fopen(path, mode); }

#endif  // _WIN32
