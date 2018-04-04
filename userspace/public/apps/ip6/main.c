#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>
#include <fcntl.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/sockios.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <fnmatch.h>
#include <time.h>
#include <errno.h>
#include <limits.h>

#include "libnetlink.h"
#include "ll_map.h"
#include "utils.h"
#include "rt_names.h"





#define ADDROPER_DEL 0
#define ADDROPER_ADD 1
#define ADDROPER_UPDATE 2






int get_unsigned(unsigned *val, const char *arg, int base)
{
	unsigned long res;
	char *ptr;

	if (!arg || !*arg)
		return -1;
	res = strtoul(arg, &ptr, base);
	if (!ptr || ptr == arg || *ptr || res > UINT_MAX)
		return -1;
	*val = res;
	return 0;
}

int dnet_pton(int af, const char *src, void *addr) { return 0; }
int default_scope (inet_prefix *lcl)        { return 0; }

int get_addr_1(inet_prefix *addr, const char *name, int family)
{
	const char *cp;
	unsigned char *ap = (unsigned char*)addr->data;
	int i;

	memset(addr, 0, sizeof(*addr));

	if (strcmp(name, "default") == 0 ||
	    strcmp(name, "all") == 0 ||
	    strcmp(name, "any") == 0) {
		if (family == AF_DECnet)
			return -1;
		addr->family = family;
		addr->bytelen = (family == AF_INET6 ? 16 : 4);
		addr->bitlen = -1;
		return 0;
	}

	if (strchr(name, ':')) {
		addr->family = AF_INET6;
		if (family != AF_UNSPEC && family != AF_INET6)
			return -1;
		if (inet_pton(AF_INET6, name, addr->data) <= 0)
			return -1;
		addr->bytelen = 16;
		addr->bitlen = -1;
		return 0;
	}

	if (family == AF_DECnet) {
		struct dn_naddr dna;
		addr->family = AF_DECnet;
		if (dnet_pton(AF_DECnet, name, &dna) <= 0)
			return -1;
		memcpy(addr->data, dna.a_addr, 2);
		addr->bytelen = 2;
		addr->bitlen = -1;
		return 0;
	}

	addr->family = AF_INET;
	if (family != AF_UNSPEC && family != AF_INET)
		return -1;
	addr->bytelen = 4;
	addr->bitlen = -1;
	for (cp=name, i=0; *cp; cp++) {
		if (*cp <= '9' && *cp >= '0') {
			ap[i] = 10*ap[i] + (*cp-'0');
			continue;
		}
		if (*cp == '.' && ++i <= 3)
			continue;
		return -1;
	}
	return 0;
}

int get_prefix_1(inet_prefix *dst, char *arg, int family)
{
	int err;
	unsigned plen;
	char *slash;

	memset(dst, 0, sizeof(*dst));

	if (strcmp(arg, "default") == 0 ||
	    strcmp(arg, "any") == 0 ||
	    strcmp(arg, "all") == 0) {
		if (family == AF_DECnet)
			return -1;
		dst->family = family;
		dst->bytelen = 0;
		dst->bitlen = 0;
		return 0;
	}

	slash = strchr(arg, '/');
	if (slash)
		*slash = 0;

	err = get_addr_1(dst, arg, family);
	if (err == 0) {
		switch(dst->family) {
			case AF_INET6:
				dst->bitlen = 128;
				break;
			case AF_DECnet:
				dst->bitlen = 16;
				break;
			default:
			case AF_INET:
				dst->bitlen = 32;
		}
		if (slash) {
			if (get_unsigned(&plen, slash+1, 0) || plen > dst->bitlen) {
				err = -1;
				goto done;
			}
			dst->flags |= PREFIXLEN_SPECIFIED;
			dst->bitlen = plen;
		}
	}
done:
	if (slash)
		*slash = '/';
	return err;
}

				  
/** 
 * adds, updates or deletes addresses to interface
 * 
 * @param addr 
 * @param ifacename 
 * @param prefixLen 
 * @param preferred 
 * @param valid 
 * @param mode - 0-delete, 1-add, 2-update
 * 
 * @return 
 */
int ipaddr_add_or_del(const char * addr, const char *ifacename, int prefixLen, 
                      unsigned long preferred, unsigned long valid, int mode)
{
    struct rtnl_handle rth;
    struct {
	struct nlmsghdr 	n;
	struct ifaddrmsg 	ifa;
	char   			buf[256];
    } req;
    inet_prefix lcl;
    inet_prefix peer;
    int local_len = 0;
    int peer_len = 0;
    int scoped = 0;
    struct ifa_cacheinfo ci;

#ifdef LOWLEVEL_DEBUG
    printf("### iface=%s, addr=%s, add=%d ###\n", ifacename, addr, add);
#endif
    
    memset(&req, 0, sizeof(req));
    req.n.nlmsg_len = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
    switch (mode) {
    case ADDROPER_DEL:
        req.n.nlmsg_type = RTM_DELADDR; /* del address */
        req.n.nlmsg_flags = NLM_F_REQUEST;
        break;
    case ADDROPER_ADD:
        req.n.nlmsg_type = RTM_NEWADDR; /* add address */
        req.n.nlmsg_flags = NLM_F_REQUEST |NLM_F_CREATE|NLM_F_EXCL;
        break;
    case ADDROPER_UPDATE:
        req.n.nlmsg_type = RTM_NEWADDR; /* update address */
        req.n.nlmsg_flags = NLM_F_REQUEST | NLM_F_REPLACE;
        break;
    }
    req.ifa.ifa_family = AF_INET6;
    req.ifa.ifa_flags = 0;
    req.ifa.ifa_prefixlen = prefixLen;
    
    get_prefix_1(&lcl, (char*)addr, AF_INET6);
    
    addattr_l(&req.n, sizeof(req), IFA_LOCAL, &lcl.data, lcl.bytelen);
    local_len = lcl.bytelen;

    memset(&ci, 0, sizeof(ci));
		if (valid > 0)
	    ci.ifa_valid = valid;
		
		if (preferred > 0)
    	ci.ifa_prefered = preferred;
    addattr_l(&req.n, sizeof(req), IFA_CACHEINFO, &ci, sizeof(ci));
    
    if (peer_len == 0 && local_len) {
	peer = lcl;
	addattr_l(&req.n, sizeof(req), IFA_ADDRESS, &lcl.data, lcl.bytelen);
    }
    if (req.ifa.ifa_prefixlen == 0)
	req.ifa.ifa_prefixlen = lcl.bitlen;
    
    if (!scoped)
	req.ifa.ifa_scope = default_scope(&lcl);
    
    rtnl_open(&rth, 0);
    ll_init_map(&rth);
    
    /* is there an interface with this ifindex? */
    if ((req.ifa.ifa_index = ll_name_to_index((char*)ifacename)) == 0) {
	printf("Cannot find device: %s", ifacename);
	return -1;
    }
    rtnl_talk(&rth, &req.n, 0, 0, NULL, NULL, NULL); fflush(stdout);

    return 0;
}

void usage()
{
	printf ("usage: ip6 add|del|update INTERFACE IPV6_ADDRESS/IPV6_PREFIX_LENGTH PREFER_TIME VALID_TIME\n");
}

int main(int argc, char **argv)
{
	int plen, ptime,vtime;
	
	
	int cmd;

	char *ip, *dev;

	char *p;
	if (argc != 6)
	{
		usage();
		return 1;
	}


	if (!strncmp(argv[1],"add",strlen(argv[1])))
		cmd=ADDROPER_ADD;
	else if (!strncmp(argv[1],"del",strlen(argv[1])))
		cmd=ADDROPER_DEL;
	else if (!strncmp(argv[1],"update",strlen(argv[1])))
		cmd=ADDROPER_UPDATE;
	else
	{
		usage();
		return 1;
	}
	
	ip=argv[3];
	dev=argv[2];

	p=strstr(ip,"/");

	if (!p)
	{
		usage();
		return -1;
	}

	ip[strlen(ip)-strlen(p)] = '\0';
	
	
	plen = atoi(p+1);
	ptime = atoi(argv[4]);
	vtime = atoi(argv[5]);

	if (ptime > vtime)
	{
		printf ("Error: PERFERRED_TIME MUST smaller than VALID_TIME.\n");
		return -1;
	}

	if ((ptime == 0) || (vtime == 0))
	{
		char cmdLine[128];
		switch (cmd)
		{
			case ADDROPER_ADD:
				snprintf (cmdLine, sizeof(cmdLine), "ip -6 addr add %s/%d dev %s",ip,plen,dev);
				system(cmdLine);
				break;
			case ADDROPER_DEL:
				snprintf (cmdLine, sizeof(cmdLine), "ip -6 addr del %s/%d dev %s",ip,plen,dev);
				system(cmdLine);
				break;
			case ADDROPER_UPDATE:
				break;
		}
	}
	else 
	{
		if (ipaddr_add_or_del(ip, dev, plen, ptime, vtime, cmd) != 0)
			return -1;
		if (cmd == ADDROPER_DEL)
		{
			char cmdLine[128];
			snprintf (cmdLine, sizeof(cmdLine), "ip -6 route del %s/%d dev %s",ip,plen,dev);
			system(cmdLine);
		}
	}

	return 0;
}
