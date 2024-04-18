#!/bin/bash

export CLANG_VERSION=16
#export CC=clang-$CLANG_VERSION
#export CXX=clang++-$CLANG_VERSION

# Local build, with system libs
if [ -f /etc/debian_version ]; then
    BUILD_DEPENDENCIES_DEBIAN="build-essential cmake lld ninja-build libboost-dev libboost-fiber-dev libboost-test-dev pkg-config spirv-tools spirv-headers libcurl4-openssl-dev libomp-dev libclang-$CLANG_VERSION-dev"
    # libze-dev nvidia-cuda-dev
    dpkg-checkbuilddeps -d "${BUILD_DEPENDENCIES_DEBIAN// /, }" /dev/null &>/dev/null
    if [[ $? != 0 ]]; then
        sudo apt -y install ${BUILD_DEPENDENCIES_DEBIAN}
    fi
fi
if [ ! -d build.release ]; then
    sudo cmake --preset release-linux
fi
cmake --build --preset default --target package

# Docker builds
declare -a docker_variants=("debian" "ubuntu")
for variant in "${docker_variants[@]}"
do
    docker build -f install/Dockerfile.${variant} -t thijswithaar/acpp-${variant} .
    docker run --rm --user $(id -u):$(id -g) -v `pwd`:/tmp -w /tmp thijswithaar/acpp-${variant} ./build_from_docker.sh build.${variant}
done

# Test
# sudo apt install --reinstall ./build/hipSYCL-24.2.0-Linux.deb
#if [ ! -d -Bbuild.examples ]; then
#   cmake -DACPP_TARGETS="hip:gfx1103" -DAdaptiveCpp_DIR=/lib/cmake/AdaptiveCpp -Sexamples -Bbuild.examples
#fi
#cmake --build build.examples
# HIPSYCL_DEBUG_LEVEL=9 ./build.examples/bruteforce_nbody/bruteforce_nbody
