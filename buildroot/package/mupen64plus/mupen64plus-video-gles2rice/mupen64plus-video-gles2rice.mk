################################################################################
#
# mupen64plus-video-gles2rice
#
################################################################################
MUPEN64PLUS_VIDEO_GLES2RICE_VERSION = 1dcd6ddb6c60750c9fe4dd6dab6a2d04c304221e
MUPEN64PLUS_VIDEO_GLES2RICE_SITE = $(call github,ricrpi,mupen64plus-video-gles2rice,$(MUPEN64PLUS_VIDEO_GLES2RICE_VERSION))

MUPEN64PLUS_VIDEO_GLES2RICE_DEPENDENCIES += \
	mupen64plus-core \
	host-pkgconf \
	sdl2 \
	libpng

MUPEN64PLUS_VIDEO_GLES2RICE_CONF += UNAME=linux

ifeq ($(BR2_arm),y)
MUPEN64PLUS_VIDEO_GLES2RICE_CONF += HOST_CPU=arm
else ifeq ($(BR2_aarch64),y)
MUPEN64PLUS_VIDEO_GLES2RICE_CONF += HOST_CPU=arm64
endif

ifeq ($(BR2_PACKAGE_HAS_LIBGLES)$(BR2_PACKAGE_SDL2_OPENGLES),yy)
MUPEN64PLUS_VIDEO_GLES2RICE_DEPENDENCIES += libgles
MUPEN64PLUS_VIDEO_GLES2RICE_CONF += USE_GLES=1
endif

define MUPEN64PLUS_VIDEO_GLES2RICE_BUILD_CMDS
	CFLAGS="$(TARGET_CFLAGS)" CXXFLAGS="$(TARGET_CXXFLAGS)" \
		LDFLAGS="$(TARGET_LDFLAGS)" \
		PKG_CONFIG="$(PKG_CONFIG_HOST_BINARY)" \
		PREFIX=/usr/ APIDIR="$(STAGING_DIR)/usr/include/mupen64plus" \
		$(MAKE) -C $(@D)/projects/unix all \
		CC="$(TARGET_CC)" CXX="$(TARGET_CXX)" LD="$(TARGET_CC)" \
		RANLIB="$(TARGET_RANLIB)" AR="$(TARGET_AR)" \
		SDL_CONFIG="$(STAGING_DIR)/usr/bin/sdl2-config" \
		OPTFLAGS="-flto -fpermissive" \
		$(MUPEN64PLUS_VIDEO_GLES2RICE_CONF)
endef

define MUPEN64PLUS_VIDEO_GLES2RICE_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/usr/lib/mupen64plus
	$(INSTALL) -m 0644 $(@D)/projects/unix/mupen64plus-video-rice.so \
		$(TARGET_DIR)/usr/lib/mupen64plus

	mkdir -p $(TARGET_DIR)/usr/share/mupen64plus
	$(INSTALL) -m 0644 $(@D)/data/RiceVideoLinux.ini \
		$(TARGET_DIR)/usr/share/mupen64plus
endef

$(eval $(generic-package))
