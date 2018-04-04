/*
 * RTSP extension for NAT alteration.
 *
 * Copyright (c) 2008 Broadcom Corporation.
 *
 * <:label-BRCM:2011:DUAL/GPL:standard
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * :>
 *
 */

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/tcp.h>
#include <net/tcp.h>

#include <net/netfilter/nf_conntrack_core.h>
#include <net/netfilter/nf_conntrack_helper.h>
#include <net/netfilter/nf_conntrack_expect.h>
#include <net/netfilter/nf_nat.h>
#include <net/netfilter/nf_nat_helper.h>
#include <net/netfilter/nf_nat_rule.h>
#include <linux/netfilter/nf_conntrack_rtsp.h>

#ifdef pr_debug
#undef pr_debug
#define pr_debug(fmt, ...) printk(fmt, ##__VA_ARGS__)
#endif


#define FIX_ACK_ERROR_ISSUE 1

#if FIX_ACK_ERROR_ISSUE
char port_tmp[sizeof("65535")];
int	 port_len = 0;
char portRang_tmp[sizeof("65535-65535")];
int	 portRang_len = 0;
char ipaddr_tmp[sizeof("255.255.255.255")];
int	 ipaddr_len = 0;
#endif

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE

/* delete matched expectations for this expect session */
void remove_match_expectations(struct nf_conntrack_expect *exp, struct nf_conn *ct)
{
	struct nf_conntrack_expect *i;
	struct hlist_node *n;
	struct net *net = nf_ct_net(ct);
	unsigned int bucket = 0;

	printk("remove_match_expectations %u.%u.%u.%u:%u-%u.%u.%u.%u:%u  save_port=[%d]  master=[%p]\n",
		NIPQUAD(exp->tuple.src.u3.ip),ntohs(exp->tuple.src.u.udp.port),NIPQUAD(exp->tuple.dst.u3.ip),ntohs(exp->tuple.dst.u.udp.port),
		exp->saved_proto.udp.port,exp->master);

	for (bucket = 0; bucket < nf_ct_expect_hsize; bucket++) {
		hlist_for_each_entry(i, n, &net->ct.expect_hash[bucket], hnode) {
			if (i->master == exp->master && i->tuple.dst.u.udp.port == exp->saved_proto.udp.port && del_timer(&i->timeout)) {
				printk("[LAN_init] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u  save_port=[%d]  master=[%p]\n",
					NIPQUAD(i->tuple.src.u3.ip),ntohs(i->tuple.src.u.udp.port),NIPQUAD(i->tuple.dst.u3.ip),ntohs(i->tuple.dst.u.udp.port),
					i->saved_proto.udp.port,i->master);
				nf_ct_unlink_expect(i);
				nf_ct_expect_put(i);
				return;
	 		}
		}
	}


	for (bucket = 0; bucket < nf_ct_expect_hsize; bucket++) {
		hlist_for_each_entry(i, n, &net->ct.expect_hash[bucket], hnode) {
			if (i->master == exp->master && i->tuple.src.u.udp.port == exp->saved_proto.udp.port && del_timer(&i->timeout)) {
				printk("[WAN_init] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u  save_port=[%d]  master=[%p]\n",
					NIPQUAD(i->tuple.src.u3.ip),ntohs(i->tuple.src.u.udp.port),NIPQUAD(i->tuple.dst.u3.ip),ntohs(i->tuple.dst.u.udp.port),
					i->saved_proto.udp.port,i->master);
				nf_ct_unlink_expect(i);
				nf_ct_expect_put(i);
				return;
	 		}
		}
	}

	
}

void rtsp_ext_expected(struct nf_conn *ct, struct nf_conntrack_expect *exp)
{
	nf_nat_follow_master(ct, exp);
	/*when match someone expect session that shoule remove the same session in nf_conntrack_expect table.*/
	remove_match_expectations(exp, ct);
	//rtsp_rtp_connected(ct);
}

void rtsp_ext_expected_client(struct nf_conn *ct, struct nf_conntrack_expect *exp)
{
	rtsp_follow_master_client(ct, exp);
	/*when match someone expect session that shoule remove the same session in nf_conntrack_expect table.*/
	remove_match_expectations(exp, ct);
	//rtsp_rtp_connected(ct);
}
#endif


#if FIX_ACK_ERROR_ISSUE
static int memmem(char *haystack, int haystacklen,
		  char *needle, int needlelen)
{
	char *p = haystack;
	int l = haystacklen - needlelen + 1;
	char c = *needle;

	while(l-- > 0) { 
		if (*p++ == c) {
			if (memcmp(p, needle+1, needlelen-1) == 0)
				return p - haystack - 1;
		}
	}
	return -1;
}


/****************************************************************************/
static int modify_reply_mesg(struct sk_buff *skb, struct nf_conn *ct,
			 	  enum ip_conntrack_info ctinfo, const char *tpdate, int matchoff, int matchlen,
			 	  int *delta)
{
	char trans_str[150]={0};
	int trans_len = 0;
	char str_buf[150];
	char buf[150];
	int len_buf = 0;
	int cmp_len = 0;
	int stroff =0;
	int ismatch = 0;
	char *p = NULL;


	snprintf(str_buf, matchlen+1 , "%s", tpdate+matchoff  );
//	printk("modify_reply_mesg=%s %d \n", str_buf  , matchlen );


	len_buf = matchlen+1;
	p = &str_buf[0];


	//printk("trans_str0 =%s	\n" , trans_str );		

	while((stroff = memmem(str_buf, len_buf, ";", 1))){
		//printk("tpoff =%d  %d\n" , stroff , len_buf );

		ismatch = 0;
		if (len_buf >= 13  ){
			cmp_len =13;
			snprintf(buf, cmp_len , "%s", str_buf);
			//printk("buf =%s  \n" , buf );

			if ( strcmp(buf ,"client_port=") == 0 && strcmp(portRang_tmp ,"\0")){
				sprintf(buf ,"client_port=%s" ,  portRang_tmp );
				strcat(trans_str ,buf); 	
				//printk("2trans_str =%s	\n" , trans_str );		
				ismatch = 1;
			}
		}
		
		if (len_buf >= 13  ){
			cmp_len =13;
			snprintf(buf, cmp_len , "%s", str_buf);
			//printk("buf =%s  \n" , buf );

			if ( strcmp(buf ,"destination=") == 0 && strcmp(ipaddr_tmp ,"\0")){
				sprintf(buf ,"destination=%s" ,  ipaddr_tmp);

				strcat(trans_str ,buf); 

				if (strcmp(port_tmp ,"\0")){
					//printk("port_tmp =%s	\n" , port_tmp );	
					strcat(trans_str , ":"); 
					strcat(trans_str , port_tmp);
				}

				
				//printk("2trans_str =%s	\n" , trans_str );		
				ismatch = 1;
			}
		}
		
		if (ismatch == 0){
		//	printk("trans_str =%s str_buf =%s   %d\n" , trans_str , str_buf  , stroff );	
			
			strncat(trans_str ,str_buf, stroff);	
		//	printk("trans_str =%s  \n" , trans_str );			
		}

		if (stroff < 0){
			break;
		}


		strcat(trans_str ,";"); 
		sprintf(str_buf , "%s" ,  p + stroff+1 );	
//		printk("str_buf2 =%s  \n" , str_buf );	
		len_buf -= (stroff+1);

	}
	trans_len = strlen(trans_str);


//printk("trans_str end =%s %d  \n" , trans_str  ,  trans_len );	


	printk("modify_ports trans_buf =%s  len=%d  matchoff=%x  matchlen=%x\n ",trans_str, trans_len, matchoff, matchlen);

	if (!nf_nat_mangle_tcp_packet(skb, ct, ctinfo, matchoff, matchlen, trans_str, trans_len)) {
		if (net_ratelimit())
			printk("nf_nat_rtsp: nf_nat_mangle_tcp_packet error\n");
		return -1;
	}
	*delta = trans_len - matchlen;

	return 0;
}


/****************************************************************************/
static int modify_setup_mesg(struct sk_buff *skb, struct nf_conn *ct,
			 	  enum ip_conntrack_info ctinfo, const char *tpdate, int matchoff, int matchlen,
			 	  int *delta)
{
	char trans_str[150]={0};
	int trans_len = 0;
	char str_buf[150];
	char buf[150];
	int len_buf = 0;
	int cmp_len = 0;
	int stroff =0;
	int ismatch = 0;
	char *p = NULL;


	snprintf(str_buf, matchlen+1 , "%s", tpdate+matchoff  );
	//printk("modify_setup_mesg=%s %d \n", str_buf  , matchlen );



	len_buf = matchlen+1;
	p = &str_buf[0];


	//printk("trans_str0 =%s	\n" , trans_str );		

	while((stroff = memmem(str_buf, len_buf, ";", 1))){
		//printk("tpoff =%d  %d\n" , stroff , len_buf );

		ismatch = 0;
		if (len_buf >= 13  ){
			cmp_len =13;
			snprintf(buf, cmp_len , "%s", str_buf);
			//printk("buf =%s  \n" , buf );

			if ( strcmp(buf ,"client_port=") == 0 && strcmp(portRang_tmp,"")){
				sprintf(buf ,"client_port=%s" ,  portRang_tmp );
				strcat(trans_str ,buf); 	
				//printk("2trans_str =%s	\n" , trans_str );		
				ismatch = 1;
			}
		}
		
		if (len_buf >= 13  ){
			cmp_len =13;
			snprintf(buf, cmp_len , "%s", str_buf);
			//printk("buf =%s  \n" , buf );

			if ( (strcmp(buf ,"destination=") == 0) && strcmp(ipaddr_tmp ,"") ){
				sprintf(buf ,"destination=%s" ,  ipaddr_tmp);

				strcat(trans_str ,buf); 	
				//printk("2trans_str =%s	\n" , trans_str );		
				ismatch = 1;
			}
		}
		
		if (ismatch == 0){
			strncat(trans_str ,str_buf, stroff);	
			//printk("trans_str =%s  \n" , trans_str );			
		}

		if (stroff < 0){
			break;
		}


		strcat(trans_str ,";"); 

		sprintf(str_buf , "%s" ,  p + stroff+1 ); 	
		//printk("str_buf2 =%s  \n" , str_buf );	
		len_buf -= (stroff+1);

	}
	trans_len = strlen(trans_str);


//printk("trans_str end =%s %d  \n" , trans_str  ,  trans_len );	


	printk("modify_ports trans_buf =%s  len=%d  matchoff=%x  matchlen=%x\n ",trans_str, trans_len, matchoff, matchlen);

	if (!nf_nat_mangle_tcp_packet(skb, ct, ctinfo, matchoff, matchlen, trans_str, trans_len)) {
		if (net_ratelimit())
			printk("nf_nat_rtsp: nf_nat_mangle_tcp_packet error\n");
		return -1;
	}
	*delta = trans_len - matchlen;

	return 0;
}


/****************************************************************************/
static int modify_ports_buf(struct sk_buff *skb, struct nf_conn *ct,
			enum ip_conntrack_info ctinfo,
			int matchoff, int matchlen,
			u_int16_t loport, u_int16_t hipport,
			char dash, int *delta)
{

	if (dash) {
		portRang_len = sprintf(portRang_tmp, "%hu%c%hu", loport, dash, hipport);
	}else {
		port_len = sprintf(port_tmp, "%hu", loport);
	}

	return 0;
}

static void clean_ipport_buf(void)
{
	memset(portRang_tmp, 0x00, sizeof(portRang_tmp));
	memset(port_tmp, 0x00, sizeof(port_tmp));
	memset(ipaddr_tmp, 0x00, sizeof(ipaddr_tmp));

	ipaddr_len = 0;
	portRang_len = 0;
	port_len = 0;
	
	return ;
}
#endif

#ifndef FIX_ACK_ERROR_ISSUE
/****************************************************************************/
static int modify_ports(struct sk_buff *skb, struct nf_conn *ct,
			enum ip_conntrack_info ctinfo,
			int matchoff, int matchlen,
			u_int16_t rtpport, u_int16_t rtcpport,
			char dash, int *delta)
{
	char buf[sizeof("65535-65535")];
	int len;

	if (dash)
		len = sprintf(buf, "%hu%c%hu", rtpport, dash, rtcpport);
	else
		len = sprintf(buf, "%hu", rtpport);


	//printk("modify_ports matchlen =%d  len=%d \n ",matchlen, len );

	if (!nf_nat_mangle_tcp_packet(skb, ct, ctinfo, matchoff, matchlen,
				      buf, len)) {
		if (net_ratelimit())
			printk("nf_nat_rtsp: nf_nat_mangle_tcp_packet error\n");
		return -1;
	}
	*delta = len - matchlen;
	return 0;
}

#endif
/****************************************************************************/
/* One data channel */
static int nat_rtsp_channel (struct sk_buff *skb, struct nf_conn *ct,
			     enum ip_conntrack_info ctinfo,
			     unsigned int matchoff, unsigned int matchlen,
			     struct nf_conntrack_expect *rtp_exp, int *delta)
{
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_conntrack_expect *exp;
	int dir = CTINFO2DIR(ctinfo);
	u_int16_t nated_port = 0;
	struct hlist_node *n;
	int exp_exist = 0;

	/* Set expectations for NAT */
	rtp_exp->saved_proto.udp.port = rtp_exp->tuple.dst.u.udp.port;
	rtp_exp->expectfn = nf_nat_follow_master;
	rtp_exp->dir = !dir;

	/* Lookup existing expects */
	spin_lock_bh(&nf_conntrack_lock);
	hlist_for_each_entry(exp, n, &help->expectations, lnode) {
		if (exp->saved_proto.udp.port == rtp_exp->saved_proto.udp.port){
			/* Expectation already exists */ 
			rtp_exp->tuple.dst.u.udp.port = 
				exp->tuple.dst.u.udp.port;
			nated_port = ntohs(exp->tuple.dst.u.udp.port);
			exp_exist = 1;
			break;
		}
	}
	spin_unlock_bh(&nf_conntrack_lock);

	if (exp_exist) {
		nf_ct_expect_related(rtp_exp);
		goto modify_message;
	}

	/* Try to get a port. */
	for (nated_port = ntohs(rtp_exp->tuple.dst.u.udp.port);
	     nated_port != 0; nated_port++) {
		rtp_exp->tuple.dst.u.udp.port = htons(nated_port);
		if (nf_ct_expect_related(rtp_exp) == 0)
			break;
	}

	if (nated_port == 0) {	/* No port available */
		if (net_ratelimit())
			printk("nf_nat_rtsp: out of UDP ports\n");
		return 0;
	}

modify_message:
	/* Modify message */
#if FIX_ACK_ERROR_ISSUE
		modify_ports_buf(skb, ct, ctinfo, matchoff, matchlen,
					 nated_port, 0, 0, delta);
#else
	if (modify_ports(skb, ct, ctinfo, matchoff, matchlen,
			 nated_port, 0, 0, delta) < 0) {
		nf_ct_unexpect_related(rtp_exp);
		return -1;
	}
#endif

	/* Success */
	pr_debug("nf_nat_rtsp: expect RTP ");
	nf_ct_dump_tuple(&rtp_exp->tuple);

	return 0;
}

/****************************************************************************/
/* A pair of data channels (RTP/RTCP) */
static int nat_rtsp_channel2 (struct sk_buff *skb, struct nf_conn *ct,
			      enum ip_conntrack_info ctinfo,
			      unsigned int matchoff, unsigned int matchlen,
			      struct nf_conntrack_expect *rtp_exp,
			      struct nf_conntrack_expect *rtcp_exp,
			      char dash, int *delta)
{
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_conntrack_expect *exp;
	int dir = CTINFO2DIR(ctinfo);
	u_int16_t nated_port = 0;
	struct hlist_node *n;
	int exp_exist = 0;

	/* Set expectations for NAT */
	rtp_exp->saved_proto.udp.port = rtp_exp->tuple.dst.u.udp.port;
	rtp_exp->expectfn = nf_nat_follow_master;
	rtp_exp->dir = !dir;
	rtcp_exp->saved_proto.udp.port = rtcp_exp->tuple.dst.u.udp.port;
	rtcp_exp->expectfn = nf_nat_follow_master;
	rtcp_exp->dir = !dir;

	/* Lookup existing expects */
	spin_lock_bh(&nf_conntrack_lock);
	hlist_for_each_entry(exp, n, &help->expectations, lnode) {
		if (exp->saved_proto.udp.port == rtp_exp->saved_proto.udp.port){
			/* Expectation already exists */ 
			rtp_exp->tuple.dst.u.udp.port = 
				exp->tuple.dst.u.udp.port;
			rtcp_exp->tuple.dst.u.udp.port = 
				htons(ntohs(exp->tuple.dst.u.udp.port) + 1);
			nated_port = ntohs(exp->tuple.dst.u.udp.port);
			exp_exist = 1;
			break;
		}
	}
	spin_unlock_bh(&nf_conntrack_lock);

	if (exp_exist) {
		nf_ct_expect_related(rtp_exp);
		nf_ct_expect_related(rtcp_exp);
		goto modify_message;
	}

	/* Try to get a pair of ports. */
	for (nated_port = ntohs(rtp_exp->tuple.dst.u.udp.port) & (~1);
	     nated_port != 0; nated_port += 2) {
		rtp_exp->tuple.dst.u.udp.port = htons(nated_port);
		if (nf_ct_expect_related(rtp_exp) == 0) {
			rtcp_exp->tuple.dst.u.udp.port =
			    htons(nated_port + 1);
			if (nf_ct_expect_related(rtcp_exp) == 0)
				break;
			nf_ct_unexpect_related(rtp_exp);
		}
	}

	if (nated_port == 0) {	/* No port available */
		if (net_ratelimit())
			printk("nf_nat_rtsp: out of RTP/RTCP ports\n");
		return 0;
	}

modify_message:
	/* Modify message */
#if FIX_ACK_ERROR_ISSUE
	modify_ports_buf(skb, ct, ctinfo, matchoff, matchlen,		 
			 nated_port, nated_port + 1, dash, delta);
#else
	if (modify_ports(skb, ct, ctinfo, matchoff, matchlen,
			 nated_port, nated_port + 1, dash, delta) < 0) {
		nf_ct_unexpect_related(rtp_exp);
		nf_ct_unexpect_related(rtcp_exp);
		return -1;
	}
#endif

	/* Success */
	pr_debug("nf_nat_rtsp: expect RTP ");
	nf_ct_dump_tuple(&rtp_exp->tuple);
	pr_debug("nf_nat_rtsp: expect RTCP ");
	nf_ct_dump_tuple(&rtcp_exp->tuple);

	return 0;
}

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
/****************************************************************************/
/* A rang of data channels (RTP/RTCP/retrasm) */
static int nat_rtsp_channel3 (struct sk_buff *skb, struct nf_conn *ct,
			      enum ip_conntrack_info ctinfo,
			      unsigned int matchoff, unsigned int matchlen,
			      struct nf_conntrack_expect *rtp_exp,
			      struct nf_conntrack_expect *rtcp_exp,
				  struct nf_conntrack_expect *rtcp2_exp,
				  struct nf_conntrack_expect *rertp_exp,
				  struct nf_conntrack_expect *rertp2_exp,
			      char dash, int *delta)
{
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_conntrack_expect *exp;
	int dir = CTINFO2DIR(ctinfo);
	u_int16_t nated_port = 0;
	struct hlist_node *n;
	int exp_exist = 0;

	/* Set expectations for NAT */
	rtp_exp->expectfn = nf_nat_follow_master;
	rtp_exp->dir = !dir;
	rtp_exp->master = ct;


//	printk("[rtp] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u	save_port=[%d] master=[%p] dir=[%d]\n",
//		NIPQUAD(rtp_exp->tuple.src.u3.ip),ntohs(rtp_exp->tuple.src.u.udp.port),NIPQUAD(rtp_exp->tuple.dst.u3.ip),ntohs(rtp_exp->tuple.dst.u.udp.port),
//		rtp_exp->saved_proto.udp.port,rtp_exp->master, rtp_exp->dir); 


	rtcp_exp->expectfn = rtsp_ext_expected;
	rtcp_exp->dir = !dir;
	rtcp_exp->master = ct;

//	printk("[rtcp] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u	save_port=[%d] master=[%p] dir=[%d]\n",
//		NIPQUAD(rtcp_exp->tuple.src.u3.ip),ntohs(rtcp_exp->tuple.src.u.udp.port),NIPQUAD(rtcp_exp->tuple.dst.u3.ip),ntohs(rtcp_exp->tuple.dst.u.udp.port),
//		rtcp_exp->saved_proto.udp.port,rtcp_exp->master, rtcp_exp->dir); 

	

	rtcp2_exp->expectfn = rtsp_ext_expected_client;
	rtcp2_exp->dir = dir;
	rtcp2_exp->master = ct;

//	printk("[rtcp2] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u	save_port=[%d] master=[%p] dir=[%d]\n",
//		NIPQUAD(rtcp2_exp->tuple.src.u3.ip),ntohs(rtcp2_exp->tuple.src.u.udp.port),NIPQUAD(rtcp2_exp->tuple.dst.u3.ip),ntohs(rtcp2_exp->tuple.dst.u.udp.port),
//		rtcp2_exp->saved_proto.udp.port,rtcp2_exp->master, rtcp2_exp->dir); 	

	rertp_exp->expectfn = rtsp_ext_expected;
	rertp_exp->dir = !dir;
	rertp_exp->master = ct;

//	printk("[rertp] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u	save_port=[%d] master=[%p] dir=[%d]\n",
//		NIPQUAD(rertp_exp->tuple.src.u3.ip),ntohs(rertp_exp->tuple.src.u.udp.port),NIPQUAD(rertp_exp->tuple.dst.u3.ip),ntohs(rertp_exp->tuple.dst.u.udp.port),
//		rertp_exp->saved_proto.udp.port,rertp_exp->master, rertp_exp->dir); 


	rertp2_exp->expectfn = rtsp_ext_expected_client;
	rertp2_exp->dir = dir;
	rertp2_exp->master = ct;

//	printk("[rertp2] %u.%u.%u.%u:%u-%u.%u.%u.%u:%u	save_port=[%d] master=[%p] dir=[%d]\n",
//	NIPQUAD(rertp2_exp->tuple.src.u3.ip),ntohs(rertp2_exp->tuple.src.u.udp.port),NIPQUAD(rertp2_exp->tuple.dst.u3.ip),ntohs(rertp2_exp->tuple.dst.u.udp.port),
//	rertp2_exp->saved_proto.udp.port,rertp2_exp->master, rertp2_exp->dir); 	


	/* Lookup existing expects */
	spin_lock_bh(&nf_conntrack_lock);
	hlist_for_each_entry(exp, n, &help->expectations, lnode) {
		if (exp->saved_proto.udp.port == rtp_exp->saved_proto.udp.port){
			/* Expectation already exists */ 			
			
			rtp_exp->tuple.dst.u.udp.port = exp->tuple.dst.u.udp.port;
			rtcp_exp->tuple.dst.u.udp.port = htons(ntohs(exp->tuple.dst.u.udp.port) + 1);
			rertp_exp->tuple.dst.u.udp.port = htons(ntohs(exp->tuple.dst.u.udp.port) + 2);

	//printk("Lookup existing expects =%d %d \n", rtp_exp->tuple.dst.u.udp.port, exp->tuple.dst.u.udp.port );

			nated_port = ntohs(exp->tuple.dst.u.udp.port);
			exp_exist = 1;
			break;
		}
	}
	spin_unlock_bh(&nf_conntrack_lock);


//printk(" Lookup existing expects end \n");

	if (exp_exist) {
		nf_ct_expect_related(rtp_exp);
		nf_ct_expect_related(rtcp_exp);
		nf_ct_expect_related(rtcp2_exp);
		nf_ct_expect_related(rertp_exp);
		nf_ct_expect_related(rertp2_exp);
		goto modify_message;
	}


	//printk(" Try to get a rang of ports \n");

	/* Try to get a rang of ports. */
	for (nated_port = ntohs(rtp_exp->tuple.dst.u.udp.port) & (~1);
	     nated_port != 0 ; nated_port += 4) {     //must change 2->4 

		if ( nated_port > 7000 || nated_port < 6970 ) {
			break;
		}
		
		rtp_exp->tuple.dst.u.all = htons(nated_port);
		if (nf_ct_expect_related(rtp_exp) == 0) {
			rtcp_exp->tuple.dst.u.all = htons(nated_port+1);
			if (nf_ct_expect_related(rtcp_exp) == 0) {
				if (nf_ct_expect_related(rtcp2_exp) == 0) {
					rertp_exp->tuple.dst.u.all = htons(nated_port+2);
					if (nf_ct_expect_related(rertp_exp) == 0) {							
						if (nf_ct_expect_related(rertp2_exp) == 0) {
		//					printk(" get a rang of ports \n");
							break;
						}	
						nf_ct_unexpect_related(rertp_exp);
					}	
					nf_ct_unexpect_related(rtcp2_exp);
				}	
				nf_ct_unexpect_related(rtcp_exp);
			}	
			nf_ct_unexpect_related(rtp_exp);
		}
	}

	if (nated_port == 0) {	/* No port available */
		if (net_ratelimit())
			printk("nf_nat_rtsp: out of RTP/RTCP ports\n");
		return 0;
	}	

modify_message:	
#if FIX_ACK_ERROR_ISSUE
	modify_ports_buf(skb, ct, ctinfo, matchoff, matchlen,		 
			 nated_port, nated_port + 2, dash, delta);
#else
	/* Modify message */
	if (modify_ports(skb, ct, ctinfo, matchoff, matchlen,        
			 nated_port, nated_port + 2, dash, delta) < 0) {    //must change 1->2
		nf_ct_unexpect_related(rtp_exp);
		nf_ct_unexpect_related(rtcp_exp);
		nf_ct_unexpect_related(rtcp2_exp);
		nf_ct_unexpect_related(rertp_exp);
		nf_ct_unexpect_related(rertp2_exp);
		return -1;
	}
#endif

	/* Success */
	pr_debug("nf_nat_rtsp: expect RTP ");
	nf_ct_dump_tuple(&rtp_exp->tuple);
	pr_debug("nf_nat_rtsp: expect RTCP ");
	nf_ct_dump_tuple(&rtcp_exp->tuple);
	pr_debug("nf_nat_rtsp2: expect RTCP2 ");
	nf_ct_dump_tuple(&rtcp2_exp->tuple);
	pr_debug("nf_nat_rertp: expect RERTP ");
	nf_ct_dump_tuple(&rertp_exp->tuple);
	pr_debug("nf_nat_rertp2: expect RERTP2 ");
	nf_ct_dump_tuple(&rertp2_exp->tuple);

	return 0;
}
#endif

/****************************************************************************/
static __be16 lookup_mapping_port(struct nf_conn *ct,
				  enum ip_conntrack_info ctinfo,
				  __be16 port)
{
	enum ip_conntrack_dir dir = CTINFO2DIR(ctinfo);
	struct nf_conn_help *help = nfct_help(ct);
	struct nf_conntrack_expect *exp;
	struct nf_conn *child;
	struct hlist_node *n;

	/* Lookup existing expects */
	pr_debug("nf_nat_rtsp: looking up existing expectations...\n");
	hlist_for_each_entry(exp, n, &help->expectations, lnode) {
		if (exp->tuple.dst.u.udp.port == port) {
			pr_debug("nf_nat_rtsp: found port %hu mapped from "
				 "%hu\n",
			       	 ntohs(exp->tuple.dst.u.udp.port),
			       	 ntohs(exp->saved_proto.all));
			return exp->saved_proto.all;
		}
	}

	/* Lookup existing connections */
	pr_debug("nf_nat_rtsp: looking up existing connections...\n");
	list_for_each_entry(child, &ct->derived_connections, derived_list) {
		if (child->tuplehash[dir].tuple.dst.u.udp.port == port) {
			pr_debug("nf_nat_rtsp: found port %hu mapped from "
				 "%hu\n",
			       	 ntohs(child->tuplehash[dir].
			       	 tuple.dst.u.udp.port),
			       	 ntohs(child->tuplehash[!dir].
			       	 tuple.src.u.udp.port));
			return child->tuplehash[!dir].tuple.src.u.udp.port;
		}
	}

	return htons(0);
}

/****************************************************************************/
static int nat_rtsp_modify_port (struct sk_buff *skb, struct nf_conn *ct,
			      	 enum ip_conntrack_info ctinfo,
				 unsigned int matchoff, unsigned int matchlen,
			      	 __be16 rtpport, int *delta)
{
	__be16 orig_port;

	orig_port = lookup_mapping_port(ct, ctinfo, rtpport);
	if (orig_port == htons(0)) {
		*delta = 0;
		return 0;
	}

	//printk("nat_rtsp_modify_port=%d \n", orig_port );

#if FIX_ACK_ERROR_ISSUE
	modify_ports_buf(skb, ct, ctinfo, matchoff, matchlen,
		 ntohs(orig_port), 0, 0, delta);
#else
	if (modify_ports(skb, ct, ctinfo, matchoff, matchlen,
			 ntohs(orig_port), 0, 0, delta) < 0)
		return -1;
#endif
	pr_debug("nf_nat_rtsp: Modified client_port from %hu to %hu\n",
	       	 ntohs(rtpport), ntohs(orig_port));
	return 0;
}

/****************************************************************************/
static int nat_rtsp_modify_port2 (struct sk_buff *skb, struct nf_conn *ct,
			       	  enum ip_conntrack_info ctinfo,
				  unsigned int matchoff, unsigned int matchlen,
			       	  __be16 rtpport, __be16 rtcpport,
				  char dash, int *delta)
{
	__be16 orig_port;

	orig_port = lookup_mapping_port(ct, ctinfo, rtpport);
	if (orig_port == htons(0)) {
		*delta = 0;
		return 0;
	}

#if FIX_ACK_ERROR_ISSUE
	modify_ports_buf(skb, ct, ctinfo, matchoff, matchlen,
				 ntohs(orig_port), ntohs(orig_port)+1, dash, delta);
#else
	if (modify_ports(skb, ct, ctinfo, matchoff, matchlen,
			 ntohs(orig_port), ntohs(orig_port)+1, dash, delta) < 0)
		return -1;
#endif
	
	pr_debug("nf_nat_rtsp: Modified client_port from %hu to %hu\n",
	       	 ntohs(rtpport), ntohs(orig_port));
	return 0;
}

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
/****************************************************************************/
static int nat_rtsp_modify_port3(struct sk_buff *skb, struct nf_conn *ct,
			       	  enum ip_conntrack_info ctinfo,
				  unsigned int matchoff, unsigned int matchlen,
			       	  __be16 rtpport, __be16 rtcpport,
				  char dash, int *delta)
{

	__be16 orig_port;

	orig_port = lookup_mapping_port(ct, ctinfo, rtpport);
	if (orig_port == htons(0)) {
		*delta = 0;
		return 0;
	}

	if (modify_ports_buf(skb, ct, ctinfo, matchoff, matchlen,
			 ntohs(orig_port), ntohs(orig_port)+2, dash, delta) < 0)
		return -1;

	return 0;
}
#endif


/****************************************************************************/
static int nat_rtsp_modify_addr(struct sk_buff *skb, struct nf_conn *ct,
				enum ip_conntrack_info ctinfo,
				int matchoff, int matchlen, int *delta)
{
	char buf[sizeof("255.255.255.255")];
	int dir = CTINFO2DIR(ctinfo);
	int len;

	//printk(" nat_rtsp_modify_addr  \n");
	/* Change the destination address to FW's WAN IP address */
#if FIX_ACK_ERROR_ISSUE
	if (memcmp(&ct->tuplehash[dir].tuple.src.u3,
		   &ct->tuplehash[!dir].tuple.dst.u3,
		   sizeof(ct->tuplehash[dir].tuple.src.u3))
		!= 0) {
		/* LAN to WAN */
		ipaddr_len = sprintf(ipaddr_tmp, "%u.%u.%u.%u",
				  NIPQUAD(ct->tuplehash[!dir].tuple.dst.u3.ip));

		printk("buf =%s  \n", ipaddr_tmp );
		return 0;
	}else{
		/* WAN to LAN */
		ipaddr_len = sprintf(ipaddr_tmp, "%u.%u.%u.%u",
			  NIPQUAD(ct->tuplehash[!dir].tuple.src.u3.ip));
		printk("buf =%s  \n", ipaddr_tmp );
		return 0;
	}
#else
	len = sprintf(buf, "%u.%u.%u.%u",
			  NIPQUAD(ct->tuplehash[!dir].tuple.dst.u3.ip));
#endif

	if (!nf_nat_mangle_tcp_packet(skb, ct, ctinfo, matchoff, matchlen,
				      buf, len)) {
		if (net_ratelimit())
			printk("nf_nat_rtsp: nf_nat_mangle_tcp_packet error\n");
		return -1;
	}
	*delta = len - matchlen;
	return 0;
}

/****************************************************************************/
static int __init init(void)
{
	BUG_ON(rcu_dereference(nat_rtsp_channel_hook) != NULL);
	BUG_ON(rcu_dereference(nat_rtsp_channel2_hook) != NULL);
	BUG_ON(rcu_dereference(nat_rtsp_modify_port_hook) != NULL);
	BUG_ON(rcu_dereference(nat_rtsp_modify_port2_hook) != NULL);
	BUG_ON(rcu_dereference(nat_rtsp_modify_addr_hook) != NULL);
	rcu_assign_pointer(nat_rtsp_channel_hook, nat_rtsp_channel);
	rcu_assign_pointer(nat_rtsp_channel2_hook, nat_rtsp_channel2);
	rcu_assign_pointer(nat_rtsp_modify_port_hook, nat_rtsp_modify_port);
	rcu_assign_pointer(nat_rtsp_modify_port2_hook, nat_rtsp_modify_port2);
	rcu_assign_pointer(nat_rtsp_modify_addr_hook, nat_rtsp_modify_addr);

#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
	BUG_ON(rcu_dereference(nat_rtsp_channel3_hook) != NULL);
	BUG_ON(rcu_dereference(nat_rtsp_modify_port3_hook) != NULL);
	rcu_assign_pointer(nat_rtsp_channel3_hook, nat_rtsp_channel3);	
	rcu_assign_pointer(nat_rtsp_modify_port3_hook, nat_rtsp_modify_port3);
#endif	
#if FIX_ACK_ERROR_ISSUE
	BUG_ON(rcu_dereference(modify_setup_mesg_hook) != NULL);
	BUG_ON(rcu_dereference(modify_reply_mesg_hook) != NULL);
	BUG_ON(rcu_dereference(clean_ipport_buf_hook) != NULL);
	rcu_assign_pointer(modify_setup_mesg_hook, modify_setup_mesg);
	rcu_assign_pointer(modify_reply_mesg_hook, modify_reply_mesg);
	rcu_assign_pointer(clean_ipport_buf_hook, clean_ipport_buf);	
#endif
	pr_debug("nf_nat_rtsp: init success\n");
	return 0;
}

/****************************************************************************/
static void __exit fini(void)
{
	rcu_assign_pointer(nat_rtsp_channel_hook, NULL);
	rcu_assign_pointer(nat_rtsp_channel2_hook, NULL);
	rcu_assign_pointer(nat_rtsp_modify_port_hook, NULL);
	rcu_assign_pointer(nat_rtsp_modify_port2_hook, NULL);
	rcu_assign_pointer(nat_rtsp_modify_addr_hook, NULL);
#ifdef CONFIG_MSTC_TELENOR_NORWAY_PORT_CHANGE
	rcu_assign_pointer(nat_rtsp_channel3_hook, NULL);
	rcu_assign_pointer(nat_rtsp_modify_port3_hook, NULL);
#endif	
#if FIX_ACK_ERROR_ISSUE
	rcu_assign_pointer(modify_setup_mesg_hook, NULL);
	rcu_assign_pointer(modify_reply_mesg_hook, NULL);
	rcu_assign_pointer(clean_ipport_buf_hook, NULL);
#endif	

	synchronize_rcu();
}

/****************************************************************************/
module_init(init);
module_exit(fini);

MODULE_AUTHOR("Broadcom Corporation");
MODULE_DESCRIPTION("RTSP NAT helper");
MODULE_LICENSE("GPL");
MODULE_ALIAS("ip_nat_rtsp");
