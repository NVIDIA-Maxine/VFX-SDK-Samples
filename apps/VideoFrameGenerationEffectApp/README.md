VideoFrameGenerationEffectApp
==============================

VideoFrameGenerationEffectApp is a sample application that demonstrates the Video Frame Generation feature of the NVIDIA Video Effects SDK. It reads a video file and generates one or more interpolated frames between each pair of consecutive input frames. 


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

VideoFrameGenerationEffectApp supports two operating modes -- pick either `--multiplier` or `--timesteps`, not both (passing both is a command-line error; if you pass neither, the default multiplier is used):

- **Multiplier mode** (default): supply `--multiplier=N` for uniform Nx frame-rate upscaling. `N - 1` evenly spaced frames are generated between every pair of input frames.
- **Timestep mode**: supply `--timesteps=t1,t2,...` to generate frames at arbitrary positions in the open interval `(0.0, 1.0)`, where `t = 0.0` is the position of the previous input frame and `t = 1.0` is the position of the current input frame (so `t = 0.5` is exactly the midpoint between them). One frame is generated per listed `t`, in the order listed -- supply ascending values for monotonic playback.

In addition to the video path, the sample accepts a pair of 16-bit PNG or TIFF still images through `--input_frame_prev` and `--input_frame_current`. This image-pair path uses packed `NVCV_RGB10A2` images for both effect inputs and every generated output. Supply `--output_frame_path` with a `.png`, `.tif`, or `.tiff` extension. One generated image is written for every requested timestep; in multiplier mode that is `N - 1` images. When multiple images are produced, `_1`, `_2`, and so on are inserted before the output extension.

> **Note**: in multiplier mode the SDK walks 1-based frame indices `i = 1, 2, ..., N - 1` and maps each to a timestep `t = i / N`. So `--multiplier=4` walks `i = 1, 2, 3` and produces `t = 1/4, 2/4, 3/4 = 0.25, 0.5, 0.75` -- the same result as `--timesteps=0.25,0.5,0.75`. Use `--multiplier` when the spacing is uniform; use `--timesteps` for specific, non-uniform positions.

The output frame rate defaults to `input_fps x multiplier` in multiplier mode, or `input_fps x (1 + number of timesteps)` in timestep mode. `--target_fps` overrides this default. Setting `--target_fps` lower than the default yields slow-motion playback (e.g. a 24 fps source with `--multiplier=4 --target_fps=24` produces 4x slow motion at the original 24 fps display rate).

Please note a minimum NVIDIA driver version is required to run the Video Frame Generation effect. Please refer to https://docs.nvidia.com/maxine/vfx/latest/Filters/VideoFrameGeneration.html for details.

Required Features
-----------------
This app requires the following feature to be installed. Make sure to install it using *install_feature.ps1* (Windows) or *install_feature.sh* (Linux) in your VFX SDK features directory before building.
- nvVFXVideoFrameGeneration

Additional Dependencies
-----------------------

By default, the Windows and Linux video runners use the direct OpenCV video-output path to write each final H.264 MP4 without creating or transcoding a temporary file.

On Windows, the OpenCV video backend uses Microsoft Media Foundation. This backend can fail to encode output at higher bitrates, frame rates, or resolutions. Review the FFmpeg instructions for [Windows x64](#ffmpeg-command-line-tool-windows-x64) or [Windows ARM64](#ffmpeg-command-line-tool-windows-arm64) below. Build FFmpeg and OpenCV with FFmpeg support when suitable packages are not available for the target platform.

For a high-quality encode, pass `--high-quality` to the video runner. This alternate path writes each generated video to a temporary lossless Ut Video RGB (`ULRG`) AVI and then uses FFmpeg with `libx264` to create the final H.264 MP4. It avoids relying on the platform video backend for the final encode and preserves the generated frames without introducing encoder artifacts in the intermediate file. The final encode uses `CRF 1`, a near-lossless `libx264` quality setting. Use this path to minimize encoding artifacts in VFG-generated frames.

### Windows x64

<a id="ffmpeg-command-line-tool-windows-x64"></a>
#### FFmpeg command-line tool

1. Download `ffmpeg-release-essentials.zip` from https://www.gyan.dev/ffmpeg/builds/.
2. Extract the archive and locate `ffmpeg.exe` in its `bin` directory.
3. Copy `ffmpeg.exe` into the same directory as `VideoFrameGenerationEffectApp.exe`, or add its `bin` directory to `PATH`.

#### OpenCV FFmpeg backend library

The backend library is required to write the temporary `ULRG` AVI:

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

<a id="ffmpeg-command-line-tool-windows-arm64"></a>
#### FFmpeg command-line tool

The `winarm64` FFmpeg package unpacked during that build also ships the command-line tool: `ffmpeg.exe` is in `external\ffmpeg-arm64\bin`. Copy it next to `VideoFrameGenerationEffectApp.exe`, or add that `bin` directory to `PATH`. It is a GPL build, so `libx264` is available. No separate download is needed.

### Linux

Install the FFmpeg command-line tools and libraries:

```bash
sudo apt install ffmpeg
```

Usage
-----

For the bundled video and image examples, run the generated scripts from the same directory as `VideoFrameGenerationEffectApp`:

```bash
# Windows
.\run_videoframegenerationeffectapp_video.bat [--high-quality]
.\run_videoframegenerationeffectapp_image.bat

# Linux
./run_videoframegenerationeffectapp_video.sh [--high-quality]
./run_videoframegenerationeffectapp_image.sh
```

The video runner sets the configured runtime library paths and produces these final video outputs:

- `output_vfg_multiplier.mp4`
- `output_vfg_timesteps.mp4`
- `output_vfg_pinned_memory.mp4`

With no option, the video runner uses the direct OpenCV video-output path. Pass the optional `--high-quality` runner argument when a high-quality encode is needed or when the OpenCV backend cannot encode the requested bitrate, frame rate, or resolution. The alternate path creates a lossless `ULRG` intermediate for each video example, encodes it with FFmpeg, and removes the temporary AVI after successful conversion.

The video runner returns a nonzero exit code if initialization, VFG processing, encoding, or video output creation fails. A successful video run must create non-empty output files. For automated validation, also probe each video and verify the expected resolution and frame count rather than relying only on the process exit code.

The image runner sets the configured runtime library paths, runs the 10-bit image-pair example, and produces these final image outputs:

- `out_1.png`, `out_2.png`, and `out_3.png`

The image runner reports success or failure from the application exit code and prints the path of each generated image.

The sample application uses OpenCV for both image and video input. The OpenCV image path preserves the 10-bit data in PNG and TIFF images, allowing the application to supply 10-bit image data to the SDK. The OpenCV video path converts 10-bit video to 8-bit during decoding and therefore cannot supply 10-bit video frames to the SDK. To test 10-bit video, replace the sample application's OpenCV video decoding path with a decoder that preserves 10-bit frame data and pass those frames to the SDK.

The default inputs are `input_vfg.mp4`, `input_vfg_10bit_1.png`, and `input_vfg_10bit_2.png`, which are copied beside the application during the build. To use another video or different generation options, edit the input and command arguments in the generated script.

VideoFrameGenerationEffectApp Command-Line Reference
----------------------------------------------------

| Argument                | Description |
|-------------------------|-------------|
| `--in_file=<path>`      | Input video file to be processed. |
| `--out_file=<path>`     | Output video file to be written. |
| `--input_frame_prev=<path>` | Previous 16-bit, three-channel PNG/TIFF input frame for the RGB10 image-pair path. Must be used together with `--input_frame_current` and `--output_frame_path`; it cannot be combined with `--in_file` or `--out_file`. |
| `--input_frame_current=<path>` | Current 16-bit, three-channel PNG/TIFF input frame for the RGB10 image-pair path. Must match the previous frame dimensions. |
| `--output_frame_path=<path>` | 16-bit PNG/TIFF path for generated RGB10 images. In multiplier mode, `N-1` intermediate images are produced. If more than one image is generated, the output names receive `_1`, `_2`, and so on before the file extension. |
| `--show`                | Display the results in a window. |
| `--multiplier=<N>`      | Multiply the input frame rate by `N` (default `2`). For each pair of consecutive input frames the SDK inserts `N-1` frames evenly spaced between them; e.g., `N=4` inserts 3 frames per pair, turning a 30 fps input into a 120 fps output. Out-of-range values are rejected with an error listing the allowed range (see also the [SDK docs](https://docs.nvidia.com/maxine/vfx/latest/Filters/VideoFrameGeneration.html)). Use either `--multiplier` or `--timesteps`, not both. |
| `--timesteps=t1,t2,...` | Generate frames at the listed positions between each input pair instead of evenly. Each value is in `(0.0, 1.0)` where `0` is the previous input frame and `1` is the current input frame (e.g., `0.5` = halfway, `0.25` = a quarter of the way). Useful for non-uniform spacing. Use either `--multiplier` or `--timesteps`, not both. |
| `--target_fps=<float>`  | fps written to the output video. Defaults to the input fps scaled by the number of frames generated per input pair, i.e. `input_fps * multiplier` in multiplier mode, or `input_fps * (1 + number_of_timesteps)` in timestep mode. Setting a lower value slows playback. (e.g. 24 fps source + `--multiplier=4` + `--target_fps=24` yields 4x slow motion at 24 fps). |
| `--mode={low\|medium\|high}` | Quality/performance mode for the effect (default `medium`). |
| `--automatic_shot_change_detection[={true\|false}]` | Enable automatic shot-change detection (default true). |
| `--use_pinned_memory[={true\|false}]` | Use `NVCV_CPU_PINNED` (page-locked) memory for input and output images. Recommended on WoA (iGPU systems; shared CPU/GPU memory) to avoid extra host↔device copies. Default is false. |
| `--codec=<fourcc>`      | The FourCC code for the desired codec (default `avc1` on Windows, `H264` on Linux). |
| `--progress`            | Show progress. |
| `--log=<file>`          | Log SDK errors to a file, `"stderr"` or `""` (default `stderr`). |
| `--log_level=<N>`       | The desired log level: `{0, 1, 2, 3}` = `{FATAL, ERROR, WARNING, INFO}` (default `1`). |
| `--verbose`             | Verbose output. |
| `--help`                | Display help information. |

Keyboard Controls
-----------------

When running with `--show`, the following keys are available:

| Key          | Description |
|--------------|-------------|
| `F`          | Toggles the frame rate display on and off. |
| `E`          | Toggles frame generation on/off (pass-through when disabled). |
| `P` or `%`   | Toggles per-frame progress display. |
| `Q` or `Esc` | Exits the app and cleanly finishes writing any output file. |

Examples
--------

The generated runner is the recommended way to produce H.264 MP4 output. The following direct application examples write lossless intermediate AVI files.

4x uniform interpolation, writing the output at 4x the source rate:

```
VideoFrameGenerationEffectApp --in_file=in.mp4 --out_file=out_4x.avi \
    --codec=ULRG --multiplier=4
```

4x slow motion: generate 3 frames per pair (4x the frames) but encode at the original frame rate, stretching the duration by 4x:

```
VideoFrameGenerationEffectApp --in_file=in_24fps.mp4 --out_file=slow.avi \
    --codec=ULRG --multiplier=4 --target_fps=24
```

Custom non-uniform timesteps -- use timestep mode when the intermediates must sit at specific temporal positions that aren't evenly spaced (uniform spacing is better expressed with `--multiplier`). One generated frame is emitted per listed `t`, followed by the original input frame at the end of each pair. The example below emits 3 frames per input pair at 30%, 50%, and 80% of the interval between consecutive input frames. Since `--target_fps` is not set, the output is encoded at `input_fps * (1 + number_of_timesteps) = input_fps * 4`:

```
VideoFrameGenerationEffectApp --in_file=in.mp4 --out_file=out_timesteps.avi \
    --codec=ULRG --timesteps=0.3,0.5,0.8
```

Generate RGB10 images from a pair of 16-bit PNG frames. `--multiplier=4` generates three intermediate images at 1/4, 1/2, and 3/4 of the interval, written as `generated_1.png`, `generated_2.png`, and `generated_3.png`:

```
VideoFrameGenerationEffectApp --input_frame_prev=prev.png --input_frame_current=current.png \
    --output_frame_path=generated.png --multiplier=4
```
