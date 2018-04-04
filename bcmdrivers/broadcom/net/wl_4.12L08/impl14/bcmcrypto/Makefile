# bcmcrypto shared library makefile

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -I../include
CFLAGS += -I../include/bcmcrypto
CFLAGS += -DIL_BIGENDIAN
CFLAGS += -s
CFLAGS += -Os
CFLAGS += -fomit-frame-pointer
CFLAGS += -Wall 

-include files

OBJS = $(foreach x, $(FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

dynamic: $(OBJS)
ifneq ($(strip $(OBJS)),)
	$(CC) -shared -Os -o libwlbcmcrypto.so $^
endif

static: $(OBJS)
ifneq ($(strip $(OBJS)),)
	$(AR) -rcs libwlbcmcrypto.a $^
endif

install: dynamic
	install -m 755 libwlbcmcrypto.so $(INSTALL_DIR)/lib

clean:
	rm -f $(OBJS)
ifneq ($(strip $(OBJS)),)
	rm -f libwlbcmcrypto.*
endif
