
# To add a lib, add it to LIB_DIRS, and write the target rule in the
# last section of this Makefile
#
ifeq ($(strip $(BUILD_ZYXEL_VMG1312)),y)
LIB_DIRS = sqlite classpath-0.98 libusb-0.1.12 libusb-1.0.9
else
LIB_DIRS = jpeg-6b libiconv sqlite classpath-0.98 libusb-0.1.12 libusb-1.0.9
endif
#
# We need to declare these directory names as phony targets,
# otherwise make will think the directories themselves are
# the targets and will never execute the cmds.
# See GNU Make, 4.6 
#
.PHONY : $(LIB_DIRS)

all : $(LIB_DIRS)


#
# Cleaning rules
# See example in GNU Make, 4.6
#
clean:
	for dir in $(LIB_DIRS); do \
		if [ -d $$dir ] ; then \
			if [ -f $$dir/Makefile ] ; then \
				$(MAKE) -C $$dir clean; \
			fi \
		fi \
	done


#
# Set our CommEngine directory (by splitting the pwd into two words
# at /userspace and taking the first word only).
# Then include the common defines under CommEngine.
# You do not need to modify this part.
#
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))

include $(BUILD_DIR)/make.common

export BUILD_DIR



##############################################################
# 
# Rules for inidividual apps
# libs can be compiled or not depending on the profile (which
# is modified via make menuconfig)
#
##############################################################

ifneq ($(strip $(BUILD_DLNA)),)
sqlite: sanity_check
	$(MAKE) -C sqlite dynamic
ifeq ($(strip $(BUILD_ZYXEL_VMG1312)),y)
	@echo "############ VMG1312-Series not support DLNA DMS v2.1.3 ############"
else
libiconv: sanity_check
	(tar xkfj libiconv.tar.bz2 2> /dev/null || true)
	$(MAKE) -C libiconv dynamic -j 1 
jpeg-6b: sanity_check
	cd jpeg-6b; make clean; make all; make install
endif

else
sqlite: sanity_check
	@echo "skipping SQLite (not configured)"
ifeq ($(strip $(BUILD_ZYXEL_VMG1312)),y)
	@echo "############ VMG1312-Series not support DLNA DMS v2.1.3 ############"
else
libiconv: sanity_check
	@echo "skipping Libiconv (not configured)"
jpeg-6b: sanity_check
	@echo "skipping jpeg-6b (not configured)"
endif

endif

ifneq ($(strip $(BUILD_OSGI_JVM)),)
classpath-0.98:
	(tar xkfz classpath-0.98.tar.gz 2> /dev/null || true)
	cd classpath-0.98; ./configure --host=mips-linux-uclibc --target=mips-linux-uclibc --enable-local-sockets --disable-gconf-peer --disable-gtk-peer --disable-plugin --enable-jni --disable-alsa --disable-dssi --disable-gjdoc --disable-libtool-lock --disable-default_toolkit --disable-gmp --disable-examples --without-x --prefix=$(INSTALL_DIR)/usr/local/classpath
	cd classpath-0.98; cp native/jni/java-nio/Makefile.modified native/jni/java-nio/Makefile
	cd classpath-0.98; cp native/jni/native-lib/Makefile.modified native/jni/native-lib/Makefile
	cd classpath-0.98; $(MAKE) install
	cd $(INSTALL_DIR)/usr/local/classpath/share/classpath; unzip -o glibj.zip
	cd $(INSTALL_DIR)/usr/local/classpath/share/classpath; rm -rf glibj.zip
	cp -f $(INSTALL_DIR)/usr/local/classpath/lib/classpath/libjavanet.so $(INSTALL_DIR)/lib/libjavanet.so
else
classpath-0.98:
	@echo "skipping classpath-0.98 (not configured)"
endif

#__ZyXEL__, WeiZen, support libusb-0.1.12
ifneq ($(strip $(BUILD_ZyXEL_WWAN_PACKAGE)),)
libusb-0.1.12:
	cd $@; ./configure --host=mips-uclibc-linux CC=$(CC) CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) STRIP=$(STRIP) --prefix=$(BUILD_DIR)/userspace/gpl/libs/libusb-0.1.12/install
	$(MAKE) -C $@ all
	cd $@; ln -sf libusb-0.1.so.4.4.4 .libs/libusb.so
	cd $@; install -m 755 .libs/libusb-0.1.so.4.4.4 $(INSTALL_DIR)/lib/libusb.so.0.1.12
	cd $@; ln -sf libusb.so.0.1.12 $(INSTALL_DIR)/lib/libusb-0.1.so.4
else
libusb-0.1.12:
	@echo "skipping libusb-0.1.12 (not configured)"
endif

#__ZyXEL__, WeiZen, support libusb-1.0.9
ifneq ($(strip $(BUILD_ZyXEL_WWAN_PACKAGE)),)
libusb-1.0.9:
	cd $@; ./configure --host=mips-uclibc-linux CC=$(CC) CXX=$(CXX) AR=$(AR) RANLIB=$(RANLIB) STRIP=$(STRIP) NM=$(NM) OBJDUMP=$(OBJDUMP) --prefix=$(BUILD_DIR)/userspace/gpl/libs/libusb-1.0.9/install
	$(MAKE) -C $@ all
	cd $@; ln -sf libusb-1.0.so.0.1.0 libusb/.libs/libusb.so
	#cd $@; install -m 755 libusb/.libs/libusb-1.0.so.0.0.0 $(INSTALL_DIR)/lib/libusb.so.1.0.9
	#cd $@; ln -sf libusb.so.1.0.9 $(INSTALL_DIR)/lib/libusb-1.0.so.0
	cd $@; cp libusb/.libs/libusb-1.0.so.0 $(INSTALL_DIR)/lib/libusb-1.0.so.0
else
libusb-1.0.9:
	@echo "skipping libusb-1.0.9 (not configured)"
endif