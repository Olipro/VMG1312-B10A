/*
 * MBSS Feature [PRIVATE] definitions for
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

#ifndef _wlc_mbss_priv_h_
#define _wlc_mbss_priv_h_

#include <wlc_types.h>

#include <wl_dbg.h>

/* txfifo blocks to reduce in case we are not able to meet template requirements for MBSS */
#define WLC_MBSS_BLKS_FOR_TEMPLATE	(12)

#define BSS_BEACONING(cfg) ((cfg) && BSSCFG_AP(cfg) && (cfg)->up)

struct wlc_mbss_info_t {
	wlc_info_t *wlc;
	uint32		max_ap_bss;		/* max ap bss supported by driver */
	struct ether_addr vether_base;		/* Base virtual MAC addr when user
						 * doesn't provide one
						 */
	uint32		last_tbtt_us;		/* Timestamp of TBTT time */
	int			bcast_next_start;	/* For rotating probe responses
						* to bcast requests
						*/
	int8		beacon_bssidx;		/* Track start config to rotate order of beacons */
	uint8		mbss_ucidx_mask;	/* used for extracting ucidx from macaddr */
	uint8		cur_dtim_count;		/* current DTIM count */
	int 		cfgh;			/* bsscfg cubby handle */
	int8		hw2sw_idx[WLC_MAXBSSCFG]; /* Map from uCode index to software index */
#if defined(WLC_HIGH) && defined(WLC_LOW)
	uint16		prq_base;		/* Base address of PRQ in shm */
	uint16		prq_rd_ptr; 	/* Cached read pointer for PRQ */
#endif
};

/* Select SSID length register based on HW index */
#define _MBSS_SSID_LEN_SELECT(wlc, idx) (MBSS_ENAB(wlc->pub) ? \
	(((idx) == 0 || (idx) == 1) ? SHM_MBSS_SSID_LEN0 : SHM_MBSS_SSID_LEN1) : \
	M_SSIDLEN)

/* Use to access a specific SSID length */
#define WLC_MBSS_SSID_LEN_GET(wlc, idx, out_val) do { \
		out_val = wlc_read_shm(wlc, _MBSS_SSID_LEN_SELECT(wlc, idx)); \
		if ((idx) % 2) \
			out_val = ((out_val) >> 8) & 0xff; \
		else \
			out_val = (out_val) & 0xff; \
	} while (0)

/* Internal macro to set SSID length register values properly */
#define _MBSS_SSID_LEN_SET(idx, reg_val, set_val) do { \
		if ((idx) % 2) { \
			(reg_val) &= ~0xff00; \
			(reg_val) |= ((set_val) & 0xff) << 8; \
		} else { \
			(reg_val) &= ~0xff; \
			(reg_val) |= (set_val) & 0xff; \
		} \
	} while (0)

#define WLC_MBSS_BSSCFG_UCIDX(pcfgcubby)	((pcfgcubby)->_ucidx)

/* MBSS debug counters used in bsscfg cubby */
typedef struct wlc_mbss_cnt {
	uint32		prq_directed_entries; /* Directed PRQ entries seen */
	uint32		prb_resp_alloc_fail;  /* Failed allocation on probe response */
	uint32		prb_resp_tx_fail;     /* Failed to TX probe response */
	uint32		prb_resp_retrx_fail;  /* Failed to TX probe response */
	uint32		prb_resp_retrx;       /* Retransmit suppressed probe resp */
	uint32		prb_resp_ttl_expy;    /* Probe responses suppressed due to TTL expry */
	uint32		bcn_tx_failed;	      /* Failed to TX beacon frame */

	uint32		mc_fifo_max;	/* Max number of BC/MC pkts in DMA queues */
	uint32		bcmc_count;	/* Total number of pkts sent thru BC/MC fifo */

	/* Timing and other measurements for PS transitions */
	uint32		ps_trans;	/* Total number of transitions started */
} wlc_mbss_cnt_t;

typedef struct {
	wlc_pkt_t	probe_template; /* Probe response master packet, including PLCP */
#ifdef WLLPRS
	wlc_pkt_t	lprs_template;	/* Legacy probe response master packet */
	prb_ie_info_t	prb_ieinfo; /* information of certain ies of interest */
#endif /* WLLPRS */
	bool		prb_modified;	/* Ucode version: push to shm if true */
	wlc_spt_t	*bcn_template;	/* Beacon DMA template */
	int8		_ucidx; 	/* the uCode index of this bsscfg,
					 * assigned at wlc_bsscfg_up()
					 */
	uint32		mc_fifo_pkts;	/* Current number of BC/MC pkts sent to DMA queues */
	uint32		prb_ttl_us; 	/* Probe rsp time to live since req. If 0, disabled */
#ifdef WLCNT
	wlc_mbss_cnt_t *cnt;		/* MBSS debug counters */
#endif
#if defined(BCMDBG_MBSS_PROFILE)
	uint32		ps_start_us;	/* When last PS (off) transition started */
	uint32		max_ps_off_us;	/* Max delay time for out-of-PS transitions */
	uint32		tot_ps_off_us;	/* Total time delay for out-of-PS transitions */
	uint32		ps_off_count;	/* Number of deferred out-of-PS transitions completed */
	bool		bcn_tx_done;	/* TX done on sw beacon */
#endif /* BCMDBG_MBSS_PROFILE */
} wlc_mbss_bsscfg_cubby_t;

/* to retreive cubby */
#define MBSS_BSSCFG_CUBBY(mbssinfo, cfg) \
	((wlc_mbss_bsscfg_cubby_t *)BSSCFG_CUBBY(cfg, (mbssinfo)->cfgh))
#endif	/* _wlc_mbss_priv_h_ */
