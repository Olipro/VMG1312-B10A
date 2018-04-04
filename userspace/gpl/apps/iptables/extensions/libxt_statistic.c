#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>

#include <xtables.h>
#include <linux/netfilter/xt_statistic.h>

static void statistic_help(void)
{
	printf(
"statistic match v%s options:\n"
" --mode mode                    Match mode (random, nth)\n"
" random mode:\n"
" --probability p		 Probability\n"
" nth mode:\n"
" --every n			 Match every nth packet\n"
" --packet p			 Initial counter value (0 <= p <= n-1, default 0)\n"
"\n",
IPTABLES_VERSION);
}

static const struct option statistic_opts[] = {
	{ "mode", 1, NULL, '1' },
	{ "probability", 1, NULL, '2' },
	{ "every", 1, NULL, '3' },
	{ "packet", 1, NULL, '4' },
	{ }
};

static struct xt_statistic_info *info;

static int
statistic_parse(int c, char **argv, int invert, unsigned int *flags,
                const void *entry, struct xt_entry_match **match)
{
	double prob;

	info = (void *)(*match)->data;

	if (invert)
		info->flags |= XT_STATISTIC_INVERT;

	switch (c) {
	case '1':
		if (*flags & 0x1)
			exit_error(PARAMETER_PROBLEM, "double --mode");
		if (!strcmp(optarg, "random"))
			info->mode = XT_STATISTIC_MODE_RANDOM;
		else if (!strcmp(optarg, "nth"))
			info->mode = XT_STATISTIC_MODE_NTH;
		else
			exit_error(PARAMETER_PROBLEM, "Bad mode `%s'", optarg);
		*flags |= 0x1;
		break;
	case '2':
		if (*flags & 0x2)
			exit_error(PARAMETER_PROBLEM, "double --probability");
		prob = atof(optarg);
		if (prob < 0 || prob > 1)
			exit_error(PARAMETER_PROBLEM,
				   "--probability must be between 0 and 1");
		info->u.random.probability = 0x80000000 * prob;
		*flags |= 0x2;
		break;
	case '3':
		if (*flags & 0x4)
			exit_error(PARAMETER_PROBLEM, "double --every");
		if (string_to_number(optarg, 0, 0xFFFFFFFF,
				     &info->u.nth.every) == -1)
			exit_error(PARAMETER_PROBLEM,
				   "cannot parse --every `%s'", optarg);
		if (info->u.nth.every == 0)
			exit_error(PARAMETER_PROBLEM, "--every cannot be 0");
		info->u.nth.every--;
		*flags |= 0x4;
		break;
	case '4':
		if (*flags & 0x8)
			exit_error(PARAMETER_PROBLEM, "double --packet");
		if (string_to_number(optarg, 0, 0xFFFFFFFF,
				     &info->u.nth.packet) == -1)
			exit_error(PARAMETER_PROBLEM,
				   "cannot parse --packet `%s'", optarg);
		*flags |= 0x8;
		break;
	default:
		return 0;
	}
	return 1;
}

/* Final check; must have specified --mark. */
static void statistic_check(unsigned int flags)
{
	if (!(flags & 0x1))
		exit_error(PARAMETER_PROBLEM, "no mode specified");
	if ((flags & 0x2) && (flags & (0x4 | 0x8)))
		exit_error(PARAMETER_PROBLEM,
			   "both nth and random parameters given");
	if (flags & 0x2 && info->mode != XT_STATISTIC_MODE_RANDOM)
		exit_error(PARAMETER_PROBLEM,
			   "--probability can only be used in random mode");
	if (flags & 0x4 && info->mode != XT_STATISTIC_MODE_NTH)
		exit_error(PARAMETER_PROBLEM,
			   "--every can only be used in nth mode");
	if (flags & 0x8 && info->mode != XT_STATISTIC_MODE_NTH)
		exit_error(PARAMETER_PROBLEM,
			   "--packet can only be used in nth mode");
	if ((flags & 0x8) && !(flags & 0x4))
		exit_error(PARAMETER_PROBLEM,
			   "--packet can only be used with --every");
	/* at this point, info->u.nth.every have been decreased. */
	if (!(info->u.nth.packet >= 0 && info->u.nth.packet <= info->u.nth.every))
		exit_error(PARAMETER_PROBLEM,
			  "the --packet p must be 0 <= p <= n-1");


	info->u.nth.count = info->u.nth.every - info->u.nth.packet;
}

/* Prints out the matchinfo. */
static void print_match(const struct xt_statistic_info *info, char *prefix)
{
	if (info->flags & XT_STATISTIC_INVERT)
		printf("! ");

	switch (info->mode) {
	case XT_STATISTIC_MODE_RANDOM:
		printf("%smode random %sprobability %f ", prefix, prefix,
		       1.0 * info->u.random.probability / 0x80000000);
		break;
	case XT_STATISTIC_MODE_NTH:
		printf("%smode nth %severy %u ", prefix, prefix,
		       info->u.nth.every + 1);
		if (info->u.nth.packet)
			printf("%spacket %u ", prefix, info->u.nth.packet);
		break;
	}
}

static void
statistic_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	struct xt_statistic_info *info = (struct xt_statistic_info *)match->data;

	printf("statistic ");
	print_match(info, "");
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void statistic_save(const void *ip, const struct xt_entry_match *match)
{
	struct xt_statistic_info *info = (struct xt_statistic_info *)match->data;

	print_match(info, "--");
}

static struct xtables_match statistic_match = {
	.family		= AF_INET,
	.name		= "statistic",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_statistic_info)),
	.userspacesize	= offsetof(struct xt_statistic_info, u.nth.count),
	.help		= statistic_help,
	.parse		= statistic_parse,
	.final_check	= statistic_check,
	.print		= statistic_print,
	.save		= statistic_save,
	.extra_opts	= statistic_opts,
};

static struct xtables_match statistic_match6 = {
	.family		= AF_INET6,
	.name		= "statistic",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_statistic_info)),
	.userspacesize	= offsetof(struct xt_statistic_info, u.nth.count),
	.help		= statistic_help,
	.parse		= statistic_parse,
	.final_check	= statistic_check,
	.print		= statistic_print,
	.save		= statistic_save,
	.extra_opts	= statistic_opts,
};

void _init(void)
{
	xtables_register_match(&statistic_match);
	xtables_register_match(&statistic_match6);
}
