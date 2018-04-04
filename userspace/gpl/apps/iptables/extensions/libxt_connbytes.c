/* Shared library add-on to iptables to add byte tracking support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <xtables.h>
#include <linux/netfilter/nf_conntrack_common.h>
#include <linux/netfilter/xt_connbytes.h>

/* Function which prints out usage message. */
static void connbytes_help(void)
{
	printf(
"connbytes v%s options:\n"
" [!] --connbytes from:[to]\n"
"     --connbytes-dir [original, reply, both]\n"
"     --connbytes-mode [packets, bytes, avgpkt]\n"
"\n", IPTABLES_VERSION);
}

static const struct option connbytes_opts[] = {
	{ "connbytes", 1, NULL, '1' },
	{ "connbytes-dir", 1, NULL, '2' },
	{ "connbytes-mode", 1, NULL, '3' },
	{ }
};

static void
parse_range(const char *arg, struct xt_connbytes_info *si)
{
	char *colon,*p;

	si->count.from = strtoul(arg,&colon,10);
	if (*colon != ':') 
		exit_error(PARAMETER_PROBLEM, "Bad range `%s'", arg);
	si->count.to = strtoul(colon+1,&p,10);
	if (p == colon+1) {
		/* second number omited */
		si->count.to = 0xffffffff;
	}
	if (si->count.from > si->count.to)
		exit_error(PARAMETER_PROBLEM, "%llu should be less than %llu",
			   (unsigned long long)si->count.from,
			   (unsigned long long)si->count.to);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
connbytes_parse(int c, char **argv, int invert, unsigned int *flags,
                const void *entry, struct xt_entry_match **match)
{
	struct xt_connbytes_info *sinfo = (struct xt_connbytes_info *)(*match)->data;
	unsigned long i;

	switch (c) {
	case '1':
		if (check_inverse(optarg, &invert, &optind, 0))
			optind++;

		parse_range(argv[optind-1], sinfo);
		if (invert) {
			i = sinfo->count.from;
			sinfo->count.from = sinfo->count.to;
			sinfo->count.to = i;
		}
		*flags |= 1;
		break;
	case '2':
		if (!strcmp(optarg, "original"))
			sinfo->direction = XT_CONNBYTES_DIR_ORIGINAL;
		else if (!strcmp(optarg, "reply"))
			sinfo->direction = XT_CONNBYTES_DIR_REPLY;
		else if (!strcmp(optarg, "both"))
			sinfo->direction = XT_CONNBYTES_DIR_BOTH;
		else
			exit_error(PARAMETER_PROBLEM,
				   "Unknown --connbytes-dir `%s'", optarg);

		*flags |= 2;
		break;
	case '3':
		if (!strcmp(optarg, "packets"))
			sinfo->what = XT_CONNBYTES_PKTS;
		else if (!strcmp(optarg, "bytes"))
			sinfo->what = XT_CONNBYTES_BYTES;
		else if (!strcmp(optarg, "avgpkt"))
			sinfo->what = XT_CONNBYTES_AVGPKT;
		else
			exit_error(PARAMETER_PROBLEM,
				   "Unknown --connbytes-mode `%s'", optarg);
		*flags |= 4;
		break;
	default:
		return 0;
	}

	return 1;
}

static void connbytes_check(unsigned int flags)
{
	if (flags != 7)
		exit_error(PARAMETER_PROBLEM, "You must specify `--connbytes'"
			   "`--connbytes-dir' and `--connbytes-mode'");
}

static void print_mode(struct xt_connbytes_info *sinfo)
{
	switch (sinfo->what) {
		case XT_CONNBYTES_PKTS:
			fputs("packets ", stdout);
			break;
		case XT_CONNBYTES_BYTES:
			fputs("bytes ", stdout);
			break;
		case XT_CONNBYTES_AVGPKT:
			fputs("avgpkt ", stdout);
			break;
		default:
			fputs("unknown ", stdout);
			break;
	}
}

static void print_direction(struct xt_connbytes_info *sinfo)
{
	switch (sinfo->direction) {
		case XT_CONNBYTES_DIR_ORIGINAL:
			fputs("original ", stdout);
			break;
		case XT_CONNBYTES_DIR_REPLY:
			fputs("reply ", stdout);
			break;
		case XT_CONNBYTES_DIR_BOTH:
			fputs("both ", stdout);
			break;
		default:
			fputs("unknown ", stdout);
			break;
	}
}

/* Prints out the matchinfo. */
static void
connbytes_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	struct xt_connbytes_info *sinfo = (struct xt_connbytes_info *)match->data;

	if (sinfo->count.from > sinfo->count.to) 
		printf("connbytes ! %llu:%llu ",
			(unsigned long long)sinfo->count.to,
			(unsigned long long)sinfo->count.from);
	else
		printf("connbytes %llu:%llu ",
			(unsigned long long)sinfo->count.from,
			(unsigned long long)sinfo->count.to);

	fputs("connbytes mode ", stdout);
	print_mode(sinfo);

	fputs("connbytes direction ", stdout);
	print_direction(sinfo);
}

/* Saves the matchinfo in parsable form to stdout. */
static void connbytes_save(const void *ip, const struct xt_entry_match *match)
{
	struct xt_connbytes_info *sinfo = (struct xt_connbytes_info *)match->data;

	if (sinfo->count.from > sinfo->count.to) 
		printf("! --connbytes %llu:%llu ",
			(unsigned long long)sinfo->count.to,
			(unsigned long long)sinfo->count.from);
	else
		printf("--connbytes %llu:%llu ",
			(unsigned long long)sinfo->count.from,
			(unsigned long long)sinfo->count.to);

	fputs("--connbytes-mode ", stdout);
	print_mode(sinfo);

	fputs("--connbytes-dir ", stdout);
	print_direction(sinfo);
}

static struct xtables_match connbytes_match = {
	.family		= AF_INET,
	.name 		= "connbytes",
	.version 	= IPTABLES_VERSION,
	.size 		= XT_ALIGN(sizeof(struct xt_connbytes_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_connbytes_info)),
	.help		= connbytes_help,
	.parse		= connbytes_parse,
	.final_check	= connbytes_check,
	.print		= connbytes_print,
	.save 		= connbytes_save,
	.extra_opts	= connbytes_opts,
};

static struct xtables_match connbytes_match6 = {
	.family		= AF_INET6,
	.name 		= "connbytes",
	.version 	= IPTABLES_VERSION,
	.size 		= XT_ALIGN(sizeof(struct xt_connbytes_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_connbytes_info)),
	.help		= connbytes_help,
	.parse		= connbytes_parse,
	.final_check	= connbytes_check,
	.print		= connbytes_print,
	.save 		= connbytes_save,
	.extra_opts	= connbytes_opts,
};

void _init(void)
{
	xtables_register_match(&connbytes_match);
	xtables_register_match(&connbytes_match6);
}
