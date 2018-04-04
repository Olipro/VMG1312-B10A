#ifndef _EXTERNAL_H_
#define _EXTERNAL_H_

struct nf_conntrack;

struct external_handler {
	int	(*init)(void);
	void	(*close)(void);

	void	(*new)(struct nf_conntrack *ct);
	void	(*update)(struct nf_conntrack *ct);
	void	(*destroy)(struct nf_conntrack *ct);

	void	(*dump)(int fd, int type);
	void	(*flush)(void);
	void	(*commit)(struct nfct_handle *h, int fd);
	void	(*stats)(int fd);
	void	(*stats_ext)(int fd);
};

extern struct external_handler external_cache;
extern struct external_handler external_inject;

#endif
