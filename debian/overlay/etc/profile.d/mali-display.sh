#!/bin/bash
# Mali GPU display backend configuration

# Auto-detect or default to wayland for newer Mali drivers
if [ -z "$MALI_DEFAULT_DISPLAY" ]; then
    if [ "$XDG_SESSION_TYPE" = "wayland" ]; then
        export MALI_DEFAULT_DISPLAY=wayland
    elif [ "$XDG_SESSION_TYPE" = "x11" ]; then
        export MALI_DEFAULT_DISPLAY=x11
    else
        export MALI_DEFAULT_DISPLAY=wayland
    fi
fi
