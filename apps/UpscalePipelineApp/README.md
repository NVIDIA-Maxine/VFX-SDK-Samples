UpscalePipelineApp
==================

UpscalePipelineApp is a sample application that demonstrates the chaining of the Encoder Artifact Reduction and Upscaler features to achieve high quality video upscaling while reducing encoding artifacts. The application requires an image or video file, as specified with command-line arguments enumerated by executing: `UpscalePipelineApp.exe --help` (on Windows) or `./UpscalePipelineApp --help` (on Linux). 

The Encoder Artifact Reduction feature supports input images/videos in the 90p-1080p resolution range. 
The Upscaler feature supports any input resolution and can be upscaled 4/3x, 1.5x, 2x, 3x, or 4x.


Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_features.ps1* (Windows) or *install_features.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXUpscale
- nvVFXArtifactReduction

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
| `--ar_mode={0\|1}`             | For Artifact Reduction, selects the strength of the filter to be applied.<br><br>- `0`: Weak effect.<br>- `1`: Strong effect. |
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
