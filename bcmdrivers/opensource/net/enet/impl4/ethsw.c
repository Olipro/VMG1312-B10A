/*
 Copyright 2007-2010 Broadcom Corp. All Rights Reserved.

 <:label-BRCM:2011:DUAL/GPL:standard
 
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
#include <linux/delay.h>
#include <linux/mii.h>
#include <linux/stddef.h>
#include <linux/ctype.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <board.h>
#include "boardparms.h"
#include <bcm_map_part.h>
#include "bcm_intr.h"
#include "bcmenet.h"
#include "bcmmii.h"
#include "ethswdefs.h"
#include "ethsw.h"
#include "ethsw_phy.h"
#include "bcmsw.h"
#include "bcmSpiRes.h"
#include "bcmswaccess.h"
#include "bcmswshared.h"
#include "bcmPktDma.h"
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
#include "fap_packet.h"
#endif

#if defined(CONFIG_BCM96828) && !defined(CONFIG_EPON_HGU)
int uni_to_uni_enabled = 0;
#endif
#if defined(CONFIG_BCM_GMAC)
#include "bcmgmac.h"
#endif

static void str_to_num(char *in, char *out, int len);
static int proc_get_sw_param(char *page, char **start, off_t off, int cnt, int *eof, void *data);
static int proc_set_sw_param(struct file *f, const char *buf, unsigned long cnt, void *data);
static void ethsw_init_table(ETHERNET_SW_INFO *sw);

static int proc_get_mii_param(char *page, char **start, off_t off, int cnt, int *eof, void *data);
static int proc_set_mii_param(struct file *f, const char *buf, unsigned long cnt, void *data);

extern struct net_device *vnet_dev[MAX_NUM_OF_VPORTS];
#if ((defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818)) && defined(DBL_DESC))
extern struct net_device *gponifid_to_dev[MAX_GEM_IDS];
#endif
#if defined(CONFIG_BCM96816)
extern struct net_device* bcm6829_to_dev[MAX_6829_IFS];
#endif
extern struct semaphore bcm_ethlock_switch_config;
extern uint8_t port_in_loopback_mode[TOTAL_SWITCH_PORTS];
extern atomic_t phy_write_ref_cnt;
extern atomic_t phy_read_ref_cnt;
void ethsw_phyport_rreg(int port, int reg, uint16 *data);
void ethsw_phyport_wreg(int port, int reg, uint16 *data);

// Software port index ---> real hardware param.
int switch_port_index[TOTAL_SWITCH_PORTS];
int switch_port_phyid[TOTAL_SWITCH_PORTS];
int switch_pport_phyid[TOTAL_SWITCH_PORTS] = {-1, -1, -1, -1, -1, -1, -1, -1};
int ext_switch_pport_phyid[TOTAL_SWITCH_PORTS] = {-1, -1, -1, -1, -1, -1, -1, -1};
#ifdef BUILD_MSTC_DSL_2492GNAU_B1BC_MLD //__MSTC__, Eason
unsigned long phyport_to_phyid[MAX_SWITCH_PORTS*2] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1};
#endif

spinlock_t spl_lock;
BcmEnet_devctrl *pVnetDev0_g;

static void ethsw_init_table(ETHERNET_SW_INFO *sw)
{
    int map, cnt;
    int i, j;

    spin_lock_init(&spl_lock);

    pVnetDev0_g = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);

    map = sw->port_map;
    bitcount(cnt, map);

    if ((cnt <= 0) || (cnt > BP_MAX_SWITCH_PORTS))
        return;

    for (i = 0, j = 0; i < cnt; i++, j++, map /= 2)
    {
        while ((map % 2) == 0)
        {
            map /= 2;
            j++;
        }
        switch_port_index[i] = j;
        switch_port_phyid[i] = sw->phy_id[j];
        switch_pport_phyid[j] = sw->phy_id[j];
//        ETHSW_PHY_SET_PHYID(j/*port*/, sw->phy_id[j] /*phy id*/);
    }

    return;
}

int ethsw_port_to_phyid(int port)
{
    return switch_port_phyid[port];
}

#ifdef BUILD_MSTC_DSL_2492GNAU_B1BC_MLD //__MSTC__, Eason
unsigned long ethsw_phyport_to_phyid(int port){
	if(port>=0 && port < MAX_SWITCH_PORTS*2){
		return phyport_to_phyid[port];
	}
	else{
		return -1;
	}
}
#endif

#if defined(CONFIG_BCM_ETH_PWRSAVE)
void ethsw_isolate_phy(int phyId, int isolate)
{
    uint16 v16;
    ethsw_phy_rreg(phyId, MII_CONTROL, &v16);
    if (isolate) {
        v16 |= MII_CONTROL_ISOLATE_MII;
    } else {
        v16 &= ~MII_CONTROL_ISOLATE_MII;
    }
    ethsw_phy_wreg(phyId, MII_CONTROL, &v16);
}
#endif

int ethsw_set_mac(int port, PHY_STAT ps)
{
    uint16 sw_port;
    int is6829 = 0;

#if defined(CONFIG_BCM96816)
    if ( IsExt6829(port) )
    {
       /* for the 6829 the port passed in is the
          physical port not the virtual port */
       sw_port = (uint16)(port & ~BCM_EXT_6829);
       is6829  = 1;
    }
    else
    {
        sw_port = (uint16)switch_port_index[port];
    }
#else
    sw_port = (uint16)switch_port_index[port];
#endif

#if defined(CONFIG_BCM96816)
    if (!is6829)
#endif
    if (port_in_loopback_mode[sw_port]) {
        return 0;
    }

    ethsw_set_mac_hw(sw_port, ps, is6829);

    return 0;
}

/* port = physical port */
int ethsw_phy_intr_ctrl(int port, int on)
{
    uint16 v16;

    down(&bcm_ethlock_switch_config);

#if defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96368) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828) || defined(CONFIG_BCM96318)
    if (on != 0)
        v16 = MII_INTR_ENABLE | MII_INTR_FDX | MII_INTR_SPD | MII_INTR_LNK;
    else
        v16 = 0;

    ethsw_phy_wreg(switch_pport_phyid[port], MII_INTERRUPT, &v16);
#endif

#if (defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818)) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828)
#if defined(CONFIG_BCM963268)
    if (port == GPHY_PORT_ID)
#elif defined(CONFIG_BCM96828)
    if ((port == GPHY1_PORT_ID) || (port == GPHY2_PORT_ID))
#endif
    {
        if (on != 0)
            v16 = ~(MII_INTR_FDX | MII_INTR_SPD | MII_INTR_LNK);
        else
            v16 = 0xFFFF;

        ethsw_phy_wreg(switch_pport_phyid[port], MII_INTERRUPT_MASK, &v16);
    }
#endif

    up(&bcm_ethlock_switch_config);

    return 0;
}

/* Called from ISR context. No sleeping locks */
void ethsw_set_mac_link_down(void)
{
    int i = 0;
    uint16 v16 = 0;
    uint8 v8 = 0;

    for (i = 0; i < EPHY_PORTS; i++) {
        if(!IsExtPhyId(switch_pport_phyid[i])) {
            ethsw_phy_rreg(switch_pport_phyid[i], MII_INTERRUPT, &v16);
            if ((pVnetDev0_g->EnetInfo[0].sw.port_map & (1U<<i)) != 0) {
                if (v16 & MII_INTR_LNK) {
                    ethsw_phy_rreg(switch_pport_phyid[i], MII_BMSR, &v16);
                    if (!(v16 & BMSR_LSTATUS)) {
                        ethsw_rreg(PAGE_CONTROL, REG_PORT_STATE + i, &v8, 1);
                        if (v8 & REG_PORT_STATE_LNK) {
                            v8 &= (~REG_PORT_STATE_LNK);
                            ethsw_wreg(PAGE_CONTROL, REG_PORT_STATE + i, &v8, 1);
                        }
                    }
                }
            }
        }
    }
}

PHY_STAT ethsw_phy_stat(int port)
{
    PHY_STAT ps;
    uint16 v16;
    uint16 ctrl;
    uint16 mii_esr = 0;
    uint16 mii_stat = 0, mii_adv = 0, mii_lpa = 0;
    uint16 mii_gb_ctrl = 0, mii_gb_stat = 0;
#if 0
    uint16 mii_gb_stat2 = 0;
    uint16 val_in = 8448;
#endif
    int phyId;
#if defined(CONFIG_BCM96816)
    int is6829 = 0;
    uint16 sw_port;
#endif

    ps.lnk = 0;
    ps.fdx = 0;
    ps.spd1000 = 0;
    ps.spd100 = 0;

#if defined(CONFIG_BCM96816)
    if ( IsExt6829(port) )
    {
       /* for the 6829 the port passed in is the physical port not a virtual port
          no guarantee that physical maps to virtual so get the phy id from board info */
       ETHERNET_SW_INFO *sw = &(((BcmEnet_devctrl *)netdev_priv(vnet_dev[0]))->EnetInfo[0].sw);

       sw_port = (uint16)(port & ~BCM_EXT_6829);
       phyId   = sw->phy_id[sw_port];
       if ( IsPhyConnected(phyId) )
       {
          phyId &= ~BCM_WAN_PORT;
          phyId  |= BCM_EXT_6829;
       }
       is6829  = 1;
    }
    else
    {
       sw_port = (uint16)switch_port_index[port];
       phyId   = switch_port_phyid[port];
    }
#else
    phyId  = switch_port_phyid[port];
#endif
    if ( !IsPhyConnected(phyId) )
    {
        // 0xff PHY ID means no PHY on this port.
        ps.lnk = 1;
        ps.fdx = 1;
        ps.spd1000 = 1;
        return ps;
    }

#if defined(CONFIG_BCM96816)
    if (!is6829)
#endif
    if (port_in_loopback_mode[switch_port_index[port]]) {
        return ps;
    }

    down(&bcm_ethlock_switch_config);

    ethsw_phy_rreg(phyId, MII_INTERRUPT, &v16);
    ethsw_phy_rreg(phyId, MII_ASR, &v16);
    BCM_ENET_DEBUG("%s mii_asr (reg 25) 0x%x\n", __FUNCTION__, v16);

#if 0
/*__ZyXEL__, Cj_Lai , Fixed the VMG1312/5313 LAN 4 port with 1000M NIC IOP issue */
	ethsw_phy_rreg(phyId, MII_STAT1000, &mii_gb_stat2);

	if(port == GPHY_PORT_ID){
		if(mii_gb_stat2 != 0){
			/*The val_in = 8448 is refer by BMCR_SPEED100 | BMCR_FULLDPLX in ethctl_cmd.c*/
			printk((KERN_CRIT "Gphy ID:%d , Fixed the LAN 4 port to speed BMCR_SPEED100 and BMCR_FULLDPLX.\n"), port);
			ethsw_phyport_wreg2(GPHY_PORT_PHY_ID,0,(uint16 *)&val_in, 0);
		}
	}
#endif
    if (!MII_ASR_LINK(v16)) {
        up(&bcm_ethlock_switch_config);
        return ps;
    }

    ps.lnk = 1;

    ethsw_phy_rreg(phyId, MII_BMCR, &ctrl);

    if (!MII_ASR_DONE(v16)) {
        if (ctrl & BMCR_ANENABLE) {
            up(&bcm_ethlock_switch_config);
            return ps;
        }
        ps.fdx = (ctrl & BMCR_FULLDPLX) ? 1 : 0;
        if((ctrl & BMCR_SPEED100) && !(ctrl & BMCR_SPEED1000))
            ps.spd100 = 1;
        else if(!(ctrl & BMCR_SPEED100) && (ctrl & BMCR_SPEED1000))
            ps.spd1000 = 1;

        up(&bcm_ethlock_switch_config);
        return ps;
    }

#ifdef CONFIG_BCM96368
    if ((!IsExtPhyId(phyId)) && MII_ASR_NOHCD(v16)) {
        ethsw_phy_rreg(phyId, MII_AENGSR, &ctrl);
        if (ctrl & MII_AENGSR_SPD) {
            ps.spd100 = 1;
        }
        if (ctrl & MII_AENGSR_DPLX) {
            ps.fdx = 1;
        }
        return ps;
    }
#endif

    //Auto neg enabled (this end) cases
    ethsw_phy_rreg(phyId, MII_ADVERTISE, &mii_adv);
    ethsw_phy_rreg(phyId, MII_LPA, &mii_lpa);
    ethsw_phy_rreg(phyId, MII_BMSR, &mii_stat);

    BCM_ENET_DEBUG("%s mii_adv 0x%x mii_lpa 0x%x mii_stat 0x%x mii_ctrl 0x%x \n", __FUNCTION__,
           mii_adv, mii_lpa, mii_stat, v16);
    // read 1000mb Phy  registers if supported
    if (mii_stat & BMSR_ESTATEN) { 

        ethsw_phy_rreg(phyId, MII_ESTATUS, &mii_esr);
        if (mii_esr & (1 << 15 | 1 << 14 |
                       ESTATUS_1000_TFULL | ESTATUS_1000_THALF))
            ethsw_phy_rreg(phyId, MII_CTRL1000, &mii_gb_ctrl);
            ethsw_phy_rreg(phyId, MII_STAT1000, &mii_gb_stat);
    }
    
    mii_adv &= mii_lpa;

    if ((mii_gb_ctrl & ADVERTISE_1000FULL) &&  // 1000mb Adv
            (mii_gb_stat & LPA_1000FULL))
    {
        ps.spd1000 = 1;
        ps.fdx = 1;
    } else if ((mii_gb_ctrl & ADVERTISE_1000HALF) && 
            (mii_gb_stat & LPA_1000HALF))
    {
        ps.spd1000 = 1;
        ps.fdx = 0;
    } else if (mii_adv & ADVERTISE_100FULL) {  // 100mb adv
        ps.spd100 = 1;
        ps.fdx = 1;
    } else if (mii_adv & ADVERTISE_100BASE4) {
        ps.spd100 = 1;
        ps.fdx = 0;
    } else if (mii_adv & ADVERTISE_100HALF) {
        ps.spd100 = 1;
        ps.fdx = 0;
    } else if (mii_adv & ADVERTISE_10FULL) {
        ps.fdx = 1;
    }

    up(&bcm_ethlock_switch_config);

    return ps;
}

#if (defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818)) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828)
void ethsw_phy_advertise_all(uint32 phy_id)
{
   uint16 v16;
   /* Advertise all speed & duplex combinations */
   /* Advertise 100BaseTX FD/HD and 10BaseT FD/HD */
   ethsw_phy_rreg(phy_id, MII_ADVERTISE, &v16);
   v16 |= AN_ADV_ALL;
   ethsw_phy_wreg(phy_id, MII_ADVERTISE, &v16);
   /* Advertise 1000BaseT FD/HD */
   ethsw_phy_rreg(phy_id, MII_CTRL1000, &v16);
   v16 |= AN_1000BASET_CTRL_ADV_ALL;
   ethsw_phy_wreg(phy_id, MII_CTRL1000, &v16);

   /* Enable Ethernet@Wirespeed which will loop through the different Advertisement
   when the link fails to negotiate */
   //add by KuanJung
   v16 = MII_REG_18_SEL(0x7);
   ethsw_phy_wreg(phy_id, MII_REGISTER_18, &v16);
   ethsw_phy_rreg(phy_id, MII_REGISTER_18, &v16);/* Enable Wirespeed */
   v16 |= RGMII_WIRESPEED_EN;
   v16 = MII_REG_18_WR(0x7, v16);
   ethsw_phy_wreg(phy_id, MII_REGISTER_18, &v16);

}
#endif

/* apply phy init board parameters for internal switch*/
void ethsw_phy_apply_init_bp(void)
{
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    unsigned int portmap, i, phy_id;
    bp_mdio_init_t* phyinit;
    uint16 data;

    portmap = pVnetDev0->EnetInfo[0].sw.port_map;
    for (i = 0; i < (TOTAL_SWITCH_PORTS - 1); i++) {
        if ((portmap & (1U<<i)) != 0) {
            phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[i];
            phyinit = pVnetDev0->EnetInfo[0].sw.phyinit[i];
            if( phyinit == 0 )
                continue;

            while(phyinit->u.op.op != BP_MDIO_INIT_OP_NULL)
            {
                if(phyinit->u.op.op == BP_MDIO_INIT_OP_WRITE)
                    ethsw_phy_wreg(phy_id, phyinit->u.write.reg, (uint16*)(&phyinit->u.write.data));
                else if(phyinit->u.op.op == BP_MDIO_INIT_OP_UPDATE)
                {
                    ethsw_phy_rreg(phy_id, phyinit->u.update.reg, &data);
                    data &= ~phyinit->u.update.mask;
                    data |= phyinit->u.update.data;
                    ethsw_phy_wreg(phy_id, phyinit->u.update.reg, &data);
                }
                phyinit++;
            }
        }
    }

}

void ethsw_phy_advertise_caps(void)
{
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    unsigned int portmap, i, phy_id;

    /* In some chips, the GPhys do not advertise all capabilities. So, fix it first */ 
#if defined(CONFIG_BCM963268)
    ethsw_phy_advertise_all(GPHY_PORT_PHY_ID);
#endif

#if defined(CONFIG_BCM96828)
    ethsw_phy_advertise_all(GPHY1_PORT_PHY_ID);
    ethsw_phy_advertise_all(GPHY2_PORT_PHY_ID);
#endif

    /* Now control advertising if boardparms says so */
    portmap = pVnetDev0->EnetInfo[0].sw.port_map;
    for (i = 0; i < (TOTAL_SWITCH_PORTS - 1); i++) {
        if ((portmap & (1U<<i)) != 0) {
            phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[i];
            if(IsPhyConnected(phy_id) && IsPhyAdvCapConfigValid(phy_id))
            {
                uint16 cap_mask = 0;

                ethsw_phy_rreg(phy_id, MII_ADVERTISE, &cap_mask);
                    cap_mask &= (~ADVERTISE_ALL);
                if (phy_id & ADVERTISE_10HD)
                    cap_mask |= ADVERTISE_10HALF;
                if (phy_id & ADVERTISE_10FD)
                    cap_mask |= ADVERTISE_10FULL;
                if (phy_id & ADVERTISE_100HD)
                    cap_mask |= ADVERTISE_100HALF;
                if (phy_id & ADVERTISE_100FD)
                    cap_mask |= ADVERTISE_100FULL;
printk("For port %d; changing the MII adv to 0x%x \n", (unsigned int)i, cap_mask);

                ethsw_phy_wreg(phy_id, MII_ADVERTISE, &cap_mask);

                ethsw_phy_rreg(phy_id, MII_CTRL1000, &cap_mask);
                cap_mask &= (~(ADVERTISE_1000HALF | ADVERTISE_1000FULL));
                if (phy_id & ADVERTISE_1000HD)
                    cap_mask |= ADVERTISE_1000HALF;
                if (phy_id & ADVERTISE_1000FD)
                    cap_mask |= ADVERTISE_1000FULL;
printk("changing the GMII adv to 0x%x \n", cap_mask);
                ethsw_phy_wreg(phy_id, MII_CTRL1000, &cap_mask);
            }
        }
    }

}

#if (defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818))
/* Setup the PHY with initial PHY config */
int ethsw_setup_phys(void)
{
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    unsigned int portmap, i, phy_id;
    uint16 v16;
    int is6829Present = 0;

    /* Get the portmap */
    portmap = pVnetDev0->EnetInfo[0].sw.port_map;

    /* For each port that has an internal or external PHY, configure it
       as per the required initial configuration */
    for (i = 0; i < (TOTAL_SWITCH_PORTS - 1); i++) {
        /* Check if the port is in the portmap or not */
        if ((portmap & (1U<<i)) != 0) {
            /* Check if the port is connected to a PHY or not */
            phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[i];
            /* If a Phy is connected, set it up with initial config */
            /* TBD: Maintain the config for each Phy */
            if(IsPhyConnected(phy_id))
            {
                if ( !IsExt6829(phy_id) )
                {
                    ethsw_phy_advertise_all(phy_id);
                }
                else
                {
                    /* Advertise all speed & duplex combinations */
                    /* Advertise 100BaseTX FD/HD and 10BaseT FD/HD */
                    int phyId6829;
                    int j;

                    phy_id &= ~(BCM_EXT_6829 | BCM_WAN_PORT);
                    for (j = 0; j < (TOTAL_SWITCH_PORTS - 1); j++)
                    {
                        if ((phy_id & (1U<<j)) != 0)
                        {
                            is6829Present = 1;
                            /* ids for 6829 match 6819 ids*/
                            phyId6829 = (pVnetDev0->EnetInfo[0].sw.phy_id[j] & ~BCM_WAN_PORT) | BCM_EXT_6829;
                            ethsw_phy_advertise_all(phyId6829);
                        }
                    }
                }
            }
        }
    }

    /* Initialize the RC calibration of the internal Phys to be able to
       pass the Ethernet Mask Measurements. The calibration for both the
       internal Phys is shared and configurable through the first Phy.
       The values written are as recommended by the Board Team */
    phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[0];
    v16 = 0x0F96;
    ethsw_phy_wreg(phy_id, MII_DSP_COEFF_ADDR, &v16);
    v16 = 0x000C;
    ethsw_phy_wreg(phy_id, MII_DSP_COEFF_RW_PORT, &v16);
    v16 = 0x0000;
    ethsw_phy_wreg(phy_id, MII_DSP_COEFF_RW_PORT, &v16);

    if ( 1 == is6829Present )
    {
       phy_id |= BCM_EXT_6829;
       v16 = 0x0F96;
       ethsw_phy_wreg(phy_id, MII_DSP_COEFF_ADDR, &v16);
       v16 = 0x000C;
       ethsw_phy_wreg(phy_id, MII_DSP_COEFF_RW_PORT, &v16);
       v16 = 0x0000;
       ethsw_phy_wreg(phy_id, MII_DSP_COEFF_RW_PORT, &v16);
    }
    return 0;
}
#elif defined(CONFIG_BCM96828)
int ethsw_setup_phys(void)
{
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    unsigned int phy_id, portmap;
    uint16 v16;


    /* Get the portmap */
    portmap = pVnetDev0->EnetInfo[0].sw.port_map;

    /* Reset the RC calibration of the internal G-Phys to improve the return loss in 10BT.
       The calibration for both the internal G-Phys is shared and configurable through the first Phy.
       The reset is recommended by HW Team : JIRA#SWBCACPE-10270 */
    if ((portmap & (1U<<GPHY1_PORT_ID)) != 0) {
        phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[GPHY1_PORT_ID];
        v16 = 0x0FB0; /* Expansion register 0xB0 */
        ethsw_phy_wreg(phy_id, MII_DSP_COEFF_ADDR, &v16);
        /* Read the current value */
        ethsw_phy_rreg(phy_id, MII_DSP_COEFF_RW_PORT, &v16);
        v16 |= 0x04; /* Set Reset Bit[2] */
        ethsw_phy_wreg(phy_id, MII_DSP_COEFF_RW_PORT, &v16);
    }
    return 0;
}
#else
int ethsw_setup_phys(void)
{
    return 0;
}
#endif

#if (defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818))
#define NUM_INT_EPHYS 0
#define NUM_INT_GPHYS 2
#define DEFAULT_BASE_PHYID 0
#elif defined(CONFIG_BCM963268)
#define NUM_INT_EPHYS 3
#define NUM_INT_GPHYS 1
#define DEFAULT_BASE_PHYID 1
#elif defined(CONFIG_BCM96828)
#define NUM_INT_EPHYS 2
#define NUM_INT_GPHYS 2
#define DEFAULT_BASE_PHYID 1
#else
#define NUM_INT_EPHYS 4
#define NUM_INT_GPHYS 0
#define DEFAULT_BASE_PHYID 1
#endif
#define NUM_INT_PHYS (NUM_INT_EPHYS + NUM_INT_GPHYS)

#if defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE)
void ethsw_setup_hw_apd(unsigned int enable)
{
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    unsigned int phy_id, i;
    uint16 v16;

    down(&bcm_ethlock_switch_config);

    /* For each configured external PHY, enable/disable APD */
    for (i = 0; i < (TOTAL_SWITCH_PORTS - 1); i++) {
        /* Check if the port is in the portmap or not */
        if ((pVnetDev0->EnetInfo[0].sw.port_map & (1U<<i)) != 0) {
            /* Check if the port is connected to a PHY or not */
            phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[i];
            /* If a Phy is connected, and is external, set APD */
            if(IsPhyConnected(phy_id) && IsExtPhyId(phy_id)) {
                /* Read MII Phy Identifier, process the AC201 differently */
                ethsw_phy_rreg(phy_id, MII_PHYSID2, &v16);
                if ((v16 & BCM_PHYID_M) == (BCMAC201_PHYID2 & BCM_PHYID_M)) {
                    v16 = 0x008B;
                    ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
                    v16 = 0x7001;
                    if (enable) {
                        v16 |= MII_1C_AUTO_POWER_DOWN;
                    }
                    ethsw_phy_wreg(phy_id, 0x1b, &v16);
                    v16 = 0x000B;
                    ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
                } else if ((v16 & BCM_PHYID_M) != (BCM54610_PHYID2 & BCM_PHYID_M)) {
                    /* Other PHYs */
                    v16 = MII_1C_WRITE_ENABLE | MII_1C_AUTO_POWER_DOWN_SV |
                      MII_1C_WAKEUP_TIMER_SEL_84 | MII_1C_APD_COMPATIBILITY;
                    if (enable) {
                        v16 |= MII_1C_AUTO_POWER_DOWN;
                    }
                    ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);
                }
            }
        }
    }

    /* For each internal PHY (including those not in boardparms), enable/disable APD */
#if NUM_INT_EPHYS > 0
    /* EPHYs */
    for (i = DEFAULT_BASE_PHYID; i < NUM_INT_EPHYS+DEFAULT_BASE_PHYID; i++) {
        v16 = 0x008B;
        ethsw_phy_wreg(i, MII_BRCM_TEST, &v16);
        v16 = 0x7001;
        if (enable) {
            v16 |= MII_1C_AUTO_POWER_DOWN;
        }
        ethsw_phy_wreg(i, 0x1b, &v16);
        v16 = 0x000B;
        ethsw_phy_wreg(i, MII_BRCM_TEST, &v16);
    }
#endif

#if NUM_INT_GPHYS > 0
    /* GPHYs */
    for (i = NUM_INT_EPHYS+DEFAULT_BASE_PHYID; i < NUM_INT_GPHYS+NUM_INT_EPHYS+DEFAULT_BASE_PHYID; i++) {
        v16 = MII_1C_WRITE_ENABLE | MII_1C_AUTO_POWER_DOWN_SV |
              MII_1C_WAKEUP_TIMER_SEL_84 | MII_1C_APD_COMPATIBILITY;
        if (enable) {
            v16 |= MII_1C_AUTO_POWER_DOWN;
        }
        ethsw_phy_wreg(i, MII_REGISTER_1C, &v16);
    }
#endif

    up(&bcm_ethlock_switch_config);
}
#endif

static uint32 ephy_forced_pwr_down_status = 0;
#if defined(CONFIG_BCM_ETH_PWRSAVE)
unsigned int  eth_auto_power_down_enabled = 1;
unsigned int  eth_eee_enabled = 1;
#else
unsigned int  eth_auto_power_down_enabled = 0;
unsigned int  eth_eee_enabled = 0;
#endif

#if defined(CONFIG_BCM_ETH_PWRSAVE)
/* Delay in miliseconds after enabling the EPHY PLL before reading the different EPHY status      */
/* The PLL requires 400 uSec to stabilize, but Energy detection on the ports requires more time. */
/* Normally, Energy detection works when PLL is down, but a long delay (minutes) is present     */
/* for ports that are in Auto Power Down mode. 40 mSec is chosen because this is the delay      */
/* that allows EPHY to send two link pulses (or series of pulses) at 16 mSec interval                  */
#define PHY_PLL_ENABLE_DELAY 1
#define PHY_PORT_ENABLE_DELAY 40

/* Describe internal PHYs */
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328) || defined(CONFIG_BCM96318)
static uint64 ephy_energy_det[NUM_INT_PHYS] = {1<<(INTERRUPT_ID_EPHY_ENERGY_0-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_EPHY_ENERGY_1-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_EPHY_ENERGY_2-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_EPHY_ENERGY_3-INTERNAL_ISR_TABLE_OFFSET)};
											   
#if !defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE)
static uint32 mdix_manual_swap = 0;
#endif

#elif (defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818))
static uint64 ephy_energy_det[NUM_INT_PHYS] = {((uint64)1)<<(INTERRUPT_ID_EPHY_ENERGY_0-INTERNAL_ISR_TABLE_OFFSET),
                                               ((uint64)1)<<(INTERRUPT_ID_EPHY_ENERGY_1-INTERNAL_ISR_TABLE_OFFSET)};
static uint32 gphy_pwr_dwn[NUM_INT_GPHYS] =   {GPHY_PWR_DOWN_0, GPHY_PWR_DOWN_1};
#define ROBOSWGPHYCTRL RoboswEphyCtrl
#undef PHY_PORT_ENABLE_DELAY
#define PHY_PORT_ENABLE_DELAY 300 // GPHY on 6816 need much more time to link when connecting one to the other

#elif defined(CONFIG_BCM963268)
static uint64 ephy_energy_det[NUM_INT_PHYS] = {1<<(INTERRUPT_ID_EPHY_ENERGY_0-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_EPHY_ENERGY_1-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_EPHY_ENERGY_2-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_GPHY_ENERGY_0-INTERNAL_ISR_TABLE_OFFSET)};
static uint32 gphy_pwr_dwn[NUM_INT_GPHYS] =   {GPHY_LOW_PWR};
#define ROBOSWGPHYCTRL RoboswGphyCtrl

#elif defined(CONFIG_BCM96828)
static uint64 ephy_energy_det[NUM_INT_PHYS] = {1<<(INTERRUPT_ID_EPHY_ENERGY_0-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_EPHY_ENERGY_1-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_GPHY_ENERGY_0-INTERNAL_ISR_TABLE_OFFSET),
                                               1<<(INTERRUPT_ID_GPHY_ENERGY_1-INTERNAL_ISR_TABLE_OFFSET)};
static uint32 gphy_pwr_dwn[NUM_INT_GPHYS] =   {GPHY1_LOW_PWR, GPHY2_LOW_PWR};
#define ROBOSWGPHYCTRL RoboswGphyCtrl
#else
#error /* Add definitions for new chips */
#endif

#if !defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE)
static uint32 ephy_pwr_down_status = 0;
#endif
extern int vport_cnt;  /* number of vports: bitcount of Enetinfo.sw.port_map */
static void ethsw_eee_all_enable(int enable);

void BcmPwrMngtSetEthAutoPwrDwn(unsigned int enable)
{
   eth_auto_power_down_enabled = enable;
#if defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE)
   ethsw_setup_hw_apd(enable);
#endif

   printk("Ethernet Auto Power Down and Sleep is %s\n", enable?"enabled":"disabled");
}
EXPORT_SYMBOL(BcmPwrMngtSetEthAutoPwrDwn);

int BcmPwrMngtGetEthAutoPwrDwn(void)
{
   return (eth_auto_power_down_enabled);
}
EXPORT_SYMBOL(BcmPwrMngtGetEthAutoPwrDwn);

void BcmPwrMngtSetEnergyEfficientEthernetEn(unsigned int enable)
{
   eth_eee_enabled = enable;
   ethsw_eee_all_enable(enable);

   printk("Energy Efficient Ethernet is %s\n", enable?"enabled":"disabled");
}
EXPORT_SYMBOL(BcmPwrMngtSetEnergyEfficientEthernetEn);

int BcmPwrMngtGetEnergyEfficientEthernetEn(void)
{
   return (eth_eee_enabled);
}
EXPORT_SYMBOL(BcmPwrMngtGetEnergyEfficientEthernetEn);

int ethsw_phy_pll_up(int ephy_and_gphy)
{
    int ephy_status_changed = 0;

#if NUM_INT_GPHYS > 0
    if (ephy_and_gphy)
    {
        int i;
        uint32 roboswGphyCtrl = GPIO->ROBOSWGPHYCTRL;

        /* Bring up internal GPHY PLLs if they are down */
        for (i = 0; i < NUM_INT_GPHYS; i++)
        {
            if ((roboswGphyCtrl & gphy_pwr_dwn[i]) && !(ephy_forced_pwr_down_status & (1<<(i+NUM_INT_EPHYS))))
            {
                roboswGphyCtrl &= ~gphy_pwr_dwn[i];
                ephy_status_changed = 1;
            }
        }
        if (ephy_status_changed) {
            GPIO->ROBOSWGPHYCTRL = roboswGphyCtrl;
        }
    }
#endif

    /* This is a safety measure in case one tries to access the EPHY */
    /* while the PLL/RoboSw is powered down */
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
    if (GPIO->RoboswEphyCtrl & EPHY_PWR_DOWN_DLL)
    {
        /* Wait for PLL to stabilize before reading EPHY registers */
        GPIO->RoboswEphyCtrl &= ~EPHY_PWR_DOWN_DLL;
        ephy_status_changed = 1;
    }
#elif defined(ROBOSW250_CLK_EN)
    if (!(PERF->blkEnables & ROBOSW250_CLK_EN))
    {
        /* Enable robosw clock */
#if defined(ROBOSW025_CLK_EN)
        PERF->blkEnables |= ROBOSW250_CLK_EN | ROBOSW025_CLK_EN;
#else
        PERF->blkEnables |= ROBOSW250_CLK_EN;
#endif
        ephy_status_changed = 1;
    }
#endif

    if (ephy_status_changed) {
        if (irqs_disabled() || (preempt_count() != 0)) {
            mdelay(PHY_PLL_ENABLE_DELAY);
        } else {
            msleep(PHY_PLL_ENABLE_DELAY);
        }
        return (msecs_to_jiffies(PHY_PLL_ENABLE_DELAY));
    }
    return 0;
}

uint32 ethsw_ephy_auto_power_down_wakeup(void)
{
#if !defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE)
    int phy_id;
    int ephy_sleep_delay = 0;
    int ephy_status_changed = 0;
    int i;
    uint16 v16;
    BcmEnet_devctrl *priv = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);

    /* Ensure that only this thread accesses PHY registers in this interval */
    down(&bcm_ethlock_switch_config);
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
   /* Prevent acceses to EPHY registers while in shadow mode */
    atomic_inc(&phy_write_ref_cnt);
    atomic_inc(&phy_read_ref_cnt);
    BcmHalInterruptDisable(INTERRUPT_ID_EPHY);
#endif

    /* Make sure EPHY PLL is up */
    ephy_sleep_delay = ethsw_phy_pll_up(1);

#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
    /* Update counter to toggle cross-over cable detection */
    mdix_manual_swap++;
#endif

    /* Make sure all PHY Ports are up */
    for (i = 0; i < NUM_INT_PHYS; i++)
    {
        if (ephy_pwr_down_status & (1<<i) && !(ephy_forced_pwr_down_status & (1<<(i))))
        {
#if NUM_INT_GPHYS > 0
            if (i >= NUM_INT_EPHYS)
            {
                // This GPHY port was down
                // Toggle pwr down bit, register 0, bit 11
                // if it was not already down, otherwise leave it down
                phy_id = priv->EnetInfo[0].sw.phy_id[i];
                ethsw_phy_rreg(phy_id, MII_CONTROL, &v16);
                if (!(v16 & MII_CONTROL_POWER_DOWN)) {
                    v16 |= MII_CONTROL_POWER_DOWN;
                    ethsw_phy_wreg(phy_id, MII_CONTROL, &v16);
                    v16 &= ~MII_CONTROL_POWER_DOWN;
                    ethsw_phy_wreg(phy_id, MII_CONTROL, &v16);
                    ephy_status_changed = 1;
                    ephy_sleep_delay += 3;
                }
                ephy_pwr_down_status &= ~(1<<i);
            }
#endif
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
            /* The following is a SW implementation of APD. It is preferable now to use HW APD when available */
            {
                /* This EPHY port is down, bring it up */
                phy_id = priv->EnetInfo[0].sw.phy_id[i];
                v16 = 0x008B;
                ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
#if defined(CONFIG_BCM96368)
                /* 6368 EPHYs are finicky and re-enabling Rx sometimes */
                /* takes too long, so we only do Tx */
                /* Enable Rx first */
                /* v16 = 0x0000; */
                /* ethsw_phy_wreg(phy_id, 0x14, &v16); */
                /* Then Tx */
                v16 = 0x0000;
                ethsw_phy_wreg(phy_id, 0x10, &v16);
#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
                /* Enable Rx first */
                v16 = 0x0008;
                ethsw_phy_wreg(phy_id, 0x14, &v16);
                /* Then Tx */
                v16 = 0x0400;
                ethsw_phy_wreg(phy_id, 0x10, &v16);
                ephy_sleep_delay += 1;
#endif
                v16 = 0x000B;
                ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
                ephy_pwr_down_status &= ~(1<<i);
                ephy_status_changed = 1;
                ephy_sleep_delay += 3;

                /* If the port has no energy, swap MDIX control */
                /* (cross-over/straight-through support) every second */
                if (!(PERF->IrqControl[0].IrqStatus & ephy_energy_det[i]))
                {
                    ethsw_phy_rreg(phy_id, 0x1c, &v16);
                    v16 |= 0x0800 | (mdix_manual_swap&1)?0x1000:0x0000;
                    ethsw_phy_wreg(phy_id, 0x1c, &v16);
                    ephy_sleep_delay += 2;
                }
           }
#endif
        }
    }

#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
    atomic_dec(&phy_write_ref_cnt);
    atomic_dec(&phy_read_ref_cnt);
    BcmHalInterruptEnable(INTERRUPT_ID_EPHY);
#endif
    up(&bcm_ethlock_switch_config);

    if (ephy_status_changed)
    {
        /* Allow the ports to be enabled and transmitting link pulses */
        msleep(PHY_PORT_ENABLE_DELAY);
        ephy_sleep_delay += msecs_to_jiffies(PHY_PORT_ENABLE_DELAY);
    }

    return ephy_sleep_delay;
#else
    return ethsw_phy_pll_up(1);
#endif
}

uint32 ethsw_ephy_auto_power_down_sleep(void)
{
    int i;
    int ephy_sleep_delay = 0;
    uint64 irqStatus = PERF->IrqControl[0].IrqStatus;
    int ephy_has_energy = 0;
    BcmEnet_devctrl *priv = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    int map = priv->EnetInfo[0].sw.port_map;
    int phy_id;
    uint16 v16;

    if (!eth_auto_power_down_enabled)
    {
        return ephy_sleep_delay;
    }

    /* Ensure that only this thread accesses PHY registers in this interval */
    down(&bcm_ethlock_switch_config);
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
   /* Prevent acceses to EPHY registers while in shadow mode */
    atomic_inc(&phy_write_ref_cnt);
    atomic_inc(&phy_read_ref_cnt);
    BcmHalInterruptDisable(INTERRUPT_ID_EPHY);
#endif

    /* Turn off EPHY Ports that have no energy */
    for (i = 0; i < NUM_INT_PHYS; i++)
    {
        if (map & (1<<i))
        {
            /* Verify if the link is down, don't want to force down while the link is still up */
            phy_id = priv->EnetInfo[0].sw.phy_id[i];
            ethsw_phy_rreg(phy_id, 0x1, &v16);
            if (!(irqStatus & ephy_energy_det[i]) || (!(v16&0x4) && (ephy_forced_pwr_down_status & (1<<(i)))))
            {
#if !defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE) && NUM_INT_GPHYS > 0
                if (i >= NUM_INT_EPHYS)
                {
                    /* This GPHY port has no energy, bring it down */
                    GPIO->ROBOSWGPHYCTRL |= gphy_pwr_dwn[i-NUM_INT_EPHYS];
                    ephy_pwr_down_status |= (1<<i);
                }
#endif
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
                /* The following is a SW implementation of APD. It is preferable now to use HW APD when available */
                {
                    /* This EPHY port has no energy, bring it down */
                    v16 = 0x008B;
                    ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
#if defined(CONFIG_BCM96368)
                    /* 6368 EPHYs are finicky and re-enabling Rx sometimes */
                    /* takes too long, so we only do Tx */
                    v16 = 0x01c0;
                    ethsw_phy_wreg(phy_id, 0x10, &v16);
                    /* v16 = 0x7000; */
                    /* ethsw_phy_wreg(phy_id, 0x14, &v16); */
#elif defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
                    v16 = 0x0700;
                    ethsw_phy_wreg(phy_id, 0x10, &v16);
                    v16 = 0x6008;
                    ethsw_phy_wreg(phy_id, 0x14, &v16);
                    ephy_sleep_delay += 1;
#endif
                    v16 = 0x000B;
                    ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
                    ephy_pwr_down_status |= (1<<i);
                    ephy_sleep_delay += 3;
                }
#endif
            }
            else
            {
                ephy_has_energy = 1;
            }
        }
    }

#if !defined(CONFIG_BCM96816)
    if (priv->extSwitch->brcm_tag_type != BRCM_TYPE2) {
        /* If no energy was found on any PHY and no other switch port is linked, bring down PLL to save power */
        if (!ephy_has_energy && !priv->linkState)
        {
#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
            GPIO->RoboswEphyCtrl |= EPHY_PWR_DOWN_DLL;
#elif defined(ROBOSW025_CLK_EN)
            PERF->blkEnables &= ~(ROBOSW250_CLK_EN | ROBOSW025_CLK_EN);
#elif defined(ROBOSW250_CLK_EN)
            PERF->blkEnables &= ~ROBOSW250_CLK_EN;
#endif
        }
    }
#endif

#if defined(CONFIG_BCM96368) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM96328)
    atomic_dec(&phy_write_ref_cnt);
    atomic_dec(&phy_read_ref_cnt);
    BcmHalInterruptEnable(INTERRUPT_ID_EPHY);
#endif
    up(&bcm_ethlock_switch_config);

    return ephy_sleep_delay;
}
#endif

void ethsw_switch_power_off(void *context)
{
#ifdef DYING_GASP_API
    enet_send_dying_gasp_pkt();
#endif
}

void ethsw_init_config(void)
{
    int i;

    /* Save the state that is restored in enable_hw_switching */
    for(i = 0; i < TOTAL_SWITCH_PORTS; i++)  {
        ethsw_rreg(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (i * 2),
                   (uint8 *)&pbvlan_map[i], 2);
    }
    ethsw_rreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&dis_learning, 2);
    ethsw_rreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&port_fwd_ctrl, 1);

#if defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828)
    {
        /* Disable tags for internal switch ports */
        uint32 tmp;
        ethsw_rreg(PAGE_CONTROL, REG_IUDMA_CTRL, (uint8_t *)&tmp, 4);
        tmp |= REG_IUDMA_CTRL_TX_MII_TAG_DISABLE;
        ethsw_wreg(PAGE_CONTROL, REG_IUDMA_CTRL, (uint8_t *)&tmp, 4); 
    }
#endif

}

int ethsw_setup_led(void)
{
    BcmEnet_devctrl *pVnetDev0 = (BcmEnet_devctrl *) netdev_priv(vnet_dev[0]);
    unsigned int phy_id, i;
    uint16 v16;

    /* For each port that has an internal or external PHY, configure it
       as per the required initial configuration */
    for (i = 0; i < (TOTAL_SWITCH_PORTS - 1); i++) {
        /* Check if the port is in the portmap or not */
        if ((pVnetDev0->EnetInfo[0].sw.port_map & (1U<<i)) != 0) {
            /* Check if the port is connected to a PHY or not */
            phy_id = pVnetDev0->EnetInfo[0].sw.phy_id[i];
            /* If a Phy is connected, set it up with initial config */
            /* TBD: Maintain the config for each Phy */
            if(IsPhyConnected(phy_id) && !IsExtPhyId(phy_id)) {
#if defined(CONFIG_BCM96368)
                v16 = 1 << 2;
                ethsw_phy_wreg(phy_id, MII_TPISTATUS, &v16);
#elif (defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818))
                v16 = 0xa410;
                ethsw_phy_wreg(phy_id, 0x1c, &v16);
#elif defined(CONFIG_BCM96328) || defined(CONFIG_BCM96362) || defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828) || defined(CONFIG_BCM96318)
                v16 = 0xa410;
                // Enable Shadow register 2
                ethsw_phy_rreg(phy_id, MII_BRCM_TEST, &v16);
                v16 |= MII_BRCM_TEST_SHADOW2_ENABLE;
                ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);

#if defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828)
#if defined(CONFIG_BCM963268)
                if (i != GPHY_PORT_ID) 
#else
                if ((i != GPHY1_PORT_ID) && (i != GPHY2_PORT_ID))
#endif
                {
                    // Set LED1 to speed. Set LED0 to blinky link
                    v16 = 0x08;
                }
#else
                // Set LED0 to speed. Set LED1 to blinky link
                v16 = 0x71;
#endif
                ethsw_phy_wreg(phy_id, 0x15, &v16);
                // Disable Shadow register 2
                ethsw_phy_rreg(phy_id, MII_BRCM_TEST, &v16);
                v16 &= ~MII_BRCM_TEST_SHADOW2_ENABLE;
                ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
#endif
            }
#if defined(CONFIG_BCM96816)
            if(IsExt6829(phy_id))
            {
                int phyId6829;
                int j;

                phy_id &= ~(BCM_EXT_6829 | BCM_WAN_PORT);
                for ( j = 0; j < (TOTAL_SWITCH_PORTS - 1); j++ )
                {
                    if ((phy_id & (1U<<j)) != 0)
                    {
                        /* ids for 6829 match 6819 ids*/
                        phyId6829 = pVnetDev0->EnetInfo[0].sw.phy_id[j] & ~BCM_WAN_PORT;
                        if ( !IsExtPhyId(phyId6829) )
                        {
                            phyId6829 |= BCM_EXT_6829;
                            v16        = 0xa410;
                            ethsw_phy_wreg(phyId6829, 0x1c, &v16);
                        }
                    }
                }
            }
#endif
            if (IsExtPhyId(phy_id)) {
                /* Configure LED for link/activity */
                v16 = MII_1C_SHADOW_LED_CONTROL << MII_1C_SHADOW_REG_SEL_S;
                ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);
                ethsw_phy_rreg(phy_id, MII_REGISTER_1C, &v16);
                v16 |= ACT_LINK_LED_ENABLE;
                v16 |= MII_1C_WRITE_ENABLE;
                v16 &= ~(MII_1C_SHADOW_REG_SEL_M << MII_1C_SHADOW_REG_SEL_S);
                v16 |= (MII_1C_SHADOW_LED_CONTROL << MII_1C_SHADOW_REG_SEL_S);
                ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);

                ethsw_phy_rreg(phy_id, MII_PHYSID2, &v16);
                if ((v16 & BCM_PHYID_M) == (BCM54610_PHYID2 & BCM_PHYID_M)) {
                    /* Configure LOM LED Mode */
                    v16 = MII_1C_EXTERNAL_CONTROL_1 << MII_1C_SHADOW_REG_SEL_S;
                    ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);
                    ethsw_phy_rreg(phy_id, MII_REGISTER_1C, &v16);
                    v16 |= LOM_LED_MODE;
                    v16 |= MII_1C_WRITE_ENABLE;
                    v16 &= ~(MII_1C_SHADOW_REG_SEL_M << MII_1C_SHADOW_REG_SEL_S);
                    v16 |= (MII_1C_EXTERNAL_CONTROL_1 << MII_1C_SHADOW_REG_SEL_S);
                    ethsw_phy_wreg(phy_id, MII_REGISTER_1C, &v16);
                }
            }
        }
    }
    return 0;
}

int ethsw_reset_ports(struct net_device *dev)
{
    BcmEnet_devctrl *pDevCtrl = netdev_priv(dev);
    int map, cnt, i;
    uint16 v16, phy_identifier;
    int phyid;
    uint8 v8;

    ethsw_init_table(&pDevCtrl->EnetInfo[0].sw);

    if (pDevCtrl->unit == 1) {
        for (i = 0; i < TOTAL_SWITCH_PORTS-1; i++)
        {
            ext_switch_pport_phyid[i] = pDevCtrl->EnetInfo[1].sw.phy_id[i];
        }
    }

    map = pDevCtrl->EnetInfo[0].sw.port_map;
    bitcount(cnt, map);

    if (cnt <= 0)
        return 0;

    // MAC level reset
    for (i = 0; i < 8; i++)
    {
#if defined(CONFIG_BCM96816)
        int phy_id = pDevCtrl->EnetInfo[0].sw.phy_id[i];
        if(IsExt6829(phy_id))
        {
            int j;
            phy_id &= ~(BCM_EXT_6829 | BCM_WAN_PORT);
            for ( j = 0; j < (TOTAL_SWITCH_PORTS - 1); j++ )
            {
                v8 = ((phy_id & (1U<<j)) != 0) ? 0: REG_PORT_CTRL_DISABLE;
                if ( j == SERDES_PORT_ID )
                {
                   /* make sure 6829 SERDES port stays active */
                   v8 = 0;
                }
                ethsw_wreg_ext(PAGE_CONTROL, REG_PORT_CTRL + j, &v8, 1, 1);
            }
        }
#endif
    }

#if defined(CONFIG_BCM963268) || defined(CONFIG_BCM_6362_PORTS_INT_EXT_SW)
    if (map & (1 << (RGMII_PORT_ID + 1))) {
        GPIO->RoboswSwitchCtrl |= (RSW_MII_2_IFC_EN | (RSW_MII_SEL_2P5V << RSW_MII_2_SEL_SHIFT));
    }
#endif

    for (i = 0; i < NUM_RGMII_PORTS; i++) {

        phyid = pDevCtrl->EnetInfo[0].sw.phy_id[RGMII_PORT_ID + i];
        ethsw_phy_rreg(phyid, MII_PHYSID2, &phy_identifier);

        ethsw_rreg(PAGE_CONTROL, REG_RGMII_CTRL_P4 + i, &v8, 1);
#if defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828)
        v8 |= REG_RGMII_CTRL_ENABLE_RGMII_OVERRIDE;
        v8 &= ~REG_RGMII_CTRL_MODE;
        if (IsRGMII(phyid)) {
            v8 |= REG_RGMII_CTRL_MODE_RGMII;
        } else if (IsRvMII(phyid)) {
            v8 |= REG_RGMII_CTRL_MODE_RvMII;
        } else if (IsGMII(phyid)) {
            v8 |= REG_RGMII_CTRL_MODE_GMII;
        } else {
            v8 |= REG_RGMII_CTRL_MODE_MII;
        }
#endif

#if defined(CONFIG_BCM963268)
        if ((pDevCtrl->chipRev == 0xA0) || (pDevCtrl->chipRev == 0xB0)) {
            /* RGMII timing workaround */
            v8 &= ~REG_RGMII_CTRL_TIMING_SEL;
        } 
        else 
#endif    
        {

            v8 |= REG_RGMII_CTRL_TIMING_SEL;
        }
        /* Enable Clock delay in RX */
        if ((phy_identifier & BCM_PHYID_M) == (BCM54616_PHYID2 & BCM_PHYID_M)) {
            v8 |= REG_RGMII_CTRL_DLL_RXC_BYPASS;
        }

        ethsw_wreg(PAGE_CONTROL, REG_RGMII_CTRL_P4 + i, &v8, 1);

#if defined(CONFIG_BCM963268)
        if ((pDevCtrl->chipRev == 0xA0) || (pDevCtrl->chipRev == 0xB0)) {
            /* RGMII timing workaround */
            v8 = 0xAB;
            ethsw_wreg(PAGE_CONTROL, REG_RGMII_TIMING_P4 + i, &v8, 1);
        }
#endif

        /* No need to check the PhyID if the board params is set correctly for RGMII. However, keeping
              *   the phy id check to make it work even when customer does not set the RGMII flag in the phy_id
              *   in board params
              */
        if ((IsRGMII(phyid) && IsPhyConnected(phyid)) ||
            ((phy_identifier & BCM_PHYID_M) == (BCM54610_PHYID2 & BCM_PHYID_M)) ||
            ((phy_identifier & BCM_PHYID_M) == (BCM50612_PHYID2 & BCM_PHYID_M))) {

            v16 = MII_1C_SHADOW_CLK_ALIGN_CTRL << MII_1C_SHADOW_REG_SEL_S;
            ethsw_phy_wreg(phyid, MII_REGISTER_1C, &v16);
            ethsw_phy_rreg(phyid, MII_REGISTER_1C, &v16);
#if defined(CONFIG_BCM963268)
            /* Temporary work-around for MII2 port RGMII delay programming */
            if (i == 1 && ((pDevCtrl->chipRev == 0xA0) || (pDevCtrl->chipRev == 0xB0)) )
                v16 |= GTXCLK_DELAY_BYPASS_DISABLE;
            else
#endif
                v16 &= (~GTXCLK_DELAY_BYPASS_DISABLE);
            v16 |= MII_1C_WRITE_ENABLE;
            v16 &= ~(MII_1C_SHADOW_REG_SEL_M << MII_1C_SHADOW_REG_SEL_S);
            v16 |= (MII_1C_SHADOW_CLK_ALIGN_CTRL << MII_1C_SHADOW_REG_SEL_S);
            ethsw_phy_wreg(phyid, MII_REGISTER_1C, &v16);
            if ((phy_identifier & BCM_PHYID_M) == (BCM54616_PHYID2 & BCM_PHYID_M)) {
                v16 = MII_REG_18_SEL(0x7);
                ethsw_phy_wreg(phyid, MII_REGISTER_18, &v16);
                ethsw_phy_rreg(phyid, MII_REGISTER_18, &v16);
                /* Disable Skew */
                v16 &= (~RGMII_RXD_TO_RXC_SKEW);
                v16 = MII_REG_18_WR(0x7,v16);
                ethsw_phy_wreg(phyid, MII_REGISTER_18, &v16);
            }
        }
    }
#if defined(CONFIG_BCM96828)
    ethsw_rreg(PAGE_CONTROL, REG_RGMII_CTRL_P7, &v8, 1);
    v8 |= REG_RGMII_CTRL_ENABLE_GMII;
    ethsw_wreg(PAGE_CONTROL, REG_RGMII_CTRL_P7, &v8, 1);
#endif

    /*Remaining port reset functionality is moved into ethsw_init_hw*/

    return 0;
}

#ifdef NO_CFE
void ethsw_configure_ports(int port_map, int *pphy_id)
{
    uint8 v8;
    int i, phy_id;

    for (i = 0; i < 8; i++) {
        v8 = REG_PORT_CTRL_DISABLE;
        ethsw_wreg_ext(PAGE_CONTROL, REG_PORT_CTRL + i, &v8, 1, 0);

        if ((port_map & (1U<<i)) != 0) {
#if defined(CONFIG_BCM96368)
            if (i == 4)
                GPIO->GPIOBaseMode |= (EN_GMII1);
            if (i == 5)
                GPIO->GPIOBaseMode |= (EN_GMII2);
#elif (defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818))
            if (i == 2)
                GPIO->GPIOBaseMode |= (EN_GMII1);
            if (i == 3)
                GPIO->GPIOBaseMode |= (EN_GMII2);
#elif defined(CONFIG_BCM96362)
            if (i == 4)
                GPIO->RoboswSwitchCtrl |= (RSW_MII_SEL_2P5V << RSW_MII_SEL_SHIFT);
            if (i == 5)
                GPIO->RoboswSwitchCtrl |= (RSW_MII_2_IFC_EN | (RSW_MII_SEL_2P5V << RSW_MII_2_SEL_SHIFT));
#elif defined(CONFIG_BCM96328)
            if (i == 4)
                MISC->miscPadCtrlHigh |= (MISC_MII_SEL_2P5V << MISC_MII_SEL_SHIFT);
#elif defined(CONFIG_BCM96318)
            if (i==4)
            {
                MISC->miscSpecialPadControl &= ~RGMII_PAD_SEL_MASK;
                MISC->miscSpecialPadControl |= (RGMII_PAD_SEL_2_5V << RGMII_PAD_SEL_SHIFT);
            }

#endif
        }
#if (defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818))
        else {
            phy_id = *(pphy_id + i);
            if (!IsExtPhyId(phy_id)) {
                if (i == 0)
                    GPIO->RoboswEphyCtrl |= (GPHY_PWR_DOWN_0 | GPHY_PWR_DOWN_SD_0);
                if (i == 1)
                    GPIO->RoboswEphyCtrl |= (GPHY_PWR_DOWN_1 | GPHY_PWR_DOWN_SD_1);
            }
        }
#endif
    }

    v8 = LINK_OVERRIDE_1000FDX;
    ethsw_wreg_ext(PAGE_CONTROL, REG_CONTROL_MII1_PORT_STATE_OVERRIDE, &v8, 1, 0);
}
#endif


static uint8 swdata[16];
static uint8 miidata[16];

int ethsw_add_proc_files(struct net_device *dev)
{
    struct proc_dir_entry *p;

    p = create_proc_entry("switch", 0644, NULL);

    if (p == NULL)
        return -1;

    memset(swdata, 0, sizeof(swdata));

    p->data        = dev;
    p->read_proc   = proc_get_sw_param;
    p->write_proc  = proc_set_sw_param;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#else
    p->owner       = THIS_MODULE;
#endif

    p = create_proc_entry("mii", 0644, NULL);

    if (p == NULL)
        return -1;

    memset(miidata, 0, sizeof(miidata));

    p->data       = dev;
    p->read_proc  = proc_get_mii_param;
    p->write_proc = proc_set_mii_param;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
#else
    p->owner      = THIS_MODULE;
#endif

    return 0;
}

int ethsw_del_proc_files(void)
{
    remove_proc_entry("switch", NULL);

    remove_proc_entry("mii", NULL);
    return 0;
}

static void str_to_num(char* in, char* out, int len)
{
    int i;
    memset(out, 0, len);

    for (i = 0; i < len * 2; i ++)
    {
        if ((*in >= '0') && (*in <= '9'))
            *out += (*in - '0');
        else if ((*in >= 'a') && (*in <= 'f'))
            *out += (*in - 'a') + 10;
        else if ((*in >= 'A') && (*in <= 'F'))
            *out += (*in - 'A') + 10;
        else
            *out += 0;

        if ((i % 2) == 0)
            *out *= 16;
        else
            out ++;

        in ++;
    }
    return;
}

static int proc_get_sw_param(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int reg_page  = swdata[0];
    int reg_addr  = swdata[1];
    int reg_len   = swdata[2];
    int i = 0;
    int r = 0;

    *eof = 1;

    if (reg_len == 0)
        return 0;

    down(&bcm_ethlock_switch_config);
    ethsw_rreg(reg_page, reg_addr, swdata + 3, reg_len);
    up(&bcm_ethlock_switch_config);

    r += sprintf(page + r, "[%02x:%02x] = ", swdata[0], swdata[1]);

    for (i = 0; i < reg_len; i ++)
        r += sprintf(page + r, "%02x ", swdata[3 + i]);

    r += sprintf(page + r, "\n");
    return (r < cnt)? r: 0;
}

static int proc_set_sw_param(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
    int i;
    int r;
    int num_of_octets;

    int reg_page;
    int reg_addr;
    int reg_len;

    if (cnt > 32)
        cnt = 32;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

    r = cnt;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    } // for

    num_of_octets = r / 2;

    if (num_of_octets < 3) // page, addr, len
        return -EFAULT;

    str_to_num(input, swdata, num_of_octets);

    reg_page  = swdata[0];
    reg_addr  = swdata[1];
    reg_len   = swdata[2];

    if (((reg_len != 1) && (reg_len % 2) != 0) || reg_len > 8)
    {
        memset(swdata, 0, sizeof(swdata));
        return -EFAULT;
    }

    if ((num_of_octets > 3) && (num_of_octets != reg_len + 3))
    {
        memset(swdata, 0, sizeof(swdata));
        return -EFAULT;
    }

    if (num_of_octets > 3) {
        down(&bcm_ethlock_switch_config);
        ethsw_wreg(reg_page, reg_addr, swdata + 3, reg_len);
        up(&bcm_ethlock_switch_config);
    }
    return cnt;
}

static int proc_get_mii_param(char *page, char **start, off_t off, int cnt, int *eof, void *data)
{
    int mii_port  = miidata[0];
    int mii_addr  = miidata[1];
    int r = 0;

    *eof = 1;

    down(&bcm_ethlock_switch_config);
    ethsw_phy_rreg(mii_port, mii_addr, (uint16 *)(miidata + 2));
    up(&bcm_ethlock_switch_config);

    r += sprintf(
        page + r,
        "[%02x:%02x] = %02x %02x\n",
        miidata[0], miidata[1], miidata[2], miidata[3]
    );

    return (r < cnt)? r: 0;
}

static int proc_set_mii_param(struct file *f, const char *buf, unsigned long cnt, void *data)
{
    char input[32];
    int i;
    int r;
    int num_of_octets;

    int mii_port;
    int mii_addr;

    if (cnt > 32)
        cnt = 32;

    if (copy_from_user(input, buf, cnt) != 0)
        return -EFAULT;

    r = cnt;

    for (i = 0; i < r; i ++)
    {
        if (!isxdigit(input[i]))
        {
            memmove(&input[i], &input[i + 1], r - i - 1);
            r --;
            i --;
        }
    }

    num_of_octets = r / 2;

    if ((num_of_octets!= 2) && (num_of_octets != 4))
    {
        memset(miidata, 0, sizeof(miidata));
        return -EFAULT;
    }

    str_to_num(input, miidata, num_of_octets);
    mii_port  = miidata[0];
    mii_addr  = miidata[1];

    down(&bcm_ethlock_switch_config);

    if (num_of_octets > 2)
        ethsw_phy_wreg(mii_port, mii_addr, (uint16 *)(miidata + 2));

    up(&bcm_ethlock_switch_config);
    return cnt;
}

#if defined(CONFIG_BCM96368)
/*
*------------------------------------------------------------------------------
* Function   : ethsw_enable_sar_port
* Description: Setup the SAR port of a 6368 Switch as follows:
*              - Learning is disabled.
*              - Full duplex, 1000Mbs, Link Up
*              - Rx Enabled, Tx Disabled.
*
* Design Note: Invoked by SAR Packet CMF when SAR XTM driver is operational,
*              via CMF Hook pktCmfSarPortEnable.
*
*              This function expects to be called from a softirq context.
*------------------------------------------------------------------------------
*/
int ethsw_enable_sar_port(void)
{
    uint8  val8;
    uint16 val16;

    ethsw_rreg(PAGE_CONTROL, REG_DISABLE_LEARNING,
        (uint8 *)&val16, sizeof(val16) );
    val16 |= ( 1 << SAR_PORT_ID );  /* add sar_port_id to disabled ports */
    ethsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING,
        (uint8 *)&val16, sizeof(val16) );

    val8 = ( REG_PORT_STATE_OVERRIDE    /* software override Phy values */
        | REG_PORT_STATE_1000        /* Speed of 1000 Mbps */
        | REG_PORT_STATE_FDX         /* Full duplex mode */
        | REG_PORT_STATE_LNK );      /* Link State Up */
    ethsw_wreg(PAGE_CONTROL, REG_PORT_STATE + SAR_PORT_ID,
        &val8, sizeof(val8) );

    val8 = REG_PORT_TX_DISABLE;
    ethsw_wreg(PAGE_CONTROL, REG_PORT_CTRL + SAR_PORT_ID,
        &val8, sizeof(val8) );

    return 0;
}

/*
*------------------------------------------------------------------------------
* Function   : ethsw_disable_sar_port
* Description: Setup the SAR port of a 6368 Switch as follows:
*              - Link Sown
*              - Rx Disabled, Tx Disabled.
*
* Design Note: Invoked via CMF Hook pktCmfSarPortDisable.
*
*              This function expects to be called from a softirq context.
*------------------------------------------------------------------------------
*/
int ethsw_disable_sar_port(void)
{
    uint8  val8;

    val8 = REG_PORT_STATE_OVERRIDE;
    ethsw_wreg(PAGE_CONTROL, REG_PORT_STATE + SAR_PORT_ID,
        &val8, sizeof(val8) );

    val8 = (REG_PORT_TX_DISABLE | REG_PORT_RX_DISABLE);
    ethsw_wreg(PAGE_CONTROL, REG_PORT_CTRL + SAR_PORT_ID,
        &val8, sizeof(val8) );

    return 0;
}
#endif

int ethsw_enable_hw_switching(void)
{
    u8 i;

    down(&bcm_ethlock_switch_config);

    /*Don't do anything if already enabled.
     *Enable is implemented by restoring values saved by disable_hw_switching().
     *This check is necessary to make sure we get correct behavior when
     *enable_hw_switching() is called without a preceeding disable_hw_switching() call.
     */
    if (hw_switching_state != HW_SWITCHING_ENABLED) {
      /* restore pbvlan config */
      for(i = 0; i < TOTAL_SWITCH_PORTS; i++)
      {
          ethsw_wreg(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (i * 2),
                     (uint8 *)&pbvlan_map[i], 2);
      }

      /* restore disable learning register */
      ethsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&dis_learning, 2);

      /* restore port forward control register */
      ethsw_wreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&port_fwd_ctrl, 1);

      i = 0;
      while (vnet_dev[i])
      {
          /* When hardware switching is enabled, enable the Linux bridge to
             not to forward the bcast packets on hardware ports */
          vnet_dev[i++]->priv_flags |= IFF_HW_SWITCH;

// we will not enable HW_switching on thsese ports.
// frames from these ports need to go to the forwaders that can handle
// the brcm tag.

#if defined(CONFIG_BCM_6362_PORTS_INT_EXT_SW)
          {
              BcmEnet_devctrl *pDevCtrl = netdev_priv(vnet_dev[i-1]);
              int swPort = pDevCtrl->sw_port_id;
              if (swPort >= MAX_EXT_SWITCH_PORTS) {
                 vnet_dev[i-1]->priv_flags &= ~IFF_HW_SWITCH;
              }
          }
#endif
      }
#if ((defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818)) && defined(DBL_DESC))
      for (i = 0; i < MAX_GEM_IDS; i++)
      {
          if (gponifid_to_dev[i]) {
              /* When hardware switching is enabled, enable the Linux bridge to
                 not to forward the bcast packets on hardware ports */
              gponifid_to_dev[i]->priv_flags |= IFF_HW_SWITCH;
          }
      }
#endif
#if defined(CONFIG_BCM96816)
      for (i = 0; i < MAX_6829_IFS; i++)
      {
          if ( bcm6829_to_dev[i] )
          {
              /* When hardware switching is enabled, enable the Linux bridge to
                 not to forward the bcast packets on hardware ports */
              bcm6829_to_dev[i]->priv_flags |= IFF_HW_SWITCH;
          }
      }
#endif

      hw_switching_state = HW_SWITCHING_ENABLED;
    }

    up(&bcm_ethlock_switch_config);
    return 0;
}

int ethsw_disable_hw_switching(void)
{
    u8 i, byte_value;
    u16 reg_value;

    down(&bcm_ethlock_switch_config);

    /*Don't do anything if already disabled.*/
    if (hw_switching_state == HW_SWITCHING_ENABLED) {

       /* set the port-based vlan control reg of each port with fwding mask of
          only that port and MIPS. For MIPS port, set the forwarding mask of
          all the ports */
       for(i = 0; i < TOTAL_SWITCH_PORTS; i++)
       {
           ethsw_rreg(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (i * 2),
                      (uint8 *)&pbvlan_map[i], 2);
           if (i == MIPS_PORT_ID) {
               reg_value = PBMAP_ALL;
           }
           else {
               reg_value = PBMAP_MIPS;
           }
           ethsw_wreg(PAGE_PORT_BASED_VLAN, REG_VLAN_CTRL_P0 + (i * 2),
                      (uint8 *)&reg_value, 2);
       }

       /* Save disable_learning_reg setting */
       ethsw_rreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&dis_learning, 2);
       /* disable learning on all ports */
       reg_value = PBMAP_ALL;
       ethsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&reg_value, 2);

       /* Save port forward control setting */
       ethsw_rreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&port_fwd_ctrl, 1);
       /* flood unlearned packets */
       byte_value = 0x00;
       ethsw_wreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&byte_value, 1);

       i = 0;
       while (vnet_dev[i])
       {
           /* When hardware switching is disabled, enable the Linux bridge to
              forward the bcast on hardware ports as well */
           vnet_dev[i++]->priv_flags &= ~IFF_HW_SWITCH;
       }

#if ((defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818)) && defined(DBL_DESC))
       for (i = 0; i < MAX_GEM_IDS; i++)
       {
           if (gponifid_to_dev[i]) {
               /* When hardware switching is enabled, enable the Linux bridge to
                  not to forward the bcast on hardware ports */
               gponifid_to_dev[i]->priv_flags &= ~IFF_HW_SWITCH;
           }
       }
#endif
#if defined(CONFIG_BCM96816)
       for (i = 0; i < MAX_6829_IFS; i++)
       {
           if ( bcm6829_to_dev[i] )
           {
               /* When hardware switching is enabled, enable the Linux bridge to
                  not to forward the bcast packets on hardware ports */
               bcm6829_to_dev[i]->priv_flags &= ~IFF_HW_SWITCH;
           }
       }
#endif

       hw_switching_state = HW_SWITCHING_DISABLED;

       /* Flush arl table dynamic entries */
       fast_age_all(0);
    }
    up(&bcm_ethlock_switch_config);
    return 0;
}

int ethsw_get_hw_switching_state(void)
{
    return hw_switching_state;
}
/*
 * This segment of code is abstracted out for re-use. 
 * Semaphore 'bcm_ethlock_switch_config' is handled by caller.
 */
void ethsw_switch_manage_ext_phy_power_mode(int portnumber, int power_mode)
{
    uint16 reg = 0; /* MII reg 0x00 */
    uint16 v16 = 0;
    if(!power_mode) {
        /* MII Power Down enable, forces link down */
        ethsw_phyport_rreg(portnumber, reg, &v16);
        v16 |= 0x0800;
        ethsw_phyport_wreg(portnumber, reg, &v16);
        ephy_forced_pwr_down_status |= (1<<portnumber);
    }
    else {
        /* MII Power Down disable */
        ethsw_phyport_rreg(portnumber, reg, &v16);
        v16 &= ~0x0800;
        ethsw_phyport_wreg(portnumber, reg, &v16);
        ephy_forced_pwr_down_status &= ~(1<<portnumber);
    }
    return; 
}

#if defined(CONFIG_BCM96368)
void ethsw_switch_manage_port_power_mode(int portnumber, int power_mode)
{
   uint16 reg = 0;
   int phy_id = enet_logport_to_phyid(portnumber);

   down(&bcm_ethlock_switch_config);
   // External switch present? 
   if (pVnetDev0_g->extSwitch->present == 1) {
       BCM_ENET_DEBUG("%s: handle the case of external switch present for 6368, port %d \n", __FUNCTION__,
                             portnumber);
       ethsw_switch_manage_ext_phy_power_mode(portnumber, power_mode);
       up(&bcm_ethlock_switch_config);
       return;
   }
   /* Prevent acceses to EPHY registers while in shadow mode */
   atomic_inc(&phy_write_ref_cnt);
   atomic_inc(&phy_read_ref_cnt);
   BcmHalInterruptDisable(INTERRUPT_ID_EPHY);

   if(!power_mode) {
      // To power down an EPHY channel, the following sequence should be used
      reg=0x008B; ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &reg); //Shadow reg enabled
      reg=0x01C0; ethsw_phy_wreg(phy_id, 0x10, &reg);
      reg=0x7000; ethsw_phy_wreg(phy_id, 0x14, &reg);
      reg=0x000F; ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &reg); //Shadow reg 2 enabled
      reg=0x20D0; ethsw_phy_wreg(phy_id, 0x10, &reg);
      reg=0x000B; ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &reg); //Shadow regs disabled
      // Set EPHY_PWR_DOWN_x bit of this register to 0x1
      GPIO->RoboswEphyCtrl |= (1 << (2 + portnumber));
      ephy_forced_pwr_down_status |= (1<<portnumber);
   }
   else {
      // Set EPHY_PWR_DOWN_x bit of this register to 0x0
      GPIO->RoboswEphyCtrl &= ~(1 << (2 + portnumber));
      // To get out of power down an EPHY channel, the following sequence should be used
      reg=0x008B; ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &reg); //Shadow reg enabled
      reg=0x0000; ethsw_phy_wreg(phy_id, 0x10, &reg);
      reg=0x0000; ethsw_phy_wreg(phy_id, 0x14, &reg);
      reg=0x000F; ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &reg); //Shadow reg 2 enabled
      reg=0x00D0; ethsw_phy_wreg(phy_id, 0x10, &reg);
      reg=0x000B; ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &reg); //Shadow regs disabled
      ephy_forced_pwr_down_status &= ~(1<<portnumber);
   }

   atomic_dec(&phy_write_ref_cnt);
   atomic_dec(&phy_read_ref_cnt);
   BcmHalInterruptEnable(INTERRUPT_ID_EPHY);
   up(&bcm_ethlock_switch_config);
}
#else
void ethsw_switch_manage_port_power_mode(int portnumber, int power_mode)
{
   int phy_id = enet_logport_to_phyid(portnumber);
#if NUM_INT_GPHYS > 0
   int phys_port = LOGICAL_PORT_TO_PHYSICAL_PORT(portnumber);
#endif

   down(&bcm_ethlock_switch_config);

   /* external ports, GPHYs and external PHYs */
   if (IsExternalSwitchPort(portnumber) ||
#if NUM_INT_GPHYS > 0
       ((phys_port >= NUM_INT_EPHYS) && (phys_port < NUM_INT_PHYS)) ||
#endif
       (IsExtPhyId(phy_id)))
   {
       ethsw_switch_manage_ext_phy_power_mode(portnumber, power_mode);
   }
#if NUM_INT_EPHYS > 0
   else if (portnumber < NUM_INT_EPHYS) {  /* EPHYs */
       uint16 v16 = 0;
       /* When the link is being brought up or down, the link status interrupt may occur
          before this command is completed, where the PHY is configured in shadow mode.
          We need to prevent this by disabling EPHY interrupts. The problem does not exist
          for GPHY because the link status changes in a single command.
       */
       atomic_inc(&phy_write_ref_cnt);
       atomic_inc(&phy_read_ref_cnt);
       BcmHalInterruptDisable(INTERRUPT_ID_EPHY);

       if(!power_mode) {
          /* Bring it down */
          v16 = 0x008B;
          ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
          v16 = 0x0700; /* tx pwr down */
          ethsw_phy_wreg(phy_id, 0x10, &v16);
          msleep(1); /* Without this, the speed LED on 63168 stays on */
          v16 = 0x7008; /* rx pwr down & link status disable */
          ethsw_phy_wreg(phy_id, 0x14, &v16);
          v16 = 0x000B;
          ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
          ephy_forced_pwr_down_status |= (1<<portnumber);
       }
       else {
          /* Bring it up */
          v16 = 0x008B;
          ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
          v16 = 0x0400;
          ethsw_phy_wreg(phy_id, 0x10, &v16);
          v16 = 0x0008;
          ethsw_phy_wreg(phy_id, 0x14, &v16);
          v16 = 0x000B;
          ethsw_phy_wreg(phy_id, MII_BRCM_TEST, &v16);
          /* Restart Autoneg in case the cable was unplugged or plugged while down */
          ethsw_phy_rreg(phy_id, MII_CONTROL, &v16);
          v16 |= MII_CONTROL_RESTART_AUTONEG;
          ethsw_phy_wreg(phy_id, MII_CONTROL, &v16);
          ephy_forced_pwr_down_status &= ~(1<<portnumber);
       }

       atomic_dec(&phy_write_ref_cnt);
       atomic_dec(&phy_read_ref_cnt);
       BcmHalInterruptEnable(INTERRUPT_ID_EPHY);
   }
#endif

   up(&bcm_ethlock_switch_config);
}
#endif
EXPORT_SYMBOL(ethsw_switch_manage_port_power_mode);

int ethsw_switch_get_port_power_mode(int portnumber)
{
   return (ephy_forced_pwr_down_status & (1<<portnumber));
}
EXPORT_SYMBOL(ethsw_switch_get_port_power_mode);


int ethsw_switch_manage_ports_leds(int led_mode)
{
#define AUX_MODE_REG 0x1d
#define LNK_LED_DIS  4 // Bit4

   uint16 v16, i;

   down(&bcm_ethlock_switch_config);

   for (i=0; i<4; i++) {
      ethsw_phy_rreg(switch_port_phyid[i], AUX_MODE_REG, &v16);

      if(led_mode)
         v16 &= ~(1 << LNK_LED_DIS);
      else
         v16 |= (1 << LNK_LED_DIS);

      ethsw_phy_wreg(switch_port_phyid[i], AUX_MODE_REG, &v16);
   }

   up(&bcm_ethlock_switch_config);
   return 0;
}
EXPORT_SYMBOL(ethsw_switch_manage_ports_leds);

// end power management routines


void extsw_rreg(int page, int reg, uint8 *data, int len)
{
    if (((len != 1) && (len % 2) != 0) || len > 8)
        panic("extsw_rreg: wrong length!\n");

    switch (pVnetDev0_g->extSwitch->accessType)
    {
      case MBUS_MDIO:
          bcmsw_pmdio_rreg(page, reg, data, len);
        break;

      case MBUS_SPI:
      case MBUS_HS_SPI:
        bcmsw_spi_rreg(pVnetDev0_g->extSwitch->bus_num, pVnetDev0_g->extSwitch->spi_ss,
                       pVnetDev0_g->extSwitch->spi_cid, page, reg, data, len);
        break;
      default:
        printk("Error: Neither SPI nor PMDIO access \n");
        break;
    }
}

void extsw_wreg(int page, int reg, uint8 *data, int len)
{
    if (((len != 1) && (len % 2) != 0) || len > 8)
        panic("extsw_wreg: wrong length!\n");

    switch (pVnetDev0_g->extSwitch->accessType)
    {
      case MBUS_MDIO:
        bcmsw_pmdio_wreg(page, reg, data, len);
        break;

      case MBUS_SPI:
      case MBUS_HS_SPI:
        bcmsw_spi_wreg(pVnetDev0_g->extSwitch->bus_num, pVnetDev0_g->extSwitch->spi_ss,
                       pVnetDev0_g->extSwitch->spi_cid, page, reg, data, len);
        break;
      default:
        printk("Error: Neither SPI nor PMDIO access \n");
        break;
    }
}

void extsw_fast_age_port(uint8 port, uint8 age_static) {
    uint8 v8;
    uint8 timeout = 100;

    v8 = FAST_AGE_START_DONE | FAST_AGE_DYNAMIC | FAST_AGE_PORT;
    if (age_static) {
        v8 |= FAST_AGE_STATIC;
    }
    extsw_wreg(PAGE_CONTROL, REG_FAST_AGING_PORT, &port, 1);

    extsw_wreg(PAGE_CONTROL, REG_FAST_AGING_CTRL, &v8, 1);
    extsw_rreg(PAGE_CONTROL, REG_FAST_AGING_CTRL, &v8, 1);
    while (v8 & FAST_AGE_START_DONE) {
        mdelay(1);
        extsw_rreg(PAGE_CONTROL, REG_FAST_AGING_CTRL, &v8, 1);
        if (!timeout--) {
            printk("Timeout of fast aging \n");
            break;
        }
    }

    v8 = 0;
    extsw_wreg(PAGE_CONTROL, REG_FAST_AGING_CTRL, &v8, 1);
}

void extsw_set_wanoe_portmap(uint16 wan_port_map)
{
    int i;
    uint8 map[2] = {0};
    //BCM_ENET_DEBUG("wanoe port map = 0x%x", wan_port_map);

    /* Set WANoE port map */
    map[0] = (uint8)wan_port_map;

    extsw_wreg(PAGE_CONTROL, REG_WAN_PORT_MAP, map, 2);

    map[0] = (uint8)wan_port_map;
    /* MIPS port */
    map[1] = 1;
    /* Disable learning */
    extsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING, map, 2);

    for(i=0; i < TOTAL_SWITCH_PORTS; i++) {
       if((wan_port_map >> i) & 0x1) {
            extsw_fast_age_port(i, 1);
       }
    }
}

#if defined(CONFIG_BCM_EXT_SWITCH)
void extsw_rreg_wrap(int page, int reg, uint8 *data, int len)
{
   uint8 val[4];

   extsw_rreg(page, reg, val, len);
   switch (len) {
   case 1:
      data[0] = val[0];
      break;
   case 2:
      *((uint16 *)data) = __le16_to_cpu(*((uint16 *)val));
      break;
   case 4:
      *((uint32 *)data) = __le32_to_cpu(*((uint32 *)val));
      break;
   default:
      printk("Length %d not supported\n", len);
      break;
   }
}

void extsw_wreg_wrap(int page, int reg, uint8 *data, int len)
{
   uint8 val[4];

   switch (len) {
   case 1:
      val[0] = data[0];
      break;
   case 2:
      *((uint16 *)val) = __cpu_to_le16(*((uint16 *)data));
      break;
   case 4:
      *((uint32 *)val) = __cpu_to_le32(*((uint32 *)data));
      break;
   default:
      printk("Length %d not supported\n", len);
      break;
   }
   extsw_wreg(page, reg, val, len);
}

int extsw_bus_contention(int operation, int retries)
{
   static int contention_req = 0;
   uint8 val;
   int i;

   if (operation) {
      if (!contention_req) {
         val=2;
         extsw_wreg_wrap(PAGE_CONTROL, 0xa0, &val , 1);
         contention_req = 1;
      }

      for (i=0;i<retries;++i) {
         extsw_rreg_wrap(PAGE_CONTROL, 0xa0, &val , 1);
         if (val == 3) {
            return 1;
         }
      }
   } else {
      contention_req = 0;
      val=0;
      extsw_wreg_wrap(PAGE_CONTROL, 0xa0, &val , 1);
      return 1;
   }

   return 0;
}
#endif

void ethsw_phyport_rreg(int port, int reg, uint16 *data)
{
   int phys_port = port;
   int unit = 0;
   int phy_id;

#if defined(CONFIG_BCM_EXT_SWITCH)
   if (pVnetDev0_g->extSwitch->present == 1) {
       phys_port = LOGICAL_PORT_TO_PHYSICAL_PORT(port);
       unit = (IsExternalSwitchPort(port)?1:0);
   }

   if (unit == 1) {
      if (pVnetDev0_g->extSwitch->accessType == MBUS_MDIO) {
         phy_id = pVnetDev0_g->EnetInfo[unit].sw.phy_id[phys_port];
         ethsw_phy_read_reg(phy_id, reg, data, unit);
      } else {
            extsw_rreg_wrap(PAGE_INTERNAL_PHY_MII+phys_port, reg*2, (uint8 *)data, 2);
      }
   } else
#endif
   {
      phy_id = pVnetDev0_g->EnetInfo[unit].sw.phy_id[phys_port];
      ethsw_phy_rreg(phy_id, reg, data);
   }
}

void ethsw_phyport_wreg(int port, int reg, uint16 *data)
{
   int phys_port = port;
   int unit = 0;
   int phy_id;

#if defined(CONFIG_BCM_EXT_SWITCH)
   if (pVnetDev0_g->extSwitch->present == 1) {
       phys_port = LOGICAL_PORT_TO_PHYSICAL_PORT(port);
       unit = (IsExternalSwitchPort(port)?1:0);
   }

   if (unit == 1) {
      if (pVnetDev0_g->extSwitch->accessType == MBUS_MDIO) {
         phy_id = pVnetDev0_g->EnetInfo[unit].sw.phy_id[phys_port];
         ethsw_phy_write_reg(phy_id, reg, data, unit);
      } else {
         extsw_wreg_wrap(PAGE_INTERNAL_PHY_MII+phys_port, reg*2, (uint8 *)data, 2);
      }
   } else
#endif
   {
      phy_id = pVnetDev0_g->EnetInfo[unit].sw.phy_id[phys_port];
      ethsw_phy_wreg(phy_id, reg, data);
   }
}

void ethsw_phyport_rreg2(int phy_id, int reg, uint16 *data, int flags)
{
#if defined(CONFIG_BCM_EXT_SWITCH)
   int unit = 0;

   if (pVnetDev0_g->extSwitch->present == 1) {
       unit = flags & ETHCTL_FLAG_ACCESS_EXTSW_PHY? 1: 0;
   }

   if (unit == 1) {
         // whether MDIO or SPI
         ethsw_phy_read_reg(phy_id, reg, data, unit);
         BCM_ENET_DEBUG("%s phy id 0x%x accessed on %s intf reg %d data 0x%x \n", __FUNCTION__,
                           phy_id, pVnetDev0_g->extSwitch->accessType == MBUS_MDIO?
                           "MDIO": "SPI", reg, *data); 
   } else
#endif
   {
      ethsw_phy_read_reg(phy_id, reg, data, flags & ETHCTL_FLAG_ACCESS_EXT_PHY);
   }
}

void ethsw_phyport_wreg2(int phy_id, int reg, uint16 *data, int flags)
{
#if defined(CONFIG_BCM_EXT_SWITCH)
   int unit = 0;

   if (pVnetDev0_g->extSwitch->present == 1) {
       unit = flags & ETHCTL_FLAG_ACCESS_EXTSW_PHY? 1: 0;
   }

   if (unit == 1) {
         // whether MDIO or SPI
         ethsw_phy_write_reg(phy_id, reg, data, unit);
         BCM_ENET_DEBUG("%s phy id 0x%x accessed on %s intf reg %d data 0x%x \n", __FUNCTION__,
                           phy_id, pVnetDev0_g->extSwitch->accessType == MBUS_MDIO?
                           "MDIO": "SPI", reg, *data); 
   } else
#endif
   {
      ethsw_phy_write_reg(phy_id, reg, data, flags & ETHCTL_FLAG_ACCESS_EXT_PHY);
   }
}

void ethsw_eee_compatibility_set(int port, int enable)
{
   uint16 v16, apdv16;
   int apd_disabled;

   /* Disable APD if it was set */
   ethsw_phyport_rreg(port, MII_REGISTER_1C, &apdv16);
   if (apdv16 & MII_1C_AUTO_POWER_DOWN) {
      apdv16 &= ~MII_1C_AUTO_POWER_DOWN;
      ethsw_phyport_wreg(port, MII_REGISTER_1C, &apdv16);
      apd_disabled = 1;
   }

   /* Write a sequence specific for these GPHYs */
   v16 = 0x0C00;
   ethsw_phyport_wreg(port, 0x18, &v16);
   v16 = 0x001A;
   ethsw_phyport_wreg(port, 0x17, &v16);
   if (enable) {
      v16 = 0x0003;
   } else {
      v16 = 0x0007;
   }
   ethsw_phyport_wreg(port, 0x15, &v16);
   v16 = 0x0400;
   ethsw_phyport_wreg(port, 0x18, &v16);

   /* Re-enable APD if it was disabled by this code */
   if (apd_disabled) {
      apdv16 |= MII_1C_AUTO_POWER_DOWN;
      ethsw_phyport_wreg(port, MII_REGISTER_1C, &apdv16);
   }
}

#if defined(CONFIG_BCM_EXT_SWITCH)
void extsw_eee_init(void)
{
   uint32 v32;
   int i;

   down(&bcm_ethlock_switch_config);
   /* EEE requires initialization on 53125 */
   if (pVnetDev0_g->extSwitch->switch_id == 0x53125) {
      extsw_bus_contention(1,5);

      /* Change default settings */
      for (i=0; i<6; ++i) {
         /* Change the Giga EEE Sleep Timer default value from 4 uS to 400 uS */
         v32 = 0x00000190;
         extsw_wreg_wrap(PAGE_EEE, REG_EEE_SLEEP_TIMER_G+(i*4), (uint8 *)&v32, 4);
         /* Change the 100 Mbps EEE Sleep Timer default value from 40 uS to 4000 uS */
         v32 = 0x00000FA0;
         extsw_wreg_wrap(PAGE_EEE, REG_EEE_SLEEP_TIMER_FE+(i*4), (uint8 *)&v32, 4);

         /* Set the initial compatibility. This is also required in mdk, if mdk is used */
         ethsw_eee_compatibility_set(i, 0);
      }

      extsw_bus_contention(0,0);
   }
   up(&bcm_ethlock_switch_config);
}
#endif

#if defined(GPHY_EEE_1000BASE_T_DEF) || defined(CONFIG_BCM_EXT_SWITCH) || defined(CONFIG_BCM96318)
/* Clause 45 register read */
void ethsw_phyport_c45_rreg(int port, int regg, int regr, uint16 *pdata16) {
   uint16 val16;
   val16 = regg;
   ethsw_phyport_wreg(port, 0x0d, &val16);
   val16 = regr;
   ethsw_phyport_wreg(port, 0x0e, &val16);
   val16 = 0x4000 | regg;
   ethsw_phyport_wreg(port, 0x0d, &val16);
   ethsw_phyport_rreg(port, 0x0e, pdata16);
}

/* Clause 45 register writes */
void ethsw_phyport_c45_wreg(int port, int regg, int regr, uint16 *pdata16) {
   uint16 val16;
   val16 = regg;
   ethsw_phyport_wreg(port, 0x0d, &val16);
   val16 = regr;
   ethsw_phyport_wreg(port, 0x0e, &val16);
   val16 = 0x4000 | regg;
   ethsw_phyport_wreg(port, 0x0d, &val16);
   ethsw_phyport_wreg(port, 0x0e, pdata16);
}
#endif

#if defined(CONFIG_BCM96318)
void ethsw_eee_ephy_enable(int phy_id, int enable)
{
   uint16 data16;

   if (!IsExtPhyId(phy_id)) {
      if (enable) {
         data16 = 0x000f;
         ethsw_phy_wreg(phy_id, 0x1f, &data16);
         data16 = 0x0003;
         ethsw_phy_wreg(phy_id, 0x0e, &data16);
         data16 = 0x0002;
         ethsw_phy_wreg(phy_id, 0x0f, &data16);
         data16 = 0x0006;
         ethsw_phy_wreg(phy_id, 0x0e, &data16);
         data16 = 0x4404;
         ethsw_phy_wreg(phy_id, 0x0f, &data16);
         data16 = 0x000e;
         ethsw_phy_wreg(phy_id, 0x0e, &data16);
         data16 = 0x0050;
         ethsw_phy_wreg(phy_id, 0x0f, &data16);
         data16 = 0x000b;
         ethsw_phy_wreg(phy_id, 0x0e, &data16);
         data16 = 0x0003;
         ethsw_phy_wreg(phy_id, 0x0f, &data16);
         data16 = 0x000b;
         ethsw_phy_wreg(phy_id, 0x1f, &data16);
         data16 = 0x3200;
         ethsw_phy_wreg(phy_id, 0x00, &data16);
      } else {
         data16 = 0x000f;
         ethsw_phy_wreg(phy_id, 0x1f, &data16);
         data16 = 0x0003;
         ethsw_phy_wreg(phy_id, 0x0e, &data16);
         data16 = 0x0000;
         ethsw_phy_wreg(phy_id, 0x0f, &data16);
         data16 = 0x000b;
         ethsw_phy_wreg(phy_id, 0x0e, &data16);
         data16 = 0x0000;
         ethsw_phy_wreg(phy_id, 0x0f, &data16);
         data16 = 0x000b;
         ethsw_phy_wreg(phy_id, 0x1f, &data16);
         data16 = 0x3200;
         ethsw_phy_wreg(phy_id, 0x00, &data16);
      }
   }
}
#endif

void ethsw_eee_port_enable(int port, int enable, int linkstate)
{
   int phys_port;
   int unit;
   int phy_id;
   uint16 data16;

   /* Disable EEE if Ethernet power savings are disabled */
   if (!eth_eee_enabled) {
      enable = 0;
   }

   if (pVnetDev0_g->extSwitch->present == 1) {
       phys_port = LOGICAL_PORT_TO_PHYSICAL_PORT(port);
       unit = (IsExternalSwitchPort(port)?1:0);
   } else {
       phys_port = port;
       unit = 0;
   }
   phy_id = pVnetDev0_g->EnetInfo[unit].sw.phy_id[phys_port];

   if ((unit == 0) && IsPhyConnected(phy_id) && IsExtPhyId(phy_id)) {
      // Internal switch with an external PHY
      // Only apply these settings to the 50612, rev 1 and 2.
      ethsw_phy_rreg(phy_id, MII_PHYSID1, &data16);
      if (data16 == 0x0362) {
         ethsw_phy_rreg(phy_id, MII_PHYSID2, &data16);
         if ((data16 == 0x5e61) || (data16 == 0x5e62) || (data16 == 0x5e6a) || (data16 == 0x5f6a) ||
             (data16 == 0x5e6e) || (data16 == 0x5e66)) {
            ethsw_eee_compatibility_set(port, enable);
         }
      }
   }

#if defined(GPHY_EEE_1000BASE_T_DEF)
   if (unit == 0) {
      /* Integrated switch */
      /* Ensure that EEE was enabled in bootloader */
      #define EEE_BITS (GPHY_LPI_FEATURE_EN_DEF_MASK | \
         GPHY_EEE_1000BASE_T_DEF | GPHY_EEE_100BASE_TX_DEF | \
         GPHY_EEE_PCS_1000BASE_T_DEF | GPHY_EEE_PCS_100BASE_TX_DEF)

      if ( (GPIO->RoboswGphyCtrl & EEE_BITS) == EEE_BITS ) {
         /* Only the GPHY port supports EEE on 63268 */
         if ((phys_port >= NUM_INT_EPHYS) && (phys_port < NUM_INT_PHYS)) {
            uint16 v16 = 0;

            if (enable) {
               /* Check if 100Base-T Bit[1] or 1000Base-T EEE Bit[2] was advertised by the partner reg 7.61 */
               /* This step is not essential since the PHY does this already */
               ethsw_phyport_c45_rreg(port, 7, 61, &data16);
               if (data16 & 0x6) {
                  v16 = REG_EEE_CTRL_ENABLE;
               }
            }
#if defined(CONFIG_BCM_GMAC)
            BCM_ENET_DEBUG("%s: Checking port %d req %d v16 %d d16 0x%x\n",
                           __FUNCTION__,  phys_port, enable, v16, data16);
            if ((phys_port == GMAC_PORT_ID) && gmac_info_pg->enabled &&
                                  (gmac_info_pg->link_speed == 1000)) { 
                volatile GmacEEE_t *gmacEEEp = GMAC_EEE;
                gmacEEEp->eeeCtrl.enable = v16? 1: 0;
            } else
#endif
            ethsw_wreg(PAGE_CONTROL, REG_EEE_CTRL + (phys_port << 1), (uint8_t *)&v16, 2);
         }
      }
   } 
#endif
#if defined(CONFIG_BCM96318)
   if (unit == 0) {
      /* Integrated switch */
      if (phys_port < NUM_INT_PHYS) {
         uint16 v16 = 0;

         if (enable) {
            /* Check if AN EEE Resolution is set */
            data16 = 0x000f;
            ethsw_phy_wreg(phy_id, 0x1f, &data16);
            data16 = 0x000b;
            ethsw_phy_wreg(phy_id, 0x0e, &data16);
            ethsw_phy_rreg(phy_id, 0x0f, &data16);
            if (data16 & 0x100) {
               v16 = REG_EEE_CTRL_ENABLE;
            }
            data16 = 0x000b;
            ethsw_phy_wreg(phy_id, 0x1f, &data16);
         }
         ethsw_wreg(PAGE_CONTROL, REG_EEE_CTRL + (phys_port << 1), (uint8_t *)&v16, 2);
      }
   } 
#endif
#if defined(CONFIG_BCM_EXT_SWITCH)
   if (unit == 1) {
      /* External switch GPHYs */
      if (pVnetDev0_g->extSwitch->switch_id == 0x53125) {
         uint16 v16;
         static int eee_strap = -1;

         extsw_bus_contention(1,5);

         /* Determine if eee strap is enabled (works when 8051 is disabled) */
         /* 8051 overwrites the strap setting with 0 at boot time, and overwrites it with 0x1f */
         /* the first time a link comes up, this is why we read the strap setting here */
         extsw_rreg_wrap(PAGE_EEE, REG_EEE_EN_CTRL, (uint8 *)&v16, 2);

         if ((eee_strap < 0) && linkstate) {
            eee_strap = v16;
            v16 = 0; /* Start with EEE disabled on all ports */
            extsw_wreg_wrap(PAGE_EEE, REG_EEE_EN_CTRL, (uint8 *)&v16, 2);
         }

         if (enable) {
            if (eee_strap > 0) {
               /* Check if 100Base-T Bit[1] or 1000Base-T EEE Bit[2] was advertised by the partner reg 7.61 */
               /* This step is not essential since the PHY does this already */
               ethsw_phyport_c45_rreg(port, 7, 61, &data16);
               if (data16 & 0x6) {
                  v16 |= (1<<phys_port);
               }
            }
         } else {
            v16 &= ~(1<<phys_port);
         }
         extsw_wreg_wrap(PAGE_EEE, REG_EEE_EN_CTRL, (uint8 *)&v16, 2);
         if (linkstate || eth_eee_enabled) {
            ethsw_eee_compatibility_set(port, linkstate);
         }

         extsw_bus_contention(0,0);
      }
   }
#endif
}

void ethsw_eee_init(void)
{
#if 0 //__ZyXEL__, Wood, disable ethernet powersaving
   int i, phy_id;

   down(&bcm_ethlock_switch_config);
   for (i=0;i<MAX_SWITCH_PORTS;i++) {
      phy_id = pVnetDev0_g->EnetInfo[0].sw.phy_id[i];

#if defined(CONFIG_BCM96318)
      if (!IsExtPhyId(phy_id)) {
         /* On 6318, need to enable EEE on the internal EPHY first */
         ethsw_eee_ephy_enable(phy_id, 1);
      }
#endif

      if (IsPhyConnected(phy_id) && IsExtPhyId(phy_id)) {
         ethsw_eee_port_enable(i, 0, 0);
      }
   }
   up(&bcm_ethlock_switch_config);
#else
   ethsw_eee_all_enable(eth_eee_enabled);
#endif
}

#ifdef CONFIG_BCM_ETH_PWRSAVE
static void ethsw_eee_all_enable(int enable)
{
#if defined(GPHY_EEE_1000BASE_T_DEF) || defined(CONFIG_BCM_EXT_SWITCH) || defined(CONFIG_BCM96318)
   struct net_device *dev = vnet_dev[0];
   BcmEnet_devctrl *priv = (BcmEnet_devctrl *)netdev_priv(dev);
   int i;
   int phys_port;
   int unit;
   uint16 val16;

   down(&bcm_ethlock_switch_config);

   /* Enable/disable EEE in the PHYs */
#if defined(CONFIG_BCM_EXT_SWITCH)
   for (i=0;i<MAX_SWITCH_PORTS+MAX_EXT_SWITCH_PORTS;i++) {
#else
   for (i=0;i<MAX_SWITCH_PORTS;i++) {
#endif
      if (pVnetDev0_g->extSwitch->present == 1) {
          phys_port = LOGICAL_PORT_TO_PHYSICAL_PORT(i);
          unit = (IsExternalSwitchPort(i)?1:0);
      }
      else {
          phys_port = i;
          unit = 0;
      }

#if 1
      if (pVnetDev0_g->EnetInfo[unit].sw.port_map & (1 << phys_port)) {
#else
      if (pVnetDev0_g->EnetInfo[unit].sw.port_map & (1 << i)) {
#endif
#if defined(CONFIG_BCM96318)
         int phy_id = pVnetDev0_g->EnetInfo[unit].sw.phy_id[phys_port];
         ethsw_eee_ephy_enable(phy_id, enable);
#else
         ethsw_phyport_c45_rreg(i, 7, 60, &val16);
         val16 = (enable)?6:0;
         ethsw_phyport_c45_wreg(i, 7, 60, &val16);
         ethsw_phyport_c45_rreg(i, 7, 60, &val16);
#endif
      }
   
      /* Only enable/disable eee MAC on links that are up */
      if (priv->linkState & (1 << i)) {
         ethsw_eee_port_enable(i, enable, 1);

         /* Restart autoneg */
         ethsw_phyport_rreg(i, MII_CONTROL, &val16);
         val16 |= MII_CONTROL_RESTART_AUTONEG;
         ethsw_phyport_wreg(i, MII_CONTROL, &val16);
      }
   }

   /* Clear the global variable since we took care of enabling/disabling eee */
   priv->eee_enable_request_flag[0] = 0;
   priv->eee_enable_request_flag[1] = 0;
   up(&bcm_ethlock_switch_config);
#endif
}
#endif

void ethsw_eee_process_delayed_enable_requests(void)
{
   struct net_device *dev = vnet_dev[0];
   BcmEnet_devctrl *priv = (BcmEnet_devctrl *)netdev_priv(dev);
   int i;

   /* Process enable requests that have been here for more than 1 second */
   if (priv->eee_enable_request_flag[1]) {
     down(&bcm_ethlock_switch_config);
      for (i=0;i<MAX_SWITCH_PORTS+MAX_EXT_SWITCH_PORTS;i++) {
         if (priv->eee_enable_request_flag[1] & (1<<i)) {
            ethsw_eee_port_enable(i, 1, priv->linkState & (1 << i));
#if defined(CONFIG_BCM_GMAC)
                // need to tell GMAC EEE controller of link up/down as well.
                if ((i == GMAC_PORT_ID) && gmac_info_pg->enabled &&
                                      (gmac_info_pg->link_speed == 1000))
                {
                    volatile GmacEEE_t *gmacEEEp = GMAC_EEE;
                    gmacEEEp->eeeCtrl.linkUp = 1;
                }
#endif
         }
      }
      up(&bcm_ethlock_switch_config);
   }

   /* Now delay recent requests by one polling interval (1 second) */
   priv->eee_enable_request_flag[1] = priv->eee_enable_request_flag[0];
   priv->eee_enable_request_flag[0] = 0;
}

#if (CONFIG_BCM_EXT_SWITCH_TYPE == 53115)
/* This code keeps the APD compatibility bit set (register 1c, shadow 0xa, bit 8)
   when the internal 8051 on the 53125 chip clears it. If the 8051 is disabled,
   then this code does not need to run. */
void extsw_apd_set_compatibility_mode(void)
{
   uint16 v16;
   int i;
   static int port = 0;
   static int contention_req = 0;

   if (pVnetDev0_g->extSwitch->switch_id != 0x53125)
      return;

   down(&bcm_ethlock_switch_config);
   /* Only check ports that don't have a link */
   if (!(pVnetDev0_g->linkState & (1 << port)) || contention_req) {

      /* Bus contention with 8051 */
      contention_req = 1;
      if (!extsw_bus_contention(1, 3)) {
         /* We'll get it next time around */
         up(&bcm_ethlock_switch_config);
         return;
      }

      /* Check if one of the ports needs to set APD compatibility */
      v16 = MII_1C_AUTO_POWER_DOWN_SV;
      ethsw_phyport_wreg(port, MII_REGISTER_1C, &v16);
      ethsw_phyport_rreg(port, MII_REGISTER_1C, &v16);

      /* If one of the ports needs to be set, process all the ports */
      if (v16 == 0x2821) {
         for (i=0;i<5;i++) {
            /* Don't need to process ports that have a link */
            if (pVnetDev0_g->linkState & (1 << i))
               continue;

            /* Only change the register if it is not correctly set */
            v16 = MII_1C_AUTO_POWER_DOWN_SV;
            ethsw_phyport_wreg(i, MII_REGISTER_1C, &v16);
            ethsw_phyport_rreg(i, MII_REGISTER_1C, &v16);
            if (v16 == 0x2821) {
               /* Change it to 0xa921 */
               v16 = MII_1C_WRITE_ENABLE | MII_1C_AUTO_POWER_DOWN_SV | MII_1C_WAKEUP_TIMER_SEL_84
                   | MII_1C_AUTO_POWER_DOWN | MII_1C_APD_COMPATIBILITY;
               ethsw_phyport_wreg(i, MII_REGISTER_1C, &v16);
            }
         }
      }

      /* Release bus to 8051 */
      contention_req = 0;
      extsw_bus_contention(0, 0);
   }

   if (++port > 4) {
      port = 0;
   }
   up(&bcm_ethlock_switch_config);
}
#endif

void ethsw_phy_config()
{
    ethsw_setup_led();
#if defined(CONFIG_BCM_ETH_HWAPD_PWRSAVE)
    ethsw_setup_hw_apd(1);
#endif

    ethsw_setup_phys();

    ethsw_phy_advertise_caps();

    ethsw_phy_apply_init_bp();
}

#if defined(CONFIG_BCM96828) && !defined(CONFIG_EPON_HGU)

static uint8 g_rx_port_to_iudma_init_cfg[MAX_SWITCH_PORTS] =
{
    PKTDMA_ETH_US_IUDMA  /* alls ports default to the US iuDMA channel */
};

void saveEthPortToRxIudmaConfig(uint8 port, uint8 iudma)
{
    if((port < MAX_SWITCH_PORTS) && (iudma < ENET_RX_CHANNELS_MAX))
    {
        g_rx_port_to_iudma_init_cfg[port] = iudma;
    }
    else
    {
        printk("%s : Invalid Argument: port <%d>, channel <%d>\n",
               __FUNCTION__, port, iudma);
    }
}

int restoreEthPortToRxIudmaConfig(uint8 port)
{
#if defined(CONFIG_BCM_EXT_SWITCH)
    if(port >= MAX_SWITCH_PORTS + MAX_EXT_SWITCH_PORTS) {
        printk("%s : Invalid Argument: port <%d>\n", __FUNCTION__, port);
        return PKTDMA_ETH_US_IUDMA;
    }
    if (IsExternalSwitchPort(port))
    {
        port = BpGetPortConnectedToExtSwitch();
    } else {
        port = LOGICAL_PORT_TO_PHYSICAL_PORT(port);
    }
#endif

    if(port < MAX_SWITCH_PORTS)
    {
        return g_rx_port_to_iudma_init_cfg[port];
    }
    else
    {
        printk("%s : Invalid Argument: port <%d>\n", __FUNCTION__, port);

        return PKTDMA_ETH_US_IUDMA;
    }
}


void ethsw_set_port_to_fap_map(unsigned int portMap, int split_upstream)
{
    struct ethswctl_data e2;
    int i, j, swap = 0;

    for(i = 0; i < BP_MAX_SWITCH_PORTS; i++)
    {
        if (portMap & (1 << i)) {
            for(j = 0; j <= MAX_PRIORITY_VALUE; j++)
            {
                e2.type = TYPE_SET;
                e2.port = i;
                e2.priority = j;

                if (i == EPON_PORT_ID) {
                    e2.queue = PKTDMA_ETH_DS_IUDMA;
                }
                else {
                    if (split_upstream) {
                        if (swap) {
                            e2.queue = PKTDMA_ETH_DS_IUDMA;
                        } else {
                            e2.queue = PKTDMA_ETH_US_IUDMA;
                        }
                    } else {
                        e2.queue = PKTDMA_ETH_US_IUDMA;
                    }
                }
                saveEthPortToRxIudmaConfig(e2.port, e2.queue);
                mapEthPortToRxIudma(e2.port, e2.queue);
                enet_ioctl_ethsw_cosq_port_mapping(&e2);
            }
            if (swap) {
                swap = 0;
            } else {
                swap = 1;
            }
        }
    }
}

int epon_uni_to_uni_ctrl(unsigned int portMap, int val)
{
    int i = 0;
    uint8_t v8;
    uint16_t v16;

    if (val) {
        /* UNI to UNI communication is needed. So, DS also goes through FAP */
        v16 = 0x1FF;
        ethsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&v16, 2);

        v16 = 0x100;
        for (i=0; i < MAX_SWITCH_PORTS; i++) {
            ethsw_wreg(PAGE_PORT_BASED_VLAN, (i * 2), (uint8*)&v16, 2);
        }

        v8 = (REG_PORT_FORWARD_MCST | REG_PORT_FORWARD_UCST | REG_PORT_FORWARD_IP_MCST);
        ethsw_wreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&v8, sizeof(v8));
        v16 = PBMAP_MIPS;
        ethsw_wreg(PAGE_CONTROL, REG_UCST_LOOKUP_FAIL, (uint8 *)&v16, 2);
        ethsw_wreg(PAGE_CONTROL, REG_IPMC_LOOKUP_FAIL, (uint8 *)&v16, 2);
        ethsw_wreg(PAGE_CONTROL, REG_MCST_LOOKUP_FAIL, (uint8 *)&v16, 2);

        ethsw_rreg(PAGE_CONTROL, REG_SWITCH_MODE, (uint8 *)&v8, 1);
        v8 &= (~BROADCAST_TO_ONLY_MIPS);
        ethsw_wreg(PAGE_CONTROL, REG_SWITCH_MODE, (uint8 *)&v8, 1);

        ethsw_set_port_to_fap_map(portMap, 0);

        ethsw_rreg(PAGE_8021Q_VLAN, REG_VLAN_GLOBAL_8021Q, (uint8 *)&v8, 1);
        v8 &=  ~(1 << VLAN_EN_8021Q_S);
        ethsw_wreg(PAGE_8021Q_VLAN, REG_VLAN_GLOBAL_8021Q, (uint8 *)&v8, 1);

        fapL2flow_defaultVlanTagConfig(0);

        for (i=0; i < MAX_SWITCH_PORTS; i++) {
            if ((portMap & (1 << i)) && (i != EPON_PORT_ID))
                fapPkt_setFloodingMask(i, PBMAP_UNIS & (~(1 << i)), 0);
        }

        fapPkt_hwArlConfig(0, 0);

        uni_to_uni_enabled = 1;
    } else {
        /* UNI to UNI communication is NOT needed. So, resolved DS goes through hardware */
        v16 = 0x1FF;
        ethsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&v16, 2);

        v16 = PBMAP_MIPS;
        for (i=0; i < 7; i++) {
            ethsw_wreg(PAGE_PORT_BASED_VLAN, (i * 2), (uint8*)&v16, 2);
        }

        v16 = PBMAP_ALL;
        ethsw_wreg(PAGE_PORT_BASED_VLAN, (EPON_PORT_ID * 2), (uint8*)&v16, 2);

        v8 = (REG_PORT_FORWARD_MCST | REG_PORT_FORWARD_UCST | REG_PORT_FORWARD_IP_MCST);
        ethsw_wreg(PAGE_CONTROL, REG_PORT_FORWARD, (uint8 *)&v8, sizeof(v8));
        v16 = PBMAP_MIPS;
        ethsw_wreg(PAGE_CONTROL, REG_UCST_LOOKUP_FAIL, (uint8 *)&v16, 2);
        ethsw_wreg(PAGE_CONTROL, REG_IPMC_LOOKUP_FAIL, (uint8 *)&v16, 2);
        ethsw_wreg(PAGE_CONTROL, REG_MCST_LOOKUP_FAIL, (uint8 *)&v16, 2);

        ethsw_rreg(PAGE_CONTROL, REG_SWITCH_MODE, (uint8 *)&v8, 1);
        v8 |= BROADCAST_TO_ONLY_MIPS;
        ethsw_wreg(PAGE_CONTROL, REG_SWITCH_MODE, (uint8 *)&v8, 1);

        v8 = (1 << VID_FFF_ENABLE_S);
        ethsw_wreg(PAGE_8021Q_VLAN, REG_VLAN_GLOBAL_CTRL5, (uint8 *)&v8, 1);

        for (i = 0; i < MAX_SWITCH_PORTS; i++) {
            ethsw_rreg(PAGE_8021Q_VLAN, REG_VLAN_DEFAULT_TAG, (uint8 *)&v16, 2);
            v16 &= (~DEFAULT_TAG_VID_M);
            v16 |= (0xFFF << DEFAULT_TAG_VID_S);
            ethsw_wreg(PAGE_8021Q_VLAN, REG_VLAN_DEFAULT_TAG + (i*2), (uint8 *)&v16, 2);
        }

        ethsw_rreg(PAGE_8021Q_VLAN, REG_VLAN_GLOBAL_8021Q, (uint8 *)&v8, 1);
        v8 |=  (1 << VLAN_EN_8021Q_S);
        v8 |= (VLAN_IVL_SVL_M << VLAN_IVL_SVL_S);
        ethsw_wreg(PAGE_8021Q_VLAN, REG_VLAN_GLOBAL_8021Q, (uint8 *)&v8, 1);

        for (i = 0; i < 4095; i++) {
            write_vlan_table(i, 0x1ff);
        }
        write_vlan_table(0xfff, 0x1ff | (0x1ff<<9));

        fapL2flow_defaultVlanTagConfig(1);

        for (i=0; i < MAX_SWITCH_PORTS; i++) {
            if ((portMap & (1 << i)) && (i != EPON_PORT_ID))
                fapPkt_setFloodingMask(i, PBMAP_EPON, 0);
        }

        ethsw_set_port_to_fap_map(portMap, 1);

        fapPkt_hwArlConfig(1, PBMAP_UNIS);

        uni_to_uni_enabled = 0;
    }

    fapPkt_mcastSetMissBehavior(1);

    return BCM_E_NONE;
}

int enet_learning_ctrl(uint32_t portMask, uint8_t enable)
{
    uint16_t v16;

    BCM_ENET_DEBUG("Given portMask: %02d \n ", portMask);
    if (portMask > PBMAP_ALL) {
        BCM_ENET_DEBUG("Invalid portMask: %02d \n ", portMask);
        return -1;
    }

    ethsw_rreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&v16, 2);
    if (enable) {
        v16 &= ~(portMask);
    } else {
        v16 |= portMask;
    }
    ethsw_wreg(PAGE_CONTROL, REG_DISABLE_LEARNING, (uint8 *)&v16, 2);

    return BCM_E_NONE;
}

int bcm_fun_enet_drv_handler(void *ptr)
{
    BCM_EnetHandle_t *pParam = (BCM_EnetHandle_t *)ptr;

    switch (pParam->type) {
        case BCM_ENET_FUN_TYPE_LEARN_CTRL:
            enet_learning_ctrl(pParam->portMask, pParam->enable);
            break;

        case BCM_ENET_FUN_TYPE_ARL_WRITE:
            enet_arl_write(pParam->arl_entry.mac, pParam->arl_entry.vid, pParam->arl_entry.val);
            break;

        case BCM_ENET_FUN_TYPE_AGE_PORT:
            fast_age_port(pParam->port, 0);
            break;

        case BCM_ENET_FUN_TYPE_PORT_RX_CTRL:
            {
                struct ethswctl_data e;
                e.type = TYPE_SET;
                e.port = pParam->port;
                e.val = (pParam->enable)?0:1;
                enet_ioctl_ethsw_port_traffic_control(&e);
            }
            break;

        case BCM_ENET_FUN_TYPE_UNI_UNI_CTRL:
            {
                epon_uni_to_uni_ctrl(pVnetDev0_g->EnetInfo[0].sw.port_map, pParam->enable);
            }
            break;

        case BCM_ENET_FUN_TYPE_GET_VPORT_CNT:
            pParam->uniport_cnt = vport_cnt - 1;
            break;

        case BCM_ENET_FUN_TYPE_GET_IF_NAME_OF_VPORT:
            strcpy(pParam->name, vnet_dev[pParam->port]->name);
            break;

        case BCM_ENET_FUN_TYPE_GET_UNIPORT_MASK:
            pParam->portMask  = pVnetDev0_g->EnetInfo[0].sw.port_map & (~(1 << EPON_PORT_ID));
            break;

        default:
            BCM_ENET_DEBUG("%s: Invalid type \n", __FUNCTION__);
            break;
    }

    return 0;
}
#endif
