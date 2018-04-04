# $USAGI: Makefile,v 1.10 2002/11/29 10:36:27 yoshfuji Exp $
#
# Makefile for some libs needed in the kernel.
#
# Note! Dependencies are done automagically by 'make dep', which also
# removes any old dependencies. DON'T put your own dependencies here
# unless it's something special (ie not a .c file).
#

lib-$(CONFIG_MSTC_ZLIB) := adler32.o compress.o crc32.o \
	gzio.o uncompr.o deflate.o trees.o zutil.o inflate.o inftrees.o inffast.o

