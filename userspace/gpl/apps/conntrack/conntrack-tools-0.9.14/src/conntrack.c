/*
 * (C) 2005-2008 by Pablo Neira Ayuso <pablo@netfilter.org>
 *
 *      This program is free software; you can redistribute it and/or modify
 *      it under the terms of the GNU General Public License as published by
 *      the Free Software Foundation; either version 2 of the License, or
 *      (at your option) any later version.
 *
 *      This program is distributed in the hope that it will be useful,
 *      but WITHOUT ANY WARRANTY; without even the implied warranty of
 *      MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *      GNU General Public License for more details.
 *
 *      You should have received a copy of the GNU General Public License
 *      along with this program; if not, write to the Free Software
 *      Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * Note:
 *	Yes, portions of this code has been stolen from iptables ;)
 *	Special thanks to the the Netfilter Core Team.
 *	Thanks to Javier de Miguel Rodriguez <jmiguel at talika.eii.us.es>
 *	for introducing me to advanced firewalling stuff.
 *
 *						--pablo 13/04/2005
 *
 * 2005-04-16 Harald Welte <laforge@netfilter.org>: 
 * 	Add support for conntrack accounting and conntrack mark
 * 2005-06-23 Harald Welte <laforge@netfilter.org>:
 * 	Add support for expect creation
 * 2005-09-24 Harald Welte <laforge@netfilter.org>:
 * 	Remove remaints of "-A"
 * 2007-04-22 Pablo Neira Ayuso <pablo@netfilter.org>:
 * 	Ported to the new libnetfilter_conntrack API
 * 2008-04-13 Pablo Neira Ayuso <pablo@netfilter.org>:
 *	Way more flexible update and delete operations
 */

#include "conntrack.h"

#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif
#include <signal.h>
#include <string.h>
#include <netdb.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libnetfilter_conntrack/libnetfilter_conntrack.h>

static const char *optflags[NUMBER_OF_OPT] = {
"src","dst","reply-src","reply-dst","protonum","timeout","status","zero",
"event-mask","tuple-src","tuple-dst","mask-src","mask-dst","nat-range","mark",
"id","family","src-nat","dst-nat","output","secmark","buffersize"};

static struct option original_opts[] = {
	{"dump", 2, 0, 'L'},
	{"create", 2, 0, 'I'},
	{"delete", 2, 0, 'D'},
	{"update", 2, 0, 'U'},
	{"get", 2, 0, 'G'},
	{"flush", 2, 0, 'F'},
	{"event", 2, 0, 'E'},
	{"counter", 2, 0, 'C'},
	{"stats", 0, 0, 'S'},
	{"version", 0, 0, 'V'},
	{"help", 0, 0, 'h'},
	{"orig-src", 1, 0, 's'},
	{"src", 1, 0, 's'},
	{"orig-dst", 1, 0, 'd'},
	{"dst", 1, 0, 'd'},
	{"reply-src", 1, 0, 'r'},
	{"reply-dst", 1, 0, 'q'},
	{"protonum", 1, 0, 'p'},
	{"timeout", 1, 0, 't'},
	{"status", 1, 0, 'u'},
	{"zero", 0, 0, 'z'},
	{"event-mask", 1, 0, 'e'},
	{"tuple-src", 1, 0, '['},
	{"tuple-dst", 1, 0, ']'},
	{"mask-src", 1, 0, '{'},
	{"mask-dst", 1, 0, '}'},
	{"nat-range", 1, 0, 'a'},	/* deprecated */
	{"mark", 1, 0, 'm'},
	{"secmark", 1, 0, 'c'},
	{"id", 2, 0, 'i'},		/* deprecated */
	{"family", 1, 0, 'f'},
	{"src-nat", 2, 0, 'n'},
	{"dst-nat", 2, 0, 'g'},
	{"output", 1, 0, 'o'},
	{"buffer-size", 1, 0, 'b'},
	{0, 0, 0, 0}
};

#define OPTION_OFFSET 256

static struct nfct_handle *cth, *ith;
static struct option *opts = original_opts;
static unsigned int global_option_offset = 0;

/* Table of legal combinations of commands and options.  If any of the
 * given commands make an option legal, that option is legal (applies to
 * CMD_LIST and CMD_ZERO only).
 * Key:
 *  0  illegal
 *  1  compulsory
 *  2  optional
 *  3  undecided, see flag combination checkings in generic_opt_check()
 */

static char commands_v_options[NUMBER_OF_CMD][NUMBER_OF_OPT] =
/* Well, it's better than "Re: Linux vs FreeBSD" */
{
          /*   s d r q p t u z e [ ] { } a m i f n g o c b*/
/*CT_LIST*/   {2,2,2,2,2,0,2,2,0,0,0,0,0,0,2,0,2,2,2,2,2,0},
/*CT_CREATE*/ {3,3,3,3,1,1,2,0,0,0,0,0,0,2,2,0,0,2,2,0,0,0},
/*CT_UPDATE*/ {2,2,2,2,2,2,2,0,0,0,0,0,0,0,2,2,2,2,2,2,0,0},
/*CT_DELETE*/ {2,2,2,2,2,2,2,0,0,0,0,0,0,0,2,2,2,2,2,2,0,0},
/*CT_GET*/    {3,3,3,3,1,0,0,0,0,0,0,0,0,0,0,2,0,0,0,2,0,0},
/*CT_FLUSH*/  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*CT_EVENT*/  {2,2,2,2,2,0,0,0,2,0,0,0,0,0,2,0,0,2,2,2,2,2},
/*VERSION*/   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*HELP*/      {0,0,0,0,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*EXP_LIST*/  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,2,0,0,0,0,0},
/*EXP_CREATE*/{1,1,2,2,1,1,2,0,0,1,1,1,1,0,0,0,0,0,0,0,0,0},
/*EXP_DELETE*/{1,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*EXP_GET*/   {1,1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*EXP_FLUSH*/ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*EXP_EVENT*/ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*CT_COUNT*/  {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*EXP_COUNT*/ {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
/*X_STATS*/   {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
};

#define ADDR_VALID_FLAGS_MAX   2
static unsigned int addr_valid_flags[ADDR_VALID_FLAGS_MAX] = {
	CT_OPT_ORIG_SRC | CT_OPT_ORIG_DST,
	CT_OPT_REPL_SRC | CT_OPT_REPL_DST,
};

static LIST_HEAD(proto_list);

static unsigned int options;

#define CT_COMPARISON (CT_OPT_PROTO | CT_OPT_ORIG | CT_OPT_REPL | CT_OPT_MARK |\
		       CT_OPT_SECMARK |  CT_OPT_STATUS | CT_OPT_ID)

void register_proto(struct ctproto_handler *h)
{
	if (strcmp(h->version, VERSION) != 0) {
		fprintf(stderr, "plugin `%s': version %s (I'm %s)\n",
			h->name, h->version, VERSION);
		exit(1);
	}
	list_add(&h->head, &proto_list);
}

extern struct ctproto_handler ct_proto_unknown;

static struct ctproto_handler *findproto(char *name, int *pnum)
{
	struct ctproto_handler *cur;
	struct protoent *pent;
	int protonum;

	/* is it in the list of supported protocol? */
	list_for_each_entry(cur, &proto_list, head) {
		if (strcmp(cur->name, name) == 0) {
			*pnum = cur->protonum;
			return cur;
		}
	}
	/* using the protocol name for an unsupported protocol? */
	if ((pent = getprotobyname(name))) {
		*pnum = pent->p_proto;
		return &ct_proto_unknown;
	}
	/* using a protocol number? */
	protonum = atoi(name);
	if (protonum > 0 && protonum <= IPPROTO_MAX) {
		/* try lookup by number, perhaps this protocol is supported */
		list_for_each_entry(cur, &proto_list, head) {
			if (cur->protonum == protonum) {
				*pnum = protonum;
				return cur;
			}
		}
		*pnum = protonum;
		return &ct_proto_unknown;
	}

	return NULL;
}

static void
extension_help(struct ctproto_handler *h, int protonum)
{
	const char *name;

	if (h == &ct_proto_unknown) {
		struct protoent *pent;

		pent = getprotobynumber(protonum);
		if (!pent)
			name = h->name;
		else
			name = pent->p_name;
	} else {
		name = h->name;
	}

	fprintf(stdout, "Proto `%s' help:\n", name);
	h->help();
}

static void __attribute__((noreturn))
exit_tryhelp(int status)
{
	fprintf(stderr, "Try `%s -h' or '%s --help' for more information.\n",
			PROGNAME, PROGNAME);
	exit(status);
}

static void free_options(void)
{
	if (opts != original_opts) {
		free(opts);
		opts = original_opts;
		global_option_offset = 0;
	}
}

void __attribute__((noreturn))
exit_error(enum exittype status, const char *msg, ...)
{
	va_list args;

	free_options();
	va_start(args, msg);
	fprintf(stderr,"%s v%s (conntrack-tools): ", PROGNAME, VERSION);
	vfprintf(stderr, msg, args);
	fprintf(stderr, "\n");
	va_end(args);
	if (status == PARAMETER_PROBLEM)
		exit_tryhelp(status);
	exit(status);
}

static int bit2cmd(int command)
{
	int i;

	for (i = 0; i < NUMBER_OF_CMD; i++)
		if (command & (1<<i))
			break;

	return i;
}

int generic_opt_check(int local_options, int num_opts,
		      char *optset, const char *optflg[],
		      unsigned int *coupled_flags, int coupled_flags_size,
		      int *partial)
{
	int i, matching = -1, special_case = 0;

	for (i = 0; i < num_opts; i++) {
		if (!(local_options & (1<<i))) {
			if (optset[i] == 1)
				exit_error(PARAMETER_PROBLEM, 
					   "You need to supply the "
					   "`--%s' option for this "
					   "command", optflg[i]);
		} else {
			if (optset[i] == 0)
				exit_error(PARAMETER_PROBLEM, "Illegal "
					   "option `--%s' with this "
					   "command", optflg[i]);
		}
		if (optset[i] == 3)
			special_case = 1;
	}

	/* no weird flags combinations, leave */
	if (!special_case || coupled_flags == NULL)
		return 1;

	*partial = -1;
	for (i=0; i<coupled_flags_size; i++) {
		/* we look for an exact matching to ensure this is correct */
		if ((local_options & coupled_flags[i]) == coupled_flags[i]) {
			matching = i;
			break;
		}
		/* ... otherwise look for the first partial matching */
		if ((local_options & coupled_flags[i]) && *partial < 0) {
			*partial = i;
		}
	}

	/* we found an exact matching, game over */
	if (matching >= 0)
		return 1;

	/* report a partial matching to suggest something */
	return 0;
}

static struct option *
merge_options(struct option *oldopts, const struct option *newopts,
	      unsigned int *option_offset)
{
	unsigned int num_old, num_new, i;
	struct option *merge;

	for (num_old = 0; oldopts[num_old].name; num_old++);
	for (num_new = 0; newopts[num_new].name; num_new++);

	global_option_offset += OPTION_OFFSET;
	*option_offset = global_option_offset;

	merge = malloc(sizeof(struct option) * (num_new + num_old + 1));
	if (merge == NULL)
		return NULL;

	memcpy(merge, oldopts, num_old * sizeof(struct option));
	for (i = 0; i < num_new; i++) {
		merge[num_old + i] = newopts[i];
		merge[num_old + i].val += *option_offset;
	}
	memset(merge + num_old + num_new, 0, sizeof(struct option));

	return merge;
}

/* From linux/errno.h */
#define ENOTSUPP        524     /* Operation is not supported */

/* Translates errno numbers into more human-readable form than strerror. */
static const char *
err2str(int err, enum action command)
{
	unsigned int i;
	struct table_struct {
		enum action act;
		int err;
		const char *message;
	} table [] =
	  { { CT_LIST, ENOTSUPP, "function not implemented" },
	    { 0xFFFF, EINVAL, "invalid parameters" },
	    { CT_CREATE, EEXIST, "Such conntrack exists, try -U to update" },
	    { CT_CREATE|CT_GET|CT_DELETE, ENOENT, 
		    "such conntrack doesn't exist" },
	    { CT_CREATE|CT_GET, ENOMEM, "not enough memory" },
	    { CT_GET, EAFNOSUPPORT, "protocol not supported" },
	    { CT_CREATE, ETIME, "conntrack has expired" },
	    { EXP_CREATE, ENOENT, "master conntrack not found" },
	    { EXP_CREATE, EINVAL, "invalid parameters" },
	    { ~0U, EPERM, "sorry, you must be root or get "
		    	   "CAP_NET_ADMIN capability to do this"}
	  };

	for (i = 0; i < sizeof(table)/sizeof(struct table_struct); i++) {
		if ((table[i].act & command) && table[i].err == err)
			return table[i].message;
	}

	return strerror(err);
}

#define PARSE_STATUS 0
#define PARSE_EVENT 1
#define PARSE_OUTPUT 2
#define PARSE_MAX 3

static struct parse_parameter {
	const char	*parameter[6];
	size_t  size;
	unsigned int value[6];
} parse_array[PARSE_MAX] = {
	{ {"ASSURED", "SEEN_REPLY", "UNSET", "FIXED_TIMEOUT", "EXPECTED"}, 5,
	  { IPS_ASSURED, IPS_SEEN_REPLY, 0, IPS_FIXED_TIMEOUT, IPS_EXPECTED} },
	{ {"ALL", "NEW", "UPDATES", "DESTROY"}, 4,
	  {~0U, NF_NETLINK_CONNTRACK_NEW, NF_NETLINK_CONNTRACK_UPDATE, 
	   NF_NETLINK_CONNTRACK_DESTROY} },
	{ {"xml", "extended", "timestamp", "id" }, 4, 
	  { _O_XML, _O_EXT, _O_TMS, _O_ID },
	},
};

static int
do_parse_parameter(const char *str, size_t str_length, unsigned int *value, 
		   int parse_type)
{
	size_t i;
	int ret = 0;
	struct parse_parameter *p = &parse_array[parse_type];

	if (strncasecmp(str, "SRC_NAT", str_length) == 0) {
		fprintf(stderr, "WARNING: ignoring SRC_NAT, "
				"use --src-nat instead\n");
		return 1;
	}

	if (strncasecmp(str, "DST_NAT", str_length) == 0) {
		fprintf(stderr, "WARNING: ignoring DST_NAT, "
				"use --dst-nat instead\n");
		return 1;
	}

	for (i = 0; i < p->size; i++)
		if (strncasecmp(str, p->parameter[i], str_length) == 0) {
			*value |= p->value[i];
			ret = 1;
			break;
		}
	
	return ret;
}

static void
parse_parameter(const char *arg, unsigned int *status, int parse_type)
{
	const char *comma;

	while ((comma = strchr(arg, ',')) != NULL) {
		if (comma == arg 
		    || !do_parse_parameter(arg, comma-arg, status, parse_type))
			exit_error(PARAMETER_PROBLEM,"Bad parameter `%s'", arg);
		arg = comma+1;
	}

	if (strlen(arg) == 0
	    || !do_parse_parameter(arg, strlen(arg), status, parse_type))
		exit_error(PARAMETER_PROBLEM, "Bad parameter `%s'", arg);
}

static void
add_command(unsigned int *cmd, const int newcmd)
{
	if (*cmd)
		exit_error(PARAMETER_PROBLEM, "Invalid commands combination");
	*cmd |= newcmd;
}

static unsigned int
check_type(int argc, char *argv[])
{
	char *table = NULL;

	/* Nasty bug or feature in getopt_long ? 
	 * It seems that it behaves badly with optional arguments.
	 * Fortunately, I just stole the fix from iptables ;) */
	if (optarg)
		return 0;
	else if (optind < argc && argv[optind][0] != '-' 
			&& argv[optind][0] != '!')
		table = argv[optind++];
	
	if (!table)
		return 0;
		
	if (strncmp("expect", table, strlen(table)) == 0)
		return 1;
	else if (strncmp("conntrack", table, strlen(table)) == 0)
		return 0;
	else
		exit_error(PARAMETER_PROBLEM, "unknown type `%s'", table);

	return 0;
}

static void set_family(int *family, int new)
{
	if (*family == AF_UNSPEC)
		*family = new;
	else if (*family != new)
		exit_error(PARAMETER_PROBLEM, "mismatched address family");
}

struct addr_parse {
	struct in_addr addr;
	struct in6_addr addr6;
	unsigned int family;
};

static int
parse_inetaddr(const char *cp, struct addr_parse *parse)
{
	if (inet_aton(cp, &parse->addr))
		return AF_INET;
#ifdef HAVE_INET_PTON_IPV6
	else if (inet_pton(AF_INET6, cp, &parse->addr6) > 0)
		return AF_INET6;
#endif

	exit_error(PARAMETER_PROBLEM, "Invalid IP address `%s'", cp);
}

union ct_address {
	uint32_t v4;
	uint32_t v6[4];
};

static int
parse_addr(const char *cp, union ct_address *address)
{
	struct addr_parse parse;
	int ret;

	if ((ret = parse_inetaddr(cp, &parse)) == AF_INET)
		address->v4 = parse.addr.s_addr;
	else if (ret == AF_INET6)
		memcpy(address->v6, &parse.addr6, sizeof(parse.addr6));

	return ret;
}

/* Shamelessly stolen from libipt_DNAT ;). Ranges expected in network order. */
static void
nat_parse(char *arg, int portok, struct nf_conntrack *obj, int type)
{
	char *colon, *error;
	union ct_address parse;

	colon = strchr(arg, ':');

	if (colon) {
		uint16_t port;

		if (!portok)
			exit_error(PARAMETER_PROBLEM,
				   "Need TCP or UDP with port specification");

		port = (uint16_t)atoi(colon+1);
		if (port == 0)
			exit_error(PARAMETER_PROBLEM,
				   "Port `%s' not valid", colon+1);

		error = strchr(colon+1, ':');
		if (error)
			exit_error(PARAMETER_PROBLEM,
				   "Invalid port:port syntax");

		if (type == CT_OPT_SRC_NAT)
			nfct_set_attr_u16(obj, ATTR_SNAT_PORT, port);
		else if (type == CT_OPT_DST_NAT)
			nfct_set_attr_u16(obj, ATTR_DNAT_PORT, port);
	}

	if (parse_addr(arg, &parse) != AF_INET)
		return;

	if (type == CT_OPT_SRC_NAT)
		nfct_set_attr_u32(obj, ATTR_SNAT_IPV4, parse.v4);
	else if (type == CT_OPT_DST_NAT)
		nfct_set_attr_u32(obj, ATTR_DNAT_IPV4, parse.v4);
}

static const char usage_commands[] =
	"Commands:\n"
	"  -L [table] [options]\t\tList conntrack or expectation table\n"
	"  -G [table] parameters\t\tGet conntrack or expectation\n"
	"  -D [table] parameters\t\tDelete conntrack or expectation\n"
	"  -I [table] parameters\t\tCreate a conntrack or expectation\n"
	"  -U [table] parameters\t\tUpdate a conntrack\n"
	"  -E [table] [options]\t\tShow events\n"
	"  -F [table]\t\t\tFlush table\n"
	"  -C [table]\t\t\tShow counter\n"
	"  -S\t\t\t\tShow statistics\n";

static const char usage_tables[] =
	"Tables: conntrack, expect\n";

static const char usage_conntrack_parameters[] =
	"Conntrack parameters and options:\n"
	"  -n, --src-nat ip\t\t\tsource NAT ip\n"
	"  -g, --dst-nat ip\t\t\tdestination NAT ip\n"
	"  -m, --mark mark\t\t\tSet mark\n"
	"  -c, --secmark secmark\t\t\tSet selinux secmark\n"
	"  -e, --event-mask eventmask\t\tEvent mask, eg. NEW,DESTROY\n"
	"  -z, --zero \t\t\t\tZero counters while listing\n"
	"  -o, --output type[,...]\t\tOutput format, eg. xml\n";

static const char usage_expectation_parameters[] =
	"Expectation parameters and options:\n"
	"  --tuple-src ip\tSource address in expect tuple\n"
	"  --tuple-dst ip\tDestination address in expect tuple\n"
	"  --mask-src ip\t\tSource mask address\n"
	"  --mask-dst ip\t\tDestination mask address\n";

static const char usage_parameters[] =
	"Common parameters and options:\n"
	"  -s, --orig-src ip\t\tSource address from original direction\n"
	"  -d, --orig-dst ip\t\tDestination address from original direction\n"
	"  -r, --reply-src ip\t\tSource addres from reply direction\n"
	"  -q, --reply-dst ip\t\tDestination address from reply direction\n"
	"  -p, --protonum proto\t\tLayer 4 Protocol, eg. 'tcp'\n"
	"  -f, --family proto\t\tLayer 3 Protocol, eg. 'ipv6'\n"
	"  -t, --timeout timeout\t\tSet timeout\n"
	"  -u, --status status\t\tSet status, eg. ASSURED\n"
	"  -b, --buffer-size\t\tNetlink socket buffer size\n"
	;
  

static void
usage(char *prog)
{
	fprintf(stdout, "Command line interface for the connection "
			"tracking system. Version %s\n", VERSION);
	fprintf(stdout, "Usage: %s [commands] [options]\n", prog);

	fprintf(stdout, "\n%s", usage_commands);
	fprintf(stdout, "\n%s", usage_tables);
	fprintf(stdout, "\n%s", usage_conntrack_parameters);
	fprintf(stdout, "\n%s", usage_expectation_parameters);
	fprintf(stdout, "\n%s\n", usage_parameters);
}

static unsigned int output_mask;

static int 
filter_nat(const struct nf_conntrack *obj, const struct nf_conntrack *ct)
{
	uint32_t ip;

	if (options & CT_OPT_SRC_NAT) {
		if (!nfct_getobjopt(ct, NFCT_GOPT_IS_SNAT))
		  	return 1;

		if (nfct_attr_is_set(obj, ATTR_SNAT_IPV4)) {
			ip = nfct_get_attr_u32(obj, ATTR_SNAT_IPV4);
			if (ip != nfct_get_attr_u32(ct, ATTR_REPL_IPV4_DST))
				return 1;
		}
	}
	if (options & CT_OPT_DST_NAT) {
		if (!nfct_getobjopt(ct, NFCT_GOPT_IS_DNAT))
			return 1;

		if (nfct_attr_is_set(obj, ATTR_DNAT_IPV4)) {
			ip = nfct_get_attr_u32(obj, ATTR_DNAT_IPV4);
			if (ip != nfct_get_attr_u32(ct, ATTR_REPL_IPV4_SRC))
				return 1;
		}
	}

	return 0;
}

static int counter;
static int dump_xml_header_done = 1;

static void __attribute__((noreturn))
event_sighandler(int s)
{
	if (dump_xml_header_done == 0) {
		printf("</conntrack>\n");
		fflush(stdout);
	}

	fprintf(stderr, "%s v%s (conntrack-tools): ", PROGNAME, VERSION);
	fprintf(stderr, "%d flow events have been shown.\n", counter);
	nfct_close(cth);
	exit(0);
}

static int event_cb(enum nf_conntrack_msg_type type,
		    struct nf_conntrack *ct,
		    void *data)
{
	char buf[1024];
	struct nf_conntrack *obj = data;
	unsigned int op_type = NFCT_O_DEFAULT;
	unsigned int op_flags = 0;

	if (filter_nat(obj, ct))
		return NFCT_CB_CONTINUE;

	if (options & CT_COMPARISON &&
	    !nfct_cmp(obj, ct, NFCT_CMP_ALL | NFCT_CMP_MASK))
		return NFCT_CB_CONTINUE;

	if (output_mask & _O_XML) {
		op_type = NFCT_O_XML;
		if (dump_xml_header_done) {
			dump_xml_header_done = 0;
			printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			       "<conntrack>\n");
		}
	} 
	if (output_mask & _O_EXT)
		op_flags = NFCT_OF_SHOW_LAYER3;
	if (output_mask & _O_TMS) {
		if (!(output_mask & _O_XML)) {
			struct timeval tv;
			gettimeofday(&tv, NULL);
			printf("[%-8ld.%-6ld]\t", tv.tv_sec, tv.tv_usec);
		} else
			op_flags |= NFCT_OF_TIME;
	}
	if (output_mask & _O_ID)
		op_flags |= NFCT_OF_ID;

	nfct_snprintf(buf, sizeof(buf), ct, type, op_type, op_flags);

	printf("%s\n", buf);
	fflush(stdout);

	counter++;

	return NFCT_CB_CONTINUE;
}

static int dump_cb(enum nf_conntrack_msg_type type,
		   struct nf_conntrack *ct,
		   void *data)
{
	char buf[1024];
	struct nf_conntrack *obj = data;
	unsigned int op_type = NFCT_O_DEFAULT;
	unsigned int op_flags = 0;

	if (filter_nat(obj, ct))
		return NFCT_CB_CONTINUE;

	if (options & CT_COMPARISON &&
	    !nfct_cmp(obj, ct, NFCT_CMP_ALL | NFCT_CMP_MASK))
		return NFCT_CB_CONTINUE;

	if (output_mask & _O_XML) {
		op_type = NFCT_O_XML;
		if (dump_xml_header_done) {
			dump_xml_header_done = 0;
			printf("<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"
			       "<conntrack>\n");
		}
	}
	if (output_mask & _O_EXT)
		op_flags = NFCT_OF_SHOW_LAYER3;
	if (output_mask & _O_ID)
		op_flags |= NFCT_OF_ID;

	nfct_snprintf(buf, sizeof(buf), ct, NFCT_T_UNKNOWN, op_type, op_flags);
	printf("%s\n", buf);

	counter++;

	return NFCT_CB_CONTINUE;
}

static int delete_cb(enum nf_conntrack_msg_type type,
		     struct nf_conntrack *ct,
		     void *data)
{
	int res;
	char buf[1024];
	struct nf_conntrack *obj = data;
	unsigned int op_type = NFCT_O_DEFAULT;
	unsigned int op_flags = 0;

	if (filter_nat(obj, ct))
		return NFCT_CB_CONTINUE;

	if (options & CT_COMPARISON &&
	    !nfct_cmp(obj, ct, NFCT_CMP_ALL | NFCT_CMP_MASK))
		return NFCT_CB_CONTINUE;

	res = nfct_query(ith, NFCT_Q_DESTROY, ct);
	if (res < 0)
		exit_error(OTHER_PROBLEM,
			   "Operation failed: %s",
			   err2str(errno, CT_DELETE));

	if (output_mask & _O_XML)
		op_type = NFCT_O_XML;
	if (output_mask & _O_EXT)
		op_flags = NFCT_OF_SHOW_LAYER3;
	if (output_mask & _O_ID)
		op_flags |= NFCT_OF_ID;

	nfct_snprintf(buf, sizeof(buf), ct, NFCT_T_UNKNOWN, op_type, op_flags);
	printf("%s\n", buf);

	counter++;

	return NFCT_CB_CONTINUE;
}

static int print_cb(enum nf_conntrack_msg_type type,
		    struct nf_conntrack *ct,
		    void *data)
{
	char buf[1024];
	unsigned int op_type = NFCT_O_DEFAULT;
	unsigned int op_flags = 0;

	if (output_mask & _O_XML)
		op_type = NFCT_O_XML;
	if (output_mask & _O_EXT)
		op_flags = NFCT_OF_SHOW_LAYER3;
	if (output_mask & _O_ID)
		op_flags |= NFCT_OF_ID;

	nfct_snprintf(buf, sizeof(buf), ct, NFCT_T_UNKNOWN, op_type, op_flags);
	printf("%s\n", buf);

	return NFCT_CB_CONTINUE;
}

static int update_cb(enum nf_conntrack_msg_type type,
		     struct nf_conntrack *ct,
		     void *data)
{
	int res;
	struct nf_conntrack *obj = data;
	char __tmp[nfct_maxsize()];
	struct nf_conntrack *tmp = (struct nf_conntrack *) (void *)__tmp;

	memset(tmp, 0, sizeof(__tmp));

	if (filter_nat(obj, ct))
		return NFCT_CB_CONTINUE;

	if (nfct_attr_is_set(obj, ATTR_ID) && nfct_attr_is_set(ct, ATTR_ID) &&
	    nfct_get_attr_u32(obj, ATTR_ID) != nfct_get_attr_u32(ct, ATTR_ID))
	    	return NFCT_CB_CONTINUE;

	if (options & CT_OPT_TUPLE_ORIG && !nfct_cmp(obj, ct, NFCT_CMP_ORIG))
		return NFCT_CB_CONTINUE;
	if (options & CT_OPT_TUPLE_REPL && !nfct_cmp(obj, ct, NFCT_CMP_REPL))
		return NFCT_CB_CONTINUE;

	nfct_copy(tmp, ct, NFCT_CP_ORIG);
	nfct_copy(tmp, obj, NFCT_CP_META);

	res = nfct_query(ith, NFCT_Q_UPDATE, tmp);
	if (res < 0)
		exit_error(OTHER_PROBLEM,
			   "Operation failed: %s",
			   err2str(errno, CT_UPDATE));

	nfct_callback_register(ith, NFCT_T_ALL, print_cb, NULL);

	res = nfct_query(ith, NFCT_Q_GET, tmp);
	if (res < 0) {
		/* the entry has vanish in middle of the update */
		if (errno == ENOENT) {
			nfct_callback_unregister(ith);
			return NFCT_CB_CONTINUE;
		}

		exit_error(OTHER_PROBLEM,
			   "Operation failed: %s",
			   err2str(errno, CT_UPDATE));
	}

	nfct_callback_unregister(ith);

	counter++;

	return NFCT_CB_CONTINUE;
}

static int dump_exp_cb(enum nf_conntrack_msg_type type,
		      struct nf_expect *exp,
		      void *data)
{
	char buf[1024];

	nfexp_snprintf(buf,sizeof(buf), exp, NFCT_T_UNKNOWN, NFCT_O_DEFAULT, 0);
	printf("%s\n", buf);
	counter++;

	return NFCT_CB_CONTINUE;
}

static int count_exp_cb(enum nf_conntrack_msg_type type,
			struct nf_expect *exp,
			void *data)
{
	counter++;
	return NFCT_CB_CONTINUE;
}

#ifndef CT_STATS_PROC
#define CT_STATS_PROC "/proc/net/stat/nf_conntrack"
#endif

/* As of 2.6.29, we have 16 entries, this is enough */
#ifndef CT_STATS_ENTRIES_MAX
#define CT_STATS_ENTRIES_MAX 64
#endif

/* maximum string length currently is 13 characters */
#ifndef CT_STATS_STRING_MAX
#define CT_STATS_STRING_MAX 64
#endif

static int display_proc_conntrack_stats(void)
{
	int ret = 0;
	FILE *fd;
	char buf[4096], *token, *nl;
	char output[CT_STATS_ENTRIES_MAX][CT_STATS_STRING_MAX];
	unsigned int value[CT_STATS_ENTRIES_MAX], i, max;

	fd = fopen(CT_STATS_PROC, "r");
	if (fd == NULL)
		return -1;

	if (fgets(buf, sizeof(buf), fd) == NULL) {
		ret = -1;
		goto out_err;
	}

	/* trim off trailing \n */
	nl = strchr(buf, '\n');
	if (nl != NULL) {
		*nl = '\0';
		nl = strchr(buf, '\n');
	}
	token = strtok(buf, " ");
	for (i=0; token != NULL && i<CT_STATS_ENTRIES_MAX; i++) {
		strncpy(output[i], token, CT_STATS_STRING_MAX);
		output[i][CT_STATS_STRING_MAX-1]='\0';
		token = strtok(NULL, " ");
	}
	max = i;

	if (fgets(buf, sizeof(buf), fd) == NULL) {
		ret = -1;
		goto out_err;
	}

	nl = strchr(buf, '\n');
	while (nl != NULL) {
		*nl = '\0';
		nl = strchr(buf, '\n');
	}
	token = strtok(buf, " ");
	for (i=0; token != NULL && i<CT_STATS_ENTRIES_MAX; i++) { 
		value[i] = (unsigned int) strtol(token, (char**) NULL, 16);
		token = strtok(NULL, " ");
	}

	for (i=0; i<max; i++)
		printf("%-10s\t\t%-8u\n", output[i], value[i]);

out_err:
	fclose(fd);
	return ret;
}

static struct ctproto_handler *h;

static const int cmd2type[][2] = {
	['L']	= { CT_LIST, 	EXP_LIST },
	['I']	= { CT_CREATE,	EXP_CREATE },
	['D']	= { CT_DELETE,	EXP_DELETE },
	['G']	= { CT_GET,	EXP_GET },
	['F']	= { CT_FLUSH,	EXP_FLUSH },
	['E']	= { CT_EVENT,	EXP_EVENT },
	['V']	= { CT_VERSION,	CT_VERSION },
	['h']	= { CT_HELP,	CT_HELP },
	['C']	= { CT_COUNT,	EXP_COUNT },
};

static const int opt2type[] = {
	['s']	= CT_OPT_ORIG_SRC,
	['d']	= CT_OPT_ORIG_DST,
	['r']	= CT_OPT_REPL_SRC,
	['q']	= CT_OPT_REPL_DST,
	['{']	= CT_OPT_MASK_SRC,
	['}']	= CT_OPT_MASK_DST,
	['[']	= CT_OPT_EXP_SRC,
	[']']	= CT_OPT_EXP_DST,
	['n']	= CT_OPT_SRC_NAT,
	['g']	= CT_OPT_DST_NAT,
	['m']	= CT_OPT_MARK,
	['c']	= CT_OPT_SECMARK,
	['i']	= CT_OPT_ID,
};

static const int opt2family_attr[][2] = {
	['s']	= { ATTR_ORIG_IPV4_SRC,	ATTR_ORIG_IPV6_SRC },
	['d']	= { ATTR_ORIG_IPV4_DST,	ATTR_ORIG_IPV6_DST },
	['r']	= { ATTR_REPL_IPV4_SRC, ATTR_REPL_IPV6_SRC },
	['q']	= { ATTR_REPL_IPV4_DST, ATTR_REPL_IPV6_DST },
	['{']	= { ATTR_ORIG_IPV4_SRC,	ATTR_ORIG_IPV6_SRC },
	['}']	= { ATTR_ORIG_IPV4_DST,	ATTR_ORIG_IPV6_DST },
	['[']	= { ATTR_ORIG_IPV4_SRC, ATTR_ORIG_IPV6_SRC },
	[']']	= { ATTR_ORIG_IPV4_DST, ATTR_ORIG_IPV6_DST },
};

static const int opt2attr[] = {
	['s']	= ATTR_ORIG_L3PROTO,
	['d']	= ATTR_ORIG_L3PROTO,
	['r']	= ATTR_REPL_L3PROTO,
	['q']	= ATTR_REPL_L3PROTO,
	['m']	= ATTR_MARK,
	['c']	= ATTR_SECMARK,
	['i']	= ATTR_ID,
};

static char exit_msg[NUMBER_OF_CMD][64] = {
	[CT_LIST_BIT] 		= "%d flow entries have been shown.\n",
	[CT_CREATE_BIT]		= "%d flow entries have been created.\n",
	[CT_UPDATE_BIT]		= "%d flow entries have been updated.\n",
	[CT_DELETE_BIT]		= "%d flow entries have been deleted.\n",
	[CT_GET_BIT] 		= "%d flow entries have been shown.\n",
	[CT_EVENT_BIT]		= "%d flow events have been shown.\n",
	[EXP_LIST_BIT]		= "%d expectations have been shown.\n",
	[EXP_DELETE_BIT]	= "%d expectations have been shown.\n",
};

int main(int argc, char *argv[])
{
	int c, cmd;
	unsigned int type = 0, event_mask = 0, l4flags = 0, status = 0;
	int res = 0, partial;
	size_t socketbuffersize = 0;
	int family = AF_UNSPEC;
	char __obj[nfct_maxsize()];
	char __exptuple[nfct_maxsize()];
	char __mask[nfct_maxsize()];
	struct nf_conntrack *obj = (struct nf_conntrack *)(void*) __obj;
	struct nf_conntrack *exptuple = 
		(struct nf_conntrack *)(void*) __exptuple;
	struct nf_conntrack *mask = (struct nf_conntrack *)(void*) __mask;
	char __exp[nfexp_maxsize()];
	struct nf_expect *exp = (struct nf_expect *)(void*) __exp;
	int l3protonum, protonum = 0;
	union ct_address ad;
	unsigned int command = 0;

	memset(__obj, 0, sizeof(__obj));
	memset(__exptuple, 0, sizeof(__exptuple));
	memset(__mask, 0, sizeof(__mask));
	memset(__exp, 0, sizeof(__exp));

	register_tcp();
	register_udp();
	register_udplite();
	register_sctp();
	register_dccp();
	register_icmp();
	register_icmpv6();
	register_gre();
	register_unknown();

	/* disable explicit missing arguments error output from getopt_long */
	opterr = 0;

	while ((c = getopt_long(argc, argv, "L::I::U::D::G::E::F::hVs:d:r:q:"
					    "p:t:u:e:a:z[:]:{:}:m:i:f:o:n::"
					    "g::c:b:C::S", 
					    opts, NULL)) != -1) {
	switch(c) {
		/* commands */
		case 'L':
		case 'I':
		case 'D':
		case 'G':
		case 'F':
		case 'E':
		case 'V':
		case 'h':
		case 'C':
			type = check_type(argc, argv);
			add_command(&command, cmd2type[c][type]);
			break;
		case 'U':
			type = check_type(argc, argv);
			if (type == 0)
				add_command(&command, CT_UPDATE);
			else
				exit_error(PARAMETER_PROBLEM, 
					   "Can't update expectations");
			break;
		case 'S':
			add_command(&command, X_STATS);
			break;
		/* options */
		case 's':
		case 'd':
		case 'r':
		case 'q':
			options |= opt2type[c];

			l3protonum = parse_addr(optarg, &ad);
			set_family(&family, l3protonum);
			if (l3protonum == AF_INET) {
				nfct_set_attr_u32(obj,
						  opt2family_attr[c][0],
						  ad.v4);
			} else if (l3protonum == AF_INET6) {
				nfct_set_attr(obj,
					      opt2family_attr[c][1],
					      &ad.v6);
			}
			nfct_set_attr_u8(obj, opt2attr[c], l3protonum);
			break;
		case '{':
		case '}':
		case '[':
		case ']':
			options |= opt2type[c];
			l3protonum = parse_addr(optarg, &ad);
			set_family(&family, l3protonum);
			if (l3protonum == AF_INET) {
				nfct_set_attr_u32(mask, 
						  opt2family_attr[c][0],
						  ad.v4);
			} else if (l3protonum == AF_INET6) {
				nfct_set_attr(mask,
					      opt2family_attr[c][1],
					      &ad.v6);
			}
			nfct_set_attr_u8(mask, ATTR_ORIG_L3PROTO, l3protonum);
			break;
		case 'p':
			options |= CT_OPT_PROTO;
			h = findproto(optarg, &protonum);
			if (!h)
				exit_error(PARAMETER_PROBLEM,
					   "`%s' unsupported protocol",
					   optarg);

			opts = merge_options(opts, h->opts, &h->option_offset);
			if (opts == NULL)
				exit_error(OTHER_PROBLEM, "out of memory");

			nfct_set_attr_u8(obj, ATTR_L4PROTO, protonum);
			break;
		case 't':
			options |= CT_OPT_TIMEOUT;
			nfct_set_attr_u32(obj, ATTR_TIMEOUT, atol(optarg));
			nfexp_set_attr_u32(exp, ATTR_EXP_TIMEOUT, atol(optarg));
			break;
		case 'u':
			options |= CT_OPT_STATUS;
			parse_parameter(optarg, &status, PARSE_STATUS);
			nfct_set_attr_u32(obj, ATTR_STATUS, status);
			break;
		case 'e':
			options |= CT_OPT_EVENT_MASK;
			parse_parameter(optarg, &event_mask, PARSE_EVENT);
			break;
		case 'o':
			options |= CT_OPT_OUTPUT;
			parse_parameter(optarg, &output_mask, PARSE_OUTPUT);
			break;
		case 'z':
			options |= CT_OPT_ZERO;
			break;
		case 'n':
		case 'g': {
			char *tmp = NULL;

			options |= opt2type[c];

			if (optarg)
				continue;
			else if (optind < argc && argv[optind][0] != '-'
				 && argv[optind][0] != '!')
				tmp = argv[optind++];

			if (tmp == NULL)
				continue;

			set_family(&family, AF_INET);
			nat_parse(tmp, 1, obj, opt2type[c]);
			break;
		}
		case 'i':
		case 'm':
		case 'c':
			options |= opt2type[c];
			nfct_set_attr_u32(obj,
					  opt2attr[c],
					  strtoul(optarg, NULL, 0));
			break;
		case 'a':
			fprintf(stderr, "WARNING: ignoring -%c, "
					"deprecated option.\n", c);
			break;
		case 'f':
			options |= CT_OPT_FAMILY;
			if (strncmp(optarg, "ipv4", strlen("ipv4")) == 0)
				set_family(&family, AF_INET);
			else if (strncmp(optarg, "ipv6", strlen("ipv6")) == 0)
				set_family(&family, AF_INET6);
			else
				exit_error(PARAMETER_PROBLEM,
					   "`%s' unsupported protocol",
					   optarg);
			break;
		case 'b':
			socketbuffersize = atol(optarg);
			options |= CT_OPT_BUFFERSIZE;
			break;
		case '?':
			if (optopt)
				exit_error(PARAMETER_PROBLEM,
					   "option `%s' requires an "
					   "argument", argv[optind-1]);
			else
				exit_error(PARAMETER_PROBLEM,
					   "unknown option `%s'", 
					   argv[optind-1]);
			break;
		default:
			if (h && h->parse_opts 
			    &&!h->parse_opts(c - h->option_offset, obj,
			    		     exptuple, mask, &l4flags))
				exit_error(PARAMETER_PROBLEM, "parse error");
			break;
		}
	}

	/* default family */
	if (family == AF_UNSPEC)
		family = AF_INET;

	cmd = bit2cmd(command);
	res = generic_opt_check(options, NUMBER_OF_OPT,
				commands_v_options[cmd], optflags,
				addr_valid_flags, ADDR_VALID_FLAGS_MAX,
				&partial);
	if (!res) {
		switch(partial) {
		case -1:
		case 0:
			exit_error(PARAMETER_PROBLEM, "you have to specify "
						      "`--src' and `--dst'");
			break;
		case 1:
			exit_error(PARAMETER_PROBLEM, "you have to specify "
						      "`--reply-src' and "
						      "`--reply-dst'");
			break;
		}
	}
	if (!(command & CT_HELP) && h && h->final_check)
		h->final_check(l4flags, cmd, obj);

	switch(command) {

	case CT_LIST:
		cth = nfct_open(CONNTRACK, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		if (options & CT_COMPARISON && 
		    options & CT_OPT_ZERO)
			exit_error(PARAMETER_PROBLEM, "Can't use -z with "
						      "filtering parameters");

		nfct_callback_register(cth, NFCT_T_ALL, dump_cb, obj);

		if (options & CT_OPT_ZERO)
			res = nfct_query(cth, NFCT_Q_DUMP_RESET, &family);
		else
			res = nfct_query(cth, NFCT_Q_DUMP, &family);

		if (dump_xml_header_done == 0) {
			printf("</conntrack>\n");
			fflush(stdout);
		}

		nfct_close(cth);
		break;

	case EXP_LIST:
		cth = nfct_open(EXPECT, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		nfexp_callback_register(cth, NFCT_T_ALL, dump_exp_cb, NULL);
		res = nfexp_query(cth, NFCT_Q_DUMP, &family);
		nfct_close(cth);
		break;
			
	case CT_CREATE:
		if ((options & CT_OPT_ORIG) && !(options & CT_OPT_REPL))
		    	nfct_setobjopt(obj, NFCT_SOPT_SETUP_REPLY);
		else if (!(options & CT_OPT_ORIG) && (options & CT_OPT_REPL))
			nfct_setobjopt(obj, NFCT_SOPT_SETUP_ORIGINAL);

		cth = nfct_open(CONNTRACK, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		res = nfct_query(cth, NFCT_Q_CREATE, obj);
		if (res != -1)
			counter++;
		nfct_close(cth);
		break;

	case EXP_CREATE:
		nfexp_set_attr(exp, ATTR_EXP_MASTER, obj);
		nfexp_set_attr(exp, ATTR_EXP_EXPECTED, exptuple);
		nfexp_set_attr(exp, ATTR_EXP_MASK, mask);

		cth = nfct_open(EXPECT, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		res = nfexp_query(cth, NFCT_Q_CREATE, exp);
		nfct_close(cth);
		break;

	case CT_UPDATE:
		cth = nfct_open(CONNTRACK, 0);
		/* internal handler for delete_cb, otherwise we hit EILSEQ */
		ith = nfct_open(CONNTRACK, 0);
		if (!cth || !ith)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		nfct_callback_register(cth, NFCT_T_ALL, update_cb, obj);

		res = nfct_query(cth, NFCT_Q_DUMP, &family);
		nfct_close(ith);
		nfct_close(cth);
		break;
		
	case CT_DELETE:
		cth = nfct_open(CONNTRACK, 0);
		ith = nfct_open(CONNTRACK, 0);
		if (!cth || !ith)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		nfct_callback_register(cth, NFCT_T_ALL, delete_cb, obj);

		res = nfct_query(cth, NFCT_Q_DUMP, &family);
		nfct_close(ith);
		nfct_close(cth);
		break;

	case EXP_DELETE:
		nfexp_set_attr(exp, ATTR_EXP_EXPECTED, obj);

		cth = nfct_open(EXPECT, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		res = nfexp_query(cth, NFCT_Q_DESTROY, exp);
		nfct_close(cth);
		break;

	case CT_GET:
		cth = nfct_open(CONNTRACK, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		nfct_callback_register(cth, NFCT_T_ALL, dump_cb, obj);
		res = nfct_query(cth, NFCT_Q_GET, obj);
		nfct_close(cth);
		break;

	case EXP_GET:
		nfexp_set_attr(exp, ATTR_EXP_MASTER, obj);

		cth = nfct_open(EXPECT, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		nfexp_callback_register(cth, NFCT_T_ALL, dump_exp_cb, NULL);
		res = nfexp_query(cth, NFCT_Q_GET, exp);
		nfct_close(cth);
		break;

	case CT_FLUSH:
		cth = nfct_open(CONNTRACK, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");
		res = nfct_query(cth, NFCT_Q_FLUSH, &family);
		nfct_close(cth);
		fprintf(stderr, "%s v%s (conntrack-tools): ",PROGNAME,VERSION);
		fprintf(stderr,"connection tracking table has been emptied.\n");
		break;

	case EXP_FLUSH:
		cth = nfct_open(EXPECT, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");
		res = nfexp_query(cth, NFCT_Q_FLUSH, &family);
		nfct_close(cth);
		break;
		
	case CT_EVENT:
		if (options & CT_OPT_EVENT_MASK)
			cth = nfct_open(CONNTRACK,
					event_mask & NFCT_ALL_CT_GROUPS);
		else
			cth = nfct_open(CONNTRACK, NFCT_ALL_CT_GROUPS);

		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		if (options & CT_OPT_BUFFERSIZE) {
			size_t ret;
			ret = nfnl_rcvbufsiz(nfct_nfnlh(cth), socketbuffersize);
			fprintf(stderr, "NOTICE: Netlink socket buffer size "
					"has been set to %zu bytes.\n", ret);
		}
		signal(SIGINT, event_sighandler);
		signal(SIGTERM, event_sighandler);
		nfct_callback_register(cth, NFCT_T_ALL, event_cb, obj);
		res = nfct_catch(cth);
		if (res == -1) {
			if (errno == ENOBUFS) {
				fprintf(stderr, 
					"WARNING: We have hit ENOBUFS! We "
					"are losing events.\nThis message "
					"means that the current netlink "
					"socket buffer size is too small.\n"
					"Please, check --buffer-size in "
					"conntrack(8) manpage.\n");
			}
		}
		nfct_close(cth);
		break;

	case EXP_EVENT:
		cth = nfct_open(EXPECT, NF_NETLINK_CONNTRACK_EXP_NEW);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");
		signal(SIGINT, event_sighandler);
		signal(SIGTERM, event_sighandler);
		nfexp_callback_register(cth, NFCT_T_ALL, dump_exp_cb, NULL);
		res = nfexp_catch(cth);
		nfct_close(cth);
		break;
	case CT_COUNT: {
#define NF_CONNTRACK_COUNT_PROC "/proc/sys/net/netfilter/nf_conntrack_count"
		FILE *fd;
		int count;
		fd = fopen(NF_CONNTRACK_COUNT_PROC, "r");
		if (fd == NULL) {
			exit_error(OTHER_PROBLEM, "Can't open %s",
				   NF_CONNTRACK_COUNT_PROC);
		}
		if (fscanf(fd, "%d", &count) != 1) {
			exit_error(OTHER_PROBLEM, "Can't read %s",
				   NF_CONNTRACK_COUNT_PROC);
		}
		fclose(fd);
		printf("%d\n", count);
		break;
	}
	case EXP_COUNT:
		cth = nfct_open(EXPECT, 0);
		if (!cth)
			exit_error(OTHER_PROBLEM, "Can't open handler");

		nfexp_callback_register(cth, NFCT_T_ALL, count_exp_cb, NULL);
		res = nfexp_query(cth, NFCT_Q_DUMP, &family);
		nfct_close(cth);
		printf("%d\n", counter);
		break;
	case X_STATS:
		if (display_proc_conntrack_stats() < 0)
			exit_error(OTHER_PROBLEM, "Can't open /proc interface");
		break;
	case CT_VERSION:
		printf("%s v%s (conntrack-tools)\n", PROGNAME, VERSION);
		break;
	case CT_HELP:
		usage(argv[0]);
		if (options & CT_OPT_PROTO)
			extension_help(h, protonum);
		break;
	default:
		usage(argv[0]);
		break;
	}

	if (res < 0)
		exit_error(OTHER_PROBLEM, "Operation failed: %s",
			   err2str(errno, command));

	free_options();

	if (command && exit_msg[cmd][0]) {
		fprintf(stderr, "%s v%s (conntrack-tools): ",PROGNAME,VERSION);
		fprintf(stderr, exit_msg[cmd], counter);
		if (counter == 0 && !(command & (CT_LIST | EXP_LIST)))
			return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
