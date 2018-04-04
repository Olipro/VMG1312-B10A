

all:
	$(MAKE) -C ./source bin/smbd
	install -m 755 ./source/bin/smbd $(INSTALL_DIR)/bin/smbd
#	install -m 755 ./source/bin/smbpasswd $(INSTALL_DIR)/bin/smbpasswd
	install -m 444 ./smb.conf $(INSTALL_DIR)/etc
	install -m 444 ./smb.passwd $(INSTALL_DIR)/etc
	

clean:
	if [ -f ./source/Makefile ];then $(MAKE) -C ./source clean;fi;

