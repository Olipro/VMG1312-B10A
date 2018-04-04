LIB = libiqctl.so
default: dynamic


libiqctl.so:
	cp libiqctl.so_$(PROFILE)_save libiqctl.so

dynamic: generic_private_lib_install

clean:
	rm -f libiqctl.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
