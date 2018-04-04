/*
 * CCA stats module source file
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_quiet.c 263508 2011-06-03 00:23:06Z $
 */

#include <wlc_cfg.h>

#ifdef CCA_STATS

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_ap.h>
#include <wlc_cca.h>
#include <wlc_bmac.h>
#include <wlc_assoc.h>
#include <wlc_alloc.h>
#include <wl_export.h>

/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_CCA_STATS,      /* Dump cca stats */
	IOV_LAST
};

static const bcm_iovar_t wlc_cca_iovars[] = {
	{"cca_get_stats", IOV_CCA_STATS,
	(0), IOVT_BUFFER, sizeof(cca_congest_channel_req_t),
	},
	{NULL, 0, 0, 0, 0}
};

static int wlc_cca_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);

static void cca_alloc_pool(cca_info_t *cca, int ch_idx, int second);
static void cca_free_pool(cca_info_t *cca, int ch_idx, int second);
static int cca_chanspec_to_index(cca_info_t *cca, chanspec_t chanspec);
static int cca_free_stats(void *ctx);
static int cca_stats_watchdog(void *ctx);
static int cca_get_stats(cca_info_t *cca, void *input, int buf_len, void *output);

struct cca_info {
	wlc_info_t *wlc;
	cca_ucode_counts_t last_cca_stats;	/* Previously read values, for computing deltas */
	cca_congest_channel_t     chan_stats[MAX_CCA_CHANNELS];
	int             cca_second;		/* which second bucket we are using */
	int             cca_second_max;		/* num of seconds to track */
	int		alloc_fail;
	wlc_congest_t	cca_pool[CCA_POOL_MAX];
};

#define CCA_POOL_DATA(cca, chanspec, second) \
	(&(cca->cca_pool[cca->chan_stats[chanspec].secs[second]]))
#define CCA_POOL_IDX(cca, chanspec, second) \
	(cca->chan_stats[chanspec].secs[second])

#define CCA_MODULE_NAME "cca_stats"

cca_info_t *
BCMATTACHFN(wlc_cca_attach)(wlc_info_t *wlc)
{
	int i;
	cca_info_t *cca = NULL;

	static const chanspec_t chanlist[] = {
		0x1001, 0x1002, 0x1003, 0x1004, 0x1005, 0x1006,
		0x1007, 0x1008, 0x1009, 0x100a, 0x100b, 0x100c, 0x100d, 0x100e, /*   1 - 11  */
		0xd024, 0xd028, 0xd02c, 0xd030, 0xd034, 0xd038, 0xd03c, 0xd040, /*  36 - 64  */
		0xd064, 0xd068, 0xd06c, 0xd070, 0xd074, 0xd078, 0xd07c, 0xd080, /* 100 - 128 */
		0xd084, 0xd088, 0xd08c,						/* 132 - 140 */
		0xd095, 0xd099, 0xd09d, 0xd0a1, 0xd0a5				/* 149 - 165 */
	};

	/* Not supported in revs < 15 */
	if (D11REV_LT(wlc->pub->corerev, 15)) {
		goto fail;
	}

	 if ((cca = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(cca_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	cca->wlc = wlc;
	cca->cca_second_max = MAX_CCA_SECS;
	cca->cca_second = 0;
	bzero(&cca->last_cca_stats, sizeof(cca->last_cca_stats));

	for (i = 0; i < MAX_CCA_CHANNELS; i++)
		cca->chan_stats[i].chanspec = chanlist[i];
	for (i = 0; i < CCA_POOL_MAX; i++)
		cca->cca_pool[i].congest_ibss = CCA_FREE_BUF;


	if (wlc_module_register(wlc->pub, wlc_cca_iovars, CCA_MODULE_NAME,
	    (void *)cca, wlc_cca_doiovar, cca_stats_watchdog, NULL,
	    cca_free_stats) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		wlc->pub->unit, __FUNCTION__));
		goto fail;
	};
	return cca;
fail:
	if (cca != NULL)
		MFREE(wlc->osh, cca, sizeof(cca_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_cca_detach)(cca_info_t *cca)
{
	wlc_info_t *wlc = cca->wlc;
	wlc_module_unregister(wlc->pub, CCA_MODULE_NAME, cca);
	MFREE(wlc->osh, cca, sizeof(cca_info_t));
}

static int
wlc_cca_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	cca_info_t *cca = (cca_info_t *)ctx;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;

	if (!cca)
		return 0;

	/* convenience int and bool vals for first 4 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);

	switch (actionid) {
	case IOV_GVAL(IOV_CCA_STATS):
		if ((p_len < sizeof(cca_congest_channel_req_t)) ||
		    (len < (int)sizeof(cca_congest_channel_req_t)))
			err = BCME_BUFTOOSHORT;
		else
			err = cca_get_stats(cca, params, len, arg);
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}
	return err;
}

/* Setup a new second for this chanspec_idx */
static void
cca_alloc_pool(cca_info_t *cca, int ch_idx, int second)
{
	int i;

	/* The zero'th entry is reserved, Its like a NULL pointer, give it out for failure */
	for (i = 1; i < CCA_POOL_MAX && cca->cca_pool[i].congest_ibss != CCA_FREE_BUF; i++)
		;
	if (i == CCA_POOL_MAX) {
		WL_ERROR(("%s: allocate an entry failed!\n", __FUNCTION__));
		/* Just leave the current bucket in place, nothing else we can do */
		/* Wait til watchdog ages out soem buckets */
		cca->alloc_fail++;
		return;
	}
#ifdef BCMDBG
	if (cca->cca_pool[i].congest_ibss != CCA_FREE_BUF)
		WL_ERROR(("%s:  NULL IDX but not CCA_FREE_BUF ch_idx = %d, dur = 0x%x\n",
			__FUNCTION__, i, cca->cca_pool[i].congest_ibss));
#endif
	bzero(&cca->cca_pool[i], sizeof(wlc_congest_t));
	CCA_POOL_IDX(cca, ch_idx, second) = (cca_idx_t)i & 0xffff;
	return;
}

/* Delete this second from given chanspec_idx */
static void
cca_free_pool(cca_info_t *cca, int ch_idx, int second)
{
	cca_idx_t pool_index = CCA_POOL_IDX(cca, ch_idx, second);
#ifdef BCMDBG
	if (cca->cca_pool[pool_index].congest_ibss == CCA_FREE_BUF)
		WL_ERROR(("%s: Freeing a free buffer\n", __FUNCTION__));
#endif
	cca->cca_pool[pool_index].congest_ibss = CCA_FREE_BUF;
	CCA_POOL_IDX(cca, ch_idx, second) = 0;
}

static int
cca_chanspec_to_index(cca_info_t *cca, chanspec_t chanspec)
{
	int i;
	for (i = 0; i < MAX_CCA_CHANNELS; i++) {
		if (cca->chan_stats[i].chanspec == chanspec)
			return (i);
	}
	return (-1);
}


chanspec_t
wlc_cca_get_chanspec(wlc_info_t *wlc, int index)
{
	cca_info_t *cca = wlc->cca_info;

	if (!cca)
		return 0;
	return cca->chan_stats[index].chanspec;
}

void
cca_stats_tsf_upd(wlc_info_t *wlc)
{
	uint32 tsf_l, tsf_h;
	cca_info_t *cca = wlc->cca_info;

	if (!cca)
		return;

	wlc_read_tsf(wlc, &tsf_l, &tsf_h);
	cca->last_cca_stats.usecs = tsf_l;
}


static int
cca_free_stats(void *ctx)
{
	int secs;
	chanspec_t chanspec;
	cca_info_t *cca = (cca_info_t *)ctx;

	if (!cca)
		return BCME_OK;
	for (secs = 0; secs < cca->cca_second_max; secs++) {
		for (chanspec = 0; chanspec < MAX_CCA_CHANNELS; chanspec++) {
			if (CCA_POOL_IDX(cca, chanspec, secs) != 0)
				cca_free_pool(cca, chanspec, secs);
		}
	}
	return BCME_OK;
}

static int
cca_stats_watchdog(void *ctx)
{
	cca_info_t *cca = (cca_info_t *)ctx;
	int ch_idx;
	chanspec_t chanspec;
	wlc_info_t *wlc;

	if (!cca)
		return BCME_OK;

	ASSERT(cca->wlc);
	wlc = cca->wlc;
	chanspec = wf_chspec_ctlchspec(wlc->chanspec);

	/* Bump the global 'second' pointer */
	cca->cca_second = MODINC(cca->cca_second, cca->cca_second_max);

	if ((ch_idx = cca_chanspec_to_index(cca, chanspec)) < 0) {
		WL_ERROR(("%s: Bad chanspec!!\n", __FUNCTION__));
		return BCME_BADCHAN;
	}

	/* The 'seconds' buffer wraps, so if we are coming to this particular
	   second again, free the previous contents.  Essentially this frees
	   buffers that are 61 seconds old
	*/
	for (chanspec = 0; chanspec < MAX_CCA_CHANNELS; chanspec++) {
		if (CCA_POOL_IDX(cca, chanspec, cca->cca_second) != 0)
			cca_free_pool(cca, chanspec, cca->cca_second);
	}

	/* Allocate new second for current channel */
	cca_alloc_pool(cca, ch_idx, cca->cca_second);

	cca_stats_upd(wlc, 1);

	return BCME_OK;
}

int
cca_query_stats(wlc_info_t *wlc, chanspec_t chanspec, int nsecs,
	wlc_congest_channel_req_t *stats_results, int buflen)
{
	int secs_done, ch_idx, second;
	wlc_congest_t *congest;
	cca_info_t *cca = wlc->cca_info;

	if (!cca)
		return 0;

	second = cca->cca_second;
	nsecs = MIN(cca->cca_second_max, nsecs);

	if ((ch_idx = cca_chanspec_to_index(cca, chanspec)) < 0) {
		stats_results->num_secs = 0;
		stats_results->chanspec = 0;
		return 0;
	}

	stats_results->chanspec = chanspec;
	buflen -= OFFSETOF(cca_congest_channel_req_t, secs);

	/* Retreive the last x secs of measurements */
	for (secs_done = 0; (secs_done < nsecs) && buflen >= sizeof(wlc_congest_t); secs_done++) {
		second = MODDEC(second, cca->cca_second_max);

		/* If the entry for this second/channel is empty, CCA_POOL_IDX
		   will be zero, and CCA_POOL_DATA will be the zero'th entry
		   which we keep empty for this purpose
		*/
		congest = CCA_POOL_DATA(cca, ch_idx, second);

		stats_results->secs[secs_done].duration =
			(congest->duration + 500)/1000;
		stats_results->secs[secs_done].congest_ibss =
			(congest->congest_ibss + 500)/1000;
		stats_results->secs[secs_done].congest_obss =
			(congest->congest_obss + 500)/1000;
		stats_results->secs[secs_done].interference =
			(congest->interference + 500)/1000;
		stats_results->secs[secs_done].timestamp =
			(congest->timestamp + 500)/1000;
#ifdef ISID_STATS
		stats_results->secs[secs_done].crsglitch =
			congest->crsglitch;
		stats_results->secs[secs_done].badplcp =
			congest->badplcp;
#endif /* ISID_STATS */

		buflen -= sizeof(wlc_congest_t);
	}
	stats_results->num_secs = (uint8)(secs_done & 0xff);
	return 0;
}

static int
cca_get_stats(cca_info_t *cca, void *input, int buf_len, void *output)
{
	int nsecs;
	chanspec_t chanspec;
	cca_congest_channel_req_t *req = (cca_congest_channel_req_t *)input;
	cca_congest_channel_req_t *stats_results = (cca_congest_channel_req_t *)output;
	wlc_congest_channel_req_t *results;
	int result_len;
	int status;

	if (!cca)
		return BCME_UNSUPPORTED;

	if (wf_chspec_malformed(req->chanspec))
		return BCME_BADCHAN;

	chanspec = wf_chspec_ctlchspec(req->chanspec);
	nsecs = req->num_secs;

	result_len = sizeof(wlc_congest_channel_req_t) +
		((nsecs ? nsecs - 1 : nsecs) * sizeof(wlc_congest_t));
	if (!(results = (wlc_congest_channel_req_t*)MALLOC(cca->wlc->osh, result_len)))
		return BCME_NOMEM;
	bzero(results, result_len);

	status = cca_query_stats(cca->wlc, chanspec, nsecs, results, result_len);

	if (status == 0) {
		int i;
		wlc_congest_t *wlc_congest = results->secs;
		cca_congest_t *cca_congest = stats_results->secs;
		stats_results->chanspec = results->chanspec;
		stats_results->num_secs = results->num_secs;
		for (i = 0; i < nsecs; i++) {
			cca_congest[i].duration = wlc_congest[i].duration;
			cca_congest[i].congest_ibss = wlc_congest[i].congest_ibss;
			cca_congest[i].congest_obss = wlc_congest[i].congest_obss;
			cca_congest[i].interference = wlc_congest[i].interference;
			cca_congest[i].timestamp = wlc_congest[i].timestamp;
		}
	}

	MFREE(cca->wlc->osh, results, result_len);

	return status;
}

void
cca_stats_upd(wlc_info_t *wlc, int calculate)
{
	cca_ucode_counts_t tmp;
	int chan;
	chanspec_t chanspec = wf_chspec_ctlchspec(wlc->chanspec);
	cca_info_t *cca = wlc->cca_info;

	if (!cca)
		return;

	if ((chan = cca_chanspec_to_index(cca, chanspec)) < 0) {
		WL_INFORM(("%s: Invalid chanspec 0x%x\n",
			__FUNCTION__, chanspec));
		return;
	}

	if (wlc_bmac_cca_stats_read(wlc->hw, &tmp))
		return;

	if (calculate) {
		/* alloc a new second if needed. */
		if (CCA_POOL_IDX(cca, chan, cca->cca_second) == 0)
			cca_alloc_pool(cca, chan, cca->cca_second);

		if (CCA_POOL_IDX(cca, chan, cca->cca_second) != 0) {
			cca_ucode_counts_t delta;
			wlc_congest_t *stats;

			/* Calc delta */
			delta.txdur = tmp.txdur  - cca->last_cca_stats.txdur;
			delta.ibss  = tmp.ibss   - cca->last_cca_stats.ibss;
			delta.obss  = tmp.obss   - cca->last_cca_stats.obss;
			delta.noctg = tmp.noctg  - cca->last_cca_stats.noctg;
			delta.nopkt = tmp.nopkt  - cca->last_cca_stats.nopkt;
			delta.usecs = tmp.usecs  - cca->last_cca_stats.usecs;
			delta.PM    = tmp.PM     - cca->last_cca_stats.PM;
#ifdef ISID_STATS
			delta.crsglitch = tmp.crsglitch - cca->last_cca_stats.crsglitch;
			delta.badplcp = tmp.badplcp - cca->last_cca_stats.badplcp;
#endif /* ISID_STATS */

			/* Update stats */
			stats = CCA_POOL_DATA(cca, chan, cca->cca_second);
			stats->duration += delta.usecs;
			/* Factor in time MAC was powered down */
			if (BSSCFG_STA(wlc->cfg) && wlc->cfg->pm->PMenabled)
				stats->duration -= delta.PM;
			stats->congest_ibss += delta.ibss + delta.txdur;
			stats->congest_obss += delta.obss + delta.noctg;
			stats->interference += delta.nopkt;
#ifdef ISID_STATS
			stats->crsglitch += delta.crsglitch;
			stats->badplcp += delta.badplcp;
#endif /* ISID_STATS */
			stats->timestamp = OSL_SYSUPTIME();
		}
	}
	/* Store raw values for next read */
	cca->last_cca_stats.txdur = tmp.txdur;
	cca->last_cca_stats.ibss  = tmp.ibss;
	cca->last_cca_stats.obss  = tmp.obss;
	cca->last_cca_stats.noctg = tmp.noctg;
	cca->last_cca_stats.nopkt = tmp.nopkt;
	cca->last_cca_stats.usecs = tmp.usecs;
	cca->last_cca_stats.PM = tmp.PM;
#ifdef ISID_STATS
	cca->last_cca_stats.crsglitch = tmp.crsglitch;
	cca->last_cca_stats.badplcp = tmp.badplcp;
#endif /* ISID_STATS */
}
#endif /* CCA_STATS */
