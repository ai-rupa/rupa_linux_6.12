################################################################################
#
# lz4
#
################################################################################

LZ4_VERSION = 1.10.0
LZ4_SITE = $(call github,lz4,lz4,v$(LZ4_VERSION))
LZ4_INSTALL_STAGING = YES
LZ4_LICENSE = BSD-2-Clause (library), GPL-2.0+ (programs)
LZ4_LICENSE_FILES = lib/LICENSE programs/COPYING
LZ4_CPE_ID_VALID = YES

# The package is a dependency to ccache so ccache cannot be a dependency
HOST_LZ4_ADD_CCACHE_DEPENDENCY = NO

ifeq ($(BR2_STATIC_LIBS),y)
LZ4_MAKE_OPTS += BUILD_SHARED=no
else ifeq ($(BR2_SHARED_LIBS),y)
LZ4_MAKE_OPTS += BUILD_STATIC=no
endif

HOST_LZ4_ENV = $(HOST_MAKE_ENV)

# We are a ccache dependency, so we can't use ccache
HOST_LZ4_ENV += CC="$(HOSTCC_NOCCACHE)" CXX="$(HOSTCXX_NOCCACHE)"

define HOST_LZ4_BUILD_CMDS
	$(HOST_CONFIGURE_OPTS) $(HOST_LZ4_ENV) $(MAKE) -C $(@D) lib
	$(HOST_CONFIGURE_OPTS) $(HOST_LZ4_ENV) $(MAKE) -C $(@D) lz4
endef

define HOST_LZ4_INSTALL_CMDS
	$(HOST_CONFIGURE_OPTS) $(HOST_LZ4_ENV)  $(MAKE) PREFIX=$(HOST_DIR) \
		install -C $(@D)
endef

LZ4_DIRS = lib

ifeq ($(BR2_PACKAGE_LZ4_PROGS),y)
LZ4_DIRS += programs
endif

define LZ4_BUILD_CMDS
	$(foreach dir,$(LZ4_DIRS),\
		$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) $(LZ4_MAKE_OPTS) \
			-C $(@D)/$(dir)
	)
endef

define LZ4_INSTALL_STAGING_CMDS
	$(foreach dir,$(LZ4_DIRS),\
		$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) DESTDIR=$(STAGING_DIR) \
			PREFIX=/usr $(LZ4_MAKE_OPTS) -C $(@D)/$(dir) install
	)
endef

define LZ4_INSTALL_TARGET_CMDS
	$(foreach dir,$(LZ4_DIRS),\
		$(TARGET_MAKE_ENV) $(TARGET_CONFIGURE_OPTS) $(MAKE) DESTDIR=$(TARGET_DIR) \
			PREFIX=/usr $(LZ4_MAKE_OPTS) -C $(@D)/$(dir) install
	)
endef

$(eval $(generic-package))
$(eval $(host-generic-package))
