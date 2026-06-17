#!/bin/bash -e

RK_PACK_TOOL_DIR="$RK_SDK_DIR/tools/linux/Linux_Pack_Firmware/rockdev"

gen_sd_parameter()
{
	PARAMETER="${1:-parameter.txt}"

	{
		echo "# For SD card updating (loader,misc and recovery only)"
		echo

		# Copy header fields from original parameter
		sed -n '/^CMDLINE:/q;p' "$RK_FIRMWARE_DIR/parameter.txt" | grep .

		echo -n "CMDLINE: mtdparts=rk29xxnand:"

		OFFSET=0x00004000
		for part in uboot trust misc recovery; do
			IMAGE=${part}.img
			if ! [ -r $IMAGE ]; then
				if [ "$part" = trust ]; then
					continue
				fi

				# Required for SDcard updating
				error "$IMAGE is missing!"
				exit 1
			fi

			SIZE=$(stat -L -c %s "$IMAGE")
			SIZE=$(( (($SIZE + 0xFFFFF) & ~0xFFFFF) / 512 ))
			printf "0x%08x@0x%08x(%s)," "$SIZE" "$OFFSET" "$part"
			OFFSET=$(( "$OFFSET" + "$SIZE" ))
		done | sed 's/,$//'

		echo
	} > "$PARAMETER"
}

gen_package_file()
{
	TYPE="${1:-update}"
	PARAMETER="${2:-parameter.txt}"
	PKG_FILE="${3:-package-file}"

	if [ ! -r "$PARAMETER" ]; then
		error "Unable to parse $PARAMETER"
		exit 1
	fi

	{
		echo -e "# NAME\tPATH"
		echo -e "package-file\tpackage-file"
		echo -e "parameter\tparameter.txt"
	} > "$PKG_FILE"

	if [ "$TYPE" = template -o -r MiniLoaderAll.bin ]; then
		echo -e "bootloader\tMiniLoaderAll.bin" >> "$PKG_FILE"
	fi

	for part in $(rk_partition_parse_names "$PARAMETER"); do
		if echo $part | grep -q "_b$"; then
			# Not packing *_b partition for OTA
			if echo "$TYPE" | grep -wq "ota"; then
				continue
			fi
		fi

		case $part in
			backup)
				echo -e "backup\tRESERVED" >> "$PKG_FILE"
				continue
				;;
			system|system_[ab]) IMAGE=rootfs.img ;;
			*_a) IMAGE="${part%_a}.img" ;;
			*_b) IMAGE="${part%_b}.img" ;;
			*) IMAGE="$part.img" ;;
		esac

		if [ "$TYPE" = template -o -r "$IMAGE" ]; then
			echo -e "$part\t$IMAGE" >> "$PKG_FILE"
		fi
	done
}

do_build_updateimg()
{
	check_config RK_UPDATE || false

	TYPE="update${1:+-$1}"
	OUT_DIR="$RK_OUTDIR/$TYPE"
	IMAGE_DIR="$OUT_DIR/Image"
	TARGET="$RK_FIRMWARE_DIR/$TYPE.img"
	STORAGE="$RK_UPDATE_STORAGE"

	case "$TYPE" in
		*ota*)
			PKG_FILE="$RK_OTA_PACKAGE_FILE"
			PKG_FILE_EDIT_CMD="edit-ota-package-file"
			;;
		*)
			PKG_FILE="$RK_PACKAGE_FILE"
			PKG_FILE_EDIT_CMD="edit-package-file"
			;;
	esac

	# Make sure that the basic firmwares are ready
	if [ ! -r "$RK_FIRMWARE_DIR/parameter.txt" ]; then
		notice "Basic firmwares are not ready, building it..."
		RK_UPDATE= "$RK_SCRIPTS_DIR/mk-firmware.sh"
	fi

	# Make sure that the loader is ready
	if [ ! -r "$RK_FIRMWARE_DIR/MiniLoaderAll.bin" ]; then
		notice "Loader is not ready, building it..."
		"$RK_SCRIPTS_DIR/mk-loader.sh"
	fi

	message "=========================================="
	message "          Start packing $TYPE image"
	message "=========================================="

	if echo "$TYPE" | grep -wq "ab"; then
		if ! rk_partition_parse_names | grep -qE "_a\>|_b\>"; then
			error "Not having A/B partitions!"
			exit 1
		fi
	fi

	rm -rf "$TARGET" "$OUT_DIR"
	mkdir -p "$IMAGE_DIR"
	cd "$IMAGE_DIR"

	# Prepare images
	ln -rsf "$RK_FIRMWARE_DIR"/* .
	rm -f update.img

	if echo "$TYPE" | grep -wq "sd"; then
		# Use blank misc image
		rm -rf misc.img
		truncate -s 4M misc.img

		# Use generated parameter
		rm parameter.txt
		notice "Generating parameter.txt for $TYPE:"
		gen_sd_parameter
		cat parameter.txt

		# Use generated package-file
		unset PKG_FILE PKG_FILE_EDIT_CMD

		# Use default storage type
		unset STORAGE
	fi

	# Prepare package-file
	if [ "$PKG_FILE" ]; then
		PKG_FILE="$RK_CHIP_DIR/$PKG_FILE"
		if [ ! -r "$PKG_FILE" ]; then
			error "$PKG_FILE not exists!"
			exit 1
		fi
		ln -rsf "$PKG_FILE" package-file
	else
		notice "Generating package-file for $TYPE:"
		gen_package_file $TYPE
		cat package-file
	fi

	notice "Packing $TARGET for $TYPE..."

	if [ ! -r MiniLoaderAll.bin ]; then
		error "MiniLoaderAll.bin is missing!"
		exit 1
	fi

	TAG=RK$(dd if=MiniLoaderAll.bin bs=1 count=4 skip=21 status=none | rev)
	"$RK_PACK_TOOL_DIR/afptool" -pack ./ update.raw.img
	"$RK_PACK_TOOL_DIR/rkImageMaker" -$TAG MiniLoaderAll.bin \
		update.raw.img update.img -os_type:androidos \
		${STORAGE:+-storage:$STORAGE}

	ln -rsf "$IMAGE_DIR/package-file" "$OUT_DIR"
	ln -rsf "$IMAGE_DIR/update.img" "$OUT_DIR"
	ln -rsf "$IMAGE_DIR/update.img" "$TARGET"

	if echo "$TYPE" | grep -wq "ab"; then
		ln -sf "$(basename "$TARGET")" \
			"$RK_FIRMWARE_DIR/${TYPE/-ab/}.img"
	fi

	if [ "$PKG_FILE_EDIT_CMD" ]; then
		notice "\nRun 'make $PKG_FILE_EDIT_CMD' if you want to modify the package-file.\n"
	fi
}

build_updateimg()
{
	if [ "$RK_AB_UPDATE" ]; then
		notice "Making A/B update image..."
		do_build_updateimg ab
	else
		notice "Making update image..."
		do_build_updateimg
	fi

	if [ "$RK_SDCARD_UPDATE" ]; then
		build_sd_updateimg
	fi

	finish_build
}

build_ota_updateimg()
{
	notice "Making update image for OTA..."
	do_build_updateimg ota

	finish_build
}

build_sd_updateimg()
{
	check_config RK_SDCARD_UPDATE || false

	notice "Making update image for SDcard..."
	do_build_updateimg sd

	notice "Use SDDiskTool to flash update-sd.img to the SD card,"
	notice "then copy update.img to the root of the SD card as sdupdate.img.\n"

	finish_build
}

# Hooks

usage_hook()
{
	usage_oneline "edit-package-file" "edit package-file"
	usage_oneline "edit-ota-package-file" "edit package-file for OTA"
	usage_oneline "updateimg" "build update image"
	usage_oneline "ota-updateimg" "build update image for OTA"
	usage_oneline "sd-updateimg" "build update image for SDcard"
}

clean_hook()
{
	rm -rf "$RK_OUTDIR/*update*"
	rm -rf "$RK_FIRMWARE_DIR/*update.img"
}

INIT_CMDS="edit-package-file edit-ota-package-file"
init_hook()
{
	case "$1" in
		edit-package-file)
			BASE_CFG=RK_PACKAGE_FILE
			PKG_FILE=package-file
			;;
		edit-ota-package-file)
			BASE_CFG=RK_OTA_PACKAGE_FILE
			PKG_FILE=ota-package-file
			;;
		*) return 0 ;;
	esac

	load_config $BASE_CFG
	if ! check_config $BASE_CFG &>/dev/null; then
		sed -i '/$BASE_CFG/d' "$RK_CONFIG"
		echo "${BASE_CFG}_CUSTOM=y" >> "$RK_CONFIG"
		echo "$BASE_CFG=\"$PKG_FILE\"" >> "$RK_CONFIG"
		"$RK_SCRIPTS_DIR/mk-config.sh" olddefconfig &>/dev/null
		"$RK_SCRIPTS_DIR/mk-config.sh" savedefconfig &>/dev/null
	fi
}

PRE_BUILD_CMDS="edit-package-file edit-ota-package-file"
pre_build_hook()
{
	case "$1" in
		edit-package-file)
			check_config RK_PACKAGE_FILE || false
			PKG_FILE="$RK_CHIP_DIR/$RK_PACKAGE_FILE" ;;
		edit-ota-package-file)
			check_config RK_OTA_PACKAGE_FILE || false
			PKG_FILE="$RK_CHIP_DIR/$RK_OTA_PACKAGE_FILE"
			;;
		*) return 0 ;;
	esac

	PKG_FILE="$(realpath -m "$PKG_FILE")"
	if [ ! -r "$PKG_FILE" ]; then
		notice "Generating template $PKG_FILE"
		gen_package_file template \
			"$RK_CHIP_DIR/$RK_PARAMETER" "$PKG_FILE"
	fi
	eval ${EDITOR:-vi} "$PKG_FILE"

	finish_build $@
}

POST_BUILD_CMDS="updateimg ota-updateimg sd-updateimg"
post_build_hook()
{
	case "$1" in
		updateimg) build_updateimg ;;
		ota-updateimg) build_ota_updateimg ;;
		sd-updateimg) build_sd_updateimg ;;
		*) usage ;;
	esac
}

source "${RK_BUILD_HELPER:-$(dirname "$(realpath "$0")")/build-helper}"

case "$@" in
	edit-*package-file)
		init_hook $@
		pre_build_hook $@
		;;
	*) post_build_hook ${@:-updateimg} ;;
esac
