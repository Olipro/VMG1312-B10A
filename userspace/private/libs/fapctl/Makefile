LIB = libfapctl.so
default: dynamic


libfapctl.so:
	cp libfapctl.so_$(PROFILE)_save libfapctl.so

dynamic: generic_private_lib_install

clean:
	rm -f libfapctl.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
