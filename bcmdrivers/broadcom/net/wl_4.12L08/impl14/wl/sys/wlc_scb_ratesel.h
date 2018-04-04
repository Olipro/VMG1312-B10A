/*
 * Wrapper to scb rate selection algorithm of Broadcom
 * algorithm of Broadcom 802.11b DCF-only Networking Adapter.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 *
 * $Id: wlc_scb_ratesel.h 378413 2013-01-11 19:11:43Z $
 */

#ifndef	_WLC_SCB_RATESEL_H_
#define	_WLC_SCB_RATESEL_H_

#include <wlc_rate_sel.h>

#define WME_MAX_AC(wlc, scb) ((WME_PER_AC_MAXRATE_ENAB((wlc)->pub) && SCB_WME(scb)) ? \
				AC_COUNT : 1)

extern wlc_ratesel_info_t *wlc_scb_ratesel_attach(wlc_info_t *wlc);
extern void wlc_scb_ratesel_detach(wlc_ratesel_info_t *wrsi);


/* get primary rate */
extern ratespec_t wlc_scb_ratesel_get_primary(wlc_info_t *wlc, struct scb *scb, void *p);

/* select transmit rate given per-scb state */
extern void wlc_scb_ratesel_gettxrate(wlc_ratesel_info_t *wrsi, struct scb *scb,
	uint16 *frameid, ratesel_txparams_t *cur_rate, uint16 *flags);

/* update per-scb state upon received tx status */
extern void wlc_scb_ratesel_upd_txstatus_normalack(wlc_ratesel_info_t *wrsi, struct scb *scb,
	tx_status_t *txs, uint16 sfbl, uint16 lfbl,
	uint8 mcs, uint8 antselid, bool fbr);

#ifdef WL11N
/* change the throughput-based algo parameters upon ACI mitigation state change */
extern void wlc_scb_ratesel_aci_change(wlc_ratesel_info_t *wrsi, bool aci_state);

/* update per-scb state upon received tx status for ampdu */
extern void wlc_scb_ratesel_upd_txs_blockack(wlc_ratesel_info_t *wrsi, struct scb *scb,
	tx_status_t *txs, uint8 suc_mpdu, uint8 tot_mpdu,
	bool ba_lost, uint8 retry, uint8 fb_lim, bool tx_error,
	uint8 mcs, uint8 antselid);

#ifdef WLAMPDU_MAC
extern void wlc_scb_ratesel_upd_txs_ampdu(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	uint8 mrt, uint8 mrt_succ, uint8 fbr, uint8 fbr_succ,
	bool tx_error, uint8 tx_mcs, uint8 antselid);
#endif

/* update rate_sel if a PPDU (ampdu or a reg pkt) is created with probe values */
extern void wlc_scb_ratesel_probe_ready(wlc_ratesel_info_t *wrsi, struct scb *scb,
	uint16 frameid, bool is_ampdu, uint8 ampdu_txretry);

extern void wlc_scb_ratesel_upd_rxstats(wlc_ratesel_info_t *wrsi, ratespec_t rx_rspec,
	uint16 rxstatus2);

/* get the fallback rate of the specified mcs rate */
extern ratespec_t wlc_scb_ratesel_getmcsfbr(wlc_ratesel_info_t *wrsi, struct scb *scb,
	uint16 frameid, uint8 mcs);
#endif /* WL11N */

extern bool wlc_scb_ratesel_minrate(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs);

extern void wlc_scb_ratesel_init(wlc_info_t *wlc, struct scb *scb);
extern void wlc_scb_ratesel_init_all(struct wlc_info *wlc);
extern void wlc_scb_ratesel_init_bss(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_scb_ratesel_clr_gotpkts(wlc_ratesel_info_t *wrsi, struct scb *scb,
	tx_status_t *txs);

#ifdef WL_LPC
/* External functions used by wlc_power_sel.c */
void wlc_scb_ratesel_get_info(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	uint8 rate_stab_thresh, uint32 *new_rate_kbps, bool *rate_stable,
	rate_lcb_info_t *lcb_info);
void wlc_scb_ratesel_reset_vals(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac);
void wlc_scb_ratesel_clr_cache(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid);
#endif /* WL_LPC */
#endif	/* _WLC_SCB_RATESEL_H_ */
