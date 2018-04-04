#  Copyright (c) 1988, 1989, 1990, 1991, 1992, 1993, 1994, 1995, 1996, 1997
# 	The Regents of the University of California.  All rights reserved.
#
#  Redistribution and use in source and binary forms, with or without
#  modification, are permitted provided that: (1) source code distributions
#  retain the above copyright notice and this paragraph in its entirety, (2)
#  distributions including binary code include the above copyright notice and
#  this paragraph in its entirety in the documentation or other materials
#  provided with the distribution, and (3) all advertising materials mentioning
#  features or use of this software display the following acknowledgement:
#  ``This product includes software developed by the University of California,
#  Lawrence Berkeley Laboratory and its contributors.'' Neither the name of
#  the University nor the names of its contributors may be used to endorse
#  or promote products derived from this software without specific prior
#  written permission.
#  THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
#  WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
#  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
#
# @(#) $Header: /tcpdump/master/tcpdump/Makefile.in,v 1.325 2008-11-21 23:17:26 guy Exp $ (LBL)

#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#

CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))
EXE=rpcapd

include $(BUILD_DIR)/make.common

#
# Various configurable paths (remember to edit Makefile.in, not Makefile)
#

# Top level hierarchy
prefix = /usr/local
exec_prefix = ${prefix}
# Pathname of directory to install the binary
sbindir = ${exec_prefix}/sbin
# Pathname of directory to install the man page
mandir = ${prefix}/share/man

# VPATH
srcdir = .


PROG = rpcapd

# CC      = gcc
CFLAGS  = -pthread -D_DEBUG -DREMOTE -DHAVE_PCAPREADEX  -g
#flags for debugging: -D_DEBUG -g -Wall

INCLUDE = -I. -I ../../libs/libpcap

LIB	= -L ../../libs/libpcap -lpcap -lcrypt
#Solaris: add '-lsocket'

INSTALL = /usr/bin/install -c
INSTALL_PROGRAM = ${INSTALL}
INSTALL_DATA = ${INSTALL} -m 644

LIBPATH	= -L../

# Files that are needed to compile this project
FILES = rpcapd.o daemon.o utils.o fileconf.o pcap-remote.o sockutils.o pcap-new.o

# Makefile syntax:
# Project name : dependencies
#  command line

rpcapd: $(FILES)
	$(CC) $(CFLAGS) $(INCLUDE) -o rpcapd $(FILES) $(LIBPATH) $(LIB)

rpcapd.o: rpcapd.c
	$(CC) $(CFLAGS) $(INCLUDE) -c rpcapd.c

daemon.o: daemon.c
	$(CC) $(CFLAGS) $(INCLUDE) -c daemon.c

utils.o: utils.c
	$(CC) $(CFLAGS) $(INCLUDE) -c utils.c

fileconf.o: fileconf.c
	$(CC) $(CFLAGS) $(INCLUDE) -c fileconf.c

sockutils.o: ../../libs/libpcap/sockutils.c
	$(CC) $(CFLAGS) $(INCLUDE) -c ../../libs/libpcap/sockutils.c

pcap-remote.o: ../../libs/libpcap/pcap-remote.c
	$(CC) $(CFLAGS) $(INCLUDE) -c ../../libs/libpcap/pcap-remote.c

pcap-new.o: ../../libs/libpcap/pcap-new.c
	$(CC) $(CFLAGS) $(INCLUDE) -c ../../libs/libpcap/pcap-new.c

install: rpcapd generic_exe_install 
clean:
	rm -f *.o
	rm rpcapd

