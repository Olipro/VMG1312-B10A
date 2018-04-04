/*
 * Block Ack protocol source file
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
 * $Id: wlc_ba.c,v 1.56.2.2 2010-05-28 16:32:26 Exp $
 */
#ifndef WLBA
#error "WLBA is not defined"
#endif	/* WLBA */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_frmutil.h>
#include <wlc_ba.h>
#include <wl_export.h>

/* iovar table */
enum {
	IOV_BA,			/* enable block ack */
	IOV_BA_BSIZE,		/* ba window size */
	IOV_BA_BARFREQ,		/* freq of packets at which bar is sent */
	IOV_BA_CF_POLICY,	/* ctrl frame ack policy; 0:normal; 1:noack */
	IOV_BA_BAR_TIMEOUT,	/* timeout (in msec) to send bar */
	IOV_BA_DELBA_TIMEOUT,	/* timeout (in sec) to initiate delba */
	IOV_BA_INI_PKT_THRESH,	/* ma in pkts/sec to initiate addba */
	IOV_BA_COUNTERS,	/* retrieve ba counters */
	IOV_BA_CLEAR_COUNTERS,	/* clear ba counters */
	IOV_BA_SEND,		/* BAXXX: testing */
	IOV_BA_SIM		/* BAXXX: sim testing */
};

static const bcm_iovar_t ba_iovars[] = {
	{"ba", IOV_BA, (IOVF_SET_DOWN), IOVT_BOOL, 0},	/* only if we are down */
	{"ba_bsize", IOV_BA_BSIZE, (0), IOVT_UINT8, 0},
	{"ba_barfreq", IOV_BA_BARFREQ, (0), IOVT_UINT8, 0},
	{"ba_cf_policy", IOV_BA_CF_POLICY, (0), IOVT_UINT8, 0},
	{"ba_bar_timeout", IOV_BA_BAR_TIMEOUT, (0), IOVT_UINT16, 0},
	{"ba_delba_timeout", IOV_BA_DELBA_TIMEOUT, (0), IOVT_UINT16, 0},
	{"ba_ini_pkt_thresh", IOV_BA_INI_PKT_THRESH, (0), IOVT_UINT16, 0},
#ifdef BCMDBG
	{"ba_counters", IOV_BA_COUNTERS, (0), IOVT_BUFFER, sizeof(wlc_ba_cnt_t)},
	{"ba_clear_counters", IOV_BA_CLEAR_COUNTERS, 0, IOVT_VOID, 0},
	{"ba_send", IOV_BA_SEND, (0), IOVT_UINT8, 0},
	{"ba_sim", IOV_BA_SIM, (0), IOVT_UINT16, 0},
#endif
	{NULL, 0, 0, 0, 0}
};

#define BA_DEF_CTL_FRAME_POLICY	DOT11_BA_CTL_POLICY_NORMAL	/* noack */
#define BA_MAX_SCB_TID		(NUMPRIO)	/* max tid; currently 8; future 16 */
#define BA_MAX_BSIZE		64		/* Max buffer size */
#define BA_DEF_BSIZE		16		/* Default buffer size */
#define BA_DEF_BARFREQ		4		/* send BAR every barfreq frame */
#define BA_DEF_DELBA_TIMEOUT	15		/* in seconds */
#define BA_DEF_BAR_TIMEOUT	100		/* in msec */
#define BA_DEF_MA_WINDOW_SZ	8		/* window size */
#define BA_DEF_INI_PKT_THRESH	50		/* packet threshold to enable ba */

#define	BA_TID_STATE_BA_ON		0x01	/* block ack ON for tid */
#define	BA_TID_STATE_BA_PENDING_ON	0x02	/* block ack pending ON for tid */

#define SCB_BA_CUBBY(ba, scb) (scb_ba_t *)SCB_CUBBY((scb), (ba)->scb_handle)

/* BA module specific state */
struct ba_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	int scb_handle;		/* scb cubby handle to retrieve data from scb */
	uint8 cf_policy;	/* control frame policy: normal(0), no ack(1) */
	uint8 bsize;		/* default window size (in pdu) per tid */
	uint8 barfreq;		/* send a BAR every barfreq frame */
	uint8 ini_count;	/* number of tids enabled as ba initiator */
	uint16 delba_timeout;	/* timeout (in sec) to send delba if no traffic */
	uint16 bar_timeout;	/* timeout (in msec) to resend bar */
	uint32 ini_pkt_thresh;	/* total pkt cnt in window to initiate ba */
	struct wl_timer	*ba_bar_timer;	/* ba resend timer */
#ifdef WLCNT
	wlc_ba_cnt_t *cnt;	/* counters/stats */
#endif /* WLCNT */
};

/* structure to store per-tid state for the BA initiator */
typedef struct scb_ba_tid_ini {
	uint8 state;		/* ba state */
	uint8 bsize;		/* negotiated buffer size */
	uint8 tx_since_bar;	/* number of MPDUs transmitted since BAR sent */
	uint8 txpending;	/* number of pending MPDUs */
	uint8 ba_recd;		/* number of ba recd during the bar_timeout */
	uint16 delba_timeout;	/* negotiated timeout (in sec) to cancel addba */
	struct pktq txq;	/* buffered transmit queue when flow controlled */
	uint8 txpin;		/* tx pending q tail pointer */
	uint8 txpout;		/* tx pending q head pointer */
	void *txpq[BA_MAX_BSIZE];	/* tx pending queue */
	uint16 start_seq;	/* seqnum upto which all pkts have been acked */
	uint8 tx_bitmap[2*BA_MAX_BSIZE];	/* bitmap of transmitted pkts */
	uint8 bindex;		/* bitmap index of start_seq */
	uint32 used;		/* wlc->pub->now (in sec) when last used */
} scb_ba_tid_ini_t;

/* structure to store per-tid state for the BA responder */
typedef struct scb_ba_tid_resp {
	uint8 state;		/* ba state */
	uint8 bsize;		/* negotiated buffer size */
	uint16 delba_timeout;	/* negotiated timeout (in sec) to cancel addba */
	uint16 exp_seq;		/* next expected seqnum */
	uint16 start_seq;	/* seqnum upto which all pkts recd */
	uint8 rx_bitmap[2*BA_MAX_BSIZE];	/* bitmap of received pkts */
	uint8 bindex;		/* bitmap index of start_seq */
	uint8 brange;		/* bitmap index range of valid bits in bitmap */
	uint8 rxqlen;		/* length of rx reorder queue */
	void *rxqhead;		/* head of rx reorder queue */
	uint32 used;		/* wlc->pub->now (in sec) when last used */
} scb_ba_tid_resp_t;

/* structure to store per-tid moving average of packets sent to initiate addba */
typedef struct scb_ba_ma {
	uint16	window[BA_DEF_MA_WINDOW_SZ]; /* pkt count in each preceding slot in window */
	uint16	count;		/* dynamic count of pkts sent in current slot */
	uint32	total;		/* total count of pkts sent during entire window */
} scb_ba_ma_t;

/* scb cubby structure. ini and resp are dynamically allocated if ba is enabled *
 * for that tid. ma (moving average) is statically allocated since we always    *
 * need to compute it to decide if addba is to be initiated                     *
 */
typedef struct scb_ba {
	scb_ba_tid_ini_t	*ini[BA_MAX_SCB_TID];	/* initiator info */
	scb_ba_tid_resp_t	*resp[BA_MAX_SCB_TID];	/* responder info */
	scb_ba_ma_t		ma[BA_MAX_SCB_TID];	/* moving ave of pkts sent */
} scb_ba_t;


/* static inline functions */
static INLINE void
ba_ini_save_txp_pkt(ba_info_t *ba, scb_ba_tid_ini_t *ini, void *p)
{
	ASSERT(!(WLPKTTAG(p)->flags & WLF_BA));
	WLPKTTAG(p)->flags |= WLF_BA;
	ASSERT(ini->txpq[ini->txpin] == NULL);
	ini->txpq[ini->txpin] = p;
	ini->txpin = MODINC_POW2(ini->txpin, BA_MAX_BSIZE);
}

static INLINE void
ba_ini_free_txp_pkt(ba_info_t *ba, scb_ba_tid_ini_t *ini, uint16 count)
{
	void *p;
	for (; count > 0; count--) {
		p = ini->txpq[ini->txpout];
		ASSERT(p);
		if (!(WLPKTTAG(p)->flags & WLF_BA)) {
			PKTFREE(ba->wlc->osh, p, TRUE);
		} else {
			/* pkt will be freed in dotxstatus() */
			WL_BA(("wl%d: wlc_ba_recv_blockack: pkt on queue(not freeing)\n",
				ba->wlc->pub->unit));
		}
		ini->txpq[ini->txpout] = NULL;
		ini->txpout = MODINC_POW2(ini->txpout, BA_MAX_BSIZE);
	}
}

static INLINE void
ba_ini_resend_txp_pkt(ba_info_t *ba, struct scb *scb, scb_ba_tid_ini_t *ini, uint16 offset)
{
	void *p;
	uint index = MODADD_POW2(ini->txpout, offset, BA_MAX_BSIZE);
	p = ini->txpq[index];
	ASSERT(p);
	if (!(WLPKTTAG(p)->flags & WLF_BA)) {
		WL_BA(("wl%d: wlc_ba_recv_blockack: resending index %d\n",
			ba->wlc->pub->unit, index));

		WLPKTTAG(p)->flags |= WLF_BA;
		WLCNTINCR(ba->cnt->txretrans);
		SCB_TX_NEXT(TXMOD_BA, scb, p, WLC_PRIO_TO_HI_PREC(PKTPRIO(p)));
	} else {
		WL_BA(("wl%d: wlc_ba_recv_blockack: index %d already in queue\n",
			ba->wlc->pub->unit, index));
	}
}

static INLINE void
ba_ini_release_pdus(ba_info_t *ba, struct scb *scb, scb_ba_tid_ini_t *ini, uint8 tid)
{
	void *p;
	uint8 nfrags;
	while ((p = pktdeq(&ini->txq))) {
		ASSERT(PKTPRIO(p) == tid);
		nfrags = 1;	/* BAXXX: get nfrags from pkttag */
		if ((ini->txpending + nfrags) <= ini->bsize) {
			ini->txpending += nfrags;
			SCB_TX_NEXT(TXMOD_BA, scb, p, WLC_PRIO_TO_PREC(PKTPRIO(p)));
		} else {
			pktenq_head(&ini->txq, p);
			break;
		}
	}
}

static INLINE void
ba_ini_bump_start_seq(scb_ba_tid_ini_t *ini, uint16 i)
{
	ini->start_seq = MODADD_POW2(ini->start_seq, i, SEQNUM_MAX);
	if ((ini->bindex + i) <= BA_MAX_BSIZE)
		bzero(&ini->tx_bitmap[ini->bindex << 1], i << 1);
	else {
		bzero(&ini->tx_bitmap[ini->bindex << 1], (BA_MAX_BSIZE - ini->bindex) << 1);
		bzero(&ini->tx_bitmap[0], (i + ini->bindex - BA_MAX_BSIZE) << 1);
	}
	ini->bindex = MODADD_POW2(ini->bindex, i, BA_MAX_BSIZE);
}

static INLINE void
ba_resp_bump_start_seq(scb_ba_tid_resp_t *resp, uint16 i)
{
	resp->start_seq = MODADD_POW2(resp->start_seq, i, SEQNUM_MAX);
	if ((resp->bindex + i) <= BA_MAX_BSIZE)
		bzero(&resp->rx_bitmap[resp->bindex << 1], i << 1);
	else {
		bzero(&resp->rx_bitmap[resp->bindex << 1], (BA_MAX_BSIZE - resp->bindex) << 1);
		bzero(&resp->rx_bitmap[0], (i + resp->bindex - BA_MAX_BSIZE) << 1);
	}
	resp->bindex = MODADD_POW2(resp->bindex, i, BA_MAX_BSIZE);
}

static INLINE uint16
ba_bump_seqnum(uint16 seqnum, bool morefrag)
{
	if (morefrag)
		seqnum++;
	else {
		seqnum &= ~FRAGNUM_MASK;
		seqnum += (FRAGNUM_MASK + 1);
	}
	return seqnum;
}

static INLINE uint16
ba_pkt_seqnum(ba_info_t *ba, void *p)
{
	struct dot11_header *h;
	h = (struct dot11_header *)PKTDATA(ba->wlc->osh, p);
	return ltoh16(h->seq);
}

#define ba_rx_pktq_peek(resp) resp->rxqhead
#define ba_rx_pktq_len(resp) resp->rxqlen
static INLINE void
ba_rx_pktq_deq(scb_ba_tid_resp_t *resp)
{
	void *p = resp->rxqhead;
	ASSERT(p);
	ASSERT(resp->rxqlen);
	resp->rxqhead = PKTLINK(p);
	PKTSETLINK(p, NULL);
	resp->rxqlen--;
}
static INLINE void
ba_rx_pktq_enq(ba_info_t *ba, scb_ba_tid_resp_t *resp, void *p, uint16 seqenq)
{
	uint16 seqcur;
	void *prev, *cur;

	ASSERT(resp->rxqlen < resp->bsize);
	ASSERT(PKTLINK(p) == NULL);
	resp->rxqlen++;
	if (resp->rxqhead == NULL) {
		resp->rxqhead = p;
		return;
	}
	seqcur = ba_pkt_seqnum(ba, resp->rxqhead);
	/* if (seqcur > seqenq) */
	if (MODSUB_POW2(seqcur, seqenq, SEQNUM_MAX) <= (SEQNUM_MAX >> 1)) {
		PKTSETLINK(p, resp->rxqhead);
		resp->rxqhead = p;
		return;
	}
	prev = resp->rxqhead;
	cur = PKTLINK(resp->rxqhead);
	while (cur != NULL) {
		seqcur = ba_pkt_seqnum(ba, cur);
		ASSERT(seqcur != seqenq);
		/* if (seqcur > seqenq) */
		if (MODSUB_POW2(seqcur, seqenq, SEQNUM_MAX) <= (SEQNUM_MAX >> 1)) {
			break;
		}
		prev = PKTLINK(prev);
		cur = PKTLINK(cur);
	}
	/* insert after prev */
	ASSERT(PKTLINK(prev) == cur);
	PKTSETLINK(p, cur);
	PKTSETLINK(prev, p);
}

static INLINE void
ba_adjust_f(ba_info_t *ba, struct wlc_frminfo *f, void *p)
{
	f->p = p;
	f->h = (struct dot11_header *) PKTDATA(ba->wlc->osh, f->p);
	/* save seq in host endian */
	f->seq = ltoh16(f->h->seq);
	f->len = PKTLEN(ba->wlc->osh, f->p);
	f->pbody = (uchar*)(f->h) + DOT11_A3_HDR_LEN;
	f->body_len = f->len - DOT11_A3_HDR_LEN;
	if (f->wds) {
		f->pbody += ETHER_ADDR_LEN;
		f->body_len -= ETHER_ADDR_LEN;
	}
	/* WME: account for QoS Control Field */
	if (f->qos) {
		f->prio = (uint8)QOS_PRIO(ltoh16_ua(f->pbody));
		f->pbody += DOT11_QOS_LEN;
		f->body_len -= DOT11_QOS_LEN;
	}
	f->fc = ltoh16(f->h->fc);
	if (f->rx_wep) {
		/* strip WEP IV */
		f->pbody += f->key->iv_len;
		f->body_len -= f->key->iv_len;
	}
}

static INLINE uint8
wlc_ba_get_nbits(uint8 *bitmap, uint8 count)
{
	uint8 nbits;
	uint8 tmp;
	for (nbits = 0; count; count--) {
		tmp = *(bitmap);
		while (tmp) {
			nbits++;
			tmp >>= 1;
		}
		bitmap++;
	}
	return nbits;
}

#define BA_VALIDATE_TID(ba, tid, fnstr) \
	if (tid >= BA_MAX_SCB_TID) { \
		WL_BA(("wl%d: %s: invalid tid %d\n", \
			ba->wlc->pub->unit, fnstr, tid)); \
		WLCNTINCR(ba->cnt->rxunexp); \
		return; \
	}

/* local prototypes */
static int scb_ba_init(void *context, struct scb *scb);
static void scb_ba_deinit(void *context, struct scb *scb);
static int wlc_ba_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static int wlc_ba_watchdog(void *hdl);

static void wlc_ba_recv_addba_req(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len);
static void wlc_ba_recv_addba_resp(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len);
static void wlc_ba_recv_delba(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len);
static void wlc_ba_recv_blockack_req(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len);
static void wlc_ba_recv_blockack(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len);

static void wlc_ba_send_addba_resp(ba_info_t *ba, struct scb *scb, uint16 status, uint8 token,
        uint16 timeout, uint16 param_set);
static void wlc_ba_send_addba_req(ba_info_t *ba, struct scb *scb, uint8 tid);
static void wlc_ba_send_delba(ba_info_t *ba, struct scb *scb, uint8 tid, uint16 initiator,
	uint16 reason);
static void wlc_ba_send_blockack_req(ba_info_t *ba, struct scb *scb, uint8 tid,
	uint16 cf_policy, bool enq_only);
static void wlc_ba_send_blockack(ba_info_t *ba, struct scb *scb, uint8 tid,
	uint16 seqnum, uint16 cf_policy);
static void wlc_ba_bar_timer(void *arg);
static void ba_cleanup_tid_resp(ba_info_t *ba, scb_ba_t *scb_ba, uint8 tid);
static void ba_cleanup_tid_ini(ba_info_t *ba, scb_ba_t *scb_ba, uint8 tid);
static void wlc_ba_send_data_pkt(void *ctx, struct scb *scb, void *p, uint prec);
static uint wlc_ba_txpktcnt(void *ctx);
#ifdef BCMDBG
extern int wlc_ba_dump(ba_info_t *ba, struct bcmstrbuf *b);
#endif /* BCMDBG */

static txmod_fns_t ba_fns = {
	wlc_ba_send_data_pkt,
	wlc_ba_txpktcnt,
	NULL,
	NULL
};

ba_info_t *
BCMATTACHFN(wlc_ba_attach)(wlc_info_t *wlc)
{
	ba_info_t *ba;

	/* some code depends on packed structures */
	ASSERT(sizeof(struct dot11_bar) == DOT11_BAR_LEN);
	ASSERT(sizeof(struct dot11_ba) == (DOT11_BA_LEN + DOT11_BA_BITMAP_LEN));
	ASSERT(sizeof(struct dot11_addba_req) == DOT11_ADDBA_REQ_LEN);
	ASSERT(sizeof(struct dot11_addba_resp) == DOT11_ADDBA_RESP_LEN);
	ASSERT(sizeof(struct dot11_delba) == DOT11_DELBA_LEN);
	ASSERT(DOT11_MAXNUMFRAGS == NBITS(uint16));
	ASSERT(ISPOWEROF2(BA_MAX_BSIZE));

	if (!(ba = (ba_info_t *)MALLOC(wlc->osh, sizeof(ba_info_t)))) {
		WL_ERROR(("wl%d: wlc_ba_attach: out of mem, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}
	bzero((char *)ba, sizeof(ba_info_t));

#ifdef WLCNT
	if (!(ba->cnt = (wlc_ba_cnt_t *)MALLOC(wlc->osh, sizeof(wlc_ba_cnt_t)))) {
		WL_ERROR(("wl%d: wlc_ba_attach: out of mem, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		MFREE(wlc->osh, ba, sizeof(ba_info_t));
		return NULL;
	}
	bzero((char *)ba->cnt, sizeof(wlc_ba_cnt_t));
#endif /* WLCNT */

	ba->wlc = wlc;
	ba->cf_policy = BA_DEF_CTL_FRAME_POLICY;
	ba->bsize = BA_DEF_BSIZE;
	ba->barfreq = BA_DEF_BARFREQ;
	ba->delba_timeout = BA_DEF_DELBA_TIMEOUT;
	ba->bar_timeout = BA_DEF_BAR_TIMEOUT;
	ba->ini_pkt_thresh = BA_DEF_INI_PKT_THRESH * BA_DEF_MA_WINDOW_SZ;

	WLCNTSET(ba->cnt->version, WLC_BA_CNT_VERSION);
	WLCNTSET(ba->cnt->length, sizeof(ba->cnt));

	/* initialize timer */
	if (!(ba->ba_bar_timer = wl_init_timer(wlc->wl, wlc_ba_bar_timer, wlc, "ba_bar"))) {
		WL_ERROR(("wl%d: wlc_ba_attach: wl_init_timer for ba_bar_timer failed\n",
			wlc->pub->unit));
		wlc_ba_detach(ba);
		return NULL;
	}

	/* reserve cubby in the scb container for per-scb private data */
	ba->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(scb_ba_t),
		scb_ba_init, scb_ba_deinit, NULL, (void *)ba);
	if (ba->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_ba_attach: wlc_scb_cubby_reserve failed\n", wlc->pub->unit));
		wlc_ba_detach(ba);
		return NULL;
	}

	/* register txmod function */
	wlc_txmod_fn_register(wlc, TXMOD_BA, ba, ba_fns);

	/* register module */
	wlc_module_register(wlc->pub, ba_iovars, "ba", ba, wlc_ba_doiovar,
		wlc_ba_watchdog, NULL, NULL);

#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "ba", (dump_fn_t)wlc_ba_dump, (void *)ba);
#endif /* BCMDBG */

	return ba;
}

void
BCMATTACHFN(wlc_ba_detach)(ba_info_t *ba)
{
	if (!ba)
		return;

	ASSERT(ba->ini_count == 0);
	if (ba->ba_bar_timer) {
		wl_free_timer(ba->wlc->wl, ba->ba_bar_timer);
		ba->ba_bar_timer = NULL;
	}
	wlc_module_unregister(ba->wlc->pub, "ba", ba);

#ifdef WLCNT
	if (ba->cnt)
		MFREE(ba->wlc->osh, ba->cnt, sizeof(wlc_ba_cnt_t));
#endif /* WLCNT */
	MFREE(ba->wlc->osh, ba, sizeof(ba_info_t));
}

static int
scb_ba_init(void *context, struct scb *scb)
{
	return 0;
}

static void
scb_ba_deinit(void *context, struct scb *scb)
{
	ba_info_t *ba = (ba_info_t *)context;
	scb_ba_t *scb_ba = SCB_BA_CUBBY(ba, scb);
	uint8 tid;

	ASSERT(scb_ba);

	for (tid = 0; tid < BA_MAX_SCB_TID; tid++) {
		ba_cleanup_tid_resp(ba, scb_ba, tid);
		ba_cleanup_tid_ini(ba, scb_ba, tid);
	}
}

static void
wlc_ba_bar_timer(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	ba_info_t *ba = wlc->bastate;
	scb_ba_t *scb_ba;
	scb_ba_tid_ini_t *ini;
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 tid;

	ASSERT(ba);
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!SCB_BA(scb))
			continue;

		scb_ba = SCB_BA_CUBBY(ba, scb);
		ASSERT(scb_ba);
		for (tid = 0; tid < BA_MAX_SCB_TID; tid++) {
			ini = scb_ba->ini[tid];
			if ((ini == NULL) || (ini->state != BA_TID_STATE_BA_ON))
				continue;

			if (ini->txpending && !ini->ba_recd) {
				wlc_ba_send_blockack_req(ba, scb, tid,
					DOT11_BA_CTL_POLICY_NORMAL, FALSE);

				ini->tx_since_bar = 0;
				WLCNTINCR(ba->cnt->txbatimer);
			}
			ini->ba_recd = 0;
		}
	}
}

static int
wlc_ba_watchdog(void *hdl)
{
	ba_info_t *ba = (ba_info_t *)hdl;
	wlc_info_t *wlc = ba->wlc;
	scb_ba_t *scb_ba;
	scb_ba_tid_ini_t *ini;
	scb_ba_tid_resp_t *resp;
	scb_ba_ma_t *ma;
	struct scb *scb;
	struct scb_iter scbiter;
	uint8 tid;
	uint now = wlc->pub->now;
	uint idx = now % BA_DEF_MA_WINDOW_SZ;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!SCB_BA(scb))
			continue;

		scb_ba = SCB_BA_CUBBY(wlc->bastate, scb);
		ASSERT(scb_ba);
		for (tid = 0; tid < BA_MAX_SCB_TID; tid++) {
			if ((ini = scb_ba->ini[tid])) {
				/* resend addba if necessary */
				if (ini->state == BA_TID_STATE_BA_PENDING_ON) {
					ASSERT(scb_ba->ma[tid].count == 0);
					wlc_ba_send_addba_req(ba, scb, tid);
				}

				/* send delba from ini if timeout expired */
				if (ini->delba_timeout &&
					((now - ini->used) > ini->delba_timeout)) {
					wlc_ba_send_delba(ba, scb, tid, TRUE,
						DOT11_SC_FAILURE);
				}
			} else {
				/* update moving average */
				ma = &scb_ba->ma[tid];
				ma->total += ma->count - ma->window[idx];
				ma->window[idx] = ma->count;
				ma->count = 0;
				if (ma->total >= ba->ini_pkt_thresh) {
					wlc_ba_send_addba_req(ba, scb, tid);
				}
			}

			/* send delba from resp if timeout expired */
			if ((resp = scb_ba->resp[tid])) {
				if (resp->delba_timeout &&
					((now - resp->used) > resp->delba_timeout)) {
					wlc_ba_send_delba(ba, scb, tid, FALSE,
						DOT11_SC_FAILURE);
				}
			}
		}
	}
	return 0;
}

/* handle BA related iovars */
static int
wlc_ba_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	ba_info_t *ba = (ba_info_t *)hdl;
	int32 int_val = 0;
	bool bool_val;
	int err = 0;
	wlc_info_t *wlc;
	int32 *ret_int_ptr = (int32 *)a;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	wlc = ba->wlc;
	ASSERT(ba == wlc->bastate);

	switch (actionid) {
	case IOV_GVAL(IOV_BA):
		*ret_int_ptr = (int32)wlc->pub->_ba;
		break;

	case IOV_SVAL(IOV_BA):
		return wlc_ba(ba, bool_val);

#ifdef BCMDBG

#ifdef WLCNT
	case IOV_GVAL(IOV_BA_COUNTERS):
		bcopy(&ba->cnt, a, sizeof(ba->cnt));
		break;

	case IOV_SVAL(IOV_BA_CLEAR_COUNTERS):
		/* zero the counters but reinit the version and length */
		bzero(&ba->cnt, sizeof(ba->cnt));
		WLCNTSET(ba->cnt->version, WLC_BA_CNT_VERSION);
		WLCNTSET(ba->cnt->length, sizeof(ba->cnt));
		break;
#endif /* WLCNT */
#endif /* BCMDBG */

	case IOV_GVAL(IOV_BA_BSIZE):
		*ret_int_ptr = (int32)ba->bsize;
		break;

	case IOV_SVAL(IOV_BA_BSIZE):
		if ((int_val == 0) || (int_val > BA_MAX_BSIZE)) {
			err = BCME_BADARG;
			break;
		}
		ba->bsize = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_BA_BARFREQ):
		*ret_int_ptr = (int32)ba->barfreq;
		break;

	case IOV_SVAL(IOV_BA_BARFREQ):
		if ((int_val == 0) || (int_val > ba->bsize)) {
			err = BCME_BADARG;
			break;
		}
		ba->barfreq = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_BA_CF_POLICY):
		*ret_int_ptr = (int32)ba->cf_policy;
		break;

	case IOV_SVAL(IOV_BA_CF_POLICY):
		if ((int_val != DOT11_BA_CTL_POLICY_NORMAL) &&
		(int_val != DOT11_BA_CTL_POLICY_NOACK)) {
			err = BCME_BADARG;
			break;
		}
		ba->cf_policy = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_BA_BAR_TIMEOUT):
		*ret_int_ptr = (int32)ba->bar_timeout;
		break;

	case IOV_SVAL(IOV_BA_BAR_TIMEOUT):
		ba->bar_timeout = (uint16)int_val;
		/* rekick the timer if necessary */
		if (ba->ini_count) {
			wl_del_timer(ba->wlc->wl, ba->ba_bar_timer);
			wl_add_timer(ba->wlc->wl, ba->ba_bar_timer, ba->bar_timeout, TRUE);
		}
		break;

	case IOV_GVAL(IOV_BA_DELBA_TIMEOUT):
		*ret_int_ptr = (int32)ba->delba_timeout;
		break;

	case IOV_SVAL(IOV_BA_DELBA_TIMEOUT):
		ba->delba_timeout = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_BA_INI_PKT_THRESH):
		*ret_int_ptr = (int32)ba->ini_pkt_thresh / BA_DEF_MA_WINDOW_SZ;
		break;

	case IOV_SVAL(IOV_BA_INI_PKT_THRESH):
		ba->ini_pkt_thresh = int_val * BA_DEF_MA_WINDOW_SZ;
		break;

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

static uint wlc_ba_txpktcnt(void *ctx)
{
	/* Not implemented */
	return 0;
}

static void
wlc_ba_send_data_pkt(void *ctx, struct scb *scb, void *p, uint prec)
{
	ba_info_t *ba = (ba_info_t *)ctx;
	scb_ba_t *scb_ba;
	scb_ba_tid_ini_t *ini;
	uint8 tid, nfrags;

	ASSERT(ba);
	ASSERT(scb);

	if (!SCB_BA(scb)) {
		SCB_TX_NEXT(TXMOD_BA, scb, p, prec);
		return;
	}

	tid = (uint8)PKTPRIO(p);
	ASSERT(tid < BA_MAX_SCB_TID);

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	/* BAXXX: retrieve nfrags from pkttag once available */
	nfrags = 1;

	ASSERT((nfrags >= 1) && (nfrags <= DOT11_MAXNUMFRAGS));

	if (!(ini = scb_ba->ini[tid])) {
		SCB_TX_NEXT(TXMOD_BA, scb, p, prec);
		return;
	}

	/* continue buffering if buffered packets */
	if (!pktq_empty(&ini->txq)) {
		WL_BA(("wl%d: wlc_ba_send_data_pkt: buffered nfrags %d tid %d\n",
			ba->wlc->pub->unit, nfrags, tid));

		if (pktq_full(&ini->txq))
			goto toss;

		/* save it in the per-tid queue */
		pktenq(&ini->txq, p);

		WLCNTINCR(ba->cnt->txfc);
		return;
	}

	/* initiate buffering if peer will not have enough buffers */
	if ((ini->txpending + nfrags) > ini->bsize) {
		WL_BA(("wl%d: wlc_ba_send_data_pkt: initiate fc: nfrags %d tid %d\n",
			ba->wlc->pub->unit, nfrags, tid));

		/* save it in the per-tid queue */
		pktenq(&ini->txq, p);

		WLCNTINCR(ba->cnt->txfci);
		return;
	}

	ini->txpending += nfrags;

	SCB_TX_NEXT(TXMOD_BA, scb, p, prec);

	return;

toss:
	WL_BA(("wl%d: wlc_ba_send_data_pkt: txq overflow\n", ba->wlc->pub->unit));
	PKTFREE(ba->wlc->osh, p, TRUE);
	WLCNTINCR(ba->wlc->pub->_cnt->txnobuf);
	return;
}

void
wlc_ba_process_data_pkt(ba_info_t *ba, struct scb *scb, uint8 tid, void **pkts, uint nfrags)
{
	scb_ba_t *scb_ba;
	scb_ba_tid_ini_t *ini;
	uint8 i, index;
	uint16 seqnum, offset;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));
	ASSERT(tid < BA_MAX_SCB_TID);

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	ASSERT((nfrags >= 1) && (nfrags <= DOT11_MAXNUMFRAGS));

	ini = scb_ba->ini[tid];

	/* update stats needed to dynamically enable addba exchange */
	if (ini == NULL) {
		scb_ba->ma[tid].count += (uint16)nfrags;
		return;
	}

	ini->used = ba->wlc->pub->now;
	ini->tx_since_bar += (uint8)nfrags;

	seqnum = SCB_SEQNUM(scb, tid) & (SEQNUM_MAX - 1);

	WL_BA(("wl%d: wlc_ba_process_data_pkt: saving seq 0x%x\n",
		ba->wlc->pub->unit, seqnum));

	/* get the tx bitmap index */
	offset = MODSUB_POW2(seqnum, ini->start_seq, SEQNUM_MAX);
	ASSERT(offset < ini->bsize);
	index = MODADD_POW2(ini->bindex, offset, BA_MAX_BSIZE) << 1;

	for (i = 0; i < nfrags; i++) {
		/* set bit in transmit bitmap */
		ASSERT(!isset(&ini->tx_bitmap[index], i));
		setbit(&ini->tx_bitmap[index], i);

		/* save packet in pending queue */
		ba_ini_save_txp_pkt(ba, ini, pkts[i]);
	}

	/* send BAR every barfreq frame */
	if (ini->tx_since_bar >= ba->barfreq) {
		ini->tx_since_bar = 0;
		wlc_ba_send_blockack_req(ba, scb, tid, ba->cf_policy, TRUE);
	}

	WLCNTADD(ba->cnt->txpdu, nfrags);
	WLCNTINCR(ba->cnt->txsdu);

	return;
}

void
wlc_ba_dotxstatus(ba_info_t *ba, struct scb *scb, void *p, uint16 seq, bool *free_pdu)
{
	scb_ba_t *scb_ba;
	scb_ba_tid_ini_t *ini;

	ASSERT(ba);

	if (!scb || !SCB_BA(scb))
		return;

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	ASSERT(WLPKTTAG(p)->flags & WLF_BA);
	WLPKTTAG(p)->flags &= ~WLF_BA;

	ASSERT(PKTPRIO(p) < BA_MAX_SCB_TID);

	ini = scb_ba->ini[PKTPRIO(p)];

	/* return if BA not enabled on TID */
	if (ini == NULL)
		return;

	seq = ltoh16(seq) >> SEQNUM_SHIFT;

	if (MODSUB_POW2(seq, ini->start_seq, SEQNUM_MAX) < (SEQNUM_MAX >> 1))
		*free_pdu = FALSE;
}


void
wlc_ba_recvdata(ba_info_t *ba, struct scb *scb, struct wlc_frminfo *f)
{
	scb_ba_t *scb_ba;
	scb_ba_tid_resp_t *resp;
	uint8 index;
	void *p;
	uint16 seqnum, offset;

	if (f->subtype != FC_SUBTYPE_QOS_DATA) {
		wlc_recvdata_ordered(ba->wlc, scb, f);
		return;
	}

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));
	ASSERT(f->prio < BA_MAX_SCB_TID);
	ASSERT(!f->ismulti);

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	resp = scb_ba->resp[f->prio];

	/* return if BA not enabled on TID */
	if (resp == NULL) {
		wlc_recvdata_ordered(ba->wlc, scb, f);
		return;
	}

	WL_BA(("wl%d: wlc_ba_recvdata: receiving seq 0x%x\n",
		ba->wlc->pub->unit, f->seq));

	resp->used = ba->wlc->pub->now;
	WLCNTINCR(ba->cnt->rxpdu);

	if (ba_rx_pktq_len(resp) > (resp->bsize-1)) {
		WL_BA(("wl%d: wlc_ba_recvdata: out of buffer, dropping seq 0x%x\n",
			ba->wlc->pub->unit, f->seq));
		PKTFREE(ba->wlc->osh, f->p, FALSE);
		WLCNTINCR(ba->cnt->rxnobuf);
		return;
	}

	seqnum = f->seq >> SEQNUM_SHIFT;
	offset = MODSUB_POW2(seqnum, resp->start_seq, SEQNUM_MAX);
	if (offset >= BA_MAX_BSIZE) {
		WL_BA(("wl%d: wlc_ba_recvdata: unexp offset %d, seq 0x%x(dropped)\n",
			ba->wlc->pub->unit, offset, seqnum));
		PKTFREE(ba->wlc->osh, f->p, FALSE);
		WLCNTINCR(ba->cnt->rxunexp);
		return;
	}
	index = MODADD_POW2(resp->bindex, offset, BA_MAX_BSIZE) << 1;

	/* update range */
	resp->brange = MAX(resp->brange, offset + 1);

	if (!isset(&resp->rx_bitmap[index], f->seq & FRAGNUM_MASK))
		setbit(&resp->rx_bitmap[index], f->seq & FRAGNUM_MASK);
	else {
		WL_BA(("wl%d: wlc_ba_recvdata: duplicate seq 0x%x\n",
			ba->wlc->pub->unit, f->seq));
		PKTFREE(ba->wlc->osh, f->p, FALSE);
		WLCNTINCR(ba->cnt->rxdup);
		return;
	}

	/* send up if expected seqnum else enq it */
	if (f->seq == resp->exp_seq) {
		resp->exp_seq = ba_bump_seqnum(resp->exp_seq, f->fc & FC_MOREFRAG);
		wlc_recvdata_ordered(ba->wlc, scb, f);

		/* release pending ordered packets */
		while ((p = ba_rx_pktq_peek(resp))) {
			seqnum = ba_pkt_seqnum(ba, p);
			if (seqnum != resp->exp_seq)
				break;

			WL_BA(("wl%d: wlc_ba_recvdata: releasing seq 0x%x\n",
				ba->wlc->pub->unit, seqnum));

			/* adjust the fields of frminfo f */
			ba_adjust_f(ba, f, p);

			/* dequeue pkt */
			ba_rx_pktq_deq(resp);

			resp->exp_seq = ba_bump_seqnum(resp->exp_seq, f->fc & FC_MOREFRAG);
			wlc_recvdata_ordered(ba->wlc, scb, f);
		}
	} else {
		WL_BA(("wl%d: wlc_ba_recvdata: q out of order seq 0x%x(exp 0x%x)\n",
			ba->wlc->pub->unit, f->seq, resp->exp_seq));

		ba_rx_pktq_enq(ba, resp, f->p, f->seq);
		WLCNTINCR(ba->cnt->rxqed);
	}

	return;
}

void
wlc_frameaction_ba(ba_info_t *ba, struct scb *scb, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	uint8 action_id;

	ASSERT(body[0] == DOT11_ACTION_CAT_BLOCKACK);

	if (!scb || !SCB_BA(scb)) {
		WL_BA(("wl%d: wlc_frameaction_ba: BA not advertized by remote\n",
			ba->wlc->pub->unit));
		return;
	}

	if (body_len < 2)
		goto err;

	action_id = body[1];

	switch (action_id) {

	case DOT11_BA_ACTION_ADDBA_REQ:
		if (body_len < DOT11_ADDBA_REQ_LEN)
			goto err;
		wlc_ba_recv_addba_req(ba, scb, body, body_len);
		break;

	case DOT11_BA_ACTION_ADDBA_RESP:
		if (body_len < DOT11_ADDBA_RESP_LEN)
			goto err;
		wlc_ba_recv_addba_resp(ba, scb, body, body_len);
		break;

	case DOT11_BA_ACTION_DELBA:
		if (body_len < DOT11_DELBA_LEN)
			goto err;
		wlc_ba_recv_delba(ba, scb, body, body_len);
		break;

	default:
		WL_ERROR(("FC_ACTION: Invalid BA action id %d\n", action_id));
		goto err;
	}

	return;

err:
	WL_ERROR(("wl%d: wlc_frameaction_ba: recd invalid frame of length %d\n",
		ba->wlc->pub->unit, body_len));
	WLCNTINCR(ba->cnt->rxunexp);
	wlc_send_action_err(ba->wlc, hdr, body, body_len);
	return;
}

void
wlc_ba_recv_ctl(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len, uint16 fk)
{

	if (!scb || !SCB_BA(scb)) {
		WL_BA(("wl%d: wlc_ba_recv_ctl: BA not advertized by remote\n",
			ba->wlc->pub->unit));
		return;
	}

	if (fk == FC_BLOCKACK_REQ) {
		if (body_len < DOT11_BAR_LEN)
			goto err;
		wlc_ba_recv_blockack_req(ba, scb, body, body_len);
	} else if (fk == FC_BLOCKACK) {
		if (body_len < (DOT11_BA_LEN + DOT11_BA_BITMAP_LEN))
			goto err;
		wlc_ba_recv_blockack(ba, scb, body, body_len);
	} else {
		ASSERT(0);
	}

	return;

err:
	WL_BA(("wl%d: wlc_ba_recv_ctl: recd invalid frame of length %d\n",
		ba->wlc->pub->unit, body_len));
	WLCNTINCR(ba->cnt->rxunexp);
	return;
}

static void
ba_cleanup_tid_resp(ba_info_t *ba, scb_ba_t *scb_ba, uint8 tid)
{
	scb_ba_tid_resp_t *resp;
	void *p;

	if (scb_ba->resp[tid] == NULL)
		return;

	resp = scb_ba->resp[tid];
	while ((p = ba_rx_pktq_peek(resp))) {
		ba_rx_pktq_deq(resp);
		/* BAXXX: do we sendup or just free the packet here */
		PKTFREE(ba->wlc->osh, p, FALSE);

	}
	ASSERT(ba_rx_pktq_len(resp) == 0);

	MFREE(ba->wlc->osh, resp, sizeof(scb_ba_tid_resp_t));
	scb_ba->resp[tid] = NULL;
}

static void
ba_cleanup_tid_ini(ba_info_t *ba, scb_ba_t *scb_ba, uint8 tid)
{
	scb_ba_tid_ini_t *ini;
	uint8 count, i;

	if (scb_ba->ini[tid] == NULL)
		return;

	ini = scb_ba->ini[tid];

	/* free all tx pending packets */
	count = MODSUB_POW2(ini->txpin, ini->txpout, BA_MAX_BSIZE);
	ba_ini_free_txp_pkt(ba, ini, count);
	ASSERT(ini->txpout == ini->txpin);
	for (i = 0; i < BA_MAX_BSIZE; i++)
		ASSERT(ini->txpq[i] == NULL);

	/* free all buffered tx packets */
	pktq_flush(ba->wlc->osh, &ini->txq, TRUE, NULL, 0);

	MFREE(ba->wlc->osh, ini, sizeof(scb_ba_tid_ini_t));
	scb_ba->ini[tid] = NULL;

	/* stop periodic timeout if necessary */
	ASSERT(ba->ini_count);
	ba->ini_count--;
	/* BAXXX: global stuff should be done elsewhere */
	if (ba->ini_count == 0)
		wl_del_timer(ba->wlc->wl, ba->ba_bar_timer);
}

static void
wlc_ba_recv_addba_req(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len)
{
	scb_ba_t *scb_ba;
	dot11_addba_req_t *addba_req;
	scb_ba_tid_resp_t *resp;
	uint16 param_set, timeout, start_seqnum;
	uint8 tid, bsize;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	addba_req = (dot11_addba_req_t *)body;

	timeout = ltoh16_ua(&addba_req->timeout);
	start_seqnum = ltoh16_ua(&addba_req->start_seqnum);
	param_set = ltoh16_ua(&addba_req->addba_param_set);

	/* accept the min of our and remote timeout */
	timeout = MIN(timeout, ba->delba_timeout);

	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	BA_VALIDATE_TID(ba, tid, "wlc_ba_recv_addba_req");

	bsize =	(param_set & DOT11_ADDBA_PARAM_BSIZE_MASK) >> DOT11_ADDBA_PARAM_BSIZE_SHIFT;
	/* accept the min of our and remote bsize */
	bsize = MIN(bsize, ba->bsize);

	param_set &= ~DOT11_ADDBA_PARAM_BSIZE_MASK;
	param_set |= (bsize << DOT11_ADDBA_PARAM_BSIZE_SHIFT) & DOT11_ADDBA_PARAM_BSIZE_MASK;

	/* cleanup old state */
	ba_cleanup_tid_resp(ba, scb_ba, tid);

	ASSERT(scb_ba->resp[tid] == NULL);

	resp = MALLOC(ba->wlc->osh, sizeof(scb_ba_tid_resp_t));

	if (resp == NULL) {
		wlc_ba_send_addba_resp(ba, scb, DOT11_SC_FAILURE, addba_req->token,
			timeout, param_set);
		return;
	}
	bzero((char *)resp, sizeof(scb_ba_tid_resp_t));

	WL_BA(("wl%d: wlc_ba_recv_addba_req: BA ON: seqnum 0x%x tid %d bsize %d\n",
		ba->wlc->pub->unit, start_seqnum, tid, bsize));

	scb_ba->resp[tid] = resp;
	resp->start_seq = start_seqnum;
	resp->exp_seq = start_seqnum << SEQNUM_SHIFT;
	bzero(resp->rx_bitmap, sizeof(resp->rx_bitmap));
	resp->bindex = 0;
	resp->bsize = bsize;
	resp->delba_timeout = timeout;
	resp->state = BA_TID_STATE_BA_ON;
	resp->used = ba->wlc->pub->now;

	WLCNTINCR(ba->cnt->rxaddbareq);

	wlc_ba_send_addba_resp(ba, scb, DOT11_SC_SUCCESS, addba_req->token, timeout, param_set);

	return;
}

static void
wlc_ba_recv_addba_resp(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len)
{
	scb_ba_t *scb_ba;
	dot11_addba_resp_t *addba_resp;
	scb_ba_tid_ini_t *ini;
	uint16 param_set, timeout, status;
	uint8 tid, i, bsize;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	addba_resp = (dot11_addba_resp_t *)body;

	status = ltoh16_ua(&addba_resp->status);
	timeout = ltoh16_ua(&addba_resp->timeout);
	param_set = ltoh16_ua(&addba_resp->addba_param_set);

	bsize =	(param_set & DOT11_ADDBA_PARAM_BSIZE_MASK) >> DOT11_ADDBA_PARAM_BSIZE_SHIFT;
	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	BA_VALIDATE_TID(ba, tid, "wlc_ba_recv_addba_resp");

	ini = scb_ba->ini[tid];

	if (ini == NULL) {
		WL_BA(("wl%d: wlc_ba_recv_addba_resp: Unexpected packet\n",
			ba->wlc->pub->unit));
		WLCNTINCR(ba->cnt->rxunexp);
		return;
	}

	/* cleanup ma details */
	scb_ba->ma[tid].total = 0;
	for (i = 0; i < BA_DEF_MA_WINDOW_SZ; i++)
		scb_ba->ma[tid].window[i] = 0;
	ini->used = ba->wlc->pub->now;

	if (status != DOT11_SC_SUCCESS) {
		WL_BA(("wl%d: wlc_ba_recv_addba_resp: Failed. status %d\n",
			ba->wlc->pub->unit, status));
		ba_cleanup_tid_ini(ba, scb_ba, tid);
		WLCNTINCR(ba->cnt->rxunexp);
		return;
	}

	if ((bsize > ba->bsize) || (timeout > ba->delba_timeout)) {
		WL_BA(("wl%d: wlc_ba_recv_addba_resp: Failed. bsize %d timeout %d\n",
			ba->wlc->pub->unit, bsize, timeout));
		ba_cleanup_tid_ini(ba, scb_ba, tid);
		WLCNTINCR(ba->cnt->rxunexp);
		return;
	}

	ini->bsize = bsize;
	ini->delba_timeout = timeout;
	ini->state = BA_TID_STATE_BA_ON;

	WLCNTINCR(ba->cnt->rxaddbaresp);

	WL_BA(("wl%d: wlc_ba_recv_addba_resp: Turning BA ON: tid %d bsize %d\n",
		ba->wlc->pub->unit, tid, bsize));

	return;
}

static void
wlc_ba_recv_delba(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len)
{
	scb_ba_t *scb_ba;
	dot11_delba_t *delba;
	uint16 param_set;
	uint8 tid;
	uint16 reason, initiator;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	delba = (dot11_delba_t *)body;

	param_set = ltoh16(delba->delba_param_set);
	reason = ltoh16(delba->reason);

	tid = (param_set & DOT11_DELBA_PARAM_TID_MASK) >> DOT11_DELBA_PARAM_TID_SHIFT;
	BA_VALIDATE_TID(ba, tid, "wlc_ba_recv_delba");

	initiator = (param_set & DOT11_DELBA_PARAM_INIT_MASK) >> DOT11_DELBA_PARAM_INIT_SHIFT;

	if (initiator)
		ba_cleanup_tid_resp(ba, scb_ba, tid);
	else
		ba_cleanup_tid_ini(ba, scb_ba, tid);

	WLCNTINCR(ba->cnt->rxdelba);

	WL_BA(("wl%d: wlc_ba_recv_delba: BA OFF: tid %d initiator %d reason %d\n",
		ba->wlc->pub->unit, tid, initiator, reason));

	return;
}

static void
wlc_ba_recv_blockack_req(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len)
{
	scb_ba_t *scb_ba;
	struct dot11_bar *ba_req;
	scb_ba_tid_resp_t *resp;
	uint8 tid;
	uint16 seqnum, tmp, cf_policy, delta;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	ba_req = (struct dot11_bar *)body;

	tmp = ltoh16(ba_req->bar_control);
	tid = (tmp & DOT11_BA_CTL_TID_MASK) >> DOT11_BA_CTL_TID_SHIFT;
	BA_VALIDATE_TID(ba, tid, "wlc_ba_recv_blockack_req");
	cf_policy = tmp & DOT11_BA_CTL_POLICY_MASK;

	seqnum = (ltoh16(ba_req->seqnum)) >> SEQNUM_SHIFT;

	resp = scb_ba->resp[tid];
	if (resp == NULL) {
		WL_BA(("wl%d: wlc_ba_recv_blockack_req: uninitialized tid %d\n",
			ba->wlc->pub->unit, tid));
		WLCNTINCR(ba->cnt->rxunexp);
		return;
	}

	WL_BA(("wl%d: wlc_ba_recv_blockack_req: length %d tid %d seqnum 0x%x\n",
		ba->wlc->pub->unit, body_len, tid, seqnum));

	WLCNTINCR(ba->cnt->rxbar);

	if (seqnum != resp->start_seq) {
		/* BAXXX: send up all packets till seqnum */
		WL_BA(("wl%d: wlc_ba_recv_blockack_req: seqnum progressed to 0x%x\n",
			ba->wlc->pub->unit, seqnum));

		delta = MODSUB_POW2(seqnum, resp->start_seq, SEQNUM_MAX);
		if (delta <= resp->brange) {
			ba_resp_bump_start_seq(resp, delta);
			resp->brange -= delta;
		} else {
			WL_BA(("wl%d: wlc_ba_recv_blockack_req: unexp seq jump %d\n",
				ba->wlc->pub->unit, delta));
			WLCNTINCR(ba->cnt->rxunexp);
			return;
		}
	}

	/* respond with BA */
	wlc_ba_send_blockack(ba, scb, tid, seqnum, cf_policy);

	return;
}

static void
wlc_ba_recv_blockack(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len)
{
	scb_ba_t *scb_ba;
	struct dot11_ba *ba_frame;
	scb_ba_tid_ini_t *ini;
	uint16 seqnum, tmp;
	uint8 *ack_bitmap, npdus = 0, nsdus, tid, i, index, offset, delta, j, range;
	bool holes = FALSE;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	ba_frame = (struct dot11_ba *)body;

	tmp = ltoh16(ba_frame->ba_control);
	tid = (tmp & DOT11_BA_CTL_TID_MASK) >> DOT11_BA_CTL_TID_SHIFT;
	BA_VALIDATE_TID(ba, tid, "wlc_ba_recv_blockack");
	seqnum = (ltoh16(ba_frame->seqnum)) >> SEQNUM_SHIFT;

	ini = scb_ba->ini[tid];
	if (ini == NULL) {
		WL_BA(("wl%d: wlc_ba_recv_blockack_req: uninitialized tid %d\n",
			ba->wlc->pub->unit, tid));
		WLCNTINCR(ba->cnt->rxunexp);
		return;
	}

	WL_BA(("wl%d: wlc_ba_recv_blockack: length %d tid %d seqnum 0x%x\n",
		ba->wlc->pub->unit, body_len, tid, seqnum));

	WLCNTINCR(ba->cnt->rxba);

	if (tmp & DOT11_BA_CTL_COMPRESSED)
		range = ((tmp & DOT11_BA_CTL_NUMMSDU_MASK) >> DOT11_BA_CTL_NUMMSDU_SHIFT) + 1;
	else
		range = ini->bsize;

	delta = MODSUB_POW2(ini->start_seq, seqnum, SEQNUM_MAX);

	if ((delta >= ini->bsize) || (range <= delta)) {
		WL_BA(("wl%d: wlc_ba_recv_blockack: inv ba: out of range seqnum 0x%x"
			" start_seq 0x%x range %d\n",
			ba->wlc->pub->unit, seqnum, ini->start_seq, range));
		WLCNTINCR(ba->cnt->rxinvba);
		return;
	}

	ini->ba_recd++;
	range -= delta;
	index = ini->bindex << 1;
	ack_bitmap = &ba_frame->bitmap[delta<<1];

	/* fast path: all matching bits */
	if (((ini->bindex + range) <= BA_MAX_BSIZE) &&
		!bcmp(ack_bitmap, &ini->tx_bitmap[index], range << 1)) {
		npdus += wlc_ba_get_nbits(ack_bitmap, range << 1);
		nsdus = range;
		goto fastpath;
	}

	/* byte by byte compare for initial matching */
	for (i = 0;
		(i < range) && ini->tx_bitmap[index];
		i++, ack_bitmap += 2, index = MODADD_POW2(index, 2, 2*BA_MAX_BSIZE)) {

		if (bcmp(ack_bitmap, &ini->tx_bitmap[index], 2))
			break;

		npdus += wlc_ba_get_nbits(ack_bitmap, 2);
	}

	nsdus = i;

	/* handle holes in bitmask */
	for (offset = npdus;
		(i < range) && ini->tx_bitmap[index];
		i++, ack_bitmap += 2, index = MODADD_POW2(index, 2, 2*BA_MAX_BSIZE)) {

		for (j = 0; j < 2*NBBY; j++) {
			if (isset(&ini->tx_bitmap[index], j)) {
				if (isclr(ack_bitmap, j)) {
					ba_ini_resend_txp_pkt(ba, scb, ini, offset);
					ini->tx_since_bar++;
					holes = TRUE;
				}
				offset++;
			} else {
				break;
			}
		}
	}

	/* send BAR every bar_count frame */
	if (ini->tx_since_bar >= ba->barfreq) {
		ini->tx_since_bar = 0;
		wlc_ba_send_blockack_req(ba, scb, tid, ba->cf_policy, TRUE);
	}

	if (holes)
		WLCNTINCR(ba->cnt->rxbaholes);

fastpath:

	/* handle positively acknowledged sdus */
	WL_BA(("wl%d: wlc_ba_recv_blockack: positive ack for %d sdus %d pdus: "
		"range %d delta %d\n",
		ba->wlc->pub->unit, nsdus, npdus, range, delta));

	ASSERT(npdus <= ini->txpending);
	ini->txpending -= npdus;

	/* free tx pending pkts that were acknowledged */
	ba_ini_free_txp_pkt(ba, ini, npdus);

	/* release some buffered pdus */
	ba_ini_release_pdus(ba, scb, ini, tid);

	/* bump up the start seqnum */
	ba_ini_bump_start_seq(ini, nsdus);

	return;
}

static void
wlc_ba_send_addba_req(ba_info_t *ba, struct scb *scb, uint8 tid)
{
	scb_ba_t *scb_ba;
	dot11_addba_req_t *addba_req;
	scb_ba_tid_ini_t *ini;
	uint16 tmp;
	void *p;
	uint8 *pbody;
	int prec;
	void *head_pkt, *pkt;
	wlc_info_t *wlc;
	struct pktq *q;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(SCB_BA(scb));
	ASSERT(tid < BA_MAX_SCB_TID);

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	p = wlc_frame_get_action(ba->wlc, FC_ACTION, &scb->ea, &scb->bsscfg->cur_etheraddr,
		&scb->bsscfg->BSSID, DOT11_ADDBA_REQ_LEN, &pbody, DOT11_ACTION_CAT_BLOCKACK);
	if (p == NULL)
		return;

	/* cleanup old state */
	ba_cleanup_tid_ini(ba, scb_ba, tid);

	ASSERT(scb_ba->ini[tid] == NULL);

	ini = MALLOC(ba->wlc->osh, sizeof(scb_ba_tid_ini_t));

	if (ini == NULL)
		return;

	bzero((char *)ini, sizeof(scb_ba_tid_ini_t));

	scb_ba->ini[tid] = ini;
	ini->start_seq = (SCB_SEQNUM(scb, tid) & (SEQNUM_MAX - 1));
	ini->txpending = 0;
	ini->bsize = ba->bsize;
	ini->state = BA_TID_STATE_BA_PENDING_ON;
	ini->used = ba->wlc->pub->now;
	pktq_init(&ini->txq, 1, PKTQ_LEN_DEFAULT);

	/* start periodic timeout */
	if (ba->ini_count++ == 0) {
		wl_add_timer(ba->wlc->wl, ba->ba_bar_timer, ba->bar_timeout, TRUE);
	}

	/* count number of packets already released into this scb's associated txq
	 * to keep txpending in sync
	 */
	q = &(SCB_WLCIFP(scb)->qi->q);
	wlc = ba->wlc;
	prec = WLC_PRIO_TO_PREC(tid);
	head_pkt = NULL;
	while (pktq_ppeek(q, prec) != head_pkt) {
		pkt = pktq_pdeq(q, prec);
		if (!head_pkt)
			head_pkt = pkt;
		if ((WLPKTTAGSCB(pkt) == scb) && (PKTPRIO(pkt) == tid)) {
			if (ini->txpending < ba->bsize)
				ini->txpending++;
			else {
				PKTFREE(wlc->osh, pkt, TRUE);
				WLCNTINCR(ba->cnt->txdrop);
				continue;
			}
		}
		pktq_penq(q, prec, pkt);
	}

	ASSERT(ini->txpending <= ba->bsize);

	addba_req = (dot11_addba_req_t *)pbody;
	addba_req->category = DOT11_ACTION_CAT_BLOCKACK;
	addba_req->action = DOT11_BA_ACTION_ADDBA_REQ;
	addba_req->token = (uint8)ba->wlc->counter;
	tmp = ((tid << DOT11_ADDBA_PARAM_TID_SHIFT) & DOT11_ADDBA_PARAM_TID_MASK) |
		((ba->bsize << DOT11_ADDBA_PARAM_BSIZE_SHIFT) & DOT11_ADDBA_PARAM_BSIZE_MASK);
	htol16_ua_store(tmp, (uint8 *)&addba_req->addba_param_set);
	htol16_ua_store(ba->delba_timeout, (uint8 *)&addba_req->timeout);
	htol16_ua_store(ini->start_seq, (uint8 *)&addba_req->start_seqnum);

	WL_BA(("wl%d: wlc_ba_send_addba_req: seqnum 0x%x tid %d bsize %d pending %d\n",
		ba->wlc->pub->unit, ini->start_seq, tid, ba->bsize, ini->txpending));

	WLCNTINCR(ba->cnt->txaddbareq);

	/* set same priority as tid */
	PKTSETPRIO(p, tid);

	wlc_sendmgmt(ba->wlc, p, SCB_WLCIFP(scb)->qi, scb);
}

static void
wlc_ba_send_addba_resp(ba_info_t *ba, struct scb *scb, uint16 status, uint8 token,
	uint16 timeout, uint16 param_set)
{
	scb_ba_t *scb_ba;
	dot11_addba_resp_t *addba_resp;
	void *p;
	uint8 *pbody;
	uint16 tid;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(SCB_BA(scb));

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	p = wlc_frame_get_action(ba->wlc, FC_ACTION, &scb->ea, &scb->bsscfg->cur_etheraddr,
		&scb->bsscfg->BSSID, DOT11_ADDBA_RESP_LEN, &pbody, DOT11_ACTION_CAT_BLOCKACK);
	if (p == NULL)
		return;

	addba_resp = (dot11_addba_resp_t *)pbody;
	addba_resp->category = DOT11_ACTION_CAT_BLOCKACK;
	addba_resp->action = DOT11_BA_ACTION_ADDBA_RESP;
	addba_resp->token = token;
	htol16_ua_store(status, (uint8 *)&addba_resp->status);
	htol16_ua_store(param_set, (uint8 *)&addba_resp->addba_param_set);
	htol16_ua_store(timeout, (uint8 *)&addba_resp->timeout);

	WL_BA(("wl%d: wlc_ba_send_addba_resp: status %d param_set 0x%x\n",
		ba->wlc->pub->unit, status, param_set));

	WLCNTINCR(ba->cnt->txaddbaresp);

	/* set same priority as tid */
	tid = (param_set & DOT11_ADDBA_PARAM_TID_MASK) >> DOT11_ADDBA_PARAM_TID_SHIFT;
	PKTSETPRIO(p, tid);

	wlc_sendmgmt(ba->wlc, p, SCB_WLCIFP(scb)->qi, scb);
}

static void
wlc_ba_send_delba(ba_info_t *ba, struct scb *scb, uint8 tid, uint16 initiator, uint16 reason)
{
	scb_ba_t *scb_ba;
	dot11_delba_t *delba;
	uint16 tmp;
	void *p;
	uint8 *pbody;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(SCB_BA(scb));
	ASSERT(tid < BA_MAX_SCB_TID);

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	p = wlc_frame_get_action(ba->wlc, FC_ACTION, &scb->ea, &scb->bsscfg->cur_etheraddr,
		&scb->bsscfg->BSSID, DOT11_DELBA_LEN, &pbody, DOT11_ACTION_CAT_BLOCKACK);
	if (p == NULL)
		return;

	/* cleanup state */
	if (initiator)
		ba_cleanup_tid_ini(ba, scb_ba, tid);
	else
		ba_cleanup_tid_resp(ba, scb_ba, tid);

	delba = (dot11_delba_t *)pbody;
	delba->category = DOT11_ACTION_CAT_BLOCKACK;
	delba->action = DOT11_BA_ACTION_DELBA;
	tmp = ((tid << DOT11_DELBA_PARAM_TID_SHIFT) & DOT11_DELBA_PARAM_TID_MASK) |
		((initiator << DOT11_DELBA_PARAM_INIT_SHIFT) & DOT11_DELBA_PARAM_INIT_MASK);
	delba->delba_param_set = htol16(tmp);
	delba->reason = htol16(reason);

	WL_BA(("wl%d: wlc_ba_send_delba: tid %d initiator %d reason %d\n",
		ba->wlc->pub->unit, tid, initiator, reason));

	WLCNTINCR(ba->cnt->txdelba);

	/* set same priority as tid */
	PKTSETPRIO(p, tid);

	wlc_sendmgmt(ba->wlc, p, SCB_WLCIFP(scb)->qi, scb);
}

static void
wlc_ba_send_blockack_req(ba_info_t *ba, struct scb *scb, uint8 tid, uint16 cf_policy,
	bool enq_only)
{
	scb_ba_t *scb_ba;
	struct dot11_ctl_header *hdr;
	struct dot11_bar *ba_req;
	scb_ba_tid_ini_t *ini;
	osl_t *osh;
	void *p;
	uint16 tmp;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));
	ASSERT(tid < BA_MAX_SCB_TID);

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	osh = ba->wlc->osh;

	ini = scb_ba->ini[tid];

	/* bail out/assert if incorrect state */
	if (ini == NULL) {
		ASSERT(0);
		return;
	}

	p = wlc_frame_get_ctl(ba->wlc, DOT11_CTL_HDR_LEN + DOT11_BAR_LEN);
	if (p == NULL)
		return;

	hdr = (struct dot11_ctl_header *)PKTDATA(osh, p);
	hdr->fc = htol16(FC_BLOCKACK_REQ);
	hdr->durid = 0;
	bcopy(&scb->ea, &hdr->ra, ETHER_ADDR_LEN);
	bcopy(&ba->wlc->pub->cur_etheraddr, &hdr->ta, ETHER_ADDR_LEN);

	ba_req = (struct dot11_bar *)&hdr[1];
	tmp = tid << DOT11_BA_CTL_TID_SHIFT;
	tmp |= (cf_policy & DOT11_BA_CTL_POLICY_MASK);
	ba_req->bar_control = htol16(tmp);
	ba_req->seqnum = htol16(ini->start_seq << SEQNUM_SHIFT);

	WL_BA(("wl%d: wlc_ba_send_blockack_req: seqnum 0x%x tid %d\n",
		ba->wlc->pub->unit, ini->start_seq, tid));

	if (ba->cf_policy & DOT11_BA_CTL_POLICY_NOACK)
		WLPKTTAG(p)->flags |= WLF_BA;

	WLCNTINCR(ba->cnt->txbar);

	/* set same priority as tid */
	PKTSETPRIO(p, tid);

	wlc_sendctl(ba->wlc, p, SCB_WLCIFP(scb)->qi, scb, TX_CTL_FIFO, 0, enq_only);
}

static void
wlc_ba_send_blockack(ba_info_t *ba, struct scb *scb, uint8 tid, uint16 seqnum,
	uint16 cf_policy)
{
	scb_ba_t *scb_ba;
	struct dot11_ctl_header *hdr;
	struct dot11_ba *ba_frame;
	scb_ba_tid_resp_t *resp;
	osl_t *osh;
	void *p;
	uint16 tmp;

	ASSERT(ba);
	ASSERT(scb);
	ASSERT(SCB_BA(scb));
	ASSERT(tid < BA_MAX_SCB_TID);

	scb_ba = SCB_BA_CUBBY(ba, scb);
	ASSERT(scb_ba);

	osh = ba->wlc->osh;

	resp = scb_ba->resp[tid];

	/* bail out/assert if incorrect state */
	if (resp == NULL) {
		ASSERT(0);
		return;
	}

	ASSERT(resp->state == BA_TID_STATE_BA_ON);

	p = wlc_frame_get_ctl(ba->wlc, DOT11_CTL_HDR_LEN + DOT11_BAR_LEN + DOT11_BA_BITMAP_LEN);
	if (p == NULL)
		return;

	hdr = (struct dot11_ctl_header *)PKTDATA(osh, p);
	hdr->fc = htol16(FC_BLOCKACK);
	hdr->durid = 0;
	bcopy(&scb->ea, &hdr->ra, ETHER_ADDR_LEN);
	bcopy(&ba->wlc->pub->cur_etheraddr, &hdr->ta, ETHER_ADDR_LEN);

	ba_frame = (struct dot11_ba *)&hdr[1];
	tmp = tid << DOT11_BA_CTL_TID_SHIFT;
	tmp |= (cf_policy & DOT11_BA_CTL_POLICY_MASK);

	/* compressed frag frame support */
	tmp |= DOT11_BA_CTL_COMPRESSED;
	ASSERT(resp->brange <= resp->bsize);
	if (resp->brange) {
		tmp |= ((resp->brange - 1) << DOT11_BA_CTL_NUMMSDU_SHIFT) &
			DOT11_BA_CTL_NUMMSDU_MASK;
	} else {
		bzero(&ba_frame->bitmap[0], 2);
	}

	ba_frame->ba_control = htol16(tmp);
	ba_frame->seqnum = htol16(resp->start_seq << SEQNUM_SHIFT);

	WL_BA(("wl%d: wlc_ba_send_blockack: seqnum 0x%x tid %d\n",
		ba->wlc->pub->unit, resp->start_seq, tid));

	if (cf_policy & DOT11_BA_CTL_POLICY_NOACK)
		WLPKTTAG(p)->flags |= WLF_BA;

	if ((resp->bindex + resp->brange) <= BA_MAX_BSIZE)
		bcopy(&resp->rx_bitmap[resp->bindex << 1], &ba_frame->bitmap[0],
			resp->brange << 1);
	else {
		bcopy(&resp->rx_bitmap[resp->bindex << 1], &ba_frame->bitmap[0],
			(BA_MAX_BSIZE - resp->bindex) << 1);
		bcopy(&resp->rx_bitmap[0], &ba_frame->bitmap[(BA_MAX_BSIZE - resp->bindex) << 1],
			(resp->brange + resp->bindex - BA_MAX_BSIZE) << 1);
	}

	WLCNTINCR(ba->cnt->txba);

	/* set same priority as tid */
	PKTSETPRIO(p, tid);

	wlc_sendctl(ba->wlc, p, SCB_WLCIFP(scb)->qi, scb, TX_CTL_FIFO, 0, FALSE);
}

int
wlc_ba(ba_info_t *ba, bool on)
{
	wlc_info_t *wlc = ba->wlc;

	if (on) {
		if (!wlc_ba_cap(ba)) {
			WL_ERROR(("wl%d: device not blockack capable\n", wlc->pub->unit));
			return BCME_UNSUPPORTED;
		}
#ifdef WLAFTERBURNER
		if (wlc->afterburner) {
			WL_ERROR(("wl%d: driver is afterburner enabled\n", wlc->pub->unit));
			return BCME_UNSUPPORTED;
		}
#endif /* WLAFTERBURNER */
	}

	wlc->pub->_ba = on;

	return 0;
}

bool
wlc_ba_cap(ba_info_t *ba)
{
	/* chip/boardflags check if needed */
	/* BAXXX: currently all chips */

	return (TRUE);
}

#ifdef BCMDBG
static int
wlc_ba_dump(ba_info_t *ba, struct bcmstrbuf *b)
{
	wlc_ba_cnt_t *cnt = &ba->cnt;

	bcm_bprintf(b, "ba_txpdu %d ba_txsdu %d ba_txretrans %d ba_txfc %d ba_txfci %d\n",
		cnt->txpdu, cnt->txsdu, cnt->txretrans, cnt->txfc, cnt->txfci);
	bcm_bprintf(b, "ba_txba %d ba_txbar %d ba_txbatimer %d ba_txdrop %d\n",
		cnt->txba, cnt->txbar, cnt->txbatimer, cnt->txdrop);
	bcm_bprintf(b, "ba_txaddbareq %d ba_txaddbaresp %d ba_txdelba %d\n",
		cnt->txaddbareq, cnt->txaddbaresp, cnt->txdelba);

	bcm_bprintf(b, "ba_rxpdu %d ba_rxqed %d ba_rxdup %d ba_rxnobuf %d ba_rxunexp %d\n",
		cnt->rxpdu, cnt->rxqed, cnt->rxdup, cnt->rxnobuf, cnt->rxunexp);
	bcm_bprintf(b, "ba_rxba %d ba_rxbar %d ba_rxinvba %d ba_rxbaholes %d\n",
		cnt->rxba, cnt->rxbar, cnt->rxinvba, cnt->rxbaholes);
	bcm_bprintf(b, "ba_rxaddbareq %d ba_rxaddbaresp %d ba_rxdelba %d\n",
		cnt->rxaddbareq, cnt->rxaddbaresp, cnt->rxdelba);

	return 0;

}
#endif /* BCMDBG */
