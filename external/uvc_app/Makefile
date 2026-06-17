
ifeq ($(APP_PARAM), )
    APP_PARAM:=../Makefile.param
    include $(APP_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)

PKG_NAME := src
PKG_BIN ?= out
PKG_BUILD ?= build

# debug: build cmake with more message
ifneq ($(V),)
PKG_CONF_OPTS += -DCMAKE_VERBOSE_MAKEFILE=ON
endif
#
RK_UVC_APP_CONFIG :=

# RTT fastboot mode use RK_APP_TYPE to enable
# ifeq ($(RK_ENABLE_FASTBOOT),y)
# PKG_CONF_OPTS += -DRK_UVC_ENABLE_FASTBOOT=ON
# RK_APP_OPTS += -DRK_UVC_ENABLE_FASTBOOT
#endif

# PKG_CONF_OPTS += -DRK_UVC_ENABLE_ASD=ON
# PKG_CONF_OPTS += -DRK_UVC_ENABLE_EPTZ=ON
# PKG_CONF_OPTS += -DRK_UVC_ENABLE_IVA=ON
# PKG_CONF_OPTS += -DRK_UVC_ENABLE_NN=ON
# PKG_CONF_OPTS += -DRK_UVC_ENABLE_FACE_NN=ON

ifneq ($(filter $(RK_APP_CHIP), rv1106 rv1103b rv1126b rk3572),)
ifneq (, $(filter UVC_TINY, $(RK_APP_TYPE)))
	RK_UVC_APP_CONFIG := -DCOMPILE_FOR_MPI=ON
else ifneq (, $(filter UVC_TINY_RV1126B_4K, $(RK_APP_TYPE)))
	RK_UVC_APP_CONFIG := -DCOMPILE_FOR_MPI=ON -DCOMPILE_FOR_UVC_TINY_RV1126B_4K=ON
else ifneq (, $(filter UVC_TINY_RV1126B_WINHELLO, $(RK_APP_TYPE)))
	RK_UVC_APP_CONFIG := -DCOMPILE_FOR_MPI=ON -DCOMPILE_FOR_UVC_TINY_RV1126B_WINHELLO=ON
else ifneq (, $(filter UVC_TINY_FASTBOOT_RTT, $(RK_APP_TYPE)))
	RK_UVC_APP_CONFIG := -DCOMPILE_FOR_MPI=ON
	PKG_CONF_OPTS += -DRK_UVC_ENABLE_FASTBOOT=ON
	RK_APP_OPTS += -DRK_UVC_ENABLE_FASTBOOT
else
$(info ### disable make uvc demo)
endif
else
$(info ### unknow RK_APP_CHIP)
endif

PKG_CONF_OPTS += -DRK_APP_CHIP=$(RK_APP_CHIP)

RK_APP_CFLAGS = -I $(RK_APP_MEDIA_INCLUDE_PATH) \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/libdrm \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/uAPI \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/uAPI2 \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/algos \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/common \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/xcore \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/iq_parser \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/rkaiq/iq_parser_v2 \
				-I $(RK_APP_MEDIA_INCLUDE_PATH)/easymedia
RK_APP_LDFLAGS = -L $(RK_APP_MEDIA_LIBS_PATH)

RK_APP_OPTS += -Wl,-rpath-link,$(RK_APP_MEDIA_LIBS_PATH):$(RK_APP_PATH_LIB_INCLUDE)/root/usr/lib
PKG_CONF_OPTS += -DCMAKE_C_FLAGS="$(RK_APP_CFLAGS) $(RK_APP_LDFLAGS) $(RK_APP_OPTS)" \
				-DCMAKE_CXX_FLAGS="$(RK_APP_CFLAGS) $(RK_APP_LDFLAGS) $(RK_APP_OPTS)"

PKG_CONF_OPTS += -DUVC_APP_CROSS_COMPILE="$(RK_APP_CROSS)"

ifneq ($(RK_APP_CMAKE_FLAGS),)
PKG_CONF_OPTS += $(RK_APP_CMAKE_FLAGS)
endif

# define project/cfg/BoardConfig*.mk
ifneq ($(RK_UVC_APP_CONFIG),)
PKG_TARGET := uvc_app-build
$(info ** $(PKG_NAME) build $(RK_APP_TYPE) **)
endif

ifeq ($(PKG_BIN),)
$(error ### $(CURRENT_DIR): PKG_BIN is NULL, Please Check !!!)
endif

all: $(PKG_TARGET)
	@echo "build $(PKG_NAME) done"

uvc_app-build:
	@echo "RK_APP_CHIP is $(RK_APP_CHIP)"
	@echo "RK_APP_TYPE is $(RK_APP_TYPE)"
	rm -rf $(PKG_BIN) $(PKG_BUILD); \
	mkdir -p $(PKG_BIN);
	mkdir -p $(PKG_BUILD);
	pushd $(PKG_BUILD)/; \
		rm -rf CMakeCache.txt; \
		cmake $(CURRENT_DIR)/$(PKG_NAME)/ \
			-DCMAKE_C_COMPILER=$(RK_APP_CROSS)-gcc \
			-DCMAKE_CXX_COMPILER=$(RK_APP_CROSS)-g++ \
			-DCMAKE_INSTALL_PREFIX="$(CURRENT_DIR)/$(PKG_BIN)" \
			-DCMAKE_BUILD_TYPE=Release \
			-DCMAKE_COLOR_MAKEFILE=OFF \
			-DCMAKE_SYSTEM_NAME=Linux \
			$(RK_UVC_APP_CONFIG) \
			$(PKG_CONF_OPTS) ;\
			make -j$(RK_APP_JOBS) || exit -1; \
			make install; \
	popd;
	$(call MAROC_COPY_PKG_TO_APP_OUTPUT, $(RK_APP_OUTPUT), $(PKG_BIN))

clean:
	@rm -rf $(PKG_BIN) $(PKG_BUILD)

distclean: clean

info:
ifneq ($(RK_UVC_APP_CONFIG),)
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
	@echo -e "RK_APP_TYPE=$(RK_APP_TYPE)"
	@echo -e "option support as follow:"
	@echo -e "	UVC"
	@echo -e "$(C_YELLOW)-------------------------------------------------------------------------$(C_NORMAL)"
endif
