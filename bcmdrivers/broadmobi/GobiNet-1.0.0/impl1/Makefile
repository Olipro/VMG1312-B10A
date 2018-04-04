obj-m := GobiNet.o
GobiNet-objs := GobiUSBNet.o QMIDevice.o QMI.o
#KDIR := /lib/modules/$(shell uname -r)/build
KDIR := $(LINUX_SRC)
PWD := $(shell pwd)
OUTPUTDIR=/lib/modules/$(LINUX_SRC)/kernel/drivers/net/usb/

all: clean
	$(MAKE) -C $(KDIR) M=$(PWD) modules

#install: all
#	mkdir -p $(OUTPUTDIR)
#	cp -f GobiNet.ko $(OUTPUTDIR)
#	depmod

clean:
	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions Module.* modules.order


