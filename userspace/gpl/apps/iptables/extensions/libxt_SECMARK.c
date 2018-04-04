/*
 * Shared library add-on to iptables to add SECMARK target support.
 *
 * Based on the MARK target.
 *
 * Copyright (C) 2006 Red Hat, Inc., James Morris <jmorris@redhat.com>
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <xtables.h>
#include <linux/netfilter/xt_SECMARK.h>

#define PFX "SECMARK target: "

static void SECMARK_help(void)
{
	printf(
"SECMARK target v%s options:\n"
"  --selctx value                     Set the SELinux security context\n"
"\n",
IPTABLES_VERSION);
}

static const struct option SECMARK_opts[] = {
	{ "selctx", 1, 0, '1' },
	{ 0 }
};

/*
 * Function which parses command options; returns true if it
 * ate an option.
 */
static int SECMARK_parse(int c, char **argv, int invert, unsigned int *flags,
                         const void *entry, struct xt_entry_target **target)
{
	struct xt_secmark_target_info *info =
		(struct xt_secmark_target_info*)(*target)->data;

	switch (c) {
	case '1':
		if (*flags & SECMARK_MODE_SEL)
			exit_error(PARAMETER_PROBLEM, PFX
				   "Can't specify --selctx twice");
		info->mode = SECMARK_MODE_SEL;

		if (strlen(optarg) > SECMARK_SELCTX_MAX-1)
			exit_error(PARAMETER_PROBLEM, PFX
				   "Maximum length %u exceeded by --selctx"
				   " parameter (%zu)",
				   SECMARK_SELCTX_MAX-1, strlen(optarg));

		strcpy(info->u.sel.selctx, optarg);
		*flags |= SECMARK_MODE_SEL;
		break;
	default:
		return 0;
	}

	return 1;
}

static void SECMARK_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM, PFX "parameter required");
}

static void print_secmark(struct xt_secmark_target_info *info)
{
	switch (info->mode) {
	case SECMARK_MODE_SEL:
		printf("selctx %s ", info->u.sel.selctx);\
		break;
	
	default:
		exit_error(OTHER_PROBLEM, PFX "invalid mode %hhu\n", info->mode);
	}
}

static void SECMARK_print(const void *ip, const struct xt_entry_target *target,
                          int numeric)
{
	struct xt_secmark_target_info *info =
		(struct xt_secmark_target_info*)(target)->data;

	printf("SECMARK ");
	print_secmark(info);
}

/* Saves the target info in parsable form to stdout. */
static void SECMARK_save(const void *ip, const struct xt_entry_target *target)
{
	struct xt_secmark_target_info *info =
		(struct xt_secmark_target_info*)target->data;

	printf("--");
	print_secmark(info);
}

static struct xtables_target secmark_target = {
	.family		= AF_INET,
	.name		= "SECMARK",
	.version	= IPTABLES_VERSION,
	.revision	= 0,
	.size		= XT_ALIGN(sizeof(struct xt_secmark_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_secmark_target_info)),
	.help		= SECMARK_help,
	.parse		= SECMARK_parse,
	.final_check	= SECMARK_check,
	.print		= SECMARK_print,
	.save		= SECMARK_save,
	.extra_opts	= SECMARK_opts,
};

static struct xtables_target secmark_target6 = {
	.family		= AF_INET6,
	.name		= "SECMARK",
	.version	= IPTABLES_VERSION,
	.revision	= 0,
	.size		= XT_ALIGN(sizeof(struct xt_secmark_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_secmark_target_info)),
	.help		= SECMARK_help,
	.parse		= SECMARK_parse,
	.final_check	= SECMARK_check,
	.print		= SECMARK_print,
	.save		= SECMARK_save,
	.extra_opts	= SECMARK_opts,
};

void _init(void)
{
	xtables_register_target(&secmark_target);
	xtables_register_target(&secmark_target6);
}
