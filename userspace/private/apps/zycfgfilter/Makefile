EXE = zycfgfilter
dynamic: generic_exe_install

clean:
	rm -f zycfgfilter


zycfgfilter:
	cp zycfgfilter_$(PROFILE)_save zycfgfilter
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
