LIB = libethswctl.so
default: dynamic


libethswctl.so:
	cp libethswctl.so_$(PROFILE)_save libethswctl.so

dynamic: generic_private_lib_install

clean:
	rm -f libethswctl.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
