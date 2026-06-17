#!/bin/bash -e

# Hooks

usage_hook()
{
	usage_oneline "shell" "setup a shell for developing"
	usage_oneline "buildroot-shell" "setup a shell for buildroot developing"
	usage_oneline "bshell" "alias of buildroot-shell"
	usage_oneline "yocto-shell" "setup a shell for yocto developing"
	usage_oneline "yshell" "alias of yocto-shell"
}

PRE_BUILD_CMDS="shell buildroot-shell bshell yocto-shell yshell"
pre_build_hook()
{
	warning "Doing this is dangerous and for developing only."
	# No error handling in develop shell.
	set +e; trap ERR

	case "${1:-shell}" in
		yocto-shell | yshell)
			YOCTO_DIR="$RK_SDK_DIR/yocto"
			"$RK_SCRIPTS_DIR/mk-rootfs.sh" yocto-config
			LANG=en_US.UTF-8 LANGUAGE=en_US.en LC_ALL=en_US.UTF-8 \
				/bin/bash -c "cd $YOCTO_DIR; \
					source oe-init-build-env; \
					PS1='\u@\h:\w (yocto-$RK_CHIP)\$ ' \
					/bin/bash -norc"
			;;
		buildroot-shell | bshell)
			BUILDROOT_DIR="$RK_SDK_DIR/buildroot"
			BUILDROOT_CFG="${2:-$RK_BUILDROOT_CFG}"
			"$RK_SCRIPTS_DIR/mk-buildroot.sh" "$BUILDROOT_CFG" prepare
			/bin/bash -c "cd $BUILDROOT_DIR; \
				source scripts/envsetup.sh \
				$(realpath $BUILDROOT_DIR/output/rootfs_latest); \
				PS1='\u@\h:\w ($BUILDROOT_CFG)\$ ' \
				/bin/bash -norc"
			;;
		*)
			export PATH="$RK_SCRIPTS_DIR:$PATH"
			PS1="\u@\h:\w (rksdk)\$ " /bin/bash --norc
			;;
	esac

	warning "Exit from $BASH_SOURCE ${@:-shell}."
}

source "${RK_BUILD_HELPER:-$(dirname "$(realpath "$0")")/build-helper}"

pre_build_hook $@
