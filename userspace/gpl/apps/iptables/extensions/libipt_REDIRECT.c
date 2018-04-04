/* Shared library add-on to iptables to add redirect support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter/nf_nat.h>

#define IPT_REDIRECT_OPT_DEST	0x01
#define IPT_REDIRECT_OPT_RANDOM	0x02

/* Function which prints out usage message. */
static void REDIRECT_help(void)
{
	printf(
"REDIRECT v%s options:\n"
" --to-ports <port>[-<port>]\n"
"				Port (range) to map to.\n\n",
IPTABLES_VERSION);
}

static const struct option REDIRECT_opts[] = {
	{ "to-ports", 1, NULL, '1' },
	{ "random", 0, NULL, '2' },
	{ }
};

/* Initialize the target. */
static void REDIRECT_init(struct xt_entry_target *t)
{
	struct ip_nat_multi_range *mr = (struct ip_nat_multi_range *)t->data;

	/* Actually, it's 0, but it's ignored at the moment. */
	mr->rangesize = 1;

}

/* Parses ports */
static void
parse_ports(const char *arg, struct ip_nat_multi_range *mr)
{
	const char *dash;
	int port;

	mr->range[0].flags |= IP_NAT_RANGE_PROTO_SPECIFIED;

	if (strchr(arg, '.'))
		exit_error(PARAMETER_PROBLEM, "IP address not permitted\n");

	port = atoi(arg);
	if (port == 0 || port > 65535)
		exit_error(PARAMETER_PROBLEM, "Port `%s' not valid\n", arg);

	dash = strchr(arg, '-');
	if (!dash) {
		mr->range[0].min.tcp.port
			= mr->range[0].max.tcp.port
			= htons(port);
	} else {
		int maxport;

		maxport = atoi(dash + 1);
		if (maxport == 0 || maxport > 65535)
			exit_error(PARAMETER_PROBLEM,
				   "Port `%s' not valid\n", dash+1);
		if (maxport < port)
			/* People are stupid. */
			exit_error(PARAMETER_PROBLEM,
				   "Port range `%s' funky\n", arg);
		mr->range[0].min.tcp.port = htons(port);
		mr->range[0].max.tcp.port = htons(maxport);
	}
}

/* Function which parses command options; returns true if it
   ate an option */
static int REDIRECT_parse(int c, char **argv, int invert, unsigned int *flags,
                          const void *e, struct xt_entry_target **target)
{
	const struct ipt_entry *entry = e;
	struct ip_nat_multi_range *mr
		= (struct ip_nat_multi_range *)(*target)->data;
	int portok;

	if (entry->ip.proto == IPPROTO_TCP
	    || entry->ip.proto == IPPROTO_UDP
	    || entry->ip.proto == IPPROTO_ICMP)
		portok = 1;
	else
		portok = 0;

	switch (c) {
	case '1':
		if (!portok)
			exit_error(PARAMETER_PROBLEM,
				   "Need TCP or UDP with port specification");

		if (check_inverse(optarg, &invert, NULL, 0))
			exit_error(PARAMETER_PROBLEM,
				   "Unexpected `!' after --to-ports");

		parse_ports(optarg, mr);
		if (*flags & IPT_REDIRECT_OPT_RANDOM)
			mr->range[0].flags |= IP_NAT_RANGE_PROTO_RANDOM;
		*flags |= IPT_REDIRECT_OPT_DEST;
		return 1;

	case '2':
		if (*flags & IPT_REDIRECT_OPT_DEST) {
			mr->range[0].flags |= IP_NAT_RANGE_PROTO_RANDOM;
			*flags |= IPT_REDIRECT_OPT_RANDOM;
		} else
			*flags |= IPT_REDIRECT_OPT_RANDOM;
		return 1;

	default:
		return 0;
	}
}

/* Prints out the targinfo. */
static void REDIRECT_print(const void *ip, const struct xt_entry_target *target,
                           int numeric)
{
	struct ip_nat_multi_range *mr
		= (struct ip_nat_multi_range *)target->data;
	struct ip_nat_range *r = &mr->range[0];

	if (r->flags & IP_NAT_RANGE_PROTO_SPECIFIED) {
		printf("redir ports ");
		printf("%hu", ntohs(r->min.tcp.port));
		if (r->max.tcp.port != r->min.tcp.port)
			printf("-%hu", ntohs(r->max.tcp.port));
		printf(" ");
		if (mr->range[0].flags & IP_NAT_RANGE_PROTO_RANDOM)
			printf("random ");
	}
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void REDIRECT_save(const void *ip, const struct xt_entry_target *target)
{
	struct ip_nat_multi_range *mr
		= (struct ip_nat_multi_range *)target->data;
	struct ip_nat_range *r = &mr->range[0];

	if (r->flags & IP_NAT_RANGE_PROTO_SPECIFIED) {
		printf("--to-ports ");
		printf("%hu", ntohs(r->min.tcp.port));
		if (r->max.tcp.port != r->min.tcp.port)
			printf("-%hu", ntohs(r->max.tcp.port));
		printf(" ");
		if (mr->range[0].flags & IP_NAT_RANGE_PROTO_RANDOM)
			printf("--random ");
	}
}

static struct iptables_target redirect_target = {
	.name		= "REDIRECT",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ip_nat_multi_range)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ip_nat_multi_range)),
	.help		= REDIRECT_help,
	.init		= REDIRECT_init,
 	.parse		= REDIRECT_parse,
	.print		= REDIRECT_print,
	.save		= REDIRECT_save,
	.extra_opts	= REDIRECT_opts,
};

void _init(void)
{
	register_target(&redirect_target);
}
