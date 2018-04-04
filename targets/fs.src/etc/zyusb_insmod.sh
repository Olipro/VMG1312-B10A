#!/bin/sh
sleep 3
insmod /lib/modules/2.6.30/kernel/drivers/usb/host/ehci-hcd.ko
sleep 15
insmod /lib/modules/2.6.30/kernel/drivers/usb/host/ohci-hcd.ko
