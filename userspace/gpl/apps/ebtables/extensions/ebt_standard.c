#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include "../include/ebtables_u.h"

static struct option opts[] =
{
	{0}
};

static void print_help()
{
	printf("Standard targets: DROP, ACCEPT, RETURN or CONTINUE;\n"
	       "The target can also be a user defined chain.\n");
}

static void init(struct ebt_entry_target *t)
{
	((struct ebt_standard_target *)t)->verdict = EBT_CONTINUE;
}

static int parse(int c, char **argv, int argc, const struct ebt_u_entry *entry,
   unsigned int *flags, struct ebt_entry_target **target)
{
	return 0;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target, const char *name,
   unsigned int hookmask, unsigned int time)
{
}

struct ebt_u_entries *nr_to_chain(int nr);

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target)
{
	int verdict = ((struct ebt_standard_target *)target)->verdict;

	if (verdict >= 0) {
		struct ebt_u_entries *entries;

		entries = nr_to_chain(verdict + NF_BR_NUMHOOKS);
		printf("%s", entries->name);
		return;
	}
	if (verdict == EBT_CONTINUE)
		printf("CONTINUE ");
	else if (verdict == EBT_ACCEPT)
		printf("ACCEPT ");
	else if (verdict == EBT_DROP)
		printf("DROP ");
	else if (verdict == EBT_RETURN)
		printf("RETURN ");
	else
		print_bug("Bad standard target");
}

static int compare(const struct ebt_entry_target *t1,
   const struct ebt_entry_target *t2)
{
	return ((struct ebt_standard_target *)t1)->verdict ==
	   ((struct ebt_standard_target *)t2)->verdict;
}

static struct ebt_u_target standard =
{
	.name		= EBT_STANDARD_TARGET,
	.size		= sizeof(struct ebt_standard_target) -
			  sizeof(struct ebt_entry_target),
	.help		= print_help,
	.init		= init,
	.parse		= parse,
	.final_check	= final_check,
	.print		= print,
	.compare	= compare,
	.extra_ops	= opts,
};

static void _init(void) __attribute__ ((constructor));
static void _init(void)
{
	register_target(&standard);
}
