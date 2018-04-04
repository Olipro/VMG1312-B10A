/*
 * AP Module Public Interface
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_ap.h 370836 2012-11-23 23:19:04Z $
 */
#ifndef _WLC_AP_H_
#define _WLC_AP_H_

#if defined(RXCHAIN_PWRSAVE) || defined(RADIO_PWRSAVE)
#define WLPWRSAVERXFADD(wlc, v)	do { if ((wlc)->ap != NULL) (wlc)->ap->rxframe += (v); } while (0)
#define WLPWRSAVERXFINCR(wlc)	do { if ((wlc)->ap != NULL) (wlc)->ap->rxframe++; } while (0)
#define WLPWRSAVETXFINCR(wlc)	do { if ((wlc)->ap != NULL) (wlc)->ap->txframe++; } while (0)
#define WLPWRSAVERXFVAL(wlc)	(((wlc)->ap != NULL) ? (wlc)->ap->rxframe : 0)
#define WLPWRSAVETXFVAL(wlc)	(((wlc)->ap != NULL) ? (wlc)->ap->txframe : 0)
#endif

struct wlc_ap_info {
	bool		lazywds;			/* create WDS partners on the fly */
	bool 		shortslot_restrict;		/* only allow assoc by shortslot STAs */
#ifdef RXCHAIN_PWRSAVE
	uint8		rxchain_pwrsave_enable;		/* rxchain based power save enable */
#endif
#ifdef RADIO_PWRSAVE
	uint8		radio_pwrsave_enable;		/* radio duty cycle power save enable */
#endif
	uint16		pre_tbtt_us;		/* Current pre-TBTT us value */
	uint32		pre_tbtt_max_lat_us;	/* Max permitted latency for TBTT DPC */
	chanspec_t	pref_chanspec;			/* User preferred chanspec */
#if defined(NDIS) && (NDISVER >= 0x0620)
	int		scb_handle;		/* scb cubby handle to retrieve data from scb */
#endif /* NDIS && (NDISVER >= 0x0620) */

#ifdef BCM_DCS
	bool dcs_enabled;
#endif /* BCM_DCS */
	uint16		txbcn_timeout;			/* txbcn inactivity timeout */
	uint32		rxframe;		/* receive frame counter */
	uint32		txframe;		/* transmit frame counter */
};

#if defined(NDIS) && (NDISVER >= 0x0620)
/* association information */
typedef struct {
	void		*assoc_req;		/* association request frame */
	uint		assoc_req_len;	/* association request frame length */
	void		*assoc_rsp;	/* association response frame */
	uint		assoc_rsp_len;	/* association response frame length */
	void		*bcn;		/* AP beacon */
	uint		bcn_len;	/* AP beacon length */
	uint8		auth_alg;	/* 802.11 authentication mode */
	uint8		WPA_auth;	/* WPA: authenticated key management */
	uint32		wsec;		/* ucast security algo */
	wpa_ie_fixed_t	*wpaie;	/* WPA ie */
	bool		ewc_cap;	/* EWC (MIMO) capable */
	bool		ofdm;		/* OFDM */
} assoc_info_t;

/* association decision information */
typedef struct {
	bool		assoc_approved;		/* (re)association approved */
	uint16		reject_reason;		/* reason code for rejecting association */
	LARGE_INTEGER	sys_time;		/* current system time */
} assoc_decision_t;
#endif /* NDIS && (NDISVER >= 0x0620) */

/* Time to live for probe response frames in microseconds; timed from when request arrived */
#if !defined(WLC_PRB_TTL_us)
#define WLC_PRB_TTL_us 40000
#endif /* ttl defined */

#ifdef BAND5G
#define WL11H_AP_ENAB(wlc)	(AP_ENAB((wlc)->pub) && WL11H_ENAB(wlc))
#else
#define WL11H_AP_ENAB(wlc)	0
#endif /* BAND5G */

#ifdef RXCHAIN_PWRSAVE
#define RXCHAIN_PWRSAVE_ENAB(ap) ((ap)->rxchain_pwrsave_enable)
#else
#define RXCHAIN_PWRSAVE_ENAB(ap) 0
#endif

#ifdef RADIO_PWRSAVE
#define RADIO_PWRSAVE_ENAB(ap) ((ap)->radio_pwrsave_enable)
#else
#define RADIO_PWRSAVE_ENAB(ap) 0
#endif

#ifdef AP
extern wlc_ap_info_t* wlc_ap_attach(wlc_info_t *wlc);
extern void wlc_ap_detach(wlc_ap_info_t *ap);
extern int wlc_ap_up(wlc_ap_info_t *apinfo, wlc_bsscfg_t *bsscfg);
extern int wlc_ap_down(wlc_ap_info_t *apinfo, wlc_bsscfg_t *bsscfg);
extern int wlc_ap_mbss4_tbtt(wlc_info_t *wlc, uint32 macintstatus);
extern int wlc_ap_mbss16_tbtt(wlc_info_t *wlc, uint32 macintstatus);
extern void wlc_ap_mbss16_write_prbrsp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool suspend);
extern void wlc_mbss16_updssid(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_restart_ap(wlc_ap_info_t *ap);
extern void wlc_ap_authresp(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, void *body, uint len, bool, bool);
extern void wlc_wme_setup_req(wlc_ap_info_t *ap, struct dot11_management_header *hdr,
	uint8 *body, int body_len);
extern void wlc_wme_initparams_ap(wlc_ap_info_t *ap, wme_param_ie_t *pe);
extern void wlc_eapol_event(wlc_ap_info_t *ap, const struct ether_addr *ea, uint8 *data,
	uint32 len);
extern int wlc_wpa_set_cap(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg, uint8 *cap, int len);
extern void wlc_ap_process_assocreq(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct dot11_management_header *hdr, void *body, uint body_len, struct scb *scb, bool);
extern bool wlc_roam_check(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg,
	struct ether_header *eh, uint len);

extern void wlc_ap_pspretend_probe(void *arg);
extern bool wlc_ap_do_pspretend_probe(wlc_info_t *wlc, struct scb *scb, uint32 elapsed_time);

#ifdef WL11N
extern bool wlc_ht_ap_coex_tea_chk(wlc_bsscfg_t *cfg, ht_cap_ie_t *cap_ie);
extern bool wlc_ht_ap_coex_ted_chk(wlc_info_t *wlc, bcm_tlv_t *tlv, uint8);
extern void wlc_ht_coex_switch_bw(wlc_bsscfg_t *cfg, bool downgrade, uint8 rc);
extern void wlc_ht_coex_update_fid_time(wlc_bsscfg_t *cfg);
extern void wlc_ht_coex_update_permit(wlc_bsscfg_t *cfg, bool permit);
extern void wlc_ht_ap_coex_tebc_proc(wlc_bsscfg_t *cfg);
extern void wlc_switch_ab_11n(wlc_info_t *wlc, bool ab, bool init);
#endif /* WL11N */

extern int wlc_wds_wpa_role_set(wlc_ap_info_t *ap, struct scb *scb, uint8 role);

extern uint wlc_ap_stas_associated(wlc_ap_info_t *ap);

extern void wlc_ap_bsscfg_scb_cleanup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);
extern void wlc_ap_scb_cleanup(wlc_info_t *wlc);

#ifdef RXCHAIN_PWRSAVE
extern void wlc_reset_rxchain_pwrsave_mode(wlc_ap_info_t *ap);
extern void wlc_disable_rxchain_pwrsave(wlc_ap_info_t *ap);
#ifdef WL11N
extern uint8 wlc_rxchain_pwrsave_stbc_rx_get(wlc_info_t *wlc);
#endif /* WL11N */
#endif /* RXCHAIN_PWRSAVE */

extern int wlc_ap_get_maxassoc(wlc_ap_info_t *ap);
extern void wlc_ap_set_maxassoc(wlc_ap_info_t *ap, int val);
extern int wlc_ap_get_maxassoc_limit(wlc_ap_info_t *ap);

#ifdef RADIO_PWRSAVE
extern int wlc_radio_pwrsave_in_power_save(wlc_ap_info_t *ap);
extern void wlc_radio_pwrsave_enter_mode(wlc_info_t *wlc, bool dtim);
extern void wlc_radio_pwrsave_exit_mode(wlc_ap_info_t *ap);
extern void wlc_radio_pwrsave_on_time_start(wlc_ap_info_t *ap, bool dtim);
#endif

extern void wlc_ap_probe_complete(wlc_info_t *wlc, void *pkt, uint txs);

extern void wlc_bss_up(wlc_ap_info_t *ap, wlc_bsscfg_t *bsscfg);

extern int wpa_cipher_enabled(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, int cipher);

#else /* AP */

/* Stub functions help eliminate using #ifdef AP macros */
#define wlc_ap_attach(a) (wlc_ap_info_t *)(uintptr)0xdeadc0de
#define wlc_ap_detach(a) do {} while (0)
#define wlc_ap_up(a, b) do {} while (0)
#define wlc_ap_down(a, b) 0
#define wlc_ap_mbss4_tbtt(a, b) 0
#define wlc_ap_mbss16_tbtt(a, b) 0
#define wlc_ap_mbss16_write_prbrsp(a, b, c) 0
#define wlc_mbss16_updssid(a, b) do {} while (0)
#define wlc_restart_ap(a) do {} while (0)
#define wlc_ap_authresp(a, b, c, d, e, f, g) do {} while (0)
#define wlc_wme_setup_req(a, b, c, d) do {} while (0)
#define wlc_wme_initparams_ap(a, b) do {} while (0)

#define wlc_eapol_event(a, b, c, d) do {} while (0)
#define wlc_wpa_set_cap(a, b, c, d) 0
#define wlc_ap_process_assocreq(a, b, c, d, e, f, g) do {} while (0)
#define wlc_roam_check(a, b, c, d) FALSE
#define wlc_ht_ap_coex_tea_chk(a, b) 0
#define wlc_ht_ap_coex_ted_chk(a, b, c) 0
#define wlc_ht_coex_update_fid_time(a) do {} while (0)
#define wlc_ht_coex_update_permit(a, b) do {} while (0)
#define wlc_ht_coex_switch_bw(a, b, c) do {} while (0)
#define wlc_ht_ap_coex_tebc_proc(a) do {} while (0)

#define wlc_wds_wpa_role_set(a, b, c) 0
#define wlc_ap_stas_associated(ap) 0
#define wlc_ap_scb_cleanup(a) do {} while (0)

#define wlc_bss_up(ap, cfg, bcn, len) do {} while (0)

#endif /* AP */

#ifdef MBSS
extern bool wlc_prq_process(wlc_info_t *wlc, bool bounded);
#else
#define wlc_prq_process(wlc, bounded) FALSE;
#endif

#endif /* _WLC_AP_H_ */
