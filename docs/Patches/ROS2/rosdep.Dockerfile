# This Dockerfile bases on the source code from
#   https://github.com/ros2/cross_compile.git
# Refer to https://docs.ros.org/en/foxy/How-To-Guides/Cross-compilation.html
# for more details about cross building ROS2.
#
# This docker image is for building ROS2 Humble/Jazzy with Rockchip Linux SDK.
#
# Build command:
#   docker build --build-arg ROS_DISTRO=humble \
#       --build-arg HOST_USER=$(whoami) --build-arg HOST_UID=$(id -u) --build-arg HOST_GID=$(id -g) \
#       -t humble-ros2-builder -f rosdep.Dockerfile ./
#   docker build --build-arg ROS_DISTRO=jazzy \
#       --build-arg HOST_USER=$(whoami) --build-arg HOST_UID=$(id -u) --build-arg HOST_GID=$(id -g) \
#       -t jazzy-ros2-builder -f rosdep.Dockerfile ./

FROM ubuntu:noble

ARG ROS_DISTRO=humble
ARG HOST_USER=builder
ARG HOST_UID=1000
ARG HOST_GID=1000

SHELL ["/bin/bash", "-c"]

# Set timezone before install tzdata
RUN echo 'Etc/UTC' > /etc/timezone && \
    ln -sf /usr/share/zoneinfo/Etc/UTC /etc/localtime

RUN apt-get update && apt-get install --no-install-recommends -y \
     tzdata \
     locales \
     vim \
     file \
     dirmngr \
     gnupg2 \
     ca-certificates \
     curl wget git less \
     lsb-release

# Set locale
RUN echo 'en_US.UTF-8 UTF-8' >> /etc/locale.gen && \
    locale-gen && \
    update-locale LC_ALL=en_US.UTF-8 LANG=en_US.UTF-8
ENV LANG en_US.UTF-8
ENV LC_ALL C.UTF-8

RUN curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key -o /usr/share/keyrings/ros-archive-keyring.gpg; \
    echo "deb [arch=amd64 signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] http://packages.ros.org/ros2/ubuntu noble main" | tee /etc/apt/sources.list.d/ros2.list > /dev/null

RUN apt-get update && apt-get install --no-install-recommends -y \
    sudo \
    qemu-user-static \
    build-essential \
    iputils-ping \
    pkgconf \
    libasio-dev \
    libtinyxml2-dev \
    cmake libpcre2-dev \
    python3-dev \
    python3-pip \
    python3-numpy \
    python3-pytest-cov \
    python3-flake8 \
    python3-flake8-blind-except \
    python3-flake8-builtins \
    python3-flake8-class-newline \
    python3-flake8-comprehensions \
    python3-flake8-deprecated \
    python3-flake8-docstrings \
    python3-flake8-import-order \
    python3-flake8-quotes \
    python3-pytest-repeat \
    python3-pytest-rerunfailures \
    python3-pytest \
    python3-pytest-cov \
    python3-pytest-runner \
    python3-colcon-common-extensions \
    python3-colcon-mixin \
    python3-vcstool \
    python3-catkin-pkg \
    python3-rosdep

RUN if id ubuntu > /dev/null 2>&1; then \
        echo "Removing ubuntu user"; \
        userdel -r ubuntu || true; \
    fi; \
    if getent group ubuntu > /dev/null 2>&1; then \
        echo "Removing ubuntu group"; \
        groupdel ubuntu || true; \
    fi

RUN groupadd -g ${HOST_GID} ${HOST_USER} && \
    useradd -d /home/${HOST_USER} -s /bin/bash -u ${HOST_UID} -g ${HOST_GID} -m ${HOST_USER} && \
    echo "${HOST_USER}:rockchip" | chpasswd && \
    usermod -aG sudo ${HOST_USER}

SHELL ["/bin/bash", "-c"]

# mixins defines building environments
RUN mkdir -p /buildroot/ /opt/ros/${ROS_DISTRO}

#FIXME: link buildroot binfmt just because CROSSCOMPILING_EMULATOR don't work
#       when building foonathan_memory_vendor, and installing qemu-user-static/qemu-user-binfmt
#       packages require root privilege. The CMAKE_CROSSCOMPILING_EMULATOR doesn't work too.
#       "-DCMAKE_CROSSCOMPILING_EMULATOR=/usr/bin/qemu-aarch64-static;-L;/buildroot/target/"
RUN mkdir -p /etc/qemu-binfmt
RUN ln -sf /buildroot/target/ /etc/qemu-binfmt/aarch64

COPY ./src /opt/ros/${ROS_DISTRO}/src
COPY ./build_ros2.sh /opt/ros/${ROS_DISTRO}/build_ros2.sh
COPY ./cross-compile /opt/ros/cross-compile

RUN chown -R ${HOST_USER}:root /buildroot /opt /opt/ros/

RUN git config --global user.email "cross-build@for-rk-linux-sdk.com" && \
    git config --global user.name "cross-build for-rk-linux-sdk.com"

USER ${HOST_USER}

WORKDIR /opt/ros/${ROS_DISTRO}

ENTRYPOINT ["/bin/bash"]