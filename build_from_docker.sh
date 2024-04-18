#!/bin/bash
BUILD_DIR=$1
#rm -rf ${BUILD_DIR}
cmake --preset release-linux-full -B${BUILD_DIR}
cmake --build ${BUILD_DIR} --target package
mkdir -p deploy
cp ${BUILD_DIR}*.zip ${BUILD_DIR}/*.deb ${BUILD_DIR}/*.rpm deploy/ 2>/dev/null || :
