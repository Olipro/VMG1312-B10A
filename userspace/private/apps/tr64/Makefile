EXE = tr64c
dynamic: generic_exe_install

clean:
	rm -f tr64c


tr64c:
	cp tr64c_$(PROFILE)_save tr64c
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
