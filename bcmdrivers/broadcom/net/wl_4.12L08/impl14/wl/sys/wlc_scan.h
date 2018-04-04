/*
 * SCAN Module Public Interface
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_scan.h 293697 2011-11-02 23:12:39Z $
 */
#ifndef _WLC_SCAN_H_
#define _WLC_SCAN_H_

/* scan state bits */
#define SCAN_STATE_SUPPRESS	(1 << 0)
#define SCAN_STATE_SAVE_PRB	(1 << 1)
#define SCAN_STATE_PASSIVE	(1 << 2)
#define SCAN_STATE_WSUSPEND	(1 << 3)
#define SCAN_STATE_RADAR_CLEAR	(1 << 4)
#define SCAN_STATE_PSPEND	(1 << 5)
#define SCAN_STATE_SUSPEND_REQ	(1 << 6)
#define SCAN_STATE_READY	(1 << 7)
#define SCAN_STATE_INCLUDE_CACHE	(1 << 8)
#define SCAN_STATE_PROHIBIT	(1 << 9)
#define SCAN_STATE_PSPEND_TIMER	(1 << 10)
#define SCAN_STATE_IN_TMR_CB	(1 << 11)

#define WLC_SCAN_PASSIVE_TIME	110	/* ms to listen on a channel for beacons for passivescan */
#define WLC_SCAN_AWAY_LIMIT	100	/* max time to be away from home channel before return */
#define WLC_SCAN_PS_PREP_TIME	10	/* average time to announce PS mode */
#define WLC_SCAN_PS_OVERHEAD	50	/* overhead time to allow for one long PS announce delay */

#define WLC_SCAN_NSSID_PREALLOC	5	/* Number of preallocated SSID slots */
#ifdef EXTENDED_SCAN
#define WLC_SCAN_NCHAN_PREALLOC	60	/* Number of preallocated (extd) channel slots */
#else
#define WLC_SCAN_NCHAN_PREALLOC	0	/* Obviously none if extended scan not included! */
#endif

struct wlc_scan_info {
	void		*scan_priv;		/* pointer to scan private struct */
	uint16		state;			/* scan state bits */
	bool		in_progress;		/* scan in progress */
	uint8 		usage;			/* scan engine usage */
	int		bss_type;		/* Scan for Infra, IBSS, or Any */
	struct ether_addr bssid;
#ifdef WLSCANCACHE
	bool		_scancache;		/* scan cache enable */
#endif
#ifdef WIFI_ACT_FRAME
	void		*action_frame;		/* action frame for off channel */
#endif
};

#define SCAN_IN_PROGRESS(scan_info)	((scan_info) && (scan_info)->in_progress)

/* scan engine usage */
#define SCAN_ENGINE_USAGE_NORM	0	/* scan, assoc, roam, etc. */
#define SCAN_ENGINE_USAGE_ESCAN	1	/* escan */
#define SCAN_ENGINE_USAGE_AF	2	/* action frame */
#define SCAN_ENGINE_USAGE_RM	3	/* RM */

#define ESCAN_IN_PROGRESS(scan)		((scan)->usage == SCAN_ENGINE_USAGE_ESCAN)
#define ACT_FRAME_IN_PROGRESS(scan)	((scan)->usage == SCAN_ENGINE_USAGE_AF)

#ifdef WLSCANCACHE
#define SCANCACHE_ENAB(scan_info)	((scan_info)->_scancache)
#else
#define SCANCACHE_ENAB(scan_info)	0
#endif

extern wlc_scan_info_t *wlc_scan_attach(wlc_info_t *wlc, void *wl, osl_t *osh, uint);
extern void wlc_scan_detach(wlc_scan_info_t *scan_ptr);

extern bool wlc_scan_in_scan_chanspec_list(wlc_scan_info_t *wlc_scan_info, chanspec_t chanspec);

extern int wlc_scan(
	wlc_scan_info_t *scan_ptr,
	int bss_type,
	const struct ether_addr* bssid,
	int nssid,
	wlc_ssid_t *ssid,
	int scan_type,
	int nprobes,
	int active_time,
	int passive_time,
	int home_time,
	const chanspec_t* chanspec_list,
	int channel_num,
	chanspec_t chanspec_start,
	bool save_prb,
	scancb_fn_t fn,
	void* arg,
	int away_limit_override,
	bool extd_scan,
	bool suppress_ssid,
	bool include_cache,
	bool chan_prohibit,
	wlc_bsscfg_t *cfg,
	uint8 usage, actcb_fn_t act_cb, void *act_arg);

extern void wlc_scan_abort(wlc_scan_info_t *wlc_scan_info, int status);
extern void wlc_scan_abort_ex(wlc_scan_info_t *wlc_scan_info, wlc_bsscfg_t *cfg, int status);
extern void wlc_scan_terminate(wlc_scan_info_t *wlc_scan_info, int status);
extern void wlc_scan_radar_clear(wlc_scan_info_t *wlc_scan_info);
extern void wlc_scan_default_channels(wlc_scan_info_t *wlc_scan_info, chanspec_t *list, int *cnt);
extern bool wlc_scan_inprog(wlc_info_t *wlc_info);
extern void wlc_scan_fifo_suspend_complete(wlc_scan_info_t *wlc_scan_info);
extern void wlc_scan_pm_pending_complete(wlc_scan_info_t *wlc_scan_info);
extern chanspec_t wlc_scan_get_current_chanspec(wlc_scan_info_t *wlc_scan_info);
extern int wlc_scan_ioctl(wlc_scan_info_t *wlc_scan_info, int cmd, void *arg, int len,
	struct wlc_if *wlcif);
extern bool wlc_scan_ssid_match(wlc_scan_info_t *wlc_scan_info, bcm_tlv_t *ssid_ie, bool filter);
extern bool wlc_scan_recv_parse_bcn_prb(wlc_scan_info_t *si, bool beacon,
	wlc_bss_info_t *bi, uint8 *body, int body_len);

#ifdef EXTENDED_SCAN
extern int wlc_extdscan_request(wlc_scan_info_t *scan_info, void *params,
	int len, scancb_fn_t fn, void* arg);
extern void wlc_extdscan(wlc_info_t *wlc, int max_txrate, int nchan, chan_scandata_t *channel_list,
	wl_scan_type_t scan_type, int nprobes, bool split_scan, int nssid, wlc_ssid_t *ssid,
	scancb_fn_t fn, void *arg);
#endif

#ifdef WLSCANCACHE
extern void wlc_scan_get_cache(wlc_scan_info_t *scan_info,
                   const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                   int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                   wlc_bss_list_t *bss_list);
#else
#define wlc_scan_get_cache(si, BSSID, nssid, SSID, BSS_type, c_list, c_num, bss_list)	\
	(void)((bss_list)->count = 0)
#endif

extern wlc_bsscfg_t *wlc_scan_bsscfg(wlc_scan_info_t *scan_info);

void wlc_scan_timer_update(wlc_scan_info_t *wlc_scan_info, uint32 ms);

#endif /* _WLC_SCAN_H_ */
