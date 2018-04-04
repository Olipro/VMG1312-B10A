/* Shared library add-on to iptables to add TCPMSS target support.
 *
 * Copyright (c) 2000 Marc Boucher
*/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_TCPMSS.h>

struct mssinfo {
	struct xt_entry_target t;
	struct xt_tcpmss_info mss;
};

/* Function which prints out usage message. */
static void __TCPMSS_help(int hdrsize)
{
	printf(
"TCPMSS target v%s mutually-exclusive options:\n"
"  --set-mss value               explicitly set MSS option to specified value\n"
"  --clamp-mss-to-pmtu           automatically clamp MSS value to (path_MTU - %d)\n",
IPTABLES_VERSION, hdrsize);
}

static void TCPMSS_help(void)
{
	__TCPMSS_help(40);
}

static void TCPMSS_help6(void)
{
	__TCPMSS_help(60);
}

static const struct option TCPMSS_opts[] = {
	{ "set-mss", 1, NULL, '1' },
	{ "clamp-mss-to-pmtu", 0, NULL, '2' },
	{ }
};

/* Function which parses command options; returns true if it
   ate an option */
static int __TCPMSS_parse(int c, char **argv, int invert, unsigned int *flags,
                          const void *entry, struct xt_entry_target **target,
                          int hdrsize)
{
	struct xt_tcpmss_info *mssinfo
		= (struct xt_tcpmss_info *)(*target)->data;

	switch (c) {
		unsigned int mssval;

	case '1':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "TCPMSS target: Only one option may be specified");
		if (string_to_number(optarg, 0, 65535 - hdrsize, &mssval) == -1)
			exit_error(PARAMETER_PROBLEM, "Bad TCPMSS value `%s'", optarg);
		
		mssinfo->mss = mssval;
		*flags = 1;
		break;

	case '2':
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "TCPMSS target: Only one option may be specified");
		mssinfo->mss = XT_TCPMSS_CLAMP_PMTU;
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static int TCPMSS_parse(int c, char **argv, int invert, unsigned int *flags,
                        const void *entry, struct xt_entry_target **target)
{
	return __TCPMSS_parse(c, argv, invert, flags, entry, target, 40);
}

static int TCPMSS_parse6(int c, char **argv, int invert, unsigned int *flags,
                         const void *entry, struct xt_entry_target **target)
{
	return __TCPMSS_parse(c, argv, invert, flags, entry, target, 60);
}

static void TCPMSS_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
		           "TCPMSS target: At least one parameter is required");
}

/* Prints out the targinfo. */
static void TCPMSS_print(const void *ip, const struct xt_entry_target *target,
                         int numeric)
{
	const struct xt_tcpmss_info *mssinfo =
		(const struct xt_tcpmss_info *)target->data;
	if(mssinfo->mss == XT_TCPMSS_CLAMP_PMTU)
		printf("TCPMSS clamp to PMTU ");
	else
		printf("TCPMSS set %u ", mssinfo->mss);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void TCPMSS_save(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_tcpmss_info *mssinfo =
		(const struct xt_tcpmss_info *)target->data;

	if(mssinfo->mss == XT_TCPMSS_CLAMP_PMTU)
		printf("--clamp-mss-to-pmtu ");
	else
		printf("--set-mss %u ", mssinfo->mss);
}

static struct xtables_target tcpmss_target = {
	.family		= AF_INET,
	.name		= "TCPMSS",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_tcpmss_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_tcpmss_info)),
	.help		= TCPMSS_help,
	.parse		= TCPMSS_parse,
	.final_check	= TCPMSS_check,
	.print		= TCPMSS_print,
	.save		= TCPMSS_save,
	.extra_opts	= TCPMSS_opts,
};

static struct xtables_target tcpmss_target6 = {
	.family		= AF_INET6,
	.name		= "TCPMSS",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_tcpmss_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_tcpmss_info)),
	.help		= TCPMSS_help6,
	.parse		= TCPMSS_parse6,
	.final_check	= TCPMSS_check,
	.print		= TCPMSS_print,
	.save		= TCPMSS_save,
	.extra_opts	= TCPMSS_opts,
};

void _init(void)
{
	xtables_register_target(&tcpmss_target);
	xtables_register_target(&tcpmss_target6);
}
