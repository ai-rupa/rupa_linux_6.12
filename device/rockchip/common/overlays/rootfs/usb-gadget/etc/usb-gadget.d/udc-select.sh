#!/bin/bash

TMP_DIR=${TMP_DIR:-/var/run/usb-gadget}
USB_INIT_FILE=$TMP_DIR/udc-select-init
USB_GADGET_SCRIPT=usb-gadget

# Check if both rockchip,usbc-orientation nodes exist
EXTCON_USB_PORT0="extcon-usb-port0"
EXTCON_USB_PORT1="extcon-usb-port1"

EXTCON_USB_PREFIX="/sys/devices/platform/extcon-usb"

UDC_SELECT_DEBUG=n

udc_select_msg()
{
	if declare -F usb_msg > /dev/null 2>&1; then
		usb_msg $@
	else
		if [ "$UDC_SELECT_DEBUG" == "y" ]; then
			echo $@
		fi
	fi
}

extcon_usb_property()
{
	echo "$EXTCON_USB_PREFIX/$1/of_node/$2"
}

extcon_usb_get_orientation()
{
	if grep -q "USB=1" $EXTCON_USB_PREFIX/$EXTCON_USB_PORT0/extcon/extcon*/state; then
		echo "$EXTCON_USB_PORT0"
	elif grep -q "USB=1" $EXTCON_USB_PREFIX/$EXTCON_USB_PORT1/extcon/extcon*/state; then
		echo "$EXTCON_USB_PORT1"
	fi
}

usb_udc_switch_mapping()
{
	# Initialize mapping array
	declare -A EXTCON_UDC_MAP

	# Get phandle for port0
	PHANDLE_EXTCON_PORT0=$(hexdump -v -e '4/1 "%02x"' "$(extcon_usb_property $EXTCON_USB_PORT0 phandle)" 2>/dev/null || echo "")
	if [ -n "$PHANDLE_EXTCON_PORT0" ]; then
		EXTCON_UDC_MAP["$EXTCON_USB_PORT0"]=""
	fi

	# Get phandle for port1
	PHANDLE_EXTCON_PORT1=$(hexdump -v -e '4/1 "%02x"' "$(extcon_usb_property $EXTCON_USB_PORT1 phandle)" 2>/dev/null || echo "")
	if [ -n "$PHANDLE_EXTCON_PORT1" ]; then
		EXTCON_UDC_MAP["$EXTCON_USB_PORT1"]=""
	fi

	# Get UDC nodes and their extcon phandle relationships
	if [ ${#EXTCON_UDC_MAP[@]} -gt 0 ]; then
		for udc in /sys/class/udc/*; do
			udc_name=$(basename "$udc")
			extcon_file="$udc/device/of_node/extcon"
			if [ -e "$extcon_file" ]; then
				udc_extcon_phandle=$(hexdump -v -e '4/1 "%02x"' "$extcon_file" 2>/dev/null || echo "")
				if [ -n "$udc_extcon_phandle" ]; then
					# Match with extcon ports
					if [ "$udc_extcon_phandle" = "$PHANDLE_EXTCON_PORT0" ]; then
						EXTCON_UDC_MAP["$EXTCON_USB_PORT0"]="$udc_name"
					elif [ "$udc_extcon_phandle" = "$PHANDLE_EXTCON_PORT1" ]; then
						EXTCON_UDC_MAP["$EXTCON_USB_PORT1"]="$udc_name"
					fi
				fi
			fi
		done

		# Record mapping to $USB_INIT_FILE
		{
			echo "# extcon-usb-port* to UDC mapping"
			# Check if both ports have UDC mappings
			UDC_SELECT_SWITCH="n"
			if [ -n "${EXTCON_UDC_MAP[extcon-usb-port0]}" ] && [ -n "${EXTCON_UDC_MAP[extcon-usb-port1]}" ]; then
				UDC_SELECT_SWITCH="y"
			fi
			echo "UDC_SELECT_SWITCH=$UDC_SELECT_SWITCH"

			# Output mappings in shell variable format
			for port in "${!EXTCON_UDC_MAP[@]}"; do
				udc="${EXTCON_UDC_MAP[$port]}"
				# Convert port name to valid shell variable name
				var_name=$(echo "$port" | tr '-' '_' | tr '.' '_')
				if [ -n "$udc" ]; then
					echo "${var_name}_UDC=\"$udc\""
				else
					echo "${var_name}_UDC=\"\""
				fi
			done
		} > "$USB_INIT_FILE" 2>/dev/null || true
	fi
}

udc_select_init()
{
	# Only proceed with mapping if both nodes exist
	if [ -e "$(extcon_usb_property $EXTCON_USB_PORT0 rockchip,usbc-orientation)" ] &&
	   [ -e "$(extcon_usb_property $EXTCON_USB_PORT1 rockchip,usbc-orientation)" ]; then
		usb_udc_switch_mapping
	else
		# If either node is missing, only record UDC_SELECT_SWITCH=n
		{
			echo "# extcon-usb-port* to UDC mapping"
			echo "UDC_SELECT_SWITCH=n"
		} > "$USB_INIT_FILE" 2>/dev/null || true
	fi
}

udc_select_run()
{
	if [ "$UDC_SELECT_SWITCH" = "y" ]; then
		ORIENTATION="$(extcon_usb_get_orientation)"
		if [ -n "$ORIENTATION" ]; then
			case "$ORIENTATION" in
				extcon-usb-port0|extcon-usb-port1)
					# Convert to variable name format
					var_name=$(echo "$ORIENTATION" | tr '-' '_' | tr '.' '_')_UDC
					# Get UDC from the variable
					eval "UDC_SELECT=\${$var_name}"
					# Check if UDC is valid
					if [ -z "$UDC_SELECT" ] || [ ! -d /sys/class/udc/$UDC_SELECT ]; then
						udc_select_msg "Invalid UDC '$UDC_SELECT' from init file for $ORIENTATION"
						UDC_SELECT="none"
						return
					fi
					udc_select_msg "UDC_SELECT_SWITCH=y, using UDC form init: $ORIENTATION:$UDC_SELECT"
					;;
				*)
					# Unable to run here
					udc_select_msg "ORIENTATION='$ORIENTATION' is not extcon-usb-port0 or extcon-usb-port1, exiting"
					UDC_SELECT="none"
					return
					;;
			esac
		else
			UDC_SELECT=$(cat "$USB_GADGET_DIR/UDC")
			if [ ! -n "$UDC_SELECT" ]; then
				UDC_SELECT=${UDC_SELECT:-$(ls /sys/class/udc/ | head -n 1)}
				udc_select_msg "UDC_SELECT_SWITCH=y but ORIENTATION is not set, using default UDC selection"
			else
				udc_select_msg "UDC_SELECT_SWITCH=y but ORIENTATION is not set, using existing UDC selection"
			fi
		fi
	else
		# Original UDC selection logic
		UDC_SELECT=${UDC_SELECT:-$(ls /sys/class/udc/ | head -n 1)}
	fi

	if [ -z "$UDC_SELECT" ] || [ ! -d /sys/class/udc/$UDC_SELECT ]; then
		udc_select_msg "Failed to find a valid UDC device..."
		UDC_SELECT="none"
	fi
}

# main

if [ "$1" == "Debug" ]; then
	UDC_SELECT_DEBUG=y
fi

if [ ! -f "$USB_INIT_FILE" ]; then
	udc_select_init
fi

# Source the init file to get variables
. "$USB_INIT_FILE" 2>/dev/null || true

udc_select_run

if [ "$(basename $0)" = "$USB_GADGET_SCRIPT" ]; then
	# source by $USB_GADGET_SCRIPT
	USB_UDC=$UDC_SELECT
else
	echo $UDC_SELECT
fi
