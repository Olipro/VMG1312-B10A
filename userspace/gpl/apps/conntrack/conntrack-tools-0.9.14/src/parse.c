/*
 * (C) 2006-2007 by Pablo Neira Ayuso <pablo@netfilter.org>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "network.h"

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

#ifndef ssizeof
#define ssizeof(x) (int)sizeof(x)
#endif

static void parse_u8(struct nf_conntrack *ct, int attr, void *data);
static void parse_u16(struct nf_conntrack *ct, int attr, void *data);
static void parse_u32(struct nf_conntrack *ct, int attr, void *data);
static void parse_group(struct nf_conntrack *ct, int attr, void *data);
static void parse_nat_seq_adj(struct nf_conntrack *ct, int attr, void *data);

struct parser {
	void 	(*parse)(struct nf_conntrack *ct, int attr, void *data);
	int 	attr;
	int	size;
};

static struct parser h[NTA_MAX] = {
	[NTA_IPV4] = {
		.parse	= parse_group,
		.attr	= ATTR_GRP_ORIG_IPV4,
		.size	= NTA_SIZE(sizeof(struct nfct_attr_grp_ipv4)),
	},
	[NTA_IPV6] = {
		.parse	= parse_group,
		.attr	= ATTR_GRP_ORIG_IPV6,
		.size	= NTA_SIZE(sizeof(struct nfct_attr_grp_ipv6)),
	},
	[NTA_PORT] = {
		.parse	= parse_group,
		.attr	= ATTR_GRP_ORIG_PORT,
		.size	= NTA_SIZE(sizeof(struct nfct_attr_grp_port)),
	},
	[NTA_L4PROTO] = {
		.parse	= parse_u8,
		.attr	= ATTR_L4PROTO,
		.size	= NTA_SIZE(sizeof(uint8_t)),
	},
	[NTA_TCP_STATE] = {
		.parse	= parse_u8,
		.attr	= ATTR_TCP_STATE,
		.size	= NTA_SIZE(sizeof(uint8_t)),
	},
	[NTA_STATUS] = {
		.parse	= parse_u32,
		.attr	= ATTR_STATUS,
		.size	= NTA_SIZE(sizeof(uint32_t)),
	},
	[NTA_MARK] = {
		.parse	= parse_u32,
		.attr	= ATTR_MARK,
		.size	= NTA_SIZE(sizeof(uint32_t)),
	},
	[NTA_TIMEOUT] = {
		.parse	= parse_u32,
		.attr	= ATTR_TIMEOUT,
		.size	= NTA_SIZE(sizeof(uint32_t)),
	},
	[NTA_MASTER_IPV4] = {
		.parse	= parse_group,
		.attr	= ATTR_GRP_MASTER_IPV4,
		.size	= NTA_SIZE(sizeof(struct nfct_attr_grp_ipv4)),
	},
	[NTA_MASTER_IPV6] = {
		.parse	= parse_group,
		.attr	= ATTR_GRP_MASTER_IPV6,
		.size	= NTA_SIZE(sizeof(struct nfct_attr_grp_ipv6)),
	},
	[NTA_MASTER_L4PROTO] = {
		.parse	= parse_u8,
		.attr	= ATTR_MASTER_L4PROTO,
		.size	= NTA_SIZE(sizeof(uint8_t)),
	},
	[NTA_MASTER_PORT] = {
		.parse	= parse_group,
		.attr	= ATTR_GRP_MASTER_PORT,
		.size	= NTA_SIZE(sizeof(struct nfct_attr_grp_port)),
	},
	[NTA_SNAT_IPV4]	= {
		.parse	= parse_u32,
		.attr	= ATTR_SNAT_IPV4,
		.size	= NTA_SIZE(sizeof(uint32_t)),
	},
	[NTA_DNAT_IPV4] = {
		.parse	= parse_u32,
		.attr	= ATTR_DNAT_IPV4,
		.size	= NTA_SIZE(sizeof(uint32_t)),
	},
	[NTA_SPAT_PORT]	= {
		.parse	= parse_u16,
		.attr	= ATTR_SNAT_PORT,
		.size	= NTA_SIZE(sizeof(uint16_t)),
	},
	[NTA_DPAT_PORT]	= {
		.parse	= parse_u16,
		.attr	= ATTR_DNAT_PORT,
		.size	= NTA_SIZE(sizeof(uint16_t)),
	},
	[NTA_NAT_SEQ_ADJ] = {
		.parse	= parse_nat_seq_adj,
		.size	= NTA_SIZE(sizeof(struct nta_attr_natseqadj)),
	},
	[NTA_SCTP_STATE] = {
		.parse	= parse_u8,
		.attr	= ATTR_SCTP_STATE,
		.size	= NTA_SIZE(sizeof(uint8_t)),
	},
	[NTA_SCTP_VTAG_ORIG] = {
		.parse	= parse_u32,
		.attr	= ATTR_SCTP_VTAG_ORIG,
		.size	= NTA_SIZE(sizeof(uint32_t)),
	},
	[NTA_SCTP_VTAG_REPL] = {
		.parse	= parse_u32,
		.attr	= ATTR_SCTP_VTAG_REPL,
		.size	= NTA_SIZE(sizeof(uint32_t)),
	},
	[NTA_DCCP_STATE] = {
		.parse	= parse_u8,
		.attr	= ATTR_DCCP_STATE,
		.size	= NTA_SIZE(sizeof(uint8_t)),
	},
	[NTA_DCCP_ROLE] = {
		.parse	= parse_u8,
		.attr	= ATTR_DCCP_ROLE,
		.size	= NTA_SIZE(sizeof(uint8_t)),
	},
	[NTA_ICMP_TYPE] = {
		.parse	= parse_u8,
		.attr	= ATTR_ICMP_TYPE,
		.size	= NTA_SIZE(sizeof(uint8_t)),
	},
	[NTA_ICMP_CODE] = {
		.parse	= parse_u8,
		.attr	= ATTR_ICMP_CODE,
		.size	= NTA_SIZE(sizeof(uint8_t)),
	},
	[NTA_ICMP_ID] = {
		.parse	= parse_u16,
		.attr	= ATTR_ICMP_ID,
		.size	= NTA_SIZE(sizeof(uint16_t)),
	},
};

static void
parse_u8(struct nf_conntrack *ct, int attr, void *data)
{
	uint8_t *value = (uint8_t *) data;
	nfct_set_attr_u8(ct, h[attr].attr, *value);
}

static void
parse_u16(struct nf_conntrack *ct, int attr, void *data)
{
	uint16_t *value = (uint16_t *) data;
	nfct_set_attr_u16(ct, h[attr].attr, ntohs(*value));
}

static void
parse_u32(struct nf_conntrack *ct, int attr, void *data)
{
	uint32_t *value = (uint32_t *) data;
	nfct_set_attr_u32(ct, h[attr].attr, ntohl(*value));
}

static void
parse_group(struct nf_conntrack *ct, int attr, void *data)
{
	nfct_set_attr_grp(ct, h[attr].attr, data);
}

static void
parse_nat_seq_adj(struct nf_conntrack *ct, int attr, void *data)
{
	struct nta_attr_natseqadj *this = data;
	nfct_set_attr_u32(ct, ATTR_ORIG_NAT_SEQ_CORRECTION_POS, 
			  ntohl(this->orig_seq_correction_pos));
	nfct_set_attr_u32(ct, ATTR_ORIG_NAT_SEQ_OFFSET_BEFORE, 
			  ntohl(this->orig_seq_correction_pos));
	nfct_set_attr_u32(ct, ATTR_ORIG_NAT_SEQ_OFFSET_AFTER, 
			  ntohl(this->orig_seq_correction_pos));
	nfct_set_attr_u32(ct, ATTR_REPL_NAT_SEQ_CORRECTION_POS, 
			  ntohl(this->orig_seq_correction_pos));
	nfct_set_attr_u32(ct, ATTR_REPL_NAT_SEQ_OFFSET_BEFORE, 
			  ntohl(this->orig_seq_correction_pos));
	nfct_set_attr_u32(ct, ATTR_REPL_NAT_SEQ_OFFSET_AFTER, 
			  ntohl(this->orig_seq_correction_pos));
}

int parse_payload(struct nf_conntrack *ct, struct nethdr *net, size_t remain)
{
	int len;
	struct netattr *attr;

	if (remain < net->len)
		return -1;

	len = net->len - NETHDR_SIZ;
	attr = NETHDR_DATA(net);

	while (len > ssizeof(struct netattr)) {
		ATTR_NETWORK2HOST(attr);
		if (attr->nta_len > len)
			return -1;
		if (attr->nta_attr > NTA_MAX)
			return -1;
		if (attr->nta_len != h[attr->nta_attr].size)
			return -1;
		if (h[attr->nta_attr].parse == NULL) {
			attr = NTA_NEXT(attr, len);
			continue;
		}
		h[attr->nta_attr].parse(ct, attr->nta_attr, NTA_DATA(attr));
		attr = NTA_NEXT(attr, len);
	}

	return 0;
}
