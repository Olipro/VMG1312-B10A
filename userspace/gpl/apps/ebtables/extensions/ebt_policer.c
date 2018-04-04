/* Shared library add-on to ebtables to add policer support, ZyXEL Stan, 20100107 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include "../include/ebtables_u.h"
#include <linux/netfilter_bridge/ebt_policer.h>

#define EBT_POLICER_RATE_KBPS        10       /* Policer default rate in kbps. */
#define EBT_POLICER_BURST_KBYTE      10       /* Policer default burst in kbyte. */
#define EBT_POLICER_MAX_INPUT_VALUE  1000000  /* Max rate value user can input in kbps or mbps. */

#if 1//__MSTC__, Jones For compilation
#define FLAG_MODE          0x01
#define FLAG_POLICER       0x02
#define FLAG_POLICER_BURST 0x04
#define FLAG_CRATE         0x08
#define FLAG_CBS_BURST     0x10
#define FLAG_PRATE         0x20
#define FLAG_PBS_BURST     0x40
#define FLAG_EBS_BURST     0x80

#define MODE_TBF   0
#define MODE_SRTCM 1
#define MODE_TRTCM 2

static struct option opts[] = {
    { "mode",          required_argument, 0, '#' },
    { "policer",       required_argument, 0, '%' },
    { "policer-burst", required_argument, 0, '$' },
    { "crate",         required_argument, 0, '1' },
    { "cbs-burst",     required_argument, 0, '2' },
    { "prate",         required_argument, 0, '3' },
    { "pbs-burst",     required_argument, 0, '4' },
    { "ebs-burst",     required_argument, 0, '5' },
    { 0 }
};
#else
#define FLAG_POLICER		         0x01
#define FLAG_POLICER_BURST	         0x02

#define ARG_POLICER	                 '1'
#define ARG_POLICER_BURST            '2'

static struct option opts[] = {
    { "policer",		required_argument, 0, ARG_POLICER },
    { "policer-burst",	required_argument, 0, ARG_POLICER_BURST },
    { 0 }
};
#endif

#if 1//__MSTC__, Jones For compilation
/* Function which prints out usage message. */
static void print_help(void)
{
	printf(
	    "policer options:\n"
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
	    EBT_POLICER_RATE_KBPS, EBT_POLICER_BURST_KBYTE,
	    EBT_POLICER_RATE_KBPS, EBT_POLICER_BURST_KBYTE, EBT_POLICER_BURST_KBYTE,
	    EBT_POLICER_RATE_KBPS, EBT_POLICER_BURST_KBYTE, EBT_POLICER_RATE_KBPS, EBT_POLICER_BURST_KBYTE);
}
#else
/* Function which prints out usage message. */
static void print_help(void)
{
	printf(
	    "policer options:\n"
	    "--policer rate			max data rate match: default %ukbps\n"
	    "                                [Bits per second followed by kbps or mbps.\n"
	    "                                Support 1kbps to 1000000kbps or 1mbps to 1000000mbps.] \n"
	    "--policer-burst size		size to match in a burst, default %ukb\n"
	    "                                [Kilo-bytes followed by kb.\n"
	    "                                Support 1kb to 1000kb.]\n"
	    "\n", EBT_POLICER_RATE_KBPS, EBT_POLICER_BURST_KBYTE);
}
#endif

/* parse_rate(): to check the rate and preprocess the rate. */
static int parse_rate(const char *rate, u_int32_t *val)
{
	const char *kbps;
	const char *mbps;
	u_int32_t r;
	u_int32_t mult = 1;

	kbps = strstr(rate, "kbps"); /* String comparison. */
	mbps = strstr(rate, "mbps"); /* String comparison. */

	if ((!kbps && !mbps) || (kbps && mbps)) {	  	
		return 0;  
	}	
	else if (kbps) {	
		if (strlen(kbps + 4) != 0) {
			return 0;	
		}		
		mult = 1; /* kbps scale */
	}	
	else if (mbps) {
		if (strlen(mbps + 4) != 0) {
			return 0;
		}
		mult = 1000; /* mbps scale */
	}
	
	r = strtoul(rate, NULL, 0);	

	if (!r) {
		return 0;
	}

	if (r > EBT_POLICER_MAX_INPUT_VALUE) { /* prevent user enter greater than IPT_POLICER_MAX_INPUT_VALUE */
		return 0;
	}	
	*val = r * mult;
	return 1;
}

#if 1//__MSTC__, Jones For compilation
/* Initialize the match. */
static void init(struct ebt_entry_match *m)
{
	struct ebt_policer_info *r = (struct ebt_policer_info *)m->data;
	/* Default mode is TBF. */
	r->policerMode = MODE_TBF;

	/* Prepare default rate string, such as 10kbps. */
	char rate_buf[16];
	sprintf(rate_buf, "%dkbps", EBT_POLICER_RATE_KBPS); 

	parse_rate(rate_buf, &r->rate);
	r->burst = EBT_POLICER_BURST_KBYTE;
	/* For srtcm and trtcm. */
	r->pRate = r->rate;
	r->pbsBurst = EBT_POLICER_BURST_KBYTE;
#if 1 /* Init creditCap to check if the rule is new or not. __OBM__. ZyXEL, Stan Su, 20100611. */
	r->creditCap = 0;
#endif

}
/* end init */
#else
/* Initialize the match. */
static void init(struct ebt_entry_match *m)
{
	struct ebt_policer_info *r = (struct ebt_policer_info *)m->data;

	/* Prepare default rate string, such as 10kbps. */
	char rate_buf[16];
	sprintf(rate_buf, "%dkbps", EBT_POLICER_RATE_KBPS); 

	parse_rate(rate_buf, &r->avg);
	r->burst = EBT_POLICER_BURST_KBYTE;
}
#endif

#if 1//__MSTC__, Jones For compilation
static int parse(int c, char **argv, int argc,
                 const struct ebt_u_entry *entry,
                 unsigned int *flags,
                 struct ebt_entry_match **match)
{
	struct ebt_policer_info *r = (struct ebt_policer_info *)(*match)->data;
	const char *str1;
	char *remainder;

	switch(c) {
	case '#':
		/* Check Mode */
		check_option(flags, FLAG_MODE);
		if (check_inverse(optarg)) {
			print_error("Unexpected `!' after --mode");
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
			print_error("bad mode '%s'", optarg);
		}
		break;

	case '%':
		if (r->policerMode == 0) {
			/* Check parameter of tbf */
			check_option(flags, FLAG_POLICER);
			if (check_inverse(optarg)) {
				print_error("Unexpected `!' after --policer");
			}
			if (!parse_rate(optarg, &r->rate)) {
				print_error("bad rate '%s'", optarg);
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
			check_option(flags, FLAG_POLICER_BURST);
			if (check_inverse(optarg)) {
				print_error("Unexpected `!' after --policer-burst");
			}
			str1 = optarg;
			r->burst = strtoul(str1, &remainder, 0);
			if (strcmp(remainder, "kb") !=0 || r->burst > 1000 || r->burst <= 0) {
				print_error("bad --policer-burst `%s'", optarg);
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
			check_option(flags, FLAG_CRATE);
			if (check_inverse(optarg)) {
				print_error("Unexpected `!' after --crate");
			}
			if (!parse_rate(optarg, &r->rate)) {
				print_error("bad committed information rate '%s'", optarg);
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
			check_option(flags, FLAG_CBS_BURST);
			if (check_inverse(optarg)) {
				print_error("Unexpected `!' after --cbs-burst");
			}
			str1 = optarg;
			r->burst = strtoul(str1, &remainder, 0);
			if (strcmp(remainder, "kb") !=0 || r->burst > 1000 || r->burst <= 0) {
				print_error("bad --cbs-burst `%s'", optarg);
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
			check_option(flags, FLAG_PRATE);
			if (check_inverse(optarg)) {
				print_error("Unexpected `!' after --prate");
			}
			if (!parse_rate(optarg, &r->pRate)) {
				print_error("bad peak information rate '%s'", optarg);
			}
			if (r->rate > r->pRate) {
				print_error("prate msut be equal or greater than crate");
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
			check_option(flags, FLAG_PBS_BURST);
			if (check_inverse(optarg)) {
				print_error("Unexpected `!' after --pbs-burst");
			}
			str1 = optarg;
			r->pbsBurst = strtoul(str1, &remainder, 0);
			if (strcmp(remainder, "kb") !=0 || r->pbsBurst > 1000 || r->pbsBurst <= 0) {
				print_error("bad --pbs-burst `%s'", optarg);
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
			check_option(flags, FLAG_EBS_BURST);
			if (check_inverse(optarg)) {
				print_error("Unexpected `!' after --ebs-burst");
			}
			str1 = optarg;
			r->pbsBurst = strtoul(str1, &remainder, 0);
			if (strcmp(remainder, "kb") !=0 || r->pbsBurst > 1000 || r->pbsBurst <= 0) {
				print_error("bad --ebs-burst `%s'", optarg);
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

	return 1;
}
/* end parse */
#else
static int parse(int c, char **argv, int argc,
                 const struct ebt_u_entry *entry,
                 unsigned int *flags,
                 struct ebt_entry_match **match)
{
	struct ebt_policer_info *r = (struct ebt_policer_info *)(*match)->data;
	const char *str1;
	char *remainder;

	switch(c) {
	case ARG_POLICER:
		check_option(flags, FLAG_POLICER);
		if (check_inverse(optarg)) {
			print_error("Unexpected `!' after --policer");
		}
		if (!parse_rate(optarg, &r->avg)) {
			print_error("bad rate `%s'", optarg);
		}
		break;

	case ARG_POLICER_BURST:
		check_option(flags, FLAG_POLICER_BURST);
		if (check_inverse(optarg)) {
			print_error("Unexpected `!' after --policer-burst");
		}

		str1 = optarg;
		r->burst = strtol(str1, &remainder, 0);
		if (strcmp(remainder, "kb") != 0 || r->burst > 1000 || r->burst <= 0) {
			print_error("bad --policer-burst `%s'", optarg);
		}
		break;

	default:
		return 0;
	}

	return 1;
}
#endif

/* Final check; nothing. */
static void final_check(const struct ebt_u_entry *entry,
                        const struct ebt_entry_match *match, 
                        const char *name,
                        unsigned int hookmask,
                        unsigned int time)
{
	/* empty */
}

struct rates
{
    const char *name;
    u_int32_t mult;
};

static struct rates g_rates[] =
{
    { "kbps", 1 },
    { "mbps", 1000 },
    { "gbps", 1000000 },
};

static void print_rate(u_int32_t period)
{
	unsigned int i;

	for (i = 1; i < sizeof(g_rates) / sizeof(struct rates); i ++)
		if (period < g_rates[i].mult 
			    || period / g_rates[i].mult < period % g_rates[i].mult ) {
			break;
		}

	printf("%u%s ", period / g_rates[i - 1].mult , g_rates[i - 1].name);
}

#if 1//__MSTC__, Jones For compilation
/* Prints out the matchinfo. */
static void
print(const struct ebt_u_entry *entry, const struct ebt_entry_match *match)
{
	struct ebt_policer_info *r = (struct ebt_policer_info *)match->data;

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
print(const struct ebt_u_entry *entry, const struct ebt_entry_match *match)
{
	struct ebt_policer_info *r = (struct ebt_policer_info *)match->data;

	printf("policer: rate ");
	print_rate(r->avg);
	printf("burst %ukbytes ", r->burst);
}
#endif
static int compare(const struct ebt_entry_match* m1, const struct ebt_entry_match *m2)
{
	struct ebt_policer_info* li1 = (struct ebt_policer_info*)m1->data;
	struct ebt_policer_info* li2 = (struct ebt_policer_info*)m2->data;
#if 1//__MSTC__, Jones For compilation
	if (li1->rate!= li2->rate) {
		return 0;
	}
#else
       if (li1->avg != li2->avg) {
		return 0;
	}
#endif
	if (li1->burst != li2->burst) {
		return 0;
	}
	return 1;
}

static struct ebt_u_match policer_match =
{
    .name         = EBT_POLICER_MATCH,
    .size         = sizeof(struct ebt_policer_info),
    .help         = print_help,
    .init         = init,
    .parse        = parse,
    .final_check  = final_check,
    .print        = print,
    .compare      = compare,
    .extra_ops    = opts,
};

static void _init(void) __attribute((constructor));
static void _init(void)
{
	register_match(&policer_match);
}

