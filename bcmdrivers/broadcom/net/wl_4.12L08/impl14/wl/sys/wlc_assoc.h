/*
 * Association/Roam related routines
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_assoc.h 300516 2011-12-04 17:39:44Z $
 */

#ifndef __wlc_assoc_h__
#define __wlc_assoc_h__

#ifdef STA
extern void wlc_join(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *SSID, int len,
	wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len);
extern void wlc_join_recreate(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

extern void wlc_join_complete(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn, int bcn_len);
extern void wlc_join_recreate_complete(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn, int bcn_len);

extern int wlc_join_pref_parse(wlc_bsscfg_t *cfg, uint8 *pref, int len);
extern void wlc_join_pref_reset(wlc_bsscfg_t *cfg);
extern void wlc_join_pref_band_upd(wlc_bsscfg_t *cfg);

extern int wlc_reassoc(wlc_bsscfg_t *cfg, wl_reassoc_params_t *reassoc_params);

extern void wlc_roam_complete(wlc_bsscfg_t *cfg, uint status,
                              struct ether_addr *addr, uint bss_type);
extern int wlc_roam_scan(wlc_bsscfg_t *cfg, uint reason);
extern void wlc_roamscan_start(wlc_bsscfg_t *cfg, uint roam_reason);
extern void wlc_assoc_roam(wlc_bsscfg_t *cfg);
extern void wlc_txrate_roam(wlc_info_t *wlc, struct scb *scb, tx_status_t *txs, bool pkt_sent,
	bool pkt_max_retries);
extern void wlc_build_roam_cache(wlc_bsscfg_t *cfg, wlc_bss_list_t *candidates);
extern void wlc_roam_motion_detect(wlc_bsscfg_t *cfg);
extern void wlc_roam_bcns_lost(wlc_bsscfg_t *cfg);
extern int wlc_roam_trigger_logical_dbm(wlc_info_t *wlc, wlcband_t *band, int val);

extern int wlc_disassociate_client(wlc_bsscfg_t *cfg, bool send_disassociate,
	pkcb_fn_t fn, void *arg);

extern int wlc_assoc_abort(wlc_bsscfg_t *cfg);
extern void wlc_assoc_timeout(void *cfg);
extern void wlc_assoc_change_state(wlc_bsscfg_t *cfg, uint newstate);
extern void wlc_authresp_client(wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, void *body, uint body_len, bool short_preamble);
extern void wlc_assocresp_client(wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, void *body, uint body_len, struct scb *scb);
extern void wlc_ftresp_client(wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, void *body, uint body_len);
extern void wlc_auth_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg);
extern void wlc_assocreq_complete(wlc_info_t *wlc, uint txstatus, void *arg);

extern void wlc_sta_assoc_upd(wlc_bsscfg_t *cfg, bool state);

extern void wlc_clear_hw_association(wlc_bsscfg_t *cfg, bool mute_mode);
#endif /* STA */

extern int wlc_mac_request_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req);

extern void wlc_roam_defaults(wlc_info_t *wlc, wlcband_t *band, int *roam_trigger, uint *rm_delta);

extern void wlc_disassoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint disassoc_reason, uint bss_type);
extern void wlc_deauth_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	const struct ether_addr *addr, uint deauth_reason, uint bss_type);
typedef struct wlc_deauth_send_cbargs {
	struct ether_addr	ea;
	int8			_idx;
} wlc_deauth_send_cbargs_t;
extern void wlc_deauth_sendcomplete(wlc_info_t *wlc, uint txstatus, void *arg);
extern void wlc_disassoc_ind_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	struct ether_addr *addr, uint disassoc_reason, uint bss_type,
	uint8 *body, int body_len);
extern void wlc_deauth_ind_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	struct ether_addr *addr, uint deauth_reason, uint bss_type,
	uint8 *body, int body_len);

#endif /* __wlc_assoc_h__ */
