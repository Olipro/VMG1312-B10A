/* Shared library add-on to iptables to add connection limit support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stddef.h>
#include <getopt.h>
#include <iptables.h>
#include "../include/linux/netfilter/xt_connlimit.h"

static void connlimit_help(void)
{
	printf(
"connlimit v%s options:\n"
"[!] --connlimit-above n        match if the number of existing "
"                               connections is (not) above n\n"
"    --connlimit-mask n         group hosts using mask\n"
"\n", IPTABLES_VERSION);
}

static const struct option connlimit_opts[] = {
	{"connlimit-above", 1, NULL, 'A'},
	{"connlimit-mask",  1, NULL, 'M'},
	{NULL},
};

static void connlimit_init(struct xt_entry_match *match)
{
	struct xt_connlimit_info *info = (void *)match->data;
	info->v4_mask = 0xFFFFFFFFUL;
}

static void prefix_to_netmask(u_int32_t *mask, unsigned int prefix_len)
{
	if (prefix_len == 0) {
		mask[0] = mask[1] = mask[2] = mask[3] = 0;
	} else if (prefix_len <= 32) {
		mask[0] <<= 32 - prefix_len;
		mask[1] = mask[2] = mask[3] = 0;
	} else if (prefix_len <= 64) {
		mask[1] <<= 32 - (prefix_len - 32);
		mask[2] = mask[3] = 0;
	} else if (prefix_len <= 96) {
		mask[2] <<= 32 - (prefix_len - 64);
		mask[3] = 0;
	} else if (prefix_len <= 128) {
		mask[3] <<= 32 - (prefix_len - 96);
	}
	mask[0] = htonl(mask[0]);
	mask[1] = htonl(mask[1]);
	mask[2] = htonl(mask[2]);
	mask[3] = htonl(mask[3]);
}

static int connlimit_parse(int c, char **argv, int invert, unsigned int *flags,
                           struct xt_connlimit_info *info, unsigned int family)
{
	char *err;
	int i;

	switch (c) {
	case 'A':
		if (*flags & 0x1)
			exit_error(PARAMETER_PROBLEM,
				"--connlimit-above may be given only once");
		*flags |= 0x1;
		check_inverse(optarg, &invert, &optind, 0);
		info->limit   = strtoul(argv[optind-1], NULL, 0);
		info->inverse = invert;
		break;
	case 'M':
		if (*flags & 0x2)
			exit_error(PARAMETER_PROBLEM,
				"--connlimit-mask may be given only once");

		*flags |= 0x2;
		i = strtoul(argv[optind-1], &err, 0);
		if (family == AF_INET6) {
			if (i > 128 || *err != '\0')
				exit_error(PARAMETER_PROBLEM,
					"--connlimit-mask must be between "
					"0 and 128");
			prefix_to_netmask(info->v6_mask, i);
		} else {
			if (i > 32 || *err != '\0')
				exit_error(PARAMETER_PROBLEM,
					"--connlimit-mask must be between "
					"0 and 32");
			if (i == 0)
				info->v4_mask = 0;
			else
				info->v4_mask = htonl(0xFFFFFFFF << (32 - i));
		}
		break;
	default:
		return 0;
	}

	return 1;
}

static int connlimit_parse4(int c, char **argv, int invert,
                            unsigned int *flags, const void *entry,
                            struct xt_entry_match **match)
{
	return connlimit_parse(c, argv, invert, flags,
	       (void *)(*match)->data, AF_INET);
}

static int connlimit_parse6(int c, char **argv, int invert,
                            unsigned int *flags, const void *entry,
                            struct xt_entry_match **match)
{
	return connlimit_parse(c, argv, invert, flags,
	       (void *)(*match)->data, AF_INET6);
}

static void connlimit_check(unsigned int flags)
{
	if (!(flags & 0x1))
		exit_error(PARAMETER_PROBLEM,
			"You must specify \"--connlimit-above\"");
}

static unsigned int count_bits4(u_int32_t mask)
{
	unsigned int bits = 0;

	for (mask = ~ntohl(mask); mask != 0; mask >>= 1)
		++bits;

	return 32 - bits;
}

static unsigned int count_bits6(const u_int32_t *mask)
{
	unsigned int bits = 0, i;
	u_int32_t tmp[4];

	for (i = 0; i < 4; ++i)
		for (tmp[i] = ~ntohl(mask[i]); tmp[i] != 0; tmp[i] >>= 1)
			++bits;
	return 128 - bits;
}

static void connlimit_print4(const void *ip,
                             const struct xt_entry_match *match, int numeric)
{
	const struct xt_connlimit_info *info = (const void *)match->data;

	printf("#conn/%u %s %u ", count_bits4(info->v4_mask),
	       info->inverse ? "<=" : ">", info->limit);
}

static void connlimit_print6(const void *ip,
                             const struct xt_entry_match *match, int numeric)
{
	const struct xt_connlimit_info *info = (const void *)match->data;
	printf("#conn/%u %s %u ", count_bits6(info->v6_mask),
	       info->inverse ? "<=" : ">", info->limit);
}

static void connlimit_save4(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_connlimit_info *info = (const void *)match->data;

	printf("%s--connlimit-above %u --connlimit-mask %u ",
	       info->inverse ? "! " : "", info->limit,
	       count_bits4(info->v4_mask));
}

static void connlimit_save6(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_connlimit_info *info = (const void *)match->data;

	printf("%s--connlimit-above %u --connlimit-mask %u ",
	       info->inverse ? "! " : "", info->limit,
	       count_bits6(info->v6_mask));
}

static struct xtables_match connlimit_match = {
	.name          = "connlimit",
	.family        = AF_INET,
	.version       = IPTABLES_VERSION,
	.size          = XT_ALIGN(sizeof(struct xt_connlimit_info)),
	.userspacesize = offsetof(struct xt_connlimit_info, data),
	.help          = connlimit_help,
	.init          = connlimit_init,
	.parse         = connlimit_parse4,
	.final_check   = connlimit_check,
	.print         = connlimit_print4,
	.save          = connlimit_save4,
	.extra_opts    = connlimit_opts,
};

static struct xtables_match connlimit_match6 = {
	.name          = "connlimit",
	.family        = AF_INET6,
	.version       = IPTABLES_VERSION,
	.size          = XT_ALIGN(sizeof(struct xt_connlimit_info)),
	.userspacesize = offsetof(struct xt_connlimit_info, data),
	.help          = connlimit_help,
	.init          = connlimit_init,
	.parse         = connlimit_parse6,
	.final_check   = connlimit_check,
	.print         = connlimit_print6,
	.save          = connlimit_save6,
	.extra_opts    = connlimit_opts,
};

void _init(void)
{
	xtables_register_match(&connlimit_match);
	xtables_register_match(&connlimit_match6);
}
