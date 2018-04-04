# bcmupnp makefile

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -Iinclude
CFLAGS += -Idevice
CFLAGS += -Idevice/InternetGatewayDevice
CFLAGS += -Idevice/WFADevice
CFLAGS += -I../../include
CFLAGS += -I../../include/bcmcrypto
CFLAGS += -I../../shared
CFLAGS += -I../../router/shared
CFLAGS += -I../../wps/common/include
CFLAGS += -DIL_BIGENDIAN
CFLAGS += -DDSLCPE
CFLAGS += -Wall
CFLAGS += -DBCMWPA2 -DBCMWPS -DBCMUPNP -DBCMWPSAP -D__CONFIG_WSCCMD__

LDFLAGS = -Os
LDFLAGS += -L$(EXTRALIBDIR)
LDFLAGS += -lc
LDFLAGS += -lgcc_s
LDFLAGS += -L$(INSTALL_DIR)/lib
LDFLAGS += -lnvram
LDFLAGS += -lwlbcmcrypto
LDFLAGS += -lwlbcmshared
LDFLAGS += -L$(BUILD_DIR)/userspace/gpl/apps/iptables/libiptc
LDFLAGS += -liptc

-include files

OBJS = $(foreach x, $(FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

vpath %.c device
vpath %.c device/WFADevice
vpath %.c device/InternetGatewayDevice
vpath %.c upnp
vpath %.c upnp/linux

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

bcmupnp: $(OBJS)
ifneq ($(strip $(OBJS)),)
	$(CC) -o $@ $^ $(LDFLAGS)
endif

install: bcmupnp
	install -m 755 $< $(INSTALL_DIR)/bin

clean:
	rm -f $(OBJS)
ifneq ($(strip $(OBJS)),)
	rm -f bcmupnp
endif

dynamic static: bcmupnp
