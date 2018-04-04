EXE = sysdiagd
dynamic: generic_exe_install

clean:
	rm -f sysdiagd


sysdiagd:
	cp sysdiagd_$(PROFILE)_save sysdiagd
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
