#!/bin/bash

# Change container name/tag if needed in config.sh
source docker_config.sh

# check if SDK is installed
SDK=VideoFX
SDK_PATH="/usr/local/$SDK"
MODEL_DIR="$SDK_PATH/lib/models"

# Validate SDK installation
if [ ! -d "$SDK_PATH" ]; then
    echo "Error: $SDK not found at $SDK_PATH"
    echo "Please install $SDK first before running this script"
    exit 1
fi

# Do a basic check for model files
if [ -z "$(find ${MODEL_DIR} -iname "*.engine.trtpkg" -type f)" ]; then
    echo "ERROR: No models in model directory (searching inside ${MODEL_DIR})"
    echo "Use models/download_models.sh to download models before building container"
    exit 1
fi

# create SDK tarball
tar -cvf $SDK.tar -C /usr/local $SDK

# create public sample app tarball
# Location of this script (docker/) and the repo root (one level up)
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"

mapfile -t ITEMS < <(ls -A "$ROOT_DIR" | grep -v '^docker$' | sort)

if (( ${#ITEMS[@]} > 0 )); then
  OUT_TAR="$SCRIPT_DIR/samples.tar"
  tar -C "$ROOT_DIR" -cvf "$OUT_TAR" "${ITEMS[@]}"
  echo "Created: $OUT_TAR"
else
  echo "No items found to archive."
fi



# build docker image with SDK filename as build argument
sudo docker build --build-arg SDK_TAR=${SDK}.tar -t${CONTAINER_NAME}:${CONTAINER_TAG} -f Dockerfile .

# delete intermediate tarball
rm $SDK.tar
rm $OUT_TAR

