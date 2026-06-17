#!/bin/sh

export AUTOVIDEOSINK_PREFERRED=kmssink
export PLAYBIN2_PREFERRED_VIDEOSINK=kmssink

# Try RGA 2D accel in videoconvert, videoscale and videoflip.
# NOTE: Might not success, and might behave different from the official plugin.
export GST_VIDEO_CONVERT_USE_RGA=1
export GST_VIDEO_FLIP_USE_RGA=1
