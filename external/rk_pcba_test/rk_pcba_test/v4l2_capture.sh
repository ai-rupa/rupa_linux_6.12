#!/bin/sh

CAM_DEV="$1"
OUT_DIR="/tmp/camera"
IMG_FILE="$OUT_DIR/capture_$(date +%s).jpg"

mkdir -p "$OUT_DIR"
echo "Using camera device: $CAM_DEV"

if [ ! -e "$CAM_DEV" ]; then
    echo "ERROR: Camera device does not exist: $CAM_DEV"
    exit 1
fi

if [ ! -r "$CAM_DEV" ]; then
    echo "ERROR: Cannot read camera device: $CAM_DEV (check permissions)"
    exit 1
fi

echo "Checking camera device info..."
if ! v4l2-ctl --device="$CAM_DEV" --info; then
    echo "ERROR: Cannot get camera device info"
    echo "Trying alternative detection method..."
    if ! v4l2-ctl --device="$CAM_DEV" --list-formats 2>/dev/null; then
        echo "ERROR: Camera device not accessible: $CAM_DEV"
        exit 1
    fi
fi

echo "Available formats:"
v4l2-ctl --device="$CAM_DEV" --list-formats-ext 2>/dev/null || echo "Cannot list formats"

# fswebcam
if which fswebcam >/dev/null 2>&1; then
    echo "Attempting capture with fswebcam..."
    for resolution in "640x480" "1280x720" "320x240"; do
        echo "Trying resolution: $resolution"
        if fswebcam -d "$CAM_DEV" -r "$resolution" --no-banner --jpeg 85 -S 5 "$IMG_FILE" 2>/dev/null; then
            echo "SUCCESS: Captured with fswebcam at $resolution"
            break
        fi
        rm -f "$IMG_FILE" 2>/dev/null
    done
fi

# v4l2-ctl
if [ ! -f "$IMG_FILE" ] && v4l2-ctl --device="$dev" --list-formats >/dev/null 2>&1; then
    echo "Attempting capture with v4l2-ctl..."
    echo "Checking supported formats..."
    v4l2-ctl --device="$dev" --list-formats-ext
    for format in "MJPG" "JPEG" "YUYV"; do
        for resolution in "640x480" "1280x720" "320x240"; do
            echo "Trying format: $format, resolution: $resolution"
            if v4l2-ctl --device="$dev" --set-fmt-video=width=${resolution%x*},height=${resolution#*x},pixelformat=$format 2>/dev/null; then
                echo "Format set successfully: $format, $resolution"
                if v4l2-ctl --device="$dev" --stream-mmap --stream-to="$IMG_FILE" --stream-count=1 2>/dev/null; then
                    if [ -f "$IMG_FILE" ]; then
                        FILE_SIZE=$(stat -c%s "$IMG_FILE" 2>/dev/null || echo 0)
                        echo "Captured $FILE_SIZE bytes"
                        HEADER=$(head -c 3 "$IMG_FILE" | od -t x1 -N 3 | head -1 | awk '{print $2 $3 $4}' 2>/dev/null)
                        echo "File header: $HEADER"
                        if [ "$HEADER" = "ffd8ff" ]; then
                            echo "SUCCESS: Valid JPEG captured with v4l2-ctl using $format, $resolution"
                            break 2
                        else
                            echo "WARNING: Captured file is not JPEG (header: $HEADER)"
                            rm -f "$IMG_FILE"
                        fi
                    fi
                fi
            fi
        done
    done
fi

if [ ! -f "$IMG_FILE" ] && which gst-launch-1.0 >/dev/null 2>&1; then
    echo "Attempting capture with gstreamer..."
    if gst-launch-1.0 -e v4l2src device="$CAM_DEV" num-buffers=1 ! jpegenc ! filesink location="$IMG_FILE" 2>/dev/null; then
        echo "SUCCESS: Captured with gstreamer"
    else
        rm -f "$IMG_FILE" 2>/dev/null
    fi
fi

if [ ! -f "$IMG_FILE" ]; then
    echo "ERROR: All capture methods failed"
    echo "Diagnostic info:"
    echo "1. Device check:"
    ls -la "$CAM_DEV"
    echo "2. Available video devices:"
    ls -la /dev/video*
    echo "3. Kernel messages:"
    dmesg | tail -20
    exit 1
fi

FILE_SIZE=$(stat -c%s "$IMG_FILE" 2>/dev/null || echo 0)
if [ "$FILE_SIZE" -lt 1024 ]; then
    echo "ERROR: Capture failed - file too small: $FILE_SIZE bytes"
    rm -f "$IMG_FILE"
    exit 1
fi

echo "SUCCESS: Captured $FILE_SIZE bytes to $IMG_FILE"
exit 0