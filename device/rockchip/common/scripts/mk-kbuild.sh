#!/bin/sh -e

case "$(uname -m)" in
	armv7l) KERNEL_ARCH=armhf ;;
	aarch64) KERNEL_ARCH=aarch64 ;;
	*)
		echo -e "\e[35mThis script is not for $(uname -m)\e[0m"
		exit 1
		;;
esac

set -a

if [ ! -d debian/ ]; then
	echo -e "\e[35m"
	echo "debian/ is not exists!"
	echo "Please download it from:"
	echo "https://salsa.debian.org/kernel-team/linux"
	echo "Tag:"
	echo "6.1: debian/6.1.76-1"
	echo "6.12: debian/6.12.33-1"
	echo -e "\e[0m"
	exit 1
fi

env -u ABINAME -u ARCH -u FEATURESET -u FLAVOUR -u VERSION -u LOCALVERSION >/dev/null

KERNAL_ARCH=${1:-$KERNAL_ARCH}
KVER3=$(grep -A 2 "^VERSION = " Makefile | cut -d' ' -f 3 | paste -sd'.')
KVER2=$(grep -A 1 "^VERSION = " Makefile | cut -d' ' -f 3 | paste -sd'.')
GVER=$(git log --oneline -1 | cut -d' ' -f1)

DISTRIBUTION_OFFICIAL_BUILD=1
DISTRIBUTOR="Rockchip"
DISTRIBUTION_VERSION="$KVER3"
KBUILD_BUILD_TIMESTAMP="$(date +%Y_%m_%d)"
KBUILD_BUILD_VERSION_TIMESTAMP="Debian $KBUILD_BUILD_TIMESTAMP - Rockchip ($GVER)"
KBUILD_BUILD_USER="Rockchip"
KBUILD_BUILD_HOST="Rockchip"

DEB_CFLAGS_SET="-static"
DEB_CPPFLAGS_SET="-static"
DEB_LDFLAGS_SET="-static"

CUR_DIR="$PWD"
OUT_DIR="$CUR_DIR/output"
BUILD_DIR="$OUT_DIR/build"
DESTDIR="$OUT_DIR/linux-kbuild"

make_subdir()
{
	SUBDIR=$1
	shift

	mkdir -p "$BUILD_DIR/$SUBDIR"
	make -j8 -s KCFLAGS="-fdebug-prefix-map=$PWD/=" \
		-C "$BUILD_DIR/$SUBDIR" \
		-f "$CUR_DIR/debian/rules.d/$SUBDIR/Makefile" \
		top_srcdir="$CUR_DIR" top_rulesdir="$CUR_DIR/debian/rules.d" \
		OUTDIR=$SUBDIR VERSION=$KVER2 KERNEL_ARCH=$KERNEL_ARCH \
		KBUILD_HOSTCFLAGS="-static" KBUILD_HOSTLDFLAGS="-static -lz" \
		HOSTCC="gcc -static" HOSTLD="ld -static" LDFLAGS="-static -lz" \
		$@
}

echo
echo "Packing linux-kbuild into $DESTDIR ..."
echo

sed -i 's/\(-lcrypto$\)/\1 -ldl -lpthread/' debian/rules.d/scripts/Makefile

make_subdir scripts
make_subdir tools/objtool
make_subdir scripts install
make_subdir tools/objtool install
