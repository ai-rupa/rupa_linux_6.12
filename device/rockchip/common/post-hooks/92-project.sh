#!/bin/bash -e
#
# Install the board/version specific customizations of the project being built.
#
# Everything under debian/overlay/ is copied into the rootfs of *every* board,
# so board specific content must not live there.  It lives in
#
#     debian/project/<project>/<same layout as the rootfs>
#
# and is only installed when <project> is the one selected by the current
# defconfig:
#
#     debian/project/hugsun_rk3572_rp08b_v10/etc/default/led-ctl
#
# <project> is, in order of precedence:
#
#     1. $RK_PROJECT                  explicit, set from the defconfig
#     2. $RK_DEFCONFIG minus "_defconfig"
#     3. $RK_KERNEL_DTS_NAME
#
# The first one that exists wins, so several defconfigs can share a directory
# (and a project may carry a different name than its defconfig).
#
# Like SDK overlays, a project may ship an install.sh that is executed instead
# of the plain copy; it receives the target rootfs directory and the OS name.

source "${RK_POST_HELPER:-$(dirname "$(realpath "$0")")/post-helper}"

# Only the real debian rootfs: recovery/ramboot have no project content.
[ "$POST_OS" = debian ] || exit 0
[ "$POST_ROOTFS" ] || exit 0

PROJECT_ROOT="$RK_SDK_DIR/debian/project"

project_names()
{
	[ -z "$RK_PROJECT" ] || echo "$RK_PROJECT"
	[ -z "$RK_DEFCONFIG" ] || echo "${RK_DEFCONFIG%_defconfig}"
	[ -z "$RK_KERNEL_DTS_NAME" ] || echo "$RK_KERNEL_DTS_NAME"
}

install_project()
{
	local DIR="$1"

	if [ -x "$DIR/install.sh" ]; then
		notice "Handling project: $(basename "$DIR")..."
		"$DIR/install.sh" "$TARGET_DIR" "$POST_OS"
		return 0
	fi

	notice "Installing project: $(basename "$DIR") to $TARGET_DIR..."

	if command -v rsync >/dev/null 2>&1; then
		# Same policy as the SDK overlays: normalize permissions,
		# follow nothing outside, keep existing directory links.
		rsync -a --chmod=u=rwX,go=rX --copy-unsafe-links --keep-dirlinks \
			--exclude .empty --exclude .git --exclude /install.sh \
			"$DIR/" "$TARGET_DIR/"
	else
		warning "rsync not found, copying with cp (permissions kept as-is)"
		cp -a "$DIR/." "$TARGET_DIR/"
	fi
}

for project in $(project_names); do
	[ -d "$PROJECT_ROOT/$project" ] || continue

	install_project "$PROJECT_ROOT/$project"
	exit 0
done

notice "No project dir in $PROJECT_ROOT for" \
	"${RK_PROJECT:-${RK_DEFCONFIG:-$RK_KERNEL_DTS_NAME}}, nothing to install"
