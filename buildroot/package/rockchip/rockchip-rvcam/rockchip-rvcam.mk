################################################################################
#
# rockchip-rvcam project
#
################################################################################

ROCKCHIP_RVCAM_VERSION = main
ROCKCHIP_RVCAM_SITE = $(TOPDIR)/../external/rvcam
ROCKCHIP_RVCAM_SITE_METHOD = local
ROCKCHIP_RVCAM_LICENSE = proprietary, GPL-2.0, Apache-2.0
ROCKCHIP_RVCAM_LICENSE_FILES = licenses/LICENSE licenses/GPL-2.0 licenses/Apache-2.0
ROCKCHIP_RVCAM_INSTALL_STAGING = YES

ROCKCHIP_RVCAM_DEPENDENCIES += tinyxml2
ROCKCHIP_RVCAM_DEPENDENCIES += rockchip-erpc

ifeq ($(BR2_PACKAGE_RK3588), y)
ROCKCHIP_RVCAM_CONF_OPTS += -DRVCAM_TARGET_SOC=rk3588
else ifeq ($(BR2_PACKAGE_RK3576), y)
ROCKCHIP_RVCAM_CONF_OPTS += -DRVCAM_TARGET_SOC=rk3576
else ifeq ($(BR2_PACKAGE_RK3568), y)
ROCKCHIP_RVCAM_CONF_OPTS += -DRVCAM_TARGET_SOC=rk3568
else ifeq ($(BR2_PACKAGE_RK3358), y)
ROCKCHIP_RVCAM_CONF_OPTS += -DRVCAM_TARGET_SOC=rk3358
endif

$(eval $(cmake-package))
