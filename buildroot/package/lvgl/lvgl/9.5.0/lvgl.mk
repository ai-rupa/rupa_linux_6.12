################################################################################
#
# LVGL - Light and Versatile Graphics Library
#
################################################################################

LVGL_VERSION = 9.5.0
LVGL_SITE = $(call github,lvgl,lvgl,v$(LVGL_VERSION))
LVGL_INSTALL_STAGING = YES

LVGL_CONF_OPTS += -DCONFIG_LV_USE_PRIVATE_API=ON

ifeq ($(BR2_PACKAGE_LVGL_DISABLE_THORVG), y)
LVGL_CONF_OPTS += -DCONFIG_LV_USE_THORVG_INTERNAL=OFF
endif

ifeq ($(BR2_PACKAGE_LVGL_DISABLE_EXAMPLES), y)
LVGL_CONF_OPTS += -DCONFIG_LV_BUILD_EXAMPLES=OFF
endif

ifeq ($(BR2_LV_USE_FREETYPE), y)
LVGL_DEPENDENCIES += freetype
LVGL_CONF_OPTS += -DLV_USE_FREETYPE=y
endif

ifeq ($(BR2_LV_USE_SDL), y)
LVGL_DEPENDENCIES += sdl2
endif

ifeq ($(BR2_LV_USE_LINUX_DRM), y)
LVGL_DEPENDENCIES += libdrm
ifeq ($(BR2_LV_DRM_USE_RGA), y)
LVGL_DEPENDENCIES += rockchip-rga
endif
endif

ifeq ($(BR2_LV_USE_DRAW_SDL), y)
LVGL_DEPENDENCIES += sdl2 sdl2_image
endif

define LVGL_PRE_HOOK
	cat $(BR2_CONFIG) | grep "LVGL\|LV_" | grep -v "#" > $(LVGL_DIR)/lv_conf.mk
	$(SED) "s/^/set\(/;s/=y/=1/;s/=/ /;s/$$/)/;s/\"/\"\\\\\"/;s/\")/\\\\\"\")/" $(LVGL_DIR)/lv_conf.mk
endef
LVGL_PRE_CONFIGURE_HOOKS += LVGL_PRE_HOOK

$(eval $(cmake-package))
