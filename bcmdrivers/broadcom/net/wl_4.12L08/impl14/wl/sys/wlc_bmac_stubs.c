/*
 * Stub functions used for dispatching RPC call to BMAC
 * Broadcom 802.11bang Networking Device Driver
 *
 * The external functions should match wlc_bmac.c
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_bmac_stubs.c 368767 2012-11-14 22:10:38Z $
 */


#include <wlc_cfg.h>

#ifdef WLC_LOW
#error "This file should not be included for WLC_LOW"
#endif
#ifndef WLC_HIGH
#error "This file needs WLC_HIGH"
#endif

/* for user space driver: begin */
#ifndef BCMDRIVER
typedef struct bcm_iovar bcm_iovar_t;
#define PKTTAG(p) (p)
struct pktq
{
	int a;
};
#define AP
#define STA
#define WLRM
#define WME
#define WL11H
#define _wlconf_h_
#endif
/* for user space driver: end */


#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <osl.h>
#include <proto/802.11.h>
#include <proto/802.1d.h>
#include <bcmwifi_channels.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <proto/bcmevent.h>
#include <sbhnddma.h>
#include <sbhndpio.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_stf.h>
#include <wlc_bmac.h>
#include <bcm_xdr.h>
#include <bcm_rpc_tp.h>
#include <bcm_rpc.h>
#include <wlc_rpc.h>
#include <wlc_apps.h>
#include <wlc_phy_hal.h>
#include <wlc_ampdu.h>
#include <wlc_extlog.h>
#include <bcmsrom_fmt.h>
#include <wlc_rpctx.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif

#if defined(BCMDBG) || defined(BCMDBG_ERR)
const struct rpc_name_entry rpc_name_tbl[] = RPC_ID_TABLE;
#endif

const uint8 ofdm_rate_lookup[] = {
	    /* signal */
	96, /* 8: 48Mbps */
	48, /* 9: 24Mbps */
	24, /* A: 12Mbps */
	12, /* B:  6Mbps */
	108, /* C: 54Mbps */
	72, /* D: 36Mbps */
	36, /* E: 18Mbps */
	18  /* F:  9Mbps */
};

const uint8 *
wlc_phy_get_ofdm_rate_lookup(void)
{
	return ofdm_rate_lookup;
}


void
wlc_bmac_rpc_watchdog(wlc_info_t *wlc)
{
	bcm_rpc_watchdog(wlc->rpc);
}

si_t *
BCMATTACHFN(wlc_bmac_si_attach)(uint device, osl_t *osh, void *regsva, uint bustype,
	void *btparam, char **vars, uint *varsz)
{
	si_t *sih;

	/* Allocate SB handle */
	sih = MALLOC(osh, sizeof(si_t));
	if (sih == NULL)
		return NULL;
	bzero(sih, sizeof(si_t));

	*vars = NULL;
	*varsz = 0;

	return sih;
}

void
BCMATTACHFN(wlc_bmac_si_detach)(osl_t *osh, si_t *sih)
{
	if (sih) {
		MFREE(osh, (void *)sih, sizeof(si_t));
	}
}

/* HIGH_ONLY bmac_attach, which sync over LOW_ONLY bmac_attach states */
int
BCMATTACHFN(wlc_bmac_attach)(wlc_info_t *wlc, uint16 vendor, uint16 device, uint unit,
	bool piomode, osl_t *osh, void *regsva, uint bustype, void *btparam)
{
	wlc_bmac_revinfo_t revinfo;
	uint idx = 0;
	rpc_info_t *rpc = (rpc_info_t*)btparam;
	wlc_hw_info_t *wlc_hw;
	uint err = 0;
	wlc_pub_t *pub = wlc->pub;
	wlc_tunables_t *tunables = pub->tunables;
	si_t *sih = pub->sih;

	ASSERT(bustype == RPC_BUS);

	if ((wlc_hw = wlc_hw_attach(wlc, osh, unit, &err)) == NULL)
		return err;

	wlc->hw = wlc_hw;

	/* install the rpc handle in the various state structures used by stub RPC functions */
	wlc->rpc = rpc;
	wlc_hw->rpc = rpc;
	wlc_hw->_piomode = piomode;

	if ((wlc->rpctx = wlc_rpctx_attach(pub, wlc)) == NULL)
		return -1;

	/*
	 * FIFO 0
	 * TX: TX_AC_BK_FIFO (TX AC Background data packets)
	 */
	/* Always initialized */
	ASSERT(tunables->rpctxbufpost <= tunables->ntxd);
	wlc_rpctx_fifoinit(wlc->rpctx, TX_DATA_FIFO, tunables->rpctxbufpost);
	wlc_rpctx_fifoinit(wlc->rpctx, TX_CTL_FIFO, tunables->rpctxbufpost);
	wlc_rpctx_fifoinit(wlc->rpctx, TX_BCMC_FIFO, tunables->rpctxbufpost);

	/* VI and BK inited only if WME */
	if (WME_ENAB(pub)) {
		wlc_rpctx_fifoinit(wlc->rpctx, TX_AC_BK_FIFO, tunables->rpctxbufpost);
		wlc_rpctx_fifoinit(wlc->rpctx, TX_AC_VI_FIFO, tunables->rpctxbufpost);
	}

#if defined(MBSS)
	/* Bcn/Proberesp pkts */
	wlc_rpctx_fifoinit(wlc->rpctx, TX_ATIM_FIFO, tunables->rpctxbufpost);
#endif

	/* sync up revinfo with BMAC */
	bzero(&revinfo, sizeof(wlc_bmac_revinfo_t));
	if (wlc_bmac_revinfo_get(wlc_hw, &revinfo) != 0)
		return -1;
	wlc->vendorid = (uint16)revinfo.vendorid;
	wlc->deviceid = (uint16)revinfo.deviceid;

	pub->boardrev = (uint16)revinfo.boardrev;
	pub->corerev = revinfo.corerev;
	pub->sromrev = (uint8)revinfo.sromrev;
	sih->chiprev = revinfo.chiprev;
	sih->chip = revinfo.chip;
	sih->chippkg = revinfo.chippkg;
	sih->boardtype = revinfo.boardtype;
	sih->boardvendor = revinfo.boardvendor;
	sih->bustype = revinfo.bustype;
	sih->buscoretype = revinfo.buscoretype;
	sih->buscorerev = revinfo.buscorerev;
	sih->issim = (bool)revinfo.issim;
	sih->rpc = rpc;

	if (revinfo.nbands == 0 || revinfo.nbands > 2)
		return -1;
	pub->_nbands = revinfo.nbands;

	wlc->pub->boardflags = revinfo.boardflags;
	wlc->pub->boardflags2 = revinfo.boardflags2;

	for (idx = 0; idx < pub->_nbands; idx++) {
		uint bandunit, bandtype; /* To access bandstate */
		wlcband_t *band;
		wlc_phy_t *pi = MALLOC(osh, sizeof(wlc_phy_t));

		if (!pi)
			return -1;
		bzero(pi, sizeof(wlc_phy_t));
		pi->rpc = rpc;

		/* Use band 1 for single band 11a */
		if (IS_SINGLEBAND_5G(wlc->deviceid))
			idx = BAND_5G_INDEX;

		bandunit = revinfo.band[idx].bandunit;
		bandtype = revinfo.band[idx].bandtype;
		band = wlc->bandstate[bandunit];
		band->radiorev = (uint8)revinfo.band[idx].radiorev;
		band->phytype = (uint16)revinfo.band[idx].phytype;
		band->phyrev = (uint16)revinfo.band[idx].phyrev;
		band->radioid = (uint16)revinfo.band[idx].radioid;
		band->abgphy_encore = revinfo.band[idx].abgphy_encore;

		band->pi = pi;
		band->bandunit = bandunit;
		band->bandtype = bandtype;
	}

	/* Initialize template config variables */
	wlc_template_cfg_init(wlc, pub->corerev);

	/* misc stuff */

#ifdef MBSS
	/* MBSS4 requires SW PRQ processing, which is not supported by bmac driver */
	if (D11REV_ISMBSS4(pub->corerev)) {
		WL_ERROR(("bmac driver doesn't support MBSS4"));
		return -1;
	}

	/* This needs to be the same with the one in wlc_bmac.c */
	if (D11REV_ISMBSS16(pub->corerev)) {
		wlc->max_ap_bss = tunables->maxucodebss;

		/* 4313 has total fifo space of 128 blocks. if we enable
		 * all 16 MBSSs we will not be left with enough fifo space to
		 * support max thru'put. so we only allow configuring/enabling
		 * max of 4 BSSs. Rest of the space is distributed acorss
		 * the tx fifos.
		 */
#ifdef WLLPRS
		/* To support legacy prs of size > 256bytes, reduce the no. of
		 * bss supported to 8.
		 */
		if (D11REV_IS(pub->corerev, 16) || D11REV_IS(pub->corerev, 17) ||
			D11REV_IS(pub->corerev, 22)) {

			wlc->max_ap_bss = 8;
			tunables->maxucodebss = 8;
		}
#endif /* WLLPRS */

		if (D11REV_IS(pub->corerev, 24) || D11REV_IS(pub->corerev, 25)) {
			wlc->max_ap_bss = 4;
			wlc->pub->tunables->maxucodebss = 4;
		}

		wlc->mbss_ucidx_mask = wlc->max_ap_bss - 1;
	}
#endif /* MBSS */

	return 0;
}

/* Free the convenience handles */
int
BCMATTACHFN(wlc_bmac_detach)(wlc_info_t *wlc)
{
	uint idx;

	for (idx = 0; idx < MAXBANDS; idx++)
		if (wlc->bandstate[idx]->pi) {
			MFREE(wlc->osh, wlc->bandstate[idx]->pi, sizeof(wlc_phy_t));
			wlc->bandstate[idx]->pi = NULL;
		}

	if (wlc->rpctx) {
		wlc_rpctx_detach(wlc->rpctx);
		wlc->rpctx = NULL;
	}

	wlc_hw_detach(wlc->hw);
	wlc->hw = NULL;

	return 0;

}

/* ============= BMAC->HOST =================== */

extern void wlc_rpc_txstatus_parse(tx_status_t *txs, wlc_pub_t *pub);
void wlc_rpc_txstatus_parse(tx_status_t *txs, wlc_pub_t *pub)
{
	uint16	status_bits = txs->status.raw_bits;
	uint16 ncons;

	if (D11REV_LT(pub->corerev, 40)) {
		txs->status.was_acked = (status_bits & TX_STATUS_ACK_RCV) != 0;
		txs->status.is_intermediate = (status_bits & TX_STATUS_INTERMEDIATE) != 0;
		txs->status.pm_indicated = (status_bits & TX_STATUS_PMINDCTD) != 0;
		txs->status.suppr_ind =
			(status_bits & TX_STATUS_SUPR_MASK) >> TX_STATUS_SUPR_SHIFT;
		txs->status.frag_tx_cnt =
			(status_bits & TX_STATUS_FRM_RTX_MASK) >> TX_STATUS_FRM_RTX_SHIFT;
		txs->status.rts_tx_cnt =
			(status_bits & TX_STATUS_RTS_RTX_MASK) >> TX_STATUS_RTS_RTX_SHIFT;
	} else {
		ncons = ((status_bits & TX_STATUS40_NCONS) >> TX_STATUS40_NCONS_SHIFT);
		txs->status.was_acked = ((ncons <= 1) ?
			((status_bits & TX_STATUS40_ACK_RCV) != 0) : TRUE);
		txs->status.is_intermediate = (status_bits & TX_STATUS40_INTERMEDIATE) != 0;
		txs->status.pm_indicated = (status_bits & TX_STATUS40_PMINDCTD) != 0;
		txs->status.suppr_ind =
			(status_bits & TX_STATUS40_SUPR) >> TX_STATUS40_SUPR_SHIFT;
		txs->status.frag_tx_cnt = TX_STATUS40_TXCNT(txs->status.s3, txs->status.s4);
		txs->status.rts_tx_cnt =
			((txs->status.s5 & TX_STATUS40_RTS_RTX_MASK) >> TX_STATUS40_RTS_RTX_SHIFT);
		txs->status.cts_rx_cnt =
			((txs->status.s5 & TX_STATUS40_CTS_RRX_MASK) >> TX_STATUS40_CTS_RRX_SHIFT);
	}
}

/* this dispatcher can be moved to a new file */
void BCMFASTPATH
wlc_rpc_high_dispatch(wlc_rpc_ctx_t *rpc_ctx, struct rpc_buf* buf)
{
	bcm_xdr_buf_t b;
	wlc_rpc_id_t rpc_id;
	int err;
	rpc_info_t *rpc = rpc_ctx->rpc;
	rpc_tp_info_t *rpcb = bcm_rpc_tp_get(rpc);
	wlc_info_t *wlc = rpc_ctx->wlc;

	if (!wlc->pub->up) {
		WL_ERROR(("%s: Driver down \n", __FUNCTION__));
		goto fail;
	}

	ASSERT(rpc);
	bcm_xdr_buf_init(&b, bcm_rpc_buf_data(rpcb, buf),
	                 bcm_rpc_buf_len_get(rpcb, buf));

	err = bcm_xdr_unpack_uint32(&b, (uint32*)&rpc_id);
	ASSERT(!err);

	WL_TRACE(("%s: Dispatch id %s\n", __FUNCTION__, WLC_RPC_ID_LOOKUP(rpc_name_tbl, rpc_id)));

	if (rpc_id == WLRPC_WLC_RESET_BMAC_DONE_ID) {
		wlc_reset_bmac_done(rpc_ctx->wlc);
	}
	if (wlc->reset_bmac_pending) {
		goto fail;
	}

	/* Handle few emergency ones */
	switch (rpc_id) {
#ifdef WLRM
	case WLRPC_WLC_RM_CCA_COMPLETE_ID: {
		uint32 cca_idle_us;

		err = bcm_xdr_unpack_uint32(&b, &cca_idle_us);
		ASSERT(!err);

		wlc_rm_cca_complete(wlc, cca_idle_us);
		break;
	}

#endif /* WLRM */

	case WLRPC_WLC_RECV_ID: {

#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY) || defined(BCM_RPC_ROC)
		void *p;
		uint len;
		err = bcm_xdr_unpack_uint32(&b, &len);
		ASSERT(!err);
		/* pass over the length and RPC header */
		p = (void *)buf;
		PKTPULL(wlc->osh, p, 2 * sizeof(uint32));
		/* set the correct length */
		PKTSETLEN(wlc->osh, p, len);

		/* process receive */
		wlc_recv(wlc, p);
		/* set buf to NULL so we don't free it */
		buf = NULL;
		break;
#else
		void *xdr_data;
		void *p;
		uint len;

		err = bcm_xdr_unpack_opaque_varlen(&b, &len, &xdr_data);
		ASSERT(!err);

		/* allocate a pkt, copy over RPC_BUFFER_RX */
		p = PKTGET(wlc->osh, len, FALSE);
		if (p == NULL) {
			WL_ERROR(("%s: PKTGET failed\n", __FUNCTION__));
			break;
		}

		memcpy(PKTDATA(wlc->osh, p), xdr_data, len);
		wlc_recv(wlc, p);

		break;

#endif /*  BCM_RPC_NOCOPY || BCM_RPC_RXNOCOPY || defined(BCM_RPC_ROC) */
	}
	case WLRPC_WLC_DOTXSTATUS_ID: {
		wlc_rpc_txstatus_t rpc_txstatus;
		tx_status_t txs;
		uint32 frm_tx2;
		wlc_rpc_id_t next_id;
		bool fatal;

		bzero(&rpc_txstatus, sizeof(wlc_rpc_txstatus_t));
		bzero(&txs, sizeof(tx_status_t));

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.frameid_framelen);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.sequence_status);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.lasttxtime);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.ackphyrxsh_phyerr);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &frm_tx2);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.s3);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.s4);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.s5);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.s6);
		ASSERT(!err);

		err = bcm_xdr_unpack_uint32(&b, &rpc_txstatus.s7);
		ASSERT(!err);

		rpc_txstatus2txstatus(&rpc_txstatus, &txs);
		wlc_rpc_txstatus_parse(&txs, wlc->pub);

		fatal = wlc_dotxstatus(wlc, &txs, frm_tx2);
		if (fatal)
			break;

		err = bcm_xdr_unpack_uint32(&b, (uint32*)&next_id);
		ASSERT(!err);

		if (next_id == WLRPC_WLC_AMPDU_TXSTATUS_COMPLETE_ID) {
			uint32 s1, s2;

			err = bcm_xdr_unpack_uint32(&b, &s1);
			ASSERT(!err);

			err = bcm_xdr_unpack_uint32(&b, &s2);
			ASSERT(!err);

			wlc_ampdu_txstatus_complete(wlc->ampdu_tx, s1, s2);
			break;
		}

		break;
	}

	case WLRPC_WLC_HIGH_DPC_ID: {
		uint32 macintstatus;

		err = bcm_xdr_unpack_uint32(&b, &macintstatus);
		ASSERT(!err);

		if (macintstatus == 0) {
			WL_ERROR(("wlc_rpc_high_dispatch: toss dummy high_dpc call\n"));
			break;
		}

		wlc_high_dpc(wlc, macintstatus);
		break;
	}

	case WLRPC_WLC_FATAL_ERROR_ID: {
		WL_ERROR(("wl%d: %s: RPC fatal error HAMMERING\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_fatal_error(wlc);
		break;
	}

	case WLRPC_WLC_NOISE_CB_ID: {
		int8 channel, noise_dbm;

		err = bcm_xdr_unpack_int8(&b, &channel);
		ASSERT(!err);

		err = bcm_xdr_unpack_int8(&b, &noise_dbm);
		ASSERT(!err);

		wlc_lq_noise_cb(rpc_ctx->wlc, channel, noise_dbm);
		break;
	}

	case WLRPC_WLC_UPDATE_PHY_MODE_ID: {
		uint32 phy_mode;

		err = bcm_xdr_unpack_int32(&b, &phy_mode);
		ASSERT(!err);

		wlc_update_phy_mode(rpc_ctx->wlc, phy_mode);
		break;
	}
	case WLRPC_WLC_BMAC_PS_SWITCH_ID: {
		struct ether_addr ea;
		int8 ps_flags;

		err = bcm_xdr_unpack_int8(&b, &ps_flags);
		ASSERT(!err);

		if (!(ps_flags & 0x40)) {
			err = bcm_xdr_unpack_opaque_cpy(&b, sizeof(struct ether_addr), &ea);
			ASSERT(!err);
		}

		wlc_apps_process_ps_switch(wlc, &ea, ps_flags);

		break;
	}

#ifdef WLEXTLOG
	case WLRPC_WLC_EXTLOG_MSG_ID: {
		uint32 val;
		uint16 module;
		uint8 id;
		uint8 level;
		uint8 sub_unit;
		int32 arg;
		uint len;
		void *pdata;

		err = bcm_xdr_unpack_uint32(&b, &val);
		ASSERT(!err);
		module = (uint16)val;

		err = bcm_xdr_unpack_uint32(&b, &val);
		ASSERT(!err);
		id = (uint8)val;

		err = bcm_xdr_unpack_uint32(&b, &val);
		ASSERT(!err);
		level = (uint8)val;

		err = bcm_xdr_unpack_uint32(&b, &val);
		ASSERT(!err);
		sub_unit = (uint8)val;

		err = bcm_xdr_unpack_uint32(&b, &val);
		ASSERT(!err);
		arg = val;

		err = bcm_xdr_unpack_opaque_varlen(&b, &len, &pdata);

		WL_TRACE(("%s(): module=%d, id=%d, level=%d, sub_unit=%d, arg=%d, str=%s\n",
			__FUNCTION__, module, id, level, sub_unit, arg,
			(len != 0) ? (char*)pdata : NULL));

		wlc_extlog_msg(wlc, module, id, level, sub_unit, arg,
			(len != 0) ? pdata : NULL);

		break;
	}
#endif /* WLEXTLOG */


		case WLRPC_WLC_BMAC_TXPPR_ID: {
#ifdef PPR_API
			ppr_t* txppr;
			void* bptr;
			uint len;
#else
			txppr_t txpwr;
#endif

#ifdef PPR_API
			err = bcm_xdr_unpack_opaque_varlen(&b, &len, &bptr);
			ASSERT(!err);

			if ((ppr_deserialize_create(wlc->osh, bptr, len, &txppr)) == BCME_OK) {
				wlc_update_txppr_offset(wlc, txppr);
				ppr_delete(wlc->osh, txppr);
			}
#else
			err = bcm_xdr_unpack_opaque_cpy(&b, sizeof(txpwr), (void**)&txpwr);
			ASSERT(!err);

			wlc_update_txppr_offset(wlc, (int8 *)&txpwr);
#endif
			break;
		}

		case WLRPC_WLC_BMAC_TX_FIFO_SYNC_COMPLETE_ID: {
#ifdef WL_MULTIQUEUE
			uint32 val32;
			uint fifo_bitmap;
			uint8 flag;

			err = bcm_xdr_unpack_uint32(&b, &val32);
			ASSERT(!err);
			fifo_bitmap = val32;

			err = bcm_xdr_unpack_uint32(&b, &val32);
			ASSERT(!err);
			flag = (uint8)val32;

			wlc_tx_fifo_sync_complete(wlc, fifo_bitmap, flag);
#endif /* WL_MULTIQUEUE */
			break;
		}

		case WLRPC_WLC_P2P_INT_PROC_ID: {
#ifdef WLMCNX
			uint8 p2p_interrupts[M_P2P_BSS_MAX];
			uint32 tsf_l, tsf_h;
#ifdef WLMEDIA_LG
			unsigned long flags;
#endif /* WLMEDIA_LG */

			err = bcm_xdr_unpack_uint32(&b, &tsf_l);
			ASSERT(!err);
			err = bcm_xdr_unpack_uint32(&b, &tsf_h);
			ASSERT(!err);
			err = bcm_xdr_unpack_uint8_vec(&b, p2p_interrupts, M_P2P_BSS_MAX);
			ASSERT(!err);

#ifdef WLMEDIA_LG
			/* Disable interrupt so p2p intterupt timing won't be affected */
			local_irq_save(flags);
#endif /* WLMEDIA_LG */
			wlc_p2p_int_proc(wlc, p2p_interrupts, tsf_l, tsf_h);
#ifdef WLMEDIA_LG
			local_irq_restore(flags);
#endif /* WLMEDIA_LG */
#endif /* WLMCNX */
		}

		default:
			break;
	}

	/* send any enq'd tx packets. Just makes sure to jump start tx */
	/* In regular NIC, this happens at the end of wlc_txstatus, for
	 * HIGH ONLY, there is no such thing. So do it here
	 */
	if (!pktq_empty(&wlc->active_queue->q))
		wlc_send_q(wlc, wlc->active_queue);

	/* RPC_BUFFER_RX: deallocate after being processed */
fail:
	if (buf) {
#if defined(BCM_RPC_NOCOPY) || defined(BCM_RPC_RXNOCOPY) || defined(BCM_RPC_ROC)
		PKTFREE(wlc->osh, buf, FALSE);
#else
		bcm_rpc_buf_free(rpc, buf);
#endif
	}
}


/* ============= HOST->BMAC =================== */
static rpc_buf_t *
wlc_rpc_call_with_return(rpc_info_t *rpc, rpc_buf_t *send, bcm_xdr_buf_t *retb)
{
	rpc_buf_t *ret_buf = NULL;
	rpc_tp_info_t *rpcb = bcm_rpc_tp_get(rpc);
	wlc_rpc_id_t ret_rpc_id;
	int err;
#ifdef WLMEDIA_LG
	int irq_off = irqs_disabled();
#endif /* WLMEDIA_LG */

#if defined(BCMDBG)
	wlc_rpc_id_t rpc_id = wlc_rpc_id_get(rpc, send);

	WL_TRACE(("%s: Called id %s\n", __FUNCTION__, WLC_RPC_ID_LOOKUP(rpc_name_tbl, rpc_id)));
#endif /* defined(BCMDBG) */

#ifdef WLMEDIA_LG
	if (irq_off)
		local_irq_enable();
#endif /* WLMEDIA_LG */
	ret_buf = bcm_rpc_call_with_return(rpc, send);
#ifdef WLMEDIA_LG
	if (irq_off)
		local_irq_disable();
#endif /* WLMEDIA_LG */

	if (!ret_buf) {
		WL_ERROR(("%s: Call with return id %s FAILED!\n",
		          __FUNCTION__, WLC_RPC_ID_LOOKUP(rpc_name_tbl, rpc_id)));
		return NULL;
	}

	bcm_xdr_buf_init(retb, bcm_rpc_buf_data(rpcb, ret_buf),
	                 bcm_rpc_buf_len_get(rpcb, ret_buf));

	/* pull the FN ID off the head */
	err = bcm_xdr_unpack_uint32(retb, (uint32 *)&ret_rpc_id);
	ASSERT(!err);

	/* Make sure that returned id is the same */
	WL_TRACE(("%s: Ret id %s\n", __FUNCTION__, WLC_RPC_ID_LOOKUP(rpc_name_tbl, ret_rpc_id)));

#if defined(BCMDBG)
	ASSERT(ret_rpc_id == rpc_id);
#endif

	return ret_buf;
}

/* ================================
 * Low stub functions
 * ================================
 */

uint32
wlc_reg_read(wlc_info_t *wlc, void *r, uint size)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = wlc->rpc;
	uint32 ret;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_REG_READ_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)(uintptr)r);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, size);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

void
wlc_reg_write(wlc_info_t *wlc, void *r, uint32 v, uint size)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	rpc_info_t *rpc = wlc->rpc;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 3, WLRPC_WLC_REG_WRITE_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)(uintptr)r);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, v);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, size);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_mhf(wlc_hw_info_t *wlc_hw, uint8 idx, uint16 mask, uint16 val, int bands)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 4, WLRPC_WLC_MHF_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, idx);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, mask);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, val);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, bands);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

uint16
wlc_bmac_mhf_get(wlc_hw_info_t *wlc_hw, uint8 idx, int bands)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_MHF_GET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, idx);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, bands);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return 0;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (uint16)ret;
}

void
wlc_bmac_reset(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_RESET_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_dngl_reboot(rpc_info_t *rpc)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_DNGL_REBOOT_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_dngl_rpc_agg(rpc_info_t *rpc, uint16 agg)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_RPC_AGG_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, agg);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_dngl_rpc_msglevel(rpc_info_t *rpc, uint16 level)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_RPC_MSGLEVEL_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, level);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_dngl_rpc_txq_wm_set(rpc_info_t *rpc, uint32 wm)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	uint8 hiwm, lowm;

	hiwm = wm >> 16;
	lowm = wm & 0xffff;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_BMAC_RPC_TXQ_WM_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, hiwm);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, lowm);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_dngl_rpc_txq_wm_get(rpc_info_t *rpc, uint32 *wm)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 uret;
	uint8 hiwm, lowm;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_RPC_TXQ_WM_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL) {
		*wm = 0;
		return;
	}

	err = bcm_xdr_unpack_uint32(&retb, &uret);
	ASSERT(!err);
	hiwm = (uint8)uret;

	err = bcm_xdr_unpack_uint32(&retb, &uret);
	ASSERT(!err);
	lowm = (uint8)uret;

	*wm = (hiwm << 16) + (lowm & 0xffff);

	bcm_rpc_buf_free(rpc, ret_buf);
}

void
wlc_bmac_dngl_rpc_agg_limit_set(rpc_info_t *rpc, uint32 val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	uint8 sf;
	uint16 bytes;

	sf = val >> 16;
	bytes = val & 0xffff;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2,
		WLRPC_WLC_BMAC_RPC_AGG_LIMIT_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, sf);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, bytes);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_dngl_rpc_agg_limit_get(rpc_info_t *rpc, uint32 *pval)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 uret;
	uint8 sf;
	uint16 bytes;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_RPC_AGG_LIMIT_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL) {
		*pval = 0;
		return;
	}

	err = bcm_xdr_unpack_uint32(&retb, &uret);
	ASSERT(!err);
	sf = (uint8)uret;

	err = bcm_xdr_unpack_uint32(&retb, &uret);
	ASSERT(!err);
	bytes = (uint16)uret;

	*pval = (sf << 16) + (bytes & 0xffff);

	bcm_rpc_buf_free(rpc, ret_buf);
}

void
wlc_bmac_hw_up(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_HW_UP_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

int
wlc_bmac_4331_epa_init(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_4331_EPA_INIT_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return failure */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

void
wlc_bmac_set_extlna_pwrsave_shmem(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_SET_EXTLNA_PWRSAVE_SHMEM_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

int
wlc_bmac_up_prep(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_UP_PREP_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return failure */
	if (ret_buf == NULL)
		return BCME_RADIOOFF;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

int
wlc_bmac_up_finish(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_UP_FINISH_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return failure */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

int
wlc_bmac_set_ctrl_ePA(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_SET_CTRL_EPA_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return failure */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

int
wlc_bmac_set_ctrl_SROM(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_SET_CTRL_SROM_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return failure */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

int
wlc_bmac_set_ctrl_bt_shd0(wlc_hw_info_t *wlc_hw, bool enable)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_SET_CTRL_BT_SHD0_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)(enable ? 1:0));
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return failure */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

int
wlc_bmac_set_btswitch(wlc_hw_info_t *wlc_hw, int8 state)
{
	return BCME_UNSUPPORTED;
}

int
wlc_bmac_down_prep(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	if (DEVICEREMOVED(wlc_hw->wlc))
		return 0;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_DOWN_PREP_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return 0 callbacks */
	if (ret_buf == NULL)
		return 0;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

int
wlc_bmac_down_finish(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	if (DEVICEREMOVED(wlc_hw->wlc))
		return 0;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_DOWN_FINISH_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return failure */
	if (ret_buf == NULL)
		return 0;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

int
wlc_bmac_dngl_wd_keep_alive(wlc_hw_info_t *wlc_hw, uint32 delay)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err = 0;
	int32 ret = 0;
	rpc_info_t *rpc = wlc_hw->rpc;

	if (DEVICEREMOVED(wlc_hw->wlc))
		return 0;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_DNGL_WD_KEEP_ALIVE_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)delay);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return failure */
	if (ret_buf == NULL)
		return 0;


	bcm_rpc_buf_free(rpc, ret_buf);
	return (int)ret;
}

void
wlc_bmac_init(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool mute, uint32 defmacintmask)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 3 * sizeof(uint32), WLRPC_WLC_BMAC_INIT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)chanspec);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, (uint32)mute);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, defmacintmask);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_set_cwmin(wlc_hw_info_t *wlc_hw, uint16 newmin)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_SET_CWMIN_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, newmin);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_mute(wlc_hw_info_t *wlc_hw, bool on, mbool flags)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_MUTE_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, on);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_set_deaf(wlc_hw_info_t *wlc_hw, bool user_flag)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_SET_DEAF);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, user_flag);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
#if defined(WLTEST)
void
wlc_bmac_clear_deaf(wlc_hw_info_t *wlc_hw, bool user_flag)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_CLEAR_DEAF);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, user_flag);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
#endif 

int
wlc_bmac_phy_iovar_dispatch(wlc_hw_info_t *wlc_hw, uint32 actionid, uint16 type, void *p, uint plen,
	void *a, int alen, int vsize)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err, ret;
	rpc_info_t *rpc = wlc_hw->rpc;
	bool buf32 = FALSE, buf16 = FALSE;

	WL_TRACE(("%s: actionid %d plen %d alen %d\n", __FUNCTION__, actionid, plen, alen));

	if (plen > 1024)
		plen = 1024;

	if (alen > 1024)
		alen = 1024;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2*sizeof(uint32) + 2*sizeof(int32) +
		ROUNDUP(plen, sizeof(uint32)), WLRPC_WLC_PHY_DOIOVAR_ID);
	if (rpc_buf == NULL) {
		WL_ERROR(("%s: wlc_rpc_buf_allof %d bytes failed. actionid %d\n", __FUNCTION__,
			(uint)(2 * sizeof(uint32) + 2 * sizeof(int32) +
			ROUNDUP(plen, sizeof(uint32))),	actionid));
		return BCME_NOMEM;
	}

	err = bcm_xdr_pack_uint32(&b, actionid);
	ASSERT(!err);

	/* Decide the buffer is 16-bit or 32-bit buffer */
	switch (IOV_ID(actionid)) {
		case IOV_PKTENG_STATS:
		case IOV_PHYTABLE:
		case IOV_POVARS:
			buf32 = TRUE;
			break;
		case IOV_PAVARS:
			buf16 = TRUE;
			break;
		default:
			buf16 = FALSE;
			buf32 = FALSE;
			break;
	}

	/* Dealing with all the structures/special cases */
	switch (actionid) {
		case IOV_SVAL(IOV_RADAR_ARGS):
		case IOV_SVAL(IOV_RADAR_ARGS_40MHZ): {
			wl_radar_args_t	*radar = (wl_radar_args_t*)p;

			radar->npulses = htol32(radar->npulses);
			radar->ncontig = htol32(radar->ncontig);
			radar->min_pw = htol32(radar->min_pw);
			radar->max_pw = htol32(radar->max_pw);
			radar->thresh0 = htol16(radar->thresh0);
			radar->thresh1 = htol16(radar->thresh1);
			radar->blank = htol16(radar->blank);
			radar->fmdemodcfg = htol16(radar->fmdemodcfg);
			radar->npulses_lp = htol32(radar->npulses_lp);
			radar->min_pw_lp = htol32(radar->min_pw_lp);
			radar->max_pw_lp = htol32(radar->max_pw_lp);
			radar->min_fm_lp = htol32(radar->min_fm_lp);
			radar->max_span_lp = htol32(radar->max_span_lp);
			radar->min_deltat = htol32(radar->min_deltat);
			radar->max_deltat = htol32(radar->max_deltat);
			radar->autocorr = htol16(radar->autocorr);
			radar->st_level_time = htol16(radar->st_level_time);
			radar->t2_min = htol16(radar->t2_min);
			radar->version = htol32(radar->version);
			radar->fra_pulse_err = htol32(radar->fra_pulse_err);
			radar->npulses_fra = htol32(radar->npulses_fra);
			radar->npulses_stg2 = htol32(radar->npulses_stg2);
			radar->npulses_stg3 = htol32(radar->npulses_stg3);
			radar->percal_mask = htol16(radar->percal_mask);
			radar->quant = htol32(radar->quant);
			radar->min_burst_intv_lp = htol32(radar->min_burst_intv_lp);
			radar->max_burst_intv_lp = htol32(radar->max_burst_intv_lp);
			radar->nskip_rst_lp = htol32(radar->nskip_rst_lp);
			radar->max_pw_tol = htol32(radar->max_pw_tol);
			radar->feature_mask = htol16(radar->feature_mask);
			break;
		}
		case IOV_SVAL(IOV_RADAR_THRS): {
			wl_radar_thr_t	*thr = (wl_radar_thr_t*)p;
			thr->version = htol32(thr->version);
			thr->thresh0_20_lo = htol32(thr->thresh0_20_lo);
			thr->thresh1_20_lo = htol16(thr->thresh1_20_lo);
			thr->thresh0_40_lo = htol16(thr->thresh0_40_lo);
			thr->thresh1_40_lo = htol16(thr->thresh1_40_lo);
			thr->thresh0_20_hi = htol16(thr->thresh0_20_hi);
			thr->thresh1_20_hi = htol16(thr->thresh1_20_hi);
			thr->thresh0_40_hi = htol16(thr->thresh0_40_hi);
			thr->thresh1_40_hi = htol16(thr->thresh1_40_hi);
			break;
		}
	}

	/* Take care of endianess for 16 and 32 bit input */
	if (buf16) {
		plen &= ~0x1;
		err = bcm_xdr_pack_uint32(&b, plen);
		ASSERT(!err);

		err = bcm_xdr_pack_uint16_vec(&b, plen, p);
	} else if (buf32 || (type == IOVT_INT32) || (type == IOVT_UINT32) ||
		(type == IOVT_INT8) || (type == IOVT_UINT8) ||
		(type == IOVT_INT16) || (type == IOVT_UINT16) || (type == IOVT_BOOL)) {
		plen &= ~0x3;
		err = bcm_xdr_pack_uint32(&b, plen);
		ASSERT(!err);

		err = bcm_xdr_pack_uint32_vec(&b, plen, p);
	} else {
		err = bcm_xdr_pack_uint32(&b, plen);
		ASSERT(!err);

		err = bcm_xdr_pack_opaque(&b, plen, p);
	}
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, alen);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, vsize);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	/* Convert back from little endian */
	if ((type == IOVT_INT16) || (type == IOVT_UINT16) ||
		(type == IOVT_INT32) || (type == IOVT_UINT32) ||
		(type == IOVT_INT8) || (type == IOVT_UINT8) || (type == IOVT_BOOL))
		err = bcm_xdr_unpack_uint32(&retb, (uint32*)a);
	else if (buf32)
		err = bcm_xdr_unpack_uint32_vec(&retb, alen, a);
	else if (buf16)
		err = bcm_xdr_unpack_uint16_vec(&retb, alen, a);
	else
		err = bcm_xdr_unpack_opaque_cpy(&retb, alen, a);
	ASSERT(!err);

	/* Dealing with all the structures/special cases */
	switch (actionid) {
		case IOV_GVAL(IOV_RADAR_ARGS):
		case IOV_GVAL(IOV_RADAR_ARGS_40MHZ): {
			wl_radar_args_t	*radar = (wl_radar_args_t*)a;

			radar->npulses = ltoh32(radar->npulses);
			radar->ncontig = ltoh32(radar->ncontig);
			radar->min_pw = ltoh32(radar->min_pw);
			radar->max_pw = ltoh32(radar->max_pw);
			radar->thresh0 = ltoh16(radar->thresh0);
			radar->thresh1 = ltoh16(radar->thresh1);
			radar->blank = ltoh16(radar->blank);
			radar->fmdemodcfg = ltoh16(radar->fmdemodcfg);
			radar->npulses_lp = ltoh32(radar->npulses_lp);
			radar->min_pw_lp = ltoh32(radar->min_pw_lp);
			radar->max_pw_lp = ltoh32(radar->max_pw_lp);
			radar->min_fm_lp = ltoh32(radar->min_fm_lp);
			radar->max_span_lp = ltoh32(radar->max_span_lp);
			radar->min_deltat = ltoh32(radar->min_deltat);
			radar->max_deltat = ltoh32(radar->max_deltat);
			radar->autocorr = ltoh16(radar->autocorr);
			radar->st_level_time = ltoh16(radar->st_level_time);
			radar->t2_min = ltoh16(radar->t2_min);
			radar->version = ltoh32(radar->version);
			radar->fra_pulse_err = ltoh32(radar->fra_pulse_err);
			radar->npulses_fra = ltoh32(radar->npulses_fra);
			radar->npulses_stg2 = ltoh32(radar->npulses_stg2);
			radar->npulses_stg3 = ltoh32(radar->npulses_stg3);
			radar->percal_mask = ltoh16(radar->percal_mask);
			radar->quant = ltoh32(radar->quant);
			radar->min_burst_intv_lp = ltoh32(radar->min_burst_intv_lp);
			radar->max_burst_intv_lp = ltoh32(radar->max_burst_intv_lp);
			radar->nskip_rst_lp = ltoh32(radar->nskip_rst_lp);
			radar->max_pw_tol = ltoh32(radar->max_pw_tol);
			radar->feature_mask = ltoh16(radar->feature_mask);
			break;
		}
		case IOV_GVAL(IOV_PHY_SAMPLE_COLLECT):
			WL_ERROR(("%s: nphy_sample_collect need endianess conversion code\n",
				__FUNCTION__));
			break;
		case IOV_GVAL(IOV_PHY_SAMPLE_DATA):
			WL_ERROR(("%s: nphy_sample_data need endianess conversion code\n",
				__FUNCTION__));
			break;
	}

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return ret;
}

void
wlc_phy_hold_upd(wlc_phy_t *ppi, mbool id, bool val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_PHY_HOLD_UPD_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, id);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, val);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

bool
wlc_phy_ismuted(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	uint32 ret;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_ISMUTED_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return FALSE;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (bool)ret;
}

uint32
wlc_phy_cap_get(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	uint32 ret;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_CAP_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return FALSE;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

void
wlc_phy_mute_upd(wlc_phy_t *ppi, bool val, mbool flags)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_PHY_MUTE_UPD_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, val);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, flags);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_clear_tssi(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_CLEAR_TSSI_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

bool
wlc_phy_ant_rxdiv_get(wlc_phy_t *ppi, uint8 *pval)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	uint32 ret;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_ANT_RXDIV_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return FALSE;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	*pval = (uint8)ret;
	bcm_rpc_buf_free(rpc, ret_buf);

	return (bool)ret;
}

void
wlc_phy_ant_rxdiv_set(wlc_phy_t *ppi, uint8 val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_ANT_RXDIV_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, val);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}


void
wlc_phy_preamble_override_set(wlc_phy_t *ppi, int8 val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_PREAMBLE_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int8(&b, val);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

/* BMAC_NOTE: Following functions should not get executed for 4322, so are considered
 * safe to put in low driver w/o concern for latency
 */
void
wlc_phy_freqtrack_end(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_FREQTRACK_END_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_freqtrack_start(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_FREQTRACK_START_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

int
wlc_phy_ioctl(wlc_phy_t *ppi, int cmd, int len, void *arg, bool *ta_ok)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	bcm_xdr_buf_t retb;
	rpc_buf_t *ret_buf = NULL;
	int ret;
	uint retlen;

	ASSERT(arg || !len);

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2*sizeof(int) + ROUNDUP(len, sizeof(uint32)),
		WLRPC_WLC_PHY_IOCTL_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, cmd);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, (uint)len);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32_vec(&b, (uint)len, arg);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	err = bcm_xdr_unpack_uint32(&retb, &retlen);
	ASSERT(!err);

	retlen = ((uint)len < retlen) ? (uint)len : retlen;
	err = bcm_xdr_unpack_uint32_vec(&retb, retlen, arg);
	ASSERT(!err);

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);
	*ta_ok = (bool)ret;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;

}

void
wlc_phy_noise_sample_request_external(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0,
	                            WLRPC_WLC_PHY_NOISE_SAMPLE_REQUEST_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_cal_perical(wlc_phy_t *ppi, uint8 reason)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_CAL_PERICAL_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, reason);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_acimode_noisemode_reset(wlc_phy_t *ppi, uint channel, bool clear_aci_state,
	bool clear_noise_state, bool disassoc)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 4 * sizeof(uint32),
		WLRPC_WLC_PHY_ACIM_NOISEM_RESET_NPHY_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, channel);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, clear_aci_state);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, clear_noise_state);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, disassoc);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_interference_set(wlc_phy_t *ppi, bool init)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_INTERFER_SET_NPHY_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, init);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

int
wlc_phy_txpower_get(wlc_phy_t *ppi, uint *qdbm, bool *override)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	uint32 uret;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_TXPOWER_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	/* qdmb and override are returned by the low driver */
	err = bcm_xdr_unpack_uint32(&retb, &uret);
	ASSERT(!err);

	*qdbm = uret;

	err = bcm_xdr_unpack_uint32(&retb, &uret);
	ASSERT(!err);

	if (override != NULL) {
		*override = (bool)uret;
	}

	bcm_rpc_buf_free(rpc, ret_buf);

	return 0;
}

int
#ifdef PPR_API
wlc_phy_txpower_set(wlc_phy_t *ppi, uint qdbm, bool override, ppr_t *reg_pwr)
#else
wlc_phy_txpower_set(wlc_phy_t *ppi, uint qdbm, bool override)
#endif
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	int32 ret;
	uint len;

#ifdef PPR_API
	uint slen;
	uint8* tbuf;
	osl_t	*osh = bcm_rpc_osh_get(rpc);

	slen = ppr_ser_size(reg_pwr);
	if ((tbuf = MALLOC(osh, slen)) == NULL) {
		return BCME_ERROR;
	}
	if (ppr_serialize(reg_pwr, tbuf, slen, &len) != BCME_OK) {
		MFREE(osh, tbuf, slen);
		return BCME_ERROR;
	}

	len = (sizeof(uint32) * 2) + XDR_PACK_OPAQUE_VAR_SZ(len);
#else
	len = sizeof(uint32) * 2;
#endif /* PPR_API */

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, len, WLRPC_WLC_PHY_TXPOWER_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, qdbm);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, override);
	ASSERT(!err);

#ifdef PPR_API
	err = bcm_xdr_pack_opaque_varlen(&b, slen, tbuf);
	ASSERT(!err);
#endif

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

#ifdef PPR_API
	MFREE(osh, tbuf, slen);
#endif

	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);
	bcm_rpc_buf_free(rpc, ret_buf);


	return (int)ret;
}

void
#ifdef PPR_API
wlc_phy_txpower_get_current(wlc_phy_t *ppi, ppr_t *reg_pwr, phy_tx_power_t *power)
#else
wlc_phy_txpower_get_current(wlc_phy_t *ppi, phy_tx_power_t *power, uint channel)
#endif
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 temp_var;
	rpc_info_t *rpc = ppi->rpc;
	uint32 flags;
	chanspec_t chanspec, local_chanspec;

#ifndef PPR_API
	uint32 offset, size;
	/* Using small buffer (not 'sizeof(tx_power_t)' which is ~2400 bytes),
	 * sufficient to send the required parameters.
	 */
	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, (5 * sizeof(uint32)),
	                 WLRPC_WLC_PHY_TXPOWER_GET_CURRENT_ID);
	ASSERT(rpc_buf != NULL);

	offset = 0;
	size = sizeof(phy_tx_power_t) / 2;
	size = ROUNDUP(size, sizeof(uint32));

	/* Do endianess conversion before packing */
	flags = htol32(power->flags);
	chanspec = htol16(power->chanspec);
	local_chanspec = htol16(power->local_chanspec);

	/* Packing the required parameters.  Make sure that the order is
	 * maintained while unpacking at the destination
	 */
	err = bcm_xdr_pack_uint32(&b, htol32(offset));
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, htol32(size));
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, channel);
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, flags);
	ASSERT(!err);
	temp_var = (((uint32)chanspec) << 16) | local_chanspec;
	err = bcm_xdr_pack_uint32(&b, temp_var);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (!ret_buf) {
	    return;
	}

	err = bcm_xdr_unpack_opaque_cpy(&retb, size, power);
	ASSERT(!err);

	/* Do conversion again after unpacking */
	power->flags = ltoh32(power->flags);
	power->chanspec = ltoh16(power->chanspec);
	power->local_chanspec = ltoh16(power->local_chanspec);

	bcm_rpc_buf_free(rpc, ret_buf);

	/* get the second chunk of the message */

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, (5 * sizeof(uint32)),
	                 WLRPC_WLC_PHY_TXPOWER_GET_CURRENT_ID);
	ASSERT(rpc_buf != NULL);

	offset = size;
	size = sizeof(phy_tx_power_t) - size;

	err = bcm_xdr_pack_uint32(&b, htol32(offset));
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, htol32(ROUNDUP(size, sizeof(uint32))));
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, flags);
	ASSERT(!err);
	temp_var = (((uint32)power->chanspec) << 16) | power->local_chanspec;
	err = bcm_xdr_pack_uint32(&b, temp_var);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (!ret_buf) {
		return;
	}

	/* extract the second portion */
	err = bcm_xdr_unpack_opaque_cpy(&retb, size, ((uint8 *)power) + offset);
	ASSERT(!err);
	bcm_rpc_buf_free(rpc, ret_buf);
#else

	ppr_t	*ppr_board_limits;
	ppr_t	*ppr_target_powers;
	uint	ser_size_bl, ser_size_tp;
	uint8	*buf_bl, *buf_tp;
	osl_t	*osh = bcm_rpc_osh_get(rpc);
	uint	len;

	uint	slen;
	uint8* tbuf;
	uint32	offset, size;

	slen = ppr_ser_size(reg_pwr);
	if ((tbuf = MALLOC(osh, slen)) == NULL) {
		return;
	}
	if (ppr_serialize(reg_pwr, tbuf, slen, &len) != BCME_OK) {
		MFREE(osh, tbuf, slen);
		return;
	}
	len = (5 * sizeof(uint32)) + XDR_PACK_OPAQUE_VAR_SZ(len);

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, len, WLRPC_WLC_PHY_TXPOWER_GET_CURRENT_ID);
	ASSERT(rpc_buf != NULL);

	/* Do endianess conversion before packing */
	flags = htol32(power->flags);
	chanspec = htol16(power->chanspec);
	local_chanspec = htol16(power->local_chanspec);

	offset = 0;
	ser_size_bl = ppr_ser_size(power->ppr_board_limits);
	size = OFFSETOF(phy_tx_power_t, ppr_board_limits) + ser_size_bl;

	/* Packing the required parameters */
	temp_var = (offset << 16) | ROUNDUP(size, sizeof(uint32));
	err = bcm_xdr_pack_uint32(&b, temp_var);
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, flags);
	ASSERT(!err);
	temp_var = (((uint32)chanspec) << 16) | local_chanspec;
	err = bcm_xdr_pack_uint32(&b, temp_var);
	ASSERT(!err);
	temp_var = ppr_get_ch_bw(power->ppr_board_limits);
	temp_var = htol32(temp_var);
	err = bcm_xdr_pack_uint32(&b, temp_var);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque_varlen(&b, slen, tbuf);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (!ret_buf) {
		MFREE(osh, tbuf, slen);
		return;
	}

	/* store the pointers of ppr_t structures */
	ppr_board_limits = power->ppr_board_limits;
	ppr_target_powers = power->ppr_target_powers;

	ser_size_tp = ppr_ser_size(power->ppr_target_powers);

	err = bcm_xdr_unpack_opaque_cpy(&retb, OFFSETOF(phy_tx_power_t, ppr_board_limits), power);
	ASSERT(!err);

	/* Do conversion again after unpacking */
	power->flags = ltoh32(power->flags);
	power->chanspec = ltoh16(power->chanspec);
	power->local_chanspec = ltoh16(power->local_chanspec);

	/* restore ppr_t pointers */
	power->ppr_board_limits = ppr_board_limits;

	err = bcm_xdr_unpack_opaque_varlen(&retb, &ser_size_bl, (void **)&buf_bl);
	ASSERT(!err);
	err = ppr_deserialize(power->ppr_board_limits, buf_bl, ser_size_bl);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, len, WLRPC_WLC_PHY_TXPOWER_GET_CURRENT_ID);
	ASSERT(rpc_buf != NULL);

	offset = size;
	ser_size_tp = ppr_ser_size(power->ppr_target_powers);
	size = ser_size_tp;

	/* Packing the required parameters */
	temp_var = (offset << 16) | ROUNDUP(size, sizeof(uint32));
	err = bcm_xdr_pack_uint32(&b, temp_var);
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, flags);
	ASSERT(!err);
	temp_var = (((uint32)chanspec) << 16) | local_chanspec;
	err = bcm_xdr_pack_uint32(&b, temp_var);
	ASSERT(!err);
	temp_var = ppr_get_ch_bw(power->ppr_target_powers);
	temp_var = htol32(temp_var);
	err = bcm_xdr_pack_uint32(&b, temp_var);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque_varlen(&b, slen, tbuf);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	MFREE(osh, tbuf, slen);

	if (!ret_buf) {
		return;
	}

	/* restore ppr_t pointers */
	power->ppr_target_powers = ppr_target_powers;

	err = bcm_xdr_unpack_opaque_varlen(&retb, &ser_size_tp, (void **)&buf_tp);
	ASSERT(!err);
	err = ppr_deserialize(power->ppr_target_powers, buf_tp, ser_size_tp);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
#endif /* PPR_API */
}

#ifdef PPR_API

void wlc_phy_txpower_sromlimit(wlc_phy_t *ppi, chanspec_t chanspec,
    uint8 *min_pwr, ppr_t *max_pwr, uint8 core)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	uint32 ret;
	void* bptr;
	uint len;
	wl_tx_bw_t ch_bw = WL_TX_BW_ALL;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 3,
		WLRPC_WLC_PHY_TXPOWER_SROMLIMIT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, chanspec);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, core);
	ASSERT(!err);

	if (max_pwr) {
		ch_bw = ppr_get_ch_bw(max_pwr);
	}

	err = bcm_xdr_pack_uint32(&b, ch_bw);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL) {
		*min_pwr = 0;
	} else {

		/* prev_pwrctrl and cur_txpower is returned by the low driver */
		err = bcm_xdr_unpack_uint32(&retb, &ret);
		ASSERT(!err);

		*min_pwr = (uint8)ret;

		if (max_pwr) {
			err = bcm_xdr_unpack_opaque_varlen(&retb, &len, &bptr);
			ASSERT(!err);

			ppr_deserialize(max_pwr, bptr, len);
		}

		bcm_rpc_buf_free(rpc, ret_buf);
	}
}

#else

void
wlc_phy_txpower_sromlimit(wlc_phy_t *ppi, uint chan, uint8 *min_pwr, uint8 *max_pwr,
                          int txp_rate_idx)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	uint32 ret;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2,
	                            WLRPC_WLC_PHY_TXPOWER_SROMLIMIT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, chan);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, txp_rate_idx);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL) {
		*min_pwr = *max_pwr = 0;
		return;
	}

	/* prev_pwrctrl and cur_txpower is returned by the low driver */
	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	*min_pwr = (uint8)ret;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	*max_pwr = (uint8)ret;
	bcm_rpc_buf_free(rpc, ret_buf);
}
#endif /* PPR_API */

bool
wlc_phy_txpower_hw_ctrl_get(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	uint32 ret;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_TXPOWER_HW_CTRL_GET_ID);
	ASSERT(rpc_buf != NULL);

	/* BMAC_NOTE: This is actually getting a sw state */
	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return FALSE */
	if (ret_buf == NULL)
		return FALSE;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (bool)ret;
}

void
wlc_phy_txpower_hw_ctrl_set(wlc_phy_t *ppi, bool hwpwrctrl)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_TXPOWER_HW_CTRL_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, hwpwrctrl);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

#if defined(AP) && defined(RADAR)
void
wlc_phy_radar_detect_enable(wlc_phy_t *ppi, bool on)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_RADAR_DETECT_ENABLE_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, on);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

int
wlc_phy_radar_detect_run(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	int32 ret;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_RADAR_DETECT_RUN_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return RADAR_TYPE_NONE;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (int)ret;
}

void
wlc_phy_radar_detect_mode_set(wlc_phy_t *pih, phy_radar_detect_mode_t mode)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = pih->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32),
	                            WLRPC_WLC_PHY_RADAR_DETECT_MODE_SET_ID);

	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, mode);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
#endif	/* defined(AP) && defined(RADAR) */

bool
wlc_phy_test_ison(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	uint32 ret;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_TEST_ISON_ID);
	ASSERT(rpc_buf != NULL);

	/* BMAC_NOTE: This is actually getting a sw state */
	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return FALSE */
	if (ret_buf == NULL)
		return FALSE;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (bool)ret;
}

void
wlc_bmac_copyfrom_vars(wlc_hw_info_t *wlc_hw, char **buf_ptr, uint *len)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 retlen;
	void *var_buf;
	uint32 var_len = 0;
	uint residue_len;
	uint8 *local_buf = NULL;
	uint offset = 0;
	rpc_info_t *rpc = wlc_hw->rpc;

	do {
		rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_BMAC_COPYFROM_VARS_ID);
		ASSERT(rpc_buf != NULL);

		err = bcm_xdr_pack_uint32(&b, offset);
		ASSERT(!err);

		ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

		if (ret_buf == NULL) {
			*len = 0;
			*buf_ptr = NULL;
			return;
		}

		err = bcm_xdr_unpack_uint32(&retb, &residue_len);
		ASSERT(!err);

		err = bcm_xdr_unpack_string(&retb, &retlen, (char**)&var_buf);
		ASSERT(!err);
		ASSERT(retlen);

		if (offset == 0) {
			var_len = residue_len + retlen;
			local_buf = MALLOC(wlc_hw->osh, var_len);
			if (!local_buf) {
				bcm_rpc_buf_free(rpc, ret_buf);
				*buf_ptr = NULL;
				*len = 0;
				return;
			}
		}

		memcpy(local_buf+offset, var_buf, retlen);
		offset += retlen;

		bcm_rpc_buf_free(rpc, ret_buf);
	} while (residue_len !=  0);

	*buf_ptr = local_buf;
	*len = var_len;
}

/* Copy a buffer to shared memory of specified type .
 * SHM 'offset' needs to be an even address and
 * Buffer length 'len' must be an even number of bytes
 * 'sel' selects the type of memory
 */
void
wlc_bmac_copyto_objmem(wlc_hw_info_t *wlc_hw, uint offset, const void* buf, int len, uint32 sel)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 3 * sizeof(uint32) + ROUNDUP(len, sizeof(uint32)),
	                            WLRPC_WLC_BMAC_COPYTO_OBJMEM_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, offset);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque_varlen(&b, len, buf);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, sel);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

/* Copy a piece of shared memory of specified type to a buffer .
 * SHM 'offset' needs to be an even address and
 * Buffer length 'len' must be an even number of bytes
 * 'sel' selects the type of memory
 */

void
wlc_bmac_copyfrom_objmem(wlc_hw_info_t *wlc_hw, uint offset, void* buf, int len, uint32 sel)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint32 retlen;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 3,
	                            WLRPC_WLC_BMAC_COPYFROM_OBJMEM_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, offset);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, len);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, sel);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);


	/* On failure, emulate hw failure by returning all -1s */
	if (ret_buf == NULL) {
		memset(buf, -1, len);
		return;
	}

	err = bcm_xdr_unpack_uint32(&retb, &retlen);
	ASSERT(!err);
	ASSERT((int)retlen >= len);

	err = bcm_xdr_unpack_uint16_vec(&retb, retlen, buf);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
}

void
wlc_bmac_enable_mac(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_ENABLE_MAC_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_mctrl(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_MCTRL_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, mask);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, val);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_corereset(wlc_hw_info_t *wlc_hw, uint32 flags)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 1, WLRPC_WLC_CORERESET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, flags);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

uint16
wlc_bmac_read_shm(wlc_hw_info_t *wlc_hw, uint offset)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint32 ret;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_READ_SHM_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, offset);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* On failure, emulate hw failure */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);
	bcm_rpc_buf_free(rpc, ret_buf);

	return (uint16)ret;
}

void
wlc_bmac_read_tsf(wlc_hw_info_t* wlc_hw, uint32* tsf_l_ptr, uint32* tsf_h_ptr)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_READ_TSF_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL) {
		*tsf_l_ptr = *tsf_h_ptr = -1;
		return;
	}

	err = bcm_xdr_unpack_uint32(&retb, tsf_l_ptr);
	ASSERT(!err);

	err = bcm_xdr_unpack_uint32(&retb, tsf_h_ptr);
	ASSERT(!err);
	bcm_rpc_buf_free(rpc, ret_buf);
}

void
wlc_bmac_set_addrmatch(wlc_hw_info_t *wlc_hw, int match_reg_offset, const struct ether_addr *addr)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) +
	                            ROUNDUP(sizeof(struct ether_addr), sizeof(uint32)),
	                            WLRPC_WLC_BMAC_SET_ADDRMATCH_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, match_reg_offset);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque(&b, sizeof(struct ether_addr), (const void*)addr);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_set_cwmax(wlc_hw_info_t *wlc_hw, uint16 newmax)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_SET_CWMAX_ID);
	ASSERT(rpc_buf != NULL);


	err = bcm_xdr_pack_uint32(&b, newmax);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_set_rcmta(wlc_hw_info_t *wlc_hw, int idx, const struct ether_addr *addr)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) +
	                            ROUNDUP(sizeof(struct ether_addr), sizeof(uint32)),
	                            WLRPC_WLC_BMAC_SET_RCMTA_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, idx);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque(&b, sizeof(struct ether_addr), (const void*)addr);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_write_amt(wlc_hw_info_t *wlc_hw, int idx, const struct ether_addr *addr, uint16 attr)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2 +
	                            ROUNDUP(sizeof(struct ether_addr), sizeof(uint32)),
	                            WLRPC_WLC_BMAC_WRITE_AMT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, idx);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque(&b, sizeof(struct ether_addr), (const void*)addr);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, attr);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_set_shm(wlc_hw_info_t *wlc_hw, uint offset, uint16 v, int len)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 3, WLRPC_WLC_BMAC_SET_SHM_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, offset);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, v);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, len);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_suspend_mac_and_wait(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_SUSPEND_MAC_AND_WAIT_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_write_hw_bcntemplates(wlc_hw_info_t *wlc_hw, void *bcn, int len, bool both)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) +
		sizeof(uint32) + ROUNDUP(len, sizeof(uint32)),
		WLRPC_WLC_BMAC_WRITE_HW_BCNTEMPLATES_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, both);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque_varlen(&b, len, bcn);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_write_shm(wlc_hw_info_t *wlc_hw, uint offset, uint16 v)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_BMAC_WRITE_SHM_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, offset);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, v);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_write_template_ram(wlc_hw_info_t *wlc_hw, int offset, int len, void *buf)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) +
	                            sizeof(uint32) + ROUNDUP(len, sizeof(uint32)),
	                            WLRPC_WLC_BMAC_WRITE_TEMPLATE_RAM_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, offset);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque_varlen(&b, len, buf);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_write_ihr(wlc_hw_info_t *wlc_hw, uint offset, uint16 v)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_BMAC_WRITE_IHR_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, offset);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, v);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_tx_fifo_suspend(wlc_hw_info_t *wlc_hw, uint tx_fifo)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_TX_FIFO_SUSPEND_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, tx_fifo);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_tx_fifo_resume(wlc_hw_info_t *wlc_hw, uint tx_fifo)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_TX_FIFO_RESUME_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, tx_fifo);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

bool
wlc_bmac_tx_fifo_suspended(wlc_hw_info_t *wlc_hw, uint tx_fifo)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	uint32 ret;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_TX_FIFO_SUSPENDED_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, tx_fifo);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error, return FIFO to be suspended */
	if (ret_buf == NULL)
		return TRUE;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);
	bcm_rpc_buf_free(rpc, ret_buf);

	return (bool) ret;
}

void
wlc_bmac_hw_etheraddr(wlc_hw_info_t *wlc_hw, struct ether_addr *ea)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_HW_ETHERADDR_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL) {
		ether_copy(&ether_null, ea);
		return;
	}

	err = bcm_xdr_unpack_opaque_cpy(&retb, sizeof(struct ether_addr), ea);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
}

void
wlc_bmac_set_hw_etheraddr(wlc_hw_info_t *wlc_hw, struct ether_addr *ea)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, ROUNDUP(sizeof(struct ether_addr), sizeof(uint32)),
	                            WLRPC_WLC_SET_HW_ETHERADDR_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_opaque(&b, sizeof(struct ether_addr), (void*)ea);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

#ifdef PPR_API
void
wlc_bmac_set_chanspec(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool mute, ppr_t *txppr)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	uint len;
	uint slen;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint8* tbuf;

	slen = ppr_ser_size(txppr);
	if ((tbuf = MALLOC(wlc_hw->osh, slen)) != NULL) {
		if (ppr_serialize(txppr, tbuf, slen, &len) == BCME_OK) {
			len = sizeof(uint32) * 2 + XDR_PACK_OPAQUE_VAR_SZ(len);

			rpc_buf = wlc_rpc_buf_alloc(rpc, &b, len, WLRPC_WLC_BMAC_CHANSPEC_SET_ID);
			ASSERT(rpc_buf != NULL);

			err = bcm_xdr_pack_uint32(&b, chanspec);
			ASSERT(!err);

			err = bcm_xdr_pack_uint32(&b, mute);
			ASSERT(!err);

			err = bcm_xdr_pack_opaque_varlen(&b, slen, tbuf);
			ASSERT(!err);

			err = wlc_rpc_call(rpc, rpc_buf);
			ASSERT(!err);
		}
		MFREE(wlc_hw->osh, tbuf, slen);
	}
}

#else

void
wlc_bmac_set_chanspec(wlc_hw_info_t *wlc_hw, chanspec_t chanspec, bool mute, txppr_t *txpwr)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	uint len;
	rpc_info_t *rpc = wlc_hw->rpc;

	len = sizeof(uint32) * 2 + ROUNDUP(sizeof(*txpwr), 4);

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, len, WLRPC_WLC_BMAC_CHANSPEC_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, chanspec);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, mute);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque(&b, sizeof(*txpwr), txpwr);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
#endif /* PPR_API */


void
wlc_bmac_txant_set(wlc_hw_info_t *wlc_hw, uint16 phytxant)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_TXANT_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, phytxant);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_antsel_type_set(wlc_hw_info_t *wlc_hw, uint8 antsel_type)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32),
	                            WLRPC_WLC_BMAC_ANTSEL_TYPE_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, antsel_type);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

bool
wlc_bmac_radio_read_hwdisabled(wlc_hw_info_t* wlc_hw)
{
	UNUSED_PARAMETER(wlc_hw);
	return FALSE;
}

#ifdef STA
#ifdef WLRM
void
wlc_bmac_rm_cca_measure(wlc_hw_info_t *wlc_hw, uint32 us)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_RM_CCA_MEASURE_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, us);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
#endif /* WLRM */
#endif /* STA */

void
wlc_bmac_set_shortslot(wlc_hw_info_t *wlc_hw, bool shortslot)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_SET_SHORTSLOT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, shortslot);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_wait_for_wake(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_WAIT_FOR_WAKE_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_BSSinit(wlc_phy_t *pih, bool bonlyap, int rssi)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = pih->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_PHY_BSSINIT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, bonlyap);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, rssi);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_band_stf_ss_set(wlc_hw_info_t *wlc_hw, uint8 stf_mode)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BAND_STF_SS_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, stf_mode);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

chanspec_t
wlc_phy_chanspec_band_firstch(wlc_phy_t *ppi, uint band)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;
	uint32 ret;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_BAND_FIRST_CHANSPEC_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, band);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return INVCHANSPEC;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (chanspec_t)ret;
}

void
wlc_phy_chanspec_band_validch(wlc_phy_t *ppi, uint band, chanvec_t *channels)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_BAND_CHANNELS_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, band);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL) {
		bzero(channels, sizeof(chanvec_t));
		return;
	}

	err = bcm_xdr_unpack_opaque_cpy(&retb, sizeof(chanvec_t), channels);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
}

void
#ifdef PPR_API
wlc_phy_txpower_limit_set(wlc_phy_t *ppi, ppr_t *txppr, chanspec_t chanspec)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	uint len;
	uint slen;
	rpc_info_t *rpc = ppi->rpc;
	uint8* tbuf;
	osl_t* osh = bcm_rpc_osh_get(rpc);

	slen = ppr_ser_size(txppr);

	if ((tbuf = MALLOC(osh, slen)) != NULL) {
		if (ppr_serialize(txppr, tbuf, slen, &len) == BCME_OK) {
			len = sizeof(uint32) + XDR_PACK_OPAQUE_VAR_SZ(len);

			rpc_buf = wlc_rpc_buf_alloc(rpc, &b, len,
				WLRPC_WLC_PHY_TXPOWER_LIMIT_SET_ID);
			ASSERT(rpc_buf != NULL);

			err = bcm_xdr_pack_uint32(&b, chanspec);
			ASSERT(!err);

			err = bcm_xdr_pack_opaque_varlen(&b, slen, tbuf);
			ASSERT(!err);

			err = wlc_rpc_call(rpc, rpc_buf);
			ASSERT(!err);
		}
		MFREE(osh, tbuf, slen);
	}
}

#else
wlc_phy_txpower_limit_set(wlc_phy_t *ppi, txppr_t *txpwr, chanspec_t chanspec)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	uint len;
	rpc_info_t *rpc = ppi->rpc;

	len = sizeof(uint32) + ROUNDUP(sizeof(*txpwr), 4);

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, len, WLRPC_WLC_PHY_TXPOWER_LIMIT_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, chanspec);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque(&b, sizeof(*txpwr), txpwr);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
#endif /* PPR_API */

uint8
wlc_phy_txpower_get_target_min(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 ret;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_TXPOWER_GET_TARGET_MIN_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (uint8)ret;
}

/* if this causes performance issue, then move it to high MAC */
uint8
wlc_phy_txpower_get_target_max(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 ret;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_TXPOWER_GET_TARGET_MAX_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (uint8)ret;
}

int
wlc_bmac_revinfo_get(wlc_hw_info_t *wlc_hw, wlc_bmac_revinfo_t *revinfo)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint idx;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_REVINFO_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	/* Ununpack the whole revinfo structure */
	err = bcm_xdr_unpack_uint32(&retb, &revinfo->vendorid);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->deviceid);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->corerev);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->boardrev);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->sromrev);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->chiprev);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->chip);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->chippkg);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->boardtype);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->boardvendor);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->bustype);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->buscoretype);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->buscorerev);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->issim);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->nbands);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->boardflags);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &revinfo->boardflags2);
	if (err)
		goto fail;

	for (idx = 0; idx < revinfo->nbands; idx++) {
		/* Use band 1 for single band 11a */
		if (IS_SINGLEBAND_5G(revinfo->deviceid))
			idx = BAND_5G_INDEX;

		err = bcm_xdr_unpack_uint32(&retb, &revinfo->band[idx].bandunit);
		if (err)
			goto fail;

		err = bcm_xdr_unpack_uint32(&retb, &revinfo->band[idx].bandtype);
		if (err)
			goto fail;

		err = bcm_xdr_unpack_uint32(&retb, &revinfo->band[idx].radiorev);
		if (err)
			goto fail;

		err = bcm_xdr_unpack_uint32(&retb, &revinfo->band[idx].phytype);
		if (err)
			goto fail;

		err = bcm_xdr_unpack_uint32(&retb, &revinfo->band[idx].phyrev);
		if (err)
			goto fail;

		err = bcm_xdr_unpack_uint32(&retb, &revinfo->band[idx].anarev);
		if (err)
			goto fail;

		err = bcm_xdr_unpack_uint32(&retb, &revinfo->band[idx].radioid);
		if (err)
			goto fail;

		err = bcm_xdr_unpack_uint32(&retb, (uint32*)(&revinfo->band[idx].abgphy_encore));
		if (err)
			goto fail;
	}

fail:
	bcm_rpc_buf_free(rpc, ret_buf);
	return err;
}

int
wlc_bmac_state_get(wlc_hw_info_t *wlc_hw, wlc_bmac_state_t *state)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_STATE_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	/* Ununpack the whole wlc_bmac_state_t structure */
	err = bcm_xdr_unpack_uint32(&retb, &state->machwcap);
	if (err)
		goto fail;

	err = bcm_xdr_unpack_uint32(&retb, &state->preamble_ovr);
	if (err)
		goto fail;


fail:
	bcm_rpc_buf_free(rpc, ret_buf);
	return err;
}

int
wlc_bmac_xmtfifo_sz_get(wlc_hw_info_t *wlc_hw, uint fifo, uint *blocks)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int fn_err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_XMTFIFO_SZ_GET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, fifo);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	err = bcm_xdr_unpack_uint32(&retb, blocks);
	ASSERT(!err);

	err = bcm_xdr_unpack_int32(&retb, &fn_err);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return fn_err;
}

int
wlc_bmac_xmtfifo_sz_set(wlc_hw_info_t *wlc_hw, uint fifo, uint16 blocks)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int fn_err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_BMAC_XMTFIFO_SZ_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, fifo);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, blocks);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	err = bcm_xdr_unpack_int32(&retb, &fn_err);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return fn_err;
}

#ifdef PHYCAL_CACHING
void
wlc_bmac_set_phycal_cache_flag(wlc_hw_info_t *wlc_hw, bool state)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32),
	                            WLRPC_WLC_BMAC_SET_PHYCAL_CACHE_FLAG_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, state);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_get_phycal_cache_flag(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = pih->rpc;
	uint32 ret;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32),
	                            WLRPC_WLC_BMAC_SET_PHYCAL_CACHE_FLAG_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (int8)ret;
}
#endif /* PHYCAL_CACHING */

bool
wlc_bmac_validate_chip_access(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_VALIDATE_CHIP_ACCESS_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return FALSE;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (ret != 0);
}

void
wlc_phy_stf_chain_init(wlc_phy_t *pih, uint8 txchain, uint8 rxchain)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = pih->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2 * sizeof(uint32), WLRPC_WLC_PHYCHAIN_INIT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, txchain);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, rxchain);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_stf_chain_set(wlc_phy_t *pih, uint8 txchain, uint8 rxchain)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = pih->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2 * sizeof(uint32), WLRPC_WLC_PHYCHAIN_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, txchain);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, rxchain);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_stf_chain_get(wlc_phy_t *pih, uint8 *txchain, uint8 *rxchain)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	int err;
	rpc_info_t *rpc = pih->rpc;
	uint32 tmp;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHYCHAIN_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return;

	err = bcm_xdr_unpack_uint32(&retb, &tmp);
	ASSERT(!err);
	*txchain = (uint8)tmp;

	err = bcm_xdr_unpack_int32(&retb, &tmp);
	ASSERT(!err);
	*rxchain = (uint8)tmp;

	bcm_rpc_buf_free(rpc, ret_buf);
}

void
wlc_phy_chanspec_ch14_widefilter_set(wlc_phy_t *ppi, bool wide_filter)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32),
	                            WLRPC_WLC_PHY_SET_CHANNEL_14_WIDE_FILTER_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, wide_filter);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

int8
wlc_phy_noise_avg(wlc_phy_t *ppi)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int8 noise;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_NOISE_AVG_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	err = bcm_xdr_unpack_int8(&retb, &noise);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return noise;
}

void
wlc_phy_tkip_rifs_war(wlc_phy_t *pih, uint8 rifs)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = pih->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_TKIP_RIFS_WAR_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, rifs);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_set_noreset(wlc_hw_info_t *wlc_hw, bool noreset_flag)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_SET_NORESET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)noreset_flag);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

bool
wlc_bmac_p2p_cap(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int8 cap;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_P2P_CAP_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return FALSE;

	err = bcm_xdr_unpack_int8(&retb, &cap);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return cap != 0;
}

int
wlc_bmac_p2p_set(wlc_hw_info_t *wlc_hw, bool enable)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;
	int success;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_P2P_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)enable);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return BCME_ERROR;

	err = bcm_xdr_unpack_int32(&retb, &success);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return success;
}

void
wlc_bmac_retrylimit_upd(wlc_hw_info_t *wlc_hw, uint16 SRL, uint16 LRL)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2, WLRPC_WLC_BMAC_RETRYLIMIT_UPD_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, SRL);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, LRL);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

int
wlc_bmac_btc_mode_set(wlc_hw_info_t *wlc_hw, int btc_mode)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int32), WLRPC_WLC_BMAC_BTC_MODE_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, btc_mode);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_btc_mode_get(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_BTC_MODE_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_btc_wire_set(wlc_hw_info_t *wlc_hw, int btc_wire)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int32), WLRPC_WLC_BMAC_BTC_WIRE_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, btc_wire);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_btc_wire_get(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_BTC_WIRE_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}


int
wlc_bmac_btc_flags_get(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_BTC_FLAGS_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_btc_flags_idx_get(wlc_hw_info_t *wlc_hw, int int_val)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int32), WLRPC_WLC_BMAC_BTC_FLAGS_IDX_GET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, int_val);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_btc_flags_idx_set(wlc_hw_info_t *wlc_hw, int int_val, int int_val2)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2*sizeof(int32), WLRPC_WLC_BMAC_BTC_FLAGS_IDX_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, int_val);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, int_val2);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_btc_params_get(wlc_hw_info_t *wlc_hw, int int_val)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int32), WLRPC_WLC_BMAC_BTC_PARAMS_GET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, int_val);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_btc_params_set(wlc_hw_info_t *wlc_hw, int int_val, int int_val2)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2*sizeof(int32), WLRPC_WLC_BMAC_BTC_PARAMS_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, int_val);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, int_val2);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_btc_period_get(wlc_hw_info_t *wlc_hw, uint16 *btperiod, bool *btactive)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint32 tmp;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_BTC_PERIOD_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	*btperiod = (uint16)ret;

	err = bcm_xdr_unpack_int32(&retb, &tmp);
	ASSERT(!err);
	*btactive = (bool)tmp;

	bcm_rpc_buf_free(rpc, ret_buf);

	return BCME_OK;
}

void
wlc_bmac_btc_stuck_war50943(wlc_hw_info_t *wlc_hw, bool enable)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int32), WLRPC_WLC_BMAC_BTC_STUCKWAR_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, enable);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_btc_rssi_threshold_get(wlc_hw_info_t *wlc_hw, uint8 *protr, uint8 *ampdut, uint8 *ampdur)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_BTC_RSSI_THRESHOLD_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);
	*protr = (uint8)ret;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);
	*ampdut = (uint8)ret;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);
	*ampdur = (uint8)ret;

	bcm_rpc_buf_free(rpc, ret_buf);
}

void
wlc_bmac_fifoerrors(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_FIFOERRORS_ID);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

}

void
wlc_bmac_ampdu_set(wlc_hw_info_t *wlc_hw, uint8 mode)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint), WLRPC_WLC_BMAC_AMPDU_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, mode);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

#ifdef WLLED
void
wlc_bmac_led_hw_deinit(wlc_hw_info_t * wlc_hw, uint32 gpiomask_cache)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_BMAC_LED_HW_DEINIT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, gpiomask_cache);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

}

void
wlc_bmac_led_hw_mask_init(wlc_hw_info_t * wlc_hw, uint32 gpiomask_cache)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_BMAC_LED_HW_MASK_INIT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, gpiomask_cache);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

}
#endif /* WLLED */


void
wlc_bmac_pllreq(wlc_hw_info_t *wlc_hw, bool set, mbool req_bit)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2*sizeof(int), WLRPC_WLC_PLLREQ_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, (int)set);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, req_bit);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

#ifdef BCMASSERT_SUPPORT
bool
wlc_bmac_taclear(wlc_hw_info_t *wlc_hw, bool ta_ok)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	int ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_BMAC_TACLEAR_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, (int)ta_ok);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return BCME_BADOPTION;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (bool)ret;
}
#endif /* BCMASSERT_SUPPORT */

void
wlc_phy_ofdm_rateset_war(wlc_phy_t *pih, bool war)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = pih->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_PHY_OFDM_RATESET_WAR_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, (int)war);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;
}

void
wlc_phy_bf_preempt_enable(wlc_phy_t *pih, bool bf_preempt)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = pih->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_PHY_BF_PREEMPT_ENABLE_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, (int)bf_preempt);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;
}

void
wlc_bmac_set_clk(wlc_hw_info_t *wlc_hw, bool on)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_BMAC_SET_CLK_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, (int)on);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;
}

int
wlc_bmac_iovars_dispatch(wlc_hw_info_t *wlc_hw, uint32 actionid, uint16 type,
	void *p, uint plen, void *a, int alen, int vsize)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err, ret;
	rpc_info_t *rpc = wlc_hw->rpc;
	bool buf32 = FALSE;

	WL_TRACE(("%s: actionid %d plen %d alen %d\n", __FUNCTION__, actionid, plen, alen));

	if (plen > 1024)
		plen = 1024;

	if (alen > 1024)
		alen = 1024;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 4*sizeof(uint32) +
		ROUNDUP(plen, sizeof(uint32)), WLRPC_WLC_BMAC_DOIOVARS_ID);
	if (rpc_buf == NULL) {
		WL_ERROR(("%s: wlc_rpc_buf_allof %d bytes failed. actionid %d\n", __FUNCTION__,
			(uint)(4 * sizeof(uint32) +
			ROUNDUP(plen, sizeof(uint32))),	actionid));
		return BCME_NOMEM;
	}

	err = bcm_xdr_pack_uint32(&b, actionid);
	ASSERT(!err);

	/* Decide the buffer is 16-bit or 32-bit buffer */
	switch (IOV_ID(actionid)) {
		case IOV_BMAC_SBGPIOOUT:
		case IOV_BMAC_CCREG:
		case IOV_BMAC_PCIEREG:
		case IOV_BMAC_PCICFGREG:
		case IOV_BMAC_PCIESERDESREG:
			buf32 = TRUE;
			break;

		case IOV_BMAC_COREREG:	/* 3 parameters, more than 32bits */
		default:
			buf32 = FALSE;
			break;
	}

	/* Take care of endianess for 16 and 32 bit input */
	if ((type == IOVT_INT16) || (type == IOVT_UINT16)) {
		plen = 2;
		err = bcm_xdr_pack_uint32(&b, plen);
		ASSERT(!err);

		err = bcm_xdr_pack_uint16_vec(&b, plen, p);
	} else if (buf32 || (type == IOVT_INT32) || (type == IOVT_UINT32)) {
		plen &= ~0x3;
		err = bcm_xdr_pack_uint32(&b, plen);
		ASSERT(!err);

		err = bcm_xdr_pack_uint32_vec(&b, plen, p);
	} else {
		err = bcm_xdr_pack_uint32(&b, plen);
		ASSERT(!err);

		err = bcm_xdr_pack_opaque(&b, plen, p);
	}
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, alen);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, vsize);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	/* Convert back from little endian */
	if ((type == IOVT_INT16) || (type == IOVT_UINT16) ||
		(type == IOVT_INT32) || (type == IOVT_UINT32))
		err = bcm_xdr_unpack_uint32(&retb, (uint32*)a);
	else if (buf32)
		err = bcm_xdr_unpack_uint32_vec(&retb, alen, a);
	else
		err = bcm_xdr_unpack_opaque_cpy(&retb, alen, a);
	ASSERT(!err);

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return ret;
}

void
wlc_bmac_dump(wlc_hw_info_t *wlc_hw, struct bcmstrbuf *strb, wlc_bmac_dump_id_t dump_id)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	uint32 len;
	rpc_info_t *rpc = wlc_hw->rpc;
	void *data = NULL;

	WL_TRACE(("%s\n", __FUNCTION__));

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2*sizeof(uint32), WLRPC_WLC_BMAC_DUMP_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, dump_id);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, strb->size);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return */
	if (ret_buf == NULL)
		return;

	err = bcm_xdr_unpack_opaque_varlen(&retb, &len, &data);
	ASSERT(!err);

	ASSERT(len <= strb->size);
	bcopy(data, strb->buf, len);
	strb->buf += len;
	strb->size -= len;

	bcm_rpc_buf_free(rpc, ret_buf);

	return;

}

#if defined(WLTEST)
int
wlc_bmac_pkteng(wlc_hw_info_t *wlc_hw, wl_pkteng_t *pkteng, void* p)
{
	int err = 0, ret = 0;
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint rpc_totlen = 0, totlen = 0;
	void *p0 = p;
	uint len = 0;
	uint i;

	totlen = (p == NULL) ? 0 : pkttotlen(wlc_hw->osh, p);
	rpc_totlen += ROUNDUP(totlen, sizeof(uint32));
	rpc_totlen += ROUNDUP(sizeof(wl_pkteng_t), sizeof(uint32));
	rpc_totlen += sizeof(uint32);

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, rpc_totlen, WLRPC_WLC_BMAC_PKTENG_ID);

	if (!rpc_buf) {
		WL_ERROR(("wl: %s RPC buf alloc failed\n", __FUNCTION__));
		PKTFREE(wlc_hw->osh, p, TRUE);
		return -1;
	}

	/* Pack pkteng_t */
	pkteng->flags = htol32(pkteng->flags);
	pkteng->delay = htol32(pkteng->delay);
	pkteng->nframes = htol32(pkteng->nframes);
	pkteng->length = htol32(pkteng->length);
	err = bcm_xdr_pack_opaque(&b, sizeof(wl_pkteng_t), (void*)pkteng);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, totlen);
	ASSERT(!err);

	if (totlen > 0) {
		i = 0;
		for (p = p0; p != NULL; p = PKTNEXT(wlc_hw->osh, p)) {
			void *data;
			len = PKTLEN(wlc_hw->osh, p);
			data = PKTDATA(wlc_hw->osh, p);

			++i;
			WL_TRACE(("%s %d:fraglen:%d\n", __FUNCTION__, i, len));

			/* add each packet fragment to the buffer without 4-byte padding */
			err = bcm_xdr_pack_opaque_raw(&b, len, data);
			ASSERT(!err);
		}

		/* after the last packet fragment add the XDR buf padding */
		err = bcm_xdr_pack_opaque_pad(&b);
		ASSERT(!err);
	}

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	/* unpack return (err) */
	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return ret;
}
#endif 

#if (defined(BCMNVRAMR) || defined(BCMNVRAMW)) && defined(WLTEST)
#ifdef BCMNVRAMW
int
wlc_bmac_ciswrite(wlc_hw_info_t *wlc_hw, cis_rw_t *cis, uint16 *tbuf, int len)
{
	int err = 0, ret = 0;
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	rpc_info_t *rpc = wlc_hw->rpc;

	WL_TRACE(("%s\n", __FUNCTION__));

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int32) + ROUNDUP(sizeof(cis_rw_t),
		sizeof(uint32)) + ROUNDUP(len, sizeof(uint32)), WLRPC_WLC_CISWRITE_ID);
	if (rpc_buf == NULL) {
		WL_ERROR(("%s: wlc_rpc_buf_allof %d bytes failed.\n", __FUNCTION__,
			(int)((sizeof(int32) + ROUNDUP(sizeof(cis_rw_t), sizeof(uint32)))
			+ ROUNDUP(len, sizeof(uint32)))));
		return BCME_NOMEM;
	}

	/* Pack len and tbuf */
	err = bcm_xdr_pack_int32(&b, len);
	ASSERT(!err);
	err = bcm_xdr_pack_opaque(&b, len, (void*)tbuf);
	ASSERT(!err);

	/* Pack cis */
	err = bcm_xdr_pack_opaque(&b, sizeof(cis_rw_t), (void*)cis);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	/* unpack return (err) */
	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return ret;
}
#endif /* def BCMNVRAMW */

int
wlc_bmac_cisdump(wlc_hw_info_t *wlc_hw, cis_rw_t *cis, uint16 *tbuf, int len)
{
	int err = 0, ret = 0;
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	rpc_info_t *rpc = wlc_hw->rpc;

	WL_TRACE(("%s\n", __FUNCTION__));

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int32) +
		ROUNDUP(sizeof(cis_rw_t), sizeof(uint32)), WLRPC_WLC_CISDUMP_ID);
	if (rpc_buf == NULL) {
		WL_ERROR(("%s: wlc_rpc_buf_allof %d bytes failed.\n", __FUNCTION__,
			(uint)(sizeof(int32) + ROUNDUP(sizeof(cis_rw_t), sizeof(uint32)))));
		return BCME_NOMEM;
	}

	/* Pack len */
	err = bcm_xdr_pack_int32(&b, len);
	ASSERT(!err);

	/* Pack cis */
	err = bcm_xdr_pack_opaque(&b, sizeof(cis_rw_t), (void*)cis);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return BCME_DONGLE_DOWN;

	/* unpack return (err) */
	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	/* unpack cis */
	err = bcm_xdr_unpack_opaque_cpy(&retb, sizeof(cis_rw_t), (void*)cis);
	ASSERT(!err);

	/* copy return buf to tbuf if no error returned */
	if (!ret) {
		err = bcm_xdr_unpack_opaque_cpy(&retb, SROM_MAX, tbuf);
		ASSERT(!err);
	}

	bcm_rpc_buf_free(rpc, ret_buf);
	return ret;
}
#endif 

bool
wlc_bmac_iscoreup(si_t *sih)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf = NULL, *ret_buf = NULL;
	int err;
	rpc_info_t *rpc = sih->rpc;
	int ret;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_SI_ISCORE_UP_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return FALSE;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (bool)ret;
}

#ifdef WLLED
/* In monilithic driver, the return value is added to pending timer callbacks
 * In splitmac driver, the led timer is in dongle and high driver should not wait for it
 *   So the return has to zero
 */
int
wlc_bmac_led_blink_event(wlc_hw_info_t *wlc_hw, bool blink)
{
	int err = 0;
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_BMAC_LED_BLINK_EVENT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, (int)blink);
	ASSERT(!err);

	/* we ignore the return value, since it's not needed in HIGH MAC */
	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return 0;
}

void
wlc_bmac_led_set(wlc_hw_info_t *wlc_hw, int indx, uint8 activehi)
{
	int err = 0;
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2*sizeof(int), WLRPC_WLC_BMAC_LED_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, indx);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, (int)activehi);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;


}

void
wlc_bmac_led_blink(wlc_hw_info_t *wlc_hw, int indx, uint16 msec_on, uint16 msec_off)
{
	int err = 0;
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 3*sizeof(int), WLRPC_WLC_BMAC_LED_BLINK_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, indx);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, msec_on);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, msec_off);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;
}
void
wlc_bmac_led(wlc_hw_info_t *wlc_hw, uint32 mask, uint32 val, bool activehi)
{
	int err = 0;
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 3*sizeof(int), WLRPC_WLC_BMAC_LED_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, mask);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, val);
	ASSERT(!err);

	err = bcm_xdr_pack_int32(&b, (int)activehi);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;

}
#endif /* WLLED */

#ifdef AP
void
wlc_bmac_process_ps_switch(wlc_hw_info_t *wlc_hw, struct ether_addr *ea, int8 ps_on)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	uint arg_size;
	int ea_null = (ea == NULL);
	rpc_info_t *rpc = wlc_hw->rpc;

	/* call always has 2 ints, ps_on and ea_null */
	arg_size = 2 * sizeof(uint32);

	/* if the ea is non-null, include space for it too */
	if (!ea_null)
		arg_size += ROUNDUP(sizeof(struct ether_addr), sizeof(uint32));
	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, arg_size, WLRPC_WLC_BMAC_PS_SWITCH_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int8(&b, ps_on);
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, ea_null);
	ASSERT(!err);

	if (!ea_null) {
		err = bcm_xdr_pack_opaque(&b, sizeof(struct ether_addr), (void*)ea);
		ASSERT(!err);
		}
	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
#endif /* AP */


uint16
wlc_bmac_rate_shm_offset(wlc_hw_info_t *wlc_hw, uint8 rate)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint32 ret;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_RATE_SHM_OFFSET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)rate);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (uint16)ret;
}

bool
wlc_phy_txpower_ipa_ison(wlc_phy_t *pih)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = pih->rpc;
	uint32 ret;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHY_TXPOWER_IPA_ISON);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return (bool)ret;
}

int8
wlc_phy_stf_ssmode_get(wlc_phy_t *pih, chanspec_t chanspec)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = pih->rpc;
	uint32 ret;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_STF_SSMODE_GET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)chanspec);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (int8)ret;
}

int
wlc_bmac_debug_template(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_DEBUG_ID);
	ASSERT(rpc_buf != NULL);
	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);
	if (ret_buf == NULL)
		return -1;

	/* retrieve any parameters */

	bcm_rpc_buf_free(rpc, ret_buf);
	return 0;
}

#ifdef WLEXTLOG
#ifdef WLC_HIGH_ONLY
void
wlc_bmac_extlog_cfg_set(wlc_hw_info_t *wlc_hw, wlc_extlog_cfg_t *cfg)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 3, WLRPC_WLC_EXTLOG_CFG_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)cfg->module);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, (uint32)cfg->level);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, (uint32)cfg->flag);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;
}
#endif /* WLC_HIGH_ONLY */
#endif /* EXTLOG */

void
wlc_bmac_assert_type_set(wlc_hw_info_t *wlc_hw, uint32 type)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_BCM_ASSERT_TYPE_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, type);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);

	return;
}

void wlc_bmac_set_txpwr_percent(wlc_hw_info_t *wlc_hw, uint8 val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_SET_TXPWR_PERCENT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)val);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);

	return;
}

uint8
wlc_phy_stf_chain_active_get(wlc_phy_t *pih)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = pih->rpc;
	uint32 ret;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_PHYCHAIN_ACTIVE_GET_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return (int8)ret;

}

#ifdef WLLED
void
wlc_bmac_blink_sync(wlc_hw_info_t *wlc_hw, uint32 blink_pins)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_BLINK_SYNC_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, blink_pins);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);

	return;
}
#endif /* WLLED */


void
wlc_bmac_ifsctl_edcrs_set(wlc_hw_info_t *wlc_hw, bool isht)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_IFSCTL_EDCRS_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, isht);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);

	return;
}

int
wlc_bmac_cca_stats_read(wlc_hw_info_t *wlc_hw, cca_ucode_counts_t *cca_counts)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint32 ret = 0;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_CCA_STATS_READ_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL) {
		WL_ERROR(("%s: rpc call failed\n", __FUNCTION__));
		return -1;
	}

	err = bcm_xdr_unpack_opaque_cpy(&retb, sizeof(cca_ucode_counts_t), (void*)cca_counts);
	STATIC_ASSERT(sizeof(cca_ucode_counts_t) > 8);
	ASSERT(!err);
	/* Do conversion again after unpacking */
	cca_counts->txdur = ltoh32(cca_counts->txdur);
	cca_counts->ibss = ltoh32(cca_counts->ibss);
	cca_counts->obss = ltoh32(cca_counts->obss);
	cca_counts->noctg = ltoh32(cca_counts->noctg);
	cca_counts->nopkt = ltoh32(cca_counts->nopkt);
	cca_counts->PM = ltoh32(cca_counts->PM);
	cca_counts->usecs = ltoh32(cca_counts->usecs);
#ifdef ISID_STATS
	cca_counts->crsglitch = ltoh32(cca_counts->crsglitch);
	cca_counts->badplcp = ltoh32(cca_counts->badplcp);
#endif /* ISID_STATS */

	bcm_rpc_buf_free(rpc, ret_buf);
	return ret;
}

void
wlc_bmac_antsel_set(wlc_hw_info_t *wlc_hw, uint32 antsel_avail)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;
	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_ANTSEL_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, antsel_avail);
	ASSERT(!err);
	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_phy_ldpc_override_set(wlc_phy_t *ppi, bool val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_LDPC_SET_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int8(&b, val);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

bool
wlc_bmac_radio_hw(wlc_hw_info_t *wlc_hw, bool enable, bool skip_anacore)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint32 ret = 0;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2 * sizeof(uint32), WLRPC_WLC_BMAC_RADIO_HW_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)enable);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, (uint32)skip_anacore);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL) {
		WL_ERROR(("%s: rpc call failed\n", __FUNCTION__));
		return FALSE;
	}

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return ret? TRUE: FALSE;
}

#ifdef WL_MULTIQUEUE
void
wlc_bmac_tx_fifo_sync(wlc_hw_info_t *wlc_hw, uint fifo_bitmap, uint8 flag)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32) * 2,
		WLRPC_WLC_BMAC_TX_FIFO_SYNC_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, fifo_bitmap);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, flag);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
#endif /* WL_MULTIQUEUE */

#ifdef WLMCHAN
void
wlc_phy_destroy_chanctx(wlc_phy_t *ppi, chanspec_t chanspec)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32),
		WLRPC_WLC_PHY_DESTROY_CHANCTX_ID);

	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, chanspec);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

int
wlc_phy_create_chanctx(wlc_phy_t *ppi, chanspec_t chanspec)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	int err, ret;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_PHY_CREATE_CHANCTX_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, chanspec);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_int32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}
#endif /* WLMCHAN */

#ifdef STA
/* Change PCIE War override for some platforms */
void
wlc_bmac_pcie_war_ovr_update(wlc_hw_info_t *wlc_hw, uint8 aspm)
{
}

void
wlc_bmac_pcie_power_save_enable(wlc_hw_info_t *wlc_hw, bool enable)
{
}
#endif

#ifdef WOWL
int
wlc_bmac_wowlucode_init(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	int err, ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_WOWL_UCODE_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_wowlucode_start(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	int err, ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_WOWL_UCODESTART_ID);
	ASSERT(rpc_buf != NULL);


	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_write_inits(wlc_hw_info_t *wlc_hw, void *inits, int len)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	int err, ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, ROUNDUP(len + 1, sizeof(uint32)),
		WLRPC_WLC_BMAC_WOWL_INITS_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, len);
	ASSERT(!err);

	err = bcm_xdr_pack_opaque(&b, len, inits);
	ASSERT(!err);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

int
wlc_bmac_wakeucode_dnlddone(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	int err, ret;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_WOWL_DNLDDONE_ID);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL)
		return -1;

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);

	return ret;
}

void
wlc_bmac_dngldown(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WL_WOWL_DNGLDOWN);
	ASSERT(rpc_buf != NULL);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

void
wlc_bmac_wowl_config_4331_5GePA(wlc_hw_info_t *wlc_hw, bool is_5G, bool is_4331_12x9)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(uint32), WLRPC_WLC_BMAC_CONFIG_4331_EPA_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)is_5G);
	ASSERT(!err);
	err = bcm_xdr_pack_uint32(&b, (uint32)is_4331_12x9);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}

#endif /* WOWL */

void
wlc_bmac_filter_war_upd(wlc_hw_info_t *wlc_hw, bool war)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, sizeof(int), WLRPC_WLC_BMAC_SET_FILT_WAR_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_int32(&b, (int)war);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;
}

#ifdef MBSS
bool
wlc_bmac_ucodembss_hwcap(wlc_hw_info_t *wlc_hw)
{
	bcm_xdr_buf_t b, retb;
	rpc_buf_t *rpc_buf, *ret_buf;
	rpc_info_t *rpc = wlc_hw->rpc;
	uint32 ret = 0;
	int err;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 0, WLRPC_WLC_BMAC_UCODEMBSS_HWCAP);
	ASSERT(rpc_buf != NULL);

	ret_buf = wlc_rpc_call_with_return(rpc, rpc_buf, &retb);

	/* In case of error return -1 as if the hw failed */
	if (ret_buf == NULL) {
		WL_ERROR(("%s: rpc call failed\n", __FUNCTION__));
		return FALSE;
	}

	err = bcm_xdr_unpack_uint32(&retb, &ret);
	ASSERT(!err);

	bcm_rpc_buf_free(rpc, ret_buf);
	return ret? TRUE: FALSE;
}
#endif /* MBSS */

void
wlc_bmac_enable_tbtt(wlc_hw_info_t *wlc_hw,  uint32 mask, uint32 val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2 * sizeof(uint32),
		WLRPC_WLC_BMAC_ENABLE_TBTT_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, mask);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, val);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}


void
wlc_bmac_set_defmacintmask(wlc_hw_info_t *wlc_hw, uint mask, uint val)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf = NULL;
	int err;
	rpc_info_t *rpc = wlc_hw->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2 * sizeof(uint32),
		WLRPC_WLC_BMAC_SET_DEFMACINTMASK_ID);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, mask);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, val);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);

	return;
}

void
wlc_phy_interf_rssi_update(wlc_phy_t *ppi, chanspec_t chanspec, int8 leastRSSI)
{
	bcm_xdr_buf_t b;
	rpc_buf_t *rpc_buf;
	int err;
	rpc_info_t *rpc = ppi->rpc;

	rpc_buf = wlc_rpc_buf_alloc(rpc, &b, 2 * sizeof(uint32), WLRPC_WLC_PHY_INTERF_RSSI_UPDATE);
	ASSERT(rpc_buf != NULL);

	err = bcm_xdr_pack_uint32(&b, (uint32)chanspec);
	ASSERT(!err);

	err = bcm_xdr_pack_uint32(&b, (uint32)leastRSSI);
	ASSERT(!err);

	err = wlc_rpc_call(rpc, rpc_buf);
	ASSERT(!err);
}
