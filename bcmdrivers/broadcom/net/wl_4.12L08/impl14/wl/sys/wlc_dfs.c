/*
 * 802.11h DFS module source file
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
 * $Id$
 */

#include <wlc_cfg.h>

#ifdef WLDFS

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
#include <wlc_alloc.h>
#include <wl_export.h>
#include <wlc_ap.h>
#include <wlc_scan.h>
#include <wlc_phy_hal.h>
#include <wlc_quiet.h>
#include <wlc_csa.h>
#include <wlc_11h.h>
#include <wlc_dfs.h>

/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_DFS_PREISM,		/* preism cac time */
	IOV_DFS_POSTISM,	/* postism cac time */
	IOV_DFS_STATUS,		/* dfs cac status */
	IOV_DFS_ISM_MONITOR,    /* control the behavior of ISM state */
	IOV_DFS_CHANNEL_FORCED, /* next dfs channel forced */
	IOV_PER_CHAN_INFO,
	IOV_LAST
};

static const bcm_iovar_t wlc_dfs_iovars[] = {
	{"dfs_preism", IOV_DFS_PREISM, 0, IOVT_UINT32, 0},
	{"dfs_postism", IOV_DFS_POSTISM, 0, IOVT_UINT32, 0},
	{"dfs_status", IOV_DFS_STATUS, (0), IOVT_BUFFER, 0},
	{"dfs_ism_monitor", IOV_DFS_ISM_MONITOR, (0), IOVT_UINT32, 0},
	{"dfs_channel_forced", IOV_DFS_CHANNEL_FORCED, (0), IOVT_UINT32, 0},
	/* it is required for regulatory testing */
	{"per_chan_info", IOV_PER_CHAN_INFO, (0), IOVT_UINT16, 0},
	{NULL, 0, 0, 0, 0}
};

typedef struct wlc_dfs_cac {
	int	cactime_pre_ism;	/* configured preism cac time in second */
	int	cactime_post_ism;	/* configured postism cac time in second */
	uint32	nop_sec;		/* Non-Operation Period in second */
	int	ism_monitor;		/* 0 for off, non-zero to force ISM to monitor-only mode */
	chanspec_t chanspec_forced;		/* next dfs channel is forced to this one */
	wl_dfs_status_t	status;		/* data structure for handling dfs_status iovar */
	uint	cactime;      /* holds cac time in WLC_DFS_RADAR_CHECK_INTERVAL for current state */
	/* use of duration
	 * 1. used as a down-counter where timer expiry is needed.
	 * 2. (cactime - duration) is time elapsed at current state
	 */
	uint	duration;
	chanspec_t chanspec_next;	/* next dfs channel */
	bool	timer_running;
} wlc_dfs_cac_t;

/* Country module info */
struct wlc_dfs_info {
	wlc_info_t *wlc;
	uint chan_blocked[MAXCHANNEL];	/* 11h: seconds remaining in channel
					 * out of service period due to radar
					 */
	bool dfs_cac_enabled;		/* set if dfs cac enabled */
	struct wl_timer *dfs_timer;	/* timer for dfs cac handler */
	wlc_dfs_cac_t dfs_cac;		/* channel availability check */
	uint32 radar;			/* radar info: just on or off for now */
};

/* local functions */
/* module */
static int wlc_dfs_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_dfs_watchdog(void *ctx);
static int wlc_dfs_up(void *ctx);
static int wlc_dfs_down(void *ctx);
#ifdef BCMDBG
static int wlc_dfs_dump(void *ctx, struct bcmstrbuf *b);
#endif

/* others */
static uint32 wlc_get_chan_info(wlc_dfs_info_t *dfs, uint16 old_chanspec);
static void wlc_dfs_cacstate_init(wlc_dfs_info_t *dfs);
static int wlc_dfs_timer_init(wlc_dfs_info_t *dfs);
static void wlc_dfs_timer_add(wlc_dfs_info_t *dfs);
static bool wlc_dfs_timer_delete(wlc_dfs_info_t *dfs);
static void wlc_dfs_chanspec_oos(wlc_dfs_info_t *dfs, chanspec_t chanspec);
static chanspec_t wlc_dfs_chanspec(wlc_dfs_info_t *dfs);
static bool wlc_valid_ap_chanspec(wlc_dfs_info_t *dfs, chanspec_t chspec);
static bool wlc_radar_detected(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_idle_set(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_ism_set(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_ooc_set(wlc_dfs_info_t *dfs, uint target_state);
static void wlc_dfs_cacstate_idle(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_cac(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_ism(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_csa(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_ooc(wlc_dfs_info_t *dfs);
static void wlc_dfs_cacstate_handler(void *arg);

/* Local Data Structures */
static  void (*const wlc_dfs_cacstate_fn_ary[WL_DFS_CACSTATES])(wlc_dfs_info_t *dfs) = {
	wlc_dfs_cacstate_idle,
	wlc_dfs_cacstate_cac, /* preism_cac */
	wlc_dfs_cacstate_ism,
	wlc_dfs_cacstate_csa,
	wlc_dfs_cacstate_cac, /* postism_cac */
	wlc_dfs_cacstate_ooc, /* preism_ooc */
	wlc_dfs_cacstate_ooc /* postism_ooc */
};

#if defined(BCMDBG) || defined(WLMSG_DFS)
static const char *wlc_dfs_cacstate_str[WL_DFS_CACSTATES] = {
	"IDLE",
	"PRE-ISM Channel Availability Check",
	"In-Service Monitoring(ISM)",
	"Channel Switching Announcement(CSA)",
	"POST-ISM Channel Availability Check",
	"PRE-ISM Out Of Channels(OOC)",
	"POSTISM Out Of Channels(OOC)"
};
#endif /* BCMDBG || WLMSG_DFS */

/* module */
wlc_dfs_info_t *
BCMATTACHFN(wlc_dfs_attach)(wlc_info_t *wlc)
{
	wlc_dfs_info_t *dfs;

	if ((dfs = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_dfs_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	dfs->wlc = wlc;

	if (wlc_dfs_timer_init(dfs) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dfs_timer_init failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef BCMDBG
	if (wlc_dump_register(wlc->pub, "dfs", wlc_dfs_dump, dfs) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif

	/* keep the module registration the last other add module unregistratin
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, wlc_dfs_iovars, "dfs", dfs, wlc_dfs_doiovar,
	                        wlc_dfs_watchdog, wlc_dfs_up, wlc_dfs_down) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	return dfs;

	/* error handling */
fail:
	if (dfs != NULL) {
		if (dfs->dfs_timer != NULL)
			wl_free_timer(wlc->wl, dfs->dfs_timer);

		MFREE(wlc->osh, dfs, sizeof(wlc_dfs_info_t));
	}
	return NULL;
}

void
BCMATTACHFN(wlc_dfs_detach)(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;

	wlc_module_unregister(wlc->pub, "dfs", dfs);

	if (dfs->dfs_timer != NULL)
		wl_free_timer(wlc->wl, dfs->dfs_timer);

	MFREE(wlc->osh, dfs, sizeof(wlc_dfs_info_t));
}

static int
wlc_dfs_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;
	wlc_info_t *wlc = dfs->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	bool bool_val2;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;
	bool_val2 = (int_val2 != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val2);

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_DFS_PREISM):
		*ret_int_ptr = dfs->dfs_cac.cactime_pre_ism;
		break;

	case IOV_SVAL(IOV_DFS_PREISM):
		if ((int_val < -1) || (int_val >= WLC_DFS_CAC_TIME_SEC_MAX)) {
			err = BCME_RANGE;
			break;
		}
		dfs->dfs_cac.cactime_pre_ism = int_val;
		break;

	case IOV_GVAL(IOV_DFS_POSTISM):
		*ret_int_ptr = dfs->dfs_cac.cactime_post_ism;
		break;

	case IOV_SVAL(IOV_DFS_POSTISM):
		if ((int_val < -1) || (int_val >= WLC_DFS_CAC_TIME_SEC_MAX)) {
			err = BCME_RANGE;
			break;
		}
		dfs->dfs_cac.cactime_post_ism = int_val;
		break;

	case IOV_GVAL(IOV_DFS_STATUS):
		dfs->dfs_cac.status.duration =
		        (dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
		        WLC_DFS_RADAR_CHECK_INTERVAL;
		bcopy((char *)&dfs->dfs_cac.status, (char *)arg, sizeof(wl_dfs_status_t));
		break;

	case IOV_SVAL(IOV_DFS_ISM_MONITOR):
		dfs->dfs_cac.ism_monitor = bool_val;
		break;

	case IOV_GVAL(IOV_DFS_ISM_MONITOR):
		*ret_int_ptr = (int32)dfs->dfs_cac.ism_monitor;
		break;

	/* IOV_DFS_CHANNEL_FORCED is required for regulatory testing */
	case IOV_SVAL(IOV_DFS_CHANNEL_FORCED):
		if (CHSPEC_CHANNEL((chanspec_t)int_val) == 0) {
			dfs->dfs_cac.chanspec_forced = wlc_create_chspec(wlc, (uint8)int_val);
			break;
		}

		if (!N_ENAB(wlc->pub)) {
			int chan = CHSPEC_CHANNEL((chanspec_t)int_val);
			if (chan < 0 || chan > MAXCHANNEL) {
				err = BCME_OUTOFRANGECHAN;
				break;
			}
			dfs->dfs_cac.chanspec_forced = wlc_create_chspec(wlc, (uint8)int_val);
			break;
		}

		if (wf_chspec_malformed((chanspec_t)int_val)) {
			err = BCME_BADCHAN;
			break;
		}

		if (!wlc_valid_chanspec_db(wlc->cmi, (chanspec_t)int_val)) {
			err = BCME_BADCHAN;
			break;
		}

		if (!N_ENAB(wlc->pub) && CHSPEC_IS40((chanspec_t)int_val)) {
			err = BCME_BADCHAN;
			break;
		}

		dfs->dfs_cac.chanspec_forced = (chanspec_t)int_val;
		break;

	case IOV_GVAL(IOV_DFS_CHANNEL_FORCED):
		if (!N_ENAB(wlc->pub))
			*ret_int_ptr = CHSPEC_CHANNEL(dfs->dfs_cac.chanspec_forced);
		else
			*ret_int_ptr = dfs->dfs_cac.chanspec_forced;
		break;

	case IOV_GVAL(IOV_PER_CHAN_INFO):
		*ret_int_ptr = wlc_get_chan_info(dfs, (uint16)int_val);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_dfs_watchdog(void *ctx)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;
	wlc_info_t *wlc = dfs->wlc;

	(void)wlc;

	/* Restore channels 30 minutes after radar detect */
	if (WL11H_ENAB(wlc) && dfs->radar) {
		int chan;

		for (chan = 0; chan < MAXCHANNEL; chan++) {
			if (dfs->chan_blocked[chan] &&
			    dfs->chan_blocked[chan] != WLC_CHANBLOCK_FOREVER) {
				dfs->chan_blocked[chan]--;
				if (!dfs->chan_blocked[chan]) {
					WL_REGULATORY(("\t** DFS *** Channel %d is"
					               " clean after 30 minutes\n", chan));
				}
			}
		}
	}

	return BCME_OK;
}

static int
wlc_dfs_up(void *ctx)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;
	wlc_info_t *wlc = dfs->wlc;

	(void)wlc;

	if (!AP_ENAB(wlc->pub))
		return BCME_OK;

	/* Start radar timer if there are any quiet channels */
	if (dfs->radar) {
		uint j;
		for (j = 0; j < MAXCHANNEL; j++) {
			if (wlc_quiet_chanspec(wlc->cmi, CH20MHZ_CHSPEC(j))) {
				wlc_dfs_timer_add(dfs);
				break;
			}
		}
	}

	return BCME_OK;
}

static int
wlc_dfs_down(void *ctx)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;
	int callback = 0;

	/* cancel the radar timer */
	if (!wlc_dfs_timer_delete(dfs))
		callback = 1;
	dfs->dfs_cac_enabled = FALSE;

	return callback;
}

#ifdef BCMDBG
static int
wlc_dfs_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)ctx;

	bcm_bprintf(b, "radar %d\n", dfs->radar);
	bcm_bprhex(b, "chan_blocked ", TRUE,
	           (uint8 *)dfs->chan_blocked, sizeof(dfs->chan_blocked));
	bcm_bprintf(b, "cactime_pre_ism %u cactime_post_ism %u nop_sec %u ism_monitor %d\n",
	            dfs->dfs_cac.cactime_pre_ism, dfs->dfs_cac.cactime_post_ism,
	            dfs->dfs_cac.nop_sec, dfs->dfs_cac.ism_monitor);
	bcm_bprintf(b, "chanspec_forced %x status %d cactime %u\n",
	            dfs->dfs_cac.chanspec_forced, dfs->dfs_cac.status,
	            dfs->dfs_cac.cactime, dfs->dfs_cac);
	bcm_bprintf(b, "duration %u chanspec_next %x timer_running %d\n",
	            dfs->dfs_cac.duration, dfs->dfs_cac.chanspec_next,
	            dfs->dfs_cac.timer_running);
	bcm_bprintf(b, "dfs_cac_enabled %d\n", dfs->dfs_cac_enabled);

	return BCME_OK;
}
#endif /* BCMDBG */

static uint32
wlc_get_chan_info(wlc_dfs_info_t *dfs, uint16 old_chanspec)
{
	wlc_info_t *wlc = dfs->wlc;
	uint32 result;
	uint channel = CHSPEC_CHANNEL(old_chanspec);
	chanspec_t chspec = CH20MHZ_CHSPEC(channel);

	result = 0;
	if (channel && channel < MAXCHANNEL) {
		if ((channel <= CH_MAX_2G_CHANNEL) && isset(chanvec_all_2G.vec, channel))
			result |= WL_CHAN_VALID_HW;
		else if ((channel > CH_MAX_2G_CHANNEL) && isset(chanvec_all_5G.vec, channel))
			result |= WL_CHAN_VALID_HW | WL_CHAN_BAND_5G;
	}

	if (result & WL_CHAN_VALID_HW) {
		if (wlc_valid_chanspec_db(wlc->cmi, chspec))
			result |= WL_CHAN_VALID_SW;

		if (AP_ENAB(wlc->pub)) {
			if (dfs->chan_blocked[channel]) {
				int minutes;

				result |= WL_CHAN_INACTIVE;

				/* Store remaining minutes until channel comes
				 * in-service in high 8 bits.
				 */
				minutes = ROUNDUP(dfs->chan_blocked[channel], 60) / 60;
				result |= ((minutes & 0xff) << 24);
			}
		}

		if (result & WL_CHAN_VALID_SW) {
			if (wlc_radar_chanspec(wlc->cmi, chspec) == TRUE)
				result |= WL_CHAN_RADAR;
			if (wlc_restricted_chanspec(wlc->cmi, chspec))
				result |= WL_CHAN_RESTRICTED;
			if (wlc_quiet_chanspec(wlc->cmi, chspec))
				result |= WL_CHAN_PASSIVE;
		}
	}

	return (result);
}

#if defined(NDIS) && (NDISVER >= 0x0620)
static wlc_bsscfg_t*
wlc_get_ap_bsscfg(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	wlc_bsscfg_t *bsscfg = NULL;
	int i;

	if (AP_ACTIVE(wlc)) {
		for (i = 0; i < 2; i++) {
			if (wlc->bsscfg[i] && wlc->bsscfg[i]->_ap && wlc->bsscfg[i]->up) {
				bsscfg = wlc->bsscfg[i];
				/* one ap supported in Win7 */
				break;
			}
		}
	}

	ASSERT(bsscfg);
	return bsscfg;
}
#endif /* NDIS && (NDISVER >= 0x0620) */

/*
 * Helper function to use correct pre- and post-ISM CAC time for european weather radar channels
 * which use a different CAC timer (default is 10 minutes for EU weather radar channels, 1 minute
 * for regular radar CAC).
 *
 * Returns cactime in WLC_DFS_RADAR_CHECK_INTERVAL units.
 */
static uint
wlc_dfs_ism_cactime(wlc_info_t *wlc, int secs_or_default)
{

	if (secs_or_default == WLC_DFS_CAC_TIME_USE_DEFAULTS)
	{
		if (wlc_is_european_weather_radar_channel(wlc, WLC_BAND_PI_RADIO_CHANSPEC)) {

			secs_or_default = WLC_DFS_CAC_TIME_SEC_DEF_EUWR;

			WL_DFS(("wl%d: dfs chanspec %04x is european weather radar\n",
				wlc->pub->unit, WLC_BAND_PI_RADIO_CHANSPEC));
		}
		else {
			secs_or_default = WLC_DFS_CAC_TIME_SEC_DEFAULT;
		}
	}

	WL_DFS(("wl%d: dfs chanspec %04x is a radar channel, using %d second CAC time\n",
		wlc->pub->unit, WLC_BAND_PI_RADIO_CHANSPEC, secs_or_default));

	return (secs_or_default*1000)/WLC_DFS_RADAR_CHECK_INTERVAL;

}

static int
wlc_dfs_timer_init(wlc_dfs_info_t *dfs)
{
	wlc_info_t* wlc = dfs->wlc;

	dfs->dfs_cac.ism_monitor = FALSE; /* put it to normal mode */

	dfs->dfs_cac.timer_running = FALSE;

	if (!(dfs->dfs_timer = wl_init_timer(wlc->wl, wlc_dfs_cacstate_handler, dfs, "dfs"))) {
		WL_ERROR(("wl%d: wlc_dfs_timer_init failed\n", wlc->pub->unit));
		return -1;
	}
	dfs->dfs_cac.cactime_pre_ism = dfs->dfs_cac.cactime_post_ism
		= WLC_DFS_CAC_TIME_USE_DEFAULTS;   /* use default values */

	dfs->dfs_cac.nop_sec = WLC_DFS_NOP_SEC_DEFAULT;

	return 0;
}

static void
wlc_dfs_timer_add(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;

	if (dfs->dfs_cac.timer_running == FALSE) {
		dfs->dfs_cac.timer_running = TRUE;
		wl_add_timer(wlc->wl, dfs->dfs_timer, WLC_DFS_RADAR_CHECK_INTERVAL, TRUE);
	}
}

static bool
wlc_dfs_timer_delete(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	bool canceled = TRUE;

	if (dfs->dfs_cac.timer_running == TRUE) {
		if (dfs->dfs_timer != NULL) {
			canceled = wl_del_timer(wlc->wl, dfs->dfs_timer);
			ASSERT(canceled == TRUE);
		}
		dfs->dfs_cac.timer_running = FALSE;
	}
	return canceled;
}

static void
wlc_dfs_chanspec_oos(wlc_dfs_info_t *dfs, chanspec_t chanspec)
{
	wlc_info_t* wlc = dfs->wlc;
	bool is_block = FALSE;
	uint8 ctrl_ch, ext_ch;
#if defined(WL11AC) && defined(ACCONF) && (ACCONF != 0)
	int dfs_band = (CHSPEC_IS2G(chanspec) ? WLC_BAND_2G : WLC_BAND_5G);
#endif

	if (VHT_ENAB_BAND(wlc->pub, dfs_band)&& CHSPEC_IS80(chanspec)) {
		uint channel;
		int i;
		channel = LOWER_40_SB(CHSPEC_CHANNEL(chanspec));
		channel = LOWER_20_SB(channel);

		/* work through each 20MHz channel in the 80MHz */
		for (i = 0; i < 4; i++, channel += CH_20MHZ_APART) {
			dfs->chan_blocked[channel] = dfs->dfs_cac.nop_sec;
			WL_DFS(("wl%d: dfs : channel %d put out of service\n", wlc->pub->unit,
				channel));
		}
		ctrl_ch = CHSPEC_CHANNEL(chanspec);
		ext_ch = 0;
	} else if (N_ENAB(wlc->pub) && CHSPEC_IS40(chanspec)) {
		ctrl_ch = LOWER_20_SB(CHSPEC_CHANNEL(chanspec));
		ext_ch = UPPER_20_SB(CHSPEC_CHANNEL(chanspec));
		dfs->chan_blocked[ctrl_ch] = dfs->dfs_cac.nop_sec;
		dfs->chan_blocked[ext_ch] = dfs->dfs_cac.nop_sec;

		WL_DFS(("wl%d: dfs : channel %d & %d put out of service\n", wlc->pub->unit,
			ctrl_ch, ext_ch));
	} else {
		ctrl_ch = CHSPEC_CHANNEL(chanspec);
		ext_ch = 0;
		dfs->chan_blocked[ctrl_ch] = dfs->dfs_cac.nop_sec;

		WL_DFS(("wl%d: dfs : channel %d put out of service\n", wlc->pub->unit, ctrl_ch));
	}

	wlc_set_quiet_chanspec(wlc->cmi, chanspec);

	if (!bcmp("US", wlc_channel_country_abbrev(wlc->cmi), 2) ||
		!bcmp("CA", wlc_channel_country_abbrev(wlc->cmi), 2)) {
		if ((ctrl_ch >= 120 && ctrl_ch <= 128) ||
		   (N_ENAB(wlc->pub) && CHSPEC_IS40(chanspec) && ext_ch >= 120 && ext_ch <= 128))
			is_block = TRUE;
	}

	/* Special US and CA handling, remove set of channels 120, 124, 128 if
	 * any get a radar pulse.  For CA they will be blocked for uptime of the driver.
	 */
	if (is_block) {
		uint32  block_time = !bcmp("CA", wlc_channel_country_abbrev(wlc->cmi), 2) ?
		    WLC_CHANBLOCK_FOREVER : dfs->dfs_cac.nop_sec;

		wlc_set_quiet_chanspec(wlc->cmi, CH20MHZ_CHSPEC(120));
		dfs->chan_blocked[120] = block_time;
		wlc_set_quiet_chanspec(wlc->cmi, CH20MHZ_CHSPEC(124));
		dfs->chan_blocked[124] = block_time;
		wlc_set_quiet_chanspec(wlc->cmi, CH20MHZ_CHSPEC(128));
		dfs->chan_blocked[128] = block_time;
	}
}

/*
 * Random channel selection for DFS
 * Returns a valid chanspec of a valid radar free channel, using the AP configuration
 * to choose 20, 40 or 80 MHz bandwidth and side-band
 * Returns 0 if there are no valid radar free channels available
 */
static chanspec_t
wlc_dfs_chanspec(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	chanvec_t channels20, channels40, channels80, *chanvec;
	chanspec_t chspec;
	uint chan20_cnt, chan40_cnt, chan80_cnt;
	uint chan, rand_idx, rand_channel;
#if defined(WL11AC) && defined(ACCONF) && (ACCONF != 0)
	int dfs_band = (CHSPEC_IS2G(wlc->default_bss->chanspec) ? WLC_BAND_2G : WLC_BAND_5G);
#endif
#if defined(BCMDBG)
	char chanbuf[CHANSPEC_STR_LEN];
#endif

	chan20_cnt = chan40_cnt = chan80_cnt = 0;
	bzero(channels20.vec, sizeof(channels20.vec));
	bzero(channels40.vec, sizeof(channels40.vec));
	bzero(channels80.vec, sizeof(channels80.vec));

	/* walk the channels looking for good 20MHz channels */
	for (chan = 0; chan < MAXCHANNEL; chan++) {
		chspec = CH20MHZ_CHSPEC(chan);
		if (wlc_valid_ap_chanspec(dfs, chspec)) {
			setbit(channels20.vec, chan);
			chan20_cnt++;
		}
	}

	/* check for 40MHz channels only if we are capable of 40MHz, the default
	 * bss was configured for 40MHz, and the locale allows 40MHz
	 */
	if (N_ENAB(wlc->pub) &&
	    CHSPEC_IS40(wlc->default_bss->chanspec) &&
	    (WL_BW_CAP_40MHZ(wlc->band->bw_cap)) &&
	    !(wlc_channel_locale_flags(wlc->cmi) & WLC_NO_40MHZ)) {
		/* walk the channels looking for good 40MHz channels */
		for (chan = 0; chan < MAXCHANNEL; chan++) {
			chspec = CH40MHZ_CHSPEC(chan, CHSPEC_CTL_SB(wlc->default_bss->chanspec));
			if (wlc_valid_ap_chanspec(dfs, chspec)) {
				setbit(channels40.vec, chan);
				chan40_cnt++;
			}
		}
	}

	/* check for 80MHz channels only if we are capable of 80MHz, the default
	 * bss was configured for 80MHz, and the locale allows 80MHz
	 */
	if (VHT_ENAB_BAND(wlc->pub, dfs_band) &&
		CHSPEC_IS80(wlc->default_bss->chanspec) &&
	    (WL_BW_CAP_80MHZ(wlc->band->bw_cap)) &&
	    !(wlc_channel_locale_flags(wlc->cmi) & WLC_NO_80MHZ)) {
		/* walk the channels looking for good 80MHz channels */
		for (chan = 0; chan < MAXCHANNEL; chan++) {
			chspec = CH80MHZ_CHSPEC(chan, CHSPEC_CTL_SB(wlc->default_bss->chanspec));
			if (wlc_valid_ap_chanspec(dfs, chspec)) {
				setbit(channels80.vec, chan);
				chan80_cnt++;
			}
		}
	}

	if (!chan20_cnt) {
		/* no channel found */
		return 0;
	}

	rand_idx = R_REG(wlc->osh, &wlc->regs->u.d11regs.tsf_random);

	if (chan80_cnt) {
		rand_idx = rand_idx % chan80_cnt;
		chanvec = &channels80;
	} else if (chan40_cnt) {
		rand_idx = rand_idx % chan40_cnt;
		chanvec = &channels40;
	} else {
		rand_idx = rand_idx % chan20_cnt;
		chanvec = &channels20;
	}

	/* choose 'rand_idx'th channel */
	for (rand_channel = 0, chan = 0; chan < MAXCHANNEL; chan++) {
		if (isset(chanvec->vec, chan)) {
			if (rand_idx == 0) {
				rand_channel = chan;
				break;
			}
			rand_idx--;
		}
	}

	ASSERT(rand_channel);

	if (chan80_cnt)
		chspec = CH80MHZ_CHSPEC(rand_channel, CHSPEC_CTL_SB(wlc->default_bss->chanspec));
	else if (chan40_cnt)
		chspec = CH40MHZ_CHSPEC(rand_channel, CHSPEC_CTL_SB(wlc->default_bss->chanspec));
	else
		chspec = CH20MHZ_CHSPEC(rand_channel);

	ASSERT(wlc_valid_chanspec_db(wlc->cmi, chspec));

	if (dfs->dfs_cac.chanspec_forced &&
		wlc_valid_chanspec_db(wlc->cmi, dfs->dfs_cac.chanspec_forced)) {
		chspec = dfs->dfs_cac.chanspec_forced;
		WL_DFS(("dfs : set dfs channel forced to 0\n"));
		dfs->dfs_cac.chanspec_forced = 0; /* no more forcing */
	}
#if defined(BCMDBG)
	WL_DFS(("wl%d: %s: dfs selected chanspec %s (%04x)\n", wlc->pub->unit, __FUNCTION__,
	        wf_chspec_ntoa(chspec, chanbuf), chspec));
#endif

	return chspec;
}

/* check for a chanspec on which an AP can set up a BSS
 * Returns TRUE if the chanspec is valid for the local, not restricted, and
 * has not been blocked by a recent radar pulse detection.
 * Otherwise will return FALSE.
 */
static bool
wlc_valid_ap_chanspec(wlc_dfs_info_t *dfs, chanspec_t chspec)
{
	uint channel = CHSPEC_CHANNEL(chspec);
	wlc_info_t *wlc = dfs->wlc;

	if (!wlc_valid_chanspec_db(wlc->cmi, chspec) ||
	    wlc_restricted_chanspec(wlc->cmi, chspec))
		return FALSE;

	if (CHSPEC_IS80(chspec)) {
		if (dfs->chan_blocked[LL_20_SB(channel)] ||
		    dfs->chan_blocked[UU_20_SB(channel)] ||
			dfs->chan_blocked[LU_20_SB(channel)] ||
		    dfs->chan_blocked[UL_20_SB(channel)])
			return FALSE;
	} else if (CHSPEC_IS40(chspec)) {
		if (dfs->chan_blocked[LOWER_20_SB(channel)] ||
		    dfs->chan_blocked[UPPER_20_SB(channel)])
			return FALSE;
	} else if (dfs->chan_blocked[channel]) {
		return FALSE;
	}

	return TRUE;
}

static bool
wlc_radar_detected(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	int radar_type;
	int	radar_interval;
	int	min_pw;
#if defined(BCMDBG) || defined(WLMSG_DFS)
	uint i;
	char radar_type_str[24];
	char chanbuf[CHANSPEC_STR_LEN];
	static const struct {
		int radar_type;
		const char *radar_type_name;
	} radar_names[] = {
		{RADAR_TYPE_NONE, "NONE"},
		{RADAR_TYPE_ETSI_1, "ETSI_1"},
		{RADAR_TYPE_ETSI_2, "ETSI_2"},
		{RADAR_TYPE_ETSI_3, "ETSI_3"},
		{RADAR_TYPE_ETSI_4, "ETSI_4"},
		{RADAR_TYPE_STG2, "S2"},
		{RADAR_TYPE_STG3, "S3"},
		{RADAR_TYPE_UNCLASSIFIED, "UNCLASSIFIED"},
		{RADAR_TYPE_FCC_5, "FCC-5"},
		{RADAR_TYPE_JP1_2_JP2_3, "JP1-2/JP2-3"},
		{RADAR_TYPE_JP2_1, "JP2-1"},
		{RADAR_TYPE_JP4, "JP4"},
		{RADAR_TYPE_FCC_1, "FCC_1"}
	};
#endif /* BCMDBG || WLMSG_DFS */
	wlc_bsscfg_t *cfg = wlc->cfg;

	(void)cfg;

	radar_type = wlc_phy_radar_detect_run(wlc->band->pi);
	radar_interval = radar_type >> 14;
	BCM_REFERENCE(radar_interval);
	min_pw = radar_type >> 4 & 0x1ff;
	BCM_REFERENCE(min_pw);
	radar_type = radar_type & 0xf;
	/* Pretend we saw radar - for testing */
	if ((wlc_11h_get_spect_state(wlc->m11h, cfg) & RADAR_SIM) ||
	    radar_type != RADAR_TYPE_NONE) {

#if defined(BCMDBG) || defined(WLMSG_DFS)
		snprintf(radar_type_str, sizeof(radar_type_str),
			"%s", "UNKNOWN");
		for (i = 0; i < ARRAYSIZE(radar_names); i++) {
			if (radar_names[i].radar_type == radar_type)
				snprintf(radar_type_str, sizeof(radar_type_str),
					"%s", radar_names[i].radar_type_name);
		}

		WL_DFS(("WL%d: DFS: %s ########## RADAR DETECTED ON CHANNEL %s"
			" ########## Intv=%d, min_pw=%d, AT %dMS\n", wlc->pub->unit,
			radar_type_str,
			wf_chspec_ntoa(WLC_BAND_PI_RADIO_CHANSPEC, chanbuf),
			radar_interval, min_pw,
			(dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
			WLC_DFS_RADAR_CHECK_INTERVAL));
#endif /* BCMDBG || WLMSG_DFS */

		/* clear one-shot radar simulator */
		wlc_11h_set_spect_state(wlc->m11h, cfg, RADAR_SIM, 0);
		return TRUE;
	} else
		return FALSE;
}

/* set cacstate to IDLE and un-mute */
static void
wlc_dfs_cacstate_idle_set(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;

	dfs->dfs_cac.status.state = WL_DFS_CACSTATE_IDLE;
	wlc_mute(wlc, OFF, PHY_MUTE_FOR_PREISM);

	WL_DFS(("wl%d: dfs : state to %s channel %d at %dms\n",
		wlc->pub->unit, wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC),
		(dfs->dfs_cac.cactime -
		dfs->dfs_cac.duration)*WLC_DFS_RADAR_CHECK_INTERVAL));

	dfs->dfs_cac.cactime =  /* unit in WLC_DFS_RADAR_CHECK_INTERVAL */
	dfs->dfs_cac.duration = wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);
}

/* set cacstate to ISM and un-mute */
static void
wlc_dfs_cacstate_ism_set(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	int  cal_mode;

	dfs->dfs_cac.status.chanspec_cleared = WLC_BAND_PI_RADIO_CHANSPEC;
	 /* clear the channel */
	wlc_clr_quiet_chanspec(wlc->cmi, dfs->dfs_cac.status.chanspec_cleared);

	dfs->dfs_cac.status.state = WL_DFS_CACSTATE_ISM;
	wlc_mute(wlc, OFF, PHY_MUTE_FOR_PREISM);

	wlc_iovar_getint(wlc, "phy_percal", (int *)&cal_mode);
	wlc_iovar_setint(wlc, "phy_percal", PHY_PERICAL_SPHASE);
	wlc_phy_cal_perical(wlc->band->pi, PHY_PERICAL_UP_BSS);
	wlc_iovar_setint(wlc, "phy_percal", cal_mode);

	WL_DFS(("wl%d: dfs : state to %s channel %d at %dms\n",
		wlc->pub->unit, wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC),
		(dfs->dfs_cac.cactime - dfs->dfs_cac.duration) * WLC_DFS_RADAR_CHECK_INTERVAL));

	dfs->dfs_cac.cactime =  /* unit in WLC_DFS_RADAR_CHECK_INTERVAL */
	dfs->dfs_cac.duration = wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);

#if defined(NDIS) && (NDISVER >= 0x0620)
	{
	wlc_bsscfg_t *bsscfg;
	bsscfg = wlc_get_ap_bsscfg(dfs);
	if (bsscfg)
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_DFS_AP_RESUME, NULL,
		                  WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);
	}
#endif /* NDIS && (NDISVER >= 0x0620) */
}

/* set cacstate to OOC and mute */
static void
wlc_dfs_cacstate_ooc_set(wlc_dfs_info_t *dfs, uint target_state)
{
	wlc_info_t *wlc = dfs->wlc;

	wlc_mute(wlc, ON, PHY_MUTE_FOR_PREISM);

	dfs->dfs_cac.status.state = target_state;

	WL_DFS(("wl%d: dfs : state to %s at %dms\n",
		wlc->pub->unit, wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		(dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
	        WLC_DFS_RADAR_CHECK_INTERVAL));

	dfs->dfs_cac.duration = dfs->dfs_cac.cactime; /* reset it */

#if defined(NDIS) && (NDISVER >= 0x0620)
	{
	wlc_bsscfg_t *bsscfg;
	bsscfg = wlc_get_ap_bsscfg(dfs);
	if (bsscfg)
		wlc_bss_mac_event(wlc, bsscfg, WLC_E_DFS_AP_STOP, NULL,
		                  WLC_E_STATUS_NOCHANS, 0, 0, 0, 0);
	}
#endif /* NDIS && (NDISVER >= 0x0620) */
}

static void
wlc_dfs_cacstate_idle(wlc_dfs_info_t *dfs)
{
	wlc_dfs_timer_delete(dfs);
}

static void
wlc_dfs_cacstate_cac(wlc_dfs_info_t *dfs)
{
	wlc_info_t* wlc = dfs->wlc;
	uint target_state;
	wlc_bsscfg_t *cfg = wlc->cfg;

	(void)cfg;

	if (wlc_radar_detected(dfs) == TRUE) {
		wlc_dfs_chanspec_oos(dfs, WLC_BAND_PI_RADIO_CHANSPEC);

		if (!(dfs->dfs_cac.chanspec_next = wlc_dfs_chanspec(dfs))) {
			/* out of channels */
			if (dfs->dfs_cac.status.state == WL_DFS_CACSTATE_PREISM_CAC) {
				target_state = WL_DFS_CACSTATE_PREISM_OOC;
			} else {
				target_state = WL_DFS_CACSTATE_POSTISM_OOC;
			}
			wlc_dfs_cacstate_ooc_set(dfs, target_state);
			return;
		}

		wlc_do_chanswitch(cfg, dfs->dfs_cac.chanspec_next);

		if (wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) == TRUE) {
			/* do cac with new channel */
			WL_DFS(("wl%d: dfs : state to %s channel %d at %dms\n",
				wlc->pub->unit,
				wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
				CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC),
				(dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
			        WLC_DFS_RADAR_CHECK_INTERVAL));
			/* Switched to new channel, set up correct pre-ISM timer */
			dfs->dfs_cac.duration =
			dfs->dfs_cac.cactime =
				wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_pre_ism);
			return;
		}
		else {
			wlc_dfs_cacstate_idle_set(dfs); /* set to IDLE */
			return;
		}
	}

	if (!dfs->dfs_cac.duration) {
		/* cac completed. un-mute all. resume normal bss operation */
		wlc_dfs_cacstate_ism_set(dfs);
	}
}

static void
wlc_dfs_cacstate_ism(wlc_dfs_info_t *dfs)
{
	wlc_info_t* wlc = dfs->wlc;
	wlc_bsscfg_t *cfg = wlc->cfg, *apcfg;
	wl_chan_switch_t csa;
	int idx;

	if (wlc_radar_detected(dfs) == FALSE)
		return;

	/* radar has been detected */

	if (dfs->dfs_cac.ism_monitor == TRUE) {
		/* channel switching is disabled */
		WL_DFS(("wl%d: dfs : current channel %d is maintained as channel switching is"
		        " disabled.\n",
		        wlc->pub->unit, CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC)));
		return;
	}

	/* radar detected. mark the channel back to QUIET channel */
	wlc_set_quiet_chanspec(wlc->cmi, dfs->dfs_cac.status.chanspec_cleared);
	dfs->dfs_cac.status.chanspec_cleared = 0; /* cleare it */

	/* continue with CSA */
	wlc_dfs_chanspec_oos(dfs, WLC_BAND_PI_RADIO_CHANSPEC);
	dfs->dfs_cac.chanspec_next = wlc_dfs_chanspec(dfs); /* it will be included in csa */

	/* send csa */
	if (!dfs->dfs_cac.chanspec_next) {
	        /* out of channels */
	        /* just use the current channel for csa */
	        csa.chspec = WLC_BAND_PI_RADIO_CHANSPEC;
	} else {
	        csa.chspec = dfs->dfs_cac.chanspec_next;
	}
	csa.mode = DOT11_CSA_MODE_NO_TX;
	csa.count = MAX((WLC_DFS_CSA_MSEC/cfg->current_bss->beacon_period), WLC_DFS_CSA_BEACONS);
	csa.reg = wlc_get_regclass(wlc->cmi, csa.chspec);
	csa.frame_type = CSA_BROADCAST_ACTION_FRAME;
	FOREACH_UP_AP(wlc, idx, apcfg) {
		wlc_csa_do_csa(wlc->csa, apcfg, &csa, FALSE);
	}

	dfs->dfs_cac.status.state = WL_DFS_CACSTATE_CSA;        /* next state */

	WL_DFS(("wl%d: dfs : state to %s channel current %d next %d at %dms, starting CSA"
		" process\n",
		wlc->pub->unit, wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC), CHSPEC_CHANNEL(csa.chspec),
		(dfs->dfs_cac.cactime -
			dfs->dfs_cac.duration)*WLC_DFS_RADAR_CHECK_INTERVAL));

	dfs->dfs_cac.duration = dfs->dfs_cac.cactime =
		wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);
}

/* csa transmission */
static void
wlc_dfs_cacstate_csa(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	wlc_bsscfg_t *cfg = wlc->cfg;

	if ((wlc_11h_get_spect_state(wlc->m11h, cfg) &
	     (NEED_TO_SWITCH_CHANNEL | NEED_TO_UPDATE_BCN)) ||
	    (wlc->block_datafifo & DATA_BLOCK_QUIET))
	        return;

	/* csa completed - TBTT dpc switched channel */

	if (!(dfs->dfs_cac.chanspec_next)) {
	        /* ran out of channels, goto OOC */
	        wlc_dfs_cacstate_ooc_set(dfs, WL_DFS_CACSTATE_POSTISM_OOC);
		return;
	}

	if (wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) == TRUE) {
		if (dfs->dfs_cac.cactime_post_ism) {
			dfs->dfs_cac.status.state = WL_DFS_CACSTATE_POSTISM_CAC;
			WL_DFS(("wl%d: dfs : state to %s at %dms\n",
				wlc->pub->unit,
				wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
			        (dfs->dfs_cac.cactime - dfs->dfs_cac.duration) *
			        WLC_DFS_RADAR_CHECK_INTERVAL));

			dfs->dfs_cac.duration =
			dfs->dfs_cac.cactime =
				wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);
		}
		else {
			wlc_dfs_cacstate_ism_set(dfs);
		}
	}
	else {
		wlc_dfs_cacstate_idle_set(dfs);
	}

	wlc_update_beacon(wlc);
	wlc_update_probe_resp(wlc, TRUE);
}


/*
 * dfs has run Out Of Channel.
 * wait for a channel to come out of Non-Occupancy Period.
 */
static void
wlc_dfs_cacstate_ooc(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	uint    current_time;

	if (!(dfs->dfs_cac.chanspec_next = wlc_dfs_chanspec(dfs))) {
		/* still no channel out of channels. Nothing to do */
		return;
	}

	wlc_do_chanswitch(wlc->cfg, dfs->dfs_cac.chanspec_next);

	if (wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) == TRUE) {
		current_time = (dfs->dfs_cac.cactime -
			dfs->dfs_cac.duration)*WLC_DFS_RADAR_CHECK_INTERVAL;
		BCM_REFERENCE(current_time);

		/* unit of cactime is WLC_DFS_RADAR_CHECK_INTERVAL */
		if (dfs->dfs_cac.status.state == WL_DFS_CACSTATE_PREISM_OOC) {
			dfs->dfs_cac.cactime = dfs->dfs_cac.duration =
				wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_pre_ism);
			dfs->dfs_cac.status.state = WL_DFS_CACSTATE_PREISM_CAC;
		} else {
			dfs->dfs_cac.cactime = dfs->dfs_cac.duration =
				wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_post_ism);
			dfs->dfs_cac.status.state = WL_DFS_CACSTATE_POSTISM_CAC;
		}

		if (dfs->dfs_cac.cactime) {
			wlc_mute(wlc, ON, PHY_MUTE_FOR_PREISM);

			WL_DFS(("wl%d: dfs : state to %s channel %d at %dms\n",
				wlc->pub->unit,
				wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
				CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC), current_time));
		} else {
			/* corresponding cac is disabled */
			wlc_dfs_cacstate_ism_set(dfs);
		}
	} else {
		wlc_dfs_cacstate_idle_set(dfs); /* set to idle */
	}
}

static void
wlc_dfs_cacstate_handler(void *arg)
{
	wlc_dfs_info_t *dfs = (wlc_dfs_info_t *)arg;
	wlc_info_t *wlc = dfs->wlc;

	if (!wlc->pub->up || !dfs->dfs_cac_enabled)
		return;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

	if (WIN7_AND_UP_OS(wlc->pub) && SCAN_IN_PROGRESS(wlc->scan))
		return;

	ASSERT(dfs->dfs_cac.status.state < WL_DFS_CACSTATES);

	dfs->dfs_cac.duration--;

	wlc_dfs_cacstate_fn_ary[dfs->dfs_cac.status.state](dfs);
}

static void
wlc_dfs_cacstate_init(wlc_dfs_info_t *dfs)
{
	wlc_info_t *wlc = dfs->wlc;
	wlc_bsscfg_t *cfg = wlc->cfg;

	ASSERT(WL11H_AP_ENAB(wlc));

	if (!wlc->pub->up)
		return;

	if (wlc_radar_chanspec(wlc->cmi, dfs->dfs_cac.status.chanspec_cleared) == TRUE) {
		 /* restore QUIET setting */
		wlc_set_quiet_chanspec(wlc->cmi, dfs->dfs_cac.status.chanspec_cleared);
	}
	dfs->dfs_cac.status.chanspec_cleared = 0; /* clear it */

	wlc_csa_reset_all(wlc->csa, cfg);
	wlc_quiet_reset_all(wlc->quiet, cfg);

	if (wlc_radar_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) == TRUE) {
		wlc_dfs_timer_add(dfs);

		/* unit of cactime is WLC_DFS_RADAR_CHECK_INTERVAL */
		dfs->dfs_cac.cactime = wlc_dfs_ism_cactime(wlc, dfs->dfs_cac.cactime_pre_ism);
		if (dfs->dfs_cac.cactime) {
			/* preism cac is enabled */
			dfs->dfs_cac.status.state = WL_DFS_CACSTATE_PREISM_CAC;
			dfs->dfs_cac.duration = dfs->dfs_cac.cactime;
			wlc_mute(wlc, ON, PHY_MUTE_FOR_PREISM);
		} else {
			/* preism cac is disabled */
			wlc_dfs_cacstate_ism_set(dfs);
		}

		wlc_radar_detected(dfs); /* refresh detector */

	} else {
		wlc_dfs_cacstate_idle_set(dfs); /* set to idle */
	}

	dfs->dfs_cac_enabled = TRUE;

	WL_REGULATORY(("wl%d: %s: state to %s channel %d \n",
		wlc->pub->unit, __FUNCTION__,
		wlc_dfs_cacstate_str[dfs->dfs_cac.status.state],
		CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC)));
}

void
wlc_dfs_setchanspec(wlc_dfs_info_t *dfs, chanspec_t chanspec)
{
	wlc_info_t *wlc = dfs->wlc;

	(void)wlc;

	if (!AP_ENAB(wlc->pub))
		return;

	if (WL11H_AP_ENAB(wlc) &&
	    CHSPEC_IS5G(chanspec)) {
#if defined(BCMDBG)
		char chanbuf[CHANSPEC_STR_LEN];
#endif
		dfs->dfs_cac.chanspec_forced = chanspec;
#if defined(BCMDBG)
		WL_DFS(("dfs: next dfs chanspec is forced to %s (%04x)\n",
			wf_chspec_ntoa(chanspec, chanbuf), chanspec));
#endif
	}
}

bool
wlc_dfs_chanspec_forced(wlc_dfs_info_t *dfs)
{
	return (dfs->dfs_cac.chanspec_forced != 0);
}

void
wlc_set_dfs_cacstate(wlc_dfs_info_t *dfs, int state)
{
	wlc_info_t *wlc = dfs->wlc;

	(void)wlc;

	if (!AP_ENAB(wlc->pub))
		return;

	if (state == OFF) {
		if (dfs->dfs_cac_enabled) {
			wlc_dfs_cacstate_idle_set(dfs);
			wlc_dfs_cacstate_idle(dfs);
			dfs->dfs_cac_enabled = FALSE;
		}
	} else {
		if (!dfs->dfs_cac_enabled) {
			wlc_dfs_cacstate_init(dfs);
			dfs->dfs_cac_enabled = TRUE;
		}
	}
}

chanspec_t
wlc_dfs_sel_chspec(wlc_dfs_info_t *dfs, bool force)
{
	wlc_info_t *wlc = dfs->wlc;

	(void)wlc;

	if (!force && dfs->dfs_cac.chanspec_next != 0)
		return dfs->dfs_cac.chanspec_next;

	dfs->dfs_cac.chanspec_next = wlc_dfs_chanspec(dfs);

	WL_REGULATORY(("wl%d: %s: dfs selected channel %d\n",
	               wlc->pub->unit, __FUNCTION__,
	               CHSPEC_CHANNEL(dfs->dfs_cac.chanspec_next)));

	return dfs->dfs_cac.chanspec_next;
}

void
wlc_dfs_reset_all(wlc_dfs_info_t *dfs)
{
	bzero(dfs->chan_blocked, sizeof(dfs->chan_blocked));
}

int
wlc_dfs_set_radar(wlc_dfs_info_t *dfs, int radar)
{
	wlc_info_t *wlc = dfs->wlc;
	wlcband_t *band5G;

	if (radar < 0 || radar > (int)WL_RADAR_SIMULATED) {
		return BCME_RANGE;
	}

	/*
	 * WL_RADAR_SIMULATED is required for Wi-Fi testing.
	 */

	/* Radar must be enabled to pull test trigger */
	if (radar == (int)WL_RADAR_SIMULATED) {
		wlc_bsscfg_t *cfg = wlc->cfg;

		if (dfs->radar != 1) {
			return BCME_BADARG;
		}

		/* Can't do radar detect on non-radar channel */
		if (wlc_radar_chanspec(wlc->cmi, wlc->home_chanspec) != TRUE) {
			return BCME_BADCHAN;
		}

		wlc_11h_set_spect_state(wlc->m11h, cfg, RADAR_SIM, RADAR_SIM);
		return BCME_OK;
	}

	if ((int)dfs->radar == radar) {
		return BCME_OK;
	}

	/* Check there is a 5G band available */
	if (BAND_5G(wlc->band->bandtype)) {
		band5G = wlc->band;
	} else if (NBANDS(wlc) > 1 &&
	           BAND_5G(wlc->bandstate[OTHERBANDUNIT(wlc)]->bandtype)) {
		band5G = wlc->bandstate[OTHERBANDUNIT(wlc)];
	} else {
		return BCME_BADBAND;
	}

	/* bcmerror if APhy rev 3+ support in any bandunit */
	if (WLCISAPHY(band5G) && AREV_LT(band5G->phyrev, 3)) {
		return BCME_UNSUPPORTED;
	}

	dfs->radar = (uint32)radar;

	wlc_phy_radar_detect_enable(wlc->band->pi, radar != 0);

	/* if we are not currently on the APhy, then radar detect
	 * will be initialized in the phy init
	 */

	return BCME_OK;
}

uint32
wlc_dfs_get_radar(wlc_dfs_info_t *dfs)
{
	return dfs->radar;
}

#endif /* WLDFS */
