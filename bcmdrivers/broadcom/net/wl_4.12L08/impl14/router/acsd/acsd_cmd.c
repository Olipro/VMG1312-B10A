/*
 * ACS deamon command module (Linux)
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: acsd_cmd.c 357022 2012-09-14 22:51:43Z $
 */

#include "acsd_svr.h"

#define CHANIM_GET(field, unit)						\
	{ \
		if (!strcmp(param, #field)) { \
			chanim_info_t* ch_info = c_info->chanim_info; \
			if (ch_info) \
				*r_size = sprintf(buf, "%d %s", \
					chanim_config(ch_info).field, unit); \
			else \
				*r_size = sprintf(buf, "ERR: Requested info not available"); \
			goto done; \
		} \
	}

#define CHANIM_SET(field, unit, type)				\
	{ \
		if (!strcmp(param, #field)) { \
			chanim_info_t* ch_info = c_info->chanim_info; \
			if (!ch_info) { \
				*r_size = sprintf(buf, "ERR: set action not successful"); \
				goto done; \
			} \
			if ((setval > (type)chanim_range(ch_info).field.max_val) || \
				(setval < (type)chanim_range(ch_info).field.min_val)) { \
				*r_size = sprintf(buf, "Value out of range (min: %d, max: %d)\n", \
					chanim_range(ch_info).field.min_val, \
					chanim_range(ch_info).field.max_val); \
				goto done; \
			} \
			chanim_config(ch_info).field = setval; \
			*r_size = sprintf(buf, "%d %s", chanim_config(ch_info).field, unit); \
			goto done; \
		} \
	}

static uint
acsd_extract_token_val(char* data, const char * token, char *output)
{
	char *p, *c;
	char copydata[512];
	char *val = NULL;

	if (data != NULL)
		strcpy(copydata, data);

	ACSD_DEBUG("copydata: %s\n", copydata);

	p = strstr(copydata, token);

	if (!p) {
		output = NULL;
		return -1;
	}
	if ((c = strchr(p, '&')))
		*c++ = '\0';

	val = strchr(p, '=') + 1;
	ACSD_DEBUG("token_val: %s\n", val);

	strcpy(output, val);
	return sizeof(val);
}

static int
acsd_pass_candi(ch_candidate_t * candi, int count, char* buf, uint* r_size)
{
	int totsize = 0;
	int i, j;
	ch_candidate_t tmp;

	for (i = 0; i < count; i++) {
		memcpy((void*)&tmp, (void*)candi, sizeof(ch_candidate_t));

		ACSD_DEBUG("candi->chspec: 0x%x\n", candi->chspec);
		tmp.chspec = htons(candi->chspec);

		if (tmp.valid) {
			for (j = 0; j < CH_SCORE_MAX; j++) {
				tmp.chscore[j].score = htonl(candi->chscore[j].score);
				tmp.chscore[j].weight = htonl(candi->chscore[j].weight);
			}
		}

		memcpy((void*)buf, (void*)&tmp, sizeof(ch_candidate_t));
		*r_size += sizeof(ch_candidate_t);
		buf += sizeof(ch_candidate_t);
		totsize += sizeof(ch_candidate_t);
		candi ++;
	}

	return totsize;
}

int
acsd_proc_cmd(acsd_wksp_t* d_info, char* buf, uint rcount, uint* r_size)
{
	char *c, *data;
	int err = 0, ret;
	char ifname[IFNAMSIZ];

	/* Check if we have command and data in the expected order */
	if (!(c = strchr(buf, '&'))) {
		ACSD_ERROR("Missing Command\n");
		err = -1;
		goto done;
	}
	*c++ = '\0';
	data = c;

	if (!strcmp(buf, "info")) {
		time_t ltime;
		int i;
		const char *mode_str[] = {"disabled", "monitor", "auto", "coex", "11h"};
		d_info->stats.valid_cmds++;

		time(&ltime);
		*r_size = sprintf(buf, "time: %s \n", ctime(&ltime));
		*r_size += sprintf(buf+ *r_size, "acsd version: %d\n", d_info->version);
		*r_size += sprintf(buf+ *r_size, "acsd ticks: %d\n", d_info->ticks);
		*r_size += sprintf(buf+ *r_size, "acsd poll_interval: %d seconds\n",
			d_info->poll_interval);

		for (i = 0; i < ACS_MAX_IF_NUM; i++) {
			acs_chaninfo_t *c_info = NULL;

			c_info = d_info->acs_info->chan_info[i];

			if (!c_info)
				continue;

			*r_size += sprintf(buf+ *r_size, "ifname: %s, mode: %s\n",
				c_info->name, mode_str[c_info->mode]);
		}

		goto done;
	}
#ifdef MSTC_WLAN_CHSCAN // __MSTC__, Paul Ho, for channel scan, only scan and build the candidate table
	if (!strcmp(buf, "csscan") || (!strcmp(buf, "autochannel")) || (!strcmp(buf, "csscan_MTS"))) {
#else // original
	if (!strcmp(buf, "csscan") || (!strcmp(buf, "autochannel"))) {
#endif /* MSTC_WLAN_CHSCAN */
		acs_chaninfo_t *c_info = NULL;
		int index;
		bool pick = FALSE;

		d_info->stats.valid_cmds++;
		pick = !strcmp(buf, "autochannel");

		if ((ret = acsd_extract_token_val(data, "ifname", ifname)) < 0) {
			*r_size = sprintf(buf, "Request failed: missing ifname");
			goto done;
		}

		ACSD_DEBUG("cmd: %s, data: %s, ifname: %s\n",
			buf, data, ifname);

		index = acs_idx_from_map(ifname);
		if (index != -1) {
			c_info = d_info->acs_info->chan_info[index];
		}

		if (!c_info) {
			*r_size = sprintf(buf, "Request not permitted: "
				"Interface was not intialized properly");
			goto done;
		}
#ifdef MSTC_WLAN_CHSCAN
		if (!AUTOCHANNEL(c_info) && strcmp(buf, "csscan_MTS")) {
			*r_size = sprintf(buf, "Request not permitted: "
				"Interface is not in autochannel mode");
			goto done;
		}
#else
		if (!AUTOCHANNEL(c_info)) {
			*r_size = sprintf(buf, "Request not permitted: "
				"Interface is not in autochannel mode");
			goto done;
		}
#endif
		err = acs_run_cs_scan(c_info);

		ACS_FREE(c_info->acs_bss_info_q);
		err = acs_request_data(c_info);

		if (pick) {
			chanim_info_t * ch_info = c_info->chanim_info;
			uint8 cur_idx = chanim_mark(ch_info).record_idx;
			uint8 start_idx;
			chanim_acs_record_t *start_record;

			start_idx = MODSUB(cur_idx, 1, CHANIM_ACS_RECORD);
			start_record = &ch_info->record[start_idx];

			acs_select_chspec(c_info);
			acs_set_chspec(c_info);
			acs_update_driver(c_info);

		/* for consecutive ioctl based trigger, replace the previous one */
			if (start_record->trigger == APCS_IOCTL)
				chanim_mark(ch_info).record_idx = start_idx;

			chanim_upd_acs_record(c_info->chanim_info,
				c_info->selected_chspec, APCS_IOCTL);
		}

#ifdef MSTC_WLAN_CHSCAN // __MSTC__, Paul Ho, for channel scan, only scan and build the candidate table
		if (!strcmp(buf, "csscan_MTS"))
			/* used for only generate candidate table (do not set channel) */
			acs_select_chspec(c_info);
#endif /* MSTC_WLAN_CHSCAN */

		*r_size = sprintf(buf, "Request finished");
		goto done;
	}

	if (!strcmp(buf, "dump")) {
		char param[128];
		int index;
		acs_chaninfo_t *c_info = NULL;

		if ((ret = acsd_extract_token_val(data, "param", param)) < 0) {
			*r_size = sprintf(buf, "Request failed: missing param");
			goto done;
		}

		if ((ret = acsd_extract_token_val(data, "ifname", ifname)) < 0) {
			*r_size = sprintf(buf, "Request failed: missing ifname");
			goto done;
		}

		ACSD_DEBUG("cmd: %s, data: %s, param: %s, ifname: %s\n",
			buf, data, param, ifname);

		index = acs_idx_from_map(ifname);

		ACSD_DEBUG("index is : %d\n", index);
		if (index != -1)
			c_info = d_info->acs_info->chan_info[index];

		ACSD_DEBUG("c_info: 0x%x\n", (uint32) c_info);

		if (!c_info) {
			*r_size = sprintf(buf, "ERR: Requested info not available");
			goto done;
		}

		d_info->stats.valid_cmds++;
		if (!strcmp(param, "chanim")) {
			wl_chanim_stats_t * chanim_stats = c_info->chanim_stats;
			wl_chanim_stats_t tmp_stats;
			int count;
			int i;
			chanim_stats_t *stats;
			chanim_stats_t loc;

			if (!chanim_stats) {
				*r_size = sprintf(buf, "ERR: Requested info not available");
				goto done;
			}

			count = chanim_stats->count;
			stats = chanim_stats->stats;
			tmp_stats.version = htonl(chanim_stats->version);
			tmp_stats.buflen = htonl(chanim_stats->buflen);
			tmp_stats.count = htonl(chanim_stats->count);

			memcpy((void*)buf, (void*)&tmp_stats, WL_CHANIM_STATS_FIXED_LEN);
			*r_size = WL_CHANIM_STATS_FIXED_LEN;
			buf += *r_size;

			for (i = 0; i < count; i++) {
				memcpy((void*)&loc, (void*)stats, sizeof(chanim_stats_t));
				loc.glitchcnt = htonl(stats->glitchcnt);
				loc.badplcp = htonl(stats->badplcp);
				loc.chanspec = htons(stats->chanspec);
				loc.timestamp = htonl(stats->timestamp);

				memcpy((void*) buf, (void*)&loc, sizeof(chanim_stats_t));
				*r_size += sizeof(chanim_stats_t);
				buf += sizeof(chanim_stats_t);
				stats++;
			}
		} else if (!strcmp(param, "candidate")) {
			ch_candidate_t * candi_80 = c_info->candidate[ACS_BW_80];
			ch_candidate_t * candi_40 = c_info->candidate[ACS_BW_40];
			ch_candidate_t * candi_20 = c_info->candidate[ACS_BW_20];

			if ((!candi_80) && (!candi_40) && (!candi_20)) {
				*r_size = sprintf(buf, "ERR: Requested info not available");
				goto done;
			}

			if (candi_80) {
				ACSD_ERROR("80 MHz candidates: count %d\n",
					c_info->c_count[ACS_BW_80]);
				acsd_pass_candi(candi_80, c_info->c_count[ACS_BW_80], buf, r_size);
			}

			if (candi_40) {
				ACSD_ERROR("40 MHz candidates: count %d\n",
					c_info->c_count[ACS_BW_40]);
				acsd_pass_candi(candi_40, c_info->c_count[ACS_BW_40], buf, r_size);
			}

			if (candi_20) {
				ACSD_ERROR("20 MHz candidates: count %d\n",
					c_info->c_count[ACS_BW_20]);
				acsd_pass_candi(candi_20, c_info->c_count[ACS_BW_20], buf, r_size);
			}

		} else if (!strcmp(param, "bss")) {
			acs_chan_bssinfo_t *bssinfo = c_info->ch_bssinfo;

			if (!bssinfo) {
				*r_size = sprintf(buf, "ERR: Requested info not available");
				goto done;
			}

			memcpy((void*)buf, (void*)bssinfo, sizeof(acs_chan_bssinfo_t) *
				c_info->scan_chspec_list.count);
			*r_size = sizeof(acs_chan_bssinfo_t) * c_info->scan_chspec_list.count;

		} else if (!strcmp(param, "acs_record")) {
			chanim_info_t * ch_info = c_info->chanim_info;
			chanim_acs_record_t record;
			uint8 idx = chanim_mark(ch_info).record_idx;
			int i, count = 0;

			if (!ch_info) {
				*r_size = sprintf(buf, "ERR: Requested info not available");
				goto done;
			}

			for (i = 0; i < CHANIM_ACS_RECORD; i++) {
				if (ch_info->record[idx].valid) {
					bcopy(&ch_info->record[idx], &record,
						sizeof(chanim_acs_record_t));
					record.selected_chspc =
						htons(record.selected_chspc);
					record.glitch_cnt =
						htonl(record.glitch_cnt);
					record.timestamp =
						htonl(record.timestamp);

					memcpy((void*) buf, (void*)&record,
						sizeof(chanim_acs_record_t));
					*r_size += sizeof(chanim_acs_record_t);
					buf += sizeof(chanim_acs_record_t);

					count++;
					ACSD_DEBUG("count: %d trigger: %d\n",
						count, record.trigger);
				}
				idx = (idx + 1) % CHANIM_ACS_RECORD;
			}

			ACSD_DEBUG("rsize: %d, sizeof: %d\n", *r_size, sizeof(chanim_acs_record_t));

		} else if (!strcmp(param, "acsd_stats")) {
			acsd_stats_t * d_stats = &d_info->stats;

			if (!d_stats) {
				*r_size = sprintf(buf, "ERR: Requested info not available");
				goto done;
			}

			*r_size = sprintf(buf, "ACSD stats:\n");
			*r_size += sprintf(buf + *r_size, "Total cmd: %d, Valid cmd: %d\n",
				d_stats->num_cmds, d_stats->valid_cmds);
			*r_size += sprintf(buf + *r_size, "Total events: %d, Valid events: %d\n",
				d_stats->num_events, d_stats->valid_events);

			goto done;

		} else if (!strcmp(param, "scanresults")) {

			acs_dump_scan_entry(c_info);
		} else {
			*r_size = sprintf(buf, "Unsupported dump command");
		}
		goto done;
	}

	if (!strcmp(buf, "get")) {
		char param[128];
		int index;
		acs_chaninfo_t *c_info = NULL;

		if ((ret = acsd_extract_token_val(data, "param", param)) < 0) {
			*r_size = sprintf(buf, "Request failed: missing param");
			goto done;
		}

		if ((ret = acsd_extract_token_val(data, "ifname", ifname)) < 0) {
			*r_size = sprintf(buf, "Request failed: missing ifname");
			goto done;
		}

		ACSD_DEBUG("cmd: %s, data: %s, param: %s, ifname: %s\n",
			buf, data, param, ifname);

		index = acs_idx_from_map(ifname);

		ACSD_DEBUG("index is : %d\n", index);
		if (index != -1)
			c_info = d_info->acs_info->chan_info[index];

		ACSD_DEBUG("c_info: 0x%x\n", (uint32) c_info);

		if (!c_info) {
			*r_size = sprintf(buf, "ERR: Requested info not available");
			goto done;
		}

		d_info->stats.valid_cmds++;
		if (!strcmp(param, "msglevel")) {
			*r_size = sprintf(buf, "%d", acsd_debug_level);
			goto done;
		}
#if 1 //ZyXEL, ShuYing, support skip channel 12 and channel 13 
		if (!strcmp(param, "skipchannel1213")) {
			*r_size = sprintf(buf, "%d", c_info->skipchannel1213);
			goto done;
		}
#endif
#if 1 //__ZyXEL__, Wood, support skip channel list
		if (!strcmp(param, "skipchannellist")) {
			*r_size = sprintf(buf, "%s", c_info->skipchannellist);
			goto done;
		}
#endif
		if (!strcmp(param, "mode")) {
			const char *mode_str[] = {"disabled", "monitor", "select", "coex", "11h"};
			int acs_mode = c_info->mode;
			*r_size = sprintf(buf, "%d: %s", acs_mode, mode_str[acs_mode]);
			goto done;
		}

		if (!strcmp(param, "acs_cs_scan_timer")) {
			if (c_info->acs_cs_scan_timer)
				*r_size = sprintf(buf, "%d sec", c_info->acs_cs_scan_timer);
			else
				*r_size = sprintf(buf, "OFF");
			goto done;
		}

		if (!strcmp(param, "acs_policy")) {
			const char *pol_str[] = {"Default", "Legacy", "Intf", "Intf Busy",
				"Optimized", "Custom1", "Custom2", "User"};

			*r_size = sprintf(buf, "index: %d : %s", c_info->policy_index,
				pol_str[c_info->policy_index]);

#ifdef MSTC_WLAN_ACS_POLICY  //__MSTC__,Peter, read settingf from MDM
			printf("ACS Policy: %d \n",c_info->policy_index);
			printf("Bg Noise threshold: %d\n", c_info->acs_policy.bgnoise_thres);
			printf("Interference threshold: %d\n", c_info->acs_policy.intf_threshold);
			printf("Channel Scoring Weights: \n");
			printf("\t BSS: %d\n", c_info->acs_policy.acs_weight[CH_SCORE_BSS]);
			printf("\t BUSY: %d\n", c_info->acs_policy.acs_weight[CH_SCORE_BUSY]);
			printf("\t INTF: %d\n", c_info->acs_policy.acs_weight[CH_SCORE_INTF]);
			printf("\t INTFADJ: %d\n", c_info->acs_policy.acs_weight[CH_SCORE_INTFADJ]);
			printf("\t FCS: %d\n", c_info->acs_policy.acs_weight[CH_SCORE_FCS]);
			printf("\t TXPWR: %d\n", c_info->acs_policy.acs_weight[CH_SCORE_TXPWR]);
			printf("\t BGNOISE: %d\n", c_info->acs_policy.acs_weight[CH_SCORE_BGNOISE]);
			printf("\t CNS: %d\n", c_info->acs_policy.acs_weight[CH_SCORE_CNS]);
#endif /* #ifdef MSTC_WLAN_ACS_POLICY  //__MSTC__,Peter, read settingf from MDM */
			
			goto done;
		}

		if (!strcmp(param, "acs_flags")) {
			*r_size = sprintf(buf, "0x%x", c_info->flags);
			goto done;
		}

		if (!strcmp(param, "chanim_flags")) {
			chanim_info_t* ch_info = c_info->chanim_info;
			if (ch_info)
				*r_size = sprintf(buf, "0x%x", chanim_config(ch_info).flags);
			else
				*r_size = sprintf(buf, "ERR: Requested info not available");
			goto done;
		}

		if (!strcmp(param, "acs_fcs_mode")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs_mode);
			goto done;
		}

		if (!strcmp(param, "acs_tx_idle_cnt")) {
			*r_size = sprintf(buf, "%d tx packets", c_info->acs_fcs.acs_tx_idle_cnt);
			goto done;
		}

		if (!strcmp(param, "acs_ci_scan_timeout")) {
			*r_size = sprintf(buf, "%d sec", c_info->acs_fcs.acs_ci_scan_timeout);
			goto done;
		}

		if (!strcmp(param, "acs_lowband_least_rssi")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_lowband_least_rssi);
			goto done;
		}
		if (!strcmp(param, "acs_nofcs_least_rssi")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_nofcs_least_rssi);
			goto done;
		}
		if (!strcmp(param, "acs_scan_chanim_stats")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_scan_chanim_stats);
			goto done;
		}
		if (!strcmp(param, "acs_fcs_chanim_stats")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_fcs_chanim_stats);
			goto done;
		}
		if (!strcmp(param, "acs_chan_dwell_time")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_chan_dwell_time);
			goto done;
		}
		if (!strcmp(param, "acs_chan_flop_period")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_chan_flop_period);
			goto done;
		}
		if (!strcmp(param, "acs_ics_dfs")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_ics_dfs);
			goto done;
		}
		if (!strcmp(param, "acs_txdelay_period")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_txdelay_period);
			goto done;
		}
		if (!strcmp(param, "acs_txdelay_cnt")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_txdelay_cnt);
			goto done;
		}
		if (!strcmp(param, "acs_txdelay_ratio")) {
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_txdelay_ratio);
			goto done;
		}
		if (!strcmp(param, "acs_scan_entry_expire")) {
			*r_size = sprintf(buf, "%d sec", c_info->acs_scan_entry_expire);
			goto done;
		}
		if (!strcmp(param, "acs_ci_scan_timer")) {
			*r_size = sprintf(buf, "%d sec", c_info->acs_ci_scan_timer);
			goto done;
		}

		CHANIM_GET(max_acs, "");
		CHANIM_GET(lockout_period, "sec");
		CHANIM_GET(sample_period, "sec");
		CHANIM_GET(threshold_time, "sample period");
		CHANIM_GET(acs_trigger_var, "dBm");
	}

	if (!strcmp(buf, "set")) {
		char param[128];
		char val[16];
		int setval = 0;
		int index;
		acs_chaninfo_t *c_info = NULL;

		if ((ret = acsd_extract_token_val(data, "param", param)) < 0) {
			*r_size = sprintf(buf, "Request failed: missing param");
			goto done;
		}

		if ((ret = acsd_extract_token_val(data, "val", val)) < 0) {
			*r_size = sprintf(buf, "Request failed: missing val");
			goto done;
		}

		setval = ntohl(atoi(val));

		if ((ret = acsd_extract_token_val(data, "ifname", ifname)) < 0) {
			*r_size = sprintf(buf, "Request failed: missing ifname");
			goto done;
		}

		index = acs_idx_from_map(ifname);

		ACSD_DEBUG("index is : %d\n", index);
		if (index != -1)
			c_info = d_info->acs_info->chan_info[index];

		ACSD_DEBUG("c_info: 0x%x\n", (uint32) c_info);

		if (!c_info) {
			*r_size = sprintf(buf, "ERR: Requested ifname not available");
			goto done;
		}

		ACSD_DEBUG("cmd: %s, data: %s, param: %s val: %d, ifname: %s\n",
			buf, data, param, setval, ifname);

		d_info->stats.valid_cmds++;

		if (!strcmp(param, "msglevel")) {
			acsd_debug_level = setval;
			*r_size = sprintf(buf, "%d", acsd_debug_level);
		}
#if 1 //ZyXEL, ShuYing, support skip channel 12 and channel 13
		if (!strcmp(param, "skipchannel1213")) {
			c_info->skipchannel1213 = setval;
			*r_size = sprintf(buf, "%d", c_info->skipchannel1213);
		}
#endif
		if (!strcmp(param, "mode")) {
			const char *mode_str[] = {"disabled", "monitor", "select"};

			if (setval < ACS_MODE_DISABLE || setval > ACS_MODE_SELECT) {
				*r_size = sprintf(buf, "Out of range");
				goto done;
			}

			c_info->mode = setval;
			*r_size = sprintf(buf, "%d: %s", setval, mode_str[setval]);
			goto done;
		}

		if (!strcmp(param, "acs_cs_scan_timer")) {
			if (setval != 0 && setval < ACS_CS_SCAN_TIMER_MIN) {
				*r_size = sprintf(buf, "Out of range");
				goto done;
			}

			c_info->acs_cs_scan_timer = setval;

			if (setval)
				*r_size = sprintf(buf, "%d sec", c_info->acs_cs_scan_timer);
			else
				*r_size = sprintf(buf, "OFF");
			goto done;
		}

		if (!strcmp(param, "acs_policy")) {
			const char *pol_str[] = {"Default", "Legacy", "Intf", "Intf Busy",
				"Optimized", "Custom1", "Custom2", "User"};
			if (setval > ACS_POLICY_USER || setval < 0)  {
				*r_size = sprintf(buf, "Out of range");
				goto done;
			}

			c_info->policy_index = setval;

			if (setval != ACS_POLICY_USER)
				acs_default_policy(&c_info->acs_policy, setval);

			*r_size = sprintf(buf, "index: %d : %s", setval, pol_str[setval]);
			goto done;
		}

		if (!strcmp(param, "acs_flags")) {

			c_info->flags = setval;

			*r_size = sprintf(buf, "flags: 0x%x", c_info->flags);
			goto done;
		}

		if (!strcmp(param, "chanim_flags")) {
			chanim_info_t* ch_info = c_info->chanim_info;
			if (!ch_info) {
				*r_size = sprintf(buf, "ERR: set action not successful");
				goto done;
			}
			chanim_config(ch_info).flags = setval;
			*r_size = sprintf(buf, "0x%x", chanim_config(ch_info).flags);
			goto done;

		}

		if (!strcmp(param, "acs_fcs_mode")) {
			c_info->acs_fcs_mode = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs_mode);
			goto done;
		}

		if (!strcmp(param, "acs_tx_idle_cnt")) {
			c_info->acs_fcs.acs_tx_idle_cnt = setval;
			*r_size = sprintf(buf, "%d tx packets", c_info->acs_fcs.acs_tx_idle_cnt);
			goto done;
		}

		if (!strcmp(param, "acs_ci_scan_timeout")) {
			c_info->acs_fcs.acs_ci_scan_timeout = setval;
			*r_size = sprintf(buf, "%d sec", c_info->acs_fcs.acs_ci_scan_timeout);
			goto done;
		}

		if (!strcmp(param, "acs_lowband_least_rssi")) {
			c_info->acs_fcs.acs_lowband_least_rssi = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_lowband_least_rssi);
			goto done;
		}
		if (!strcmp(param, "acs_nofcs_least_rssi")) {
			c_info->acs_fcs.acs_nofcs_least_rssi = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_nofcs_least_rssi);
			goto done;
		}
		if (!strcmp(param, "acs_scan_chanim_stats")) {
			c_info->acs_fcs.acs_scan_chanim_stats = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_scan_chanim_stats);
			goto done;
		}
		if (!strcmp(param, "acs_fcs_chanim_stats")) {
			c_info->acs_fcs.acs_fcs_chanim_stats = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_fcs_chanim_stats);
			goto done;
		}
		if (!strcmp(param, "acs_chan_dwell_time")) {
			c_info->acs_fcs.acs_chan_dwell_time = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_chan_dwell_time);
			goto done;
		}
		if (!strcmp(param, "acs_chan_flop_period")) {
			c_info->acs_fcs.acs_chan_flop_period = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_chan_flop_period);
			goto done;
		}
		if (!strcmp(param, "acs_ics_dfs")) {
			c_info->acs_fcs.acs_ics_dfs = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_ics_dfs);
			goto done;
		}
		if (!strcmp(param, "acs_txdelay_period")) {
			c_info->acs_fcs.acs_txdelay_period = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_txdelay_period);
			goto done;
		}
		if (!strcmp(param, "acs_txdelay_cnt")) {
			c_info->acs_fcs.acs_txdelay_cnt = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_txdelay_cnt);
			goto done;
		}
		if (!strcmp(param, "acs_txdelay_ratio")) {
			c_info->acs_fcs.acs_txdelay_ratio = setval;
			*r_size = sprintf(buf, "%d", c_info->acs_fcs.acs_txdelay_ratio);
			goto done;
		}
		if (!strcmp(param, "acs_scan_entry_expire")) {
			c_info->acs_scan_entry_expire = setval;
			*r_size = sprintf(buf, "%d sec", c_info->acs_scan_entry_expire);
			goto done;
		}
		if (!strcmp(param, "acs_ci_scan_timer")) {
			c_info->acs_ci_scan_timer = setval;
			*r_size = sprintf(buf, "%d sec", c_info->acs_ci_scan_timer);
			goto done;
		}

		CHANIM_SET(max_acs, "", uint8);
		CHANIM_SET(acs_trigger_var, "dBm", int8);
		CHANIM_SET(lockout_period, "sec", uint32);
		CHANIM_SET(sample_period, "sec", uint8);
		CHANIM_SET(threshold_time, "sample period", uint8);
	}
done:
	return err;
}
