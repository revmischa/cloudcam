#!/usr/bin/env bash

AXIS_SDK_BASE=../../../axis
AXIS_SDK_1_4_ROOT=${AXIS_SDK_BASE}/emb-app-sdk_1_4
AXIS_SDK_2_0_ROOT=${AXIS_SDK_BASE}/emb-app-sdk_2_0_3
RUST_BUILD_IMAGE=messense/rust-musl-cross
AXIS_BUILDTYPE=$1   # mipsisa32r2el | armv6

case ${AXIS_BUILDTYPE} in
  mipsisa32r2el)
    RUST_TARGET=mipsel-unknown-linux-musl
    RUST_BUILD_IMAGE_TAG=mipsel-musl
    ;;
  armv6)
    RUST_TARGET=arm-unknown-linux-musleabi
    RUST_BUILD_IMAGE_TAG=arm-musleabi
    ;;
esac

if [ -z "${AXIS_BUILDTYPE}" ]
then
  echo "Please specify Axis built type: build-packages.sh [ mipsisa32r2el | armv6 ]"
  exit
fi

echo "Axis build type: ${AXIS_BUILDTYPE} Rust target: ${RUST_TARGET}"

# build Rust client
echo docker run --rm -it -v "$(pwd)/..":/home/rust/src ${RUST_BUILD_IMAGE}:${RUST_BUILD_IMAGE_TAG} cargo build --release
cp ../target/${RUST_TARGET}/release/cloudcam .

# build ACAP 1.4 application package
if [ ! -z "${AXIS_SDK_1_4_ROOT}" ]
then
  (cd ${AXIS_SDK_1_4_ROOT} && source init_env)
  rm *.o
  cp package-1.4.conf package.conf
  create-package.sh ${AXIS_BUILDTYPE}
fi

# build ACAP 2.0 application package
if [ ! -z "${AXIS_SDK_2_0_ROOT}" ]
then
  (cd $AXIS_SDK_2_0_ROOT && source init_env)
  rm *.o
  cp package-2.0.conf package.conf
  create-package.sh ${AXIS_BUILDTYPE}
fi
