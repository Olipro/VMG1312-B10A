EXE = mcpctl
dynamic: generic_exe_install

clean:
	rm -f mcpctl


mcpctl:
	cp mcpctl_$(PROFILE)_save mcpctl
	ln -sf mcpctl $(INSTALL_DIR)/bin/mcp

CURR_DIR := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
