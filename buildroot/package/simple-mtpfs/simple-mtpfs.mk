################################################################################
#
# simple-mtpfs
#
################################################################################

SIMPLE_MTPFS_VERSION = 0.4.0
SIMPLE_MTPFS_SITE = $(call github,phatina,simple-mtpfs,v$(SIMPLE_MTPFS_VERSION))
SIMPLE_MTPFS_LICENSE = GPL-2.0
SIMPLE_MTPFS_LICENSE_FILES = COPYING
SIMPLE_MTPFS_DEPENDENCIES = libmtp

define SIMPLE_MTPFS_RUN_AUTOGEN
	sed -i '/AX_CXX_COMPILE_STDCXX_17/d' $(@D)/configure.ac
	cd $(@D) && PATH=$(BR_PATH) ./autogen.sh
endef
SIMPLE_MTPFS_PRE_CONFIGURE_HOOKS += SIMPLE_MTPFS_RUN_AUTOGEN

$(eval $(autotools-package))
