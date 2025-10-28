TritonClientApp
===============

The TritonClientApp is a sample application, only for the Triton enabled VFX SDK, which can be used to run VFX SDK features on the server.

It can concurrently process multiple input video files with AI Green Screen feature.

Required Features
-----------------
This app requires the following features to be installed. Make sure to install them using *install_features.ps1* (Windows) or *install_features.sh* (Linux) in your VFX SDK features directory before building it.
- nvVFXGreenScreen

Run the Triton Client Application
---------------------------------

First make sure you have the Triton server application running. See the base README.md for information on this.

TritonClientApp can be used to run AI Green Screen
on the server.

The following sets up the VFX SDK library path and then
runs AI Green Screen mode 0 and mode 1 on the three input videos and
produces output files.

```
source setup_env.sh

./TritonClientApp --effect=GreenScreen --mode=0 video1.mp4 video2.mp4
video3.mp4

./TritonClientApp --effect=GreenScreen --mode=1 video1.mp4 video2.mp4
video3.mp4
```

Command-Line Arguments for the Triton Client Application
--------------------------------------------------------

| Flag                     | Description |
|--------------------------|-------------|
| `--effect=<effect_name>` | Name of the effect to run (Supported: `GreenScreen`). |
| `--url=<URL>`            | URL to the Triton server |
| `--grpc[={true\|false}]` | Use gRPC for data transfer to the Triton server instead of CUDA shared memory. |
| `--out_file=<path>`      | Output video files to be written (a pattern with one `%u` or `%d`), default `"BatchOut_%02u.mp4"` |
| `--model_dir=<path>`     | The path to the directory that contains the models |
| `--mode=<value>`         | Which model to pick for processing (default: `0`) |
| `--verbose`              | Verbose output |
| `--codec=<fourcc>`       | The fourcc code for the desired codec (default `avc1`) |
| `--log=<file>`           | Log SDK errors to a file, "stderr" or "" (default stderr) |
| `--log_level=<N>`        | The desired log level: {`0`, `1`, `2`} = {FATAL, ERROR, WARNING}, respectively (default `1`) |
