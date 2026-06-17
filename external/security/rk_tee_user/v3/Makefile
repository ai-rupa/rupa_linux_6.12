ifeq ($O,)
out-dir := $(CURDIR)/out
else
include scripts/common.mk
out-dir := $(call strip-trailing-slashes-and-dots,$(O))
ifeq ($(out-dir),)
$(error invalid output directory (O=$(O)))
endif
endif

bindir ?= /bin
libdir ?= /lib

-include $(TA_DEV_KIT_DIR)/host_include/conf.mk
-include $(OPTEE_CLIENT_EXPORT)/include/optee_client_config.mk

ifneq ($V,1)
	q := @
	echo := @echo
else
	q :=
	echo := @:
endif
# export 'q', used by sub-makefiles.
export q

# If _HOST or _TA specific compilers are not specified, then use CROSS_COMPILE
CROSS_COMPILE_HOST ?= $(CROSS_COMPILE)
CROSS_COMPILE_TA ?= $(CROSS_COMPILE)

.PHONY: all
ifneq ($(wildcard $(TA_DEV_KIT_DIR)/host_include/conf.mk),)
ifeq ($(BUILD_CA),y)
all: xtest rk_test ta extra_app test_plugin rk_anti_copy_board
else
all: ta
endif
else
all:
	$(q)echo "TA_DEV_KIT_DIR is not correctly defined" && false
endif

.PHONY: extra_app
extra_app:
	$(q)[ ! -d host/extra_app ] || \
	    $(MAKE) -C host/extra_app CROSS_COMPILE="$(CROSS_COMPILE_HOST)" \
				--no-builtin-variables \
				O=$(out-dir)/extra_app  \
				$@

.PHONY: xtest
xtest:
	$(q)$(MAKE) -C host/xtest CROSS_COMPILE="$(CROSS_COMPILE_HOST)" \
			     --no-builtin-variables \
			     O=$(out-dir) \
			     $@

.PHONY: rk_test
rk_test:
	$(q)$(MAKE) -C host/rk_test CROSS_COMPILE="$(CROSS_COMPILE_HOST)" \
			     --no-builtin-variables \
			     O=$(out-dir)/rk_test \
			     $@

.PHONY: ta
ta:
	$(q)$(MAKE) -C ta CROSS_COMPILE="$(CROSS_COMPILE_TA)" \
			  O=$(out-dir) \
			  $@

.PHONY: test_plugin
test_plugin:
	$(q)$(MAKE) -C host/supp_plugin CROSS_COMPILE="$(CROSS_COMPILE_HOST)" \
			     --no-builtin-variables \
			     O=$(out-dir)

.PHONY: rk_anti_copy_board
rk_anti_copy_board:
	$(q)$(MAKE) -C host/rk_anti_copy_board CROSS_COMPILE="$(CROSS_COMPILE_HOST)" \
				--no-builtin-variables \
				q=$(q) \
				O=$(out-dir)/rk_anti_copy_board  \
				$@

.PHONY: clean
ifneq ($(wildcard $(TA_DEV_KIT_DIR)/host_include/conf.mk),)
clean:
	$(q)$(MAKE) -C host/xtest O=$(out-dir) $@
	$(q)$(MAKE) -C ta O=$(out-dir) $@
	$(q)$(MAKE) -C host/supp_plugin O=$(out-dir) $@
else
clean:
	$(q)echo "TA_DEV_KIT_DIR is not correctly defined"
	$(q)echo "You can remove manually $(out-dir)"
endif

.PHONY: checkpatch checkpatch-staging checkpatch-working
checkpatch: checkpatch-staging checkpatch-working

checkpatch-working:
	@./scripts/checkpatch.sh

checkpatch-staging:
	@./scripts/checkpatch.sh --cached

install:
	$(echo) '  INSTALL ${DESTDIR}${libdir}/optee_armtz'
	$(q)mkdir -p ${DESTDIR}${libdir}/optee_armtz
	$(q)find $(out-dir) -name \*.ta -exec cp {} ${DESTDIR}${libdir}/optee_armtz \;
	$(echo) '  INSTALL ${DESTDIR}${bindir}'
	$(q)mkdir -p ${DESTDIR}${bindir}
	$(q)cp $(out-dir)/xtest/xtest ${DESTDIR}${bindir}
	$(echo) '  INSTALL ${DESTDIR}/$(CFG_TEE_PLUGIN_LOAD_PATH)'
	$(q)mkdir -p ${DESTDIR}/$(CFG_TEE_PLUGIN_LOAD_PATH)
	$(q)cp $(out-dir)/supp_plugin/*.plugin ${DESTDIR}/$(CFG_TEE_PLUGIN_LOAD_PATH)

.PHONY: cscope
cscope:
	$(echo) '  CSCOPE  .'
	${q}rm -f cscope.*
	${q}find $(PWD) -name "*.[ch]" -o -name "*.cpp" | grep -v /package/ > cscope.files
	${q}cscope -b -q -k
