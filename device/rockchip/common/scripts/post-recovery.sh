#!/bin/bash -e

# Redirect log output to serial console
if [ "$RK_RECOVERY_CONSOLE_LOG" ]; then
	message "Enabling serial console logging..."
	touch "$TARGET_DIR/.rkdebug"
fi
