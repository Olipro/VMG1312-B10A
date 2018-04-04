dynamic: install


all: install


install:
	cp -f celld_$(PROFILE)_save celld
	cp -f celld_$(PROFILE)_save $(INSTALL_DIR)/bin/celld
	cp -f libcell.so_$(PROFILE)_save libcell.so
	cp -f libcell.so_$(PROFILE)_save $(INSTALL_DIR)/lib/libcell.so
	tar xf ./wwan_scripts_$(PROFILE)_save.tgz
	cp -f ./wwan_scripts/des3_encrypt.sh $(INSTALL_DIR)/sbin/des3_encrypt.sh
	cp -f ./wwan_scripts/wwan-add.sh $(INSTALL_DIR)/sbin/wwan-add.sh
	cp -f ./wwan_scripts/wwan-conn-down.sh $(INSTALL_DIR)/sbin/wwan-conn-down.sh
	cp -f ./wwan_scripts/wwan-conn-up.sh $(INSTALL_DIR)/sbin/wwan-conn-up.sh
	cp -f ./wwan_scripts/wwan-hotplug.sh $(INSTALL_DIR)/sbin/wwan-hotplug.sh
	cp -f ./wwan_scripts/wwan-info.sh $(INSTALL_DIR)/sbin/wwan-info.sh
	cp -f ./wwan_scripts/wwan-init.sh $(INSTALL_DIR)/sbin/wwan-init.sh
	cp -f ./wwan_scripts/wwan-lib.sh $(INSTALL_DIR)/sbin/wwan-lib.sh
	cp -f ./wwan_scripts/wwan-main.sh $(INSTALL_DIR)/sbin/wwan-main.sh
	cp -f ./wwan_scripts/wwan-package-match.sh $(INSTALL_DIR)/sbin/wwan-package-match.sh
	cp -f ./wwan_scripts/wwan-remove.sh $(INSTALL_DIR)/sbin/wwan-remove.sh


clean:
	rm -f celld
	rm -f libcell.so
	rm -rf wwan_scripts


CURR_DIR  := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
