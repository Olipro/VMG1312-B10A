EXE = ethctl
dynamic: generic_exe_install


clean:
	rm -f ethctl


ethctl:
	cp ethctl_$(PROFILE)_save ethctl
	ln -sf ../bin/ethctl $(INSTALL_DIR)/sbin/ethctl
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
