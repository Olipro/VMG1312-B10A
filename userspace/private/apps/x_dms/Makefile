dynamic: install


all: install


install:
	cp -f bcmmserver_VMG1312-B10A_save $(INSTALL_DIR)/bin/bcmmserver
	cp -f libbdlna-dms.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/private/libbdlna-dms.so
	cp -f libbmedia.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/private/libbmedia.so
	cp -f libbdlna-dms-aal.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/private/libbdlna-dms-aal.so
	cp -f libbdlna.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/private/libbdlna.so
	cp -f libb_playback_ip.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/private/libb_playback_ip.so
CURR_DIR  := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
