#!/bin/sh
# 
# Tips:
# $0 file name
# $$ curren script PID
# $! pre-command PID
# $? pre-command exit value
# $# parameter numbers
# $@ parameters
# $* parameters(as a sting)
# 
# SET ANSI COLORS {{{ START
C_RED="[1;31m" 
C_CYAN="[1;36m" 
C_BLUE="[1;34m" 
C_GREEN="[1;32m" 
C_WHITE="[1;37m" 
C_YELLOW="[1;33m" 
C_MAGENTA="[1;35m" 
C_NORMAL="[0;39m" 
# SET ANSI COLORS END }}}

set -e
cmd=`realpath $0`
BUILD_DIR=`dirname $cmd`
build_sdk_dir=$BUILD_DIR/..
build_sdk_dir_output=$BUILD_DIR/../output
build_sdk_dir_out=$build_sdk_dir_output/out
build_sdk_dir_image=$build_sdk_dir_output/image
PKG_NAME=$BUILD_DIR
export RK_JOBS=$((`getconf _NPROCESSORS_ONLN` / 2 + 1 ))

# target_build="arm-rockchip830-linux-uclibcgnueabihf arm-rockchip830-linux-gnueabihf aarch64-rockchip1031-linux-gnu"
target_build="arm-rockchip830-linux-uclibcgnueabihf "

for item in `echo $target_build`;
do
	rm -rf $build_sdk_dir_out/build-$item
done
for item in `echo $target_build`;
do
	item_build="$build_sdk_dir_out/build-$item"
	item_toolchain="$build_sdk_dir/tools/linux/toolchain/$item/bin/$item"
	depend_libs=$build_sdk_dir/project/app/component/third_libs/
	mkdir -p $item_build;
	cd $item_build && \
		cmake $PKG_NAME/ \
		-DCMAKE_C_COMPILER=${item_toolchain}-gcc \
		-DCMAKE_CXX_COMPILER=${item_toolchain}-g++ \
		-DCMAKE_INSTALL_PREFIX="${build_sdk_dir_image}" \
		-DCMAKE_BUILD_TYPE=Release \
		-DCMAKE_SYSROOT="$(${item_toolchain}-gcc -print-sysroot)" \
		-DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER \
		-DCMAKE_FIND_ROOT_PATH_MODE_PACKAGE=ONLY \
		-DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY \
		-DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY \
		-DCMAKE_NO_SYSTEM_FROM_IMPORTED=ON \
		-DCMAKE_VERBOSE_MAKEFILE=ON \
		-DCMAKE_C_FLAGS="-I$depend_libs/include -L$depend_libs/lib/$item -liconv" \
		-DCMAKE_CXX_FLAGS="-I$depend_libs/include -L$depend_libs/lib/$item -liconv" \
		-DCMAKE_SYSTEM_NAME=Linux
	make -j$RK_JOBS;
	make install;
done
tree $build_sdk_dir_image
