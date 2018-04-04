#wput.mk
#mm make file for wput
SHELL=/bin/sh
prefix=/usr
bindir=${exec_prefix}/bin
localedir=$(prefix)/share/locale
CC=gcc
CFLAGS=  -Wall  -g -DLOCALEDIR=\"$(localedir)\" -INONE/include
#LIBS=   -lgnutls-openssl
LIBS=
EXE=../wput
GETOPT=
MEMDBG=
OBJ=wput.o netrc.o ftp.o ftplib.o utils.o progress.o socketlib.o queue.o ftp-ls.o $(GETOPT) $(MEMDBG)
HEAD=wput.h netrc.h ftp.h ftplib.h utils.h progress.h socketlib.h _queue.h windows.h config.h constants.h


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common



#
# GPL apps and libs are only allowed to include header files from the
# gpl and public directories
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_INCLUDE_PATHS := -I.\
                         -I$(BUILD_DIR)/userspace/gpl/include  \
                         -I$(BUILD_DIR)/userspace/gpl/include/$(OALDIR) \
                         -I$(BUILD_DIR)/userspace/public/include  \
                         -I$(BUILD_DIR)/userspace/public/include/$(OALDIR)


#
# GPL apps and libs are only allowed to link with libraries from the
# gpl and public directories.
#
# WARNING: Do not modify this section unless you understand the
# license implications of what you are doing.
#
ALLOWED_LIB_DIRS := /lib:/lib/gpl:/lib/public



all: wput

wput.o ftp.o queue.o: $(HEAD)
$(OBJ): utils.h windows.h constants.h config.h
socketlib.o: socketlib.h
progress.o: progress.h
ftplib.o: socketlib.h ftplib.h
ftp-ls.o: ftp.h wget.h url.h

wput:   $(OBJ)
	$(CC) -o $(EXE) $(OBJ) $(LIBS)
clean:
	rm -f *.o *~ *.bak ../wput getopt/*.o
win-clean: clean
	rm -r msvcpp/[Dd]ebug msvcpp/[Rr]elease ../wput.exe
