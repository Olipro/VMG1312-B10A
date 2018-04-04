LIB = libsnoopctl.so
default: dynamic


libsnoopctl.so:
	cp libsnoopctl.so_$(PROFILE)_save libsnoopctl.so

dynamic: generic_private_lib_install

clean:
	rm -f libsnoopctl.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
