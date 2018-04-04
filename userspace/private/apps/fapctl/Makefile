EXE = fapctl
dynamic: generic_exe_install

clean:
	rm -f fapctl


fapctl:
	cp fapctl_$(PROFILE)_save fapctl
	ln -sf fapctl $(INSTALL_DIR)/bin/fap

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
