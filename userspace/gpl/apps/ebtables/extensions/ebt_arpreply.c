#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/ether.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include <linux/netfilter_bridge/ebt_arpreply.h>

static int mac_supplied;

#define REPLY_MAC '1'
#define REPLY_TARGET '2'
static struct option opts[] =
{
	{ "arpreply-mac" ,    required_argument, 0, REPLY_MAC    },
	{ "arpreply-target" , required_argument, 0, REPLY_TARGET },
	{ 0 }
};

static void print_help()
{
	printf(
	"arpreply target options:\n"
	" --arpreply-mac address           : source MAC of generated reply\n"
	" --arpreply-target target         : ACCEPT, DROP, RETURN or CONTINUE\n"
	"                                    (standard target is DROP)\n");
}

static void init(struct ebt_entry_target *target)
{
	struct ebt_arpreply_info *replyinfo =
	   (struct ebt_arpreply_info *)target->data;

	replyinfo->target = EBT_DROP;
	memset(replyinfo->mac, 0, ETH_ALEN);
	mac_supplied = 0;
}

#define OPT_REPLY_MAC     0x01
#define OPT_REPLY_TARGET  0x02
static int parse(int c, char **argv, int argc,
   const struct ebt_u_entry *entry, unsigned int *flags,
   struct ebt_entry_target **target)
{
	struct ebt_arpreply_info *replyinfo =
	   (struct ebt_arpreply_info *)(*target)->data;
	struct ether_addr *addr;

	switch (c) {
	case REPLY_MAC:
		check_option(flags, OPT_REPLY_MAC);
		if (!(addr = ether_aton(optarg)))
			print_error("Problem with specified "
			            "--arpreply-mac mac");
		memcpy(replyinfo->mac, addr, ETH_ALEN);
		mac_supplied = 1;
		break;
	case REPLY_TARGET:
		check_option(flags, OPT_REPLY_TARGET);
		if (FILL_TARGET(optarg, replyinfo->target))
			print_error("Illegal --arpreply-target target");
		break;

	default:
		return 0;
	}
	return 1;
}

static void final_check(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target, const char *name,
   unsigned int hookmask, unsigned int time)
{
	struct ebt_arpreply_info *replyinfo =
	   (struct ebt_arpreply_info *)target->data;

	if (entry->ethproto != ETH_P_ARP || entry->invflags & EBT_IPROTO)
		print_error("For ARP replying the protocol must be "
		            "specified as ARP");
	if (time == 0 && mac_supplied == 0)
		print_error("No arpreply mac supplied");
	if (BASE_CHAIN && replyinfo->target == EBT_RETURN)
		print_error("--arpreply-target RETURN not allowed on "
		            "base chain");
	CLEAR_BASE_CHAIN_BIT;
	if (strcmp(name, "nat") || hookmask & ~(1 << NF_BR_PRE_ROUTING))
		print_error("arpreply only allowed in PREROUTING");
}

static void print(const struct ebt_u_entry *entry,
   const struct ebt_entry_target *target)
{
	struct ebt_arpreply_info *replyinfo =
	   (struct ebt_arpreply_info *)target->data;

	printf("--arpreply-mac ");
	print_mac(replyinfo->mac);
	if (replyinfo->target == EBT_DROP)
		return;
	printf(" --arpreply-target %s", TARGET_NAME(replyinfo->target));
}

static int compare(const struct ebt_entry_target *t1,
   const struct ebt_entry_target *t2)
{
	struct ebt_arpreply_info *replyinfo1 =
	   (struct ebt_arpreply_info *)t1->data;
	struct ebt_arpreply_info *replyinfo2 =
	   (struct ebt_arpreply_info *)t2->data;

	return memcmp(replyinfo1->mac, replyinfo2->mac, ETH_ALEN) == 0
		&& replyinfo1->target == replyinfo2->target;
}

static struct ebt_u_target arpreply_target =
{
	.name		= EBT_ARPREPLY_TARGET,
	.size		= sizeof(struct ebt_arpreply_info),
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
	register_target(&arpreply_target);
}
