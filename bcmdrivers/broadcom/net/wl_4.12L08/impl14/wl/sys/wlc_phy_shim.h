/*
 * Interface layer between WL driver and PHY driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_shim.h 363142 2012-10-16 08:12:36Z $
 */

#ifndef _wlc_phy_shim_h_
#define _wlc_phy_shim_h_

#include <wlc_ppr.h>

#define RADAR_TYPE_NONE		0	/* Radar type None */
#define RADAR_TYPE_ETSI_1	1	/* ETSI 1 Radar type */
#define RADAR_TYPE_ETSI_2	2	/* ETSI 2 Radar type */
#define RADAR_TYPE_ETSI_3	3	/* ETSI 3 Radar type */
#define RADAR_TYPE_ETSI_4	4	/* ETSI Radar type */
#define RADAR_TYPE_STG2 	5	/* staggered-2 radar */
#define RADAR_TYPE_STG3 	6	/* staggered-3 radar */
#define RADAR_TYPE_UNCLASSIFIED	7	/* Unclassified Radar type  */
#define RADAR_TYPE_FCC_5	8	/* long pulse radar type */
#define RADAR_TYPE_JP1_2_JP2_3	9	/* JAPAN 1_2, 2_3 radar */
#define RADAR_TYPE_JP2_1	10	/* JAPAN 2_1 radar */
#define RADAR_TYPE_JP4		11	/* JAPAN 4 radar */
#define RADAR_TYPE_FCC_1	12	/* FCC 1 radar */
#define ANTSEL_NA		0	/* No boardlevel selection available */
#define ANTSEL_2x4		1	/* 2x4 boardlevel selection available */
#define ANTSEL_2x3		2	/* 2x3 SW boardlevel selection available */
#define ANTSEL_1x2_CORE1	3	/* 1x2 Core 1 based SW boardlevel selection available */
#define ANTSEL_2x3_HWRX		4	/* 2x3 SWTX + HWRX boardlevel selection available */
#define ANTSEL_1x2_HWRX		5	/* 1x2 SWTX + HWRX boardlevel selection available */
#define ANTSEL_1x2_CORE0	6	/* 1x2 Core 0 based SW boardlevel selection available */

/* Rx Antenna diversity control values */
#define	ANT_RX_DIV_FORCE_0		0	/* Use antenna 0 */
#define	ANT_RX_DIV_FORCE_1		1	/* Use antenna 1 */
#define	ANT_RX_DIV_START_1		2	/* Choose starting with 1 */
#define	ANT_RX_DIV_START_0		3	/* Choose starting with 0 */
#define	ANT_RX_DIV_ENABLE		3	/* APHY bbConfig Enable RX Diversity */
#define ANT_RX_DIV_DEF		ANT_RX_DIV_START_0	/* default antdiv setting */

/* Forward declarations */
struct wlc_hw_info;
typedef struct wlc_phy_shim_info wlc_phy_shim_info_t;

extern uint8 wlapi_bmac_time_since_bcn_get(wlc_phy_shim_info_t *physhim);
extern wlc_phy_shim_info_t *wlc_phy_shim_attach(struct wlc_hw_info *wlc_hw, void *wl, void *wlc);
extern void wlc_phy_shim_detach(wlc_phy_shim_info_t *physhim);

/* PHY to WL utility functions */
struct wlapi_timer;
extern struct wlapi_timer *wlapi_init_timer(wlc_phy_shim_info_t *physhim, void (*fn)(void* arg),
	void *arg, const char *name);
extern void wlapi_free_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t);
extern void wlapi_add_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t, uint ms,
	int periodic);
extern bool wlapi_del_timer(wlc_phy_shim_info_t *physhim, struct wlapi_timer *t);
extern void wlapi_intrson(wlc_phy_shim_info_t *physhim);
extern uint32 wlapi_intrsoff(wlc_phy_shim_info_t *physhim);
extern void wlapi_intrsrestore(wlc_phy_shim_info_t *physhim, uint32 macintmask);

extern void wlapi_bmac_write_shm(wlc_phy_shim_info_t *physhim, uint offset, uint16 v);
extern uint16 wlapi_bmac_read_shm(wlc_phy_shim_info_t *physhim, uint offset);
extern void wlapi_bmac_mhf(wlc_phy_shim_info_t *physhim, uint8 idx, uint16 mask, uint16 val,
	int bands);
extern void wlapi_bmac_corereset(wlc_phy_shim_info_t *physhim, uint32 flags);
extern void wlapi_suspend_mac_and_wait(wlc_phy_shim_info_t *physhim);
extern void wlapi_switch_macfreq(wlc_phy_shim_info_t *physhim, uint8 spurmode);
extern void wlapi_enable_mac(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_mctrl(wlc_phy_shim_info_t *physhim, uint32 mask, uint32 val);
extern void wlapi_bmac_phy_reset(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_bw_set(wlc_phy_shim_info_t *physhim, uint16 bw);
extern void wlapi_bmac_phyclk_fgc(wlc_phy_shim_info_t *physhim, bool clk);
extern void wlapi_bmac_macphyclk_set(wlc_phy_shim_info_t *physhim, bool clk);
extern void wlapi_bmac_core_phypll_ctl(wlc_phy_shim_info_t *physhim, bool on);
extern void wlapi_bmac_core_phypll_reset(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_ucode_wake_override_phyreg_set(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_ucode_wake_override_phyreg_clear(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_write_template_ram(wlc_phy_shim_info_t *physhim, int o, int len, void *buf);
extern uint16 wlapi_bmac_rate_shm_offset(wlc_phy_shim_info_t *physhim, uint8 rate);
extern void wlapi_ucode_sample_init(wlc_phy_shim_info_t *physhim);
extern void wlapi_copyfrom_objmem(wlc_phy_shim_info_t *physhim, uint, void* buf, int, uint32 sel);
extern void wlapi_copyto_objmem(wlc_phy_shim_info_t *physhim, uint, const void* buf, int, uint32);

extern void wlapi_high_update_phy_mode(wlc_phy_shim_info_t *physhim, uint32 phy_mode);
extern void wlapi_noise_cb(wlc_phy_shim_info_t *physhim, uint8 channel, int8 noise_dbm);
extern uint16 wlapi_bmac_get_txant(wlc_phy_shim_info_t *physhim);
extern int wlapi_bmac_btc_mode_get(wlc_phy_shim_info_t *physhim);
extern void wlapi_bmac_btc_period_get(wlc_phy_shim_info_t *physhim, uint16 *btperiod,
	bool *btactive);
#ifdef PPR_API
extern void wlapi_high_update_txppr_offset(wlc_phy_shim_info_t *physhim, ppr_t *txpwr);
#else
extern void wlapi_high_update_txppr_offset(wlc_phy_shim_info_t *physhim, int8 *txpwr);
#endif
#endif	/* _wlc_phy_shim_h_ */
