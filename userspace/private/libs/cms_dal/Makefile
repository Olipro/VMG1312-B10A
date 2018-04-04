LIB = libcms_dal.so
default: dynamic


libcms_dal.so:
	cp libcms_dal.so_$(PROFILE)_save libcms_dal.so

dynamic: generic_private_lib_install

clean:
	rm -f libcms_dal.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
