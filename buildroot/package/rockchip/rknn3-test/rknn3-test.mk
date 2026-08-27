################################################################################
#
# rknn3-test
#
################################################################################
RKNN3_TEST_VERSION = 1.0.0
RKNN3_TEST_SITE_METHOD = local
RKNN3_TEST_SITE = $(TOPDIR)/../external/rknn3-test
RKNN3_TEST_LICENSE = ROCKCHIP

RKNN3_TEST_TARGET_SOC = $(call qstrip,$(BR2_PACKAGE_RKNN3_TEST_TARGET_SOC))

# Determine target architecture
ifeq ($(BR2_aarch64),y)
RKNN3_TEST_TARGET_ARCH = aarch64
else ifeq ($(BR2_arm),y)
RKNN3_TEST_TARGET_ARCH = armhf
else
RKNN3_TEST_TARGET_ARCH = x86_64
endif

# Set cross compiler prefix (remove trailing - as build-linux.sh will add it)
RKNN3_TEST_GCC_COMPILER = $(patsubst %-,%, $(TARGET_CROSS))

# Collect all enabled demos
RKNN3_TEST_DEMOS =
ifeq ($(BR2_PACKAGE_RKNN3_TEST_CNN),y)
RKNN3_TEST_DEMOS += rknn3_cnn_test
endif
ifeq ($(BR2_PACKAGE_RKNN3_TEST_LLM),y)
RKNN3_TEST_DEMOS += rknn3_llm_test
endif
ifeq ($(BR2_PACKAGE_RKNN3_TEST_VLM),y)
RKNN3_TEST_DEMOS += rknn3_vlm_test
endif
ifeq ($(BR2_PACKAGE_RKNN3_TEST_AUDIO_ENCODER),y)
RKNN3_TEST_DEMOS += rknn3_audio_encoder_test
endif

define RKNN3_TEST_CONFIGURE_CMDS
endef

define RKNN3_TEST_BUILD_CMDS
	# Create symlinks for dependencies at the buildroot output level
	mkdir -p $(BASE_DIR)/rknn
	ln -sf $(TOPDIR)/../external/rknn3_model_zoo $(BASE_DIR)/rknn/rknn3-model-zoo
	$(foreach demo,$(RKNN3_TEST_DEMOS), \
		cd $(@D) && \
		GCC_COMPILER=$(RKNN3_TEST_GCC_COMPILER) \
		./build-linux.sh \
			-t $(RKNN3_TEST_TARGET_SOC) \
			-a $(RKNN3_TEST_TARGET_ARCH) \
			-d $(demo) \
			-s;
	)
endef

define RKNN3_TEST_INSTALL_TARGET_CMDS
	mkdir -p $(TARGET_DIR)/usr/share/rknn3-test/
	$(foreach demo,$(RKNN3_TEST_DEMOS), \
		INSTALL_DIR=$(@D)/install/$(RKNN3_TEST_TARGET_SOC)_linux_$(RKNN3_TEST_TARGET_ARCH)/rknn_$(demo)_demo; \
		if [ -d "$$INSTALL_DIR" ]; then \
			cp -r $$INSTALL_DIR/* $(TARGET_DIR)/usr/share/rknn3-test/; \
		fi;
	)
	chmod +x $(TARGET_DIR)/usr/share/rknn3-test/rknn3_*_demo 2>/dev/null || true
	# Install demo binaries to /usr/bin
	$(foreach demo,$(RKNN3_TEST_DEMOS), \
		INSTALL_DIR=$(@D)/install/$(RKNN3_TEST_TARGET_SOC)_linux_$(RKNN3_TEST_TARGET_ARCH)/rknn_$(demo)_demo; \
		if [ -d "$$INSTALL_DIR" ]; then \
			for bin in $$INSTALL_DIR/rknn3_*_demo; do \
				if [ -f "$$bin" ]; then \
					cp -f $$bin $(TARGET_DIR)/usr/bin/; \
					chmod +x $(TARGET_DIR)/usr/bin/$$(basename $$bin); \
				fi; \
			done; \
		fi;
	)
endef

$(eval $(generic-package))
