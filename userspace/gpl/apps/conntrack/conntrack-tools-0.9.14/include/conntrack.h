#ifndef _CONNTRACK_H
#define _CONNTRACK_H

#include "linux_list.h"
#include <stdint.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

#define PROGNAME "conntrack"

#include <netinet/in.h>

enum action {
	CT_NONE		= 0,
	
	CT_LIST_BIT 	= 0,
	CT_LIST 	= (1 << CT_LIST_BIT),
	
	CT_CREATE_BIT	= 1,
	CT_CREATE	= (1 << CT_CREATE_BIT),

	CT_UPDATE_BIT	= 2,
	CT_UPDATE	= (1 << CT_UPDATE_BIT),
	
	CT_DELETE_BIT	= 3,
	CT_DELETE	= (1 << CT_DELETE_BIT),
	
	CT_GET_BIT	= 4,
	CT_GET		= (1 << CT_GET_BIT),

	CT_FLUSH_BIT	= 5,
	CT_FLUSH	= (1 << CT_FLUSH_BIT),

	CT_EVENT_BIT	= 6,
	CT_EVENT	= (1 << CT_EVENT_BIT),

	CT_VERSION_BIT	= 7,
	CT_VERSION	= (1 << CT_VERSION_BIT),

	CT_HELP_BIT	= 8,
	CT_HELP		= (1 << CT_HELP_BIT),

	EXP_LIST_BIT 	= 9,
	EXP_LIST 	= (1 << EXP_LIST_BIT),
	
	EXP_CREATE_BIT	= 10,
	EXP_CREATE	= (1 << EXP_CREATE_BIT),
	
	EXP_DELETE_BIT	= 11,
	EXP_DELETE	= (1 << EXP_DELETE_BIT),
	
	EXP_GET_BIT	= 12,
	EXP_GET		= (1 << EXP_GET_BIT),

	EXP_FLUSH_BIT	= 13,
	EXP_FLUSH	= (1 << EXP_FLUSH_BIT),

	EXP_EVENT_BIT	= 14,
	EXP_EVENT	= (1 << EXP_EVENT_BIT),

	CT_COUNT_BIT	= 15,
	CT_COUNT	= (1 << CT_COUNT_BIT),

	EXP_COUNT_BIT	= 16,
	EXP_COUNT	= (1 << EXP_COUNT_BIT),

	X_STATS_BIT	= 17,
	X_STATS		= (1 << X_STATS_BIT),
};
#define NUMBER_OF_CMD   18

enum options {
	CT_OPT_ORIG_SRC_BIT	= 0,
	CT_OPT_ORIG_SRC 	= (1 << CT_OPT_ORIG_SRC_BIT),
	
	CT_OPT_ORIG_DST_BIT	= 1,
	CT_OPT_ORIG_DST		= (1 << CT_OPT_ORIG_DST_BIT),

	CT_OPT_ORIG		= (CT_OPT_ORIG_SRC | CT_OPT_ORIG_DST),
	
	CT_OPT_REPL_SRC_BIT	= 2,
	CT_OPT_REPL_SRC		= (1 << CT_OPT_REPL_SRC_BIT),
	
	CT_OPT_REPL_DST_BIT	= 3,
	CT_OPT_REPL_DST		= (1 << CT_OPT_REPL_DST_BIT),

	CT_OPT_REPL		= (CT_OPT_REPL_SRC | CT_OPT_REPL_DST),

	CT_OPT_PROTO_BIT	= 4,
	CT_OPT_PROTO		= (1 << CT_OPT_PROTO_BIT),

	CT_OPT_TUPLE_ORIG	= (CT_OPT_ORIG | CT_OPT_PROTO),
	CT_OPT_TUPLE_REPL	= (CT_OPT_REPL | CT_OPT_PROTO),

	CT_OPT_TIMEOUT_BIT	= 5,
	CT_OPT_TIMEOUT		= (1 << CT_OPT_TIMEOUT_BIT),

	CT_OPT_STATUS_BIT	= 6,
	CT_OPT_STATUS		= (1 << CT_OPT_STATUS_BIT),

	CT_OPT_ZERO_BIT		= 7,
	CT_OPT_ZERO		= (1 << CT_OPT_ZERO_BIT),

	CT_OPT_EVENT_MASK_BIT	= 8,
	CT_OPT_EVENT_MASK	= (1 << CT_OPT_EVENT_MASK_BIT),

	CT_OPT_EXP_SRC_BIT	= 9,
	CT_OPT_EXP_SRC		= (1 << CT_OPT_EXP_SRC_BIT),

	CT_OPT_EXP_DST_BIT	= 10,
	CT_OPT_EXP_DST		= (1 << CT_OPT_EXP_DST_BIT),

	CT_OPT_MASK_SRC_BIT	= 11,
	CT_OPT_MASK_SRC		= (1 << CT_OPT_MASK_SRC_BIT),

	CT_OPT_MASK_DST_BIT	= 12,
	CT_OPT_MASK_DST		= (1 << CT_OPT_MASK_DST_BIT),

	CT_OPT_NATRANGE_BIT	= 13,
	CT_OPT_NATRANGE		= (1 << CT_OPT_NATRANGE_BIT),

	CT_OPT_MARK_BIT		= 14,
	CT_OPT_MARK		= (1 << CT_OPT_MARK_BIT),

	CT_OPT_ID_BIT		= 15,
	CT_OPT_ID		= (1 << CT_OPT_ID_BIT),

	CT_OPT_FAMILY_BIT	= 16,
	CT_OPT_FAMILY		= (1 << CT_OPT_FAMILY_BIT),

	CT_OPT_SRC_NAT_BIT	= 17,
	CT_OPT_SRC_NAT		= (1 << CT_OPT_SRC_NAT_BIT),

	CT_OPT_DST_NAT_BIT	= 18,
	CT_OPT_DST_NAT		= (1 << CT_OPT_DST_NAT_BIT),

	CT_OPT_OUTPUT_BIT	= 19,
	CT_OPT_OUTPUT		= (1 << CT_OPT_OUTPUT_BIT),

	CT_OPT_SECMARK_BIT	= 20,
	CT_OPT_SECMARK		= (1 << CT_OPT_SECMARK_BIT),

	CT_OPT_BUFFERSIZE_BIT	= 21,
	CT_OPT_BUFFERSIZE	= (1 << CT_OPT_BUFFERSIZE_BIT),
	
	CT_OPT_MAX		= CT_OPT_BUFFERSIZE_BIT
};
#define NUMBER_OF_OPT	CT_OPT_MAX+1

enum {
	_O_XML			= (1 << 0),
	_O_EXT			= (1 << 1),
	_O_TMS			= (1 << 2),
	_O_ID			= (1 << 3),
};

struct ctproto_handler {
	struct list_head 	head;

	const char		*name;
	uint16_t 		protonum;
	const char		*version;

	enum ctattr_protoinfo	protoinfo_attr;
	
	int (*parse_opts)(char c,
			  struct nf_conntrack *ct,
			  struct nf_conntrack *exptuple,
			  struct nf_conntrack *mask,
			  unsigned int *flags);

	void (*final_check)(unsigned int flags,
			    unsigned int command,
			    struct nf_conntrack *ct);

	void (*help)(void);

	struct option 		*opts;

	unsigned int		option_offset;
};

enum exittype {
	OTHER_PROBLEM = 1,
	PARAMETER_PROBLEM,
	VERSION_PROBLEM
};

int generic_opt_check(int options, int nops,
		      char *optset, const char *optflg[],
		      unsigned int *coupled_flags, int coupled_flags_size,
		      int *partial);
void exit_error(enum exittype status, const char *msg, ...);

extern void register_proto(struct ctproto_handler *h);

extern void register_tcp(void);
extern void register_udp(void);
extern void register_udplite(void);
extern void register_sctp(void);
extern void register_dccp(void);
extern void register_icmp(void);
extern void register_icmpv6(void);
extern void register_gre(void);
extern void register_unknown(void);

#endif
