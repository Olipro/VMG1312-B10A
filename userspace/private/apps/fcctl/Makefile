EXE = fcctl
dynamic: generic_exe_install

clean:
	rm -f fcctl


fcctl:
	cp fcctl_$(PROFILE)_save fcctl
	ln -sf fcctl $(INSTALL_DIR)/bin/fc

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
