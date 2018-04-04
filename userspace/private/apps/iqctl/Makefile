EXE = iqctl
dynamic: generic_exe_install

clean:
	rm -f iqctl


iqctl:
	cp iqctl_$(PROFILE)_save iqctl
	ln -sf iqctl $(INSTALL_DIR)/bin/iq

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
