################################################################################
#
# rknn-llm
#
################################################################################
RKNN_LLM_VERSION = 1.0.0
RKNN_LLM_SITE_METHOD = local
RKNN_LLM_SITE = $(TOPDIR)/../external/rknn-llm
RKNN_LLM_INSTALL_STAGING = YES

RKNN_LLM_LICENSE = ROCKCHIP
RKNN_LLM_LICENSE_FILES = LICENSE

RKNN_LLM_ARCH = $(call qstrip,$(BR2_PACKAGE_RKNN_LLM_ARCH))

define RKNN_LLM_INSTALL_TARGET_CMDS
	cp -r $(@D)/rkllm-runtime/Linux/librkllm_api/$(RKNN_LLM_ARCH)/* \
		$(TARGET_DIR)/usr/lib/
endef

define RKNN_LLM_INSTALL_STAGING_CMDS
	cp -r $(@D)/rkllm-runtime/Linux/librkllm_api/$(RKNN_LLM_ARCH)/* \
		$(STAGING_DIR)/usr/lib/
	cp -rT $(@D)/rkllm-runtime/Linux/librkllm_api/include \
		$(STAGING_DIR)/usr/include/rkllm
endef

$(eval $(generic-package))
