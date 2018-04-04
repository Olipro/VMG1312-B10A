EXE = tr69c
dynamic: generic_exe_install

clean:
	rm -f tr69c


tr69c:
	cp tr69c_$(PROFILE)_save tr69c
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
