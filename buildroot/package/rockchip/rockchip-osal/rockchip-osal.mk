################################################################################
#
# rockchip-osal
#
################################################################################

ROCKCHIP_OSAL_VERSION = 1.0
ROCKCHIP_OSAL_SITE = $(TOPDIR)/../external/rockchip-osal
ROCKCHIP_OSAL_SITE_METHOD = local
ROCKCHIP_OSAL_INSTALL_STAGING = YES
ROCKCHIP_OSAL_INSTALL_TARGET = YES
ROCKCHIP_OSAL_LICENSE = ROCKCHIP
ROCKCHIP_OSAL_LICENSE_FILES = LICENSE

# Debug/Release/RelWithDebInfo
ROCKCHIP_OSAL_BUILD_TYPE ?= Release

define ROCKCHIP_OSAL_BUILD_CMDS
    cd $(@D)/ && \
    $(MAKE) -j$(PARALLEL_JOBS) CC=$(TARGET_CROSS)gcc EXT_CFLAGS="$(TARGET_CFLAGS)"
endef

define ROCKCHIP_OSAL_INSTALL_TARGET_CMDS
    cd $(@D)/ && \
    $(MAKE) install DESTDIR=$(TARGET_DIR)
endef

define ROCKCHIP_OSAL_INSTALL_STAGING_CMDS
    cd $(@D)/ && \
    $(MAKE) install DESTDIR=$(STAGING_DIR)
endef

$(eval $(generic-package))
