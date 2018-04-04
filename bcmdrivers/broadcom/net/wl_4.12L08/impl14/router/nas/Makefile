#
# nas: 802.1x EAPOL to RADIUS proxy (Network Access Server)
#

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -I.
CFLAGS += -I../../include
CFLAGS += -I../../bcmcrypto
CFLAGS += -I../../shared
CFLAGS += -I../../router/eapd
CFLAGS += -I../../router/shared
CFLAGS += -I../../shared/bcmwifi/include

ifeq ($(DSLCPE_DT_BUILD),1)
	CFLAGS	+= -DRADIUS_RESTRICTION
endif
CFLAGS  += -DNAS_WKSP_BUILD_NAS_AUTH=1
CFLAGS	+= -DNAS_WKSP_BUILD_NAS_SUPPL=1
CFLAGS	+= -DNAS_WKSP_BUILD_CMD_LINE=0
CFLAGS	+= -DNAS_WKSP_USE_FILE_SYSTEM=1
CFLAGS	+= -DNAS_RADIUS
CFLAGS	+= -DNAS_WKSP_SELECT_SUPPORT
CFLAGS	+= -DNAS_WDS_SUPPORT
CFLAGS	+= -DNAS_WKSP_ON_DEMAND
CFLAGS	+= -DBCMSUPPL
CFLAGS	+= -DBCMWPS
CFLAGS	+= -DBCMWPA2
CFLAGS  += -DIL_BIGENDIAN
CFLAGS	+= -DDSLCPE
#CFLAGS += -DBCMDBG

CFLAGS += -Wall
CFLAGS += -s
CFLAGS += -Os
CFLAGS += -fomit-frame-pointer              

LDFLAGS = -Os
LDFALGS += -L$(EXTRALIBDIR)
LDFLAGS += -lc
LDFLAGS += -lgcc_s
LDFLAGS += -L$(INSTALL_DIR)/lib
LDFLAGS += -lnvram
LDFLAGS += -lwlbcmcrypto
LDFLAGS += -lwlbcmshared

-include files

OBJS = $(foreach x, $(FILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

nas: $(OBJS)
ifneq ($(strip $(OBJS)),)
	$(CC) -o $@ $^ $(LDFLAGS) 
endif

nas.a: $(OBJS)
ifneq ($(strip $(OBJS)),)
	$(AR) rcs $@ $^
endif

install: nas
	install -m 755 $< $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/$<
	ln -sf ../bin/$< $(INSTALL_DIR)/bin/nas4not

clean:
	rm -f $(OBJS)
ifneq ($(strip $(OBJS)),)
	rm -f nas.a
	rm -f nas
endif

dynamic: nas
static: nas.a


