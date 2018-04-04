/*
 *  ebt_ip
 *
 *	Authors:
 *	Bart De Schuymer <bdschuym@pandora.be>
 *
 *  April, 2002
 *
 *  Changes:
 *    added ip-sport and ip-dport
 *    Innominate Security Technologies AG <mhopf@innominate.com>
 *    September, 2002
 */
#include <linux/ip.h>
#include <net/ip.h>
#include <linux/in.h>
#include <linux/module.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_bridge/ebtables.h>
#include <linux/netfilter_bridge/ebt_ip.h>
#if 1 /* ZyXEL QoS, John (porting from MSTC) */
#include <linux/tcp.h>
#include <linux/udp.h>
#endif
struct tcpudphdr {
	__be16 src;
	__be16 dst;
};

#if 1 /* ZyXEL QoS, John (porting from MSTC) */

unsigned char *get_DhcpOption(struct dhcpMessage *packet, int code)
{
	int i, length;
	unsigned char *optionptr;
	int over = 0, done = 0, curr = OPTION_FIELD;

	optionptr = packet->options;
	i = 0;
	length = 308;

	while (!done) {
		if (i >= length){
			return NULL;
		}
		if (optionptr[i + OPT_CODE] == code) {
			if (i + 1 + optionptr[i + OPT_LEN] >= length) {
				return NULL;
			}
			return optionptr + i + 2;
		}
		switch (optionptr[i + OPT_CODE]) {
			case DHCP_PADDING:
				i++;
				break;
			case DHCP_OPTION_OVER:
				if (i + 1 + optionptr[i + OPT_LEN] >= length) {
					return NULL;
				}
				over = optionptr[i + 3];
				i += optionptr[OPT_LEN] + 2;
				break;
			case DHCP_END:
				if (curr == OPTION_FIELD && over & FILE_FIELD) {
					optionptr = packet->file;
					i = 0;
					length = 128;
					curr = FILE_FIELD;
				} else if (curr == FILE_FIELD && over & SNAME_FIELD) {
					optionptr = packet->sname;
					i = 0;
					length = 64;
					curr = SNAME_FIELD;
				} else done = 1;
				break;
			default:
				i += optionptr[OPT_LEN + i] + 2;
		}
	}
	return NULL;
}

/* If match, return value is 0 */
static int cmp_option60(char *optval60, const struct ebt_ip_info *info)
{
	int len;
	char optionData[254];
	const struct cfgopt *cfgptr = NULL;
	printk("%s %d\n", __FUNCTION__, __LINE__);

	if (optval60 == NULL)
		return false;

	cfgptr = &(info->cfg60);

	/* Compare option data length */
	len = (int)(*((unsigned char *)optval60 - 1));
	if(len != cfgptr->len)
		return false;

	/* Compare option data content */
	memset(optionData, 0, 254);
	strncpy(optionData, optval60, len);
	printk("%s %d: cfgdata[%s] optionData[%s]\n", __FUNCTION__, __LINE__, cfgptr->cfgdata, optionData);
	if(strcmp(cfgptr->cfgdata, optionData) == 0)
		return true;

	return 1;
}

/* If match, return value is 0 */
static int cmp_option61(char *optval61, const struct ebt_ip_info *info)
{
        int len;
        char optionData[254];
        const struct cfgopt *cfgptr = NULL;

		

        if (optval61 == NULL)
                return false;

        cfgptr = &(info->cfg61);

        /* Compare option data length */
        len = (int)(*((unsigned char *)optval61 - 1));

        if(len != cfgptr->len)
                return false;

        /* Compare option data content */
        memset(optionData, 0, 254);
        memcpy(optionData, optval61, len);
		
        if(!memcmp(cfgptr->cfgdata, optionData, len))
                return true;

        return 1;
}

/* If match, return value is 0 */
static int cmp_option77(char *optval77, const struct ebt_ip_info *info)
{
        uint8_t len = 0, total_len = 0, current_len = 0;
        char optionData[254];
        const struct cfgopt *cfgptr = NULL;


        if (optval77 == NULL)
                return false;

        cfgptr = &(info->cfg77);

        /* Record option 77 total length */
        total_len = (uint8_t)(*((unsigned char *)optval77 - 1));

        while(total_len != current_len){
                len = (uint8_t)*((unsigned char *)optval77);    /* For option 77, one data length */

                if(len != cfgptr->len)
                        return false;

                /* Compare option data content */
                memset(optionData, 0, 254);
                memcpy(optionData, optval77 + DHCP_OPT_LEN_FIELD_LEN, len );
                if(!memcmp(cfgptr->cfgdata, optionData, len))
                        return true;

                /* shift to next vendor class data in option 125 */
                current_len += (len + DHCP_OPT_LEN_FIELD_LEN);
                optval77 += (len + DHCP_OPT_LEN_FIELD_LEN);
        }
        return false;
}


/* If match, return value is 0 */
static int cmp_option125(char *optval125, const struct ebt_ip_info *info)
{
        int len = 0,  total_len = 0, current_len = 0;
        char optionData[254];
        const struct cfgopt *cfgptr = NULL;

        if (optval125 == NULL)
                return false;

        cfgptr = &(info->cfg125);

        /* Record option 125 total length */
        total_len = (int)(*((unsigned char *)optval125 - 1));

        while(total_len != current_len){

                len = (int)(*((unsigned char *)optval125 +DHCP_OPT125_ENTERPRISE_NUM_LEN));     /* For option 125, one data length */


                if(len + DHCP_OPT125_DATA_SHIFT != cfgptr->len) /* Add 5 is for enterprise-number(4 bytes) and data length represent(1 byte) */
                        return false;

                /* Compare option data content */
                memset(optionData, 0, 254);
                memcpy(optionData, optval125, len + DHCP_OPT125_DATA_SHIFT);

                if(!memcmp(cfgptr->cfgdata, optionData, len + DHCP_OPT125_DATA_SHIFT))
                        return true;

                /* shift to next vendor class data in option 125 */
                current_len += (len + DHCP_OPT125_DATA_SHIFT);
                optval125 += (len + DHCP_OPT125_DATA_SHIFT);
        }
        return false;

}





#endif
static bool
ebt_ip_mt(const struct sk_buff *skb, const struct xt_match_param *par)
{
	const struct ebt_ip_info *info = par->matchinfo;
	const struct iphdr *ih;
	struct iphdr _iph;
	const struct tcpudphdr *pptr;
	struct tcpudphdr _ports;
#if 1 /* ZyXEL QoS, John (porting from MSTC) */
	struct tcphdr _tcph, *th;
#endif

	ih = skb_header_pointer(skb, 0, sizeof(_iph), &_iph);
	if (ih == NULL)
		return false;
#if 1 /* ZyXEL QoS, John (porting from MSTC) */
	if((info->bitmask & EBT_IP_DHCP_OPT60)||(info->bitmask & EBT_IP_DHCP_OPT61)||
       (info->bitmask & EBT_IP_DHCP_OPT77)||(info->bitmask & EBT_IP_DHCP_OPT125)){
		 unsigned char payload[DHCP_OPTION_MAX_LEN];
        struct dhcpMessage *dhcpPtr;
        struct iphdr _iph, *ih;
        struct tcpudphdr _ports, *pptr;
        int     skb_data_len=0, i, LastEntry = -1;
        bool OptMatch = 0;
        char TmpMac[ETH_ALEN];

        memset(TmpMac , 0, sizeof(TmpMac));

	ih = skb_header_pointer(skb, 0, sizeof(_iph), &_iph);
	if (ih == NULL)
		return false;

        pptr = skb_header_pointer(skb, ih->ihl*4,  sizeof(_ports), &_ports);

        /* not DHCP packet, then try to compared with recoded mac */
        if(pptr->src!=67 && pptr->src!=68 && pptr->dst!=67 && pptr->dst!=68){
			for(i=0;i<OPTION_MAC_ENTRY;i++){
		        if(!compare_ether_addr(eth_hdr(skb)->h_source, info->SrcMacArray[i])){
					/* if match, continue to check other conditions */
		        	goto CONTINUE;
		        }
		        if(!memcmp(TmpMac,info->SrcMacArray[i],ETH_ALEN)){
		        	break;
		        }
			}
            return false;
        }else{ 
            /* If packet is dhcp packet */
            memset(payload, 0, sizeof(payload));
            skb_data_len = skb->len;

            if (skb_copy_bits(skb, 0, payload, skb_data_len))
                            printk("Copy packet is failed by ebtables of filtering DHCP Option\n\r");

            dhcpPtr = (struct dhcpMessage *)(payload + sizeof(struct iphdr) + sizeof(struct udphdr));

            if(info->bitmask & EBT_IP_DHCP_OPT60){
                char *opt60 = get_DhcpOption(dhcpPtr, DHCP_VENDOR);
                if(cmp_option60(opt60, info)^ !!(info->invflags & EBT_IP_DHCP_OPT60))
                	OptMatch = 1;
            }

            if(info->bitmask & EBT_IP_DHCP_OPT61){
                char *opt61 = get_DhcpOption(dhcpPtr, DHCP_CLIENT_ID);
                if(cmp_option61(opt61, info)^ !!(info->invflags & EBT_IP_DHCP_OPT61))
                	OptMatch = 1;
            }

            if(info->bitmask & EBT_IP_DHCP_OPT77){
                char *opt77 = get_DhcpOption(dhcpPtr, DHCP_USER_CLASS_ID);
                if(cmp_option77(opt77, info)^ !!(info->invflags & EBT_IP_DHCP_OPT77))
                	OptMatch = 1;
            }

            if(info->bitmask & EBT_IP_DHCP_OPT125){
                char *opt125 = get_DhcpOption(dhcpPtr, DHCP_VENDOR_IDENTIFYING);
                if(cmp_option125(opt125, info)^ !!(info->invflags & EBT_IP_DHCP_OPT125))
                	OptMatch = 1;
            }

//                              printk("\nOptMatch is %d\n",OptMatch);

            if(OptMatch){ 
				/* match dhcp option, then record its MAC addr for future filter */
				for(i=0;i<OPTION_MAC_ENTRY;i++){
					if(!memcmp(TmpMac,info->SrcMacArray[i],ETH_ALEN)){
				        memcpy((void *)(info->SrcMacArray[i]), eth_hdr(skb)->h_source, ETH_ALEN);
				        break;
					}else if(!memcmp(eth_hdr(skb)->h_source, info->SrcMacArray[i], ETH_ALEN)){
					    break;
					}
				}
            }else{
            	/* not match, need to check current list whether this MAC hace once been matched, if so, clear this entry from this DHCP 
            				  option criteria (client might change another vendor information, ex different vendor ID string) */
                for(i=0;i<OPTION_MAC_ENTRY;i++){
                    if(!memcmp(TmpMac,info->SrcMacArray[i],ETH_ALEN)){
                        if(LastEntry!=-1){
                            memcpy((void *)(info->SrcMacArray[LastEntry]),  info->SrcMacArray[i-1], ETH_ALEN);
                            memset((void *)(info->SrcMacArray[i-1]), 0, ETH_ALEN);
                        }
                        break;
                    }else if(!memcmp(eth_hdr(skb)->h_source, info->SrcMacArray[i], ETH_ALEN)){
                        LastEntry = i;//Record clear entry
                    }
                }
				
                return false;
            }
		}
		
	}
CONTINUE:
#endif
	if (info->bitmask & EBT_IP_TOS &&
	   FWINV(info->tos != ih->tos, EBT_IP_TOS))
		return false;
   /* brcm */
	if (info->bitmask & EBT_IP_DSCP &&
	   FWINV(info->dscp != (ih->tos & 0xFC), EBT_IP_DSCP))
		return false;
	if (info->bitmask & EBT_IP_SOURCE &&
	   FWINV((ih->saddr & info->smsk) !=
	   info->saddr, EBT_IP_SOURCE))
		return false;
	if ((info->bitmask & EBT_IP_DEST) &&
	   FWINV((ih->daddr & info->dmsk) !=
	   info->daddr, EBT_IP_DEST))
		return false;
#if 1 /* ZyXEL QoS, John (porting from MSTC) */
	if(info->bitmask & EBT_IP_LENGTH) { /* IP Length */
		u16 len = ntohs(ih->tot_len);
			if (FWINV(len < info->length[0] ||
					  len > info->length[1],
					  EBT_IP_LENGTH))
		return false;
	}
#endif
	if (info->bitmask & EBT_IP_PROTO) {
		if (FWINV(info->protocol != ih->protocol, EBT_IP_PROTO)){
			return false;
		}
#if 0 /* ZyXEL QoS, John (porting from MSTC) */
		if (!(info->bitmask & EBT_IP_DPORT) &&
		    !(info->bitmask & EBT_IP_SPORT))
#else
		if (!(info->bitmask & EBT_IP_DPORT) &&
		    !(info->bitmask & EBT_IP_SPORT) &&
		    !(info->bitmask & EBT_IP_TCP_FLAGS))
#endif 
			return true;

		if (ntohs(ih->frag_off) & IP_OFFSET)
			return false;
		pptr = skb_header_pointer(skb, ih->ihl*4,
					  sizeof(_ports), &_ports);
		if (pptr == NULL)
			return false;
		if (info->bitmask & EBT_IP_DPORT) {
			u32 dst = ntohs(pptr->dst);
			if (FWINV(dst < info->dport[0] ||
				  dst > info->dport[1],
				  EBT_IP_DPORT))
			return false;
		}
		if (info->bitmask & EBT_IP_SPORT) {
			u32 src = ntohs(pptr->src);
			if (FWINV(src < info->sport[0] ||
				  src > info->sport[1],
				  EBT_IP_SPORT))
			return false;
		}
#if 1 /* ZyXEL QoS, John (porting from MSTC) */
                if (info->bitmask & EBT_IP_TCP_FLAGS) {
			th = skb_header_pointer(skb, ih->ihl*4, sizeof(_tcph), &_tcph);
			if (th == NULL) {
				/* We've been asked to examine this packet, and we
				   can't.  Hence, no choice but to drop. */
				printk("Dropping evil TCP offset=0 tinygram.\n");
				return 0;
			}
			if (FWINV((((unsigned char *)th)[13] & info->tcp_flg_mask) != info->tcp_flg_cmp, EBT_IP_TCP_FLAGS))
				return false;
		}
#endif
	}

	return true;
}

static bool ebt_ip_mt_check(const struct xt_mtchk_param *par)
{
	const struct ebt_ip_info *info = par->matchinfo;
	const struct ebt_entry *e = par->entryinfo;

	if (e->ethproto != htons(ETH_P_IP) ||
	   e->invflags & EBT_IPROTO)
		return false;
	if (info->bitmask & ~EBT_IP_MASK || info->invflags & ~EBT_IP_MASK)
		return false;
	if (info->bitmask & (EBT_IP_DPORT | EBT_IP_SPORT)) {
		if (info->invflags & EBT_IP_PROTO)
			return false;
		if (info->protocol != IPPROTO_TCP &&
		    info->protocol != IPPROTO_UDP &&
		    info->protocol != IPPROTO_UDPLITE &&
		    info->protocol != IPPROTO_SCTP &&
		    info->protocol != IPPROTO_DCCP)
			 return false;
	}
	if (info->bitmask & EBT_IP_DPORT && info->dport[0] > info->dport[1])
		return false;
	if (info->bitmask & EBT_IP_SPORT && info->sport[0] > info->sport[1])
		return false;
#if 1 /* ZyXEL QoS, John (porting from MSTC) */
        if (info->bitmask & EBT_IP_LENGTH && info->length[0] > info->length[1])
		return false;
#endif
	return true;
}

static struct xt_match ebt_ip_mt_reg __read_mostly = {
	.name		= "ip",
	.revision	= 0,
	.family		= NFPROTO_BRIDGE,
	.match		= ebt_ip_mt,
	.checkentry	= ebt_ip_mt_check,
	.matchsize	= XT_ALIGN(sizeof(struct ebt_ip_info)),
	.me		= THIS_MODULE,
};

static int __init ebt_ip_init(void)
{
	return xt_register_match(&ebt_ip_mt_reg);
}

static void __exit ebt_ip_fini(void)
{
	xt_unregister_match(&ebt_ip_mt_reg);
}

module_init(ebt_ip_init);
module_exit(ebt_ip_fini);
MODULE_DESCRIPTION("Ebtables: IPv4 protocol packet match");
MODULE_LICENSE("GPL");
