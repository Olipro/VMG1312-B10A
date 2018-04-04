#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <asm/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <errno.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>

struct nlmsg_list
{
	struct nlmsg_list *next;
	struct nlmsghdr h;
};

struct rtnl_handle
{
        int                     fd;
        struct sockaddr_nl      local;
        struct sockaddr_nl      peer;
        __u32                   seq;
        __u32                   dump;
};

typedef int (*rtnl_filter_t)(const struct sockaddr_nl *, struct nlmsghdr *n, void *);
struct rtnl_dump_filter_arg
{
	rtnl_filter_t filter;
	void *arg1;
	rtnl_filter_t junk;
	void *arg2;
};

static void rtnl_close(struct rtnl_handle *rth)
{
	close(rth->fd);
}

static int rtnl_open(struct rtnl_handle *rth, unsigned subscriptions)
{
	socklen_t addr_len;
	int sndbuf = 32768;
	int rcvbuf = 32768;

	memset(rth, 0, sizeof(rth));

	rth->fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE);
	if (rth->fd < 0) {
		return -1;
	}	
	if (setsockopt(rth->fd,SOL_SOCKET,SO_SNDBUF,&sndbuf,sizeof(sndbuf)) < 0) {
		return -1;
	}
	if (setsockopt(rth->fd,SOL_SOCKET,SO_RCVBUF,&rcvbuf,sizeof(rcvbuf)) < 0) {
		return -1;
	}
	
	memset(&rth->local, 0, sizeof(rth->local));
	rth->local.nl_family = AF_NETLINK;
	rth->local.nl_groups = subscriptions;

	if (bind(rth->fd, (struct sockaddr*)&rth->local, sizeof(rth->local)) < 0) {
		return -1;
	}
	addr_len = sizeof(rth->local);
	if (getsockname(rth->fd, (struct sockaddr*)&rth->local, &addr_len) <0 ) {
		return -1;
	}
	if (addr_len != sizeof(rth->local)) {
		return -1;
	}
	if (rth->local.nl_family != AF_NETLINK) {
		return -1;
	}
	rth->seq = time(NULL);

	return 0;
}

static int rtnl_wilddump_request(struct rtnl_handle *rth, int family, int type)
{
	struct {
		struct nlmsghdr nlh;
		struct rtgenmsg g;
	} req;

	memset(&req, 0, sizeof(req));
	req.nlh.nlmsg_len = sizeof(req);
	req.nlh.nlmsg_type = type;
	req.nlh.nlmsg_flags = NLM_F_ROOT|NLM_F_MATCH|NLM_F_REQUEST;
	req.nlh.nlmsg_pid = 0;
	req.nlh.nlmsg_seq = rth->dump = ++rth->seq;
	req.g.rtgen_family = family;

	return send(rth->fd, (void*)&req, sizeof(req), 0);
}

static int rtnl_dump_filter_l(struct rtnl_handle *rth, const struct rtnl_dump_filter_arg *arg)
{
	struct sockaddr_nl nladdr;
	char buf[16384];
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = sizeof(buf),
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	int status;
	const struct rtnl_dump_filter_arg *a;

	while (1) {
		status = recvmsg(rth->fd, &msg, 0);
		
		if (status < 0) {
			if (errno == EINTR || errno == EAGAIN)
				continue;
			return -1;
		}
		if (status == 0) {
			return -1;
		}
	
		for (a = arg; a->filter; a++) {
			struct nlmsghdr *h = (struct nlmsghdr*)buf;

			while (NLMSG_OK(h, status)) {
				int err;
				if (nladdr.nl_pid != 0 
					|| h->nlmsg_pid != rth->local.nl_pid
					|| h->nlmsg_seq != rth->dump) 
				{
					goto skip_it;
				}
			
				if (h->nlmsg_type == NLMSG_DONE)
					return 0;
				if (h->nlmsg_type == NLMSG_ERROR) {
					return -1;
				}
				err = a->filter(&nladdr, h, a->arg1);
				if (err < 0)
					return err;
skip_it:
				h = NLMSG_NEXT(h, status);
			}
		} while(0);	
		if (msg.msg_flags & MSG_TRUNC)
			continue;
		if (status)
			exit(1);
	}
}

static int rtnl_dump_filter(struct rtnl_handle *rth, rtnl_filter_t filter, void *arg1, 
			rtnl_filter_t junk, void *arg2)
{
	const struct rtnl_dump_filter_arg a[2] = {
		{.filter=filter, .arg1=arg1, .junk=junk, .arg2=arg2},
		{.filter=NULL, .arg1=NULL, .junk=NULL, .arg2=NULL}
	};

	return rtnl_dump_filter_l(rth, a);
}

static int parse_rtattr(struct rtattr *tb[], int max, struct rtattr *rta, int len)
{
	memset(tb, 0, sizeof(struct rtattr *) * (max+1));
	while (RTA_OK(rta, len)) {
		if (rta->rta_type <= max)
			tb[rta->rta_type] = rta;
		rta = RTA_NEXT(rta, len);
	}
	return 0;	
}

static int store_addr(const struct sockaddr_nl *who, struct nlmsghdr *n, void *arg)
{
	struct nlmsg_list **linfo = (struct nlmsg_list**)arg;
	struct nlmsg_list *h;
	struct nlmsg_list **lp;

	h = malloc(n->nlmsg_len+sizeof(void*));
	if (h == NULL)
		return -1;
	memcpy(&h->h, n, n->nlmsg_len);
	h->next = NULL;

	for (lp = linfo; *lp; lp = &(*lp)->next);
	*lp = h;

	return 0;
}

int get_tentative_addr(struct in6_addr *iana)
{
	struct rtnl_handle rth;
	struct rtattr *rta_tb[IFA_MAX+1];
	struct nlmsg_list *ainfo = NULL, *tmp;
	int len;
	int iana_index = 0;

	rtnl_open(&rth,0);
	if (rtnl_wilddump_request(&rth, AF_INET6, RTM_GETADDR) < 0 ) {
		return (-1);
	}
	if (rtnl_dump_filter(&rth, store_addr, &ainfo, NULL, NULL) < 0) {
		return (-1);
	}

	tmp = ainfo;
	for (; ainfo; ainfo = ainfo->next) {
		struct nlmsghdr *n = &ainfo->h;
		struct ifaddrmsg *ifa = NLMSG_DATA(n);
		
		if ((n->nlmsg_type != RTM_NEWADDR) || (ifa->ifa_family != AF_INET6))
			continue;
		if (n->nlmsg_len < NLMSG_LENGTH(sizeof(ifa))) {
			iana_index = -1;
			goto free_resource;
		}
		len = n->nlmsg_len - NLMSG_LENGTH(sizeof(*ifa));
		if (len < 0) {
			iana_index = -1;
			goto free_resource;
		}
		parse_rtattr(rta_tb, IFA_MAX, IFA_RTA(ifa), len);

		if (!rta_tb[IFA_LOCAL])
			rta_tb[IFA_LOCAL] = rta_tb[IFA_ADDRESS];
		if (!rta_tb[IFA_ADDRESS])
			rta_tb[IFA_ADDRESS] = rta_tb[IFA_LOCAL];
		
		if (ifa->ifa_flags & IFA_F_TENTATIVE) {
			memcpy(&iana[iana_index], RTA_DATA(rta_tb[IFA_LOCAL]), 
					sizeof(struct in6_addr));
			iana_index++;
		}
	}

free_resource:
	ainfo = tmp;	
	while (ainfo) {
		tmp = ainfo->next;
		free(ainfo);
		ainfo = tmp;
	}

	rtnl_close(&rth);

	return (iana_index);
}

char* link_detect_main(int fd)
{
	struct sockaddr_nl nladdr;
	char buf[2048];
	struct iovec iov = {
		.iov_base = buf,
		.iov_len = sizeof(buf),
	};
	struct msghdr msg = {
		.msg_name = &nladdr,
		.msg_namelen = sizeof(nladdr),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	int status, ifilen;
	struct nlmsghdr *h = NULL;
	struct ifinfomsg *ifi;
	struct rtattr *rta_tb[IFLA_MAX+1];
	static char ifname[20];

	status = recvmsg(fd, &msg, 0);
	h = (struct nlmsghdr*)buf;
	for(; NLMSG_OK(h, status); h = NLMSG_NEXT(h, status)) {
		if (h->nlmsg_type == NLMSG_DONE) {
			return NULL;
		}
		else if (h->nlmsg_type == NLMSG_ERROR) {
			return NULL;
		}
		else if (h->nlmsg_type == RTM_NEWLINK) {
			ifi = NLMSG_DATA(h);
			ifilen = h->nlmsg_len - NLMSG_LENGTH(sizeof(struct ifinfomsg));
			if (ifilen < 0) {
				return NULL;
			}
			parse_rtattr(rta_tb, IFLA_MAX, IFLA_RTA(ifi), ifilen);
			if (rta_tb[IFLA_IFNAME]) {
				if (ifi->ifi_flags & (IFF_UP|IFF_RUNNING)) {
					/* link up */
					strcpy(ifname, (char*)RTA_DATA(rta_tb[IFLA_IFNAME]));
					return ifname;
				}
			}
		}
	}
	return NULL;
}

int link_detect_init()
{
	struct sockaddr_nl sa = {
		.nl_family = AF_NETLINK,
		.nl_groups = RTMGRP_LINK | RTMGRP_IPV4_IFADDR | RTMGRP_IPV6_IFADDR,
	};
	int fd;
	
	if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) == -1) {
		return -1;
	}
	if (bind(fd, (struct sockaddr*)&sa, sizeof(sa)) == -1) {
		return -1;
	}
	return fd;
}

