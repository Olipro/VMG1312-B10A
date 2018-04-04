LIB = libmdm.so
default: dynamic


libmdm.so:
	cp libmdm.so_$(PROFILE)_save libmdm.so

dynamic: generic_private_lib_install

clean:
	rm -f libmdm.so

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
