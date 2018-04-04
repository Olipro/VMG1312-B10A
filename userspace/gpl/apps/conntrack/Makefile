

all:
	$(MAKE) -C conntrack-tools-0.9.14
	install -m 755 ./libs/libnfnetlink.so.0.2.0 $(INSTALL_DIR)/lib/libnfnetlink.so.0
	install -m 755 ./libs/libnetfilter_conntrack.so.3.0.0 $(INSTALL_DIR)/lib/libnetfilter_conntrack.so.3
	install -m 755 ./conntrack-tools-0.9.14/src/.libs/conntrack $(INSTALL_DIR)/bin/conntrack

clean:
	if [ -f ./conntrack-tools-0.9.14/Makefile ];then $(MAKE) -C ./conntrack-tools-0.9.14 clean;fi;

