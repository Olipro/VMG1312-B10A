/*
 * WAPI (WLAN Authentication and Privacy Infrastructure) private header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: $
 */
#ifndef _wlc_wapi_priv_h_
#define _wlc_wapi_priv_h_

struct wlc_wapi_info {
	wlc_info_t *wlc;
	wlc_pub_t *pub;
	int cfgh;			/* bsscfg cubby handle to retrieve data from bsscfg */
	uint priv_offset;		/* offset of private bsscfg cubby */
	int scbh;			/* scb cubby handle to retrieve data from scb */

	uint wapimickeys;		/* 8 WAPI MIC key table shm address */
};

/* WPI scb cubby */
typedef struct wapi_scb_cubby {
	wsec_key_t *prev_key;	/* to support key rotation per station */
	uint32 prev_key_valid_time;
} wapi_scb_cubby_t;

#define WAPI_SCB_CUBBY(wapi, scb) ((wapi_scb_cubby_t *)SCB_CUBBY(scb, (wapi)->scbh))

#ifdef BCMWAPI_WAI
/* WAPI private bsscfg cubby structure and access macro */
typedef struct wlc_wapi_bsscfg_cubby_priv {
	/* non-critical data path security variables */
	bool	wai_preauth;	/* default is FALSE */
	uint8	*wapi_ie;	/* user plumbed wapi_ie */
	int	wapi_ie_len;	/* wapi_ie len */
} wlc_wapi_bsscfg_cubby_priv_t;


#define WAPI_BSSCFG_PRIV(wapi, cfg) \
	((wlc_wapi_bsscfg_cubby_priv_t *)BSSCFG_CUBBY((cfg), \
	((wapi)->cfgh + (wapi)->priv_offset)))
#endif /* BCMWAPI_WAI */

#endif /* _wlc_wapi_priv_h_ */
