UpscalePipelineApp
==================

UpscalePipelineApp is a sample application that demonstrates the Upscaler features to achieve high quality video upscaling. The application requires an image or video file, as specified with command-line arguments enumerated by executing: `UpscalePipelineApp.exe --help` (on Windows) or `./UpscalePipelineApp --help` (on Linux). 

The Upscaler feature supports any input resolution and can be upscaled 4/3x, 1.5x, 2x, 3x, or 4x.

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
- nvVFXUpscale

UpscalePipeline Application Command-Line Reference
--------------------------------------------------

| Argument                       | Description |
|--------------------------------|-------------|
| `--in_file=<path>`             | The image file or video file for the application to process. |
| `--out_file=<path>`            | The file in which the video output is to be stored. |
| `--resolution=<n>`             | The vertical resolution of the output image or video, which is scaled from the input vertical resolution by `1.3333`, `1.5`, `2`, `3`, or `4`. |
| `--show={true\|false}`         | If true, displays the resulting video output in a window. |
| `--model_dir=<path>`           | The path to the folder that contains the model files that will be used for the transformation. |
| `--codec=<fourcc>`             | The four-character code (FourCC) of the video codec of the output video file. The default is `H264`. |
| `--upscale_strength={0.0-1.0}` | Selects the strength of the Upscale filter to be applied.<br><br>- `0.0`: No enhancement.<br>- `1.0`: Maximum crispness.<br>- The default value is `0.4`. |
| `--progress`                   | Show the progress. |
| `--verbose={true\|false}`      | Shows verbose output. |
| `--debug={true\|false}`        | Prints extra debugging information. |
| `--help`                       | Displays help information for the command. |
| `--log=<file>`                 | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`              | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |

Keyboard Controls
-----------------

The sample application provides keyboard controls for changing the run-time behavior of the application.

| Key          | Description |
|--------------|-------------|
| `F`          | Toggles the frame rate display on and off. |
| `Q` or `Esc` | Exits the app and cleanly finishes writing any output file. |
