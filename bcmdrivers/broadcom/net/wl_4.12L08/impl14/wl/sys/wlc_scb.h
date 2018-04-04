/*
 * Common interface to the 802.11 Station Control Block (scb) structure
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_scb.h 380701 2013-01-23 16:36:13Z $
 */

#ifndef _wlc_scb_h_
#define _wlc_scb_h_

#include <proto/802.1d.h>

#ifdef PSTA
/* In Proxy STA mode we support up to max 50 hosts and repeater itself */
#define SCB_BSSCFG_BITSIZE ROUNDUP(51, NBBY)/NBBY
#else /* PSTA */
#define SCB_BSSCFG_BITSIZE ROUNDUP(32, NBBY)/NBBY
#if (WLC_MAXBSSCFG > 32)
#error "auth_bsscfg cannot handle WLC_MAXBSSCFG"
#endif
#endif /* PSTA */

/* Information node for scb packet transmit path */
struct tx_path_node {
	txmod_tx_fn_t next_tx_fn;		/* Next function to be executed */
	void *next_handle;
	uint8 next_fid;			/* Next fid in transmit path */
	bool configured;		/* Whether this feature is configured */
};

#ifdef WLCNTSCB
typedef struct wlc_scb_stats {
	uint32 tx_pkts;			/* # of packets transmitted */
	uint32 tx_failures;		/* # of packets failed */
	uint32 rx_ucast_pkts;		/* # of unicast packets received */
	uint32 rx_mcast_pkts;		/* # of multicast packets received */
	ratespec_t tx_rate;		/* Rate of last successful tx frame */
	ratespec_t rx_rate;		/* Rate of last successful rx frame */
	uint32 rx_decrypt_succeeds;	/* # of packet decrypted successfully */
	uint32 rx_decrypt_failures;	/* # of packet decrypted unsuccessfully */
#if defined(DSLCPE) && defined(DSLCPE_FAST_TIMEOUT)
	uint32 tx_failures_burst;	/* # of accumulated failed packets since last succeeded packet in a burst */
#endif
} wlc_scb_stats_t;
#endif /* WLCNTSCB */

typedef struct wlc_rate_histo {
	uint		vitxrspecidx;	/* Index into the video TX rate array */
	ratespec_t	vitxrspec[NVITXRATE][2];	/* History of Video MPDU's txrate */
	uint32		vitxrspectime[NVITXRATE][2];	/* Timestamp for each Video Tx */
	uint32		txrspectime[NTXRATE][2];	/* Timestamp for each Tx */
	uint		txrspecidx; /* Index into the TX rate array */
	ratespec_t	txrspec[NTXRATE][2];	/* History of MPDU's txrate */
	uint		rxrspecidx; /* Index into the Rx rate array */
	ratespec_t	rxrspec[NTXRATE];	/* History of MPDU's rxrate */
} wlc_rate_histo_t;


#define WLMEDIA_DCS_RELOCATION_NONE	0x0
#define WLMEDIA_DCS_RELOCATION_PENDING	0x1
#define WLMEDIA_DCS_RELOCATION_SUCCESS	0x2

/* station control block - one per remote MAC address */
struct scb {
	void *scb_priv;		/* internal scb data structure */
#ifdef MACOSX
	uint32 magic;
#endif
	uint32	flags;		/* various bit flags as defined below */
	uint32	flags2;		/* various bit flags2 as defined below */
	wsec_key_t	*key;		/* per station WEP key */
	wlc_bsscfg_t	*bsscfg;	/* bsscfg to which this scb belongs */
	struct ether_addr ea;		/* station address, must be aligned */
	uint8   auth_bsscfg[SCB_BSSCFG_BITSIZE]; /* authentication state w/ respect to bsscfg(s) */
	uint8	state; /* current state bitfield of auth/assoc process */
	bool		permanent;	/* scb should not be reclaimed */
	uint		used;		/* time of last use */
	uint32		assoctime;	/* time of association */
	uint		bandunit;	/* tha band it belongs to */
#if defined(IBSS_PEER_GROUP_KEY)
	wsec_key_t	*ibss_grp_keys[WSEC_MAX_DEFAULT_KEYS];	/* Group Keys for IBSS peer */
#endif

	uint16	 WPA_auth;	/* WPA: authenticated key management */
	uint32	 wsec;	/* ucast security algo. should match key->algo. Needed before key is set */

	wlc_rateset_t	rateset;	/* operational rates for this remote station */

	void	*fragbuf[NUMPRIO];	/* defragmentation buffer per prio */
	uint	fragresid[NUMPRIO];	/* #bytes unused in frag buffer per prio */

	uint16	 seqctl[NUMPRIO];	/* seqctl of last received frame (for dups) */
	uint16	 seqctl_nonqos;		/* seqctl of last received frame (for dups) for
					 * non-QoS data and management
					 */
	uint16	 seqnum[NUMPRIO];	/* WME: driver maintained sw seqnum per priority */

	/* APSD configuration */
	struct {
		uint16		maxsplen;   /* Maximum Service Period Length from assoc req */
		ac_bitmap_t	ac_defl;    /* Bitmap of ACs enabled for APSD from assoc req */
		ac_bitmap_t	ac_trig;    /* Bitmap of ACs currently trigger-enabled */
		ac_bitmap_t	ac_delv;    /* Bitmap of ACs currently delivery-enabled */
	} apsd;

#ifdef AP
	uint16		aid;		/* association ID */
	uint8		*challenge;	/* pointer to shared key challenge info element */
	uint16		tbtt;		/* count of tbtt intervals since last ageing event */
	uint8		auth_alg;	/* 802.11 authentication mode */
	bool		PS;		/* remote STA in PS mode */
	bool            PS_pend;        /* Pending PS state */
	uint8           PS_pretend;     /* AP pretending STA is in PS mode */
	uint		grace_attempts;	/* Additional attempts made beyond scb_timeout
					 * before scb is removed
					 */
#endif /* AP */
	uint8		*wpaie;		/* WPA IE */
	uint		wpaie_len;	/* Length of wpaie */
	wlc_if_t	*wds;		/* per-port WDS cookie */
	int		*rssi_window;	/* rssi samples */
	int		rssi_index;
	int		rssi_enabled;	/* enable rssi collection */
	uint16		cap;		/* sta's advertized capability field */
	uint16		listen;		/* minimum # bcn's to buffer PS traffic */

	uint16		amsdu_ht_mtu_pref;	/* preferred HT AMSDU mtu in bytes */
	uint16		amsdu_vht_mtu_pref;	/* preferred VHT AMSDU mtu in bytes */

#ifdef WL11N
	bool		ht_mimops_enabled;	/* cached state: a mimo ps mode is enabled */
	bool		ht_mimops_rtsmode;	/* cached state: TRUE=RTS mimo, FALSE=no mimo */
	uint16		ht_capabilities;	/* current advertised capability set */
	uint8		ht_ampdu_params;	/* current adverised AMPDU config */
#if defined(BCMDBG) || defined(DONGLEBUILD)
	uint8		rclen;			/* regulatory class length */
	uint8		rclist[MAXRCLISTSIZE];	/* regulatory class list */
#endif 
#endif /* WL11N */

#ifdef WL11AC
	uint16		vht_flags;		/* converted vht flags */
	uint16		vht_rxmcsmap;	/* raw vht rxmcsmap */
	uint8		vht_bwcap;
	uint8		stbc_num;
	uint8		oper_mode;		/* VHT operational mode */
	bool		oper_mode_enabled;		/* VHT operational mode is enabled */
#endif /* WL11AC */
	struct tx_path_node	*tx_path; /* Function chain for tx path for a pkt */
	uint32		fragtimestamp[NUMPRIO];
#ifdef WLCNTSCB
	wlc_scb_stats_t scb_stats;
#endif /* WLCNTSCB */
	bool		stale_remove;
#ifdef PROP_TXSTATUS
	uint8		mac_address_handle;
#endif
#ifdef WLPKTDLYSTAT
#ifdef WLPKTDLYSTAT_IND
	txdelay_params_t txdelay_params;
#endif /* WLPKTDLYSTAT_IND */
	scb_delay_stats_t	delay_stats[AC_COUNT];	/* per-AC delay stats */
#endif /* WLPKTDLYSTAT */
	bool		rssi_upd;		/* per scb rssi is enabled by ... */
#if defined(STA) && defined(DBG_BCN_LOSS)
	struct wlc_scb_dbg_bcn dbg_bcn;
#endif
	uint32	flags3;		/* various bit flags2 as defined below */
#if defined(AP) && defined(WLWNM)
	uint32	rx_tstamp;		/* time of last frame received */
	uint8	tim_bcast_status;	/* TIM Broadcast status */
	uint8	tim_bcast_interval;	/* TIM Broadcast interval */
	ratespec_t	tim_bcast_high_rate;	/* high rate cached for fast referencing */
	uint8	*tfs_list;		/* tfs-request list constructed by TFS request */
#endif
	struct	scb *psta_prim;	/* pointer to primary proxy sta */
#ifdef PROP_TXSTATUS
	uint16 		first_sup_pkt;
#endif
	uint8	dcs_relocation_state; /* unicast CSA state */

#ifdef WL11AC
	uint8	vht_ratemask;
#endif

#if defined(PKTC) || defined(PKTC_DONGLE)
	uint32	pktc_pps;		/* pps counter for activating pktc */
#endif
#ifdef AP
	uint32 ps_pretend_start;
	uint32 ps_pretend_probe;
	uint32 ps_pretend_count;
	uint8  ps_pretend_succ_count;
	uint8  ps_pretend_failed_ack_count;
	uint32 ps_pretend_total_time_in_pps;
#endif /* AP */
};

#ifdef AP
/* bit flags for (uint8) scb.PS_pretend */
#define PS_PRETEND_NOT_ACTIVE    0

/* PS_PRETEND_PROBING states to do probing to the scb */
#define PS_PRETEND_PROBING       (1 << 0)

/* PS_PRETEND_DO_FIFO_FLUSH states to do a tx fifo flush */
#define PS_PRETEND_DO_FIFO_FLUSH (1 << 1)

/* PS_PRETEND_ACTIVE indicates that ps pretend is current active */
#define	PS_PRETEND_ACTIVE        (1 << 2)

/* PS_PRETEND_ACTIVE_PMQ indicates that we have had a PPS PMQ entry */
#define	PS_PRETEND_ACTIVE_PMQ    (1 << 3)

/* PS_PRETEND_NO_BLOCK states that we should not expect to see a PPS
 * PMQ entry, hence, not to block ourselves waiting to get one
 */
#define PS_PRETEND_NO_BLOCK      (1 << 4)

/* PS_PRETEND_PREVENT states to not do normal ps pretend for a scb */
#define PS_PRETEND_PREVENT       (1 << 5)

/* PS_PRETEND_THRESHOLD indicates that the successive failed TX status
 * count has exceeded the threshold
 */
#define PS_PRETEND_THRESHOLD     (1 << 7)

/* PS_PRETEND_ON is a bit mask of all active states that is used
 * to clear the scb state when ps pretend exits
 */
#define PS_PRETEND_ON            (PS_PRETEND_ACTIVE | PS_PRETEND_PROBING | \
							PS_PRETEND_DO_FIFO_FLUSH | \
							PS_PRETEND_THRESHOLD | \
							PS_PRETEND_ACTIVE_PMQ)
#endif /* AP */

typedef enum {
	RSSI_UPDATE_FOR_WLC = 0,	       /* Driver level */
	RSSI_UPDATE_FOR_TM	       /* Traffic Management */
} scb_rssi_requestor_t;

extern bool wlc_scb_rssi_update_enable(struct scb *scb, bool enable, scb_rssi_requestor_t);

/* Test whether RSSI update is enabled. Made a macro to reduce fn call overhead. */
#define WLC_SCB_RSSI_UPDATE_ENABLED(scb) (scb->rssi_upd != 0)

/* Iterator for scb list */
struct scb_iter {
	struct scb	*next;			/* next scb in bss */
	wlc_bsscfg_t	*next_bss;		/* next bss pointer */
	bool		all;			/* walk all bss or not */
};

#define SCB_BSSCFG(a)           ((a)->bsscfg)

/* Initialize an scb iterator pre-fetching the next scb as it moves along the list */
void wlc_scb_iterinit(scb_module_t *scbstate, struct scb_iter *scbiter,
	wlc_bsscfg_t *bsscfg);
/* move the iterator */
struct scb *wlc_scb_iternext(scb_module_t *scbstate, struct scb_iter *scbiter);

/* Iterate thru' scbs of specified bss */
#define FOREACH_BSS_SCB(scbstate, scbiter, bss, scb) \
	for (wlc_scb_iterinit((scbstate), (scbiter), (bss)); \
	     ((scb) = wlc_scb_iternext((scbstate), (scbiter))) != NULL; )

/* Iterate thru' scbs of all bss. Use this only when needed. For most of
 * the cases above one should suffice.
 */
#define FOREACHSCB(scbstate, scbiter, scb) \
	for (wlc_scb_iterinit((scbstate), (scbiter), NULL); \
	     ((scb) = wlc_scb_iternext((scbstate), (scbiter))) != NULL; )

scb_module_t *wlc_scb_attach(wlc_info_t *wlc);
void wlc_scb_detach(scb_module_t *scbstate);

/* scb cubby cb functions */
typedef int (*scb_cubby_init_t)(void *, struct scb *);
typedef void (*scb_cubby_deinit_t)(void *, struct scb *);
typedef void (*scb_cubby_dump_t)(void *, struct scb *, struct bcmstrbuf *b);

/* This function allocates an opaque cubby of the requested size in the scb container.
 * The cb functions fn_init/fn_deinit are called when a scb is allocated/freed.
 * The functions are called with the context passed in and a scb pointer.
 * It returns a handle that can be used in macro SCB_CUBBY to retrieve the cubby.
 * Function returns a negative number on failure
 */
int wlc_scb_cubby_reserve(wlc_info_t *wlc, uint size, scb_cubby_init_t fn_init,
	scb_cubby_deinit_t fn_deinit, scb_cubby_dump_t fn_dump, void *context);

/* macro to retrieve pointer to module specific opaque data in scb container */
#define SCB_CUBBY(scb, handle)	(void *)(((uint8 *)(scb)) + handle)

/*
 * Accessors
 */

struct wlcband * wlc_scbband(struct scb *scb);

/* Find station control block corresponding to the remote id */
struct scb *wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea);

/* Lookup station control for ID. If not found, create a new entry. */
struct scb *wlc_scblookup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea);

/* Lookup station control for ID. If not found, create a new entry. */
struct scb *wlc_scblookupband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
                              const struct ether_addr *ea, int bandunit);

/* Get scb from band */
struct scb *wlc_scbfindband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
                            const struct ether_addr *ea, int bandunit);

/* Determine if any SCB associated to ap cfg */
bool wlc_scb_associated_to_ap(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

/* Move the scb's band info */
void wlc_scb_update_band_for_cfg(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, chanspec_t chanspec);

extern struct scb *wlc_scbibssfindband(wlc_info_t *wlc, const struct ether_addr *ea,
	int bandunit, wlc_bsscfg_t **bsscfg);

extern struct scb *wlc_scbbssfindband(wlc_info_t *wlc, const struct ether_addr *hwaddr,
	const struct ether_addr *ea, int bandunit, wlc_bsscfg_t **bsscfg);

struct scb *wlc_internalscb_alloc(wlc_info_t *wlc, const struct ether_addr *ea,
	struct wlcband *band);
void wlc_internalscb_free(wlc_info_t *wlc, struct scb *scb);

bool wlc_scbfree(wlc_info_t *wlc, struct scb *remove);

/* * "|" operation */
void wlc_scb_setstatebit(struct scb *scb, uint8 state);

/* * "& ~" operation . */
void wlc_scb_clearstatebit(struct scb *scb, uint8 state);

/* * "|" operation . idx = position of the bsscfg in the wlc array of multi ssids. */

void wlc_scb_setstatebit_bsscfg(struct scb *scb, uint8 state, int idx);

/* * "& ~" operation . idx = position of the bsscfg in the wlc array of multi ssids. */
void wlc_scb_clearstatebit_bsscfg(struct scb *scb, uint8 state, int idx);

/* * reset all state. the multi ssid array is cleared as well. */
void wlc_scb_resetstate(struct scb *scb);

void wlc_scb_reinit(wlc_info_t *wlc);

/* free all scbs, unless permanent. Force indicates reclaim permanent as well */
void wlc_scbclear(struct wlc_info *wlc, bool force);
/* free all scbs of a bsscfg */
void wlc_scb_bsscfg_scbclear(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, bool perm);

/* (de)authorize/(de)authenticate single station */
void wlc_scb_set_auth(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb, bool enable,
                      uint32 flag, int rc);

/* sort rates for a single scb */
void wlc_scb_sortrates(wlc_info_t *wlc, struct scb *scb);

/* sort rates for all scb in wlc */
void BCMINITFN(wlc_scblist_validaterates)(wlc_info_t *wlc);

#ifdef PROP_TXSTATUS
extern void wlc_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 mac_handle, uint8 ta_bmp);
#endif

#ifdef AP
void wlc_scb_wds_free(struct wlc_info *wlc);
#else
#define wlc_scb_wds_free(a) do {} while (0)
#endif /* AP */

extern void wlc_scb_set_bsscfg(struct scb *scb, wlc_bsscfg_t *cfg);

/* average rssi over window */

int wlc_scb_rssi(struct scb *scb);
void wlc_scb_rssi_init(struct scb *scb, int rssi);


/* SCB flags */
#define SCB_NONERP		0x0001		/* No ERP */
#define SCB_LONGSLOT		0x0002		/* Long Slot */
#define SCB_SHORTPREAMBLE	0x0004		/* Short Preamble ok */
#define SCB_8021XHDR		0x0008		/* 802.1x Header */
#define SCB_WPA_SUP		0x0010		/* 0 - authenticator, 1 - supplicant */
#define SCB_DEAUTH		0x0020		/* 0 - ok to deauth, 1 - no (just did) */
#define SCB_WMECAP		0x0040		/* WME Cap; may ONLY be set if WME_ENAB(wlc) */
#define SCB_USME2		0x0080
#define SCB_BRCM		0x0100		/* BRCM AP or STA */
#define SCB_WDS_LINKUP		0x0200		/* WDS link up */
#define SCB_LEGACY_AES		0x0400		/* legacy AES device */
#define SCB_USME1		0x0800
#define SCB_MYAP		0x1000		/* We are associated to this AP */
#define SCB_PENDING_PROBE	0x2000		/* Probe is pending to this SCB */
#define SCB_AMSDUCAP		0x4000		/* A-MSDU capable */
#define SCB_USEME		0x8000
#define SCB_HTCAP		0x10000		/* HT (MIMO) capable device */
#define SCB_RECV_PM		0x20000		/* state of PM bit in last data frame recv'd */
#define SCB_AMPDUCAP		0x40000		/* A-MPDU capable */
#define SCB_IS40		0x80000		/* 40MHz capable */
#define SCB_NONGF		0x100000	/* Not Green Field capable */
#define SCB_APSDCAP		0x200000	/* APSD capable */
#define SCB_PENDING_FREE	0x400000	/* marked for deletion - clip recursion */
#define SCB_PENDING_PSPOLL	0x800000	/* PS-Poll is pending to this SCB */
#define SCB_RIFSCAP		0x1000000	/* RIFS capable */
#define SCB_HT40INTOLERANT	0x2000000	/* 40 Intolerant */
#define SCB_WMEPS		0x4000000	/* PS + WME w/o APSD capable */
#define SCB_SENT_APSD_TRIG	0x8000000	/* APSD Trigger Null Frame was recently sent */
#define SCB_COEX_MGMT		0x10000000	/* Coexistence Management supported */
#define SCB_IBSS_PEER		0x20000000	/* Station is an IBSS peer */
#define SCB_STBCCAP		0x40000000	/* STBC Capable */

/* scb flags2 */
#define SCB2_SGI20_CAP          0x00000001      /* 20MHz SGI Capable */
#define SCB2_SGI40_CAP          0x00000002      /* 40MHz SGI Capable */
#define SCB2_RX_LARGE_AGG       0x00000004      /* device can rx large aggs */
#define SCB2_INTERNAL           0x00000008      /* This scb is an internal scb */
#define SCB2_IN_ASSOC           0x00000010      /* Incoming assocation in progress */
#ifdef BCMWAPI_WAI
#define SCB2_WAIHDR             0x00000020      /* WAI Header */
#endif
#define SCB2_P2P                0x00000040      /* WiFi P2P */
#define SCB2_LDPCCAP            0x00000080      /* LDPC Cap */
#define SCB2_BCMDCS             0x00000100      /* BCM_DCS */
#define SCB2_MFP                0x00000200      /* 802.11w MFP_ENABLE */
#define SCB2_SHA256             0x00000400      /* sha256 for AKM */
#define SCB2_VHTCAP             0x00000800      /* VHT (11ac) capable device */


#ifdef PROP_TXSTATUS
#define SCB2_PROPTXTSTATUS_SUPPR_STATEMASK      0x00001000
#define SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT     12
#define SCB2_PROPTXTSTATUS_SUPPR_GENMASK        0x00002000
#define SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT       13
#define SCB2_PROPTXTSTATUS_PKTWAITING_MASK      0x00004000
#define SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT     14
#define SCB2_PROPTXTSTATUS_POLLRETRY_MASK       0x00008000
#define SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT      15
/* 4 bits for AC[0-3] traffic pending status from the host */
#define SCB2_PROPTXTSTATUS_TIM_SHIFT            16
#define SCB2_PROPTXTSTATUS_TIM_MASK             (0xf << SCB2_PROPTXTSTATUS_TIM_SHIFT)
#endif
#define SCB2_TDLS_PROHIBIT      0x00100000      /* TDLS prohibited */
#define SCB2_TDLS_CHSW_PROHIBIT 0x00200000      /* TDLS channel switch prohibited */
#define SCB2_TDLS_SUPPORT       0x00400000
#define SCB2_TDLS_PU_BUFFER_STA 0x00800000
#define SCB2_TDLS_PEER_PSM      0x01000000
#define SCB2_TDLS_CHSW_SUPPORT  0x02000000
#define SCB2_TDLS_PU_SLEEP_STA  0x04000000
#define SCB2_TDLS_MASK          0x07f00000
#define SCB2_IGN_SMPS			0x08000000 	/* ignore SM PS update */
#define SCB2_IS80           	0x10000000      /* 80MHz capable */
#define SCB2_AMSDU_IN_AMPDU_CAP	0x20000000      /* AMSDU over AMPDU */
#define SCB2_CCX_MFP			0x40000000	/* CCX MFP enable */
#define SCB2_DWDS		0x80000000      /* DWDS capable */

/* scb flags3 */
#define SCB3_A4_DATA		0x00000001      /* scb does 4 addr data frames */
#define SCB3_A4_NULLDATA	0x00000002	/* scb does 4-addr null data frames */
#define SCB3_A4_8021X		0x00000004	/* scb does 4-addr 8021x frames */

#ifdef WLWNM
#define SCB3_WNM_PROXY_ARP	0x00000008      /* WNM proxy ARP service */
#define SCB3_WNM_TFS		0x00000010      /* WNM TFS */
#define SCB3_WNM_SLEEP_MODE	0x00000020      /* WNM WNM-Sleep mode */
#define SCB3_WNM_TIM_BCAST	0x00000040      /* WNM TIM broadcast */
#define SCB3_WNM_BSS_TRANS	0x00000080      /* WNM BSS transition */
#define SCB3_WNM_DMS		0x00000100      /* WNM DMS */
#define SCB3_WNM_NOTIFICATION	0x00000200      /* WNM WNM-Notification */
#define SCB3_WNM_FMS		0x00000400      /* WNM FMS */
#define SCB3_WNM_MASK		0x000007f8

#define SCB_PROXYARP(a)		((a) && ((a)->flags3 & SCB3_WNM_PROXY_ARP))
#define SCB_TFS(a)		((a) && ((a)->flags3 & SCB3_WNM_TFS))
#define SCB_WNM_SLEEP(a)	((a) && ((a)->flags3 & SCB3_WNM_SLEEP_MODE))
#define SCB_TIM_BCAST(a)	((a) && ((a)->flags3 & SCB3_WNM_TIM_BCAST))
#define SCB_BSS_TRANS(a)	((a) && ((a)->flags3 & SCB3_WNM_BSS_TRANS))
#define SCB_DMS(a)		((a) && ((a)->flags3 & SCB3_WNM_DMS))
#define SCB_FMS(a)		((a) && ((a)->flags3 & SCB3_WNM_FMS))
#endif /* WLWNM */

#ifdef WL_RELMCAST
#define SCB3_RELMCAST		0x00000800		/* Reliable Multicast */
#define SCB3_RELMCAST_NOACK	0x00001000		/* Reliable Multicast No ACK rxed */
#endif

#define SCB3_PKTC		0x00002000      /* Enable packet chaining */
#define SCB3_OPER_MODE_NOTIF	0x00004000      /* 11ac Oper Mode Notif'n */


#ifdef PROP_TXSTATUS
#define SCB_PROPTXTSTATUS_SUPPR_STATE(s)	(((s)->flags2 & \
	SCB2_PROPTXTSTATUS_SUPPR_STATEMASK) >> SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT)
#define SCB_PROPTXTSTATUS_SUPPR_GEN(s)		(((s)->flags2 & SCB2_PROPTXTSTATUS_SUPPR_GENMASK) \
	>> SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT)
#define SCB_PROPTXTSTATUS_TIM(s)		(((s)->flags2 & \
	SCB2_PROPTXTSTATUS_TIM_MASK) >> SCB2_PROPTXTSTATUS_TIM_SHIFT)
#define SCB_PROPTXTSTATUS_PKTWAITING(s)		(((s)->flags2 & \
	SCB2_PROPTXTSTATUS_PKTWAITING_MASK) >> SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT)
#define SCB_PROPTXTSTATUS_POLLRETRY(s)		(((s)->flags2 & \
	SCB2_PROPTXTSTATUS_POLLRETRY_MASK) >> SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT)

#define SCB_PROPTXTSTATUS_SUPPR_SETSTATE(s, state)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_SUPPR_STATEMASK) | \
		(((state) << SCB2_PROPTXTSTATUS_SUPPR_STATESHIFT) & \
		SCB2_PROPTXTSTATUS_SUPPR_STATEMASK)
#define SCB_PROPTXTSTATUS_SUPPR_SETGEN(s, gen)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_SUPPR_GENMASK) | \
		(((gen) << SCB2_PROPTXTSTATUS_SUPPR_GENSHIFT) & SCB2_PROPTXTSTATUS_SUPPR_GENMASK)
#define SCB_PROPTXTSTATUS_SETPKTWAITING(s, waiting)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_PKTWAITING_MASK) | \
		(((waiting) << SCB2_PROPTXTSTATUS_PKTWAITING_SHIFT) & \
		SCB2_PROPTXTSTATUS_PKTWAITING_MASK)
#define SCB_PROPTXTSTATUS_SETPOLLRETRY(s, retry)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_POLLRETRY_MASK) | \
		(((retry) << SCB2_PROPTXTSTATUS_POLLRETRY_SHIFT) & \
		SCB2_PROPTXTSTATUS_POLLRETRY_MASK)
#define SCB_PROPTXTSTATUS_SETTIM(s, tim)	(s)->flags2 = ((s)->flags2 & \
		~SCB2_PROPTXTSTATUS_TIM_MASK) | \
		(((tim) << SCB2_PROPTXTSTATUS_TIM_SHIFT) & SCB2_PROPTXTSTATUS_TIM_MASK)
#endif /* PROP_TXSTATUS */

/* scb vht flags */
#define SCB_VHT_LDPCCAP		0x0001
#define SCB_SGI80       0x0002
#define SCB_SGI160		0x0004
#define SCB_VHT_TX_STBCCAP	0x0008
#define SCB_VHT_RX_STBCCAP	0x0010
#define SCB_SU_BEAMFORMER	0x0020
#define SCB_SU_BEAMFORMEE	0x0040
#define SCB_MU_BEAMFORMER	0x0080
#define SCB_MU_BEAMFORMEE	0x0100
#define SCB_VHT_TXOP_PS		0x0200
#define SCB_HTC_VHT_CAP		0x0400

/* scb association state bitfield */
#define UNAUTHENTICATED		0	/* unknown */
#define AUTHENTICATED		1	/* 802.11 authenticated (open or shared key) */
#define ASSOCIATED		2	/* 802.11 associated */
#define PENDING_AUTH		4	/* Waiting for 802.11 authentication response */
#define PENDING_ASSOC		8	/* Waiting for 802.11 association response */
#define AUTHORIZED		0x10	/* 802.1X authorized */
#define TAKEN4IBSS		0x80	/* Taken */

/* scb association state helpers */
#define SCB_ASSOCIATED(a)	((a)->state & ASSOCIATED)
#define SCB_AUTHENTICATED(a)	((a)->state & AUTHENTICATED)
#define SCB_AUTHORIZED(a)	((a)->state & AUTHORIZED)

/* flag access */
#define SCB_ISMYAP(a)           ((a)->flags & SCB_MYAP)
#define SCB_ISPERMANENT(a)      ((a)->permanent)
#define	SCB_INTERNAL(a) 	((a)->flags2 & SCB2_INTERNAL)
/* scb association state helpers w/ respect to ssid (in case of multi ssids)
 * The bit set in the bit field is relative to the current state (i.e. if
 * the current state is "associated", a 1 at the position "i" means the
 * sta is associated to ssid "i"
 */
#define SCB_ASSOCIATED_BSSCFG(a, i)	\
	(((a)->state & ASSOCIATED) && isset((scb->auth_bsscfg), i))

#define SCB_AUTHENTICATED_BSSCFG(a, i)	\
	(((a)->state & AUTHENTICATED) && isset((scb->auth_bsscfg), i))

#define SCB_AUTHORIZED_BSSCFG(a, i)	\
	(((a)->state & AUTHORIZED) && isset((scb->auth_bsscfg), i))

#define SCB_LONG_TIMEOUT	3600	/* # seconds of idle time after which we proactively
					 * free an authenticated SCB
					 */
#define SCB_SHORT_TIMEOUT	  60	/* # seconds of idle time after which we will reclaim an
					 * authenticated SCB if we would otherwise fail
					 * an SCB allocation.
					 */
#ifdef WLMEDIA_LG
#define SCB_TIMEOUT		  10	/* # seconds: interval to probe idle STAs */
#else
#define SCB_TIMEOUT		  60	/* # seconds: interval to probe idle STAs */
#endif
#define SCB_ACTIVITY_TIME	   5	/* # seconds: skip probe if activity during this time */
#define SCB_GRACE_ATTEMPTS	   3	/* # attempts to probe sta beyond scb_activity_time */

/* scb_info macros */
#ifdef AP
#define SCB_PS(a)                    ((a) && (a)->PS)

#define	SCB_PS_PRETEND(a)            ((a) && ((a)->PS_pretend & PS_PRETEND_ACTIVE))
#define SCB_PS_PRETEND_NORMALPS(a)   (SCB_PS(a) && !SCB_PS_PRETEND(a))

#define	SCB_PS_PRETEND_BLOCKED(a)    \
						(SCB_PS_PRETEND(a) && \
						!(((a)->PS_pretend & PS_PRETEND_ACTIVE_PMQ) || \
						((a)->PS_pretend & PS_PRETEND_NO_BLOCK)))

#define SCB_PS_PRETEND_THRESHOLD(a)  ((a) && ((a)->PS_pretend & PS_PRETEND_THRESHOLD))

#define SCB_PS_PRETEND_FLUSH(a)      \
						(SCB_PS_PRETEND(a) && \
						((a)->PS_pretend & \
						PS_PRETEND_DO_FIFO_FLUSH))

#define	SCB_PS_PRETEND_PROBING(a)	 \
						(SCB_PS_PRETEND(a) && \
						((a)->PS_pretend & \
						PS_PRETEND_PROBING))

#define SCB_PS_PRETEND_ENABLED(w, a)  \
						(PS_PRETEND_ENABLED(w) && \
						!((a)->PS_pretend & PS_PRETEND_PREVENT) && \
						!SCB_ISMULTI(scb))

#define SCB_PS_PRETEND_THRESHOLD_ENABLED(w, a)  \
						(PS_PRETEND_THRESHOLD_ENABLED(w) && \
						!((a)->PS_pretend & PS_PRETEND_PREVENT) && \
						!SCB_ISMULTI(scb))

#ifdef WDS
#define SCB_WDS(a)		((a)->wds)
#else
#define SCB_WDS(a)		NULL
#endif
#define SCB_INTERFACE(a)        ((a)->wds ? (a)->wds->wlif : (a)->bsscfg->wlcif->wlif)
#define SCB_WLCIFP(a)           ((a)->wds ? (a)->wds : ((a)->bsscfg->wlcif))
#define WLC_BCMC_PSMODE(wlc, bsscfg) (SCB_PS(WLC_BCMCSCB_GET(wlc, bsscfg)))
#else
#define SCB_PS(a)		NULL
#define SCB_WDS(a)		NULL
#define SCB_INTERFACE(a)        ((a)->bsscfg->wlcif->wlif)
#define SCB_WLCIFP(a)           (((a)->bsscfg->wlcif))
#define WLC_BCMC_PSMODE(wlc, bsscfg) (TRUE)
#endif /* AP */

#ifdef WME
#define SCB_WME(a)		((a)->flags & SCB_WMECAP)	/* Also implies WME_ENAB(wlc) */
#else
#define SCB_WME(a)		FALSE
#endif

#ifdef WLAMPDU
#define SCB_AMPDU(a)		((a)->flags & SCB_AMPDUCAP)
#else
#define SCB_AMPDU(a)		FALSE
#endif

#ifdef WLAMSDU
#define SCB_AMSDU(a)		((a)->flags & SCB_AMSDUCAP)
#define SCB_AMSDU_IN_AMPDU(a) ((a)->flags2 & SCB2_AMSDU_IN_AMPDU_CAP)
#else
#define SCB_AMSDU(a)		FALSE
#define SCB_AMSDU_IN_AMPDU(a) FALSE
#endif

#ifdef WL11N
#define SCB_HT_CAP(a)		(((a)->flags & SCB_HTCAP) != 0)
#define SCB_VHT_CAP(a)		(((a)->flags2 & SCB2_VHTCAP) != 0)
#define SCB_ISGF_CAP(a)		(((a)->flags & (SCB_HTCAP | SCB_NONGF)) == SCB_HTCAP)
#define SCB_NONGF_CAP(a)	(((a)->flags & (SCB_HTCAP | SCB_NONGF)) == \
					(SCB_HTCAP | SCB_NONGF))
#define SCB_COEX_CAP(a)		((a)->flags & SCB_COEX_MGMT)
#define SCB_STBC_CAP(a)		((a)->flags & SCB_STBCCAP)
#define SCB_LDPC_CAP(a)		(SCB_HT_CAP(a) && ((a)->flags2 & SCB2_LDPCCAP))
#else
#define SCB_HT_CAP(a)		FALSE
#define SCB_VHT_CAP(a)		FALSE
#define SCB_ISGF_CAP(a)		FALSE
#define SCB_NONGF_CAP(a)	FALSE
#define SCB_COEX_CAP(a)		FALSE
#define SCB_STBC_CAP(a)		FALSE
#define SCB_LDPC_CAP(a)		FALSE
#endif /* WL11N */

#ifdef WL11AC
#define SCB_VHT_LDPC_CAP(a)	(SCB_VHT_CAP(a) && ((a)->vht_flags & SCB_VHT_LDPCCAP))
#define SCB_VHT_TX_STBC_CAP(a)	(SCB_VHT_CAP(a) && ((a)->vht_flags & SCB_VHT_TX_STBCCAP))
#define SCB_VHT_RX_STBC_CAP(a)	(SCB_VHT_CAP(a) && ((a)->vht_flags & SCB_VHT_RX_STBCCAP))
#define SCB_VHT_SGI80(a)	(SCB_VHT_CAP(a) && ((a)->vht_flags & SCB_SGI80))
#define SCB_OPER_MODE_NOTIF_CAP(a) ((a)->flags3 & SCB3_OPER_MODE_NOTIF)
#else /* WL11AC */
#define SCB_VHT_LDPC_CAP(a)		FALSE
#define SCB_VHT_TX_STBC_CAP(a)	FALSE
#define SCB_VHT_RX_STBC_CAP(a)	FALSE
#define SCB_VHT_SGI80(a)		FALSE
#endif /* WL11AC */

#define SCB_IS_IBSS_PEER(a)	((a)->flags & SCB_IBSS_PEER)
#define SCB_SET_IBSS_PEER(a)	((a)->flags |= SCB_IBSS_PEER)
#define SCB_UNSET_IBSS_PEER(a)	((a)->flags &= ~SCB_IBSS_PEER)

#if defined(PKTC) || defined(PKTC_DONGLE)
#define SCB_PKTC_ENABLE(a)	((a)->flags3 |= SCB3_PKTC)
#define SCB_PKTC_DISABLE(a)	((a)->flags3 &= ~SCB3_PKTC)
#define SCB_PKTC_ENABLED(a)	((a)->flags3 & SCB3_PKTC)
#else
#define SCB_PKTC_ENABLE(a)
#define SCB_PKTC_DISABLE(a)
#define SCB_PKTC_ENABLED(a)	FALSE
#endif

#define SCB_11E(a)		FALSE

#define SCB_QOS(a)		((a)->flags & (SCB_WMECAP | SCB_HTCAP))

#ifdef WLP2P
#define SCB_P2P(a)		((a)->flags2 & SCB2_P2P)
#else
#define SCB_P2P(a)		FALSE
#endif

#ifdef DWDS
#define SCB_DWDS(a)		((a)->flags2 & SCB2_DWDS)
#else
#define SCB_DWDS(a)		FALSE
#endif

#define SCB_LEGACY_WDS(a)	(SCB_WDS(a) && !SCB_DWDS(a))

#define SCB_A4_DATA(a)		((a)->flags3 & SCB3_A4_DATA)
#define SCB_A4_DATA_ENABLE(a)	((a)->flags3 |= SCB3_A4_DATA)
#define SCB_A4_DATA_DISABLE(a)	((a)->flags3 &= ~SCB3_A4_DATA)

#define SCB_A4_NULLDATA(a)	((a)->flags3 & SCB3_A4_NULLDATA)
#define SCB_A4_8021X(a)		((a)->flags3 & SCB3_A4_8021X)

#define SCB_MFP(a)		((a) && ((a)->flags2 & SCB2_MFP))
#define SCB_SHA256(a)		((a) && ((a)->flags2 & SCB2_SHA256))
#define SCB_CCX_MFP(a)	((a) && ((a)->flags2 & SCB2_CCX_MFP))

#define SCB_SEQNUM(scb, prio)	(scb)->seqnum[(prio)]

#define SCB_ISMULTI(a)	ETHER_ISMULTI((a)->ea.octet)

#ifdef WLCNTSCB
#define WLCNTSCBINCR(a)			((a)++)	/* Increment by 1 */
#define WLCNTSCBDECR(a)			((a)--)	/* Decrement by 1 */
#define WLCNTSCBADD(a,delta)		((a) += (delta)) /* Increment by specified value */
#define WLCNTSCBSET(a,value)		((a) = (value)) /* Set to specific value */
#define WLCNTSCBVAL(a)			(a)	/* Return value */
#define WLCNTSCB_COND_SET(c, a, v)	do { if (c) (a) = (v); } while (0)
#define WLCNTSCB_COND_ADD(c, a, d)	do { if (c) (a) += (d); } while (0)
#define WLCNTSCB_COND_INCR(c, a)	do { if (c) (a) += (1); } while (0)
#else /* WLCNTSCB */
#define WLCNTSCBINCR(a)			/* No stats support */
#define WLCNTSCBDECR(a)			/* No stats support */
#define WLCNTSCBADD(a,delta)		/* No stats support */
#define WLCNTSCBSET(a,value)		/* No stats support */
#define WLCNTSCBVAL(a)		0	/* No stats support */
#define WLCNTSCB_COND_SET(c, a, v)	/* No stats support */
#define WLCNTSCB_COND_ADD(c, a, d) 	/* No stats support */
#define WLCNTSCB_COND_INCR(c, a)	/* No stats support */
#endif /* WLCNTSCB */

/* Given the 'feature', invoke the next stage of transmission in tx path */
#define SCB_TX_NEXT(fid, scb, pkt, prec) \
	(scb->tx_path[(fid)].next_tx_fn((scb->tx_path[(fid)].next_handle), (scb), (pkt), (prec)))

/* Is the feature currently in the path to handle transmit. ACTIVE implies CONFIGURED */
#define SCB_TXMOD_ACTIVE(scb, fid) (scb->tx_path[(fid)].next_tx_fn != NULL)

/* Is the feature configured? */
#define SCB_TXMOD_CONFIGURED(scb, fid) (scb->tx_path[(fid)].configured)

/* Next feature configured */
#define SCB_TXMOD_NEXT_FID(scb, fid) (scb->tx_path[(fid)].next_fid)

extern void wlc_scb_txmod_activate(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid);
extern void wlc_scb_txmod_deactivate(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid);

#ifdef WDS
extern int wlc_wds_create(wlc_info_t *wlc, struct scb *scb, uint flags);
#else
#define wlc_wds_create(a, b, c)	0
#endif
/* flags for wlc_wds_create() */
#define WDS_INFRA_BSS	0x1	/* WDS link is part of the infra mode BSS */
#define WDS_DYNAMIC	0x2	/* WDS link is dynamic */

extern void wlc_scb_switch_band(wlc_info_t *wlc, struct scb *scb, int new_bandunit,
	wlc_bsscfg_t *bsscfg);

#ifdef WL11AC
extern void wlc_scb_update_oper_mode(wlc_info_t *wlc, struct scb *scb,
	uint8 mode);
#endif /* WL11AC */

#endif /* _wlc_scb_h_ */
