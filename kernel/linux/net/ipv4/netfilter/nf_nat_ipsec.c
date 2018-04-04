/*
<:copyright-gpl
 Copyright 2010 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
*/

#include <linux/module.h>
#include <linux/udp.h>
#include <linux/ip.h>

#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netfilter/nf_nat_rule.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <linux/netfilter/nf_conntrack_ipsec.h>

MODULE_AUTHOR("Pavan Kumar <pavank@broadcom.com>");
MODULE_DESCRIPTION("Netfilter connection tracking module for ipsec");
MODULE_LICENSE("GPL");
MODULE_ALIAS("nf_nat_ipsec");

#if 0
#define DEBUGP(format, args...) printk(KERN_DEBUG "%s:%s: " format, __FILE__, \
				       __FUNCTION__, ## args)
#else
#define DEBUGP(format, args...)
#endif


/* outbound packets == from LAN to WAN */
static int
ipsec_outbound_pkt(struct sk_buff **pskb,
                   struct nf_conn *ct, enum ip_conntrack_info ctinfo)

{
   struct iphdr *iph = (*pskb)->nh.iph;
   struct udphdr *udph = (void *)iph + iph->ihl * 4;

   /* make sure source port is 500 */
   udph->source = htons(IPSEC_PORT);
   udph->check = 0;
   
   return NF_ACCEPT;
}


/* inbound packets == from WAN to LAN */
static int
ipsec_inbound_pkt(struct sk_buff **pskb, struct nf_conn *ct,
                  enum ip_conntrack_info ctinfo, __be32 lan_ip)
{
   struct iphdr *iph = (*pskb)->nh.iph;
   struct udphdr *udph = (void *)iph + iph->ihl * 4;

   iph->daddr = lan_ip;
   udph->check = 0;
   iph->check = 0;
   iph->check = ip_fast_csum((unsigned char *)iph, iph->ihl);
   
   return NF_ACCEPT;
}

static int __init nf_nat_helper_ipsec_init(void)
{
   BUG_ON(rcu_dereference(nf_nat_ipsec_hook_outbound));
   rcu_assign_pointer(nf_nat_ipsec_hook_outbound, ipsec_outbound_pkt);

   BUG_ON(rcu_dereference(nf_nat_ipsec_hook_inbound));
   rcu_assign_pointer(nf_nat_ipsec_hook_inbound, ipsec_inbound_pkt);

   return 0;
}

static void __exit nf_nat_helper_ipsec_fini(void)
{
   rcu_assign_pointer(nf_nat_ipsec_hook_inbound, NULL);
   rcu_assign_pointer(nf_nat_ipsec_hook_outbound, NULL);
   synchronize_rcu();
}

module_init(nf_nat_helper_ipsec_init);
module_exit(nf_nat_helper_ipsec_fini);
