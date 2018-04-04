/*
<:label-BRCM:2012:DUAL/GPL:standard

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

:> 
*/
#include <linux/types.h>
#include <linux/pci.h>
#include <linux/kernel.h>
#include <linux/init.h>

#include <bcm_map_part.h>
#include <bcmpci.h>
#include <boardparms.h>

extern struct pci_ops bcm63xx_pci_ops;
static struct resource bcm_pci_io_resource = {
    .name   = "bcm63xx pci IO space",
    .start  = BCM_PCI_IO_BASE,
    .end    = BCM_PCI_IO_BASE + BCM_PCI_IO_SIZE - 1,
    .flags  = IORESOURCE_IO
};

static struct resource bcm_pci_mem_resource = {
    .name   = "bcm63xx pci memory space",
    .start  = BCM_PCI_MEM_BASE,
    .end    = BCM_PCI_MEM_BASE + BCM_PCI_MEM_SIZE - 1,
    .flags  = IORESOURCE_MEM
};

struct pci_controller bcm63xx_controller = {
    .pci_ops	= &bcm63xx_pci_ops,
    .io_resource	= &bcm_pci_io_resource,
    .mem_resource	= &bcm_pci_mem_resource,
};

#if defined(PCIEH)
extern struct pci_ops bcm63xx_pcie_ops;
static struct resource bcm_pcie_io_resource = {
    .name   = "bcm63xx pcie null io space",
    .start  = 0,
    .end    = 0,
    .flags  = 0
};

static struct resource bcm_pcie_mem_resource = {
    .name   = "bcm63xx pcie memory space",
    .start  = BCM_PCIE_MEM1_BASE,
    .end    = BCM_PCIE_MEM1_BASE + BCM_PCIE_MEM1_SIZE - 1,
    .flags  = IORESOURCE_MEM
};

struct pci_controller bcm63xx_pcie_controller = {
    .pci_ops	= &bcm63xx_pcie_ops,
    .io_resource	= &bcm_pcie_io_resource,
    .mem_resource	= &bcm_pcie_mem_resource,
};

#endif

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268)
extern uint32 pci63xx_wlan_soft_config_space[WLAN_ONCHIP_DEV_NUM][WLAN_ONCHIP_PCI_HDR_DW_LEN];
static int __init bcm63xx_pci_swhdr_patch(void)
{
    /* modify sw pci hdr for different board for onchip wlan */
    int i;
    for (i = 0; i <WLAN_ONCHIP_DEV_NUM; i++) {       
        BpUpdateWirelessPciConfig(pci63xx_wlan_soft_config_space[i][0],pci63xx_wlan_soft_config_space[i],WLAN_ONCHIP_PCI_HDR_DW_LEN);
    }
    return 0;
}
#endif

static int __init bcm63xx_pci_init(void)
{
    /* adjust global io port range */
    ioport_resource.start = BCM_PCI_IO_BASE;
    ioport_resource.end = BCM_PCI_IO_BASE + BCM_PCI_IO_SIZE-1;

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268)
    bcm63xx_pci_swhdr_patch();
#endif
    /* bus 0 */
    register_pci_controller(&bcm63xx_controller);

#if defined(PCIEH)
#if defined(UBUS2_PCIE)
#if defined(CONFIG_BCM96318)
    PCIEH_CPU_INTR1_REGS->maskClear = (
        PCIE_CPU_INTR1_PCIE_INTD_CPU_INTR |
        PCIE_CPU_INTR1_PCIE_INTC_CPU_INTR |
        PCIE_CPU_INTR1_PCIE_INTB_CPU_INTR |
        PCIE_CPU_INTR1_PCIE_INTA_CPU_INTR );
        
   /* setup outgoing mem resource window */
	 PCIEH_MISC_REGS->cpu_2_pcie_mem_win0_base_limit = (((BCM_PCIE_MEM1_BASE+BCM_PCIE_MEM1_SIZE-1)&PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_LIMIT_MASK) \
	 																								|((BCM_PCIE_MEM1_BASE>>PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_LIMIT_SHIFT)<<PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_LIMIT_BASE_SHIFT));	 																								
	 																									
	 PCIEH_MISC_REGS->cpu_2_pcie_mem_win0_lo |= (BCM_PCIE_MEM1_BASE&PCIE_MISC_CPU_2_PCI_MEM_WIN_LO_BASE_ADDR_MASK);
	 
	 /* setup incoming DDR memory BAR(1) */
   PCIEH_RC_CFG_VENDOR_REGS->specificReg1 = PCIE_RC_CFG_VENDOR_SPECIFIC_REG1_ENDIAN_MODE_BYTE_ALIGN;
	 PCIEH_MISC_REGS->rc_bar1_config_lo = ((DDR_UBUS_ADDRESS_BASE&PCIE_MISC_RC_BAR_CONFIG_LO_MATCH_ADDRESS_MASK) \
	                                       | PCIE_MISC_RC_BAR_CONFIG_LO_SIZE_256MB);

   PCIEH_MISC_REGS->ubus_bar1_config_remap = PCIE_MISC_UBUS_BAR_CONFIG_ACCESS_EN;

    /* set device bus/func/func */
    PCIEH_PCIE_EXT_CFG_REGS->index = (BCM_BUS_PCIE_DEVICE<<PCIE_EXT_CFG_BUS_NUM_SHIFT);
#endif

#else	/* UBUS2_PCIE */
		
#if defined(CONFIG_BCM96816)
    PCIEH_BRIDGE_REGS->bridgeOptReg1 |= (PCIE_BRIDGE_OPT_REG1_EN_RD_BE | PCIE_BRIDGE_OPT_REG1_EN_RD_BE_NOSWAP);
#endif

#if defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)
    PCIEH_BRIDGE_REGS->bridgeOptReg1 |= (
        PCIE_BRIDGE_OPT_REG1_en_l1_int_status_mask_polarity |
        PCIE_BRIDGE_OPT_REG1_en_pcie_bridge_hole_detection  |
        PCIE_BRIDGE_OPT_REG1_en_rd_reply_be_fix |
        PCIE_BRIDGE_OPT_REG1_enable_rd_be_opt);
    
    PCIEH_BRIDGE_REGS->rcInterruptMask |= (
    	PCIE_BRIDGE_INTERRUPT_MASK_int_a_MASK |
        PCIE_BRIDGE_INTERRUPT_MASK_int_b_MASK |
        PCIE_BRIDGE_INTERRUPT_MASK_int_c_MASK |
        PCIE_BRIDGE_INTERRUPT_MASK_int_c_MASK );

    /* enable credit checking and error checking */
    PCIEH_BRIDGE_REGS->bridgeOptReg2 |= ( PCIE_BRIDGE_OPT_REG2_enable_tx_crd_chk_MASK |
                                          PCIE_BRIDGE_OPT_REG2_dis_ubus_ur_decode_MASK );
#endif

#if defined(CONFIG_BCM963268)
    /* setup outgoing window */
    PCIEH_BRIDGE_REGS->Ubus2PcieBar0BaseMask |= ((BCM_PCIE_MEM1_BASE&PCIE_BRIDGE_BAR0_BASE_base_MASK)|
                                                 (((BCM_PCIE_MEM1_BASE+BCM_PCIE_MEM1_SIZE-1) >>PCIE_BRIDGE_BAR0_BASE_base_MASK_SHIFT)
                                                    << PCIE_BRIDGE_BAR0_BASE_mask_MASK_SHIFT)) | PCIE_BRIDGE_BAR0_BASE_swap_enable;
#endif

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM963268)
    /* set device bus/func/func */
    PCIEH_BRIDGE_REGS->bridgeOptReg2 |= ((BCM_BUS_PCIE_DEVICE<<PCIE_BRIDGE_OPT_REG2_cfg_type1_bus_no_SHIFT) |
        PCIE_BRIDGE_OPT_REG2_cfg_type1_bd_sel_MASK );
#endif

#endif /* UBUS2_PCIE */

    /* setup class code, as bridge */
    PCIEH_BLK_428_REGS->idVal3 &= ~PCIE_IP_BLK428_ID_VAL3_CLASS_CODE_MASK;
    PCIEH_BLK_428_REGS->idVal3 |= (PCI_CLASS_BRIDGE_PCI << 8);	
    /* disable bar0 size */
    PCIEH_BLK_404_REGS->config2 &= ~PCIE_IP_BLK404_CONFIG_2_BAR1_SIZE_MASK;
    
#ifdef LOW_MEM_WINDOW_1MB_ONLY
    {
        extern bool bcm63xx_pcie_early_detect_ext_bridge(void);
        if(bcm63xx_pcie_early_detect_ext_bridge()){
        /* external PCIE switch */
        /* use memory resource of the larger window at HIMEM */
        bcm_pcie_mem_resource.start = BCM_PCIE_MEM2_BASE;
        bcm_pcie_mem_resource.end = BCM_PCIE_MEM2_BASE + BCM_PCIE_MEM2_SIZE - 1;
        }
    }		
#endif    
    /*bus 1 and 2 */                            
    register_pci_controller(&bcm63xx_pcie_controller);
#endif /* PCIEH */

    return 0;
}

arch_initcall(bcm63xx_pci_init);
