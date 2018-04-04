/*
 * MBSS Feature [public interface] definitions for
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
 * $Id: wlc.h,v 1.1832 2011/01/14 20:04:57 Exp $
 */

#ifndef _wlc_mbss_h_
#define _wlc_mbss_h_

#include <wlc_types.h>

#include <wl_dbg.h>
#include <wlc_pub.h>

#include <d11.h>
#include <wlc_bsscfg.h>
#include <wlc_hw.h>


struct wlc_mbss_info_t;

/* BCMC_FID_INIT - Set driver and shm FID to invalid */
#define BCMC_FID_INIT(bsscfg) do { \
		(bsscfg)->bcmc_fid = INVALIDFID; \
		(bsscfg)->bcmc_fid_shm = INVALIDFID; \
	} while (0)

/* ************ bsscfg.h related ****************** */

/* Macros related to Multi-BSS. */
#if defined(MBSS)
#define SOFTBCN_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_SW_BCN) != 0)
#define SOFTPRB_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_SW_PRB) != 0)
#define UCTPL_MBSS_ENAB(cfg)		(((cfg)->flags & WLC_BSSCFG_MBSS16) != 0)
/* Define as all bits less than and including the msb shall be one's */
#define EADDR_TO_UC_IDX(eaddr, mask)	((eaddr).octet[5] & (mask))
#define MBSS_BCN_ENAB(cfg)		(SOFTBCN_ENAB(cfg) || UCTPL_MBSS_ENAB(cfg))
#define MBSS_PRB_ENAB(cfg)		(SOFTPRB_ENAB(cfg) || UCTPL_MBSS_ENAB(cfg))

#ifdef WLC_HIGH
/*
 * BC/MC FID macros.  Only valid under MBSS
 *
 *    BCMC_FID_SHM_COMMIT  Committing FID to SHM; move driver's value to bcmc_fid_shm
 *    BCMC_FID_QUEUED	   Are any packets enqueued on the BC/MC fifo?
 */

extern void wlc_mbss_bcmc_fid_shm_commit(wlc_bsscfg_t *bsscfg);
#define BCMC_FID_SHM_COMMIT(bsscfg) wlc_mbss_bcmc_fid_shm_commit(bsscfg)

#define BCMC_PKTS_QUEUED(bsscfg) \
	(((bsscfg)->bcmc_fid_shm != INVALIDFID) || ((bsscfg)->bcmc_fid != INVALIDFID))

extern int wlc_write_mbss_basemac(struct wlc_mbss_info_t *mbss);
#endif /* WLC_HIGH */

extern bool wlc_mbss_ucode_hwcap(wlc_info_t *wlc);

/* ****************  from wlc_ap ******** */
#ifdef WLC_HIGH

extern int wlc_mbss4_tbtt(wlc_info_t *wlc, uint32 macintstatus);
extern int wlc_mbss16_tbtt(wlc_info_t *wlc, uint32 macintstatus);
extern void wlc_mbss16_write_prbrsp(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	bool suspend);

extern bool bcmc_pkt_q_check(wlc_info_t *wlc, struct scb *bcmc_scb, wlc_pkt_t pkt);

#endif

/* ****************  from wlc_apps ******** */
void wlc_mbss_apps_bss_ps_off_start(wlc_info_t *wlc, struct scb *bcmc_scb);
extern void wlc_mbss_apps_bss_ps_off_done(wlc_info_t *wlc,
	wlc_bsscfg_t *bsscfg);
extern void wlc_mbss_apps_bss_ps_on_done(wlc_info_t *wlc);

#ifdef WLC_HIGH
extern void wlc_apps_update_bss_bcmc_fid(wlc_info_t *wlc);
extern int wlc_apps_bcmc_ps_enqueue(wlc_info_t *wlc, struct scb *bcmc_scb,
	void *pkt);

bool wlc_mbss_bcmc_pkt_q_check(wlc_info_t *wlc, struct scb *bcmc_scb,
	wlc_pkt_t pkt);

void wlc_mbss_upd_bcmc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint16 *frameid);

void wlc_mbss_dotxstatus_bcmc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	uint16 frameid);

wlc_spt_t *wlc_mbss_get_bcm_template(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

wlc_pkt_t wlc_mbss_prepare_prb_template(wlc_info_t *wlc,
	wlc_bsscfg_t *bsscfg, uint8 **pbody);

void wlc_mbss_bsscfg_set_prb_modif(struct wlc_mbss_info_t *mbss,
	wlc_bsscfg_t *cfg, bool flag);

uint32 wlc_mbss_bsscfg_get_mc_fifo_pkts(struct wlc_mbss_info_t *mbss,
	wlc_bsscfg_t *cfg);

#endif /* WLC_HIGH */

#else /* defined (MBSS) */

#define SOFTBCN_ENAB(pub)    (0)
#define SOFTPRB_ENAB(pub)    (0)
#define wlc_write_mbss_basemac(x) (0)

#define BCMC_FID_SHM_COMMIT(bsscfg)
#define BCMC_PKTS_QUEUED(bsscfg) 0
#define MBSS_BCN_ENAB(cfg)       0
#define MBSS_PRB_ENAB(cfg)       0
#define UCTPL_MBSS_ENAB(cfg)         0

/* ************* from wlc.h *************** */
#define wlc_ucodembss_hwcap(a) 0
#define wlc_shm_ssid_get(wlc, idx, ssid) do {} while (0)


/* ****************  from wlc_ap ******** */
#define wlc_ap_mbss4_tbtt(a, b) 0
#define wlc_ap_mbss16_tbtt(a, b) 0
#define wlc_ap_mbss16_write_prbrsp(a, b, c) 0

#define wlc_mbss_bcmc_pkt_q_check(wlc, scb, pkt) 		(0)

/*****************  from wlc_apps ******** */
#define wlc_mbss_apps_bss_ps_off_start(wlc, bcmc_scb)
#define wlc_apps_bss_ps_off_done(wlc, bsscfg)
#define wlc_apps_bss_ps_on_done(wlc)
#define wlc_apps_update_bss_bcmc_fid(wlc)
#define wlc_mbss_upd_bcmc(wlc, bsscfg, frameid)

#endif /* defined(MBSS) */

/*
 * Conversion between HW and SW BSS indexes.  HW is (currently) based on lower
 * bits of BSSID/MAC address.  SW is based on allocation function.
 * BSS does not need to be up, so caller should check if required.  No error checking.
 * For the reverse map, use WLC_BSSCFG_UCIDX(cfg)
 */
#if defined(MBSS)
#define WLC_BSSCFG_HW2SW_IDX(mbss, uc_idx) ((int)((mbss)->hw2sw_idx[(uc_idx)]))
#else /* !MBSS */
#define WLC_BSSCFG_HW2SW_IDX(mbss, uc_idx) 0
#endif /* MBSS */

/*
 * Under MBSS, a pre-TBTT interrupt is generated.  The driver puts beacons in
 * the ATIM fifo at that time and tells uCode about pending BC/MC packets.
 * The delay is settable thru uCode.  MBSS_PRE_TBTT_DEFAULT_us is the default
 * setting for this value.
 * If the driver experiences significant latency, it must avoid setting up
 * beacons or changing the SHM FID registers.  The "max latency" setting
 * indicates the maximum permissible time between the TBTT interrupt and the
 * DPC to respond to the interrupt before the driver must abort the TBTT
 * beacon operations.
 */
#define MBSS_PRE_TBTT_DEFAULT_us 5000		/* 5 milliseconds! */
#define MBSS_PRE_TBTT_MAX_LATENCY_us 4000

struct wlc_mbss_info_t *BCMATTACHFN(wlc_mbss_attach)(wlc_info_t *wlc);

void BCMATTACHFN(wlc_mbss_detach)(struct wlc_mbss_info_t *mbssinfo);

int wlc_mbss_wlc_up(struct wlc_mbss_info_t *mbssinfo);

void wlc_mbss_reset_macaddrs(struct wlc_mbss_info_t *mbssinfo);

#ifdef WLC_HIGH

int wlc_mbss_validate_mac(struct wlc_mbss_info_t *mbssinfo,
	wlc_bsscfg_t *cfg, struct ether_addr *addr);

int wlc_mbss_shm_ssid_upd(struct wlc_mbss_info_t *mbss,
	wlc_bsscfg_t *cfg, uint16 *base);

void wlc_mbss_dotxstatus(wlc_info_t *wlc, tx_status_t *txs, void *pkt,
	uint16 fc,	wlc_pkttag_t *pkttag, uint supr_status);

void mbss_ucode_set(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

void wlc_mbss_bsscfg_down(struct wlc_mbss_info_t *mbss, wlc_bsscfg_t *cfg);

bool wlc_prq_process(wlc_info_t *wlc, bool bounded);

#endif /* WLC_HIGH */

uint16 wlc_mbss_get_fifostartblk(wlc_info_t *wlc);

void wlc_mbss_update_last_tbtt(wlc_info_t *wlc, uint32 tbttval);

int wlc_mbss_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

int wlc_mbss_bsscfg_enable(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg);

int wlc_mbss_gen_bsscfg_dump(wlc_info_t *wlc, struct bcmstrbuf *b);

int wlc_mbss_per_bsscfg_dump(struct wlc_mbss_info_t *mbss,
	wlc_bsscfg_t *cfg, struct bcmstrbuf *b);

void wlc_mbss_bsscfg_set_prb_ttl_us(struct wlc_mbss_info_t *mbss,
	wlc_bsscfg_t *bsscfg, uint32 prb_ttl_us);

uint16
wlc_mbss_upd_fifo_and_get_start_blk(wlc_info_t *wlc, uint corerev,
	uint startrev, uint16 xmtfifo_sz[][NFIFO]);

bool
wlc_mbss_prq_process(wlc_info_t *wlc, bool bounded);

#endif	/* _wlc_mbss_h_ */
