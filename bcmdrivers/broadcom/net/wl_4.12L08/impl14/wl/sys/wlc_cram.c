/*
 * CRAM source file
 * CRAM is a BRCM proprietary small TCP frame aggregator
 * protocol. Receiver software de-aggregates the frames
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cram.c,v 1.48.2.3 2010-07-23 17:05:23 Exp $
 */
#ifndef CRAM
#error "CRAM is not defined"
#endif	/* CRAM */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/bcmip.h>
#include <wlioctl.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_cram.h>

#ifdef AP
/* iovar table */
enum {
	IOV_CRAM,
	};

static const bcm_iovar_t cram_iovars[] = {
	{"cram", IOV_CRAM, (0), IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
};

struct cram_info {
	wlc_info_t *wlc;		/* pointer to main wlc structure */
	void	*cramp;			/* cram-protocol packet under construction */
	int	scb_handle;		/* scb cubby handle to retrieve data from scb */
	wlc_hwtimer_to_t *cram_to;	/* cram timeout object used with 
					 * multiplexed hw timers
					 */
};

#define WLC_CRAM_MAXBYTES	256	/* cram protocol: max # bytes to pack */
#define WLC_CRAM_MAXTIME	750	/* cram protocol: max # microseconds to buffer */

static void wlc_cram(void *ctx, struct scb *scb, void *p, uint prec);
static void wlc_cram_timeout_cb(void *arg);
static bool wlc_cram_open(cram_info_t *crami, struct ether_addr *ea);
static bool wlc_cram_append(cram_info_t *crami, void *p);
static void wlc_cram_scb_free_notify(void *context, struct scb *scb);
static int wlc_cram_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
                            void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static uint wlc_cram_txpktcnt(void *ctx);

static txmod_fns_t cram_txmod_fns = {
	wlc_cram,
	wlc_cram_txpktcnt,
	wlc_cram_close,
	NULL
};

cram_info_t *
BCMATTACHFN(wlc_cram_attach)(wlc_info_t *wlc)
{
	cram_info_t *cram;

	if (!(cram = (cram_info_t *)MALLOC(wlc->osh, sizeof(cram_info_t)))) {
		WL_ERROR(("wl%d: wlc_cram_attach: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)cram, sizeof(cram_info_t));
	cram->wlc = wlc;

	/* allocate cram_timeout object */
	cram->cram_to = wlc_hwtimer_alloc_timeout(wlc);
	if (cram->cram_to == NULL) {
		WL_ERROR(("wl%d: wlc_cram_attach: cram_to alloc failed\n", wlc->pub->unit));
		wlc_cram_detach(cram);
		return NULL;
	}

	/* reserve cubby in the scb container for per-scb private data */
	cram->scb_handle = wlc_scb_cubby_reserve(wlc, 0, NULL, wlc_cram_scb_free_notify,
		NULL, (void *)cram);
	if (cram->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_cram_attach: wlc_scb_cubby_reserve failed\n", wlc->pub->unit));
		wlc_cram_detach(cram);
		return NULL;
	}

	wlc_txmod_fn_register(wlc, TXMOD_CRAM,  cram, cram_txmod_fns);

	/* register module */
	wlc_module_register(wlc->pub, cram_iovars, "cram", cram,
		wlc_cram_doiovar, NULL, NULL, NULL);

	return cram;
}

void
BCMATTACHFN(wlc_cram_detach)(cram_info_t *cram)
{
	wlc_info_t *wlc;

	if (!cram)
		return;

	wlc = cram->wlc;
	wlc_module_unregister(wlc->pub, "cram", cram);
	/* free the cram timeout object */
	wlc_hwtimer_del_timeout(wlc->gptimer, cram->cram_to);
	wlc_hwtimer_free_timeout(wlc, cram->cram_to);

	MFREE(wlc->osh, cram, sizeof(cram_info_t));
}

/* handle cram related iovars */
static int
wlc_cram_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
                 void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	cram_info_t *crami = (cram_info_t *)hdl;
	int32 int_val = 0;
	bool bool_val;
	int err = 0;
	wlc_info_t *wlc;
	int32 *ret_int_ptr = (int32 *)a;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	wlc = crami->wlc;
	ASSERT(crami == wlc->crami);

	switch (actionid) {
	case IOV_GVAL(IOV_CRAM):
		*ret_int_ptr = (int32)wlc->pub->_cram;
		break;

	case IOV_SVAL(IOV_CRAM):
		wlc->pub->_cram = bool_val;
		if (!wlc->pub->_cram)
			wlc_cram_stop(crami);
		break;
	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

/* Return the #of transmit packets held by CRAM */
static uint
wlc_cram_txpktcnt(void *ctx)
{
	cram_info_t *crami = (cram_info_t *)ctx;

	if (crami->cramp)
		return 1;

	return 0;
}

/*
 * Append small tcp frames into a single "cram" protocol frame
 * for improved media rates when used with afterburner.
 * Return TRUE if packet consumed, otherwise FALSE .
 */
static void
wlc_cram(void *ctx, struct scb *scb, void *p, uint prec)
{
	wlc_if_t *cif;
	wlc_if_t *pif;
	bool candidate, match = FALSE;
	struct scb *scb_cram;
	cram_info_t *crami = (cram_info_t *)ctx;
	osl_t *osh;
	uint16 etype;
	uint8 *body;
	wlc_info_t *wlc = crami->wlc;

	osh = wlc->osh;

	/* Cram is not applicable... go to next feature */
	if (
#ifdef WLAFTERBURNER
	    !wlc->afterburner ||
#endif /* WLAFTERBURNER */
	    !CRAM_ENAB(wlc->pub)) {
		SCB_TX_NEXT(TXMOD_CRAM, scb, p, WLC_PRIO_TO_PREC(PKTPRIO(p)));
		return;
	}

	etype = wlc_sdu_etype(wlc, p);
	body = wlc_sdu_data(wlc, p);

	/* one-entry cache indexed by dst addr */
	candidate = (ntoh16(etype) == ETHER_TYPE_IP) &&
	        (pkttotlen(osh, p) < 100) &&
	        (IP_VER(body) == IP_VER_4) &&
	        (IPV4_PROT(body) == IP_PROT_TCP); /* only IPv4 allowed */

#ifdef WLAFTERBURNER
	/* This function should be in the path only if SCB is capable */
	ASSERT(scb->flags & SCB_ABCAP);
#endif /* WLAFTERBURNER */

	/* see if p matches the currently open cram packet */
	if (candidate && crami->cramp &&
	    !bcmp(PKTDATA(osh, p), PKTDATA(osh, crami->cramp), ETHER_ADDR_LEN)) {
		scb_cram = WLPKTTAGSCBGET(crami->cramp);
		/* DA matches, make sure packets are being sent on the same interface */
		cif = SCB_WLCIFP(scb);
		pif = SCB_WLCIFP(scb_cram);
		match = (cif == NULL && pif == NULL) ||
			((cif && cif->type == WLC_IFTYPE_BSS) &&
			(pif && pif->type == WLC_IFTYPE_BSS) &&
			(cif->u.bsscfg == pif->u.bsscfg));
	}

	if (match) {
		if (wlc_cram_append(crami, p))
			return;
	} else {
		if (crami->cramp)
			wlc_cram_close(crami, NULL);
		if (candidate && wlc_cram_append(crami, p))
			return;
	}

	SCB_TX_NEXT(TXMOD_CRAM, scb, p, WLC_PRIO_TO_PREC(PKTPRIO(p)));
}

static void wlc_cram_timeout_cb(void *arg)
{
	wlc_cram_close((cram_info_t *)arg, NULL);
}

/* return true on success, false on error */
static bool
wlc_cram_open(cram_info_t *crami, struct ether_addr *ea)
{
	struct ether_header *eh;
	uint16 *subtype;
	void *p;
	osl_t *osh;
	wlc_info_t *wlc = crami->wlc;

	osh = wlc->osh;

	/* alloc new frame buffer */
	if ((p = PKTGET(osh, (TXOFF + ETHER_MAX_LEN), TRUE)) == NULL) {
		WL_ERROR(("wl%d: wlc_cram_open: pktget failed\n", wlc->pub->unit));
		WLCNTINCR(wlc->pub->_cnt->txnobuf);
		return (FALSE);
	}
	ASSERT(ISALIGNED((uintptr)PKTDATA(osh, p), sizeof(uint32)));

	/* reserve TXOFF bytes of headroom */
	PKTPULL(osh, p, TXOFF);
	PKTSETLEN(osh, p, 0);

	/* install it as crami->cramp */
	ASSERT(crami->cramp == NULL);
	crami->cramp = p;

	/* init ether_header */
	eh = (struct ether_header*) PKTDATA(osh, p);
	bcopy((char*)ea, eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy((char*)&wlc->pub->cur_etheraddr, eh->ether_shost, ETHER_ADDR_LEN);
	eh->ether_type = hton16(ETHER_TYPE_BRCM);

	/* add brcm cram subtype field */
	subtype = (uint16*) &eh[1];
	*subtype++ = 0;	/* reserved
			 */
	*subtype = hton16(ETHER_BRCM_CRAM);

	PKTSETLEN(osh, p, (ETHER_HDR_LEN + ETHER_BRCM_SUBTYPE_LEN));

	/* start timer */
	/* the new timeout will supercede the old one so delete old one first */
	wlc_hwtimer_del_timeout(wlc->gptimer, crami->cram_to);
	wlc_hwtimer_add_timeout(wlc->gptimer, crami->cram_to,
	                        WLC_CRAM_MAXTIME, wlc_cram_timeout_cb, (void *)crami);

	return (TRUE);
}

static void
wlc_cram_scb_free_notify(void *context, struct scb *scb)
{
	cram_info_t *crami = (cram_info_t *)context;
	wlc_info_t *wlc = crami->wlc;

	if (!CRAM_ENAB(wlc->pub) || crami->cramp == NULL || WLPKTTAGSCBGET(crami->cramp) != scb)
		return;

	PKTFREE(wlc->osh, crami->cramp, TRUE);
	crami->cramp = NULL;
}

void
wlc_cram_close(void *ctx, struct scb *scb)
{
	cram_info_t *crami = (cram_info_t *)ctx;
	wlc_info_t *wlc = crami->wlc;
	struct scb *cramp_scb;
	struct ether_header *eh;
	osl_t *osh;

	osh = wlc->osh;

	if (crami->cramp == NULL)
		return;

	cramp_scb = WLPKTTAGSCBGET(crami->cramp);

	/* If scb does not match, don't close this */
	if (scb && scb != cramp_scb)
		return;

	/* convert cram hdr to 8023hdr, it can be done in open time, but length field needs fixup */
	eh = (struct ether_header*) PKTDATA(osh, crami->cramp);
	ASSERT(ntoh16(eh->ether_type) == ETHER_TYPE_BRCM);

	WLPKTTAG(crami->cramp)->flags |= WLF_NON8023;
	wlc_ether_8023hdr(wlc, wlc->osh, eh, crami->cramp);

	/* if non-empty cram frame send it, else just free it */
	ASSERT(PKTLEN(wlc->osh, crami->cramp) > (ETHER_HDR_LEN + ETHER_BRCM_SUBTYPE_LEN));

	SCB_TX_NEXT(TXMOD_CRAM, cramp_scb, crami->cramp, WLC_PRIO_TO_PREC(PKTPRIO(crami->cramp)));

	crami->cramp = NULL;
}

void
wlc_cram_stop(cram_info_t *crami)
{
	wlc_info_t *wlc = crami->wlc;

	if (crami->cramp == NULL)
		return;

	PKTFREE(wlc->osh, crami->cramp, TRUE);
	crami->cramp = NULL;

	wlc_hwtimer_del_timeout(wlc->gptimer, crami->cram_to);
}

/* return true on success, false on error */
static bool
wlc_cram_append(cram_info_t *crami, void *p)
{
	uint origlen, len, totlen;
	osl_t *osh;
	uchar *dst;
	wlc_info_t *wlc = crami->wlc;
	struct ether_header *eh, *neh;

	osh = wlc->osh;

	/* alloc new pack tx buffer if necessary */
	if (crami->cramp == NULL) {
		if (!wlc_cram_open(crami, (struct ether_addr*) PKTDATA(osh, p)))
			return (FALSE);
		/* copy the scb pointer to the cram packet */
		WLPKTTAGSCBSET(crami->cramp, WLPKTTAGSCBGET(p));
	}

	/* undo ether->8023hdr conversion to be conform to original 
	 * ethernet-header-based protocol definition. wlc_uncram() requires no changes.
	 */
	if (WLPKTTAG(p)->flags & WLF_NON8023) {
		ASSERT(PKTLEN(osh, p) >= (ETHER_HDR_LEN + DOT11_LLC_SNAP_HDR_LEN));
		eh = (struct ether_header *)PKTDATA(osh, p);
		neh = (struct ether_header *)PKTPULL(osh, p, DOT11_LLC_SNAP_HDR_LEN);
		/* copy SA first to avoid overwritten */
		bcopy((char*)eh->ether_shost, (char*)neh->ether_shost, ETHER_ADDR_LEN);
		bcopy((char*)eh->ether_dhost, (char*)neh->ether_dhost, ETHER_ADDR_LEN);
	}

	origlen = PKTLEN(osh, crami->cramp);
	len = pkttotlen(osh, p);
	totlen = origlen + sizeof(uint16) + len;

	/* pad if odd length */
	if (len & 1)
		totlen++;

	ASSERT(totlen <= ETHER_MAX_LEN);

	dst = PKTDATA(osh, crami->cramp) + origlen;

	/* write length of frame that follows */
	*((uint16*)dst) = hton16((uint16)len);
	dst += sizeof(uint16);

	/* append (copy) packet onto end */
	pktcopy(osh, p, 0, len, dst);
	PKTSETLEN(osh, crami->cramp, totlen);

	/* Append any packet callbacks from p to crami->cramp */
	wlc_pkt_callback_append(wlc, crami->cramp, p);

	/* free original packet */
	PKTFREE(osh, p, TRUE);

	/* if the resulting packet exceeds the maxbyte threshold, send it */
	if (totlen >= WLC_CRAM_MAXBYTES)
		wlc_cram_close(crami, NULL);

	return (TRUE);
}
#endif /* AP */

#ifdef STA
/* uncram contents of a received frame */
#define HEADROOM  \
	DOT11_A3_HDR_LEN+DOT11_LLC_SNAP_HDR_LEN-ETHER_HDR_LEN

static bool wlc_cram_islegacy(wlc_info_t *wlc, void *p);

void
wlc_uncram(wlc_info_t *wlc, struct scb *scb, bool wds,
	struct ether_addr *da, void *orig_pkt, char *prx_ctxt,
	int len_rx_ctxt)
{
	osl_t	*osh;
	uint	len;
	void	*p;
	uchar *pdata;	/* ptr to first byte of subtype field */
	int datalen;	/* number of bytes in all subframes */
	int headroom;

	WL_TRACE(("uncram() ... enter\n"));

	osh = wlc->osh;
	pdata = PKTDATA(osh, orig_pkt);
	datalen = PKTLEN(osh, orig_pkt);

	/* Assert that our subtype is cram */
	ASSERT(ntoh16(*(uint16 *)(pdata+2)) == ETHER_BRCM_CRAM);
	pdata += ETHER_BRCM_SUBTYPE_LEN;
	datalen -= ETHER_BRCM_SUBTYPE_LEN;

	/* Process subframes that follow the subtype */
	while (datalen >= (int)sizeof(uint16)) {
		/* Each subframe starts with a length */
		len = ntoh16(load16_ua(pdata));
		pdata += sizeof(uint16);
		datalen -= sizeof(uint16);

		if ((int)len > datalen)
			break;

		headroom = len + 2;
		if (WLEXTSTA_ENAB(wlc->pub))
			headroom += HEADROOM + len_rx_ctxt;
		else
			headroom += BCMDONGLEHDRSZ;

		/* Allocate a new packet buffer with headroom sufficient
		 * to allow prepending DOT11 and SNAP headers.
		 */
		if ((p = PKTGET(osh, headroom, FALSE)) == NULL) {
			WL_ERROR(("wl%d: wlc_uncram: pktget error\n", wlc->pub->unit));
			WLCNTINCR(wlc->pub->_cnt->rxnobuf);
			goto done;
		}

		if (WLEXTSTA_ENAB(wlc->pub))
			PKTPULL(osh, p, 2 + HEADROOM);
		else
			PKTPULL(osh, p, 2 + BCMDONGLEHDRSZ);

		/* Copy next cram'd subframe into new pkt */
		bcopy(pdata, PKTDATA(osh, p), len);
		PKTSETLEN(osh, p, len);

		/* Adjust for odd length in the subframe */
		if ((len & 1) && !(scb->flags & SCB_LEGACY_CRAM))
			len++;

		pdata += len;
		datalen -= len;

		/* Append the rx context structure */
		if (WLEXTSTA_ENAB(wlc->pub))
			bcopy(prx_ctxt, (uchar *)PKTDATA(wlc->osh, p) +
				PKTLEN(wlc->osh, p), len_rx_ctxt);

		wlc_recvdata_sendup(wlc, scb, wds, da, p);
		WL_TRACE(("uncram() ... sending up\n"));
	}

	if (datalen < 0) {
		WL_ERROR(("wl%d: wlc_uncram: error: resid %d\n", wlc->pub->unit, datalen));
		/* is remote is a legacy cram station? */
		if (!(scb->flags & SCB_LEGACY_CRAM) && wlc_cram_islegacy(wlc, orig_pkt))
			scb->flags |= SCB_LEGACY_CRAM;
	}

done:
	PKTFREE(osh, orig_pkt, FALSE);
}

static bool
wlc_cram_islegacy(wlc_info_t *wlc, void *p)
{
	osl_t *osh;
	uint len;
	uchar *data;
	int resid;
	bool oddfound;

	osh = wlc->osh;
	data = PKTDATA(osh, p);
	resid = PKTLEN(osh, p);
	oddfound = FALSE;

	data += (ETHER_BRCM_SUBTYPE_LEN);
	resid -= (ETHER_BRCM_SUBTYPE_LEN);

	while (resid > 0) {
		if (resid < (int) sizeof(uint16))
			return (FALSE);

		/* get length of next frame */
		len = ntoh16_ua(data);

		data += sizeof(uint16);
		resid -= sizeof(uint16);

		/* check for errors */
		if ((len < ETHER_HDR_LEN) || (len > 100) || ((int)len > resid))
			return (FALSE);

		if (len & 1)
			oddfound = TRUE;

		data += len;
		resid -= len;
	}

	/* minimize false positives */
	if ((resid == 0) && oddfound)
		return (TRUE);

	return (FALSE);
}
#endif /* STA */
