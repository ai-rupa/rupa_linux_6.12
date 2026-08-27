#!/bin/bash
# benchmark/glmark2_test.sh - Glmark2 benchmark (delegates to GPU module)

SCRIPT_PATH="$(cd "$(dirname "$0")" && pwd)/../gpu/gpu_test.sh"

if [[ -x "$SCRIPT_PATH" ]]; then
    exec bash "$SCRIPT_PATH" --test fullscreen
else
    echo "Error: GPU test script not found at $SCRIPT_PATH" >&2
    exit 1
fi
