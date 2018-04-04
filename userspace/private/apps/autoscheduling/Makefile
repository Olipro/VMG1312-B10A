EXE = autoscheduling
dynamic: generic_exe_install

clean:
	rm -f autoscheduling


autoscheduling:
	cp autoscheduling_$(PROFILE)_save autoscheduling
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
