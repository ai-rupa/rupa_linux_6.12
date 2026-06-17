################################################################################
#
# libunwind
#
################################################################################

LIBUNWIND_VERSION = 1.8.1
LIBUNWIND_SITE = https://github.com/libunwind/libunwind/releases/download/v$(LIBUNWIND_VERSION)
LIBUNWIND_INSTALL_STAGING = YES
LIBUNWIND_LICENSE_FILES = COPYING
LIBUNWIND_LICENSE = MIT
LIBUNWIND_CPE_ID_VALID = YES

ifeq ($(BR2_PACKAGE_XZ),y)
LIBUNWIND_DEPENDENCIES += xz
endif

LIBUNWIND_CONF_OPTS = \
	--disable-tests \
	$(if $(BR2_INSTALL_LIBSTDCPP),--enable-cxx-exceptions,--disable-cxx-exceptions)

ifeq ($(BR2_PACKAGE_LIBUCONTEXT),y)
LIBUNWIND_DEPENDENCIES += libucontext
LIBUNWIND_CONF_OPTS += LIBS=-lucontext
endif

ifeq ($(BR2_PACKAGE_LIBUNWIND_STATIC),y)
LIBUNWIND_CONF_OPTS += --disable-zlibdebuginfo
endif

$(eval $(autotools-package))
