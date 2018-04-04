/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THIS IS A GENERATED FILE - DO NOT EDIT
 * Generated on Thu Jul  5 08:24:00 PDT 2007
 *
 * Copyright(c) 2007 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id: wlc_phytbl_ssn.h 245911 2011-03-11 19:48:22Z $
 */
/* FILE-CSTYLED */
#ifndef _wlc_phytbl_ssn_h_
#define _wlc_phytbl_ssn_h_

typedef phytbl_info_t dot11sslpnphytbl_info_t;


extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev0[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev2[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev2_shared[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphytbl_info_rev3[];
extern CONST uint32 dot11sslpnphytbl_info_sz_rev0;
extern CONST uint32 dot11sslpnphytbl_info_sz_rev2;
extern CONST uint32 dot11sslpnphytbl_info_sz_rev3;
extern CONST uint32 dot11sslpnphytbl_aci_sz;
extern CONST uint32 dot11sslpnphytbl_no_aci_sz;
extern CONST uint32 dot11sslpnphytbl_cmrxaci_sz;
extern CONST uint32 dot11sslpnphytbl_cmrxaci_rev2_sz;
extern CONST uint32 dot11sslpnphytbl_extlna_cmrxaci_sz;
extern CONST uint32 dot11lpphy_rx_gain_init_tbls_40Mhz_sz;
extern CONST uint32 dot11lpphy_rx_gain_init_tbls_40Mhz_sz_rev3;
extern CONST uint32 dot11lpphy_rx_gain_extlna_tbls_A_sz;
extern CONST uint32 dot11lpphy_rx_gain_extlna_tbls_A_40Mhz_sz;
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_aci[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_no_aci[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_cmrxaci[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_cmrxaci_rev2[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_extlna_cmrxaci[];
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_40Mhz_smic;
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_info_olympic_x7;
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_init_tbls_A;
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_rev1_5Ghz;
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_cmrxaciWOT;
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_init_tbls_40Mhz[];
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_init_tbls_40Mhz_rev3[];
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_extlna_tbls_A[];
extern CONST dot11sslpnphytbl_info_t dot11lpphy_rx_gain_extlna_tbls_A_40Mhz[];
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_info_4319_usbb;
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_info_4319_sdio;
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_info_ninja6l;
extern CONST uint16 sw_ctrl_tbl_sdna[];
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_info_arcadyn;
extern CONST dot11sslpnphytbl_info_t sw_ctrl_tbl_LG_TY;
extern CONST uint16 sw_ctrl_tbl_rev02_shared[];
extern CONST uint16 sw_ctrl_tbl_rev02_shared_mlap[];
extern CONST uint16 sw_ctrl_tbl_rev02_shared_mlap_5g[];
extern CONST uint16 sw_ctrl_tbl_rev02_shared_mlap_emu3[];
extern CONST uint16 sw_ctrl_tbl_rev02_shared_mlap_emu3_5g[];
extern CONST uint16 sw_ctrl_tbl_rev02_shared_mlap_windsor_5g[];
extern CONST uint16 sw_ctrl_tbl_rev02_shared_mlap_combiner[];
extern CONST uint16 sw_ctrl_tbl_rev02_shared_mlap_emu3_combiner[];
extern CONST uint32 fltr_ctrl_tbl_40Mhz[];


typedef struct {
	uchar gm;
	uchar pga;
	uchar pad;
	uchar dac;
	uchar bb_mult;
} sslpnphy_tx_gain_tbl_entry;

extern CONST sslpnphy_tx_gain_tbl_entry dot11sslpnphy_2GHz_gaintable_rev0[];

extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable[];
extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_MidBand[];
extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_HiBand[];
extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_X17_ePA[];

extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_4319_midband[];
extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_4319_hiband[];

extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_4319_midband_gmboost[];
extern CONST sslpnphy_tx_gain_tbl_entry dot11lpphy_5GHz_gaintable_4319_hiband_gmboost[];

extern CONST sslpnphy_tx_gain_tbl_entry dot11sslpnphy_noPA_gaintable_rev0[];

extern CONST uint32 sslpnphy_papd_cal_ofdm_tbl[25][64];
extern uint16 sslpnphy_rev2_real_ofdm[5];
extern uint16 sslpnphy_rev4_real_ofdm[5];
extern uint16 sslpnphy_rev2_real_cck[5];
extern uint16 sslpnphy_rev4_real_cck[5];
extern uint16 sslpnphy_phybw40_real_ht[5];
extern uint16 sslpnphy_rev2_cx_ht[10];
extern uint16 sslpnphy_rev2_default[10];
extern uint16 sslpnphy_rev0_cx_cck[10];
extern uint16 sslpnphy_rev0_cx_ofdm[10];
extern uint16 sslpnphy_rev4_cx_ofdm[10];
extern uint16 sslpnphy_cx_cck[13][10];
extern uint16 sslpnphy_real_cck[13][5];
extern uint16 sslpnphy_rev4_phybw40_real_ofdm[5];
extern uint16 sslpnphy_rev4_phybw40_cx_ofdm[10];
extern uint16 sslpnphy_rev2_phybw40_cx_ofdm[10];
extern uint16 sslpnphy_rev4_phybw40_cx_ht[10];
extern uint16 sslpnphy_rev2_phybw40_cx_ht[10];
extern uint16 sslpnphy_rev4_phybw40_real_ht[5];
extern uint16 sslpnphy_rev2_phybw40_real_ht[5];
extern uint16 sslpnphy_cx_ofdm[2][10];
extern uint16 sslpnphy_real_ofdm[2][5];
extern uint16 sslpnphy_rev1_cx_ofdm_fcc[10];
extern uint16 sslpnphy_rev1_cx_ofdm[10];
extern uint16 sslpnphy_rev1_real_ht[5];
extern uint16 sslpnphy_rev1_real_ht_fcc[5];
extern uint16 sslpnphy_rev1_real_ofdm[5];
extern uint16 sslpnphy_rev1_real_ofdm_fcc[5];
extern uint16 sslpnphy_tdk_mdl_real_ofdm[5];
extern uint16 sslpnphy_olympic_cx_ofdm[10];
extern uint16 sslpnphy_rev1_cx_ofdm_sec[10];
extern uint16 sslpnphy_rev1_real_ofdm_sec[5];
extern uint16 sslpnphy_rev1_real_ht_sec[5];

extern CONST uint32 sslpnphy_gain_idx_extlna_cmrxaci_tbl[];
extern CONST uint16 sslpnphy_gain_extlna_cmrxaci_tbl[];
extern CONST uint32 sslpnphy_gain_idx_extlna_2g_x17[];
extern CONST uint16 sslpnphy_gain_tbl_extlna_2g_x17[];
extern CONST uint16 sw_ctrl_tbl_rev0_olympic_x17_2g[];
extern CONST uint32 dot11lpphy_rx_gain_init_tbls_A_tbl[];
extern CONST uint16 gain_tbl_rev0[];
extern CONST uint32 gain_idx_tbl_rev0[];
extern CONST uint16 sw_ctrl_tbl_rev0[];
extern CONST uint32 sslpnphy_gain_idx_extlna_5g_x17[];
extern CONST uint16 sslpnphy_gain_tbl_extlna_5g_x17[];
extern CONST uint16 sw_ctrl_tbl_rev0_olympic_x17_5g[];
extern CONST uint16 sw_ctrl_tbl_rev1_5Ghz_tbl[];


extern uint16 sslpnphy_gain_idx_extlna_cmrxaci_tbl_sz;
extern uint16 sslpnphy_gain_extlna_cmrxaci_tbl_sz;
extern uint16 sslpnphy_gain_idx_extlna_2g_x17_sz;
extern uint16 sslpnphy_gain_tbl_extlna_2g_x17_sz;
extern uint16 sw_ctrl_tbl_rev0_olympic_x17_2g_sz;
extern uint16 dot11lpphy_rx_gain_init_tbls_A_tbl_sz;
extern uint16 gain_tbl_rev0_sz;
extern uint16 gain_idx_tbl_rev0_sz;
extern uint16 sw_ctrl_tbl_rev0_sz;
extern uint16 sslpnphy_gain_idx_extlna_5g_x17_sz;
extern uint16 sslpnphy_gain_tbl_extlna_5g_x17_sz;
extern uint16 sw_ctrl_tbl_rev0_olympic_x17_5g_sz;
extern uint16 sw_ctrl_tbl_rev1_5Ghz_tbl_sz;

#endif /* _wlc_phytbl_ssn_h_ */
