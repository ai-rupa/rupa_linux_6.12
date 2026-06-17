#!/bin/bash -e

BR2_VENDOR=rockchip

if [ -z "$BASH_SOURCE" ];then
	echo "Not in bash, switching to it..."
	"$(find . -maxdepth 3 -name envsetup.sh | head -n 1 || echo /bin/bash)"
fi

# Export global functions
set -a

bpkg()
{
	case "${1:-dir}" in
		dir)
			if [ -n "$2" ]; then
				find "$BUILDROOT_OUTPUT_DIR/build/" -maxdepth 1 \
					-type d -name "*$2*" | head -n 1 || \
					echo "no pkg build dir for $2." >&2
			else
				echo $(realpath "$PWD") | \
					grep -oE ".*/output/[^/]*/build/[^/]*" || \
					echo "not in a pkg build dir." >&2
			fi
			;;
		*)
			bpkg dir $1
			;;
	esac
}

bpkg_run()
{
	DIR=$(bpkg dir $2)
	[ -d "$DIR" ] || return 1

	for stage in $1; do
		case "$stage" in
			reconfig) bpkg_run "configure build install deploy" $2 ;;
			rebuild) bpkg_run "build install deploy" $2 ;;
			reinstall) bpkg_run "install deploy" $2 ;;
			reconfig-update)
				bpkg_run "configure build install update" $2 ;;
			rebuild-update) bpkg_run "build install update" $2 ;;
			reinstall-update) bpkg_run "install update" $2 ;;
			configure|build|deploy|update)
				SCRIPT="$DIR/.$stage.sh"
				[ -x "$SCRIPT" ] || return 1
				"$SCRIPT" || return 1
				;;
			install)
				SCRIPT="$DIR/.staging_install.sh"
				if [ -x "$SCRIPT" ]; then
					"$SCRIPT" || return 1
				fi

				SCRIPT="$DIR/.target_install.sh"
				if [ -x "$SCRIPT" ]; then
					"$SCRIPT" || return 1

					SCRIPT="$DIR/.image_install.sh"
					if [ -x "$SCRIPT" ]; then
						"$SCRIPT" || return 1
					fi

					continue
				fi

				SCRIPT="$DIR/.host_install.sh"
				if [ -x "$SCRIPT" ]; then
					"$SCRIPT" || return 1
					continue
				fi

				return 1
				;;
			*)
				echo "Unknown stage: $stage"
				return 1
				;;
		esac
	done
}

set +a
# End of global functions

choose_board()
{
	echo "Pick a board:"
	echo ""

	echo ${BOARD_ARRAY[@]} | xargs -n 1 | sed "=" | sed "N;s/\n/. /"

	local INDEX
	while true; do
		read -p "Which would you like? [1]: " INDEX
		INDEX=$((${INDEX:-1} - 1))

		if echo $INDEX | grep -vq [^0-9] && \
			[ "${BOARD_ARRAY[$INDEX]}" ]; then
			BR2_DEFCONFIG="${BOARD_ARRAY[$INDEX]}_defconfig"
			break
		fi

		echo
		echo "Choice not available. Please try again."
		echo
	done
}

parse_board()
{
	unset BUILDROOT_OUTPUT_DIR BR2_DEFCONFIG

	# Handle output dir with .config
	if [ -r "$1/.config" ]; then
		ARG="$1/.config"
	else
		ARG="$1"
	fi
	case "$ARG" in
		*/.config)
			if [ ! -r "$ARG" ]; then
				# without .config
				ARG="$(dirname "$ARG")"
			else
				BR2_DEFCONFIG="$(grep "BR2_DEFCONFIG=" "$ARG" | \
					cut -d'=' -f2 | xargs basename)"
				if [ ! -r "$BUILDROOT_DIR/configs/$BR2_DEFCONFIG" ]; then
					# .config without defconfig
					unset BR2_DEFCONFIG
				fi

				BUILDROOT_OUTPUT_DIR="$(dirname "$ARG")"
				return
			fi
			;;
	esac

	# Handle output dir
	if [ -d "$ARG" ]; then
		BUILDROOT_OUTPUT_DIR="$ARG"
		BR2_DEFCONFIG="$(basename "$ARG")_defconfig"

		if [ ! -r "$BUILDROOT_DIR/configs/$BR2_DEFCONFIG" ]; then
			echo "No available defconfig for $ARG}"
			return 1
		fi
		return
	fi

	# Handle defconfig pattern
	BOARD_ARRAY=($(cd "$BUILDROOT_DIR/configs/"; ls ${BR2_VENDOR}_* | \
		grep "$(basename "$1")" | sed "s/_defconfig$//" | sort))
	BOARD_ARRAY_LEN=${#BOARD_ARRAY[@]}

	case $BOARD_ARRAY_LEN in
		0)
			echo "No available configs${1:+" for: $1"}"
			return 1
			;;
		1)
			BR2_DEFCONFIG="${BOARD_ARRAY[0]}_defconfig"
			;;
		*)
			if [ "$ARG" = ${BOARD_ARRAY[0]} ]; then
				# Prefer exact-match
				BR2_DEFCONFIG="${ARG}_defconfig"
			else
				choose_board
			fi
			;;
	esac

	BUILDROOT_OUTPUT_DIR="$BUILDROOT_DIR/output/${BR2_DEFCONFIG%_defconfig}"
}

setup_board()
{
	BMAKE="make -C $BUILDROOT_DIR O=$BUILDROOT_OUTPUT_DIR"

	if [ -z "$BR2_DEFCONFIG" ]; then
		echo "Continue without defconfig..."
		${BMAKE} olddefconfig &>/dev/null
		return 0
	fi

	${BMAKE} "$BR2_DEFCONFIG"

	CONFIG="$BUILDROOT_OUTPUT_DIR/.config"
	cp "$CONFIG" "$CONFIG.new"
	mv "$CONFIG.old" "$CONFIG" &>/dev/null || return 0

	${BMAKE} olddefconfig &>/dev/null

	if ! diff "$CONFIG" "$CONFIG.new"; then
		read -t 10 -p "Found old config, override it? (y/n):" YES
		[ "$YES" = "n" ] || cp "$CONFIG.new" "$CONFIG"
	fi
}

main()
{
	SCRIPTS_DIR="$(dirname "$(realpath "$BASH_SOURCE")")"
	BUILDROOT_DIR="$(dirname "$SCRIPTS_DIR")"

	echo -e "\n############### Rockchip Buildroot SDK ###############\n"

	if ! parse_board "$1"; then
		return 1
	fi

	if ! setup_board; then
		return 1
	fi

	export BUILDROOT_DIR="$(realpath "$BUILDROOT_DIR")"
	export BUILDROOT_OUTPUT_DIR="$(realpath "$BUILDROOT_OUTPUT_DIR")"

	# Export helper alias
	set -a

	# Set alias
	alias broot='cd "$BUILDROOT_DIR"'
	alias bmake='make -C "$BUILDROOT_OUTPUT_DIR"'

	alias bconfig='bpkg_run configure'
	alias bbuild='bpkg_run build'
	alias binstall='bpkg_run install'
	alias bdeploy='bpkg_run deploy'
	alias bupdate='bpkg_run update'

	alias breconfig='bpkg_run reconfig'
	alias brebuild='bpkg_run rebuild'
	alias breinstall='bpkg_run reinstall'
	alias breconfig-update='bpkg_run reconfig-update'
	alias brebuild-update='bpkg_run rebuild-update'
	alias breinstall-update='bpkg_run reinstall-update'

	set +a
	# End of helper alias
}

if main "$@" && [ "$BASH_SOURCE" == "$0" ]; then
	echo This script is executed directly
	/bin/bash
fi
