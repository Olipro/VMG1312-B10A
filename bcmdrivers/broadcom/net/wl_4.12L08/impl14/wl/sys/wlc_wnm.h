/*
 * 802.11v definitions for
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_wnm.h 365273 2012-10-29 01:32:40Z $
 */

#ifndef _wlc_wnm_h_
#define _wlc_wnm_h_
#define WNM_BSSTRANS_ENABLED(mask)		((mask & WL_WNM_BSSTRANS)? TRUE: FALSE)
#define WNM_PROXYARP_ENABLED(mask)		((mask & WL_WNM_PROXYARP)? TRUE: FALSE)
#define WNM_TIM_BCAST_ENABLED(mask)		((mask & WL_WNM_TIM_BCAST)? TRUE: FALSE)
#define WNM_BSS_MAX_IDLE_PERIOD_ENABLED(mask)	((mask & WL_WNM_BSS_MAX_IDLE_PERIOD)? TRUE: FALSE)
#define WNM_TFS_ENABLED(mask)			((mask & WL_WNM_TFS)? TRUE: FALSE)
#define WNM_WNM_SLEEP_ENABLED(mask)		((mask & WL_WNM_SLEEP)? TRUE: FALSE)
#define WNM_DMS_ENABLED(mask)			((mask & WL_WNM_DMS)? TRUE: FALSE)
#define WNM_FMS_ENABLED(mask)			((mask & WL_WNM_FMS)? TRUE: FALSE)
#define WNM_NOTIF_ENABLED(mask)			((mask & WL_WNM_NOTIF)? TRUE: FALSE)

extern wlc_wnm_info_t *wlc_wnm_attach(wlc_info_t *wlc);
extern void wlc_wnm_detach(wlc_wnm_info_t *wnm_info);
extern void wlc_frameaction_wnm(wlc_wnm_info_t *wnm_info, uint action_id,
	struct dot11_management_header *hdr, uint8 *body, int body_len,
	int8 rssi, ratespec_t rspec);
extern void wlc_wnm_recv_process_wnm(wlc_wnm_info_t *wnm_info, wlc_bsscfg_t *bsscfg,
	uint action_id, struct scb *scb, struct dot11_management_header *hdr,
	uint8 *body, int body_len);

int wlc_wnm_get_trans_candidate_list_pref(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	struct ether_addr *bssid);
extern void wlc_wnm_clear_scbstate(void *p);
extern uint32 wlc_wnm_get_cap(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern uint32 wlc_wnm_set_cap(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 cap);
#ifdef AP
#ifdef PROXYARP
/* WNM proxy_arp enable/disable */
extern bool wlc_wnm_proxyarp(wlc_info_t *wlc);
#endif /* PROXYARP */

/* WNM packet handler */
extern int wlc_wnm_packets_handle(wlc_bsscfg_t *bsscfg, void *p, bool frombss);
#endif /* AP */
extern uint16 wlc_wnm_bss_max_idle_prd(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#ifdef AP
extern uint32 wlc_wnm_timestamp(wlc_wnm_info_t *wnm_info);
extern void wlc_process_tim_bcast_req_ie(wlc_info_t *wlc, uint8 *tlvs, int len, struct scb *scb);
extern int wlc_wnm_get_tim_bcast_resp_ie(wlc_info_t *wlc, uint8 *p, int *plen, struct scb *scb);
extern void wlc_wnm_tbtt(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_wnm_update_checkbeacon(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	bool perm_cb, bool temp_cb);
extern bool wlc_wnm_dms_amsdu_on(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
#endif /* AP */
#ifdef STA
extern void wlc_process_bss_max_idle_period_ie(wlc_info_t *wlc, uint8 *tlvs,
	int len, struct scb *scb);
extern int wlc_wnm_set_bss_max_idle_prd(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 p, uint8 o);
extern void wlc_process_tim_bcast_resp_ie(wlc_info_t *wlc, uint8 *tlvs, int len, struct scb *scb);
#endif /* STA */
#endif	/* _wlc_wnm_h_ */
