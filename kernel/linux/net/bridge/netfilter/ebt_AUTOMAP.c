/* Kernel module to control the rate in kbps. */
/* This program is free software; you can redistribute it and/or modify 
 * it under the terms of the GNU General Public License version 2 as 
 * published by the Free Software Foundation. */
/*  MitraStar Jeff, 20110114*/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/if_vlan.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_AUTOMAP.h>


static unsigned int
ebt_automap_tg(struct sk_buff *skb, const struct xt_target_param *par) 
{
	const struct ebt_automap_t_info *aminfo = par->targinfo;
	struct iphdr *ih=NULL;
	int value =0;
	unsigned char prio = 0;
	unsigned short TCI = 0;
	const struct vlan_hdr *fp;
	struct vlan_hdr _frame;
	unsigned short id = 0;  /* VLAN ID, given from frame TCI */

	if((aminfo->type&AUTOMAP_TYPE_DSCP)||(aminfo->type&AUTOMAP_TYPE_PKTLEN)){
		if (skb->protocol == __constant_htons(ETH_P_IP))
			ih = (struct iphdr *)(skb->network_header);
		else if (skb->protocol == __constant_htons(ETH_P_8021Q)) {
			if (*(unsigned short *)(skb->network_header + VLAN_HLEN - 2) == __constant_htons(ETH_P_IP))
				ih = (struct iphdr *)(skb->network_header + VLAN_HLEN);
		}
	}
	if(ntohs(((struct vlan_hdr *)(skb->vlan_header))->h_vlan_encapsulated_proto) == 0){
		if(skb->protocol == ETH_P_8021Q) {
			fp = skb_header_pointer(skb, 0, sizeof(_frame), &_frame);
			if (fp == NULL)
				return EBT_CONTINUE;
			/* Tag Control Information (TCI) consists of the following elements:
			 * - User_priority. The user_priority field is three bits in length,
			 *   interpreted as a binary number.
			 * - Canonical Format Indicator (CFI). The Canonical Format Indicator
			 *   (CFI) is a single bit flag value. Currently ignored.
			 * - VLAN Identifier (VID). The VID is encoded as
			 *   an unsigned binary number. */
			TCI = ntohs(fp->h_vlan_TCI);
			id = TCI & VLAN_VID_MASK;
			prio = (TCI >> 13) & 0x7;
		}
		//Packet with no VLAN tag
		else {
			TCI = 0;
			id = 0;
			//Packet with no VLAN tag will be sent to default queue just like 1p value is 1
			prio = 1;
		}
	}
	else {
		// for new broadcom vlan device
		TCI = ntohs(((struct vlan_hdr *)(skb->vlan_header))->h_vlan_TCI);
		id = TCI & VLAN_VID_MASK;
		prio = (TCI >> 13) & 0x7;
	}

	switch(aminfo->type){
		case AUTOMAP_TYPE_8021P:
			skb->mark|=aminfo->marktable[MapTable[1][prio]];
			break;
		case AUTOMAP_TYPE_DSCP:
			if(ih==NULL)
				skb->mark|=0x0;
			else {
				value = ((ih->tos)>>5)&0x7 ;
				skb->mark|=aminfo->marktable[MapTable[0][value]];
			}
			break;
		case AUTOMAP_TYPE_PKTLEN:
			if(ih==NULL)
				skb->mark|=0x0;
			else {
				if(ih->tot_len > 1100){
					skb->mark|=aminfo->marktable[2];
				}else if(ih->tot_len < 250){
					skb->mark|=aminfo->marktable[5];
				}else{  /*250~1100*/
					skb->mark|=aminfo->marktable[3];
				}
			}
			break;
		default:
			break;
	}

	return EBT_CONTINUE;
}

/* As a policer rule added, this function will be executed */ 
static bool ebt_automap_tg_check(const struct xt_tgchk_param *par)
{
	return 1;
}

static struct xt_target ebt_automap_tg_reg __read_mostly =
{
    .name = EBT_AUTOMAP_TARGET,
    .revision	= 0,
    .family		= NFPROTO_BRIDGE,
    .target  = ebt_automap_tg,
    .checkentry  = ebt_automap_tg_check,
    .targetsize	= XT_ALIGN(sizeof(struct ebt_automap_t_info)),
    .me     = THIS_MODULE,
};

static int __init ebt_automap_init(void)
{
   return xt_register_target(&ebt_automap_tg_reg);
}

static void __exit ebt_automap_fini(void)
{
   xt_unregister_target(&ebt_automap_tg_reg);
}

module_init(ebt_automap_init);
module_exit(ebt_automap_fini);
MODULE_LICENSE("GPL");

