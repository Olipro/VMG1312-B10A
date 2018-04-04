/* Shared library add-on to iptables for NFQ
 *
 * (C) 2005 by Harald Welte <laforge@netfilter.org>
 *
 * This program is distributed under the terms of GNU GPL v2, 1991
 *
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_NFQUEUE.h>

static void NFQUEUE_help(void)
{
	printf(
"NFQUEUE target options\n"
"  --queue-num value		Send packet to QUEUE number <value>.\n"
"  		                Valid queue numbers are 0-65535\n"
);
}

static const struct option NFQUEUE_opts[] = {
	{ "queue-num", 1, NULL, 'F' },
	{ }
};

static void
parse_num(const char *s, struct xt_NFQ_info *tinfo)
{
	unsigned int num;
       
	if (string_to_number(s, 0, 65535, &num) == -1)
		exit_error(PARAMETER_PROBLEM,
			   "Invalid queue number `%s'\n", s);

    	tinfo->queuenum = num & 0xffff;
    	return;
}

static int
NFQUEUE_parse(int c, char **argv, int invert, unsigned int *flags,
              const void *entry, struct xt_entry_target **target)
{
	struct xt_NFQ_info *tinfo
		= (struct xt_NFQ_info *)(*target)->data;

	switch (c) {
	case 'F':
		if (*flags)
			exit_error(PARAMETER_PROBLEM, "NFQUEUE target: "
				   "Only use --queue-num ONCE!");
		parse_num(optarg, tinfo);
		break;
	default:
		return 0;
	}

	return 1;
}

/* Prints out the targinfo. */
static void NFQUEUE_print(const void *ip,
                          const struct xt_entry_target *target, int numeric)
{
	const struct xt_NFQ_info *tinfo =
		(const struct xt_NFQ_info *)target->data;
	printf("NFQUEUE num %u", tinfo->queuenum);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void NFQUEUE_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_NFQ_info *tinfo =
		(const struct xt_NFQ_info *)target->data;

	printf("--queue-num %u ", tinfo->queuenum);
}

static struct xtables_target nfqueue_target = {
	.family		= AF_INET,
	.name		= "NFQUEUE",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_NFQ_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_NFQ_info)),
	.help		= NFQUEUE_help,
	.parse		= NFQUEUE_parse,
	.print		= NFQUEUE_print,
	.save		= NFQUEUE_save,
	.extra_opts	= NFQUEUE_opts
};

static struct xtables_target nfqueue_target6 = {
	.family		= AF_INET6,
	.name		= "NFQUEUE",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_NFQ_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_NFQ_info)),
	.help		= NFQUEUE_help,
	.parse		= NFQUEUE_parse,
	.print		= NFQUEUE_print,
	.save		= NFQUEUE_save,
	.extra_opts	= NFQUEUE_opts,
};

void _init(void)
{
	xtables_register_target(&nfqueue_target);
	xtables_register_target(&nfqueue_target6);
}
