/*
 * 802.11h CCA stats module header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cca.h 253231 2011-04-14 06:59:23Z $
*/

#ifndef _wlc_cca_h_
#define _wlc_cca_h_

#ifdef ISID_STATS
#define CCA_POOL_MAX		450	/* bigger pool for interference samples */
#else
#define CCA_POOL_MAX		300
#endif
#define CCA_FREE_BUF		0xffff

typedef uint16 cca_idx_t;
typedef struct {
	chanspec_t chanspec;
	cca_idx_t  secs[MAX_CCA_SECS];
} cca_congest_channel_t;

typedef struct {
	uint32 duration;	/* millisecs spent sampling this channel */
	uint32 congest_ibss;	/* millisecs in our bss (presumably this traffic will */
				/*  move if cur bss moves channels) */
	uint32 congest_obss;	/* traffic not in our bss */
	uint32 interference;	/* millisecs detecting a non 802.11 interferer. */
	uint32 timestamp;	/* second timestamp */
#ifdef ISID_STATS
	uint32 crsglitch;	/* crs glitchs */
	uint32 badplcp;		/* num bad plcp */
#endif /* ISID_STATS */
} wlc_congest_t;

typedef struct {
	chanspec_t chanspec;	/* Which channel? */
	uint8 num_secs;		/* How many secs worth of data */
	wlc_congest_t  secs[1];	/* Data */
} wlc_congest_channel_req_t;

extern void cca_stats_upd(wlc_info_t *wlc, int calculate);
extern void cca_stats_tsf_upd(wlc_info_t *wlc);
extern cca_info_t *wlc_cca_attach(wlc_info_t *wlc);
extern void wlc_cca_detach(cca_info_t *cca);
extern int cca_query_stats(wlc_info_t *wlc, chanspec_t chanspec, int nsecs,
	wlc_congest_channel_req_t *stats_results, int buflen);
extern chanspec_t wlc_cca_get_chanspec(wlc_info_t *wlc, int index);

#endif /* _wlc_cca_h_ */
