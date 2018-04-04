/*
 * WAPI (WLAN Authentication and Privacy Infrastructure) public header file
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

#ifndef _wlc_wapi_h_
#define _wlc_wapi_h_

/*
 * We always have BCMWAPI_WPI enabled we don't have
 * other cipher alternation instead of WPI (SMS4)
 */
#if defined(BCMWAPI_WPI) || defined(BCMWAPI_WAI)

/* For INLINE wlc_wapi_rxiv_update */
#include <wlc_frmutil.h>

#ifdef BCMWAPI_WAI

/* WAPI public bsscfg cubby structure and access macro */
typedef struct wlc_wapi_bsscfg_cubby_pub wlc_wapi_bsscfg_cubby_pub_t;

/* Use wlc instead of wapi to save one dereference instruction time */
#define WAPI_BSSCFG_PUB(wlc, cfg) \
	((wlc_wapi_bsscfg_cubby_pub_t *)BSSCFG_CUBBY((cfg), (wlc)->wapi_cfgh))

struct wlc_wapi_bsscfg_cubby_pub {
	/* data path variables provide macro access */
	bool	wai_restrict;	/* restrict data until WAI auth succeeds */
};
#define WAPI_WAI_RESTRICT(wlc, cfg)	(WAPI_BSSCFG_PUB((wlc), (cfg))->wai_restrict)

/* Macros for lookup the unicast/multicast ciphers for SMS4 in RSN */
#define WAPI_RSN_UCAST_LOOKUP(prsn)	(wlc_rsn_ucast_lookup((prsn), WAPI_CIPHER_SMS4))
#define WAPI_RSN_MCAST_LOOKUP(prsn)	((prsn)->multicast == WAPI_CIPHER_SMS4)

/*
 * WAI LLC header,
 * DSAP/SSAP/CTL = AA:AA:03
 * OUI = 00:00:00
 * Ethertype = 0x88b4 (WAI Port Access Entity)
 */
#define WAPI_WAI_HDR	"\xAA\xAA\x03\x00\x00\x00\x88\xB4"
#define WAPI_WAI_SNAP(pbody)	(bcmp(WAPI_WAI_HDR, (pbody), DOT11_LLC_SNAP_HDR_LEN) == 0)
#endif /* BCMWAPI_WAI */

/* INLINE, because it is used in the time-critical tx path */
static INLINE void
wlc_wapi_key_iv_update(wsec_key_t *key, uchar *buf, bool update)
{
	ASSERT(key->algo == CRYPTO_ALGO_SMS4);

	/* as per spec increment 8.2.4 of WAPI specification increment counter before sending */
	/* if multilcast packet should increment the iv by 1, IBSS or AP */
	if (update) {
		if (ETHER_ISNULLADDR(&key->ea))
			bcm_inc_bytes((uchar *)key->wapi_txiv.PN, 16, 1);
		else
			bcm_inc_bytes((uchar *)key->wapi_txiv.PN, 16, 2);
	}
	bcopy((char *)&key->wapi_txiv, &buf[0], sizeof(struct wpi_iv));
}

/* INLINE, because it is used in the time-critical rx path */
static INLINE void
wlc_wapi_rxiv_update(struct wlc_frminfo *f)
{
	struct wpi_iv *wpi_rxiv;

	wpi_rxiv = (struct wpi_iv *)(f->pbody - f->key->iv_len);
	bcopy(&wpi_rxiv->PN[0], &f->key->wapi_rxiv.PN[0], SMS4_WPI_PN_LEN);

	/* check for broadcast packets */
	if (ETHER_ISNULLADDR(&f->key->ea))
		bcm_inc_bytes((uchar *)f->key->wapi_rxiv.PN, 16, 1);
	else
		bcm_inc_bytes((uchar *)f->key->wapi_rxiv.PN, 16, 2);
}

#define WAPI_HW_WPI_ENAB(wlc)	((wlc)->pub->_wapi_hw_wpi)

/* module */
extern wlc_wapi_info_t *wlc_wapi_attach(wlc_info_t *wlc);
extern void wlc_wapi_detach(wlc_wapi_info_t *wapi);

#ifdef BCMWAPI_WAI
extern uint8 *wlc_wapi_write_ie_safe(wlc_wapi_info_t *wapi, uint8 *cp, int buflen,
	uint16 WPA_auth, uint32 wsec, wlc_bsscfg_t *bsscfg);
extern int wlc_wapi_parse_ie(wlc_wapi_info_t *wapi, bcm_tlv_t *wapiie, wlc_bss_info_t *bi);
extern void wlc_wapi_station_event(wlc_wapi_info_t* wapi, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *addr, void *ie, uint8 *gsn, uint16 msg_type);
extern void wlc_wapi_copy_info_elt(wlc_wapi_info_t *wapi, wlc_bsscfg_t *bsscfg, uint8 **pbody);
#if !defined(WLNOEIND)
extern void wlc_wapi_bss_wai_event(wlc_wapi_info_t *wapi, wlc_bsscfg_t * bsscfg,
	const struct ether_addr *ea, uint8 *data, uint32 len);
#endif /* !WLNOEIND */
#ifdef AP
extern int wlc_wapi_check_ie(wlc_wapi_info_t *wapi, wlc_bsscfg_t *bsscfg, bcm_tlv_t *wapiie,
	uint16 *auth, uint32 *wsec);
extern void wlc_wapi_wai_req_ind(wlc_wapi_info_t *wapi, wlc_bsscfg_t *bsscfg, struct scb *scb);
#endif /* AP */
#endif /* BCMWAPI_WAI */

extern void wlc_wapi_write_hw_mic_key(wlc_wapi_info_t *wapi, int indx, wsec_key_t *key);
extern void wlc_wapi_key_iv_init(wlc_wapi_info_t *wapi, wlc_bsscfg_t *cfg, wsec_key_t *key,
	wsec_iv_t *initial_iv);
extern bool wlc_wapi_key_rotation_update(wlc_wapi_info_t *wapi, struct scb *scb, uint32 key_algo,
	uint32 key_id);
extern wsec_key_t *wlc_wapi_scb_prev_key(wlc_wapi_info_t *wapi, struct scb *scb);
extern wsec_key_t *wlc_wapi_key_lookup(wlc_wapi_info_t *wapi, struct scb *scb,
	wsec_key_t *key, uint indx);
extern void wlc_wapi_key_delete(wlc_wapi_info_t *wapi, struct scb *scb);
extern void wlc_wapi_key_scb_delete(wlc_wapi_info_t *wapi, struct scb *scb, wlc_bsscfg_t *bsscfg);

#endif /* BCMWAPI_WPI || BCMWAPI_WAI */
#endif /* _wlc_wapi_h_ */
