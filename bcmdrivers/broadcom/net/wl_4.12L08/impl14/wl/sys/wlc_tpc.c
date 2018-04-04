/*
 * 802.11h TPC and wl power control module source file
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_tpc.c 363337 2012-10-17 06:29:29Z $
 */

#include <wlc_cfg.h>

#ifdef WLTPC
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
#include <wlc_channel.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_alloc.h>
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_channel.h>
#include <wlc_clm.h>
#include <wlc_stf.h>

/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_TPC_RPT_OVERRIDE,
	IOV_AP_TPC_MODE,	/* ap tpc mode */
	IOV_AP_TPC_PERIOD,	/* ap tpc periodicity */
	IOV_AP_TPC_LM,		/* ap tpc link margins */
	IOV_CONSTRAINT,
	IOV_CURPOWER,
	IOV_LAST
};

static const bcm_iovar_t wlc_tpc_iovars[] = {
#if defined(WLTEST)
	{"tpc_rpt_override", IOV_TPC_RPT_OVERRIDE, (0), IOVT_UINT16, 0},
#endif
#ifdef WL_AP_TPC
	{"tpc_mode", IOV_AP_TPC_MODE, (0), IOVT_UINT8, 0},
	{"tpc_period", IOV_AP_TPC_PERIOD, (0), IOVT_UINT8, 0},
	{"tpc_lm", IOV_AP_TPC_LM, (0), IOVT_UINT16, 0},
#endif
	{"constraint", IOV_CONSTRAINT, (0), IOVT_UINT8, 0},
#if defined(WL_EXPORT_CURPOWER)
	{"curpower", IOV_CURPOWER, (IOVF_GET_UP), IOVT_UINT8, 0},
#endif
	{NULL, 0, 0, 0, 0}
};

/* ioctl table */
static const wlc_ioctl_cmd_t wlc_tpc_ioctls[] = {
	{WLC_SEND_PWR_CONSTRAINT, 0, sizeof(int)}
};

/* TPC module info */
struct wlc_tpc_info {
	wlc_info_t *wlc;
	uint16 tpc_rpt_override;	/* overrides for tpc report. */
#ifdef WL_AP_TPC
	uint8 ap_tpc;
	uint8 ap_tpc_interval;
	int scbh;			/* scb cubby handle */
	int cfgh;			/* bsscfg cubby handle */
#endif
	int8 txpwr_local_max;		/* regulatory local txpwr max */
	uint8 txpwr_local_constraint;	/* local power contraint in dB */
	uint8 pwr_constraint;
};

/* local functions */
static int wlc_tpc_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_tpc_watchdog(void *ctx);
#ifdef BCMDBG
static int wlc_tpc_dump(void *ctx, struct bcmstrbuf *b);
#endif
static int wlc_tpc_doioctl(void *ctx, int cmd, void *arg, int len, struct wlc_if *wlcif);

#ifdef WL_AP_TPC
static int wlc_ap_tpc_scb_init(void *ctx, struct scb *scb);
static void wlc_ap_tpc_scb_deinit(void *ctx, struct scb *scb);
static int wlc_ap_tpc_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_ap_tpc_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_ap_tpc_req(wlc_tpc_info_t *tpc);
static int wlc_ap_bss_tpc_get(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg);
#ifdef BCMDBG
static void wlc_ap_tpc_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b);
static void wlc_ap_tpc_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_ap_tpc_scb_dump NULL
#define wlc_ap_tpc_bsscfg_dump NULL
#endif
#endif /* WL_AP_TPC */

#ifdef WL_AP_TPC
static int wlc_ap_tpc_setup(wlc_tpc_info_t *tpc, uint8 mode);
static void wlc_ap_tpc_rpt_upd(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr,
	dot11_tpc_rep_t *rpt, int8 rssi, ratespec_t rspec);
#else
#define wlc_ap_tpc_setup(tpc, mode) BCME_OK
#define wlc_ap_tpc_rpt_upd(tpc, cfg, hdr, rpt, rssi, rspec) do {} while (0)
#endif /* !WL_AP_TPC */

#ifndef WLC_NET80211
#if defined(WLTEST)
static void wlc_tpc_rpt_ovrd(wlc_tpc_info_t *tpc, dot11_tpc_rep_t *rpt);
#else
#define wlc_tpc_rpt_ovrd(tpc, rpt) do {} while (0)
#endif
#endif /* WLC_NET80211 */

#if defined(WL_EXPORT_CURPOWER) && defined(PPR_API)
static int wlc_tpc_get_current(wlc_tpc_info_t *tpc, void *pwr, uint len, wlc_bsscfg_t *bsscfg);
#else
#define wlc_tpc_get_current(tpc, pwr, len) BCME_ERROR
#endif

#ifdef WL_AP_TPC
typedef struct {
	int8 sta_link_margin;	/* STAs present link margin */
	int8 ap_link_margin;	/* APs present link margin */
} ap_tpc_scb_cubby_t;

#define AP_TPC_SCB_CUBBY(tpc, scb) ((ap_tpc_scb_cubby_t *)SCB_CUBBY(scb, (tpc)->scbh))

typedef struct {
	int8 sta_link_margin;	/* STAs present link margin */
	int8 ap_link_margin;	/* APs present link margin */
} ap_tpc_bsscfg_cubby_t;

#define AP_TPC_BSSCFG_CUBBY(tpc, cfg) ((ap_tpc_bsscfg_cubby_t *)BSSCFG_CUBBY(cfg, (tpc)->cfgh))
#endif /* WL_AP_TPC */

wlc_tpc_info_t *
BCMATTACHFN(wlc_tpc_attach)(wlc_info_t *wlc)
{
	wlc_tpc_info_t *tpc;

	if ((tpc = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_tpc_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	tpc->wlc = wlc;

#ifdef WL_AP_TPC
	/* reserve cubby in the scb container for per-scb private data */
	if ((tpc->scbh = wlc_scb_cubby_reserve(wlc, sizeof(ap_tpc_scb_cubby_t),
	                wlc_ap_tpc_scb_init, wlc_ap_tpc_scb_deinit, wlc_ap_tpc_scb_dump,
	                (void *)tpc)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((tpc->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(ap_tpc_bsscfg_cubby_t),
	                wlc_ap_tpc_bsscfg_init, wlc_ap_tpc_bsscfg_deinit, wlc_ap_tpc_bsscfg_dump,
	                (void *)tpc)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif /* WL_AP_TPC */

#ifdef BCMDBG
	if (wlc_dump_register(wlc->pub, "tpc", wlc_tpc_dump, tpc) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif

	if (wlc_module_register(wlc->pub, wlc_tpc_iovars, "tpc", tpc, wlc_tpc_doiovar,
	                        wlc_tpc_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	if (wlc_module_add_ioctl_fn(wlc->pub, tpc, wlc_tpc_doioctl,
	                            ARRAYSIZE(wlc_tpc_ioctls), wlc_tpc_ioctls) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	return tpc;

	/* error handling */
fail:
	wlc_tpc_detach(tpc);
	return NULL;
}

void
BCMATTACHFN(wlc_tpc_detach)(wlc_tpc_info_t *tpc)
{
	wlc_info_t *wlc;

	if (tpc == NULL)
		return;

	wlc = tpc->wlc;

	wlc_module_remove_ioctl_fn(wlc->pub, tpc);
	wlc_module_unregister(wlc->pub, "tpc", tpc);

	MFREE(wlc->osh, tpc, sizeof(wlc_tpc_info_t));
}

static int
wlc_tpc_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;
	wlc_info_t *wlc = tpc->wlc;
	int err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	bool bool_val2;
	wlc_bsscfg_t *bsscfg;

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
	BCM_REFERENCE(bool_val);
	BCM_REFERENCE(bool_val2);

	/* Do the actual parameter implementation */
	switch (actionid) {
#if defined(WLTEST)
	case IOV_GVAL(IOV_TPC_RPT_OVERRIDE):
		*ret_int_ptr = (int32)tpc->tpc_rpt_override;
		break;
	case IOV_SVAL(IOV_TPC_RPT_OVERRIDE):
		tpc->tpc_rpt_override = (uint16)int_val;
		break;
#endif 
#ifdef WL_AP_TPC
	case IOV_GVAL(IOV_AP_TPC_MODE):
		*ret_int_ptr = tpc->ap_tpc;
		break;
	case IOV_SVAL(IOV_AP_TPC_MODE):
		err = wlc_ap_tpc_setup(tpc, (uint8)int_val);
		break;
	case IOV_GVAL(IOV_AP_TPC_PERIOD):
		*ret_int_ptr = tpc->ap_tpc_interval;
		break;
	case IOV_SVAL(IOV_AP_TPC_PERIOD):
		if (!WL11H_ENAB(wlc)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		tpc->ap_tpc_interval = (uint8)int_val;
		break;
	case IOV_GVAL(IOV_AP_TPC_LM):
		if (!tpc->ap_tpc) {
			err = BCME_EPERM;
			break;
		}

		*ret_int_ptr = wlc_ap_bss_tpc_get(tpc, bsscfg);
		break;
#endif /* WL_AP_TPC */
	case IOV_SVAL(IOV_CONSTRAINT):
		tpc->pwr_constraint = (uint8)int_val;
		if (wlc->pub->associated) {
			WL_APSTA_BCN(("wl%d: WLC_SEND_PWR_CONSTRAINT ->"
			              " wlc_update_beacon()\n", wlc->pub->unit));
			wlc_update_beacon(wlc);
			wlc_update_probe_resp(wlc, TRUE);
		}
		break;
#if defined(WL_EXPORT_CURPOWER)
	case IOV_GVAL(IOV_CURPOWER):
		err = wlc_tpc_get_current(tpc, arg, len, bsscfg);
		break;
#endif
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_tpc_doioctl(void *ctx, int cmd, void *arg, int len, struct wlc_if *wlcif)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;
	wlc_info_t *wlc = tpc->wlc;
	int err = BCME_OK;

	switch (cmd) {
	case WLC_SEND_PWR_CONSTRAINT:
		err = wlc_iovar_op(wlc, "constraint", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_tpc_watchdog(void *ctx)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;
	wlc_info_t *wlc = tpc->wlc;

	(void)wlc;

#ifdef WL_AP_TPC
	if (tpc->ap_tpc_interval > 0 &&
	    (wlc->pub->now % tpc->ap_tpc_interval) == 0)
		wlc_ap_tpc_req(tpc);
#endif

	return BCME_OK;
}

#ifdef BCMDBG
static int
wlc_tpc_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;

#if defined(WLTEST)
	bcm_bprintf(b, "tpc_rpt_override: %d\n", tpc->tpc_rpt_override);
#endif

#ifdef WL_AP_TPC
	bcm_bprintf(b, "ap_tpc: %d ap_tpc_interval: %d\n", tpc->ap_tpc, tpc->ap_tpc_interval);
#endif

	bcm_bprintf(b, "pwr_constraint %u txpwr_local_max %d txpwr_local_constraint %u\n",
	            tpc->pwr_constraint, tpc->txpwr_local_max, tpc->txpwr_local_constraint);

	return BCME_OK;
}
#endif /* BCMDBG */

#ifdef WL_AP_TPC
/* scb cubby */
static int
wlc_ap_tpc_scb_init(void *ctx, struct scb *scb)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;

	wlc_ap_tpc_assoc_reset(tpc, scb);

	return BCME_OK;
}

static void
wlc_ap_tpc_scb_deinit(void *ctx, struct scb *scb)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;

	wlc_ap_tpc_assoc_reset(tpc, scb);
}

#ifdef BCMDBG
static void
wlc_ap_tpc_scb_dump(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;
	ap_tpc_scb_cubby_t *tpc_scb = AP_TPC_SCB_CUBBY(tpc, scb);

	ASSERT(tpc_scb != NULL);

	bcm_bprintf(b, "     ap_link_margin: %d sta_link_margin: %d\n",
	            tpc_scb->ap_link_margin, tpc_scb->sta_link_margin);
}
#endif

/* bsscfg cubby */
static int
wlc_ap_tpc_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;

	(void)tpc;

	return BCME_OK;
}

static void
wlc_ap_tpc_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;

	(void)tpc;
}

#ifdef BCMDBG
static void
wlc_ap_tpc_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_tpc_info_t *tpc = (wlc_tpc_info_t *)ctx;
	ap_tpc_bsscfg_cubby_t *tpc_cfg = AP_TPC_BSSCFG_CUBBY(tpc, cfg);

	ASSERT(tpc_cfg != NULL);

	bcm_bprintf(b, "ap_link_margin: %d sta_link_margin: %d\n",
	            tpc_cfg->ap_link_margin, tpc_cfg->sta_link_margin);
}
#endif

void
wlc_ap_tpc_assoc_reset(wlc_tpc_info_t *tpc, struct scb *scb)
{
	ap_tpc_scb_cubby_t *tpc_scb = AP_TPC_SCB_CUBBY(tpc, scb);

	ASSERT(tpc_scb != NULL);

	/* set to max */
	tpc_scb->sta_link_margin = AP_TPC_MAX_LINK_MARGIN;
	tpc_scb->ap_link_margin = AP_TPC_MAX_LINK_MARGIN;
}

static int
wlc_ap_bss_tpc_get(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg)
{
	ap_tpc_bsscfg_cubby_t *tpc_cfg = AP_TPC_BSSCFG_CUBBY(tpc, cfg);
	int tpc_val;

	ASSERT(tpc_cfg != NULL);

	tpc_val = (((uint16)tpc_cfg->ap_link_margin << 8) & 0xff00) |
		(tpc_cfg->sta_link_margin & 0x00ff);

	return tpc_val;
}

void
wlc_ap_bss_tpc_setup(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = tpc->wlc;
	ap_tpc_bsscfg_cubby_t *tpc_cfg = AP_TPC_BSSCFG_CUBBY(tpc, cfg);

	ASSERT(tpc_cfg != NULL);

	/* reset BSS pwr and AP pwr before enabling new mode. */
	tpc_cfg->sta_link_margin = AP_TPC_MAX_LINK_MARGIN;
	tpc_cfg->ap_link_margin = AP_TPC_MAX_LINK_MARGIN;

	if (BSSCFG_AP(cfg) && cfg->up) {
		wlc_bss_update_beacon(wlc, cfg);
		wlc_bss_update_probe_resp(wlc, cfg, TRUE);
	}
}

static int
wlc_ap_tpc_setup(wlc_tpc_info_t *tpc, uint8 mode)
{
	wlc_info_t *wlc = tpc->wlc;
	int idx;
	wlc_bsscfg_t *cfg;

	if (!WL11H_ENAB(wlc))
		return BCME_UNSUPPORTED;

	if (mode > 3)
		return BCME_RANGE;

	if (!tpc->ap_tpc_interval)
		tpc->ap_tpc_interval = 3;

	tpc->ap_tpc = mode;

	tpc->pwr_constraint = 0;

	FOREACH_BSS(wlc, idx, cfg) {
		wlc_ap_bss_tpc_setup(tpc, cfg);
	}

	tpc->txpwr_local_constraint = 0;
	if (wlc->pub->up && wlc->pub->associated &&
	    (wlc->chanspec == wlc->home_chanspec)) {
		uint8 qdbm = wlc_tpc_get_local_constraint_qdbm(tpc);
		wlc_channel_set_txpower_limit(wlc->cmi, qdbm);
	}

	return BCME_OK;
}

static void
wlc_ap_bss_tpc_req(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = tpc->wlc;
	struct scb_iter scbiter;
	struct scb *scb;

	if (BSSCFG_AP(cfg) && cfg->up) {

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			if (SCB_ASSOCIATED(scb) &&
			    (scb->cap & DOT11_CAP_SPECTRUM)) {
				/* send TPC request to all assoc STAs.
				 */
				if (!ETHER_ISMULTI(&scb->ea))
					wlc_send_tpc_request(tpc, cfg, &scb->ea);

				/* reset the margins when channel switch is
				 * pending.
				 */
				if (wlc_11h_get_spect_state(wlc->m11h, cfg) &
				    NEED_TO_SWITCH_CHANNEL) {
					ap_tpc_scb_cubby_t *tpc_scb = AP_TPC_SCB_CUBBY(tpc, scb);

					tpc_scb->sta_link_margin = AP_TPC_MAX_LINK_MARGIN;
					tpc_scb->ap_link_margin = AP_TPC_MAX_LINK_MARGIN;
				}
			}
		}
	}
}

static void
wlc_ap_tpc_req(wlc_tpc_info_t *tpc)
{
	wlc_info_t *wlc = tpc->wlc;
	int idx;
	wlc_bsscfg_t *cfg;

	if (WL11H_ENAB(wlc) && tpc->ap_tpc) {

		FOREACH_BSS(wlc, idx, cfg) {
			wlc_ap_bss_tpc_req(tpc, cfg);
		}
	}
}

static void
wlc_ap_bss_tpc_upd(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = tpc->wlc;
	struct scb_iter scbiter;
	struct scb *scb;
	int8 sta_least_link_margin, ap_least_link_margin;

	if (BSSCFG_AP(cfg) && cfg->up) {
		ap_tpc_bsscfg_cubby_t *tpc_cfg = AP_TPC_BSSCFG_CUBBY(tpc, cfg);

		ASSERT(tpc_cfg != NULL);

		sta_least_link_margin = AP_TPC_MAX_LINK_MARGIN;
		ap_least_link_margin = AP_TPC_MAX_LINK_MARGIN;

		FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
			if (SCB_ASSOCIATED(scb) &&
			    (scb->cap & DOT11_CAP_SPECTRUM)) {
				ap_tpc_scb_cubby_t *tpc_scb = AP_TPC_SCB_CUBBY(tpc, scb);

				/* now record the least link margin
				 * from previous reports.
				 */
				if (sta_least_link_margin >= tpc_scb->sta_link_margin)
					sta_least_link_margin = tpc_scb->sta_link_margin;

				/* find the least link margin AP has
				 * with respect to all the associated
				 * STAs.
				 */
				if (ap_least_link_margin >= tpc_scb->ap_link_margin)
					ap_least_link_margin = tpc_scb->ap_link_margin;
			}
		}

		/* record the link margin info for record keeping. */
		tpc_cfg->sta_link_margin = sta_least_link_margin;
		tpc_cfg->ap_link_margin = ap_least_link_margin;
	}
}

static void
wlc_ap_tpc_upd(wlc_tpc_info_t *tpc)
{
	wlc_info_t *wlc = tpc->wlc;
	int idx;
	wlc_bsscfg_t *cfg;
	int8 sta_least_link_margin, ap_least_link_margin;

	if (WL11H_ENAB(wlc) && tpc->ap_tpc) {

		sta_least_link_margin = AP_TPC_MAX_LINK_MARGIN;
		ap_least_link_margin = AP_TPC_MAX_LINK_MARGIN;

		FOREACH_BSS(wlc, idx, cfg) {
			ap_tpc_bsscfg_cubby_t *tpc_cfg = AP_TPC_BSSCFG_CUBBY(tpc, cfg);

			ASSERT(tpc_cfg != NULL);

			wlc_ap_bss_tpc_upd(tpc, cfg);

			/* find the least link margin */
			if (sta_least_link_margin >= tpc_cfg->sta_link_margin)
				sta_least_link_margin = tpc_cfg->sta_link_margin;

			/* find the least link margin AP has
			 * with respect to all the associated
			 * STAs.
			 */
			if (ap_least_link_margin >= tpc_cfg->ap_link_margin)
				ap_least_link_margin = tpc_cfg->ap_link_margin;
		}

		/* reduce the AP power if stas have better link
		 * margin.
		 */
		if (tpc->ap_tpc == AP_TPC_AP_PWR || tpc->ap_tpc == AP_TPC_AP_BSS_PWR) {
			uint8 txpwr_local_constraint;

			/* now update the bcn and probe responses with new pwr
			 * constriant.
			 */
			if (sta_least_link_margin == AP_TPC_MAX_LINK_MARGIN) {
				txpwr_local_constraint = 0;
			} else if (sta_least_link_margin >= 9) {
				txpwr_local_constraint = 6;
			} else if (sta_least_link_margin >= 6) {
				txpwr_local_constraint = 3;
			} else {
				txpwr_local_constraint = 0;
			}

			WL_REGULATORY(("wl%d:%s STAs least link margin:%d "
				"txpwr_local_constraint:%d \n", wlc->pub->unit, __FUNCTION__,
				sta_least_link_margin, txpwr_local_constraint));

			if (txpwr_local_constraint != tpc->txpwr_local_constraint) {
				tpc->txpwr_local_constraint = txpwr_local_constraint;

				/* only update power targets if we are up and on the BSS
				 * home channel.
				 */
				if (wlc->chanspec == wlc->home_chanspec) {
					uint8 qdbm = wlc_tpc_get_local_constraint_qdbm(tpc);
					wlc_channel_set_txpower_limit(wlc->cmi, qdbm);
				}
			}
		}

		/* reduce the BS pwr based on how best link margin AP
		 * has.
		 */
		if (tpc->ap_tpc == AP_TPC_BSS_PWR || tpc->ap_tpc == AP_TPC_AP_BSS_PWR) {
			uint8 pwr_constraint;

			if ((ap_least_link_margin == AP_TPC_MAX_LINK_MARGIN))
				pwr_constraint = 0;
			else if (ap_least_link_margin >= 9)
				pwr_constraint = 6;
			else if (ap_least_link_margin >= 6)
				pwr_constraint = 3;
			else
				pwr_constraint = 0;

			WL_REGULATORY(("wl%d:%s APs least link margin:%d pwr_constraint:%d\n",
				wlc->pub->unit, __FUNCTION__,
				ap_least_link_margin, pwr_constraint));

			if (pwr_constraint != tpc->pwr_constraint) {
				tpc->pwr_constraint = pwr_constraint;

				wlc_update_beacon(wlc);
				wlc_update_probe_resp(wlc, TRUE);
			}
		}
	}
}

static void
wlc_ap_tpc_rpt_upd(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg, struct dot11_management_header *hdr,
	dot11_tpc_rep_t *ie, int8 rssi, ratespec_t rspec)
{
	wlc_info_t *wlc = tpc->wlc;
	struct scb *scb;
	int nominal_req_pwr;
	uint8 reg_chan_pwr, cur_chan, txpwr_max;
	uint8 target_pwr;
	ap_tpc_scb_cubby_t *tpc_scb;

	nominal_req_pwr = wlc_find_nominal_req_pwr(rspec);

	cur_chan = CHSPEC_CHANNEL(wlc->home_chanspec);

	reg_chan_pwr = wlc_get_reg_max_power_for_channel(wlc->cmi, cur_chan, TRUE);

	target_pwr = wlc_phy_txpower_get_target_max((wlc_phy_t *)wlc->band->pi);

	txpwr_max = (target_pwr + wlc->band->antgain) / WLC_TXPWR_DB_FACTOR;

	WL_REGULATORY(("wl%d: %s: Nominal req pwr: %d RSSI of packet:%d current channel:%d "
		"regulatory pwr for channel:%d max tx pwr:%d\n",
		wlc->pub->unit, __FUNCTION__,
		nominal_req_pwr, rssi, cur_chan, reg_chan_pwr, txpwr_max));

	/* Record the STA link margin now */
	if ((scb = wlc_scbfind(wlc, cfg, &hdr->sa)) == NULL) {
		WL_INFORM(("did not find scb\n"));
		return;
	}

	tpc_scb = AP_TPC_SCB_CUBBY(tpc, scb);
	ASSERT(tpc_scb != NULL);

	/* record sta's link margin */
	tpc_scb->sta_link_margin = (int8)ie->margin + (reg_chan_pwr - txpwr_max);

	/* record ap's link margin */
	tpc_scb->ap_link_margin = rssi - nominal_req_pwr + (reg_chan_pwr - ie->tx_pwr);

	WL_REGULATORY(("wl%d:%s STAs link margin:%d APs link margin:%d\n", wlc->pub->unit,
	               __FUNCTION__, tpc_scb->sta_link_margin, tpc_scb->ap_link_margin));

	wlc_ap_tpc_upd(tpc);
}
#endif /* WL_AP_TPC */

#ifndef WLC_NET80211
#if defined(WLTEST)
static void
wlc_tpc_rpt_ovrd(wlc_tpc_info_t *tpc, dot11_tpc_rep_t *rpt)
{
	if (tpc->tpc_rpt_override != 0) {
		rpt->tx_pwr = (int8)((tpc->tpc_rpt_override >> 8) & 0xff);
		rpt->margin = (int8)(tpc->tpc_rpt_override & 0xff);
	}
}
#endif 

void
wlc_recv_tpc_request(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg, struct dot11_management_header *hdr,
	uint8 *body, int body_len, int8 rssi, ratespec_t rspec)
{
	wlc_info_t *wlc = tpc->wlc;
	struct dot11_action_measure * action_hdr;
	struct ether_addr *ea = &hdr->sa;
#if defined(BCMDBG_ERR) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	(void)wlc;

	if (body_len < 3) {
		WL_ERROR(("wl%d: %s: got TPC Request from %s, but frame body len"
			" was %d, expected 3\n",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(ea, eabuf), body_len));
		return;
	}

	action_hdr = (struct dot11_action_measure *)body;

	WL_INFORM(("wl%d: wlc_recv_tpc_request: got TPC Request (token %d) from %s\n",
	           wlc->pub->unit, action_hdr->token, bcm_ether_ntoa(ea, eabuf)));

	wlc_send_tpc_report(tpc, cfg, ea, action_hdr->token, rssi, rspec);
}

void
wlc_recv_tpc_report(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg, struct dot11_management_header *hdr,
	uint8 *body, int body_len, int8 rssi, ratespec_t rspec)
{
	wlc_info_t *wlc = tpc->wlc;
	struct dot11_action_measure * action_hdr;
	int len;
	dot11_tpc_rep_t* rep_ie;
#ifdef BCMDBG
	char da[ETHER_ADDR_STR_LEN];
	char sa[ETHER_ADDR_STR_LEN];
	char bssid[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	(void)wlc;

	WL_REGULATORY(("Action Frame: DA %s SA %s BSSID %s\n",
	       bcm_ether_ntoa(&hdr->da, da), bcm_ether_ntoa(&hdr->sa, sa),
	       bcm_ether_ntoa(&hdr->bssid, bssid)));

	if (body_len < 3) {
		WL_INFORM(("Action frame body len was %d, expected > 3\n", body_len));
		return;
	}

	/* re-using action measure struct here also */
	action_hdr = (struct dot11_action_measure *)body;
	rep_ie = (dot11_tpc_rep_t*)action_hdr->data;
	len = body_len - DOT11_ACTION_MEASURE_LEN;

	WL_REGULATORY(("Action Frame: category %d action %d dialog token %d\n",
	       action_hdr->category, action_hdr->action, action_hdr->token));

	if (action_hdr->category != DOT11_ACTION_CAT_SPECT_MNG) {
		WL_INFORM(("Unexpected category, expected Spectrum Management %d\n",
			DOT11_ACTION_CAT_SPECT_MNG));
		return;
	}

	if (action_hdr->action != DOT11_SM_ACTION_TPC_REP) {
		WL_INFORM(("Unexpected action type (%d)\n", action_hdr->action));
		return;
	}

	if (len < 4) {
		WL_INFORM(("Malformed Action frame, less that an IE header length (4 bytes)"
			" remaining in buffer\n"));
		return;
	}

	if (rep_ie->id != DOT11_MNG_TPC_REPORT_ID) {
		WL_INFORM(("Unexpected IE (id %d len %d):\n", rep_ie->id, rep_ie->len));
		prhex(NULL, (uint8*)rep_ie + TLV_HDR_LEN, rep_ie->len);
		return;
	}

	if (rep_ie->len != 2) {
		WL_INFORM(("Unexpected TPC report IE len != 2\n"));
		return;
	}

	WL_REGULATORY(("%s (id %d len %d): tx_pwr:%d margin:%d\n", "TPC Report", rep_ie->id,
		rep_ie->len, (int8)rep_ie->tx_pwr, (int8)rep_ie->margin));

	wlc_ap_tpc_rpt_upd(wlc->tpc, cfg, hdr, rep_ie, rssi, rspec);
}

void
wlc_send_tpc_request(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg, struct ether_addr *da)
{
	wlc_info_t *wlc = tpc->wlc;
	void *p;
	uint8* pbody;
	uint8* end;
	uint body_len;
	struct dot11_action_measure * action_hdr;
	struct scb *scb = NULL;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */

	WL_INFORM(("wl%d: %s: sending TPC Request to %s\n",
	           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(da, eabuf)));

	ASSERT(cfg != NULL);

	/* TPC Request frame is
	 * 3 bytes Action Measure Req frame
	 * 2 bytes empty TPC Request IE
	 */
	body_len = DOT11_ACTION_MEASURE_LEN + TLV_HDR_LEN;

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, da, &cfg->cur_etheraddr, &cfg->BSSID,
	                       body_len, &pbody);
	if (p == NULL) {
		WL_INFORM(("wl%d: %s: no memory for TPC Request\n",
		           wlc->pub->unit, __FUNCTION__));
		return;
	}

	action_hdr = (struct dot11_action_measure *)pbody;
	action_hdr->category = DOT11_ACTION_CAT_SPECT_MNG;
	action_hdr->action = DOT11_SM_ACTION_TPC_REQ;
	/* Token needs to be non-zero, so burn the high bit */
	action_hdr->token = (uint8)(wlc->counter | 0x80);
	end = wlc_write_info_elt(action_hdr->data, DOT11_MNG_TPC_REQUEST_ID, 0, NULL);

	ASSERT((end - pbody) == (int)body_len);
	BCM_REFERENCE(end);

	if (!ETHER_ISMULTI(da)) {
		scb = wlc_scbfindband(wlc, cfg, da, CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec));
	}

	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, scb);
}

void
wlc_send_tpc_report(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg, struct ether_addr *da,
	uint8 token, int8 rssi, ratespec_t rspec)
{
	wlc_info_t *wlc = tpc->wlc;
	void *p;
	uint8* pbody;
	uint body_len;
	struct dot11_action_measure * action_hdr;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */

	WL_INFORM(("wl%d: %s: sending TPC Report to %s\n",
	           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(da, eabuf)));

	ASSERT(cfg != NULL);

	/* TPC Report frame is
	 * 3 bytes Action Measure Req frame
	 * 4 bytes TPC Report IE
	 */
	body_len = DOT11_ACTION_MEASURE_LEN + TLV_HDR_LEN + DOT11_MNG_IE_TPC_REPORT_LEN;

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, da, &cfg->cur_etheraddr, &cfg->BSSID,
	                       body_len, &pbody);
	if (p == NULL) {
		WL_INFORM(("wl%d: %s: no memory for TPC Report\n",
		           wlc->pub->unit, __FUNCTION__));
		return;
	}

	action_hdr = (struct dot11_action_measure *)pbody;
	action_hdr->category = DOT11_ACTION_CAT_SPECT_MNG;
	action_hdr->action = DOT11_SM_ACTION_TPC_REP;
	action_hdr->token = token;

	wlc_tpc_rep_build(wlc, rssi, rspec, (dot11_tpc_rep_t *)&action_hdr->data[0]);

	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, NULL);
}

void
wlc_tpc_rep_build(wlc_info_t *wlc, int8 rssi, ratespec_t rspec, dot11_tpc_rep_t *tpc_rep)
{
	int txpwr, link_margin;
	int nominal_req_pwr;
	uint8 target_pwr;

	target_pwr = wlc_phy_txpower_get_target_max((wlc_phy_t *)wlc->band->pi);

	/* tx power for the outgoing frame will be our current txpwr setting
	 * include the antenna gain value to get radiated power, EIRP.
	 * Adjust from internal units to dbm.
	 */
	txpwr = (target_pwr + wlc->band->antgain) / WLC_TXPWR_DB_FACTOR;

	nominal_req_pwr = wlc_find_nominal_req_pwr(rspec);

	link_margin = rssi;
	link_margin -= nominal_req_pwr;
	link_margin -= 3; /* TPC Report Safety Margin, 3 dB */
	/* clamp link_margin value if we overflow an int8 */
	link_margin = MIN(link_margin, 127);
	link_margin = MAX(link_margin, -128);

	tpc_rep->id = DOT11_MNG_TPC_REPORT_ID;
	tpc_rep->len = DOT11_MNG_IE_TPC_REPORT_LEN;
	tpc_rep->tx_pwr = (int8)txpwr;
	tpc_rep->margin = (int8)link_margin;

	wlc_tpc_rpt_ovrd(wlc->tpc, tpc_rep);

	WL_INFORM(("wl%d: wlc_build_tpc_report: TPC Report: txpwr %d, link margin %d\n",
		wlc->pub->unit, txpwr, link_margin));
}
#endif /* WLC_NET80211 */

void
wlc_tpc_report(wlc_tpc_info_t *tpc, dot11_tpc_rep_t *ie)
{
#ifdef BCMDBG
	wlc_info_t *wlc = tpc->wlc;

	(void)wlc;

	if (ie->len < 2) {
		WL_ERROR(("wl%d: wlc_tpc_report: TPC Report IE len %d too short, expected 2.\n",
			wlc->pub->unit, ie->len));
		return;
	}

	WL_NONE(("wl%d: wlc_tpc_report: TPC Report TX Pwr %d dBm, Link Margin %d dB.\n",
		wlc->pub->unit, (int)ie->tx_pwr, (int)ie->margin));
#endif /* BCMDBG */
}

/* STA: Handle Power Constraint IE */
void
wlc_tpc_set_local_constraint(wlc_tpc_info_t *tpc, dot11_power_cnst_t *pwr)
{
	wlc_info_t *wlc = tpc->wlc;
	uint8 local;

	if (pwr->len < 1)
		return;

	local = pwr->power;

	if (local == tpc->txpwr_local_constraint)
		return;

	WL_REGULATORY(("wl%d: adjusting local power constraint from %d to %d dBm\n",
		wlc->pub->unit, tpc->txpwr_local_constraint, local));

	tpc->txpwr_local_constraint = local;

	/* only update power targets if we are up and on the BSS home channel */
	if (wlc->pub->up && wlc->pub->associated &&
	    (wlc->chanspec == wlc->home_chanspec)) {
		uint8 constraint;

		/* Set the power limits for this locale after computing
		 * any 11h local tx power constraints.
		 */
		constraint = wlc_tpc_get_local_constraint_qdbm(tpc);
		wlc_channel_set_txpower_limit(wlc->cmi, constraint);

		/* wlc_phy_cal_txpower_recalc_sw(wlc->band->pi); */
	}
}

void
wlc_tpc_reset_all(wlc_tpc_info_t *tpc)
{
	wlc_info_t *wlc = tpc->wlc;

	(void)wlc;

	/* reset BSS local power limits */
	tpc->txpwr_local_max = WLC_TXPWR_MAX;

	/* Need to set the txpwr_local_max to external reg max for
	 * this channel as per the locale selected for AP.
	 */
#ifdef AP
	if (AP_ONLY(wlc->pub)) {
		uint8 chan =  CHSPEC_CHANNEL(wlc->chanspec);
		tpc->txpwr_local_max =
		        wlc_get_reg_max_power_for_channel(wlc->cmi, chan, TRUE);
	}
#endif

	tpc->txpwr_local_constraint = 0;
}

uint8
wlc_tpc_get_local_constraint_qdbm(wlc_tpc_info_t *tpc)
{
	wlc_info_t *wlc = tpc->wlc;
	uint8 local;
	int16 local_max;

	local = WLC_TXPWR_MAX;

	if ((tpc->txpwr_local_max != WL_RATE_DISABLED) && wlc->pub->associated &&
		(wf_chspec_ctlchan(wlc->chanspec) == wf_chspec_ctlchan(wlc->home_chanspec))) {

		/* get the local power constraint if we are on the AP's
		 * channel [802.11h, 7.3.2.13]
		 */
		/* Clamp the value between 0 and WLC_TXPWR_MAX w/o overflowing the target */
		local_max = tpc->txpwr_local_max * WLC_TXPWR_DB_FACTOR;
		local_max -= tpc->txpwr_local_constraint * WLC_TXPWR_DB_FACTOR;
		if (local_max > 0 && local_max < WLC_TXPWR_MAX)
			return (uint8)local_max;
		if (local_max < 0)
			return 0;
	}

	return local;
}

#if defined(WL_EXPORT_CURPOWER) && defined(PPR_API)
static int
wlc_tpc_get_current(wlc_tpc_info_t *tpc, void *pwr, uint len, wlc_bsscfg_t *bsscfg)
{
	wlc_info_t *wlc = tpc->wlc;
	phy_tx_power_t *power;
	tx_power_legacy_t *old_power    = NULL;
	uint serlen;
	bool override;
	wl_tx_bw_t ppr_bw;
	ppr_t* reg_limits;
	uint8 *pserbuf;
	tx_pwr_rpt_t *pwr_to_wl = (tx_pwr_rpt_t*)pwr;

	clm_power_limits_t limits;		/* 20, 40 or 80 MHz limits */
	clm_power_limits_t limits_subchan1;	/* 20in40 or 40in80MHz limits */
#ifdef WL11AC
	clm_power_limits_t limits_subchan2;	/* 20in80MHz limits */
#endif
	clm_limits_type_t sband;
	chanspec_t chanspec = wlc->chanspec;
	/* convert chanspec into CLM units */
	clm_band_t bandtype = CHSPEC_IS5G(chanspec) ? CLM_BAND_5G : CLM_BAND_2G;
	clm_bandwidth_t bw = CLM_BW_20;
	unsigned int chan;
	clm_country_t country;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale;
	wlcband_t *band;
	int ant_gain;
	clm_limits_params_t lim_params;
	uint16 spatial_map;
	int err = 0;

	ASSERT(WL_BW_20MHZ == CLM_BW_20);
	ASSERT(WL_BW_40MHZ == CLM_BW_40);
#ifdef WL11AC
	ASSERT(WL_BW_80MHZ == CLM_BW_80);
	ASSERT(WL_BW_160MHZ == CLM_BW_160);
	ASSERT(CLM_BW_NUM == 4);
#else
	ASSERT(CLM_BW_NUM == 2);
#endif
	if (CHSPEC_IS40(chanspec))
		bw = CLM_BW_40;
#ifdef WL11AC
	else if (CHSPEC_IS80(chanspec))
		bw = CLM_BW_80;
#endif

	ASSERT(!len || pwr != NULL);
	if (len == sizeof(tx_power_legacy_t)) {
		old_power = (tx_power_legacy_t*)pwr;
		pwr_to_wl = (tx_pwr_rpt_t*)MALLOC(wlc->osh, sizeof(*pwr_to_wl));
		if (!pwr_to_wl)
			return BCME_NOMEM;
	} else if (len < sizeof(*pwr_to_wl) + pwr_to_wl->board_limit_len + pwr_to_wl->target_len) {
		return BCME_BUFTOOSHORT;
	}

	if (!(power = (phy_tx_power_t*)MALLOC(wlc->osh, sizeof(phy_tx_power_t)))) {
		err = BCME_NOMEM;
		goto free_pwr_to_wl;
	}

	bzero(power, sizeof(phy_tx_power_t));
	pserbuf = pwr_to_wl->pprdata;

	ppr_bw = ppr_get_max_bw();
	if ((power->ppr_target_powers = ppr_create(wlc->osh, ppr_bw)) == NULL) {
		err = BCME_NOMEM;
		goto free_power;
	}
	if ((power->ppr_board_limits = ppr_create(wlc->osh, ppr_bw)) == NULL) {
		err = BCME_NOMEM;
		goto free_power;
	}

	pwr_to_wl->version = TX_POWER_T_VERSION;

	/* Tell wlu.c if we're using 20MHz or 40MHz. */
	/* This only works if we only support 20MHz and 40MHz bandwidths. */
	pwr_to_wl->channel_bandwidth = bw;

	power->chanspec = WLC_BAND_PI_RADIO_CHANSPEC;
	if (wlc->pub->associated)
		power->local_chanspec = wlc->home_chanspec;

	/* Return the user target tx power limits for the various rates. */
	wlc_phy_txpower_get(wlc->band->pi, &pwr_to_wl->user_target, &override);

	pwr_to_wl->local_max = tpc->txpwr_local_max * WLC_TXPWR_DB_FACTOR;
	pwr_to_wl->local_constraint = tpc->txpwr_local_constraint * WLC_TXPWR_DB_FACTOR;

	pwr_to_wl->antgain[0] = wlc->bandstate[BAND_2G_INDEX]->antgain;
	pwr_to_wl->antgain[1] = wlc->bandstate[BAND_5G_INDEX]->antgain;

	memset(&limits, (unsigned char)WL_RATE_DISABLED, sizeof(clm_power_limits_t));
	memset(&limits_subchan1, (unsigned char)WL_RATE_DISABLED, sizeof(clm_power_limits_t));
#ifdef WL11AC
	memset(&limits_subchan2, (unsigned char)WL_RATE_DISABLED, sizeof(clm_power_limits_t));
#endif

	country = wlc_get_country(wlc);
	chan = CHSPEC_CHANNEL(chanspec);
	band = wlc->bandstate[CHSPEC_WLCBANDUNIT(chanspec)];
	ant_gain = band->antgain;

	result = wlc_get_locale(country, &locale);
	if (result == CLM_RESULT_OK)
		result = clm_limits_params_init(&lim_params);

	lim_params.sar = band->sar;
	lim_params.bw = bw;

	if (result != CLM_RESULT_OK) {
		WL_ERROR(("wlc_tpc_get_current: wlc_get_locale failed\n"));
		err = BCME_ERROR;
		goto free_power;
	}

	pwr_to_wl->sar = (int8)lim_params.sar;

	result = clm_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
		&lim_params, &limits);

	if (result != CLM_RESULT_OK) {
		WL_ERROR(("wlc_tpc_get_current: clm_limits failed\n"));
		memset(&limits, (unsigned char)WL_RATE_DISABLED, sizeof(clm_power_limits_t));
	}

#ifdef WL11AC
	if ((result == CLM_RESULT_OK) && (bw == CLM_BW_40 || bw == CLM_BW_80)) {
		if (bw == CLM_BW_40) {
			sband = CHSPEC_SB_UPPER(chanspec) ?
				CLM_LIMITS_TYPE_SUBCHAN_U : CLM_LIMITS_TYPE_SUBCHAN_L;
		} else {
			sband = CLM_LIMITS_TYPE_SUBCHAN_L;
			if ((CHSPEC_CTL_SB(chanspec) == WL_CHANSPEC_CTL_SB_UU) ||
				(CHSPEC_CTL_SB(chanspec) == WL_CHANSPEC_CTL_SB_UL))
			/* Primary 40MHz is on upper side */
				sband = CLM_LIMITS_TYPE_SUBCHAN_U;
		}
#else
	if ((result == CLM_RESULT_OK) && (bw == CLM_BW_40)) {
		sband = CHSPEC_SB_UPPER(chanspec) ?
			CLM_LIMITS_TYPE_SUBCHAN_U : CLM_LIMITS_TYPE_SUBCHAN_L;
#endif /* WL11AC */
		result = clm_limits(&locale, bandtype, chan, ant_gain, sband,
			&lim_params, &limits_subchan1);

		if (result != CLM_RESULT_OK) {
			WL_ERROR(("wlc_tpc_get_current: clm_limits failed\n"));
			memset(&limits_subchan1, (unsigned char)WL_RATE_DISABLED,
				sizeof(clm_power_limits_t));
		}
	}
#ifdef WL11AC
	if ((result == CLM_RESULT_OK) && (bw == CLM_BW_80)) {

		switch CHSPEC_CTL_SB(chanspec) {
		case WL_CHANSPEC_CTL_SB_LL:
			sband = CLM_LIMITS_TYPE_SUBCHAN_LL;
			break;
		case WL_CHANSPEC_CTL_SB_LU:
			sband = CLM_LIMITS_TYPE_SUBCHAN_LU;
			break;
		case WL_CHANSPEC_CTL_SB_UL:
			sband = CLM_LIMITS_TYPE_SUBCHAN_UL;
			break;
		case WL_CHANSPEC_CTL_SB_UU:
		default:
			sband = CLM_LIMITS_TYPE_SUBCHAN_UU;
			break;
		}

		result = clm_limits(&locale, bandtype, chan, ant_gain, sband,
			&lim_params, &limits_subchan2);

		if (result != CLM_RESULT_OK) {
			WL_ERROR(("wlc_tpc_get_current: 20in80 clm_limits failed\n"));
			memset(&limits_subchan2, (unsigned char)WL_RATE_DISABLED,
				sizeof(clm_power_limits_t));
		}
	}
#endif /* WL11AC */

	memcpy(&pwr_to_wl->clm_limits, &limits, WL_NUMRATES);
	memcpy(&pwr_to_wl->clm_limits_subchan1, &limits_subchan1, WL_NUMRATES);
#ifdef WL11AC
	memcpy(&pwr_to_wl->clm_limits_subchan2, &limits_subchan2, WL_NUMRATES);
#endif

	if (PHYTYPE_HT_CAP(wlc->band)) {
		power->flags |= WL_TX_POWER_F_HT;
	}

	if ((reg_limits = ppr_create(wlc->osh, PPR_CHSPEC_BW(power->chanspec))) == NULL) {
		err = BCME_ERROR;
		goto free_power;
	}

	wlc_channel_reg_limits(wlc->cmi, power->chanspec, reg_limits);

	wlc_phy_txpower_get_current(wlc->band->pi, reg_limits, power);
	ppr_delete(wlc->osh, reg_limits);


	/* copy the tx_ppr_t struct to the return buffer,
	 * or convert to a tx_power_legacy_t struct
	 */
	if (old_power) {
		uint r;
		bzero(old_power, sizeof(tx_power_legacy_t));

		old_power->txpwr_local_max = pwr_to_wl->local_max;

		for (r = 0; r < NUM_PWRCTRL_RATES; r++) {
			old_power->txpwr_band_max[r] = (uint8)pwr_to_wl->user_target;
		}


	} else {
		pwr_to_wl->flags = power->flags;
		pwr_to_wl->chanspec = power->chanspec;
		pwr_to_wl->local_chanspec = power->local_chanspec;
		pwr_to_wl->display_core = power->display_core;
		pwr_to_wl->rf_cores = power->rf_cores;
		memcpy(&pwr_to_wl->est_Pout, &power->est_Pout, 4);
		memcpy(&pwr_to_wl->est_Pout_act, &power->est_Pout_act, 4);
		memcpy(&pwr_to_wl->est_Pout, &power->est_Pout, 4);
		pwr_to_wl->est_Pout_cck = power->est_Pout_cck;
		memcpy(&pwr_to_wl->tx_power_max, &power->tx_power_max, 4);
		memcpy(&pwr_to_wl->tx_power_max_rate_ind, &power->tx_power_max_rate_ind, 4);
		pwr_to_wl->last_tx_ratespec = wlc_get_rspec_history(bsscfg);
#ifdef WL_SARLIMIT
		memcpy(&pwr_to_wl->SARLIMIT, &power->SARLIMIT, MAX_STREAMS_SUPPORTED);
#else
		memset(&pwr_to_wl->SARLIMIT, WLC_TXPWR_MAX, MAX_STREAMS_SUPPORTED);
#endif
		if (PHYTYPE_MIMO_CAP(wlc->band->phytype))
		{
			if (WLCISNPHY(wlc->band))
			{
				spatial_map = 0;
			}
			else
			{
				spatial_map = wlc_stf_spatial_expansion_get(wlc,
					pwr_to_wl->last_tx_ratespec);
			}
			pwr_to_wl->target_offsets[0] = wlc_stf_get_pwrperrate(wlc,
				pwr_to_wl->last_tx_ratespec, spatial_map) *2;
		}
		else
		{
			pwr_to_wl->target_offsets[0] = WL_RATE_DISABLED;
		}
		/* Copy first offset into other cores until we have different per core targets. */
		pwr_to_wl->target_offsets[1] = pwr_to_wl->target_offsets[0];
		pwr_to_wl->target_offsets[2] = pwr_to_wl->target_offsets[0];

		if ((err = ppr_serialize(power->ppr_board_limits, pserbuf,
			pwr_to_wl->board_limit_len, &serlen)) != BCME_OK) {
			err = BCME_ERROR;
			goto free_power;
		}
		pserbuf += pwr_to_wl->board_limit_len;
		if ((err = ppr_serialize(power->ppr_target_powers, pserbuf,
			pwr_to_wl->target_len, &serlen)) != BCME_OK) {
			err = BCME_ERROR;
			goto free_power;
		}
	}
free_power:
	if (power->ppr_board_limits)
		ppr_delete(wlc->osh, power->ppr_board_limits);
	if (power->ppr_target_powers)
		ppr_delete(wlc->osh, power->ppr_target_powers);
	MFREE(wlc->osh, power, sizeof(phy_tx_power_t));
free_pwr_to_wl:
	if (old_power)
		MFREE(wlc->osh, pwr_to_wl, len);
	return err;
}
#endif /* (defined(BCMDBG) || defined(WLTEST) || ...) && defined(PPI_API) */


#ifndef WLC_NET80211
uint8 *
wlc_tpc_write_constraint_ie(wlc_tpc_info_t *tpc, wlc_bsscfg_t *cfg, uint8 *cp, int buflen)
{
	return wlc_write_info_elt_safe(cp, buflen, DOT11_MNG_PWR_CONSTRAINT_ID,
	                               1, &tpc->pwr_constraint);
}
#endif

/* accessors */
void
wlc_tpc_set_local_max(wlc_tpc_info_t *tpc, uint8 pwr)
{
	tpc->txpwr_local_max = pwr;
}

#endif /* WLTPC */
