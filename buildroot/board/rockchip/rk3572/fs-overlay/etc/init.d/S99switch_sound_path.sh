#!/bin/sh

# Define sound card and extcon device names
CARD="rk730-es7210"
TARGET_NAME="rk730-es7210-sound"

case "$1" in
    start)
        echo "Detecting headset state..."

        # 1. Locate the state file path for the specific extcon device
        # Iterate through all extcon devices to find the one matching TARGET_NAME
        STATE_FILE=""
        for d in /sys/class/extcon/extcon*; do
            if [ "$(cat $d/name 2>/dev/null)" = "$TARGET_NAME" ]; then
                STATE_FILE="$d/state"
                break
            fi
        done

        # 2. If no matching node is found, default to Speaker and exit
        if [ -z "$STATE_FILE" ] || [ ! -f "$STATE_FILE" ]; then
            echo "Extcon node not found, defaulting to Speaker"
            /usr/bin/alsaucm -c "$CARD" set _verb HiFi set _enadev Speaker
            exit 0
        fi

        # 3. Read the state and switch the audio path
        # grep -q returns 0 if HEADPHONE=1 is found, indicating headset is plugged in
        if grep -q "HEADPHONE=1" "$STATE_FILE"; then
            echo "Headset plugged in. Switching to Headphones."
            /usr/bin/alsaucm -c "$CARD" set _verb HiFi set _enadev Headphones
        else
            echo "No headset detected. Switching to Speaker."
            /usr/bin/alsaucm -c "$CARD" set _verb HiFi set _enadev Speaker
        fi
        ;;
    stop)
        # No specific action needed on shutdown
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
