AigsEffectApp
===============

AigsEffectApp is a sample application that demonstrates the AI Green Screen feature of the NVIDIA Video Effects SDK. The application requires a video feed from a camera connected to the computer running the application, or from a video file, as specified with command-line arguments enumerated by executing: `AigsEffectApp.exe --help` (on Windows) or `./AigsEffectApp --help` (on Linux). 

The AI Green Screen feature requires that input images/videos be at least 512x288 (WxH) resolution

Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_features.ps1* (Windows) or *install_features.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXGreenScreen
- nvVFXBackgroundBlur

AI Green Screen Application Command-Line Reference
--------------------------------------------------

| Argument                             | Description |
|--------------------------------------|-------------|
| `--in_file=<path>`                   | The image file or video file for the application to process. |
| `--webcam={true\|false}`             | If true, use a webcam as input instead of a file. |
| `--cam_res=[<width>x]<height>`       | If `--webcam` is true, specify the resolution of the webcam; <width> is optional. If omitted, <width> is computed from <height> to give an aspect ratio of 16:9. For example:<br><br>`--cam_res=1280x720` or `--cam_res=720`<br><br>If `--webcam` is false, this argument is ignored. |
| `--out_file=<path>`                  | The file in which the video output is to be stored. |
| `--show={true\|false}`               | If true, display the resulting video output in a window. |
| `--model_dir=<path>`                 | The path to the folder that contains the model files to be used for the transformation. |
| `--codec=<fourcc>`                   | The four-character code (FourCC) of the video codec of the output video file. The default is `H264`. |
| `--help`                             | Display help information for the command. |
| `--mode={0\|1\|2\|3}`                | Selects the mode in which to run the application:<br><br>- `0`: Best quality with segmentation of the chairs as the foreground.<br>- `1`: Fastest performance with segmentation of the chairs as the foreground.<br>- `2`: Best quality with segmentation of the chairs as the background.<br>- `3`: Fastest performance with segmentation of the chairs as the background. |
| `--comp_mode={0\|1\|2\|3\|4\|5\|6}`  | Selects which composition mode to use:<br><br>- `0`: Displays the segmentation mask (`compMatte`).<br>- `1`: Overlays the mask on top of the image (`compLight`).<br>- `2`: Provides a composition with a `BGR={0,255,0}` background image (`compGreen`).<br>- `3`: Provides a composition with a `BGR={255,255,255}` background image (`compWhite`).<br>- `4`: No composition, but displays the input image (`compNone`).<br>- `5`: Overlays the mask on the image (`compBG`).<br>- `6`: Applies a background blur filter on the input image by using the segmentation mask (`compBlur`). |
| `--log=<file>`                       | Log SDK errors to a file, "stderr", or "" (default stderr). |
| `--log_level=<n>`                    | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |

Keyboard Controls
-----------------

The sample application provides keyboard controls to change the run-time behavior of the application.

| Key          | Description |
|--------------|-------------|
| `C`          | Cycles through the following ways of rendering the image:<br><br>- `0`: Displays the segmentation mask (`compMatte`).<br>- `1`: Overlays the mask on top of the image (`compLight`).<br>- `2`: Provides a composition with a `BGR={0,255,0}` background image (`compGreen`).<br>- `3`: Provides a composition with a `BGR={255,255,255}` background image (`compWhite`).<br>- `4`: No composition, but displays the input image (`compNone`).<br>- `5`: Overlays the mask on the image (`compBG`).<br>- `6`: Applies a background blur filter on the input image by using the segmentation mask (`compBlur`). |
| `F`          | Toggles the frame rate display on and off. |
| `Q` or `Esc` | Exits the app and cleanly finishes writing any output file. |
