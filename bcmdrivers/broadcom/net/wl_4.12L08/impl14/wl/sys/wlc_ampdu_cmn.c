/*
 * A-MPDU (with extended Block Ack protocol) source file
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
 * $Id: wlc_ampdu_cmn.c 355499 2012-09-07 00:20:43Z $
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
#include <wlc_ampdu_rx.h>
#include <wlc_ampdu_cmn.h>
#endif
#include <wlc_scb_ratesel.h>
#include <wl_export.h>

#ifdef BCMDBG
uint32 wl_ampdu_dbg = WL_AMPDU_ERR_VAL;
#endif

/* iovar table */
enum {
	IOV_AMPDU,		/* enable/disable ampdu */
	IOV_AMPDU_BA_WSIZE,	/* ampdu ba window size :kept for backward compatibility */
	IOV_AMPDU_DENSITY,	/* ampdu density */
	IOV_AMPDU_CLEAR_DUMP,	/* clear ampdu counters */
	IOV_ACTIVATE_TEST,
	IOV_AMPDU_DBG,
	IOV_AMPDU_LAST
};

static const bcm_iovar_t ampdu_iovars[] = {
	{"ampdu", IOV_AMPDU, (IOVF_SET_DOWN), IOVT_BOOL, 0},	/* only if down */
	{"ampdu_ba_wsize", IOV_AMPDU_BA_WSIZE, (0), IOVT_INT8, 0},
	{"ampdu_density", IOV_AMPDU_DENSITY, (0), IOVT_UINT8, 0},
#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
	{"ampdu_clear_dump", IOV_AMPDU_CLEAR_DUMP, 0, IOVT_VOID, 0},
#endif  /* defined(BCMDBG) || defined(WLTEST) */
#ifdef BCMDBG
	{"ampdu_dbg", IOV_AMPDU_DBG, (0), IOVT_UINT32, 0},
#endif
	{NULL, 0, 0, 0, 0}
};

static int wlc_ampdu_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
        void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
static int wlc_ampdu_dump(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* BCMDBG || WLTEST */

int
BCMATTACHFN(wlc_ampdu_init)(wlc_info_t *wlc)
{

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
	wlc_dump_register(wlc->pub, "ampdu", (dump_fn_t)wlc_ampdu_dump, (void *)wlc);
#endif /*  defined(BCMDBG) || defined(WLTEST) */

	/* register module */
	if (wlc_module_register(wlc->pub, ampdu_iovars, "ampdu", wlc, wlc_ampdu_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: ampdu wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	return BCME_OK;

fail:
	wlc_ampdu_deinit(wlc);
	return BCME_ERROR;
}

void
BCMATTACHFN(wlc_ampdu_deinit)(wlc_info_t *wlc)
{
	if (!wlc)
		return;

	wlc_module_unregister(wlc->pub, "ampdu", wlc);
}

/* handle AMPDU related iovars */
static int
wlc_ampdu_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_info_t *wlc = (wlc_info_t *)hdl;
	int32 int_val = 0;
	int32 *ret_int_ptr = (int32 *) a;
	int err = 0;
	wlc_bsscfg_t *bsscfg;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_AMPDU):
		*ret_int_ptr = (int32)(wlc->pub->_ampdu_tx);
		break;

	case IOV_SVAL(IOV_AMPDU):
		if ((err = wlc_ampdu_tx_set(wlc->ampdu_tx, (bool)int_val)))
			return err;
		if ((err = wlc_ampdu_rx_set(wlc->ampdu_rx, (bool)int_val))) {
			wlc_ampdu_tx_set(wlc->ampdu_tx, (bool)~int_val);
			return err;
		}
		break;

	case IOV_GVAL(IOV_AMPDU_BA_WSIZE):
		*ret_int_ptr = (int32)wlc_ampdu_tx_get_ba_tx_wsize(wlc->ampdu_tx);
		break;

	case IOV_SVAL(IOV_AMPDU_BA_WSIZE):
		if ((int_val == 0) ||
			(int_val > wlc_ampdu_tx_get_ba_max_tx_wsize(wlc->ampdu_tx)) ||
			(int_val > wlc_ampdu_rx_get_ba_max_rx_wsize(wlc->ampdu_rx))) {
			err = BCME_BADARG;
			break;
		}
		wlc_ampdu_tx_set_ba_tx_wsize(wlc->ampdu_tx, (uint8)int_val);
		wlc_ampdu_rx_set_ba_rx_wsize(wlc->ampdu_rx, (uint8)int_val);

		break;

	case IOV_GVAL(IOV_AMPDU_DENSITY):
		*ret_int_ptr = (int32)wlc_ampdu_rx_get_mpdu_density(wlc->ampdu_rx);
		break;

	case IOV_SVAL(IOV_AMPDU_DENSITY):
		if (int_val > AMPDU_MAX_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}

		if (int_val < AMPDU_DEF_MPDU_DENSITY) {
			err = BCME_RANGE;
			break;
		}
		wlc_ampdu_rx_set_mpdu_density(wlc->ampdu_rx, (uint8)int_val);
		wlc_ampdu_tx_set_mpdu_density(wlc->ampdu_tx, (uint8)int_val);
		wlc_ampdu_update_ie_param(wlc->ampdu_rx);
		break;

#if defined(BCMDBG) || defined(WLTEST) || defined(WLPKTDLYSTAT) || \
	defined(BCMDBG_AMPDU)
#ifdef WLCNT
	case IOV_SVAL(IOV_AMPDU_CLEAR_DUMP):
	{
		wlc_ampdu_clear_tx_dump(wlc->ampdu_tx);
		wlc_ampdu_clear_rx_dump(wlc->ampdu_rx);
		break;
	}
#endif /* WLCNT */
#endif /* defined(BCMDBG) || defined(WLTEST) */
#ifdef BCMDBG
	case IOV_GVAL(IOV_AMPDU_DBG):
		*ret_int_ptr = wl_ampdu_dbg;
		break;

	case IOV_SVAL(IOV_AMPDU_DBG):
	        wl_ampdu_dbg = (uint32)int_val;
		break;
#endif /* BCMDBG */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
static int
wlc_ampdu_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	wlc_ampdu_tx_dump(wlc->ampdu_tx, b);
	wlc_ampdu_rx_dump(wlc->ampdu_rx, b);

	return 0;
}
#endif

/* FOLLOWING FUNCTIONS 2B MOVED TO COMMON (ampdu and ba) CODE ONCE DELAYED BA IS REVIVED */
void
wlc_frameaction_ampdu(wlc_info_t *wlc, struct scb *scb,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	uint8 action_id;

	ASSERT((body[0] & DOT11_ACTION_CAT_MASK) == DOT11_ACTION_CAT_BLOCKACK);

	if (!scb) {
		WL_AMPDU_CTL(("wl%d: wlc_frameaction_ampdu: scb not found\n",
			wlc->pub->unit));
		return;
	}

	if (body_len < 2)
		goto err;

	action_id = body[1];

	switch (action_id) {

	case DOT11_BA_ACTION_ADDBA_REQ:
		if (body_len < DOT11_ADDBA_REQ_LEN)
			goto err;
		wlc_ampdu_recv_addba_req(wlc, scb, body, body_len);
		break;

	case DOT11_BA_ACTION_ADDBA_RESP:
		if (body_len < DOT11_ADDBA_RESP_LEN)
			goto err;
		wlc_ampdu_recv_addba_resp(wlc->ampdu_tx, scb, body, body_len);
		break;

	case DOT11_BA_ACTION_DELBA:
		if (body_len < DOT11_DELBA_LEN)
			goto err;
		wlc_ampdu_recv_delba(wlc, scb, body, body_len);
		break;

	default:
		WL_ERROR(("wl%d: FC_ACTION: Invalid BA action id %d\n",
			wlc->pub->unit, action_id));
		goto err;
	}

	return;

err:
	WL_ERROR(("wl%d: %s: recd invalid frame of length %d\n",
		wlc->pub->unit, __FUNCTION__, body_len));
	WLCNTINCR(wlc->pub->_cnt->rxbadproto);
	wlc_send_action_err(wlc, hdr, body, body_len);
}

void
wlc_scb_ampdu_enable(wlc_info_t *wlc, struct scb *scb)
{
	ASSERT(scb);

	wlc_txmod_config(wlc, scb, TXMOD_AMPDU);
	wlc_ampdu_agg_state_update_rx(wlc, SCB_BSSCFG(scb), ON);
}

void
wlc_scb_ampdu_disable(wlc_info_t *wlc, struct scb *scb)
{
	ASSERT(scb);

	wlc_txmod_unconfig(wlc, scb, TXMOD_AMPDU);
	scb_ampdu_rx_flush(wlc->ampdu_rx, scb);
}

void
wlc_ampdu_recv_ctl(wlc_info_t *wlc, struct scb *scb, uint8 *body, int body_len, uint16 fk)
{

	if (!scb || !SCB_AMPDU(scb)) {
		WL_AMPDU_CTL(("wl%d: wlc_ampdu_recv_ctl: AMPDU not advertized by remote\n",
			wlc->pub->unit));
		return;
	}

	if (fk == FC_BLOCKACK_REQ) {
		if (body_len < DOT11_BAR_LEN)
			goto err;
		wlc_ampdu_recv_bar(wlc->ampdu_rx, scb, body, body_len);
	} else if (fk == FC_BLOCKACK) {
		if (body_len < (DOT11_BA_LEN + DOT11_BA_CMP_BITMAP_LEN))
			goto err;
		wlc_ampdu_recv_ba(wlc->ampdu_tx, scb, body, body_len);
	} else {
		ASSERT(0);
	}

	return;

err:
	WL_ERROR(("wl%d: %s: recd invalid frame of length %d\n",
		wlc->pub->unit, __FUNCTION__, body_len));
	WLCNTINCR(wlc->pub->_cnt->rxbadproto);
}

void
wlc_ampdu_recv_addba_req(wlc_info_t *wlc, struct scb *scb, uint8 *body, int body_len)
{
	dot11_addba_req_t *addba_req;

	ASSERT(scb);

	addba_req = (dot11_addba_req_t *)body;

	/* check if it is action err frame */
	if (addba_req->category & DOT11_ACTION_CAT_ERR_MASK)
		wlc_ampdu_recv_addba_req_ini(wlc->ampdu_tx, scb, addba_req, body_len);
	else
		wlc_ampdu_recv_addba_req_resp(wlc->ampdu_rx, scb, addba_req, body_len);

}
/* function to send addba req
 * Does not have any dependency on ampdu, so can be used for delayed ba as well
 */
int
wlc_send_addba_req(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint16 wsize,
	uint8 ba_policy, uint8 delba_timeout)
{
	dot11_addba_req_t *addba_req;
	uint16 tmp, start_seq;
	void *p;
	uint8 *pbody;
#if defined(STA) && defined(WME)
	wlc_bsscfg_t *cfg;
#endif /* defined(STA) && defined(WME) */

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(tid < AMPDU_MAX_SCB_TID);
	ASSERT(wsize <= wlc_ampdu_tx_get_ba_tx_wsize(wlc->ampdu_tx));

	if (wlc->block_datafifo)
		return BCME_NOTREADY;

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, &scb->ea, &scb->bsscfg->cur_etheraddr,
		&scb->bsscfg->BSSID, DOT11_ADDBA_REQ_LEN, &pbody);
	if (p == NULL)
		return BCME_NOMEM;

	addba_req = (dot11_addba_req_t *)pbody;
	addba_req->category = DOT11_ACTION_CAT_BLOCKACK;
	addba_req->action = DOT11_BA_ACTION_ADDBA_REQ;
	addba_req->token = (uint8)wlc->counter;
	/* token cannot be zero */
	if (!addba_req->token) {
		wlc->counter++;
		addba_req->token++;
	}

	tmp = ((tid << DOT11_ADDBA_PARAM_TID_SHIFT) & DOT11_ADDBA_PARAM_TID_MASK) |
		((wsize << DOT11_ADDBA_PARAM_BSIZE_SHIFT) & DOT11_ADDBA_PARAM_BSIZE_MASK) |
		((ba_policy << DOT11_ADDBA_PARAM_POLICY_SHIFT) & DOT11_ADDBA_PARAM_POLICY_MASK);
	htol16_ua_store(tmp, (uint8 *)&addba_req->addba_param_set);
	htol16_ua_store(delba_timeout, (uint8 *)&addba_req->timeout);
	start_seq = (SCB_SEQNUM(scb, tid) & (SEQNUM_MAX - 1)) << SEQNUM_SHIFT;
	htol16_ua_store(start_seq, (uint8 *)&addba_req->start_seqnum);

	WL_AMPDU_CTL(("wl%d: wlc_send_addba_req: seq 0x%x tid %d wsize %d\n",
		wlc->pub->unit, start_seq, tid, wsize));

	/* set same priority as tid */
	PKTSETPRIO(p, tid);
	wlc_sendmgmt(wlc, p, SCB_WLCIFP(scb)->qi, scb);

#if defined(STA) && defined(WME)
	/* If STA has PM with APSD enabled, then send null data frame as the trigger frame
	 * so that the addba response can be delivered
	 */
	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	if (BSSCFG_STA(cfg) && cfg->BSS && cfg->pm->PM != PM_OFF &&
	    AC_BITMAP_TST(scb->apsd.ac_delv, WME_PRIO2AC(tid))) {

		WL_ERROR(("wl%d: %s: sending null data frame on tid %d\n",
			wlc->pub->unit, __FUNCTION__, tid));
		wlc_sendnulldata(wlc, scb->bsscfg, &scb->ea, 0, 0, tid);
	}
#endif /* defined(STA) && defined(WME) */

	return 0;
}

/* function to send delba.
 * Does not have any dependency on ampdu, so can be used for delayed ba as well
 */
int
wlc_send_delba(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint16 initiator, uint16 reason)
{
	dot11_delba_t *delba;
	uint16 tmp;
	void *p;
	uint8 *pbody;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	if (wlc->block_datafifo)
		return BCME_NOTREADY;

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, &scb->ea, &scb->bsscfg->cur_etheraddr,
		&scb->bsscfg->BSSID, DOT11_DELBA_LEN, &pbody);
	if (p == NULL)
		return BCME_NOMEM;

	delba = (dot11_delba_t *)pbody;
	delba->category = DOT11_ACTION_CAT_BLOCKACK;
	delba->action = DOT11_BA_ACTION_DELBA;
	tmp = ((tid << DOT11_DELBA_PARAM_TID_SHIFT) & DOT11_DELBA_PARAM_TID_MASK) |
		((initiator << DOT11_DELBA_PARAM_INIT_SHIFT) & DOT11_DELBA_PARAM_INIT_MASK);
	delba->delba_param_set = htol16(tmp);
	delba->reason = htol16(reason);

	WL_AMPDU_CTL(("wl%d: wlc_send_delba: tid %d initiator %d reason %d\n",
		wlc->pub->unit, tid, initiator, reason));

	/* set same priority as tid */
	PKTSETPRIO(p, tid);

	wlc_sendmgmt(wlc, p, SCB_WLCIFP(scb)->qi, scb);

	return 0;
}

/* function to send a bar.
 * Does not have any dependency on ampdu, so can be used for delayed ba as well
 */
void *
wlc_send_bar(wlc_info_t *wlc, struct scb *scb, uint8 tid, uint16 start_seq,
	uint16 cf_policy, bool enq_only, bool *blocked)
{
	struct dot11_ctl_header *hdr;
	struct dot11_bar *bar;
	void *p;
	uint16 tmp;

	ASSERT(wlc);
	ASSERT(scb);
	ASSERT(scb->bsscfg);
	ASSERT(tid < AMPDU_MAX_SCB_TID);

	if (wlc->block_datafifo) {
		*blocked = TRUE;
		return NULL;
	}

	*blocked = FALSE;

	p = wlc_frame_get_ctl(wlc, DOT11_CTL_HDR_LEN + DOT11_BAR_LEN);
	if (p == NULL)
		return NULL;

	hdr = (struct dot11_ctl_header *)PKTDATA(wlc->osh, p);
	hdr->fc = htol16(FC_BLOCKACK_REQ);
	hdr->durid = 0;
	bcopy(&scb->ea, &hdr->ra, ETHER_ADDR_LEN);
	bcopy(&scb->bsscfg->cur_etheraddr, &hdr->ta, ETHER_ADDR_LEN);

	bar = (struct dot11_bar *)&hdr[1];
	tmp = tid << DOT11_BA_CTL_TID_SHIFT;
	tmp |= (cf_policy & DOT11_BA_CTL_POLICY_MASK);
	tmp |= DOT11_BA_CTL_COMPRESSED;
	bar->bar_control = htol16(tmp);
	bar->seqnum = htol16(start_seq << SEQNUM_SHIFT);

	WL_ERROR(("wl%d: %s: seq 0x%x tid %d\n", wlc->pub->unit, __FUNCTION__, start_seq, tid));

	/* set same priority as tid */
	PKTSETPRIO(p, tid);

	if (wlc_sendctl(wlc, p, SCB_WLCIFP(scb)->qi, scb, TX_CTL_FIFO, 0, enq_only))
		return p;
	else
		return NULL;
}

void
wlc_ampdu_recv_delba(wlc_info_t *wlc, struct scb *scb, uint8 *body, int body_len)
{
	dot11_delba_t *delba;
	uint16 param_set;
	uint8 tid;
	uint16 reason, initiator;

	ASSERT(scb);

	delba = (dot11_delba_t *)body;

	param_set = ltoh16(delba->delba_param_set);
	reason = ltoh16(delba->reason);
	BCM_REFERENCE(reason);

	tid = (param_set & DOT11_DELBA_PARAM_TID_MASK) >> DOT11_DELBA_PARAM_TID_SHIFT;
	initiator = (param_set & DOT11_DELBA_PARAM_INIT_MASK) >> DOT11_DELBA_PARAM_INIT_SHIFT;

	if (initiator)
		wlc_ampdu_rx_recv_delba(wlc->ampdu_rx, scb, tid, delba->category, initiator,
			reason);
	else
		wlc_ampdu_tx_recv_delba(wlc->ampdu_tx, scb, tid, delba->category, initiator,
			reason);
}

void
scb_ampdu_cleanup(wlc_info_t *wlc, struct scb *scb)
{
	uint8 tid;

	WL_AMPDU_UPDN(("scb_ampdu_cleanup: enter\n"));

	for (tid = 0; tid < AMPDU_MAX_SCB_TID; tid++) {
		ampdu_cleanup_tid_resp(wlc->ampdu_rx, scb, tid);
		ampdu_cleanup_tid_ini(wlc->ampdu_tx, scb, tid, FALSE);
	}
}

void
scb_ampdu_cleanup_all(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb;
	struct scb_iter scbiter;

	WL_AMPDU_UPDN(("scb_ampdu_cleanup_all: enter\n"));
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (SCB_AMPDU(scb))
			scb_ampdu_tx_flush(wlc->ampdu_tx, scb);
			scb_ampdu_rx_flush(wlc->ampdu_rx, scb);
	}
}

/* configuration to disable/enable AMPDU..cases where iovars may not be available */
void
BCMATTACHFN(wlc_ampdu_agg_state_override)(wlc_info_t *wlc, int8 txaggr, int8 rxaggr)
{
	wlc_ampdu_agg_state_txaggr_override(wlc->ampdu_tx, txaggr);
	wlc_ampdu_agg_state_rxaggr_override(wlc->ampdu_rx, rxaggr);
}
