DenoiseEffectApp
================

DenoiseEffectApp is a sample application that demonstrates the Webcam Denoising feature of the NVIDIA Video Effects SDK. The application requires a video feed from a camera connected to the computer running the application, or from a video file, as specified with command-line arguments enumerated by executing: `DenoiseEffectApp.exe --help` (on Windows) or `./DenoiseEffectApp --help` (on Linux). 


Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_features.ps1* (Windows) or *install_features.sh* (Linux) in your VFX SDK features directory before building it.
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
| `--log=<file>`                 | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`              | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |

Keyboard Controls
-----------------

The sample application provides keyboard controls for changing the run-time behavior of the application.

| Key          | Description |
|--------------|-------------|
| `E`          | Toggles the effect on and off. |
| `F`          | Toggles the frame rate display on and off. |
| `Q` or `Esc` | Exits the app and cleanly finishes writing any output file. |
