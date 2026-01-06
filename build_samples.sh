#!/bin/sh -eu
#
# script to build the video effects sdk sample apps
#
# - ask the location for the sample apps
# - install any pre-requisites via package manager
# - build any pre-requisites not available via package manager (if any)
# - build the sample apps
#
# usage:
#
#  build_samples.sh [ -f | --force ] [-c|--cleanup] [-i <i>|--installdir <i>] [-y|--yes]
#
# where:
#  -f | --force : force rebuild or reinstall of pre-requisites
#  -c | --cleanup : clean up build intermediates
#  -i <i> | --installdir <i> : installation directory (default ~/mysamples)
#  -y | --yes : no prompting - use default values
#

# there are exact requirements
_CUDAVER="12"
_TRTVER="10.9.0.34"
_CUDNNVER="9.7.1"


usage() { echo "usage: $0 [-i <dir> | --installdir <dir>] [-y|--yes] [-f|--force] [-p|--prereq]"; exit 0;}
die() { echo "$0: ERROR: $@" 1>&2; exit 1; }
progress() { echo "========== $@ =========="; }

check_exist() {
    msg="$1"
    shift
    for i; do if [ -e "$i" ]; then return; fi; done
    die "Could not find $msg: $@"
}

SDK_DIR="/usr/local/VideoFX"
# usually /usr/local/VideoFX/share or /usr/share/libvideofx
SAMPDIR=$(cd ${VFX_SAMPLES_DIR:-`dirname "$0"`}; pwd)
if [ ! -e "$SAMPDIR/build_samples.sh" ]
then
    die "cannot find samples directory"
fi

if [ -f /etc/os-release ]
then
    . /etc/os-release
    OS="$ID$VERSION_ID"
    case "$OS" in
        rocky8.*) OS="rocky8";;
        rocky9.*) OS="rocky9";;
        rhel8.*) OS="rhel8";;
        rhel9.*) OS="rhel9";;
        centos7 | ubuntu20.04 | ubuntu22.04 | ubuntu24.04 | debian12 );;
        *) die "unsupported linux version: $OS";;
    esac
else
    die "cannot determine os version"
fi

# process args
TEMP=$(getopt -o 'xfdci:ypb?' --long 'force,debug,cleanup,installdir:,yes,build,prereq,help' -- "$@")
if [ $? -ne 0 ]; then usage; fi
eval set -- "$TEMP"
unset TEMP

# for

_YES="false"
_FORCE="false"
_BUILD_TYPE="Release"
_INSTALLDIR=""
_CLEANUP="false"
_BUILD="true"
_PREREQ="true"
_PREGEN="true"
while true; do
    case "$1" in
        '-x') set -x;;                                      # script debug output
        '-f' | '--force') _FORCE="true";;                   # force build
        '-d' | '--debug') _BUILD_TYPE="Debug";;             # debug build
        '-c' | '--cleanup') _CLEANUP="true";;               # clean build
        '-i' | '--installdir') _INSTALLDIR="$2";shift;;
        '-y' | '--yes') _YES="true";;                       # answer yes to questions
        '-p' | '--prereq') _BUILD="false"; _PREGEN="true";; # pregenerate prerequisites
        '-b' | '--build') _PREREQ="false";;                 # build without checking prerequisites
        '--') shift; break;;
        *) usage;;
    esac
    shift
done


if ! $_YES
then
    echo ""
    echo "===================================================================="
    echo "This script will:"
    case "$OS" in
        centos7 | rhel* | rocky* )
            echo "- INSTALL pre-requisite packages using yum/dnf"
            echo "- BUILD pre-requisite packages (opencv + ffmpeg)"
            ;;
        *)
            echo "- INSTALL pre-requisite packages using apt-get"
            ;;
    esac
    echo "- build the sample apps to a location of your choice"
    echo ""
    echo "This may install many pre-requisite packages"
    case "$OS" in
        centos7 | rhel* | rocky*)
            echo "and the the build may take some time"
            ;;
    esac
    echo ""
    read -p "Ok to continue? (yes/no) " ans
    case "$ans" in
        [yY] | [Yy][Ee][Ss]);;
        *) echo "aborted."; exit 1;;
    esac
fi

if [ -z "$_INSTALLDIR" ] && ! $_YES; then
    echo ""
    echo "Please provide an absolute path for installation directory below, or use the default. "
    read -p "Install to [~/mysamples]: " _INSTALLDIR
fi

_INSTALLDIR="${_INSTALLDIR:-$HOME/mysamples}"
mkdir -p "$_INSTALLDIR/source"

progress "copy samples to $_INSTALLDIR"

echo "cp -r $SAMPDIR/* $_INSTALLDIR/source/"
cp -r $SAMPDIR/* "$_INSTALLDIR/source/"

#copy preinstalled packages if any
if [ -d "$_INSTALLDIR/pregen" ] && [ ! -d "$_INSTALLDIR/extras" ]
then
    progress "copy preinstalled packages to $_INSTALLDIR"
    cp -r "$_INSTALLDIR/pregen/" "$_INSTALLDIR/extras/"
fi

# containers don't require / provide sudo
if [ -x /usr/bin/sudo ]
then
    SUDO="sudo"
else
    SUDO=""
fi

if $_PREREQ
then

    progress "check prerequisities"

    case "$OS" in

        centos7)

            # also epel-release
            PRE_REQ_LIST="
            cmake3
            gcc
            gcc-c++
            git
            gtk2-devel
            jasper-devel
            libdc1394-devel
            libjpeg-turbo-devel
            libpng-devel
            libtiff-devel
            libv4l-devel
            libwebp-devel
            v4l-utils
            yasm

            autoconf
            automake
            libtool

            make
            wget
            "

            if $_FORCE || ! rpm -q epel-release $PRE_REQ_LIST
            then
                echo "installing required packages:"
                set -x
                $SUDO yum -y install epel-release
                $SUDO yum -y install $PRE_REQ_LIST
                set +x
            else
                echo "Required packages are already installed"
            fi

            CMAKE=cmake3

            ;;

        rocky8)
            PRE_REQ_LIST="
                git
                cmake
                gcc
                gcc-c++
                make
                python3
                python3-numpy
                platform-python-devel
                gtk2-devel
                findutils
                diffutils
                libdc1394
                libjpeg-turbo
                libjpeg-turbo-devel

                libv4l-devel

                autoconf
                automake
                libtool
            "

            if $_FORCE || ! rpm -q epel-release $PRE_REQ_LIST
            then
                echo "installing required packages:"
                set -xe
                # dnf will fail with missing package
                $SUDO yum -y install epel-release
                $SUDO dnf config-manager --set-enabled powertools
                $SUDO dnf -y install $PRE_REQ_LIST
                set +x
            else
                echo "Required packages are already installed"
            fi

            CMAKE=cmake

            ;;

        rhel8)
            PRE_REQ_LIST="
                git
                cmake
                gcc
                gcc-c++
                make
                python3-numpy
                platform-python-devel
                gtk2-devel
                findutils
                diffutils
                libdc1394
                libjpeg-turbo
                libjpeg-turbo-devel

                libv4l-devel

                autoconf
                automake
                libtool
            "

            if $_FORCE || ! rpm -q $PRE_REQ_LIST
            then
                echo "installing required packages:"
                if $_FORCE
                then echo "FORCE"
                else echo "no force"
                fi
                for i in $(echo "${PRE_REQ_LIST}")
                do
                    if rpm -q $i
                    then
                        echo "ok $i"
                    else
                        echo "need $i"
                    fi
                done
                set -xe
                # dnf will fail with missing package
                $SUDO subscription-manager repos --enable codeready-builder-for-rhel-8-$(arch)-rpms
                $SUDO dnf -y install $PRE_REQ_LIST
                set +x
            else
                echo "Required packages are already installed"
            fi

            CMAKE=cmake

            ;;

        rocky9)
            PRE_REQ_LIST="
                git
                cmake
                gcc
                gcc-c++
                make
                python3-numpy
                platform-python-devel
                gtk2-devel
                findutils
                diffutils
                libdc1394
                libjpeg-turbo
                libjpeg-turbo-devel

                libv4l-devel

                autoconf
                automake
                libtool
            "

            if $_FORCE || ! rpm -q epel-release $PRE_REQ_LIST
            then
                echo "installing required packages:"
                set -xe
                # dnf will fail with missing package
                $SUDO yum -y install epel-release
                $SUDO dnf config-manager --set-enabled crb
                $SUDO dnf -y install $PRE_REQ_LIST
                set +x
            else
                echo "Required packages are already installed"
            fi

            CMAKE=cmake

            ;;

        rhel9)
            PRE_REQ_LIST="
                git
                cmake
                gcc
                gcc-c++
                make
                python3-numpy
                platform-python-devel
                gtk2-devel
                findutils
                diffutils
                libdc1394
                libjpeg-turbo
                libjpeg-turbo-devel

                libv4l-devel

                autoconf
                automake
                libtool
            "

            if $_FORCE || ! rpm -q $PRE_REQ_LIST
            then
                echo "installing required packages:"
                set -xe
                $SUDO subscription-manager repos --enable codeready-builder-for-rhel-9-$(arch)-rpms
                $SUDO dnf -y install $PRE_REQ_LIST
                set +x
            else
                echo "Required packages are already installed"
            fi

            CMAKE=cmake

            ;;

        ubuntu20.04 | ubuntu22.04 | ubuntu24.04  )

            PRE_REQ_LIST="
            cmake
            g++
            make
            ffmpeg
            libopencv-dev
            libpng-dev
            libjpeg-turbo8-dev
            libssl-dev
            rapidjson-dev
            "

            if $_FORCE || ! dpkg --verify $PRE_REQ_LIST >/dev/null
            then
                echo "installing required packages:"
                set -x
                $SUDO apt-get update
                if [ -z "$SUDO" ]
                then
                    export DEBIAN_FRONTEND=noninteractive
                    apt-get install -y $PRE_REQ_LIST
                else
                    $SUDO DEBIAN_FRONTEND=noninteractive apt-get install -y $PRE_REQ_LIST
                fi
                set +x
            else
                echo "Required packages are already installed"
            fi

            CMAKE=cmake

            ;;

        debian12)

            PRE_REQ_LIST="
            cmake
            g++
            make
            ffmpeg
            libopencv-dev
            libpng-dev
            libjpeg62-turbo-dev
            "

            if $_FORCE || ! dpkg --verify $PRE_REQ_LIST >/dev/null
            then
                echo "installing required packages:"
                set -x
                $SUDO apt-get update
                if [ -z "$SUDO" ]
                then
                    export DEBIAN_FRONTEND=noninteractive
                    apt-get install -y $PRE_REQ_LIST
                else
                    $SUDO DEBIAN_FRONTEND=noninteractive apt-get install -y $PRE_REQ_LIST
                fi
                set +x
            else
                echo "Required packages are already installed"
            fi

            CMAKE=cmake

            ;;

        *) die "unexpected OS":;

    esac


    cd "$_INSTALLDIR"

    if [ "$OS" = "centos7" ] || [ "$OS" = "rocky8" ] || [ "$OS" = "rhel8" ] || \
       [ "$OS" = "rocky9" ] || [ "$OS" = "rhel9" ]
    then
        progress "build any additional prerequisites"

        case "$OS" in
            centos7|rocky8|rhel8)
                NASM_TAG="tags/nasm-2.15.05"
                FFMPEG_TAG="n4.3.2"
                OPENCV_TAG="3.4.6"
                ;;
            rocky9|rhel9)
                NASM_TAG="tags/nasm-2.15.05"
                FFMPEG_TAG="n4.3.2"
                OPENCV_TAG="4.6.0"
                ;;
            *) die "unexpected OS":;
        esac

        # fail on error
        set -e

        if [ -e $_INSTALLDIR/extras/bin/nasm ]  && ! $_FORCE
        then

            echo "nasm: already exists"

        else

            mkdir -p $_INSTALLDIR/build/nasm
            cd $_INSTALLDIR/build/nasm

            git clone https://github.com/netwide-assembler/nasm
            cd nasm
            git -c advice.detachedHead=false checkout $NASM_TAG

            sh autogen.sh
            sh configure --prefix="$_INSTALLDIR/extras"
            touch nasm.1 ndisasm.1 # cheat

            make -j$(nproc)
            make install

            cd $_INSTALLDIR/build
            $_CLEANUP && rm -rf $_INSTALLDIR/build/nasm
        fi

        export PATH="$PATH:$_INSTALLDIR/extras/bin"

        # -------------- x264 --------------

        if [ -e $_INSTALLDIR/extras/lib/libx264.a ] && ! $_FORCE
        then
            echo "libx264: already exists"
        else
            mkdir -p $_INSTALLDIR/build/x264
            cd $_INSTALLDIR/build/x264

            git clone https://git.videolan.org/git/x264.git
            cd x264
            #git checkout <ver>

            ./configure \
                --enable-static \
                --enable-strip \
                --enable-pic \
                --disable-cli \
                --disable-opencl \
                \
                --disable-avs     \
                --disable-swscale \
                --disable-lavf    \
                --disable-ffms    \
                --disable-gpac    \
                --disable-lsmash \
                \
                --prefix="$_INSTALLDIR/extras"

            make -j$(nproc)
            make install

            cd $_INSTALLDIR/build
            $_CLEANUP && rm -rf $_INSTALLDIR/build/x264
        fi

        # -------------- ffmpeg --------------
        if [ -e $_INSTALLDIR/extras/bin/ffmpeg ] && ! $_FORCE
        then
            echo "ffmpeg: already exists"
        else
            mkdir -p $_INSTALLDIR/build/ffmpeg
            cd $_INSTALLDIR/build/ffmpeg

            export PKG_CONFIG_PATH="$_INSTALLDIR/extras/lib/pkgconfig:${PKG_CONFIG_PATH:-}"

            echo "git clone FFmpeg"
            git -c advice.detachedHead=false clone -b $FFMPEG_TAG --depth=1 https://github.com/FFmpeg/FFmpeg

            cd FFmpeg
            git -c advice.detachedHead=false checkout tags/$FFMPEG_TAG

            ./configure \
                --enable-pic \
                --enable-shared \
                --enable-ffmpeg \
                --enable-ffplay \
                --enable-ffprobe \
                --enable-gpl \
                --enable-libx264 \
                --prefix="$_INSTALLDIR/extras" \
                --disable-doc

            make -j$(nproc)
            make install

            cd $_INSTALLDIR/build
            $_CLEANUP && rm -rf $_INSTALLDIR/build/ffmpeg

            #echo "/usr/local/lib" > /etc/ld.so.conf.d/ffmpeg.conf
            #ldconfig
        fi

        # -------------- opencv --------------

        export LD_LIBRARY_PATH="$_INSTALLDIR/extras/lib:/usr/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"

        if [ -e $_INSTALLDIR/extras/lib64/libopencv_core.so ]  && ! $_FORCE
        then
            echo "opencv: already exists"
        else

            export PKG_CONFIG_PATH="$_INSTALLDIR/extras/lib/pkgconfig:$_INSTALLDIR/extras/lib64/pkgconfig:/usr/lib64/pkgconfig:/usr/share/pkgconfig:${PKG_CONFIG_PATH:-/usr/share/pkgconfig}"
            export PKG_CONFIG_LIBDIR="$_INSTALLDIR/extras/lib:$_INSTALLDIR/extras/lib64:/usr/lib64:${PKG_CONFIG_LIBDIR:-/usr/lib64}"

            cd "$_INSTALLDIR/build"

            echo "git clone opencv"
            git -c advice.detachedHead=false clone https://github.com/opencv/opencv.git

            cd opencv
            git -c advice.detachedHead=false checkout tags/$OPENCV_TAG

            mkdir build
            cd build

            $CMAKE \
                -D CMAKE_INSTALL_PREFIX="$_INSTALLDIR/extras" \
                -D CMAKE_BUILD_TYPE=RELEASE \
                -D BUILD_opencv_stitching=OFF \
                -D WITH_1394=OFF \
                -D WITH_FFMPEG=ON \
                \
                -D WITH_GSTREAMER=OFF \
                -D WITH_V4L=ON \
                -D WITH_LIB4VL=ON \
                -D OPENCV_SKIP_PYTHON_LOADER=ON \
                -D OPENCV_GENERATE_PKGCONFIG=ON \
                -D ENABLE_CXX11=ON \
                \
                -D BUILD_opencv_apps=OFF \
                -D BUILD_DOCS=OFF \
                -D BUILD_PACKAGE=OFF \
                -D BUILD_PERF_TESTS=OFF \
                -D BUILD_TESTS=OFF \
                \
                -DBUILD_LIST=core,videoio,highgui,imgproc \
                ..

            make -j$(nproc)
            make install

            cd $_INSTALLDIR/build
            $_CLEANUP && rm -rf $_INSTALLDIR/build/opencv
        fi

        if $_PREGEN
        then
            cp -R $_INSTALLDIR/extras/ $_INSTALLDIR/pregen/
        fi

    fi

    progress "prerequisites for samples are complete"
fi

if $_BUILD
then

    progress "verify cuda / cudnn / tensorrt"

    if [ -d $SDK_DIR/external/cuda ] && [ -d $SDK_DIR/external/tensorrt ]
    then
        echo "found VideoFX tensorrt/cuda/cudnn"
        # cuda/cudnn
        export LD_LIBRARY_PATH="$SDK_DIR/external/cuda/lib${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
        export PATH="$SDK_DIR/external/cuda/bin:$PATH"

        # tensorrt
        export LD_LIBRARY_PATH="$SDK_DIR/external/tensorrt/lib:$LD_LIBRARY_PATH"
        export PATH="$SDK_DIR/external/tensorrt/bin:$PATH"
    else

        # 7.2.2.3 -> 7.2.2.
        _TRT_VER_SHORT=$(echo $_TRTVER | sed 's/^\([0-9]*\)\.\([0-9]*\)\.\([0-9]*\)\..*/\1.\2\.\3/')

        check_exist "CUDA" /usr/local/cuda-$_CUDAVER
        check_exist "TensorRT" /usr/local/TensorRT-$_TRTVER/lib/libnvinfer.so.$_TRT_VER_SHORT \
                    /usr/lib64/libnvinfer.so.$_TRT_VER_SHORT \
                    /usr/lib/$(arch)-linux-gnu/libnvinfer.so.$_TRT_VER_SHORT
        check_exist "cuDNN" /usr/local/cuda-$_CUDAVER/lib64/libcudnn.so.$_CUDNNVER \
                    /usr/lib64/libcudnn.so.$_CUDNNVER \
                    /usr/lib/$(arch)-linux-gnu/libcudnn.so.$_CUDNNVER

        export LD_LIBRARY_PATH="/usr/local/cuda-${_CUDAVER}/lib64${LD_LIBRARY_PATH:+:${LD_LIBRARY_PATH}}"
        export PATH="/usr/local/cuda-${_CUDAVER}/bin:$PATH"

        export LD_LIBRARY_PATH="/usr/local/TensorRT-${_TRTVER}/lib:${LD_LIBRARY_PATH}"
        export PATH="/usr/local/TensorRT-${_TRTVER}/bin:$PATH"
    fi

    progress "build sample apps"

    # quit on error
    set -e

    # build samples referencing the read-only source
    cd $_INSTALLDIR

    ${CMAKE:-cmake} \
        -DCMAKE_BUILD_TYPE=$_BUILD_TYPE \
        $_INSTALLDIR/source

    make -k


    echo ""
    echo "the sample apps have been created"
    echo "If you used sudo permissions to execute this build_samples.sh script"
    echo "please use sudo permissions when trying out the samples below as well"

    echo "You can try the samples using:"
    echo "  cd $_INSTALLDIR/apps"

    echo "or follow the instructions in the documentation found in:"
    echo "  $SAMPDIR/apps/doc"
    echo ""
    echo "you can modify the sources in $_INSTALLDIR/source and rebuild:"
    echo "  cd $_INSTALLDIR"
    echo "  $CMAKE $_INSTALLDIR/source"
    echo "  make"
fi
