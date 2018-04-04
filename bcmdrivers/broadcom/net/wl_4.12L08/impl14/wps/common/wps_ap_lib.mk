#
# Copyright (C) 2009, Broadcom Corporation
# All Rights Reserved.
# 
# This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
# the contents of this file may not be disclosed to third parties, copied
# or duplicated in any form, in whole or in part, without the prior
# written permission of Broadcom Corporation.
#
# $Id: wps_ap_lib.mk,v 1.13 2010/03/17 03:27:48 Exp $
# 
# Linux makefile
#

BLDTYPE = release
#BLDTYPE = debug

ifeq ("$(CROSS_COMPILE)","mipsel-uclibc-")
CC_TYPE=uclibc
else
CC_TYPE=linux
endif

ifeq ($(BLDTYPE),debug)
export CFLAGS = -Wall -Wnested-externs -g -D_TUDEBUGTRACE
export CXXFLAGS = -Wall -Wnested-externs -g -D_TUDEBUGTRACE
else
export CFLAGS = -Wall -Wnested-externs
export CXXFLAGS = -Wall -Wnested-externs
endif


export CC = mipsel-$(CC_TYPE)-gcc
export LD = mipsel-$(CC_TYPE)-gcc
export LDFLAGS = -r

BRCMBASE = ../..
UPNPBASE = $(SRCBASE)/router/bcmupnp

export INCLUDE = -I$(TOOLCHAINS)/include -I$(SRCBASE)/include \
	-I$(SRCBASE)/router/shared -I$(BRCMBASE)/include -I./include \
	-I$(UPNPBASE)/include -I$(UPNPBASE)/upnp/linux -I$(UPNPBASE)/device/InternetGatewayDevice -I$(UPNPBASE)/device -I$(UPNPBASE)/device/WFADevice \
	-I$(SRCBASE)/router/libbcm -I$(SRCBASE)/router/eapd

# Include external openssl path
ifeq ($(EXTERNAL_OPENSSL),1)
export INCLUDE += -I$(EXTERNAL_OPENSSL_INC)
else
export INCLUDE += -I$(SRCBASE)/include/bcmcrypto
endif

LIBDIR = .

OBJS =  $(addprefix $(LIBDIR)/, ap/ap_api.o \
	ap/ap_eap_sm.o \
	ap/ap_upnp_sm.o \
	registrar/reg_sm.o )




default: $(LIBDIR)/libwpsap.a

$(LIBDIR)/libwpsap.a: $(OBJS)
	$(AR) cr  $@ $^  

$(LIBDIR)/shared/%.o :  shared/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/ap/%.o :  ap/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/enrollee/%.o :  enrollee/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@

$(LIBDIR)/registrar/%.o :  registrar/%.c
	$(CC) -c $(CFLAGS) $(INCLUDE) $< -o $@
