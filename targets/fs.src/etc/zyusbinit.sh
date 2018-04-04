#!/bin/sh
echo -e "\033[1;34mInitial USB driver...\033[0m"
KERNELVER=`uname -r`
test -e /lib/modules/$KERNELVER/kernel/drivers/usb/class/usblp.ko && insmod /lib/modules/$KERNELVER/kernel/drivers/usb/class/usblp.ko 
test -e /lib/modules/$KERNELVER/kernel/drivers/usb/storage/usb-storage.ko && insmod /lib/modules/$KERNELVER/kernel/drivers/usb/storage/usb-storage.ko 
#test -e /lib/modules/$KERNELVER/kernel/drivers/usb/host/ohci-hcd.ko && insmod /lib/modules/$KERNELVER/kernel/drivers/usb/host/ohci-hcd.ko
#sleep 5
#test -e /lib/modules/$KERNELVER/kernel/drivers/usb/host/ehci-hcd.ko && insmod /lib/modules/$KERNELVER/kernel/drivers/usb/host/ehci-hcd.ko 
