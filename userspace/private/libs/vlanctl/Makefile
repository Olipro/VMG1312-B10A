LIB = libvlanctl.so
default: dynamic


libvlanctl.so:
	cp libvlanctl.so_$(PROFILE)_save libvlanctl.so

dynamic: generic_private_lib_install

clean:
	rm -f libvlanctl.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
