#ifndef _INTERNAL_H_
#define _INTERNAL_H_

#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

struct nf_conntrack;

enum {
	INTERNAL_F_POPULATE	= (1 << 0),
	INTERNAL_F_RESYNC	= (1 << 1),
	INTERNAL_F_MAX		= (1 << 2)
};

struct internal_handler {
	void	*data;
	unsigned int flags;

	int	(*init)(void);
	void	(*close)(void);

	void	(*new)(struct nf_conntrack *ct, int origin_type);
	void	(*update)(struct nf_conntrack *ct, int origin_type);
	int	(*destroy)(struct nf_conntrack *ct, int origin_type);

	void	(*dump)(int fd, int type);
	void	(*populate)(struct nf_conntrack *ct);
	void	(*purge)(void);
	int	(*resync)(enum nf_conntrack_msg_type type,
			  struct nf_conntrack *ct, void *data);
	void	(*flush)(void);

	void	(*stats)(int fd);
	void	(*stats_ext)(int fd);
};

extern struct internal_handler internal_cache;
extern struct internal_handler internal_bypass;

#endif
