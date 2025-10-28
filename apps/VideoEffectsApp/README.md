VideoEffectsApp
===============

VideoEffectsApp is a sample application that demonstrates the Super Resolution, Encoder Artifact Reduction and Upscaler features of the NVIDIA Video Effects SDK. The application requires a video feed from a camera connected to the computer running the application, or from a video file, as specified with command-line arguments enumerated by executing: `VideoEffectsApp.exe --help` (on Windows) or `./VideoEffectsApp --help` (on Linux). 

The Encoder Artifact Reduction feature supports input images/videos in the 90p-1080p resolution range. 
The Upscaler feature supports any input resolution and can be upscaled 4/3x, 1.5x, 2x, 3x, or 4x.
The Super Resolution feature supports the following input resolution and scale factors:

| Scale factor | Input resolution |
|--------------|------------------|
| 4/3x         | [90x90, 2160p]   |
| 1.5x         | [90x90, 2160p]   |
| 2x           | [90x90, 2160p]   |
| 3x           | [90x90, 720p]    |
| 4x           | [90x90, 540p]    |

Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_features.ps1* (Windows) or *install_features.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXArtifactReduction
- nvVFXSuperRes
- nvVFXUpscale
- nvVFXTransfer

VideoEffects Application Command-Line Reference
-----------------------------------------------

| Argument                    | Description |
|-----------------------------|-------------|
| `--in_file=<path>`          | The image file or video file for the application to process. |
| `--effect=<effect>`         | The effect to be applied:<br><br>- `ArtifactReduction`: Removes the encoder artifact without changing the resolution.<br>- `SuperRes`: Removes artifacts (mode 0) and upscales to the specified output resolution.<br>- `Upscale`: Fast upscaler that increases the video resolution to the specified output resolution.<br><br>**Note:** You can also select any of the effects that are listed when you run VideoEffectsApp with the `--help` flag. |
| `--resolution=<n>`          | The desired output vertical resolution from Upscale and SuperRes, scaled to `1.3333`, `1.5`, `2`, `3`, or `4` times the input. |
| `--out_file=<path>`         | The file in which the video output is to be stored. |
| `--show={true\|false}`      | If true, displays the resulting video output in a window. |
| `--model_dir=<path>`        | The path to the folder that contains the model files to be used for the transformation. |
| `--codec=<fourcc>`          | The four-character code (FourCC) of the video codec of the output video file. The default value is `H264`. |
| `--strength=<value>`        | The strength of the Upscale effect, specified as a float value from `0.0` to `1.0`. |
| `--mode=<mode>`             | For SuperRes or ArtifactReduction, selects the strength of the filter to be applied.<br><br>- `0`: Weak effect.<br>- `1`: Strong effect. |
| `--verbose[={true\|false}]` | Shows verbose output. |
| `--debug`                   | Prints extra debugging information. |
| `--help`                    | Displays help information. |
| `--log=<file>`              | Log SDK errors to a file, "stderr" (default), or "". |
| `--log_level=<n>`           | The desired log level: `0` (fatal), `1` (error; default), `2` (warning), or `3` (info). |


Keyboard Controls
-----------------

The sample application provides keyboard controls for changing the run-time behavior of the application.

| Key          | Description |
|--------------|-------------|
| `F`          | Toggles the frame rate display on and off. |
| `Q` or `Esc` | Exits the app and cleanly finishes writing any output file. |
