#	$Id: Makefile,v 0.1 2010/09/01 01:54:32 Justin.hu@zyxel.cn $


PPPDSRCS = chat.c

CDEF1=	-DTERMIOS			# Use the termios structure
CDEF2=	-DSIGTYPE=void			# Standard definition
CDEF3=	-UNO_SLEEP			# Use the usleep function
CDEF4=	-DFNDELAY=O_NDELAY		# Old name value
CDEFS=	$(CDEF1) $(CDEF2) $(CDEF3) $(CDEF4)

#CFLAGS=	-O2 -g -pipe $(CDEFS)
CFLAGS=	-s -Os -pipe -Wall $(CDEFS)

INSTALL= install

all:	chat install

chat:	chat.o
	$(CC) -o chat chat.o

chat.o:	chat.c
	$(CC) -c $(CFLAGS) -o chat.o chat.c

install: chat
	mkdir -p $(INSTALL_DIR)/bin
	$(INSTALL) -m 755 chat $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/chat
#	$(INSTALL) -c -m 644 chat.8 $(MANDIR)/man8

clean:
	rm -f $(INSTALL_DIR)/bin/chat
	rm -f chat.o chat *~

depend:
	$(CPP) -M $(CFLAGS) $(CHATSRCS) >.depend