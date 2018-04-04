/* x_tables module for setting the IPv4/IPv6 DSCP field, Version 1.8
 *
 * (C) 2002 by Harald Welte <laforge@netfilter.org>
 * based on ipt_FTOS.c (C) 2000 by Matthew G. Marsh <mgm@paktronix.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * See RFC2474 for a description of the DSCP field within the IP Header.
 *
 * xt_DSCP.c,v 1.8 2002/08/06 18:41:57 laforge Exp
*/

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <net/dsfield.h>
#include <linux/if_vlan.h>

#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_AUTOMAP.h>

MODULE_LICENSE("GPL");
MODULE_ALIAS("ipt_AUTOMAP");
MODULE_ALIAS("ip6t_AUTOMAP");

//static unsigned int target(struct sk_buff **pskb, const struct xt_tgchk_param *par)
static unsigned int AUTOMAP_target(struct sk_buff *skb, const struct xt_target_param *par)
			   //const struct net_device *in,
			   //const struct net_device *out,
			   //unsigned int hooknum,
			   //const struct xt_target *target,
			   //const void *targinfo)
{	
	const struct xt_automap_target_info *aminfo = par->targinfo;
	struct iphdr *ih=NULL;
	int value =0;
	//struct iphdr *iph = ip_hdr(*pskb);
	unsigned char prio = 0;
	unsigned short TCI;
        /* Need to recalculate IP header checksum after altering TOS byte */
	const struct vlan_hdr *fp;
	struct vlan_hdr _frame;
	unsigned short id;	/* VLAN ID, given from frame TCI */
	
	if(aminfo->flags & XT_AUTO_TYPE){
//printk(KERN_ERR "aminfo->type=%d\n\r",aminfo->type);		
		if((aminfo->type&AUTOMAP_TYPE_DSCP)||(aminfo->type&AUTOMAP_TYPE_PKTLEN)){
				//ih = skb_header_pointer(*pskb, 0, sizeof(_iph), &_iph);
				ih = ip_hdr(skb);
		}
#if 1 //__MSTC__, Jeff
		if(ntohs(((struct vlan_hdr *)(skb->vlan_header))->h_vlan_encapsulated_proto) == 0){
#else
		if(ntohs(((struct vlan_hdr *)(skb->vlan_header))->h_vlan_TCI) == 0){
#endif

#if 1 //__MSTC__, Jeff
			if(skb->protocol == ETH_P_8021Q) {
#endif
                 fp = skb_header_pointer(skb, 0, sizeof(_frame), &_frame);
                 if (fp == NULL)
                    return false;
              
              /* Tag Control Information (TCI) consists of the following elements:
              * - User_priority. The user_priority field is three bits in length,
              * interpreted as a binary number.
              * - Canonical Format Indicator (CFI). The Canonical Format Indicator
              * (CFI) is a single bit flag value. Currently ignored.
              * - VLAN Identifier (VID). The VID is encoded as
              * an unsigned binary number. */
                 TCI = ntohs(fp->h_vlan_TCI);
                 id = TCI & VLAN_VID_MASK;
                 prio = (TCI >> 13) & 0x7;
#if 1 //__MSTC__, Jeff
			}
			//Packet with no VLAN tag
			else {
				TCI = 0;
				id = 0;
				//Packet with no VLAN tag will be sent to default queue just like 1p value is 1
				prio = 1;
			}
#endif
              }else{
                 // for new broadcom vlan device
                 TCI = ntohs(((struct vlan_hdr *)(skb->vlan_header))->h_vlan_TCI);
                 id = TCI & VLAN_VID_MASK;
                 prio = (TCI >> 13) & 0x7;
  		   //printk(KERN_ERR "111prio=%d\n\r",prio);
              }
		switch(aminfo->type){
			case AUTOMAP_TYPE_8021P:
				//printk(KERN_ERR "prio=%d\n\r",prio);
				skb->mark|=aminfo->marktable[MapTable[1][prio]];
				break;
			case AUTOMAP_TYPE_DSCP:
				value = ((ih->tos)>>5)&0x7 ;
				//printk("value is %x\n",value);
				skb->mark|=aminfo->marktable[MapTable[0][value]];
				break;
			case AUTOMAP_TYPE_PKTLEN:
				if(ih->tot_len > 1100){
					skb->mark|=aminfo->marktable[2];
				}else if(ih->tot_len < 250){
					skb->mark|=aminfo->marktable[5];
				}else{	/*250~1100*/
					skb->mark|=aminfo->marktable[3];
				}
				break;
			default:
				break;
		}
	}
	return XT_CONTINUE;
}

static bool AUTOMAP_checkentry(const struct xt_tgchk_param *par)
{
	return 1;
}

static struct xt_target xt_auto_target __read_mostly = {
	//{
	       .name		= "AUTOMAP",
	       .revision   = 0,
		.family		= NFPROTO_IPV4,
		.checkentry	= AUTOMAP_checkentry,
		.target		= AUTOMAP_target,
		.targetsize	= sizeof(struct xt_automap_target_info),
		.table		= "mangle",
		.me		= THIS_MODULE,
	//}
	/*,
	{
		.name		= "AUTOMAP",
		.family		= AF_INET6,
		.checkentry	= checkentry,
		.target		= target6,
		.targetsize	= sizeof(struct xt_automap_target_info),
		.me		= THIS_MODULE,
	},*/
};

static int __init xt_automap_target_init(void)
{
	//return xt_register_targets(xt_auto_target, ARRAY_SIZE(xt_auto_target));
	return xt_register_target(&xt_auto_target);
}

static void __exit xt_automap_target_fini(void)
{
	//xt_unregister_targets(xt_auto_target, ARRAY_SIZE(xt_auto_target));
	xt_unregister_target(&xt_auto_target);
}

module_init(xt_automap_target_init);
module_exit(xt_automap_target_fini);
