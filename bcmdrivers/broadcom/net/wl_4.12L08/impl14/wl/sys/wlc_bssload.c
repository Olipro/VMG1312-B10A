/*
 * BSS load IE source file
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
 * $Id:$
 */

#include <wlc_cfg.h>
#ifdef WLBSSLOAD
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wl_dbg.h>
#include <wlc_alloc.h>
#include <wlc_bmac.h>
#include <wlc_bssload.h>

#define MAX_CHAN_UTIL 255

/* iovar table */
enum {
	IOV_BSSLOAD,	/* enable/disable BSS load IE */
	IOV_LAST
};

static const bcm_iovar_t bssload_iovars[] = {
	{"bssload", IOV_BSSLOAD, (0), IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
};

/* BSSLOAD module info */
struct wlc_bssload_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	cca_ucode_counts_t cca_stats;	/* cca stats from ucode */
	uint8 chan_util;	/* channel utilization */
};

/* local functions */
/* module */
static int wlc_bssload_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_bssload_watchdog(void *ctx);

wlc_bssload_info_t *
BCMATTACHFN(wlc_bssload_attach)(wlc_info_t *wlc)
{
	wlc_bssload_info_t *mbssload;

	if (!wlc)
		return NULL;

	if ((mbssload = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_bssload_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	mbssload->wlc = wlc;

	/* keep the module registration the last other add module unregistration
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, bssload_iovars, "bssload", mbssload, wlc_bssload_doiovar,
	                        wlc_bssload_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	wlc->pub->_bssload = TRUE;
	return mbssload;

	/* error handling */
fail:
	if (mbssload != NULL)
		MFREE(wlc->osh, mbssload, sizeof(wlc_bssload_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_bssload_detach)(wlc_bssload_info_t *mbssload)
{
	wlc_info_t *wlc;

	if (mbssload) {
		wlc = mbssload->wlc;
		wlc_module_unregister(wlc->pub, "bssload", mbssload);
		MFREE(wlc->osh, mbssload, sizeof(wlc_bssload_info_t));
	}
}

static int
wlc_bssload_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_bssload_info_t *mbssload = (wlc_bssload_info_t *)ctx;
	wlc_info_t *wlc = mbssload->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_BSSLOAD):
		*ret_int_ptr = (int32)wlc->pub->_bssload;
		break;
	case IOV_SVAL(IOV_BSSLOAD):
		wlc->pub->_bssload = bool_val;
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static void wlc_bssload_get_chan_util(wlc_info_t *wlc, uint8 *chan_util,
	cca_ucode_counts_t *cca_stats)
{
	cca_ucode_counts_t tmp;
	cca_ucode_counts_t delta;
	uint32 cu;

	if (wlc_bmac_cca_stats_read(wlc->hw, &tmp))
		return;

	/* Calc delta */
	delta.txdur = tmp.txdur - cca_stats->txdur;
	delta.ibss = tmp.ibss - cca_stats->ibss;
	delta.obss = tmp.obss - cca_stats->obss;
	delta.noctg = tmp.noctg - cca_stats->noctg;
	delta.nopkt = tmp.nopkt - cca_stats->nopkt;
	delta.usecs = tmp.usecs - cca_stats->usecs;

	if (delta.usecs == 0)
		return;

	cu = delta.ibss + delta.txdur + delta.obss + delta.noctg + delta.nopkt;
	cu = cu * MAX_CHAN_UTIL / delta.usecs;
	if (cu > MAX_CHAN_UTIL)
		cu = MAX_CHAN_UTIL;
	*chan_util = (uint8)cu;

	/* Store raw values for next read */
	cca_stats->txdur = tmp.txdur;
	cca_stats->ibss = tmp.ibss;
	cca_stats->obss = tmp.obss;
	cca_stats->noctg = tmp.noctg;
	cca_stats->nopkt = tmp.nopkt;
	cca_stats->usecs = tmp.usecs;
}

static int wlc_bssload_watchdog(void *ctx)
{
	wlc_bssload_info_t *mbssload = (wlc_bssload_info_t *)ctx;
	wlc_info_t *wlc = mbssload->wlc;
	uint8 chan_util;	/* old channel utilization */

	if (WLBSSLOAD_ENAB(wlc->pub)) {
		chan_util = mbssload->chan_util;
		wlc_bssload_get_chan_util(wlc, &mbssload->chan_util, &mbssload->cca_stats);
		/* update beacon only when CU is changed */
		if ((chan_util != mbssload->chan_util) && wlc->pub->up) {
			wlc_update_beacon(wlc);
			wlc_update_probe_resp(wlc, TRUE);
		}
	}
	return BCME_OK;
}

uint8 *wlc_bssload_write_ie_beacon(wlc_bssload_info_t *mbssload,
	wlc_bsscfg_t *cfg, uint8 *cp, uint8 *bufend)
{
	wlc_info_t *wlc;
	uint16 sta_count;

	if (!mbssload)
		return cp;

	wlc = mbssload->wlc;
	if (!BSS_WLBSSLOAD_ENAB(wlc, cfg))
		return cp;

	if (BUFLEN(cp, bufend) < BSS_LOAD_IE_SIZE) {
		WL_ERROR(("wl%d: %s: buffer is too small\n",
		          wlc->pub->unit, __FUNCTION__));
		return cp;
	}

	sta_count = wlc_bss_assocscb_getcnt(wlc, cfg);

	cp[0] = DOT11_MNG_QBSS_LOAD_ID;
	cp[1] = BSS_LOAD_IE_SIZE - TLV_HDR_LEN;
	cp[2] = (uint8)(sta_count & 0xFF);	/* LSB - station count */
	cp[3] = (uint8)(sta_count >> 8);	/* MSB - station count */
	cp[4] = mbssload->chan_util;	/* channel utilization */
	cp[5] = 0;	/* LSB - available admission capacity */
	cp[6] = 0;	/* MSB - available admission capacity */
	cp += BSS_LOAD_IE_SIZE;
	return cp;
}
#endif /* WLBSSLOAD */
