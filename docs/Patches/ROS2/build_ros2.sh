#!/bin/bash
set -euxo pipefail

# ROS_DISTRO can be set via environment variable or command line argument
# Supported values: humble, jazzy
ROS_DISTRO="${ROS_DISTRO:-humble}"
if [ $# -ge 1 ]; then
    ROS_DISTRO="$1"
fi

# Validate ROS_DISTRO
if [ "$ROS_DISTRO" != "humble" ] && [ "$ROS_DISTRO" != "jazzy" ]; then
    echo "Error: ROS_DISTRO must be 'humble' or 'jazzy'"
    echo "Usage: $0 [humble|jazzy]"
    echo "Or set environment variable: ROS_DISTRO=humble"
    exit 1
fi

echo "Building ROS2 ${ROS_DISTRO}..."

# Set PKG_CONFIG_PATH so pkg_check_modules can find buildroot target pkgconfig insteads of HOST pkgconfig
export PKG_CONFIG_PATH=/buildroot/host/aarch64-buildroot-linux-gnu/sysroot/usr/lib/pkgconfig
# The target include search path is not a standard one, set it explictly
export CMAKE_INCLUDE_PATH='/buildroot/host/aarch64-buildroot-linux-gnu/sysroot/usr/include/;/buildroot/target/opt/ros/lib'

export SYSROOT=/buildroot/host/aarch64-buildroot-linux-gnu/sysroot/
export CC=/buildroot/host/bin/aarch64-buildroot-linux-gnu-gcc
export CXX=/buildroot/host/bin/aarch64-buildroot-linux-gnu-g++
export CROSS_COMPILE=/buildroot/host/bin/aarch64-buildroot-linux-gnu-
export TARGET_ARCH=aarch64
# Consist with buildroot target, not flag:m.
# flag:m(cpython-38m...) is enabled by --with-pymalloc
export PYTHON_SOABI=cpython-312-aarch64-linux-gnu
export ROS2_INSTALL_PATH=/buildroot/host/aarch64-buildroot-linux-gnu/sysroot/opt
export TARGET_TRIPLE=/buildroot/host/bin/aarch64-buildroot-linux-gnu
export ARCH=aarch64

export LD_LIBRARY_PATH=/buildroot/host/lib/

# QEMU_LD_PREFIX for running aarch64 binaries directly (not through CMake)
# Note: CMAKE_SYSROOT and CMAKE_CROSSCOMPILING_EMULATOR are set in cross-compile.mixin
export QEMU_LD_PREFIX=/buildroot/target

cleanup() {
  TARGET_PATH=$(realpath ${ROS2_INSTALL_PATH})

  find ${TARGET_PATH} -type f \( -executable -o -name "*.so*" \) -exec sh -c '
      for f; do 
        if file -b "$f" | grep -q "not stripped"; then
          ${CROSS_COMPILE}strip "$f";
        fi;
      done ' _ {} \;
}

trap 'cleanup' EXIT

# Set COLCON_IGNORE for packages that require UI (not supported in Buildroot)
touch src/ros-visualization/COLCON_IGNORE || true
touch src/ros2/rviz/COLCON_IGNORE || true
touch src/ros/ros_tutorials/turtlesim/COLCON_IGNORE || true

mkdir -p /buildroot/build/ros /buildroot/target/opt/ros

# cmake's find_package only search sysroot path if CMAKE_SYSROOT has defined
# setting COLCON_CURRENT_PREFIX instead?
pushd /buildroot/host/aarch64-buildroot-linux-gnu/sysroot/
rm -rf opt
ln -sf ../../../target/opt/ros/ opt
popd

# install mixin which setup building evinronments
colcon mixin remove default || true
colcon mixin add default file:///opt/ros/cross-compile/index.yaml
colcon mixin update default

# HINTS:
#  - Use --merge-install to avoid some find_package/find_path/find_library fails
colcon build \
  --mixin aarch64-linux \
  --merge-install \
  --build-base /buildroot/build/ros/ \
  --install-base /buildroot/host/aarch64-buildroot-linux-gnu/sysroot/opt \
  --cmake-force-configure \
#  --packages-select urdf \
#  --cmake-args "--debug-find" \
#  --packages-up-to rosbag2_converter_default_plugins
#  --packages-select rcutils