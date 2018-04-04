# lltd makefile

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -I../../include
CFLAGS += -I../../shared
CFLAGS += -I../../router/shared
CFLAGS += -I../../shared/bcmwifi/include
CFLAGS += -DIL_BIGENDIAN
CFLAGS += -DDSLCPE
CFLAGS += -DBCMWPA2
CFLAGS += -s
CFLAGS += -Os
CFLAGS += -Wall
CFLAGS += -fomit-frame-pointer

LDFLAGS = -Os 
LDFLAGS += -L$(INSTALL_DIR)/lib
LDFLAGS += -lnvram
LDFLAGS += -lwlbcmcrypto 
LDFLAGS += -lwlbcmshared

-include files

OBJS = $(foreach x, $(FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

vpath %.c ./src

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

lld2d: $(OBJS)
ifneq ($(strip $(OBJS)),)
	$(CC) -o $@ $^ $(LDFLAGS)
endif

install: lld2d
	install -m 755 $< $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/$<
	install -m 444 ./src/wrt54g.large.ico $(INSTALL_DIR)/etc
	install -m 444 ./src/wrt54g.small.ico $(INSTALL_DIR)/etc

clean:
	rm -f $(OBJS)
ifneq ($(strip $(OBJS)),)
	rm -f lld2d
endif

dynamic static: lld2d
