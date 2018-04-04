EXE = pwrctl
dynamic: generic_exe_install

clean:
	rm -f pwrctl


pwrctl:
	cp pwrctl_$(PROFILE)_save pwrctl
	ln -sf pwrctl $(INSTALL_DIR)/bin/pwr

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
