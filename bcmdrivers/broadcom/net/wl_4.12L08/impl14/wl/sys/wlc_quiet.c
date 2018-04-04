/*
 * 802.11h Quiet module source file
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
 * $Id: wlc_quiet.c 365823 2012-10-31 04:24:30Z $
 */

#include <wlc_cfg.h>

#ifdef WLQUIET

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
#include <wlc_bmac.h>
#include <wlc_assoc.h>
#include <wlc_alloc.h>
#include <wl_export.h>
#include <wlc_11h.h>
#include <wlc_quiet.h>
#include <wlc_utils.h>

/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_QUIET,	/* send Quiet IE in beacons */
	IOV_LAST
};

static const bcm_iovar_t wlc_quiet_iovars[] = {
#ifdef AP
	{"quiet", IOV_QUIET, (IOVF_SET_UP|IOVF_BSSCFG_AP_ONLY), IOVT_BUFFER, sizeof(dot11_quiet_t)},
#endif
	{NULL, 0, 0, 0, 0}
};

/* ioctl table */
static const wlc_ioctl_cmd_t wlc_quiet_ioctls[] = {
	{WLC_SEND_QUIET, WLC_IOCF_DRIVER_UP|WLC_IOCF_BSSCFG_AP_ONLY, sizeof(dot11_quiet_t)}
};

/* Quiet module info */
struct wlc_quiet_info {
	wlc_info_t *wlc;
	int cfgh;			/* bsscfg cubby handle */
};

/* local functions */
/* module */
static int wlc_quiet_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_quiet_doioctl(void *ctx, int cmd, void *arg, int len, struct wlc_if *wlcif);

/* cubby */
static int wlc_quiet_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_quiet_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg);
#ifdef BCMDBG
static void wlc_quiet_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_quiet_bsscfg_dump NULL
#endif

/* up/down */
static void wlc_quiet_bsscfg_up_down(void *ctx, bsscfg_up_down_event_data_t *evt_data);

/* timer */
static void wlc_quiet_timer(void *arg);

/* STA quiet procedures */
static void wlc_start_quiet0(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg);
static void wlc_start_quiet1(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg);
static void wlc_start_quiet2(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg);
static void wlc_start_quiet3(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg);

/* cubby structure and access macros */
typedef struct wlc_quiet {
	uint32 ext_state;	/* external states */
	uint32 state;		/* internal states */
	uint32 start_prep_tsf;	/* When to start preparing to be quiet(ie shutdown FIFOs) */
	uint32 start_tsf;	/* When to start being quiet */
	uint32 end_tsf;		/* When we can start xmiting again */
	/* dual purpose: wait for offset with beacon & wait for duration */
	struct wl_timer *timer;
	/* quiet info */
	uint8 count;
	uint8 period;
	uint16 duration;
	uint16 offset;
} wlc_quiet_t;
#define QUIET_BSSCFG_CUBBY_LOC(qm, cfg) ((wlc_quiet_t **)BSSCFG_CUBBY(cfg, (qm)->cfgh))
#define QUIET_BSSCFG_CUBBY(qm, cfg) (*QUIET_BSSCFG_CUBBY_LOC(qm, cfg))

/* quiet->state */
#define WAITING_FOR_FLUSH_COMPLETE 	(1 << 0)
#define WAITING_FOR_INTERVAL		(1 << 1)
#define WAITING_FOR_OFFSET		(1 << 2)
#define WAITING_FOR_DURATION		(1 << 3)

/* module */
wlc_quiet_info_t *
BCMATTACHFN(wlc_quiet_attach)(wlc_info_t *wlc)
{
	wlc_quiet_info_t *qm;

	if ((qm = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_quiet_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	qm->wlc = wlc;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((qm->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(wlc_quiet_t *),
	                wlc_quiet_bsscfg_init, wlc_quiet_bsscfg_deinit, wlc_quiet_bsscfg_dump,
	                (void *)qm)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register bsscfg up/down callbacks */
	if (wlc_bsscfg_updown_register(wlc, wlc_quiet_bsscfg_up_down, qm) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	if (wlc_module_register(wlc->pub, wlc_quiet_iovars, "quiet", (void *)qm, wlc_quiet_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	if (wlc_module_add_ioctl_fn(wlc->pub, qm, wlc_quiet_doioctl,
	                            ARRAYSIZE(wlc_quiet_ioctls), wlc_quiet_ioctls) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	return qm;

	/* error handling */
fail:
	wlc_quiet_detach(qm);
	return NULL;
}

void
BCMATTACHFN(wlc_quiet_detach)(wlc_quiet_info_t *qm)
{
	wlc_info_t *wlc;

	if (qm == NULL)
		return;

	wlc = qm->wlc;

	/* unregister bsscfg up/down callbacks */
	wlc_bsscfg_updown_unregister(wlc, wlc_quiet_bsscfg_up_down, qm);

	wlc_module_remove_ioctl_fn(wlc->pub, qm);
	wlc_module_unregister(wlc->pub, "quiet", qm);

	MFREE(wlc->osh, qm, sizeof(wlc_quiet_info_t));
}

static int
wlc_quiet_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_quiet_info_t *qm = (wlc_quiet_info_t *)ctx;
	wlc_info_t *wlc = qm->wlc;
	wlc_bsscfg_t *cfg;
	int err = BCME_OK;
	int32 int_val = 0;
	int32 *ret_int_ptr;

	/* update bsscfg w/provided interface context */
	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	/* Do the actual parameter implementation */
	switch (actionid) {
#ifdef AP
	case IOV_SVAL(IOV_QUIET):
#ifdef RADIO_PWRSAVE
		/*
		 * Check for radio power save feature enabled
		 */
		if (RADIO_PWRSAVE_ENAB(wlc->ap)) {
			WL_ERROR(("Please disable Radio Power Save feature using"
			          "radio_pwrsave_enable IOVAR"
			          "to continue with quiet IE testing\n"));
			err = BCME_ERROR;
			break;
		}
#endif
		wlc_quiet_do_quiet(qm, cfg, (dot11_quiet_t *)arg);

		break;
#endif /* AP */

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_quiet_doioctl(void *ctx, int cmd, void *arg, int len, struct wlc_if *wlcif)
{
	wlc_quiet_info_t *qm = (wlc_quiet_info_t *)ctx;
	wlc_info_t *wlc = qm->wlc;
	int err = BCME_OK;

	switch (cmd) {
	case WLC_SEND_QUIET:
		err = wlc_iovar_op(wlc, "quiet", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* bsscfg cubby */
static int
wlc_quiet_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_quiet_info_t *qm = (wlc_quiet_info_t *)ctx;
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t **pquiet = QUIET_BSSCFG_CUBBY_LOC(qm, cfg);
	wlc_quiet_t *quiet;
	int err;

	if ((quiet = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_quiet_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	*pquiet = quiet;

	/* init quiet timer (for STA only) */
	if ((quiet->timer =
	     wl_init_timer(wlc->wl, wlc_quiet_timer, cfg, "quiet")) == NULL) {
		WL_ERROR(("wl%d: wl_init_timer for bsscfg %d quiet timer failed\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		err = BCME_NORESOURCE;
		goto fail;
	}

	return BCME_OK;

fail:
	if (quiet != NULL)
		wlc_quiet_bsscfg_deinit(ctx, cfg);
	return err;
}

static void
wlc_quiet_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_quiet_info_t *qm = (wlc_quiet_info_t *)ctx;
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t **pquiet = QUIET_BSSCFG_CUBBY_LOC(qm, cfg);
	wlc_quiet_t *quiet = *pquiet;

	if (quiet == NULL) {
		WL_ERROR(("wl%d: %s: Quiet info not found\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* delete quiet timer */
	if (quiet->timer != NULL) {
		wl_free_timer(wlc->wl, quiet->timer);
		quiet->timer = NULL;
	}

	MFREE(wlc->osh, quiet, sizeof(wlc_quiet_t));
	*pquiet = NULL;
}

#ifdef BCMDBG
static void
wlc_quiet_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_quiet_info_t *qm = (wlc_quiet_info_t *)ctx;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	ASSERT(quiet != NULL);

	/* Quiet info */
	bcm_bprintf(b, "timer: %p\n", quiet->timer);
	bcm_bprintf(b, "ie: period %d count %d, offset %d duration %d\n",
	            quiet->period, quiet->count, quiet->offset, quiet->duration);
	bcm_bprintf(b, "ext_state: 0x%x state: 0x%x\n", quiet->ext_state, quiet->state);
	bcm_bprintf(b, " start_prep_tsf: %x start_tsf: %x end_tsf: %x\n",
	            quiet->start_prep_tsf, quiet->start_tsf, quiet->end_tsf);
}
#endif /* BCMDBG */

/* bsscfg up/down callbacks */
static void
wlc_quiet_bsscfg_up_down(void *ctx, bsscfg_up_down_event_data_t *evt_data)
{
	wlc_quiet_info_t *qm = (wlc_quiet_info_t *)ctx;
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, evt_data->bsscfg);

	/* Only process bsscfg down events. */
	if (!evt_data->up) {
		ASSERT(quiet != NULL);

		quiet->ext_state = 0;
		quiet->state = 0;

		/* cancel any quiet timer */
		evt_data->callbacks_pending +=
		   wl_del_timer(wlc->wl, quiet->timer) ? 0 : 1;
	}
}

/* Maximum FIFO drain is how long it takes to fully drain the FIFO when transmitting at the slowest
 * supported rate (1Mbps).
 */
#define MAX_FIFO_DRAIN_TIME	33	/* millisecs */

#ifdef STA
/*
 *    Theory of quiet mode:
 *	AP sends quiet mode info element.
 *	wlc_11h_quiet()-parses IE. Take into account the overhead in
 *	to shutting down the FIFOs and determine
 *	proper interval and offset. Register quiet0 to be called when
 *	we have reached the target beacon interval.
 *
 *	step0() -	Now in correct beacon interval but need to wait
 *			for the right offset within beacon.  Use OS timer
 *			then proceed to step1().
 *
 *	step1()-	Timer dinged and we are now at the right place to start
 *			shutting down FIFO's. Wait for FIFO drain interrupts.
 *
 *	step2()-	Interrupts came in. We are now mute. Set timer
 *			when its time to come out of quiet mode.
 *
 *	step3()-	Timer dinged. Resume normal operation.
 *
 */

/* Came from: wlc_parse_11h()
 * Will go to: wlc_start_quiet0()
 */
void
wlc_11h_quiet(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg,
	dot11_quiet_t *ie, struct dot11_bcn_prb *bcn)
{
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);
	wlc_bss_info_t *current_bss = cfg->current_bss;
	uint32 bi_us = (uint32)current_bss->beacon_period * DOT11_TU_TO_US;
	uint32 tsf_h, tsf_l;
	uint32 delta_tsf_l, delta_tsf_h;
	uint32 offset, beacon_start;
	int32 countdown;

	ASSERT(quiet != NULL);

	/*
	 * Ignore further requests for quiet if we're already handling one or if it's a "holdoff"
	 * one.
	 *
	 * In "holdoff", Cisco APs continually send Quiet IEs with count=255 instead of omitting the
	 * Quiet IE when quiet period is disabled. It's safe to ignore these because when the quiet
	 * period is enabled, we'll eventually see a non-255 Quiet IE as the AP counts down to the
	 * quiet period.
	 */
	if (quiet->state != 0 || quiet->count == 255) {
		WL_REGULATORY(("quiet: rcvd but ignoring:count %d, period %d, duration %d, offset"
			" %d\n",
			ie->count, ie->period, ltoh16(ie->duration), ltoh16(ie->offset)));
		return;
	}

	/* Parse info and setup to wait for the right beacon interval */
	WL_REGULATORY(("quiet: count %d, period %d, duration %d, offset %d\n",
		ie->count, ie->period, ltoh16(ie->duration), ltoh16(ie->offset)));

	quiet->duration = ltoh16(ie->duration);	/* TU's */
	quiet->offset = ltoh16(ie->offset);	/* TU's offset from TBTT in Count field */
	quiet->period = ie->period;
	countdown = ie->count;			/* num beacons until start */

	if (!countdown)
		return;

	/* The FIFO's will take a while to drain.  If the offset is less than that
	 * time, back up into the previous beacon interval.
	 */
	if (quiet->offset < MAX_FIFO_DRAIN_TIME) {
		countdown--;
	}

	/* Calculate absolute TSF of when to start preparing to be quiet */
	tsf_l = ltoh32_ua(&bcn->timestamp[0]);
	tsf_h = ltoh32_ua(&bcn->timestamp[1]);
	offset = wlc_calc_tbtt_offset(ltoh16(bcn->beacon_interval), tsf_h, tsf_l);
	beacon_start = tsf_l - offset;

	/* Get delta between local running tsf and beacon timestamp */
	wlc_read_tsf(wlc, &delta_tsf_l, &delta_tsf_h);
	wlc_uint64_sub(&delta_tsf_h, &delta_tsf_l, tsf_h, tsf_l); /* a - b => a */

	/* usecs for starting quiet period */
	quiet->start_tsf = beacon_start + delta_tsf_l + (bi_us * ie->count) +
		(quiet->offset * DOT11_TU_TO_US);
	quiet->end_tsf = quiet->start_tsf + quiet->duration * DOT11_TU_TO_US;

	if ((!WIN7_AND_UP_OS(wlc->pub) || AP_ACTIVE(wlc)) && APSTA_ENAB(wlc->pub)) {
		uint32 qoffset;
		uint32 mytsf_l, mytsf_h, myoffset;

		wlc_read_tsf(wlc, &mytsf_l, &mytsf_h);
		myoffset = wlc_calc_tbtt_offset(current_bss->beacon_period, mytsf_h, mytsf_l);
		qoffset = (uint32)(quiet->offset) + (myoffset - offset);
		if (qoffset < MAX_FIFO_DRAIN_TIME) {
			if (countdown == ie->count) {
				countdown--;
			}
		}

		quiet->offset = (int)qoffset;
		quiet->start_tsf += (mytsf_l - tsf_l);
		quiet->end_tsf = quiet->start_tsf + quiet->duration * DOT11_TU_TO_US;
	}

	/* Save this for last cuz this is what the dpc looks at */
	quiet->state = WAITING_FOR_INTERVAL;
	quiet->count = (uint8)countdown;
	WL_REGULATORY(("%s: Set timer for = %d TBTTs\n", __FUNCTION__, countdown));
	if (countdown == 0) {
		/* We need to go quiet in this interval, jump straight to offset. */
		wlc_start_quiet0(qm, cfg);
	} else {
		wl_add_timer(wlc->wl, quiet->timer,
			((current_bss->beacon_period * DOT11_TU_TO_US)/1000) * countdown, 0);
	}

	/* When the quiet timer expires, wlc_start_quiet0() will be called in timer handler
	 * wlc_quiet_timer()
	 */
}
#endif /* STA */

/* Triple use timer handler: Either
 *	- Waiting to advance to 'count' tbtts into offset waiting
 *	- Or waiting to advance to 'offset' usecs into bcn interval to start quiet period.
 *	- Or waiting for end of quiet period.
 *	Came from:  wlc_11h_quiet() via timer interrupt, wlc_start_quiet0() via timer interrupt, or
 *		wlc_start_quiet2() via timer interrupt.
 *	Will go to: wlc_start_quiet0(), wlc_start_quiet1() or wlc_start_quiet3()
 */
static void
wlc_quiet_timer(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t*)arg;
	wlc_info_t *wlc = cfg->wlc;
	wlc_quiet_info_t *qm = wlc->quiet;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	if (!wlc->pub->up)
		return;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

	WL_REGULATORY(("Entering %s\n", __FUNCTION__));

	if (quiet->state & WAITING_FOR_INTERVAL) {
		wlc_start_quiet0(qm, cfg);
		return;
	}
	if (quiet->state & WAITING_FOR_OFFSET) {
		wlc_start_quiet1(qm, cfg);
	}
	if (quiet->state & WAITING_FOR_DURATION) {
		wlc_start_quiet3(qm, cfg);
	}
}

/* Now in correct beacon interval but need to wait for offset.
 * Came from: quiet timer handler wlc_quiet_timer()
 * Will go to: wlc_start_quiet1() via wlc_quiet_timer()
 */
static void
wlc_start_quiet0(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);
	uint32 tsf_hi, tsf_lo;
	uint32 start_prep_ms;
	uint32 start_prep_tsf;
	uint32 cur_rate;
	uint32 fifo_drain_time;

	ASSERT(quiet != NULL);

	ASSERT(quiet->state & WAITING_FOR_INTERVAL);
	WL_REGULATORY(("Entering %s\n", __FUNCTION__));

	quiet->state = WAITING_FOR_OFFSET;
	/*
	 * Adjust offset based on estimated FIFO drain time (based on current TX rate).
	 *
	 * This is using a conservative version of the formula so that no matter how fast we
	 * transmit, we'll have a minimum FIFO drain time of 1ms.
	 */
	cur_rate = RSPEC2KBPS(wlc_get_rspec_history(cfg)) / 1000;
	fifo_drain_time = ((MAX_FIFO_DRAIN_TIME - 1) / cur_rate) + 1;
	WL_REGULATORY(("%s: txrate=%d drain_time=%d\n", __FUNCTION__, cur_rate, fifo_drain_time));

	start_prep_tsf = quiet->start_tsf - fifo_drain_time * 1000;

	wlc_read_tsf(wlc, &tsf_lo, &tsf_hi);

	if (((tsf_lo == (uint32)-1) && (tsf_hi == (uint32)-1)) || (tsf_lo > quiet->end_tsf) ||
	    (tsf_lo >= start_prep_tsf)) {
		/* Already late so call routine directly instead of through timer. */
		WL_REGULATORY(("%s: Already late, call quiet1 directly. \n", __FUNCTION__));
		wlc_start_quiet1(qm, cfg);
	} else {
		start_prep_ms = (start_prep_tsf - tsf_lo) / 1000;
		/* Set timer for difference (in millisecs) */
		WL_REGULATORY(("%s: Start FIFO drain in %d ms\n",
			__FUNCTION__, start_prep_ms));
		wl_add_timer(wlc->wl, quiet->timer, start_prep_ms, 0);
	}
}

/* Called from wlc_quiet_timer() when we are at proper offset within
 * proper beacon.  Need to initiate quiet period now.
 * Came from: wlc_start_quiet0() via wlc->quiet_timer.
 * Will go to: wlc_start_quiet2().
 */
static void
wlc_start_quiet1(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	ASSERT(quiet != NULL);


	WL_REGULATORY(("Entering %s\n", __FUNCTION__));

	ASSERT(quiet->state & WAITING_FOR_OFFSET);
	quiet->state &= ~WAITING_FOR_OFFSET;

	if (wlc_mac_request_entry(wlc, cfg, WLC_ACTION_QUIET) != BCME_OK) {
		/* blocked from entry, cancel quiet period */
		quiet->ext_state = 0;
		quiet->state = 0;	/* All done */
		return;
	}

	/* reject future scans until done with quiet */
	quiet->ext_state |= SILENCE;

	/* Send feedback upstream */
	wlc_bss_mac_event(wlc, cfg, WLC_E_QUIET_START, NULL, WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);

	wlc_bmac_tx_fifo_suspend(wlc->hw, TX_DATA_FIFO);
	wlc_bmac_tx_fifo_suspend(wlc->hw, TX_CTL_FIFO);

	quiet->state |= WAITING_FOR_FLUSH_COMPLETE;

	/* Now wait for FIFO drain interrupt and proceed to wlc_start_quiet2() */
}

/* Fifo's have drained, now be quiet.
 * Came from: wlc_start_quiet1() via dpc FIFO drain handler.
 * Will go to: wlc_start_quiet3() via wlc_quiet_timer.
 */
static void
wlc_start_quiet2(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);
	uint32 tsf_hi, tsf_lo;

	ASSERT(quiet != NULL);

	WL_REGULATORY(("Entering %s\n", __FUNCTION__));
	ASSERT(!(quiet->state & WAITING_FOR_FLUSH_COMPLETE));

	/* zero the address match register so we do not send ACKs */
	wlc_clear_mac(cfg);

	wlc_read_tsf(wlc, &tsf_lo, &tsf_hi);

	quiet->state |= WAITING_FOR_DURATION;

	if (((tsf_lo == (uint32)-1) && (tsf_hi == (uint32)-1)) || (tsf_lo > quiet->end_tsf)) {
		/* wlc_read_tsf failed, or end the quiet period immediately */
		WL_REGULATORY(("%s; Warning: ending quiet immediatly\n", __FUNCTION__));
		WL_REGULATORY(("Current tsf: tsf_h 0x%x, tsf_l 0x%x\n", tsf_hi, tsf_lo));
		WL_REGULATORY(("Start_tsf(lo) 0x%x, End_tsf(lo) 0x%x\n",
			quiet->start_tsf, quiet->end_tsf));
		wlc_start_quiet3(qm, cfg);
	}
	else {
		/* Set timer for difference (in millisecs) */
		WL_REGULATORY(("Arming timer to end quiet period in 0x%x - 0x%x = %d ms\n",
			quiet->end_tsf, tsf_lo, (quiet->end_tsf - tsf_lo) / 1000));

		wl_add_timer(wlc->wl, quiet->timer, (quiet->end_tsf - tsf_lo) / 1000, 0);
	}
}

/* Done with quiet period.
 * Called from: timer via wlc_start_quiet2().
 * Will go to: wlc_quiet_timer()->dpc handler.
 */
static void
wlc_start_quiet3(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	ASSERT(quiet != NULL);

	WL_REGULATORY(("Entering %s\n", __FUNCTION__));
	ASSERT(quiet->state & WAITING_FOR_DURATION);
	quiet->state &= ~WAITING_FOR_DURATION;

	/* restore the address match register for this cfg */
	wlc_set_mac(cfg);

	wlc_mute(wlc, OFF, 0);

	wlc_bss_mac_event(wlc, cfg, WLC_E_QUIET_END, NULL, WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);

	WL_REGULATORY(("%s:Quiet Complete\n", __FUNCTION__));
	quiet->ext_state = 0;
	quiet->state = 0;	/* All done */
}

void
wlc_quiet_txfifo_suspend_complete(wlc_quiet_info_t *qm)
{
	wlc_info_t *wlc = qm->wlc;
	int idx;
	wlc_bsscfg_t *cfg;

	FOREACH_AS_STA(wlc, idx, cfg) {
		wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);
		ASSERT(quiet != NULL);

		if (quiet->state & WAITING_FOR_FLUSH_COMPLETE) {
			quiet->state &= ~WAITING_FOR_FLUSH_COMPLETE;
			wlc_start_quiet2(qm, cfg);
		}
	}
}

uint8 *
wlc_quiet_write_ie(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg, uint8 *cp, int len)
{
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	ASSERT(quiet != NULL);

	if (quiet->count != 0) {
		if (len >= (int)sizeof(dot11_quiet_t)) {
			dot11_quiet_t *qie = (dot11_quiet_t *)cp;

			qie->id = DOT11_MNG_QUIET_ID;
			qie->len = 6;
			qie->count = quiet->count--;
			qie->period = quiet->period;
			qie->duration = htol16(quiet->duration);
			qie->offset = htol16(quiet->offset);
			cp += sizeof(dot11_quiet_t);
		}
		else {
			WL_ERROR(("%s, line %d, quiet ie dropped, buffer too short\n",
			          __FUNCTION__, __LINE__));
		}
	}

	return cp;
}

void
wlc_quiet_do_quiet(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg, dot11_quiet_t *qie)
{
	wlc_info_t *wlc = qm->wlc;
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	ASSERT(quiet != NULL);

	quiet->count = qie->count;
	quiet->period = qie->period;
	quiet->duration = qie->duration;
	quiet->offset = qie->offset;

	WL_REGULATORY(("wl%d: Quiet ioctl: count %d, dur %d, offset %d\n",
	               wlc->pub->unit, qie->count, qie->duration, qie->offset));

	/* Update the beacon/probe response */
	wlc_11h_set_spect_state(wlc->m11h, cfg, NEED_TO_UPDATE_BCN, NEED_TO_UPDATE_BCN);
	wlc_bss_update_beacon(wlc, cfg);
	wlc_bss_update_probe_resp(wlc, cfg, TRUE);
}

void
wlc_quiet_reset_all(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg)
{
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	ASSERT(quiet != NULL);

	quiet->count = 0;
}

/* accessors */
uint
wlc_quiet_get_quiet_state(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg)
{
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	ASSERT(quiet != NULL);

	return quiet->ext_state;
}

uint
wlc_quiet_get_quiet_count(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg)
{
	wlc_quiet_t *quiet = QUIET_BSSCFG_CUBBY(qm, cfg);

	ASSERT(quiet != NULL);

	return quiet->count;
}

#endif /* WLQUIET */
