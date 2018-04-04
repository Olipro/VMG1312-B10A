EXE = vcautohunt
dynamic: generic_exe_install

clean:
	rm -f vcautohunt


vcautohunt:
	cp vcautohunt_$(PROFILE)_save vcautohunt
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
