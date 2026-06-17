################################################################################
#
# rkscript
#
################################################################################

RKSCRIPT_LICENSE = ROCKCHIP
RKSCRIPT_LICENSE_FILES = LICENSE

RKSCRIPT_ADD_TOOLCHAIN_DEPENDENCY=no

ifeq ($(BR2_PACKAGE_RKSCRIPT_IODOMAIN_NOTICE),y)
define RKSCRIPT_INSTALL_TARGET_IODOMAIN_NOTICE
	$(INSTALL) -m 0755 -D $(RKSCRIPT_PKGDIR)/list-iodomain.sh \
		$(TARGET_DIR)/usr/bin/
endef
RKSCRIPT_POST_INSTALL_TARGET_HOOKS += RKSCRIPT_INSTALL_TARGET_IODOMAIN_NOTICE

define RKSCRIPT_INSTALL_INIT_SYSV
	mkdir -p $(TARGET_DIR)/etc/init.d/
	$(INSTALL) -m 0755 -D $(RKSCRIPT_PKGDIR)/S98iodomain.sh \
		$(TARGET_DIR)/etc/init.d/
endef
endif # IODOMAIN_NOTICE

ifneq ($(BR2_PACKAGE_RKSCRIPT_DEFAULT_PCM),"")
define RKSCRIPT_INSTALL_TARGET_PCM_HOOK
	$(INSTALL) -m 0644 -D $(RKSCRIPT_PKGDIR)/asound.conf.in \
		$(TARGET_DIR)/etc/asound.conf
	$(SED) "s#\#PCM_ID#$(BR2_PACKAGE_RKSCRIPT_DEFAULT_PCM)#g" \
		$(TARGET_DIR)/etc/asound.conf
endef
RKSCRIPT_POST_INSTALL_TARGET_HOOKS += RKSCRIPT_INSTALL_TARGET_PCM_HOOK
endif # PCM

$(eval $(generic-package))
