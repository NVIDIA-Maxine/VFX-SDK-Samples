TrueHDREffectApp
================

TrueHDREffectApp is a sample application that demonstrates the TrueHDR (SDR to HDR conversion) feature of the NVIDIA Video Effects SDK.

Quickstart
----------

This section provides an end-to-end overview of the setup required to build and run this sample application. Complete each step in order for your target platform before proceeding to the detailed sections below.

### Before you begin

Complete the following prerequisites in order. For platform-specific steps, use the link that matches your target operating system. Use the online doc version corresponding to your SDK version (default to "latest").

1. **Verify hardware and software requirements.**
   - Windows: [Get Started on Windows](https://docs.nvidia.com/maxine/vfx/latest/WindowsVFXSDK/GetStartedonWindows.html)
   - Linux: [Get Started on Linux](https://docs.nvidia.com/maxine/vfx/latest/LinuxVFXSDK/GetStartedonLinux.html)

   > **Note:** The Get Started pages linked above define the SDK-wide baseline. Individual features may impose additional NVIDIA hardware and driver requirements beyond this baseline. Review the feature-specific notes later in this README before deploying.

2. **Download and install the VFX SDK Core and the required feature(s).**
   - Windows: [Install the VFX SDK (Windows)](https://docs.nvidia.com/maxine/vfx/latest/WindowsVFXSDK/InstalltheVFXSDK.html)
   - Linux: [Install the VFX SDK (Linux)](https://docs.nvidia.com/maxine/vfx/latest/LinuxVFXSDK/InstalltheVFXSDK.html)
3. **Download the sample application source code and build the samples.**
   - Source: [NVIDIA-Maxine/VFX-SDK-Samples on GitHub](https://github.com/NVIDIA-Maxine/VFX-SDK-Samples)
4. **Run the sample application.**
   - Reference: see the remainder of this README for the supported command-line arguments and expected behavior.

### Running the sample application

Once the SDK and sample applications are installed, refer to the remaining sections of this README for the complete list of command-line arguments and run-time behavior. Certain workflows may require additional tooling (for example, FFmpeg for high-quality video encoding); see the additional dependencies section below when applicable.

For convenience, this application ships with platform-specific launcher scripts — a `.bat` file on Windows and a `.sh` file on Linux — that invoke the executable with a default asset and configure the required runtime environment variables. When run in offline mode (any non-webcam input), these scripts also produce an output file for playback.

Feature Details
---------------
TrueHDREffectApp reads an 8-bit SDR image or video (any container OpenCV can decode), runs the NVIDIA NGX TrueHDR Algorithm (deep-learning de-banding and dynamic reverse tone-mapping), and writes the resulting HDR10 frames to disk as packed 10-bit-per-channel RGB10A2. The TrueHDR effect operates on already-decoded 8-bit SDR RGBA pixels, so the input codec/container is not restricted.

Output frames are written verbatim to the path passed via `--out_file` as a single headerless raw stream of packed `R10G10B10A2` pixels (4 bytes/pixel, no row padding, dimensions match the input). Because the file has no container header, any downstream tool needs the width, height, and framerate supplied explicitly. The bundled `run_truehdreffectapp_video.sh` / `.bat` (for the sample MP4) and `run_truehdreffectapp_image.sh` / `.bat` (for the sample JPG) runner scripts hard-code these values at the top and will *optionally* package the `.r10` into a playable HDR10-tagged HEVC `.mp4` using `ffmpeg` via the shared `package_hdr10.sh` / `.bat` helper (see [Optional: HDR10 playback packaging](#optional-hdr10-playback-packaging) below).

Please note a minimum NVIDIA driver version is required to run the TrueHDR effect. Please refer to https://docs.nvidia.com/maxine/vfx/latest/Filters/TrueHDR.html for details.

Required Features
-----------------
This app requires the following feature to be installed. Make sure to install it using *install_feature.ps1* (Windows) or *install_feature.sh* (Linux) in your VFX SDK features directory before building.
- nvVFXTrueHDR

Additional Dependencies
-----------------------

TrueHDREffectApp decodes video input through OpenCV's `VideoCapture`, which needs the OpenCV FFmpeg backend library. The optional HDR10 packaging step shells out to the `ffmpeg` command-line tool, which must be version 5.0 or newer and built with `libx265` -- see [Optional: HDR10 playback packaging](#optional-hdr10-playback-packaging).

### Windows x64

#### FFmpeg command-line tool

1. Download `ffmpeg-release-essentials.zip` from https://www.gyan.dev/ffmpeg/builds/. These builds are 7.x and include `libx265`.
2. Extract the archive and locate `ffmpeg.exe` in its `bin` directory.
3. Copy `ffmpeg.exe` into the same directory as `TrueHDREffectApp.exe`, or add its `bin` directory to `PATH`.

#### OpenCV FFmpeg backend library

The backend library is required for `cv::VideoCapture` to decode video input. Image input does not need it:

1. Download `opencv-3.4.6-vc14_vc15.exe` from https://sourceforge.net/projects/opencvlibrary/files/3.4.6/opencv-3.4.6-vc14_vc15.exe/download.
2. Run the downloaded executable to extract the OpenCV files.
3. Locate `opencv_ffmpeg346_64.dll` under `opencv\build\bin`.
4. Copy `opencv_ffmpeg346_64.dll` into the runtime directory containing `opencv_world346.dll`.

These OpenCV instructions apply to Windows x64 builds using OpenCV 3.4.6. Other OpenCV versions require the matching backend library; for Windows on ARM, see the next section.

### Windows ARM64

#### OpenCV FFmpeg backend library

There are no prebuilt OpenCV binaries with FFmpeg support for Windows on ARM. The x64 packages above cannot be used, so OpenCV has to be built from source on an ARM64 machine. That build/deploy flow produces `opencv_world4120.dll` plus a separate `opencv_videoio_ffmpeg4120.dll` plugin, which is the ARM64 counterpart of `opencv_ffmpeg346_64.dll`.

[`external/README.md`](../../external/README.md) walks through the build step by step -- clone OpenCV, unpack the `winarm64` FFmpeg, configure with the native ARM64 toolchain, build, and deploy. `external/build-opencv-arm64.ps1` automates all of it:

```powershell
cd external
.\build-opencv-arm64.ps1
```

The script runs only on an ARM64 host and stops with an error on x64. It leaves `opencv_world4120.dll`, `opencv_videoio_ffmpeg4120.dll`, and FFmpeg's `av*` runtime DLLs together in `external\opencv\arm64\bin`, where the sample app build picks up its OpenCV runtime.

#### FFmpeg command-line tool

The `winarm64` FFmpeg package unpacked during that build also ships the command-line tool: `ffmpeg.exe` is in `external\ffmpeg-arm64\bin`. Copy it next to `TrueHDREffectApp.exe`, or add that `bin` directory to `PATH`. No separate download is needed. It is a GPL build of FFmpeg 7.1, so it satisfies the `>= 5.0` requirement; confirm the encoder is present with `ffmpeg -hide_banner -encoders | findstr x265`.

### Linux

Install the FFmpeg command-line tools and libraries:

```bash
sudo apt install ffmpeg
```

On Ubuntu 22.04 this installs 4.4.2, which is too old for the HDR10 packaging step. See the version table in [Optional: HDR10 playback packaging](#optional-hdr10-playback-packaging).

TrueHDREffectApp Command-Line Reference
---------------------------------------

| Argument                              | Description |
|---------------------------------------|-------------|
| `--in_file=<path>`                    | 8-bit SDR input file (image: `.bmp` / `.jpg` / `.png`; video: any container OpenCV can decode; or - with `--in_raw_rgba8` - a concatenated raw RGBA8 byte stream). |
| `--out_file=<path>`                   | Path (including extension) for the headerless raw HDR10 output. Written verbatim; you supply the file ending. The stream is packed `R10G10B10A2`, dimensions match `--in_file`. |
| `--in_raw_rgba8=<W>x<H>`              | Interpret `--in_file` as a headerless raw RGBA8 byte stream of `<W>x<H>` interleaved frames (4 bytes/pixel, no row padding). Frame count is inferred from the file size. |
| `--hdr_contrast=<N>`                  | HDR contrast tunable. Range `0..200`, default `100`. |
| `--hdr_saturation=<N>`                | HDR saturation tunable. Range `0..200`, default `100`. |
| `--hdr_middle_gray=<N>`               | HDR middle-grey reference. Range `10..100`, default `50`. |
| `--hdr_max_luminance=<N>`             | Target HDR display peak luminance, in nits. Range `400..2000`, default `650`. |
| `--hdr_debanding_off={0\|1}`          | Set to `1` to skip the DL-Debander pass for higher throughput on clean sources. Default `0`. |
| `--use_pinned_memory[={true\|false}]` | Allocate input and output GPU buffers as `NVCV_CPU_PINNED` to remove one `cuMemcpy2DAsync` per frame at fwrite time. |
| `--progress`                          | Show progress. |
| `--log=<file>`                        | Log SDK errors to a file, `"stderr"` or `""` (default `stderr`). |
| `--log_level=<N>`                     | The desired log level: `{0, 1, 2, 3}` = `{FATAL, ERROR, WARNING, INFO}` (default `1`). |
| `--verbose`                           | Verbose output. |
| `--help`                              | Display help information. |

> To preview the results -- use a calibrated HDR display via the HDR10-tagged `.mp4` produced by the runner script.

Optional: HDR10 playback packaging
----------------------------------

The `run_truehdreffectapp_video.sh` / `.bat` (video) and `run_truehdreffectapp_image.sh` / `.bat` (image) runner scripts each set `WIDTH`, `HEIGHT`, and `FPS` for the bundled sample input at the top of the file, then invoke the shared `package_hdr10.sh` / `.bat` helper which *optionally* packages the raw `.r10` output into a playable HDR10-tagged HEVC `.mp4` via `ffmpeg`. If you swap in a different input, update the three variables at the top of the runner to match; a wrong `WIDTH`/`HEIGHT` will produce a corrupt `.mp4` even though the `.r10` bytes are fine. The image runner passes `FPS=1` so single-frame outputs are held for ~1s and remain openable in HDR10 players. This step is opt-in by virtue of having `ffmpeg >= 5.0` on `PATH` (the packed-RGB10A2 input pixel format `x2bgr10le` is only available in FFmpeg 5.0 or newer).

When ffmpeg is usable, the helper writes `<out_file>.mp4` alongside the `.r10`: HEVC Main10 (`libx265` + `yuv420p10le`), tagged Rec.2020 primaries / SMPTE ST 2084 (PQ) transfer / BT.2020 NCL matrix. When it isn't, the raw `.r10` is left in place and the runner still succeeds; only genuine failures propagate.

Runner exit codes (`package_hdr10` also uses `2` / `3` / `4` / `5` when run standalone):

| Code | Meaning |
|:---:|---|
| `0` | Success. `.r10` non-empty; `.mp4` non-empty when `ffmpeg >= 5.0` (with `x2bgr10le`) was on `PATH`, absent on soft-skip. |
| `1` | Runner-side postcondition failed: `.r10` missing / empty after the app returned `0`, or `.mp4` missing / empty after `package_hdr10` returned `0`. Partial outputs are cleaned up. |
| `2` | `package_hdr10` invoked with bad arguments. |
| `3` | `package_hdr10` mandatory `.r10` input missing. |
| `4` | ffmpeg was invoked but the encode failed. `.r10` preserved, no `.mp4`. |
| `5` | Soft-skip: `ffmpeg` missing or lacks `x2bgr10le`. `package_hdr10` standalone returns `5`; the runner folds this into its own `0`. `.r10` preserved, no `.mp4`. |
| other | `TrueHDREffectApp`'s own exit code, propagated verbatim (e.g. app init / decode failures). |

Also `ffprobe` the `.mp4` to verify resolution, frame count, `pix_fmt=yuv420p10le`, and HDR10 tags (`color_transfer=smpte2084`, `color_primaries=bt2020`, `color_space=bt2020nc`).

The helper's signature is `package_hdr10.sh <r10_path> <width> <height> [framerate]`; run it standalone against any TrueHDREffectApp output when you know the dimensions.

Install `ffmpeg >= 5.0`:

| Platform         | Install command |
|------------------|-----------------|
| Ubuntu 22.04     | apt ships 4.4.2 (too old); use a static build from <https://johnvansickle.com/ffmpeg/> or a snap (`sudo snap install ffmpeg`). |
| Ubuntu 24.04+    | `sudo apt-get install ffmpeg` (ships 6.x/7.x). |
| Fedora / RHEL    | `sudo dnf install ffmpeg-free` (or `ffmpeg` from RPM Fusion). |
| macOS (Homebrew) | `brew install ffmpeg`. |
| Windows          | See [Additional Dependencies](#additional-dependencies) for x64 and ARM64. Either way the build must be `libx265`-enabled. |

To package an already-produced `.r10` by hand, mirror what the runner script does (substitute the actual `W`, `H`, `FPS`, and filename):

```
ffmpeg -y -f rawvideo -pixel_format x2bgr10le -video_size <W>x<H> -framerate <FPS> \
       -i <path>.r10 \
       -c:v libx265 -pix_fmt yuv420p10le \
       -x265-params 'crf=20:colorprim=bt2020:transfer=smpte2084:colormatrix=bt2020nc:hdr-opt=1' \
       <path>.mp4
```

The default recipe encodes **HEVC Main10 4:2:0 YUV** because that is the de-facto HDR10 distribution format -- consumer HDR10 TVs, Blu-ray UHD players, and streaming chains all decode HEVC Main10 YUV; the HEVC Main 4:4:4 10 Intra (RGB) profile is rarely supported. The RGB &rarr; YUV conversion uses the BT.2020 NCL matrix; YUV output defaults to limited range. The raw `.r10` itself is full-range packed RGB (`R10G10B10A2`, with the 2-bit alpha unused), which is exactly the `x2bgr10le` input layout above.

If you need an **RGB pass-through** through HEVC (for reference / pro decoders only), encode to the Format Range Extensions GBR profile instead. The pixel data round-trips without a colour-space conversion:

```
ffmpeg -y -f rawvideo -pixel_format x2bgr10le -video_size <W>x<H> -framerate <FPS> \
       -i <path>.r10 \
       -c:v libx265 -pix_fmt gbrp10le \
       -x265-params 'crf=20:colorprim=bt2020:transfer=smpte2084:range=full:hdr-opt=1' \
       <path>.mp4
```

Note that the resulting `.mp4` will **not** play on consumer HDR10 TVs.

Examples
--------

The direct-invocation examples below are shown for Linux (run from the app directory with `./TrueHDREffectApp`); on Windows use `.\TrueHDREffectApp.exe` (works in both PowerShell and `cmd.exe`; `cmd.exe` also accepts the shorter `TrueHDREffectApp.exe` because it searches the current directory by default). Values shown in angle brackets (`<...>`) are placeholders — replace them with your own paths / numbers before running. Filenames that are not bracketed (e.g. `input_truehdr.mp4`, `input1.jpg`) refer to fixtures bundled next to the binary at build time and can be used verbatim.

Default-tunable end-to-end run on the bundled sample video clip (`input_truehdr.mp4` is copied next to the binary at build time; the bundled clip is 852x480, H.264, 60fps). The `run_truehdreffectapp_video.sh` / `.bat` runner runs this and packages the `.r10` at 60fps if `ffmpeg >= 5.0` is on `PATH`:

```
./TrueHDREffectApp --in_file=input_truehdr.mp4 --out_file=output_truehdr_video.r10
```

Default-tunable end-to-end run on the bundled sample image (`input1.jpg` is copied next to the binary at build time; the bundled still is 1280x720). The `run_truehdreffectapp_image.sh` / `.bat` runner runs this and packages the single-frame `.r10` at 1fps so the still is held for ~1s:

```
./TrueHDREffectApp --in_file=input1.jpg --out_file=output_truehdr_image.r10
```

Contrast-boosted run on the bundled sample clip (same input as the first example, with a non-default tunable):

```
./TrueHDREffectApp --in_file=input_truehdr.mp4 --out_file=output_boosted.r10 \
    --hdr_contrast=150
```

Higher-throughput run on a clean SDR source — skip the DL-Debander with `--hdr_debanding_off=1` and enable pinned-memory output. `--hdr_debanding_off=1` is intended for content that is already free of banding artefacts (a graded master, a lightly-compressed high-bitrate SDR encode, or synthetic renders); applying it to a heavily-compressed source will leave visible gradient banding in the HDR result, so the bundled sample clip is *not* an appropriate input for this flag. Bring your own asset that satisfies the input requirements — any container OpenCV can decode (H.264 / H.265 / etc. in `.mp4` / `.mov` / `.mkv` / …), 8-bit 4:2:0 SDR, BT.709 primaries and transfer, any resolution the GPU can hold — and adjust the output name:

```
./TrueHDREffectApp --in_file=<your-clean-sdr-clip.mp4> --out_file=<output>.r10 \
    --hdr_debanding_off=1 --use_pinned_memory
```

Raw RGBA8 input (bypasses OpenCV decode). Useful when you already have decoded RGBA frames in memory or in a headerless byte stream. TrueHDREffectApp expects the file to be a concatenation of `<W>x<H>x4`-byte RGBA8 frames with no row padding and no container header; frame count is inferred from the file size, which must be an exact multiple of `<W>x<H>x4`.

To generate a raw-RGBA fixture from the bundled MP4 using `ffmpeg` (852x480 at ~45s / 60fps → a multi-gigabyte file, roughly 4 GB):

```bash
ffmpeg -y -i input_truehdr.mp4 -pix_fmt rgba -f rawvideo bundled_frames.rgba
```

Then run TrueHDR against the prepared fixture (matching `--in_raw_rgba8=<W>x<H>` to the bundled clip's resolution):

```
./TrueHDREffectApp --in_file=bundled_frames.rgba --in_raw_rgba8=852x480 \
    --out_file=output_raw.r10
```

Or with your own raw-RGBA asset (set `<W>x<H>` to the source resolution used when the frames were produced):

```
./TrueHDREffectApp --in_file=<your-frames>.rgba --in_raw_rgba8=<W>x<H> \
    --out_file=<output>.r10
```
