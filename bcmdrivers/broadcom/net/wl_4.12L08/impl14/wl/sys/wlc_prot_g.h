/*
 * 11g protection module APIs
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

#ifndef _wlc_prot_g_h_
#define _wlc_prot_g_h_

/* data APIs */
/* module public states - read only */
struct wlc_prot_g_info {
	int cfgh;
};
#define WLC_PROT_G_INFO_CFGH(prot)	((prot)->cfgh)

/* bsscfg specific states - read only */
typedef	struct {
	bool _g;
} wlc_prot_g_cfg_t;
#define WLC_PROT_G_CFG(prot, cfg)	(*(wlc_prot_g_cfg_t **) \
					 BSSCFG_CUBBY(cfg, WLC_PROT_G_INFO_CFGH(prot)))
#define WLC_PROT_G_CFG_G(prot, cfg)	(WLC_PROT_G_CFG(prot, cfg)->_g)

/* function APIs */
/* module entries */
extern wlc_prot_g_info_t *wlc_prot_g_attach(wlc_info_t *wlc);
extern void wlc_prot_g_detach(wlc_prot_g_info_t *prot);

/* configuration change */
extern void wlc_prot_g_cfg_set(wlc_prot_g_info_t *prot, uint idx, int val);
/* wlc_prot_g_cfg_set() idx */
#define	WLC_PROT_G_USER		1	/* gmode specified by user */
extern void wlc_prot_g_cfg_track(wlc_prot_g_info_t *prot, wlc_bsscfg_t *cfg,
	uint8 *erp, int erp_len);
extern void wlc_prot_g_cfg_init(wlc_prot_g_info_t *prot, wlc_bsscfg_t *cfg);

/* condition change */
extern void wlc_prot_g_cond_upd(wlc_prot_g_info_t *prot, struct scb *scb);

/* timeout change */
extern void wlc_prot_g_ovlp_upd(wlc_prot_g_info_t *prot, chanspec_t chspec,
	uint8 *erp, int erp_len, bool is_erp,
	bool short_cap);
extern void wlc_prot_g_to_upd(wlc_prot_g_info_t *prot, wlc_bsscfg_t *cfg,
	uint8 *erp, int erp_len, bool is_erp, bool shortpreamble);

/* gmode change */
extern void wlc_prot_g_mode_reset(wlc_prot_g_info_t *prot);
extern bool wlc_prot_g_mode_upd(wlc_prot_g_info_t *prot, wlc_bsscfg_t *cfg);

/* IE change */
extern uint8 *wlc_prot_g_write_erp_ie(wlc_prot_g_info_t *prot, wlc_bsscfg_t *cfg,
	uint8 *buf, int buflen);

/* protection init */
extern void wlc_prot_g_init(wlc_prot_g_info_t *prot, wlc_bsscfg_t *cfg);

#endif /* _wlc_prot_g_h_ */
