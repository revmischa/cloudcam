#!/usr/bin/env bash -e

AXIS_SDK_BASE=/axis
AXIS_SDK_1_4_ROOT=${AXIS_SDK_BASE}/emb-app-sdk_1_4
AXIS_SDK_2_0_ROOT=${AXIS_SDK_BASE}/emb-app-sdk_2_0_3
RUST_BUILD_IMAGE=messense/rust-musl-cross
ACAP_BUILD_IMAGE_TAG=acap:2.0.3
AXIS_BUILDTYPE=$1   # mipsisa32r2el | arm

ACAP_RUN="docker run -ti -v $PWD:/client ${ACAP_BUILD_IMAGE_TAG}"

# if [ ! -d "$AXIS_SDK_2_0_ROOT" ]; then
    # echo "Missing ACAP SDK: ${AXIS_SDK_2_0_ROOT}"
    # exit 1
# fi

case ${AXIS_BUILDTYPE} in
  mipsisa32r2el)
    RUST_TARGET=mipsel-unknown-linux-musl
    RUST_BUILD_IMAGE_TAG=mipsel-musl
    ;;
  arm*)
    RUST_TARGET=arm-unknown-linux-musleabi
    RUST_BUILD_IMAGE_TAG=arm-musleabi
    ;;
esac

if [ -z "${AXIS_BUILDTYPE}" ]
then
  echo "Please specify Axis built type: build-packages.sh [ mipsisa32r2el | arm ]"
  exit
fi

echo "Axis build type: ${AXIS_BUILDTYPE} Rust target: ${RUST_TARGET}"

# build Rust client
echo docker run --rm -it -v "$(pwd)/..":/home/rust/src ${RUST_BUILD_IMAGE}:${RUST_BUILD_IMAGE_TAG} cargo build --release
cp ../target/${RUST_TARGET}/release/cloudcam ./cloudcam-exec

# (cd $AXIS_SDK_2_0_ROOT && source init_env)
rm -f *.o
# cp package-2.0.conf package.conf
# create-package.sh ${AXIS_BUILDTYPE}
${ACAP_RUN} /client/script/build-package-on-docker.sh ${AXIS_BUILDTYPE}
