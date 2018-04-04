EXE = xdslctl
dynamic: generic_exe_install


clean:
	rm -f xdslctl


xdslctl:
	cp xdslctl_$(PROFILE)_save xdslctl
	ln -sf xdslctl $(INSTALL_DIR)/bin/adslctl
	ln -sf xdslctl $(INSTALL_DIR)/bin/adsl


CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
