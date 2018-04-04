/* Shared library add-on to iptables to add CLASSIFY target support. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

//#include <iptables.h>
//#include <linux/netfilter_ipv4/ip_tables.h>
//#include <linux/netfilter_ipv4/ipt_AUTOMAP.h>
#include <xtables.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter_ipv4/ipt_AUTOMAP.h>
//#include <linux/netfilter/xt_AUTOMAP.h>
//#include <linux/types.h>
//#include <linux/pkt_sched.h>

/* Function which prints out usage message. */
static void
AUTOMAP_help(void)
{
	printf(
"AUTOMAP v%s options:\n"
" --automap-type type\n"
"				Auto priority mapping by defined type(1:802.1P 2:DSCP 4:IP Length)\n"
/*
" --automap-mark-or value\n"
"				Mark value for auto priority mapping (It will mark value for each queue from value to value+7)\n"
" --automap-dscp \n"
"				802.1P  mapping to DSCP\n"
" --automap-ethpri value\n"
"				DSCP mapping to 802.1P\n"
*/
"\n",
IPTABLES_VERSION);
}

static struct option opts[] = {
	{ "automap-type", 1, 0, '1' },
	//{ "automap-mark-or", 1, 0, '2' },	
	//{ "automap-dscp", 0, 0, '3' },	
	//{ "automap-ethpri", 0, 0, '4' },	
	{ 0 }
};

/* Initialize the target. */
static void
AUTOMAP_init(struct xt_entry_target *t, unsigned int *nfcache)
{
	struct ipt_automap_target_info *aminfo
		= (struct ipt_automap_target_info *)t->data;

	/* Setup default type	*/
	aminfo->type=AUTOMAP_TYPE_PKTLEN;
#if 1 //__MSTC__, Jones
	/* Setup default mark value	*/
	aminfo->marktable[0]=0x0;   /* Queue priority 0*/
	aminfo->marktable[1]=0x1; /* Queue priority 1*/
	aminfo->marktable[2]=0x2; /* Queue priority 2*/
	aminfo->marktable[3]=0x3; /* Queue priority 3*/
	aminfo->marktable[4]=0x4; /* Queue priority 4*/
	aminfo->marktable[5]=0x5; /* Queue priority 5*/
	aminfo->marktable[6]=0x6; /* Queue priority 6*/
	aminfo->marktable[7]=0x7; /* Queue priority 7*/
#else
	/* Setup default mark value	*/
	aminfo->marktable[0]=0x0;   /* Queue priority 0*/
	aminfo->marktable[1]=0xCC0; /* Queue priority 1*/
	aminfo->marktable[2]=0xCE0; /* Queue priority 2*/
	aminfo->marktable[3]=0xD00; /* Queue priority 3*/
	aminfo->marktable[4]=0xD20; /* Queue priority 4*/
	aminfo->marktable[5]=0xD40; /* Queue priority 5*/
	aminfo->marktable[6]=0xD60; /* Queue priority 6*/
	aminfo->marktable[7]=0xD80; /* Queue priority 7*/
#endif
}

static void
parse_automap_type(const char *typestring, struct ipt_automap_target_info *aminfo)
{
	int type;
	type = strtol(typestring, NULL, 0);
	if((type!=AUTOMAP_TYPE_DSCP)&&
		(type!=AUTOMAP_TYPE_8021P)&&
		(type!=AUTOMAP_TYPE_PKTLEN)){
			exit_error(PARAMETER_PROBLEM,
			   "Invalid mapping type (1:802.1P 2:DSCP 4:IP Length)");
	}
	aminfo->type = type;
	return;
}

void parse_automap_mark(const char *value, struct ipt_automap_target_info *aminfo)
{
	int markVal=0, i=0;

	markVal = strtol(value, NULL, 0);
	for(i=0; i<8; i++){
		*(aminfo->marktable+i)=markVal+(i<<5);
	}
	
	return;
}

/* Function which parses command options; returns true if it
   ate an option */
static int
AUTOMAP_parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
      struct xt_entry_target **target)
{
	struct ipt_automap_target_info *aminfo
		= (struct ipt_automap_target_info *)(*target)->data;
	switch (c) {
	case '1':
		if (*flags & IPT_AUTO_TYPE)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--automap-type' allowed");
		parse_automap_type(argv[optind-1], aminfo);
		aminfo->flags |= IPT_AUTO_TYPE;
		*flags |= IPT_AUTO_TYPE;
		break;
#if 0		
	case '2':
		if (*flags & IPT_AUTO_MARK)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--automap-mark-or' allowed");
		parse_automap_mark(argv[optind-1], aminfo);	
		aminfo->flags |= IPT_AUTO_MARK;
		*flags |= IPT_AUTO_MARK;
		break;
	case '3':
		if (*flags & IPT_AUTO_DSCP)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--automap-dscp' allowed");
		aminfo->flags |= IPT_AUTO_DSCP;
		*flags |= IPT_AUTO_DSCP;
		break;
	case '4':
		if (*flags & IPT_AUTO_ETHPRI)
			exit_error(PARAMETER_PROBLEM,
				   "Only one `--automap-ethpri' allowed");
		aminfo->flags |= IPT_AUTO_ETHPRI;
		*flags |= IPT_AUTO_ETHPRI;
		break;
#endif		
	default:
		return 0;
	}

	return 1;
}

static void
AUTOMAP_final_check(unsigned int flags)
{
	
}


/* Prints out the targinfo. */
static void
AUTOMAP_print(const void *ip,
      const struct xt_entry_target *target,
      int numeric)
{
	const struct ipt_automap_target_info *clinfo =
		(const struct ipt_automap_target_info *)target->data;
}
/* Prints out the matchinfo. */

/* Saves the union ipt_targinfo in parsable form to stdout. */
static void
AUTOMAP_save(const void *ip, const struct xt_entry_target *target)
{
	const struct ipt_automap_target_info *clinfo =
		(const struct ipt_automap_target_info *)target->data;
}

static struct xtables_target classify = {
	.family		= AF_INET,
	//.next		= NULL,
	.name		= "AUTOMAP",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct ipt_automap_target_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct ipt_automap_target_info)),
	.help		= AUTOMAP_help,
	.init		= AUTOMAP_init,
	.parse		= AUTOMAP_parse,
	.final_check	= AUTOMAP_final_check,
	.print		= AUTOMAP_print,
	.save		= AUTOMAP_save,
	.extra_opts	= opts,
};

void _init(void)
{
	xtables_register_target(&classify);
}
