/*
 * LCNPHY module header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_lcn40.h 365871 2012-10-31 07:02:06Z $*
 */

#ifndef _wlc_phy_lcn40_h_
#define _wlc_phy_lcn40_h_

#include <typedefs.h>
#include <wlc_phy_int.h>
#include <wlc_phy_lcn.h>
#define LCN40PHY_SWCTRL_NVRAM_PARAMS 5
#define LCN40PHY_RXIQCOMP_PARAMS 2
#define LCN40PHY_NOTCHFILTER_COEFFS 10

#define LCN40PHY_RCAL_OFFSET 0x10
#define LCN40PHY_PAPR_NVRAM_PARAMS 20

/* PAPD linearization path selection */
#ifdef WLPHY_IPA_ONLY
#define LCN40_LINPATH(val)	(LCN40PHY_PAPDLIN_IPA)
#else
#define LCN40_LINPATH(val)	(val)
#endif

typedef enum {
	LCN40PHY_PAPDLIN_PAD = 0,
	LCN40PHY_PAPDLIN_EPA,
	LCN40PHY_PAPDLIN_IPA
} lcn40phy_papd_lin_path_t;

struct phy_info_lcn40phy {
	phy_info_lcnphy_t lcnphycommon;
	uint16 rx_iq_comp_5g[LCN40PHY_RXIQCOMP_PARAMS];
	uint8 trGain;
	int16 tx_iir_filter_type_cck;
	int16 tx_iir_filter_type_ofdm;
	int16 tx_iir_filter_type_ofdm40;
	bool phycrs_war_en;
	int16 pwr_offset40mhz_2g;
	int16 pwr_offset40mhz_5g;
	uint8 dac2x_enable;
	uint8 dac2x_enable_nvm;
	uint8 rcal;
	uint16 mixboost_5g;
	uint8 dacpu;
	uint8 elna_off_gain_idx_2g;
	uint8 elna_off_gain_idx_5g;
	uint8 gain_settle_dly_2g;
	uint8 gain_settle_dly_5g;
	bool tia_dc_loop_enable_2g;
	bool hpc_sequencer_enable_2g;
	bool tia_dc_loop_enable_5g;
	bool hpc_sequencer_enable_5g;
	int16 padbias5g;
	int8 aci_detect_en_2g;
	int8 tx_agc_reset_2g;
	int8 gaintbl_force_2g;
	int8 aci_detect_en_5g;
	int8 tx_agc_reset_5g;
	int8 gaintbl_force_5g;
	int iqcalidx5g;
	bool tx_alpf_pu;
	int16 lcnphy_idletssi_corr;
	int16 tempsenseCorr;
	bool epa_or_pad_lpbk;
	bool loflag;
	int8 dlocalidx5g;
	int8 dlorange_lowlimit;
	int32 noise_cal_deltamax;
	int32 noise_cal_deltamin;
	int8 dsss_thresh;
	uint32 startdiq_2g;
	uint32 startdiq_5g;
	bool btc_clamp;
	uint16 rx_iq_comp_2g[LCN40PHY_RXIQCOMP_PARAMS];
	int8 high_temp_threshold;
	uint8 temp_bckoff_2g;
	uint8 temp_bckoff_5g;
	uint8 cond_bckoff;
	int8 low_temp_threshold;
	uint8 temp_boost_2g;
	uint8 temp_boost_5g;
	uint8 cond_boost;
	int8 temp_diff;
	uint16	cck_tssi_idx;
	uint16	init_ccktxpwrindex;
	int lowpower_beacon_mode;
	int8 localoffs5gmh;
	int16 temp_offset_2g;
	int16 temp_offset_5g;
	int16 pretemp;
	int8 temp_cal_en_2g;
	int8 temp_cal_en_5g;
	int8 sample_collect_gainadj;
	int16  ofdm40_dig_filt_type_2g;
	int16  ofdm40_dig_filt_type_5g;
	uint16 save_digi_gain_ovr;
	phy_idletssi_perband_info_t lcn40_idletssi0_cache;
	phy_idletssi_perband_info_t lcn40_idletssi1_cache;
	int8 vlin2g;
	int8 vlin5g;
	int16 edonthreshold40;
	int16 edoffthreshold40;
	int16 edonthreshold20U;
	int16 edonthreshold20L;
	int16 edoffthreshold20UL;
	uint8 ppidletssi_en;
	uint8 ppidletssi_en_2g;
	uint8 ppidletssi_en_5g;
	int16 ppidletssi_corr_2g;
	int16 ppidletssi_corr_5g;
	uint16 tempsense_tx_cnt;
	int16 last_tempsense_avg;
	int8 papden2g;
	int8 papden5g;
	bool papd_enable;
	int16 papdlinpath2g;
	int16 papdlinpath5g;
	int16 papd_lin_path;
	uint16 papd_bbmult_init_bw20;
	uint16 papd_bbmult_init_bw40;
	uint16 papd_bbmult_step_bw20;
	uint16 papd_bbmult_step_bw40;
	uint16 papd_lut_step;
	uint16 papd_lut_begin;
	phy_tx_gain_tbl_entry *txgaintable;
	uint8 paprr_enable2g;
	uint8 paprr_enable5g;
	uint32 paprgamtbl2g[LCN40PHY_PAPR_NVRAM_PARAMS];
	uint32 paprgamtbl5g[LCN40PHY_PAPR_NVRAM_PARAMS];
	uint32 papr40gamtbl5g[LCN40PHY_PAPR_NVRAM_PARAMS];

	int8 paprofftbl2g[LCN40PHY_PAPR_NVRAM_PARAMS];
	int8 paprofftbl5g[LCN40PHY_PAPR_NVRAM_PARAMS];
	int8 papr40offtbl5g[LCN40PHY_PAPR_NVRAM_PARAMS];

	uint8 paprgaintbl2g[LCN40PHY_PAPR_NVRAM_PARAMS];
	uint8 paprgaintbl5g[LCN40PHY_PAPR_NVRAM_PARAMS];
	uint8 papr40gaintbl5g[LCN40PHY_PAPR_NVRAM_PARAMS];
	int16 papdsfac5g;
	int16 papdsfac2g;
	int16 papd_mag_th_2g;
	int16 papd_mag_th_5g;
	int16 max_amam_dB;
	bool rfpll_doubler_2g;
	bool rfpll_doubler_5g;
	uint16 pll_loop_bw_desired_2g;
	uint16 pll_loop_bw_desired_5g;
	uint32 temp_cal_adj_2g;
	uint32 temp_cal_adj_5g;
	uint8 fstr_flag;
	uint8 cdd_mod;
	uint8 rssi_log_nsamps;
	uint8 rssi_iqest_en;
	int8 rssi_iqest_gain_adj;
};

extern void wlc_lcn40phy_set_bbmult(phy_info_t *pi, uint8 m0);
#if defined(WLTEST)
extern void wlc_phy_get_rxgainerr_lcn40phy(phy_info_t *pi, int16 *gainerr);
extern void wlc_phy_get_SROMnoiselvl_lcn40phy(phy_info_t *pi, int8 *noiselvl);
extern void wlc_lcn40phy_rx_power(phy_info_t *pi, uint16 num_samps,
	uint8 wait_time, uint8 wait_for_crs, phy_iq_est_t* est);
extern int16 wlc_lcn40phy_rxgaincal_tempadj(phy_info_t *pi);
extern void wlc_phy_get_noiseoffset_lcn40phy(phy_info_t *pi, int16 *noiseoff);
extern void wlc_lcn40phy_get_lna_freq_correction(phy_info_t *pi, int8 *freq_offset_fact);
#endif
#ifdef SAMPLE_COLLECT
int wlc_phy_sample_collect_lcn40phy(phy_info_t *pi, wl_samplecollect_args_t *collect, uint32 *buf);
#endif
extern void wlc_lcn40phy_aci_init(phy_info_t *pi);
extern void wlc_lcn40phy_aci(phy_info_t *pi, bool on);
extern void wlc_lcn40phy_aci_upd(phy_info_t *pi);
extern int wlc_lcn40phy_idle_tssi_reg_iovar(phy_info_t *pi, int32 int_val, bool set, int *err);
extern int wlc_lcn40phy_avg_tssi_reg_iovar(phy_info_t *pi);
extern int16 wlc_lcn40phy_rssi_tempcorr(phy_info_t *pi, bool mode);
extern uint8 wlc_lcn40phy_max_cachedchans(phy_info_t *pi);
extern int16 wlc_lcn40phy_get_rxpath_gain_by_index(phy_info_t *pi,
	uint8 gain_index, uint16 board_atten);
#endif /* _wlc_phy_lcn40_h_ */
