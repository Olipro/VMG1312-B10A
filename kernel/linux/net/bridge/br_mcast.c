/*
*    Copyright (c) 2012 Broadcom Corporation
*    All Rights Reserved
*
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

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/spinlock.h>
#include <linux/times.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/jhash.h>
#include <asm/atomic.h>
#include <linux/ip.h>
#include <linux/seq_file.h>
#include <linux/proc_fs.h>
#include <linux/list.h>
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
#include <linux/if_vlan.h>
#include <linux/blog.h>
#include <linux/blog_rule.h>
#endif
#include <linux/rtnetlink.h>
#include "br_private.h"
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
#include "br_igmp.h"
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
#include "br_mld.h"
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
#include <linux/module.h>
#endif
#endif
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
#include <linux/bcm_skb_defines.h>
#endif
#if defined(CONFIG_11ac_throughput_patch_from_412L07)

#if defined(CONFIG_MIPS_BRCM) && (defined(CONFIG_BR_IGMP_SNOOP) || defined(CONFIG_BR_MLD_SNOOP))
static t_MCAST_CFG multiConfig = { -1 };

void br_mcast_set_pri_queue(int val)
{
   multiConfig.mcastPriQueue = val;
}

int br_mcast_get_pri_queue(void)
{
   return multiConfig.mcastPriQueue;
}

void br_mcast_set_skb_mark_queue(struct sk_buff *skb)
{
   int isMulticast = 0;
   const unsigned char *dest = eth_hdr(skb)->h_dest;

#if defined(CONFIG_BR_MLD_SNOOP)
   if((BR_MLD_MULTICAST_MAC_PREFIX == dest[0]) && 
      (BR_MLD_MULTICAST_MAC_PREFIX == dest[1])) {
      isMulticast = 1;
   }
#endif

#if defined(CONFIG_BR_IGMP_SNOOP)
   if (is_multicast_ether_addr(dest)) {
      isMulticast = 1;
   }
#endif

   if ( (isMulticast) && (multiConfig.mcastPriQueue != -1) )
   {
      skb->mark = SKBMARK_SET_Q(skb->mark, multiConfig.mcastPriQueue);
   }
}
#endif
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)

inline void br_mcast_ipv4_to_eth(unsigned long ipv4_addr,
                                       unsigned char *mac_addr_p)
{
    unsigned char *ipv4_addr8_p = (unsigned char *)(&ipv4_addr);

    mac_addr_p[0] = 0x01;
    mac_addr_p[1] = 0x00;
    mac_addr_p[2] = 0x5E;
    mac_addr_p[3] = ipv4_addr8_p[1] & 0x7F;
    mac_addr_p[4] = ipv4_addr8_p[2];
    mac_addr_p[5] = ipv4_addr8_p[3];
}

inline void br_mcast_ipv6_to_eth(unsigned char *ipv6_addr,
                                       unsigned char *mac_addr_p)
{
    mac_addr_p[0] = 0x33;
    mac_addr_p[1] = 0x33;
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
    mac_addr_p[2] = ipv6_addr[12];
    mac_addr_p[3] = ipv6_addr[13];
    mac_addr_p[4] = ipv6_addr[14];
    mac_addr_p[5] = ipv6_addr[15];
#else

    mac_addr_p[2] = *(ipv6_addr + 13);
    mac_addr_p[3] = *(ipv6_addr + 14);
    mac_addr_p[4] = *(ipv6_addr + 15);
    mac_addr_p[5] = *(ipv6_addr + 16);
#endif
}

void br_mcast_blog_release(t_BR_MCAST_PROTO_TYPE proto, void *mc_fdb)
{
	Blog_t *blog_p = BLOG_NULL;
	uint32_t blog_idx = BLOG_KEY_INVALID;
	BlogTraffic_t traffic;

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
	if(proto == BR_MCAST_PROTO_IGMP)
	{
		blog_idx =  ((struct net_bridge_mc_fdb_entry *)mc_fdb)->blog_idx;
		((struct net_bridge_mc_fdb_entry *)mc_fdb)->blog_idx = 0;
		traffic = BlogTraffic_IPV4_MCAST;
	}
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	if(proto == BR_MCAST_PROTO_MLD)
	{
		blog_idx =  ((struct net_br_mld_mc_fdb_entry *)mc_fdb)->blog_idx;
		((struct net_br_mld_mc_fdb_entry *)mc_fdb)->blog_idx = 0;
		traffic = BlogTraffic_IPV6_MCAST;
	}
#endif

	if(BLOG_KEY_INVALID == blog_idx)
		return;
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
	blog_p = blog_deactivate(blog_idx, traffic, BlogClient_fcache);
#else
	blog_p = blog_deactivate(blog_idx, traffic);
#endif
	if ( blog_p )
	{
		blog_rule_free_list(blog_p);
		blog_put(blog_p);
	}

	return;
}

static void br_mcast_blog_process_wan(blogRule_t *rule_p,
                                     void *mc_fdb,
                                     t_BR_MCAST_PROTO_TYPE proto,
                                     struct net_device **wan_dev_pp,
                                     struct net_device **wan_vlan_dev_pp)
{
	blogRuleAction_t ruleAction;
	struct net_device *dev_p = NULL;
	struct net_bridge_mc_fdb_entry *igmp_fdb = NULL;
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	struct net_br_mld_mc_fdb_entry *mld_fdb = NULL;
#endif
	uint8_t                *dev_addr = NULL;
	uint32_t phyType;
	char wan_ops;

	if(!mc_fdb)
		return;

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	if(BR_MCAST_PROTO_MLD == proto)
	{
		mld_fdb  = (struct net_br_mld_mc_fdb_entry *)mc_fdb;
		dev_p    = mld_fdb->from_dev;
		dev_addr = mld_fdb->dst->dev->dev_addr;
		wan_ops  = mld_fdb->type;
	}
	else
#endif
	{
		igmp_fdb = (struct net_bridge_mc_fdb_entry *)mc_fdb;
		dev_p    = igmp_fdb->from_dev;
		dev_addr = igmp_fdb->dst->dev->dev_addr;
		wan_ops  = igmp_fdb->type;
	}

	while(1)
	{
		if(netdev_path_is_root(dev_p))
		{
			*wan_dev_pp = dev_p;
			break;
		}

		if(dev_p->priv_flags & IFF_PPP)
		{
			rule_p->filter.hasPppoeHeader = 1;
			memset(&ruleAction, 0, sizeof(blogRuleAction_t));
			ruleAction.cmd = BLOG_RULE_CMD_POP_PPPOE_HDR;
			blog_rule_add_action(rule_p, &ruleAction);

			memset(&ruleAction, 0, sizeof(blogRuleAction_t));
			ruleAction.cmd = BLOG_RULE_CMD_SET_MAC_DA;
			if(igmp_fdb)
				br_mcast_ipv4_to_eth(igmp_fdb->grp.s_addr, ruleAction.macAddr);
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
			else
				br_mcast_ipv6_to_eth(mld_fdb->grp.s6_addr, ruleAction.macAddr);
#endif
			blog_rule_add_action(rule_p, &ruleAction);
		}
		else if(*wan_vlan_dev_pp == NULL &&
		        dev_p->priv_flags & IFF_BCM_VLAN)
		{
			*wan_vlan_dev_pp = dev_p;
		}
		dev_p = netdev_path_next_dev(dev_p);
	}

	/* For IPoA */
	phyType = netdev_path_get_hw_port_type(*wan_dev_pp);
	phyType = BLOG_GET_HW_ACT(phyType);
	if((phyType == VC_MUX_IPOA) || (phyType == LLC_SNAP_ROUTE_IP))
	{
		memset(&ruleAction, 0, sizeof(blogRuleAction_t));
		ruleAction.cmd = BLOG_RULE_CMD_SET_MAC_DA;
		if(igmp_fdb)
			br_mcast_ipv4_to_eth(igmp_fdb->grp.s_addr, ruleAction.macAddr);
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
		else
			br_mcast_ipv6_to_eth(mld_fdb->grp.s6_addr, ruleAction.macAddr);
#endif
		blog_rule_add_action(rule_p, &ruleAction);
	}

	if(wan_ops == MCPD_IF_TYPE_ROUTED)
	{
		memset(&ruleAction, 0, sizeof(blogRuleAction_t));
		ruleAction.cmd = BLOG_RULE_CMD_SET_MAC_SA;
		memcpy(ruleAction.macAddr, dev_addr, ETH_ALEN);
		blog_rule_add_action(rule_p, &ruleAction);

		memset(&ruleAction, 0, sizeof(blogRuleAction_t));
		ruleAction.cmd = BLOG_RULE_CMD_DECR_TTL;
		blog_rule_add_action(rule_p, &ruleAction);
	}
}

static void br_mcast_blog_process_lan(void *mc_fdb,
                                     t_BR_MCAST_PROTO_TYPE proto,
                                     struct net_device **lan_dev_pp,
                                     struct net_device **lan_vlan_dev_pp)
{
    struct net_device *dev_p = NULL;
    struct net_bridge_mc_fdb_entry *igmp_fdb = NULL;
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
    struct net_br_mld_mc_fdb_entry *mld_fdb = NULL;
#endif

    if(!mc_fdb)
        return;

#if defined(CONFIG_11ac_throughput_patch_from_412L07)
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
    if (BR_MCAST_PROTO_MLD == proto )
    {
        mld_fdb = (struct net_br_mld_mc_fdb_entry *)mc_fdb;
        dev_p = mld_fdb->dst->dev;
    }
    else
#endif
    {
        igmp_fdb = (struct net_bridge_mc_fdb_entry *)mc_fdb;
        dev_p    = igmp_fdb->dst->dev;
    }
#else
    if(BR_MCAST_PROTO_IGMP == proto)
    {
        igmp_fdb = (struct net_bridge_mc_fdb_entry *)mc_fdb;

        dev_p = igmp_fdb->dst->dev;
    }
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
    else
    {
        mld_fdb = (struct net_br_mld_mc_fdb_entry *)mc_fdb;

        dev_p = mld_fdb->dst->dev;
    }
#endif
#endif

    while(1)
    {
        if(netdev_path_is_root(dev_p))
        {
            *lan_dev_pp = dev_p;
            break;
        }

        if(*lan_vlan_dev_pp == NULL &&
           dev_p->priv_flags & IFF_BCM_VLAN)
        {
            *lan_vlan_dev_pp = dev_p;
        }

        dev_p = netdev_path_next_dev(dev_p);
    }
}

static void br_mcast_mc_fdb_update_bydev(t_BR_MCAST_PROTO_TYPE proto, 
                                         struct net_bridge *br,
                                         struct net_device *dev)
{
	if(!br || !dev)
		return;

	if(BR_MCAST_PROTO_IGMP == proto)
		br_igmp_mc_fdb_update_bydev(br, dev);
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	else if(BR_MCAST_PROTO_MLD == proto)
		br_mld_mc_fdb_update_bydev(br, dev);
#endif

	return;
}

void br_mcast_vlan_notify_for_blog_update(struct net_device *ndev,
                                          blogRuleVlanNotifyDirection_t direction,
                                          uint32_t nbrOfTags)
{
	struct net_bridge *br = NULL;
	struct net_device *dev = NULL;

	if((ndev->priv_flags & IFF_WANDEV) && (direction == DIR_TX))
		return;

	read_lock(&dev_base_lock);
	for(dev = first_net_device(&init_net); 
	    dev; 
	    dev = next_net_device(dev)) 
	{
		br = netdev_priv(dev);
		if((dev->priv_flags & IFF_EBRIDGE) && (br))
		{
			/* snooping entries could be present even if snooping is
			   disabled, update existing entries */
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
			spin_lock_bh(&br->lock);
			br_mcast_mc_fdb_update_bydev(BR_MCAST_PROTO_IGMP, br, ndev);
			spin_unlock_bh(&br->lock);
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
			spin_lock_bh(&br->lock);
			br_mcast_mc_fdb_update_bydev(BR_MCAST_PROTO_MLD, br, ndev);
			spin_unlock_bh(&br->lock);
#endif /* CONFIG_BR_MLD_SNOOP */
		}
	}
	read_unlock(&dev_base_lock);

	return;
}

void br_mcast_handle_netdevice_events(struct net_device *ndev, unsigned long event)
{
    struct net_bridge *br = NULL;
    struct net_device *dev = NULL;
	int i;

    switch (event) {
        case NETDEV_GOING_DOWN:
        case NETDEV_CHANGE:
            for(dev = first_net_device(&init_net); 
                dev; 
                dev = next_net_device(dev)) {
                br = netdev_priv(dev);
                if((dev->priv_flags & IFF_EBRIDGE) && (br)) {
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
                    struct net_br_mld_mc_fdb_entry *mld_dst;
                    struct net_br_mld_mc_rep_entry *mldrep, *mldrep_n;
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
                    /* snooping entries could be present even if snooping is
                       disabled, update existing entries */
                    struct net_bridge_mc_fdb_entry *dst;
                    struct net_bridge_mc_rep_entry *rep_entry, *rep_entry_n;
                    spin_lock_bh(&br->lock);
                    spin_lock_bh(&br->mcl_lock);
                    for (i = 0; i < BR_IGMP_HASH_SIZE; i++) 
                    {
                        struct hlist_node *h, *n;
                        hlist_for_each_entry_safe(dst, h, n, &br->mc_hash[i], hlist) 
                        {
                    if((!memcmp(ndev->name, dst->wan_name, IFNAMSIZ)) ||
                        (!memcmp(ndev->name, dst->lan_name, IFNAMSIZ)) ||
                        (!memcmp(ndev->name, dev->name, IFNAMSIZ))) {
                            if ( br->igmp_snooping )
                            {
                               mcpd_nl_send_igmp_purge_entry(dst);
                            }
                            list_for_each_entry_safe(rep_entry, 
                                     rep_entry_n, &dst->rep_list, list) {
                                list_del(&rep_entry->list);
                                        br_igmp_mc_rep_free(rep_entry);
                            }
                                hlist_del(&dst->hlist);
                            br_mcast_blog_release(BR_MCAST_PROTO_IGMP, dst);
                                br_igmp_mc_fdb_free(dst);
                            }
                        }
                    }
                    spin_unlock_bh(&br->mcl_lock);
                    spin_unlock_bh(&br->lock);
#endif
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
                    /* snooping entries could be present even if snooping is
                       disabled, update existing entries */
                    spin_lock_bh(&br->lock);
                    spin_lock_bh(&br->mld_mcl_lock);
                    for (i = 0; i < BR_MLD_HASH_SIZE; i++) 
                    {
                        struct hlist_node *h, *n;
                        hlist_for_each_entry_safe(mld_dst, h, n, &br->mld_mc_hash[i], hlist) 
                        {
                    if((!memcmp(ndev->name, mld_dst->wan_name, IFNAMSIZ)) ||
                        (!memcmp(ndev->name, mld_dst->lan_name, IFNAMSIZ)) ||
                        (!memcmp(ndev->name, dev->name, IFNAMSIZ))) {
                                list_for_each_entry_safe(mldrep, 
                                                        mldrep_n, &mld_dst->rep_list, list) {
                                    list_del(&mldrep->list);
                                    br_mld_mc_rep_free(mldrep);
                            }
                                hlist_del(&mld_dst->hlist);
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
                                    br_mld_wl_del_entry(br,mld_dst);
#endif
                            br_mcast_blog_release(BR_MCAST_PROTO_MLD, mld_dst);
                                br_mld_mc_fdb_free(mld_dst);
                            }
                        }
                    }
                    spin_unlock_bh(&br->mld_mcl_lock);
                    spin_unlock_bh(&br->lock);
#endif
                }
            }
            break;
    }

    return;
}

static void *br_mcast_mc_fdb_copy(t_BR_MCAST_PROTO_TYPE proto,
								struct net_bridge *br, 
                                const void *mc_fdb)
{
    if(!mc_fdb)
        return NULL;

    if(BR_MCAST_PROTO_IGMP == proto)
        return br_igmp_mc_fdb_copy(br, (struct net_bridge_mc_fdb_entry *)mc_fdb);
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
    else if(BR_MCAST_PROTO_MLD == proto)
        return br_mld_mc_fdb_copy(br, (struct net_br_mld_mc_fdb_entry *)mc_fdb);
#endif

	return NULL;
}

static void br_mcast_mc_fdb_del_entry(t_BR_MCAST_PROTO_TYPE proto, 
                               struct net_bridge *br, 
                               void *mc_fdb)
{

    if(!mc_fdb)
        return;

    if(BR_MCAST_PROTO_IGMP == proto)
        br_igmp_mc_fdb_del_entry(br, (struct net_bridge_mc_fdb_entry *)mc_fdb);
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
    else if(BR_MCAST_PROTO_MLD == proto)
        br_mld_mc_fdb_del_entry(br, (struct net_br_mld_mc_fdb_entry *)mc_fdb);
#endif

    return;
} /* br_mcast_mc_fdb_del_entry */

static void br_mcast_blog_link_devices(Blog_t *blog_p, struct net_device *rxDev, 
                                       struct net_device *txDev, int wanType )
{
    struct net_device *dev_p;
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
    uint32_t delta;
    struct net_device *rxPath[MAX_VIRT_DEV];
    int rxPathIdx = 0;
    int i;

    /* save rx path required for reverse path traversal for delta calc */
    memset(&rxPath[0], 0, (MAX_VIRT_DEV * sizeof(struct net_device *)));
#endif

    dev_p = rxDev;
    while(1)
    {
        if(netdev_path_is_root(dev_p))
        {
            break;
        }
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
        rxPath[rxPathIdx] = dev_p;
        rxPathIdx++;
#else
        blog_link(IF_DEVICE, blog_p, dev_p, DIR_RX, blog_p->tx.pktlen);
#endif
        dev_p = netdev_path_next_dev(dev_p);
    }
#if defined(CONFIG_11ac_throughput_patch_from_412L07)

    /* omit Ethernet header from virtual dev RX stats */
    delta = BLOG_ETH_HDR_LEN;

    for(i = (MAX_VIRT_DEV-1); i >= 0; i--)
    {
        if(NULL == rxPath[i])
        {
            continue;
        }

        if ( rxPath[i]->priv_flags & IFF_PPP )
        {
            delta += BLOG_PPPOE_HDR_LEN;
        }

        if ( rxPath[i]->priv_flags & IFF_802_1Q_VLAN )
        {
            delta += BLOG_VLAN_HDR_LEN;
        }

        if ( (rxPath[i]->priv_flags & IFF_BCM_VLAN) && 
             (blog_p->vtag_num > 0) )
        {
            delta += BLOG_VLAN_HDR_LEN;
        }

        blog_link(IF_DEVICE_MCAST, blog_p, rxPath[i], DIR_RX, delta);
        dev_p = netdev_path_next_dev(dev_p);
    }

    /* include Ethernet header in virtual TX stats */
    delta -= BLOG_ETH_HDR_LEN;

    if ( (wanType == MCPD_IF_TYPE_ROUTED) && (txDev->br_port) )
    {
       /* routed packets will come through br_dev_xmit, link bridge
          device with blog */
       blog_link(IF_DEVICE_MCAST, blog_p, txDev->br_port->br->dev, 
                 DIR_TX, delta );
    }
#else
    if ( (wanType == MCPD_IF_TYPE_ROUTED) && (txDev->br_port) )
    {
       /* routed packets will come through br_dev_xmit, link bridge
          device with blog */
       blog_link(IF_DEVICE, blog_p, txDev->br_port->br->dev, 
                 DIR_TX, blog_p->tx.pktlen);
    }
#endif
    dev_p = txDev;
    while(1)
    {
        if(netdev_path_is_root(dev_p))
        {
            break;
        }
#if defined(CONFIG_11ac_throughput_patch_from_412L07)

        if ( dev_p->priv_flags & IFF_802_1Q_VLAN )
        {
            delta -= BLOG_VLAN_HDR_LEN;
        }

        if ( dev_p->priv_flags & IFF_BCM_VLAN )
        {
            delta -= BLOG_VLAN_HDR_LEN;
        }

        blog_link(IF_DEVICE_MCAST, blog_p, dev_p, DIR_TX, delta);
#else
        blog_link(IF_DEVICE, blog_p, dev_p, DIR_TX, blog_p->tx.pktlen);
#endif
        dev_p = netdev_path_next_dev(dev_p);
    }
}


static int br_mcast_vlan_process(struct net_bridge     *br,
                                 void                  *mc_fdb,
                                 t_BR_MCAST_PROTO_TYPE  proto,
                                 Blog_t                *blog_p)
{
    Blog_t           *new_blog_p;
    void             *new_mc_fdb = NULL;
    blogRule_t       *rule_p = NULL;
    int               firstRule = 1;
    uint32_t          vid = 0;
    blogRuleFilter_t *rule_filter = NULL;
    BlogTraffic_t     traffic;
    int               activates = 0;
    void             *rxDev;
    void             *txDev;
    int               wanType;

    if(!mc_fdb || !blog_p || !br)
        return 0;

    if(!((BR_MCAST_PROTO_IGMP == proto) || (BR_MCAST_PROTO_MLD == proto)))
        return 0;

    firstRule = 1;
    rule_p = (blogRule_t *)blog_p->blogRule_p;
    while( rule_p )
    {
        blogRuleFilter_t *filter_p;

        filter_p = &rule_p->filter;
        /* if there is a rule that specifies a protocol filter that does not match
           blog key protocol skip it */
        if(blog_rule_filterInUse(filter_p->ipv4.mask.ip_proto))
        {
            if(filter_p->ipv4.mask.ip_proto & BLOG_RULE_IP_PROTO_MASK)
            {
                uint8_t proto;

                proto = filter_p->ipv4.value.ip_proto >> BLOG_RULE_IP_PROTO_SHIFT;
                if (proto != blog_p->key.protocol)
                {
                    /* skip this rule */
                    blog_p->blogRule_p = rule_p->next_p;
                    blog_rule_free(rule_p);
                    rule_p = blog_p->blogRule_p;
                    continue;
                }
            }
        }

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
        if(blog_rule_filterInUse(filter_p->ipv6.mask.nxtHdr))
        {
            if(filter_p->ipv6.mask.nxtHdr & BLOG_RULE_IP6_NXT_HDR_MASK)
            {
                uint8_t nxtHdr;

                nxtHdr = filter_p->ipv6.value.nxtHdr >> BLOG_RULE_IP6_NXT_HDR_SHIFT;
                if (nxtHdr != blog_p->key.protocol)
                {
                    /* skip this rule */
                    blog_p->blogRule_p = rule_p->next_p;
                    blog_rule_free(rule_p);
                    rule_p = blog_p->blogRule_p;
                    continue;
                }
            }
        }
#endif

        /* create new fdb entry unless this is the first rule. For the
           first rule use the fdb entry that was passed in */
        if ( 1 == firstRule )
        {
            firstRule  = 0;
            new_mc_fdb = mc_fdb;
        }
        else
        {
            new_mc_fdb = br_mcast_mc_fdb_copy(proto, br , mc_fdb);
            if(!new_mc_fdb)
            {
                printk(KERN_WARNING "%s new_mc_fdb allocation failed\n",__FUNCTION__);
                break;
            }
        }

        /* get a new blog and copy original blog */
        new_blog_p = blog_get();
        if(new_blog_p == BLOG_NULL) {
            br_mcast_mc_fdb_del_entry(proto, br, new_mc_fdb);
            break;
        }
        blog_copy(new_blog_p, blog_p);

        /* pop the rule off the original blog now that a new fdb and blog have been
           allocated. This is to ensure that all rules are freed in case of error */
        blog_p->blogRule_p = rule_p->next_p;
        rule_p->next_p = NULL;
        new_blog_p->blogRule_p = rule_p;

        rule_filter = &(((blogRule_t *)new_blog_p->blogRule_p)->filter);
        new_blog_p->vtag_num = rule_filter->nbrOfVlanTags;
        vid = ((rule_filter->vlan[0].value.h_vlan_TCI &
                rule_filter->vlan[0].mask.h_vlan_TCI) & 0xFFF);
        new_blog_p->vid = vid ? vid : 0xFFFF; 
        vid = ((rule_filter->vlan[1].value.h_vlan_TCI &
                rule_filter->vlan[1].mask.h_vlan_TCI) & 0xFFF);
        new_blog_p->vid |= vid ? (vid << 16) : 0xFFFF0000;

        blog_link(MCAST_FDB, new_blog_p, (void *)new_mc_fdb, 0, 0);

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
        if(BR_MCAST_PROTO_MLD == proto)
        {
            traffic = BlogTraffic_IPV6_MCAST;
            ((struct net_br_mld_mc_fdb_entry *)new_mc_fdb)->wan_tci = new_blog_p->vid;
            ((struct net_br_mld_mc_fdb_entry *)new_mc_fdb)->num_tags = new_blog_p->vtag_num;
            rxDev   = ((struct net_br_mld_mc_fdb_entry *)new_mc_fdb)->from_dev;
            txDev   = ((struct net_br_mld_mc_fdb_entry *)new_mc_fdb)->dst->dev;
            wanType = ((struct net_br_mld_mc_fdb_entry *)new_mc_fdb)->type;
        }
        else//IGMP
#endif
        {
            traffic = BlogTraffic_IPV4_MCAST;
            ((struct net_bridge_mc_fdb_entry *)new_mc_fdb)->wan_tci = new_blog_p->vid;
            ((struct net_bridge_mc_fdb_entry *)new_mc_fdb)->num_tags = new_blog_p->vtag_num;
            rxDev   = ((struct net_bridge_mc_fdb_entry *)new_mc_fdb)->from_dev;
            txDev   = ((struct net_bridge_mc_fdb_entry *)new_mc_fdb)->dst->dev;
            wanType = ((struct net_bridge_mc_fdb_entry *)new_mc_fdb)->type;
        }

        br_mcast_blog_link_devices(new_blog_p, rxDev, txDev, wanType);
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
        if ( blog_activate(new_blog_p, traffic, BlogClient_fcache) == BLOG_KEY_INVALID )
#else
        if ( blog_activate(new_blog_p, traffic) == BLOG_KEY_INVALID )
#endif
        {
            blog_rule_free_list(new_blog_p);
            blog_put(new_blog_p);
            if ( new_mc_fdb != mc_fdb )
            {
               br_mcast_mc_fdb_del_entry(proto, br, new_mc_fdb);
            }
        }
        else
        {
            activates++;
        }

        /* advance to the next rule */
        rule_p = blog_p->blogRule_p;
    }

    /* Free blog. The blog will only have rules if there was an error */
    blog_rule_free_list(blog_p);
    blog_put(blog_p);

    return activates;
} /* br_mcast_vlan_process */


int br_mcast_blog_process(struct net_bridge *br,
                            void            *mc_fdb,
                            t_BR_MCAST_PROTO_TYPE proto)
{
	Blog_t *blog_p = BLOG_NULL;
	blogRule_t *rule_p = NULL;
	struct net_device *wan_vlan_dev_p = NULL;
	struct net_device *lan_vlan_dev_p = NULL;
	struct net_device *wan_dev_p = NULL;
	struct net_device *lan_dev_p = NULL;
	struct net_bridge_mc_fdb_entry *igmp_fdb = NULL;
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	struct net_br_mld_mc_fdb_entry *mld_fdb = NULL;
#endif
	struct net_device *from_dev = NULL;
	uint32_t phyType;
   int numActivates;

	if(!mc_fdb)
		return -1;

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	if(BR_MCAST_PROTO_MLD == proto)
	{
		mld_fdb = (struct net_br_mld_mc_fdb_entry *)mc_fdb;
		from_dev = mld_fdb->from_dev;
	}
   else
#endif
	{
		igmp_fdb = (struct net_bridge_mc_fdb_entry *)mc_fdb;
		from_dev = igmp_fdb->from_dev;
	}

	/* allocate blog */
	blog_p = blog_get();
	if(blog_p == BLOG_NULL) {
		printk(KERN_WARNING "%s blog_p allocation failed\n",__FUNCTION__);
		return -1;
	}

	/* allocate blog rule */
	rule_p = blog_rule_alloc();
	if(rule_p == NULL)
	{
		printk(KERN_WARNING "%s blog_rule allocation failed\n",__FUNCTION__);
		blog_put(blog_p);
		return -1;
	}

	blog_rule_init(rule_p);
	blog_p->blogRule_p = (void *)rule_p;

	/* for LAN2LAN don't do anything */
	if(br->dev != from_dev) 
	{
		/* find WAN devices */
		br_mcast_blog_process_wan(rule_p, mc_fdb, proto,
		                          &wan_dev_p, &wan_vlan_dev_p);
	}

	/* find LAN devices */
	br_mcast_blog_process_lan(mc_fdb, proto, &lan_dev_p, &lan_vlan_dev_p);

	/* for LAN2LAN don't do anything */
	if(br->dev == from_dev) 
	{
		blog_p->rx.info.phyHdr = 0;
		blog_p->rx.info.channel = 0xFF; /* for lan2lan mcast */
		blog_p->rx.info.bmap.BCM_SWC = 1;
	}
	else
	{
		phyType = netdev_path_get_hw_port_type(wan_dev_p);
		blog_p->rx.info.phyHdr = (phyType & BLOG_PHYHDR_MASK);
		phyType = BLOG_GET_HW_ACT(phyType);

		if(blog_p->rx.info.phyHdrType == BLOG_GPONPHY)
		{
			unsigned int hw_subport_mcast_idx=0;
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
			hw_subport_mcast_idx = netdev_path_get_hw_subport_mcast_idx(wan_dev_p);
#endif

			if(hw_subport_mcast_idx < CONFIG_BCM_MAX_GEM_PORTS)
			{
                blog_p->rx.info.channel = hw_subport_mcast_idx;
			}
			else
			{
				/* Not a GPON Multicast WAN device */
				blog_rule_free_list(blog_p);
				blog_put(blog_p);
				return -1;
			}
		}
		else /* Ethernet or DSL WAN device */
		{
			blog_p->rx.info.channel = netdev_path_get_hw_port(wan_dev_p);
		}

		if( (blog_p->rx.info.phyHdrType == BLOG_XTMPHY) &&
		    ((phyType == VC_MUX_PPPOA) ||
		     (phyType == VC_MUX_IPOA) ||
		     (phyType == LLC_SNAP_ROUTE_IP) ||
		     (phyType == LLC_ENCAPS_PPP)) )
		{
			blog_p->insert_eth = 1;
		}

		if( (blog_p->rx.info.phyHdrType == BLOG_XTMPHY) &&
		    ((phyType == VC_MUX_PPPOA) ||
		     (phyType == LLC_ENCAPS_PPP)) )
		{
			blog_p->pop_pppoa = 1;
		}

#if defined(CONFIG_BCM96816) || defined(CONFIG_BCM96818)
		blog_p->rx.info.bmap.BCM_SWC = 1;
#else
		if(blog_p->rx.info.phyHdrType == BLOG_ENETPHY)
		{
			blog_p->rx.info.bmap.BCM_SWC = 1;
		}
		else
		{
			blog_p->rx.info.bmap.BCM_XPHY = 1;
		}
#endif
	}

	blog_p->tx.info.bmap.BCM_SWC = 1;

	blog_p->key.l1_tuple.phy = blog_p->rx.info.phyHdr;
	blog_p->key.l1_tuple.channel = blog_p->rx.info.channel;
	blog_p->key.protocol = BLOG_IPPROTO_UDP;

	phyType = netdev_path_get_hw_port_type(lan_dev_p);
	blog_p->tx.info.phyHdr = (phyType & BLOG_PHYHDR_MASK);
	blog_p->tx.info.channel = netdev_path_get_hw_port(lan_dev_p);

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
	if(BR_MCAST_PROTO_MLD == proto)
	{
		BCM_IN6_ASSIGN_ADDR(&blog_p->tupleV6.saddr, &mld_fdb->src_entry.src);
		BCM_IN6_ASSIGN_ADDR(&blog_p->tupleV6.daddr, &mld_fdb->grp);
		blog_p->rx.info.bmap.L3_IPv6 = 1;
	}
	else
#endif
	{
		blog_p->rx.tuple.saddr = igmp_fdb->src_entry.src.s_addr;
		blog_p->rx.tuple.daddr = igmp_fdb->grp.s_addr;
		blog_p->tx.tuple.saddr = igmp_fdb->src_entry.src.s_addr;
		blog_p->tx.tuple.daddr = igmp_fdb->grp.s_addr;
		blog_p->rx.info.bmap.L3_IPv4 = 1;
	}

	blog_p->rx.dev_p = wan_dev_p;
	blog_p->rx.info.multicast = 1;
	blog_p->tx.dev_p = lan_dev_p;

	if ( multiConfig.mcastPriQueue != -1 )
	{
		blog_p->mark = SKBMARK_SET_Q(blog_p->mark, multiConfig.mcastPriQueue);
	}

	/* add vlan blog rules, if any vlan interfaces were found */
	if(blogRuleVlanHook && (wan_vlan_dev_p || lan_vlan_dev_p)) {
		if(blogRuleVlanHook(blog_p, wan_vlan_dev_p, lan_vlan_dev_p) < 0) {
			printk(KERN_WARNING "Error while processing VLAN blog rules\n");
			blog_rule_free_list(blog_p);
			blog_put(blog_p);
			return -1;
		}
	}

	/* blog must have at least one rule */
	if (NULL == blog_p->blogRule_p)
	{
		/* blogRule_p == NULL may be valid if there are no 
		   VLAN rules and the default behavior for either interface is DROP */
		//printk(KERN_WARNING "Error while processing VLAN blog rules\n");
		blog_put(blog_p);
		return -1;
	}

	numActivates = br_mcast_vlan_process(br, mc_fdb, proto, blog_p);
	if ( 0 == numActivates )
	{
		return - 1;
	}

	return 0;
} /* br_mcast_blog_process */
#endif

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
struct net_device *br_get_device_by_index(char *brname,char index) {
	struct net_bridge *br = NULL;
	struct net_bridge_port *from_br = NULL;
	struct net_device *dev = dev_get_by_name(&init_net,brname); 
	if(!dev)
		return NULL;
	br = netdev_priv(dev);
	if(!br)
	{
		dev_put(dev);
		return NULL;
	}
	rcu_read_lock();
	from_br = br_get_port(br, index);
	rcu_read_unlock();
	if(!from_br) return NULL;
	else return from_br->dev;
}

static int br_get_port_id_by_name(struct net_bridge *br,char *name) {
	struct net_bridge_port *p;
	list_for_each_entry_rcu(p, &br->port_list, list) {
			//printk("p-dev->name:%s, devic name:%s,port_id:%d\n",p->dev->name,name,p->port_no);
			if(!strcmp(name,p->dev->name))
			return (int)p->port_no;
	}
	return -1;
}

static RAW_NOTIFIER_HEAD(mcast_snooping_chain);

int register_mcast_snooping_notifier(struct notifier_block *nb) {
	return raw_notifier_chain_register(&mcast_snooping_chain,nb);
}

int unregister_mcast_snooping_notifier(struct notifier_block *nb) {
	return raw_notifier_chain_unregister(&mcast_snooping_chain,nb);
}

int mcast_snooping_call_chain(unsigned long val,void *v) 
{
	return raw_notifier_call_chain(&mcast_snooping_chain,val,v);
}


void br_mcast_wl_flush(struct net_bridge *br) {
	t_MCPD_MLD_SNOOP_ENTRY snoopEntry;
	struct net_bridge_port *p;
	list_for_each_entry_rcu(p, &br->port_list, list) {
		if(!strncmp(p->dev->name,"wl",2)){
			snoopEntry.port_no= p->port_no;
			memcpy(snoopEntry.br_name,br->dev->name,IFNAMSIZ);
			mcast_snooping_call_chain(SNOOPING_FLUSH_ENTRY_ALL,(void *)&snoopEntry);
		}
	}
}

void br_mld_wl_del_entry(struct net_bridge *br,struct net_br_mld_mc_fdb_entry *dst) {
	if(dst && (!strncmp(dst->lan_name,"wl",2))) { 
		t_MCPD_MLD_SNOOP_ENTRY snoopEntry;
		snoopEntry.port_no=(char)br_get_port_id_by_name(br,dst->lan_name);
		memcpy(snoopEntry.br_name,br->dev->name,IFNAMSIZ);
		memcpy(&snoopEntry.grp,&dst->grp,sizeof(struct in6_addr));
		mcast_snooping_call_chain(SNOOPING_FLUSH_ENTRY,(void *)&snoopEntry);
	} 

}
EXPORT_SYMBOL(unregister_mcast_snooping_notifier);
EXPORT_SYMBOL(register_mcast_snooping_notifier);
EXPORT_SYMBOL(br_get_device_by_index);
#endif
