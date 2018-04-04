/*
 * Separate alloc/free module for wlc_xxx.c files. Decouples
 * the code that does alloc/free from other code so data
 * structure changes don't affect ROMMED code as much.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_alloc.c 375473 2012-12-18 23:12:14Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <proto/802.11.h>
#include <proto/802.11e.h>
#include <proto/wpa.h>
#include <proto/vlan.h>
#include <wlioctl.h>
#if defined(BCMSUP_PSK)
#include <proto/eapol.h>
#endif 
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_alloc.h>

static wlc_pub_t *wlc_pub_malloc(osl_t *osh, uint unit, uint devid);
static void wlc_pub_mfree(osl_t *osh, wlc_pub_t *pub);

static void wlc_tunables_init(wlc_tunables_t *tunables, uint devid);

static bool wlc_attach_malloc_high(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err, uint devid);
static bool wlc_attach_malloc_misc(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err, uint devid);
static void wlc_detach_mfree_high(wlc_info_t *wlc, osl_t *osh);
static void wlc_detach_mfree_misc(wlc_info_t *wlc, osl_t *osh);

void *
wlc_calloc(osl_t *osh, uint unit, uint size)
{
	void *item;

	if ((item = MALLOC(osh, size)) == NULL)
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          unit, __FUNCTION__, MALLOCED(osh)));
	else
		bzero((char*)item, size);
	return item;
}

#ifndef NTXD_USB_4319
#define NTXD_USB_4319 0
#define NRPCTXBUFPOST_USB_4319 0
#endif
#ifndef DNGL_MEM_RESTRICT_RXDMA     /* used only for BMAC low driver */
#define DNGL_MEM_RESTRICT_RXDMA 0
#endif

void
BCMATTACHFN(wlc_tunables_init)(wlc_tunables_t *tunables, uint devid)
{
	/* tx/rx ring size for DMAs with 512 descriptor ring size max */
	tunables->ntxd = NTXD;
	tunables->nrxd = NRXD;

	/* tx/rx ring size for DMAs with 4096 descriptor ring size max */
	tunables->ntxd_large = NTXD_LARGE;
	tunables->nrxd_large = NRXD_LARGE;

	tunables->rxbufsz = RXBUFSZ;
	tunables->nrxbufpost = NRXBUFPOST;
	tunables->maxscb = MAXSCB;
	tunables->ampdunummpdu2streams = AMPDU_NUM_MPDU;
	tunables->ampdunummpdu3streams = AMPDU_NUM_MPDU_3STREAMS;
	tunables->maxpktcb = MAXPKTCB;
	tunables->maxdpt = WLC_MAXDPT;
	tunables->maxucodebss = WLC_MAX_UCODE_BSS;
	tunables->maxucodebss4 = WLC_MAX_UCODE_BSS4;
	tunables->maxbss = MAXBSS;
	tunables->datahiwat = WLC_DATAHIWAT;
	tunables->ampdudatahiwat = WLC_AMPDUDATAHIWAT;
	tunables->rxbnd = RXBND;
	tunables->txsbnd = TXSBND;
	tunables->pktcbnd = PKTCBND;
#ifdef WLC_HIGH_ONLY
	tunables->rpctxbufpost = NRPCTXBUFPOST;
	if (devid == BCM4319_CHIP_ID) {
		tunables->ntxd = NTXD_USB_4319;
		tunables->rpctxbufpost = NRPCTXBUFPOST_USB_4319;
	}
#endif /* WLC_HIGH_ONLY */
#if defined(WLC_LOW_ONLY)
	tunables->dngl_mem_restrict_rxdma = DNGL_MEM_RESTRICT_RXDMA;
#endif
	tunables->maxtdls = WLC_MAXTDLS;
#ifdef DONGLEBUILD
	tunables->pkt_maxsegs = 1;
#else /* DONGLEBUILD */
	tunables->pkt_maxsegs = MAX_DMA_SEGS;
#endif /* DONGLEBUILD */
	tunables->maxscbcubbies = MAXSCBCUBBIES;
	tunables->maxbsscfgcubbies = MAXBSSCFGCUBBIES;

	tunables->max_notif_clients = MAX_NOTIF_CLIENTS;
	tunables->max_notif_servers = MAX_NOTIF_SERVERS;
	tunables->max_mempools = MAX_MEMPOOLS;
	tunables->amsdu_resize_buflen = PKTBUFSZ/4;
	tunables->ampdu_pktq_size = AMPDU_PKTQ_LEN;
	tunables->ampdu_pktq_fav_size = AMPDU_PKTQ_FAVORED_LEN;

	/* set 4360 specific tunables */
	if (IS_DEV_AC3X3(devid) || IS_DEV_AC2X2(devid)) {
		tunables->ntxd = NTXD_AC3X3;
		tunables->nrxd = NRXD_AC3X3;
		tunables->rxbnd = RXBND_AC3X3;
		tunables->nrxbufpost = NRXBUFPOST_AC3X3;
		tunables->pktcbnd = PKTCBND_AC3X3;
#ifdef DSLCPE
		tunables->txsbnd = TXBND_AC3X3;
#endif

		/* tx/rx ring size for DMAs with 4096 descriptor ring size max */
		tunables->ntxd_large = NTXD_LARGE_AC3X3;
		tunables->nrxd_large = NRXD_LARGE_AC3X3;
	}
#ifdef PROP_TXSTATUS
	tunables->wlfcfifocreditac0 = WLFCFIFOCREDITAC0;
	tunables->wlfcfifocreditac1 = WLFCFIFOCREDITAC1;
	tunables->wlfcfifocreditac2 = WLFCFIFOCREDITAC2;
	tunables->wlfcfifocreditac3 = WLFCFIFOCREDITAC3;
	tunables->wlfcfifocreditbcmc = WLFCFIFOCREDITBCMC;
	tunables->wlfcfifocreditother = WLFCFIFOCREDITOTHER;
	tunables->wlfc_fifo_cr_pending_thresh_ac_bk = WLFC_FIFO_CR_PENDING_THRESH_AC_BK;
	tunables->wlfc_fifo_cr_pending_thresh_ac_be = WLFC_FIFO_CR_PENDING_THRESH_AC_BE;
	tunables->wlfc_fifo_cr_pending_thresh_ac_vi = WLFC_FIFO_CR_PENDING_THRESH_AC_VI;
	tunables->wlfc_fifo_cr_pending_thresh_ac_vo = WLFC_FIFO_CR_PENDING_THRESH_AC_VO;
#endif /* PROP_TXSTATUS */
}

static wlc_pub_t *
BCMATTACHFN(wlc_pub_malloc)(osl_t *osh, uint unit, uint devid)
{
	wlc_pub_t *pub;

	if ((pub = (wlc_pub_t*) wlc_calloc(osh, unit, sizeof(wlc_pub_t))) == NULL) {
		goto fail;
	}

	if ((pub->tunables = (wlc_tunables_t *)
	     wlc_calloc(osh, unit, sizeof(wlc_tunables_t))) == NULL) {
		goto fail;
	}

	/* need to init the tunables now */
	wlc_tunables_init(pub->tunables, devid);

#ifdef WLCNT
	if ((pub->_cnt = (wl_cnt_t*) wlc_calloc(osh, unit, sizeof(wl_cnt_t))) == NULL) {
		goto fail;
	}

	if ((pub->_wme_cnt = (wl_wme_cnt_t*)
	     wlc_calloc(osh, unit, sizeof(wl_wme_cnt_t))) == NULL) {
		goto fail;
	}
#endif /* WLCNT */

	return pub;

fail:
	wlc_pub_mfree(osh, pub);
	return NULL;
}

static void
BCMATTACHFN(wlc_pub_mfree)(osl_t *osh, wlc_pub_t *pub)
{
	if (pub == NULL)
		return;

	if (pub->tunables) {
		MFREE(osh, pub->tunables, sizeof(wlc_tunables_t));
		pub->tunables = NULL;
	}

#ifdef WLCNT
	if (pub->_cnt) {
		MFREE(osh, pub->_cnt, sizeof(wl_cnt_t));
		pub->_cnt = NULL;
	}

	if (pub->_wme_cnt) {
		MFREE(osh, pub->_wme_cnt, sizeof(wl_wme_cnt_t));
		pub->_wme_cnt = NULL;
	}
#endif /* WLCNT */

	MFREE(osh, pub, sizeof(wlc_pub_t));
}

#ifdef WLCHANIM
static void
BCMATTACHFN(wlc_chanim_mfree)(osl_t *osh, chanim_info_t *c_info)
{
	wlc_chanim_stats_t *headptr = c_info->stats;
	wlc_chanim_stats_t *curptr;

	while (headptr) {
		curptr = headptr;
		headptr = headptr->next;
		MFREE(osh, curptr, sizeof(wlc_chanim_stats_t));
	}
	c_info->stats = NULL;

	MFREE(osh, c_info, sizeof(chanim_info_t));
}
#endif /* WLCHANIM */

static bool
BCMATTACHFN(wlc_attach_malloc_high)(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err, uint devid)
{
#ifdef WLC_HIGH
	if ((wlc->modulecb = (modulecb_t*)
	     wlc_calloc(osh, unit, sizeof(modulecb_t) * WLC_MAXMODULES)) == NULL) {
		*err = 1012;
		goto fail;
	}

	if ((wlc->default_bss = (wlc_bss_info_t*)
	     wlc_calloc(osh, unit, sizeof(wlc_bss_info_t))) == NULL) {
		*err = 1010;
		goto fail;
	}

	if ((wlc->stf = (wlc_stf_t*)
	     wlc_calloc(osh, unit, sizeof(wlc_stf_t))) == NULL) {
		*err = 1017;
		goto fail;
	}

	if ((wlc->corestate->macstat_snapshot = (macstat_t*)
	     wlc_calloc(osh, unit, sizeof(macstat_t))) == NULL) {
		*err = 1027;
		goto fail;
	}

#ifndef WLC_NET80211
	if ((wlc->scan_results = (wlc_bss_list_t*)
	     wlc_calloc(osh, unit, sizeof(wlc_bss_list_t))) == NULL) {
		*err = 1007;
		goto fail;
	}

	if ((wlc->custom_scan_results = (wlc_bss_list_t*)
	     wlc_calloc(osh, unit, sizeof(wlc_bss_list_t))) == NULL) {
		*err = 1008;
		goto fail;
	}

	if ((wlc->obss = (obss_info_t*)
	     wlc_calloc(osh, unit, sizeof(obss_info_t))) == NULL) {
		*err = 1012;
		goto fail;
	}

	if ((wlc->pkt_callback = (pkt_cb_t*)
	     wlc_calloc(osh, unit,
	                (sizeof(pkt_cb_t) * (wlc->pub->tunables->maxpktcb + 1)))) == NULL) {
		*err = 1013;
		goto fail;
	}

	if ((wlc->txmod_fns = (txmod_info_t*)
	     wlc_calloc(osh, unit, sizeof(txmod_info_t) * TXMOD_LAST)) == NULL) {
		*err = 1014;
		goto fail;
	}

	if ((wlc->wsec_def_keys[0] = (wsec_key_t*)
	     wlc_calloc(osh, unit, (sizeof(wsec_key_t) * WLC_DEFAULT_KEYS))) == NULL) {
		*err = 1015;
		goto fail;
	}
	else {
		int i;
		for (i = 1; i < WLC_DEFAULT_KEYS; i++) {
			wlc->wsec_def_keys[i] = (wsec_key_t *)
				((uintptr)wlc->wsec_def_keys[0] + (sizeof(wsec_key_t) * i));
		}

#ifdef WLAMPDU_HOSTREORDER
		for (i = 0; i < WLC_DEFAULT_KEYS; i++) {
			wlc->wsec_def_keys[i]->iv_tw = (iv_tw_t *)wlc_calloc(osh, unit,
				(sizeof(iv_tw_t) * WLC_NUMRXIVS));
			if (wlc->wsec_def_keys[i]->iv_tw == NULL) {
				*err = 1015;
				goto fail;
			}
		}
#endif /* WLAMPDU_HOSTREORDER */
	}

	if ((wlc->btch = (wlc_btc_config_t*)
	     wlc_calloc(osh, unit, sizeof(wlc_btc_config_t))) == NULL) {
		*err = 1018;
		goto fail;
	}

#ifdef STA
	if ((wlc->join_targets = (wlc_bss_list_t*)
	     wlc_calloc(osh, unit, sizeof(wlc_bss_list_t))) == NULL) {
		*err = 1019;
		goto fail;
	}
#endif /* STA */

#if defined(DELTASTATS)
	if ((wlc->delta_stats = (delta_stats_info_t*)
	     wlc_calloc(osh, unit, sizeof(delta_stats_info_t))) == NULL) {
		*err = 1023;
		goto fail;
	}
#endif /* DELTASTATS */


#ifdef WLCHANIM
	if ((wlc->chanim_info = (chanim_info_t*)
	     wlc_calloc(osh, unit, sizeof(chanim_info_t))) == NULL) {
		*err = 1031;
		goto fail;
	}
#endif
#endif /* WLC_NET80211 */

#if	defined(PKTC) || defined(PKTC_DONGLE)
	if ((wlc->pktc_info = (wlc_pktc_info_t*)
	     wlc_calloc(osh, unit, sizeof(wlc_pktc_info_t))) == NULL) {
		*err = 1032;
		goto fail;
	}
#endif

#endif /* WLC_HIGH */

	return TRUE;

#ifdef WLC_HIGH
fail:
	return FALSE;
#endif
}

static bool
BCMATTACHFN(wlc_attach_malloc_misc)(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err, uint devid)
{
	return TRUE;
}

/*
 * The common driver entry routine. Error codes should be unique
 */
wlc_info_t *
BCMATTACHFN(wlc_attach_malloc)(osl_t *osh, uint unit, uint *err, uint devid)
{
	wlc_info_t *wlc;

	if ((wlc = (wlc_info_t*) wlc_calloc(osh, unit, sizeof(wlc_info_t))) == NULL) {
		*err = 1002;
		goto fail;
	}
	wlc->hwrxoff = WL_HWRXOFF;
	wlc->hwrxoff_pktget = (wlc->hwrxoff % 4) ?  wlc->hwrxoff : (wlc->hwrxoff + 2);
	/* allocate wlc_pub_t state structure */
	if ((wlc->pub = wlc_pub_malloc(osh, unit, devid)) == NULL) {
		*err = 1003;
		goto fail;
	}
	wlc->pub->wlc = wlc;

#ifdef BCMPKTPOOL
	wlc->pub->pktpool = SHARED_POOL;
#endif

	if ((wlc->bandstate[0] = (wlcband_t*)
	     wlc_calloc(osh, unit, (sizeof(wlcband_t) * MAXBANDS))) == NULL) {
		*err = 1010;
		goto fail;
	}
	else {
		int i;

		for (i = 1; i < MAXBANDS; i++) {
			wlc->bandstate[i] =
				(wlcband_t *)((uintptr)wlc->bandstate[0] + (sizeof(wlcband_t) * i));
		}
	}

	if ((wlc->corestate = (wlccore_t*)
	     wlc_calloc(osh, unit, sizeof(wlccore_t))) == NULL) {
		*err = 1011;
		goto fail;
	}

	if (!wlc_attach_malloc_high(wlc, osh, unit, err, devid))
		goto fail;

	if (!wlc_attach_malloc_misc(wlc, osh, unit, err, devid))
		goto fail;

	return wlc;

fail:
	wlc_detach_mfree(wlc, osh);
	return NULL;
}

static void
BCMATTACHFN(wlc_detach_mfree_high)(wlc_info_t *wlc, osl_t *osh)
{
#ifdef WLC_HIGH
	if (wlc->scan_results) {
		MFREE(osh, wlc->scan_results, sizeof(wlc_bss_list_t));
		wlc->scan_results = NULL;
	}

	if (wlc->custom_scan_results) {
		MFREE(osh, wlc->custom_scan_results, sizeof(wlc_bss_list_t));
		wlc->custom_scan_results = NULL;
	}

	if (wlc->modulecb) {
		MFREE(osh, wlc->modulecb, sizeof(modulecb_t) * WLC_MAXMODULES);
		wlc->modulecb = NULL;
	}

	if (wlc->default_bss) {
		MFREE(osh, wlc->default_bss, sizeof(wlc_bss_info_t));
		wlc->default_bss = NULL;
	}

	if (wlc->obss) {
		MFREE(osh, wlc->obss, sizeof(obss_info_t));
		wlc->obss = NULL;
	}

	if (wlc->pkt_callback && wlc->pub && wlc->pub->tunables) {
		MFREE(osh, wlc->pkt_callback,
		      sizeof(pkt_cb_t) * (wlc->pub->tunables->maxpktcb + 1));
		wlc->pkt_callback = NULL;
	}

	if (wlc->txmod_fns)
		MFREE(osh, wlc->txmod_fns, sizeof(txmod_info_t) * TXMOD_LAST);


#ifdef WLAMPDU_HOSTREORDER
	{
		int i = 0;
		for (i = 0; i < WLC_DEFAULT_KEYS; i++) {
			if (wlc->wsec_def_keys[i]->iv_tw)
				MFREE(osh, wlc->wsec_def_keys[i]->iv_tw, sizeof(iv_tw_t));
		}
	}
#endif /* WLAMPDU_HOSTREORDER */

	if (wlc->wsec_def_keys[0])
		MFREE(osh, wlc->wsec_def_keys[0], (sizeof(wsec_key_t) * WLC_DEFAULT_KEYS));

	if (wlc->stf) {
		MFREE(osh, wlc->stf, sizeof(wlc_stf_t));
		wlc->stf = NULL;
	}

	if (wlc->btch) {
		MFREE(osh, wlc->btch, sizeof(wlc_btc_config_t));
		wlc->btch = NULL;
	}

#ifdef STA
	if (wlc->join_targets) {
		MFREE(osh, wlc->join_targets, sizeof(wlc_bss_list_t));
		wlc->join_targets = NULL;
	}
#endif /* STA */

#if defined(DELTASTATS)
	if (wlc->delta_stats) {
		MFREE(osh, wlc->delta_stats, sizeof(delta_stats_info_t));
		wlc->delta_stats = NULL;
	}
#endif /* DELTASTATS */
#endif /* WLC_HIGH */
}

static void
BCMATTACHFN(wlc_detach_mfree_misc)(wlc_info_t *wlc, osl_t *osh)
{
}

void
BCMATTACHFN(wlc_detach_mfree)(wlc_info_t *wlc, osl_t *osh)
{
	if (wlc == NULL)
		return;

	wlc_detach_mfree_misc(wlc, osh);

	wlc_detach_mfree_high(wlc, osh);

	if (wlc->bandstate[0])
		MFREE(osh, wlc->bandstate[0], (sizeof(wlcband_t) * MAXBANDS));

	if (wlc->corestate) {
#ifdef WLC_HIGH
		if (wlc->corestate->macstat_snapshot) {
			MFREE(osh, wlc->corestate->macstat_snapshot, sizeof(macstat_t));
			wlc->corestate->macstat_snapshot = NULL;
		}
#endif
		MFREE(osh, wlc->corestate, sizeof(wlccore_t));
		wlc->corestate = NULL;
	}

#ifdef WLCHANIM
	if (wlc->chanim_info) {
		wlc_chanim_mfree(osh, wlc->chanim_info);
		wlc->chanim_info = NULL;
	}
#endif

#if	defined(PKTC) || defined(PKTC_DONGLE)
	if (wlc->pktc_info) {
		MFREE(osh, wlc->pktc_info, sizeof(wlc_pktc_info_t));
		wlc->pktc_info = NULL;
	}
#endif

	if (wlc->pub) {
		/* free pub struct */
		wlc_pub_mfree(osh, wlc->pub);
		wlc->pub = NULL;
	}

	/* free the wlc */
	MFREE(osh, wlc, sizeof(wlc_info_t));
	wlc = NULL;
}
