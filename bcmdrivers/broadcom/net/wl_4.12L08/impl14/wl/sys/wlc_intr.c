/*
 * Common (OS-independent) portion of
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Interrupt/dpc handlers of common driver. Shared by BMAC driver, High driver,
 * and Full driver.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_intr.c 350356 2012-08-13 12:05:23Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_channel.h>
#include <wlc_pio.h>
#include <wlc_rm.h>
#ifdef WLC_HIGH
#include <wlc_key.h>
#endif /* WLC_HIGH */
/* BMAC_NOTE: a WLC_HIGH compile include of wlc.h adds in more structures and type
 * dependencies. Need to include these to files to allow a clean include of wlc.h
 * with WLC_HIGH defined.
 * At some point we may be able to skip the include of wlc.h and instead just
 * define a stub wlc_info and band struct to allow rpc calls to get the rpc handle.
 */
#include <wlc.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#ifdef WLOFFLD
#include <wlc_offloads.h>
#endif
#ifdef WLC_HIGH
#include <wlc_ap.h>
#include <wl_export.h>
#include <wlc_extlog.h>
#include <wlc_11h.h>
#include <wlc_quiet.h>
#include <wlc_hrt.h>
#endif /* WLC_HIGH */

#ifdef WLC_HIGH
#ifndef WLC_PHYTXERR_THRESH
#define WLC_PHYTXERR_THRESH 20
#endif
uint wlc_phytxerr_thresh = WLC_PHYTXERR_THRESH;
#endif /* WLC_HIGH */

#ifdef BCMDBG
static const bcm_bit_desc_t int_flags[] = {
	{MI_MACSSPNDD,	"MACSSPNDD"},
	{MI_BCNTPL,	"BCNTPL"},
	{MI_TBTT,	"TBTT"},
	{MI_BCNSUCCESS,	"BCNSUCCESS"},
	{MI_BCNCANCLD,	"BCNCANCLD"},
	{MI_ATIMWINEND,	"ATIMWINEND"},
	{MI_PMQ,	"PMQ"},
	{MI_NSPECGEN_0,	"NSPECGEN_0"},
	{MI_NSPECGEN_1,	"NSPECGEN_1"},
	{MI_MACTXERR,	"MACTXERR"},
	{MI_NSPECGEN_3,	"NSPECGEN_3"},
	{MI_PHYTXERR,	"PHYTXERR"},
	{MI_PME,	"PME"},
	{MI_GP0,	"GP0"},
	{MI_GP1,	"GP1"},
	{MI_DMAINT,	"DMAINT"},
	{MI_TXSTOP,	"TXSTOP"},
	{MI_CCA,	"CCA"},
	{MI_BG_NOISE,	"BG_NOISE"},
	{MI_DTIM_TBTT,	"DTIM_TBTT"},
	{MI_PRQ,	"PRQ"},
	{MI_PWRUP,	"PWRUP"},
	{MI_BT_RFACT_STUCK,	"BT_RFACT_STUCK"},
	{MI_BT_PRED_REQ,	"BT_PRED_REQ"},
	{MI_RFDISABLE,	"RFDISABLE"},
	{MI_TFS,	"TFS"},
	{MI_PHYCHANGED,	"PHYCHANGED"},
	{MI_TO,		"TO"},
	{MI_P2P,	"P2P"},
	{0, NULL}
};
#endif /* BCMDBG */

#ifndef WLC_HIGH_ONLY
/* second-level interrupt processing
 *   Return TRUE if another dpc needs to be re-scheduled. FALSE otherwise.
 *   Param 'bounded' indicates if applicable loops should be bounded.
 *   Param 'dpc' returns info such as how many packets have been received/processed.
 */
bool BCMFASTPATH
wlc_dpc(wlc_info_t *wlc, bool bounded, wlc_dpc_info_t *dpc)
#else /* WLC_HIGH_ONLY */
/*
 * This fn has all the high level dpc processing from wlc_dpc.
 * POLICY: no macinstatus change, no bounding loop.
 *         All dpc bounding should be handled in BMAC dpc, like txstatus and rxint
 */
void /* WLC_HIGH_ONLY */
wlc_high_dpc(wlc_info_t *wlc, uint32 macintstatus)
#endif /* WLC_HIGH_ONLY */
{
#ifndef WLC_HIGH_ONLY
	uint32 macintstatus;
#endif
	wlc_hw_info_t *wlc_hw = wlc->hw;
	d11regs_t *regs = wlc->regs;
#ifndef WLC_HIGH_ONLY
	bool fatal = FALSE;
#endif

#ifndef WLC_HIGH_ONLY
#ifdef WLC_HIGH
	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return FALSE;
	}
#endif

	/* grab and clear the saved software intstatus bits */
	macintstatus = wlc_hw->macintstatus;
	wlc_hw->macintstatus = 0;
#endif /* !WLC_HIGH_ONLY */

#ifdef BCMDBG
	{
	char flagstr[128];
#ifdef WLC_HIGH
	uint unit = wlc->pub->unit;
#else
	uint unit = wlc_hw->unit;
#endif
	bcm_format_flags(int_flags, macintstatus, flagstr, sizeof(flagstr));
	WL_TRACE(("wl%d: %s: macintstatus 0x%x %s\n", unit, __FUNCTION__, macintstatus, flagstr));
	}
#endif /* BCMDBG */

#ifndef WLC_HIGH_ONLY
#ifdef STA
	if (macintstatus & MI_BT_PRED_REQ)
		wlc_bmac_btc_update_predictor(wlc_hw);
#endif
#endif /* !WLC_HIGH_ONLY */

#ifdef WLC_HIGH
	/* TBTT indication */
	/* ucode only gives either TBTT or DTIM_TBTT, not both */
	if (macintstatus & (MI_TBTT | MI_DTIM_TBTT)) {
#ifdef RADIO_PWRSAVE
		/* Enter power save mode */
		wlc_radio_pwrsave_enter_mode(wlc, ((macintstatus & MI_DTIM_TBTT) != 0));
#endif /* RADIO_PWRSAVE */
#ifdef MBSS
		if (MBSS_ENAB(wlc->pub)) {
			if (MBSS_ENAB16(wlc->pub))
				(void)wlc_ap_mbss16_tbtt(wlc, macintstatus);
			else
				(void)wlc_ap_mbss4_tbtt(wlc, macintstatus);
		}
#endif /* MBSS */

		wlc_tbtt(wlc, regs);
	}

	/* Process probe request FIFO */
	if (macintstatus & MI_PRQ) {
#ifdef MBSS
#if defined(WLC_HIGH) && defined(WLC_LOW)
		if (MBSS_ENAB(wlc->pub)) {
			bool bound = bounded;

			/* Only used by the soft PRQ */
			ASSERT(MBSS_ENAB4(wlc->pub));
			if (!MBSS_ENAB16(wlc->pub) &&
			    wlc_prq_process(wlc, bound)) {
				wlc_hw->macintstatus |= MI_PRQ;
			}
		}
#endif /* WLC_HIGH && WLC_LOW */
#else
		ASSERT(!"PRQ Interrupt in non-MBSS");
#endif /* MBSS */
	}

	/* BCN template is available */
	/* ZZZ: Use AP_ACTIVE ? */
	if (macintstatus & MI_BCNTPL) {
		if (AP_ENAB(wlc->pub) && (!APSTA_ENAB(wlc->pub) || wlc->aps_associated)) {
			WL_APSTA_BCN(("wl%d: wlc_dpc: template -> wlc_update_beacon()\n",
			              wlc->pub->unit));
			wlc_update_beacon(wlc);
		}
	}
#endif /* WLC_HIGH */

#ifndef WLC_HIGH_ONLY
	/* PMQ entry addition */
	if (macintstatus & MI_PMQ) {
#ifdef AP
		if (wlc_bmac_processpmq(wlc_hw, bounded))
			wlc_hw->macintstatus |= MI_PMQ;
#endif
	}

	/* tx status */
	if (macintstatus & MI_TFS) {
		if (wlc_bmac_txstatus(wlc_hw, bounded, &fatal))
			wlc_hw->macintstatus |= MI_TFS;
#ifdef WLC_HIGH
		if (fatal) {
			WL_ERROR(("wl%d: %s: txstatus fatal error\n",
				wlc_hw->unit, __FUNCTION__));
			goto fatal;
		}
#endif
	}

	/* ATIM window end */
	if (macintstatus & MI_ATIMWINEND) {
		WL_TRACE(("wlc_isr: end of ATIM window\n"));

		OR_REG(wlc->osh, &regs->maccommand, wlc->qvalid);
		wlc->qvalid = 0;
	}

	/* phy tx error */
	if (macintstatus & MI_PHYTXERR) {
		if (D11REV_IS(wlc_hw->corerev, 11) || D11REV_IS(wlc_hw->corerev, 12))
			WL_INFORM(("wl%d: PHYTX error\n", wlc_hw->unit));
		else
			WL_ERROR(("wl%d: PHYTX error\n", wlc_hw->unit));
		WLCNTINCR(wlc->pub->_cnt->txphyerr);
		wlc_hw->phyerr_cnt++;
#ifdef BCMDBG
#ifdef WLC_HIGH
		wlc_dump_phytxerr(wlc, 0);
#endif /* WLC_HIGH */
#endif /* BCMDBG */
	} else
		wlc_hw->phyerr_cnt = 0;

#ifdef WLC_HIGH
	if (WLCISSSLPNPHY(wlc_hw->band) && (wlc_hw->phyerr_cnt >= wlc_phytxerr_thresh)) {
		WL_ERROR(("wl%d: HAMMERING!\n", wlc_hw->unit));
		wl_init(wlc->wl);
	}
#endif /* WLC_HIGH */


#if defined(BCMPKTPOOL) && defined(DMATXRC)
	/* Opportunistically reclaim tx buffers */
	if (macintstatus & (MI_DMAINT | MI_DMATX | MI_TFS)) {
		/* Reclaim more pkts */
		if (DMATXRC_ENAB(wlc->pub))
			wlc_dmatx_reclaim(wlc_hw);
	}
#endif

#ifdef WLRXOV
	if (macintstatus & MI_RXOV) {
		if (WLRXOV_ENAB(wlc->pub))
			wlc_rxov_int(wlc);
	}
#endif

	/* received data or control frame, MI_DMAINT is indication of RX_FIFO interrupt */
	if (macintstatus & MI_DMAINT) {
#ifdef WLC_HIGH
		if ((BUSTYPE(wlc_hw->sih->bustype) == PCI_BUS) &&
		    wlc_hw->dma_lpbk) {
			wlc_recover_pkts(wlc_hw->wlc, TX_DATA_FIFO);
			dma_rxfill(wlc_hw->di[RX_FIFO]);
		} else
#endif /* WLC_HIGH */
		if (wlc_bmac_recv(wlc_hw, RX_FIFO, bounded, dpc)) {
			wlc_hw->macintstatus |= MI_DMAINT;
		}
	}
#endif /* !WLC_HIGH_ONLY */

#ifdef WLC_HIGH
	/* TX FIFO suspend/flush completion */
	if (macintstatus & MI_TXSTOP) {
		if (wlc_bmac_tx_fifo_suspended(wlc_hw, TX_DATA_FIFO)) {
			wlc_txstop_intr(wlc);
		}
	}
#endif /* WLC_HIGH */

#ifndef WLC_HIGH_ONLY
	/* noise sample collected */
	if (macintstatus & MI_BG_NOISE) {
		WL_NONE(("wl%d: got background noise samples\n", wlc_hw->unit));
		wlc_phy_noise_sample_intr(wlc_hw->band->pi);
	}

#if defined(STA) && defined(WLRM)
	if (macintstatus & MI_CCA) {		/* CCA measurement complete */
		WL_INFORM(("wl%d: CCA measurement interrupt\n", wlc_hw->unit));
		wlc_bmac_rm_cca_int(wlc_hw);
	}
#endif
#endif /* !WLC_HIGH_ONLY */

#ifdef WLC_HIGH
	if (macintstatus & MI_GP0) {
		WL_ERROR(("wl%d: PSM microcode watchdog fired at %d (seconds). Resetting.\n",
			wlc->pub->unit, wlc->pub->now));
		/* BMAC_NOTE: maybe make this a WLC_HIGH_API ? */
		wlc_dump_ucode_fatal(wlc);
		if (!((CHIPID(wlc->pub->sih->chip) == BCM4321_CHIP_ID) &&
		      (CHIPREV(wlc->pub->sih->chiprev) == 0))) {
			WLC_EXTLOG(wlc, LOG_MODULE_COMMON, FMTSTR_REG_PRINT_ID, WL_LOG_LEVEL_ERR,
				0, R_REG(wlc->osh, &regs->psmdebug), "psmdebug");
			WLC_EXTLOG(wlc, LOG_MODULE_COMMON, FMTSTR_REG_PRINT_ID, WL_LOG_LEVEL_ERR,
				0, R_REG(wlc->osh, &regs->phydebug), "phydebug");
			WLC_EXTLOG(wlc, LOG_MODULE_COMMON, FMTSTR_REG_PRINT_ID, WL_LOG_LEVEL_ERR,
				0, R_REG(wlc->osh, &regs->psm_brc), "psm_brc");

			ASSERT(!"PSM Watchdog");
		}

		WLCNTINCR(wlc->pub->_cnt->psmwds);

		/* big hammer */
		WL_ERROR(("wl%d: %s: macintstatus & MI_GP0 non-zero, HAMMERING\n",
			wlc->pub->unit, __FUNCTION__));
		wl_init(wlc->wl);
	}

	/* gptimer timeout */
	if (macintstatus & MI_TO) {
		wlc_hrt_isr(wlc->hrti);
	}

#ifdef STA
	if (macintstatus & MI_RFDISABLE) {
		wlc_rfd_intr(wlc);
	}
#endif /* STA */
#endif /* WLC_HIGH */

#ifndef WLC_HIGH_ONLY
	if ((macintstatus & MI_P2P) && DL_P2P_UC(wlc_hw))
		wlc_p2p_bmac_int_proc(wlc_hw);
#endif /* !WLC_HIGH_ONLY */

#ifdef WLC_HIGH
	/* send any enq'd tx packets. Just makes sure to jump start tx */
	if (!pktq_empty(&wlc->active_queue->q))
		wlc_send_q(wlc, wlc->active_queue);

#ifdef WLC_HIGH_ONLY
#else
	ASSERT(wlc_ps_check(wlc));
#endif
#endif	/* WLC_HIGH */

#ifdef WLC_LOW_ONLY
#define MACINTMASK_BMAC (MI_TFS | MI_ATIMWINEND | MI_PHYTXERR | MI_DMAINT | MI_BG_NOISE | \
			 MI_CCA | MI_GP0 | MI_PMQ | MI_P2P | \
			 0)
	macintstatus &= ~MACINTMASK_BMAC;
	if (macintstatus != 0) {
		wlc_high_dpc(wlc, macintstatus);
	}
#endif /* WLC_LOW_ONLY */

#if defined(BCMDBG) && !defined(WLC_LOW_ONLY)
	wlc_update_perf_stats(wlc, WLC_PERF_STATS_DPC);
#endif

#ifdef WLOFFLD
	if (WLOFFLD_ENAB(wlc->pub))
		wlc_ol_dpc(wlc->ol);
#endif

#ifndef WLC_HIGH_ONLY
	/* make sure the bound indication and the implementation are in sync */
	ASSERT(bounded == TRUE || wlc_hw->macintstatus == 0);

	goto exit;

#ifdef WLC_HIGH
fatal:
	wl_init(wlc->wl);
#endif
exit:
	/* it isn't done and needs to be resched if macintstatus is non-zero */
	return (wlc_hw->macintstatus != 0);
#endif /* !WLC_HIGH_ONLY */
}

#ifndef WLC_HIGH_ONLY
/*
 * Read and clear macintmask and macintstatus and intstatus registers.
 * This routine should be called with interrupts off
 * Return:
 *   -1 if DEVICEREMOVED(wlc) evaluates to TRUE;
 *   0 if the interrupt is not for us, or we are in some special cases;
 *   device interrupt status bits otherwise.
 */
static INLINE uint32
wlc_intstatus(wlc_info_t *wlc, bool in_isr)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	d11regs_t *regs = wlc_hw->regs;
	uint32 macintstatus, mask;
	uint32 intstatus_rxfifo, intstatus_txsfifo;
	osl_t *osh;

	osh = wlc_hw->osh;

	/* macintstatus includes a DMA interrupt summary bit */
	macintstatus = R_REG(osh, &regs->macintstatus);

	WL_TRACE(("wl%d: macintstatus: 0x%x\n", wlc_hw->unit, macintstatus));

	/* detect cardbus removed, in power down(suspend) and in reset */
	if (DEVICEREMOVED(wlc))
		return -1;

	/* DEVICEREMOVED succeeds even when the core is still resetting,
	 * handle that case here.
	 */
	if (macintstatus == 0xffffffff) {
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		return 0;
	}

	mask = (in_isr ? wlc_hw->macintmask : wlc_hw->defmacintmask);

	mask |= DELAYEDINTMASK;

	/* defer unsolicited interrupts */
	macintstatus &= mask;

	/* if not for us */
	if (macintstatus == 0)
		return 0;

#ifdef WLOFFLD
	/* Disable PCIE MB interrupt also until DPC is run */
	if (WLOFFLD_ENAB(wlc->pub))
		wlc_ol_enable_intrs(wlc->ol, FALSE);
#endif
	/* interrupts are already turned off for CFE build
	 * Caution: For CFE Turning off the interrupts again has some undesired
	 * consequences
	 */
#if !defined(_CFE_)
	/* turn off the interrupts */
	W_REG(osh, &regs->macintmask, 0);
	(void)R_REG(osh, &regs->macintmask);	/* sync readback */
	wlc_hw->macintmask = 0;
#endif /* !defined(_CFE_) */

#ifdef WLC_HIGH
	/* BMAC_NOTE: this code should not be in the ISR code, it should be in DPC.
	 * We can not modify wlc state except for very restricted items.
	 */
	if (macintstatus & MI_BT_RFACT_STUCK) {
		/* disable BT Coexist and WAR detection when stuck pin is detected
		 * This has to be done before clearing the macintstatus
		 */
		WL_ERROR(("Disabling BTC mode (BT_RFACT stuck detected)\n"));
		wlc_hw->btc->stuck_detected = TRUE;
		wlc_bmac_btc_mode_set(wlc_hw, WL_BTC_DISABLE);
		wlc_bmac_btc_stuck_war50943(wlc_hw, FALSE);
	}
#endif

	/* clear device interrupts */
	W_REG(osh, &regs->macintstatus, macintstatus);

	/* MI_DMAINT is indication of non-zero intstatus */
	if (macintstatus & MI_DMAINT) {
		if (D11REV_IS(wlc_hw->corerev, 4)) {
			intstatus_rxfifo = R_REG(osh, &regs->intctrlregs[RX_FIFO].intstatus);
			intstatus_txsfifo = R_REG(osh,
			                          &regs->intctrlregs[RX_TXSTATUS_FIFO].intstatus);
			WL_TRACE(("wl%d: intstatus_rxfifo 0x%x, intstatus_txsfifo 0x%x\n",
			          wlc_hw->unit, intstatus_rxfifo, intstatus_txsfifo));

			/* defer unsolicited interrupt hints */
			intstatus_rxfifo &= DEF_RXINTMASK;
			intstatus_txsfifo &= DEF_RXINTMASK;

			/* MI_DMAINT bit in macintstatus is indication of RX_FIFO interrupt */
			/* clear interrupt hints */
			if (intstatus_rxfifo)
				W_REG(osh,
				      &regs->intctrlregs[RX_FIFO].intstatus, intstatus_rxfifo);
			else
				macintstatus &= ~MI_DMAINT;

			/* MI_TFS bit in macintstatus is encoding of RX_TXSTATUS_FIFO interrupt */
			if (intstatus_txsfifo) {
				W_REG(osh, &regs->intctrlregs[RX_TXSTATUS_FIFO].intstatus,
				      intstatus_txsfifo);
				macintstatus |= MI_TFS;
			}
#if defined(DMA_TX_FREE)
			/* do not support DMA_TX_FREE for corerev 4 */
			ASSERT(0);
#endif
		} else {
#if !defined(DMA_TX_FREE)
			/*
			 * For corerevs >= 5, only fifo interrupt enabled is I_RI in RX_FIFO.
			 * If MI_DMAINT is set, assume it is set and clear the interrupt.
			 */
			W_REG(osh, &regs->intctrlregs[RX_FIFO].intstatus, DEF_RXINTMASK);
#else
			int i;

			/* clear rx and tx interrupts */
			for (i = 0; i < NFIFO; i++) {
				if (i == RX_FIFO)
					W_REG(osh, &regs->intctrlregs[i].intstatus,
					      DEF_RXINTMASK | I_XI);
				else if (wlc_hw->di[i] && dma_txactive(wlc_hw->di[i]))
					W_REG(osh, &regs->intctrlregs[i].intstatus, I_XI);
			}
#endif /* DMA_TX_FREE */
		}
	}

#if defined(BCMPKTPOOL) && defined(DMATXRC)
	/* Opportunistically reclaim tx buffers */
	if (macintstatus & (MI_DMAINT | MI_DMATX | MI_TFS)) {
		/* Reclaim pkts immediately */
		if (DMATXRC_ENAB(wlc->pub))
			wlc_dmatx_reclaim(wlc_hw);
	}
#endif

	return macintstatus;
}

/* Update wlc_hw->macintstatus and wlc_hw->intstatus[]. */
/* Return TRUE if they are updated successfully. FALSE otherwise */
bool
wlc_intrsupd(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint32 macintstatus;

	ASSERT(wlc_hw->macintstatus != 0);

	/* read and clear macintstatus and intstatus registers */
	macintstatus = wlc_intstatus(wlc, FALSE);

	/* device is removed */
	if (macintstatus == 0xffffffff) {
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		return FALSE;
	}

#if defined(WLC_HIGH) && defined(MBSS)
	if (MBSS_ENAB(wlc->pub) &&
	    (macintstatus & (MI_DTIM_TBTT | MI_TBTT))) {
		/* Record latest TBTT/DTIM interrupt time for latency calc */
		wlc->last_tbtt_us = R_REG(wlc_hw->osh, &wlc_hw->regs->tsf_timerlow);
	}
#endif /* WLC_HIGH && MBSS */

	/* update interrupt status in software */
	wlc_hw->macintstatus |= macintstatus;

	return TRUE;
}

/*
 * First-level interrupt processing.
 * Return TRUE if this was our interrupt, FALSE otherwise.
 * *wantdpc will be set to TRUE if further wlc_dpc() processing is required,
 * FALSE otherwise.
 */
bool BCMFASTPATH
wlc_isr(wlc_info_t *wlc, bool *wantdpc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint32 macintstatus;

	*wantdpc = FALSE;

	if (!wlc_hw->up || !wlc_hw->macintmask)
		return (FALSE);

	/* read and clear macintstatus and intstatus registers */
	macintstatus = wlc_intstatus(wlc, TRUE);

#ifdef WLOFFLD
	if (WLOFFLD_ENAB(wlc->pub)) {
		if (wlc_ol_intstatus(wlc->ol) && (macintstatus == 0)) {
			/* Disable MAC interrupts until DPC is run */
			wlc_intrsoff(wlc);
			*wantdpc = TRUE;
			return TRUE;
		}
	}
#endif

	if (macintstatus == 0xffffffff) {
		WL_HEALTH_LOG(wlc, DEADCHIP_ERROR);
		WL_ERROR(("DEVICEREMOVED detected in the ISR code path.\n"));
		/* in rare cases, we may reach this condition as a race condition may occur */
		/* between disabling interrupts and clearing the SW macintmask */
		/* clear mac int status as there is no valid interrupt for us */
		wlc_hw->macintstatus = 0;
		/* assume this is our interrupt as before; note: want_dpc is FALSE */
		return (TRUE);
	}

	/* it is not for us */
	if (macintstatus == 0)
		return (FALSE);

	*wantdpc = TRUE;

#ifdef WLC_HIGH
#if defined(MBSS)
	/* BMAC_NOTE: This mechanism to bail out of sending beacons in
	 * wlc_ap.c:wlc_ap_sendbeacons() does not seem like a good idea across a bus with
	 * non-negligible reg-read time. The time values in the code have
	 * wlc_ap_sendbeacons() bail out if the delay between the isr time and it is >
	 * 4ms. But the check is done in wlc_ap_sendbeacons() with a reg read that might
	 * take on order of 0.3ms.  Should mbss only be supported with beacons in
	 * templates instead of beacons from host driver?
	 */
	if (MBSS_ENAB(wlc->pub) &&
	    (macintstatus & (MI_DTIM_TBTT | MI_TBTT))) {
		/* Record latest TBTT/DTIM interrupt time for latency calc */
		wlc->last_tbtt_us = R_REG(wlc_hw->osh, &wlc_hw->regs->tsf_timerlow);
	}
#endif /* MBSS */
#endif	/* WLC_HIGH */

	/* save interrupt status bits */
	ASSERT(wlc_hw->macintstatus == 0);
	wlc_hw->macintstatus = macintstatus;

#if defined(BCMDBG) && !defined(WLC_LOW_ONLY)
	wlc_update_isr_stats(wlc, macintstatus);
#endif
	return (TRUE);
}

void
wlc_intrson(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;

	ASSERT(wlc_hw->defmacintmask);
	wlc_hw->macintmask = wlc_hw->defmacintmask;
	W_REG(wlc_hw->osh, &wlc_hw->regs->macintmask, wlc_hw->macintmask);
#ifdef WLOFFLD
	if (WLOFFLD_ENAB(wlc->pub))
	/* Enable PCIE MB interrupt */
		wlc_ol_enable_intrs(wlc->ol, TRUE);
#endif
}


uint32
wlc_intrsoff(wlc_info_t *wlc)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;
	uint32 macintmask;

	if (!wlc_hw->clk)
		return 0;

	macintmask = wlc_hw->macintmask;	/* isr can still happen */

	W_REG(wlc_hw->osh, &wlc_hw->regs->macintmask, 0);
	(void)R_REG(wlc_hw->osh, &wlc_hw->regs->macintmask);	/* sync readback */
	OSL_DELAY(1); /* ensure int line is no longer driven */
	wlc_hw->macintmask = 0;

	/* return previous macintmask; resolve race between us and our isr */
	return (wlc_hw->macintstatus ? 0 : macintmask);
}

void
wlc_intrsrestore(wlc_info_t *wlc, uint32 macintmask)
{
	wlc_hw_info_t *wlc_hw = wlc->hw;

	if (!wlc_hw->clk)
		return;

	wlc_hw->macintmask = macintmask;
	W_REG(wlc_hw->osh, &wlc_hw->regs->macintmask, wlc_hw->macintmask);
}
#endif /* !WLC_HIGH_ONLY */
