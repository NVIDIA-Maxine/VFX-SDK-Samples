VideoSuperResEffectApp
======================

VideoSuperResEffectApp is a sample application that demonstrates the Video Super Resolution feature of the NVIDIA Video Effects SDK. The application accepts a webcam feed, a video file, or a still image as input, as specified with command-line arguments enumerated by executing: `VideoSuperResEffectApp.exe --help` (on Windows) or `./VideoSuperResEffectApp --help` (on Linux).

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

Video Super Resolution (VSR) is an AI-powered upscaling technology that enhances video resolution using deep learning. Unlike traditional bicubic upscaling, VSR can reconstruct fine details and textures. In addition to upscaling, VSR also supports denoise and deblur features. The full list of modes are shown below:

| Mode | Name |
|------|------|
| 0    | VSR_Bicubic |
| 1    | VSR_Low |
| 2    | VSR_Medium |
| 3    | VSR_High |
| 4    | VSR_Ultra |
| 8    | Denoise_Low |
| 9    | Denoise_Medium |
| 10   | Denoise_High |
| 11   | Denoise_Ultra |
| 12   | Deblur_Low |
| 13   | Deblur_Medium |
| 14   | Deblur_High |
| 15   | Deblur_Ultra |
| 16   | HighBitrate_Low |
| 17   | HighBitrate_Medium |
| 18   | HighBitrate_High |
| 19   | HighBitrate_Ultra |
| 21   | Streaming_Medium |
| 23   | Streaming_Ultra |

Please note:
1. For Linux, minimum Nvidia driver version is required to run VSR effect. Please refer to https://docs.nvidia.com/maxine/vfx/latest/Filters/VideoSuperResolution.html for details.
2. For Windows GPUs that are TCC (tesla compute cluster) devices, Nvidia driver r595+ is required.
3. Upscaling is not supported in Denoise(modes 8-11) and Deblur(mode 12-15) . For these modes, the resolution of the output should be the same as input.
4. The suggested minimum input resolution for VSR is 360p.
5. There is no restriction on the output frame aspect ratio, but keep it the same as input for the best quality.
6. Typical use cases of the denoise mode are low-light footage from consumer devices; archived content with film grain or analog artifacts; pre-encoding optimization to reduce bitrate overhead. It is NOT recommended for extreme noise levels obscuring underlying detail; structured compression artifacts (banding, blocking); content with intentional cinematic grain.
7. Typical use cases of the deblur mode are low-to-moderate QP encoded video with visible softness but intact structure; pre-processing for super-resolution pipelines; consumer camera footage with focus or lens softness issues; digitized archival content with inherent optical blur. It is NOT recommended for severe motion blur or artistic bokeh effects; noisy or heavily compressed inputs where deblurring amplifies artifacts.
8. Typical use cases of the high-bitrate mode are high-bitrate natural and gaming video with minimal compression artifacts; detail restoration after downscaling or quality enhancement in pre-encode workflows. It is NOT recommended for heavily compressed or severely blurred content in which the underlying information is irrecoverable.
9. 10-bit image input: Supply a 16-bit TIFF or PNG file (`--in_file`) to exercise the 10-bit (RGB10A2) VSR path. The app auto-detects the bit depth and selects the appropriate GPU buffer format. The output file will also be written as a 16-bit image when a 16-bit source is used. 10-bit video input is not supported in this sample — `cv::VideoCapture` decodes all video to 8-bit BGR regardless of source bit depth.
10. Streaming modes 21 and 23 require an Ampere or newer GPU. Existing VSR modes remain supported on Turing GPUs.

Required Features
-----------------
This app requires the following feature to be installed. Make sure to install it using *install_feature.ps1* (Windows) or *install_feature.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXVideoSuperRes

Additional Dependencies
-----------------------

By default, the Windows and Linux video runners use the direct OpenCV video-output path to write the final output file without creating or transcoding a temporary file.

On Windows, the OpenCV video backend uses Microsoft Media Foundation. This backend can fail to encode output at higher bitrates, frame rates, or resolutions. Review the FFmpeg instructions for [Windows x64](#ffmpeg-command-line-tool-windows-x64) or [Windows ARM64](#ffmpeg-command-line-tool-windows-arm64) below. Build FFmpeg and OpenCV with FFmpeg support when suitable packages are not available for the target platform.

For a high-quality encode, pass `--high-quality` to the video runner. This alternate path writes the generated video to a temporary lossless Ut Video RGB (`ULRG`) AVI and then uses FFmpeg with `libx264` to create the final H.264 MP4. It avoids relying on the platform video backend for the final encode and preserves the processed VSR frames without introducing encoder artifacts in the intermediate file. Use this path to minimize encoding artifacts in VSR-upscaled video.

The final encode uses `CRF 1`, a near-lossless `libx264` quality setting. This minimizes compression artifacts so the encoded output remains representative of the VSR result.

### Windows x64

<a id="ffmpeg-command-line-tool-windows-x64"></a>
#### FFmpeg command-line tool

1. Download `ffmpeg-release-essentials.zip` from https://www.gyan.dev/ffmpeg/builds/.
2. Extract the archive and locate `ffmpeg.exe` in its `bin` directory.
3. Copy `ffmpeg.exe` into the same directory as `VideoSuperResEffectApp.exe`, or add its `bin` directory to `PATH`.

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

The `winarm64` FFmpeg package unpacked during that build also ships the command-line tool: `ffmpeg.exe` is in `external\ffmpeg-arm64\bin`. Copy it next to `VideoSuperResEffectApp.exe`, or add that `bin` directory to `PATH`. It is a GPL build, so `libx264` is available. No separate download is needed.

### Linux

Install the FFmpeg command-line tools and libraries:

```bash
sudo apt install ffmpeg
```

Usage
-----

For the bundled video example, run the generated script from the same directory as `VideoSuperResEffectApp`:

```bash
# Windows
.\run_videosuperreseffectapp_video.bat [--high-quality]

# Linux
./run_videosuperreseffectapp_video.sh [--high-quality]
```

The default input is `input_vsr_1.mp4`, which is copied beside the application during the build. To use another video or different VSR options, edit the input and command arguments in the generated script.

The platform runner sets the configured runtime library paths and produces this output:

- `output_vsr_video.mp4`

The runner returns a nonzero exit code if initialization, VSR processing, or output creation fails. A successful run must create a non-empty output file. For automated validation, also probe the output and verify the expected resolution and frame count rather than relying only on the process exit code.

With no option, the runner uses the direct OpenCV video-output path. Pass the optional `--high-quality` runner argument when a high-quality encode is needed or when the OpenCV backend cannot encode the requested bitrate, frame rate, or resolution. The alternate path creates a lossless `ULRG` intermediate, encodes it with FFmpeg, and removes the temporary AVI after successful conversion.

VideoSuperResEffect Application Command-Line Reference
------------------------------------------------------

| Argument                              | Description |
|---------------------------------------|-------------|
| `--in_file=<path>`                    | The image file or video file for the application to process. For VideoSuperRes, 16-bit TIFF or PNG files are accepted and automatically route through the 10-bit (RGB10A2) VSR path. |
| `--resolution=[WWWx]HHH`              | The output resolution, specified as height (for example, `1080`) or width x height (for example, `1920x1080`). Video Super Resolution does not support downscaling. Input and output resolutions must match for Denoise and Deblur modes. |
| `--out_file=<path>`                   | The file in which the video output is to be stored. |
| `--show={true\|false}`                | If true, displays the resulting video output in a window. |
| `--model_dir=<path>`                  | The path to the folder that contains the model files to be used for the transformation. |
| `--codec=<fourcc>`                    | The four-character code (FourCC) of the video codec of the output video file. The default value is `H264`. |
| `--mode=<mode>`                       | For Video SuperRes, selects the quality level of the filter to be applied.<br><br>Supported modes:<br>- `0`: VSR_Bicubic<br>- `1`: VSR_Low<br>- `2`: VSR_Medium<br>- `3`: VSR_High<br>- `4`: VSR_Ultra<br>- `8`: Denoise_Low<br>- `9`: Denoise_Medium<br>- `10`: Denoise_High<br>- `11`: Denoise_Ultra<br>- `12`: Deblur_Low<br>- `13`: Deblur_Medium<br>- `14`: Deblur_High<br>- `15`: Deblur_Ultra<br>- `16`: HighBitrate_Low<br>- `17`: HighBitrate_Medium<br>- `18`: HighBitrate_High<br>- `19`: HighBitrate_Ultra<br>- `21`: Streaming_Medium<br>- `23`: Streaming_Ultra |
| `--strength=<value>`                  | For Video SuperRes, sets the effect strength in the inclusive range `[0.0, 1.0]`. The default is `1.0`. |
| `--use_pinned_memory[={true\|false}]` | Use `NVCV_CPU_PINNED` (page-locked) memory for input and output images instead of regular GPU memory. Can improve transfer throughput on some systems. Default is false. |
| `--progress`                          | Show per-frame progress as a percentage while processing a video file. |
| `--verbose[={true\|false}]`           | Shows verbose output. |
| `--debug`                             | Prints extra debugging information. |
| `--help`                              | Displays help information. |
| `--log=<file>`                        | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`                     | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |
| `--webcam[={true\|false}]`            | Use a webcam as the input source instead of a file. |
| `--cam_res=[WWWx]HHH`                 | Specify camera resolution as height (e.g., `720`) or width x height (e.g., `1280x720`). Common webcam resolutions include 720p and 1080p. The actual supported resolutions depend on your camera hardware. Default is `1280x720`. |


Keyboard Controls
-----------------

The sample application provides keyboard controls for changing the run-time behavior of the application.

| Key          | Description |
|--------------|-------------|
| `F`          | Toggles the frame rate display on and off. |
| `N`          | Cycles through VSR quality levels. Skips Denoise/Deblur modes when upscaling. Only works when effect is enabled. |
| `E`          | Toggles the effect on/off. Only available when source and destination resolutions match (no upscaling). |
| `Q` or `Esc` | Exits the app and cleanly finishes writing any output file. |
