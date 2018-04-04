# acsd makefile

BUILD_DIR=$(word 1, $(subst /bcmdrivers, /bcmdrivers,$(shell pwd)))
include $(BUILD_DIR)/make.common

CFLAGS += -Iinclude
CFLAGS += -I../../include
CFLAGS += -I../../include/bcmcrypto
CFLAGS += -I../../shared
CFLAGS += -I../../router/shared
CFLAGS += -I../../shared/bcmwifi/include
CFLAGS += -DDSLCPE
CFLAGS += -Wall
#CFLAGS += -DDEBUG



LDFLAGS = -Os
LDFLAGS += -L$(EXTRALIBDIR)
LDFLAGS += -lc
LDFLAGS += -lgcc_s
LDFLAGS += -L$(INSTALL_DIR)/lib
LDFLAGS += -lnvram
LDFLAGS += -lwlbcmcrypto
LDFLAGS += -lwlbcmshared

ACSD_SRCFILES = dcs.c acs.c acsd_utils.c acsd_main.c acsd_cmd.c acsd_chanim.c bcmutils.c
ACSD_OBJS = $(foreach x, $(ACSD_SRCFILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

ACSCLI_SRCFILES = acsd_cli.c acsd_utils.c
ACSCLI_OBJS = $(foreach x, $(ACSCLI_SRCFILES), $(shell find . -name "$x" -exec echo $(x:.c=.o) ";"))

#$(warning ACSD_OBJ=$(ACSD_OBJS))
#$(warning ACSCLI_OBJ=$(ACSCLI_OBJS))

vpath %.c ../../shared/

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

acsd: $(ACSD_OBJS)
ifneq ($(strip $(ACSD_OBJS)),)
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS)
endif
	install -m 755 $@ $(INSTALL_DIR)/bin/$@

acs_cli: $(ACSCLI_OBJS)
ifneq ($(strip $(ACSCLI_OBJS)),)
	$(CC) $(CFLAGS) $^ -o $@  $(LDFLAGS)
endif
	install -m 755 $@ $(INSTALL_DIR)/bin/$@

clean:
	rm -f $(ACSD_OBJS)
	rm -f $(ACSCLI_OBJS)
ifneq ($(strip $(ACSD_OBJS)),)
	rm -f acsd acsd_cli
endif

dynamic static: acsd acs_cli
