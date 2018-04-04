/*
 * 802.11u module source file (interworking protocol)
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

#ifdef WL11U

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
#include <wlc_scb.h>
#include <wlc_tpc.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wlc_11u.h>
#include <wlc_probresp.h>
#include "proto/802.11.h"

/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_INTERWORKING, /* 802.11u enable/dsiable */
	IOV_LAST
};

const bcm_iovar_t wlc_11u_iovars[] = {
	{"interworking", IOV_INTERWORKING, (0), IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
};

/* 11h module info */
struct wlc_11u_info {
	wlc_info_t *wlc;
	int cfgh;			/* bsscfg cubby handle */
};

/* local functions */
/* module */
static int wlc_11u_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);

/* cubby */
static int wlc_11u_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_11u_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg);
#ifdef BCMDBG
static void wlc_11u_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_11u_bsscfg_dump NULL
#endif

typedef struct {
	uint8		*iw_ie; /* 802.11u interworking(IW) IE */
} wlc_11u_bsscfg_cubby_t;
#define WLC_11U_BSSCFG_CUBBY(m11u, cfg) \
	((wlc_11u_bsscfg_cubby_t *)BSSCFG_CUBBY((cfg), (m11u)->cfgh))

wlc_11u_info_t *
BCMATTACHFN(wlc_11u_attach)(wlc_info_t *wlc)
{
	wlc_11u_info_t *m11u;

	if (!wlc)
		return NULL;

	if ((m11u = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_11u_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	m11u->wlc = wlc;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((m11u->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(wlc_11u_bsscfg_cubby_t),
		wlc_11u_bsscfg_init, wlc_11u_bsscfg_deinit, wlc_11u_bsscfg_dump,
		m11u)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* keep the module registration the last other add module unregistration
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, wlc_11u_iovars, "11u", m11u, wlc_11u_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	wlc->pub->_11u = TRUE;
	wlc_probresp_register(wlc->mprobresp, m11u, wlc_11u_check_probe_req_iw, FALSE);
	return m11u;

	/* error handling */
fail:
	if (m11u != NULL)
		MFREE(wlc->osh, m11u, sizeof(wlc_11u_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_11u_detach)(wlc_11u_info_t *m11u)
{
	wlc_info_t *wlc;

	if (m11u) {
		wlc = m11u->wlc;
		wlc_probresp_unregister(wlc->mprobresp, m11u);
		wlc_module_unregister(wlc->pub, "11u", m11u);

		MFREE(wlc->osh, m11u, sizeof(wlc_11u_info_t));
	}
}

static int
wlc_11u_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_11u_info_t *m11u = (wlc_11u_info_t *)ctx;
	wlc_info_t *wlc = m11u->wlc;
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
	case IOV_GVAL(IOV_INTERWORKING):
		*ret_int_ptr = (int32)wlc->pub->_11u;
		break;
	case IOV_SVAL(IOV_INTERWORKING):
		if (wlc->pub->_11u != bool_val) {
			wlc_11u_bsscfg_cubby_t *cubby_11u;
			wlc_bsscfg_t *cfg;
			int idx;
			wlc->pub->_11u = bool_val;

			FOREACH_BSS(wlc, idx, cfg) {
				cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);
				if (cubby_11u->iw_ie != NULL) {
					/* update extend capabilities */
					wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_IW,
						bool_val);
					if (cfg->up &&
					    (BSSCFG_AP(cfg) ||
					    (!cfg->BSS && !BSS_TDLS_ENAB(wlc, cfg)))) {
						/* update AP or IBSS beacons */
						wlc_bss_update_beacon(wlc, cfg);
						/* update AP or IBSS probe responses */
						wlc_bss_update_probe_resp(wlc, cfg, TRUE);
					}
				}
			}
		}
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}


/* bsscfg cubby */
static int
wlc_11u_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	return BCME_OK;
}

static void
wlc_11u_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
}

#ifdef BCMDBG
static void
wlc_11u_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_11u_info_t *m11u = (wlc_11u_info_t *)ctx;
	wlc_11u_bsscfg_cubby_t *cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);
	int i;
	ASSERT(cubby_11u != NULL);

	if (cubby_11u->iw_ie) {
		bcm_bprintf(b, "IW IE len: %d\n", cubby_11u->iw_ie[1]);
		for (i = 0; i < cubby_11u->iw_ie[1]; i++) {
			bcm_bprintf(b, "IW data[%d]: 0x%x\n", i, cubby_11u->iw_ie[i]);
		}
	}
}
#endif /* BCMDBG */

/* check interworking IE in probe request */
bool
wlc_11u_check_probe_req_iw(void *handle, wlc_bsscfg_t *cfg,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, struct dot11_management_header *hdr,
	uint8 *body, int body_len, bool *psendProbeResp)
{
	wlc_11u_info_t *m11u = (wlc_11u_info_t *)handle;
	wlc_11u_bsscfg_cubby_t *cubby_11u;
	wlc_info_t *wlc;
	bcm_tlv_t *iw;
	bool sendProbeResp = TRUE;
	uint8 ap_iw_len;

	if (!m11u)
		return TRUE;

	wlc = m11u->wlc;
	if (!WL11U_ENAB(wlc))
		return TRUE;

	cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);

	if (cubby_11u->iw_ie == NULL)
		return sendProbeResp;

	iw = bcm_parse_tlvs(body, body_len, DOT11_MNG_INTERWORKING_ID);
	ap_iw_len = cubby_11u->iw_ie[TLV_LEN_OFF];
	if (iw && iw->len && ap_iw_len) {
		uint8 sta_ant = (iw->data[IW_ANT_OFFSET] & IW_ANT_MASK);
		uint8 ap_ant = (cubby_11u->iw_ie[TLV_HDR_LEN+IW_ANT_OFFSET] & IW_ANT_MASK);
		if ((sta_ant != IW_ANT_WILDCARD_NETWORK) && (ap_ant != IW_ANT_WILDCARD_NETWORK) &&
		            (sta_ant != ap_ant)) {
			sendProbeResp = FALSE;
		} else if ((iw->len > ETHER_ADDR_LEN) && (ap_iw_len > ETHER_ADDR_LEN)) {
			uint8 *hessid = (iw->len >= IW_LEN) ?
				(&iw->data[IW_HESSID_OFFSET]) : (&iw->data[SHORT_IW_HESSID_OFFSET]);
			uint8 *hessid_ap = (ap_iw_len >= IW_LEN) ?
				(&cubby_11u->iw_ie[TLV_HDR_LEN+IW_HESSID_OFFSET]) :
				(&cubby_11u->iw_ie[TLV_HDR_LEN+SHORT_IW_HESSID_OFFSET]);
			if ((!ETHER_ISBCAST(hessid)) && (!ETHER_ISBCAST(hessid_ap)) &&
				bcmp(hessid, hessid_ap, ETHER_ADDR_LEN))
					sendProbeResp = FALSE;
		}
	}
	return sendProbeResp;
}

/* whether interworking service Activated */
bool
wlc_11u_iw_activated(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg)
{
	wlc_11u_bsscfg_cubby_t *cubby_11u;
	wlc_info_t *wlc;
	bool result = FALSE;

	if (m11u) {
		wlc = m11u->wlc;
		if (WL11U_ENAB(wlc)) {
			cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);
			ASSERT(cubby_11u);
			result = (cubby_11u->iw_ie != NULL);
		}
	}

	return result;
}

/* get 802.11u IE */
uint8 *
wlc_11u_get_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 ie_type)
{
	uint8 *ie_data = NULL;
	wlc_11u_bsscfg_cubby_t *cubby_11u;
	wlc_info_t *wlc;

	if (!m11u)
		return NULL;

	wlc = m11u->wlc;
	if (!WL11U_ENAB(wlc))
		return NULL;

	cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);

	switch (ie_type) {
		case DOT11_MNG_INTERWORKING_ID:
			ie_data = cubby_11u->iw_ie;
			break;
		default:
			ie_data = wlc_vndr_ie_find_by_type(cfg, ie_type);
			break;
	}
	return ie_data;
}

/* set 802.11u IE */
int
wlc_11u_set_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 *ie_data,
	bool *bcn_upd, bool *prbresp_upd)
{
	wlc_11u_bsscfg_cubby_t *cubby_11u;
	wlc_info_t *wlc;
	int err = BCME_OK;
	int ie_len;
	uint8 ie_type;

	if (!m11u)
		return BCME_UNSUPPORTED;

	wlc = m11u->wlc;
	if (!WL11U_ENAB(wlc))
		return BCME_UNSUPPORTED;

	cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);

	ie_type = ie_data[TLV_TAG_OFF];
	ie_len = ie_data[TLV_LEN_OFF] + TLV_HDR_LEN;

	switch (ie_type) {
		case DOT11_MNG_INTERWORKING_ID:
			if (BSSCFG_AP(cfg)) {
				*bcn_upd = TRUE;
				*prbresp_upd = TRUE;
			}
			break;
#ifdef AP
		case DOT11_MNG_ADVERTISEMENT_ID:
		case DOT11_MNG_ROAM_CONSORT_ID:
			if ((cubby_11u->iw_ie != NULL) && BSSCFG_AP(cfg)) {
				*bcn_upd = TRUE;
				*prbresp_upd = TRUE;
			}
			break;
#endif /* AP */
		default:
			err = BCME_UNSUPPORTED;
			return err;
	}

	if (ie_len == TLV_HDR_LEN) {
		/* delete the IE if len is zero */
		wlc_vndr_ie_del_by_type(cfg, ie_type);
	} else {
		/* update the IE */
		err = wlc_vndr_ie_mod_elem_by_type(cfg, ie_type,
			VNDR_IE_CUSTOM_FLAG, (vndr_ie_t *)ie_data);
	}

	/* update the pointer to the TLV field in the list for quick access */
	switch (ie_type) {
		case DOT11_MNG_INTERWORKING_ID:
			cubby_11u->iw_ie = wlc_vndr_ie_find_by_type(cfg,
				DOT11_MNG_INTERWORKING_ID);
			wlc_bsscfg_set_ext_cap(cfg, DOT11_EXT_CAP_IW,
				(cubby_11u->iw_ie != NULL));
			break;
		default:
			break;
	}

	return err;
}

/* write 802.11u IW, IWAP and IWRC IEs to beacon and probe response frame */
uint8 *
wlc_11u_write_ie_beacon(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 *cp, uint8 *bufend)
{
	wlc_11u_bsscfg_cubby_t *cubby_11u;
	wlc_info_t *wlc;

	if (!m11u)
		return cp;

	wlc = m11u->wlc;
	if (!WL11U_ENAB(wlc))
		return cp;

	cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);

	if (cubby_11u->iw_ie) {
#ifdef AP
		uint8 *iwap_ie = wlc_vndr_ie_find_by_type(cfg,
			DOT11_MNG_ADVERTISEMENT_ID);
		uint8 *iwrc_ie = wlc_vndr_ie_find_by_type(cfg,
			DOT11_MNG_ROAM_CONSORT_ID);
#endif /* AP */
		/* 11u interworking */
		cp = wlc_copy_info_elt_safe(cp, BUFLEN(cp, bufend), cubby_11u->iw_ie);
#ifdef AP
		/* 11u advertisement protocol */
		if (iwap_ie != NULL) {
			if (iwap_ie[TLV_BODY_OFF+1] != DOT11_MNG_PROPR_ID) {
				cp = wlc_copy_info_elt_safe(cp, BUFLEN(cp, bufend), iwap_ie);
			} else {
				uint8 *iwap_ie_dst = cp;
				cp[TLV_TAG_OFF] = DOT11_MNG_ADVERTISEMENT_ID;
				cp[TLV_BODY_OFF] = iwap_ie[TLV_BODY_OFF];
				cp += (TLV_BODY_OFF+1);
				cp = wlc_vndr_ie_write(cfg, cp, BUFLEN(cp, bufend),
					VNDR_IE_IWAPID_FLAG);
				iwap_ie_dst[TLV_LEN_OFF] = cp - iwap_ie_dst - TLV_HDR_LEN;
				if (iwap_ie_dst[TLV_LEN_OFF] == IWAP_QUERY_INFO_SIZE) {
					/* if no matched vendor IE,
					 * discard this advertisement protocol IE
					 */
					cp = iwap_ie_dst;
					WL_ERROR(("wl%d: %s: discard IW AP IE\n",
						wlc->pub->unit, __FUNCTION__));
				}
			}
		}
		/* 11u IW roaming consortium IE */
		if (iwrc_ie != NULL) {
			cp = wlc_copy_info_elt_safe(cp, BUFLEN(cp, bufend), iwrc_ie);
		}
#endif /* AP */
	}
	return cp;
}

/* write 802.11u IW IE */
uint8 *
wlc_11u_write_iw_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 *pbody)
{
	wlc_11u_bsscfg_cubby_t *cubby_11u;
	wlc_info_t *wlc;

	if (m11u) {
		wlc = m11u->wlc;
		if (WL11U_ENAB(wlc)) {
			cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);
			if (cubby_11u->iw_ie) {
				/* 11u interworking */
				pbody = wlc_copy_info_elt(pbody, cubby_11u->iw_ie);
			}
		}
	}

	return pbody;
}

/* get IW IE total length */
int
wlc_11u_iw_ie_len(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg)
{
	wlc_11u_bsscfg_cubby_t *cubby_11u;
	wlc_info_t *wlc;
	int ie_len = 0;

	if (!m11u)
		return 0;

	wlc = m11u->wlc;
	if (!WL11U_ENAB(wlc))
		return 0;

	cubby_11u = WLC_11U_BSSCFG_CUBBY(m11u, cfg);

	if (cubby_11u->iw_ie)
		ie_len = cubby_11u->iw_ie[TLV_LEN_OFF] + TLV_HDR_LEN;
	return ie_len;
}

/* whether it is 11u IE */
bool
wlc_11u_is_11u_ie(wlc_11u_info_t *m11u, uint8 ie_type)
{
	bool result;
	switch (ie_type) {
		case DOT11_MNG_INTERWORKING_ID:
		case DOT11_MNG_ADVERTISEMENT_ID:
		case DOT11_MNG_ROAM_CONSORT_ID:
			result = TRUE;
			break;
		default:
			result = FALSE;
			break;
	}
	return result;
}

#endif /* WL11U */
