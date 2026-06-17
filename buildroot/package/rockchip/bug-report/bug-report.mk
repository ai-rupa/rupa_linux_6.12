################################################################################
#
# bug-report
#
################################################################################

BUG_REPORT_LICENSE_FILES = NOTICE
BUG_REPORT_LICENSE = Apache V2.0
BUG_REPORT_SITE = $(TOPDIR)/package/rockchip/bug-report
BUG_REPORT_SITE_METHOD = local
BUG_REPORT_LICENSE = Apache V2.0
BUG_REPORT_LICENSE_FILES = NOTICE
CXX="$(TARGET_CXX)"
PROJECT_DIR="$(@D)"
BUG_REPORT_BUILD_OPTS=-I$(PROJECT_DIR) -fPIC \
	--sysroot=$(STAGING_DIR) \
	-ldl -lpthread

BUG_REPORT_MAKE_OPTS = \
        CXXFLAGS="$(TARGET_CPPFLAGS) $(BUG_REPORT_BUILD_OPTS)" \
        PROJECT_DIR="$(@D)"

define BUG_REPORT_BUILD_CMDS
	$(TARGET_MAKE_ENV) $(MAKE) -C $(@D) CXX="$(TARGET_CXX)" $(BUG_REPORT_MAKE_OPTS)
endef

define BUG_REPORT_INSTALL_TARGET_CMDS
	$(INSTALL) -D -m 755 $(@D)/bug-report $(TARGET_DIR)/usr/bin/
endef

$(eval $(generic-package))
