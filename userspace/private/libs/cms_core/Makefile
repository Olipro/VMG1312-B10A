LIB = libcms_core.so
default: dynamic


libcms_core.so:
	cp libcms_core.so_$(PROFILE)_save libcms_core.so

dynamic: generic_private_lib_install

clean:
	rm -f libcms_core.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
