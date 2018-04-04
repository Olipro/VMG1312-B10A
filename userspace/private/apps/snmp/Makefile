EXE = snmpd
dynamic: generic_exe_install




snmpd:
	cp snmpd_$(PROFILE)_save snmpd
	ln -sf ../bin/snmpd $(INSTALL_DIR)/sbin/snmpd


clean:
	rm -f snmpd


CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
