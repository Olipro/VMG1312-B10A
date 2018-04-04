#
# Router Wireless Interface Configuration Utility Makefile
#
# Copyright (C) 2012, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: Makefile 341899 2012-06-29 04:06:38Z $
#

include $(SRCBASE)/router/.config

CFLAGS	+= -I. -I$(TOP)/shared -I$(SRCBASE)/include -Wall
CFLAGS	+= $(WLAN_ComponentIncPath)
#CFLAGS	+= -g -DDEBUG
CFLAGS	+= -s
ifeq ($(CONFIG_RTR_OPTIMIZE_SIZE),y)
CFLAGS  += -Os
else
CFLAGS  += -O2
endif
LDFLAGS	+= -L$(TOP)/nvram -lnvram -L$(TOP)/shared -lshared
LDFLAGS += $(EXTRA_LDFLAGS)


vpath %.c $(SRCBASE)/shared $(WLAN_ComponentSrcDirs)

OBJS := wlconf.o bcmwifi_channels.o

all: wlconf

clean:
	rm -f *.o wlconf

install: all
	install -d $(INSTALLDIR)/usr/sbin
	install wlconf $(INSTALLDIR)/usr/sbin
	$(STRIP) $(INSTALLDIR)/usr/sbin/wlconf

wlconf: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
