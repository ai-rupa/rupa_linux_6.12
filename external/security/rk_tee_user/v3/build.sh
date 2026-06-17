#!/bin/bash -e
WORK_DIR_TOP=$(cd `dirname $0` ; pwd)

#./build.sh or ./build.sh 3232 to compile CA TA with 32 bits
#./build.sh 3264 to compile CA with 32 bits and TA with 64 bits
#./build.sh 6464 to compile CA TA with 64 bits
#./build.sh 6432 to compile CA with 64 bits and TA with 32 bits
#./build.sh ta to compile TA with 32 bits

TOOLCHAIN_PREBUILTS=$WORK_DIR_TOP/../../../../../../tools/linux/toolchain
if [ -d "$TOOLCHAIN_PREBUILTS" ]; then
	TOOLCHAIN_PATH_ARM32=arm-rockchip830-linux-uclibcgnueabihf/bin/
	TOOLCHAIN_PATH_AARCH64=aarch64-rockchip830-linux-uclibcgnu/bin/
	CROSS_COMPILE32=arm-rockchip830-linux-uclibcgnueabihf-
	CROSS_COMPILE64=aarch64-rockchip830-linux-uclibcgnu-
	export TOOLCHAIN_UCLIBC=y
	if [ ! -d "$TOOLCHAIN_PREBUILTS/$TOOLCHAIN_PATH_ARM32" ]; then
		TOOLCHAIN_PATH_ARM32=arm-rockchip831-linux-uclibcgnueabihf/bin/
		CROSS_COMPILE32=arm-rockchip831-linux-uclibcgnueabihf-
	fi
	if [ ! -d "$TOOLCHAIN_PREBUILTS/$TOOLCHAIN_PATH_AARCH64" ]; then
		TOOLCHAIN_PATH_AARCH64=aarch64-rockchip831-linux-uclibcgnu/bin/
		CROSS_COMPILE64=aarch64-rockchip831-linux-uclibcgnu-
	fi
else
	TOOLCHAIN_PREBUILTS=$WORK_DIR_TOP/../../../../prebuilts
	if [ ! -d "$TOOLCHAIN_PREBUILTS" ]; then
		TOOLCHAIN_PREBUILTS=$WORK_DIR_TOP/../../../prebuilts
	fi
	TOOLCHAIN_PATH_ARM32=gcc/linux-x86/arm/gcc-arm-10.3-2021.07-x86_64-arm-none-linux-gnueabihf/bin/
	TOOLCHAIN_PATH_AARCH64=gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/
	CROSS_COMPILE32=arm-none-linux-gnueabihf-
	CROSS_COMPILE64=aarch64-none-linux-gnu-
fi

function check_toolchain32(){
	if [ ! -d "$TOOLCHAIN_PREBUILTS/$TOOLCHAIN_PATH_ARM32" ] && [ ! -d "$TOOLCHAIN_PATH_ARM32" ]; then
		echo "Toolchain32 error! Need toolchain version: $TOOLCHAIN_PATH_ARM32"
		echo "You can get it from following address:"
		echo "https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-a/downloads"
		exit -1
	fi
}

function check_toolchain64(){
	if [ ! -d "$TOOLCHAIN_PREBUILTS/$TOOLCHAIN_PATH_AARCH64" ] && [ ! -d "$TOOLCHAIN_PATH_AARCH64" ]; then
		echo "Toolchain64 error! Need toolchain version: $TOOLCHAIN_PATH_AARCH64"
		echo "You can get it from following address:"
		echo "https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-a/downloads"
		exit -1
	fi
}

if [ -z "$AARCH64_TOOLCHAIN" ]; then
	AARCH64_TOOLCHAIN=$TOOLCHAIN_PREBUILTS/$TOOLCHAIN_PATH_AARCH64/$CROSS_COMPILE64
else
	AARCH64_TOOLCHAIN=`readlink -f $AARCH64_TOOLCHAIN`
	TOOLCHAIN_PATH_AARCH64=`dirname $AARCH64_TOOLCHAIN`
fi

if [ -z "$ARM32_TOOLCHAIN" ]; then
	ARM32_TOOLCHAIN=$TOOLCHAIN_PREBUILTS/$TOOLCHAIN_PATH_ARM32/$CROSS_COMPILE32
else
	ARM32_TOOLCHAIN=`readlink -f $ARM32_TOOLCHAIN`
	TOOLCHAIN_PATH_ARM32=`dirname $ARM32_TOOLCHAIN`
fi

make TA_DEV_KIT_DIR=$WORK_DIR_TOP/export-ta_arm32 clean

export OPENSSL_INSTALL_PATH=$WORK_DIR_TOP/host/openssl

BUILD_CATA_BITS="$1"

if [ "$BUILD_CATA_BITS" == "3232" ]; then
	check_toolchain32
	export BUILD_CA=y
	make CROSS_COMPILE_HOST=$ARM32_TOOLCHAIN \
	CROSS_COMPILE_TA=$ARM32_TOOLCHAIN \
	CROSS_COMPILE_user_ta=$ARM32_TOOLCHAIN \
	CROSS_COMPILE=$ARM32_TOOLCHAIN \
	TA_DEV_KIT_DIR=$WORK_DIR_TOP/export-ta_arm32 \
	COMPILE_NS_USER=32 \
	O=$WORK_DIR_TOP/out
fi

if [ "$BUILD_CATA_BITS" == "3264" ]; then
	check_toolchain32
	check_toolchain64
	export BUILD_CA=y
	make CROSS_COMPILE_HOST=$ARM32_TOOLCHAIN \
	CROSS_COMPILE_TA=$AARCH64_TOOLCHAIN \
	CROSS_COMPILE_user_ta=$AARCH64_TOOLCHAIN \
	CROSS_COMPILE=$AARCH64_TOOLCHAIN \
	TA_DEV_KIT_DIR=$WORK_DIR_TOP/export-ta_arm64 \
	COMPILE_NS_USER=32 \
	O=$WORK_DIR_TOP/out
fi

if [ "$BUILD_CATA_BITS" == "6464" ]; then
	check_toolchain64
	export BUILD_CA=y
	make CROSS_COMPILE_HOST=$AARCH64_TOOLCHAIN \
	CROSS_COMPILE_TA=$AARCH64_TOOLCHAIN \
	CROSS_COMPILE_user_ta=$AARCH64_TOOLCHAIN \
	CROSS_COMPILE=$AARCH64_TOOLCHAIN \
	TA_DEV_KIT_DIR=$WORK_DIR_TOP/export-ta_arm64 \
	COMPILE_NS_USER=64 \
	O=$WORK_DIR_TOP/out
fi

if [ "$BUILD_CATA_BITS" == "6432" ]; then
	check_toolchain32
	check_toolchain64
	export BUILD_CA=y
	make CROSS_COMPILE_HOST=$AARCH64_TOOLCHAIN \
	CROSS_COMPILE_TA=$ARM32_TOOLCHAIN \
	CROSS_COMPILE_user_ta=$ARM32_TOOLCHAIN \
	CROSS_COMPILE=$ARM32_TOOLCHAIN \
	TA_DEV_KIT_DIR=$WORK_DIR_TOP/export-ta_arm32 \
	COMPILE_NS_USER=64 \
	O=$WORK_DIR_TOP/out
fi

if [ "$BUILD_CATA_BITS" == "" ]; then
	check_toolchain32
	export BUILD_CA=y
	make CROSS_COMPILE_HOST=$ARM32_TOOLCHAIN \
	CROSS_COMPILE_TA=$ARM32_TOOLCHAIN \
	CROSS_COMPILE_user_ta=$ARM32_TOOLCHAIN \
	CROSS_COMPILE=$ARM32_TOOLCHAIN \
	TA_DEV_KIT_DIR=$WORK_DIR_TOP/export-ta_arm32 \
	COMPILE_NS_USER=32 \
	O=$WORK_DIR_TOP/out
fi

if [ "$BUILD_CATA_BITS" == "ta" ]; then
	check_toolchain32
	make CROSS_COMPILE_HOST=$ARM32_TOOLCHAIN \
	CROSS_COMPILE_TA=$ARM32_TOOLCHAIN \
	CROSS_COMPILE_user_ta=$ARM32_TOOLCHAIN \
	CROSS_COMPILE=$ARM32_TOOLCHAIN \
	TA_DEV_KIT_DIR=$WORK_DIR_TOP/export-ta_arm32 \
	COMPILE_NS_USER=32 \
	O=$WORK_DIR_TOP/out
fi
