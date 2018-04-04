
#include <linux/netfilter_ipv4.h>
#include <linux/netfilter_ipv6.h>
#include <linux/if_vlan.h>
#include <linux/netfilter/xt_ether.h>
#include <linux/netfilter/x_tables.h>
#if 1 //__MSTC__, FuChia, QoS
#include <linux/module.h>
#include <linux/skbuff.h>
#endif //__MSTC__, FuChia, QoS

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Xtables: ether match");
MODULE_ALIAS("ipt_ether");
MODULE_ALIAS("ip6t_ether");

#if 1 //__MSTC__, FuChia, QoS
static bool ether_mt(const struct sk_buff *skb, const struct xt_match_param *par)
#else
static int
match(const struct sk_buff *skb,
      const struct net_device *in,
      const struct net_device *out,
      const struct xt_match *match,
      const void *matchinfo,
      int offset,
      unsigned int protoff,
      int *hotdrop)
#endif //__MSTC__, FuChia, QoS
{
#if 1 //__MSTC__, FuChia, QoS
	const struct xt_ether_info *info = par->matchinfo;
#else
	const struct xt_ether_info *info = matchinfo;
#endif //__MSTC__, FuChia, QoS
	struct ethhdr *eth = NULL;
	struct vlan_hdr _frame, *fp;
	unsigned short TCI;	/* Whole TCI, given from parsed frame */
	unsigned short id;	/* VLAN ID, given from frame TCI */
	unsigned char prio;	/* user_priority, given from frame TCI */
#ifdef CONFIG_CLASS_DOWNSTREAM_8021Q  //__MSTC__, Jeff
	unsigned short savedEtherType = skb->vtag_word>>16;
#endif
		
	if(info->bitmask & IPT_ETHER_TYPE){
		eth = eth_hdr(skb);
		//printk("\n\nethe->type is 0x%x\n",eth->h_proto);
		//printk("info->ethertype is 0x%x\n",info->ethertype);
		//printk("result is %d\n",((eth->h_proto != info->ethertype) ^ !!(info->invert & IPT_ETHER_TYPE)));
#if 1 //__MSTC__, FuChia, QoS
    /*Valid this packet first.*/
		if(!(skb_mac_header(skb) >= skb->head && skb_mac_header(skb) + ETH_HLEN <= skb->data))
		  return False;
#endif //__MSTC__, FuChia, QoS
#ifdef CONFIG_CLASS_DOWNSTREAM_8021Q  //__MSTC__, Jeff
		if(info->ethertype == ETH_P_8021Q && skb->protocol != ETH_P_8021Q)
		{
			/* for new broadcom vlan device */
			if( (!(ntohs(((struct vlan_hdr *)(skb->vlan_header))->h_vlan_encapsulated_proto))) ^ !!(info->invert & IPT_ETHER_TYPE))
			{
				if(savedEtherType != ETH_P_8021Q)
					return False;
			}
		}
		else
		{
#endif
#if 1 //__AT&T__, FuChia, QoS
      if((skb->protocol!= info->ethertype) ^ !!(info->invert & IPT_ETHER_TYPE))
#else
		if((eth->h_proto != info->ethertype) ^ !!(info->invert & IPT_ETHER_TYPE))
#endif //__AT&T__, FuChia, QoS
			return False;
#ifdef CONFIG_CLASS_DOWNSTREAM_8021Q  //__MSTC__, Jeff
		}
#endif
	}

#ifdef CONFIG_CLASS_DOWNSTREAM_8021Q  //__MSTC__, Jeff
	TCI = (unsigned short) skb->vtag_word;
	if(savedEtherType == 0 && TCI == 0){
		if(ntohs(((struct vlan_hdr *)(skb->vlan_header))->h_vlan_encapsulated_proto) == 0){
			if(skb->protocol == ETH_P_8021Q) {
				fp = skb_header_pointer(skb, 0, sizeof(_frame), &_frame);
				if (fp == NULL)
					return False;
				TCI = ntohs(fp->h_vlan_TCI);
			}
		}
		else{
			TCI = ntohs(((struct vlan_hdr *)(skb->vlan_header))->h_vlan_TCI);
		}
	}
#else
	if(skb->vtag_save==0){
  		fp = skb_header_pointer(skb, 0, sizeof(_frame), &_frame);
		if (fp == NULL)
			return False;
		TCI = ntohs(fp->h_vlan_TCI);
	}
	else{
		TCI = skb->vtag_save;
	}
#endif
	if(info->bitmask & IPT_ETHER_8021Q){
		id = TCI & VLAN_VID_MASK;
		//printk("id is 0x%x\n",id);
		//printk("info->vlanid is 0x%x\n",info->vlanid);
		//printk("result is %d\n",((id != info->vlanid) ^ !!(info->invert & IPT_ETHER_8021Q)));
		if((id != info->vlanid) ^ !!(info->invert & IPT_ETHER_8021Q))
			return False;
	}
	if(info->bitmask & IPT_ETHER_8021P){
		prio = (TCI >> 13) & 0x7;
		//printk("prio  is 0x%x\n",prio );
		//printk("info->vlanpriority is 0x%x\n",info->vlanpriority);
		//printk("result is %d\n",((prio != info->vlanpriority) ^ !!(info->invert & IPT_ETHER_8021P)));
		if((prio != info->vlanpriority) ^ !!(info->invert & IPT_ETHER_8021P))
			return False;
	}

	return True;
}

#if 1 //__MSTC__, FuChia, QoS
static struct xt_match xt_ether_match[] __read_mostly = {
#else
static struct xt_match xt_ether_match[] = {
#endif //__Verizon, FuChia, QoS
	{
		.name		= "ether",
#if 1 //__MSTC__, FuChia, QoS
  .family		= NFPROTO_UNSPEC,
		.match		= ether_mt,

#else
		.family		= AF_INET,
		.match		= match,
#endif //__MSTC__, FuChia, QoS
		.matchsize	= sizeof(struct xt_ether_info),
#if 1 //__MSTC__, FuChia, QoS
		.hooks     = (1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_LOCAL_OUT) |
	               (1 << NF_INET_FORWARD) | (1 << NF_INET_POST_ROUTING),
#else
		.hooks		= (1 << 0/*NF_IP_PRE_ROUTING*/) |
				  (1 << 3/*NF_IP_LOCAL_OUT*/) |
				  (1 << 2/*NF_IP_FORWARD*/),
#endif //__MSTC__, FuChia, QoS
		.me		= THIS_MODULE,
	},
	{
		.name		= "ether",
#if 1 //__MSTC__, FuChia, QoS
  .family		= NFPROTO_UNSPEC,
		.match		= ether_mt,

#else
		.family		= AF_INET,
		.match		= match,
#endif //__MSTC__, FuChia, QoS
		.matchsize	= sizeof(struct xt_ether_info),
#if 1 //__MSTC__, FuChia, QoS
		.hooks     = (1 << NF_INET_PRE_ROUTING) | (1 << NF_INET_LOCAL_OUT) |
	               (1 << NF_INET_FORWARD) | (1 << NF_INET_POST_ROUTING),
#else
		.hooks		= (1 << 0/*NF_IP_PRE_ROUTING*/) |
				  (1 << 3/*NF_IP_LOCAL_OUT*/) |
				  (1 << 2/*NF_IP_FORWARD*/),
#endif //__MSTC__, FuChia, QoS
		.me		= THIS_MODULE,
	},
};


static int __init xt_ether_init(void)
{
	return xt_register_matches(xt_ether_match, ARRAY_SIZE(xt_ether_match));
}

static void __exit xt_ether_fini(void)
{
	xt_unregister_matches(xt_ether_match, ARRAY_SIZE(xt_ether_match));
}

module_init(xt_ether_init);
module_exit(xt_ether_fini);
