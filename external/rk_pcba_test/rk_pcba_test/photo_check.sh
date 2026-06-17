#!/bin/sh

[ $# -eq 1 ] || { echo "Usage: $0 <image_file>"; exit 1; }

IMG_FILE="$1"
MIN_SIZE=5120

[ -f "$IMG_FILE" ] || { echo "ERROR: File not found"; exit 1; }

FILE_SIZE=$(stat -c%s "$IMG_FILE" 2>/dev/null || echo 0)
[ "$FILE_SIZE" -ge "$MIN_SIZE" ] || { echo "ERROR: File too small: $FILE_SIZE bytes"; exit 1; }

HEADER=$(head -c 3 "$IMG_FILE" | od -t x1 -N 3 | head -1 | awk '{print $2 $3 $4}')
echo "File header: $HEADER"

case "$HEADER" in
    "ffd8ff")
        echo "JPEG file header verified"
        FORMAT="JPEG"
        ;;
    "424d"*)  # BMP
        echo "BMP file detected"
        FORMAT="BMP"
        ;;
    "89504e"*) # PNG
        echo "PNG file detected"
        FORMAT="PNG"
        ;;
    *)
        # other file types
        echo "WARNING: Unrecognized file header: $HEADER"
        echo "File might be in raw YUV format, attempting to convert..."
        FORMAT="RAW"
        ;;
esac

case "$FORMAT" in
    "JPEG")
        # jpeg check
        if which gst-launch-1.0 >/dev/null 2>&1; then
            if gst-launch-1.0 filesrc location="$IMG_FILE" ! jpegdec ! fakesink silent=TRUE >/dev/null 2>&1; then
                echo "SUCCESS: JPEG image decoded successfully"
                exit 0
            else
                echo "ERROR: Failed to decode JPEG image - file may be corrupted"
                exit 1
            fi
        else
            echo "SUCCESS: Basic JPEG check passed"
            exit 0
        fi
        ;;
    "RAW")
        echo "Attempting to convert raw data to JPEG..."
        # ffmpeg
        if which ffmpeg >/dev/null 2>&1; then
            CONVERTED_FILE="${IMG_FILE%.*}_converted.jpg"
            for resolution in "640x480" "1280x720" "1920x1080"; do
                for format in "yuyv422" "nv12" "uyvy"; do
                    echo "Trying conversion: $resolution, $format"
                    if ffmpeg -f rawvideo -pixel_format "$format" -video_size "$resolution" -i "$IMG_FILE" -frames:v 1 "$CONVERTED_FILE" 2>/dev/null; then
                        if [ -f "$CONVERTED_FILE" ] && [ $(stat -c%s "$CONVERTED_FILE") -gt 1024 ]; then
                            echo "SUCCESS: Converted raw data to JPEG using $format, $resolution"
                            mv "$CONVERTED_FILE" "$IMG_FILE"
                            exit 0
                        fi
                        rm -f "$CONVERTED_FILE" 2>/dev/null
                    fi
                done
            done
        fi

        if which gst-launch-1.0 >/dev/null 2>&1; then
            CONVERTED_FILE="${IMG_FILE%.*}_converted.jpg"
            for caps in "video/x-raw,format=YUY2,width=640,height=480" \
                        "video/x-raw,format=NV12,width=640,height=480" \
                        "video/x-raw,format=I420,width=640,height=480"; do
                echo "Trying gstreamer conversion: $caps"
                if gst-launch-1.0 filesrc location="$IMG_FILE" ! "capsfilter caps=$caps" ! jpegenc ! filesink location="$CONVERTED_FILE" 2>/dev/null; then
                    if [ -f "$CONVERTED_FILE" ] && [ $(stat -c%s "$CONVERTED_FILE") -gt 1024 ]; then
                        echo "SUCCESS: Converted raw data to JPEG using gstreamer"
                        mv "$CONVERTED_FILE" "$IMG_FILE"
                        exit 0
                    fi
                    rm -f "$CONVERTED_FILE" 2>/dev/null
                fi
            done
        fi
        echo "WARNING: Could not convert raw data, but file exists and has content"
        RAW_FILE="${IMG_FILE%.*}.raw"
        mv "$IMG_FILE" "$RAW_FILE"
        echo "Renamed to: $RAW_FILE"
        echo "File contains data but is not in JPEG format"
        exit 1
        ;;
    "BMP"|"PNG")
        if which convert >/dev/null 2>&1; then
            CONVERTED_FILE="${IMG_FILE%.*}_converted.jpg"
            if convert "$IMG_FILE" "$CONVERTED_FILE" 2>/dev/null; then
                echo "SUCCESS: Converted $FORMAT to JPEG"
                mv "$CONVERTED_FILE" "$IMG_FILE"
                exit 0
            fi
        elif which ffmpeg >/dev/null 2>&1; then
            CONVERTED_FILE="${IMG_FILE%.*}_converted.jpg"
            if ffmpeg -i "$IMG_FILE" "$CONVERTED_FILE" 2>/dev/null; then
                echo "SUCCESS: Converted $FORMAT to JPEG"
                mv "$CONVERTED_FILE" "$IMG_FILE"
                exit 0
            fi
        fi
        echo "ERROR: $FORMAT file detected but no conversion tool available"
        exit 1
        ;;
    *)
        echo "ERROR: Unsupported file format"
        exit 1
        ;;
esac