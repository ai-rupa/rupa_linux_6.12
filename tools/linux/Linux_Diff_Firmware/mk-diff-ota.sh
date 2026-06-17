#!/bin/bash

OLD_IMG=$1
NEW_IMG=$2
DIFF_IMG=$3

if [ x$OLD_IMG = x"" -o x$NEW_IMG = x"" -o x$DIFF_IMG = x"" ]; then
  echo "Usage: $0 [old_img] [new_img] [diff_img]"
  exit 1
fi

echo "To differ image from OLD: $1 and NEW: $2"

echo_red()
{
  echo -e "\033[31m$1\033[0m"
}

DIR=$(cd $(dirname $0); pwd)
export PATH=$PATH:"$DIR"

check_tool()
{
  which "$1" &> /dev/null
  if [ $? -ne 0 ]; then
    echo "command: $1 not found!"
    exit 1;
  fi
}

get_fs_type()
{
  file_info=$(file -b $1)
  if [[ "$file_info" == *"Squashfs"* ]]; then
    echo -n Squashfs
  elif [[ "$file_info" == *"ext4"* ]]; then
    echo -n ext4
  else
    echo -n unsupported
  fi
}

# Check if file starts with UBI# magic (UBI image)
is_ubi_image()
{
  local magic=$(hexdump -n 4 -e '4/1 "%c"' "$1" 2>/dev/null)
  if [ "$magic" = "UBI#" ]; then
    return 0
  fi
  return 1
}

# Extract squashfs from UBI image using ubireader_extract_images.
# Usage: extract_squashfs_from_ubi <ubi_image> <output_squashfs>
extract_squashfs_from_ubi()
{
  local ubi_img=$1
  local output=$2
  local tmp_dir=$(mktemp -d)

  check_tool ubireader_extract_images

  echo "Extracting squashfs from UBI using ubireader..."
  ubireader_extract_images -o "$tmp_dir" "$ubi_img" || cleanup "ubireader_extract_images failed: $?"
  local sqfs=$(find "$tmp_dir" -type f | head -1)
  if [ -z "$sqfs" ]; then
    rm -rf "$tmp_dir"
    cleanup "No volume extracted from UBI image: $ubi_img"
  fi
  cp "$sqfs" "$output"
  rm -rf "$tmp_dir"

  local verify=$(file -b "$output")
  if [[ "$verify" != *"Squashfs"* ]]; then
    cleanup "Extracted file is not squashfs: $verify"
  fi
  echo "Extracted squashfs: $(stat --printf=%s $output) bytes"
}

check_tool bsdiff
check_tool rkImageMaker
check_tool afptool
check_tool md5sum

OLD_DIR=$(mktemp -d --suffix=-OLD -p ${DIR})
NEW_DIR=$(mktemp -d --suffix=-NEW -p ${DIR})
DIFF_DIR=$(mktemp -d --suffix=-DIFF -p ${DIR})

cleanup()
{
  echo -e "\033[41;33m $1 \033[0m"
  exit 1
}

# Get chip tag from MiniLoaderAll.bin header (4 bytes at offset 21, reversed)
get_chip_from_loader()
{
  local loader=$1
  local tag=RK$(dd if="$loader" bs=1 count=4 skip=21 status=none | rev)
  echo -n -${tag}
}

# diff_img <name> <old_file> <new_file> <diff_file>
diff_img()
{
  name=$1
  diff=$4
  old=$2
  new=$3
  old_size=$(stat --printf=%s $old)
  new_size=$(stat --printf=%s $new)
  md5sum=$(md5sum $new | cut -f 1 -d ' ')

  echo_red "Diff ${NAME}..."
  sec_1=$(date +%s)
  bsdiff "$old" "$new" "${diff}" || cleanup "bsdiff $new: $?"
  sec_2=$(date +%s)
  # MAGIC HEADER 80 bytes
  printf "DIFF:%-15s:%-12s:%-12s:%-32s:" $name $old_size $new_size $md5sum  >> "${diff}"
  echo_red "Diff ${NAME} use $((($sec_2 - $sec_1)/3600))h:$((($sec_2 - $sec_1)%3600/60))m:$((($sec_2 - $sec_1)%60))s"
}

echo "Unpacking old image: ${OLD_IMG}"
rkImageMaker -unpack ${OLD_IMG} ${OLD_DIR} || cleanup "rkImageMaker unpack: $?"
afptool -unpack ${OLD_DIR}/firmware.img ${OLD_DIR} || cleanup "afptool unpack: $?"

echo "Unpacking new image: ${NEW_IMG}"
rkImageMaker -unpack ${NEW_IMG} ${NEW_DIR} || cleanup "rkImageMaker unpack: $?"
afptool -unpack ${NEW_DIR}/firmware.img ${NEW_DIR} || cleanup "afptool unpack: $?"

mkdir ${DIFF_DIR}/Image

# Copy parameter/loader to target dir
cp ${NEW_DIR}/package-file ${DIFF_DIR}/
cp ${NEW_DIR}/MiniLoaderAll.bin ${DIFF_DIR}/
cp ${NEW_DIR}/MiniLoaderAll.bin ${DIFF_DIR}/Image
cp ${NEW_DIR}/parameter.txt ${DIFF_DIR}/
cp ${NEW_DIR}/parameter.txt ${DIFF_DIR}/Image

while read LINE; do
  # Strip trailing Windows carriage return (\r) for cross-platform compatibility
  LINE=${LINE%$'\r'}
  # delete any blank at beginning or "#" line
  STRIP=$(eval echo "$LINE")
  if [[ $STRIP = "" ]]; then
    continue
  fi

  # get the paratition name by deleting the chars after [[:blank:]]
  NAME=${STRIP/%[[:blank:]]*/}
  IMG=${STRIP/#*[[:blank:]]/}

  if [[ $NAME = "backup" ]]; then
    echo_red "Skip backup, it's not a real image"
    continue
  fi

  if [ $NAME = "package-file" -o $NAME = "parameter" -o $NAME = "bootloader" ]; then
    echo_red "Copy ${IMG}"
    # Already copied
    continue
  fi

  if [ ! -f ${OLD_DIR}/${IMG} ]; then
    echo_red "Copy ${IMG}, there's not corresponding old file"
    ln -s ${NEW_DIR}/${IMG} ${DIFF_DIR}/${IMG}
    continue
  fi

  case "${NAME%_[ab]}" in
    uboot | trust | boot | recovery)
      diff_img $NAME ${OLD_DIR}/${IMG} ${NEW_DIR}/${IMG} ${DIFF_DIR}/${IMG}
      ;;
    rootfs | system)
      old_is_ubi=0
      new_is_ubi=0
      old_sqfs=""
      new_sqfs=""

      is_ubi_image ${OLD_DIR}/${IMG} && old_is_ubi=1
      is_ubi_image ${NEW_DIR}/${IMG} && new_is_ubi=1

      if [ $old_is_ubi -ne $new_is_ubi ]; then
        echo_red "Copy ${IMG}, image format mismatch: old_ubi=$old_is_ubi new_ubi=$new_is_ubi"
        ln -s ${NEW_DIR}/${IMG} ${DIFF_DIR}/${IMG}
        continue
      fi

      if [ $old_is_ubi -eq 1 ]; then
        # UBI image: extract squashfs from UBI volume before diffing
        echo_red "UBI image detected for ${NAME}, extracting squashfs..."
        old_sqfs=$(mktemp --suffix=-old-sqfs -p ${DIR})
        new_sqfs=$(mktemp --suffix=-new-sqfs -p ${DIR})
        extract_squashfs_from_ubi ${OLD_DIR}/${IMG} ${old_sqfs}
        extract_squashfs_from_ubi ${NEW_DIR}/${IMG} ${new_sqfs}
        diff_img $NAME ${old_sqfs} ${new_sqfs} ${DIFF_DIR}/${IMG}
        rm -f ${old_sqfs} ${new_sqfs}
      else
        # Non-UBI: check filesystem type directly
        fs_new=$(get_fs_type ${NEW_DIR}/${IMG})
        fs_old=$(get_fs_type ${OLD_DIR}/${IMG})

        if [ $fs_new != $fs_old ]; then
          echo_red "Copy ${IMG}, fs type are different: $fs_old vs $fs_new"
          ln -s ${NEW_DIR}/${IMG} ${DIFF_DIR}/${IMG}
        elif [ $fs_new != "Squashfs" ]; then
          echo_red "Copy ${IMG}, $fs_new not supported"
          ln -s ${NEW_DIR}/${IMG} ${DIFF_DIR}/${IMG}
        else
          diff_img $NAME ${OLD_DIR}/${IMG} ${NEW_DIR}/${IMG} ${DIFF_DIR}/${IMG}
        fi
      fi
      ;;
    *)
      echo_red "Copy ${IMG}"
      ln -s ${NEW_DIR}/${IMG} ${DIFF_DIR}/${IMG}
      ;;
  esac
done < ${NEW_DIR}/package-file

afptool -pack ${DIFF_DIR} ${DIFF_DIR}/Image/update.img || cleanup "afptool pack: $?"
rkImageMaker $(get_chip_from_loader ${DIFF_DIR}/MiniLoaderAll.bin) \
  ${DIFF_DIR}/Image/MiniLoaderAll.bin \
  ${DIFF_DIR}/Image/update.img \
  ${DIFF_IMG} -os_type:androidos || cleanup "rkImageMaker pack: $?"

#rm -rf ${DIFF_DIR} ${NEW_DIR} ${OLD_DIR}

echo_red "Make diff ${DIFF_IMG} Finished"
