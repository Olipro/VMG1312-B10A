#
# Copyright (C) 2009, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: wps_common_lib.mk,v 1.8 2010/03/17 06:06:03 Exp $
#
# Linux makefile
#

BLDTYPE = release
#BLDTYPE = debug

ifeq ($(BLDTYPE),debug)
export CFLAGS = -Wall -Wnested-externs -g -D_TUDEBUGTRACE
export CXXFLAGS = -Wall -Wnested-externs -g -D_TUDEBUGTRACE
else
export CFLAGS = -Wall -Wnested-externs
export CXXFLAGS = -Wall -Wnested-externs
endif


ifeq ($(CC), arm-linux-gcc)
CFLAGS += -mstructure-size-boundary=8
STRIP = arm-linux-strip
endif
ifeq ($(CC), mipsel-uclibc-gcc)
STRIP = mipsel-uclibc-strip
endif
ifeq ($(CC), mipsel-linux-gcc)
STRIP = mipsel-linux-strip
endif
ifeq ($(CC), gcc)
STRIP = strip
endif

export LD = $(CC)
export LDFLAGS = -r

export INCLUDE = -I$(SRCBASE)/include -I./include

# Include external openssl path
ifeq ($(EXTERNAL_OPENSSL),1)
export INCLUDE += -I$(EXTERNAL_OPENSSL_INC)
else
export INCLUDE += -I$(SRCBASE)/include/bcmcrypto 
endif

LIBDIR = .

OBJS =  $(addprefix $(LIBDIR)/, shared/tutrace.o \
	shared/dev_config.o \
	shared/slist.o \
	enrollee/enr_reg_sm.o \
	shared/reg_proto_utils.o \
	shared/reg_proto_msg.o \
	shared/tlv.o \
	shared/state_machine.o \
	shared/buffobj.o )

# Add OpenSSL wrap API if EXTERNAL_OPENSSL defined
ifeq ($(EXTERNAL_OPENSSL),1)
OBJS += $(addprefix $(LIBDIR)/, shared/wps_openssl.o)
endif


ifeq ($(CC), gcc)
default: $(LIBDIR)/libwpscom.a
else
default: $(LIBDIR)/libwpscom.so $(LIBDIR)/libwpscom.a
endif


$(LIBDIR)/libwpscom.a: $(OBJS)
	$(AR) cr  $@ $^ 

$(LIBDIR)/libwpscom.so: $(OBJS)
	$(LD) -shared -o $@ $^

$(LIBDIR)/shared/%.o :  shared/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/ap/%.o :  ap/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/enrollee/%.o :  enrollee/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/registrar/%.o :  registrar/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@
