################################################################################
#
# rockit project
#
################################################################################

IPC_DRV_KO_SITE = $(TOPDIR)/../external/ipc_drv_ko

IPC_DRV_KO_SITE_METHOD = local

IPC_DRV_KO_INSTALL_STAGING = YES

IPC_DRV_KO_CONF_OPTS += -DFOR_BUILDROOT=TRUE

ifeq ($(call qstrip,$(BR2_ARCH)), arm)
IPC_DRV_KO_ARCH = arm
else ifeq ($(call qstrip, $(BR2_ARCH)), aarch64)
IPC_DRV_KO_ARCH = arm64
endif

IPC_DRV_KO_CONF_OPTS += -DRK_APP_ARCH_TYPE=$(IPC_DRV_KO_ARCH)

$(eval $(cmake-package))
