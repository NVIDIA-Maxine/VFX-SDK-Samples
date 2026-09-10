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

#ifndef UNICODE_UTF8_UTILS_H_
#define UNICODE_UTF8_UTILS_H_

#include <stdio.h>
#include <string>

#ifdef _WIN32

/// Convert a UTF-8 string to a UTF-16 wide string.
/// @param[in] utf8  null-terminated UTF-8 string.
/// @return          the equivalent wide string, or empty on error.
std::wstring Utf8ToWide(const char* utf8);

/// Convert a UTF-16 wide string to a UTF-8 string.
/// @param[in] wide  null-terminated wide string.
/// @return          the equivalent UTF-8 string, or empty on error.
std::string WideToUtf8(const wchar_t* wide);

#endif  // _WIN32

/// Open a file given a UTF-8 path, using _wfopen on Windows.
/// On Linux, this is equivalent to fopen.
/// @param[in] path  UTF-8 file path.
/// @param[in] mode  fopen-style mode string (e.g. "rb", "wb").
/// @return          FILE pointer on success, nullptr on failure.
///                  Sets errno to EILSEQ if UTF-8 to UTF-16 conversion fails (Windows only).
FILE* Utf8FileOpen(const char* path, const char* mode);

#endif  // UNICODE_UTF8_UTILS_H_
