/*
 * A-MPDU Tx (with extended Block Ack protocol) source file
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_ampdu.c 380701 2013-01-23 16:36:13Z $
 */

#include <wlc_cfg.h>

#ifndef WLAMPDU
#error "WLAMPDU is not defined"
#endif	/* WLAMPDU */


#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#ifdef PKTC
#include <proto/ethernet.h>
#include <proto/802.3.h>
#endif
#include <wlioctl.h>
#ifdef WLOVERTHRUSTER
#include <proto/ethernet.h>
#endif
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wlc_scb.h>
#include <wlc_frmutil.h>
#ifdef AP
#include <wlc_apps.h>
#endif
#ifdef WLAMPDU
#include <wlc_ampdu.h>
#include <wlc_ampdu_cmn.h>
#endif
#ifdef WLAMSDU
#include <wlc_amsdu.h>
#endif
#include <wlc_scb_ratesel.h>
#include <wl_export.h>
#include <wlc_rm.h>

#ifdef PROP_TXSTATUS
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#endif

#ifdef WLC_HIGH_ONLY
#include <bcm_rpc_tp.h>
#include <wlc_rpctx.h>
#endif

#ifdef WLAMPDU_PRECEDENCE
#define wlc_ampdu_pktq_plen(pq, tid) \
	(pktq_plen(pq, WLC_PRIO_TO_HI_PREC(tid))\
	+ pktq_plen(pq, WLC_PRIO_TO_PREC(tid)))
static bool wlc_ampdu_prec_enq(wlc_info_t *wlc, struct pktq *q, void *pkt, int tid);
static void * wlc_ampdu_pktq_pdeq(struct pktq *pq, int tid);
static void
wlc_ampdu_pktq_pflush(osl_t *osh, struct pktq *pq, int tid, bool dir, ifpkt_cb_t fn, int arg);
#else
#define wlc_ampdu_prec_enq(wlc, q, pkt, prec) 	wlc_prec_enq_head(wlc, q, pkt, prec, FALSE)
#define wlc_ampdu_pktq_plen 					pktq_plen
#define wlc_ampdu_pktq_pdeq 					pktq_pdeq
#define wlc_ampdu_pktq_pflush 					pktq_pflush
#endif /* WLAMPDU_PRECEDENCE */

#ifdef WLAMPDU_MAC
static void BCMFASTPATH
wlc_ampdu_dotxstatus_aqm_complete(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
                                  void *p, tx_status_t *txs, wlc_txh_info_t *txh_info);
#endif /* WLAMPDU_MAC */

#ifdef BCMDBG
static  int 	_txq_prof_enab;
#endif

/* iovar table */
enum {
	IOV_AMPDU_TX,		/* enable/disable ampdu tx */
	IOV_AMPDU_TID,		/* enable/disable per-tid ampdu */
	IOV_AMPDU_TX_DENSITY,	/* ampdu density */
	IOV_AMPDU_SEND_ADDBA,	/* send addba req to specified scb and tid */
	IOV_AMPDU_SEND_DELBA,	/* send delba req to specified scb and tid */
	IOV_AMPDU_MANUAL_MODE,	/* addba tx to be driven by cmd line */
	IOV_AMPDU_NO_BAR,	/* do not send bars */
	IOV_AMPDU_MPDU,		/* max number of mpdus in an ampdu */
	IOV_AMPDU_DUR,		/* max duration of an ampdu (in usec) */
	IOV_AMPDU_RTS,		/* use rts for ampdu */
	IOV_AMPDU_TX_BA_WSIZE,	/* ampdu TX ba window size */
	IOV_AMPDU_RETRY_LIMIT,	/* ampdu retry limit */
	IOV_AMPDU_RR_RETRY_LIMIT, /* regular rate ampdu retry limit */
	IOV_AMPDU_TX_LOWAT,	/* ampdu tx low wm */
	IOV_AMPDU_HIAGG_MODE,	/* agg mpdus with diff retry cnt */
	IOV_AMPDU_PROBE_MPDU,	/* number of mpdus/ampdu for rate probe frames */
	IOV_AMPDU_TXPKT_WEIGHT,	/* weight of ampdu in the txfifo; helps rate lag */
	IOV_AMPDU_FFPLD_RSVD,	/* bytes to reserve in pre-loading info */
	IOV_AMPDU_FFPLD,        /* to display pre-loading info */
	IOV_AMPDU_MAX_TXFUNFL,   /* inverse of acceptable proportion of tx fifo underflows */
	IOV_AMPDU_RETRY_LIMIT_TID,	/* ampdu retry limit per-tid */
	IOV_AMPDU_RR_RETRY_LIMIT_TID, /* regular rate ampdu retry limit per-tid */
	IOV_AMPDU_PS_RETRY_LIMIT, /* max number of pretend power save transitions */
	IOV_AMPDU_MFBR,		/* Use multiple fallback rate */
#ifdef WLOVERTHRUSTER
	IOV_AMPDU_TCP_ACK_RATIO, 	/* max number of ack to suppress in a row. 0 disable */
	IOV_AMPDU_TCP_ACK_RATIO_DEPTH, 	/* max number of multi-ack. 0 disable */
#endif /* WLOVERTHRUSTER */
	IOV_AMPDUMAC,		/* Use ucode or hw assisted agregation */
	IOV_AMPDU_AGGMODE,	/* agregation mode: HOST or MAC */
	IOV_AMPDU_AGGFIFO,      /* aggfifo depth */
#ifdef WLMEDIA_TXFAILEVENT
	IOV_AMPDU_TXFAIL_EVENT, /* Tx failure event to the host */
#endif /* WLMEDIA_TXFAILEVENT */
	IOV_FRAMEBURST_OVR,	/* Override framebursting in absense of ampdu */
	IOV_AMPDU_TXQ_PROFILING_START,	/* start sampling TXQ profiling data */
	IOV_AMPDU_TXQ_PROFILING_DUMP,	/* dump TXQ histogram */
	IOV_AMPDU_TXQ_PROFILING_SNAPSHOT,	/* take a snapshot of TXQ histogram */
	IOV_AMPDU_RELEASE,	 /* max # of mpdus released at a time */
	IOV_AMPDU_LAST
};

static const bcm_iovar_t ampdu_iovars[] = {
	{"ampdu_tx", IOV_AMPDU_TX, (IOVF_SET_DOWN), IOVT_BOOL, 0},	/* only if down */
	{"ampdu_tid", IOV_AMPDU_TID, (0), IOVT_BUFFER, sizeof(struct ampdu_tid_control)},
	{"ampdu_tx_density", IOV_AMPDU_TX_DENSITY, (0), IOVT_UINT8, 0},
	{"ampdu_send_addba", IOV_AMPDU_SEND_ADDBA, (0), IOVT_BUFFER, sizeof(struct ampdu_ea_tid)},
	{"ampdu_send_delba", IOV_AMPDU_SEND_DELBA, (0), IOVT_BUFFER, sizeof(struct ampdu_ea_tid)},
	{"ampdu_manual_mode", IOV_AMPDU_MANUAL_MODE, (IOVF_SET_DOWN), IOVT_BOOL, 0},
	{"ampdu_mpdu", IOV_AMPDU_MPDU, (0), IOVT_INT8, 0},
#if defined(AP)
	{"ampdu_ps_retry_limit", IOV_AMPDU_PS_RETRY_LIMIT, (0), IOVT_UINT8, 0},
#endif
#ifdef WLOVERTHRUSTER
	{"ack_ratio", IOV_AMPDU_TCP_ACK_RATIO, (0), IOVT_UINT8, 0},
	{"ack_ratio_depth", IOV_AMPDU_TCP_ACK_RATIO_DEPTH, (0), IOVT_UINT8, 0},
#endif /* WLOVERTHRUSTER */
#ifdef BCMDBG
	{"ampdu_aggfifo", IOV_AMPDU_AGGFIFO, 0, IOVT_UINT8, 0},
	{"ampdu_ffpld", IOV_AMPDU_FFPLD, (0), IOVT_UINT32, 0},
#endif

	{"ampdumac", IOV_AMPDUMAC, (0), IOVT_UINT8, 0},
#ifdef WLMEDIA_TXFAILEVENT
	{"ampdu_txfail_event", IOV_AMPDU_TXFAIL_EVENT, (0), IOVT_BOOL, 0},
#endif /* WLMEDIA_TXFAILEVENT */
	{"ampdu_aggmode", IOV_AMPDU_AGGMODE, (IOVF_SET_DOWN), IOVT_INT8, 0},	/* only if down */
	{"frameburst_override", IOV_FRAMEBURST_OVR, (0), IOVT_BOOL, 0},
	{"ampdu_txq_prof_start", IOV_AMPDU_TXQ_PROFILING_START, (0), IOVT_VOID, 0},
	{"ampdu_txq_prof_dump", IOV_AMPDU_TXQ_PROFILING_DUMP, (0), IOVT_VOID, 0},
	{"ampdu_txq_ss", IOV_AMPDU_TXQ_PROFILING_SNAPSHOT, (0), IOVT_VOID, 0},
	{"ampdu_release", IOV_AMPDU_RELEASE, (0), IOVT_UINT8, 0},
	{NULL, 0, 0, 0, 0}
};

#ifdef WLPKTDLYSTAT
static const char ac_names[4][6] = {"AC_BE", "AC_BK", "AC_VI", "AC_VO"};
#endif

#if D11CONF_GE(40)
#define AMPDU_MAX_MPDU		64		/* max number of mpdus in an ampdu */
#else
#define AMPDU_MAX_MPDU		32		/* max number of mpdus in an ampdu */
#endif
#define AMPDU_NUM_MPDU_LEGACY	16		/* max number of mpdus in an ampdu to a legacy */
#define AMPDU_DEF_PROBE_MPDU	2		/* def number of mpdus in a rate probe ampdu */

#ifndef AMPDU_BA_MAX_WSIZE
/* max Tx ba window size (in pdu) for array allocations in structures. */
#define AMPDU_BA_MAX_WSIZE	64
#endif

#ifndef AMPDU_TX_BA_MAX_WSIZE
#define AMPDU_TX_BA_MAX_WSIZE	64		/* max Tx ba window size (in pdu) */
#endif /* AMPDU_TX_BA_MAX_WSIZE */
#ifndef AMPDU_TX_BA_DEF_WSIZE
#define AMPDU_TX_BA_DEF_WSIZE	64		/* default Tx ba window size (in pdu) */
#endif /* AMPDU_TX_BA_DEF_WSIZE */

#define	AMPDU_BA_BITS_IN_BITMAP	64		/* number of bits in bitmap */
#define	AMPDU_MAX_DUR		5416		/* max dur of tx ampdu (in usec) */
#define AMPDU_MAX_RETRY_LIMIT	32		/* max tx retry limit */
#define AMPDU_DEF_RETRY_LIMIT	5		/* default tx retry limit */
#define AMPDU_DEF_RR_RETRY_LIMIT	2	/* default tx retry limit at reg rate */
#define AMPDU_DEF_TX_LOWAT	1		/* default low transmit wm */
#define AMPDU_DEF_TXPKT_WEIGHT	2		/* default weight of ampdu in txfifo */
#define AMPDU_DEF_FFPLD_RSVD	2048		/* default ffpld reserved bytes */
#define AMPDU_MIN_FFPLD_RSVD	512		/* minimum ffpld reserved bytes */
#define AMPDU_BAR_RETRY_CNT	50		/* # of bar retries before delba */
#define AMPDU_INI_FREE		10		/* # of inis to be freed on detach */
#define AMPDU_ADDBA_REQ_RETRY_CNT	4	/* # of addbareq retries before delba */
#define AMPDU_INI_OFF_TIMEOUT	60		/* # of sec in off state */
#define	AMPDU_SCB_MAX_RELEASE	32		/* max # of mpdus released at a time */
#ifndef AMPDU_SCB_MAX_RELEASE_AQM
#define	AMPDU_SCB_MAX_RELEASE_AQM	64	/* max # of mpdus released at a time */
#endif

#define AMPDU_INI_DEAD_TIMEOUT	2		/* # of sec without ini progress */
#define AMPDU_INI_CLEANUP_TIMEOUT	2	/* # of sec in pending off state */

/* internal BA states */
#define	AMPDU_TID_STATE_BA_OFF		0x00	/* block ack OFF for tid */
#define	AMPDU_TID_STATE_BA_ON		0x01	/* block ack ON for tid */
#define	AMPDU_TID_STATE_BA_PENDING_ON	0x02	/* block ack pending ON for tid */
#define	AMPDU_TID_STATE_BA_PENDING_OFF	0x03	/* block ack pending OFF for tid */

#define NUM_FFPLD_FIFO 4                        /* number of fifo concerned by pre-loading */
#define FFPLD_TX_MAX_UNFL   200                 /* default value of the average number of ampdu
						 * without underflows
						 */
#define FFPLD_MPDU_SIZE 1800                    /* estimate of maximum mpdu size */
#define FFPLD_PLD_INCR 1000                     /* increments in bytes */
#define FFPLD_MAX_AMPDU_CNT 5000                /* maximum number of ampdu we
						 * accumulate between resets.
						 */
/* retry BAR in watchdog reason codes */
#define AMPDU_R_BAR_DISABLED		0	/* disabled retry BAR in watchdog */
#define AMPDU_R_BAR_NO_BUFFER		1	/* retry BAR due to out of buffer */
#define AMPDU_R_BAR_CALLBACK_FAIL	2	/* retry BAR due to callback register fail */
#define AMPDU_R_BAR_HALF_RETRY_CNT	3	/* retry BAR due to reach to half
						 * AMPDU_BAR_RETRY_CNT
						 */
#define AMPDU_R_BAR_BLOCKED		4	/* retry BAR due to blocked data fifo */

#define AMPDU_DEF_TCP_ACK_RATIO		2	/* default TCP ACK RATIO */
#define AMPDU_DEF_TCP_ACK_RATIO_DEPTH	0	/* default TCP ACK RATIO DEPTH */

#ifndef AMPDU_SCB_DRAIN_CNT
#define AMPDU_SCB_DRAIN_CNT	8
#endif

/* maximum number of frames can be released to HW (in-transit limit) */
#define AMPDU_AQM_RELEASE_MAX          256	/* for BE */
#define AMPDU_AQM_RELEASE_DEFAULT       32


#define IS_SEQ_ADVANCED(a, b) \
	(MODSUB_POW2((a), (b), SEQNUM_MAX) < (SEQNUM_MAX >> 1))

int wl_ampdu_drain_cnt = AMPDU_SCB_DRAIN_CNT;

/* useful macros */
#define NEXT_SEQ(seq) MODINC_POW2((seq), SEQNUM_MAX)
#define NEXT_TX_INDEX(index) MODINC_POW2((index), (ampdu_tx->ba_max_tx_wsize))
#define TX_SEQ_TO_INDEX(seq) ((seq) & ((ampdu_tx->ba_max_tx_wsize) - 1))

/* max possible overhead per mpdu in the ampdu; 3 is for roundup if needed */
#define AMPDU_MAX_MPDU_OVERHEAD (DOT11_FCS_LEN + DOT11_ICV_AES_LEN + AMPDU_DELIMITER_LEN + 3 \
	+ DOT11_A4_HDR_LEN + DOT11_QOS_LEN + DOT11_IV_MAX_LEN)

/* ampdu related stats */
typedef struct wlc_ampdu_cnt {
	/* initiator stat counters */
	uint32 txampdu;		/* ampdus sent */
#ifdef WLCNT
	uint32 txmpdu;		/* mpdus sent */
	uint32 txregmpdu;	/* regular(non-ampdu) mpdus sent */
	union {
		uint32 txs;		/* MAC agg: txstatus received */
		uint32 txretry_mpdu;	/* retransmitted mpdus */
	} u0;
	uint32 txretry_ampdu;	/* retransmitted ampdus */
	uint32 txfifofull;	/* release ampdu due to insufficient tx descriptors */
	uint32 txfbr_mpdu;	/* retransmitted mpdus at fallback rate */
	uint32 txfbr_ampdu;	/* retransmitted ampdus at fallback rate */
	union {
		uint32 txampdu_sgi;	/* ampdus sent with sgi */
		uint32 txmpdu_sgi;	/* ucode agg: mpdus sent with sgi */
	} u1;
	union {
		uint32 txampdu_stbc;	/* ampdus sent with stbc */
		uint32 txmpdu_stbc;	/* ucode agg: mpdus sent with stbc */
	} u2;
	uint32 txampdu_mfbr_stbc; /* ampdus sent at mfbr with stbc */
	uint32 txrel_wm;	/* mpdus released due to lookahead wm */
	uint32 txrel_size;	/* mpdus released due to max ampdu size */
	uint32 sduretry;	/* sdus retry returned by sendsdu() */
	uint32 sdurejected;	/* sdus rejected by sendsdu() */
	uint32 txdrop;		/* dropped packets */
	uint32 txr0hole;	/* lost packet between scb and sendampdu */
	uint32 txrnhole;	/* lost retried pkt */
	uint32 txrlag;		/* laggard pkt (was considered lost) */
	uint32 txreg_noack;	/* no ack for regular(non-ampdu) mpdus sent */
	uint32 txaddbareq;	/* addba req sent */
	uint32 rxaddbaresp;	/* addba resp recd */
	uint32 txbar;		/* bar sent */
	uint32 rxba;		/* ba recd */
	uint32 noba;            /* ba missing */
	uint32 txstuck;		/* watchdog bailout for stuck state */
	uint32 orphan;		/* orphan pkts where scb/ini has been cleaned */

#ifdef WLAMPDU_MAC
	uint32 epochdelta;	/* How many times epoch has changed */
	uint32 echgr1;          /* epoch change reason -- plcp */
	uint32 echgr2;          /* epoch change reason -- rate_probe */
	uint32 echgr3;          /* epoch change reason -- a-mpdu as regmpdu */
	uint32 echgr4;          /* epoch change reason -- regmpdu */
	uint32 echgr5;          /* epoch change reason -- dest/tid */
	uint32 echgr6;          /* epoch change reason -- seq no */
	uint32 tx_mrt, tx_fbr;  /* number of MPDU tx at main/fallback rates */
	uint32 txsucc_mrt;      /* number of successful MPDU tx at main rate */
	uint32 txsucc_fbr;      /* number of successful MPDU tx at fallback rate */
	uint32 enq;             /* totally enqueued into aggfifo */
	uint32 cons;            /* totally reported in txstatus */
	uint32 pending;         /* number of entries currently in aggfifo or txsq */
#endif /* WLAMPDU_MAC */

	/* general: both initiator and responder */
	uint32 rxunexp;		/* unexpected packets */
	uint32 txdelba;		/* delba sent */
	uint32 rxdelba;		/* delba recd */

#ifdef WLPKTDLYSTAT
	/* PER (per mcs) statistics */
	uint32 txmpdu_cnt[AMPDU_MAX_MCS+1];	/* MPDUs per mcs */
	uint32 txmpdu_succ_cnt[AMPDU_MAX_MCS+1]; /* acked MPDUs per MCS */
#ifdef WL11AC
	uint32 txmpdu_vht_cnt[AMPDU_MAX_VHT];	/* MPDUs per vht */
	uint32 txmpdu_vht_succ_cnt[AMPDU_MAX_VHT]; /* acked MPDUs per vht */
#endif /* WL11AC */
#endif /* WLPKTDLYSTAT */
#endif /* WLCNT */
} wlc_ampdu_tx_cnt_t;

/* structure to hold tx fifo information and pre-loading state
 * counters specific to tx underflows of ampdus
 * some counters might be redundant with the ones in wlc or ampdu structures.
 * This allows to maintain a specific state independantly of
 * how often and/or when the wlc counters are updated.
 */
typedef struct wlc_fifo_info {
	uint16 ampdu_pld_size;	/* number of bytes to be pre-loaded */
	uint8 mcs2ampdu_table[AMPDU_MAX_MCS+1]; /* per-mcs max # of mpdus in an ampdu */
	uint16 prev_txfunfl;	/* num of underflows last read from the HW macstats counter */
	uint32 accum_txfunfl;	/* num of underflows since we modified pld params */
	uint32 accum_txampdu;	/* num of tx ampdu since we modified pld params  */
	uint32 prev_txampdu;	/* previous reading of tx ampdu */
	uint32 dmaxferrate;	/* estimated dma avg xfer rate in kbits/sec */
} wlc_fifo_info_t;

#ifdef WLAMPDU_MAC
/* frame type enum for epoch change */
enum {
	AMPDU_NONE,
	AMPDU_11N,
	AMPDU_11VHT
};

typedef struct ampdumac_info {
#ifdef WLAMPDU_UCODE
	uint16 txfs_addr_strt;
	uint16 txfs_addr_end;
	uint16 txfs_wptr;
	uint16 txfs_rptr;
	uint16 txfs_wptr_addr;
	uint8  txfs_wsize;
#endif
	uint8 epoch;
	bool change_epoch;
	/* any change of following elements will change epoch */
	struct scb *prev_scb;
	uint8 prev_tid;
	uint8 prev_antsel;
	uint8 prev_txpwr;
	uint8 prev_ft;
#if D11CONF_GE(40)
	uint8 prev_plcp[5];
#else
	uint8 prev_plcp[2];
#endif
	/* stats */
	int in_queue;
	uint8 depth;
} ampdumac_info_t;
#endif /* WLAMPDU_MAC */

#ifdef WLOVERTHRUSTER
typedef struct wlc_tcp_ack_info {
	uint8 tcp_ack_ratio;
	uint32 tcp_ack_total;
	uint32 tcp_ack_dequeued;
	uint32 tcp_ack_multi_dequeued;
	uint32 current_dequeued;
	uint8 tcp_ack_ratio_depth;
} wlc_tcp_ack_info_t;
#endif

typedef struct {
	uint32 retry_histogram[AMPDU_MAX_MPDU+1]; /* histogram for retried pkts */
	uint32 end_histogram[AMPDU_MAX_MPDU+1];	/* errs till end of ampdu */
	uint32 mpdu_histogram[AMPDU_MAX_MPDU+1]; /* mpdus per ampdu histogram */
	/* txmcs in sw agg is ampdu cnt, and is mpdu cnt for mac agg */
	uint32 supr_reason[NUM_TX_STATUS_SUPR];	/* reason for suppressed err code */
	uint32 txmcs[AMPDU_MAX_MCS+1];		/* mcs of tx pkts */

#ifdef WLAMPDU_UCODE
	uint32 schan_depth_histo[AMPDU_MAX_MPDU+1]; /* side channel depth */
#endif /* WLAMPDU_UCODE */
	uint32 txmcssgi[AMPDU_MAX_MCS+1];		/* mcs of tx pkts */
	uint32 txmcsstbc[AMPDU_MAX_MCS+1];		/* mcs of tx pkts */
#ifdef WLAMPDU_MAC
	/* used by aqm_agg to get PER */
	uint32 txmcs_succ[AMPDU_MAX_MCS+1];    /* succ mpdus tx per mcs */
#endif

#ifdef WL11AC
	uint32 txvht[AMPDU_MAX_VHT];		/* vht of tx pkts */
#ifdef WLAMPDU_MAC
	/* used by aqm_agg to get PER */
	uint32 txvht_succ[AMPDU_MAX_VHT];    /* succ mpdus tx per vht */
#endif
	uint32 txvhtsgi[AMPDU_MAX_VHT];		/* vht of tx pkts */
	uint32 txvhtstbc[AMPDU_MAX_VHT];		/* vht of tx pkts */
#endif /* WL11AC */
} ampdu_dbg_t;

/* AMPDU module specific state */
struct ampdu_tx_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	int scb_handle;		/* scb cubby handle to retrieve data from scb */
	bool manual_mode;	/* addba tx to be driven by user */
	bool no_bar;		/* do not send bar on failure */
	uint8 ini_enable[AMPDU_MAX_SCB_TID]; /* per-tid initiator enable/disable of ampdu */
	uint8 ba_policy;	/* ba policy; immediate vs delayed */
	uint8 ba_tx_wsize;      /* Tx ba window size (in pdu) */
	uint8 retry_limit;	/* mpdu transmit retry limit */
#if defined(AP)
	uint8 ps_retry_limit;	/* how many times we do the 'pretend' PS mode */
#endif
	uint8 rr_retry_limit;	/* mpdu transmit retry limit at regular rate */
	uint8 retry_limit_tid[AMPDU_MAX_SCB_TID];	/* per-tid mpdu transmit retry limit */
	/* per-tid mpdu transmit retry limit at regular rate */
	uint8 rr_retry_limit_tid[AMPDU_MAX_SCB_TID];
	uint8 mpdu_density;	/* min mpdu spacing (0-7) ==> 2^(x-1)/8 usec */
	int8 max_pdu;		/* max pdus allowed in ampdu */
	uint16 dur;		/* max duration of an ampdu (in usec) */
	uint8 hiagg_mode;	/* agg mpdus with different retry cnt */
	uint8 probe_mpdu;	/* max mpdus allowed in ampdu for probe pkts */
	uint8 txpkt_weight;	/* weight of ampdu in txfifo; reduces rate lag */
	uint8 delba_timeout;	/* timeout after which to send delba (sec) */
	uint8 tx_rel_lowat;	/* low watermark for num of pkts in transit */
	uint8 ini_free_index;	/* index for next entry in ini_free */
	uint32 ffpld_rsvd;	/* number of bytes to reserve for preload */
	uint32 max_txlen[MCS_TABLE_SIZE][2][2];	/* max size of ampdu per mcs, bw and sgi */
	void *ini_free[AMPDU_INI_FREE];	/* array of ini's to be freed on detach */
#ifdef WLCNT
	wlc_ampdu_tx_cnt_t *cnt;	/* counters/stats */
#endif
	bool mfbr;		/* enable multiple fallback rate */
#ifdef WLAMPDU_MAC
	ampdumac_info_t hagg[AC_COUNT];
#endif
	ampdu_dbg_t *amdbg;
	uint32 tx_max_funl;              /* underflows should be kept such that
					  * (tx_max_funfl*underflows) < tx frames
					  */
	wlc_fifo_info_t fifo_tb[NUM_FFPLD_FIFO]; /* table of fifo infos  */

#ifdef WLC_HIGH_ONLY
	void *p;
	tx_status_t txs;
	bool waiting_status; /* To help sanity checks */
#endif
	bool txaggr;	  /* tx state of aggregation: ON/OFF */
	int8 txaggr_override;	/* config override to disable ampdu tx */
#ifdef WLOVERTHRUSTER
	wlc_tcp_ack_info_t tcp_ack_info;
#endif
	uint8  aggfifo_depth;   /* soft capability of AGGFIFO */
	int8 ampdu_aggmode;	/* aggregation mode, HOST or MAC */
	int8 default_pdu;	/* default pdus allowed in ampdu */
#ifdef WLMEDIA_TXFAILEVENT
	bool tx_fail_event;     /* Whether host requires TX failure event: ON/OFF */
#endif /* WLMEDIA_TXFAILEVENT */
	bool	fb_override;		/* override for frame bursting in absense of ba session */
	bool	fb_override_enable; /* configuration to enable/disable ampd_no_frameburst */
	uint8	ba_max_tx_wsize;	/* Tx ba window size (in pdu) */
	uint8	release;			/* # of mpdus released at a time */
	uint16  aqm_max_release[AMPDU_MAX_SCB_TID];
};

static void wlc_ampdu_tx_cleanup(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ampdu_tx_info_t *ampdu_tx);

#ifdef WLMEDIA_TXFAILEVENT
static void wlc_tx_failed_event(wlc_info_t *wlc, struct scb *scb, wlc_bsscfg_t *bsscfg,
	tx_status_t *txs, void *p, uint32 flags);
#endif /* WLMEDIA_TXFAILEVENT */

/* structure to store per-tid state for the ampdu initiator */
struct scb_ampdu_tid_ini {
	uint8 ba_state;		/* ampdu ba state */
	uint8 ba_wsize;		/* negotiated ba window size (in pdu) */
	uint8 tid;		/* initiator tid for easy lookup */
	uint8 txretry[AMPDU_BA_MAX_WSIZE];	/* tx retry count; indexed by seq modulo */
	uint8 ackpending[AMPDU_BA_MAX_WSIZE/NBBY];	/* bitmap: set if ack is pending */
	uint8 barpending[AMPDU_BA_MAX_WSIZE/NBBY];	/* bitmap: set if bar is pending */
	uint16 tx_in_transit;	/* number of pending mpdus in transit in driver */
	uint16 barpending_seq;	/* seqnum for bar */
	uint16 acked_seq;	/* last ack recevied */
	uint16 start_seq;	/* seqnum of the first unacknowledged packet */
	uint16 max_seq;		/* max unacknowledged seqnum sent */
	uint16 tx_exp_seq;	/* next exp seq in sendampdu */
	uint16 rem_window;	/* remaining ba window (in pdus) that can be txed */
	uint16 bar_ackpending_seq; /* seqnum of bar for which ack is pending */
	uint16 retry_seq[AMPDU_BA_MAX_WSIZE]; /* seq of released retried pkts */
	uint16 retry_head;	/* head of queue ptr for retried pkts */
	uint16 retry_tail;	/* tail of queue ptr for retried pkts */
	uint16 retry_cnt;	/* cnt of retried pkts */
	bool bar_ackpending;	/* true if there is a bar for which ack is pending */
	bool free_me;		/* true if ini is to be freed on receiving ack for bar */
	bool alive;		/* true if making forward progress */
	uint8 retry_bar;	/* reason code if bar to be retried at watchdog */
	uint8 bar_cnt;		/* number of bars sent with no progress */
	uint8 addba_req_cnt;	/* number of addba_req sent with no progress */
	uint8 cleanup_ini_cnt;	/* number of sec waiting in pending off state */
	uint8 dead_cnt;		/* number of sec without any progress */
	uint8 off_cnt;		/* number of sec in off state before next try */
	struct scb *scb;	/* backptr for easy lookup */
};

#ifdef BCMDBG
typedef struct scb_ampdu_cnt_tx {
	uint32 txampdu;
	uint32 txmpdu;
	uint32 txdrop;
	uint32 txstuck;
	uint32 txaddbareq;
	uint32 txrlag;
	uint32 txnoroom;
	uint32 sduretry;
	uint32 sdurejected;
	uint32 txbar;
	uint32 txreg_noack;
	uint32 noba;
	uint32 rxaddbaresp;
	uint32 rxdelba;
	uint32 rxba;
} scb_ampdu_cnt_tx_t;
#endif	/* BCMDBG */

/* scb cubby structure. ini and resp are dynamically allocated if needed */
struct scb_ampdu_tx {
	struct scb *scb;		/* back pointer for easy reference */
	uint8 mpdu_density;		/* mpdu density */
	uint8 max_pdu;			/* max pdus allowed in ampdu */
	uint8 release;			/* # of mpdus released at a time */
	uint16 min_len;			/* min mpdu len to support the density */
	uint32 max_rxlen;		/* max ampdu rcv length; 8k, 16k, 32k, 64k */
	struct pktq txq;		/* sdu transmit queue pending aggregation */
	scb_ampdu_tid_ini_t *ini[AMPDU_MAX_SCB_TID];	/* initiator info */
	uint16 min_lens[AMPDU_MAX_MCS+1];	/* min mpdu lens per mcs */
	ampdu_tx_info_t *ampdu_tx; /* back ref to main ampdu */
#ifdef BCMDBG
	scb_ampdu_cnt_tx_t cnt;
#endif	/* BCMDBG */
};

#ifdef BCMDBG
#define AMPDUSCBCNTADD(cnt, upd) ((cnt) += (upd))
#define AMPDUSCBCNTINCR(cnt) ((cnt)++)
#else
#define AMPDUSCBCNTADD(a, b) do { } while (0)
#define AMPDUSCBCNTINCR(a)  do { } while (0)
#endif

struct ampdu_tx_cubby {
	scb_ampdu_tx_t *scb_tx_cubby;
};

#define SCB_AMPDU_INFO(ampdu, scb) (SCB_CUBBY((scb), (ampdu)->scb_handle))
#define SCB_AMPDU_TX_CUBBY(ampdu, scb) \
	(((struct ampdu_tx_cubby *)SCB_AMPDU_INFO(ampdu, scb))->scb_tx_cubby)

#define P2P_ABS_SUPR(wlc, supr)	(P2P_ENAB((wlc)->pub) && (supr) == TX_STATUS_SUPR_NACK_ABS)

/* local prototypes */
static int scb_ampdu_tx_init(void *context, struct scb *scb);
static void scb_ampdu_tx_deinit(void *context, struct scb *scb);
static void scb_ampdu_txflush(void *context, struct scb *scb);
static int wlc_ampdu_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static int wlc_ampdu_watchdog(void *hdl);
static int wlc_ampdu_down(void *hdl);
static void wlc_ampdu_init_min_lens(scb_ampdu_tx_t *scb_ampdu);
static void wlc_ffpld_init(ampdu_tx_info_t *ampdu_tx);

static int wlc_ffpld_check_txfunfl(wlc_info_t *wlc, int f);
static void wlc_ffpld_calc_mcs2ampdu_table(ampdu_tx_info_t *ampdu_tx, int f);
#ifdef BCMDBG
static void wlc_ffpld_show(ampdu_tx_info_t *ampdu_tx);
#endif /* BCMDBG */
#ifdef WLOVERTHRUSTER
static void wlc_ampdu_tcp_ack_suppress(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
                                       void *p, uint8 tid);
#endif
#ifdef WLAMPDU_MAC
static int BCMFASTPATH _wlc_sendampdu_aqm(ampdu_tx_info_t *ampdu, wlc_txq_info_t *qi,
                                          void **pdu, int prec);
#endif /* WLAMPDU_MAC */
static int BCMFASTPATH _wlc_sendampdu_noaqm(ampdu_tx_info_t *ampdu, wlc_txq_info_t *qi,
                                            void **pdu, int prec);

#ifdef WLAMPDU_MAC
static void wlc_ampdu_dotxstatus_regmpdu_aqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs);
static bool BCMFASTPATH ampdu_is_exp_seq_aqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini,
	uint16 seq, int* seq_changed);
static INLINE void wlc_ampdu_ini_move_window_aqm(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);
#endif /* WLAMPDU_MAC */

static void wlc_ampdu_dotxstatus_regmpdu_noaqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs);
static bool BCMFASTPATH ampdu_is_exp_seq_noaqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini,
	uint16 seq, int* seq_changed);
static INLINE void wlc_ampdu_ini_move_window_noaqm(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);

static void wlc_ampdu_send_bar(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini, bool start);

static void wlc_ampdu_agg(void *ctx, struct scb *scb, void *p, uint prec);
static scb_ampdu_tid_ini_t *wlc_ampdu_init_tid_ini(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, uint8 tid, bool override);
static void ampdu_ba_state_off(scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini);
static bool wlc_ampdu_txeval(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, bool force);
static void wlc_ampdu_release(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, uint16 release);
static void wlc_ampdu_free_chain(ampdu_tx_info_t *ampdu_tx, void *p, tx_status_t *txs,
	bool txs3_done);

static void wlc_send_bar_complete(wlc_info_t *wlc, uint txstatus, void *arg);
static void ampdu_update_max_txlen(ampdu_tx_info_t *ampdu_tx, uint16 dur);

static void scb_ampdu_update_config(ampdu_tx_info_t *ampdu_tx, struct scb *scb);
static void scb_ampdu_update_config_all(ampdu_tx_info_t *ampdu_tx);

#ifdef WLAMPDU_MAC
static int aggfifo_enque(ampdu_tx_info_t *ampdu_tx, int length, int qid);
static uint get_aggfifo_space(ampdu_tx_info_t *ampdu_tx, int qid);
static uint16 wlc_ampdu_calc_maxlen(ampdu_tx_info_t *ampdu_tx, uint8 plcp0, uint plcp3,
	uint32 txop);
static void wlc_print_ampdu_txstatus(ampdu_tx_info_t *ampdu_tx,
	tx_status_t *pkg1, uint32 s1, uint32 s2, uint32 s3, uint32 s4);
#endif /* WLAMPDU_MAC */

#if defined(DONGLEBUILD)
static void wlc_ampdu_txflowcontrol(wlc_info_t *wlc, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini);
#else
#define wlc_ampdu_txflowcontrol(a, b, c)	do {} while (0)
#endif

static void wlc_ampdu_release_pkts_loop(ampdu_tx_info_t *ampdu_tx,
	scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini, uint16 release);


static void wlc_ampdu_dotxstatus_complete(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
	tx_status_t *txs, uint32 frmtxstatus, uint32 frmtxstatus2, uint32 s3, uint32 s4);

#ifdef BCMDBG
static void ampdu_dump_ini(scb_ampdu_tid_ini_t *ini);
#else
#define	ampdu_dump_ini(a)
#endif
static uint wlc_ampdu_txpktcnt(void *hdl);

static void wlc_ampdu_activate(void *ctx, struct scb *scb);

static txmod_fns_t ampdu_txmod_fns = {
	wlc_ampdu_agg,
	wlc_ampdu_txpktcnt,
	scb_ampdu_txflush,
	wlc_ampdu_activate
};

#ifdef WLAMPDU_MAC
static uint16
wlc_ampdu_rawbits_to_ncons(uint16 raw_bits)
{
	return ((raw_bits & TX_STATUS40_NCONS) >> TX_STATUS40_NCONS_SHIFT);
}
#endif /* WLAMPDU_MAC */

static void wlc_ampdu_activate(void *ctx, struct scb *scb)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)ctx;
#ifdef MFP
	if (SCB_AUTHORIZED(scb))
#endif

	wlc_ampdu_agg_state_update_tx(ampdu_tx->wlc, SCB_BSSCFG(scb), ON);
}

#define pkt_txh_seqnum(wlc, p) wlc_pkt_get_seq(wlc, p)

static void wlc_set_ampdu_tunables(wlc_info_t *wlc)
{
	/* Select the max PDUs tunables for 2 stream and 3 stream
	  * depending on the hardware support.
	  */
	if (D11REV_GE(wlc->pub->corerev, D11AQM_CORE)) {
		wlc->pub->tunables->ampdunummpdu2streams = AMPDU_NUM_MPDU_2SS_D11AQM;
		wlc->pub->tunables->ampdunummpdu3streams = AMPDU_NUM_MPDU_3SS_D11AQM;
	} else if (D11REV_GE(wlc->pub->corerev, D11HT_CORE)) {
		wlc->pub->tunables->ampdunummpdu2streams = AMPDU_NUM_MPDU_2SS_D11HT;
		wlc->pub->tunables->ampdunummpdu3streams = AMPDU_NUM_MPDU_3SS_D11HT;
	} else {
		wlc->pub->tunables->ampdunummpdu2streams = AMPDU_NUM_MPDU_2SS_D11LEGACY;
		wlc->pub->tunables->ampdunummpdu3streams = AMPDU_NUM_MPDU_2SS_D11LEGACY;
	}
}

ampdu_tx_info_t *
BCMATTACHFN(wlc_ampdu_tx_attach)(wlc_info_t *wlc)
{
	ampdu_tx_info_t *ampdu_tx;
	int i;

	/* some code depends on packed structures */
	STATIC_ASSERT(sizeof(struct dot11_bar) == DOT11_BAR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_ba) == DOT11_BA_LEN + DOT11_BA_BITMAP_LEN);
	STATIC_ASSERT(sizeof(struct dot11_ctl_header) == DOT11_CTL_HDR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_addba_req) == DOT11_ADDBA_REQ_LEN);
	STATIC_ASSERT(sizeof(struct dot11_addba_resp) == DOT11_ADDBA_RESP_LEN);
	STATIC_ASSERT(sizeof(struct dot11_delba) == DOT11_DELBA_LEN);
	STATIC_ASSERT(DOT11_MAXNUMFRAGS == NBITS(uint16));
	STATIC_ASSERT(ISPOWEROF2(AMPDU_TX_BA_MAX_WSIZE));

	wlc_set_ampdu_tunables(wlc);

	ASSERT(wlc->pub->tunables->ampdunummpdu2streams <= AMPDU_MAX_MPDU);
	ASSERT(wlc->pub->tunables->ampdunummpdu2streams > 0);
	ASSERT(wlc->pub->tunables->ampdunummpdu3streams <= AMPDU_MAX_MPDU);
	ASSERT(wlc->pub->tunables->ampdunummpdu3streams > 0);

	if (!(ampdu_tx = (ampdu_tx_info_t *)MALLOC(wlc->osh, sizeof(ampdu_tx_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	bzero((char *)ampdu_tx, sizeof(ampdu_tx_info_t));
	ampdu_tx->wlc = wlc;

	ampdu_tx->ba_max_tx_wsize = AMPDU_TX_BA_MAX_WSIZE;

#ifdef WLCNT
	if (!(ampdu_tx->cnt = (wlc_ampdu_tx_cnt_t *)MALLOC(wlc->osh, sizeof(wlc_ampdu_tx_cnt_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero((char *)ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
#endif /* WLCNT */

	/* initialize all priorities to allow AMPDU aggregation */
	for (i = 0; i < AMPDU_MAX_SCB_TID; i++)
		ampdu_tx->ini_enable[i] = TRUE;

	/* For D11 revs < 40, disable AMPDU on some priorities due to underlying fifo space
	 * allocation.
	 * D11 rev >= 40 has a shared tx fifo space managed by BMC hw that allows AMPDU agg
	 * to be used for any fifo.
	 */
	if (D11REV_LT(wlc->pub->corerev, 40)) {
		/* Disable ampdu for VO by default */
		ampdu_tx->ini_enable[PRIO_8021D_VO] = FALSE;
		ampdu_tx->ini_enable[PRIO_8021D_NC] = FALSE;

#ifndef MACOSX
		/* Disable ampdu for BK by default since not enough fifo space except for MACOS */
		ampdu_tx->ini_enable[PRIO_8021D_NONE] = FALSE;
		ampdu_tx->ini_enable[PRIO_8021D_BK] = FALSE;
#endif /* MACOSX */

		/* Enable aggregation for BK */
		if (D11REV_IS(wlc->pub->corerev, 28) ||
		    D11REV_IS(wlc->pub->corerev, 16) ||
		    D11REV_IS(wlc->pub->corerev, 17) ||
		    D11REV_IS(wlc->pub->corerev, 26) ||
			D11REV_IS(wlc->pub->corerev, 22) ||
			D11REV_IS(wlc->pub->corerev, 30) ||
		    (D11REV_IS(wlc->pub->corerev, 29) && WLCISHTPHY(wlc->band))) {
			ampdu_tx->ini_enable[PRIO_8021D_NONE] = TRUE;
			ampdu_tx->ini_enable[PRIO_8021D_BK] = TRUE;
		}
	}

#if defined(__mips__) || defined(__ARM_ARCH_7A__)
	ampdu_tx->ini_enable[PRIO_8021D_VO] = FALSE;
	ampdu_tx->ini_enable[PRIO_8021D_NC] = FALSE;
#endif

	ampdu_tx->ba_policy = DOT11_ADDBA_POLICY_IMMEDIATE;
	ampdu_tx->ba_tx_wsize = AMPDU_TX_BA_DEF_WSIZE;

	if (ampdu_tx->ba_tx_wsize > ampdu_tx->ba_max_tx_wsize) {
		WL_ERROR(("wl%d: The Default AMPDU_TX_BA_WSIZE is greater than MAX value\n",
			wlc->pub->unit));
		ampdu_tx->ba_tx_wsize = ampdu_tx->ba_max_tx_wsize;
	}
	/* PR:106844: Adjust default tx AMPDU density for d11rev40/41 to 8us
	 * to avoid tx underflow. The bug is fixed in corerev42
	 */

#ifdef DSLCPE_C610384
	if (D11REV_LE(wlc->pub->corerev, 41)) {
#else			 
	if (D11REV_IS(wlc->pub->corerev, 40) || D11REV_IS(wlc->pub->corerev, 41) ||
		D11REV_IS(wlc->pub->corerev, 17) || D11REV_IS(wlc->pub->corerev, 28) ||
		D11REV_IS(wlc->pub->corerev, 44)) {
#endif		
		ampdu_tx->mpdu_density = AMPDU_DENSITY_8_US;
	} else {
		ampdu_tx->mpdu_density = AMPDU_DEF_MPDU_DENSITY;
	}
	ampdu_tx->max_pdu = AUTO;
	ampdu_tx->default_pdu = (wlc->stf->txstreams < 3) ? AMPDU_NUM_MPDU_LEGACY : AMPDU_MAX_MPDU;
	ampdu_tx->dur = AMPDU_MAX_DUR;
	ampdu_tx->hiagg_mode = FALSE;
	ampdu_tx->txaggr = ON;
	ampdu_tx->probe_mpdu = AMPDU_DEF_PROBE_MPDU;

	if (AMPDU_MAC_ENAB(wlc->pub))
		ampdu_tx->txpkt_weight = 1;
	else
		ampdu_tx->txpkt_weight = AMPDU_DEF_TXPKT_WEIGHT;

	ampdu_tx->txaggr_override = AUTO;

	ampdu_tx->ffpld_rsvd = AMPDU_DEF_FFPLD_RSVD;

	ampdu_tx->retry_limit = AMPDU_DEF_RETRY_LIMIT;
	ampdu_tx->rr_retry_limit = AMPDU_DEF_RR_RETRY_LIMIT;

	for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
		ampdu_tx->retry_limit_tid[i] = ampdu_tx->retry_limit;
		ampdu_tx->rr_retry_limit_tid[i] = ampdu_tx->rr_retry_limit;
		if (i == PRIO_8021D_BE) {
			ampdu_tx->aqm_max_release[i] = AMPDU_AQM_RELEASE_MAX;
		} else {
			ampdu_tx->aqm_max_release[i] = AMPDU_AQM_RELEASE_DEFAULT;
		}
	}

	ampdu_tx->delba_timeout = 0; /* AMPDUXXX: not yet supported */
	ampdu_tx->tx_rel_lowat = AMPDU_DEF_TX_LOWAT;

#ifdef WLMEDIA_TXFAILEVENT
	ampdu_tx->tx_fail_event = FALSE; /* Tx failure notification off by default */
#endif

	ampdu_update_max_txlen(ampdu_tx, ampdu_tx->dur);

	wlc->ampdu_rts = TRUE;

#ifdef WLOVERTHRUSTER
	if (WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) {
		ampdu_tx->tcp_ack_info.tcp_ack_ratio = AMPDU_DEF_TCP_ACK_RATIO;
		ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth = AMPDU_DEF_TCP_ACK_RATIO_DEPTH;
	}
#endif

	/* reserve cubby in the scb container */
	ampdu_tx->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(struct ampdu_tx_cubby),
		scb_ampdu_tx_init, scb_ampdu_tx_deinit, NULL, (void *)ampdu_tx);

	if (ampdu_tx->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve() failed\n", wlc->pub->unit));
		goto fail;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, ampdu_iovars, "ampdu_tx", ampdu_tx, wlc_ampdu_doiovar,
	                        wlc_ampdu_watchdog, NULL, wlc_ampdu_down)) {
		WL_ERROR(("wl%d: ampdu_tx wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	ampdu_tx->mfbr = TRUE;

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
	if (!(ampdu_tx->amdbg = (ampdu_dbg_t *)MALLOC(wlc->osh, sizeof(ampdu_dbg_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero((char *)ampdu_tx->amdbg, sizeof(ampdu_dbg_t));
#endif /*  defined(BCMDBG) || defined(WLTEST) */
	/* register txmod function */
	wlc_txmod_fn_register(wlc, TXMOD_AMPDU, ampdu_tx, ampdu_txmod_fns);

	ampdu_tx->ampdu_aggmode = AMPDU_AGG_AUTO;
#ifdef WLAMPDU_UCODE
	if (CHIPID(wlc->pub->sih->chip) == BCM4330_CHIP_ID)
		ampdu_tx->ampdu_aggmode = AMPDU_AGGMODE_HOST;
#endif

	/* try to set ampdu to the default value */
	wlc_ampdu_tx_set(ampdu_tx, wlc->pub->_ampdu_tx);

	ampdu_tx->tx_max_funl = FFPLD_TX_MAX_UNFL;
	if (CHIPID(wlc->pub->sih->chip) == BCM4314_CHIP_ID ||
		CHIPID(wlc->pub->sih->chip) == BCM43142_CHIP_ID)
		ampdu_tx->tx_max_funl = 1;
	wlc_ffpld_init(ampdu_tx);
	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub))
		ampdu_tx->release = AMPDU_SCB_MAX_RELEASE;
	else
		ampdu_tx->release = AMPDU_SCB_MAX_RELEASE_AQM;

	return ampdu_tx;

fail:
#ifdef WLCNT
	if (ampdu_tx->cnt)
		MFREE(wlc->osh, ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
#endif /* WLCNT */
	MFREE(wlc->osh, ampdu_tx, sizeof(ampdu_tx_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_ampdu_tx_detach)(ampdu_tx_info_t *ampdu_tx)
{
	int i;

	if (!ampdu_tx)
		return;

	/* free all ini's which were to be freed on callbacks which were never called */
	for (i = 0; i < AMPDU_INI_FREE; i++) {
		if (ampdu_tx->ini_free[i]) {
			MFREE(ampdu_tx->wlc->osh, ampdu_tx->ini_free[i],
				sizeof(scb_ampdu_tid_ini_t));
		}
	}

#ifdef WLCNT
	if (ampdu_tx->cnt)
		MFREE(ampdu_tx->wlc->osh, ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
#endif
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
	if (ampdu_tx->amdbg) {
		MFREE(ampdu_tx->wlc->osh, ampdu_tx->amdbg, sizeof(ampdu_dbg_t));
		ampdu_tx->amdbg = NULL;
	}
#endif

	wlc_module_unregister(ampdu_tx->wlc->pub, "ampdu_tx", ampdu_tx);
	MFREE(ampdu_tx->wlc->osh, ampdu_tx, sizeof(ampdu_tx_info_t));
}

#ifdef PKTQ_LOG
struct pktq * scb_ampdu_prec_pktq(wlc_info_t* wlc, struct scb* scb)
{
	struct pktq *q;
	scb_ampdu_tx_t *scb_ampdu;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	q = &scb_ampdu->txq;

	return q;
}
#endif /* PKTQ_LOG */

/* scb cubby init fn */
static int
scb_ampdu_tx_init(void *context, struct scb *scb)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;
	struct ampdu_tx_cubby *cubby_info = (struct ampdu_tx_cubby *)SCB_AMPDU_INFO(ampdu_tx, scb);
	scb_ampdu_tx_t *scb_ampdu;
	wlc_tunables_t *tunables = ampdu_tx->wlc->pub->tunables;
#ifdef WLAMPDU_PRECEDENCE
	int i;
#endif

	if (scb && !SCB_INTERNAL(scb)) {
		scb_ampdu = MALLOC(ampdu_tx->wlc->osh, sizeof(scb_ampdu_tx_t));
		if (!scb_ampdu)
			return BCME_NOMEM;
		bzero(scb_ampdu, sizeof(scb_ampdu_tx_t));
		cubby_info->scb_tx_cubby = scb_ampdu;
		scb_ampdu->scb = scb;
		scb_ampdu->ampdu_tx = ampdu_tx;

#ifdef WLAMPDU_PRECEDENCE
		pktq_init(&scb_ampdu->txq, WLC_PREC_COUNT,
			MAX(tunables->ampdu_pktq_size, tunables->ampdu_pktq_fav_size));
		for (i = 0; i < WLC_PREC_COUNT; i += 2) {
			pktq_set_max_plen(&scb_ampdu->txq, i, tunables->ampdu_pktq_size);
			pktq_set_max_plen(&scb_ampdu->txq, i+1, tunables->ampdu_pktq_fav_size);
		}
#else
		pktq_init(&scb_ampdu->txq, AMPDU_MAX_SCB_TID, tunables->ampdu_pktq_size);
#endif /* WLAMPDU_PRECEDENCE */

	}
	return 0;
}

/* scb cubby deinit fn */
static void
scb_ampdu_tx_deinit(void *context, struct scb *scb)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;
	struct ampdu_tx_cubby *cubby_info = (struct ampdu_tx_cubby *)SCB_AMPDU_INFO(ampdu_tx, scb);
	scb_ampdu_tx_t *scb_ampdu = NULL;

	WL_AMPDU_UPDN(("scb_ampdu_deinit: enter\n"));

	ASSERT(cubby_info);

	if (cubby_info)
		scb_ampdu = cubby_info->scb_tx_cubby;
	if (!scb_ampdu)
		return;

	scb_ampdu_tx_flush(ampdu_tx, scb);

	MFREE(ampdu_tx->wlc->osh, scb_ampdu, sizeof(scb_ampdu_tx_t));
	cubby_info->scb_tx_cubby = NULL;
}

static void
scb_ampdu_txflush(void *context, struct scb *scb)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)context;

	scb_ampdu_tx_flush(ampdu_tx, scb);
}

void
scb_ampdu_tx_flush(ampdu_tx_info_t *ampdu_tx, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	uint8 tid;

	WL_AMPDU_UPDN(("scb_ampdu_tx_flush: enter\n"));

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
	}

	/* free all buffered tx packets */
	pktq_flush(ampdu_tx->wlc->osh, &scb_ampdu->txq, TRUE, NULL, 0);

#ifdef WLC_HIGH_ONLY
	ASSERT(ampdu_tx->p == NULL);
#endif
}

/* reset the ampdu state machine so that it can gracefully handle packets that were
 * freed from the dma and tx queues during reinit
 */
void
wlc_ampdu_tx_reset(ampdu_tx_info_t *ampdu_tx)
{
	struct scb *scb;
	struct scb_iter scbiter;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint8 tid;

	WL_AMPDU_UPDN(("wlc_ampdu_tx_reset: enter\n"));
	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
		if (!SCB_AMPDU(scb))
			continue;
		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
			ini = scb_ampdu->ini[tid];
			if (!ini)
				continue;
			if ((ini->ba_state == AMPDU_TID_STATE_BA_ON) && ini->tx_in_transit)
				ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
		}
	}
}

static void
scb_ampdu_update_config(ampdu_tx_info_t *ampdu_tx, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	int i;
	int peer3ss = 0;


	/* The maximum number of TX MPDUs supported for the device is determined at the
	  * AMPDU attach time
	 * At this point we know the largest size the device can send, here the intersection
	 * with the peer's capability is done.
	 */

	/* 3SS peer check */
	if (SCB_VHT_CAP(scb)) {
		/* VHT capable peer */
		peer3ss = VHT_MCS_SS_SUPPORTED(3, scb->rateset.vht_mcsmap);
	} else {
		/* HT Peer */
		peer3ss = (scb->rateset.mcs[2] != 0);
	}

	if ((scb->flags & SCB_BRCM) && !(scb->flags2 & SCB2_RX_LARGE_AGG) && !(peer3ss))
	{
		scb_ampdu->max_pdu =
			MIN(ampdu_tx->wlc->pub->tunables->ampdunummpdu2streams,
			AMPDU_NUM_MPDU_LEGACY);
	} else {
		if (peer3ss) {
			scb_ampdu->max_pdu =
				(uint8)ampdu_tx->wlc->pub->tunables->ampdunummpdu3streams;
		} else {
			scb_ampdu->max_pdu =
				(uint8)ampdu_tx->wlc->pub->tunables->ampdunummpdu2streams;
		}
	}

	ampdu_tx->default_pdu = scb_ampdu->max_pdu;

	/* go back to legacy size if some preloading is occuring */
	for (i = 0; i < NUM_FFPLD_FIFO; i++) {
		if (ampdu_tx->fifo_tb[i].ampdu_pld_size > FFPLD_PLD_INCR)
			scb_ampdu->max_pdu = ampdu_tx->default_pdu;
	}

	/* apply user override */
	if (ampdu_tx->max_pdu != AUTO)
		scb_ampdu->max_pdu = (uint8)ampdu_tx->max_pdu;

	if (AMPDU_HW_ENAB(ampdu_tx->wlc->pub))
		scb_ampdu->release = ampdu_tx->ba_max_tx_wsize;
	else {

		if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub))
			scb_ampdu->release = MIN(scb_ampdu->max_pdu, ampdu_tx->release);
		else
			scb_ampdu->release = ampdu_tx->release;

		if (scb_ampdu->max_rxlen)
			scb_ampdu->release = MIN(scb_ampdu->release, scb_ampdu->max_rxlen/1600);

		scb_ampdu->release = MIN(scb_ampdu->release,
			ampdu_tx->fifo_tb[TX_AC_BE_FIFO].mcs2ampdu_table[AMPDU_MAX_MCS]);
	}
	ASSERT(scb_ampdu->release);
}

void
scb_ampdu_update_config_all(ampdu_tx_info_t *ampdu_tx)
{
	struct scb *scb;
	struct scb_iter scbiter;

	WL_AMPDU_UPDN(("scb_ampdu_update_config_all: enter\n"));
	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb))
			scb_ampdu_update_config(ampdu_tx, scb);
	}
}

/* Return the transmit packets held by AMPDU */
static uint
wlc_ampdu_txpktcnt(void *hdl)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	struct scb *scb;
	struct scb_iter scbiter;
	int pktcnt = 0;
	scb_ampdu_tx_t *scb_ampdu;

	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			pktcnt += pktq_len(&scb_ampdu->txq);
		}
	}

	return pktcnt;
}

/* frees all the buffers and cleanup everything on down */
static int
wlc_ampdu_down(void *hdl)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	struct scb *scb;
	struct scb_iter scbiter;

	WL_AMPDU_UPDN(("wlc_ampdu_down: enter\n"));

	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb))
			scb_ampdu_tx_flush(ampdu_tx, scb);
	}

	/* we will need to re-run the pld tuning */
	wlc_ffpld_init(ampdu_tx);

	return 0;
}

static void
wlc_ampdu_tx_cleanup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, ampdu_tx_info_t *ampdu_tx)
{
	uint8 tid;
	scb_ampdu_tx_t *scb_ampdu = NULL;
	struct scb *scb;
	struct scb_iter scbiter;

	scb_ampdu_tid_ini_t *ini;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!SCB_AMPDU(scb))
			continue;

		if (!SCB_ASSOCIATED(scb))
			continue;

		scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
		ASSERT(scb_ampdu);
		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
			ini = scb_ampdu->ini[tid];

			if (ini != NULL)
			{
				if ((ini->ba_state == AMPDU_TID_STATE_BA_ON) ||
					(ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON))
					wlc_ampdu_tx_send_delba(ampdu_tx, scb, tid, TRUE,
						DOT11_RC_TIMEOUT);

				ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, FALSE);
			}
		}

		/* free all buffered tx packets */
		pktq_flush(ampdu_tx->wlc->pub->osh, &scb_ampdu->txq, TRUE, NULL, 0);
	}
}

bool
wlc_ampdu_frameburst_override(ampdu_tx_info_t *ampdu_tx)
{
	if (ampdu_tx == NULL)
		return FALSE;
	else
		return ampdu_tx->fb_override;
}

/* resends ADDBA-Req if the ADDBA-Resp has not come back */
static int
wlc_ampdu_watchdog(void *hdl)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	wlc_info_t *wlc = ampdu_tx->wlc;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 tid;
	void *p;
	bool any_ba_state_on = FALSE;
	bool any_ht_cap = FALSE;


	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_HT_CAP(scb))any_ht_cap = TRUE;

		if (!SCB_AMPDU(scb))
			continue;
		scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
		ASSERT(scb_ampdu);
		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {

			ini = scb_ampdu->ini[tid];
			if (!ini)
				continue;

			switch (ini->ba_state) {
			case AMPDU_TID_STATE_BA_ON:
				/* tickle the sm and release whatever pkts we can */
				wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, TRUE);

				any_ba_state_on = TRUE;

				if (ini->retry_bar) {
					/* Free queued packets when system is short of memory;
					 * release 1/4 buffered and at most wl_ampdu_drain_cnt pkts.
					 */
					if (ini->retry_bar == AMPDU_R_BAR_NO_BUFFER) {
						int rel_cnt, i;

						WL_ERROR(("wl%d: wlc_ampdu_watchdog: "
							"no memory\n", wlc->pub->unit));
						rel_cnt = wlc_ampdu_pktq_plen(&scb_ampdu->txq,
							ini->tid) >> 2;
						if (wl_ampdu_drain_cnt <= 0)
							wl_ampdu_drain_cnt = AMPDU_SCB_DRAIN_CNT;
						rel_cnt = MIN(rel_cnt, wl_ampdu_drain_cnt);
						for (i = 0; i < rel_cnt; i++) {
							p = wlc_ampdu_pktq_pdeq(&scb_ampdu->txq,
								ini->tid);
							ASSERT(PKTPRIO(p) == ini->tid);
							PKTFREE(wlc->pub->osh, p, TRUE);
							AMPDUSCBCNTINCR(scb_ampdu->cnt.txdrop);
							WLCNTINCR(ampdu_tx->cnt->txdrop);
						}
					}

					wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
				}
				else if (ini->bar_cnt >= AMPDU_BAR_RETRY_CNT) {
					wlc_ampdu_tx_send_delba(ampdu_tx, scb, tid,
						TRUE, DOT11_RC_TIMEOUT);
				}
				/* check on forward progress */
				else if (ini->alive) {
					ini->alive = FALSE;
					ini->dead_cnt = 0;
				} else {
#ifdef DSLCPE_C583851
					if (ini->tx_in_transit || wlc_ampdu_pktq_plen(&scb_ampdu->txq,
							ini->tid))
#else
					if (ini->tx_in_transit)
#endif
						ini->dead_cnt++;
					if (ini->dead_cnt == AMPDU_INI_DEAD_TIMEOUT) {
#if defined(AP) && defined(BCMDBG)
						if (SCB_PS(scb)) {
							char* mode = "PS";

							if (SCB_PS_PRETEND(scb)) {
								if (SCB_PS_PRETEND_THRESHOLD(scb)) {
									mode = "threshold PPS";
								}
								else {
									mode = "PPS";
								}
							}

							WL_ERROR(("wl%d: %s: "MACF" in %s mode may "
							          "be stuck or receiver died\n",
							           wlc->pub->unit, __FUNCTION__,
							           ETHER_TO_MACF(scb->ea), mode));
						}
#endif /* AP && BCMDBG */

						WL_ERROR(("wl%d: %s: cleaning up ini"
							" tid %d due to no progress for %d secs\n",
							wlc->pub->unit, __FUNCTION__,
							tid, ini->dead_cnt));
						WLCNTINCR(ampdu_tx->cnt->txstuck);
						AMPDUSCBCNTINCR(scb_ampdu->cnt.txstuck);
						/* AMPDUXXX: unknown failure, send delba */
						wlc_ampdu_tx_send_delba(ampdu_tx, scb, tid,
							TRUE, DOT11_RC_TIMEOUT);
						ampdu_dump_ini(ini);
						ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
					}
				}
				break;

			case AMPDU_TID_STATE_BA_PENDING_ON:
				/* resend addba if not enough retries */
				if (ini->addba_req_cnt++ >= AMPDU_ADDBA_REQ_RETRY_CNT) {
					ampdu_ba_state_off(scb_ampdu, ini);
				} else {
#ifdef MFP
					if (SCB_AUTHORIZED(scb))
#endif
					{
						WL_AMPDU_ERR(("%s: addba timed out\n",
							__FUNCTION__));
						wlc_send_addba_req(wlc, scb, tid,
							ampdu_tx->ba_tx_wsize, ampdu_tx->ba_policy,
							ampdu_tx->delba_timeout);
					}

					WLCNTINCR(ampdu_tx->cnt->txaddbareq);
					AMPDUSCBCNTINCR(scb_ampdu->cnt.txaddbareq);
				}
				break;

			case AMPDU_TID_STATE_BA_PENDING_OFF:
				if (ini->cleanup_ini_cnt++ >= AMPDU_INI_CLEANUP_TIMEOUT) {
					WL_ERROR(("wl%d: %s: cleaning up tid %d "
						"from poff\n", wlc->pub->unit, __FUNCTION__, tid));
					WLCNTINCR(ampdu_tx->cnt->txstuck);
					AMPDUSCBCNTINCR(scb_ampdu->cnt.txstuck);
					ampdu_dump_ini(ini);
					ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, TRUE);
				} else
					ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, FALSE);
				break;

			case AMPDU_TID_STATE_BA_OFF:
				if (ini->off_cnt++ >= AMPDU_INI_OFF_TIMEOUT) {
					ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu,
						tid, FALSE);
					/* make a single attempt only */
					if (ini)
						ini->addba_req_cnt = AMPDU_ADDBA_REQ_RETRY_CNT;
				}
				break;
			default:
				break;
			}
			/* dont do anything with ini here since it might be freed */

		}
	}

	if (ampdu_tx->fb_override_enable && any_ht_cap && (!any_ba_state_on)) {
		ampdu_tx->fb_override = TRUE;
	} else {
		ampdu_tx->fb_override = FALSE;
	}

	return 0;
}

/* handle AMPDU related iovars */
static int
wlc_ampdu_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)hdl;
	int32 int_val = 0;
	int32 *ret_int_ptr = (int32 *) a;
	bool bool_val;
	int err = 0;
	wlc_info_t *wlc;
	wlc_bsscfg_t *bsscfg;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	wlc = ampdu_tx->wlc;
	ASSERT(ampdu_tx == wlc->ampdu_tx);

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_AMPDU_TX):
		*ret_int_ptr = (int32)wlc->pub->_ampdu_tx;
		break;

	case IOV_SVAL(IOV_AMPDU_TX):
		return wlc_ampdu_tx_set(ampdu_tx, (bool)int_val);

	case IOV_GVAL(IOV_AMPDU_TID): {
		struct ampdu_tid_control *ampdu_tid = (struct ampdu_tid_control *)p;

		if (ampdu_tid->tid >= AMPDU_MAX_SCB_TID) {
			err = BCME_BADARG;
			break;
		}
		ampdu_tid->enable = ampdu_tx->ini_enable[ampdu_tid->tid];
		bcopy(ampdu_tid, a, sizeof(*ampdu_tid));
		break;
		}

	case IOV_SVAL(IOV_AMPDU_TID): {
		struct ampdu_tid_control *ampdu_tid = (struct ampdu_tid_control *)a;

		if (ampdu_tid->tid >= AMPDU_MAX_SCB_TID) {
			err = BCME_BADARG;
			break;
		}
		ampdu_tx->ini_enable[ampdu_tid->tid] = ampdu_tid->enable ? TRUE : FALSE;
		break;
		}

	case IOV_GVAL(IOV_AMPDU_TX_DENSITY):
		*ret_int_ptr = (int32)ampdu_tx->mpdu_density;
		break;

	case IOV_SVAL(IOV_AMPDU_TX_DENSITY):
		if (int_val > AMPDU_MAX_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}

		if (int_val < AMPDU_DEF_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}
		ampdu_tx->mpdu_density = (uint8)int_val;
		break;

	case IOV_SVAL(IOV_AMPDU_SEND_ADDBA):
	{
		struct ampdu_ea_tid *ea_tid = (struct ampdu_ea_tid *)a;
		struct scb *scb;
		scb_ampdu_tx_t *scb_ampdu;

		if (!AMPDU_ENAB(wlc->pub) || (ea_tid->tid >= AMPDU_MAX_SCB_TID)) {
			err = BCME_BADARG;
			break;
		}

		if (!(scb = wlc_scbfind(wlc, bsscfg, &ea_tid->ea))) {
			err = BCME_NOTFOUND;
			break;
		}

		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
		if (!scb_ampdu || !SCB_AMPDU(scb)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (scb_ampdu->ini[ea_tid->tid]) {
			err = BCME_NOTREADY;
			break;
		}

		if (!wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, ea_tid->tid, TRUE)) {
			err = BCME_ERROR;
			break;
		}

		break;
	}

	case IOV_SVAL(IOV_AMPDU_SEND_DELBA):
	{
		struct ampdu_ea_tid *ea_tid = (struct ampdu_ea_tid *)a;
		struct scb *scb;
		scb_ampdu_tx_t *scb_ampdu;

		if (!AMPDU_ENAB(wlc->pub) || (ea_tid->tid >= AMPDU_MAX_SCB_TID)) {
			err = BCME_BADARG;
			break;
		}

		if (!(scb = wlc_scbfind(wlc, bsscfg, &ea_tid->ea))) {
			err = BCME_NOTFOUND;
			break;
		}

		scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
		if (!scb_ampdu || !SCB_AMPDU(scb)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		wlc_ampdu_tx_send_delba(ampdu_tx, scb, ea_tid->tid, TRUE,
			DOT11_RC_BAD_MECHANISM);

		break;
	}

	case IOV_GVAL(IOV_AMPDU_MANUAL_MODE):
		*ret_int_ptr = (int32)ampdu_tx->manual_mode;
		break;

	case IOV_SVAL(IOV_AMPDU_MANUAL_MODE):
		ampdu_tx->manual_mode = (uint8)int_val;
		break;

#ifdef WLOVERTHRUSTER
	case IOV_SVAL(IOV_AMPDU_TCP_ACK_RATIO):
		ampdu_tx->tcp_ack_info.tcp_ack_ratio = (uint8)int_val;
		break;
	case IOV_GVAL(IOV_AMPDU_TCP_ACK_RATIO):
		*ret_int_ptr = (uint32)ampdu_tx->tcp_ack_info.tcp_ack_ratio;
		break;
	case IOV_SVAL(IOV_AMPDU_TCP_ACK_RATIO_DEPTH):
		ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth = (uint8)int_val;
		break;
	case IOV_GVAL(IOV_AMPDU_TCP_ACK_RATIO_DEPTH):
		*ret_int_ptr = (uint32)ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth;
		break;
#endif

	case IOV_GVAL(IOV_AMPDU_MPDU):
		*ret_int_ptr = (int32)ampdu_tx->max_pdu;
		break;

	case IOV_SVAL(IOV_AMPDU_MPDU):
		if ((!int_val) || (int_val > AMPDU_MAX_MPDU)) {
			err = BCME_RANGE;
			break;
		}
		ampdu_tx->max_pdu = (int8)int_val;
		scb_ampdu_update_config_all(ampdu_tx);
		break;


#ifdef BCMDBG
#ifdef WLC_LOW
	case IOV_GVAL(IOV_AMPDU_FFPLD):
		wlc_ffpld_show(ampdu_tx);
		*ret_int_ptr = 0;
		break;
#endif
#endif /* BCMDBG */

#ifdef BCMDBG
#ifdef WLAMPDU_HW
	case IOV_GVAL(IOV_AMPDU_AGGFIFO):
		*ret_int_ptr = ampdu_tx->aggfifo_depth;
		break;
	case IOV_SVAL(IOV_AMPDU_AGGFIFO): {
		uint8 depth = (uint8)int_val;
		if (depth < 8 || depth > AGGFIFO_CAP) {
			WL_ERROR(("aggfifo depth has to be in [8, %d]\n", AGGFIFO_CAP));
			return BCME_BADARG;
		}
		ampdu_tx->aggfifo_depth = depth;
	}
		break;
#endif /* WLAMPDU_HW */
#endif /* BCMDBG */
	case IOV_GVAL(IOV_AMPDUMAC):
		*ret_int_ptr = (int32)wlc->pub->_ampdumac;
		break;

	case IOV_GVAL(IOV_AMPDU_AGGMODE):
		*ret_int_ptr = (int32)ampdu_tx->ampdu_aggmode;
		break;

	case IOV_SVAL(IOV_AMPDU_AGGMODE):
		if (int_val != AMPDU_AGG_AUTO &&
		    int_val != AMPDU_AGGMODE_HOST &&
		    int_val != AMPDU_AGGMODE_MAC) {
			err = BCME_RANGE;
			break;
		}
		ampdu_tx->ampdu_aggmode = (int8)int_val;
		wlc_ampdu_tx_set(ampdu_tx, wlc->pub->_ampdu_tx);
		break;

#ifdef WLMEDIA_TXFAILEVENT
	case IOV_GVAL(IOV_AMPDU_TXFAIL_EVENT):
		*ret_int_ptr = (int32)ampdu_tx->tx_fail_event;
		break;

	case IOV_SVAL(IOV_AMPDU_TXFAIL_EVENT):
		ampdu_tx->tx_fail_event = (bool)int_val;
		break;
#endif /* WLMEDIA_TXFAILEVENT */

	case IOV_GVAL(IOV_FRAMEBURST_OVR):
		*ret_int_ptr = ampdu_tx->fb_override_enable;
		break;

	case IOV_SVAL(IOV_FRAMEBURST_OVR):
		ampdu_tx->fb_override_enable = bool_val;
		break;

#ifdef BCMDBG
	case IOV_SVAL(IOV_AMPDU_TXQ_PROFILING_START):
		/* one shot mode, stop once circular buffer is full */
		wlc_ampdu_txq_prof_enab();
		break;

	case IOV_SVAL(IOV_AMPDU_TXQ_PROFILING_DUMP):
		wlc_ampdu_txq_prof_print_histogram(-1);
		break;

	case IOV_SVAL(IOV_AMPDU_TXQ_PROFILING_SNAPSHOT): {
		int tmp = _txq_prof_enab;
		WLC_AMPDU_TXQ_PROF_ADD_ENTRY(ampdu_tx->wlc, NULL);
		wlc_ampdu_txq_prof_print_histogram(1);
		_txq_prof_enab = tmp;
	}
		break;
#endif /* BCMDBG */
	case IOV_GVAL(IOV_AMPDU_RELEASE):
		*ret_int_ptr = (uint32)ampdu_tx->release;
		break;

	case IOV_SVAL(IOV_AMPDU_RELEASE):
		ampdu_tx->release = (int8)int_val;
		break;
	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

void
wlc_ampdu_tx_set_mpdu_density(ampdu_tx_info_t *ampdu_tx, uint8 mpdu_density)
{
	ampdu_tx->mpdu_density = mpdu_density;
}

void
wlc_ampdu_tx_set_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx, uint8 wsize)
{
	ampdu_tx->ba_tx_wsize = wsize;
}

uint8
wlc_ampdu_tx_get_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx)
{
	return (ampdu_tx->ba_tx_wsize);
}

uint8
wlc_ampdu_tx_get_ba_max_tx_wsize(ampdu_tx_info_t *ampdu_tx)
{
	return (ampdu_tx->ba_max_tx_wsize);
}

static void
wlc_ffpld_init(ampdu_tx_info_t *ampdu_tx)
{
	int i, j;
	wlc_fifo_info_t *fifo;

	for (j = 0; j < NUM_FFPLD_FIFO; j++) {
		fifo = (ampdu_tx->fifo_tb + j);
		fifo->ampdu_pld_size = 0;
		for (i = 0; i <= AMPDU_MAX_MCS; i++)
			fifo->mcs2ampdu_table[i] = 255;
		fifo->dmaxferrate = 0;
		fifo->accum_txampdu = 0;
		fifo->prev_txfunfl = 0;
		fifo->accum_txfunfl = 0;

	}

}

/* evaluate the dma transfer rate using the tx underflows as feedback.
 * If necessary, increase tx fifo preloading. If not enough,
 * decrease maximum ampdu_tx size for each mcs till underflows stop
 * Return 1 if pre-loading not active, -1 if not an underflow event,
 * 0 if pre-loading module took care of the event.
 */
static int
wlc_ffpld_check_txfunfl(wlc_info_t *wlc, int fid)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	uint32 phy_rate = MCS_RATE(AMPDU_MAX_MCS, TRUE, FALSE);
	uint32 txunfl_ratio;
	uint8  max_mpdu;
	uint32 current_ampdu_cnt = 0;
	uint16 max_pld_size;
	uint32 new_txunfl;
	wlc_fifo_info_t *fifo = (ampdu_tx->fifo_tb + fid);
	uint xmtfifo_sz;
	uint16 cur_txunfl;

	/* return if we got here for a different reason than underflows */
	cur_txunfl = wlc_read_shm(wlc, M_UCODE_MACSTAT + OFFSETOF(macstat_t, txfunfl[fid]));
	new_txunfl = (uint16)(cur_txunfl - fifo->prev_txfunfl);
	if (new_txunfl == 0) {
		WL_FFPLD(("check_txunfl : TX status FRAG set but no tx underflows\n"));
		return -1;
	}
	fifo->prev_txfunfl = cur_txunfl;

	if (!ampdu_tx->tx_max_funl)
		return 1;

	/* check if fifo is big enough */
	xmtfifo_sz = wlc->xmtfifo_szh[fid];

	if ((TXFIFO_SIZE_UNIT * (uint32)xmtfifo_sz) <= ampdu_tx->ffpld_rsvd)
		return 1;

	max_pld_size = TXFIFO_SIZE_UNIT * xmtfifo_sz - ampdu_tx->ffpld_rsvd;
#ifdef WLCNT
#ifdef WLAMPDU_MAC
	if (AMPDU_MAC_ENAB(wlc->pub) && !AMPDU_AQM_ENAB(wlc->pub))
		ampdu_tx->cnt->txampdu = wlc_read_shm(wlc, M_TXAMPDU_CNT);
#endif
	current_ampdu_cnt = ampdu_tx->cnt->txampdu - fifo->prev_txampdu;
#endif /* WLCNT */
	fifo->accum_txfunfl += new_txunfl;

	/* we need to wait for at least 10 underflows */
	if (fifo->accum_txfunfl < 10)
		return 0;

	WL_FFPLD(("ampdu_count %d  tx_underflows %d\n",
		current_ampdu_cnt, fifo->accum_txfunfl));

	/*
	   compute the current ratio of tx unfl per ampdu.
	   When the current ampdu count becomes too
	   big while the ratio remains small, we reset
	   the current count in order to not
	   introduce too big of a latency in detecting a
	   large amount of tx underflows later.
	*/

	txunfl_ratio = current_ampdu_cnt/fifo->accum_txfunfl;

	if (txunfl_ratio > ampdu_tx->tx_max_funl) {
		if (current_ampdu_cnt >= FFPLD_MAX_AMPDU_CNT) {
#ifdef WLCNT
			fifo->prev_txampdu = ampdu_tx->cnt->txampdu;
#endif
			fifo->accum_txfunfl = 0;
		}
		return 0;
	}
	max_mpdu = MIN(fifo->mcs2ampdu_table[AMPDU_MAX_MCS], ampdu_tx->default_pdu);

	/* In case max value max_pdu is already lower than
	   the fifo depth, there is nothing more we can do.
	*/

	if (fifo->ampdu_pld_size >= max_mpdu * FFPLD_MPDU_SIZE) {
		WL_FFPLD(("tx fifo pld : max ampdu fits in fifo\n)"));
#ifdef WLCNT
		fifo->prev_txampdu = ampdu_tx->cnt->txampdu;
#endif
		fifo->accum_txfunfl = 0;
		return 0;
	}

	if (fifo->ampdu_pld_size < max_pld_size) {

		/* increment by TX_FIFO_PLD_INC bytes */
		fifo->ampdu_pld_size += FFPLD_PLD_INCR;
		if (fifo->ampdu_pld_size > max_pld_size)
			fifo->ampdu_pld_size = max_pld_size;

		/* update scb release size */
		scb_ampdu_update_config_all(ampdu_tx);

		/*
		   compute a new dma xfer rate for max_mpdu @ max mcs.
		   This is the minimum dma rate that
		   can acheive no unferflow condition for the current mpdu size.
		*/
		/* note : we divide/multiply by 100 to avoid integer overflows */
		fifo->dmaxferrate =
		        (((phy_rate/100)*(max_mpdu*FFPLD_MPDU_SIZE-fifo->ampdu_pld_size))
		         /(max_mpdu * FFPLD_MPDU_SIZE))*100;

		WL_FFPLD(("DMA estimated transfer rate %d; pre-load size %d\n",
		          fifo->dmaxferrate, fifo->ampdu_pld_size));
	} else {

		/* decrease ampdu size */
		if (fifo->mcs2ampdu_table[AMPDU_MAX_MCS] > 1) {
			if (fifo->mcs2ampdu_table[AMPDU_MAX_MCS] == 255)
				fifo->mcs2ampdu_table[AMPDU_MAX_MCS] = ampdu_tx->default_pdu - 1;
			else
				fifo->mcs2ampdu_table[AMPDU_MAX_MCS] -= 1;

			/* recompute the table */
			wlc_ffpld_calc_mcs2ampdu_table(ampdu_tx, fid);

			/* update scb release size */
			scb_ampdu_update_config_all(ampdu_tx);
		}
	}
#ifdef WLCNT
	fifo->prev_txampdu = ampdu_tx->cnt->txampdu;
#endif
	fifo->accum_txfunfl = 0;
	return 0;
}

static void
wlc_ffpld_calc_mcs2ampdu_table(ampdu_tx_info_t *ampdu_tx, int f)
{
	int i;
	uint32 phy_rate, dma_rate, tmp;
	uint8 max_mpdu;
	wlc_fifo_info_t *fifo = (ampdu_tx->fifo_tb + f);

	/* recompute the dma rate */
	/* note : we divide/multiply by 100 to avoid integer overflows */
	max_mpdu = MIN(fifo->mcs2ampdu_table[AMPDU_MAX_MCS], ampdu_tx->default_pdu);
	phy_rate = MCS_RATE(AMPDU_MAX_MCS, TRUE, FALSE);
	dma_rate = (((phy_rate/100) * (max_mpdu*FFPLD_MPDU_SIZE-fifo->ampdu_pld_size))
	         /(max_mpdu*FFPLD_MPDU_SIZE))*100;
	fifo->dmaxferrate = dma_rate;

	/* fill up the mcs2ampdu table; do not recalc the last mcs */
	dma_rate = dma_rate >> 7;
	for (i = 0; i < AMPDU_MAX_MCS; i++) {
		/* shifting to keep it within integer range */
		phy_rate = MCS_RATE(i, TRUE, FALSE) >> 7;
		if (phy_rate > dma_rate) {
			tmp = ((fifo->ampdu_pld_size * phy_rate) /
				((phy_rate - dma_rate) * FFPLD_MPDU_SIZE)) + 1;
			tmp = MIN(tmp, 255);
			fifo->mcs2ampdu_table[i] = (uint8)tmp;
		}
	}

#ifdef BCMDBG
	wlc_ffpld_show(ampdu_tx);
#endif /* BCMDBG */
}

#ifdef BCMDBG
static void
wlc_ffpld_show(ampdu_tx_info_t *ampdu_tx)
{
	int i, j;
	wlc_fifo_info_t *fifo;

	WL_ERROR(("MCS to AMPDU tables:\n"));
	for (j = 0; j < NUM_FFPLD_FIFO; j++) {
		fifo = ampdu_tx->fifo_tb + j;
		WL_ERROR(("  FIFO %d : Preload settings: size %d dmarate %d kbps\n",
		          j, fifo->ampdu_pld_size, fifo->dmaxferrate));
		for (i = 0; i <= AMPDU_MAX_MCS; i++) {
			WL_ERROR(("  %d", fifo->mcs2ampdu_table[i]));
		}
		WL_ERROR(("\n\n"));
	}
}
#endif /* BCMDBG */


#if defined(DONGLEBUILD)
static void
wlc_ampdu_txflowcontrol(wlc_info_t *wlc, scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini)
{
	wlc_txq_info_t *qi = SCB_WLCIFP(scb_ampdu->scb)->qi;

	if (wlc_txflowcontrol_override_isset(wlc, qi, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL)) {
		if ((pktq_len(&scb_ampdu->txq) + ini->tx_in_transit) <=
		    wlc->pub->tunables->ampdudatahiwat) {
			wlc_txflowcontrol_override(wlc, qi, OFF, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL);
		}
	} else {
		if ((pktq_len(&scb_ampdu->txq) + ini->tx_in_transit) >=
		    wlc->pub->tunables->ampdudatahiwat) {
			wlc_txflowcontrol_override(wlc, qi, ON, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL);
		}
	}
}
#endif	/* DONGLEBUILD */

void
BCMATTACHFN(wlc_ampdu_agg_state_txaggr_override)(ampdu_tx_info_t *ampdu_tx, int8 txaggr)
{
	ampdu_tx->txaggr_override = txaggr;
	if (txaggr != AUTO)
		ampdu_tx->txaggr = txaggr;
}

/* Changes txaggr/rxaggr global states
	currently supported values:
	txaggr 	rxaggr
	ON		ON
	OFF		OFF
	ON		OFF
		XXXOthers are NOT TESTED
 */
void
wlc_ampdu_agg_state_update_tx(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool txaggr)
{
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;

	if (ampdu_tx->txaggr_override != AUTO)
		return;

	if (ampdu_tx->txaggr != txaggr) {
		if (txaggr == OFF) {
			wlc_ampdu_tx_cleanup(wlc, bsscfg, ampdu_tx);
		}
		ampdu_tx->txaggr = txaggr;
	}
}

#ifndef PKTC
/* AMPDU aggregation function called through txmod */
static void BCMFASTPATH
wlc_ampdu_agg(void *ctx, struct scb *scb, void *p, uint prec)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)ctx;
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc;
	scb_ampdu_tid_ini_t *ini;
	uint8 tid;

	ASSERT(ampdu_tx);
	wlc = ampdu_tx->wlc;

	if ((ampdu_tx->txaggr == OFF) ||
		(WLPKTTAG(p)->flags2 & WLF2_TDLS_BYPASS_AMPDU)) {
		WL_AMPDU(("wlc_ampdu_agg: tx agg off -- returning\n"));
		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
		return;
	}

	tid = (uint8)PKTPRIO(p);
	ASSERT(tid < AMPDU_MAX_SCB_TID);
	if (tid >= AMPDU_MAX_SCB_TID) {
		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
		WL_AMPDU(("wlc_ampdu_agg: tid wrong -- returning\n"));
		return;
	}

	ASSERT(SCB_AMPDU(scb));
	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* initialize initiator on first packet; sends addba req */
	if (!(ini = scb_ampdu->ini[tid])) {
		ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, tid, FALSE);
		if (!ini) {
			SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
			WL_AMPDU(("wlc_ampdu_agg: ini NULL -- returning\n"));
			return;
		}
	}

	if (ini->ba_state != AMPDU_TID_STATE_BA_ON) {
		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
		return;
	}

#ifdef WLOVERTHRUSTER
	if (PKTLEN(wlc->osh, p) <= (ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN + 120))
		wlc_ampdu_tcp_ack_suppress(ampdu_tx, scb_ampdu, p, tid);
#endif

	if (wlc_ampdu_prec_enq(ampdu_tx->wlc, &scb_ampdu->txq, p, tid)) {
		wlc_ampdu_txflowcontrol(wlc, scb_ampdu, ini);
		wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, FALSE);
		return;
	}

	WL_AMPDU_ERR(("wl%d: wlc_ampdu_agg: txq overflow\n", wlc->pub->unit));

	PKTFREE(wlc->osh, p, TRUE);
	WLCNTINCR(wlc->pub->_cnt->txnobuf);
	WLCNTINCR(ampdu_tx->cnt->txdrop);
	AMPDUSCBCNTINCR(scb_ampdu->cnt.txdrop);
	WLCIFCNTINCR(scb, txnobuf);
	WLCNTSCBINCR(scb->scb_stats.tx_failures);
}
#else /* PKTC */
/* AMPDU aggregation function called through txmod */
static void BCMFASTPATH
wlc_ampdu_agg(void *ctx, struct scb *scb, void *p, uint prec)
{
	ampdu_tx_info_t *ampdu_tx = (ampdu_tx_info_t *)ctx;
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc;
	scb_ampdu_tid_ini_t *ini;
	uint8 tid;
	void *n;
	uint32 lifetime = 0;
	bool amsdu_in_ampdu;
	int chained_pkts;

	ASSERT(ampdu_tx);
	wlc = ampdu_tx->wlc;
	ASSERT((PKTNEXT(wlc->osh, p) == NULL) || !PKTISCHAINED(p));
	tid = (uint8)PKTPRIO(p);

	/* Set packet exptime */
	if (!(WLPKTTAG(p)->flags & WLF_EXPTIME))
		lifetime = wlc->lifetime[(SCB_WME(scb) ? WME_PRIO2AC(tid) : AC_BE)];

	if ((ampdu_tx->txaggr == OFF) ||
		(WLPKTTAG(p)->flags2 & WLF2_TDLS_BYPASS_AMPDU)) {
		WL_AMPDU(("wlc_ampdu_agg: tx agg off -- returning\n"));
		goto txmod_ampdu;
	}

	ASSERT(tid < AMPDU_MAX_SCB_TID);
	if (tid >= AMPDU_MAX_SCB_TID) {
		WL_AMPDU(("wlc_ampdu_agg: tid wrong -- returning\n"));
		goto txmod_ampdu;
	}

	ASSERT(SCB_AMPDU(scb));
	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	/* initialize initiator on first packet; sends addba req */
	if (!(ini = scb_ampdu->ini[tid])) {
		ini = wlc_ampdu_init_tid_ini(ampdu_tx, scb_ampdu, tid, FALSE);
		if (!ini) {
			WL_AMPDU(("wlc_ampdu_agg: ini NULL -- returning\n"));
			goto txmod_ampdu;
		}
	}

	if (ini->ba_state != AMPDU_TID_STATE_BA_ON)
		goto txmod_ampdu;

	amsdu_in_ampdu = ((SCB_AMSDU_IN_AMPDU(scb) != 0) &
	                  (SCB_AMSDU(scb) != 0) &
#ifdef WLCNTSCB
	                  RSPEC_ISVHT(scb->scb_stats.tx_rate) &
#else
	                  FALSE &
#endif
	                  TRUE);

	chained_pkts = 0;
	while (1) {
		ASSERT(PKTISCHAINED(p) || (PKTCLINK(p) == NULL));
		n = PKTCLINK(p);
		PKTSETCLINK(p, NULL);
#ifdef WLOVERTHRUSTER
		/* Avoid fn call if the packet is too long to be an ACK */
		if (PKTLEN(wlc->osh, p) <= TCPACKSZSDU)
			wlc_ampdu_tcp_ack_suppress(ampdu_tx, scb_ampdu, p, tid);
#endif

		/* Queue could overflow while walking the chain */
		if (!wlc_ampdu_prec_enq(wlc, &scb_ampdu->txq, p, tid)) {
			PKTSETCLINK(p, n);
			wlc_ampdu_txflowcontrol(wlc, scb_ampdu, ini);
			wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, FALSE);
			break;
		}

		PKTCLRCHAINED(wlc->osh, p);

		chained_pkts ++;
		if (n == NULL) {
				WLCNTSET(wlc->pub->_cnt->txcurrchainsz, chained_pkts);
				if (wlc->pub->_cnt->txcurrchainsz > wlc->pub->_cnt->maxtxchainsz) 
					WLCNTSET(wlc->pub->_cnt->maxtxchainsz, wlc->pub->_cnt->txcurrchainsz);
		}
		
		/* Last packet in chain */
		if ((n == NULL) ||
		    (wlc_pktc_sdu_prep(wlc, scb, p, n, lifetime), (amsdu_in_ampdu &&
#ifdef WLOVERTHRUSTER
		    (PKTLEN(wlc->osh, n) > TCPACKSZSDU) &&
#endif
		    ((n = wlc_amsdu_pktc_agg(wlc->ami, scb, p, n, tid, lifetime)) == NULL)))) {
			wlc_ampdu_txflowcontrol(wlc, scb_ampdu, ini);
			wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, FALSE);
			return;
		}
		p = n;
	}

	WL_AMPDU_ERR(("wl%d: wlc_ampdu_agg: txq overflow\n", wlc->pub->unit));
//	printk("wl%d: wlc_ampdu_agg: txq overflow\n", wlc->pub->unit);

	FOREACH_CHAINED_PKT(p, n) {
		PKTCLRCHAINED(wlc->osh, p);
		PKTFREE(wlc->osh, p, TRUE);
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		WLCNTINCR(ampdu_tx->cnt->txdrop);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.txdrop);
		WLCIFCNTINCR(scb, txnobuf);
		WLCNTSCBINCR(scb->scb_stats.tx_failures);
	}

	return;

txmod_ampdu:
	FOREACH_CHAINED_PKT(p, n) {
		PKTCLRCHAINED(wlc->osh, p);
		if (n != NULL)
			wlc_pktc_sdu_prep(wlc, scb, p, n, lifetime);
		SCB_TX_NEXT(TXMOD_AMPDU, scb, p, prec);
	}
}
#endif /* PKTC */

#ifdef WLOVERTHRUSTER
uint
wlc_ampdu_tx_get_tcp_ack_ratio(ampdu_tx_info_t *ampdu_tx)
{
	return (uint)ampdu_tx->tcp_ack_info.tcp_ack_ratio;
}

static void BCMFASTPATH
wlc_ampdu_tcp_ack_suppress(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
                           void *p, uint8 tid)
{
	wlc_tcp_ack_info_t *tcp_info = &(ampdu_tx->tcp_ack_info);
	uint8 *ip_header;
	uint8 *tcp_header;
	uint32 ack_num;
	void *prev_p;
	uint16 totlen;
	uint32 ip_hdr_len;
	uint32 tcp_hdr_len;
	uint32 pktlen;
	uint16 ethtype;
	uint8 prec;
	osl_t *osh = ampdu_tx->wlc->pub->osh;

	if (tcp_info->tcp_ack_ratio == 0)
		return;

	ethtype = wlc_sdu_etype(ampdu_tx->wlc, p);
	if (ethtype != ntoh16(ETHER_TYPE_IP))
		return;

	pktlen = pkttotlen(osh, p);

	ip_header = wlc_sdu_data(ampdu_tx->wlc, p);
	ip_hdr_len = 4*(ip_header[0] & 0x0f);

	if (pktlen < ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN + ip_hdr_len)
		return;

	if (ip_header[9] != 0x06)
		return;

	tcp_header = ip_header + ip_hdr_len;

#ifdef WLAMPDU_PRECEDENCE
	prec = WLC_PRIO_TO_PREC(tid);
#else
	prec = tid;
#endif

	if (tcp_header[13] & 0x10) {
		totlen =  (ip_header[3] & 0xff) + (ip_header[2] << 8);
		tcp_hdr_len = 4*((tcp_header[12] & 0xf0) >> 4);

		if (totlen ==  ip_hdr_len + tcp_hdr_len) {
			tcp_info->tcp_ack_total++;
			if (ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth) {
				WLPKTTAG(p)->flags2 |= WLF2_FAVORED;
#ifdef WLAMPDU_PRECEDENCE
				prec = WLC_PRIO_TO_HI_PREC(tid);
#endif
			}
			if (tcp_header[13] == 0x10) {
				WLPKTTAG(p)->flags2 |= WLF2_DATA_TCP_ACK;
				if (tcp_info->current_dequeued >= tcp_info->tcp_ack_ratio) {
					tcp_info->current_dequeued = 0;
					return;
				}
			}
		}
	}

	if (!(WLPKTTAG(p)->flags2 & WLF2_DATA_TCP_ACK)) {
		return;
	}

	ack_num = (tcp_header[8] << 24) + (tcp_header[9] << 16) +
	        (tcp_header[10] << 8) +  tcp_header[11];

	if ((prev_p = pktq_ppeek_tail(&scb_ampdu->txq, prec)) &&
		(WLPKTTAG(prev_p)->flags2 & WLF2_DATA_TCP_ACK)) {

		uint8 *prev_ip_hdr = wlc_sdu_data(ampdu_tx->wlc, prev_p);
		uint8 *prev_tcp_hdr = prev_ip_hdr + 4*(ip_header[0] & 0x0f);
		uint32 prev_ack_num = (prev_tcp_hdr[8] << 24) +
			(prev_tcp_hdr[9] << 16) +
			(prev_tcp_hdr[10] << 8) +  prev_tcp_hdr[11];

		if ((ack_num > prev_ack_num) &&
			!memcmp(prev_ip_hdr+12, ip_header+12, 8) &&
			!memcmp(prev_tcp_hdr, tcp_header, 4)) {
			prev_p = pktq_pdeq_tail(&scb_ampdu->txq, prec);
			PKTFREE(ampdu_tx->wlc->pub->osh, prev_p, TRUE);
			tcp_info->tcp_ack_dequeued++;
			tcp_info->current_dequeued++;
			return;
		}
	}

	if ((pktq_plen(&scb_ampdu->txq, prec) < ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth)) {
		int count = 0;
		void *previous_p = NULL;
		prev_p = pktq_ppeek(&scb_ampdu->txq, prec);

		while (prev_p && (count < ampdu_tx->tcp_ack_info.tcp_ack_ratio_depth)) {
			if ((WLPKTTAG(prev_p)->flags2 & WLF2_DATA_TCP_ACK)) {
				uint8 *prev_ip_hdr = wlc_sdu_data(ampdu_tx->wlc, prev_p);
				uint8 *prev_tcp_hdr = prev_ip_hdr +
					4*(ip_header[0] & 0x0f);
				uint32 prev_ack_num = (prev_tcp_hdr[8] << 24) +
					(prev_tcp_hdr[9] << 16) +
					(prev_tcp_hdr[10] << 8) +  prev_tcp_hdr[11];

				/* is it the same dest/src IP addr, port # etc ??
				 * IPs: compare 8 bytes at IP hdrs offset 12
				 * compare tcp hdrs for 8 bytes : includes both ports and
				 * sequence number.
				 */
				if ((ack_num > prev_ack_num) &&
					(!memcmp(prev_ip_hdr+12, ip_header+12, 8)) &&
					(!memcmp(prev_tcp_hdr, tcp_header, 4))) {
					if (!previous_p) {
						prev_p = pktq_pdeq(&scb_ampdu->txq, prec);
					} else {
						prev_p = pktq_pdeq_prev(&scb_ampdu->txq,
							prec, previous_p);
					}
					if (prev_p) {
						PKTFREE(ampdu_tx->wlc->pub->osh,
							prev_p, TRUE);
						tcp_info->tcp_ack_multi_dequeued++;
						tcp_info->current_dequeued++;
					}
					return;
				}
			}
			previous_p = prev_p;
			prev_p = PKTLINK(prev_p);
			count++;
		}
	}

	tcp_info->current_dequeued = 0;
}
#endif /* WLOVERTHRUSTER */

/* function which contains the smarts of when to release the pdu. Can be
 * called from various places
 * Returns TRUE if released else FALSE
 */
static bool BCMFASTPATH
wlc_ampdu_txeval(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, bool force)
{
	uint16 qlen, release;
	uint16 in_transit;
	wlc_txq_info_t *qi;
	struct pktq *q;
	uint16 wlc_txq_avail;

	ASSERT(scb_ampdu);
	ASSERT(ini);

	if (ini->ba_state != AMPDU_TID_STATE_BA_ON)
		return FALSE;

	qlen = wlc_ampdu_pktq_plen(&scb_ampdu->txq, ini->tid);
	if (qlen == 0)
		return FALSE;

	release = MIN(ini->rem_window, MIN(qlen, scb_ampdu->release));
	in_transit = ini->tx_in_transit;

	qi = SCB_WLCIFP((scb_ampdu->scb))->qi;
	q = &qi->q;
	wlc_txq_avail = pktq_pavail(q, WLC_PRIO_TO_PREC(ini->tid));

#ifdef WLAMPDU_MAC
	/*
	 * For AQM, limit the number of packets in transit simultaneously so that other
	 * stations get a chance to transmit as well.
	 */
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
#ifdef AP
		struct scb * scb = scb_ampdu->scb;
		bool ps_pretend_limit_transit = SCB_PS_PRETEND(scb);
#else
		const bool ps_pretend_limit_transit = FALSE;
#endif

		if (IS_SEQ_ADVANCED(ini->max_seq, ini->start_seq)) {
			in_transit = MODSUB_POW2(ini->max_seq, ini->start_seq, SEQNUM_MAX);
		} else {
			in_transit = 0;
		}

		ASSERT(in_transit < SEQNUM_MAX/2);

#ifdef AP
		/* scb->ps_pretend_start will be non-zero if ps pretend has occurred recently, it
		 * contains the TSF time for when ps pretend was last activated.
		 * In poor link conditions, there is a high chance that ps pretend will be
		 * re-triggered.
		 * Within a short time window following ps pretend, this code is trying to
		 * constrain the number of packets in transit and so avoid a large number of packets
		 * repetitively going between transmit and ps mode.
		 */
		if (!ps_pretend_limit_transit && scb->ps_pretend_start) {
			/* time elapsed in us since last pretend ps event */
			uint32 ps_pretend_time = R_REG(ampdu_tx->wlc->osh,
			                               &ampdu_tx->wlc->regs->tsf_timerlow) -
			                         scb->ps_pretend_start;


			/* arbitrary calculation, roughly we don't allow in_transit to be big
			 * when we recently have done a pretend ps. As time goes by, we gradually
			 * allow in_transit to increase. There could be some tuning to do for this.
			 * This is to prevent a large in_transit in the case where we are performing
			 * pretend ps regularly (of order every second or faster), as the time
			 * required to suppress a large number of packets in transit can cause
			 * the data path to choke.
			 * When pretend ps is currently active, we always prevent packet release.
			 */
			if (in_transit > (40 + (ps_pretend_time >> 13 /* divide 8192 */))) {
				ps_pretend_limit_transit = TRUE;
			}

			if ((ps_pretend_time >> 10 /* divide 1024 */) > 2000) {
				/* After 2 seconds (approx) elapsed since last ps pretend, reset
				 * entry time to avoid executing this limitation code until ps
				 * pretend next occurs.
				 */
				scb->ps_pretend_start = 0;
			}
		}
#endif /* AP */

		if (!force && ((in_transit > ampdu_tx->aqm_max_release[ini->tid]) ||
			(wlc_txq_avail <  MIN(AMPDU_AQM_RELEASE_MAX, (pktq_max(q) / 2))) ||
			ps_pretend_limit_transit)) {

			WL_AMPDU_TX(("wl%d: txeval: Stop Releasing %d in_transit %d tid %d "
				"wm %d rem_wdw %d qlen %d force %d txq_avail %d pps %d\n",
				ampdu_tx->wlc->pub->unit, release, in_transit,
				ini->tid, ampdu_tx->tx_rel_lowat, ini->rem_window, qlen,
				force, wlc_txq_avail, ps_pretend_limit_transit));
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txnoroom);
			release = 0;
		}
	}
#endif /* WLAMPDU_MAC */

	if (wlc_txq_avail <= release) {
	    /* change decision, as we are running out of space in common queue */
	    release = 0;
	    AMPDUSCBCNTINCR(scb_ampdu->cnt.txnoroom);
	}

	if (release == 0)
		return FALSE;

#ifdef WLOTAMPDU_FORCE_RELEASE
	if (PKTLEN(wlc->osh, pktq_ppeek_tail(&scb_ampdu->txq, ini->tid)) >
		WLOTAMPDU_FORCE_RELEASE) {
		force = TRUE;
	}
#endif

	/* release mpdus if any one of the following conditions are met */
	if ((in_transit < ampdu_tx->tx_rel_lowat) ||
	    (release == MIN(scb_ampdu->release, ini->ba_wsize)) ||
	    force || AMPDU_HW_ENAB(ampdu_tx->wlc->pub)) {

		if (in_transit < ampdu_tx->tx_rel_lowat)
			WLCNTADD(ampdu_tx->cnt->txrel_wm, release);
		if (release == scb_ampdu->release)
			WLCNTADD(ampdu_tx->cnt->txrel_size, release);

		WL_AMPDU_TX(("wl%d: wlc_ampdu_txeval: Releasing %d mpdus: in_transit %d tid %d "
			"wm %d rem_wdw %d qlen %d force %d start_seq %x, max_seq %x\n",
			ampdu_tx->wlc->pub->unit, release, in_transit,
			ini->tid, ampdu_tx->tx_rel_lowat, ini->rem_window, qlen, force,
			ini->start_seq, ini->max_seq));

		wlc_ampdu_release(ampdu_tx, scb_ampdu, ini, release);
#ifdef BCMDBG
		WLC_AMPDU_TXQ_PROF_ADD_ENTRY(ampdu_tx->wlc, scb_ampdu->scb);
#endif
		return TRUE;
	}

	WL_AMPDU_TX(("wl%d: wlc_ampdu_txeval: Cannot release: in_transit %d tid %d wm %d "
	             "rem_wdw %d qlen %d release %d\n",
	             ampdu_tx->wlc->pub->unit, in_transit, ini->tid, ampdu_tx->tx_rel_lowat,
	             ini->rem_window, qlen, release));

	return FALSE;
}

static void BCMFASTPATH
wlc_ampdu_release(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, uint16 release)
{
	uint16 seq = 0, delta;

	WL_AMPDU_TX(("%s start release %d\n", __FUNCTION__, release));

	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		seq = SCB_SEQNUM(ini->scb, ini->tid) & (SEQNUM_MAX - 1);
		delta = MODSUB_POW2(seq, MODADD_POW2(ini->max_seq, 1, SEQNUM_MAX), SEQNUM_MAX);

		/* do not release past window (seqnum based check). There might be pkts
		 * in txq which are not marked AMPDU so will use up a seq number
		 */
		if ((delta + release) > ini->rem_window) {
			WL_AMPDU_ERR(("wl%d: wlc_ampdu_release: cannot release %d"
				"(out of window)\n", ampdu_tx->wlc->pub->unit, release));
			return;
		}

		ini->rem_window -= (release + delta);
	}
	ini->tx_in_transit += release;
	wlc_ampdu_release_pkts_loop(ampdu_tx, scb_ampdu, ini, release);
}

static void BCMFASTPATH
wlc_ampdu_release_pkts_loop(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini, uint16 release)
{
	void* p;
	uint16 seq = 0, indx;
	bool aqm_no_bitmap = AMPDU_AQM_ENAB(ampdu_tx->wlc->pub);

#if defined(TINY_PKT_JOINING) && !(defined(DMATXRC) && !defined(DMATXRC_DISABLED))
	struct pktq tmp_q;
	bool tiny_join = (release > 1);
	if (tiny_join) {
	uint8 count = release;
	void *prev_p = NULL;
	pktq_init(&tmp_q, 1, AMPDU_PKTQ_LEN);
	while (count--) {
		p = wlc_ampdu_pktq_pdeq(&scb_ampdu->txq, ini->tid);
		if ((PKTLEN(ampdu_tx->wlc->osh, p) <= TCPACKSZSDU) &&
			(prev_p  != NULL)) {
			if (PKTTAILROOM(ampdu_tx->wlc->osh, prev_p) > 512) {
				void *new_pkt;
				new_pkt = osl_pktclone(ampdu_tx->wlc->osh,
					prev_p,
					PKTLEN(ampdu_tx->wlc->osh, prev_p) + 64,
					PKTTAILROOM(ampdu_tx->wlc->osh, prev_p)-64);
				if (new_pkt) {
					PKTPULL(ampdu_tx->wlc->osh, new_pkt, 192);
					bcopy(PKTDATA(ampdu_tx->wlc->osh, p),
						PKTDATA(ampdu_tx->wlc->osh, new_pkt),
						PKTLEN(ampdu_tx->wlc->osh, p));
					wlc_pkttag_info_move(ampdu_tx->wlc->pub, p, new_pkt);
					PKTSETLEN(ampdu_tx->wlc->osh,
						new_pkt,
						PKTLEN(ampdu_tx->wlc->osh, p));
					PKTSETNEXT(ampdu_tx->wlc->osh, new_pkt,
						PKTNEXT(ampdu_tx->wlc->osh, p));
					PKTFREE(ampdu_tx->wlc->osh, p, TRUE);
					p = new_pkt;
				}
			}
		}
		prev_p = p;
		pktq_penq(&tmp_q, 0, p);
	}
	}
#endif /* TINY_PKT_JOINING */

	for (; release != 0; release--) {
#if defined(TINY_PKT_JOINING) && !(defined(DMATXRC) && !defined(DMATXRC_DISABLED))
		if (tiny_join)
			p = pktq_pdeq(&tmp_q, 0);
		else
			p = wlc_ampdu_pktq_pdeq(&scb_ampdu->txq, ini->tid);
#else
		p = wlc_ampdu_pktq_pdeq(&scb_ampdu->txq, ini->tid);
#endif
		ASSERT(p != NULL);
		if (p == NULL) break;

		WL_TRACE(("%p now released to sendampdu q\n", p));
		ASSERT(PKTPRIO(p) == ini->tid);

		WLPKTTAG(p)->flags |= WLF_AMPDU_MPDU;

		/* assign seqnum and save in pkttag */
		seq = SCB_SEQNUM(ini->scb, ini->tid) & (SEQNUM_MAX - 1);
		SCB_SEQNUM(ini->scb, ini->tid)++;
		WLPKTTAG(p)->seq = seq;

		/* ASSERT(!isset(ini->ackpending, indx)); */
		/* ASSERT(ini->txretry[indx] == 0); */
		if (!aqm_no_bitmap) {
			indx = TX_SEQ_TO_INDEX(seq);
			setbit(ini->ackpending, indx);
		}
		ini->max_seq = seq;

		ASSERT(!PKTISCHAINED(p));
		SCB_TX_NEXT(TXMOD_AMPDU, ini->scb, p, WLC_PRIO_TO_PREC(ini->tid));
	}
	WL_AMPDU_TX(("%s release left %d\n", __FUNCTION__, release));
}

/* returns TRUE if receives the next expected sequence
 * protects against pkt frees in the txpath by others
 */
static bool BCMFASTPATH
ampdu_is_exp_seq(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini, uint16 seq, int* seq_changed)
{
#ifdef WLAMPDU_MAC
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub))
		return ampdu_is_exp_seq_aqm(ampdu_tx, ini, seq, seq_changed);
	else
#endif /* WLAMPDU_MAC */
		return ampdu_is_exp_seq_noaqm(ampdu_tx, ini, seq, seq_changed);

}

#ifdef WLAMPDU_MAC
static bool BCMFASTPATH
ampdu_is_exp_seq_aqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini,
	uint16 seq, int* seq_changed)
{
	uint16 offset;
#ifdef BCMDBG
	scb_ampdu_tx_t *scb_ampdu;
	ASSERT(ini);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb);
	ASSERT(scb_ampdu);
#endif

	if (ini->tx_exp_seq != seq) {
		*seq_changed = 1;
		offset = MODSUB_POW2(seq, ini->tx_exp_seq, SEQNUM_MAX);
		WL_AMPDU_ERR(("wl%d: r0hole: tx_exp_seq 0x%x, seq 0x%x\n",
			ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));

		/* pkts bypassed (or lagging) on way down */
		if (offset > (SEQNUM_MAX >> 1)) {
			WL_ERROR(("wl%d: rlag: tx_exp_seq 0x%x, seq 0x%x\n",
			ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));
			WLCNTINCR(ampdu_tx->cnt->txrlag);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.txrlag);
			return FALSE;
		}

		ini->barpending_seq = seq;
		wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
		WLCNTADD(ampdu_tx->cnt->txr0hole, offset);
	}
	ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
	return TRUE;
}
#endif /* WLAMPDU_MAC */

static bool BCMFASTPATH
ampdu_is_exp_seq_noaqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini,
	uint16 seq, int* seq_changed)
{
	uint16 txretry, offset, indx;
	uint i;
	bool found;
#ifdef BCMDBG
	scb_ampdu_tx_t *scb_ampdu;
	ASSERT(ini);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb);
	ASSERT(scb_ampdu);
#endif

	txretry = ini->txretry[TX_SEQ_TO_INDEX(seq)];
	if (txretry == 0) {
		if (ini->tx_exp_seq != seq) {
			*seq_changed = 1;
			offset = MODSUB_POW2(seq, ini->tx_exp_seq, SEQNUM_MAX);
			WL_AMPDU_ERR(("wl%d: r0hole: tx_exp_seq 0x%x, seq 0x%x\n",
				ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));

			/* pkts bypassed (or lagging) on way down */
			if (offset > (SEQNUM_MAX >> 1)) {
				WL_ERROR(("wl%d: rlag: tx_exp_seq 0x%x, seq 0x%x\n",
					ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));
				WLCNTINCR(ampdu_tx->cnt->txrlag);
				AMPDUSCBCNTINCR(scb_ampdu->cnt.txrlag);
				return FALSE;
			}

			if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >= ini->ba_wsize) {
				WL_ERROR(("wl%d: unexpected seq: start_seq 0x%x, seq 0x%x\n",
				ampdu_tx->wlc->pub->unit, ini->start_seq, seq));
				WLCNTINCR(ampdu_tx->cnt->txrlag);
				AMPDUSCBCNTINCR(scb_ampdu->cnt.txrlag);
				return FALSE;
			}

			WLCNTADD(ampdu_tx->cnt->txr0hole, offset);
			/* send bar to move the window */
			indx = TX_SEQ_TO_INDEX(ini->tx_exp_seq);
			for (i = 0; i < offset; i++, indx = NEXT_TX_INDEX(indx))
				setbit(ini->barpending, indx);
			wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
		}

		ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
	} else {
		if (ini->retry_cnt && (ini->retry_seq[ini->retry_head] == seq)) {
			ini->retry_seq[ini->retry_head] = 0;
			ini->retry_head = NEXT_TX_INDEX(ini->retry_head);
			ini->retry_cnt--;
			ASSERT(ini->retry_cnt <= ampdu_tx->ba_max_tx_wsize);
		} else {
			found = FALSE;
			indx = ini->retry_head;
			for (i = 0; i < ini->retry_cnt; i++, indx = NEXT_TX_INDEX(indx)) {
				if (ini->retry_seq[indx] == seq) {
					found = TRUE;
					break;
				}
			}
			if (!found) {
				WL_ERROR(("wl%d: rnlag: tx_exp_seq 0x%x, seq 0x%x\n",
					ampdu_tx->wlc->pub->unit, ini->tx_exp_seq, seq));
				WLCNTINCR(ampdu_tx->cnt->txrlag);
				AMPDUSCBCNTINCR(scb_ampdu->cnt.txrlag);
				return FALSE;
			} else {
				while (ini->retry_seq[ini->retry_head] != seq) {
					WL_AMPDU_ERR(("wl%d: rnhole: retry_seq 0x%x, seq 0x%x\n",
						ampdu_tx->wlc->pub->unit,
						ini->retry_seq[ini->retry_head], seq));
					setbit(ini->barpending,
						TX_SEQ_TO_INDEX(ini->retry_seq[ini->retry_head]));
					WLCNTINCR(ampdu_tx->cnt->txrnhole);
					ini->retry_seq[ini->retry_head] = 0;
					ini->retry_head = NEXT_TX_INDEX(ini->retry_head);
					ini->retry_cnt--;
				}
				wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
				ini->retry_seq[ini->retry_head] = 0;
				ini->retry_head = NEXT_TX_INDEX(ini->retry_head);
				ini->retry_cnt--;
				ASSERT(ini->retry_cnt < ampdu_tx->ba_max_tx_wsize);
			}
		}
	}

	/* discard the frame if it is a retry and we are in pending off state */
	if (txretry && (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF)) {
		setbit(ini->barpending, TX_SEQ_TO_INDEX(seq));
		return FALSE;
	}

	if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >= ini->ba_wsize) {
		ampdu_dump_ini(ini);
		ASSERT(MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) < ini->ba_wsize);
	}
	return TRUE;
}

#ifdef WLAMPDU_MAC
void
wlc_ampdu_change_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, int reason_code)
{
#ifdef BCMDBG
	const char *str = "Undefined";

	switch (reason_code) {
	case AMU_EPOCH_CHG_PLCP:
		str = "PLCP";
		WLCNTINCR(ampdu_tx->cnt->echgr1);
		break;
	case AMU_EPOCH_CHG_FID:
		WLCNTINCR(ampdu_tx->cnt->echgr2);
		str = "FRAMEID";
		break;
	case AMU_EPOCH_CHG_NAGG:
		WLCNTINCR(ampdu_tx->cnt->echgr3);
		str = "ampdu_tx OFF";
		break;
	case AMU_EPOCH_CHG_MPDU:
		WLCNTINCR(ampdu_tx->cnt->echgr4);
		str = "reg mpdu";
		break;
	case AMU_EPOCH_CHG_DSTTID:
		WLCNTINCR(ampdu_tx->cnt->echgr5);
		str = "DST/TID";
		break;
	case AMU_EPOCH_CHG_SEQ:
		WLCNTINCR(ampdu_tx->cnt->echgr6);
		str = "SEQ No";
		break;
	}

	WL_AMPDU_HWDBG(("wl%d: %s: fifo %d: change epoch for %s\n",
		ampdu_tx->wlc->pub->unit, __FUNCTION__, fifo, str));
#endif /* BCMDBG */
	ASSERT(fifo < AC_COUNT);
	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		ampdu_tx->hagg[fifo].change_epoch = TRUE;
	} else {
		ampdu_tx->hagg[fifo].epoch = (ampdu_tx->hagg[fifo].epoch) ? 0 : 1;
		WL_NONE(("wl%d: %s: fifo %d: change epoch for %s to %d\n",
		         ampdu_tx->wlc->pub->unit, __FUNCTION__, fifo, str,
		         ampdu_tx->hagg[fifo].epoch));
	}
}

static uint16
wlc_ampdu_calc_maxlen(ampdu_tx_info_t *ampdu_tx, uint8 plcp0, uint plcp3, uint32 txop)
{
	uint8 is40, sgi;
	uint8 mcs = 0;
	uint16 txoplen = 0;
	uint16 maxlen = 0xffff;

	is40 = (plcp0 & MIMO_PLCP_40MHZ) ? 1 : 0;
	sgi = PLCP3_ISSGI(plcp3) ? 1 : 0;
	mcs = plcp0 & ~MIMO_PLCP_40MHZ;
	ASSERT(mcs < MCS_TABLE_SIZE);

	if (txop) {
		/* rate is in Kbps; txop is in usec
		 * ==> len = (rate * txop) / (1000 * 8)
		 */
		txoplen = (MCS_RATE(mcs, is40, sgi) >> 10) * (txop >> 3);

		maxlen = MIN((txoplen),
			(ampdu_tx->max_txlen[mcs][is40][sgi] *
			ampdu_tx->ba_tx_wsize));
	}
	WL_AMPDU_HW(("wl%d: %s: txop %d txoplen %d maxlen %d\n",
		ampdu_tx->wlc->pub->unit, __FUNCTION__, txop, txoplen, maxlen));

	return maxlen;
}
#endif /* WLAMPDU_MAC */


#ifdef WLAMPDU_MAC
/*
 * Function: check whether prev_ft is 0 or not
 */
extern bool
wlc_ampdu_was_ampdu(ampdu_tx_info_t *ampdu_tx, int fifo)
{
	return (ampdu_tx->hagg[fifo].prev_ft != AMPDU_NONE);
}

/* change epoch and save epoch determining params
 * return updated epoch
 */
uint8
wlc_ampdu_chgnsav_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, int reason_code,
	struct scb *scb, uint8 tid, wlc_txh_info_t* txh_info)
{
	ampdumac_info_t *hagg;
	bool isAmpdu;
	wlc_ampdu_change_epoch(ampdu_tx, fifo, reason_code);

	isAmpdu = wlc_txh_get_isAMPDU(ampdu_tx->wlc, txh_info);

	hagg = &(ampdu_tx->hagg[fifo]);
	hagg->prev_scb = scb;
	hagg->prev_tid = tid;
	hagg->prev_ft = AMPDU_NONE;
	if (isAmpdu) {
		uint16 tmp;
		tmp = ltoh16(txh_info->PhyTxControlWord0) & D11AC_PHY_TXC_FT_MASK;
		ASSERT(tmp ==  D11AC_PHY_TXC_FT_11N ||
		       tmp == D11AC_PHY_TXC_FT_11AC);
		hagg->prev_ft = (tmp == D11AC_PHY_TXC_FT_11N) ? AMPDU_11N : AMPDU_11VHT;
	}
	hagg->prev_antsel = txh_info->w.asp.AntSel;
	hagg->prev_txpwr = txh_info->w.asp.TxPower;
	hagg->prev_plcp[0] = txh_info->plcpPtr[0];
	hagg->prev_plcp[3] = txh_info->plcpPtr[3];
	if (hagg->prev_ft == AMPDU_11VHT) {
		hagg->prev_plcp[1] = txh_info->plcpPtr[1];
		hagg->prev_plcp[2] = txh_info->plcpPtr[2];
		hagg->prev_plcp[4] = txh_info->plcpPtr[4];
	}
	WL_AMPDU_HWDBG(("%s fifo %d epoch %d\n", __FUNCTION__, fifo, hagg->epoch));

	return hagg->epoch;
}

/*
 * Function: check whether txparams have changed for ampdu tx.
 * txparmas include <frametype: 11n or 11vht>, <rate info in plcp>, <antsel>
 * Call this function *only* for aggreteable frames
 */
static bool
wlc_ampdu_epoch_params_chg(ampdumac_info_t *hagg,
	struct scb *scb, uint8 tid, wlc_txh_info_t* txh_info)
{
	bool chg;
	uint8 tmp, frametype;

	if (hagg->prev_scb != scb || hagg->prev_tid != tid)
		return TRUE;

	/* map phy dependent frametype to independent frametype enum */
	tmp = ltoh16(txh_info->PhyTxControlWord0) & D11AC_PHY_TXC_FT_MASK;
	if (tmp == D11AC_PHY_TXC_FT_11N)
		frametype = AMPDU_11N;
	else if (tmp == D11AC_PHY_TXC_FT_11AC)
		frametype = AMPDU_11VHT;
	else {
		ASSERT(0);
		return TRUE;
	}

	chg = (frametype != hagg->prev_ft) || (txh_info->w.asp.AntSel != hagg->prev_antsel) ||
		(txh_info->w.asp.TxPower != hagg->prev_txpwr);
	if (chg)
		return TRUE;

	if (frametype == hagg->prev_ft) {
		if (frametype == AMPDU_11N) {
			/* 11N frame: only care plcp0 and plcp3 */
			if (txh_info->plcpPtr[0] != hagg->prev_plcp[0] ||
			    txh_info->plcpPtr[3] != hagg->prev_plcp[3])
				chg = TRUE;
		} else if (frametype == AMPDU_11VHT) {
			/* 11AC frame: only care plcp0-4 */
			if (txh_info->plcpPtr[0] != hagg->prev_plcp[0] ||
			    txh_info->plcpPtr[1] != hagg->prev_plcp[1] ||
			    txh_info->plcpPtr[2] != hagg->prev_plcp[2] ||
			    txh_info->plcpPtr[3] != hagg->prev_plcp[3] ||
			    txh_info->plcpPtr[4] != hagg->prev_plcp[4])
				chg = TRUE;
		}
	}

	return chg;
}

typedef struct ampdu_creation_params
{
	wlc_bsscfg_t *cfg;
	scb_ampdu_tx_t *scb_ampdu;
	wlc_txh_info_t* tx_info;

	scb_ampdu_tid_ini_t *ini;
	ampdu_tx_info_t *ampdu;

} ampdu_create_info_t;

static bool
wlc_txs_was_acked(wlc_info_t *wlc, tx_status_macinfo_t *status, uint16 idx)
{
	uint32 mask = 0;

	ASSERT(idx < 64);
	/* Byte 4-11 of 2nd pkg */
	/* use :status->ack_map[idx/32]; compare div + indirection  <> branch + (sub)*50% */
	if (idx < 32) {
		mask = (1 << idx);
		return (status->ack_map1 & mask) ? TRUE : FALSE;
	} else {
		idx -= 32;
		mask = (1 << idx);
		return (status->ack_map2 & mask) ? TRUE : FALSE;
	}
}


static void
wlc_ampdu_fill_percache_info(wlc_info_t* wlc, ampdu_create_info_t* ampdu_cons_info)
{
	d11actxh_cache_t* cache_info = NULL;
	uint16 ampdu_dur;

	ASSERT(ampdu_cons_info);
	ASSERT(D11REV_GE(wlc->pub->corerev, 40));

	/* if short header, skip it - percache will be gotten from cache */
	if (ampdu_cons_info->tx_info->hdrSize != D11AC_TXH_LEN) {
		return;
	}

	cache_info = &(ampdu_cons_info->tx_info->hdrPtr->txd.CacheInfo);

	cache_info->PrimeMpduMax = ampdu_cons_info->scb_ampdu->max_pdu;
	cache_info->FallbackMpduMax = ampdu_cons_info->scb_ampdu->max_pdu;

	/* maximum duration is in 16 usec, putting into upper 12 bits
	 * ampdu density is in low 4 bits
	 */
	ampdu_dur = (ampdu_cons_info->ampdu->dur & D11AC_AMPDU_MAX_DUR_MASK);
	ampdu_dur |= (ampdu_cons_info->scb_ampdu->mpdu_density << D11AC_AMPDU_MIN_DUR_IDX_SHIFT);
	cache_info->AmpduDur = htol16(ampdu_dur);
	/* Fill in as BAWin of recvr -1 */
	cache_info->BAWin = ampdu_cons_info->ini->ba_wsize - 1;
}
#endif /* WLAMPDU_MAC */

/*
 * ampdu transmit routine
 * Returns BCM error code
 */
int BCMFASTPATH
wlc_sendampdu(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **pdu, int prec)
{
	wlc_info_t *wlc;
	osl_t *osh;
	void *p;
	struct scb *scb;
	wlc_bsscfg_t *cfg;
	int err = 0;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint8 tid;

	ASSERT(ampdu_tx);
	ASSERT(qi);
	ASSERT(pdu);

	wlc = ampdu_tx->wlc;
	osh = wlc->osh;
	p = *pdu;
	ASSERT(p);
	if (p == NULL)
		return err;
	scb = WLPKTTAGSCBGET(p);
	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);


	/* SCB setting may have changed during transition, so just discard the frame */
	if (!SCB_AMPDU(scb)) {
		/* ASSERT removed for 4360 as */
		/* the much longer tx pipeline (amsdu+ampdu) made it even more likely */
		/* to see this case */
		WLCNTINCR(ampdu_tx->cnt->orphan);
		PKTFREE(osh, p, TRUE);
		*pdu = NULL;
		WL_AMPDU(("wlc_sendampdu: entry func: scb ampdu flag null -- error\n"));
		return err;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);

	ASSERT(scb_ampdu);

	tid = (uint8)PKTPRIO(p);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	ini = scb_ampdu->ini[tid];

	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU(("wlc_sendampdu: bailing out for bad ini (%p)/bastate\n",
			ini));
		WLCNTINCR(ampdu_tx->cnt->orphan);
		if (ini)
			ini->tx_in_transit--;
		PKTFREE(osh, p, TRUE);
		*pdu = NULL;
		return err;
	}

	/* Something is blocking data packets */
	if (wlc->block_datafifo) {
		WL_AMPDU(("wlc_sendampdu: datafifo blocked\n"));
		return BCME_BUSY;
	}

#ifdef WLAMPDU_MAC
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		return _wlc_sendampdu_aqm(ampdu_tx, qi, pdu, prec);
	} else
#endif /* WLAMPDU_MAC */
	{
		return _wlc_sendampdu_noaqm(ampdu_tx, qi, pdu, prec);
	}
}

static int BCMFASTPATH
_wlc_sendampdu_noaqm(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **pdu, int prec)
{
	wlc_info_t *wlc;
	osl_t *osh;
	void *p;
#ifdef WLAMPDU_HW
	void *pkt[AGGFIFO_CAP];
#else
	void *pkt[AMPDU_MAX_MPDU];
#endif
	uint8 tid, ndelim;
	int err = 0, txretry = -1;
	uint8 preamble_type = WLC_GF_PREAMBLE;
	uint8 fbr_preamble_type = WLC_GF_PREAMBLE;
	uint8 rts_preamble_type = WLC_LONG_PREAMBLE;
	uint8 rts_fbr_preamble_type = WLC_LONG_PREAMBLE;

	bool rr = FALSE, fbr = FALSE, vrate_probe = FALSE;
	uint i, count = 0, fifo, npkts, nsegs, seg_cnt = 0;
	uint16 plen, len, seq, mcl, mch, indx, frameid, dma_len = 0;
	uint32 ampdu_len, maxlen = 0;
	d11txh_t *txh = NULL;
	uint8 *plcp;
	struct dot11_header *h;
	struct scb *scb;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint8 mcs = 0;
	bool regmpdu = FALSE;
	bool use_rts = FALSE, use_cts = FALSE;
	ratespec_t rspec = 0, rspec_fallback = 0;
	ratespec_t rts_rspec = 0, rts_rspec_fallback = 0;
	struct dot11_rts_frame *rts = NULL;
	uint8 rr_retry_limit;
	wlc_fifo_info_t *f;
	bool fbr_iscck, use_mfbr = FALSE;
	uint32 txop = 0;
	int seq_changed = 0;
#ifdef WLAMPDU_MAC
	ampdumac_info_t *hagg;
	int sc; /* Side channel index */
	uint aggfifo_space = 0;
#endif
	wlc_bsscfg_t *cfg;
	wlc_txh_info_t txh_info;

	wlc = ampdu_tx->wlc;
	osh = wlc->osh;
	p = *pdu;
	wlc_get_txh_info(wlc, p, &txh_info);

	tid = (uint8)PKTPRIO(p);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	f = ampdu_tx->fifo_tb + prio2fifo[tid];

	scb = WLPKTTAGSCBGET(p);
	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[tid];
	rr_retry_limit = ampdu_tx->rr_retry_limit_tid[tid];

	ASSERT(ini->scb == scb);

#ifdef WLAMPDU_MAC
	sc = prio2fifo[tid];
	hagg = &ampdu_tx->hagg[sc];

	/* agg-fifo space */
	if (AMPDU_MAC_ENAB(wlc->pub)) {
		aggfifo_space = get_aggfifo_space(ampdu_tx, sc);
		WL_AMPDU_HWDBG(("%s: aggfifo_space %d txfifo %d\n", __FUNCTION__, aggfifo_space,
			R_REG(wlc->osh, &wlc->regs->u.d11regs.xmtfifo_frame_cnt)));
#if (defined(BCMDBG) || defined(BCMDBG_AMPDU)) && defined(WLAMPDU_UCODE)
		ampdu_tx->amdbg->schan_depth_histo[MIN(AMPDU_MAX_MPDU, aggfifo_space)]++;
#endif
		if (aggfifo_space == 0) {
			return BCME_BUSY;
		}
		/* check if there are enough descriptors available */
		if (wlc->dma_avoidance_war)
			nsegs = pktsegcnt_war(osh, p);
		else
			nsegs = pktsegcnt(osh, p);
		if (TXAVAIL(wlc, sc) <= nsegs) {
			WLCNTINCR(ampdu_tx->cnt->txfifofull);
			return BCME_BUSY;
		}
	}
#endif /* WLAMPDU_MAC */

	/* compute txop once for all */
	txop = cfg->wme->edcf_txop[wme_fifo2ac[prio2fifo[tid]]];
	if (txop && wlc->txburst_limit)
		txop = MIN(txop, wlc->txburst_limit);
	else if (wlc->txburst_limit)
		txop = wlc->txburst_limit;

	ampdu_len = 0;
	dma_len = 0;
	while (p) {
		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);
		seq = WLPKTTAG(p)->seq;

		if (WLPKTTAG(p)->flags & WLF_MPDU)
			err = wlc_prep_pdu(wlc, p, &fifo);
		else
#ifdef PKTC
			if ((err = wlc_prep_sdu_fast(wlc, cfg, scb, p, &fifo)) == BCME_UNSUPPORTED)
#endif
			err = wlc_prep_sdu(wlc, &p, (int*)&npkts, &fifo);

		if (err) {
			if (err == BCME_BUSY) {
				WL_AMPDU_TX(("wl%d: wlc_sendampdu: prep_xdu retry; seq 0x%x\n",
					wlc->pub->unit, seq));
				WLCNTINCR(ampdu_tx->cnt->sduretry);
				*pdu = p;
				break;
			}

			/* error in the packet; reject it */
			WL_AMPDU_ERR(("wl%d: wlc_sendampdu: prep_xdu rejected; seq 0x%x\n",
				wlc->pub->unit, seq));
			WLCNTINCR(ampdu_tx->cnt->sdurejected);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.sdurejected);

			/* send bar since this may be blocking pkts in scb */
			if (ini->tx_exp_seq == seq) {
				setbit(ini->barpending, TX_SEQ_TO_INDEX(seq));
				wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
				ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
			}
			*pdu = NULL;
			break;
		}

		WL_AMPDU_TX(("wl%d: wlc_sendampdu: prep_xdu success; seq 0x%x tid %d\n",
			wlc->pub->unit, seq, tid));

		/* pkt is good to be aggregated */

		ASSERT(WLPKTTAG(p)->flags & WLF_MPDU);
		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

		txh = (d11txh_t *)PKTDATA(osh, p);
		plcp = (uint8 *)(txh + 1);
		h = (struct dot11_header*)(plcp + D11_PHY_HDR_LEN);
		seq = ltoh16(h->seq) >> SEQNUM_SHIFT;
		indx = TX_SEQ_TO_INDEX(seq);

		/* during roam we get pkts with null a1. kill it here else
		 * does not find scb on dotxstatus and things gets out of sync
		 */
		if (ETHER_ISNULLADDR(&h->a1)) {
			WL_AMPDU_ERR(("wl%d: sendampdu; dropping frame with null a1\n",
				wlc->pub->unit));
			WLCNTINCR(ampdu_tx->cnt->txdrop);
			if (ini->tx_exp_seq == seq) {
				setbit(ini->barpending, indx);
				wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
				ini->tx_exp_seq = MODINC_POW2(seq, SEQNUM_MAX);
			}
			PKTFREE(osh, p, TRUE);
			err = BCME_ERROR;
			*pdu = NULL;
			break;
		}

		/* If the packet was not retrieved from HW FIFO before attempting
		 * over the air for multi-queue
		 */
		if (!(WLPKTTAG(p)->flags & WLF_FIFOPKT) &&
		    !ampdu_is_exp_seq(ampdu_tx, ini, seq, &seq_changed)) {
			PKTFREE(osh, p, TRUE);
			err = BCME_ERROR;
			*pdu = NULL;
			break;
		}

		if (WLPKTTAG(p)->flags & WLF_FIFOPKT)
			WLPKTTAG(p)->flags &= ~WLF_FIFOPKT;

		if (!isset(ini->ackpending, indx)) {
			ampdu_dump_ini(ini);
			ASSERT(isset(ini->ackpending, indx));
		}

		/* check mcl fields and test whether it can be agg'd */
		mcl = ltoh16(txh->MacTxControlLow);
		mcl &= ~TXC_AMPDU_MASK;
		fbr_iscck = !(ltoh16(txh->XtraFrameTypes) & 0x3);
		txh->PreloadSize = 0; /* always default to 0 */

		/*  Handle retry limits */
		if (AMPDU_HOST_ENAB(wlc->pub)) {

			/* first packet in ampdu */
			if (txretry == -1) {
			    txretry = ini->txretry[indx];
			    vrate_probe = (WLPKTTAG(p)->flags & WLF_VRATE_PROBE) ? TRUE : FALSE;
			}

			ASSERT(txretry == ini->txretry[indx]);

			if ((txretry == 0) ||
			    (txretry < rr_retry_limit && !(vrate_probe && ampdu_tx->probe_mpdu))) {
				rr = TRUE;
				ASSERT(!fbr);
			} else {
				/* send as regular mpdu if not a mimo frame or in ps mode
				 * or we are in pending off state
				 * make sure the fallback rate can be only HT or CCK
				 */
				fbr = TRUE;
				ASSERT(!rr);

				regmpdu = fbr_iscck;
				if (regmpdu) {
					mcl &= ~(TXC_SENDRTS | TXC_SENDCTS | TXC_LONGFRAME);
					mcl |= TXC_STARTMSDU;
					txh->MacTxControlLow = htol16(mcl);

					mch = ltoh16(txh->MacTxControlHigh);
					mch |= TXC_AMPDU_FBR;
					txh->MacTxControlHigh = htol16(mch);
					break;
				}
			}
		} /* AMPDU_HOST_ENAB */

		/* extract the length info */
		len = fbr_iscck ? WLC_GET_CCK_PLCP_LEN(txh->FragPLCPFallback)
			: WLC_GET_MIMO_PLCP_LEN(txh->FragPLCPFallback);

		if (!(WLPKTTAG(p)->flags & WLF_MIMO) ||
		    (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF) ||
		    SCB_PS(scb)) {
			/* clear ampdu related settings if going as regular frame */
			if ((WLPKTTAG(p)->flags & WLF_MIMO) &&
			    (SCB_PS(scb) || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF))) {
				WLC_CLR_MIMO_PLCP_AMPDU(plcp);
				WLC_SET_MIMO_PLCP_LEN(plcp, len);
			}
			regmpdu = TRUE;
			mcl &= ~(TXC_SENDRTS | TXC_SENDCTS | TXC_LONGFRAME);
			txh->MacTxControlLow = htol16(mcl);
			break;
		}

		/* pkt is good to be aggregated */

		if (ini->txretry[indx]) {
			WLCNTINCR(ampdu_tx->cnt->u0.txretry_mpdu);
		}

		/* retrieve null delimiter count */
		ndelim = txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM];
		if (wlc->dma_avoidance_war)
			seg_cnt += pktsegcnt_war(osh, p);
		else
			seg_cnt += pktsegcnt(osh, p);

		if (!WL_AMPDU_HW_ON())
			WL_AMPDU_TX(("wl%d: wlc_sendampdu: mpdu %d plcp_len %d\n",
				wlc->pub->unit, count, len));

#ifdef WLAMPDU_MAC
		/*
		 * aggregateable mpdu. For ucode/hw agg,
		 * test whether need to break or change the epoch
		 */
		if (AMPDU_MAC_ENAB(wlc->pub)) {
			uint8 max_pdu;

			ASSERT((uint)sc == fifo);

			/* check if link or tid has changed */
			if (scb != hagg->prev_scb || tid != hagg->prev_tid) {
				hagg->prev_scb = scb;
				hagg->prev_tid = tid;
				wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_DSTTID);
			}

			/* mark every MPDU's plcp to indicate ampdu */
			WLC_SET_MIMO_PLCP_AMPDU(plcp);

			/* rate/sgi/bw/etc phy info */
			if (plcp[0] != hagg->prev_plcp[0] ||
			    (plcp[3] & 0xf0) != hagg->prev_plcp[1]) {
				hagg->prev_plcp[0] = plcp[0];
				hagg->prev_plcp[1] = plcp[3] & 0xf0;
				wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_PLCP);
			}
			/* frameid -- force change by the rate selection algo */
			frameid = ltoh16(txh->TxFrameID);
			if (frameid & TXFID_RATE_PROBE_MASK) {
				wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_FID);
				wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid, TRUE, 0);
			}

			/* Set bits 9:10 to any nonzero value to indicate to
			 * ucode that this packet can be aggregated
			 */
			mcl |= (TXC_AMPDU_FIRST << TXC_AMPDU_SHIFT);
			/* set sequence number in tx-desc */
			txh->AmpduSeqCtl = htol16(h->seq);

			/*
			 * compute aggregation parameters
			 */
			/* limit on number of mpdus per ampdu */
			mcs = plcp[0] & ~MIMO_PLCP_40MHZ;
			max_pdu = scb_ampdu->max_pdu;

			/* MaxAggSize in duration(usec) and bytes for hw/ucode agg, respectively */
			if (AMPDU_HW_ENAB(wlc->pub)) {
				/* use duration limit in usec */
				uint32 maxdur;
				maxdur = ampdu_tx->dur ? (uint32)ampdu_tx->dur : 0xffff;
				if (txop && (txop < maxdur))
					maxdur = txop;
				if ((maxdur & 0xffff0000) != 0)
					maxdur = 0xffff;
				txh->u1.MaxAggDur = htol16(maxdur & 0xffff);
				ASSERT(txh->u1.MaxAggDur != 0);
				/* hack for now :
				 * should compute from xmtfifo_sz[fifo] * 256 / max_bytes_per_mpdu
				 * e.g. fifo1: floor(255 * 256, 1650)
				 */
				txh->u2.MaxRNum = htol16(wlc->xmtfifo_frmmaxh[fifo]);
				ASSERT(txh->u2.MaxRNum > 0);
			} else {
				if (MCS_RATE(mcs, TRUE, FALSE) >= f->dmaxferrate) {
					uint16 preloadsize = f->ampdu_pld_size;
					if (D11REV_IS(wlc->pub->corerev, 29))
						preloadsize >>= 2;
					txh->PreloadSize = htol16(preloadsize);
					if (max_pdu > f->mcs2ampdu_table[mcs])
						max_pdu = f->mcs2ampdu_table[mcs];
				}
				txh->u1.MaxAggLen = htol16(wlc_ampdu_calc_maxlen(ampdu_tx,
					plcp[0], plcp[3], txop));
				if (!fbr_iscck)
					txh->u2.MaxAggLen_FBR =
						htol16(wlc_ampdu_calc_maxlen(ampdu_tx,
						txh->FragPLCPFallback[0], txh->FragPLCPFallback[3],
						txop));
				else
					txh->u2.MaxAggLen_FBR = 0;
			}

			txh->MaxNMpdus = htol16((max_pdu << 8) | max_pdu);
			/* retry limits - overload MModeFbrLen */
			if (WLPKTTAG(p)->flags & WLF_VRATE_PROBE) {
				txh->MModeFbrLen = htol16((1 << 8) |
					(ampdu_tx->retry_limit_tid[tid] & 0xff));
				txh->MaxNMpdus = htol16((max_pdu << 8) | ampdu_tx->probe_mpdu);
			} else {
				txh->MModeFbrLen = htol16(((rr_retry_limit & 0xff) << 8) |
					(ampdu_tx->retry_limit_tid[tid] & 0xff));
			}

			WL_AMPDU_HW(("wl%d: %s len %d MaxNMpdus 0x%x null delim %d MinMBytes %d "
				     "MaxAggDur %d MaxRNum %d rr_lmnt 0x%x\n", wlc->pub->unit,
				     __FUNCTION__, len, txh->MaxNMpdus, ndelim, txh->MinMBytes,
				     txh->u1.MaxAggDur, txh->u2.MaxRNum, txh->MModeFbrLen));

			/* Ucode agg:
			 * length to be written into side channel includes:
			 * leading delimiter, mac hdr to fcs, padding, null delimiter(s)
			 * i.e. (len + 4 + 4*num_paddelims + padbytes)
			 * padbytes is num required to round to 32 bits
			 */
			if (AMPDU_UCODE_ENAB(wlc->pub)) {
				if (ndelim) {
					len = ROUNDUP(len, 4);
					len += (ndelim  + 1) * AMPDU_DELIMITER_LEN;
				} else {
					len += AMPDU_DELIMITER_LEN;
				}
			}

			/* flip the epoch? */
			if (hagg->change_epoch) {
				hagg->epoch = !hagg->epoch;
				hagg->change_epoch = FALSE;
				WL_AMPDU_HWDBG(("sendampdu: fifo %d new epoch: %d frameid %x\n",
					sc, hagg->epoch, frameid));
				WLCNTINCR(ampdu_tx->cnt->epochdelta);
			}
			aggfifo_enque(ampdu_tx, len, sc);
		} else
#endif /* WLAMPDU_MAC */
		{ /* host agg */
			if (count == 0) {
				uint16 fc;
				mcl |= (TXC_AMPDU_FIRST << TXC_AMPDU_SHIFT);
				/* refill the bits since might be a retx mpdu */
				mcl |= TXC_STARTMSDU;
				rts = (struct dot11_rts_frame*)&txh->rts_frame;
				fc = ltoh16(rts->fc);
				if ((fc & FC_KIND_MASK) == FC_RTS) {
					mcl |= TXC_SENDRTS;
					use_rts = TRUE;
				}
				if ((fc & FC_KIND_MASK) == FC_CTS) {
					mcl |= TXC_SENDCTS;
					use_cts = TRUE;
				}
			} else {
				mcl |= (TXC_AMPDU_MIDDLE << TXC_AMPDU_SHIFT);
				mcl &= ~(TXC_STARTMSDU | TXC_SENDRTS | TXC_SENDCTS);
			}

			len = ROUNDUP(len, 4);
			ampdu_len += (len + (ndelim + 1) * AMPDU_DELIMITER_LEN);
			ASSERT(ampdu_len <= scb_ampdu->max_rxlen);
			dma_len += (uint16)pkttotlen(osh, p);

			WL_AMPDU_TX(("wl%d: wlc_sendampdu: ampdu_len %d seg_cnt %d null delim %d\n",
				wlc->pub->unit, ampdu_len, seg_cnt, ndelim));
		}
		txh->MacTxControlLow = htol16(mcl);

		/* this packet is added */
		pkt[count++] = p;

		/* patch the first MPDU */
		if (AMPDU_HOST_ENAB(wlc->pub) && count == 1) {
			uint8 plcp0, plcp3, is40, sgi;
			uint32 txoplen = 0;

			if (rr) {
				plcp0 = plcp[0];
				plcp3 = plcp[3];
			} else {
				plcp0 = txh->FragPLCPFallback[0];
				plcp3 = txh->FragPLCPFallback[3];

				/* Support multiple fallback rate:
				 * For txtimes > rr_retry_limit, use fallback -> fallback.
				 * To get around the fix-rate case check if primary == fallback rate
				 */
				if (ini->txretry[indx] > rr_retry_limit && ampdu_tx->mfbr &&
				    plcp[0] != plcp0) {
					ratespec_t fbrspec;
					uint8 fbr_mcs;
					uint16 phyctl1;

					use_mfbr = TRUE;
					txh->FragPLCPFallback[3] &=  ~PLCP3_STC_MASK;
					plcp3 = txh->FragPLCPFallback[3];
					fbrspec = wlc_scb_ratesel_getmcsfbr(wlc->wrsi, scb,
						ltoh16(txh->TxFrameID), plcp0);
					fbr_mcs = RSPEC_RATE_MASK & fbrspec;

					/* restore bandwidth */
					fbrspec &= ~RSPEC_BW_MASK;
					switch (ltoh16(txh->PhyTxControlWord_1_Fbr)
						 & PHY_TXC1_BW_MASK)
					{
						case PHY_TXC1_BW_20MHZ:
						case PHY_TXC1_BW_20MHZ_UP:
							fbrspec |= RSPEC_BW_20MHZ;
						break;
						case PHY_TXC1_BW_40MHZ:
						case PHY_TXC1_BW_40MHZ_DUP:
							fbrspec |= RSPEC_BW_40MHZ;
						break;
						default:
							ASSERT(0);
					}
					if ((RSPEC_IS40MHZ(fbrspec) &&
						(CHSPEC_IS20(wlc->chanspec))) ||
						(IS_CCK(fbrspec))) {
						fbrspec &= ~RSPEC_BW_MASK;
						fbrspec |= RSPEC_BW_20MHZ;
					}

					if (IS_SINGLE_STREAM(fbr_mcs)) {
						fbrspec &= ~(RSPEC_TXEXP_MASK | RSPEC_STBC);

						/* For SISO MCS use STBC if possible */
						if (RSPEC_ISHT(fbrspec) &&
						    WLC_STF_SS_STBC_TX(wlc, scb)) {
							uint8 stc = 1;

							ASSERT(WLC_STBC_CAP_PHY(wlc));
							fbrspec |= RSPEC_STBC;

							/* also set plcp bits 29:28 */
							plcp3 = (plcp3 & ~PLCP3_STC_MASK) |
							        (stc << PLCP3_STC_SHIFT);
							txh->FragPLCPFallback[3] = plcp3;
							WLCNTINCR(ampdu_tx->cnt->txampdu_mfbr_stbc);
						} else if (wlc->stf->ss_opmode == PHY_TXC1_MODE_CDD)
						{
							fbrspec |= (1 << RSPEC_TXEXP_SHIFT);
						}
					}
					/* rspec returned from getmcsfbr() doesn't have SGI info. */
					if (wlc->sgi_tx == ON)
						fbrspec |= RSPEC_SHORT_GI;
					phyctl1 = wlc_phytxctl1_calc(wlc, fbrspec);
					txh->PhyTxControlWord_1_Fbr = htol16(phyctl1);

					txh->FragPLCPFallback[0] = fbr_mcs |
						(plcp0 & MIMO_PLCP_40MHZ);
					plcp0 = txh->FragPLCPFallback[0];
				}
			}
			is40 = (plcp0 & MIMO_PLCP_40MHZ) ? 1 : 0;
			sgi = PLCP3_ISSGI(plcp3) ? 1 : 0;
			mcs = plcp0 & ~MIMO_PLCP_40MHZ;
			ASSERT(mcs < MCS_TABLE_SIZE);
			maxlen = MIN(scb_ampdu->max_rxlen, ampdu_tx->max_txlen[mcs][is40][sgi]);

			/* take into account txop and tx burst limit restriction */
			if (txop) {
				/* rate is in Kbps; txop is in usec
				 * ==> len = (rate * txop) / (1000 * 8)
				 */
				txoplen = (MCS_RATE(mcs, is40, sgi) >> 10) * (txop >> 3);
				maxlen = MIN(maxlen, txoplen);
			}

			/* rebuild the rspec and rspec_fallback */
			rspec = HT_RSPEC(plcp[0] & MIMO_PLCP_MCS_MASK);

			if (plcp[0] & MIMO_PLCP_40MHZ)
				rspec |= RSPEC_BW_40MHZ;

			if (fbr_iscck) /* CCK */
				rspec_fallback =
					CCK_RSPEC(CCK_PHY2MAC_RATE(txh->FragPLCPFallback[0]));
			else { /* MIMO */
				rspec_fallback =
				        HT_RSPEC(txh->FragPLCPFallback[0] & MIMO_PLCP_MCS_MASK);
				if (txh->FragPLCPFallback[0] & MIMO_PLCP_40MHZ)
					rspec_fallback |= RSPEC_BW_40MHZ;
			}

			if (use_rts || use_cts) {
				uint16 fc;
				rts_rspec = wlc_rspec_to_rts_rspec(cfg, rspec, FALSE);
				rts_rspec_fallback = wlc_rspec_to_rts_rspec(cfg, rspec_fallback,
				                                            FALSE);
				/* need to reconstruct plcp and phyctl words for rts_fallback */
				if (use_mfbr) {
					int rts_phylen;
					uint8 rts_plcp_fallback[D11_PHY_HDR_LEN], old_ndelim;
					uint16 phyctl1, xfts;

					old_ndelim = txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM];
					phyctl1 = wlc_phytxctl1_calc(wlc, rts_rspec_fallback);
					txh->PhyTxControlWord_1_FbrRts = htol16(phyctl1);
					if (use_cts)
						rts_phylen = DOT11_CTS_LEN + DOT11_FCS_LEN;
					else
						rts_phylen = DOT11_RTS_LEN + DOT11_FCS_LEN;

					fc = ltoh16(rts->fc);
					wlc_compute_plcp(wlc, rts_rspec_fallback, rts_phylen,
					fc, rts_plcp_fallback);

					bcopy(rts_plcp_fallback, (char*)&txh->RTSPLCPFallback,
						sizeof(txh->RTSPLCPFallback));

					/* restore it */
					txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM] = old_ndelim;

					/* update XtraFrameTypes in case it has changed */
					xfts = ltoh16(txh->XtraFrameTypes);
					xfts &= ~(PHY_TXC_FT_MASK << XFTS_FBRRTS_FT_SHIFT);
					xfts |= ((IS_CCK(rts_rspec_fallback) ? FT_CCK : FT_OFDM)
						<< XFTS_FBRRTS_FT_SHIFT);
					txh->XtraFrameTypes = htol16(xfts);
				}
			}
		} /* if (first mpdu for host agg) */

		/* test whether to add more */
		if (AMPDU_HOST_ENAB(wlc->pub)) {
			if ((MCS_RATE(mcs, TRUE, FALSE) >= f->dmaxferrate) &&
			    (count == f->mcs2ampdu_table[mcs])) {
				WL_AMPDU_ERR(("wl%d: PR 37644: stopping ampdu at %d for mcs %d\n",
					wlc->pub->unit, count, mcs));
				break;
			}

			/* limit the size of ampdu for probe packets */
			if (vrate_probe && (count == ampdu_tx->probe_mpdu))
				break;

			if (count == scb_ampdu->max_pdu)
				break;
		}
#ifdef WLAMPDU_MAC
		else {
			if (count >= aggfifo_space) {
				uint space;
				space = get_aggfifo_space(ampdu_tx, sc);
				if (space > 0)
					aggfifo_space += space;
				else {
				WL_AMPDU_HW(("sendampdu: break due to aggfifo full."
					     "count %d space %d\n", count, aggfifo_space));
				break;
			}
		}
		}
#endif /* WLAMPDU_MAC */

		/* check to see if the next pkt is a candidate for aggregation */
		p = pktq_ppeek(&qi->q, prec);

		if (ampdu_tx->hiagg_mode) {
			if (!p && (prec & 1)) {
				prec = prec & ~1;
				p = pktq_ppeek(&qi->q, prec);
			}
		}
		if (p) {
			if (WLPKTFLAG_AMPDU(WLPKTTAG(p)) &&
				(WLPKTTAGSCBGET(p) == scb) &&
				((uint8)PKTPRIO(p) == tid)) {
				if (wlc->dma_avoidance_war)
					nsegs = pktsegcnt_war(osh, p);
				else
					nsegs = pktsegcnt(osh, p);
				WL_NONE(("%s: txavail %d  < seg_cnt %d nsegs %d\n", __FUNCTION__,
					TXAVAIL(wlc, fifo), seg_cnt, nsegs));
				/* check if there are enough descriptors available */
				if (TXAVAIL(wlc, fifo) <= (seg_cnt + nsegs)) {
					WLCNTINCR(ampdu_tx->cnt->txfifofull);
					p = NULL;
					continue;
				}

				if (AMPDU_HOST_ENAB(wlc->pub)) {
					plen = pkttotlen(osh, p) + AMPDU_MAX_MPDU_OVERHEAD;
					plen = MAX(scb_ampdu->min_len, plen);
					if ((plen + ampdu_len) > maxlen) {
						p = NULL;
						continue;
					}

					ASSERT((rr && !fbr) || (!rr && fbr));
					indx = TX_SEQ_TO_INDEX(WLPKTTAG(p)->seq);
					if (ampdu_tx->hiagg_mode) {
						if (((ini->txretry[indx] < rr_retry_limit) &&
						     fbr) ||
						    ((ini->txretry[indx] >= rr_retry_limit) &&
						     rr)) {
							p = NULL;
							continue;
						}
					} else {
						if (txretry != ini->txretry[indx]) {
							p = NULL;
							continue;
						}
					}
				}

				p = pktq_pdeq(&qi->q, prec);
				ASSERT(p);
			} else {
#ifdef WLAMPDU_MAC
				if (AMPDU_MAC_ENAB(wlc->pub))
					wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_DSTTID);
#endif
				p = NULL;
			}
		}
	}  /* end while(p) */

	if (count) {

		WL_AMPDU_HWDBG(("%s: tot_mpdu %d txfifo %d\n", __FUNCTION__, count,
			R_REG(wlc->osh, &wlc->regs->u.d11regs.xmtfifo_frame_cnt)));
		WLCNTADD(ampdu_tx->cnt->txmpdu, count);
		AMPDUSCBCNTADD(scb_ampdu->cnt.txmpdu, count);
		if (AMPDU_HOST_ENAB(wlc->pub)) {
#ifdef WLCNT
			ampdu_tx->cnt->txampdu++;
#endif
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
			if (ampdu_tx->amdbg)
				ampdu_tx->amdbg->txmcs[mcs]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */
#ifdef WLPKTDLYSTAT
			WLCNTADD(ampdu_tx->cnt->txmpdu_cnt[mcs], count);
#endif

			/* patch up the last txh */
			txh = (d11txh_t *)PKTDATA(osh, pkt[count - 1]);
			mcl = ltoh16(txh->MacTxControlLow);
			mcl &= ~TXC_AMPDU_MASK;
			mcl |= (TXC_AMPDU_LAST << TXC_AMPDU_SHIFT);
			txh->MacTxControlLow = htol16(mcl);

			/* remove the null delimiter after last mpdu */
			ndelim = txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM];
			txh->RTSPLCPFallback[AMPDU_FBR_NULL_DELIM] = 0;
			ampdu_len -= ndelim * AMPDU_DELIMITER_LEN;

			/* remove the pad len from last mpdu */
			fbr_iscck = ((ltoh16(txh->XtraFrameTypes) & 0x3) == 0);
			len = fbr_iscck ? WLC_GET_CCK_PLCP_LEN(txh->FragPLCPFallback)
				: WLC_GET_MIMO_PLCP_LEN(txh->FragPLCPFallback);
			ampdu_len -= ROUNDUP(len, 4) - len;

			/* patch up the first txh & plcp */
			txh = (d11txh_t *)PKTDATA(osh, pkt[0]);
			plcp = (uint8 *)(txh + 1);

			WLC_SET_MIMO_PLCP_LEN(plcp, ampdu_len);
			/* mark plcp to indicate ampdu */
			WLC_SET_MIMO_PLCP_AMPDU(plcp);

			/* reset the mixed mode header durations */
			if (txh->MModeLen) {
				uint16 mmodelen = wlc_calc_lsig_len(wlc, rspec, ampdu_len);
				txh->MModeLen = htol16(mmodelen);
				preamble_type = WLC_MM_PREAMBLE;
			}
			if (txh->MModeFbrLen) {
				uint16 mmfbrlen = wlc_calc_lsig_len(wlc, rspec_fallback, ampdu_len);
				txh->MModeFbrLen = htol16(mmfbrlen);
				fbr_preamble_type = WLC_MM_PREAMBLE;
			}

			/* set the preload length */
			if (MCS_RATE(mcs, TRUE, FALSE) >= f->dmaxferrate) {
				dma_len = MIN(dma_len, f->ampdu_pld_size);
				if (D11REV_IS(wlc->pub->corerev, 29))
					dma_len >>= 2;
				txh->PreloadSize = htol16(dma_len);
			} else
				txh->PreloadSize = 0;

			mch = ltoh16(txh->MacTxControlHigh);

			/* update RTS dur fields */
			if (use_rts || use_cts) {
				uint16 durid;
				rts = (struct dot11_rts_frame*)&txh->rts_frame;
				if ((mch & TXC_PREAMBLE_RTS_MAIN_SHORT) ==
				    TXC_PREAMBLE_RTS_MAIN_SHORT)
					rts_preamble_type = WLC_SHORT_PREAMBLE;

				if ((mch & TXC_PREAMBLE_RTS_FB_SHORT) == TXC_PREAMBLE_RTS_FB_SHORT)
					rts_fbr_preamble_type = WLC_SHORT_PREAMBLE;

				durid = wlc_compute_rtscts_dur(wlc, use_cts, rts_rspec, rspec,
					rts_preamble_type, preamble_type, ampdu_len, TRUE);
				rts->durid = htol16(durid);
				durid = wlc_compute_rtscts_dur(wlc, use_cts,
					rts_rspec_fallback, rspec_fallback, rts_fbr_preamble_type,
					fbr_preamble_type, ampdu_len, TRUE);
				txh->RTSDurFallback = htol16(durid);
				/* set TxFesTimeNormal */
				txh->TxFesTimeNormal = rts->durid;
				/* set fallback rate version of TxFesTimeNormal */
				txh->TxFesTimeFallback = txh->RTSDurFallback;
			}

			/* set flag and plcp for fallback rate */
			if (fbr) {
				WLCNTADD(ampdu_tx->cnt->txfbr_mpdu, count);
				WLCNTINCR(ampdu_tx->cnt->txfbr_ampdu);
				mch |= TXC_AMPDU_FBR;
				txh->MacTxControlHigh = htol16(mch);
				WLC_SET_MIMO_PLCP_AMPDU(plcp);
				WLC_SET_MIMO_PLCP_AMPDU(txh->FragPLCPFallback);
				if (PLCP3_ISSGI(txh->FragPLCPFallback[3])) {
					WLCNTINCR(ampdu_tx->cnt->u1.txampdu_sgi);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
					if (ampdu_tx->amdbg)
						ampdu_tx->amdbg->txmcssgi[mcs]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */
				}
				if (PLCP3_ISSTBC(txh->FragPLCPFallback[3])) {
					WLCNTINCR(ampdu_tx->cnt->u2.txampdu_stbc);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
					if (ampdu_tx->amdbg)
						ampdu_tx->amdbg->txmcsstbc[mcs]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */
				}
			} else {
				if (PLCP3_ISSGI(plcp[3])) {
					WLCNTINCR(ampdu_tx->cnt->u1.txampdu_sgi);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
					if (ampdu_tx->amdbg)
						ampdu_tx->amdbg->txmcssgi[mcs]++;
#endif /* defined(BCMDBG) || defined(WLTEST) */
				}
				if (PLCP3_ISSTBC(plcp[3])) {
					WLCNTINCR(ampdu_tx->cnt->u2.txampdu_stbc);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
					if (ampdu_tx->amdbg)
						ampdu_tx->amdbg->txmcsstbc[mcs]++;
#endif
				}
			}

			if (txretry)
				WLCNTINCR(ampdu_tx->cnt->txretry_ampdu);

			WL_AMPDU_TX(("wl%d: wlc_sendampdu: count %d ampdu_len %d\n",
				wlc->pub->unit, count, ampdu_len));

			/* inform rate_sel if it this is a rate probe pkt */
			frameid = ltoh16(txh->TxFrameID);
			if (frameid & TXFID_RATE_PROBE_MASK) {
				wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid, TRUE,
					(uint8)txretry);
			}
		}
#ifdef WLC_HIGH_ONLY
		/* start aggregating rpc calls as we pass down the individual pkts in this AMPDU */
		if (wlc->rpc_agg & BCM_RPC_TP_HOST_AGG_AMPDU)
			bcm_rpc_tp_agg_set(bcm_rpc_tp_get(wlc->rpc), BCM_RPC_TP_HOST_AGG_AMPDU,
				TRUE);
#endif

		/* send all mpdus atomically to the dma engine */
		if (AMPDU_MAC_ENAB(wlc->pub)) {
			for (i = 0; i < count; i++)
				/* always commit */
				wlc_txfifo(wlc, fifo, pkt[i], &txh_info, TRUE, 1);
		} else {
			for (i = 0; i < count; i++)
				wlc_txfifo(wlc, fifo, pkt[i], &txh_info,
				           i == (count-1), ampdu_tx->txpkt_weight);
		}
#ifdef WLC_HIGH_ONLY
		/* stop the rpc packet aggregation and release all queued rpc packets */
		if (wlc->rpc_agg & BCM_RPC_TP_HOST_AGG_AMPDU)
			bcm_rpc_tp_agg_set(bcm_rpc_tp_get(wlc->rpc), BCM_RPC_TP_HOST_AGG_AMPDU,
				FALSE);
#endif

	} /* endif (count) */

	if (regmpdu) {
		WL_AMPDU_TX(("wl%d: wlc_sendampdu: sending regular mpdu\n", wlc->pub->unit));

		/* inform rate_sel if it this is a rate probe pkt */
		txh = (d11txh_t *)PKTDATA(osh, p);
		frameid = ltoh16(txh->TxFrameID);
		if (frameid & TXFID_RATE_PROBE_MASK) {
			wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid, FALSE, 0);
		}

		WLCNTINCR(ampdu_tx->cnt->txregmpdu);
		wlc_txfifo(wlc, fifo, p, &txh_info, TRUE, 1);

#ifdef WLAMPDU_MAC
		if (AMPDU_MAC_ENAB(wlc->pub))
			wlc_ampdu_change_epoch(ampdu_tx, sc, AMU_EPOCH_CHG_NAGG);
#endif
	}

	return err;
}

#ifdef WLAMPDU_MAC
/*
 * for HW with AQM feature, life is simpler
 */
static int BCMFASTPATH
_wlc_sendampdu_aqm(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **pdu, int prec)
{
	wlc_info_t *wlc;
	osl_t* osh;
	void* p;
	struct scb *scb;
	uint8 tid;
	scb_ampdu_tid_ini_t *ini;
	wlc_bsscfg_t *cfg = NULL;
	scb_ampdu_tx_t *scb_ampdu;
	ampdumac_info_t *hagg;

	/* in time: use this to refactor */
	ampdu_create_info_t ampdu_cons_info;

	wlc_txh_info_t tx_info;
	int err = 0;
	uint fifo, npkts, count = 0, fifoavail = 0;
	uint16 seq;
	struct dot11_header* h;
	int seq_changed = FALSE;
	bool fill_txh_info;

	BCM_REFERENCE(fifoavail);
	wlc = ampdu_tx->wlc;
	osh = wlc->osh;
	p = *pdu;

	/*
	 * since we obtain <scb, tid, fifo> etc outside the loop,
	 * have to break and return if either of them has changed
	 */
	tid = (uint8)PKTPRIO(p);
	ASSERT(tid < AMPDU_MAX_SCB_TID);
	fifo = prio2fifo[tid];
	ASSERT(fifo < AC_COUNT);
	hagg = &ampdu_tx->hagg[fifo];

	scb = WLPKTTAGSCBGET(p);
	cfg = SCB_BSSCFG(scb);
	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ini = scb_ampdu->ini[tid];

	ampdu_cons_info.scb_ampdu = scb_ampdu;
	ampdu_cons_info.cfg = cfg;
	ampdu_cons_info.ini = ini;
	ampdu_cons_info.ampdu = ampdu_tx;

#ifdef WLC_HIGH_ONLY
	fifoavail = TXAVAIL(wlc, fifo);
	if (!fifoavail) {
		return BCME_BUSY;
	}
	/* start aggregating rpc calls as we pass down the individual pkts in this AMPDU */
	if (wlc->rpc_agg & BCM_RPC_TP_HOST_AGG_AMPDU) {
		bcm_rpc_tp_agg_set(bcm_rpc_tp_get(wlc->rpc), BCM_RPC_TP_HOST_AGG_AMPDU,
		TRUE);
	}
#endif

	while (p != NULL) {
		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);
		seq = WLPKTTAG(p)->seq;

		if (WLPKTTAG(p)->flags & WLF_MPDU)
			err = wlc_prep_pdu(wlc, p, &fifo);
		else if ((err = wlc_prep_sdu_fast(wlc, cfg, scb, p, &fifo)) == BCME_UNSUPPORTED)
			err = wlc_prep_sdu(wlc, &p, (int*)&npkts, &fifo);

		if (err) {
			if (err == BCME_BUSY) {
				WL_AMPDU_TX(("wl%d: %s prep_xdu retry; seq 0x%x\n",
					wlc->pub->unit, __FUNCTION__, seq));
				WLCNTINCR(ampdu_tx->cnt->txfifofull);
				WLCNTINCR(ampdu_tx->cnt->sduretry);
				*pdu = p;
				break;
			}

			/* error in the packet; reject it */
			WL_AMPDU_ERR(("wl%d: wlc_sendampdu: prep_xdu rejected; seq 0x%x\n",
				wlc->pub->unit, seq));
			WLCNTINCR(ampdu_tx->cnt->sdurejected);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.sdurejected);

			/*
			 * let ampdu_is_exp_seq() catch it, update tx_exp_seq, send_bar and
			 * change epoch if necessary
			 */
			*pdu = NULL;
			break;
		}
		WL_AMPDU_TX(("wl%d: %s: prep_xdu success; seq 0x%x tid %d\n",
			wlc->pub->unit, __FUNCTION__, seq, tid));

		/* if no error, populate tx hdr info and continue */
		ASSERT(p != NULL);
		ASSERT(WLPKTTAG(p)->flags & WLF_MPDU);
		fill_txh_info = ((count == 0) || (WLPKTTAG(p)->flags2 & WLF2_TXCMISS));
		if (fill_txh_info)
			wlc_get_txh_info(wlc, p, &tx_info);
		else {
			int tsoHdrSize;
			d11actxh_t* vhtHdr = NULL;
			tsoHdrSize = wlc_pkt_get_vht_hdr(wlc, p, &vhtHdr);
#ifdef WLTOEHW
			tx_info.tsoHdrSize = tsoHdrSize;
#endif
			tx_info.tsoHdrPtr = (void*)((tsoHdrSize != 0) ?
				PKTDATA(wlc->osh, p) : NULL);
			tx_info.hdrPtr = (wlc_txd_t *)(PKTDATA(wlc->osh, p) + tsoHdrSize);
			tx_info.hdrSize = D11AC_TXH_LEN;

			tx_info.d11HdrPtr = ((uint8 *)tx_info.hdrPtr) + D11AC_TXH_LEN;

			tx_info.TxFrameID = vhtHdr->PktInfo.TxFrameID;
			tx_info.MacTxControlLow = vhtHdr->PktInfo.MacTxControlLow;
			tx_info.MacTxControlHigh = vhtHdr->PktInfo.MacTxControlHigh;
			tx_info.plcpPtr = (vhtHdr->RateInfo[0].plcp);
		}

		h = (struct dot11_header*)(tx_info.d11HdrPtr);
		seq = ltoh16(h->seq) >> SEQNUM_SHIFT;

		/* during roam we get pkts with null a1. kill it here else
		 * does not find scb on dotxstatus and things gets out of sync
		 */
		if (ETHER_ISNULLADDR(&h->a1)) {
			WL_AMPDU_ERR(("wl%d: %s: dropping frame with null a1\n",
				wlc->pub->unit, __FUNCTION__));
			err = BCME_ERROR;
		} else if (!(WLPKTTAG(p)->flags & WLF_FIFOPKT) &&
			!ampdu_is_exp_seq(ampdu_tx, ini, seq, &seq_changed)) {
			/* If the packet was not retrieved from HW FIFO before
			 * attempting over the air for multi-queue
			 */
			WL_AMPDU_TX(("%s: multi-queue error\n", __FUNCTION__));
			err = BCME_ERROR;
		}

		if (err == BCME_ERROR) {
			PKTFREE(osh, p, TRUE);
			*pdu = NULL;
			WLCNTINCR(ampdu_tx->cnt->txdrop);
			ini->tx_in_transit--;
			break;
		}

		if (WLPKTTAG(p)->flags & WLF_FIFOPKT)
			WLPKTTAG(p)->flags &= ~WLF_FIFOPKT;

		if (!(WLPKTTAG(p)->flags & WLF_MIMO) ||
		    (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF) ||
		    SCB_PS(scb)) {
			WL_AMPDU_TX(("%s: reg mpdu found\n", __FUNCTION__));
			wlc_get_txh_info(wlc, p, &tx_info);
			wlc_txh_set_isAMPDU(wlc, &tx_info, FALSE);
			if (hagg->prev_ft != 0) {
				/* ampdu switch to regmpdu */
				wlc_ampdu_chgnsav_epoch(ampdu_tx, fifo,
					AMU_EPOCH_CHG_NAGG, scb, tid, &tx_info);
				wlc_txh_set_epoch(ampdu_tx->wlc, &tx_info, hagg->epoch);
				hagg->prev_ft = 0;
			}
			/* if prev is regmpdu already, don't bother to set epoch at all */
		} else {
			wlc_txh_set_isAMPDU(wlc, &tx_info, TRUE);
			ampdu_cons_info.tx_info = &tx_info;
			wlc_ampdu_fill_percache_info(wlc, &ampdu_cons_info);

			if (fill_txh_info) {
				uint16 frameid;
				bool change = TRUE;
				uint8 reason_code = 0;

				frameid = ltoh16(tx_info.TxFrameID);
				if (frameid & TXFID_RATE_PROBE_MASK) {
					/* check vrate probe flag
					 * this flag is only get the frame to become first mpdu
					 * of ampdu and no need to save epoch_params
					 */
					wlc_scb_ratesel_probe_ready(wlc->wrsi, scb, frameid,
					                            FALSE, 0);
					reason_code = AMU_EPOCH_CHG_FID;
				} else if (scb != hagg->prev_scb || tid != hagg->prev_tid) {
					reason_code = AMU_EPOCH_CHG_DSTTID;
				} else if (wlc_ampdu_epoch_params_chg(hagg, scb, tid, &tx_info)) {
					/* rate/sgi/bw/stbc/antsel tx params */
					reason_code = AMU_EPOCH_CHG_PLCP;
				} else if (seq_changed) {
					reason_code = AMU_EPOCH_CHG_SEQ;
				}  else
					change = FALSE;
				if (change) {
					wlc_ampdu_chgnsav_epoch(ampdu_tx, fifo, reason_code,
						scb, tid, &tx_info);
				}
			}
			/* always set epoch for ampdu */
			wlc_txh_set_epoch(ampdu_tx->wlc, &tx_info, hagg->epoch);
		}

		wlc_txfifo(wlc, fifo, p, &tx_info, TRUE, 1);
		count++;
#ifdef WLC_HIGH_ONLY
		/* stop sending packets to the fifo  if full */
		if (count == fifoavail) {
			break;
		}
#endif

		/* check to see if the next pkt is a candidate for aggregation */
		p = pktq_ppeek(&qi->q, prec);

		if (p != NULL) {
			if (WLPKTFLAG_AMPDU(WLPKTTAG(p)) &&
				(WLPKTTAGSCBGET(p) == scb) &&
				((uint8)PKTPRIO(p) == tid)) {
				p = pktq_pdeq(&qi->q, prec);
				ASSERT(p);
			} else {
				p = NULL;
			}
		}
	}  /* end while(p) */

	if (count) {
		ASSERT(ampdu_tx->cnt);
		AMPDUSCBCNTADD(scb_ampdu->cnt.txmpdu, count);
	}
#ifdef WLC_HIGH_ONLY
	/* stop the rpc packet aggregation and release all queued rpc packets */
	if (wlc->rpc_agg & BCM_RPC_TP_HOST_AGG_AMPDU) {
		bcm_rpc_tp_agg_set(bcm_rpc_tp_get(wlc->rpc), BCM_RPC_TP_HOST_AGG_AMPDU,
		FALSE);
	}
#endif

	WL_AMPDU_TX(("%s: fifo %d count %d epoch %d txpktpend %d fifocnt 0x%x\n",
		__FUNCTION__, fifo, count, ampdu_tx->hagg[fifo].epoch,
		TXPKTPENDGET(wlc, fifo),
		R_REG(wlc->osh, &wlc->regs->u.d11acregs.XmtFifoFrameCnt)));
	return err;
}
#endif /* WLAMPDU_MAC */

/* send bar if needed to move the window forward;
 * if start is set move window to start_seq
 */
static void
wlc_ampdu_send_bar(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tid_ini_t *ini, bool start)
{
	int indx, i;
	scb_ampdu_tx_t *scb_ampdu;
	void *p = NULL;
	uint16 seq;

	/* stop retry BAR if subsequent tx has made the BAR seq. stale  */
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub) &&
		(ini->retry_bar != AMPDU_R_BAR_DISABLED)) {
		uint16 offset, stop_retry = 0;

		WL_AMPDU(("retry bar for reason %x, bar_seq 0x%04x, max_seq 0x%04x,"
			"acked_seq 0x%04x, bar_cnt %d, tx_in_transit %d\n",
			ini->retry_bar, ini->barpending_seq, ini->max_seq, ini->acked_seq,
			ini->bar_cnt, ini->tx_in_transit));

		ini->retry_bar = AMPDU_R_BAR_DISABLED;
		offset = MODSUB_POW2(ini->acked_seq, ini->barpending_seq, SEQNUM_MAX);
		if ((offset > ini->ba_wsize) && (offset < (SEQNUM_MAX >> 1)))
			stop_retry = 1;

		if (MODSUB_POW2(ini->max_seq, ini->barpending_seq, SEQNUM_MAX)
			>= (SEQNUM_MAX >> 1))
			stop_retry = 1;
		if (stop_retry) {
			WL_AMPDU(("wl%d: stop retry bar %x\n", ampdu_tx->wlc->pub->unit,
				ini->barpending_seq));
			return;
		}
	}
	ini->retry_bar = AMPDU_R_BAR_DISABLED;
	if (ini->bar_ackpending)
		return;

	if (ini->ba_state != AMPDU_TID_STATE_BA_ON)
		return;

	if (ini->bar_cnt >= AMPDU_BAR_RETRY_CNT)
		return;

	if (ini->bar_cnt == (AMPDU_BAR_RETRY_CNT >> 1)) {
		ini->bar_cnt++;
		ini->retry_bar = AMPDU_R_BAR_HALF_RETRY_CNT;
		return;
	}

	ASSERT(ini->ba_state == AMPDU_TID_STATE_BA_ON);

	if (start)
		seq = ini->start_seq;
	else {
		int offset = -1;

		if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
			indx = TX_SEQ_TO_INDEX(ini->start_seq);
			for (i = 0; i < (ini->ba_wsize - ini->rem_window); i++) {
				if (isset(ini->barpending, indx))
					offset = i;
				indx = NEXT_TX_INDEX(indx);
			}

			if (offset == -1) {
				ini->bar_cnt = 0;
				return;
			}
			seq = MODADD_POW2(ini->start_seq, offset + 1, SEQNUM_MAX);
		} else {
			seq = ini->barpending_seq;
			ASSERT(ini->barpending_seq < SEQNUM_MAX);
		}
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb);
	ASSERT(scb_ampdu);
	BCM_REFERENCE(scb_ampdu);

	if (ampdu_tx->no_bar == FALSE) {
		bool blocked;
		p = wlc_send_bar(ampdu_tx->wlc, ini->scb, ini->tid, seq,
		                 DOT11_BA_CTL_POLICY_NORMAL, TRUE, &blocked);
		if (blocked) {
			ini->retry_bar = AMPDU_R_BAR_BLOCKED;
			return;
		}

		if (p == NULL) {
			ini->retry_bar = AMPDU_R_BAR_NO_BUFFER;
			return;
		}

		if (wlc_pkt_callback_register(ampdu_tx->wlc, wlc_send_bar_complete, ini, p)) {
			ini->retry_bar = AMPDU_R_BAR_CALLBACK_FAIL;
			return;
		}

		WLCNTINCR(ampdu_tx->cnt->txbar);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.txbar);
	}

	ini->bar_cnt++;
	ini->bar_ackpending = TRUE;
	ini->bar_ackpending_seq = seq;
	ini->barpending_seq = seq;

	if (ampdu_tx->no_bar == TRUE) {
		wlc_send_bar_complete(ampdu_tx->wlc, TX_STATUS_ACK_RCV, ini);
	}
}

#ifdef WLAMPDU_MAC
static INLINE void
wlc_ampdu_ini_move_window_aqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini)
{

	ASSERT(ini);
	ASSERT(ampdu_tx);
	ASSERT(ampdu_tx->wlc);
	ASSERT(ampdu_tx->wlc->pub);

	if (ini->acked_seq != ini->start_seq) {
		if (IS_SEQ_ADVANCED(ini->acked_seq, ini->start_seq)) {
			ini->start_seq = MODINC_POW2(ini->acked_seq, SEQNUM_MAX);
			ini->alive = TRUE;
		}
	}
	/* if possible, release some buffered pdus */
	wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, FALSE);
	return;
}
#endif /* WLAMPDU_MAC */

static INLINE void
wlc_ampdu_ini_move_window_noaqm(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini)
{
	uint16 indx, i, range;

	ASSERT(ini);
	ASSERT(ampdu_tx);
	ASSERT(ampdu_tx->wlc);
	ASSERT(ampdu_tx->wlc->pub);

	/* ba_wsize can be zero in AMPDU_TID_STATE_BA_OFF or PENDING_ON state */
	if (ini->ba_wsize == 0)
		return;

	range = MODSUB_POW2(MODADD_POW2(ini->max_seq, 1, SEQNUM_MAX), ini->start_seq, SEQNUM_MAX);
	for (i = 0, indx = TX_SEQ_TO_INDEX(ini->start_seq);
		(i < range) && (!isset(ini->ackpending, indx)) &&
		(!isset(ini->barpending, indx));
		i++, indx = NEXT_TX_INDEX(indx));

	ini->start_seq = MODADD_POW2(ini->start_seq, i, SEQNUM_MAX);
	ini->rem_window += i;
	/* mark alive only if window moves forward */
	if (i)
		ini->alive = TRUE;

	/* if possible, release some buffered pdus */
	wlc_ampdu_txeval(ampdu_tx, scb_ampdu, ini, FALSE);

	WL_AMPDU_TX(("wl%d: wlc_ampdu_ini_move_window: tid %d start_seq bumped to 0x%x\n",
		ampdu_tx->wlc->pub->unit, ini->tid, ini->start_seq));

	if (ini->rem_window > ini->ba_wsize) {
		WL_ERROR(("wl%d: %s: rem_window %d, ba_wsize %d "
			"i %d range %d sseq 0x%x\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__, ini->rem_window, ini->ba_wsize,
			i, range, ini->start_seq));
		ASSERT(0);
	}
}

/* bump up the start seq to move the window */
static INLINE void
wlc_ampdu_ini_move_window(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu,
	scb_ampdu_tid_ini_t *ini)
{
#ifdef WLAMPDU_MAC
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub))
		wlc_ampdu_ini_move_window_aqm(ampdu_tx, scb_ampdu, ini);
	else
#endif /* WLAMPDU_MAC */
		wlc_ampdu_ini_move_window_noaqm(ampdu_tx, scb_ampdu, ini);
}

static void
wlc_send_bar_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	scb_ampdu_tid_ini_t *ini = (scb_ampdu_tid_ini_t *)arg;
	ampdu_tx_info_t *ampdu_tx = wlc->ampdu_tx;
	uint16 indx, s_seq, i;
	bool aqm_no_bitmap = AMPDU_AQM_ENAB(ampdu_tx->wlc->pub);

	WL_AMPDU_CTL(("wl%d: wlc_send_bar_complete for tid %d status 0x%x\n",
		wlc->pub->unit, ini->tid, txstatus));

	if (ini->free_me) {
		for (i = 0; i < AMPDU_INI_FREE; i++) {
			if (ampdu_tx->ini_free[i] == ini) {
				ampdu_tx->ini_free[i] = NULL;
				break;
			}
		}
		ASSERT(i != AMPDU_INI_FREE);
		MFREE(wlc->osh, ini, sizeof(scb_ampdu_tid_ini_t));
		return;
	}

	ASSERT(ini->bar_ackpending);
	ini->bar_ackpending = FALSE;

	/* ack received */
	if (txstatus & TX_STATUS_ACK_RCV) {
		if (!aqm_no_bitmap) {
			for (s_seq = ini->start_seq;
			     s_seq != ini->bar_ackpending_seq;
			     s_seq = NEXT_SEQ(s_seq)) {
				indx = TX_SEQ_TO_INDEX(s_seq);
				if (isset(ini->barpending, indx)) {
					clrbit(ini->barpending, indx);
					if (isset(ini->ackpending, indx)) {
						clrbit(ini->ackpending, indx);
						ini->txretry[indx] = 0;
						ini->tx_in_transit--;
					}
				}
			}
			/* bump up the start seq to move the window */
			wlc_ampdu_ini_move_window(ampdu_tx,
				SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb), ini);
		} else {
			if (IS_SEQ_ADVANCED(ini->bar_ackpending_seq, ini->start_seq) &&
				IS_SEQ_ADVANCED(ini->max_seq, ini->bar_ackpending_seq)) {
				ini->acked_seq = ini->bar_ackpending_seq;
				wlc_ampdu_ini_move_window(ampdu_tx,
					SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb), ini);
			 }

			if (ini->barpending_seq == ini->bar_ackpending_seq)
				ini->barpending_seq = SEQNUM_MAX;
			ini->bar_cnt--;
		}
	}

	wlc_ampdu_txflowcontrol(wlc, SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb), ini);
	if (aqm_no_bitmap) {
		/* not acked or need to send  bar for another seq no.  */
		if (ini->barpending_seq != SEQNUM_MAX) {
			wlc_ampdu_send_bar(wlc->ampdu_tx, ini, FALSE);
		} else {
			ini->bar_cnt = 0;
			ini->retry_bar = AMPDU_R_BAR_DISABLED;
		}
	} else {
		wlc_ampdu_send_bar(wlc->ampdu_tx, ini, FALSE);
	}

}

#ifdef WLAMPDU_MAC
static void
wlc_ampdu_dotxstatus_regmpdu_aqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint16 seq, bar_seq = 0;

	ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

	if (!scb || !SCB_AMPDU(scb))
		return;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[PKTPRIO(p)];
	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU_ERR(("wl%d: ampdu_dotxstatus_regmpdu: NULL ini or pon state for tid %d\n",
			ampdu_tx->wlc->pub->unit, PKTPRIO(p)));
		return;
	}
	ASSERT(ini->scb == scb);

	seq = pkt_txh_seqnum(ampdu_tx->wlc, p);
	ini->tx_in_transit--;
	if (txs->status.was_acked) {
		ini->acked_seq = seq;
		wlc_ampdu_ini_move_window(ampdu_tx,
			SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb), ini);
	}
	else {
		WLCNTINCR(ampdu_tx->cnt->txreg_noack);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.txreg_noack);
		WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus_regmpdu: ack not recd for "
			"seq 0x%x tid %d status 0x%x\n",
			ampdu_tx->wlc->pub->unit, seq, ini->tid, txs->status.raw_bits));

#if defined(WLVSDB) && defined(PROP_TXSTATUS)
		if (!(MCHAN_ACTIVE(ampdu_tx->wlc->pub) &&
			(PROP_TXSTATUS_ENAB(ampdu_tx->wlc->pub)) &&
			ampdu_tx->wlc->txfifo_detach_pending)) {
#endif

		if ((AP_ENAB(ampdu_tx->wlc->pub) &&
			(txs->status.suppr_ind == TX_STATUS_SUPR_PMQ))) {
			return;
		}
#ifdef WLP2P
		if (P2P_ABS_SUPR(ampdu_tx->wlc, txs->status.suppr_ind))
			return;
#endif
		/*
		 *  send bar to move the window, if it is still within the window from
	         *  the last frame released to HW
		 */
		if (MODSUB_POW2(ini->max_seq, seq, SEQNUM_MAX) < ini->ba_wsize) {
			bar_seq = MODINC_POW2(seq, SEQNUM_MAX);
			/* check if there is another bar with advence seq no */
			if (!ini->bar_ackpending || IS_SEQ_ADVANCED(bar_seq,
				ini->barpending_seq)) {
				ini->barpending_seq = bar_seq;
				wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
			}
		}
#if defined(WLVSDB) && defined(PROP_TXSTATUS)
		} else {
			/* adjust for tx_in_transit reduction in ini_adjust() */
			ini->tx_in_transit++;
			wlc_ampdu_mchan_ini_adjust(ampdu_tx, scb, p);
		}
#endif
	}
	/*
	 * Check whether the packets on tx side is less than watermark level
	 * and disable flow
	 */
	wlc_ampdu_txflowcontrol(ampdu_tx->wlc, scb_ampdu, ini);
	return;
}
#endif /* WLAMPDU_MAC */


static void
wlc_ampdu_dotxstatus_regmpdu_noaqm(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint16 indx, seq;

	ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

	if (!scb || !SCB_AMPDU(scb))
		return;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[PKTPRIO(p)];
	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU_ERR(("wl%d: ampdu_dotxstatus_regmpdu: NULL ini or pon state for tid %d\n",
			ampdu_tx->wlc->pub->unit, PKTPRIO(p)));
		return;
	}
	ASSERT(ini->scb == scb);

	seq = pkt_txh_seqnum(ampdu_tx->wlc, p);
	if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >= ini->ba_wsize) {
		WL_ERROR(("wl%d: %s: unexpected completion: seq 0x%x, "
			"start seq 0x%x\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__, seq, ini->start_seq));
		return;
	}

	indx = TX_SEQ_TO_INDEX(seq);
	if (!isset(ini->ackpending, indx)) {
		WL_ERROR(("wl%d: %s: seq 0x%x\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__, seq));
		ampdu_dump_ini(ini);
		ASSERT(isset(ini->ackpending, indx));
		return;
	}

	if (txs->status.was_acked) {
		clrbit(ini->ackpending, indx);
		if (isset(ini->barpending, indx)) {
			clrbit(ini->barpending, indx);
		}
		ini->txretry[indx] = 0;
		ini->tx_in_transit--;
		wlc_ampdu_ini_move_window(ampdu_tx, SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb), ini);
	} else {
		WLCNTINCR(ampdu_tx->cnt->txreg_noack);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.txreg_noack);
		WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus_regmpdu: ack not recd for "
			"seq 0x%x tid %d status 0x%x\n",
			ampdu_tx->wlc->pub->unit, seq, ini->tid, txs->status.raw_bits));

#if defined(WLVSDB) && defined(PROP_TXSTATUS)
		if (!(MCHAN_ACTIVE(ampdu_tx->wlc->pub) &&
			(PROP_TXSTATUS_ENAB(ampdu_tx->wlc->pub)) &&
			ampdu_tx->wlc->txfifo_detach_pending)) {
#endif

		/* suppressed pkts are resent; so dont move window */
		if ((AP_ENAB(ampdu_tx->wlc->pub) &&
			(txs->status.suppr_ind == TX_STATUS_SUPR_PMQ)) ||
#ifdef WLP2P
			P2P_ABS_SUPR(ampdu_tx->wlc, txs->status.suppr_ind) ||
#endif
			FALSE) {
			ini->txretry[indx]++;
			ASSERT(ini->retry_cnt < ampdu_tx->ba_max_tx_wsize);
			ASSERT(ini->retry_seq[ini->retry_tail] == 0);
			ini->retry_seq[ini->retry_tail] = seq;
			ini->retry_tail = NEXT_TX_INDEX(ini->retry_tail);
			ini->retry_cnt++;

			return;
		}

		/* send bar to move the window */
		setbit(ini->barpending, indx);
		wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
#if defined(WLVSDB) && defined(PROP_TXSTATUS)
		} else {
			wlc_ampdu_mchan_ini_adjust(ampdu_tx, scb, p);
		}
#endif /* WLVSDB && PROP_TXSTATUS */
	}

	/* Check whether the packets on tx side is less than watermark level and disable flow */
	wlc_ampdu_txflowcontrol(ampdu_tx->wlc, scb_ampdu, ini);
}

/* function to process tx completion for a reg mpdu */
void
wlc_ampdu_dotxstatus_regmpdu(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	void *p, tx_status_t *txs)
{
#ifdef WLAMPDU_MAC
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub))
		wlc_ampdu_dotxstatus_regmpdu_aqm(ampdu_tx, scb, p, txs);
	else
#endif /*  WLAMPDU_MAC */
		wlc_ampdu_dotxstatus_regmpdu_noaqm(ampdu_tx, scb, p, txs);
}


static void
wlc_ampdu_free_chain(ampdu_tx_info_t *ampdu_tx, void *p, tx_status_t *txs, bool txs3_done)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	uint16 mcl = 0;
	uint8 queue;
	bool host_enab = AMPDU_HOST_ENAB(wlc->pub), done = FALSE;
#ifdef WLAMPDU_MAC
	uint16 ncons = 0, count = 0;
	if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub))
		ncons = wlc_ampdu_rawbits_to_ncons(txs->status.raw_bits);
	else
		ncons = txs->sequence;
#endif

	WL_AMPDU_TX(("wl%d: wlc_ampdu_free_chain: free ampdu_tx chain\n", wlc->pub->unit));

	queue = txs->frameid & TXFID_QUEUE_MASK;

	/* loop through all pkts in the dma ring and free */
	while (p) {

		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

		if (host_enab) {
			mcl = ltoh16(wlc_pkt_get_mcl(wlc, p));
			/* if not the last ampdu, take out the next packet from dma chain */
			if (((mcl & TXC_AMPDU_MASK) >> TXC_AMPDU_SHIFT) == TXC_AMPDU_LAST) {
				done = TRUE;
			}
		} else {
#ifdef WLAMPDU_MAC
			/* if ucode/hw agg, ncons marks where we stop */
			wlc_txfifo_complete(wlc, queue, 1);
			count ++;
			if (count == ncons) {
				done = TRUE;
			}
#endif
		}

		WLCNTINCR(ampdu_tx->cnt->orphan);
		PKTFREE(wlc->osh, p, TRUE);
		if (done) {
			break;
		} else {
			p = GETNEXTTXP(wlc, queue);
		}
	}

	if (host_enab)
		wlc_txfifo_complete(wlc, queue, ampdu_tx->txpkt_weight);
#ifdef WLAMPDU_MAC
	else {
		WL_AMPDU_TX(("%s: fifo %d cons %d txpktpend %d\n",
			__FUNCTION__, queue, ncons, TXPKTPENDGET(wlc, queue)));
	}
#endif

	/* for corerev >= 40, all txstatus have been read already */
	if (D11REV_GE(wlc->pub->corerev, 40))
		return;

#ifdef WLC_LOW
	/* retrieve the next status if it is there */
	if (txs->status.was_acked) {
		uint32 s1;
		uint8 status_delay = 0;

		ASSERT(txs->status.is_intermediate);

		/* wait till the next 8 bytes of txstatus is available */
		while (((s1 = R_REG(wlc->osh, &wlc->regs->frmtxstatus)) & TXS_V)
		       == 0) {
			OSL_DELAY(1);
			status_delay++;
			if (status_delay > 10) {
				ASSERT(0);
				return;
			}
		}

		ASSERT(!(s1 & TX_STATUS_INTERMEDIATE));
		ASSERT(s1 & TX_STATUS_AMPDU);

#ifdef WLAMPDU_HW
		if (!txs3_done && AMPDU_HW_ENAB(wlc->pub)) {
			/* yet another txs to retrieve */
			status_delay = 0;
			while (((s1 = R_REG(wlc->osh, &wlc->regs->frmtxstatus)) & TXS_V) == 0) {
				OSL_DELAY(1);
				status_delay++;
				if (status_delay > 10) {
					ASSERT(0);
					return;
				}
			}
		}
#endif /* WLAMPDU_HW */
	}
#endif /* WLC_LOW */
}

bool BCMFASTPATH
wlc_ampdu_dotxstatus(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p, tx_status_t *txs,
	wlc_txh_info_t *txh_info)
{
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc = ampdu_tx->wlc;
	scb_ampdu_tid_ini_t *ini;
	uint32 s1 = 0, s2 = 0, s3 = 0, s4 = 0;
#ifdef WLC_LOW
	/* For low driver, we don't want this function to return TRUE for certain err conditions
	 * returning true will cause a wl_init() to get called.
	 * we want to reserve wl_init() call for hw related err conditions.
	 */
	bool need_reset = FALSE;
#else
	/* For high only driver, we need to return TRUE for all error conditions. */
	bool need_reset = TRUE;
#endif /* WLC_LOW */

	ASSERT(p);
	ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

#ifdef WLAMPDU_MAC
	/* update aggfifo/side_channel upfront in case we bail out early */
	if (AMPDU_MAC_ENAB(wlc->pub) && !AMPDU_AQM_ENAB(wlc->pub)) {
		ampdumac_info_t *hagg;
		uint16 max_entries = 0;
		uint16 queue, ncons;

		queue = txs->frameid & TXFID_QUEUE_MASK;
		ASSERT(queue < AC_COUNT);
		hagg = &(ampdu_tx->hagg[queue]);
		ncons = txs->sequence;
#ifdef WLAMPDU_HW
		if (AMPDU_HW_ENAB(wlc->pub)) {
			max_entries = ampdu_tx->aggfifo_depth + 1;
		} else
#endif
		{
#ifdef WLAMPDU_UCODE
			max_entries = hagg->txfs_addr_end - hagg->txfs_addr_strt + 1;
			hagg->txfs_rptr += ncons;
			if (hagg->txfs_rptr > hagg->txfs_addr_end)
				hagg->txfs_rptr -= max_entries;
#endif
		}
		ASSERT(ncons > 0 && ncons < max_entries && ncons <= hagg->in_queue);
		BCM_REFERENCE(max_entries);
		hagg->in_queue -= ncons;
	}
#endif /* WLAMPDU_MAC */

	if (!scb || !SCB_AMPDU(scb)) {
		if (!scb)
			WL_ERROR(("wl%d: %s: scb is null\n",
				wlc->pub->unit, __FUNCTION__));

		wlc_ampdu_free_chain(ampdu_tx, p, txs, FALSE);
		return need_reset;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	ini = scb_ampdu->ini[(uint8)PKTPRIO(p)];

	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		wlc_ampdu_free_chain(ampdu_tx, p, txs, FALSE);
		WL_ERROR(("wl%d: %s: bail: bad ini (%p) or ba_state (%d)\n",
			wlc->pub->unit, __FUNCTION__, ini, (ini ? ini->ba_state : -1)));
		return need_reset;
	}

	ASSERT(ini->scb == scb);
	ASSERT(WLPKTTAGSCBGET(p) == scb);

#ifdef WLAMPDU_MAC
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		wlc_ampdu_dotxstatus_aqm_complete(ampdu_tx, scb, p, txs, txh_info);
		wlc_ampdu_txflowcontrol(wlc, scb_ampdu, ini);
#ifdef BCMDBG
		WLC_AMPDU_TXQ_PROF_ADD_ENTRY(ampdu_tx->wlc, scb_ampdu->scb);
#endif
		return FALSE;
	}
#endif /* WLAMPDU_MAC */

	/* BMAC_NOTE: For the split driver, second level txstatus comes later
	 * So if the ACK was received then wait for the second level else just
	 * call the first one
	 */
	if (txs->status.was_acked) {
#ifdef WLC_LOW
		uint8 status_delay = 0;

		scb->used = wlc->pub->now;
		/* wait till the next 8 bytes of txstatus is available */
		while (((s1 = R_REG(wlc->osh, &wlc->regs->frmtxstatus)) & TXS_V) == 0) {
			OSL_DELAY(1);
			status_delay++;
			if (status_delay > 10) {
				ASSERT(0);
				wlc_ampdu_free_chain(ampdu_tx, p, txs, FALSE);
				return TRUE;
			}
		}

		WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: 2nd status in delay %d\n",
			wlc->pub->unit, status_delay));

		ASSERT(!(s1 & TX_STATUS_INTERMEDIATE));
		ASSERT(s1 & TX_STATUS_AMPDU);

		s2 = R_REG(wlc->osh, &wlc->regs->frmtxstatus2);

#ifdef WLAMPDU_HW
		if (AMPDU_HW_ENAB(wlc->pub)) {
			/* wait till the next 8 bytes of txstatus is available */
			status_delay = 0;
			while (((s3 = R_REG(wlc->osh, &wlc->regs->frmtxstatus)) & TXS_V) == 0) {
				OSL_DELAY(1);
				status_delay++;
				if (status_delay > 10) {
					ASSERT(0);
					wlc_ampdu_free_chain(ampdu_tx, p, txs, TRUE);
					return TRUE;
				}
			}
			s4 = R_REG(wlc->osh, &wlc->regs->frmtxstatus2);
		}
#endif /* WLAMPDU_HW */
#else
		scb->used = wlc->pub->now;

		/* Store the relevant information in ampdu structure */
		WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: High Recvd \n", wlc->pub->unit));

		ASSERT(!ampdu_tx->p);
		ampdu_tx->p = p;
		bcopy(txs, &ampdu_tx->txs, sizeof(tx_status_t));
		ampdu_tx->waiting_status = TRUE;
		return FALSE;
#endif /* WLC_LOW */
	}

	wlc_ampdu_dotxstatus_complete(ampdu_tx, scb, p, txs, s1, s2, s3, s4);
	wlc_ampdu_txflowcontrol(wlc, scb_ampdu, ini);

	return FALSE;
}

#ifdef WLC_HIGH_ONLY
void
wlc_ampdu_txstatus_complete(ampdu_tx_info_t *ampdu_tx, uint32 s1, uint32 s2)
{
	WL_AMPDU_TX(("wl%d: wlc_ampdu_txstatus_complete: High Recvd 0x%x 0x%x p:%p\n",
	          ampdu_tx->wlc->pub->unit, s1, s2, ampdu_tx->p));

	ASSERT(ampdu_tx->waiting_status);

	/* The packet may have been freed if the SCB went away, if so, then still free the
	 * DMA chain
	 */
	if (ampdu_tx->p) {
		wlc_ampdu_dotxstatus_complete(ampdu_tx, WLPKTTAGSCBGET(ampdu_tx->p), ampdu_tx->p,
			&ampdu_tx->txs, s1, s2, 0, 0);
		ampdu_tx->p = NULL;
	}

	ampdu_tx->waiting_status = FALSE;
}
#endif /* WLC_HIGH_ONLY */

#ifdef WLMEDIA_TXFAILEVENT
static void BCMFASTPATH
wlc_tx_failed_event(wlc_info_t *wlc, struct scb *scb, wlc_bsscfg_t *bsscfg, tx_status_t *txs,
	void *p, uint32 flags)
{
	txfailinfo_t txinfo;
	d11txh_t *txh = NULL;
	struct dot11_header *h;
	uint16 fc;
	char *p1;

	/* Just in case we need events only for certain clients or
	 * additional information is needed in the future
	 */
	UNUSED_PARAMETER(scb);
	UNUSED_PARAMETER(flags);

	memset(&txinfo, 0, sizeof(txfailinfo_t));
	wlc_read_tsf(wlc, &txinfo.tsf_l, &txinfo.tsf_h);

	if (txs)
		txinfo.txstatus = txs->status;

	if (p) {
		txinfo.prio  = PKTPRIO(p);
		txh = (d11txh_t *)PKTDATA(wlc->osh, p);
		if (txh)
			txinfo.rates = ltoh16(txh->MainRates);
		p1 = (char*) PKTDATA(wlc->osh, p)
			 + sizeof(d11txh_t) + D11_PHY_HDR_LEN;
		h = (struct dot11_header*)(p1);
		fc = ltoh16(h->fc);
		if ((fc & FC_TODS))
			memcpy(&txinfo.dest, h->a3.octet, ETHER_ADDR_LEN);
		else if ((fc & FC_FROMDS))
			memcpy(&txinfo.dest, h->a1.octet, ETHER_ADDR_LEN);
	}

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_TXFAIL, (struct ether_addr *)txh->TxFrameRA,
		WLC_E_STATUS_TIMEOUT, 0, 0, &txinfo, sizeof(txinfo));


} /* wlc_tx_failed_event */

#endif /* WLMEDIA_TXFAILEVENT */

#ifdef WLAMPDU_MAC
static void BCMFASTPATH
wlc_ampdu_dotxstatus_aqm_complete(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
                                  void *p, tx_status_t *txs, wlc_txh_info_t *txh_info)
{
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc = ampdu_tx->wlc;
	scb_ampdu_tid_ini_t *ini;
	wlc_bsscfg_t *bsscfg;

	uint8 queue, tid, tot_mpdu = 0;

	/* per txstatus info */
	uint ncons, supr_status;
	bool update_rate, retry = FALSE;
	bool fix_rate = FALSE;

	/* per packet info */
	bool acked = FALSE, send_bar = FALSE;
	uint16 seq = 0, bar_seq = 0;

	/* for first packet */
	int rnum = 0;
	uint8 mcs[D11AC_MFBR_NUM], antselid = 0;
	uint16 start_seq = 0xFFFF; /* invalid */

#ifdef WLPKTDLYSTAT
	uint8 ac;
	uint32 delay, now;
	scb_delay_stats_t *delay_stats;
#endif

#if (defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU))
	int k;
#endif
	uint16 tx_cnt[D11AC_MFBR_NUM] = {0, 0, 0, 0};
	uint16 txsucc_cnt[D11AC_MFBR_NUM] = {0, 0, 0, 0};
#ifdef PKTQ_LOG
	int prec_index;
	pktq_counters_t *prec_cnt;
	pktq_counters_t *actq_cnt;
	pktq_counters_t *prec_bytes;
	pktq_counters_t *actq_bytes;
	uint32           prec_pktlen;
#endif
#ifdef AP
	bool ps_retry = FALSE;
	bool relist = FALSE;
	bool pps_recvd_ack = FALSE;
#else
	const bool relist = FALSE;
#endif
#if (defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU))
	uint8 vht[D11AC_MFBR_NUM];
	bool is_vht[D11AC_MFBR_NUM];
	bool is_stbc[D11AC_MFBR_NUM];
	bool is_sgi[D11AC_MFBR_NUM];

	bzero(is_vht, sizeof(is_vht));
	bzero(is_stbc, sizeof(is_stbc));
	bzero(is_sgi, sizeof(is_sgi));
#endif

	BCM_REFERENCE(start_seq);
	BCM_REFERENCE(bsscfg);

	ncons = wlc_ampdu_rawbits_to_ncons(txs->status.raw_bits);
	ASSERT(ncons > 0);
	WLCNTADD(ampdu_tx->cnt->cons, ncons);

	if (!scb || !SCB_AMPDU(scb)) {
		if (!scb)
			WL_ERROR(("wl%d: %s: scb is null\n",
				wlc->pub->unit, __FUNCTION__));

		wlc_ampdu_free_chain(ampdu_tx, p, txs, FALSE);
		return;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	bsscfg = SCB_BSSCFG(scb);

	ASSERT(scb_ampdu);
	ASSERT(p);
	tid = (uint8)PKTPRIO(p);

	queue = txs->frameid & TXFID_QUEUE_MASK;
	ASSERT(queue < AC_COUNT);

#ifdef WLPKTDLYSTAT
	ac = WME_PRIO2AC(tid);
	delay_stats = &scb->delay_stats[ac];
#endif

/*
2 	RTS tx count
3 	CTS rx count
11:4 	Acked bitmap for each of consumed mpdus reported in this txstatus, sequentially.
That is, bit 0 is for the first consumed mpdu, bit 1 for the second consumed frame.
*/
	ini = scb_ampdu->ini[tid];

	if (!ini) {
		WL_ERROR(("%s: bad ini error\n", __FUNCTION__));
		wlc_ampdu_free_chain(ampdu_tx, p, txs, FALSE);
		return;
	}

	update_rate = (WLPKTTAG(p)->flags & WLF_RATE_AUTO) ? TRUE : FALSE;
	supr_status = txs->status.suppr_ind;

	if (supr_status) {
		update_rate = FALSE;
#if defined(BCMDBG) || defined(BCMDBG_AMPDU)
		ampdu_tx->amdbg->supr_reason[supr_status] += ncons;
#endif
		WL_AMPDU_TX(("wl%d: %s: supr_status 0x%x\n",
			wlc->pub->unit, __FUNCTION__, supr_status));
		if (supr_status == TX_STATUS_SUPR_BADCH) {
			/* no need to retry for badch; will fail again */
			WLCNTADD(wlc->pub->_cnt->txchanrej, ncons);
		} else if (supr_status == TX_STATUS_SUPR_EXPTIME) {
			WLCNTADD(wlc->pub->_cnt->txexptime, ncons);
			/* Interference detected */
			if (wlc->rfaware_lifetime)
				wlc_exptime_start(wlc);
		} else if (supr_status == TX_STATUS_SUPR_FRAG) {
			WL_ERROR(("wl%d: %s: tx underflow?!\n", wlc->pub->unit, __FUNCTION__));
		} else if (supr_status == TX_STATUS_SUPR_PMQ) {
			/* Need to requeue the packet */
			retry = TRUE;
			WL_NONE(("wl%d: %s: PMQ suppression\n", wlc->pub->unit, __FUNCTION__));
		}
#ifdef AP
		else if (supr_status == TX_STATUS_SUPR_PPS) {
			/* Need to requeue the packet due to PS Pretend */
			retry = TRUE;
		}
#endif /* AP */

#ifdef WLP2P
		else if (P2P_ABS_SUPR(wlc, supr_status)) {
			retry = TRUE;
		}
#endif
	} else if (txs->phyerr) {
		update_rate = FALSE;
		WLCNTADD(wlc->pub->_cnt->txphyerr, ncons);
		WL_ERROR(("wl%d: %s: tx phy error (0x%x)\n",
			wlc->pub->unit, __FUNCTION__, txs->phyerr));
	}


	/* Check if interference is still there */
	if (wlc->rfaware_lifetime && wlc->exptime_cnt && (supr_status != TX_STATUS_SUPR_EXPTIME))
		wlc_exptime_check_end(wlc);

#ifdef WLPKTDLYSTAT
	/* Get the current time */
	now = WLC_GET_CURR_TIME(wlc);
#endif

	/* loop through all pkts -- update internal data structs */
	while (tot_mpdu < ncons) {

		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

		seq = WLPKTTAG(p)->seq;

#ifdef PKTQ_LOG
		prec_index = (WLPKTTAG(p)->flags2 & WLF2_FAVORED) ?
		              WLC_PRIO_TO_HI_PREC(tid) : WLC_PRIO_TO_PREC(tid);
		prec_cnt = &scb_ampdu->txq._prec_cnt[prec_index];
		actq_cnt = &wlc->active_queue->q._prec_cnt[prec_index];
		prec_bytes = &scb_ampdu->txq._prec_bytes[prec_index];
		actq_bytes = &wlc->active_queue->q._prec_bytes[prec_index];
		prec_pktlen = txh_info->d11FrameSize - DOT11_LLC_SNAP_HDR_LEN -
		              DOT11_MAC_HDR_LEN - DOT11_QOS_LEN;

		if (supr_status) {
			WLCNTINCR(prec_cnt->suppress);
			WLCNTINCR(actq_cnt->suppress);
		}
#endif
		if (tot_mpdu == 0) {
			bool last_rate;
			uint16 ft;
			d11actxh_rate_t *rinfo;

#ifdef BCMDBG
			if (txs->frameid != htol16(txh_info->TxFrameID)) {
				WL_ERROR(("wl%d: %s: txs->frameid 0x%x txh->TxFrameID 0x%x\n",
					wlc->pub->unit, __FUNCTION__,
					txs->frameid, htol16(txh_info->TxFrameID)));
				ASSERT(txs->frameid == htol16(txh_info->TxFrameID));
			}
#endif
			start_seq = seq;
			fix_rate = ltoh16(txh_info->MacTxControlHigh) & D11AC_TXC_FIX_RATE;
			rnum = 0;
			do {
				rinfo = &txh_info->hdrPtr->txd.RateInfo[rnum];
				ft = ltoh16(rinfo->PhyTxControlWord_0) & PHY_TXC_FT_MASK;
				mcs[rnum] = ltoh16(rinfo->PhyTxControlWord_2) &
					D11AC_PHY_TXC_PHY_RATE_MASK;
				if (ft == FT_VHT) {
					/* VHT rate */
					uint8 nss, modu;
					modu = mcs[rnum] & 0xf;
					nss = (mcs[rnum] >> 4) & 0xf;

#if (defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU))
					is_vht[rnum] = TRUE;
					vht[rnum] = nss * 10 + modu;
					is_stbc[rnum] =
						(rinfo->plcp[0] & VHT_SIGA1_STBC) ? TRUE : FALSE;
					is_sgi[rnum] = VHT_PLCP3_ISSGI(rinfo->plcp[3]);
#endif
					mcs[rnum] = 0x80 | ((nss + 1) << 4) | modu;
				}
#if (defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU))
				else {
					is_stbc[rnum] = PLCP3_ISSTBC(rinfo->plcp[3]);
					is_sgi[rnum] = PLCP3_ISSGI(rinfo->plcp[3]);
				}
#endif
				last_rate = (ltoh16(rinfo->RtsCtsControl) &
				             D11AC_RTSCTS_LAST_RATE) ?
					TRUE : FALSE;

				rnum ++;
			} while (!last_rate && rnum < D11AC_MFBR_NUM);

			if (WLANTSEL_ENAB(wlc))
				antselid = wlc_antsel_antsel2id(wlc->asi, txh_info->w.asp.AntSel);
		}

		/* can we make this more compact? */
		acked = wlc_txs_was_acked(wlc, &(txs->status), tot_mpdu);

		/* Dont update tx_in_transmit while re-queueing for PMQ or absence
		 * suppression.
		 */
		if (!(retry && (supr_status == TX_STATUS_SUPR_PMQ ||
#ifdef AP
			(supr_status == TX_STATUS_SUPR_PPS) ||
#endif
#ifdef WLP2P
			P2P_ABS_SUPR(wlc, supr_status) ||
#endif
			FALSE))) {
			ini->tx_in_transit--;
		}

		if (acked) {
			WL_AMPDU_TX(("wl%d: %s pkt ack seq 0x%04x idx %d\n",
				wlc->pub->unit, __FUNCTION__, seq, tot_mpdu));
			ini->acked_seq = seq;

			/* Already acked, no need to retry */
			retry = FALSE;

#ifdef PKTQ_LOG
			WLCNTINCR(prec_cnt->acked);
			WLCNTINCR(actq_cnt->acked);
			WLCNTADD(prec_bytes->acked, prec_pktlen);
			WLCNTADD(actq_bytes->acked, prec_pktlen);
#endif
#ifdef AP
			pps_recvd_ack = TRUE;
#endif

			/* call any matching pkt callbacks */
			if (WLPKTTAG(p)->callbackidx) {
				WL_AMPDU_TX(("%s: calling pkt callback\n", __FUNCTION__));

				wlc_pkt_callback((void *)wlc, p, TX_STATUS_ACK_RCV);
			}
		}

#ifdef AP
		/* relist means that if the packet failed, we save it with ps pretend */
		relist = (SCB_PS_PRETEND_ENABLED(wlc, scb) || SCB_PS_PRETEND_THRESHOLD(scb)) &&
		         !SCB_PS_PRETEND_NORMALPS(scb);

		if (!acked && relist) {
			uint16	macCtlHigh = ltoh16(txh_info->MacTxControlHigh);

			if (!(macCtlHigh & D11AC_TXC_PPS) && !SCB_PS_PRETEND_THRESHOLD(scb)) {
				/* packet does not have PPS bit set or we are not supporting
				 * pps threshold option. We normally get here if we previously
				 * exceeded the pspretend_retry_limit and the packet subsequently
				 * fails to be sent
				 */
				relist = FALSE;
				retry = FALSE;
			}
			else {
				d11actxh_t* vhtHdr = &(txh_info->hdrPtr->txd);

				retry = TRUE;
#ifdef PKTQ_LOG
				if (!supr_status) {
					WLCNTINCR(prec_cnt->ps_retry);
					WLCNTINCR(actq_cnt->ps_retry);
				}
#endif
				/* in case we haven't yet processed PMQ status change because of
				 * previously blocked data path or delayed PMQ interrupt
				 */
				if (supr_status == TX_STATUS_SUPR_PPS && !SCB_PS(scb)) {
					WL_PS(("wl%d: packet returned TX_STATUS_SUPR_PPS "
						   "in PS OFF State\n", wlc->pub->unit));
				}

				ps_retry = TRUE;
				wlc_apps_scb_pspretend_on(wlc, scb, 0);

				/* check to see if successive pps transitions exceed limit */
				if (scb->ps_pretend_succ_count >= wlc->ps_pretend_retry_limit &&
				                !SCB_PS_PRETEND_THRESHOLD(scb)) {

					/* The retry limit has been hit, so clear the PPS bit so
					 * they no longer trigger another ps pretend.
					 */

					macCtlHigh &= ~D11AC_TXC_PPS;
					vhtHdr->PktInfo.MacTxControlHigh = htol16(macCtlHigh);
				}
				if ((WLPKTTAG(p)->flags & WLF_TXHDR) &&
				         !(macCtlHigh & D11AC_TXC_FIX_RATE))
				{
				}

				/* Mark as retrieved from HW FIFO */
				WLPKTTAG(p)->flags |= WLF_FIFOPKT;
				WLPKTTAGSCBSET(p, scb);
				SCB_TX_NEXT(TXMOD_AMPDU, scb, p, WLC_PRIO_TO_HI_PREC(tid));
				p = NULL;
			}
		}
#endif /* AP */

		/* either retransmit or send bar if ack not recd */
		if (!acked && !relist) {

			if (retry && (supr_status == TX_STATUS_SUPR_PMQ ||
#ifdef WLP2P
				P2P_ABS_SUPR(wlc, supr_status) ||
#endif
				FALSE)) {
				/* Mark as retrieved from HW FIFO */
				WLPKTTAG(p)->flags |= WLF_FIFOPKT;
				WLPKTTAGSCBSET(p, scb);
				SCB_TX_NEXT(TXMOD_AMPDU, scb, p, WLC_PRIO_TO_HI_PREC(tid));
				p = NULL;
			} else {
				bar_seq = seq;
				send_bar = TRUE;
				retry = FALSE;

				WLPKTTAGSCBSET(p, scb);
				WL_AMPDU_TX(("wl%d: %s: pkt seq 0x%04x not acked\n",
					wlc->pub->unit, __FUNCTION__, seq));

#ifdef WLMEDIA_TXFAILEVENT
				wlc_tx_failed_event(wlc, scb, bsscfg, txs, p, 0);
#endif /* WLMEDIA_TXFAILEVENT */

#ifdef PKTQ_LOG
				WLCNTINCR(prec_cnt->retry_drop);
				WLCNTINCR(actq_cnt->retry_drop);
				WLCNTADD(prec_bytes->retry_drop, prec_pktlen);
				WLCNTADD(actq_bytes->retry_drop, prec_pktlen);
#endif
			}
		}

#ifdef WLPKTDLYSTAT
		/* calculate latency and packet loss statistics */
		if (!retry) {
			/* Ignore wrap-around case and error case */
			if (now > WLPKTTAG(p)->shared.enqtime) {
				uint tr = 0;

				delay = (now - WLPKTTAG(p)->shared.enqtime);
				wlc_delay_stats_upd(delay_stats, delay, tr, acked);
				WL_AMPDU_STAT(("Enq %d retry %d cnt %d: acked %d delay/min/"
					"max/sum %d %d %d %d\n", WLPKTTAG(p)->shared.enqtime,
					tr, delay_stats->txmpdu_cnt[tr], acked, delay,
					delay_stats->delay_min, delay_stats->delay_max,
					delay_stats->delay_sum[tr]));
			}
		}
#endif /* WLPKTDLYSTAT */

		if (!retry) {
#ifdef WLAMSDU
			if (SCB_AMSDU(scb))
				wlc_amsdu_dotxstatus(wlc->ami, scb, p);
#endif /* WLAMSDU */
			PKTFREE(wlc->osh, p, TRUE);
			p = NULL;
		}
		wlc_txfifo_complete(wlc, queue, 1);
		tot_mpdu++;

		if (p) {
			PKTFREE(wlc->osh, p, TRUE);
			p = NULL;
		}

		/* only loop ncons times */
		if (tot_mpdu >= ncons) {
			break;
		}

		/* get next pkt -- check and handle NULL */
		p = GETNEXTTXP(wlc, queue);

#ifdef PROP_TXSTATUS
		if (p && PROP_TXSTATUS_ENAB(wlc->pub)) {
			uint8 wlfc_status =  WLFC_D11_STATUS_INTERPRET(txs);
			if (scb && (wlfc_status == WLFC_CTL_PKTFLAG_D11SUPPRESS)) {
				wlc_suppress_sync_fsm(wlc, scb, p, TRUE);
				wlc_process_wlhdr_txstatus(wlc, wlfc_status, p, FALSE);
			} else {
				if (tot_mpdu < (ncons - 1)) {
					wlc_process_wlhdr_txstatus(wlc,
						wlfc_status, p, TRUE);
				} else {
					wlc_process_wlhdr_txstatus(wlc,
						wlfc_status, p, FALSE);
				}
			}
	/* TODO: p2p_set_abs_intr_status */
		}
#endif /* PROP_TXSTATUS */
		/* if tot_mpdu < ncons, should have pkt in queue */
		ASSERT(p != NULL);
	} /* while p */

#ifdef PKTQ_LOG
	/* cannot distinguish hi or lo prec hereafter - so just standardise lo prec */
	prec_index = WLC_PRIO_TO_PREC(tid);
	prec_cnt = &scb_ampdu->txq._prec_cnt[prec_index];
	actq_cnt = &wlc->active_queue->q._prec_cnt[prec_index];
#endif

	if (send_bar) {
		if  ((MODSUB_POW2(ini->max_seq, bar_seq, SEQNUM_MAX) >= ini->ba_wsize) &&
			(MODSUB_POW2(bar_seq, (ini->acked_seq), SEQNUM_MAX) < ini->ba_wsize * 2)) {
			send_bar = FALSE;
			WL_AMPDU(("wl%d: %s: skipping BAR %x, max_seq %x, acked_seq %x\n",
				wlc->pub->unit, __FUNCTION__, MODINC_POW2(seq, SEQNUM_MAX),
				ini->max_seq, ini->acked_seq));
		} else if (ini->bar_ackpending) {
			/*
			 * check if no bar with newer seq has been send out due to other condition,
			 * like unexpected tx seq
			 */
			if (!IS_SEQ_ADVANCED(MODINC_POW2(bar_seq, SEQNUM_MAX), ini->barpending_seq))
				send_bar = FALSE;
		}

		if (send_bar) {
			bar_seq = MODINC_POW2(seq, SEQNUM_MAX);
			ini->barpending_seq = bar_seq;
			wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);
		}
	}

#ifdef AP
	if (BSSCFG_AP(bsscfg)) {
		wlc_apps_process_pspretend_status(wlc, scb, pps_recvd_ack, ps_retry);
	}
#endif /* AP */

	/* bump up the start seq to move the window */
	wlc_ampdu_ini_move_window(ampdu_tx, scb_ampdu, ini);

	WL_AMPDU_TX(("wl%d: %s: ncons %d tot_mpdus %d start_seq 0x%04x\n",
		wlc->pub->unit, __FUNCTION__, ncons, tot_mpdu, start_seq));

#ifdef STA
	/* PM state change */
	wlc_update_pmstate(bsscfg, (uint)(wlc_txs_alias_to_old_fmt(wlc, &(txs->status))));
#endif /* STA */

#ifdef PKTQ_LOG
	WLCNTADD(prec_cnt->rtsfail, txs->status.rts_tx_cnt - txs->status.cts_rx_cnt);
	WLCNTADD(actq_cnt->rtsfail, txs->status.rts_tx_cnt - txs->status.cts_rx_cnt);
#endif

	if (fix_rate) {
		/* if using fix rate, retrying 64 mpdus >=4 times can overflow 8-bit cnt.
		 * So ucode treats fix rate specially.
		 */
		tx_cnt[0]     = txs->status.s3 & 0xffff;
		txsucc_cnt[0] = (txs->status.s3 >> 16) & 0xffff;
#ifdef PKTQ_LOG
		WLCNTADD(prec_cnt->retry, tx_cnt[0] - txsucc_cnt[0]);
		WLCNTADD(actq_cnt->retry, tx_cnt[0] - txsucc_cnt[0]);
#endif
	} else {
#ifdef PKTQ_LOG
		uint32    retry_count;
#endif
		tx_cnt[0]     = txs->status.s3 & 0xff;
		txsucc_cnt[0] = (txs->status.s3 >> 8) & 0xff;
		tx_cnt[1]     = (txs->status.s3 >> 16) & 0xff;
		txsucc_cnt[1] = (txs->status.s3 >> 24) & 0xff;
		tx_cnt[2]     = (txs->status.s4 >>  0) & 0xff;
		txsucc_cnt[2] = (txs->status.s4 >>  8) & 0xff;
		tx_cnt[3]     = (txs->status.s4 >> 16) & 0xff;
		txsucc_cnt[3] = (txs->status.s4 >> 24) & 0xff;
#ifdef PKTQ_LOG
		retry_count = tx_cnt[0] + tx_cnt[1] + tx_cnt[2] + tx_cnt[3]
		            - txsucc_cnt[0] - txsucc_cnt[1] - txsucc_cnt[2] - txsucc_cnt[3];
		WLCNTADD(prec_cnt->retry, retry_count);
		WLCNTADD(actq_cnt->retry, retry_count);
#endif
	}
#ifdef BCMDBG
	for (k = 0; k < rnum; k++) {
		if (txsucc_cnt[k] > tx_cnt[k]) {
			WL_ERROR(("assertion: %s: txsucc_cnt[%d] > tx_cnt[%d]: %d > %d\n",
				__FUNCTION__, k, k, txsucc_cnt[k], tx_cnt[k]));
			WL_ERROR(("ncons %d raw txstatus %08X | %04X %04X | %08X %08X || "
				 "%08X | %08X %08X\n", ncons,
				  txs->status.raw_bits, txs->sequence, txs->phyerr,
				  txs->status.s3, txs->status.s4, txs->status.s5,
				  txs->status.ack_map1, txs->status.ack_map2));
			/* ASSERT(txsucc_cnt[k] <= tx_cnt[k]); */
		}
	}
#endif
#if (defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU))
	for (k = 0; k < rnum; k++) {
		if (ampdu_tx->amdbg) {
			if (!is_vht[k])  {
				WLCNTADD(ampdu_tx->amdbg->txmcs[mcs[k]], tx_cnt[k]);
				WLCNTADD(ampdu_tx->amdbg->txmcs_succ[mcs[k]], txsucc_cnt[k]);
			}
#ifdef WL11AC
			else {
				WLCNTADD(ampdu_tx->amdbg->txvht[vht[k]], tx_cnt[k]);
				WLCNTADD(ampdu_tx->amdbg->txvht_succ[vht[k]], txsucc_cnt[k]);
			}
#endif
			if (is_sgi[k]) {
				WLCNTADD(ampdu_tx->cnt->u1.txmpdu_sgi, tx_cnt[k]);
				if (!is_vht[k])
					WLCNTADD(ampdu_tx->amdbg->txmcssgi[mcs[k]], tx_cnt[k]);
#ifdef WL11AC
				else
					WLCNTADD(ampdu_tx->amdbg->txvhtsgi[vht[k]], tx_cnt[k]);
#endif
			}

			if (is_stbc[k]) {
				WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc, tx_cnt[k]);
				if (!is_vht[k])
					WLCNTADD(ampdu_tx->amdbg->txmcsstbc[mcs[k]], tx_cnt[k]);
#ifdef WL11AC
				else
					WLCNTADD(ampdu_tx->amdbg->txvhtstbc[vht[k]], tx_cnt[k]);
#endif
			}
		}
#ifdef WLPKTDLYSTAT
		if (!is_vht[k]) {
			WLCNTADD(ampdu_tx->cnt->txmpdu_cnt[mcs[k]], tx_cnt[k]);
			WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[mcs[k]], txsucc_cnt[k]);
		}
#ifdef WL11AC
		else {
			WLCNTADD(ampdu_tx->cnt->txmpdu_vht_cnt[vht[k]], tx_cnt[k]);
			WLCNTADD(ampdu_tx->cnt->txmpdu_vht_succ_cnt[vht[k]], txsucc_cnt[k]);
		}
#endif /* WL11AC */
#endif /* WLPKTDLYSTAT */
	}
#endif /* BCMDBG || WLTEST */

	if (update_rate) {
		if (tx_cnt[0] > 0xff) tx_cnt[0] = 0xff;
		if (txsucc_cnt[0] > 0xff) txsucc_cnt[0] = 0xff;

		if (tx_cnt[0] == 0 && tx_cnt[1] == 0) {
			tx_cnt[0] = (ncons > 0xff) ? 0xff : (uint8)ncons;
			wlc_scb_ratesel_upd_txs_ampdu(wlc->wrsi, scb, txs->frameid,
				(uint8)tx_cnt[0], 0, 0, 0, FALSE, mcs[0], antselid);

#ifdef AP
			if (ps_retry) {
				WL_PS(("wl%d: "MACF" ps retry with only RTS failure\n",
				        wlc->pub->unit, ETHER_TO_MACF(scb->ea)));
			}
#endif
		} else {
#ifdef AP
			/* we have done successive attempts to send the packet and it is
			 * not working, despite RTS going through. Suspect that the rate
			 * info is wrong so reset it
			 */
			if (SCB_PS_PRETEND(scb) && !SCB_PS_PRETEND_THRESHOLD(scb) &&
				(scb->ps_pretend_succ_count >= wlc->ps_pretend_retry_limit)) {

				WL_PS(("wl%d: "MACF" ps pretend rate adaptation (count %d)\n",
				        wlc->pub->unit, ETHER_TO_MACF(scb->ea),
				        scb->ps_pretend_succ_count));
				wlc_scb_ratesel_clr_gotpkts(wlc->wrsi, scb, txs);
			}
#endif /* AP */
			wlc_scb_ratesel_upd_txs_ampdu(wlc->wrsi, scb, txs->frameid,
				(uint8)tx_cnt[0], (uint8)txsucc_cnt[0], (uint8)tx_cnt[1],
				(uint8)txsucc_cnt[1], FALSE, mcs[0], antselid);
		}
	}
}
#endif /* WLAMPDU_MAC */

static void BCMFASTPATH
wlc_ampdu_dotxstatus_complete(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p, tx_status_t *txs,
	uint32 s1, uint32 s2, uint32 s3, uint32 s4)
{
	scb_ampdu_tx_t *scb_ampdu;
	wlc_info_t *wlc = ampdu_tx->wlc;
	scb_ampdu_tid_ini_t *ini;
	uint8 bitmap[8], queue, tid, requeue = 0;
	d11txh_t *txh;
	uint8 *plcp;
	struct dot11_header *h;
	uint16 seq, start_seq = 0, bindex, indx, mcl;
	uint8 mcs = 0;
	bool ba_recd = FALSE, ack_recd = FALSE, send_bar = FALSE;
	uint8 suc_mpdu = 0, tot_mpdu = 0;
	uint supr_status;
	bool update_rate = TRUE, retry = TRUE;
	int txretry = -1;
	uint16 mimoantsel = 0;
	uint8 antselid = 0;
	uint8 retry_limit, rr_retry_limit;
	wlc_bsscfg_t *bsscfg;
#if defined(AP)
	bool ps_retry = FALSE;
#endif
#ifdef WLPKTDLYSTAT
	uint8 mcs_fbr = 0;
	uint8 ac;
	bool pkt_freed = FALSE;
	uint32 delay, now;
	scb_delay_stats_t *delay_stats;
#endif
#ifdef PKTQ_LOG
	int prec_index;

	pktq_counters_t *prec_cnt;
	pktq_counters_t *actq_cnt;
	pktq_counters_t *prec_bytes;
	pktq_counters_t *actq_bytes;
	uint32           prec_pktlen;
#endif

#ifdef WLAMPDU_MAC
	/* used for ucode/hw agg */
	int pdu_count = 0;
	uint16 first_frameid = 0xffff, fid, ncons = 0;
	uint8 tx_mrt = 0, txsucc_mrt = 0, tx_fbr = 0, txsucc_fbr = 0;
#endif /* WLAMPDU_MAC */

#ifdef BCMDBG
	uint8 hole[AMPDU_MAX_MPDU];
	uint8 idx = 0;
	bzero(hole, sizeof(hole));
#endif

	ASSERT(p && (WLPKTTAG(p)->flags & WLF_AMPDU_MPDU));

	if (!scb || !SCB_AMPDU(scb)) {
		if (!scb)
			WL_ERROR(("wl%d: %s: scb is null\n",
				wlc->pub->unit, __FUNCTION__));

		wlc_ampdu_free_chain(ampdu_tx, p, txs, FALSE);
		return;
	}

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	tid = (uint8)PKTPRIO(p);
#ifdef WLPKTDLYSTAT
	ac = WME_PRIO2AC(tid);
	delay_stats = &scb->delay_stats[ac];
#endif
	ini = scb_ampdu->ini[tid];
	retry_limit = ampdu_tx->retry_limit_tid[tid];
	rr_retry_limit = ampdu_tx->rr_retry_limit_tid[tid];

	if (!ini || (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
		wlc_ampdu_free_chain(ampdu_tx, p, txs, FALSE);
		return;
	}

	ASSERT(ini->scb == scb);
	ASSERT(WLPKTTAGSCBGET(p) == scb);

	bzero(bitmap, sizeof(bitmap));
	queue = txs->frameid & TXFID_QUEUE_MASK;
	ASSERT(queue < AC_COUNT);

	update_rate = (WLPKTTAG(p)->flags & WLF_RATE_AUTO) ? TRUE : FALSE;
	supr_status = txs->status.suppr_ind;

	bsscfg = SCB_BSSCFG(scb);
	ASSERT(bsscfg != NULL);
	BCM_REFERENCE(bsscfg);

#ifdef PKTQ_LOG
	prec_index = (WLPKTTAG(p)->flags2 & WLF2_FAVORED) ?
	             WLC_PRIO_TO_HI_PREC(tid) : WLC_PRIO_TO_PREC(tid);

	prec_cnt = &scb_ampdu->txq._prec_cnt[prec_index];
	actq_cnt = &wlc->active_queue->q._prec_cnt[prec_index];
	prec_bytes = &scb_ampdu->txq._prec_bytes[prec_index];
	actq_bytes = &wlc->active_queue->q._prec_bytes[prec_index];
#endif

#ifdef WLAMPDU_HW
	if (AMPDU_HW_ENAB(wlc->pub)) {
		WL_AMPDU_HWDBG(("%s: cons %d burst 0x%04x aggfifo1_space %d txfifo_cnt %d\n",
			__FUNCTION__, txs->sequence, (s4 >> 16) & 0xffff, /* s4 is overloaded */
			get_aggfifo_space(ampdu_tx, 1),
			R_REG(wlc->osh, &wlc->regs->u.d11regs.xmtfifo_frame_cnt)));
	}
#endif
	if (AMPDU_HOST_ENAB(wlc->pub)) {
		if (txs->status.was_acked) {
			/*
			 * Underflow status is reused  for BTCX to indicate AMPDU preemption.
			 * This prevents un-necessary rate fallback.
			 */
			if (TX_STATUS_SUPR_UF == supr_status) {
				WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: BT preemption, skip rate "
					     "fallback\n", wlc->pub->unit));
				update_rate = FALSE;
			}

#ifdef WLP2P
			if (P2P_ABS_SUPR(wlc, supr_status)) {
				WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: absence period start,"
				             "skip rate fallback\n", wlc->pub->unit));
				update_rate = FALSE;
			}
#endif

			ASSERT(txs->status.is_intermediate);
			start_seq = txs->sequence >> SEQNUM_SHIFT;
			bitmap[0] = (txs->status.raw_bits & TX_STATUS_BA_BMAP03_MASK) >>
				TX_STATUS_BA_BMAP03_SHIFT;

			ASSERT(!(s1 & TX_STATUS_INTERMEDIATE));
			ASSERT(s1 & TX_STATUS_AMPDU);

			bitmap[0] |= (s1 & TX_STATUS_BA_BMAP47_MASK) << TX_STATUS_BA_BMAP47_SHIFT;
			bitmap[1] = (s1 >> 8) & 0xff;
			bitmap[2] = (s1 >> 16) & 0xff;
			bitmap[3] = (s1 >> 24) & 0xff;

			/* remaining 4 bytes in s2 */
			bitmap[4] = s2 & 0xff;
			bitmap[5] = (s2 >> 8) & 0xff;
			bitmap[6] = (s2 >> 16) & 0xff;
			bitmap[7] = (s2 >> 24) & 0xff;

			ba_recd = TRUE;

#if defined(STA) && defined(DBG_BCN_LOSS)
			scb->dbg_bcn.last_tx = wlc->pub->now;
#endif

			WL_AMPDU_TX(("%s: Block ack received: start_seq is 0x%x bitmap "
				"%02x %02x %02x %02x %02x %02x %02x %02x\n",
				__FUNCTION__, start_seq, bitmap[0], bitmap[1], bitmap[2],
				bitmap[3], bitmap[4], bitmap[5], bitmap[6], bitmap[7]));

		}
	}
#ifdef WLAMPDU_MAC
	else {
		/* AMPDU_HW txstatus package */

		ASSERT(txs->status.is_intermediate);

		if (WL_AMPDU_HWTXS_ON()) {
			wlc_print_ampdu_txstatus(ampdu_tx, txs, s1, s2, s3, s4);
		}

		ASSERT(!(s1 & TX_STATUS_INTERMEDIATE));
		start_seq = ini->start_seq;
		ncons = txs->sequence;
		bitmap[0] = (txs->status.raw_bits & TX_STATUS_BA_BMAP03_MASK) >>
			TX_STATUS_BA_BMAP03_SHIFT;
		bitmap[0] |= (s1 & TX_STATUS_BA_BMAP47_MASK) << TX_STATUS_BA_BMAP47_SHIFT;
		bitmap[1] = (s1 >> 8) & 0xff;
		bitmap[2] = (s1 >> 16) & 0xff;
		bitmap[3] = (s1 >> 24) & 0xff;
		ba_recd = bitmap[0] || bitmap[1] || bitmap[2] || bitmap[3];
#ifdef WLAMPDU_HW
		if (AMPDU_HW_ENAB(wlc->pub)) {
			/* 3rd txstatus */
			bitmap[4] = (s3 >> 16) & 0xff;
			bitmap[5] = (s3 >> 24) & 0xff;
			bitmap[6] = s4 & 0xff;
			bitmap[7] = (s4 >> 8) & 0xff;
			ba_recd = ba_recd || bitmap[4] || bitmap[5] || bitmap[6] || bitmap[7];
		}
#endif
		/* remaining 4 bytes in s2 */
		tx_mrt 		= (uint8)(s2 & 0xff);
		txsucc_mrt 	= (uint8)((s2 >> 8) & 0xff);
		tx_fbr 		= (uint8)((s2 >> 16) & 0xff);
		txsucc_fbr 	= (uint8)((s2 >> 24) & 0xff);

		WLCNTINCR(ampdu_tx->cnt->u0.txs);
		WLCNTADD(ampdu_tx->cnt->tx_mrt, tx_mrt);
		WLCNTADD(ampdu_tx->cnt->txsucc_mrt, txsucc_mrt);
		WLCNTADD(ampdu_tx->cnt->tx_fbr, tx_fbr);
		WLCNTADD(ampdu_tx->cnt->txsucc_fbr, txsucc_fbr);

		if (!(txs->status.was_acked)) {

			WL_ERROR(("Status: "));
			if ((txs->status.was_acked) == 0)
				WL_ERROR(("Not ACK_RCV "));
			WL_ERROR(("\n"));
			WL_ERROR(("Total attempts %d succ %d\n", tx_mrt, txsucc_mrt));
		}
	}
#endif /* WLAMPDU_MAC */

	if (!ba_recd || AMPDU_MAC_ENAB(wlc->pub)) {
		if (!ba_recd) {
			WLCNTINCR(ampdu_tx->cnt->noba);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.noba);
			if (AMPDU_MAC_ENAB(wlc->pub)) {
				WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: error txstatus 0x%x\n",
					wlc->pub->unit, txs->status.raw_bits));
			}
		}
		if (supr_status) {
#ifdef PKTQ_LOG
			WLCNTINCR(prec_cnt->suppress);
			WLCNTINCR(actq_cnt->suppress);
#endif
			update_rate = FALSE;
#if defined(BCMDBG) || defined(BCMDBG_AMPDU)
			ampdu_tx->amdbg->supr_reason[supr_status]++;
#endif
			WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: supr_status 0x%x\n",
				wlc->pub->unit, supr_status));
			/* no need to retry for badch; will fail again */
			if (supr_status == TX_STATUS_SUPR_BADCH) {
				retry = FALSE;
				WLCNTINCR(wlc->pub->_cnt->txchanrej);
			} else if (supr_status == TX_STATUS_SUPR_EXPTIME) {

				WLCNTINCR(wlc->pub->_cnt->txexptime);

				/* Interference detected */
				if (wlc->rfaware_lifetime)
					wlc_exptime_start(wlc);
			/* TX underflow : try tuning pre-loading or ampdu size */
			} else if (supr_status == TX_STATUS_SUPR_FRAG) {
				/* if there were underflows, but pre-loading is not active,
				   notify rate adaptation.
				*/
				if (wlc_ffpld_check_txfunfl(wlc, prio2fifo[tid]) > 0) {
#ifdef WLC_HIGH_ONLY
					/* With BMAC, TX Underflows should not happen */
					WL_ERROR(("wl%d: BMAC TX Underflow?", wlc->pub->unit));
#endif
				}
			}
#ifdef WLP2P
			else if (P2P_ABS_SUPR(wlc, supr_status)) {
				/* We want to retry */
			}
#endif
		} else if (txs->phyerr) {
			update_rate = FALSE;
			WLCNTINCR(wlc->pub->_cnt->txphyerr);
			WL_ERROR(("wl%d: %s: tx phy error (0x%x)\n",
				wlc->pub->unit, __FUNCTION__, txs->phyerr));

#ifdef BCMDBG
			if (WL_ERROR_ON()) {
				prpkt("txpkt (AMPDU)", wlc->osh, p);
				if (AMPDU_HOST_ENAB(wlc->pub)) {
					wlc_print_txdesc(wlc, (wlc_txd_t*)PKTDATA(wlc->osh, p));
					wlc_print_txstatus(wlc, txs);
				}
#ifdef WLAMPDU_MAC
				else {
					wlc_print_ampdu_txstatus(ampdu_tx, txs, s1, s2, s3, s4);
					wlc_dump_aggfifo(wlc, NULL);
				}
#endif /* WLAMPDU_MAC */
			}
#endif /* BCMDBG */
		}
	}

	/* Check if interference is still there */
	if (wlc->rfaware_lifetime && wlc->exptime_cnt && (supr_status != TX_STATUS_SUPR_EXPTIME))
		wlc_exptime_check_end(wlc);

#ifdef WLPKTDLYSTAT
	/* Get the current time */
	now = WLC_GET_CURR_TIME(wlc);
#endif

	/* loop through all pkts and retry if not acked */
	while (p) {
		ASSERT(WLPKTTAG(p)->flags & WLF_AMPDU_MPDU);

		txh = (d11txh_t *)PKTDATA(wlc->osh, p);
		mcl = ltoh16(txh->MacTxControlLow);
		plcp = (uint8 *)(txh + 1);
		h = (struct dot11_header*)(plcp + D11_PHY_HDR_LEN);
		seq = ltoh16(h->seq) >> SEQNUM_SHIFT;
#ifdef WLPKTDLYSTAT
		pkt_freed = FALSE;
#endif
#ifdef WLAMPDU_MAC
		fid = ltoh16(txh->TxFrameID);
		BCM_REFERENCE(fid);
#endif
#ifdef PKTQ_LOG
		prec_pktlen = pkttotlen(wlc->osh, p) - sizeof(d11txh_t) - DOT11_LLC_SNAP_HDR_LEN -
		              DOT11_MAC_HDR_LEN - DOT11_QOS_LEN;
#endif
		if (tot_mpdu == 0) {
			mcs = plcp[0] & MIMO_PLCP_MCS_MASK;
			mimoantsel = ltoh16(txh->ABI_MimoAntSel);
			if (AMPDU_HOST_ENAB(wlc->pub)) {
				/* this is only needed for host agg */
#ifdef WLPKTDLYSTAT
				mcs_fbr = txh->FragPLCPFallback[0] & MIMO_PLCP_MCS_MASK;
#endif
			}
#ifdef WLAMPDU_MAC
			else {
				first_frameid = ltoh16(txh->TxFrameID);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
				if (PLCP3_ISSGI(plcp[3])) {
					WLCNTADD(ampdu_tx->cnt->u1.txmpdu_sgi, tx_mrt);
					if (ampdu_tx->amdbg)
						WLCNTADD(ampdu_tx->amdbg->txmcssgi[mcs], tx_mrt);
				}
				if (PLCP3_ISSTBC(plcp[3])) {
					WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc, tx_mrt);
					if (ampdu_tx->amdbg)
						WLCNTADD(ampdu_tx->amdbg->txmcsstbc[mcs], tx_mrt);
				}
				if (ampdu_tx->amdbg)
					WLCNTADD(ampdu_tx->amdbg->txmcs[mcs], tx_mrt);
#ifdef WLPKTDLYSTAT
				WLCNTADD(ampdu_tx->cnt->txmpdu_cnt[mcs], tx_mrt);
				WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[mcs], txsucc_mrt);
#endif
				if ((ltoh16(txh->XtraFrameTypes) & 0x3) != 0) {
#ifndef WLPKTDLYSTAT
					uint8 mcs_fbr;
					/* if not fbr_cck */
					mcs_fbr = txh->FragPLCPFallback[0] & MIMO_PLCP_MCS_MASK;
#endif
					if (ampdu_tx->amdbg)
						WLCNTADD(ampdu_tx->amdbg->txmcs[mcs_fbr], tx_fbr);
#ifdef WLPKTDLYSTAT
					WLCNTADD(ampdu_tx->cnt->txmpdu_cnt[mcs_fbr], tx_fbr);
					WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[mcs_fbr],
						txsucc_fbr);
#endif
					if (PLCP3_ISSGI(txh->FragPLCPFallback[3])) {
						WLCNTADD(ampdu_tx->cnt->u1.txmpdu_sgi, tx_fbr);
						if (ampdu_tx->amdbg)
							WLCNTADD(ampdu_tx->amdbg->txmcssgi[mcs_fbr],
								tx_fbr);
					}
					if (PLCP3_ISSTBC(txh->FragPLCPFallback[3])) {
						WLCNTADD(ampdu_tx->cnt->u2.txmpdu_stbc, tx_fbr);
						if (ampdu_tx->amdbg)
						   WLCNTADD(ampdu_tx->amdbg->txmcsstbc[mcs_fbr],
						            tx_fbr);
					}
				}
#endif /* BCMDBG || WLTEST */
			}
#endif /* WLAMPDU_MAC */
		}

		indx = TX_SEQ_TO_INDEX(seq);
		if (AMPDU_HOST_ENAB(wlc->pub)) {
			txretry = ini->txretry[indx];
		}

		if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) >= ini->ba_wsize) {
			WL_ERROR(("wl%d: %s: unexpected completion: seq 0x%x, "
				"start seq 0x%x\n",
				wlc->pub->unit, __FUNCTION__, seq, ini->start_seq));
#ifdef WLPKTDLYSTAT
			pkt_freed = TRUE;
#else
			PKTFREE(wlc->osh, p, TRUE);
#endif
#ifdef BCMDBG
#ifdef WLAMPDU_MAC
			wlc_print_ampdu_txstatus(ampdu_tx, txs, s1, s2, s3, s4);
#endif
#endif /* BCMDBG */
			goto nextp;
		}

		if (!isset(ini->ackpending, indx)) {
			WL_ERROR(("wl%d: %s: seq 0x%x\n",
				wlc->pub->unit, __FUNCTION__, seq));
			ampdu_dump_ini(ini);
			ASSERT(isset(ini->ackpending, indx));
		}

		ack_recd = FALSE;
		if (ba_recd || AMPDU_MAC_ENAB(wlc->pub)) {
			if (AMPDU_MAC_ENAB(wlc->pub))
				/* use position bitmap */
				bindex = tot_mpdu;
			else
				bindex = MODSUB_POW2(seq, start_seq, SEQNUM_MAX);

			WL_AMPDU_TX(("%s: tid %d seq is 0x%x, start_seq is 0x%x, "
			          "bindex is %d set %d, txretry %d, index %d\n",
			          __FUNCTION__, tid, seq, start_seq, bindex,
			          isset(bitmap, bindex), txretry, indx));

			/* if acked then clear bit and free packet */
			if ((bindex < AMPDU_BA_BITS_IN_BITMAP) && isset(bitmap, bindex)) {
				if (isset(ini->ackpending, indx)) {
					clrbit(ini->ackpending, indx);
					if (isset(ini->barpending, indx)) {
						clrbit(ini->barpending, indx);
					}
					ini->txretry[indx] = 0;
					ini->tx_in_transit--;
				}
				/* call any matching pkt callbacks */
				if (WLPKTTAG(p)->callbackidx) {
					wlc_pkt_callback((void *)wlc, p, TX_STATUS_ACK_RCV);
				}
#ifdef WLTXMONITOR
				if (MONITOR_ENAB(wlc) || PROMISC_ENAB(wlc->pub))
					wlc_tx_monitor(wlc, txh, txs, p, NULL);
#endif
#ifdef WLPKTDLYSTAT
				pkt_freed = TRUE;
#else
				PKTFREE(wlc->osh, p, TRUE);
#endif
				ack_recd = TRUE;
#ifdef PKTQ_LOG
				WLCNTINCR(prec_cnt->acked);
				WLCNTINCR(actq_cnt->acked);
				WLCNTADD(prec_bytes->acked, prec_pktlen);
				WLCNTADD(actq_bytes->acked, prec_pktlen);
#endif
				suc_mpdu++;
			}
		}
		/* either retransmit or send bar if ack not recd */
		if (!ack_recd)
		{
			bool relist = FALSE;

			WLPKTTAGSCBSET(p, scb);

#if defined(AP)
			/* For now, only do this for non-WDS */
			if (BSSCFG_AP(bsscfg) && !SCB_A4_DATA(ini->scb))
			{
				int32 max_ps_retry = (int32)retry_limit +
				                     (int32)wlc->ps_pretend_retry_limit;

				if (max_ps_retry > (uint8)(-1)) {
					max_ps_retry = (uint8)(-1);
				}

				relist = (((txretry >= (int32)retry_limit) &&
				         (txretry < max_ps_retry)) ||
				         SCB_PS_PRETEND_THRESHOLD(scb)) &&
				         !SCB_PS_PRETEND_NORMALPS(scb);
			}
#endif /* AP */

			if (AMPDU_HOST_ENAB(wlc->pub) &&
				retry && ((txretry < (int)retry_limit) || relist))
			{

				/* Set retry bit */
				h->fc |= htol16(FC_RETRY);

				ini->txretry[indx]++;
				requeue++;
				ASSERT(ini->retry_cnt < ampdu_tx->ba_max_tx_wsize);
				ASSERT(ini->retry_seq[ini->retry_tail] == 0);
				ini->retry_seq[ini->retry_tail] = seq;
				ini->retry_tail = NEXT_TX_INDEX(ini->retry_tail);
				ini->retry_cnt++;

#ifdef PKTQ_LOG
				WLCNTINCR(prec_cnt->retry);
				WLCNTINCR(actq_cnt->retry);
				WLCNTADD(prec_bytes->retry, prec_pktlen);
				WLCNTADD(actq_bytes->retry, prec_pktlen);
#endif

#if defined(AP)
				if (relist) {

					/* not using pmq for ps pretend, so indicate
					 * no block flag
					 */
					ps_retry = TRUE;
					wlc_apps_scb_pspretend_on(wlc, scb, PS_PRETEND_NO_BLOCK);
#ifdef PKTQ_LOG
					WLCNTINCR(prec_cnt->ps_retry);
					WLCNTINCR(actq_cnt->ps_retry);
					WLCNTADD(prec_bytes->ps_retry, prec_pktlen);
					WLCNTADD(actq_bytes->ps_retry, prec_pktlen);
#endif
				}
#endif /* AP */
				SCB_TX_NEXT(TXMOD_AMPDU, scb, p, WLC_PRIO_TO_HI_PREC(tid));
			} else {
				setbit(ini->barpending, indx);
				send_bar = TRUE;

#ifdef WLMEDIA_TXFAILEVENT
				wlc_tx_failed_event(wlc, scb, bsscfg, txs, p, 0);
#endif


#ifdef WLTXMONITOR
				if (MONITOR_ENAB(wlc) || PROMISC_ENAB(wlc->pub))
					wlc_tx_monitor(wlc, txh, txs, p, NULL);
#endif
#ifdef PKTQ_LOG
				WLCNTINCR(prec_cnt->retry_drop);
				WLCNTINCR(actq_cnt->retry_drop);
				WLCNTADD(prec_bytes->retry_drop, prec_pktlen);
				WLCNTADD(actq_bytes->retry_drop, prec_pktlen);
#endif

#ifdef WLPKTDLYSTAT
				pkt_freed = TRUE;
#else
				PKTFREE(wlc->osh, p, TRUE);
#endif
			}
		} else if (AMPDU_MAC_ENAB(wlc->pub)) {
			ba_recd = TRUE;
		}
#ifdef BCMDBG
		/* tot_mpdu may exceeds AMPDU_MAX_MPDU if doing ucode/hw agg */
		if (AMPDU_HOST_ENAB(wlc->pub) && ba_recd && !ack_recd)
			hole[idx]++;
#endif

nextp:
#ifdef BCMDBG
		if ((idx = tot_mpdu) >= AMPDU_MAX_MPDU) {
			WL_ERROR(("%s: idx out-of-bound to array size (%d)\n", __FUNCTION__, idx));
			idx = AMPDU_MAX_MPDU - 1;
		}
#endif
		tot_mpdu++;

#ifdef WLPKTDLYSTAT
		/* calculate latency and packet loss statistics */
		if (pkt_freed) {

			/* Ignore wrap-around case and error case */
			if (now > WLPKTTAG(p)->shared.enqtime) {
				uint tr;
				if (AMPDU_HOST_ENAB(ampdu_tx->wlc->pub))
					tr = (txretry >= AMPDU_DEF_RETRY_LIMIT) ?
						AMPDU_DEF_RETRY_LIMIT : txretry;
				else
					tr = 0;

				delay = now - WLPKTTAG(p)->shared.enqtime;
				wlc_delay_stats_upd(delay_stats, delay, tr, ack_recd);
				WL_AMPDU_STAT(("Enq %d retry %d cnt %d: acked %d delay/min/"
					"max/sum %d %d %d %d\n", WLPKTTAG(p)->shared.enqtime,
					tr, delay_stats->txmpdu_cnt[tr], ack_recd, delay,
					delay_stats->delay_min, delay_stats->delay_max,
					delay_stats->delay_sum[tr]));
			}
			PKTFREE(wlc->osh, p, TRUE);
		}
#endif /* WLPKTDLYSTAT */

		/* break out if last packet of ampdu */
		if (AMPDU_MAC_ENAB(wlc->pub)) {
#ifdef WLAMPDU_MAC
			WLCNTDECR(ampdu_tx->cnt->pending);
			WLCNTINCR(ampdu_tx->cnt->cons);

			wlc_txfifo_complete(wlc, queue, 1);
			if (tot_mpdu == ncons) {
				ASSERT(txs->frameid == fid);
				break;
			}
			if (++pdu_count >= ampdu_tx->ba_max_tx_wsize) {
				WL_ERROR(("%s: Reach max num of MPDU without finding frameid\n",
					__FUNCTION__));
				ASSERT(pdu_count >= ampdu_tx->ba_max_tx_wsize);
			}
#endif /* WLAMPDU_MAC */
		} else {
			if (((mcl & TXC_AMPDU_MASK) >> TXC_AMPDU_SHIFT) == TXC_AMPDU_LAST)
				break;
		}

		p = GETNEXTTXP(wlc, queue);
#ifdef PROP_TXSTATUS
		if (p && PROP_TXSTATUS_ENAB(wlc->pub)) {
			uint8 wlfc_status =  WLFC_D11_STATUS_INTERPRET(txs);
			if (scb && (wlfc_status == WLFC_CTL_PKTFLAG_D11SUPPRESS)) {
				wlc_suppress_sync_fsm(wlc, scb, p, TRUE);
			}
			wlc_process_wlhdr_txstatus(wlc, wlfc_status, p, FALSE);
		}
#endif

		/* For external use big hammer to restore the sync */
		if (p == NULL) {
#ifdef WLAMPDU_MAC
			WL_ERROR(("%s: p is NULL. tot_mpdu %d suc %d pdu %d first_frameid %x"
				"fifocnt %d\n", __FUNCTION__, tot_mpdu, suc_mpdu, pdu_count,
				first_frameid,
				R_REG(wlc->osh, &wlc->regs->u.d11regs.xmtfifo_frame_cnt)));
#endif

#ifdef BCMDBG
#ifdef WLAMPDU_MAC
			wlc_print_ampdu_txstatus(ampdu_tx, txs, s1, s2, s3, s4);
			wlc_dump_aggfifo(wlc, NULL);
#endif
#endif	/* BCMDBG */
			ASSERT(p);
			break;
		}
	} /* while(p) */


#ifdef BCMDBG
	/* post-process the statistics */
	if (AMPDU_HOST_ENAB(wlc->pub)) {
		int i, j;
		ampdu_tx->amdbg->mpdu_histogram[idx]++;
		for (i = 0; i < idx; i++) {
			if (hole[i])
				ampdu_tx->amdbg->retry_histogram[i]++;
			if (hole[i]) {
				for (j = i + 1; (j < idx) && hole[j]; j++)
					;
				if (j == idx)
					ampdu_tx->amdbg->end_histogram[i]++;
			}
		}

#ifdef WLPKTDLYSTAT
		if (txretry < rr_retry_limit)
			WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[mcs], suc_mpdu);
		else
			WLCNTADD(ampdu_tx->cnt->txmpdu_succ_cnt[mcs_fbr], suc_mpdu);
#endif
	}
#endif /* BCMDBG */

	/* send bar to move the window */
	if (send_bar)
		wlc_ampdu_send_bar(ampdu_tx, ini, FALSE);

	/* update rate state */
	if (WLANTSEL_ENAB(wlc))
		antselid = wlc_antsel_antsel2id(wlc->asi, mimoantsel);

#ifdef WLAMPDU_MAC
	WL_AMPDU_HWDBG(("%s: consume %d txfifo_cnt %d\n", __FUNCTION__, tot_mpdu,
		R_REG(wlc->osh, &wlc->regs->u.d11regs.xmtfifo_frame_cnt)));

	if (AMPDU_MAC_ENAB(wlc->pub)) {
		if (tx_mrt + tx_fbr == 0) {
			/* must have failed because of rts failure */
			ASSERT(txsucc_mrt + txsucc_fbr == 0);
#ifdef BCMDBG
			if (txsucc_mrt + txsucc_fbr != 0) {
				WL_ERROR(("%s: wrong condition\n", __FUNCTION__));
				wlc_print_ampdu_txstatus(ampdu_tx, txs, s1, s2, s3, s4);
			}
			if (WL_AMPDU_ERR_ON()) {
				wlc_print_ampdu_txstatus(ampdu_tx, txs, s1, s2, s3, s4);
			}
#endif
			if (update_rate)
				wlc_scb_ratesel_upd_txs_ampdu(wlc->wrsi, scb, first_frameid,
					(uint8)ncons, 0, 0, 0, FALSE, mcs, antselid);
		} else if (update_rate)
		if (update_rate)
			wlc_scb_ratesel_upd_txs_ampdu(wlc->wrsi, scb, first_frameid,
				tx_mrt, txsucc_mrt, tx_fbr, txsucc_fbr,
				FALSE, mcs, antselid);
	} else
#endif /* WLAMPDU_MAC */
	if (update_rate) {
		wlc_scb_ratesel_upd_txs_blockack(wlc->wrsi, scb,
			txs, suc_mpdu, tot_mpdu, !ba_recd, (uint8)txretry,
			rr_retry_limit, FALSE, mcs & MIMO_PLCP_MCS_MASK, antselid);
	}

	WL_AMPDU_TX(("wl%d: wlc_ampdu_dotxstatus: requeueing %d packets\n",
		wlc->pub->unit, requeue));

	/* Call only once per ampdu_tx for SW agg */
	if (AMPDU_HOST_ENAB(wlc->pub))
		wlc_txfifo_complete(wlc, queue, ampdu_tx->txpkt_weight);

#ifdef AP
	if (BSSCFG_AP(bsscfg)) {
		wlc_apps_process_pspretend_status(wlc, scb, ba_recd, ps_retry);
	}
#endif
	/* bump up the start seq to move the window */
	wlc_ampdu_ini_move_window(ampdu_tx, scb_ampdu, ini);

#ifdef STA
	/* PM state change */
	wlc_update_pmstate(bsscfg, (uint)(wlc_txs_alias_to_old_fmt(wlc, &(txs->status))));
#endif /* STA */
}

#ifdef WLAMPDU_MAC
static void
wlc_print_ampdu_txstatus(ampdu_tx_info_t *ampdu_tx,
	tx_status_t *pkg1, uint32 s1, uint32 s2, uint32 s3, uint32 s4)
{
	uint16 *p = (uint16 *)pkg1;
	printf("%s: txstatus 0x%04x\n", __FUNCTION__, pkg1->status.raw_bits);
	if (AMPDU_MAC_ENAB(ampdu_tx->wlc->pub) && !AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		uint16 last_frameid, txstat;
		uint8 bitmap[8];
		uint16 txcnt_mrt, succ_mrt, txcnt_fbr, succ_fbr;
		uint16 ncons = pkg1->sequence;

		last_frameid = p[2]; txstat = p[3];

		bitmap[0] = (txstat & TX_STATUS_BA_BMAP03_MASK) >> TX_STATUS_BA_BMAP03_SHIFT;
		bitmap[0] |= (s1 & TX_STATUS_BA_BMAP47_MASK) << TX_STATUS_BA_BMAP47_SHIFT;
		bitmap[1] = (s1 >> 8) & 0xff;
		bitmap[2] = (s1 >> 16) & 0xff;
		bitmap[3] = (s1 >> 24) & 0xff;
		txcnt_mrt = s2 & 0xff;
		succ_mrt = (s2 >> 8) & 0xff;
		txcnt_fbr = (s2 >> 16) & 0xff;
		succ_fbr = (s2 >> 24) & 0xff;
		printf("\t\t ncons %d last frameid: 0x%x\n", ncons, last_frameid);
		printf("\t\t txcnt: mrt %2d succ %2d fbr %2d succ %2d\n",
		       txcnt_mrt, succ_mrt, txcnt_fbr, succ_fbr);

		if (AMPDU_UCODE_ENAB(ampdu_tx->wlc->pub)) {
			printf("\t\t bitmap: %02x %02x %02x %02x\n",
				bitmap[0], bitmap[1], bitmap[2], bitmap[3]);
		} else {
			bitmap[4] = (s3 >> 16) & 0xff;
			bitmap[5] = (s3 >> 24) & 0xff;
			bitmap[6] = s4 & 0xff;
			bitmap[7] = (s4 >> 8) & 0xff;
			printf("\t\t bitmap: %02x %02x %02x %02x %02x %02x %02x %02x\n",
			       bitmap[0], bitmap[1], bitmap[2], bitmap[3],
			       bitmap[4], bitmap[5], bitmap[6], bitmap[7]);
			printf("\t\t timestamp 0x%04x\n", (s4 >> 16) & 0xffff);
		}
#ifdef WLAMPDU_UCODE
		if (AMPDU_UCODE_ENAB(ampdu_tx->wlc->pub) && txcnt_mrt + txcnt_fbr == 0) {
			uint16 strt, end, wptr, rptr, rnum, qid;
			uint16 base =
				(wlc_read_shm(ampdu_tx->wlc, M_TXFS_PTR) + C_TXFSD_WOFFSET) * 2;
			qid = last_frameid & TXFID_QUEUE_MASK;
			strt = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_STRT_POS(base, qid));
			end = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_END_POS(base, qid));
			wptr = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_WPTR_POS(base, qid));
			rptr = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_RPTR_POS(base, qid));
			rnum = wlc_read_shm(ampdu_tx->wlc, C_TXFSD_RNUM_POS(base, qid));
			printf("%s: side channel %d ---- \n", __FUNCTION__, qid);
			printf("\t\t strt : shm(0x%x) = 0x%x\n", C_TXFSD_STRT_POS(base, qid), strt);
			printf("\t\t end  : shm(0x%x) = 0x%x\n", C_TXFSD_END_POS(base, qid), end);
			printf("\t\t wptr : shm(0x%x) = 0x%x\n", C_TXFSD_WPTR_POS(base, qid), wptr);
			printf("\t\t rptr : shm(0x%x) = 0x%x\n", C_TXFSD_RPTR_POS(base, qid), rptr);
			printf("\t\t rnum : shm(0x%x) = 0x%x\n", C_TXFSD_RNUM_POS(base, qid), rnum);
		}
#endif /* WLAMPDU_UCODE */
	}
}

void
wlc_dump_aggfifo(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	osl_t *osh;
	d11regs_t *regs;
	osh = wlc->osh;
	regs = wlc->regs;
	if (!wlc->clk) {
		printf("%s: clk off\n", __FUNCTION__);
		return;
	}

	printf("%s:\n", __FUNCTION__);

	if (AMPDU_AQM_ENAB(wlc->pub)) {
		printf("framerdy 0x%x bmccmd %d framecnt %d \n",
		       R_REG(osh, &regs->u.d11acregs.AQMFifoReady),
		       R_REG(osh, &regs->u.d11acregs.BMCCmd),
		       R_REG(osh, &regs->u.d11acregs.XmtFifoFrameCnt));
		printf("AQM agg params 0x%x maxlen hi/lo 0x%x 0x%x minlen 0x%x adjlen 0x%x\n",
		       R_REG(osh, &regs->u.d11acregs.AQMAggParams),
		       R_REG(osh, &regs->u.d11acregs.AQMMaxAggLenHi),
		       R_REG(osh, &regs->u.d11acregs.AQMMaxAggLenLow),
		       R_REG(osh, &regs->u.d11acregs.AQMMinMpduLen),
		       R_REG(osh, &regs->u.d11acregs.AQMMacAdjLen));
		printf("AQM agg results 0x%x len hi/lo: 0x%x 0x%x BAbitmap(0-3) %x %x %x %x\n",
		       R_REG(osh, &regs->u.d11acregs.AQMAggStats),
		       R_REG(osh, &regs->u.d11acregs.AQMAggLenHi),
		       R_REG(osh, &regs->u.d11acregs.AQMAggLenLow),
		       R_REG(osh, &regs->u.d11acregs.AQMUpdBA0),
		       R_REG(osh, &regs->u.d11acregs.AQMUpdBA1),
		       R_REG(osh, &regs->u.d11acregs.AQMUpdBA2),
		       R_REG(osh, &regs->u.d11acregs.AQMUpdBA3));
		return;
	}

#ifdef WLAMPDU_UCODE
	if (AMPDU_UCODE_ENAB(wlc->pub)) {
		int i;
		ampdu_tx_info_t* ampdu_tx = wlc->ampdu_tx;
		for (i = 0; i < 4; i++) {
			int k = 0, addr;
			printf("fifo %d: rptr %x wptr %x\n",
			       i, ampdu_tx->hagg[i].txfs_rptr,
			       ampdu_tx->hagg[i].txfs_wptr);
			for (addr = ampdu_tx->hagg[i].txfs_addr_strt;
			     addr <= ampdu_tx->hagg[i].txfs_addr_end; addr++) {
				printf("\tentry %d addr 0x%x: 0x%x\n",
				       k, addr, wlc_read_shm(wlc, addr * 2));
				k++;
			}
		}
	}
#endif /* WLAMPDU_UCODE */
#if defined(WLCNT) && defined(WLAMPDU_MAC)
	printf("driver statistics: aggfifo pending %d enque/cons %d %d\n",
	       wlc->ampdu_tx->cnt->pending,
	       wlc->ampdu_tx->cnt->enq,
	       wlc->ampdu_tx->cnt->cons);
#endif

#ifdef WLAMPDU_HW
	if (AMPDU_HW_ENAB(wlc->pub)) {
		int i;
		printf("AGGFIFO regs: availcnt 0x%x txfifo fr_count %d by_count %d\n",
		       R_REG(osh, &regs->aggfifocnt),
		       R_REG(osh, &regs->u.d11regs.xmtfifo_frame_cnt),
		       R_REG(osh, &regs->u.d11regs.xmtfifo_byte_cnt));
		printf("cmd 0x%04x stat 0x%04x cfgctl 0x%04x cfgdata 0x%04x mpdunum 0x%02x"
		       " len 0x%04x bmp 0x%04x ackedcnt %d\n",
		       R_REG(osh, &regs->u.d11regs.aggfifo_cmd),
		       R_REG(osh, &regs->u.d11regs.aggfifo_stat),
		       R_REG(osh, &regs->u.d11regs.aggfifo_cfgctl),
		       R_REG(osh, &regs->u.d11regs.aggfifo_cfgdata),
		       R_REG(osh, &regs->u.d11regs.aggfifo_mpdunum),
		       R_REG(osh, &regs->u.d11regs.aggfifo_len),
		       R_REG(osh, &regs->u.d11regs.aggfifo_bmp),
		       R_REG(osh, &regs->u.d11regs.aggfifo_ackedcnt));

		/* aggfifo */
		printf("AGGFIFO dump: \n");
		for (i = 1; i < 2; i ++) {
			int j;
			printf("AGGFIFO %d:\n", i);
			for (j = 0; j < AGGFIFO_CAP; j ++) {
				uint16 entry;
				W_REG(osh, &regs->u.d11regs.aggfifo_sel, (i << 6) | j);
				W_REG(osh, &regs->u.d11regs.aggfifo_cmd, (6 << 2) | i);
				entry = R_REG(osh, &regs->u.d11regs.aggfifo_data);
				if (j % 4 == 0) {
					printf("\tEntry 0x%02x: 0x%04x	", j, entry);
				} else if (j % 4 == 3) {
					printf("0x%04x\n", entry);
				} else {
					printf("0x%04x	", entry);
				}
			}
		}
	}
#endif /* WLAMPDU_HW */
}
#endif /* WLAMPDU_MAC */

static void
ampdu_ba_state_off(scb_ampdu_tx_t *scb_ampdu, scb_ampdu_tid_ini_t *ini)
{
	void *p;

	ini->ba_state = AMPDU_TID_STATE_BA_OFF;

	/* release all buffered pkts */
	while ((p = wlc_ampdu_pktq_pdeq(&scb_ampdu->txq, ini->tid))) {
		ASSERT(PKTPRIO(p) == ini->tid);
		SCB_TX_NEXT(TXMOD_AMPDU, ini->scb, p, WLC_PRIO_TO_PREC(ini->tid));
	}

	wlc_txflowcontrol_override(scb_ampdu->ampdu_tx->wlc, SCB_WLCIFP(scb_ampdu->scb)->qi,
		OFF, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL);
}

void
ampdu_cleanup_tid_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid, bool force)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint16 indx, i;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);
	ASSERT(scb_ampdu->scb);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	AMPDU_VALIDATE_TID(ampdu_tx, tid, "ampdu_cleanup_tid_ini");

	if (!(ini = scb_ampdu->ini[tid]))
		return;

	ini->ba_state = AMPDU_TID_STATE_BA_PENDING_OFF;

	WL_AMPDU_CTL(("wl%d: ampdu_cleanup_tid_ini: tid %d force %d\n",
		ampdu_tx->wlc->pub->unit, tid, force));
	/* cleanup stuff that was to be done on bar send complete */
	if (!ini->bar_ackpending) {
		for (indx = 0; indx < ampdu_tx->ba_max_tx_wsize; indx++) {
			if (isset(ini->barpending, indx)) {
				clrbit(ini->barpending, indx);
				if (isset(ini->ackpending, indx)) {
					clrbit(ini->ackpending, indx);
					ini->txretry[indx] = 0;
					ini->tx_in_transit--;
				}
				wlc_ampdu_ini_move_window(ampdu_tx, scb_ampdu, ini);
			}
		}
	}

	if (ini->tx_in_transit && !force)
		return;

	if (ini->tx_in_transit == 0)
		ASSERT(ini->rem_window == ini->ba_wsize);

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, ini->scb);

	ASSERT(ini == scb_ampdu->ini[ini->tid]);

	scb_ampdu->ini[ini->tid] = NULL;

	/* free all buffered tx packets */
	wlc_ampdu_pktq_pflush(ampdu_tx->wlc->osh, &scb_ampdu->txq, ini->tid, TRUE, NULL, 0);

	if (!ini->bar_ackpending)
		MFREE(ampdu_tx->wlc->osh, ini, sizeof(scb_ampdu_tid_ini_t));
	else {
		/* save ini in case bar complete cb is not called */
		i = ampdu_tx->ini_free_index;
		if (ampdu_tx->ini_free[i])
			MFREE(ampdu_tx->wlc->osh, ampdu_tx->ini_free[i],
				sizeof(scb_ampdu_tid_ini_t));
		ampdu_tx->ini_free[i] = (void *)ini;
		ampdu_tx->ini_free_index = MODINC(ampdu_tx->ini_free_index, AMPDU_INI_FREE);
		ini->free_me = TRUE;
	}

	wlc_txflowcontrol_override(ampdu_tx->wlc, SCB_WLCIFP(scb_ampdu->scb)->qi,
	                           OFF, TXQ_STOP_FOR_AMPDU_FLOW_CNTRL);
}

void
wlc_ampdu_recv_addba_resp(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 *body, int body_len)
{
	scb_ampdu_tx_t *scb_ampdu_tx;
	struct pktq *txq;
	dot11_addba_resp_t *addba_resp;
	scb_ampdu_tid_ini_t *ini;
	uint16 param_set, status;
	uint8 tid, wsize, policy;
	uint16 current_wlctxq_len = 0, i = 0;
	void *p = NULL;

	ASSERT(scb);

	scb_ampdu_tx = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu_tx);

	addba_resp = (dot11_addba_resp_t *)body;

	if (addba_resp->category & DOT11_ACTION_CAT_ERR_MASK) {
		WL_ERROR(("wl%d: %s: unexp error action frame\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__));
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}

	status = ltoh16_ua(&addba_resp->status);
	param_set = ltoh16_ua(&addba_resp->addba_param_set);

	wsize =	(param_set & DOT11_ADDBA_PARAM_BSIZE_MASK) >> DOT11_ADDBA_PARAM_BSIZE_SHIFT;
	policy = (param_set & DOT11_ADDBA_PARAM_POLICY_MASK) >> DOT11_ADDBA_PARAM_POLICY_SHIFT;
	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	AMPDU_VALIDATE_TID(ampdu_tx, tid, "wlc_ampdu_recv_addba_resp");

	ini = scb_ampdu_tx->ini[tid];

	if ((ini == NULL) || (ini->ba_state != AMPDU_TID_STATE_BA_PENDING_ON)) {
		WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_addba_resp: unsolicited packet\n",
			ampdu_tx->wlc->pub->unit));
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}

	if ((status != DOT11_SC_SUCCESS) ||
	    (policy != ampdu_tx->ba_policy) ||
	    (wsize > ampdu_tx->ba_max_tx_wsize)) {
		WL_ERROR(("wl%d: %s: Failed. status %d wsize %d policy %d\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__, status, wsize, policy));
		ampdu_ba_state_off(scb_ampdu_tx, ini);
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}

#ifdef WLAMSDU
	/* Record whether this scb supports amsdu over ampdu */
	scb->flags2 &= ~SCB2_AMSDU_IN_AMPDU_CAP;
	if ((param_set & DOT11_ADDBA_PARAM_AMSDU_SUP) != 0) {
		scb->flags2 |= SCB2_AMSDU_IN_AMPDU_CAP;
	}
#endif /* WLAMSDU */

	ini->ba_wsize = wsize;
	ini->rem_window = wsize;
	ini->start_seq = (SCB_SEQNUM(scb, tid) & (SEQNUM_MAX - 1));
	ini->tx_exp_seq = ini->start_seq;
	ini->max_seq = MODSUB_POW2(ini->start_seq, 1, SEQNUM_MAX);
	ini->ba_state = AMPDU_TID_STATE_BA_ON;

	WLCNTINCR(ampdu_tx->cnt->rxaddbaresp);
	AMPDUSCBCNTINCR(scb_ampdu_tx->cnt.rxaddbaresp);

	WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_addba_resp: Turning BA ON: tid %d wsize %d\n",
		ampdu_tx->wlc->pub->unit, tid, wsize));

	/* send bar to set initial window */
	wlc_ampdu_send_bar(ampdu_tx, ini, TRUE);

	/* if packets already exist in wlcq, conditionally transfer them to ampduq
		to avoid barpending stuck issue.
	*/
	txq = &SCB_WLCIFP(scb)->qi->q;

	current_wlctxq_len = pktq_plen(txq, WLC_PRIO_TO_PREC(tid));

	for (i = 0; i < current_wlctxq_len; i++) {
		p = pktq_pdeq(txq, WLC_PRIO_TO_PREC(tid));

		if (!p) break;
		/* omit control/mgmt frames and queue them to the back
		 * only the frames relevant to this scb should be moved to ampduq;
		 * otherwise, queue them back.
		 */
		if (!(WLPKTTAG(p)->flags & WLF_DATA) || 	/* mgmt/ctl */
		    scb != WLPKTTAGSCBGET(p) || /* frame belong to some other scb */
		    (WLPKTTAG(p)->flags & WLF_AMPDU_MPDU)) { /* possibly retried AMPDU */
			if (wlc_prec_enq(ampdu_tx->wlc, txq, p, WLC_PRIO_TO_PREC(tid)))
				continue;
		} else { /* XXXCheck if Flow control/watermark issues */
			if (wlc_ampdu_prec_enq(ampdu_tx->wlc, &scb_ampdu_tx->txq, p, tid))
				continue;
		}

		/* Toss packet as queuing failed */
		WL_AMPDU_ERR(("wl%d: txq overflow\n", ampdu_tx->wlc->pub->unit));
		PKTFREE(ampdu_tx->wlc->osh, p, TRUE);
		WLCNTINCR(ampdu_tx->wlc->pub->_cnt->txnobuf);
		WLCNTINCR(ampdu_tx->cnt->txdrop);
		AMPDUSCBCNTINCR(scb_ampdu_tx->cnt.txdrop);
		WLCIFCNTINCR(scb, txnobuf);
		WLCNTSCBINCR(scb->scb_stats.tx_failures);
	}

	/* release pkts */
	wlc_ampdu_txeval(ampdu_tx, scb_ampdu_tx, ini, FALSE);
}

void
wlc_ampdu_recv_ba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 *body, int body_len)
{
	/* AMPDUXXX: silently ignore the ba since we dont handle it for immediate ba */
	WLCNTINCR(ampdu_tx->cnt->rxba);
}

/* initialize the initiator code for tid */
static scb_ampdu_tid_ini_t*
wlc_ampdu_init_tid_ini(ampdu_tx_info_t *ampdu_tx, scb_ampdu_tx_t *scb_ampdu, uint8 tid,
	bool override)
{
	scb_ampdu_tid_ini_t *ini;
	uint8 peer_density;

	ASSERT(scb_ampdu);
	ASSERT(scb_ampdu->scb);
	ASSERT(SCB_AMPDU(scb_ampdu->scb));
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	if (ampdu_tx->manual_mode && !override)
		return NULL;

	/* check for per-tid control of ampdu */
	if (!ampdu_tx->ini_enable[tid])
		return NULL;

	/* AMPDUXXX: No support for dynamic update of density/len from peer */
	/* retrieve the density and max ampdu len from scb */

	peer_density = (scb_ampdu->scb->ht_ampdu_params &
		HT_PARAMS_DENSITY_MASK) >> HT_PARAMS_DENSITY_SHIFT;

	/* our density requirement is for tx side as well */
	scb_ampdu->mpdu_density = MAX(peer_density, ampdu_tx->mpdu_density);

	/* should call after mpdu_density is init'd */
	wlc_ampdu_init_min_lens(scb_ampdu);

	scb_ampdu->max_rxlen = AMPDU_RX_FACTOR_BASE <<
		(scb_ampdu->scb->ht_ampdu_params & HT_PARAMS_RX_FACTOR_MASK);

	scb_ampdu_update_config(ampdu_tx, scb_ampdu->scb);

	if (!scb_ampdu->ini[tid]) {
		ini = MALLOC(ampdu_tx->wlc->osh, sizeof(scb_ampdu_tid_ini_t));

		if (ini == NULL)
			return NULL;

		scb_ampdu->ini[tid] = ini;
	} else
		ini = scb_ampdu->ini[tid];

	bzero((char *)ini, sizeof(scb_ampdu_tid_ini_t));

	ini->ba_state = AMPDU_TID_STATE_BA_PENDING_ON;
	ini->tid = tid;
	ini->scb = scb_ampdu->scb;
	ini->retry_bar = AMPDU_R_BAR_DISABLED;

	wlc_send_addba_req(ampdu_tx->wlc, ini->scb, tid, ampdu_tx->ba_tx_wsize,
		ampdu_tx->ba_policy, ampdu_tx->delba_timeout);

	WLCNTINCR(ampdu_tx->cnt->txaddbareq);
	AMPDUSCBCNTINCR(scb_ampdu->cnt.txaddbareq);

	return ini;
}

void
wlc_ampdu_recv_addba_req_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	dot11_addba_req_t *addba_req, int body_len)
{
	scb_ampdu_tx_t *scb_ampdu_tx;
	scb_ampdu_tid_ini_t *ini;
	uint16 param_set;
	uint8 tid;

	scb_ampdu_tx = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu_tx);

	param_set = ltoh16_ua(&addba_req->addba_param_set);
	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	AMPDU_VALIDATE_TID(ampdu_tx, tid, "wlc_ampdu_recv_addba_req_ini");

	/* check if it is action err frame */
	if (addba_req->category & DOT11_ACTION_CAT_ERR_MASK) {
		ini = scb_ampdu_tx->ini[tid];
		if ((ini != NULL) && (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)) {
			ampdu_ba_state_off(scb_ampdu_tx, ini);
			WL_ERROR(("wl%d: %s: error action frame\n",
				ampdu_tx->wlc->pub->unit, __FUNCTION__));
		} else {
			WL_ERROR(("wl%d: %s: unexp error action "
				"frame\n", ampdu_tx->wlc->pub->unit, __FUNCTION__));
		}
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}
}

int
wlc_ampdu_tx_set(ampdu_tx_info_t *ampdu_tx, bool on)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
	uint8 ampdu_mac_agg = AMPDU_AGG_OFF;
	uint16 value = 0;
	int err = BCME_OK;
	int8 aggmode;

	wlc->pub->_ampdu_tx = FALSE;

	if (ampdu_tx->ampdu_aggmode == AMPDU_AGG_AUTO) {
		if (D11REV_IS(wlc->pub->corerev, 31) && WLCISNPHY(wlc->band)) {
			aggmode = AMPDU_AGGMODE_HOST;
		} else if ((D11REV_IS(wlc->pub->corerev, 26) ||
			D11REV_IS(wlc->pub->corerev, 29)) &&
			WLCISHTPHY(wlc->band)) {
			aggmode = AMPDU_AGGMODE_HOST;
		}
#ifdef WLP2P
		else if (D11REV_IS(wlc->pub->corerev, 24) && WLCISLCNPHY(wlc->band))
			aggmode = AMPDU_AGGMODE_HOST;
#endif
		else if (D11REV_GE(wlc->pub->corerev, 16) && D11REV_LT(wlc->pub->corerev, 26))
			aggmode = AMPDU_AGGMODE_MAC;
		else if (D11REV_GE(wlc->pub->corerev, 40))
			aggmode = AMPDU_AGGMODE_MAC;
		else
			aggmode = AMPDU_AGGMODE_HOST;
	} else
		aggmode = ampdu_tx->ampdu_aggmode;

	if (on) {
		if (!N_ENAB(wlc->pub)) {
			WL_AMPDU_ERR(("wl%d: driver not nmode enabled\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			goto exit;
		}
		if (!wlc_ampdu_tx_cap(ampdu_tx)) {
			WL_AMPDU_ERR(("wl%d: device not ampdu capable\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			goto exit;
		}
		if (PIO_ENAB(wlc->pub)) {
			WL_AMPDU_ERR(("wl%d: driver is pio mode\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			goto exit;
		}

#ifdef WLAMPDU_MAC
		if (aggmode == AMPDU_AGGMODE_MAC) {
			if (D11REV_GE(wlc->pub->corerev, 16) && D11REV_LT(wlc->pub->corerev, 26)) {
#ifdef WLAMPDU_UCODE
				ampdu_mac_agg = AMPDU_AGG_UCODE;
				WL_AMPDU(("wl%d: 26 > Corerev >= 16 required for ucode agg\n",
					wlc->pub->unit));
#endif
			}
			if (D11REV_IS(wlc->pub->corerev, 24) && WLCISNPHY(wlc->band)) {
#ifdef WLAMPDU_UCODE
				ampdu_mac_agg = AMPDU_AGG_UCODE;
				WL_AMPDU(("wl%d: Corerev is 26 NPHY support ucode agg only\n",
				wlc->pub->unit));
#endif
			}
			if ((D11REV_IS(wlc->pub->corerev, 26) ||
				D11REV_IS(wlc->pub->corerev, 29)) && WLCISHTPHY(wlc->band)) {
				ampdu_mac_agg = AMPDU_AGG_HW;
				WL_AMPDU(("wl%d: Corerev is 26 or 29 support hw agg only\n",
				wlc->pub->unit));
			}
			if (D11REV_IS(wlc->pub->corerev, 31) && WLCISNPHY(wlc->band)) {
				ampdu_mac_agg = AMPDU_AGG_HW;
				WL_AMPDU(("wl%d: Corerev is 31 NPHY support hw agg only\n",
					wlc->pub->unit));
			}
			if (D11REV_GE(wlc->pub->corerev, 40)) {
				ampdu_mac_agg = AMPDU_AGG_AQM;
				WL_AMPDU(("wl%d: Corerev is 40 aqm agg\n",
					wlc->pub->unit));
			}
		}
#endif /* WLAMPDU_MAC */
	}

	/* if aggmode define as MAC, but mac_agg is set to OFF,
	 * then set aggmode to HOST
	 */
	if ((aggmode == AMPDU_AGGMODE_MAC) && (ampdu_mac_agg == AMPDU_AGG_OFF))
		aggmode = AMPDU_AGGMODE_HOST;

	/* setup ucode tx-retrys and max pkt aggr for MAC mode */
	if (aggmode == AMPDU_AGGMODE_MAC) {
		if (ampdu_mac_agg == AMPDU_AGG_AQM) {
			wlc->pub->txmaxpkts = MAXTXPKTS_AMPDUAQM;
		}
		else
			wlc->pub->txmaxpkts = MAXTXPKTS_AMPDUMAC;
		value = MHF3_UCAMPDU_RETX;
	} else {
		wlc->pub->txmaxpkts = MAXTXPKTS;
	}
	WL_ERROR(("wl%d: %s: %s txmaxpkts %d\n", wlc->pub->unit, __FUNCTION__,
	          ((aggmode == AMPDU_AGGMODE_HOST) ? " AGG Mode = HOST" :
	           ((ampdu_mac_agg == AMPDU_AGG_UCODE) ? "AGG Mode = MAC+Ucode" :
	           ((ampdu_mac_agg == AMPDU_AGG_HW) ? "AGG Mode = MAC+HW" : "AGG Mode = MAC+AQM"))),
	          wlc->pub->txmaxpkts));

	if (wlc->pub->_ampdu_tx != on) {
#ifdef WLCNT
		bzero(ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
#endif
		wlc->pub->_ampdu_tx = on;
	}

exit:
	if (wlc->pub->_ampdumac != ampdu_mac_agg) {
		wlc->pub->_ampdumac = ampdu_mac_agg;
		wlc_mhf(wlc, MHF3, MHF3_UCAMPDU_RETX, value, WLC_BAND_ALL);
		/* just differentiate HW or not for now */
		wlc_ampdu_mode_upd(wlc, AMPDU_HW_ENAB(wlc->pub) ?
			AMPDU_AGG_HW : AMPDU_AGGMODE_HOST);
	}

	return err;
}

bool
wlc_ampdu_tx_cap(ampdu_tx_info_t *ampdu_tx)
{
	if (WLC_PHY_11N_CAP(ampdu_tx->wlc->band))
		return TRUE;
	else
		return FALSE;
}

static void
ampdu_update_max_txlen(ampdu_tx_info_t *ampdu_tx, uint16 dur)
{
	uint32 rate, mcs;

	dur = dur >> 10;
	for (mcs = 0; mcs < MCS_TABLE_SIZE; mcs++) {
		/* rate is in Kbps; dur is in msec ==> len = (rate * dur) / 8 */
		/* 20MHz, No SGI */
		rate = MCS_RATE(mcs, FALSE, FALSE);
		ampdu_tx->max_txlen[mcs][0][0] = (rate * dur) >> 3;
		/* 40 MHz, No SGI */
		rate = MCS_RATE(mcs, TRUE, FALSE);
		ampdu_tx->max_txlen[mcs][1][0] = (rate * dur) >> 3;
		/* 20MHz, SGI */
		rate = MCS_RATE(mcs, FALSE, TRUE);
		ampdu_tx->max_txlen[mcs][0][1] = (rate * dur) >> 3;
		/* 40 MHz, SGI */
		rate = MCS_RATE(mcs, TRUE, TRUE);
		ampdu_tx->max_txlen[mcs][1][1] = (rate * dur) >> 3;
	}
}

static void
wlc_ampdu_init_min_lens(scb_ampdu_tx_t *scb_ampdu)
{
	int mcs, tmp;
	tmp = 1 << (16 - scb_ampdu->mpdu_density);
	for (mcs = 0; mcs <= AMPDU_MAX_MCS; mcs++) {
		scb_ampdu->min_lens[mcs] = 0;
		if (scb_ampdu->mpdu_density > AMPDU_DENSITY_4_US)
			continue;
		if (mcs >= 12 && mcs <= 15) {
			/* use mcs21 */
			scb_ampdu->min_lens[mcs] = CEIL(MCS_RATE(21, 1, 0), tmp);
		} else if (mcs == 21 || mcs == 22) {
			/* use mcs23 */
			scb_ampdu->min_lens[mcs] = CEIL(MCS_RATE(23, 1, 0), tmp);
		}
	}
	scb_ampdu->min_len = CEIL(MCS_RATE(AMPDU_MAX_MCS, 1, 1), tmp);
}

uint8 BCMFASTPATH
wlc_ampdu_null_delim_cnt(ampdu_tx_info_t *ampdu_tx, struct scb *scb, ratespec_t rspec,
	int phylen, uint16* minbytes)
{
	scb_ampdu_tx_t *scb_ampdu;
	int bytes = 0, cnt, tmp;
	uint8 tx_density;

	ASSERT(scb);
	ASSERT(SCB_AMPDU(scb));

	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu);

	if (scb_ampdu->mpdu_density == 0)
		return 0;

	/* RSPEC2RATE is in kbps units ==> ~RSPEC2RATE/2^13 is in bytes/usec
	   density x is in 2^(x-3) usec
	   ==> # of bytes needed for req density = rate/2^(16-x)
	   ==> # of null delimiters = ceil(ceil(rate/2^(16-x)) - phylen)/4)
	 */

	tx_density = scb_ampdu->mpdu_density;

	if (wlc_btc_wire_get(ampdu_tx->wlc) >= WL_BTC_3WIRE)
		tx_density = AMPDU_DENSITY_8_US;

	ASSERT(tx_density <= AMPDU_MAX_MPDU_DENSITY);
	if (tx_density < 6 && ampdu_tx->wlc->frameburst && RSPEC_IS40MHZ(rspec)) {
		int mcs = rspec & RSPEC_RATE_MASK;
		ASSERT(mcs <= AMPDU_MAX_MCS || mcs == 32);
		if (mcs <= AMPDU_MAX_MCS)
			bytes = scb_ampdu->min_lens[mcs];
	}
	if (bytes == 0) {
		tmp = 1 << (16 - tx_density);
		bytes = CEIL(RSPEC2RATE(rspec), tmp);
	}
	*minbytes = (uint16)bytes;

	if (bytes > phylen) {
		cnt = CEIL(bytes - phylen, AMPDU_DELIMITER_LEN);
		ASSERT(cnt <= 255);
		return (uint8)cnt;
	} else
		return 0;
}

void
wlc_ampdu_macaddr_upd(wlc_info_t *wlc)
{
	char template[T_RAM_ACCESS_SZ*2];

	/* driver needs to write the ta in the template; ta is at offset 16 */
	bzero(template, sizeof(template));
	bcopy((char*)wlc->pub->cur_etheraddr.octet, template, ETHER_ADDR_LEN);
	wlc_write_template_ram(wlc, (T_BA_TPL_BASE + 16), (T_RAM_ACCESS_SZ*2), template);
}

#ifdef MACOSX
/* Return the # of transmit packets pending for given PRIO */
uint
wlc_ampdu_txpktcnt_prio(ampdu_tx_info_t *ampdu_tx, uint prio)
{
	struct scb *scb;
	struct scb_iter scbiter;
	int pktcnt = 0;
	scb_ampdu_tx_t *scb_ampdu;

	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			pktcnt += pktq_plen(&scb_ampdu->txq, prio);
		}
	}

	return pktcnt;
}
#endif /* MACOSX */

#if defined(WLNAR)
/*
 * wlc_ampdu_ba_on_tidmask() - return a bitmask of aggregating TIDs (priorities).
 *
 * Inputs:
 *	scb	pointer to station control block to examine.
 *
 * Returns:
 *	The return value is a bitmask of TIDs for which a block ack agreement exists.
 *	Bit <00> = TID 0, bit <01> = TID 1, etc. A set bit indicates a BA agreement.
 */
extern uint8 BCMFASTPATH
wlc_ampdu_ba_on_tidmask(const struct scb *scb)
{
	uint8 mask = 0;

	/*
	 * If AMPDU_MAX_SCB_TID changes to a larger value, we will need to adjust the
	 * return value from this function, as well as the calling functions.
	 */
	STATIC_ASSERT(AMPDU_MAX_SCB_TID <= NBITS(uint8));

	/* Figure out the wlc this scb belongs to. */

	if (scb && SCB_BSSCFG(scb) && SCB_BSSCFG(scb)->wlc) {

	    wlc_info_t *wlc = SCB_BSSCFG(scb)->wlc;

	    /* See if the scb is ampdu capable and tx aggregation is not disabled. */

	    if (SCB_AMPDU(scb) && wlc->ampdu_tx && (wlc->ampdu_tx->txaggr != OFF)) {

		/* Locate the ampdu scb cubby, then check all TIDs */

		scb_ampdu_tx_t *scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);

		if (scb_ampdu) {

		    int i;

		    for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {

			scb_ampdu_tid_ini_t *ini = scb_ampdu->ini[i];

			/* Set the corresponding bit in the mask if the ba state is ON */

			if (ini && (ini->ba_state == AMPDU_TID_STATE_BA_ON))
			    mask |= (1<<i);
		    }
		}
	    }
	}
	return mask;
}
#endif /* WLNAR */

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
int
wlc_ampdu_tx_dump(ampdu_tx_info_t *ampdu_tx, struct bcmstrbuf *b)
{
#ifdef WLCNT
	wlc_ampdu_tx_cnt_t *cnt = ampdu_tx->cnt;
#endif
	wlc_info_t *wlc = ampdu_tx->wlc;
	wlc_pub_t *pub = wlc->pub;
	int i, j, last;
	uint32 max_val, total = 0;
	struct scb *scb;
	struct scb_iter scbiter;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	int inic = 0, ini_on = 0, ini_off = 0, ini_pon = 0, ini_poff = 0;
	int nbuf = 0;
	wlc_fifo_info_t *fifo;
	char eabuf[ETHER_ADDR_STR_LEN];
#ifdef WLPKTDLYSTAT
	uint ac;
	uint32 sum;
	scb_delay_stats_t *delay_stats;
#endif
#ifdef  WLOVERTHRUSTER
	wlc_tcp_ack_info_t *tcp_ack_info = &ampdu_tx->tcp_ack_info;
#endif  /* WLOVERTHRUSTER */
#ifdef BCMDBG
	int tid;
#endif

	BCM_REFERENCE(pub);

	bcm_bprintf(b, "HOST_ENAB %d UCODE_ENAB %d 4331_HW_ENAB %d AQM_ENAB %d\n",
		AMPDU_HOST_ENAB(pub), AMPDU_UCODE_ENAB(pub),
		AMPDU_HW_ENAB(pub), AMPDU_AQM_ENAB(pub));

	bcm_bprintf(b, "AMPDU Tx counters:\n");
#ifdef WLAMPDU_MAC
	if (wlc->clk && AMPDU_MAC_ENAB(pub) &&
		!AMPDU_AQM_ENAB(pub)) {

		uint32 hwampdu, hwmpdu, hwrxba, hwnoba;
		hwampdu = wlc_read_shm(wlc, M_TXAMPDU_CNT);
		hwmpdu = wlc_read_shm(wlc, M_TXMPDU_CNT);
		hwrxba = wlc_read_shm(wlc, M_RXBA_CNT);
		hwnoba = (hwampdu < hwrxba) ? 0 : (hwampdu - hwrxba);
		bcm_bprintf(b, "%s: txampdu %d txmpdu %d txmpduperampdu %d noba %d (%d%%)\n",
			AMPDU_UCODE_ENAB(pub) ? "Ucode" : "HW",
			hwampdu, hwmpdu,
			hwampdu ? CEIL(hwmpdu, hwampdu) : 0, hwnoba,
			hwampdu ? CEIL(hwnoba*100, hwampdu) : 0);
	}
#endif /* WLAMPDU_MAC */

#ifdef WLCNT

#ifdef WLAMPDU_MAC
	if (wlc->clk && AMPDU_MAC_ENAB(pub)) {
		if (!AMPDU_AQM_ENAB(pub))
		bcm_bprintf(b, "txmpdu(enq) %d txs %d txmrt/succ %d %d (f %d%%) "
			"txfbr/succ %d %d (f %d%%)\n", cnt->txmpdu, cnt->u0.txs,
			cnt->tx_mrt, cnt->txsucc_mrt,
			cnt->tx_mrt ? CEIL((cnt->tx_mrt - cnt->txsucc_mrt)*100, cnt->tx_mrt) : 0,
			cnt->tx_fbr, cnt->txsucc_fbr,
			cnt->tx_fbr ? CEIL((cnt->tx_fbr - cnt->txsucc_fbr)*100, cnt->tx_fbr) : 0);
	} else
#endif /* WLAMPDU_MAC */
	{
		bcm_bprintf(b, "txampdu %d txmpdu %d txmpduperampdu %d noba %d (%d%%)\n",
			cnt->txampdu, cnt->txmpdu,
			cnt->txampdu ? CEIL(cnt->txmpdu, cnt->txampdu) : 0, cnt->noba,
			cnt->txampdu ? CEIL(cnt->noba*100, cnt->txampdu) : 0);
		bcm_bprintf(b, "retry_ampdu %d retry_mpdu %d (%d%%) txfifofull %d\n",
		  cnt->txretry_ampdu, cnt->u0.txretry_mpdu,
		  (cnt->txmpdu ? CEIL(cnt->u0.txretry_mpdu*100, cnt->txmpdu) : 0), cnt->txfifofull);
		bcm_bprintf(b, "fbr_ampdu %d fbr_mpdu %d\n",
			cnt->txfbr_ampdu, cnt->txfbr_mpdu);
	}
	bcm_bprintf(b, "txregmpdu %d txreg_noack %d txfifofull %d txdrop %d txstuck %d orphan %d\n",
		cnt->txregmpdu, cnt->txreg_noack, cnt->txfifofull,
		cnt->txdrop, cnt->txstuck, cnt->orphan);
	bcm_bprintf(b, "txrel_wm %d txrel_size %d sduretry %d sdurejected %d\n",
		cnt->txrel_wm, cnt->txrel_size, cnt->sduretry, cnt->sdurejected);

#ifdef WLAMPDU_MAC
	if (AMPDU_MAC_ENAB(pub)) {
		bcm_bprintf(b, "aggfifo_w %d epochdeltas %d mpduperepoch %d\n",
			cnt->enq, cnt->epochdelta,
			(cnt->epochdelta+1) ? CEIL(cnt->enq, (cnt->epochdelta+1)) : 0);
		bcm_bprintf(b, "epoch_change reason: plcp %d rate %d fbr %d reg %d link %d"
			" seq no %d\n", cnt->echgr1, cnt->echgr2, cnt->echgr3,
			cnt->echgr4, cnt->echgr5, cnt->echgr6);
	}
#endif
	bcm_bprintf(b, "txr0hole %d txrnhole %d txrlag %d rxunexp %d\n",
		cnt->txr0hole, cnt->txrnhole, cnt->txrlag, cnt->rxunexp);
	bcm_bprintf(b, "txaddbareq %d rxaddbaresp %d txbar %d rxba %d txdelba %d \n",
		cnt->txaddbareq, cnt->rxaddbaresp, cnt->txbar, cnt->rxba, cnt->txdelba);

#ifdef WLAMPDU_MAC
	if (AMPDU_MAC_ENAB(pub))
		bcm_bprintf(b, "txmpdu_sgi %d txmpdu_stbc %d\n",
		  cnt->u1.txmpdu_sgi, cnt->u2.txmpdu_stbc);
	else
#endif
		bcm_bprintf(b, "txampdu_sgi %d txampdu_stbc %d "
			"txampdu_mfbr_stbc %d\n", cnt->u1.txampdu_sgi,
			cnt->u2.txampdu_stbc, cnt->txampdu_mfbr_stbc);

#endif /* WLCNT */

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			ASSERT(scb_ampdu);
			for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
				if ((ini = scb_ampdu->ini[i])) {
					inic++;
					if (ini->ba_state == AMPDU_TID_STATE_BA_OFF)
						ini_off++;
					if (ini->ba_state == AMPDU_TID_STATE_BA_ON)
						ini_on++;
					if (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_OFF)
						ini_poff++;
					if (ini->ba_state == AMPDU_TID_STATE_BA_PENDING_ON)
						ini_pon++;
				}
			}
			nbuf += pktq_len(&scb_ampdu->txq);
		}
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "ini %d ini_off %d ini_on %d ini_poff %d ini_pon %d nbuf %d\n",
		inic, ini_off, ini_on, ini_poff, ini_pon, nbuf);

#ifdef WLOVERTHRUSTER
	if (tcp_ack_info->tcp_ack_ratio) {
		bcm_bprintf(b, "tcp_ack_ratio %d/%d total %d/%d dequeued %d multi_dequeued %d\n",
			tcp_ack_info->tcp_ack_ratio, tcp_ack_info->tcp_ack_ratio_depth,
			tcp_ack_info->tcp_ack_total - tcp_ack_info->tcp_ack_dequeued
			- tcp_ack_info->tcp_ack_multi_dequeued,
			tcp_ack_info->tcp_ack_total,
			tcp_ack_info->tcp_ack_dequeued, tcp_ack_info->tcp_ack_multi_dequeued);
	}
#endif /* WLOVERTHRUSTER */

	bcm_bprintf(b, "Supr Reason: pmq(%d) flush(%d) frag(%d) badch(%d) exptime(%d) uf(%d)",
		ampdu_tx->amdbg->supr_reason[TX_STATUS_SUPR_PMQ],
		ampdu_tx->amdbg->supr_reason[TX_STATUS_SUPR_FLUSH],
		ampdu_tx->amdbg->supr_reason[TX_STATUS_SUPR_FRAG],
		ampdu_tx->amdbg->supr_reason[TX_STATUS_SUPR_BADCH],
		ampdu_tx->amdbg->supr_reason[TX_STATUS_SUPR_EXPTIME],
		ampdu_tx->amdbg->supr_reason[TX_STATUS_SUPR_UF]);
#ifdef WLP2P
	if (P2P_ENAB(pub))
		bcm_bprintf(b, " abs(%d)",
			ampdu_tx->amdbg->supr_reason[TX_STATUS_SUPR_NACK_ABS]);
#endif
#ifdef AP
	bcm_bprintf(b, " pps(%d)",
		ampdu_tx->amdbg->supr_reason[TX_STATUS_SUPR_PPS]);
#endif
	bcm_bprintf(b, "\n\n");

#ifdef WLAMPDU_HW
	if (wlc->clk && AMPDU_HW_ENAB(pub)) {
		uint16 stat_addr = 2 * wlc_read_shm(wlc, M_HWAGG_STATS_PTR);
		stat_addr += C_HWAGG_STATS_MPDU_WSZ * 2;
		for (i  = 0; i <= AMPDU_MAX_MCS; i++)
			ampdu_tx->amdbg->txmcs[i] = wlc_read_shm(wlc, stat_addr + i * 2);
	}
#endif

	for (i = 0, total = 0, last = 0; i <= AMPDU_MAX_MCS; i++) {
		total += ampdu_tx->amdbg->txmcs[i];
		if (ampdu_tx->amdbg->txmcs[i]) last = i;
	}
	last = 8 * (last/8 + 1) - 1;
	bcm_bprintf(b, "TX MCS  :");
	if (total) {
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d(%d%%)", ampdu_tx->amdbg->txmcs[i],
				(ampdu_tx->amdbg->txmcs[i] * 100) / total);
			if ((i % 8) == 7 && i != last)
				bcm_bprintf(b, "\n        :");
		}
	}
#ifdef WLAMPDU_MAC
	if (AMPDU_AQM_ENAB(pub)) {
		bcm_bprintf(b, "\nMCS PER :");
		if (total) {
			for (i = 0; i <= last; i++) {
				int unacked = 0, per = 0;
				unacked = ampdu_tx->amdbg->txmcs[i] -
					ampdu_tx->amdbg->txmcs_succ[i];
				if ((unacked > 0) && (ampdu_tx->amdbg->txmcs[i]))
					per = (unacked * 100) / ampdu_tx->amdbg->txmcs[i];
				bcm_bprintf(b, "  %d(%d%%)", unacked, per);
				if ((i % 8) == 7 && i != last)
					bcm_bprintf(b, "\n        :");
			}
		}
	}
#endif /* WLAMPDU_MAC */
#ifdef WL11AC
	for (i = 0, total = 0, last = 0; i < AMPDU_MAX_VHT; i++) {
		total += ampdu_tx->amdbg->txvht[i];
		if (ampdu_tx->amdbg->txvht[i]) last = i;
	}
	last = 10 * (last/10 + 1) - 1;
	bcm_bprintf(b, "\nTX VHT  :");
	if (total) {
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d(%d%%)", ampdu_tx->amdbg->txvht[i],
				(ampdu_tx->amdbg->txvht[i] * 100) / total);
			if ((i % 10) == 9 && i != last)
				bcm_bprintf(b, "\n        :");
		}
	}
#ifdef WLAMPDU_MAC
	if (AMPDU_AQM_ENAB(pub)) {
		bcm_bprintf(b, "\nVHT PER :");
		if (total) {
			for (i = 0; i <= last; i++) {
				int unacked = 0, per = 0;
				unacked = ampdu_tx->amdbg->txvht[i] -
					ampdu_tx->amdbg->txvht_succ[i];
				if ((unacked > 0) && (ampdu_tx->amdbg->txvht[i]))
					per = (unacked * 100) / ampdu_tx->amdbg->txvht[i];
				bcm_bprintf(b, "  %d(%d%%)", unacked, per);
				if (((i % 10) == 9) && (i != last))
					bcm_bprintf(b, "\n        :");
			}
		}
		bcm_bprintf(b, "\n");
	}
#endif /* WLAMPDU_MAC */
#endif /* WL11AC */

#if defined(WLPKTDLYSTAT) && defined(WLCNT)
#if defined(WLAMPDU_HW)
	if (wlc->clk && AMPDU_HW_ENAB(pub)) {
		uint16 stat_addr = 2 * wlc_read_shm(wlc, M_HWAGG_STATS_PTR);
		stat_addr += C_HWAGG_STATS_MPDU_WSZ * 2;
		for (i  = 0; i <= AMPDU_MAX_MCS; i++) {
			cnt->txmpdu_cnt[i] = wlc_read_shm(wlc, stat_addr + i * 2);
			/* cnt->txmpdu_succ_cnt[i] =
			 * 	wlc_read_shm(wlc, stat_addr + i * 2 +
			 *	C_HWAGG_STATS_TXMCS_WSZ * 2);
			 */
		}
	}
#endif

	/* Report PER statistics (for each MCS) */
	for (i = 0, max_val = 0, last = 0; i <= AMPDU_MAX_MCS; i++) {
		max_val += cnt->txmpdu_cnt[i];
		if (cnt->txmpdu_cnt[i]) last = i;
	}
	last = 8 * (last/8 + 1) - 1;
	if (max_val) {
		bcm_bprintf(b, "PER : ");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d/%d (%d%%)", (cnt->txmpdu_cnt[i] -
			 cnt->txmpdu_succ_cnt[i]), cnt->txmpdu_cnt[i],
			(cnt->txmpdu_cnt[i] > 0) ? ((cnt->txmpdu_cnt[i] -
			 cnt->txmpdu_succ_cnt[i]) * 100) / cnt->txmpdu_cnt[i] : 0);
			if ((i % 8) == 7 && i != last)
			bcm_bprintf(b, "\n    : ");
		}
		bcm_bprintf(b, "\n");
	}
#endif /* WLPKTDLYSTAT && WLCNT */

#ifdef WLAMPDU_HW
	if (wlc->clk) {
		uint16 stat_addr = 0;
		uint16 val, ucode_ampdu;
		int cnt1 = 0, cnt2 = 0;
		uint16 nbins;

		nbins = sizeof(ampdu_tx->amdbg->mpdu_histogram);
		if (nbins > C_MPDUDEN_NBINS)
			nbins = C_MPDUDEN_NBINS;
		if (AMPDU_HW_ENAB(pub) || AMPDU_AQM_ENAB(pub)) {
			stat_addr = 2 * wlc_read_shm(wlc,
				(AMPDU_HW_ENAB(pub)) ? M_HWAGG_STATS_PTR : M_AMP_STATS_PTR);
			for (i = 0, total = 0; i < nbins; i++) {
				val = wlc_read_shm(wlc, stat_addr + i * 2);
				ampdu_tx->amdbg->mpdu_histogram[i] = val;
				total += val;
				cnt1 += (val * (i+1));
			}
			bcm_bprintf(b, "--------------------------\n");
			if (AMPDU_HW_ENAB(pub)) {
				int rempty;
				rempty = wlc_read_shm(wlc, stat_addr + C_HWAGG_RDEMP_WOFF * 2);
				ucode_ampdu = wlc_read_shm(wlc, stat_addr + C_HWAGG_NAMPDU_WOFF*2);
				bcm_bprintf(b, "tot_mpdus %d tot_ampdus %d mpduperampdu %d "
				    "ucode_ampdu %d rempty %d (%d%%)\n",
				    cnt1, total,
				    (total == 0) ? 0 : CEIL(cnt1, total),
				    ucode_ampdu, rempty,
				    (ucode_ampdu == 0) ? 0 : CEIL(rempty * 100, ucode_ampdu));
			} else {
				bcm_bprintf(b, "tot_mpdus %d tot_ampdus %d mpduperampdu %d\n",
					cnt1, total, (total == 0) ? 0 : CEIL(cnt1, total));
			}
		}

		if (AMPDU_AQM_ENAB(pub)) {
			/* print out agg stop reason */
			uint16 stop_len, stop_mpdu, stop_bawin, stop_epoch, stop_fempty;
			stat_addr  += (C_MPDUDEN_NBINS * 2);
			stop_len    = wlc_read_shm(wlc, stat_addr);
			stop_mpdu   = wlc_read_shm(wlc, stat_addr + 2);
			stop_bawin  = wlc_read_shm(wlc, stat_addr + 4);
			stop_epoch  = wlc_read_shm(wlc, stat_addr + 6);
			stop_fempty = wlc_read_shm(wlc, stat_addr + 8);
			total = stop_len + stop_mpdu + stop_bawin + stop_epoch + stop_fempty;
			if (total) {
				bcm_bprintf(b, "agg stop reason: len %d (%d%%) ampdu_mpdu %d (%d%%)"
					    " bawin %d (%d%%) epoch %d (%d%%) fempty %d (%d%%)\n",
					    stop_len, (stop_len * 100) / total,
					    stop_mpdu, (stop_mpdu * 100) / total,
					    stop_bawin, (stop_bawin * 100) / total,
					    stop_epoch, (stop_epoch * 100) / total,
					    stop_fempty, (stop_fempty * 100) / total);
			}
			stat_addr  += C_AGGSTOP_NBINS * 2;
		}

		if (wlc->frameburst && (D11REV_IS(pub->corerev, 26) ||
		    D11REV_IS(pub->corerev, 29) || AMPDU_AQM_ENAB(pub))) {
			/* burst size */
			cnt1 = 0;
			if (!AMPDU_AQM_ENAB(pub))
				stat_addr += (C_MBURST_WOFF * 2);
			bcm_bprintf(b, "Frameburst histogram:");
			for (i = 0; i < C_MBURST_NBINS; i++) {
				val = wlc_read_shm(wlc, stat_addr + i * 2);
				cnt1 += val;
				cnt2 += (i+1) * val;
				bcm_bprintf(b, "  %d", val);
			}
			bcm_bprintf(b, " avg %d\n", (cnt1 == 0) ? 0 : CEIL(cnt2, cnt1));
			bcm_bprintf(b, "--------------------------\n");
		}
	}
#endif /* WLAMPDU_HW */

	for (i = 0, last = 0, total = 0; i <= AMPDU_MAX_MPDU; i++) {
		total += ampdu_tx->amdbg->mpdu_histogram[i];
		if (ampdu_tx->amdbg->mpdu_histogram[i])
			last = i;
	}
	last = 8 * (last/8 + 1) - 1;
	bcm_bprintf(b, "MPDUdens:");
	for (i = 0; i <= last; i++) {
		bcm_bprintf(b, " %3d (%d%%)", ampdu_tx->amdbg->mpdu_histogram[i],
			(total == 0) ? 0 :
			(ampdu_tx->amdbg->mpdu_histogram[i] * 100 / total));
		if ((i % 8) == 7 && i != last)
			bcm_bprintf(b, "\n        :");
	}
	bcm_bprintf(b, "\n");

	if (AMPDU_HOST_ENAB(pub)) {
		for (i = 0, last = 0; i <= AMPDU_MAX_MPDU; i++)
			if (ampdu_tx->amdbg->retry_histogram[i])
				last = i;
		bcm_bprintf(b, "Retry   :");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, " %3d", ampdu_tx->amdbg->retry_histogram[i]);
			if ((i % 8) == 7 && i != last)
				bcm_bprintf(b, "\n        :");
		}
		bcm_bprintf(b, "\n");

		for (i = 0, last = 0; i <= AMPDU_MAX_MPDU; i++)
			if (ampdu_tx->amdbg->end_histogram[i])
				last = i;
		bcm_bprintf(b, "Till End:");
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, " %3d", ampdu_tx->amdbg->end_histogram[i]);
			if ((i % 8) == 7 && i != last)
				bcm_bprintf(b, "\n        :");
		}
		bcm_bprintf(b, "\n");
	}

	if (WLC_SGI_CAP_PHY(wlc)) {
		bcm_bprintf(b, "TX MCS SGI:");
		for (i = 0, max_val = 0; i <= AMPDU_MAX_MCS; i++) {
			max_val += ampdu_tx->amdbg->txmcssgi[i];
			if (ampdu_tx->amdbg->txmcssgi[i]) last = i;
		}
		last = 8 * (last/8 + 1) - 1;
		if (max_val) {
			for (i = 0; i <= last; i++) {
				bcm_bprintf(b, "  %d(%d%%)", ampdu_tx->amdbg->txmcssgi[i],
				            (ampdu_tx->amdbg->txmcssgi[i] * 100) / max_val);
				if ((i % 8) == 7 && i != last)
					bcm_bprintf(b, "\n          :");
			}
		}

#ifdef WL11AC
		bcm_bprintf(b, "\nTX VHT SGI:");
		for (i = 0, max_val = 0; i < AMPDU_MAX_VHT; i++) {
			max_val += ampdu_tx->amdbg->txvhtsgi[i];
			if (ampdu_tx->amdbg->txvhtsgi[i]) last = i;
		}
		last = 10 * (last/10 + 1) - 1;
		if (max_val) {
			for (i = 0; i <= last; i++) {
				bcm_bprintf(b, "  %d(%d%%)", ampdu_tx->amdbg->txvhtsgi[i],
					(ampdu_tx->amdbg->txvhtsgi[i] * 100) / max_val);
				if (((i % 10) == 9) && i != last)
					bcm_bprintf(b, "\n          :");
			}
		}
#endif /* WL11AC */
		bcm_bprintf(b, "\n");

		if (WLCISLCNPHY(wlc->band) || (NREV_GT(wlc->band->phyrev, 3) &&
			NREV_LE(wlc->band->phyrev, 6)))
		{
			bcm_bprintf(b, "TX MCS STBC:");
			for (i = 0, max_val = 0; i <= AMPDU_MAX_MCS; i++)
				max_val += ampdu_tx->amdbg->txmcsstbc[i];
			if (max_val) {
				for (i = 0; i <= 7; i++)
					bcm_bprintf(b, "  %d(%d%%)", ampdu_tx->amdbg->txmcsstbc[i],
						(ampdu_tx->amdbg->txmcsstbc[i] * 100) / max_val);
			}

#ifdef WL11AC
			for (i = 0, max_val = 0; i < AMPDU_MAX_VHT; i++)
				max_val += ampdu_tx->amdbg->txvhtstbc[i];
			if (max_val) {
				bcm_bprintf(b, "\nTX VHT STBC:");
				for (i = 0; i < 10; i++) {
					bcm_bprintf(b, "  %d(%d%%)", ampdu_tx->amdbg->txvhtstbc[i],
					(ampdu_tx->amdbg->txvhtstbc[i] * 100) / max_val);
				}
			}
#endif /* WL11AC */
			bcm_bprintf(b, "\n");
		}
	}

	bcm_bprintf(b, "MCS to AMPDU tables:");
	for (j = 0; j < NUM_FFPLD_FIFO; j++) {
		fifo = ampdu_tx->fifo_tb + j;
		if (fifo->ampdu_pld_size || fifo->dmaxferrate) {
			bcm_bprintf(b, " FIFO %d: Preload settings: size %d dmarate %d kbps\n",
			          j, fifo->ampdu_pld_size, fifo->dmaxferrate);
			bcm_bprintf(b, "        ");
			for (i = 0; i <= AMPDU_MAX_MCS; i++) {
				bcm_bprintf(b, " %d", fifo->mcs2ampdu_table[i]);
				if ((i % 8) == 7 && i != AMPDU_MAX_MCS)
					bcm_bprintf(b, "\n       :");
			}
			bcm_bprintf(b, "\n");
		}
	}
	bcm_bprintf(b, "\n");

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			bcm_bprintf(b, "%s: max_pdu %d release %d\n",
				bcm_ether_ntoa(&scb->ea, eabuf),
				scb_ampdu->max_pdu, scb_ampdu->release);
#ifdef BCMDBG
			bcm_bprintf(b, "\ttxdrop %u txstuck %u "
			            "txaddbareq %u txrlag %u sdurejected %u\n"
			            "\ttxmpdu %u txbar %d txreg_noack %u noba %u "
			            "rxaddbaresp %u\n",
			            scb_ampdu->cnt.txdrop, scb_ampdu->cnt.txstuck,
			            scb_ampdu->cnt.txaddbareq, scb_ampdu->cnt.txrlag,
			            scb_ampdu->cnt.sdurejected, scb_ampdu->cnt.txmpdu,
			            scb_ampdu->cnt.txbar, scb_ampdu->cnt.txreg_noack,
			            scb_ampdu->cnt.noba, scb_ampdu->cnt.rxaddbaresp);
			bcm_bprintf(b, "\ttxnoroom %u\n", scb_ampdu->cnt.txnoroom);
			for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
				ini = scb_ampdu->ini[tid];
				if (!ini)
					continue;
				if (!(ini->ba_state == AMPDU_TID_STATE_BA_ON))
					continue;

				bcm_bprintf(b, "\tba_state %d ba_wsize %d tx_in_transit %d "
					"tid %d rem_window %d\n",
					ini->ba_state, ini->ba_wsize, ini->tx_in_transit,
					ini->tid, ini->rem_window);
				bcm_bprintf(b, "\tstart_seq 0x%x max_seq 0x%x tx_exp_seq 0x%x"
					" bar_ackpending_seq 0x%x\n",
					ini->start_seq, ini->max_seq, ini->tx_exp_seq,
					ini->bar_ackpending_seq);
				bcm_bprintf(b, "\tbar_ackpending %d free_me %d alive %d "
					"retry_bar %d\n",
					ini->bar_ackpending, ini->free_me,
					ini->alive, ini->retry_bar);
			}
#endif /* BCMDBG */
		}
#ifdef WLPKTDLYSTAT
		/* Report Latency and packet loss statistics (for each AC) */
		for (ac = 0; ac < AC_COUNT; ac++) {

			delay_stats = &scb->delay_stats[ac];
			for (i = 0, total = 0, sum = 0; i < RETRY_SHORT_DEF; i++) {
				total += delay_stats->txmpdu_cnt[i];
				sum += delay_stats->delay_sum[i];
			}
			if (total) {
				bcm_bprintf(b, "%s:  PLoss %d/%d (%d%%)\n",
				 ac_names[ac], delay_stats->txmpdu_lost, total,
				  (delay_stats->txmpdu_lost * 100) / total);
#if defined(WLC_HIGH_ONLY)
				bcm_bprintf(b, "\tDelay stats in ms (avg/min/max): %d %d %d\n",
				 CEIL(sum, total), delay_stats->delay_min, delay_stats->delay_max);
#else
				bcm_bprintf(b, "\tDelay stats in usec (avg/min/max): %d %d %d\n",
				 CEIL(sum, total), delay_stats->delay_min, delay_stats->delay_max);
#endif
				bcm_bprintf(b, "\tTxcnt Hist    :");
				for (i = 0; i < RETRY_SHORT_DEF; i++) {
					bcm_bprintf(b, "  %d", delay_stats->txmpdu_cnt[i]);
				}
				bcm_bprintf(b, "\n\tDelay vs Txcnt:");
				for (i = 0; i < RETRY_SHORT_DEF; i++) {
					if (delay_stats->txmpdu_cnt[i])
						bcm_bprintf(b, "  %d", delay_stats->delay_sum[i]/
						delay_stats->txmpdu_cnt[i]);
					else
						bcm_bprintf(b, "  -1");
				}
				bcm_bprintf(b, "\n");

				for (i = 0, max_val = 0, last = 0; i < WLPKTDLY_HIST_NBINS; i++) {
					max_val += delay_stats->delay_hist[i];
					if (delay_stats->delay_hist[i]) last = i;
				}
				last = 8 * (last/8 + 1) - 1;
				if (max_val) {
					bcm_bprintf(b, "\tDelay Hist \n\t\t:");
					for (i = 0; i <= last; i++) {
						bcm_bprintf(b, " %d(%d%%)", delay_stats->
						delay_hist[i], (delay_stats->delay_hist[i] * 100)
						 / max_val);
						if ((i % 8) == 7 && i != last)
							bcm_bprintf(b, "\n\t\t:");
					}
				}
				bcm_bprintf(b, "\n");
			}
		}
		bcm_bprintf(b, "\n");
#endif /* WLPKTDLYSTAT */
	}

#ifdef WLAMPDU_HW
	if (wlc->clk && AMPDU_HW_ENAB(pub) && b) {
		d11regs_t* regs = wlc->regs;
		bcm_bprintf(b, "AGGFIFO regs: availcnt 0x%x\n", R_REG(wlc->osh, &regs->aggfifocnt));
		bcm_bprintf(b, "cmd 0x%04x stat 0x%04x cfgctl 0x%04x cfgdata 0x%04x mpdunum 0x%02x "
			"len 0x%04x bmp 0x%04x ackedcnt %d\n",
			R_REG(wlc->osh, &regs->u.d11regs.aggfifo_cmd),
			R_REG(wlc->osh, &regs->u.d11regs.aggfifo_stat),
			R_REG(wlc->osh, &regs->u.d11regs.aggfifo_cfgctl),
			R_REG(wlc->osh, &regs->u.d11regs.aggfifo_cfgdata),
			R_REG(wlc->osh, &regs->u.d11regs.aggfifo_mpdunum),
			R_REG(wlc->osh, &regs->u.d11regs.aggfifo_len),
			R_REG(wlc->osh, &regs->u.d11regs.aggfifo_bmp),
			R_REG(wlc->osh, &regs->u.d11regs.aggfifo_ackedcnt));
#if defined(WLCNT) && defined(WLAMPDU_MAC)
		bcm_bprintf(b, "driver statistics: aggfifo pending %d enque/cons %d %d",
			cnt->pending, cnt->enq, cnt->cons);
#endif
	}
#endif /* WLAMPDU_HW */
	bcm_bprintf(b, "\n");

	return 0;
}
#endif /* BCMDBG || WLTEST */

#ifdef BCMDBG
static void
ampdu_dump_ini(scb_ampdu_tid_ini_t *ini)
{
	printf("ba_state %d ba_wsize %d tx_in_transit %d tid %d rem_window %d\n",
		ini->ba_state, ini->ba_wsize, ini->tx_in_transit, ini->tid, ini->rem_window);
	printf("start_seq 0x%x max_seq 0x%x tx_exp_seq 0x%x bar_ackpending_seq 0x%x\n",
		ini->start_seq, ini->max_seq, ini->tx_exp_seq, ini->bar_ackpending_seq);
	printf("bar_ackpending %d free_me %d alive %d retry_bar %d\n",
		ini->bar_ackpending, ini->free_me, ini->alive, ini->retry_bar);
	printf("retry_head %d retry_tail %d retry_cnt %d\n",
		ini->retry_head, ini->retry_tail, ini->retry_cnt);
	prhex("ackpending", &ini->ackpending[0], sizeof(ini->ackpending));
	prhex("barpending", &ini->barpending[0], sizeof(ini->barpending));
	prhex("txretry", &ini->txretry[0], sizeof(ini->txretry));
	prhex("retry_seq", (uint8 *)&ini->retry_seq[0], sizeof(ini->retry_seq));
}
#endif /* BCMDBG */

void
wlc_sidechannel_init(ampdu_tx_info_t *ampdu_tx)
{
	wlc_info_t *wlc = ampdu_tx->wlc;
#ifdef WLAMPDU_UCODE
	uint16 txfs_waddr; /* side channel base addr in 16 bit units */
	uint16 txfsd_addr; /* side channel descriptor base addr */
	const uint8 txfs_wsz[AC_COUNT] =
	        {10, 32, 4, 4}; /* Size of side channel fifos in 16 bit units */
	ampdumac_info_t *hagg;
	int i;
#endif /* WLAMPDU_UCODE */

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	if (!(D11REV_IS(wlc->pub->corerev, 26) || D11REV_IS(wlc->pub->corerev, 29)) ||
	    !AMPDU_MAC_ENAB(wlc->pub) || AMPDU_AQM_ENAB(wlc->pub)) {
		WL_INFORM(("wl%d: %s; NOT UCODE or HW aggregation or side channel"
			"not supported\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	ASSERT(ampdu_tx);

#ifdef WLAMPDU_UCODE
	if (AMPDU_UCODE_ENAB(wlc->pub)) {
		ASSERT(txfs_wsz[0] + txfs_wsz[1] + txfs_wsz[2] + txfs_wsz[3] <= TOT_TXFS_WSIZE);
		txfs_waddr = wlc_read_shm(wlc, M_TXFS_PTR);
		txfsd_addr = (txfs_waddr + C_TXFSD_WOFFSET) * 2;
		for (i = 0; i < AC_COUNT; i++) {
			/* 16 bit word arithmetic */
			hagg = &(ampdu_tx->hagg[i]);
			hagg->txfs_addr_strt = txfs_waddr;
			hagg->txfs_addr_end = txfs_waddr + txfs_wsz[i] - 1;
			hagg->txfs_wptr = hagg->txfs_addr_strt;
			hagg->txfs_rptr = hagg->txfs_addr_strt;
			hagg->txfs_wsize = txfs_wsz[i];
			/* write_shmem takes a 8 bit address and 16 bit pointer */
			wlc_write_shm(wlc, C_TXFSD_STRT_POS(txfsd_addr, i),
				hagg->txfs_addr_strt);
			wlc_write_shm(wlc, C_TXFSD_END_POS(txfsd_addr, i),
				hagg->txfs_addr_end);
			wlc_write_shm(wlc, C_TXFSD_WPTR_POS(txfsd_addr, i), hagg->txfs_wptr);
			wlc_write_shm(wlc, C_TXFSD_RPTR_POS(txfsd_addr, i), hagg->txfs_rptr);
			wlc_write_shm(wlc, C_TXFSD_RNUM_POS(txfsd_addr, i), 0);
			WL_AMPDU_HW(("%d: start 0x%x 0x%x, end 0x%x 0x%x, w 0x%x 0x%x,"
				" r 0x%x 0x%x, sz 0x%x 0x%x\n",
				i,
				C_TXFSD_STRT_POS(txfsd_addr, i), hagg->txfs_addr_strt,
				C_TXFSD_END_POS(txfsd_addr, i),  hagg->txfs_addr_end,
				C_TXFSD_WPTR_POS(txfsd_addr, i), hagg->txfs_wptr,
				C_TXFSD_RPTR_POS(txfsd_addr, i), hagg->txfs_rptr,
				C_TXFSD_RNUM_POS(txfsd_addr, i), 0));
			hagg->txfs_wptr_addr = C_TXFSD_WPTR_POS(txfsd_addr, i);
			txfs_waddr += txfs_wsz[i];
		}
	}
#endif /* WLAMPDU_UCODE */

#ifdef WLAMPDU_HW
	if (AMPDU_HW_ENAB(wlc->pub)) {
		ampdu_tx->aggfifo_depth = AGGFIFO_CAP - 1;
	}
#endif
}

#ifdef WLAMPDU_MAC
/* For ucode agg:
 * enqueue an entry to side channel (a.k.a. aggfifo) and inc the wptr
 * The actual capacity of the buffer is one less than the actual length
 * of the buffer so that an empty and a full buffer can be
 * distinguished.  An empty buffer will have the readPostion and the
 * writePosition equal to each other.  A full buffer will have
 * the writePosition one less than the readPostion.
 *
 * The Objects available to be read go from the readPosition to the writePosition,
 * wrapping around the end of the buffer.  The space available for writing
 * goes from the write position to one less than the readPosition,
 * wrapping around the end of the buffer.
 *
 * For hw agg: it simply enqueues an entry to aggfifo
 */
static int
aggfifo_enque(ampdu_tx_info_t *ampdu_tx, int length, int qid)
{
	ampdumac_info_t *hagg = &(ampdu_tx->hagg[qid]);
#if defined(WLAMPDU_HW) || defined(WLAMPDU_UCODE)
	uint16 epoch = hagg->epoch;
	uint32 entry;

	if (length > (MPDU_LEN_MASK >> MPDU_LEN_SHIFT))
		WL_ERROR(("%s: Length too long %d\n", __FUNCTION__, length));
#endif

#ifdef WLAMPDU_HW
	if (AMPDU_HW_ENAB(ampdu_tx->wlc->pub)) {
		d11regs_t *regs = ampdu_tx->wlc->regs;

		entry = length |
			((epoch ? 1 : 0) << MPDU_EPOCH_HW_SHIFT) | (qid << MPDU_FIFOSEL_SHIFT);

		W_REG(ampdu_tx->wlc->osh, &regs->aggfifodata, entry);
		WLCNTINCR(ampdu_tx->cnt->enq);
		WLCNTINCR(ampdu_tx->cnt->pending);
		hagg->in_queue ++;
		WL_AMPDU_HW(("%s: aggfifo %d entry 0x%04x\n", __FUNCTION__, qid, entry));
	}
#endif /* WLAMPDU_HW */

#ifdef WLAMPDU_UCODE
	if (AMPDU_UCODE_ENAB(ampdu_tx->wlc->pub)) {
		uint16 rptr = hagg->txfs_rptr;
		uint16 wptr_next;

		entry = length | (((epoch ? 1 : 0)) << MPDU_EPOCH_SHIFT);

		/* wptr always points to the next available entry to be written */
		if (hagg->txfs_wptr == hagg->txfs_addr_end) {
			wptr_next = hagg->txfs_addr_strt;	/* wrap */
		} else {
			wptr_next = hagg->txfs_wptr + 1;
		}

		if (wptr_next == rptr) {
			WL_ERROR(("%s: side channel %d is full !!!\n", __FUNCTION__, qid));
			ASSERT(0);
			return -1;
		}

		/* Convert word addr to byte addr */
		wlc_write_shm(ampdu_tx->wlc, hagg->txfs_wptr * 2, entry);

		WL_AMPDU_HW(("%s: aggfifo %d rptr 0x%x wptr 0x%x entry 0x%04x\n",
			__FUNCTION__, qid, rptr, hagg->txfs_wptr, entry));

		hagg->txfs_wptr = wptr_next;
		wlc_write_shm(ampdu_tx->wlc, hagg->txfs_wptr_addr, hagg->txfs_wptr);
	}
#endif /* WLAMPDU_UCODE */

	WLCNTINCR(ampdu_tx->cnt->enq);
	WLCNTINCR(ampdu_tx->cnt->pending);
	hagg->in_queue ++;
	return 0;
}

/* For ucode agg:
 * Side channel queue is setup with a read and write ptr.
 * - R == W means empty.
 * - (W + 1 % size) == R means full.
 * - Max buffer capacity is size - 1 elements (one element remains unused).
 * For hw agg:
 * simply read ihr reg
 */
static uint
get_aggfifo_space(ampdu_tx_info_t *ampdu_tx, int qid)
{
	uint ret = 0;
	d11regs_t *regs;
	regs = ampdu_tx->wlc->regs;

#ifdef WLAMPDU_HW
	if (AMPDU_HW_ENAB(ampdu_tx->wlc->pub)) {
		uint actual;
		ret = R_REG(ampdu_tx->wlc->osh, &regs->aggfifocnt);
		switch (qid) {
			case 3:
				ret >>= 8;
			case 2:
				ret >>= 8;
			case 1:
				ret >>= 8;
			case 0:
				ret &= 0x7f;
				break;
			default:
				ASSERT(0);
		}
		actual = ret;
		BCM_REFERENCE(actual);

		if (ret >= (uint)(AGGFIFO_CAP - ampdu_tx->aggfifo_depth))
			ret -= (AGGFIFO_CAP - ampdu_tx->aggfifo_depth);
		else
			ret = 0;

		/* due to the txstatus can only hold 32 bits in bitmap, limit the size to 32 */
		WL_AMPDU_HW(("%s: fifo %d fifo availcnt %d ret %d\n", __FUNCTION__, qid, actual,
			ret));
	}
#endif	/* WLAMPDU_HW */
#ifdef WLAMPDU_UCODE
	if (AMPDU_UCODE_ENAB(ampdu_tx->wlc->pub)) {
		if (ampdu_tx->hagg[qid].txfs_wptr < ampdu_tx->hagg[qid].txfs_rptr)
			ret = ampdu_tx->hagg[qid].txfs_rptr - ampdu_tx->hagg[qid].txfs_wptr - 1;
		else
			ret = (ampdu_tx->hagg[qid].txfs_wsize - 1) -
			      (ampdu_tx->hagg[qid].txfs_wptr - ampdu_tx->hagg[qid].txfs_rptr);
		ASSERT(ret < ampdu_tx->hagg[qid].txfs_wsize);
		WL_AMPDU_HW(("%s: fifo %d rptr %04x wptr %04x availcnt %d\n", __FUNCTION__,
			qid, ampdu_tx->hagg[qid].txfs_rptr, ampdu_tx->hagg[qid].txfs_wptr, ret));
	}
#endif /* WLAMPDU_UCODE */
	return ret;
}
#endif /* WLAMPDU_MAC */


#ifdef WLAMPDU_PRECEDENCE
static bool BCMFASTPATH
wlc_ampdu_prec_enq(wlc_info_t *wlc, struct pktq *q, void *pkt, int tid)
{
	uint8 prec;

	if (WLPKTTAG(pkt)->flags2 & WLF2_FAVORED)
		prec = WLC_PRIO_TO_HI_PREC(tid);
	else
		prec = WLC_PRIO_TO_PREC(tid);

	return (wlc_prec_enq_head(wlc, q, pkt, prec, FALSE));
}

static void * BCMFASTPATH
wlc_ampdu_pktq_pdeq(struct pktq *pq, int tid)
{
	void *p;

	p = pktq_pdeq(pq, WLC_PRIO_TO_HI_PREC(tid));

	if (p == NULL)
		p = pktq_pdeq(pq, WLC_PRIO_TO_PREC(tid));

	return p;
}

static void
wlc_ampdu_pktq_pflush(osl_t *osh, struct pktq *pq, int tid, bool dir, ifpkt_cb_t fn, int arg)
{
	pktq_pflush(osh, pq, WLC_PRIO_TO_HI_PREC(tid), dir, fn, arg);
	pktq_pflush(osh, pq, WLC_PRIO_TO_PREC(tid), dir, fn, arg);
}

#endif /* WLAMPDU_PRECEDENCE */

#if defined(BCMDBG) || defined(WLTEST) || defined(WLPKTDLYSTAT) || \
	defined(BCMDBG_AMPDU)
#ifdef WLCNT
void
wlc_ampdu_clear_tx_dump(ampdu_tx_info_t *ampdu_tx)
{
#if defined(BCMDBG) || defined(WLPKTDLYSTAT)
	struct scb *scb;
	struct scb_iter scbiter;
#endif /* BCMDBG || WLPKTDLYSTAT */
#ifdef BCMDBG
	scb_ampdu_tx_t *scb_ampdu;
#endif /* BCMDBG */

	/* zero the counters */
	bzero(ampdu_tx->cnt, sizeof(wlc_ampdu_tx_cnt_t));
	/* reset the histogram as well */
	if (ampdu_tx->amdbg) {
		bzero(ampdu_tx->amdbg->retry_histogram, sizeof(ampdu_tx->amdbg->retry_histogram));
		bzero(ampdu_tx->amdbg->end_histogram, sizeof(ampdu_tx->amdbg->end_histogram));
		bzero(ampdu_tx->amdbg->mpdu_histogram, sizeof(ampdu_tx->amdbg->mpdu_histogram));
		bzero(ampdu_tx->amdbg->supr_reason, sizeof(ampdu_tx->amdbg->supr_reason));
		bzero(ampdu_tx->amdbg->txmcs, sizeof(ampdu_tx->amdbg->txmcs));
		bzero(ampdu_tx->amdbg->txmcssgi, sizeof(ampdu_tx->amdbg->txmcssgi));
		bzero(ampdu_tx->amdbg->txmcsstbc, sizeof(ampdu_tx->amdbg->txmcsstbc));
#ifdef WLAMPDU_MAC
		bzero(ampdu_tx->amdbg->txmcs_succ, sizeof(ampdu_tx->amdbg->txmcs_succ));
#endif

#ifdef WL11AC
		bzero(ampdu_tx->amdbg->txvht, sizeof(ampdu_tx->amdbg->txvht));
		bzero(ampdu_tx->amdbg->txvhtsgi, sizeof(ampdu_tx->amdbg->txvhtsgi));
		bzero(ampdu_tx->amdbg->txvhtstbc, sizeof(ampdu_tx->amdbg->txvhtstbc));
#ifdef WLAMPDU_MAC
		bzero(ampdu_tx->amdbg->txvht_succ, sizeof(ampdu_tx->amdbg->txvht_succ));
#endif
#endif /* WL11AC */
	}
#ifdef WLOVERTHRUSTER
	ampdu_tx->tcp_ack_info.tcp_ack_total = 0;
	ampdu_tx->tcp_ack_info.tcp_ack_dequeued = 0;
	ampdu_tx->tcp_ack_info.tcp_ack_multi_dequeued = 0;
#endif /* WLOVERTHRUSTER */

#if defined(BCMDBG) || defined(WLPKTDLYSTAT)
	FOREACHSCB(ampdu_tx->wlc->scbstate, &scbiter, scb) {
#ifdef WLPKTDLYSTAT
		/* reset the per-SCB delay statistics */
		bzero(scb->delay_stats, sizeof(scb->delay_stats));
#endif
#ifdef BCMDBG
		if (SCB_AMPDU(scb)) {
			/* reset the per-SCB statistics */
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
			bzero(&scb_ampdu->cnt, sizeof(scb_ampdu_cnt_tx_t));
		}
#endif /* BCMDBG */
	}
#endif /* BCMDBG || WLPKTDLYSTAT */

#ifdef WLAMPDU_MAC
	if (ampdu_tx->wlc->clk) {
#ifdef WLAMPDU_UCODE
		if (ampdu_tx->amdbg) {
			bzero(ampdu_tx->amdbg->schan_depth_histo,
				sizeof(ampdu_tx->amdbg->schan_depth_histo));
		}
#endif
	/* zero out shmem counters */
		if (AMPDU_MAC_ENAB(ampdu_tx->wlc->pub) && !AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
			wlc_write_shm(ampdu_tx->wlc, M_TXMPDU_CNT, 0);
			wlc_write_shm(ampdu_tx->wlc, M_TXAMPDU_CNT, 0);
		}

#ifdef WLAMPDU_HW
		if (AMPDU_HW_ENAB(ampdu_tx->wlc->pub)) {
			if (D11REV_IS(ampdu_tx->wlc->pub->corerev, 26) ||
				D11REV_IS(ampdu_tx->wlc->pub->corerev, 29)) {
				int i;
				uint16 stat_addr = 2 * wlc_read_shm(ampdu_tx->wlc,
					M_HWAGG_STATS_PTR);
				for (i = 0; i < (C_MBURST_WOFF + C_MBURST_NBINS); i++)
					wlc_write_shm(ampdu_tx->wlc, stat_addr + i * 2, 0);
			}
		} else if (AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
			int i;
			uint16 stat_addr = 2 * wlc_read_shm(ampdu_tx->wlc, M_AMP_STATS_PTR);
			for (i = 0; i < C_AMP_STATS_SIZE; i++)
				wlc_write_shm(ampdu_tx->wlc, stat_addr + i * 2, 0);
		}
#endif /* WLAMPDU_HW */
	}
#endif /* WLAMPDU_MAC */
}
#endif /* WLCNT */
#endif /* defined(BCMDBG) || defined(WLTEST) */

void
wlc_ampdu_tx_send_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	uint16 initiator, uint16 reason)
{
	ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, FALSE);

	WL_ERROR(("wl%d: %s: tid %d initiator %d reason %d\n",
		ampdu_tx->wlc->pub->unit, __FUNCTION__, tid, initiator, reason));

	wlc_send_delba(ampdu_tx->wlc, scb, tid, initiator, reason);

	WLCNTINCR(ampdu_tx->cnt->txdelba);
}

void
wlc_ampdu_tx_recv_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid, uint8 category,
	uint16 initiator, uint16 reason)
{
	scb_ampdu_tx_t *scb_ampdu_tx;

	ASSERT(scb);

	scb_ampdu_tx = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu_tx);

	if (category & DOT11_ACTION_CAT_ERR_MASK) {
		WL_ERROR(("wl%d: %s: unexp error action frame\n",
			ampdu_tx->wlc->pub->unit, __FUNCTION__));
		WLCNTINCR(ampdu_tx->cnt->rxunexp);
		return;
	}

	ampdu_cleanup_tid_ini(ampdu_tx, scb, tid, FALSE);

	WLCNTINCR(ampdu_tx->cnt->rxdelba);
	AMPDUSCBCNTINCR(scb_ampdu_tx->cnt.rxdelba);

	WL_ERROR(("wl%d: %s: AMPDU OFF: tid %d initiator %d reason %d\n",
		ampdu_tx->wlc->pub->unit, __FUNCTION__, tid, initiator, reason));
}

#ifdef WLAMPDU_MAC
void wlc_ampdu_upd_pm(wlc_info_t *wlc, uint8 PM_mode)
{
	if (PM_mode == 1)
		wlc->ampdu_tx->aqm_max_release[PRIO_8021D_BE] = AMPDU_AQM_RELEASE_DEFAULT;
	else
		wlc->ampdu_tx->aqm_max_release[PRIO_8021D_BE] = AMPDU_AQM_RELEASE_MAX;
}
#endif /* WLAMPDU_MAC */


/* This function moves the window for the pkt freed during DMA TX reclaim
 * for which status is not received till flush for channel switch
 */

void wlc_ampdu_mchan_ini_adjust(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p)
{
	scb_ampdu_tx_t *scb_ampdu_tx;
	scb_ampdu_tid_ini_t *ini;
	uint16 index, seq, bar_seq = 0;

	BCM_REFERENCE(index);
	BCM_REFERENCE(seq);
	BCM_REFERENCE(bar_seq);

	if ((WLPKTTAG(p)->flags & WLF_AMPDU_MPDU) == 0)
		return;

	if (!scb || !SCB_AMPDU(scb))
		return;

	scb_ampdu_tx = SCB_AMPDU_TX_CUBBY(ampdu_tx, scb);
	ASSERT(scb_ampdu_tx);

	ini = scb_ampdu_tx->ini[PKTPRIO(p)];
	if (!ini) {
		WL_AMPDU_ERR(("wl%d: ampdu_mchan_ini_adjust: NULL ini or pon state for tid %d\n",
			ampdu_tx->wlc->pub->unit, PKTPRIO(p)));
		return;
	}
	ASSERT(ini->scb == scb);

	seq = WLPKTTAG(p)->seq;
	if (!AMPDU_AQM_ENAB(ampdu_tx->wlc->pub)) {
		index = TX_SEQ_TO_INDEX(seq);
		if (!isset(ini->ackpending, index)) {
			return;
		}
		setbit(ini->barpending, index);
	} else {
		WL_AMPDU(("%s: set barpending_seq ini->bar_ackpending:%d bar_seq:%d"
			"ini->barpending_seq:%d seq:%d ini->max_seq:%d ini->ba_wsize:%d",
			__FUNCTION__, ini->bar_ackpending, bar_seq, ini->barpending_seq, seq,
			ini->max_seq, ini->ba_wsize));

		if (MODSUB_POW2(ini->max_seq, seq, SEQNUM_MAX) < ini->ba_wsize) {
			bar_seq = seq;
			/* check if there is another bar with advence seq no */
			if (!ini->bar_ackpending || IS_SEQ_ADVANCED(bar_seq,
				ini->barpending_seq)) {
				ini->barpending_seq = bar_seq;
			}
			else  {
				WL_AMPDU(("%s:cannot set barpending_seq:"
					"ini->bar_ackpending:%d bar_seq:%d"
					"ini->barpending_seq:%d", __FUNCTION__,
					ini->bar_ackpending, bar_seq, ini->barpending_seq));
			}
		}

		/* adjust the pkts in transit during flush */
		ini->tx_in_transit--;
	}
}

#if defined(WLVSDB) && defined(PROP_TXSTATUS)
void wlc_ampdu_flush_ampdu_q(ampdu_tx_info_t *ampdu, wlc_bsscfg_t *cfg)
{
	struct scb_iter scbiter;
	struct scb *scb = NULL;
	scb_ampdu_tx_t *scb_ampdu;

	FOREACHSCB(ampdu->wlc->scbstate, &scbiter, scb) {
		if (scb->bsscfg == cfg) {
			if (!SCB_AMPDU(scb))
				return;
			scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu, scb);

			wlc_mchan_flush_queue(ampdu->wlc, &scb_ampdu->txq);
		}
	}

}

void wlc_ampdu_send_bar_cfg(ampdu_tx_info_t * ampdu, struct scb *scb)
{
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	uint32 pktprio;
	if (!scb || !SCB_AMPDU(scb))
		return;
	scb_ampdu = SCB_AMPDU_TX_CUBBY(ampdu, scb);

	for (pktprio = 0; pktprio <  AMPDU_MAX_SCB_TID; pktprio++) {
		ini = scb_ampdu->ini[pktprio];
		if (!ini) {
			continue;
		}
		/* We have no bitmap for AQM so use barpending_seq */
		if ((!AMPDU_AQM_ENAB(ampdu->wlc->pub)) || (ini->barpending_seq != SEQNUM_MAX))
			wlc_ampdu_send_bar(ampdu, ini, FALSE);
	}
}
#endif /* defined(WLVSDB) && defined(PROP_TXSTATUS) */

#ifdef BCMDBG
struct wlc_ampdu_txq_prof_entry {
	uint32 timestamp;
	struct scb *scb;

	uint32 ampdu_q_len;
	uint32 rem_window;
	uint32 tx_in_transit;
	uint32 wlc_txq_len;	/* qlen of all precedence */
	uint32 wlc_pktq_qlen;	/* qlen of a single precedence */
	uint32 prec_map;
	uint32 txpkt_pend; /* TXPKTPEND */
	uint32 dma_desc_avail;
	uint32 dma_desc_pending; /* num of descriptor that has not been processed */
	const char * func;
	uint32 line;
	uint32 tid;
};

#define AMPDU_TXQ_HIS_MAX_ENTRY (1 << 7)
#define AMPDU_TXQ_HIS_MASK (AMPDU_TXQ_HIS_MAX_ENTRY - 1)

static  struct wlc_ampdu_txq_prof_entry _txq_prof[AMPDU_TXQ_HIS_MAX_ENTRY];
static  uint32 _txq_prof_index;
static  struct scb *_txq_prof_last_scb;
static  int 	_txq_prof_cnt;

void wlc_ampdu_txq_prof_enab(void)
{
	_txq_prof_enab = 1;
}

void wlc_ampdu_txq_prof_add_entry(wlc_info_t *wlc, struct scb *scb, const char * func, uint32 line)
{
	struct wlc_ampdu_txq_prof_entry *p_entry;
	scb_ampdu_tx_t *scb_ampdu;
	scb_ampdu_tid_ini_t *ini;
	wlc_txq_info_t *qi;
	struct pktq *q;


	if (!_txq_prof_enab)
		return;

	if (!scb)
		scb = _txq_prof_last_scb;

	if (!scb)
		return;

	scb_ampdu = SCB_AMPDU_TX_CUBBY(wlc->ampdu_tx, scb);
	if (!scb_ampdu) {
		return;
	}

	if (!(ini = scb_ampdu->ini[PRIO_8021D_BE])) {
		return;
	}

	/* only gather statistics for BE */
	if (ini->tid !=  PRIO_8021D_BE) {
		return;
	}

	if (prio2fifo[ini->tid] != TX_AC_BE_FIFO)
		return;

	_txq_prof_cnt++;
	if (_txq_prof_cnt == AMPDU_TXQ_HIS_MAX_ENTRY) {
		_txq_prof_enab = 0;
		_txq_prof_cnt = 0;
	}
	_txq_prof_index = (_txq_prof_index + 1) & AMPDU_TXQ_HIS_MASK;
	p_entry = &_txq_prof[_txq_prof_index];

	p_entry->timestamp = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
	p_entry->scb = scb;
	p_entry->func = func;
	p_entry->line = line;
	p_entry->ampdu_q_len =
		wlc_ampdu_pktq_plen((&scb_ampdu->txq), (ini->tid));
	p_entry->rem_window = ini->rem_window;
	p_entry->tx_in_transit = ini->tx_in_transit;
	qi = SCB_WLCIFP(scb)->qi;
	q = &qi->q;
	p_entry->wlc_txq_len = pktq_len(q);
	p_entry->wlc_pktq_qlen = pktq_plen(q, WLC_PRIO_TO_PREC(ini->tid));
	p_entry->prec_map = wlc->tx_prec_map;

	p_entry->txpkt_pend = TXPKTPENDGET(wlc, (prio2fifo[ini->tid])),
	p_entry->dma_desc_avail = TXAVAIL(wlc, (prio2fifo[ini->tid]));
	p_entry->dma_desc_pending = dma_txpending(WLC_HW_DI(wlc, (prio2fifo[ini->tid])));

	_txq_prof_last_scb = scb;

	p_entry->tid = ini->tid;
}

void wlc_ampdu_txq_prof_print_histogram(int entries)
{
	int i, index;
	struct wlc_ampdu_txq_prof_entry *p_entry;

	printf("TXQ HISTOGRAM\n");

	index = _txq_prof_index;
	if (entries == -1)
		entries = AMPDU_TXQ_HIS_MASK;
	for (i = 0; i < entries; i++) {
		p_entry = &_txq_prof[index];
		printf("ts: %u	@ %s:%d\n", p_entry->timestamp, p_entry->func, p_entry->line);
		printf("ampdu_q  rem_win   in_trans    wlc_q prec_map pkt_pend "
			"Desc_avail Desc_pend\n");
		printf("%7d  %7d  %7d  %7d %7x %7d %7d   %7d\n",
			p_entry->ampdu_q_len,
			p_entry->rem_window,
			p_entry->tx_in_transit,
			p_entry->wlc_txq_len,
			p_entry->prec_map,
			p_entry->txpkt_pend,
			p_entry->dma_desc_avail,
			p_entry->dma_desc_pending);
		index = (index - 1) & AMPDU_TXQ_HIS_MASK;
	}
}
#endif /* BCMDBG */
