RelightingTritonClientApp
=========================

The RelightingTritonClientApp is a sample application, only for the Triton enabled VFX SDK, which can be used to run Relighting features on the server.

It chains various effects together:
* **AIGS** - to perform segmentation.
* **Relighting** - to relight the foreground, given the input video and a corresponding segmentation mask.
* **Background blur** (optional) - to blur the background.
* **Composition** - to merge the relit foreground with your choice of background.

Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_features.ps1* (Windows) or *install_features.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXRelighting

Run the Triton Client Application
---------------------------------

First make sure you have the Triton server application running. See the base README.md for information on this.

RelightingTritonClientApp can be used to run relighting on the server.

The following sets up the VFX SDK library path and then
runs AI relighitng to produce an output video file.

```
source setup_env.sh

./RelightingTritonClientApp                   
    --use_triton
    --in_file="input_100_250_frames.mp4"    
    --in_hdr="vkl_mid.hdr"                  
    --in_bg="input1.jpg"                    
    --autorotate                            
    --out_file=test_out.mp4                 
```

Command-Line Arguments for the Relighting Triton Client Application
-------------------------------------------------------------------

| Argument                       | Description |
|--------------------------------|-------------|
| `--autorotate[={true\|false}]` | Automatically rotate the environment |
| `--bg_mode=<n>`                | Background mode: `0`=src `1`=srcBlur `2`=HDR `3`=bgImg `4`=bgImgBlur |
| `--cam_res=[<width>x]<height>` | Specify resolution as height or width x height |
| `--codec=<fourcc>`             | The fourcc code for the desired codec (default `avc1`) |
| `--debug[={true\|false}]`      | Print extra debugging information |
| `--help[={true\|false}]`       | Print help message |
| `--in_bg=<file\|color>`        | Use the specified file (png or jpg) or color (gray or 0xRRGGBB) for the background |
| `--in_file=<file>`             | Specify input source file (image or video) |
| `--in_hdr=<file>`              | Specify input HDR file (hdr or exr) or directory, for illumination. |
| `--in_mat=<file>`              | Specify input matte file. Only supported when processing images.<br><br>If not specified, or when processing videos/webcam, AIGS is run |
| `--log=<file>`                 | Log SDK errors to a file, "stderr" or "" (default stderr) |
| `--log_level=<N>`              | The desired log level: {`0`, `1`, `2`, `3`} = {FATAL, ERROR, WARNING, INFO}, respectively (default `1`) |
| `--model_dir=<path>`           | The path to the directory that contains the .trtmodel files |
| `--out_dir=<dir>`              | Set the output directory. Must use in conjunction with --out_file to create an output file  |
| `--out_file=<file>`            | Specify an output video file |
| `--pan=<num>`                  | Set the initial pan angle, in degrees (default `-90`) |
| `--rotation_rate=<N>`          | The auto-rotation rate, in degrees per second |
| `--show_mode=<mode>`           | Options: `output`, `input` |
| `--show[={true\|false}]`       | Display images on-screen |
| `--verbose[={true\|false}]`    | Verbose output |
| `--vfov=<num>`                 | Set the initial vertical field of view, in degrees (default `60`) |
| `--webcam[={true\|false}]`     | Use a webcam as the input, rather than a file |
| `--use_triton[={true\|false}]` | Use Triton server inference |
| `--url=<URL>`                  | URL to the Triton server |
| `--grpc[={true\|false}]`       | Use gRPC for data transfer to the Triton server instead of CUDA shared memory. |

Keyboard Controls
-----------------

If running with `--show`, there are some additional controls available interactively:

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
| `h`            | Print help message |
