/*
 * Assorted functions for Wireless Security
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_security.c 376280 2012-12-24 05:36:39Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>

#define MBUFSIZE 4096 /* Max buffer size */
#include <osl.h>
#include <bcmutils.h>
#include <bcmendian.h>

#include <siutils.h>

#include <proto/802.11.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_key.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_security.h>
#include <wlc_event.h>
#include <bcmwpa.h>
#ifndef LINUX_CRYPTO
#include <bcmcrypto/aes.h>
#include <bcmcrypto/tkmic.h>
#include <bcmcrypto/wep.h>
#endif /* LINUX_CRYPTO */
#include <wlc_frmutil.h>
#ifdef BCMWAPI_WPI
#include <bcmcrypto/sms4.h>
#endif /* BCMWAPI_WPI */
#include <wlc_assoc.h>

#ifdef MFP
#include <wlc_mfp.h>
#endif

#include <wl_export.h>

#if defined(BCMWAPI_WPI) || defined(BCMWAPI_WAI)
#include <wlc_wapi.h>
#endif

#define _wlc_wsec_isreplay(iv32, iv16, last_iv32, last_iv16) \
	(((iv32) < (last_iv32)) || (((iv32) == (last_iv32)) && ((iv16) < (last_iv16))))

#ifdef BRCMAPIVTW

/* IV add/sub/comp. Use them with cautions! */
#define WSEC_IV_ADD(h, l, inc_h, inc_l) \
	do { \
		uint32 temp = l; \
		l += inc_l; \
		h += inc_h; \
		if ((uint32)l < temp) \
			h += 1; \
	} while (0)
#define WSEC_IV_SUB(h, l, dec_h, dec_l) \
	do { \
		uint32 temp = l; \
		l -= dec_l; \
		h -= dec_h; \
		if (temp < (uint32)l) \
			h -= 1; \
	} while (0)
#define WSEC_IV_GT(h1, l1, h2, l2) ((h1 > h2) || ((h1 == h2) && (l1 > l2)))
#define WSEC_IV_LE(h1, l1, h2, l2) ((h1 < h2) || ((h1 == h2) && (l1 <= l2)))
#define WSEC_IV_LT(h1, l1, h2, l2) ((h1 < h2) || ((h1 == h2) && (l1 < l2)))
#define WSEC_IV_GE(h1, l1, h2, l2) ((h1 > h2) || ((h1 == h2) && (l1 >= l2)))

/* The window size must be 2 power N otherwise it requires 48-bit modulo operation */
#define TWMASK	(TWSIZE-1)

/* Assume IV trace window are all initiailized to 0 */
static bool
wlc_wsec_isreplay(wlc_info_t *wlc, wsec_key_t *key, uint ividx, uint32 iv32, uint16 iv16,
	bool ismulti)
{
	uint16 off_tw;
	bool in_tw;

	/* Use the normal replay check code */
	if (!AMPDU_HOST_REORDER_ENABLED(wlc)) {
		if (!(wlc->brcm_ap_iv_tw_override == AUTO && wlc->brcm_ap_iv_tw) &&
			!(wlc->brcm_ap_iv_tw_override == ON))
		{
			return _wlc_wsec_isreplay(iv32, iv16,
				key->rxiv[ividx].hi, key->rxiv[ividx].lo);
		}
	}

	/* It is a replay when the IV is beyond the trace window's lower bound,
	 * or when the IV is in the trace window and it has been seen.
	 */
	off_tw = iv16 & TWMASK;
	if (WSEC_IV_LT(iv32, iv16, key->iv_tw[ividx].lb.hi, key->iv_tw[ividx].lb.lo) ||
	    ((in_tw = WSEC_IV_LE(iv32, iv16, key->iv_tw[ividx].ub.hi, key->iv_tw[ividx].ub.lo)) &&
	     (key->iv_tw[ividx].bmp[off_tw >> 3] & (1 << (off_tw & 7))))) {
		WL_WSEC(("tid %d iv %08x%04x map %d:%02x min %08x%04x max %08x%04x REPLAY\n",
		         ividx, iv32, iv16, off_tw >> 3, key->iv_tw[ividx].bmp[off_tw >> 3],
		         key->iv_tw[ividx].lb.hi, key->iv_tw[ividx].lb.lo,
		         key->iv_tw[ividx].ub.hi, key->iv_tw[ividx].ub.lo));
		return TRUE;
	}

	/* It is not a replay when the IV passes the trace window. Update
	 * the trace window bounds and initialize the trace entries.
	 */
	if (!in_tw) {
		wsec_iv_t old, new, adv;
		/* Update trace window upper bound */
		old = key->iv_tw[ividx].ub;
		key->iv_tw[ividx].ub.hi = iv32;
		key->iv_tw[ividx].ub.lo = iv16;
		/* Update trace window lower bound and trace entries */
		if (WSEC_IV_GE(key->iv_tw[ividx].ub.hi, key->iv_tw[ividx].ub.lo, 0, TWSIZE)) {
			/* Update trace window lower bound */
			key->iv_tw[ividx].lb = key->iv_tw[ividx].ub;
			WSEC_IV_SUB(key->iv_tw[ividx].lb.hi, key->iv_tw[ividx].lb.lo, 0, TWSIZE-1);
			/* Calculate how many entries need to be updated */
			adv = new = key->iv_tw[ividx].ub;
			WSEC_IV_SUB(adv.hi, adv.lo, old.hi, old.lo);
			/* Start trace entry update from the next entry pointed by the
			 * previous upper bound.
			 */
			WSEC_IV_ADD(old.hi, old.lo, 0, 1);
			/* Clear trace entries. */
			if (adv.lo > 1) {
				uint16 off_old = old.lo & TWMASK;
				uint16 off_new = new.lo & TWMASK;
				/* IV has advanced so much and there is no need to clear
				 * each individual trace entries between the old upper bound
				 * the new upper bound so just do a big hammer.
				 */
				if (adv.lo >= TWSIZE)
					bzero(key->iv_tw[ividx].bmp, sizeof(key->iv_tw[ividx].bmp));
				/* IV has moved up but the trace entries between
				 * the old upper bound and the new upper bound are
				 * still in the same byte in the trace window.
				 */
				else if (off_new > off_old &&
				         (off_old >> 3) == (off_new >> 3))
					key->iv_tw[ividx].bmp[off_old >> 3] &=
					        ((0xff >> (8 - (off_old & 7))) &
					         (0xff << (off_new & 7)));
				/* IV has moved up more and the trace entries between
				 * the old upper bound and the new upper bound are now
				 * across different bytes in the trace window.
				 */
				else {
					/* The old upper bound points at an entry in the
					 * middle of a byte in the trace window. Clear
					 * all entries up to the byte boundary.
					 */
					if ((off_old & 7) > 0) {
						uint16 bits = 8 - (off_old & 7);
						key->iv_tw[ividx].bmp[off_old >> 3] &=
						        (uint8)(0xff >> bits);
						off_old += bits;
						off_old &= TWMASK;
						adv.lo -= bits;
					}
					/* The new upper bound points at an entry in the
					 * middle of a byte in the trace window. Clear
					 * all entries from the byte boundary.
					 */
					if ((off_new & 7) < 7) {
						uint16 bits = (off_new & 7) + 1;
						key->iv_tw[ividx].bmp[off_new >> 3] &=
						        (uint8)(0xff << bits);
						off_new -= bits;
						off_new &= TWMASK;
						adv.lo -= bits;
					}
					/* Clear all entries that are in the full bytes */
					ASSERT((adv.lo & 7) == 0);
					if (off_old < off_new)
						bzero(&key->iv_tw[ividx].bmp[off_old >> 3],
						      (off_new - off_old + 1) >> 3);
					else if (adv.lo > 0) {
						bzero(&key->iv_tw[ividx].bmp[off_old >> 3],
						      (TWSIZE - off_old) >> 3);
						bzero(&key->iv_tw[ividx].bmp[0],
						      (off_new + 1) >> 3);
					}
				}
			}
		}
	}
	/* Update the trace entry for the current IV */
	key->iv_tw[ividx].bmp[off_tw >> 3] |= (1 << (off_tw & 7));

	WL_WSEC(("tid %d iv %08x%04x map %d:%02x min %08x%04x max %08x%04x\n",
	         ividx, iv32, iv16, off_tw >> 3, key->iv_tw[ividx].bmp[off_tw >> 3],
	         key->iv_tw[ividx].lb.hi, key->iv_tw[ividx].lb.lo,
	         key->iv_tw[ividx].ub.hi, key->iv_tw[ividx].ub.lo));
	return FALSE;
}
#else
static bool
wlc_wsec_isreplay(wlc_info_t *wlc, wsec_key_t *key, uint ividx, uint32 iv32, uint16 iv16,
	bool ismulti)
{
	bool ret;
	WL_NONE(("wl%d: wlc_wsec_isreplay: ividx %d, (iv32, iv16)0x%08x 0x%04x"
		 " (key_iv32, key_iv16)0x%08x 0x%04x\n",
		 wlc->pub->unit, ividx, iv32, iv16,
		 key->rxiv[ividx].hi, key->rxiv[ividx].lo));
	ret = _wlc_wsec_isreplay(iv32, iv16, (key)->rxiv[ividx].hi, (key)->rxiv[ividx].lo);

#ifdef GTK_RESET
	if (key->gtk_plumbed && ismulti && ret) {
		WL_WSEC(("wl%d: wlc_wsec_isreplay: replay detected, sync up replay: ividx %d,"
			"(rxiv_32, rxiv_16)0x%08x%04x (iv32, iv16)0x%08x%04x\n", wlc->pub->unit,
			ividx, key->rxiv[ividx].hi, key->rxiv[ividx].lo, iv32, iv16));
		key->rxiv[ividx].lo = iv16;
		key->rxiv[ividx].hi = iv32;
		key->gtk_plumbed = FALSE;
		ret = FALSE;
	}

	if (key->gtk_plumbed && ismulti)
		key->gtk_plumbed = FALSE;
#endif /* GTK_RESET */

	return ret;
}
#endif	/* BRCMAPIVTW */

/* Helper macros for posting decryption errors events.
 *
 */
#define WLC_ICV_ERROR(wlc, addr, bsscfg_idx) \
	wlc_decrypt_error((wlc), (addr), (bsscfg_idx), WLC_E_ICV_ERROR, FALSE, NULL, FALSE)

#define WLC_UNICAST_DECODE_ERROR(wlc, addr, bsscfg_idx) \
	wlc_decrypt_error((wlc), (addr), (bsscfg_idx), WLC_E_UNICAST_DECODE_ERROR, \
	                  FALSE, NULL, FALSE)

#define WLC_MULTICAST_DECODE_ERROR(wlc, addr, bsscfg_idx) \
	wlc_decrypt_error((wlc), (addr), (bsscfg_idx), WLC_E_MULTICAST_DECODE_ERROR, \
	                  FALSE, NULL, FALSE)

#define WLC_MIC_ERROR(wlc, addr, bsscfg_idx, group, key, flush_txq) {\
	WL_ERROR(("wl%d.%d: handling MIC error PSTA: %d, Group Key: %d\n", \
		wlc->pub->unit, bsscfg_idx, PSTA_ENAB(wlc->pub), (group))); \
	if (PSTA_ENAB(wlc->pub) && (group)) {\
		int sta_idx; \
		wlc_bsscfg_t *sta_bsscfg; \
		FOREACH_PSTA(wlc, sta_idx, sta_bsscfg) {\
			WL_ERROR(("wl%d.%d: dispatching group key MIC error to PSTA\n", \
				wlc->pub->unit, WLC_BSSCFG_IDX(sta_bsscfg))); \
			wlc_decrypt_error((wlc), (addr), WLC_BSSCFG_IDX(sta_bsscfg), \
				WLC_E_MIC_ERROR, (group), (key), (flush_txq)); \
		}\
	}\
	wlc_decrypt_error((wlc), (addr), (bsscfg_idx), WLC_E_MIC_ERROR, \
		(group), (key), (flush_txq)); \
}

/* 802.1X LLC header,
 * DSAP/SSAP/CTL = AA:AA:03
 * OUI = 00:00:00
 * Ethertype = 0x888e (802.1X Port Access Entity)
 */
const uint8 wlc_802_1x_hdr[] = {0xAA, 0xAA, 0x03, 0x00, 0x00, 0x00, 0x88, 0x8e};

#if defined(BCMCCMP)
static uint8 wlc_get_nonce_1st_byte(struct scb *scb, struct dot11_header *h,
	void *p, bool legacy);
#endif

static void
wlc_decrypt_error(wlc_info_t *wlc, struct ether_addr *addr, int bsscfg_idx,
	uint32 event_type, bool group, wsec_key_t *key, bool flush_txq);

static void
wlc_wsec_recvdata_enc_toss(wlc_info_t *wlc, struct wlc_frminfo *f, struct scb *scb)
{
	wlc_bsscfg_t *bsscfg;
#if defined(BCMDBG_ERR) || defined(WLMSG_WSEC)
	char addrstr[32];
#endif

	ASSERT(scb != NULL);

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

#ifdef AP
	/* 802.11i D5.0 8.4.10.1 Illegal data transfer */
	if (!f->ismulti && BSSCFG_AP(bsscfg) && SCB_LEGACY_WDS(scb) &&
	    (f->WPA_auth != WPA_AUTH_DISABLED)) {

		/* pairwise key is out of sync with peer, send deauth */
		if (!(scb->flags & SCB_DEAUTH)) {
			/* Use the cur_etheraddr of the BSSCFG that this WDS
			 * interface is tied to as our BSSID.  We can't use the
			 * BSSCFG's BSSID because the BSSCFG may not be "up" (yet).
			 */
			wlc_senddeauth(wlc, &scb->ea, &bsscfg->cur_etheraddr,
			               &bsscfg->cur_etheraddr,
			               scb, DOT11_RC_AUTH_INVAL);
			wlc_scb_clearstatebit(scb, AUTHORIZED);
			wlc_deauth_complete(wlc, bsscfg, WLC_E_STATUS_SUCCESS, &scb->ea,
				DOT11_RC_AUTH_INVAL, 0);
			scb->flags |= SCB_DEAUTH;
		}
	}
#endif /* AP */
	/* Post error event for unicast or multicast */
	if (!f->ismulti) {
		WLC_UNICAST_DECODE_ERROR(wlc, &bsscfg->current_bss->BSSID,
		                         WLC_BSSCFG_IDX(bsscfg));

		WL_ERROR(("wl%d.%d: %s unsupported encrypted unicast frame from %s\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
			bcm_ether_ntoa(&(f->h->a2), (char*)addrstr)));
	} else {
		WLC_MULTICAST_DECODE_ERROR(wlc, &bsscfg->current_bss->BSSID,
			WLC_BSSCFG_IDX(bsscfg));

		WL_WSEC(("wl%d.%d: %s unsupported encrypted multicast frame from %s\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__,
			bcm_ether_ntoa(&(f->h->a2), (char*)addrstr)));
	}
	WLCNTINCR(wlc->pub->_cnt->rxundec);
	WLCNTINCR(wlc->pub->_cnt->wepundec);
	if (f->ismulti) {
		WLCNTINCR(wlc->pub->_cnt->rxundec_mcst);
		WLCNTINCR(wlc->pub->_cnt->wepundec_mcst);
	}
}

static bool
wlc_wsec_recvdata_decrypt(wlc_info_t *wlc, osl_t *osh, struct scb *scb,
	struct wlc_frminfo *f, uint8 prio)
{
	wlc_bsscfg_t *bsscfg;
	uint8 ividx = f->ividx;
	uchar *piv = f->pbody;
	uint16 iv16 = 0;
	uint32 iv32 = 0;
	uint min_len;
	bool ext_tkip = FALSE;
#if defined(BCMCCMP)
	uint8 nonce_1st_byte;
#endif

	ASSERT(scb != NULL);

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);

#ifdef BCMWAPI_WPI
	if (scb->key && scb->key->algo == CRYPTO_ALGO_SMS4)
		f->key_index = f->pbody[0] & 0x01;
	else
#endif /* BCMWAPI_WPI */
		f->key_index = f->pbody[3] >> DOT11_KEY_INDEX_SHIFT;
	f->key = wlc_key_lookup(wlc, scb, bsscfg, f->key_index, f->ismulti);

	/* toss encrypted packets if we cannot decrypt them */
#ifdef DSLCPE_WDSSEC
	if (!(f->key && (LMAC_ENAB(wlc->pub) ||
		(!LMAC_ENAB(wlc->pub) && ((!bsscfg->wdswsec_enable && WSEC_ENABLED(bsscfg->wsec)) ||
		(bsscfg->wdswsec_enable &&((!SCB_WDS(scb)&&WSEC_ENABLED(bsscfg->wsec))||(SCB_WDS(scb)&&WSEC_ENABLED(bsscfg->wdswsec))))))))) {
#else
	if (!(f->key &&	WSEC_ENABLED(bsscfg->wsec))) {
#endif
		wlc_wsec_recvdata_enc_toss(wlc, f, scb);
		return FALSE;
	}

	/* security validation... */
	WL_WSEC(("wl%d.%d: %s: received encrypted frame\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
		__FUNCTION__));

	ASSERT(f->key != NULL);
	ASSERT(WSEC_KEY_INDEX(wlc, f->key) < WLC_MAX_WSEC_KEYS(wlc));

	/* check packet length */
	min_len = f->key->iv_len + f->key->icv_len;
	if (f->body_len < min_len) {
		/* toss the packet if length is too short */
		WL_WSEC(("wl%d.%d: invalid frame len\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		return FALSE;
	}
#ifdef BCMWAPI_WPI
	if (f->rxh->RxStatus1 & RXS_DECATMPT) {
		if (f->key && (f->key->algo == CRYPTO_ALGO_SMS4) && wlc->pr80838_war &&
			(WSEC_KEY_INDEX(wlc, f->key) < WSEC_MAX_DEFAULT_KEYS))
		{
			f->rxh->RxStatus1 &= ~RXS_DECATMPT;
		}
	}
#endif /* BCMWAPI_WPI */

#ifdef BCMDBG
	if (f->rxh->RxStatus1 & RXS_DECATMPT) {
		WL_WSEC(("wl%d.%d:  RXS_DECATMPT\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
	}

	if (f->rxh->RxStatus1 & RXS_DECERR) {
		WL_WSEC(("wl%d.%d:  RXS_DECERR\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
	}
#endif
	switch (f->key->algo) {
	case CRYPTO_ALGO_TKIP:
		iv16 = (piv[0] << 8) | piv[2];
		iv32 = ltoh32_ua(&piv[4]);
		if ((f->WPA_auth & WPA_AUTH_NONE) == 0 &&
		    wlc_wsec_isreplay(wlc, f->key, ividx, iv32, iv16, f->ismulti)) {
			WL_WSEC(("wl%d.%d:  TKIP replay detected: ividx %d, got 0x%08x%04x"
				" expected greater than or equal to 0x%08x%04x, retry 0x%x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), ividx, iv32, iv16,
				f->key->rxiv[ividx].hi, f->key->rxiv[ividx].lo, f->fc & FC_RETRY));

			if (BSSCFG_STA(bsscfg)) {
				WLCNTINCR(wlc->pub->_cnt->rxundec);
				if (f->ismulti)
					WLCNTINCR(wlc->pub->_cnt->rxundec_mcst);
				{
					WLCNTINCR(wlc->pub->_cnt->tkipreplay);
#ifdef STA
					if (bsscfg->pm->pm2_refresh_badiv) {
						wlc_pm2_ret_upd_last_wake_time(bsscfg, NULL);
					}
#endif /* STA */
					if (f->ismulti)
						WLCNTINCR(wlc->pub->_cnt->tkipreplay_mcst);
				}
			}
			return FALSE;
		}
		WL_WSEC(("wl%d.%d:  TKIP iv32 0x%08x iv16 0x%04x\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg), iv32, iv16));
		break;
	case CRYPTO_ALGO_AES_CCM:
		iv16 = (piv[1] << 8) | piv[0];
		iv32 = (piv[7] << 24) | (piv[6] << 16) | (piv[5] << 8) | (piv[4]);
		if ((f->WPA_auth & WPA_AUTH_NONE) == 0 &&
		    wlc_wsec_isreplay(wlc, f->key, ividx, iv32, iv16, f->ismulti)) {
			WL_WSEC(("wl%d.%d:  AES replay detected: ividx %d, got 0x%08x%04x"
				" expected greater than or equal to 0x%08x%04x, retry 0x%x\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), ividx, iv32, iv16,
				f->key->rxiv[ividx].hi, f->key->rxiv[ividx].lo, f->fc & FC_RETRY));

			if (BSSCFG_STA(bsscfg)) {
				WLCNTINCR(wlc->pub->_cnt->rxundec);
				if (f->ismulti)
					WLCNTINCR(wlc->pub->_cnt->rxundec_mcst);
				{
					WLCNTINCR(wlc->pub->_cnt->ccmpreplay);
#ifdef STA
					if (bsscfg->pm->pm2_refresh_badiv) {
						wlc_pm2_ret_upd_last_wake_time(bsscfg, NULL);
					}
#endif /* STA */
					if (f->ismulti)
						WLCNTINCR(wlc->pub->_cnt->ccmpreplay_mcst);
				}
			}
			return FALSE;
		}
		WL_WSEC(("wl%d.%d:  AES iv32 0x%08x iv16 0x%04x\n", wlc->pub->unit,
			WLC_BSSCFG_IDX(bsscfg), iv32, iv16));
		break;
#ifdef BCMWAPI_WPI
		case CRYPTO_ALGO_SMS4:
		{
			struct wpi_iv *wapi_rxiv;
			wapi_rxiv = (struct wpi_iv *)piv;
			/* reply check for SMS4 */
			if (bcm_cmp_bytes(&wapi_rxiv->PN[0], &f->key->wapi_rxiv.PN[0],
				SMS4_WPI_PN_LEN) < 0) {
				WL_ERROR(("WAPI: detected reply\n"));
				bcm_print_bytes("GOT: ", &wapi_rxiv->PN[0], SMS4_WPI_PN_LEN);
				bcm_print_bytes("EXP: ", &f->key->wapi_rxiv.PN[0], SMS4_WPI_PN_LEN);
				return FALSE;
			}
			break;
		}
#endif /* BCMWAPI_WPI */
	default:
		/* No replay check for WEP */
		break;
	}

	if (WLPKTFLAG_PMF(WLPKTTAG(f->p))) {
		ASSERT(!(f->rxh->RxStatus1 & RXS_DECATMPT));
	}

	/* Software Decryption */

	/* !!! assume received frame is in a single, contiguous buffer */
	if (WLC_SW_KEYS(wlc, bsscfg) ||
	    (f->key->algo == CRYPTO_ALGO_TKIP && ETHER_ISNULLADDR(&f->key->ea)) ||
	    WLPKTFLAG_PMF(WLPKTTAG(f->p)) ||
	    !(f->rxh->RxStatus1 & RXS_DECATMPT)) {
#ifndef LINUX_CRYPTO
		uint8 key_data[DOT11_MAX_KEY_SIZE + DOT11_MAX_IV_SIZE];
		rc4_ks_t ks;
#endif
#ifdef CTFMAP
		bool pkt_is_fast = PKTISFAST(osh, f->p);
#endif

		if (WLPKTTAG(f->p)->flags & WLF_AMSDU) {
			WL_ERROR((" A-MSDU deagg only supports hardware AES, toss\n"));
			return FALSE;
		}

		WL_WSEC(("wl%d.%d: software decryption\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));

		f->rxh->RxStatus1 |= RXS_DECATMPT;

#ifdef BCMDBG
		if (WL_WSEC_DUMP_ON())
			prhex("   wepsw-rx pre-decryption", PKTDATA(osh, f->p), PKTLEN(osh, f->p));
#endif

		/* Map the remaining unmapped frame contents before sw decryption */
		PKTCTFMAP(osh, f->p);

		/* decrypt */
		switch (f->key->algo) {
		case CRYPTO_ALGO_TKIP:
			WL_WSEC(("  TKIP soft decryption\n"));
			if (!(f->pbody[3] & DOT11_EXT_IV_FLAG)) {
				WL_ERROR(("use TKIP key but extended IV flag not set on frame\n"));
			}

#ifdef LINUX_CRYPTO
			{
			int err;
			int hdr_len = ((uchar *)f->pbody - (uchar *)f->h);
			err = wl_tkip_decrypt(wlc->wl, f->p, hdr_len, f->ismulti);
			/* linux kernel tkip module removes the iv and icv after the above
			 * routine returns. pbody pointer will be adjust it later
			 */
			f->h = (struct dot11_header *)(PKTDATA(osh, f->p));
			ext_tkip = TRUE;
			f->rxh->RxStatus1 &= ~RXS_DECERR;
			if (err < 0)
				f->rxh->RxStatus1 |=  RXS_DECERR;
			}
#else /* LINUX_CRYPTO */

			/* recompute phase1 if necessary */

			if ((ETHER_ISNULLADDR(&f->key->ea) && !bsscfg->BSS) ||
			    (iv32 > f->key->tkip_rx_iv32) ||
			    (iv32 != f->key->tkip_rx_iv32 && ividx != f->key->tkip_rx_ividx)) {
				tkhash_phase1(f->phase1, f->key->data, &(f->h->a2.octet[0]), iv32);

				f->pp1 = f->phase1;
				WL_WSEC((" TKIP recompute phase 1 hash: iv32 0x%08x frame %u"
					 "ividx %d ta %02x:%02x:%02x:%02x:%02x:%02x\n",
					 iv32, f->seq >> SEQNUM_SHIFT, ividx,
					 f->h->a2.octet[0], f->h->a2.octet[1],
					 f->h->a2.octet[2], f->h->a2.octet[3],
					 f->h->a2.octet[4], f->h->a2.octet[5]));
#ifdef BCMDBG
				if (WL_WSEC_DUMP_ON())
					prhex("  key data", (uchar *) f->key->data, TKIP_KEY_SIZE);
#endif
			} else {
				f->pp1 = f->key->tkip_rx.phase1;
			}

			tkhash_phase2(f->phase2, f->key->data, f->pp1, iv16);

			prepare_key(f->phase2, TKHASH_P2_KEY_SIZE, &ks);
#ifdef BCMDBG
			if (WL_WSEC_DUMP_ON()) {
				prhex("   TKIP rx phase1 hash", (uchar *)(f->pp1),
					TKHASH_P1_KEY_SIZE);
				prhex("   TKIP rx phase2 hash", (uchar *)(f->phase2),
					TKHASH_P2_KEY_SIZE);
				prhex("   rc4 keystate", (uchar *)&ks, sizeof(ks));
			}
#endif /* BCMDBG */

			rc4(f->pbody + f->key->iv_len, f->body_len - f->key->iv_len, &ks);

			/* check ICV */
			if (hndcrc32(f->pbody + f->key->iv_len, f->body_len - f->key->iv_len,
				CRC32_INIT_VALUE) == CRC32_GOOD_VALUE) {
				f->rxh->RxStatus1 &= ~RXS_DECERR;
				WL_WSEC((" SW TKIP checking ICV OK\n"));
			} else {
				f->rxh->RxStatus1 |= RXS_DECERR;
				WL_WSEC((" SW TKIP checking ICV error\n"));
			}
#endif /* LINUX_CRYPTO */
#ifdef CTFMAP
			if (pkt_is_fast) {
				/* Flush all software decrypted data */
				DMA_MAP(osh, PKTDATA(osh, f->p), PKTLEN(osh, f->p),
					DMA_TX, NULL, NULL);
			}
#endif
			break;

		case CRYPTO_ALGO_WEP1:
		case CRYPTO_ALGO_WEP128:
#ifndef LINUX_CRYPTO
			WL_WSEC(("wl%d.%d:  WEP\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));

			if (f->pbody[3] & DOT11_EXT_IV_FLAG) {
				WL_ERROR((" use WEP key but extended IV flag is set on frame\n"));
			}

			bcopy(f->pbody, key_data, 3);
			bcopy(f->key->data, &key_data[3], f->key->len);
			prepare_key(key_data, 3 + f->key->len, &ks);
			rc4(f->pbody + f->key->iv_len, f->body_len - f->key->iv_len, &ks);

			/* check ICV */
			if (hndcrc32(f->pbody + f->key->iv_len, f->body_len -
				f->key->iv_len, CRC32_INIT_VALUE) == CRC32_GOOD_VALUE) {
				f->rxh->RxStatus1 &= ~RXS_DECERR;
				WL_WSEC((" SW WEP checking ICV OK\n"));
			} else {
				f->rxh->RxStatus1 |= RXS_DECERR;
				WL_WSEC((" SW WEP checking ICV error\n"));
			}
#endif /* LINUX_CRYPTO */
#ifdef CTFMAP
			if (pkt_is_fast) {
				/* Flush all software decrypted data */
				DMA_MAP(osh, PKTDATA(osh, f->p), PKTLEN(osh, f->p),
					DMA_TX, NULL, NULL);
			}
#endif
			break;

#if defined BCMCCMP
		case CRYPTO_ALGO_AES_CCM:
			if (!(f->pbody[3] & DOT11_EXT_IV_FLAG)) {
				WL_ERROR((" use CCMP but extended IV flag not set on frame\n"));
				WLCNTINCR(wlc->pub->_cnt->rxundec);
				WLCNTINCR(wlc->pub->_cnt->ccmpundec);
				if (f->ismulti) {
					WLCNTINCR(wlc->pub->_cnt->rxundec_mcst);
					WLCNTINCR(wlc->pub->_cnt->ccmpundec_mcst);
				}
				return FALSE;
			}

#ifdef BCMCCMP
			{
				uint32 rk[4*(AES_MAXROUNDS+1)];
				int status;

				WL_WSEC((" CCMP soft decryption D7.0\n"));
				rijndaelKeySetupEnc(rk, f->key->data, AES_KEY_BITLEN(f->key->len));
				nonce_1st_byte = wlc_get_nonce_1st_byte(scb, f->h, f->p, FALSE);
				status = aes_ccmp_decrypt(rk, f->key->len, f->len, (uint8 *)(f->h),
					FALSE, nonce_1st_byte);
				if (status == AES_CCMP_DECRYPT_MIC_FAIL) {
					WL_ERROR((" CCMP MIC error\n"));
					f->rxh->RxStatus1 |= RXS_DECERR;
				} else if (status == AES_CCMP_DECRYPT_ERROR) {
					WL_ERROR((" CCMP decryption failed!\n"));
					{
						WLCNTINCR(wlc->pub->_cnt->rxundec);
						WLCNTINCR(wlc->pub->_cnt->ccmpundec);
						if (f->ismulti) {
							WLCNTINCR(wlc->pub->_cnt->rxundec_mcst);
							WLCNTINCR(wlc->pub->_cnt->ccmpundec_mcst);
						}
					}
					return FALSE;
				}
			}
#endif	/* BCMCCMP */
			break;
#endif 

#ifdef BCMWAPI_WPI
			case CRYPTO_ALGO_SMS4:
			{
				int err;
				WL_WSEC(("Trying to decrypt the frame, f->len is %d\n", f->len));
				err = sms4_wpi_pkt_decrypt(&f->key->data[0],
					&f->key->data[SMS4_KEY_LEN],
					f->len, (uint8 *)f->h);
				WL_WSEC(("Status of decrypt is %d\n", err));
				if (err != SMS4_WPI_SUCCESS)
					return FALSE;
#ifdef BCMWAPI_WAI
				/* Key receive cipher packets counter */
				f->key->wapi_rx++;
				if ((f->key->wapi_rx % WAPI_USK_REKEY_COUNT) == 0) {
					wlc_wapi_station_event(wlc->wapi, bsscfg, &scb->ea, NULL,
						NULL, WAPI_UNICAST_REKEY);
				}
#endif /* BCMWAPI_WAI */
				break;
			}
#endif /* BCMWAPI_WPI */

		default:
			WL_WSEC(("wl%d.%d:  unsupported algorithm %d\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), f->key->algo));

			if (BSSCFG_STA(bsscfg)) {
				WLCNTINCR(wlc->pub->_cnt->rxundec);
				if (f->ismulti)
					WLCNTINCR(wlc->pub->_cnt->rxundec_mcst);
			}
			return FALSE;
		}
#ifdef BCMDBG
		if (WL_WSEC_DUMP_ON())
			prhex("  wepsw-rx post-decryption", PKTDATA(osh, f->p), PKTLEN(osh, f->p));

#endif
		WLCNINC(wlc->pub->swdecrypt);
	}

	/* remove WEP encap on body by striping IV and ICV */
	f->pbody += f->key->iv_len;
	f->body_len -= f->key->iv_len + f->key->icv_len;
	f->totlen -= f->key->iv_len + f->key->icv_len;

	/* check ICV error */
	if ((f->rxh->RxStatus1 & RXS_DECATMPT) && (f->rxh->RxStatus1 & RXS_DECERR)) {
#if defined(BCMDBG_ERR)
		char addrstr[32];
#endif
		WL_ERROR(("wl%d.%d: %s ICV error using key index %d,"
			" dropping %scast frame %u ividx %d from %s\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), __FUNCTION__, f->key_index,
			f->ismulti ? "multi" : "uni", f->seq >> SEQNUM_SHIFT, ividx,
			bcm_ether_ntoa(&(f->h->a2), (char*)addrstr)));

		if (BSSCFG_STA(bsscfg)) {

			WLC_ICV_ERROR(wlc, &bsscfg->current_bss->BSSID, WLC_BSSCFG_IDX(bsscfg));
			WLCNTINCR(wlc->pub->_cnt->rxundec);
			WLCNTINCR(wlc->pub->_cnt->ccmpfmterr);
			WLCNTINCR(wlc->pub->_cnt->ccmpundec);
			if (f->ismulti) {
				WLCNTINCR(wlc->pub->_cnt->rxundec_mcst);
				WLCNTINCR(wlc->pub->_cnt->ccmpfmterr_mcst);
				WLCNTINCR(wlc->pub->_cnt->ccmpundec_mcst);
			}
			switch (f->key->algo) {
			case CRYPTO_ALGO_TKIP:
				{
					WLCNTINCR(wlc->pub->_cnt->tkipicverr);
					if (f->ismulti)
						WLCNTINCR(wlc->pub->_cnt->tkipicverr_mcst);
				}
				break;
			case CRYPTO_ALGO_WEP1:
			case CRYPTO_ALGO_WEP128:
				WLCNTINCR(wlc->pub->_cnt->wepicverr);
				WLCNTINCR(wlc->pub->_cnt->wepundec);
				if (f->ismulti) {
					WLCNTINCR(wlc->pub->_cnt->wepicverr_mcst);
					WLCNTINCR(wlc->pub->_cnt->wepundec_mcst);
				}
				break;
			}
		}
		return FALSE;
	}

	/* strip off ICV from base packet */
	f->len = PKTLEN(osh, f->p);
	if (f->key && !ext_tkip) {
		if (!f->isamsdu) {
			f->len -= f->key->icv_len;
			PKTSETLEN(osh, f->p, f->len);
		} else {
			void *pt = pktlast(osh, f->p);
			PKTSETLEN(osh, pt, PKTLEN(osh, pt) - f->key->icv_len);
		}

		f->iv32 = iv32;
		f->iv16 = iv16;
	}

	return TRUE;
}

#define BTAMP_SNAP(pbody) 0

#define DOT11_SNAP(pbody) \
	(bcmp(wlc_802_1x_hdr, (pbody), DOT11_LLC_SNAP_HDR_LEN) == 0)

/* Check if the received frame passes security filter */
bool
wlc_wsec_recvdata(wlc_info_t *wlc, osl_t *osh, struct scb *scb, struct wlc_frminfo *f, uint8 prio)
{
	struct wlc_bsscfg *bsscfg = SCB_BSSCFG(scb);

	/* unencrypted */
	if (!f->rx_wep) {
#ifdef DSLCPE_WDSSEC
		if ((!bsscfg->wdswsec_enable && WSEC_ENABLED(bsscfg->wsec) && bsscfg->wsec_restrict) ||
		    (bsscfg->wdswsec_enable&&((!SCB_WDS(scb)&&WSEC_ENABLED(bsscfg->wsec)&& bsscfg->wsec_restrict)||(SCB_WDS(scb)&&WSEC_ENABLED(bsscfg->wdswsec))))) {
#else
		if (WSEC_ENABLED(bsscfg->wsec) && bsscfg->wsec_restrict) {
#endif
#if defined(BCMDBG_ERR)
			char addrstr[32];
#endif
			/* accept unencrypted 802.1x, but only if WPA is disabled, or if WPA is
			 * enabled but we don't have a per-path key
			 */
			if ((((f->seq & FRAGNUM_MASK) == 0) &&
			     ((f->body_len < DOT11_LLC_SNAP_HDR_LEN) ||
			      (!BTAMP_SNAP(f->pbody) && !DOT11_SNAP(f->pbody) &&
#ifdef BCMWAPI_WAI
			       !WAPI_WAI_SNAP(f->pbody) &&
#endif
			       TRUE))) ||
			    (((f->seq & FRAGNUM_MASK) != 0) &&
			     ((f->seq & ~FRAGNUM_MASK) == (scb->seqctl[prio] & ~FRAGNUM_MASK)) &&
			     ((scb->flags & SCB_8021XHDR) == 0) &&
#ifdef BCMWAPI_WAI
			     ((scb->flags2 & SCB2_WAIHDR) == 0) &&
#endif
			     TRUE) ||
			    ((f->WPA_auth != WPA_AUTH_DISABLED) &&
#ifdef BCMWAPI_WAI
			     (!IS_WAPI_AUTH(f->WPA_auth)) &&
#endif /* BCMWAPI_WAI */
			     WSEC_SCB_KEY_VALID(scb))) {
				WL_ERROR(("wl%d.%d: unsupported unencrypted frame from %s\n",
				          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg),
				          bcm_ether_ntoa(&(f->h->a2), (char*)addrstr)));
				WLCNTINCR(wlc->pub->_cnt->rxbadproto);
				WLCNTINCR(wlc->pub->_cnt->wepexcluded);
				if (f->ismulti)
					WLCNTINCR(wlc->pub->_cnt->wepexcluded_mcst);
				return FALSE;
			}
		}
	} else {
		return wlc_wsec_recvdata_decrypt(wlc, osh, scb, f, prio);
	}

	return TRUE;
}

#ifndef LINUX_CRYPTO
/* sw encryption function. PKTDATA(p) is the beginning of d11 hdr */
bool
wlc_wsec_sw_encrypt_data(wlc_info_t *wlc, osl_t *osh, void *p, wlc_bsscfg_t *cfg,
	struct scb *scb, wsec_key_t *key)
{
	uint8 key_data[DOT11_MAX_KEY_SIZE + DOT11_MAX_IV_SIZE];
	rc4_ks_t ks;
	uchar *pbody;
	uint len, body_len;
	struct dot11_header *h;

	uint32 icv;
#if defined(BCMCCMP)
	uint8 nonce_1st_byte;
#endif
#ifdef BCMCCMP
	uint32 rk[4*(AES_MAXROUNDS+1)];
#endif

	h = (struct dot11_header *)PKTDATA(osh, p);
	len = PKTLEN(osh, p);

	pbody = (uchar*)h + DOT11_A3_HDR_LEN;
	body_len = len - DOT11_A3_HDR_LEN;
	if (!WLPKTFLAG_PMF(WLPKTTAG(p))) {
		if (SCB_A4_DATA(scb)) {
			pbody += ETHER_ADDR_LEN;
			body_len -= ETHER_ADDR_LEN;
		}
		if (SCB_QOS(scb)) {
			pbody += DOT11_QOS_LEN;
			body_len -= DOT11_QOS_LEN;
		}
	}

	/* skip IV */
	pbody += key->iv_len;
	body_len -= key->iv_len;
	WL_INFORM(("Packet being encrypted is of size %d, body_len %d\n", len, body_len));

	/* Map the remaining unmapped frame contents. This is to cover for the
	 * cases where we receive hw decrypted frame and have to send it using sw
	 * encryption.
	 */
	PKTCTFMAP(osh, p);

#ifdef BCMDBG
	if (WL_WSEC_DUMP_ON())
		prhex("  wlc_wsec_sw_encrydata: wepsw-tx pre-encryption",
		      (uchar*)h, len);
#endif

	/* prepare key */
	switch (key->algo) {
	case CRYPTO_ALGO_TKIP:
		WL_WSEC(("wl%d: wlc_wsec_sw_encrydata: TKIP\n", wlc->pub->unit));
		prepare_key(key->tkip_tx.phase2, TKHASH_P2_KEY_SIZE, &ks);

		/* calculate ICV on 'data' portion of frame only */
		icv = ~hndcrc32(pbody, body_len, CRC32_INIT_VALUE);
#if defined(BCMDBG)
		/* If ICV error flag set, set the ICV incorrectly, then clear the flag. */
		icv += (key->flags & WSEC_ICV_ERROR);
		key->flags &= ~WSEC_ICV_ERROR;
#endif /* defined(BCMDBG) */
		icv = htol32(icv);

		/* Append ICV to frag */
		bcopy((uchar *)&icv, pbody + body_len, key->icv_len);
		PKTSETLEN(osh, p, PKTLEN(osh, p) + key->icv_len);
		body_len += key->icv_len;

		/* encrypt */
		rc4(pbody, body_len, &ks);
		break;

	case CRYPTO_ALGO_WEP1:
	case CRYPTO_ALGO_WEP128: {
		uchar *piv = pbody - key->iv_len;
		WL_WSEC(("wl%d: wlc_wsec_sw_encrydata: WEP\n", wlc->pub->unit));
		bcopy(piv, key_data, 3);
		bcopy(key->data, &key_data[3], key->len);
		prepare_key(key_data, 3 + key->len, &ks);

		/* calculate ICV on 'data' portion of frame only */
		icv = ~hndcrc32(pbody, body_len, CRC32_INIT_VALUE);
#if defined(BCMDBG)
		/* If ICV error flag set, set the ICV incorrectly, then clear the flag. */
		icv += (key->flags & WSEC_ICV_ERROR);
		key->flags &= ~WSEC_ICV_ERROR;
#endif /* defined(BCMDBG) */
		icv = htol32(icv);

		/* Append ICV to frag */
		bcopy((uchar *)&icv, pbody + body_len, key->icv_len);
		PKTSETLEN(osh, p, PKTLEN(osh, p) + key->icv_len);
		body_len += key->icv_len;

		/* encrypt */
		rc4(pbody, body_len, &ks);
		break;
	}

#if defined BCMCCMP
	case CRYPTO_ALGO_AES_CCM:
		PKTSETLEN(osh, p, PKTLEN(osh, p) + key->icv_len);
#ifdef BCMCCMP
			WL_WSEC(("wl%d: wlc_wsec_sw_encrydata: CCMP D7.0\n", wlc->pub->unit));
			rijndaelKeySetupEnc(rk, key->data, AES_KEY_BITLEN(key->len));
#ifdef MFP
			if (WLC_MFP_ENAB(wlc) &&
				wlc_mfp_check_key_error(wlc->mfp, ltoh16(h->fc), key)) {
				rk[0] = ~rk[0];
				rk[1] = ~rk[1];
				rk[2] = ~rk[2];
			}
#endif /* MFP */
			nonce_1st_byte = wlc_get_nonce_1st_byte(scb, h, p, FALSE);
			if (aes_ccmp_encrypt(rk, key->len, len, (uint8 *)h, FALSE,
				nonce_1st_byte) != 0) {
				WL_ERROR(("wl%d: wlc_wsec_sw_encrydata: CCMP encryption failed\n",
					wlc->pub->unit));
				return FALSE;
			}
#endif	/* BCMCCMP */
		break;
#endif	

#ifdef BCMWAPI_WPI
	case CRYPTO_ALGO_SMS4:
	{
		int err;
		err = sms4_wpi_pkt_encrypt(&key->data[0], &key->data[SMS4_KEY_LEN],
			len, (uint8 *)h);
		if (err != 0) {
			WL_ERROR(("%s: SMS4 Crypt Error: %d, for packet %p, pkt_len %d\n",
				__FUNCTION__, err, p, len));
			ASSERT(0);
		}
#ifdef BCMWAPI_WAI
		/* Increment transmit multicast cipher packets counter */
		if (ETHER_ISNULLADDR(&key->ea))
			key->wapi_tx++;
#endif /* BCMWAPI_WAI */
	}
	break;
#endif /* BCMWAPI_WPI */

	default:
		WL_WSEC(("wl%d: wlc_wsec_sw_encrydata: unsupported algorithm %d\n", wlc->pub->unit,
			key->algo));
	}

#ifdef BCMDBG
	if (WL_WSEC_DUMP_ON())
		prhex("  wlc_wsec_sw_encrydata: wepsw-tx post-encryption",
		      (uchar*)h, len + key->icv_len);
#endif

	return TRUE;
}
#endif /* LINUX_CRYPTO */

void
wlc_wsec_rxiv_update(wlc_info_t *wlc, struct wlc_frminfo *f)
{
	if (f->iv16 == 0xffff)
		f->key->rxiv[f->ividx].hi = f->iv32 + 1;
	else
		f->key->rxiv[f->ividx].hi = f->iv32;
	f->key->rxiv[f->ividx].lo = f->iv16 + 1;
}

bool
#ifdef LINUX_CRYPTO
wlc_wsec_miccheck(wlc_info_t *wlc, osl_t *osh, struct scb *scb, struct wlc_frminfo *f)
{
	wsec_key_t *key = f->key;
	int mic_error;

	if (key->algo != CRYPTO_ALGO_TKIP)
		return TRUE;

	if ((mic_error = wl_tkip_miccheck(wlc->wl, f->p,
		((uchar *)f->pbody - (uchar *)f->h), f->ismulti, key->id))) {
		wl_tkip_printstats(wlc->wl, f->ismulti);
		return FALSE;
	}
	return TRUE;
}
#else /* LINUX_CRYPTO */
wlc_wsec_miccheck(wlc_info_t *wlc, osl_t *osh, struct scb *scb, struct wlc_frminfo *f)
{
	wsec_key_t *key = f->key;
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	uint8 ividx = f->ividx;
	uint16 iv16 = f->iv16;
	uint32 iv32 = f->iv32;
	uint len;
	struct dot11_header *h = f->h;
#ifdef BCMDBG_ERR
	char addrstr[32];
	char addrstr2[32];
#endif /* BCMDBG_ERR */

	struct ether_header ehl;
	uint32 qos;
	uchar *pbody = f->pbody;
	uint body_len = f->body_len;
	bool mic_error;
	int key_offset;

	switch (key->algo) {
	case CRYPTO_ALGO_TKIP: {
		WL_WSEC(("wl%d: %s: TKIP MIC check\n", wlc->pub->unit, __FUNCTION__));

		/* check packet length */
		if (body_len < TKIP_MIC_SIZE) {
			WL_WSEC(("wl%d: %s: invalid frame length\n", wlc->pub->unit, __FUNCTION__));
			return FALSE;
		}

		/* strip MIC  - part 1 - packet length still has MIC; this is
		 * so that PKTCFMAP call below  will also map the MIC
		 */
		f->len = len = PKTLEN(osh, f->p) - TKIP_MIC_SIZE;
		body_len -= TKIP_MIC_SIZE;
		f->body_len = body_len;

		/* d11 cores rev13 and higher support hardware TKIP MIC for non-frag frame */
		if (D11REV_GE(wlc->pub->corerev, 13) && (f->rxh->RxStatus2 & RXS_TKMICATMPT) &&
			!(h->fc & FC_MOREFRAG) && !(h->seq & FRAGNUM_MASK)) {
			WL_WSEC((" hardware TKIP MIC check\n"));

			if ((mic_error = ((f->rxh->RxStatus2 & RXS_TKMICERR) ? TRUE : FALSE)))
				WL_ERROR((" TKIP MIC failure frame %u blen %u ividx %d, RxStatus2"
					"0x%04x, da %s, sa %s\n", f->seq >> SEQNUM_SHIFT,
					body_len, ividx, f->rxh->RxStatus2,
					bcm_ether_ntoa((f->da), addrstr),
					bcm_ether_ntoa((f->sa), addrstr2)));

			/* strip MIC part 2 */
			PKTSETLEN(osh, f->p, len);

		} else {
			uint32 l, r, rxl, rxr;
			uint body_eom_len;
			uchar *micp;

			WL_WSEC((" software TKIP MIC check\n"));

			/* Map the remaining unmapped frame contents before doing
			 * sw tkip mic check. Note that MIC is included in the
			 * mapping.
			 */
#ifdef CTFMAP
#if defined(linux) && defined(__ARM_ARCH_7A__)
			if (PKTISCTF(osh, f->p))
				CTFMAPPTR(osh, f->p) = (void *)PKTDATA(osh, f->p);
#endif

			PKTCTFMAP(osh, f->p);
#endif	/* CTFMAP */

			/* Save received MIC and strip MIC (part 2) */
			micp = (uchar *)(f->eh) + PKTLEN(osh, f->p) - TKIP_MIC_SIZE;
			rxl = ltoh32_ua(micp);
			rxr = ltoh32_ua(micp + TKIP_MIC_SIZE/2);
			PKTSETLEN(osh, f->p, len);

			/* Calculate MIC on DA and SA. They need to be contiguous */
			bcopy((char*)(f->da), (char*)&ehl.ether_dhost, ETHER_ADDR_LEN);
			bcopy((char*)(f->sa), (char*)&ehl.ether_shost, ETHER_ADDR_LEN);

			key_offset = wlc_wsec_rx_tkmic_offset(wlc->pub, bsscfg, scb);
			key->tkip_rx.micl = ltoh32(*(uint32*)(key->data + key_offset));
			key->tkip_rx.micr = ltoh32(*(uint32*)(key->data + key_offset + 4));

			tkip_mic(key->tkip_rx.micl, key->tkip_rx.micr, 2*ETHER_ADDR_LEN,
				(uint8 *)&ehl, &l, &r);
			key->tkip_rx.micl = l;
			key->tkip_rx.micr = r;

			qos = htol32(f->prio);
			tkip_mic(key->tkip_rx.micl, key->tkip_rx.micr, 4, (uint8 *)&qos,
				&l, &r);
			key->tkip_rx.micl = l;
			key->tkip_rx.micr = r;

			body_eom_len = tkip_mic_eom(pbody, body_len, 0);
			tkip_mic(key->tkip_rx.micl, key->tkip_rx.micr, body_eom_len, pbody,
				&l, &r);
#ifdef CTFMAP
			/* tkip_mic_eom padded the body, invalidate it.  */
			_DMA_MAP(osh, pbody + body_len, body_eom_len - body_len,
				DMA_RX, NULL, NULL);
#endif
			if ((mic_error = ((l != rxl) || (r != rxr))))
				WL_ERROR((" TKIP MIC failure frame %u blen %u ividx %d, got "
					"0x%08x,0x%08x, expected 0x%08x,0x%08x, da %s, sa %s\n",
					f->seq >> SEQNUM_SHIFT, body_len, ividx, rxl, rxr, l, r,
					bcm_ether_ntoa((f->da), addrstr),
					bcm_ether_ntoa((f->sa), addrstr2)));
		}

#ifdef TKIP_CM_TEST
		do {
			uint32 mask;

			/* should we ignore mic errors on rx? */
			if (mic_error) {
				mask = TKIP_CM_KEYERR_IGN|TKIP_CM_KEYERR_RX;
				if ((bsscfg->tkip_cm_keyerr & mask) == mask) {
					WL_ERROR(("wl%d.%d: tkip_cm_keyerr: ignoring mic error\n",
						WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg)));
					mic_error = false;
				}
				break;
			}

			/* should we trigger a mic error for bc or uc ? */
			mask = TKIP_CM_KEYERR_TRIGGER|TKIP_CM_KEYERR_RX;
			mask |= ETHER_ISNULLADDR(&key->ea) ? TKIP_CM_KEYERR_MC :
				TKIP_CM_KEYERR_UC;
			if ((bsscfg->tkip_cm_keyerr & mask) != mask)
				break;

			WL_ERROR(("wl%d.%d: tkip_cm_keyerr: rxerr once: trigger=0x%08x\n",
				WLCWLUNIT(wlc), WLC_BSSCFG_IDX(bsscfg),
				bsscfg->tkip_cm_keyerr));
			mic_error = TRUE;
			bsscfg->tkip_cm_keyerr &= ~TKIP_CM_KEYERR_TRIGGER;
		} while (0);
#endif /* TKIP_CM_TEST */

		if (mic_error) {
			if (BSSCFG_STA(bsscfg)) {
				bool is_group;
				struct pktq *q;

				{
					WLCNTINCR(wlc->pub->_cnt->tkipmicfaill);
					if (f->ismulti)
						WLCNTINCR(wlc->pub->_cnt->tkipmicfaill_mcst);
				}
				/* no countermeasures in IBSS (for now) */
				if (!bsscfg->BSS)
					return FALSE;

				if (bsscfg->tk_cm_dt == 0) {
					WL_WSEC((" TKIP countermeasures: indicating MIC failure"
						"...starting detect timer\n"));
					WLCNTINCR(wlc->pub->_cnt->tkipcntrmsr);
					if (f->ismulti)
						WLCNTINCR(wlc->pub->_cnt->tkipcntrmsr_mcst);
					/* start detect timer */
					bsscfg->tk_cm_dt = WPA_TKIP_CM_DETECT;
				} else {
					WL_WSEC((" TKIP countermeasures: indicating 2nd MIC"
						"failure before detect timer timeout...activate"
					         " countermeasures\n"));

					q = &bsscfg->wlcif->qi->q;

					WLCNTINCR(wlc->pub->_cnt->tkipcntrmsr);
					if (f->ismulti)
						WLCNTINCR(wlc->pub->_cnt->tkipcntrmsr_mcst);
					if (WIN7_AND_UP_OS(wlc->pub))
						/* clear common txq for if */
						pktq_flush(wlc->osh, q, TRUE,
							wlc_ifpkt_chk_cb, WLC_BSSCFG_IDX(bsscfg));
					else
						/* clear common txq */
						pktq_flush(wlc->osh, q, TRUE, NULL, 0);

					/* indicate intention to activate countermeasures
					 * after MIC failure indication is sent
					 */
					bsscfg->tk_cm_activate = TRUE;
				}

				/* signal MIC failure */
				is_group = ETHER_ISNULLADDR(&key->ea);
				WLC_MIC_ERROR(wlc, &bsscfg->current_bss->BSSID,
				              WLC_BSSCFG_IDX(bsscfg), is_group, key,
				              bsscfg->tk_cm_activate);
			}
#ifdef AP
			else if ((f->WPA_auth != WPA_AUTH_DISABLED) && WSEC_ENABLED(bsscfg->wsec)) {
				/* Authenticator may take countermeasures. */
				WLC_MIC_ERROR(wlc, f->sa, WLC_BSSCFG_IDX(bsscfg), FALSE,
				              key, FALSE);
			}
#endif /* AP */
			return FALSE;
		} else {
			WL_WSEC((" TKIP MIC success\n"));
			/* update the stored phase and save the current TSC */
			/* rxiv.lo and rxiv.hi are the next expected value */
			if (iv16 == 0xffff) {
				/* if iv16 is about to roll over, compute the next phase1 */
				WL_WSEC((" TKIP precomputing next phase1 hash: iv32 0x%08x frame"
					" %u ividx %d ta %02x:%02x:%02x:%02x:%02x:%02x\n",
					iv32 + 1, f->seq >> SEQNUM_SHIFT, ividx,
					h->a2.octet[0], h->a2.octet[1], h->a2.octet[2],
					h->a2.octet[3], h->a2.octet[4], h->a2.octet[5]));
				key->tkip_rx_iv32 = iv32 + 1;
				key->tkip_rx_ividx = ividx;
				tkhash_phase1(key->tkip_rx.phase1, key->data, &(h->a2.octet[0]),
					key->tkip_rx_iv32);
				wlc_key_update(wlc, WSEC_KEY_INDEX(wlc, key), bsscfg);
			}
			/* phase1 was recomputed, send it to h/w. see 'pp1 = phase1;' above */
			else if (f->pp1 == f->phase1) {
				WL_WSEC((" TKIP storing new phase1 hash: iv32 0x%08x frame %u "
					"ividx %d\n", iv32, h->seq >> SEQNUM_SHIFT, ividx));
				bcopy((uchar *)(f->phase1), (uchar *)key->tkip_rx.phase1,
				      TKHASH_P1_KEY_SIZE);
				key->tkip_rx_iv32 = iv32;
				key->tkip_rx_ividx = ividx;
				wlc_key_update(wlc, WSEC_KEY_INDEX(wlc, key), bsscfg);
			}
		}
		break;
	}

	default:
		break;	/* no MIC check is necessary */

		WLCNTINCR(wlc->pub->_cnt->decsuccess);
		if (f->ismulti)
			WLCNTINCR(wlc->pub->_cnt->decsuccess_mcst);
	}
	return TRUE;
}
#endif /* LINUX_CRYPTO */

static void
wlc_decrypt_error(wlc_info_t *wlc, struct ether_addr *addr, int bsscfg_idx,
	uint32 event_type, bool group, wsec_key_t *key, bool flush_txq)
{

	wlc_event_t *e;
	wlc_bsscfg_t *bsscfg;

	e = wlc_event_alloc(wlc->eventq);
	if (e == NULL) {
		WL_ERROR(("wl%d: wlc_decrypt_error wlc_event_alloc failed\n",
		          wlc->pub->unit));
		return;
	}
	e->event.event_type = event_type;
	e->event.flags = (group?WLC_EVENT_MSG_GROUP:0) | (flush_txq?WLC_EVENT_MSG_FLUSHTXQ:0);

	bsscfg = WLC_BSSCFG(wlc, bsscfg_idx);
	ASSERT(bsscfg != NULL);

	wlc_event_if(wlc, bsscfg, e, addr);

	wlc_process_event(wlc, e);
}

/* return transmit TKIP MIC key offset in the temporal key	*/
int
wlc_wsec_tx_tkmic_offset(wlc_pub_t *wlp, wlc_bsscfg_t *cfg, struct scb *scb)
{
	/*
	* For WPA IBSS, behave like authenticator w.r.t. MIC keys.
	* This will change for TGi IBSS, when there are real
	* authenticators and supplicants.
	*/
	if ((BSSCFG_AP(cfg) && (!scb || !SCB_LEGACY_WDS(scb) || (scb->flags & SCB_WPA_SUP))) ||
	    (BSSCFG_STA(cfg) && !cfg->BSS)) {
		if (WIN7_AND_UP_OS(wlp) && BSSCFG_HAS_NATIVEIF(cfg))
			/* Win7 swaps 16-byte mic keys for AP */
			return TKIP_MIC_SUP_TX;
		else
			return TKIP_MIC_AUTH_TX;
	} else {
		return TKIP_MIC_SUP_TX;
	}
}

/* return receive TKIP MIC key offset in the temporal key	*/
int
wlc_wsec_rx_tkmic_offset(wlc_pub_t *wlp, wlc_bsscfg_t *cfg, struct scb *scb)
{
	if ((BSSCFG_AP(cfg) && !(WIN7_AND_UP_OS(wlp) && BSSCFG_HAS_NATIVEIF(cfg)) &&
	     (!scb || !SCB_LEGACY_WDS(scb) || (scb->flags & SCB_WPA_SUP)))) {
		return TKIP_MIC_AUTH_RX;
	} else {
		return TKIP_MIC_SUP_RX;
	}
}

/* return length, including ETHER_HDR_LEN, of the next fragment
 * flen_hdr is frag_length + ETHER_HDR_LEN
 */
uint
wlc_wsec_tkip_nextfrag_len(wlc_pub_t *wlp, osl_t *osh, void *pkt_curr, void *pkt_next,
	uint flen_hdr)
{
	uint length;
	uint len_curr, len_next;

	ASSERT(pkt_curr != NULL);
	ASSERT(pkt_next != NULL);

	len_curr = pkttotlen(osh, pkt_curr);
	len_next = pkttotlen(osh, pkt_next);

	WL_WSEC(("wl%d: %s: len_curr %d len_next %d flen_hdr %d\n",
		wlp->unit, __FUNCTION__, len_curr, len_next, flen_hdr));

	ASSERT(len_curr <= flen_hdr);
	ASSERT(len_next <= flen_hdr);

	if (len_curr >= flen_hdr) {
		if (len_next + TKIP_MIC_SIZE > flen_hdr) {
			length = flen_hdr; /* full fragment */
		} else {
			length = len_next + TKIP_MIC_SIZE; /* entire MIC fits into fragment */
		}
	} else {
		/* since current pkt is not full, next one has just MIC */
		ASSERT(len_curr + TKIP_MIC_SIZE > flen_hdr);

		/* ETHER_HDR_LEN + MIC bytes to spill over to next frag */
		length = ETHER_HDR_LEN + TKIP_MIC_SIZE - (flen_hdr - len_curr);
	}

	WL_WSEC(("wl%d: %s: returns %d", wlp->unit, __FUNCTION__, length));
	return length;
}

#ifndef LINUX_CRYPTO
void
wlc_dofrag_tkip(wlc_pub_t *wlp, void *p, uint frag, uint nfrags, osl_t *osh,
	wlc_bsscfg_t *cfg, struct scb *scb, struct ether_header *eh, wsec_key_t *key,
	uint8 prio, uint frag_length)
{
	uint32 l, r;
	uint32 qos = htol32(prio);
	uint body_len, mic_len;
	uchar *pbody;
	void *micpkt = p;
	uint16 ahead;
	int key_offset;
	uint32 ml, mr;
	uint to_full;
	int fmic_written;
	bool eom_done = FALSE;
	uint bytestowrite;
	uint16 min_ahead;
	uchar *pbody_s;
	uint body_len_s;

	/* Map the unmapped buffer before doing sw mic */
	PKTCTFMAP(osh, p);

	WL_WSEC(("wl%d: %s: frag %d nfrags %d PKTLEN(osh,p) %d plen %d fmic_written %d\n",
		wlp->unit, __FUNCTION__, frag, nfrags, PKTLEN(osh, p), pkttotlen(osh, p),
		key->tkip_tx_fmic_written));

	pbody = PKTDATA(osh, p) + ETHER_HDR_LEN;
	body_len = PKTLEN(osh, p) - ETHER_HDR_LEN;
	mic_len = body_len;

	ASSERT(frag_length >= body_len);
	to_full = frag_length - body_len; /* this much room before becoming full */

	WL_WSEC(("wl%d: %s: after ETHER_HDR_LEN adj: pbody %p, body_len mic_len %d plen %d\n",
		wlp->unit, __FUNCTION__, pbody, mic_len, pkttotlen(osh, p)));

	if (!frag)
		key->tkip_tx_fmic_written = 0;

	fmic_written = key->tkip_tx_fmic_written;
	if (fmic_written) {
		ASSERT(fmic_written < TKIP_MIC_SIZE);
		ASSERT(!body_len);
		bytestowrite = TKIP_MIC_SIZE - fmic_written;

		bcopy((uchar *)&key->tkip_tx_fmic[fmic_written], pbody, bytestowrite);
		PKTSETLEN(osh, micpkt, PKTLEN(osh, micpkt) + bytestowrite);
		key->tkip_tx_fmic_written += bytestowrite;
		WL_WSEC(("wl%d: %s: tkip_tx_fmic_written %d"
			" tkip_tx_offset %d tkip_tx_lefts %d PKTLEN(osh, p) %d\n",
			wlp->unit, __FUNCTION__, key->tkip_tx_fmic_written,
			key->tkip_tx_offset, key->tkip_tx_lefts, PKTLEN(osh, p)));
		return;
	}

	if (frag) {
		/* not first frag, so pick up what's been done so far. */
		l = key->tkip_tx.micl;
		r = key->tkip_tx.micr;
	} else {
		key_offset = wlc_wsec_tx_tkmic_offset(wlp, cfg, scb);

		l = ltoh32(*(uint32*)(key->data + key_offset));
		r = ltoh32(*(uint32*)(key->data + key_offset + 4));

		WL_WSEC(("wl%d: %s: tkip_mic(body_len %d)\n",
			wlp->unit, __FUNCTION__, 2*ETHER_ADDR_LEN));
		tkip_mic(l, r, 2*ETHER_ADDR_LEN, (uint8 *)eh, &l, &r);

		WL_WSEC(("wl%d: %s: tkip_mic(body_len %d)\n", wlp->unit, __FUNCTION__, 4));
		tkip_mic(l, r, 4, (uint8 *)&qos, &l, &r);

		key->tkip_tx_offset = 0;
		key->tkip_tx_lefts = 0;

		WL_WSEC(("wl%d: %s: tkip_tx_offset %d tkip_tx_lefts %d tkip_tx_fmic_written %d\n",
			wlp->unit, __FUNCTION__, key->tkip_tx_offset, key->tkip_tx_lefts,
			key->tkip_tx_fmic_written));
	}

	/* Check for non-NULL next pointer.  If there is one, that's
	 *   where the MIC goes.
	 */
	if (PKTNEXT(osh, p)) {
		if (body_len) {
			/* take care of the leftover by borrowing into the current frag */
			if (key->tkip_tx_lefts && body_len >= (ahead = 4 - key->tkip_tx_lefts)) {
				WL_WSEC(("wl%d: %s: frag %d chained seg 1, tkip_tx_lefts %d"
					" body_len %d\n",
					wlp->unit, __FUNCTION__, frag, key->tkip_tx_lefts,
					body_len));
				bcopy(pbody, &key->tkip_tx_left[key->tkip_tx_lefts], ahead);
				WL_WSEC(("wl%d: %s: tkip_mic(body_len %d)\n",
					wlp->unit, __FUNCTION__, 4));
				tkip_mic(l, r, 4, key->tkip_tx_left, &l, &r);
				key->tkip_tx_offset += ahead;
				pbody += ahead;
				body_len -= ahead;
				key->tkip_tx_lefts = 0;
				WL_WSEC(("wl%d: %s: frag %d chained seg 1, tkip_tx_lefts %d"
					" body_len %d\n",
					wlp->unit, __FUNCTION__, frag, key->tkip_tx_lefts,
					body_len));
			}
			/* tkip_mic only works on 0 mod 4 buffer! */
			key->tkip_tx_offset += (uint16)body_len;
			key->tkip_tx_lefts = body_len & 3;
			body_len &= ~3;
			if (body_len) {
				WL_WSEC(("wl%d: %s: tkip_mic(body_len %d)\n",
					wlp->unit, __FUNCTION__, body_len));
				tkip_mic(l, r, body_len, pbody, &l, &r);
			}
			/* save the remaining bytes for next frag */
			bcopy(&pbody[body_len], key->tkip_tx_left, key->tkip_tx_lefts);
			WL_WSEC(("wl%d: %s: done frag %d tkip_tx_offset %d, tkip_tx_lefts %d\n",
				wlp->unit, __FUNCTION__, frag,
				key->tkip_tx_offset, key->tkip_tx_lefts));
		}

		/* Map the unmapped buffer before doing sw mic */
		PKTCTFMAP(osh, PKTNEXT(osh, p));

		micpkt = PKTNEXT(osh, p);
		pbody =  PKTDATA(osh, micpkt);
		body_len = PKTLEN(osh, micpkt);
		mic_len = body_len;
	}

	if (to_full > 0) {
		WL_WSEC(("wl%d: %s: tkip_mic_eom(pbody %p body_len %d tkip_tx_offset %d)\n",
			wlp->unit, __FUNCTION__, pbody, body_len, key->tkip_tx_offset));
		mic_len = tkip_mic_eom(pbody, body_len, key->tkip_tx_offset);
		eom_done = TRUE; /* final mic is computed */
		WL_WSEC(("wl%d: %s: mic_len from tkip_mic_eom() %d"
			" tkip_tx_offset %d, tkip_tx_lefts %d body_len %d\n",
			wlp->unit, __FUNCTION__, mic_len,
			key->tkip_tx_offset, key->tkip_tx_lefts, body_len));
	}

	min_ahead = 0;
	pbody_s = pbody;
	body_len_s = body_len;
	/* take care of the leftover by borrowing a few bytes from the current frag */
	if (key->tkip_tx_lefts && mic_len >= (ahead = 4 - key->tkip_tx_lefts)) {
		WL_WSEC(("wl%d: %s ahead %d tkip_tx_offset %d tkip_tx_lefts %d, body_len %d\n",
			wlp->unit, __FUNCTION__, ahead, key->tkip_tx_offset,
			key->tkip_tx_lefts, body_len));
		bcopy(pbody, &key->tkip_tx_left[key->tkip_tx_lefts], ahead);
		WL_WSEC(("wl%d: %s: tkip_mic(body_len %d)\n", wlp->unit, __FUNCTION__, 4));
		tkip_mic(l, r, 4, key->tkip_tx_left, &l, &r);

		min_ahead = MIN(body_len, ahead);

		key->tkip_tx_offset += ahead;
		pbody += ahead;
		mic_len -= ahead;
		key->tkip_tx_lefts = 0;
		WL_WSEC(("wl%d: %s: tkip_tx_offset %d tkip_tx_lefts %d body_len %d mic_len %d\n",
			wlp->unit, __FUNCTION__,
			key->tkip_tx_offset, key->tkip_tx_lefts, body_len, mic_len));
	}

	pbody_s += min_ahead;
	body_len_s -= min_ahead;

#ifdef BCMDBG
	if (eom_done) {
		WL_WSEC(("wl%d: %s: eom_done %d tkip_tx_offset %d tkip_tx_lefts %d\n",
			wlp->unit, __FUNCTION__,
			eom_done, key->tkip_tx_offset, key->tkip_tx_lefts));
		ASSERT(!(mic_len & 3));
	}
#endif /* def BCMDBG */

	/* tkip_mic only works on 0 mod 4 buffer! */
	key->tkip_tx_offset += (uint16)mic_len;
	key->tkip_tx_lefts = mic_len & 3;
	mic_len &= ~3;
	WL_WSEC(("wl%d: %s: frag %d tkip_tx_offset %d tkip_tx_lefts %d mic_len %d\n",
		wlp->unit, __FUNCTION__, frag, key->tkip_tx_offset, key->tkip_tx_lefts, mic_len));
	if (mic_len) {
		WL_WSEC(("wl%d: %s: tkip_mic(mic_len %d)\n", wlp->unit, __FUNCTION__, mic_len));
		tkip_mic(l, r, mic_len, pbody, &l, &r);
	}
	/* save the remaining bytes if any for next frag */
	bcopy(&pbody[mic_len], key->tkip_tx_left, key->tkip_tx_lefts);

	key->tkip_tx.micl = l;
	key->tkip_tx.micr = r;

	/* save final mic */
	if (eom_done) {
		ml = htol32(l);
		mr = htol32(r);

#ifdef TKIP_CM_TEST
		do {
			uint32 mask;

			/* need to trigger for UC or MC? */
			mask  = TKIP_CM_KEYERR_TRIGGER|TKIP_CM_KEYERR_TX;
			mask |= ETHER_ISNULLADDR(&key->ea) ? TKIP_CM_KEYERR_MC :
				TKIP_CM_KEYERR_UC;
			if ((cfg->tkip_cm_keyerr & mask) != mask)
				break;

			/* need to trigger for NONSTA src */
			mask  = TKIP_CM_KEYERR_TRIGGER|TKIP_CM_KEYERR_TX|
				TKIP_CM_KEYERR_NONSTA;
			if (BSSCFG_AP(cfg) && ((cfg->tkip_cm_keyerr & mask) == mask)) {
				struct scb *scb;
				struct scb_iter scbiter;
				wlc_info_t *wlc = (wlc_info_t *)wlp->wlc;
				bool from_mysta = FALSE;
				FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
					if (!memcmp(eh->ether_shost, scb->ea.octet, ETH_ALEN)) {
						from_mysta = TRUE;
						break;
					}
				}
				if (from_mysta) break;
			}

			WL_ERROR(("wl%d.%d: tkip_cm_keyerr: txerr once: trigger=0x%08x\n",
				wlp->unit, WLC_BSSCFG_IDX(cfg), cfg->tkip_cm_keyerr));
			ml = mr = ~0;
			cfg->tkip_cm_keyerr &= ~TKIP_CM_KEYERR_TRIGGER;
			prpkt("tkip_cm_keyerr", osh, micpkt);
		} while (0);
#endif /* TKIP_CM_TEST */

#if defined(BCMDBG)
		/* If TKIP MIC error flag is set, get MIC wrong and clear the flag. */
		ml += (key->flags & WSEC_TKIP_ERROR);
		key->flags &= ~WSEC_TKIP_ERROR;
#endif /* BCMDBG  */

		bcopy((uchar *)&ml, &key->tkip_tx_fmic[0], TKIP_MIC_SIZE/2);
		bcopy((uchar *)&mr, &key->tkip_tx_fmic[TKIP_MIC_SIZE/2], TKIP_MIC_SIZE/2);

		WL_WSEC(("wl%d: %s: saving final mic: ml 0x%x mr 0x%x\n",
			wlp->unit, __FUNCTION__, ml, mr));

		ASSERT(!key->tkip_tx_lefts);

		WL_WSEC(("wl%d: %s: writing fmic (l 0x%x r 0x%x) to %p\n",
			wlp->unit, __FUNCTION__, l, r, (void *)(pbody_s + body_len_s)));
		fmic_written = MIN(to_full, TKIP_MIC_SIZE);
		WL_WSEC(("wl%d: %s: before copy: fmic_written %d PKTLEN(osh, micpkt) %d\n",
			wlp->unit, __FUNCTION__, fmic_written, PKTLEN(osh, micpkt)));
		bcopy((uchar *)&key->tkip_tx_fmic[0], pbody_s + body_len_s, fmic_written);

		PKTSETLEN(osh, micpkt, PKTLEN(osh, micpkt) + (uint)fmic_written);
		WL_WSEC(("wl%d: %s: after copy: PKTLEN(osh, micpkt) %d copied to %p\n",
			wlp->unit, __FUNCTION__, PKTLEN(osh, micpkt), pbody_s + body_len_s));
		key->tkip_tx_fmic_written = fmic_written;
	}

	WL_WSEC(("wl%d: %s exits: tkip_tx_offset %d tkip_tx_lefts %d"
		" PKTLEN(osh, p) %d fmic_written %d\n",
		wlp->unit, __FUNCTION__, key->tkip_tx_offset, key->tkip_tx_lefts,
		PKTLEN(osh, p), key->tkip_tx_fmic_written));
}
#endif /* LINUX_CRYPTO */

#if defined(BCMCCMP)
static uint8
wlc_get_nonce_1st_byte(struct scb *scb, struct dot11_header *h, void *p, bool legacy)
{
	uint16 fc, type, subtype;
	uint16 qc = 0;
	uint8 nfb = 0;
	bool wds, qos;

	fc = ltoh16(h->fc);
	type = FC_TYPE(fc);
	if (WLPKTFLAG_PMF(WLPKTTAG(p))) {
		ASSERT(type == FC_TYPE_MNG);
#ifdef MFP
		if (SCB_MFP(scb))
			return AES_CCMP_NF_MANAGEMENT;
#endif
		return 0;
	}

	subtype = (fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT;
	wds = ((fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	/* all QoS subtypes have the FC_SUBTYPE_QOS_DATA bit set */
	qos = (type == FC_TYPE_DATA) && (subtype & FC_SUBTYPE_QOS_DATA);

	if (qos) {
		qc = ltoh16(*((uint16 *)((uchar *)h +
			(wds ? DOT11_A4_HDR_LEN : DOT11_A3_HDR_LEN))));
	}

	/* nonce = priority octet || A2 || PN */
	if (!legacy) {
		nfb = (uint8)(QOS_TID(qc) & AES_CCMP_NF_PRIORITY);
		return nfb;
	} else {
		/* 802.11i Draft 3.0:
		 * Clause 8.3.4.4.2: "QoS-TC occupies bits 103-96 of the Nonce
		 * (bits 119-112 of the Initial Block). This field is reserved
		 * for the QoS traffic class and shall be set to the fixed
		 * value 0 (0x00 hex)."
		 */

		/* Our implementation: 0 */
		return 0;
	}
}
#endif 
