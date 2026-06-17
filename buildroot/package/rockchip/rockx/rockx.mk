################################################################################
#
# rockit project
#
################################################################################

ROCKX_VERSION = 1.2.0
ROCKX_SITE = $(TOPDIR)/../external/rockx
ROCKX_SITE_METHOD = local
ROCKX_INSTALL_STAGING = YES
ROCKX_INSTALL_TARGET = YES
ROCKX_LICENSE = ROCKCHIP
ROCKX_LICENSE_FILES = LICENSE

$(eval $(cmake-package))
