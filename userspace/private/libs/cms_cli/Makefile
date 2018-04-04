LIB = libcms_cli.so
default: dynamic


libcms_cli.so:
	cp libcms_cli.so_$(PROFILE)_save libcms_cli.so

dynamic: generic_private_lib_install

clean:
	rm -f libcms_cli.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
