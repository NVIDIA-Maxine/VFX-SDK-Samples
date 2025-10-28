FROM ubuntu:20.04

# Define build argument for SDK filename
ARG SDK_TAR
# copy SDK. uptarred automatically
ADD ${SDK_TAR} /usr/local
# copy samples.tar
WORKDIR /app_src
ADD samples.tar /app_src
# build and install sample apps
RUN /app_src/build_samples.sh -y
