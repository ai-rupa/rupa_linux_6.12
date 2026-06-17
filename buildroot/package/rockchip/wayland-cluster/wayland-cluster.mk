################################################################################
#
# wayland-cluster
#
################################################################################

WAYLAND_CLUSTER_VERSION = main
WAYLAND_CLUSTER_SITE = $(TOPDIR)/../app/wayland-cluster-app
WAYLAND_CLUSTER_SITE_METHOD = local

WAYLAND_CLUSTER_LICENSE = BSD3

WAYLAND_CLUSTER_DEPENDENCIES += cairo
WAYLAND_CLUSTER_DEPENDENCIES += fontconfig
WAYLAND_CLUSTER_DEPENDENCIES += libegl
WAYLAND_CLUSTER_DEPENDENCIES += stb
WAYLAND_CLUSTER_DEPENDENCIES += wayland
WAYLAND_CLUSTER_DEPENDENCIES += wayland-protocols
WAYLAND_CLUSTER_DEPENDENCIES += rockchip-rga

ifneq ($(BR2_PACKAGE_CLUSTER_ERPC),)
	WAYLAND_CLUSTER_CONF_OPTS += "-DWITH_ERPC=TRUE"
	WAYLAND_CLUSTER_DEPENDENCIES += rockchip-erpc
else
	WAYLAND_CLUSTER_CONF_OPTS += "-DWITH_ERPC=FALSE"
endif

$(eval $(cmake-package))
