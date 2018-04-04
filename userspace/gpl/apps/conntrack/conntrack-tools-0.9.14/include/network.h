#ifndef _NETWORK_H_
#define _NETWORK_H_

#include <stdint.h>
#include <sys/types.h>

#define CONNTRACKD_PROTOCOL_VERSION	0

struct nf_conntrack;

struct nethdr {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t type:4,
		version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t version:4,
		type:4;
#else
#error  "Unknown system endianess!"
#endif
	uint8_t flags;
	uint16_t len;
	uint32_t seq;
};
#define NETHDR_SIZ nethdr_align(sizeof(struct nethdr))

enum nethdr_type {
	NET_T_STATE_NEW = 0,
	NET_T_STATE_UPD,
	NET_T_STATE_DEL,
	NET_T_STATE_MAX = NET_T_STATE_DEL,
	NET_T_CTL = 10,
};

int nethdr_align(int len);
int nethdr_size(int len);
void nethdr_set(struct nethdr *net, int type);
void nethdr_set_ack(struct nethdr *net);
void nethdr_set_ctl(struct nethdr *net);
int object_status_to_network_type(int status);

#define NETHDR_DATA(x)							 \
	(struct netattr *)(((char *)x) + NETHDR_SIZ)
#define NETHDR_TAIL(x)							 \
	(struct netattr *)(((char *)x) + x->len)

struct nethdr_ack {
#if __BYTE_ORDER == __LITTLE_ENDIAN
	uint8_t type:4,
		version:4;
#elif __BYTE_ORDER == __BIG_ENDIAN
	uint8_t version:4,
		type:4;
#else
#error  "Unknown system endianess!"
#endif
	uint8_t flags; 
	uint16_t len;
	uint32_t seq;
	uint32_t from;
	uint32_t to;
};
#define NETHDR_ACK_SIZ nethdr_align(sizeof(struct nethdr_ack))

enum {
	NET_F_UNUSED 	= (1 << 0),
	NET_F_RESYNC 	= (1 << 1),
	NET_F_NACK 	= (1 << 2),
	NET_F_ACK 	= (1 << 3),
	NET_F_ALIVE 	= (1 << 4),
	NET_F_HELLO	= (1 << 5),
	NET_F_HELLO_BACK= (1 << 6),
};

enum {
	MSG_DATA,
	MSG_CTL,
	MSG_DROP,
	MSG_BAD,
};

#define BUILD_NETMSG(ct, query)					\
({								\
	static char __net[4096];				\
	struct nethdr *__hdr = (struct nethdr *) __net;		\
	memset(__hdr, 0, NETHDR_SIZ);				\
	nethdr_set(__hdr, query);				\
	build_payload(ct, __hdr);				\
	HDR_HOST2NETWORK(__hdr);				\
	__hdr;							\
})

struct mcast_sock_multi;

enum {
	SEQ_UNKNOWN,
	SEQ_UNSET,
	SEQ_IN_SYNC,
	SEQ_AFTER,
	SEQ_BEFORE,
};

int nethdr_track_seq(uint32_t seq, uint32_t *exp_seq);
void nethdr_track_update_seq(uint32_t seq);
int nethdr_track_is_seq_set(void);

struct mcast_conf;

#define IS_DATA(x)	(x->type <= NET_T_STATE_MAX && \
			(x->flags & ~(NET_F_HELLO | NET_F_HELLO_BACK)) == 0)
#define IS_ACK(x)	(x->type == NET_T_CTL && x->flags & NET_F_ACK)
#define IS_NACK(x)	(x->type == NET_T_CTL && x->flags & NET_F_NACK)
#define IS_RESYNC(x)	(x->type == NET_T_CTL && x->flags & NET_F_RESYNC)
#define IS_ALIVE(x)	(x->type == NET_T_CTL && x->flags & NET_F_ALIVE)
#define IS_HELLO(x)	(x->flags & NET_F_HELLO)
#define IS_HELLO_BACK(x)(x->flags & NET_F_HELLO_BACK)

#define HDR_NETWORK2HOST(x)						\
({									\
	x->len   = ntohs(x->len);					\
	x->seq   = ntohl(x->seq);					\
	if (IS_ACK(x) || IS_NACK(x) || IS_RESYNC(x)) {			\
		struct nethdr_ack *__ack = (struct nethdr_ack *) x;	\
		__ack->from = ntohl(__ack->from);			\
		__ack->to = ntohl(__ack->to);				\
	}								\
})

#define HDR_HOST2NETWORK(x)						\
({									\
	if (IS_ACK(x) || IS_NACK(x) || IS_RESYNC(x)) {			\
		struct nethdr_ack *__ack = (struct nethdr_ack *) x;	\
		__ack->from = htonl(__ack->from);			\
		__ack->to = htonl(__ack->to);				\
	}								\
	x->len   = htons(x->len);					\
	x->seq   = htonl(x->seq);					\
})

/* extracted from net/tcp.h */

/*
 * The next routines deal with comparing 32 bit unsigned ints
 * and worry about wraparound (automatic with unsigned arithmetic).
 */

static inline int before(uint32_t seq1, uint32_t seq2)
{
	return (int32_t)(seq1-seq2) < 0;
}
#define after(seq2, seq1)       before(seq1, seq2)

/* is s2<=s1<=s3 ? */
static inline int between(uint32_t seq1, uint32_t seq2, uint32_t seq3)
{
	return seq3 - seq2 >= seq1 - seq2;
}

#define PLD_NETWORK2HOST(x)						 \
({									 \
	x->len = ntohs(x->len);						 \
	x->query = ntohs(x->query);					 \
})

#define PLD_HOST2NETWORK(x)						 \
({									 \
	x->len = htons(x->len);						 \
	x->query = htons(x->query);					 \
})

struct netattr {
	uint16_t nta_len;
	uint16_t nta_attr;
};

#define ATTR_NETWORK2HOST(x)						 \
({									 \
	x->nta_len = ntohs(x->nta_len);					 \
	x->nta_attr = ntohs(x->nta_attr);				 \
})

#define NTA_SIZE(len) NTA_ALIGN(sizeof(struct netattr)) + len

#define NTA_DATA(x)							 \
	(void *)(((char *)x) + NTA_ALIGN(sizeof(struct netattr)))

#define NTA_NEXT(x, len)						      \
(									      \
	len -= NTA_ALIGN(x->nta_len),					      \
	(struct netattr *)(((char *)x) + NTA_ALIGN(x->nta_len))		      \
)

#define NTA_ALIGNTO	4
#define NTA_ALIGN(len)	(((len) + NTA_ALIGNTO - 1) & ~(NTA_ALIGNTO - 1))
#define NTA_LENGTH(len)	(NTA_ALIGN(sizeof(struct netattr)) + (len))

enum nta_attr {
	NTA_IPV4 = 0,		/* struct nfct_attr_grp_ipv4 */
	NTA_IPV6,		/* struct nfct_attr_grp_ipv6 */
	NTA_L4PROTO,		/* uint8_t */
	NTA_PORT,		/* struct nfct_attr_grp_port */
	NTA_TCP_STATE = 4,	/* uint8_t */
	NTA_STATUS,		/* uint32_t */
	NTA_TIMEOUT,		/* uint32_t */
	NTA_MARK,		/* uint32_t */
	NTA_MASTER_IPV4 = 8,	/* struct nfct_attr_grp_ipv4 */
	NTA_MASTER_IPV6,	/* struct nfct_attr_grp_ipv6 */
	NTA_MASTER_L4PROTO,	/* uint8_t */
	NTA_MASTER_PORT,	/* struct nfct_attr_grp_port */
	NTA_SNAT_IPV4 = 12,	/* uint32_t */
	NTA_DNAT_IPV4,		/* uint32_t */
	NTA_SPAT_PORT,		/* uint16_t */
	NTA_DPAT_PORT,		/* uint16_t */
	NTA_NAT_SEQ_ADJ = 16,	/* struct nta_attr_natseqadj */
	NTA_SCTP_STATE,		/* uint8_t */
	NTA_SCTP_VTAG_ORIG,	/* uint32_t */
	NTA_SCTP_VTAG_REPL,	/* uint32_t */
	NTA_DCCP_STATE = 20,	/* uint8_t */
	NTA_DCCP_ROLE,		/* uint8_t */
	NTA_ICMP_TYPE,		/* uint8_t */
	NTA_ICMP_CODE,		/* uint8_t */
	NTA_ICMP_ID,		/* uint16_t */
	NTA_MAX
};

struct nta_attr_natseqadj {
	uint32_t orig_seq_correction_pos;
	uint32_t orig_seq_offset_before;
	uint32_t orig_seq_offset_after;
	uint32_t repl_seq_correction_pos;
	uint32_t repl_seq_offset_before;
	uint32_t repl_seq_offset_after;
};

void build_payload(const struct nf_conntrack *ct, struct nethdr *n);

int parse_payload(struct nf_conntrack *ct, struct nethdr *n, size_t remain);

#endif
