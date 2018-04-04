# wps makefile

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -I.
CFLAGS += -I./common/include
CFLAGS += -I./brcm_apps/apps
CFLAGS += -I./brcm_apps/arch/bcm947xx
CFLAGS += -I./brcm_apps/include
CFLAGS += -I../include
CFLAGS += -I../include/bcmcrypto
CFLAGS += -I../router/libupnp
CFLAGS += -I../router/libupnp/include
CFLAGS += -I../router/eapd
CFLAGS += -I../router/nas
CFLAGS += -I../router/shared
CFLAGS += -I../shared
CFLAGS += -I../shared/bcmwifi/include
CFLAGS += -I../wps/brcm_apps/upnp/WFADevice
CFLAGS += -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD)
CFLAGS += -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
CFLAGS += -I$(BUILD_DIR)/userspace/private/include
CFLAGS += -I$(BUILD_DIR)/userspace/public/include
CFLAGS += -I$(BUILD_DIR)/userspace/public/include/linux
CFLAGS += -DBCMUPNP
CFLAGS += -DBCMWPA2
CFLAGS += -DBCMWPSAP
CFLAGS += -DDSLCPE
CFLAGS += -D_REENTRANT 
CFLAGS += -D__linux__
CFLAGS += -DIL_BIGENDIAN
CFLAGS += -DUPNP_WFA_DEVICE
CFLAGS += -DWPS_UPNP_DEVICE
CFLAGS += -DWPS_AP_M2D
CFLAGS += -DWPS_ADDCLIENT_WWTP
CFLAGS += -DWPS_ROUTER
#CFLAGS += -D_TUDEBUGTRACE
#CFLAGS += -DDSLCPE_NG
# __MSTC__, Paul Ho, WEP block in ap-pbc / Open block in ap-pbc
CFLAGS += -DSUPPORT_ENR_BLOCK
#endif


# __MSTC__, Paul Ho, MTS WPS Button Scenario
ifneq ($(strip $(BUILD_MSTC_WPS_BTN_SCE)),)
CFLAGS += -DMSTC_WPS_BTN_SCE
endif
ifneq ($(strip $(BUILD_MSTC_WPS_BTN_PER)),)
CFLAGS += -DMTS_WPS_BTN_PER=$(BUILD_MSTC_WPS_BTN_PER)
endif
ifneq ($(strip $(BUILD_MSTC_WPS_WLAN_ONE_LED)),)
CFLAGS += -DMTS_WPS_WLAN_ONE_LED
endif
ifneq ($(strip $(BUILD_WPS5GLED)),)
CFLAGS += -DWPS5GLED
endif
# end, __MSTC__
LDFLAGS = -Os
LDFLAGS += -L$(EXTRALIBDIR)
LDFLAGS += -lc
LDFLAGS += -lgcc_s
LDFLAGS += -L$(INSTALL_DIR)/lib
LDFLAGS += -lnvram
LDFLAGS += -lwlbcmcrypto
LDFLAGS += -lwlbcmshared
LDFLAGS += -lwlupnp
LDFLAGS += -lwlctl
LDFALSG += -ldl
LDFLAGS += -L$(INSTALL_DIR)/lib/public
LDFLAGS += -lcms_msg
LDFLAGS += -L$(INSTALL_DIR)/lib/private
LDFLAGS += $(CMS_COMMON_LIBS)
LDFLAGS += -L.
LDFLAGS += -lwps

-include files

LIBWPS_OBJS         = $(foreach x, $(LIBWPS_FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))
WPS_MONITOR_OBJS    = $(foreach x, $(WPS_MONITOR_FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))
WPS_AP_OBJS         = $(foreach x, $(WPS_AP_FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))
WPS_ENR_OBJS        = $(foreach x, $(WPS_ENR_FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))
WPS_WFA_OBJS        = $(foreach x, $(WPS_WFA_FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

vpath %.c ./brcm_apps/apps
vpath %.c ./brcm_apps/arch/bcm947xx
vpath %.c ./brcm_apps/linux
vpath %.c ./brcm_apps/upnp/WFADevice
vpath %.c ./linux/ap
vpath %.c ./common/ap
vpath %.c ./common/enrollee
vpath %.c ./common/registrar
vpath %.c ./common/shared
vpath %.c ./common/sta

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

mkversion:
	sh ./mkversion.sh ./common/include/version.h.in ./common/include/wps_version.h "$(PROD_TAG)"

libwps.so: $(LIBWPS_OBJS)
ifneq ($(strip $(LIBWPS_OBJS)),)
	$(CC) -shared -Os -o $@ $^
endif

wps_monitor.lib: $(WPS_MONITOR_OBJS)
ifneq ($(strip $(WPS_MONITOR_OBJS)),)
	$(AR) -rcs $@ $^
endif

wps_ap.lib : $(WPS_AP_OBJS)
ifneq ($(strip $(WPS_AP_OBJS)),)
	$(AR) -rcs $@ $^
endif

wps_enr.lib : $(WPS_ENR_OBJS)
ifneq ($(strip $(WPS_ENR_OBJS)),)
	$(AR) -rcs $@ $^
endif

wps_wfa.lib : $(WPS_WFA_OBJS)
ifneq ($(strip $(WPS_WFA_OBJS)),)
	$(AR) -rcs $@ $^
endif

wps_monitor: wps_gpio_dslcpe.o wps_monitor.lib wps_ap.lib wps_enr.lib xml_WFADevice.o wps_wfa.lib libwps.so
	$(CC) -o $@ $^ $(LDFLAGS)

install: dynamic
	install -m 755 libwps.so $(INSTALL_DIR)/lib
	install -m 755 wps_monitor $(INSTALL_DIR)/bin

clean:
	rm -f wps_monitor
	rm -f wps_gpio_dslcpe.o
	rm -f $(LIBWPS_OBJS)
	rm -f $(WPS_MONITOR_OBJS)
	rm -f $(WPS_AP_OBJS)
	rm -f $(WPS_ENR_OBJS)
	rm -f xml_WFADevice.o
	rm -f $(WPS_WFA_OBJS)
ifneq ($(strip $(LIBWPS_OBJS)),)
	rm -f libwps.so
endif
ifneq ($(strip $(WPS_MONITOR_FILES)),)
	rm -f wps_monitor.lib
endif
ifneq ($(strip $(WPS_AP_FILES)),)
	rm -f wps_ap.lib
endif
ifneq ($(strip $(WPS_ENR_FILES)),)
	rm -f wps_enr.lib
endif
ifneq ($(strip $(WPS_WFA_FILES)),)
	rm -f wps_wfa.lib
endif

dynamic: wps_monitor
static:
