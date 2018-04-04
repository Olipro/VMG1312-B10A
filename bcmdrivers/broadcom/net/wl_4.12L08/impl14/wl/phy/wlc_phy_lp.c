/*
 * PHY and RADIO specific portion of Broadcom BCM43XX 802.11abgn
 * Networking Device Driver.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_lp.c 345233 2012-07-17 06:12:10Z $
 */


#include <wlc_cfg.h>
#include <typedefs.h>
#include <qmath.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wlc_phy_radio.h>
#include <bitfuncs.h>
#include <bcmdevs.h>
#include <bcmnvram.h>
#include <proto/802.11.h>
#include <hndpmu.h>
#include <bcmsrom_fmt.h>
#include <sbsprom.h>

#include <wlc_phy_hal.h>
#include <wlc_phy_int.h>

#include <wlc_phyreg_lp.h>
#include <wlc_phyreg_ssn.h>
#include <wlc_phytbl_lp.h>
#include <wlc_phytbl_ssn.h>
#include <wlc_phy_lp.h>

#define wlc_radio_2063_rc_cal_done(pi) (0 != (read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_6) & 0x02))

/* %%%%%% LPPHY macros/structure */
#define LPPHY_ACI_CHANNEL_SKIP 4
#if (ACI_CHANNEL_DELTA <= LPPHY_ACI_CHANNEL_SKIP)
#error "ACI_CHANNEL_DELTA should be > LPPHY_ACI_CHANNEL_SKIP !"
#endif

#define LPPHY_ACI_X (ACI_CHANNEL_DELTA - LPPHY_ACI_CHANNEL_SKIP)
#define LPPHY_ACI_Y (ACI_CHANNEL_DELTA + 1 + LPPHY_ACI_CHANNEL_SKIP)
#define LPPHY_ACI_S_M_L ((1<<LPPHY_ACI_X)-1)
#define LPPHY_ACI_S_M (LPPHY_ACI_S_M_L | (LPPHY_ACI_S_M_L << LPPHY_ACI_Y))
#define LPPHY_ACI_C_M ((1<<(ACI_LAST_CHAN - ACI_FIRST_CHAN + 1))-1)

#define LPPHY_ACI_CHAN_SCAN_MASK (LPPHY_ACI_S_M << ACI_FIRST_CHAN)
#define LPPHY_ACI_CHAN_MASK (LPPHY_ACI_C_M <<ACI_FIRST_CHAN)
#define LPPHY_ACI_CHAN_CENTER (ACI_CHANNEL_DELTA + ACI_FIRST_CHAN)

/* To print dbg statistics on aci channel scan */
#define LPPHY_ACI_CHAN_SCAN_DBG_BINS 10
#define LPPHY_ACI_CHAN_SCAN_DBG_SHFT 3
#define LPPHY_ACI_CHAN_SCAN_MAX_PWR ((1<<LPPHY_ACI_CHAN_SCAN_DBG_SHFT) * \
				(LPPHY_ACI_CHAN_SCAN_DBG_BINS-1))

#undef	LPPHY_ACI_SCAN_CHK_TIME

#define LPPHY_ACI_R_A_M 0xffff
#define LPPHY_ACI_R_A_S 16
#define LPPHY_ACI_R_S_M 0xf
#define LPPHY_ACI_R_S_S 12
#define LPPHY_ACI_R_N_M 0xf
#define LPPHY_ACI_R_N_S 8
#define LPPHY_ACI_R_V_M 0xff
#define LPPHY_ACI_R_V_S 0
#define LPPHY_ACI_DEF_REG(a, f, n, v) \
	((LPPHY_##a & LPPHY_ACI_R_A_M) << LPPHY_ACI_R_A_S) |\
	((LPPHY_##a##_##f##_SHIFT  & LPPHY_ACI_R_S_M) << LPPHY_ACI_R_S_S) |\
	((n & LPPHY_ACI_R_N_M) << LPPHY_ACI_R_N_S) |\
	((v & LPPHY_ACI_R_V_M) << LPPHY_ACI_R_V_S)
#define LPPHY_ACI_NULL 0xffffffff
#define LPPHY_ACI_REG_SZ_CHK(n) \
	typedef  struct { \
	int n[LPPHY_ACI_MAX_REGS - (sizeof(n)/sizeof(uint32))]; \
	} n##_chk

#define LPPHY_NOISE_SAMPLES_DEFAULT	5000

/* LPPHY table id */
#define LPPHY_TBL_ID_IQLOCAL		0
#define LPPHY_TBL_ID_SAMPLEPLAY		5
#define LPPHY_TBL_ID_TXPWRCTL (LPREV_GE(pi->pubpi.phy_rev, 2) ? 0x07 : 0x0A)
#define LPPHY_TBL_ID_RFSEQ (LPREV_GE(pi->pubpi.phy_rev, 2) ? 0x08 : 0x0B)
/* #define LPPHY_TBL_ID_PAPD_EPS	0x09 move to wlc_phy_int.h */
#define LPPHY_TBL_ID_PAPD_MULT	0x0A
#define LPPHY_TBL_ID_GAIN_VAL_TBL_REV2 17
#define LPPHY_TBL_ID_GAIN_VAL_TBL_REV3 17

#define LPPHY_PAPD_EPS_TBL_SIZE 64
#define LPPHY_REV2_NUM_DIG_FILT_COEFFS 9

#define wlc_lpphy_tx_gain_override_enabled(pi) \
	(0 != (phy_reg_read((pi), LPPHY_AfeCtrlOvr) & LPPHY_AfeCtrlOvr_dacattctrl_ovr_MASK))

#define wlc_lpphy_enable_tx_gain_override(pi)	wlc_lpphy_set_tx_gain_override(pi, TRUE)
#define wlc_lpphy_disable_tx_gain_override(pi)	wlc_lpphy_set_tx_gain_override(pi, FALSE)


#define wlc_lpphy_total_tx_frames(pi) \
	wlapi_bmac_read_shm((pi)->sh->physhim, M_UCODE_MACSTAT + OFFSETOF(macstat_t, txallfrm))

#define LPPHY_TX_PWR_CTRL_RATE_OFFSET 	64
#define LPPHY_TX_PWR_CTRL_MAC_OFFSET 	128
#define LPPHY_TX_PWR_CTRL_GAIN_OFFSET 	192
#define LPPHY_TX_PWR_CTRL_IQ_OFFSET		320
#define LPPHY_TX_PWR_CTRL_LO_OFFSET		448
#define LPPHY_TX_PWR_CTRL_PWR_OFFSET		576

#define LPPHY_TX_PWR_CTRL_START_INDEX_2G_HGPA	64
#ifdef HPWRCALWAR
#define LPPHY_TX_PWR_CTRL_START_INDEX_2G	LPPHY_TX_PWR_CTRL_START_INDEX_2G_HGPA
#else
#define LPPHY_TX_PWR_CTRL_START_INDEX_2G	32
#endif
#define LPPHY_TX_PWR_CTRL_START_INDEX_5G	64

#define LPPHY_TX_PWR_CTRL_START_NPT		0
#define LPPHY_TX_PWR_CTRL_MAX_NPT			7

#define LPPHY_REV2_txgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(LPPHY_REV2_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT + 8)
#define LPPHY_REV2_txgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << LPPHY_REV2_txgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define LPPHY_REV2_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(LPPHY_REV2_stxtxgainctrlovrval1_stxtxgainctrl_ovr_val1_SHIFT + 8)
#define LPPHY_REV2_stxtxgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << LPPHY_REV2_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT)
#define LPPHY_IQLOCC_READ(val) ((uint8)(-(int8)(((val) & 0xf0) >> 4) + (int8)((val) & 0x0f)))

#define LPPHY_PAPD_RECAL_GAIN_DELTA 4
#define LPPHY_PAPD_RECAL_MIN_INTERVAL 30			/* seconds */
#define LPPHY_PAPD_RECAL_MAX_INTERVAL 120			/* seconds */
#define LPPHY_PAPD_RECAL_ENABLE 1

#define LPPHY_MAX_CAL_CACHE		2	/* Max number of cal cache contexts reqd */

#define PLL_2063_LOW_END_VCO 	3000
#define PLL_2063_LOW_END_KVCO 	27
#define PLL_2063_HIGH_END_VCO	4200
#define PLL_2063_HIGH_END_KVCO	68
#define PLL_2063_LOOP_BW			300
#define PLL_2063_D30				3000
#define PLL_2063_CAL_REF_TO		8
#define PLL_2063_MHZ				1000000
#define PLL_2063_OPEN_LOOP_DELAY	5

typedef struct {
	bool valid;
	int8 rx_pwr_dB;
} lpphy_rx_iq_cal_data_for_temp_adj_t;

typedef struct {
	uint32 iq_prod;
	uint32 i_pwr;
	uint32 q_pwr;
} lpphy_iq_est_t;

typedef struct {
	uint16 gm_gain;
	uint16 pga_gain;
	uint16 pad_gain;
	uint16 dac_gain;
} lpphy_txgains_t;

typedef struct {
	lpphy_txgains_t gains;
	bool useindex;
	uint8 index;
} lpphy_txcalgains_t;

typedef struct {
	uint16 tx_pwr_ctrl;
	int8 gain_index;
} lpphy_tx_pwr_state;

uint32 lpphy_rev2_gaincode_table[] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 256, 2048, 2304, 4096, 4352,
	416, 672, 928, 1184, 1440, 938, 1194, 1450,
	1706, 1023, 1279, 1535, 1791, 3583, 3839, 5631,
	5887, 13823, 14079, 79615
};

uint8 lpphy_rev2_gain_table[] = {
	19, 19, 19, 19, 19, 19, 19, 19,
	19, 19, 19, 22, 25, 28, 31, 34,
	35, 38, 41, 44, 47, 53, 56, 59,
	61, 64, 67, 70, 72, 76, 78, 82,
	84, 88, 90, 93
};

#define LPPHY_REV2_NUM_TX_DIG_FILTERS_OFDM 2
uint16 LPPHY_REV2_txdigfiltcoeffs_ofdm[LPPHY_REV2_NUM_TX_DIG_FILTERS_OFDM][9] = {
	{
	0xec4d,
	0xe831,
	0xe330,
	0x3018,
	0x0018,
	0xfa20,
	0x0020,
	0xc340,
	0x0040,
	},
	{
	0xde5e,
	0xe832,
	0xe331,
	0x4d26,
	0x0026,
	0x1420,
	0x0020,
	0xfe08,
	0x0008,
	}
	};

#define LPPHY_REV2_NUM_TX_DIG_FILTERS_CCK 6
/* cck digital filter coeffs in order of improving evm and decreasing SM margin */
uint16 LPPHY_REV2_txdigfiltcoeffs_cck[LPPHY_REV2_NUM_TX_DIG_FILTERS_CCK][9] = {
	{
	0xd75a,
	0xec2f,
	0xee30,
	0xaa54,
	0x0054,
	0xf01a,
	0x001a,
	0x1610,
	0x0010,
	},
	{
	0xd758,
	0xec2f,
	0xee30,
	0xbd5e,
	0x005e,
	0xfb1a,
	0x001a,
	0x0f08,
	0x0008,
	},
	{
	0xd74b,
	0xec2e,
	0xee30,
	0x342a,
	0x002a,
	0x3b20,
	0x0020,
	0xf610,
	0x0010,
	},
	{
	0xe645,
	0xf328,
	0xf426,
	0x1e0a,
	0x0016,
	0x6020,
	0x0048,
	0x1808,
	0x0012,
	},
	{
	0xe63d,
	0xf327,
	0xf426,
	0x2e0f,
	0x0022,
	0x6020,
	0x0048,
	0x1808,
	0x0012,
	},
	/* gauss23, evm and sm fit between filter 1 and 2 */
	{
	0xd751,
	0xec2f,
	0xee30,
	0x231c,
	0x001c,
	0x3b20,
	0x0020,
	0xf610,
	0x0010,
	}
};


uint16 LPPHY_REV2_txdigfiltcoeffs_cck_chan14[] = {
	0xcc4c,
	0xec29,
	0xee2f,
	0x6a35,
	0x0035,
	0x331a,
	0x001a,
	0x150b,
	0x000b,
	};

/* LPPHY IQCAL parameters for various Tx gain settings */
/* table format: */
/*	target, gm, pga, pad, ncorr for each of 5 cal types */
typedef uint16 iqcal_gain_params_lpphy[9];

static const iqcal_gain_params_lpphy tbl_iqcal_gainparams_lpphy_2G[] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	};

static const iqcal_gain_params_lpphy tbl_iqcal_gainparams_lpphy_5G[] = {
	{0x7ef, 7, 0xe, 0xe, 0, 0, 0, 0, 0},
	};

static const iqcal_gain_params_lpphy *tbl_iqcal_gainparams_lpphy[2] = {
	tbl_iqcal_gainparams_lpphy_2G,
	tbl_iqcal_gainparams_lpphy_5G
	};

static const uint16 iqcal_gainparams_numgains[2] = {
	sizeof(tbl_iqcal_gainparams_lpphy_2G) / sizeof(*tbl_iqcal_gainparams_lpphy_2G),
	sizeof(tbl_iqcal_gainparams_lpphy_5G) / sizeof(*tbl_iqcal_gainparams_lpphy_5G),
	};

/* LO Comp Gain ladder. Format: {m genv} */
static const
uint16 lpphy_iqcal_loft_gainladder[]  = {
	((2 << 8) | 0),
	((3 << 8) | 0),
	((4 << 8) | 0),
	((6 << 8) | 0),
	((8 << 8) | 0),
	((11 << 8) | 0),
	((16 << 8) | 0),
	((16 << 8) | 1),
	((16 << 8) | 2),
	((16 << 8) | 3),
	((16 << 8) | 4),
	((16 << 8) | 5),
	((16 << 8) | 6),
	((16 << 8) | 7),
	((23 << 8) | 7),
	((32 << 8) | 7),
	((45 << 8) | 7),
	((64 << 8) | 7),
	((91 << 8) | 7),
	((128 << 8) | 7)
};

/* Image Rejection Gain ladder. Format: {m genv} */
static const
uint16 lpphy_iqcal_ir_gainladder[] = {
	((1 << 8) | 0),
	((2 << 8) | 0),
	((4 << 8) | 0),
	((6 << 8) | 0),
	((8 << 8) | 0),
	((11 << 8) | 0),
	((16 << 8) | 0),
	((23 << 8) | 0),
	((32 << 8) | 0),
	((45 << 8) | 0),
	((64 << 8) | 0),
	((64 << 8) | 1),
	((64 << 8) | 2),
	((64 << 8) | 3),
	((64 << 8) | 4),
	((64 << 8) | 5),
	((64 << 8) | 6),
	((64 << 8) | 7),
	((91 << 8) | 7),
	((128 << 8) | 7)
};

#define wlc_lpphy_get_genv(pi)	\
	(read_radio_reg((pi), RADIO_2062_IQCAL_CTRL0_NORTH) & 0x7)

#define wlc_lpphy_set_genv(pi, genv)	\
	mod_radio_reg((pi), RADIO_2062_IQCAL_CTRL0_NORTH, 0x77, ((genv) << 4) | (genv))

#define wlc_lpphy_iqcal_active(pi)	\
	(phy_reg_read((pi), LPPHY_iqloCalCmd) & \
	(LPPHY_iqloCalCmd_iqloCalCmd_MASK | LPPHY_iqloCalCmd_iqloCalDFTCmd_MASK))

#define wlc_lpphy_get_tx_pwr_ctrl(pi) \
	(phy_reg_read((pi), LPPHY_TxPwrCtrlCmd) & \
			(LPPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK | \
			LPPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK))

#define wlc_lpphy_tssi_enabled(pi) \
	(LPPHY_TX_PWR_CTRL_OFF != wlc_lpphy_get_tx_pwr_ctrl((pi)))

#define wlc_lpphy_set_tx_pwr_npt(pi, npt) \
	phy_reg_mod(pi, LPPHY_TxPwrCtrlNnum, \
		LPPHY_TxPwrCtrlNnum_Npt_intg_log2_MASK, \
		(uint16)(npt) << LPPHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)

#define wlc_lpphy_get_tx_pwr_npt(pi) \
	((phy_reg_read(pi, LPPHY_TxPwrCtrlNnum) & \
		LPPHY_TxPwrCtrlNnum_Npt_intg_log2_MASK) >> \
		LPPHY_TxPwrCtrlNnum_Npt_intg_log2_SHIFT)

#define wlc_lpphy_set_start_tx_pwr_idx(pi, idx) \
	phy_reg_mod(pi, LPPHY_TxPwrCtrlCmd, \
		LPPHY_TxPwrCtrlCmd_pwrIndex_init_MASK, \
		(uint16)(idx) << LPPHY_TxPwrCtrlCmd_pwrIndex_init_SHIFT)

#define wlc_lpphy_set_target_tx_pwr(pi, target) \
	phy_reg_mod(pi, LPPHY_TxPwrCtrlTargetPwr, \
		LPPHY_TxPwrCtrlTargetPwr_targetPwr0_MASK, \
		(uint16)(target) << LPPHY_TxPwrCtrlTargetPwr_targetPwr0_SHIFT)

#define wlc_lpphy_get_target_tx_pwr(pi) \
	((phy_reg_read(pi, LPPHY_TxPwrCtrlTargetPwr) & \
		LPPHY_TxPwrCtrlTargetPwr_targetPwr0_MASK) >> \
		LPPHY_TxPwrCtrlTargetPwr_targetPwr0_SHIFT)

/* channel info type for 2062 radio */
typedef struct _chan_info_2062 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */
	uint8 RF_lgena_tune0;
	uint8 RF_lgena_tune2;
	uint8 RF_lgena_tune3;
	uint8 RF_tx_tune;
	uint8 RF_lgeng_ctrl01;
	uint8 RF_lgena_ctrl5;
	uint8 RF_lgena_ctrl6;
	uint8 RF_tx_pga;
	uint8 RF_tx_pad;
} chan_info_2062_t;

/* channel info type for 2063 radio */
typedef struct _chan_info_2063 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */
	uint8 RF_logen_vcobuf_1;
	uint8 RF_logen_mixer_2;
	uint8 RF_logen_buf_2;
	uint8 RF_logen_rccr_1;
	uint8 RF_grx_1st_3;
	uint8 RF_grx_2nd_2;
	uint8 RF_arx_1st_3;
	uint8 RF_arx_2nd_1;
	uint8 RF_arx_2nd_4;
	uint8 RF_arx_2nd_7;
	uint8 RF_arx_ps_6;
	uint8 RF_txrf_ctrl_2;
	uint8 RF_txrf_ctrl_5;
	uint8 RF_pa_ctrl_11;
	uint8 RF_arx_mix_4;
	uint8 RF_wrf_slna_RX_2G_1st_VT_STG1;
	uint8 RF_txrf_sp_9;
	uint8 RF_txrf_sp_6;
} chan_info_2063_t;

/* Autogenerated by 2063_chantbl_rev0_tcl2c.tcl */
static chan_info_2063_t chan_info_2063_rev0[] = {
	{   1, 2412, 0x6F, 0x3C, 0x3C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{   2, 2417, 0x6F, 0x3C, 0x3C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{   3, 2422, 0x6F, 0x3C, 0x3C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{   4, 2427, 0x6F, 0x2C, 0x2C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{   5, 2432, 0x6F, 0x2C, 0x2C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{   6, 2437, 0x6F, 0x2C, 0x2C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{   7, 2442, 0x6F, 0x2C, 0x2C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{   8, 2447, 0x6F, 0x2C, 0x2C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{   9, 2452, 0x6F, 0x1C, 0x1C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{  10, 2457, 0x6F, 0x1C, 0x1C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{  11, 2462, 0x6E, 0x1C, 0x1C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{  12, 2467, 0x6E, 0x1C, 0x1C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{  13, 2472, 0x6E, 0x1C, 0x1C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{  14, 2484, 0x6E, 0x0C, 0x0C, 0x04, 0x05, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x00, 0x00, 0x00},
	{  34, 5170, 0x6A, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x08,
	0x0F, 0x08, 0x77, 0x80, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  38, 5190, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0x80, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  42, 5210, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0E, 0x07,
	0x0E, 0x07, 0x77, 0x70, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  46, 5230, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0D, 0x06,
	0x0D, 0x06, 0x77, 0x60, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  36, 5180, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0x80, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  40, 5200, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0x70, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  44, 5220, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0E, 0x06,
	0x0E, 0x06, 0x77, 0x60, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  48, 5240, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0D, 0x05,
	0x0D, 0x05, 0x77, 0x60, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  52, 5260, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0C, 0x04,
	0x0C, 0x04, 0x77, 0x60, 0x20, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  56, 5280, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0B, 0x03,
	0x0B, 0x03, 0x77, 0x50, 0x10, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  60, 5300, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0A, 0x02,
	0x0A, 0x02, 0x77, 0x50, 0x10, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{  64, 5320, 0x67, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x09, 0x01,
	0x09, 0x01, 0x77, 0x50, 0x10, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 100, 5500, 0x64, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x20, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 104, 5520, 0x64, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x20, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 108, 5540, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 112, 5560, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 116, 5580, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x10, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 120, 5600, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 124, 5620, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 128, 5640, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 132, 5660, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 136, 5680, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 140, 5700, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 149, 5745, 0x61, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 153, 5765, 0x61, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 157, 5785, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 161, 5805, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 165, 5825, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 184, 4920, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xC0, 0x50, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 188, 4940, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xB0, 0x50, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 192, 4960, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xB0, 0x50, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 196, 4980, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xA0, 0x40, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 200, 5000, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xA0, 0x40, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 204, 5020, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0E,
	0x0F, 0x0E, 0x77, 0xA0, 0x40, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 208, 5040, 0x6C, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0D,
	0x0F, 0x0D, 0x77, 0x90, 0x40, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 212, 5060, 0x6C, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0C,
	0x0F, 0x0C, 0x77, 0x90, 0x40, 0x00, 0xF3, 0x00, 0x00, 0x00},
	{ 216, 5080, 0x6B, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0B,
	0x0F, 0x0B, 0x77, 0x90, 0x40, 0x00, 0xF3, 0x00, 0x00, 0x00}
};

/* Autogenerated by 2063_chantbl_rev1_csm_tcl2c.tcl */
static chan_info_2063_t chan_info_2063_rev1[] = {
	{   1, 2412, 0x6F, 0x3C, 0x3C, 0x04, 0x0E, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x0C, 0xF0, 0x20},
	{   2, 2417, 0x6F, 0x3C, 0x3C, 0x04, 0x0E, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x0B, 0xF0, 0x20},
	{   3, 2422, 0x6F, 0x3C, 0x3C, 0x04, 0x0E, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x09, 0xF0, 0x20},
	{   4, 2427, 0x6F, 0x2C, 0x2C, 0x04, 0x0D, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x08, 0xF0, 0x20},
	{   5, 2432, 0x6F, 0x2C, 0x2C, 0x04, 0x0D, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x07, 0xF0, 0x20},
	{   6, 2437, 0x6F, 0x2C, 0x2C, 0x04, 0x0C, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x06, 0xF0, 0x20},
	{   7, 2442, 0x6F, 0x2C, 0x2C, 0x04, 0x0B, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x05, 0xF0, 0x20},
	{   8, 2447, 0x6F, 0x2C, 0x2C, 0x04, 0x0B, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x04, 0xF0, 0x20},
	{   9, 2452, 0x6F, 0x1C, 0x1C, 0x04, 0x0A, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x04, 0xF0, 0x20},
	{  10, 2457, 0x6F, 0x1C, 0x1C, 0x04, 0x09, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x03, 0xF0, 0x20},
	{  11, 2462, 0x6E, 0x1C, 0x1C, 0x04, 0x08, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x03, 0xF0, 0x20},
	{  12, 2467, 0x6E, 0x1C, 0x1C, 0x04, 0x07, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x02, 0xF0, 0x20},
	{  13, 2472, 0x6E, 0x1C, 0x1C, 0x04, 0x06, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x02, 0xF0, 0x20},
	{  14, 2484, 0x6E, 0x0C, 0x0C, 0x04, 0x02, 0x55, 0x05, 0x05,
	0x05, 0x05, 0x77, 0x80, 0x80, 0x70, 0xF3, 0x01, 0xF0, 0x20},
	{  34, 5170, 0x6A, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x08,
	0x0F, 0x08, 0x77, 0xCF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  38, 5190, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xCF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  42, 5210, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0E, 0x07,
	0x0E, 0x07, 0x77, 0xBF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  46, 5230, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0D, 0x06,
	0x0D, 0x06, 0x77, 0xBF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  36, 5180, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xCF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  40, 5200, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xBF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  44, 5220, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0E, 0x06,
	0x0E, 0x06, 0x77, 0xBF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  48, 5240, 0x69, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0D, 0x05,
	0x0D, 0x05, 0x77, 0xBF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  52, 5260, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0C, 0x04,
	0x0C, 0x04, 0x77, 0xBF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  56, 5280, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0B, 0x03,
	0x0B, 0x03, 0x77, 0xAF, 0x70, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  60, 5300, 0x68, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0A, 0x02,
	0x0A, 0x02, 0x77, 0xAF, 0x60, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{  64, 5320, 0x67, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x09, 0x01,
	0x09, 0x01, 0x77, 0xAF, 0x60, 0x10, 0xF3, 0x00, 0x00, 0x00},
	{ 100, 5500, 0x64, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x8F, 0x50, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 104, 5520, 0x64, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x7F, 0x50, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 108, 5540, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x7F, 0x50, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 112, 5560, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x7F, 0x50, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 116, 5580, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x7F, 0x50, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 120, 5600, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x7F, 0x50, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 124, 5620, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x6F, 0x40, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 128, 5640, 0x63, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x6F, 0x40, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 132, 5660, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x6F, 0x40, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 136, 5680, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x6F, 0x40, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 140, 5700, 0x62, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x5F, 0x40, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 149, 5745, 0x61, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x5F, 0x40, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 153, 5765, 0x61, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x4F, 0x30, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 157, 5785, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x4F, 0x30, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 161, 5805, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x4F, 0x30, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 165, 5825, 0x60, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0x3F, 0x30, 0x00, 0x03, 0x00, 0x00, 0x00},
	{ 184, 4920, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xFF, 0x70, 0x30, 0xF3, 0x00, 0x00, 0x00},
	{ 188, 4940, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xFF, 0x70, 0x30, 0xF3, 0x00, 0x00, 0x00},
	{ 192, 4960, 0x6E, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xEF, 0x70, 0x30, 0xF3, 0x00, 0x00, 0x00},
	{ 196, 4980, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xEF, 0x70, 0x20, 0xF3, 0x00, 0x00, 0x00},
	{ 200, 5000, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0F,
	0x0F, 0x0F, 0x77, 0xEF, 0x70, 0x20, 0xF3, 0x00, 0x00, 0x00},
	{ 204, 5020, 0x6D, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0E,
	0x0F, 0x0E, 0x77, 0xEF, 0x70, 0x20, 0xF3, 0x00, 0x00, 0x00},
	{ 208, 5040, 0x6C, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0D,
	0x0F, 0x0D, 0x77, 0xDF, 0x70, 0x20, 0xF3, 0x00, 0x00, 0x00},
	{ 212, 5060, 0x6C, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0C,
	0x0F, 0x0C, 0x77, 0xDF, 0x70, 0x20, 0xF3, 0x00, 0x00, 0x00},
	{ 216, 5080, 0x6B, 0x0C, 0x0C, 0x00, 0x05, 0x55, 0x0F, 0x0B,
	0x0F, 0x0B, 0x77, 0xDF, 0x70, 0x20, 0xF3, 0x00, 0x00, 0x00}
};

/* Autogenerated by 2062_chantbl_tcl2c.tcl */
static chan_info_2062_t chan_info_2062[] = {
	{   1, 2412, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{   2, 2417, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{   3, 2422, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{   4, 2427, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{   5, 2432, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{   6, 2437, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{   7, 2442, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{   8, 2447, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{   9, 2452, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{  10, 2457, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{  11, 2462, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{  12, 2467, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{  13, 2472, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{  14, 2484, 0xFF, 0xFF, 0xB5, 0x1B, 0x24, 0x32, 0x32, 0x88, 0x88 },
	{  34, 5170, 0x00, 0x22, 0x20, 0x84, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  38, 5190, 0x00, 0x11, 0x10, 0x83, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  42, 5210, 0x00, 0x11, 0x10, 0x83, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  46, 5230, 0x00, 0x00, 0x00, 0x83, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  36, 5180, 0x00, 0x11, 0x20, 0x83, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  40, 5200, 0x00, 0x11, 0x10, 0x84, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  44, 5220, 0x00, 0x11, 0x00, 0x83, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  48, 5240, 0x00, 0x00, 0x00, 0x83, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  52, 5260, 0x00, 0x00, 0x00, 0x83, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  56, 5280, 0x00, 0x00, 0x00, 0x83, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  60, 5300, 0x00, 0x00, 0x00, 0x63, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{  64, 5320, 0x00, 0x00, 0x00, 0x62, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{ 100, 5500, 0x00, 0x00, 0x00, 0x30, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 104, 5520, 0x00, 0x00, 0x00, 0x20, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 108, 5540, 0x00, 0x00, 0x00, 0x20, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 112, 5560, 0x00, 0x00, 0x00, 0x20, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 116, 5580, 0x00, 0x00, 0x00, 0x10, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 120, 5600, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 124, 5620, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 128, 5640, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 132, 5660, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 136, 5680, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 140, 5700, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 149, 5745, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 153, 5765, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 157, 5785, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 161, 5805, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 165, 5825, 0x00, 0x00, 0x00, 0x00, 0x3C, 0x77, 0x37, 0xFF, 0x88 },
	{ 184, 4920, 0x55, 0x77, 0x90, 0xF7, 0x3C, 0x77, 0x35, 0xFF, 0xFF },
	{ 188, 4940, 0x44, 0x77, 0x80, 0xE7, 0x3C, 0x77, 0x35, 0xFF, 0xFF },
	{ 192, 4960, 0x44, 0x66, 0x80, 0xE7, 0x3C, 0x77, 0x35, 0xFF, 0xFF },
	{ 196, 4980, 0x33, 0x66, 0x70, 0xC7, 0x3C, 0x77, 0x35, 0xFF, 0xFF },
	{ 200, 5000, 0x22, 0x55, 0x60, 0xD7, 0x3C, 0x77, 0x35, 0xFF, 0xFF },
	{ 204, 5020, 0x22, 0x55, 0x60, 0xC7, 0x3C, 0x77, 0x35, 0xFF, 0xFF },
	{ 208, 5040, 0x22, 0x44, 0x50, 0xC7, 0x3C, 0x77, 0x35, 0xFF, 0xFF },
	{ 212, 5060, 0x11, 0x44, 0x50, 0xA5, 0x3C, 0x77, 0x35, 0xFF, 0x88 },
	{ 216, 5080, 0x00, 0x44, 0x40, 0xB6, 0x3C, 0x77, 0x35, 0xFF, 0x88 }
};

/* Init values for 2062 regs (autogenerated by 2062_regs_tcl2c.tcl)
 *   Entries: addr, init value A, init value G, do_init A, do_init G.
 *   Last line (addr FF is dummy as delimiter. This table has dual use
 *   between dumping and initializing.
 */
radio_regs_t regs_2062[] = {
	{ 0x00,             0,             0,   0,   0  },
	{ 0x01,             0,             0,   0,   0  },
	{ 0x02,             0,             0,   0,   0  },
	{ 0x03,             0,             0,   0,   0  },
	{ 0x04,           0x1,             0,   1,   1  },
	{ 0x05,             0,             0,   0,   0  },
	{ 0x06,             0,             0,   0,   0  },
	{ 0x07,             0,             0,   0,   0  },
	{ 0x08,             0,             0,   0,   0  },
	{ 0x09,             0,             0,   0,   0  },
	{ 0x0A,             0,             0,   0,   0  },
	{ 0x0B,             0,             0,   0,   0  },
	{ 0x0C,             0,             0,   0,   0  },
	{ 0x0D,             0,             0,   0,   0  },
	{ 0x0E,             0,             0,   0,   0  },
	{ 0x0F,             0,             0,   0,   0  },
	{ 0x10,             0,             0,   0,   0  },
	{ 0x11,             0,          0xca,   0,   1  },
	{ 0x12,          0x18,          0x18,   0,   0  },
	{ 0x13,             0,             0,   1,   1  },
	{ 0x14,          0x15,          0x2a,   1,   1  },
	{ 0x15,             0,             0,   0,   0  },
	{ 0x16,           0x1,           0x1,   0,   0  },
	{ 0x17,          0xdb,          0xff,   1,   0  },
	{ 0x18,           0x1,           0x1,   0,   0  },
	{ 0x19,          0x41,          0x41,   0,   0  },
	{ 0x1A,           0x2,           0x2,   0,   0  },
	{ 0x1B,          0x32,          0x32,   0,   0  },
	{ 0x1C,             0,             0,   0,   0  },
	{ 0x1D,             0,             0,   0,   0  },
	{ 0x1E,          0xdd,             0,   1,   1  },
	{ 0x1F,             0,             0,   0,   0  },
	{ 0x20,          0xdd,             0,   1,   1  },
	{ 0x21,          0x77,          0xb5,   1,   1  },
	{ 0x22,             0,          0xff,   1,   1  },
	{ 0x23,          0x1f,          0x1f,   0,   0  },
	{ 0x24,          0x32,          0x32,   0,   0  },
	{ 0x25,          0x32,          0x32,   0,   0  },
	{ 0x26,          0x33,          0x33,   1,   1  },
	{ 0x27,           0x9,           0x9,   0,   0  },
	{ 0x28,             0,             0,   0,   1  },
	{ 0x29,          0x18,          0x18,   0,   0  },
	{ 0x2A,          0x27,          0x27,   0,   0  },
	{ 0x2B,          0x28,          0x28,   0,   0  },
	{ 0x2C,           0x7,           0x7,   0,   0  },
	{ 0x2D,             0,             0,   0,   0  },
	{ 0x2E,           0x8,           0x8,   0,   0  },
	{ 0x2F,          0x82,          0x80,   1,   1  },
	{ 0x30,             0,             0,   0,   0  },
	{ 0x31,             0,             0,   0,   0  },
	{ 0x32,             0,             0,   0,   0  },
	{ 0x33,           0x4,           0x4,   1,   1  },
	{ 0x34,             0,             0,   1,   1  },
	{ 0x35,          0x11,          0x11,   0,   0  },
	{ 0x36,          0x43,          0x43,   0,   0  },
	{ 0x37,          0x33,          0x33,   0,   0  },
	{ 0x38,          0x10,          0x10,   0,   0  },
	{ 0x39,             0,             0,   0,   0  },
	{ 0x3A,             0,             0,   0,   0  },
	{ 0x3B,           0x6,           0x6,   0,   0  },
	{ 0x3C,          0x2a,          0x2a,   0,   0  },
	{ 0x3D,          0xaa,          0xaa,   0,   0  },
	{ 0x3E,          0x21,          0x21,   0,   0  },
	{ 0x3F,          0xaa,          0xaa,   0,   0  },
	{ 0x40,          0x22,          0x22,   0,   0  },
	{ 0x41,           0x1,           0x1,   0,   0  },
	{ 0x42,          0x55,          0x55,   0,   0  },
	{ 0x43,           0x1,           0x1,   0,   0  },
	{ 0x44,          0x55,          0x55,   0,   0  },
	{ 0x45,           0x1,           0x1,   0,   0  },
	{ 0x46,             0,             0,   0,   0  },
	{ 0x47,          0x84,          0x84,   0,   0  },
	{ 0x48,             0,             0,   0,   0  },
	{ 0x49,           0x3,           0x3,   1,   1  },
	{ 0x4A,           0x2,           0x2,   1,   1  },
	{ 0x4B,             0,             0,   0,   0  },
	{ 0x4C,          0x58,          0x58,   0,   0  },
	{ 0x4D,          0x82,          0x82,   0,   0  },
	{ 0x4E,             0,             0,   0,   0  },
	{ 0x4F,             0,             0,   0,   0  },
	{ 0x50,          0xff,          0xff,   0,   0  },
	{ 0x51,          0xff,          0xff,   0,   0  },
	{ 0x52,          0x88,          0x1b,   1,   1  },
	{ 0x53,          0x88,          0x88,   0,   0  },
	{ 0x54,          0x88,          0x88,   0,   0  },
	{ 0x55,          0x33,          0x33,   0,   0  },
	{ 0x56,          0x33,          0x33,   0,   0  },
	{ 0x57,             0,             0,   0,   0  },
	{ 0x58,             0,             0,   0,   0  },
	{ 0x59,             0,             0,   0,   0  },
	{ 0x5A,          0x33,          0x33,   0,   0  },
	{ 0x5B,          0x55,          0x55,   0,   0  },
	{ 0x5C,          0x32,          0x32,   0,   0  },
	{ 0x5D,             0,             0,   0,   0  },
	{ 0x5E,             0,             0,   0,   0  },
	{ 0x5F,          0x15,          0x15,   0,   0  },
	{ 0x60,           0xf,           0xf,   0,   0  },
	{ 0x61,             0,             0,   0,   0  },
	{ 0x62,             0,             0,   0,   0  },
	{ 0x63,             0,             0,   0,   0  },
	{ 0x64,             0,             0,   0,   0  },
	{ 0x65,             0,             0,   0,   0  },
	{ 0x66,             0,             0,   0,   0  },
	{ 0x69,             0,             0,   0,   0  },
	{ 0x6A,             0,             0,   0,   0  },
	{ 0x6B,             0,             0,   0,   0  },
	{ 0x6C,             0,             0,   0,   0  },
	{ 0x4000,             0,             0,   0,   0  },
	{ 0x4001,             0,             0,   0,   0  },
	{ 0x4002,             0,             0,   0,   0  },
	{ 0x4003,             0,             0,   0,   0  },
	{ 0x4004,           0x1,             0,   1,   1  },
	{ 0x4005,             0,             0,   0,   0  },
	{ 0x4006,             0,             0,   0,   0  },
	{ 0x4007,             0,             0,   0,   0  },
	{ 0x4008,             0,             0,   0,   0  },
	{ 0x4009,             0,             0,   0,   0  },
	{ 0x400A,             0,             0,   0,   0  },
	{ 0x400B,             0,             0,   0,   0  },
	{ 0x400C,             0,             0,   0,   0  },
	{ 0x400D,             0,             0,   0,   0  },
	{ 0x400E,             0,             0,   0,   0  },
	{ 0x400F,             0,             0,   0,   0  },
	{ 0x4010,          0xff,          0xff,   1,   1  },
	{ 0x4011,             0,             0,   0,   0  },
	{ 0x4012,          0x8e,          0x8e,   0,   0  },
	{ 0x4013,             0,             0,   0,   0  },
	{ 0x4014,           0x6,           0x6,   0,   0  },
	{ 0x4015,             0,             0,   0,   0  },
	{ 0x4016,          0x11,          0x11,   0,   0  },
	{ 0x4017,          0xf8,          0xd8,   1,   1  },
	{ 0x4018,          0x3c,          0x24,   1,   1  },
	{ 0x4019,             0,             0,   0,   0  },
	{ 0x401A,          0x41,          0x41,   0,   0  },
	{ 0x401B,           0x2,           0x2,   0,   0  },
	{ 0x401C,          0x33,          0x33,   0,   0  },
	{ 0x401D,          0x22,          0x22,   0,   0  },
	{ 0x401E,             0,             0,   0,   0  },
	{ 0x401F,          0x88,          0x80,   1,   1  },
	{ 0x4020,          0x88,          0x88,   0,   0  },
	{ 0x4021,          0x88,          0x80,   1,   1  },
	{ 0x4022,             0,             0,   0,   0  },
	{ 0x4023,             0,             0,   0,   0  },
	{ 0x4024,           0x7,           0x7,   0,   0  },
	{ 0x4025,          0xaf,          0xaf,   0,   0  },
	{ 0x4026,          0x12,          0x12,   0,   0  },
	{ 0x4027,           0xb,           0xb,   0,   0  },
	{ 0x4028,          0x5f,          0x5f,   0,   0  },
	{ 0x4029,             0,             0,   0,   0  },
	{ 0x402A,          0x40,          0x40,   0,   0  },
	{ 0x402B,          0x52,          0x52,   0,   0  },
	{ 0x402C,          0x26,          0x26,   0,   0  },
	{ 0x402D,           0x3,           0x3,   0,   0  },
	{ 0x402E,          0x36,          0x36,   0,   0  },
	{ 0x402F,          0x57,          0x57,   0,   0  },
	{ 0x4030,          0x11,          0x11,   0,   0  },
	{ 0x4031,          0x75,          0x75,   0,   0  },
	{ 0x4032,          0xb4,          0xb4,   0,   0  },
	{ 0x4033,             0,             0,   0,   0  },
	{ 0x4034,          0x98,          0x98,   1,   1  },
	{ 0x4035,          0x10,          0x10,   1,   1  },
	{ 0x4036,             0,             0,   0,   0  },
	{ 0x4037,             0,             0,   0,   0  },
	{ 0x4038,             0,             0,   0,   0  },
	{ 0x4039,          0x43,          0x43,   1,   1  },
	{ 0x403A,          0x47,          0x47,   1,   1  },
	{ 0x403B,           0xc,           0xc,   1,   1  },
	{ 0x403C,          0x11,          0x11,   1,   1  },
	{ 0x403D,          0x11,          0x11,   1,   1  },
	{ 0x403E,           0xe,           0xe,   1,   1  },
	{ 0x403F,           0x8,           0x8,   1,   1  },
	{ 0x4040,          0x33,          0x33,   1,   1  },
	{ 0x4041,           0xa,           0xa,   1,   1  },
	{ 0x4042,           0x16,           0x16,   1,   1  },
	{ 0x4043,             0,             0,   0,   0  },
	{ 0x4044,             0,             0,   0,   0  },
	{ 0x4045,             0,             0,   0,   0  },
	{ 0x4046,          0x3e,          0x3e,   1,   1  },
	{ 0x4047,          0x13,          0x13,   1,   1  },
	{ 0x4048,             0,             0,   0,   0  },
	{ 0x4049,          0x62,          0x62,   1,   1  },
	{ 0x404A,           0x7,           0x7,   1,   1  },
	{ 0x404B,          0x16,          0x16,   1,   1  },
	{ 0x404C,          0x5c,          0x5c,   1,   1  },
	{ 0x404D,          0x95,          0x95,   1,   1  },
	{ 0x404E,             0,             0,   0,   0  },
	{ 0x404F,             0,             0,   0,   0  },
	{ 0x4050,             0,             0,   0,   0  },
	{ 0x4051,             0,             0,   0,   0  },
	{ 0x4052,          0xa0,          0xa0,   1,   1  },
	{ 0x4053,           0x4,           0x4,   1,   1  },
	{ 0x4054,             0,             0,   0,   0  },
	{ 0x4055,          0xcc,          0xcc,   1,   1  },
	{ 0x4056,           0x7,           0x7,   1,   1  },
	{ 0x4057,          0x10,          0x10,   0,   0  },
	{ 0x4058,             0,             0,   0,   0  },
	{ 0x4059,             0,             0,   0,   0  },
	{ 0x405A,             0,             0,   0,   0  },
	{ 0x405B,             0,             0,   0,   0  },
	{ 0x405C,          0x55,          0x55,   0,   0  },
	{ 0x405D,          0x55,          0x55,   0,   0  },
	{ 0x405E,           0x5,           0x5,   0,   0  },
	{ 0x405F,           0xf,           0xf,   1,   0  },
	{ 0x4060,             0,             0,   0,   0  },
	{ 0x4061,          0x55,          0x55,   0,   0  },
	{ 0x4062,          0x66,          0x66,   0,   0  },
	{ 0x4063,          0x55,          0x55,   0,   0  },
	{ 0x4064,          0x44,          0x44,   0,   0  },
	{ 0x4065,          0xa0,          0xa0,   0,   0  },
	{ 0x4066,           0x4,           0x4,   0,   0  },
	{ 0x4067,             0,             0,   0,   0  },
	{ 0x4068,          0x55,          0x55,   0,   0  },
	{ 0xFFFF,             0,             0,   0,   0  }
	};

/* Init values for 2063 regs (autogenerated by 2063_regs_rev0_tcl2c.tcl)
 *   Entries: addr, init value A, init value G, do_init A, do_init G.
 *   Last line (addr FF is dummy as delimiter. This table has dual use
 *   between dumping and initializing.
 */
radio_regs_t WLBANDINITDATA(regs_2063_rev0)[] = {
{ 0x00,             0,             0,   0,   1  },
{ 0x01,     0x206317f,     0x206317f,   0,   0  },
{ 0x02,             0,             0,   0,   0  },
{ 0x03,             0,             0,   0,   0  },
{ 0x04,             0,             0,   0,   0  },
{ 0x05,             0,             0,   0,   0  },
{ 0x06,             0,             0,   0,   0  },
{ 0x07,             0,             0,   0,   0  },
{ 0x08,             0,             0,   0,   0  },
{ 0x09,             0,             0,   0,   0  },
{ 0x0A,           0x1,             0,   1,   0  },
{ 0x0B,             0,             0,   0,   0  },
{ 0x0C,             0,             0,   0,   0  },
{ 0x0D,             0,             0,   0,   0  },
{ 0x0E,           0x6,           0x6,   0,   0  },
{ 0x0F,           0xf,           0xf,   0,   0  },
{ 0x10,             0,             0,   0,   1  },
{ 0x11,             0,             0,   0,   1  },
{ 0x12,             0,             0,   0,   1  },
{ 0x13,             0,             0,   0,   1  },
{ 0x14,             0,             0,   0,   1  },
{ 0x15,             0,             0,   0,   1  },
{ 0x16,             0,             0,   0,   1  },
{ 0x17,             0,             0,   0,   1  },
{ 0x18,             0,             0,   0,   1  },
{ 0x19,          0x7f,          0x7f,   0,   0  },
{ 0x1A,          0x3f,          0x3f,   0,   0  },
{ 0x1B,             0,             0,   0,   0  },
{ 0x1C,          0xe8,          0xd4,   1,   1  },
{ 0x1D,          0xa7,          0x53,   1,   1  },
{ 0x1E,          0xff,          0xff,   0,   0  },
{ 0x1F,          0xf0,           0xf,   1,   1  },
{ 0x20,           0x1,           0x1,   0,   0  },
{ 0x21,          0x1f,          0x5e,   0,   1  },
{ 0x22,          0x7f,          0x7e,   0,   1  },
{ 0x23,          0x30,          0xf0,   0,   1  },
{ 0x24,          0x35,          0x35,   0,   0  },
{ 0x25,          0x3f,          0x3f,   0,   0  },
{ 0x26,             0,             0,   0,   0  },
{ 0x27,          0x7f,          0x7f,   1,   1  },
{ 0x28,             0,             0,   0,   0  },
{ 0x29,             0,             0,   0,   0  },
{ 0x2A,           0xc,           0xc,   1,   1  },
{ 0x2B,             0,             0,   0,   0  },
{ 0x2C,          0x3c,          0x3f,   1,   0  },
{ 0x2D,          0xfc,          0xfe,   1,   0  },
{ 0x2E,          0xff,          0xff,   0,   0  },
{ 0x2F,          0xff,          0xff,   0,   0  },
{ 0x30,             0,             0,   0,   0  },
{ 0x31,             0,             0,   0,   0  },
{ 0x32,           0x8,           0x8,   1,   1  },
{ 0x33,           0xf,           0xf,   0,   0  },
{ 0x34,          0x22,          0x22,   0,   0  },
{ 0x35,          0xa8,          0xa8,   0,   0  },
{ 0x36,          0x60,          0x60,   1,   1  },
{ 0x37,          0x11,          0x11,   0,   0  },
{ 0x38,             0,             0,   0,   0  },
{ 0x39,             0,             0,   0,   0  },
{ 0x3A,          0x30,          0x30,   1,   1  },
{ 0x3B,           0x1,           0x1,   0,   0  },
{ 0x3C,           0x3,           0x3,   0,   0  },
{ 0x3D,           0xc,           0xb,   1,   1  },
{ 0x3E,          0x10,           0xf,   1,   1  },
{ 0x3F,           0xf,           0xf,   0,   0  },
{ 0x40,          0x80,          0x80,   0,   0  },
{ 0x41,          0x68,          0x68,   0,   0  },
{ 0x42,          0x68,          0x68,   0,   0  },
{ 0x43,          0x80,          0x80,   0,   0  },
{ 0x44,          0xff,          0xff,   0,   0  },
{ 0x45,           0x3,           0x3,   0,   0  },
{ 0x46,          0x38,          0x38,   0,   0  },
{ 0x47,          0xff,          0xff,   0,   0  },
{ 0x48,          0x38,          0x38,   0,   0  },
{ 0x49,          0xc0,          0xc0,   0,   0  },
{ 0x4A,          0xff,          0xff,   0,   0  },
{ 0x4B,          0xff,          0xff,   0,   0  },
{ 0x4C,          0x3d,          0xfd,   1,   1  },
{ 0x4D,           0xc,           0xc,   0,   0  },
{ 0x4E,          0x96,          0x96,   0,   0  },
{ 0x4F,          0x5a,          0x5a,   0,   0  },
{ 0x50,          0x7f,          0x7f,   0,   0  },
{ 0x51,          0x7f,          0x7f,   0,   0  },
{ 0x52,          0x33,          0x33,   0,   0  },
{ 0x53,           0x2,           0x2,   1,   1  },
{ 0x54,             0,             0,   0,   0  },
{ 0x55,          0x30,          0x30,   0,   0  },
{ 0x56,             0,             0,   0,   0  },
{ 0x57,          0x56,          0x56,   1,   1  },
{ 0x58,           0x6,           0x6,   0,   0  },
{ 0x59,           0xe,           0xe,   0,   0  },
{ 0x5A,          0x7e,          0x7e,   0,   0  },
{ 0x5B,          0x15,          0x15,   0,   0  },
{ 0x5C,           0xf,           0xf,   0,   0  },
{ 0x5D,             0,             0,   0,   0  },
{ 0x5E,             0,             0,   0,   0  },
{ 0x5F,             0,             0,   0,   0  },
{ 0x60,             0,             0,   0,   0  },
{ 0x61,             0,             0,   0,   0  },
{ 0x62,             0,             0,   0,   0  },
{ 0x63,             0,             0,   0,   0  },
{ 0x64,           0x4,           0x4,   0,   0  },
{ 0x65,             0,             0,   0,   0  },
{ 0x66,             0,             0,   0,   0  },
{ 0x67,          0xcf,          0xcf,   0,   0  },
{ 0x68,          0x59,          0x59,   0,   0  },
{ 0x69,           0x7,           0x7,   0,   0  },
{ 0x6A,          0x42,          0x42,   0,   0  },
{ 0x6B,             0,             0,   0,   0  },
{ 0x6C,          0xdb,          0xdb,   0,   0  },
{ 0x6D,          0x94,          0x94,   0,   0  },
{ 0x6E,          0x28,          0x28,   0,   0  },
{ 0x6F,          0x63,          0x63,   0,   0  },
{ 0x70,           0x7,           0x7,   0,   0  },
{ 0x71,          0xd3,          0xd3,   0,   0  },
{ 0x72,          0xb1,          0xb1,   0,   0  },
{ 0x73,          0x3b,          0x3b,   0,   0  },
{ 0x74,           0x6,           0x6,   0,   0  },
{ 0x75,          0x58,          0x58,   0,   0  },
{ 0x76,          0xf7,          0xf7,   1,   1  },
{ 0x77,             0,             0,   0,   0  },
{ 0x78,             0,             0,   0,   0  },
{ 0x79,           0x2,           0x2,   0,   0  },
{ 0x7A,             0,             0,   0,   0  },
{ 0x7B,           0x9,           0x9,   0,   0  },
{ 0x7C,           0x5,           0x5,   0,   0  },
{ 0x7D,          0x16,          0x16,   0,   0  },
{ 0x7E,          0x6b,          0x6b,   0,   0  },
{ 0x7F,             0,             0,   0,   0  },
{ 0x80,          0xb3,          0xb3,   0,   0  },
{ 0x81,           0x4,           0x4,   0,   0  },
{ 0x82,             0,             0,   0,   0  },
{ 0x83,             0,             0,   0,   0  },
{ 0x84,             0,             0,   0,   0  },
{ 0x85,             0,             0,   0,   0  },
{ 0x86,             0,             0,   0,   0  },
{ 0x87,             0,             0,   0,   0  },
{ 0x88,             0,             0,   0,   0  },
{ 0x89,             0,             0,   0,   0  },
{ 0x8A,             0,             0,   0,   0  },
{ 0x8B,             0,             0,   0,   0  },
{ 0x8C,           0x2,           0x2,   0,   0  },
{ 0x8D,             0,             0,   0,   0  },
{ 0x8E,             0,             0,   0,   0  },
{ 0x8F,          0x66,          0x66,   0,   0  },
{ 0x90,          0x66,          0x66,   0,   0  },
{ 0x91,          0x66,          0x66,   0,   0  },
{ 0x92,          0x66,          0x66,   0,   0  },
{ 0x93,          0x66,          0x66,   0,   0  },
{ 0x94,          0x66,          0x66,   0,   0  },
{ 0x95,          0x66,          0x66,   0,   0  },
{ 0x96,             0,             0,   0,   0  },
{ 0x97,             0,             0,   0,   0  },
{ 0x98,             0,             0,   0,   0  },
{ 0x99,             0,             0,   0,   0  },
{ 0x9A,             0,             0,   0,   0  },
{ 0x9B,             0,             0,   0,   0  },
{ 0x9C,             0,             0,   0,   0  },
{ 0x9D,             0,             0,   0,   0  },
{ 0x9E,             0,             0,   0,   0  },
{ 0x9F,             0,             0,   0,   0  },
{ 0xA0,          0xff,          0xff,   0,   0  },
{ 0xA1,             0,             0,   0,   0  },
{ 0xA2,          0x60,          0x60,   0,   0  },
{ 0xA3,          0x66,          0x66,   0,   0  },
{ 0xA4,           0xc,           0xc,   0,   0  },
{ 0xA5,          0x66,          0x66,   0,   0  },
{ 0xA6,           0xc,           0xc,   0,   0  },
{ 0xA7,           0x1,           0x1,   0,   0  },
{ 0xA8,          0x66,          0x66,   0,   0  },
{ 0xA9,          0x66,          0x66,   0,   0  },
{ 0xAA,          0x66,          0x66,   0,   0  },
{ 0xAB,          0x66,          0x66,   0,   0  },
{ 0xAC,          0x66,          0x66,   0,   0  },
{ 0xAD,          0x66,          0x66,   0,   0  },
{ 0xAE,             0,             0,   0,   0  },
{ 0xAF,           0x1,           0x1,   0,   0  },
{ 0xB0,             0,             0,   0,   0  },
{ 0xB1,             0,             0,   0,   0  },
{ 0xB2,          0x33,          0x33,   0,   0  },
{ 0xB3,             0,             0,   0,   0  },
{ 0xB4,           0x5,           0x5,   0,   0  },
{ 0xB5,          0x30,          0x30,   0,   0  },
{ 0xB6,          0x55,          0x55,   0,   0  },
{ 0xB7,          0x33,          0x33,   0,   0  },
{ 0xB8,             0,             0,   0,   0  },
{ 0xB9,          0x33,          0x33,   0,   0  },
{ 0xBA,             0,             0,   0,   0  },
{ 0xBB,          0x35,          0x35,   0,   0  },
{ 0xBC,             0,             0,   0,   0  },
{ 0xBD,          0x33,          0x33,   0,   0  },
{ 0xBE,             0,             0,   0,   0  },
{ 0xBF,          0x33,          0x33,   0,   0  },
{ 0xC0,             0,             0,   0,   0  },
{ 0xC1,             0,             0,   0,   0  },
{ 0xC2,          0x44,          0x44,   0,   0  },
{ 0xC3,             0,             0,   0,   0  },
{ 0xC4,          0x71,          0x71,   1,   1  },
{ 0xC5,          0x71,          0x71,   1,   1  },
{ 0xC6,           0x3,           0x3,   0,   0  },
{ 0xC7,          0x88,          0x88,   0,   0  },
{ 0xC8,          0x44,          0x44,   0,   0  },
{ 0xC9,           0x1,           0x1,   0,   0  },
{ 0xCA,             0,             0,   0,   0  },
{ 0xCB,             0,             0,   0,   0  },
{ 0xCC,             0,             0,   0,   0  },
{ 0xCD,             0,             0,   0,   0  },
{ 0xCE,             0,             0,   0,   0  },
{ 0xCF,          0xf0,          0x30,   1,   0  },
{ 0xD0,           0x5,           0x5,   0,   0  },
{ 0xD1,          0x33,          0x33,   0,   0  },
{ 0xD2,             0,             0,   0,   0  },
{ 0xD3,           0x5,           0x5,   0,   0  },
{ 0xD4,             0,             0,   0,   0  },
{ 0xD5,             0,             0,   0,   0  },
{ 0xD6,           0x5,           0x5,   0,   0  },
{ 0xD7,             0,             0,   0,   0  },
{ 0xD8,             0,             0,   0,   0  },
{ 0xD9,           0x5,           0x5,   0,   0  },
{ 0xDA,             0,             0,   0,   0  },
{ 0xDB,          0x33,          0x33,   0,   0  },
{ 0xDC,             0,             0,   0,   0  },
{ 0xDD,          0x33,          0x33,   0,   0  },
{ 0xDE,             0,             0,   0,   0  },
{ 0xDF,          0x77,          0x77,   1,   1  },
{ 0xE0,          0x88,          0x88,   0,   0  },
{ 0xE1,             0,             0,   0,   0  },
{ 0xE2,          0x44,          0x44,   0,   0  },
{ 0xE3,           0x3,           0x3,   1,   1  },
{ 0xE4,           0xf,           0xf,   1,   1  },
{ 0xE5,           0xf,           0xf,   1,   1  },
{ 0xE6,          0x44,          0x44,   0,   0  },
{ 0xE7,           0x1,           0x1,   0,   0  },
{ 0xE8,             0,             0,   0,   0  },
{ 0xE9,             0,             0,   0,   0  },
{ 0xEA,             0,             0,   0,   0  },
{ 0xEB,             0,             0,   0,   0  },
{ 0xEC,          0x77,          0x77,   1,   1  },
{ 0xED,          0x58,          0x58,   0,   0  },
{ 0xEE,          0x77,          0x77,   1,   1  },
{ 0xEF,          0x58,          0x58,   0,   0  },
{ 0xF0,             0,             0,   0,   0  },
{ 0xF1,             0,             0,   0,   0  },
{ 0xF2,          0x74,          0x74,   0,   0  },
{ 0xF3,           0x4,           0x4,   1,   1  },
{ 0xF4,          0xa2,          0xa2,   0,   0  },
{ 0xF5,          0xaa,          0xaa,   0,   0  },
{ 0xF6,          0x24,          0x24,   0,   0  },
{ 0xF7,          0xa9,          0xa9,   0,   0  },
{ 0xF8,          0x28,          0x28,   0,   0  },
{ 0xF9,          0x10,          0x10,   0,   0  },
{ 0xFA,          0x55,          0x55,   0,   0  },
{ 0xFB,          0x80,          0x80,   0,   0  },
{ 0xFC,          0x88,          0x88,   0,   0  },
{ 0xFD,          0x88,          0x88,   0,   0  },
{ 0xFE,          0x88,          0x88,   0,   0  },
{ 0xFF,          0x88,          0x88,   0,   0  },
{ 0x100,          0x80,          0x80,   0,   0  },
{ 0x101,          0x38,          0x38,   0,   0  },
{ 0x102,          0xb8,          0xb8,   0,   0  },
{ 0x103,          0x80,          0x80,   0,   0  },
{ 0x104,          0x38,          0x38,   0,   0  },
{ 0x105,          0x78,          0x78,   0,   0  },
{ 0x106,          0xc0,          0xc0,   0,   0  },
{ 0x107,           0x3,           0x3,   0,   0  },
{ 0x108,             0,             0,   0,   0  },
{ 0x109,             0,             0,   0,   0  },
{ 0x10A,             0,             0,   0,   0  },
{ 0x10B,             0,           0x4,   1,   0  },
{ 0x10C,           0xc,           0xc,   0,   0  },
{ 0x10D,             0,             0,   0,   0  },
{ 0x10E,             0,             0,   0,   0  },
{ 0x10F,          0x96,          0x96,   0,   0  },
{ 0x110,          0x77,          0x77,   0,   0  },
{ 0x111,          0x5a,          0x5a,   0,   0  },
{ 0x112,             0,             0,   0,   0  },
{ 0x113,             0,             0,   0,   0  },
{ 0x114,          0x21,          0x21,   0,   0  },
{ 0x115,          0x70,          0x70,   0,   0  },
{ 0x116,             0,             0,   0,   0  },
{ 0x117,             0,             0,   0,   0  },
{ 0x118,             0,             0,   0,   0  },
{ 0x119,          0xb3,          0xb3,   0,   0  },
{ 0x11A,          0x55,          0x55,   0,   0  },
{ 0x11B,           0xb,           0xb,   0,   0  },
{ 0x11C,             0,             0,   0,   0  },
{ 0x11D,           0x3,           0x3,   1,   1  },
{ 0x11E,             0,             0,   0,   0  },
{ 0x11F,          0xb3,          0xb3,   0,   0  },
{ 0x120,          0x55,          0x55,   0,   0  },
{ 0x121,          0x30,          0x30,   0,   0  },
{ 0x122,          0x46,          0x46,   0,   0  },
{ 0x123,             0,             0,   0,   0  },
{ 0x124,             0,             0,   0,   0  },
{ 0x125,             0,             0,   0,   0  },
{ 0x126,          0x21,          0x21,   0,   0  },
{ 0x127,          0x23,          0x23,   0,   0  },
{ 0x128,           0x2,           0x2,   0,   0  },
{ 0xFFFF,             0,             0,   0,   0  },
};

/* Init values for 2063 regs (autogenerated by 2063_regs_rev1_csm_tcl2c.tcl)
 *   Entries: addr, init value A, init value G, do_init A, do_init G.
 *   Last line (addr FF is dummy as delimiter. This table has dual use
 *   between dumping and initializing.
 */
radio_regs_t WLBANDINITDATA(regs_2063_rev1)[] = {
{ 0x00,             0,             0,   0,   1  },
{ 0x01,    0x1206317f,    0x1206317f,   0,   0  },
{ 0x02,             0,             0,   0,   0  },
{ 0x03,             0,             0,   0,   0  },
{ 0x04,             0,             0,   0,   0  },
{ 0x05,             0,             0,   0,   0  },
{ 0x06,             0,             0,   0,   0  },
{ 0x07,             0,             0,   0,   0  },
{ 0x08,             0,             0,   0,   0  },
{ 0x09,             0,             0,   0,   0  },
{ 0x0A,           0x1,             0,   1,   0  },
{ 0x0B,             0,             0,   0,   0  },
{ 0x0C,             0,             0,   0,   0  },
{ 0x0D,             0,             0,   0,   0  },
{ 0x0E,           0x6,           0x6,   0,   0  },
{ 0x0F,           0xf,           0xf,   0,   0  },
{ 0x10,             0,             0,   0,   0  },
{ 0x11,           0xc,           0xc,   0,   0  },
{ 0x12,             0,             0,   0,   1  },
{ 0x13,             0,             0,   0,   1  },
{ 0x14,             0,             0,   0,   1  },
{ 0x15,             0,             0,   0,   1  },
{ 0x16,             0,             0,   0,   1  },
{ 0x17,             0,             0,   0,   1  },
{ 0x18,             0,             0,   0,   1  },
{ 0x19,          0x7f,          0x7f,   0,   0  },
{ 0x1A,          0x3f,          0x3f,   0,   0  },
{ 0x1B,             0,             0,   0,   0  },
{ 0x1C,          0xe8,          0xd4,   1,   1  },
{ 0x1D,          0xa7,          0x53,   1,   1  },
{ 0x1E,          0xff,          0xff,   0,   0  },
{ 0x1F,          0xf0,           0xf,   1,   1  },
{ 0x20,           0x1,           0x1,   0,   0  },
{ 0x21,          0x1f,          0x5e,   0,   1  },
{ 0x22,          0x7f,          0x7e,   0,   1  },
{ 0x23,          0x30,          0xf0,   0,   1  },
{ 0x24,          0x35,          0x35,   0,   0  },
{ 0x25,          0x3f,          0x3f,   0,   0  },
{ 0x26,           0x2,           0x2,   1,   1  },
{ 0x27,          0x7f,          0x7f,   1,   1  },
{ 0x28,             0,             0,   0,   0  },
{ 0x29,             0,             0,   0,   0  },
{ 0x2A,           0xc,           0xc,   1,   1  },
{ 0x2B,             0,             0,   0,   0  },
{ 0x2C,          0x3c,          0x3f,   1,   0  },
{ 0x2D,          0xfc,          0xfe,   1,   0  },
{ 0x2E,          0xff,          0xff,   0,   0  },
{ 0x2F,          0xff,          0xff,   0,   0  },
{ 0x30,             0,             0,   0,   0  },
{ 0x31,             0,             0,   0,   0  },
{ 0x32,           0x8,           0x8,   1,   1  },
{ 0x33,           0xf,           0xf,   0,   0  },
{ 0x34,          0x22,          0x22,   0,   0  },
{ 0x35,          0xa8,          0xa8,   0,   0  },
{ 0x36,          0x60,          0x60,   1,   1  },
{ 0x37,          0x11,          0x11,   0,   0  },
{ 0x38,             0,             0,   0,   0  },
{ 0x39,             0,             0,   0,   0  },
{ 0x3A,          0x30,          0x30,   1,   1  },
{ 0x3B,           0x1,           0x1,   0,   0  },
{ 0x3C,           0x3,           0x3,   0,   0  },
{ 0x3D,           0xc,           0xb,   1,   1  },
{ 0x3E,          0x10,           0xf,   1,   1  },
{ 0x3F,           0xf,           0xf,   0,   0  },
{ 0x40,          0x80,          0x80,   0,   0  },
{ 0x41,          0x68,          0x68,   0,   0  },
{ 0x42,          0x68,          0x68,   0,   0  },
{ 0x43,          0x80,          0x80,   0,   0  },
{ 0x44,          0xff,          0xff,   0,   0  },
{ 0x45,           0x3,           0x3,   0,   0  },
{ 0x46,          0x38,          0x38,   0,   0  },
{ 0x47,          0xff,          0xff,   0,   0  },
{ 0x48,          0x38,          0x38,   0,   0  },
{ 0x49,          0xc0,          0xc0,   0,   0  },
{ 0x4A,          0xff,          0xff,   0,   0  },
{ 0x4B,          0xff,          0xff,   0,   0  },
{ 0x4C,          0x3d,          0xfd,   1,   1  },
{ 0x4D,          0x20,          0x20,   1,   1  },
{ 0x4E,          0x20,          0x20,   1,   1  },
{ 0x4F,          0x20,          0x20,   1,   1  },
{ 0x50,          0x7f,          0x7f,   0,   0  },
{ 0x51,          0x7f,          0x7f,   0,   0  },
{ 0x52,          0x33,          0x33,   0,   0  },
{ 0x53,           0x2,           0x2,   1,   1  },
{ 0x54,             0,             0,   0,   0  },
{ 0x55,          0x30,          0x30,   0,   0  },
{ 0x56,             0,             0,   0,   0  },
{ 0x57,          0x56,          0x56,   1,   1  },
{ 0x58,           0x6,           0x6,   0,   0  },
{ 0x59,           0xe,           0xe,   0,   0  },
{ 0x5A,          0x7e,          0x7e,   0,   0  },
{ 0x5B,          0x15,          0x15,   0,   0  },
{ 0x5C,           0xf,           0xf,   0,   0  },
{ 0x5D,             0,             0,   0,   0  },
{ 0x5E,             0,             0,   0,   0  },
{ 0x5F,             0,             0,   0,   0  },
{ 0x60,             0,             0,   0,   0  },
{ 0x61,             0,             0,   0,   0  },
{ 0x62,             0,             0,   0,   0  },
{ 0x63,             0,             0,   0,   0  },
{ 0x64,           0x4,           0x4,   0,   0  },
{ 0x65,             0,             0,   0,   0  },
{ 0x66,             0,             0,   0,   0  },
{ 0x67,          0xcf,          0xcf,   0,   0  },
{ 0x68,          0x59,          0x59,   0,   0  },
{ 0x69,           0x7,           0x7,   0,   0  },
{ 0x6A,          0x42,          0x42,   0,   0  },
{ 0x6B,             0,             0,   0,   0  },
{ 0x6C,          0xdb,          0xdb,   0,   0  },
{ 0x6D,          0x94,          0x94,   0,   0  },
{ 0x6E,          0x28,          0x28,   0,   0  },
{ 0x6F,          0x63,          0x63,   0,   0  },
{ 0x70,           0x7,           0x7,   0,   0  },
{ 0x71,          0xd3,          0xd3,   0,   0  },
{ 0x72,          0xb1,          0xb1,   0,   0  },
{ 0x73,          0x3b,          0x3b,   0,   0  },
{ 0x74,           0x6,           0x6,   0,   0  },
{ 0x75,          0x58,          0x58,   0,   0  },
{ 0x76,          0xf7,          0xf7,   1,   1  },
{ 0x77,             0,             0,   0,   0  },
{ 0x78,             0,             0,   0,   0  },
{ 0x79,           0x2,           0x2,   0,   0  },
{ 0x7A,             0,             0,   0,   0  },
{ 0x7B,           0x9,           0x9,   0,   0  },
{ 0x7C,           0x5,           0x5,   0,   0  },
{ 0x7D,          0x16,          0x16,   0,   0  },
{ 0x7E,          0x6b,          0x6b,   0,   0  },
{ 0x7F,             0,             0,   0,   0  },
{ 0x80,          0xb3,          0xb3,   0,   0  },
{ 0x81,           0x4,           0x4,   0,   0  },
{ 0x82,             0,             0,   0,   0  },
{ 0x83,             0,             0,   0,   0  },
{ 0x84,             0,             0,   0,   0  },
{ 0x85,             0,             0,   0,   0  },
{ 0x86,             0,             0,   0,   0  },
{ 0x87,             0,             0,   0,   0  },
{ 0x88,             0,             0,   0,   0  },
{ 0x89,             0,             0,   0,   0  },
{ 0x8A,             0,             0,   0,   0  },
{ 0x8B,             0,             0,   0,   0  },
{ 0x8C,           0x2,           0x2,   0,   0  },
{ 0x8D,             0,             0,   0,   0  },
{ 0x8E,             0,             0,   0,   0  },
{ 0x8F,          0x66,          0x66,   0,   0  },
{ 0x90,          0x66,          0x66,   0,   0  },
{ 0x91,          0x66,          0x66,   0,   0  },
{ 0x92,          0x66,          0x66,   0,   0  },
{ 0x93,          0x66,          0x66,   0,   0  },
{ 0x94,          0x66,          0x66,   0,   0  },
{ 0x95,          0x66,          0x66,   0,   0  },
{ 0x96,             0,             0,   0,   0  },
{ 0x97,             0,             0,   0,   0  },
{ 0x98,             0,             0,   0,   0  },
{ 0x99,             0,             0,   0,   0  },
{ 0x9A,             0,             0,   0,   0  },
{ 0x9B,             0,             0,   0,   0  },
{ 0x9C,             0,             0,   0,   0  },
{ 0x9D,             0,             0,   0,   0  },
{ 0x9E,             0,             0,   0,   0  },
{ 0x9F,             0,             0,   0,   0  },
{ 0xA0,          0xff,          0xff,   0,   0  },
{ 0xA1,             0,             0,   0,   0  },
{ 0xA2,          0x60,          0x60,   0,   0  },
{ 0xA3,          0x66,          0x66,   0,   0  },
{ 0xA4,           0xc,           0xc,   0,   0  },
{ 0xA5,          0x66,          0x66,   0,   0  },
{ 0xA6,           0xc,           0xc,   0,   0  },
{ 0xA7,           0x1,           0x1,   0,   0  },
{ 0xA8,          0x66,          0x66,   0,   0  },
{ 0xA9,          0x66,          0x66,   0,   0  },
{ 0xAA,          0x66,          0x66,   0,   0  },
{ 0xAB,          0x66,          0x66,   0,   0  },
{ 0xAC,          0x66,          0x66,   0,   0  },
{ 0xAD,          0x66,          0x66,   0,   0  },
{ 0xAE,             0,             0,   0,   0  },
{ 0xAF,           0x1,           0x1,   0,   0  },
{ 0xB0,             0,             0,   0,   0  },
{ 0xB1,             0,             0,   0,   0  },
{ 0xB2,          0xf0,          0xf0,   1,   1  },
{ 0xB3,             0,             0,   0,   0  },
{ 0xB4,           0x5,           0x5,   0,   0  },
{ 0xB5,          0x30,          0x30,   0,   0  },
{ 0xB6,          0x55,          0x55,   0,   0  },
{ 0xB7,          0x33,          0x33,   0,   0  },
{ 0xB8,             0,             0,   0,   0  },
{ 0xB9,          0x33,          0x33,   0,   0  },
{ 0xBA,             0,             0,   0,   0  },
{ 0xBB,          0x35,          0x35,   0,   0  },
{ 0xBC,             0,             0,   0,   0  },
{ 0xBD,          0x33,          0x33,   0,   0  },
{ 0xBE,             0,             0,   0,   0  },
{ 0xBF,          0x33,          0x33,   0,   0  },
{ 0xC0,             0,             0,   0,   0  },
{ 0xC1,             0,             0,   0,   0  },
{ 0xC2,          0x44,          0x44,   0,   0  },
{ 0xC3,             0,             0,   0,   0  },
{ 0xC4,          0x71,          0x71,   1,   1  },
{ 0xC5,          0x71,          0x71,   1,   1  },
{ 0xC6,           0x3,           0x3,   0,   0  },
{ 0xC7,          0x88,          0x88,   0,   0  },
{ 0xC8,          0x44,          0x44,   0,   0  },
{ 0xC9,           0x1,           0x1,   0,   0  },
{ 0xCA,             0,             0,   0,   0  },
{ 0xCB,             0,             0,   0,   0  },
{ 0xCC,             0,             0,   0,   0  },
{ 0xCD,             0,             0,   0,   0  },
{ 0xCE,             0,             0,   0,   0  },
{ 0xCF,          0xf0,          0x30,   1,   0  },
{ 0xD0,           0x5,           0x5,   0,   0  },
{ 0xD1,          0x33,          0x33,   0,   0  },
{ 0xD2,             0,             0,   0,   0  },
{ 0xD3,           0x5,           0x5,   0,   0  },
{ 0xD4,             0,             0,   0,   0  },
{ 0xD5,             0,             0,   0,   0  },
{ 0xD6,           0x5,           0x5,   0,   0  },
{ 0xD7,             0,             0,   0,   0  },
{ 0xD8,             0,             0,   0,   0  },
{ 0xD9,           0x5,           0x5,   0,   0  },
{ 0xDA,             0,             0,   0,   0  },
{ 0xDB,          0x33,          0x33,   0,   0  },
{ 0xDC,             0,             0,   0,   0  },
{ 0xDD,          0x33,          0x33,   0,   0  },
{ 0xDE,             0,             0,   0,   0  },
{ 0xDF,          0x77,          0x77,   1,   1  },
{ 0xE0,          0x88,          0x88,   0,   0  },
{ 0xE1,             0,             0,   0,   0  },
{ 0xE2,          0x44,          0x44,   0,   0  },
{ 0xE3,           0x3,           0x3,   1,   1  },
{ 0xE4,           0xf,           0xf,   1,   1  },
{ 0xE5,           0xf,           0xf,   1,   1  },
{ 0xE6,          0x44,          0x44,   0,   0  },
{ 0xE7,           0x1,           0x1,   0,   0  },
{ 0xE8,             0,             0,   0,   0  },
{ 0xE9,             0,             0,   0,   0  },
{ 0xEA,             0,             0,   0,   0  },
{ 0xEB,             0,             0,   0,   0  },
{ 0xEC,          0x77,          0x77,   1,   1  },
{ 0xED,          0x58,          0x58,   0,   0  },
{ 0xEE,          0x77,          0x77,   1,   1  },
{ 0xEF,          0x58,          0x58,   0,   0  },
{ 0xF0,             0,             0,   0,   0  },
{ 0xF1,             0,             0,   0,   0  },
{ 0xF2,          0x74,          0x74,   0,   0  },
{ 0xF3,           0x4,           0x4,   1,   1  },
{ 0xF4,          0xa2,          0xa2,   0,   0  },
{ 0xF5,          0xaa,          0xaa,   0,   0  },
{ 0xF6,          0x24,          0x24,   0,   0  },
{ 0xF7,          0xa9,          0xa9,   0,   0  },
{ 0xF8,          0x28,          0x28,   0,   0  },
{ 0xF9,          0x10,          0x10,   0,   0  },
{ 0xFA,          0x55,          0x55,   0,   0  },
{ 0xFB,          0x80,          0x80,   0,   0  },
{ 0xFC,          0x88,          0x88,   0,   0  },
{ 0xFD,          0x88,          0x88,   0,   0  },
{ 0xFE,          0x88,          0x88,   0,   0  },
{ 0xFF,          0x88,          0x88,   0,   0  },
{ 0x100,          0x80,          0x80,   0,   0  },
{ 0x101,          0x38,          0x38,   0,   0  },
{ 0x102,          0xb8,          0xb8,   0,   0  },
{ 0x103,          0x80,          0x80,   0,   0  },
{ 0x104,          0x38,          0x38,   0,   0  },
{ 0x105,          0x78,          0x78,   0,   0  },
{ 0x106,          0xc0,          0xc0,   0,   0  },
{ 0x107,           0x3,           0x3,   0,   0  },
{ 0x108,             0,             0,   0,   0  },
{ 0x109,             0,             0,   0,   0  },
{ 0x10A,             0,             0,   0,   0  },
{ 0x10B,             0,           0x4,   1,   0  },
{ 0x10C,           0xc,           0xc,   0,   0  },
{ 0x10D,             0,             0,   0,   0  },
{ 0x10E,             0,             0,   0,   0  },
{ 0x10F,          0x96,          0x96,   0,   0  },
{ 0x110,          0x77,          0x77,   0,   0  },
{ 0x111,          0x5a,          0x5a,   0,   0  },
{ 0x112,             0,             0,   0,   0  },
{ 0x113,             0,             0,   0,   0  },
{ 0x114,          0x21,          0x21,   0,   0  },
{ 0x115,          0x70,          0x70,   0,   0  },
{ 0x116,             0,             0,   0,   0  },
{ 0x117,             0,             0,   0,   0  },
{ 0x118,             0,             0,   0,   0  },
{ 0x119,          0xb3,          0xb3,   0,   0  },
{ 0x11A,          0x55,          0x55,   0,   0  },
{ 0x11B,           0xb,           0xb,   0,   0  },
{ 0x11C,             0,             0,   0,   0  },
{ 0x11D,           0x3,           0x3,   1,   1  },
{ 0x11E,             0,             0,   0,   0  },
{ 0x11F,          0xb3,          0xb3,   0,   0  },
{ 0x120,          0x55,          0x55,   0,   0  },
{ 0x121,          0x30,          0x30,   0,   0  },
{ 0x122,          0x46,          0x46,   0,   0  },
{ 0x123,             0,             0,   0,   0  },
{ 0x124,             0,             0,   0,   0  },
{ 0x125,             0,             0,   0,   0  },
{ 0x126,          0x21,          0x21,   0,   0  },
{ 0x127,          0x23,          0x23,   0,   0  },
{ 0x128,           0x2,           0x2,   0,   0  },
{ 0x129,             0,             0,   0,   0  },
{ 0x12A,             0,             0,   0,   0  },
{ 0x12B,           0x4,           0x4,   0,   0  },
{ 0x12C,             0,             0,   0,   0  },
{ 0x12D,           0x4,           0x4,   0,   0  },
{ 0x12E,             0,             0,   0,   0  },
{ 0x12F,             0,             0,   0,   0  },
{ 0x130,          0x1b,          0x1b,   0,   0  },
{ 0x131,          0x1b,          0x1b,   0,   0  },
{ 0x132,          0x1b,          0x1b,   0,   0  },
{ 0x133,          0x13,          0x13,   0,   0  },
{ 0x134,          0x13,          0x13,   0,   0  },
{ 0x135,           0xe,           0xe,   0,   0  },
{ 0x136,           0xe,           0xe,   0,   0  },
{ 0x137,             0,             0,   0,   0  },
{ 0x138,          0x11,          0x11,   0,   0  },
{ 0x139,          0x11,          0x11,   0,   0  },
{ 0xFFFF,             0,             0,   0,   0  }
};

typedef struct {
	uint8 chan;
	int8 c1;
	int8 c0;
} lpphy_rx_iqcomp_t;

/* Autogenerated by 2062_chantbl_rev0_tcl2c.tcl */
static const
lpphy_rx_iqcomp_t lpphy_rx_iqcomp_table_rev0[] = {
	{ 1, -64, 13 },
	{ 2, -64, 13 },
	{ 3, -64, 13 },
	{ 4, -64, 13 },
	{ 5, -64, 12 },
	{ 6, -64, 12 },
	{ 7, -64, 12 },
	{ 8, -64, 12 },
	{ 9, -64, 12 },
	{ 10, -64, 11 },
	{ 11, -64, 11 },
	{ 12, -64, 11 },
	{ 13, -64, 11 },
	{ 14, -64, 10 },
	{ 34, -62, 24 },
	{ 38, -62, 24 },
	{ 42, -62, 24 },
	{ 46, -62, 23 },
	{ 36, -62, 24 },
	{ 40, -62, 24 },
	{ 44, -62, 23 },
	{ 48, -62, 23 },
	{ 52, -62, 23 },
	{ 56, -62, 22 },
	{ 60, -62, 22 },
	{ 64, -62, 22 },
	{ 100, -62, 16 },
	{ 104, -62, 16 },
	{ 108, -62, 15 },
	{ 112, -62, 14 },
	{ 116, -62, 14 },
	{ 120, -62, 13 },
	{ 124, -62, 12 },
	{ 128, -62, 12 },
	{ 132, -62, 12 },
	{ 136, -62, 11 },
	{ 140, -62, 10 },
	{ 149, -61, 9 },
	{ 153, -61, 9 },
	{ 157, -61, 9 },
	{ 161, -61, 8 },
	{ 165, -61, 8 },
	{ 184, -62, 25 },
	{ 188, -62, 25 },
	{ 192, -62, 25 },
	{ 196, -62, 25 },
	{ 200, -62, 25 },
	{ 204, -62, 25 },
	{ 208, -62, 25 },
	{ 212, -62, 25 },
	{ 216, -62, 26 }
};

/* Autogenerated by 2063_chantbl_rev1_csm_tcl2c.tcl */
static const
lpphy_rx_iqcomp_t lpphy_rx_iqcomp_table_rev2[] = {
	{ 1, -64, 0 },
	{ 2, -64, 0 },
	{ 3, -64, 0 },
	{ 4, -64, 0 },
	{ 5, -64, 0 },
	{ 6, -64, 0 },
	{ 7, -64, 0 },
	{ 8, -64, 0 },
	{ 9, -64, 0 },
	{ 10, -64, 0 },
	{ 11, -64, 0 },
	{ 12, -64, 0 },
	{ 13, -64, 0 },
	{ 14, -64, 0 },
	{ 34, -64, 0 },
	{ 38, -64, 0 },
	{ 42, -64, 0 },
	{ 46, -64, 0 },
	{ 36, -64, 0 },
	{ 40, -64, 0 },
	{ 44, -64, 0 },
	{ 48, -64, 0 },
	{ 52, -64, 0 },
	{ 56, -64, 0 },
	{ 60, -64, 0 },
	{ 64, -64, 0 },
	{ 100, -64, 0 },
	{ 104, -64, 0 },
	{ 108, -64, 0 },
	{ 112, -64, 0 },
	{ 116, -64, 0 },
	{ 120, -64, 0 },
	{ 124, -64, 0 },
	{ 128, -64, 0 },
	{ 132, -64, 0 },
	{ 136, -64, 0 },
	{ 140, -64, 0 },
	{ 149, -64, 0 },
	{ 153, -64, 0 },
	{ 157, -64, 0 },
	{ 161, -64, 0 },
	{ 165, -64, 0 },
	{ 184, -64, 0 },
	{ 188, -64, 0 },
	{ 192, -64, 0 },
	{ 196, -64, 0 },
	{ 200, -64, 0 },
	{ 204, -64, 0 },
	{ 208, -64, 0 },
	{ 212, -64, 0 },
	{ 216, -64, 0 }
};

static const
lpphy_rx_iqcomp_t lpphy_rx_iqcomp_5354_table[] = {
	{ 1, -66, 15 },
	{ 2, -66, 15 },
	{ 3, -66, 15 },
	{ 4, -66, 15 },
	{ 5, -66, 15 },
	{ 6, -66, 15 },
	{ 7, -66, 14 },
	{ 8, -66, 14 },
	{ 9, -66, 14 },
	{ 10, -66, 14 },
	{ 11, -66, 14 },
	{ 12, -66, 13 },
	{ 13, -66, 13 },
	{ 14, -66, 13 }
};

static const
uint32 lpphy_rc_ideal_pwr[] = {
	65536,
	66903,
	69165,
	70624,
	69410,
	65380,
	60834,
	58836,
	61393,
	64488,
	47032,
	19253,
	6750,
	2571,
	1092,
	509,
	255,
	136,
	76,
	44,
	26
};

/* %%%%%% LPPHY specific function declaration */
static void wlc_phy_txpower_recalc_target2_lpphy(phy_info_t *pi);
static void wlc_lpphy_btcx_override_enable(phy_info_t *pi);
static void wlc_lpphy_aci_load_tbl(phy_info_t *pi, bool  aci);
static bool wlc_lpphy_aci_scan(phy_info_t *pi);

static uint32 wlc_lpphy_measure_digital_power(phy_info_t *pi, uint16 nsamples);
static uint32 wlc_lpphy_get_receive_power(phy_info_t *pi, int32 *gain_index);

static void wlc_lpphy_set_tx_gain_by_index(phy_info_t *pi, int indx);

static void wlc_lpphy_rc_cal(phy_info_t *pi);
static void wlc_lpphy_load_tx_gain_table(phy_info_t *pi, const lpphy_tx_gain_tbl_entry * gain_tbl);
static void wlc_lpphy_papd_decode_epsilon(uint32 epsilon, int32 *eps_real, int32 *eps_imag);
static void wlc_lpphy_tx_pwr_state_save(phy_info_t *pi, lpphy_tx_pwr_state *state);
static void wlc_lpphy_tx_pwr_state_restore(phy_info_t *pi, lpphy_tx_pwr_state *state);
static void wlc_lpphy_tx_filter_init(phy_info_t *pi);

static bool wlc_lpphy_rx_iq_est(phy_info_t *pi, uint16 n, uint8 wait_time, lpphy_iq_est_t *iq_est);
static void wlc_lpphy_rx_gain_override_enable(phy_info_t *pi, bool enable);
static void wlc_lpphy_set_rx_gain(phy_info_t *pi, uint32 gain);

static int wlc_lpphy_aux_adc_accum(phy_info_t *pi, uint32 num, uint32 wT, int32 *sum, int32 *prod);
static int wlc_lpphy_wait_phy_reg(phy_info_t *pi, uint16 addr, uint32 val, uint32, int, int us);

static void wlc_radio_2062_init(phy_info_t *pi);
static void wlc_radio_2063_init(phy_info_t *pi);
static void wlc_radio_2062_channel_tune(phy_info_t *pi, uint8 channel);
static void wlc_radio_2063_channel_tune(phy_info_t *pi, uint8 channel);

static void wlc_phy_tx_dig_filt_lpphy_rev2(phy_info_t *pi, bool bset, uint16 *coeffs);
static void wlc_phy_tx_dig_filt_ucode_set_lpphy_rev2(phy_info_t *pi, bool iscck, uint16 *coeffs);

static bool wlc_phy_txpwr_srom_read_lpphy(phy_info_t *pi);

#if defined(PHYCAL_CACHING) && defined(BCMDBG)
static void wlc_phy_cal_cache_dbg_lpphy(ch_calcache_t *ctx);
#endif

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  function implementation   					*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

/* %%%%%% LPPHY function */
static void
wlc_lpphy_set_tx_gain_override(phy_info_t *pi, bool bEnable)
{
	uint16 bit = bEnable ? 1 : 0;

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_MOD(pi, LPPHY_REV2, rfoverride2, txgainctrl_ovr, bit);
		PHY_REG_MOD(pi, LPPHY_REV2, rfoverride2, stxtxgainctrl_ovr, bit);
	} else {
		PHY_REG_MOD(pi, LPPHY, rfoverride2, txgainctrl_ovr, bit);
	}

	PHY_REG_MOD(pi, LPPHY, AfeCtrlOvr, dacattctrl_ovr, bit);
}

void
wlc_phy_tx_pwr_update_npt_lpphy(phy_info_t *pi)
{
	uint16 tx_cnt, tx_total, npt;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	ASSERT(wlc_phy_tpc_isenabled_lpphy(pi));

	tx_total = wlc_lpphy_total_tx_frames(pi);
	tx_cnt = tx_total - pi_lp->lpphy_tssi_tx_cnt;
	npt = wlc_lpphy_get_tx_pwr_npt(pi);

	if (tx_cnt > (1 << npt)) {
		/* Reset frame counter */
		pi_lp->lpphy_tssi_tx_cnt = tx_total;

		/* Set new NPT */
		if (npt < LPPHY_TX_PWR_CTRL_MAX_NPT) {
			npt++;
			wlc_lpphy_set_tx_pwr_npt(pi, npt);
		}

		/* Update cached power index & NPT */
		pi_lp->lpphy_tssi_idx = wlc_phy_get_current_tx_pwr_idx_lpphy(pi);
		pi_lp->lpphy_tssi_npt = npt;

		PHY_INFORM(("wl%d: %s: Index: %d, NPT: %d, TxCount: %d\n",
			pi->sh->unit, __FUNCTION__, pi_lp->lpphy_tssi_idx, npt, tx_cnt));
	}
}

static void
wlc_lpphy_clear_tx_power_offsets(phy_info_t *pi)
{
	uint32 data_buf[64];
	lpphytbl_info_t tab;

	/* Clear out buffer */
	bzero(data_buf, sizeof(data_buf));

	/* Preset txPwrCtrltbl */
	tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_phywidth = 32; /* phy width */
	tab.tbl_ptr = data_buf; /* ptr to buf */

	/* Per rate power offset */
	tab.tbl_len = 12; /* # values   */
	tab.tbl_offset = LPPHY_TX_PWR_CTRL_RATE_OFFSET;
	wlc_phy_table_write_lpphy(pi, &tab);

	/* Per index power offset */
	tab.tbl_len = 64; /* # values   */
	tab.tbl_offset = LPPHY_TX_PWR_CTRL_MAC_OFFSET;
	wlc_phy_table_write_lpphy(pi, &tab);
}

void
wlc_phy_set_tx_pwr_ctrl_lpphy(phy_info_t *pi, uint16 mode)
{
	uint16 old_mode = wlc_lpphy_get_tx_pwr_ctrl(pi);
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	ASSERT(
		(LPPHY_TX_PWR_CTRL_OFF == mode) ||
		(LPPHY_TX_PWR_CTRL_SW == mode) ||
		(LPPHY_TX_PWR_CTRL_HW == mode));

	if (old_mode != mode) {
		if (LPPHY_TX_PWR_CTRL_HW == old_mode) {
			/* Get latest power estimates before turning off power control */
			wlc_phy_tx_pwr_update_npt_lpphy(pi);

			/* Clear out all power offsets */
			wlc_lpphy_clear_tx_power_offsets(pi);
		} else if (LPPHY_TX_PWR_CTRL_HW == mode) {
			/* Recalculate target power to restore power offsets */
			wlc_phy_txpower_recalc_target2_lpphy(pi);

			/* Set starting index & NPT to best known values for that target */
			wlc_lpphy_set_start_tx_pwr_idx(pi, pi_lp->lpphy_tssi_idx);
			wlc_lpphy_set_tx_pwr_npt(pi, pi_lp->lpphy_tssi_npt);

			/* Reset frame counter for NPT calculations */
			pi_lp->lpphy_tssi_tx_cnt = wlc_lpphy_total_tx_frames(pi);

			/* Disable any gain overrides */
			wlc_lpphy_disable_tx_gain_override(pi);
			pi_lp->lpphy_tx_power_idx_override = -1;
		}

		if (pi_lp->lpphy_use_tx_pwr_ctrl_coeffs) {
			PHY_REG_MOD(pi, LPPHY, TxPwrCtrlCmd, use_txPwrCtrlCoefs,
				(LPPHY_TX_PWR_CTRL_HW == mode) ? 1 : 0);
		} else {
			PHY_REG_MOD(pi, LPPHY, TxPwrCtrlCmd, use_txPwrCtrlCoefs, 0);
		}

		/* Feed back RF power level to PAPD block */
		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_MOD(pi, LPPHY_REV2, papdctrl, rfpowerOverride,
				(LPPHY_TX_PWR_CTRL_HW == mode) ? 0 : 1);
		}

		/* Set requested tx power control mode */
		phy_reg_mod(pi, LPPHY_TxPwrCtrlCmd,
			(LPPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			LPPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK),
			mode);

		PHY_INFORM(("wl%d: %s: %s \n", pi->sh->unit, __FUNCTION__,
			mode ? ((LPPHY_TX_PWR_CTRL_HW == mode) ? "Auto" : "Manual") : "Off"));
	}
}

static int32
wlc_lpphy_tssi2dbm(int32 tssi, int32 a1, int32 b0, int32 b1)
{
	int32 a, b, p;

	a = 32768 + (a1 * tssi);
	b = (512 * b0) + (32 * b1 * tssi);
	p = ((2 * b) + a) / (2 * a);

	return p;
}

static void
wlc_lpphy_txpower_reset_npt(phy_info_t *pi)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
	if (CHSPEC_IS5G(pi->radio_chanspec))
		pi_lp->lpphy_tssi_idx = LPPHY_TX_PWR_CTRL_START_INDEX_5G;
	else if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)
		pi_lp->lpphy_tssi_idx = LPPHY_TX_PWR_CTRL_START_INDEX_2G_HGPA;
	else
		pi_lp->lpphy_tssi_idx = LPPHY_TX_PWR_CTRL_START_INDEX_2G;
	pi_lp->lpphy_tssi_npt = LPPHY_TX_PWR_CTRL_START_NPT;
}

static void
wlc_phy_txpower_recalc_target2_lpphy(phy_info_t *pi)
{
	lpphytbl_info_t tab;
	int32 a1 = 0, b0 = 0, b1 = 0;
	int32 tssi, pwr;
	uint freq;
#ifdef PPR_API
	uint32 rate_table[WL_RATESET_SZ_DSSS + WL_RATESET_SZ_OFDM];
#else
	uint32 rate_table[TXP_LAST_OFDM + 1];
#endif
	uint i;
	bool reset_npt = FALSE;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

#ifdef PPR_API
	ppr_dsss_rateset_t dsss_offsets;
	ppr_ofdm_rateset_t ofdm_offsets;

	if (pi->tx_power_offset == NULL)
		return;
	ppr_get_dsss(pi->tx_power_offset, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_offsets);
	ppr_get_ofdm(pi->tx_power_offset, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		&ofdm_offsets);
#endif

	/* Preset txPwrCtrltbl */
	tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_phywidth = 32; /* phy width */

	/* Adjust rate based power offset */
	for (i = 0; i < ARRAYSIZE(rate_table); i++) {
		/* The first 4 values belongs to CCK. If PAPD for CCK is disabled
		 * Then reduce extra 1.5 dB. (1.5dB means 6 as two fractional bits are
		 * used
		 */
#ifdef PPR_API
		if (i < WL_RATESET_SZ_DSSS) {
			if (pi_lp->lpphy_cck_papd_disabled)
				rate_table[i] =  (uint32)((int32)(-dsss_offsets.pwr[i] - 6));
			else
				rate_table[i] =  (uint32)((int32)(-dsss_offsets.pwr[i]));
		} else {
			rate_table[i] =
				(uint32)((int32)(-ofdm_offsets.pwr[i-WL_RATESET_SZ_DSSS]));
		}
#else
		if (pi_lp->lpphy_cck_papd_disabled && (i < 4)) {
			rate_table[i] =  (uint32)((int32)(-pi->tx_power_offset[i] - 6));
		} else {
			rate_table[i] =  (uint32)((int32)(-pi->tx_power_offset[i]));
		}
#endif /* PPR_API */
		PHY_TMP((" Rate %d, offset %d\n", i, rate_table[i]));
	}
	tab.tbl_len = ARRAYSIZE(rate_table); /* # values   */
	tab.tbl_ptr = rate_table; /* ptr to buf */
	tab.tbl_offset = LPPHY_TX_PWR_CTRL_RATE_OFFSET;
	wlc_phy_table_write_lpphy(pi, &tab);

	/* Adjust power LUT's */
	freq = wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec));
	if (pi_lp->lpphy_target_tx_freq != (uint16)freq) {
		switch (wlc_phy_chanspec_bandrange_get(pi, pi->radio_chanspec)) {
		case WL_CHAN_FREQ_RANGE_2G:
			/* 2.4 GHz */
			b0 = pi->txpa_2g[0];
			b1 = pi->txpa_2g[1];
			a1 = pi->txpa_2g[2];
			break;

		case WL_CHAN_FREQ_RANGE_5GL:
			/* 5 GHz low */
			b0 = pi->txpa_5g_low[0];
			b1 = pi->txpa_5g_low[1];
			a1 = pi->txpa_5g_low[2];
			break;

		case WL_CHAN_FREQ_RANGE_5GM:
			/* 5 GHz middle */
			b0 = pi->txpa_5g_mid[0];
			b1 = pi->txpa_5g_mid[1];
			a1 = pi->txpa_5g_mid[2];
			break;

		case WL_CHAN_FREQ_RANGE_5GH:
		default:
			/* 5 GHz high */
			b0 = pi->txpa_5g_hi[0];
			b1 = pi->txpa_5g_hi[1];
			a1 = pi->txpa_5g_hi[2];
			break;
		}

		/* Convert tssi to power LUT */
		tab.tbl_ptr = &pwr; /* ptr to buf */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_offset = 0; /* estPwrLuts */
		for (tssi = 0; tssi < 64; tssi++) {
			pwr = wlc_lpphy_tssi2dbm(tssi, a1, b0, b1);
			wlc_phy_table_write_lpphy(pi,  &tab);
			tab.tbl_offset++;
		}

		/* Save new target frequency */
		pi_lp->lpphy_target_tx_freq = (uint16)freq;
	}

	/* Set new target power */
#ifdef PPR_API
	if (wlc_lpphy_get_target_tx_pwr(pi) != wlc_phy_txpower_get_target_min((wlc_phy_t*)pi)) {
		wlc_lpphy_set_target_tx_pwr(pi, wlc_phy_txpower_get_target_min((wlc_phy_t*)pi));
#else
	if (wlc_lpphy_get_target_tx_pwr(pi) != pi->tx_power_min) {
		wlc_lpphy_set_target_tx_pwr(pi, pi->tx_power_min);
#endif
		/* Should reset power index cache */
		reset_npt = TRUE;
	}

	/* Invalidate power index and NPT if new power targets */
	if (reset_npt)
		wlc_lpphy_txpower_reset_npt(pi);
}

static void
wlc_lpphy_adjust_gain_tables(phy_info_t *pi, uint freq)
{
	uint16 tr_isolation;
	uint16 gain_codes[3];
	lpphytbl_info_t tab;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
	} else {
		/* Gain table (CCK & OFDM) */
		tab.tbl_width = 16;	/* 16 bit wide	*/
		tab.tbl_phywidth = 16; /* phy width */
		tab.tbl_len = 3;        /* # values   */
		tab.tbl_ptr = gain_codes; /* ptr to buf */
		tab.tbl_offset = 0;

		switch (wlc_phy_chanspec_freq2bandrange_lpssn(freq)) {
		case WL_CHAN_FREQ_RANGE_2G:
			/* 2.4 GHz */
			tr_isolation = pi_lp->lpphy_tr_isolation_2g;
			break;

		case WL_CHAN_FREQ_RANGE_5GL:
			/* 5 GHz low */
			tr_isolation = pi_lp->lpphy_tr_isolation_5g_low;
			break;

		case WL_CHAN_FREQ_RANGE_5GM:
			/* 5 GHz middle */
			tr_isolation = pi_lp->lpphy_tr_isolation_5g_mid;
			break;

		case WL_CHAN_FREQ_RANGE_5GH:
		default:
			/* 5 GHz high */
			tr_isolation = pi_lp->lpphy_tr_isolation_5g_hi;
			break;
		}

		/* Calculate new gains */
		gain_codes[0] = ((tr_isolation - 26) / 12) << 12;
		gain_codes[1] = gain_codes[0] + 0x1000;
		gain_codes[2] = gain_codes[1] + 0x1000;

		/* Update CCK gain table */
		tab.tbl_id = 0x0d;
		wlc_phy_table_write_lpphy(pi,  &tab);

		/* Update OFDM gain table */
		tab.tbl_id = 0x0c;
		wlc_phy_table_write_lpphy(pi,  &tab);
	}
}

static void
wlc_lpphy_set_trsw_override(phy_info_t *pi, bool tx, bool rx)
{
	/* Set TR switch */
	phy_reg_mod(pi, LPPHY_RFOverrideVal0,
		LPPHY_RFOverrideVal0_trsw_tx_pu_ovr_val_MASK |
		LPPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK,
		(tx ? LPPHY_RFOverrideVal0_trsw_tx_pu_ovr_val_MASK : 0) |
		(rx ? LPPHY_RFOverrideVal0_trsw_rx_pu_ovr_val_MASK : 0));

	/* Enable overrides */
	PHY_REG_OR(pi, LPPHY, RFOverride0,
		LPPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
		LPPHY_RFOverride0_trsw_rx_pu_ovr_MASK);
}

static void
wlc_lpphy_clear_trsw_override(phy_info_t *pi)
{
	/* Clear overrides */
	PHY_REG_AND(pi, LPPHY, RFOverride0,
		(uint16)~(LPPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
		LPPHY_RFOverride0_trsw_rx_pu_ovr_MASK));
}

static void
wlc_lpphy_set_pa_gain(phy_info_t *pi, uint16 gain)
{
	ASSERT(LPREV_GE(pi->pubpi.phy_rev, 2));

	PHY_REG_MOD(pi, LPPHY_REV2, txgainctrlovrval1, pagain_ovr_val1, gain);
	PHY_REG_MOD(pi, LPPHY_REV2, stxtxgainctrlovrval1, pagain_ovr_val1, gain);

}

static uint16
wlc_lpphy_get_pa_gain(phy_info_t *pi)
{
	uint16 pa_gain;

	ASSERT(LPREV_GE(pi->pubpi.phy_rev, 2));

	pa_gain = PHY_REG_READ(pi, LPPHY_REV2, txgainctrlovrval1, pagain_ovr_val1);

	return pa_gain;
}

static void
wlc_lpphy_set_dac_gain(phy_info_t *pi, uint16 dac_gain)
{
	uint16 dac_ctrl;

	dac_ctrl =
		((phy_reg_read(pi, LPPHY_AfeDACCtrl) >> LPPHY_AfeDACCtrl_dac_ctrl_SHIFT) & 0xc7f) |
		(dac_gain << 7);
	PHY_REG_MOD(pi, LPPHY, AfeDACCtrl, dac_ctrl, dac_ctrl);
}

static void
wlc_lpphy_set_tx_gain(phy_info_t *pi,  lpphy_txgains_t *target_gains)
{
	uint16 rf_gain;

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		uint16 pa_gain = wlc_lpphy_get_pa_gain(pi);

		PHY_REG_MOD(pi, LPPHY_REV2, txgainctrlovrval0, txgainctrl_ovr_val0,
			((target_gains->gm_gain) | (target_gains->pga_gain << 8)));
		PHY_REG_MOD(pi, LPPHY_REV2, txgainctrlovrval1, txgainctrl_ovr_val1,
			((target_gains->pad_gain) | (pa_gain << 8)));
		PHY_REG_MOD(pi, LPPHY_REV2, stxtxgainctrlovrval0, stxtxgainctrl_ovr_val0,
			((target_gains->gm_gain) | (target_gains->pga_gain << 8)));
		PHY_REG_MOD(pi, LPPHY_REV2, stxtxgainctrlovrval1, stxtxgainctrl_ovr_val1,
			((target_gains->pad_gain) | (pa_gain << 8)));

	} else {
		rf_gain =
			(target_gains->gm_gain) |
			(target_gains->pga_gain << 3) |
			(target_gains->pad_gain << 7);
		PHY_REG_MOD(pi, LPPHY, txgainctrlovrval, txgainctrl_ovr_val, rf_gain);
	}

	wlc_lpphy_set_dac_gain(pi, target_gains->dac_gain);

	/* Enable gain overrides */
	wlc_lpphy_enable_tx_gain_override(pi);
}

static void
wlc_lpphy_get_tx_gain(phy_info_t *pi,  lpphy_txgains_t *gains)
{
	uint16 dac_gain;

	dac_gain = phy_reg_read(pi, LPPHY_AfeDACCtrl) >>
		LPPHY_AfeDACCtrl_dac_ctrl_SHIFT;
	gains->dac_gain = (dac_gain & 0x380) >> 7;

	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
		uint16 rf_gain = PHY_REG_READ(pi, LPPHY, txgainctrlovrval, txgainctrl_ovr_val);

		gains->gm_gain = rf_gain & 0x7;
		gains->pga_gain = (rf_gain & 0x78) >> 3;
		gains->pad_gain = (rf_gain & 0x780) >> 7;
	} else {
		uint16 rfgain0, rfgain1;

		rfgain0 = PHY_REG_READ(pi, LPPHY_REV2, txgainctrlovrval0, txgainctrl_ovr_val0);
		rfgain1 = PHY_REG_READ(pi, LPPHY_REV2, txgainctrlovrval1, txgainctrl_ovr_val1);

		gains->gm_gain = rfgain0 & 0xff;
		gains->pga_gain = (rfgain0 >> 8) & 0xff;
		gains->pad_gain = rfgain1 & 0xff;
	}
}

static uint8
wlc_lpphy_get_bbmult(phy_info_t *pi)
{
	uint16 m0m1;
	lpphytbl_info_t tab;

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = 0;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	tab.tbl_phywidth = 16;	/* phy table element address space width */
	wlc_phy_table_read_lpphy(pi, &tab);

	return (uint8)((m0m1 & 0xff00) >> 8);
}

static void
wlc_lpphy_set_bbmult(phy_info_t *pi, uint8 m0)
{
	uint16 m0m1 = (uint16)m0 << 8;
	lpphytbl_info_t tab;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = 0;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	tab.tbl_phywidth = 16;	/* phy width */
	wlc_phy_table_write_lpphy(pi, &tab);
}

void
wlc_phy_get_tx_iqcc_lpphy(phy_info_t *pi, uint16 *a, uint16 *b)
{
	uint16 iqcc[2];
	lpphytbl_info_t tab;

	tab.tbl_ptr = iqcc; /* ptr to buf */
	tab.tbl_len = 2;        /* # values   */
	tab.tbl_id = 0;         /* iqloCaltbl      */
	tab.tbl_offset = 80; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	tab.tbl_phywidth = 16;	/* phy table element address space width */
	wlc_phy_table_read_lpphy(pi, &tab);

	*a = iqcc[0];
	*b = iqcc[1];
}

void
wlc_phy_set_tx_iqcc_lpphy(phy_info_t *pi, uint16 a, uint16 b)
{
	lpphytbl_info_t tab;
	uint16 iqcc[2];

	/* Fill buffer with coeffs */
	iqcc[0] = a;
	iqcc[1] = b;

	/* Update iqloCaltbl */
	tab.tbl_id = 0;			/* iqloCaltbl		*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_phywidth = 16; /* phy width */
	tab.tbl_ptr = iqcc;
	tab.tbl_len = 2;
	tab.tbl_offset = 80;
	wlc_phy_table_write_lpphy(pi, &tab);
}

uint16
wlc_phy_get_tx_locc_lpphy(phy_info_t *pi)
{
	lpphytbl_info_t tab;
	uint16 didq;

	/* Update iqloCaltbl */
	tab.tbl_id = 0;			/* iqloCaltbl		*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_phywidth = 16;	/* phy table element address space width */
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = 85;
	wlc_phy_table_read_lpphy(pi, &tab);

	return didq;
}

void
wlc_phy_set_tx_locc_lpphy(phy_info_t *pi, uint16 didq)
{
	lpphytbl_info_t tab;

	/* Update iqloCaltbl */
	tab.tbl_id = 0;			/* iqloCaltbl		*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_phywidth = 16; /* phy width */
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = 85;
	wlc_phy_table_write_lpphy(pi, &tab);
}

/* Set ucode ofdm/cck specific digital LOFT comp */
void
wlc_phy_set_tx_locc_ucode_lpphy(phy_info_t *pi, bool iscck, uint16 didq)
{
	uint16 addr;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (!pi_lp->lpphy_use_cck_dig_loft_coeffs)
		return;

	/* make sure ucode is suspended */
	ASSERT(!(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));

	if ((addr = 2*wlapi_bmac_read_shm(pi->sh->physhim, M_PHY_TX_FLT_PTR)) == 0)
		return;

	addr += 4*LPPHY_REV2_NUM_DIG_FILT_COEFFS;

	if (!iscck)
		addr += 2;

	wlapi_bmac_write_shm(pi->sh->physhim, addr, didq);
}

void
wlc_phy_get_radio_loft_lpphy(phy_info_t *pi, uint8 *ei0, uint8 *eq0, uint8 *fi0, uint8 *fq0)
{
	if (BCM2062_ID == LPPHY_RADIO_ID(pi)) {
		*ei0 = LPPHY_IQLOCC_READ(
			read_radio_reg(pi, RADIO_2062_TX_CTRL3_NORTH));
		*eq0 = LPPHY_IQLOCC_READ(
			read_radio_reg(pi, RADIO_2062_TX_CTRL4_NORTH));
		*fi0 = LPPHY_IQLOCC_READ(
			read_radio_reg(pi, RADIO_2062_TX_CTRL5_NORTH));
		*fq0 = LPPHY_IQLOCC_READ(
			read_radio_reg(pi, RADIO_2062_TX_CTRL6_NORTH));
	} else {
		*ei0 = LPPHY_IQLOCC_READ(
			read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I));
		*eq0 = LPPHY_IQLOCC_READ(
			read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q));
		*fi0 = LPPHY_IQLOCC_READ(
			read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I));
		*fq0 = LPPHY_IQLOCC_READ(
			read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q));
	}
}

static uint16
lpphy_iqlocc_write(phy_info_t *pi, uint8 data)
{
	int32 data32 = (int8)data;
	int32 rf_data32;

	if (BCM2062_ID == LPPHY_RADIO_ID(pi)) {
		if (data32 < 0) {
			rf_data32 = (-1 * (data32 & 0xf))<<4;
		} else {
			rf_data32 = (data32 & 0xf);
		}
	} else {
		int32 ip, in;
		ip = 8 + (data32 >> 1);
		in = 8 - ((data32+1) >> 1);
		rf_data32 = (in << 4) | ip;
	}

	return (uint16)(rf_data32);
}

void
wlc_phy_set_radio_loft_lpphy(phy_info_t *pi, uint8 ei0, uint8 eq0, uint8 fi0, uint8 fq0)
{
	if (BCM2062_ID == LPPHY_RADIO_ID(pi)) {
		write_radio_reg(pi, RADIO_2062_TX_CTRL3_NORTH, lpphy_iqlocc_write(pi, ei0));
		write_radio_reg(pi, RADIO_2062_TX_CTRL4_NORTH, lpphy_iqlocc_write(pi, eq0));
		write_radio_reg(pi, RADIO_2062_TX_CTRL5_NORTH, lpphy_iqlocc_write(pi, fi0));
		write_radio_reg(pi, RADIO_2062_TX_CTRL6_NORTH, lpphy_iqlocc_write(pi, fq0));
	} else {
		write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I, lpphy_iqlocc_write(pi, ei0));
		write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q, lpphy_iqlocc_write(pi, eq0));
		write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I, lpphy_iqlocc_write(pi, fi0));
		write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q, lpphy_iqlocc_write(pi, fq0));
	}
}


static const uint32 k_lpphy_aci_regs_rev3_extlna0_g[] = {
	LPPHY_ACI_DEF_REG(SyncPeakCnt, MaxPeakCntM1, 3, 7),
	LPPHY_ACI_DEF_REG(DSSSConfirmCnt, DSSSConfirmCntLoGain, 3, 4),
	LPPHY_ACI_DEF_REG(ClipCtrThresh, clipCtrThresh, 5, 19),
	LPPHY_ACI_DEF_REG(MinPwrLevel, ofdmMinPwrLevel, 8, -94),
	LPPHY_ACI_DEF_REG(MinPwrLevel, dsssMinPwrLevel, 8, -94),
	LPPHY_ACI_DEF_REG(InputPowerDB, inputpwroffsetdb, 8, -5),
	LPPHY_ACI_DEF_REG(ClipCtrThresh, clipCtrThreshLoGain, 5, 5),
	LPPHY_ACI_DEF_REG(ClipCtrThresh, clipCtrThresh, 5, 12),
	LPPHY_ACI_DEF_REG(VeryLowGainDB, veryLowGainDB, 8, 11),
	LPPHY_ACI_DEF_REG(crsgainCtrl, phycrsctrl, 4, 10),
	LPPHY_ACI_NULL
};
LPPHY_ACI_REG_SZ_CHK(k_lpphy_aci_regs_rev3_extlna0_g);

static const uint32 k_lpphy_aci_regs_rev3_extlna1_g[] = {
	LPPHY_ACI_DEF_REG(SyncPeakCnt, MaxPeakCntM1, 3, 7),
	LPPHY_ACI_DEF_REG(DSSSConfirmCnt, DSSSConfirmCntLoGain, 3, 4),
	LPPHY_ACI_DEF_REG(ClipCtrThresh, clipCtrThresh, 5, 19),
	LPPHY_ACI_DEF_REG(MinPwrLevel, ofdmMinPwrLevel, 8, -94),
	LPPHY_ACI_DEF_REG(MinPwrLevel, dsssMinPwrLevel, 8, -97),
	LPPHY_ACI_DEF_REG(InputPowerDB, inputpwroffsetdb, 8, -5),
	LPPHY_ACI_DEF_REG(ClipCtrThresh, clipCtrThreshLoGain, 5, 5),
	LPPHY_ACI_DEF_REG(VeryLowGainDB, veryLowGainDB, 8, 11),
	LPPHY_ACI_DEF_REG(crsgainCtrl, phycrsctrl, 4, 10),
	LPPHY_ACI_NULL
};
LPPHY_ACI_REG_SZ_CHK(k_lpphy_aci_regs_rev3_extlna1_g);

static const uint32 k_lpphy_aci_regs[] = {
	LPPHY_ACI_DEF_REG(DSSSConfirmCnt, DSSSConfirmCntLoGain, 3, 3),
	LPPHY_ACI_DEF_REG(ClipCtrThresh, clipCtrThresh, 5, 17),
	LPPHY_ACI_DEF_REG(crsgainCtrl, phycrsctrl, 4, 10),
	LPPHY_ACI_NULL
};
LPPHY_ACI_REG_SZ_CHK(k_lpphy_aci_regs);

static void
wlc_lpphy_aci_reset(phy_info_t *pi)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
	wlapi_bmac_write_shm(pi->sh->physhim, M_NOISE_IF_TIMEOUT,
		(uint16)pi_lp->lpphy_aci.glitch_timeout);
	wlapi_bmac_write_shm(pi->sh->physhim, M_NOISE_IF_COUNT, 0);
	pi_lp->lpphy_aci.glitch_cnt = 0;
}

void
wlc_phy_aci_init_lpphy(phy_info_t *pi, bool sys)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (sys) {
		const uint32 *p_aci_reg, *p_aci_reg_end;
		uint32 x;
		uint8 *p_dflt_reg, n, s;
		uint16 a;

		/* init samples */
		pi_lp->lpphy_noise_samples = LPPHY_NOISE_SAMPLES_DEFAULT;

		pi->aci_state = 0;

		pi_lp->lpphy_aci.chan_scan_cnt = 200;
		pi_lp->lpphy_aci.chan_scan_cnt_thresh = 40;
		pi_lp->lpphy_aci.chan_scan_pwr_thresh = 55;
		pi_lp->lpphy_aci.chan_scan_timeout = 60;
		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			pi_lp->lpphy_aci.on_thresh = 80;
			pi_lp->lpphy_aci.on_timeout = 20;
			pi_lp->lpphy_aci.off_thresh = 50;
			pi_lp->lpphy_aci.off_timeout = 20;
			pi_lp->lpphy_aci.glitch_timeout = 75;
		} else {
			pi_lp->lpphy_aci.on_thresh = 100;
			pi_lp->lpphy_aci.on_timeout = 1;
			pi_lp->lpphy_aci.off_thresh = 75;
			pi_lp->lpphy_aci.off_timeout = 60;
			pi_lp->lpphy_aci.glitch_timeout = 1000;
			pi_lp->lpphy_aci.chan_scan_timeout = 0;
		}

		/* Save init value of regs which will be modified by ACI tweaks */
		p_aci_reg = k_lpphy_aci_regs;
		if (LPREV_GE(pi->pubpi.phy_rev, 3) &&
		    (CHSPEC_IS2G(pi->radio_chanspec)) &&
		    (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {
			if (pi->u.pi_lpphy->extlna_type == 1)
				p_aci_reg = k_lpphy_aci_regs_rev3_extlna1_g;
			else
				p_aci_reg = k_lpphy_aci_regs_rev3_extlna0_g;
		}

		pi_lp->lpphy_aci.aci_reg = p_aci_reg;
		p_aci_reg_end = p_aci_reg + LPPHY_ACI_MAX_REGS;
		p_dflt_reg = pi_lp->lpphy_aci.dflt_reg;
		while (p_aci_reg < p_aci_reg_end) {
			if ((*p_aci_reg) == LPPHY_ACI_NULL)
				break;
			x = *p_aci_reg;
			a = (uint16)((x >> LPPHY_ACI_R_A_S) & LPPHY_ACI_R_A_M);
			s = (uint8)((x >> LPPHY_ACI_R_S_S) & LPPHY_ACI_R_S_M);
			n = (uint8)((x >> LPPHY_ACI_R_N_S) & LPPHY_ACI_R_N_M);
			*p_dflt_reg = (uint8)((phy_reg_read(pi, a) >> s) &
				((uint16)(1<<n)-1));
			p_aci_reg++;
			p_dflt_reg++;
		}
	}
	wlc_phy_aci_enable_lpphy(pi, FALSE);
	wlc_lpphy_aci_reset(pi);
	pi_lp->lpphy_aci.t = 0;
	pi_lp->lpphy_aci.t_scan = pi_lp->lpphy_aci.chan_scan_timeout;
	pi_lp->lpphy_aci.state = 0;
	pi_lp->lpphy_aci.last_chan = CHSPEC_CHANNEL(pi->radio_chanspec);
}

static void
wlc_lpphy_aci_tweaks(phy_info_t *pi, bool on)
{
	const uint32 *p_aci_reg, *p_aci_reg_end;
	uint32 x;
	uint8 *p_dflt_reg, n, s, v;
	uint16 a;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	wlc_lpphy_aci_load_tbl(pi, on);

	p_dflt_reg = pi_lp->lpphy_aci.dflt_reg;
	p_aci_reg = pi_lp->lpphy_aci.aci_reg;
	p_aci_reg_end = p_aci_reg + LPPHY_ACI_MAX_REGS;
	while (p_aci_reg < p_aci_reg_end) {
		if ((*p_aci_reg) == LPPHY_ACI_NULL)
			break;
		x = *p_aci_reg;
		a = (uint16)((x >> LPPHY_ACI_R_A_S) & LPPHY_ACI_R_A_M);
		s = (uint8)((x >> LPPHY_ACI_R_S_S) & LPPHY_ACI_R_S_M);
		n = (uint8)((x >> LPPHY_ACI_R_N_S) & LPPHY_ACI_R_N_M);
		phy_reg_and(pi, a, ~(((uint16)(1<<n) - 1) << s));
		if (on)
			v = (uint8)((x >> LPPHY_ACI_R_V_S) & LPPHY_ACI_R_V_M);
		else
			v = *p_dflt_reg;
		phy_reg_or(pi, a, ((uint16)v << s));
		p_aci_reg++;
		p_dflt_reg++;
	}

	PHY_REG_LIST_START
		PHY_REG_OR_ENTRY(LPPHY_REV2, resetCtrl,   0x44)
		PHY_REG_AND_ENTRY(LPPHY_REV2, resetCtrl, ~0x44)
	PHY_REG_LIST_EXECUTE(pi);

	wlc_lpphy_aci_reset(pi);
}

void
wlc_phy_aci_enable_lpphy(phy_info_t *pi, bool on)
{
	if (on && !(pi->aci_state & ACI_ACTIVE)) {
		PHY_CAL(("ACI ON\n"));
		pi->aci_state |= ACI_ACTIVE;

		wlc_lpphy_aci_tweaks(pi, on);

	} else if (!on && (pi->aci_state & ACI_ACTIVE)) {
		PHY_CAL(("ACI OFF\n"));
		pi->aci_state &= ~ACI_ACTIVE;

		wlc_lpphy_aci_tweaks(pi, on);
	}
}

static bool
wlc_lpphy_aci_scan(phy_info_t *pi)
{
	bool suspend;
	uint16 radar_en;
	int chan_mask = 0;
	int aci_mask = 0;
	int chan_current = CHSPEC_CHANNEL(pi->radio_chanspec);
	int chan, i, n;
	int pwr_metric;
	int pwr_metric_hist[LPPHY_ACI_CHAN_SCAN_DBG_BINS];
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

#ifdef LPPHY_ACI_SCAN_CHK_TIME
	uint32 t_l_0, t_l_1, t_h, t;
#endif

	if ((chan_current < ACI_FIRST_CHAN) || (chan_current > ACI_LAST_CHAN)) {
		/* Shouldn't be here */
		return FALSE;
	}

	/* Figure out which channels to scan... */
	if (chan_current >= LPPHY_ACI_CHAN_CENTER)
		chan_mask = (LPPHY_ACI_CHAN_SCAN_MASK <<
			(chan_current - LPPHY_ACI_CHAN_CENTER));
	else
		chan_mask = (LPPHY_ACI_CHAN_SCAN_MASK >>
			(LPPHY_ACI_CHAN_CENTER - chan_current));
	chan_mask &= LPPHY_ACI_CHAN_MASK;

	/* Set ACI in progress state. Make sure no return in between this and the reset of state. */
	wlc_phy_hold_upd((wlc_phy_t *)pi, PHY_HOLD_FOR_ACI_SCAN, TRUE);

	suspend = !(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		wlapi_suspend_mac_and_wait(pi->sh->physhim);

	wlc_lpphy_btcx_override_enable(pi);

	/* Save settings */
	radar_en = phy_reg_read(pi, LPPHY_REV2_RadardetectEn);


	chan = ACI_FIRST_CHAN - 1;
	while ((chan < ACI_LAST_CHAN) && !(chan_mask & aci_mask)) {

		chan++;
		if (((1<<chan) & chan_mask) == 0)
			continue;

		wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(chan));

		PHY_REG_LIST_START
			PHY_REG_OR_ENTRY(LPPHY_REV2, resetCtrl,   0x44)
			PHY_REG_AND_ENTRY(LPPHY_REV2, resetCtrl, ~0x44)
			/* Init stuff to read rx pwr metric from phy */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, RadardetectEn, radar_detect_enable, 1)
			PHY_REG_WRITE_ENTRY(LPPHY, gpioSel, 9)
			PHY_REG_WRITE_ENTRY(LPPHY, gpioOutEn, 0xffff)
		PHY_REG_LIST_EXECUTE(pi);

		/* Debug */
		for (i = 0; i < LPPHY_ACI_CHAN_SCAN_DBG_BINS; i++) {
			pwr_metric_hist[i] = 0;
		}

		for (i = 0, n = 0; i < pi_lp->lpphy_aci.chan_scan_cnt; i++) {

			/* Get pwr metric -- should be positive number */
			pwr_metric =
			  (phy_reg_read(pi, LPPHY_gpioOut) & 0xff);
			if (pwr_metric < 128) {
				pwr_metric = 0;
			} else {
				pwr_metric = (256 - pwr_metric);
			}

			/* Check thresholds */
			if (pwr_metric <= pi_lp->lpphy_aci.chan_scan_pwr_thresh) {
				n++;
				if (n >=  pi_lp->lpphy_aci.chan_scan_cnt_thresh) {
					aci_mask |= (1<<chan);
					break;
				}
			}

			/* Debug */
			if (pwr_metric > LPPHY_ACI_CHAN_SCAN_MAX_PWR) {
				pwr_metric = LPPHY_ACI_CHAN_SCAN_MAX_PWR;
			} else if (pwr_metric < 0) {
				pwr_metric = 0;
			}
			pwr_metric_hist[(pwr_metric >> LPPHY_ACI_CHAN_SCAN_DBG_SHFT)]++;

		}

		/* Print dbg stuff */
		PHY_CAL(("lpphy_aci_scan: %2d : %3d : ", chan, n));
		for (i = 0; i < LPPHY_ACI_CHAN_SCAN_DBG_BINS; i++) {
			PHY_CAL((" %3d,", pwr_metric_hist[i]));
		}
		PHY_CAL(("\n"));
	}


	/* Restore settings */
	PHY_REG_WRITE(pi, LPPHY_REV2, RadardetectEn, radar_en);
	wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(chan_current));

	/* Avoid triggering recal */
	pi->phy_forcecal = FALSE;

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

	/* ACI Scan is over */
	wlc_phy_hold_upd((wlc_phy_t *)pi, PHY_HOLD_FOR_ACI_SCAN, FALSE);

	return (chan_mask & aci_mask) ? TRUE : FALSE;
}

void
wlc_phy_aci_upd_lpphy(phy_info_t *pi)
{
	int cnt, delta, state, timeout, chan;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	cnt = (int)wlapi_bmac_read_shm(pi->sh->physhim, M_NOISE_IF_COUNT);
	delta = (cnt - pi_lp->lpphy_aci.glitch_cnt) & 0xffff;
	pi_lp->lpphy_aci.glitch_cnt = cnt;

	if (delta > pi_lp->lpphy_aci.on_thresh) {
		state = 1;
		timeout = pi_lp->lpphy_aci.on_timeout;
	} else if (delta < pi_lp->lpphy_aci.off_thresh) {
		state = -1;
		timeout = pi_lp->lpphy_aci.off_timeout;
	} else {
		state = 0;
		timeout = 0;
	}

	if (state != pi_lp->lpphy_aci.state)
		pi_lp->lpphy_aci.t = 0;

	pi_lp->lpphy_aci.state = state;

	chan = CHSPEC_CHANNEL(pi->radio_chanspec);
	if (chan != pi_lp->lpphy_aci.last_chan) {
		/* If there's a change in channel allow scan to happen right away */
		pi_lp->lpphy_aci.last_chan = chan;
		pi_lp->lpphy_aci.t = 0;
		pi_lp->lpphy_aci.t_scan = pi_lp->lpphy_aci.chan_scan_timeout;
	}

	if (pi_lp->lpphy_aci.t >= timeout) {
		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			if ((pi_lp->lpphy_aci.t_scan >=
			     pi_lp->lpphy_aci.chan_scan_timeout) &&
			    ((state > 0) ||
			     ((state < 0) && (pi->aci_state & ACI_ACTIVE))))
				{
			        /* Check pwr in adjacent channels periodically if
				   glitch count has been high for a while
				   Once glitch count has been low for a while
				   check pwr in adjacent channels periodically
				   as long as ACI tweaks are enabled.
				*/
				  bool aci_en;
				  aci_en = wlc_lpphy_aci_scan(pi);
				  pi_lp->lpphy_aci.t_scan = 0;
				  wlc_phy_aci_enable_lpphy(pi, aci_en);
				}
		} else {
			/* Enable ACI tweaks based on glitches only. */
			pi_lp->lpphy_aci.t_scan = 0;
			wlc_phy_aci_enable_lpphy(pi, (state > 0) ? TRUE : FALSE);
		}
	}

	PHY_CAL(("lpphy aci: %s %d %d %d %d,%d\n",
		((pi->aci_state & ACI_ACTIVE)?"ON":"OFF"), cnt, delta,
		pi_lp->lpphy_aci.t, pi_lp->lpphy_aci.t_scan, state));

	pi_lp->lpphy_aci.t++;
	pi_lp->lpphy_aci.t_scan++;

}

static uint32
wlc_lpphy_measure_digital_power(phy_info_t *pi, uint16 nsamples)
{
	lpphy_iq_est_t iq_est = {0, 0, 0};

	if (!wlc_lpphy_rx_iq_est(pi, nsamples, 32, &iq_est))
	    return 0;

	return (iq_est.i_pwr + iq_est.q_pwr) / nsamples;
}

static uint32
wlc_lpphy_get_receive_power(phy_info_t *pi, int32 *gain_index)
{
	uint32 received_power = 0;
	int32 max_index = 0;
	uint32 gain_code = 0;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		max_index = 36;
		if (*gain_index >= 0)
			gain_code = lpphy_rev2_gaincode_table[*gain_index];
	}
	else {
		lpphytbl_info_t tab;

		max_index = 33;
		if ((int32)(*gain_index) < 0)
			*gain_index = 20;

		/* Gain table */
		tab.tbl_id = 0x0c; /* OFDM gain */
		tab.tbl_width = 16;	/* 16 bit wide	*/
		tab.tbl_phywidth = 16;	/* phy table element address space width */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_ptr = &gain_code;
		tab.tbl_offset = *gain_index;

		/* Read gain from the table */
		wlc_phy_table_read_lpphy(pi,  &tab);
	}

	/* wlc_lpphy_set_deaf(pi); */
	if (*gain_index == -1) {
		*gain_index = 0;
		while ((*gain_index < (int32)max_index) && (received_power < 700)) {
			wlc_lpphy_set_rx_gain(pi, lpphy_rev2_gaincode_table[*gain_index]);
			received_power =
				wlc_lpphy_measure_digital_power(pi, pi_lp->lpphy_noise_samples);
			(*gain_index) ++;
		}
		(*gain_index) --;
	}
	else {
		wlc_lpphy_set_rx_gain(pi, gain_code);
		received_power = wlc_lpphy_measure_digital_power(pi, pi_lp->lpphy_noise_samples);
	}

	/* wlc_lpphy_clear_deaf(pi); */
	return received_power;
}

static uint32
wlc_calc_log(uint32 power)
{
	uint32 msb1, msb2, val1, val2, diff1, diff2;

	msb1 = find_msbit(power);
	msb2 = msb1 + 1;

	val1 = 1 << msb1;
	val2 = 1 << msb2;

	diff1 = (power - val1);
	diff2 = (val2 - power);

	if (diff1 < diff2)
		return msb1;
	else
		return msb2;
}

int32
wlc_phy_rx_signal_power_lpphy(phy_info_t *pi, int32 gain_index)
{
	uint32 gain = 0;
	uint32 nominal_power_db;
	uint32 log_val, gain_mismatch, desired_gain, input_power_offset_db, input_power_db;
	int32 received_power;

	received_power = wlc_lpphy_get_receive_power(pi, &gain_index);

	if (LPREV_GE(pi->pubpi.phy_rev, 2))
		gain = lpphy_rev2_gain_table[gain_index];
	else
		gain = 3*gain_index - 6;

	nominal_power_db = phy_reg_read(pi, LPPHY_VeryLowGainDB) >>
	                       LPPHY_VeryLowGainDB_NominalPwrDB_SHIFT;

	log_val = wlc_calc_log(received_power*16);
	log_val = log_val * 3;

	gain_mismatch = (nominal_power_db/2) - (log_val);

	desired_gain = gain + gain_mismatch;

	input_power_offset_db = phy_reg_read(pi, LPPHY_InputPowerDB) & 0xFF;

	if (input_power_offset_db > 127)
		input_power_offset_db -= 256;

	input_power_db = input_power_offset_db - desired_gain;

	wlc_lpphy_rx_gain_override_enable(pi, 0);

	return input_power_db;
}

static void
wlc_lpphy_rx_gain_override_enable(phy_info_t *pi, bool enable)
{
	uint16 ebit = enable ? 1 : 0;

	PHY_REG_MOD(pi, LPPHY, RFOverride0, trsw_rx_pu_ovr, ebit);
	PHY_REG_MOD(pi, LPPHY, RFOverride0, gmode_rx_pu_ovr, ebit);
	PHY_REG_MOD(pi, LPPHY, RFOverride0, amode_rx_pu_ovr, ebit);

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_MOD(pi, LPPHY_REV2, rfoverride2, rxgainctrl_ovr, ebit);
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			PHY_REG_MOD(pi, LPPHY_REV2, rfoverride2, slna_gain_ctrl_ovr, ebit);
			PHY_REG_MOD(pi, LPPHY_REV2, RFinputOverride, wlslnagainctrl_ovr, ebit);
		}
	} else {
		PHY_REG_MOD(pi, LPPHY, rfoverride2, rxgainctrl_ovr, ebit);
	}
}

static void
wlc_lpphy_set_rx_gain(phy_info_t *pi, uint32 gain)
{
	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		uint16 trsw, ext_lna, lna1, lna2, gain0_15, gain16_19;

		trsw = (gain & ((uint32)1 << 20)) ? 0 : 1;
		ext_lna = (uint16)(gain >> 21) & 0x01;
		lna1 = (uint16)(gain >> 2) & 0x03;
		lna2 = (uint16)(gain >> 6) & 0x03;
		BCM_REFERENCE(lna2);

		gain0_15 = (uint16)gain & 0xffff;
		gain16_19 = (uint16)(gain >> 16) & 0x0f;

		PHY_REG_MOD(pi, LPPHY_REV2, RFOverrideVal0, trsw_rx_pu_ovr_val, trsw);
		PHY_REG_MOD(pi, LPPHY_REV2, rfoverride2val, gmode_ext_lna_gain_ovr_val, ext_lna);
		PHY_REG_MOD(pi, LPPHY_REV2, rfoverride2val, amode_ext_lna_gain_ovr_val, ext_lna);
		PHY_REG_MOD(pi, LPPHY_REV2, rxgainctrl0ovrval, rxgainctrl_ovr_val0, gain0_15);
		PHY_REG_MOD(pi, LPPHY_REV2, rxlnaandgainctrl1ovrval, rxgainctrl_ovr_val1,
			gain16_19);

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			PHY_REG_MOD(pi, LPPHY_REV2, rfoverride2val, slna_gain_ctrl_ovr_val, lna1);
			PHY_REG_MOD(pi, LPPHY_REV2, RFinputOverrideVal, wlslnagainctrl_ovr_val,
				lna1);
		}
	} else {
		uint16 trsw, ext_lna, lna;

		trsw = (uint16)gain & 0x0001;
		ext_lna = (uint16)(gain & 0x0002) >> 1;
		lna = (uint16)((gain & 0xfffc) | ((gain & 0x000c) >> 2));

		PHY_REG_MOD(pi, LPPHY, RFOverrideVal0, trsw_rx_pu_ovr_val, trsw);
		PHY_REG_MOD(pi, LPPHY, rfoverride2val, gmode_ext_lna_gain_ovr_val, ext_lna);
		PHY_REG_MOD(pi, LPPHY, rfoverride2val, amode_ext_lna_gain_ovr_val, ext_lna);
		PHY_REG_MOD(pi, LPPHY, rxgainctrlovrval, rxgainctrl_ovr_val, lna);
	}

	wlc_lpphy_rx_gain_override_enable(pi, TRUE);
}

static void
wlc_lpphy_set_tx_filter_bw(phy_info_t *pi, uint16 bw)
{
	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		uint8 idac_setting;

		/* cck/all non-ofdm setting */
		PHY_REG_MOD(pi, LPPHY_REV2, lpfbwlutreg0, lpfbwlut0, bw);
		/* ofdm setting */
		PHY_REG_MOD(pi, LPPHY_REV2, lpfbwlutreg1, lpfbwlut5, bw);

		if (LPREV_IS(pi->pubpi.phy_rev, 2)) {
			if (bw <= 1)
				idac_setting = 0x0e;
			else if (bw <= 3)
				idac_setting = 0x13;
			else
				idac_setting = 0x1b;
			mod_radio_reg(pi, RADIO_2063_TXBB_CTRL_2, 0x1f, idac_setting);
		}
	}
}

static void
wlc_lpphy_set_rx_gain_by_index(phy_info_t *pi, uint16 indx)
{
	lpphytbl_info_t tab;
	uint16 gain;

	/* Gain table */
	tab.tbl_id = 0x0c; /* OFDM gain */
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_phywidth = 16;	/* phy table element address space width */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_ptr = &gain; /* ptr to buf */
	tab.tbl_offset = indx;

	/* Read gain from the table */
	wlc_phy_table_read_lpphy(pi,  &tab);
	/* Apply gain override */
	wlc_lpphy_set_rx_gain(pi, gain);
}

static void
wlc_lpphy_set_tx_gain_by_index(phy_info_t *pi, int indx)
{
	lpphytbl_info_t tab;
	uint16 rf_gain, dac_gain, a, b;
	uint8 bb_mult;
	uint32 bbmultiqcomp, txgain, locoeffs, rfpower;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	/* Preset txPwrCtrltbl */
	tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_phywidth = 32;	/* phy table element address space width */
	tab.tbl_len = 1;        /* # values   */

	/* Read index based bb_mult, a, b from the table */
	tab.tbl_offset = LPPHY_TX_PWR_CTRL_IQ_OFFSET + indx; /* iqCoefLuts */
	tab.tbl_ptr = &bbmultiqcomp; /* ptr to buf */
	wlc_phy_table_read_lpphy(pi,  &tab);

	/* Read index based tx gain from the table */
	tab.tbl_offset = LPPHY_TX_PWR_CTRL_GAIN_OFFSET + indx; /* gainCtrlLuts */
	tab.tbl_ptr = &txgain; /* ptr to buf */
	wlc_phy_table_read_lpphy(pi,  &tab);

	/* Apply tx gain */
	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
		rf_gain = (uint16)((txgain >> 4) & 0x7ff);
		dac_gain = (uint16)(txgain & 0x7);
		PHY_REG_MOD(pi, LPPHY, txgainctrlovrval, txgainctrl_ovr_val, rf_gain);
		wlc_lpphy_set_dac_gain(pi, dac_gain);
	} else {
		lpphy_txgains_t gains;

		gains.gm_gain = (uint16)(txgain & 0xff);
		gains.pga_gain = (uint16)(txgain >> 8) & 0xff;
		gains.pad_gain = (uint16)(txgain >> 16) & 0xff;
		gains.dac_gain = (uint16)(bbmultiqcomp >> 28) & 0x07;

		wlc_lpphy_set_tx_gain(pi, &gains);
		wlc_lpphy_set_pa_gain(pi,  (uint16)(txgain >> 24) & 0x7f);
	}

	/* Apply bb_mult */
	bb_mult = (uint8)((bbmultiqcomp >> 20) & 0xff);
	wlc_lpphy_set_bbmult(pi, bb_mult);

	if (pi_lp->lpphy_use_tx_pwr_ctrl_coeffs) {
		/* Apply iqcc */
		a = (uint16)((bbmultiqcomp >> 10) & 0x3ff);
		b = (uint16)(bbmultiqcomp & 0x3ff);
		wlc_phy_set_tx_iqcc_lpphy(pi, a, b);

		/* Read index based di & dq from the table */
		tab.tbl_offset = LPPHY_TX_PWR_CTRL_LO_OFFSET + indx; /* loftCoefLuts */
		tab.tbl_ptr = &locoeffs; /* ptr to buf */
		wlc_phy_table_read_lpphy(pi,  &tab);

		/* Apply locc */
		wlc_phy_set_tx_locc_lpphy(pi, (uint16)locoeffs);
	}

	/* Apply PAPD rf power correction */
	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		tab.tbl_offset = LPPHY_TX_PWR_CTRL_PWR_OFFSET + indx;
		tab.tbl_ptr = &rfpower; /* ptr to buf */
		wlc_phy_table_read_lpphy(pi,  &tab);

		PHY_REG_MOD(pi, LPPHY_REV2, papdrfpowerOvrVal, rfpowerOvrVal, rfpower);
	}

	/* Enable gain overrides */
	wlc_lpphy_enable_tx_gain_override(pi);
}

void
wlc_phy_set_tx_pwr_by_index_lpphy(phy_info_t *pi, int indx)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	ASSERT(indx <= LPPHY_MAX_TX_POWER_INDEX);

	/* Save forced index */
	pi_lp->lpphy_tx_power_idx_override = (int8)indx;

	/* Turn off automatic power control */
	if (wlc_lpphy_tssi_enabled(pi))
		wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_SW);

	wlc_lpphy_set_tx_gain_by_index(pi, indx);
}

static void
wlc_lpphy_set_rc_cap(phy_info_t *pi, uint8 rc_cap)
{
	uint8 tx_filt_setting;
	bool wide_filter;

	PHY_INFORM(("wl%d: %s: RC Cap: 0x%x\n",
		pi->sh->unit, __FUNCTION__, rc_cap));

	/* Set the Rx filter Cap */
	write_radio_reg(pi, RADIO_2062_RXBB_CAL2_NORTH, MAX(0x80, rc_cap - 4));
	/* Set the Tx filter Cap */
	tx_filt_setting = (rc_cap & 0x1f) >> 1;
	if (LPREV_IS(pi->pubpi.phy_rev, 1)) {
		if (CHSPEC_CHANNEL(pi->radio_chanspec) != 14) {
			wide_filter = FALSE;
		} else {
			wide_filter = pi->u.pi_lpphy->japan_wide_filter;
		}

		if (!wide_filter) {
			tx_filt_setting += 5;
			tx_filt_setting = MIN(0xf, tx_filt_setting);
			}
	}
	write_radio_reg(pi, RADIO_2062_TX_CTRLA_NORTH, (tx_filt_setting | 0x80));
	/* Set the Phase Shifter Cap */
	write_radio_reg(pi,  RADIO_2062_RXG_CTR16_SOUTH, ((rc_cap & 0x1f) >> 2) | 0x80);
}

static void
wlc_lpphy_btcx_override_enable(phy_info_t *pi)
{
	if (LPREV_GE(pi->pubpi.phy_rev, 2) && (pi->sh->machwcap & MCAP_BTCX)) {
		/* Ucode better be suspended when we mess with BTCX regs directly */
		ASSERT(!(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));

		/* Enable manual BTCX mode */
		W_REG(pi->sh->osh, &pi->regs->btcx_ctrl, 0x03);
		/* Force WLAN priority */
		W_REG(pi->sh->osh, &pi->regs->btcx_trans_ctrl, 0xff);
	}
}

#if defined(BCMDBG) || defined(WLTEST)
static void
wlc_lpphy_full_cal(phy_info_t *pi)
{
	/* Force full calibration run */
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	ASSERT(ctx);
	ctx->valid = FALSE;
#else
	pi->u.pi_lpphy->lpphy_full_cal_channel = 0;
#endif

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Run lpphy cals */
	wlc_phy_periodic_cal_lpphy(pi);
}
#endif 

void
wlc_phy_get_tssi_lpphy(phy_info_t *pi, int8 *ofdm_pwr, int8 *cck_pwr)
{
	int8 cck_offset;
	uint16 status;

	if (wlc_lpphy_tssi_enabled(pi) &&
		((status = (phy_reg_read(pi, LPPHY_TxPwrCtrlStatus))) &
		LPPHY_TxPwrCtrlStatus_estPwrValid_MASK)) {
		*ofdm_pwr = (int8)((status &
			LPPHY_TxPwrCtrlStatus_estPwr_MASK) >>
			LPPHY_TxPwrCtrlStatus_estPwr_SHIFT);
		if (wlc_phy_tpc_isenabled_lpphy(pi)) {
#ifdef PPR_API
			ppr_dsss_rateset_t dsss_offsets;
			ppr_get_dsss(pi->tx_power_offset,
				WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_offsets);
			cck_offset = dsss_offsets.pwr[0];
#else
			cck_offset = pi->tx_power_offset[TXP_FIRST_CCK];
#endif /* PPR_API */
		} else {
			cck_offset = 0;
		}
		*cck_pwr = *ofdm_pwr + cck_offset;
	} else {
		*ofdm_pwr = 0;
		*cck_pwr = 0;
	}
}


/* initialize all the tables defined in auto-generated lpphytbls.c,
 * see lpphyprocs.tcl, proc lpphy_tbl_init
 */
static void
wlc_lpphy_aci_load_tbl(phy_info_t *pi, bool  aci)
{
	uint idx;
	const dot11lpphytbl_info_t *dot11lpphytbl_info;
	uint32 tbl_info_sz = 0;

	dot11lpphytbl_info = NULL;

	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {

		/* ??? not implemented */

	} else if (LPREV_IS(pi->pubpi.phy_rev, 2)) {
		/* BCM4325 A0,A1: no ACI support */
		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
			(CHSPEC_IS2G(pi->radio_chanspec))) {
			/* Only 1 board supports BCM4325A0/1 w/ ext LNA */
			dot11lpphytbl_info = dot11lpphytbl_rx_gain_ext_lna_g_info_rev2;
			tbl_info_sz = dot11lpphytbl_rx_gain_ext_lna_g_info_sz_rev2;
		} else {
			dot11lpphytbl_info = dot11lpphytbl_rx_gain_info_rev2;
			tbl_info_sz = dot11lpphytbl_rx_gain_info_sz_rev2;
		}
	} else if (LPREV_GE(pi->pubpi.phy_rev, 3)) {
		/* BCM4325 B0 and above */
		if (aci) {
			if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
			    (CHSPEC_IS2G(pi->radio_chanspec))) {

				dot11lpphytbl_info = dot11lpphytbl_rx_gain_ext_lna_g_aci_info_rev3;
				tbl_info_sz = dot11lpphytbl_rx_gain_ext_lna_g_aci_info_sz_rev3;
			} else {
				dot11lpphytbl_info =  dot11lpphytbl_rx_gain_aci_info_rev3;
				tbl_info_sz = dot11lpphytbl_rx_gain_aci_info_sz_rev3;
			}
		} else {
			if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
			    (CHSPEC_IS2G(pi->radio_chanspec))) {
				dot11lpphytbl_info = dot11lpphytbl_rx_gain_ext_lna_g_info_rev3;
				tbl_info_sz = dot11lpphytbl_rx_gain_ext_lna_g_info_sz_rev3;
			} else {
				dot11lpphytbl_info =  dot11lpphytbl_rx_gain_info_rev3;
				tbl_info_sz = dot11lpphytbl_rx_gain_info_sz_rev3;
			}
		}
	} else {
		PHY_ERROR(("wlc_lpphy_aci_load_tbl, lpphy rev %d unsupported!\n",
		          pi->pubpi.phy_rev));
		ASSERT(0);
	}

	if (dot11lpphytbl_info) {
		for (idx = 0; idx < tbl_info_sz; idx++) {
			wlc_phy_table_write_lpphy(pi, &dot11lpphytbl_info[idx]);
		}
	}

}

static void
WLBANDINITFN(wlc_lpphy_tbl_init)(phy_info_t *pi)
{
	uint idx;
	lpphytbl_info_t tab;
	uint32 j, val;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		const dot11lpphytbl_info_t *phytbl_info;
		uint32 tbl_info_sz;
		const lpphy_tx_gain_tbl_entry *p2GHz_gaintable;
		const lpphy_tx_gain_tbl_entry *p5GHz_gaintable;
		const lpphy_tx_gain_tbl_entry *pNoPA_gaintable;

		if (LPREV_GE(pi->pubpi.phy_rev, 3)) {
			phytbl_info = dot11lpphytbl_info_rev3;
			tbl_info_sz = (uint32)dot11lpphytbl_info_sz_rev3;
			p2GHz_gaintable = dot11lpphy_2GHz_gaintable_rev3;
			p5GHz_gaintable = dot11lpphy_5GHz_gaintable_rev3;
			pNoPA_gaintable = dot11lpphy_5GHz_gaintable_rev3;
		} else {
			phytbl_info = dot11lpphytbl_info_rev2;
			tbl_info_sz = (uint32)dot11lpphytbl_info_sz_rev2;
			p2GHz_gaintable = dot11lpphy_2GHz_gaintable_rev2;
			p5GHz_gaintable = dot11lpphy_5GHz_gaintable_rev2;
			pNoPA_gaintable = dot11lpphy_5GHz_gaintable_rev2;
		}

		/* Clear tx power control table */
		tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;	/* 32 bit wide	*/
		tab.tbl_phywidth = 32; /* phy width */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_ptr = &val; /* ptr to buf */
		val = 0;
		for (tab.tbl_offset = 0; tab.tbl_offset < 704; tab.tbl_offset++) {
			wlc_phy_table_write_lpphy(pi, &tab);
		}

		/* Load lpphy init tables */
		for (idx = 0; idx < tbl_info_sz; idx++) {
			wlc_phy_table_write_lpphy(pi, &phytbl_info[idx]);
		}

		/* Overwrite the initially loaded switch control table with
		 * BCM4315 switch control table if the chip is BCM4315.
		 */
		if ((CHIPID(pi->sh->chip) == BCM4315_CHIP_ID) &&
		    (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_FEM)) {
			if ((BOARDTYPE(pi->sh->boardtype) == BCM94315DEVBU_SSID) ||
				(BOARDTYPE(pi->sh->boardtype) == BCM94315BGABU_SSID)) {
				wlc_phy_table_write_lpphy(pi,
					&dot11lpphytbl_bringup_board_4315_sw_ctrl_info_rev3[0]);
			}
			else if ((BOARDTYPE(pi->sh->boardtype) == BCM94315USBGP_SSID) ||
				(BOARDTYPE(pi->sh->boardtype) == BCM94315USBGP41_SSID)) {
				wlc_phy_table_write_lpphy(pi,
					&dot11lpphytbl_ref_board_4315_sw_ctrl_info_rev3[0]);
			}
		}

		if (BOARDTYPE(pi->sh->boardtype) == BCM94325SDABGWBA_BOARD) {
			/* apple X7 board. Load the switch control table for this board */
			wlc_phy_table_write_lpphy(pi,
				&dot11lpphytbl_x7_board_4325_sw_ctrl_info_rev3[0]);
		}

		/* Load regular rx gain tables */
		wlc_lpphy_aci_load_tbl(pi, FALSE);

		/* Load tx gain tables */
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_NOPA) {
			wlc_lpphy_load_tx_gain_table(pi, pNoPA_gaintable);
		} else if (CHSPEC_IS5G(pi->radio_chanspec)) {
			wlc_lpphy_load_tx_gain_table(pi, p5GHz_gaintable);
		} else {
			wlc_lpphy_load_tx_gain_table(pi, p2GHz_gaintable);
		}

		/* Load papd rf power lut portion of txpwrCtrltbl */
		for (j = 0; j < 128; j ++) {
			tab.tbl_offset = LPPHY_TX_PWR_CTRL_PWR_OFFSET + j;
			val = 127 - j;
			wlc_phy_table_write_lpphy(pi, &tab);
		}
	} else if (LPREV_GE(pi->pubpi.phy_rev, 1)) {
		for (idx = 0; idx < dot11lpphytbl_info_sz_rev1; idx++) {
			wlc_phy_table_write_lpphy(pi, &dot11lpphytbl_info_rev1[idx]);
		}

		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_NOPA) ||
			(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
			wlc_lpphy_load_tx_gain_table(pi, dot11lpphy_noPA_gaintable_rev1);
		} else if (CHSPEC_IS5G(pi->radio_chanspec)) {
			wlc_lpphy_load_tx_gain_table(pi, dot11lpphy_5GHz_gaintable_rev1);
		} else {
			wlc_lpphy_load_tx_gain_table(pi, dot11lpphy_2GHz_gaintable_rev1);
		}
	} else {
		for (idx = 0; idx < dot11lpphytbl_info_sz_rev0; idx++) {
			wlc_phy_table_write_lpphy(pi, &dot11lpphytbl_info_rev0[idx]);
		}

		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) {
			for (idx = 0; idx < dot11lpphytbl_ext_lna_info_sz_rev0; idx++) {
				wlc_phy_table_write_lpphy(pi,
					&dot11lpphytbl_ext_lna_info_rev0[idx]);
			}
		}

		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_NOPA) ||
			(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
			if ((CHIPID(pi->sh->chip) == BCM5354_CHIP_ID) &&
				(pi->sh->chiprev >= 3)) {
				wlc_lpphy_load_tx_gain_table(pi,
					dot11lpphy_noPA_gaintable_5354smic_rev0);
			} else {
				wlc_lpphy_load_tx_gain_table(pi, dot11lpphy_noPA_gaintable_rev0);
			}
		} else if (CHSPEC_IS5G(pi->radio_chanspec)) {
			wlc_lpphy_load_tx_gain_table(pi, dot11lpphy_5GHz_gaintable_rev0);
		} else {
			wlc_lpphy_load_tx_gain_table(pi, dot11lpphy_2GHz_gaintable_rev0);
		}
	}

	/* Channel specific gain tables */
	wlc_lpphy_adjust_gain_tables(pi,
		wlc_phy_channel2freq(CHSPEC_CHANNEL(pi->radio_chanspec)));
}

static void
WLBANDINITFN(wlc_lpphy_load_tx_gain_table)(phy_info_t *pi,
	const lpphy_tx_gain_tbl_entry * gain_table)
{
	int j;
	lpphytbl_info_t tab;
	uint32 val;
	uint16 pa_gain = 0;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_phywidth = 32; /* phy width */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_ptr = &val; /* ptr to buf */

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		if (LPREV_GE(pi->pubpi.phy_rev, 3)) {
			if (CHSPEC_IS5G(pi->radio_chanspec))
				pa_gain = 0x10;
			else
				pa_gain = 0x70;
		} else {
			if (CHSPEC_IS5G(pi->radio_chanspec))
				pa_gain = 20;
			else
				pa_gain = 127;
		}
	}

	for (j = 0; j < 128; j++) {
		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			val = ((uint32)pa_gain << 24) |
				(gain_table[j].pad << 16) |
				(gain_table[j].pga << 8) |
				(gain_table[j].gm << 0);
			tab.tbl_offset = LPPHY_TX_PWR_CTRL_GAIN_OFFSET + j;
			wlc_phy_table_write_lpphy(pi, &tab);

			val = (gain_table[j].dac << 28) |
				(gain_table[j].bb_mult << 20);
			tab.tbl_offset = LPPHY_TX_PWR_CTRL_IQ_OFFSET + j;
			wlc_phy_table_write_lpphy(pi, &tab);
		} else {
			val = (gain_table[j].pad << 11) |
				(gain_table[j].pga << 7) |
				(gain_table[j].gm << 4) |
				(gain_table[j].dac);
			tab.tbl_offset = LPPHY_TX_PWR_CTRL_GAIN_OFFSET + j;
			wlc_phy_table_write_lpphy(pi, &tab);

			val = gain_table[j].bb_mult << 20;
			tab.tbl_offset = LPPHY_TX_PWR_CTRL_IQ_OFFSET + j;
			wlc_phy_table_write_lpphy(pi, &tab);
		}
	}
}

typedef enum {
	LPPHY_TSSI_PRE_PA,
	LPPHY_TSSI_POST_PA,
	LPPHY_TSSI_EXT
} lpphy_tssi_mode_t;

static void
wlc_lpphy_set_tssi_mux(phy_info_t *pi, lpphy_tssi_mode_t pos)
{
	if (LPPHY_TSSI_EXT == pos) {
		/* Not supported on current design */
		ASSERT(FALSE);
	} else {
		/* Power up internal TSSI */
		or_radio_reg(pi, RADIO_2063_PA_SP_1, 0x01 << 1);
		PHY_REG_OR(pi, LPPHY_REV2, extstxctrl1, 0x01 << 12);

		/* Adjust TSSI bias */
		write_radio_reg(pi, RADIO_2063_PA_CTRL_10, 0x51);

		/* Set TSSI/RSSI mux */
		if (LPPHY_TSSI_POST_PA == pos) {
			mod_radio_reg(pi, RADIO_2063_PA_SP_1, 0x01, 0);
			PHY_REG_MOD(pi, LPPHY_REV2, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 0x00);
		} else {
			mod_radio_reg(pi, RADIO_2063_PA_SP_1, 0x01, 1);
			PHY_REG_MOD(pi, LPPHY_REV2, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 0x04);
		}
	}
}

static void
wlc_lpphy_load_estpwrlut_sequential_values(phy_info_t *pi, uint16 start_value, uint16 increment)
{
	lpphytbl_info_t tab;
	uint32 ind;
	uint32 value;

	value = start_value;

	tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_phywidth = 32; /* addressing width in the phy */
	tab.tbl_ptr = &value; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_offset = 0; /* estPwrLuts */
	for (ind = 0; ind < 64; ind++) {
		wlc_phy_table_write_lpphy(pi,  &tab);
		tab.tbl_offset++;
		value = value + increment;
	}
}

static void
wlc_lpphy_compute_idle_tssi(phy_info_t *pi, int save_restore_flag)
{
	uint16 tssi_val, status, save_pagain = 0;
	uint16 auxadc_ctrl_old = 0;
	uint8 iqcal_ctrl2_old = 0;
	uint16 internalrftxpu_ovr_old, internalrftxpu_ovr_val_old;
	lpphy_tx_pwr_state tx_pwr_state;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if ((!(NORADIO_ENAB(pi->pubpi))) && pi->hwpwrctrl_capable) {

		/* Save tx power control mode */
		wlc_lpphy_tx_pwr_state_save(pi, &tx_pwr_state);

		wlc_lpphy_load_estpwrlut_sequential_values(pi, 0, 1);

		PHY_REG_LIST_START
			/* The below two register fields were used only here.
			 * So they are just modified without saving and restoring.
			 */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, TxPwrCtrlNnum, Ntssi_delay, 255)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, TxPwrCtrlNnum, Ntssi_intg_log2, 6)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, TxPwrCtrlIdleTssi, idleTssi0, 0x1f)
		PHY_REG_LIST_EXECUTE(pi);

		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			uint8 iqcal_ctrl2;
			/* save the aux adc settings */
			auxadc_ctrl_old = phy_reg_read(pi, LPPHY_REV2_auxadcCtrl);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, rssifiltEn, 0)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, rssiformatConvEn, 1)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, txpwrctrlEn, 1)
			PHY_REG_LIST_EXECUTE(pi);

			/* save the pa gain */
			save_pagain = wlc_lpphy_get_pa_gain(pi);

			/* Set IQCAL mux to TSSI */
			iqcal_ctrl2 = (uint8)read_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2);
			iqcal_ctrl2_old = iqcal_ctrl2;
			iqcal_ctrl2 &= (uint8)~(0x0c);
			iqcal_ctrl2 |= 0x01;
			write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, iqcal_ctrl2);

			/* Use PA output for TSSI */
			wlc_lpphy_set_tssi_mux(pi, LPPHY_TSSI_POST_PA);
		} else {
			/* Set TSSI mux */
			if (LPREV_IS(pi->pubpi.phy_rev, 1)) {
				phy_reg_mod(pi, LPPHY_REV1_AfeRSSISel,
					LPPHY_REV1_AfeRSSISel_grssi_sel_MASK |
					LPPHY_REV1_AfeRSSISel_grssi_sel_ovr_MASK,
					LPPHY_REV1_AfeRSSISel_grssi_sel_ovr_MASK);
			}
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(LPPHY, lpphyCtrl, rssifiltEn, 0)
				PHY_REG_MOD_ENTRY(LPPHY, lpphyCtrl, rssiFormatConvEn, 1)
			PHY_REG_LIST_EXECUTE(pi);
		}

		PHY_REG_LIST_START
			/* This register field is used only here. So no need to save and restore */
			PHY_REG_MOD_ENTRY(LPPHY, TxPwrCtrlIdleTssi, rawTssiOffsetBinFormat, 1)
			/* Step */
			/* This register is used only in this function. So no need to save and
			 * restore
			 */
			PHY_REG_WRITE_ENTRY(LPPHY, TxPwrCtrlDeltaPwrLimit, 10)
		PHY_REG_LIST_EXECUTE(pi);
		/* Set starting index & NPT to 0 for idle TSSI measurements */
		wlc_lpphy_set_start_tx_pwr_idx(pi, 0);
		wlc_lpphy_set_tx_pwr_npt(pi, 0);

		/* Force manual power control */
		phy_reg_mod(pi, LPPHY_TxPwrCtrlCmd,
			(LPPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			LPPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
			LPPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK),
			LPPHY_TX_PWR_CTRL_SW);

		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			/* Force WLAN antenna */
			wlc_lpphy_btcx_override_enable(pi);
			wlc_lpphy_set_tx_gain_by_index(pi, 127);
			wlc_lpphy_set_pa_gain(pi, 0x60);
		}

		internalrftxpu_ovr_old = PHY_REG_READ(pi, LPPHY, RFOverride0, internalrftxpu_ovr);
		internalrftxpu_ovr_val_old = PHY_REG_READ(pi,
			LPPHY, RFOverrideVal0, internalrftxpu_ovr_val);

		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, internalrftxpu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, internalrftxpu_ovr_val, 0)
		PHY_REG_LIST_EXECUTE(pi);

		/* Send dummy packet to get TSSI */
		wlc_phy_do_dummy_tx(pi, TRUE, ON);
		status = phy_reg_read(pi, LPPHY_TxPwrCtrlStatus);

		/* CCK calculation offset */
		if (pi_lp->lpphy_cck_papd_disabled) {
			PHY_REG_MOD(pi, LPPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 6);
		} else {
			PHY_REG_MOD(pi, LPPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 0);
		}

		if (status & LPPHY_TxPwrCtrlStatus_estPwrValid_MASK) {
			tssi_val = (status & LPPHY_TxPwrCtrlStatus_estPwr_MASK) >>
				LPPHY_TxPwrCtrlStatus_estPwr_SHIFT;
			/* 2's compliment */
			tssi_val -= 32;

			/* Write measured idle TSSI value */
			PHY_REG_MOD(pi, LPPHY, TxPwrCtrlIdleTssi, idleTssi0, tssi_val);

			PHY_INFORM(("wl%d: %s: Measured idle TSSI: %d\n",
				pi->sh->unit, __FUNCTION__, (int8)tssi_val));
		} else {
			PHY_INFORM(("wl%d: %s: Failed to measure idle TSSI\n",
				pi->sh->unit, __FUNCTION__));
		}

		/* Clear tx PU override and restore pa gain */
		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_MOD(pi, LPPHY_REV2, RFOverride0, internalrftxpu_ovr, 0);
			wlc_lpphy_set_pa_gain(pi, save_pagain);
		}
		else {
			PHY_REG_MOD(pi, LPPHY, RFOverride0, internalrftxpu_ovr, 0);
		}

		/* Invalidate target frequency */
		pi_lp->lpphy_target_tx_freq = 0;

		/* Enable hardware power control */
		wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_HW);

		if (save_restore_flag) {
			if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
				/* Restore ADC control */
				PHY_REG_WRITE(pi, LPPHY_REV2, auxadcCtrl, auxadc_ctrl_old);

				/* Restore tssi switch */
				write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2,
					(uint16)iqcal_ctrl2_old);
				PHY_REG_MOD(pi, LPPHY, RFOverride0, internalrftxpu_ovr,
					internalrftxpu_ovr_old);
				PHY_REG_MOD(pi, LPPHY, RFOverrideVal0, internalrftxpu_ovr_val,
					internalrftxpu_ovr_val_old);
			} else {
				PHY_REG_MOD(pi, LPPHY_REV2, RFOverride0, internalrftxpu_ovr,
					internalrftxpu_ovr_old);
				PHY_REG_MOD(pi, LPPHY_REV2, RFOverrideVal0, internalrftxpu_ovr_val,
					internalrftxpu_ovr_val_old);
			}

			/* Restore tx power control */
			wlc_lpphy_tx_pwr_state_restore(pi, &tx_pwr_state);
		}

	}

	return;

}

static void
WLBANDINITFN(wlc_lpphy_tx_pwr_ctrl_init)(phy_info_t *pi)
{
	lpphy_txgains_t tx_gains;
	uint8 bbmult;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi)) {
		if (LPREV_IS(pi->pubpi.phy_rev, 2))
			wlc_lpphy_set_bbmult(pi, 0x30);
		else if (LPREV_GE(pi->pubpi.phy_rev, 3))
			wlc_lpphy_set_bbmult(pi, 0x40);
		return;
	}

	if (!pi->hwpwrctrl_capable) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			tx_gains.gm_gain = 4;
			tx_gains.pga_gain = 12;
			tx_gains.pad_gain = 12;
			tx_gains.dac_gain = 0;

			bbmult = 150;
		} else {
			tx_gains.gm_gain = 7;
			tx_gains.pga_gain = 15;
			tx_gains.pad_gain = 14;
			tx_gains.dac_gain = 0;

			bbmult = 150;
		}
		wlc_lpphy_set_tx_gain(pi, &tx_gains);
		wlc_lpphy_set_bbmult(pi, bbmult);
	} else {
		/* Clear out all power offsets */
		wlc_lpphy_clear_tx_power_offsets(pi);

		wlc_lpphy_compute_idle_tssi(pi, 0);
	}

	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
		pi_lp->lpphy_use_tx_pwr_ctrl_coeffs = 1;
		pi_lp->lpphy_use_cck_dig_loft_coeffs = 0;
	} else {
		pi_lp->lpphy_use_tx_pwr_ctrl_coeffs = 0;
		pi_lp->lpphy_use_cck_dig_loft_coeffs = 1;
		wlc_lpphy_set_bbmult(pi, 64);
	}

	{
		/* Enable hardware power control
		 * Important: Making the power control to OFF and ON is needed
		 * for some registers (ex baseindex) to take effect
		 */
		wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_OFF);

		wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_HW);
	}
}

static void
WLBANDINITFN(wlc_lpphy_rev2_baseband_init)(phy_info_t *pi)
{
	uint16 tempu16 = 0;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Enable DAC/ADC and disable rf overrides */
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, AfeDACCtrl, 0x50)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, AfeCtrl, 0x8800)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, AfeCtrlOvr, 0x0000)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, AfeCtrlOvrVal, 0x0000)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, RFOverride0, 0x0000)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, rfoverride2, 0x0000)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, rfoverride3, 0x0000)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, swctrlOvr, 0x0000)
	PHY_REG_LIST_EXECUTE(pi);

	/* Swap ADC output */
	phy_reg_mod(pi, LPPHY_REV2_adcCompCtrl,
	            LPPHY_REV2_adcCompCtrl_flipiq_adcswap_MASK,
	            !ISSIM_ENAB(pi->sh->sih) ?
	            (1 << LPPHY_REV2_adcCompCtrl_flipiq_adcswap_SHIFT) : 0);

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY_REV2, crsgainCtrl, MRCEnable, 0)
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, lnaputable, 0x5555)
		PHY_REG_LIST_EXECUTE(pi);
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Disable aux table since it's not needed */
		PHY_REG_AND_ENTRY(LPPHY_REV2, radioCtrl,
			~(LPPHY_REV2_radioCtrl_auxgaintblEn_MASK |
			LPPHY_REV2_radioCtrl_extlnagainSelect_MASK))
		/* Write agc params whose reset value differs from c-model */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, ofdmSyncThresh0, ofdmSyncThresh0, 180)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, DCOffsetTransient, dcOffsetTransientFreeCtr, 2)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, DCOffsetTransient, dcOffsetTransientThresh, 127)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gaindirectMismatch, LogainDirectMismatch, 4)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, PreambleConfirmTimeout, OFDMPreambleConfirmTimeout, 2)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, crsgainCtrl, wlpriogainChangeEn, 0)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, crsgainCtrl, preferredAntEn, 0)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, radioTRCtrl, gainrequestTRAttnOnEn, 1)
		/* This is needed to fix per issue at high powers for rev1.8 boards. default 3 */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, ofdmSyncThresh1, ofdmSyncThresh2, 3)
	PHY_REG_LIST_EXECUTE(pi);

	if (LPREV_LE(pi->pubpi.phy_rev, 3)) {
		if (BOARDTYPE(pi->sh->boardtype) == BCM94325SDABGWBA_BOARD) {
			/* x7 board */
			PHY_REG_MOD(pi, LPPHY_REV2, radioTRCtrl, gainrequestTRAttnOnEn, 0);
		} else if (BOARDTYPE(pi->sh->boardtype) == BCM94325SDGWB_BOARD) {
			/* Appolo board */
			if (pi->sh->boardrev >= 0x18) {
				lpphytbl_info_t tab;
				uint32 tainted_data_offset;
				uint8 tainted_data_value;

				tainted_data_value = 0xec;
				tainted_data_offset = 65;
				tab.tbl_ptr = &tainted_data_value;
				tab.tbl_len = 1;
				tab.tbl_id = LPPHY_TBL_ID_GAIN_VAL_TBL_REV2;
				tab.tbl_offset = tainted_data_offset;
				tab.tbl_width = 8;
				tab.tbl_phywidth = 32; /* width in the phy */

				PHY_REG_MOD(pi, LPPHY_REV2, radioTRCtrl,
					gainrequestTRAttnOnOffset, 10);

				wlc_phy_table_write_lpphy(pi, &tab);
			} else {
				PHY_REG_MOD(pi, LPPHY_REV2, radioTRCtrl,
					gainrequestTRAttnOnOffset, 8);
			}
		} else {
			/* Anything else */
			PHY_REG_MOD(pi, LPPHY_REV2, radioTRCtrl,
				gainrequestTRAttnOnOffset, 8);
		}
	} else {
		/* for 4315 */
		PHY_REG_MOD(pi, LPPHY_REV2, radioTRCtrl, gainrequestTRAttnOnEn, 0);
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gainidxoffset, ofdmgainidxtableoffset, 0xf4)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gainidxoffset, dsssgainidxtableoffset, 0xf1)
		/* AGC tweaks */
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, ClipThresh, 72)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, HiGainDB, HiGainDB, 70)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, nftrAdj, nftrAdj, 16)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, PwrThresh1, LargeGainMismatchThresh, 9)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gaindirectMismatch, MedHigainDirectMismatch, 0)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, VeryLowGainDB, NominalPwrDB, 85)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, ClipCtrThresh, clipCtrThreshLoGain, 5)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gaindirectMismatch, GainmisMatchMedGain, 3)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, HiGainDB, MedHiGainDB, 42)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, LowGainDB, MedLowGainDB, 30)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, VeryLowGainDB, veryLowGainDB, 13)
		/* MED_GAIN_2 state is disabled b/c LargeGainMismatchThresh ==
		 * MedHigainDirectMismatch
		 */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, clipCtrThreshLowGainEx, clipCtrThreshLoGain2, 31)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gainMismatchMedGainEx, gainMismatchMedGain2, 12)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, veryLowGainEx, veryLowGain_2DB, 25)
		/* MED_GAIN_3 is disabled b/c  LargeGainMismatchThresh >=
		 * medHiGainDirectMismatchOFDMDet
		 */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gainMismatchMedGainEx,
			medHiGainDirectMismatchOFDMDet, 15)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, clipCtrThreshLowGainEx, clipCtrThreshLoGain3, 31)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gainMismatchMedGainEx, gainMismatchMedGain2, 12)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, veryLowGainEx, veryLowGain_3DB, 25)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, ClipCtrThresh, clipCtrThresh, 22)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, ClipCtrThresh, ClipCtrThreshHiGain, 18)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, gainMismatch, GainmisMatchPktRx, 5)
	PHY_REG_LIST_EXECUTE(pi);

	/* Select phy band */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* Enable DSS detector */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, crsgainCtrl, DSSSDetectionEnable, 0x1)
			/* Don't assert phycrs based on crsgain's DSSS detector */
			/* Wait for bphy to detect it as well */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, crsgainCtrl, phycrsctrl, 0xb)
			/* Make bphy's detector less sensitive */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, SyncPeakCnt, MaxPeakCntM1, 6)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, MinPwrLevel, dsssMinPwrLevel, 0x9d)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, MinPwrLevel, ofdmMinPwrLevel, 0xa1)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_MOD(pi, LPPHY_REV2, crsgainCtrl, DSSSDetectionEnable, 0x0);
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		PHY_REG_MOD_ENTRY(LPPHY_REV2, crsedthresh, edonthreshold, 0xb3)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, crsedthresh, edoffthreshold, 0xad)
	PHY_REG_LIST_EXECUTE(pi);

	{
		uint8 lpphy_rx_power_offset =
			(CHSPEC_IS5G(pi->radio_chanspec)) ?
			pi_lp->lpphy_rx_power_offset_5g :
			pi_lp->lpphy_rx_power_offset_2g;

		PHY_REG_MOD(pi, LPPHY_REV2, InputPowerDB, inputpwroffsetdb,
			(uint16)lpphy_rx_power_offset);
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Reset radio ctrl and crs gain */
		PHY_REG_OR_ENTRY(LPPHY_REV2, resetCtrl, 0x44)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, resetCtrl, 0x80)
		/* RSSI settings */
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, AfeRSSICtrl0, 0xA954)
	PHY_REG_LIST_EXECUTE(pi);

	{
		uint8 lpphy_rssi_vf;
		uint8 lpphy_rssi_vc;
		uint8 lpphy_rssi_gs;

		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			lpphy_rssi_vf = pi_lp->lpphy_rssi_vf_5g;
			lpphy_rssi_vc = pi_lp->lpphy_rssi_vc_5g;
			lpphy_rssi_gs = pi_lp->lpphy_rssi_gs_5g;
		} else {
			lpphy_rssi_vf = pi_lp->lpphy_rssi_vf_2g;
			lpphy_rssi_vc = pi_lp->lpphy_rssi_vc_2g;
			lpphy_rssi_gs = pi_lp->lpphy_rssi_gs_2g;
		}

		PHY_REG_WRITE(pi, LPPHY_REV2, AfeRSSICtrl1,
			((uint16)lpphy_rssi_vf << 0) | /* selmid_rssi: RSSI Vmid fine */
			((uint16)lpphy_rssi_vc << 4) | /* selmid_rssi: RSSI Vmid coarse */
			(0x00 << 8) | /* selmid_rssi: default value from AMS */
			((uint16)lpphy_rssi_gs << 10) | /* selav_rssi: RSSI gain select */
			(0x01 << 13)); /* slpinv_rssi */
	}

	if ((LPREV_GE(pi->pubpi.phy_rev, 4)) ||
		((CHIPID(pi->sh->chip) == BCM4325_CHIP_ID) &&
		(pi->sh->chiprev >= 3))) {
		lpphytbl_info_t tab;
		int8 tableBuffer[8] = {11, 17, 20, 29, -5, 4, 10, 13};

		tab.tbl_ptr = tableBuffer;   /* table buffer */
		tab.tbl_len = 8;             /* number of values */
		tab.tbl_id = 17;             /* table id of gain_val_tbl */
		tab.tbl_offset = 0;          /* table offset from which write has to happen */
		tab.tbl_width = 8;          /* bit width of table element */
		tab.tbl_phywidth = 32;
		wlc_phy_table_write_lpphy(pi, &tab);

		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY_REV2, InputPowerDB, inputpwroffsetdb, -1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, VeryLowGainDB, NominalPwrDB, 81)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, MinPwrLevel, ofdmMinPwrLevel, -93)
		PHY_REG_LIST_EXECUTE(pi);
	}

	pi_lp->lpphy_cck_papd_disabled = TRUE;
	wlapi_bmac_mhf(pi->sh->physhim, MHF3, MHF3_PAPD_OFF_CCK, MHF3_PAPD_OFF_CCK, WLC_BAND_AUTO);

	if (CHIPID(pi->sh->chip) == BCM4325_CHIP_ID &&
	    (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
	    (CHSPEC_IS2G(pi->radio_chanspec))) {
		/* do the processing for murata board external lna */
		lpphytbl_info_t tab;
		int8 tabBuffer0[2] = {-15, 12};
		int8 tabBuffer1[4] = {0, 10, 15, 20};
		int8 tabBuffer2[4] = {5, 9, 14, 21};
		int32 RGain, TGain;
		int32 temp32;

		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY_REV2, radioCtrl, extlnaen, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, extlnagainvalue0, extlnagain1, 12)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, extlnagainvalue0, extlnagain0, -15)
		PHY_REG_LIST_EXECUTE(pi);

		tab.tbl_ptr = tabBuffer0;
		tab.tbl_len = 2;
		tab.tbl_id = LPPHY_TBL_ID_GAIN_VAL_TBL_REV3;
		tab.tbl_offset = 66;
		tab.tbl_width = 8;
		tab.tbl_phywidth = 32; /* width in the phy */
		wlc_phy_table_write_lpphy(pi, &tab);

		tab.tbl_ptr = tabBuffer2;
		tab.tbl_len = 4;
		tab.tbl_offset = 0;
		wlc_phy_table_write_lpphy(pi, &tab);

		tab.tbl_ptr = tabBuffer1;
		tab.tbl_len = 4;
		tab.tbl_offset = 4;
		wlc_phy_table_write_lpphy(pi, &tab);

		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY_REV2, nftrAdj, nftrAdj, 48)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, radioTRCtrl, gainrequestTRAttnOnOffset, 10)
		PHY_REG_LIST_EXECUTE(pi);

		tab.tbl_ptr = tabBuffer2;
		tab.tbl_len = 1;
		tab.tbl_offset = 64;
		wlc_phy_table_read_lpphy(pi, &tab);
		RGain = tabBuffer2[0];
		if (RGain > 63)
			RGain = RGain - 128;
		temp32 = (PHY_REG_READ(pi, LPPHY_REV2, radioTRCtrl, gainrequestTRAttnOnOffset));
		TGain = RGain - 3 * temp32;

		tab.tbl_len = 2;
		tab.tbl_offset = 64;
		tabBuffer2[0] = (int8)RGain;
		tabBuffer2[1] = (int8)TGain;
		wlc_phy_table_write_lpphy(pi, &tab);

		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ofdmSyncThresh0, ofdmSyncThresh0, 160)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ofdmSyncThresh0, ofdmSyncThresh1, 85)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ofdmSyncThresh1, ofdmSyncThresh2, 2)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ofdmSyncThresh1, ofdmSyncThresh3, 3)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, LTRNCtrl, crsLTRNOffset, 14)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, radioTRCtrl, gainrequestTRAttnOnEn, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, InputPowerDB, inputpwroffsetdb, -1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, VeryLowGainDB, veryLowGainDB, 14)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, LowGainDB, MedLowGainDB, 35)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ClipCtrThresh, clipCtrThreshLoGain, 4)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, gaindirectMismatch, GainmisMatchMedGain, 3)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ClipCtrThresh, clipCtrThresh, 22)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ClipCtrThresh, ClipCtrThreshHiGain, 18)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, HiGainDB, HiGainDB, 73)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, PwrThresh1, LargeGainMismatchThresh, 9)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, gaindirectMismatch,
				MedHigainDirectMismatch, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, DCBlankInterval, DCBlankIntervalLoGain, 14)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ofdmSyncTimerCtrl,
				OFDMPreambleSyncTimeOut, 8)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, ofdmSyncTimerCtrl, ofdmSyncConfirmTime, 8)
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, ClipThresh, 64)
		PHY_REG_LIST_EXECUTE(pi);

		tempu16 = PHY_REG_READ(pi, LPPHY_REV2, ClipCtrThresh, clipCtrThreshLoGain);
		PHY_REG_MOD(pi, LPPHY_REV2,
		            clipCtrThreshLowGainEx, clipCtrThreshLoGain2, tempu16);
		PHY_REG_MOD(pi, LPPHY_REV2,
		            clipCtrThreshLowGainEx, clipCtrThreshLoGain3, tempu16);
		tempu16 = PHY_REG_READ(pi, LPPHY_REV2,
		                       gaindirectMismatch, GainmisMatchMedGain);
		PHY_REG_MOD(pi, LPPHY_REV2,
		            gainMismatchMedGainEx, gainMismatchMedGain2, tempu16);
		PHY_REG_MOD(pi, LPPHY_REV2,
		            gainMismatchMedGainEx, gainMismatchMedGain3, tempu16);
		tempu16 = PHY_REG_READ(pi, LPPHY_REV2, VeryLowGainDB, veryLowGainDB);
		PHY_REG_MOD(pi, LPPHY_REV2, veryLowGainEx, veryLowGain_2DB, tempu16);
		PHY_REG_MOD(pi, LPPHY_REV2, veryLowGainEx, veryLowGain_3DB, tempu16);
	}

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		/* reset the AGC */
		tempu16 = (phy_reg_read(pi, LPPHY_REV2_resetCtrl) |
		           (1 << LPPHY_REV2_resetCtrl_radioctrlSoftReset_SHIFT) |
		           (1 << LPPHY_REV2_resetCtrl_rxfrontendSoftReset_SHIFT));
		PHY_REG_WRITE(pi, LPPHY_REV2, resetCtrl, tempu16);
		PHY_REG_WRITE(pi, LPPHY_REV2, resetCtrl,
		              1<<LPPHY_REV2_resetCtrl_rxfrontendresetStretchEn_SHIFT);
	}

	/* set up Tx digital filter */
	wlc_phy_tx_dig_filt_ofdm_setup_lpphy(pi, TRUE);
	wlc_phy_tx_dig_filt_cck_setup_lpphy(pi, FALSE);

}

static void
WLBANDINITFN(wlc_lpphy_rev0_baseband_init)(phy_info_t *pi)
{
	lpphytbl_info_t tab;
	uint16 delay_val;
	uint16 rssi_ctrl;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Enable DAC/ADC */
		PHY_REG_AND_ENTRY(LPPHY, AfeDACCtrl, ~0x0800)
		PHY_REG_WRITE_ENTRY(LPPHY, AfeCtrl, 0)
		/* Clear overrides by default */
		PHY_REG_WRITE_ENTRY(LPPHY, AfeCtrlOvr, 0)
		PHY_REG_WRITE_ENTRY(LPPHY, RFOverride0, 0)
		PHY_REG_WRITE_ENTRY(LPPHY, rfoverride2, 0)
	PHY_REG_LIST_EXECUTE(pi);

	if (NORADIO_ENAB(pi->pubpi))
		return;

	PHY_REG_LIST_START_WLBANDINITDATA
		PHY_REG_OR_ENTRY(LPPHY, AfeDACCtrl, 0x04)
		/* in tot spreadsheet(revision 1.141) */
		PHY_REG_MOD_ENTRY(LPPHY, ofdmSyncThresh0, ofdmSyncThresh0, 0x78)
		PHY_REG_MOD_ENTRY(LPPHY, ClipCtrThresh, clipCtrThresh, 0x16)
		/* flip IQ */
		PHY_REG_WRITE_ENTRY(LPPHY, adcCompCtrl, 0x0016)
	PHY_REG_LIST_EXECUTE(pi);

	/* Table in AFE AMS is wrong. Details provide the right value */
	/* tadj_iqadc_ref40u: default value is 4. */
	phy_reg_mod(pi, LPPHY_AfeADCCtrl0, 0x07, 0x04);

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Gain control */
		PHY_REG_MOD_ENTRY(LPPHY, VeryLowGainDB, NominalPwrDB, 0x54)
		PHY_REG_MOD_ENTRY(LPPHY, HiGainDB, MedHiGainDB, 36)
		PHY_REG_MOD_ENTRY(LPPHY, LowGainDB, MedLowGainDB, 33)
		PHY_REG_MOD_ENTRY(LPPHY, VeryLowGainDB, veryLowGainDB, 6)
		PHY_REG_MOD_ENTRY(LPPHY, RxRadioControl, fine_gain, 0)
		PHY_REG_MOD_ENTRY(LPPHY, ClipCtrThresh, ClipCtrThreshHiGain, 5)
		PHY_REG_MOD_ENTRY(LPPHY, ClipCtrThresh, clipCtrThreshLoGain, 12)
		PHY_REG_MOD_ENTRY(LPPHY, ClipCtrThresh, clipCtrThresh, 15)
		PHY_REG_MOD_ENTRY(LPPHY, gaindirectMismatch, MedHigainDirectMismatch, 5)
		PHY_REG_MOD_ENTRY(LPPHY, gainMismatchLimit, gainmismatchlimit, 26)
		/* Setup energy detector */
		PHY_REG_MOD_ENTRY(LPPHY, crsedthresh, edonthreshold, 0xb3)
		PHY_REG_MOD_ENTRY(LPPHY, crsedthresh, edoffthreshold, 0xad)
	PHY_REG_LIST_EXECUTE(pi);

	{
		uint8 lpphy_rx_power_offset =
			(CHSPEC_IS5G(pi->radio_chanspec)) ?
			pi_lp->lpphy_rx_power_offset_5g :
			pi_lp->lpphy_rx_power_offset_2g;

		PHY_REG_MOD(pi, LPPHY, InputPowerDB, inputpwroffsetdb,
			(uint16)lpphy_rx_power_offset);
	}

	if ((!(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_FEM) &&
	     (CHSPEC_IS5G(pi->radio_chanspec))) ||
		(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_PAREF)) {

		si_pmu_set_ldo_voltage(pi->sh->sih, pi->sh->osh, SET_LDO_VOLTAGE_PAREF,
		                       pi->u.pi_lpphy->ldo_voltage);
		si_pmu_paref_ldo_enable(pi->sh->sih, pi->sh->osh, TRUE);

		if (LPREV_IS(pi->pubpi.phy_rev, 0))
			PHY_REG_MOD(pi, LPPHY, lprfsignallut, amode_tx_pu_lut, 0x1);

		/* A little more time for the LDO voltage to settle */
		delay_val = 60;
	} else 	{
		si_pmu_paref_ldo_enable(pi->sh->sih, pi->sh->osh, FALSE);

		PHY_REG_MOD(pi, LPPHY, lprfsignallut, amode_tx_pu_lut, 0x2);

		/* Add 1.25 us delay for txpu_on */
		delay_val = 100;
	}

	tab.tbl_ptr = &delay_val;
	tab.tbl_len = 1;
	tab.tbl_id = LPPHY_TBL_ID_RFSEQ;
	tab.tbl_offset = 7;
	tab.tbl_width = 16;
	tab.tbl_phywidth = 16;
	wlc_phy_table_write_lpphy(pi, &tab);

	/* RSSI settings */
	{
		uint8 lpphy_rssi_vf;
		uint8 lpphy_rssi_vc;
		uint8 lpphy_rssi_gs;

		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			lpphy_rssi_vf = pi_lp->lpphy_rssi_vf_5g;
			lpphy_rssi_vc = pi_lp->lpphy_rssi_vc_5g;
			lpphy_rssi_gs = pi_lp->lpphy_rssi_gs_5g;
		} else {
			lpphy_rssi_vf = pi_lp->lpphy_rssi_vf_2g;
			lpphy_rssi_vc = pi_lp->lpphy_rssi_vc_2g;
			lpphy_rssi_gs = pi_lp->lpphy_rssi_gs_2g;
		}

		PHY_REG_WRITE(pi, LPPHY, AfeRSSICtrl0,
			((uint16)lpphy_rssi_vf << 0) | /* selmid_rssi: RSSI Vmid fine */
			((uint16)lpphy_rssi_vc << 4) | /* selmid_rssi: RSSI Vmid coarse */
			(0x00 << 8) | /* selmid_rssi: default value from AMS */
			((uint16)lpphy_rssi_gs << 10) | /* selav_rssi: RSSI gain select */
			(0x05 << 13)); /* default value from RSSI ADC AMS */
	}

	/* ctrlbias_rssi[13:3]: default value from RSSI ADC AMS */
	/* slpinv_rssi: */
	/* for inverted PDet 0=max_RF=>min_Vpdet=>min_code. */
	/* for positive PDet 1=min_RF=>min_Vpdet=>max_code. */
	/* RSSI_ADC output is Offset Binary. */
	rssi_ctrl = 0x2aa;
	if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_RSSIINV)
		rssi_ctrl |= 0x800;
	phy_reg_mod(pi, LPPHY_AfeRSSICtrl1, 0xfff, rssi_ctrl);

	/* Extra delay for CCK wait state to prevent using old gains when doing power control */
	delay_val = 24;
	tab.tbl_offset = 1;
	wlc_phy_table_write_lpphy(pi, &tab);

	/* Board switch architecture */
	{
		uint8 lpphy_bx_arch = (CHSPEC_IS5G(pi->radio_chanspec)) ?
			pi_lp->lpphy_bx_arch_5g :
			pi_lp->lpphy_bx_arch_2g;

		PHY_REG_MOD(pi, LPPHY, RxRadioControl, board_switch_arch, (uint16)lpphy_bx_arch);
	}

	/* TR switches */
	if (LPREV_IS(pi->pubpi.phy_rev, 1) && (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
		BFL_FEM_BT)) {

	PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup1, TRLut0, 0x9)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup1, TRLut1, 0x9)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup2, TRLut2, 0xa)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup2, TRLut3, 0xa)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup3, TRLut4, 0x9)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup3, TRLut5, 0x9)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup4, TRLut6, 0xa)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup4, TRLut7, 0xa)
			PHY_REG_MOD_ENTRY(LPPHY_REV1, TRLookup5, TRLut8, 0x5)
			PHY_REG_MOD_ENTRY(LPPHY_REV1, TRLookup5, TRLut9, 0x5)
			PHY_REG_MOD_ENTRY(LPPHY_REV1, TRLookup6, TRLut10, 0x6)
			PHY_REG_MOD_ENTRY(LPPHY_REV1, TRLookup6, TRLut11, 0x6)
			PHY_REG_MOD_ENTRY(LPPHY_REV1, TRLookup7, TRLut12, 0x5)
			PHY_REG_MOD_ENTRY(LPPHY_REV1, TRLookup7, TRLut13, 0x5)
			PHY_REG_MOD_ENTRY(LPPHY_REV1, TRLookup8, TRLut14, 0x6)
			PHY_REG_MOD_ENTRY(LPPHY_REV1, TRLookup8, TRLut15, 0x6)
	PHY_REG_LIST_EXECUTE(pi);

	} else if ((CHSPEC_IS5G(pi->radio_chanspec)) ||
	     (BOARDTYPE(pi->sh->boardtype) == BU4312_SSID) ||
	     (!LPREV_IS(pi->pubpi.phy_rev, 1) && (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
		BFL_FEM))) {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup1, TRLut0, 0x1)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup1, TRLut1, 0x4)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup2, TRLut2, 0x1)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup2, TRLut3, 0x5)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup3, TRLut4, 0x2)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup3, TRLut5, 0x8)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup4, TRLut6, 0x2)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup4, TRLut7, 0xa)
		PHY_REG_LIST_EXECUTE(pi);
	} else if (LPREV_IS(pi->pubpi.phy_rev, 1) && (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
		BFL_FEM)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup1, TRLut0, 0x4)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup1, TRLut1, 0x8)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup2, TRLut2, 0x4)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup2, TRLut3, 0xc)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup3, TRLut4, 0x2)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup3, TRLut5, 0x1)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup4, TRLut6, 0x2)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup4, TRLut7, 0x3)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup1, TRLut0, 0xa)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup1, TRLut1, 0x9)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup2, TRLut2, 0xa)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup2, TRLut3, 0xb)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup3, TRLut4, 0x6)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup3, TRLut5, 0x5)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup4, TRLut6, 0x6)
			PHY_REG_MOD_ENTRY(LPPHY, TRLookup4, TRLut7, 0x7)
		PHY_REG_LIST_EXECUTE(pi);
	}

	if (LPREV_IS(pi->pubpi.phy_rev, 1) && !(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
		BFL_FEM_BT)) {
		PHY_REG_WRITE(pi, LPPHY_REV1, TRLookup5, phy_reg_read(pi, LPPHY_REV1_TRLookup1));
		PHY_REG_WRITE(pi, LPPHY_REV1, TRLookup6, phy_reg_read(pi, LPPHY_REV1_TRLookup2));
		PHY_REG_WRITE(pi, LPPHY_REV1, TRLookup7, phy_reg_read(pi, LPPHY_REV1_TRLookup3));
		PHY_REG_WRITE(pi, LPPHY_REV1, TRLookup8, phy_reg_read(pi, LPPHY_REV1_TRLookup4));
	}

	/* Use Phase Shifter */
	if ((CHIPID(pi->sh->chip) == BCM5354_CHIP_ID) &&
	    (pi->sh->chippkg == BCM5354E_PKG_ID) &&
	    (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_PHASESHIFT)) {
		/* Enable MRC */
		PHY_REG_OR(pi, LPPHY, crsgainCtrl,
			LPPHY_crsgainCtrl_DiversityChkEnable_MASK |
			LPPHY_crsgainCtrl_MRCEnable_MASK);

		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_WRITE_ENTRY(LPPHY, gpioSel, 5)
			PHY_REG_WRITE_ENTRY(LPPHY, gpioOutEn, 0xffff)
		PHY_REG_LIST_EXECUTE(pi);

		wlapi_bmac_mhf(pi->sh->physhim, MHF3, MHF3_PR45960_WAR, MHF3_PR45960_WAR,
			WLC_BAND_ALL);
	}

	/* Select phy band */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_OR_ENTRY(LPPHY, lpphyCtrl, LPPHY_lpphyCtrl_muxGmode_MASK)
			PHY_REG_OR_ENTRY(LPPHY, crsgainCtrl,
				LPPHY_crsgainCtrl_DSSSDetectionEnable_MASK)
			/* Raise the minimum DSSS detector power */
			PHY_REG_MOD_ENTRY(LPPHY, MinPwrLevel, dsssMinPwrLevel, 0xa4)
			/* Don't assert phycrs based on crsgain's DSSS detector */
			/* Wait for bphy to detect it as well */
			PHY_REG_MOD_ENTRY(LPPHY, crsgainCtrl, phycrsctrl, 0xb)
			/* Make bphy's detector less sensitive */
			PHY_REG_MOD_ENTRY(LPPHY, SyncPeakCnt, MaxPeakCntM1, 7)
			PHY_REG_MOD_ENTRY(LPPHY, DSSSConfirmCnt, DSSSConfirmCntHiGain, 3)
			PHY_REG_MOD_ENTRY(LPPHY, DSSSConfirmCnt, DSSSConfirmCntLoGain, 4)
			/* Prevent bphy from dropping CRS after exiting rx state */
			PHY_REG_MOD_ENTRY(LPPHY, IDLEafterPktRXTimeout,
				BPHYIdleAfterPktRxTimeOut, 0)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_AND_ENTRY(LPPHY, lpphyCtrl, (uint16)~LPPHY_lpphyCtrl_muxGmode_MASK)
			PHY_REG_AND_ENTRY(LPPHY, crsgainCtrl,
				(uint16)~LPPHY_crsgainCtrl_DSSSDetectionEnable_MASK)
		PHY_REG_LIST_EXECUTE(pi);
	}

	/* BCM4312 has rev 1 */
	if (LPREV_IS(pi->pubpi.phy_rev, 1)) {
		uint16 tmp, val;

		tmp = PHY_REG_READ(pi, LPPHY_REV1, ClipCtrThresh, clipCtrThreshLoGain);

		val =
			(tmp << LPPHY_REV1_clipCtrThreshLowGainEx_clipCtrThreshLoGain2_SHIFT) |
			(tmp << LPPHY_REV1_clipCtrThreshLowGainEx_clipCtrThreshLoGain3_SHIFT);
		PHY_REG_WRITE(pi, LPPHY_REV1, clipCtrThreshLowGainEx, val);

		tmp = PHY_REG_READ(pi, LPPHY_REV1, gaindirectMismatch, GainmisMatchMedGain);

		val =
			(tmp << LPPHY_REV1_gainMismatchMedGainEx_gainMismatchMedGain2_SHIFT) |
			(tmp << LPPHY_REV1_gainMismatchMedGainEx_gainMismatchMedGain3_SHIFT);
		PHY_REG_WRITE(pi, LPPHY_REV1, gainMismatchMedGainEx, val);

	        tmp = PHY_REG_READ(pi, LPPHY_REV1, VeryLowGainDB, veryLowGainDB);

		val =
			(tmp << LPPHY_REV1_veryLowGainEx_veryLowGain_2DB_SHIFT) |
			(tmp << LPPHY_REV1_veryLowGainEx_veryLowGain_3DB_SHIFT);
		PHY_REG_WRITE(pi, LPPHY_REV1, veryLowGainEx, val);
	}
}

static void
WLBANDINITFN(wlc_lpphy_agc_temp_init)(phy_info_t *pi)
{
	uint32 temp;
	lpphytbl_info_t tab;
	int8 tableBuffer[2];
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	/* reference ofdm gain index table offset */
	temp = phy_reg_read(pi, LPPHY_REV2_gainidxoffset);
	pi_lp->lpphy_ofdmgainidxtableoffset =
	    (temp & LPPHY_REV2_gainidxoffset_ofdmgainidxtableoffset_MASK) >>
	    LPPHY_REV2_gainidxoffset_ofdmgainidxtableoffset_SHIFT;

	if (pi_lp->lpphy_ofdmgainidxtableoffset > 127) pi_lp->lpphy_ofdmgainidxtableoffset -= 256;

	/* reference dsss gain index table offset */
	pi_lp->lpphy_dsssgainidxtableoffset =
	    (temp & LPPHY_REV2_gainidxoffset_dsssgainidxtableoffset_MASK) >>
	    LPPHY_REV2_gainidxoffset_dsssgainidxtableoffset_SHIFT;

	if (pi_lp->lpphy_dsssgainidxtableoffset > 127) pi_lp->lpphy_dsssgainidxtableoffset -= 256;

	tab.tbl_ptr = tableBuffer;	/* ptr to buf */
	tab.tbl_len = 2;			/* # values   */
	tab.tbl_id = 17;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = 64;		/* tbl offset */
	tab.tbl_width = 8;			/* 32 bit wide */
	tab.tbl_phywidth = 32;		/* width of the phy element address space */
	wlc_phy_table_read_lpphy(pi, &tab);

	/* reference value of gain_val_tbl at index 64 */
	if (tableBuffer[0] > 63) tableBuffer[0] -= 128;
	pi->u.pi_lpphy->tr_R_gain_val = tableBuffer[0];

	/* reference value of gain_val_tbl at index 65 */
	if (tableBuffer[1] > 63) tableBuffer[1] -= 128;
	pi->u.pi_lpphy->tr_T_gain_val = tableBuffer[1];
}

static void
WLBANDINITFN(wlc_lpphy_baseband_init)(phy_info_t *pi)
{
	/* Initialize LPPHY tables */
	wlc_lpphy_tbl_init(pi);

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		wlc_lpphy_rev2_baseband_init(pi);
		/*
		 *Initialize the reference values
		 */
		wlc_lpphy_agc_temp_init(pi);
	}
	else
		wlc_lpphy_rev0_baseband_init(pi);
}


/* Read band specific data from the SROM */
static bool
BCMATTACHFN(wlc_phy_txpwr_srom_read_lpphy)(phy_info_t *pi)
{
	char varname[32];
	int8 txpwr;
	int i;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
	/*
	 * Band 5G specific setup
	 */

	{
		uint32 offset;

		/* TR switch isolation */
		pi_lp->lpphy_tr_isolation_5g_low = (uint8)PHY_GETINTVAR(pi, "tri5gl");
		pi_lp->lpphy_tr_isolation_5g_mid = (uint8)PHY_GETINTVAR(pi, "tri5g");
		pi_lp->lpphy_tr_isolation_5g_hi = (uint8)PHY_GETINTVAR(pi, "tri5gh");

		/* Board switch architecture */
		pi_lp->lpphy_bx_arch_5g = (uint8)PHY_GETINTVAR(pi, "bxa5g");

		/* Input power offset */
		pi_lp->lpphy_rx_power_offset_5g = (uint8)PHY_GETINTVAR(pi, "rxpo5g");

		/* RSSI */
		pi_lp->lpphy_rssi_vf_5g = (uint8)PHY_GETINTVAR(pi, "rssismf5g");
		pi_lp->lpphy_rssi_vc_5g = (uint8)PHY_GETINTVAR(pi, "rssismc5g");
		pi_lp->lpphy_rssi_gs_5g = (uint8)PHY_GETINTVAR(pi, "rssisav5g");

		/* Max tx power */
		txpwr = (int8)PHY_GETINTVAR(pi, "pa1maxpwr");

		/* PA coeffs */
		for (i = 0; i < 3; i++) {
			snprintf(varname, sizeof(varname), "pa1b%d", i);
			pi->txpa_5g_mid[i] = (int16)PHY_GETINTVAR(pi, varname);
		}

		/* Low channels */
		for (i = 0; i < 3; i++) {
			snprintf(varname, sizeof(varname), "pa1lob%d", i);
			pi->txpa_5g_low[i] = (int16)PHY_GETINTVAR(pi, varname);
		}

		/* High channels */
		for (i = 0; i < 3; i++) {
			snprintf(varname, sizeof(varname), "pa1hib%d", i);
			pi->txpa_5g_hi[i] = (int16)PHY_GETINTVAR(pi, varname);
		}

		/* The *po variables introduce a separate max tx power for reach rate.
		 * Each per-rate txpower is specified as offset from the maxtxpower
		 * from the maxtxpwr in that band (lo,mid,hi).
		 * The offsets in the variables is stored in half dbm units to save
		 * srom space, which need to be doubled to convert to quarter dbm units
		 * before using.
		 * For small 1Kbit sroms of PCI/PCIe cards, the getintav will always return 0;
		 * For bigger sroms or NVRAM or CIS, they are present
		 */

		/* Mid band channels */
		/* Extract 8 OFDM rates for mid channels */
		offset = (uint32)PHY_GETINTVAR(pi, "ofdmapo");
		pi->tx_srom_max_5g_mid = txpwr;
#ifdef PPR_API
		pi->ppr.srlgcy.ofdmapo = offset;
#else
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_5g_mid[i] = txpwr - ((offset & 0xf) * 2);
			offset >>= 4;
		}
#endif /* PPR_API */

		/* Extract 8 OFDM rates for low channels */
		offset = (uint32)PHY_GETINTVAR(pi, "ofdmalpo");
#ifdef PPR_API
		pi->ppr.srlgcy.ofdmalpo = offset;
#else
		pi->tx_srom_max_5g_low = txpwr = (int8)PHY_GETINTVAR(pi, "pa1lomaxpwr");
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_5g_low[i] = txpwr - ((offset & 0xf) * 2);
			offset >>= 4;
		}
#endif /* PPR_API */

		/* Extract 8 OFDM rates for hi channels */
		offset = (uint32)PHY_GETINTVAR(pi, "ofdmahpo");
#ifdef PPR_API
		pi->ppr.srlgcy.ofdmahpo = offset;
#else
		pi->tx_srom_max_5g_hi = txpwr = (int8)PHY_GETINTVAR(pi, "pa1himaxpwr");

		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_5g_hi[i] = txpwr - ((offset & 0xf) * 2);
			offset >>= 4;
		}
#endif /* PPR_API */
	}

	/*
	 * Band 2G specific setup
	 */

	{
		char *val;

		/* TR switch isolation */
		pi_lp->lpphy_tr_isolation_2g = (uint8)PHY_GETINTVAR(pi, "tri2g");

		/* Board switch architecture */
		pi_lp->lpphy_bx_arch_2g = (uint8)PHY_GETINTVAR(pi, "bxa2g");

		/* Input power offset */
		pi_lp->lpphy_rx_power_offset_2g = (uint8)PHY_GETINTVAR(pi, "rxpo2g");

		/* RSSI */
		pi_lp->lpphy_rssi_vf_2g = (uint8)PHY_GETINTVAR(pi, "rssismf2g");
		pi_lp->lpphy_rssi_vc_2g = (uint8)PHY_GETINTVAR(pi, "rssismc2g");
		pi_lp->lpphy_rssi_gs_2g = (uint8)PHY_GETINTVAR(pi, "rssisav2g");

		/* Max tx power */
		txpwr = (int8)PHY_GETINTVAR(pi, "pa0maxpwr");
		pi->tx_srom_max_2g = txpwr;

		/* PA coeffs */
		for (i = 0; i < 3; i++) {
			snprintf(varname, sizeof(varname), "pa0b%d", i);
			pi->txpa_2g[i] = (int16)PHY_GETINTVAR(pi, varname);
		}

		val = PHY_GETVAR(pi, "cckpo");
		if (val) {
			uint16 offset;
			uint32 offset_ofdm;
#ifndef PPR_API
			uint max_pwr_chan = txpwr;
#endif
			/* Extract offsets for 4 CCK rates. Remember to convert from
			* .5 to .25 dbm units
			*/
			offset = (int16)bcm_strtoul(val, NULL, 0);
#ifdef PPR_API
			pi->ppr.srlgcy.cckpo = offset;
#else
			for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
				pi->tx_srom_max_rate_2g[i] = max_pwr_chan - ((offset & 0xf) * 2);
				offset >>= 4;
			}
#endif /* PPR_API */
			/* Extract offsets for 8 OFDM rates */
			offset_ofdm = (uint32)PHY_GETINTVAR(pi, "ofdmgpo");
#ifdef PPR_API
			pi->ppr.srlgcy.ofdmgpo = offset_ofdm;
#else
			for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
				pi->tx_srom_max_rate_2g[i] =
					max_pwr_chan - ((offset_ofdm & 0xf) * 2);
				offset_ofdm >>= 4;
			}
#endif /* PPR_API */
		} else {
			uint8 opo = (uint8)PHY_GETINTVAR(pi, "opo");
#ifdef PPR_API
			pi->ppr.srlgcy.opo = opo;
#else
			/* Populate max power array for CCK rates */
			for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
				pi->tx_srom_max_rate_2g[i] = txpwr;
			}

			/* Populate max power array for OFDM rates */
			for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM;  i++) {
				pi->tx_srom_max_rate_2g[i] = txpwr - opo;
			}
#endif /* PPR_API */
		}
	}

	/*
	 * ldo_voltage is only used by the rev0 init path even though a value is
	 * always put in the pi structure for a lpphy.
	 */

	pi->u.pi_lpphy->ldo_voltage =
		(uint8)PHY_GETINTVAR_DEFAULT(pi, "parefldovoltage", 0x28); /* 2.9 V */


	pi->u.pi_lpphy->japan_wide_filter =
		(bool)PHY_GETINTVAR_DEFAULT(pi, "japanwidefilter", TRUE);

	/* PAPD */
	pi_lp->lpphy_papd_recal_min_interval = PHY_GETINTVAR_DEFAULT(pi, "lpphyparecalmininterval",
		LPPHY_PAPD_RECAL_MIN_INTERVAL);
	pi_lp->lpphy_papd_recal_max_interval = PHY_GETINTVAR_DEFAULT(pi, "lpphyparecalmaxinterval",
		LPPHY_PAPD_RECAL_MAX_INTERVAL);
	pi_lp->lpphy_papd_recal_gain_delta = PHY_GETINTVAR_DEFAULT(pi, "lpphyparecalgaindelta",
		LPPHY_PAPD_RECAL_GAIN_DELTA);
	pi_lp->lpphy_papd_recal_enable = (bool)PHY_GETINTVAR_DEFAULT(pi, "lpphyparecalenable",
		LPPHY_PAPD_RECAL_ENABLE);

	pi->u.pi_lpphy->extlna_type = (uint16)PHY_GETINTVAR(pi, "extlnatype");
	if (pi->u.pi_lpphy->extlna_type > 1) {
		pi->u.pi_lpphy->extlna_type = 0;
	}

	pi_lp->lpphy_cck_dig_filt_type = (int16)PHY_GETINTVAR_DEFAULT(pi, "cckdigfilttype", -1);
	if ((pi_lp->lpphy_cck_dig_filt_type < 0) ||
		(pi_lp->lpphy_cck_dig_filt_type >= LPPHY_REV2_NUM_TX_DIG_FILTERS_CCK)) {
		pi_lp->lpphy_cck_dig_filt_type  = -1;
	}

	pi_lp->lpphy_ofdm_dig_filt_type = (int16)PHY_GETINTVAR_DEFAULT(pi, "ofdmdigfilttype", -1);
	if ((pi_lp->lpphy_ofdm_dig_filt_type < 0) ||
		(pi_lp->lpphy_ofdm_dig_filt_type >= LPPHY_REV2_NUM_TX_DIG_FILTERS_OFDM)) {
		pi_lp->lpphy_ofdm_dig_filt_type  = -1;
	}

	pi_lp->lpphy_txrf_sp_9_override = (int16)PHY_GETINTVAR_DEFAULT(pi, "txrf_sp_9", -1);

	pi->ofdm_analog_filt_bw_override = (int16)PHY_GETINTVAR_DEFAULT(pi, "ofdmanalogfiltbw", -1);
	pi->cck_analog_filt_bw_override = (int16)PHY_GETINTVAR_DEFAULT(pi, "cckanalogfiltbw", -1);
	pi->ofdm_rccal_override = (int16)PHY_GETINTVAR_DEFAULT(pi, "ofdmrccaloverride", -1);
	pi->cck_rccal_override = (int16)PHY_GETINTVAR_DEFAULT(pi, "cckrccaloverride", -1);

	return TRUE;
}

typedef struct {
	uint16 phy_addr;
	uint8 phy_shift;
	uint8 rf_addr;
	uint8 rf_shift;
	uint8 mask;
} lpphy_extstxdata_t;

static lpphy_extstxdata_t
WLBANDINITDATA(lpphy_extstxdata)[] = {
	{LPPHY_REV2_extstxctrl0 + 2, 6, 0x3d, 3, 0x1},
	{LPPHY_REV2_extstxctrl0 + 1, 12, 0x4c, 1, 0x1},
	{LPPHY_REV2_extstxctrl0 + 1, 8, 0x50, 0, 0x7f},
	{LPPHY_REV2_extstxctrl0 + 0, 8, 0x44, 0, 0xff},
	{LPPHY_REV2_extstxctrl0 + 1, 0, 0x4a, 0, 0xff},
	{LPPHY_REV2_extstxctrl0 + 0, 4, 0x4d, 0, 0xff},
	{LPPHY_REV2_extstxctrl0 + 1, 4, 0x4e, 0, 0xff},
	{LPPHY_REV2_extstxctrl0 + 0, 12, 0x4f, 0, 0xf},
	{LPPHY_REV2_extstxctrl0 + 1, 0, 0x4f, 4, 0xf},
	{LPPHY_REV2_extstxctrl0 + 3, 0, 0x49, 0, 0xf},
	{LPPHY_REV2_extstxctrl0 + 4, 3, 0x46, 4, 0x7},
	{LPPHY_REV2_extstxctrl0 + 3, 15, 0x46, 0, 0x1},
	{LPPHY_REV2_extstxctrl0 + 4, 0, 0x46, 1, 0x7},
	{LPPHY_REV2_extstxctrl0 + 3, 8, 0x48, 4, 0x7},
	{LPPHY_REV2_extstxctrl0 + 3, 11, 0x48, 0, 0xf},
	{LPPHY_REV2_extstxctrl0 + 3, 4, 0x49, 4, 0xf},
	{LPPHY_REV2_extstxctrl0 + 2, 15, 0x45, 0, 0x1},
	{LPPHY_REV2_extstxctrl0 + 5, 13, 0x52, 4, 0x7},
	{LPPHY_REV2_extstxctrl0 + 6, 0, 0x52, 7, 0x1},
	{LPPHY_REV2_extstxctrl0 + 5, 3, 0x41, 5, 0x7},
	{LPPHY_REV2_extstxctrl0 + 5, 6, 0x41, 0, 0xf},
	{LPPHY_REV2_extstxctrl0 + 5, 10, 0x42, 5, 0x7},
	{LPPHY_REV2_extstxctrl0 + 4, 15, 0x42, 0, 0x1},
	{LPPHY_REV2_extstxctrl0 + 5, 0, 0x42, 1, 0x7},
	{LPPHY_REV2_extstxctrl0 + 4, 11, 0x43, 4, 0xf},
	{LPPHY_REV2_extstxctrl0 + 4, 7, 0x43, 0, 0xf},
	{LPPHY_REV2_extstxctrl0 + 4, 6, 0x45, 1, 0x1},
	{LPPHY_REV2_extstxctrl0 + 2, 7, 0x40, 4, 0xf},
	{LPPHY_REV2_extstxctrl0 + 2, 11, 0x40, 0, 0xf},
	{LPPHY_REV2_extstxctrl0 + 1, 14, 0x3c, 3, 0x3},
	{LPPHY_REV2_extstxctrl0 + 2, 0, 0x3c, 5, 0x7},
	{LPPHY_REV2_extstxctrl0 + 2, 3, 0x3c, 0, 0x7},
	{LPPHY_REV2_extstxctrl0 + 0, 0, 0x52, 0, 0xf},
	};

static void
WLBANDINITFN(lpphy_run_jtag_rcal)(phy_info_t *pi)
{
	int RCAL_done;
	int RCAL_timeout = 10;

	/* global RCAL override Enable */
	write_radio_reg(pi, RADIO_2063_COMMON_13, 0x10);

	/* put in override and power down RCAL */
	write_radio_reg(pi, RADIO_2063_COMMON_16, 0x10);

	/* Run RCAL */
	write_radio_reg(pi, RADIO_2063_COMMON_16, 0x14);

	/* Wait for RCAL Valid bit to be set */
	RCAL_done = (read_radio_reg(pi, RADIO_2063_COMMON_17) & 0x20) >> 5;

	while (RCAL_done == 0 && RCAL_timeout > 0) {
		OSL_DELAY(1);
		RCAL_done = (read_radio_reg(pi, RADIO_2063_COMMON_17) & 0x20) >> 5;
		RCAL_timeout--;
	}

	ASSERT(RCAL_done != 0);
}

static void
WLBANDINITFN(wlc_lpphy_synch_stx)(phy_info_t *pi)
{
	uint i;

	mod_radio_reg(pi, RADIO_2063_COMMON_04, 0xf8, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_05, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_06, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_07, 0xff);
	mod_radio_reg(pi, RADIO_2063_COMMON_08, 0x7, 0xff);

	for (i = 0; i < ARRAYSIZE(lpphy_extstxdata); i++) {
		phy_reg_mod(pi,
			lpphy_extstxdata[i].phy_addr,
			(uint16)lpphy_extstxdata[i].mask << lpphy_extstxdata[i].phy_shift,
			(uint16)(read_radio_reg(pi, lpphy_extstxdata[i].rf_addr) >>
			lpphy_extstxdata[i].rf_shift) << lpphy_extstxdata[i].phy_shift);
	}

	mod_radio_reg(pi, RADIO_2063_COMMON_04, 0xf8, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_05, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_06, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_07, 0);
	mod_radio_reg(pi, RADIO_2063_COMMON_08, 0x7, 0);
}

static void
WLBANDINITFN(wlc_lpphy_radio_init)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* Toggle radio reset */
	PHY_REG_OR(pi, LPPHY, fourwireControl, LPPHY_fourwireControl_radioReset_MASK);
	OSL_DELAY(1);
	PHY_REG_AND(pi, LPPHY, fourwireControl, ~LPPHY_fourwireControl_radioReset_MASK);
	OSL_DELAY(1);

	if (BCM2063_ID == LPPHY_RADIO_ID(pi)) {
		uint32 macintmask;

		/* Initialize 2063 radio */
		wlc_radio_2063_init(pi);

		/* Synchronize phy overrides for RF registers that are mapped through the CLB */
		wlc_lpphy_synch_stx(pi);

		PHY_REG_LIST_START_WLBANDINITDATA
			/* Shared RX clb signals */
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, extslnactrl0, 0x5f80)
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, extslnactrl1, 0x0)
		PHY_REG_LIST_EXECUTE(pi);

		/* Run RCal */
		macintmask = wlapi_intrsoff(pi->sh->physhim);
		if (LPREV_IS(pi->pubpi.phy_rev, 4)) {
			lpphy_run_jtag_rcal(pi);
		} else {
			si_pmu_rcal(pi->sh->sih, pi->sh->osh);
		}
		wlapi_intrsrestore(pi->sh->physhim, macintmask);
	} else {
		/* Initialize 2062 radio */
		wlc_radio_2062_init(pi);
	}
}

static void
wlc_lpphy_restore_txiqlo_calibration_results(phy_info_t *pi)
{
	lpphytbl_info_t tab;
	uint16 a, b;
	uint16 didq;
	uint32 val;
	uint idx;
	uint8 ei0, eq0, fi0, fq0;
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lpphy_calcache_t *cache = &ctx->u.lpphy_cache;
#else
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
#endif

#if defined(PHYCAL_CACHING)
	ASSERT(cache->txiqlocal_bestcoeffs_valid);
	a = cache->txiqlocal_bestcoeffs[0];
	b = cache->txiqlocal_bestcoeffs[1];
	didq = cache->txiqlocal_bestcoeffs[5];
#else
	ASSERT(pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs_valid);
	a = pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[0];
	b = pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[1];
	didq = pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[5];
#endif

	wlc_phy_set_tx_iqcc_lpphy(pi, a, b);
	wlc_phy_set_tx_locc_lpphy(pi, didq);

	wlc_phy_set_tx_locc_ucode_lpphy(pi, FALSE, didq);
#if defined(PHYCAL_CACHING)
	wlc_phy_set_tx_locc_ucode_lpphy(pi, TRUE, cache->didq_cck);
#else
	wlc_phy_set_tx_locc_ucode_lpphy(pi, TRUE, pi_lp->lpphy_cal_results.didq_cck);
#endif

	/* restore iqlo portion of tx power control tables */
	/* remaining element */
	tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_phywidth = 32; /* widht in the phy */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_ptr = &val; /* ptr to buf */
	for (idx = 0; idx < 128; idx++) {
		/* iq */
		tab.tbl_offset = LPPHY_TX_PWR_CTRL_IQ_OFFSET + idx;
		wlc_phy_table_read_lpphy(pi,  &tab);
		val = (val & 0x0ff00000) |
			((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_phy_table_write_lpphy(pi,  &tab);
		/* loft */
		tab.tbl_offset = LPPHY_TX_PWR_CTRL_LO_OFFSET + idx;
		val = didq;
		wlc_phy_table_write_lpphy(pi,  &tab);
	}

	/* Do not move the below statements up */
	/* We need at least 2us delay to read phytable after writing radio registers */
	/* Apply analog LO */
#if defined(PHYCAL_CACHING)
	ei0 = (uint8)(cache->txiqlocal_bestcoeffs[7] >> 8);
	eq0 = (uint8)(cache->txiqlocal_bestcoeffs[7]);
	fi0 = (uint8)(cache->txiqlocal_bestcoeffs[9] >> 8);
	fq0 = (uint8)(cache->txiqlocal_bestcoeffs[9]);
#else
	ei0 = (uint8)(pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[7] >> 8);
	eq0 = (uint8)(pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[7]);
	fi0 = (uint8)(pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[9] >> 8);
	fq0 = (uint8)(pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[9]);
#endif
	wlc_phy_set_radio_loft_lpphy(pi, ei0, eq0, fi0, fq0);
}

static void
wlc_lpphy_restore_papd_calibration_results(phy_info_t *pi)
{
	lpphytbl_info_t tab;

#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lpphy_calcache_t *cache = &ctx->u.lpphy_cache;
#else
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
#endif

	/* Apply PAPD cal results */
	/* write EPS table */
	tab.tbl_id = LPPHY_TBL_ID_PAPD_EPS;
	tab.tbl_width = 32;
	tab.tbl_phywidth = 32;	/* width of table element in phy address space */
	tab.tbl_len = LPPHY_PAPD_EPS_TBL_SIZE;
	tab.tbl_offset = 0;
#if defined(PHYCAL_CACHING)
	tab.tbl_ptr = cache->papd_eps_tbl;
#else
	tab.tbl_ptr = pi_lp->lpphy_cal_results.papd_eps_tbl;
#endif
	wlc_phy_table_write_lpphy(pi, &tab);

	/* Apply papd index offset */
#if defined(PHYCAL_CACHING)
	PHY_REG_MOD(pi, LPPHY_REV2, papdScalar, indexOffset,
		cache->papd_indexOffset);
#else
	PHY_REG_MOD(pi, LPPHY_REV2, papdScalar, indexOffset,
		pi_lp->lpphy_cal_results.papd_indexOffset);
#endif

	/* Apply start stop index */
	phy_reg_write(pi, LPPHY_REV2_papdstartstopIndex,
#if defined(PHYCAL_CACHING)
		cache->papd_startstopindex);
#else
		pi_lp->lpphy_cal_results.papd_startstopindex);
#endif

	/* Enable papd */
	PHY_REG_MOD(pi, LPPHY_REV2, papdctrl, papdcompEn, 1);
}

static void
wlc_lpphy_restore_txrxiq_calibration_results(phy_info_t *pi)
{
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lpphy_calcache_t *cache;

	ASSERT(ctx);
	cache = &ctx->u.lpphy_cache;
	ASSERT(cache);
#else
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
#endif

	wlc_lpphy_restore_txiqlo_calibration_results(pi);

	/* restore rx iq cal results */
#if defined(PHYCAL_CACHING)
	PHY_REG_WRITE(pi, LPPHY, RxCompcoeff, cache->rxiqcal_coeffs);
#else
	PHY_REG_WRITE(pi, LPPHY, RxCompcoeff, pi_lp->lpphy_cal_results.rxiqcal_coeffs);
#endif

	/* restore rx gain adjustment based on temperature */
	wlc_phy_rx_gain_temp_adj_lpphy(pi);
}

void
WLBANDINITFN(wlc_phy_init_lpphy)(phy_info_t *pi)
{
	/* Initialize baseband */
	wlc_lpphy_baseband_init(pi);

	/* Initialize radio */
	wlc_lpphy_radio_init(pi);

	/* Run RC Cal */
	wlc_lpphy_rc_cal(pi);

	/* Tune to the current channel */
	wlc_phy_chanspec_set((wlc_phy_t *)pi, pi->radio_chanspec);

	/* Tx power control */
	wlc_lpphy_tx_pwr_ctrl_init(pi);

	wlc_phy_aci_init_lpphy(pi, TRUE);
}


void
WLBANDINITFN(wlc_phy_cal_init_lpphy)(phy_info_t *pi)
{
}

void
wlc_phy_set_deaf_lpphy(phy_info_t *pi, bool user_flag)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (user_flag)
		pi_lp->lpphy_crs_disable_by_user = 1;
	else
		pi_lp->lpphy_crs_disable_by_system = 1;

	phy_reg_mod((pi), LPPHY_crsgainCtrl,
		LPPHY_crsgainCtrl_crseddisable_MASK |
		LPPHY_crsgainCtrl_DSSSDetectionEnable_MASK |
		LPPHY_crsgainCtrl_OFDMDetectionEnable_MASK,
		1 << LPPHY_crsgainCtrl_crseddisable_SHIFT);
}

void
wlc_phy_clear_deaf_lpphy(phy_info_t *pi, bool user_flag)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (user_flag)
		pi_lp->lpphy_crs_disable_by_user = 0;
	else
		pi_lp->lpphy_crs_disable_by_system = 0;

	if ((pi_lp->lpphy_crs_disable_by_user == 0) && (pi_lp->lpphy_crs_disable_by_system == 0)) {
		phy_reg_mod((pi), LPPHY_crsgainCtrl,
			LPPHY_crsgainCtrl_crseddisable_MASK |
			LPPHY_crsgainCtrl_DSSSDetectionEnable_MASK |
			LPPHY_crsgainCtrl_OFDMDetectionEnable_MASK,
			((CHSPEC_IS2G(pi->radio_chanspec)) ? 1 : 0) <<
			LPPHY_crsgainCtrl_DSSSDetectionEnable_SHIFT |
			1 << LPPHY_crsgainCtrl_OFDMDetectionEnable_SHIFT);
	}
}

static bool
wlc_lpphy_iqcal_wait(phy_info_t *pi)
{
	uint delay_count = 0;

	while (wlc_lpphy_iqcal_active(pi)) {
		OSL_DELAY(100);
		delay_count++;

		if (delay_count > (10 * 500)) /* 500 ms */
			break;
	}

	PHY_TMP(("wl%d: %s: %u us\n", pi->sh->unit, __FUNCTION__, delay_count * 100));

	return (wlc_lpphy_iqcal_active(pi) == 0);
}

void
wlc_phy_stop_tx_tone_lpphy(phy_info_t *pi)
{
	int delay_count = 0;

	pi->phy_tx_tone_freq = 0;

	/* Stop sample buffer playback */
	PHY_REG_AND(pi, LPPHY, SampleplayCnt, (uint16)~LPPHY_SampleplayCnt_sampPlayCount_MASK);

	/* Wait for completion */
	while (phy_reg_read(pi, LPPHY_AphyControlAddr) & LPPHY_AphyControlAddr_phyloopbackEn_MASK) {
		OSL_DELAY(100);
		delay_count++;

		if (delay_count > (10 * 3)) { /* 3 ms */
			PHY_ERROR(("wl%d: %s: failed to stop\n", pi->sh->unit, __FUNCTION__));
			break;
		}
	}

	PHY_INFORM(("wl%d: %s: completed in %d us\n",
		pi->sh->unit, __FUNCTION__, delay_count * 100));

}

/*
 * Play samples from sample play buffer
 */
static void
wlc_lpphy_run_samples(phy_info_t *pi, uint16 num_samps, uint16 num_loops, uint16 wait)
{
	PHY_REG_MOD(pi, LPPHY, SampplayBufControl, sampPlaydepth, num_samps - 1);

	if (num_loops != 0xffff)
		num_loops--;
	PHY_REG_MOD(pi, LPPHY, SampleplayCnt, sampPlayCount, num_loops);
	PHY_REG_MOD(pi, LPPHY, SampplayBufControl, sampPlayWait, wait);
	PHY_REG_MOD(pi, LPPHY, AphyControlAddr, phyloopbackEn, 1);
}

/*
* Given a test tone frequency, continuously play the samples. Ensure that num_periods
* specifies the number of periods of the underlying analog signal over which the
* digital samples are periodic
*/
void
wlc_phy_start_tx_tone_lpphy(phy_info_t *pi, int32 f_kHz, uint16 max_val)
{
	uint8 phy_bw;
	uint16 num_samps, t, bw, k;
	fixed theta = 0, rot = 0;
	cint32 tone_samp;
	uint16 data_buf[64];
	uint16 i_samp, q_samp;
	lpphytbl_info_t tab;

	/* Save active tone frequency */
	pi->phy_tx_tone_freq = f_kHz;

	/* check phy_bw */
	phy_bw = 20;
	ASSERT(!CHSPEC_IS40(pi->radio_chanspec));

	/* allocate buffer */
	if (f_kHz) {
		k = 1;
		do {
			bw = phy_bw * 1000 * k;
			num_samps = bw / ABS(f_kHz);
			ASSERT(num_samps <= ARRAYSIZE(data_buf));
			k++;
		} while ((num_samps * ABS(f_kHz)) !=  bw);
	} else
		num_samps = 2;

	PHY_INFORM(("wl%d: %s: %d kHz, %d samples\n",
		pi->sh->unit, __FUNCTION__,
		f_kHz, num_samps));

	/* set up params to generate tone */
	rot = FIXED((f_kHz * 36)/phy_bw) / 100; /* 2*pi*f/bw/1000  Note: f in KHz */
	theta = 0;			/* start angle 0 */

	/* tone freq = f_c MHz ; phy_bw = phy_bw MHz ; # samples = phy_bw (1us) ; max_val = 151 */
	/* TCL: set tone_buff [mimophy_gen_tone $f_c $phy_bw $phy_bw $max_val] */
	for (t = 0; t < num_samps; t++) {
		/* compute phasor */
		wlc_phy_cordic(theta, &tone_samp);
		/* update rotation angle */
		theta += rot;
		/* produce sample values for play buffer */
		i_samp = (uint16)(FLOAT(tone_samp.i * max_val) & 0xff);
		q_samp = (uint16)(FLOAT(tone_samp.q * max_val) & 0xff);
		data_buf[t] = (i_samp << 8) | q_samp;
	}

	/* load sample table */
	tab.tbl_ptr = data_buf;
	tab.tbl_len = num_samps;
	tab.tbl_id = 5;
	tab.tbl_offset = 0;
	tab.tbl_width = 16;
	tab.tbl_phywidth = 16;
	wlc_phy_table_write_lpphy(pi, &tab);

	/* run samples */
	wlc_lpphy_run_samples(pi, num_samps, 0xffff, 0);
}

/*
* Get Rx IQ Imbalance Estimate from modem
*/
static bool
wlc_lpphy_rx_iq_est(phy_info_t *pi, uint16 num_samps, uint8 wait_time, lpphy_iq_est_t *iq_est)
{
	int wait_count = 0;
	bool result = TRUE;

	/* Force OFDM receiver on */
	PHY_REG_MOD(pi, LPPHY, crsgainCtrl, APHYGatingEnable, 0);
	PHY_REG_MOD(pi, LPPHY, IQNumSampsAddress, numSamps, num_samps);
	PHY_REG_MOD(pi, LPPHY, IQEnableWaitTimeAddress, waittimevalue, (uint16)wait_time);

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LPPHY, IQEnableWaitTimeAddress, iqmode, 0)
		PHY_REG_MOD_ENTRY(LPPHY, IQEnableWaitTimeAddress, iqstart, 1)
	PHY_REG_LIST_EXECUTE(pi);

	/* Wait for IQ estimation to complete */
	while (phy_reg_read(pi, LPPHY_IQEnableWaitTimeAddress) &
		LPPHY_IQEnableWaitTimeAddress_iqstart_MASK) {
		/* Check for timeout */
		if (wait_count > (10 * 500)) { /* 500 ms */
			PHY_ERROR(("wl%d: %s: IQ estimation failed to complete\n",
				pi->sh->unit, __FUNCTION__));
			result = FALSE;
			goto cleanup;
		}
		OSL_DELAY(100);
		wait_count++;
	}

	/* Save results */
	iq_est->iq_prod = ((uint32)phy_reg_read(pi, LPPHY_IQAccHiAddress) << 16) |
		(uint32)phy_reg_read(pi, LPPHY_IQAccLoAddress);
	iq_est->i_pwr = ((uint32)phy_reg_read(pi, LPPHY_IQIPWRAccHiAddress) << 16) |
		(uint32)phy_reg_read(pi, LPPHY_IQIPWRAccLoAddress);
	iq_est->q_pwr = ((uint32)phy_reg_read(pi, LPPHY_IQQPWRAccHiAddress) << 16) |
		(uint32)phy_reg_read(pi, LPPHY_IQQPWRAccLoAddress);
	PHY_TMP(("wl%d: %s: IQ estimation completed in %d us,"
		"i_pwr: %d, q_pwr: %d, iq_prod: %d\n",
		pi->sh->unit, __FUNCTION__,
		wait_count * 100, iq_est->i_pwr, iq_est->q_pwr, iq_est->iq_prod));

cleanup:
	PHY_REG_MOD(pi, LPPHY, crsgainCtrl, APHYGatingEnable, 1);

	return result;
}

/*
* Compute Rx compensation coeffs
*   -- run IQ est and calculate compensation coefficients
*/
static bool
wlc_lpphy_calc_rx_iq_comp(phy_info_t *pi,  uint16 num_samps,
	lpphy_rx_iq_cal_data_for_temp_adj_t *p_temp_adj_data)
{
#define LPPHY_MIN_RXIQ_PWR 2
	bool result;
	uint16 c0_new, c1_new;
	lpphy_iq_est_t iq_est = {0, 0, 0};
	int32  a, c;
	int16  iq_nbits, qq_nbits, crsh, arsh;
	int32  iq;
	uint32 ii = 0, qq = 0, ii_qq = 0;

	/* Save original c0 & c1 */
	c0_new = PHY_REG_READ(pi, LPPHY, RxCompcoeff, c0);
	c1_new = PHY_REG_READ(pi, LPPHY, RxCompcoeff, c1);

	PHY_REG_LIST_START
		/* Zero out comp coeffs and do "one-shot" calibration */
		PHY_REG_MOD_ENTRY(LPPHY, RxCompcoeff, c1, 0xc0)
		PHY_REG_MOD_ENTRY(LPPHY, RxCompcoeff, c0, 0x00)
	PHY_REG_LIST_EXECUTE(pi);

	if (!(result = wlc_lpphy_rx_iq_est(pi, num_samps, 32, &iq_est)))
		goto cleanup;

	iq = (int32)iq_est.iq_prod;
	ii = iq_est.i_pwr;
	qq = iq_est.q_pwr;
	ii_qq = ii + qq;

	/* bounds check estimate info */
	if ((ii + qq) < LPPHY_MIN_RXIQ_PWR) {
		PHY_ERROR(("wl%d: %s: RX IQ imbalance estimate power too small\n",
			pi->sh->unit, __FUNCTION__));
		result = FALSE;
		goto cleanup;
	}

	/* Calculate new coeffs */
	iq_nbits = wlc_phy_nbits(iq);
	qq_nbits = wlc_phy_nbits(qq);

	crsh = 10-(30-iq_nbits);
	if (crsh >= 0) {
		c = ((iq << (30 - iq_nbits)) + (ii >> (1 + crsh)));
		c /= (int32) (ii >>  crsh);
	} else {
		c = ((iq << (30 - iq_nbits)) + (ii << (-1 - crsh)));
		c /= (int32) (ii << -crsh);
	}

	arsh = qq_nbits-31+20;
	if (arsh >= 0) {
		a = (qq << (31-qq_nbits));
		a /= (int32) (ii >>  arsh);
	} else {
		a = (qq << (31-qq_nbits));
		a /= (int32) (ii << -arsh);
	}
	a -= c*c;
	a = -(int32)wlc_phy_sqrt_int((uint32) a);

	c0_new = (uint16)(c >> 3);
	c1_new = (uint16)(a >> 4);

cleanup:
	/* Apply new coeffs */
	PHY_REG_MOD(pi, LPPHY, RxCompcoeff, c1, c1_new);
	PHY_REG_MOD(pi, LPPHY, RxCompcoeff, c0, c0_new);

	p_temp_adj_data->valid = result;
	ASSERT(pi->pubpi.phy_corenum <= 1);
	wlc_phy_compute_dB(&ii_qq, &(p_temp_adj_data->rx_pwr_dB), pi->pubpi.phy_corenum);

	return result;
}

static void
wlc_lpphy_restore_crs(phy_info_t *pi, bool user_flag)
{
	/* Restore crs signals to the MAC */
	wlc_phy_clear_deaf_lpphy(pi, (bool)user_flag);

	/* Clear overrides */
	PHY_REG_AND(pi, LPPHY, RFOverride0,
		~(uint16)(
		LPPHY_RFOverride0_trsw_rx_pu_ovr_MASK |
		LPPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
		LPPHY_RFOverride0_ant_selp_ovr_MASK |
		LPPHY_RFOverride0_gmode_tx_pu_ovr_MASK |
		LPPHY_RFOverride0_gmode_rx_pu_ovr_MASK |
		LPPHY_RFOverride0_amode_tx_pu_ovr_MASK |
		LPPHY_RFOverride0_amode_rx_pu_ovr_MASK));

	PHY_REG_AND(pi, LPPHY, rfoverride2,
		~(uint16)(
		LPPHY_rfoverride2_hpf1_ctrl_ovr_MASK |
		LPPHY_rfoverride2_hpf2_ctrl_ovr_MASK |
		LPPHY_rfoverride2_lpf_lq_ovr_MASK |
		LPPHY_rfoverride2_lna1_pu_ovr_MASK |
		LPPHY_rfoverride2_lna2_pu_ovr_MASK |
		LPPHY_rfoverride2_ps_ctrl_ovr_MASK |
		LPPHY_rfoverride2_gmode_ext_lna_gain_ovr_MASK |
		LPPHY_rfoverride2_amode_ext_lna_gain_ovr_MASK |
		LPPHY_rfoverride2_txgainctrl_ovr_MASK |
		LPPHY_rfoverride2_rxgainctrl_ovr_MASK));
}

static void
wlc_lpphy_disable_crs(phy_info_t *pi, bool user_flag)
{
	/* Turn off all the crs signals to the MAC */
	wlc_phy_set_deaf_lpphy(pi, (bool)user_flag);

	/* Force the TR switch to receive */
	wlc_lpphy_set_trsw_override(pi, FALSE, TRUE);

	PHY_REG_LIST_START
		/* Force the antenna to 0 */
		PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, ant_selp_ovr_val, 0)
		PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, ant_selp_ovr, 1)
		/* Power down the 2.4 GHz Tx */
		PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, gmode_tx_pu_ovr_val, 0)
		PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, gmode_tx_pu_ovr, 1)
		/* Power up the 2.4 GHz Rx */
		PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, gmode_rx_pu_ovr_val, 1)
		PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, gmode_rx_pu_ovr, 1)
		/* Power down the 5 GHz Tx */
		PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, amode_tx_pu_ovr_val, 0)
		PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, amode_tx_pu_ovr, 1)
		/* Power down the 5 GHz Rx */
		PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, amode_rx_pu_ovr_val, 0)
		PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, amode_rx_pu_ovr, 1)
		/* Force the HPFs to high BW */
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, hpf1_ctrl_ovr_val, 0x7)
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, hpf2_ctrl_ovr_val, 0x7)
		/* Force the LPF to low Q */
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, lpf_lq_ovr_val, 0x0)
		/* Power up LNA 1 and power down LNA 2 */
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, lna1_pu_ovr_val, 1)
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, lna2_pu_ovr_val, 0)
		/* Bypass phase shifter on signal from LNA1 and turn LNA 2 path off */
		PHY_REG_WRITE_ENTRY(LPPHY, psctrlovrval0, 0x0000)
		PHY_REG_WRITE_ENTRY(LPPHY, psctrlovrval1, 0x0001)
		PHY_REG_WRITE_ENTRY(LPPHY, psctrlovrval2, 0x20)
		/* Force external LNA gain to 0 */
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, gmode_ext_lna_gain_ovr_val, 0)
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, amode_ext_lna_gain_ovr_val, 0)
		/* Override tx and rx gains  */
		PHY_REG_WRITE_ENTRY(LPPHY, txgainctrlovrval, 0x0)
		PHY_REG_WRITE_ENTRY(LPPHY, rxgainctrlovrval, 0x45AF)
		/* Enable above overrides */
		PHY_REG_WRITE_ENTRY(LPPHY, rfoverride2, 0x03ff)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
wlc_lpphy_stop_ddfs(phy_info_t *pi)
{
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LPPHY, afe_ddfs, playoutEn, 0)
		PHY_REG_MOD_ENTRY(LPPHY, lpphyCtrl, afe_ddfs_en, 0)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
wlc_lpphy_run_ddfs(phy_info_t *pi, int i_on, int q_on,
	int incr1, int incr2, int scale_index)
{
	wlc_lpphy_stop_ddfs(pi);

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LPPHY, afe_ddfs_pointer_init, lutPointer1Init, 0)
		PHY_REG_MOD_ENTRY(LPPHY, afe_ddfs_pointer_init, lutPointer2Init, 0)
	PHY_REG_LIST_EXECUTE(pi);

	PHY_REG_MOD(pi, LPPHY, afe_ddfs_incr_init, lutIncr1Init, incr1);
	PHY_REG_MOD(pi, LPPHY, afe_ddfs_incr_init, lutIncr2Init, incr2);
	PHY_REG_MOD(pi, LPPHY, afe_ddfs, chanIEn, i_on);
	PHY_REG_MOD(pi, LPPHY, afe_ddfs, chanQEn, q_on);
	PHY_REG_MOD(pi, LPPHY, afe_ddfs, scaleIndex, scale_index);

	PHY_REG_LIST_START
		/* Single tone */
		PHY_REG_MOD_ENTRY(LPPHY, afe_ddfs, twoToneEn, 0x0)
		PHY_REG_MOD_ENTRY(LPPHY, afe_ddfs, playoutEn, 0x1)
		PHY_REG_MOD_ENTRY(LPPHY, lpphyCtrl, afe_ddfs_en, 1)
	PHY_REG_LIST_EXECUTE(pi);
}

static int
wlc_lpphy_loopback(phy_info_t *pi)
{
	lpphy_iq_est_t iq_est = {0, 0, 0};
	int gain_index, good_gain_index = -1;
	uint32 iq_pwr;

	/* Set TR switches in loopback mode */
	wlc_lpphy_set_trsw_override(pi, TRUE, TRUE);

	PHY_REG_LIST_START
		/* Force ADC on */
		PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvr, pwdn_adc_ovr, 1)
		PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvrVal, pwdn_adc_ovr_val, 0)
		/* Force RxPu on */
		PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, internalrfrxpu_ovr, 1)
		PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, internalrfrxpu_ovr_val, 1)
		/* Turn PA on */
		PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, gmode_tx_pu_ovr, 1)
		PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, gmode_tx_pu_ovr_val, 1)
	PHY_REG_LIST_EXECUTE(pi);

	/* Set the Tx filter bandwidth to wide */
	write_radio_reg(pi, RADIO_2062_TX_CTRLA_NORTH, 0x80);

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, lpf_bw_ovr, 1)
		PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, lpf_bw_ovr_val, 1)
	PHY_REG_LIST_EXECUTE(pi);

	/* Loop through Rx gains to find a good gain setting where we are not clipping */
	for (gain_index = 0; gain_index < 32; gain_index++) {
		wlc_lpphy_set_rx_gain_by_index(pi, (uint16)gain_index);
		wlc_lpphy_run_ddfs(pi, 1, 1, 5, 5, 0);
		if (!wlc_lpphy_rx_iq_est(pi, 1000, 32, &iq_est))
			continue;
		iq_pwr = (iq_est.i_pwr + iq_est.q_pwr) / 1000;

		if ((iq_pwr > 4000) && (iq_pwr < 10000))  {
			good_gain_index = gain_index;
			PHY_INFORM(("%s: found good Rx gain index 0x%x\n",
				__FUNCTION__, gain_index));
			break;
		}
	}
	wlc_lpphy_stop_ddfs(pi);

	return good_gain_index;
}

static void
wlc_lpphy_rc_cal_rev0(phy_info_t *pi)
{
	bool old_tx_gain_override;
	lpphy_txgains_t old_gains;
	uint8 old_bbmult;
	uint16 old_rfovr0, old_rfovr2, old_afeovr;
	uint16 old_rfovr0val, old_rfovr2val, old_afeovrval;
	uint16 old_rx_ctrl;
	uint16 old_tx_pwr_ctrl;
	int gain_index;
	uint32 iq_pwr, iq_pwr_3mhz = 0;
	uint32 norm_pwr, ideal_pwr;
	uint32 mean_sq, acc_mean_sq, least_mean_sq = 0;
	lpphy_iq_est_t iq_est = {0, 0, 0};
	uint8 cap;
	int ddfs_tone;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;


	ASSERT(CHSPEC_IS2G(pi->radio_chanspec));
	wlc_phy_chanspec_set_lpphy(pi, CH20MHZ_CHSPEC(7));

	/* Save old tx gains */
	old_tx_gain_override = wlc_lpphy_tx_gain_override_enabled(pi);
	old_bbmult = wlc_lpphy_get_bbmult(pi);
	if (old_tx_gain_override)
		wlc_lpphy_get_tx_gain(pi, &old_gains);

	/* Save overrides */
	old_rfovr0 = phy_reg_read(pi, LPPHY_RFOverride0);
	old_rfovr0val = phy_reg_read(pi, LPPHY_RFOverrideVal0);
	old_afeovr = phy_reg_read(pi, LPPHY_AfeCtrlOvr);
	old_afeovrval = phy_reg_read(pi, LPPHY_AfeCtrlOvrVal);
	old_rfovr2 = phy_reg_read(pi, LPPHY_rfoverride2);
	old_rfovr2val = phy_reg_read(pi, LPPHY_rfoverride2val);

	/* Save control settings */
	old_rx_ctrl = phy_reg_read(pi, LPPHY_lpphyCtrl);

	/* Save original tx power control mode */
	old_tx_pwr_ctrl = wlc_lpphy_get_tx_pwr_ctrl(pi);

	/* Disable tx power control */
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_OFF);

	/* Disable CRS detector */
	wlc_lpphy_disable_crs(pi, (bool)1);

	/*
	* Call lpphy_loopback to setup loopback mode and get a good Rx gain
	* setting where we don't clip
	*/
	if ((gain_index = wlc_lpphy_loopback(pi)) == -1) {
		PHY_ERROR(("wl%d: %s: Unable to find proper gain index, skipping RC cal\n",
			pi->sh->unit, __FUNCTION__));
		goto cleanup;
	}
	wlc_lpphy_set_rx_gain_by_index(pi, (uint16)gain_index);

	PHY_REG_LIST_START
		/* Bypass digital RX filter */
		PHY_REG_MOD_ENTRY(LPPHY, lpphyCtrl, rx_filt_bypass, 1)
		/* Fix the hpf and lpf values */
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, hpf1_ctrl_ovr_val, 0x01)
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, hpf2_ctrl_ovr_val, 0x01)
		PHY_REG_MOD_ENTRY(LPPHY, rfoverride2val, lpf_lq_ovr_val, 0x03)
	PHY_REG_LIST_EXECUTE(pi);

	/* Loop through 32 Cap values */
	for (cap = 0x80; cap <= 0x9f; cap++) {
		write_radio_reg(pi, RADIO_2062_RXBB_CAL2_NORTH, cap);

		/* Get IQ power for tones from 3MHz to 15.6MHz using DDFS */
		acc_mean_sq = 0;
		for (ddfs_tone = 5; ddfs_tone <= 25; ddfs_tone++) {
			/* Send Tone and get IQ power */
			wlc_lpphy_run_ddfs(pi, 1, 1, ddfs_tone, ddfs_tone, 0);
			if (!wlc_lpphy_rx_iq_est(pi, 1000, 32, &iq_est)) {
				goto cleanup;
			}
			iq_pwr = iq_est.i_pwr + iq_est.q_pwr;

			/* Save base power */
			if (ddfs_tone == 5)
				iq_pwr_3mhz = iq_pwr;

			/* Normalized power */
			norm_pwr = wlc_phy_qdiv_roundup(iq_pwr, iq_pwr_3mhz, 12);
			/* ddfs_tone 5 corresponds to the first entry in the ideal power table */
			ideal_pwr = (((lpphy_rc_ideal_pwr[ddfs_tone - 5] >> 3) + 1) >> 1);
			/*
			* Calculate difference based on (sqr(IDealIQPwr - CurrentIQPwr))
			* Accumulate this over the entire range and
			* choose the one with the lowest value
			* The one with the lowest value is the closest to the Ideal curve
			*/
			mean_sq = (uint32)ABS((int32)(ideal_pwr - norm_pwr));
			mean_sq *= mean_sq;
			acc_mean_sq += mean_sq;
		}
		PHY_INFORM(("wl%d: %s: cap: 0x%x acc: %d\n",
			pi->sh->unit, __FUNCTION__, cap, acc_mean_sq));
		/* Keep storing the lowest value */
		if ((cap == 0x80) || (acc_mean_sq < least_mean_sq)) {
			pi_lp->lpphy_rc_cap = cap;
			least_mean_sq = acc_mean_sq;
		}
	}
	wlc_lpphy_stop_ddfs(pi);

cleanup:
	/* Restore CRS detector */
	wlc_lpphy_restore_crs(pi, (bool)1);

	/* Restore overrides */
	PHY_REG_WRITE(pi, LPPHY, RFOverrideVal0, old_rfovr0val);
	PHY_REG_WRITE(pi, LPPHY, RFOverride0, old_rfovr0);
	PHY_REG_WRITE(pi, LPPHY, AfeCtrlOvrVal, old_afeovrval);
	PHY_REG_WRITE(pi, LPPHY, AfeCtrlOvr, old_afeovr);
	PHY_REG_WRITE(pi, LPPHY, rfoverride2val, old_rfovr2val);
	PHY_REG_WRITE(pi, LPPHY, rfoverride2, old_rfovr2);

	/* Restore control settings */
	PHY_REG_WRITE(pi, LPPHY, lpphyCtrl, old_rx_ctrl);

	/* Restore tx power settings */
	wlc_lpphy_set_bbmult(pi, old_bbmult);
	if (old_tx_gain_override)
		wlc_lpphy_get_tx_gain(pi, &old_gains);
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, old_tx_pwr_ctrl);

	/* Apply results */
	if (pi_lp->lpphy_rc_cap)
		wlc_lpphy_set_rc_cap(pi, pi_lp->lpphy_rc_cap);
}

static void
wlc_lpphy_rc_cal_rev2(phy_info_t *pi)
{
	uint8 rxbb_sp8, txbb_sp_3;
	uint32 x;

	/* Save old cap value incase RCCal fails */
	rxbb_sp8 = (uint8)read_radio_reg(pi, RADIO_2063_RXBB_SP_8);

	/* Clear the RCCal Reg Override */
	write_radio_reg(pi, RADIO_2063_RXBB_SP_8, 0x0);

	/* Power down RC CAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7e);

	/* Power up PLL_cal_out_pd_0 (bit 4) */
	and_radio_reg(pi, RADIO_2063_PLL_SP_1, 0xf7);

	/* Power Up RC CAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7c);

	/* setup to run RX RC Cal and setup R1/Q1/P1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_2, 0x15);

	/* set X1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_3, 0x70);

	/* scale RC cal params based on xtal frequency */
	x = (13 * PHY_XTALFREQ(pi->xtalfreq)) / 1000000;

	/* set Trc1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, (x & 0xff));

	/* set Trc2 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_5, ((x >> 8) & 0xff));

	/* Start rx RCCAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7d);

	/* Wait for rx RCCAL completion */
	OSL_DELAY(50);
	SPINWAIT(!wlc_radio_2063_rc_cal_done(pi), 10 * 1000 * 1000);

	if (!wlc_radio_2063_rc_cal_done(pi)) {
		PHY_ERROR(("wl%d: %s: Rx RC Cal failed\n", pi->sh->unit, __FUNCTION__));
		write_radio_reg(pi, RADIO_2063_RXBB_SP_8, rxbb_sp8);
	} else
		PHY_INFORM(("wl%d: %s:  Rx RC Cal completed: N0: %x%x, N1: %x%x, code: %x\n",
			pi->sh->unit, __FUNCTION__,
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_8),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_7),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_10),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_9),
			read_radio_reg(pi, RADIO_2063_COMMON_11) & 0x1f));

	/* Save old cap value incase RCCal fails */
	txbb_sp_3 = (uint8)read_radio_reg(pi, RADIO_2063_TXBB_SP_3);

	/* Clear the RCCal Reg Override */
	write_radio_reg(pi, RADIO_2063_TXBB_SP_3, 0x0);

	/* Power down RC CAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7e);

	/* Power Up RC CAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7c);

	/* setup to run TX RC Cal and setup R1/Q1/P1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_2, 0x55);

	/* set X1 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_3, 0x76);

	if (PHY_XTALFREQ(pi->xtalfreq) == 24000000) {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0xfc);
		/* set Trc2 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_5, 0x0);
	} else {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x13);
		/* set Trc2 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_5, 0x1);
	}

	/* Start tx RCCAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7d);

	/* Wait for tx RCCAL completion */
	OSL_DELAY(50);
	SPINWAIT(!wlc_radio_2063_rc_cal_done(pi), 10 * 1000 * 1000);

	if (!wlc_radio_2063_rc_cal_done(pi)) {
		PHY_ERROR(("wl%d: %s: Tx RC Cal failed\n", pi->sh->unit, __FUNCTION__));
		write_radio_reg(pi, RADIO_2063_TXBB_SP_3, txbb_sp_3);
	} else
		PHY_INFORM(("wl%d: %s:  Tx RC Cal completed: N0: %x%x, N1: %x%x, code: %x\n",
			pi->sh->unit, __FUNCTION__,
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_8),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_7),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_10),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_9),
			read_radio_reg(pi, RADIO_2063_COMMON_12) & 0x1f));

	/* Power down RCCAL after it is done */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7e);
}

static void
wlc_lpphy_rc_cal(phy_info_t *pi)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;

	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
		if (pi_lp->lpphy_rc_cap)
			wlc_lpphy_set_rc_cap(pi, pi_lp->lpphy_rc_cap);
		else if (CHSPEC_IS2G(pi->radio_chanspec))
			wlc_lpphy_rc_cal_rev0(pi);
	} else
		wlc_lpphy_rc_cal_rev2(pi);
}

/*
* RX IQ Calibration
*/
static bool
wlc_lpphy_rx_iq_cal(phy_info_t *pi, const lpphy_rx_iqcomp_t *iqcomp, int iqcomp_sz,
	bool use_tone, bool tx_switch, bool rx_switch, bool pa, lpphy_txgains_t *tx_gain,
	lpphy_rx_iq_cal_data_for_temp_adj_t *p_temp_adj_data)
{

	bool result = FALSE;
	uint32 SAVE_papd_on;
	uint32 ps;
	uint32 gain;
	lpphy_tx_pwr_state tx_pwr_state;
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lpphy_calcache_t *cache = &ctx->u.lpphy_cache;
#else
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
#endif

	if ((CHIPID(pi->sh->chip) != BCM4325_CHIP_ID) &&
	    (CHIPID(pi->sh->chip) != BCM4315_CHIP_ID))
	{
		ASSERT(iqcomp_sz);
		while (iqcomp_sz--) {
			if (iqcomp[iqcomp_sz].chan == CHSPEC_CHANNEL(pi->radio_chanspec)) {
				/* Apply new coeffs */
				PHY_REG_MOD(pi, LPPHY_REV2, RxCompcoeff, c1,
					(uint16)iqcomp[iqcomp_sz].c1);
				PHY_REG_MOD(pi, LPPHY_REV2, RxCompcoeff, c0,
					(uint16)iqcomp[iqcomp_sz].c0);
				result = TRUE;
				break;
			}
		}
		ASSERT(result);
	} else {
		/* do calibration using loop back tone */

		/* Turn off tx pwr ctrl */
		/* This is already done in the parent function */


		/* Turn off papd */
		SAVE_papd_on = PHY_REG_READ(pi, LPPHY_REV2, papdctrl, papdcompEn);
		PHY_REG_MOD(pi, LPPHY_REV2, papdctrl, papdcompEn, 0);

		/* Setup Tx Gains */
		wlc_lpphy_tx_pwr_state_save(pi, &tx_pwr_state);
		wlc_phy_set_tx_pwr_by_index_lpphy(pi, 127);

		/* turn on TX chain
		 */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverride0, gmode_tx_pu_ovr, 1)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverrideVal0,
					gmode_tx_pu_ovr_val, 1)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverride0, amode_tx_pu_ovr, 1)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverrideVal0,
					amode_tx_pu_ovr_val, 1)
			PHY_REG_LIST_EXECUTE(pi);
		}

		PHY_REG_LIST_START
			/* PA driver override */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val, stxpadpu2g_ovr_val, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpadpu2g_ovr, 1)
			/* PA override */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val, stxpapu_ovr_val, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpapu_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);

		/* Set T/R switch to TX for extra isolation on receiver
		 */
		tx_switch = 1;
		rx_switch = 0;

		/* Use 2nd path to avoid SLNA issues. Only power on 2nd stage LNA in
		 * 2nd path
		 */
		ps = 0x00e08e38;

		PHY_REG_WRITE(pi, LPPHY, psctrlovrval0, (uint16)(ps & 0xffff));
		PHY_REG_WRITE(pi, LPPHY, psctrlovrval1, (uint16)((ps >> 16) & 0xff));

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2val, slna_pu_ovr_val, 0)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFinputOverrideVal,
					wlslnapu_ovr_val, 0)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, rxlnaandgainctrl1ovrval,
					lnapuovr_Val, 0x50)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2val, slna_pu_ovr_val, 0)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFinputOverrideVal,
					wlslnapu_ovr_val, 0)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, rxlnaandgainctrl1ovrval,
					lnapuovr_Val, 0x0a)
			PHY_REG_LIST_EXECUTE(pi);
		}

		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LPPHY_REV2, RFinputOverride, wlslnapu_ovr, 0x1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, slna_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, lna_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, ps_ctrl_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			PHY_REG_MOD(pi, LPPHY_REV2, rxlnaandgainctrl1ovrval, lnapuovr_Val, 0x40);
			write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1, 0x4c);
			write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2, 0);
		} else {
			PHY_REG_MOD(pi, LPPHY_REV2, rxlnaandgainctrl1ovrval, lnapuovr_Val, 0x08);
			write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1, 0);
			write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2, 0x4c);
		}

		/* Max out gains before mixer
		 */
		/* TCL proc - lpphy_rx_gain_override 0 0 0 0 7 3 0 */
		/* tia = 7, lna2 = 3 lna1 = 0 */
		gain = (7 << 8) | (3 << 4) | (3 << 6);
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2val,
					slna_gain_ctrl_ovr_val, 0)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFinputOverrideVal,
					wlslnagainctrl_ovr_val, 0)
			PHY_REG_LIST_EXECUTE(pi);
		}
		PHY_REG_MOD(pi, LPPHY_REV2, rxgainctrl0ovrval, rxgainctrl_ovr_val0, gain);
		PHY_REG_MOD(pi, LPPHY_REV2, rxlnaandgainctrl1ovrval, rxgainctrl_ovr_val1, 0);
		wlc_lpphy_rx_gain_override_enable(pi, TRUE);

		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, gmode_ext_lna_gain_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, amode_ext_lna_gain_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);

		/* crs disable */
		wlc_phy_set_deaf_lpphy(pi, (bool)0);

		/* Set TR switch */
		PHY_REG_MOD(pi, LPPHY_REV2, RFOverride0, trsw_tx_pu_ovr, 1);
		PHY_REG_MOD(pi, LPPHY_REV2, RFOverrideVal0, trsw_tx_pu_ovr_val, tx_switch);
		PHY_REG_MOD(pi, LPPHY_REV2, RFOverride0, trsw_rx_pu_ovr, 1);
		PHY_REG_MOD(pi, LPPHY_REV2, RFOverrideVal0, trsw_rx_pu_ovr_val, rx_switch);

		PHY_REG_LIST_START
			/* Force ADC on */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, AfeCtrlOvr, pwdn_adc_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, AfeCtrlOvrVal, pwdn_adc_ovr_val, 0)
			/* Force Rx on	 */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverride0, internalrfrxpu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverrideVal0, internalrfrxpu_ovr_val, 1)
		PHY_REG_LIST_EXECUTE(pi);

		/* Run calibration */
		wlc_phy_start_tx_tone_lpphy(pi, 4000, 100);


		result = wlc_lpphy_calc_rx_iq_comp(pi, 512, p_temp_adj_data);


		wlc_phy_stop_tx_tone_lpphy(pi);

		PHY_REG_LIST_START
			/* Rx on/off */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverride0, internalrfrxpu_ovr, 0)
			/* ADC on/off */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, AfeCtrlOvr, pwdn_adc_ovr, 0)
		PHY_REG_LIST_EXECUTE(pi);

		/* Restore TR switch */
		wlc_lpphy_clear_trsw_override(pi);

		/* return from deaf */
		wlc_phy_clear_deaf_lpphy(pi, (bool)0);

		/* restore rx gain settings */
		wlc_lpphy_rx_gain_override_enable(pi, FALSE);

		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, gmode_ext_lna_gain_ovr, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, amode_ext_lna_gain_ovr, 0)
			/* RX LNA PUs */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, RFinputOverride, wlslnapu_ovr, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, slna_pu_ovr, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, lna_pu_ovr, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, ps_ctrl_ovr, 0)
		PHY_REG_LIST_EXECUTE(pi);

		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1, 0);
		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2, 0);

		PHY_REG_LIST_START
			/* restore PA */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpapu_ovr, 0)
			/* restore PA driver */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpadpu2g_ovr, 0)
			/* TX PU */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverride0, gmode_tx_pu_ovr, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverride0, amode_tx_pu_ovr, 0)
		PHY_REG_LIST_EXECUTE(pi);

		/* Restore Tx gain */
		wlc_lpphy_tx_pwr_state_restore(pi, &tx_pwr_state);

		/* restore papd */
		PHY_REG_MOD(pi, LPPHY_REV2, papdctrl, papdcompEn, (uint16)SAVE_papd_on);

		/* restore tx pwr ctrl */
		/* This will be done in the parent function wlc_phy_periodic_cal_lpphy() */
	}

	/* Save rx iq cal results */
	/* RX IQ cal results */
#if defined(PHYCAL_CACHING)
	cache->rxiqcal_coeffs = phy_reg_read(pi, LPPHY_RxCompcoeff);
#else
	pi_lp->lpphy_cal_results.rxiqcal_coeffs = phy_reg_read(pi, LPPHY_RxCompcoeff);
#endif

	PHY_INFORM(("wl%d: %s: Rx IQ cal complete, coeffs: A: %d, C: %d\n",
		pi->sh->unit, __FUNCTION__,
		(int8)PHY_REG_READ(pi, LPPHY, RxCompcoeff, c1),
		(int8)PHY_REG_READ(pi, LPPHY, RxCompcoeff, c0)));

		return result;
}


/* Check if IQLO is currently broken due to misaligned ADC/RSSI ADC clocks */
static bool
wlc_lpphy_tx_iqlo_functional(phy_info_t *pi)
{
	lpphy_txgains_t old_gains;
	lpphy_txgains_t new_gains = {6, 12, 12, 5};
	uint8 old_bbmult;
	uint16 a, b;
	uint16 ripple_bin;
	uint16 old_genv;
	lpphytbl_info_t tab;

	wlc_lpphy_get_tx_gain(pi, &old_gains);
	wlc_phy_get_tx_iqcc_lpphy(pi, &a, &b);
	old_bbmult = wlc_lpphy_get_bbmult(pi);
	old_genv = wlc_lpphy_get_genv(pi);

	/* Set gains and IQ imbalance to ensure we have a measurable ripple */
	wlc_lpphy_set_tx_gain(pi, &new_gains);
	wlc_lpphy_set_genv(pi, 7);
	wlc_lpphy_set_bbmult(pi, 64);
	wlc_phy_set_tx_iqcc_lpphy(pi, 500, 500);

	PHY_REG_LIST_START
		PHY_REG_WRITE_ENTRY(LPPHY, iqloCalCmdNnum, 0x7877)
		PHY_REG_WRITE_ENTRY(LPPHY, iqloCalCmdGctl, 0x0aa9)
	PHY_REG_LIST_EXECUTE(pi);

	wlc_phy_set_deaf_lpphy(pi, (bool)0);
	wlc_phy_start_tx_tone_lpphy(pi, 2500, 100);

	PHY_REG_LIST_START
		PHY_REG_OR_ENTRY(LPPHY, iqloCalCmdGctl, LPPHY_iqloCalCmdGctl_iqlo_cal_en_MASK)
		PHY_REG_WRITE_ENTRY(LPPHY, iqloCalCmd, 0x4000)
	PHY_REG_LIST_EXECUTE(pi);

	if (!wlc_lpphy_iqcal_wait(pi)) {
		PHY_ERROR(("wl%d: %s: iqcal did not complete\n", pi->sh->unit, __FUNCTION__));
	}

	PHY_REG_AND(pi, LPPHY, iqloCalCmdGctl, (uint16)~LPPHY_iqloCalCmdGctl_iqlo_cal_en_MASK);

	tab.tbl_ptr = &ripple_bin;
	tab.tbl_len = 1;
	tab.tbl_id = 0;
	tab.tbl_offset = 107;
	tab.tbl_width = 16;
	tab.tbl_phywidth = 16;	/* width of the phy table element address space */
	wlc_phy_table_read_lpphy(pi, &tab);
	PHY_INFORM(("wl%d: %s: ripple_bin: %x\n", pi->sh->unit, __FUNCTION__, ripple_bin));

	/* Restore original settings */
	wlc_lpphy_set_tx_gain(pi, &old_gains);
	wlc_phy_set_tx_iqcc_lpphy(pi, a, b);
	wlc_lpphy_set_bbmult(pi, old_bbmult);
	wlc_lpphy_set_genv(pi, old_genv);

	return (ripple_bin != 0);
}


static void
wlc_lpphy_smooth_papd(phy_info_t *pi, uint32 winsz, uint32 start, uint32 end)
{
	uint32 *buf, *src, *dst, sz;
	lpphytbl_info_t tab;

	sz = end - start + 1;
	ASSERT(end > start);
	ASSERT(end < LPPHY_PAPD_EPS_TBL_SIZE);

	/* Allocate storage for both source & destination tables */
	if ((buf = MALLOC(pi->sh->osh, 2 * sizeof(uint32) * LPPHY_PAPD_EPS_TBL_SIZE)) == NULL) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return;
	}

	/* Setup source & destination pointers */
	src = buf;
	dst = buf + LPPHY_PAPD_EPS_TBL_SIZE;

	/* Preset PAPD eps table */
	tab.tbl_len = LPPHY_PAPD_EPS_TBL_SIZE;
	tab.tbl_id = LPPHY_TBL_ID_PAPD_EPS;
	tab.tbl_offset = 0;
	tab.tbl_width = 32;
	tab.tbl_phywidth = 32;

	/* Read original table */
	tab.tbl_ptr = src;
	wlc_phy_table_read_lpphy(pi, &tab);

	/* Average coeffs across window */
	do {
		uint32 win_start, win_end;
		int32 nAvr, eps, eps_real, eps_img;

		win_start = end - MIN(end, (winsz >> 1));
		win_end = MIN(LPPHY_PAPD_EPS_TBL_SIZE - 1, end + (winsz >> 1));
		nAvr = win_end - win_start + 1;
		eps_real = 0;
		eps_img = 0;

		do {
			if ((eps = (src[win_end] >> 12)) > 0x7ff)
				eps -= 0x1000; /* Sign extend */
			eps_real += eps;

			if ((eps = (src[win_end] & 0xfff)) > 0x7ff)
				eps -= 0x1000; /* Sign extend */
			eps_img += eps;
		} while (win_end-- != win_start);

		eps_real /= nAvr;
		eps_img /= nAvr;
		dst[end] = ((uint32)eps_real << 12) | ((uint32)eps_img & 0xfff);
	} while (end-- != start);

	/* Write updated table */
	tab.tbl_ptr = dst;
	tab.tbl_len = sz;
	tab.tbl_offset = start;
	wlc_phy_table_write_lpphy(pi, &tab);

	/* Free allocated buffer */
	MFREE(pi->sh->osh, buf, 2 * sizeof(uint32) * LPPHY_PAPD_EPS_TBL_SIZE);
}

/* get/set tx dig filt coeffs */
void
wlc_phy_tx_dig_filt_lpphy_rev2(phy_info_t *pi, bool bset, uint16 *coeffs)
{
	int j;
	uint16 addr[] = {
		LPPHY_REV2_txfiltCoeffStg0A1,
		LPPHY_REV2_txfiltCoeffStg1A1,
		LPPHY_REV2_txfiltCoeffStg2A1,
		LPPHY_REV2_txfiltCoeffStg0B0,
		LPPHY_REV2_txfiltCoeffStg0B2,
		LPPHY_REV2_txfiltCoeffStg1B0,
		LPPHY_REV2_txfiltCoeffStg1B2,
		LPPHY_REV2_txfiltCoeffStg2B0,
		LPPHY_REV2_txfiltCoeffStg2B2,
		};

	for (j = 0; j < LPPHY_REV2_NUM_DIG_FILT_COEFFS; j++) {
		if (bset) {
			phy_reg_write(pi, addr[j], coeffs[j]);
		} else {
			coeffs[j] = phy_reg_read(pi, addr[j]);
		}
	}
}


/* setup cck digital filter coefficients */
void
wlc_phy_tx_dig_filt_cck_setup_lpphy(phy_info_t *pi, bool set_now)
{
	int filt_index;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if ((pi_lp->lpphy_cck_dig_filt_type >= 0) &&
		(pi_lp->lpphy_cck_dig_filt_type < LPPHY_REV2_NUM_TX_DIG_FILTERS_CCK)) {
		filt_index = pi_lp->lpphy_cck_dig_filt_type;
	} else if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_CCKFAVOREVM) {
		filt_index = 1;
	} else {
		filt_index = 0;
	}

	wlc_phy_tx_dig_filt_ucode_set_lpphy_rev2(pi, TRUE,
		LPPHY_REV2_txdigfiltcoeffs_cck[filt_index]);

	if (set_now) {
		wlc_phy_tx_dig_filt_lpphy_rev2(pi, TRUE,
			LPPHY_REV2_txdigfiltcoeffs_cck[filt_index]);
	}
}

/* setup ofdm digital filter coefficients */
void
wlc_phy_tx_dig_filt_ofdm_setup_lpphy(phy_info_t *pi, bool set_now)
{
	int filt_index;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if ((pi_lp->lpphy_ofdm_dig_filt_type >= 0) &&
		(pi_lp->lpphy_ofdm_dig_filt_type < LPPHY_REV2_NUM_TX_DIG_FILTERS_OFDM)) {
		filt_index = pi_lp->lpphy_ofdm_dig_filt_type;
	} else {
		filt_index = 1;
	}

	wlc_phy_tx_dig_filt_ucode_set_lpphy_rev2(pi, FALSE,
		LPPHY_REV2_txdigfiltcoeffs_ofdm[filt_index]);

	if (set_now) {
		wlc_phy_tx_dig_filt_lpphy_rev2(pi, TRUE,
			LPPHY_REV2_txdigfiltcoeffs_ofdm[filt_index]);
	}
}


/* Set the digital filter coeffs set by the ucode based on packet type */
void
wlc_phy_tx_dig_filt_ucode_set_lpphy_rev2(phy_info_t *pi, bool iscck, uint16 *coeffs)
{
	uint16 addr;
	int j;

	/* make sure ucode is suspended */
	ASSERT(!(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));

	if ((addr = 2*wlapi_bmac_read_shm(pi->sh->physhim, M_PHY_TX_FLT_PTR)) == 0)
		return;

	if (!iscck)
		addr += 2*LPPHY_REV2_NUM_DIG_FILT_COEFFS;

	for (j = 0; j < LPPHY_REV2_NUM_DIG_FILT_COEFFS; j++) {
		wlapi_bmac_write_shm(pi->sh->physhim, addr + 2*j, coeffs[j]);
	}
}

typedef struct papd_restore_state_t {
	uint16 pa_sp1_old;
	uint16 rxbb_ctrl2_old;
	uint8 bb_mult;
} papd_restore_state;

static void
wlc_lpphy_papd_cal_setup(phy_info_t *pi, papd_restore_state *state)
{
	uint32 rx_gain;
	int32 tone_freq;

	/* Disable CRS */
	wlc_phy_set_deaf_lpphy(pi, (bool)0);

	/* Force WLAN antenna */
	wlc_lpphy_btcx_override_enable(pi);

	/* Set PA filter bias (since ucode switches it based on packet type */
	if (LPREV_GE(pi->pubpi.phy_rev, 3)) {
		write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0x11);
	}

	/* Set Rx path mux to PAPD and turn on PAPD mixer */
	state->pa_sp1_old = read_radio_reg(pi, RADIO_2063_PA_SP_1);
	state->rxbb_ctrl2_old = read_radio_reg(pi, RADIO_2063_RXBB_CTRL_2);

	mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, (3 << 3), (uint8)(2 << 3));
	/* turn on PAPD mixer */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		mod_radio_reg(pi, RADIO_2063_PA_SP_1, (3 << 4), (uint8)(2 << 4));
	} else {
		mod_radio_reg(pi, RADIO_2063_PA_SP_1, (3 << 4), (uint8)(1 << 4));
	}

	/* PAPD feedback atten */
	mod_radio_reg(pi, RADIO_2063_PA_CTRL_1, 3, (uint8)(3));

	PHY_REG_LIST_START
		/* Force rx PU on */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverride0, internalrfrxpu_ovr, 1)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverrideVal0, internalrfrxpu_ovr_val, 1)
	PHY_REG_LIST_EXECUTE(pi);

	/* Force rx gain */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		rx_gain = 0x30000;
	} else {
		if (LPREV_GE(pi->pubpi.phy_rev, 3)) {
			rx_gain = 0x20000;
		} else {
			rx_gain = 0x00000;
		}
	}
	wlc_lpphy_set_rx_gain(pi, rx_gain);

	PHY_REG_LIST_START
		/* Rx HPF */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2val, hpf1_ctrl_ovr_val, 0x7)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2val, hpf2_ctrl_ovr_val, 0x7)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, hpf1_ctrl_ovr, 1)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, hpf2_ctrl_ovr, 1)
	PHY_REG_LIST_EXECUTE(pi);

	/* Set TR switch to transmit */
	wlc_lpphy_set_trsw_override(pi, TRUE, FALSE);

	PHY_REG_LIST_START
		/* Force ADC on */
		PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvr, pwdn_adc_ovr, 1)
		PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvrVal, pwdn_adc_ovr_val, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Force bbmult */
	state->bb_mult = wlc_lpphy_get_bbmult(pi);
	wlc_lpphy_set_bbmult(pi, 23);

	PHY_REG_LIST_START
		/* Shut off PA */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val, stxpapu_ovr_val, 0)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpapu_ovr, 1)
	PHY_REG_LIST_EXECUTE(pi);

	/* start tone */
	if ((LPREV_GE(pi->pubpi.phy_rev, 4) ||
	     (LPREV_GE(pi->pubpi.phy_rev, 3) &&
	      (pi->sh->chiprev >= 3))) &&
	    (CHSPEC_CHANNEL(pi->radio_chanspec) <= 5)) {
		tone_freq = -3750;
	} else {
		tone_freq = 3750;
	}
	wlc_phy_start_tx_tone_lpphy(pi, tone_freq, 100);

	PHY_REG_LIST_START
		/* PAPD regs */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, papdctrl, papdcompEn, 1)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, papdScalar, indexScalar, 8)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
wlc_lpphy_papd_cal_cleanup(phy_info_t *pi, papd_restore_state *state)
{
	/* TR switch */
	wlc_lpphy_clear_trsw_override(pi);

	/* Restore rx path mux and turn off PAPD mixer */
	write_radio_reg(pi, RADIO_2063_PA_SP_1, state->pa_sp1_old);
	write_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, state->rxbb_ctrl2_old);

	/* Clear rx PU override */
	PHY_REG_MOD(pi, LPPHY_REV2, RFOverride0, internalrfrxpu_ovr, 0);

	/* Clear rx gain override */
	wlc_lpphy_rx_gain_override_enable(pi, FALSE);

	PHY_REG_LIST_START
		/* Rx HPF */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, hpf1_ctrl_ovr, 0)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride2, hpf2_ctrl_ovr, 0)
		/* Clear ADC override */
		PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvr, pwdn_adc_ovr, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Stop tone, restore CRS */
	wlc_phy_stop_tx_tone_lpphy(pi);
	wlc_phy_clear_deaf_lpphy(pi, (bool)0);

	wlc_lpphy_set_bbmult(pi, state->bb_mult);

	PHY_REG_MOD(pi, LPPHY_REV2, rfoverride3, stxpapu_ovr, 0);
}

static void
wlc_lpphy_papd_cal(phy_info_t *pi, lpphy_txcalgains_t *txgains, phy_cal_mode_t cal_mode)
{
	uint16 rfpowerOvrVal;
	lpphytbl_info_t tab;
	uint32 val;
	uint32 j;
	uint16 startindex, stopindex;
	bool swcal;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	ASSERT((cal_mode == CAL_FULL) || (cal_mode == CAL_GCTRL) || (cal_mode == CAL_SOFT));

	swcal = ((cal_mode == CAL_GCTRL) || (cal_mode == CAL_SOFT)) ? TRUE : FALSE;

	PHY_CAL(("Running papd cal, channel: %d, slow cal: %d\n",
		CHSPEC_CHANNEL(pi->radio_chanspec),
		pi_lp->lpphy_papd_slow_cal));

	/* Not supported below rev2 */
	if (LPREV_LT(pi->pubpi.phy_rev, 2))
		return;

	/* Set tx gain */
	if (txgains) {
		if (txgains->useindex) {
			wlc_phy_set_tx_pwr_by_index_lpphy(pi, txgains->index);
		} else {
			wlc_lpphy_set_tx_gain(pi, &txgains->gains);
		}
	}

	/* Force bbmult */
	wlc_lpphy_set_bbmult(pi, 23);

	stopindex = 63;
	if (cal_mode == CAL_FULL) {
		startindex = 10;
	} else if (cal_mode == CAL_SOFT) {
		startindex = 20;
	} else {		/* (cal_mode == CAL_GCTRL) */
		startindex = 63;
	}
	PHY_REG_MOD(pi, LPPHY_REV2, papdctrl, caltype, (swcal ? 1 : 0));
	PHY_REG_MOD(pi, LPPHY_REV2, papdstartstopIndex, startIndex, startindex);
	PHY_REG_MOD(pi, LPPHY_REV2, papdstartstopIndex, stopIndex, stopindex);
	if ((cal_mode == CAL_FULL) || (cal_mode == CAL_SOFT)) {
		rfpowerOvrVal = PHY_REG_READ(pi, LPPHY_REV2, papdrfpowerOvrVal, rfpowerOvrVal);
		PHY_REG_MOD(pi, LPPHY_REV2, papdScalar, indexOffset, -100 + (76 - rfpowerOvrVal)/2);
	}

	if (pi_lp->lpphy_papd_slow_cal) {
		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, papdNSampMeasure, (32 * 256))
			PHY_REG_MOD_ENTRY(LPPHY_REV2, papdrfpowerOvrVal, bitShiftCorrelator, 8)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, papditerpergain, iterpergain, 127)
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, papdNSampSettle, 512)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, papditerpergain, bitshiftepsCalc, 0)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, papdNSampMeasure, 128)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, papdrfpowerOvrVal, bitShiftCorrelator, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, papditerpergain, iterpergain, 32)
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, papdNSampSettle, 16)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, papditerpergain, bitshiftepsCalc, 3)
		PHY_REG_LIST_EXECUTE(pi);
	}
	if (cal_mode == CAL_GCTRL) {
		PHY_REG_MOD(pi, LPPHY_REV2, papditerpergain, iterpergain, 64);
	}

	if (!swcal) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val, stxpapu_ovr_val, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, papdctrl, runCal, 1)
		PHY_REG_LIST_EXECUTE(pi);

		/* Wait for completion */
		SPINWAIT(
			phy_reg_read(pi, LPPHY_REV2_papdctrl) & LPPHY_REV2_papdctrl_runCal_MASK,
			10 * 1000 * 1000);

		PHY_REG_MOD(pi, LPPHY_REV2, rfoverride3_val, stxpapu_ovr_val, 0);
	} else {	/* software/single-measurement based cal */
		int iter, num_iter;
		int papd_idx;
		int16 Yi, Yq;
		int16 Y0i, Y0q;
		int32 eps_i, eps_q; /* q15 */
		int32 err_i, err_q;
		int16 bs_eps_calc;
		uint32 eps_table[64];
		int32 eps_table_i[64], eps_table_q[64];
		uint16 k_fracbits_epsilon_register = 11;
		uint16 k_fracbits_epsilon_sw = 15;
		uint16 k_bs_epsilon = k_fracbits_epsilon_sw - k_fracbits_epsilon_register;

		PHY_REG_WRITE(pi, LPPHY_REV2, papdNSampMeasure, 64);
		bs_eps_calc = 9;
		if (cal_mode == CAL_GCTRL)
			num_iter = 24;
		else
			num_iter = 12;

		/* set papd_mult */
		tab.tbl_id = LPPHY_TBL_ID_PAPD_MULT;
		tab.tbl_width = 32;
		tab.tbl_phywidth = 32;	/* width of the table element in the phy address space */
		tab.tbl_ptr = &val;
		tab.tbl_len = 1;
		tab.tbl_offset = 32;
		wlc_phy_table_read_lpphy(pi, &tab);
		PHY_REG_MOD(pi, LPPHY_REV2, papdMult, papd_mult, val & 0xfff);
		PHY_REG_MOD(pi, LPPHY_REV2, papdCorrScalar, corrScalar, (val >> 12) & 0xfff);

		PHY_REG_LIST_START
			/* measure Y0 */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, papdctrl, epsilonOverride, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, epsilonIOverVal, epsilonOverrideI, 0)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, epsilonQOverVal, epsilonOverrideQ, 0)
			PHY_REG_WRITE_ENTRY(LPPHY_REV2, papdNSampSettle, 40)
		PHY_REG_LIST_EXECUTE(pi);

		for (j = 0; j < 5; j++) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val, stxpapu_ovr_val, 1)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, papdctrl, runCal, 1)
			PHY_REG_LIST_EXECUTE(pi);
			while (PHY_REG_READ(pi, LPPHY_REV2, papdctrl, runCal))
				OSL_DELAY(1);
			PHY_REG_MOD(pi, LPPHY_REV2, rfoverride3_val, stxpapu_ovr_val, 0);

			OSL_DELAY(7);
		}

		Y0i = PHY_REG_READ(pi, LPPHY_REV2, papdcorrResultI, corrI);
		Y0q = PHY_REG_READ(pi, LPPHY_REV2, papdcorrResultQ, corrQ);

		/* Read existing PAPD table (for extrapolation) */
		tab.tbl_id = LPPHY_TBL_ID_PAPD_EPS;
		tab.tbl_width = 32;
		tab.tbl_phywidth = 32; /* width of phy table element address space */
		tab.tbl_ptr =  &eps_table[MAX(startindex - 2, 0)];
		tab.tbl_len = MIN(startindex, 2);
		tab.tbl_offset = MAX(startindex - 2, 0);
		wlc_phy_table_read_lpphy(pi, &tab);
		for (papd_idx = startindex - 2; papd_idx < startindex; papd_idx++) {
			if ((eps_table_i[papd_idx] = (eps_table[papd_idx] >> 12)) > 0x7ff)
				eps_table_i[papd_idx] -= 0x1000; /* Sign extend */
			if ((eps_table_q[papd_idx] = (eps_table[papd_idx])) > 0x7ff)
				eps_table_q[papd_idx] -= 0x1000; /* Sign extend */
		}

		eps_i = 0;
		eps_q = 0;
		for (papd_idx = startindex; papd_idx <= stopindex; papd_idx++) {

			/* set papd_mult */
			tab.tbl_id = LPPHY_TBL_ID_PAPD_MULT;
			tab.tbl_width = 32;
			tab.tbl_phywidth = 32;
			tab.tbl_ptr = &val;
			tab.tbl_len = 1;
			tab.tbl_offset = papd_idx;
			wlc_phy_table_read_lpphy(pi, &tab);
			PHY_REG_MOD(pi, LPPHY_REV2, papdMult, papd_mult, val & 0xfff);
			PHY_REG_MOD(pi, LPPHY_REV2, papdCorrScalar, corrScalar,
				(val >> 12) & 0xfff);

			/* extrapolate epsilon based on previous two points */
			if ((cal_mode != CAL_GCTRL) && (papd_idx >= 40)) {
				if (papd_idx >= 2) {
					eps_i = (2*eps_table_i[papd_idx - 1] -
						eps_table_i[papd_idx - 2]) << k_bs_epsilon;
					eps_q = (2*eps_table_q[papd_idx - 1] -
						eps_table_q[papd_idx - 2]) << k_bs_epsilon;
				}
			}

			for (iter = 0; iter < num_iter; iter++) {

				eps_i = MIN(eps_i, 1 << k_fracbits_epsilon_sw);
				eps_i = MAX(eps_i, -1 << k_fracbits_epsilon_sw);
				eps_q = MIN(eps_q, 1 << k_fracbits_epsilon_sw);
				eps_q = MAX(eps_q, -1 << k_fracbits_epsilon_sw);

				PHY_REG_MOD(pi, LPPHY_REV2, epsilonIOverVal,
					epsilonOverrideI, eps_i >> k_bs_epsilon);
				PHY_REG_MOD(pi, LPPHY_REV2, epsilonQOverVal,
					epsilonOverrideQ, eps_q >> k_bs_epsilon);

				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val,
						stxpapu_ovr_val, 1)
					PHY_REG_MOD_ENTRY(LPPHY_REV2, papdctrl, runCal, 1)
				PHY_REG_LIST_EXECUTE(pi);

				while (PHY_REG_READ(pi, LPPHY_REV2, papdctrl, runCal))
					OSL_DELAY(1);
				PHY_REG_MOD(pi, LPPHY_REV2, rfoverride3_val, stxpapu_ovr_val, 0);

				Yi = PHY_REG_READ(pi, LPPHY_REV2, papdcorrResultI, corrI);
				Yq = PHY_REG_READ(pi, LPPHY_REV2, papdcorrResultQ, corrQ);

				err_i = -(Y0i*Yi + Y0q*Yq - Y0i*Y0i - Y0q*Y0q);
				err_q = -(Y0q*Yi - Y0i*Yq);

				eps_i = qm_sat32(eps_i + (err_i >> bs_eps_calc));
				eps_q = qm_sat32(eps_q + (err_q >> bs_eps_calc));
			}

			eps_table_i[papd_idx] = eps_i >> k_bs_epsilon;
			eps_table_q[papd_idx] = eps_q >> k_bs_epsilon;
			eps_table[papd_idx] = ((uint32)(eps_i >> k_bs_epsilon) << 12) |
				((uint32) (eps_q >> k_bs_epsilon) & 0xfff);
		}

		PHY_REG_MOD(pi, LPPHY_REV2, papdctrl, epsilonOverride, 0);

		/* write eps table */
		tab.tbl_id = LPPHY_TBL_ID_PAPD_EPS;
		tab.tbl_width = 32;
		tab.tbl_phywidth = 32;	/* width of table element in phy address space */
		tab.tbl_ptr = &eps_table[startindex];
		tab.tbl_len = stopindex - startindex + 1;
		tab.tbl_offset = startindex;
		wlc_phy_table_write_lpphy(pi, &tab);
	}


	ASSERT((phy_reg_read(pi, LPPHY_REV2_papdctrl) & LPPHY_REV2_papdctrl_runCal_MASK) == 0);
	PHY_CAL(("wl%d: %s: PAPD cal completed: %d + j * %d\n",
		pi->sh->unit, __FUNCTION__,
		phy_reg_read(pi, LPPHY_REV2_papdcorrResultI),
		phy_reg_read(pi, LPPHY_REV2_papdcorrResultQ)));

	if ((cal_mode == CAL_FULL) || (cal_mode == CAL_SOFT)) {
		/* Copy epsilon value from start index to all lower values */
		tab.tbl_id = 9;			/* papdEpstbl */
		tab.tbl_width = 32;
		tab.tbl_phywidth = 32;
		tab.tbl_ptr = &val;
		tab.tbl_len = 1;
		tab.tbl_offset = startindex;
		wlc_phy_table_read_lpphy(pi, &tab);
		for (j = 0; j < startindex; j++) {
			tab.tbl_offset = j;
			wlc_phy_table_write_lpphy(pi, &tab);
		}

		/* Average coeffs */
		if (cal_mode == CAL_SOFT) {
			wlc_lpphy_smooth_papd(pi, 5, 0, 32);
		}
	}

	if (((cal_mode == CAL_FULL) || (cal_mode == CAL_SOFT)) && PHY_CAL_ON()) {
		/* Dump epsilon table */
		tab.tbl_len = 1;
		tab.tbl_id = LPPHY_TBL_ID_PAPD_EPS;
		tab.tbl_offset = 0;
		tab.tbl_width = 32;
		tab.tbl_phywidth = 32;
		tab.tbl_ptr = &val;

		PHY_CAL(("papd eps table: "));
		for (j = 0; j < LPPHY_PAPD_EPS_TBL_SIZE; j++) {
			int32 eps_real, eps_imag;

			tab.tbl_offset = j;
			wlc_phy_table_read_lpphy(pi, &tab);
			wlc_lpphy_papd_decode_epsilon(val, &eps_real, &eps_imag);
			PHY_CAL(("{%d %d} ", eps_real, eps_imag));
		}
		PHY_CAL(("\n"));
	}
}

/* Convert epsilon table value to complex number */
static void
wlc_lpphy_papd_decode_epsilon(uint32 epsilon, int32 *eps_real, int32 *eps_imag)
{
	if ((*eps_real = (epsilon>>12)) > 0x7ff)
		*eps_real -= 0x1000; /* Sign extend */
	if ((*eps_imag = (epsilon & 0xfff)) > 0x7ff)
		*eps_imag -= 0x1000; /* Sign extend */
}

/* Find suitable gain index at which to run papd cal */
/* returns gain index */
static uint8
wlc_lpphy_papd_cal_gctrl(phy_info_t *pi, uint8 start_gain)
{
	int gain_step = 5;
	int max_iter = 20;
	lpphytbl_info_t tab;
	bool clipping;
	bool prev_clipping = FALSE;
	bool first_pass = TRUE;
	lpphy_txcalgains_t txgains;
	int32 eps_real, eps_imag;
	uint32 val;
	int j;
	bool done = FALSE;
	int gain_index;

	txgains.useindex = TRUE;
	gain_index = start_gain;

	/* run cal once and discard result, to avoid problem with bad result on first cal */
	txgains.index = (uint8) gain_index;
	wlc_lpphy_papd_cal(pi, &txgains, CAL_GCTRL);

	for (j = 0; j < max_iter; j++) {
		txgains.index = (uint8) gain_index;
		wlc_lpphy_papd_cal(pi, &txgains, CAL_GCTRL);

		tab.tbl_len = 1;
		tab.tbl_id = LPPHY_TBL_ID_PAPD_EPS;
		tab.tbl_offset = 63;
		tab.tbl_width = 32;
		tab.tbl_phywidth = 32;
		tab.tbl_ptr = &val;
		wlc_phy_table_read_lpphy(pi, &tab);

		wlc_lpphy_papd_decode_epsilon(val, &eps_real, &eps_imag);

		PHY_CAL(("papd_gctrl: gain: %d epsilon: %d + j*%d\n",
			gain_index, eps_real, eps_imag));

		clipping = ((eps_real == 2047) || (eps_real == -2048) ||
			(eps_imag == 2047) || (eps_imag == -2048));

		if (!first_pass && (clipping != prev_clipping)) {
			if (!clipping) {
				gain_index -= (uint8)gain_step;
			}
			done = TRUE;
			break;
		}

		if (clipping)
			gain_index += (uint8)gain_step;
		else
			gain_index -= (uint8)gain_step;

		/* limit index to gain table range */
		if ((gain_index < 0) || (gain_index > 127)) {
			if (gain_index < 0) {
				gain_index = 0;
			} else {
				gain_index = 127;
			}
			done = TRUE;
			break;
		}

		first_pass = FALSE;
		prev_clipping = clipping;
	}

	if (!done) {
		PHY_ERROR(("Warning PAPD gain control failed to converge in %d attempts\n",
			max_iter));
	}

	PHY_CAL(("papd gctrl settled on index: %d, in %d attempts\n", gain_index, j));

	return (uint8) gain_index;
}

/* Run PAPD cal at power level appropriate for tx gain table */
void
wlc_phy_papd_cal_txpwr_lpphy(phy_info_t *pi, bool full_cal)
{
	lpphy_txcalgains_t txgains;
	papd_restore_state restore_state;
	bool suspend;
	lpphy_tx_pwr_state tx_pwr_state;
	uint16 save_dig_filt[LPPHY_REV2_NUM_DIG_FILT_COEFFS];
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
	lpphytbl_info_t tab;
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lpphy_calcache_t *cache = &ctx->u.lpphy_cache;
	cache->lpphy_papd_tx_gain_at_last_cal = wlc_phy_get_current_tx_pwr_idx_lpphy(pi);
	ctx->cal_info.last_papd_cal_time = pi->sh->now;
#else
	pi_lp->lpphy_papd_tx_gain_at_last_cal = wlc_phy_get_current_tx_pwr_idx_lpphy(pi);
	pi_lp->lpphy_papd_last_cal = pi->sh->now;
#endif
	pi_lp->lpphy_force_papd_cal = FALSE;
	pi_lp->lpphy_papd_recal_counter++;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	suspend = !(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 10000);
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	}

	/* Set digital filter to ofdm coeffs */
	if (LPREV_GE(pi->pubpi.phy_rev, 2))
	{
		wlc_phy_tx_dig_filt_lpphy_rev2(pi, FALSE, save_dig_filt);
		wlc_phy_tx_dig_filt_lpphy_rev2(pi, TRUE, LPPHY_REV2_txdigfiltcoeffs_ofdm[0]);
	}

	/* Save tx power control and gain state */
	wlc_lpphy_tx_pwr_state_save(pi, &tx_pwr_state);

	txgains.useindex = TRUE;

	wlc_lpphy_papd_cal_setup(pi, &restore_state);

	if (full_cal)
		txgains.index = 65;
	else
#if defined(PHYCAL_CACHING)
	txgains.index = cache->lpphy_papd_cal_gain_index;
#else
	txgains.index = pi_lp->lpphy_papd_cal_gain_index;
#endif

	txgains.index = wlc_lpphy_papd_cal_gctrl(pi, txgains.index);
#if defined(PHYCAL_CACHING)
	cache->lpphy_papd_cal_gain_index = txgains.index;
#else
	pi_lp->lpphy_papd_cal_gain_index = txgains.index;
#endif

	switch (pi_lp->lpphy_papd_cal_type) {
		case 0:
			wlc_lpphy_papd_cal(pi, &txgains, CAL_SOFT);
			break;
		case 1:
			wlc_lpphy_papd_cal(pi, &txgains, CAL_FULL);
			break;
	}

	wlc_lpphy_papd_cal_cleanup(pi, &restore_state);

	/* Restore tx power and reenable tx power control */
	wlc_lpphy_tx_pwr_state_restore(pi, &tx_pwr_state);

	if (LPREV_GE(pi->pubpi.phy_rev, 2))
		wlc_phy_tx_dig_filt_lpphy_rev2(pi, TRUE, save_dig_filt);

	/* Save papd calibration results */
	/* Save epsilon table */
	tab.tbl_len = LPPHY_PAPD_EPS_TBL_SIZE;
	tab.tbl_id = LPPHY_TBL_ID_PAPD_EPS;
	tab.tbl_offset = 0;
	tab.tbl_width = 32;
	tab.tbl_phywidth = 32;
#if defined(PHYCAL_CACHING)
	tab.tbl_ptr = cache->papd_eps_tbl;
#else
	tab.tbl_ptr = pi_lp->lpphy_cal_results.papd_eps_tbl;
#endif
	wlc_phy_table_read_lpphy(pi, &tab);

	/* Save papd index offset */
#if defined(PHYCAL_CACHING)
	cache->papd_indexOffset =
		PHY_REG_READ(pi, LPPHY_REV2, papdScalar, indexOffset);
#else
	pi_lp->lpphy_cal_results.papd_indexOffset =
		PHY_REG_READ(pi, LPPHY_REV2, papdScalar, indexOffset);
#endif

	/* Save start stop index */
#if defined(PHYCAL_CACHING)
	cache->papd_startstopindex =
		phy_reg_read(pi, LPPHY_REV2_papdstartstopIndex);
#else
	pi_lp->lpphy_cal_results.papd_startstopindex =
		phy_reg_read(pi, LPPHY_REV2_papdstartstopIndex);
#endif

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);
}

/* Save tx gain and power control state */
void
wlc_lpphy_tx_pwr_state_save(phy_info_t *pi, lpphy_tx_pwr_state *state)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	state->tx_pwr_ctrl = wlc_lpphy_get_tx_pwr_ctrl(pi);
	state->gain_index = pi_lp->lpphy_tx_power_idx_override;
}

/* Restore tx gain and power control state */
void
wlc_lpphy_tx_pwr_state_restore(phy_info_t *pi, lpphy_tx_pwr_state *state)
{
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, state->tx_pwr_ctrl);
	if ((state->tx_pwr_ctrl != LPPHY_TX_PWR_CTRL_HW) && (state->gain_index != -1))
		wlc_phy_set_tx_pwr_by_index_lpphy(pi, state->gain_index);
}

/*
 * TX IQ/LO Calibration
 *
 * args: target_gains = Tx gains *for* which the cal is done, not necessarily *at* which it is done
 *       If not specified, will use current Tx gains as target gains
 *       keep_tone: 0: shut off tone
 *                  1: leave tone on
 *                  2: stop tone, but don't wait for stop tone to complete
 */
static void
wlc_lpphy_tx_iqlo_cal(phy_info_t *pi,  lpphy_txgains_t *target_gains, phy_cal_mode_t cal_mode,
	uchar keep_tone, bool apply_results)
{
	/* starting values used in full cal
	 * -- can fill non-zero vals based on lab campaign (e.g., per channel)
	 * -- format: a0,b0,a1,b1,ci0_cq0_ci1_cq1,di0_dq0,di1_dq1,ei0_eq0,ei1_eq1,fi0_fq0,fi1_fq1
	 */
	lpphy_txgains_t cal_gains, temp_gains;
	uint16 hash;
	uint8 band_idx;
	int j;
	uint16 ncorr_override[5];
	uint16 syst_coeffs[] =
		{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

	/* cal commands full cal and recal */
	uint16 commands_fullcal[] =  { 0x8421, 0x8423, 0x8312, 0x8084, 0x8234, 0x8056, 0x8234 };
	uint16 commands_recal[] =  { 0x8312, 0x8055, 0x8212 };
	uint16 commands_dig_lo[] = {0x8234};

	/* calCmdNum register: log2 of settle/measure times for search/gain-ctrl, 4 bits each */
	uint16 command_nums_fullcal[] = { 0x7a97, 0x7a97, 0x7a97, 0x7a97, 0x7a87, 0x7a87, 0x7b97 };
	uint16 command_nums_recal[] = {  0x7997, 0x7987, 0x7a97 };
	uint16 command_nums_dig_lo[] = {0x7b97};
	uint16 *command_nums = command_nums_fullcal;

	uint16 *start_coeffs = NULL, *cal_cmds = NULL, cal_type, diq_start;
	uint16 tx_pwr_ctrl_old, rssi_old;
	uint16 papd_ctrl_old = 0, auxadc_ctrl_old = 0, rssi_muxsel_old = 0;
	uint8 iqcal_old;
	bool tx_gain_override_old;
	lpphy_txgains_t old_gains = {0, 0, 0, 0};
	uint8 ei0, eq0, fi0, fq0;
	lpphytbl_info_t tab;
	uint i, n_cal_cmds = 0, n_cal_start = 0;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lpphy_calcache_t *cache = &ctx->u.lpphy_cache;
#endif

	/* Preset iqloCaltbl */
	tab.tbl_id = 0;			/* iqloCaltbl		*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_phywidth = 16; /* width in the phy */

	switch (cal_mode) {
		case CAL_FULL:
			start_coeffs = syst_coeffs;
			cal_cmds = commands_fullcal;
			n_cal_cmds = ARRAYSIZE(commands_fullcal);
			break;

		case CAL_RECAL:
#if defined(PHYCAL_CACHING)
			ASSERT(cache->txiqlocal_bestcoeffs_valid);
			start_coeffs = cache->txiqlocal_bestcoeffs;
#else
			ASSERT(pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs_valid);
			start_coeffs = pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs;
#endif
			cal_cmds = commands_recal;
			n_cal_cmds = ARRAYSIZE(commands_recal);
			command_nums = command_nums_recal;
			break;

		case CAL_CURRECAL:
			tab.tbl_ptr = syst_coeffs; /* ptr to buf */
			tab.tbl_len = 7;        /* # values   */
			tab.tbl_offset = 80; /* tbl offset */
			wlc_phy_table_read_lpphy(pi, &tab);

			cal_cmds = commands_recal;
			n_cal_cmds = ARRAYSIZE(commands_recal);
			command_nums = command_nums_recal;
			/* FALL THROUGH */
		case CAL_DIGCAL:
			wlc_phy_get_radio_loft_lpphy(pi, &ei0, &eq0, &fi0, &fq0);

			syst_coeffs[7] = ((ei0 & 0xff) << 8) | ((eq0 & 0xff) << 0);
			syst_coeffs[8] = 0;
			syst_coeffs[9] = ((fi0 & 0xff) << 8) | ((fq0 & 0xff) << 0);
			syst_coeffs[10] = 0;

			start_coeffs = syst_coeffs;

			if (CAL_CURRECAL == cal_mode)
				break;

			cal_cmds = commands_fullcal;
			n_cal_cmds = ARRAYSIZE(commands_fullcal);
			command_nums = command_nums_fullcal;
			/* Skip analog commands */
			n_cal_start = 2;
			break;

		case CAL_DIGLO:
#if defined(PHYCAL_CACHING)
			ASSERT(cache->txiqlocal_bestcoeffs_valid);
			start_coeffs = cache->txiqlocal_bestcoeffs;
#else
			ASSERT(pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs_valid);
			start_coeffs = pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs;
#endif
			cal_cmds = commands_dig_lo;
			n_cal_cmds = ARRAYSIZE(commands_dig_lo);
			command_nums = command_nums_dig_lo;
			break;
		default:
			ASSERT(FALSE);
	}

	/* Fill in Start Coeffs */
	tab.tbl_ptr = start_coeffs; /* ptr to buf */
	tab.tbl_len = 11;        /* # values   */
	tab.tbl_offset = 64; /* tbl offset */
	wlc_phy_table_write_lpphy(pi,  &tab);

	/* Save original tx power control mode */
	tx_pwr_ctrl_old = wlc_lpphy_get_tx_pwr_ctrl(pi);

	/* Disable tx power control */
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_OFF);

	/* Save old and apply new tx gains if needed */
	tx_gain_override_old = wlc_lpphy_tx_gain_override_enabled(pi);
	if (tx_gain_override_old)
		wlc_lpphy_get_tx_gain(pi, &old_gains);

	if (!target_gains) {
		if (!tx_gain_override_old)
			wlc_phy_set_tx_pwr_by_index_lpphy(pi, pi_lp->lpphy_tssi_idx);
		wlc_lpphy_get_tx_gain(pi, &temp_gains);
		target_gains = &temp_gains;
	}

	hash = (target_gains->gm_gain << 8) |
		(target_gains->pga_gain << 4) |
		(target_gains->pad_gain);

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	cal_gains = *target_gains;
	bzero(ncorr_override, sizeof(ncorr_override));
	for (j = 0; j < iqcal_gainparams_numgains[band_idx]; j++) {
		if (hash == tbl_iqcal_gainparams_lpphy[band_idx][j][0]) {
			cal_gains.gm_gain = tbl_iqcal_gainparams_lpphy[band_idx][j][1];
			cal_gains.pga_gain = tbl_iqcal_gainparams_lpphy[band_idx][j][2];
			cal_gains.pad_gain = tbl_iqcal_gainparams_lpphy[band_idx][j][3];
			bcopy(&tbl_iqcal_gainparams_lpphy[band_idx][j][3], ncorr_override,
				sizeof(ncorr_override));
			break;
		}
	}

	wlc_lpphy_set_tx_gain(pi, &cal_gains);

	PHY_INFORM(("wl%d: %s: target gains: %d %d %d %d, cal_gains: %d %d %d %d\n",
		pi->sh->unit, __FUNCTION__,
		target_gains->gm_gain,
		target_gains->pga_gain,
		target_gains->pad_gain,
		target_gains->dac_gain,
		cal_gains.gm_gain,
		cal_gains.pga_gain,
		cal_gains.pad_gain,
		cal_gains.dac_gain));

	PHY_REG_LIST_START
		/* Turn off ADC */
		PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvrVal, pwdn_adc_ovr_val, 0)
		PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvr, pwdn_adc_ovr, 1)
	PHY_REG_LIST_EXECUTE(pi);

	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
		PHY_REG_LIST_START
			/* Turn off PA */
			PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, gmode_tx_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, gmode_tx_pu_ovr_val, 0)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, amode_tx_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, amode_tx_pu_ovr_val, 0)
		PHY_REG_LIST_EXECUTE(pi);

		/* Adjust ADC common mode */
		rssi_old = phy_reg_read(pi, LPPHY_AfeRSSICtrl0);
		phy_reg_mod(pi, LPPHY_AfeRSSICtrl0, 0xff, 0x59);

		/* Set tssi switch to use IQLO */
		iqcal_old = (uint8)read_radio_reg(pi, RADIO_2062_IQCAL_NORTH);
		and_radio_reg(pi, RADIO_2062_IQCAL_NORTH, 0xf3);
		if (LPREV_IS(pi->pubpi.phy_rev, 1)) {
			uint16 rssi_muxsel;

			rssi_muxsel = rssi_muxsel_old = phy_reg_read(pi, LPPHY_REV1_AfeRSSISel);
			rssi_muxsel &= ~LPPHY_REV1_AfeRSSISel_grssi_sel_MASK;
			rssi_muxsel |= (3 << LPPHY_REV1_AfeRSSISel_grssi_sel_SHIFT);
			rssi_muxsel |= LPPHY_REV1_AfeRSSISel_grssi_sel_ovr_MASK;
			PHY_REG_WRITE(pi, LPPHY_REV1, AfeRSSISel, rssi_muxsel);
		}

		/* Power on IQLO block */
		and_radio_reg(pi, RADIO_2062_PDN_CTRL2_NORTH, (uint8)~0x10);

		/* Temp sense */
		PHY_REG_MOD(pi, LPPHY, tempsenseCtrlAddr, tempsenseCtrl, 0x0001);
	} else {
		uint16 adc_mid;

		/* Adjust ADC common mode */
		rssi_old = phy_reg_read(pi, LPPHY_REV2_AfeRSSICtrl1);
		if (LPREV_GE(pi->pubpi.phy_rev, 3))
			adc_mid = 175;
		else if (CHIPID(pi->sh->chip) == BCM4325_CHIP_ID &&
		         BOARDTYPE(pi->sh->boardtype) == BCM94325DEVBU_BOARD)
			adc_mid = 165;
		else
			adc_mid = 202;
		phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, 0x3ff, adc_mid);

		/* Set tssi switch to use IQLO */
		iqcal_old = (uint8)read_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2);
		and_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, (uint8)~0x0d);

		/* Power on IQLO block */
		and_radio_reg(pi, RADIO_2063_IQCAL_GVAR, (uint8)~0x80);

		/* Turn off PAPD */
		papd_ctrl_old = phy_reg_read(pi, LPPHY_REV2_papdctrl);
		PHY_REG_MOD(pi, LPPHY_REV2, papdctrl, papdcompEn, 0);

		auxadc_ctrl_old = phy_reg_read(pi, LPPHY_REV2_auxadcCtrl);

		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, iqlocalEn, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, rssiformatConvEn, 0)
		PHY_REG_LIST_EXECUTE(pi);

		write_radio_reg(pi, RADIO_2063_PA_CTRL_10, 0x51);
	}

	/* Open TR switch */
	wlc_lpphy_set_trsw_override(pi, FALSE, TRUE);

	/* Load the LO compensation gain table */
	tab.tbl_ptr = lpphy_iqcal_loft_gainladder;
	tab.tbl_len = ARRAYSIZE(lpphy_iqcal_loft_gainladder);
	tab.tbl_offset = 0;
	wlc_phy_table_write_lpphy(pi, &tab);

	/* Load the IQ calibration gain table */
	tab.tbl_ptr = lpphy_iqcal_ir_gainladder;
	tab.tbl_len = ARRAYSIZE(lpphy_iqcal_ir_gainladder);
	tab.tbl_offset = 32;
	wlc_phy_table_write_lpphy(pi, &tab);

	if (!pi->phy_tx_tone_freq && LPREV_LT(pi->pubpi.phy_rev, 2)) {
		uint16 adc_pu_time;
		lpphytbl_info_t rfseq_tab;

		/* Preset for rfseq table */
		rfseq_tab.tbl_ptr = &adc_pu_time;
		rfseq_tab.tbl_len = 1;
		rfseq_tab.tbl_id = 0x0b;
		rfseq_tab.tbl_offset = 9;
		rfseq_tab.tbl_width = 16;
		rfseq_tab.tbl_phywidth = 16;

		for (i = 1; !wlc_lpphy_tx_iqlo_functional(pi); i++) {
			if (i > 20) {
				PHY_ERROR(("wl%d: %s: tx iqlo cal not functional\n",
					pi->sh->unit, __FUNCTION__));
			/* No point to continue */
			goto cleanup;
			}
			PHY_INFORM((
				"wl%d: %s: misaligned RSSI clock detected, "
				"retrying for the %d time\n",
				pi->sh->unit, __FUNCTION__, i));

			wlc_phy_stop_tx_tone_lpphy(pi);
			wlc_phy_clear_deaf_lpphy(pi, (bool)0);
			/* Tweak ADC power up time */
			if ((i % 3) == 0) {
				wlc_phy_table_read_lpphy(pi, &rfseq_tab);
				if (adc_pu_time & 0x01)
					adc_pu_time++;
				else
					adc_pu_time--;
				wlc_phy_table_write_lpphy(pi, &rfseq_tab);
			}
		}
	}

	/* Set Gain Control Parameters */
	/* iqlocal_en<15> / start_index / thresh_d2 / ladder_length_d2 */
	PHY_REG_WRITE(pi, LPPHY, iqloCalCmdGctl, 0x0aa9);

	/* Send out calibration tone */
	if (!pi->phy_tx_tone_freq) {
		wlc_phy_set_deaf_lpphy(pi, (bool)0);
		if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
			if ((CHIPID(pi->sh->chip) == BCM5354_CHIP_ID) &&
				(pi->sh->chiprev >= 3)) {
				wlc_phy_start_tx_tone_lpphy(pi, 2500, 28);
			} else {
				wlc_phy_start_tx_tone_lpphy(pi, 2500, 100);
			}
		} else
			wlc_phy_start_tx_tone_lpphy(pi, 3750, 28);
	}
	/* Enable calibration */
	PHY_REG_OR(pi, LPPHY, iqloCalCmdGctl, LPPHY_iqloCalCmdGctl_iqlo_cal_en_MASK);

	/*
	 * Cal Steps
	 */
	for (i = n_cal_start; i < n_cal_cmds; i++) {
		uint16 zero_diq = 0;
		uint16 best_coeffs[11];
		uint16 command_num;

		cal_type = (cal_cmds[i] & 0x0f00) >> 8;


		/* get & set intervals */
		command_num = command_nums[i];
		if (ncorr_override[cal_type])
			command_num = ncorr_override[cal_type] << 8 | (command_num & 0xff);

		PHY_REG_WRITE(pi, LPPHY, iqloCalCmdNnum, command_num);

		PHY_TMP(("wl%d: %s: running cmd: %x, cmd_num: %x\n",
			pi->sh->unit, __FUNCTION__, cal_cmds[i], command_nums[i]));

		/* need to set di/dq to zero if analog LO cal */
		if ((cal_type == 3) || (cal_type == 4)) {
			tab.tbl_ptr = &diq_start;
			tab.tbl_len = 1;
			tab.tbl_offset = 69;
			wlc_phy_table_read_lpphy(pi, &tab);

			/* Set to zero during analog LO cal */
			tab.tbl_ptr = &zero_diq;
			wlc_phy_table_write_lpphy(pi, &tab);
		}

		/* Issue cal command */
		PHY_REG_WRITE(pi, LPPHY, iqloCalCmd, cal_cmds[i]);

		/* Wait until cal command finished */
		if (!wlc_lpphy_iqcal_wait(pi)) {
			PHY_ERROR(("wl%d: %s: tx iqlo cal failed to complete\n",
				pi->sh->unit, __FUNCTION__));
		/* No point to continue */
		goto cleanup;
		}

		/* Copy best coefficients to start coefficients */
		tab.tbl_ptr = best_coeffs;
		tab.tbl_len = ARRAYSIZE(best_coeffs);
		tab.tbl_offset = 96;
		wlc_phy_table_read_lpphy(pi, &tab);
		tab.tbl_offset = 64;
		wlc_phy_table_write_lpphy(pi, &tab);

		/* restore di/dq in case of analog LO cal */
		if ((cal_type == 3) || (cal_type == 4)) {
			tab.tbl_ptr = &diq_start;
			tab.tbl_len = 1;
			tab.tbl_offset = 69;
			wlc_phy_table_write_lpphy(pi, &tab);
		}
	}

	/* Dump results */
	if (PHY_CAL_ON()) {
		uint16 iqlo_coeffs[11];

		tab.tbl_ptr = iqlo_coeffs;
		tab.tbl_len = ARRAYSIZE(iqlo_coeffs);
		tab.tbl_offset = 96;
		wlc_phy_table_read_lpphy(pi, &tab);

		PHY_CAL(("wl%d: %s complete, apply results: %d, IQ %d %d LO %d %d %d %d %d %d\n",
			pi->sh->unit, __FUNCTION__,
			apply_results,
			(int16)iqlo_coeffs[0],
			(int16)iqlo_coeffs[1],
			(int8)((iqlo_coeffs[5] & 0xff00) >> 8),
			(int8)(iqlo_coeffs[5] & 0x00ff),
			(int8)((iqlo_coeffs[7] & 0xff00) >> 8),
			(int8)(iqlo_coeffs[7] & 0x00ff),
			(int8)((iqlo_coeffs[9] & 0xff00) >> 8),
			(int8)(iqlo_coeffs[9] & 0x00ff)));
	}

	/*
	 * Apply Results
	 */
	if (apply_results) {
		/* Save calibration results */
#if defined(PHYCAL_CACHING)
		tab.tbl_ptr = cache->txiqlocal_bestcoeffs;
		tab.tbl_len = ARRAYSIZE(cache->txiqlocal_bestcoeffs);
#else
		tab.tbl_ptr = pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs;
		tab.tbl_len = ARRAYSIZE(pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs);
#endif
		tab.tbl_offset = 96;
		wlc_phy_table_read_lpphy(pi, &tab);
#if defined(PHYCAL_CACHING)
		cache->txiqlocal_bestcoeffs_valid = TRUE;
#else
		pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs_valid = TRUE;
#endif

		/* Apply IQ Cal Results */
#if defined(PHYCAL_CACHING)
		tab.tbl_ptr = &cache->txiqlocal_bestcoeffs[0];
#else
		tab.tbl_ptr = &pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[0];
#endif
		tab.tbl_len = 4;
		tab.tbl_offset = 80;
		wlc_phy_table_write_lpphy(pi, &tab);

		/* Apply Digital LOFT Comp */
#if defined(PHYCAL_CACHING)
		tab.tbl_ptr = &cache->txiqlocal_bestcoeffs[5];
#else
		tab.tbl_ptr = &pi_lp->lpphy_cal_results.txiqlocal_bestcoeffs[5];
#endif
		tab.tbl_len = 2;
		tab.tbl_offset = 85;
		wlc_phy_table_write_lpphy(pi, &tab);
	}

cleanup:
	/* Switch off test tone */
	if (keep_tone == 0) {
		wlc_phy_stop_tx_tone_lpphy(pi);
		wlc_phy_clear_deaf_lpphy(pi, (bool)0);
	} else if (keep_tone == 2) {
		/* Stop sample buffer playback */
		PHY_REG_AND(pi, LPPHY, SampleplayCnt,
			(uint16)~LPPHY_SampleplayCnt_sampPlayCount_MASK);
	}

	/* Stop calibration */
	PHY_REG_WRITE(pi, LPPHY, iqloCalCmdGctl, 0);

	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
		/* Power off IQLO block */
		or_radio_reg(pi, RADIO_2062_PDN_CTRL2_NORTH, 0x10);

		/* Adjust ADC common mode */
		PHY_REG_WRITE(pi, LPPHY, AfeRSSICtrl0, rssi_old);

		/* Restore tssi switch */
		write_radio_reg(pi, RADIO_2062_IQCAL_NORTH, iqcal_old);
		if (LPREV_IS(pi->pubpi.phy_rev, 1))
			PHY_REG_WRITE(pi, LPPHY_REV1, AfeRSSISel, rssi_muxsel_old);
	} else {
		/* RSSI ADC selection */
		PHY_REG_MOD(pi, LPPHY_REV2, auxadcCtrl, iqlocalEn, 1);

		/* Power off IQLO block */
		or_radio_reg(pi, RADIO_2063_IQCAL_GVAR, 0x80);

		/* Adjust ADC common mode */
		PHY_REG_WRITE(pi, LPPHY_REV2, AfeRSSICtrl1, rssi_old);

		/* Restore tssi switch */
		write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, iqcal_old);

		/* Restore PAPD */
		PHY_REG_WRITE(pi, LPPHY_REV2, papdctrl, papd_ctrl_old);

		/* Restore ADC control */
		PHY_REG_WRITE(pi, LPPHY_REV2, auxadcCtrl, auxadc_ctrl_old);
	}

	/* TR switch */
	wlc_lpphy_clear_trsw_override(pi);

	PHY_REG_LIST_START
		/* RSSI on/off */
		PHY_REG_AND_ENTRY(LPPHY, AfeCtrlOvr, (uint16)~LPPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK)
		/* Reenable ADC */
		PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvr, pwdn_adc_ovr, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Restore tx power and reenable tx power control */
	if (tx_gain_override_old)
		wlc_lpphy_set_tx_gain(pi, &old_gains);
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, tx_pwr_ctrl_old);
}

static void
wlc_2062_vco_cal(phy_info_t *pi)
{
	/*  Toggle reset */
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL21_SOUTH, 0x42);
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL21_SOUTH, 0x62);
	OSL_DELAY(200);
}

static void
wlc_2062_reset_pll_bias(phy_info_t *pi)
{
	/* Reset bias */
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL02_SOUTH, 0xff);
	OSL_DELAY(20);

	if (CHIPID(pi->sh->chip) == BCM5354_CHIP_ID) {
		write_radio_reg(pi, RADIO_2062_COMMON_01_NORTH, 0x4);
		write_radio_reg(pi, RADIO_2062_RFPLL_CTRL02_SOUTH, 0x04);
	} else
		write_radio_reg(pi, RADIO_2062_RFPLL_CTRL02_SOUTH, 0x00);
	OSL_DELAY(5);
}

/* Run iqlo cal and populate iqlo portion of tx power control table */
static void
wlc_lpphy_txpwrtbl_iqlo_cal(phy_info_t *pi, phy_cal_mode_t cal_mode)
{
	lpphy_txgains_t target_gains;
	uint8 bb_mult, save_bb_mult;
	uint16 a, b, didq, save_pa_gain = 0;
	lpphytbl_info_t tab;
	uint idx;
	uint32 val;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	lpphy_calcache_t *cache = &ctx->u.lpphy_cache;
#endif

	/* Store state */
	save_bb_mult = wlc_lpphy_get_bbmult(pi);

	/* Set up appropriate target gains */
	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_NOPA) ||
			(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
			if ((CHIPID(pi->sh->chip) == BCM5354_CHIP_ID) &&
				(pi->sh->chiprev >= 3)) {
				target_gains.gm_gain = 7;
				target_gains.pga_gain = 15;
				target_gains.pad_gain = 10;
				target_gains.dac_gain = 0;
				bb_mult = 64;
			} else {
				target_gains.gm_gain = 7;
				target_gains.pga_gain = 15;
				target_gains.pad_gain = 14;
				target_gains.dac_gain = 0;
				bb_mult = 150;
			}
		} else if (CHSPEC_IS5G(pi->radio_chanspec)) {
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 15;
			target_gains.pad_gain = 14;
			target_gains.dac_gain = 0;
			bb_mult = 64;
		} else {
			target_gains.gm_gain = 4;
			target_gains.pga_gain = 14;
			target_gains.pad_gain = 9;
			target_gains.dac_gain = 0;
			bb_mult = 64;
		}
		wlc_lpphy_set_bbmult(pi, bb_mult);

		/* Turn off PA */
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, gmode_tx_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, gmode_tx_pu_ovr_val, 0)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, amode_tx_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, amode_tx_pu_ovr_val, 0)
		PHY_REG_LIST_EXECUTE(pi);

	} else {
		/* PA gain */
		uint16 pa_gain;
		if (LPREV_GE(pi->pubpi.phy_rev, 3)) {
			if (CHSPEC_IS2G(pi->radio_chanspec))
				pa_gain = 0x30;
			else
				pa_gain = 0x70;
		} else {
			if (CHSPEC_IS2G(pi->radio_chanspec))
				pa_gain = 32;
			else
				pa_gain = 64;
		}
		save_pa_gain = wlc_lpphy_get_pa_gain(pi);
		wlc_lpphy_set_pa_gain(pi, pa_gain);

		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			target_gains.gm_gain = 255;
			target_gains.pga_gain = 60;
			target_gains.dac_gain = 0;
			if (LPREV_IS(pi->pubpi.phy_rev, 2)) {
				target_gains.pad_gain = 255;
			} else {
				target_gains.pad_gain = 0xf0;
			}
		} else {
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 62;
			target_gains.pad_gain = 241;
			target_gains.dac_gain = 0;
		}
	}

	/* Run cal */
	if (!pi_lp->lpphy_use_cck_dig_loft_coeffs) {
		wlc_lpphy_tx_iqlo_cal(pi, &target_gains, cal_mode, 2, TRUE);
	} else {
		/* run separate dig lo cal for cck filter coeffs,
		apply coeffs in ucode based on packet type
		*/

		int save_ofdm_filt_bw;
		int cck_filt_bw;

		wlc_lpphy_tx_iqlo_cal(pi, &target_gains, cal_mode, 1, TRUE);

		/* apply cck filter settings for ofdm/spb mode */
		save_ofdm_filt_bw = PHY_REG_READ(pi, LPPHY_REV2, lpfbwlutreg1, lpfbwlut5);
		cck_filt_bw = PHY_REG_READ(pi, LPPHY_REV2, lpfbwlutreg0, lpfbwlut0);
		PHY_REG_MOD(pi, LPPHY_REV2, lpfbwlutreg1, lpfbwlut5, cck_filt_bw);

		wlc_lpphy_tx_iqlo_cal(pi, &target_gains, CAL_DIGLO, 2, FALSE);

		/* restore filters */
		PHY_REG_MOD(pi, LPPHY_REV2, lpfbwlutreg1, lpfbwlut5, save_ofdm_filt_bw);

		/* apply didq settings to ucode */
		/* cck coeffs */
		tab.tbl_id = LPPHY_TBL_ID_IQLOCAL;
		tab.tbl_width = 16;	/* 16 bit wide	*/
		tab.tbl_phywidth = 16; /* width in the phy */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_ptr = &didq;
		tab.tbl_offset = 101;
		wlc_phy_table_read_lpphy(pi,  &tab);
		PHY_CAL(("cck didq: %x\n", didq));
		wlc_phy_set_tx_locc_ucode_lpphy(pi, TRUE, didq);

#if defined(PHYCAL_CACHING)
		cache->didq_cck = didq;
#else
		pi_lp->lpphy_cal_results.didq_cck = didq;
#endif

		/* ofdm coeffs */
		didq = wlc_phy_get_tx_locc_lpphy(pi);
		wlc_phy_set_tx_locc_ucode_lpphy(pi, FALSE, didq);
	}

	if (LPREV_IS(pi->pubpi.phy_rev, 2)) {
		uint8 ei0, eq0, fi0, fq0;

		wlc_phy_get_radio_loft_lpphy(pi, &ei0, &eq0, &fi0, &fq0);
		if ((ABS((int8)fi0) == 15) && (ABS((int8)fq0) == 15)) {
			PHY_ERROR(("wl%d: %s: tx iqlo cal failed, retrying...\n",
				pi->sh->unit, __FUNCTION__));

			target_gains.gm_gain = 7;
			target_gains.pga_gain = 45;
			target_gains.pad_gain = 186;
			target_gains.dac_gain = 0;
			/* Re-run cal */
			wlc_lpphy_tx_iqlo_cal(pi, &target_gains, cal_mode, 2, TRUE);
		}
	}

	if (pi_lp->lpphy_use_tx_pwr_ctrl_coeffs) {
		/* Get calibration results */
		wlc_phy_get_tx_iqcc_lpphy(pi, &a, &b);
		didq = wlc_phy_get_tx_locc_lpphy(pi);

		/* Populate tx power control table with coeffs */
		tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;	/* 32 bit wide	*/
		tab.tbl_phywidth = 32; /* widht in the phy */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_ptr = &val; /* ptr to buf */
		for (idx = 0; idx < 128; idx++) {
			/* iq */
			tab.tbl_offset = LPPHY_TX_PWR_CTRL_IQ_OFFSET + idx;
			wlc_phy_table_read_lpphy(pi,  &tab);
			val = (val & 0x0ff00000) |
				((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
			wlc_phy_table_write_lpphy(pi,  &tab);

			/* loft */
			tab.tbl_offset = LPPHY_TX_PWR_CTRL_LO_OFFSET + idx;
			val = didq;
			wlc_phy_table_write_lpphy(pi,  &tab);
		}
	}

	wlc_phy_stop_tx_tone_lpphy(pi);
	wlc_phy_clear_deaf_lpphy(pi, (bool)0);

	/* Restore state */
	wlc_lpphy_set_bbmult(pi, save_bb_mult);
	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		wlc_lpphy_set_pa_gain(pi, save_pa_gain);
	} else {
		/* Restore PA */
		PHY_REG_AND(pi, LPPHY, RFOverride0, (uint16)~
			(LPPHY_RFOverride0_gmode_tx_pu_ovr_MASK |
			LPPHY_RFOverride0_amode_tx_pu_ovr_MASK));
	}
}

static void
wlc_lpphy_pr41573(phy_info_t *pi)
{
	void *tbl_ptr;
	lpphytbl_info_t tab;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	ASSERT(LPREV_IS(pi->pubpi.phy_rev, 0));

	tbl_ptr = MALLOC(pi->sh->osh, sizeof(uint32) * 256);
	if (!tbl_ptr) {
		PHY_ERROR(("wlc_phy_periodic_cal_lpphy: out of memory\n"));
	} else {
		uint16 npt, idx, tx_pwr_ctrl;
		uint8 tx_power_idx_override;

		/* Save tx power control mode */
		tx_pwr_ctrl = wlc_lpphy_get_tx_pwr_ctrl(pi);

		/* Save tx power override */
		tx_power_idx_override = pi_lp->lpphy_tx_power_idx_override;

		/* Save NPT & power index before baseband reset */
		npt = pi_lp->lpphy_tssi_npt;
		idx = pi_lp->lpphy_tssi_idx;

		/* Save iqlo data from the power control table */
		tab.tbl_id = LPPHY_TBL_ID_TXPWRCTL;
		tab.tbl_width = 32;	/* 32 bit wide	*/
		tab.tbl_phywidth = 32; /* width in the phy */
		tab.tbl_len = 256;        /* # values   */
		tab.tbl_ptr = tbl_ptr; /* ptr to buf */
		tab.tbl_offset = LPPHY_TX_PWR_CTRL_IQ_OFFSET;
		wlc_phy_table_read_lpphy(pi,  &tab);

		/* Reset lpphy baseband */
		wlapi_bmac_phy_reset(pi->sh->physhim);
		wlc_lpphy_baseband_init(pi);
		wlc_lpphy_tx_pwr_ctrl_init(pi);
		wlc_phy_switch_radio((wlc_phy_t *)pi, TRUE);
		wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_OFF);

		/* Restore iqlo data in the power control table */
		wlc_phy_table_write_lpphy(pi,  &tab);

		/* Restore NPT & power index */
		pi_lp->lpphy_tssi_npt = npt;
		pi_lp->lpphy_tssi_idx = idx;

		/* Restore current channel for the ucode */
		write_phy_channel_reg(pi,
			CHSPEC_CHANNEL(pi->radio_chanspec));

		wlc_lpphy_tx_filter_init(pi);

		/* Restore and apply tx power override */
		pi_lp->lpphy_tx_power_idx_override = tx_power_idx_override;
		if (-1 != pi_lp->lpphy_tx_power_idx_override)
			wlc_phy_set_tx_pwr_by_index_lpphy(pi, pi_lp->lpphy_tx_power_idx_override);

		/* Restore RCCal */
		if (pi_lp->lpphy_rc_cap)
			wlc_lpphy_set_rc_cap(pi, pi_lp->lpphy_rc_cap);

		/* Restore rx antenna diversity */
		wlc_phy_ant_rxdiv_set((wlc_phy_t *)pi, pi->sh->rx_antdiv);

		/* Restore tx power control */
		wlc_phy_set_tx_pwr_ctrl_lpphy(pi, tx_pwr_ctrl);

		/* Free iqlo data storage memory */
		MFREE(pi->sh->osh, tbl_ptr, sizeof(uint32) * 256);
	}
}

/* Set channel specific Tx digital and analog filter settings */
static void
wlc_lpphy_tx_filter_init(phy_info_t *pi)
{
	bool japan_wide_filter;
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (CHSPEC_CHANNEL(pi->radio_chanspec) != 14) {
		japan_wide_filter = FALSE;
	} else {
		japan_wide_filter = pi->u.pi_lpphy->japan_wide_filter;
	}

	if (LPREV_LT(pi->pubpi.phy_rev, 2)) {
		PHY_REG_MOD(pi, LPPHY, lpphyCtrl, txfiltSelect, (japan_wide_filter ? 2 : 0));

		if (LPREV_IS(pi->pubpi.phy_rev, 1) && pi_lp->lpphy_rc_cap) {
			wlc_lpphy_set_rc_cap(pi, pi_lp->lpphy_rc_cap);
		}
	} else {
		int16 cck_filt_bw, ofdm_filt_bw;

		PHY_REG_MOD(pi, LPPHY, lpphyCtrl, txfiltSelect, 2);
		write_radio_reg(pi, RADIO_2063_TXBB_SP_3, japan_wide_filter ? 0x30 : 0x3f);

		if ((pi->ofdm_rccal_override != -1) || (pi->cck_rccal_override != -1)) {
			uint8 cck_ovr, ofdm_ovr;

			if (japan_wide_filter)
				cck_ovr = 0x30;
			else if (pi->cck_rccal_override != -1)
				cck_ovr = (uint8) pi->cck_rccal_override;
			else
				cck_ovr = (uint8) read_radio_reg(pi, RADIO_2063_TXBB_SP_3);

			if (pi->ofdm_rccal_override != -1)
				ofdm_ovr = (uint8) pi->ofdm_rccal_override;
			else
				ofdm_ovr = (uint8) read_radio_reg(pi, RADIO_2063_TXBB_SP_3);

			wlapi_bmac_write_shm(pi->sh->physhim, M_LP_RCCAL_OVR,
				(ofdm_ovr<<8) | cck_ovr);
		} else {
			wlapi_bmac_write_shm(pi->sh->physhim, M_LP_RCCAL_OVR, 0);
		}

		/* Analog tx filter bw */
		if (LPREV_IS(pi->pubpi.phy_rev, 2) && !japan_wide_filter) {
			wlc_lpphy_set_tx_filter_bw(pi, 0);
		}

		if (pi->ofdm_analog_filt_bw_override != -1) {
			ofdm_filt_bw = pi->ofdm_analog_filt_bw_override;
		} else {
			ofdm_filt_bw = 3;
		}

		if (japan_wide_filter) {
			cck_filt_bw = 3;
		} else if (pi->cck_analog_filt_bw_override != -1) {
			cck_filt_bw = pi->ofdm_analog_filt_bw_override;
		} else {
			cck_filt_bw = 0;
		}

		PHY_REG_MOD(pi, LPPHY_REV2, lpfbwlutreg0, lpfbwlut0, cck_filt_bw);
		PHY_REG_MOD(pi, LPPHY_REV2, lpfbwlutreg1, lpfbwlut5, ofdm_filt_bw);

		if (japan_wide_filter) {
			wlc_phy_tx_dig_filt_ucode_set_lpphy_rev2(pi, TRUE,
				LPPHY_REV2_txdigfiltcoeffs_cck_chan14);
		} else {
			wlc_phy_tx_dig_filt_cck_setup_lpphy(pi, FALSE);
		}
	}
}

/* This function adjusts the applied ofdm gain idx offset, dsss gain idx offset,
 * tr_R_gain_val, tr_T_gain_val according to the temperature for better
 * PER across the temperature range.
 */
static void
wlc_lpphy_temp_adj_tables(phy_info_t *pi, int32 temp_adj)
{
	lpphytbl_info_t tab;
	int8 tableBuffer[2];
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	/* adjust the reference ofdm gain index table offset */
	PHY_REG_MOD(pi, LPPHY_REV2, gainidxoffset, ofdmgainidxtableoffset,
		pi_lp->lpphy_ofdmgainidxtableoffset +  temp_adj);

	/* adjust the reference dsss gain index table offset */
	PHY_REG_MOD(pi, LPPHY_REV2, gainidxoffset, dsssgainidxtableoffset,
		pi_lp->lpphy_dsssgainidxtableoffset +  temp_adj);

	/* adjust the reference gain_val_tbl at index 64 and 65 in gain_val_tbl */
	tab.tbl_ptr = tableBuffer;	/* ptr to buf */
	tableBuffer[0] = pi->u.pi_lpphy->tr_R_gain_val + temp_adj;
	tableBuffer[1] = pi->u.pi_lpphy->tr_T_gain_val + temp_adj;
	tab.tbl_len = 2;			/* # values   */
	tab.tbl_id = 17;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = 64;		/* tbl offset */
	tab.tbl_width = 8;			/* bit width */
	tab.tbl_phywidth = 32;		/* width in the phy */

	wlc_phy_table_write_lpphy(pi, &tab);
}


void
wlc_phy_rx_gain_temp_adj_lpphy(phy_info_t *pi)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
	  int8 temp_adj = 0;

	  if (pi_lp->lpphy_rx_gain_temp_adj_tempsense_metric) {
	    if (pi_lp->lpphy_rx_gain_temp_adj_metric < -15)
	      temp_adj = 6;
	    else if (pi_lp->lpphy_rx_gain_temp_adj_metric < 22)
	      temp_adj = 3;
	    else if (pi_lp->lpphy_rx_gain_temp_adj_metric < 63)
	      temp_adj = 0;
	    else
	      temp_adj = -3;

	    PHY_CAL(("lpphy_rx_gain_temp_adj_rx_iq_cal: temp=%d adj=%d\n",
	      pi_lp->lpphy_rx_gain_temp_adj_metric, temp_adj));
	  } else {
	    if (pi_lp->lpphy_rx_gain_temp_adj_metric > pi_lp->lpphy_rx_gain_temp_adj_thresh[0])
	      temp_adj = 6;
	    else if (pi_lp->lpphy_rx_gain_temp_adj_metric > pi_lp->lpphy_rx_gain_temp_adj_thresh[1])
	      temp_adj = 3;
	    else if (pi_lp->lpphy_rx_gain_temp_adj_metric > pi_lp->lpphy_rx_gain_temp_adj_thresh[2])
	      temp_adj = 0;
	    else
	      temp_adj = -3;

	    PHY_CAL(("lpphy_rx_gain_temp_adj_rx_iq_cal: rx_pwr_dB=%d adj=%d\n",
	      pi_lp->lpphy_rx_gain_temp_adj_metric, temp_adj));
	  }

	  wlc_lpphy_temp_adj_tables(pi, (int32)temp_adj);

	}
}

void
wlc_phy_periodic_cal_lpphy(phy_info_t *pi)
{
	bool suspend, full_cal;
	uint16 tx_pwr_ctrl;
	int current_temperature;
	lpphy_rx_iq_cal_data_for_temp_adj_t temp_adj_data;
	uint16 save_dig_filt[LPPHY_REV2_NUM_DIG_FILT_COEFFS];
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
#endif

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	temp_adj_data.valid = FALSE;
	pi->phy_forcecal = FALSE;
	current_temperature = wlc_phy_tempsense_lpphy(pi);
#if defined(PHYCAL_CACHING)
	ctx->cal_info.last_cal_time = pi->sh->now;
	full_cal = !(ctx->valid);
	ctx->cal_info.last_cal_temp = (int16)current_temperature;
#else
	pi->phy_lastcal = pi->sh->now;
	full_cal = (pi_lp->lpphy_full_cal_channel != CHSPEC_CHANNEL(pi->radio_chanspec));
	pi_lp->lpphy_last_cal_temperature = (int16)current_temperature;
	pi_lp->lpphy_full_cal_channel = CHSPEC_CHANNEL(pi->radio_chanspec);
#endif

	if (NORADIO_ENAB(pi->pubpi))
		return;

	suspend = !(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 10000);
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
	}

	wlc_lpphy_btcx_override_enable(pi);

	/* Set digital filter to ofdm coeffs */
	if (LPREV_GE(pi->pubpi.phy_rev, 2))
	{
		wlc_phy_tx_dig_filt_lpphy_rev2(pi, FALSE, save_dig_filt);
		wlc_phy_tx_dig_filt_lpphy_rev2(pi, TRUE, LPPHY_REV2_txdigfiltcoeffs_ofdm[0]);
	}

	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_lpphy_get_tx_pwr_ctrl(pi);
	/* Disable tx power control */
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_OFF);

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
		if (current_temperature >= (int)(pi_lp->lpphy_last_idle_tssi_temperature +
			pi_lp->lpphy_idle_tssi_update_delta_temp) ||
			current_temperature <= (int)(pi_lp->lpphy_last_idle_tssi_temperature
				- pi_lp->lpphy_idle_tssi_update_delta_temp)) {
			wlc_lpphy_compute_idle_tssi(pi, 1);
			pi_lp->lpphy_last_idle_tssi_temperature = (int16)current_temperature;
		}
	}

	/* Tx iqlo calibration */
	wlc_lpphy_txpwrtbl_iqlo_cal(pi, (full_cal ? CAL_FULL : CAL_RECAL));

	if (LPREV_IS(pi->pubpi.phy_rev, 0) && tx_pwr_ctrl)
		wlc_lpphy_pr41573(pi);

	if (LPREV_GE(pi->pubpi.phy_rev, 2) && full_cal)
		wlc_phy_papd_cal_txpwr_lpphy(pi, full_cal);

	/* Rx iq calibration */
	if (CHIPID(pi->sh->chip) == BCM5354_CHIP_ID)
		wlc_lpphy_rx_iq_cal(pi, lpphy_rx_iqcomp_5354_table,
			ARRAYSIZE(lpphy_rx_iqcomp_5354_table),
			TRUE, TRUE, FALSE, FALSE, NULL, &temp_adj_data);
	else {
		const lpphy_rx_iqcomp_t *rx_iqcomp;
		int rx_iqcomp_sz;

		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			rx_iqcomp = lpphy_rx_iqcomp_table_rev2;
			rx_iqcomp_sz = ARRAYSIZE(lpphy_rx_iqcomp_table_rev2);
		} else {
			rx_iqcomp = lpphy_rx_iqcomp_table_rev0;
			rx_iqcomp_sz = ARRAYSIZE(lpphy_rx_iqcomp_table_rev0);
		}
		wlc_lpphy_rx_iq_cal(pi,
			rx_iqcomp,
			rx_iqcomp_sz,
			TRUE, TRUE, FALSE, FALSE, NULL, &temp_adj_data);
	}

	/* Restore tx power control */
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, tx_pwr_ctrl);

	if (LPREV_GE(pi->pubpi.phy_rev, 2))
		wlc_phy_tx_dig_filt_lpphy_rev2(pi, TRUE, save_dig_filt);

	if (!(temp_adj_data.valid) || pi_lp->lpphy_rx_gain_temp_adj_tempsense) {
	  /* adjust rx gains using current temperature */
	  pi_lp->lpphy_rx_gain_temp_adj_tempsense_metric = 1;
	  pi_lp->lpphy_rx_gain_temp_adj_metric = (int8)current_temperature;
	} else {
	  /* adjust rx gains using data from rx iq imbalance calibration */
	  pi_lp->lpphy_rx_gain_temp_adj_tempsense_metric = 0;
	  pi_lp->lpphy_rx_gain_temp_adj_metric = temp_adj_data.rx_pwr_dB;
	}
	wlc_phy_rx_gain_temp_adj_lpphy(pi);

	if (!suspend)
		wlapi_enable_mac(pi->sh->physhim);

#if defined(PHYCAL_CACHING)
	/* Already cached the Tx and Rx IQ Cal results */
	ctx->valid = TRUE;

#if defined(BCMDBG)
	wlc_phy_cal_cache_dbg_lpphy(ctx);
#endif

#endif /* PHYCAL_CACHING */
}

bool
wlc_lpphy_txrxiq_cal_reqd(phy_info_t *pi)
{
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	uint time_since_last_txrx_iq_cal = (pi->sh->now > ctx->cal_info.last_cal_time)?
		(pi->sh->now - ctx->cal_info.last_cal_time):
		((uint)~0 - ctx->cal_info.last_cal_time + pi->sh->now);

	if (ctx->valid)
#else
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
	uint time_since_last_txrx_iq_cal = (pi->sh->now > pi->phy_lastcal)?
		(pi->sh->now - pi->phy_lastcal):
		((uint)~0 - pi->phy_lastcal + pi->sh->now);

	if (pi_lp->lpphy_full_cal_channel == CHSPEC_CHANNEL(pi->radio_chanspec))
#endif
		return (time_since_last_txrx_iq_cal >= pi->sh->glacial_timer);

	return TRUE;
}

bool
wlc_lpphy_papd_cal_reqd(phy_info_t *pi)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, pi->radio_chanspec);
	uint time_since_last_papd_cal = (pi->sh->now >= ctx->cal_info.last_papd_cal_time)?
		(pi->sh->now - ctx->cal_info.last_papd_cal_time):
		((uint)~0 - ctx->cal_info.last_papd_cal_time + pi->sh->now);
	lpphy_calcache_t *cache = &ctx->u.lpphy_cache;
	uint32 gain_delta = (uint32)ABS((int32)(wlc_phy_get_current_tx_pwr_idx_lpphy(pi) -
		cache->lpphy_papd_tx_gain_at_last_cal));
#else
	uint time_since_last_papd_cal = (pi->sh->now >= pi_lp->lpphy_papd_last_cal)?
		(pi->sh->now - pi_lp->lpphy_papd_last_cal):
		((uint)~0 - pi_lp->lpphy_papd_last_cal + pi->sh->now);
	uint32 gain_delta = (uint32)ABS((int32)(wlc_phy_get_current_tx_pwr_idx_lpphy(pi) -
		pi_lp->lpphy_papd_tx_gain_at_last_cal));
#endif

	/* Recal if max interval has elapsed, or min interval has elapsed and */
	/*  tx gain index has changed by more than some threshold */
	return (pi_lp->lpphy_papd_recal_enable &&
		(((time_since_last_papd_cal >= pi_lp->lpphy_papd_recal_max_interval) &&
		(pi_lp->lpphy_papd_recal_max_interval != 0)) ||
		(wlc_phy_tpc_isenabled_lpphy(pi) &&
		(pi_lp->lpphy_papd_recal_min_interval != 0) &&
		(gain_delta >= pi_lp->lpphy_papd_recal_gain_delta) &&
		(time_since_last_papd_cal >= pi_lp->lpphy_papd_recal_min_interval))));
}

static bool
wlc_lpphy_no_cal_possible(phy_info_t *pi)
{
	return (SCAN_RM_IN_PROGRESS(pi) ||
		PLT_INPROG_PHY(pi) ||
		ASSOC_INPROG_PHY(pi) ||
		ACI_SCAN_INPROG_PHY(pi) ||
		PHY_MUTED(pi));
}

void
wlc_phy_chanspec_set_lpphy(phy_info_t *pi, chanspec_t chanspec)
{
	uint8 channel = CHSPEC_CHANNEL(chanspec); /* see wlioctl.h */
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;
#if defined(PHYCAL_CACHING)
	ch_calcache_t *ctx = wlc_phy_get_chanctx(pi, chanspec);
#endif

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	wlc_phy_chanspec_radio_set((wlc_phy_t *)pi, chanspec);

	/* Tune radio for the channel */
	if (!NORADIO_ENAB(pi->pubpi)) {
		if (BCM2063_ID == LPPHY_RADIO_ID(pi))
			wlc_radio_2063_channel_tune(pi, channel);
		else {
			wlc_radio_2062_channel_tune(pi, channel);
		}
		wlc_lpphy_tx_filter_init(pi);
		/* Update gain tables */
		wlc_lpphy_adjust_gain_tables(pi, wlc_phy_channel2freq(channel));
	}

	write_phy_channel_reg(pi, channel);

	/* Perform Tx IQ, Rx IQ and PAPD cal only if no scan in progress */
	if (wlc_lpphy_no_cal_possible(pi))
		return;

#if defined(PHYCAL_CACHING)
	/* Fresh calibration or restoration required */
	if (!ctx) {
		if (LPPHY_MAX_CAL_CACHE == pi->phy_calcache_num) {
			/* Already max num ctx exist, reuse oldest */
			ctx = wlc_phy_get_chanctx_oldest(pi);
			ASSERT(ctx);
			wlc_phy_reinit_chanctx(pi, ctx, chanspec);
		} else {
			/* Prepare a fresh calibration context */
			if (BCME_OK == wlc_phy_create_chanctx((wlc_phy_t *)pi,
				pi->radio_chanspec)) {
				ctx = pi->phy_calcache;
				/* This increment is moved to wlc_create_chanctx() */
				/* pi->phy_calcache_num++; */
			}
			else
				ASSERT(ctx);
		}
	}
#endif /* PHYCAL_CACHING */

	if (wlc_lpphy_txrxiq_cal_reqd(pi)) {
		if (!(pi->carrier_suppr_disable || pi->disable_percal)) {
			/* Perform the periodic calibrations */
			wlc_phy_periodic_cal_lpphy(pi);
		}
	} else {
		/* Restore the cached calibration results as those are still valid */
		wlc_lpphy_restore_txrxiq_calibration_results(pi);
	}

	if (LPREV_GE(pi->pubpi.phy_rev, 2) && wlc_lpphy_papd_cal_reqd(pi)) {
		/* Perform PAPD Calibration */
		wlc_phy_papd_cal_txpwr_lpphy(pi, pi_lp->lpphy_force_papd_cal);
	} else {
		/* Restore the PAPD call cache */
		if (LPREV_GE(pi->pubpi.phy_rev, 2))
			wlc_lpphy_restore_papd_calibration_results(pi);
	}
}

int
wlc_phy_tempsense_lpphy(phy_info_t *pi)
{
	uint32 save_rssi_settings, save_rssiformat;
	uint32 rcalvalue, savemux;
	int32 sum0, prod0, sum1, prod1, sum;
	int32 temp32 = 0;
	bool suspend;

#define m 461.5465

	  /* b = -12.0992 in q11 format */
#define b ((int16)(-12.0992*(1<<11)))
#define qb 11

	  /* thousand_by_m = 1000/m = 1000/461.5465 in q13 format */
#define thousand_by_m ((int16)((1000/m)*(1<<13)))
#define qthousand_by_m 13

#define numsamps 40	/* 40 samples can be accumulated in 1us timeout */
#define one_by_numsamps 26214	/* 1/40 in q.20 format */
#define qone_by_numsamps 20	/* q format of one_by_numsamps */

	if (LPREV_GE(pi->pubpi.phy_rev, 2)) {

		/* suspend the mac if it is not already suspended */
		suspend = !(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);

		  save_rssi_settings = phy_reg_read(pi, LPPHY_REV2_AfeRSSICtrl1);
		  save_rssiformat = phy_reg_read(pi, LPPHY_REV2_auxadcCtrl) &
		    LPPHY_REV2_auxadcCtrl_rssiformatConvEn_MASK;

		  /* set the "rssiformatConvEn" field in the auxadcCtrl to 1 */
		  PHY_REG_MOD(pi, LPPHY_REV2, auxadcCtrl, rssiformatConvEn, 1);

		  /* slpinv_rssi */
		  phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, (1<<13), (0<<13));
		  phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, (0xf<<0), (0<<0));
		  phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, (0xf<<4), (11<<4));
		  phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, (0x7<<10), (5<<10));

		  /* read the rcal value */
		  rcalvalue = read_radio_reg(pi, RADIO_2063_COMMON_13) & 0xf;

		  /* set powerdetector before PA and rssi mux to tempsense */
		  savemux = phy_reg_read(pi, LPPHY_REV2_AfeCtrlOvrVal) &
		          LPPHY_REV2_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK;

		  PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(LPPHY_REV2, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 5)
			/* reset auxadc */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, auxadcreset, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, auxadcreset, 0)
		  PHY_REG_LIST_EXECUTE(pi);

		  /* set rcal override */
		  mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<7), (1<<7));

		  /* power up temp sense */
		  mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<6), (0<<6));

		  /* set temp sense mux */
		  mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<5), (0<<5));

		  /* set rcal value */
		  mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (0xf<<0), (rcalvalue<<0));

		  /* set TPSENSE_swap to 0 */
		  mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<4), (0<<4));

		  wlc_lpphy_aux_adc_accum(pi, numsamps, 0, &sum0, &prod0);

		  PHY_REG_LIST_START
			/* reset auxadc */
			PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, auxadcreset, 1)
			PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, auxadcreset, 0)
		  PHY_REG_LIST_EXECUTE(pi);

		  /* set TPSENSE swap to 1 */
		  mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<4), (1<<4));

		  wlc_lpphy_aux_adc_accum(pi, numsamps, 0, &sum1, &prod1);

		  sum = (sum0 + sum1) >> 1;

		  /* restore rssi settings */
		  PHY_REG_WRITE(pi, LPPHY_REV2, AfeRSSICtrl1, (uint16)save_rssi_settings);
		  phy_reg_mod(pi, (uint16)LPPHY_REV2_auxadcCtrl,
		              (uint16)LPPHY_REV2_auxadcCtrl_rssiformatConvEn_MASK,
		              (uint16)save_rssiformat);
		  phy_reg_mod(pi, (uint16)LPPHY_REV2_AfeCtrlOvrVal,
		              (uint16)LPPHY_REV2_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
		              (uint16)savemux);

		  /* powerdown tempsense */
		  mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<6), (1<<6));

		  /* sum = sum/numsamps in qsum=0+qone_by_numsamps format
		   * as the accumulated values are always less than 200, 6 bit values, the
		   * sum always fits into 16 bits
		   */

		  sum = qm_mul321616((int16)sum, one_by_numsamps);

		  /* bring sum into qb format */
		  sum = sum >> (qone_by_numsamps-qb);

		  /* sum-b in qb format */
		  temp32 = sum - b;

		  /* calculate (sum-b)*1000/m in qb+qthousand_by_m-15=11+13-16 format */
		  temp32 = qm_mul323216(temp32, (int16)thousand_by_m);

		  /* bring temp32 into q0 format */
		  temp32 = (temp32+(1<<7)) >> 8;

		  /* enable the mac if it is suspended by tempsense function */
		  if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);

	}
	return temp32;
#undef m
#undef b
#undef qb
#undef thousand_by_m
#undef qthousand_by_m
#undef numsamps
#undef one_by_numsamps
#undef qone_by_numsamps
}

int
wlc_phy_vbatsense_lpphy(phy_info_t *pi)
{
	  uint32 save_rssi_settings, save_rssiformat;
	  uint32 savemux;
	  int32 sum, prod, x, voltage;

#define numsamps 40	/* 40 samples can be accumulated in 1us timeout */
#define one_by_numsamps 26214	/* 1/40 in q.20 format */
#define qone_by_numsamps 20	/* q format of one_by_numsamps */

#define c1 (int16)((0.0580833 * (1<<19)) + 0.5)	/* polynomial coefficient in q.19 format */
#define qc1 19									/* qformat of c1 */
#define c0 (int16)((3.9591333 * (1<<13)) + 0.5) 	/* polynomial coefficient in q.14 format */
#define qc0 13									/* qformat of c0 */

	  /* suspend the mac */
	  wlapi_suspend_mac_and_wait(pi->sh->physhim);

	  save_rssi_settings = phy_reg_read(pi, LPPHY_REV2_AfeRSSICtrl1);
	  save_rssiformat = phy_reg_read(pi, LPPHY_REV2_auxadcCtrl) &
	    LPPHY_REV2_auxadcCtrl_rssiformatConvEn_MASK;

	  /* set the "rssiformatConvEn" field in the auxadcCtrl to 1 */
	  PHY_REG_MOD(pi, LPPHY_REV2, auxadcCtrl, rssiformatConvEn, 1);

	  /* slpinv_rssi */
	  phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, (1<<13), (0<<13));
	  phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, (0xf<<0), (8<<0));
	  phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, (0xf<<4), (10<<4));
	  phy_reg_mod(pi, LPPHY_REV2_AfeRSSICtrl1, (0x7<<10), (3<<10));

	  /* set powerdetector before PA and rssi mux to tempsense */
	  savemux = phy_reg_read(pi, LPPHY_REV2_AfeCtrlOvrVal) &
	          LPPHY_REV2_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK;
	  PHY_REG_MOD(pi, LPPHY_REV2, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 4);

		/* set iqcal mux to select VBAT */
	  mod_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, (0xF<<0), (0x4<<0));

	  PHY_REG_LIST_START
		/* reset auxadc */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, auxadcreset, 1)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, auxadcreset, 0)
	  PHY_REG_LIST_EXECUTE(pi);

	  wlc_lpphy_aux_adc_accum(pi, numsamps, 0, &sum, &prod);

	  /* restore rssi settings */
	  PHY_REG_WRITE(pi, LPPHY_REV2, AfeRSSICtrl1, (uint16)save_rssi_settings);
	  phy_reg_mod(pi, (uint16)LPPHY_REV2_auxadcCtrl,
	              (uint16)LPPHY_REV2_auxadcCtrl_rssiformatConvEn_MASK,
	              (uint16)save_rssiformat);
	  phy_reg_mod(pi, (uint16)LPPHY_REV2_AfeCtrlOvrVal,
	              (uint16)LPPHY_REV2_AfeCtrlOvrVal_rssi_muxsel_ovr_val_MASK,
	              (uint16)savemux);

	  /* sum = sum/numsamps in qsum=0+qone_by_numsamps format
	   * as the accumulated values are always less than 200, 6 bit values, the
	   * sum always fits into 16 bits
	   */
	  x = qm_mul321616((int16)sum, one_by_numsamps);

	  /* compute voltage = c1*sum + co */
	  voltage = qm_mul323216(x, c1); /* voltage in q.qone_by_numsamps+qc1-16 format */

	  /* bring sum to qc0 format */
	  voltage = voltage >> (qone_by_numsamps+qc1-16 - qc0);

	  /* commute c1*x + c0 */
	  voltage = voltage + c0;

	  /* bring voltage to q.4 format */
	  voltage = voltage >> (qc0 - 4);

	  /* enable the mac */
	  wlapi_enable_mac(pi->sh->physhim);

	  return voltage;

#undef numsamps
#undef one_by_numsamps
#undef qone_by_numsamps
#undef c1
#undef qc1
#undef c0
#undef qc0
}


int
wlc_lpphy_wait_phy_reg(phy_info_t *pi, uint16 addr, uint32 val, uint32 mask, int shift,
	int timeout_us)
{
	int timer_us, done;

	for (timer_us = 0, done = 0; (timer_us < timeout_us) && (!done);
		timer_us = timer_us + 1) {

		/* wait for poll interval in units of microseconds */
		OSL_DELAY(1);

		/* check if the current field value is same as the required value */
		if (val == (uint32)(((phy_reg_read(pi, addr)) & mask) >> shift)) {
			done = 1;
		}
	}
	return done;
}

int
wlc_lpphy_aux_adc_accum(phy_info_t *pi, uint32 numberOfSamples, uint32 waitTime, int32 *sum,
	int32 *prod)
{
	uint32 save_pwdn_rssi_ovr, term0, term1;
	int done;

	save_pwdn_rssi_ovr = phy_reg_read(pi, LPPHY_REV2_AfeCtrlOvr) &
		LPPHY_REV2_AfeCtrlOvr_pwdn_rssi_ovr_MASK;

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(LPPHY_REV2, AfeCtrlOvr, pwdn_rssi_ovr, 1)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, AfeCtrlOvrVal, pwdn_rssi_ovr_val, 0)
		/* clear accumulators */
		PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, auxadcreset, 1)
		PHY_REG_MOD_ENTRY(LPPHY_REV2, auxadcCtrl, auxadcreset, 0)
	PHY_REG_LIST_EXECUTE(pi);

	PHY_REG_MOD(pi, LPPHY_REV2, NumrssiSamples, numrssisamples, numberOfSamples);
	PHY_REG_MOD(pi, LPPHY_REV2, rssiwaittime, rssiwaittimeValue, waitTime);
	PHY_REG_MOD(pi, LPPHY_REV2, auxadcCtrl, rssiestStart, 1);

	done = wlc_lpphy_wait_phy_reg(pi, LPPHY_REV2_auxadcCtrl, 0,
		LPPHY_REV2_auxadcCtrl_rssiestStart_MASK,
		LPPHY_REV2_auxadcCtrl_rssiestStart_SHIFT,
		2);

	if (done) {
		term0 = phy_reg_read(pi, LPPHY_REV2_rssiaccValResult0);
		term0 = (term0 & LPPHY_REV2_rssiaccValResult0_rssiaccResult0_MASK);
		term1 = phy_reg_read(pi, LPPHY_REV2_rssiaccValResult1);
		term1 = (term1 & LPPHY_REV2_rssiaccValResult1_rssiaccResult1_MASK);
		*sum = (term1 << 16) + term0;
		term0 = phy_reg_read(pi, LPPHY_REV2_rssiprodValResult0);
		term0 = (term0 & LPPHY_REV2_rssiprodValResult0_rssiProdResult0_MASK);
		term1 = phy_reg_read(pi, LPPHY_REV2_rssiprodValResult1);
		term1 = (term1 & LPPHY_REV2_rssiprodValResult1_rssiProdResult1_MASK);
		*prod = (term1 << 16) + term0;
	}
	else {
		*sum = 0;
		*prod = 0;
	}

	/* restore result */
	phy_reg_mod(pi, (uint16)LPPHY_REV2_AfeCtrlOvr,
	(uint16)LPPHY_REV2_AfeCtrlOvr_pwdn_rssi_ovr_MASK,
	(uint16)save_pwdn_rssi_ovr);

	return done;
}

bool
wlc_phy_tpc_isenabled_lpphy(phy_info_t *pi)
{
	return (LPPHY_TX_PWR_CTRL_HW == wlc_lpphy_get_tx_pwr_ctrl((pi)));
}

uint16
wlc_phy_get_current_tx_pwr_idx_lpphy(phy_info_t *pi)
{
	return PHY_REG_READ(pi, LPPHY, TxPwrCtrlStatus, baseIndex);
}

static void
wlc_radio_2063_channel_tune(phy_info_t *pi, uint8 channel)
{
	uint i;
	const chan_info_2063_t *ci;
	uint8 rfpll_doubler = 1;
	uint16 rf_common15;
	fixed qFref, qFvco, qFcal, qVco, qVal;
	uint8  to, refTo, cp_current, kpd_scale, ioff_scale, offset_current;
	uint32 setCount, div_int, div_frac, iVal, fvco3, fref, fref3, fcal_div;
	chan_info_2063_t *table = NULL;
	uint numberOfElementsInTable = 0;

	if (LPREV_IS(pi->pubpi.phy_rev, 2)) {
		/* radio rev 0 */
		table = chan_info_2063_rev0;
		numberOfElementsInTable = ARRAYSIZE(chan_info_2063_rev0);
	} else if (LPREV_GT(pi->pubpi.phy_rev, 2)) {
		/* radio rev 1 */
		table = chan_info_2063_rev1;
		numberOfElementsInTable = ARRAYSIZE(chan_info_2063_rev1);
	} else {
		PHY_ERROR(("unsupported radio revision\n"));
		return;
	}

	/* lookup radio-chip-specific channel code */
	for (i = 0; i < numberOfElementsInTable; i++)
		if (table[i].chan == channel)
			break;

	if (i >= numberOfElementsInTable) {
		PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
		          pi->sh->unit, __FUNCTION__, channel));
		return;
	}

	ci = &table[i];

	/* Radio tunables */
	write_radio_reg(pi, RADIO_2063_LOGEN_VCOBUF_1, ci->RF_logen_vcobuf_1);
	write_radio_reg(pi, RADIO_2063_LOGEN_MIXER_2, ci->RF_logen_mixer_2);
	write_radio_reg(pi, RADIO_2063_LOGEN_BUF_2, ci->RF_logen_buf_2);
	write_radio_reg(pi, RADIO_2063_LOGEN_RCCR_1, ci->RF_logen_rccr_1);
	write_radio_reg(pi, RADIO_2063_GRX_1ST_3, ci->RF_grx_1st_3);
	write_radio_reg(pi, RADIO_2063_GRX_2ND_2, ci->RF_grx_2nd_2);
	write_radio_reg(pi, RADIO_2063_ARX_1ST_3, ci->RF_arx_1st_3);
	write_radio_reg(pi, RADIO_2063_ARX_2ND_1, ci->RF_arx_2nd_1);
	write_radio_reg(pi, RADIO_2063_ARX_2ND_4, ci->RF_arx_2nd_4);
	write_radio_reg(pi, RADIO_2063_ARX_2ND_7, ci->RF_arx_2nd_7);
	write_radio_reg(pi, RADIO_2063_ARX_PS_6, ci->RF_arx_ps_6);
	write_radio_reg(pi, RADIO_2063_TXRF_CTRL_2, ci->RF_txrf_ctrl_2);
	write_radio_reg(pi, RADIO_2063_TXRF_CTRL_5, ci->RF_txrf_ctrl_5);
	write_radio_reg(pi, RADIO_2063_PA_CTRL_11, ci->RF_pa_ctrl_11);
	write_radio_reg(pi, RADIO_2063_ARX_MIX_4, ci->RF_arx_mix_4);

	PHY_REG_LIST_START
		/* Shared RX clb signals */
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, extslnactrl0, 0x5f80)
		PHY_REG_WRITE_ENTRY(LPPHY_REV2, extslnactrl1, 0x0)
	PHY_REG_LIST_EXECUTE(pi);

	if (LPREV_GE(pi->pubpi.phy_rev, 3)) {
		uint16 temp;
		/* Important: This below loop is important. This is intended to wait
		 * till the value got into hardware correctly.
		 */
		while (phy_reg_read(pi, LPPHY_REV2_extslnactrl0) != 0x5f80) {
			;
		}
		temp = phy_reg_read(pi, LPPHY_REV2_extslnactrl0);
		temp = (uint16)(temp & 0x0fff);
		temp = (uint16)(temp | (ci->RF_wrf_slna_RX_2G_1st_VT_STG1 << 12));
		PHY_REG_WRITE(pi, LPPHY_REV2, extslnactrl0, temp);
	}

	/* Turn on PLL power supplies */
	rf_common15 = read_radio_reg(pi, RADIO_2063_COMMON_15);
	write_radio_reg(pi, RADIO_2063_COMMON_15, rf_common15 | (0x0f << 1));

	/* Calculate various input frequencies */
	fref = rfpll_doubler ? PHY_XTALFREQ(pi->xtalfreq) : (PHY_XTALFREQ(pi->xtalfreq) << 1);
	if (PHY_XTALFREQ(pi->xtalfreq) <= 26000000)
		fcal_div = rfpll_doubler ? 1 : 4;
	else
		fcal_div = 2;
	if (ci->freq > 2484)
		fvco3 = (ci->freq << 1);
	else
		fvco3 = (ci->freq << 2);
	fref3 = 3 * fref;

	/* Convert into Q16 MHz */
	qFref =  wlc_phy_qdiv_roundup(fref, PLL_2063_MHZ, 16);
	qFcal = wlc_phy_qdiv_roundup(fref, fcal_div * PLL_2063_MHZ, 16);
	qFvco = wlc_phy_qdiv_roundup(fvco3, 3, 16);

	/* PLL_delayBeforeOpenLoop */
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_3, 0x02);

	/* PLL_enableTimeout */
	to = (uint8)((((fref * PLL_2063_CAL_REF_TO) /
		(PLL_2063_OPEN_LOOP_DELAY * fcal_div * PLL_2063_MHZ)) + 1) >> 1) - 1;
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_6, (0x07 << 0), to >> 2);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_7, (0x03 << 5), to << 5);

	/* PLL_cal_ref_timeout */
	refTo = (uint8)((((fref * PLL_2063_CAL_REF_TO) / (fcal_div * (to + 1))) +
		(PLL_2063_MHZ - 1)) / PLL_2063_MHZ) - 1;
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_5, refTo);

	/* PLL_calSetCount */
	setCount = (uint32)FLOAT(
		(fixed)wlc_phy_qdiv_roundup(qFvco, qFcal * 16, 16) * (refTo + 1) * (to + 1)) - 1;
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_7, (0x0f << 0), (uint8)(setCount >> 8));
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCOCAL_8, (uint8)(setCount & 0xff));

	/* Divider, integer bits */
	div_int = ((fvco3 * (PLL_2063_MHZ >> 4)) / fref3) << 4;

	/* Divider, fractional bits */
	div_frac = ((fvco3 * (PLL_2063_MHZ >> 4)) % fref3) << 4;
	while (div_frac >= fref3) {
		div_int++;
		div_frac -= fref3;
	}
	div_frac = wlc_phy_qdiv_roundup(div_frac, fref3, 20);

	/* Program PLL */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_1, (0x1f << 0), (uint8)(div_int >> 4));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_2, (0x1f << 4), (uint8)(div_int << 4));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_2, (0x0f << 0), (uint8)(div_frac >> 16));
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_3, (uint8)(div_frac >> 8) & 0xff);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_4, (uint8)div_frac & 0xff);

	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_1, 0xb9);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_2, 0x88);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_3, 0x28);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_4, 0x63);

	/* PLL_cp_current */
	qVco = ((PLL_2063_HIGH_END_KVCO - PLL_2063_LOW_END_KVCO) *
		(qFvco - FIXED(PLL_2063_LOW_END_VCO)) /
		(PLL_2063_HIGH_END_VCO - PLL_2063_LOW_END_VCO)) +
		FIXED(PLL_2063_LOW_END_KVCO);
	iVal = ((PLL_2063_D30 - 680)  + (490 >> 1))/ 490;
	qVal = wlc_phy_qdiv_roundup(
		440 * PLL_2063_LOOP_BW * div_int,
		27 * (68 + (iVal * 49)), 16);
	kpd_scale = ((qVal + qVco - 1) / qVco) > 60 ? 1 : 0;
	if (kpd_scale)
		cp_current = ((qVal + qVco) / (qVco << 1)) - 8;
	else
		cp_current = ((qVal + (qVco >> 1)) / qVco) - 8;
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_2, 0x3f, cp_current);

	/*  PLL_Kpd_scale2 */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_2, 1 << 6, (kpd_scale << 6));

	/* PLL_offset_current */
	qVal = wlc_phy_qdiv_roundup(100 * qFref, qFvco, 16) * (cp_current + 8) * (kpd_scale + 1);
	ioff_scale = (qVal > FIXED(150)) ? 1 : 0;
	qVal = (qVal / (6 * (ioff_scale + 1))) - FIXED(2);
	if (qVal < 0)
		offset_current = 0;
	else
		offset_current = FLOAT(qVal + (FIXED(1) >> 1));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_3, 0x1f, offset_current);

	/*  PLL_ioff_scale2 */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_3, 1 << 5, ioff_scale << 5);

	/* PLL_pd_div2_BB */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 1 << 2, rfpll_doubler << 2);

	/* PLL_cal_xt_endiv */
	if (!rfpll_doubler || (PHY_XTALFREQ(pi->xtalfreq) > 26000000))
		or_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0x02);
	else
		and_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0xfd);

	/* PLL_cal_xt_sdiv */
	if (!rfpll_doubler && (PHY_XTALFREQ(pi->xtalfreq) > 26000000))
		or_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0x01);
	else
		and_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0xfe);

	/* PLL_sel_short */
	if (qFref > FIXED(45))
		or_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCO_1, 0x02);
	else
		and_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_VCO_1, 0xfd);

	mod_radio_reg(pi, RADIO_2063_PLL_SP_2, 0x03, 0x03);
	OSL_DELAY(1);
	mod_radio_reg(pi, RADIO_2063_PLL_SP_2, 0x03, 0);

	/* Force VCO cal */
	wlc_phy_radio_2063_vco_cal(pi);

	/* Restore state */
	write_radio_reg(pi, RADIO_2063_COMMON_15, rf_common15);
}

static void
wlc_radio_2062_channel_tune(phy_info_t *pi, uint8 channel)
{
	uint i;
	const chan_info_2062_t *ci;
	uint32 xtal, pdiv, fvco, vcoCount, to, wb_div, wb_cur, wb_rem, wb_top;

	/* lookup radio-chip-specific channel code */
	for (i = 0; i < ARRAYSIZE(chan_info_2062); i++)
		if (chan_info_2062[i].chan == channel)
			break;

	if (i >= ARRAYSIZE(chan_info_2062)) {
		PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
		          pi->sh->unit, __FUNCTION__, channel));
		return;
	}

	ci = &chan_info_2062[i];

	/* PowerUp VCO cal clock */
	or_radio_reg(pi, RADIO_2062_RFPLL_CTRL14_SOUTH, 0x04);

	/* Radio tunables */
	write_radio_reg(pi, RADIO_2062_LGENA_TUNE0_NORTH, ci->RF_lgena_tune0);
	write_radio_reg(pi, RADIO_2062_LGENA_TUNE2_NORTH, ci->RF_lgena_tune2);
	write_radio_reg(pi, RADIO_2062_LGENA_TUNE3_NORTH, ci->RF_lgena_tune3);
	write_radio_reg(pi, RADIO_2062_TX_TUNE_NORTH, ci->RF_tx_tune);
	write_radio_reg(pi, RADIO_2062_LGENG_CTRL01_SOUTH, ci->RF_lgeng_ctrl01);
	write_radio_reg(pi, RADIO_2062_LGENA_CTRL5_NORTH, ci->RF_lgena_ctrl5);
	write_radio_reg(pi, RADIO_2062_LGENA_CTRL6_NORTH, ci->RF_lgena_ctrl6);
	write_radio_reg(pi, RADIO_2062_TX_PGA_NORTH, ci->RF_tx_pga);
	write_radio_reg(pi, RADIO_2062_TX_PAD_NORTH, ci->RF_tx_pad);

	xtal = PHY_XTALFREQ(pi->xtalfreq) / 1000;
	pdiv = pi->u.pi_lpphy->pdiv * 1000;

	/* Set VCO bias current */
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL33_SOUTH, 0xcc);
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL34_SOUTH, 0x07);
	wlc_2062_reset_pll_bias(pi);

	 /* FVCO */
	if (ci->freq > 3000)
		fvco = ci->freq * pdiv;
	else
		fvco = 2 * ci->freq * pdiv;

	/* Wildbase calculations */
	wb_div = xtal * 3 * (1 << 4);

	/* Wildbase 3 */
	wb_top = fvco;
	wb_cur = wb_top / wb_div;
	wb_rem = wb_top % wb_div;
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL26_SOUTH, (uint8)wb_cur);
	PHY_TMP(("wl%d: %s: RFPLL_CTRL26: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL26_SOUTH)));

	/* Wildbase 2 */
	wb_top = wb_rem * (1 << 8);
	wb_cur = wb_top / wb_div;
	wb_rem = wb_top % wb_div;
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL27_SOUTH, (uint8)wb_cur);
	PHY_TMP(("wl%d: %s: RFPLL_CTRL27: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL27_SOUTH)));

	/* Wildbase 1 */
	wb_top = wb_rem * (1 << 8);
	wb_cur = wb_top / wb_div;
	wb_rem = wb_top % wb_div;
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL28_SOUTH, (uint8)wb_cur);
	PHY_TMP(("wl%d: %s: RFPLL_CTRL28: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL28_SOUTH)));

	/* Wildbase 0 */
	wb_top = wb_rem * (1 << 8);
	wb_cur = wb_top / wb_div;
	wb_rem = wb_top % wb_div;
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL29_SOUTH,
		(uint8)(wb_cur + ((2 * wb_rem) / wb_div)));
	PHY_TMP(("wl%d: %s: RFPLL_CTRL29: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL29_SOUTH)));

	/* VCO count */
	to = read_radio_reg(pi, RADIO_2062_RFPLL_CTRL19_SOUTH);
	vcoCount = ((2 * fvco * (to + 1)) + (3 *xtal)) / (6 * xtal);
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL23_SOUTH, (uint8)((vcoCount >> 8) + 16));
	PHY_TMP(("wl%d: %s: RFPLL_CTRL23: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL23_SOUTH)));
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL24_SOUTH, (uint8)(vcoCount & 0xff));
	PHY_TMP(("wl%d: %s: RFPLL_CTRL24: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL24_SOUTH)));

	/*  Toggle VCO cal */
	wlc_2062_vco_cal(pi);

	/* Check if PLL charge pump is out of range */
	if (read_radio_reg(pi, RADIO_2062_RFPLL_CTRL03_SOUTH) & 0x10) {
		PHY_INFORM(("wl%d: %s: PLL charge pump out of range\n",
			pi->sh->unit, __FUNCTION__));
		/* Adjust VCO bias */
		write_radio_reg(pi, RADIO_2062_RFPLL_CTRL33_SOUTH, 0xfc);
		write_radio_reg(pi, RADIO_2062_RFPLL_CTRL34_SOUTH, 0x00);
		wlc_2062_reset_pll_bias(pi);

		/*  Redo VCO cal */
		wlc_2062_vco_cal(pi);
		if (0 != (read_radio_reg(pi, RADIO_2062_RFPLL_CTRL03_SOUTH) & 0x10))
			PHY_ERROR(("wl%d: %s: PLL charge pump remains out of range\n",
				pi->sh->unit, __FUNCTION__));
	}

	/* PowerDown VCO cal clock */
	and_radio_reg(pi, RADIO_2062_RFPLL_CTRL14_SOUTH, (uint8)~0x04);
}

void
wlc_phy_radio_2062_check_vco_cal(phy_info_t *pi)
{
	/* Check for radio off, if off then dont try to tune. */
	if (!pi->radio_is_on)
		return;

	/* Monitor vctrl range and relock PLL if out of range */
	if (read_radio_reg(pi, RADIO_2062_RFPLL_CTRL03_SOUTH) & 0x10) {
		bool suspend = !(R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);

		PHY_INFORM(("wl%d: %s: vctrl out of range, trigger VCO cal\n",
			pi->sh->unit, __FUNCTION__));

		if (!suspend)
			wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_radio_2062_channel_tune(pi, CHSPEC_CHANNEL(pi->radio_chanspec));
		if (!suspend)
			wlapi_enable_mac(pi->sh->physhim);
	}
}

static void
WLBANDINITFN(wlc_radio_2063_init)(phy_info_t *pi)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	PHY_INFORM(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Load registers from the table */
	if (ISLPPHY(pi)) {
		if (LPREV_IS(pi->pubpi.phy_rev, 2)) {
			/* for radio rev 0 */
			wlc_phy_init_radio_regs(pi, regs_2063_rev0, RADIO_DEFAULT_CORE);
		}
		else if (LPREV_GT(pi->pubpi.phy_rev, 2)) {
			/* for radio rev 1 */
			wlc_phy_init_radio_regs(pi, regs_2063_rev1, RADIO_DEFAULT_CORE);
		}
	}

	if (LPREV_GE(pi->pubpi.phy_rev, 4) ||
	     (LPREV_GE(pi->pubpi.phy_rev, 3) && (pi->sh->chiprev >= 3))) {
		uint16 temp;
		temp = phy_reg_read(pi, LPPHY_REV2_extslnactrl0);
		temp = (uint16)(temp & 0xff80);
		temp = temp  | 2;
		PHY_REG_WRITE(pi, LPPHY_REV2, extslnactrl0, temp);
	}

	/* Set some PLL registers overridden by DC/CLB */
	write_radio_reg(pi, RADIO_2063_LOGEN_SP_5, 0x0);

	or_radio_reg(pi, RADIO_2063_COMMON_08, (0x07 << 3));
	write_radio_reg(pi, RADIO_2063_BANDGAP_CTRL_1, 0x56);

	/* Set rx lpf bw to 9MHz */
	mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, 0x1 << 1, 0);

	/*
	* Apply rf reg settings to mitigate 2063 spectrum
	* asymmetry problems, including setting
	* PA and PAD in class A mode
	*/
	write_radio_reg(pi, RADIO_2063_PA_SP_7, 0);

	/* pga/pad */
	write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0x20);
	if (pi_lp->lpphy_txrf_sp_9_override != -1) {
		write_radio_reg(pi, RADIO_2063_TXRF_SP_9, (uint16) pi_lp->lpphy_txrf_sp_9_override);
	} else {
		write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0xff);	/* was 0x40 */
	}
	write_radio_reg(pi, RADIO_2063_PA_SP_7, 0);
	if (ISLPPHY(pi)) {
		if (LPREV_IS(pi->pubpi.phy_rev, 2)) {
			/*  pa cascode voltage */
			write_radio_reg(pi, RADIO_2063_PA_SP_3, 0xa0);
			/*  PA, PAD class A settings */
			write_radio_reg(pi, RADIO_2063_PA_SP_4, 0xa0);
			write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x18);
		} else {
			/*  pa cascode voltage */
			write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x20);
			/*  PA, PAD class A settings */
			write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x20);
			write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x20);
		}
	}
}


typedef struct {
	uint16 fref_khz;
	uint8 c1;
	uint8 c2;
	uint8 c3;
	uint8 c4;
	uint8 r1;
	uint8 r2;
} loop_filter_2062_t;

static const
loop_filter_2062_t WLBANDINITDATA(loop_filter_2062)[] = {
	{12000, 6, 6, 6, 6, 10, 6 },
	{13000, 4, 4, 4, 4, 11, 7 },
	{14400, 3, 3, 3, 3, 12, 7 },
	{16200, 3, 3, 3, 3, 13, 8 },
	{18000, 2, 2, 2, 2, 14, 8 },
	{19200, 1, 1, 1, 1, 14, 9 }
};

static void
WLBANDINITFN(wlc_radio_2062_init)(phy_info_t *pi)
{
	uint32 pdiv_mhz;
	uint16 fref_khz;
	const loop_filter_2062_t *rc_vals = &loop_filter_2062[0];
	uint i;

	PHY_INFORM(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Load registers from the table */
	wlc_phy_init_radio_regs(pi, regs_2062, RADIO_DEFAULT_CORE);

	/* Reset tx IQLO coeffs */
	write_radio_reg(pi, RADIO_2062_TX_CTRL3_NORTH, 0);
	write_radio_reg(pi, RADIO_2062_TX_CTRL4_NORTH, 0);
	write_radio_reg(pi, RADIO_2062_TX_CTRL5_NORTH, 0);
	write_radio_reg(pi, RADIO_2062_TX_CTRL6_NORTH, 0);

	/* Do RCAL */
	write_radio_reg(pi, RADIO_2062_PDN_CTRL0_NORTH,  0x40);
	write_radio_reg(pi, RADIO_2062_PDN_CTRL0_NORTH, 0x00);
	write_radio_reg(pi, RADIO_2062_CAL_TS_NORTH, 0x10);
	write_radio_reg(pi, RADIO_2062_CAL_TS_NORTH, 0x00);

	/* apply RCAL setting */
	if (LPREV_GT(pi->pubpi.phy_rev, 0)) {
		write_radio_reg(pi, RADIO_2062_BG_CTRL1_SOUTH,
			(read_radio_reg(pi, RADIO_2062_COMMON_02_NORTH) >> 1) | (1 << 7));
	}

	if (CHSPEC_IS2G(pi->radio_chanspec))
		or_radio_reg(pi, RADIO_2062_TSSI_CTRL0_NORTH, 0x01);
	else
		and_radio_reg(pi, RADIO_2062_TSSI_CTRL0_NORTH, ~(uint8)0x01);

	/* Program PLL divider */
	if (PHY_XTALFREQ(pi->xtalfreq) > (uint32)30000000) {
		pi->u.pi_lpphy->pdiv = 2;
		or_radio_reg(pi, RADIO_2062_RFPLL_CTRL01_SOUTH, 0x04);
	} else {
		pi->u.pi_lpphy->pdiv = 1;
		and_radio_reg(pi, RADIO_2062_RFPLL_CTRL01_SOUTH, ~0x04);
	}
	pdiv_mhz = pi->u.pi_lpphy->pdiv * 1000000;

	/* Set KPD_SCALE */
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL07_SOUTH,
		(uint8)((((800 * pdiv_mhz) + PHY_XTALFREQ(pi->xtalfreq)) /
		(2 * PHY_XTALFREQ(pi->xtalfreq))) - 8));
	PHY_TMP(("wl%d: %s: RFPLL_CTRL07: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL07_SOUTH)));

	/* VCO calibration delay after RF */
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL18_SOUTH,
		(uint8)((((PHY_XTALFREQ(pi->xtalfreq) * 10) + ((pdiv_mhz << 4)/10)) /
		((pdiv_mhz << 5)/10)) - 1));
	PHY_TMP(("wl%d: %s: RFPLL_CTRL18: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL18_SOUTH)));

	/* Timeout */
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL19_SOUTH,
		(uint8)(((2 * PHY_XTALFREQ(pi->xtalfreq)) + pdiv_mhz) / (2 * pdiv_mhz)) - 1);
	PHY_TMP(("wl%d: %s: RFPLL_CTRL19: %x\n",
		pi->sh->unit, __FUNCTION__, read_radio_reg(pi, RADIO_2062_RFPLL_CTRL19_SOUTH)));

	/* Set loop filter */
	fref_khz = (uint16)(((PHY_XTALFREQ(pi->xtalfreq) * 2) + (pi->u.pi_lpphy->pdiv * 1000)) /
		(pi->u.pi_lpphy->pdiv * 2000));
	for (i = 0; i < ARRAYSIZE(loop_filter_2062); i++) {
		if (loop_filter_2062[i].fref_khz > fref_khz) {
			break;
		}
		rc_vals = &loop_filter_2062[i];
	}
	PHY_TMP(("wl%d: %s: loop filter values: %d %d %d %d %d %d\n",
		pi->sh->unit, __FUNCTION__,
		rc_vals->c1,  rc_vals->c2,  rc_vals->c3,  rc_vals->c4,  rc_vals->r1,  rc_vals->r2));
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL08_SOUTH,
		((rc_vals->c2 & 0xff) << 4) | (rc_vals->c1 & 0x0f));
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL09_SOUTH,
		((rc_vals->c4 & 0xff) << 4) | (rc_vals->c3 & 0x0f));
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL10_SOUTH, rc_vals->r1);
	write_radio_reg(pi, RADIO_2062_RFPLL_CTRL11_SOUTH, rc_vals->r2);
}


/* %%%%%% major flow operations */
void
wlc_phy_txpower_recalc_target_lpphy(phy_info_t *pi)
{
	uint16 pwr_ctrl;

	if (!wlc_lpphy_tssi_enabled(pi))
		return;

	/* Temporary disable power control to update settings */
	pwr_ctrl = wlc_lpphy_get_tx_pwr_ctrl(pi);
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_OFF);
	wlc_phy_txpower_recalc_target2_lpphy(pi);
	/* Restore power control */
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, pwr_ctrl);
}

void
wlc_phy_detach_lpphy(phy_info_t *pi)
{
	MFREE(pi->sh->osh, pi->u.pi_lpphy, sizeof(phy_info_lpphy_t));
}


bool
wlc_phy_attach_lpphy(phy_info_t *pi)
{
	phy_info_lpphy_t *pi_lp;

	pi->u.pi_lpphy = (phy_info_lpphy_t*)MALLOC(pi->sh->osh, sizeof(phy_info_lpphy_t));
	if (pi->u.pi_lpphy == NULL) {
	PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return FALSE;
	}
	bzero((char *)pi->u.pi_lpphy, sizeof(phy_info_lpphy_t));

	pi_lp = pi->u.pi_lpphy;

	if (pi->sh->sromrev < 3) {
		PHY_ERROR(("wl%d: LPPHY requires SROM ver >= 3(%d)\n",
			pi->sh->unit, pi->sh->sromrev));
		return FALSE;
	}

#if defined(PHYCAL_CACHING)
	/* Reset the var as no cal cache context should exist yet */
	pi->phy_calcache_num = 0;
#endif

	if (((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_NOPA) == 0) &&
	    !NORADIO_ENAB(pi->pubpi)) {
		pi->hwpwrctrl = TRUE;
		pi->hwpwrctrl_capable = TRUE;
	}

	/* Get xtal frequency from PMU */
#if !defined(XTAL_FREQ)
	pi->xtalfreq = si_alp_clock(pi->sh->sih);
#endif
	ASSERT((PHY_XTALFREQ(pi->xtalfreq) % 1000) == 0);

	PHY_INFORM(("wl%d: %s: using %d.%d MHz xtalfreq for RF PLL\n",
		pi->sh->unit, __FUNCTION__,
		PHY_XTALFREQ(pi->xtalfreq) / 1000000, PHY_XTALFREQ(pi->xtalfreq) % 1000000));

	pi_lp->lpphy_rx_gain_temp_adj_tempsense = 0;
	pi_lp->lpphy_rx_gain_temp_adj_thresh[0] = 62;
	pi_lp->lpphy_rx_gain_temp_adj_thresh[1] = 58;
	pi_lp->lpphy_rx_gain_temp_adj_thresh[2] = 53;
	pi_lp->lpphy_rx_gain_temp_adj_tempsense_metric = 0;
	pi_lp->lpphy_rx_gain_temp_adj_metric = 0;

	pi->pi_fptr.init = wlc_phy_init_lpphy;
	pi->pi_fptr.calinit = wlc_phy_cal_init_lpphy;
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_lpphy;
	pi->pi_fptr.txpwrrecalc = wlc_phy_txpower_recalc_target_lpphy;
#if defined(BCMDBG) || defined(WLTEST)
	pi->pi_fptr.longtrn = wlc_phy_lpphy_long_train;
#endif
	pi->pi_fptr.txiqccget = wlc_phy_get_tx_iqcc_lpphy;
	pi->pi_fptr.txiqccset = wlc_phy_set_tx_iqcc_lpphy;
	pi->pi_fptr.txloccget = wlc_phy_get_tx_locc_lpphy;
	pi->pi_fptr.radioloftget = wlc_phy_get_radio_loft_lpphy;
#if defined(WLTEST)
	pi->pi_fptr.carrsuppr = wlc_phy_carrier_suppress_lpssnphy;
#endif
	pi->pi_fptr.rxsigpwr = wlc_phy_rx_signal_power_lpphy;
	pi->pi_fptr.detach = wlc_phy_detach_lpphy;

	if (!wlc_phy_txpwr_srom_read_lpphy(pi))
		return FALSE;

	return TRUE;
}

/* %%%%%% testing */
#if defined(BCMDBG) || defined(WLTEST)
int
wlc_phy_lpphy_long_train(phy_info_t *pi, int channel)
{
	uint16 num_samps;
	lpphytbl_info_t tab;

	/* stop any test in progress */
	wlc_phy_test_stop(pi);

	/* channel 0 means restore original contents and end the test */
	if (channel == 0) {
		wlc_phy_stop_tx_tone_lpphy(pi);
		wlc_phy_clear_deaf_lpphy(pi, (bool)1);
		return 0;
	}

	if (wlc_phy_test_init(pi, channel, TRUE)) {
		return 1;
	}

	wlc_phy_set_deaf_lpphy(pi, (bool)1);

	num_samps = sizeof(ltrn_list)/sizeof(*ltrn_list);
	/* load sample table */
	tab.tbl_ptr = ltrn_list;
	tab.tbl_len = num_samps;
	tab.tbl_id = LPPHY_TBL_ID_SAMPLEPLAY;
	tab.tbl_offset = 0;
	tab.tbl_width = 16;
	tab.tbl_phywidth = 16; /* width in the phy address space */
	wlc_phy_table_write_lpphy(pi, &tab);

	wlc_lpphy_run_samples(pi, num_samps, 0xffff, 0);

	return 0;
}

void
wlc_phy_init_test_lpphy(phy_info_t *pi)
{
	phy_info_lpphy_t *pi_lp = pi->u.pi_lpphy;

	if (LPREV_LT(pi->pubpi.phy_rev, 2))
		return;

	/* Force WLAN antenna */
	wlc_lpphy_btcx_override_enable(pi);

	if (wlc_phy_tpc_isenabled_lpphy(pi)) {
		/* override tx gain index with last known tx power control index */
		wlc_phy_tx_pwr_update_npt_lpphy(pi);
		wlc_phy_set_tx_pwr_by_index_lpphy(pi, pi_lp->lpphy_tssi_idx);
	}

	/* Disable tx power control */
	wlc_phy_set_tx_pwr_ctrl_lpphy(pi, LPPHY_TX_PWR_CTRL_OFF);
	PHY_REG_MOD(pi, LPPHY_REV2, papdctrl, forcepapdClkOn, 1);
	/* Recalibrate for this channel */
	wlc_lpphy_full_cal(pi);
}

/* Force transmit chain on */
void
wlc_phy_tx_pu_lpphy(phy_info_t *pi, bool bEnable)
{
	if (!bEnable) {
		/* Clear overrides */
		PHY_REG_AND(pi, LPPHY, RFOverride0,
			~(uint16)(LPPHY_RFOverride0_gmode_tx_pu_ovr_MASK |
			LPPHY_RFOverride0_trsw_rx_pu_ovr_MASK |
			LPPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
			LPPHY_RFOverride0_ant_selp_ovr_MASK));
		PHY_REG_AND(pi, LPPHY, AfeCtrlOvr,
			~(uint16)(LPPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
			LPPHY_AfeCtrlOvr_dac_clk_disable_ovr_MASK));

		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_AND(pi, LPPHY_REV2, rfoverride3,
				~(uint16)(LPPHY_REV2_rfoverride3_stxpapu_ovr_MASK |
				LPPHY_REV2_rfoverride3_stxpadpu2g_ovr_MASK |
				LPPHY_REV2_rfoverride3_stxpapu2g_ovr_MASK));
		}
	} else {
		PHY_REG_LIST_START
			/* Force on DAC */
			PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvr, pwdn_dac_ovr, 1)
			/* Force on AFE */
			PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvrVal, pwdn_dac_ovr_val, 0)
			PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvr, dac_clk_disable_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY, AfeCtrlOvrVal, dac_clk_disable_ovr_val, 0)
			/* Force PA on */
			PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, gmode_tx_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, gmode_tx_pu_ovr_val, 1)
		PHY_REG_LIST_EXECUTE(pi);

		/* Force the TR switch to transmit */
		wlc_lpphy_set_trsw_override(pi, TRUE, FALSE);

		PHY_REG_LIST_START
			/* Force antenna  0 */
			PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, ant_selp_ovr_val, 0)
			PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, ant_selp_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);

		if (LPREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_LIST_START
				/* Force on the transmit chain */
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverride0, internalrftxpu_ovr, 1)
				PHY_REG_MOD_ENTRY(LPPHY_REV2, RFOverrideVal0,
					internalrftxpu_ovr_val, 1)
			PHY_REG_LIST_EXECUTE(pi);

			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				PHY_REG_LIST_START
					/* PAP PU */
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3,
						stxpadpu2g_ovr, 1)
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val,
						stxpadpu2g_ovr_val, 1)
					/* PGA PU */
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpapu2g_ovr, 1)
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val,
						stxpapu2g_ovr_val, 1)
					/* PA PU */
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpapu_ovr, 1)
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val,
						stxpapu_ovr_val, 1)
				PHY_REG_LIST_EXECUTE(pi);
			} else {
				PHY_REG_LIST_START
					/* PAP PU */
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3,
						stxpadpu2g_ovr, 1)
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val,
						stxpadpu2g_ovr_val, 0)
					/* PGA PU */
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpapu2g_ovr, 1)
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val,
						stxpapu2g_ovr_val, 0)
					/* PA PU */
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3, stxpapu_ovr, 1)
					PHY_REG_MOD_ENTRY(LPPHY_REV2, rfoverride3_val,
						stxpapu_ovr_val, 0)
				PHY_REG_LIST_EXECUTE(pi);
			}
		} else {
			PHY_REG_LIST_START
				/* Force on the transmit chain */
				PHY_REG_MOD_ENTRY(LPPHY, RFOverride0, internalrftxpu_ovr, 1)
				PHY_REG_MOD_ENTRY(LPPHY, RFOverrideVal0, internalrftxpu_ovr_val, 1)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}
}
#endif 

#if defined(PHYCAL_CACHING) && defined(BCMDBG)
static void
wlc_phy_cal_cache_dbg_lpphy(ch_calcache_t *ctx)
{
	uint i;
	lpphy_calcache_t *cache = &ctx->u.lpphy_cache;

	/* Generic parameters */
	PHY_INFORM(("_____Generic params_____\n"));
	PHY_INFORM(("%d\n", CHSPEC_CHANNEL(ctx->chanspec)));
	PHY_INFORM(("last_cal_time: %d\n", ctx->cal_info.last_cal_time));
	PHY_INFORM(("last_cal_temp: %d\n", ctx->cal_info.last_cal_temp));
	PHY_INFORM(("lpphy_papd_tx_gain_at_last_cal: %d\n", cache->lpphy_papd_tx_gain_at_last_cal));
	PHY_INFORM(("lpphy_papd_cal_gain_index: %d\n", cache->lpphy_papd_cal_gain_index));

	/* TX IQ LO cal results */
	PHY_INFORM(("_____TX IQ LO cal results_____\n"));
	PHY_INFORM(("txiqlocal_bestcoeffs: \n"));
	for (i = 0; i < 11; i++) {
		PHY_INFORM(("[%u]:0x%x\n", i, cache->txiqlocal_bestcoeffs[i]));
	}
	PHY_INFORM(("\ntxiqlocal_bestcoeffs_valid: %d\n", cache->txiqlocal_bestcoeffs_valid));
	PHY_INFORM(("\ndidq_cck: %d\n", cache->didq_cck));

	/* PAPD results */
	PHY_INFORM(("_____PAPD results_____\n"));
	PHY_INFORM(("papd_eps_tbl: \n"));
	for (i = 0; i < PHY_PAPD_EPS_TBL_SIZE_LPPHY; i++) {
		PHY_INFORM(("[%u]:0x%x\n", i, cache->papd_eps_tbl[i]));
	}
	PHY_INFORM(("\npapd_indexOffset: %d\n", cache->papd_indexOffset));
	PHY_INFORM(("\npapd_startstopindex: %d\n", cache->papd_startstopindex));

	/* RX IQ cal results */
	PHY_INFORM(("_____RX IQ cal results_____\n"));
	PHY_INFORM(("rxiqcal_coeffs: %d\n", cache->rxiqcal_coeffs));

	cache = NULL;
}
#endif /* PHYCAL_CACHING */

#if defined(WLTEST)
static void
wlc_lpphy_reset_radio_loft(phy_info_t *pi)
{
	if (BCM2062_ID == LPPHY_RADIO_ID(pi)) {
		write_radio_reg(pi, RADIO_2062_TX_CTRL3_NORTH, 0x0);
		write_radio_reg(pi, RADIO_2062_TX_CTRL4_NORTH, 0x0);
		write_radio_reg(pi, RADIO_2062_TX_CTRL5_NORTH, 0x0);
		write_radio_reg(pi, RADIO_2062_TX_CTRL6_NORTH, 0x0);
	} else if (BCM2063_ID == LPPHY_RADIO_ID(pi)) {
		write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I, 0x88);
		write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q, 0x88);
		write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I, 0x88);
		write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q, 0x88);
	}
}

void
wlc_phy_carrier_suppress_lpssnphy(phy_info_t *pi)
{
	if (ISLPPHY(pi)) {
		wlc_lpphy_reset_radio_loft(pi);
		if (wlc_phy_tpc_isenabled_lpphy(pi))
			wlc_lpphy_clear_tx_power_offsets(pi);
		else
			wlc_phy_set_tx_locc_lpphy(pi, 0);
	} else
		ASSERT(0);

}
#endif 
