/* Shared library add-on to iptables to add UDP support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <xtables.h>
#include <linux/netfilter/xt_tcpudp.h>

/* Function which prints out usage message. */
static void udp_help(void)
{
	printf(
"UDP v%s options:\n"
" --source-port [!] port[:port]\n"
" --sport ...\n"
"				match source port(s)\n"
" --destination-port [!] port[:port]\n"
" --dport ...\n"
"				match destination port(s)\n",
IPTABLES_VERSION);
}

static const struct option udp_opts[] = {
	{ "source-port", 1, NULL, '1' },
	{ "sport", 1, NULL, '1' }, /* synonym */
	{ "destination-port", 1, NULL, '2' },
	{ "dport", 1, NULL, '2' }, /* synonym */
	{ }
};

static void
parse_udp_ports(const char *portstring, u_int16_t *ports)
{
	char *buffer;
	char *cp;

	buffer = strdup(portstring);
	if ((cp = strchr(buffer, ':')) == NULL)
		ports[0] = ports[1] = parse_port(buffer, "udp");
	else {
		*cp = '\0';
		cp++;

		ports[0] = buffer[0] ? parse_port(buffer, "udp") : 0;
		ports[1] = cp[0] ? parse_port(cp, "udp") : 0xFFFF;

		if (ports[0] > ports[1])
			exit_error(PARAMETER_PROBLEM,
				   "invalid portrange (min > max)");
	}
	free(buffer);
}

/* Initialize the match. */
static void udp_init(struct xt_entry_match *m)
{
	struct xt_udp *udpinfo = (struct xt_udp *)m->data;

	udpinfo->spts[1] = udpinfo->dpts[1] = 0xFFFF;
}

#define UDP_SRC_PORTS 0x01
#define UDP_DST_PORTS 0x02

/* Function which parses command options; returns true if it
   ate an option */
static int
udp_parse(int c, char **argv, int invert, unsigned int *flags,
          const void *entry, struct xt_entry_match **match)
{
	struct xt_udp *udpinfo = (struct xt_udp *)(*match)->data;

	switch (c) {
	case '1':
		if (*flags & UDP_SRC_PORTS)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--source-port' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_udp_ports(argv[optind-1], udpinfo->spts);
		if (invert)
			udpinfo->invflags |= XT_UDP_INV_SRCPT;
		*flags |= UDP_SRC_PORTS;
		break;

	case '2':
		if (*flags & UDP_DST_PORTS)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--destination-port' allowed");
		check_inverse(optarg, &invert, &optind, 0);
		parse_udp_ports(argv[optind-1], udpinfo->dpts);
		if (invert)
			udpinfo->invflags |= XT_UDP_INV_DSTPT;
		*flags |= UDP_DST_PORTS;
		break;

	default:
		return 0;
	}

	return 1;
}

static char *
port_to_service(int port)
{
	struct servent *service;

	if ((service = getservbyport(htons(port), "udp")))
		return service->s_name;

	return NULL;
}

static void
print_port(u_int16_t port, int numeric)
{
	char *service;

	if (numeric || (service = port_to_service(port)) == NULL)
		printf("%u", port);
	else
		printf("%s", service);
}

static void
print_ports(const char *name, u_int16_t min, u_int16_t max,
	    int invert, int numeric)
{
	const char *inv = invert ? "!" : "";

	if (min != 0 || max != 0xFFFF || invert) {
		printf("%s", name);
		if (min == max) {
			printf(":%s", inv);
			print_port(min, numeric);
		} else {
			printf("s:%s", inv);
			print_port(min, numeric);
			printf(":");
			print_port(max, numeric);
		}
		printf(" ");
	}
}

/* Prints out the union ipt_matchinfo. */
static void
udp_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_udp *udp = (struct xt_udp *)match->data;

	printf("udp ");
	print_ports("spt", udp->spts[0], udp->spts[1],
		    udp->invflags & XT_UDP_INV_SRCPT,
		    numeric);
	print_ports("dpt", udp->dpts[0], udp->dpts[1],
		    udp->invflags & XT_UDP_INV_DSTPT,
		    numeric);
	if (udp->invflags & ~XT_UDP_INV_MASK)
		printf("Unknown invflags: 0x%X ",
		       udp->invflags & ~XT_UDP_INV_MASK);
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void udp_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_udp *udpinfo = (struct xt_udp *)match->data;

	if (udpinfo->spts[0] != 0
	    || udpinfo->spts[1] != 0xFFFF) {
		if (udpinfo->invflags & XT_UDP_INV_SRCPT)
			printf("! ");
		if (udpinfo->spts[0]
		    != udpinfo->spts[1])
			printf("--sport %u:%u ",
			       udpinfo->spts[0],
			       udpinfo->spts[1]);
		else
			printf("--sport %u ",
			       udpinfo->spts[0]);
	}

	if (udpinfo->dpts[0] != 0
	    || udpinfo->dpts[1] != 0xFFFF) {
		if (udpinfo->invflags & XT_UDP_INV_DSTPT)
			printf("! ");
		if (udpinfo->dpts[0]
		    != udpinfo->dpts[1])
			printf("--dport %u:%u ",
			       udpinfo->dpts[0],
			       udpinfo->dpts[1]);
		else
			printf("--dport %u ",
			       udpinfo->dpts[0]);
	}
}

static struct xtables_match udp_match = {
	.family		= AF_INET,
	.name		= "udp",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_udp)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_udp)),
	.help		= udp_help,
	.init		= udp_init,
	.parse		= udp_parse,
	.print		= udp_print,
	.save		= udp_save,
	.extra_opts	= udp_opts,
};

static struct xtables_match udp_match6 = {
	.family		= AF_INET6,
	.name		= "udp",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_udp)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_udp)),
	.help		= udp_help,
	.init		= udp_init,
	.parse		= udp_parse,
	.print		= udp_print,
	.save		= udp_save,
	.extra_opts	= udp_opts,
};

void
_init(void)
{
	xtables_register_match(&udp_match);
	xtables_register_match(&udp_match6);
}
