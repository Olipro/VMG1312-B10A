dynamic: install


all: install


install:
	install -m 755 app/voiceApp $(INSTALL_DIR)/bin
	install -m 755 app/icf.exe $(INSTALL_DIR)/bin
	install -m 755 -T app/icf.cfg $(INSTALL_DIR)/bin/icf.cfg
	install -m 755 app/mm.exe $(INSTALL_DIR)/bin
	install -m 755 app/ivrsys.bin $(INSTALL_DIR)/bin
	install -m 755 libs/libmicrosipaccessor.so $(INSTALL_DIR)/lib
	install -m 755 libs/libmicrosipclone.so $(INSTALL_DIR)/lib
	install -m 755 libs/libmicrosipcore.so $(INSTALL_DIR)/lib
	install -m 755 libs/libua.so $(INSTALL_DIR)/lib
	install -m 755 libs/libuaport.a $(INSTALL_DIR)/lib
	install -m 755 libs/libbos.so $(INSTALL_DIR)/lib/private/
	


clean:


