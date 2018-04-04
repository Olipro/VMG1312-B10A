#
# Makefile for the Broadcom NAND MTD
#

obj-$(CONFIG_MTD_BRCMNAND)		+=  bcm63xx-nand.o brcmnand_base.o brcmnand_bbt.o
## obj-$(CONFIG_MTD_BRCMNAND_EDU)		+= edu.o
obj-$(CONFIG_MTD_BRCMNAND_CORRECTABLE_ERR_HANDLING)	+= brcmnand_cet.o
#start __MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11
EXTRA_CFLAGS	+= -I $(TOPDIR)/include/asm-generic -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD)
ifeq ($(strip $(INC_NAND_FLASH_DRIVER)),1)
EXTRA_CFLAGS += -DINC_NAND_FLASH_DRIVER=$(INC_NAND_FLASH_DRIVER)
endif
#end __MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11

