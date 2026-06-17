#!/bin/bash -e

RK_SCRIPTS_DIR="${RK_SCRIPTS_DIR:-$(dirname "$(realpath "$0")")}"
RK_SDK_DIR="${RK_SDK_DIR:-$RK_SCRIPTS_DIR/../../../..}"

TARGET_IMG="$1"
ITS="$2"
KERNEL_IMG="$3"
KERNEL_DTB="$4"
RESOURCE_IMG="$5"
RAMDISK_IMG="$6"

if [ ! -f "$ITS" ]; then
	echo "$ITS not exists!"
	exit 1
fi

TMP_ITS=$(mktemp)
cp "$ITS" "$TMP_ITS"

if [ "$RK_FASTBOOT" ]; then
	notice "Fastboot rework its"

	if [ -z "$RAMDISK_IMG" ]; then
		kernel_r=`cat $TMP_ITS | grep entry | sed -E 's/.*entry = <(0x[0-9a-fA-F]+)>;.*/\1/'`
		KERNEL_GZ=$(mktemp)
		cat $KERNEL_IMG | gzip -n -f -9 > $KERNEL_GZ
		kernel_align_1mb_size=`stat -c %s $KERNEL_IMG | \
			     xargs -I{} bash -c 'echo $(( ( ({} + 1048575) / 1048576 ) * 1048576 ))' | \
			     xargs printf "0x%x"`
		kernel_c=$((kernel_r+kernel_align_1mb_size))
		printf -v kernel_c "0x%x" "$kernel_c"

		sed -i -e "s~@KERNEL_C@~$kernel_c~" \
			-e "s~@KERNEL_IMG@~$(realpath -q "$KERNEL_GZ")~" "$TMP_ITS"
		notice "kernel_r is " $kernel_r
		notice "kernel_c is " $kernel_c
	else
		notice "kernel dts" $(realpath -q "$KERNEL_DTB")
		DUMP_DTB_DTS=$(mktemp)

		dtc -I dtb -O dts -o $DUMP_DTB_DTS $(realpath -q "$KERNEL_DTB") 2>/dev/null
		ramdisk_c=`grep -A 3 -e ramdisk_c $DUMP_DTB_DTS | grep -w "reg" | awk -F\< '{print $2}' | awk '{print $1}'`
		ramdisk_r=`grep -A 3 -e ramdisk_r $DUMP_DTB_DTS | grep -w "reg" | awk -F\< '{print $2}' | awk '{print $1}'`
		ramdisk_c_size=`grep -A 3 -e ramdisk_c $DUMP_DTB_DTS | grep -w "reg" | awk -F'[<> ]' '{print $5}'`
		ramdisk_r_size=`grep -A 3 -e ramdisk_r $DUMP_DTB_DTS | grep -w "reg" | awk -F'[<> ]' '{print $5}'`

		rm -f "$DUMP_DTB_DTS"

		sed -i -e "s~@RAMDISK_C@~$ramdisk_c~" "$TMP_ITS"
		sed -i -e "s~@RAMDISK_R@~$ramdisk_r~" "$TMP_ITS"
		notice "ramdisk_c is " $ramdisk_c
		notice "ramdisk_c_size is " $ramdisk_c_size
		notice "ramdisk_r is " $ramdisk_r
		notice "ramdisk_r_size is " $ramdisk_r_size

		if [ $((ramdisk_c_size)) -lt `du -bL $RAMDISK_IMG | awk '{print $1}'` ]; then
			error "The size of $RAMDISK_IMG is larger than $ramdisk_c_size"
			error "Please check the ramdisk_c node of ${KERNEL_DTB%.*}.dts"
			exit 1
		fi

		if [ $((ramdisk_r_size)) -lt `du --apparent-size -sbL $RK_OUTDIR/rootfs/target 2>/dev/null | awk '{print $1}'` ]; then
			error "The size of $RK_OUTDIR/rootfs/target is larger than $ramdisk_r_size"
			error "Please check the ramdisk_r node of ${KERNEL_DTB%.*}.dts"
			exit 1
		fi
	fi
fi

if [ "$RK_SECURITY" ]; then
	echo "Security boot enabled, removing uboot-ignore ..."
	sed -i "/uboot-ignore/d" "$TMP_ITS"
fi

sed -i -e "s~@KERNEL_DTB@~$(realpath -q "$KERNEL_DTB")~" \
	-e "s~@KERNEL_IMG@~$(realpath -q "$KERNEL_IMG")~" \
	-e "s~@RAMDISK_IMG@~$(realpath -q "$RAMDISK_IMG")~" \
	-e "s~@RESOURCE_IMG@~$(realpath -q "$RESOURCE_IMG")~" "$TMP_ITS"

"$RK_SDK_DIR/rkbin/tools/mkimage" -f "$TMP_ITS"  -E -p 0x800 "$TARGET_IMG"
rm -f "$TMP_ITS"
rm -f "$KERNEL_GZ"
