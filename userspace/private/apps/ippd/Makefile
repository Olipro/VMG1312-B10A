EXE = ippd
dynamic: generic_exe_install

clean:
	rm -f ippd


ippd:
	cp ippd_$(PROFILE)_save ippd
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
