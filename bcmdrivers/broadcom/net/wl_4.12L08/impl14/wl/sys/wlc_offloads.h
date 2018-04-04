/*
 * Broadcom 802.11 host offload module
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_offloads.h chandrum $
 */

#ifndef _WL_OFFLOADS_H_
#define _WL_OFFLOADS_H_

#ifdef WLOFFLD
/* Whether offload capable or not */
extern bool wlc_ol_cap(wlc_info_t *wlc);

/* Offload module attach */
extern wlc_ol_info_t * wlc_ol_attach(wlc_info_t *wlc);

/* Offload module detach */
extern void wlc_ol_detach(wlc_ol_info_t *ol);
extern int wlc_ol_up(void *hdl);
extern int wlc_ol_down(void *hdl);
extern void wlc_ol_clear(wlc_ol_info_t *ol);
extern void wlc_ol_restart(wlc_ol_info_t *ol);

/* Returns true of the interrupt is from CR4 */
extern bool wlc_ol_intstatus(wlc_ol_info_t *ol);
/* DPC */
extern void wlc_ol_dpc(wlc_ol_info_t *ol);
extern void wlc_ol_enable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg);
extern void wlc_ol_disable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg);

#define OL_CFG_MASK	      0x1	/* For configuration */
#define OL_SCAN_MASK      0x2	/* For SCANNING */
#define OL_UATBTT_MASK    0x4	/* For unaligned TBTT */
void wlc_ol_rx_deferral(wlc_ol_info_t *ol, uint32 mask, uint32 val);

extern bool wlc_ol_time_since_bcn(wlc_ol_info_t *ol);

extern void
wlc_ol_enable_intrs(wlc_ol_info_t *ol, bool enable);
extern bool wlc_ol_chkintstatus(wlc_ol_info_t *ol);
#endif /* WLOFFLD */

#endif /* _WL_OFFLOADS_H_ */
