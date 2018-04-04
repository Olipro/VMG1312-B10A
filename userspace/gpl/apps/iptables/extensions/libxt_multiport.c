/* Shared library add-on to iptables to add multiple TCP port support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <xtables.h>
#include <libiptc/libiptc.h>
#include <libiptc/libip6tc.h>
/* To ensure that iptables compiles with an old kernel */
#include "../include/linux/netfilter/xt_multiport.h"

/* Function which prints out usage message. */
static void multiport_help(void)
{
	printf(
"multiport v%s options:\n"
" --source-ports port[,port,port...]\n"
" --sports ...\n"
"				match source port(s)\n"
" --destination-ports port[,port,port...]\n"
" --dports ...\n"
"				match destination port(s)\n"
" --ports port[,port,port]\n"
"				match both source and destination port(s)\n"
" NOTE: this kernel does not support port ranges in multiport.\n",
IPTABLES_VERSION);
}

static void multiport_help_v1(void)
{
	printf(
"multiport v%s options:\n"
" --source-ports [!] port[,port:port,port...]\n"
" --sports ...\n"
"				match source port(s)\n"
" --destination-ports [!] port[,port:port,port...]\n"
" --dports ...\n"
"				match destination port(s)\n"
" --ports [!] port[,port:port,port]\n"
"				match both source and destination port(s)\n",
IPTABLES_VERSION);
}

static const struct option multiport_opts[] = {
	{ "source-ports", 1, NULL, '1' },
	{ "sports", 1, NULL, '1' }, /* synonym */
	{ "destination-ports", 1, NULL, '2' },
	{ "dports", 1, NULL, '2' }, /* synonym */
	{ "ports", 1, NULL, '3' },
	{ }
};

static char *
proto_to_name(u_int8_t proto)
{
	switch (proto) {
	case IPPROTO_TCP:
		return "tcp";
	case IPPROTO_UDP:
		return "udp";
	case IPPROTO_UDPLITE:
		return "udplite";
	case IPPROTO_SCTP:
		return "sctp";
	case IPPROTO_DCCP:
		return "dccp";
	default:
		return NULL;
	}
}

static unsigned int
parse_multi_ports(const char *portstring, u_int16_t *ports, const char *proto)
{
	char *buffer, *cp, *next;
	unsigned int i;

	buffer = strdup(portstring);
	if (!buffer) exit_error(OTHER_PROBLEM, "strdup failed");

	for (cp=buffer, i=0; cp && i<XT_MULTI_PORTS; cp=next,i++)
	{
		next=strchr(cp, ',');
		if (next) *next++='\0';
		ports[i] = parse_port(cp, proto);
	}
	if (cp) exit_error(PARAMETER_PROBLEM, "too many ports specified");
	free(buffer);
	return i;
}

static void
parse_multi_ports_v1(const char *portstring, 
		     struct xt_multiport_v1 *multiinfo,
		     const char *proto)
{
	char *buffer, *cp, *next, *range;
	unsigned int i;
	u_int16_t m;

	buffer = strdup(portstring);
	if (!buffer) exit_error(OTHER_PROBLEM, "strdup failed");

	for (i=0; i<XT_MULTI_PORTS; i++)
		multiinfo->pflags[i] = 0;
 
	for (cp=buffer, i=0; cp && i<XT_MULTI_PORTS; cp=next, i++) {
		next=strchr(cp, ',');
 		if (next) *next++='\0';
		range = strchr(cp, ':');
		if (range) {
			if (i == XT_MULTI_PORTS-1)
				exit_error(PARAMETER_PROBLEM,
					   "too many ports specified");
			*range++ = '\0';
		}
		multiinfo->ports[i] = parse_port(cp, proto);
		if (range) {
			multiinfo->pflags[i] = 1;
			multiinfo->ports[++i] = parse_port(range, proto);
			if (multiinfo->ports[i-1] >= multiinfo->ports[i])
				exit_error(PARAMETER_PROBLEM,
					   "invalid portrange specified");
			m <<= 1;
		}
 	}
	multiinfo->count = i;
 	if (cp) exit_error(PARAMETER_PROBLEM, "too many ports specified");
 	free(buffer);
}

static const char *
check_proto(u_int16_t pnum, u_int8_t invflags)
{
	char *proto;

	if (invflags & XT_INV_PROTO)
		exit_error(PARAMETER_PROBLEM,
			   "multiport only works with TCP, UDP, UDPLITE, SCTP and DCCP");

	if ((proto = proto_to_name(pnum)) != NULL)
		return proto;
	else if (!pnum)
		exit_error(PARAMETER_PROBLEM,
			   "multiport needs `-p tcp', `-p udp', `-p udplite', "
			   "`-p sctp' or `-p dccp'");
	else
		exit_error(PARAMETER_PROBLEM,
			   "multiport only works with TCP, UDP, UDPLITE, SCTP and DCCP");
}

/* Function which parses command options; returns true if it
   ate an option */
static int
__multiport_parse(int c, char **argv, int invert, unsigned int *flags,
                  struct xt_entry_match **match, u_int16_t pnum,
                  u_int8_t invflags)
{
	const char *proto;
	struct xt_multiport *multiinfo
		= (struct xt_multiport *)(*match)->data;

	switch (c) {
	case '1':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(pnum, invflags);
		multiinfo->count = parse_multi_ports(argv[optind-1],
						     multiinfo->ports, proto);
		multiinfo->flags = XT_MULTIPORT_SOURCE;
		break;

	case '2':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(pnum, invflags);
		multiinfo->count = parse_multi_ports(argv[optind-1],
						     multiinfo->ports, proto);
		multiinfo->flags = XT_MULTIPORT_DESTINATION;
		break;

	case '3':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(pnum, invflags);
		multiinfo->count = parse_multi_ports(argv[optind-1],
						     multiinfo->ports, proto);
		multiinfo->flags = XT_MULTIPORT_EITHER;
		break;

	default:
		return 0;
	}

	if (invert)
		exit_error(PARAMETER_PROBLEM,
			   "multiport does not support invert");

	if (*flags)
		exit_error(PARAMETER_PROBLEM,
			   "multiport can only have one option");
	*flags = 1;
	return 1;
}

static int
multiport_parse(int c, char **argv, int invert, unsigned int *flags,
                const void *e, struct xt_entry_match **match)
{
	const struct ipt_entry *entry = e;
	return __multiport_parse(c, argv, invert, flags, match,
	       entry->ip.proto, entry->ip.invflags);
}

static int
multiport_parse6(int c, char **argv, int invert, unsigned int *flags,
                 const void *e, struct xt_entry_match **match)
{
	const struct ip6t_entry *entry = (const struct ip6t_entry *)e;
	return __multiport_parse(c, argv, invert, flags, match,
	       entry->ipv6.proto, entry->ipv6.invflags);
}

static int
__multiport_parse_v1(int c, char **argv, int invert, unsigned int *flags,
                     struct xt_entry_match **match, u_int16_t pnum,
                     u_int8_t invflags)
{
	const char *proto;
	struct xt_multiport_v1 *multiinfo
		= (struct xt_multiport_v1 *)(*match)->data;

	switch (c) {
	case '1':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(pnum, invflags);
		parse_multi_ports_v1(argv[optind-1], multiinfo, proto);
		multiinfo->flags = XT_MULTIPORT_SOURCE;
		break;

	case '2':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(pnum, invflags);
		parse_multi_ports_v1(argv[optind-1], multiinfo, proto);
		multiinfo->flags = XT_MULTIPORT_DESTINATION;
		break;

	case '3':
		check_inverse(argv[optind-1], &invert, &optind, 0);
		proto = check_proto(pnum, invflags);
		parse_multi_ports_v1(argv[optind-1], multiinfo, proto);
		multiinfo->flags = XT_MULTIPORT_EITHER;
		break;

	default:
		return 0;
	}

	if (invert)
		multiinfo->invert = 1;

	if (*flags)
		exit_error(PARAMETER_PROBLEM,
			   "multiport can only have one option");
	*flags = 1;
	return 1;
}

static int
multiport_parse_v1(int c, char **argv, int invert, unsigned int *flags,
                   const void *e, struct xt_entry_match **match)
{
	const struct ipt_entry *entry = e;
	return __multiport_parse_v1(c, argv, invert, flags, match,
	       entry->ip.proto, entry->ip.invflags);
}

static int
multiport_parse6_v1(int c, char **argv, int invert, unsigned int *flags,
                    const void *e, struct xt_entry_match **match)
{
	const struct ip6t_entry *entry = (const struct ip6t_entry *)e;
	return __multiport_parse_v1(c, argv, invert, flags, match,
	       entry->ipv6.proto, entry->ipv6.invflags);
}

/* Final check; must specify something. */
static void multiport_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM, "multiport expection an option");
}

static char *
port_to_service(int port, u_int8_t proto)
{
	struct servent *service;

	if ((service = getservbyport(htons(port), proto_to_name(proto))))
		return service->s_name;

	return NULL;
}

static void
print_port(u_int16_t port, u_int8_t protocol, int numeric)
{
	char *service;

	if (numeric || (service = port_to_service(port, protocol)) == NULL)
		printf("%u", port);
	else
		printf("%s", service);
}

/* Prints out the matchinfo. */
static void
__multiport_print(const struct xt_entry_match *match, int numeric,
                  u_int16_t proto)
{
	const struct xt_multiport *multiinfo
		= (const struct xt_multiport *)match->data;
	unsigned int i;

	printf("multiport ");

	switch (multiinfo->flags) {
	case XT_MULTIPORT_SOURCE:
		printf("sports ");
		break;

	case XT_MULTIPORT_DESTINATION:
		printf("dports ");
		break;

	case XT_MULTIPORT_EITHER:
		printf("ports ");
		break;

	default:
		printf("ERROR ");
		break;
	}

	for (i=0; i < multiinfo->count; i++) {
		printf("%s", i ? "," : "");
		print_port(multiinfo->ports[i], proto, numeric);
	}
	printf(" ");
}

static void multiport_print(const void *ip_void,
                            const struct xt_entry_match *match, int numeric)
{
	const struct ipt_ip *ip = ip_void;
	__multiport_print(match, numeric, ip->proto);
}

static void multiport_print6(const void *ip_void,
                             const struct xt_entry_match *match, int numeric)
{
	const struct ip6t_ip6 *ip = (const struct ip6t_ip6 *)ip_void;
	__multiport_print(match, numeric, ip->proto);
}

static void __multiport_print_v1(const struct xt_entry_match *match,
                                 int numeric, u_int16_t proto)
{
	const struct xt_multiport_v1 *multiinfo
		= (const struct xt_multiport_v1 *)match->data;
	unsigned int i;

	printf("multiport ");

	switch (multiinfo->flags) {
	case XT_MULTIPORT_SOURCE:
		printf("sports ");
		break;

	case XT_MULTIPORT_DESTINATION:
		printf("dports ");
		break;

	case XT_MULTIPORT_EITHER:
		printf("ports ");
		break;

	default:
		printf("ERROR ");
		break;
	}

	if (multiinfo->invert)
		printf("! ");

	for (i=0; i < multiinfo->count; i++) {
		printf("%s", i ? "," : "");
		print_port(multiinfo->ports[i], proto, numeric);
		if (multiinfo->pflags[i]) {
			printf(":");
			print_port(multiinfo->ports[++i], proto, numeric);
		}
	}
	printf(" ");
}

static void multiport_print_v1(const void *ip_void,
                               const struct xt_entry_match *match, int numeric)
{
	const struct ipt_ip *ip = ip_void;
	__multiport_print_v1(match, numeric, ip->proto);
}

static void multiport_print6_v1(const void *ip_void,
                                const struct xt_entry_match *match, int numeric)
{
	const struct ip6t_ip6 *ip = (const struct ip6t_ip6 *)ip_void;
	__multiport_print_v1(match, numeric, ip->proto);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void __multiport_save(const struct xt_entry_match *match,
                             u_int16_t proto)
{
	const struct xt_multiport *multiinfo
		= (const struct xt_multiport *)match->data;
	unsigned int i;

	switch (multiinfo->flags) {
	case XT_MULTIPORT_SOURCE:
		printf("--sports ");
		break;

	case XT_MULTIPORT_DESTINATION:
		printf("--dports ");
		break;

	case XT_MULTIPORT_EITHER:
		printf("--ports ");
		break;
	}

	for (i=0; i < multiinfo->count; i++) {
		printf("%s", i ? "," : "");
		print_port(multiinfo->ports[i], proto, 1);
	}
	printf(" ");
}

static void multiport_save(const void *ip_void,
                           const struct xt_entry_match *match)
{
	const struct ipt_ip *ip = ip_void;
	__multiport_save(match, ip->proto);
}

static void multiport_save6(const void *ip_void,
                            const struct xt_entry_match *match)
{
	const struct ip6t_ip6 *ip = (const struct ip6t_ip6 *)ip_void;
	__multiport_save(match, ip->proto);
}

static void __multiport_save_v1(const struct xt_entry_match *match,
                                u_int16_t proto)
{
	const struct xt_multiport_v1 *multiinfo
		= (const struct xt_multiport_v1 *)match->data;
	unsigned int i;

	switch (multiinfo->flags) {
	case XT_MULTIPORT_SOURCE:
		printf("--sports ");
		break;

	case XT_MULTIPORT_DESTINATION:
		printf("--dports ");
		break;

	case XT_MULTIPORT_EITHER:
		printf("--ports ");
		break;
	}

	if (multiinfo->invert)
		printf("! ");

	for (i=0; i < multiinfo->count; i++) {
		printf("%s", i ? "," : "");
		print_port(multiinfo->ports[i], proto, 1);
		if (multiinfo->pflags[i]) {
			printf(":");
			print_port(multiinfo->ports[++i], proto, 1);
		}
	}
	printf(" ");
}

static void multiport_save_v1(const void *ip_void,
                              const struct xt_entry_match *match)
{
	const struct ipt_ip *ip = ip_void;
	__multiport_save_v1(match, ip->proto);
}

static void multiport_save6_v1(const void *ip_void,
                               const struct xt_entry_match *match)
{
	const struct ip6t_ip6 *ip = (const struct ip6t_ip6 *)ip_void;
	__multiport_save_v1(match, ip->proto);
}

static struct xtables_match multiport_match = {
	.family		= AF_INET,
	.name		= "multiport",
	.revision	= 0,
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_multiport)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_multiport)),
	.help		= multiport_help,
	.parse		= multiport_parse,
	.final_check	= multiport_check,
	.print		= multiport_print,
	.save		= multiport_save,
	.extra_opts	= multiport_opts,
};

static struct xtables_match multiport_match6 = {
	.family		= AF_INET6,
	.name		= "multiport",
	.revision	= 0,
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_multiport)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_multiport)),
	.help		= multiport_help,
	.parse		= multiport_parse6,
	.final_check	= multiport_check,
	.print		= multiport_print6,
	.save		= multiport_save6,
	.extra_opts	= multiport_opts,
};

static struct xtables_match multiport_match_v1 = {
	.family		= AF_INET,
	.name		= "multiport",
	.version	= IPTABLES_VERSION,
	.revision	= 1,
	.size		= XT_ALIGN(sizeof(struct xt_multiport_v1)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_multiport_v1)),
	.help		= multiport_help_v1,
	.parse		= multiport_parse_v1,
	.final_check	= multiport_check,
	.print		= multiport_print_v1,
	.save		= multiport_save_v1,
	.extra_opts	= multiport_opts,
};

static struct xtables_match multiport_match6_v1 = {
	.family		= AF_INET6,
	.name		= "multiport",
	.version	= IPTABLES_VERSION,
	.revision	= 1,
	.size		= XT_ALIGN(sizeof(struct xt_multiport_v1)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_multiport_v1)),
	.help		= multiport_help_v1,
	.parse		= multiport_parse6_v1,
	.final_check	= multiport_check,
	.print		= multiport_print6_v1,
	.save		= multiport_save6_v1,
	.extra_opts	= multiport_opts,
};

void
_init(void)
{
	xtables_register_match(&multiport_match);
	xtables_register_match(&multiport_match6);
	xtables_register_match(&multiport_match_v1);
	xtables_register_match(&multiport_match6_v1);
}
