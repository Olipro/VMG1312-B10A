/*
 * WAPI (WLAN Authentication and Privacy Infrastructure) source file
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
#if !defined(BCMWAPI_WPI) && !defined(BCMWAPI_WAI)
#error "BCMWAPI_WPI or BCMWAPI_WAI is not defined"
#endif
#if defined(BCMWAPI_WAI) && !defined(BCMWAPI_WPI)
#error "BCMWAPI_WPI must enabled when BCMWPAI_WAI enabled"
#endif

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmwpa.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_scb.h>
#include <wlc_alloc.h>
#include <wlc_ap.h>

#include <wlc_wapi.h>
#include <wlc_wapi_priv.h>


#ifdef BCMWAPI_WAI
/* IOVAR table ,no ordering is imposed */
enum {
	IOV_WAPI_HW_ENABLED,
	IOV_WAI_RESTRICT,
	IOV_WAI_REKEY,
	IOV_WAPIIE,
	IOV_LAST
};

static const bcm_iovar_t wlc_wapi_iovars[] = {
	{"wapi_hw_enabled", IOV_WAPI_HW_ENABLED, (0), IOVT_BOOL, 0},
	{"wai_restrict", IOV_WAI_RESTRICT, (0), IOVT_BOOL, 0},
	{"wai_rekey", IOV_WAI_REKEY, (0), IOVT_BUFFER, ETHER_ADDR_LEN},
	{"wapiie", IOV_WAPIIE, (0), IOVT_BUFFER, 0},
	{NULL, 0, 0, 0, 0}
};

static int wlc_wapi_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_wapi_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg);
static int wlc_wapi_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
static void wlc_wapi_wai_rekey(wlc_wapi_info_t *wapi, wlc_bsscfg_t *cfg, struct ether_addr *addr);
#else
#define wlc_wapi_iovars		NULL
#define wlc_wapi_doiovar	NULL
#endif /* BCMWAPI_WAI */

#ifdef BCMDBG
static int wlc_wapi_dump(wlc_wapi_info_t *wapi, struct bcmstrbuf *b);
static void wlc_wapi_scb_dump(void *context, struct scb *scb, struct bcmstrbuf *b);
#ifdef BCMWAPI_WAI
static void wlc_wapi_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#endif
#else
#define wlc_wapi_dump		NULL
#define wlc_wapi_scb_dump	NULL
#ifdef BCMWAPI_WAI
#define wlc_wapi_bsscfg_dump	NULL
#endif
#endif /* BCMDBG */

static int wlc_wapi_wlc_up(void *ctx);


/* Module attach */
wlc_wapi_info_t *
BCMATTACHFN(wlc_wapi_attach)(wlc_info_t *wlc)
{
	wlc_wapi_info_t *wapi = NULL;
#ifdef BCMWAPI_WAI
	/* bsscfg private variables are appened to wlc_wapi_bsscfg_cubby_pub_t structure */
	uint priv_offset = ROUNDUP(sizeof(wlc_wapi_bsscfg_cubby_pub_t), PTRSZ);
#endif
	if ((wapi = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_wapi_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	wapi->wlc = wlc;
	wapi->pub = wlc->pub;

#ifdef HW_WAPI
	/* default enable wapi */
	wapi->pub->_wapi_hw_wpi = WAPI_HW_WPI_CAP(wlc);
#endif

	/* reserve cubby in the scb container for per-scb private data */
	if ((wapi->scbh = wlc_scb_cubby_reserve(wlc, sizeof(wapi_scb_cubby_t),
	                NULL, NULL, wlc_wapi_scb_dump, (void *)wapi)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef BCMWAPI_WAI
	/* reserve cubby in the bsscfg container for per-bsscfg public/private data */
	if ((wlc->wapi_cfgh = wlc_bsscfg_cubby_reserve(wlc,
		priv_offset + sizeof(wlc_wapi_bsscfg_cubby_priv_t),
		wlc_wapi_bsscfg_init, wlc_wapi_bsscfg_deinit, wlc_wapi_bsscfg_dump,
		(void *)wapi)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* save module internal useful variables */
	wapi->cfgh = wlc->wapi_cfgh;
	wapi->priv_offset = priv_offset;
#endif /* BCMWAPI_WAI */

	/* keep the module registration the last other add module unregistratin
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, wlc_wapi_iovars, "wapi", (void *)wapi,
		wlc_wapi_doiovar, NULL, wlc_wapi_wlc_up, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "wapi", (dump_fn_t)wlc_wapi_dump, (void *)wapi);
#endif

	return wapi;

	/* error handling */
fail:
	if (wapi != NULL)
		MFREE(wlc->osh, wapi, sizeof(wlc_wapi_info_t));

	return NULL;
}

/* Module detach */
void
BCMATTACHFN(wlc_wapi_detach)(wlc_wapi_info_t *wapi)
{
	if (wapi == NULL)
		return;

	wlc_module_unregister(wapi->pub, "wapi", wapi);

	MFREE(wapi->wlc->osh, wapi, sizeof(wlc_wapi_info_t));
}

static int
wlc_wapi_wlc_up(void *ctx)
{
	wlc_wapi_info_t *wapi = (wlc_wapi_info_t *)ctx;

	if (WAPI_HW_WPI_ENAB(wapi->wlc))
		wapi->wapimickeys = wlc_bmac_read_shm(wapi->wlc->hw, M_WAPIMICKEYS_PTR) * 2;

	return BCME_OK;
}

#ifdef BCMWAPI_WAI
static int
wlc_wapi_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_wapi_info_t *wapi = (wlc_wapi_info_t *)ctx;
	wlc_info_t *wlc = wapi->wlc;
	wlc_wapi_bsscfg_cubby_pub_t *cfg_pub;
	wlc_wapi_bsscfg_cubby_priv_t *cfg_priv;
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

	cfg_pub = WAPI_BSSCFG_PUB(wlc, bsscfg);
	cfg_priv = WAPI_BSSCFG_PRIV(wapi, bsscfg);
	ASSERT(cfg_pub != NULL);
	ASSERT(cfg_priv != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_WAPI_HW_ENABLED):
		*ret_int_ptr = (int32)WAPI_HW_WPI_ENAB(wlc);
		break;

	case IOV_GVAL(IOV_WAI_RESTRICT):
		*ret_int_ptr = (int32)cfg_pub->wai_restrict;
		break;

	case IOV_SVAL(IOV_WAI_RESTRICT):
		cfg_pub->wai_restrict = bool_val;
		break;

	case IOV_SVAL(IOV_WAI_REKEY):
		wlc_wapi_wai_rekey(wapi, bsscfg, arg);
		break;

	case IOV_GVAL(IOV_WAPIIE):
		if (len < cfg_priv->wapi_ie_len)
			err = BCME_BUFTOOSHORT;
		else if (!cfg_priv->wapi_ie_len)
			err = BCME_NOTFOUND;
		else
			bcopy(cfg_priv->wapi_ie, (uint8 *)arg, cfg_priv->wapi_ie_len);
		break;

	case IOV_SVAL(IOV_WAPIIE):
		if (cfg_priv->wapi_ie) {
			MFREE(wapi->pub->osh, cfg_priv->wapi_ie,
				cfg_priv->wapi_ie_len);
			cfg_priv->wapi_ie = NULL;
			cfg_priv->wapi_ie_len = 0;
		}

		if (p_len == 0)
			break;

		if (!(cfg_priv->wapi_ie = MALLOC(wapi->pub->osh, p_len))) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wapi->pub->unit, __FUNCTION__,
				MALLOCED(wapi->pub->osh)));
			err = BCME_NOMEM;
			break;
		}
		cfg_priv->wapi_ie_len = p_len;
		bcopy((uint8*)params, cfg_priv->wapi_ie, p_len);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_wapi_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_wapi_info_t *wapi = (wlc_wapi_info_t *)ctx;
	wlc_wapi_bsscfg_cubby_pub_t *cfg_pub = WAPI_BSSCFG_PUB(wapi->wlc, cfg);

	ASSERT(cfg_pub != NULL);

	/* disable WAI authentication by default */
	cfg_pub->wai_restrict = FALSE;

	return BCME_OK;
}

static void
wlc_wapi_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_wapi_info_t *wapi = (wlc_wapi_info_t *)ctx;
	wlc_wapi_bsscfg_cubby_priv_t *cfg_priv = WAPI_BSSCFG_PRIV(wapi, cfg);

	if (cfg_priv == NULL)
		return;

	/* free wapi_ie */
	if (cfg_priv->wapi_ie) {
		MFREE(wapi->pub->osh, cfg_priv->wapi_ie, cfg_priv->wapi_ie_len);
		cfg_priv->wapi_ie = NULL;
		cfg_priv->wapi_ie_len = 0;
	}

	return;
}

/* WAI rekey for mcast/unicast key */
static void
wlc_wapi_wai_rekey(wlc_wapi_info_t *wapi, wlc_bsscfg_t *cfg, struct ether_addr *addr)
{
	/* sendup a rekey event */
	wlc_wapi_station_event(wapi, cfg, addr, NULL, NULL,
		ETHER_ISNULLADDR(addr) ? WAPI_MUTIL_REKEY : WAPI_UNICAST_REKEY);

	return;
}

/* decode wapi IE to retrieve mcast/unicast ciphers and auth modes */
int
wlc_wapi_parse_ie(wlc_wapi_info_t *wapi, bcm_tlv_t *wapiie, wlc_bss_info_t *bi)
{
	int len = wapiie->len;		/* value length */
	wpa_suite_t *mcast;
	wpa_suite_ucast_t *ucast;
	wpa_suite_auth_key_mgmt_t *mgmt;
	uint8 *cap;
	uint16 count;
	uint i, j;

	/* Check min length and version */
	if (len < WAPI_IE_MIN_LEN) {
		WL_ERROR(("wl%d: WAPI IE illegally short %d\n", wapi->pub->unit, len));
		return BCME_UNSUPPORTED;
	}

	if ((len < WAPI_VERSION_LEN) ||
	    (ltoh16_ua(wapiie->data) != WAPI_VERSION)) {
		WL_ERROR(("wl%d: unsupported WAPI version %d\n", wapi->pub->unit,
			ltoh16_ua(wapiie->data)));
		return BCME_UNSUPPORTED;
	}
	len -= WAPI_VERSION_LEN;

	/* Default WAPI parameters */
	bi->wapi.flags = RSN_FLAGS_SUPPORTED;
	bi->wapi.multicast = WAPI_CIPHER_SMS4;
	bi->wapi.ucount = 1;
	bi->wapi.unicast[0] = WAPI_CIPHER_SMS4;
	bi->wapi.acount = 1;
	bi->wapi.auth[0] = RSN_AKM_UNSPECIFIED;

	/* Check for auth key management suite(s) */
	/* walk thru auth management suite list and pick up what we recognize */
	mgmt = (wpa_suite_auth_key_mgmt_t *)&wapiie->data[WAPI_VERSION_LEN];
	count = ltoh16_ua(&mgmt->count);
	len -= WPA_IE_SUITE_COUNT_LEN;
	for (i = 0, j = 0;
	     i < count && j < ARRAYSIZE(bi->wapi.auth) && len >= WPA_SUITE_LEN;
	     i ++, len -= WPA_SUITE_LEN) {
		if (!bcmp(mgmt->list[i].oui, WAPI_OUI, DOT11_OUI_LEN)) {
			if (IS_WAPI_AKM(mgmt->list[i].type))
				bi->wapi.auth[j++] = mgmt->list[i].type;
			else
				WL_INFORM(("wl%d: unsupported WAPI auth %d\n",
					wapi->pub->unit, mgmt->list[i].type));
		} else
			WL_INFORM(("wl%d: unsupported proprietary auth OUI "
				   "%02X:%02X:%02X\n", wapi->pub->unit,
				   mgmt->list[i].oui[0], mgmt->list[i].oui[1],
				   mgmt->list[i].oui[2]));
	}
	bi->wapi.acount = (uint8)j;
	bi->flags |= WLC_BSS_WAPI;

	/* jump to unicast suites */
	len -= (count - i) * WPA_SUITE_LEN;

	/* Check for unicast suites */
	if (len < WPA_IE_SUITE_COUNT_LEN) {
		WL_INFORM(("wl%d: no unicast suite\n", wapi->pub->unit));
		return BCME_UNSUPPORTED;
	}

	/* walk thru unicast cipher list and pick up what we recognize */
	ucast = (wpa_suite_ucast_t *)&mgmt->list[count];
	count = ltoh16_ua(&ucast->count);
	len -= WPA_IE_SUITE_COUNT_LEN;
	for (i = 0, j = 0;
	     i < count && j < ARRAYSIZE(bi->wapi.unicast) && len >= WPA_SUITE_LEN;
	     i ++, len -= WPA_SUITE_LEN) {
		if (!bcmp(ucast->list[i].oui, WAPI_OUI, DOT11_OUI_LEN)) {
			if (IS_WAPI_CIPHER(ucast->list[i].type))
				bi->wapi.unicast[j++] = WAPI_CSE_WPI_2_CIPHER(ucast->list[i].type);
			else
				WL_INFORM(("wl%d: unsupported WAPI unicast cipher %d\n",
					wapi->pub->unit, ucast->list[i].type));
		} else
			WL_INFORM(("wl%d: unsupported proprietary unicast cipher OUI "
				   "%02X:%02X:%02X\n", wapi->pub->unit,
				   ucast->list[i].oui[0], ucast->list[i].oui[1],
				   ucast->list[i].oui[2]));
	}
	bi->wapi.ucount = (uint8)j;

	/* jump to mcast suites */
	len -= (count - i) * WPA_SUITE_LEN;

	/* Check for multicast cipher suite */
	if (len < WPA_SUITE_LEN) {
		WL_INFORM(("wl%d: no multicast cipher suite\n", wapi->pub->unit));
		return BCME_UNSUPPORTED;
	}

	/* pick up multicast cipher if we know what it is */
	mcast = (wpa_suite_mcast_t *)&ucast->list[count];
	len -= WPA_SUITE_LEN;
	if (!bcmp(mcast->oui, WAPI_OUI, DOT11_OUI_LEN)) {
		if (IS_WAPI_CIPHER(mcast->type))
			bi->wapi.multicast = WAPI_CSE_WPI_2_CIPHER(mcast->type);
		else
			WL_INFORM(("wl%d: unsupported WAPI multicast cipher %d\n",
				wapi->pub->unit, mcast->type));
	} else
		WL_INFORM(("wl%d: unsupported proprietary multicast cipher OUI "
			   "%02X:%02X:%02X\n", wapi->pub->unit,
			   mcast->oui[0], mcast->oui[1], mcast->oui[2]));

	/* RSN capabilities is optional */
	if (len < RSN_CAP_LEN) {
		WL_INFORM(("wl%d: no rsn cap\n", wapi->pub->unit));
		/* it is ok to not have RSN Cap */
		return 0;
	}

	/* parse RSN capabilities */
	cap = (uint8 *)&mcast[1];
	if (cap[0] & WAPI_CAP_PREAUTH)
		bi->wapi.flags |= RSN_FLAGS_PREAUTH;

	return 0;
}

uint8 *
wlc_wapi_write_ie_safe(wlc_wapi_info_t *wapi, uint8 *cp, int buflen, uint16 WPA_auth,
	uint32 wsec, wlc_bsscfg_t *bsscfg)
{
	/* Infrastructure WAPI info element */
	uint WPA_len = 0;	/* tag length */
	bcm_tlv_t *wapiie = (bcm_tlv_t *)cp;
	wpa_suite_mcast_t *mcast;
	wpa_suite_ucast_t *ucast;
	wpa_suite_auth_key_mgmt_t *auth;
	uint16 count;
	uint8 *cap;
	uint8 *orig_cp = cp;
	wlc_wapi_bsscfg_cubby_priv_t *cfg_priv = WAPI_BSSCFG_PRIV(wapi, bsscfg);

	ASSERT(cfg_priv != NULL);

	if (!INCLUDES_WAPI_AUTH(bsscfg->WPA_auth) || !WSEC_ENABLED(bsscfg->wsec))
		return cp;

	WL_WSEC(("wl%d: adding RSN IE, wsec = 0x%x\n", wapi->pub->unit, wsec));

	/* perform length check */
	/* if buffer too small, return untouched buffer */
	BUFLEN_CHECK_AND_RETURN((&wapiie->data[WAPI_VERSION_LEN] - &wapiie->id), buflen, orig_cp);

	/* fixed portion */
	wapiie->id = DOT11_MNG_WAPI_ID;
	wapiie->data[0] = (uint8)WAPI_VERSION;
	wapiie->data[1] = (uint8)(WAPI_VERSION>>8);
	WPA_len = WAPI_VERSION_LEN;

	/* authenticated key management suite list */
	auth = (wpa_suite_auth_key_mgmt_t *)&wapiie->data[WAPI_VERSION_LEN];
	count = 0;

	/* length check */
	/* if buffer too small, return untouched buffer */
	BUFLEN_CHECK_AND_RETURN(WPA_IE_SUITE_COUNT_LEN, buflen, orig_cp);

	WPA_len += WPA_IE_SUITE_COUNT_LEN;
	buflen -= WPA_IE_SUITE_COUNT_LEN;

	if (WPA_auth & WAPI_AUTH_UNSPECIFIED) {
		/* length check */
		/* if buffer too small, return untouched buffer */
		BUFLEN_CHECK_AND_RETURN(WPA_SUITE_LEN, buflen, orig_cp);
		bcopy(WAPI_OUI, auth->list[count].oui, DOT11_OUI_LEN);
		auth->list[count++].type = RSN_AKM_UNSPECIFIED;
		WPA_len += WPA_SUITE_LEN;
		buflen -= WPA_SUITE_LEN;
	}
	if (WPA_auth & WAPI_AUTH_PSK) {
		/* length check */
		/* if buffer too small, return untouched buffer */
		BUFLEN_CHECK_AND_RETURN(WPA_SUITE_LEN, buflen, orig_cp);
		bcopy(WAPI_OUI, auth->list[count].oui, DOT11_OUI_LEN);
		auth->list[count++].type = RSN_AKM_PSK;
		WPA_len += WPA_SUITE_LEN;
		buflen -= WPA_SUITE_LEN;
	}

	ASSERT(count);
	auth->count.low = (uint8)count;
	auth->count.high = (uint8)(count>>8);

	/* unicast suite list */
	ucast = (wpa_suite_ucast_t *)&auth->list[count];
	count = 0;

	/* length check */
	/* if buffer too small, return untouched buffer */
	BUFLEN_CHECK_AND_RETURN(WPA_IE_SUITE_COUNT_LEN, buflen, orig_cp);

	WPA_len += WPA_IE_SUITE_COUNT_LEN;
	buflen -= WPA_IE_SUITE_COUNT_LEN;

	if (WSEC_SMS4_ENABLED(wsec)) {
		/* length check */
		/* if buffer too small, return untouched buffer */
		BUFLEN_CHECK_AND_RETURN(WPA_SUITE_LEN, buflen, orig_cp);
		bcopy(WAPI_OUI, ucast->list[count].oui, DOT11_OUI_LEN);
		ucast->list[count++].type = WAPI_CIPHER_2_CSE_WPI(WAPI_CIPHER_SMS4);
		WPA_len += WPA_SUITE_LEN;
		buflen -= WPA_SUITE_LEN;
	}
	ASSERT(count);
	ucast->count.low = (uint8)count;
	ucast->count.high = (uint8)(count>>8);

	/* multicast suite */
	/* length check */
	/* if buffer too small, return untouched buffer */
	BUFLEN_CHECK_AND_RETURN(WPA_SUITE_LEN, buflen, orig_cp);
	mcast = (wpa_suite_mcast_t *)&ucast->list[count];
	bcopy(WAPI_OUI, mcast->oui, DOT11_OUI_LEN);
	mcast->type = WAPI_CIPHER_2_CSE_WPI(wlc_wpa_mcast_cipher(wapi->wlc, bsscfg));
	WPA_len += WPA_SUITE_LEN;
	buflen -= WPA_SUITE_LEN;

	/* WAPI capabilities */
	/* length check */
	/* if buffer too small, return untouched buffer */
	BUFLEN_CHECK_AND_RETURN(WPA_CAP_LEN, buflen, orig_cp);

	cap = (uint8 *)&mcast[1];
	cap[0] = 0;
	cap[1] = 0;
	if (BSSCFG_AP(bsscfg) && (bsscfg->WPA_auth & WAPI_AUTH_UNSPECIFIED) &&
	    (cfg_priv->wai_preauth == TRUE))
	    cap[0] = WAPI_CAP_PREAUTH;
	WPA_len += WPA_CAP_LEN;
	buflen -= WPA_CAP_LEN;

	/* update tag length */
	wapiie->len = (uint8)WPA_len;

	if (WPA_len)
		cp += TLV_HDR_LEN + WPA_len;

	return (cp);
}

#if !defined(WLNOEIND)
/* Send BRCM encapsulated WAI Events to applications. */
void
wlc_wapi_bss_wai_event(wlc_wapi_info_t *wapi, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea,
	uint8 *data, uint32 len)
{
	wlc_event_t *e;

	/* 'data' should point to a WAI header */
	if (data == NULL || len == 0) {
		WL_ERROR(("wl%d: wai missing", wapi->pub->unit));
		return;
	}

	e = wlc_event_alloc(wapi->wlc->eventq);
	if (e == NULL) {
		WL_ERROR(("wl%d: %s wlc_event_alloc failed\n", wapi->pub->unit, __FUNCTION__));
		return;
	}

	e->event.event_type = WLC_E_WAI_MSG;
	e->event.status = WLC_E_STATUS_SUCCESS;
	e->event.reason = 0;
	e->event.auth_type = 0;

	e->event.datalen = len;
	e->data = MALLOC(wapi->pub->osh, e->event.datalen);
	if (e->data == NULL) {
		wlc_event_free(wapi->wlc->eventq, e);
		WL_ERROR(("wl%d: %s MALLOc failed\n", wapi->pub->unit, __FUNCTION__));
		return;
	}

	bcopy(data, e->data, e->event.datalen);

	wlc_event_if(wapi->wlc, bsscfg, e, ea);

	WL_WSEC(("wl%d: notify WAPID of WAI frame data len %d\n", wapi->pub->unit, len));
	wlc_process_event(wapi->wlc, e);
}
#endif /* !WLNOEIND */

void
wlc_wapi_station_event(wlc_wapi_info_t *wapi, wlc_bsscfg_t *bsscfg, const struct ether_addr *addr,
	void *ie, uint8 *gsn, uint16 msg_type)
{
	uint8 ie_len;
	struct wapi_sta_msg_t wapi_sta_msg;
	uint32 *dst_iv, *src_iv;
	int i, swap_len = IV_LEN / sizeof(uint32) - 1;

	memset(&wapi_sta_msg, 0, sizeof(struct wapi_sta_msg_t));

	switch (msg_type) {
	case WAPI_STA_AGING:
	case WAPI_UNICAST_REKEY:
	case WAPI_MUTIL_REKEY:
	case WAPI_STA_STATS:
		break;

	case WAPI_WAI_REQUEST:
		ASSERT(gsn);
		ASSERT(ie);
		dst_iv = (uint32 *)wapi_sta_msg.gsn;
		src_iv = (uint32 *)gsn;
		src_iv += swap_len;
		for (i = 0; i <= swap_len; i++) {
			memcpy(dst_iv, src_iv, sizeof(uint32));
			dst_iv++;
			src_iv--;
		}
		ie_len = ((uint8*)ie)[1] + 2; /* +2: wapi ie id+ wapi ie len */
		memcpy(wapi_sta_msg.wie, ie, ie_len);
		break;

	default:
		WL_ERROR(("wl%d: %s failed, unknown msg_type %d\n",
			wapi->pub->unit, __FUNCTION__, msg_type));
		return;
	}

	wapi_sta_msg.msg_type = msg_type;
	wapi_sta_msg.datalen = sizeof(struct wapi_sta_msg_t);
	memcpy(wapi_sta_msg.vap_mac, bsscfg->cur_etheraddr.octet, 6);
	memcpy(wapi_sta_msg.sta_mac, addr, 6);

	wlc_bss_mac_event(wapi->wlc, bsscfg, WLC_E_WAI_STA_EVENT, addr, 0, 0, 0,
		&wapi_sta_msg, wapi_sta_msg.datalen);

	return;
}

void
wlc_wapi_copy_info_elt(wlc_wapi_info_t *wapi, wlc_bsscfg_t *bsscfg, uint8 **pbody)
{
	wlc_wapi_bsscfg_cubby_priv_t *cfg_priv = WAPI_BSSCFG_PRIV(wapi, bsscfg);

	ASSERT(cfg_priv != NULL);

	if (cfg_priv->wapi_ie_len)
		*pbody = wlc_copy_info_elt(*pbody, cfg_priv->wapi_ie);
	else
		WL_ERROR(("wl%d: WAPI IE not set. Check if the wapi supplicant "
			"is running\n", wapi->pub->unit));
}

#ifdef AP
int
wlc_wapi_check_ie(wlc_wapi_info_t *wapi, wlc_bsscfg_t *bsscfg, bcm_tlv_t *wapiie, uint16 *auth,
	uint32 *wsec)
{
	uint8 len = wapiie->len;
	uint32 WPA_auth = bsscfg->WPA_auth;
	wpa_suite_mcast_t *mcast;
	wpa_suite_ucast_t *ucast;
	wpa_suite_auth_key_mgmt_t *mgmt;
	uint16 count;

	/* Check min length and version */
	if (len < WAPI_IE_MIN_LEN) {
		WL_ERROR(("wl%d: WAPI IE illegally short %d\n", wapi->pub->unit, len));
		return 1;
	}

	if ((len < WAPI_VERSION_LEN) ||
	    (ltoh16_ua(wapiie->data) != WPA2_VERSION)) {
		WL_ERROR(("wl%d: unsupported WAPI version %d\n", wapi->pub->unit,
			ltoh16_ua(wapiie->data)));
		return 1;
	}
	len -= WAPI_VERSION_LEN;

	/* Check the AKM */
	mgmt = (wpa_suite_auth_key_mgmt_t *)&wapiie->data[WAPI_VERSION_LEN];
	count = ltoh16_ua(&mgmt->count);
	if ((count != 1) ||
	    !((bcmp(mgmt->list[0].oui, WAPI_OUI, DOT11_OUI_LEN) == 0) &&
	      (((mgmt->list[0].type == RSN_AKM_UNSPECIFIED) &&
		(WPA_auth & WAPI_AUTH_UNSPECIFIED)) ||
	       ((mgmt->list[0].type == RSN_AKM_PSK) &&
		(WPA_auth & WAPI_AUTH_PSK))))) {
		WL_ERROR(("wl%d: bad AKM in WAPI IE.\n", wapi->pub->unit));
		return 1;
	}
	if (!bcmwpa_akm2WPAauth((uint8 *)&mgmt->list[0], &WPA_auth, FALSE)) {
		WL_ERROR(("wl%d: bcmwpa_akm2WPAauth: can't convert AKM %02x%02x%02x%02x.\n",
			wapi->pub->unit, mgmt->list[0].oui[0], mgmt->list[0].oui[1],
			mgmt->list[0].oui[2], mgmt->list[0].type));
		return 1;
	}
	len -= (WPA_IE_SUITE_COUNT_LEN + WPA_SUITE_LEN);
	*auth = (uint16)WPA_auth;

	/* Check the unicast cipher */
	ucast = (wpa_suite_ucast_t *)&mgmt->list[1];
	count = ltoh16_ua(&ucast->count);
	if (count != 1 ||
		bcmp(ucast->list[0].oui, WAPI_OUI, DOT11_OUI_LEN) ||
		!wpa_cipher_enabled(wapi->wlc, bsscfg,
		WAPI_CSE_WPI_2_CIPHER(ucast->list[0].type))) {
		WL_ERROR(("wl%d: bad unicast suite in WAPI IE.\n", wapi->pub->unit));
		return 1;
	}
	len -= (WPA_IE_SUITE_COUNT_LEN + WPA_SUITE_LEN);
	bcmwpa_cipher2wsec(ucast->list[0].oui, wsec);

	/* Check the mcast cipher */
	mcast = (wpa_suite_mcast_t *)&ucast->list[1];
	if (bcmp(mcast->oui, WAPI_OUI, DOT11_OUI_LEN) ||
		!wpa_cipher_enabled(wapi->wlc, bsscfg, WAPI_CSE_WPI_2_CIPHER(mcast->type))) {
		WL_ERROR(("wl%d: WAPI mcast cipher %02x:%02x:%02x:%d not enabled\n",
			wapi->pub->unit, mcast->oui[0], mcast->oui[1], mcast->oui[2],
			mcast->type));
		return 1;
	}
	len -= WPA_SUITE_LEN;

	/* Optional RSN capabilities */
	/* Reach this only if the IE looked okay.
	 * Note that capability bits of the IE have no use here yet.
	 */
	return 0;
}

void
wlc_wapi_wai_req_ind(wlc_wapi_info_t *wapi, wlc_bsscfg_t *bsscfg, struct scb *scb)
{
	int i;
	wsec_key_t *defkey = WSEC_BSS_DEFAULT_KEY(bsscfg);
	uint8 defPN[SMS4_WPI_PN_LEN], *gsn;

	if (defkey) {
		gsn = defkey->wapi_txiv.PN;
	} else {
		/* AP is AE and STA is ASUE */
		for (i = 0; i < SMS4_WPI_PN_LEN;) {
			defPN[i++] = 0x36;
			defPN[i++] = 0x5C;
		}
		defPN[0] = 0x37;
		defPN[0] = 0x37;
		gsn = defPN;
	}

	wlc_wapi_station_event(wapi, bsscfg, &scb->ea, scb->wpaie, gsn, WAPI_WAI_REQUEST);
}
#endif /* AP */
#endif /* BCMWAPI_WAI */

void
wlc_wapi_write_hw_mic_key(wlc_wapi_info_t *wapi, int indx, wsec_key_t *key)
{
	uint offset;

	ASSERT(WAPI_HW_WPI_ENAB(wapi->wlc));

	offset = indx * SMS4_WPI_CBC_MAC_LEN;
	WL_WSEC(("Updating the WAPI MIC key at offset 0x%02x, len %d\n",
		(wapi->wapimickeys + offset), SMS4_WPI_CBC_MAC_LEN));
	wlc_copyto_shm(wapi->wlc, wapi->wapimickeys + offset,
		key->data + SMS4_KEY_LEN, SMS4_WPI_CBC_MAC_LEN);
}

void
wlc_wapi_key_iv_init(wlc_wapi_info_t *wapi, wlc_bsscfg_t *cfg, wsec_key_t *key,
	wsec_iv_t *initial_iv)
{
	int i;

	bzero(&key->wapi_txiv, SMS4_WPI_IV_LEN);
	bzero(&key->wapi_rxiv, SMS4_WPI_IV_LEN);

	/* AP is AE and STA is ASUE */
	for (i = 0; i < SMS4_WPI_PN_LEN;) {
		key->wapi_txiv.PN[i++] = 0x36;
		key->wapi_txiv.PN[i++] = 0x5C;
	}
	for (i = 0; i < SMS4_WPI_PN_LEN;) {
		key->wapi_rxiv.PN[i++] = 0x36;
		key->wapi_rxiv.PN[i++] = 0x5C;
	}
	if (BSSCFG_AP(cfg)) {
		key->wapi_txiv.PN[0] = 0x37;
	}
	else if (cfg->BSS) {
		key->wapi_rxiv.PN[0] = 0x37;
	}
	else {
		int val;
		/* Compare the mac addresses to figure which one is AE which is ASE */
		val = bcm_cmp_bytes((uchar *)&key->ea, (uchar *)&cfg->cur_etheraddr,
		                    ETHER_ADDR_LEN);
		if (val >= 0)
			key->wapi_rxiv.PN[0] = 0x37;
		else
			key->wapi_txiv.PN[0] = 0x37;
	}
	key->wapi_txiv.key_idx = key->id;
	key->wapi_rxiv.key_idx = key->id;
}

bool
wlc_wapi_key_rotation_update(wlc_wapi_info_t *wapi, struct scb *scb, uint32 key_algo, uint32 key_id)
{
	wapi_scb_cubby_t *cubby = WAPI_SCB_CUBBY(wapi, scb);
	wsec_key_t *key = scb->key;

	if ((key->algo == key_algo) && (key->algo == CRYPTO_ALGO_SMS4)) {
		if (key->id != key_id) {
			/* save the key until AP starts using the new Key */
			if (cubby->prev_key)
				wlc_key_delete(wapi->wlc, scb->bsscfg, cubby->prev_key);

			cubby->prev_key_valid_time = wapi->pub->now + SMS4_OLD_KEY_MAXVALIDTIME;
			cubby->prev_key = scb->key;
			scb->key = NULL;
			return TRUE;
		}
	}
	return FALSE;
}

wsec_key_t *
wlc_wapi_scb_prev_key(wlc_wapi_info_t *wapi, struct scb *scb)
{
	wapi_scb_cubby_t *cubby = WAPI_SCB_CUBBY(wapi, scb);

	return cubby->prev_key;
}

/* This function is called in BCMFASTPATH only when key->algo == CRYPTO_ALGO_SMS4 */
wsec_key_t *
wlc_wapi_key_lookup(wlc_wapi_info_t *wapi, struct scb *scb, wsec_key_t *key, uint indx)
{
	wsec_key_t *valid_key = NULL;
	wapi_scb_cubby_t *cubby = WAPI_SCB_CUBBY(wapi, scb);

	if (key->id == indx) {
		if (cubby->prev_key) {
			wlc_key_delete(wapi->wlc, scb->bsscfg, cubby->prev_key);
			cubby->prev_key = NULL;
			cubby->prev_key_valid_time = 0;
		}
		valid_key = key;
	} else if (cubby->prev_key && (cubby->prev_key->id == indx)) {
		if (wapi->pub->now > cubby->prev_key_valid_time) {
			wlc_key_delete(wapi->wlc, scb->bsscfg, cubby->prev_key);
			cubby->prev_key = NULL;
			cubby->prev_key_valid_time = 0;
			WL_WSEC(("WAPI: prev key not Valid anymore %d\n", indx));
		} else
			valid_key = cubby->prev_key;
	} else {
		WL_WSEC(("WAPI: no Valid key for key_id %d\n", indx));
	}

	return valid_key;
}

void
wlc_wapi_key_delete(wlc_wapi_info_t *wapi, struct scb *scb)
{
	wapi_scb_cubby_t *cubby = WAPI_SCB_CUBBY(wapi, scb);

	ASSERT(cubby->prev_key->algo == CRYPTO_ALGO_SMS4);

	cubby->prev_key = NULL;
	cubby->prev_key_valid_time = 0;
}

void
wlc_wapi_key_scb_delete(wlc_wapi_info_t *wapi, struct scb *scb, wlc_bsscfg_t *bsscfg)
{
	int key_index;
	wapi_scb_cubby_t *cubby = WAPI_SCB_CUBBY(wapi, scb);
	wsec_key_t *key;
#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_WSEC */

	key = cubby->prev_key;
	cubby->prev_key = NULL;
	cubby->prev_key_valid_time = 0;
	if (key) {
		ASSERT(key->algo == CRYPTO_ALGO_SMS4);
		key_index = WSEC_KEY_INDEX(wapi->wlc, key);

		WL_WSEC(("wl%d: %s: deleting prev pairwise key %d for %s\n",
		         wapi->pub->unit, __FUNCTION__,
		         key_index, bcm_ether_ntoa(&scb->ea, eabuf)));
		/* note: the scb key pointer will be set to NULL by this function. */
		wlc_key_delete(wapi->wlc, scb->bsscfg, key);

		/* if we are freeing up a hardware key, look for a scb using a softkey
		 * to swap in to the hardware key slot
		 */
		if (key_index < (int)WLC_MAX_WSEC_HW_KEYS(wapi->wlc) &&
		    !WLC_SW_KEYS(wapi->wlc, bsscfg)) {
			wlc_key_hw_reallocate(wapi->wlc, key_index, SCB_BSSCFG(scb));
		}
	}
}

#ifdef BCMDBG
static int
wlc_wapi_dump(wlc_wapi_info_t *wapi, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "\n");

	bcm_bprintf(b, "wapi_hw_wpi %d\n", wapi->pub->_wapi_hw_wpi);

	return 0;
}

static void
wlc_wapi_scb_dump(void *context, struct scb *scb, struct bcmstrbuf *b)
{
	uint i;
	wlc_wapi_info_t *wapi = (wlc_wapi_info_t *)context;
	wapi_scb_cubby_t *cubby = WAPI_SCB_CUBBY(wapi, scb);
	wsec_key_t *prev_key;

	ASSERT(cubby != NULL);

	prev_key = cubby->prev_key;

	if (!prev_key || prev_key->algo != CRYPTO_ALGO_SMS4)
		return;

	bcm_bprintf(b, "     WPI prev_key: ");

	for (i = 0; i < prev_key->len; i ++)
		bcm_bprintf(b, "%02X", prev_key->data[i]);

	if (wapi->pub->now > cubby->prev_key_valid_time)
		bcm_bprintf(b, " Expired!");

	bcm_bprintf(b, "\n");
}

#ifdef BCMWAPI_WAI
static void
wlc_wapi_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_wapi_info_t *wapi = (wlc_wapi_info_t *)context;
	wlc_wapi_bsscfg_cubby_pub_t *cfg_pub = WAPI_BSSCFG_PUB(wapi->wlc, cfg);
	wlc_wapi_bsscfg_cubby_priv_t *cfg_priv = WAPI_BSSCFG_PRIV(wapi, cfg);
	bcm_tlv_t *ie;

	ASSERT(cfg_pub != NULL);
	ASSERT(cfg_priv != NULL);

	bcm_bprintf(b, "wai_restrict: %d wai_preauth: %d\n",
	            cfg_pub->wai_restrict, cfg_priv->wai_preauth);

	if ((ie = (bcm_tlv_t *)cfg_priv->wapi_ie) != NULL) {
		int remain = cfg_priv->wapi_ie_len;

		bcm_bprintf(b, "  Assoc Req IEs:\n");
		while (remain >= TLV_HDR_LEN) {
			int ie_len = ie->len + TLV_HDR_LEN;

			if (ie_len <= remain) {
				bcm_bprintf(b, "    ");
				wlc_dump_ie(wapi->wlc, ie, b);
				bcm_bprintf(b, "\n");
			}

			ie = (bcm_tlv_t *)((uint8 *)ie + ie_len);

			remain -= ie_len;
		}
	}
}
#endif /* BCMWAPI_WAI */
#endif /* BCMDBG */
