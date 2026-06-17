################################################################################
#
# PPSSPP
#
################################################################################
PPSSPP_VERSION = 9912aa5c8d3b95165c56e29ffaa50069aeae0860
PPSSPP_SITE = https://github.com/hrydgard/ppsspp.git
PPSSPP_SITE_METHOD = git
PPSSPP_GIT_SUBMODULES = yes

PPSSPP_DEPENDENCIES += sdl2 sdl2_ttf fontconfig zlib zstd libpng libzip

PPSSPP_CONF_OPTS += \
	-DUSING_X11_VULKAN=OFF -DUSING_X11=OFF -DUSE_FFMPEG=OFF \
	-DARM_NO_VULKAN=ON \
	-DCMAKE_CXX_FLAGS="-fpermissive"

ifeq ($(BR2_PACKAGE_HAS_LIBEGL),y)
	PPSSPP_CONF_OPTS += -DUSING_EGL=ON
	PPSSPP_CONF_OPTS += -DUSING_GLES2=ON
endif

define PPSSPP_INSTALL_TARGET_LIBS
	$(INSTALL) -D $(@D)/lib/*.so* $(TARGET_DIR)/usr/lib/
endef
PPSSPP_POST_INSTALL_TARGET_HOOKS += PPSSPP_INSTALL_TARGET_LIBS

$(eval $(cmake-package))
