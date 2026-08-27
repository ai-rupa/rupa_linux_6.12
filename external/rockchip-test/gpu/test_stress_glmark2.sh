#!/bin/bash
# Backward compatibility wrapper - delegates to unified glmark2_test.sh
exec bash "$(dirname "$0")/glmark2_test.sh" --mode stress
