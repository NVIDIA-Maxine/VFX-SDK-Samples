DenoiseEffectApp
================

DenoiseEffectApp is a sample application that demonstrates the Webcam Denoising feature of the NVIDIA Video Effects SDK. The application requires a video feed from a camera connected to the computer running the application, or from a video file, as specified with command-line arguments enumerated by executing: `DenoiseEffectApp.exe --help` (on Windows) or `./DenoiseEffectApp --help` (on Linux). 

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

Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_feature.ps1* (Windows) or *install_feature.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXDenoising

DenoiseEffect Application Command-Line Reference
------------------------------------------------

| Argument                       | Description |
|--------------------------------|-------------|
| `--in_file=<path>`             | The image file or video file for the application to process. |
| `--out_file=<path>`            | The file in which the image or video output is to be stored. |
| `--show={true\|false}`         | If true, displays the resulting video output in a window. |
| `--model_dir=<path>`           | The path to the folder that contains the model files that will be used for the transformation. |
| `--codec=<fourcc>`             | The four-character code (FourCC) of the video codec of the output video file. The default value is `H264`. |
| `--strength={0\|1}`            | The strength of the effect:<br><br>- `0`: Weak effect.<br>- `1`: Strong effect. |
| `--progress`                   | Shows the progress. |
| `--webcam`                     | Uses the webcam as input. |
| `--cam_res=[<width>x]<height>` | If `--webcam` is true, specify the resolution of the webcam; <width> is optional. If omitted, <width> is computed from <height> to give an aspect ratio of 16:9. For example:<br><br>`--cam_res=1280x720` or `--cam_res=720`<br><br>If `--webcam` is false, this argument is ignored. |
| `--verbose={true\|false}`      | Show verbose output. |
| `--debug={true\|false}`        | Prints extra debugging information. |
| `--help`                       | Displays help information for the command. |
| `--cache_dir=<path>`           | Model cache directory (default: `model_dir/cache`). WoA only (e.g. RTX Spark). |
| `--cache_mode={0\|1\|2}`       | Model cache mode: `0`=Auto (default), `1`=Disabled, `2`=ForceRegenerate. WoA only (e.g. RTX Spark). |
| `--use_pinned_memory[={true\|false}]` | Use `NVCV_CPU_PINNED` (page-locked) memory for input and output images. Recommended on WoA (iGPU systems; shared CPU/GPU memory) to avoid extra host↔device copies. Default is false. |
| `--log=<file>`                 | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`              | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |

Windows on Arm (WoA) notes
--------------------------

On Windows on Arm (WoA, e.g. RTX Spark), the first model load incurs a one-time JIT (~5 seconds per model) and writes a `.cache` file under `model_dir/cache` (or `--cache_dir`). Subsequent launches reuse the cache and load much faster. Inference quality is unchanged.

Prefer `--use_pinned_memory` on WoA (iGPU systems) for better steady-state transfer performance.

Keyboard Controls
-----------------

The sample application provides keyboard controls for changing the run-time behavior of the application.

| Key          | Description |
|--------------|-------------|
| `E`          | Toggles the effect on and off. |
| `F`          | Toggles the frame rate display on and off. |
| `Q` or `Esc` | Exits the app and cleanly finishes writing any output file. |
