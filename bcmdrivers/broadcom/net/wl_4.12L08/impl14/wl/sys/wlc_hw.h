/*
 * H/W info API of
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

#ifndef _wlc_hw_h_
#define _wlc_hw_h_

typedef struct wlc_hw wlc_hw_t;
typedef struct wlc_hw_info wlc_hw_info_t;

#include <hnddma.h>
#include <wlc_pio.h>

#define WLC_HW_DI(wlc, fifo) ((wlc)->hw_pub->di[fifo])
#define WLC_HW_PIO(wlc, fifo) ((wlc)->hw_pub->pio[fifo])

/* Data APIs */
/* Code outside the HW module (wlc_bmac, wlc_high_stubs, wlc_intr, wlc_diag,
 * wlc_bmac_stubs?, etc.) shall not modify nor read these data directly, and
 * they shall use APIs or MACROs in case these data are moved to inside the
 * module later when performance is determined to be a non-issue.
 *
 * These data shall be read via the following MACROs:
 * - WLC_HW_DI()
 * - WLC_HW_PIO()
 * These data shall be modified via the following APIs:
 * - wlc_hw_set_di()
 * - wlc_hw_set_pio()
 */
struct wlc_hw {
	hnddma_t	*di[NFIFO];		/* hnddma handles, per fifo */
	pio_t		*pio[NFIFO];		/* pio handlers, per fifo */
};

/*
 * Detect Card removed.
 * Even checking an sbconfig register read will not false trigger when the core is in reset.
 * it breaks CF address mechanism. Accessing gphy phyversion will cause SB error if aphy
 * is in reset on 4306B0-DB. Need a simple accessible reg with fixed 0/1 pattern
 * (some platforms return all 0).
 * If clocks are present, call the sb routine which will figure out if the device is removed.
 */
#if defined(WLC_HIGH_ONLY)
#define DEVICEREMOVED(wlc)	(!wlc->device_present)
#else
#define DEVICEREMOVED(wlc)      wlc_hw_deviceremoved((wlc)->hw)
#endif 

/* Function APIs */
extern wlc_hw_info_t *wlc_hw_attach(wlc_info_t *wlc, osl_t *osh, uint unit, uint *err);
extern void wlc_hw_detach(wlc_hw_info_t *wlc_hw);

extern void wlc_hw_set_piomode(wlc_hw_info_t *wlc_hw, bool piomode);
extern bool wlc_hw_get_piomode(wlc_hw_info_t *wlc_hw);

extern void wlc_hw_set_di(wlc_hw_info_t *wlc_hw, uint fifo, hnddma_t *di);
extern void wlc_hw_set_pio(wlc_hw_info_t *wlc_hw, uint fifo, pio_t *pio);

#ifdef WLC_LOW
extern bool wlc_hw_deviceremoved(wlc_hw_info_t *wlc_hw);
extern uint32 wlc_hw_get_wake_override(wlc_hw_info_t *wlc_hw);
extern uint wlc_hw_get_bandunit(wlc_hw_info_t *wlc_hw);
extern void wlc_hw_get_txavail(wlc_hw_info_t *wlc_hw, uint *txavail[]);
extern uint32 wlc_hw_get_macintmask(wlc_hw_info_t *wlc_hw);
extern uint32 wlc_hw_get_macintstatus(wlc_hw_info_t *wlc_hw);
#endif

#ifdef WLC_HIGH
/* MHF2_SKIP_ADJTSF ucode host flag manipulation - global user ID */
#define WLC_SKIP_ADJTSF_SCAN		0
#define WLC_SKIP_ADJTSF_RM		1
#define WLC_SKIP_ADJTSF_USER_MAX	4
extern void wlc_skip_adjtsf(wlc_info_t *wlc, bool skip, wlc_bsscfg_t *cfg, uint32 user, int bands);
/* MCTL_AP maccontrol register bit manipulation - global user ID */
#define WLC_AP_MUTE_SCAN	0
#define WLC_AP_MUTE_RM		1
#define WLC_AP_MUTE_USER_MAX	4
/* mux s/w MCTL_AP on/off request */
#ifdef AP
extern void wlc_ap_mute(wlc_info_t *wlc, bool mute, wlc_bsscfg_t *cfg, uint32 user);
#else
#define wlc_ap_mute(wlc, mute, cfg, user) do {} while (0)
#endif
/* force MCTL_AP off; mux s/w MCTL_AP on request */
extern void wlc_ap_ctrl(wlc_info_t *wlc, bool on, wlc_bsscfg_t *cfg, uint32 user);
#endif /* WLC_HIGH */

extern void wlc_template_cfg_init(wlc_info_t *wlc, uint corerev);

#endif /* !_wlc_hw_h_ */
