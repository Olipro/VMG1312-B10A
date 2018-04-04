EXE = gmacctl
dynamic: generic_exe_install

clean:
	rm -f gmacctl


gmacctl:
	cp gmacctl_$(PROFILE)_save gmacctl
	ln -sf gmacctl $(INSTALL_DIR)/bin/gmac

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
