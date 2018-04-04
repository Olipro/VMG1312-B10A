/*
 * Common (OS-independent) definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_lmac.h,v 1.19.14.1 2010-04-21 03:57:03 Exp $: wlc_lmac.h
 */

#ifndef _wlc_lmac_h_
#define _wlc_lmac_h_

#include <wllmacctl.h>

#define LMAC_MAXRXBUF		10

#define LMAC_MAXTXBUF_AC_BE	10
#define LMAC_MAXTXBUF_AC_BK	4
#define LMAC_MAXTXBUF_AC_VI	4
#define LMAC_MAXTXBUF_AC_VO	2

#define LMAC_MAXTXBUF	\
	(LMAC_MAXTXBUF_AC_BE + LMAC_MAXTXBUF_AC_BK + LMAC_MAXTXBUF_AC_VI + LMAC_MAXTXBUF_AC_VO)

struct wl_lmac_txrate_class;
struct wllmac_htobss_op;
struct wllmac_htcap;

typedef struct wl_lmac_frmtemplate wl_lmac_frmtemplate_t;
#define WLC_MAXFRM_TEMPLATES			6
#define	WLLMAC_PF_MAX_UDPPORTS			16
#define	WLLMAC_PF_MAX_ETHTYPES			16

typedef struct lmac_tx_rate {
	bool		inited;
	wlc_rateset_t	rateset;
} lmac_tx_rate_t;

/* same LMAC build would work with different S60 versions */
#define LMAC_S60_VERSION_3_2	1
#define LMAC_S60_VERSION_5_0	2

#define LMAC_S60_VERSION_3_2_STR	"3.2"
#define LMAC_S60_VERSION_5_0_STR	"5.0"

#define LMAC_S60_5_0(lmac) ((lmac->s60_version == LMAC_S60_VERSION_5_0) ? TRUE : FALSE)
#define LMAC_S60_3_2(lmac) ((lmac->s60_version == LMAC_S60_VERSION_3_2) ? TRUE : FALSE)

struct lmac_info {
	wlc_info_t 	*wlc;
	wlc_pub_t       *pub;

	uint		ucoderev;

	wl_lmac_frmtemplate_t	*lmac_templates[WLC_MAXFRM_TEMPLATES];

	uint32 		lmac_ps[AC_COUNT];

	uint32		rx_maxlifetime[AC_COUNT]; /* per AC rxmaxlifetime value  */

	int32		txpower_override;

	uint32		bcn_count;
	bool		bcn_filter_enable;
	wl_lmac_bcniefilter_t	usr_bcnie_filter;
	uint8		*bcn_tagparams;	/* buffer */
	uint32		bcn_tagparams_len;

	uint32		device_powerstate;
	uint8		wakeinterval;
	uint32		ibss_ps;

	/* lmac tx rate policies */
	bool		txautorate_enable;
	lmac_tx_rate_t	lmac_tx_rate[AC_COUNT];

	bool		lmac_ps_pending;
	struct wl_timer *radar_timer;
	struct wl_timer *pschange_timer;
	struct wl_timer *join_timer;
	bool		lmac_rssi_low_indicated;
	uint32		lmac_rxbcn;
	int8		lmac_def_wep_key_idx;
	wsec_key_t	*lmac_keys[WSEC_MAX_KEYS];
	uint32		lmac_mode;
	bool		lmac_probe_for_join;
	bool		lmac_assoc_in_progress;	/* deduced state to stop the cals from happening */

	/* Buffer to hold onto lmac txstatus to send them to user */
	uchar 		*lmactxstatusbuf;
	uint32		lmac_txsbufcuroffset;
	uint32		pend_txs_cnt;
	uint32		max_pend_txs_cnt;
	bool		txs_sendup_pending;
	bool		txs_alloc_fails;

	/* LMAC packet filters */
	mbool		lmac_hostpktfilter;

	uint32		pf_arp_hostip;

	uint32		pf_udpport_cnt;
	uint32		pf_udpport_flags;
	uchar 		pf_udpports[WLLMAC_PF_MAX_UDPPORTS * 2];

	uint32		pf_ethtype_cnt;
	uint32		pf_ethtype_flags;
	uchar 		pf_ethtypes[WLLMAC_PF_MAX_ETHTYPES * 2];

	bool		lmac_bss_htcapable;
	ht_cap_ie_t	lmac_bss_htcap;
	ht_add_ie_t	lmac_bss_htinfo;

	uint32		*lmac_txs_glomstats;
	uint32		s60_version;
	/* amsdu stats */
	uint32		rx_amsdu_cnt;
	uint32		rx_amsdu_msdu_cnt;

	/* rxdrops */
	uint32		rxdrops;

};

extern bool wlc_send80211pkt(wlc_info_t *wlc, void *sdu, struct wlc_if *wlcif);
extern void wlc_lmac_event(lmac_info_t *lmac, uint32 eventid, uint32 status,
	void *data, uint32 len);
extern void wlc_lmac_rx_sendup(lmac_info_t *lmac, void *p, wlc_d11rxhdr_t *wrxh,
	uint32 rxflags, uint32 status, wsec_key_t *key);
extern void wlc_lmac_event(lmac_info_t *lmac, uint32 eventid, uint32 status, void *data,
	uint32 len);
extern uint8 rssi2rcpi(int8 rssi);
extern int8 rcpi2rssi(uint8 rcpi);

extern void dump_80211_htcap(ht_cap_ie_t *cap_ie);
extern void dump_80211_htinfo(ht_add_ie_t *add_ie);
extern void dump_mcs_set(uchar *mcs, int len);

#ifdef WLLMAC
extern void wlc_lmac_txstatus(lmac_info_t *lmac, void *p, struct tx_status *txs,
	uint supr_status, int retries, int rts_count, bool ampdu);
extern void wlc_lmac_txstatus_sendup(lmac_info_t *lmac, bool flush);
extern void wlc_lmac_tbtt(lmac_info_t *lmac, d11regs_t *regs);
extern void wlc_lmac_pschange_complete(void * lmac);
extern void wlc_lmac_recvdata(lmac_info_t *lmac, struct wlc_frminfo *f, uint32 status);
extern void wlc_lmac_indicate_initdone(lmac_info_t *lmac);
extern int wlc_lmac_up(lmac_info_t *lmac);
extern int wlc_lmac_down(lmac_info_t *lmac);
extern uint16 wlc_lmac_recvfilter(lmac_info_t *lmac, struct dot11_header *h,
	int rx_bandunit, struct scb **pscb);
extern void wlc_lmac_recvctl(lmac_info_t *lmac, osl_t *osh, wlc_d11rxhdr_t *rxh, void *p);
extern void* wlc_lmac_pspoll_get(lmac_info_t *lmac);
extern void* wlc_lmac_sendnulldata(lmac_info_t *lmac, struct ether_addr *ea,
	ratespec_t rate_override, uint32 pktflags, int prio);
extern void wlc_lmac_roam_rssi_check(lmac_info_t *lmac);
extern void wlc_lmac_sendprobe(lmac_info_t *lmac, uchar ssid[], int ssid_len,
	const struct ether_addr *da, const struct ether_addr *bssid, ratespec_t rspec);

extern int wlc_lmac_ioctl(lmac_info_t *lmac, int cmd, void *arg, int len, struct wlc_if  *wlcif);
extern void *wlc_lmac_attach(void *wl, uint16 vendor, uint16 device, uint unit, bool piomode,
	osl_t *osh, void *regsva, uint bustype, void *btparam, uint *perr);

extern void wlc_lmac_assoc_timeout(void *arg);

#else /* WLLMAC */

#define wlc_lmac_up(a) 					0
#define wlc_lmac_down(a)				0
#define wlc_lmac_recvfilter(a, b, c, d)			0
#define wlc_lmac_pspoll_get(a)				NULL
#define wlc_lmac_sendnulldata(a, b, c, d, e)		NULL
#define wlc_lmac_ioctl(a, b, c, d, e)			0
#define wlc_lmac_tbtt(a, b) 				do {} while (0)
#define wlc_lmac_pschange_complete(a) 			do {} while (0)
#define wlc_lmac_recvdata(a, b, c)			do {} while (0)
#define wlc_lmac_indicate_initdone(a)			do {} while (0)
#define wlc_lmac_recvctl(a, b, c, d)			do {} while (0)
#define wlc_lmac_roam_rssi_check(a)			do {} while (0)
#define wlc_lmac_sendprobe(a, b, c, d, e, f)		do {} while (0)
#define wlc_lmac_txstatus(a, b, c, d, e, f)		do {} while (0)
#define wlc_lmac_txstatus_sendup(a, b)			do {} while (0)
#endif /* WLLMAC */

#endif /* wlc_lmac.h */
