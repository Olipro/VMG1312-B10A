/*
 * RPC Tx module
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_rpctx.c 359986 2012-10-01 13:38:04Z $
 */

#include <wlc_cfg.h>

#ifndef WLC_HIGH
#error "WLC_HIGH is not defined"
#endif	/* WLC_HIGH */

#ifdef WLC_LOW
#error "WLC_LOW is defined"
#endif /* WLC_LOW */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>

#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <bcm_rpc_tp.h>
#include <bcm_rpc.h>
#include <bcm_xdr.h>
#include <wlc_rpc.h>
#include <wlc_rpctx.h>

/* private RPC TX structure */
struct rpctx_info {
	wlc_info_t	*wlc;		/* common code handler */
	wlc_pub_t	*pub;		/* public common code handler */
	struct spktq	txq[NFIFO];	/* tx sw queue, wait txstatus to reclaim */
	int16 pktpend[NFIFO];		/* Maintain separate counter as AMPDU does
								* special calculations
								*/
};

rpctx_info_t*
wlc_rpctx_attach(wlc_pub_t *pub, wlc_info_t *cwlc)
{
	rpctx_info_t *rpcti;

	ASSERT(cwlc->rpctx == NULL);

	/* Allocate & initialize if not already done */
	if ((rpcti = (rpctx_info_t *)MALLOC(pub->osh, sizeof(rpctx_info_t))) == NULL) {
		WL_ERROR(("wlc_rpctx_attach: out of memory, malloced %d bytes",
		          MALLOCED(pub->osh)));
		return NULL;
	}
	bzero((char *)rpcti, sizeof(rpctx_info_t));
	rpcti->wlc = cwlc;
	rpcti->pub = pub;

	wlc_dump_register(pub, "rpctx", (dump_fn_t)wlc_rpctx_dump, (void *)rpcti);

	return (rpctx_info_t*)rpcti;
}

int
wlc_rpctx_fifoinit(rpctx_info_t *rpcti, uint fifo, uint ntxd)
{
	pktqinit(&rpcti->txq[fifo], ntxd);
	return 0;
}

void
wlc_rpctx_detach(rpctx_info_t *rpctx)
{
	if (rpctx == NULL)
		return;

	/* Free the current queues */
	wlc_rpctx_txreclaim(rpctx);

	MFREE(rpctx->pub->osh, rpctx, sizeof(rpctx_info_t));
}

int
wlc_rpctx_txpktpend(rpctx_info_t *rpctx, uint fifo, bool all)
{
	int total = 0;
	uint i = 0;
	if (!all)
		return rpctx->pktpend[fifo];
	/* Only sum across fifos 0-3 */
	for (; i < NFIFO-2; i++)
		total += rpctx->pktpend[i];
	return total;
}

void
wlc_rpctx_txpktpendinc(rpctx_info_t *rpctx, uint fifo, uint8 val)
{
	rpctx->pktpend[fifo] += val;
}

void
wlc_rpctx_txpktpenddec(rpctx_info_t *rpctx, uint fifo, uint16 val)
{
	rpctx->pktpend[fifo] -= val;
}

void
wlc_rpctx_txpktpendclr(rpctx_info_t *rpctx, uint fifo)
{
	rpctx->pktpend[fifo] = 0;
}

int
wlc_rpctx_dump(rpctx_info_t *rpctx, struct bcmstrbuf *b)
{
	uint fifo;

	bcm_bprintf(b, "\nrpctx:\n");
	for (fifo = 0; fifo < NFIFO; fifo++)
		bcm_bprintf(b, "Fifo: %d Available:%d Pending:%d Depth: %d\n",
		            fifo, pktq_avail(&rpctx->txq[fifo]),
		            rpctx->pktpend[fifo],
		            pktq_len(&rpctx->txq[fifo]));
	return 0;
}

void *
wlc_rpctx_getnexttxp(rpctx_info_t *rpctx, uint fifo)
{
	WL_TRACE(("%s fifo:%d depth:%d\n", __FUNCTION__, fifo, pktq_len(&rpctx->txq[fifo])));
	return pktdeq(&rpctx->txq[fifo]);
}

void
wlc_rpctx_txreclaim(rpctx_info_t *rpctx)
{
	uint fifo;

	for (fifo = 0; fifo < NFIFO; fifo++) {
		void *p;

		TXPKTPENDCLR(rpctx->wlc, fifo);
		WL_TRACE(("wlc_rpctx_txreclaim: pktpend fifo %d cleared\n", fifo));

		if (pktq_len(&rpctx->txq[fifo]) == 0)
		    continue;

		while ((p = pktdeq(&rpctx->txq[fifo])) != NULL)
			PKTFREE(rpctx->pub->osh, p, TRUE);
	}
}

uint
wlc_rpctx_txavail(rpctx_info_t *rpctx, uint fifo)
{
	return pktq_avail(&rpctx->txq[fifo]);
}

uint
wlc_rpctx_fifo_enabled(rpctx_info_t *rpctx, uint fifo)
{
	return (rpctx->txq[fifo].num_prec > 0);
}

int
wlc_rpctx_pkteng(rpctx_info_t *rpctx, uint fifo, void *p)
{
	ASSERT(pktq_avail(&rpctx->txq[fifo]));

	pktenq(&rpctx->txq[fifo], p);
	WL_TRACE(("%s: queue: %d %p depth:%d\n", __FUNCTION__, fifo, p,
		pktq_len(&rpctx->txq[fifo])));

	return 0;
}

/* For not NDIS, do not copy packets before sending to RPC layer. */
int
wlc_rpctx_tx(rpctx_info_t *rpctx, uint fifo, void *p, bool commit, uint16 frameid, uint8 txpktpend)
{
	uint rpc_totlen = 0;
	uint totlen = 0;
	wlc_info_t *wlc = rpctx->wlc;
	rpc_info_t *rpc = wlc->rpc;
	rpc_buf_t *rpc_buf;
	uint8 *buf;
	bcm_xdr_buf_t b;
	void *p0 = p;
	uint len;
	int err;
	bool pkt_chained;
	void *newp;
	uint32 rpc_hdrlen, flags;
	uint i;

	buf = NULL;
	pkt_chained = FALSE;
	newp = NULL;
	rpc_hdrlen = 0;
	i = 0;
	flags = 0;
	len = 0;

	if (WLPKTTAG(p)->flags & WLF_AMPDU_MPDU)
		flags = WLC_BMAC_F_AMPDU_MPDU;

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC)
	if (!ISALIGNED(PKTDATA(rpctx->pub->osh, p), sizeof(uint32))) {
		PKTPUSH(rpctx->pub->osh, p, WLC_RPC_TXFIFO_UNALIGN_PAD_2BYTE);
		flags |= WLC_BMAC_F_PAD2;
	}
#endif
	totlen = pkttotlen(rpctx->pub->osh, p);
	rpc_totlen = ROUNDUP(totlen, sizeof(uint32));

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY) || defined(BCM_RPC_TOC)

	/* 6 uint32 params(pkttotlen, fifo, commit, fid, txpktpend, pktflag)
	 *  + rpc_id + rpcheader + tp header
	 */
	rpc_hdrlen = WLC_RPCTX_PARAMS + bcm_rpc_buf_header_len(rpc);

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_TXNOCOPY)
	if (PKTNEXT(rpctx->pub->osh, p))
		pkt_chained = TRUE;
	else
		if ((uint)PKTHEADROOM(rpctx->pub->osh, p) < rpc_hdrlen) {
			WL_TRACE(("%s %p has not enough headroom (%d)  to add RPC header (%d) !!\n",
			          __FUNCTION__, p, PKTHEADROOM(rpctx->pub->osh, p), rpc_hdrlen));
			pkt_chained = TRUE;
		}
#elif defined(BCM_RPC_TOC)
	if ((uint)PKTHEADROOM(rpctx->pub->osh, p) < rpc_hdrlen) {
		WL_ERROR(("%s %p has not enough headroom (%d)  to add RPC header (%d) !!\n",
		          __FUNCTION__, p, (int)PKTHEADROOM(rpctx->pub->osh, p), rpc_hdrlen));
		ASSERT(0);
		pkt_chained = TRUE;
	}
#endif /* BCM_RPC_NOCOPY || BCM_RPC_TXNOCOPY */

	/* if chained, copy all into a single buffer */
	if (pkt_chained == TRUE) {
		newp = PKTGET(rpctx->pub->osh, rpc_totlen + rpc_hdrlen, FALSE);
		if (!newp) {
			WL_ERROR(("wl%d: %s PKTGET failed\n", rpctx->pub->unit, __FUNCTION__));
			PKTFREE(rpctx->pub->osh, p0, TRUE);
			return -1;
		}

		PKTPULL(rpctx->pub->osh, newp, rpc_hdrlen);
		buf = PKTDATA(rpctx->pub->osh, newp);
		/* add each packet fragment to the buffer */
		for (p = p0; (p != NULL); p = PKTNEXT(rpctx->pub->osh, p)) {
			void *data;
			len = PKTLEN(rpctx->pub->osh, p);
			data = PKTDATA(rpctx->pub->osh, p);

			++i;
			WL_TRACE(("%s %d:fraglen:%d\n", __FUNCTION__, i, len));

			/* !!! BYTE_COPY */
			memcpy(buf, data, len);
			buf += len;
		}
		p = newp;
		/* transfer all pkttags */
		memcpy(WLPKTTAG(p),  WLPKTTAG(p0), sizeof(wlc_pkttag_t));
	}

	/* if we copied it, toss original buffer and point p0 to the new one */
	if (pkt_chained) {
		PKTFREE(rpctx->pub->osh, p0, FALSE);
		p0 = newp;	/* p0 will be deposited to rpctx->txq */
	}

	/* txonecopy, navigate to the last fragment */
	p = pktlast(rpctx->pub->osh, p);

	/* padding the end(round to 4 bytes aligned with 0) */
	buf = PKTDATA(rpctx->pub->osh, p);
	len = PKTLEN(rpctx->pub->osh, p);
	if (rpc_totlen - totlen) {
		memset(buf + len, 0, (rpc_totlen - totlen));
	}
	/* set the right length(include padding) before next PKTPUSH */
	PKTSETLEN(rpctx->pub->osh, p, len + (rpc_totlen - totlen));

	p = p0;

	/* PKTDATA pointer has to move in order for RPC/TP to get right pkt length */
	PKTPUSH(rpctx->pub->osh, p, WLC_RPCTX_PARAMS);	/* params + rpc_id */

	/* init with buffer at offset including header + args */
	buf = PKTDATA(rpctx->pub->osh, p);
	bcm_xdr_buf_init(&b, buf, WLC_RPCTX_PARAMS);
	err = bcm_xdr_pack_uint32(&b, WLRPC_WLC_BMAC_TXFIFO_ID); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)fifo); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)commit); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)frameid);	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)txpktpend); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)flags); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, WLPKTTAG(p0)->u.exptime); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, totlen); ASSERT(!err);

	rpc_buf = (rpc_buf_t *)p;	/* This is direct mapping */
	WL_TRACE(("%s fifo:%d commit:%d frameid:0x%x txpktpend:%d totlen:%d\n",
		__FUNCTION__, fifo, commit, frameid, txpktpend, totlen));
#else
	/* for pkttotlen, fifo, commit, fid, txpktpend, pktflag, exptime */
	rpc_totlen += sizeof(uint32) * 7;

	/* RPC_BUFFER_TX, alloc */
	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, rpc_totlen, WLRPC_WLC_BMAC_TXFIFO_ID);
	if (!rpc_buf) {
		WL_ERROR(("wl%d: %s RPC buf alloc failed\n", rpctx->pub->unit, __FUNCTION__));
		PKTFREE(rpctx->pub->osh, p, TRUE);
		return -1;
	}

	err = bcm_xdr_pack_uint32(&b, (uint32)fifo); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)commit); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)frameid);	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)txpktpend); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)flags); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, WLPKTTAG(p0)->u.exptime); ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, totlen); ASSERT(!err);

	WL_TRACE(("%s fifo:%d commit:%d frameid:0x%x txpktpend:%d totlen:%d\n", __FUNCTION__, fifo,
		commit, frameid, txpktpend, totlen));

	/* add each packet fragment to the buffer without 4-byte padding */
	i = 0;
	for (p = p0; p != NULL; p = PKTNEXT(rpctx->pub->osh, p)) {
		void *data;
		len = PKTLEN(rpctx->pub->osh, p);
		data = PKTDATA(rpctx->pub->osh, p);

		++i;
		WL_TRACE(("%s %d:fraglen:%d\n", __FUNCTION__, i, len));

		err = bcm_xdr_pack_opaque_raw(&b, len, data);
		ASSERT(!err);
	}
	/* for the last packet fragment, add the XDR buf padding to be 4 byte aligned */
	err = bcm_xdr_pack_opaque_pad(&b);
	ASSERT(!err);

#endif /* BCM_RPC_NOCOPY || BCM_RPC_TXNOCOPY  || BCM_RPC_TOC */

	wlc_rpctx_pkteng(rpctx, fifo, p0);
	if (commit) {
		TXPKTPENDINC(wlc, fifo, txpktpend);
		WL_TRACE(("wlc_rpctx_tx, pktpend inc %d to %d\n", txpktpend,
			TXPKTPENDGET(wlc, fifo)));
	}
	/* RPC_BUFFER_TX, pass it to dbus */
	return bcm_rpc_call(rpc, rpc_buf);
}
