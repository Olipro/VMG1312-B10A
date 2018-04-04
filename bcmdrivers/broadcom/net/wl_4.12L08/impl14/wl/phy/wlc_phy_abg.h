/*
 * ABGPHY module header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_abg.h 254389 2011-04-20 13:43:16Z $
 */

#ifndef _wlc_phy_abg_h_
#define _wlc_phy_abg_h_

#include <typedefs.h>

typedef struct {
	uint16  rc_cal_ovr;
	uint16  phycrsth1;
	uint16  phycrsth2;
	uint16  init_n1p1_gain;
	uint16  p1_p2_gain;
	uint16  n1_n2_gain;
	uint16  n1_p1_gain;
	uint16  div_search_gain;
	uint16  div_p1_p2_gain;
	uint16  div_search_gn_change;
	uint16  table_7_2;
	uint16  table_7_3;
	uint16  cckshbits_gnref;
	uint16  clip_thresh;
	uint16  clip2_thresh;
	uint16  clip3_thresh;
	uint16  clip_p2_thresh;
	uint16  clip_pwdn_thresh;
	uint16  clip_n1p1_thresh;
	uint16  clip_n1_pwdn_thresh;
	uint16  bbconfig;
	uint16  cthr_sthr_shdin;
	uint16  energy;
	uint16  clip_p1_p2_thresh;
	uint16  threshold;
	uint16  reg15;
	uint16  reg16;
	uint16  reg17;
	uint16  div_srch_idx;
	uint16  div_srch_p1_p2;
	uint16  div_srch_gn_back;
	uint16  ant_dwell;
	uint16  ant_wr_settle;
} aci_save_gphy_t;

typedef struct _lo_complex_t {
	int8 i;
	int8 q;
} abgphy_lo_complex_info_t;

typedef struct {
	uint    abgphy_cal_nslope;              /* time of last nrssislope calibration */
	uint    abgphy_cal_noffset;             /* time of last nrssioffset calibration */
	uint    abgphy_cal_mlo;         /* last time measurelow calibration was done */
	/* nrssi calibration: */
	int             nrssi_slope_scale;
	int             min_rssi;
	int             max_rssi;
} abgphy_calibration_t;

struct phy_info_abgphy {
bool            sbtml_gm;
uint16          radiopwr;               /* radio power */
uint16          mintxbias;
abgphy_lo_complex_info_t gphy_locomp_iq[STATIC_NUM_RF][STATIC_NUM_BB];

/* nrssi calibration: */
	uint16          rc_cal;
	int             nrssi_table_delta;
	int             nrssi_slope_offset;
#if defined(BCMDBG) || defined(WLTEST)
	/* Long training */
	int     long_train_phytest;
	uint16  ltrn_phyregc3;
	uint16  ltrn_phyreg29;
	uint16  ltrn_reg04;
	uint16  ltrn_phyreg10;
	uint16  ltrn_phyreg11;

	uint16  freq_anacore;
	uint16  freq_test;
	uint16  freq_rf_override;
	uint16  freq_freq_ctrl;
	uint16  freq_phase_scale;
	uint16  freq_ddfs_enable;
	uint16  freq_pwr;
#endif 
	uint16          mintxmag;
	int8            stats_11b_txpower[STATIC_NUM_RF][STATIC_NUM_BB]; /* settings in use */
	uint16          gain_table[TX_GAIN_TABLE_LENGTH];
	bool            loopback_gain;  /* Flag indicating that loopback gain is computed */
	int16           max_lpback_gain_hdB;
	int16           trsw_rx_gain_hdB;
	uint8           power_vec[8];   /* Store power indexes used by ucode during hwpwrctrl */

uint16          bb_atten;               /* baseband attenuation */
uint16          txctl1;                 /* iff 2050 */

/* measlo for 11b */
	int             rf_list_size;
	int             bb_list_size;
	uint16          *rf_attn_list;
	uint16          *bb_attn_list;
	uint16          padmix_mask;
	uint16          padmix_reg;
	uint16          *txmag_list;
	uint            txmag_len;
	bool            txmag_enable;

	bool            txpwridx_override_aphy;         /* override power index on A-phy */

	/* tssi to dbm translation table */
	int8            *a_tssi_to_dbm; /* a-band rev 1 srom */
	int8            *m_tssi_to_dbm; /* mid band rev 2 srom */
	int8            *l_tssi_to_dbm; /* low band rev 2 srom */
	int8            *h_tssi_to_dbm; /* high band rev 2 srom */
	int8            target_idle_tssi;               /* target idle tssi value */
	bool            bf_preempt_4306;        /* True to enable 'darwin' mode */
	/* 11a LOFT */
	uint16          tx_vos;
	uint16          global_tx_bb_dc_bias_loft;

	/* measlo for 11b */
	int             rf_max;
	int             bb_max;
	abgphy_calibration_t abgphy_cal;
	aci_save_gphy_t interference_save;

	int8		idle_tssi[CH_5G_GROUP];		/* Measured idle_tssi for lo,mid,hi */
	bool		channel_14_wide_filter;		/* used in abg phy code */
};
#endif /* _wlc_phy_abg_h_ */
