################################################################################
#
# Rockchip Recovery For Linux
#
################################################################################

RECOVERY_VERSION = develop
RECOVERY_SITE = $(TOPDIR)/../external/recovery
RECOVERY_SITE_METHOD = local

RECOVERY_LICENSE = ROCKCHIP
RECOVERY_LICENSE_FILES = NOTICE

RECOVERY_DEPENDENCIES += libpthread-stubs util-linux bzip2
ifeq ($(BR2_PACKAGE_RECOVERY_NO_UI),)
RECOVERY_DEPENDENCIES += libpng libdrm libzlib
else ifneq ($(BR2_PACKAGE_RECOVERY_NO_UI),y)
RECOVERY_DEPENDENCIES += libpng libdrm libzlib
endif

ifeq ($(BR2_PACKAGE_RECOVERY_SOURCE_URL),y)
RECOVERY_DEPENDENCIES += libcurl
endif

################################################################################
# New refactored recovery tree
#
# Source marker: app/engine/engine_main.c. Keep the complete new-tree build and
# install policy in this section so the legacy section below can be deleted
# directly after all SDKs switch to the refactored tree.
################################################################################

RECOVERY_NEW_MAKE_ENV = $(TARGET_MAKE_ENV)
RECOVERY_NEW_CFLAGS = $(TARGET_CFLAGS) -fPIC
RECOVERY_NEW_LDFLAGS = $(TARGET_LDFLAGS)
RECOVERY_NEW_EXTRA_LDLIBS =
RECOVERY_NEW_FEATURES = \
	RK_STORAGE_EMMC RK_STORAGE_MTD RK_STORAGE_UFS \
	RK_SOURCE_SD RK_SOURCE_USB \
	RK_DIFF_UPDATE RK_GPT_UPDATE RK_IDBLOCK_UPDATE \
	RK_AB_SUPPORT

ifeq ($(BR2_PACKAGE_RECOVERY_NO_UI),y)
RECOVERY_NEW_MAKE_ENV += RecoveryNoUi=true
else
RECOVERY_NEW_CFLAGS += -I$(STAGING_DIR)/usr/include/libdrm
RECOVERY_NEW_EXTRA_LDLIBS += -lm
endif

ifeq ($(BR2_PACKAGE_RECOVERY_SOURCE_URL),y)
RECOVERY_NEW_FEATURES += RK_SOURCE_URL
endif

ifeq ($(BR2_PACKAGE_RECOVERY_RETRY),y)
RECOVERY_NEW_FEATURES += RETRY_BOOT
else ifeq ($(BR2_PACKAGE_RECOVERY_SUCCESSFUL_BOOT),y)
RECOVERY_NEW_FEATURES += SUCCESSFUL_BOOT
endif

ifeq ($(BR2_PACKAGE_RECOVERY_STATIC),y)
RECOVERY_NEW_LDFLAGS += -static
ifeq ($(BR2_PACKAGE_RECOVERY_SOURCE_URL),y)
RECOVERY_NEW_EXTRA_LDLIBS += `$(PKG_CONFIG_HOST_BINARY) --libs libcurl --static`
endif
endif

################################################################################
# Legacy recovery tree
#
# Keep the old flags and old option semantics unchanged for existing chips.
################################################################################

RECOVERY_OLD_MAKE_ENV = $(TARGET_MAKE_ENV)
RECOVERY_OLD_CFLAGS = $(TARGET_CFLAGS) -I. \
	-fPIC \
	-lpthread \
	-lbz2


ifeq ($(BR2_PACKAGE_RECOVERY_SOURCE_URL),y)
RECOVERY_OLD_CFLAGS += -lcurl
endif

ifeq ($(BR2_PACKAGE_RECOVERY_NO_UI),y)
RECOVERY_OLD_MAKE_ENV += RecoveryNoUi=true
else
RECOVERY_OLD_CFLAGS += -lpng -ldrm -lz -lm -I$(STAGING_DIR)/usr/include/libdrm
endif

ifeq ($(BR2_PACKAGE_RECOVERY_USE_RKUPDATE),y)
RECOVERY_OLD_CFLAGS += -DUSE_RKUPDATE=ON
endif

ifeq ($(BR2_PACKAGE_RECOVERY_USE_UPDATEENGINE),y)
RECOVERY_OLD_CFLAGS += -DUSE_UPDATEENGINE=ON
endif

ifeq ($(BR2_PACKAGE_RECOVERY_SUCCESSFUL_BOOT),y)
RECOVERY_OLD_CFLAGS += -DSUCCESSFUL_BOOT=ON
endif

ifeq ($(BR2_PACKAGE_RECOVERY_RETRY),y)
RECOVERY_OLD_CFLAGS += -DRETRY_BOOT=ON
endif

ifeq ($(BR2_PACKAGE_RECOVERY_STATIC),y)
RECOVERY_OLD_CFLAGS += -static
ifeq ($(BR2_PACKAGE_RECOVERY_SOURCE_URL),y)
RECOVERY_OLD_CFLAGS += `$(PKG_CONFIG_HOST_BINARY) --libs libcurl --static`
endif
endif

################################################################################
# Source-layout dispatch
################################################################################

define RECOVERY_BUILD_CMDS
	if [ -f $(@D)/app/engine/engine_main.c ]; then \
		$(RECOVERY_NEW_MAKE_ENV) $(MAKE) -C $(@D) \
			CC="$(TARGET_CC)" \
			CFLAGS="$(RECOVERY_NEW_CFLAGS)" \
			LDFLAGS="$(RECOVERY_NEW_LDFLAGS)" \
			RECOVERY_FEATURES="$(RECOVERY_NEW_FEATURES)" \
			EXTRA_LDLIBS="$(RECOVERY_NEW_EXTRA_LDLIBS)"; \
	else \
		$(RECOVERY_OLD_MAKE_ENV) $(MAKE) -C $(@D) \
			CC="$(TARGET_CC)" CFLAGS="$(RECOVERY_OLD_CFLAGS)"; \
	fi
endef

define RECOVERY_INSTALL_TARGET_CMDS
	@if [ -f $(@D)/app/engine/engine_main.c ]; then \
		echo "INSTALL /usr/bin/updateEngine"; \
		$(INSTALL) -D -m 755 $(@D)/updateEngine $(TARGET_DIR)/usr/bin/updateEngine; \
		if [ "$(BR2_PACKAGE_RECOVERY_RECOVERYBIN)" = "y" ]; then \
			echo "INSTALL /usr/bin/recovery"; \
			$(INSTALL) -D -m 755 $(@D)/recovery $(TARGET_DIR)/usr/bin/recovery; \
			if [ -d $(@D)/ui/res/images ]; then \
				echo "INSTALL /res/images/"; \
				mkdir -p $(TARGET_DIR)/res/images; \
				cp -a $(@D)/ui/res/images/. $(TARGET_DIR)/res/images/; \
			fi; \
		fi; \
		if [ "$(BR2_PACKAGE_RECOVERY_BOOTCONTROL)" = "y" ]; then \
			echo "INSTALL /etc/init.d/S99_bootcontrol"; \
			$(INSTALL) -D -m 755 $(@D)/scripts/S99_bootcontrol \
				$(TARGET_DIR)/etc/init.d/S99_bootcontrol; \
		fi; \
	else \
		if [ "$(BR2_PACKAGE_RECOVERY_RECOVERYBIN)" = "y" ]; then \
			echo "INSTALL /usr/bin/recovery"; \
			$(INSTALL) -D -m 755 $(@D)/recovery $(TARGET_DIR)/usr/bin/recovery; \
			echo "INSTALL /res/images/"; \
			mkdir -p $(TARGET_DIR)/res/images; \
			cp $(@D)/res/images/* $(TARGET_DIR)/res/images/; \
		fi; \
		if [ "$(BR2_PACKAGE_RECOVERY_UPDATEENGINEBIN)" = "y" ]; then \
			echo "INSTALL /usr/bin/updateEngine"; \
			$(INSTALL) -D -m 755 $(@D)/updateEngine $(TARGET_DIR)/usr/bin/updateEngine; \
		fi; \
		if [ "$(BR2_PACKAGE_RECOVERY_BOOTCONTROL)" = "y" ]; then \
			echo "INSTALL /etc/init.d/S99_bootcontrol"; \
			$(INSTALL) -D -m 755 $(@D)/update_engine/S99_bootcontrol \
				$(TARGET_DIR)/etc/init.d/S99_bootcontrol; \
		fi; \
	fi
endef

ifeq ($(BR2_PACKAGE_RECOVERY_RECOVERYBIN),y)
define RECOVERY_INSTALL_INIT_SYSV
	@echo "INSTALL /etc/init.d/S40recovery"
	@$(INSTALL) -D -m 755 $(RECOVERY_PKGDIR)/S40recovery \
		$(TARGET_DIR)/etc/init.d/S40recovery
endef
endif

$(eval $(generic-package))
