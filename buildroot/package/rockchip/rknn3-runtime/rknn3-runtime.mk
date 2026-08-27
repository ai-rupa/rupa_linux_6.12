################################################################################
#
# rknn3-runtime
#
################################################################################
RKNN3_RUNTIME_VERSION = 1.0.0
RKNN3_RUNTIME_SITE_METHOD = local
RKNN3_RUNTIME_SITE = $(TOPDIR)/../external/rknn3-runtime
RKNN3_RUNTIME_INSTALL_STAGING = YES

RKNN3_RUNTIME_LICENSE = ROCKCHIP
RKNN3_RUNTIME_LICENSE_FILES = LICENSE

RKNN3_RUNTIME_ARCH = $(call qstrip,$(BR2_PACKAGE_RKNN3_RUNTIME_ARCH))

define RKNN3_RUNTIME_INSTALL_TARGET_CMDS
	cp -r $(@D)/rknn3-api/Linux/$(RKNN3_RUNTIME_ARCH)/*.so* \
		$(TARGET_DIR)/usr/lib/
endef

define RKNN3_RUNTIME_INSTALL_STAGING_CMDS
	cp -r $(@D)/rknn3-api/Linux/$(RKNN3_RUNTIME_ARCH)/*.so* \
		$(STAGING_DIR)/usr/lib/
	cp -rT $(@D)/rknn3-api/include \
		$(STAGING_DIR)/usr/include/rknn3
endef

$(eval $(generic-package))
