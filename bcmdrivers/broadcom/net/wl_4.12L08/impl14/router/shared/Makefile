# wlbcmshared shared library makefile

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -I.
CFLAGS += -I../../include
CFLAGS += -I../../shared
CFLAGS += -I../../shared/bcmwifi/include
CFLAGS += -DBCMSUP_PSK
CFLAGS += -DBCMSUPPL
CFLAGS += -DBCMWPS
CFLAGS += -DBCMWPA2
CFLAGS += -DIL_BIGENDIAN
CFLAGS += -DDSLCPE
CFLAGS += -s
CFLAGS += -Os
CFLAGS += -Wall
CFLAGS += -fomit-frame-pointer

LDFLAGS = -shared
LDFLAGS += -Os

-include files

OBJS = $(foreach x, $(FILES), $(shell find . ../../shared -name "$x" -exec echo $(x:.c=.o) ";"))

vpath %.c ./
vpath %.c ../../shared

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

dynamic: $(OBJS)
ifneq ($(strip $(OBJS)),)
ifeq ($(BRCM_WAPI),y)
	$(CC) $(LDFLAGS) -o libwlbcmshared_wapi.so $^
else
	$(CC) $(LDFLAGS) -o libwlbcmshared.so $^
endif
endif

static: $(OBJS)
ifneq ($(strip $(OBJS)),)
ifeq ($(BRCM_WAPI),y)
	$(AR) -rcs libwlbcmshared_wapi.a $^
else
	$(AR) -rcs libwlbcmshared.a $^
endif
endif

install: dynamic
ifeq ($(BRCM_WAPI),y)
	install -m 755 libwlbcmshared_wapi.so $(INSTALL_DIR)/lib/libwlbcmshared.so
else
	install -m 755 libwlbcmshared.so $(INSTALL_DIR)/lib/libwlbcmshared.so
endif

clean:
	rm -f $(OBJS)
ifneq ($(strip $(OBJS)),)
	rm -f libwlbcmshared.a libwlbcmshared.so
	rm -f libwlbcmshared_wapi.a libwlbcmshared_wapi.so
endif
