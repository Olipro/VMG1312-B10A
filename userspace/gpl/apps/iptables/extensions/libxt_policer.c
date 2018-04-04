/* Shared library add-on to iptables to add policer support, ZyXEL Birken, 20100107 */
/* __MSTC__, richard, QoS
 *  Change file name from libipt_policer.c to libxt_policer.c
 *  Change module struct from iptables_match to xtables_match
 *  Reference from brcm 405L04 libxt_limit.c
 */
#include <stdio.h>
#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>

#include <xtables.h>
#if 1 //__MSTC__, richard, QoS
#include <stddef.h>
#include <linux/netfilter/x_tables.h>
#endif //__MSTC__, richard, QoS
/* For 64bit kernel / 32bit userspace */
#include "../include/linux/netfilter/xt_policer.h"

#define IPT_POLICER_RATE_KBPS        10       /* Policer default rate in kbps. */
#define IPT_POLICER_BURST_KBYTE      10       /* Policer default burst in kbyte. */
#define IPT_POLICER_MAX_INPUT_VALUE  1000000  /* Max rate value user can input in kbps or mbps. */

#if 1//__MSTC__, Jones For compilation
#define MODE_TBF   0
#define MODE_SRTCM 1
#define MODE_TRTCM 2
/* Function which prints out usage message. */
static void policer_help(void)
{
	printf(
	    "policer v%s options:\n"
	    "--mode name                     mode name match, default is tbf.\n"
	    "                                If you want to use tbf mode, you can skip this option.\n"
	    "                                [Support tbf, srtcm, trtcm.]\n" 
	    "tbf mode: \n"
	    "--policer rate			max data rate match, default %ukbps\n"
	    "                                [Bits per second followed by kbps or mbps.\n"
	    "                                Support 1kbps to 1000000kbps or 1mbps to 1000000mbps.] \n"   
	    "--policer-burst size		size to match in a burst, default %ukb\n"
	    "                                [Kilo-bytes followed by kb.\n"
	    "                                Support 1kb to 1000kb.]\n" 
	    "srtcm mode: \n"
	    "The nfmark field of red packet is marked as 0x10000, \n"
	    "yellow packet is 0x20000 and green packet is 0x30000.\n"
	    "--crate rate			committed data rate match, default %ukbps\n" 
	    "--cbs-burst size		size to match in CBS burst, default %ukb\n"
	    "--ebs-burst size		size to match in EBS burst, default %ukb\n"
	    "trtcm mode: \n"
	    "The nfmark field of red packet is marked as 0x10000, \n"
	    "yellow packet is 0x20000 and green packet is 0x30000.\n"
	    "--crate rate			committed data rate match, default %ukbps\n" 
	    "--cbs-burst size		size to match in CBS burst, default %ukb\n"
	    "--prate rate			peak data rate match, default %ukbps\n"
	    "                                [Msut be equal or greater than crate.]\n"
	    "--pbs-burst size		size to match in PBS burst, default %ukb\n\n",
	    IPTABLES_VERSION, IPT_POLICER_RATE_KBPS, IPT_POLICER_BURST_KBYTE,
	    IPT_POLICER_RATE_KBPS, IPT_POLICER_BURST_KBYTE, IPT_POLICER_BURST_KBYTE,
	    IPT_POLICER_RATE_KBPS, IPT_POLICER_BURST_KBYTE, IPT_POLICER_RATE_KBPS, IPT_POLICER_BURST_KBYTE);
}
#else
/* Function which prints out usage message. */
static void policer_help(void)
{
	printf(
	    "policer v%s options:\n"
	    "--policer rate			max data rate match, default %ukbps\n"
	    "                                [Bits per second followed by kbps or mbps.\n"
	    "                                Support 1kbps to 1000000kbps or 1mbps to 1000000mbps.] \n"   
	    "--policer-burst size		size to match in a burst, default %ukb\n"
	    "                                [Kilo-bytes followed by kb.\n"
	    "                                Support 1kb to 1000kb.]\n" 
	    "\n", IPTABLES_VERSION, IPT_POLICER_RATE_KBPS, IPT_POLICER_BURST_KBYTE);
}
#endif

#if 1//__MSTC__, Jones For compilation
static const struct option policer_opts[] = {
    { "mode",          1, 0, '#' },
    { "policer",       1, 0, '%' },
    { "policer-burst", 1, 0, '$' },
    { "crate",         1, 0, '1' },
    { "cbs-burst",     1, 0, '2' },
    { "prate",         1, 0, '3' },
    { "pbs-burst",     1, 0, '4' },
    { "ebs-burst",     1, 0, '5' },
    { 0 }
};
#else
static const struct option policer_opts[] = {
   { "policer", 1, NULL, '%' },
   { "policer-burst", 1, NULL, '$' },
   { 0 }
};
#endif

static int
#if 1//__MSTC__, Jones For compilation
parse_rate(const char *rate, u_int32_t *val)
#else
parse_rate(const char *rate, struct xt_policer_info *val)
#endif
{
	const char *kbps;				
	const char *mbps;			
	u_int32_t r;
	u_int32_t mult = 1;	 			 

	kbps = strstr(rate, "kbps"); /*the string comparation*/
	mbps = strstr(rate, "mbps"); /*the string comparation*/
	if ((!kbps && !mbps) || (kbps && mbps)) {
		return 0;
	}
	else if (kbps) {
		if (strlen(kbps + 4) != 0) {
			return 0;
		}
		mult = 1;                /*kbps scale*/
	}
	else if (mbps){
		if (strlen(mbps + 4) != 0) {
			return 0;
		}
		mult = 1000;             /*mbps scale*/
	}		
	r = strtoul(rate, NULL, 0);						
	if (!r) {
		return 0;
	}

	if ( r > IPT_POLICER_MAX_INPUT_VALUE) { /* prevent user enter greater than IPT_POLICER_MAX_INPUT_VALUE */
		exit_error(PARAMETER_PROBLEM, "Rate too fast '%s'\n", rate);
	}
#if 1//__MSTC__, Jones For compilation
	*val = r * mult ;
#else
	val->avg = r * mult;
#endif
	return 1;
}

#if 1//__MSTC__, Jones For compilation
/* Initialize the match. */
static void policer_init(struct xt_entry_match *m)
{
	struct xt_policer_info *r = (struct xt_policer_info *)m->data;
	/* Default mode is TBF. */
	r->policerMode = MODE_TBF;

	/* Prepare default rate string, such as 10kbps. */
	char rate_buf[16];
	sprintf(rate_buf, "%dkbps", IPT_POLICER_RATE_KBPS);

	parse_rate(rate_buf, &r->rate);
	r->burst = IPT_POLICER_BURST_KBYTE;
	/* For srtcm and trtcm. */
	r->pRate = r->rate;
	r->pbsBurst = IPT_POLICER_BURST_KBYTE;
#if 1 /* Init creditCap to check if the rule is new or not. __OBM__. ZyXEL, Stan Su, 20100611. */
	r->creditCap = 0;
#endif
}
/* end init */
#else
/* Initialize the match. */
static void policer_init(struct xt_entry_match *m)
{
   struct xt_policer_info *r = (struct xt_policer_info *)m->data;

	/* Prepare default rate string, such as 10kbps. */
	char rate_buf[16];
	sprintf(rate_buf, "%dkbps", IPT_POLICER_RATE_KBPS);
   parse_rate(rate_buf, r);

	r->burst = IPT_POLICER_BURST_KBYTE;
}
#endif

#if 1//__MSTC__, Jones For compilation
/* Function which parses command options; returns true if it
   ate an option */
static int
policer_parse(int c, char **argv, int invert, unsigned int *flags,
           const void *entry, struct xt_entry_match **match)
{
	struct xt_policer_info *r = (struct xt_policer_info *)(*match)->data;
	const char *str1;
	char *remainder;
	
	switch(c) {
	case '#':
		/* Check Mode */
		if (check_inverse(argv[optind-1], &invert, &optind, 0)) {
			break;
		}
		if (strcmp(optarg, "tbf") == 0) {
			r->policerMode = MODE_TBF;
		}
		else if (strcmp(optarg, "srtcm") == 0) {
			r->policerMode = MODE_SRTCM;
		}
		else if (strcmp(optarg, "trtcm") == 0) {
			r->policerMode = MODE_TRTCM;
		}
		else {
			exit_error(PARAMETER_PROBLEM,
			    "bad mode '%s'", optarg);
		}
		break;
		
	case '%':
		if (r->policerMode == 0) {
			/* Check parameter of tbf */
			if (check_inverse(argv[optind-1], &invert, &optind, 0)) { 
				break;
			}
			if (!parse_rate(optarg, &r->rate)) {
				exit_error(PARAMETER_PROBLEM,
				    "bad rate '%s'", optarg);
			}
			break;
		}
		else {
			return 0;
			break;
		}	

	case '$':
		if (r->policerMode == 0) {
			/* Check parameter of tbf */
			if (check_inverse(argv[optind-1], &invert, &optind, 0)) {
				break;
			}
			str1 = optarg;
			r->burst = strtoul(str1, &remainder, 0);
			if (strcmp(remainder, "kb") !=0 || r->burst > 1000 || r->burst <= 0) {
				exit_error(PARAMETER_PROBLEM,
				    "bad --policer-burst `%s'", optarg);
			}
			break;
		}
		else {
			return 0;
			break;
		}

	case '1':
		if (r->policerMode == 1 || r->policerMode == 2) {
			/* Check parameter of srtcm or trtcm */
			if (check_inverse(argv[optind-1], &invert, &optind, 0)) { 
				break;
			}
			if (!parse_rate(optarg, &r->rate)) {
				exit_error(PARAMETER_PROBLEM,
				    "bad committed information rate '%s'", optarg);
			}
			break;
		}
		else {
			return 0;
			break;
		}

	case '2':
		if (r->policerMode == 1 || r->policerMode == 2) {
			/* Check parameter of srtcm or trtcm */
			if (check_inverse(argv[optind-1], &invert, &optind, 0)) {
				break;
			}
			str1 = optarg;
			r->burst = strtoul(str1, &remainder, 0);
			if (strcmp(remainder, "kb") !=0 || r->burst > 1000 || r->burst <= 0) {
				exit_error(PARAMETER_PROBLEM,
				    "bad --cbs-burst `%s'", optarg);
			}
			break;
		}
		else {
			return 0;
			break;
		}

	case '3':
		if (r->policerMode == 2) {
			/* Check parameter of trtcm */
			if (check_inverse(argv[optind-1], &invert, &optind, 0)) { 
				break;
			}
			if (!parse_rate(optarg, &r->pRate)) {
				exit_error(PARAMETER_PROBLEM,
				    "bad peak information rate '%s'", optarg);
			}			
			if (r->rate > r->pRate) {
				exit_error(PARAMETER_PROBLEM,
				    "prate msut be equal or greater than crate");
			}
			break;
		}
		else {
			return 0;
			break;
		}

	case '4':
		if (r->policerMode == 2) {
			/* Check parameter of trtcm */
			if (check_inverse(argv[optind-1], &invert, &optind, 0)) {
				break;
			}
			str1 = optarg;
			r->pbsBurst = strtoul(str1, &remainder, 0);
			if (strcmp(remainder, "kb") !=0 || r->pbsBurst > 1000 || r->pbsBurst <= 0) {
				exit_error(PARAMETER_PROBLEM,
				    "bad --pbs-burst `%s'", optarg);
			}
			break;
		}
		else {
			return 0;
			break;
		}

	case '5':
		if (r->policerMode == 1) {
			/* Check parameter of srtcm */
			if (check_inverse(argv[optind-1], &invert, &optind, 0)) {
				break;
			}
			str1 = optarg;
			r->pbsBurst = strtoul(str1, &remainder, 0);
			if (strcmp(remainder, "kb") !=0 || r->pbsBurst > 1000 || r->pbsBurst <= 0) {
				exit_error(PARAMETER_PROBLEM,
				    "bad --ebs-burst `%s'", optarg);
			}
			break;
		}
		else {
			return 0;
			break;
		}
		
	default:
		return 0;
	}

	if (invert) {
		exit_error(PARAMETER_PROBLEM,
		    "policer does not support invert");
	}

	return 1;
}
/* end parse */
#else
/* Function which parses command options; returns true if it
   ate an option */
static int
policer_parse(int c, char **argv, int invert, unsigned int *flags,
           const void *entry, struct xt_entry_match **match)
{
	struct xt_policer_info *r = (struct xt_policer_info *)(*match)->data;
   
	const char *str1;
	char *remainder;
	switch(c) {
	case '%':
		if (check_inverse(argv[optind-1], &invert, &optind, 0)) { 
			break;
		}
#if 0 //__MSTC__, richard, QoS
		if (!parse_rate(optarg, &(r->avg))) {
#else
      if (!parse_rate(optarg, r)) {
#endif //__MSTC__, richard, QoS
			exit_error(PARAMETER_PROBLEM,
			    "bad rate '%s'", optarg);
		}
      *flags = 1;
		break;

	case '$':
		if (check_inverse(argv[optind-1], &invert, &optind, 0)) {
			break;
		}
		str1 = optarg;
		r->burst = strtoul(str1, &remainder, 0);
		if (strcmp(remainder, "kb") !=0 || r->burst > 1000 || r->burst <= 0) {
			exit_error(PARAMETER_PROBLEM,
			    "bad --policer-burst `%s'", optarg);
		}
      *flags = 1;
		break;

	default:
		return 0;
	}

	if (invert) {
		exit_error(PARAMETER_PROBLEM,
		    "policer does not support invert");
	}

	return 1;

}
#endif

/* Final check; must have specified --policer. */
static void policer_check(unsigned int flags)
{
#if 0//__MSTC__, Jones For compilation
	if (!flags)
		exit_error(PARAMETER_PROBLEM,
			   "POLICER match: You must specify `--policer'");
#endif
}

static struct rates {
   const char *name;
	u_int32_t mult;
} rates[] = { { "kbps", 1 },
              { "mbps", 1000 },
              { "gbps", 1000000}};

static void print_rate(u_int32_t period)
{
	unsigned int i;

	for (i = 1; i < sizeof(rates) / sizeof(struct rates); i++) {
		if (period < rates[i].mult
                || period / rates[i].mult < period % rates[i].mult) {
			break;
		}
	}

	printf("%u%s ", period / rates[i - 1].mult, rates[i - 1].name);
}

#if 1//__MSTC__, Jones For compilation
/* Prints out the matchinfo. */
static void policer_print(const struct ipt_ip *ip,
      const struct xt_entry_match *match,
      int numeric) 
{
	struct xt_policer_info *r = (struct xt_policer_info *)match->data;

	switch(r->policerMode) {
	case MODE_TBF:
		printf("policer: rate ");
		print_rate(r->rate);
		printf("burst %ukbytes ", r->burst);
		break;

	case MODE_SRTCM:
		printf("srtcm: cRate ");
		print_rate(r->rate);
		printf("cbs-burst %ukbytes ", r->burst);
		printf("ebs-burst %ukbytes ", r->pbsBurst);
		break;
	
	case MODE_TRTCM:
		printf("trtcm: cRate ");
		print_rate(r->rate);
		printf("cbs-burst %ukbytes ", r->burst);
		printf("pRate ");
		print_rate(r->pRate);
		printf("pbs-burst %ukbytes ", r->pbsBurst);	
		break;
	}
}
/* end print */
#else
/* Prints out the matchinfo. */
static void
policer_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
   struct xt_policer_info *r = (struct xt_policer_info *)match->data;
   printf("policer: rate ");
   print_rate(r->avg);
   printf("burst %ukbytes ", r->burst);
}
#endif

/* Saves the union ipt_matchinfo in parsable form to stdout. */
static void
policer_save(const void *ip, const struct xt_entry_match *match)
{
	struct xt_policer_info *r = (struct xt_policer_info *)match->data;

	printf("--policer ");
#if 1//__MSTC__, Jones For compilation
	print_rate(r->rate);
#else
	print_rate(r->avg);
#endif
	if (r->burst != IPT_POLICER_BURST_KBYTE) {
		printf("--policer-burst %ukbytes ", r->burst);
	}
}

static struct xtables_match policer_match = {
	.family		= AF_INET,
	.name		= "policer",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_policer_info)),
	.userspacesize	= offsetof(struct xt_policer_info, prev),
	.help		= policer_help,
	.parse		= policer_parse,
	.final_check	= policer_check,
	.print		= policer_print,
	.save		= policer_save,
	.extra_opts	= policer_opts,
};

static struct xtables_match policer_match6 = {
	.family		= AF_INET6,
	.name		= "policer",
	.version	= IPTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_policer_info)),
	.userspacesize	= offsetof(struct xt_policer_info, prev),
	.help		= policer_help,
	.init    = policer_init,
	.parse		= policer_parse,
	.final_check	= policer_check,
	.print		= policer_print,
	.save		= policer_save,
	.extra_opts	= policer_opts,
};

void _init(void)
{
	xtables_register_match(&policer_match);
	xtables_register_match(&policer_match6);
}
