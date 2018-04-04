/*
 * DLS source file
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
#ifdef WLDLS
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
#include <wlc_scb.h>
#include <wlc_scan.h>
#include <wl_dbg.h>
#include <wlc_alloc.h>
#include <wlc_dls.h>

/* iovar table */
enum {
	IOV_DLS,	/* enable/disable DLS */
	IOV_DLS_REJECT,	/* AP don't allow DLS in the BSS by policy */
	IOV_LAST
};

static const bcm_iovar_t dls_iovars[] = {
	{"dls", IOV_DLS, (0), IOVT_BOOL, 0},
	{"dls_reject", IOV_DLS_REJECT, (0), IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
};

/* DLS module info */
struct dls_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	bool reject_dls;
};

/* local functions */
/* module */
static int wlc_dls_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);

dls_info_t *
BCMATTACHFN(wlc_dls_attach)(wlc_info_t *wlc)
{
	dls_info_t *dls;

	if (!wlc)
		return NULL;

	if ((dls = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(dls_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	dls->wlc = wlc;

	/* keep the module registration the last other add module unregistration
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, dls_iovars, "dls", dls, wlc_dls_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	wlc->pub->_dls = TRUE;
	return dls;

	/* error handling */
fail:
	if (dls != NULL)
		MFREE(wlc->osh, dls, sizeof(dls_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_dls_detach)(dls_info_t *dls)
{
	wlc_info_t *wlc;

	if (dls) {
		wlc = dls->wlc;
		wlc_module_unregister(wlc->pub, "dls", dls);
		MFREE(wlc->osh, dls, sizeof(dls_info_t));
	}
}

static int
wlc_dls_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	dls_info_t *dls = (dls_info_t *)ctx;
	wlc_info_t *wlc = dls->wlc;
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

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_DLS):
		*ret_int_ptr = (int32)wlc->pub->_dls;
		break;
	case IOV_SVAL(IOV_DLS):
		wlc->pub->_dls = bool_val;
		break;
	case IOV_GVAL(IOV_DLS_REJECT):
		*ret_int_ptr = (int32)dls->reject_dls;
		break;
	case IOV_SVAL(IOV_DLS_REJECT):
		dls->reject_dls = bool_val;
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static void
wlc_dls_send_response(dls_info_t *dls, struct ether_addr *da,
	wlc_bsscfg_t *bsscfg, uint8 *body, int body_len)
{
	wlc_info_t *wlc;
	dot11_dls_req_t *dlsreq;
	dot11_dls_resp_t *dlsresp;
	void *p;
	uint8* pbody;
	int len;

	if (body == NULL)
		return;

	wlc = dls->wlc;

	if (dls->reject_dls && BSSCFG_AP(bsscfg)) {
		dlsreq = (dot11_dls_req_t *)body;
		len = DOT11_DLS_RESP_LEN;
		p = wlc_frame_get_mgmt(wlc, FC_ACTION, da, &bsscfg->cur_etheraddr, &bsscfg->BSSID,
		                       len, &pbody);
		if (p == NULL) {
			WL_ERROR(("wl%d: %s: no memory for DLS response\n",
			           wlc->pub->unit, __FUNCTION__));
			return;
		}

		dlsresp = (dot11_dls_resp_t *)pbody;
		dlsresp->category = DOT11_ACTION_CAT_DLS;
		dlsresp->action = DOT11_DLS_ACTION_RESP;
		dlsresp->status = htol16((uint16)DOT11_SC_DLS_NOT_ALLOWED);
		/* copy dest and src mac address from DLS request frame */
		bcopy(&dlsreq->da, &dlsresp->da, ETHER_ADDR_LEN + ETHER_ADDR_LEN);

		PKTSETLEN(wlc->osh, p, len + DOT11_MGMT_HDR_LEN);
		wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, NULL);
	}
}

void wlc_dls_recv_process_dls(dls_info_t *dls,
	uint action_id, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;

	if (dls == NULL)
		return;

	wlc = dls->wlc;

	bsscfg = wlc_bsscfg_find_by_hwaddr(wlc, &hdr->da);
	if (bsscfg == NULL)
		return;

	switch (action_id) {
	case DOT11_DLS_ACTION_REQ:
		if (body_len < DOT11_DLS_REQ_LEN)
			return;
		wlc_dls_send_response(dls, &hdr->sa, bsscfg, body, body_len);
		break;
	case DOT11_DLS_ACTION_RESP:
		break;
	case DOT11_DLS_ACTION_TD:
		break;
	default:
		break;
	}
}
#endif /* WLDLS */
