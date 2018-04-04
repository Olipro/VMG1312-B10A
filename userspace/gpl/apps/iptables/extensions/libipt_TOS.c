/* Shared library add-on to iptables to add TOS target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <iptables.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_TOS.h>

struct tosinfo {
	struct xt_entry_target t;
	struct ipt_tos_target_info tos;
};

/* TOS names and values. */
static const
struct TOS_value
{
	unsigned char TOS;
	const char *name;
} TOS_values[] = {
	{ IPTOS_LOWDELAY,    "Minimize-Delay" },
	{ IPTOS_THROUGHPUT,  "Maximize-Throughput" },
	{ IPTOS_RELIABILITY, "Maximize-Reliability" },
	{ IPTOS_MINCOST,     "Minimize-Cost" },
	{ IPTOS_NORMALSVC,   "Normal-Service" },
};

/* Function which prints out usage message. */
static void TOS_help(void)
{
	unsigned int i;

	printf(
"TOS target v%s options:\n"
"  --set-tos value                   Set Type of Service field to one of the\n"
"                                following numeric or descriptive values:\n",
IPTABLES_VERSION);

	for (i = 0; i < sizeof(TOS_values)/sizeof(struct TOS_value);i++)
		printf("                                     %s %u (0x%02x)\n",
		       TOS_values[i].name,
                       TOS_values[i].TOS,
                       TOS_values[i].TOS);
	fputc('\n', stdout);
}

static const struct option TOS_opts[] = {
	{ "set-tos", 1, NULL, '1' },
	{ }
};

static void
parse_tos(const char *s, struct ipt_tos_target_info *info)
{
	unsigned int i, tos;

	if (string_to_number(s, 0, 255, &tos) != -1) {
		if (tos == IPTOS_LOWDELAY
		    || tos == IPTOS_THROUGHPUT
		    || tos == IPTOS_RELIABILITY
		    || tos == IPTOS_MINCOST
		    || tos == IPTOS_NORMALSVC) {
		    	info->tos = (u_int8_t )tos;
		    	return;
		}
	} else {
		for (i = 0; i<sizeof(TOS_values)/sizeof(struct TOS_value); i++)
			if (strcasecmp(s,TOS_values[i].name) == 0) {
				info->tos = TOS_values[i].TOS;
				return;
			}
	}
	exit_error(PARAMETER_PROBLEM, "Bad TOS value `%s'", s);
}

/* Function which parses command options; returns true if it
   ate an option */
static int TOS_parse(int c, char **argv, int invert, unsigned int *flags,
                     const void *entry, struct xt_entry_target **target)
{
	struct ipt_tos_target_info *tosinfo
		= (struct ipt_tos_target_info *)(*target)->data;

	switch (c) {
	case '1':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "TOS target: Cant specify --set-tos twice");
		parse_tos(optarg, tosinfo);
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static void TOS_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
		           "TOS target: Parameter --set-tos is required");
}

static void
print_tos(u_int8_t tos, int numeric)
{
	unsigned int i;

	if (!numeric) {
		for (i = 0; i<sizeof(TOS_values)/sizeof(struct TOS_value); i++)
			if (TOS_values[i].TOS == tos) {
				printf("%s ", TOS_values[i].name);
				return;
			}
	}
	printf("0x%02x ", tos);
}

/* Prints out the targinfo. */
static void TOS_print(const void *ip, const struct xt_entry_target *target,
                      int numeric)
{
	const struct ipt_tos_target_info *tosinfo =
		(const struct ipt_tos_target_info *)target->data;
	printf("TOS set ");
	print_tos(tosinfo->tos, numeric);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void TOS_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_tos_target_info *tosinfo =
		(const struct ipt_tos_target_info *)target->data;

	printf("--set-tos 0x%02x ", tosinfo->tos);
}

static struct iptables_target tos_target = {
	.name		= "TOS",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_tos_target_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_tos_target_info)),
	.help		= TOS_help,
	.parse		= TOS_parse,
	.final_check	= TOS_check,
	.print		= TOS_print,
	.save		= TOS_save,
	.extra_opts	= TOS_opts,
};

void _init(void)
{
	register_target(&tos_target);
}
