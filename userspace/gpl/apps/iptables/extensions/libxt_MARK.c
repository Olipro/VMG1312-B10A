/* Shared library add-on to iptables to add MARK target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/xt_MARK.h>

/* Function which prints out usage message. */
static void MARK_help(void)
{
	printf(
"MARK target v%s options:\n"
"  --set-mark value                   Set nfmark value\n"
"  --and-mark value                   Binary AND the nfmark with value\n"
"  --or-mark  value                   Binary OR  the nfmark with value\n"

#if 1 //__MSTC__, ZyXEL richard, QoS
"  --vtag-set value                   Set Vlan tag value\n"
#endif //__MSTC__, ZyXEL richard, QoS

"\n",
IPTABLES_VERSION);
}

static const struct option MARK_opts[] = {
	{ "set-mark", 1, NULL, '1' },
	{ "and-mark", 1, NULL, '2' },
	{ "or-mark", 1, NULL, '3' },

#if 1 //__MSTC__, ZyXEL richard, QoS
	{ "vtag-set", 1, NULL, '4' },
#endif //__MSTC__, ZyXEL richard, QoS

	{ }
};

/* Function which parses command options; returns true if it
   ate an option */
static int
MARK_parse_v0(int c, char **argv, int invert, unsigned int *flags,
              const void *entry, struct xt_entry_target **target)
{
	struct xt_mark_target_info *markinfo
		= (struct xt_mark_target_info *)(*target)->data;

	switch (c) {
	case '1':
		if (string_to_number_l(optarg, 0, 0, 
				     &markinfo->mark))
			exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);
		if (*flags)
			exit_error(PARAMETER_PROBLEM,
			           "MARK target: Can't specify --set-mark twice");
		*flags = 1;
		break;
	case '2':
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: kernel too old for --and-mark");
	case '3':
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: kernel too old for --or-mark");

#if 1 //__MSTC__, ZyXEL richard, QoS
	case '4':
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: kernel too old for --vtag-set");
#endif //__MSTC__, ZyXEL richard, QoS

	default:
		return 0;
	}

	return 1;
}

static void MARK_check(unsigned int flags)
{
	if (!flags)
#if 0 //__MSTC__, ZyXEL richard, QoS
		exit_error(PARAMETER_PROBLEM,
		           "MARK target: Parameter --set/and/or-mark"
			   " is required");
#else

		exit_error(PARAMETER_PROBLEM,
		           "MARK target: Parameter --set/and/or-mark/vtag-set"
			   " is required");
#endif //__MSTC__, ZyXEL richard, QoS
}

/* Function which parses command options; returns true if it
   ate an option */
static int
MARK_parse_v1(int c, char **argv, int invert, unsigned int *flags,
              const void *entry, struct xt_entry_target **target)
{
	struct xt_mark_target_info_v1 *markinfo
		= (struct xt_mark_target_info_v1 *)(*target)->data;

	switch (c) {
	case '1':
	        markinfo->mode = XT_MARK_SET;
		break;
	case '2':
	        markinfo->mode = XT_MARK_AND;
		break;
	case '3':
	        markinfo->mode = XT_MARK_OR;
		break;

#if 1 //__MSTC__, ZyXEL richard, QoS
	case '4':
	        markinfo->mode = 4;
		break;
#endif //__MSTC__, ZyXEL richard, QoS

	default:
		return 0;
	}

	if (string_to_number_l(optarg, 0, 0, &markinfo->mark))
		exit_error(PARAMETER_PROBLEM, "Bad MARK value `%s'", optarg);

	if (*flags)
		exit_error(PARAMETER_PROBLEM,
			   "MARK target: Can't specify --set-mark twice");

	*flags = 1;
	return 1;
}

static void
print_mark(unsigned long mark)
{
	printf("0x%lx ", mark);
}

/* Prints out the targinfo. */
static void MARK_print_v0(const void *ip,
                          const struct xt_entry_target *target, int numeric)
{
	const struct xt_mark_target_info *markinfo =
		(const struct xt_mark_target_info *)target->data;
	printf("MARK set ");
	print_mark(markinfo->mark);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void MARK_save_v0(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_mark_target_info *markinfo =
		(const struct xt_mark_target_info *)target->data;

	printf("--set-mark ");
	print_mark(markinfo->mark);
}

/* Prints out the targinfo. */
static void MARK_print_v1(const void *ip, const struct xt_entry_target *target,
                          int numeric)
{
	const struct xt_mark_target_info_v1 *markinfo =
		(const struct xt_mark_target_info_v1 *)target->data;

	switch (markinfo->mode) {
	case XT_MARK_SET:
		printf("MARK set ");
		break;
	case XT_MARK_AND:
		printf("MARK and ");
		break;
	case XT_MARK_OR: 
		printf("MARK or ");
		break;

#if 1 //__MSTC__, ZyXEL richard, QoS
	case 4: 
		printf("vtag set ");
		break;
#endif //__MSTC__, ZyXEL richard, QoS

	}
	print_mark(markinfo->mark);
}

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void MARK_save_v1(const void *ip, const struct xt_entry_target *target)
{
	const struct xt_mark_target_info_v1 *markinfo =
		(const struct xt_mark_target_info_v1 *)target->data;

	switch (markinfo->mode) {
	case XT_MARK_SET:
		printf("--set-mark ");
		break;
	case XT_MARK_AND:
		printf("--and-mark ");
		break;
	case XT_MARK_OR: 
		printf("--or-mark ");
		break;

#if 1 //__MSTC__, ZyXEL richard, QoS
	case 4: 
		printf("--vtag-set ");
		break;
#endif //__MSTC__, ZyXEL richard, QoS

	}
	print_mark(markinfo->mark);
}

static struct xtables_target mark_target_v0 = {
	.family		= AF_INET,
	.name		= "MARK",
	.version	= IPTABLES_VERSION,
	.revision	= 0,
	.size		= XT_ALIGN(sizeof(struct xt_mark_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_mark_target_info)),
	.help		= MARK_help,
	.parse		= MARK_parse_v0,
	.final_check	= MARK_check,
	.print		= MARK_print_v0,
	.save		= MARK_save_v0,
	.extra_opts	= MARK_opts,
};

static struct xtables_target mark_target_v1 = {
	.family		= AF_INET,
	.name		= "MARK",
	.version	= IPTABLES_VERSION,
	.revision	= 1,
	.size		= XT_ALIGN(sizeof(struct xt_mark_target_info_v1)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_mark_target_info_v1)),
	.help		= MARK_help,
	.parse		= MARK_parse_v1,
	.final_check	= MARK_check,
	.print		= MARK_print_v1,
	.save		= MARK_save_v1,
	.extra_opts	= MARK_opts,
};

static struct xtables_target mark_target6_v0 = {
	.family		= AF_INET6,
	.name		= "MARK",
	.version	= IPTABLES_VERSION,
	.revision	= 0,
	.size		= XT_ALIGN(sizeof(struct xt_mark_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_mark_target_info)),
	.help		= MARK_help,
	.parse		= MARK_parse_v0,
	.final_check	= MARK_check,
	.print		= MARK_print_v0,
	.save		= MARK_save_v0,
	.extra_opts	= MARK_opts,
};

static struct xtables_target mark_target6_v1 = {
        .family         = AF_INET6,
        .name           = "MARK",
        .version        = IPTABLES_VERSION,
        .revision       = 1,
        .size           = XT_ALIGN(sizeof(struct xt_mark_target_info_v1)),
        .userspacesize  = XT_ALIGN(sizeof(struct xt_mark_target_info_v1)),
        .help           = MARK_help,
        .parse          = MARK_parse_v1,
        .final_check    = MARK_check,
        .print          = MARK_print_v1,
        .save           = MARK_save_v1,
        .extra_opts     = MARK_opts,
};


void _init(void)
{
	xtables_register_target(&mark_target_v0);
	xtables_register_target(&mark_target_v1);
	xtables_register_target(&mark_target6_v0);
        xtables_register_target(&mark_target6_v1);
}
