################################################################################
#
# libmtp
#
################################################################################

LIBMTP_VERSION = 1.1.22
LIBMTP_SITE = $(call github,libmtp,libmtp,v$(LIBMTP_VERSION))
LIBMTP_LICENSE = LGPL-2.1
LIBMTP_LICENSE_FILES = COPYING
LIBMTP_INSTALL_STAGING = YES
LIBMTP_DEPENDENCIES = libusb

define LIBMTP_RUN_AUTOGEN
	cd $(@D) && PATH=$(BR_PATH) yes n | ./autogen.sh
endef
LIBMTP_PRE_CONFIGURE_HOOKS += LIBMTP_RUN_AUTOGEN

$(eval $(autotools-package))
