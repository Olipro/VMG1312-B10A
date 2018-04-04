EXE = link_updown
dynamic: generic_exe_install

clean:
	rm -f link_updown


link_updown:
	cp link_updown_$(PROFILE)_save link_updown
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
