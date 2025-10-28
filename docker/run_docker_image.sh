#!/bin/bash

_HOST_DIR=`pwd`

usage() { echo "Usage: $0 
	-m <host_dir>    map host directory to /host in the container for file sharing. Current directory as default" 1>&2;exit 1; }

while [[ $# -gt 0 ]]; do
  case $1 in
    -m|--map)
      _HOST_DIR="$(realpath $2)"
      shift # past argument
      shift # past value
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

sudo docker run --rm --gpus=all -v$_HOST_DIR:/host -it ${CONTAINER_NAME}:${CONTAINER_TAG} /bin/bash
