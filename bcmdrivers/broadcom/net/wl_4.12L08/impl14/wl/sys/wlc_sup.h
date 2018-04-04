/*
 * Exposed interfaces of wlc_sup.c
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_sup.h 365823 2012-10-31 04:24:30Z $
 */

#ifndef _wlc_sup_h_
#define _wlc_sup_h_


/* Initiate supplicant private context */
extern void *wlc_sup_attach(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

/* Remove supplicant private context */
extern void wlc_sup_detach(struct supplicant *sup);

/* Down the supplicant, return the number of callbacks/timers pending */
extern int wlc_sup_down(struct supplicant *sup);

/* Send received EAPOL to supplicant; Return whether packet was used
 * (might still want to send it up to other supplicant)
 */
extern bool wlc_sup_eapol(struct supplicant *sup, eapol_header_t *eapol_hdr,
	bool encrypted, bool *auth_pending);


/* Values for type parameter of wlc_set_sup() */
#define SUP_UNUSED	0 /* Supplicant unused */
#ifdef	BCMSUP_PSK
#define SUP_WPAPSK	2 /* Used for WPA-PSK */
#endif /* BCMSUP_PSK */

extern bool wlc_set_sup(struct supplicant *sup, int type,
	/* parameters used only for PSK follow */
	uint8 *sup_ies, uint sup_ies_len, uint8 *auth_ies, uint auth_ies_len);
extern void wlc_sup_set_ea(supplicant_t *sup, struct ether_addr *ea);
/* helper fn to find supplicant and authenticator ies from assocreq and prbresp */
extern void wlc_find_sup_auth_ies(struct supplicant *sup, uint8 **sup_ies,
	uint *sup_ies_len, uint8 **auth_ies, uint *auth_ies_len);

#ifdef	BCMSUP_PSK
extern void wlc_sup_clear_pmkid_store(struct supplicant *sup);
extern void wlc_sup_pmkid_cache_req(struct supplicant *sup);

/* Install WPA PSK material in supplicant */
extern int wlc_sup_set_pmk(struct supplicant *sup, wsec_pmk_t *psk, bool assoc);

/* Send SSID to supplicant for PMK computation */
extern int wlc_sup_set_ssid(struct supplicant *sup, uchar ssid[], int ssid_len);

/* send deauthentication */
extern void wlc_wpa_senddeauth(wlc_bsscfg_t *bsscfg, char *da, int reason);

/* tell supplicant to send a MIC failure report */
extern void wlc_sup_send_micfailure(struct supplicant *sup, bool ismulti);

#ifdef WLFBT
/* verify with current mdid */
extern bool wlc_sup_is_cur_mdid(struct supplicant *sup, wlc_bss_info_t *bi);
extern int wlc_sup_ftauth_getlen(supplicant_t *sup);
extern int wlc_sup_ftreq_getlen(supplicant_t *sup);
extern void wlc_sup_ftauth_clear_ies(wlc_info_t *wlc, supplicant_t *sup);
extern uint8 * wlc_sup_ft_authreq(struct supplicant *sup, uint8 *pbody);
extern uint8 *wlc_sup_ft_pmkr1name(supplicant_t *sup);
extern uint8 * wlc_sup_ft_assocreq(supplicant_t *sup, uint8 *pbody, int len,
	bcm_tlv_t *mdie, bcm_tlv_t *rsnie, uint8 *ricdata, int ricdata_len, int ric_ie_count);
extern bool wlc_sup_ft_reassoc(supplicant_t *sup);
extern int wlc_sup_ft_authresp(supplicant_t *sup, uint8 *body, int body_len);
extern bool wlc_sup_ft_parse_reassocresp(supplicant_t *sup, uint8 *tlvs, int tlvs_len);
#endif /* WLFBT */
#endif	/* BCMSUP_PSK */


#if defined(BCMSUP_PSK)
/* Return the supplicant authentication status */
extern sup_auth_status_t wlc_sup_get_auth_status(struct supplicant *sup);

/* Return the extended supplicant authentication status */
extern sup_auth_status_t wlc_sup_get_auth_status_extended(struct supplicant *sup);

/* Send a supplicant status event */
extern void wlc_wpa_send_sup_status(struct supplicant *sup, uint reason);
#else
#define wlc_wpa_send_sup_status(sup, reason) do {} while (0)
#endif 


#if defined(WOWL) && defined(BCMSUP_PSK)
extern uint16 aes_invsbox[];
extern uint16 aes_xtime9dbe[];
extern void *wlc_sup_hw_wowl_init(struct supplicant *sup);
extern void wlc_sup_sw_wowl_update(struct supplicant *sup);
#else
#define wlc_sup_hw_wowl_init(a) NULL
#define wlc_sup_sw_wowl_update(a) do { } while (0)
#endif /* defined(WOWL) && defined (BCMSUP_PSK) */

#endif	/* _wlc_sup_h_ */
