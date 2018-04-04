EXE = hotplug
dynamic: generic_exe_install


clean:
	rm -f hotplug


hotplug:
	cp hotplug_$(PROFILE)_save hotplug
	ln -sf ../bin/hotplug $(INSTALL_DIR)/sbin/hotplug
CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
