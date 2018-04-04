/*
 * A-MPDU Rx (with extended Block Ack protocol) source file
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
 * $Id: wlc_ampdu_rx.c 374748 2012-12-14 10:18:39Z $
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
#include <wlioctl.h>
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
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#endif
#include <wlc_scb_ratesel.h>
#include <wl_export.h>
#include <wlc_rm.h>

#ifdef PROP_TXSTATUS
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#endif

/* iovar table */
enum {
	IOV_AMPDU_RX,		/* enable/disable ampdu rx */
	IOV_AMPDU_RX_TID, 	/* RX BA Enable per-tid */
	IOV_AMPDU_RX_DENSITY,	/* ampdu density */
	IOV_AMPDU_RX_FACTOR,	/* ampdu rcv len */
	IOV_AMPDU_RESP_TIMEOUT_B, /* timeout (ms) for left edge of win move for brcm peer */
	IOV_AMPDU_RESP_TIMEOUT_NB, /* timeout (ms) for left edge of win move for non-brcm peer */
	IOV_AMPDU_RX_BA_WSIZE,	/* ampdu RX ba window size */
	IOV_AMPDU_HOSTREORDER, /* enable host reordering of packets */
	IOV_AMPDU_LAST
};

static const bcm_iovar_t ampdu_iovars[] = {
	{"ampdu_rx", IOV_AMPDU_RX, (IOVF_SET_DOWN), IOVT_BOOL, 0},	/* only if down */
	{"ampdu_rx_tid", IOV_AMPDU_RX_TID, (0), IOVT_BUFFER, sizeof(struct ampdu_tid_control)},
	{"ampdu_rx_density", IOV_AMPDU_RX_DENSITY, (0), IOVT_UINT8, 0},
	{"ampdu_rx_factor", IOV_AMPDU_RX_FACTOR, (IOVF_SET_DOWN), IOVT_UINT32, 0},
#ifdef  WLAMPDU_HOSTREORDER
	{"ampdu_hostreorder", IOV_AMPDU_HOSTREORDER, (IOVF_SET_DOWN), IOVT_BOOL, 0},
#endif /* WLAMPDU_HOSTREORDER */
	{NULL, 0, 0, 0, 0}
};

#ifdef WLAMPDU_HOSTREORDER
#define AMPDU_CHECK_HOST_HASPKT(resp, index) 	((resp)->host_pkt_pending[index] == TRUE)
#define AMPDU_HOST_HASPKT(resp, index) 		((resp)->host_pkt_pending[index] = TRUE)
#define AMPDU_CLEAR_HOSTPKT(resp, index) 	((resp)->host_pkt_pending[index] = FALSE)
#else
#define AMPDU_CHECK_HOST_HASPKT(resp, index) 	FALSE
#define AMPDU_HOST_HASPKT(resp, index) 		do {} while (0)
#define AMPDU_CLEAR_HOSTPKT(resp, index) 	do {} while (0)
#endif /* WLAMPDU_HOSTREORDER */

#define AMPDU_PKT_PENDING(wlc, resp, index)	\
	((resp->rxq[index]) || \
	 (AMPDU_HOST_REORDER_ENABLED(wlc) && AMPDU_CHECK_HOST_HASPKT(resp, index)))

#ifndef AMPDU_BA_MAX_WSIZE
/* max Rx ba window size (in pdu) for array allocations in structures. */
#define AMPDU_BA_MAX_WSIZE	64
#endif

#ifndef AMPDU_RX_BA_MAX_WSIZE
#define AMPDU_RX_BA_MAX_WSIZE	64		/* max Rx ba window size (in pdu) */
#endif /* AMPDU_RX_BA_MAX_WSIZE */
#ifndef AMPDU_RX_BA_DEF_WSIZE
#define AMPDU_RX_BA_DEF_WSIZE	64		/* default Rx ba window size (in pdu) */
#endif /* AMPDU_RX_BA_DEF_WSIZE */

#define AMPDU_RESP_TIMEOUT_B		1000	/* # of ms wo resp prog with brcm peer */
#define AMPDU_RESP_TIMEOUT_NB		200	/* # of ms wo resp prog with non-brcm peer */
#define AMPDU_RESP_TIMEOUT		100	/* timeout interval in msec for resp prog */

/* internal BA states */
#define	AMPDU_TID_STATE_BA_OFF		0x00	/* block ack OFF for tid */
#define	AMPDU_TID_STATE_BA_ON		0x01	/* block ack ON for tid */
#define	AMPDU_TID_STATE_BA_PENDING_ON	0x02	/* block ack pending ON for tid */
#define	AMPDU_TID_STATE_BA_PENDING_OFF	0x03	/* block ack pending OFF for tid */

/* useful macros */
#define NEXT_SEQ(seq) MODINC_POW2((seq), SEQNUM_MAX)
#define NEXT_RX_INDEX(index) MODINC_POW2((index), (wlc->ampdu_rx->ba_max_rx_wsize))
#define RX_SEQ_TO_INDEX(ampdu_rx, seq) ((seq) & (((ampdu_rx)->ba_max_rx_wsize) - 1))

/* ampdu related stats */
typedef struct wlc_ampdu_cnt {
#ifdef WLCNT
	/* responder side counters */
	uint32 rxampdu;		/* ampdus recd */
	uint32 rxmpdu;		/* mpdus recd in a ampdu */
	uint32 rxht;		/* mpdus recd at ht rate and not in a ampdu */
	uint32 rxlegacy;	/* mpdus recd at legacy rate */
	uint32 rxampdu_sgi;	/* ampdus recd with sgi */
	uint32 rxampdu_stbc; /* ampdus recd with stbc */
	uint32 rxnobapol;	/* mpdus recd without a ba policy */
	uint32 rxholes;		/* missed seq numbers on rx side */
	uint32 rxqed;		/* pdus buffered before sending up */
	uint32 rxdup;		/* duplicate pdus */
	uint32 rxstuck;		/* watchdog bailout for stuck state */
	uint32 rxoow;		/* out of window pdus */
	uint32 rxoos;		/* out of seq pdus */
	uint32 rxaddbareq;	/* addba req recd */
	uint32 txaddbaresp;	/* addba resp sent */
	uint32 rxbar;		/* bar recd */
	uint32 txba;		/* ba sent */

	/* general: both initiator and responder */
	uint32 rxunexp;		/* unexpected packets */
	uint32 txdelba;		/* delba sent */
	uint32 rxdelba;		/* delba recd */
#endif /* WLCNT */
} wlc_ampdu_rx_cnt_t;

typedef struct {
	uint32 rxmcs[AMPDU_MAX_MCS+1];		/* mcs of rx pkts */
	uint32 rxmcssgi[AMPDU_MAX_MCS+1];		/* mcs of rx pkts */
	uint32 rxmcsstbc[AMPDU_MAX_MCS+1];		/* mcs of rx pkts */
#ifdef WL11AC
	uint32 rxvht[AMPDU_MAX_VHT];		/* vht of rx pkts */
	uint32 rxvhtsgi[AMPDU_MAX_VHT];		/* vht of rx pkts */
	uint32 rxvhtstbc[AMPDU_MAX_VHT];		/* vht of rx pkts */
#endif /* WL11AC */

} ampdu_rx_dbg_t;

/* AMPDU module specific state */
struct ampdu_rx_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	int scb_handle;		/* scb cubby handle to retrieve data from scb */
	uint8 ba_policy;	/* ba policy; immediate vs delayed */
	uint8 ba_rx_wsize;      /* Rx ba window size (in pdu) */
	uint8 delba_timeout;	/* timeout after which to send delba (sec) */
	uint8 rx_factor;	/* maximum rx ampdu factor (0-3) ==> 2^(13+x) bytes */
	uint8 mpdu_density;	/* min mpdu spacing (0-7) ==> 2^(x-1)/8 usec */
	uint16 resp_timeout_b;	/* timeout (ms) for left edge of win move for brcm peer */
	uint16 resp_timeout_nb;	/* timeout (ms) for left edge of win move for non-brcm peer */
	uint16 resp_cnt;	/* count of resp reorder queues */
	struct wl_timer *resp_timer;	/* timer for resp reorder q flush */
#ifdef WLCNT
	wlc_ampdu_rx_cnt_t *cnt;	/* counters/stats */
#endif
	ampdu_rx_dbg_t *amdbg;

	uint8 rxba_enable[AMPDU_MAX_SCB_TID]; /* per-tid responder enable/disable of ampdu */
	bool rxaggr;	  /* rx state of aggregation: ON/OFF */
	int8 rxaggr_override;	/* config override to disable ampdu rx */
	bool	resp_timer_running; /* ampdu resp timer state */
	uint8	ba_max_rx_wsize;	/* Rx ba window size (in pdu) */
	uint16  flow_id;
};

static void wlc_ampdu_rx_cleanup(wlc_info_t *wlc, wlc_bsscfg_t *cfg, ampdu_rx_info_t *ampdu_rx);

/* structure to store per-tid state for the ampdu responder */
struct scb_ampdu_tid_resp {
	uint8 ba_state;		/* ampdu ba state */
	uint8 ba_wsize;		/* negotiated ba window size (in pdu) */
	uint8 queued;		/* number of queued packets */
	uint8 dead_cnt;		/* number of sec without any progress */
	bool alive;		/* true if making forward progress */
	uint16 exp_seq;		/* next expected seq */
	void *rxq[AMPDU_BA_MAX_WSIZE];		/* rx reorder queue */
	void *wrxh[AMPDU_BA_MAX_WSIZE];	/* saved rxh queue */
#ifdef WLAMPDU_HOSTREORDER
	bool host_pkt_pending[AMPDU_BA_MAX_WSIZE];  /* rx reorder pending in host rxh queue */
	uint16 flow_id;
	void *tohost_ctrlpkt;
#endif /* WLAMPDU_HOSTREORDER */
};

/* structure to store per-tid state for the ampdu resp when off. statically alloced */
typedef struct scb_ampdu_tid_resp_off {
	bool ampdu_recd;	/* TRUE is ampdu was recd in the 1 sec window */
	uint8 ampdu_cnt;	/* number of secs during which ampdus are recd */
} scb_ampdu_tid_resp_off_t;

#ifdef BCMDBG
typedef struct scb_ampdu_cnt_rx {
	uint32 rxunexp;
	uint32 rxholes;
	uint32 rxstuck;
	uint32 txaddbaresp;
	uint32 sduretry;
	uint32 sdurejected;
	uint32 noba;
	uint32 rxampdu;
	uint32 rxmpdu;
	uint32 rxlegacy;
	uint32 rxdup;
	uint32 rxoow;
	uint32 rxdelba;
	uint32 rxbar;
} scb_ampdu_cnt_rx_t;
#endif	/* BCMDBG */

/* scb cubby structure. ini and resp are dynamically allocated if needed */
struct scb_ampdu_rx {
	struct scb *scb;		/* back pointer for easy reference */
	scb_ampdu_tid_resp_t *resp[AMPDU_MAX_SCB_TID];	/* responder info */
	scb_ampdu_tid_resp_off_t resp_off[AMPDU_MAX_SCB_TID];	/* info when resp is off */
	ampdu_rx_info_t *ampdu_rx; /* back ref to main ampdu_rx */
#ifdef BCMDBG
	scb_ampdu_cnt_rx_t cnt;
#endif	/* BCMDBG */
};


#ifdef WLAMPDU_HOSTREORDER

#undef NO_NEWHOLE
#undef NEWHOLE
#undef NO_DEL_FLOW
#undef DEL_FLOW
#undef NO_FLUSH_ALL
#undef FLUSH_ALL
#undef AMPDU_INVALID_INDEX

#define NO_NEWHOLE		FALSE
#define NEWHOLE			TRUE
#define NO_DEL_FLOW		FALSE
#define DEL_FLOW		TRUE
#define NO_FLUSH_ALL		FALSE
#define FLUSH_ALL		TRUE
#define AMPDU_INVALID_INDEX	0xFFFF

static void wlc_ampdu_setpkt_hostreorder_info(wlc_info_t *wlc, scb_ampdu_tid_resp_t *resp,
	void *p, uint16 cur_idx, bool new_hole, bool del_flow, bool flush_all);
static int wlc_ampdu_alloc_flow_id(ampdu_rx_info_t *ampdu);
static int wlc_ampdu_free_flow_id(ampdu_rx_info_t *ampdu, scb_ampdu_tid_resp_t *resp,
	struct scb *scb);
#else
#define wlc_ampdu_setpkt_hostreorder_info(a, b, c, d, e, f, g) do {} while (0)
#define wlc_ampdu_alloc_flow_id(a) 		0
#define wlc_ampdu_free_flow_id(a, b, c) 	0
#endif /* WLAMPDU_HOSTREORDER */

#ifdef BCMDBG
#define AMPDUSCBCNTADD(cnt, upd) ((cnt) += (upd))
#define AMPDUSCBCNTINCR(cnt) ((cnt)++)
#else
#define AMPDUSCBCNTADD(a, b) do { } while (0)
#define AMPDUSCBCNTINCR(a)  do { } while (0)
#endif

struct ampdu_rx_cubby {
	scb_ampdu_rx_t *scb_rx_cubby;
};

#define SCB_AMPDU_INFO(ampdu_rx, scb) (SCB_CUBBY((scb), (ampdu_rx)->scb_handle))
#define SCB_AMPDU_RX_CUBBY(ampdu_rx, scb) \
	(((struct ampdu_rx_cubby *)SCB_AMPDU_INFO(ampdu_rx, scb))->scb_rx_cubby)

/* local prototypes */
static int scb_ampdu_rx_init(void *context, struct scb *scb);
static void scb_ampdu_rx_deinit(void *context, struct scb *scb);
static int wlc_ampdu_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static int wlc_ampdu_watchdog(void *hdl);
static int wlc_ampdu_down(void *hdl);

static INLINE void wlc_ampdu_release_all_ordered(wlc_info_t *wlc, scb_ampdu_rx_t *scb_ampdu,
	uint8 tid);

static void ampdu_create_f(wlc_info_t *wlc, struct scb *scb, struct wlc_frminfo *f,
	void *p, wlc_d11rxhdr_t *wrxh);

static void wlc_ampdu_resp_timeout(void *arg);

static INLINE uint16
pkt_h_seqnum(wlc_info_t *wlc, void *p)
{
	struct dot11_header *h;
	h = (struct dot11_header *)PKTDATA(wlc->osh, p);
	return (ltoh16(h->seq) >> SEQNUM_SHIFT);
}

INLINE void
wlc_ampdu_release_ordered(wlc_info_t *wlc, scb_ampdu_rx_t *scb_ampdu, uint8 tid)
{
	void *p = NULL;
	struct wlc_frminfo f;
	uint16 indx;
	uint bandunit;
	struct ether_addr ea;
	struct scb *newscb;
	struct scb *scb = scb_ampdu->scb;
	scb_ampdu_tid_resp_t *resp = scb_ampdu->resp[tid];
	wlc_bsscfg_t *bsscfg;

	if (resp == NULL)
		return;

	for (indx = RX_SEQ_TO_INDEX(wlc->ampdu_rx, resp->exp_seq);
		AMPDU_PKT_PENDING(wlc, resp, indx); indx = NEXT_RX_INDEX(indx))
	{
		resp->queued--;

		if (!AMPDU_HOST_REORDER_ENABLED(wlc)) {
			p = resp->rxq[indx];
			resp->rxq[indx] = NULL;
#if defined(__ARM_ARCH_7A__) && defined(WL_PL310_WAR)
			if (resp->exp_seq != pkt_h_seqnum(wlc, p)) {
				WL_ERROR(("wl%d: %s: sequence number mismatched\n",
					wlc->pub->unit, __FUNCTION__));
				PKTFREE(wlc->osh, p, FALSE);
				resp->alive = TRUE;
				resp->exp_seq = NEXT_SEQ(resp->exp_seq);
				wlc_ampdu_rx_send_delba(wlc->ampdu_rx, scb, tid, FALSE,
					DOT11_RC_UNSPECIFIED);
				return;
			}
#else
			ASSERT(resp->exp_seq == pkt_h_seqnum(wlc, p));
#endif /* defined(__ARM_ARCH_7A__) && defined(WL_PL310_WAR) */
		}
		resp->alive = TRUE;
		resp->exp_seq = NEXT_SEQ(resp->exp_seq);

		if (AMPDU_HOST_REORDER_ENABLED(wlc)) {
			AMPDU_CLEAR_HOSTPKT(resp, indx);
			continue;
		}

		WL_AMPDU_RX(("wl%d: wlc_ampdu_release_ordered: releasing seq 0x%x\n",
			wlc->pub->unit, resp->exp_seq));

		/* create the fields of frminfo f */
		ampdu_create_f(wlc, scb, &f, p, resp->wrxh[indx]);

		bsscfg = scb->bsscfg;
		bandunit = scb->bandunit;
		bcopy(&scb->ea, &ea, ETHER_ADDR_LEN);

		wlc_recvdata_ordered(wlc, scb, &f);

		/* validate that the scb is still around and some path in
		 * wlc_recvdata_ordered() did not free it
		 */
		newscb = wlc_scbfindband(wlc, bsscfg, &ea, bandunit);
		if ((newscb == NULL) || (newscb != scb)) {
			WL_ERROR(("wl%d: %s: scb freed; bail out\n",
				wlc->pub->unit, __FUNCTION__));
			return;
		}

		/* Make sure responder was not freed when we gave up the lock in sendup */
		if ((resp = scb_ampdu->resp[tid]) == NULL)
			return;
	}
}

/* release next n pending ordered packets starting from index going over holes */
INLINE void
wlc_ampdu_release_n_ordered(wlc_info_t *wlc, scb_ampdu_rx_t *scb_ampdu, uint8 tid, uint16 offset)
{
	void *p;
	struct wlc_frminfo f;
	uint16 indx;
	uint bandunit;
	struct ether_addr ea;
	struct scb *newscb;
	struct scb *scb = scb_ampdu->scb;
	scb_ampdu_tid_resp_t *resp = scb_ampdu->resp[tid];
	wlc_bsscfg_t *bsscfg;

	ASSERT(resp);
	if (resp == NULL)
		return;

	for (; offset; offset--) {
	        indx = RX_SEQ_TO_INDEX(wlc->ampdu_rx, resp->exp_seq);
		if (AMPDU_PKT_PENDING(wlc, resp, indx)) {

			resp->queued--;

			if (AMPDU_HOST_REORDER_ENABLED(wlc)) {
				AMPDU_CLEAR_HOSTPKT(resp, indx);
				continue;
			}
			else {
				p = resp->rxq[indx];
				resp->rxq[indx] = NULL;
#if defined(__ARM_ARCH_7A__) && defined(WL_PL310_WAR)
				if (resp->exp_seq != pkt_h_seqnum(wlc, p)) {
					WL_ERROR(("wl%d: %s: sequence number mismatched\n",
						wlc->pub->unit, __FUNCTION__));
					PKTFREE(wlc->osh, p, FALSE);
					resp->alive = TRUE;
					resp->exp_seq = NEXT_SEQ(resp->exp_seq);
					wlc_ampdu_rx_send_delba(wlc->ampdu_rx, scb, tid, FALSE,
						DOT11_RC_UNSPECIFIED);
					return;
				}
#else
				ASSERT(resp->exp_seq == pkt_h_seqnum(wlc, p));
#endif /* defined(__ARM_ARCH_7A__) && defined(WL_PL310_WAR) */

				bsscfg = scb->bsscfg;
				bandunit = scb->bandunit;
				bcopy(&scb->ea, &ea, ETHER_ADDR_LEN);
				WL_AMPDU_RX(("wl%d: wlc_ampdu_release_n_ordered: releas seq 0x%x\n",
					wlc->pub->unit, resp->exp_seq));

				/* set the fields of frminfo f */
				ampdu_create_f(wlc, scb, &f, p, resp->wrxh[indx]);

				wlc_recvdata_ordered(wlc, scb, &f);

				/* validate that the scb is still around and some path in
				 * wlc_recvdata_ordered() did not free it
				*/
				newscb = wlc_scbfindband(wlc, bsscfg, &ea, bandunit);
				if ((newscb == NULL) || (newscb != scb)) {
					WL_ERROR(("wl%d: %s: scb freed; bail out\n",
						wlc->pub->unit, __FUNCTION__));
					return;
				}

				/* Make sure responder was not freed when we gave up
				 * the lock in sendup
				 */
				if ((resp = scb_ampdu->resp[tid]) == NULL)
					return;
			}

		} else {
			WLCNTINCR(wlc->ampdu_rx->cnt->rxholes);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.rxholes);
		}
		resp->alive = TRUE;
		resp->exp_seq = NEXT_SEQ(resp->exp_seq);
	}
}

/* release all pending ordered packets starting from index going over holes */
static INLINE void
wlc_ampdu_release_all_ordered(wlc_info_t *wlc, scb_ampdu_rx_t *scb_ampdu, uint8 tid)
{
	uint16 seq, max_seq, offset, i;
	scb_ampdu_tid_resp_t *resp = scb_ampdu->resp[tid];

	ASSERT(resp);
	if (resp == NULL)
		return;

	for (i = 0, seq = resp->exp_seq, max_seq = resp->exp_seq;
	     i < wlc->ampdu_rx->ba_max_rx_wsize;
	     i++, seq = NEXT_SEQ(seq)) {
		if (resp->rxq[RX_SEQ_TO_INDEX(wlc->ampdu_rx, seq)])
			max_seq = seq;
	}

	offset = MODSUB_POW2(max_seq, resp->exp_seq, SEQNUM_MAX) + 1;
	wlc_ampdu_release_n_ordered(wlc, scb_ampdu, tid, offset);
}

ampdu_rx_info_t *
BCMATTACHFN(wlc_ampdu_rx_attach)(wlc_info_t *wlc)
{
	ampdu_rx_info_t *ampdu_rx;
	int i;

	/* some code depends on packed structures */
	STATIC_ASSERT(sizeof(struct dot11_bar) == DOT11_BAR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_ba) == DOT11_BA_LEN + DOT11_BA_BITMAP_LEN);
	STATIC_ASSERT(sizeof(struct dot11_ctl_header) == DOT11_CTL_HDR_LEN);
	STATIC_ASSERT(sizeof(struct dot11_addba_req) == DOT11_ADDBA_REQ_LEN);
	STATIC_ASSERT(sizeof(struct dot11_addba_resp) == DOT11_ADDBA_RESP_LEN);
	STATIC_ASSERT(sizeof(struct dot11_delba) == DOT11_DELBA_LEN);
	STATIC_ASSERT(DOT11_MAXNUMFRAGS == NBITS(uint16));
	STATIC_ASSERT(ISPOWEROF2(AMPDU_RX_BA_MAX_WSIZE));

	ASSERT(wlc->pub->tunables->ampdunummpdu2streams <= AMPDU_MAX_MPDU);
	ASSERT(wlc->pub->tunables->ampdunummpdu2streams > 0);
	ASSERT(wlc->pub->tunables->ampdunummpdu3streams <= AMPDU_MAX_MPDU);
	ASSERT(wlc->pub->tunables->ampdunummpdu3streams > 0);

	if (!(ampdu_rx = (ampdu_rx_info_t *)MALLOC(wlc->osh, sizeof(ampdu_rx_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	bzero((char *)ampdu_rx, sizeof(ampdu_rx_info_t));
	ampdu_rx->wlc = wlc;

	ampdu_rx->ba_max_rx_wsize = AMPDU_RX_BA_MAX_WSIZE;

#ifdef WLCNT
	if (!(ampdu_rx->cnt = (wlc_ampdu_rx_cnt_t *)MALLOC(wlc->osh, sizeof(wlc_ampdu_rx_cnt_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero((char *)ampdu_rx->cnt, sizeof(wlc_ampdu_rx_cnt_t));
#endif /* WLCNT */

	/* enable rxba_enable on TIDs */
	for (i = 0; i < AMPDU_MAX_SCB_TID; i++)
		ampdu_rx->rxba_enable[i] = TRUE;

	ampdu_rx->ba_policy = DOT11_ADDBA_POLICY_IMMEDIATE;
	ampdu_rx->ba_rx_wsize = AMPDU_RX_BA_DEF_WSIZE;

	if (ampdu_rx->ba_rx_wsize > ampdu_rx->ba_max_rx_wsize) {
		WL_ERROR(("wl%d: The Default AMPDU_RX_BA_WSIZE is greater than MAX value\n",
			wlc->pub->unit));
		ampdu_rx->ba_rx_wsize = ampdu_rx->ba_max_rx_wsize;
	}
	
#ifdef DSLCPE_C610384
	if (D11REV_LE(wlc->pub->corerev, 41))
		ampdu_rx->mpdu_density = AMPDU_DENSITY_8_US; 
	else
#endif	

	if (D11REV_IS(wlc->pub->corerev, 17) || D11REV_IS(wlc->pub->corerev, 28))
		ampdu_rx->mpdu_density = AMPDU_DENSITY_8_US;
	else
		ampdu_rx->mpdu_density = AMPDU_DEF_MPDU_DENSITY;

	ampdu_rx->resp_timeout_b = AMPDU_RESP_TIMEOUT_B;
	ampdu_rx->resp_timeout_nb = AMPDU_RESP_TIMEOUT_NB;
	ampdu_rx->rxaggr = ON;

	ampdu_rx->rxaggr_override = AUTO;
	/* bump max ampdu rcv size to 64k for all 11n devices except 4321A0 and 4321A1 */
	if (WLCISNPHY(wlc->band) && NREV_LT(wlc->band->phyrev, 2))
		ampdu_rx->rx_factor = AMPDU_RX_FACTOR_32K;
	else
		ampdu_rx->rx_factor = AMPDU_RX_FACTOR_64K;
#ifdef WLC_HIGH_ONLY
	/* Restrict to smaller rcv size for BMAC dongle */
	ampdu_rx->rx_factor = AMPDU_RX_FACTOR_32K;
#endif

	ampdu_rx->delba_timeout = 0; /* AMPDUXXX: not yet supported */

	wlc_ampdu_update_ie_param(ampdu_rx);

	/* reserve cubby in the scb container */
	ampdu_rx->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(struct ampdu_rx_cubby),
		scb_ampdu_rx_init, scb_ampdu_rx_deinit, NULL, (void *)ampdu_rx);

	if (ampdu_rx->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve() failed\n", wlc->pub->unit));
		goto fail;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, ampdu_iovars, "ampdu_rx", ampdu_rx, wlc_ampdu_doiovar,
	                        wlc_ampdu_watchdog, NULL, wlc_ampdu_down)) {
		WL_ERROR(("wl%d: ampdu_rx wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	if (!(ampdu_rx->resp_timer =
		wl_init_timer(wlc->wl, wlc_ampdu_resp_timeout, ampdu_rx, "resp"))) {
		WL_ERROR(("wl%d: ampdu_rx wl_init_timer() failed\n", wlc->pub->unit));
		goto fail;
	}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
	if (!(ampdu_rx->amdbg = (ampdu_rx_dbg_t *)MALLOC(wlc->osh, sizeof(ampdu_rx_dbg_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero((char *)ampdu_rx->amdbg, sizeof(ampdu_rx_dbg_t));
#endif /*  defined(BCMDBG) || defined(WLTEST) */

	/* try to set ampdu to the default value */
	wlc_ampdu_rx_set(ampdu_rx, wlc->pub->_ampdu_rx);

	/* ampdu_resp_timer state is inited to not running */
	ampdu_rx->resp_timer_running = FALSE;

	return ampdu_rx;

fail:
#ifdef WLCNT
	if (ampdu_rx->cnt)
		MFREE(wlc->osh, ampdu_rx->cnt, sizeof(wlc_ampdu_rx_cnt_t));
#endif /* WLCNT */
	MFREE(wlc->osh, ampdu_rx, sizeof(ampdu_rx_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_ampdu_rx_detach)(ampdu_rx_info_t *ampdu_rx)
{
	if (!ampdu_rx)
		return;

	ASSERT(ampdu_rx->resp_cnt == 0);
	ASSERT(ampdu_rx->resp_timer_running == FALSE);
	if (ampdu_rx->resp_timer) {
		if (ampdu_rx->resp_timer_running)
			wl_del_timer(ampdu_rx->wlc->wl, ampdu_rx->resp_timer);
		wl_free_timer(ampdu_rx->wlc->wl, ampdu_rx->resp_timer);
		ampdu_rx->resp_timer = NULL;
	}


#ifdef WLCNT
	if (ampdu_rx->cnt)
		MFREE(ampdu_rx->wlc->osh, ampdu_rx->cnt, sizeof(wlc_ampdu_rx_cnt_t));
#endif
#if defined(BCMDBG) || defined(WLTEST)
	if (ampdu_rx->amdbg) {
		MFREE(ampdu_rx->wlc->osh, ampdu_rx->amdbg, sizeof(ampdu_rx_dbg_t));
		ampdu_rx->amdbg = NULL;
	}
#endif

	wlc_module_unregister(ampdu_rx->wlc->pub, "ampdu_rx", ampdu_rx);
	MFREE(ampdu_rx->wlc->osh, ampdu_rx, sizeof(ampdu_rx_info_t));
}

/* scb cubby init fn */
static int
scb_ampdu_rx_init(void *context, struct scb *scb)
{
	ampdu_rx_info_t *ampdu_rx = (ampdu_rx_info_t *)context;
	struct ampdu_rx_cubby *cubby_info = (struct ampdu_rx_cubby *)SCB_AMPDU_INFO(ampdu_rx, scb);
	scb_ampdu_rx_t *scb_ampdu;

	if (scb && !SCB_INTERNAL(scb)) {
		scb_ampdu = MALLOC(ampdu_rx->wlc->osh, sizeof(scb_ampdu_rx_t));
		if (!scb_ampdu)
			return BCME_NOMEM;
		bzero(scb_ampdu, sizeof(scb_ampdu_rx_t));
		cubby_info->scb_rx_cubby = scb_ampdu;
		scb_ampdu->scb = scb;
		scb_ampdu->ampdu_rx = ampdu_rx;
	}
	return 0;
}

/* scb cubby deinit fn */
static void
scb_ampdu_rx_deinit(void *context, struct scb *scb)
{
	ampdu_rx_info_t *ampdu_rx = (ampdu_rx_info_t *)context;
	struct ampdu_rx_cubby *cubby_info = (struct ampdu_rx_cubby *)SCB_AMPDU_INFO(ampdu_rx, scb);
	scb_ampdu_rx_t *scb_ampdu = NULL;

	WL_AMPDU_UPDN(("scb_ampdu_deinit: enter\n"));

	ASSERT(cubby_info);

	if (cubby_info)
		scb_ampdu = cubby_info->scb_rx_cubby;
	if (!scb_ampdu)
		return;

	scb_ampdu_rx_flush(ampdu_rx, scb);

	MFREE(ampdu_rx->wlc->osh, scb_ampdu, sizeof(scb_ampdu_rx_t));
	cubby_info->scb_rx_cubby = NULL;
}

void
scb_ampdu_rx_flush(ampdu_rx_info_t *ampdu_rx, struct scb *scb)
{
	uint8 tid;

	WL_AMPDU_UPDN(("scb_ampdu_rx_flush: enter\n"));

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		ampdu_cleanup_tid_resp(ampdu_rx, scb, tid);
	}
}

/* frees all the buffers and cleanup everything on down */
static int
wlc_ampdu_down(void *hdl)
{
	ampdu_rx_info_t *ampdu_rx = (ampdu_rx_info_t *)hdl;
	struct scb *scb;
	struct scb_iter scbiter;

	WL_AMPDU_UPDN(("wlc_ampdu_down: enter\n"));

	FOREACHSCB(ampdu_rx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb))
			scb_ampdu_rx_flush(ampdu_rx, scb);
	}

	return 0;
}

static void
wlc_ampdu_resp_timeout(void *arg)
{
	ampdu_rx_info_t *ampdu_rx = (ampdu_rx_info_t *)arg;
	wlc_info_t *wlc = ampdu_rx->wlc;
	scb_ampdu_rx_t *scb_ampdu;
	scb_ampdu_tid_resp_t *resp;
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 tid;
	uint32 lim;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!SCB_AMPDU(scb))
			continue;

		scb_ampdu = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
			if ((resp = scb_ampdu->resp[tid]) == NULL)
				continue;

			/* check on resp forward progress */

			if (resp->alive) {
				resp->alive = FALSE;
				resp->dead_cnt = 0;
			} else {
				if (!resp->queued)
					continue;

				resp->dead_cnt++;
				lim = (scb->flags & SCB_BRCM) ?
					(ampdu_rx->resp_timeout_b / AMPDU_RESP_TIMEOUT) :
					(ampdu_rx->resp_timeout_nb / AMPDU_RESP_TIMEOUT);

				if (resp->dead_cnt >= lim) {
					void *p = NULL;
					if (AMPDU_HOST_REORDER_ENABLED(wlc))
						p = PKTGET(wlc->osh, TXOFF, FALSE);
					if (!AMPDU_HOST_REORDER_ENABLED(wlc) || p) {
						WL_ERROR(("wl%d: ampdu_resp_timeout: cleaning up "
							"resp tid %d waiting for seq 0x%x for %d "
							"ms\n",
							wlc->pub->unit, tid, resp->exp_seq,
							lim*AMPDU_RESP_TIMEOUT));
					WLCNTINCR(ampdu_rx->cnt->rxstuck);
					AMPDUSCBCNTINCR(scb_ampdu->cnt.rxstuck);
					wlc_ampdu_release_all_ordered(wlc, scb_ampdu, tid);
						if (AMPDU_HOST_REORDER_ENABLED(wlc) && (p != NULL))
						{
							wlc_ampdu_setpkt_hostreorder_info(wlc,
								resp, p, AMPDU_INVALID_INDEX,
								NO_NEWHOLE, NO_DEL_FLOW, FLUSH_ALL);
							wl_sendup(wlc->wl,
								SCB_INTERFACE(scb), p, 1);
						}
					}
				}
			}
		}
	}
}

static void
wlc_ampdu_rx_cleanup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, ampdu_rx_info_t *ampdu_rx)
{
	uint8 tid;
	scb_ampdu_rx_t *scb_ampdu = NULL;
	struct scb *scb;
	struct scb_iter scbiter;

	scb_ampdu_tid_resp_t *resp;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (!SCB_AMPDU(scb))
			continue;

		if (!SCB_ASSOCIATED(scb))
			continue;

		scb_ampdu = SCB_AMPDU_RX_CUBBY(wlc->ampdu_rx, scb);
		ASSERT(scb_ampdu);
		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
			resp = scb_ampdu->resp[tid];

			if (resp != NULL) {
				if ((resp->ba_state == AMPDU_TID_STATE_BA_ON) ||
					(resp->ba_state == AMPDU_TID_STATE_BA_PENDING_ON))
					wlc_ampdu_rx_send_delba(ampdu_rx, scb, tid, FALSE,
						DOT11_RC_TIMEOUT);

				ampdu_cleanup_tid_resp(ampdu_rx, scb, tid);
			}
		}
	}
}

/* resends ADDBA-Req if the ADDBA-Resp has not come back */
static int
wlc_ampdu_watchdog(void *hdl)
{
	ampdu_rx_info_t *ampdu_rx = (ampdu_rx_info_t *)hdl;
	wlc_info_t *wlc = ampdu_rx->wlc;
	scb_ampdu_rx_t *scb_ampdu;
	scb_ampdu_tid_resp_t *resp;
	scb_ampdu_tid_resp_off_t *resp_off;
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 tid;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!SCB_AMPDU(scb))
			continue;
		scb_ampdu = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
		ASSERT(scb_ampdu);
		for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {

			resp = scb_ampdu->resp[tid];
			resp_off = &scb_ampdu->resp_off[tid];

			if (resp) {
				resp_off->ampdu_cnt = 0;
				resp_off->ampdu_recd = FALSE;
			}
			if (resp_off->ampdu_recd) {
				resp_off->ampdu_recd = FALSE;
				resp_off->ampdu_cnt++;
				if (resp_off->ampdu_cnt >= AMPDU_RESP_NO_BAPOLICY_TIMEOUT) {
					resp_off->ampdu_cnt = 0;
					WL_ERROR(("wl%d: %s: ampdus recd for"
						" tid %d with no BA policy in effect\n",
						ampdu_rx->wlc->pub->unit, __FUNCTION__, tid));
					wlc_ampdu_rx_send_delba(ampdu_rx, scb, tid,
						FALSE, DOT11_RC_SETUP_NEEDED);
				}
			}
		}
	}

	return 0;
}

/* handle AMPDU related iovars */
static int
wlc_ampdu_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	ampdu_rx_info_t *ampdu_rx = (ampdu_rx_info_t *)hdl;
	int32 int_val = 0;
	int32 *ret_int_ptr = (int32 *) a;
	int err = 0;
	wlc_info_t *wlc;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

#ifdef  WLAMPDU_HOSTREORDER
		bool bool_val;
		bool_val = (int_val != 0) ? TRUE : FALSE;
#endif

	wlc = ampdu_rx->wlc;
	ASSERT(ampdu_rx == wlc->ampdu_rx);

	switch (actionid) {
	case IOV_GVAL(IOV_AMPDU_RX):
		*ret_int_ptr = (int32)wlc->pub->_ampdu_rx;
		break;

	case IOV_SVAL(IOV_AMPDU_RX):
		return wlc_ampdu_rx_set(ampdu_rx, (bool)int_val);

	case IOV_GVAL(IOV_AMPDU_RX_TID): {
		struct ampdu_tid_control *ampdu_tid = (struct ampdu_tid_control *)p;

		if (ampdu_tid->tid >= AMPDU_MAX_SCB_TID) {
			err = BCME_BADARG;
			break;
		}
		ampdu_tid->enable = ampdu_rx->rxba_enable[ampdu_tid->tid];
		bcopy(ampdu_tid, a, sizeof(*ampdu_tid));
		break;
		}

	case IOV_SVAL(IOV_AMPDU_RX_TID): {
		struct ampdu_tid_control *ampdu_tid = (struct ampdu_tid_control *)a;

		if (ampdu_tid->tid >= AMPDU_MAX_SCB_TID) {
			err = BCME_BADARG;
			break;
		}
		ampdu_rx->rxba_enable[ampdu_tid->tid] = ampdu_tid->enable ? TRUE : FALSE;
		break;
		}

	case IOV_GVAL(IOV_AMPDU_RX_DENSITY):
		*ret_int_ptr = (int32)ampdu_rx->mpdu_density;
		break;

	case IOV_SVAL(IOV_AMPDU_RX_DENSITY):
		if (int_val > AMPDU_MAX_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}

		if (int_val < AMPDU_DEF_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}
		ampdu_rx->mpdu_density = (uint8)int_val;
		wlc_ampdu_update_ie_param(wlc->ampdu_rx);
		break;

	case IOV_GVAL(IOV_AMPDU_RX_FACTOR):
		*ret_int_ptr = (int32)ampdu_rx->rx_factor;
		break;

	case IOV_SVAL(IOV_AMPDU_RX_FACTOR):
		/* limit to the max aggregation size possible based on chip
		 * limitations
		 */
		if ((int_val > AMPDU_RX_FACTOR_64K) ||
		    (int_val > AMPDU_RX_FACTOR_32K &&
		     D11REV_LE(wlc->pub->corerev, 11))) {
			err = BCME_RANGE;
			break;
		}
		ampdu_rx->rx_factor = (uint8)int_val;
		wlc_ampdu_update_ie_param(ampdu_rx);
		break;


#ifdef  WLAMPDU_HOSTREORDER
	case IOV_GVAL(IOV_AMPDU_HOSTREORDER):
		*ret_int_ptr = (int32)wlc->pub->ampdu_hostreorder;
		break;

	case IOV_SVAL(IOV_AMPDU_HOSTREORDER):
		wlc->pub->ampdu_hostreorder = bool_val;
		break;
#endif /* WLAMPDU_HOSTREORDER */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

void
BCMATTACHFN(wlc_ampdu_agg_state_rxaggr_override)(ampdu_rx_info_t *ampdu_rx, int8 rxaggr)
{
	ampdu_rx->rxaggr_override = rxaggr;
	if (rxaggr != AUTO)
		ampdu_rx->rxaggr = rxaggr;
}

void
wlc_ampdu_agg_state_update_rx(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool rxaggr)
{
	ampdu_rx_info_t *ampdu_rx = wlc->ampdu_rx;

	if (ampdu_rx->rxaggr_override != AUTO)
		return;

	if (ampdu_rx->rxaggr != rxaggr) {
		if (rxaggr == OFF) {
			wlc_ampdu_rx_cleanup(wlc, bsscfg, ampdu_rx);
		}
		ampdu_rx->rxaggr = rxaggr;
	}
}

/* AMPDUXXX: this has to be kept upto date as fields get added to f */
static void BCMFASTPATH
ampdu_create_f(wlc_info_t *wlc, struct scb *scb, struct wlc_frminfo *f, void *p,
	wlc_d11rxhdr_t *wrxh)
{
	uint16 offset = DOT11_A3_HDR_LEN;


	bzero((void *)f, sizeof(struct wlc_frminfo));
	f->p = p;
	f->h = (struct dot11_header *) PKTDATA(wlc->osh, f->p);
	f->fc = ltoh16(f->h->fc);
	f->type = FC_TYPE(f->fc);
	f->subtype = (f->fc & FC_SUBTYPE_MASK) >> FC_SUBTYPE_SHIFT;
	f->ismulti = ETHER_ISMULTI(&(f->h->a1));
	f->len = PKTLEN(wlc->osh, f->p);
	f->seq = ltoh16(f->h->seq);
	f->ividx = 0;
#if defined(WLTDLS)
	f->istdls = BSSCFG_IS_DPT(scb->bsscfg) || BSSCFG_IS_TDLS(scb->bsscfg);
#endif 
	f->wds = ((f->fc & (FC_TODS | FC_FROMDS)) == (FC_TODS | FC_FROMDS));
	if (f->wds)
		offset += ETHER_ADDR_LEN;
	f->pbody = (uchar*)(f->h) + offset;

	/* account for QoS Control Field */
	f->qos = (f->type == FC_TYPE_DATA && FC_SUBTYPE_ANY_QOS(f->subtype));
	if (f->qos) {
		uint16 qoscontrol = ltoh16_ua(f->pbody);
		f->isamsdu = (qoscontrol & QOS_AMSDU_MASK) != 0;
		f->prio = (uint8)QOS_PRIO(qoscontrol);
		f->ac = WME_PRIO2AC(f->prio);
		f->apsd_eosp = QOS_EOSP(qoscontrol);
		f->pbody += DOT11_QOS_LEN;
		offset += DOT11_QOS_LEN;
		f->ividx = (uint8)PRIO2IVIDX(f->prio);
		ASSERT(f->ividx < WLC_NUMRXIVS);
	}
	f->ht = ((wrxh->rxhdr.PhyRxStatus_0 & PRXS0_FT_MASK) == PRXS0_PREN) &&
		((f->fc & FC_ORDER) && FC_SUBTYPE_ANY_QOS(f->subtype));
	if (f->ht) {
		f->pbody += DOT11_HTC_LEN;
		offset += DOT11_HTC_LEN;
	}

	f->body_len = f->len - offset;
	f->totlen = pkttotlen(wlc->osh, p) - offset;
	/* AMPDUXXX: WPA_auth may not be valid for wds */
	f->WPA_auth = scb->WPA_auth;
	f->wrxh = wrxh;
	f->rxh = &wrxh->rxhdr;
	f->rx_wep = 0;
	f->key = NULL;
}

#if defined(PKTC) || defined(PKTC_DONGLE)
bool BCMFASTPATH
wlc_ampdu_chainable(ampdu_rx_info_t *ampdu_rx, void *p, struct scb *scb, uint16 seq, uint16 tid)
{
	scb_ampdu_rx_t *scb_ampdu;
	scb_ampdu_tid_resp_t *resp;

	scb_ampdu = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
	ASSERT(scb_ampdu != NULL);
	resp = scb_ampdu->resp[tid];

	/* return if ampdu_rx not enabled on TID */
	if (resp == NULL)
		return FALSE;

	/* send up if expected seq */
	seq = seq >> SEQNUM_SHIFT;
	if (seq != resp->exp_seq) {
		WLCNTINCR(ampdu_rx->cnt->rxoos);
		return FALSE;
	}

	resp->alive = TRUE;

#ifdef WLAMPDU_HOSTREORDER
	if (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc) && resp->queued) {
		return FALSE;
	}
#endif
	if (resp->rxq[RX_SEQ_TO_INDEX(ampdu_rx, NEXT_SEQ(resp->exp_seq))] != NULL)
		return FALSE;

	resp->exp_seq = NEXT_SEQ(resp->exp_seq);

	return TRUE;
}
#endif /* PKTC */

void BCMFASTPATH
wlc_ampdu_update_rxcounters(ampdu_rx_info_t *ampdu_rx, uint32 ft, struct scb *scb,
	struct dot11_header *h)
{
	scb_ampdu_rx_t *scb_ampdu;
	uint8 *plcp;

	scb_ampdu = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
	ASSERT(scb_ampdu != NULL);

	plcp = ((uint8 *)h) - D11_PHY_HDR_LEN;
	if (ft == PRXS0_PREN) {
		if (WLC_IS_MIMO_PLCP_AMPDU(plcp)) {
			WLCNTINCR(ampdu_rx->cnt->rxampdu);
			WLCNTINCR(ampdu_rx->cnt->rxmpdu);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.rxampdu);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.rxmpdu);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
			if (ampdu_rx->amdbg && ((plcp[0] & 0x3f) <= AMPDU_MAX_MCS))
				ampdu_rx->amdbg->rxmcs[plcp[0] & 0x3f]++;
#endif
			if (PLCP3_ISSGI(plcp[3])) {
				WLCNTINCR(ampdu_rx->cnt->rxampdu_sgi);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
				if (ampdu_rx->amdbg && ((plcp[0] & 0x3f) <= AMPDU_MAX_MCS))
					ampdu_rx->amdbg->rxmcssgi[plcp[0] & 0x1f]++;
#endif
			}
			if (PLCP3_ISSTBC(plcp[3])) {
				WLCNTINCR(ampdu_rx->cnt->rxampdu_stbc);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
				if (ampdu_rx->amdbg && ((plcp[0] & 0x3f) <= AMPDU_MAX_MCS))
					ampdu_rx->amdbg->rxmcsstbc[plcp[0] & 0x3f]++;
#endif
			}
		} else if (!(plcp[0] | plcp[1] | plcp[2]))
			WLCNTINCR(ampdu_rx->cnt->rxmpdu);
		else
			WLCNTINCR(ampdu_rx->cnt->rxht);
	}
#ifdef WL11AC
	 else if (ft == FT_VHT) {
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
		uint8  vht = 0;
#endif
		if ((plcp[0] | plcp[1] | plcp[2])) {
			WLCNTINCR(ampdu_rx->cnt->rxampdu);
			WLCNTINCR(ampdu_rx->cnt->rxmpdu);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.rxampdu);
			AMPDUSCBCNTINCR(scb_ampdu->cnt.rxmpdu);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
			if (ampdu_rx->amdbg) {
				vht = wlc_vht_get_rate_from_plcp(plcp);
				ASSERT(vht & 0x80);
				vht  = (vht & 0xf) + (((vht & 0x70) >> 4)-1) * 10;
				ampdu_rx->amdbg->rxvht[vht]++;
			}
#endif
			if (VHT_PLCP3_ISSGI(plcp[3])) {
				WLCNTINCR(ampdu_rx->cnt->rxampdu_sgi);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
				if (ampdu_rx->amdbg)
					ampdu_rx->amdbg->rxvhtsgi[vht]++;
#endif
			}
			if (VHT_PLCP0_ISSTBC(plcp[0])) {
				WLCNTINCR(ampdu_rx->cnt->rxampdu_stbc);
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
				if (ampdu_rx->amdbg)
					ampdu_rx->amdbg->rxvhtstbc[vht]++;
#endif
			}
		} else
			WLCNTINCR(ampdu_rx->cnt->rxmpdu);
	}
#endif /* WL11AC */
	else {
		WLCNTINCR(ampdu_rx->cnt->rxlegacy);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.rxlegacy);
	}
}

void BCMFASTPATH
wlc_ampdu_recvdata(ampdu_rx_info_t *ampdu_rx, struct scb *scb, struct wlc_frminfo *f)
{
	scb_ampdu_rx_t *scb_ampdu;
	scb_ampdu_tid_resp_t *resp;
	wlc_info_t *wlc;
	uint16 seq, offset, indx, delta;
	uint8 *plcp;
	uint8 tid = f->prio;
	uint8  vht = 0;

#ifdef WLAMPDU_HOSTREORDER
		bool new_hole = FALSE;
#endif
	BCM_REFERENCE(vht);

	wlc = ampdu_rx->wlc;

	if (f->subtype != FC_SUBTYPE_QOS_DATA) {
		wlc_recvdata_ordered(wlc, scb, f);
		return;
	}

	ASSERT(scb);
	ASSERT(SCB_AMPDU(scb));

	ASSERT(tid < AMPDU_MAX_SCB_TID);
	ASSERT(!f->ismulti);

	scb_ampdu = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
	ASSERT(scb_ampdu);

	plcp = ((uint8 *)(f->h)) - D11_PHY_HDR_LEN;

	wlc_ampdu_update_rxcounters(ampdu_rx, f->rxh->PhyRxStatus_0 & PRXS0_FT_MASK, scb, f->h);

	resp = scb_ampdu->resp[tid];

	/* return if ampdu_rx not enabled on TID */
	if (resp == NULL) {
		if (((f->rxh->PhyRxStatus_0 & PRXS0_FT_MASK) == PRXS0_PREN) &&
			WLC_IS_MIMO_PLCP_AMPDU(plcp)) {
			scb_ampdu->resp_off[tid].ampdu_recd = TRUE;
			WLCNTINCR(ampdu_rx->cnt->rxnobapol);
		}
		wlc_recvdata_ordered(wlc, scb, f);
		return;
	}

	/* track if receiving aggregates from non-HT device */
	if (!SCB_HT_CAP(scb) &&
	    ((f->rxh->PhyRxStatus_0 & PRXS0_FT_MASK) == PRXS0_PREN) &&
	    WLC_IS_MIMO_PLCP_AMPDU(plcp)) {
		scb_ampdu->resp_off[tid].ampdu_recd = TRUE;
		WLCNTINCR(ampdu_rx->cnt->rxnobapol);
	}

	/* fragments not allowed */
	if (f->seq & FRAGNUM_MASK) {
		WL_ERROR(("wl%d: %s: unexp frag seq 0x%x, exp seq 0x%x\n",
			wlc->pub->unit, __FUNCTION__, f->seq, resp->exp_seq));
		goto toss;
	}

	seq = f->seq >> SEQNUM_SHIFT;
	indx = RX_SEQ_TO_INDEX(ampdu_rx, seq);

	WL_AMPDU_RX(("wl%d: %s: receiving seq 0x%x tid %d, exp seq %d indx %d\n",
	          wlc->pub->unit, __FUNCTION__, seq, tid, resp->exp_seq, indx));

	/* send up if expected seq */
	if (seq == resp->exp_seq) {
		uint bandunit;
		struct ether_addr ea;
		struct scb *newscb;
		wlc_bsscfg_t *bsscfg;
		bool update_host = FALSE;

		ASSERT(resp->exp_seq == pkt_h_seqnum(wlc, f->p));
		if (AMPDU_HOST_REORDER_ENABLED(wlc))
			ASSERT(!AMPDU_CHECK_HOST_HASPKT(resp, indx));
		else
			ASSERT(!resp->rxq[indx]);
		resp->alive = TRUE;
		resp->exp_seq = NEXT_SEQ(resp->exp_seq);

		bsscfg = scb->bsscfg;
		bandunit = scb->bandunit;
		bcopy(&scb->ea, &ea, ETHER_ADDR_LEN);

		if (!AMPDU_HOST_REORDER_ENABLED(wlc)) {
			wlc_recvdata_ordered(wlc, scb, f);
			/* validate that the scb is still around and some path in
			 * wlc_recvdata_ordered() did not free it
			*/
			newscb = wlc_scbfindband(wlc, bsscfg, &ea, bandunit);
			if ((newscb == NULL) || (newscb != scb)) {
				WL_ERROR(("wl%d: %s: scb freed; bail out\n",
					wlc->pub->unit, __FUNCTION__));
				return;
			}
		}
		/* release pending ordered packets */
		WL_AMPDU_RX(("wl%d: %s: Releasing pending packets\n",
			wlc->pub->unit, __FUNCTION__));
		if (AMPDU_HOST_REORDER_ENABLED(wlc) && resp->queued)
			update_host = TRUE;
		wlc_ampdu_release_ordered(wlc, scb_ampdu, tid);
		if (AMPDU_HOST_REORDER_ENABLED(wlc)) {
			/* exp_seq is updated now, cur is known,
			 * set the right flags and call wlc_recvdata_ordered
			*/
			if (update_host) {
				wlc_ampdu_setpkt_hostreorder_info(wlc, resp, f->p, indx, NO_NEWHOLE,
					NO_DEL_FLOW, NO_FLUSH_ALL);
			}
			wlc_recvdata_ordered(wlc, scb, f);
		}
		return;
	}

	/* out of order packet; validate and enq */
	offset = MODSUB_POW2(seq, resp->exp_seq, SEQNUM_MAX);

	/* check for duplicate or beyond half the sequence space */
	if (((offset < resp->ba_wsize) && AMPDU_PKT_PENDING(wlc, resp, indx)) ||
		(offset > (SEQNUM_MAX >> 1)))
	{
		ASSERT(seq == pkt_h_seqnum(wlc, f->p));
		WL_AMPDU_RX(("wl%d: wlc_ampdu_recvdata: duplicate seq 0x%x(dropped)\n",
			wlc->pub->unit, seq));
		PKTFREE(wlc->osh, f->p, FALSE);
		WLCNTINCR(ampdu_rx->cnt->rxdup);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.rxdup);
		return;
	}

	if (resp->queued == 0) {
#ifdef WLAMPDU_HOSTREORDER
		new_hole = TRUE;
#endif
	}


	/* move the start of window if acceptable out of window pkts */
	if (offset >= resp->ba_wsize) {
		delta = offset - resp->ba_wsize + 1;
		WL_AMPDU_RX(("wl%d: wlc_ampdu_recvdata: out of window pkt with"
			" seq 0x%x delta %d (exp seq 0x%x): moving window fwd\n",
			wlc->pub->unit, seq, delta, resp->exp_seq));

		wlc_ampdu_release_n_ordered(wlc, scb_ampdu, tid, delta);

		/* recalc resp since may have been freed while releasing frames */
		if ((resp = scb_ampdu->resp[tid])) {
			if (AMPDU_HOST_REORDER_ENABLED(wlc)) {
				ASSERT(!AMPDU_CHECK_HOST_HASPKT(resp, indx));
				/* set the index to say pkt is pending */
				AMPDU_HOST_HASPKT(resp, indx);
			}
			else {
				ASSERT(!resp->rxq[indx]);
				resp->rxq[indx] = f->p;
				resp->wrxh[indx] = f->wrxh;
			}
			resp->queued++;
		}
		wlc_ampdu_release_ordered(wlc, scb_ampdu, tid);

		if (AMPDU_HOST_REORDER_ENABLED(wlc)) {
			if (delta >  resp->ba_wsize) {
				wlc_ampdu_setpkt_hostreorder_info(wlc, resp, f->p, indx, new_hole,
					NO_DEL_FLOW, FLUSH_ALL);
			}
			else {
				wlc_ampdu_setpkt_hostreorder_info(wlc, resp, f->p, indx, new_hole,
					NO_DEL_FLOW, NO_FLUSH_ALL);
			}
			wlc_recvdata_ordered(wlc, scb, f);
		}

		WLCNTINCR(ampdu_rx->cnt->rxoow);
		AMPDUSCBCNTINCR(scb_ampdu->cnt.rxoow);
		return;
	}

	WL_AMPDU_RX(("wl%d: wlc_ampdu_recvdata: q out of order seq 0x%x(exp 0x%x)\n",
		wlc->pub->unit, seq, resp->exp_seq));

	resp->queued++;
	if (AMPDU_HOST_REORDER_ENABLED(wlc)) {
		ASSERT(!AMPDU_CHECK_HOST_HASPKT(resp, indx));
		/* set the index to say pkt is pending */
		AMPDU_HOST_HASPKT(resp, indx);
		wlc_ampdu_setpkt_hostreorder_info(wlc, resp, f->p, indx, new_hole,
			NO_DEL_FLOW, NO_FLUSH_ALL);
		wlc_recvdata_ordered(wlc, scb, f);
	}
	else {
		ASSERT(!resp->rxq[indx]);
		resp->rxq[indx] = f->p;
		resp->wrxh[indx] = f->wrxh;
	}
	if (ampdu_rx->resp_timer_running == FALSE) {
		ampdu_rx->resp_timer_running = TRUE;
		wl_add_timer(wlc->wl, ampdu_rx->resp_timer, AMPDU_RESP_TIMEOUT, TRUE);
	}

	WLCNTINCR(ampdu_rx->cnt->rxqed);

	return;

toss:
	WL_AMPDU_RX(("wl%d: %s: Received some unexpected packets\n", wlc->pub->unit, __FUNCTION__));
	PKTFREE(wlc->osh, f->p, FALSE);
	WLCNTINCR(ampdu_rx->cnt->rxunexp);

	/* AMPDUXXX: protocol failure, send delba */
	wlc_ampdu_rx_send_delba(ampdu_rx, scb, tid, FALSE, DOT11_RC_UNSPECIFIED);
}

void
ampdu_cleanup_tid_resp(ampdu_rx_info_t *ampdu_rx, struct scb *scb, uint8 tid)
{
	scb_ampdu_rx_t *scb_ampdu;
	scb_ampdu_tid_resp_t *resp;

	scb_ampdu = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
	ASSERT(scb_ampdu);
	ASSERT(scb_ampdu->scb);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	AMPDU_VALIDATE_TID(ampdu_rx, tid, "ampdu_cleanup_tid_resp");

	if (scb_ampdu->resp[tid] == NULL)
		return;

	WL_AMPDU_CTL(("wl%d: ampdu_cleanup_tid_resp: tid %d\n", ampdu_rx->wlc->pub->unit, tid));


#ifdef WLAMPDU_HOSTREORDER
	if (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc)) {
		wlc_ampdu_free_flow_id(ampdu_rx, scb_ampdu->resp[tid], scb_ampdu->scb);
		}
#endif /* WLAMPDU_HOSTREORDER */

	/* send up all the pending pkts in order from the rx reorder q going over holes */
	wlc_ampdu_release_n_ordered(ampdu_rx->wlc, scb_ampdu, tid, ampdu_rx->ba_max_rx_wsize);

	/* recheck scb_ampdu->resp[] since it may have been freed while releasing */
	if ((resp = scb_ampdu->resp[tid])) {
		ASSERT(resp->queued == 0);
#ifdef WLAMPDU_HOSTREORDER
		if (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc)) {
			if (resp->tohost_ctrlpkt != NULL)
				PKTFREE(ampdu_rx->wlc->osh, resp->tohost_ctrlpkt, FALSE);
		}
#endif /* WLAMPDU_HOSTREORDER */
		MFREE(ampdu_rx->wlc->osh, resp, sizeof(scb_ampdu_tid_resp_t));
		scb_ampdu->resp[tid] = NULL;
	}

	ampdu_rx->resp_cnt--;
	if ((ampdu_rx->resp_cnt == 0) && (ampdu_rx->resp_timer_running == TRUE)) {
		wl_del_timer(ampdu_rx->wlc->wl, ampdu_rx->resp_timer);
		ampdu_rx->resp_timer_running = FALSE;
	}
}

void
wlc_ampdu_recv_addba_req_resp(ampdu_rx_info_t *ampdu_rx, struct scb *scb,
	dot11_addba_req_t *addba_req, int body_len)
{
	scb_ampdu_rx_t *scb_ampdu_rx;
	scb_ampdu_tid_resp_t *resp;
	uint16 param_set, timeout, start_seq;
	uint8 tid, wsize, policy;
#ifdef WLAMPDU_HOSTREORDER
	void *tohost_ctrlpkt = NULL;
#endif /* WLAMPDU_HOSTREORDER */

	ASSERT(scb);
	ASSERT(ampdu_rx);

	scb_ampdu_rx = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
	ASSERT(scb_ampdu_rx);

	timeout = ltoh16_ua(&addba_req->timeout);
	start_seq = ltoh16_ua(&addba_req->start_seqnum);
	param_set = ltoh16_ua(&addba_req->addba_param_set);

	/* accept the min of our and remote timeout */
	timeout = MIN(timeout, ampdu_rx->delba_timeout);

	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	AMPDU_VALIDATE_TID(ampdu_rx, tid, "wlc_ampdu_recv_addba_req_resp");

	if (ampdu_rx->rxaggr == OFF) {
		wlc_send_addba_resp(ampdu_rx->wlc, scb, DOT11_SC_DECLINED,
			addba_req->token, timeout, param_set);
		return;
	}

	if (!AMPDU_ENAB(ampdu_rx->wlc->pub) || (scb->bsscfg->BSS && !SCB_HT_CAP(scb))) {
		wlc_send_addba_resp(ampdu_rx->wlc, scb, DOT11_SC_DECLINED,
			addba_req->token, timeout, param_set);
		WLCNTINCR(ampdu_rx->cnt->txaddbaresp);
		return;
	}

	if (!ampdu_rx->rxba_enable[tid]) {
		wlc_send_addba_resp(ampdu_rx->wlc, scb, DOT11_SC_DECLINED,
			addba_req->token, timeout, param_set);
		WLCNTINCR(ampdu_rx->cnt->txaddbaresp);
		return;
	}

	policy = (param_set & DOT11_ADDBA_PARAM_POLICY_MASK) >> DOT11_ADDBA_PARAM_POLICY_SHIFT;
	if (policy != ampdu_rx->ba_policy) {
		wlc_send_addba_resp(ampdu_rx->wlc, scb, DOT11_SC_INVALID_PARAMS,
			addba_req->token, timeout, param_set);
		WLCNTINCR(ampdu_rx->cnt->txaddbaresp);
		return;
	}

	/* cleanup old state */
	ampdu_cleanup_tid_resp(ampdu_rx, scb, tid);

	ASSERT(scb_ampdu_rx->resp[tid] == NULL);

	resp = MALLOC(ampdu_rx->wlc->osh, sizeof(scb_ampdu_tid_resp_t));

#ifdef WLAMPDU_HOSTREORDER
	if (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc))
		tohost_ctrlpkt = PKTGET(ampdu_rx->wlc->osh, TXOFF, FALSE);
	if ((resp == NULL) || (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc) &&
		(tohost_ctrlpkt == NULL)))
#else /* WLAMPDU_HOSTREORDER */
	if (resp == NULL)
#endif /* WLAMPDU_HOSTREORDER */
	{
		wlc_send_addba_resp(ampdu_rx->wlc, scb, DOT11_SC_FAILURE,
			addba_req->token, timeout, param_set);
		WLCNTINCR(ampdu_rx->cnt->txaddbaresp);
		return;
	}
	bzero((char *)resp, sizeof(scb_ampdu_tid_resp_t));

#ifdef WLAMPDU_HOSTREORDER
	if (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc))
		resp->tohost_ctrlpkt = tohost_ctrlpkt;
#endif /* WLAMPDU_HOSTREORDER */

	wsize =	(param_set & DOT11_ADDBA_PARAM_BSIZE_MASK) >> DOT11_ADDBA_PARAM_BSIZE_SHIFT;
	/* accept the min of our and remote wsize if remote has the advisory set */
	if (wsize)
		wsize = MIN(wsize, ampdu_rx->ba_rx_wsize);
	else
		wsize = ampdu_rx->ba_rx_wsize;
	WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_addba_req: BA ON: seq 0x%x tid %d wsize %d\n",
		ampdu_rx->wlc->pub->unit, start_seq, tid, wsize));

	param_set &= ~DOT11_ADDBA_PARAM_BSIZE_MASK;
	param_set |= (wsize << DOT11_ADDBA_PARAM_BSIZE_SHIFT) & DOT11_ADDBA_PARAM_BSIZE_MASK;

	scb_ampdu_rx->resp[tid] = resp;
	resp->exp_seq = start_seq >> SEQNUM_SHIFT;
	resp->ba_wsize = wsize;
	resp->ba_state = AMPDU_TID_STATE_BA_ON;

	param_set &= ~DOT11_ADDBA_PARAM_AMSDU_SUP;
#ifdef WLAMPDU_HOSTREORDER
	if (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc))
		resp->flow_id = wlc_ampdu_alloc_flow_id(ampdu_rx);
#endif /* WLAMPDU_HOSTREORDER */

#ifdef WLAMSDU
	/* Set the A-MSDU supported field for aqm chips */
	if (D11REV_GE(ampdu_rx->wlc->pub->corerev, 40) && ampdu_rx->wlc->_rx_amsdu_in_ampdu) {
		param_set |= DOT11_ADDBA_PARAM_AMSDU_SUP;
	}
#endif /* WLAMSDU */

	WLCNTINCR(ampdu_rx->cnt->rxaddbareq);

	wlc_send_addba_resp(ampdu_rx->wlc, scb, DOT11_SC_SUCCESS, addba_req->token,
		timeout, param_set);
	WLCNTINCR(ampdu_rx->cnt->txaddbaresp);
	AMPDUSCBCNTINCR(scb_ampdu_rx->cnt.txaddbaresp);

	ampdu_rx->resp_cnt++;
}

void
wlc_ampdu_rx_recv_delba(ampdu_rx_info_t *ampdu_rx, struct scb *scb, uint8 tid, uint8 category,
	uint16 initiator, uint16 reason)
{
	scb_ampdu_rx_t *scb_ampdu_rx;

	ASSERT(scb);

	scb_ampdu_rx = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
	ASSERT(scb_ampdu_rx);

	if (category & DOT11_ACTION_CAT_ERR_MASK) {
		WL_ERROR(("wl%d: %s: unexp error action frame\n",
			ampdu_rx->wlc->pub->unit, __FUNCTION__));
		WLCNTINCR(ampdu_rx->cnt->rxunexp);
		return;
	}

	ampdu_cleanup_tid_resp(ampdu_rx, scb, tid);

	WLCNTINCR(ampdu_rx->cnt->rxdelba);
	AMPDUSCBCNTINCR(scb_ampdu_rx->cnt.rxdelba);

	WL_ERROR(("wl%d: %s: AMPDU OFF: tid %d initiator %d reason %d\n",
		ampdu_rx->wlc->pub->unit, __FUNCTION__, tid, initiator, reason));
}

/* moves the window forward on receipt of a bar */
void
wlc_ampdu_recv_bar(ampdu_rx_info_t *ampdu_rx, struct scb *scb, uint8 *body, int body_len)
{
	scb_ampdu_rx_t *scb_ampdu_rx;
	struct dot11_bar *bar = (struct dot11_bar *)body;
	scb_ampdu_tid_resp_t *resp;
	uint8 tid;
	uint16 seq, tmp, offset;
	void *p = NULL;

	ASSERT(scb);
	ASSERT(SCB_AMPDU(scb));

	scb_ampdu_rx = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
	ASSERT(scb_ampdu_rx);

	tmp = ltoh16(bar->bar_control);
	tid = (tmp & DOT11_BA_CTL_TID_MASK) >> DOT11_BA_CTL_TID_SHIFT;
	AMPDU_VALIDATE_TID(ampdu_rx, tid, "wlc_ampdu_recv_bar");

	if (tmp & DOT11_BA_CTL_MTID) {
		WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_bar: multi tid not supported\n",
			ampdu_rx->wlc->pub->unit));
		WLCNTINCR(ampdu_rx->cnt->rxunexp);
		return;
	}

	resp = scb_ampdu_rx->resp[tid];
	if (resp == NULL) {
		WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_bar: uninitialized tid %d\n",
			ampdu_rx->wlc->pub->unit, tid));
		WLCNTINCR(ampdu_rx->cnt->rxunexp);
		return;
	}

	WLCNTINCR(ampdu_rx->cnt->rxbar);
	AMPDUSCBCNTINCR(scb_ampdu_rx->cnt.rxbar);

	seq = (ltoh16(bar->seqnum)) >> SEQNUM_SHIFT;

	WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_bar: length %d tid %d seq 0x%x\n",
		ampdu_rx->wlc->pub->unit, body_len, tid, seq));

	offset = MODSUB_POW2(seq, resp->exp_seq, SEQNUM_MAX);

	/* ignore if it is in the "old" half of sequence space */
	if (offset > (SEQNUM_MAX >> 1)) {
		WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_bar: ignore bar with offset 0x%x\n",
			ampdu_rx->wlc->pub->unit, offset));
		return;
	}

	if (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc) && resp->queued) {
		p = PKTGET(ampdu_rx->wlc->osh, TXOFF, FALSE);
		if (p == NULL) {
			return;
		}
		PKTPULL(ampdu_rx->wlc->osh, p, TXOFF);
		PKTSETLEN(ampdu_rx->wlc->osh, p, 0);
	}
	/* release all received pkts till the seq */
	wlc_ampdu_release_n_ordered(ampdu_rx->wlc, scb_ampdu_rx, tid, offset);

	/* release more pending ordered packets if possible */
	wlc_ampdu_release_ordered(ampdu_rx->wlc, scb_ampdu_rx, tid);

	if (AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc) && (p != NULL)) {
		if (offset > ampdu_rx->wlc->ampdu_rx->ba_max_rx_wsize) {
			wlc_ampdu_setpkt_hostreorder_info(ampdu_rx->wlc, resp,
				p, AMPDU_INVALID_INDEX, NO_NEWHOLE, NO_DEL_FLOW, FLUSH_ALL);
		}
		else {
			wlc_ampdu_setpkt_hostreorder_info(ampdu_rx->wlc, resp, p,
				AMPDU_INVALID_INDEX, NO_NEWHOLE, NO_DEL_FLOW, NO_FLUSH_ALL);
		}
		wl_sendup(ampdu_rx->wlc->wl, SCB_INTERFACE(scb), p, 1);
	}
}

void
wlc_ampdu_rx_send_delba(ampdu_rx_info_t *ampdu_rx, struct scb *scb, uint8 tid,
	uint16 initiator, uint16 reason)
{
	ampdu_cleanup_tid_resp(ampdu_rx, scb, tid);

	WL_ERROR(("wl%d: %s: tid %d initiator %d reason %d\n",
		ampdu_rx->wlc->pub->unit, __FUNCTION__, tid, initiator, reason));

	wlc_send_delba(ampdu_rx->wlc, scb, tid, initiator, reason);

	WLCNTINCR(ampdu_rx->cnt->txdelba);
}

int
wlc_ampdu_rx_set(ampdu_rx_info_t *ampdu_rx, bool on)
{
	wlc_info_t *wlc = ampdu_rx->wlc;
	int err = BCME_OK;

	wlc->pub->_ampdu_rx = FALSE;

	if (on) {
		if (!N_ENAB(wlc->pub)) {
			WL_AMPDU_ERR(("wl%d: driver not nmode enabled\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			goto exit;
		}
		if (!wlc_ampdu_rx_cap(ampdu_rx)) {
			WL_AMPDU_ERR(("wl%d: device not ampdu capable\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			goto exit;
		}
		if (PIO_ENAB(wlc->pub)) {
			WL_AMPDU_ERR(("wl%d: driver is pio mode\n", wlc->pub->unit));
			err = BCME_UNSUPPORTED;
			goto exit;
		}
	}

	if (wlc->pub->_ampdu_rx != on) {
#ifdef WLCNT
		bzero(ampdu_rx->cnt, sizeof(wlc_ampdu_rx_cnt_t));
#endif
		wlc->pub->_ampdu_rx = on;
	}

exit:
	return err;
}

bool
wlc_ampdu_rx_cap(ampdu_rx_info_t *ampdu_rx)
{
	if (WLC_PHY_11N_CAP(ampdu_rx->wlc->band))
		return TRUE;
	else
		return FALSE;
}

void
wlc_ampdu_update_ie_param(ampdu_rx_info_t *ampdu_rx)
{
	uint8 params;
	wlc_info_t *wlc = ampdu_rx->wlc;

	params = wlc->ht_cap.params;
	params &= ~(HT_PARAMS_RX_FACTOR_MASK | HT_PARAMS_DENSITY_MASK);
	params |= (ampdu_rx->rx_factor & HT_PARAMS_RX_FACTOR_MASK);
	params |= (ampdu_rx->mpdu_density << HT_PARAMS_DENSITY_SHIFT) & HT_PARAMS_DENSITY_MASK;
	wlc->ht_cap.params = params;

	if (AP_ENAB(wlc->pub) && wlc->clk) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}

}

void
wlc_ampdu_shm_upd(ampdu_rx_info_t *ampdu_rx)
{
	wlc_info_t *wlc = ampdu_rx->wlc;

	if (AMPDU_ENAB(wlc->pub) && (WLC_PHY_11N_CAP(wlc->band)))	{
		/* Extend ucode internal watchdog timer to match larger received frames */
		if ((ampdu_rx->rx_factor & HT_PARAMS_RX_FACTOR_MASK) == AMPDU_RX_FACTOR_64K) {
			wlc_write_shm(wlc, M_MIMO_MAXSYM, MIMO_MAXSYM_MAX);
			wlc_write_shm(wlc, M_WATCHDOG_8TU, WATCHDOG_8TU_MAX);
		} else {
			wlc_write_shm(wlc, M_MIMO_MAXSYM, MIMO_MAXSYM_DEF);
			wlc_write_shm(wlc, M_WATCHDOG_8TU, WATCHDOG_8TU_DEF);
		}
	}
}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
int
wlc_ampdu_rx_dump(ampdu_rx_info_t *ampdu_rx, struct bcmstrbuf *b)
{
#ifdef WLCNT
	wlc_ampdu_rx_cnt_t *cnt = ampdu_rx->cnt;
#endif
	int i;
	uint32 max_val;
	struct scb *scb;
	struct scb_iter scbiter;
	scb_ampdu_rx_t *scb_ampdu;
	int resp = 0;
	char eabuf[ETHER_ADDR_STR_LEN];
	int last = 0;

	bcm_bprintf(b, "AMPDU Rx counters:\n");

#ifdef WLCNT
	bcm_bprintf(b, "rxampdu %d rxmpdu %d rxmpduperampdu %d\n",
		cnt->rxampdu, cnt->rxmpdu,
		cnt->rxampdu ? CEIL(cnt->rxmpdu, cnt->rxampdu) : 0);
#ifdef WLAMPDU_MAC
	if (AMPDU_MAC_ENAB(ampdu_rx->wlc->pub))
		bcm_bprintf(b, "rxampdu_sgi %d rxampdu_stbc %d ",
		  cnt->rxampdu_sgi, cnt->rxampdu_stbc);
	else
#endif
		bcm_bprintf(b, "rxampdu_sgi %d rxampdu_stbc %d ",
			cnt->rxampdu_sgi, cnt->rxampdu_stbc);
	bcm_bprintf(b, "rxht %d rxlegacy %d\n", cnt->rxht, cnt->rxlegacy);

	bcm_bprintf(b, "rxholes %d rxqed %d rxdup %d rxnobapol %d "
		"rxstuck %d rxoow %d rxoos %d\n",
		cnt->rxholes, cnt->rxqed, cnt->rxdup, cnt->rxnobapol,
		cnt->rxstuck, cnt->rxoow, cnt->rxoos);
	bcm_bprintf(b, "rxaddbareq %d rxbar %d txba %d txaddbaresp %d rxdelba %d rxunexp %d\n",
		cnt->rxaddbareq, cnt->rxbar, cnt->txba,
		cnt->txaddbaresp, cnt->rxdelba, cnt->rxunexp);
#endif /* WLCNT */

	FOREACHSCB(ampdu_rx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
			ASSERT(scb_ampdu);
			for (i = 0; i < AMPDU_MAX_SCB_TID; i++) {
				if (scb_ampdu->resp[i])
					resp++;
			}
		}
	}

	bcm_bprintf(b, "resp %d\n", resp);

	for (i = 0, max_val = 0, last = 0; i <= AMPDU_MAX_MCS; i++) {
		max_val += ampdu_rx->amdbg->rxmcs[i];
		if (ampdu_rx->amdbg->rxmcs[i]) last = i;
	}
	last = 8 * (last/8 + 1) - 1;
	bcm_bprintf(b, "RX MCS  :");
	if (max_val) {
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d(%d%%)", ampdu_rx->amdbg->rxmcs[i],
				(ampdu_rx->amdbg->rxmcs[i] * 100) / max_val);
			if ((i % 8) == 7 && i != last)
				bcm_bprintf(b, "\n        :");
		}
	}
#ifdef WL11AC
	for (i = 0, max_val = 0, last = 0; i < AMPDU_MAX_VHT; i++) {
		max_val += ampdu_rx->amdbg->rxvht[i];
		if (ampdu_rx->amdbg->rxvht[i]) last = i;
	}
	last = 10 * (last/10 + 1) - 1;
	bcm_bprintf(b, "\nRX VHT  :");
	if (max_val) {
		for (i = 0; i <= last; i++) {
			bcm_bprintf(b, "  %d(%d%%)", ampdu_rx->amdbg->rxvht[i],
				(ampdu_rx->amdbg->rxvht[i] * 100) / max_val);
			if ((i % 10) == 9 && i != last)
				bcm_bprintf(b, "\n        :");
		}
	}
#endif /* WL11AC */
	bcm_bprintf(b, "\n");

	if (WLC_SGI_CAP_PHY(ampdu_rx->wlc)) {
		for (i = 0, max_val = 0, last = 0; i <= AMPDU_MAX_MCS; i++) {
			max_val += ampdu_rx->amdbg->rxmcssgi[i];
			if (ampdu_rx->amdbg->rxmcssgi[i]) last = i;
		}
		last = 8 * (last/8 + 1) - 1;
		bcm_bprintf(b, "RX MCS SGI:");
		if (max_val) {
			for (i = 0; i <= last; i++) {
				bcm_bprintf(b, "  %d(%d%%)", ampdu_rx->amdbg->rxmcssgi[i],
				            (ampdu_rx->amdbg->rxmcssgi[i] * 100) / max_val);
				if ((i % 8) == 7 && i != last)
					bcm_bprintf(b, "\n          :");
			}
		}
#ifdef WL11AC
		for (i = 0, max_val = 0, last = 0; i < AMPDU_MAX_VHT; i++) {
			max_val += ampdu_rx->amdbg->rxvhtsgi[i];
			if (ampdu_rx->amdbg->rxvhtsgi[i]) last = i;
		}

		bcm_bprintf(b, "\nRX VHT SGI:");
		if (max_val) {
			for (i = 0; i <= last; i++) {
				bcm_bprintf(b, "  %d(%d%%)", ampdu_rx->amdbg->rxvhtsgi[i],
					(ampdu_rx->amdbg->rxvhtsgi[i] * 100) / max_val);
				if ((i % 10) == 9 && i != last)
					bcm_bprintf(b, "\n          :");
			}
		}
#endif /* WL11AC */
		bcm_bprintf(b, "\n");

		if (WLCISLCNPHY(ampdu_rx->wlc->band) || (NREV_GT(ampdu_rx->wlc->band->phyrev, 3) &&
			NREV_LE(ampdu_rx->wlc->band->phyrev, 6)))
		{
			bcm_bprintf(b, "RX MCS STBC:");
			for (i = 0, max_val = 0; i <= AMPDU_MAX_MCS; i++)
				max_val += ampdu_rx->amdbg->rxmcsstbc[i];

			if (max_val) {
				for (i = 0; i <= 7; i++)
					bcm_bprintf(b, "  %d(%d%%)", ampdu_rx->amdbg->rxmcsstbc[i],
						(ampdu_rx->amdbg->rxmcsstbc[i] * 100) / max_val);
			}
#ifdef WL11AC
			for (i = 0, max_val = 0; i < AMPDU_MAX_VHT; i++)
				max_val += ampdu_rx->amdbg->rxvhtstbc[i];

			bcm_bprintf(b, "\nRX VHT STBC:");
			if (max_val) {
				for (i = 0; i < 10; i++) {
					bcm_bprintf(b, "  %d(%d%%)", ampdu_rx->amdbg->rxvhtstbc[i],
					(ampdu_rx->amdbg->rxvhtstbc[i] * 100) / max_val);
				}
			}
#endif /* WL11AC */
			bcm_bprintf(b, "\n");
		}
	}

	FOREACHSCB(ampdu_rx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			scb_ampdu = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
			bcm_bprintf(b, "%s: \n", bcm_ether_ntoa(&scb->ea, eabuf));
#ifdef BCMDBG
			bcm_bprintf(b, "\trxampdu %u rxmpdu %u rxlegacy %u rxbar %u rxdelba %u\n"
			            "\trxholes %u rxstuck %u rxoow %u rxdup %u\n",
			            scb_ampdu->cnt.rxampdu, scb_ampdu->cnt.rxmpdu,
			            scb_ampdu->cnt.rxlegacy, scb_ampdu->cnt.rxbar,
			            scb_ampdu->cnt.rxdelba, scb_ampdu->cnt.rxholes,
			            scb_ampdu->cnt.rxstuck, scb_ampdu->cnt.rxoow,
			            scb_ampdu->cnt.rxdup);
#endif /* BCMDBG */
		}
	}

	bcm_bprintf(b, "\n");

	return 0;
}
#endif /* BCMDBG || WLTEST */

/* function to send addba resp
 * Does not have any dependency on ampdu, so can be used for delayed ba as well
 */
int
wlc_send_addba_resp(wlc_info_t *wlc, struct scb *scb, uint16 status,
	uint8 token, uint16 timeout, uint16 param_set)
{
	dot11_addba_resp_t *addba_resp;
	void *p;
	uint8 *pbody;
	uint16 tid;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb->bsscfg);

	if (wlc->block_datafifo)
		return BCME_NOTREADY;

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, &scb->ea, &scb->bsscfg->cur_etheraddr,
		&scb->bsscfg->BSSID, DOT11_ADDBA_RESP_LEN, &pbody);
	if (p == NULL)
		return BCME_NOMEM;

	addba_resp = (dot11_addba_resp_t *)pbody;
	addba_resp->category = DOT11_ACTION_CAT_BLOCKACK;
	addba_resp->action = DOT11_BA_ACTION_ADDBA_RESP;
	addba_resp->token = token;
	htol16_ua_store(status, (uint8 *)&addba_resp->status);
	htol16_ua_store(param_set, (uint8 *)&addba_resp->addba_param_set);
	htol16_ua_store(timeout, (uint8 *)&addba_resp->timeout);

	WL_AMPDU_CTL(("wl%d: wlc_send_addba_resp: status %d param_set 0x%x\n",
		wlc->pub->unit, status, param_set));


	/* set same priority as tid */
	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	PKTSETPRIO(p, tid);

	wlc_sendmgmt(wlc, p, SCB_WLCIFP(scb)->qi, scb);

	return 0;
}

#if defined(BCMDBG) || defined(WLTEST) || defined(WLPKTDLYSTAT) || \
	defined(BCMDBG_AMPDU)
#ifdef WLCNT
void
wlc_ampdu_clear_rx_dump(ampdu_rx_info_t *ampdu_rx)
{
#ifdef BCMDBG
	struct scb *scb;
	struct scb_iter scbiter;
	scb_ampdu_rx_t *scb_ampdu_rx;
#endif /* BCMDBG */

	/* zero the counters */
	bzero(ampdu_rx->cnt, sizeof(wlc_ampdu_rx_cnt_t));

	/* reset the histogram as well */
	if (ampdu_rx->amdbg) {
		bzero(ampdu_rx->amdbg->rxmcs, sizeof(ampdu_rx->amdbg->rxmcs));
		bzero(ampdu_rx->amdbg->rxmcssgi, sizeof(ampdu_rx->amdbg->rxmcssgi));
		bzero(ampdu_rx->amdbg->rxmcsstbc, sizeof(ampdu_rx->amdbg->rxmcsstbc));
#ifdef WL11AC
		bzero(ampdu_rx->amdbg->rxvht, sizeof(ampdu_rx->amdbg->rxvht));
		bzero(ampdu_rx->amdbg->rxvhtsgi, sizeof(ampdu_rx->amdbg->rxvhtsgi));
		bzero(ampdu_rx->amdbg->rxvhtstbc, sizeof(ampdu_rx->amdbg->rxvhtstbc));
#endif
	}

#ifdef WLAMPDU_MAC
		/* zero out shmem counters */
		if (AMPDU_MAC_ENAB(ampdu_rx->wlc->pub))
			wlc_write_shm(ampdu_rx->wlc, M_RXBA_CNT, 0);
#endif /* WLAMPDU_MAC */

#ifdef BCMDBG
	FOREACHSCB(ampdu_rx->wlc->scbstate, &scbiter, scb) {
		if (SCB_AMPDU(scb)) {
			/* reset the per-SCB statistics */
			scb_ampdu_rx = SCB_AMPDU_RX_CUBBY(ampdu_rx, scb);
			bzero(&scb_ampdu_rx->cnt, sizeof(scb_ampdu_cnt_rx_t));
		}
	}
#endif /* BCMDBG */
}
#endif /* WLCNT */
#endif /* defined(BCMDBG) || defined(WLTEST) */

bool
wlc_ampdu_rx_aggr(ampdu_rx_info_t *ampdu_rx)
{
	return (ampdu_rx->rxaggr);
}

bool
wlc_ampdu_rxba_enable(ampdu_rx_info_t *ampdu_rx, uint8 tid)
{
	return (ampdu_rx->rxba_enable[tid]);
}

uint8
wlc_ampdu_rx_get_mpdu_density(ampdu_rx_info_t *ampdu_rx)
{
	return (ampdu_rx->mpdu_density);
}

void
wlc_ampdu_rx_set_mpdu_density(ampdu_rx_info_t *ampdu_rx, uint8 mpdu_density)
{
	ampdu_rx->mpdu_density = mpdu_density;
}

void
wlc_ampdu_rx_set_ba_rx_wsize(ampdu_rx_info_t *ampdu_rx, uint8 wsize)
{
	ampdu_rx->ba_rx_wsize = wsize;
}

uint8
wlc_ampdu_rx_get_ba_max_rx_wsize(ampdu_rx_info_t *ampdu_rx)
{
	return (ampdu_rx->ba_max_rx_wsize);
}


#ifdef WLAMPDU_HOSTREORDER
static int
wlc_ampdu_alloc_flow_id(ampdu_rx_info_t *ampdu_rx)
{
	if (!AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc)) {
		WL_ERROR(("%s: ERROR: AMPDU Host reordering not enabled, so shouldn't be here\n",
			__FUNCTION__));
		ASSERT(0);
		return -1;
	}
	ampdu_rx->flow_id++;
	return (ampdu_rx->flow_id);
}

static int
wlc_ampdu_free_flow_id(ampdu_rx_info_t *ampdu_rx, scb_ampdu_tid_resp_t *resp, struct scb *scb)
{
	void *p;

	if (!AMPDU_HOST_REORDER_ENABLED(ampdu_rx->wlc)) {
		WL_ERROR(("%s: ERROR: AMPDU Host reordering not enabled, so shouldn't be here\n",
			__FUNCTION__));
		ASSERT(0);
		return -1;
	}

	p = resp->tohost_ctrlpkt;
	resp->tohost_ctrlpkt = NULL;
	if (p == NULL)
		p = PKTGET(ampdu_rx->wlc->osh, TXOFF, FALSE);

	if (p == NULL) {
		WL_ERROR(("error couldn't alloc packet to cleanup the ampdu host reorder flow\n"));
		return -1;
	}
	PKTPULL(ampdu_rx->wlc->osh, p, TXOFF);
	PKTSETLEN(ampdu_rx->wlc->osh, p, 0);

	wlc_ampdu_setpkt_hostreorder_info(ampdu_rx->wlc, resp, p, AMPDU_INVALID_INDEX,
		NO_NEWHOLE, DEL_FLOW, FLUSH_ALL);
	wl_sendup(ampdu_rx->wlc->wl, SCB_INTERFACE(scb), p, 1);
	return 0;
}

static void
wlc_ampdu_setpkt_hostreorder_info(wlc_info_t *wlc, scb_ampdu_tid_resp_t *resp, void *p,
	uint16 cur_idx, bool new_hole, bool del_flow, bool flush_all)
{
	wlc_pkttag_t *pkttag =  WLPKTTAG(p);

	if (!AMPDU_HOST_REORDER_ENABLED(wlc)) {
		WL_ERROR(("%s: ERROR: AMPDU Host reordering not enabled, so shouldn't be here\n",
			__FUNCTION__));
		ASSERT(0);
		return;
	}

	pkttag->flags2 |= WLF2_HOSTREORDERAMPDU_INFO;
	pkttag->u.ampdu_info_to_host.ampdu_flow_id = resp->flow_id;
	/* 0 based...so -1 */
	pkttag->u.ampdu_info_to_host.max_idx = AMPDU_BA_MAX_WSIZE -  1;
	if (del_flow) {
		pkttag->u.ampdu_info_to_host.flags = WLHOST_REORDERDATA_DEL_FLOW;
		return;
	}

	if (cur_idx != AMPDU_INVALID_INDEX) {
		pkttag->u.ampdu_info_to_host.flags = WLHOST_REORDERDATA_CURIDX_VALID;
		if (flush_all) {
			printf("setting the flush all flag");
			pkttag->u.ampdu_info_to_host.flags |= WLHOST_REORDERDATA_FLUSH_ALL;
		}

		pkttag->shared.ampdu_seqs_to_host.cur_idx = cur_idx;
	}
	pkttag->u.ampdu_info_to_host.flags |= WLHOST_REORDERDATA_EXPIDX_VALID;
	pkttag->shared.ampdu_seqs_to_host.exp_idx =  RX_SEQ_TO_INDEX(wlc->ampdu_rx, resp->exp_seq);

	if (new_hole) {
		pkttag->u.ampdu_info_to_host.flags |= WLHOST_REORDERDATA_NEW_HOLE;
		WL_INFORM(("AMPDU_HOSTREORDER message to host...curidx %d expidx %d,"
			"flags 0x%02x\n",
			pkttag->shared.ampdu_seqs_to_host.cur_idx,
			pkttag->shared.ampdu_seqs_to_host.exp_idx,
			pkttag->u.ampdu_info_to_host.flags));
	}
}
#endif /* WLAMPDU_HOSTREORDER */
