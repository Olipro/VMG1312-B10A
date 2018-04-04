#include ${PRODUCT_ROOT_DIR}/Project/Make.config

LDLIBS += -lusb
EXEC = usb_modeswitch
SRCS = usb_modeswitch.c

all: $(EXEC)

#LIBDIR = $(APP_LIBUSB_0_1_12_DIR)/.libs
#CFLAGS += -I$(APP_LIBUSB_0_1_12_DIR)

#$(EXEC): $(SRCS)
#	$(CC) $(CFLAGS) -L$(LIBDIR) -o $@ $^ $(LDLIBS)

#clean:
#	-rm -f $(EXEC) *.o

# I-Wei 05/07/2012 porting
CURR_DIR := $(shell pwd)
BUILD_DIR:=$(subst /userspace, /userspace,$(CURR_DIR))
BUILD_DIR:=$(word 1, $(BUILD_DIR))
include $(BUILD_DIR)/make.common
export BUILD_DIR
LIBDIR = $(BUILD_DIR)/userspace/gpl/libs/libusb-0.1.12/.libs
CFLAGS += -I$(BUILD_DIR)/userspace/gpl/libs/libusb-0.1.12

$(EXEC): $(SRCS)
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -L$(LIBDIR) $(LDFLAGS) -o $@ $^ $(LDLIBS)

# I-Wei 05/07/2012 porting
romfs:
	cp -f usb_modeswitch $(INSTALL_DIR)/bin/usb_modeswitch
	mkdir -p $(INSTALL_DIR)/etc/wwan/
	cp -rf usb_modeswitch.d $(INSTALL_DIR)/etc/wwan/
#	$(ROMFSINST) /bin/$(EXEC)
#	$(ROMFSINST) -d /etc_ro/mwr/usb_modeswitch.d

clean:
	-rm -f $(EXEC) *.o