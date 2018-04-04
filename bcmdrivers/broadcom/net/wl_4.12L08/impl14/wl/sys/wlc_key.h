/*
 * Key management related declarations
 * and exported functions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: wlc_key.h 381851 2013-01-30 03:33:18Z $
 */

#ifndef _wlc_key_h_
#define _wlc_key_h_

#ifndef LINUX_CRYPTO
#include <bcmcrypto/tkhash.h>
#endif

#ifndef LINUX_POSTMOGRIFY_REMOVAL
#ifdef BCMWAPI_WPI
#include <bcmcrypto/sms4.h>
#endif /* BCMWAPI_WPI */

struct scb;
struct wlc_info;
struct wlc_bsscfg;
/* Maximum # of keys that wl driver supports in S/W.
 * Keys supported in H/W is less than or equal to WSEC_MAX_KEYS.
 */
#ifdef AP
#define WSEC_MAX_KEYS		132	/* Max # of keys (128 + 4 default keys) */
#else
#define WSEC_MAX_KEYS		54	/* Max # of keys (50 + 4 default keys) */
#endif /* AP */
#define WLC_DEFAULT_KEYS	4	/* Default # of keys */

#define WSEC_MAX_WOWL_KEYS 5 /* Max keys in WOWL mode (1 + 4 default keys) */

#define WPA2_GTK_MAX	3

/*
* Max # of keys currently supported:
*
*     s/w keys if WSEC_SW(wlc->wsec).
*     h/w keys otherwise.
*/
#define WLC_MAX_WSEC_KEYS(wlc) WSEC_MAX_KEYS

/* number of 802.11 default (non-paired, group keys) */
#define WSEC_MAX_DEFAULT_KEYS	4	/* # of default keys */

/* Max # of hardware keys supported */
#define WLC_MAX_WSEC_HW_KEYS(wlc) (WOWL_ACTIVE((wlc)->pub) ? \
					WSEC_MAX_WOWL_KEYS : (wlc)->pub->wsec_max_rcmta_keys)

/* Max # of hardware TKIP MIC keys supported */
#define WLC_MAX_TKMIC_HW_KEYS(wlc) ((D11REV_GE((wlc)->pub->corerev, 13)) ? \
		WSEC_MAX_TKMIC_ENGINE_KEYS : 0)

#define WSEC_HW_TKMIC_KEY(wlc, key, bsscfg) \
	(((D11REV_GE((wlc)->pub->corerev, 13)) && ((wlc)->machwcap & MCAP_TKIPMIC)) && \
	 (key) && ((key)->algo == CRYPTO_ALGO_TKIP) && \
	 !WSEC_SOFTKEY(wlc, key, bsscfg) && \
	WSEC_KEY_INDEX(wlc, key) >= WLC_DEFAULT_KEYS && \
	(WSEC_KEY_INDEX(wlc, key) < WSEC_MAX_TKMIC_ENGINE_KEYS))

#ifdef BCMWAPI_WPI
/* Max # of hardware SMS4 MIC keys supported */
#define WLC_MAX_SMS4MIC_HW_KEYS(wlc) ((D11REV_GE((wlc)->pub->corerev, 24)) ? \
	WSEC_MAX_SMS4MIC_ENGINE_KEYS : 0)

#define SMS4_HW_KEY(wlc, key)   \
	(WAPI_HW_WPI_CAP(wlc) && WAPI_HW_WPI_ENAB(wlc) && \
	(key) && ((key)->algo == CRYPTO_ALGO_SMS4) && \
	(WSEC_KEY_INDEX(wlc, key) < WSEC_MAX_SMS4MIC_ENGINE_KEYS))

#define SMS4_SW_KEY(wlc, key)	(!(SMS4_HW_KEY(wlc, key)))
#define IS_SMS4_KEY(key)	(((key)->algo == CRYPTO_ALGO_SMS4) ? TRUE : FALSE)
#define WSEC_SMS4_SW_KEY(wlc, key) ((IS_SMS4_KEY(key) ? (SMS4_SW_KEY(wlc, key)) : 0))
#else /* BCMWAPI_WPI */
#define WSEC_SMS4_SW_KEY(wlc, key)      0
#endif /* BCMWAPI_WPI */

/* index of key in key table */
#define WSEC_KEY_INDEX(wlc, key)	((key)->idx)

#define WSEC_4360B0_WAR(wlc, key) ((key)->algo == CRYPTO_ALGO_TKIP && \
	(wlc->pub->sih->chip == BCM4360_CHIP_ID || \
	wlc->pub->sih->chip == BCM43460_CHIP_ID || \
	wlc->pub->sih->chip == BCM43526_CHIP_ID || \
	wlc->pub->sih->chip == BCM4352_CHIP_ID) && \
	wlc->pub->sih->chiprev == 3)

/* is this key to be used with software encrypt/decrypt */
#define WSEC_SOFTKEY(wlc, key, bsscfg) (WLC_SW_KEYS(wlc, bsscfg) || \
	WSEC_SMS4_SW_KEY(wlc, key) || \
	WSEC_KEY_INDEX(wlc, key) >= WLC_MAX_WSEC_HW_KEYS(wlc) || \
	WSEC_4360B0_WAR(wlc, key))

/* get a key, non-NULL only if key allocated and not clear */
#define WSEC_KEY(wlc, i)	(((wlc)->wsec_keys[i] && (wlc)->wsec_keys[i]->len) ? \
	(wlc)->wsec_keys[i] : NULL)

#define WSEC_SCB_KEY_VALID(scb)	(((scb)->key && (scb)->key->len) ? TRUE : FALSE)

/* default key */
#define WSEC_BSS_DEFAULT_KEY(bsscfg) (((bsscfg)->wsec_index == -1) ? \
	(struct wsec_key*)NULL : (bsscfg)->bss_def_keys[(bsscfg)->wsec_index])

/* Macros for key management in IBSS mode */
#define WSEC_IBSS_MAX_PEERS	16	/* Max # of IBSS Peers */
#define WSEC_IBSS_RCMTA_INDEX(idx) \
	(((idx - WSEC_MAX_DEFAULT_KEYS) % WSEC_IBSS_MAX_PEERS) + WSEC_MAX_DEFAULT_KEYS)

/* contiguous # key slots for infrastructure mode STA */
#define WSEC_BSS_STA_KEY_GROUP_SIZE	5

#ifndef LINUX_CRYPTO
typedef struct tkip_info {
	uint16		phase1[TKHASH_P1_KEY_SIZE/sizeof(uint16)];	/* tkhash phase1 result */
	uint8		phase2[TKHASH_P2_KEY_SIZE];	/* tkhash phase2 result */
	uint32		micl;
	uint32		micr;
} tkip_info_t;
#endif
#endif /* LINUX_POSTMOGRIFY_REMOVAL */

typedef struct wsec_iv {
	uint32		hi;	/* upper 32 bits of IV */
	uint16		lo;	/* lower 16 bits of IV */
} wsec_iv_t;

#ifdef WOWL
#if defined(MFP)
#if !(defined(NDIS) && (NDISVER >= 0x0630))
#error "MFP not currently supported with WOWL"
#endif /* !(defined(NDIS) && (NDISVER >= 0x0630)) */
#endif /* MFP */
#if defined(MFP)
#define WLC_NUMRXIVS	5	/* # rx IVs (one per 802.11e TID plus one for mfp */
#define WLC_MFP_IVIDX	4	/* index to mfp iv element */
#else
#define WLC_NUMRXIVS	4	/* # rx IVs (one per AC) */
#endif 
#else /* !WOWL */
#if defined(MFP)
#define WLC_NUMRXIVS	17	/* # rx IVs (one per 802.11e TID plus one for mfp */
#define WLC_MFP_IVIDX	16	/* index to mfp iv element */
#else
#define WLC_NUMRXIVS	16	/* # rx IVs (one per 802.11e TID) */
#endif 
#endif /* WOWL */

#if defined(BRCMAPIVTW)
#define TWSIZE	BRCMAPIVTW
/* IV trace window */
typedef struct iv_tw {
		uint8		bmp[(TWSIZE+7)>>3];	/* bitmap */
		wsec_iv_t	lb;			/* lower bound */
		wsec_iv_t	ub;			/* upper bound */
	} iv_tw_t;
#else
#define TWSIZE	128
#endif	/* BRCMAPIVTW */

typedef struct wsec_key {
	struct ether_addr ea;		/* per station */
	uint8		idx;		/* key index in wsec_keys array */
	uint8		id;		/* key ID [0-3] */
	uint8		algo;		/* CRYPTO_ALGO_AES_CCM, CRYPTO_ALGO_WEP128, etc */
	uint8		rcmta;		/* rcmta entry index, same as idx by default */
	uint16		flags;		/* misc flags */
	uint8 		algo_hw;	/* cache for hw register */
	uint8 		aes_mode;	/* cache for hw register */
	int8		iv_len;		/* IV length */
	int8		icv_len;	/* ICV length */
	uint32		len;		/* key length..don't move this var */
	/* data is 4byte aligned */
	uint8		data[DOT11_MAX_KEY_SIZE];	/* key data */
	wsec_iv_t	rxiv[WLC_NUMRXIVS];		/* Rx IV (one per TID) */
	wsec_iv_t	txiv;		/* Tx IV */
#ifndef LINUX_CRYPTO
	tkip_info_t	tkip_tx;	/* tkip transmit state */
	tkip_info_t	tkip_rx;	/* tkip receive state */
	uint32		tkip_rx_iv32;	/* upper 32 bits of rx IV used to calc phase1 */
	uint8		tkip_rx_ividx;	/* index of rxiv above iv32 belongs to */
	uint8		tkip_tx_lefts;	/* # of not-mic'd bytes */
	uint8		tkip_tx_left[4];	/* not-mic'd bytes */
	uint16		tkip_tx_offset;	/* frag offset in frame */
	uint8		tkip_tx_fmic[8];	/* storage for final TKIP MIC */
	int		tkip_tx_fmic_written;	/* number of bytes of final mic written
						 * to fragment
						 */
#endif

#ifdef BCMWAPI_WPI
	struct wpi_iv 	wapi_txiv;
	struct wpi_iv 	wapi_rxiv;
#ifdef BCMWAPI_WAI
	uint32		wapi_rx;	/* key receive cipher packets counter */
	uint32		wapi_tx;	/* key transmit multicast cipher packets counter */
#endif /* BCMWAPI_WAI */
#endif /* BCMWAPI_WPI */
#ifdef GTK_RESET
	bool		gtk_plumbed;
#endif /* GTK_RESET */
#if defined(BRCMAPIVTW)
	/* IV trace window - allocated if required */
	iv_tw_t *iv_tw;
#endif /* BRCMAPIVTW */
} wsec_key_t;

#ifndef LINUX_POSTMOGRIFY_REMOVAL
typedef struct {
	uint8	vec[ROUNDUP(WSEC_MAX_KEYS, NBBY)/NBBY];	/* bitvec of wsec_key indexes */
} wsec_key_vec_t;


/* For use with wsec_key_t.flags */

#define WSEC_BS_UPDATE		(1 << 0)	/* Indicates hw needs key update on BS switch */
#define WSEC_PRIMARY_KEY	(1 << 1)	/* Indicates this key is the primary (ie tx) key */
#define WSEC_TKIP_ERROR		(1 << 2)	/* Provoke deliberate MIC error */
#define WSEC_REPLAY_ERROR	(1 << 3)	/* Provoke deliberate replay */
#define WSEC_IBSS_PEER_GROUP_KEY	(1 << 6)	/* Flag: group key for a IBSS PEER */
#define WSEC_ICV_ERROR		(1 << 7)	/* Provoke deliberate ICV error */
#define WSEC_MFP_ACT_ERROR	(1 << 8)	/* Provoke deliberate MFP Action Frame error */
#define WSEC_MFP_DISASSOC_ERROR	(1 << 9)	/* Provoke deliberate MFP Disassoc Frame error */
#define WSEC_MFP_DEAUTH_ERROR	(1 << 10)	/* Provoke deliberate MFP Deauth Frame error */

#ifdef WLWSEC
extern int wlc_key_insert(struct wlc_info *wlc, struct wlc_bsscfg *bsscfg, uint32 key_len,
	uint32 key_id, uint32 key_flags, uint32 key_algo, uint8 *key_data,
	struct ether_addr *key_ea, wsec_iv_t *initial_iv, wsec_key_t **key_ptr);
extern void wlc_key_update(struct wlc_info *wlc, int i, struct wlc_bsscfg *bsscfg);
extern void wlc_key_remove(struct wlc_info *wlc, struct wlc_bsscfg *bsscfg, wl_wsec_key_t *remove);
extern void wlc_key_remove_all(struct wlc_info *wlc, struct wlc_bsscfg *bsscfg);
extern void wlc_key_delete(struct wlc_info *wlc, struct wlc_bsscfg *bsscfg, wsec_key_t *key);
extern void wlc_key_scb_delete(struct wlc_info *wlc, struct scb *scb);
extern wsec_key_t* wlc_key_lookup(struct wlc_info *wlc, struct scb *scb, struct wlc_bsscfg *bsscfg,
	uint indx, bool ismulti);

extern void wlc_key_hw_init_all(struct wlc_info *wlc);
extern void wlc_key_hw_init(struct wlc_info *wlc, int i, struct wlc_bsscfg *bsscfg);

extern void wlc_key_hw_wowl_init(struct wlc_info *wlc, wsec_key_t *key, int rcmta_index,
                                 struct wlc_bsscfg *bsscfg);
extern void wlc_key_hw_reallocate(struct wlc_info *wlc, int hw_index, struct wlc_bsscfg *bsscfg);
extern void wlc_key_sw_wowl_update(struct wlc_info *wlc, wsec_key_t *key, int rcmta_index,
                                   struct wlc_bsscfg *bsscfg, bool keyrot, bool restore_tsc_pn);
extern int wlc_key_sw_wowl_create(struct wlc_info *wlc, int rcmta_index, struct wlc_bsscfg *bsscfg);
extern void wlc_key_iv_update(struct wlc_info *wlc, struct wlc_bsscfg *bsscfg, wsec_key_t *key,
	uchar *buf, bool update);
extern void wlc_key_iv_init(struct wlc_info *wlc, struct wlc_bsscfg *cfg,
	wsec_key_t *key, wsec_iv_t *initial_iv);

#ifdef BCMDBG
/* Set the key flag that provokes a deliberate error. */
extern int wlc_key_set_error(struct wlc_info *wlc, struct wlc_bsscfg *cfg, int kflag,
                             wl_wsec_key_t *key_param);
#endif /* BCMDBG */

#if defined(BCMDBG)
extern int wlc_key_dump_hw(struct wlc_info *wlc, struct bcmstrbuf *b);
extern int wlc_key_dump_sw(struct wlc_info *wlc, struct bcmstrbuf *b);
#endif 

#if defined(BCMDBG)
extern const char *wlc_key_algo_name(wsec_key_t *key);
#endif

extern uint16 wlc_key_defkeyflag(struct wlc_info *wlc);

extern int wlc_rcmta_add_bssid(struct wlc_info *wlc, struct wlc_bsscfg *cfg);
extern void wlc_rcmta_del_bssid(struct wlc_info *wlc, struct wlc_bsscfg *cfg);

#else /* WLWSEC */
	#define wlc_key_insert(a, b, c, d, e, f, g, h, i, j) (BCME_ERROR)
	#define wlc_key_update(a, b, c) do {} while (0)
	#define wlc_key_remove(a, b, c) do {} while (0)
	#define wlc_key_remove_all(a, b) do {} while (0)
	#define wlc_key_delete(a, b, c) do {} while (0)
	#define wlc_scb_key_delete(a, b) do {} while (0)
	#define wlc_key_lookup(a, b, c, d, e) (NULL)
	#define wlc_key_hw_init_all(a) do {} while (0)
	#define wlc_key_hw_init(a, b, c)  do {} while (0)
	#define wlc_key_hw_wowl_init(a, b, c, d) do {} while (0)
	#define wlc_key_hw_reallocate(a, b, c) do {} while (0)
	#define wlc_key_sw_wowl_update(a, b, c, d, e, f) do {} while (0)
	#define wlc_key_sw_wowl_create(a, b, c) (BCME_ERROR)
	#define wlc_key_iv_update(a, b, c, d, e) do {(void)e;} while (0)
	#define wlc_key_iv_init(a, b, c) do {} while (0)
	#define wlc_key_set_error(a, b, c) (BCME_ERROR)
	#define wlc_key_dump_hw(a, b) (BCME_ERROR)
	#define wlc_key_dump_sw(a, b) (BCME_ERROR)
	#define wlc_key_defkeyflag(a) (0)
	#define wlc_rcmta_add_bssid(a, b) do {} while (0)
	#define wlc_rcmta_del_bssid(a, b) do {} while (0)
	#define wlc_key_scb_delete(a, b) do {} while (0)

#endif /* WLWSEC */
#endif /* LINUX_POSTMOGRIFY_REMOVAL */
#endif /* _wlc_key_h_ */
