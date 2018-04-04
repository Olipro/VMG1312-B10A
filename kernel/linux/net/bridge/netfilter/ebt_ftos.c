/*
 *  ebt_ftos
 *
 *	Authors:
 *	 Song Wang <songw@broadcom.com>
 *
 *  Feb, 2004
 *
 */

// The ftos target can be used in any chain
#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <net/checksum.h>
#include <linux/if_vlan.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_ftos_t.h>

#include <net/dsfield.h>

#define PPPOE_HLEN   6
#define PPP_TYPE_IPV4   0x0021  /* IPv4 in PPP */
#define PPP_TYPE_IPV6   0x0057  /* IPv6 in PPP */

static unsigned int ebt_ftos_tg(struct sk_buff *skb, const struct xt_target_param *par)      
{
	//struct ebt_ftos_t_info *ftosinfo = (struct ebt_ftos_t_info *)data;
	const  struct ebt_ftos_t_info *ftosinfo = par->targinfo;
	struct iphdr *iph = NULL;
	struct ipv6hdr *ipv6h = NULL;
        /* Need to recalculate IP header checksum after altering TOS byte */
	u_int16_t diffs[2];

	/* if VLAN frame, we need to point to correct network header */
   if (skb->protocol == __constant_htons(ETH_P_IP))
      iph = (struct iphdr *)(skb->network_header);
   else if ((skb)->protocol == __constant_htons(ETH_P_IPV6))
      ipv6h = (struct ipv6hdr *)(skb->network_header);
   else if (skb->protocol == __constant_htons(ETH_P_8021Q)) {
      if (*(unsigned short *)(skb->network_header + VLAN_HLEN - 2) == __constant_htons(ETH_P_IP))
         iph = (struct iphdr *)(skb->network_header + VLAN_HLEN);
      else if (*(unsigned short *)(skb->network_header + VLAN_HLEN - 2) == __constant_htons(ETH_P_IPV6))
         ipv6h = (struct ipv6hdr *)(skb->network_header + VLAN_HLEN);
   }
   else if (skb->protocol == __constant_htons(ETH_P_PPP_SES)) {
      if (*(unsigned short *)(skb->network_header + PPPOE_HLEN) == PPP_TYPE_IPV4)
         iph = (struct iphdr *)(skb->network_header + PPPOE_HLEN + 2);
      else if (*(unsigned short *)(skb->network_header + PPPOE_HLEN) == PPP_TYPE_IPV6)
         ipv6h = (struct ipv6hdr *)(skb->network_header + PPPOE_HLEN + 2);
   }
   /* if not IP header, do nothing. */
   if ((iph == NULL) && (ipv6h == NULL))
	   return ftosinfo->target;

   if ( iph != NULL ) //IPv4
   {
	if ((ftosinfo->ftos_set & FTOS_SETFTOS) && (iph->tos != ftosinfo->ftos)) {
#if 1 //__MSTC__, FuChia
                printk("ebt_target_ftos:FTOS_SETFTOS  0x%08x .....\n", ftosinfo->ftos);
#endif
		diffs[0] = htons(iph->tos) ^ 0xFFFF;
		iph->tos = ftosinfo->ftos;
		diffs[1] = htons(iph->tos);
		iph->check = csum_fold(csum_partial((char *)diffs,
		                                    sizeof(diffs),
		                                    iph->check^0xFFFF));		
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// member below is removed
//		(*pskb)->nfcache |= NFC_ALTERED;
	} else if (ftosinfo->ftos_set & FTOS_WMMFTOS) {
#if 1 //__MSTC__, FuChia
	    printk("ebt_target_ftos:FTOS_WMMFTOS .....0x%08x\n", (skb)->mark & 0xf);
#endif
      diffs[0] = htons(iph->tos) ^ 0xFFFF;
      iph->tos |= ((skb->mark >> PRIO_LOC_NFMARK) & PRIO_LOC_NFMASK) << DSCP_MASK_SHIFT;
      diffs[1] = htons(iph->tos);
      iph->check = csum_fold(csum_partial((char *)diffs,
		                                    sizeof(diffs),
		                                    iph->check^0xFFFF));
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// member below is removed
//        (*pskb)->nfcache |= NFC_ALTERED;
	} else if ((ftosinfo->ftos_set & FTOS_8021QFTOS) && skb->protocol == __constant_htons(ETH_P_8021Q)) {
	    
      struct vlan_hdr *frame;	
      unsigned char prio = 0;
      unsigned short TCI;

      frame = (struct vlan_hdr *)(skb->network_header);
      TCI = ntohs(frame->h_vlan_TCI);
      prio = (unsigned char)((TCI >> 13) & 0x7);
#if 1 //__MSTC__, FuChia
        printk("ebt_target_ftos:FTOS_8021QFTOS ..... 0x%08x\n", prio);
#endif
      diffs[0] = htons(iph->tos) ^ 0xFFFF;
      iph->tos |= (prio & 0xf) << DSCP_MASK_SHIFT;
      diffs[1] = htons(iph->tos);
      iph->check = csum_fold(csum_partial((char *)diffs,
		                                    sizeof(diffs),
		                                    iph->check^0xFFFF)); 
// !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
// member below is removed
//        (*pskb)->nfcache |= NFC_ALTERED;
	}
   }
   else //IPv6
   {
      __u8 tos;

      /* TOS consists of priority field and first 4 bits of flow_lbl */
      tos = ipv6_get_dsfield((struct ipv6hdr *)(skb->network_header));

      if ((ftosinfo->ftos_set & FTOS_SETFTOS) && (tos != ftosinfo->ftos))
      {
#if 1 //__MSTC__, FuChia
         printk("ebt_target_ftos:FTOS_SETFTOS .....\n");
#endif
         ipv6_change_dsfield((struct ipv6hdr *)(skb->network_header), 0, ftosinfo->ftos);
      } 
      else if (ftosinfo->ftos_set & FTOS_WMMFTOS) 
      {
         //printk("ebt_target_ftos:FTOS_WMMFTOS .....0x%08x\n", 
	     tos |= ((skb->mark >> PRIO_LOC_NFMARK) & PRIO_LOC_NFMASK) << DSCP_MASK_SHIFT;
         ipv6_change_dsfield((struct ipv6hdr *)(skb->network_header), 0, tos);
      } 
      else if ((ftosinfo->ftos_set & FTOS_8021QFTOS) && 
               skb->protocol == __constant_htons(ETH_P_8021Q)) 
      {
         struct vlan_hdr *frame;	
         unsigned char prio = 0;
         unsigned short TCI;

         frame = (struct vlan_hdr *)(skb->network_header);
         TCI = ntohs(frame->h_vlan_TCI);
         prio = (unsigned char)((TCI >> 13) & 0x7);
#if 1 //__MSTC__, FuChia
         printk("ebt_target_ftos:FTOS_8021QFTOS ..... 0x%08x\n", prio);
#endif
         tos |= (prio & 0xf) << DSCP_MASK_SHIFT;
         ipv6_change_dsfield((struct ipv6hdr *)(skb->network_header), 0, tos);
      }
   }

	return ftosinfo->target;
}

static bool ebt_ftos_tg_check(const struct xt_tgchk_param *par)
{
	const struct ebt_ftos_t_info *info = par->targinfo;
/*
	if (datalen != sizeof(struct ebt_ftos_t_info))
		return -EINVAL;
*/
	if (BASE_CHAIN && info->target == EBT_RETURN)
		return false;
	
	//CLEAR_BASE_CHAIN_BIT;
	
	if (INVALID_TARGET)
		return false;
	
	return true;
}

static struct xt_target ebt_ftos_tg_reg = {
	.name		= EBT_FTOS_TARGET,
	.revision	= 0,
	.family		= NFPROTO_BRIDGE,
	.target		= ebt_ftos_tg,
	.checkentry	= ebt_ftos_tg_check,
	.targetsize	= XT_ALIGN(sizeof(struct ebt_ftos_t_info)),
	.me		= THIS_MODULE,
};

static int __init ebt_ftos_init(void)
{
	int ret;
	ret = xt_register_target(&ebt_ftos_tg_reg);

	if(ret == 0)
		printk(KERN_INFO "ebt_ftos registered\n");

	return ret;
}

static void __exit ebt_ftos_fini(void)
{
	xt_unregister_target(&ebt_ftos_tg_reg);
}

module_init(ebt_ftos_init);
module_exit(ebt_ftos_fini);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Song Wang, songw@broadcom.com");
MODULE_DESCRIPTION("Target to overwrite the full TOS byte in IP header");
