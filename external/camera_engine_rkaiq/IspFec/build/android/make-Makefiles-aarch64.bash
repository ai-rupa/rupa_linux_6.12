#/bin/bash

export PATH=/data/project_codes/android_tool/cmake-3.22.1-linux-x86_64/bin:$PATH
export ANDROID_NDK=/data/project_codes/android_tool/android-ndk-r25c
export AIQ_BUILD_ARCH=aarch64

OUTPUT=$(pwd)/output/${AIQ_BUILD_ARCH}
SOURCE_PATH=$OUTPUT/../../../../

mkdir -p $OUTPUT
pushd $OUTPUT

cmake -G "Ninja" \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI="arm64-v8a" \
    -DANDROID_NDK=$ANDROID_NDK \
    -DANDROID_PLATFORM=android-28 \
    -DCMAKE_EXPORT_COMPILE_COMMANDS=OFF \
    -DRKAIQ_BUILD_BINARY_IQ=OFF \
    -DARCH=${AIQ_BUILD_ARCH} \
    -DCMAKE_BUILD_TYPE=MinSizeRel \
    -DISP_HW_VERSION=${ISP_HW_VERSION} \
    -DRKFEC_TARGET_SOC=${RKFEC_TARGET_SOC} \
    -DCMAKE_INSTALL_PREFIX="installed" \
    -DENABLE_SAMPLE_ISPFEC=ON \
    -DRK_GDC_GEN_MESH_ONLINE=ON \
    $SOURCE_PATH \
&& ninja -j$(nproc) \
&& ninja install

popd
