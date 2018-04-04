#
# for Singly linked list package.
# muhammad a muquit
# Aug-07-1998
#
#CC= gcc
DEFS= -DPACKAGE_NAME=\"\" -DPACKAGE_TARNAME=\"\" -DPACKAGE_VERSION=\"\" -DPACKAGE_STRING=\"\" -DPACKAGE_BUGREPORT=\"\" -DSTDC_HEADERS=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_SYS_STAT_H=1 -DHAVE_STDLIB_H=1 -DHAVE_STRING_H=1 -DHAVE_MEMORY_H=1 -DHAVE_STRINGS_H=1 -DHAVE_INTTYPES_H=1 -DHAVE_STDINT_H=1 -DHAVE_UNISTD_H=1 -DHAVE_STRING_H=1 -DHAVE_STRINGS_H=1 -DHAVE_MEMORY_H=1 -DHAVE_MALLOC_H=1 -DHAVE_UNISTD_H=1 -DHAVE_CTYPE_H=1 -DHAVE_STDINT_H=1 -DHAVE_SYS_TYPES_H=1 -DHAVE_STDLIB_H=1 -DHAVE_FCNTL_H=1 -DHAVE_SYS_FILE_H=1 -DHAVE_FLOCK=1 -DHAVE_SOCKET=1 -DHAVE_HTONL=1 -DHAVE_GETHOSTNAME=1 -DHAVE_GETHOSTBYADDR=1 -DHAVE_YP_GET_DEFAULT_DOMAIN=1 -DHAVE_LIBNSL=1 -DHAVE_RES_SEARCH=1 -DHAVE_LIBRESOLV=1 -DHAVE_INET_ATON=1 -DHAVE_DN_SKIPNAME=1 -DHAVE_MKSTEMP=1 -DHAVE_OPENSSL=1 
#AR= ar cq
#RANLIB= ranlib
LIBNAME= libsll.a

INCLUDES=  -I. 

# replace -O with -g in order to debug

DEFINES= $(INCLUDES) $(DEFS) -DSYS_UNIX=1
CFLAGS= -O $(DEFINES)

SRCS = sll.c
OBJS = sll.o

.c.o:
	rm -f $@
	$(CC) $(CFLAGS) -c $*.c

all: $(LIBNAME)

$(LIBNAME): $(OBJS)
	rm -f $@
	$(AR) -rcs $@ $(OBJS)
	$(RANLIB) $@

sll.o: sll.h

clean:
	rm -f $(OBJS) $(LIBNAME) core a.out

realclean:
	rm -f $(OBJS) $(LIBNAME) core a.out config.cache config.log config.status
	(cd examples/append;make clean)
	(cd examples/append_sorted;make clean)
	(cd examples/delete;make clean)
	(cd examples/insert;make clean)
