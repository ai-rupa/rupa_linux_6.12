
ifeq ($(MEDIA_PARAM), )
    MEDIA_PARAM:=../Makefile.param
    include $(MEDIA_PARAM)
endif

export LC_ALL=C
SHELL:=/bin/bash

CURRENT_DIR := $(shell pwd)
PKG_TARBALL_CALIB := avs_calib
PKG_TARBALL_LUT := middle_lut
PKG_LIB_INSTALL_PATH := lib
PKG_BIN ?= out

ifeq ($(CONFIG_RK_AVS),y)
PKG_TARGET := avs-build
endif

ifeq ($(PKG_BIN),)
$(error ### $(CURRENT_DIR): PKG_BIN is NULL, Please Check !!!)
endif

all: $(PKG_TARGET)
	@echo "build $(PKG_NAME) done";

avs-build:
	@rm -rf $(PKG_BIN);
	@mkdir -p $(PKG_BIN)/lib;
	@mkdir -p $(PKG_BIN)/include;
	cp -rfa $(PKG_TARBALL_CALIB) $(PKG_BIN)/;
	cp -rfa $(PKG_TARBALL_LUT) $(PKG_BIN)/;
	cp -rfa lib/librkgfx_avs.so  $(PKG_BIN)/lib;
	cp -rfa lib/librkgfx_remap.so  $(PKG_BIN)/lib;
	cp -rfa lib/libpanoStitchApp.so  $(PKG_BIN)/lib;
	cp -rfa lib/*.h  $(PKG_BIN)/include;
	$(call MAROC_COPY_PKG_TO_MEDIA_OUTPUT, $(RK_MEDIA_OUTPUT), $(PKG_BIN))

clean: distclean

distclean:
	-rm -rf $(PKG_BIN)
