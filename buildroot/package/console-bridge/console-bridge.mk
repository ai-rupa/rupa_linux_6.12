################################################################################
#
# console_bridge
#
################################################################################

CONSOLE_BRIDGE_VERSION = 1.0.1
CONSOLE_BRIDGE_SITE = $(call github,ros,console_bridge,$(CONSOLE_BRIDGE_VERSION))
CONSOLE_BRIDGE_LICENSE = BSD-3-Clause
CONSOLE_BRIDGE_LICENSE_FILES = LICENSE
CONSOLE_BRIDGE_INSTALL_STAGING = YES

# Use CMake package
CONSOLE_BRIDGE_CONF_OPTS = \
	-DBUILD_SHARED_LIBS=$(if $(BR2_STATIC_LIBS),OFF,ON)

$(eval $(cmake-package))
