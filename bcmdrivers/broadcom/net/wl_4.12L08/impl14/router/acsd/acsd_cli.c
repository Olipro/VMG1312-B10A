/*
 * Frontend command-line utility client for ACSD
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: acsd_cli.c 357088 2012-09-15 03:30:01Z $
 */

#include <ctype.h>
#include <netdb.h>

#include "acsd.h"

typedef struct acsdc_info {
	int version;
	int conn_fd;
	char ifname[IFNAMSIZ];
	char cmd_buf[ACSD_BUFSIZE_4K];
	char cur_cmd[128];
	char param[128];
	unsigned int cmd_size;
} acsdc_info_t;

acsdc_info_t acsdc_info;

/* command line argument usage */
#define CMD_ERR	-1	/* Error for command */
#define CMD_OPT	0	/* a command line option */
#define CMD_ACS	1	/* the start of a wl command */

#ifdef DSLCPE
#define DEFAULT_IFNAME "wl0"
#else
#define DEFAULT_IFNAME "eth1"
#endif /* DSLCPE */
/*
 * Print the usage of acsd_cli utility
 */
void
usagec(void)
{
	fprintf(stderr,
		"acsd client utility for auto channel management\n"
		"Options:\n"
		"   info     	-Show all the related general information on server\n"
		"   csscan   	-Trigger a CS scan (without selecting a new channel) \n"
		"   autochannel -Trigger a CS scan, select a channel \n"
		"   dump     	-Dump intermedia results on the server side\n"
		"   serv     	-Specify the IP address and port number of the server\n"
		"			 \n"
		"usage: acs_cli [-i ifname] <command> [serv ipaddr:port]\n"
		"   <command>:	[info] | [dump name] | csscan |\n"
		"			    [autochannel] \n"
		"NOTE:- Start the acsd on target to use this command\n");
		exit(0);
}

/*
 * connects by default to 0.0.0.0:5312
 */
static int
acsdc_connect_server(char **argv)
{
	struct sockaddr_in servaddr;
	unsigned int port = ACSD_DFLT_CLI_PORT;
	char *server_addr = "0.0.0.0", *addr;

	/* Get IP addr and port info from command line, if available */
	if (*++argv) {
		if (!strcmp(*argv, "serv")) {
			if (*++argv) {
				addr = strsep(argv, ":");
				if (addr) {
					server_addr = addr;
				}
				if (*argv && **argv) {
					port = strtol(*argv, NULL, 0);
				}
			} else usagec();
		}
	}

	memset(&servaddr, 0, sizeof(servaddr));
	if ((acsdc_info.conn_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		printf("socekt failed: %s\n", strerror(errno));
		return -1;
	}

	servaddr.sin_family = AF_INET;
	servaddr.sin_port = htons(port);

	if (isalpha(server_addr[0]) || (inet_pton(AF_INET,
		(const char *)server_addr, &servaddr.sin_addr) == 0)) {
		printf("Invalid IPADDR: %s\n", server_addr);
		usagec();
	}

	if (connect(acsdc_info.conn_fd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0) {
		printf("connect failed: %s\n", strerror(errno));
		return -1;
	}

	return 0;
}

/* parse/validate the command line arguments */
/*
 * pargv is updated upon return if the first argument is an option.
 * It remains intact otherwise.
 */
static int
acs_option(char ***pargv, char **pifname, int *phelp)
{
	char *ifname = NULL;
	int help = FALSE;
	int status = CMD_OPT;
	char **argv = *pargv;

	while (*argv) {
		/* select different netif */
		if (!strcmp(*argv, "-i")) {
			char *opt = *argv++;
			ifname = *argv;
			if (!ifname) {
				fprintf(stderr,
					"error: expected interface name after option %s\n", opt);
				status = CMD_ERR;
				break;
			}
		}

		/* command usage */
		else if (!strcmp(*argv, "-h") || !strcmp(*argv, "--help"))
			help = TRUE;

		/* start of non wl options */
		else {
			status = CMD_ACS;
			break;
		}

		/* consume the argument */
		argv ++;
		break;
	}

	*phelp = help;
	*pifname = ifname;
	*pargv = argv;

	return status;
}

static void
acsdc_get_set(char **argv, const char * cmd, char* buf)
{
	if (*++argv) {
		/* set command */
		char *s = *argv;
		char *endptr = NULL;
		uint val = strtoul(s, &endptr, 0);
		sprintf(acsdc_info.cur_cmd, "set");
		sprintf(acsdc_info.param, cmd);
		acsdc_info.cmd_size = sprintf(buf, "%s&param=%s&val=%d&ifname=%s",
			acsdc_info.cur_cmd,	acsdc_info.param, htonl(val),
			acsdc_info.ifname) + 1;
	} else {
		argv--;
		sprintf(acsdc_info.cur_cmd, "get");
		sprintf(acsdc_info.param, cmd);
		acsdc_info.cmd_size = sprintf(buf, "%s&param=%s&ifname=%s",
			acsdc_info.cur_cmd, acsdc_info.param, acsdc_info.ifname) + 1;
	}
}

/*
 * Based on the acs_cli arguments, prepare equivalent
 * command for the server.
 */
static int
acsdc_make_cmd(char ***arg)
{
	char * buf = acsdc_info.cmd_buf;
	char **argv = *arg;

	if (!*argv)
		usagec();

	/* Process the arguments */
	if (!strcmp(*argv, "dump")) {
		if (*++argv) {
			sprintf(acsdc_info.cur_cmd, "dump");
			sprintf(acsdc_info.param, "%s", *argv);
			acsdc_info.cmd_size =
				sprintf(buf, "%s&param=%s&ifname=%s", acsdc_info.cur_cmd,
				acsdc_info.param, acsdc_info.ifname) + 1;
		} else usagec();
	} else if (!strcmp(*argv, "csscan")) {
		sprintf(acsdc_info.cur_cmd, "csscan");
		acsdc_info.cmd_size = sprintf(buf, "%s&ifname=%s", acsdc_info.cur_cmd,
			acsdc_info.ifname) + 1;
#ifdef MSTC_WLAN_CHSCAN // __MSTC__, Paul Ho, for channel scan, only scan and build the candidate table
        /* add command "csscan_MTS" in order to generate candidates table */
	} else if (!strcmp(*argv, "csscan_MTS")) {
		sprintf(acsdc_info.cur_cmd, "csscan_MTS");
		acsdc_info.cmd_size = sprintf(buf, "%s&ifname=%s", acsdc_info.cur_cmd,
			acsdc_info.ifname) + 1;
#endif /* MSTC_WLAN_CHSCAN */	
	} else if (!strcmp(*argv, "autochannel")) {
		sprintf(acsdc_info.cur_cmd, "autochannel");
		acsdc_info.cmd_size = sprintf(buf, "%s&ifname=%s", acsdc_info.cur_cmd,
			acsdc_info.ifname) + 1;
	} else if (!strcmp(*argv, "msglevel")) {
		acsdc_get_set(argv, "msglevel", buf);
#if 1//ZyXEL, ShuYing, support skip channel 12 and channel 13
	} else if (!strcmp(*argv, "skipchannel1213")) {
		acsdc_get_set(argv, "skipchannel1213", buf);
#endif
#if 1 //__ZyXEL__, Wood, supprot skip channel list
	} else if (!strcmp(*argv, "skipchannellist")) {
		acsdc_get_set(argv, "skipchannellist", buf);
#endif
	} else if (!strcmp(*argv, "max_acs")) {
		acsdc_get_set(argv, "max_acs", buf);
	} else if (!strcmp(*argv, "lockout_period")) {
		acsdc_get_set(argv, "lockout_period", buf);
	} else if (!strcmp(*argv, "sample_period")) {
		acsdc_get_set(argv, "sample_period", buf);
	} else if (!strcmp(*argv, "threshold_time")) {
		acsdc_get_set(argv, "threshold_time", buf);
	} else if (!strcmp(*argv, "acs_trigger_var")) {
		acsdc_get_set(argv, "acs_trigger_var", buf);
	} else if (!strcmp(*argv, "mode")) {
		acsdc_get_set(argv, "mode", buf);
	} else if (!strcmp(*argv, "acs_policy")) {
		acsdc_get_set(argv, "acs_policy", buf);
	} else if (!strcmp(*argv, "acs_cs_scan_timer")) {
		acsdc_get_set(argv, "acs_cs_scan_timer", buf);
	} else if (!strcmp(*argv, "acs_flags")) {
		acsdc_get_set(argv, "acs_flags", buf);
	} else if (!strcmp(*argv, "chanim_flags")) {
		acsdc_get_set(argv, "chanim_flags", buf);
	} else if (!strcmp(*argv, "info")) {
		sprintf(acsdc_info.cur_cmd, "info");
		acsdc_info.cmd_size = sprintf(buf, "%s&", acsdc_info.cur_cmd) + 1;
	} else if (!strcmp(*argv, "status")) {
		sprintf(acsdc_info.cur_cmd, "status");
		acsdc_info.cmd_size = sprintf(buf, "%s&", acsdc_info.cur_cmd) + 1;
	} else if (!strcmp(*argv, "acs_fcs_mode")) {
		acsdc_get_set(argv, "acs_fcs_mode", buf);
	} else if (!strcmp(*argv, "acs_tx_idle_cnt")) {
		acsdc_get_set(argv, "acs_tx_idle_cnt", buf);
	} else if (!strcmp(*argv, "acs_ci_scan_timeout")) {
		acsdc_get_set(argv, "acs_ci_scan_timeout", buf);
	} else if (!strcmp(*argv, "acs_lowband_least_rssi")) {
		acsdc_get_set(argv, "acs_lowband_least_rssi", buf);
	} else if (!strcmp(*argv, "acs_nofcs_least_rssi")) {
		acsdc_get_set(argv, "acs_nofcs_least_rssi", buf);
	} else if (!strcmp(*argv, "acs_scan_chanim_stats")) {
		acsdc_get_set(argv, "acs_scan_chanim_stats", buf);
	} else if (!strcmp(*argv, "acs_fcs_chanim_stats")) {
		acsdc_get_set(argv, "acs_fcs_chanim_stats", buf);
	} else if (!strcmp(*argv, "acs_chan_dwell_time")) {
		acsdc_get_set(argv, "acs_chan_dwell_time", buf);
	} else if (!strcmp(*argv, "acs_chan_flop_period")) {
		acsdc_get_set(argv, "acs_chan_flop_period", buf);
	} else if (!strcmp(*argv, "acs_ics_dfs")) {
		acsdc_get_set(argv, "acs_ics_dfs", buf);
	} else if (!strcmp(*argv, "acs_txdelay_period")) {
		acsdc_get_set(argv, "acs_txdelay_period", buf);
	} else if (!strcmp(*argv, "acs_txdelay_cnt")) {
		acsdc_get_set(argv, "acs_txdelay_cnt", buf);
	} else if (!strcmp(*argv, "acs_txdelay_ratio")) {
		acsdc_get_set(argv, "acs_txdelay_ratio", buf);
	} else if (!strcmp(*argv, "acs_scan_entry_expire")) {
		acsdc_get_set(argv, "acs_scan_entry_expire", buf);
	} else if (!strcmp(*argv, "acs_ci_scan_timer")) {
		acsdc_get_set(argv, "acs_ci_scan_timer", buf);
	} else usagec();

	*arg = argv;

	return 0;
}

static int
acsdc_check_resp_err(void)
{
	char *p;
	int err = 0;

	p = strstr(acsdc_info.cmd_buf, "ERR:");

	if (p && acsdc_info.cmd_buf[0]) {
		err = -1;
		printf("%s\n", acsdc_info.cmd_buf);
	}
	return err;
}

static void
acs_dump_candi(ch_candidate_t * candi)
{
	char chanbuf[CHANSPEC_STR_LEN]; 
#ifdef MSTC_WLAN_CHSCAN
/*Don't print useless message : Peter.Lee.MitraStar 2012-11-28*/
	if((candi->chspec == 0) || (!candi->valid))
		return;
#endif
	printf("Candidate channel spec: 0x%x\n", candi->chspec);
	printf(" In_use: %d\n", candi->in_use);
	printf(" Valid: %s\n", (candi->valid)? "TRUE" : "FALSE");

	if (!candi->valid)
		printf(" Reason: %s%s%s%s%s%s%s\n",
			(candi->reason & ACS_INVALID_COEX)? "COEX " : "",
			(candi->reason & ACS_INVALID_OVLP)? "OVERLAP " : "",
			(candi->reason & ACS_INVALID_NOISE)? "NOISE " : "",
			(candi->reason & ACS_INVALID_ALIGN)? "NON-ALIGNED " : "",
			(candi->reason & ACS_INVALID_144)? "144 " : "",
			(candi->reason & ACS_INVALID_DFS)? "DFS " : "",
			(candi->reason & ACS_INVALID_CHAN_FLOP_PERIOD)? "CHAN_FLOP " : "");

#ifdef MSTC_WLAN_CHSCAN // __MSTC__, Paul Ho, for channel scan, show the result not only for channel 1, 6, and 11
   if (candi->valid || (candi->reason & ACS_INVALID_OVLP))
#else // original
	if (candi->valid)
#endif /* MSTC_WLAN_CHSCAN */
		acs_dump_score(candi->chscore);
}

int
main(int argc, char **argv)
{
	int rcount = 0;
	int size = ACSD_BUFSIZE_4K;
	int err = 0;
	char *ifname = NULL;
	int help = 0;
	int status = CMD_ACS;

	acsdc_info.version = ACSD_VERSION;

	(void)*argv++;

	if ((status = acs_option(&argv, &ifname, &help)) == CMD_OPT) {
		if (ifname)
			strncpy(acsdc_info.ifname, ifname, IFNAMSIZ);
	}

	if (!ifname)
		strncpy(acsdc_info.ifname, DEFAULT_IFNAME, sizeof(DEFAULT_IFNAME));

	/* Create the acs_cli command for server */
	if (acsdc_make_cmd(&argv) <  0) {
		err = -1;
		goto main_exit;
	}

	/* Connect to the acsd server */
	if (acsdc_connect_server(argv) < 0)
		return 2;

	/* Send the command to server */
	if (swrite(acsdc_info.conn_fd, acsdc_info.cmd_buf, acsdc_info.cmd_size)
		 < acsdc_info.cmd_size) {
		ACSD_ERROR("Command send failed");
		err = -1;
		goto main_exit;
	}

	/* Help server get the data till EOF */
	shutdown(acsdc_info.conn_fd, SHUT_WR);

	/* Process the response */
	if ((rcount = sread(acsdc_info.conn_fd, acsdc_info.cmd_buf,
		(size - 1))) < 0) {
		ACSD_ERROR("Response receive failed: %s\n", strerror(errno));
		err = -1;
		goto main_exit;
	}

	acsdc_info.cmd_buf[rcount] = '\0';

	if (!strcmp(acsdc_info.cur_cmd, "dump")) {
		if (!strcmp(acsdc_info.param, "bss")) {

			acs_chan_bssinfo_t *cur = (acs_chan_bssinfo_t *) acsdc_info.cmd_buf;
			int ncis;
			int i;

			if (acsdc_check_resp_err())
				goto main_exit;

			ncis = rcount / sizeof(acs_chan_bssinfo_t);
			printf("channel  aAPs bAPs gAPs lSBs uSBs nEXs llSBs "
				"luSBs ulSBs uuSBs wEX20s wEx40s\n");

			for (i = 0; i < ncis; i++) {
				printf("%3d  %5d%5d%5d%5d%5d%5d%6d%6d%6d%6d%6d%6d\n",
					cur->channel, cur->aAPs, cur->bAPs, cur->gAPs,
					cur->lSBs, cur->uSBs, cur->nEXs, cur->llSBs, cur->luSBs,
				   cur->ulSBs, cur->uuSBs, cur->wEX20s, cur->wEX40s);
				cur++;
			}
		}
	    else if (!strcmp(acsdc_info.param, "chanim")) {
			wl_chanim_stats_t * chanim_stats = (wl_chanim_stats_t *) acsdc_info.cmd_buf;
			int i, j;
			chanim_stats_t *stats = chanim_stats->stats;

			if (acsdc_check_resp_err())
				goto main_exit;

			chanim_stats->version = ntohl(chanim_stats->version);
			chanim_stats->buflen = ntohl(chanim_stats->buflen);
			chanim_stats->count = ntohl(chanim_stats->count);


			if (chanim_stats->version != WL_CHANIM_STATS_VERSION) {
				printf("Version Mismatch\n");
				goto main_exit;
			}

			printf("Chanim Stats: version: %d, count: %d\n",
				chanim_stats->version, chanim_stats->count);

			printf("chanspec tx   inbss   obss   nocat   nopkt   doze   txop   "
				"goodtx  badtx  glitch   badplcp  knoise  timestamp\n");

			for (i = 0; i < chanim_stats->count; i++) {
				stats->chanspec = ntohs(stats->chanspec);
				printf("0x%4x\t", stats->chanspec);

				for (j = 0; j < CCASTATS_MAX; j++)
					printf("%d\t", stats->ccastats[j]);

				printf("%d\t%d\t%d\t%d", ntohl(stats->glitchcnt),
					ntohl(stats->badplcp), stats->bgnoise,
					ntohl(stats->timestamp));
				printf("\n");
				stats ++;
			}
		}
		else if (!strcmp(acsdc_info.param, "candidate")) {
			ch_candidate_t *candi = (ch_candidate_t *) acsdc_info.cmd_buf;
			int i, j;
			int count;

			if (acsdc_check_resp_err())
				goto main_exit;

			count = rcount / sizeof(ch_candidate_t);

			for (i = 0; i < count; i++, candi++) {
				candi->chspec = ntohs(candi->chspec);

				for (j = 0; j < CH_SCORE_MAX; j++) {
					candi->chscore[j].score = ntohl(candi->chscore[j].score);
					candi->chscore[j].weight = ntohl(candi->chscore[j].weight);
				}

				acs_dump_candi(candi);
			}
		}
		else if (!strcmp(acsdc_info.param, "acs_record")) {
			chanim_acs_record_t *result = (chanim_acs_record_t *) acsdc_info.cmd_buf;
			int i;
			const char *trig_str[] = {"INIT", "IOCTL", "CHANIM", "TIMER", "BTA", "TXDLY", "NONACS"};
			int count;

			count = rcount / sizeof(chanim_acs_record_t);

			if (acsdc_check_resp_err())
				goto main_exit;

			if (!result || (!count)) {
				printf("There is no ACS recorded\n");
				goto main_exit;
			}

			printf("Timestamp(ms) ACS Trigger  Chanspec  BG Noise  CCA Count\n");
			for (i = 0; i < count; i++) {
				time_t ltime = ntohl(result->timestamp);
				printf("%8u \t%s \t%8x \t%d \t%d\n",
					(uint32)ltime,
					trig_str[result->trigger],
					ntohs(result->selected_chspc),
					result->bgnoise,
					result->ccastats);
				result++;
			}
		}
		else if (acsdc_info.cmd_buf[0])
			printf("%s\n", acsdc_info.cmd_buf);
	}
	else {
	/* Display the response */
		if (acsdc_info.cmd_buf[0]) printf("%s\n", acsdc_info.cmd_buf);
	}

main_exit:
	return err;
}
