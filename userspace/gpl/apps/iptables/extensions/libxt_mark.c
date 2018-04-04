/* Shared library add-on to iptables to add NFMARK matching support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <xtables.h>
/* For 64bit kernel / 32bit userspace */
#include "../include/linux/netfilter/xt_mark.h"

/* Function which prints out usage message. */
static void mark_help(void)
{
	printf(
"MARK match v%s options:\n"
"[!] --mark value[/mask]         Match nfmark value with optional mask\n"
#if 1 //__MSTC__, Jeff
"--mark-classid-cmp value: Compare classid bits(skb-mark[5:10]), if the bits is not zero and less than value, then matching result is TRUE\n"
#endif
"\n",
IPTABLES_VERSION);
}

static const struct option mark_opts[] = {
	{ "mark", 1, NULL, '1' },
#if 1 //__MSTC__, Jeff
	{ "mark-classid-cmp", 1, NULL, '2' },
#endif
	{ }
};

/* Function which parses command options; returns true if it
   ate an option */
static int
mark_parse(int c, char **argv, int invert, unsigned int *flags,
           const void *entry, struct xt_entry_match **match)
{
	struct xt_mark_info *markinfo = (struct xt_mark_info *)(*match)->data;

	switch (c) {
		char *end;
	case '1':
		check_inverse(optarg, &invert, &optind, 0);
		markinfo->mark = strtoul(optarg, &end, 0);
		if (*end == '/') {
			markinfo->mask = strtoul(end+1, &end, 0);
		} else
			markinfo->mask = 0xffffffff;
		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);
#if 1 //__MSTC__, Jeff
		markinfo->bitmask |= IPT_MARK;
#endif
		if (invert)
			markinfo->invert = 1;
#if 1 //__MSTC__, Jeff
		*flags |= 1;
#else
		*flags = 1;
#endif
		break;
#if 1 //__MSTC__, Jeff
	case '2':
		markinfo->mark = strtoul(optarg, &end, 0);
		if (*end != '\0' || end == optarg)
			exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);
		markinfo->bitmask |= IPT_MARK_CLASSID_CMP;
		*flags |= 2;
		break;
#endif

	default:
		return 0;
	}
	return 1;
}

static void
print_mark(unsigned long mark, unsigned long mask, int numeric)
{
	if(mask != 0xffffffff)
		printf("0x%lx/0x%lx ", mark, mask);
	else
		printf("0x%lx ", mark);
}

/* Final check; must have specified --mark. */
static void mark_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "MARK match: You must specify `--mark'");
}

/* Prints out the matchinfo. */
static void
mark_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	struct xt_mark_info *info = (struct xt_mark_info *)match->data;

#if 1 //__MSTC__, Jeff
	if(info->bitmask & IPT_MARK)
	{
#endif
	printf("MARK match ");

	if (info->invert)
		printf("!");
	
	print_mark(info->mark, info->mask, numeric);
#if 1 //__MSTC__, Jeff
	}
	else if(info->bitmask & IPT_MARK_CLASSID_CMP)
	{
		printf("MARK_CLASSID_CMP with %x ", info->mark);
	}
#endif
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
mark_save(const void *ip, const struct xt_entry_match *match)
{
	struct xt_mark_info *info = (struct xt_mark_info *)match->data;

#if 1 //__MSTC__, Jeff
	if(info->bitmask & IPT_MARK)
	{
#endif
	if (info->invert)
		printf("! ");
	
	printf("--mark ");
	print_mark(info->mark, info->mask, 0);
#if 1 //__MSTC__, Jeff
	}
	else if(info->bitmask & IPT_MARK_CLASSID_CMP)
	{
		printf("--mark-classid-cmp %x ", info->mark);
	}
#endif
}

static struct xtables_match mark_match = {
	.family		= AF_INET,
	.name		= "mark",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_mark_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_mark_info)),
	.help		= mark_help,
	.parse		= mark_parse,
	.final_check	= mark_check,
	.print		= mark_print,
	.save		= mark_save,
	.extra_opts	= mark_opts,
};

static struct xtables_match mark_match6 = {
	.family		= AF_INET6,
	.name		= "mark",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_mark_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_mark_info)),
	.help		= mark_help,
	.parse		= mark_parse,
	.final_check	= mark_check,
	.print		= mark_print,
	.save		= mark_save,
	.extra_opts	= mark_opts,
};

void _init(void)
{
	xtables_register_match(&mark_match);
	xtables_register_match(&mark_match6);
}
