RelightingEffectApp
===================

RelightingEffectApp is a sample application that demonstrates the Relighting effect of the NVIDIA Video Effects SDK.

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

Required Parameters
-------------------

The following parameters are required to run the application:

| Parameter                      | Description |
|--------------------------------|-------------|
| `--model_dir=<path>`           | This provides a path to the models directory |
| `--in_file=<file>` or `--webcam` | You can choose to provide the input either from a video file or the webcam. |
| `--in_hdr=<file>`              | An HDR image with .hdr or .exr suffix. You can choose the studio lighting HDR image at https://polyhaven.com/a/studio_small_05 or any of the other HDRIs at that or other sites. |
| `--out_file=<file>` or `--show` | You can choose to write the output to a file, show it on the screen, or both. |

**Note:** You must set up various paths in the environment to be able to access the libraries that this app needs. There are script and batch files that do this for you -- you will just need to edit them to set up the other necessary parameters.
The --out_dir cmd line parameter to set up the output directory is not sufficient to create an output file. You must use it in conjunction with the --out_file parameter as well.

Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_feature.ps1* (Windows) or *install_feature.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXRelighting
- nvVFXGreenScreen
- nvVFXBackgroundBlur

RelightingEffectApp Command-Line Reference
------------------------------------------

| Argument                       | Description |
|--------------------------------|-------------|
| `--in_file=<file>`             | Specify input source file (image or video) |
| `--webcam[={true\|false}]`     | Use a webcam as the input, rather than a file |
| `--cam_res=[<width>x]<height>` | Specify resolution as height or width x height |
| `--out_dir=<dir>`              | Set the output directory. Must use in conjunction with --out_file to create an output file |
| `--out_file=<file>`            | Specify an output video file |
| `--in_bg=<file\|color>`        | Use the specified file (png or jpg) or color (gray or 0xRRGGBB) for the background |
| `--in_hdr=<file>`              | Specify input HDR file (hdr or exr) or directory, for illumination. |
| `--in_mat=<file>`              | Specify input matte file. Only supported when processing images.<br><br>If not specified, or when processing videos/webcam, AIGS is run |
| `--pan=<num>`                  | Set the initial pan angle, in degrees (default `-90`) |
| `--vfov=<num>`                 | Set the initial vertical field of view, in degrees (default `60`) |
| `--autorotate[={true\|false}]` | Automatically rotate the environment |
| `--rotation_rate=<N>`          | The auto-rotation rate, in degrees per second |
| `--show[={true\|false}]`       | Display images on-screen |
| `--show_mode=<mode>`           | Options: `output`, `input` |
| `--model_dir=<path>`           | The path to the directory that contains the .trtmodel files |
| `--codec=<fourcc>`             | The fourcc code for the desired codec (default `avc1`) |
| `--verbose[={true\|false}]`    | Verbose output |
| `--debug[={true\|false}]`      | Print extra debugging information |
| `--log=<file>`                 | Log SDK errors to a file, "stderr" or "" (default stderr) |
| `--log_level=<N>`              | The desired log level: {`0`, `1`, `2`, `3`} = {FATAL, ERROR, WARNING, INFO}, respectively (default `1`) |
| `--help[={true\|false}]`       | Print help message |
| `--bg_mode=<n>`                | Background mode: `0`=src `1`=srcBlur `2`=HDR `3`=bgImg `4`=bgImgBlur |
| `--use_pinned_memory[={true\|false}]` | Use `NVCV_CPU_PINNED` (page-locked) memory for input and output images. Recommended on WoA (iGPU systems; shared CPU/GPU memory) to avoid extra host↔device copies. Default is false. |
| `--cache_dir=<path>`           | Model cache directory (default: `model_dir/cache`). WoA only (e.g. RTX Spark). |
| `--cache_mode={0\|1\|2}`       | Model cache mode: `0`=Auto (default), `1`=Disabled, `2`=ForceRegenerate. WoA only (e.g. RTX Spark). |

Windows on Arm (WoA) notes
--------------------------

On Windows on Arm (WoA, e.g. RTX Spark), Relighting (and the AI Green Screen matte path) loads two models, so a cold first launch can take ~10 seconds of JIT (~5 seconds per model) while `.cache` files are written under `model_dir/cache` (or `--cache_dir`). Later launches reuse those caches and load much faster. Inference quality is unchanged.

This sample enables CUDA graphs on the Green Screen matte path automatically. Prefer `--use_pinned_memory` on WoA (iGPU systems) for better steady-state transfer performance.

Keyboard Controls
-----------------

Assuming that you have `--show` enabled, you can control various parameters interactively from the keyboard while running.

| Key            | Function |
|----------------|----------|
| `ESC` or `q`   | Quit |
| `,` (comma)    | Adjust pan by -1 degree |
| `.` (period)   | Adjust pan by +1 degree |
| `<`            | Adjust pan by -10 degrees |
| `>`            | Adjust pan by +10 degrees |
| `v` (lower)    | Adjust vfov by -10 degrees |
| `V` (upper)    | Adjust vfov by +10 degrees |
| `r`            | Auto-rotate |
| `p` or `space` | Pause video |
| `f`            | Toggle between showing and not showing the frame rate |
| `i`            | Toggle between showing output and showing input |
| `n`            | Advance to the next HDR for illumination |
| `b`            | Cycle through background mode |
| `z`            | Reset to studio lighting |
| `h`            | Print help message |
