/*
 * 11n protection module source
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
 * $Id$
 */

#include <wlc_cfg.h>

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_alloc.h>
#include <wlc_scb.h>
#include <wl_dbg.h>
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif
#include <wlc_prot.h>
#include <wlc_prot_priv.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>

/* iovar table */
enum {
	IOV_PREAMBLE_OVERRIDE,
	IOV_N_PROTECTION,
	IOV_N_PROTECTION_OVERRIDE,
	IOV_OFDM_ASSOC,		/* get status of ofdm_assoc */
	IOV_40_INTOLERANT_DET,	/* 40 Mhz intolerant device detected */
	IOV_GF_PROTECTION,
	IOV_GF_PROTECTION_OVERRIDE,
	IOV_LAST
};

static const bcm_iovar_t prot_n_iovars[] = {
	{"mimo_preamble", IOV_PREAMBLE_OVERRIDE, (IOVF_SET_DOWN|IOVF_OPEN_ALLOW), IOVT_INT8, 0},
	{"nmode_protection", IOV_N_PROTECTION, (IOVF_OPEN_ALLOW), IOVT_UINT8, 0},
	{"nmode_protection_override", IOV_N_PROTECTION_OVERRIDE, (IOVF_OPEN_ALLOW), IOVT_INT32, 0},
#ifdef BCMDBG
	{"ofdm_present", IOV_OFDM_ASSOC, (0), IOVT_BOOL, 0},
#endif
	{"intol40_det", IOV_40_INTOLERANT_DET, (IOVF_OPEN_ALLOW), IOVT_BOOL, 0},
	{"gf_protection", IOV_GF_PROTECTION, (IOVF_OPEN_ALLOW), IOVT_UINT8, 0},
	{"gf_protection_override", IOV_GF_PROTECTION_OVERRIDE, (IOVF_OPEN_ALLOW), IOVT_INT32, 0},
	{NULL, 0, 0, 0, 0}
};

/* module private states */
typedef struct {
	wlc_info_t *wlc;
	uint16 cfg_offset;	/* bss_prot_n_cfg_t offset in bsscfg cubby client */
	uint16 cond_offset;	/* bss_prot_n_cond_t offset in bsscfg cubby client */
	uint16 to_offset;	/* bss_prot_n_to_t offset in bsscfg cubby client */
	int8 nmode_user;	/* user config nmode, operating pub->nmode is different */
} wlc_prot_n_info_priv_t;

/* wlc_prot_n_info_priv_t offset in module states */
static uint16 wlc_prot_n_info_priv_offset = sizeof(wlc_prot_n_info_t);

/* module states layout */
typedef struct {
	wlc_prot_n_info_t pub;
	wlc_prot_n_info_priv_t priv;
} wlc_prot_n_t;
/* module states size */
#define WLC_PROT_N_SIZE	(sizeof(wlc_prot_n_t))
/* moudle states location */
#define WLC_PROT_N_INFO_PRIV(prot) ((wlc_prot_n_info_priv_t *) \
				    ((uintptr)(prot) + wlc_prot_n_info_priv_offset))

/* bsscfg specific states */
/* configurations */
typedef	struct {
	int8	n_cfg_override;		/* override for use of N protection */
	int8	nongf_override;		/* override for use of GF protection */
	bool	n_obss;			/* indicated OBSS Non-HT STA present */
} bss_prot_n_cfg_t;

/* conditions - each field must be 8 bits */
typedef struct {
	uint8	ofdm_assoc;		/* OFDM STAs associated */
	uint8	non_gf_assoc;		/* non GF STAs associated */
	uint8	ht20in40_assoc;		/* 20MHz only HT STAs associated in a 40MHz */
	uint8	non11n_apsd_assoc;	/* an associated STA is non-11n APSD */
	uint8	ht40intolerant_assoc;	/* an associated STA that is 40 intolerant */
	uint8	wme_apsd_assoc;		/* an associated STA is using APSD */
} bss_prot_n_cond_t;
/* access macros */
#define OFDM_ASSOC	OFFSETOF(bss_prot_n_cond_t, ofdm_assoc)
#define NONGF_ASSOC	OFFSETOF(bss_prot_n_cond_t, non_gf_assoc)
#define HT20IN40_ASSOC	OFFSETOF(bss_prot_n_cond_t, ht20in40_assoc)
#define NONNAPSD_ASSOC	OFFSETOF(bss_prot_n_cond_t, non11n_apsd_assoc)
#define HT40INT_ASSOC	OFFSETOF(bss_prot_n_cond_t, ht40intolerant_assoc)
#define WMEAPSD_ASSOC	OFFSETOF(bss_prot_n_cond_t, wme_apsd_assoc)

/* timeouts */
typedef struct {
	uint	ofdm_ibss_timeout;	/* #sec until ofdm IBSS beacons gone */
	uint	ofdm_ovlp_timeout;	/* #sec until ofdm overlapping BSS bcns gone */
	uint	n_ibss_timeout;		/* #sec until bcns signaling Use_OFDM_Protection gone */
	uint	ht20in40_ovlp_timeout;	/* #sec until 20MHz overlapping OPMODE gone */
	uint	ht20in40_ibss_timeout;	/* #sec until 20MHz-only HT station bcns gone */
	uint	non_gf_ibss_timeout;	/* #sec until non-GF bcns gone */
} bss_prot_n_to_t;

/* bsscfg states layout */
typedef struct {
	wlc_prot_n_cfg_t pub;
	bss_prot_n_cfg_t cfg;
	bss_prot_n_cond_t cond;
	bss_prot_n_to_t to;
} bss_prot_n_t;
/* bsscfg states size */
#define BSS_PROT_N_SIZE	(sizeof(bss_prot_n_t))
/* bsscfg states location */
#define BSS_PROT_N_CUBBY_LOC(prot, cfg) ((bss_prot_n_t **)BSSCFG_CUBBY((cfg), (prot)->cfgh))
#define BSS_PROT_N_CUBBY(prot, cfg) (*BSS_PROT_N_CUBBY_LOC(prot, cfg))
#define BSS_PROT_N_CFG(prot, cfg) ((bss_prot_n_cfg_t *) \
				   ((uintptr)BSS_PROT_N_CUBBY(prot, cfg) + \
				    WLC_PROT_N_INFO_PRIV(prot)->cfg_offset))
#define BSS_PROT_N_COND(prot, cfg) ((bss_prot_n_cond_t *) \
				    ((uintptr)BSS_PROT_N_CUBBY(prot, cfg) + \
				     WLC_PROT_N_INFO_PRIV(prot)->cond_offset))
#define BSS_PROT_N_TO(prot, cfg) ((bss_prot_n_to_t *) \
				  ((uintptr)BSS_PROT_N_CUBBY(prot, cfg) + \
				   WLC_PROT_N_INFO_PRIV(prot)->to_offset))

/* local functions */
/* module entries */
static int wlc_prot_n_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static int wlc_prot_n_watchdog(void *ctx);
#if defined(BCMDBG)
static int wlc_prot_n_dump(void *ctx, struct bcmstrbuf *b);
#endif

/* bsscfg cubby */
static int wlc_prot_n_bss_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_prot_n_bss_deinit(void *ctx, wlc_bsscfg_t *cfg);
#if defined(BCMDBG)
static void wlc_prot_n_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_prot_n_bss_dump NULL
#endif

/* update configuration */
static void wlc_prot_n_cfg_upd(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg, uint idx, int val);
/* wlc_prot_n_cfg_upd() idx */
#define	WLC_PROT_N_SPEC		1	/* n protection */
#define	WLC_PROT_N_CFG_OVR	2	/* n protection override */
#define	WLC_PROT_N_NONGF	3	/* non-GF protection */
#define	WLC_PROT_N_NONGF_OVR	4	/* non-GF protection override */
#define	WLC_PROT_N_OBSS		5	/* non-HT OBSS present */

#ifdef AP
/* update condition */
static void wlc_prot_n_cond_set(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg, uint coff, bool set);
#endif

/* others */
static void wlc_prot_n_nongf_init(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg);

/* 11n protection code starts here... */

/* module entries */
wlc_prot_n_info_t *
BCMATTACHFN(wlc_prot_n_attach)(wlc_info_t *wlc)
{
	wlc_prot_n_info_t *prot;
	wlc_prot_n_info_priv_t *priv;
	uint j;

	/* sanity check */
	ASSERT(wlc != NULL);

	/* module states storage */
	if ((prot = wlc_calloc(wlc->osh, wlc->pub->unit, WLC_PROT_N_SIZE)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	wlc_prot_n_info_priv_offset = OFFSETOF(wlc_prot_n_t, priv);
	priv = WLC_PROT_N_INFO_PRIV(prot);
	priv->wlc = wlc;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((prot->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(bss_prot_n_t *),
	                wlc_prot_n_bss_init, wlc_prot_n_bss_deinit, wlc_prot_n_bss_dump,
	                prot)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	priv->cfg_offset = OFFSETOF(bss_prot_n_t, cfg);
	priv->cond_offset = OFFSETOF(bss_prot_n_t, cond);
	priv->to_offset = OFFSETOF(bss_prot_n_t, to);

	/* register module callbacks */
	if (wlc_module_register(wlc->pub, prot_n_iovars, "prot_n", prot, wlc_prot_n_doiovar,
	                        wlc_prot_n_watchdog, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "prot_n", wlc_prot_n_dump, (void *)prot);
#endif

	/* default configurations */
	for (j = 0; j < NBANDS(wlc); j++) {
		wlcband_t *band;

		/* Use band 1 for single band 11a */
		if (IS_SINGLEBAND_5G(wlc->deviceid))
			j = BAND_5G_INDEX;

		band = wlc->bandstate[j];

		/* init _n_enab supported mode */
		if (WLC_PHY_11N_CAP(band)) {
			int val = wlc->pub->_n_enab == OFF ? OFF :
			        wlc->pub->_n_enab == SUPPORT_11N ?
			        WL_11N_2x2 : WL_11N_3x3;
			wlc_prot_n_cfg_set(prot, WLC_PROT_N_USER, val);
		}
	}
	wlc_prot_n_cfg_set(prot, WLC_PROT_N_PAM_OVR, AUTO);

	return prot;

	/* error handling */
fail:
	wlc_prot_n_detach(prot);
	return NULL;
}

void
BCMATTACHFN(wlc_prot_n_detach)(wlc_prot_n_info_t *prot)
{
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc;

	if (prot == NULL)
		return;

	wlc = priv->wlc;

	wlc_module_unregister(wlc->pub, "prot_n", prot);

	MFREE(wlc->osh, prot, WLC_PROT_N_SIZE);
}

static int
wlc_prot_n_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_prot_n_info_t *prot = (wlc_prot_n_info_t *)ctx;
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	wlc_bsscfg_t *cfg;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bss_prot_n_cfg_t *pnc;
	wlc_prot_n_cfg_t *wpnc;
	bss_prot_n_cond_t *pnd;

	/* update bsscfg w/provided interface context */
	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	wpnc = WLC_PROT_N_CFG(prot, cfg);
	ASSERT(wpnc != NULL);

	pnc = BSS_PROT_N_CFG(prot, cfg);
	ASSERT(pnc != NULL);

	pnd = BSS_PROT_N_COND(prot, cfg);
	ASSERT(pnd != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_PREAMBLE_OVERRIDE):
		*ret_int_ptr = (int32)prot->n_pam_override;
		break;

	case IOV_SVAL(IOV_PREAMBLE_OVERRIDE):
		if ((int_val != AUTO) &&
		    (int_val != WLC_N_PREAMBLE_MIXEDMODE) &&
		    (int_val != WLC_N_PREAMBLE_GF) &&
		    (int_val != WLC_N_PREAMBLE_GF_BRCM)) {
			err = BCME_RANGE;
			break;
		}

		if (WLCISACPHY(wlc->band)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (WLC_PHY_11N_CAP(wlc->band)) {
			if (NREV_IS(wlc->band->phyrev, 3) ||
			    NREV_IS(wlc->band->phyrev, 4)) {
				WL_ERROR(("can't change preamble for 4322, ignore\n"));
				break;
			}

			wlc_prot_n_cfg_set(prot, WLC_PROT_N_PAM_OVR, (int8)int_val);
			wlc_phy_preamble_override_set(wlc->band->pi, (int8)int_val);
		}
		break;

	case IOV_GVAL(IOV_N_PROTECTION):
		*ret_int_ptr = (int32)wpnc->n_cfg;
		break;

	case IOV_GVAL(IOV_N_PROTECTION_OVERRIDE):
		*ret_int_ptr = (int32)pnc->n_cfg_override;
		break;

	case IOV_SVAL(IOV_N_PROTECTION_OVERRIDE):
		if ((int_val != WLC_PROTECTION_AUTO) &&
		    (int_val != WLC_PROTECTION_OFF) &&
		    (int_val != WLC_PROTECTION_ON) &&
		    (int_val != WLC_PROTECTION_CTS_ONLY) &&
		    (int_val != WLC_PROTECTION_MMHDR_ONLY)) {
			err = BCME_RANGE;
			break;
		}

		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_CFG_OVR, (int8)int_val);

		if (cfg->associated) {
			/* let watchdog or beacon processing update protection */
		} else {
			wlc_prot_n_cfg_init(prot, cfg);
		}
		break;

	case IOV_GVAL(IOV_GF_PROTECTION):
		*ret_int_ptr = (int32)wpnc->nongf;
		break;

	case IOV_GVAL(IOV_GF_PROTECTION_OVERRIDE):
		*ret_int_ptr = (int32)pnc->nongf_override;
		break;

	case IOV_SVAL(IOV_GF_PROTECTION_OVERRIDE):
		if ((int_val != WLC_PROTECTION_AUTO) &&
		    (int_val != WLC_PROTECTION_OFF) &&
		    (int_val != WLC_PROTECTION_ON)) {
			err = BCME_RANGE;
			break;
		}

		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_NONGF_OVR, (int8)int_val);

		if (cfg->associated) {
			/* let watchdog or beacon processing update protection */
		} else {
			wlc_prot_n_nongf_init(prot, cfg);
		}
		break;

#ifdef BCMDBG
	case IOV_GVAL(IOV_OFDM_ASSOC):
		*ret_int_ptr = pnd->ofdm_assoc ? 1 : 0;
		break;
#endif

	case IOV_GVAL(IOV_40_INTOLERANT_DET):
		*ret_int_ptr = (WLC_INTOL40_DET(cfg) | pnd->ht40intolerant_assoc) ? 1 : 0;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_prot_n_watchdog(void *ctx)
{
	wlc_prot_n_info_t *prot = (wlc_prot_n_info_t *)ctx;
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	int idx;
	wlc_bsscfg_t *cfg;

#ifdef AP
	FOREACH_UP_AP(wlc, idx, cfg) {
		bss_prot_n_cond_t *pnd;

		pnd = BSS_PROT_N_COND(prot, cfg);
		ASSERT(pnd != NULL);

		/* update our ofdm flag based on associated STAs */
		if (pnd->ofdm_assoc && !wlc_prot_scb_scan(wlc, cfg, (SCB_HTCAP | SCB_NONERP), 0))
			wlc_prot_n_cond_set(prot, cfg, OFDM_ASSOC, FALSE);

		if (pnd->non_gf_assoc && !wlc_prot_scb_scan(wlc, cfg, SCB_NONGF, SCB_NONGF))
			wlc_prot_n_cond_set(prot, cfg, NONGF_ASSOC, FALSE);

		if (pnd->ht20in40_assoc && !wlc_prot_scb_scan(wlc, cfg, SCB_HTCAP, SCB_HTCAP))
			wlc_prot_n_cond_set(prot, cfg, HT20IN40_ASSOC, FALSE);

		/* update non 11n APSD association status */
		if (pnd->non11n_apsd_assoc &&
		    !wlc_prot_scb_scan(wlc, cfg, SCB_APSDCAP, SCB_APSDCAP))
			wlc_prot_n_cond_set(prot, cfg, NONNAPSD_ASSOC, FALSE);

		if (pnd->ht40intolerant_assoc &&
		    !wlc_prot_scb_scan(wlc, cfg, SCB_HT40INTOLERANT, SCB_HT40INTOLERANT))
			wlc_prot_n_cond_set(prot, cfg, HT40INT_ASSOC, FALSE);

		if (pnd->wme_apsd_assoc && !wlc_prot_scb_scan(wlc, cfg, SCB_APSDCAP, SCB_APSDCAP))
			wlc_prot_n_cond_set(prot, cfg, WMEAPSD_ASSOC, FALSE);

	}
#endif /* AP */

	FOREACH_AS_BSS(wlc, idx, cfg) {
		bss_prot_n_to_t *pnt;

		pnt = BSS_PROT_N_TO(prot, cfg);
		ASSERT(pnt != NULL);

		/* decrement beacon detection timeouts */

		/* overlapping OFDM BSS */
		if (pnt->ofdm_ovlp_timeout)
			pnt->ofdm_ovlp_timeout--;
		/* overlapping 20MHz Operating BSS */
		if (pnt->ht20in40_ovlp_timeout)
			pnt->ht20in40_ovlp_timeout--;
		/* member OFDM IBSS bcn */
		if (pnt->ofdm_ibss_timeout)
			pnt->ofdm_ibss_timeout--;
		/* member non-GF IBSS bcn */
		if (pnt->non_gf_ibss_timeout)
			pnt->non_gf_ibss_timeout--;
		/* member 20MHz-only IBSS bcn */
		if (pnt->ht20in40_ibss_timeout)
			pnt->ht20in40_ibss_timeout--;
		/* Beacon in IBSS signaling Use_OFDM_Protection detected */
		if (pnt->n_ibss_timeout)
			pnt->n_ibss_timeout--;

		if (BSSCFG_AP(cfg) || !cfg->BSS) {
			if (N_ENAB(wlc->pub))
				wlc_prot_n_mode_upd(prot, cfg);
		}
	}

	return BCME_OK;
}

#if defined(BCMDBG)
static int
wlc_prot_n_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_prot_n_info_t *prot = (wlc_prot_n_info_t *)ctx;
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	int idx;
	wlc_bsscfg_t *cfg;

	bcm_bprintf(b, "prot_n: priv_offset %d cfgh %d nmode_user %d n_pam_override %d\n",
	            wlc_prot_n_info_priv_offset, prot->cfgh,
	            priv->nmode_user, prot->n_pam_override);

	FOREACH_AS_BSS(wlc, idx, cfg) {
		bcm_bprintf(b, "bsscfg %d >\n", WLC_BSSCFG_IDX(cfg));
	        wlc_prot_n_bss_dump(prot, cfg, b);
	}

	return BCME_OK;
}
#endif 

/* bsscfg cubby */
static int
wlc_prot_n_bss_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_prot_n_info_t *prot = (wlc_prot_n_info_t *)ctx;
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	bss_prot_n_t **ppn = BSS_PROT_N_CUBBY_LOC(prot, cfg);
	bss_prot_n_t *pn = NULL;
	int err = BCME_OK;

	/* allocate memory and point bsscfg cubby to it */
	if ((pn = wlc_calloc(wlc->osh, wlc->pub->unit, BSS_PROT_N_SIZE)) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	*ppn = pn;

	/* default 11n protection configurations */
	wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC, WLC_PROTECTION_OFF);
	wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_CFG_OVR, WLC_PROTECTION_AUTO);
	wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_NONGF, FALSE);
	wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_NONGF_OVR, WLC_PROTECTION_AUTO);

	return BCME_OK;

fail:
	wlc_prot_n_bss_deinit(ctx, cfg);
	return err;
}

static void
wlc_prot_n_bss_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_prot_n_info_t *prot = (wlc_prot_n_info_t *)ctx;
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc;
	bss_prot_n_t **ppn = BSS_PROT_N_CUBBY_LOC(prot, cfg);
	bss_prot_n_t *pn = *ppn;

	if (pn == NULL)
		return;

	wlc = priv->wlc;

	MFREE(wlc->osh, pn, BSS_PROT_N_SIZE);
	*ppn = NULL;
}

#if defined(BCMDBG)
static void
wlc_prot_n_bss_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_prot_n_info_t *prot = (wlc_prot_n_info_t *)ctx;
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_prot_n_cfg_t *wpnc;
	bss_prot_n_cfg_t *pnc;
	bss_prot_n_cond_t *pnd;
	bss_prot_n_to_t *pnt;

	ASSERT(cfg != NULL);

	wpnc = WLC_PROT_N_CFG(prot, cfg);
	ASSERT(wpnc != NULL);

	pnc = BSS_PROT_N_CFG(prot, cfg);
	ASSERT(pnc != NULL);

	pnd = BSS_PROT_N_COND(prot, cfg);
	ASSERT(pnd != NULL);

	pnt = BSS_PROT_N_TO(prot, cfg);
	ASSERT(pnt != NULL);

	bcm_bprintf(b, "\tcfg_offset %d cond_offset %d to_offset %d\n",
	            priv->cfg_offset, priv->cond_offset, priv->to_offset);
	bcm_bprintf(b, "\tn_cfg %d n_cfg_ovrrd %d "
	            "nongf %d nongf_ovrrd %d obss_nonht_sta %d\n",
	            wpnc->n_cfg, pnc->n_cfg_override,
	            wpnc->nongf, pnc->nongf_override, pnc->n_obss);

	bcm_bprintf(b, "\tofdm_assoc %d "
	            "ht20in40_assoc %d non_gf_assoc %d ht20intolerant_assoc %d "
	            "non11n_apsd_assoc %d wme_apsd_assoc %d\n",
	            pnd->ofdm_assoc,
	            pnd->ht20in40_assoc, pnd->non_gf_assoc, pnd->ht40intolerant_assoc,
	            pnd->non11n_apsd_assoc, pnd->wme_apsd_assoc);

	bcm_bprintf(b, "\tofdm_ovlp_timeout %d ofdm_ibss_timeout %d "
	            "n_prot_ibss_detect_timeout %d "
	            "ht20in40_ovlp_timeout %d ht20in40_ibss_timeout %d "
	            "non_gf_ibss_timeout %d\n",
	            pnt->ofdm_ovlp_timeout,
	            pnt->ofdm_ibss_timeout,
	            pnt->n_ibss_timeout,
	            pnt->ht20in40_ovlp_timeout,
	            pnt->ht20in40_ibss_timeout,
	            pnt->non_gf_ibss_timeout);
}
#endif 

/* centralized protection config change function to simplify debugging, no consistency checking
 * this should be called only on changes to avoid overhead in periodic function
 */
void
wlc_prot_n_cfg_set(wlc_prot_n_info_t *prot, uint idx, int val)
{
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);

	WL_TRACE(("wlc_prot_n_cfg_set: idx %d, val %d\n", idx, val));

	switch (idx) {
	case WLC_PROT_N_USER:
		priv->nmode_user = (int8)val;
		break;
	case WLC_PROT_N_PAM_OVR:
		prot->n_pam_override = (int8)val;
		break;
	default:
		ASSERT(0);
		break;
	}
}

void
wlc_prot_n_mode_reset(wlc_prot_n_info_t *prot, bool force)
{
#ifdef WL11N
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;

	if (force || N_ENAB(wlc->pub) != priv->nmode_user)
		wlc_set_nmode(wlc, priv->nmode_user);
#endif
}

static void
wlc_prot_n_cfg_upd(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg, uint idx, int val)
{
	wlc_prot_n_info_priv_t *priv;
	wlc_prot_n_cfg_t *wpnc;
	bss_prot_n_cfg_t *pnc;

	ASSERT(cfg != NULL);

	wpnc = WLC_PROT_N_CFG(prot, cfg);
	ASSERT(wpnc != NULL);

	pnc = BSS_PROT_N_CFG(prot, cfg);
	ASSERT(pnc != NULL);

	priv = WLC_PROT_N_INFO_PRIV(prot);

	WL_PROT(("wl%d.%d %s: idx %d, val %d\n",
	         priv->wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, idx, val));
	BCM_REFERENCE(priv);

	switch (idx) {
	case WLC_PROT_N_SPEC:
		wpnc->n_cfg = (int8)val;
		break;
	case WLC_PROT_N_CFG_OVR:
		pnc->n_cfg_override = (int8)val;
		break;
	case WLC_PROT_N_NONGF:
		wpnc->nongf = (bool)val;
		break;
	case WLC_PROT_N_NONGF_OVR:
		pnc->nongf_override = (int8)val;
		break;
	case WLC_PROT_N_OBSS:
		pnc->n_obss = (bool)val;
		break;

	default:
		ASSERT(0);
		break;
	}
}

/* update n_cfg, but comply with its override */
void
wlc_prot_n_cfg_init(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg)
{
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	bss_prot_n_cfg_t *pnc;

	/* shared */
	wlc_prot_cfg_init(wlc->prot, cfg);

	/* 11n specific */
	ASSERT(cfg != NULL);

	pnc = BSS_PROT_N_CFG(prot, cfg);
	ASSERT(pnc != NULL);

	wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_OBSS, FALSE);

	if (pnc->n_cfg_override == WLC_PROTECTION_AUTO) {
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC, WLC_N_PROTECTION_OFF);
	} else if (pnc->n_cfg_override == WLC_PROTECTION_ON) {
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC, WLC_N_PROTECTION_MIXEDMODE);
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_OBSS, TRUE);
	} else if (pnc->n_cfg_override == WLC_PROTECTION_MMHDR_ONLY) {
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC, WLC_N_PROTECTION_OPTIONAL);
	} else if (pnc->n_cfg_override == WLC_PROTECTION_CTS_ONLY) {
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC, WLC_N_PROTECTION_20IN40);
	} else {
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC, WLC_N_PROTECTION_OFF);
	}
}

static void
wlc_prot_n_nongf_init(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg)
{
	bss_prot_n_cfg_t *pnc;

	ASSERT(cfg != NULL);

	pnc = BSS_PROT_N_CFG(prot, cfg);
	ASSERT(pnc != NULL);

	if (pnc->nongf_override == WLC_PROTECTION_AUTO)
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_NONGF, FALSE);
	else
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_NONGF,
		                   (pnc->nongf_override == WLC_PROTECTION_ON));
}

/* policy routine
 *  - honor n_cfg_override
 *  - update only when there is change, this can be called by process_beacon()
 *  - may update ucode when necessary
 *  - Overlap is not supported due to protection propagate
 */
bool
wlc_prot_n_mode_upd(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg)
{
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	bool update_ucode = FALSE;
	bool ofdm_protect, nongf_protect, ht20in40_protect;
	bool n_protect = FALSE;
	bool obss_protect;
	int8 use_n_protection;
	bss_prot_cfg_t *pc;
	wlc_prot_n_cfg_t *wpnc;
	bss_prot_n_cfg_t *pnc;
	bss_prot_n_cond_t *pnd;
	bss_prot_n_to_t *pnt;
	bool g_prot;

	ASSERT(cfg != NULL);

	pc = BSS_PROT_CFG(wlc->prot, cfg);
	ASSERT(pc != NULL);

	wpnc = WLC_PROT_N_CFG(prot, cfg);
	ASSERT(wpnc != NULL);

	pnc = BSS_PROT_N_CFG(prot, cfg);
	ASSERT(pnc != NULL);

	pnd = BSS_PROT_N_COND(prot, cfg);
	ASSERT(pnd != NULL);

	pnt = BSS_PROT_N_TO(prot, cfg);
	ASSERT(pnt != NULL);

	g_prot = WLC_PROT_G_CFG_G(wlc->prot_g, cfg);

	ASSERT(N_ENAB(wlc->pub));

	ofdm_protect = (pnd->ofdm_assoc || pnt->ofdm_ibss_timeout > 0);
	nongf_protect = (pnd->non_gf_assoc || pnt->non_gf_ibss_timeout > 0);
	ht20in40_protect = (pnd->ht20in40_assoc || pnt->ht20in40_ibss_timeout > 0);

	obss_protect = FALSE;
	/* Update current OFDM protection */
	if (pnc->n_cfg_override == WLC_PROTECTION_AUTO) {
		if ((pc->overlap == WLC_PROTECTION_CTL_LOCAL ||
		     pc->overlap == WLC_PROTECTION_CTL_OVERLAP) &&
		    (ofdm_protect || ht20in40_protect)) {
			/* Use protection if OFDM STAs associated */
			n_protect = TRUE;
			obss_protect = TRUE;
		}
		else if (pc->overlap == WLC_PROTECTION_CTL_OVERLAP &&
		         pnt->ofdm_ovlp_timeout != 0) {
			n_protect = TRUE;
			obss_protect = TRUE;
		}

		/* Use Mixed Mode protection if we are advertising protection or
		 * we have seen Use_Protection signaled in our IBSS
		 */
		use_n_protection = WLC_N_PROTECTION_OFF;
		if (n_protect || pnt->n_ibss_timeout ||
		    (g_prot && CHSPEC_IS2G(wlc->home_chanspec))) {
			if (ofdm_protect ||
			    (g_prot && CHSPEC_IS2G(wlc->home_chanspec)))
				use_n_protection = WLC_N_PROTECTION_MIXEDMODE;
			else if (ht20in40_protect)
				/* use CTS-to-self if operate in 40MHz with
				 * Legacy OFDM associated or 20MHz operation
				 * overlapped or g-mode protection enabled
				 */
				use_n_protection = WLC_N_PROTECTION_20IN40;
			else if (pnt->ofdm_ovlp_timeout)
				use_n_protection = WLC_N_PROTECTION_OPTIONAL;
		}
	}
	else {
		/* the logic is the same as wlc_prot_n_cfg_init() */
		if (pnc->n_cfg_override == WLC_PROTECTION_ON) {
			use_n_protection = WLC_N_PROTECTION_MIXEDMODE;
			obss_protect = TRUE;
		}
		else if (pnc->n_cfg_override == WLC_PROTECTION_MMHDR_ONLY)
			use_n_protection = WLC_N_PROTECTION_OPTIONAL;
		else if (pnc->n_cfg_override == WLC_PROTECTION_CTS_ONLY)
			use_n_protection = WLC_N_PROTECTION_20IN40;
		else
			use_n_protection = WLC_N_PROTECTION_OFF;
	}

	if (wpnc->n_cfg != use_n_protection) {
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC, use_n_protection);
		update_ucode = TRUE;
	}

	if (pnc->nongf_override != WLC_PROTECTION_AUTO)
		nongf_protect =	(pnc->nongf_override == WLC_PROTECTION_ON);

	if (wpnc->nongf != nongf_protect) {
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_NONGF, nongf_protect);
		update_ucode = TRUE;
	}

	if (pnc->n_obss != obss_protect) {
		wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_OBSS, obss_protect);
		update_ucode = TRUE;
	}

	if (update_ucode) {
		WL_APSTA_BCN(("wl%d: %s() -> wlc_update_beacon()\n",
		              wlc->pub->unit, __FUNCTION__));
		wlc_bss_update_beacon(wlc, cfg);
		wlc_bss_update_probe_resp(wlc, cfg, TRUE);
	}

	return update_ucode;
}

static void
_wlc_prot_n_ovlp_upd(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg,
	chanspec_t home_chspec, ht_add_ie_t *add_ie, ht_cap_ie_t *cap_ie,
	bool is_erp, bool bw40)
{
	bss_prot_n_to_t *pnt;
	bool cap_40, bw_40;

	ASSERT(cfg != NULL);

	pnt = BSS_PROT_N_TO(prot, cfg);
	ASSERT(pnt != NULL);

	/* Check for overlapping NonMIMO OFDM BSS
	 * Consider as OFDM BSS if not MIMO and has BSS OFDM rates
	 * or the HT ADD Info indicates that OFDM STAs are associated,
	 */
	if ((!add_ie && is_erp) ||
	    (add_ie && HT_MIXEDMODE_PRESENT(add_ie)))
		pnt->ofdm_ovlp_timeout = WLC_IBSS_BCN_TIMEOUT;
	if (add_ie && HT_HT20_PRESENT(add_ie) && bw40)
		pnt->ht20in40_ovlp_timeout = WLC_IBSS_BCN_TIMEOUT;

	/* Check for overlapping BSS with 20MHz Operation only */
	if (cap_ie && add_ie) {
		cap_40 = ((ltoh16_ua(&cap_ie->cap) & HT_CAP_40MHZ) != 0);
		bw_40 = add_ie->byte1 & HT_BW_ANY;
		if (!cap_40 && bw_40 && bw40)
			pnt->ht20in40_ovlp_timeout = WLC_IBSS_BCN_TIMEOUT;
	}

	wlc_prot_n_mode_upd(prot, cfg);
}

void
wlc_prot_n_ovlp_upd(wlc_prot_n_info_t *prot, chanspec_t chspec,
	ht_add_ie_t *add_ie, ht_cap_ie_t *cap_ie, bool is_erp, bool bw40)
{
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	int i;
	wlc_bsscfg_t *bc;

	FOREACH_AS_BSS(wlc, i, bc) {
		if (BSSCFG_STA(bc) && bc->BSS)
			continue;

#ifdef WLMCHAN
		if (MCHAN_ENAB(wlc->pub) &&
		    bc->chan_context != NULL &&
		    !CHSPEC_CTLOVLP(chspec, bc->chan_context->chanspec, CH_20MHZ_APART))
			continue;
#endif
		_wlc_prot_n_ovlp_upd(prot, bc, chspec, add_ie, cap_ie, is_erp, bw40);
	}
}

void
wlc_prot_n_cfg_track(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg, ht_add_ie_t *add_ie)
{
	wlc_prot_n_cfg_t *wpnc;
	bss_prot_n_cfg_t *pnc;

	ASSERT(cfg != NULL);

	wpnc = WLC_PROT_N_CFG(prot, cfg);
	ASSERT(wpnc != NULL);

	pnc = BSS_PROT_N_CFG(prot, cfg);
	ASSERT(pnc != NULL);

	if (pnc->n_cfg_override == WLC_PROTECTION_AUTO) {
		int8 tmp = GET_HT_OPMODE(add_ie);
		if (wpnc->n_cfg != tmp)
			wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC, tmp);
		tmp = HT_NONGF_PRESENT(add_ie);
		if (wpnc->nongf != tmp)
			wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_NONGF, tmp);
		tmp = DOT11N_OBSS_NONHT_PRESENT(add_ie);
		if (pnc->n_obss != tmp)
			wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_OBSS, tmp);
		return;
	}

	wlc_prot_n_cfg_upd(prot, cfg, WLC_PROT_N_SPEC,
	                   pnc->n_cfg_override == WLC_PROTECTION_ON ?
	                   WLC_N_PROTECTION_MIXEDMODE : WLC_N_PROTECTION_OFF);
}

#ifdef AP
static uint8 *
wlc_prot_n_cond_loc(void *ctx, wlc_bsscfg_t *cfg, uint off)
{
	wlc_prot_n_info_t *prot = (wlc_prot_n_info_t *)ctx;
	bss_prot_n_cond_t *pnd;

	ASSERT(cfg != NULL);

	pnd = BSS_PROT_N_COND(prot, cfg);
	ASSERT(pnd != NULL);

	return (uint8 *)pnd + off;
}

static void
wlc_prot_n_cond_set(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg, uint offset, bool set)
{
	wlc_prot_n_info_priv_t *priv;

	priv = WLC_PROT_N_INFO_PRIV(prot);

	WL_PROT(("wl%d.%d %s: offset %d, val %d\n",
	         priv->wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, offset, set));

	wlc_prot_cond_set(priv->wlc, cfg, offset, set, wlc_prot_n_cond_loc, prot);
}

void
wlc_prot_n_cond_upd(wlc_prot_n_info_t *prot, struct scb *scb)
{
	wlc_bsscfg_t *cfg;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* Check for association of an OFDM STA */
	if ((scb->flags & (SCB_NONERP | SCB_HTCAP)) == 0)
		wlc_prot_n_cond_set(prot, cfg, OFDM_ASSOC, TRUE);

	/* Check for association of a Non Green Field STA */
	if (scb->flags & SCB_NONGF)
		wlc_prot_n_cond_set(prot, cfg, NONGF_ASSOC, TRUE);

	/* Check for association of a 20MHz only HT STA in 40MHz operating */
	if (!(scb->flags & SCB_IS40) &&
	    !CHSPEC_IS20(cfg->current_bss->chanspec))
		wlc_prot_n_cond_set(prot, cfg, HT20IN40_ASSOC, TRUE);

	/* Check for non 11n APSD association STA */
	if ((scb->flags & SCB_APSDCAP) && ((scb->flags & SCB_HTCAP) == 0))
		wlc_prot_n_cond_set(prot, cfg, NONNAPSD_ASSOC, TRUE);

	/* Check for association of a 40 intolerant STA */
	if (scb->flags & SCB_HT40INTOLERANT)
		wlc_prot_n_cond_set(prot, cfg, HT40INT_ASSOC, TRUE);

	/* Check for APSD association STA */
	if (scb->flags & SCB_APSDCAP)
		wlc_prot_n_cond_set(prot, cfg, WMEAPSD_ASSOC, TRUE);
}
#endif /* AP */

void
wlc_prot_n_build_add_ie(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg, ht_add_ie_t *add_ie)
{
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	wlc_bss_info_t *current_bss;
	chanspec_t chanspec;
	wlc_prot_n_cfg_t *wpnc;
	bss_prot_n_cfg_t *pnc;
	bss_prot_n_cond_t *pnd;

	ASSERT(cfg != NULL);

	current_bss = cfg->current_bss;
	chanspec = current_bss->chanspec;

	wpnc = WLC_PROT_N_CFG(prot, cfg);
	ASSERT(wpnc != NULL);

	pnc = BSS_PROT_N_CFG(prot, cfg);
	ASSERT(pnc != NULL);

	pnd = BSS_PROT_N_COND(prot, cfg);
	ASSERT(pnd != NULL);

	bzero(add_ie, sizeof(ht_add_ie_t));

#ifdef WL11AC
	/* Set up the HT IE according to the primary 40MHz channel
	 * for wider bandwidth channels
	 */
	if (!CHSPEC_IS20(chanspec) && !CHSPEC_IS40(chanspec)) {
		chanspec = wf_chspec_primary40_chspec(chanspec);
	}
#endif /* WL11AC */

	/* primary 20Mhz side band for 40Mhz channels */
	add_ie->ctl_ch = wf_chspec_ctlchan(chanspec);

	/* set the extension channel bits */
	if (CHSPEC_IS40(chanspec)) {
		switch (chanspec & WL_CHANSPEC_CTL_SB_MASK) {
			case WL_CHANSPEC_CTL_SB_UPPER:
				add_ie->byte1 |= DOT11_EXT_CH_LOWER;
				break;
			case WL_CHANSPEC_CTL_SB_LOWER:
				add_ie->byte1 |= DOT11_EXT_CH_UPPER;
				break;
			default:
				ASSERT(0);
				break;
		}
	} else {
		add_ie->byte1 |= DOT11_EXT_CH_NONE;
	}
	/* Set the MIMO b/w bit (and ext ch for 40Mhz mode) */
	if (CHSPEC_IS40(chanspec)) {
#ifdef WL11AC
	/* If Operating Mode BW 20MHz don't set this bit (11ac 10.41) */
	if (!cfg->oper_mode_enabled ||
		!DOT11_OPER_MODE_CHANNEL_WIDTH_20MHZ(cfg->oper_mode))
#endif /* WL11AC */
		add_ie->byte1 |= HT_BW_ANY;
	}


	/* Set the operating mode */
	add_ie->opmode = 0;
	if (wpnc->n_cfg == WLC_N_PROTECTION_20IN40)
		add_ie->opmode |= htol16(HT_OPMODE_HT20IN40);
	else if (wpnc->n_cfg == WLC_N_PROTECTION_MIXEDMODE)
		add_ie->opmode |= htol16(HT_OPMODE_MIXED);
	else if (wpnc->n_cfg == WLC_N_PROTECTION_OPTIONAL)
		add_ie->opmode |= htol16(HT_OPMODE_OPTIONAL);

	if (wpnc->nongf)
		add_ie->opmode |= htol16(HT_OPMODE_NONGF);
	if (pnc->n_obss)
		add_ie->opmode |= htol16(DOT11N_OBSS_NONHT);
	/* Set the RIFS mode enabled if no apsd STAs detected */
	add_ie->byte1 &= ~HT_RIFS_PERMITTED;
	wlc->rifs_mode = 0;
	if (wlc->rifs_advert && (wpnc->n_cfg != WLC_N_PROTECTION_MIXEDMODE)) {
		add_ie->byte1 |= HT_RIFS_PERMITTED;
		wlc->rifs_mode = 1;
	}

	wlc->txburst_limit = 0;
	if (wlc->txburst_limit_override == ON || pnd->non11n_apsd_assoc ||
	    (wlc->txburst_limit_override == AUTO && pnd->non11n_apsd_assoc)) {
		add_ie->opmode |= htol16(DOT11N_TXBURST);
		wlc->txburst_limit =
		        ((CHSPEC_WLCBANDUNIT(chanspec) ==
		                       BAND_5G_INDEX) ?
		                      DOT11N_5G_TXBURST_LIMIT :
		                      DOT11N_2G_TXBURST_LIMIT);
	}

	/* set the basic mcs */
	bcopy(&wlc->ht_add.basic_mcs[0], &add_ie->basic_mcs[0], MCSSET_LEN);
}

void
wlc_prot_n_cap_upd(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg, ht_cap_ie_t *ht_cap)
{
	if (prot->n_pam_override != AUTO) {
		if (prot->n_pam_override == WLC_N_PREAMBLE_MIXEDMODE)
			ht_cap->cap &= ~HT_CAP_GF;
		else
			ht_cap->cap |= HT_CAP_GF;
	}
}

void
wlc_prot_n_to_upd(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg,
	ht_add_ie_t *add_ie, ht_cap_ie_t *cap_ie, bool is_erp, bool bw40)
{
	bss_prot_n_to_t *pnt;

	ASSERT(cfg != NULL);

	pnt = BSS_PROT_N_TO(prot, cfg);
	ASSERT(pnt != NULL);

	/* Check for Mixedmode_Protection signaled in our IBSS */
	if (add_ie != NULL && HT_MIXEDMODE_PRESENT(add_ie))
		pnt->n_ibss_timeout = WLC_IBSS_BCN_TIMEOUT;

	if (cap_ie != NULL) {
		uint16 cap_ie_cap = ltoh16_ua(&cap_ie->cap);
		/* Check for Non-GF cap in beacons */
		if (!(cap_ie_cap & HT_CAP_GF))
			pnt->non_gf_ibss_timeout = WLC_IBSS_BCN_TIMEOUT;
		/* Check for 20in40_Protection signaled in our IBSS */
		if (!(cap_ie_cap & HT_CAP_40MHZ) && bw40) {
			pnt->ht20in40_ibss_timeout = WLC_IBSS_BCN_TIMEOUT;
		}
	}

	/* Check for OFDM beacons */
	if (add_ie == NULL && is_erp)
		pnt->ofdm_ibss_timeout = WLC_IBSS_BCN_TIMEOUT;

	wlc_prot_n_mode_upd(prot, cfg);
}

bool
wlc_prot_n_ht40int(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg)
{
	bss_prot_n_cond_t *pnd;

	ASSERT(cfg != NULL);

	pnd = BSS_PROT_N_COND(prot, cfg);
	ASSERT(pnd != NULL);

	return pnd->ht40intolerant_assoc != 0;
}

void
wlc_prot_n_init(wlc_prot_n_info_t *prot, wlc_bsscfg_t *cfg)
{
	wlc_prot_n_info_priv_t *priv = WLC_PROT_N_INFO_PRIV(prot);
	wlc_info_t *wlc = priv->wlc;
	bss_prot_n_cond_t *pnd;
	bss_prot_n_to_t *pnt;

	/* shared */
	wlc_prot_init(wlc->prot, cfg);

	/* 11n specific */
	ASSERT(cfg != NULL);

	pnd = BSS_PROT_N_COND(prot, cfg);
	ASSERT(pnd != NULL);

	pnt = BSS_PROT_N_TO(prot, cfg);
	ASSERT(pnt != NULL);

	bzero(pnd, sizeof(*pnd));
	bzero(pnt, sizeof(*pnt));

	wlc_prot_n_cfg_init(prot, cfg);
	wlc_prot_n_nongf_init(prot, cfg);
}
