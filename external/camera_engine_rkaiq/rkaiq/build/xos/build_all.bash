#!/bin/bash

socs=(
  'rk356x'
  'rk3588'
  'rk3562'
  'rv1106'
  'rv1103b'
  'rk3576'
  'rv1126b'
  'rk3572'
)

isps=(
  '351s'
  '35'
  '39'
  '33'
  '32_LITE'
  '32'
  '30'
  '21'
)

archs=(
  # 'aarch64'
  'arm'
)

libcs=(
  'clang'
)

for soc in ${socs[@]}; do
  for isp in ${isps[@]}; do
    for arch in ${archs[@]}; do
      for libc in ${libcs[@]}; do
        export TARGET_SOC=$soc
        export TARGET_ARCH=$arch 
        export TARGET_LIBC=$libc
        export TARGET_ISP_VERSION=$isp
        export WORKSPACE=$(pwd)
        combined=$TARGET_SOC/$TARGET_ISP_VERSION/$TARGET_ARCH/$TARGET_LIBC
        echo $combined | grep -q "aarch64/uclibc"
        if [ $? -eq 0 ]; then
          echo ">> $combined"
          continue
        fi
        echo $combined | grep -q "rv1106/32/"
        if [ $? -eq 0 ]; then
          echo ">> $combined"
          /bin/bash ./build.sh
          if [ $? = 1 ];then
            exit 1
          fi
          continue
        fi
        echo $combined | grep -q "rk3588/30"
        if [ $? -eq 0 ]; then
          echo ">> $combined"
          /bin/bash ./build.sh
          if [ $? = 1 ];then
            exit 1
          fi
          continue
        fi
        echo $combined | grep -q "rk356x/21"
        if [ $? -eq 0 ]; then
          echo ">> $combined"
          /bin/bash ./build.sh
          if [ $? = 1 ];then
            exit 1
          fi
          continue
        fi
        echo $combined | grep -q "rk3562/32_LIT"
        if [ $? -eq 0 ]; then
          echo ">> $combined"
          /bin/bash ./build.sh
          if [ $? = 1 ];then
            exit 1
          fi
          continue
        fi
        echo $combined | grep -q "rk3576/39"
        if [ $? -eq 0 ]; then
          echo ">> $combined"
          /bin/bash ./build.sh
          if [ $? = 1 ];then
            exit 1
          fi
          continue
        fi
        echo $combined | grep -q "rv1126b/35/"
        if [ $? -eq 0 ]; then
          echo ">> $combined"
          /bin/bash ./build.sh
          if [ $? = 1 ];then
            exit 1
          fi
          continue
        fi
        echo $combined | grep -q "rk3572/351s"
        if [ $? -eq 0 ]; then
          echo ">> $combined"
          /bin/bash ./build.sh
          if [ $? = 1 ];then
            exit 1
          fi
          continue
        fi
        continue
      done
    done
  done
done
