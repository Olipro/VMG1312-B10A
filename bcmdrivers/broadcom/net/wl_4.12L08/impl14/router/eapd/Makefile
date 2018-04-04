# eapd makefile

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -I.
CFLAGS += -I../../include
CFLAGS += -I../../shared
CFLAGS += -I../../router/shared
CFLAGS += -I../../router/nas
CFLAGS += -I../../shared/bcmwifi/include
CFLAGS += -DBCMWPA2 
CFLAGS += -DEAPD_WKSP_AUTO_CONFIG
CFLAGS += -DIL_BIGENDIAN
CFLAGS += -DDSLCPE
CFLAGS += -DDSLCPE_EVT
CFLAGS += -Wall

LDFLAGS = -Os 
LDFLAGS += -L$(EXTRALIBDIR)
LDFLAGS += -lc
LDFLAGS += -lgcc_s
LDFLAGS += -L$(INSTALL_DIR)/lib
LDFLAGS += -lnvram
LDFLAGS += -lwlbcmcrypto
LDFLAGS += -lwlbcmshared

-include files

OBJS = $(foreach x, $(FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

ifneq ($(strip $(OBJS)),)
ifeq ($(BRCM_WAPI),y)
OBJS += wai_eap.o
endif
endif

ifneq ($(strip $(OBJS)),)
ifeq ($(CONFIG_BCMDCS),y)
OBJS += dcs_eap.o
CFLAGS += -DBCM_DCS
endif
endif


%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

eapd: $(OBJS)
ifneq ($(strip $(OBJS)),)
ifeq ($(BRCM_WAPI),y)
	$(CC) -o $@_wapi $^ $(LDFLAGS)
else
	$(CC) -o $@ $^ $(LDFLAGS)
endif
endif

install: eapd
ifeq ($(BRCM_WAPI),y)
	install -m 755 $<_wapi $(INSTALL_DIR)/bin/$<
else
	install -m 755 $< $(INSTALL_DIR)/bin/$<
endif

clean:
	rm -f $(OBJS)
ifneq ($(strip $(OBJS)),)
	rm -f eapd_wapi eapd
endif

dynamic static: eapd
