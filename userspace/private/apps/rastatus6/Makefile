EXE = rastatus6
dynamic: generic_exe_install

clean:
	rm -f rastatus6


rastatus6:
	cp rastatus6_$(PROFILE)_save rastatus6
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
