/* Shared library add-on to iptables for the TTL target
 * (C) 2000 by Harald Welte <laforge@gnumonks.org>
 *
 * $Id: libipt_TTL.c 7062 2007-10-04 16:29:00Z /C=EU/ST=EU/CN=Patrick McHardy/emailAddress=kaber@trash.net $
 *
 * This program is distributed under the terms of GNU GPL
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <iptables.h>

#include <linux/netfilter_ipv4/ip_tables.h>
#include <linux/netfilter_ipv4/ipt_TTL.h>

#define IPT_TTL_USED	1

static void TTL_help(void)
{
	printf(
"TTL target v%s options\n"
"  --ttl-set value		Set TTL to <value 0-255>\n"
"  --ttl-dec value		Decrement TTL by <value 1-255>\n"
"  --ttl-inc value		Increment TTL by <value 1-255>\n"
, IPTABLES_VERSION);
}

static int TTL_parse(int c, char **argv, int invert, unsigned int *flags,
                     const void *entry, struct xt_entry_target **target)
{
	struct ipt_TTL_info *info = (struct ipt_TTL_info *) (*target)->data;
	unsigned int value;

	if (*flags & IPT_TTL_USED) {
		exit_error(PARAMETER_PROBLEM, 
				"Can't specify TTL option twice");
	}

	if (!optarg) 
		exit_error(PARAMETER_PROBLEM, 
				"TTL: You must specify a value");

	if (check_inverse(optarg, &invert, NULL, 0))
		exit_error(PARAMETER_PROBLEM,
				"TTL: unexpected `!'");
	
	if (string_to_number(optarg, 0, 255, &value) == -1)
		exit_error(PARAMETER_PROBLEM,
		           "TTL: Expected value between 0 and 255");

	switch (c) {

		case '1':
			info->mode = IPT_TTL_SET;
			break;

		case '2':
			if (value == 0) {
				exit_error(PARAMETER_PROBLEM,
					"TTL: decreasing by 0?");
			}

			info->mode = IPT_TTL_DEC;
			break;

		case '3':
			if (value == 0) {
				exit_error(PARAMETER_PROBLEM,
					"TTL: increasing by 0?");
			}

			info->mode = IPT_TTL_INC;
			break;

		default:
			return 0;

	}
	
	info->ttl = value;
	*flags |= IPT_TTL_USED;

	return 1;
}

static void TTL_check(unsigned int flags)
{
	if (!(flags & IPT_TTL_USED))
		exit_error(PARAMETER_PROBLEM,
				"TTL: You must specify an action");
}

static void TTL_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_TTL_info *info = 
		(struct ipt_TTL_info *) target->data;

	switch (info->mode) {
		case IPT_TTL_SET:
			printf("--ttl-set ");
			break;
		case IPT_TTL_DEC:
			printf("--ttl-dec ");
			break;

		case IPT_TTL_INC:
			printf("--ttl-inc ");
			break;
	}
	printf("%u ", info->ttl);
}

static void TTL_print(const void *ip, const struct xt_entry_target *target,
                      int numeric)
{
	const struct ipt_TTL_info *info =
		(struct ipt_TTL_info *) target->data;

	printf("TTL ");
	switch (info->mode) {
		case IPT_TTL_SET:
			printf("set to ");
			break;
		case IPT_TTL_DEC:
			printf("decrement by ");
			break;
		case IPT_TTL_INC:
			printf("increment by ");
			break;
	}
	printf("%u ", info->ttl);
}

static const struct option TTL_opts[] = {
	{ "ttl-set", 1, NULL, '1' },
	{ "ttl-dec", 1, NULL, '2' },
	{ "ttl-inc", 1, NULL, '3' },
	{ }
};

static struct iptables_target ttl_target = {
	.next		= NULL, 
	.name		= "TTL",
	.version	= IPTABLES_VERSION,
	.size		= IPT_ALIGN(sizeof(struct ipt_TTL_info)),
	.userspacesize	= IPT_ALIGN(sizeof(struct ipt_TTL_info)),
	.help		= TTL_help,
	.parse		= TTL_parse,
	.final_check	= TTL_check,
	.print		= TTL_print,
	.save		= TTL_save,
	.extra_opts	= TTL_opts,
};

void _init(void)
{
	register_target(&ttl_target);
}
