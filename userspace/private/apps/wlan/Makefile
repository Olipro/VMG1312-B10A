dynamic: install


all: install


install:
	cp -f libwlbcmcrypto.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/libwlbcmcrypto.so
	cp -f libwlbcmshared.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/libwlbcmshared.so
	cp -f libwlupnp.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/libwlupnp.so
	cp -f eapd_VMG1312-B10A_save $(INSTALL_DIR)/bin/eapd
	cp -f lld2d_VMG1312-B10A_save $(INSTALL_DIR)/bin/lld2d
	cp -f wrt54g.large.ico_VMG1312-B10A_save $(INSTALL_DIR)/etc/wrt54g.large.ico
	cp -f wrt54g.small.ico_VMG1312-B10A_save $(INSTALL_DIR)/etc/wrt54g.small.ico
	cp -f nas_VMG1312-B10A_save $(INSTALL_DIR)/bin/nas
	cp -f nvram_VMG1312-B10A_save $(INSTALL_DIR)/bin/nvram
	cp -f libnvram.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/libnvram.so
	cp -f nvram_params_VMG1312-B10A_save $(INSTALL_DIR)/etc/wlan/nvram_params
	cp -f nvramUpdate_VMG1312-B10A_save $(INSTALL_DIR)/bin/nvramUpdate
	cp -f wlctl_VMG1312-B10A_save $(INSTALL_DIR)/bin/wlctl
	cp -f libwlctl.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/libwlctl.so
	cp -f wlmngr_VMG1312-B10A_save $(INSTALL_DIR)/bin/wlmngr
	cp -f wlevt_VMG1312-B10A_save $(INSTALL_DIR)/bin/wlevt
	cp -f libwlmngr.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/libwlmngr.so
	cp -f wps_monitor_VMG1312-B10A_save $(INSTALL_DIR)/bin/wps_monitor
	cp -f libwps.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/libwps.so
	cp -f epi_ttcp_VMG1312-B10A_save $(INSTALL_DIR)/bin/epi_ttcp
	cp -f acsd_VMG1312-B10A_save $(INSTALL_DIR)/bin/acsd
	cp -f acs_cli_VMG1312-B10A_save $(INSTALL_DIR)/bin/acs_cli
	cp -f wl_server_socket_VMG1312-B10A_save $(INSTALL_DIR)/bin/wl_server_socket
	cp -f libwl_server_socket.so_VMG1312-B10A_save $(INSTALL_DIR)/lib/libwl_server_socket.so
	ln -sf wl_server_socket $(INSTALL_DIR)/bin/wl_server
	ln -sf wlctl $(INSTALL_DIR)/bin/wl
	ln -sf nas $(INSTALL_DIR)/bin/nas4not
CURR_DIR  := $(shell pwd)
BUILD_DIR := $(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR := $(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
