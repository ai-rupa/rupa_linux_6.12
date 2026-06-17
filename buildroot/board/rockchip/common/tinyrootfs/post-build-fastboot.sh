#!/bin/bash

TARGET=$1

# cp those ko before the app runs
mkdir -p $TARGET/root/ko
cp -fv $TARGET/usr/lib/modules/*/kernel/fs/jbd2/jbd2.ko $TARGET/root/ko
cp -fv $TARGET/usr/lib/modules/*/kernel/fs/mbcache.ko $TARGET/root/ko
cp -fv $TARGET/usr/lib/modules/*/kernel/fs/ext4/ext4.ko $TARGET/root/ko
cp -fv $TARGET/usr/lib/modules/*/kernel/drivers/mmc/host/dw_mmc.ko $TARGET/root/ko
cp -fv $TARGET/usr/lib/modules/*/kernel/drivers/mmc/host/dw_mmc-pltfm.ko $TARGET/root/ko
cp -fv $TARGET/usr/lib/modules/*/kernel/drivers/mmc/host/dw_mmc-rockchip.ko $TARGET/root/ko
cp -fv $TARGET/usr/lib/modules/*/kernel/drivers/mmc/core/mmc_block.ko $TARGET/root/ko
rm -rf $TARGET/usr/lib/modules/

# TODO: mpp rockit need build-in
# rm $TARGET/usr/ko/insmod_ko.sh

exit 0
