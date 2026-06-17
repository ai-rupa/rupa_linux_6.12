################################################################################
#
# rockchip-rkai
#
################################################################################

ROCKCHIP_AI_VERSION = 1.0
ROCKCHIP_AI_SITE = $(TOPDIR)/../app/rkai
ROCKCHIP_AI_SITE_METHOD = local
ROCKCHIP_AI_INSTALL_STAGING = YES
ROCKCHIP_AI_INSTALL_TARGET = YES
ROCKCHIP_AI_DEPENDENCIES = host-pkgconf host-cmake rknn-llm rknpu2 opencv4
ROCKCHIP_AI_LICENSE = ROCKCHIP
ROCKCHIP_AI_LICENSE_FILES = LICENSE

# Debug/Release/RelWithDebInfo
ROCKCHIP_AI_BUILD_TYPE ?= Release

define ROCKCHIP_AI_FIX_OPENCV_INCLUDES
    ln -sf ../opencv4/opencv2 $(STAGING_DIR)/usr/include/opencv2
endef
ROCKCHIP_AI_PRE_CONFIGURE_HOOKS += ROCKCHIP_AI_FIX_OPENCV_INCLUDES

# CMake
define ROCKCHIP_AI_CONFIGURE_CMDS
    mkdir -p $(@D)/build
    cd $(@D)/build && \
    $(TARGET_MAKE_ENV) $(HOST_DIR)/bin/cmake .. \
        -DCMAKE_SYSTEM_PROCESSOR=$(BR2_ARCH) \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_C_COMPILER="$(TARGET_CC)" \
        -DCMAKE_CXX_COMPILER="$(TARGET_CXX)" \
        -DCMAKE_BUILD_TYPE=$(ROCKCHIP_AI_BUILD_TYPE) \
        -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
        -DCMAKE_INSTALL_PREFIX=/usr
endef

define ROCKCHIP_AI_BUILD_CMDS
    cd $(@D)/build && \
    $(TARGET_MAKE_ENV) $(MAKE) -j$(PARALLEL_JOBS)
endef

define ROCKCHIP_AI_INSTALL_TARGET_CMDS
    cd $(@D)/build && \
    $(TARGET_MAKE_ENV) $(MAKE) install DESTDIR=$(TARGET_DIR)
endef

define ROCKCHIP_AI_INSTALL_STAGING_CMDS
    cd $(@D)/build && \
    $(TARGET_MAKE_ENV) $(MAKE) install DESTDIR=$(STAGING_DIR)
endef

ifeq ($(BR2_PACKAGE_RV1126B), y)
    ROCKCHIP_AI_CONFIGURE_CMDS += -DROCKCHIP_AI_CHIP=rv1126b
endif

$(eval $(generic-package))
