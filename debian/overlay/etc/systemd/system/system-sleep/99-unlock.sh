#!/bin/bash
case "$1" in
    post)
        sleep 1
        pkill -9 light-locker 2>/dev/null || true
        ;;
esac
