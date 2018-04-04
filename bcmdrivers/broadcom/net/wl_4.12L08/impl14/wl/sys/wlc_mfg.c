/*
 * Manufacturing Test Module
 * 
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_mfg.c,v 1.5.2.1 2010-02-10 04:09:21 Exp $
 */
#ifndef WLTEST
#error "WLTEST must be defined to include this module"
#endif  /* WLTEST */

#ifndef WLMFG
#error "WLMFG is not defined"
#endif /* WLMFG */

#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wl_dbg.h>
#include <wlc_cfg.h>
#include <wlc_pub.h>
#include <bcmnvram.h>
#include <bcmotp.h>
#include <proto/802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_mfg.h>

struct mfg_info {
	wlc_info_t	*wlc;
	wlc_pub_t	*pub;
};

enum {
	IOV_MFG_TEST,
	IOV_MFG_LAST
};

static const bcm_iovar_t mfg_iovars[] = {
	{"mfgtest1", IOV_MFG_TEST,
	(IOVF_MFG), IOVT_UINT32, 0
	},
	{NULL, 0, 0, 0, 0 }
};

/* Local functions */
static int wlc_mfg_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);

mfg_info_t *
BCMATTACHFN(wlc_mfg_attach)(wlc_pub_t *pub, wlc_info_t *wlc)
{
	mfg_info_t *info;

	WL_TRACE(("wl: %s\n", __FUNCTION__));

	if ((info = (mfg_info_t *)MALLOC(pub->osh, sizeof(mfg_info_t))) == NULL) {
		WL_ERROR(("wlc_mfg_attach: out of memory, malloced %d bytes", MALLOCED(pub->osh)));
		goto fail;
	}
	bzero((char *)info, sizeof(mfg_info_t));
	info->wlc = wlc;
	info->pub = pub;

	/* register module */
	if (wlc_module_register(pub, mfg_iovars, "mfg", info, wlc_mfg_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: mfg wlc_module_register() failed\n", pub->unit));
		goto fail;
	}

	return info;

fail:
	if (info) {
		MFREE(info->pub->osh, info, sizeof(mfg_info_t));
	}
	return NULL;
}

int
BCMATTACHFN(wlc_mfg_detach)(mfg_info_t *info)
{
	WL_TRACE(("wl: %s\n", __FUNCTION__));

	if (info == NULL)
		return BCME_ERROR;

	wlc_module_unregister(info->pub, "mfg", info);

	MFREE(info->pub->osh, info, sizeof(mfg_info_t));

	return BCME_OK;
}

void
BCMINITFN(wlc_mfg_init)(mfg_info_t *info)
{
}

void
BCMUNINITFN(wlc_mfg_deinit)(mfg_info_t *info)
{
}

void
wlc_mfg_up(mfg_info_t *info)
{
}

int
wlc_mfg_down(mfg_info_t *info)
{
	return 0;
}

static int
wlc_mfg_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	int err = 0;
	int32 int_val;
	int32 *ret_int_ptr = (int32 *)arg;

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	switch (actionid) {
	case IOV_GVAL(IOV_MFG_TEST):
	{
		*ret_int_ptr = 0;
		break;
	}
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

#if (defined(BCMROMMFG) && defined(BCMROMBUILD))
/* For ROM build, WLTEST enabled; this RAM function
 * enables WLTEST ioctl functionality;
 * Do not ROM this function
 */
int
wlc_mfg_ioctl_filter(wlc_pub_t *pub, int cmd)
{
	int err = BCME_OK;

	/* These ioctls are extracted from all .c files with
	 * WLTEST encapsulated
	 */
	switch (cmd) {
		case WLC_GET_PHYREG:
		case WLC_SET_PHYREG:
		case WLC_GET_TSSI:
		case WLC_GET_ATTEN:
		case WLC_SET_ATTEN:
		case WLC_LONGTRAIN:
		case WLC_EVM:
		case WLC_FREQ_ACCURACY:
		case WLC_CARRIER_SUPPRESS:
		case WLC_GET_PWRIDX:
		case WLC_SET_PWRIDX:
		case WLC_GET_MSGLEVEL:
		case WLC_SET_MSGLEVEL:
		case WLC_GET_UCANTDIV:
		case WLC_SET_UCANTDIV:
		case WLC_SET_SROM:
		case WLC_NVRAM_GET:
		case WLC_CURRENT_PWR:
		case WLC_OTPW:
		case WLC_NVOTPW:
#if defined(BCMROMBUILD)
			err = BCME_UNSUPPORTED;
#endif
			break;
		default:
			break;
	}

	return err;
}

/* For ROM build, WLTEST enabled; this RAM function
 * enables WLTEST iovar functionality;
 * Do not ROM this function
 */
int
wlc_mfg_iovar_filter(wlc_pub_t *pub, const bcm_iovar_t *vi)
{
	int err = BCME_OK;

	if (vi->flags & IOVF_MFG) {
#if defined(BCMROMBUILD)
		err = BCME_UNSUPPORTED;
#endif
	}

	return err;
}
#endif 
