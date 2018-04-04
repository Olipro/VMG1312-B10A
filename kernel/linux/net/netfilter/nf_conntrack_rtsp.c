/* RTSP helper for connection tracking. */

/* (C) 2008 Broadcom Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/netfilter.h>
#include <linux/ip.h>
#include <linux/ipv6.h>
#include <linux/ctype.h>
#include <linux/inet.h>
#include <linux/in.h>
#include <net/checksum.h>
#include <net/tcp.h>

#include <net/netfilter/nf_conntrack.h>
#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_conntrack_ecache.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <linux/netfilter/nf_conntrack_rtsp.h>
#include <linux/iqos.h>
#include <net/netfilter/nf_nat_helper.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("RTSP connection tracking helper");
MODULE_ALIAS("ip_conntrack_rtsp");

#ifdef pr_debug
#undef pr_debug
#define pr_debug(fmt, ...) printk(fmt, ##__VA_ARGS__)
#endif

#define RTSP_PORT 554


#define FIX_ACK_ERROR_ISSUE 1

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
__be16 rtsp_old_port = 6970;        //Store the transform port of DUT wan
__be16 rtsp_new_port;        //Store the transform port of DUT wan
__be16 rtsp_min_port = 6970;  //Store the lower port of DUT wan
__be16 rtsp_max_port = 7000;  //Store the higher port of DUT wan

int teardown_flag =0 ;
#endif
__be16 client_loport;        //Store the client lower port
__be16 client_hiport;        //Store the client higher port


/* This is slow, but it's simple. --RR */
static char *rtsp_buffer;

static DEFINE_SPINLOCK(nf_rtsp_lock);

#define MAX_PORTS 8
static u_int16_t ports[MAX_PORTS];
static unsigned int ports_c;
module_param_array(ports, ushort, &ports_c, 0400);
MODULE_PARM_DESC(ports, "port numbers of RTSP servers");

#define RTSP_CHANNEL_MAX 8
static int max_outstanding = RTSP_CHANNEL_MAX;
module_param(max_outstanding, int, 0600);
MODULE_PARM_DESC(max_outstanding,
		 "max number of outstanding SETUP requests per RTSP session");

/* Single data channel */
int (*nat_rtsp_channel_hook) (struct sk_buff *skb, struct nf_conn *ct,
			      enum ip_conntrack_info ctinfo,
			      unsigned int matchoff, unsigned int matchlen,
			      struct nf_conntrack_expect *exp, int *delta);
EXPORT_SYMBOL_GPL(nat_rtsp_channel_hook);

/* A pair of data channels (RTP/RTCP) */
int (*nat_rtsp_channel2_hook) (struct sk_buff *skb, struct nf_conn *ct,
			       enum ip_conntrack_info ctinfo,
			       unsigned int matchoff, unsigned int matchlen,
			       struct nf_conntrack_expect *rtp_exp,
			       struct nf_conntrack_expect *rtcp_exp,
			       char dash, int *delta);
EXPORT_SYMBOL_GPL(nat_rtsp_channel2_hook);

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
/* A pair of data channels (RTP/RTCP/retrasm) */
int (*nat_rtsp_channel3_hook) (struct sk_buff *skb, struct nf_conn *ct,
			       enum ip_conntrack_info ctinfo,
			       unsigned int matchoff, unsigned int matchlen,
			       struct nf_conntrack_expect *rtp_exp,
			       struct nf_conntrack_expect *rtcp_exp,
				   struct nf_conntrack_expect *rtcp2_exp,
			  	   struct nf_conntrack_expect *rertp_exp,
     			   struct nf_conntrack_expect *rertp2_exp,
			       char dash, int *delta);
EXPORT_SYMBOL_GPL(nat_rtsp_channel3_hook);
#endif
#if FIX_ACK_ERROR_ISSUE
int (*modify_setup_mesg_hook) (struct sk_buff *skb, struct nf_conn *ct,
			 	  enum ip_conntrack_info ctinfo, const char *tpdate, int matchoff, int matchlen,
			 	  int *delta);
EXPORT_SYMBOL_GPL(modify_setup_mesg_hook);

int (*modify_reply_mesg_hook) (struct sk_buff *skb, struct nf_conn *ct,
			 	  enum ip_conntrack_info ctinfo, const char *tpdate, int matchoff, int matchlen,
			 	  int *delta);
EXPORT_SYMBOL_GPL(modify_reply_mesg_hook);
#endif

/* Modify parameters like client_port in Transport for single data channel */
int (*nat_rtsp_modify_port_hook) (struct sk_buff *skb, struct nf_conn *ct,
			      	  enum ip_conntrack_info ctinfo,
			      	  unsigned int matchoff, unsigned int matchlen,
			      	  __be16 rtpport, int *delta);
EXPORT_SYMBOL_GPL(nat_rtsp_modify_port_hook);

/* Modify parameters like client_port in Transport for multiple data channels*/
int (*nat_rtsp_modify_port2_hook) (struct sk_buff *skb, struct nf_conn *ct,
			       	   enum ip_conntrack_info ctinfo,
			       	   unsigned int matchoff, unsigned int matchlen,
			       	   __be16 rtpport, __be16 rtcpport,
				   char dash, int *delta);
EXPORT_SYMBOL_GPL(nat_rtsp_modify_port2_hook);

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
/* Modify parameters like client_port in Transport for multiple data channels*/
int (*nat_rtsp_modify_port3_hook) (struct sk_buff *skb, struct nf_conn *ct,
			       	   enum ip_conntrack_info ctinfo,
			       	   unsigned int matchoff, unsigned int matchlen,
			       	   __be16 rtpport, __be16 rtcpport,
				   char dash, int *delta);

EXPORT_SYMBOL_GPL(nat_rtsp_modify_port3_hook);
#endif
#if FIX_ACK_ERROR_ISSUE
/* Modify parameters like client_port in Transport for multiple data channels*/
void (*clean_ipport_buf_hook) (void);

EXPORT_SYMBOL_GPL(clean_ipport_buf_hook);
#endif

/* Modify parameters like destination in Transport */
int (*nat_rtsp_modify_addr_hook) (struct sk_buff *skb, struct nf_conn *ct,
			 	  enum ip_conntrack_info ctinfo,
			 	  int matchoff, int matchlen, int *delta);
EXPORT_SYMBOL_GPL(nat_rtsp_modify_addr_hook);

static int memmem(const char *haystack, int haystacklen,
		  const char *needle, int needlelen)
{
	const char *p = haystack;
	int l = haystacklen - needlelen + 1;
	char c = *needle;

	while(l-- > 0) { /* "!=0" won't handle haystacklen less than needlelen, need ">" */
		if (*p++ == c) {
			if (memcmp(p, needle+1, needlelen-1) == 0)
				return p - haystack - 1;
		}
	}
	return -1;
}

static int memstr(const char *haystack, int haystacklen,
		  const char *needle, int needlelen)
{
	const char *p = haystack;
	int l = haystacklen - needlelen + 1;
	char c = *needle;

	if (isalpha(c)) {
		char lower = __tolower(c);
		char upper = __toupper(c);

		while(l-- > 0) {  /* "!=0" won't handle haystacklen less than needlelen, need ">" */
			if (*p == lower || *p == upper) {
				if (strncasecmp(p, needle, needlelen) == 0)
					return p - haystack;
			}
			p++;
		}
	} else {
		while(l-- > 0) {
			if (*p++ == c) {
				if (strncasecmp(p, needle+1, needlelen-1) == 0)
					return p - haystack - 1;
			}
		}
	}
	return -1;
}
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
static int get_cseq(const char *str)
{
	unsigned long cseq = 0, i = 0;
	char c = *str;
	while(i++ < 10 && c && c != 0xd && c>='0' && c <= '9'){
		cseq = (cseq * 10) + (c - '0');
		c = *(str + i);
	}
	if(!cseq)
		cseq = -1;
	return (int) cseq;
}
#endif

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
/* delete matched expectations for this expect session */
void remove_stay_expectations(struct nf_conn *ct)
{
	struct nf_conntrack_expect *i;
	struct hlist_node *n;
	struct net *net = nf_ct_net(ct);
	unsigned int bucket = 0;


	for (bucket = 0; bucket < nf_ct_expect_hsize; bucket++) {
		hlist_for_each_entry(i, n, &net->ct.expect_hash[bucket], hnode) {
			printk("[remove_expectations] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u  save_port=[%d]  master=[%p]\n",
				NIPQUAD(i->tuple.src.u3.ip),ntohs(i->tuple.src.u.udp.port),NIPQUAD(i->tuple.dst.u3.ip),ntohs(i->tuple.dst.u.udp.port),
				i->saved_proto.udp.port,i->master);

			if (del_timer(&i->timeout)) {
				nf_ct_unlink_expect(i);
				nf_ct_expect_put(i);
			}
		}
	}

	return;
}



void remove_conntrack_entries(struct nf_conn *ct) {
	if (!list_empty(&ct->derived_connections)) {
		struct nf_conn *child, *tmp;
		list_for_each_entry_safe(child, tmp, &ct->derived_connections,	derived_list) {

			printk("[rtp] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u  %u.%u.%u.%u:%u-%u.%u.%u.%u:%u	 timeout=%d \n",
				NIPQUAD( child->tuplehash[0].tuple.src.u3.ip), ntohs( child->tuplehash[0].tuple.src.u.udp.port),
				NIPQUAD( child->tuplehash[0].tuple.dst.u3.ip), ntohs( child->tuplehash[0].tuple.dst.u.udp.port),	
				NIPQUAD( child->tuplehash[1].tuple.src.u3.ip), ntohs( child->tuplehash[1].tuple.src.u.udp.port),
				NIPQUAD( child->tuplehash[1].tuple.dst.u3.ip), ntohs( child->tuplehash[1].tuple.dst.u.udp.port), child->derived_timeout ); 
		
			if (del_timer(&child->timeout)) {						
				death_by_timeout((unsigned long)child);
			}
	
		}
	}
	return;
}


int get_available_port(struct nf_conn *ct, int portRangIndex )
{
	int i, tmp_port=0, found = 0, out=0;

again:
	if (rtsp_old_port >= rtsp_max_port) {
		rtsp_old_port=rtsp_min_port;
	}

	for (rtsp_new_port=rtsp_old_port; rtsp_new_port<=rtsp_max_port; rtsp_new_port++) {	
		// If we have a range, assume rtp: start loport must be even.
		if ((rtsp_new_port & 0x0001) != 0) {
			rtsp_new_port++;
		}
		
		printk(" rtsp_new_port=[%d] client_lohiport=[%d,%d]\n",rtsp_new_port, client_loport, client_hiport);

		switch (portRangIndex) {
			case 1:
			case 2:
				for(i=0 ; i<=(client_hiport-client_loport) ; i++) {
					tmp_port = rtsp_new_port + i;

					if(tmp_port > rtsp_max_port) {
						if(out==0){
							found = 0;
							rtsp_old_port=rtsp_min_port;		
							out = 1;
							goto again;
						}else{
							found = 1;
							printk("The RTSP port has already been used up! {6970~7000} \n");
							return found;							
						}
					}
					#if 0
					exp->tuple.dst.u.udp.port = tmp_port;					
					if(lookup_expect_existing_port(exp,ct) || lookup_conntrack_existing_port(exp)){
						printk("nf_onnntrack_rtsp : Port[%d] is in used!!\n",exp->tuple.dst.u.udp.port);
						found = 1;
						break;
					}
					#endif				
				}
				rtsp_old_port = tmp_port+1;
				break;
			default:
				break;
		}

		if(found == 0){
			break;
		}
		found = 0;
	}	

	return found;
}
#endif



/* Get next message in a packet */
static int get_next_message(const char *tcpdata, int tcpdatalen,
			    int *msgoff, int *msglen, int *msghdrlen)
{
	if (*msglen == 0) { /* The first message */
		*msgoff = 0;
	} else {
		*msgoff += *msglen;
		if ((*msgoff + 4) >= tcpdatalen) /* No more message */
			return 0;
	}

	/* Get message header length */
	*msghdrlen = memmem(tcpdata+*msgoff, tcpdatalen-*msgoff, "\r\n\r\n", 4);
	if (*msghdrlen < 0) {
		*msghdrlen = *msglen = tcpdatalen - *msgoff;
	} else {
		/* Get message length including SDP */
		int cloff = memstr(tcpdata+*msgoff, *msghdrlen, "Content-Length: ", 16);
		if (cloff < 0) {
			*msglen = *msghdrlen + 4;
		} else {
			unsigned long cl = simple_strtoul(tcpdata+*msgoff+cloff+16, NULL, 10);
			*msglen = *msghdrlen + 4 + cl;
		}
	}

	return 1;
}

/* Get next client_port parameter in a Transport header */
static int get_next_client_port(const char *tcpdata, int tpoff, int tplen,
				int *portoff, int *portlen,
				__be16 *rtpport, __be16 *rtcpport,
				char *dash)
{
	int off;
	char *p;

	if (*portlen == 0) { /* The first client_port */
		*portoff = tpoff;
	} else {
		*portoff += *portlen;
		if (*portoff >= tpoff + tplen) /* No more data */
			return 0;
	}

	off = memmem(tcpdata+*portoff, tplen-(*portoff-tpoff),
		     ";client_port=", 13);
	if (off < 0)
		return 0;
	*portoff += off + 13;

	*rtpport = htons((unsigned short)simple_strtoul(tcpdata+*portoff,
							&p, 10));
	if (*p != '-' && *p != '/') {
		*dash = 0;
	} else {
		*dash = *p++;
		*rtcpport = htons((unsigned short)simple_strtoul(p, &p, 10));
	}
	*portlen = p - tcpdata - *portoff;
	return 1;
}

/* Get next destination=<ip>:<port> parameter in a Transport header
 * This is not a standard parameter, so far, it's only seen in some customers'
 * products.
 */
static int get_next_dest_ipport(const char *tcpdata, int tpoff, int tplen,
				int *destoff, int *destlen, __be32 *dest,
				int *portoff, int *portlen, __be16 *port)
{
	int off;
	char *p;

	if (*destlen == 0) { /* The first destination */
		*destoff = tpoff;
	} else {
		*destoff += *destlen + 1 + *portlen;
		if (*destoff >= tpoff + tplen) /* No more data */
			return 0;
	}

	off = memmem(tcpdata+*destoff, tplen-(*destoff-tpoff),
		     ";destination=", 13);
	if (off < 0)
		return 0;
	*destoff += off + 13;

        if (in4_pton(tcpdata+*destoff, tplen-(*destoff-tpoff), (u8 *)dest,
                     -1, (const char **)&p) == 0) {
		return 0;
	}
	*destlen = p - tcpdata - *destoff;

	if (*p != ':') {
		return 0;
	}
	*portoff = p - tcpdata + 1;

	*port = htons((unsigned short)simple_strtoul(tcpdata+*portoff, &p, 10));
	*portlen = p - tcpdata - *portoff;

	return 1;
}

/* Get next destination parameter in a Transport header */
static int get_next_destination(const char *tcpdata, int tpoff, int tplen,
				int *destoff, int *destlen, __be32 *dest)
{
	int off;
	char *p;

	if (*destlen == 0) { /* The first destination */
		*destoff = tpoff;
	} else {
		*destoff += *destlen;
		if (*destoff >= tpoff + tplen) /* No more data */
			return 0;
	}

//printk(" get_next_destination \n");

	off = memmem(tcpdata+*destoff, tplen-(*destoff-tpoff),
		     ";destination=", 13);
	if (off < 0)
		return 0;
	*destoff += off + 13;

        if (in4_pton(tcpdata+*destoff, tplen-(*destoff-tpoff), (u8 *)dest,
                     -1, (const char **)&p)) {
		*destlen = p - tcpdata - *destoff;
		return 1;
	} else {
		return 0;
	}
}

static int expect_rtsp_channel(struct sk_buff *skb, struct nf_conn *ct,
			       enum ip_conntrack_info ctinfo,
			       int portoff, int portlen,
			       __be16 rtpport, int *delta)
{
	int ret = 0;
	int dir = CTINFO2DIR(ctinfo);
	struct nf_conntrack_expect *rtp_exp;
	typeof(nat_rtsp_channel_hook) nat_rtsp_channel;

	if (rtpport == 0)
		return -1;

	/* Create expect for RTP */
	if ((rtp_exp = nf_ct_expect_alloc(ct)) == NULL)
		return -1;
/*
	nf_ct_expect_init(rtp_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
			  &ct->tuplehash[!dir].tuple.src.u3,
			  &ct->tuplehash[!dir].tuple.dst.u3,
			  IPPROTO_UDP, NULL, &rtpport);
*/
        nf_ct_expect_init(rtp_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
                          NULL,
                          &ct->tuplehash[!dir].tuple.dst.u3,
                          IPPROTO_UDP, NULL, &rtpport);

	if ((nat_rtsp_channel = rcu_dereference(nat_rtsp_channel_hook)) &&
	    ct->status & IPS_NAT_MASK) {
		/* NAT needed */
		ret = nat_rtsp_channel(skb, ct, ctinfo, portoff, portlen,
				       rtp_exp, delta);
	} else {		/* Conntrack only */
		if (nf_ct_expect_related(rtp_exp) == 0) {
			pr_debug("nf_ct_rtsp: expect RTP ");
			nf_ct_dump_tuple(&rtp_exp->tuple);
		} else
			ret = -1;
	}

	nf_ct_expect_put(rtp_exp);

	return ret;
}

static int expect_rtsp_channel2(struct sk_buff *skb, struct nf_conn *ct,
				enum ip_conntrack_info ctinfo,
				int portoff, int portlen,
				__be16 rtpport, __be16 rtcpport,
				char dash, int *delta)
{
	int ret = 0;
	int dir = CTINFO2DIR(ctinfo);
	struct nf_conntrack_expect *rtp_exp;
	struct nf_conntrack_expect *rtcp_exp;
	typeof(nat_rtsp_channel2_hook) nat_rtsp_channel2;

	if (rtpport == 0 || rtcpport == 0)
		return -1;

/*
	printk("[rtp] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u	 %u.%u.%u.%u:%u-%u.%u.%u.%u:%u dir=[%d]\n",
		NIPQUAD( ct->tuplehash[dir].tuple.src.u3.ip), ntohs( ct->tuplehash[dir].tuple.src.u.udp.port),
		NIPQUAD( ct->tuplehash[dir].tuple.dst.u3.ip), ntohs( ct->tuplehash[dir].tuple.dst.u.udp.port),  
		NIPQUAD( ct->tuplehash[!dir].tuple.src.u3.ip), ntohs( ct->tuplehash[!dir].tuple.src.u.udp.port),
		NIPQUAD( ct->tuplehash[!dir].tuple.dst.u3.ip), ntohs( ct->tuplehash[!dir].tuple.dst.u.udp.port), dir); 
*/

	/* Create expect for RTP */
	if ((rtp_exp = nf_ct_expect_alloc(ct)) == NULL)
		return -1;

	/* Create expect for RTCP */
	if ((rtcp_exp = nf_ct_expect_alloc(ct)) == NULL) {
		nf_ct_expect_put(rtp_exp);
		return -1;
	}

#if defined(CONFIG_11ac_throughput_patch_from_412L08)           

#if 0
	nf_ct_expect_init(rtp_exp, NF_CT_EXPECT_CLASS_DEFAULT ,nf_ct_l3num(ct),
			  &ct->tuplehash[!dir].tuple.src.u3,
			  &ct->tuplehash[!dir].tuple.dst.u3,
			  IPPROTO_UDP, NULL, &rtpport);
#else
        nf_ct_expect_init(rtp_exp, NF_CT_EXPECT_CLASS_DEFAULT ,nf_ct_l3num(ct),
                  NULL,
                  &ct->tuplehash[!dir].tuple.dst.u3,
                  IPPROTO_UDP, NULL, &rtpport);
#endif

#else
        nf_ct_expect_init(rtp_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
                          NULL,
                          &ct->tuplehash[!dir].tuple.dst.u3,
                          IPPROTO_UDP, NULL, &rtpport);
#endif
	nf_ct_expect_init(rtcp_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
					  NULL,
					  &ct->tuplehash[!dir].tuple.dst.u3,
					  IPPROTO_UDP, NULL, &rtcpport);

	if ((nat_rtsp_channel2 = rcu_dereference(nat_rtsp_channel2_hook)) &&
	    ct->status & IPS_NAT_MASK) {
		/* NAT needed */
		ret = nat_rtsp_channel2(skb, ct, ctinfo, portoff, portlen,
				   	rtp_exp, rtcp_exp, dash, delta);
	} else {		/* Conntrack only */
		if (nf_ct_expect_related(rtp_exp) == 0) {
			if (nf_ct_expect_related(rtcp_exp) == 0) {
				pr_debug("nf_ct_rtsp: expect RTP ");
				nf_ct_dump_tuple(&rtp_exp->tuple);
				pr_debug("nf_ct_rtsp: expect RTCP ");
				nf_ct_dump_tuple(&rtcp_exp->tuple);	
			} else {
				nf_ct_unexpect_related(rtp_exp);
				ret = -1;
			}
		} else
			ret = -1;
	}

	nf_ct_expect_put(rtp_exp);
	nf_ct_expect_put(rtcp_exp);

	return ret;
}

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
static int expect_rtsp_channel3(struct sk_buff *skb, struct nf_conn *ct,
				enum ip_conntrack_info ctinfo,
				int portoff, int portlen,
				__be16 loport, __be16 hiport,
				char dash, int *delta)
{
	int ret = 0;
	int dir = CTINFO2DIR(ctinfo);
	struct nf_conntrack_expect *rtp_exp;
	struct nf_conntrack_expect *rtcp_exp;
	struct nf_conntrack_expect *rtcp2_exp;   //For LAN to WAN	
	struct nf_conntrack_expect *rertp_exp;
	struct nf_conntrack_expect *rertp2_exp;  //For LAN to WAN	
	__be16 nowclientPort = 0;
	__be16 clientPort = 0;

	typeof(nat_rtsp_channel3_hook) nat_rtsp_channel3;


	if (loport == 0 || hiport == 0)
		return -1;



	/* Create expect for RTP */
	if ((rtp_exp = nf_ct_expect_alloc(ct)) == NULL) {
		return -1;
	}
	
	/* Create expect for RTCP  */
	if ((rtcp_exp = nf_ct_expect_alloc(ct)) == NULL) {
		nf_ct_expect_put(rtp_exp);
		return -1;
	}

	/* Create expect for RTCP2  */
	if ((rtcp2_exp = nf_ct_expect_alloc(ct)) == NULL) {
		nf_ct_expect_put(rtp_exp);
		nf_ct_expect_put(rtcp_exp);
		return -1;
	}

	/* Create expect for ReRTP */
	if ((rertp_exp = nf_ct_expect_alloc(ct)) == NULL) {
		nf_ct_expect_put(rtp_exp);
		nf_ct_expect_put(rtcp_exp);
		nf_ct_expect_put(rtcp2_exp);
		return -1;
	}

	/* Create expect for ReRTP2 */
	if ((rertp2_exp = nf_ct_expect_alloc(ct)) == NULL) {
		nf_ct_expect_put(rtp_exp);
		nf_ct_expect_put(rtcp_exp);
		nf_ct_expect_put(rtcp2_exp);
		nf_ct_expect_put(rertp_exp);		
		return -1;
	}


	nowclientPort = rtsp_new_port;
	nf_ct_expect_init(rtp_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
			NULL,
			&ct->tuplehash[!dir].tuple.dst.u3,
			IPPROTO_UDP, NULL, &nowclientPort);
	rtp_exp->saved_proto.udp.port = loport; 
	rtp_exp->tuple.src.u.udp.port = 0;
	rtp_exp->mask.src.u.udp.port  = 0;



	nowclientPort = rtsp_new_port+1;
	nf_ct_expect_init(rtcp_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
			NULL,
			&ct->tuplehash[!dir].tuple.dst.u3,
			IPPROTO_UDP, NULL, &nowclientPort);
	rtcp_exp->saved_proto.udp.port = loport+1; 
	rtcp_exp->tuple.src.u.udp.port = 0;
	rtcp_exp->mask.src.u.udp.port  = 0;


	clientPort = loport+1;
	nf_ct_expect_init(rtcp2_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
			&ct->tuplehash[dir].tuple.src.u3,
			&ct->tuplehash[dir].tuple.dst.u3,
			IPPROTO_UDP, NULL, NULL);
	memset(&rtcp2_exp->tuple.dst.u3, 0x00, sizeof(rtcp2_exp->tuple.dst.u3));
	rtcp2_exp->saved_proto.udp.port = rtsp_new_port+1; 
	rtcp_exp->tuple.src.l3num = PF_INET ;	
	rtcp2_exp->mask.src.u3.ip  = 0xffffffff; //orginal
	rtcp2_exp->tuple.src.u.udp.port = clientPort;
	rtcp2_exp->mask.src.u.udp.port  = 0xffff ;
	rtcp2_exp->tuple.dst.u.udp.port = 0;

	nowclientPort = rtsp_new_port+2;
	nf_ct_expect_init(rertp_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
			  NULL,
			  &ct->tuplehash[!dir].tuple.dst.u3,
			  IPPROTO_UDP, NULL, &nowclientPort);
	rertp_exp->saved_proto.udp.port = loport+2; 
	rertp_exp->tuple.src.u.udp.port = 0;
	rertp_exp->mask.src.u.udp.port  = 0;

	clientPort = loport+2;
	nf_ct_expect_init(rertp2_exp, NF_CT_EXPECT_CLASS_DEFAULT, nf_ct_l3num(ct),
			  &ct->tuplehash[dir].tuple.src.u3,
			  &ct->tuplehash[dir].tuple.dst.u3,
			  IPPROTO_UDP, NULL, NULL);	
	memset(&rertp2_exp->tuple.dst.u3, 0x00, sizeof(rertp2_exp->tuple.dst.u3));
	rertp2_exp->saved_proto.udp.port = rtsp_new_port+2; 
	rertp2_exp->tuple.src.l3num = PF_INET ;
	rertp2_exp->mask.src.u3.ip  = 0xffffffff; //orginal
	rertp2_exp->tuple.src.u.udp.port = clientPort;
	rertp2_exp->mask.src.u.udp.port  = 0xffff ;
	rertp2_exp->tuple.dst.u.udp.port = 0;


	if ((nat_rtsp_channel3 = rcu_dereference(nat_rtsp_channel3_hook)) &&
	    ct->status & IPS_NAT_MASK) {
		/* NAT needed */
		ret = nat_rtsp_channel3(skb, ct, ctinfo, portoff, portlen,
				   	rtp_exp, rtcp_exp, rtcp2_exp, rertp_exp, rertp2_exp, dash, delta);
	} else {		/* Conntrack only */
		if (nf_ct_expect_related(rtp_exp) == 0) {
			if ((nf_ct_expect_related(rtcp_exp) == 0) && (nf_ct_expect_related(rtcp2_exp) == 0)
					&& (nf_ct_expect_related(rertp_exp) == 0) && (nf_ct_expect_related(rertp2_exp) == 0)) {
				pr_debug("nf_ct_rtsp: expect RTP ");
				nf_ct_dump_tuple(&rtp_exp->tuple);
				pr_debug("nf_ct_rtsp: expect RTCP ");
				nf_ct_dump_tuple(&rtcp_exp->tuple);
				pr_debug("nf_ct_rtsp2: expect RTCP2 ");
				nf_ct_dump_tuple(&rtcp2_exp->tuple);				
				pr_debug("nf_ct_rertp: expect ReTrasm ");
				nf_ct_dump_tuple(&rertp_exp->tuple);
				pr_debug("nf_ct_rertp2: expect ReTrasm2 ");
				pr_debug("\n");
				nf_ct_dump_tuple(&rertp2_exp->tuple);				
			} else {
				nf_ct_unexpect_related(rtp_exp);
				ret = -1;
			}
		} else
			ret = -1;
	}

	nf_ct_expect_put(rtp_exp);
	nf_ct_expect_put(rtcp_exp);
	nf_ct_expect_put(rtcp2_exp);
	nf_ct_expect_put(rertp_exp);
	nf_ct_expect_put(rertp2_exp);

	return ret;
}
#endif

static void set_normal_timeout(struct nf_conn *ct, struct sk_buff *skb)
{
	struct nf_conn *child;

	/* nf_conntrack_lock is locked inside __nf_ct_refresh_acct, locking here results in a deadlock */
	/* write_lock_bh(&nf_conntrack_lock); */ 
	list_for_each_entry(child, &ct->derived_connections, derived_list) {
		child->derived_timeout = 5*HZ;
		nf_ct_refresh(child, skb, 5*HZ);
	}
	/* write_unlock_bh(&nf_conntrack_lock); */
}

static void set_long_timeout(struct nf_conn *ct, struct sk_buff *skb)
{
	struct nf_conn *child;

	/* write_lock_bh(&nf_conntrack_lock); */
	list_for_each_entry(child, &ct->derived_connections, derived_list) {
#if defined(CONFIG_11ac_throughput_patch_from_412L07)
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BLOG)
		if ( (child->blog_key[IP_CT_DIR_ORIGINAL] != BLOG_KEY_NONE)
			|| (child->blog_key[IP_CT_DIR_REPLY] != BLOG_KEY_NONE) )
		{
			/* remove flow from flow cache */
                        blog_notify(DESTROY_FLOWTRACK, (void*)child,
                                                (uint32_t)child->blog_key[IP_CT_DIR_ORIGINAL],
                                                (uint32_t)child->blog_key[IP_CT_DIR_REPLY]);

                        /* Safe: In case blog client does not set key to 0 explicilty */
                        child->blog_key[IP_CT_DIR_ORIGINAL] = BLOG_KEY_NONE;
                        child->blog_key[IP_CT_DIR_REPLY]    = BLOG_KEY_NONE;
		}
#endif
#endif
		nf_ct_refresh(child, skb, 3600*HZ);
	}
	/*	write_unlock_bh(&nf_conntrack_lock); */
}

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
static inline int refresh_timer(struct nf_conntrack_expect *i)
{
	struct nf_conn_help *master_help = nfct_help(i->master);
	const struct nf_conntrack_expect_policy *p;

	if (!del_timer(&i->timeout))
		return 0;

	p = &master_help->helper->expect_policy[i->class];
	i->timeout.expires = jiffies + p->timeout * HZ;
	add_timer(&i->timeout);
	return 1;
}


static void refresh_expect_timer(struct nf_conn *ct)
{
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_conntrack_expect *exp;
	struct hlist_node *n, *next;

	/* Optimization: most connection never expect any others. */
	if (!help)
		return;

	hlist_for_each_entry_safe(exp, n, next, &help->expectations, lnode) {
		
	//	printk("[rertp] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u	save_port=[%d] master=[%p] dir=[%d]\n",
	//		NIPQUAD(exp->tuple.src.u3.ip),ntohs(exp->tuple.src.u.udp.port),NIPQUAD(exp->tuple.dst.u3.ip),ntohs(exp->tuple.dst.u.udp.port),
	//		exp->saved_proto.udp.port,exp->master, exp->dir); 
		
		refresh_timer(exp);
	}
	return;

}
#endif

static int help(struct sk_buff *skb, unsigned int protoff,
		struct nf_conn *ct, enum ip_conntrack_info ctinfo)
{
	int dir = CTINFO2DIR(ctinfo);
	struct nf_conn_help *hlp = nfct_help(ct);
	struct tcphdr _tcph, *th;
	unsigned int tcpdataoff, tcpdatalen;
	char *tcpdata;
	int msgoff, msglen, msghdrlen;
	int tpoff, tplen;
	int portlen = 0;
	int portoff = 0;
	__be16 rtpport = 0;
	__be16 rtcpport = 0;
	char dash = 0;
	int destlen = 0;
	int destoff = 0;
	__be32 dest = 0;
#if FIX_ACK_ERROR_ISSUE
	int record_request = 0; 
#endif
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
	int portRangIndex = 0;
#endif

	typeof(nat_rtsp_modify_addr_hook) nat_rtsp_modify_addr;
	typeof(nat_rtsp_modify_port_hook) nat_rtsp_modify_port;
	typeof(nat_rtsp_modify_port2_hook) nat_rtsp_modify_port2;
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
	typeof(nat_rtsp_modify_port3_hook) nat_rtsp_modify_port3;
#endif

#if FIX_ACK_ERROR_ISSUE
	typeof(modify_setup_mesg_hook) modify_setup_mesg;
	typeof(modify_reply_mesg_hook) modify_reply_mesg;
	typeof(clean_ipport_buf_hook) clean_ipport_buf;
#endif

	/* Until there's been traffic both ways, don't look in packets. */
	if (ctinfo != IP_CT_ESTABLISHED
	    && ctinfo != IP_CT_ESTABLISHED+IP_CT_IS_REPLY) {
		return NF_ACCEPT;
	}
	//pr_debug("nf_ct_rtsp: skblen = %u\n", skb->len);

	/* Get TCP header */
	th = skb_header_pointer(skb, protoff, sizeof(_tcph), &_tcph);
	if (th == NULL) {
		return NF_ACCEPT;
	}

	/* Get TCP payload offset */
	tcpdataoff = protoff + th->doff * 4;
	if (tcpdataoff >= skb->len) { /* No data? */
		return NF_ACCEPT;
	}

	/* Get TCP payload length */
	tcpdatalen = skb->len - tcpdataoff;

	spin_lock_bh(&nf_rtsp_lock);

	/* Get TCP payload pointer */
	tcpdata = skb_header_pointer(skb, tcpdataoff, tcpdatalen, rtsp_buffer);
	BUG_ON(tcpdata == NULL);

	/* There may be more than one message in a packet, check them
	 * one by one */
	msgoff = msglen = msghdrlen = 0;
	while(get_next_message(tcpdata, tcpdatalen, &msgoff, &msglen,
			       &msghdrlen)) {
#if FIX_ACK_ERROR_ISSUE
		record_request = 0; 
#endif		   
		/* Messages from LAN side through MASQUERADED connections */
		if (memcmp(&ct->tuplehash[dir].tuple.src.u3,
			   &ct->tuplehash[!dir].tuple.dst.u3,
			   sizeof(ct->tuplehash[dir].tuple.src.u3)) != 0) {
			if(memcmp(tcpdata+msgoff, "PAUSE ", 6) == 0) {
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
				int cseq = memmem(tcpdata+msgoff, msglen, "CSeq: ", 6);
				if(cseq == -1) {
				        /* Fix the IOP issue with DSS on Drawin system */
				        cseq = memmem(tcpdata+msgoff, msglen, "Cseq: ", 6);
				        if(cseq == -1) {
					   pr_debug("nf_ct_rtsp: wrong PAUSE msg\n");
				        } else {
					   cseq = get_cseq(tcpdata+msgoff+cseq+6);
				        }
				} else {
					cseq = get_cseq(tcpdata+msgoff+cseq+6);
				}
				
				pr_debug("nf_ct_rtsp: PAUSE, CSeq=%d\n", cseq);
				hlp->help.ct_rtsp_info.paused = cseq;
#else
				pr_debug("nf_ct_rtsp: PAUSE\n");
				hlp->help.ct_rtsp_info.paused = 1;
#endif
				continue;
			} else {
				hlp->help.ct_rtsp_info.paused = 0;
			}
			if(memcmp(tcpdata+msgoff, "TEARDOWN ", 9) == 0) {
				pr_debug("nf_ct_rtsp: TEARDOWN\n");
				set_normal_timeout(ct, skb);
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
				teardown_flag=1;
#endif
				continue;
			} 
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
			else if(memcmp(tcpdata+msgoff, "GET_PARAMETER ", 14) == 0) {
				refresh_expect_timer(ct);
				continue;
			}
#endif
			else if(memcmp(tcpdata+msgoff, "SETUP ", 6) != 0) {
				continue;
			}

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
			spin_lock_bh(&nf_conntrack_lock);
			remove_stay_expectations(ct);
			spin_unlock_bh(&nf_conntrack_lock);
#endif
			
#if FIX_ACK_ERROR_ISSUE
			record_request = 1;	 //setup
#endif	
			/* Now begin to process SETUP message */
			pr_debug("nf_ct_rtsp: SETUP\n");
		/* Reply message that's from WAN side. */
		} else {
			/* We only check replies */
			if(memcmp(tcpdata+msgoff, "RTSP/", 5) != 0)
				continue;
			
			pr_debug("nf_ct_rtsp: Reply message\n");

		 	/* Response to a previous PAUSE message */
			if (hlp->help.ct_rtsp_info.paused) {
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
				int cseq = memmem(tcpdata+msgoff, msglen, "CSeq: ", 6);
				if(cseq == -1) {
				        /* Fix the IOP issue with DSS on Drawin system */
				        cseq = memmem(tcpdata+msgoff, msglen, "Cseq: ", 6);
				        if(cseq == -1) {
					   pr_debug("nf_ct_rtsp: wrong reply msg\n");
				        } else {
					   cseq = get_cseq(tcpdata+msgoff+cseq+6);
				        }
				} else {
					cseq = get_cseq(tcpdata+msgoff+cseq+6);
				}
				if(cseq == hlp->help.ct_rtsp_info.paused) {
#endif
				pr_debug("nf_ct_rtsp: Reply to PAUSE\n");
				set_long_timeout(ct, skb);
				hlp->help.ct_rtsp_info.paused = 0;
				goto end;
			}
#if defined(CONFIG_11ac_throughput_patch_from_412L08)			
			}
#endif
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
			if (teardown_flag == 1) {
				/* destroy the conntrack entries associated with specified master ct */
				remove_conntrack_entries(ct);
				/* destroy the conntrack_expect entries associated with specified master ct */
				spin_lock_bh(&nf_conntrack_lock);
				nf_ct_remove_expectations(ct);
				spin_unlock_bh(&nf_conntrack_lock);
				teardown_flag = 0;
			}	
#endif
#if FIX_ACK_ERROR_ISSUE
			record_request = 2;	  //reply
#endif
			/* Now begin to process other reply message */
		}

		/* Get Transport header offset */
		tpoff = memmem(tcpdata+msgoff+6, msghdrlen-6, 
			       "\r\nTransport:", 12);
		if (tpoff < 0){
			continue;
		}
		
		tpoff += msgoff + 6 + 12; 
		
		/* Get Transport header length */
		tplen = memmem(tcpdata+tpoff, msghdrlen-(tpoff - msgoff),
			       "\r\n", 2);
		if (tplen < 0)
			tplen = msghdrlen - (tpoff - msgoff);

		/* There maybe more than one client_port parameter in this
		 * field, we'll process each of them. I know not all of them
		 * are unicast UDP ports, but that is the only situation we
		 * care about so far. So just KISS. */

#if FIX_ACK_ERROR_ISSUE
		clean_ipport_buf = rcu_dereference(clean_ipport_buf_hook);
		if (clean_ipport_buf){
			clean_ipport_buf();
		}
#endif
		
		portoff = portlen = 0;
		while(get_next_client_port(tcpdata, tpoff, tplen,
					   &portoff, &portlen,
					   &rtpport, &rtcpport, &dash)) {
			int ret=0, delta=0;

	//		printk("[ct] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u  %u.%u.%u.%u:%u-%u.%u.%u.%u:%u dir=[%d]  portlen=%d \n",
	//			NIPQUAD( ct->tuplehash[dir].tuple.src.u3.ip), ntohs( ct->tuplehash[dir].tuple.src.u.udp.port),
	//			NIPQUAD( ct->tuplehash[dir].tuple.dst.u3.ip), ntohs( ct->tuplehash[dir].tuple.dst.u.udp.port),	
	//			NIPQUAD( ct->tuplehash[!dir].tuple.src.u3.ip), ntohs( ct->tuplehash[!dir].tuple.src.u.udp.port),
	//			NIPQUAD( ct->tuplehash[!dir].tuple.dst.u3.ip), ntohs( ct->tuplehash[!dir].tuple.dst.u.udp.port), dir, portlen ); 


			if (memcmp(&ct->tuplehash[dir].tuple.src.u3,
			   	   &ct->tuplehash[!dir].tuple.dst.u3,
			   	   sizeof(ct->tuplehash[dir].tuple.src.u3))
			    != 0) {

		//		printk("LAN to WAN   \n");
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
				client_loport = rtpport;
				client_hiport = rtcpport;
				portRangIndex = client_hiport - client_loport;
				get_available_port(ct, portRangIndex);
		//		printk("get_available_port =%d \n", portRangIndex );			
#endif
				
				/* LAN to WAN */
				if (dash == 0) {
					/* Single data channel */
					ret = expect_rtsp_channel(skb, ct,
								  ctinfo,
			 					  portoff,
								  portlen,
								  rtpport,
								  &delta);
				}
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
				else if (portRangIndex == 2 ){
						/* A range of data channels (RTP/RTCP/Retrasm) */
						ret = expect_rtsp_channel3(skb, ct,
									   ctinfo,
									   portoff,
									   portlen,
									   client_loport,
									   client_hiport,
									   dash,
									   &delta);
				}
#endif				
				else {
					/* A pair of data channels (RTP/RTCP)*/
					ret = expect_rtsp_channel2(skb, ct,
								   ctinfo,
								   portoff,
								   portlen,
								   rtpport,
								   rtcpport,
								   dash,
								   &delta);
				}

				/* register the RTP ports with ingress QoS classifier */
				iqos_add_L4port(IPPROTO_UDP, client_loport, IQOS_ENT_DYN, IQOS_PRIO_HIGH);
				iqos_add_L4port(IPPROTO_UDP, client_hiport, IQOS_ENT_DYN, IQOS_PRIO_HIGH);
			
			} else {
				nat_rtsp_modify_port = rcu_dereference(
					nat_rtsp_modify_port_hook);
				nat_rtsp_modify_port2 = rcu_dereference(
					nat_rtsp_modify_port2_hook);
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
				nat_rtsp_modify_port3 = rcu_dereference(
					nat_rtsp_modify_port3_hook);
				portRangIndex = rtcpport- rtpport;
#endif
		//		printk("WAN to LAN	  \n");

				/* WAN to LAN */
				if (dash == 0 ) {
					/* Single data channel */
					if (nat_rtsp_modify_port) {
					ret = nat_rtsp_modify_port(skb, ct,
								   ctinfo,
								   portoff,
								   portlen,
								   rtpport,
								   &delta);
					}
				}
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
				else if (portRangIndex == 2 ){
					/* A range of data channels (RTP/RTCP/Retrasm) */
					if (nat_rtsp_modify_port3) {
						ret = nat_rtsp_modify_port3(skb, ct,
									    ctinfo,
									    portoff,
									    portlen,
									    rtpport,
									    rtcpport,
									    dash,
									    &delta);
					}	
				}
#endif		
				else {
					/* A pair of data channels (RTP/RTCP)*/
					if (nat_rtsp_modify_port2) {
					ret = nat_rtsp_modify_port2(skb, ct,
								    ctinfo,
								    portoff,
								    portlen,
								    rtpport,
								    rtcpport,
								    dash,
								    &delta);
					}
				}
				
				/* register the RTP ports with ingress QoS classifier */
	            iqos_add_L4port(IPPROTO_UDP, rtpport, IQOS_ENT_DYN, IQOS_PRIO_HIGH);
	            iqos_add_L4port(IPPROTO_UDP, rtcpport, IQOS_ENT_DYN, IQOS_PRIO_HIGH);
			
			}

#ifndef FIX_ACK_ERROR_ISSUE
			if (ret < 0) 
				goto end;

			if (delta) {
				/* Packet length has changed, we need to adjust
				 * everthing */
				tcpdatalen += delta;
				msglen += delta;
				msghdrlen += delta;
				tplen += delta;
				portlen += delta;

				/* Relocate TCP payload pointer */
				tcpdata = skb_header_pointer(skb,
							     tcpdataoff,
							     tcpdatalen,
							     rtsp_buffer);
				BUG_ON(tcpdata == NULL);
			}
#endif
		}

		/* Process special destination=<ip>:<port> parameter in 
		 * Transport header. This is not a standard parameter,
		 * so far, it's only seen in some customers' products.
 		 */
		while(get_next_dest_ipport(tcpdata, tpoff, tplen,
					   &destoff, &destlen, &dest,
					   &portoff, &portlen, &rtpport)) {
			int ret = 0, delta;

			/* Process the port part */
			if (memcmp(&ct->tuplehash[dir].tuple.src.u3,
			   	   &ct->tuplehash[!dir].tuple.dst.u3,
			   	   sizeof(ct->tuplehash[dir].tuple.src.u3))
			    != 0) {
				/* LAN to WAN */
			//	printk(" LAN to WAN \n");
				ret = expect_rtsp_channel(skb, ct, ctinfo,
							  portoff, portlen,
							  rtpport, &delta);
			} else {
				/* WAN to LAN */
			//	printk(" WAN to LAN \n");
				if ((nat_rtsp_modify_port = rcu_dereference(
					nat_rtsp_modify_port_hook))) {
				ret = nat_rtsp_modify_port(skb, ct,
							   ctinfo,
							   portoff,
							   portlen,
							   rtpport,
							   &delta);
				}
			}
			
            /* register the RTP ports with ingress QoS classifier */
			//printk("\n RTP Port = %d\n", rtpport);
            iqos_add_L4port(IPPROTO_UDP, rtpport, IQOS_ENT_DYN, IQOS_PRIO_HIGH);

#ifndef FIX_ACK_ERROR_ISSUE	
			if (ret < 0)
				goto end;

			if (delta) {
				/* Packet length has changed, we need to adjust
				 * everthing */
				tcpdatalen += delta;
				msglen += delta;
				msghdrlen += delta;
				tplen += delta;
				portlen += delta;

				/* Relocate TCP payload pointer */
				tcpdata = skb_header_pointer(skb,
							     tcpdataoff,
							     tcpdatalen,
							     rtsp_buffer);
				BUG_ON(tcpdata == NULL);
			}
#endif
				
			/* Then the IP part */
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
			if (dest != ct->tuplehash[dir].tuple.dst.u3.ip) {
			    continue;
			}
#else
			if (dest != ct->tuplehash[dir].tuple.src.u3.ip)
				continue;
#endif
			if ((nat_rtsp_modify_addr =
			     rcu_dereference(nat_rtsp_modify_addr_hook)) &&
			    ct->status & IPS_NAT_MASK) {
			}
			/* NAT needed */
			ret = nat_rtsp_modify_addr(skb, ct, ctinfo,
						   destoff, destlen, &delta);


#ifndef FIX_ACK_ERROR_ISSUE		
			if (ret < 0)
				goto end;

			if (delta) {
				/* Packet length has changed, we need
				 * to adjust everthing */
				tcpdatalen += delta;
				msglen += delta;
				msghdrlen += delta;
				tplen += delta;
				portlen += delta;

				/* Relocate TCP payload pointer */
				tcpdata = skb_header_pointer(skb, tcpdataoff,
							     tcpdatalen,
							     rtsp_buffer);
				BUG_ON(tcpdata == NULL);
			}
#endif
		}

		/* Process the SETUP destination IP part */
		if ((nat_rtsp_modify_addr =
		     rcu_dereference(nat_rtsp_modify_addr_hook)) &&
		    ct->status & IPS_NAT_MASK) {
			destoff = destlen = 0;
			while(get_next_destination(tcpdata, tpoff, tplen,
					   	   &destoff, &destlen, &dest)) {
				int ret, delta;

				//printk("\n  ~~~~~~~~~~~~~get_next_destination~~~~~~~~~~ \n");

				/* For LAN to WAN */		
				if (dest != ct->tuplehash[dir].tuple.src.u3.ip)
					continue;

				/* NAT needed */
				ret = nat_rtsp_modify_addr(skb, ct, ctinfo,
							   destoff, destlen,
				   			   &delta);

#ifndef FIX_ACK_ERROR_ISSUE
				if (ret < 0)
					goto end;

				if (delta) {
					/* Packet length has changed, we need
					 * to adjust everthing */
					tcpdatalen += delta;
					msglen += delta;
					msghdrlen += delta;
					tplen += delta;
					portlen += delta;

					/* Relocate TCP payload pointer */
					tcpdata = skb_header_pointer(skb,
								 tcpdataoff,
								 tcpdatalen,
								 rtsp_buffer);
					BUG_ON(tcpdata == NULL);
				}
#endif

			}
		}

#if FIX_ACK_ERROR_ISSUE
		if( record_request == 1) {

			int  delta, ret=0;

			modify_setup_mesg = rcu_dereference( modify_setup_mesg_hook );
			/* NAT needed */


			if ( modify_setup_mesg ){
				ret = modify_setup_mesg(skb, ct, ctinfo, tcpdata, tpoff, tplen, &delta);
			}

			if (ret < 0)
				goto end;

			if (delta) {
				/* Packet length has changed, we need
				 * to adjust everthing */

				//printk("delta = %d	\n", delta);
				
				tcpdatalen += delta;
				msglen += delta;
				msghdrlen += delta;
				tplen += delta;
				portlen += delta;

				/* Relocate TCP payload pointer */
				tcpdata = skb_header_pointer(skb,
							 tcpdataoff,
							 tcpdatalen,
							 rtsp_buffer);
				BUG_ON(tcpdata == NULL);
			}
		}
		else if( record_request == 2) {

				int ret=0, delta=0;

				modify_reply_mesg = rcu_dereference( modify_reply_mesg_hook );
				/* NAT needed */							
				if ( modify_reply_mesg ){
					ret = modify_reply_mesg(skb, ct, ctinfo, tcpdata, tpoff, tplen, &delta);
				}

				if (ret < 0)
					goto end;
				
				if (delta) {
					/* Packet length has changed, we need
					 * to adjust everthing */
				
					//printk("delta = %d	\n", delta);
					
					tcpdatalen += delta;
					msglen += delta;
					msghdrlen += delta;
					tplen += delta;
					portlen += delta;
				
					/* Relocate TCP payload pointer */
					tcpdata = skb_header_pointer(skb,
								 tcpdataoff,
								 tcpdatalen,
								 rtsp_buffer);
					BUG_ON(tcpdata == NULL);
				}

		}
#endif
	}

end:
	spin_unlock_bh(&nf_rtsp_lock);
	return NF_ACCEPT;
}

static struct nf_conntrack_helper rtsp[MAX_PORTS];
static char rtsp_names[MAX_PORTS][sizeof("rtsp-65535")];
static struct nf_conntrack_expect_policy rtsp_exp_policy;

/* don't make this __exit, since it's called from __init ! */
static void nf_conntrack_rtsp_fini(void)
{
	int i;

	for (i = 0; i < ports_c; i++) {
		if (rtsp[i].me == NULL)
			continue;

        /* unregister the RTSP ports with ingress QoS classifier */
        iqos_rem_L4port( rtsp[i].tuple.dst.protonum, 
                         rtsp[i].tuple.src.u.tcp.port, IQOS_ENT_STAT );
		pr_debug("nf_ct_rtsp: unregistering helper for port %d\n",
		       	 ports[i]);
		nf_conntrack_helper_unregister(&rtsp[i]);
	}

	kfree(rtsp_buffer);
}

static int __init nf_conntrack_rtsp_init(void)
{
	int i, ret = 0;
	char *tmpname;

	rtsp_buffer = kmalloc(4000, GFP_KERNEL);
	if (!rtsp_buffer)
		return -ENOMEM;

	if (ports_c == 0)
		ports[ports_c++] = RTSP_PORT;

	rtsp_exp_policy.max_expected = max_outstanding;
	rtsp_exp_policy.timeout	= 5 * 60;
	for (i = 0; i < ports_c; i++) {
		rtsp[i].tuple.src.l3num = PF_INET;
		rtsp[i].tuple.src.u.tcp.port = htons(ports[i]);
		rtsp[i].tuple.dst.protonum = IPPROTO_TCP;
		rtsp[i].expect_policy = &rtsp_exp_policy;
		rtsp[i].expect_class_max = 1;
		rtsp[i].me = THIS_MODULE;
		rtsp[i].help = help;
		tmpname = &rtsp_names[i][0];
		if (ports[i] == RTSP_PORT)
			sprintf(tmpname, "rtsp");
		else
			sprintf(tmpname, "rtsp-%d", ports[i]);
		rtsp[i].name = tmpname;

		pr_debug("nf_ct_rtsp: registering helper for port %d\n",
		       	 ports[i]);
		ret = nf_conntrack_helper_register(&rtsp[i]);
		if (ret) {
			printk("nf_ct_rtsp: failed to register helper "
			       "for port %d\n", ports[i]);
			nf_conntrack_rtsp_fini();
			return ret;
		}

        /* register the RTSP ports with ingress QoS classifier */
        iqos_add_L4port( IPPROTO_TCP, ports[i], IQOS_ENT_STAT, IQOS_PRIO_HIGH );
	}

	return 0;
}

module_init(nf_conntrack_rtsp_init);
module_exit(nf_conntrack_rtsp_fini);
