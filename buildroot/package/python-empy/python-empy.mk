################################################################################
#
# python-empy
#
################################################################################

ifeq ($(BR2_PACKAGE_PYTHON_EMPY_4_1),y)
PYTHON_EMPY_VERSION = 4.1
else ifeq ($(BR2_PACKAGE_PYTHON_EMPY_3_3_4),y)
PYTHON_EMPY_VERSION = 3.3.4
endif

PYTHON_EMPY_SOURCE = empy-$(PYTHON_EMPY_VERSION).tar.gz
PYTHON_EMPY_SITE = https://files.pythonhosted.org/packages/source/e/empy
PYTHON_EMPY_SETUP_TYPE = setuptools
PYTHON_EMPY_LICENSE = LGPL-2.1+
PYTHON_EMPY_LICENSE_FILES = COPYING

$(eval $(python-package))
$(eval $(host-python-package))
