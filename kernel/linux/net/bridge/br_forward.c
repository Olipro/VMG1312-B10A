/*
 *	Forwarding decision
 *	Linux ethernet bridge
 *
 *	Authors:
 *	Lennert Buytenhek		<buytenh@gnu.org>
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>
#include <linux/netfilter_bridge.h>
#include "br_private.h"
#if defined(CONFIG_MIPS_BRCM)
#include <linux/ip.h>
#include <linux/igmp.h>
#include <linux/blog.h>
#endif /* for IGMP */

#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_11ac_throughput_patch_from_412L08)
#if defined(CONFIG_BCM96828) && !defined(CONFIG_EPON_HGU)
#if defined(CONFIG_EPON_UNI_UNI_ENABLED)
int uni_uni_enabled = 1;
#else
int uni_uni_enabled = 0;
#endif
#endif
#endif

/* Don't forward packets to originating port or forwarding diasabled */
static inline int should_deliver(const struct net_bridge_port *p,
				 const struct sk_buff *skb)
{
#if defined(CONFIG_MIPS_BRCM)
	const struct iphdr *pip = NULL;
	const unsigned char *dest = eth_hdr(skb)->h_dest;

	if (skb->dev == p->dev || p->state != BR_STATE_FORWARDING)
	{
		return 0;
	}

	/*
	 * Do not forward any packets received from one WAN interface 
	 * to other WAN interfaces in multiple PVC case
	 */
	if( (skb->dev->priv_flags & p->dev->priv_flags) & IFF_WANDEV )
	{
		return 0;
	}

#define SUPPORT_LANVLAN
#ifdef SUPPORT_LANVLAN
	if ((skb->dev->priv_flags & IFF_WANDEV) == 0 &&
	    (p->dev->priv_flags   & IFF_WANDEV) == 0)
	{
		/* From LAN to LAN */
		/* Do not forward any packets to virtual interfaces on the same
		 * real interface of the originating virtual interface.
		 */
		struct net_device *sdev = skb->dev;
		struct net_device *ddev = p->dev;
         
		while (!netdev_path_is_root(sdev))
		{
			sdev = netdev_path_next_dev(sdev);
		}

		while (!netdev_path_is_root(ddev))
		{
			ddev = netdev_path_next_dev(ddev);
		}
      
		if (strcmp(sdev->name, ddev->name) == 0)
		{
			return 0;
		}

/* TBD: Check for 6816 */
#if !defined(CONFIG_BCM96816) && defined(CONFIG_11ac_throughput_patch_from_412L08)
		if (skb->pkt_type == PACKET_BROADCAST) 
		{
			if (sdev->priv_flags & IFF_HW_SWITCH & ddev->priv_flags)
			{
#if defined(CONFIG_BCM96828) && !defined(CONFIG_EPON_HGU) 
				if (!uni_uni_enabled) {
					/* Forward only US or DS broadcast */
					if (!(sdev->priv_flags & IFF_EPON_IF) && !(ddev->priv_flags & IFF_EPON_IF))
					{
						return 0;
					}
	}
#else
#if defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828)
				if (!((sdev->priv_flags & IFF_EXT_SWITCH) ^ (ddev->priv_flags & IFF_EXT_SWITCH)))
				{
					return 0;
				}
#else
				return 0;
#endif
#endif
			}
		}
#if defined(CONFIG_BCM96828) && !defined(CONFIG_EPON_HGU)
		else if (skb->pkt_type != PACKET_MULTICAST && skb->pkt_type != PACKET_HOST)
		{
			return 0;
		}
#endif
#endif // defined(CONFIG_11ac_throughput_patch_from_412L08)
	}
#endif //SUPPORT_LANVLAN

#if (defined(CONFIG_BCM96816) || !defined(SUPPORT_LANVLAN)) && defined(CONFIG_11ac_throughput_patch_from_412L08)
	/* IFF_HW_SWITCH now indicates switching of only bcast in hardware. Mcast is assumed
	   to be not switched in hardware and will be handled by bridge. */
	if (skb->pkt_type == PACKET_BROADCAST)
	{
		/* If source and destination interfaces belong to the switch, don't forward packet */
		if ((skb->dev->priv_flags & IFF_HW_SWITCH) && (p->dev->priv_flags & IFF_HW_SWITCH))
		{
#if defined(CONFIG_BCM963268) || defined(CONFIG_BCM96828)
			if (!((skb->dev->priv_flags & IFF_EXT_SWITCH) ^ (p->dev->priv_flags & IFF_EXT_SWITCH)))
			{
				return 0;
			}
#else
			return 0;
#endif
		}
	}
#endif

	/*
	 * CPE is querying for LAN-2-LAN multicast.  These query messages 
	 * should not go on WAN interfaces.
	 * Also don't alow leaking of IGMPv2 report messages among LAN ports
	 */ 
	if(is_multicast_ether_addr(dest))
	{
		__u8 igmpTypeOffset = 0;
		if ( vlan_eth_hdr(skb)->h_vlan_proto == ETH_P_IP )
		{
			pip = ip_hdr(skb);
			igmpTypeOffset = (pip->ihl << 2);
		}
		else if ( vlan_eth_hdr(skb)->h_vlan_proto == ETH_P_8021Q )
		{
			if ( vlan_eth_hdr(skb)->h_vlan_encapsulated_proto == ETH_P_IP )
			{
				pip = (struct iphdr *)(skb_network_header(skb) + sizeof(struct vlan_hdr));
				igmpTypeOffset = (pip->ihl << 2) + sizeof(struct vlan_hdr);
			}
		}

		if ((pip) && (pip->protocol == IPPROTO_IGMP))
		{
			__u8 igmp_type = skb->data[igmpTypeOffset];
        
			if((p->dev->priv_flags & IFF_WANDEV))
			{
				if (igmp_type == IGMP_HOST_MEMBERSHIP_QUERY) 
				{
					return 0;
				}
			}
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_IGMP_SNOOP)
			else
			{
				if ((p->br->igmp_snooping) && (igmp_type != IGMP_HOST_MEMBERSHIP_QUERY)) 
				{
					return 0;
				}
			}
#endif
		}
	}

	return 1;
#else
	return (skb->dev != p->dev && p->state == BR_STATE_FORWARDING);
#endif
}

static inline unsigned packet_length(const struct sk_buff *skb)
{
	return skb->len - (skb->protocol == htons(ETH_P_8021Q) ? VLAN_HLEN : 0);
}

int br_dev_queue_push_xmit(struct sk_buff *skb)
{
	/* drop mtu oversized packets except gso */
	if (packet_length(skb) > skb->dev->mtu && !skb_is_gso(skb))
		kfree_skb(skb);
	else {
		/* ip_refrag calls ip_fragment, doesn't copy the MAC header. */
		if (nf_bridge_maybe_copy_header(skb))
			kfree_skb(skb);
		else {
			skb_push(skb, ETH_HLEN);

			dev_queue_xmit(skb);
		}
	}

	return 0;
}

int br_forward_finish(struct sk_buff *skb)
{
	return NF_HOOK(PF_BRIDGE, NF_BR_POST_ROUTING, skb, NULL, skb->dev,
		       br_dev_queue_push_xmit);

}

static void __br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	skb->dev = to->dev;
	NF_HOOK(PF_BRIDGE, NF_BR_LOCAL_OUT, skb, NULL, skb->dev,
			br_forward_finish);
}

static void __br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	struct net_device *indev;

	if (skb_warn_if_lro(skb)) {
		kfree_skb(skb);
		return;
	}

	indev = skb->dev;
	skb->dev = to->dev;
	skb_forward_csum(skb);

	NF_HOOK(PF_BRIDGE, NF_BR_FORWARD, skb, indev, skb->dev,
			br_forward_finish);
}

/* called with rcu_read_lock */
void br_deliver(const struct net_bridge_port *to, struct sk_buff *skb)
{
	if (should_deliver(to, skb)) {
		__br_deliver(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called with rcu_read_lock */
void br_forward(const struct net_bridge_port *to, struct sk_buff *skb)
{
	if (should_deliver(to, skb)) {
		__br_forward(to, skb);
		return;
	}

	kfree_skb(skb);
}

/* called under bridge lock */
static void br_flood(struct net_bridge *br, struct sk_buff *skb,
	void (*__packet_hook)(const struct net_bridge_port *p,
			      struct sk_buff *skb))
{
	struct net_bridge_port *p;
	struct net_bridge_port *prev;


#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
	Blog_t * blog_p = blog_ptr(skb);
	if ( blog_p && !blog_p->rx.info.multicast)
		blog_skip(skb);
#endif
	prev = NULL;

	list_for_each_entry_rcu(p, &br->port_list, list) {
		if (should_deliver(p, skb)) {
			if (prev != NULL) {
				struct sk_buff *skb2;

				if ((skb2 = skb_clone(skb, GFP_ATOMIC)) == NULL) {
					br->dev->stats.tx_dropped++;
					kfree_skb(skb);
					return;
				}
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
				blog_clone(skb, blog_ptr(skb2));
#endif
				__packet_hook(prev, skb2);
			}

			prev = p;
		}
	}

	if (prev != NULL) {
		__packet_hook(prev, skb);
		return;
	}

	kfree_skb(skb);
}


/* called with rcu_read_lock */
void br_flood_deliver(struct net_bridge *br, struct sk_buff *skb)
{
	br_flood(br, skb, __br_deliver);
}

/* called under bridge lock */
void br_flood_forward(struct net_bridge *br, struct sk_buff *skb)
{
	br_flood(br, skb, __br_forward);
}
