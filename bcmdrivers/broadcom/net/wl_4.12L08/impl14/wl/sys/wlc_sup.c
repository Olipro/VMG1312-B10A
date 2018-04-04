/*
 * wlc_sup.c -- driver-resident supplicants.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_sup.c 365823 2012-10-31 04:24:30Z $
 */

#ifdef BCMINTSUP
#include <wlc_cfg.h>

#endif /* BCMINTSUP */

#ifndef	STA
#error "STA must be defined for wlc_sup.c"
#endif /* STA */
#if !defined(BCMSUP_PSK) && !defined(BCMAUTH_PSK)
#error "BCMCCX and/or BCMSUP_PSK and/or BCMAUTH_PSK must be defined"
#endif 

#ifdef BCMINTSUP
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <proto/eap.h>
#include <proto/eapol.h>
#include <bcmwpa.h>
#ifdef	BCMSUP_PSK
#include <bcmcrypto/passhash.h>
#include <bcmcrypto/prf.h>
#include <bcmcrypto/sha1.h>
#endif /* BCMSUP_PSK */

#include <proto/802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_led.h>
#include <wlc_rm.h>
#include <wl_export.h>
#include <wlc_security.h>
#include <wlc_scb.h>
#if defined(BCMSUP_PSK)
#include <wlc_wpa.h>
#endif 
#include <wlc_sup.h>
#ifdef WOWL
#include <wlc_wowl.h>
#endif

#else /* external supplicant */

#include <stdio.h>
#include <typedefs.h>
#include <wlioctl.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <bcmwpa.h>
#include <sup_dbg.h>
#include <bcmutils.h>
#include <string.h>
#include <bcmendian.h>
#include <bcmcrypto/prf.h>
#include <proto/eapol.h>
#include <bcm_osl.h>
#include "bcm_supenv.h"

#include "wpaif.h"
#include "wlc_sup.h"
#include "wlc_wpa.h"

#endif /* BCMINTSUP */

#ifdef MFP
#include <wlc_mfp.h>
#define SUPMFP(s) ((s)->wlc->mfp)
#endif

/* msg retry settings: */
#define LEAP_MAX_RETRY		3		/* no. of retries */
#define LEAP_TIMER_MSECS	30000		/* msecs between retries */
#define LEAP_START_DELAY	500		/* msecs delay for 1st START */
#define LEAP_HELD_DELAY		30000		/* msecs delay between retries */

#define SUP_CHECK_MCIPHER(sup) (sup->wpa->mcipher != cipher && IS_WPA_AUTH(sup->wpa->WPA_auth))
#define SUP_CHECK_EAPOL(body) (body->type == EAPOL_WPA_KEY || body->type == EAPOL_WPA2_KEY)

#define SUP_CHECK_WPAPSK_SUP_TYPE(sup) (sup->sup_type == SUP_WPAPSK)



/* Supplicant top-level structure hanging off bsscfg */
struct supplicant {
	/* copies of driver `common' things */
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	wlc_bsscfg_t *bsscfg;		/* pointer to sup's bsscfg */
	wlc_pub_t *pub;			/* pointer to wlc public portion */
	void *wl;			/* per-port handle */
	osl_t *osh;			/* PKT* stuff wants this */

	/* items common to any supplicant */
	int sup_type;			/* supplicant discriminator */
	struct ether_addr peer_ea;      /* peer's ea */

	/* items specific to the kind of supplicant */

#if defined(BCMSUP_PSK)
	uint		npmkid;
	sup_pmkid_t	pmkid[SUP_MAXPMKID];
	wpapsk_t *wpa;			/* volatile, initialized in set_sup */
	wpapsk_info_t *wpa_info;		/* persistent wpa related info */
	unsigned char ap_eapver;	/* eapol version from ap */
#endif	/* BCMSUP_PSK */

};

/* Simplify maintenance of references to driver `common' items. */
#define UNIT(ptr)	(((supplicant_t *)ptr)->pub->unit)
#define CUR_EA(ptr)	(((supplicant_t *)ptr)->bsscfg->cur_etheraddr)
#define PEER_EA(ptr)	(((supplicant_t *)ptr)->peer_ea)
#define BSS_EA(ptr)	(((supplicant_t *)ptr)->bsscfg->BSSID)
#define BSS_SSID(ptr)	(((supplicant_t *)ptr)->bsscfg->current_bss->SSID)
#define BSS_SSID_LEN(ptr)	(((supplicant_t *)ptr)->bsscfg->current_bss->SSID_len)
#define OSH(ptr)	(((supplicant_t *)ptr)->osh)

#if defined(WLFBT)
/* Transaction Sequence Numbers for FT MIC calculation. */
#define FT_MIC_ASSOC_REQUEST_TSN	5	/* TSN for association request frames. */
#define FT_MIC_REASSOC_RESPONSE_TSN	6	/* TSN for reassociation request response frames. */

static void wlc_sup_calc_fbt_ptk(supplicant_t *sup);
#ifdef BCMDBG
static void wlc_sup_dump_fbt_keys(supplicant_t *sup, uchar *pmkr0, uchar *pmkr1);
#endif
static bool wlc_sup_parse_ric(uint8 *pbody, int len, uint8 **ricend, int *ric_ie_count);
static uint8 *
wlc_sup_ft_write_protected_ies(uint8 *pbody, int len, bcm_tlv_t *ftie, uint8 *ricdata,
	int ricdata_len);
static bool
wlc_sup_ft_calc_mic(supplicant_t *sup, dot11_ft_ie_t *ftie, bcm_tlv_t *mdie, bcm_tlv_t *rsnie,
	uint8 *ricdata, int ricdata_len, int ric_ie_count, uint8 trans_seq_nbr, uint8* mic,
	uint16* mic_control);
#endif /* WLFBT */

#if defined(BCMSUP_PSK) || defined(BCMAUTH_PSK)

/* Get an EAPOL packet and fill in some of the common fields */
void *
wlc_eapol_pktget(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct ether_addr *da,
	uint len)
{
	osl_t *osh = wlc->osh;
	void *p;
	eapol_header_t *eapol_hdr;

	if ((p = PKTGET(osh, len + TXOFF, TRUE)) == NULL) {
		WL_ERROR(("wl%d: %s: pktget error for len %d\n",
		          wlc->pub->unit, __FUNCTION__, len));
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		return (NULL);
	}
	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));

	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, len);

	/* fill in common header fields */
	eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
	bcopy(da, &eapol_hdr->eth.ether_dhost, ETHER_ADDR_LEN);
	bcopy(&bsscfg->cur_etheraddr, &eapol_hdr->eth.ether_shost, ETHER_ADDR_LEN);
	eapol_hdr->eth.ether_type = hton16(ETHER_TYPE_802_1X);
#ifdef BCMSUP_PSK
	if (IS_WPA2_AUTH(bsscfg->WPA_auth)) {
		if ((wlc->sup_wpa2_eapver == -1) && (bsscfg->sup != NULL) &&
		    ((supplicant_t *)bsscfg->sup)->ap_eapver) {
			eapol_hdr->version = ((supplicant_t *)bsscfg->sup)->ap_eapver;
		} else if (wlc->sup_wpa2_eapver == 1) {
			eapol_hdr->version = WPA_EAPOL_VERSION;
		} else {
			eapol_hdr->version = WPA2_EAPOL_VERSION;
		}
	} else
#endif /* defined(BCMSUP_PSK) */
	eapol_hdr->version = WPA_EAPOL_VERSION;
	return p;
}
#endif	


#ifdef BCMINTSUP
/* Look for AP's and STA's IE list in probe response and assoc req */
void
wlc_find_sup_auth_ies(supplicant_t *sup, uint8 **sup_ies, uint *sup_ies_len,
	uint8 **auth_ies, uint *auth_ies_len)
{
	wlc_bsscfg_t *cfg = sup->bsscfg;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *current_bss = cfg->current_bss;

	if ((current_bss->bcn_prb == NULL) ||
	    (current_bss->bcn_prb_len <= sizeof(struct dot11_bcn_prb))) {
		*auth_ies = NULL;
		*auth_ies_len = 0;
	} else {
		*auth_ies = (uint8 *)&current_bss->bcn_prb[1];
		*auth_ies_len = current_bss->bcn_prb_len - sizeof(struct dot11_bcn_prb);
	}

	if ((as->req == NULL) || as->req_len == 0) {
		*sup_ies = NULL;
		*sup_ies_len = 0;
	} else {

		*sup_ies = (uint8 *)&as->req[1];	/* position past hdr */
		*sup_ies_len = as->req_len;

		/* If this was a re-assoc, there's another ether addr to skip */
		if (as->req_is_reassoc) {
			*sup_ies_len -= ETHER_ADDR_LEN;
			*sup_ies += ETHER_ADDR_LEN;
		}
		*sup_ies_len -= sizeof(struct dot11_assoc_req);
	}
}
#endif /* BCMINTSUP */

#ifdef BCMSUP_PSK

void
wlc_wpa_senddeauth(wlc_bsscfg_t *bsscfg, char *da, int reason)
{
#ifdef BCMINTSUP
	scb_val_t scb_val;

	bzero(&scb_val, sizeof(scb_val_t));
	bcopy(da, &scb_val.ea, ETHER_ADDR_LEN);
	scb_val.val = (uint32) reason;
	wlc_ioctl(bsscfg->wlc, WLC_SCB_DEAUTHENTICATE_FOR_REASON,
	          &scb_val, sizeof(scb_val_t), bsscfg->wlcif);
#else
	DEAUTHENTICATE(bsscfg, reason);
#endif /* BCMINTSUP */
}

static void *
wlc_wpa_sup_prepeapol(supplicant_t *sup, uint16 flags, wpa_msg_t msg);

/* Build and send an EAPOL WPA key message */
static bool
wlc_wpa_sup_sendeapol(supplicant_t *sup, uint16 flags, wpa_msg_t msg)
{
#ifdef BCMINTSUP
	wlc_info_t *wlc = sup->wlc;
#endif
	void * p;

	p = wlc_wpa_sup_prepeapol(sup, flags, msg);

	if (p != NULL) {

#ifdef BCMINTSUP
		wlc_sendpkt(wlc, p, sup->bsscfg->wlcif);
#else
		(void)SEND_PKT(sup->bsscfg, p);
#endif
		return TRUE;
	}
	return FALSE;
}

static void *
wlc_wpa_sup_prepeapol(supplicant_t *sup, uint16 flags, wpa_msg_t msg)
{
	uint16 len, key_desc, fbt_len = 0;
	void *p = NULL;
	eapol_header_t *eapol_hdr = NULL;
	eapol_wpa_key_header_t *wpa_key = NULL;
	uchar mic[PRF_OUTBUF_LEN];
	osl_t *osh;
	wpapsk_t *wpa = sup->wpa;

	osh = OSH(sup);
	BCM_REFERENCE(osh);

	len = EAPOL_HEADER_LEN + EAPOL_WPA_KEY_LEN;
	switch (msg) {
	case PMSG2:		/* pair-wise msg 2 */
		if (wpa->sup_wpaie == NULL)
			break;
		if (wpa->ini_fbt)
			fbt_len = sizeof(wpa_pmkid_list_t) + wpa->ftie_len +
				sizeof(dot11_mdid_ie_t);
		len += wpa->sup_wpaie_len + fbt_len;
		if ((p = wlc_eapol_pktget(sup->wlc, sup->bsscfg, &PEER_EA(sup), len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(len - EAPOL_HEADER_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store((flags | PMSG2_REQUIRED), (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->tk_len, (uint8 *)&wpa_key->key_len);
		bcopy(wpa->snonce, wpa_key->nonce, EAPOL_WPA_KEY_NONCE_LEN);
		wpa_key->data_len = hton16(wpa->sup_wpaie_len + fbt_len);
		bcopy(wpa->sup_wpaie, wpa_key->data, wpa->sup_wpaie_len);
		if (wpa->ini_fbt) {
			wpa_pmkid_list_t *pmkid;
			wpa_key->data[1] += sizeof(wpa_pmkid_list_t);

			pmkid = (wpa_pmkid_list_t *)&wpa_key->data[wpa->sup_wpaie_len];
			pmkid->count.low = 1;
			pmkid->count.high = 0;
			bcopy(wpa->pmkr1name, &pmkid->list[0], WPA2_PMKID_LEN);
			bcopy(wpa->mdie, &pmkid[1], sizeof(dot11_mdid_ie_t));
			bcopy(wpa->ftie, (uint8 *)&pmkid[1] + sizeof(dot11_mdid_ie_t),
			      wpa->ftie_len);
		}
		WL_WSEC(("wl%d: wlc_wpa_sup_sendeapol: sending message 2\n",
			sup->wlc->pub->unit));
		break;

	case PMSG4:		/* pair-wise msg 4 */
		if ((p = wlc_eapol_pktget(sup->wlc, sup->bsscfg, &PEER_EA(sup), len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store((flags | PMSG4_REQUIRED), (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->tk_len, (uint8 *)&wpa_key->key_len);
		WL_WSEC(("wl%d: wlc_wpa_sup_sendeapol: sending message 4\n",
			sup->wlc->pub->unit));
		break;

	case GMSG2:	       /* group msg 2 */
		if ((p = wlc_eapol_pktget(sup->wlc, sup->bsscfg, &PEER_EA(sup), len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store((flags | GMSG2_REQUIRED), (uint8 *)&wpa_key->key_info);
		hton16_ua_store(wpa->gtk_len, (uint8 *)&wpa_key->key_len);
		break;

	case MIC_FAILURE:	/* MIC failure report */
		if ((p = wlc_eapol_pktget(sup->wlc, sup->bsscfg, &PEER_EA(sup), len)) == NULL)
			break;
		eapol_hdr = (eapol_header_t *) PKTDATA(osh, p);
		eapol_hdr->length = hton16(EAPOL_WPA_KEY_LEN);
		wpa_key = (eapol_wpa_key_header_t *) eapol_hdr->body;
		bzero(wpa_key, EAPOL_WPA_KEY_LEN);
		hton16_ua_store(flags, (uint8 *)&wpa_key->key_info);
		break;

	default:
		WL_WSEC(("wl%d: wlc_wpa_sup_sendeapol: unexpected message type %d\n",
		         UNIT(sup), msg));
		break;
	}

	if (p != NULL) {
		/* do common message fields here; make and copy MIC last. */
		eapol_hdr->type = EAPOL_KEY;
		if (IS_WPA2_AUTH(wpa->WPA_auth))
			wpa_key->type = EAPOL_WPA2_KEY;
		else
			wpa_key->type = EAPOL_WPA_KEY;
		bcopy(wpa->replay, wpa_key->replay, EAPOL_KEY_REPLAY_LEN);
		/* If my counter is one greater than the last one of his I
		 * used, then a ">=" test on receipt works AND the problem
		 * of zero at the beginning goes away.  Right?
		 */
		wpa_incr_array(wpa->replay, EAPOL_KEY_REPLAY_LEN);
		key_desc = flags & (WPA_KEY_DESC_V1 |  WPA_KEY_DESC_V2);
		if (!wpa_make_mic(eapol_hdr, key_desc, wpa->eapol_mic_key,
			mic)) {
			WL_WSEC(("wl%d: wlc_wpa_sup_sendeapol: MIC generation failed\n",
			         UNIT(sup)));
			return FALSE;
		}
		bcopy(mic, wpa_key->mic, EAPOL_WPA_KEY_MIC_LEN);
	}
	return p;
}




/* plumb the group key */
uint32
wlc_wpa_plumb_gtk(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *gtk, uint32 gtk_len,
	uint32 key_index, uint32 cipher, uint8 *rsc, bool primary_key)
{
	wl_wsec_key_t *key;
	uint32 ret_index;

	WL_WSEC(("wlc_wpa_plumb_gtk\n"));

	if (!(key = MALLOC(wlc->osh, sizeof(wl_wsec_key_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__,  MALLOCED(wlc->osh)));
		return (uint32)(-1);
	}

	bzero(key, sizeof(wl_wsec_key_t));
	key->index = key_index;
	/* NB: wlc_insert_key() will re-infer key->algo from key_len */
	key->algo = cipher;
	key->len = gtk_len;
	bcopy(gtk, key->data, key->len);

	if (primary_key)
		key->flags |= WL_PRIMARY_KEY;


	/* Extract the Key RSC in an Endian independent format */
	key->iv_initialized = 1;
	if (rsc != NULL) {
		/* Extract the Key RSC in an Endian independent format */
		key->rxiv.lo = (((rsc[1] << 8) & 0xFF00) |
			(rsc[0] & 0x00FF));
		key->rxiv.hi = (((rsc[5] << 24) & 0xFF000000) |
			((rsc[4] << 16) & 0x00FF0000) |
			((rsc[3] << 8) & 0x0000FF00) |
			((rsc[2]) & 0x000000FF));
	} else {
		key->rxiv.lo = bsscfg->wpa_none_txiv.lo;
		key->rxiv.hi = bsscfg->wpa_none_txiv.hi;
	}

	WL_WSEC(("wl%d: wlc_wpa_plumb_gtk: Group Key is stored as Low :0x%x,"
	         " High: 0x%x\n", wlc->pub->unit, key->rxiv.lo, key->rxiv.hi));

#ifdef BCMINTSUP
	wlc_ioctl(wlc, WLC_SET_KEY, key, sizeof(wl_wsec_key_t), bsscfg->wlcif);
	if (key->index != key_index) {
		WL_WSEC(("%s(): key_index changed from %d to %d\n",
			__FUNCTION__, key_index, key->index));
	}
#else
	(void)PLUMB_GTK(key, bsscfg);
#endif
	ret_index = key->index;
	MFREE(wlc->osh, key, sizeof(wl_wsec_key_t));
	return ret_index;
}

#if defined(BCMSUP_PSK)
void
wlc_wpa_send_sup_status(supplicant_t *sup, uint reason)
{
	uint status = wlc_sup_get_auth_status_extended(sup);
	if (status != WLC_SUP_DISCONNECTED) {
#ifdef BCMINTSUP
		wlc_bss_mac_event(sup->wlc, sup->bsscfg, WLC_E_PSK_SUP,
		                  NULL, status, reason, 0, 0, 0);
#else
		wpaif_forward_mac_event_cb(sup->bsscfg, reason, status);
#endif /* BCMINTSUP */
	}
}
#endif 

static bool
wlc_wpa_sup_eapol(supplicant_t *sup, eapol_header_t *eapol, bool encrypted)
{
	eapol_wpa_key_header_t *body = (eapol_wpa_key_header_t *)eapol->body;
	uint16 key_info, key_len, data_len;
	uint16 cipher;
	uint16 prohibited, required;
	wpapsk_t *wpa = sup->wpa;

	WL_WSEC(("wl%d: wlc_wpa_sup_eapol: received EAPOL_WPA_KEY packet\n",
	         UNIT(sup)));

	key_info = ntoh16_ua(&body->key_info);

	if ((key_info & WPA_KEY_PAIRWISE) && !(key_info & WPA_KEY_MIC)) {
		/* This is where cipher checks would be done for WDS.
		 * See what NAS' nsup does when that's needed.
		 */
	}

	/* check for replay */
	if (wpa_array_cmp(MAX_ARRAY, body->replay, wpa->replay, EAPOL_KEY_REPLAY_LEN) ==
	    wpa->replay) {
#if defined(BCMDBG) || defined(WLMSG_WSEC)
		uchar *g = body->replay, *s = wpa->replay;
		WL_WSEC(("wl%d: wlc_wpa_sup_eapol: ignoring replay "
				 "(got %02x%02x%02x%02x%02x%02x%02x%02x"
				 " last saw %02x%02x%02x%02x%02x%02x%02x%02x)\n", UNIT(sup),
				 g[0], g[1], g[2], g[3], g[4], g[5], g[6], g[7],
				 s[0], s[1], s[2], s[3], s[4], s[5], s[6], s[7]));
#endif /* BCMDBG || WLMSG_WSEC */

		return TRUE;
	}

	/* check message MIC */
	if ((key_info & WPA_KEY_MIC) &&
	    !wpa_check_mic(eapol, key_info & (WPA_KEY_DESC_V1|WPA_KEY_DESC_V2),
	                   wpa->eapol_mic_key)) {
		/* 802.11-2007 clause 8.5.3.3 - silently discard MIC failure */
		WL_WSEC(("wl%d: wlc_wpa_sup_eapol: MIC failure, discarding pkt\n",
		         UNIT(sup)));
		return TRUE;
	}

	/* if MIC was okay, save replay counter */
	/* last_replay is NOT incremented after transmitting a message */
	bcopy(body->replay, wpa->replay, EAPOL_KEY_REPLAY_LEN);
	bcopy(body->replay, wpa->last_replay, EAPOL_KEY_REPLAY_LEN);

	/* decrypt key data field */
	if (IS_WPA2_AUTH(wpa->WPA_auth) &&
	    (key_info & WPA_KEY_ENCRYPTED_DATA)) {

		uint8 *data, *encrkey;
		rc4_ks_t *rc4key;
		bool decr_status;

		if (!(data = MALLOC(sup->osh, WPA_KEY_DATA_LEN_256))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_DECRYPT_KEY_DATA);
			return FALSE;
		}
		if (!(encrkey = MALLOC(sup->osh, WPA_MIC_KEY_LEN*2))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
			MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_DECRYPT_KEY_DATA);
			return FALSE;
		}
		if (!(rc4key = MALLOC(sup->osh, sizeof(rc4_ks_t)))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
			MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
			MFREE(sup->osh, encrkey, WPA_MIC_KEY_LEN*2);
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_DECRYPT_KEY_DATA);
			return FALSE;
		}

		decr_status = wpa_decr_key_data(body, key_info,
		                       wpa->eapol_encr_key, NULL, data, encrkey, rc4key);

		MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
		MFREE(sup->osh, encrkey, WPA_MIC_KEY_LEN*2);
		MFREE(sup->osh, rc4key, sizeof(rc4_ks_t));

		if (!decr_status) {
			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: decryption of key"
					"data failed\n", UNIT(sup)));
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_DECRYPT_KEY_DATA);
			return FALSE;
		}
	}

	key_len = ntoh16_ua(&body->key_len);
	cipher = CRYPTO_ALGO_OFF;

	if (IS_WPA_AUTH(wpa->WPA_auth) || (key_info & WPA_KEY_PAIRWISE)) {

		/* Infer cipher from message key_len.  Association shouldn't have
		 * succeeded without checking that the cipher is okay, so this is
		 * as good a way as any to find it here.
		 */
		switch (key_len) {
		case TKIP_KEY_SIZE:
			cipher = CRYPTO_ALGO_TKIP;
			break;
		case AES_KEY_SIZE:
			cipher = CRYPTO_ALGO_AES_CCM;
			break;
		case WEP128_KEY_SIZE:
			if (!(key_info & WPA_KEY_PAIRWISE)) {
				cipher = CRYPTO_ALGO_WEP128;
				break;
			} else {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: illegal use of ucast WEP128\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_BAD_UCAST_WEP128);
				return FALSE;
			}
		case WEP1_KEY_SIZE:
			if (!(key_info & WPA_KEY_PAIRWISE)) {
				cipher = CRYPTO_ALGO_WEP1;
				break;
			} else {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: illegal use of ucast WEP40\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_BAD_UCAST_WEP40);
				return FALSE;
			}
		default:
			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: unsupported key_len = %d\n",
			         UNIT(sup), key_len));
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_UNSUP_KEY_LEN);
			return FALSE;
		}
	}

	if (key_info & WPA_KEY_PAIRWISE) {
		if (wpa->ucipher != cipher) {
			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: unicast cipher mismatch in pairwise key"
			         " message\n",
			         UNIT(sup)));
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_PW_KEY_CIPHER);
			return FALSE;
		}

		if (!(key_info & WPA_KEY_MIC)) {

			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: processing message 1\n",
			         UNIT(sup)));

			/* Test message 1 key_info flags */
			prohibited = encrypted ? (PMSG1_PROHIBITED & ~WPA_KEY_SECURE)
				: PMSG1_PROHIBITED;
			required = encrypted ? (PMSG1_REQUIRED & ~WPA_KEY_SECURE) : PMSG1_REQUIRED;
			if (((key_info & required) != required) || ((key_info & prohibited) != 0)) {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: unexpected key_info (0x%04x) in"
				         " WPA pairwise key message 1\n",
				         UNIT(sup), (uint)key_info));
			}
			wpa->state = WPA_SUP_STAKEYSTARTP_PREP_M2;

			if ((!WLFBT_ENAB(sup->pub) || (!wpa->ini_fbt)) &&
				(wpa->WPA_auth == WPA2_AUTH_UNSPECIFIED)) {
				eapol_wpa2_encap_data_t *data_encap;

				/* extract PMKID */
				data_len = ntoh16_ua(&body->data_len);
				data_encap = wpa_find_kde(body->data, data_len,
				                          WPA2_KEY_DATA_SUBTYPE_PMKID);
				if (data_encap) {
					uint i;

#if defined(BCMDBG) || defined(WLMSG_WSEC)
					if (WL_WSEC_ON()) {
						WL_WSEC(("wl%d: PMKID received: ", UNIT(sup)));
						for (i = 0; i < WPA2_PMKID_LEN; i++)
							WL_WSEC(("0x%x ", data_encap->data[i]));
						WL_WSEC(("\n"));
					}
#endif /* BCMDBG || WLMSG_WSEC */

					/* retrieve PMK from supplicant PMKID store */
					for (i = 0; i < sup->npmkid; i++) {
						if (!bcmp(data_encap->data, sup->pmkid[i].PMKID,
						          WPA2_PMKID_LEN) &&
						    !bcmp(&BSS_EA(sup), &sup->pmkid[i].BSSID,
						          ETHER_ADDR_LEN)) {
							bcopy(sup->pmkid[i].PMK, sup->wpa_info->pmk,
								PMK_LEN);
							sup->wpa_info->pmk_len = PMK_LEN;
							break;
						}
					}
					if (i == sup->npmkid) {
						WL_WSEC(("wl%d: wlc_wpa_sup_eapol: unrecognized"
						         " PMKID in WPA pairwise key message 1\n",
						         UNIT(sup)));
						return TRUE;
					}
				}
			}
			if (sup->wpa_info->pmk_len == 0) {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: No PMK available to compose"
				         " pairwise msg 2\n",
				         UNIT(sup)));
				return TRUE;
			}

			/* Save Anonce, generate Snonce, and produce PTK */
			bcopy(body->nonce, wpa->anonce, sizeof(wpa->anonce));
			wlc_getrand(sup->wlc, wpa->snonce, EAPOL_WPA_KEY_NONCE_LEN);
			{


#if defined(WLFBT)
				if (WLFBT_ENAB(sup->pub) && (wpa->ini_fbt))
					wlc_sup_calc_fbt_ptk(sup);
				else
#endif /* WLFBT */

#ifdef MFP
				if ((sup->bsscfg->wsec & MFP_SHA256) ||
				((sup->bsscfg->current_bss->wpa2.flags & RSN_FLAGS_SHA256) &&
					sup->bsscfg->wsec & MFP_CAPABLE))
					kdf_calc_ptk(&PEER_EA(sup), &CUR_EA(sup),
					       wpa->anonce, wpa->snonce,
					       sup->wpa_info->pmk, (uint)sup->wpa_info->pmk_len,
					       wpa->eapol_mic_key, (uint)wpa->ptk_len);
				else
#endif
				{
					if (!memcmp(&PEER_EA(sup), &CUR_EA(sup), ETHER_ADDR_LEN)) {
						/* something is wrong -- toss; invalid eapol */
						WL_WSEC(("wl%d:%s: toss msg; same mac\n",
							UNIT(sup), __FUNCTION__));
						return TRUE;
					} else {
						wpa_calc_ptk(&PEER_EA(sup), &CUR_EA(sup),
							wpa->anonce, wpa->snonce,
							sup->wpa_info->pmk,
							(uint)sup->wpa_info->pmk_len,
							wpa->eapol_mic_key,
							(uint)wpa->ptk_len);
					}
				}
				/* Send pair-wise message 2 */
				if (wlc_wpa_sup_sendeapol(sup, (key_info & PMSG2_MATCH_FLAGS),
				                          PMSG2)) {
					wpa->state = WPA_SUP_STAKEYSTARTP_WAIT_M3;
				} else {
					WL_WSEC(("wl%d: wlc_wpa_sup_eapol: send message 2 failed\n",
					         UNIT(sup)));
					wlc_wpa_send_sup_status(sup, WLC_E_SUP_SEND_FAIL);
				}
			}
		} else {
			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: processing message 3\n", UNIT(sup)));

			/* Test message 3 key_info flags */
			prohibited = (encrypted || sup->wlc->sup_m3sec_ok)
			        ? (PMSG3_PROHIBITED & ~WPA_KEY_SECURE)
				: PMSG3_PROHIBITED;
			required = encrypted ? (PMSG3_REQUIRED & ~WPA_KEY_SECURE) : PMSG3_REQUIRED;

			if (IS_WPA2_AUTH(wpa->WPA_auth)) {
				prohibited = 0;
					required = PMSG3_WPA2_REQUIRED;
			}

			if (((key_info & required) != required) ||
				((key_info & prohibited) != 0))
			{
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: unexpected key_info (0x%04x) in"
				         " WPA pairwise key message 3\n",
				         UNIT(sup), (uint)key_info));
				return TRUE;
			} else if (wpa->state < WPA_SUP_STAKEYSTARTP_PREP_M2 ||
			           wpa->state > WPA_SUP_STAKEYSTARTG_PREP_G2) {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: unexpected 4-way msg 3 in state"
				         " %d\n",
				         UNIT(sup), wpa->state));
				/* don't accept msg3 unless it follows msg1 */
				return TRUE;
			}
			wpa->state = WPA_SUP_STAKEYSTARTP_PREP_M4;

			/* check anonce */
			if (bcmp(body->nonce, wpa->anonce, sizeof(wpa->anonce))) {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: anonce in key message 3 doesn't"
				         " match anonce in key message 1, discarding pkt \n",
				         UNIT(sup)));
				return TRUE;
			}

			/* Test AP's WPA IE against saved one */
			data_len = ntoh16_ua(&body->data_len);
			if (IS_WPA2_AUTH(wpa->WPA_auth)) {
				uint16 len, wpa2ie_len;
				bcm_tlv_t *wpa2ie;
				wpa2ie_len = 0;
				wpa2ie = bcm_parse_tlvs(body->data, data_len, DOT11_MNG_RSN_ID);
				if (wpa2ie)
					/* verify RSN IE */
					/* wpa2ie for initial FBT has an extra pmkr1name in msg3 */
					wpa2ie_len = (wpa->ini_fbt) ?
					wpa2ie->len - sizeof(wpa_pmkid_list_t) : wpa2ie->len;
				if (!wpa2ie || wpa->auth_wpaie_len !=
					((uint16)TLV_HDR_LEN + wpa2ie_len) ||
				    (bcmp((wpa->auth_wpaie+TLV_HDR_LEN), &wpa2ie->data[0],
					wpa2ie_len))) {
					WL_WSEC(("wl%d: wlc_wpa_sup_eapol: WPA IE mismatch in key"
						" message 3\n", UNIT(sup)));
					wlc_wpa_send_sup_status(sup, WLC_E_SUP_MSG3_IE_MISMATCH);
					/* should cause a deauth */
					wlc_wpa_senddeauth(sup->bsscfg, (char *)&PEER_EA(sup),
						DOT11_RC_WPA_IE_MISMATCH);
					return TRUE;
				}
				/* looking for second RSN IE.  deauth if presents */
				len = data_len - (uint16)((uint8*)wpa2ie - (uint8*)body->data);
				if (len > ((uint16)TLV_HDR_LEN + (uint16)wpa2ie->len) &&
					bcm_parse_tlvs((uint8*)wpa2ie + TLV_HDR_LEN + wpa2ie->len,
					len - (TLV_HDR_LEN + wpa2ie->len), DOT11_MNG_RSN_ID)) {
					WL_WSEC(("wl%d: wlc_wpa_sup_eapol: WPA IE contains more"
						" than one RSN IE in key message 3\n", UNIT(sup)));
					wlc_wpa_send_sup_status(sup, WLC_E_SUP_MSG3_TOO_MANY_IE);
					/* should cause a deauth */
					wlc_wpa_senddeauth(sup->bsscfg,
					                   (char *)&PEER_EA(sup),
					                   DOT11_RC_WPA_IE_MISMATCH);
					return TRUE;
				}
			}
			else if ((wpa->auth_wpaie_len != data_len) ||
			         (bcmp(wpa->auth_wpaie, body->data,
			               wpa->auth_wpaie_len))) {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: WPA IE mismatch in key message"
				         " 3\n",
				         UNIT(sup)));
				/* should cause a deauth */
				wlc_wpa_senddeauth(sup->bsscfg, (char *)&PEER_EA(sup),
				                   DOT11_RC_WPA_IE_MISMATCH);
				return TRUE;
			}

			if (wlc_wpa_sup_sendeapol(sup, (key_info & PMSG4_MATCH_FLAGS), PMSG4)) {
				wpa->state = WPA_SUP_STAKEYSTARTG_WAIT_G1;
			} else {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: send message 4 failed\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_SEND_FAIL);
				return FALSE;
			}

			if (key_info & WPA_KEY_INSTALL)
				/* Plumb paired key */
				wlc_wpa_plumb_tk(sup->wlc, sup->bsscfg,
					(uint8*)wpa->temp_encr_key,
					wpa->tk_len, wpa->ucipher, &PEER_EA(sup));
			else {
				/* While INSTALL is in the `required' set this
				 * test is a tripwire for when that changes
				 */
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: INSTALL flag unset in 4-way msg"
				         " 3\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_NO_INSTALL_FLAG);
				return FALSE;
			}

			if (IS_WPA2_AUTH(wpa->WPA_auth) &&
			    TRUE) {
				eapol_wpa2_encap_data_t *data_encap;
				eapol_wpa2_key_gtk_encap_t *gtk_kde;

				/* extract GTK */
				data_encap = wpa_find_gtk_encap(body->data, data_len);
				if (!data_encap) {
					WL_WSEC(("wl%d: wlc_wpa_sup_eapol: encapsulated GTK missing"
					         " from message 3\n", UNIT(sup)));
					wlc_wpa_send_sup_status(sup, WLC_E_SUP_MSG3_NO_GTK);
					return FALSE;
				}
				wpa->gtk_len = data_encap->length -
				    ((EAPOL_WPA2_ENCAP_DATA_HDR_LEN - TLV_HDR_LEN) +
				     EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN);
				gtk_kde = (eapol_wpa2_key_gtk_encap_t *)data_encap->data;
				wpa->gtk_index = (gtk_kde->flags & WPA2_GTK_INDEX_MASK) >>
				    WPA2_GTK_INDEX_SHIFT;
				bcopy(gtk_kde->gtk, wpa->gtk, wpa->gtk_len);

				/* plumb GTK */
				wlc_wpa_plumb_gtk(sup->wlc, sup->bsscfg, wpa->gtk,
					wpa->gtk_len, wpa->gtk_index, cipher, body->rsc,
					gtk_kde->flags & WPA2_GTK_TRANSMIT);
#ifdef MFP
				if (WLC_MFP_ENAB(sup->wlc))
					wlc_mfp_extract_igtk(SUPMFP(sup), sup->bsscfg, eapol);
#endif
			}
			if (IS_WPA2_AUTH(wpa->WPA_auth)) {
				wpa->state = WPA_SUP_KEYUPDATE;

				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: WPA2 key update complete\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_OTHER);

				/* Authorize scb for data */
#ifdef BCMINTSUP
				(void)wlc_ioctl(sup->wlc, WLC_SCB_AUTHORIZE,
					&PEER_EA(sup), ETHER_ADDR_LEN, sup->bsscfg->wlcif);
				wpa->ini_fbt = FALSE;
#else
				AUTHORIZE(sup->bsscfg);
#endif

			} else
				wpa->state = WPA_SUP_STAKEYSTARTG_WAIT_G1;
		}

	} else {
		/* Pairwise flag clear; should be group key message. */
		if (wpa->state <  WPA_SUP_STAKEYSTARTG_WAIT_G1) {
			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: unexpected group key msg1 in state %d\n",
			         UNIT(sup), wpa->state));
			return TRUE;
		}

		if (SUP_CHECK_MCIPHER(sup)) {
			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: multicast cipher mismatch in group key"
			         " message\n",
			         UNIT(sup)));
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_GRP_KEY_CIPHER);
			return FALSE;
		}

		if ((key_info & GMSG1_REQUIRED) != GMSG1_REQUIRED) {
			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: unexpected key_info (0x%04x)in"
				 "WPA group key message\n",
				 UNIT(sup), (uint)key_info));
			return TRUE;
		}

		wpa->state = WPA_SUP_STAKEYSTARTG_PREP_G2;
		if (IS_WPA2_AUTH(wpa->WPA_auth)) {
			eapol_wpa2_encap_data_t *data_encap;
			eapol_wpa2_key_gtk_encap_t *gtk_kde;

			/* extract GTK */
			data_len = ntoh16_ua(&body->data_len);
			data_encap = wpa_find_gtk_encap(body->data, data_len);
			if (!data_encap) {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: encapsulated GTK missing from"
					" group message 1\n", UNIT(sup)));
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_GRP_MSG1_NO_GTK);
				return FALSE;
			}
			wpa->gtk_len = data_encap->length - ((EAPOL_WPA2_ENCAP_DATA_HDR_LEN -
			                                          TLV_HDR_LEN) +
			                                         EAPOL_WPA2_KEY_GTK_ENCAP_HDR_LEN);
			gtk_kde = (eapol_wpa2_key_gtk_encap_t *)data_encap->data;
			wpa->gtk_index = (gtk_kde->flags & WPA2_GTK_INDEX_MASK) >>
			    WPA2_GTK_INDEX_SHIFT;
			bcopy(gtk_kde->gtk, wpa->gtk, wpa->gtk_len);

			/* plumb GTK */
			wlc_wpa_plumb_gtk(sup->wlc, sup->bsscfg, wpa->gtk, wpa->gtk_len,
				wpa->gtk_index, wpa->mcipher, body->rsc,
				gtk_kde->flags & WPA2_GTK_TRANSMIT);
#ifdef MFP
			if (WLC_MFP_ENAB(sup->wlc))
				wlc_mfp_extract_igtk(SUPMFP(sup), sup->bsscfg, eapol);
#endif
		} else {

			uint8 *data, *encrkey;
			rc4_ks_t *rc4key;
			bool decr_status;

			if (!(data = MALLOC(sup->osh, WPA_KEY_DATA_LEN_256))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_GTK_DECRYPT_FAIL);
				return FALSE;
			}
			if (!(encrkey = MALLOC(sup->osh, WPA_MIC_KEY_LEN*2))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
				MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_GTK_DECRYPT_FAIL);
				return FALSE;
			}
			if (!(rc4key = MALLOC(sup->osh, sizeof(rc4_ks_t)))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
				MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
				MFREE(sup->osh, encrkey, WPA_MIC_KEY_LEN*2);
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_GTK_DECRYPT_FAIL);
				return FALSE;
			}

			decr_status = wpa_decr_gtk(body, key_info, wpa->eapol_encr_key,
			                  wpa->gtk, data, encrkey, rc4key);

			MFREE(sup->osh, data, WPA_KEY_DATA_LEN_256);
			MFREE(sup->osh, encrkey, WPA_MIC_KEY_LEN*2);
			MFREE(sup->osh, rc4key, sizeof(rc4_ks_t));


			wpa->gtk_len = key_len;
			if (!decr_status) {
				WL_WSEC(("wl%d: wlc_wpa_sup_eapol: GTK decrypt failure\n",
				         UNIT(sup)));
				wlc_wpa_send_sup_status(sup, WLC_E_SUP_GTK_DECRYPT_FAIL);
				return FALSE;
			}

			/* plumb GTK */
			wlc_wpa_plumb_gtk(sup->wlc, sup->bsscfg, wpa->gtk, wpa->gtk_len,
				(key_info & WPA_KEY_INDEX_MASK) >> WPA_KEY_INDEX_SHIFT,
				cipher, body->rsc, key_info & WPA_KEY_INSTALL);
		}

		/* send group message 2 */
		if (wlc_wpa_sup_sendeapol(sup, (key_info & GMSG2_MATCH_FLAGS), GMSG2)) {
			wpa->state = WPA_SUP_KEYUPDATE;

			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: key update complete\n",
			         UNIT(sup)));
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_OTHER);
		} else {
			WL_WSEC(("wl%d: wlc_wpa_sup_eapol: send grp msg 2 failed\n",
			         UNIT(sup)));
			wlc_wpa_send_sup_status(sup, WLC_E_SUP_SEND_FAIL);
		}

		/* Authorize scb for data */
#ifdef BCMINTSUP
		(void)wlc_ioctl(sup->wlc, WLC_SCB_AUTHORIZE,
			&PEER_EA(sup), ETHER_ADDR_LEN, sup->bsscfg->wlcif);
#else
		AUTHORIZE(sup->bsscfg);
#endif


	}
	return TRUE;
}

#ifdef BCMINTSUP
/* Break a lengthy passhash algorithm into smaller pieces. It is necessary
 * for dongles with under-powered CPUs.
 */
static void
wlc_sup_wpa_passhash_timer(void *arg)
{
	supplicant_t *sup = (supplicant_t *)arg;
	wpapsk_info_t *info = sup->wpa_info;

	if (do_passhash(&info->passhash_states, 256) == 0) {
		WL_WSEC(("wl%d: passhash is done\n", UNIT(sup)));
		get_passhash(&info->passhash_states, info->pmk, PMK_LEN);
		info->pmk_len = PMK_LEN;
		wlc_join_bss_prep(sup->bsscfg);
		return;
	}

	WL_WSEC(("wl%d: passhash is in progress\n", UNIT(sup)));
	wl_add_timer(info->wlc->wl, info->passhash_timer, 0, 0);
}
#endif /* BCMINTSUP */


static bool
wlc_sup_wpapsk_start(supplicant_t *sup, uint8 *sup_ies, uint sup_ies_len,
	uint8 *auth_ies, uint auth_ies_len)
{
	bool ret = TRUE;
	wpapsk_t *wpa;

	wpa = sup->wpa;
	wlc_wpapsk_free(sup->wlc, wpa);

	wpa->state = WPA_SUP_INITIALIZE;

	if (SUP_CHECK_WPAPSK_SUP_TYPE(sup)) {
		wpa->WPA_auth = sup->bsscfg->WPA_auth;
	}

	if (!wlc_wpapsk_start(sup->wlc, wpa, sup_ies, sup_ies_len, auth_ies, auth_ies_len)) {
		WL_ERROR(("wl%d: wlc_wpapsk_start() failed\n",
			sup->wlc->pub->unit));
		return FALSE;
	}

	if ((sup->sup_type == SUP_WPAPSK) && (sup->wpa_info->pmk_len == 0)) {
		WL_WSEC(("wl%d: wlc_sup_wpapsk_start: no PMK material found\n", UNIT(sup)));
		ret = FALSE;
	}

	/* Parse assoc resp for mdie and ftie for initial fbt association */
	if (WLFBT_ENAB(sup->pub) && IS_WPA2_AUTH(wpa->WPA_auth)) {
		wlc_bsscfg_t *cfg = sup->bsscfg;
		wlc_assoc_t *as = cfg->assoc;
		bcm_tlv_t *mdie, *ftie;
		int ies_len;
		uchar * assoc_ies = (uchar *)&as->resp[1];

		ies_len = as->resp_len - sizeof(struct dot11_assoc_resp);
		if ((mdie = bcm_parse_tlvs(assoc_ies, ies_len, DOT11_MNG_MDIE_ID)) != NULL) {
			wpa->mdie_len = mdie->len + TLV_HDR_LEN;
			wpa->mdie = MALLOC(sup->osh, wpa->mdie_len);
			if (!wpa->mdie) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					sup->pub->unit, __FUNCTION__, MALLOCED(sup->osh)));
				return FALSE;
			}
			bcopy(mdie, wpa->mdie, wpa->mdie_len);
			wpa->mdid = ltoh16_ua(&mdie->data[0]);
		}
		if ((ftie = bcm_parse_tlvs(assoc_ies, ies_len, DOT11_MNG_FTIE_ID)) != NULL) {
			uchar *tlvs;
			uint tlv_len;

			wpa->ftie_len = ftie->len + TLV_HDR_LEN;
			wpa->ftie = MALLOC(sup->osh, wpa->ftie_len);
			if (!wpa->ftie) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					sup->pub->unit, __FUNCTION__, MALLOCED(sup->osh)));
				return FALSE;
			}
			bcopy(ftie, wpa->ftie, wpa->ftie_len);
			tlvs = (uchar *)((uintptr)wpa->ftie + sizeof(dot11_ft_ie_t));
			tlv_len = wpa->ftie_len - sizeof(dot11_ft_ie_t);
			wpa->r1khid = bcm_parse_tlvs(tlvs, tlv_len, 1);
			wpa->r0khid = bcm_parse_tlvs(tlvs, tlv_len, 3);
		}

		wpa->ini_fbt = (mdie && ftie) ? TRUE : FALSE;
	}

	return ret;
}

#ifdef BCMINTSUP
/* return 0 when succeeded, 1 when passhash is in progress, -1 when failed */
int
wlc_sup_set_ssid(supplicant_t *sup, uchar ssid[], int len)
{
	if (sup == NULL) {
		WL_WSEC(("wlc_sup_set_ssid: called with NULL sup\n"));
		return -1;
	} else if (sup->wpa_info->psk_len == 0) {
		WL_WSEC(("wlc_sup_set_ssid: called with NULL psk\n"));
		return 0;
	} else if (sup->wpa_info->pmk_len != 0) {
		WL_WSEC(("wlc_sup_set_ssid: called with non-NULL pmk\n"));
		return 0;
	}
	return wlc_wpa_cobble_pmk(sup->wpa_info, (char *)sup->wpa_info->psk,
		sup->wpa_info->psk_len, ssid, len);
}
#endif /* BCMINTSUP */

void
wlc_sup_clear_pmkid_store(supplicant_t *sup)
{
	if (sup == NULL) {
		WL_WSEC(("wlc_sup_clear_pmkid_store called with NULL sup\n"));
	} else
		sup->npmkid = 0;
}

#ifdef BCMINTSUP
void
wlc_sup_pmkid_cache_req(supplicant_t *sup)
{
	uint i, j, k;
	wlc_bsscfg_t *cfg = sup->bsscfg;

	/*
	 * for each element in driver's candidate list
	 * - find matching entries in supplicant's PMKID store
	 * - install in driver's PMKID cache
	 */
	k = 0;
	for (i = 0; i < cfg->npmkid_cand; i++) {
		pmkid_cand_t *pmkid_cand = &cfg->pmkid_cand[i];
		for (j = 0; j < sup->npmkid && k < MAXPMKID; j++) {
			if (!bcmp(&pmkid_cand->BSSID, &sup->pmkid[j].BSSID, ETHER_ADDR_LEN)) {
				/* install in driver's PMKID cache */
				bcopy(&sup->pmkid[j].BSSID, &cfg->pmkid[k].BSSID, ETHER_ADDR_LEN);
				bcopy(sup->pmkid[j].PMKID, &cfg->pmkid[k].PMKID, WPA2_PMKID_LEN);

#if defined(BCMDBG) || defined(WLMSG_WSEC)
				if (WL_WSEC_ON()) {
					char eabuf[ETHER_ADDR_STR_LEN];
					uint l;

					WL_WSEC(("wl%d: PMKID[%d]: %s = ", UNIT(sup), k,
					         bcm_ether_ntoa(&cfg->pmkid[k].BSSID,
					                        eabuf)));
					for (l = 0; l < WPA2_PMKID_LEN; l++)
						WL_WSEC(("%02x ", cfg->pmkid[k].PMKID[l]));
					WL_WSEC(("\n"));
				}
#endif /* BCMDBG || WLMSG_WSEC */
				k++;
			}
		}
	}
	cfg->npmkid = k;
}
#endif /* BCMINTSUP */

static int
wlc_sup_set_pmkid(supplicant_t *sup)
{
	struct ether_addr *BSSID = &BSS_EA(sup);
	sup_pmkid_t *pmkid = NULL;
	uint i;
	uint8 *data, *digest;

	if (ETHER_ISNULLADDR(BSSID)) {
		WL_WSEC(("wl%d: wlc_sup_set_pmk: can't calculate PMKID - NULL BSSID\n",
			UNIT(sup)));
		return 0;
	}

	/* Overwrite existing PMKID for this BSSID */
	for (i = 0; i < sup->npmkid; i++) {
		pmkid = &sup->pmkid[i];
		if (!bcmp(BSSID, &pmkid->BSSID, ETHER_ADDR_LEN))
			break;
	}

	/* Add new PMKID to store if no existing PMKID was found */
	if (i == sup->npmkid) {
		if (sup->npmkid == SUP_MAXPMKID) {
			WL_WSEC(("wl%d: wlc_sup_set_pmk: can't calculate PMKID - no room in"
			         " the inn\n", UNIT(sup)));
			return 0;
		} else {
			pmkid = &sup->pmkid[sup->npmkid++];
			bcopy(BSSID, &pmkid->BSSID, ETHER_ADDR_LEN);
		}
	}

	ASSERT(pmkid);
	if (!pmkid) {
		/* Unreachable by construction */
		return 0;
	}

	/* compute PMKID and add to supplicant store */
	bzero(pmkid->PMK, PMK_LEN);
	bcopy(sup->wpa_info->pmk, pmkid->PMK, sup->wpa_info->pmk_len);

	if (!(data = MALLOC(sup->osh, WPA_KEY_DATA_LEN_128))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
		return 0;
	}
	if (!(digest = MALLOC(sup->osh, PRF_OUTBUF_LEN))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			UNIT(sup), __FUNCTION__,  MALLOCED(sup->osh)));
		MFREE(sup->osh, data, WPA_KEY_DATA_LEN_128);
		return 0;
	}

#ifdef MFP
	if (sup->bsscfg->wsec & MFP_CAPABLE)
		kdf_calc_pmkid(BSSID, &CUR_EA(sup), sup->wpa_info->pmk,
		(uint)sup->wpa_info->pmk_len, pmkid->PMKID, data, digest);
	else
#endif
	wpa_calc_pmkid(BSSID, &CUR_EA(sup), sup->wpa_info->pmk,
	               (uint)sup->wpa_info->pmk_len, pmkid->PMKID, data, digest);

	MFREE(sup->osh, data, WPA_KEY_DATA_LEN_128);
	MFREE(sup->osh, digest, PRF_OUTBUF_LEN);

#if defined(BCMDBG) || defined(WLMSG_WSEC)
	if (WL_WSEC_ON()) {
		char eabuf[ETHER_ADDR_STR_LEN];

		WL_WSEC(("wpa_calc_pmkid: auth_ea %s\n",
		         bcm_ether_ntoa(BSSID, eabuf)));
		WL_WSEC(("wpa_calc_pmkid: sup_ea %s\n",
		         bcm_ether_ntoa(&CUR_EA(sup), eabuf)));
		WL_WSEC(("wpa_calc_pmkid: PMK "));
		for (i = 0; i < sup->wpa_info->pmk_len; i++)
			WL_WSEC(("0x%x ", pmkid->PMK[i]));
		WL_WSEC(("\n"));
		WL_WSEC(("wpa_calc_pmkid: PMKID "));
		for (i = 0; i < WPA2_PMKID_LEN; i++)
			WL_WSEC(("0x%x ", pmkid->PMKID[i]));
		WL_WSEC(("\n"));
	}
#endif /* BCMDBG || WLMSG_WSEC */
	return 0;
}

int
wlc_sup_set_pmk(supplicant_t *sup, wsec_pmk_t *pmk, bool assoc)
{
	wpapsk_info_t *info;
	wpapsk_t *wpa;
	wlc_bsscfg_t *bsscfg;

	if (sup == NULL || pmk == NULL) {
		WL_WSEC(("%s: missing required parameter\n", __FUNCTION__));
		return BCME_BADARG;
	}

	bsscfg = sup->bsscfg;
	info = sup->wpa_info;
	wpa = sup->wpa;

	/* Zero length means forget what's there now */
	if (pmk->key_len == 0)
		return 0;

	info->pmk_len = 0;
	info->psk_len = 0;

#ifdef BCMINTSUP
	/* A key that needs hashing has to wait until we see the SSID */
	if (pmk->flags & WSEC_PASSPHRASE) {
		WL_WSEC(("wl%d: %s: saving raw PSK\n",
		         info->wlc->pub->unit, __FUNCTION__));

		if (pmk->key_len == WSEC_MAX_PSK_LEN) {
			info->psk_len = 0;
			/* this size must be legible hex and need not wait */
			if (wlc_wpa_cobble_pmk(info, (char *)pmk->key, pmk->key_len,
				NULL, 0) < 0) {
				return BCME_ERROR;
			}
			else {
				if ((wpa->WPA_auth & WPA2_AUTH_UNSPECIFIED) && assoc) {
					/* Derive the PMKID for EAP connections. */
					wlc_sup_set_pmkid(bsscfg->sup);
				}
				return 0;
			}

		} else if ((pmk->key_len >= WSEC_MIN_PSK_LEN) &&
		    (pmk->key_len < WSEC_MAX_PSK_LEN)) {
			bcopy((char*)pmk->key, info->psk, pmk->key_len);
			info->psk_len = pmk->key_len;
			return 0;
		}
		return BCME_ERROR;
	}
#endif /* BCMINTSUP */

	/* If it's not a passphrase it must be a proper PMK */
	if (pmk->key_len > TKIP_KEY_SIZE) {
		WL_WSEC(("wl%d: %s: unexpected key size (%d)\n",
		         info->wlc->pub->unit, __FUNCTION__, pmk->key_len));
		return BCME_BADARG;
	}

	bcopy((char*)pmk->key, info->pmk, pmk->key_len);
	info->psk_len = 0;
	info->pmk_len = pmk->key_len;


	if ((wpa->WPA_auth & WPA2_AUTH_UNSPECIFIED) && assoc)
		wlc_sup_set_pmkid(bsscfg->sup);

	return 0;
}

void
wlc_sup_send_micfailure(supplicant_t *sup, bool ismulti)
{
	uint16 flags;

	if (sup == NULL) {
		WL_WSEC(("wlc_sup_send_micfailure called with NULL supplicant\n"));
		return;
	}

	flags = (uint16) (MIC_ERROR_REQUIRED | sup->wpa->desc);
	if (!ismulti)
		flags |= (uint16) WPA_KEY_PAIRWISE;
	WL_WSEC(("wl%d: wlc_sup_send_micfailure: sending MIC failure report\n",
	         UNIT(sup)));
	(void) wlc_wpa_sup_sendeapol(sup, flags, MIC_FAILURE);
	return;
}
#endif	/* BCMSUP_PSK */

#if defined(BCMSUP_PSK) || defined(BCMAUTH_PSK)

void
wlc_wpapsk_free(wlc_info_t *wlc, wpapsk_t *wpa)
{
	/* Toss IEs if there are any */
	if (wpa->auth_wpaie != NULL) {
		MFREE(wlc->osh, wpa->auth_wpaie, wpa->auth_wpaie_len);
		wpa->auth_wpaie = NULL;
		wpa->auth_wpaie_len = 0;
	}
	if (wpa->sup_wpaie != NULL) {
		MFREE(wlc->osh, wpa->sup_wpaie, wpa->sup_wpaie_len);
		wpa->sup_wpaie = NULL;
		wpa->sup_wpaie_len = 0;
	}
	if (wpa->mdie != NULL)
		MFREE(wlc->osh, wpa->mdie, wpa->mdie_len);

	if (wpa->ftie != NULL)
		MFREE(wlc->osh, wpa->ftie, wpa->ftie_len);
	bzero(wpa, sizeof(wpapsk_t));
}

bool
wlc_wpapsk_start(wlc_info_t *wlc, wpapsk_t *wpa, uint8 *sup_ies, uint sup_ies_len,
	uint8 *auth_ies, uint auth_ies_len)
{
	uchar *auth_wpaie, *sup_wpaie;
	wpa_suite_t *cipher_suite;
	bool wep_ok = FALSE;
	bool ret = TRUE;

	/* get STA's WPA IE */
	if (IS_WPA2_AUTH(wpa->WPA_auth)) {
		sup_wpaie = (uchar *)bcm_parse_tlvs(sup_ies, sup_ies_len, DOT11_MNG_RSN_ID);
		if (sup_wpaie == NULL) {
			WL_WSEC(("wl%d: wlc_wpapsk_start: STA RSN IE not found in association"
				" request\n", wlc->pub->unit));
			return FALSE;
		}
	} else {
		sup_wpaie = (uchar *)bcm_find_wpaie(sup_ies, sup_ies_len);
		if (sup_wpaie == NULL) {
			WL_WSEC(("wl%d: wlc_wpapsk_start: STA WPA IE not found in sup_ies\n",
				wlc->pub->unit));
			return FALSE;
		}
	}

	/* get AP's WPA IE */
	if (IS_WPA2_AUTH(wpa->WPA_auth)) {
		auth_wpaie = (uchar *)bcm_parse_tlvs(auth_ies, auth_ies_len, DOT11_MNG_RSN_ID);
		if (auth_wpaie == NULL) {
			WL_WSEC(("wl%d: wlc_wpapsk_start: AP RSN IE not found in auth_ies\n",
			         wlc->pub->unit));
			return FALSE;
		}
	} else {
		auth_wpaie = (uchar *)bcm_find_wpaie(auth_ies, auth_ies_len);
		if (auth_wpaie == NULL) {
			WL_WSEC(("wl%d:  wlc_wpapsk_start: AP WPA IE not found in probe response\n",
				wlc->pub->unit));
			return FALSE;
		}
	}

	/* initialize with default ciphers */
	wpa->ucipher = CRYPTO_ALGO_TKIP;
	wpa->mcipher = CRYPTO_ALGO_TKIP;
	if (IS_WPA2_AUTH(wpa->WPA_auth)) {
		wpa->ucipher = CRYPTO_ALGO_AES_CCM;
		wpa->mcipher = CRYPTO_ALGO_AES_CCM;
	}


	/* get ciphers from STA's WPA IE if it's long enough */
	if (IS_WPA2_AUTH(wpa->WPA_auth)) {
		bcm_tlv_t *wpa2ie;
		wpa_suite_mcast_t *mcast;
		wpa_suite_ucast_t *ucast_suites;

		wpa2ie = (bcm_tlv_t *)sup_wpaie;
		mcast = (wpa_suite_mcast_t *)&wpa2ie->data[WPA2_VERSION_LEN];
		ucast_suites = (wpa_suite_ucast_t *)&mcast[1];
		cipher_suite = ucast_suites->list;
		if (!wpa2_cipher(cipher_suite, &wpa->ucipher, wep_ok)) {
			WL_WSEC(("wl%d: wlc_wpapsk_start: unexpected unicast cipher"
			         " %02x:%02x:%02x:%02x\n",
			         wlc->pub->unit, cipher_suite->oui[0],
			         cipher_suite->oui[1], cipher_suite->oui[2],
			         cipher_suite->type));
			return FALSE;
		}
	} else
	if (sup_wpaie[1] + TLV_HDR_LEN > WPA_IE_FIXED_LEN + WPA_SUITE_LEN) {
		wpa_suite_ucast_t *ucast_suites;

		/* room for ucast cipher, so use that */
		ucast_suites = (wpa_suite_ucast_t *)
			((uint8 *)sup_wpaie + WPA_IE_FIXED_LEN + WPA_SUITE_LEN);
		cipher_suite = ucast_suites->list;
		if (!wpa_cipher(cipher_suite, &wpa->ucipher, wep_ok)) {
			WL_WSEC(("wl%d: wlc_wpapsk_start: unexpected unicast cipher"
			         " %02x:%02x:%02x:%02x\n",
			         wlc->pub->unit, cipher_suite->oui[0],
			         cipher_suite->oui[1], cipher_suite->oui[2],
			         cipher_suite->type));
			return FALSE;
		}
	}
	if (!wlc_wpa_set_ucipher(wpa, wpa->ucipher, wep_ok)) {
		return FALSE;
	}

	if (IS_WPA2_AUTH(wpa->WPA_auth)) {
		bcm_tlv_t *wpa2ie;

		wpa2ie = (bcm_tlv_t *)sup_wpaie;
		cipher_suite = (wpa_suite_t *)&wpa2ie->data[WPA2_VERSION_LEN];
		wep_ok = TRUE;
		if (!wpa2_cipher(cipher_suite, &wpa->mcipher, wep_ok)) {
			WL_WSEC(("wl%d: wlc_wpapsk_start: unexpected multicast cipher"
			         " %02x:%02x:%02x:%02x\n",
			         wlc->pub->unit, cipher_suite->oui[0],
			         cipher_suite->oui[1], cipher_suite->oui[2],
			         cipher_suite->type));
			return FALSE;
		}
	} else
	if (sup_wpaie[1] + TLV_HDR_LEN >= WPA_IE_FIXED_LEN + WPA_SUITE_LEN) {
		/* room for a mcast cipher, so use that */
		cipher_suite = (wpa_suite_t *)(sup_wpaie + WPA_IE_FIXED_LEN);
		wep_ok = TRUE;
		if (!wpa_cipher(cipher_suite, &wpa->mcipher, wep_ok)) {
			WL_WSEC(("wl%d: wlc_wpapsk_start: unexpected multicast cipher"
			         " %02x:%02x:%02x:%02x\n",
			         wlc->pub->unit, cipher_suite->oui[0],
			         cipher_suite->oui[1], cipher_suite->oui[2],
			         cipher_suite->type));
			return FALSE;
		}
	}

	/* Save copy of AP's WPA IE */
	wpa->auth_wpaie_len = (uint16) (auth_wpaie[1] + TLV_HDR_LEN);
	wpa->auth_wpaie = (uchar *) MALLOC(wlc->osh, wpa->auth_wpaie_len);
	if (!wpa->auth_wpaie) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return FALSE;
	}
	bcopy(auth_wpaie, wpa->auth_wpaie, wpa->auth_wpaie_len);

	/* Save copy of STA's WPA IE */
	wpa->sup_wpaie_len = (uint16) (sup_wpaie[1] + TLV_HDR_LEN);
	wpa->sup_wpaie = (uchar *) MALLOC(wlc->osh, wpa->sup_wpaie_len);
	if (!wpa->sup_wpaie) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return FALSE;
	}
	bcopy(sup_wpaie, wpa->sup_wpaie, wpa->sup_wpaie_len);

	return ret;
}

/* plumb the pairwise key */
void
wlc_wpa_plumb_tk(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *tk, uint32 tk_len,
	uint32 cipher, struct ether_addr *ea)
{
	wl_wsec_key_t *key;
#ifdef BCMINTSUP
	int err;
#endif

	if (!(key = MALLOC(wlc->osh, sizeof(wl_wsec_key_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__,  MALLOCED(wlc->osh)));
		return;
	}

	WL_WSEC(("wlc_wpa_plumb_tk\n"));

	bzero(key, sizeof(wl_wsec_key_t));
	key->len = tk_len;
	bcopy(tk, key->data, key->len);
	bcopy(ea, &key->ea, ETHER_ADDR_LEN);
	/* NB: wlc_insert_key() will re-infer key.algo from key_len */
	key->algo = cipher;
	key->flags = WL_PRIMARY_KEY;
#ifndef BCMINTSUP
	PLUMB_TK(key, bsscfg);
#else
	err = wlc_iovar_op(wlc, "wsec_key", NULL, 0, key, sizeof(wl_wsec_key_t),
	                   IOV_SET, bsscfg->wlcif);
	if (err) {
		WL_ERROR(("wl%d: ERROR %d calling wlc_iovar_op with iovar \"wsec_key\"\n",
		          wlc->pub->unit, err));
	}
#endif /* BCMINTSUP */
	MFREE(wlc->osh, key, sizeof(wl_wsec_key_t));
	return;
}

#ifdef BCMINTSUP
/* Make a PMK from a pre-shared key */
/* Return 0 when pmk calculation is done and 1 when pmk calculation is in progress.
 * Return -1 when any error happens.
 */
int
wlc_wpa_cobble_pmk(wpapsk_info_t *info, char *psk, size_t psk_len, uchar *ssid, uint ssid_len)
{
	uchar in_key[DOT11_MAX_KEY_SIZE*2];

	/* There must be a PSK of at least 8 characters and no more than 64
	 * characters.  If it's 64 characters, they must all be legible hex.
	 * (It could be 66 characters if the first are the hex radix.)
	 */
	if ((psk == NULL) || (psk_len < WPA_MIN_PSK_LEN)) {
		WL_WSEC(("wl%d: wlc_wpa_cobble_pmk: insufficient key material for PSK\n",
			info->wlc->pub->unit));
		return -1;
	}
	/* check for hex radix if long enough for one */
	if ((psk_len ==  WSEC_MAX_PSK_LEN + 2) && (psk[0] == '0') &&
	    ((psk[1] == 'x') || (psk[1] == 'X'))) {
		psk += 2;
		psk_len -= 2;
	}

	/* If it's the right size for a hex PSK, check that it is
	 * really all ASCII hex characters.
	 */
	if (psk_len == WSEC_MAX_PSK_LEN) {
		char hex[] = "XX";
		int i = 0;

		do {
			hex[0] = *psk++;
			hex[1] = *psk++;
			if (!bcm_isxdigit(hex[0]) || !bcm_isxdigit(hex[1])) {
				WL_WSEC(("wl%d: wlc_wpa_cobble_pmk: numeric PSK is not 256-bit hex"
					" number\n", info->wlc->pub->unit));
				return -1;
			}
			/* okay so far; make this piece a number */
			in_key[i] = (uint8) bcm_strtoul(hex, NULL, 16);
		} while (++i < DOT11_MAX_KEY_SIZE);
		bcopy(in_key, info->pmk, sizeof(info->pmk));

	} else if (psk_len < WSEC_MAX_PSK_LEN) {

		/* Make certain the PSK string is NULL-terminated */
		psk[psk_len] = '\0';

		/* It's something that needs hashing */

		if (init_passhash(&info->passhash_states, psk, (int)psk_len, ssid, ssid_len))
			return -1;

		/* remove timer left from previous run */
		wl_del_timer(info->wlc->wl, info->passhash_timer);

		/* start the timer */
		wl_add_timer(info->wlc->wl, info->passhash_timer, 0, 0);
		return 1;
	} else {
		WL_WSEC(("wl%d: wlc_wpa_cobble_pmk: illegal PSK length (%u)\n",
		         info->wlc->pub->unit, (uint)psk_len));
		return -1;
	}
	info->pmk_len = PMK_LEN;
	return 0;
}
#endif /* BCMINTSUP */

/* setup cipher info in supplicant stucture */
bool
wlc_wpa_set_ucipher(wpapsk_t *wpa, ushort ucipher, bool wep_ok)
{
	/* update sta supplicant info */
	switch (ucipher) {
	case CRYPTO_ALGO_AES_CCM:
		wpa->ptk_len = AES_PTK_LEN;
		wpa->tk_len = AES_TK_LEN;
		wpa->desc = WPA_KEY_DESC_V2;
		break;
	case CRYPTO_ALGO_TKIP:
		wpa->ptk_len = TKIP_PTK_LEN;
		wpa->tk_len = TKIP_TK_LEN;
		wpa->desc = WPA_KEY_DESC_V1;
		break;
	default:
		WL_WSEC(("wlc_wpa_set_ucipher: unexpected unicast cipher (%d)\n",
			(int)ucipher));
		return FALSE;
	}
	return TRUE;
}
#endif /* defined(BCMSUP_PSK) || defined(BCMAUTH_PSK) */

#if defined(BCMSUP_PSK)
#ifdef BCMSUP_PSK
/* Convert the basic supplicant state from internal to external format */
static sup_auth_status_t
wlc_sup_conv_auth_state(wpapsk_state_t state)
{
	switch (state) {
		case WPA_SUP_DISCONNECTED:
			return WLC_SUP_DISCONNECTED;
		case WPA_SUP_INITIALIZE:
			return WLC_SUP_AUTHENTICATED;
		case WPA_SUP_AUTHENTICATION:
		case WPA_SUP_STAKEYSTARTP_PREP_M2:
		case WPA_SUP_STAKEYSTARTP_WAIT_M3:
		case WPA_SUP_STAKEYSTARTP_PREP_M4:
		case WPA_SUP_STAKEYSTARTG_WAIT_G1:
		case WPA_SUP_STAKEYSTARTG_PREP_G2:
			return WLC_SUP_KEYXCHANGE;
		case WPA_SUP_KEYUPDATE:
			return WLC_SUP_KEYED;
		default:
			return WLC_SUP_DISCONNECTED;
	}
}
#endif  /* BCMSUP_PSK */

sup_auth_status_t
wlc_sup_get_auth_status(supplicant_t *sup)
{

#ifdef BCMSUP_PSK
	if (sup->sup_type == SUP_WPAPSK) {
		return wlc_sup_conv_auth_state(sup->wpa->state);
	}
#endif  /* BCMSUP_PSK */

	return WLC_SUP_DISCONNECTED;
}

#if defined(BCMSUP_PSK)
/* Convert the extended supplicant state from internal to external format */
static sup_auth_status_t
wlc_sup_conv_ext_auth_state(wpapsk_state_t state)
{
	switch (state) {
		case WPA_SUP_STAKEYSTARTP_WAIT_M1:
			return WLC_SUP_KEYXCHANGE_WAIT_M1;
		case WPA_SUP_STAKEYSTARTP_PREP_M2:
			return WLC_SUP_KEYXCHANGE_PREP_M2;
		case WPA_SUP_STAKEYSTARTP_WAIT_M3:
			return WLC_SUP_KEYXCHANGE_WAIT_M3;
		case WPA_SUP_STAKEYSTARTP_PREP_M4:
			return WLC_SUP_KEYXCHANGE_PREP_M4;
		case WPA_SUP_STAKEYSTARTG_WAIT_G1:
			return WLC_SUP_KEYXCHANGE_WAIT_G1;
		case WPA_SUP_STAKEYSTARTG_PREP_G2:
			return WLC_SUP_KEYXCHANGE_PREP_G2;
		default:
			return wlc_sup_conv_auth_state(state);
	}
}
#endif	/* BCMSUP_PSK */

/* Return the extended supplicant authentication state */
sup_auth_status_t
wlc_sup_get_auth_status_extended(supplicant_t *sup)
{
	sup_auth_status_t	status = wlc_sup_get_auth_status(sup);
#if defined(BCMSUP_PSK)
#ifdef BCMINTSUP
	if (!sup->bsscfg->associated) {
		status = WLC_SUP_DISCONNECTED;
	}
	else
#endif /* BCMINTSUP */
	if (status == WLC_SUP_KEYXCHANGE) {
		status = wlc_sup_conv_ext_auth_state(sup->wpa->state);
	}
#endif	/* BCMSUP_PSK */
	return status;
}

void
wlc_sup_set_ea(supplicant_t *sup, struct ether_addr *ea)
{
	bcopy(ea, &sup->peer_ea, ETHER_ADDR_LEN);
}

/* ARGSUSED */
bool
wlc_set_sup(supplicant_t *sup, int sup_type,
	/* the following parameters are used only for PSK */
	uint8 *sup_ies, uint sup_ies_len, uint8 *auth_ies, uint auth_ies_len)
{
	bool ret = TRUE;

	if (sup == NULL) {
		WL_WSEC(("wlc_set_sup called with NULL sup context\n"));
		return FALSE;
	}
	sup->sup_type = SUP_UNUSED;
#if defined(BCMSUP_PSK)
	sup->ap_eapver = 0;
#endif /* defined(BCMSUP_PSK) */

#ifdef	BCMSUP_PSK
	if (sup_type == SUP_WPAPSK) {
		sup->sup_type = sup_type;
		ret = wlc_sup_wpapsk_start(sup, sup_ies, sup_ies_len, auth_ies, auth_ies_len);
	}
#endif	/* BCMSUP_PSK */


	/* If sup_type is still SUP_UNUSED, the passed type must be bogus */
	if (sup->sup_type == SUP_UNUSED) {
		WL_WSEC(("wl%d: wlc_set_sup: unexpected supplicant type %d\n",
		         UNIT(sup), sup_type));
		return FALSE;
	}
	return ret;
}

/* Dispatch EAPOL to supplicant.
 * Return boolean indicating whether supplicant's use of message means
 * it should be freed or sent up.
 */
bool
wlc_sup_eapol(supplicant_t *sup, eapol_header_t *eapol_hdr, bool encrypted,
	bool *auth_pending)
{

	if (!sup) {
		/* no unit to report if this happens */
		WL_ERROR(("%s: called with NULL sup\n", __FUNCTION__));
		return FALSE;
	}

#ifdef	BCMSUP_PSK
#ifdef	REDO_THIS_STUFF
	if (eapol_hdr->type == EAPOL_KEY) {
		eapol_wpa_key_header_t *body;

		body = (eapol_wpa_key_header_t *)eapol_hdr->body;
		if (body->type == EAPOL_WPA_KEY) {
		}
	}
#endif /* REDO_THIS_STUFF */
	/* Save eapol version from the AP */
	sup->ap_eapver = eapol_hdr->version;
	/* If the supplicant is set to do a WPA key exchange and this is
	 * a WPA key message, send it to the code for WPA.
	 */
	if ((eapol_hdr->type == EAPOL_KEY) &&
	    (sup->sup_type == SUP_WPAPSK))
	 {
		eapol_wpa_key_header_t *body;

		body = (eapol_wpa_key_header_t *)eapol_hdr->body;
		if (SUP_CHECK_EAPOL(body)) {
			(void) wlc_wpa_sup_eapol(sup, eapol_hdr, encrypted);
			return TRUE;
		}
	}
#endif	/* BCMSUP_PSK */


	/* Get here if no supplicant saw the message */

#ifdef BCMSUP_PSK
	/* Reset sup state and clear PMK on (re)auth (i.e., EAP Id Request) */
	if (sup->sup_type == SUP_WPAPSK && eapol_hdr->type == EAP_PACKET) {
		eap_header_t *eap_hdr = (eap_header_t *)eapol_hdr->body;

		if (eap_hdr->type == EAP_IDENTITY &&
		    eap_hdr->code == EAP_REQUEST) {
			if (sup->wpa->WPA_auth == WPA_AUTH_UNSPECIFIED ||
			    sup->wpa->WPA_auth == WPA2_AUTH_UNSPECIFIED) {
				WL_WSEC(("wl%d: wlc_sup_eapol: EAP-Identity Request received - "
				         "reset supplicant state and clear PMK\n", UNIT(sup)));
				sup->wpa->state = WPA_SUP_INITIALIZE;
				sup->wpa_info->pmk_len = 0;
			} else {
				WL_WSEC(("wl%d: wlc_sup_eapol: EAP-Identity Request ignored\n",
				         UNIT(sup)));
			}
		} else {
			WL_WSEC(("wl%d: wlc_sup_eapol: EAP packet ignored\n", UNIT(sup)));
		}
	}
#endif /* BCMSUP_PSK */

	return FALSE;
}

int
wlc_sup_down(supplicant_t *sup)
{
	int callbacks = 0;

	if (sup == NULL)
		return callbacks;

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	if (!wl_del_timer(sup->wl, sup->wpa_info->passhash_timer))
		callbacks ++;
#endif	/* BCMSUP_PSK && BCMINTSUP */

	return callbacks;
}

/* Allocate supplicant context, squirrel away the passed values,
 * and return the context handle.
 */
void *
wlc_sup_attach(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	supplicant_t *sup;

	WL_TRACE(("wl%d: wlc_sup_attach\n", wlc->pub->unit));

	if (!(sup = (supplicant_t *)MALLOC(wlc->osh, sizeof(supplicant_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	bzero(sup, sizeof(supplicant_t));
	sup->wlc = wlc;
	sup->bsscfg = cfg;
	sup->pub = wlc->pub;
	sup->wl = wlc->wl;
	sup->osh = wlc->osh;
	sup->sup_type = SUP_UNUSED;

#if defined(BCMSUP_PSK)
	if (!(sup->wpa = MALLOC(wlc->osh, sizeof(wpapsk_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto err;
	}
	bzero(sup->wpa, sizeof(wpapsk_t));
	if (!(sup->wpa_info = MALLOC(wlc->osh, sizeof(wpapsk_info_t)))) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto err;
	}
	bzero(sup->wpa_info, sizeof(wpapsk_info_t));
	sup->wpa_info->wlc = wlc;
#endif /* BCMSUP_PSK */


#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	if (!(sup->wpa_info->passhash_timer =
	           wl_init_timer(sup->wl, wlc_sup_wpa_passhash_timer, sup, "passhash"))) {
		WL_ERROR(("wl%d: %s: passhash timer failed\n",
		          UNIT(sup), __FUNCTION__));
		goto err;
	}
#endif	/* BCMSUP_PSK && BCMINTSUP */

	return sup;

err:
	wlc_sup_detach(sup);
	return NULL;
}

/* Toss supplicant context */
void
wlc_sup_detach(supplicant_t *sup)
{
	if (sup != NULL) {
		WL_TRACE(("wl%d: wlc_sup_detach\n", UNIT(sup)));

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
		if (sup->wpa_info) {
			if (sup->wpa_info->passhash_timer)
				wl_free_timer(sup->wl, sup->wpa_info->passhash_timer);
			MFREE(sup->osh, sup->wpa_info, sizeof(wpapsk_info_t));
		}
#endif	/* BCMSUP_PSK && BCMINTSUP */


#ifdef	BCMSUP_PSK
		if (sup->wpa) {
			wlc_wpapsk_free(sup->wlc, sup->wpa);
			MFREE(sup->osh, sup->wpa, sizeof(wpapsk_t));
		}
#endif	/* BCMSUP_PSK */

		MFREE(sup->osh, sup, sizeof(supplicant_t));
	} else
		WL_ERROR(("%s: called with NULL supplicant info addr\n", __FUNCTION__));
}
#endif  

#if defined(WOWL) && defined(BCMSUP_PSK)
#define PUTU32(ct, st) { \
		(ct)[0] = (uint8)((st) >> 24); \
		(ct)[1] = (uint8)((st) >> 16); \
		(ct)[2] = (uint8)((st) >>  8); \
		(ct)[3] = (uint8)(st); }

/* For Wake-on-wireless lan, broadcast key rotation feature requires a information like
 * KEK - KCK to be programmed in the ucode
 */
void *
wlc_sup_hw_wowl_init(supplicant_t *sup)
{
	uint32 rk[4*(AES_MAXROUNDS+1)];
	int rounds;
	void *gtkp;
	int i;
	wlc_info_t *wlc = sup->wlc;
	uint keyrc_offset, kck_offset, kek_offset;
	uint16 ram_base;

	/* Program last reply counter -- sup->wpa.last_replay. sup->wpa.replay is the expected
	 * value of next message while the ucode requires replay value from the last message
	 */
	if (D11REV_LT(sup->wlc->pub->corerev, 40)) {
		keyrc_offset = M_KEYRC_LAST;
		kck_offset = M_KCK;
		kek_offset = M_KEK;
		ram_base = WOWL_TX_FIFO_TXRAM_BASE;
	}
	else {
		keyrc_offset = M_KEYRC_LAST_GE42;
		kck_offset = M_KCK_GE42;
		kek_offset = M_KEK_GE42;
		ram_base = WOWL_TX_FIFO_TXRAM_BASE_GE42;
	}

	wlc_copyto_shm(wlc, keyrc_offset,
		sup->wpa->last_replay, EAPOL_KEY_REPLAY_LEN);

	/* Prepare a dummy GTK MSG2 packet to program header for WOWL ucode */
	/* We don't care about the actual flag, we just need a dummy frame to create d11hdrs from */
	if (sup->wpa->ucipher != CRYPTO_ALGO_AES_CCM)
		gtkp = wlc_wpa_sup_prepeapol(sup, (WPA_KEY_DESC_V1), GMSG2);
	else
		gtkp = wlc_wpa_sup_prepeapol(sup, (WPA_KEY_DESC_V2), GMSG2);

	if (!gtkp)
		return NULL;

	/* Program KCK -- sup->wpa->eapol_mic_key */
	wlc_copyto_shm(wlc, kck_offset, sup->wpa->eapol_mic_key, WPA_MIC_KEY_LEN);

	/* Program KEK for WEP/TKIP (how do I find what's what) */
	/* Else program expanded key using rijndaelKeySetupEnc and program the keyunwrapping
	 * tables
	 */
	if (sup->wpa->ucipher != CRYPTO_ALGO_AES_CCM) {
		wlc_copyto_shm(wlc, kek_offset,
			sup->wpa->eapol_encr_key, WPA_ENCR_KEY_LEN);

	}
	else {
		rounds = rijndaelKeySetupEnc(rk, sup->wpa->eapol_encr_key,
		                             AES_KEY_BITLEN(WPA_ENCR_KEY_LEN));
		ASSERT(rounds == EXPANDED_KEY_RNDS);

		/* Convert the table to format that ucode expects */
		for (i = 0; i < (int)(EXPANDED_KEY_LEN/sizeof(uint32)); i++) {
			uint32 *v = &rk[i];
			uint8 tmp[4];

			PUTU32(tmp, rk[i]);

			*v = (uint32)*((uint32*)tmp);
		}

		/* Program the template ram with AES key unwrapping tables */
		wlc_write_shm(wlc, M_AESTABLES_PTR, ram_base);

		wlc_write_template_ram(wlc, ram_base,
		                       ARRAYSIZE(aes_xtime9dbe) * 2, (void *)aes_xtime9dbe);

		wlc_write_template_ram(wlc, ram_base +
		                       (ARRAYSIZE(aes_xtime9dbe) * 2),
		                       ARRAYSIZE(aes_invsbox) * 2,
		                       (void *)aes_invsbox);

		wlc_write_template_ram(wlc, ram_base +
		                       ((ARRAYSIZE(aes_xtime9dbe) + ARRAYSIZE(aes_invsbox)) * 2),
		                       EXPANDED_KEY_LEN, (void *)rk);
	}

	return gtkp;
}

/* Update the Supplicant's software state as the key could have rotated while driver was in
 * Wake mode
 */
void
wlc_sup_sw_wowl_update(supplicant_t *sup)
{
	uint keyrc_offset;

	if (D11REV_LT(sup->wlc->pub->corerev, 40))
		keyrc_offset = M_KEYRC_LAST;
	else
		keyrc_offset = M_KEYRC_LAST_GE42;
	/* Update the replay counter from the AP */
	wlc_copyfrom_shm(sup->wlc, keyrc_offset, sup->wpa->last_replay, EAPOL_KEY_REPLAY_LEN);

	/* Driver's copy of replay counter is one more than APs */
	bcopy(sup->wpa->last_replay, sup->wpa->replay, EAPOL_KEY_REPLAY_LEN);
	wpa_incr_array(sup->wpa->replay, EAPOL_KEY_REPLAY_LEN);
}
#endif /* WOWL && BCMSUP_PSK */


#ifdef WLFBT
void
wlc_sup_ftauth_clear_ies(wlc_info_t *wlc, supplicant_t *sup)
{
	wpapsk_t *wpa = sup->wpa;
	wlc_wpapsk_free(wlc, wpa);
	sup->wpa_info->pmk_len = 0;
}


void
wlc_sup_calc_fbt_ptk(supplicant_t *sup)
{
	uchar pmkr0[PMK_LEN], pmkr1[PMK_LEN];

	/* calc PMK-R0 */
	wpa_calc_pmkR0(BSS_SSID(sup), BSS_SSID_LEN(sup), sup->wpa->mdid,
	               sup->wpa->r0khid->data, sup->wpa->r0khid->len, &CUR_EA(sup),
	               sup->wpa_info->pmk, (uint)sup->wpa_info->pmk_len,
	               pmkr0, sup->wpa->pmkr0name);

	/* calc PMK-R1 */
	wpa_calc_pmkR1((struct ether_addr *)sup->wpa->r1khid->data, &CUR_EA(sup),
		pmkr0, PMK_LEN, sup->wpa->pmkr0name, pmkr1, sup->wpa->pmkr1name);

	/* calc PTK */
	wpa_calc_ft_ptk(&PEER_EA(sup), &CUR_EA(sup),
	             sup->wpa->anonce, sup->wpa->snonce, pmkr1, PMK_LEN,
	             sup->wpa->eapol_mic_key, (uint)sup->wpa->ptk_len);

#ifdef BCMDBG
	wlc_sup_dump_fbt_keys(sup, pmkr0, pmkr1);
#endif
}


#ifdef BCMDBG
void
wlc_sup_dump_fbt_keys(supplicant_t *sup, uchar *pmkr0, uchar *pmkr1)
{
	uchar *ptk;
	int i;

	prhex("PMK-R0", pmkr0, 32);

	printf("R0KHID len %d : \n", sup->wpa->r0khid->len);
	prhex("R0KHID", sup->wpa->r0khid->data, sup->wpa->r0khid->len);

	printf("R1KHID len %d : \n", sup->wpa->r1khid->len);
	prhex("R1KHID", sup->wpa->r1khid->data, sup->wpa->r1khid->len);

	prhex("PMK-R0name", sup->wpa->pmkr0name, 16);

	prhex("PMK-R1", pmkr1, 32);

	prhex("PMK-R1name", sup->wpa->pmkr1name, 16);

	prhex("Anonce", sup->wpa->anonce, 32);
	prhex("Snonce", sup->wpa->snonce, 32);

	printf("BSSID : \n");
	for (i = 0; i < 6; i++) {
		printf(" 0x%2x ", PEER_EA(sup).octet[i]);
	}
	printf("\n");

	ptk = (uchar *)sup->wpa->eapol_mic_key;
	prhex("PTK", ptk, 16);
}
#endif /* BCMDBG */

bool
wlc_sup_is_cur_mdid(struct supplicant *sup, wlc_bss_info_t *bi)
{
	return ((sup->wpa->mdie_len != 0) && (bi->flags & WLC_BSS_FBT)	&&
		(bi->mdid == sup->wpa->mdid));
}

int
wlc_sup_ftauth_getlen(supplicant_t *sup)
{
	wpapsk_t *wpa = sup->wpa;
	return (wpa->sup_wpaie_len + sizeof(wpa_pmkid_list_t) + TLV_HDR_LEN + wpa->r0khid->len +
		wpa->mdie_len + sizeof(dot11_ft_ie_t));
}


int
wlc_sup_ftreq_getlen(supplicant_t *sup)
{
	wpapsk_t *wpa = sup->wpa;
	return (wpa->sup_wpaie_len + TLV_HDR_LEN + wpa->r0khid->len + wpa->mdie_len +
		sizeof(dot11_ft_ie_t));
}

uint8 *
wlc_sup_ft_authreq(struct supplicant *sup, uint8 *pbody)
{
	dot11_ft_ie_t *ftie;
	wpa_pmkid_list_t *wpa_pmkid;
	wpapsk_t *wpa = sup->wpa;

	bcopy((char *)wpa->sup_wpaie, pbody, wpa->sup_wpaie_len);
	if (wpa->sup_wpaie_len != 0) {
		/* Add pmkr0name to rsnie */
		pbody[1] += sizeof(wpa_pmkid_list_t);
		pbody += wpa->sup_wpaie_len;
		wpa_pmkid = (wpa_pmkid_list_t *)pbody;
		wpa_pmkid->count.low = 1;
		wpa_pmkid->count.high = 0;
		bcopy(wpa->pmkr0name, &wpa_pmkid->list[0], WPA2_PMKID_LEN);
		pbody += sizeof(wpa_pmkid_list_t);

		/* Add mdie */
		if (wpa->mdie != NULL) {
			bcopy((char *)wpa->mdie, pbody, wpa->mdie_len);
			pbody += wpa->mdie_len;
		}

		/* Add FTIE with snonce, r0kh-id */
		if (wpa->r0khid != NULL) {
			ftie = (dot11_ft_ie_t *)pbody;
			bzero(ftie, sizeof(dot11_ft_ie_t));
			ftie->id = DOT11_MNG_FTIE_ID;
			ftie->len = sizeof(dot11_ft_ie_t) + wpa->r0khid->len;
			++wpa->snonce[0];
			bcopy(wpa->snonce, ftie->snonce, EAPOL_WPA_KEY_NONCE_LEN);
			pbody += sizeof(dot11_ft_ie_t);
			ASSERT((wpa->ftie_len >= sizeof(dot11_ft_ie_t)) && (wpa->ftie_len < 255));
			bcopy(wpa->r0khid, pbody, TLV_HDR_LEN + wpa->r0khid->len);
			pbody += TLV_HDR_LEN + wpa->r0khid->len;
		}
	}
	return pbody;
}

/* Parse auth resp frame for ftie */
int
wlc_sup_ft_authresp(supplicant_t *sup, uint8 *body, int body_len)
{
	dot11_ft_ie_t *ftie;
	wpapsk_t *wpa = sup->wpa;
	uchar *tlvs;
	uint tlv_len;


	if ((ftie = (dot11_ft_ie_t *)bcm_parse_tlvs(body, body_len, DOT11_MNG_FTIE_ID)) == NULL) {
		WL_WSEC(("%s: no ftie in auth resp frame \n", __FUNCTION__));
		return FALSE;
	}

	if (wpa->ftie)
		MFREE(sup->wlc->osh, wpa->ftie, wpa->ftie_len);

	wpa->ftie_len = ftie->len + TLV_HDR_LEN;
	wpa->ftie = MALLOC(sup->wlc->osh, wpa->ftie_len);
	if (!wpa->ftie) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			sup->wlc->pub->unit, __FUNCTION__, MALLOCED(sup->wlc->osh)));
		return FALSE;
	}
	bcopy(ftie, wpa->ftie, wpa->ftie_len);
	tlvs = (uchar *)((uintptr)wpa->ftie + sizeof(dot11_ft_ie_t));
	tlv_len = wpa->ftie_len - sizeof(dot11_ft_ie_t);
	wpa->r1khid = bcm_parse_tlvs(tlvs, tlv_len, 1);
	wpa->r0khid = bcm_parse_tlvs(tlvs, tlv_len, 3);
	bcopy(ftie->anonce, sup->wpa->anonce, sizeof(sup->wpa->anonce));
	ASSERT(wpa->r0khid && wpa->r1khid);
	wlc_sup_calc_fbt_ptk(sup);

	return TRUE;
}


/* Calculate the RIC IE endptr and IE count. */
static bool
wlc_sup_parse_ric(uint8 *pbody, int len, uint8 **ricend, int *ric_ie_count)
{
	bool found = FALSE;
	uint8 *ricptr;
	uint8 *_ricend = NULL;
	uint8 *end = NULL;
	int _ric_ie_count = 0;

	ricptr = pbody;
	end = pbody + len;
	while (ricptr < end) {
		bcm_tlv_t *ie = (bcm_tlv_t*)ricptr;
		int ie_len = TLV_HDR_LEN + ie->len;
		ricptr += ie_len;
		if (ie->id == DOT11_MNG_RDE_ID) {
			int i;
			dot11_rde_ie_t *rde = (dot11_rde_ie_t*)ie;

			/* Include RDE in MIC calculations. */
			_ric_ie_count += 1;
			/* Include protected elements too. */
			_ric_ie_count += rde->rd_count;
			for (i = 0; i < rde->rd_count; i++) {
				bcm_tlv_t *ie = (bcm_tlv_t*)ricptr;

				ricptr += TLV_HDR_LEN + ie->len;
				_ricend = ricptr;
			}
			/* Set return val. */
			found = TRUE;
		}
	}

	if (found) {
		*ricend = _ricend;
		*ric_ie_count = _ric_ie_count;
	}

	return found;
}

/* Internal function that writes out the FT IE and frames it protects. */
static uint8 *
wlc_sup_ft_write_protected_ies(uint8 *pbody, int len, bcm_tlv_t *ftie, uint8 *ricdata,
	int ricdata_len)
{
	/* Check that there's enough room in pbody before writing anything. */
	if (len < (ftie->len + TLV_HDR_LEN + ricdata_len)) {
		WL_ERROR(("%s: Not enough buffer for FTIE & RICs \n", __FUNCTION__));
		return pbody;
	}

	/* Write IEs to pbody. */
	bcopy(ftie, pbody, ftie->len + TLV_HDR_LEN);
	pbody += ftie->len + TLV_HDR_LEN;
	len -= ftie->len + TLV_HDR_LEN;

	/* Write RIC request(s) after Fast BSS transition IE. */
	if (ricdata && ricdata_len) {
		bcopy(ricdata, pbody, ricdata_len);
		pbody += ricdata_len;
		len -= ricdata_len;
	}

	return pbody;
}

/* Populates the MIC and MIC control fields of the supplied FTIE. */
static bool
wlc_sup_ft_calc_mic(supplicant_t *sup, dot11_ft_ie_t *ftie, bcm_tlv_t *mdie, bcm_tlv_t *rsnie,
	uint8 *ricdata, int ricdata_len, int ric_ie_count, uint8 trans_seq_nbr, uint8* mic,
	uint16* mic_control)
{
	int total_len = 0;
	uint8 *micdata;
	uint8 *pos;
	uint prot_ie_len = 3 + ric_ie_count;
	wpapsk_t *wpa = sup->wpa;
	uint8 *src, *dst;

	/* See 802.11r 11A.8.4 & 11A.8.5 for details. */
	/* Total expected size of buffer needed to calculate the MIC. */
	total_len += 2 * ETHER_ADDR_LEN;	/* AP & STA MAC addresses. */
	total_len += 1;				/* Transaction sequence number byte. */
	total_len += TLV_HDR_LEN + rsnie->len;
	total_len += TLV_HDR_LEN + mdie->len;
	total_len += TLV_HDR_LEN + ftie->len;
	total_len += ricdata_len;

	micdata = MALLOC(sup->osh, total_len);
	if (micdata == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			UNIT(sup), __FUNCTION__, MALLOCED(sup->osh)));
		return FALSE;
	}
	pos = micdata;

	/* calc mic */
	bcopy((uint8 *)&CUR_EA(sup), pos, ETHER_ADDR_LEN);
	pos += ETHER_ADDR_LEN;
	bcopy((uint8 *)&PEER_EA(sup), pos, ETHER_ADDR_LEN);
	pos += ETHER_ADDR_LEN;

	/* Transaction sequence number. The value is dependent on whether we're called for an
	 * (re)association request, (re)association response, or other.
	 */
	*pos++ = trans_seq_nbr;

	bcopy(rsnie, pos, rsnie->len + TLV_HDR_LEN);
	pos += rsnie->len + TLV_HDR_LEN;

	bcopy(mdie, pos, mdie->len + TLV_HDR_LEN);
	pos += mdie->len + TLV_HDR_LEN;

	/* copy the ftie before playing with it */
	bcopy(ftie, pos, ftie->len + TLV_HDR_LEN);
	ftie = (dot11_ft_ie_t*)pos;
	/* Prepare the FTIE. Set protected frame count in MIC control and zero the MIC field. */
	*mic_control = htol16(prot_ie_len << 8);
	src = (uint8*)mic_control;
	dst = (uint8*)&ftie->mic_control;
	*dst++ = *src++;
	*dst = *src;
	bzero(ftie->mic, sizeof(ftie->mic));

	pos += ftie->len + TLV_HDR_LEN;

	/* Add any RIC IEs to MIC data. */
	if (ricdata && ricdata_len) {
		/* Include RDE and counted frames in protected IE calculations. */
		bcopy(ricdata, pos, ricdata_len);
		pos += ricdata_len;
	}

	aes_cmac_calc(micdata, pos - micdata, wpa->eapol_mic_key, EAPOL_WPA_KEY_MIC_LEN, mic);
	MFREE(sup->osh, micdata, total_len);

	return TRUE;
}


uint8 *
wlc_sup_ft_assocreq(supplicant_t *sup, uint8 *pbody, int len, bcm_tlv_t *mdie,
	bcm_tlv_t *rsnie, uint8 *ricdata, int ricdata_len, int ric_ie_count)
{
	uint8 *ftptr;

	if (sup == NULL || sup->wpa == NULL) {
		/* no unit to report if this happens */
		WL_ERROR(("%s: called with NULL sup\n", __FUNCTION__));
		return pbody;
	}
	if (mdie == NULL) {
		WL_ERROR(("wl%d: %s: MDIE not found and required\n",
			UNIT(sup), __FUNCTION__));
		return pbody;
	}
	if (rsnie == NULL) {
		WL_ERROR(("wl%d: %s: RSNIE not found and required\n",
			UNIT(sup), __FUNCTION__));
		return pbody;
	}
	if (sup->wpa->ftie == NULL || sup->wpa->ftie_len == 0) {
		WL_ERROR(("wl%d: %s: FTIE not found and required\n",
			UNIT(sup), __FUNCTION__));
		return pbody;
	}

	ftptr = MALLOC(sup->osh, sup->wpa->ftie_len);
	if (ftptr == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			UNIT(sup), __FUNCTION__, MALLOCED(sup->osh)));
	} else {
		bcopy(sup->wpa->ftie, ftptr, sup->wpa->ftie_len);
		if (wlc_sup_ft_calc_mic(sup, (dot11_ft_ie_t*)ftptr, mdie, rsnie,
			ricdata, ricdata_len, ric_ie_count, FT_MIC_ASSOC_REQUEST_TSN,
			((dot11_ft_ie_t*)ftptr)->mic,
			&((dot11_ft_ie_t*)ftptr)->mic_control)) {
			pbody = wlc_sup_ft_write_protected_ies(pbody, len,
				(bcm_tlv_t*)ftptr, ricdata, ricdata_len);
		}
		MFREE(sup->osh, ftptr, sup->wpa->ftie_len);
	}

	return pbody;
}


uint8 *
wlc_sup_ft_pmkr1name(supplicant_t *sup)
{

	return (sup->wpa->pmkr1name);
}

/*
 * wlc_sup_ft_parse_reassocresp: Checks the validity of the FTIE (if provided) in the reassociation
 * response frame.
 *
 * Returns TRUE if the reassociation response frame is good, FALSE otherwise.
 *
 * A FALSE return will mean that the frame contained an FTIE but was missing either an MDIE or an
 * RSNIE (shouldn't really happen) or that the FT MIC was invalid. In either case, the calling
 * function should discard the reassociation response frame.
 */
bool
wlc_sup_ft_parse_reassocresp(supplicant_t *sup, uint8 *tlvs, int tlvs_len)
{
	bool is_valid = TRUE;
	dot11_ft_ie_t *ftie;
	bcm_tlv_t *mdie;
	bcm_tlv_t *rsnie;

	ftie = (dot11_ft_ie_t *)bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_FTIE_ID);
	if (ftie != NULL) {
		uint8 *ricie = NULL;
		uint8 *ricend = NULL;
		int ric_ie_count = 0;
		uint8 mic[PRF_OUTBUF_LEN];
		uint16 mic_control;

		mdie = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_MDIE_ID);
		if (mdie == NULL) {
			WL_ERROR(("wl%d: %s: MDIE not found and required\n",
				UNIT(sup), __FUNCTION__));
			return FALSE;
		}
		rsnie = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_RSN_ID);
		if (rsnie == NULL) {
			WL_ERROR(("wl%d: %s: RSNIE not found and required\n",
				UNIT(sup), __FUNCTION__));
			return FALSE;
		}
		ricie = (uint8*)bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_RDE_ID);
		if (ricie != NULL) {
			wlc_sup_parse_ric(ricie, tlvs_len - (ricie - tlvs), &ricend, &ric_ie_count);
		}
		wlc_sup_ft_calc_mic(sup, ftie, mdie, rsnie, ricie, ricend - ricie,
			ric_ie_count, FT_MIC_REASSOC_RESPONSE_TSN, mic, &mic_control);
		/* Now get the calculated MIC and compare to that in ftie. */
		if (bcmp(mic, ftie->mic, EAPOL_WPA_KEY_MIC_LEN)) {
			/* MICs are different. */
			is_valid = FALSE;
			if (WL_ERROR_ON()) {
				WL_ERROR(("FT-MICs do not match!\n"));
				prhex("Recv MIC", ftie->mic, EAPOL_WPA_KEY_MIC_LEN);
				prhex("Calc MIC", mic, EAPOL_WPA_KEY_MIC_LEN);
			}
		}
	}

	return is_valid;
}

bool
wlc_sup_ft_reassoc(supplicant_t *sup)
{
	wlc_bsscfg_t *cfg = sup->bsscfg;
	wlc_assoc_t *as = cfg->assoc;
	dot11_ft_ie_t *ftie;
	uint8 *tlvs;
	uint tlv_len;
	dot11_gtk_ie_t *gtk;
	wpapsk_t *wpa = sup->wpa;

	tlvs = (uint8 *)&as->resp[1];
	tlv_len = as->resp_len - DOT11_ASSOC_RESP_FIXED_LEN;
	if ((ftie = (dot11_ft_ie_t *)bcm_parse_tlvs(tlvs, tlv_len, DOT11_MNG_FTIE_ID)) != NULL) {
		tlvs = (uint8 *)((uintptr)ftie + sizeof(dot11_ft_ie_t));
		tlv_len = ftie->len - sizeof(dot11_ft_ie_t) + TLV_HDR_LEN;
		/* plumb the keys here and set the scb authorized */
		if ((gtk = (dot11_gtk_ie_t *)bcm_parse_tlvs(tlvs, tlv_len, 2)) != NULL) {
			wpa->gtk_len = gtk->key_len;
			wpa->gtk_index = ltoh16_ua(&gtk->key_info) & 0x3;
			/* extract and plumb GTK */
			ASSERT(gtk->len == 35);
			if (aes_unwrap(WPA_ENCR_KEY_LEN, wpa->eapol_encr_key, gtk->len - 11,
				&gtk->data[0], wpa->gtk)) {
				WL_WSEC(("FBT reassoc: GTK decrypt failed\n"));
				return FALSE;
			}
			wlc_wpa_plumb_gtk(sup->wlc, sup->bsscfg, wpa->gtk,
				wpa->gtk_len, wpa->gtk_index, wpa->mcipher, gtk->rsc, TRUE);

			wlc_wpa_plumb_tk(sup->wlc, sup->bsscfg,
				(uint8*)sup->wpa->temp_encr_key,
				sup->wpa->tk_len, sup->wpa->ucipher, &PEER_EA(sup));

			wlc_ioctl(sup->wlc, WLC_SCB_AUTHORIZE, &PEER_EA(sup),
				ETHER_ADDR_LEN, sup->bsscfg->wlcif);

			return TRUE;
		}
	}

	return FALSE;
}
#endif /* WLFBT */
