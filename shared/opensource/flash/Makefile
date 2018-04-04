
ifeq ($(CONFIG_MIPS_BRCM),y)

# Linux

obj-y		+= flash_api.o flash_common.o

ifeq ($(strip $(BRCM_CHIP)),6368)
INC_CFI_FLASH_DRIVER=1
INC_SPI_FLASH_DRIVER=1
endif
ifeq ($(strip $(BRCM_CHIP)),6816)
INC_CFI_FLASH_DRIVER=1
INC_SPI_FLASH_DRIVER=1
endif
ifeq ($(strip $(BRCM_CHIP)),6362)
INC_CFI_FLASH_DRIVER=0
INC_SPI_FLASH_DRIVER=1
endif
ifeq ($(strip $(BRCM_CHIP)),6328)
INC_CFI_FLASH_DRIVER=0
INC_SPI_FLASH_DRIVER=1
endif
ifeq ($(strip $(BRCM_CHIP)),6318)
INC_CFI_FLASH_DRIVER=0
INC_SPI_FLASH_DRIVER=1
endif
ifeq ($(strip $(BRCM_CHIP)),63268)
INC_CFI_FLASH_DRIVER=0
INC_SPI_FLASH_DRIVER=1
endif

ifeq ($(strip $(INC_CFI_FLASH_DRIVER)),1)
obj-y		+= cfiflash.o
endif
ifeq ($(strip $(INC_SPI_FLASH_DRIVER)),1)
obj-y		+= spiflash.o
endif
#start __MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
# __MSTC__, zongyue
ifeq ($(strip $(INC_NAND_FLASH_DRIVER)),1)
obj-y		+= nandflash.o
EXTRA_CFLAGS += -I$(INC_BRCMSHARED_PRIV_PATH)/$(BRCM_BOARD) -I$(INC_BRCMDRIVER_PRIV_PATH)/$(BRCM_BOARD) -DCFG_RAMAPP
endif
#end __MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 

#start __MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 
EXTRA_CFLAGS += -DCONFIG_BCM9$(BRCM_CHIP) -I$(INC_BRCMDRIVER_PUB_PATH)/$(BRCM_BOARD) -I$(INC_BRCMSHARED_PUB_PATH)/$(BRCM_BOARD) -DINC_CFI_FLASH_DRIVER=$(INC_CFI_FLASH_DRIVER) -DINC_SPI_FLASH_DRIVER=$(INC_SPI_FLASH_DRIVER)
ifeq ($(strip $(INC_NAND_FLASH_DRIVER)),1)
EXTRA_CFLAGS += -DINC_NAND_FLASH_DRIVER=$(INC_NAND_FLASH_DRIVER)
endif
#end __MSTC__, RaynorChung: Support 963268 nand flash, patch form SVN#3597 on http://svn.zyxel.com.tw/svn/CPE_SW1/BCM96368/trunk/P-870HA/branches/cht/fttb8/4.11 

endif
ifeq ($(strip $(MSTC_ROM_D)), y)
EXTRA_CFLAGS += -DMSTC_ROM_D
endif
# __MSTC__, Dennis ZyXEL OBM ImageDefault feature, zongyue
ifeq ($(strip $(MSTC_OBM_IMAGE_DEFAULT)), y)
EXTRA_CFLAGS += -DMSTC_OBM_IMAGE_DEFAULT
endif






