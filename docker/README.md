# SDK container

AR/VideoFX SDK can run in an isolated container without being actually installed on the host system. This allows user to minimize the system impact while still be able to try AR/Video SDK new features.

## A. prerequisite software
- please refer to the AR/VideoFX SDK system guide to install Docker Engine and Nvidia Container Toolkit.

## B. Build Docker Image
- run the build_docker_image.sh script. 
```
    sudo bash build_docker_image.sh
```
The building script pulls base image (Ubuntu 20.04) from Docker Hub, installs AR/VideoFX SDK and required third party libraries, builds and installs the AR/VideoFX sample applications. If you want to add text editor like vi, you can modify the Dockerfile by appending commands "RUN apt update -y && apt install vim -y" before building the docker image. Total docker image building time depends on your system configurations and network speed, but should be around 5 minutes.

- Once finished, you will see a Docker image named "maxine_arsdk" or "maxine_videofx" created on your system using the "sudo docker image ls" command.

## C. Test the sample app inside container
- Launch the container by executing the script "./run_docker_image.sh".
- You can choose a directory on the host machine mapped to "/host" in the container by specifying "-m <host_dir>" for file sharing purposes. 
- Once the container is launched, you are ready to try the AR/VideoFX sample applications in the default installation path which is "/root/mysamples". 
- The triton sample app is not supported to run inside container either (due to the Triton server needs to be running in its own container).
- Only offline mode is supported inside the container (webcam mode is not supported). You can dump the output to "/host" directory so that you can evaluate it in the host system for video playback.
    - For ARSDK, if the effect comes with two scripts like run_xxx_webcam.sh and run_xxx_offline.sh, only use run_xxx_offline.sh.
    - For VideoFX SDK, remove the arguments "--webcam" and "--show" in the scripts you want to use.