all:
	tar xf ./scripts_$(PROFILE)_save.tgz
	cp ./scripts/dumpsysinfo.sh $(INSTALL_DIR)/opt/scripts/


clean:
	rm -f $(INSTALL_DIR)/opt/scripts/dumpsysinfo.sh


CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
