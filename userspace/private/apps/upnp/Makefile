EXE = upnp
dynamic: generic_exe_install

clean:
	rm -f upnp


upnp:
	cp upnp_$(PROFILE)_save upnp
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
