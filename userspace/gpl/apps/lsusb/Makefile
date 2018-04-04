#include ${PRODUCT_ROOT_DIR}/Project/Make.config

#LDFLAGS += -lpthread -lusb
#CFLAGS += -I$(APP_LIBUSB_1_0_9_DIR) -L$(APP_LIBUSB_1_0_9_DIR)/libusb/.libs

#TARGET = lsusb
#OBJECTS = lsusb.o

#all: $(OBJECTS) $(TARGET)
#		@echo "###################################"
#		@echo "#             LSUSB"
#		@echo "#        Build up successfully"
#		@echo "###################################"		

#$(TARGET): $(OBJECTS) 
#		$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

#clean:
#	rm -rf $(OBJECTS)
#	rm -rf $(TARGET)
	

LDFLAGS += -lpthread -lusb

all: lsusb 

# I-Wei 05/07/2012 porting
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
export BUILD_DIR
CFLAGS += -I$(BUILD_DIR)/userspace/gpl/libs/libusb-1.0.9 -L$(BUILD_DIR)/userspace/gpl/libs/libusb-1.0.9/libusb/.libs

lsusb: lsusb.c
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS)

# CHIA 9/29/2011 porting
romfs:
	cp -f lsusb $(INSTALL_DIR)/bin/lsusb
	cp -f librt-0.9.29.so $(INSTALL_DIR)/lib
	cp -f librt.so.0 $(INSTALL_DIR)/lib

clean:
	-rm -f lsusb *.o