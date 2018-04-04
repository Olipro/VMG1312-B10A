/* (C) 2001-2002 Magnus Boden <mb@ozaba.mine.nu>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kmod.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/in.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/netfilter.h>

#include <net/ip.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_tuple.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <net/netfilter/nf_conntrack_helper.h>

MODULE_AUTHOR("Jues");
MODULE_DESCRIPTION("FCC connection tracking helper");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ip_conntrack_fcc");

#define MAX_PORTS 8//jues need to check
static unsigned short ports[MAX_PORTS];
static int ports_c=0; //brian, initiate the default value as 0
module_param_array(ports, ushort, &ports_c, 0400);
//MODULE_PARM_DESC(ports, "Port numbers of TFTP servers");

#if 1
#define DEBUGP(format, args...) printk("%s:%s:" format, \
                                       __FILE__, __FUNCTION__ , ## args)
#else
#define DEBUGP(format, args...)
#endif
#if 1 //jues for rtcp
struct rtcp {
__u8 rtcp_version;
__u8 rtcp_type;
__be16 rtcp_length;
char rtcp_data[1];
};
#endif

#define CTNUM 24
#define FCC_PORT 8027

struct nf_conn *usedct[CTNUM];

#define FCC_CT_DUMP_TUPLE(tp)						    \
DEBUGP("tuple %p: %u %u " "%02x:%02x:%02x:%02x" " %hu -> " "%02x:%02x:%02x:%02x" " %hu\n",	    \
	(tp), (tp)->src.l3num, (tp)->dst.protonum,			    \
	NIPQUAD((tp)->src.u3.all), ntohs((tp)->src.u.all), \
	NIPQUAD((tp)->dst.u3.all), ntohs((tp)->dst.u.all))

#if 1
static void fcc_nat_follow_master(struct nf_conn *ct,
			  struct nf_conntrack_expect *exp)
{
	struct nf_nat_range range;
	//struct net *net;
        
         
	/* This must be a fresh one. */
	BUG_ON(ct->status & IPS_NAT_DONE_MASK);

//	DEBUGP("inside the ip_nat_follow_master with saved= %u ...tcp = %u  udp ...=  %u   \n ",
//	exp->saved_proto, exp->tuple.dst.u.tcp.port, exp->tuple.dst.u.udp.port); //brian, saved_proto is not a interger....


#if 1
    #ifdef CONFIG_IFX_ALG_QOS
     if(exp->master->rtcp_expect_registered == 1)
	 { 
       DEBUGP("\nMaster conntracker ifx_alg_qos_mark is : %x \n",ct->ifx_alg_qos_mark );
        ct->ifx_alg_qos_mark = IFX_ALG_APP_FTP | IFX_ALG_PROTO_DATA;
       DEBUGP("\n Marked the Child conntrackeri with value: %x !!! \n",ct->ifx_alg_qos_mark );
     }
	 
	 else if(exp->master->rtcp_expect_registered == 2 )
	 {
	 DEBUGP("\nMaster conntracker ifx_alg_qos_mark(RTSP) is : %x \n",ct->ifx_alg_qos_mark ); 
	 ct->ifx_alg_qos_mark = IFX_ALG_APP_RTSP | IFX_ALG_PROTO_RTP;
	// printk("\nMastefcc_nat_follow_masterr conntracker ifx_alg_qos_mark (RTSP) is : %x \n",ct->ifx_alg_qos_mark );
	 }
	
	else if (exp->master->rtcp_expect_registered == 3 )
	     {
		      ct->ifx_alg_qos_mark = IFX_ALG_APP_H323 | IFX_ALG_PROTO_RTP;
			 // printk("\nMaster conntracker ifx_alg_qos_mark (RTP) is : %x \n",ct->ifx_alg_qos_mark );
		 }
		
	else if (exp->master->rtcp_expect_registered == 4 )
     {
		      ct->ifx_alg_qos_mark = IFX_ALG_APP_H323 | IFX_ALG_PROTO_RTCP;
			 // printk("\nMaster conntracker ifx_alg_qos_mark (RTCP) is : %x \n",ct->ifx_alg_qos_mark );
      }
				   
	 else if (exp->master->rtcp_expect_registered == 5 )
	 {
	 
	 ct->ifx_alg_qos_mark = IFX_ALG_APP_H323;
	// printk("\nMaster conntracker ifx_alg_qos_mark (H323) is : %x \n",ct->ifx_alg_qos_mark );
	 
	 }
	 
   #endif


FCC_CT_DUMP_TUPLE(&ct->master->tuplehash[exp->dir].tuple);
FCC_CT_DUMP_TUPLE(&ct->master->tuplehash[!exp->dir].tuple);


#if 1
        /* Change src to where master sends to */
        range.flags = IP_NAT_RANGE_MAP_IPS;
        range.min_ip = range.max_ip
                = ct->tuplehash[!exp->dir].tuple.src.u3.ip;
        /* hook doesn't matter, but it has to do source manip */
        nf_nat_setup_info(ct, &range, IP_NAT_MANIP_SRC);


	/* For DST manip, map port here to where it's expected. */
	range.flags = (IP_NAT_RANGE_MAP_IPS | IP_NAT_RANGE_PROTO_SPECIFIED);
	range.min = range.max = exp->saved_proto;
	range.min_ip = range.max_ip
		= ct->master->tuplehash[!exp->dir].tuple.src.u3.ip;
	/* hook doesn't matter, but it has to do destination manip */
	nf_nat_setup_info(ct, &range, IP_NAT_MANIP_DST);

#endif

#endif
}
#endif

static void  fcc_expectfn(struct nf_conn *ct,struct nf_conntrack_expect *exp)
{
        DEBUGP("\nFcc match!! \n");
	FCC_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	FCC_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);

	fcc_nat_follow_master(ct, exp);

	DEBUGP("\nFcc_nat result!! \n");
	FCC_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
	FCC_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);
	DEBUGP("\nct->master = %x \n",(unsigned int)ct->master);
}

static void mangle_contents(struct sk_buff *skb,
			    unsigned int dataoff,
			    unsigned int match_offset,
			    unsigned int match_len,
			    const char *rep_buffer,
			    unsigned int rep_len)
{
	unsigned char *data;

//	DEBUGP(skb_is_nonlinear(skb));
	data = (unsigned char*)ip_hdr(skb) + dataoff;

	/* move post-replacement */ //brian, no need?
	memmove(data + match_offset + rep_len,
		data + match_offset + match_len,
		skb->tail - (data + match_offset + match_len));

	/* insert data from buffer */
	memcpy(data + match_offset, rep_buffer, rep_len);

	/* update skb info */ //brian, no need?
	if (rep_len > match_len) {
		DEBUGP("nf_nat_mangle_packet: Extending packet by "
		       "%u from %u bytes\n", rep_len - match_len,
		       skb->len);
		skb_put(skb, rep_len - match_len);
	} else {
		DEBUGP("nf_nat_mangle_packet: Shrinking packet from "
		       "%u from %u bytes\n", match_len - rep_len,
		       skb->len);
		__skb_trim(skb, skb->len + rep_len - match_len);
	}

	/* fix IP hdr checksum information */
	ip_hdr(skb)->tot_len = htons(skb->len);
	ip_send_check(ip_hdr(skb));//brian, is it needed for IP checksum?

}

#if 0
static int removeUsedct(struct nf_conn *ct){
	int i;
	for(i = 0; i < CTNUM; i++){
		if(usedct[i] != NULL){
			DEBUGP("usedct[i]->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip = %x ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip = %x\n",usedct[i]->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip,ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip);
		}#endif
		if(usedct[i] != 0 && (usedct[i]->tuplehash[IP_CT_DIR_REPLY].tuple.src.u3.ip == ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u3.ip)){
			atomic_dec(&usedct[i]->ct_general.use);
			DEBUGP("Remove conntrack (%d): %d and master = %x \n",i,usedct[i]->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.udp.port,usedct[i]->master);
			//nf_ct_single_cleanup(usedct[i]->master);
			nf_ct_single_cleanup(usedct[i]);
			usedct[i]=NULL;
		}
	}
}
#endif

#if 1
static int fcc_help(struct sk_buff *skb,
#else
static int fcc_help(struct sk_buff **pskb,
#endif
		     unsigned int protoff,
		     struct nf_conn *ct,
		     enum ip_conntrack_info ctinfo)
{
	struct rtcp _rtcp, *rtcp;
	struct udphdr _udphdr, *udphp;
	struct iphdr *iph;
	struct nf_conntrack_expect *exp;
	struct nf_conntrack_expect *exp2;
	struct nf_conntrack_tuple *tuple,*tuple1;
	unsigned int ret = NF_ACCEPT;

	int family = ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.l3num;
	int datalen;

	iph = ip_hdr(skb);
	udphp = skb_header_pointer(skb, protoff,sizeof(_udphdr),&_udphdr);
	if(udphp == NULL)
		return NF_ACCEPT;

	rtcp = skb_header_pointer(skb, protoff + sizeof(struct udphdr),
				 sizeof(_rtcp), &_rtcp);
	if (rtcp == NULL)
		return NF_ACCEPT;

	if(((rtcp->rtcp_version >> 6) & 0xf) != 2){
		return NF_ACCEPT;
	}

	switch(rtcp->rtcp_type) {
		case 0xcd:
#if 1
			printk("rtcp->rtcp_length = %d\n",rtcp->rtcp_length);
			if(rtcp->rtcp_length != 6)
				return NF_ACCEPT;
			DEBUGP("Receive FCC  type 205\n");
			FCC_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple);
			FCC_CT_DUMP_TUPLE(&ct->tuplehash[IP_CT_DIR_REPLY].tuple);

			if (!skb_make_writable(skb, skb->len))
				return 0;

			mangle_contents(skb,iph->ihl*4 + sizeof(*udphp) ,20, 4, (char *)ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u3.all, 4);
			mangle_contents(skb,iph->ihl*4 + sizeof(*udphp) ,24, 2, (char *)&ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.udp.port, 2);
			datalen = skb->len - iph->ihl*4;
			udphp->len = htons(datalen);
			if (skb->ip_summed != CHECKSUM_PARTIAL) {
				udphp->check = 0;
				udphp->check = csum_tcpudp_magic(iph->saddr, iph->daddr,
				                                datalen, IPPROTO_UDP,
				                                csum_partial((char *)udphp,
				                                             datalen, 0));
				if (!udphp->check)
					udphp->check = CSUM_MANGLED_0;
			} else
#if 1 //__ZyXEL__, Wood
				inet_proto_csum_replace2(&udphp->check, skb,
						       htons(datalen), htons(datalen), 1);
#endif
#if 0
	DEBUGP("\nct = %x \n",ct);
			removeUsedct(ct);
#endif

			
			tuple = &ct->tuplehash[IP_CT_DIR_REPLY].tuple;
			tuple1 = &ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple;
			
//==================================				
			exp = nf_ct_expect_alloc(ct);
	 		if (exp == NULL)
				return NF_DROP;
			nf_ct_expect_init(exp, NF_CT_EXPECT_CLASS_DEFAULT, family,
					 NULL, &tuple->dst.u3,
					 IPPROTO_UDP,
					 NULL, NULL);
					 
			exp->tuple.src.u.udp.port = (tuple->src.u.udp.port) -1;
			exp->tuple.dst.u.udp.port =  (tuple->dst.u.udp.port) -1;
			exp->saved_proto.udp.port = (tuple1->src.u.udp.port) -1;
			exp->expectfn = fcc_expectfn;
			exp->dir = IP_CT_DIR_REPLY;
			exp->mask.src.u.all = 0xFFFF;
//==================================			
			exp2 = nf_ct_expect_alloc(ct);
	 		if (exp2 == NULL)
				return NF_DROP;
			nf_ct_expect_init(exp2, NF_CT_EXPECT_CLASS_DEFAULT, family,
					 NULL, &tuple->dst.u3,
					 IPPROTO_UDP,
					 NULL, NULL);
					 
			exp2->tuple.src.u.udp.port = (tuple->src.u.udp.port);
			exp2->tuple.dst.u.udp.port =  (tuple->dst.u.udp.port);
			exp2->saved_proto.udp.port = (tuple1->src.u.udp.port);
			exp2->expectfn = fcc_expectfn;
			exp2->dir = IP_CT_DIR_REPLY;
			exp2->mask.src.u.all = 0xFFFF;			
//==================================		

			if (nf_ct_expect_related(exp2) == 0) {
				if (nf_ct_expect_related(exp) == 0) {
					DEBUGP("expect1: \n");
					FCC_CT_DUMP_TUPLE(&exp->tuple);
					DEBUGP("expect2: \n");
					FCC_CT_DUMP_TUPLE(&exp2->tuple);
				} else {
					nf_ct_unexpect_related(exp2);
					ret = NF_DROP;
				}
			} else
				ret = NF_DROP;
			nf_ct_expect_put(exp);
			nf_ct_expect_put(exp2);

#endif
			break;
			
		case 0xcb:
#if 1
		       pr_debug("Receive FCC  type 203 source port = %d\n",udphp->source);
#else
		       DEBUGP("Receive FCC  type = %d\n",rtcp->rtcp_type);
#endif
#if 0
			for(i = 0; i < CTNUM; i++){
#if 0
				if(usedct[i] != NULL){
					DEBUGP("(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port) = %d (ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.udp.port) = %d = %d\n",(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port),(ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.udp.port));
					DEBUGP("(ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u.udp.port) = %d (ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.udp.port) = %d = %d\n",(ct->tuplehash[IP_CT_DIR_REPLY].tuple.src.u.udp.port),(ct->tuplehash[IP_CT_DIR_REPLY].tuple.dst.u.udp.port));
				}
#endif
#if 0
				if((usedct[i] != NULL) && ((ct->tuplehash[IP_CT_DIR_ORIGINAL].tuple.src.u.udp.port) -1 == usedct[i]->tuplehash[IP_CT_DIR_ORIGINAL].t
uple.src.u.udp.port)){
#else
				if((usedct[i] != NULL) && ((udphp->source) -1 == usedct[i]->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.udp.port)){
#endif
					DEBUGP("Remove conntrack (%d): %d\n",i,usedct[i]->tuplehash[IP_CT_DIR_ORIGINAL].tuple.dst.u.udp.port);
					atomic_dec(&usedct[i]->ct_general.use);
					//on nf_ct_single_cleanup(usedct[i]->master);
					nf_ct_single_cleanup(usedct[i]);
					usedct[i]=NULL;
					break;
				}
			}
#endif
			break;
		default:
			return NF_ACCEPT;

	}
	return NF_ACCEPT;
}

static struct nf_conntrack_helper rtcp_fcc[MAX_PORTS] __read_mostly;
static char fcc_names[MAX_PORTS][sizeof("tftp-65535")] __read_mostly;

static const struct nf_conntrack_expect_policy rtcp_exp_policy = {
        .max_expected   = 2,
        .timeout        = 30,
};

static void nf_conntrack_fcc_fini(void)
{
	int i;

	for (i = 0; i < ports_c; i++) {
		nf_conntrack_helper_unregister(&rtcp_fcc[i]);
	}
}

static int __init nf_conntrack_fcc_init(void)
{
	int i, ret;

	memset(usedct,0,sizeof(usedct));
	if (ports_c == 0)
		ports[ports_c++] = FCC_PORT;

	for (i = 0; i < ports_c; i++) {
		memset(&rtcp_fcc[i], 0, sizeof(rtcp_fcc[i]));

		rtcp_fcc[i].tuple.src.l3num = AF_INET;
		rtcp_fcc[i].tuple.dst.protonum = IPPROTO_UDP;
		rtcp_fcc[i].tuple.src.u.udp.port = htons(ports[i]); //brian, not dst port?
		rtcp_fcc[i].expect_policy = &rtcp_exp_policy;
		rtcp_fcc[i].me = THIS_MODULE;
		rtcp_fcc[i].help = fcc_help;

		sprintf(fcc_names[i], "FCC");
		rtcp_fcc[i].name = fcc_names[i];

		ret = nf_conntrack_helper_register(&rtcp_fcc[i]);
		if (ret) {
			printk(KERN_ERR"nf_ct_fcc: failed to register helper "
			       "for pf: %u port: %u\n",
				rtcp_fcc[i].tuple.src.l3num, ports[i]);
			nf_conntrack_fcc_fini();
			return ret;
			}
		else{
			printk(KERN_ERR"%s register helper success",__FUNCTION__);
		}

	}
	return 0;
}

module_init(nf_conntrack_fcc_init);
module_exit(nf_conntrack_fcc_fini);
