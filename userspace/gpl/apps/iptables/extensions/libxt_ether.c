/* Shared library add-on to iptables to add MAC address support. */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#if defined(__GLIBC__) && __GLIBC__ == 2
#include <net/ethernet.h>
#else
#include <linux/if_ether.h>
#endif
#include <xtables.h>
#include <stddef.h>
#include <linux/netfilter/x_tables.h>
/* For 64bit kernel / 32bit userspace */
#include <linux/netfilter/xt_ether.h>

/* Function which prints out usage message. */
static void
ether_help(void)
{
	printf(
"Ether v%s options:\n"
" --ether-type [!] protocol(protocol hexadecimal) : Match ether type of the packet\n"
" --ether-8021q [!] vlan idl(vlan id hexadecimal) : Match vlan id(802.1Q) of the packet\n"
" --ether-8021p [!] vlan priority(vlan priority hexadecimal) : Match vlan priority(802.1P) of the packet\n"
"\n", IPTABLES_VERSION);
}

static const struct option ether_opts[] = {
	{ "ether-type", 1, NULL, 'F' },
	{ "ether-8021q", 1, NULL, 'G' },
	{ "ether-8021p", 1, NULL, 'H' },
	{ }
};

static void
parse_ether_type(const char *ethertype, struct xt_ether_info *info)
{
	info->ethertype = strtol(ethertype, NULL, 0);
}

static void
parse_ether_8021q(const char *vlanid, struct xt_ether_info *info)
{
	info->vlanid = strtol(vlanid, NULL, 0);
}

static void
parse_ether_8021p(const char *vlanpriority, struct xt_ether_info *info){
	info->vlanpriority = strtol(vlanpriority, NULL, 0);
}

/* Function which parses command options; returns true if it
   ate an option */
static int
ether_parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry, struct xt_entry_match **match)
{
	struct xt_ether_info *etherinfo = (struct xt_ether_info *)(*match)->data;

	switch (c) {
	case 'F':
#if 0 //__MSTC__, JEff
		if (check_inverse(optarg, &invert, &optind, 0)) {
           printf("ether-type parameter '%s'", optarg);
           printf("ether-type parameter '%s'", argv[optind-1]);
			break;
		}
#else
		check_inverse(optarg, &invert, &optind, 0);
#endif
		parse_ether_type(argv[optind-1], etherinfo);
		etherinfo->bitmask |= IPT_ETHER_TYPE;
		if (invert)
			etherinfo->invert |= IPT_ETHER_TYPE;
		*flags |= IPT_ETHER_TYPE;
		break;
	case 'G':
#if 0 //__MSTC__, Jeff
		if (check_inverse(optarg, &invert, &optind, 0)) {
          printf("ether-8021q parameter '%s'", optarg);
           printf("ether-8021q parameter '%s'", argv[optind-1]);
			break;
		}
#else
		check_inverse(optarg, &invert, &optind, 0);
#endif
		parse_ether_8021q(argv[optind-1], etherinfo);
		etherinfo->bitmask |= IPT_ETHER_8021Q;
		if (invert)
			etherinfo->invert |= IPT_ETHER_8021Q;
		*flags |= IPT_ETHER_8021Q;
		break;
	case 'H':
#if 0 //__MSTC__, Jeff
		if (check_inverse(optarg, &invert, &optind, 0)) {
          printf("ether-8021p parameter '%s'", optarg);
           printf("ether-8021p parameter '%s'", argv[optind-1]);
			break;
		}
#else
		check_inverse(optarg, &invert, &optind, 0);
#endif
		parse_ether_8021p(argv[optind-1], etherinfo);
		etherinfo->bitmask |= IPT_ETHER_8021P;
		if (invert)
			etherinfo->invert |= IPT_ETHER_8021P;
		*flags |= IPT_ETHER_8021P;
		break;	
	default:
		return 0;
	}
	return 1;
}

static void print_ether_type(struct xt_ether_info *info)
{
	if(info->invert & IPT_ETHER_TYPE)
		printf("! ");
	printf("0x%x", info->ethertype);
	printf(" ");
}

static void print_ether_8021q(struct xt_ether_info *info)
{
	if(info->invert & IPT_ETHER_8021Q)
		printf("! ");
	printf("0x%x", info->vlanid);
	printf(" ");
}

static void print_ether_8021p(struct xt_ether_info *info)
{
	if(info->invert & IPT_ETHER_TYPE)
		printf("! ");
	printf("0x%x", info->vlanpriority);
	printf(" ");
}

/* Final check; must have specified --mac. */
static void ether_final_check(unsigned int flags)
{
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "You must specify one or more options'");

}

/* Prints out the matchinfo. */
static void
ether_print(const void *ip,
      const struct xt_entry_match *match,
      int numeric)
{
	struct xt_ether_info *ether_info;
	ether_info = (struct xt_ether_info *)match->data;
	if(ether_info->bitmask & IPT_ETHER_TYPE){
		printf("EtherType ");
		print_ether_type(ether_info);
	}
	if(ether_info->bitmask & IPT_ETHER_8021Q){
		printf("8021Q ");
		print_ether_8021q(ether_info);
		}
	if(ether_info->bitmask & IPT_ETHER_8021P){
		printf("8021P ");
		print_ether_8021p(ether_info);
	}
}

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void ether_save(const void *ip, const struct xt_entry_match *match)
{

	struct xt_ether_info *ether_info;
	ether_info = (struct xt_ether_info *)match->data;
  
	if(ether_info->bitmask & IPT_ETHER_TYPE){
		printf("--ether-type ");
		print_ether_type(ether_info);
	}
	if(ether_info->bitmask & IPT_ETHER_8021Q){
		printf("--ether-8021q ");
		print_ether_8021q(ether_info);
		}
	if(ether_info->bitmask & IPT_ETHER_8021P){
		printf("--ether-8021p ");
		print_ether_8021p(ether_info);
	}
}

static struct xtables_match ether_match = { 
	.family		= AF_INET, 	
 	.name		= "ether",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_ether_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_ether_info)),
	.help		= ether_help,
	.parse		= ether_parse,
	.final_check	= ether_final_check,
	.print		= ether_print,
	.save		= ether_save,
	.extra_opts	= ether_opts
};

static struct xtables_match ether_match6 = { 
	.family		= AF_INET6,
 	.name		= "ether",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_ether_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_ether_info)),
	.help		= ether_help,
	.parse		= ether_parse,
	.final_check	= ether_final_check,
	.print		= ether_print,
	.save		= ether_save,
	.extra_opts	= ether_opts
};

void _init(void)
{
	xtables_register_match(&ether_match);
	xtables_register_match(&ether_match6);
}
