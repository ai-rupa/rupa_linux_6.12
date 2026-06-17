################################################################################
#
# RKNNSTREAM
#
################################################################################

RKNNSTREAM_VERSION = 1.0
RKNNSTREAM_SITE = $(TOPDIR)/../external/rknnstream
RKNNSTREAM_SITE_METHOD = local
RKNNSTREAM_INSTALL_STAGING = YES
RKNNSTREAM_INSTALL_TARGET = YES
RKNNSTREAM_LICENSE = ROCKCHIP
RKNNSTREAM_LICENSE_FILES = LICENSE

# Debug/Release/RelWithDebInfo
RKNNSTREAM_BUILD_TYPE ?= Release

RKNNSTREAM_DEPENDENCIES += rockchip-osal rockx

$(eval $(cmake-package))
