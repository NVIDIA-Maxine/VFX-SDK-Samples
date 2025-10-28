NVIDIA VFX SDK Sample Apps
==========================

Overview
--------

NVIDIA Maxine Video Effects SDK enables AI-based visual effects that run with standard webcam input and can easily be integrated into video conference and content creation pipelines. The underlying deep learning models are optimized with NVIDIA AI using NVIDIA® TensorRT™ for high-performance inference, making it possible for developers to apply multiple effects in real-time applications.

This repository contains light weight sample applications to demonstrate the features of the VFX SDK. The applictions use a real time web camera or video stream, which is processed through the SDK differently depending on which application is in use. The application source code is intended to show how to use the SDK.


<p align="center">
<img src="resources/reference_images/SR.gif" alt="NVIDIA Super Resolution" width="640" height="320"/>
 </p>

<p align="center">
<img src="resources/reference_images/Denoise.gif" alt="NVIDIA Video Noise Removal" width="640" height="320"/>
 </p>


Requirements
------------

### Pre-requisites - Windows

- Windows OS supported: 64-bit Windows 10 or later
- Microsoft Visual Studio: 2022 (MSVC17.0) or later: https://visualstudio.microsoft.com/downloads/
  - Ensure the **Desktop development with C++** workload is selected and installed
- Git: https://git-scm.com/downloads
- Git-lfs: https://git-lfs.com/
- CMake: v3.21 or later: https://cmake.org/download/
- NVIDIA Graphics Driver for Windows: 570.65 or later

The SDK is supported on NVIDIA GPUs that are based on the NVIDIA® Turing™, Ampere™, Ada™, or Blackwell™, architecture and have Tensor Cores.

### Pre-requisites - Linux

- OS support: Ubuntu 20.04, 22.04, 24.04, Debian 12, RHEL 8/9
- Git: https://git-scm.com/downloads
- Git-lfs: https://git-lfs.com/
  - Install:
  ```
  $ curl -s https://packagecloud.io/install/repositories/github/git-lfs/script.deb.sh | sudo bash
  $ sudo apt install git-lfs
  ```
- NVIDIA Graphics Driver: 570.26 or later

The SDK is supported on NVIDIA GPUs that are based on the NVIDIA® Turing™, Ampere™, Ada™, Blackwell™, or Hopper™ architecture and have Tensor Cores.

Setup
-----

Before any of the sample apps can be built, the VFX SDK needs to be installed

### The VFX SDK

1. Get an NGC account: https://ngc.nvidia.com/signup/complete-profile
2. navigate to VFX SDK NGC page: https://catalog.ngc.nvidia.com/orgs/nvidia/teams/maxine/collections/maxine_vfx_sdk
3. Download and install the latest SDK version 1.0.0.0
4. Follow the "Getting Started" guide on https://docs.nvidia.com/maxine/vfx/index.html

### Features

VFX SDK features, such as green screen, and artifact reduction, need to be installed explicitly. Once a local installation of the VFX SDK exists, features can be installed using the *install_feature* script.

Download all feautres:
-  `$ cd </path/to/VFX_SDK>/features`
- Windows: `$ ./install_features.ps1`
- Linux: `$ ./install_features.sh`

For specific help instructions:
- Windows: `$ ./install_features..ps1 -h`
- Linux: `$ ./install_features.sh -h`

For features requiring EA access: https://developer.nvidia.com/maxine-early-access

### Accessing the sample code

Clone using git:
  - `$ git clone git@github.com:NVIDIA-Maxine/VFX-SDK-Samples.git`
  - `$ cd VFX-SDK-Samples`

Initialize git-lfs
  - `$ git lfs install`
  - `$ git pull`

Building and running
--------------------

### Build applications - Windows - Command prompt

In a Visual Studio 2022 Developer Command Prompt:

```
$ cd VFX-SDK-Samples
$ mkdir build
$ cd build
$ cmake.exe .. -G "Visual Studio 17 2022" -DVFXSDK_ROOT=</path/to/VFX_SDK>
$ cmake.exe --build . --config Release
```

Where `</path/to/VFX_SDK>` is the root path to where the you installed the VFX SDK package.

### Run applications - Windows - Command prompt

Example with AigsEffectApp

```
$ cd <build_directory>\apps\AigsEffectApp\<Release|Debug>\
$ run_aigseffectapp_webcam.bat
```

### Build applications - Windows - GUI

1. Open CMake GUI and select the path to VFX-SDK-Samples in "Where is the source code:". Select the path to VFX-SDK-Samples/build in "Where to build the binaries:"
2. Configure
3. Set the variable `VFXSDK_ROOT` to the location where the VFX SDK is installed
4. Configure again
5. Generate
6. Open Project
7. In Visual Studio: *Build -> Build Solution*, (or **Ctrl+Shift+B**)

### Run applications - Windows - GUI

Example with AigsEffectApp

1. Right click **AigsEffectApp** In the Solution Explorer of Visual Studio
2. Set as Startup Project
3. Run **Local Windows Debugger** 


### Build applications - Linux - Terminal

```
$ cd VFX-SDK-Samples
$ ./build_samples.sh
```

### Run applications - Linux - Terminal

Example with AigsEffectApp

```
$ cd ~/mysamples/build/apps/AigsEffectApp
$ run_aigseffectapp_webcam.sh
```

See README.md in each individual app directory for more information on every sample application.

Triton - Linux Only
-------------------

Triton client applications are applications that communicate with an NVIDIA Triton Inference Server allowing off-client inference processing. The Triton backend application comes with the SDK and needs to run in a separate process from the client sample applicaitons in this repository.

To set up the Triton server. Follow the instructions on https://docs.nvidia.com/maxine/triton/GetStarted/InstallServerandSDK.html

### Building Triton Client Apps

To build Triton client applications, pass the flag `-DENABLE_TRITON=ON` to the CMake command during configuration. The flag will be enabled by default when running the `build_samples.sh` script on Linux.

### Run the Triton Server

Before running the sample applications, you must run the
Triton server by running the ``run_triton_server.sh`` script in the server
package.

```
$ sudo bash run_triton_server.sh
```

This script runs the Triton server and loads the VFX SDK features. If
successful, the server displays messages saying the gRPC and metric
service started and block the terminal as shown:

```
I0411 22:05:33.459919 1 grpc_server.cc:4819] Started
GRPCInferenceService at 0.0.0.0:8001

I0411 22:05:33.502086 1 http_server.cc:184] Started Metrics Service at
0.0.0.0:8002
```

This script runs the Triton server without selecting any command-line
options. Run ``tritonserver --help`` inside the Docker container to see
the list of command-line options supported by the Triton server
application.

The sample applications need to be run as a separate process. When
running manually, the server and the sample applications can be run
on separate terminals or using utilities such as tmux.

### Run the Triton Client Applications

See each corresponding README.md file for all Triton client applications for details on how to run the app.

Common Issues
-------------

### Missing Feature Installations

If CMake complains with a warning message:
```
Required feature <feature name> is not available.
```
or
```
REQUIRED FEATURE: <feature name> VERSION: <version> is not available.
```
followed by
```
Skipping <app name>.
```
Make sure that the latest version of the feature is installed (see section on Features under Setup).

Note that it is possible to build a subset of the sample apps by installing a subset of all features, say only the ones that are required for the sample app you want to build.

### Missing Git LFS

An error like:

`moov atom not found`, followed by
`Error: Could not open video`, is likely due to OpenCV trying to interpret temporary text files as video. These larger files are maintained using git-lfs, which needs to be installed and initialized in the repository for any of the applications to be able to load the provided sample videos. See the section on **Accessing the sample code** to initialize git-lfs.

Documentation
-------------

Please refer to the online documentation guides
- [NVIDIA VFX SDK User Guide](https://docs.nvidia.com/maxine/vfx/index.html)
- [NVIDIA Triton Inference Guide](https://docs.nvidia.com/maxine/triton/index.html)
- [NvCVImage API Guide](https://docs.nvidia.com/maxine/nvcvimage/index.html)

License
-------

- **Software license** - Refer to [LICENSE](LICENSE)
- **Third party licenses** - Refer to [external/ThirdPartyLicenses.txt](external/ThirdPartyLicenses.txt)
- **Sample data** - Refer to [resources/NVIDIA Maxine Sample Data License (2025.10.22).pdf](resources/NVIDIA%20Maxine%20Sample%20Data%20License%20(2025.10.22).pdf)
