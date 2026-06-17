#!/bin/bash -e

cd "$TARGET_DIR"

rm -rfv sha256sum*

DEFAULT_USER="$(ls home 2>/dev/null| head -n 1)"
if [ "$DEFAULT_USER" ] && [ "$RK_DEBIAN_USER" != "$DEFAULT_USER" ]; then
	message "Changing default user ($DEFAULT_USER) to $RK_DEBIAN_USER..."

	if [ ! -d home/$RK_DEBIAN_USER ]; then
		mv -v home/$DEFAULT_USER home/$RK_DEBIAN_USER || true
	fi
	grep -rwl "$DEFAULT_USER" etc/ 2>/dev/null | \
		xargs sed -i "s/\<$DEFAULT_USER\>/$RK_DEBIAN_USER/g" \
		2>/dev/null || true
fi

DEFAULT_PASSWORD="$DEFAULT_USER"
if [ "$RK_DEBIAN_PASSWORD" != "$DEFAULT_PASSWORD" ]; then
	message "Changing default user's password..."
	echo "$RK_DEBIAN_USER:$RK_DEBIAN_PASSWORD" | chroot . chpasswd -m || true
fi

if ! [ "$RK_DEBIAN_UNIFIED_IMAGE" ]; then
	if grep -q rknpu "$RK_SDK_DIR/$RK_KERNEL_DTB"; then
		message "Extracting NPU2 tarball..."
		tar xvf rknpu2.tar || true
	fi

	PKGS=()

	if grep -q rkisp "$RK_SDK_DIR/$RK_KERNEL_DTB"; then
		PKGS+=(./camera_engine_rkaiq_${RK_CHIP_FAMILY#rk3566_}_*.deb)

		# HACK: Fallback to rkisp.
		if [ ${#PKGS[@]} -eq 0 ]; then
			PKGS+=(./camera_engine_rkisp_*.deb)
		fi
	fi

	# HACK: Drop unused RK3288 libmali packages.
	rm -f libmali-midgard-t76x-*-r0p0-*.deb

	GPU_CHIP="$(grep -w "$RK_CHIP_FAMILY" \
		"$RK_SDK_DIR/external/libmali/gpu-chips.txt" | cut -d':' -f1)"
	if [ "$GPU_CHIP" ]; then
		PKGS+=(./libmali-${GPU_CHIP}-*.deb)
	fi

	if [ ${#PKGS[@]} -gt 0 ]; then
		message "Installing chip related packages..."
		message "${PKGS[*]}"

		mkdir -pv var/cache/apt/archives/partial

		if ! chroot . bash -c "apt install -fy ${PKGS[*]}"; then
			warning "Package installation failed..."
		fi

		"$RK_TOOLS_DIR/x86_64/ldconfig" -r "$TARGET_DIR/" || true
	fi

	rm -rfv *.deb *.tar
	touch usr/local/first_boot_flag
fi
