# Makefile for the bcm963xx board drivers
#


obj-y := board.o bcm63xx_flash.o bcm63xx_led.o bcm63xx_gpio.o

ifeq ($(strip $(BRCM_CHIP)),6816)
obj-y += spidevices.o
endif

ifeq ($(strip $(BRCM_CHIP)),63268)
obj-y += spidevices.o
endif


ifneq ($(strip $(CONFIG_BCM_CPLD1)),)
    obj-y += bcmcpld1.o
	EXTRA_CFLAGS += -DCONFIG_BCM_CPLD1
endif

ifneq ($(strip $(CONFIG_MSTC_WDT)),)
	obj-y += mstc_wdt.o
endif

EXTRA_CFLAGS += -I. -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)

# __MSTC__, Paul Ho, for WLAN/WPS Button
ifneq ($(strip $(CONFIG_BCM_WLAN)),)
	ifneq ($(strip $(BUILD_MSTC_WLAN_BTN_PER)),)
		EXTRA_CFLAGS += -DMTS_WLAN_BTN_PER=$(BUILD_MSTC_WLAN_BTN_PER)
	endif
	ifneq ($(strip $(BUILD_MSTC_WPS_BTN_PER)),)
		EXTRA_CFLAGS += -DMTS_WPS_BTN_PER=$(BUILD_MSTC_WPS_BTN_PER)
	endif
	ifneq ($(strip $(BUILD_MSTC_WLAN_WPS_BTN_SEPARATE)),)
		EXTRA_CFLAGS += -DMSTC_WLAN_WPS_BTN_SEPARATE
	endif
	ifneq ($(strip $(BUILD_MSTC_WPS_BTN_SCE)),)
		EXTRA_CFLAGS += -DMSTC_WPS_BTN_SCE=$(BUILD_MSTC_WPS_BTN_SCE)
	endif
endif
# end, __MSTC__

#__MSTC__, FuChia, Reset button time
ifneq ($(strip $(MSTC_RESTORE_BTN_PER)),)
EXTRA_CFLAGS += -DCV_RESTORE_BTN_PER=$(MSTC_RESTORE_BTN_PER)
endif
#end, __MSTC__, FuChia


EXTRA_CFLAGS += -Werror -Wfatal-errors
EXTRA_CFLAGS += -g



ifneq ($(strip $(BUILD_NORWAY_BTN_LED_OFF)),)
EXTRA_CFLAGS += -DNORWAY_BTN_LED_OFF
endif


ifneq ($(strip $(CONFIG_BCM_WLAN)),)
	EXTRA_CFLAGS += -DWIRELESS
endif

# __MSTC__, RaynorChung: Support 963268, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
ifeq ($(strip $(INC_NAND_FLASH_DRIVER)),1)
EXTRA_CFLAGS += -DINC_NAND_FLASH_DRIVER=$(INC_NAND_FLASH_DRIVER)
endif
# end __MSTC__, RaynorChung: Support 963268, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
# __MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
ifeq ($(strip $(MSTC_ROM_D)), y)
EXTRA_CFLAGS += -DMSTC_ROM_D
endif

ifeq ($(strip $(MSTC_OBM_IMAGE_DEFAULT)), y)
EXTRA_CFLAGS += -DMSTC_OBM_IMAGE_DEFAULT
endif
# end __MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
#__MSTC__, Richard Huang
ifeq ($(strip $(BUILD_MSTC_ALL_WAN_SAME_MAC)),y)
EXTRA_CFLAGS += -DMSTC_ALL_WAN_SAME_MAC
endif
ifeq ($(strip $(BUILD_11ac_throughput_patch_from_412L07)),y)
EXTRA_CFLAGS += -DBUILD_11ac_throughput_patch_from_412L07
endif
ifeq ($(strip $(BUILD_MSTC_DIFF_L2_SAME_MAC)),y)
EXTRA_CFLAGS += -DMSTC_DIF_L2_SAME_MAC
endif
ifeq ($(strip $(BUILD_MTSC_DUAL_IMAGE_SELECT)),y)
EXTRA_CFLAGS += -DMTSC_DUAL_IMAGE_SELECT
endif
ifeq ($(strip $(BUILD_NORWAY_CUSTOMIZATION)),y)
EXTRA_CFLAGS += -DMTSC_NORWAY_CUSTOMIZATION
endif
#__MSTC__, Richard Huang

# ZyXEL
ifeq ($(strip $(BUILD_UPC_FIXMAC_CUSTOMIZATION)),y)
EXTRA_CFLAGS += -DUPC_FIXMAC_CUSTOMIZATION
endif
# ZyXEL
#__MSTC__, LingChun
ifeq ($(strip $(BUILD_MSTC_FWUP_FROM_FILE)),y)
EXTRA_CFLAGS += -DMSTC_FWUP_FROM_FILE
endif
#__MSTC__, LingChun

-include $(TOPDIR)/Rules.make

clean:
	rm -f core *.o *.a *.s

