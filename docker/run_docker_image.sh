#!/bin/bash

_HOST_DIR=`pwd`
_VERBOSE=0

usage() { echo "Usage: $0 
	-m <host_dir>    map host directory to /host in the container for file sharing. Current directory as default
	-v               verbose output" 1>&2;exit 1; }

while [[ $# -gt 0 ]]; do
  case $1 in
    -m|--map)
      _HOST_DIR="$(realpath $2)"
      shift # past argument
      shift # past value
      ;;
    -v|--verbose)
      _VERBOSE=1
      shift
      ;;
    -h|--help)
      usage
      exit 1
      ;;
    -*|--*)
      echo "Unknown option $1"
      usage
      exit 1
      ;;
  esac
done

source docker_config.sh

# Find libnvidia-ngx.so.* from host (part of NVIDIA display driver)
# The installation path varies depending on how the driver was installed
# Must mount both symlinks and their targets for the library to work
_NGX_MOUNT=""
_NGX_FILES=$(ldconfig -p 2>/dev/null | grep "libnvidia-ngx.so" | awk '{print $NF}')
if [ -z "$_NGX_FILES" ]; then
  # Fallback: search common NVIDIA driver paths
  _NGX_FILES=$(find /usr/lib/x86_64-linux-gnu /usr/lib/nvidia* /usr/lib64 /lib/x86_64-linux-gnu -name "libnvidia-ngx.so*" 2>/dev/null)
fi
if [ -n "$_NGX_FILES" ]; then
  for _NGX_FILE in $_NGX_FILES; do
    if [ -e "$_NGX_FILE" ]; then
      _NGX_MOUNT="$_NGX_MOUNT -v${_NGX_FILE}:${_NGX_FILE}:ro"
      [ $_VERBOSE -eq 1 ] && echo "Mapping: $_NGX_FILE"
      # If it's a symlink, also mount the target file
      if [ -L "$_NGX_FILE" ]; then
        _NGX_TARGET=$(readlink -f "$_NGX_FILE")
        if [ -f "$_NGX_TARGET" ]; then
          _NGX_MOUNT="$_NGX_MOUNT -v${_NGX_TARGET}:${_NGX_TARGET}:ro"
          [ $_VERBOSE -eq 1 ] && echo "Mapping: $_NGX_TARGET (symlink target)"
        fi
      fi
    fi
  done
fi
if [ -z "$_NGX_MOUNT" ]; then
  echo "Error: libnvidia-ngx.so.* not found on host. Please ensure NVIDIA display driver is installed."
  exit 1
fi

sudo docker run --rm --gpus=all -v$_HOST_DIR:/host $_NGX_MOUNT -it ${CONTAINER_NAME}:${CONTAINER_TAG} /bin/bash
