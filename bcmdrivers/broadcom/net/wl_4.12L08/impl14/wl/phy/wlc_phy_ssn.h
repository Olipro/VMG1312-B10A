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
 * $Id: wlc_phy_ssn.h 316408 2012-02-22 03:11:49Z $
 */

#ifndef _wlc_phy_ssn_h_
#define _wlc_phy_ssn_h_

#include <typedefs.h>
#include <wlc_phytbl_ssn.h>
#define SSLPNPHY_NOISE_PWR_FIFO_DEPTH 6
#define SSLPNPHY_INIT_NOISE_CAL_TMOUT 38000 /* In uS */
#define SSLPNPHY_NPWR_MINLMT 1
#define SSLPNPHY_NPWR_MAXLMT_2G 50
#define SSLPNPHY_NPWR_MAXLMT_5G 200
#define SSLPNPHY_NPWR_LGC_MINLMT_20MHZ 7
#define SSLPNPHY_NPWR_LGC_MAXLMT_20MHZ 21
#define SSLPNPHY_NPWR_LGC_MINLMT_40MHZ 4
#define SSLPNPHY_NPWR_LGC_MAXLMT_40MHZ 14
#define SSLPNPHY_NOISE_MEASURE_WINDOW_2G 1800 /* In uS */
#define SSLPNPHY_NOISE_MEASURE_WINDOW_5G 1400 /* In uS */
#define SSLPNPHY_MAX_GAIN_CHANGE_LMT_2G 9
#define SSLPNPHY_MAX_GAIN_CHANGE_LMT_5G 15
#define SSLPNPHY_MAX_RXPO_CHANGE_LMT_2G 12
#define SSLPNPHY_MAX_RXPO_CHANGE_LMT_5G 18


typedef struct {
	uint8 bb_mult_old;
	uint16 Core1TxControl_old, sslpnCtrl3_old;
	uint16 pa_sp1_old_5_4, rxbb_ctrl2_old;
	uint16 rf_common_03_old, rf_grx_sp_1_old;
	uint16 SSLPNPHY_sslpnCalibClkEnCtrl_old;
	uint16 lpfbwlut0, lpfbwlut1, rf_txbb_sp_3, rf_pa_ctrl_14;
	uint8 CurTxGain;
} sslpnphy_store_t;

typedef struct {
	int on_thresh; /* number of glitches */
	int on_timeout; /* in seconds */
	int off_thresh; /* number of glitches */
	int off_timeout; /* in seconds */
	int glitch_cnt;
	int ts;
	int gain_backoff;
} sslpnphy_aci_t;


typedef struct {
	/* TX IQ LO cal results */
	uint16 txiqlocal_bestcoeffs[11];
	uint txiqlocal_bestcoeffs_valid;
	/* PAPD results */
	uint32 papd_compdelta_tbl[128];
	uint papd_table_valid;
	uint16 analog_gain_ref, lut_begin,
		lut_end, lut_step, rxcompdbm, papdctrl;

	/* RX IQ cal results */
	uint16 rxiqcal_coeffa0;
	uint16 rxiqcal_coeffb0;
	uint16 rxiq_enable;
	uint8 rxfe;
	uint8 loopback2, loopback1;
} sslpnphy_cal_results_t;

struct phy_info_sslpnphy {
/* SSLPNPHY */
	uint16	sslpnphy_txiqlocal_bestcoeffs[11]; /* best coefficients */
	bool	sslpnphy_txiqlocal_bestcoeffs_valid;
	uint16 sslpnphy_bphyctrl;

	uint8 sslpnphy_full_cal_channel[2];
	uint16 sslpnphy_full_cal_chanspec[2];
	uint8	sslpnphy_rc_cap;

	uint8	sslpnphy_tr_isolation_mid;	/* TR switch isolation for each sub-band */
	uint8	sslpnphy_tr_isolation_low;
	uint8	sslpnphy_tr_isolation_hi;

	uint8	sslpnphy_bx_arch;		/* Board switch architecture */

	uint8	sslpnphy_rx_power_offset;	 /* Input power offset */
	uint16  sslpnphy_rxpo2gchnflg;
	uint8   sslpnphy_fabid; /* 0 is default ,1 is CSM, 2 is TSMC etc */
	uint16  sslpnphy_fabid_otp;

#ifdef BAND5G
	uint8	sslpnphy_rx_power_offset_5g;
	uint8	sslpnphy_rssi_5g_vf;	/* 5G RSSI Vmid fine */
	uint8	sslpnphy_rssi_5g_vc; /* 5G RSSI Vmid coarse */
	uint8	sslpnphy_rssi_5g_gs; /* 5G RSSI gain select */
#endif
	uint8	sslpnphy_rssi_vf;		/* RSSI Vmid fine */
	uint8	sslpnphy_rssi_vc;		/* RSSI Vmid coarse */
	uint8	sslpnphy_rssi_gs;		/* RSSI gain select */
	uint8	sslpnphy_tssi_val; /* tssi value */
	uint8 sslpnphy_auxadc_val; /* aux adc  value */
	uint8	sslpnphy_rssi_vf_lowtemp;	/* RSSI Vmid fine */
	uint8	sslpnphy_rssi_vc_lowtemp; /* RSSI Vmid coarse */
	uint8	sslpnphy_rssi_gs_lowtemp; /* RSSI gain select */

	uint8	sslpnphy_rssi_vf_hightemp;	/* RSSI Vmid fine */
	uint8	sslpnphy_rssi_vc_hightemp;	/* RSSI Vmid coarse */
	uint8	sslpnphy_rssi_gs_hightemp;	/* RSSI gain select */

	uint16	sslpnphy_tssi_tx_cnt; /* Tx frames at that level for NPT calculations */
	uint16	sslpnphy_tssi_idx;	/* Estimated index for target power */
	uint16	sslpnphy_tssi_npt;	/* NPT for TSSI averaging */

	uint16	sslpnphy_target_tx_freq;	/* Target freq for which LUT's were calculated */
	uint16 sslpnphy_last_tx_freq;
	int8	sslpnphy_tx_power_idx_override; /* Forced tx power index */
	uint16	sslpnphy_noise_samples;

	uint32	sslpnphy_papdRxGnIdx;
	uint32	sslpnphy_papd_rxGnCtrl_init;

	uint32	sslpnphy_gain_idx_14_lowword;
	uint32	sslpnphy_gain_idx_14_hiword;
	uint32	sslpnphy_gain_idx_27_lowword;
	uint32	sslpnphy_gain_idx_27_hiword;
	int16	sslpnphy_ofdmgainidxtableoffset;  /* reference ofdm gain index table offset */
	int16	sslpnphy_dsssgainidxtableoffset;  /* reference dsss gain index table offset */
	uint32	sslpnphy_tr_R_gain_val;  /* reference value of gain_val_tbl at index 64 */
	uint32	sslpnphy_tr_T_gain_val;	/* reference value of gain_val_tbl at index 65 */
	int8	sslpnphy_input_pwr_offset_db;
	uint16	sslpnphy_Med_Low_Gain_db;
	uint16	sslpnphy_Very_Low_Gain_db;
	int8	sslpnphy_lastsensed_temperature;
	int8	sslpnphy_pkteng_rssi_slope;
#ifndef PPR_API
	uint8	sslpnphy_saved_tx_user_target[TXP_NUM_RATES];
#endif
	uint8	sslpnphy_volt_winner;
	uint8	sslpnphy_volt_low;
	uint8	sslpnphy_54_48_36_24mbps_backoff;
	uint8	sslpnphy_11n_backoff;
	uint8	sslpnphy_lowerofdm;
	uint8	sslpnphy_cck;
	uint8	sslpnphy_psat_2pt3_detected;
	int32	sslpnphy_lowest_Re_div_Im;
	int32	sslpnphy_max_gain;
	int32	sslpnphy_lowest_gain_diff;
	int8 	sslpnphy_final_papd_cal_idx;
	uint16	sslpnphy_extstxctrl4;
	uint16  sslpnphy_extstxctrl5;
	uint16	sslpnphy_extstxctrl0;
	uint16	sslpnphy_extstxctrl1;
	uint16  sslpnphy_extstxctrl3;
	uint16  sslpnphy_extstxctrl2;

	uint32  sslpnphy_mcs20_po;
	uint32  sslpnphy_mcs40_po;
	uint16 sslpnphy_filt_bw;
	uint16 sslpnphy_ofdm_filt_bw;
	uint32 *sslpnphy_gain_idx_2g;
	uint32 *sslpnphy_gain_idx_5g;
	uint16 *sslpnphy_gain_tbl_2g;
	uint16 *sslpnphy_gain_tbl_5g;
	uint16 *sslpnphy_swctrl_lut_2g;
	uint16 *sslpnphy_swctrl_lut_5g;

	/* Periodic cal flags */
	int8 sslpnphy_last_cal_temperature;
	int8 sslpnphy_last_full_cal_temperature;
	int32 sslpnphy_last_cal_voltage;
	uint sslpnphy_force_1_idxcal;
	int sslpnphy_papd_nxt_cal_idx;
	uint sslpnphy_vbat_ripple;
	uint sslpnphy_recal;
	uint8 sslpnphy_last_cal_tia_gain;
	uint8 sslpnphy_last_cal_lna2_gain;
	uint8 sslpnphy_last_cal_vga_gain;
	int8 sslpnphy_last_cal_tx_idx;
	uint sslpnphy_percal_ctr;
	uint8 sslpnphy_tx_idx_prev_cal;
	uint sslpnphy_volt_track;
	uint sslpnphy_txidx_drift;
	uint sslpnphy_cur_idx;
	uint sslpnphy_force_percal;
	uint32 *sslpnphy_papdIntlut;
	uint8 *sslpnphy_papdIntlutVld;

	uint8 sslpnphy_cck_filt_sel;
	uint8 sslpnphy_ofdm_filt_sel;
	bool sslpnphy_OLYMPIC;
	bool sslpnphy_vbat_ripple_check;

	/* used for debug */
	int32	sslpnphy_voltage_sensed;
	uint8 	sslpnphy_psat_pwr;
	uint8 	sslpnphy_psat_indx;
	int32	sslpnphy_min_phase;
	uint8	sslpnphy_final_idx;
	uint8	sslpnphy_start_idx;
	uint8	sslpnphy_current_index;
	uint16	sslpnphy_logen_buf_1;
	uint16	sslpnphy_local_ovr_2;
	uint16	sslpnphy_local_oval_6;
	uint16	sslpnphy_local_oval_5;
	uint16	sslpnphy_logen_mixer_1;
	uint8   sslpnphy_dummy_tx_done;

	uint8	sslpnphy_aci_stat;
	uint	sslpnphy_aci_start_time;
#ifdef PPR_API
	uint32  sslpnphy_rate_table[WL_RATESET_SZ_DSSS + WL_RATESET_SZ_OFDM + WL_RATESET_SZ_HT_MCS];
#else
	int8	sslpnphy_tx_power_offset[TXP_NUM_RATES];		/* Offset from base power */
#endif
	bool	sslpnphy_papd_cal_done;

	sslpnphy_aci_t		sslpnphy_aci;
	sslpnphy_store_t	sslpnphy_store;
	sslpnphy_cal_results_t	sslpnphy_cal_results[2];
	/* uCode Dynamic Noise Cal Flags */
	uint8   Listen_GaindB_BfrNoiseCal;
	uint8   Listen_GaindB_BASE;
	uint8   Listen_GaindB_AfrNoiseCal;
	uint8   Listen_RF_Gain;
	uint16  NfSubtractVal_BfrNoiseCal;
	uint16  NfSubtractVal_BASE;
	uint16  NfSubtractVal_AfrNoiseCal;
	int8    RxpowerOffset_Required_BASE;
	int8    rxpo_required_AfrNoiseCal;
	uint8   Latest_Listen_GaindB_Latch;

	int8    EdOn_Thresh20_BASE;
	int8    EdOn_Thresh40_BASE;
	int8    EdOn_Thresh20L_BASE;
	int8    EdOn_Thresh20U_BASE;
	int8    EdOn_Thresh20_Latch;
	int8    EdOn_Thresh40_Latch;
	int8    EdOn_Thresh20L_Latch;
	int8    EdOn_Thresh20U_Latch;

	/* Will be TRUE After FirstTime NoiseCal Happens in a Channel */
	bool    sslpnphy_init_noise_cal_done;
	uint32 sslpnphy_noisepwr_fifo_Min[SSLPNPHY_NOISE_PWR_FIFO_DEPTH];
	uint32 sslpnphy_noisepwr_fifo_Max[SSLPNPHY_NOISE_PWR_FIFO_DEPTH];
	uint8  sslpnphy_noisepwr_fifo_filled;
	uint8  sslpnphy_NPwr_MinLmt;
	uint8  sslpnphy_NPwr_MaxLmt;
	uint8  sslpnphy_NPwr_LGC_MinLmt;
	uint8  sslpnphy_NPwr_LGC_MaxLmt;
	bool   sslpnphy_disable_noise_percal;
	uint sslpnphy_last_noise_cal;
	uint sslpnphy_last_idletssi_cal;
	uint16 sslpnphy_noise_measure_window;
	uint8  sslpnphy_max_listen_gain_change_lmt;
	uint8  sslpnphy_max_rxpo_change_lmt;
	uint8	sslpnphy_tssi_pwr_limit;
	uint sslpnphy_restore_papd_cal_results;
	sslpnphy_tx_gain_tbl_entry * sslpnphy_tx_gaintbl_5GHz_midband;
	sslpnphy_tx_gain_tbl_entry * sslpnphy_tx_gaintbl_5GHz_hiband;
	uint8 sslpnphy_tssi_idx_ch[LAST_5G_CHAN + 1];
	int8 sslpnphy_tssi_max_pwr_limit;
	int8 sslpnphy_tssi_min_pwr_limit;
#ifdef DONGLEOVERLAYS
	/* needed to sequentially init/cal on both bands */
	uint8 phyinit_state;
#endif
	uint8 fem_combiner_target_state;
	uint8 fem_combiner_current_state;
	int8 samp_collect_agc_gain;
	int8 sslpnphy_tssi_min_pwr_nvram;
	int8 sslpnphy_tssi_max_pwr_nvram;
	int8 sslpnphy_noise_cal_offset;
	bool   sslpnphy_force_noise_cal;
	int8   sslpnphy_force_lgain;
	int8   sslpnphy_force_rxpo;
	int32 sslpnphy_spbdump_pwr_dBm;

	uint16 sslpnphy_end_location;
	uint16 sslpnphy_start_location;
	uint8 sslpnphy_ant1_max_pwr;
	int8 sslpnphy_cga_5g[24];
	int8 sslpnphy_cga_2g[14];

	bool sslpnphy_radio_classA;
	bool sslpnphy_papd_tweaks_enable;
	struct {
		uint32	final_idx_thresh;
		uint16	papd_track_pa_lut_begin;
		uint16	papd_track_pa_lut_step;
		uint16  min_final_idx_thresh;
	} sslpnphy_papd_tweaks;
	bool pkteng_in_progress;
	/* For wl noise iovar */
	uint32  IQ_Avg_Pwr;
	int16   noise_level_dBm;
	int     noisedBm_ma;
	uint16  noisedBm_ma_count;
	uint16  noisedBm_ma_win_sz;
	int     noisedBm_window[MA_WINDOW_SZ];
	int     noisedBm_index;
	int     noisedBm_ma_avg;
    bool  peak_current_mode;
	struct {
		bool  cal_at_init_done;
		uint8 cal_tgtpwr_idx;
		int   cal_tgtpwr;
		uint8 tgtPapdCalpwr_idx;
		int tgtPapdCalpwr;
		uint8 predict_cal_count;
	} sslpnphy_papd_peakcurmode;
};
#endif /* _wlc_phy_ssn_h_ */
