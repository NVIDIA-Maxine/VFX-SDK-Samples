RelightingEffectApp
===================

RelightingEffectApp is a sample application that demonstrates the Relighting effect of the NVIDIA Video Effects SDK.

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
This app requires the following features to be installed. Make sure to install them using *install_features.ps1* (Windows) or *install_features.sh* (Linux) in your VFX SDK features directory before building it.
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
