#!/bin/bash
# flash_test/flash_stress_test.sh - Flash write/read stress test
# Usage: flash_stress_test.sh [dirnum] [looptime]

DIR_FLASH="$(cd "$(dirname "$0")" && pwd)"
source "${DIR_FLASH}/../common/common.sh"

test_dir=/userdata/rockchip-test/flash_test
source_dir=$test_dir/src_test_data
dest_dir=$test_dir/des_test_data
md5_dir=$test_dir/md5_data

test_max_dir="${1:-5}"
test_max_count="${2:-200}"

log_info "flash" "Flash stress test: dirs=$test_max_dir, loops=$test_max_count"

TEST_ID=$(test_begin "flash" "stress" "Flash stress: $test_max_dir dirs, $test_max_count loops")

count=0
dir_loop=0
rm -rf "$test_dir/test_log.txt"
mkdir -p "$dest_dir" "$md5_dir" "$source_dir"

# Generate random test files
rm -rf "$source_dir"/*
file_size=(512 1024 3456 512 1024 3456)
file_radio=(5 5 5 5 5 5)
file_size_count=${#file_size[*]}

for ((i = 0; i < file_size_count; i++)); do
    for ((n = 0; n < ${file_radio[$i]}; n++)); do
        if [[ $i -eq 0 ]]; then
            rand=$((RANDOM % file_size[$i] + 1))
        else
            rand=$((RANDOM % (file_size[$i] - file_size[$i-1]) + file_size[$i-1] + 1))
        fi
        dd if=/dev/urandom of="$source_dir/test.$i.$rand.bin" bs=$rand count=1024 2>/dev/null
    done
done

cd "$source_dir"
md5sum ./* > "$md5_dir/source.md5"
cd /

set +e
while [[ $count -lt $test_max_count ]]; do
    log_info "flash" "Loop $count/$test_max_count"
    echo "$count" >> "$test_dir/test_log.txt"
    dir_loop=0

    while [[ $dir_loop -lt $test_max_dir ]]; do
        rm -rf "$dest_dir/$dir_loop"
        if [[ $? -ne 0 ]]; then
            log_error "flash" "Failed to clean $dest_dir/$dir_loop"
            test_end "$TEST_ID" "FAIL" "Clean failed at loop $count dir $dir_loop"
            exit 1
        fi

        cp -rf "$source_dir" "$dest_dir/$dir_loop"
        if [[ $? -ne 0 ]]; then
            log_error "flash" "Copy failed: $source_dir to $dest_dir/$dir_loop"
            test_end "$TEST_ID" "FAIL" "Copy failed at loop $count dir $dir_loop"
            exit 1
        fi
        dir_loop=$((dir_loop + 1))
    done

    dir_loop=0
    sync && echo 3 > /proc/sys/vm/drop_caches 2>/dev/null
    sleep 5
    sync

    while [[ $dir_loop -lt $test_max_dir ]]; do
        cd "$dest_dir/$dir_loop"
        md5sum ./* > "$md5_dir/dest${dir_loop}.md5"
        cd /

        if diff "$md5_dir/source.md5" "$md5_dir/dest${dir_loop}.md5" >/dev/null 2>&1; then
            log_debug "flash" "Check loop $count dir $dir_loop: OK"
            rm -f "$md5_dir/dest${dir_loop}.md5"
            rm -rf "$dest_dir/$dir_loop"
        else
            log_error "flash" "MD5 mismatch at loop $count dir $dir_loop"
            test_end "$TEST_ID" "FAIL" "MD5 mismatch at loop $count dir $dir_loop"
            exit 1
        fi
        dir_loop=$((dir_loop + 1))
    done

    count=$((count + 1))
    echo "-----------------------------------------" >> "$test_dir/test_log.txt"
done
set -e

log_info "flash" "Flash stress test completed successfully"
test_end "$TEST_ID" "PASS" "Completed $test_max_count loops with $test_max_dir dirs"
echo "-------copy and check success------------" >> "$test_dir/test_log.txt"
