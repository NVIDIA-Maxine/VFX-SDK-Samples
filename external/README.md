# Building OpenCV 4.12.0 + `opencv_videoio_ffmpeg4120.dll` for Windows on ARM64

A single source build that produces **both** `opencv_world4120.dll` and the FFmpeg video I/O plugin,
built natively on a Windows-on-ARM machine.

Clone OpenCV, unpack a prebuilt `winarm64` FFmpeg, then configure one CMake tree that emits the world
DLL plus a *separate* `opencv_videoio_ffmpeg.dll` — so FFmpeg is never linked into
`opencv_world4120.dll` — and finally build, install, copy both alongside FFmpeg's runtime DLLs into
`opencv/arm64/bin`, and rename the plugin to `opencv_videoio_ffmpeg4120.dll`, where this repository's
`external/CMakeLists.txt` expects them.

## Requirements

**An ARM64 Windows host.** These instructions are native-only: cross-compiling from x64 is not
supported here, and every step below assumes the machine you are typing on is Windows on ARM.

* Visual Studio 2022 with **MSVC v143 – VS 2022 C++ ARM64/ARM64EC build tools**. VS 2019's ARM64
  compiler has NEON bugs that break this build.
* Git and CMake 3.20 or newer on `PATH`.

Run every command from this `external` directory — the relative paths in Step 3 depend on it.

## Automated build

`build-opencv-arm64.ps1` performs Steps 1–5 end to end.

```powershell
.\build-opencv-arm64.ps1
```

It is resumable: already-completed steps are skipped unless `-Force` is passed. Intermediates
(`opencv-4.12.0`, `ffmpeg-arm64`, `build-opencv`, `install-opencv`) are left in this directory and
can be deleted once `opencv/arm64` is populated. The rest of this document is what the script does,
step by step.

---

## Step 1 — Get the sources

```powershell
git clone --depth 1 --branch 4.12.0 https://github.com/opencv/opencv.git opencv-4.12.0
```

## Step 2 — Get FFmpeg binaries for Windows on ARM

```powershell
$url = "https://github.com/BtbN/FFmpeg-Builds/releases/download/autobuild-2026-06-30-13-34/ffmpeg-n7.1.5-1-g7d0e842004-winarm64-gpl-shared-7.1.zip"
Invoke-WebRequest -Uri $url -OutFile ffmpeg-n71.zip
Expand-Archive ffmpeg-n71.zip -DestinationPath . -Force
Rename-Item ffmpeg-n7.1.5-1-g7d0e842004-winarm64-gpl-shared-7.1 ffmpeg-arm64
```

## Step 3 — Configure

```powershell
cmake -S opencv-4.12.0 -B build-opencv -A ARM64 -T host=ARM64 `
  "-DCMAKE_INSTALL_PREFIX=install-opencv" `
  -DCPU_BASELINE=NEON -DCPU_DISPATCH= `
  -DBUILD_SHARED_LIBS=ON -DBUILD_opencv_world=ON `
  -DBUILD_TESTS=OFF -DBUILD_PERF_TESTS=OFF -DBUILD_EXAMPLES=OFF -DBUILD_DOCS=OFF `
  -DBUILD_opencv_apps=OFF -DBUILD_opencv_python3=OFF -DBUILD_JAVA=OFF `
  -DWITH_FFMPEG=ON -DOPENCV_FFMPEG_SKIP_DOWNLOAD=ON `
  -DOPENCV_FFMPEG_USE_FIND_PACKAGE=FFMPEG `
  "-DCMAKE_MODULE_PATH=cmake-modules" `
  "-DFFMPEG_ROOT=ffmpeg-arm64" `
  -DVIDEOIO_PLUGIN_LIST=ffmpeg `
  -DOPENCV_PLUGIN_VERSION= -DOPENCV_PLUGIN_ARCH= -DOPENCV_PLUGIN_SUFFIX=4120`
  -DWITH_GSTREAMER=OFF -DWITH_QT=OFF -DWITH_1394=OFF `
  -DWITH_IPP=OFF -DBUILD_IPP_IW=OFF -DWITH_ITT=OFF -DBUILD_ITT=OFF `
  -DWITH_JASPER=OFF -DOPENCV_ENABLE_NONFREE=OFF -DWITH_CAROTENE=OFF `
  -DWITH_CUDA=OFF -DWITH_OPENCL=OFF -DCMAKE_DISABLE_FIND_PACKAGE_OpenCL=ON `
  -DWITH_OBSENSOR=OFF
```

Read the compiler path in the configure output:

```
-- Check for working CXX compiler: .../VC/Tools/MSVC/<ver>/bin/HostARM64/arm64/cl.exe
```

`HostARM64/arm64` is correct. `Hostx86/arm64` means the ARM64 host tools are missing — install
**MSVC v143 – VS 2022 C++ ARM64/ARM64EC build tools** in the Visual Studio Installer.

Confirm FFmpeg detection too. Empty or absent versions mean it failed:

```
-- Video I/O: add builtin plugin 'opencv_videoio_ffmpeg'
--     FFMPEG:                      YES (find_package)
--       avcodec:                   YES (61.19.101)
--       avformat:                  YES (61.7.103)
--       avutil:                    YES (59.39.100)
--       swscale:                   YES (8.3.100)
```

## Step 4 — Build and install

```powershell
cmake --build build-opencv --config Release --target INSTALL --parallel
```

## Step 5 — Deploy

Everything lands in **one folder**, `opencv\arm64\bin`: the folder is
also the `DYNAMIC_LIBRARY_DIR` the `opencv4120` target in `external/CMakeLists.txt` points the sample
apps at. OpenCV installs the plugin as `opencv_videoio_ffmpeg.dll`; rename it to the versioned
`opencv_videoio_ffmpeg4120.dll` name the runtime looks for next to `opencv_world4120.dll`.

```powershell
$arch = "opencv\arm64"

Copy-Item install-opencv\bin\*.dll            $arch\bin   # opencv_world4120 + opencv_videoio_ffmpeg
Copy-Item ffmpeg-arm64\bin\*.dll              $arch\bin   # avcodec-61, avformat-61, avutil-59, swscale-8, ...
Rename-Item $arch\bin\opencv_videoio_ffmpeg.dll opencv_videoio_ffmpeg4120.dll
```

## Troubleshooting

**`FFMPEG: NO (find_package)`** — `FindFFMPEG.cmake` was not on `CMAKE_MODULE_PATH`. CMake reports
this as "By not providing FindFFMPEG.cmake ... could not find a package configuration file", which
misleadingly suggests you need an `FFMPEGConfig.cmake`. Usually the path argument was unquoted and
PowerShell left a `$variable` unexpanded, or you ran the command from outside this `external`
directory.

**`FFMPEG: NO` with versions listed** — the version check or the `try_compile` in
`detect_ffmpeg.cmake` failed. `-DOPENCV_FFMPEG_SKIP_BUILD_CHECK=ON` skips the latter to isolate it.

**Backend reports `MSMF`, or capture fails to open** — the plugin loaded but its `av*.dll`
dependencies are not on the search path, or the plugin kept its versioned filename. Set
`$env:OPENCV_VIDEOIO_DEBUG = "1"` and `$env:OPENCV_LOG_LEVEL = "DEBUG"` to log every candidate path
and separate "file not found" from "found but failed to load". `OPENCV_FFMPEG_DLL_DIR` overrides the
directory searched, and `OPENCV_VIDEOIO_PLUGIN_FFMPEG` overrides the expected filename outright —
useful if you did keep the default versioned name.

**Link errors against `avcodec.lib`** — you downloaded a `win64` (x64) package instead of
`winarm64`, or a static variant instead of `shared`.

**CMake 4.x rejects `cmake_minimum_required(VERSION 3.5)`** — add `-DCMAKE_POLICY_VERSION_MINIMUM=3.5`.
