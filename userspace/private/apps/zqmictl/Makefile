EXE = zqmictl
dynamic: generic_exe_install

clean:
	rm -f zqmictl


zqmictl:
	cp zqmictl_$(PROFILE)_save zqmictl
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
