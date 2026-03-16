VideoEffectsApp
===============

VideoEffectsApp is a sample application that demonstrates the Video Super Resolution feature of the NVIDIA Video Effects SDK. The application requires a video feed from a camera connected to the computer running the application, or from a video file, as specified with command-line arguments enumerated by executing: `VideoEffectsApp.exe --help` (on Windows) or `./VideoEffectsApp --help` (on Linux). 

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

Please note:
1. For Linux, minimum Nvidia driver version is required to run VSR effect. Please refer to https://docs.nvidia.com/maxine/vfx/latest/Filters/VideoSuperResolution.html for details.
2. For Windows GPUs that are TCC (tesla compute cluster) devices, Nvidia driver r595+ is required.
3. Upscaling is not supported in Denoise(modes 8-11) and Deblur(mode 12-15) . For these modes, the resolution of the output should be the same as input.
4. The suggested minimum input resolution for VSR is 360p.
5. There is no restriction on the output frame aspect ratio, but keep it the same as input for the best quality.
6. Typical use cases of the denoise mode are low-light footage from consumer devices; archived content with film grain or analog artifacts; pre-encoding optimization to reduce bitrate overhead. It is NOT recommended for extreme noise levels obscuring underlying detail; structured compression artifacts (banding, blocking); content with intentional cinematic grain.
7. Typical use cases of the deblur mode are low-to-moderate QP encoded video with visible softness but intact structure; pre-processing for super-resolution pipelines; consumer camera footage with focus or lens softness issues; digitized archival content with inherent optical blur. It is NOT recommended for severe motion blur or artistic bokeh effects; noisy or heavily compressed inputs where deblurring amplifies artifacts.
8. Typical use cases of the high-bitrate mode are high-bitrate natural and gaming video with minimal compression artifacts; detail restoration after downscaling or quality enhancement in pre-encode workflows. It is NOT recommended for heavily compressed or severely blurred content in which the underlying information is irrecoverable.

Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_feature.ps1* (Windows) or *install_feature.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXVideoSuperRes
- nvVFXTransfer

VideoEffects Application Command-Line Reference
-----------------------------------------------

| Argument                    | Description |
|-----------------------------|-------------|
| `--in_file=<path>`          | The image file or video file for the application to process. |
| `--effect=<effect>`         | The effect to be applied:<br><br>- `VideoSuperRes`: Removes artifacts (mode 0) and upscales to the specified output resolution.<br><br>**Note:** You can also select any of the effects that are listed when you run VideoEffectsApp with the `--help` flag. |
| `--resolution=<n>`          | The desired output vertical resolution from Video Super Resolution, scaled up x times of the input |
| `--out_file=<path>`         | The file in which the video output is to be stored. |
| `--show={true\|false}`      | If true, displays the resulting video output in a window. |
| `--model_dir=<path>`        | The path to the folder that contains the model files to be used for the transformation. |
| `--codec=<fourcc>`          | The four-character code (FourCC) of the video codec of the output video file. The default value is `H264`. |
| `--mode=<mode>`             | For Video SuperRes, selects the quality level of the filter to be applied.<br><br>Supported modes:<br>- `0`: VSR_Bicubic<br>- `1`: VSR_Low<br>- `2`: VSR_Medium<br>- `3`: VSR_High<br>- `4`: VSR_Ultra<br>- `8`: Denoise_Low<br>- `9`: Denoise_Medium<br>- `10`: Denoise_High<br>- `11`: Denoise_Ultra<br>- `12`: Deblur_Low<br>- `13`: Deblur_Medium<br>- `14`: Deblur_High<br>- `15`: Deblur_Ultra<br>- `16`: HighBitrate_Low<br>- `17`: HighBitrate_Medium<br>- `18`: HighBitrate_High<br>- `19`: HighBitrate_Ultra |
| `--verbose[={true\|false}]` | Shows verbose output. |
| `--debug`                   | Prints extra debugging information. |
| `--help`                    | Displays help information. |
| `--log=<file>`              | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`           | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |
| `--webcam[={true\|false}]`  | Use a webcam as the input source instead of a file. |
| `--cam_res=[WWWx]HHH`       | Specify camera resolution as height (e.g., `720`) or width x height (e.g., `1280x720`). Common webcam resolutions include 720p and 1080p. The actual supported resolutions depend on your camera hardware. Default is `1280x720`. |


Keyboard Controls
-----------------

The sample application provides keyboard controls for changing the run-time behavior of the application.

| Key          | Description |
|--------------|-------------|
| `F`          | Toggles the frame rate display on and off. |
| `N`          | Cycles through VSR quality levels. Skips Denoise/Deblur modes when upscaling. Only works when effect is enabled. |
| `E`          | Toggles the effect on/off. Only available when source and destination resolutions match (no upscaling). |
| `Q` or `Esc` | Exits the app and cleanly finishes writing any output file. |
