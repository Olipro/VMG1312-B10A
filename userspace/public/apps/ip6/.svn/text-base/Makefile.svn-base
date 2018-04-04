
INSTALL=/usr/bin/install -c
INSTALL_PROGRAM=${INSTALL}
INSTALL_DATA=${INSTALL} -m 644
prefix=	/usr/local
exec_prefix=	${prefix}
bindir=	${exec_prefix}/bin
sbindir=${exec_prefix}/sbin


all: ip6

ip6: main.o libnetlink.o ll_map.o
	$(CC) $^ -o $@ 

main.o: main.c libnetlink.h ll_map.h utils.h rtm_map.h rt_names.h
	$(CC) -c $<
libnetlink.o: libnetlink.c libnetlink.h
	$(CC) -c $<


ll_map.o: ll_map.c libnetlink.h ll_map.h
	$(CC) -c $<

.PHONY: clean install
clean:
	-rm ip6 *.o

install: 
	$(INSTALL_PROGRAM) -m 755 ip6 $(INSTALL_DIR)/bin
	$(STRIP) $(INSTALL_DIR)/bin/ip6

	