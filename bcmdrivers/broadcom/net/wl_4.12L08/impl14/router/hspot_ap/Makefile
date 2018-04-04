#Linux Makefile
BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
SRCBASE=$(BRCMDRIVERS_DIR)/broadcom/net/wl/bcm9$(BRCM_CHIP)
include $(BUILD_DIR)/make.common
include $(SRCBASE)/dslcpe_wlan_minor_version
DSLCPE_WLAN_VERSION=cpe$(BRCM_VERSION).$(BRCM_RELEASE)'L'$(BRCM_EXTRAVERSION).$(DSLCPE_WLAN_MINOR_VERSION)

INCLUDE += -I$(SRCBASE)/include 
INCLUDE += -I$(SRCBASE)/hspot/include -I$(SRCBASE)/hspot/dsp -I$(SRCBASE)/hspot/wlan -I$(SRCBASE)/hspot/tcp_srv -I$(SRCBASE)/hspot/pkt -I$(SRCBASE)/hspot/pro
INCLUDE += -I. -I.. -I$(SRCBASE)/include -I$(SRCBASE)/shared -I$(SRCBASE)/router/shared -I$(SRCBASE)/wl/exe
INCLUDE += -I$(SRCBASE)/security/secfrw/include -I$(SRCBASE)/supp/include -I$(SRCBASE)/shared/bcmwifi/include

CFLAGS += $(INCLUDE)
CFLAGS += -DWIFI_ACT_FRAME -DHOTSPOT_AP -DWLTDLS -Wall

CFLAGS  += -s
CFLAGS += -DDSLCPE_WLAN_VERSION=\"$(DSLCPE_WLAN_VERSION)\"
CFLAGS += -DDSLCPE

#CFLAGS += -c -g -m32 -Wextra -Werror
#LDFLAGS += -m32 --cref -lpthread -lrt
#LDFLAGS += -Wl,-Map,hspotap.map

LDFLAGS += -L. -L$(TOP)/nvram -L$(INSTALL_DIR)/lib -lnvram
LDFLAGS += -lwlbcmcrypto
LDFLAGS += -lwlbcmshared
LDFLAGS += -lwlctl
#LDFLAGS += -L$(TOP)/shared -L$(INSTALL_DIR)/shared/usr/lib -lshared

vpath=$(SRCBASE)/hspot/util

vpath %.c $(SRCBASE)/shared
vpath %.c $(SRCBASE)/shared/bcmwifi/src
vpath %.c $(SRCBASE)/security/secfrw/tmr
vpath %.c $(SRCBASE)/security/secfrw/utils
vpath %.c $(SRCBASE)/router/hspot_ap
vpath %.c $(SRCBASE)/hspot/util
vpath %.c $(SRCBASE)/hspot/pkt
vpath %.c $(SRCBASE)/hspot/dsp
vpath %.c $(SRCBASE)/hspot/wlan
vpath %.c $(SRCBASE)/hspot/pro
vpath %.c $(SRCBASE)/hspot/tcp_srv

TESTOBJ= $(shell  find ../../hspot -name trace.c 2>/dev/null)

OBJS = trace.o
OBJS += pktEncode.o pktDecode.o
OBJS += pktEncodeIe.o pktDecodeIe.o
OBJS += pktEncodeGas.o pktDecodeGas.o
OBJS += pktEncodeAnqp.o pktDecodeAnqp.o
OBJS += pktEncodeHspotAnqp.o pktDecodeHspotAnqp.o pktDecodeWfd.o pktEncodeWnm.o
OBJS += dsp_ap.o tmr.o
OBJS += wlu_api.o wlu_linux_api.o
OBJS += bcm_app_utils.o bcmseclib_linux_timer.o bcmseclib_timer.o bcm_llist.o
OBJS += proGas.o
OBJS += bcmevent.o
OBJS += tcp_srv.o

all: hspotap
ifneq ($(strip $(TESTOBJ)),)
libhspotap.so: $(OBJS)
	$(LD) -s -Os -shared -soname libhspotap.so -o libhspotap.so  $(OBJS)
else
libhspotap.so:
endif
	install -m 755 libhspotap.so $(INSTALL_DIR)/lib

hspotap: libhspotap.so  hspotap.o
	$(CC)  -s -Os -o $@  hspotap.o $(LDFLAGS) -lhspotap



install: all
	install -d $(INSTALL_DIR)/bin
	install hspotap $(INSTALL_DIR)/bin
	install -m 755 libhspotap.so $(INSTALL_DIR)/lib

clean:
	rm -f *.o *.map hspotap
ifneq ($(strip $(TESTOBJ)),)
	rm -f libhspotap.so
endif
