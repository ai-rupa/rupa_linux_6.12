################################################################################
#
# catkin_pkg
#
################################################################################

CATKIN_PKG_VERSION = 1.0.0
CATKIN_PKG_SITE = $(call github,ros-infrastructure,catkin_pkg,$(CATKIN_PKG_VERSION))
CATKIN_PKG_SETUP_TYPE = setuptools
CATKIN_PKG_LICENSE = BSD-3-Clause
CATKIN_PKG_LICENSE_FILES = LICENSE

CATKIN_PKG_DEPENDENCIES = python3

HOST_CATKIN_PKG_DEPENDENCIES = host-python3

$(eval $(python-package))
$(eval $(host-python-package))
