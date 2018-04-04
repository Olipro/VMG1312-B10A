/*
 * Wrapper to scb rate selection algorithm of Broadcom
 * 802.11 Networking Adapter Device Driver.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_scb_ratesel.c 378413 2013-01-11 19:11:43Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>

#include <proto/802.11.h>
#include <d11.h>

#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>

#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wlc_scb_ratesel.h>
#ifdef WL_LPC
#include <wlc_scb_powersel.h>
#endif

#include <wl_dbg.h>

#ifdef WL11AC
#include <wlc_vht.h>
#endif

struct wlc_ratesel_info {
	wlc_info_t	*wlc;		/* pointer to main wlc structure */
	wlc_pub_t	*pub;		/* public common code handler */
	ratesel_info_t *rsi;
	int32 scb_handle;
	int32 cubby_sz;
};

typedef struct ratesel_cubby ratesel_cubby_t;

/* rcb is per scb per ac rate control block. */
struct ratesel_cubby {
	rcb_t *scb_cubby;
};
#define SCB_RATESEL_INFO(wss, scb) ((SCB_CUBBY((scb), (wrsi)->scb_handle)))

#if defined(WME_PER_AC_TX_PARAMS)
#define SCB_RATESEL_CUBBY(wrsi, scb, ac) 	\
	((void *)(((char*)((ratesel_cubby_t *)SCB_RATESEL_INFO(wrsi, scb))->scb_cubby) + \
		(ac * (wrsi)->cubby_sz)))
#define FID2AC(pub, fid) \
	(WME_PER_AC_MAXRATE_ENAB(pub) ? wme_fifo2ac[(fid) & TXFID_QUEUE_MASK] : 0)
#else /* WME_PER_AC_TX_PARAMS */
#define SCB_RATESEL_CUBBY(wrsi, scb, ac)	\
	(((ratesel_cubby_t *)SCB_RATESEL_INFO(wrsi, scb))->scb_cubby)
#define FID2AC(pub, fid) (0)
#endif /* WME_PER_AC_TX_PARAMS */

static int wlc_scb_ratesel_scb_init(void *context, struct scb *scb);
static void wlc_scb_ratesel_scb_deinit(void *context, struct scb *scb);
#ifdef BCMDBG
extern void wlc_scb_ratesel_dump_scb(void *ctx, struct scb *scb, struct bcmstrbuf *b);
#endif

static rcb_t *wlc_scb_ratesel_get_cubby(wlc_ratesel_info_t *wrsi, struct scb *scb,
	uint16 frameid);
static int wlc_scb_ratesel_cubby_sz(void);
#ifdef WL11N
void wlc_scb_ratesel_rssi_enable(rssi_ctx_t *ctx);
void wlc_scb_ratesel_rssi_disable(rssi_ctx_t *ctx);
int wlc_scb_ratesel_get_rssi(rssi_ctx_t *ctx);
#endif

wlc_ratesel_info_t *
BCMATTACHFN(wlc_scb_ratesel_attach)(wlc_info_t *wlc)
{
	wlc_ratesel_info_t *wrsi;

	if (!(wrsi = (wlc_ratesel_info_t *)MALLOC(wlc->osh, sizeof(wlc_ratesel_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)wrsi, sizeof(wlc_ratesel_info_t));
	wrsi->wlc = wlc;
	wrsi->pub = wlc->pub;

	if ((wrsi->rsi = wlc_ratesel_attach(wlc)) == NULL) {
		WL_ERROR(("%s: failed\n", __FUNCTION__));
		goto fail;
	}

	/* reserve cubby in the scb container for per-scb-ac private data */
	wrsi->scb_handle = wlc_scb_cubby_reserve(wlc, wlc_scb_ratesel_cubby_sz(),
	                                        wlc_scb_ratesel_scb_init,
	                                        wlc_scb_ratesel_scb_deinit,
#ifdef BCMDBG
	                                        wlc_scb_ratesel_dump_scb,
#else
	                                        NULL,
#endif
	                                        (void *)wlc);


	if (wrsi->scb_handle < 0) {
		WL_ERROR(("wl%d: %s:wlc_scb_cubby_reserve failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wrsi->cubby_sz = wlc_ratesel_rcb_sz();

#ifdef WL11N
	wlc_ratesel_rssi_attach(wrsi->rsi, wlc_scb_ratesel_rssi_enable,
		wlc_scb_ratesel_rssi_disable, wlc_scb_ratesel_get_rssi);
#endif

	return wrsi;

fail:
	if (wrsi->rsi)
		wlc_ratesel_detach(wrsi->rsi);

	MFREE(wlc->osh, wrsi, sizeof(wlc_ratesel_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_scb_ratesel_detach)(wlc_ratesel_info_t *wrsi)
{
	if (!wrsi)
		return;

	wlc_ratesel_detach(wrsi->rsi);

	MFREE(wrsi->pub->osh, wrsi, sizeof(wlc_ratesel_info_t));
}

/* alloc per ac cubby space on scb attach. */
static int
wlc_scb_ratesel_scb_init(void *context, struct scb *scb)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_cubby_t *cubby_info = SCB_RATESEL_INFO(wrsi, scb);
	rcb_t *scb_rate_cubby;
	int cubby_size;

#if defined(WME_PER_AC_TX_PARAMS)
	cubby_size = AC_COUNT * wrsi->cubby_sz;
#else
	cubby_size = wrsi->cubby_sz;
#endif

	WL_RATE(("%s scb %p allocate cubby space.\n", __FUNCTION__, scb));
	if (scb && !SCB_INTERNAL(scb)) {
		scb_rate_cubby = (rcb_t *)MALLOC(wlc->osh, cubby_size);
		if (!scb_rate_cubby)
			return BCME_NOMEM;
		bzero(scb_rate_cubby, cubby_size);
		cubby_info->scb_cubby = scb_rate_cubby;
	}
	return BCME_OK;
}

/* free cubby space after scb detach */
static void
wlc_scb_ratesel_scb_deinit(void *context, struct scb *scb)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	ratesel_cubby_t *cubby_info = SCB_RATESEL_INFO(wrsi, scb);
	int cubby_size;

#if defined(WME_PER_AC_TX_PARAMS)
	cubby_size = AC_COUNT * wrsi->cubby_sz;
#else
	cubby_size = wrsi->cubby_sz;
#endif

	WL_RATE(("%s scb %p free cubby space.\n", __FUNCTION__, scb));
	if (wlc && cubby_info && !SCB_INTERNAL(scb) && cubby_info->scb_cubby) {
		MFREE(wlc->osh, cubby_info->scb_cubby, cubby_size);
		cubby_info->scb_cubby = NULL;
	}
}

#ifdef BCMDBG
extern void
wlc_scb_ratesel_dump_scb(void *ctx, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	int32 ac;
	rcb_t *rcb;

	for (ac = 0; ac < WME_MAX_AC(wlc, scb); ac++) {
		rcb = SCB_RATESEL_CUBBY(wrsi, scb, ac);
		wlc_ratesel_dump_rcb(rcb, ac, b);
	}
}
#endif

static rcb_t *
wlc_scb_ratesel_get_cubby(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid)
{
	int ac;

	ASSERT(wrsi);

	ac = FID2AC(wrsi->pub, frameid);
	ASSERT(ac < AC_COUNT);
	return (SCB_RATESEL_CUBBY(wrsi, scb, ac));
}


extern const uint8 prio2fifo[NUMPRIO];
extern int
wlc_wme_downgrade_fifo(wlc_info_t *wlc, uint* p_fifo, struct scb *scb);

/* given only wlc and scb, return best guess at the primary rate */
ratespec_t
wlc_scb_ratesel_get_primary(wlc_info_t *wlc, struct scb *scb, void *pkt)
{
	uint16 frameid = 0, seq = 0;
	/* appears ok to pass in bogus ones as not read/chked in gettxrate chain */
	uint16 rate_flag = 0;
	ratesel_txparams_t cur_rate;
	ratespec_t rspec = 0;
	wlcband_t *scbband = wlc_scbband(scb);
	uint phyctl1_stf = wlc->stf->ss_opmode;
	uint32 mimo_txbw = 0;
	uint fifo, tfifo;
	uint8 prio;

#if defined(BCMDBG) || defined(WLTEST)
	uint32 _txbw2rspecbw[] = {
		RSPEC_BW_20MHZ, /* WL_TXBW20L	*/
		RSPEC_BW_20MHZ, /* WL_TXBW20U	*/
		RSPEC_BW_40MHZ, /* WL_TXBW40	*/
		RSPEC_BW_40MHZ, /* WL_TXBW40DUP */
	};
#endif /* defined(BCMDBG) || defined(WLTEST) */


	prio = 0;
	if ((pkt != NULL) && SCB_QOS(scb)) {
		prio = (uint8)PKTPRIO(pkt);
		ASSERT(prio <= MAXPRIO);
	}

	fifo = TX_AC_BE_FIFO;

	if (BSSCFG_AP(scb->bsscfg) && SCB_ISMULTI(scb) &&
		WLC_BCMC_PSMODE(wlc, scb->bsscfg)) {
		fifo = TX_BCMC_FIFO;
	}
	else if (SCB_WME(scb)) {
		fifo = prio2fifo[prio];
		tfifo = fifo;
#ifdef	WME
		if (wlc_wme_downgrade_fifo(wlc, &fifo, scb) == BCME_ERROR) {
			/* packet may be tossed; give a best guess anyway */
			fifo = tfifo;
		}
#endif /* WME */
	}

	seq = (wlc->counter << SEQNUM_SHIFT);
	/* get best guess at frameid */
	frameid = ((seq << TXFID_SEQ_SHIFT) & TXFID_SEQ_MASK) |
		(TX_AC_BE_FIFO & TXFID_QUEUE_MASK);

	if (scbband == NULL) {
		ASSERT(0);
		return 0;
	}
/* need to consolidate this */
#ifdef WL11N
	if (N_ENAB(wlc->pub)) {
		if (RSPEC_ACTIVE(scbband->rspec_override)) {
			/* get override if active */
			rspec = scbband->rspec_override;
		} else {
			/* let ratesel figure it out if override not present */
			wlc_scb_ratesel_gettxrate(wlc->wrsi, scb, &frameid,
				&cur_rate, &rate_flag);
			if (cur_rate.num > 0) {
				rspec = cur_rate.rspec[0];
			} else {
				rspec = 0;
			}
		}
		/* apply siso/cdd to single stream mcs's or ofdm if rspec is auto selected */
		if (((IS_MCS(rspec) && IS_SINGLE_STREAM(rspec & RSPEC_RATE_MASK)) ||
			IS_OFDM(rspec)) &&
			!(rspec & RSPEC_OVERRIDE_MODE)) {

			rspec &= ~(RSPEC_TXEXP_MASK | RSPEC_STBC);

			/* For SISO MCS use STBC if possible */
			if (IS_MCS(rspec) && WLC_STF_SS_STBC_TX(wlc, scb) &&
				(!scb->ht_mimops_enabled || scb->ht_mimops_rtsmode)) {
				ASSERT(WLC_STBC_CAP_PHY(wlc));
				rspec |= RSPEC_STBC;
			} else if (phyctl1_stf == PHY_TXC1_MODE_CDD) {
				rspec |= (1 << RSPEC_TXEXP_SHIFT);
			}
		}

		/* bandwidth */
		if (CHSPEC_IS80(wlc->chanspec) && RSPEC_ISVHT(rspec)) {
			mimo_txbw = RSPEC_BW_80MHZ;
		} else if (CHSPEC_IS40(wlc->chanspec) || CHSPEC_IS80(wlc->chanspec)) {
			/* default txbw is 20in40 */
			mimo_txbw = RSPEC_BW_20MHZ;

			if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
				if (scb->flags & SCB_IS40) {
					mimo_txbw = RSPEC_BW_40MHZ;
#ifdef WLMCHAN
				if (MCHAN_ENAB(wlc->pub) && BSSCFG_AP(scb->bsscfg) &&
					CHSPEC_IS20(scb->bsscfg->current_bss->chanspec)) {
					mimo_txbw = RSPEC_BW_20MHZ;
				}
#endif /* WLMCHAN */
				}
			}

#if defined(BCMDBG) || defined(WLTEST)
			/* use txbw overrides */
			if (RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) {
				if (wlc->mimo_40txbw != AUTO) {
					mimo_txbw = _txbw2rspecbw[wlc->mimo_40txbw];
				}
			} else if (IS_OFDM(rspec)) {
				if (wlc->ofdm_40txbw != AUTO) {
					mimo_txbw = _txbw2rspecbw[wlc->ofdm_40txbw];
				}
			} else {
				ASSERT(IS_CCK(rspec));
				if (wlc->cck_40txbw != AUTO) {
					mimo_txbw = _txbw2rspecbw[wlc->cck_40txbw];
				}
			}
#endif /* defined(BCMDBG) || defined(WLTEST) */
		} else	{
			mimo_txbw = RSPEC_BW_20MHZ;
		}
		rspec &= ~RSPEC_BW_MASK;
		rspec |= mimo_txbw;
	} else
#endif /* WL11N */
	{
		rspec |= RSPEC_BW_20MHZ;
		/* for nphy, stf of ofdm frames must follow policies */
		if ((WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band)) && IS_OFDM(rspec)) {
			rspec &= ~RSPEC_TXEXP_MASK;
			if (phyctl1_stf == PHY_TXC1_MODE_CDD) {
				rspec |= (1 << RSPEC_TXEXP_SHIFT);
			}
		}
	}

	if (!RSPEC_ACTIVE(wlc->band->rspec_override)) {
		if (IS_MCS(rspec) && (wlc->sgi_tx == ON))
			rspec |= RSPEC_SHORT_GI;
		else if (wlc->sgi_tx == OFF)
			rspec &= ~RSPEC_SHORT_GI;

	}

	if (!RSPEC_ACTIVE(wlc->band->rspec_override)) {
		ASSERT(!(rspec & RSPEC_LDPC_CODING));
		rspec &= ~RSPEC_LDPC_CODING;
		if (wlc->stf->ldpc_tx == ON ||
			(SCB_LDPC_CAP(scb) && wlc->stf->ldpc_tx == AUTO)) {
			if (IS_MCS(rspec))
				rspec |= RSPEC_LDPC_CODING;
		}
	}
	return rspec;
}

/* wrapper function to select transmit rate given per-scb state */
void BCMFASTPATH
wlc_scb_ratesel_gettxrate(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 *frameid,
	ratesel_txparams_t *cur_rate, uint16 *flags)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, *frameid);
	if (state == NULL) {
		WL_ERROR(("%s: null state wrsi = %p scb = %p frameid = %d\n",
			__FUNCTION__, wrsi, scb, *frameid));
		ASSERT(0);
		cur_rate->rspec[0] = WLC_RATE_6M;
		return;
	}
	wlc_ratesel_gettxrate(state, frameid, cur_rate, flags);
}

#ifdef WL11N
void
wlc_scb_ratesel_probe_ready(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	bool is_ampdu, uint8 ampdu_txretry)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	if (state == NULL) {
		WL_ERROR(("%s: null state wrsi = %p scb = %p frameid = %d\n",
			__FUNCTION__, wrsi, scb, frameid));
		ASSERT(0);
		return;
	}
	wlc_ratesel_probe_ready(state, frameid, is_ampdu, ampdu_txretry);
}

void BCMFASTPATH
wlc_scb_ratesel_upd_rxstats(wlc_ratesel_info_t *wrsi, ratespec_t rx_rspec, uint16 rxstatus2)
{
	wlc_ratesel_upd_rxstats(wrsi->rsi, rx_rspec, rxstatus2);
}
#endif /* WL11N */

/* non-AMPDU txstatus rate update, default to use non-mcs rates only */
void
wlc_scb_ratesel_upd_txstatus_normalack(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs,
	uint16 sfbl, uint16 lfbl, uint8 tx_mcs, uint8 antselid, bool fbr)

{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, txs->frameid);
	if (state == NULL) {
		ASSERT(0);
		return;
	}

	wlc_ratesel_upd_txstatus_normalack(state, txs, sfbl, lfbl, tx_mcs, antselid, fbr);
}

#ifdef WL11N
void
wlc_scb_ratesel_aci_change(wlc_ratesel_info_t *wrsi, bool aci_state)
{
	wlc_ratesel_aci_change(wrsi->rsi, aci_state);
}

/*
 * Return the fallback rate of the specified mcs rate.
 * Ensure that is a mcs rate too.
 */
ratespec_t
wlc_scb_ratesel_getmcsfbr(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid, uint8 mcs)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	ASSERT(state);

	return (wlc_ratesel_getmcsfbr(state, frameid, mcs));
}

#ifdef WLAMPDU_MAC
/*
 * The case that (mrt+fbr) == 0 is handled as RTS transmission failure.
 */
void
wlc_scb_ratesel_upd_txs_ampdu(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	uint8 mrt, uint8 mrt_succ, uint8 fbr, uint8 fbr_succ,
	bool tx_error, uint8 tx_mcs, uint8 antselid)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	ASSERT(state);

	wlc_ratesel_upd_txs_ampdu(state, frameid, mrt, mrt_succ, fbr, fbr_succ, tx_error,
		tx_mcs, antselid);
}
#endif /* WLAMPDU_MAC */

/* update state upon received BA */
void BCMFASTPATH
wlc_scb_ratesel_upd_txs_blockack(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs,
	uint8 suc_mpdu, uint8 tot_mpdu, bool ba_lost, uint8 retry, uint8 fb_lim, bool tx_error,
	uint8 mcs, uint8 antselid)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, txs->frameid);
	ASSERT(state);

	wlc_ratesel_upd_txs_blockack(state, txs, suc_mpdu, tot_mpdu, ba_lost, retry, fb_lim,
		tx_error, mcs, antselid);
}
#endif /* WL11N */

bool
wlc_scb_ratesel_minrate(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs)
{
	rcb_t *state;

	state = wlc_scb_ratesel_get_cubby(wrsi, scb, txs->frameid);
	ASSERT(state);

	return (wlc_ratesel_minrate(state, txs));
}

/* initialize per-scb state utilized by rate selection
 *   ATTEN: this fcn can be called to "reinit", avoid dup MALLOC
 *   this new design makes this function the single entry points for any select_rates changes
 *   this function should be called when any its parameters changed: like bw or stream
 *   this function will build select_rspec[] with all constraint and rateselection will
 *      be operating on this constant array with reference to known_rspec[] for threshold
 */

void
wlc_scb_ratesel_init(wlc_info_t *wlc, struct scb *scb)
{
	wlc_ratesel_info_t *wrsi = wlc->wrsi;
	rcb_t *state;
	uint8 bw = BW_20MHZ;
	int8 sgi_tx = OFF;
	int8 ldpc_tx = OFF;
	uint8 active_antcfg_num = 0;
	uint8 antselid_init = 0;
	int32 ac;
	uint *txc_ptr;
	wlc_rateset_t new_rateset;
	chanspec_t chanspec = wlc->chanspec;

	if (SCB_INTERNAL(scb))
		return;
#ifdef WL11N
	if (WLANTSEL_ENAB(wlc))
		wlc_antsel_ratesel(wlc->asi, &active_antcfg_num, &antselid_init);

#ifdef WL11AC
	if (CHSPEC_IS80(chanspec) && SCB_VHT_CAP(scb))
		bw = BW_80MHZ;
	else
#endif /* WL11AC */
	if (((scb->flags & SCB_IS40) ? TRUE : FALSE) &&
	    (CHSPEC_IS40(chanspec) || CHSPEC_IS80(chanspec)))
		bw = BW_40MHZ;

	/* here bw derived from chanspec and capabilities */

#ifdef WL11AC
	/* process operating mode notification for channel bw */

	if ((SCB_HT_CAP(scb) || SCB_VHT_CAP(scb)) &&
		scb->oper_mode_enabled && !DOT11_OPER_MODE_RXNSS_TYPE(scb->oper_mode)) {
		if (DOT11_OPER_MODE_CHANNEL_WIDTH_20MHZ(scb->oper_mode))
			bw = BW_20MHZ;
		else if (DOT11_OPER_MODE_CHANNEL_WIDTH_40MHZ(scb->oper_mode) && bw == BW_80MHZ)
			bw = BW_40MHZ;

		/* here if bw == 40 && oper_mode_bw != 20 -> bw = 40
				if bw == 80 && oper_mode_bw != 20 && oper_mode_bw != 40 -> bw = 80
				if bw == 20 -> bw = 20
		*/
	}
#endif /* WL11AC */

	if (wlc->stf->ldpc_tx == AUTO) {
		if ((bw != BW_80MHZ && SCB_LDPC_CAP(scb)) ||
			(bw == BW_80MHZ && SCB_VHT_LDPC_CAP(scb)))
			ldpc_tx = AUTO;
	}

	if (wlc->sgi_tx == AUTO) {
		if ((bw == BW_40MHZ && (scb->flags2 & SCB2_SGI40_CAP)) ||
		    (bw == BW_20MHZ && (scb->flags2 & SCB2_SGI20_CAP)) ||
			(bw == BW_80MHZ && SCB_VHT_SGI80(scb)))
			sgi_tx = AUTO;

		/* Disable SGI Tx in 20MHz on IPA chips */
		if (bw == BW_20MHZ && wlc->stf->ipaon)
			sgi_tx = OFF;
	}
#endif /* WL11N */

#ifdef WL11AC
	/* Set up the mcsmap in scb->rateset.vht_mcsmap */
	if (SCB_VHT_CAP(scb))
	{
		/* Refresh the txmcsmap and rxmcsmap of our node */
		wlc_rateset_t rs;
		wlc_rateset_vhtmcs_build(&rs, wlc->stf->txstreams);
		wlc->vht_cap.tx_mcs_map = rs.vht_mcsmap;
		wlc_vht_upd_rate_mcsmap(wlc, scb, scb->vht_rxmcsmap);

		WL_RATE(("%s: scb->vht_rxmcsmap 0x%x txmcsmap 0x%x\n",
			__FUNCTION__,  scb->vht_rxmcsmap, wlc->vht_cap.tx_mcs_map));
	}
#endif /* WL11AC */

	txc_ptr = (uint *)scb;

	for (ac = 0; ac < WME_MAX_AC(wlc, scb); ac++) {
		uint8 vht_ratemask = 0;
		state = SCB_RATESEL_CUBBY(wrsi, scb, ac);

		if (state == NULL) {
			ASSERT(0);
			return;
		}

		/* Rates above per AC max rate, below per AC min are removed from the rateset */
		if (WME_PER_AC_MAXRATE_ENAB(wrsi->pub) && SCB_WME(scb))
			wlc_ratesel_filter_rateset(wrsi->rsi, &scb->rateset, &new_rateset, bw,
				wrsi->wlc->wme_max_rate[ac], 0);
		else
			bcopy(&scb->rateset, &new_rateset, sizeof(wlc_rateset_t));

#ifdef WL11N
		if (BSS_N_ENAB(wlc, scb->bsscfg)) {
			if (((scb->ht_mimops_enabled && !scb->ht_mimops_rtsmode) ||
				(wlc->stf->txstreams == 1) || (wlc->stf->siso_tx == 1))) {
				new_rateset.mcs[1] = 0;
				new_rateset.mcs[2] = 0;
			} else if (wlc->stf->txstreams == 2)
				new_rateset.mcs[2] = 0;
		}
#endif
		WL_RATE(("%s: scb 0x%p ac %d state 0x%p bw %s txstreams %d"
			" active_ant %d band %d vht:%u\n",
			__FUNCTION__, scb, ac, state, (bw == BW_20MHZ) ?
			"20" : ((bw == BW_40MHZ) ? "40" : "80"),
			wlc->stf->txstreams, active_antcfg_num,
			wlc->band->bandtype, SCB_VHT_CAP(scb)));

#ifdef WL11AC
		if (SCB_VHT_CAP(scb))
			vht_ratemask = scb->vht_ratemask;
#endif
		wlc_ratesel_init(wrsi->rsi, state, scb, txc_ptr, &new_rateset, bw, sgi_tx, ldpc_tx,
			vht_ratemask, active_antcfg_num, antselid_init);
	}

#ifdef WL_LPC
	if (LPC_ENAB(wlc))
		wlc_scb_lpc_init(wlc->wlpci, scb);
#endif
}

void
wlc_scb_ratesel_init_all(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb)
		wlc_scb_ratesel_init(wlc, scb);

#ifdef WL_LPC
	if (LPC_ENAB(wlc))
		wlc_scb_lpc_init_all(wlc->wlpci);
#endif
}

void
wlc_scb_ratesel_init_bss(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, cfg, scb) {
		wlc_scb_ratesel_init(wlc, scb);
	}
#ifdef WL_LPC
	if (LPC_ENAB(wlc))
		wlc_scb_lpc_init_bss(wlc->wlpci, cfg);
#endif
}

void
wlc_scb_ratesel_clr_gotpkts(wlc_ratesel_info_t *wrsi, struct scb *scb, tx_status_t *txs)
{
	rcb_t *state;
	state = wlc_scb_ratesel_get_cubby(wrsi, scb, txs->frameid);
	ASSERT(state);

	wlc_ratesel_clr_gotpkts(state);
}

static int wlc_scb_ratesel_cubby_sz(void)
{
	return (sizeof(struct ratesel_cubby));
}

#ifdef WL11N
void wlc_scb_ratesel_rssi_enable(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	scb->rssi_enabled++;
}

void wlc_scb_ratesel_rssi_disable(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	scb->rssi_enabled--;
}

int wlc_scb_ratesel_get_rssi(rssi_ctx_t *ctx)
{
	struct scb *scb = (struct scb *)ctx;

	if (BSSCFG_STA(scb->bsscfg))
		return scb->bsscfg->link->rssi;
#if defined(AP) || defined(WLTDLS)
	if (scb->rssi_enabled <= 0)
		WL_ERROR(("%s: scb %p rssi_enabled %d\n",
			__FUNCTION__, scb, scb->rssi_enabled));
	ASSERT(scb->rssi_enabled > 0);
	return wlc_scb_rssi(scb);
#endif
	return 0;
}
#endif /* WL11N */

#ifdef WL_LPC
/* External functions */
void
wlc_scb_ratesel_get_info(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid,
	uint8 rate_stab_thresh, uint32 *new_rate_kbps, bool *rate_stable,
	rate_lcb_info_t *lcb_info)
{
	rcb_t *state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	wlc_ratesel_get_info(state, rate_stab_thresh, new_rate_kbps, rate_stable, lcb_info);
	return;
}

void
wlc_scb_ratesel_reset_vals(wlc_ratesel_info_t *wrsi, struct scb *scb, uint8 ac)
{
	rcb_t *state = NULL;

	if (!scb)
		return;

	state = SCB_RATESEL_CUBBY(wrsi, scb, ac);
	wlc_ratesel_lpc_init(state);
	return;
}

void
wlc_scb_ratesel_clr_cache(wlc_ratesel_info_t *wrsi, struct scb *scb, uint16 frameid)
{
	rcb_t *state = wlc_scb_ratesel_get_cubby(wrsi, scb, frameid);
	wlc_ratesel_clr_cache(state);
	return;
}
#endif /* WL_LPC */
