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
 * $Id: wlc_phy_ac.c 379781 2013-01-18 23:43:30Z $
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
#include <wlc_phy_ac.h>
#include <sbchipc.h>
#include <bcmotp.h>

#include "wlc_phyreg_ac.h"
#include "wlc_phytbl_ac.h"
#include "wlc_phytbl_acdc.h"

#define ACPHYREGCE(reg, core) ((ACPHY_##reg##0) + ((core) * 0x200))
#define ACPHYREGC(reg, core) ((ACPHY_Core0##reg) + ((core) * 0x200))

#define MOD_PHYREG(pi, reg, field, value)				\
	phy_reg_mod(pi, reg,						\
	            reg##_##field##_MASK, (value) << reg##_##field##_##SHIFT);

#define MOD_PHYREGC(pi, reg, core, field, value)			\
	phy_reg_mod(pi, \
	(core == 0) ? ACPHY_Core0##reg : \
	((core == 1) ? ACPHY_Core1##reg : ACPHY_Core2##reg), \
	(core == 0) ? ACPHY_Core0##reg##_##field##_MASK : \
	((core == 1) ? ACPHY_Core1##reg##_##field##_MASK : ACPHY_Core2##reg##_##field##_MASK), \
	(core == 0) ? ((value) << ACPHY_Core0##reg##_##field##_SHIFT) : \
	((core == 1) ? ((value) << ACPHY_Core1##reg##_##field##_SHIFT) : \
	    ((value) << ACPHY_Core2##reg##_##field##_SHIFT)));
#define MOD_PHYREGCE(pi, reg, core, field, value)			\
	phy_reg_mod(pi,							\
	            (core == 0) ? ACPHY_##reg##0 :			\
	            ((core == 1) ? ACPHY_##reg##1 : ACPHY_##reg##2),	\
	            (core == 0) ? ACPHY_##reg##0##_##field##_MASK :	\
	            ((core == 1) ? ACPHY_##reg##1##_##field##_MASK :    \
	             ACPHY_##reg##2##_##field##_MASK),			\
	            (core == 0) ? ((value) << ACPHY_##reg##0##_##field##_SHIFT) : \
	            ((core == 1) ? ((value) << ACPHY_##reg##1##_##field##_SHIFT) : \
	             ((value) << ACPHY_##reg##2##_##field##_SHIFT)));
#define MOD_PHYREGCEE(pi, reg, core, field, value)			\
	phy_reg_mod(pi,							\
	            (core == 0) ? ACPHY_##reg##0 :			\
	            ((core == 1) ? ACPHY_##reg##1 : ACPHY_##reg##2),	\
	            (core == 0) ? ACPHY_##reg##0##_##field##0##_MASK :	\
	            ((core == 1) ? ACPHY_##reg##1##_##field##1##_MASK :    \
	             ACPHY_##reg##2##_##field##2##_MASK),			\
	            (core == 0) ? ((value) << ACPHY_##reg##0##_##field##0##_SHIFT) : \
	            ((core == 1) ? ((value) << ACPHY_##reg##1##_##field##1##_SHIFT) : \
	             ((value) << ACPHY_##reg##2##_##field##2##_SHIFT)));
#define READ_PHYREG(pi, reg, field)				\
	((phy_reg_read(pi, reg)					\
	  & reg##_##field##_##MASK) >> reg##_##field##_##SHIFT)
#define READ_PHYREGC(pi, reg, core, field) \
	((phy_reg_read(pi, ACPHYREGC(reg, core))\
	& ((core == 0) ? ACPHY_Core0##reg##_##field##_MASK :	\
		((core == 1) ? ACPHY_Core1##reg##_##field##_MASK :    \
		ACPHY_Core2##reg##_##field##_MASK)))  \
	>> ((core == 0) ? ACPHY_Core0##reg##_##field##_SHIFT : \
		((core == 1) ? ACPHY_Core1##reg##_##field##_SHIFT : \
		ACPHY_Core2##reg##_##field##_SHIFT)));
#define READ_PHYREGCE(pi, reg, core, field) \
	((phy_reg_read(pi, ACPHYREGCE(reg, core))\
	& ((core == 0) ? ACPHY_##reg##0##_##field##_MASK :	\
		((core == 1) ? ACPHY_##reg##1##_##field##_MASK :    \
		ACPHY_##reg##2##_##field##_MASK)))  \
	>> ((core == 0) ? ACPHY_##reg##0##_##field##_SHIFT : \
		((core == 1) ? ACPHY_##reg##1##_##field##_SHIFT : \
		ACPHY_##reg##2##_##field##_SHIFT)));

#define MOD_RADIO_REG(pi, regpfx, regnm, fldname, value) \
	mod_radio_reg(pi, \
	              regpfx##_2069_##regnm, \
	              RF_2069_##regnm##_##fldname##_MASK, \
	              (value) << RF_2069_##regnm##_##fldname##_SHIFT);

#define MOD_RADIO_REGFLDC(pi, regnmcr, regnm, fldname, value) \
	mod_radio_reg(pi, \
	              regnmcr, \
	              RF_2069_##regnm##_##fldname##_MASK, \
	              (value) << RF_2069_##regnm##_##fldname##_SHIFT);

#define READ_RADIO_REGFLD(pi, regpfx, regnm, fldname) \
	(read_radio_reg(pi, regpfx##_2069_##regnm) & \
	              RF_2069_##regnm##_##fldname##_MASK) \
	              >> RF_2069_##regnm##_##fldname##_SHIFT;

#define READ_RADIO_REGFLDC(pi, regnmcr, regnm, fldname) \
	(read_radio_reg(pi, regnmcr) & \
	              RF_2069_##regnm##_##fldname##_MASK) \
	              >> RF_2069_##regnm##_##fldname##_SHIFT;
#define ACPHYREG_BCAST(pi, reg, val) phy_reg_write(pi, reg | ACPHY_REG_BROADCAST, val);
#define ACPHY_DISABLE_STALL(pi)	\
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls, 1);

#define ACPHY_ENABLE_STALL(pi, stall_val) \
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls, stall_val);

#define WLC_PHY_PRECAL_TRACE(tx_idx, target_gains) \
	PHY_TRACE(("Index was found to be %d\n", tx_idx)); \
	PHY_TRACE(("Gain Code was found to be : \n")); \
	PHY_TRACE(("radio gain = 0x%x%x%x, bbm=%d, dacgn = %d  \n", \
		target_gains->rad_gain_hi, \
		target_gains->rad_gain_mi, \
		target_gains->rad_gain, \
		target_gains->bbmult, \
		target_gains->dac_gain));

#ifndef D11AC_IOTYPES
/* 80 MHz support is included if D11AC_IOTYPES is defined */
#define CHSPEC_IS80(chspec) (0)
#define WL_CHANSPEC_CTL_SB_LL (0)
#define WL_CHANSPEC_CTL_SB_LU (0)
#define WL_CHANSPEC_CTL_SB_UL (0)
#define WL_CHANSPEC_CTL_SB_UU (0)
#endif /* D11AC_IOTYPES */


typedef struct {
	uint8 percent;
	uint8 g_env;
} acphy_txiqcal_ladder_t;

typedef struct {
	uint8 nwords;
	uint8 offs;
	uint8 boffs;
} acphy_coeff_access_t;

typedef struct {
	uint8 idx;
	uint16 val;
} sparse_array_entry_t;

int acphychipid;	/* To select radio offsets depending on chipid */
static uint16 qt_rfseq_val1[] = {0x8b5, 0x8b5, 0x8b5};
static uint16 qt_rfseq_val2[] = {0x0, 0x0, 0x0};
static uint16 rfseq_reset2rx_cmd[] = {0x4, 0x3, 0x6, 0x5, 0x2, 0x1, 0x8,
            0x2a, 0x2b, 0xf, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f, 0x1f};
static uint16 rfseq_reset2rx_dly[] = {12, 2, 2, 4, 4, 6, 1, 4, 1, 2, 1, 1, 1, 1, 1, 1};
static uint16 rfseq_rx2tx_cmd[] =
        {0x0, 0x1, 0x2, 0x8, 0x5, 0x0, 0x6, 0x3, 0xf, 0x4, 0x0, 0x35, 0xf, 0x0, 0x36, 0x1f};
static uint16 rfseq_rx2tx_cmd_withtssisleep[] =
{0x0000, 0x0001, 0x0002, 0x0008, 0x0005, 0x0006, 0x0003, 0x000f, 0x0004, 0x0035,
0x000f, 0x0000, 0x0000, 0x0036, 0x0080, 0x001f};
static uint16 rfseq_rx2tx_dly_withtssisleep[] =
{0x0008, 0x0006, 0x0006, 0x0004, 0x0006, 0x0010, 0x0026, 0x0002, 0x0006, 0x0004,
0x00ff, 0x00ff, 0x00a8, 0x0004, 0x0001, 0x0001};

static uint16 rfseq_tx2rx_cmd[] =
        {0x4, 0x3, 0x6, 0x5, 0x0, 0x2, 0x1, 0x8, 0x2a, 0xf, 0x0, 0xf, 0x2b, 0x1f, 0x1f, 0x1f};

static uint16 rf_updh_cmd_clamp[] = {0x2a, 0x07, 0x0a, 0x00, 0x08, 0x2b, 0x1f, 0x1f};
static uint16 rf_updh_dly_clamp[] = {0x01, 0x02, 0x02, 0x02, 0x10, 0x01, 0x01, 0x01};
static uint16 rf_updl_cmd_clamp[] = {0x2a, 0x07, 0x08, 0x0c, 0x0e, 0x2b, 0x1f, 0x1f};
static uint16 rf_updl_dly_clamp[] = {0x01, 0x06, 0x12, 0x08, 0x10, 0x01, 0x01, 0x01};
static uint16 rf_updu_cmd_clamp[] = {0x2a, 0x07, 0x08, 0x0e, 0x2b, 0x1f, 0x1f, 0x1f};
static uint16 rf_updu_dly_clamp[] = {0x01, 0x06, 0x1e, 0x1c, 0x01, 0x01, 0x01, 0x01};

static uint16 rf_updh_cmd_adcrst[] = {0x07, 0x0a, 0x00, 0x08, 0xb0, 0xb1, 0x1f, 0x1f};
static uint16 rf_updh_dly_adcrst[] = {0x02, 0x02, 0x02, 0x01, 0x0a, 0x01, 0x01, 0x01};
static uint16 rf_updl_cmd_adcrst[] = {0x07, 0x08, 0x0c, 0x0e, 0xb0, 0xb2, 0x1f, 0x1f};
static uint16 rf_updl_dly_adcrst[] = {0x06, 0x12, 0x08, 0x01, 0x0a, 0x01, 0x01, 0x01};
static uint16 rf_updu_cmd_adcrst[] = {0x07, 0x08, 0x0e, 0xb0, 0xb1, 0x1f, 0x1f, 0x1f};
static uint16 rf_updu_dly_adcrst[] = {0x06, 0x1e, 0x1c, 0x0a, 0x01, 0x01, 0x01, 0x01};

static uint16 rfseq_updl_lpf_hpc_ml[] = {0x0aaa, 0x0aaa};
static uint16 rfseq_updl_tia_hpc_ml[] = {0x0222, 0x0222};

static uint16 sdadc_cfg20 = 0xd5eb;
static uint16 sdadc_cfg40 = 0x45ea;
static uint16 sdadc_cfg80 = 0x07f8;

static uint8 ac_lna1_2g[]       = {0xf6, 0xff, 0x6, 0xc, 0x12, 0x19};
static uint8 ac_lna1_5g_20mhz[] = {0xf9, 0xfe, 0x4, 0xa, 0x10, 0x17};
static uint8 ac_lna1_5g_40mhz[] = {0xf8, 0xfd, 0x3, 0x9, 0x0f, 0x16};
static uint8 ac_lna1_5g_80mhz[] = {0xf6, 0xfb, 0x1, 0x7, 0x0d, 0x14};
static uint8 ac_lna2_2g[] = {0xf4, 0xf8, 0xfc, 0xff, 0x2, 0x5, 0x9};
static uint8 ac_lna2_5g[] = {0xf5, 0xf8, 0xfb, 0xfe, 0x2, 0x5, 0x9};


/* defs for iqlo cal */
enum {  /* mode selection for reading/writing tx iqlo cal coefficients */
	TB_START_COEFFS_AB, TB_START_COEFFS_D, TB_START_COEFFS_E, TB_START_COEFFS_F,
	TB_BEST_COEFFS_AB,  TB_BEST_COEFFS_D,  TB_BEST_COEFFS_E,  TB_BEST_COEFFS_F,
	TB_OFDM_COEFFS_AB,  TB_OFDM_COEFFS_D,  TB_BPHY_COEFFS_AB,  TB_BPHY_COEFFS_D,
	PI_INTER_COEFFS_AB, PI_INTER_COEFFS_D, PI_INTER_COEFFS_E, PI_INTER_COEFFS_F,
	PI_FINAL_COEFFS_AB, PI_FINAL_COEFFS_D, PI_FINAL_COEFFS_E, PI_FINAL_COEFFS_F
};

#define ACPHY_IQCAL_TONEFREQ_80MHz 8000
#define ACPHY_IQCAL_TONEFREQ_40MHz 4000
#define ACPHY_IQCAL_TONEFREQ_20MHz 2000
#define ACPHY_RXCAL_MAX_NUM_FREQ 6

#define CAL_TYPE_IQ                 0
#define CAL_TYPE_LOFT_DIG           2
#define CAL_TYPE_LOFT_ANA_FINE      3
#define CAL_TYPE_LOFT_ANA_COARSE    4

#define CAL_COEFF_READ    0
#define CAL_COEFF_WRITE   1
#define MPHASE_TXCAL_CMDS_PER_PHASE  2 /* number of tx iqlo cal commands per phase in mphase cal */

#define IQTBL_CACHE_COOKIE_OFFSET	95
#define TXCAL_CACHE_VALID		0xACDC

#define TXMAC_IFHOLDOFF_DEFAULT		0x12	/* 9.0us */
#define TXMAC_MACDELAY_DEFAULT		0x2a8	/* 8.5us */


#define MAX_PAD_GAIN				0xFF
#define MAX_TX_IDX				127

/* %%%%%% function declaration */
/* Radio Functions */
void wlc_phy_switch_radio_acphy(phy_info_t *pi, bool on);
static void wlc_phy_radio2069_pwron_seq(phy_info_t *pi);
static void wlc_phy_radio2069_upd_prfd_values(phy_info_t *pi);
static void wlc_phy_radio2069_rcal(phy_info_t *pi);
static void wlc_phy_radio2069_rccal(phy_info_t *pi);
static void wlc_phy_chanspec_radio2069_setup(phy_info_t *pi, const chan_info_radio2069_t *ci,
                                     const chan_info_radio2069revGE16_t *ciGE16,
                                     uint8 toggle_logen_reset);
static void wlc_phy_radio2069_vcocal(phy_info_t *pi);
static void wlc_phy_radio2069_afecal(phy_info_t *pi);
static void wlc_phy_radio2069_afecal_invert(phy_info_t *pi);
static void wlc_phy_radio2069_mini_pwron_seq_rev16(phy_info_t *pi);
static void wlc_phy_set_lowpwr_phy_reg(phy_info_t *pi);
void wlc_phy_radio2069_pwrdwn_seq(phy_info_t *pi);
void wlc_phy_radio2069_pwrup_seq(phy_info_t *pi);

/* PHY Functions */
static void wlc_phy_set_tx_iir_coeffs(phy_info_t *pi, bool cck, uint8 filter_type);
static void wlc_phy_detach_acphy(phy_info_t *pi);
static void wlc_phy_init_acphy(phy_info_t *pi);
static void wlc_phy_resetcca_acphy(phy_info_t *pi);
static void wlc_phy_cal_init_acphy(phy_info_t *pi);
static void wlc_phy_edcrs_thresh_acphy(phy_info_t *pi);
static void wlc_phy_chanspec_set_acphy(phy_info_t *pi, chanspec_t chanspec);
static void wlc_phy_set_regtbl_on_pwron_acphy(phy_info_t *pi);
static void wlc_phy_set_reg_on_reset_acphy(phy_info_t *pi);
static void wlc_phy_set_tbl_on_reset_acphy(phy_info_t *pi);
static void wlc_phy_set_regtbl_on_band_change_acphy(phy_info_t *pi);
static void wlc_phy_set_regtbl_on_bw_change_acphy(phy_info_t *pi);
static void wlc_phy_set_regtbl_on_chan_change_acphy(phy_info_t *pi,
                                     const chan_info_radio2069_t *ci,
                                     const chan_info_radio2069revGE16_t *ciGE16);
static void wlc_phy_set_regtbl_on_femctrl(phy_info_t *pi);
static void wlc_phy_set_pdet_on_reset_acphy(phy_info_t *pi);
static void wlc_phy_txpower_recalc_target_acphy(phy_info_t *pi);
static void wlc_phy_get_tssi_floor_acphy(phy_info_t *pi, int16 *floor);
static void wlc_phy_watchdog_acphy(phy_info_t *pi);
static void wlc_phy_subband_cust_2g_acphy(phy_info_t *pi);
static void wlc_phy_subband_cust_5g_acphy(phy_info_t *pi);
static bool wlc_phy_chan2freq_acphy(phy_info_t *pi, uint channel, int *f,
                                     chan_info_radio2069_t **t,
                                     chan_info_radio2069revGE16_t **tGE16);
static bool wlc_phy_chan2freq_acdcphy(phy_info_t *pi, uint channel, int *f,
                                      chan_info_radio20691_t **t);
static void wlc_phy_rfldo_trim_value(phy_info_t *pi);

static void wlc_phy_get_tx_bbmult_acphy(phy_info_t *pi, uint16 *bb_mult, uint16 core);
static void wlc_phy_set_tx_bbmult_acphy(phy_info_t *pi, uint16 *bb_mult, uint16 core);
static void wlc_phy_farrow_setup_acphy(phy_info_t *pi, chanspec_t chanspec);
void wlc_phy_txpwr_by_index_acphy(phy_info_t *pi, uint8 core_mask, int8 txpwrindex);
static void wlc_phy_get_txgain_settings_by_index_acphy(phy_info_t *pi,
                                     txgain_setting_t *txgain_settings, int8 txpwrindex);
static void wlc_phy_runsamples_acphy(phy_info_t *pi, uint16 num_samps, uint16 loops,
                                     uint16 wait, uint8 iqmode, uint8 mac_based);
static void wlc_phy_loadsampletable_acphy(phy_info_t *pi, cint32 *tone_buf, uint16 num_samps);
static uint16 wlc_phy_gen_load_samples_acphy(phy_info_t *pi, int32 f_kHz, uint16 max_val,
                                             uint8 mac_based);
int wlc_phy_tx_tone_acphy(phy_info_t *pi, int32 f_kHz, uint16 max_val, uint8 iqmode,
                                     uint8 mac_based, bool modify_bbmult);
void wlc_phy_stopplayback_acphy(phy_info_t *pi);
static void wlc_phy_runsamples_acphy(phy_info_t *pi, uint16 num_samps, uint16 loops,
                                     uint16 wait, uint8 iqmode, uint8 mac_based);
static int  wlc_phy_cal_txiqlo_acphy(phy_info_t *pi, uint8 searchmode, uint8 mphase);
static void wlc_phy_cal_txiqlo_coeffs_acphy(phy_info_t *pi,
                                            uint8 rd_wr, uint16 *coeffs, uint8 select, uint8 core);
static void wlc_phy_precal_txgain_acphy(phy_info_t *pi, txgain_setting_t *target_gains);

static void wlc_phy_precal_target_tssi_search(phy_info_t *pi, txgain_setting_t *target_gains);

static void wlc_phy_txcal_txgain_setup_acphy(phy_info_t *pi, txgain_setting_t *txcal_txgain,
                                             txgain_setting_t *orig_txgain);
static void wlc_phy_txcal_txgain_cleanup_acphy(phy_info_t *pi, txgain_setting_t *orig_txgain);
static void wlc_phy_txcal_radio_setup_acphy(phy_info_t *pi);
static void wlc_phy_txcal_radio_cleanup_acphy(phy_info_t *pi);
static void wlc_phy_txcal_phy_setup_acphy(phy_info_t *pi);
static void wlc_phy_txcal_phy_cleanup_acphy(phy_info_t *pi);
static void wlc_phy_cal_txiqlo_update_ladder_acphy(phy_info_t *pi, uint16 bbmult);
static void wlc_phy_clip_det_acphy(phy_info_t *pi, bool enable);

void
wlc_phy_table_write_acphy(phy_info_t *pi, uint32 id, uint32 len, uint32 offset, uint32 width,
                          const void *data);
void
wlc_phy_table_read_acphy(phy_info_t *pi, uint32 id, uint32 len, uint32 offset, uint32 width,
                         void *data);
void wlc_2069_rfpll_150khz(phy_info_t *pi);
void wlc_phy_force_rfseq_acphy(phy_info_t *pi, uint8 cmd);
void wlc_phy_set_analog_tx_lpf(phy_info_t *pi, uint16 mode_mask, int bq0_bw, int bq1_bw,
                               int rc_bw, int gmult, int gmult_rc, int core_num);
void wlc_phy_set_analog_rx_lpf(phy_info_t *pi, uint8 mode_mask, int bq0_bw, int bq1_bw,
                               int rc_bw, int gmult, int gmult_rc, int core_num);
void wlc_phy_set_tx_afe_dacbuf_cap(phy_info_t *pi, uint16 mode_mask, int dacbuf_cap,
                                   int dacbuf_fixed_cap, int core_num);
void wlc_phy_rx_iq_est_acphy(phy_info_t *pi, phy_iq_est_t *est, uint16 num_samps,
                             uint8 wait_time, uint8 wait_for_crs);

static void wlc_phy_crs_min_pwr_cal_acphy(phy_info_t *pi);
static void wlc_phy_rx_iq_comp_acphy(phy_info_t *pi, uint8 write, phy_iq_comp_t *pcomp, uint8);
static void wlc_phy_rxcal_phy_setup_acphy(phy_info_t *pi);
static void wlc_phy_rxcal_phy_cleanup_acphy(phy_info_t *pi);
static void wlc_phy_rxcal_radio_setup_acphy(phy_info_t *pi);
static void wlc_phy_rxcal_radio_cleanup_acphy(phy_info_t *pi);
static void wlc_phy_rxcal_loopback_gainctrl_acphy(phy_info_t *pi);

static int  wlc_phy_cal_rx_fdiqi_acphy(phy_info_t *pi);
static void wlc_phy_rx_fdiqi_lin_reg_acphy(phy_info_t *pi, acphy_rx_fdiqi_t *freq_ang_mag,
                                        uint16 num_data);
static void wlc_phy_rx_fdiqi_comp_acphy(phy_info_t *pi, bool enable);
static void wlc_phy_rxcal_leakage_comp_acphy(phy_info_t *pi, phy_iq_est_t loopback_rx_iq,
                                   phy_iq_est_t leakage_rx_iq, int32 *angle, int32 *mag);

void wlc_phy_init_test_acphy(phy_info_t *pi);

static void wlc_phy_rx_fdiqi_freq_config(phy_info_t *pi, int8 *fdiqi_cal_freqs, uint16 *num_data);

#if defined(BCMDBG)
void wlc_phy_force_gainlevel_acphy(phy_info_t *pi, int16 int_val);
#endif
#if defined(BCMDBG)
void wlc_phy_force_fdiqi_acphy(phy_info_t *pi, uint16 int_val);
#endif

static void wlc_phy_txpwrctrl_set_target_acphy(phy_info_t *pi, uint8 pwr_qtrdbm, uint8 core);
static uint8 wlc_phy_txpwrctrl_get_cur_index_acphy(phy_info_t *pi, uint8 core);
static void wlc_phy_txpwrctrl_set_cur_index_acphy(phy_info_t *pi, uint8 idx, uint8 core);
static bool wlc_phy_txpwrctrl_ison_acphy(phy_info_t *pi);
uint32 wlc_phy_txpwr_idx_get_acphy(phy_info_t *pi);
void wlc_phy_txpwrctrl_enable_acphy(phy_info_t *pi, uint8 ctrl_type);
static void wlc_phy_txpwrctrl_set_idle_tssi_acphy(phy_info_t *pi, int16 idle_tssi, uint8 core);
static void wlc_phy_tssi_radio_setup_acphy(phy_info_t *pi, uint8 core_mask);
static void wlc_phy_tssi_phy_setup_acphy(phy_info_t *pi);
static void wlc_phy_gpiosel_acphy(phy_info_t *pi, uint16 sel, uint8 word_swap);
static void wlc_phy_poll_adc_acphy(phy_info_t *pi, int32 *adc_buf, uint8 nsamps);
static void wlc_phy_poll_samps_acphy(phy_info_t *pi, int16 *samp, bool is_tssi);
static void wlc_phy_poll_samps_WAR_acphy(phy_info_t *pi, int16 *samp, bool is_tssi,
bool for_idle, txgain_setting_t *target_gains);
#ifndef DSLCPE_C590068
static
#endif
void wlc_phy_txpwrctrl_idle_tssi_meas_acphy(phy_info_t *pi);
static uint8 wlc_phy_set_txpwr_clamp_acphy(phy_info_t *pi);
extern void wlc_phy_get_paparams_for_band_acphy(phy_info_t *pi, int16 *a1, int16 *b0, int16 *b1);
static void wlc_phy_txpwrctrl_config_acphy(phy_info_t *pi);
static void wlc_phy_txpwrctrl_pwr_setup_acphy(phy_info_t *pi);
static void wlc_phy_txpwr_fixpower_acphy(phy_info_t *pi);
static void wlc_phy_pulse_adc_reset_acphy(phy_info_t *pi);
static uint8 wlc_phy_tssi2dbm_acphy(phy_info_t *pi, int32 tssi, int32 a1, int32 b0, int32 b1);
#if defined(BCMDBG_RXCAL)
static void wlc_phy_rxcal_snr_acphy(phy_info_t *pi, uint16 num_samps, uint8 core_mask);
#endif /* BCMDBG_RXCAL */
static uint8 wlc_poll_adc_clamp_status(phy_info_t *pi, uint8 core, uint8 do_reset);
static bool wlc_phy_srom_read_acphy(phy_info_t *pi);
void wlc_phy_txpwr_est_pwr_acphy(phy_info_t *pi, uint8 *Pout, uint8 *Pout_act);
int16 wlc_phy_tempsense_acphy(phy_info_t *pi);
static void wlc_phy_tempsense_radio_setup_acphy(phy_info_t *pi, uint16 Av, uint16 Vmid);
static void wlc_phy_tempsense_phy_setup_acphy(phy_info_t *pi);
static void wlc_phy_tempsense_radio_cleanup_acphy(phy_info_t *pi);
static void wlc_phy_tempsense_phy_cleanup_acphy(phy_info_t *pi);
static void wlc_phy_tx_gm_gain_boost(phy_info_t *pi);
static void acphy_load_txv_for_spexp(phy_info_t *pi);
static int wlc_phy_txpower_core_offset_set_acphy(phy_info_t *pi,
                                                 struct phy_txcore_pwr_offsets *offsets);
static int wlc_phy_txpower_core_offset_get_acphy(phy_info_t *pi,
                                                 struct phy_txcore_pwr_offsets *offsets);
static void wlc_phy_set_crs_min_pwr_acphy(phy_info_t *pi, uint8 ac_th,
                                          int8 offset_1, int8 offset_2);

static void wlc_phy_set_analog_rxgain(phy_info_t *pi, uint8 clipgain, uint8 *gain_idx,
                                      bool trtx, uint8 core);
static void wlc_phy_srom_read_gainctrl_acphy(phy_info_t *pi);

/* Rx Gainctrl */
static void wlc_phy_rxgainctrl_gainctrl_acphy(phy_info_t *pi);
static int8 wlc_phy_rxgainctrl_calc_low_sens_acphy(phy_info_t *pi, uint8 clipgain, bool trtx,
                                                   uint8 core);
static int8 wlc_phy_rxgainctrl_calc_high_sens_acphy(phy_info_t *pi, uint8 clipgain, bool trtx,
                                                    uint8 core);
static uint8 wlc_phy_rxgainctrl_set_init_clip_gain_acphy(phy_info_t *pi, uint8 clipgain,
                                                         uint8 gain_dB, bool trtx, uint8 core);

static void wlc_phy_rxgainctrl_set_gaintbls_acphy(phy_info_t *pi, bool init,
                                                         bool band_change, bool bw_change);
static uint8 wlc_phy_rxgainctrl_encode_gain_acphy(phy_info_t *pi, uint8 core,
                                                  uint8 gain_dB, bool trloss, uint8 *gidx);
static void wlc_phy_rxgainctrl_nbclip_acphy(phy_info_t *pi, uint8 core, int rxpwr_dBm);
static void wlc_phy_rxgainctrl_w1clip_acphy(phy_info_t *pi, uint8 core, int rxpwr_dBm);

static uint8 wlc_phy_get_max_lna_index_acphy(phy_info_t *pi, uint8 lna);
static void  wlc_phy_upd_lna1_lna2_gaintbls_acphy(phy_info_t *pi, uint8 lna12);
static void wlc_phy_upd_lna1_lna2_gainlimittbls_acphy(phy_info_t *pi, uint8 lna12);
static void wlc_phy_limit_rxgaintbl_acphy(uint8 gaintbl[], uint8 gainbitstbl[], uint8 sz,
                                          uint8 default_gaintbl[], uint8 min_idx, uint8 max_idx);

static void wlc_phy_bt_on_gpio4_acphy(phy_info_t *pi);
static void wlc_phy_compute_rssi_gainerror_acphy(phy_info_t *pi);
static void wlc_phy_get_initgain_dB_acphy(phy_info_t *pi, int16 *initgain_dB);
static int16 wlc_phy_rxgaincode_to_dB_acphy(phy_info_t *pi, uint16 gain_code, uint8 core);
static uint32 wlc_phy_pdoffset_cal_acphy(uint32 pdoffs, uint16 pdoffset, uint8 band);
static uint32 wlc_phy_pdoffset_cal_2g_acphy(uint32 pdoffs, uint8 pdoffset);
static void wlc_phy_set_aci_regs_acphy(phy_info_t *pi);

uint8 wlc_phy_calc_extra_init_gain_acphy(phy_info_t *pi, uint8 extra_gain_3dB, rxgain_t rxgain[]);

/* ACI, BT Desense (start) */
static void wlc_phy_desense_calc_total_acphy(phy_info_t *pi);
static void wlc_phy_desense_apply_acphy(phy_info_t *pi, bool apply_desense);
static void wlc_phy_desense_mf_high_thresh_acphy(phy_info_t *pi, bool on);
static acphy_aci_params_t* wlc_phy_desense_aci_getset_chanidx_acphy(phy_info_t *pi,
                                                                  chanspec_t chanspec, bool create);
static uint8 wlc_phy_desense_aci_calc_acphy(phy_info_t *pi, desense_history_t *aci_desense,
                                       uint8 desense, uint16 glitch_cnt, uint16 glitch_th_lo,
                                       uint16 glitch_th_hi);
static uint16 wlc_phy_desense_aci_get_avg_max_glitches_acphy(uint16 glitches[]);
static void wlc_phy_desense_print_phyregs_acphy(phy_info_t *pi, const char str[]);
/* ACI, BT Desense (end) */

static bool wlc_phy_is_scan_chan_acphy(phy_info_t *pi);

#ifdef ENABLE_FCBS
static bool wlc_phy_fcbsinit_acphy(phy_info_t *pi, int chanidx, chanspec_t chanspec);
static bool wlc_phy_prefcbsinit_acphy(phy_info_t *pi, int chanidx);
static bool wlc_phy_postfcbsinit_acphy(phy_info_t *pi, int chanidx);
static bool wlc_phy_fcbs_acphy(phy_info_t *pi, int chanidx);
static bool wlc_phy_prefcbs_acphy(phy_info_t *pi, int chanidx);
static bool wlc_phy_postfcbs_acphy(phy_info_t *pi, int chanidx);
#endif /* ENABLE_FCBS */

void wlc_phy_lpf_hpc_override_acphy(phy_info_t *pi, bool setup_not_cleanup);
static void wlc_phy_set_bt_on_core1_acphy(phy_info_t *pi, uint8 bt_fem_val);
void wlc_phy_proprietary_mcs_acphy(phy_info_t *pi, bool enable_prop_mcs);
uint8 wlc_phy_11b_rssi_WAR(phy_info_t *pi, d11rxhdr_t *rxh);
void wlc_phy_populate_tx_loft_comp_tbl_acphy(phy_info_t *pi, uint16 *loft_coeffs);
uint16 wlc_phy_set_txpwr_by_index_acphy(phy_info_t *pi, uint8 core_mask, int8 txpwrindex);
void wlc_phy_read_txgain_acphy(phy_info_t *pi);

#ifdef WL_LPC
uint8 wlc_acphy_lpc_getminidx(void);
uint8 wlc_acphy_lpc_getoffset(uint8 index);
#ifdef WL_LPC_DEBUG
uint8 * wlc_acphy_lpc_get_pwrlevelptr(void);
#endif
#endif /* WL_LPC */

bool
BCMATTACHFN(wlc_phy_attach_acphy)(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac;
	uint16 core;
	uint8 i;
	uint8 gain_len[] = {2, 6, 7, 10, 8, 8};  /* elna, lna1, lna2, mix, bq0, bq1 */

	phyhal_msg_level |= PHYHAL_ERROR;

	pi->u.pi_acphy = (phy_info_acphy_t*)MALLOC(pi->sh->osh, sizeof(phy_info_acphy_t));
	if (pi->u.pi_acphy == NULL) {
		PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return FALSE;
	}
	bzero((char *)pi->u.pi_acphy, sizeof(phy_info_acphy_t));

	/* Default Values */
	pi_ac = pi->u.pi_acphy;
	pi_ac->init = FALSE;
#if defined(BCMDBG)
	pi_ac->fdiqi.forced = FALSE;
	pi_ac->fdiqi.forced_val = 0;
#endif
	pi_ac->curr_band2g = CHSPEC_IS2G(pi->radio_chanspec);
	pi_ac->curr_bw = CHSPEC_BW(pi->radio_chanspec);
	pi_ac->dac_mode = 1;
	pi_ac->rccal_gmult = 128;
	pi_ac->rccal_gmult_rc = 128;
	pi_ac->rccal_dacbuf = 12;
	pi->u.pi_acphy->txcal_cache_cookie = 0;
	pi->phy_cal_mode = PHY_PERICAL_MPHASE;
	pi_ac->poll_adc_WAR = FALSE;
	pi->phy_scraminit = AUTO;
	pi_ac->crsmincal_enable = TRUE;
	pi_ac->crsmincal_run = 0;
	pi_ac->srom.elna2g_present = FALSE;
	pi_ac->srom.elna5g_present = FALSE;

	pi_ac->acphy_lp_mode = 1;
	pi_ac->acphy_prev_lp_mode = pi_ac->acphy_lp_mode;
	pi_ac->acphy_lp_status = pi_ac->acphy_lp_mode;
	pi_ac->acphy_4335_radio_pd_status = 0;
	/* AFE */
	pi->u.pi_acphy->afeRfctrlCoreAfeCfg10 = phy_reg_read(pi, ACPHY_RfctrlCoreAfeCfg10);
	pi->u.pi_acphy->afeRfctrlCoreAfeCfg20 = phy_reg_read(pi, ACPHY_RfctrlCoreAfeCfg20);
	pi->u.pi_acphy->afeRfctrlOverrideAfeCfg0 = phy_reg_read(pi, ACPHY_RfctrlOverrideAfeCfg0);
	/* Radio RX */
	pi->u.pi_acphy->rxRfctrlCoreRxPus0 = phy_reg_read(pi, ACPHY_RfctrlCoreRxPus0);
	pi->u.pi_acphy->rxRfctrlOverrideRxPus0 = phy_reg_read(pi, ACPHY_RfctrlOverrideRxPus0);
	/* Radio TX */
	pi->u.pi_acphy->txRfctrlCoreTxPus0 = phy_reg_read(pi, ACPHY_RfctrlCoreTxPus0);
	pi->u.pi_acphy->txRfctrlOverrideTxPus0 = phy_reg_read(pi, ACPHY_RfctrlOverrideTxPus0);
	/* {radio, rfpll, pllldo}_pu = 0 */
	pi->u.pi_acphy->radioRfctrlCmd = phy_reg_read(pi, ACPHY_RfctrlCmd);
	pi->u.pi_acphy->radioRfctrlCoreGlobalPus = phy_reg_read(pi, ACPHY_RfctrlCoreGlobalPus);
	pi->u.pi_acphy->radioRfctrlOverrideGlobalPus =
		phy_reg_read(pi, ACPHY_RfctrlOverrideGlobalPus);
	/* read chipid */
	/* Used to select correct radio offsets based on chipid */
	acphychipid = (CHIPID(pi->sh->chip));

	/* pre_init to ON, register POR default setting */
	pi_ac->ac_rxldpc_override = ON;

	pi->n_preamble_override = WLC_N_PREAMBLE_MIXEDMODE;

	/* Get xtal frequency from PMU */
#if !defined(XTAL_FREQ)
	pi->xtalfreq = si_alp_clock(pi->sh->sih);
#endif
	ASSERT((PHY_XTALFREQ(pi->xtalfreq) % 1000) == 0);

	PHY_INFORM(("wl%d: %s: using %d.%d MHz xtalfreq for RF PLL\n",
		pi->sh->unit, __FUNCTION__,
		PHY_XTALFREQ(pi->xtalfreq) / 1000000, PHY_XTALFREQ(pi->xtalfreq) % 1000000));

	for (core = 0; core < PHY_CORE_MAX; core++) {
		pi_ac->txpwrindex_hw_save[core] = 128;
	}

	for (i = 0; i < ACPHY_MAX_RX_GAIN_STAGES; i++)
		pi_ac->rxgainctrl_stage_len[i] = gain_len[i];

	if ((PHY_GETVAR(pi, "subband5gver")) != NULL) {
		pi->sh->subband5Gver = (uint8)PHY_GETINTVAR(pi, "subband5gver");
	} else {
		pi->sh->subband5Gver = PHY_SUBBAND_4BAND;
	}

	if ((PHY_GETVAR(pi, "extpagain2g")) != NULL) {
		pi->sh->extpagain2g = (uint8)PHY_GETINTVAR(pi, "extpagain2g");
	} else {
		pi->sh->extpagain2g = 0;
	}

	if ((PHY_GETVAR(pi, "extpagain5g")) != NULL) {
		pi->sh->extpagain5g = (uint8)PHY_GETINTVAR(pi, "extpagain5g");
	} else {
		pi->sh->extpagain5g = 0;
	}

	if ((PHY_GETVAR(pi, "femctrl")) != NULL) {
		pi_ac->srom.femctrl = (uint8)PHY_GETINTVAR(pi, "femctrl");
	} else {
		pi_ac->srom.femctrl = 0;
	}

	if ((PHY_GETVAR(pi, "boardflags3")) != NULL) {
		pi_ac->srom.femctrl_sub = PHY_GETINTVAR(pi, "boardflags3") &
			BFL3_FEMCTRL_SUB;
	} else {
		pi_ac->srom.femctrl_sub = 0;
	}
	if ((PHY_GETVAR(pi, "boardflags3")) != NULL) {
		pi_ac->srom.rcal_war = PHY_GETINTVAR(pi, "boardflags3") & BFL3_RCAL_WAR;
	} else {
		pi_ac->srom.rcal_war = 0;
	}
	if ((PHY_GETVAR(pi, "boardflags3")) != NULL) {
		pi_ac->srom.txgaintbl_id = (PHY_GETINTVAR(pi, "boardflags3") & BFL3_TXGAINTBLID)
		        >> BFL3_TXGAINTBLID_SHIFT;
	} else {
		pi_ac->srom.txgaintbl_id = 0;
	}
	pi_ac->srom.rfpll_5g = (BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2) &
		BFL2_SROM11_APLL_WAR) != 0;

	pi_ac->srom.bt_coex = (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
		BFL_SROM11_BTCOEX) != 0;

	pi_ac->srom.gainboosta01 = (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
	                            BFL_SROM11_GAINBOOSTA01) != 0;

	if ((PHY_GETVAR(pi, "pdgain2g")) != NULL) {
		pi_ac->srom_2g_pdrange_id = (uint8)PHY_GETINTVAR(pi, "pdgain2g");
	} else {
		pi_ac->srom_2g_pdrange_id = 0;
	}

	if ((PHY_GETVAR(pi, "pdgain5g")) != NULL) {
		pi_ac->srom_5g_pdrange_id = (uint8)PHY_GETINTVAR(pi, "pdgain5g");
	} else {
		pi_ac->srom_5g_pdrange_id = 0;
	}

	if ((PHY_GETVAR(pi, "cckdigfilttype")) != NULL) {
		pi_ac->acphy_cck_dig_filt_type = (uint8)PHY_GETINTVAR(pi, "cckdigfilttype");
	} else {
		/* bit0 is gaussian shaping and bit1 & 2 are for RRC alpha */
		pi_ac->acphy_cck_dig_filt_type = 0x01;
	}
	/* Find out the number of cores contained in this ACPHY */
	pi->pubpi.phy_corenum = READ_PHYREG(pi, ACPHY_PhyCapability0, NumberOfStreams);

	wlc_phy_txpwrctrl_config_acphy(pi);

	if (!wlc_phy_srom_read_acphy(pi))
		return FALSE;

	/* Read RFLDO from OTP */
	wlc_phy_rfldo_trim_value(pi);

	pi->pi_fptr.init = wlc_phy_init_acphy;
	pi->pi_fptr.calinit = wlc_phy_cal_init_acphy;
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_acphy;
	pi->pi_fptr.txpwrrecalc = wlc_phy_txpower_recalc_target_acphy;
	pi->pi_fptr.phywatchdog = wlc_phy_watchdog_acphy;
	pi->pi_fptr.detach = wlc_phy_detach_acphy;
	pi->pi_fptr.txcorepwroffsetset = wlc_phy_txpower_core_offset_set_acphy;
	pi->pi_fptr.txcorepwroffsetget = wlc_phy_txpower_core_offset_get_acphy;

	/* desense (aci, cci, bt) */
	bzero(&pi_ac->curr_desense, sizeof(acphy_desense_values_t));
	bzero(&pi_ac->zero_desense, sizeof(acphy_desense_values_t));
	bzero(&pi_ac->total_desense, sizeof(acphy_desense_values_t));
	bzero(&pi_ac->bt_desense, sizeof(acphy_desense_values_t));
	pi_ac->limit_desense_on_rssi = TRUE;
	bzero(pi_ac->aci_list, ACPHY_ACI_CHAN_LIST_SZ * sizeof(acphy_aci_params_t));
	pi_ac->aci = NULL;
	pi_ac->btc_mode = 0;

#ifdef ENABLE_FCBS
	pi->pi_fptr.fcbsinit = wlc_phy_fcbsinit_acphy;
	pi->pi_fptr.prefcbsinit = wlc_phy_prefcbsinit_acphy;
	pi->pi_fptr.postfcbsinit = wlc_phy_postfcbsinit_acphy;
	pi->pi_fptr.fcbs = wlc_phy_fcbs_acphy;
	pi->pi_fptr.prefcbs = wlc_phy_prefcbs_acphy;
	pi->pi_fptr.postfcbs = wlc_phy_postfcbs_acphy;
	pi->phy_fcbs.HW_FCBS = TRUE;
	if ((pi->pubpi.phy_rev == 0) || (pi->pubpi.phy_rev == 1)) {
		pi->phy_fcbs.FCBS_ucode = TRUE;
	} else {
		pi->phy_fcbs.FCBS_ucode = FALSE;
	}

#endif /* ENABLE_FCBS */

#ifdef WL_LPC
	pi->pi_fptr.lpcgetminidx = wlc_acphy_lpc_getminidx;
	pi->pi_fptr.lpcgetpwros = wlc_acphy_lpc_getoffset;
	pi->pi_fptr.lpcgettxcpwrval = NULL;
	pi->pi_fptr.lpcsettxcpwrval = NULL;
	pi->pi_fptr.lpcsetmode = NULL;
#ifdef WL_LPC_DEBUG
	pi->pi_fptr.lpcgetpwrlevelptr = wlc_acphy_lpc_get_pwrlevelptr;
#endif
#endif /* WL_LPC */
	return TRUE;
}


/*
************************ RADIO procs **************************
*/

void
wlc_phy_radio_override_acphy(phy_info_t *pi, bool on)
{
	uint16 addr, *tbl_ptr;
	uint16 ovr_val = on ? 0xFFFF : 0x0;

	if (ACRADIO1X1_IS(pi->pubpi.radiorev)) {
		tbl_ptr = ovr_regs_2069_rev16;
	} else {
		tbl_ptr = ovr_regs_2069_rev2;
	}

	while ((addr = *tbl_ptr++) != 0xFFFF) {
		ASSERT(addr != 0);
		write_radio_reg(pi, addr, ovr_val);
	}

	MOD_RADIO_REG(pi, RF2, OVR2, ovr_otp_rcal_sel, 0);
	/* Reg conflict with 2069 rev 16 */
	if (ACREV0) {
		MOD_RADIO_REG(pi, RFP,  OVR15, rfpll_vco_EN_DEGEN_jtag_ovr, 0);
	} else {
		MOD_RADIO_REG(pi, RFP,  GE16_OVR16, rfpll_vco_EN_DEGEN_jtag_ovr, 0);

	}
}

void
wlc_phy_switch_radio_acphy(phy_info_t *pi, bool on)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	PHY_TRACE(("wl%d: %s %s\n", pi->sh->unit, __FUNCTION__, on ? "ON" : "OFF"));

	if (on) {
		if (!pi->radio_is_on) {
			if (ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
				/*  set the low pwoer reg before radio init */
				wlc_phy_set_lowpwr_phy_reg(pi);
				wlc_phy_radio2069_mini_pwron_seq_rev16(pi);
			}
			wlc_phy_radio2069_pwron_seq(pi);
			if (pi->u.pi_acphy->srom.rcal_war) {

		/* --------------------------RCAL WAR ---------------------------- */
		/* Currently RCAL resistor is not connected on the board. The pin  */
		/* labelled TSSI_G/GPIO goes into the TSSI pin of the FEM through  */
		/* a 0 Ohm resistor. There is an option to add a shunt 10k to GND  */
		/* on this trace but it is depop. Adding shunt resistance on the   */
		/* TSSI line may affect the voltage from the FEM to our TSSI input */
		/* So, this issue is worked around by forcing below registers      */
		/* THIS IS APPLICABLE FOR BOARDTYPE = $def(boardtype)			   */
		/* --------------------------RCAL WAR ---------------------------- */

				MOD_RADIO_REG(pi, RFP,  GE16_BG_CFG1, rcal_trim, 0xa);
				MOD_RADIO_REG(pi, RFP,  GE16_OVR2, ovr_bg_rcal_trim, 1);
				MOD_RADIO_REG(pi, RFP,  GE16_OVR2, ovr_otp_rcal_sel, 0);

			} else {
				wlc_phy_radio2069_rcal(pi);
			}
			if (pi_ac->init_done) {
			  wlc_phy_set_regtbl_on_pwron_acphy(pi);
			  wlc_phy_chanspec_set_acphy(pi, pi->radio_chanspec);
			}
			wlc_phy_radio2069_rccal(pi);
			pi->radio_is_on = TRUE;
		}
	} else {
		/* wlc_phy_radio2069_off(); */
		pi->radio_is_on = FALSE;
		/* FEM */
		ACPHYREG_BCAST(pi, ACPHY_RfctrlIntc0, 0);

		/* AFE */
		ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreAfeCfg10, 0);
		ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreAfeCfg20, 0);
		ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideAfeCfg0, 0x1fff);

		/* Radio RX */
		ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreRxPus0, 0);
		ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideRxPus0, 0xffff);

		/* Radio TX */
		ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreTxPus0, 0);
		ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideTxPus0, 0x3ff);

		/* {radio, rfpll, pllldo}_pu = 0 */
		MOD_PHYREG(pi, ACPHY_RfctrlCmd, chip_pu, 0);
		phy_reg_write(pi, ACPHY_RfctrlCoreGlobalPus, 0);
		phy_reg_write(pi, ACPHY_RfctrlOverrideGlobalPus, 0x1);
	}

	if ((
		CHIPID(pi->sh->chip) == BCM4335_CHIP_ID) &&
		(!CST4335_CHIPMODE_USB20D(pi->sh->sih->chipst))) {
		/* Power down HSIC */
		if (ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
				MOD_RADIO_REG(pi, RFP,  GE16_PLL_XTAL2, xtal_pu_HSIC, 0x0);
		}
	}
}
static void
wlc_phy_radio2069_mini_pwron_seq_rev16(phy_info_t *pi)
{

	uint8 cntr = 0;
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x80, 0x80);
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x10, 0x10);
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x8, 0x8);
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x4, 0x4);
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x2, 0x2);

	OSL_DELAY(100);

	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x20, 0x20);
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x2000, 0x2000);
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x4000, 0x4000);

	while (!(read_radio_reg(pi, RFP_2069_PMU_STAT) & 1)) {
		OSL_DELAY(100);
		cntr++;
		if (cntr > 100) {
			printf("PMU cal Fail \n");
			break;
		}
	}
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x4000, 0);
	mod_radio_reg(pi, RFP_2069_PMU_OP, 0x2000, 0);
}
void
wlc_phy_radio2069_pwrdwn_seq(phy_info_t *pi)
{

	/* AFE */
	pi->u.pi_acphy->afeRfctrlCoreAfeCfg10 = phy_reg_read(pi, ACPHY_RfctrlCoreAfeCfg10);
	pi->u.pi_acphy->afeRfctrlCoreAfeCfg20 = phy_reg_read(pi, ACPHY_RfctrlCoreAfeCfg20);
	pi->u.pi_acphy->afeRfctrlOverrideAfeCfg0 = phy_reg_read(pi, ACPHY_RfctrlOverrideAfeCfg0);
	phy_reg_write(pi, ACPHY_RfctrlCoreAfeCfg10, 0);
	phy_reg_write(pi, ACPHY_RfctrlCoreAfeCfg20, 0);
	phy_reg_write(pi, ACPHY_RfctrlOverrideAfeCfg0, 0x1fff);

	/* Radio RX */
	pi->u.pi_acphy->rxRfctrlCoreRxPus0 = phy_reg_read(pi, ACPHY_RfctrlCoreRxPus0);
	pi->u.pi_acphy->rxRfctrlOverrideRxPus0 = phy_reg_read(pi, ACPHY_RfctrlOverrideRxPus0);
	phy_reg_write(pi, ACPHY_RfctrlCoreRxPus0, 0x40);
	phy_reg_write(pi, ACPHY_RfctrlOverrideRxPus0, 0xffbf);

	/* Radio TX */
	pi->u.pi_acphy->txRfctrlCoreTxPus0 = phy_reg_read(pi, ACPHY_RfctrlCoreTxPus0);
	pi->u.pi_acphy->txRfctrlOverrideTxPus0 = phy_reg_read(pi, ACPHY_RfctrlOverrideTxPus0);
	phy_reg_write(pi, ACPHY_RfctrlCoreTxPus0, 0);
	phy_reg_write(pi, ACPHY_RfctrlOverrideTxPus0, 0x3ff);

	/* {radio, rfpll, pllldo}_pu = 0 */
	pi->u.pi_acphy->radioRfctrlCmd = phy_reg_read(pi, ACPHY_RfctrlCmd);
	pi->u.pi_acphy->radioRfctrlCoreGlobalPus = phy_reg_read(pi, ACPHY_RfctrlCoreGlobalPus);
	pi->u.pi_acphy->radioRfctrlOverrideGlobalPus =
	        phy_reg_read(pi, ACPHY_RfctrlOverrideGlobalPus);
	MOD_PHYREG(pi, ACPHY_RfctrlCmd, chip_pu, 0);
	phy_reg_write(pi, ACPHY_RfctrlCoreGlobalPus, 0);
	phy_reg_write(pi, ACPHY_RfctrlOverrideGlobalPus, 0x1);
}
void
wlc_phy_radio2069_pwrup_seq(phy_info_t *pi)
{

	/* AFE */
	phy_reg_write(pi, ACPHY_RfctrlCoreAfeCfg10, pi->u.pi_acphy->afeRfctrlCoreAfeCfg10);
	phy_reg_write(pi, ACPHY_RfctrlCoreAfeCfg20, pi->u.pi_acphy->afeRfctrlCoreAfeCfg20);
	phy_reg_write(pi, ACPHY_RfctrlOverrideAfeCfg0, pi->u.pi_acphy->afeRfctrlOverrideAfeCfg0);

	/* Restore Radio RX */
	phy_reg_write(pi, ACPHY_RfctrlCoreRxPus0, pi->u.pi_acphy->rxRfctrlCoreRxPus0);
	phy_reg_write(pi, ACPHY_RfctrlOverrideRxPus0, pi->u.pi_acphy->rxRfctrlOverrideRxPus0);

	/* Radio TX */
	phy_reg_write(pi, ACPHY_RfctrlCoreTxPus0, pi->u.pi_acphy->txRfctrlCoreTxPus0);
	phy_reg_write(pi, ACPHY_RfctrlOverrideTxPus0, pi->u.pi_acphy->txRfctrlOverrideTxPus0);

	/* {radio, rfpll, pllldo}_pu = 0 */
	phy_reg_write(pi, ACPHY_RfctrlCmd, pi->u.pi_acphy->radioRfctrlCmd);
	phy_reg_write(pi, ACPHY_RfctrlCoreGlobalPus, pi->u.pi_acphy->radioRfctrlCoreGlobalPus);
	phy_reg_write(pi, ACPHY_RfctrlOverrideGlobalPus,
	              pi->u.pi_acphy->radioRfctrlOverrideGlobalPus);
}

static void
wlc_phy_set_lowpwr_phy_reg(phy_info_t *pi)
{
	phy_reg_mod(pi, ACPHY_radio_logen2g, ACPHY_radio_logen2g_idac_gm_MASK,
	                0x3<<ACPHY_radio_logen2g_idac_gm_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen2g, ACPHY_radio_logen2g_idac_gm_2nd_MASK,
	                0x3<<ACPHY_radio_logen2g_idac_gm_2nd_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen2g, ACPHY_radio_logen2g_idac_qb_MASK,
	                0x3<<ACPHY_radio_logen2g_idac_qb_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen2g, ACPHY_radio_logen2g_idac_qb_2nd_MASK,
	                0x3<<ACPHY_radio_logen2g_idac_qb_2nd_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen2g, ACPHY_radio_logen2g_idac_qtx_MASK,
	                0x4<<ACPHY_radio_logen2g_idac_qtx_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen2gN5g, ACPHY_radio_logen2gN5g_idac_itx_MASK,
	                0x4<<ACPHY_radio_logen2gN5g_idac_itx_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen2gN5g, ACPHY_radio_logen2gN5g_idac_qrx_MASK,
	                0x4<<ACPHY_radio_logen2gN5g_idac_qrx_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen2gN5g, ACPHY_radio_logen2gN5g_idac_irx_MASK,
	                0x4<<ACPHY_radio_logen2gN5g_idac_irx_SHIFT);
	phy_reg_mod(pi,	ACPHY_radio_logen2gN5g,	ACPHY_radio_logen2gN5g_idac_buf_MASK,
	                0x3<<ACPHY_radio_logen2gN5g_idac_buf_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen2gN5g, ACPHY_radio_logen2gN5g_idac_mix_MASK,
	                0x3<<ACPHY_radio_logen2gN5g_idac_mix_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5g, ACPHY_radio_logen5g_idac_div_MASK,
	                0x3<<ACPHY_radio_logen5g_idac_div_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5g, ACPHY_radio_logen5g_idac_vcob_MASK,
	                0x3<<ACPHY_radio_logen5g_idac_vcob_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gbufs, ACPHY_radio_logen5gbufs_idac_bufb_MASK,
	                0x3<<ACPHY_radio_logen5gbufs_idac_bufb_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5g, ACPHY_radio_logen5g_idac_mixb_MASK,
	                0x3<<ACPHY_radio_logen5g_idac_mixb_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5g, ACPHY_radio_logen5g_idac_load_MASK,
	                0x3<<ACPHY_radio_logen5g_idac_load_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gbufs, ACPHY_radio_logen5gbufs_idac_buf2_MASK,
	                0x3<<ACPHY_radio_logen5gbufs_idac_buf2_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gbufs, ACPHY_radio_logen5gbufs_idac_bufb2_MASK,
	                0x3<<ACPHY_radio_logen5gbufs_idac_bufb2_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gbufs, ACPHY_radio_logen5gbufs_idac_buf1_MASK,
	                0x3<<ACPHY_radio_logen5gbufs_idac_buf1_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gbufs, ACPHY_radio_logen5gbufs_idac_bufb1_MASK,
	                0x3<<ACPHY_radio_logen5gbufs_idac_bufb1_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gQI, ACPHY_radio_logen5gQI_idac_qtx_MASK,
	                0x4<<ACPHY_radio_logen5gQI_idac_qtx_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gQI, ACPHY_radio_logen5gQI_idac_itx_MASK,
	                0x4<<ACPHY_radio_logen5gQI_idac_itx_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gQI, ACPHY_radio_logen5gQI_idac_qrx_MASK,
	                0x4<<ACPHY_radio_logen5gQI_idac_qrx_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_logen5gQI, ACPHY_radio_logen5gQI_idac_irx_MASK,
	                0x4<<ACPHY_radio_logen5gQI_idac_irx_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcocal, ACPHY_radio_pll_vcocal_vcocal_rstn_MASK,
	                0x1<<ACPHY_radio_pll_vcocal_vcocal_rstn_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcocal, ACPHY_radio_pll_vcocal_vcocal_force_caps_MASK,
	                0x0<<ACPHY_radio_pll_vcocal_vcocal_force_caps_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcocal, ACPHY_radio_pll_vcocal_vcocal_force_caps_val_MASK,
	                0x40<<ACPHY_radio_pll_vcocal_vcocal_force_caps_val_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet1, ACPHY_radio_pll_vcoSet1_vco_ALC_ref_ctrl_MASK,
	                0xd<<ACPHY_radio_pll_vcoSet1_vco_ALC_ref_ctrl_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet1, ACPHY_radio_pll_vcoSet1_vco_bias_mode_MASK,
	                0x1<<ACPHY_radio_pll_vcoSet1_vco_bias_mode_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet1, ACPHY_radio_pll_vcoSet1_vco_cvar_extra_MASK,
	                0xb<<ACPHY_radio_pll_vcoSet1_vco_cvar_extra_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet1, ACPHY_radio_pll_vcoSet1_vco_cvar_MASK,
	                0xf<<ACPHY_radio_pll_vcoSet1_vco_cvar_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet1, ACPHY_radio_pll_vcoSet1_vco_en_alc_MASK,
	                0x0<<ACPHY_radio_pll_vcoSet1_vco_en_alc_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet2, ACPHY_radio_pll_vcoSet2_vco_tempco_dcadj_MASK,
	                0xe<<ACPHY_radio_pll_vcoSet2_vco_tempco_dcadj_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet2, ACPHY_radio_pll_vcoSet2_vco_tempco_MASK,
	                0xb<<ACPHY_radio_pll_vcoSet2_vco_tempco_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet3, ACPHY_radio_pll_vcoSet3_vco_cal_en_MASK,
	                0x1<<ACPHY_radio_pll_vcoSet3_vco_cal_en_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet3, ACPHY_radio_pll_vcoSet3_vco_cal_en_empco_MASK,
	                0x1<<ACPHY_radio_pll_vcoSet3_vco_cal_en_empco_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet3, ACPHY_radio_pll_vcoSet3_vco_cap_mode_MASK,
	                0x0<<ACPHY_radio_pll_vcoSet3_vco_cap_mode_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet4, ACPHY_radio_pll_vcoSet4_vco_ib_ctrl_MASK,
	                0x0<<ACPHY_radio_pll_vcoSet4_vco_ib_ctrl_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet3, ACPHY_radio_pll_vcoSet3_vco_por_MASK,
	                0x0<<ACPHY_radio_pll_vcoSet3_vco_por_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_r1, ACPHY_radio_pll_lf_r1_lf_r1_MASK,
	                0x0<<ACPHY_radio_pll_lf_r1_lf_r1_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_r2r3, ACPHY_radio_pll_lf_r2r3_lf_r2_MASK,
	                0xc<<ACPHY_radio_pll_lf_r2r3_lf_r2_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_r2r3, ACPHY_radio_pll_lf_r2r3_lf_r3_MASK,
	                0xc<<ACPHY_radio_pll_lf_r2r3_lf_r3_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_cm, ACPHY_radio_pll_lf_cm_lf_rs_cm_MASK,
	                0xff<<ACPHY_radio_pll_lf_cm_lf_rs_cm_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_cm, ACPHY_radio_pll_lf_cm_lf_rf_cm_MASK,
	                0xc<<ACPHY_radio_pll_lf_cm_lf_rf_cm_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_cSet1, ACPHY_radio_pll_lf_cSet1_lf_c1_MASK,
	                0x99<<ACPHY_radio_pll_lf_cSet1_lf_c1_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_cSet1, ACPHY_radio_pll_lf_cSet1_lf_c2_MASK,
	                0x8b<<ACPHY_radio_pll_lf_cSet1_lf_c2_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_cSet2, ACPHY_radio_pll_lf_cSet2_lf_c3_MASK,
	                0x8b<<ACPHY_radio_pll_lf_cSet2_lf_c3_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_lf_cSet2, ACPHY_radio_pll_lf_cSet2_lf_c4_MASK,
	                0x8f<<ACPHY_radio_pll_lf_cSet2_lf_c4_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_cp, ACPHY_radio_pll_cp_cp_kpd_scale_MASK,
	                0x34<<ACPHY_radio_pll_cp_cp_kpd_scale_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_cp, ACPHY_radio_pll_cp_cp_ioff_MASK,
	                0x60<<ACPHY_radio_pll_cp_cp_ioff_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_ldo, ACPHY_radio_ldo_ldo_1p2_xtalldo1p2_lowquiescenten_MASK,
	                0x0<<ACPHY_radio_ldo_ldo_1p2_xtalldo1p2_lowquiescenten_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_ldo, ACPHY_radio_ldo_ldo_2p5_lowpwren_VCO_MASK,
	                0x0<<ACPHY_radio_ldo_ldo_2p5_lowpwren_VCO_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_ldo, ACPHY_radio_ldo_ldo_2p5_lowquiescenten_VCO_aux_MASK,
	                0x0<<ACPHY_radio_ldo_ldo_2p5_lowquiescenten_VCO_aux_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_ldo, ACPHY_radio_ldo_ldo_2p5_lowpwren_VCO_aux_MASK,
	                0x0<<ACPHY_radio_ldo_ldo_2p5_lowpwren_VCO_aux_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_ldo, ACPHY_radio_ldo_ldo_2p5_lowquiescenten_CP_MASK,
	                0x0<<ACPHY_radio_ldo_ldo_2p5_lowquiescenten_CP_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_ldo, ACPHY_radio_ldo_ldo_2p5_lowquiescenten_VCO_MASK,
	                0x0<<ACPHY_radio_ldo_ldo_2p5_lowquiescenten_VCO_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_lna2g, ACPHY_radio_rxrf_lna2g_lna2g_lna1_bias_idac_MASK,
	                0x2<<ACPHY_radio_rxrf_lna2g_lna2g_lna1_bias_idac_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_lna2g,
	                ACPHY_radio_rxrf_lna2g_lna2g_lna2_aux_bias_idac_MASK,
	                0x8<<ACPHY_radio_rxrf_lna2g_lna2g_lna2_aux_bias_idac_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_lna2g,
	                ACPHY_radio_rxrf_lna2g_lna2g_lna2_main_bias_idac_MASK,
	                0x8<<ACPHY_radio_rxrf_lna2g_lna2g_lna2_main_bias_idac_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_lna5g, ACPHY_radio_rxrf_lna5g_lna5g_lna1_bias_idac_MASK,
	                0x8<<ACPHY_radio_rxrf_lna5g_lna5g_lna1_bias_idac_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_lna5g,
	                ACPHY_radio_rxrf_lna5g_lna5g_lna2_aux_bias_idac_MASK,
	                0x7<<ACPHY_radio_rxrf_lna5g_lna5g_lna2_aux_bias_idac_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_lna5g,
	                ACPHY_radio_rxrf_lna5g_lna5g_lna2_main_bias_idac_MASK,
	                0x4<<ACPHY_radio_rxrf_lna5g_lna5g_lna2_main_bias_idac_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_rxmix, ACPHY_radio_rxrf_rxmix_rxmix2g_aux_bias_idac_MASK,
	                0x8<<ACPHY_radio_rxrf_rxmix_rxmix2g_aux_bias_idac_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_rxmix, ACPHY_radio_rxrf_rxmix_rxmix2g_main_bias_idac_MASK,
	                0x8<<ACPHY_radio_rxrf_rxmix_rxmix2g_main_bias_idac_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_rxmix,
	                ACPHY_radio_rxrf_rxmix_rxmix5g_gm_aux_bias_idac_i_MASK,
	                0x8<<ACPHY_radio_rxrf_rxmix_rxmix5g_gm_aux_bias_idac_i_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxrf_rxmix,
	                ACPHY_radio_rxrf_rxmix_rxmix5g_gm_main_bias_idac_i_MASK,
	                0x8<<ACPHY_radio_rxrf_rxmix_rxmix5g_gm_main_bias_idac_i_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxbb_tia, ACPHY_radio_rxbb_tia_tia_DC_Ib1_MASK,
	                0x6<<ACPHY_radio_rxbb_tia_tia_DC_Ib1_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxbb_tia, ACPHY_radio_rxbb_tia_tia_DC_Ib2_MASK,
	                0x6<<ACPHY_radio_rxbb_tia_tia_DC_Ib2_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxbb_tia, ACPHY_radio_rxbb_tia_tia_Ib_I_MASK,
	                0x6<<ACPHY_radio_rxbb_tia_tia_Ib_I_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxbb_tia, ACPHY_radio_rxbb_tia_tia_Ib_Q_MASK,
	                0x6<<ACPHY_radio_rxbb_tia_tia_Ib_Q_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxbb_bias12, ACPHY_radio_rxbb_bias12_lpf_bias_level1_MASK,
	                0x4<<ACPHY_radio_rxbb_bias12_lpf_bias_level1_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxbb_bias12, ACPHY_radio_rxbb_bias12_lpf_bias_level2_MASK,
	                0x8<<ACPHY_radio_rxbb_bias12_lpf_bias_level2_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxbb_bias34, ACPHY_radio_rxbb_bias34_lpf_bias_level3_MASK,
	                0x10<<ACPHY_radio_rxbb_bias34_lpf_bias_level3_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_rxbb_bias34, ACPHY_radio_rxbb_bias34_lpf_bias_level4_MASK,
	                0x20<<ACPHY_radio_rxbb_bias34_lpf_bias_level4_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet4, ACPHY_radio_pll_vcoSet4_vco_tempco_dcadj_1p2_MASK,
	                0x9<<ACPHY_radio_pll_vcoSet4_vco_tempco_dcadj_1p2_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet2, ACPHY_radio_pll_vcoSet2_vco_vctrl_buf_ical_MASK,
	                0x3<<ACPHY_radio_pll_vcoSet2_vco_vctrl_buf_ical_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet4, ACPHY_radio_pll_vcoSet4_vco_ib_bias_opamp_MASK,
	                0x6<<ACPHY_radio_pll_vcoSet4_vco_ib_bias_opamp_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet4,
	                ACPHY_radio_pll_vcoSet4_vco_ib_bias_opamp_fastsettle_MASK,
	                0xf<<ACPHY_radio_pll_vcoSet4_vco_ib_bias_opamp_fastsettle_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet1, ACPHY_radio_pll_vcoSet1_vco_bypass_vctrl_buf_MASK,
	                0x0<<ACPHY_radio_pll_vcoSet1_vco_bypass_vctrl_buf_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet3, ACPHY_radio_pll_vcoSet3_vco_HDRM_CAL_MASK,
	                0x2<<ACPHY_radio_pll_vcoSet3_vco_HDRM_CAL_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet2, ACPHY_radio_pll_vcoSet2_vco_ICAL_MASK,
	                0x16<<ACPHY_radio_pll_vcoSet2_vco_ICAL_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet3, ACPHY_radio_pll_vcoSet3_vco_ICAL_1p2_MASK,
	                0xc<<ACPHY_radio_pll_vcoSet3_vco_ICAL_1p2_SHIFT);
	phy_reg_mod(pi, ACPHY_radio_pll_vcoSet1, ACPHY_radio_pll_vcoSet1_vco_USE_2p5V_MASK,
	                0x1<<ACPHY_radio_pll_vcoSet1_vco_USE_2p5V_SHIFT);
}

static void
wlc_phy_radio2069_pwron_seq(phy_info_t *pi)
{
	/* Note: if RfctrlCmd.rfctrl_bundle_en = 0, then rfpll_pu = radio_pu
	   So, to make rfpll_pu = 0 & radio_pwrup = 1, make RfctrlCmd.rfctrl_bundle_en = 1
	*/
	uint16 txovrd = phy_reg_read(pi, ACPHY_RfctrlCoreTxPus0);
	uint16 rfctrlcmd = phy_reg_read(pi, ACPHY_RfctrlCmd) & 0xfc38;

#ifndef BCMQT
	/*
	 * Force an assert when not building with QT, so that this function is checked
	 * for validity for non-2069 radios.
	 */
	ASSERT(RADIOID(pi->pubpi.radioid) == BCM2069_ID);
#endif

	/* Using usleep of 100us below, so don't need these */
	phy_reg_write(pi, ACPHY_Pllldo_resetCtrl, 0);
	phy_reg_write(pi, ACPHY_Rfpll_resetCtrl, 0);
	phy_reg_write(pi, ACPHY_Logen_AfeDiv_reset, 0x2000);

	/* Start with everything off: {radio, rfpll, plldlo, logen}_{pu, reset} = 0 */
	phy_reg_write(pi, ACPHY_RfctrlCmd, rfctrlcmd);
	phy_reg_write(pi, ACPHY_RfctrlCoreGlobalPus, 0);
	phy_reg_write(pi, ACPHY_RfctrlOverrideGlobalPus, 0xd);
	phy_reg_write(pi, ACPHY_RfctrlCoreTxPus0, txovrd & 0x7e7f);
	phy_reg_write(pi, ACPHY_RfctrlOverrideTxPus0,
	              phy_reg_read(pi, ACPHY_RfctrlOverrideTxPus0) | 0x180);

	/* ***  Start Radio rfpll pwron seq  ***
	   Start with chip_pu = 0, por_reset = 0, rfctrl_bundle_en = 0
	*/
	phy_reg_write(pi, ACPHY_RfctrlCmd, rfctrlcmd);

	/* Toggle jtag reset (not required for uCode PM) */
	phy_reg_write(pi, ACPHY_RfctrlCmd, rfctrlcmd | 1);
	OSL_DELAY(1);
	phy_reg_write(pi, ACPHY_RfctrlCmd, rfctrlcmd | 0);

	/* Update preferred values (not required for uCode PM) */
	wlc_phy_radio2069_upd_prfd_values(pi);

	/* Toggle radio_reset (while radio_pu = 1) */
	MOD_RADIO_REG(pi, RF2, VREG_CFG, bg_filter_en, 0);   /* radio_reset = 1 */
	phy_reg_write(pi, ACPHY_RfctrlCmd, rfctrlcmd | 6);   /* radio_pwrup = 1, rfpll_pu = 0 */
	OSL_DELAY(100);                                      /* radio_reset to be high for 100us */
	MOD_RADIO_REG(pi, RF2, VREG_CFG, bg_filter_en, 1);   /* radio_reset = 0 */

	/* {rfpll, pllldo, logen}_{pu, reset} pwron seq */
	phy_reg_write(pi, ACPHY_RfctrlCoreGlobalPus, 0xd);
	phy_reg_write(pi, ACPHY_RfctrlCmd, rfctrlcmd | 2);
	phy_reg_write(pi, ACPHY_RfctrlCoreTxPus0, txovrd | 0x180);
	OSL_DELAY(100);
	phy_reg_write(pi, ACPHY_RfctrlCoreGlobalPus, 0x4);
	phy_reg_write(pi, ACPHY_RfctrlCoreTxPus0, txovrd & 0xfeff);
}

static void
wlc_phy_radio2069_upd_prfd_values(phy_info_t *pi)
{
	uint8 core;
	radio_20xx_prefregs_t *prefregs_2069_ptr = NULL;

	if (RADIOID(pi->pubpi.radioid) == BCM2069_ID) {
		switch (RADIOREV(pi->pubpi.radiorev)) {
		case 3:
			prefregs_2069_ptr = prefregs_2069_rev3;
			break;
		case 4:
			prefregs_2069_ptr = prefregs_2069_rev4;
			break;
		case 16:
			prefregs_2069_ptr = prefregs_2069_rev16;
			break;
		case 17:
			prefregs_2069_ptr = prefregs_2069_rev17;
			break;
		case 18:
			prefregs_2069_ptr = prefregs_2069_rev18;
			break;
		case 23:
			prefregs_2069_ptr = prefregs_2069_rev23;
			break;
		case 24:
			prefregs_2069_ptr = prefregs_2069_rev24;
			break;
		case 25:
			prefregs_2069_ptr = prefregs_2069_rev25;
			break;
		case 26:
			prefregs_2069_ptr = prefregs_2069_rev26;
			break;
		default:
			PHY_ERROR(("wl%d: %s: Unsupported radio revision %d\n",
				pi->sh->unit, __FUNCTION__, RADIOREV(pi->pubpi.radiorev)));
			ASSERT(0);
			return;
		}
	} else {
		ASSERT(RADIOID(pi->pubpi.radioid) == BCM20691_ID);
		switch (RADIOREV(pi->pubpi.radiorev)) {
		case 1:
			prefregs_2069_ptr = prefregs_20691_rev1;
			break;
		default:
			PHY_ERROR(("wl%d: %s: Unsupported radio revision %d\n",
				pi->sh->unit, __FUNCTION__, RADIOREV(pi->pubpi.radiorev)));
			ASSERT(0);
			return;
		}
	}

	if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
		BFL_SROM11_WLAN_BT_SH_XTL) {
		MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_BT, 1);
	}

	/* Update preferred values */
	wlc_phy_init_radio_prefregs_allbands(pi, prefregs_2069_ptr);

	/* **** NOTE : Move the following to XLS (whenever possible) *** */

	/* Reg conflict with 2069 rev 16 */
	if (ACREV0) {
		MOD_RADIO_REG(pi, RFP, OVR15, ovr_rfpll_rst_n, 1);
		MOD_RADIO_REG(pi, RFP, OVR15, ovr_rfpll_en_vcocal, 1);
		MOD_RADIO_REG(pi, RFP, OVR16, ovr_rfpll_vcocal_rstn, 1);

		MOD_RADIO_REG(pi, RFP, OVR15, ovr_rfpll_cal_rst_n, 1);
		MOD_RADIO_REG(pi, RFP, OVR15, ovr_rfpll_pll_pu, 1);
		MOD_RADIO_REG(pi, RFP, OVR15, ovr_rfpll_vcocal_cal, 1);
	} else {
		MOD_RADIO_REG(pi, RFP, GE16_OVR16, ovr_rfpll_en_vcocal, 1);
		MOD_RADIO_REG(pi, RFP, GE16_OVR17, ovr_rfpll_vcocal_rstn, 1);
		MOD_RADIO_REG(pi, RFP, GE16_OVR16, ovr_rfpll_rst_n, 1);

		MOD_RADIO_REG(pi, RFP, GE16_OVR16, ovr_rfpll_cal_rst_n, 1);
		MOD_RADIO_REG(pi, RFP, GE16_OVR16, ovr_rfpll_pll_pu, 1);
		MOD_RADIO_REG(pi, RFP, GE16_OVR16, ovr_rfpll_vcocal_cal, 1);

		/* Ensure that we read the values that are actually applied */
		/* to the radio block and not just the radio register values */

		MOD_RADIO_REG(pi, RF0, GE16_READOVERRIDES, read_overrides, 1);
		MOD_RADIO_REG(pi, RFP, GE16_READOVERRIDES, read_overrides, 1);
	}

	/* Jira: CRDOT11ACPHY-108. Force bg_pulse to be high. Need to check impact on ADC SNR. */
	MOD_RADIO_REG(pi, RF2, BG_CFG1, bg_pulse, 1);

	/* Give control of bg_filter_en to radio */
	MOD_RADIO_REG(pi, RF2, OVR2, ovr_vreg_bg_filter_en, 1);

	FOREACH_CORE(pi, core) {
		/* Fang's recommended settings */
		MOD_RADIO_REGFLDC(pi, RF_2069_ADC_RC1(core), ADC_RC1, adc_ctl_RC_9_8, 1);
		MOD_RADIO_REGFLDC(pi, RF_2069_ADC_RC2(core), ADC_RC2, adc_ctrl_RC_17_16, 2);

		/* These should be 0, as they are controlled via direct control lines
		   If they are 1, then during 5g, they will turn on
		*/
		MOD_RADIO_REGFLDC(pi, RF_2069_PA2G_CFG1(core), PA2G_CFG1, pa2g_bias_cas_pu, 0);
		MOD_RADIO_REGFLDC(pi, RF_2069_PA2G_CFG1(core), PA2G_CFG1, pa2g_2gtx_pu, 0);
		MOD_RADIO_REGFLDC(pi, RF_2069_PA2G_CFG1(core), PA2G_CFG1, pa2g_bias_pu, 0);
		MOD_RADIO_REGFLDC(pi, RF_2069_PAD2G_CFG1(core), PAD2G_CFG1, pad2g_pu, 0);
	}
}


#ifdef ENABLE_FCBS
/* List of all PHY TBL segments to be saved during FCBS */
static fcbs_phytbl_list_entry fcbs_phytbl16_list_acphy [ ] =
{
	{ ACPHY_TBL_ID_RFSEQ, 0x30, 6 },
	{ ACPHY_TBL_ID_RFSEQ, 0x100, 9 },
	/* Commenting out for now, because HW FCBS TBL is short
	 * { ACPHY_TBL_ID_RFSEQ, 0x40, 6 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x50, 6 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0xa0, 5 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0xb0, 5 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0xc0, 4 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0xF9, 3 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x100, 9 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x140, 11 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x150, 11 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x160, 11 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x360, 11 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x370, 11 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x380, 11 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x3cd, 1 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x3cf, 1 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x3dd, 1 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x3df, 1 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x3ed, 1 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x3ef, 1 },
	 *{ ACPHY_TBL_ID_RFSEQ, 0x441, 5 },
	 */
	{0xFFFF, 0xFFFF, 0}
};

/* List of all Radio regs to be saved during FCBS */
static fcbs_radioreg_core_list_entry fcbs_radioreg_list_acphy [ ] =
{
	{ RF1_2069_REV0_TXGM_LOFT_COARSE_I, 0x1 },
	{ RF0_2069_REV0_TXGM_LOFT_COARSE_Q, 0x0 },
	{ RF2_2069_REV0_TXGM_LOFT_FINE_I, 0x2   },
	{ RFP_2069_REV0_PLL_ADC4, 0x4           },
	{ RF0_2069_REV0_TXGM_LOFT_FINE_Q, 0x3           },
	{ RF1_2069_REV0_NBRSSI_BIAS, 0x1                },
	{ RF0_2069_REV0_LPF_BIAS_LEVELS_HIGH, 0x0       },
	{ RF0_2069_REV0_LPF_BIAS_LEVELS_LOW, 0x3        },
	{ RF2_2069_REV0_LPF_BIAS_LEVELS_MID, 0x2        },
	{ RF0_2069_REV0_LOGEN2G_IDAC2, 0x0              },
	{ RFP_2069_REV0_PLL_ADC1, 0x4                   },
	{ RF2_2069_REV0_BG_TRIM2, 0x2                   },
	{ RF0_2069_REV0_PA2G_TSSI, 0x3                  },
	{ RF0_2069_REV0_LNA2G_CFG1, 0x3                 },
	{ RF0_2069_REV0_LNA2G_CFG2, 0x3                 },
	{ RF0_2069_REV0_LNA2G_RSSI, 0x3                 },
	{ RF0_2069_REV0_LNA5G_CFG1, 0x3                 },
	{ RF0_2069_REV0_LNA5G_CFG2, 0x3                 },
	{ RF0_2069_REV0_LNA5G_RSSI, 0x3                 },
	{ RF0_2069_REV0_RXMIX2G_CFG1, 0x3               },
	{ RF0_2069_REV0_RXMIX5G_CFG1, 0x3               },
	{ RF0_2069_REV0_RXRF2G_CFG1, 0x3                },
	{ RF0_2069_REV0_RXRF5G_CFG1, 0x3                },
	{ RF0_2069_REV0_TIA_CFG1,	0X3                },
	{ RF0_2069_REV0_LPF_MAIN_CONTROLS, 0X3          },
	{ RF0_2069_REV0_LPF_CORNER_FREQUENCY_TUNING, 0x3},
	{ RF0_2069_REV0_TXGM_CFG1, 0x3 },
	{ RF0_2069_REV0_PGA2G_CFG1, 0x3},
	{ RF0_2069_REV0_PGA5G_CFG1, 0x3},
	{ RF0_2069_REV0_PAD5G_CFG1, 0x3},
	{ RF0_2069_REV0_PA5G_CFG1, 0x3 },
	{ RF0_2069_REV0_LOGEN2G_CFG1, 0x0 },
	{ RF0_2069_REV0_LOGEN2G_CFG2, 0x3 },
	{ RF0_2069_REV0_LOGEN5G_CFG2, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE1, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE2, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE3, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE4, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE5, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE6, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE7, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE9, 0x3 },
	{ RF0_2069_REV0_ADC_CALCODE10, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE11, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE12, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE13, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE14, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE15, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE16, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE17, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE18, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE19, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE20, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE21, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE23, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE24, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE25, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE26, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE27, 0x3},
	{ RF0_2069_REV0_ADC_CALCODE28, 0x3},
	{ RFP_2069_REV0_PLL_VCOCAL5, 0x4},
	{ RFP_2069_REV0_PLL_VCOCAL6, 0x4},
	{ RFP_2069_REV0_PLL_VCOCAL2, 0x4},
	{ RFP_2069_REV0_PLL_VCOCAL1, 0x4},
	{ RFP_2069_REV0_PLL_VCOCAL11, 0x4},
	{ RFP_2069_REV0_PLL_VCOCAL12, 0x4},
	{ RFP_2069_REV0_PLL_FRCT2, 0x4},
	{ RFP_2069_REV0_PLL_FRCT3, 0x4},
	{ RFP_2069_REV0_PLL_VCOCAL10, 0x4},
	{ RFP_2069_REV0_PLL_XTAL3, 0x4},
	{ RFP_2069_REV0_PLL_VCO2, 0x4},
	{ RF0_2069_REV0_LOGEN5G_CFG1, 0x0},
	{ RFP_2069_REV0_PLL_VCO8, 0x4},
	{ RFP_2069_REV0_PLL_VCO6, 0x4},
	{ RFP_2069_REV0_PLL_VCO3, 0x4},
	{ RFP_2069_REV0_PLL_XTALLDO1, 0x4},
	{ RFP_2069_REV0_PLL_HVLDO1, 0x4},
	{ RFP_2069_REV0_PLL_HVLDO2, 0x4},
	{ RFP_2069_REV0_PLL_VCO5, 0x4},
	{ RFP_2069_REV0_PLL_VCO4, 0x4},
	{ RFP_2069_REV0_PLL_LF4, 0x4},
	{ RFP_2069_REV0_PLL_LF5, 0x4},
	{ RFP_2069_REV0_PLL_LF7, 0x4},
	{ RFP_2069_REV0_PLL_LF2, 0x4},
	{ RFP_2069_REV0_PLL_LF3, 0x4},
	{ RFP_2069_REV0_PLL_CP4, 0x4},
	{ RFP_2069_REV0_PLL_DSP1, 0x4},
	{ RFP_2069_REV0_PLL_DSP2, 0x4},
	{ RFP_2069_REV0_PLL_DSP3, 0x4},
	{ RFP_2069_REV0_PLL_DSP4, 0x4},
	{ RFP_2069_REV0_PLL_DSP6, 0x4},
	{ RFP_2069_REV0_PLL_DSP7, 0x4},
	{ RFP_2069_REV0_PLL_DSP8, 0x4},
	{ RFP_2069_REV0_PLL_DSP9, 0x4},
	{ RF0_2069_REV0_LOGEN2G_TUNE, 0x0},
	{ RF0_2069_REV0_LNA2G_TUNE, 0x3},
	{ RF0_2069_REV0_TXMIX2G_CFG1, 0x3},
	{ RF0_2069_REV0_PGA2G_CFG2, 0x3},
	{ RF0_2069_REV0_PAD2G_TUNE, 0x3},
	{ RF0_2069_REV0_LOGEN5G_TUNE1, 0x0},
	{ RF0_2069_REV0_LOGEN5G_TUNE2, 0x0},
	{ RF0_2069_REV0_LOGEN5G_RCCR, 0x3},
	{ RF0_2069_REV0_LNA5G_TUNE, 0x3},
	{ RF0_2069_REV0_TXMIX5G_CFG1, 0x3},
	{ RF0_2069_REV0_PGA5G_CFG2, 0x3},
	{ RF0_2069_REV0_PAD5G_TUNE, 0x3},
	{ RFP_2069_REV0_PLL_CP5, 0x4},
	{ RF0_2069_REV0_AFEDIV1, 0x0},
	{ RF0_2069_REV0_AFEDIV2, 0x0},
	{ RF0_2069_REV0_ADC_CFG5, 0x3},
	{ RFP_2069_REV0_OVR15, 0x4 },
	{ RFP_2069_REV0_OVR16, 0x4 },
	{ 0xFFFF, 0}
};

/* List of all PHY regs to be saved during FCBS */
static uint16 fcbs_phyreg_list_acphy [ ] =
{
	ACPHY_TxResamplerMuDelta0u,
	ACPHY_TxResamplerMuDelta0l,
	ACPHY_TxResamplerMuDeltaInit0u,
	ACPHY_TxResamplerMuDeltaInit0l,
	ACPHY_TxResamplerMuDelta1u,
	ACPHY_TxResamplerMuDelta1l,
	ACPHY_TxResamplerMuDeltaInit1u,
	ACPHY_TxResamplerMuDeltaInit1l,
	ACPHY_TxResamplerMuDelta2u,
	ACPHY_TxResamplerMuDelta2l,
	ACPHY_TxResamplerMuDeltaInit2u,
	ACPHY_TxResamplerMuDeltaInit2l,
	ACPHY_RfctrlCoreLowPwr0,
	ACPHY_RfctrlCoreLowPwr1,
	ACPHY_RfctrlCoreLowPwr2,
	ACPHY_nvcfg3,
	ACPHY_DcFiltAddress,
	ACPHY_RxFilt40Num00,
	ACPHY_RxFilt40Num01,
	ACPHY_RxFilt40Num02,
	ACPHY_RxFilt40Den00,
	ACPHY_RxFilt40Den01,
	ACPHY_RxFilt40Num10,
	ACPHY_RxFilt40Num11,
	ACPHY_RxFilt40Num12,
	ACPHY_RxFilt40Den10,
	ACPHY_RxFilt40Den11,
	ACPHY_RxStrnFilt40Num00,
	ACPHY_RxStrnFilt40Num01,
	ACPHY_RxStrnFilt40Num02,
	ACPHY_RxStrnFilt40Den00,
	ACPHY_RxStrnFilt40Den01,
	ACPHY_RxStrnFilt40Num10,
	ACPHY_RxStrnFilt40Num11,
	ACPHY_RxStrnFilt40Num12,
	ACPHY_RxStrnFilt40Den10,
	ACPHY_RxStrnFilt40Den11,
	ACPHY_rxfdiqImbCompCtrl,
	ACPHY_RfctrlCoreAfeCfg20,
	ACPHY_RfctrlCoreAfeCfg21,
	ACPHY_RfctrlCoreAfeCfg22,
	ACPHY_crsminpoweroffset0,
	ACPHY_crsminpoweroffsetSub10,
	ACPHY_crsmfminpoweroffset0,
	ACPHY_crsmfminpoweroffsetSub10,
	ACPHY_crsminpoweroffset1,
	ACPHY_crsminpoweroffsetSub11,
	ACPHY_crsmfminpoweroffset1,
	ACPHY_crsmfminpoweroffsetSub11,
	ACPHY_crsminpoweroffset2,
	ACPHY_crsminpoweroffsetSub12,
	ACPHY_crsmfminpoweroffset2,
	ACPHY_crsmfminpoweroffsetSub12,
	ACPHY_Core0RssiClipMuxSel,
	ACPHY_Core1RssiClipMuxSel,
	ACPHY_Core2RssiClipMuxSel,
	ACPHY_Core0FastAgcClipCntTh,
	ACPHY_Core1FastAgcClipCntTh,
	ACPHY_Core2FastAgcClipCntTh,
	0xFFFF
};

static bool
wlc_phy_prefcbsinit_acphy(phy_info_t *pi, int chanidx)
{
	phy_reg_mod(pi, ACPHY_ChannelSwitch,
	            ACPHY_ChannelSwitch_ChannelIndicator_MASK, (uint16)chanidx);
	return TRUE;
}

static bool
wlc_phy_postfcbsinit_acphy(phy_info_t *pi, int chanidx)
{
	return TRUE;
	/* Nothing to do right now */
}

static bool
wlc_phy_fcbsinit_acphy(phy_info_t *pi, int chanidx, chanspec_t chanspec)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	pi->phy_fcbs.fcbs_phytbl16_list = fcbs_phytbl16_list_acphy;
	pi->phy_fcbs.fcbs_radioreg_list = fcbs_radioreg_list_acphy;
	pi->phy_fcbs.fcbs_phyreg_list = fcbs_phyreg_list_acphy;
	/* Obtain the buffer pointers for the appropriate FCBS channel */
	pi->phy_fcbs.phytbl16_buf[chanidx] = pi_ac->ac_fcbs.phytbl16_buf_ChanA;

	if (pi->phy_fcbs.FCBS_ucode) {
		/* Starting address of the FCBS cache on the on-chip RAM */
		pi->phy_fcbs.cache_startaddr = FCBS_ACPHY_TMPLRAM_STARTADDR;

		/* Shared memory locations for specifying the starting offset
		   of the radio register cache, phytbl16 cache, phytbl32 cache
		   phyreg cache, bphyctrl register and the FCBS channel specific
		   starting cache address
		*/
		pi->phy_fcbs.shmem_radioreg = M_FCBS_ACPHY_RADIOREG;
		pi->phy_fcbs.shmem_phytbl16 = M_FCBS_ACPHY_PHYTBL16;
		pi->phy_fcbs.shmem_phytbl32 = M_FCBS_ACPHY_PHYTBL32;
		pi->phy_fcbs.shmem_phyreg = M_FCBS_ACPHY_PHYREG;
		pi->phy_fcbs.shmem_bphyctrl = M_FCBS_ACPHY_BPHYCTRL;
		pi->phy_fcbs.shmem_cache_ptr = M_FCBS_ACPHY_TEMPLATE_PTR;
	}
	return TRUE;
}

static bool
wlc_phy_prefcbs_acphy(phy_info_t *pi, int chanidx)
{
	/* CRDOT11ACPHY-176 :: Timing issues cause the VCO cal not to be triggered during
	 * channel switch. we need to clear these ovr bits before the switch and
	 * set them during switch (throough the FCBS TBL which then triggers the VCO cal
	 */
	/* Reg conflict with 2069 rev 16 */
	if (ACREV0) {
		MOD_RADIO_REG(pi, RFP, OVR15, ovr_rfpll_vcocal_cal, 0);
		MOD_RADIO_REG(pi, RFP, OVR15, ovr_rfpll_rst_n, 0);
		MOD_RADIO_REG(pi, RFP, OVR16, ovr_rfpll_vcocal_rstn, 0);
	} else {
		MOD_RADIO_REG(pi, RFP, GE16_OVR16, ovr_rfpll_vcocal_cal, 0);
		MOD_RADIO_REG(pi, RFP, GE16_OVR16, ovr_rfpll_rst_n, 0);
		MOD_RADIO_REG(pi, RFP, GE16_OVR17, ovr_rfpll_vcocal_rstn, 0);

	}

	return TRUE;
}

static bool
wlc_phy_postfcbs_acphy(phy_info_t *pi, int chanidx)
{
	return TRUE;
	/* Nothing to do right now */
}

static bool
wlc_phy_fcbs_acphy(phy_info_t *pi, int chanidx)
{
	phy_reg_mod(pi, ACPHY_ChannelSwitch,
	            ACPHY_ChannelSwitch_ChannelIndicator_MASK,
	            (uint16)chanidx << ACPHY_ChannelSwitch_ChannelIndicator_SHIFT);
	phy_reg_mod(pi, ACPHY_ChannelSwitch, ACPHY_ChannelSwitch_VCO_cal_reqd_MASK,
	            0x1 << ACPHY_ChannelSwitch_VCO_cal_reqd_SHIFT);
	phy_reg_mod(pi, ACPHY_ChannelSwitch, ACPHY_ChannelSwitch_SwitchTrigger_MASK,
	            0x1 << ACPHY_ChannelSwitch_SwitchTrigger_SHIFT);
	SPINWAIT(phy_reg_read(pi, ACPHY_ChannelSwitch) &
	        (0x1 << ACPHY_ChannelSwitch_SwitchTrigger_SHIFT), ACPHY_SPINWAIT_FCBS_SWITCH);
	ASSERT(!(phy_reg_read(pi, ACPHY_ChannelSwitch) &
	      (0x1 << ACPHY_ChannelSwitch_SwitchTrigger_SHIFT)));
	/* PHY seems to be a weird state after switch and we need rset CCA */
	wlc_phy_resetcca_acphy(pi);

	return TRUE;
}

#endif /* ENABLE_FCBS */

#define MAX_2069_RCAL_WAITLOOPS 100
/* rcal takes ~50us */
static void
wlc_phy_radio2069_rcal(phy_info_t *pi)
{
	uint8 done, rcal_val;
	uint16 rcal_itr;


	/* Power-up rcal clock (need both of them for rcal) */
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL1, 1);
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL, 1);

	/* Rcal can run with 40mhz cls, no diving */
	MOD_RADIO_REG(pi, RFP, PLL_XTAL5, xtal_sel_RCCAL, 0);
	MOD_RADIO_REG(pi, RFP, PLL_XTAL5, xtal_sel_RCCAL1, 0);

	/* Make connection with the external 10k resistor */
	/* Turn off all test points in cgpaio block to avoid conflict */
	MOD_RADIO_REG(pi, RF2, CGPAIO_CFG1, cgpaio_pu, 1);
	write_radio_reg(pi, RF2_2069_CGPAIO_CFG2, 0);
	write_radio_reg(pi, RF2_2069_CGPAIO_CFG3, 0);
	write_radio_reg(pi, RF2_2069_CGPAIO_CFG4, 0);
	write_radio_reg(pi, RF2_2069_CGPAIO_CFG5, 0);

	/* NOTE: xtal_pu, xtal_buf_pu & xtalldo_pu direct control lines should be(& are) ON */

	/* Toggle the rcal pu for calibration engine */
	MOD_RADIO_REG(pi, RF2, RCAL_CFG, pu, 0);
	OSL_DELAY(1);
	MOD_RADIO_REG(pi, RF2, RCAL_CFG, pu, 1);

	/* Wait for rcal to be done, max = 10us * 100 = 1ms  */
	done = 0;
	for (rcal_itr = 0; rcal_itr < MAX_2069_RCAL_WAITLOOPS; rcal_itr++) {
		OSL_DELAY(10);
		done = READ_RADIO_REGFLD(pi, RF2, RCAL_CFG, i_wrf_jtag_rcal_valid);
		if (done == 1) {
			break;
		}
	}

	/* don't call assert on QT */
	if (!ISSIM_ENAB(pi->sh->sih)) {
		ASSERT(done & 0x1);
	}

	/* Status */
	rcal_val = READ_RADIO_REGFLD(pi, RF2, RCAL_CFG, i_wrf_jtag_rcal_value);
	rcal_val = rcal_val >> 1;
	PHY_INFORM(("wl%d: %s rcal=%d\n", pi->sh->unit, __FUNCTION__, rcal_val));

	if (!ISSIM_ENAB(pi->sh->sih)) {
		/* Valid range of values for rcal */
		ASSERT((rcal_val > 2) && (rcal_val < 15));
	}

	/*  Power down blocks not needed anymore */
	MOD_RADIO_REG(pi, RF2, CGPAIO_CFG1, cgpaio_pu, 0);
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL1, 0);
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL, 0);
	MOD_RADIO_REG(pi, RF2, RCAL_CFG, pu, 0);
}

#define MAX_2069_RCCAL_WAITLOOPS 100
#define NUM_2069_RCCAL_CAPS 3
/* rccal takes ~3ms per, i.e. ~9ms total */
static void
wlc_phy_radio2069_rccal(phy_info_t *pi)
{
	uint8 cal, core, done, rccal_val[NUM_2069_RCCAL_CAPS];
	uint16 rccal_itr, n0, n1;

	/* lpf, adc, dacbuf */
	uint8 sr[] = {0x1, 0x0, 0x0};
	uint8 sc[] = {0x0, 0x2, 0x1};
	uint8 x1[] = {0x1c, 0x70, 0x40};
	uint16 trc[] = {0x14a, 0x101, 0x11a};
	uint16 gmult_const = 193;

	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		if (pi->xtalfreq == 40000000) {
			gmult_const = 160;
		} else if (pi->xtalfreq == 37400000) {
			gmult_const = 158;
			trc[0] = 0x22d;
			trc[1] = 0xf0;
			trc[2] = 0x10a;
		} else {
			gmult_const = 160;
		}
	} else {
		gmult_const = 193;
	}

	/* Powerup rccal driver & set divider radio (rccal needs to run at 20mhz) */
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL, 1);
	MOD_RADIO_REG(pi, RFP, PLL_XTAL5, xtal_sel_RCCAL, 2);

	/* Calibrate lpf, adc, dacbuf */
	for (cal = 0; cal < NUM_2069_RCCAL_CAPS; cal++) {
		/* Setup */
		MOD_RADIO_REG(pi, RF2, RCCAL_CFG, sr, sr[cal]);
		MOD_RADIO_REG(pi, RF2, RCCAL_CFG, sc, sc[cal]);
		MOD_RADIO_REG(pi, RF2, RCCAL_LOGIC1, rccal_X1, x1[cal]);
		write_radio_reg(pi, RF2_2069_RCCAL_TRC, trc[cal]);

		/* For dacbuf force fixed dacbuf cap to be 0 while calibration, restore it later */
		if (cal == 2) {
			FOREACH_CORE(pi, core) {
				MOD_RADIO_REGFLDC(pi, RF_2069_DAC_CFG2(core), DAC_CFG2,
				                  DACbuf_fixed_cap, 0);
				MOD_RADIO_REGFLDC(pi, RF_2069_OVR21(core), OVR21,
				                  ovr_afe_DACbuf_fixed_cap, 1);
			}
		}

		/* Toggle RCCAL power */
		MOD_RADIO_REG(pi, RF2, RCCAL_CFG, pu, 0);
		OSL_DELAY(1);
		MOD_RADIO_REG(pi, RF2, RCCAL_CFG, pu, 1);

		OSL_DELAY(35);

		/* Start RCCAL */
		MOD_RADIO_REG(pi, RF2, RCCAL_LOGIC1, rccal_START, 1);

		/* Wait for rcal to be done, max = 100us * 100 = 10ms  */
		done = 0;
		for (rccal_itr = 0; rccal_itr < MAX_2069_RCCAL_WAITLOOPS; rccal_itr++) {
			OSL_DELAY(100);
			done = READ_RADIO_REGFLD(pi, RF2, RCCAL_LOGIC2, rccal_DONE);
			if (done == 1)
				break;
		}

		/* Stop RCCAL */
		MOD_RADIO_REG(pi, RF2, RCCAL_LOGIC1, rccal_START, 0);

		if (done == 0) {
			/* don't call assert on QT */
			if (!ISSIM_ENAB(pi->sh->sih)) {
				ASSERT(done & 0x1);
			}
		} else {
			if (cal == 0) {
				/* lpf */
				n0 = read_radio_reg(pi, RF2_2069_RCCAL_LOGIC3);
				n1 = read_radio_reg(pi, RF2_2069_RCCAL_LOGIC4);
				/* gmult = (30/40) * (n1-n0) = (193 * (n1-n0)) >> 8 */
				rccal_val[cal] = (gmult_const * (n1 - n0)) >> 8;
				pi_ac->rccal_gmult = rccal_val[cal];
				pi_ac->rccal_gmult_rc = pi_ac->rccal_gmult;
				PHY_INFORM(("wl%d: %s rccal_lpf_gmult = %d\n", pi->sh->unit,
				            __FUNCTION__, rccal_val[cal]));
			} else if (cal == 1) {
				/* adc */
				rccal_val[cal] = READ_RADIO_REGFLD(pi, RF2, RCCAL_LOGIC5,
				                                   rccal_raw_adc1p2);
				PHY_INFORM(("wl%d: %s rccal_adc = %d\n", pi->sh->unit,
				            __FUNCTION__, rccal_val[cal]));

				/* don't change this loop to active core loop,
				   gives slightly higher floor, why?
				*/
				FOREACH_CORE(pi, core) {
					MOD_RADIO_REGFLDC(pi, RF_2069_ADC_RC1(core), ADC_RC1,
					                  adc_ctl_RC_4_0, rccal_val[cal]);
					MOD_RADIO_REGFLDC(pi, RF_2069_TIA_CFG3(core), TIA_CFG3,
					                  rccal_hpc, rccal_val[cal]);
				}
			} else {
				/* dacbuf */
				rccal_val[cal] = READ_RADIO_REGFLD(pi, RF2, RCCAL_LOGIC5,
				                                   rccal_raw_dacbuf);
				pi_ac->rccal_dacbuf = rccal_val[cal];

				/* take away the override on dacbuf fixed cap */
				FOREACH_CORE(pi, core) {
					MOD_RADIO_REGFLDC(pi, RF_2069_OVR21(core), OVR21,
					                  ovr_afe_DACbuf_fixed_cap, 0);
				}
				PHY_INFORM(("wl%d: %s rccal_dacbuf = %d\n", pi->sh->unit,
				            __FUNCTION__, rccal_val[cal]));
			}
		}

		/* Turn off rccal */
		MOD_RADIO_REG(pi, RF2, RCCAL_CFG, pu, 0);
	}

	/* Powerdown rccal driver */
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL, 0);
}

#define MAX_2069_VCOCAL_WAITLOOPS 100
/* vcocal should take < 120 us */
static void
wlc_phy_radio2069_vcocal(phy_info_t *pi)
{
	/* Use legacy mode */
	uint8 done, itr, legacy_n = 0;

	/* VCO cal mode selection */
	MOD_RADIO_REG(pi, RFP, PLL_VCOCAL10, rfpll_vcocal_ovr_mode, legacy_n);

	/* VCO-Cal startup seq */
	MOD_RADIO_REG(pi, RFP, PLL_CFG2, rfpll_rst_n, 0);
	MOD_RADIO_REG(pi, RFP, PLL_VCOCAL13, rfpll_vcocal_rst_n, 0);
	MOD_RADIO_REG(pi, RFP, PLL_VCOCAL1, rfpll_vcocal_cal, 0);
	OSL_DELAY(11);
	MOD_RADIO_REG(pi, RFP, PLL_CFG2, rfpll_rst_n, 1);
	MOD_RADIO_REG(pi, RFP, PLL_VCOCAL13, rfpll_vcocal_rst_n, 1);
	OSL_DELAY(1);
	MOD_RADIO_REG(pi, RFP, PLL_VCOCAL1, rfpll_vcocal_cal, 1);

	/* Wait for vco_cal to be done, max = 100us * 10 = 1ms  */
	done = 0;
	for (itr = 0; itr < MAX_2069_VCOCAL_WAITLOOPS; itr++) {
		OSL_DELAY(10);
		done = READ_RADIO_REGFLD(pi, RFP, PLL_VCOCAL14, rfpll_vcocal_done_cal);
		if (done == 1)
			break;
	}

	/* Need to wait extra time after vcocal done bit is high for it to settle */
	OSL_DELAY(120);

	if ((done == 0) && (!ISSIM_ENAB(pi->sh->sih))) {
		ASSERT(done & 0x1);
	}

	PHY_INFORM(("wl%d: %s vcocal done\n", pi->sh->unit, __FUNCTION__));
}

#define MAX_2069_AFECAL_WAITLOOPS 10
static void
wlc_phy_radio2069_afecal(phy_info_t *pi)
{
	uint8 core, itr, done_i, done_q;
	uint16 adc_cfg4, afectrl_ovrd, afectrl_core_cfg1, afectrl_core_cfg2;

	/* Used to latch (clk register) rcal, rccal, ADC cal code */
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL, 1);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* Save the regs before  them */
		afectrl_core_cfg1 = phy_reg_read(pi, ACPHYREGCE(RfctrlCoreAfeCfg1, core));
		afectrl_core_cfg2 = phy_reg_read(pi, ACPHYREGCE(RfctrlCoreAfeCfg2, core));
		afectrl_ovrd = phy_reg_read(pi, ACPHYREGCE(RfctrlOverrideAfeCfg, core));

		/* AFE CAL setup (phyregs) */
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg1, core, afe_iqadc_reset, 1);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, core, afe_iqadc_reset, 1);

		/* AFE CAL setup (radio) */
		MOD_RADIO_REGFLDC(pi, RF_2069_ADC_CFG3(core), ADC_CFG3, flash_calrstb, 0);
		OSL_DELAY(100);
		MOD_RADIO_REGFLDC(pi, RF_2069_ADC_CFG3(core), ADC_CFG3, flash_calrstb, 1);

		/* Start the cal. {I, Q}_{ch_calmode, ch_run_flashcal} = 1 */
		adc_cfg4 = read_radio_reg(pi, RF_2069_ADC_CFG4(core));
		write_radio_reg(pi, RF_2069_ADC_CFG4(core), adc_cfg4 | 0xf);

		/* Wait for cal to be done */
		done_i = done_q = 0;
		for (itr = 0; itr < MAX_2069_AFECAL_WAITLOOPS; itr++) {
			OSL_DELAY(10);
			done_i = READ_RADIO_REGFLDC(pi, RF_2069_ADC_STATUS(core), ADC_STATUS,
			                          i_wrf_jtag_afe_iqadc_Ich_cal_state);
			done_q = READ_RADIO_REGFLDC(pi, RF_2069_ADC_STATUS(core), ADC_STATUS,
			                          i_wrf_jtag_afe_iqadc_Qch_cal_state);
			if ((done_i == 1) && (done_q == 1)) {
				PHY_INFORM(("wl%d: %s afecal(%d) done\n", pi->sh->unit,
				            __FUNCTION__, core));
				break;
			}
		}

		/* don't call assert on QT */
		if (!ISSIM_ENAB(pi->sh->sih)) {
			ASSERT((done_i == 1) && (done_q == 1));
		}

		/* Restore */
		write_radio_reg(pi, RF_2069_ADC_CFG4(core), (adc_cfg4 & 0xfff0));  /* calMode = 0 */
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAfeCfg, core), afectrl_ovrd);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg1, core), afectrl_core_cfg1);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg2, core), afectrl_core_cfg2);
	}

	/* Turn off the clk */
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL, 0);

	if (RADIOREV(pi->pubpi.radiorev) < 4) {
		/* JIRA (CRDOT11ACPHY-153) calCodes are inverted for 4360a0 */
		wlc_phy_radio2069_afecal_invert(pi);
	}
}

static void
wlc_phy_radio2069_afecal_invert(phy_info_t *pi)
{
	uint8 core;
	uint16 calcode;

	/* Switch on the clk */
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL, 1);

	/* Output calCode = 1:14, latched = 15:28 */

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* Use calCodes 1:14 instead of 15:28 */
		MOD_RADIO_REGFLDC(pi, RF_2069_OVR3(core), OVR3, ovr_afe_iqadc_flash_calcode_Ich, 1);
		MOD_RADIO_REGFLDC(pi, RF_2069_OVR3(core), OVR3, ovr_afe_iqadc_flash_calcode_Qch, 1);

		/* Invert the CalCodes */
		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE28(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE14(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE27(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE13(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE26(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE12(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE25(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE11(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE24(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE10(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE23(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE9(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE22(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE8(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE21(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE7(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE20(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE6(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE19(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE5(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE18(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE4(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE17(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE3(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE16(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE2(core), ~calcode & 0xffff);

		calcode = read_radio_reg(pi, RF_2069_ADC_CALCODE15(core));
		write_radio_reg(pi, RF_2069_ADC_CALCODE1(core), ~calcode & 0xffff);
	}

	/* Turn off the clk */
	MOD_RADIO_REG(pi, RFP, PLL_XTAL2, xtal_pu_RCCAL, 0);
}


/*
************************   PHY procs **************************
*/

void
wlc_phy_table_write_acphy(phy_info_t *pi, uint32 id, uint32 len, uint32 offset, uint32 width,
                          const void *data)
{
	acphytbl_info_t tbl;

	/*
	 * PHY_TRACE(("wlc_phy_table_write_acphy, id %d, len %d, offset %d, width %d\n",
	 * 	id, len, offset, width));
	*/
	tbl.tbl_id = id;
	tbl.tbl_len = len;
	tbl.tbl_offset = offset;
	tbl.tbl_width = width;
	tbl.tbl_ptr = data;

	wlc_phy_write_table_ext(pi, &tbl, ACPHY_TableID, ACPHY_TableOffset,
	                        ACPHY_TableDataWide, ACPHY_TableDataHi, ACPHY_TableDataLo);
}

void
wlc_phy_table_read_acphy(phy_info_t *pi, uint32 id, uint32 len, uint32 offset, uint32 width,
                         void *data)
{
	acphytbl_info_t tbl;

	/*	PHY_TRACE(("wlc_phy_table_read_acphy, id %d, len %d, offset %d, width %d\n",
	 *	id, len, offset, width));
	 */
	tbl.tbl_id = id;
	tbl.tbl_len = len;
	tbl.tbl_offset = offset;
	tbl.tbl_width = width;
	tbl.tbl_ptr = data;

	wlc_phy_read_table_ext(pi, &tbl, ACPHY_TableID, ACPHY_TableOffset,
	                       ACPHY_TableDataWide, ACPHY_TableDataHi, ACPHY_TableDataLo);
}

/* initialize the static tables defined in auto-generated wlc_phytbl_ac.c,
 * see acphyprocs.tcl, proc acphy_init_tbls
 * After called in the attach stage, all the static phy tables are reclaimed.
 */
static void
WLBANDINITFN(wlc_phy_static_table_download_acphy)(phy_info_t *pi)
{
	uint idx;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	if (pi->phy_init_por) {
		/* these tables are not affected by phy reset, only power down */
		if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
			for (idx = 0; idx < acphytbl_info_sz_rev2; idx++) {
				wlc_phy_write_table_ext(pi, &acphytbl_info_rev2[idx], ACPHY_TableID,
				  ACPHY_TableOffset, ACPHY_TableDataWide, ACPHY_TableDataHi,
					ACPHY_TableDataLo);
			}
		} else {
			for (idx = 0; idx < acphytbl_info_sz_rev0; idx++) {
				wlc_phy_write_table_ext(pi, &acphytbl_info_rev0[idx], ACPHY_TableID,
				  ACPHY_TableOffset, ACPHY_TableDataWide, ACPHY_TableDataHi,
					ACPHY_TableDataLo);
			}
		}
	}

	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_detach_acphy(phy_info_t *pi)
{
	MFREE(pi->sh->osh, pi->u.pi_acphy, sizeof(phy_info_acphy_t));
}

static void
wlc_phy_resetcca_acphy(phy_info_t *pi)
{
	uint16 val;

	/* MAC should be suspended before calling this function */
	ASSERT((R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC) == 0);
	/* 4335 bilge count sequence fix */
	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		wlapi_bmac_phyclk_fgc(pi->sh->physhim, ON);

		val = phy_reg_read(pi, ACPHY_BBConfig);
		phy_reg_write(pi, ACPHY_BBConfig, val | (1 << ACPHY_BBConfig_resetCCA_SHIFT));
		OSL_DELAY(1);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, rxfe_bilge_cnt, 0);
		OSL_DELAY(1);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 1);
		OSL_DELAY(1);
		wlapi_bmac_phyclk_fgc(pi->sh->physhim, OFF);
		OSL_DELAY(1);
		phy_reg_write(pi, ACPHY_BBConfig, val & ~(1 << ACPHY_BBConfig_resetCCA_SHIFT));
		OSL_DELAY(1);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 0);
	} else {
		wlapi_bmac_phyclk_fgc(pi->sh->physhim, ON);

		val = phy_reg_read(pi, ACPHY_BBConfig);
		phy_reg_write(pi, ACPHY_BBConfig, val | (1 << ACPHY_BBConfig_resetCCA_SHIFT));
		OSL_DELAY(1);
		phy_reg_write(pi, ACPHY_BBConfig, val & ~(1 << ACPHY_BBConfig_resetCCA_SHIFT));

		wlapi_bmac_phyclk_fgc(pi->sh->physhim, OFF);
	}

	/* wait for reset2rx finish, which is triggered by resetcca in hw */
	OSL_DELAY(2);
}

static void
BCMATTACHFN(wlc_phy_rfldo_trim_value)(phy_info_t *pi)
{

	uint8 otp_select;
	uint16 otp = 0;

	uint32 sromctl;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));


	sromctl = si_get_sromctl(pi->sh->sih);
	otp_select = (sromctl >> 4) & 0x1;
	if (otp_select == 0)
		si_set_sromctl(pi->sh->sih, sromctl | (1 << 4));
	otp_read_word(pi->sh->sih, 16, &otp);
	if (otp_select == 0)
		si_set_sromctl(pi->sh->sih, sromctl);
	otp = (otp >> 8) & 0x1f;

	pi->u.pi_acphy->rfldo = otp;
}

static void
WLBANDINITFN(wlc_phy_init_acphy)(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	uint32 rfldo = 0;
	uint8 phyver;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	phyver = READ_PHYREG(pi, ACPHY_Version, version);
	if (phyver <= 1) {
		if (phyver == 0) {
			/* 4360a0 */
			if (pi_ac->rfldo == 0) {
				/* Use rfldo = 1.26 V for phyver = 0 by default */
				rfldo = 5;
			} else {
				rfldo = pi_ac->rfldo;
			}
		} else {
			/* 4360b0 */
			if (pi_ac->rfldo <= 3) {
				rfldo = 0;
			} else {
				rfldo = pi_ac->rfldo - 3;
			}
		}

		rfldo = rfldo << 20;
		si_pmu_regcontrol(pi->sh->sih, 0, 0x1f00000, rfldo);
	}

	/* Start with PHY not controlling any gpio's */
	si_gpiocontrol(pi->sh->sih, 0xffff, 0, GPIO_DRV_PRIORITY);

	/* Init regs/tables only once that do not get reset on phy_reset */
	wlc_phy_set_regtbl_on_pwron_acphy(pi);

	/* Call chan_change with default chan */
	pi_ac->init = TRUE;

	wlc_phy_chanspec_set_acphy(pi, pi->radio_chanspec);

#if defined(AP) && defined(RADAR)
	/* Initialze Radar detect, on or off */
	wlc_phy_radar_detect_init(pi, pi->sh->radar);
#endif /* defined(AP) && defined(RADAR) */

	pi_ac->init = FALSE;
	pi_ac->init_done = TRUE;
	/* Sets Assert and Deassert thresholds for all 20MHz subbands for EDCRS */
	wlc_phy_edcrs_thresh_acphy(pi);
	if (ACREV0) {
		phy_reg_write(pi, ACPHY_timeoutEn, 0x13);
		phy_reg_write(pi, ACPHY_ofdmpaydecodetimeoutlen, 0x7d0);
		phy_reg_write(pi, ACPHY_cckpaydecodetimeoutlen, 0x7d0);
	}

	if (pi->phy_init_por)
		pi->interf.curr_home_channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	wlc_phy_txpwrctrl_idle_tssi_meas_acphy(pi);
}

/* enable/disable receiving of LDPC frame */
void
wlc_phy_update_rxldpc_acphy(phy_info_t *pi, bool ldpc)
{
	phy_info_acphy_t *pi_ac;
	uint16 val, bit;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	if (ldpc == pi_ac->ac_rxldpc_override)
		return;

	pi_ac->ac_rxldpc_override = ldpc;
	val = phy_reg_read(pi, ACPHY_HTSigTones);

	bit = ACPHY_HTSigTones_support_ldpc_MASK;
	if (ldpc)
		val |= bit;
	else
		val &= ~bit;
	phy_reg_write(pi, ACPHY_HTSigTones, val);
}

static void
WLBANDINITFN(wlc_phy_edcrs_thresh_acphy)(phy_info_t *pi)
{


	if (ACREV1X1_IS(pi->pubpi.phy_rev))
		return;

	/* Set the EDCRS Assert Threshold to -71dBm */
	phy_reg_write(pi, ACPHY_ed_crs20LAssertThresh0, 917);
	phy_reg_write(pi, ACPHY_ed_crs20LAssertThresh1, 917);
	phy_reg_write(pi, ACPHY_ed_crs20UAssertThresh0, 917);
	phy_reg_write(pi, ACPHY_ed_crs20UAssertThresh1, 917);
	phy_reg_write(pi, ACPHY_ed_crs20Lsub1AssertThresh0, 917);
	phy_reg_write(pi, ACPHY_ed_crs20Lsub1AssertThresh1, 917);
	phy_reg_write(pi, ACPHY_ed_crs20Usub1AssertThresh0, 917);
	phy_reg_write(pi, ACPHY_ed_crs20Usub1AssertThresh1, 917);

	/* Set the EDCRS De-assert Threshold to -77dBm */
	phy_reg_write(pi, ACPHY_ed_crs20LDeassertThresh0, 789);
	phy_reg_write(pi, ACPHY_ed_crs20LDeassertThresh1, 789);
	phy_reg_write(pi, ACPHY_ed_crs20UDeassertThresh0, 789);
	phy_reg_write(pi, ACPHY_ed_crs20UDeassertThresh1, 789);
	phy_reg_write(pi, ACPHY_ed_crs20Lsub1DeassertThresh0, 789);
	phy_reg_write(pi, ACPHY_ed_crs20Lsub1DeassertThresh1, 789);
	phy_reg_write(pi, ACPHY_ed_crs20Usub1DeassertThresh0, 789);
	phy_reg_write(pi, ACPHY_ed_crs20Usub1DeassertThresh1, 789);
}

void wlc_phy_ed_thres_acphy(phy_info_t *pi, int32 *assert_thresh_dbm, bool set_threshold)
{
	/* Set the EDCRS Assert and De-assert Threshold
	The de-assert threshold is set to 6dB lower then the assert threshold
	Accurate Formula:64*log2(round((10.^((THRESHOLD_dBm +65-30)./10).*50).*(2^9./0.4).^2))
	Simplified Accurate Formula: 64*(THRESHOLD_dBm + 75)/(10*log10(2)) + 832;
	Implemented Approximate Formula: 640000*(THRESHOLD_dBm + 75)/30103 + 832;
	*/
	int32 assert_thres_val, de_assert_thresh_val;

	if (set_threshold == TRUE) {
		assert_thres_val = (640000*(*assert_thresh_dbm + 75) + 25045696)/30103;
		de_assert_thresh_val = (640000*(*assert_thresh_dbm + 69) + 25045696)/30103;
		/* Set the EDCRS Assert Threshold */
		phy_reg_write(pi, ACPHY_ed_crs20LAssertThresh0, (uint16)assert_thres_val);
		phy_reg_write(pi, ACPHY_ed_crs20LAssertThresh1, (uint16)assert_thres_val);
		phy_reg_write(pi, ACPHY_ed_crs20UAssertThresh0, (uint16)assert_thres_val);
		phy_reg_write(pi, ACPHY_ed_crs20UAssertThresh1, (uint16)assert_thres_val);
		phy_reg_write(pi, ACPHY_ed_crs20Lsub1AssertThresh0, (uint16)assert_thres_val);
		phy_reg_write(pi, ACPHY_ed_crs20Lsub1AssertThresh1, (uint16)assert_thres_val);
		phy_reg_write(pi, ACPHY_ed_crs20Usub1AssertThresh0, (uint16)assert_thres_val);
		phy_reg_write(pi, ACPHY_ed_crs20Usub1AssertThresh1, (uint16)assert_thres_val);

		/* Set the EDCRS De-assert Threshold */
		phy_reg_write(pi, ACPHY_ed_crs20LDeassertThresh0, (uint16)de_assert_thresh_val);
		phy_reg_write(pi, ACPHY_ed_crs20LDeassertThresh1, (uint16)de_assert_thresh_val);
		phy_reg_write(pi, ACPHY_ed_crs20UDeassertThresh0, (uint16)de_assert_thresh_val);
		phy_reg_write(pi, ACPHY_ed_crs20UDeassertThresh1, (uint16)de_assert_thresh_val);
		phy_reg_write(pi, ACPHY_ed_crs20Lsub1DeassertThresh0, (uint16)de_assert_thresh_val);
		phy_reg_write(pi, ACPHY_ed_crs20Lsub1DeassertThresh1, (uint16)de_assert_thresh_val);
		phy_reg_write(pi, ACPHY_ed_crs20Usub1DeassertThresh0, (uint16)de_assert_thresh_val);
		phy_reg_write(pi, ACPHY_ed_crs20Usub1DeassertThresh1, (uint16)de_assert_thresh_val);
	}
	else {
		assert_thres_val = phy_reg_read(pi, ACPHY_ed_crs20LAssertThresh0);
		*assert_thresh_dbm = ((((assert_thres_val - 832)*30103)) - 48000000)/640000;
	}
}

static void
WLBANDINITFN(wlc_phy_cal_init_acphy)(phy_info_t *pi)
{
	printf("%s: NOT Implemented\n", __FUNCTION__);
}

static void
wlc_phy_chanspec_set_acphy(phy_info_t *pi, chanspec_t chanspec)
{
	int freq;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	chan_info_radio2069_t *ci = NULL;
	chan_info_radio2069revGE16_t *ciGE16 = NULL;
	chan_info_radio20691_t *ci20691 = NULL;
	bool band_changed = FALSE, bw_changed = FALSE, phy_init = pi_ac->init;
	uint8 max_rxchain, tx_pwr_ctrl_state = PHY_TPC_HW_OFF;
	uint8 run_subband_cust = 0;
	uint8 stall_val, orig_rxfectrl1;
#ifndef PPR_API
	uint8 qdbm;
	int i;
#endif

	PHY_TRACE(("wl%d: %s chan = %d\n", pi->sh->unit, __FUNCTION__, CHSPEC_CHANNEL(chanspec)));

	if (RADIOID(pi->pubpi.radioid) == BCM2069_ID) {
		if (!wlc_phy_chan2freq_acphy(pi, CHSPEC_CHANNEL(chanspec), &freq, &ci, &ciGE16))
			return;
	} else {
		ASSERT(RADIOID(pi->pubpi.radioid) == BCM20691_ID);
		if (!wlc_phy_chan2freq_acdcphy(pi, CHSPEC_CHANNEL(chanspec), &freq, &ci20691))
			return;
	}

	/* channel specifi PLL frequency only for 4335/4345 WLBGA */
	if ((
		CHIPID(pi->sh->chip) == BCM4335_CHIP_ID) &&
		(pi->sh->chippkg == BCM4335_WLBGA_PKG_ID) &&
		(!CST4335_CHIPMODE_USB20D(pi->sh->sih->chipst))) {
		wlc_phy_set_spurmode(pi, (uint16)freq);
	}

	if (pi_ac->init)
	{
		if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
			phy_reg_mod(pi, ACPHY_BT_SwControl, ACPHY_BT_SwControl_inv_btcx_prisel_MASK,
			0x1<<ACPHY_BT_SwControl_inv_btcx_prisel_SHIFT);
		}
	}

	/* BAND CHANGED */
	if (phy_init || (pi_ac->curr_band2g != CHSPEC_IS2G(chanspec))) {
		pi_ac->curr_band2g = CHSPEC_IS2G(chanspec);
		band_changed = TRUE;
	}

	if (phy_init || (pi_ac->curr_bw != CHSPEC_BW(chanspec))) {
		pi_ac->curr_bw = CHSPEC_BW(chanspec);
		bw_changed = TRUE;

		/* If called from init, don't call this, as this is called before init */
		if (!pi_ac->init) {
			/* Set the phy BW as dictated by the chanspec (also calls phy_reset) */
			wlapi_bmac_bw_set(pi->sh->physhim, CHSPEC_BW(chanspec));
		}

		phy_init = TRUE;

		OSL_DELAY(2);
	}

	/* Change the band bit. Do this after phy_reset */
	if (CHSPEC_IS2G(chanspec)) {
		MOD_PHYREG(pi, ACPHY_ChannelControl, currentBand, 0);
	} else {
		MOD_PHYREG(pi, ACPHY_ChannelControl, currentBand, 1);
	}

	/* JIRA(CRDOT11ACPHY-143) : Turn off receiver duing channel change */
	wlc_phy_stay_in_carriersearch_acphy(pi, TRUE);

	/* Disable stalls and hold FIFOs in reset before changing channels */
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	orig_rxfectrl1 = READ_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset);

	ACPHY_DISABLE_STALL(pi);
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 1);

	/* Change the channel, and then load registers, in the meantime vco_cal should settle */
	/* logen_reset needs to be toggled whnenever bandsel bit is changed */
	/* On a bw change, phy_reset is issued which causes currentBand to toggle */
	/* So, issue logen_reset on both band & bw change */
	wlc_phy_chanspec_radio_set((wlc_phy_t *)pi, chanspec);
	if (ci != NULL || ciGE16 != NULL)
		wlc_phy_chanspec_radio2069_setup(pi, ci, ciGE16, (band_changed | bw_changed));
#ifndef BCMQT
	/*
	 * Force an assert when not building with QT, until wlc_phy_chanspec_radio20691_setup is
	 * implemented.
	 */
	else
		ASSERT(RADIOID(pi->pubpi.radioid) == BCM2069_ID);
#endif

	/* Restore FIFO reset and Stalls */
	ACPHY_ENABLE_STALL(pi, stall_val);
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, orig_rxfectrl1);

	if (phy_init) {
		wlc_phy_set_reg_on_reset_acphy(pi);
		wlc_phy_set_tbl_on_reset_acphy(pi);

		/* If any rx cores were disabled before phy_init,
		 * disable them again since phy_init enables all rx cores
		 * Also make RfseqCoreActv2059.EnTx = hw_txchain & rxchain
		 */
		max_rxchain =  (1 << READ_PHYREG(pi, ACPHY_PhyCapability0, NumberOfStreams)) - 1;
		if ((pi->sh->phyrxchain != max_rxchain) || (pi->sh->hw_phytxchain != max_rxchain)) {
			wlc_phy_rxcore_setstate_acphy((wlc_phy_t *)pi, pi->sh->phyrxchain);
		}
	}

	if (phy_init || band_changed) {
		wlc_phy_set_regtbl_on_band_change_acphy(pi);
	}

	if (phy_init || bw_changed) {
		wlc_phy_set_regtbl_on_bw_change_acphy(pi);
	}

	if (ci != NULL || ciGE16 != NULL)
		wlc_phy_set_regtbl_on_chan_change_acphy(pi, ci, ciGE16);
#ifndef BCMQT
	/*
	 * Force an assert when not building with QT, until a 20691 version of
	 * wlc_phy_set_regtbl_on_chan_change_acphy is implemented.
	 */
	else
		ASSERT(RADIOID(pi->pubpi.radioid) == BCM2069_ID);
#endif

	/* Rx gainctrl (if not QT) */
	if (!ISSIM_ENAB(pi->sh->sih)) {
		pi_ac->aci = NULL;
		if (!wlc_phy_is_scan_chan_acphy(pi)) {
			pi->interf.curr_home_channel = CHSPEC_CHANNEL(pi->radio_chanspec);

			/* Get pointer to current aci channel list */
			pi_ac->aci = wlc_phy_desense_aci_getset_chanidx_acphy(pi, chanspec, TRUE);
		}

		/* Merge ACI & BT params into one */
		wlc_phy_desense_calc_total_acphy(pi);

		wlc_phy_rxgainctrl_set_gaintbls_acphy(pi, phy_init, band_changed, bw_changed);

		if (run_subband_cust) {
			/* Set INIT, Clip gains, clip thresh (fixed) */
			if (CHSPEC_IS2G(chanspec)) {
				wlc_phy_subband_cust_2g_acphy(pi);
			} else {
				wlc_phy_subband_cust_5g_acphy(pi);
			}
		} else {
			/* Set INIT, Clip gains, clip thresh (srom based) */
			wlc_phy_rxgainctrl_gainctrl_acphy(pi);
		}

		/* Desense on top of default gainctrl, if desense on (otherwise restore defaults) */
		wlc_phy_desense_apply_acphy(pi, pi_ac->total_desense.on);
	}

	/* bw_change requires afe cal */
	if (pi_ac->init || bw_changed) {
		/* so that all the afe_iqadc signals are correctly set */
		wlc_phy_resetcca_acphy(pi);
		OSL_DELAY(1);
		wlc_phy_radio2069_afecal(pi);
	}

	if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5)) {
		phy_reg_mod(pi, ACPHY_BT_SwControl, ACPHY_BT_SwControl_inv_btcx_prisel_MASK,
		        0x1<<ACPHY_BT_SwControl_inv_btcx_prisel_SHIFT);
	}

	/* set txgain in case txpwrctrl is disabled */
	wlc_phy_txpwr_fixpower_acphy(pi);

	/* ensure power control is off before starting cals */
	tx_pwr_ctrl_state = pi->txpwrctrl;
	wlc_phy_txpwrctrl_enable_acphy(pi, PHY_TPC_HW_OFF);

	/* Idle TSSI measurement and pwrctrl setup */
	wlc_phy_txpwrctrl_idle_tssi_meas_acphy(pi);
#ifdef PPR_API
	; /* not set to txpwroveride mode, maxPwr comes from SROM */
#else
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		qdbm = 60;
	} else {
		qdbm = 44;
	}

	for (i = 0; i < TXP_NUM_RATES; i++)
		pi->tx_user_target[i] = qdbm;

	pi->txpwroverride = TRUE;
#endif

	wlc_phy_txpwrctrl_pwr_setup_acphy(pi);
	/* re-enable (if necessary) tx power control */
	wlc_phy_txpwrctrl_enable_acphy(pi, tx_pwr_ctrl_state);

	/* Clean up */
	wlc_phy_resetcca_acphy(pi);

	wlc_phy_compute_rssi_gainerror_acphy(pi);

	wlc_phy_stay_in_carriersearch_acphy(pi, FALSE);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		wlc_phy_classifier_acphy(pi, ACPHY_ClassifierCtrl_classifierSel_MASK, 7);
	} else {
		/* No bphy in 5g */
		wlc_phy_classifier_acphy(pi, ACPHY_ClassifierCtrl_classifierSel_MASK, 6);
	}
	if (ACREV1X1_IS(pi->pubpi.phy_rev))
		MOD_PHYREG(pi, ACPHY_RfseqMode, CoreActv_override, 0);
}


static uint8
wlc_phy_search_chan_for_spurmode(uint16 *pchanlist, uint16 max_channels, uint16 freq)
{
	uint8 chan_found = 0, i;
	/* Search for the channel */
	for (i = 0; i < max_channels; i++) {
		if (freq == pchanlist[i]) {
				chan_found = 1;
				return chan_found;
		}
	}
	return chan_found;
}

void
wlc_phy_set_spurmode(phy_info_t *pi, uint16 freq)
{
	/* Below channels use spurmode 2 and the rest of the channels use spurmode 3 */
	uint16 bbpllch_961[64] = {5180, 5190, 5240, 5250, 5300, 5310, 5360, 5370, 5420, 5430, 5480,
	5490, 5540, 5550, 5600, 5660, 5670, 5720, 5725, 5730, 5785, 5845, 5905, 5200, 5260, 5320,
	5380, 5560, 5620, 5680, 5805, 5855, 2412, 2417, 2422, 2427, 2432, 2437, 2442, 2447, 2452,
	2457, 2462, 2467, 2472, 2484, 5210, 5775};
	uint16 bbpllch_964[64] =
	{5220, 5230, 5280, 5340, 5350, 5400, 5410, 5460, 5470, 5520, 5530, 5580,
	5590, 5640, 5650, 5700, 5710, 5765, 5825, 5885, 5270, 5330, 5390, 5440, 5450, 5510, 5570,
	5630, 5745, 5755, 5815, 5835, 5865, 5875, 5885, 5895};
	uint16 bbpllch_962[4] = {5610, 5795};
	uint16 bbpllch_965[4] = {5500};
	uint16 bbpllch_966[4] = {5690};
	uint16 bbpllch_968[4] = {5290};
	uint8 spuravoid = 2;

	if (pi->block_for_slowcal) {
		pi->blocked_freq_for_slowcal = freq;
		return;
	}

	if (wlc_phy_search_chan_for_spurmode(bbpllch_961, 48, freq)) {
		PHY_TRACE(("Channel belongs to spurmode 2 list; chanfreq %d: PLLfre:961Mhz; %s \n",
			freq, __FUNCTION__));
		si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, 2);
		spuravoid = 2;
	} else if (wlc_phy_search_chan_for_spurmode(bbpllch_964, 36, freq)) {
		PHY_TRACE(("Channel belongs to spurmode 3 list; chanfreq %d: PLLfre:964Mhz; %s \n",
			freq, __FUNCTION__));
		si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, 3);
		spuravoid = 3;
	} else if (wlc_phy_search_chan_for_spurmode(bbpllch_962, 2, freq)) {
		PHY_TRACE(("Channel belongs to spurmode 4 list; chanfreq %d: PLLfre:962Mhz; %s \n",
			freq, __FUNCTION__));
		si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, 4);
		spuravoid = 4;
	} else if (wlc_phy_search_chan_for_spurmode(bbpllch_965, 1, freq)) {
		PHY_TRACE(("Channel belongs to spurmode 5 list; chanfreq %d: PLLfre:965Mhz; %s \n",
			freq, __FUNCTION__));
		si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, 5);
		spuravoid = 5;
	} else if (wlc_phy_search_chan_for_spurmode(bbpllch_966, 1, freq)) {
		PHY_TRACE(("Channel belongs to spurmode 6 list; chanfreq %d: PLLfre:966Mhz; %s \n",
			freq, __FUNCTION__));
		si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, 6);
		spuravoid = 6;
	} else if (wlc_phy_search_chan_for_spurmode(bbpllch_968, 1, freq)) {
		PHY_TRACE(("Channel belongs to spurmode 8 list; chanfreq %d: PLLfre:968Mhz; %s \n",
			freq, __FUNCTION__));
		si_pmu_spuravoid(pi->sh->sih, pi->sh->osh, 8);
		spuravoid = 8;
	}
	wlapi_switch_macfreq(pi->sh->physhim, spuravoid);
}


/*
Initialize chip regs(RWP) & tables with init vals that do not get reset with phy_reset
*/
static void
wlc_phy_set_regtbl_on_pwron_acphy(phy_info_t *pi)
{
	uint16 val;

	/* force afediv(core 0, 1, 2) always high */
	phy_reg_write(pi, ACPHY_AfeClkDivOverrideCtrl, 0x77);

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		phy_reg_write(pi, ACPHY_AfeClkDivOverrideCtrlNO, 0x3);
	}
	/* Remove RFCTRL signal overrides for all cores */
	ACPHYREG_BCAST(pi, ACPHY_RfctrlIntc0, 0);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideAfeCfg0, 0);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideGains0, 0);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideLpfCT0, 0);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideLpfSwtch0, 0);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideAfeCfg0, 0);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideLowPwrCfg0, 0);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideAuxTssi0, 0);
	ACPHYREG_BCAST(pi, ACPHY_AfectrlOverride0, 0);

	/* logen_pwrup = 1, logen_reset = 0 */
	ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreTxPus0, 0x80);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideTxPus0, 0x180);

	/* Switch off rssi2 & rssi3 as they are not used in normal operation */
	ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreRxPus0, 0);
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideRxPus0, 0x5000);

	/* Disable the SD-ADC's overdrive detect feature */
	val = phy_reg_read(pi, ACPHY_RfctrlCoreAfeCfg20);
	val |= ACPHY_RfctrlCoreAfeCfg20_afe_iqadc_reset_ov_det_MASK;
	/* val |= ACPHY_RfctrlCoreAfeCfg20_afe_iqadc_clamp_en_MASK; */
	ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreAfeCfg20, val);
	val = phy_reg_read(pi, ACPHY_RfctrlOverrideAfeCfg0);
	val |= ACPHY_RfctrlOverrideAfeCfg0_afe_iqadc_reset_ov_det_MASK;
	/* val |= ACPHY_RfctrlOverrideAfeCfg0_afe_iqadc_clamp_en_MASK; */
	ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideAfeCfg0, val);

	/* initialize all the tables defined in auto-generated wlc_phytbl_ac.c,
	 * see acphyprocs.tcl, proc acphy_init_tbls
	 *  skip static one after first up
	 */
	PHY_TRACE(("wl%d: %s, dnld tables = %d\n", pi->sh->unit,
	           __FUNCTION__, pi->phy_init_por));

	/* these tables are not affected by phy reset, only power down */
	wlc_phy_static_table_download_acphy(pi);

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		/* wihr 0x15a 0x8000 */

		/* acphy_rfctrl_override bg_pulse 1 */
		/* Override is not required now ..bg pulsing is enabled along with tssi sleep */
		/* val  = phy_reg_read(pi, ACPHY_RfctrlCoreTxPus0); */
		/* val |= (1 << 14); */
		/* phy_reg_write(pi, ACPHY_RfctrlCoreTxPus0, val); */
		/* val = phy_reg_read(pi, ACPHY_RfctrlOverrideTxPus0); */
		/* val |= (1 << 9); */
		/* phy_reg_write(pi, ACPHY_RfctrlOverrideTxPus0, val); */

	}
}

/* Load pdet related Rfseq on reset */
static void
wlc_phy_set_pdet_on_reset_acphy(phy_info_t *pi)
{
	uint8 core, pdet_range_id, subband_idx;
	uint16 offset, tmp_val, val_av, val_vmid;
	uint8 chan_freq_range;
	uint8 av[3];
	uint8 vmid[3];
	uint8 avvmid_set[16][5][6] = {{{2, 1, 2,   107, 150, 110},  /* pdet_id = 0 */
				       {2, 2, 1,   157, 153, 160},
				       {2, 2, 1,   157, 153, 161},
				       {2, 2, 0,   157, 153, 186},
				       {2, 2, 0,   157, 153, 187}},
				      {{1, 0, 1,   159, 174, 161},  /* pdet_id = 1 */
				       {1, 0, 1,   160, 185, 156},
				       {1, 0, 1,   163, 185, 162},
				       {1, 0, 1,   169, 187, 167},
				       {1, 0, 1,   152, 188, 160}},
				      {{1, 1, 1,   159, 166, 166},  /* pdet_id = 2 */
				       {2, 2, 4,   140, 151, 100},
				       {2, 2, 3,   143, 153, 116},
				       {2, 2, 2,   143, 153, 140},
				       {2, 2, 2,   145, 160, 154}},
				      {{1, 1, 2,   130, 131, 106},  /* pdet_id = 3 */
				       {1, 1, 2,   130, 131, 106},
				       {1, 1, 2,   128, 127, 97},
				       {0, 1, 3,   159, 137, 75},
				       {0, 0, 3,   164, 162, 76}},
				      {{1, 1, 1,   156, 160, 158},  /* pdet_id = 4 */
				       {1, 1, 1,   156, 160, 158},
				       {1, 1, 1,   156, 160, 158},
				       {1, 1, 1,   156, 160, 158},
				       {1, 1, 1,   156, 160, 158}},
				      {{2, 2, 2,   104, 108, 106},  /* pdet_id = 5 */
				       {2, 2, 2,   104, 108, 106},
				       {2, 2, 2,   104, 108, 106},
				       {2, 2, 2,   104, 108, 106},
				       {2, 2, 2,   104, 108, 106}},
				      {{2, 0, 2,   102, 170, 104},  /* pdet_id = 6 */
				       {3, 4, 3,    82, 102,  82},
				       {1, 3, 1,   134, 122, 136},
				       {1, 3, 1,   134, 124, 136},
				       {2, 3, 2,   104, 122, 108}},
				      {{0, 0, 0,   180, 180, 180},  /* pdet_id = 7 */
				       {0, 0, 0,   180, 180, 180},
				       {0, 0, 0,   180, 180, 180},
				       {0, 0, 0,   180, 180, 180},
				       {0, 0, 0,   180, 180, 180}},
				      {{2, 1, 2,   102, 138, 104},  /* pdet_id = 8 */
				       {3, 5, 3,    82, 100,  82},
				       {1, 4, 1,   134, 116, 136},
				       {1, 3, 1,   134, 136, 136},
				       {2, 3, 2,   104, 136, 108}},
				      {{3, 2, 3,    90, 106,  86},  /* pdet_id = 9 */
				       {3, 1, 3,    90, 158,  90},
				       {2, 1, 2,   114, 158, 112},
				       {2, 1, 1,   116, 158, 142},
				       {2, 1, 1,   116, 158, 142}},
				      {{2, 2, 2,   152, 156, 156},  /* pdet_id = 10 */
				       {2, 2, 2,   152, 156, 156},
				       {2, 2, 2,   152, 156, 156},
				       {2, 2, 2,   152, 156, 156},
				       {2, 2, 2,   152, 156, 156}},
				      {{1, 1, 1,   134, 134, 134},  /* pdet_id = 11 */
				       {1, 1, 1,   136, 136, 136},
				       {1, 1, 1,   136, 136, 136},
				       {1, 1, 1,   136, 136, 136},
				       {1, 1, 1,   136, 136, 136}},
				      {{3, 3, 3,    90,  92,  86},  /* pdet_id = 12 */
				       {3, 3, 3,    90,  89,  90},
				       {2, 3, 2,   114,  89, 112},
				       {2, 2, 1,   116, 111, 142},
				       {2, 3, 1,   116,  89, 142}},
				      {{2, 2, 2,   112, 114, 112},  /* pdet_id = 13 */
				       {2, 2, 2,   114, 114, 114},
				       {2, 2, 2,   114, 114, 114},
				       {2, 2, 2,   113, 114, 112},
				       {2, 2, 2,   113, 114, 112}}};

	uint8 avvmid_set1[16][5][2] = {
		{{1, 154}, {0, 168}, {0, 168}, {0, 168}, {0, 168}},  /* pdet_id = 0 */
		{{1, 150}, {1, 150}, {1, 150}, {1, 150}, {1, 150}},  /* pdet_id = 1 */
		{{6,  76}, {1, 160}, {6,  76}, {6,  76}, {6,  76}},  /* pdet_id = 2 */
		{{1, 156}, {1, 152}, {1, 152}, {1, 152}, {1, 152}}   /* pdet_id = 3 */
	};

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		pdet_range_id = pi->u.pi_acphy->srom_2g_pdrange_id;
	} else {
		pdet_range_id = pi->u.pi_acphy->srom_5g_pdrange_id;
	}

	chan_freq_range = wlc_phy_get_chan_freq_range_acphy(pi, 0);

	switch (chan_freq_range) {
	case WL_CHAN_FREQ_RANGE_2G:
		subband_idx = 0;
		break;
	case WL_CHAN_FREQ_RANGE_5G_BAND0:
		subband_idx = 1;
		break;
	case WL_CHAN_FREQ_RANGE_5G_BAND1:
		subband_idx = 2;
		break;
	case WL_CHAN_FREQ_RANGE_5G_BAND2:
		subband_idx = 3;
		break;
	case WL_CHAN_FREQ_RANGE_5G_BAND3:
		subband_idx = 4;
		break;
	default:
		PHY_ERROR(("--------- chan_freq_range error for AvVmid ---------"));
		subband_idx = 0;
		break;
	}

	FOREACH_CORE(pi, core) {
		if (ACREV_IS(pi->pubpi.phy_rev, 0)||
		       ACREV_IS(pi->pubpi.phy_rev, 1)) {
			av[core] = avvmid_set[pdet_range_id][subband_idx][core];
			vmid[core] = avvmid_set[pdet_range_id][subband_idx][core+3];

		} else if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
			if (core == 0) {
				av[core] = avvmid_set1[pdet_range_id][subband_idx][core];
				vmid[core] = avvmid_set1[pdet_range_id][subband_idx][core+1];
			}
		}
	}

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		if (ACREV_IS(pi->pubpi.phy_rev, 0) ||
		    ACREV_IS(pi->pubpi.phy_rev, 1) ||
		    (ACREV1X1_IS(pi->pubpi.phy_rev) && (core == 0))) {
			offset = 0x3c0 + 0xd + core*0x10;
			wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ,
			                         1, offset, 16, &tmp_val);
			val_av = (tmp_val & 0x1ff8) | (av[core]&0x7);
			val_vmid = (val_av & 0x7) | ((vmid[core]&0x3ff)<<3);
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ,
			                          1, offset, 16, &val_vmid);
		}
	}
}

/* Initialize chip regs(RW) that get reset with phy_reset */
static void
wlc_phy_set_reg_on_reset_acphy(phy_info_t *pi)
{
	uint8 core;
	uint16 val;

	/* IQ Swap (revert swap happening in the radio) */
	if (!ISSIM_ENAB(pi->sh->sih)) {
		val = (7 << ACPHY_RxFeCtrl1_swap_iq0_SHIFT);
		phy_reg_or(pi, ACPHY_RxFeCtrl1, val);
	}

	/* kimmer - add change from 0x667 to x668 very slight improvement */
	phy_reg_write(pi, ACPHY_DsssStep, 0x668);

	/* Avoid underflow trigger for loopback Farrow */
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, en_txrx_sdfeFifoReset, 1);

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, rxfe_bilge_cnt, 0);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 1);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 0);
	} else {
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, rxfe_bilge_cnt, 4);
	}


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		/* Write 0x0 to RfseqMode to turn off both CoreActv_override */
		phy_reg_write(pi, ACPHY_RfseqMode, 0);
	}

	/* Turn on TxCRS extension.
	 * (Need to eventually make the 1.0 be native TxCRSOff (1.0us))
	 */
	phy_reg_write(pi, ACPHY_dot11acphycrsTxExtension, 200);

	/* Currently PA turns on 1us before first DAC sample. Decrease that gap to 0.5us */

	if (!ACREV1X1_IS(pi->pubpi.phy_rev)) {
		phy_reg_write(pi, ACPHY_TxRealFrameDelay, 146);
	}

	phy_reg_write(pi, ACPHY_payloadcrsExtensionLen, 80);

	/* This number combined with MAC RIFS results in 2.0us RIFS air time */
	phy_reg_write(pi, ACPHY_TxRifsFrameDelay, 48);

	si_core_cflags(pi->sh->sih, SICF_MPCLKE, SICF_MPCLKE);

	/* allow TSSI loopback path to turn off */
	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		MOD_PHYREG(pi, ACPHY_AfePuCtrl, tssiSleepEn, 1);
	} else {
		MOD_PHYREG(pi, ACPHY_AfePuCtrl, tssiSleepEn, 0);
	}

	/* In event of high power spurs/interference that causes crs-glitches,
	   stay in WAIT_ENERGY_DROP for 1 clk20 instead of default 1 ms.
	   This way, we get back to CARRIER_SEARCH quickly and will less likely to miss
	   actual packets. PS: this is actually one settings for ACI
	*/
	/* phy_reg_write(pi, ACPHY_energydroptimeoutLen, 0x2); */

	/* Upon Reception of a High Tone/Tx Spur, the default 40MHz MF settings causes ton of
	   glitches. Set the MF settings similar to 20MHz uniformly. Provides Robustness for
	   tones (on-chip, on-platform, accidential loft coming from other devices)
	*/
	MOD_PHYREG(pi, ACPHY_crsControll, mfLessAve, 0);
	MOD_PHYREG(pi, ACPHY_crsThreshold2l, peakThresh, 85);
	MOD_PHYREG(pi, ACPHY_crsControlu, mfLessAve, 0);
	MOD_PHYREG(pi, ACPHY_crsThreshold2u, peakThresh, 85);
	MOD_PHYREG(pi, ACPHY_crsControllSub1, mfLessAve, 0);
	MOD_PHYREG(pi, ACPHY_crsThreshold2lSub1, peakThresh, 85);
	MOD_PHYREG(pi, ACPHY_crsControluSub1, mfLessAve, 0);
	MOD_PHYREG(pi, ACPHY_crsThreshold2uSub1, peakThresh, 85);

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		uint8 txevmtbl[40] = {0x13, 0xE, 0x11, 0x14, 0x17, 0x1A, 0x1D, 0x20, 0x13,
			0xE, 0x11, 0x14, 0x17, 0x1A, 0x1D, 0x20, 0x22, 0x24, 0x13, 0xE,
			0x11, 0x14, 0x17, 0x1A, 0x1D, 0x20, 0x22, 0x24, 0x13, 0xE, 0x11,
			0x14, 0x17, 0x1A, 0x1D, 0x20, 0x22, 0x24, 0x0, 0x0};

		MOD_PHYREG(pi, ACPHY_crsThreshold2l, peakThresh, 77);
		MOD_PHYREG(pi, ACPHY_crsThreshold2u, peakThresh, 77);
		MOD_PHYREG(pi, ACPHY_crsThreshold2lSub1, peakThresh, 77);
		MOD_PHYREG(pi, ACPHY_crsThreshold2uSub1, peakThresh, 77);

		MOD_PHYREG(pi, ACPHY_crsacidetectThreshl, acidetectThresh, 0x80);
		MOD_PHYREG(pi, ACPHY_crsacidetectThreshlSub1, acidetectThresh, 0x80);
		MOD_PHYREG(pi, ACPHY_crsacidetectThreshu, acidetectThresh, 0x80);
		MOD_PHYREG(pi, ACPHY_crsacidetectThreshuSub1, acidetectThresh, 0x80);
		MOD_PHYREG(pi, ACPHY_initcarrierDetLen, initcarrierDetLen, 0x40);
		MOD_PHYREG(pi, ACPHY_clip1carrierDetLen, clip1carrierDetLen, 0x5c);
		MOD_PHYREG(pi, ACPHY_clip2carrierDetLen, clip2carrierDetLen, 0x48);
		MOD_PHYREG(pi, ACPHY_clip_detect_normpwr_var_mux, use_norm_var_for_clip_detect, 0);
		MOD_PHYREG(pi, ACPHY_norm_var_hyst_th_pt8us, cck_gain_pt8us_en, 1);
		MOD_PHYREG(pi, ACPHY_bOverAGParams, bOverAGlog2RhoSqrth, 0);
		MOD_PHYREG(pi, ACPHY_CRSMiscellaneousParam, bphy_pre_det_en, 0);
		MOD_PHYREG(pi, ACPHY_CRSMiscellaneousParam, b_over_ag_falsedet_en, 1);
		MOD_PHYREG(pi, ACPHY_CRSMiscellaneousParam, mf_crs_initgain_only, 1);
		MOD_PHYREG(pi, ACPHY_RxControl, bphyacidetEn, 1);

		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_TXEVMTBL, 40, 0, 8, txevmtbl);

		phy_reg_write(pi, ACPHY_RfseqCoreActv2059, 0x1111);
		phy_reg_write(pi, ACPHY_HTSigTones, 0x9ee1);
		MOD_PHYREG(pi, ACPHY_FSTRCtrl, fineStrSgiVldCntVal,  0xb);
		MOD_PHYREG(pi, ACPHY_FSTRCtrl, fineStrVldCntVal, 0xa);

		phy_reg_mod(pi, ACPHY_musigb1, ACPHY_musigb1_mu_sigbmcs6_MASK,
		        0x7<<ACPHY_musigb1_mu_sigbmcs6_SHIFT);
		phy_reg_mod(pi, ACPHY_musigb1, ACPHY_musigb1_mu_sigbmcs5_MASK,
		        0x7<<ACPHY_musigb1_mu_sigbmcs5_SHIFT);
		phy_reg_mod(pi, ACPHY_musigb1, ACPHY_musigb1_mu_sigbmcs4_MASK,
		        0x7<<ACPHY_musigb1_mu_sigbmcs4_SHIFT);
		phy_reg_mod(pi, ACPHY_musigb0, ACPHY_musigb0_mu_sigbmcs3_MASK,
		        0x7<<ACPHY_musigb0_mu_sigbmcs3_SHIFT);
		phy_reg_mod(pi, ACPHY_musigb0, ACPHY_musigb0_mu_sigbmcs2_MASK,
		        0x7<<ACPHY_musigb0_mu_sigbmcs2_SHIFT);
		phy_reg_mod(pi, ACPHY_musigb0, ACPHY_musigb0_mu_sigbmcs1_MASK,
		        0x3<<ACPHY_musigb0_mu_sigbmcs1_SHIFT);
		phy_reg_mod(pi, ACPHY_musigb0, ACPHY_musigb0_mu_sigbmcs0_MASK,
		        0x2<<ACPHY_musigb0_mu_sigbmcs0_SHIFT);

	}
	phy_reg_write(pi, ACPHY_RfseqMode, 0);

	/* Disable Viterbi cache-hit low power featre for 4360
	 * since it is hard to meet 320 MHz timing
	 */

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		MOD_PHYREG(pi, ACPHY_ViterbiControl0, CacheHitEn, 1);
	} else {
		MOD_PHYREG(pi, ACPHY_ViterbiControl0, CacheHitEn, 0);
	}

	/* Reset pktproc state and force RESET2RX sequence */
	wlc_phy_resetcca_acphy(pi);

	/* Try to fix the Tx2RX turnaround issue */
	if (0) {
		MOD_PHYREG(pi, ACPHY_RxFeStatus, sdfeFifoResetCntVal, 0xF);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, resetsdFeInNonActvSt, 0x1);
	}

	/* Make TSSI to select Q-rail */
	MOD_PHYREG(pi, ACPHY_TSSIMode, tssiADCSel, 1);

	/* Increase this by 10 ticks helps in getting rid of humps at high SNR, single core runs */
	phy_reg_write(pi, ACPHY_defer_setClip1_CtrLen, 30);

	MOD_PHYREG(pi, ACPHY_HTSigTones, ldpc_proprietary_mcs_vht, 0);

	MOD_PHYREG(pi, ACPHY_HTSigTones, support_gf, 0);

	/* JIRA-CRDOT11ACPHY-273: SIG errror check For number of VHT symbols calculated */
	MOD_PHYREG(pi, ACPHY_partialAIDCountDown, check_vht_siga_length, 1);

	FOREACH_CORE(pi, core) {
		MOD_PHYREGCE(pi, forceFront, core, freqCor, 1);
		MOD_PHYREGCE(pi, forceFront, core, freqEst, 1);
	}

	phy_reg_write(pi, ACPHY_pktgainSettleLen, 48);


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		phy_reg_write(pi, ACPHY_CoreConfig, 0x29);
		phy_reg_write(pi, ACPHY_RfseqMode, 0x111);
		wlc_phy_set_lowpwr_phy_reg(pi);
	}

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		/* 4335:tkip macdelay & mac holdoff */
		phy_reg_write(pi, ACPHY_TxMacIfHoldOff, TXMAC_IFHOLDOFF_DEFAULT);
		phy_reg_write(pi, ACPHY_TxMacDelay, TXMAC_MACDELAY_DEFAULT);
	}
}


/* Initialize chip tbls(reg-based) that get reset with phy_reset */
static void
wlc_phy_set_tbl_on_reset_acphy(phy_info_t *pi)
{
	uint8 stall_val;
	phy_info_acphy_t *pi_ac;
	uint16 adc_war_val = 0x20, pablowup_war_val = 120, lpf_war_val;
	uint8 core;
	uint16 rfseq_bundle_tssisleep48[3];

	bool ext_pa_ana_2g =  ((BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2) &
		BFL2_SROM11_ANAPACTRL_2G) != 0);
	bool ext_pa_ana_5g =  ((BOARDFLAGS2(GENERIC_PHY_INFO(pi)->boardflags2) &
		BFL2_SROM11_ANAPACTRL_5G) != 0);

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	pi_ac = pi->u.pi_acphy;

	/* Load settings related to antswctrl */
	wlc_phy_set_regtbl_on_femctrl(pi);

	/* Load Pdet related settings */
	wlc_phy_set_pdet_on_reset_acphy(pi);

	/* Quickturn only init */
	if (ISSIM_ENAB(pi->sh->sih)) {
		uint8 core_idx;
		uint16 val = 64;
		for (core_idx = 0; core_idx < pi->pubpi.phy_corenum; core_idx++) {
			wlc_phy_set_tx_bbmult_acphy(pi, &val, core_idx);
		}

		/* dummy call to satisfy compiler */
		wlc_phy_get_tx_bbmult_acphy(pi, &val, 0);

		/* on QT: force the init gain to allow noise_var not limiting 256QAM performance */
		ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeA, 0x16a);
		ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x24);

		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
				0xf9 + core, 16, &qt_rfseq_val1[core]);
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
				0xf6 + core, 16, &qt_rfseq_val2[core]);
		}
	}

	/* Update gmult, dacbuf after radio init */
	wlc_phy_set_analog_tx_lpf(pi, 0x1ff, -1, -1, -1, pi_ac->rccal_gmult,
	                          pi_ac->rccal_gmult_rc, -1);
	wlc_phy_set_analog_rx_lpf(pi, 0x7, -1, -1, -1, pi_ac->rccal_gmult,
	                          pi_ac->rccal_gmult_rc, -1);
	wlc_phy_set_tx_afe_dacbuf_cap(pi, 0x1ff, pi_ac->rccal_dacbuf, -1, -1);

	/* reset2rx : Take away the bundle commands */
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 16, 0x20, 16, rfseq_reset2rx_cmd);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 16, 0x90, 16, rfseq_reset2rx_dly);

	/* during updateGainL make sure the lpf/tia hpc corner is set properly to optimum setting */
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 2, 0x121, 16, rfseq_updl_lpf_hpc_ml);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 2, 0x131, 16, rfseq_updl_lpf_hpc_ml);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 2, 0x124, 16, rfseq_updl_tia_hpc_ml);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 2, 0x137, 16, rfseq_updl_tia_hpc_ml);

	/* tx2rx/rx2tx: Remove SELECT_RFPLL_AFE_CLKDIV/RESUME as we are not in boost mode */
	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 16, 0x00,
			16, rfseq_rx2tx_cmd_withtssisleep);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 16, 0x70,
			16, rfseq_rx2tx_dly_withtssisleep);
		MOD_PHYREG(pi, ACPHY_RfBiasControl, tssi_sleep_bg_pulse_val, 1);
		MOD_PHYREG(pi, ACPHY_AfePuCtrl, tssiSleepEn, 1);
		rfseq_bundle_tssisleep48[0] = 0x0000;
		rfseq_bundle_tssisleep48[1] = 0x20;
		rfseq_bundle_tssisleep48[2] = 0x0;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQBUNDLE, 1, 0, 48,
			rfseq_bundle_tssisleep48);
	} else {
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 16, 0x00,
			16, rfseq_rx2tx_cmd);
	}
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 16, 0x10, 16, rfseq_tx2rx_cmd);

	if (1) {
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x3c6, 16, &adc_war_val);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x3c7, 16, &adc_war_val);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x3d6, 16, &adc_war_val);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x3d7, 16, &adc_war_val);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x3e6, 16, &adc_war_val);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x3e7, 16, &adc_war_val);
	}

	if ((CHSPEC_IS2G(pi->radio_chanspec) && ext_pa_ana_2g) ||
	    (CHSPEC_IS5G(pi->radio_chanspec) && ext_pa_ana_5g)) {
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x80, 16, &pablowup_war_val);
	}


	if (!ACREV1X1_IS(pi->pubpi.phy_rev)) {
		/* load the txv for spatial expansion */
		acphy_load_txv_for_spexp(pi);
	}

	if (ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
		wlc_phy_table_read_acphy(pi,  ACPHY_TBL_ID_RFSEQ, 1, 0x143, 16, &lpf_war_val);
		lpf_war_val = (lpf_war_val & 0xfe00) | 0x16b;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x143, 16, &lpf_war_val);

		wlc_phy_table_read_acphy(pi,  ACPHY_TBL_ID_RFSEQ, 1, 0x144, 16, &lpf_war_val);
		lpf_war_val = (lpf_war_val & 0xfe00) | 0x16b;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x144, 16, &lpf_war_val);

		wlc_phy_table_read_acphy(pi,  ACPHY_TBL_ID_RFSEQ, 1, 0x146, 16, &lpf_war_val);
		lpf_war_val = (lpf_war_val & 0xfe00) | 0x16b;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x146, 16, &lpf_war_val);

		wlc_phy_table_read_acphy(pi,  ACPHY_TBL_ID_RFSEQ, 1, 0x147, 16, &lpf_war_val);
		lpf_war_val = (lpf_war_val & 0xfe00) | 0x16b;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x147, 16, &lpf_war_val);

		wlc_phy_table_read_acphy(pi,  ACPHY_TBL_ID_RFSEQ, 1, 0x149, 16, &lpf_war_val);
		lpf_war_val = (lpf_war_val & 0xfe00) | 0x1b3;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x149, 16, &lpf_war_val);


	}

	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_rxgainctrl_set_gaintbls_acphy(phy_info_t *pi, bool init, bool band_change,
                                             bool bw_change)
{
	uint8 elna[2];

	/* 2g settings */
	uint8 mix_2g[]  = {0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3, 0x3};
	uint8 mixbits_2g[] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};

	/* 5g settings */
	uint8 mix5g_elna[]  = {0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7, 0x7};
	uint8 mixbits5g_elna[] = {2, 2, 2, 2, 2, 2, 2, 2, 2, 2};
	uint8 mix5g_ilna[]  = {16, 16, 16, 16, 16, 16, 16, 16, 16, 16};
	uint8 mixbits5g_ilna[] = {5, 5, 5, 5, 5, 5, 5, 5, 5, 5};
	uint8 mix_5g[10], mixbits_5g[10];

	/* lna1 GainLimit */
	uint8 stall_val, core, i;
	uint16 save_forclks, gain_tblid, gainbits_tblid;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	uint phyrev = pi->pubpi.phy_rev;

	/* Mixer tables based on elna/ilna */
	if (pi_ac->srom.elna5g_present) {
		memcpy(mix_5g, mix5g_elna,
		       sizeof(uint8)*pi_ac->rxgainctrl_stage_len[3]);
		memcpy(mixbits_5g, mixbits5g_elna,
		       sizeof(uint8)*pi_ac->rxgainctrl_stage_len[3]);
	} else {
		memcpy(mix_5g, mix5g_ilna,
		       sizeof(uint8)*pi_ac->rxgainctrl_stage_len[3]);
		memcpy(mixbits_5g, mixbits5g_ilna,
		       sizeof(uint8)*pi_ac->rxgainctrl_stage_len[3]);
	}

	/* Disable stall before writing tables */
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	/* If reciever is in active demod, it will NOT update the Gain tables */
	save_forclks = phy_reg_read(pi, ACPHY_fineRxclockgatecontrol);
	MOD_PHYREG(pi, ACPHY_fineRxclockgatecontrol, forcegaingatedClksOn, 1);

	/* LNA1/2 (always do this, as the previous channel could have been in ACI mitigation) */
	for (i = 1; i <= 2; i++) {
		wlc_phy_upd_lna1_lna2_gaintbls_acphy(pi, i);
		wlc_phy_upd_lna1_lna2_gainlimittbls_acphy(pi, i);
	}

	FOREACH_CORE(pi, core) {
		if (core == 0) {
			gain_tblid =  ACPHY_TBL_ID_GAIN0;
			gainbits_tblid =  ACPHY_TBL_ID_GAINBITS0;
		} else if (core == 1) {
			gain_tblid =  ACPHY_TBL_ID_GAIN1;
			gainbits_tblid =  ACPHY_TBL_ID_GAINBITS1;
		} else {
			gain_tblid =  ACPHY_TBL_ID_GAIN2;
			gainbits_tblid =  ACPHY_TBL_ID_GAINBITS2;
		}

		/* FEM - elna, trloss (from srom) */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			elna[0] = elna[1] = pi_ac->srom.femrx_2g[core].elna;
			pi_ac->fem_rxgains[core].elna = elna[0];
			pi_ac->fem_rxgains[core].trloss = pi_ac->srom.femrx_2g[core].trloss;
			pi_ac->fem_rxgains[core].elna_bypass_tr =
			        pi_ac->srom.femrx_2g[core].elna_bypass_tr;
		} else if (CHSPEC_CHANNEL(pi->radio_chanspec) < 100) {
			elna[0] = elna[1] = pi_ac->srom.femrx_5g[core].elna;
			pi_ac->fem_rxgains[core].elna = elna[0];
			pi_ac->fem_rxgains[core].trloss = pi_ac->srom.femrx_5g[core].trloss;
			pi_ac->fem_rxgains[core].elna_bypass_tr =
			        pi_ac->srom.femrx_5g[core].elna_bypass_tr;
		} else {
			elna[0] = elna[1] = pi_ac->srom.femrx_5gh[core].elna;
			pi_ac->fem_rxgains[core].elna = elna[0];
			pi_ac->fem_rxgains[core].trloss = pi_ac->srom.femrx_5gh[core].trloss;
			pi_ac->fem_rxgains[core].elna_bypass_tr =
			        pi_ac->srom.femrx_5gh[core].elna_bypass_tr;
		}

		switch (core) {
			case 0 :
				if ((phyrev == 0) && (core == 0)) {
					MOD_PHYREG(pi, ACPHY_TRLossValue,
						freqGainTLoss, pi_ac->fem_rxgains[0].trloss);
				} else if (phyrev > 0) {
					MOD_PHYREG(pi, ACPHY_Core0_TRLossValue,
						freqGainTLoss0, pi_ac->fem_rxgains[0].trloss);
				}
				break;
			case 1:
				if (phyrev > 0) {
					MOD_PHYREG(pi, ACPHY_Core1_TRLossValue,
						freqGainTLoss1, pi_ac->fem_rxgains[1].trloss);
				}
				break;
			case 2:
				if (phyrev > 0) {
					MOD_PHYREG(pi, ACPHY_Core2_TRLossValue,
						freqGainTLoss2, pi_ac->fem_rxgains[2].trloss);
				}
				break;
			default:
				break;
		}
		wlc_phy_table_write_acphy(pi, gain_tblid, 2, 0, 8, elna);
		memcpy(pi_ac->rxgainctrl_params[core].gaintbl[0],
		       elna, sizeof(uint8)*pi_ac->rxgainctrl_stage_len[0]);

		/* MIX, LPF */
		if (init || band_change) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				/* Use lna2_gm_sz = 2 (for ACI), mix/tia_gm_sz = 1 */
				ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreLowPwr0, 0x18);
				ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideLowPwrCfg0, 0xc);

				wlc_phy_table_write_acphy(pi, gain_tblid, 10, 32, 8, mix_2g);
				wlc_phy_table_write_acphy(pi, gainbits_tblid, 10, 32, 8,
				                          mixbits_2g);

				/* copying values into gaintbl arrays to avoid reading from table */
				memcpy(pi_ac->rxgainctrl_params[core].gaintbl[3], mix_2g,
				       sizeof(uint8)*pi_ac->rxgainctrl_stage_len[3]);
			} else {
				/* Use lna2_gm_sz = 3, mix/tia_gm_sz = 2 */
				ACPHYREG_BCAST(pi, ACPHY_RfctrlCoreLowPwr0, 0x2c);
				ACPHYREG_BCAST(pi, ACPHY_RfctrlOverrideLowPwrCfg0, 0xc);

				wlc_phy_table_write_acphy(pi, gain_tblid, 10, 32, 8, mix_5g);
				wlc_phy_table_write_acphy(pi, gainbits_tblid, 10, 32, 8,
				                          mixbits_5g);

				/* copying values into gaintbl arrays to avoid reading from table */
				memcpy(pi_ac->rxgainctrl_params[core].gaintbl[3], mix_5g,
				       sizeof(uint8)*pi_ac->rxgainctrl_stage_len[3]);
			}
		}

		if (init) {
			/* Store gainctrl info (to be used for Auto-Gainctrl)
			 * lna1,2 taken care in wlc_phy_upd_lna1_lna2_gaintbls_acphy()
			 */
			wlc_phy_table_read_acphy(pi, gainbits_tblid, 1, 0, 8,
			                         pi_ac->rxgainctrl_params[core].gainbitstbl[0]);
			wlc_phy_table_read_acphy(pi, gainbits_tblid, 10, 32, 8,
			                         pi_ac->rxgainctrl_params[core].gainbitstbl[3]);
			wlc_phy_table_read_acphy(pi, gain_tblid, 8, 96, 8,
			                         pi_ac->rxgainctrl_params[core].gaintbl[4]);
			wlc_phy_table_read_acphy(pi, gain_tblid, 8, 112, 8,
			                         pi_ac->rxgainctrl_params[core].gaintbl[5]);
			wlc_phy_table_read_acphy(pi, gainbits_tblid, 8, 96, 8,
			                         pi_ac->rxgainctrl_params[core].gainbitstbl[4]);
			wlc_phy_table_read_acphy(pi, gainbits_tblid, 8, 112, 8,
			                         pi_ac->rxgainctrl_params[core].gainbitstbl[5]);
		}
	}

	/* Restore */
	phy_reg_write(pi, ACPHY_fineRxclockgatecontrol, save_forclks);
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static uint16 *
wlc_phy_get_tx_pwrctrl_tbl_2069(phy_info_t *pi, uint8 is_ipa)
{
	uint16 *tx_pwrctrl_tbl = NULL;

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
#ifndef ACPHY_1X1_ONLY
		tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev0;
#endif
		/* In event of high power spurs/interference that causes crs-glitches,
		   stay in WAIT_ENERGY_DROP for 1 clk20 instead of default 1 ms.
		   This way, we get back to CARRIER_SEARCH quickly and will less likely to miss
		   actual packets. PS: this is actually one settings for ACI
		*/
		phy_reg_write(pi, ACPHY_energydroptimeoutLen, 0x2);


		/* Fine timing mod to have more overlap(~10dB) between low and high SNR regimes */
		MOD_PHYREG(pi, ACPHY_FSTRMetricTh, hiPwr_min_metric_th, 0xf);

		if (is_ipa == 1) {
#ifdef ACPHY_1X1_ONLY
			ASSERT((RADIOREV(pi->pubpi.radiorev) == 16) ||
				(RADIOREV(pi->pubpi.radiorev) == 17) ||
				(RADIOREV(pi->pubpi.radiorev) == 23) ||
				(RADIOREV(pi->pubpi.radiorev) == 24));
#endif
			switch (RADIOREV(pi->pubpi.radiorev)) {
			case 16:
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev16;
				break;
			case 17:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev17;
				break;
#ifndef ACPHY_1X1_ONLY
			case 18:
			case 24:
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev18;
				break;
			default:
				tx_pwrctrl_tbl = acphy_txgain_ipa_2g_2069rev0;
				break;
#endif
			}
		} else {
#ifdef ACPHY_1X1_ONLY
			ASSERT((RADIOREV(pi->pubpi.radiorev) == 17) ||
				(RADIOREV(pi->pubpi.radiorev) == 18) ||
				(RADIOREV(pi->pubpi.radiorev) == 23) ||
				(RADIOREV(pi->pubpi.radiorev) == 24));
#endif
			switch (RADIOREV(pi->pubpi.radiorev)) {
			case 17:
			case 23:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev17;
				break;
			case 18:
			case 24:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev18;
				break;
#ifndef ACPHY_1X1_ONLY
			case 4:
				switch (pi->u.pi_acphy->srom.txgaintbl_id) {
				case 0:
					tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev4;
					break;
				case 1:
					tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev4_id1;
					break;
				default:
					tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev4;
					break;
				}
				break;
			case 16:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev16;
				break;
			default:
				tx_pwrctrl_tbl = acphy_txgain_epa_2g_2069rev0;
				break;
#endif /* ACPHY_1x1_ONLY */
			}
		}
	} else {
		phy_reg_write(pi, ACPHY_energydroptimeoutLen, 0x9c40);
		/* Fine timing mod to have more overlap(~10dB) between low and high SNR regimes */
		/* change to 0x8 to prevent the radar to trigger the fine timing */
		MOD_PHYREG(pi, ACPHY_FSTRMetricTh, hiPwr_min_metric_th, 0x8);

		if (is_ipa == 1) {
#ifdef ACPHY_1X1_ONLY
			ASSERT(RADIOREV(pi->pubpi.radiorev) == 17);
			tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev17;
#else
			switch (RADIOREV(pi->pubpi.radiorev)) {
			case 17:
			case 23:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev17;
				break;
			case 18:
			case 24:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev18;
				break;
			case 16:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev16;
				break;
			default:
				tx_pwrctrl_tbl = acphy_txgain_ipa_5g_2069rev0;
				break;
			}
#endif /* ACPHY_1X1_ONLY */
		} else {
#ifdef ACPHY_1X1_ONLY
			ASSERT((RADIOREV(pi->pubpi.radiorev) == 17) ||
				(RADIOREV(pi->pubpi.radiorev) == 18) ||
				(RADIOREV(pi->pubpi.radiorev) == 23));
#endif
			switch (RADIOREV(pi->pubpi.radiorev)) {
			case 17:
			case 23:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev17;
				break;
			case 18:
			case 24:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev18;
				break;
#ifndef ACPHY_1X1_ONLY
			case 4:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev4;
				break;
			case 16:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev16;
				break;
			default:
				tx_pwrctrl_tbl = acphy_txgain_epa_5g_2069rev0;
				break;
#endif
			}
		}
	}

	return tx_pwrctrl_tbl;
}

static uint16 *
wlc_phy_get_tx_pwrctrl_tbl_20691(phy_info_t *pi, uint8 is_ipa)
{
	uint16 *tx_pwrctrl_tbl = NULL;

#ifndef BCMQT
	/*
	 * Force an assert when not building with QT, so that this function is checked
	 * for validity for the 20691 radio.
	 */
	ASSERT(RADIOID(pi->pubpi.radioid) == BCM2069_ID);
#endif
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		/* In event of high power spurs/interference that causes crs-glitches,
		   stay in WAIT_ENERGY_DROP for 1 clk20 instead of default 1 ms.
		   This way, we get back to CARRIER_SEARCH quickly and will less likely to miss
		   actual packets. PS: this is actually one settings for ACI
		*/
		phy_reg_write(pi, ACPHY_energydroptimeoutLen, 0x2);


		/* Fine timing mod to have more overlap(~10dB) between low and high SNR regimes */
		MOD_PHYREG(pi, ACPHY_FSTRMetricTh, hiPwr_min_metric_th, 0xf);

		if (is_ipa == 1) {
			tx_pwrctrl_tbl = acphy_txgain_ipa_2g_20691rev1;
		} else {
			tx_pwrctrl_tbl = acphy_txgain_epa_2g_20691rev1;
		}
	} else {
		phy_reg_write(pi, ACPHY_energydroptimeoutLen, 0x9c40);
		/* Fine timing mod to have more overlap(~10dB) between low and high SNR regimes */
		/* change to 0x8 to prevent the radar to trigger the fine timing */
		MOD_PHYREG(pi, ACPHY_FSTRMetricTh, hiPwr_min_metric_th, 0x8);

		if (is_ipa == 1) {
			tx_pwrctrl_tbl = acphy_txgain_ipa_5g_20691rev1;
		} else {
			tx_pwrctrl_tbl = acphy_txgain_epa_5g_20691rev1;
		}
	}

	return tx_pwrctrl_tbl;
}

static void
wlc_phy_set_regtbl_on_band_change_acphy(phy_info_t *pi)
{
	uint8 stall_val, is_ipa = 0;
	uint16 *tx_pwrctrl_tbl;
	uint16 bq1_gain_core1 = 0x49;
	uint8 pdet_range_id;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);

	ACPHY_DISABLE_STALL(pi);

	if ((CHSPEC_IS2G(pi->radio_chanspec) && (pi->sh->extpagain2g == 2)) ||
		(CHSPEC_IS5G(pi->radio_chanspec) && (pi->sh->extpagain5g == 2))) {
		is_ipa = 1;
	}

	if (RADIOID(pi->pubpi.radioid) == BCM2069_ID) {
		tx_pwrctrl_tbl = wlc_phy_get_tx_pwrctrl_tbl_2069(pi, is_ipa);
	} else {
		ASSERT(RADIOID(pi->pubpi.radioid) == BCM20691_ID);
		tx_pwrctrl_tbl = wlc_phy_get_tx_pwrctrl_tbl_20691(pi, is_ipa);
	}

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			MOD_PHYREG(pi, ACPHY_fineRxclockgatecontrol, forcedigigaingatedClksOn, 1);
		} else {
			MOD_PHYREG(pi, ACPHY_fineRxclockgatecontrol, forcedigigaingatedClksOn, 0);
		}
	}

	/* Assert if tbl not found */
	ASSERT(tx_pwrctrl_tbl != NULL);

	/* Load tx gain table */
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_GAINCTRLBBMULTLUTS, 128, 0, 48, tx_pwrctrl_tbl);

	if (ACREV_IS(pi->pubpi.phy_rev, 0)) {
		wlc_phy_tx_gm_gain_boost(pi);
	}

	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		pdet_range_id = pi->u.pi_acphy->srom_5g_pdrange_id;
		if (pdet_range_id == 9) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x18e,
			    16, &bq1_gain_core1);
		}
	}

	ACPHY_ENABLE_STALL(pi, stall_val);


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		/* When WLAN is in 5G, WLAN table should control the FEM lines */
		/* and BT should not have any access permissions */
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			/* disable BT Fem control table accesses */
			phy_reg_mod(pi, ACPHY_FemCtrl, ACPHY_FemCtrl_enBtSignalsToFEMLut_MASK,
			        0x0<<ACPHY_FemCtrl_enBtSignalsToFEMLut_SHIFT);
			if (pi->u.pi_acphy->srom.femctrl == 4) {
				if (pi->u.pi_acphy->srom.femctrl_sub == 1) {
					phy_reg_mod(pi, ACPHY_FemCtrl,
					  ACPHY_FemCtrl_femCtrlMask_MASK,
					  0x23c<<ACPHY_FemCtrl_femCtrlMask_SHIFT);
				} else if (pi->u.pi_acphy->srom.femctrl_sub == 2) {
					phy_reg_mod(pi, ACPHY_FemCtrl,
					  ACPHY_FemCtrl_femCtrlMask_MASK,
					  0x297<<ACPHY_FemCtrl_femCtrlMask_SHIFT);
				} else if (pi->u.pi_acphy->srom.femctrl_sub == 3) {
					phy_reg_mod(pi, ACPHY_FemCtrl,
					  ACPHY_FemCtrl_femCtrlMask_MASK,
					  0x058<<ACPHY_FemCtrl_femCtrlMask_SHIFT);
				} else if (pi->u.pi_acphy->srom.femctrl_sub == 4) {
					phy_reg_mod(pi, ACPHY_FemCtrl,
					  ACPHY_FemCtrl_femCtrlMask_MASK,
					  0x058<<ACPHY_FemCtrl_femCtrlMask_SHIFT);
				} else {
					phy_reg_mod(pi, ACPHY_FemCtrl,
					  ACPHY_FemCtrl_femCtrlMask_MASK,
					  0x3ff<<ACPHY_FemCtrl_femCtrlMask_SHIFT);
				}
			} else {
				phy_reg_mod(pi, ACPHY_FemCtrl,
				  ACPHY_FemCtrl_femCtrlMask_MASK,
				  0x3ff<<ACPHY_FemCtrl_femCtrlMask_SHIFT);
			}
		} else { /* When WLAN is in 2G, BT controls should be allowed to go through */
			/* BT should also be able to control FEM Control Table */
			phy_reg_mod(pi, ACPHY_FemCtrl, ACPHY_FemCtrl_enBtSignalsToFEMLut_MASK,
			        0x1<<ACPHY_FemCtrl_enBtSignalsToFEMLut_SHIFT);
			phy_reg_mod(pi, ACPHY_FemCtrl, ACPHY_FemCtrl_femCtrlMask_MASK,
			                0x3ff<<ACPHY_FemCtrl_femCtrlMask_SHIFT);
		}
	}
}


static void
wlc_phy_set_regtbl_on_bw_change_acphy(phy_info_t *pi)
{
	int sp_tx_bw = 0;
	uint8 stall_val, core, nbclip_cnt_4360 = 15;
	uint16 rfseq_bundle_adcrst48[3];
	uint16 rfseq_bundle_adcrst49[3];
	uint16 rfseq_bundle_adcrst50[3];
	uint phyrev = pi->pubpi.phy_rev;

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);


	if (CHSPEC_IS80(pi->radio_chanspec)) {
		/* 80mhz */
		sp_tx_bw = 5;
		nbclip_cnt_4360 = 60;
	} else if (CHSPEC_IS40(pi->radio_chanspec)) {
		/* 40mhz */
		sp_tx_bw = 4;
		nbclip_cnt_4360 = 30;
	} else if (CHSPEC_IS20(pi->radio_chanspec)) {
		/* 20mhz */
		sp_tx_bw = 3;
		nbclip_cnt_4360 = 15;
	} else {
		PHY_ERROR(("%s: No primary channel settings for bw=%d\n",
		           __FUNCTION__, CHSPEC_BW(pi->radio_chanspec)));
	}

	/* reduce NB clip CNT thresholds */

	if (!ACREV1X1_IS(pi->pubpi.phy_rev)) {
		FOREACH_CORE(pi, core) {
			MOD_PHYREGC(pi, FastAgcClipCntTh, core,
			            fastAgcNbClipCntTh, nbclip_cnt_4360);
		}
	}

	wlc_phy_set_analog_tx_lpf(pi, 0x100, -1, sp_tx_bw, sp_tx_bw, -1, -1, -1);
	/* change the barelyclipgainbackoff to 6 for 80Mhz due to some PER issue for 4360A0 CHIP */
	if (phyrev == 0) {
	  if (CHSPEC_IS80(pi->radio_chanspec)) {
	      ACPHYREG_BCAST(pi, ACPHY_Core0computeGainInfo, 0xcc0);
	  } else {
	      ACPHYREG_BCAST(pi, ACPHY_Core0computeGainInfo, 0xc60);
	  }
	}


	/* JIRA (HW11ACRADIO-30) - clamp_en needs to be high for ~1us for clipped pkts (80mhz) */
	if (CHSPEC_IS80(pi->radio_chanspec)) {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_iqadc_clamp_en, 1);
			MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, core, afe_iqadc_clamp_en, 1);}

		rfseq_bundle_adcrst48[2]  = 0;
		rfseq_bundle_adcrst49[2]  = 0;
		rfseq_bundle_adcrst50[2]  = 0;
		if (CHSPEC_IS20(pi->radio_chanspec)) {
			rfseq_bundle_adcrst48[0] = 0xef52;
			rfseq_bundle_adcrst48[1] = 0x94;
			rfseq_bundle_adcrst49[0] = 0xef42;
			rfseq_bundle_adcrst49[1] = 0x84;
			rfseq_bundle_adcrst50[0] = 0xef52;
			rfseq_bundle_adcrst50[1] = 0x84;
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			rfseq_bundle_adcrst48[0] = 0x4f52;
			rfseq_bundle_adcrst48[1] = 0x94;
			rfseq_bundle_adcrst49[0] = 0x4f42;
			rfseq_bundle_adcrst49[1] = 0x84;
			rfseq_bundle_adcrst50[0] = 0x4f52;
			rfseq_bundle_adcrst50[1] = 0x84;
		} else {
			rfseq_bundle_adcrst48[0] = 0x0fd2;
			rfseq_bundle_adcrst48[1] = 0x96;
			rfseq_bundle_adcrst49[0] = 0x0fc2;
			rfseq_bundle_adcrst49[1] = 0x86;
			rfseq_bundle_adcrst50[0] = 0x0fd2;
			rfseq_bundle_adcrst50[1] = 0x86;
		}
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQBUNDLE, 1, 48, 48,
		                          rfseq_bundle_adcrst48);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQBUNDLE, 1, 49, 48,
		                          rfseq_bundle_adcrst49);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQBUNDLE, 1, 50, 48,
		                          rfseq_bundle_adcrst50);

		/* updategainH : issue adc reset for 250ns */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0x30, 16, rf_updh_cmd_adcrst);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0xa0, 16, rf_updh_dly_adcrst);

		/* updategainL : issue adc reset for 250ns */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0x40, 16, rf_updl_cmd_adcrst);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0xb0, 16, rf_updl_dly_adcrst);

		/* updategainU : issue adc reset for 250n */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0x50, 16, rf_updu_cmd_adcrst);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0xc0, 16, rf_updu_dly_adcrst);
	} else {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, core, afe_iqadc_clamp_en, 1);
			/* 4360A0 : SD-ADC was not monotonic for 1st revision, but is fixed now */
			if (ACREV_IS(pi->pubpi.phy_rev, 0)) {
				MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_iqadc_clamp_en, 0);
			} else {
				MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_iqadc_clamp_en, 1);
			}
		}

		/* updategainH : increase clamp_en off delay to 16 */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0x30, 16, rf_updh_cmd_clamp);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0xa0, 16, rf_updh_dly_clamp);

		/* updategainL : increase clamp_en off delay to 16 */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0x40, 16, rf_updl_cmd_clamp);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0xb0, 16, rf_updl_dly_clamp);

		/* updategainU : increase clamp_en off delay to 16 */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0x50, 16, rf_updu_cmd_clamp);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 8, 0xc0, 16, rf_updu_dly_clamp);
	}

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
			if (CHSPEC_IS20(pi->radio_chanspec)) {
				phy_reg_write(pi, ACPHY_nonpaydecodetimeoutlen, 1);
				MOD_PHYREG(pi, ACPHY_timeoutEn, resetCCAontimeout, 1);
				MOD_PHYREG(pi, ACPHY_timeoutEn, nonpaydecodetimeoutEn, 1);
			} else {
				phy_reg_write(pi, ACPHY_nonpaydecodetimeoutlen, 32);
				MOD_PHYREG(pi, ACPHY_timeoutEn, resetCCAontimeout, 0);
				MOD_PHYREG(pi, ACPHY_timeoutEn, nonpaydecodetimeoutEn, 0);
			}
	}

	ACPHY_ENABLE_STALL(pi, stall_val);
}


static void
wlc_phy_set_aci_regs_acphy(phy_info_t *pi)
{
	uint16 aci_th;

	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (CHSPEC_IS80(pi->radio_chanspec)) {
			aci_th = 0x100;
		} else {
			aci_th = 0xbf;	/* 5GHz, 40/20MHz BW */
		}
	} else {
		if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5))
			aci_th = 0x80;
		else
			aci_th = 0xff;	/* 2.4GHz */
	}

	phy_reg_write(pi, ACPHY_crsacidetectThreshl, aci_th);
	phy_reg_write(pi, ACPHY_crsacidetectThreshu, aci_th);
	phy_reg_write(pi, ACPHY_crsacidetectThreshlSub1, aci_th);
	phy_reg_write(pi, ACPHY_crsacidetectThreshuSub1, aci_th);

	if (0) {
		/* CRDOT11ACPHY-280 : enabled bphy aci det is causing hangs */
		if (CHSPEC_IS2G(pi->radio_chanspec) && CHSPEC_IS20(pi->radio_chanspec)) {
			/* Enable bphy ACI Detection HW */
			MOD_PHYREG(pi, ACPHY_RxControl, bphyacidetEn, 1);
			phy_reg_write(pi, ACPHY_bphyaciThresh0, 0);
			phy_reg_write(pi, ACPHY_bphyaciThresh1, 0);
			phy_reg_write(pi, ACPHY_bphyaciThresh2, 0);
			phy_reg_write(pi, ACPHY_bphyaciThresh3, 0x9F);
			phy_reg_write(pi, ACPHY_bphyaciPwrThresh0, 0);
			phy_reg_write(pi, ACPHY_bphyaciPwrThresh1, 0);
			phy_reg_write(pi, ACPHY_bphyaciPwrThresh2, 0);
		}
	}
}

static void
wlc_phy_set_tx_iir_coeffs(phy_info_t *pi, bool cck, uint8 filter_type)
{
	if (cck == FALSE) {
	         /* Default filters */
	} else {
	    if (filter_type == 0) {
	        /* Default filter */
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st0a1, 0x0a94);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st0a2, 0x0373);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st0n, 0x0005);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st1a1, 0x0a93);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st1a2, 0x0298);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st1n, 0x0004);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st2a1, 0x0a52);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st2a2, 0x021d);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st2n, 0x0004);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20finescale, 0x0080);
	    } else if (filter_type == 1) {
	        /* Gaussian  shaping filter */
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st0a1, 0x0b54);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st0a2, 0x0290);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st0n, 0x0004);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st1a1, 0x0a40);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st1a2, 0x0290);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st1n, 0x0005);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st2a1, 0x0a06);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st2a2, 0x0240);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20st2n, 0x0005);
	        phy_reg_write(pi, ACPHY_txfiltbphy20in20finescale, 0x0080);
	    }
	}
}

static void
wlc_phy_set_regtbl_on_chan_change_acphy(phy_info_t *pi, const chan_info_radio2069_t *ci,
                                     const chan_info_radio2069revGE16_t *ciGE16)
{

	/* Setup the Tx/Rx Farrow resampler */
	wlc_phy_farrow_setup_acphy(pi, pi->radio_chanspec);
	if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5)) {
			ASSERT(ciGE16 != NULL);
			phy_reg_write(pi, ACPHY_BW1a, ciGE16->PHY_BW1a);
			phy_reg_write(pi, ACPHY_BW2, ciGE16->PHY_BW2);
			phy_reg_write(pi, ACPHY_BW3, ciGE16->PHY_BW3);
			phy_reg_write(pi, ACPHY_BW4, ciGE16->PHY_BW4);
			phy_reg_write(pi, ACPHY_BW5, ciGE16->PHY_BW5);
			phy_reg_write(pi, ACPHY_BW6, ciGE16->PHY_BW6);
	} else {
			ASSERT(ci != NULL);
			/**** set SFO parameters ****
			 * sfo_chan_center_Ts20 = round([fc-10e6 fc fc+10e6] / 20e6 * 8), fc in Hz
			 *                      = round([$channel-10 $channel $channel+10] * 0.4),
			 *                              $channel in MHz
			 */
			phy_reg_write(pi, ACPHY_BW1a, ci->PHY_BW1a);
			phy_reg_write(pi, ACPHY_BW2, ci->PHY_BW2);
			phy_reg_write(pi, ACPHY_BW3, ci->PHY_BW3);

			/* sfo_chan_center_factor = round(2^17./([fc-10e6 fc fc+10e6]/20e6))
			 * fc in Hz
			 *  = round(2621440./[$channel-10 $channel $channel+10]),
			 *    $channel in MHz
			 */
			phy_reg_write(pi, ACPHY_BW4, ci->PHY_BW4);
			phy_reg_write(pi, ACPHY_BW5, ci->PHY_BW5);
			phy_reg_write(pi, ACPHY_BW6, ci->PHY_BW6);
	}
	/* Set the correct primary channel */
	if (CHSPEC_IS80(pi->radio_chanspec)) {
		/* 80mhz */
		if (CHSPEC_CTL_SB(pi->radio_chanspec) == WL_CHANSPEC_CTL_SB_LL) {
			MOD_PHYREG(pi, ACPHY_ClassifierCtrl2, prim_sel, 0);
		} else if (CHSPEC_CTL_SB(pi->radio_chanspec) == WL_CHANSPEC_CTL_SB_LU) {
			MOD_PHYREG(pi, ACPHY_ClassifierCtrl2, prim_sel, 1);
		} else if (CHSPEC_CTL_SB(pi->radio_chanspec) == WL_CHANSPEC_CTL_SB_UL) {
			MOD_PHYREG(pi, ACPHY_ClassifierCtrl2, prim_sel, 2);
		} else if (CHSPEC_CTL_SB(pi->radio_chanspec) == WL_CHANSPEC_CTL_SB_UU) {
			MOD_PHYREG(pi, ACPHY_ClassifierCtrl2, prim_sel, 3);
		} else {
			PHY_ERROR(("%s: No primary channel settings for CTL_SB=%d\n",
			           __FUNCTION__, CHSPEC_CTL_SB(pi->radio_chanspec)));
		}
	} else if (CHSPEC_IS40(pi->radio_chanspec)) {
		/* 40mhz */
		if (CHSPEC_SB_UPPER(pi->radio_chanspec)) {
			MOD_PHYREG(pi, ACPHY_RxControl, bphy_band_sel, 1);
			MOD_PHYREG(pi, ACPHY_ClassifierCtrl2, prim_sel, 1);
		} else {
			MOD_PHYREG(pi, ACPHY_RxControl, bphy_band_sel, 0);
			MOD_PHYREG(pi, ACPHY_ClassifierCtrl2, prim_sel, 0);
		}
	} else if (CHSPEC_IS20(pi->radio_chanspec)) {
		/* 20mhz */
		MOD_PHYREG(pi, ACPHY_RxControl, bphy_band_sel, 0);
		MOD_PHYREG(pi, ACPHY_ClassifierCtrl2, prim_sel, 0);
	} else {
		PHY_ERROR(("%s: No primary channel settings for bw=%d\n",
		           __FUNCTION__, CHSPEC_BW(pi->radio_chanspec)));
	}


	/* set aci thresholds */
	wlc_phy_set_aci_regs_acphy(pi);

	/* set default crs_min thresholds */
	wlc_phy_set_crs_min_pwr_acphy(pi, 0, 0, 0);

	/* making IIR filter gaussian like for BPHY to improve ACPR */

	if (
		CHIPID(pi->sh->chip) == BCM4335_CHIP_ID) {
		uint8 bphy_testmode_val;

		ASSERT(ciGE16 != NULL);

		/* set RRC filter alpha
		 FiltSel2 is 11 bit which msb, bphyTest's 6th bit is lsb
		 These 2 bits control alpha
		 bits 11 & 6    Resulting filter
		  -----------    ----------------
		      00         alpha=0.35 - default
		      01         alpha=0.75 - alternate
		      10         alpha=0.2  - for use in Japan on channel 14
		      11         no TX filter
		*/
		if (ciGE16->freq == 2484) {
			bphy_testmode_val = (0x3F & READ_PHYREG(pi, ACPHY_bphyTest, testMode));
			MOD_PHYREG(pi, ACPHY_bphyTest, testMode, bphy_testmode_val);
			MOD_PHYREG(pi, ACPHY_bphyTest, FiltSel2, 1);
			/* Load default filter */
			wlc_phy_set_tx_iir_coeffs(pi, 1, 0);
		} else {
			bphy_testmode_val = (0x3F & READ_PHYREG(pi, ACPHY_bphyTest, testMode));
			bphy_testmode_val = bphy_testmode_val |
				((pi->u.pi_acphy->acphy_cck_dig_filt_type & 0x2)  << 5);
			MOD_PHYREG(pi, ACPHY_bphyTest, testMode, bphy_testmode_val);
			MOD_PHYREG(pi, ACPHY_bphyTest, FiltSel2,
				((pi->u.pi_acphy->acphy_cck_dig_filt_type & 0x4) >> 2));
			/* Load filter with Gaussian shaping */
			wlc_phy_set_tx_iir_coeffs(pi, 1,
				(pi->u.pi_acphy->acphy_cck_dig_filt_type & 0x1));
		}
	}


}

static void wlc_phy_table_write_acphy_fectrl_fem5516(phy_info_t *pi, uint32 id, uint32 len,
	uint32 offset, uint32 width)
{
	uint8 fectrl_fem5516[] = {0, 0, 4, 0, 0, 0, 4, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 0, 0, 0, 4, 0, 0, 2, 0, 0, 0, 0, 0, 0};
	wlc_phy_table_write_acphy(pi, id, len, offset, width, fectrl_fem5516);
}

static void
wlc_phy_write_regtbl_fc1(phy_info_t *pi)
{
	wlc_phy_table_write_acphy_fectrl_fem5516(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 0, 8);
	wlc_phy_table_write_acphy_fectrl_fem5516(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8);
	wlc_phy_table_write_acphy_fectrl_fem5516(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 64, 8);
}

static void
wlc_phy_write_regtbl_fc2(phy_info_t *pi, bool femctrl_sub_is_zero)
{
	uint8 fectrl_x29c_c1_sub0[] = {0, 0, 0x50, 0x10, 0, 0, 0x50, 0x10, 0, 0x80, 0,
		0, 0, 0, 0, 0, 0, 0, 6, 2, 0, 0, 6, 2, 0, 1, 0, 0, 0, 0, 0, 0};
	uint8 fectrl_x29c_c1_sub1[] = {0, 0, 0x30, 0x20, 0, 0, 0x30, 0x20, 0, 0x80, 0,
		0, 0, 0, 0, 0, 0x40, 0x40, 0x46, 0x42, 0x40, 0x40, 0x46, 0x42, 0x40, 0x41, 0x40,
		0x40, 0x40, 0x40, 0x40, 0x40};

	wlc_phy_table_write_acphy_fectrl_fem5516(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 0, 8);
	wlc_phy_table_write_acphy_fectrl_fem5516(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 64, 8);

	if (femctrl_sub_is_zero) {
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8,
			fectrl_x29c_c1_sub0);
	} else {
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8,
			fectrl_x29c_c1_sub1);
	}
}

static void
wlc_phy_write_regtbl_fc3_sub0(phy_info_t *pi)
{
	uint8 fectrl_mch5_c0_p200_p400[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		2, 4, 3, 11, 2, 4, 3, 11, 0x02, 0x24, 0x03, 0x2d, 0x02, 0x24, 0x03, 0x2d};
	uint8 fectrl_mch5_c1_p200_p400[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		2, 1, 6, 14, 2, 1, 6, 14, 0x02, 0x21, 0x06, 0x2d, 0x02, 0x21, 0x06, 0x2d};
	uint8 fectrl_mch5_c2_p200_p400[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		4, 1, 6, 14, 4, 1, 6, 14, 0x04, 0x21, 0x06, 0x2b, 0x04, 0x21, 0x06, 0x2b};

	si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
		0xffffff, CCTRL4360_DISCRETE_FEMCTRL_MODE);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32,	0, 8,
		fectrl_mch5_c0_p200_p400);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8,
		fectrl_mch5_c1_p200_p400);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 64, 8,
		fectrl_mch5_c2_p200_p400);

}

static void
wlc_phy_write_regtbl_fc3_sub1(phy_info_t *pi)
{
	uint8 fectrl_mch5_c0[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 4, 3, 8, 8, 4, 3, 8, 0x08, 0x24, 0x03, 0x25, 0x08, 0x24, 0x03, 0x25};
	uint8 fectrl_mch5_c1[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 1, 6, 8, 8, 1, 6, 8, 0x08, 0x21, 0x06, 0x25, 0x08, 0x21, 0x06, 0x25};
	uint8 fectrl_mch5_c2[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		8, 1, 6, 8, 8, 1, 6, 8, 0x08, 0x21, 0x06, 0x23, 0x08, 0x21, 0x06, 0x23};

	/* P500+ */
	si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
		0xffffff, CCTRL4360_DISCRETE_FEMCTRL_MODE);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32,	0, 8,
		fectrl_mch5_c0);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8,
		fectrl_mch5_c1);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 64, 8,
		fectrl_mch5_c2);
}

static void
wlc_phy_write_regtbl_fc3_sub2(phy_info_t *pi)
{
	uint8 fectrl_j28[] =  {2, 4, 3, 2, 2, 4, 3, 2, 0x22, 0x24, 0x23, 0x25, 0x22, 0x24, 0x23,
		0x25, 2, 4, 3, 2, 2, 4, 3, 2, 0x22, 0x24, 0x23, 0x25, 0x22, 0x24, 0x23, 0x25};

	/* J28 */
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32,	0, 8,
		fectrl_j28);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8,
		fectrl_j28);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 64, 8,
		fectrl_j28);
}

static void
wlc_phy_write_regtbl_fc3_sub3(phy_info_t *pi)
{
	uint8 fectrl3_sub3_c0[] = {2, 4, 3, 2, 2, 4, 3, 2, 0x22, 0x24, 0x23, 0x25, 0x22, 0x24, 0x23,
		0x25, 2, 4, 3, 2, 2, 4, 3, 2, 0x22, 0x24, 0x23, 0x25, 0x22, 0x24, 0x23, 0x25};
	uint8 fectrl3_sub3_c1[] = {2, 1, 6, 2, 2, 1, 6, 2, 0x22, 0x21, 0x26, 0x25, 0x22, 0x21, 0x26,
		0x25, 2, 1, 6, 2, 2, 1, 6, 2, 0x22, 0x21, 0x26, 0x25, 0x22, 0x21, 0x26, 0x25};
	uint8 fectrl3_sub3_c2[] = {4, 1, 6, 4, 4, 1, 6, 4, 0x24, 0x21, 0x26, 0x23, 0x24, 0x21, 0x26,
		0x23, 4, 1, 6, 4, 4, 1, 6, 4, 0x24, 0x21, 0x26, 0x23, 0x24, 0x21, 0x26, 0x23};

	/* MCH2 */
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32,	0, 8,
		fectrl3_sub3_c0);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8,
		fectrl3_sub3_c1);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 64, 8,
		fectrl3_sub3_c2);
}

static INLINE void
wlc_phy_write_regtbl_fc3(phy_info_t *pi, phy_info_acphy_t *pi_ac)
{
	switch (pi_ac->srom.femctrl_sub) {
		case 0:
			wlc_phy_write_regtbl_fc3_sub0(pi);
		break;
		case 1:
			wlc_phy_write_regtbl_fc3_sub1(pi);
		break;
		case 2:
			wlc_phy_write_regtbl_fc3_sub2(pi);
		break;
		case 3:
			wlc_phy_write_regtbl_fc3_sub3(pi);
		break;
	}
}

static void
wlc_phy_write_regtbl_fc4_sub0(phy_info_t *pi)
{
	uint16 fectrl_zeroval[] = {0};
	uint16 kk, fem_idx = 0;
	sparse_array_entry_t fectrl_fcbga_epa_elna[] =
		{{2, 264}, {3, 8}, {9, 32}, {18, 5}, {19, 4}, {25, 128}, {130, 64}, {192, 64}};

	for (kk = 0; kk < 256; kk++) {
		if (fem_idx < ARRAYSIZE(fectrl_fcbga_epa_elna) &&
			kk == fectrl_fcbga_epa_elna[fem_idx].idx) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				&(fectrl_fcbga_epa_elna[fem_idx].val));
			fem_idx++;
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				fectrl_zeroval);
		}
	}
}

static void
wlc_phy_write_regtbl_fc4_sub1(phy_info_t *pi)
{
	uint16 fectrl_zeroval[] = {0};
	uint16 kk, fem_idx = 0;
	sparse_array_entry_t fectrl_wlbga_epa_elna[] =
	{{2, 3}, {3, 1}, {9, 256}, {18, 20}, {19, 16}, {25, 8}, {66, 3}, {67, 1},
	{73, 256}, {82, 20}, {83, 16}, {89, 8}, {128, 3}, {129, 1}, {130, 3}, {131, 1},
	{132, 1}, {133, 1}, {134, 1}, {135, 1}, {136, 3}, {137, 1}, {138, 3}, {139, 1},
	{140, 1}, {141, 1}, {142, 1}, {143, 1}, {160, 3}, {161, 1}, {162, 3}, {163, 1},
	{164, 1}, {165, 1}, {166, 1}, {167, 1}, {168, 3}, {169, 1}, {170, 3}, {171, 1},
	{172, 1}, {173, 1}, {174, 1}, {175, 1}, {192, 128}, {193, 128}, {196, 128}, {197, 128},
	{200, 128}, {201, 128}, {204, 128}, {205, 128}, {224, 128}, {225, 128}, {228, 128},
	{229, 128}, {232, 128}, {233, 128}, {236, 128}, {237, 128} };
	for (kk = 0; kk < 256; kk++) {
		if (fem_idx < ARRAYSIZE(fectrl_wlbga_epa_elna) &&
			kk == fectrl_wlbga_epa_elna[fem_idx].idx) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				&(fectrl_wlbga_epa_elna[fem_idx].val));
			fem_idx++;
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				fectrl_zeroval);
		}
	}
}

static void
wlc_phy_write_regtbl_fc4_sub2(phy_info_t *pi)
{
	uint16 fectrl_zeroval[] = {0};
	uint16 kk, fem_idx = 0;
	sparse_array_entry_t fectrl_fchm_epa_elna[] =
	{{2, 280}, {3, 24}, {9, 48}, {18, 21}, {19, 20}, {25, 144}, {34, 776}, {35, 520},
	{41, 544}, {50, 517}, {51, 516}, {57, 640}, {66, 280}, {67, 24}, {73, 48}, {82, 21},
	{83, 20}, {89, 144}, {98, 776}, {99, 520}, {105, 544}, {114, 517}, {115, 516}, {121, 640},
	{128, 280}, {129, 24}, {130, 280}, {131, 24}, {132, 24}, {133, 24}, {134, 24}, {135, 24},
	{136, 280}, {137, 24}, {138, 280}, {139, 24}, {140, 24}, {141, 24}, {142, 24}, {143, 24},
	{160, 776}, {161, 520}, {162, 776}, {163, 520}, {164, 520}, {165, 520}, {166, 520},
	{167, 520}, {168, 776}, {169, 520}, {170, 776}, {171, 520}, {172, 520}, {173, 520},
	{174, 520}, {175, 520},	{192, 80}, {193, 80}, {196, 80}, {197, 80}, {200, 80}, {201, 80},
	{204, 80}, {205, 80}, {224, 576}, {225, 576}, {228, 576}, {229, 576}, {232, 576},
	{233, 576}, {236, 576}, {237, 576}};
	for (kk = 0; kk < 256; kk++) {
		if (fem_idx < ARRAYSIZE(fectrl_fchm_epa_elna) &&
			kk == fectrl_fchm_epa_elna[fem_idx].idx) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				&(fectrl_fchm_epa_elna[fem_idx].val));
			fem_idx++;
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				fectrl_zeroval);
		}
	}
}

static void
wlc_phy_write_regtbl_fc4_sub34(phy_info_t *pi)
{
	uint16 fectrl_zeroval[] = {0};
	uint16 kk, fem_idx = 0;

	sparse_array_entry_t fectrl_wlcsp_epa_elna[] =
		{{2, 34}, {3, 2}, {9, 1}, {18, 80}, {19, 16}, {25, 8}, {66, 34}, {67, 2},
		{73, 1}, {82, 80}, {83, 16}, {89, 8}, {128, 34}, {129, 2}, {130, 34}, {131, 2},
		{132, 2}, {133, 2}, {134, 2}, {135, 2}, {136, 34}, {137, 2}, {138, 34}, {139, 2},
		{140, 2}, {141, 2}, {142, 2}, {143, 2}, {160, 34}, {161, 2}, {162, 34}, {163, 2},
		{164, 2}, {165, 2}, {166, 2}, {167, 2}, {168, 34}, {169, 2}, {170, 34}, {171, 2},
		{172, 2}, {173, 2}, {174, 2}, {175, 2}, {192, 4}, {193, 4}, {196, 4}, {197, 4},
		{200, 4}, {201, 4}, {204, 4}, {205, 4}, {224, 4}, {225, 4}, {228, 4}, {229, 4},
		{232, 4}, {233, 4}, {236, 4}, {237, 4} };
	for (kk = 0; kk < 256; kk++) {
		if (fem_idx < ARRAYSIZE(fectrl_wlcsp_epa_elna) &&
			kk == fectrl_wlcsp_epa_elna[fem_idx].idx) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				&(fectrl_wlcsp_epa_elna[fem_idx].val));
			fem_idx++;
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				fectrl_zeroval);
		}
	}
}

static void
wlc_phy_write_regtbl_fc4_sub5(phy_info_t *pi)
{
	uint16 fectrl_zeroval[] = {0};
	uint16 kk, fem_idx = 0;

	sparse_array_entry_t fectrl_fp_dpdt_epa_elna[] =
		{{2, 280}, {3, 24}, {9, 48}, {18, 21}, {19, 20}, {25, 144}, {34, 776},
		{35, 520}, {41, 544}, {50, 517}, {51, 516}, {57, 640}, {130, 80},
		{192, 80}};

	for (kk = 0; kk < 256; kk++) {
		if (fem_idx < ARRAYSIZE(fectrl_fp_dpdt_epa_elna) &&
			kk == fectrl_fp_dpdt_epa_elna[fem_idx].idx) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				&(fectrl_fp_dpdt_epa_elna[fem_idx].val));
			fem_idx++;
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				fectrl_zeroval);
		}
	}
}

static INLINE void
wlc_phy_write_regtbl_fc4(phy_info_t *pi, phy_info_acphy_t *pi_ac)
{
	switch (pi_ac->srom.femctrl_sub) {
		case 0:
			wlc_phy_write_regtbl_fc4_sub0(pi);
		break;
		case 1:
			wlc_phy_write_regtbl_fc4_sub1(pi);
		break;
		case 2:
			wlc_phy_write_regtbl_fc4_sub2(pi);
		break;
		case 3:
		case 4:
			wlc_phy_write_regtbl_fc4_sub34(pi);
		break;
		case 5:
			wlc_phy_write_regtbl_fc4_sub5(pi);
		break;
	}
}

static void
wlc_phy_write_regtbl_fc5(phy_info_t *pi)
{
	uint8 fectrl_femctrl5_c1[] = {0, 0, 0x50, 0x40, 0, 0, 0x50, 0x40, 0, 0x20, 0, 0, 0, 0, 0,
		0, 0x80, 0x80, 0x86, 0x82, 0x80, 0x80, 0x86, 0x82, 0x80, 0x81, 0x80, 0x80, 0x80,
		0x80, 0x80, 0x80};
	uint8 fectrl_zeros[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

	/* 4352hmb (2x2 bt), with A0
	   Core 0 has 5516 fem. Core 1 has 2g(rfmd 4203) & 5g(4501) fems
	*/
	wlc_phy_table_write_acphy_fectrl_fem5516(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32,  0, 8);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8,
	                          fectrl_femctrl5_c1);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 64, 8,
	                          fectrl_zeros);
}

static void
wlc_phy_write_regtbl_fc6(phy_info_t *pi)
{
	uint8 fectrl_femctrl6[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
		0, 0, 6, 2, 0, 0, 6, 2, 0, 1, 0, 0, 0, 0, 0, 0};

	/* 4360MC5 (3X3 5g only using rfmd 4501 fem) */
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32,	0, 8,
		fectrl_femctrl6);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 32, 8,
		fectrl_femctrl6);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 32, 64, 8,
		fectrl_femctrl6);
}

static void
wlc_phy_write_regtbl_fc7(phy_info_t *pi)
{
	uint16 fectrl_femctrl_4335_WLBGA_add[] = {18, 50, 80, 112, 129, 130, 131, 134, 135, 137,
		145, 146, 147, 150, 151, 153, 161, 162, 163, 166, 167, 169, 177, 178, 179, 182,
		183, 185};
	uint16 fectrl_femctrl_4335_WLBGA[] = {66, 34, 68, 36, 72, 64, 72, 64, 72,
		72, 65, 66, 65,
		66, 65, 65, 40,
		32, 40, 32, 40,
		40, 33, 34, 33,
		34, 33, 33};

	uint16 fem_idx1 = 0, kk1, fectrl_zeroval1 = 0;

	for (kk1 = 0; kk1 < 256; kk1++) {
		if (fem_idx1 < ARRAYSIZE(fectrl_femctrl_4335_WLBGA_add) &&
			kk1 == fectrl_femctrl_4335_WLBGA_add[fem_idx1]) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1,
				kk1, 16, &(fectrl_femctrl_4335_WLBGA[fem_idx1]));
			fem_idx1++;
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1,
				kk1, 16, &fectrl_zeroval1);
		}
	}
}

static void
wlc_phy_write_regtbl_fc8(phy_info_t *pi)
{
	/* 4335 femctrl tables are 256 entries.  using a compact way to write the table */
	uint16 fectrl_femctrl_4335_FCBGA_add[] = {161, 162, 163, 166, 167, 169, 177, 178, 179,
		182, 183, 185};
	uint16 fectrl_femctrl_4335_FCBGA[] = {9, 12, 9, 12, 9, 9, 65, 9, 65, 9, 65, 65};

	uint16 fem_idx1 = 0, fectrl_zeroval1 = 0, kk1;
	for (kk1 = 0; kk1 < 256; kk1++) {
		if (fem_idx1 < ARRAYSIZE(fectrl_femctrl_4335_FCBGA_add) &&
			kk1 == fectrl_femctrl_4335_FCBGA_add[fem_idx1]) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1,
				kk1, 16, &(fectrl_femctrl_4335_FCBGA[fem_idx1]));
			fem_idx1++;
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1,
				kk1, 16, &fectrl_zeroval1);
		}
	}
}

static void
wlc_phy_write_regtbl_fc9(phy_info_t *pi)
{
	uint16 fectrl_fcbgabu_epa_elna_idx[] = {2, 3, 9, 18, 19, 25, 130, 192};
	uint16 fectrl_fcbgabu_epa_elna_val[] = {128, 0, 4, 64, 0, 3, 8, 8};
	uint16 fectrl_zeroval[] = {0};
	uint kk, fem_idx = 0;
	for (kk = 0; kk < 256; kk++) {
		if (fem_idx < ARRAYSIZE(fectrl_fcbgabu_epa_elna_idx) &&
			kk == fectrl_fcbgabu_epa_elna_idx[fem_idx]) {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
			&(fectrl_fcbgabu_epa_elna_val[fem_idx]));
			fem_idx++;
		} else {
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, kk, 16,
				fectrl_zeroval);
		}
	}
}

static void
wlc_phy_set_regtbl_on_femctrl(phy_info_t *pi)
{
	uint8 stall_val, bt_fem;
	bool bt_on_gpio4;
	bool femctrl_sub_is_zero = FALSE;

	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;


	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	switch (pi_ac->srom.femctrl) {
	case 0:
		/* Chip default, do nothing */
		break;
	case 1:
		MOD_PHYREG(pi, ACPHY_BT_SwControl, bt_sharing_en, 1);  /* chip_bandsel = bandsel */
		wlc_phy_write_regtbl_fc1(pi);
		break;
	case 2:
		/*  X29c & 4352hmb(wiht B0)
		    Cores {0, 2} have 5516 fem. Core 1 has separate 2g/5g fems
		*/
		bt_fem = 0; bt_on_gpio4 = FALSE;
		femctrl_sub_is_zero = (pi_ac->srom.femctrl_sub == 0);
		wlc_phy_write_regtbl_fc2(pi, femctrl_sub_is_zero);

		if (femctrl_sub_is_zero) {
			bt_on_gpio4 = TRUE;  /* fem_bt = gpio4 */
		} else {
			bt_fem = 4;  /* fem_bt = bt_fem[2] */
		}

		/* Setup middle core for BT */
		wlc_phy_set_bt_on_core1_acphy(pi, bt_fem);

		/* Release control of gpio4 if required */
		if (bt_on_gpio4)
			wlc_phy_bt_on_gpio4_acphy(pi);

		break;
	case 3:
		/*  Routers (MCH5, J28) */
		MOD_PHYREG(pi, ACPHY_BT_SwControl, bt_sharing_en, 0);
		si_pmu_regcontrol(pi->sh->sih, 0, 0x4, 4);   /* pwron pavref ldo */
		wlc_phy_write_regtbl_fc3(pi, pi_ac);

		break;
	case 4:
		/* 4335 epa elna boards */
		if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5)) {
			wlc_phy_write_regtbl_fc4(pi, pi_ac);
		}
		break;
	case 5:
		wlc_phy_write_regtbl_fc5(pi);

		/* Setup middle core for BT */
		wlc_phy_set_bt_on_core1_acphy(pi, 8);
		break;
	case 6:
		wlc_phy_write_regtbl_fc6(pi);
		break;
	case 7:
		wlc_phy_write_regtbl_fc7(pi);
		break;
	case 8:
		wlc_phy_write_regtbl_fc8(pi);
		break;
	case 9:
		if (pi_ac->srom.femctrl_sub == 1) {
			wlc_phy_write_regtbl_fc9(pi);
		}
		break;
	/* LOOK: when adding new cases, follow above pattern to minimize stack/memory usage! */
	default:
		/* 5516 on all cores */
		MOD_PHYREG(pi, ACPHY_BT_SwControl, bt_sharing_en, 1);  /* chip_bandsel = bandsel */
		wlc_phy_write_regtbl_fc1(pi);

		break;
	}

	if (pi_ac->srom.bt_coex) {
		if (ACREV_IS(pi->pubpi.phy_rev, 0)) {
			si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
			           CCTRL4360_SECI_MODE, CCTRL4360_SECI_MODE);
		} else if (ACREV_IS(pi->pubpi.phy_rev, 1)) {
			si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
			           CCTRL4360_SECI_ON_GPIO01, CCTRL4360_SECI_ON_GPIO01);
		} else if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5)) {
			PHY_ERROR(("wl%d: %s: FIXME bt_coex\n", pi->sh->unit, __FUNCTION__));
		} else {
			ASSERT(0);
		}
	}

	ACPHY_ENABLE_STALL(pi, stall_val);
}


static void
wlc_phy_txpower_recalc_target_acphy(phy_info_t *pi)
{

#ifdef PPR_API
	wlapi_high_update_txppr_offset(pi->sh->physhim, pi->tx_power_offset);
#else
	uint rate;
	for (rate = 0; rate < TXP_NUM_RATES; rate++) {
		pi->tx_power_offset[rate] = 0;
	}
	wlapi_high_update_txppr_offset(pi->sh->physhim, pi->tx_power_offset);
#endif
	/* recalc targets -- turns hwpwrctrl off */
	wlc_phy_txpwrctrl_pwr_setup_acphy(pi);

	/* restore power control */
	wlc_phy_txpwrctrl_enable_acphy(pi, pi->txpwrctrl);
}

static void
wlc_phy_watchdog_acphy(phy_info_t *pi)
{
	/* printf("%s: NOT Implemented\n", __FUNCTION__); */
}

static void
wlc_phy_subband_cust_2g_acphy(phy_info_t *pi)
{
	uint8 stall_val, core;
	uint16 rxgain_tbl[3];
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		if (pi_ac->srom.femctrl == 4 && pi_ac->srom.femctrl_sub == 0) {
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeA, 0x14a);
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x724);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeA, 0x14a);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeB, 0x014);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeA, 0x12a);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeB, 0x004);
			ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeA, 0x16a);
			ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeB, 0x018);
			rxgain_tbl[0] = 0xe8a5;
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,  0xf9, 16, rxgain_tbl);
		} else {
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeA, 0x15a);
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x724);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeA, 0x15a);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeB, 0x014);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeA, 0x15a);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeB, 0x004);
			ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeA, 0x16a);
			ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeB, 0x018);
			rxgain_tbl[0] = 0xe8ad;
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,  0xf9, 16, rxgain_tbl);
			wlc_phy_set_crs_min_pwr_acphy(pi, 58, 0, 0);
		}
	} else {
		/* For now use the same settings for X29 & MC */

		ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeA, 0x14a);
		ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x724);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeA, 0x14a);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeB, 0x014);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeA, 0x12a);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeB, 0x004);
		ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeA, 0x16a);
		ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeB, 0x018);

		rxgain_tbl[0] = rxgain_tbl[1] = rxgain_tbl[2] = 0xe8a5;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 3,  0xf9, 16, rxgain_tbl);
	}
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		phy_reg_write(pi, ACPHYREGC(RssiClipMuxSel, core), 5);

		MOD_RADIO_REGFLDC(pi, RF_2069_NBRSSI_CONFG(core), NBRSSI_CONFG,
		                  nbrssi_Refctrl_mid, 3);
		MOD_RADIO_REGFLDC(pi, RF_2069_LNA2G_RSSI(core), LNA2G_RSSI,
		        dig_wrssi1_threshold, 6);

	}

	/* X29, core1 has less isolation */
	if (pi_ac->srom.femctrl == 2) {
		phy_reg_write(pi, ACPHY_Core1cliploGainCodeA, 0x14a);
		phy_reg_write(pi, ACPHY_Core1cliploGainCodeB, 0x008);

		phy_reg_write(pi, ACPHY_Core1RssiClipMuxSel, 1);
		MOD_RADIO_REG(pi, RF1, LNA2G_RSSI, dig_wrssi1_threshold, 8);
	}

	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_subband_cust_5g_acphy(phy_info_t *pi)
{
	uint16 rxgain_tbl[3];
	uint8 stall_val, core;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	uint16 fc;
	uint16 boardrev = pi->sh->boardrev & 0xfff;

	if (CHSPEC_CHANNEL(pi->radio_chanspec) > 14) {
		fc = CHAN5G_FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	} else {
		fc = CHAN2G_FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	}

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		/* for now 4335 use these values on 5G band */
		ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeA, 0x16a);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeB, 0x004);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeA, 0x13a);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeB, 0x014);
		ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeA, 0x14a);
		ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeB, 0x018);
		rxgain_tbl[0] = 0xa8b5;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,  0xf9, 16, rxgain_tbl);
		if (pi_ac->srom.femctrl == 4 && pi_ac->srom.femctrl_sub == 0) {
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeA, 0x16a);
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x524);
			rxgain_tbl[0] = 0xa8b5;
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,  0xf9, 16, rxgain_tbl);
			wlc_phy_set_crs_min_pwr_acphy(pi, 76, 0, 0);
		} else if (pi_ac->srom.femctrl == 4 && pi_ac->srom.femctrl_sub == 1) {
			ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeA, 0x138);
			ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeB, 0x014);
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeA, 0x16a);
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x624);
			rxgain_tbl[0] = 0xc8b5;
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,  0xf9, 16, rxgain_tbl);
			wlc_phy_set_crs_min_pwr_acphy(pi, 63, 0, 0);
		} else {
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeA, 0x16a);
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x424);
			rxgain_tbl[0] = 0x88b5;
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,  0xf9, 16, rxgain_tbl);
			if (boardrev == 0x110) {
				wlc_phy_set_crs_min_pwr_acphy(pi, 66, 0, 0);
			} else {
				wlc_phy_set_crs_min_pwr_acphy(pi, 60, 0, 0);
			}
		}
	} else {
		/* For now use the same settings for X29 & MC */
		ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeA, 0x16a);
		ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x514);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeA, 0x16a);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipHiGainCodeB, 0x004);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeA, 0x13a);
		ACPHYREG_BCAST(pi, ACPHY_Core0clipmdGainCodeB, 0x004);
		ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeA, 0x16a);
		ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeB, 0x038);
		rxgain_tbl[0] = rxgain_tbl[1] = rxgain_tbl[2] = 0xa4b5;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 3,  0xf9, 16, rxgain_tbl);
	}

	/* MCH5 INIT/Clip gains */
	if (pi_ac->srom.femctrl == 3) {
		if (fc < 5500) {
			/* MCH5 Lower aband has ~4dB less gain */
			ACPHYREG_BCAST(pi, ACPHY_Core0InitGainCodeB, 0x524);
			rxgain_tbl[0] = rxgain_tbl[1] = rxgain_tbl[2] = 0xa8b5;
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 3,  0xf9, 16, rxgain_tbl);
		}

		ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeA, 0x16a);
		ACPHYREG_BCAST(pi, ACPHY_Core0cliploGainCodeB, 0x008);
	}

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		MOD_RADIO_REGFLDC(pi, RF_2069_NBRSSI_CONFG(core), NBRSSI_CONFG,
		                  nbrssi_Refctrl_high, 0);
		if (pi_ac->srom.femctrl == 3) {
			phy_reg_write(pi, ACPHYREGC(RssiClipMuxSel, core), 2);
			if (fc < 5500) {
				MOD_RADIO_REGFLDC(pi, RF_2069_LNA5G_RSSI(core), LNA5G_RSSI,
				                  dig_wrssi1_threshold, 13);
			} else {
				MOD_RADIO_REGFLDC(pi, RF_2069_LNA5G_RSSI(core), LNA5G_RSSI,
				                  dig_wrssi1_threshold, 10);
			}
		} else {
			phy_reg_write(pi, ACPHYREGC(RssiClipMuxSel, core), 6);
			MOD_RADIO_REGFLDC(pi, RF_2069_LNA5G_RSSI(core), LNA5G_RSSI,
			                  dig_wrssi1_threshold, 7);
		}
	}

	/* X29c - core1 */
	if (pi_ac->srom.femctrl == 2) {
		/* X29, core1 has less isolation */
		phy_reg_write(pi, ACPHY_Core1cliploGainCodeB, 0x008);

		phy_reg_write(pi, ACPHY_Core1RssiClipMuxSel, 2);
		MOD_RADIO_REG(pi, RF1, LNA5G_RSSI, dig_wrssi1_threshold, 12);
	}

	ACPHY_ENABLE_STALL(pi, stall_val);
}


/*  lookup radio-chip-specific channel code */
static bool
wlc_phy_chan2freq_acphy(phy_info_t *pi, uint channel, int *f,
                                      chan_info_radio2069_t **t,
                                      chan_info_radio2069revGE16_t **tGE16)
{
	uint i;
	chan_info_radio2069_t *chan_info_tbl = NULL;
	chan_info_radio2069revGE16_t *chan_info_tbl_GE16 = NULL;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	uint32 tbl_len = 0;
	int freq = 0;

	ASSERT(RADIOID(pi->pubpi.radioid) == BCM2069_ID);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	switch (RADIOREV(pi->pubpi.radiorev)) {
	case 3:
		chan_info_tbl = chan_tuning_2069rev3;
		tbl_len = ARRAYSIZE(chan_tuning_2069rev3);
		break;

	case 4:
		chan_info_tbl = chan_tuning_2069rev4;
		tbl_len = ARRAYSIZE(chan_tuning_2069rev4);
		break;

	case 16:
		if (pi->xtalfreq == 40000000) {
#ifndef ACPHY_1X1_37P4
			pi_ac->acphy_lp_status = pi_ac->acphy_lp_mode;
			if ((pi_ac->acphy_lp_mode == 2) || (pi_ac->acphy_lp_mode == 3)) {
				/* In this configure the LP mode settings */
				/* For Rev16/17/18 using the same LP setting TBD */
				chan_info_tbl_GE16 = chan_tuning_2069rev_GE16_40_lp;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_GE16_40_lp);
			} else {
				chan_info_tbl_GE16 = chan_tuning_2069rev_16_17_40;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_16_17_40);
			}
#else
			ASSERT(0);
#endif
		} else {
			pi_ac->acphy_lp_status = pi_ac->acphy_lp_mode;
			if ((pi_ac->acphy_lp_mode == 2) || (pi_ac->acphy_lp_mode == 3)) {
				/* In this configure the LP mode settings */
				/* For Rev16/17/18 using the same LP setting TBD */
				chan_info_tbl_GE16 = chan_tuning_2069rev_GE16_lp;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_GE16_lp);
			} else {
				chan_info_tbl_GE16 = chan_tuning_2069rev_16_17;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_16_17);
			}
		}
		break;
	case 17:
	case 23:
		if (pi->xtalfreq == 40000000) {
#ifndef ACPHY_1X1_37P4
			pi_ac->acphy_lp_status = pi_ac->acphy_lp_mode;
			if ((pi_ac->acphy_lp_mode == 2) || (pi_ac->acphy_lp_mode == 3)) {
				/* In this configure the LP mode settings */
				/* For Rev16/17/18 using the same LP setting TBD */
				chan_info_tbl_GE16 = chan_tuning_2069rev_GE16_40_lp;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_GE16_40_lp);
			} else {
				chan_info_tbl_GE16 = chan_tuning_2069rev_16_17_40;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_16_17_40);
			}
#else
			ASSERT(0);
#endif
		} else {
			pi_ac->acphy_lp_status = pi_ac->acphy_lp_mode;
			if ((pi_ac->acphy_lp_mode == 2) || (pi_ac->acphy_lp_mode == 3)) {
				/* In this configure the LP mode settings */
				/* For Rev16/17/18 using the same LP setting TBD */
				chan_info_tbl_GE16 = chan_tuning_2069rev_GE16_lp;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_GE16_lp);
			} else {
				chan_info_tbl_GE16 = chan_tuning_2069rev_16_17;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_16_17);
			}
		}
		if (pi_ac->acphy_prev_lp_mode != pi_ac->acphy_lp_mode) {
			if ((pi_ac->acphy_lp_mode == 2) || (pi_ac->acphy_lp_mode == 3)) {
				si_pmu_chipcontrol(pi->sh->sih, PMU_CHIPCTL0, 0x1F<<10, 4);
				si_pmu_chipcontrol(pi->sh->sih, PMU_CHIPCTL0, 0x1F<<15, 4);
			}
			pi_ac->acphy_prev_lp_mode = pi_ac->acphy_lp_mode;
		}
		break;
	case 18:
	case 24:
		if (pi->xtalfreq == 40000000) {
#ifndef ACPHY_1X1_37P4
			pi_ac->acphy_lp_status = pi_ac->acphy_lp_mode;
			if ((pi_ac->acphy_lp_mode == 2) || (pi_ac->acphy_lp_mode == 3)) {
				/* In this configure the LP mode settings */
				/* For Rev16/17/18 using the same LP setting TBD */
				chan_info_tbl_GE16 = chan_tuning_2069rev_GE16_40_lp;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_GE16_40_lp);
			} else {
			    chan_info_tbl_GE16 = chan_tuning_2069rev_18_40;
			    tbl_len = ARRAYSIZE(chan_tuning_2069rev_18_40);
			}
#else
			ASSERT(0);
#endif
		} else {
			pi_ac->acphy_lp_status = pi_ac->acphy_lp_mode;
			if ((pi_ac->acphy_lp_mode == 2) || (pi_ac->acphy_lp_mode == 3)) {
				/* In this configure the LP mode settings */
				/* For Rev16/17/18 using the same LP setting TBD */
				chan_info_tbl_GE16 = chan_tuning_2069rev_GE16_lp;
				tbl_len = ARRAYSIZE(chan_tuning_2069rev_GE16_lp);
			} else {
			    chan_info_tbl_GE16 = chan_tuning_2069rev_18;
			    tbl_len = ARRAYSIZE(chan_tuning_2069rev_18);
			}
		}
		break;

	default:

		PHY_ERROR(("wl%d: %s: Unsupported radio revision %d\n",
		           pi->sh->unit, __FUNCTION__, RADIOREV(pi->pubpi.radiorev)));
		ASSERT(0);
	}

	for (i = 0; i < tbl_len; i++) {

		if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
			if (chan_info_tbl_GE16[i].chan == channel)
				break;
		} else {
			if (chan_info_tbl[i].chan == channel)
				break;
		}
	}

	if (i >= tbl_len) {
		PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
		           pi->sh->unit, __FUNCTION__, channel));
		ASSERT(i < tbl_len);
		goto fail;
	}


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		*tGE16 = &chan_info_tbl_GE16[i];
		freq = chan_info_tbl_GE16[i].freq;
	} else {
		*t = &chan_info_tbl[i];
		freq = chan_info_tbl[i].freq;
	}

	*f = freq;
	return TRUE;

fail:
	*f = WL_CHAN_FREQ_RANGE_2G;
	return FALSE;
}

static bool
wlc_phy_chan2freq_acdcphy(phy_info_t *pi, uint channel, int *f, chan_info_radio20691_t **t)
{
	uint i;
	chan_info_radio20691_t *chan_info_tbl = NULL;
	uint32 tbl_len = 0;

	ASSERT(RADIOID(pi->pubpi.radioid) == BCM20691_ID);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	switch (RADIOREV(pi->pubpi.radiorev)) {
	case 1:
		chan_info_tbl = chan_tuning_20691rev_1;
		tbl_len = ARRAYSIZE(chan_tuning_20691rev_1);
		break;

	default:
		PHY_ERROR(("wl%d: %s: Unsupported radio revision %d\n",
		           pi->sh->unit, __FUNCTION__, RADIOREV(pi->pubpi.radiorev)));
		ASSERT(0);
	}

	for (i = 0; i < tbl_len && chan_info_tbl[i].chan != channel; i++);

	if (i >= tbl_len) {
		PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
		           pi->sh->unit, __FUNCTION__, channel));
		ASSERT(i < tbl_len);
		goto fail;
	}

	*t = &chan_info_tbl[i];
	*f = chan_info_tbl[i].freq;

	return TRUE;

fail:
	*f = WL_CHAN_FREQ_RANGE_2G;

	return FALSE;
}

static void
wlc_phy_2069_4335_set_ovrds(phy_info_t *pi)
{

	write_radio_reg(pi, RFP_2069_GE16_OVR30, 0x1df3);
	write_radio_reg(pi, RFP_2069_GE16_OVR31, 0x1ffc);
	write_radio_reg(pi, RFP_2069_GE16_OVR32, 0x0078);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		write_radio_reg(pi, RF0_2069_GE16_OVR28, 0x0);
		write_radio_reg(pi, RFP_2069_GE16_OVR29, 0x0);
	} else {
		write_radio_reg(pi, RF0_2069_GE16_OVR28, 0xffff);
		write_radio_reg(pi, RFP_2069_GE16_OVR29, 0xffff);
	}


}
static void
wlc_phy_chanspec_radio2069_setup(phy_info_t *pi, const chan_info_radio2069_t *ci,
                                      const chan_info_radio2069revGE16_t *ciGE16,
                                      uint8 toggle_logen_reset)
{
	uint16 radio_rev_id;
	uint channel = CHSPEC_CHANNEL(pi->radio_chanspec);

	radio_rev_id = read_radio_reg(pi, RF_2069_REV_ID(0));

	/* logen_reset needs to be toggled whnenever bandsel bit if changed */
	/* On a bw change, phy_reset is issued which causes currentBand getting reset to 0 */
	/* So, issue this on both band & bw change */
	if (toggle_logen_reset == 1) {
		MOD_PHYREG(pi, ACPHY_RfctrlCoreTxPus0, logen_reset, 1);
		OSL_DELAY(1);
		MOD_PHYREG(pi, ACPHY_RfctrlCoreTxPus0, logen_reset, 0);
	}


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		ASSERT(ciGE16 != NULL);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL5, ciGE16->RFP_pll_vcocal5);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL6, ciGE16->RFP_pll_vcocal6);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL2, ciGE16->RFP_pll_vcocal2);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL1, ciGE16->RFP_pll_vcocal1);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL11, ciGE16->RFP_pll_vcocal11);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL12, ciGE16->RFP_pll_vcocal12);
		write_radio_reg(pi, RFP_2069_PLL_FRCT2, ciGE16->RFP_pll_frct2);
		write_radio_reg(pi, RFP_2069_PLL_FRCT3, ciGE16->RFP_pll_frct3);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL10, ciGE16->RFP_pll_vcocal10);
		write_radio_reg(pi, RFP_2069_PLL_XTAL3, ciGE16->RFP_pll_xtal3);
		write_radio_reg(pi, RFP_2069_PLL_VCO2, ciGE16->RFP_pll_vco2);
		write_radio_reg(pi, RF0_2069_LOGEN5G_CFG1, ciGE16->RFP_logen5g_cfg1);
		write_radio_reg(pi, RFP_2069_PLL_VCO8, ciGE16->RFP_pll_vco8);
		write_radio_reg(pi, RFP_2069_PLL_VCO6, ciGE16->RFP_pll_vco6);
		write_radio_reg(pi, RFP_2069_PLL_VCO3, ciGE16->RFP_pll_vco3);
		write_radio_reg(pi, RFP_2069_PLL_XTALLDO1, ciGE16->RFP_pll_xtalldo1);
		write_radio_reg(pi, RFP_2069_PLL_HVLDO1, ciGE16->RFP_pll_hvldo1);
		write_radio_reg(pi, RFP_2069_PLL_HVLDO2, ciGE16->RFP_pll_hvldo2);
		write_radio_reg(pi, RFP_2069_PLL_VCO5, ciGE16->RFP_pll_vco5);
		write_radio_reg(pi, RFP_2069_PLL_VCO4, ciGE16->RFP_pll_vco4);

		write_radio_reg(pi, RFP_2069_PLL_LF4, ciGE16->RFP_pll_lf4);
		write_radio_reg(pi, RFP_2069_PLL_LF5, ciGE16->RFP_pll_lf5);
		write_radio_reg(pi, RFP_2069_PLL_LF7, ciGE16->RFP_pll_lf7);
		write_radio_reg(pi, RFP_2069_PLL_LF2, ciGE16->RFP_pll_lf2);
		write_radio_reg(pi, RFP_2069_PLL_LF3, ciGE16->RFP_pll_lf3);
		write_radio_reg(pi, RFP_2069_PLL_CP4, ciGE16->RFP_pll_cp4);
		write_radio_reg(pi, RFP_2069_PLL_LF6, ciGE16->RFP_pll_lf6);

		write_radio_reg(pi, RF0_2069_LOGEN2G_TUNE, ciGE16->RFP_logen2g_tune);
		write_radio_reg(pi, RF0_2069_LNA2G_TUNE, ciGE16->RF0_lna2g_tune);
		write_radio_reg(pi, RF0_2069_TXMIX2G_CFG1, ciGE16->RF0_txmix2g_cfg1);
		write_radio_reg(pi, RF0_2069_PGA2G_CFG2, ciGE16->RF0_pga2g_cfg2);
		write_radio_reg(pi, RF0_2069_PAD2G_TUNE, ciGE16->RF0_pad2g_tune);
		write_radio_reg(pi, RF0_2069_LOGEN5G_TUNE1, ciGE16->RFP_logen5g_tune1);
		write_radio_reg(pi, RF0_2069_LOGEN5G_TUNE2, ciGE16->RFP_logen5g_tune2);
		write_radio_reg(pi, RF0_2069_LOGEN5G_RCCR, ciGE16->RF0_logen5g_rccr);
		write_radio_reg(pi, RF0_2069_LNA5G_TUNE, ciGE16->RF0_lna5g_tune);
		write_radio_reg(pi, RF0_2069_TXMIX5G_CFG1, ciGE16->RF0_txmix5g_cfg1);
		write_radio_reg(pi, RF0_2069_PGA5G_CFG2, ciGE16->RF0_pga5g_cfg2);
		write_radio_reg(pi, RF0_2069_PAD5G_TUNE, ciGE16->RF0_pad5g_tune);
		/*
		* write_radio_reg(pi, RFP_2069_PLL_CP5, ciGE16->RFP_pll_cp5);
		* write_radio_reg(pi, RF0_2069_AFEDIV1, ciGE16->RF0_afediv1);
		* write_radio_reg(pi, RF0_2069_AFEDIV2, ciGE16->RF0_afediv2);
		* write_radio_reg(pi, RF0_2069_ADC_CFG5, ciGE16->RF0_adc_cfg5);
		*/

	} else {
		ASSERT(ci != NULL);
		/* Write chan specific tuning register */
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL5, ci->RFP_pll_vcocal5);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL6, ci->RFP_pll_vcocal6);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL2, ci->RFP_pll_vcocal2);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL1, ci->RFP_pll_vcocal1);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL11, ci->RFP_pll_vcocal11);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL12, ci->RFP_pll_vcocal12);
		write_radio_reg(pi, RFP_2069_PLL_FRCT2, ci->RFP_pll_frct2);
		write_radio_reg(pi, RFP_2069_PLL_FRCT3, ci->RFP_pll_frct3);
		write_radio_reg(pi, RFP_2069_PLL_VCOCAL10, ci->RFP_pll_vcocal10);
		write_radio_reg(pi, RFP_2069_PLL_XTAL3, ci->RFP_pll_xtal3);
		write_radio_reg(pi, RFP_2069_PLL_VCO2, ci->RFP_pll_vco2);
		write_radio_reg(pi, RF0_2069_LOGEN5G_CFG1, ci->RF0_logen5g_cfg1);
		write_radio_reg(pi, RFP_2069_PLL_VCO8, ci->RFP_pll_vco8);
		write_radio_reg(pi, RFP_2069_PLL_VCO6, ci->RFP_pll_vco6);
		write_radio_reg(pi, RFP_2069_PLL_VCO3, ci->RFP_pll_vco3);
		write_radio_reg(pi, RFP_2069_PLL_XTALLDO1, ci->RFP_pll_xtalldo1);
		write_radio_reg(pi, RFP_2069_PLL_HVLDO1, ci->RFP_pll_hvldo1);
		write_radio_reg(pi, RFP_2069_PLL_HVLDO2, ci->RFP_pll_hvldo2);
		write_radio_reg(pi, RFP_2069_PLL_VCO5, ci->RFP_pll_vco5);
		write_radio_reg(pi, RFP_2069_PLL_VCO4, ci->RFP_pll_vco4);
		write_radio_reg(pi, RFP_2069_PLL_LF4, ci->RFP_pll_lf4);
		write_radio_reg(pi, RFP_2069_PLL_LF5, ci->RFP_pll_lf5);
		write_radio_reg(pi, RFP_2069_PLL_LF7, ci->RFP_pll_lf7);
		write_radio_reg(pi, RFP_2069_PLL_LF2, ci->RFP_pll_lf2);
		write_radio_reg(pi, RFP_2069_PLL_LF3, ci->RFP_pll_lf3);
		write_radio_reg(pi, RFP_2069_PLL_CP4, ci->RFP_pll_cp4);
		write_radio_reg(pi, RFP_2069_PLL_DSP1, ci->RFP_pll_dsp1);
		write_radio_reg(pi, RFP_2069_PLL_DSP2, ci->RFP_pll_dsp2);
		write_radio_reg(pi, RFP_2069_PLL_DSP3, ci->RFP_pll_dsp3);
		write_radio_reg(pi, RFP_2069_PLL_DSP4, ci->RFP_pll_dsp4);
		write_radio_reg(pi, RFP_2069_PLL_DSP6, ci->RFP_pll_dsp6);
		write_radio_reg(pi, RFP_2069_PLL_DSP7, ci->RFP_pll_dsp7);
		write_radio_reg(pi, RFP_2069_PLL_DSP8, ci->RFP_pll_dsp8);
		write_radio_reg(pi, RFP_2069_PLL_DSP9, ci->RFP_pll_dsp9);
		write_radio_reg(pi, RF0_2069_LOGEN2G_TUNE, ci->RF0_logen2g_tune);
		write_radio_reg(pi, RFX_2069_LNA2G_TUNE, ci->RFX_lna2g_tune);
		write_radio_reg(pi, RFX_2069_TXMIX2G_CFG1, ci->RFX_txmix2g_cfg1);
		write_radio_reg(pi, RFX_2069_PGA2G_CFG2, ci->RFX_pga2g_cfg2);
		write_radio_reg(pi, RFX_2069_PAD2G_TUNE, ci->RFX_pad2g_tune);
		write_radio_reg(pi, RF0_2069_LOGEN5G_TUNE1, ci->RF0_logen5g_tune1);
		write_radio_reg(pi, RF0_2069_LOGEN5G_TUNE2, ci->RF0_logen5g_tune2);
		write_radio_reg(pi, RFX_2069_LOGEN5G_RCCR, ci->RFX_logen5g_rccr);
		write_radio_reg(pi, RFX_2069_LNA5G_TUNE, ci->RFX_lna5g_tune);
		write_radio_reg(pi, RFX_2069_TXMIX5G_CFG1, ci->RFX_txmix5g_cfg1);
		write_radio_reg(pi, RFX_2069_PGA5G_CFG2, ci->RFX_pga5g_cfg2);
		write_radio_reg(pi, RFX_2069_PAD5G_TUNE, ci->RFX_pad5g_tune);
		write_radio_reg(pi, RFP_2069_PLL_CP5, ci->RFP_pll_cp5);
		write_radio_reg(pi, RF0_2069_AFEDIV1, ci->RF0_afediv1);
		write_radio_reg(pi, RF0_2069_AFEDIV2, ci->RF0_afediv2);
		write_radio_reg(pi, RFX_2069_ADC_CFG5, ci->RFX_adc_cfg5);

		/* We need different values for ADC_CFG5 for cores 1 and 2
		 * in order to get the best reduction of spurs from the AFE clk
		 */
		if (radio_rev_id < 4) {
			write_radio_reg(pi, RF1_2069_ADC_CFG5, 0x3e9);
			write_radio_reg(pi, RF2_2069_ADC_CFG5, 0x3e9);
			MOD_RADIO_REG(pi, RFP, PLL_CP4, rfpll_cp_ioff, 0xa0);
		}

		/* Reduce 500 KHz spur at fc=2427 MHz for both 4360 A0 and B0 */
		if (channel == 4) {
			write_radio_reg(pi, RFP_2069_PLL_VCO2, 0xce4);
			MOD_RADIO_REG(pi, RFP, PLL_XTAL4, xtal_xtbufstrg, 0x5);
		}

		if (radio_rev_id <= 4) {
			/* 4360 a0 & b0 */

			/* Move nbclip by 2dBs to the right */
			MOD_RADIO_REG(pi, RFX, NBRSSI_CONFG, nbrssi_ib_Refladder, 7);

			/* 5g only: Changing RFPLL bandwidth to be 150MHz */
			if (CHIPID(pi->sh->chip) != BCM43526_CHIP_ID) {
				/* Don't do it for 43526usb chip for now */
				if (CHSPEC_IS5G(pi->radio_chanspec))
					wlc_2069_rfpll_150khz(pi);
			}
		}
	}

	if (radio_rev_id >= 4) {
		/* Make clamping stronger */
		write_radio_reg(pi, RFX_2069_ADC_CFG5, 0x83e0);
	}


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		MOD_RADIO_REG(pi, RFP, PLL_CP4, rfpll_cp_ioff, 0xe0);
	}

	/* increasing pabias to get good evm with pagain3 */
	if (ACRADIO_IPA1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
		write_radio_reg(pi, RF0_2069_PA5G_IDAC2, 0x8484);
	}

	if (ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) {

		wlc_phy_2069_4335_set_ovrds(pi);
	}


	/* Do a VCO cal after writing the tuning table regs */
	wlc_phy_radio2069_vcocal(pi);
}


void
wlc_2069_rfpll_150khz(phy_info_t *pi)
{
	MOD_RADIO_REG(pi, RFP, PLL_LF4, rfpll_lf_lf_r1, 0);
	MOD_RADIO_REG(pi, RFP, PLL_LF4, rfpll_lf_lf_r2, 2);
	write_radio_reg(pi, RFP_2069_PLL_LF5, 2);
	MOD_RADIO_REG(pi, RFP, PLL_LF7, rfpll_lf_lf_rs_cm, 2);
	MOD_RADIO_REG(pi, RFP, PLL_LF7, rfpll_lf_lf_rf_cm, 0xff);
	write_radio_reg(pi, RFP_2069_PLL_LF2, 0xffff);
	write_radio_reg(pi, RFP_2069_PLL_LF3, 0xffff);
}

void
wlc_phy_force_rfseq_acphy(phy_info_t *pi, uint8 cmd)
{
	uint16 trigger_mask, status_mask;
	uint16 orig_RfseqCoreActv, orig_rxfectrl1;

	switch (cmd) {
	case ACPHY_RFSEQ_RX2TX:
		trigger_mask = ACPHY_RfseqTrigger_rx2tx_MASK;
		status_mask = ACPHY_RfseqStatus0_rx2tx_MASK;
		break;
	case ACPHY_RFSEQ_TX2RX:
		trigger_mask = ACPHY_RfseqTrigger_tx2rx_MASK;
		status_mask = ACPHY_RfseqStatus0_tx2rx_MASK;
		break;
	case ACPHY_RFSEQ_RESET2RX:
		trigger_mask = ACPHY_RfseqTrigger_reset2rx_MASK;
		status_mask = ACPHY_RfseqStatus0_reset2rx_MASK;
		break;
	case ACPHY_RFSEQ_UPDATEGAINH:
		trigger_mask = ACPHY_RfseqTrigger_updategainh_MASK;
		status_mask = ACPHY_RfseqStatus0_updategainh_MASK;
		break;
	case ACPHY_RFSEQ_UPDATEGAINL:
		trigger_mask = ACPHY_RfseqTrigger_updategainl_MASK;
		status_mask = ACPHY_RfseqStatus0_updategainl_MASK;
		break;
	case ACPHY_RFSEQ_UPDATEGAINU:
		trigger_mask = ACPHY_RfseqTrigger_updategainu_MASK;
		status_mask = ACPHY_RfseqStatus0_updategainu_MASK;
		break;
	default:
		PHY_ERROR(("wlc_phy_force_rfseq_acphy: unrecognized command."));
		return;
	}

	/* Save */
	orig_RfseqCoreActv = phy_reg_read(pi, ACPHY_RfseqMode);
	orig_rxfectrl1 = phy_reg_read(pi, ACPHY_RxFeCtrl1);

	ACPHY_DISABLE_STALL(pi);
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 1);

	/* Trigger */
	phy_reg_or(pi, ACPHY_RfseqMode,
	           (ACPHY_RfseqMode_CoreActv_override_MASK |
	            ACPHY_RfseqMode_Trigger_override_MASK));
	phy_reg_or(pi, ACPHY_RfseqTrigger, trigger_mask);
	SPINWAIT((phy_reg_read(pi, ACPHY_RfseqStatus0) & status_mask), ACPHY_SPINWAIT_RFSEQ_FORCE);
	ASSERT((phy_reg_read(pi, ACPHY_RfseqStatus0) & status_mask) == 0);

	/* Restore */
	phy_reg_write(pi, ACPHY_RfseqMode, orig_RfseqCoreActv);
	phy_reg_write(pi, ACPHY_RxFeCtrl1, orig_rxfectrl1);


	if (ACREV1X1_IS(pi->pubpi.phy_rev))
		return;

	ASSERT((phy_reg_read(pi, ACPHY_RfseqStatus0) & status_mask) == 0);
}


/*
gmult_rc (24:17), gmult(16:9), bq1_bw(8:6), rc_bw(5:3), bq0_bw(2:0)
LO: (15:0), HI (24:16)
mode_mask = bits[0:8] = 11b_20, 11n_20, 11ag_11ac_20, 11b_40, 11n_40, 11ag_11ac_40, 11b_80,
11n_11ag_11ac_80, samp_play
*/
void wlc_phy_set_analog_tx_lpf(phy_info_t *pi, uint16 mode_mask, int bq0_bw, int bq1_bw,
                               int rc_bw, int gmult, int gmult_rc, int core_num)
{
	uint8 ctr, core, max_modes = 9;
	uint16 addr_lo_offs[] = {0x142, 0x152, 0x162};
	uint16 addr_hi_offs[] = {0x362, 0x372, 0x382};
	uint16 addr_lo_base, addr_hi_base, addr_lo, addr_hi;
	uint16 val_lo, val_hi;
	uint32 val;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	/* core_num = -1 ==> all cores */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		if ((core_num == -1) || (core_num == core)) {
			addr_lo_base = addr_lo_offs[core];
			addr_hi_base = addr_hi_offs[core];
			for (ctr = 0; ctr < max_modes; ctr++) {
				if ((mode_mask >> ctr) & 1) {
					addr_lo = addr_lo_base + ctr;
					addr_hi = addr_hi_base + ctr;
					wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ,
					                         1, addr_lo, 16, &val_lo);
					wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ,
					                         1, addr_hi, 16, &val_hi);
					val = (val_hi << 16) | val_lo;

					if (bq0_bw >= 0) {
						val = (val & 0x1fffff8) | (bq0_bw << 0);
						}
					if (rc_bw >= 0) {
						val = (val & 0x1ffffc7) | (rc_bw << 3);
					}
					if (bq1_bw >= 0) {
						val = (val & 0x1fffe3f) | (bq1_bw << 6);
					}
					if (gmult >= 0) {
						val = (val & 0x1fe01ff) | (gmult << 9);
					}
					if (gmult_rc >= 0) {
						val = (val & 0x001ffff) | (gmult_rc << 17);
					}

					val_lo = val & 0xffff;
					val_hi = (val >> 16) & 0x1ff;
					wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ,
					                          1, addr_lo, 16, &val_lo);
					wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ,
					                          1, addr_hi, 16, &val_hi);
				}
			}
		}
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}


/*
gmult_rc (24:17), rc_bw(16:14), gmult(13:6), bq1_bw(5:3), bq0_bw(2:0)
LO: (15:0), HI (24:16)
mode_mask = bits[0:2] = 20, 40, 80
*/
void
wlc_phy_set_analog_rx_lpf(phy_info_t *pi, uint8 mode_mask, int bq0_bw, int bq1_bw,
                          int rc_bw, int gmult, int gmult_rc, int core_num)
{
	uint8 ctr, core, max_modes = 3;
	uint16 addr20_lo_offs[] = {0x140, 0x150, 0x160};
	uint16 addr20_hi_offs[] = {0x360, 0x370, 0x380};
	uint16 addr40_lo_offs[] = {0x141, 0x151, 0x161};
	uint16 addr40_hi_offs[] = {0x361, 0x371, 0x381};
	uint16 addr80_lo_offs[] = {0x441, 0x443, 0x445};
	uint16 addr80_hi_offs[] = {0x440, 0x442, 0x444};
	uint16 addr_lo, addr_hi;
	uint16 val_lo, val_hi;
	uint32 val;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	/* core_num = -1 ==> all cores */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		if ((core_num == -1) || (core_num == core)) {
			for (ctr = 0; ctr < max_modes; ctr++) {
				if ((mode_mask >> ctr) & 1) {
					if (ctr == 0) {
						addr_lo = addr20_lo_offs[core];
						addr_hi = addr20_hi_offs[core];
					}
					else if (ctr == 1) {
						addr_lo = addr40_lo_offs[core];
						addr_hi = addr40_hi_offs[core];
					} else {
						addr_lo = addr80_lo_offs[core];
						addr_hi = addr80_hi_offs[core];
					}

					wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ,
					                         1, addr_lo, 16, &val_lo);
					wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ,
					                         1, addr_hi, 16, &val_hi);
					val = (val_hi << 16) | val_lo;

					if (bq0_bw >= 0) {
						val = (val & 0x1fffff8) | (bq0_bw << 0);
					}
					if (bq1_bw >= 0) {
						val = (val & 0x1ffffc7) | (bq1_bw << 3);
					}
					if (gmult >= 0) {
						val = (val & 0x1ffc03f) | (gmult << 6);
					}
					if (rc_bw >= 0) {
						val = (val & 0x1fe3fff) | (rc_bw << 14);
					}
					if (gmult_rc >= 0) {
						val = (val & 0x001ffff) | (gmult_rc << 17);
					}

					val_lo = val & 0xffff;
					val_hi = (val >> 16) & 0x1ff;
					wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
					                          addr_lo, 16, &val_lo);
					wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
					                          addr_hi, 16, &val_hi);
				}
			}
		}
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}

/*
dacbuf_fixed_cap[5], dacbuf_cap[4:0]
mode_mask = bits[0:8] = 11b_20, 11n_20, 11ag_11ac_20, 11b_40, 11n_40, 11ag_11ac_40, 11b_80,
11n_11ag_11ac_80, samp_play
*/
void wlc_phy_set_tx_afe_dacbuf_cap(phy_info_t *pi, uint16 mode_mask, int dacbuf_cap,
                                   int dacbuf_fixed_cap, int core_num)
{
	uint8 ctr, core, max_modes = 9;
	uint16 core_base[] = {0x3f0, 0x60, 0xd0};
	uint8 offset[] = {0xb, 0xb, 0xc, 0xc, 0xe, 0xe, 0xf, 0xf, 0xa};
	uint8 shift[] = {0, 6, 0, 6, 0, 6, 0, 6, 0};
	uint16 addr, read_val, val;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	/* core_num = -1 ==> all cores */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		if ((core_num == -1) || (core_num == core)) {
			for (ctr = 0; ctr < max_modes; ctr++) {
				if ((mode_mask >> ctr) & 1) {
					addr = core_base[core] + offset[ctr];
					wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ,
					                         1, addr, 16, &read_val);
					val = (read_val >> shift[ctr]) & 0x3f;

					if (dacbuf_cap >= 0) {
							val = (val & 0x20) | dacbuf_cap;
					}
					if (dacbuf_fixed_cap >= 0) {
						val = (val & 0x1f) |
						        (dacbuf_fixed_cap << 5);
					}

					if (shift[ctr] == 0) {
						val = (read_val & 0xfc0) | val;
					} else {
						val = (read_val & 0x3f) | (val << 6);
					}

					wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ,
					                          1, addr, 16, &val);
				}
			}
		}
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}


static void
wlc_phy_get_tx_bbmult_acphy(phy_info_t *pi, uint16 *bb_mult, uint16 core)
{
	uint16 tbl_ofdm_offset[] = { 99, 103, 107, 111};
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_IQLOCAL, 1,
	                         tbl_ofdm_offset[core], 16,
	                         bb_mult);
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_set_tx_bbmult_acphy(phy_info_t *pi, uint16 *bb_mult, uint16 core)
{
	uint16 tbl_ofdm_offset[] = { 99, 103, 107, 111};
	uint16 tbl_bphy_offset[] = {115, 119, 123, 127};
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IQLOCAL, 1,
	                          tbl_ofdm_offset[core], 16,
	                          bb_mult);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IQLOCAL, 1,
	                          tbl_bphy_offset[core], 16,
	                          bb_mult);
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_farrow_setup_acphy(phy_info_t *pi, chanspec_t chanspec)
{
#ifndef ACPHY_1X1_ONLY
	phy_info_acphy_t *pi_ht = (phy_info_acphy_t *)pi->u.pi_acphy;
#endif
	uint16 channel = CHSPEC_CHANNEL(chanspec);
	chan_info_rx_farrow *rx_farrow = NULL;
	chan_info_tx_farrow *tx_farrow = NULL;
	uint16 regval;
	int bw_idx = 0;
	int tbl_idx = 0;
	bool found = FALSE;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (ISSIM_ENAB(pi->sh->sih)) {
		/* Use channel 7(2g)/151(5g) settings for Quickturn */
		if (CHSPEC_IS2G(chanspec)) {
			channel = 7;
		} else {
			channel = 151;
		}
	}

	bw_idx = CHSPEC_IS20(chanspec)? 0 : (CHSPEC_IS40(chanspec)? 1 : 2);

	/* Find the Rx Farrow settings in the table for the specific b/w and channel */
	for (tbl_idx = 0; tbl_idx < ACPHY_NUM_CHANS; tbl_idx++) {
		rx_farrow = &rx_farrow_tbl[bw_idx][tbl_idx];

		if (rx_farrow->chan == channel) {
			/* Setup the Rx Farrow */
			phy_reg_write(pi, ACPHY_rxFarrowDeltaPhase_lo, rx_farrow->deltaphase_lo);
			phy_reg_write(pi, ACPHY_rxFarrowDeltaPhase_hi, rx_farrow->deltaphase_hi);
			phy_reg_write(pi, ACPHY_rxFarrowDriftPeriod, rx_farrow->drift_period);
			phy_reg_write(pi, ACPHY_rxFarrowCtrl, rx_farrow->farrow_ctrl);

			/* Use the same settings for the loopback Farrow */
			phy_reg_write(pi, ACPHY_lbFarrowDeltaPhase_lo, rx_farrow->deltaphase_lo);
			phy_reg_write(pi, ACPHY_lbFarrowDeltaPhase_hi, rx_farrow->deltaphase_hi);
			phy_reg_write(pi, ACPHY_lbFarrowDriftPeriod, rx_farrow->drift_period);
			phy_reg_write(pi, ACPHY_lbFarrowCtrl, rx_farrow->farrow_ctrl);

			found = TRUE;
			break;
		}
	}

	if (! found) {
		PHY_ERROR(("wl%d: %s: Failed to find Rx Farrow settings for bw=%d, channel=%d\n",
		    pi->sh->unit, __FUNCTION__, CHSPEC_BW(chanspec), channel));
		return;
	}

	/* No need to iterate through the Tx Farrow table, since the channels have the same order
	 * as the Rx Farrow table
	 */
#ifdef ACPHY_1X1_ONLY
	ASSERT(((phy_info_acphy_t *)pi->u.pi_acphy)->dac_mode == 1);
	tx_farrow = &tx_farrow_dac1_tbl[bw_idx][tbl_idx];
#else
	switch (pi_ht->dac_mode) {
	case 1:
		tx_farrow = &tx_farrow_dac1_tbl[bw_idx][tbl_idx];
		break;
	case 2:
		tx_farrow = &tx_farrow_dac2_tbl[bw_idx][tbl_idx];
		break;
	case 3:
		tx_farrow = &tx_farrow_dac3_tbl[bw_idx][tbl_idx];
		break;
	default:
		/* default to dac_mode 1 */
		tx_farrow = &tx_farrow_dac1_tbl[bw_idx][tbl_idx];
		break;
	}
#endif /* ACPHY_1X1_ONLY */

	ACPHYREG_BCAST(pi, ACPHY_TxResamplerMuDelta0l, tx_farrow->MuDelta_l);
	ACPHYREG_BCAST(pi, ACPHY_TxResamplerMuDelta0u, tx_farrow->MuDelta_u);
	ACPHYREG_BCAST(pi, ACPHY_TxResamplerMuDeltaInit0l, tx_farrow->MuDeltaInit_l);
	ACPHYREG_BCAST(pi, ACPHY_TxResamplerMuDeltaInit0u, tx_farrow->MuDeltaInit_u);

	/* Enable the Tx resampler on all cores */
	regval = phy_reg_read(pi, ACPHY_TxResamplerEnable0);
	regval |= (1 < ACPHY_TxResamplerEnable0_enable_tx_SHIFT);
	ACPHYREG_BCAST(pi, ACPHY_TxResamplerEnable0,  regval);
}

uint16
wlc_phy_classifier_acphy(phy_info_t *pi, uint16 mask, uint16 val)
{
	uint16 curr_ctl, new_ctl;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Turn on/off classification (bphy, ofdm, and wait_ed), mask and
	 * val are bit fields, bit 0: bphy, bit 1: ofdm, bit 2: wait_ed;
	 * for types corresponding to bits set in mask, apply on/off state
	 * from bits set in val; if no bits set in mask, simply returns
	 * current on/off state.
	 */
	curr_ctl = phy_reg_read(pi, ACPHY_ClassifierCtrl);

	new_ctl = (curr_ctl & (~mask)) | (val & mask);

	phy_reg_write(pi, ACPHY_ClassifierCtrl, new_ctl);

	return new_ctl;
}

/* see acphyproc.tcl acphy_tx_idx */
void
wlc_phy_txpwr_by_index_acphy(phy_info_t *pi, uint8 core_mask, int8 txpwrindex)
{
	uint16 core;
	txgain_setting_t txgain_settings;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Set tx power based on an input "index"
	 * (Emulate what HW power control would use for a given table index)
	 */

	FOREACH_ACTV_CORE(pi, core_mask, core) {

		/* Check txprindex >= 0 */
		if (txpwrindex < 0)
			ASSERT(0); /* negative index not supported */

		/* Read tx gain table */
		wlc_phy_get_txgain_settings_by_index_acphy(pi, &txgain_settings, txpwrindex);

		/* Override gains: DAC, Radio and BBmult */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
		                          (0x100 + core), 16, &(txgain_settings.rad_gain));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
		                          (0x103 + core), 16, &(txgain_settings.rad_gain_mi));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
		                          (0x106 + core), 16, &(txgain_settings.rad_gain_hi));

		wlc_phy_set_tx_bbmult_acphy(pi, &txgain_settings.bbmult, core);


		PHY_TXPWR(("wl%d: %s: Fixed txpwrindex for core%d is %d\n",
		          pi->sh->unit, __FUNCTION__, core, txpwrindex));

		/* Update the per-core state of power index */
		pi->u.pi_acphy->txpwrindex[core] = txpwrindex;
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}

uint16
wlc_phy_set_txpwr_by_index_acphy(phy_info_t *pi, uint8 core_mask, int8 txpwrindex)
{
	uint16 core;
	txgain_setting_t txgain_settings;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Set tx power based on an input "index"
	 * (Emulate what HW power control would use for a given table index)
	 */

	FOREACH_ACTV_CORE(pi, core_mask, core) {
		/* Check txprindex >= 0 */
		if (txpwrindex < 0)
			ASSERT(0); /* negative index not supported */

		/* Read tx gain table */
		wlc_phy_get_txgain_settings_by_index_acphy(pi, &txgain_settings, txpwrindex);

		/* Override gains: DAC, Radio and BBmult */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
		                          (0x100 + core), 16, &(txgain_settings.rad_gain));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
		                          (0x103 + core), 16, &(txgain_settings.rad_gain_mi));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
		                          (0x106 + core), 16, &(txgain_settings.rad_gain_hi));

		wlc_phy_set_tx_bbmult_acphy(pi, &txgain_settings.bbmult, core);


		PHY_TXPWR(("wl%d: %s: Fixed txpwrindex for core%d is %d\n",
		          pi->sh->unit, __FUNCTION__, core, txpwrindex));
	}
	ACPHY_ENABLE_STALL(pi, stall_val);

	return txgain_settings.bbmult;
}

static void
wlc_phy_get_txgain_settings_by_index_acphy(phy_info_t *pi, txgain_setting_t *txgain_settings,
                                     int8 txpwrindex)
{
	uint16 txgain[3];
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_GAINCTRLBBMULTLUTS, 1, txpwrindex, 48, &txgain);

	txgain_settings->rad_gain    = ((txgain[0] >> 8) & 0xff) + ((txgain[1] & 0xff) << 8);
	txgain_settings->rad_gain_mi = ((txgain[1] >> 8) & 0xff) + ((txgain[2] & 0xff) << 8);
	txgain_settings->rad_gain_hi = ((txgain[2] >> 8) & 0xff);
	txgain_settings->bbmult      = (txgain[0] & 0xff);
	ACPHY_ENABLE_STALL(pi, stall_val);
}

void
wlc_phy_read_txgain_acphy(phy_info_t *pi)
{
	uint16 core;
	uint8 stall_val;
	txgain_setting_t txcal_txgain[3];

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* store off orig tx radio gain */
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x100 + core), 16,
		                         &(txcal_txgain[core].rad_gain));
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x103 + core), 16,
		                         &(txcal_txgain[core].rad_gain_mi));
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x106 + core), 16,
		                         &(txcal_txgain[core].rad_gain_hi));
		wlc_phy_get_tx_bbmult_acphy(pi, &(txcal_txgain[core].bbmult),  core);
		printf("\n radio gain = 0x%x%x%x, bbm=%d, dacgn = %d  \n",
			txcal_txgain[core].rad_gain_hi,
			txcal_txgain[core].rad_gain_mi,
			txcal_txgain[core].rad_gain,
			txcal_txgain[core].bbmult,
			txcal_txgain[core].dac_gain);
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_txcal_txgain_setup_acphy(phy_info_t *pi, txgain_setting_t *txcal_txgain,
	txgain_setting_t *orig_txgain)
{
	uint16 core;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* store off orig and set new tx radio gain */
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x100 + core), 16,
		                         &(orig_txgain[core].rad_gain));
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x103 + core), 16,
		                         &(orig_txgain[core].rad_gain_mi));
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x106 + core), 16,
		                         &(orig_txgain[core].rad_gain_hi));

		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x100 + core), 16,
		                          &(txcal_txgain[core].rad_gain));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x103 + core), 16,
		                          &(txcal_txgain[core].rad_gain_mi));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x106 + core), 16,
		                          &(txcal_txgain[core].rad_gain_hi));

		PHY_NONE(("\n radio gain = 0x%x%x%x, bbm=%d, dacgn = %d  \n",
			txcal_txgain[core].rad_gain_hi,
			txcal_txgain[core].rad_gain_mi,
			txcal_txgain[core].rad_gain,
			txcal_txgain[core].bbmult,
			txcal_txgain[core].dac_gain));

		/* store off orig and set new bbmult gain */
		wlc_phy_get_tx_bbmult_acphy(pi, &(orig_txgain[core].bbmult),  core);
		wlc_phy_set_tx_bbmult_acphy(pi, &(txcal_txgain[core].bbmult), core);
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_txcal_txgain_cleanup_acphy(phy_info_t *pi, txgain_setting_t *orig_txgain)
{
	uint8 core;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* restore gains: DAC, Radio and BBmult */

		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x100 + core), 16,
		                          &(orig_txgain[core].rad_gain));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x103 + core), 16,
		                          &(orig_txgain[core].rad_gain_mi));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x106 + core), 16,
		                          &(orig_txgain[core].rad_gain_hi));

		wlc_phy_set_tx_bbmult_acphy(pi, &(orig_txgain[core].bbmult), core);
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_precal_target_tssi_search(phy_info_t *pi, txgain_setting_t *target_gains)
{
	int8  gain_code_found, delta_threshold, dont_alter_step;
	int16  target_tssi, min_delta, prev_delta, delta_tssi;
	int16  idle_tssi[PHY_CORE_MAX] = {0};
	uint8  tx_idx;
	int16  tone_tssi[PHY_CORE_MAX] = {0};
	int16  tssi[PHY_CORE_MAX] = {0};

	int16  pad_gain_step, curr_pad_gain, pad_gain;

	txgain_setting_t orig_txgain[PHY_CORE_MAX];
	int16  sat_count, sat_threshold, sat_delta, ct;
	int16 temp_val;
	int16 tx_idx_step, pad_step_size, pad_iteration_count;

	/* prevent crs trigger */
	wlc_phy_stay_in_carriersearch_acphy(pi, TRUE);

	/* Set the target TSSIs for different bands/Bandwidth cases.
	 * These numbers are arrived by running the TCL proc:
	 * "get_target_tssi_for_iqlocal" for a representative channel
	 * by sending a tone at a chosen Tx gain which gives best
	 * Image/LO rejection at room temp
	 */

	if (CHSPEC_IS5G(pi->radio_chanspec) == 1) {
		if (CHSPEC_IS80(pi->radio_chanspec)) {
			target_tssi = 900;
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			target_tssi = 900;
		} else {
			target_tssi = 950;
		}
	} else {
		if (CHSPEC_IS40(pi->radio_chanspec)) {
			target_tssi = 913;
		} else {
			target_tssi = 950;
		}

	}

	wlc_phy_tssi_phy_setup_acphy(pi);

	/* The correct fix should be:
	 *   wlc_phy_tssi_radio_setup_acphy(pi, pi->sh->phyrxchain);
	 * Temporarily delay this correct fix till further verification
	 */
	if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5)) {
		wlc_phy_tssi_radio_setup_acphy(pi, 1);
	} else {
		wlc_phy_tssi_radio_setup_acphy(pi, 7);
	}

	gain_code_found = 0;

	/* delta_threshold is the minimum tolerable difference between
	 * target tssi and the measured tssi. This was determined by experimental
	 * observations. delta_tssi ( target_tssi - measured_tssi ) values upto
	 * 15 are found to give identical performance in terms of Tx EVM floor
	 * when compared to delta_tssi values upto 10. Threshold value of 15 instead
	 * of 10 will cut down the algo time as the algo need not search for
	 * index to meet delta of 10.
	 */
	delta_threshold = 15;

	min_delta = 1024;
	prev_delta = 1024;

	/* Measure the Idle TSSI */
	wlc_phy_poll_samps_WAR_acphy(pi, idle_tssi, TRUE, TRUE, target_gains);

	/* Measure the tone TSSI before start searching */
	tx_idx = 0;
	wlc_phy_txpwr_by_index_acphy(pi, 1, tx_idx);

	wlc_phy_get_txgain_settings_by_index_acphy(
				pi, target_gains, tx_idx);

	/* Save the original Gain code */
	wlc_phy_txcal_txgain_setup_acphy(pi, target_gains, &orig_txgain[0]);

	PHY_TRACE(("radio gain = 0x%x%x%x, bbm=%d, dacgn = %d  \n",
		target_gains->rad_gain_hi,
		target_gains->rad_gain_mi,
		target_gains->rad_gain,
		target_gains->bbmult,
		target_gains->dac_gain));

	wlc_phy_poll_samps_WAR_acphy(pi, tssi, TRUE, FALSE, target_gains);

	tone_tssi[0] = tssi[0] - idle_tssi[0];

	delta_tssi = target_tssi - tone_tssi[0];

	PHY_TRACE(("Index = %3d target_TSSI = %4i tone_TSSI = %4i"
			"delta_TSSI = %4i min_delta = %4i\n",
			tx_idx, target_tssi, tone_tssi[0], delta_tssi, min_delta));

	PHY_TRACE(("*********** Search Control loop begins now ***********\n"));

	/* When the measured tssi saturates and is unable to meet
	 * the target tssi, there is no point in continuing search
	 * for the next higher PAD gain. The variable 'sat_count'
	 * is the threshold which will control when to stop the search.
	 * change in PAD gain code by "10" ticks should atleast translate
	 * to 1dBm of power level change when not saturated. When the
	 * measured tssi is saturated, this doesnt hold good and we
	 * need to break out.
	 */
	sat_count = 10;
	sat_threshold = 20;

	/* delta_tssi > 0 ==> target_tssi is greater than tone tssi and
	 * hence we have to increase the PAD gain as the inference was
	 * drawn by measuring the tone tssi at index 0
	 */

	if (delta_tssi > 0) {
		PHY_TRACE(("delta_tssi > 0 ==> target_tssi is greater than tone tssi and\n"));
		PHY_TRACE(("hence we have to increase the PAD gain as the inference was\n"));
		PHY_TRACE(("drawn by measuring the tone tssi at index 0\n"));

		tx_idx = 0;
		wlc_phy_txpwr_by_index_acphy(pi, 1, tx_idx);

		wlc_phy_get_txgain_settings_by_index_acphy(
				pi, target_gains, tx_idx);

		min_delta = 1024;
		prev_delta = 1024;

		sat_delta = 0;
		ct = 0;

		curr_pad_gain = target_gains->rad_gain_mi & 0x00ff;

		PHY_TRACE(("Current PAD Gain (Before Search) is %d\n", curr_pad_gain));
		pad_gain_step = 1;

		for (pad_gain = curr_pad_gain; pad_gain <= MAX_PAD_GAIN; pad_gain += pad_gain_step)
		{
			target_gains->rad_gain_mi = target_gains->rad_gain_mi & 0xff00;
			target_gains->rad_gain_mi |= pad_gain;

			PHY_TRACE(("Current PAD Gain is %d\n", pad_gain));

			wlc_phy_poll_samps_WAR_acphy(pi, tssi, TRUE, FALSE, target_gains);
					tone_tssi[0] = tssi[0] - idle_tssi[0];

			delta_tssi = target_tssi - tone_tssi[0];

			/* Manipulate the step size to cut down the search time */
			if (delta_tssi > 50) {
				pad_gain_step = 10;
			} else if (delta_tssi > 30) {
				pad_gain_step = 5;
			} else if (delta_tssi > 15) {
				pad_gain_step = 2;
			} else {
				pad_gain_step = 1;
			}

			/* Check for TSSI Saturation */
			if (ct == 0) {
				sat_delta = delta_tssi;
			} else {
				sat_delta = delta_tssi - prev_delta;
				sat_delta = ABS(sat_delta);

				PHY_TRACE(("Ct=%d sat_delta=%d delta_tssi=%d sat_delta=%d\n",
					ct, sat_delta, delta_tssi, sat_delta));
			}

			if (sat_delta > sat_threshold) {
				ct = 0;
			}

			if ((ct == sat_count) && (sat_delta < sat_threshold)) {

				PHY_TRACE(("Ct = %d\t sat_delta = %d \t "
						"sat_threshold = %d\n",
						ct, sat_delta, sat_threshold));

				gain_code_found = 0;

				PHY_TRACE(("Breaking out of search as TSSI "
						" seems to have saturated\n"));
				WLC_PHY_PRECAL_TRACE(tx_idx, target_gains);

				break;
			}

			ct = ct + 1;

			PHY_TRACE(("Index = %3d target_TSSI = %4i tone_TSSI = %4i"
					"delta_TSSI = %4i min_delta = %4i radio gain = 0x%x%x%x, "
					"bbm=%d, dacgn = %d\n", tx_idx,
					target_tssi, tone_tssi[0], delta_tssi, min_delta,
					target_gains->rad_gain_hi, target_gains->rad_gain_mi,
					target_gains->rad_gain, target_gains->bbmult,
					target_gains->dac_gain));

			temp_val = ABS(delta_tssi);
			if (temp_val <= min_delta) {
				min_delta = ABS(delta_tssi);

				if (min_delta <= delta_threshold) {
					gain_code_found	= 1;

					PHY_TRACE(("Breaking out of search as min delta"
							" tssi threshold conditions are met\n"));

					WLC_PHY_PRECAL_TRACE(tx_idx, target_gains);

					break;
				}
			}
			prev_delta = delta_tssi;
		}

		if (gain_code_found == 0) {
			PHY_TRACE(("*** Search failed Again ***\n"));
		}

	/* delta_tssi < 0 ==> target tssi is less than tone tssi and we have to reduce the gain */
	} else {

		PHY_TRACE(("delta_tssi < 0 ==> target tssi is less than"
				"tone tssi and we have to reduce the gain\n"));

		tx_idx_step = 1;
		dont_alter_step = 0;
		pad_step_size = 0;
		pad_iteration_count = 0;

		sat_delta = 0;
		ct = 0;

		for (tx_idx = 0; tx_idx <= MAX_TX_IDX; tx_idx +=  tx_idx_step) {
			wlc_phy_txpwr_by_index_acphy(pi, 1, tx_idx);

			wlc_phy_get_txgain_settings_by_index_acphy(
					pi, &(target_gains[0]), tx_idx);

			if (pad_step_size != 0) {
				curr_pad_gain = target_gains->rad_gain_mi & 0x00ff;
				curr_pad_gain = curr_pad_gain -
				(pad_iteration_count * pad_step_size);

				target_gains->rad_gain_mi =
				target_gains->rad_gain_mi & 0xff00;

				target_gains->rad_gain_mi |= curr_pad_gain;
			}

			wlc_phy_poll_samps_WAR_acphy(pi, tssi, TRUE, FALSE, &(target_gains[0]));
			tone_tssi[0] = tssi[0] - idle_tssi[0];

			delta_tssi = target_tssi - tone_tssi[0];

			PHY_TRACE(("Index = %3d target_TSSI = %4i "
					"tone_TSSI = %4i delta_TSSI = %4i min_delta = %4i "
					"radio gain = 0x%x%x%x, bbm=%d, dacgn = %d\n", tx_idx,
					target_tssi, tone_tssi[0], delta_tssi, min_delta,
					target_gains->rad_gain_hi, target_gains->rad_gain_mi,
					target_gains->rad_gain,
					target_gains->bbmult, target_gains->dac_gain));

			/* Check for TSSI Saturation */
			if (ct == 0) {
				sat_delta = delta_tssi;
			} else {
				sat_delta = delta_tssi - prev_delta;
				sat_delta = ABS(sat_delta);

				PHY_TRACE(("Ct=%d sat_delta=%d delta_tssi=%d sat_delta=%d\n",
					ct, sat_delta, delta_tssi, sat_delta));
			}

			if (sat_delta > sat_threshold) {
				ct = 0;
			}

			if ((ct == sat_count) && (sat_delta < sat_threshold) &&
				(ABS(delta_tssi) < sat_threshold)) {

				PHY_TRACE(("Ct = %d\t sat_delta = %d \t sat_threshold = %d\n",
					ct, sat_delta, sat_threshold));

				gain_code_found	= 0;

				PHY_TRACE(("Breaking out of search as TSSI "
						" seems to have saturated\n"));

				WLC_PHY_PRECAL_TRACE(tx_idx, target_gains);

				break;
			}

			ct = ct + 1;


			temp_val = ABS(delta_tssi);
			if (temp_val <= min_delta) {
				min_delta = ABS(delta_tssi);

				if (min_delta <= delta_threshold) {
					gain_code_found	= 1;

					PHY_TRACE(("Breaking out of search "
							"as min delta tssi threshold "
							"conditions are met\n"));

					WLC_PHY_PRECAL_TRACE(tx_idx, target_gains);

					PHY_TRACE(("===== IQLOCAL PreCalGainControl: END =====\n"));
					break;
				}
			}

			/* Change of sign in delta tssi => increase
			 * the step size with smaller resolution
			 */

			if ((prev_delta < 0) && (delta_tssi > 0)&& (tx_idx != 0))
			{
				PHY_TRACE(("Scenario 2 -- BELOW TARGET\n"));
				/* Now that tx idx is sufficiently dropped ,
				 * there is change in sign of the delta tssi.
				 * implies, now target tssi is more than tone tssi.
				 * So increase the gain in very small steps
				 * by decrementing the index
				 */
				tx_idx_step = -1;
				dont_alter_step = 1;
			} else if ((prev_delta < 0) && (delta_tssi < 0) && (dont_alter_step == 1)) {
				PHY_TRACE(("Scenario 3 --  OSCILLATORY\n"));

				/* this case is to take care of the oscillatory
				 * behaviour of the tone tssi about the target
				 * tssi. Here tone tssi has again
				 * overshot the target tssi. So donot change the
				 * tx gain index, but reduce the PAD gain
				 */
				tx_idx_step = 0;
				pad_step_size = 1;
				pad_iteration_count += 1;

			} else {
				PHY_TRACE(("Scenario 1 -- NORMAL\n"));
				/* tone tssi is more than target tssi.
				 * So increase the index and hence reduce the gain
				 */
				if (dont_alter_step == 0) {
					/* Manipulate the step size to cut down the search time */
					if (delta_tssi >= 50) {
						tx_idx_step = 5;
					} else if (delta_tssi >= 25) {
						tx_idx_step = 3;
					} else if (delta_tssi >= 10) {
						tx_idx_step = 2;
					} else {
						tx_idx_step = 1;
					}
				}
			}
			prev_delta = delta_tssi;
		}

	}

	/* Search found the right gain code meeting required tssi conditions */
	if (gain_code_found == 1) {

		PHY_TRACE(("******* SUMMARY *******\n"));
		WLC_PHY_PRECAL_TRACE(tx_idx, target_gains);

		PHY_TRACE(("Measured TSSI Value is %d\n", tone_tssi[0]));
		PHY_TRACE(("***********************\n"));
	}
	/* prevent crs trigger */
	wlc_phy_stay_in_carriersearch_acphy(pi, FALSE);
	PHY_TRACE(("======= IQLOCAL PreCalGainControl : END =======\n"));

	/* Restore the original Gain code */
	wlc_phy_txcal_txgain_cleanup_acphy(pi, &orig_txgain[0]);

	return;
}

static void
wlc_phy_precal_txgain_acphy(phy_info_t *pi, txgain_setting_t *target_gains)
{
	/*   This function determines the tx gain settings to be
	 *   used during tx iqlo calibration; that is, it sends back
	 *   the following settings for each core:
	 *       - radio gain
	 *       - dac gain
	 *       - bbmult
	 *   This is accomplished by choosing a predefined power-index, or by
	 *   setting gain elements explicitly to predefined values, or by
	 *   doing actual "pre-cal gain control". Either way, the idea is
	 *   to get a stable setting for which the swing going into the
	 *   envelope detectors is large enough for good "envelope ripple"
	 *   while avoiding distortion or EnvDet overdrive during the cal.
	 *
	 *   Note:
	 *       - this function and the calling infrastructure is set up
	 *         in a way not to leave behind any modified state; this
	 *         is in contrast to mimophy ("nphy"); in acphy, only the
	 *         desired gain quantities are set/found and set back
	 */

	uint8 core;
	uint8 phy_bw;
	acphy_cal_result_t *accal = &pi->cal_info->u.accal;

	uint8 en_precal_gain_control = 0;
	int8 tx_pwr_idx[3] = {25, 20, 30};

	/* reset ladder_updated flags so tx-iqlo-cal ensures appropriate recalculation */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		accal->txiqlocal_ladder_updated[core] = 0;
	}
	/* phy_bw */
	if (CHSPEC_IS80(pi->radio_chanspec)) {
		phy_bw = 80;
	} else if (CHSPEC_IS40(pi->radio_chanspec)) {
		phy_bw = 40;
	} else {
		phy_bw = 20;
	}

	/* get target tx gain settings */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		if (1) {
			/* specify tx gain by index (reads from tx power table) */
			int8 target_pwr_idx;
			if (RADIOREV(pi->pubpi.radiorev) == 4) {
				/* for 4360B0 using 0.5dB-step, idx is lower */
				target_pwr_idx = 20;
				if (ACREV_IS(pi->pubpi.phy_rev, 1) &&
				        (wlc_phy_get_chan_freq_range_acphy(pi, 0) ==
				        WL_CHAN_FREQ_RANGE_5G_BAND3)) {
					target_pwr_idx = tx_pwr_idx[core];
				}
			} else if (ACRADIO_IPA1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
				if (CHSPEC_IS2G(pi->radio_chanspec) == 1) {
					target_pwr_idx = 1;
				} else {
					if (phy_bw == 20)
						target_pwr_idx = 0;
					else if (phy_bw == 40)
						target_pwr_idx = 15;
					else
						target_pwr_idx = 10;
				}
			} else {
				target_pwr_idx = 30;
			}

			wlc_phy_get_txgain_settings_by_index_acphy(
				pi, &(target_gains[core]), target_pwr_idx);

			/******************************************************
			 * Start: Pre cal gain control for target tssi search *
			 ******************************************************
			 */
			/* Disable Precal gctrl for 4335 until further testing for stability */
			if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5)) {
				en_precal_gain_control = 0;
			}

			if (en_precal_gain_control == 1) {
				PHY_TRACE(("========= Calling precal gain control =========\n"));
				wlc_phy_precal_target_tssi_search(pi, &(target_gains[core]));
			}

			/* ****************************************************
			 *  End: Pre cal gain control for target tssi search
			 * ****************************************************
			 */

			if ((CHSPEC_IS5G(pi->radio_chanspec) == 1) &&
			    (
			    CHIPID(pi->sh->chip) == BCM4335_CHIP_ID)) {
				/* use PAD gain 255 for TXIQLOCAL */
				target_gains[core].rad_gain_mi |= 0xff;
			}

		} else if (0) {
			/* specify tx gain as hardcoded explicit gain */
			target_gains[core].rad_gain  = 0x000;
			target_gains[core].dac_gain  = 0;
			target_gains[core].bbmult = 64;
		} else {
			/* actual precal gain control */
		}
	}
}

static void
wlc_phy_loadsampletable_acphy(phy_info_t *pi, cint32 *tone_buf, uint16 num_samps)
{
	uint16 t;
	uint32* data_buf = NULL;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	/* allocate buffer */
	if ((data_buf = (uint32 *)MALLOC(pi->sh->osh, sizeof(uint32) * num_samps)) == NULL) {
		PHY_ERROR(("wl%d: %s: out of memory, malloced %d bytes", pi->sh->unit,
		           __FUNCTION__, MALLOCED(pi->sh->osh)));
		return;
	}

	/* load samples into sample play buffer */
	for (t = 0; t < num_samps; t++) {
		data_buf[t] = ((((unsigned int)tone_buf[t].i) & 0x3ff) << 10) |
		               (((unsigned int)tone_buf[t].q) & 0x3ff);
	}
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_SAMPLEPLAY, num_samps, 0, 32, data_buf);

	if (data_buf != NULL)
		MFREE(pi->sh->osh, data_buf, sizeof(uint32) * num_samps);
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static uint16
wlc_phy_gen_load_samples_acphy(phy_info_t *pi, int32 f_kHz, uint16 max_val, uint8 mac_based)
{
	uint8 phy_bw;
	uint16 num_samps, t;
	fixed theta = 0, rot = 0;
	uint32 tbl_len;
	cint32* tone_buf = NULL;

	/* check phy_bw */
	if (CHSPEC_IS80(pi->radio_chanspec))
		phy_bw = 80;
	else if (CHSPEC_IS40(pi->radio_chanspec))
		phy_bw = 40;
	else
		phy_bw = 20;
	tbl_len = phy_bw << 1;

	/* allocate buffer */
	if ((tone_buf = (cint32 *)MALLOC(pi->sh->osh, sizeof(cint32) * tbl_len)) == NULL) {
		PHY_ERROR(("wl%d: %s: out of memory, malloced %d bytes", pi->sh->unit,
		          __FUNCTION__, MALLOCED(pi->sh->osh)));
		return 0;
	}

	/* set up params to generate tone */
	num_samps  = (uint16)tbl_len;
	rot = FIXED((f_kHz * 36)/phy_bw) / 100; /* 2*pi*f/bw/1000  Note: f in KHz */
	theta = 0; /* start angle 0 */

	/* tone freq = f_c MHz ; phy_bw = phy_bw MHz ; # samples = phy_bw (1us) */
	for (t = 0; t < num_samps; t++) {
		/* compute phasor */
		wlc_phy_cordic(theta, &tone_buf[t]);
		/* update rotation angle */
		theta += rot;
		/* produce sample values for play buffer */
		tone_buf[t].q = (int32)FLOAT(tone_buf[t].q * max_val);
		tone_buf[t].i = (int32)FLOAT(tone_buf[t].i * max_val);
	}

	/* load sample table */
	wlc_phy_loadsampletable_acphy(pi, tone_buf, num_samps);

	if (tone_buf != NULL)
		MFREE(pi->sh->osh, tone_buf, sizeof(cint32) * tbl_len);

	return num_samps;
}

int
wlc_phy_tx_tone_acphy(phy_info_t *pi, int32 f_kHz, uint16 max_val, uint8 iqmode,
                      uint8 mac_based, bool modify_bbmult)
{
	uint8 core;
	uint16 num_samps;
	uint16 bb_mult;
	uint16 loops = 0xffff;
	uint16 wait = 0;
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (max_val == 0) {
		num_samps = 1;
	} else if ((num_samps = wlc_phy_gen_load_samples_acphy(pi, f_kHz, max_val, mac_based))
	           == 0) {
		return BCME_ERROR;
	}

	if (pi_ac->bb_mult_save_valid == 0) {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			wlc_phy_get_tx_bbmult_acphy(pi, &pi_ac->bb_mult_save[core], core);
		}
		pi_ac->bb_mult_save_valid = 1;
	}

	if (max_val == 0 || modify_bbmult) {
		if (max_val == 0) {
			bb_mult = 0;
		} else {
			if (CHSPEC_IS80(pi->radio_chanspec))
				bb_mult = 64;
			else if (CHSPEC_IS40(pi->radio_chanspec))
				bb_mult = 64;
			else
				bb_mult = 64;
		}
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			wlc_phy_set_tx_bbmult_acphy(pi, &bb_mult, core);
		}
	}

	wlc_phy_runsamples_acphy(pi, num_samps, loops, wait, iqmode, mac_based);

	return BCME_OK;
}

void
wlc_phy_stopplayback_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	uint16 playback_status;
	uint8 core;

	/* check status register */
	playback_status = phy_reg_read(pi, ACPHY_sampleStatus);
	if (playback_status & 0x1) {
		phy_reg_or(pi, ACPHY_sampleCmd, ACPHY_sampleCmd_stop_MASK);
	} else if (playback_status & 0x2) {
		phy_reg_and(pi, ACPHY_iqloCalCmdGctl,
		            (uint16)~ACPHY_iqloCalCmdGctl_iqlo_cal_en_MASK);
	} else {
		PHY_CAL(("wlc_phy_stopplayback_acphy: already disabled\n"));
	}
	/* disable the dac_test mode */
	phy_reg_and(pi, ACPHY_sampleCmd, ~ACPHY_sampleCmd_DacTestMode_MASK);

	/* if bb_mult_save does exist, restore bb_mult and undef bb_mult_save */
	if (pi_ac->bb_mult_save_valid != 0) {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			wlc_phy_set_tx_bbmult_acphy(pi, &pi_ac->bb_mult_save[core], core);
		}
		pi_ac->bb_mult_save_valid = 0;
	}

	wlc_phy_resetcca_acphy(pi);
}

static void
wlc_phy_runsamples_acphy(phy_info_t *pi, uint16 num_samps, uint16 loops, uint16 wait, uint8 iqmode,
                         uint8 mac_based)
{
	uint8  sample_cmd;
	uint16 orig_RfseqCoreActv;
	uint8  dac_test_mode = 0;

	if (mac_based == 1) {
		phy_reg_or(pi, ACPHY_macbasedDACPlay, ACPHY_macbasedDACPlay_macBasedDACPlayEn_MASK);

		if (CHSPEC_IS80(pi->radio_chanspec)) {
			phy_reg_or(pi, ACPHY_macbasedDACPlay,
				ACPHY_macbasedDACPlay_macBasedDACPlayMode_MASK &
				(0x3 << ACPHY_macbasedDACPlay_macBasedDACPlayMode_SHIFT));
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			phy_reg_or(pi, ACPHY_macbasedDACPlay,
				ACPHY_macbasedDACPlay_macBasedDACPlayMode_MASK &
				(0x2 << ACPHY_macbasedDACPlay_macBasedDACPlayMode_SHIFT));
		} else {
			phy_reg_or(pi, ACPHY_macbasedDACPlay,
				ACPHY_macbasedDACPlay_macBasedDACPlayMode_MASK &
				(0x1 << ACPHY_macbasedDACPlay_macBasedDACPlayMode_SHIFT));
		}

		PHY_TRACE(("Starting MAC based Sample Play"));
		wlc_phy_force_rfseq_acphy(pi, ACPHY_RFSEQ_RX2TX);
	} else {
		phy_reg_and(pi, ACPHY_macbasedDACPlay,
		            ~ACPHY_macbasedDACPlay_macBasedDACPlayEn_MASK);

		/* configure sample play buffer */
		phy_reg_write(pi, ACPHY_sampleDepthCount, num_samps-1);

		if (loops != 0xffff) { /* 0xffff means: keep looping forever */
			phy_reg_write(pi, ACPHY_sampleLoopCount, loops - 1);
		} else {
			phy_reg_write(pi, ACPHY_sampleLoopCount, loops);
		}

		/* Wait time should be atleast 60 for farrow FIFO depth to settle
		 * 60 is to support 80mhz mode.
		 * Though 20 is even for 20mhz mode, and 40 for 80mhz mode,
		 * but just giving some extra wait time
		 */
		phy_reg_write(pi, ACPHY_sampleInitWaitCount, (wait > 60) ? wait : 60);

		/* start sample play buffer (in regular mode or iqcal mode) */
		orig_RfseqCoreActv = phy_reg_read(pi, ACPHY_RfseqMode);
		phy_reg_or(pi, ACPHY_RfseqMode, ACPHY_RfseqMode_CoreActv_override_MASK);
		phy_reg_and(pi, ACPHY_sampleCmd, ~ACPHY_sampleCmd_DacTestMode_MASK);
		phy_reg_and(pi, ACPHY_sampleCmd, ~ACPHY_sampleCmd_start_MASK);
		phy_reg_and(pi, ACPHY_iqloCalCmdGctl, 0x3FFF);
		if (iqmode) {
			phy_reg_or(pi, ACPHY_iqloCalCmdGctl, 0x8000);
		} else {
			sample_cmd = ACPHY_sampleCmd_start_MASK;
			sample_cmd |= (dac_test_mode == 1 ? ACPHY_sampleCmd_DacTestMode_MASK : 0);
			phy_reg_or(pi, ACPHY_sampleCmd, sample_cmd);
		}

		/* Wait till the Rx2Tx sequencing is done */
		SPINWAIT(((phy_reg_read(pi, ACPHY_RfseqStatus0) & 0x1) == 1),
		         ACPHY_SPINWAIT_RUNSAMPLE);

		/* restore mimophyreg(RfseqMode.CoreActv_override) */
		phy_reg_write(pi, ACPHY_RfseqMode, orig_RfseqCoreActv);
	}
}

static void
wlc_phy_cal_txiqlo_coeffs_acphy(phy_info_t *pi, uint8 rd_wr, uint16 *coeff_vals,
                                uint8 select, uint8 core) {

	/* handles IQLOCAL coefficients access (read/write from/to
	 * iqloCaltbl and pi State)
	 *
	 * not sure if reading/writing the pi state coeffs via this appraoch
	 * is a bit of an overkill
	 */

	/* {num of 16b words to r/w, start offset (ie address), core-to-core block offset} */
	acphy_coeff_access_t coeff_access_info[] = {
		{2, 64, 8},  /* TB_START_COEFFS_AB   */
		{1, 67, 8},  /* TB_START_COEFFS_D    */
		{1, 68, 8},  /* TB_START_COEFFS_E    */
		{1, 69, 8},  /* TB_START_COEFFS_F    */
		{2, 128, 7}, /*   TB_BEST_COEFFS_AB  */
		{1, 131, 7}, /*   TB_BEST_COEFFS_D   */
		{1, 132, 7}, /*   TB_BEST_COEFFS_E   */
		{1, 133, 7}, /*   TB_BEST_COEFFS_F   */
		{2, 96,  4}, /* TB_OFDM_COEFFS_AB    */
		{1, 98,  4}, /* TB_OFDM_COEFFS_D     */
		{2, 112, 4}, /* TB_BPHY_COEFFS_AB    */
		{1, 114, 4}, /* TB_BPHY_COEFFS_D     */
		{2, 0, 5},   /*   PI_INTER_COEFFS_AB */
		{1, 2, 5},   /*   PI_INTER_COEFFS_D  */
		{1, 3, 5},   /*   PI_INTER_COEFFS_E  */
		{1, 4, 5},   /*   PI_INTER_COEFFS_F  */
		{2, 0, 5},   /* PI_FINAL_COEFFS_AB   */
		{1, 2, 5},   /* PI_FINAL_COEFFS_D    */
		{1, 3, 5},   /* PI_FINAL_COEFFS_E    */
		{1, 4, 5}    /* PI_FINAL_COEFFS_F    */
	};
	acphy_cal_result_t *accal = &pi->cal_info->u.accal;

	uint8 nwords, offs, boffs, k;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	/* get access info for desired choice */
	nwords = coeff_access_info[select].nwords;
	offs   = coeff_access_info[select].offs;
	boffs  = coeff_access_info[select].boffs;

	/* read or write given coeffs */
	if (select <= TB_BPHY_COEFFS_D) { /* START and BEST coeffs in Table */
		if (rd_wr == CAL_COEFF_READ) { /* READ */
			wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_IQLOCAL, nwords,
				offs + boffs*core, 16, coeff_vals);
		} else { /* WRITE */
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IQLOCAL, nwords,
				offs + boffs*core, 16, coeff_vals);
		}
	} else if (select <= PI_INTER_COEFFS_F) { /* PI state intermediate coeffs */
		for (k = 0; k < nwords; k++) {
			if (rd_wr == CAL_COEFF_READ) { /* READ */
				coeff_vals[k] = accal->txiqlocal_interm_coeffs[offs +
				                                               boffs*core + k];
			} else { /* WRITE */
				accal->txiqlocal_interm_coeffs[offs +
				                               boffs*core + k] = coeff_vals[k];
			}
		}
	} else { /* PI state final coeffs */
		for (k = 0; k < nwords; k++) { /* PI state final coeffs */
			if (rd_wr == CAL_COEFF_READ) { /* READ */
				coeff_vals[k] = accal->txiqlocal_coeffs[offs + boffs*core + k];
			} else { /* WRITE */
				accal->txiqlocal_coeffs[offs + boffs*core + k] = coeff_vals[k];
			}
		}
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_cal_txiqlo_update_ladder_acphy(phy_info_t *pi, uint16 bbmult)
{
	uint8  indx;
	uint32 bbmult_scaled;
	uint16 tblentry;
	uint8 stall_val;
	acphy_txiqcal_ladder_t ladder_lo[] = {
	{3, 0}, {4, 0}, {6, 0}, {9, 0}, {13, 0}, {18, 0},
	{25, 0}, {25, 1}, {25, 2}, {25, 3}, {25, 4}, {25, 5},
	{25, 6}, {25, 7}, {35, 7}, {50, 7}, {71, 7}, {100, 7}};

	acphy_txiqcal_ladder_t ladder_iq[] = {
	{3, 0}, {4, 0}, {6, 0}, {9, 0}, {13, 0}, {18, 0},
	{25, 0}, {35, 0}, {50, 0}, {71, 0}, {100, 0}, {100, 1},
	{100, 2}, {100, 3}, {100, 4}, {100, 5}, {100, 6}, {100, 7}};

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	for (indx = 0; indx < 18; indx++) {

		/* calculate and write LO cal gain ladder */
		bbmult_scaled = ladder_lo[indx].percent * bbmult;
		bbmult_scaled /= 100;
		tblentry = ((bbmult_scaled & 0xff) << 8) | ladder_lo[indx].g_env;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IQLOCAL, 1, indx, 16, &tblentry);

		/* calculate and write IQ cal gain ladder */
		bbmult_scaled = ladder_iq[indx].percent * bbmult;
		bbmult_scaled /= 100;
		tblentry = ((bbmult_scaled & 0xff) << 8) | ladder_iq[indx].g_env;
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IQLOCAL, 1, indx+32, 16, &tblentry);
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_txcal_radio_setup_acphy(phy_info_t *pi)
{
	/* This stores off and sets Radio-Registers for Tx-iqlo-Calibration;
	 *
	 * Note that Radio Behavior controlled via RFCtrl is handled in the
	 * phy_setup routine, not here; also note that we use the "shotgun"
	 * approach here ("coreAll" suffix to write to all jtag cores at the
	 * same time)
	 */

	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_txcal_radioregs_t *porig = &(pi_ac->ac_txcal_radioregs_orig);
	uint16 core;

	/* SETUP: set 2059 into iq/lo cal state while saving off orig state */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* save off orig */
		porig->iqcal_cfg1[core]    = read_radio_reg(pi, RF_2069_IQCAL_CFG1(core));
		porig->iqcal_cfg2[core]    = read_radio_reg(pi, RF_2069_IQCAL_CFG2(core));
		porig->iqcal_cfg3[core]    = read_radio_reg(pi, RF_2069_IQCAL_CFG3(core));
		porig->pa2g_tssi[core]     = read_radio_reg(pi, RF_2069_PA2G_TSSI(core));
		porig->tx5g_tssi[core]     = read_radio_reg(pi, RF_2069_TX5G_TSSI(core));
		porig->auxpga_cfg1[core]   = read_radio_reg(pi, RF_2069_AUXPGA_CFG1(core));

		/* Reg conflict with 2069 rev 16 */
		if (ACREV0)
			porig->OVR20[core] = read_radio_reg(pi, RF_2069_OVR20(core));
		else
			porig->OVR21[core] = read_radio_reg(pi, RF_2069_GE16_OVR21(core));

		/* now write desired values */

		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			MOD_RADIO_REGFLDC(pi, RF_2069_IQCAL_CFG1(core), IQCAL_CFG1,
			                  sel_sw,                 0xb);
			MOD_RADIO_REGFLDC(pi, RF_2069_TX5G_TSSI(core),  TX5G_TSSI,
			                  pa5g_ctrl_tssi_sel,     0x1);

			/* Reg conflict with 2069 rev 16 */
			if (ACREV0) {
				MOD_RADIO_REGFLDC(pi, RF_2069_OVR20(core),      OVR20,
				                  ovr_pa5g_ctrl_tssi_sel, 0x1);
				MOD_RADIO_REGFLDC(pi, RF_2069_OVR20(core),      OVR20,
				                  ovr_pa2g_ctrl_tssi_sel, 0x0);
			} else {
				MOD_RADIO_REGFLDC(pi, RF_2069_GE16_OVR21(core), GE16_OVR21,
				                  ovr_pa5g_ctrl_tssi_sel, 0x1);
				MOD_RADIO_REGFLDC(pi, RF_2069_GE16_OVR21(core), GE16_OVR21,
				                  ovr_pa2g_ctrl_tssi_sel, 0x0);
			}
			MOD_RADIO_REGFLDC(pi, RF_2069_PA2G_TSSI(core),  PA2G_TSSI,
			                  pa2g_ctrl_tssi_sel,     0x0);
		} else {
			MOD_RADIO_REGFLDC(pi, RF_2069_IQCAL_CFG1(core), IQCAL_CFG1,
			                  sel_sw,                 0x8);
			MOD_RADIO_REGFLDC(pi, RF_2069_TX5G_TSSI(core),  TX5G_TSSI,
			                  pa5g_ctrl_tssi_sel,     0x0);
			/* Reg conflict with 2069 rev 16 */
			if (ACREV0) {
				MOD_RADIO_REGFLDC(pi, RF_2069_OVR20(core),      OVR20,
				                  ovr_pa5g_ctrl_tssi_sel, 0x0);
				MOD_RADIO_REGFLDC(pi, RF_2069_OVR20(core),      OVR20,
				                  ovr_pa2g_ctrl_tssi_sel, 0x1);
			} else {
				MOD_RADIO_REGFLDC(pi, RF_2069_GE16_OVR21(core), GE16_OVR21,
				                  ovr_pa5g_ctrl_tssi_sel, 0x0);
				MOD_RADIO_REGFLDC(pi, RF_2069_GE16_OVR21(core), GE16_OVR21,
				                  ovr_pa2g_ctrl_tssi_sel, 0x1);
			}
			MOD_RADIO_REGFLDC(pi, RF_2069_PA2G_TSSI(core),  PA2G_TSSI,
			                  pa2g_ctrl_tssi_sel,     0x1);
		}
		MOD_RADIO_REGFLDC(pi, RF_2069_IQCAL_CFG1(core), IQCAL_CFG1,
		    tssi_GPIO_ctrl, 0x0);

		if (ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) {

			MOD_RADIO_REGFLDC(pi, RF_2069_AUXPGA_CFG1(core), AUXPGA_CFG1,
			                  auxpga_i_vcm_ctrl, 0x0);
			/* This bit is supposed to be controlled by phy direct control line.
			 * Please check: http://jira.broadcom.com/browse/HW11ACRADIO-45
			 */
			MOD_RADIO_REGFLDC(pi, RF_2069_AUXPGA_CFG1(core), AUXPGA_CFG1,
			                  auxpga_i_sel_input, 0x0);
		}
	} /* for core */
}

static void
wlc_phy_txcal_radio_cleanup_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_txcal_radioregs_t *porig = &(pi_ac->ac_txcal_radioregs_orig);
	uint16 core;

	/* CLEANUP: restore reg values */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		write_radio_reg(pi, RF_2069_IQCAL_CFG1(core), porig->iqcal_cfg1[core]);
		write_radio_reg(pi, RF_2069_IQCAL_CFG2(core), porig->iqcal_cfg2[core]);
		write_radio_reg(pi, RF_2069_IQCAL_CFG3(core), porig->iqcal_cfg3[core]);
		write_radio_reg(pi, RF_2069_PA2G_TSSI(core),  porig->pa2g_tssi[core]);
		write_radio_reg(pi, RF_2069_TX5G_TSSI(core),  porig->tx5g_tssi[core]);
		write_radio_reg(pi, RF_2069_AUXPGA_CFG1(core),  porig->auxpga_cfg1[core]);

		/* Reg conflict with 2069 rev 16 */
		if (ACREV0)
			write_radio_reg(pi, RF_2069_OVR20(core),      porig->OVR20[core]);
		else
			write_radio_reg(pi, RF_2069_GE16_OVR21(core),      porig->OVR21[core]);
	} /* for core */
}

static void
wlc_phy_txcal_phy_setup_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_txcal_phyregs_t *porig = &(pi_ac->ac_txcal_phyregs_orig);
	uint16 sdadc_config;
	uint8  core, bw_idx;
	uint8  stall_val;

	porig->RxFeCtrl1 = phy_reg_read(pi, ACPHY_RxFeCtrl1);
	porig->AfePuCtrl = phy_reg_read(pi, ACPHY_AfePuCtrl);
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	if (CHSPEC_IS80(pi->radio_chanspec)) {
		bw_idx = 2;
		sdadc_config = sdadc_cfg80;
	} else if (CHSPEC_IS40(pi->radio_chanspec)) {
		bw_idx = 1;
		sdadc_config = sdadc_cfg40;
	} else {
		bw_idx = 0;
		sdadc_config = sdadc_cfg20;
	}


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, rxfe_bilge_cnt, 4);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 1);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 0);
	}
	/* turn off tssi sleep feature during cal */
	MOD_PHYREG(pi, ACPHY_AfePuCtrl, tssiSleepEn, 0);

	/*  SETUP: save off orig reg values and configure for cal  */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* Power Down External PA (simply always do 2G & 5G),
		 * and set T/R to T (double check TR position)
		 */
		porig->RfctrlIntc[core] = phy_reg_read(pi, ACPHYREGCE(RfctrlIntc, core));
		phy_reg_write(pi, ACPHYREGCE(RfctrlIntc, core), 0);
		MOD_PHYREGCE(pi, RfctrlIntc, core, ext_2g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, core, ext_5g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, core, tr_sw_tx_pu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, core, tr_sw_rx_pu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, core, override_ext_pa, 1);
		MOD_PHYREGCE(pi, RfctrlIntc, core, override_tr_sw, 1);

		/* Core Activate/Deactivate */
		/* MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, DisRx, 0);
		   MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, EnTx, (1 << core));
		 */

		/* Internal RFCtrl: save and adjust state of internal PA override */
		/* save state of Rfctrl override */
		porig->RfctrlOverrideAfeCfg[core]   = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideAfeCfg, core));
		porig->RfctrlCoreAfeCfg1[core]      = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreAfeCfg1, core));
		porig->RfctrlCoreAfeCfg2[core]      = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreAfeCfg2, core));
		porig->RfctrlOverrideRxPus[core]    = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideRxPus, core));
		porig->RfctrlCoreRxPus[core]        = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreRxPus, core));
		porig->RfctrlOverrideTxPus[core]    = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideTxPus, core));
		porig->RfctrlCoreTxPus[core]        = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreTxPus, core));
		porig->RfctrlOverrideLpfSwtch[core] = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideLpfSwtch, core));
		porig->RfctrlCoreLpfSwtch[core]     = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreLpfSwtch, core));
		porig->RfctrlOverrideLpfCT[core]    = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideLpfCT, core));
		porig->RfctrlCoreLpfCT[core]        = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreLpfCT, core));
		porig->RfctrlCoreLpfGmult[core]     = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreLpfGmult, core));
		porig->RfctrlCoreRCDACBuf[core]     = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreRCDACBuf, core));
		porig->RfctrlOverrideAuxTssi[core]  = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideAuxTssi, core));
		porig->RfctrlCoreAuxTssi1[core]     = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreAuxTssi1, core));

		/* Turning off all the RF component that are not needed */
		MOD_PHYREGCE(pi, RfctrlOverrideTxPus, core, pa_pwrup,               1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus,     core, pa_pwrup,               0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rxrf_lna1_pwrup,        1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,     core, rxrf_lna1_pwrup,        0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rxrf_lna1_5G_pwrup,     1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,     core, rxrf_lna1_5G_pwrup,     0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rxrf_lna2_pwrup,        1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,     core, rxrf_lna2_pwrup,        0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, lpf_nrssi_pwrup,        1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,     core, lpf_nrssi_pwrup,        0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rssi_wb1g_pu,           1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,     core, rssi_wb1g_pu,           0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rssi_wb1a_pu,           1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,     core, rssi_wb1a_pu,           0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, lpf_wrssi3_pwrup,       1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus,     core, lpf_wrssi3_pwrup,       0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rxrf_lna2_wrssi2_pwrup, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,     core, rxrf_lna2_wrssi2_pwrup, 0);

		/* Setting the loopback path */
		MOD_PHYREGCE(pi, RfctrlOverrideTxPus, core, lpf_bq1_pu,             1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus,     core, lpf_bq1_pu,             1);
		MOD_PHYREGCE(pi, RfctrlOverrideTxPus, core, lpf_bq2_pu,             1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus,     core, lpf_bq2_pu,             1);
		MOD_PHYREGCE(pi, RfctrlOverrideTxPus, core, lpf_pu,                 1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus,     core, lpf_pu,                 1);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, lpf_pu_dc,              1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,     core, lpf_pu_dc,              1);
		MOD_PHYREGCE(pi, RfctrlOverrideAuxTssi,  core, tssi_pu,             1);
		MOD_PHYREGCE(pi, RfctrlCoreAuxTssi1,     core, tssi_pu,             1);

		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideLpfSwtch, core),     0x3ff);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfSwtch, core),         0x152);

		/* Setting the SD-ADC related stuff */
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_iqadc_mode,      1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,      core,
		             afe_iqadc_mode,      sdadc_config & 0x7);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_iqadc_pwrup,     1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg1,      core,
		             afe_iqadc_pwrup,     (sdadc_config >> 3) & 0x3f);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_iqadc_flashhspd, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,      core,
		             afe_iqadc_flashhspd, (sdadc_config >> 9) & 0x1);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_ctrl_flash17lvl, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,      core,
		             afe_ctrl_flash17lvl, (sdadc_config >> 10) & 0x1);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_iqadc_adc_bias,  1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,      core,
		             afe_iqadc_adc_bias,  (sdadc_config >> 11) & 0x3);

		/* Setting the LPF related stuff */
		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT,    core, lpf_bq1_bw,    1);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT,    core, lpf_bq2_bw,    1);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT,    core, lpf_rc_bw,     1);

		MOD_PHYREGCE(pi, RfctrlCoreLpfCT,        core, lpf_bq1_bw,    3+bw_idx);
		if (ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
			if (CHSPEC_IS80(pi->radio_chanspec)) {
				MOD_PHYREGCE(pi, RfctrlCoreLpfCT,     core, lpf_bq2_bw,  6);
				MOD_PHYREGCE(pi, RfctrlCoreRCDACBuf,  core, lpf_rc_bw,   6);
			} else {
				MOD_PHYREGCE(pi, RfctrlCoreLpfCT,     core, lpf_bq2_bw,  5);
				MOD_PHYREGCE(pi, RfctrlCoreRCDACBuf,  core, lpf_rc_bw,   5);
			}
		} else {
			MOD_PHYREGCE(pi, RfctrlCoreLpfCT,     core, lpf_bq2_bw,  3+bw_idx);
			MOD_PHYREGCE(pi, RfctrlCoreRCDACBuf,  core, lpf_rc_bw,   3+bw_idx);
		}

		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT,    core, lpf_q_biq2,    1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfCT,        core, lpf_q_biq2,    0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT,    core, lpf_dc_bypass, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfCT,        core, lpf_dc_bypass, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT,    core, lpf_dc_bw,     1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfCT,        core, lpf_dc_bw,     4);  /* 133KHz */

		MOD_PHYREGCE(pi, RfctrlOverrideAuxTssi,  core, amux_sel_port, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAuxTssi1,     core, amux_sel_port, 2);
		MOD_PHYREGCE(pi, RfctrlOverrideAuxTssi,  core, afe_iqadc_aux_en, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAuxTssi1,     core, afe_iqadc_aux_en, 1);


	} /* for core */

	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, swap_iq0, 1);
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, swap_iq1, 1);
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, swap_iq2, 1);

	/* ADC pulse clamp en fix */
	wlc_phy_pulse_adc_reset_acphy(pi);

	/* we should not need spur avoidance anymore
	porig->BBConfig = phy_reg_read(pi, ACPHY_BBConfig);
	MOD_PHYREG(pi, ACPHY_BBConfig, resample_clk160, 0);
	*/
	ACPHY_ENABLE_STALL(pi, stall_val);
}


static void
wlc_phy_txcal_phy_cleanup_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_txcal_phyregs_t *porig = &(pi_ac->ac_txcal_phyregs_orig);
	uint8  core;

	/*  CLEANUP: Restore Original Values  */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* restore ExtPA PU & TR */
		phy_reg_write(pi, ACPHYREGCE(RfctrlIntc, core), porig->RfctrlIntc[core]);

		/* restore Rfctrloverride setting */
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideRxPus, core),
		              porig->RfctrlOverrideRxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRxPus, core),
		              porig->RfctrlCoreRxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideTxPus, core),
		              porig->RfctrlOverrideTxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreTxPus, core),
		              porig->RfctrlCoreTxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideLpfSwtch, core),
		              porig->RfctrlOverrideLpfSwtch[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfSwtch, core),
		              porig->RfctrlCoreLpfSwtch[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideLpfCT, core),
		              porig->RfctrlOverrideLpfCT[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfCT, core),
		              porig->RfctrlCoreLpfCT[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfGmult, core),
		              porig->RfctrlCoreLpfGmult[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRCDACBuf, core),
		              porig->RfctrlCoreRCDACBuf[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAuxTssi, core),
		              porig->RfctrlOverrideAuxTssi[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAuxTssi1, core),
		              porig->RfctrlCoreAuxTssi1[core]);

		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAfeCfg, core),
		              porig->RfctrlOverrideAfeCfg[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg1, core),
		              porig->RfctrlCoreAfeCfg1[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg2, core),
		              porig->RfctrlCoreAfeCfg2[core]);


	} /* for core */

	phy_reg_write(pi, ACPHY_RxFeCtrl1, porig->RxFeCtrl1);
	phy_reg_write(pi, ACPHY_AfePuCtrl, porig->AfePuCtrl);

	/* phy_reg_write(pi, ACPHY_RfseqCoreActv2059, porig->RfseqCoreActv2059); */

	/* we should not need spur avoidance anymore
	phy_reg_write(pi, ACPHY_BBConfig, porig->BBConfig);
	*/

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, rxfe_bilge_cnt, 0);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 1);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 0);
	}
	wlc_phy_resetcca_acphy(pi);
}

void
wlc_phy_deaf_acphy(phy_info_t *pi, bool mode)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	if (mode) {
		if (pi_ac->deaf_count == 0)
			wlc_phy_stay_in_carriersearch_acphy(pi, TRUE);
		else
			PHY_ERROR(("%s: Deafness already set\n", __FUNCTION__));
	}
	else {
		if (pi_ac->deaf_count > 0)
			wlc_phy_stay_in_carriersearch_acphy(pi, FALSE);
		else
			PHY_ERROR(("%s: Deafness already cleared\n", __FUNCTION__));
	}
	wlapi_enable_mac(pi->sh->physhim);
}

void
wlc_phy_stay_in_carriersearch_acphy(phy_info_t *pi, bool enable)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* MAC should be suspended before calling this function */
	ASSERT((R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC) == 0);

	if (enable) {
		if (pi_ac->deaf_count == 0) {
			pi_ac->classifier_state = wlc_phy_classifier_acphy(pi, 0, 0);
			wlc_phy_classifier_acphy(pi, ACPHY_ClassifierCtrl_classifierSel_MASK, 4);
			wlc_phy_ofdm_crs_acphy(pi, FALSE);
			wlc_phy_clip_det_acphy(pi, FALSE);
		}

		pi_ac->deaf_count++;
		wlc_phy_resetcca_acphy(pi);

	} else {
		ASSERT(pi_ac->deaf_count > 0);

		pi_ac->deaf_count--;

		if (pi_ac->deaf_count == 0) {
			wlc_phy_classifier_acphy(pi, ACPHY_ClassifierCtrl_classifierSel_MASK,
			                         pi_ac->classifier_state);
			wlc_phy_ofdm_crs_acphy(pi, TRUE);
			wlc_phy_clip_det_acphy(pi, TRUE);
		}
	}
}

void
wlc_phy_ofdm_crs_acphy(phy_info_t *pi, bool enable)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* MAC should be suspended before calling this function */
	ASSERT((R_REG(pi->sh->osh, &pi->regs->maccontrol) & MCTL_EN_MAC) == 0);

	if (enable) {
		MOD_PHYREG(pi, ACPHY_crsControlu, totEnable, 1);
		MOD_PHYREG(pi, ACPHY_crsControll, totEnable, 1);
		MOD_PHYREG(pi, ACPHY_crsControluSub1, totEnable, 1);
		MOD_PHYREG(pi, ACPHY_crsControllSub1, totEnable, 1);
	} else {
		MOD_PHYREG(pi, ACPHY_crsControlu, totEnable, 0);
		MOD_PHYREG(pi, ACPHY_crsControll, totEnable, 0);
		MOD_PHYREG(pi, ACPHY_crsControluSub1, totEnable, 0);
		MOD_PHYREG(pi, ACPHY_crsControllSub1, totEnable, 0);
	}
}

void
wlc_phy_populate_tx_loft_comp_tbl_acphy(phy_info_t *pi, uint16 *loft_coeffs)
{
	uint8 core, num_cores, tbl_idx;
	uint16 coeffs;
	uint8 nwords;
	uint8 idx[3]    = {40, 30, 40};

	num_cores = pi->pubpi.phy_corenum;

	nwords = 1;

	for (tbl_idx = 0; tbl_idx < 128; tbl_idx ++) {
		for (core = 0; core < num_cores; core++) {
			coeffs = (tbl_idx < idx[core]) ?
			               loft_coeffs[2*core] : loft_coeffs[2*core + 1];
			switch (core) {
			case 0:
				wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_LOFTCOEFFLUTS0, nwords,
				    tbl_idx, 16, &coeffs);
				break;
			case 1:
				wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_LOFTCOEFFLUTS1, nwords,
				    tbl_idx, 16, &coeffs);
				break;
			case 2:
				wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_LOFTCOEFFLUTS2, nwords,
				    tbl_idx, 16, &coeffs);
				break;
			}
		}
	}
}

static int
wlc_phy_cal_txiqlo_acphy(phy_info_t *pi, uint8 searchmode, uint8 mphase)
{
	uint8  bw_idx, rd_select = 0, wr_select1 = 0, wr_select2 = 0;
	uint16 tone_ampl;
	uint16 tone_freq;
	int    bcmerror = BCME_OK;
	uint8  num_cmds_total, num_cmds_per_core;
	uint8  cmd_idx, cmd_stop_idx, core, cal_type;
	uint16 *cmds;
	uint16 cmd;
	uint16 coeffs[2];
	uint16 *coeff_ptr;
	uint16 zero = 0;
	uint8  cr, k, num_cores, thidx;
	uint16 classifier_state;
	txgain_setting_t orig_txgain[4];
	acphy_cal_result_t *accal = &pi->cal_info->u.accal;

	/* -----------
	 *  Constants
	 * -----------
	 */

	/* Table of commands for RESTART & REFINE search-modes
	 *
	 *     This uses the following format (three hex nibbles left to right)
	 *      1. cal_type: 0 = IQ (a/b),   1 = deprecated
	 *                   2 = LOFT digital (di/dq)
	 *                   3 = LOFT analog, fine,   injected at mixer      (ei/eq)
	 *                   4 = LOFT analog, coarse, injected at mixer, too (fi/fq)
	 *      2. initial stepsize (in log2)
	 *      3. number of cal precision "levels"
	 *
	 *     Notes: - functions assumes that order of LOFT cal cmds will be f => e => d,
	 *              where it's ok to have multiple cmds (say interrupted by IQ) of
	 *              the same type; this is due to zeroing out of e and/or d that happens
	 *              even during REFINE cal to avoid a coefficient "divergence" (increasing
	 *              LOFT comp over time of different types that cancel each other)
	 *            - final cal cmd should NOT be analog LOFT cal (otherwise have to manually
	 *              pick up analog LOFT settings from best_coeffs and write to radio)
	 */
	uint16 cmds_RESTART[] = { 0x434, 0x334, 0x084, 0x267, 0x056, 0x234};
	uint16 cmds_REFINE[] = { 0x423, 0x334, 0x073, 0x267, 0x045, 0x234};
	/* txidx dependent digital loft comp table */
	uint16 cmds_RESTART_TDDLC[] = { 0x434, 0x334, 0x084, 0x267, 0x056, 0x234, 0x267};
	uint16 cmds_REFINE_TDDLC[] = { 0x423, 0x334, 0x073, 0x267, 0x045, 0x234, 0x267};

	/* zeros start coeffs (a,b,di/dq,ei/eq,fi/fq for each core) */
	uint16 start_coeffs_RESTART[] = {0, 0, 0, 0, 0,  0, 0, 0, 0, 0,  0, 0, 0, 0, 0};

	/* interval lengths for gain control and correlation segments
	 *   (top/bottom nibbles are for guard and measurement intlvs, resp., in log 2 # samples)
	 */
	uint8 nsamp_gctrl[3];
	uint8 nsamp_corrs[3];
	uint8 thres_ladder[7];

	uint8 txpwridx[3] = {35, 50, 50};
	uint16 loft_coeffs[6] = {0, 0, 0, 0, 0, 0};
	uint16 *loft_coeffs_ptr = loft_coeffs;
	uint16 bbmult, idx_for_second_loft = 6;
	uint16 idx_for_loft_comp_tbl = 5;

	/* -------
	 *  Inits
	 * -------
	 */

	num_cores = pi->pubpi.phy_corenum;

	/* prevent crs trigger */
	wlc_phy_stay_in_carriersearch_acphy(pi, TRUE);
	/* phy_bw */
	if (CHSPEC_IS80(pi->radio_chanspec)) {
		bw_idx = 2;
	} else if (CHSPEC_IS40(pi->radio_chanspec)) {
		bw_idx = 1;
	} else {
		bw_idx = 0;
	}

	if (ACRADIO_IPA1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
		if (CHSPEC_IS2G(pi->radio_chanspec) == 1) {
			nsamp_gctrl[0] = 0x87; nsamp_gctrl[1] = 0x77; nsamp_gctrl[2] = 0x77;
			nsamp_corrs[0] = 0x79; nsamp_corrs[1] = 0x79; nsamp_corrs[2] = 0x79;
		} else {
			nsamp_gctrl[0] = 0x78; nsamp_gctrl[1] = 0x88; nsamp_gctrl[2] = 0x98;
			nsamp_corrs[0] = 0x89; nsamp_corrs[1] = 0x79; nsamp_corrs[2] = 0x79;
		}

		cmds_REFINE[0] =  0x434; cmds_REFINE[1] = 0x334; cmds_REFINE[2] = 0x084;
		cmds_REFINE[3] =  0x267; cmds_REFINE[4] = 0x056; cmds_REFINE[5] = 0x234;
		thres_ladder[0] = 0x3d; thres_ladder[1] = 0x2d; thres_ladder[2] = 0x1d;
		thres_ladder[3] = 0x0d; thres_ladder[4] = 0x07; thres_ladder[5] = 0x03;
		thres_ladder[6] = 0x01;
	} else {
		nsamp_gctrl[0] = 0x76; nsamp_gctrl[1] = 0x87; nsamp_gctrl[2] = 0x98;
		nsamp_corrs[0] = 0x79; nsamp_corrs[1] = 0x79; nsamp_corrs[2] = 0x79;

		cmds_REFINE[0] =  0x423; cmds_REFINE[1] = 0x334; cmds_REFINE[2] = 0x073;
		cmds_REFINE[3] =  0x267; cmds_REFINE[4] = 0x045; cmds_REFINE[5] = 0x234;
		thres_ladder[0] = 0x3d; thres_ladder[1] = 0x1e; thres_ladder[2] = 0xf;
		thres_ladder[3] = 0x07; thres_ladder[4] = 0x03;
		thres_ladder[5] = 0x01;
	}

	/* Put the radio and phy into TX iqlo cal state, including tx gains */
	classifier_state = phy_reg_read(pi, ACPHY_ClassifierCtrl);
	wlc_phy_classifier_acphy(pi, ACPHY_ClassifierCtrl_classifierSel_MASK, 4);
	wlc_phy_txcal_radio_setup_acphy(pi);
	wlc_phy_txcal_phy_setup_acphy(pi);
	wlc_phy_txcal_txgain_setup_acphy(pi, &accal->txcal_txgain[0], &orig_txgain[0]);

	/* Set IQLO Cal Engine Gain Control Parameters including engine Enable
	 * Format: iqlocal_en<15> / gain start_index / NOP / ladder_length_d2)
	 */
	phy_reg_write(pi, ACPHY_iqloCalCmdGctl, 0x8a09);


	/*
	 *   Retrieve and set Start Coeffs
	 */
	if (pi->cal_info->cal_phase_id > ACPHY_CAL_PHASE_TX0) {
		/* mphase cal and have done at least 1 Tx phase already */
		coeff_ptr = accal->txiqlocal_interm_coeffs; /* use results from previous phase */
	} else {
		/* single-phase cal or first phase of mphase cal */
		if (searchmode == PHY_CAL_SEARCHMODE_REFINE) {
			/* recal ("refine") */
			coeff_ptr = accal->txiqlocal_coeffs; /* use previous cal's final results */
		} else {
			/* start from zero coeffs ("restart") */
			coeff_ptr = start_coeffs_RESTART; /* zero coeffs */
		}
		/* copy start coeffs to intermediate coeffs, for pairwise update from here on
		 *    (after all cmds/phases have filled this with latest values, this
		 *    will be copied to OFDM/BPHY coeffs and to accal->txiqlocal_coeffs
		 *    for use by possible REFINE cal next time around)
		 */
		for (k = 0; k < 5*num_cores; k++) {
			accal->txiqlocal_interm_coeffs[k] = coeff_ptr[k];
		}
	}
	for (core = 0; core < num_cores; core++) {
		wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE, coeff_ptr + 5*core + 0,
		                                TB_START_COEFFS_AB, core);
		wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE, coeff_ptr + 5*core + 2,
		                                TB_START_COEFFS_D,  core);
		wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE, coeff_ptr + 5*core + 3,
		                                TB_START_COEFFS_E,  core);
		wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE, coeff_ptr + 5*core + 4,
		                                TB_START_COEFFS_F,  core);
	}

	/*
	 *   Choose Cal Commands for this Phase
	 */
	if (searchmode == PHY_CAL_SEARCHMODE_RESTART) {
		if (ACREV_IS(pi->pubpi.phy_rev, 1)) {
			cmds = cmds_RESTART_TDDLC;
			num_cmds_per_core = ARRAYSIZE(cmds_RESTART_TDDLC);
		} else {
			cmds = cmds_RESTART;
			num_cmds_per_core = ARRAYSIZE(cmds_RESTART);
		}
		num_cmds_total    = num_cores * num_cmds_per_core;
	} else {
		if (ACREV_IS(pi->pubpi.phy_rev, 1)) {
			cmds = cmds_REFINE_TDDLC;
			num_cmds_per_core = ARRAYSIZE(cmds_REFINE_TDDLC);
		} else {
			cmds = cmds_REFINE;
			num_cmds_per_core = ARRAYSIZE(cmds_REFINE);
		}
		num_cmds_total    = num_cores * num_cmds_per_core;
	}

	if (mphase) {
		/* multi-phase: get next subset of commands (first & last index) */
		cmd_idx = (pi->cal_info->cal_phase_id - ACPHY_CAL_PHASE_TX0) *
			MPHASE_TXCAL_CMDS_PER_PHASE; /* first cmd index in this phase */
		if ((cmd_idx + MPHASE_TXCAL_CMDS_PER_PHASE - 1) < num_cmds_total) {
			cmd_stop_idx = cmd_idx + MPHASE_TXCAL_CMDS_PER_PHASE - 1;
		} else {
			cmd_stop_idx = num_cmds_total - 1;
		}
	} else {
		/* single-phase: execute all commands for all cores */
		cmd_idx = 0;
		cmd_stop_idx = num_cmds_total - 1;
	}

	/* turn on test tone */
	tone_ampl = 250;
	tone_freq = CHSPEC_IS80(pi->radio_chanspec) ? ACPHY_IQCAL_TONEFREQ_80MHz :
		CHSPEC_IS40(pi->radio_chanspec) ? ACPHY_IQCAL_TONEFREQ_40MHz :
		ACPHY_IQCAL_TONEFREQ_20MHz;
	tone_freq = tone_freq >> 1;

	bcmerror = wlc_phy_tx_tone_acphy(pi, (int32)tone_freq, tone_ampl, 1, 0, FALSE);

	OSL_DELAY(5);
	MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, 0, afe_iqadc_reset_ov_det, 1);
	MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,    0, afe_iqadc_reset_ov_det, 0);

	if (pi->pubpi.phy_corenum > 1) {
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, 1, afe_iqadc_reset_ov_det, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,    1, afe_iqadc_reset_ov_det, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, 2, afe_iqadc_reset_ov_det, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,    2, afe_iqadc_reset_ov_det, 0);
	}
	PHY_NONE(("wlc_phy_cal_txiqlo_acphy (after inits): SearchMd=%d, MPhase=%d,"
		" CmdIds=(%d to %d)\n",
		searchmode, mphase, cmd_idx, cmd_stop_idx));

	/* ---------------
	 *  Cmd Execution
	 * ---------------
	 */

	if (bcmerror == BCME_OK) { /* in case tone doesn't start (still needed?) */

		/* loop over commands in this cal phase */
		for (; cmd_idx <= cmd_stop_idx; cmd_idx++) {

			/* get command, cal_type, and core */
			core     = cmd_idx / num_cmds_per_core; /* integer divide */

			/* only execute commands when the current core is active
			 * if ((pi->sh->phytxchain >> core) & 0x1) {
			 */
				cmd = cmds[cmd_idx % num_cmds_per_core] | 0x8000 | (core << 12);
				cal_type = ((cmd & 0x0F00) >> 8);

				/* PHY_CAL(("wlc_phy_cal_txiqlo_acphy:
				 *  Cmds => cmd_idx=%2d, Cmd=0x%04x,	\
				 *  cal_type=%d, core=%d\n", cmd_idx, cmd, cal_type, core));
				 */

				/* change tone idx for 4360 loft cal */
				if (ACREV_IS(pi->pubpi.phy_rev, 1) &&
				    ((cmd_idx % num_cmds_per_core) == idx_for_second_loft)) {
					if (wlc_phy_get_chan_freq_range_acphy(pi, 0) ==
					         WL_CHAN_FREQ_RANGE_5G_BAND3) {
						wlc_phy_stopplayback_acphy(pi);
						bbmult = wlc_phy_set_txpwr_by_index_acphy(pi,
						         (1 << core), txpwridx[core]);
						OSL_DELAY(5);
						bcmerror = wlc_phy_tx_tone_acphy(pi,
						                 (int32)tone_freq, tone_ampl,
						                 1, 0, FALSE);
						OSL_DELAY(5);
						wlc_phy_cal_txiqlo_update_ladder_acphy(pi, bbmult);
					}
				}

				/* set up scaled ladders for desired bbmult of current core */
				if (!accal->txiqlocal_ladder_updated[core]) {
					wlc_phy_cal_txiqlo_update_ladder_acphy(pi,
					        accal->txcal_txgain[core].bbmult);
					accal->txiqlocal_ladder_updated[core] = TRUE;
				}

				/* set intervals settling and measurement intervals */
				phy_reg_write(pi, ACPHY_iqloCalCmdNnum,
				              (nsamp_corrs[bw_idx] << 8) | nsamp_gctrl[bw_idx]);

				/* if coarse-analog-LOFT cal (fi/fq),
				 *     always zero out ei/eq and di/dq;
				 * if fine-analog-LOFT   cal (ei/dq),
				 *     always zero out di/dq
				 *   - even do this with search-type REFINE, to prevent a "drift"
				 *   - assumes that order of LOFT cal cmds will be f => e => d,
				 *     where it's ok to have multiple cmds (say interrupted by
				 *     IQ cal) of the same type
				 */
				if ((cal_type == CAL_TYPE_LOFT_ANA_COARSE) ||
				    (cal_type == CAL_TYPE_LOFT_ANA_FINE)) {
					wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					        &zero, TB_START_COEFFS_D, core);
				}
				if (cal_type == CAL_TYPE_LOFT_ANA_COARSE) {
					wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					        &zero, TB_START_COEFFS_E, core);
				}

				for (thidx = 0; thidx < 6; thidx++) {

					/* Set thresh_d2 */
					phy_reg_write(pi, ACPHY_iqloCalgtlthres,
					              thres_ladder[thidx]);

					/* now execute this command and wait max of ~20ms */
					phy_reg_write(pi, ACPHY_iqloCalCmd, cmd);
					SPINWAIT(((phy_reg_read(pi, ACPHY_iqloCalCmd)
					           & 0xc000) != 0), ACPHY_SPINWAIT_TXIQLO);
					ASSERT((phy_reg_read(pi, ACPHY_iqloCalCmd) & 0xc000) == 0);

					if (wlc_poll_adc_clamp_status(pi, core, 1) == 0) {
						break;
					}

					PHY_CAL(("wlc_phy_cal_txiqlo_acphy: Cmds => cmd_idx=%2d,",
					         cmd_idx));
					PHY_CAL(("Cmd=0x%04x, cal_type=%d, core=%d, ",
					         cmd, cal_type, core));
					PHY_CAL(("thresh_idx = %d\n", thidx));
				}

				switch (cal_type) {
				case CAL_TYPE_IQ:
					rd_select  = TB_BEST_COEFFS_AB;
					wr_select1 = TB_START_COEFFS_AB;
					wr_select2 = PI_INTER_COEFFS_AB;
					break;
				case CAL_TYPE_LOFT_DIG:
					rd_select  = TB_BEST_COEFFS_D;
					wr_select1 = TB_START_COEFFS_D;
					wr_select2 = PI_INTER_COEFFS_D;
					break;
				case CAL_TYPE_LOFT_ANA_FINE:
					rd_select  = TB_BEST_COEFFS_E;
					wr_select1 = TB_START_COEFFS_E;
					wr_select2 = PI_INTER_COEFFS_E;
					break;
				case CAL_TYPE_LOFT_ANA_COARSE:
					rd_select  = TB_BEST_COEFFS_F;
					wr_select1 = TB_START_COEFFS_F;
					wr_select2 = PI_INTER_COEFFS_F;
					break;
				default:
					ASSERT(0);
				}
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ,
				                                coeffs, rd_select,  core);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
				                                coeffs, wr_select1, core);
				if (ACREV_IS(pi->pubpi.phy_rev, 1) &&
				    ((cmd_idx % num_cmds_per_core) >= idx_for_loft_comp_tbl)) {
					/* write to the txpwrctrl tbls */
					*loft_coeffs_ptr++ = coeffs[0];
				}
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
				                                coeffs, wr_select2, core);
			/* }  txchain loop */
		} /* command loop */

		/* single phase or last tx stage in multiphase cal: apply & store overall results */
		if ((mphase == 0) || (pi->cal_info->cal_phase_id == ACPHY_CAL_PHASE_TX_LAST)) {

			PHY_CAL(("wlc_phy_cal_txiqlo_acphy (mphase = %d, refine = %d):\n",
			         mphase, searchmode == PHY_CAL_SEARCHMODE_REFINE));
			for (cr = 0; cr < num_cores; cr++) {
				/* Save and Apply IQ Cal Results */
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ,
					coeffs, PI_INTER_COEFFS_AB, cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					coeffs, PI_FINAL_COEFFS_AB, cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					coeffs, TB_OFDM_COEFFS_AB,  cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					coeffs, TB_BPHY_COEFFS_AB,  cr);

				/* Save and Apply Dig LOFT Cal Results */
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ,
					coeffs, PI_INTER_COEFFS_D, cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					coeffs, PI_FINAL_COEFFS_D, cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					coeffs, TB_OFDM_COEFFS_D,  cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					coeffs, TB_BPHY_COEFFS_D,  cr);

				/* Apply Analog LOFT Comp
				 * - unncessary if final command on each core is digital
				 * LOFT-cal or IQ-cal
				 * - then the loft comp coeffs were applied to radio
				 * at the beginning of final command per core
				 * - this is assumed to be the case, so nothing done here
				 */

				/* Save Analog LOFT Comp in PI State */
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ,
					coeffs, PI_INTER_COEFFS_E, cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					coeffs, PI_FINAL_COEFFS_E, cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ,
					coeffs, PI_INTER_COEFFS_F, cr);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					coeffs, PI_FINAL_COEFFS_F, cr);

				/* Print out Results */
				PHY_CAL(("\tcore-%d: a/b = (%4d,%4d), d = (%4d,%4d),"
					" e = (%4d,%4d), f = (%4d,%4d)\n", cr,
					accal->txiqlocal_coeffs[cr*5+0],  /* a */
					accal->txiqlocal_coeffs[cr*5+1],  /* b */
					(accal->txiqlocal_coeffs[cr*5+2] & 0xFF00) >> 8, /* di */
					(accal->txiqlocal_coeffs[cr*5+2] & 0x00FF),      /* dq */
					(accal->txiqlocal_coeffs[cr*5+3] & 0xFF00) >> 8, /* ei */
					(accal->txiqlocal_coeffs[cr*5+3] & 0x00FF),      /* eq */
					(accal->txiqlocal_coeffs[cr*5+4] & 0xFF00) >> 8, /* fi */
					(accal->txiqlocal_coeffs[cr*5+4] & 0x00FF)));   /* fq */
			} /* for cr */

			/* validate availability of results and store off channel */
			accal->txiqlocal_coeffsvalid = TRUE;
			accal->chanspec = pi->radio_chanspec;
		} /* writing of results */

		/* Switch off test tone */
		wlc_phy_stopplayback_acphy(pi);	/* mimophy_stop_playback */

		/* disable IQ/LO cal */
		phy_reg_write(pi, ACPHY_iqloCalCmdGctl, 0x0000);
	} /* if BCME_OK */

	if (ACREV_IS(pi->pubpi.phy_rev, 1)) {
		wlc_phy_populate_tx_loft_comp_tbl_acphy(pi, loft_coeffs);
	}

	/* clean Up PHY and radio */
	wlc_phy_txcal_txgain_cleanup_acphy(pi, &orig_txgain[0]);
	wlc_phy_txcal_phy_cleanup_acphy(pi);
	wlc_phy_txcal_radio_cleanup_acphy(pi);
	phy_reg_write(pi, ACPHY_ClassifierCtrl, classifier_state);


	/*
	 *-----------*
	 *  Cleanup  *
	 *-----------
	 */

	/* prevent crs trigger */
	wlc_phy_stay_in_carriersearch_acphy(pi, FALSE);

	return bcmerror;
}


/* see also: proc acphy_rx_iq_est { {num_samps 2000} {wait_time ""} } */
void
wlc_phy_rx_iq_est_acphy(phy_info_t *pi, phy_iq_est_t *est, uint16 num_samps,
                        uint8 wait_time, uint8 wait_for_crs)
{
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	wlc_phy_pulse_adc_reset_acphy(pi);

	/* Get Rx IQ Imbalance Estimate from modem */
	phy_reg_write(pi, ACPHY_IqestSampleCount, num_samps);
	MOD_PHYREG(pi, ACPHY_IqestWaitTime, waitTime, wait_time);
	MOD_PHYREG(pi, ACPHY_IqestCmd, iqMode, wait_for_crs);

	MOD_PHYREG(pi, ACPHY_IqestCmd, iqstart, 1);

	/* wait for estimate */
	SPINWAIT(((phy_reg_read(pi, ACPHY_IqestCmd) & ACPHY_IqestCmd_iqstart_MASK) != 0),
		ACPHY_SPINWAIT_IQEST);
	ASSERT((phy_reg_read(pi, ACPHY_IqestCmd) & ACPHY_IqestCmd_iqstart_MASK) == 0);

	if ((phy_reg_read(pi, ACPHY_IqestCmd) & ACPHY_IqestCmd_iqstart_MASK) == 0) {
		ASSERT(pi->pubpi.phy_corenum <= PHY_CORE_MAX);
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			est[core].i_pwr = (phy_reg_read(pi, ACPHYREGCE(IqestipwrAccHi, core)) <<
			                   16) | phy_reg_read(pi, ACPHYREGCE(IqestipwrAccLo, core));
			est[core].q_pwr = (phy_reg_read(pi, ACPHYREGCE(IqestqpwrAccHi, core)) <<
			                   16) | phy_reg_read(pi, ACPHYREGCE(IqestqpwrAccLo, core));
			est[core].iq_prod = (phy_reg_read(pi, ACPHYREGCE(IqestIqAccHi, core)) <<
			                     16) | phy_reg_read(pi, ACPHYREGCE(IqestIqAccLo, core));
			PHY_NONE(("wlc_phy_rx_iq_est_acphy: core%d "
			         "i_pwr = %u, q_pwr = %u, iq_prod = %d\n",
			         core, est[core].i_pwr, est[core].q_pwr, est[core].iq_prod));
		}
	} else {
		PHY_ERROR(("wlc_phy_rx_iq_est_acphy: IQ measurement timed out\n"));
	}
}

static void
wlc_phy_clip_det_acphy(phy_info_t *pi, bool enable)
{
	uint16 core;
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;

	/* Make clip detection difficult (impossible?) */
	/* don't change this loop to active core loop, gives 100% per, why? */
	FOREACH_CORE(pi, core) {
		if (pi->pubpi.phy_rev == 0) {
			if (enable) {
				phy_reg_write(pi, ACPHYREGC(Clip1Threshold, core),
				              pi_ac->clip_state[core]);
			} else {
				pi_ac->clip_state[core] = phy_reg_read(pi, ACPHYREGC(Clip1Threshold,
				                                                     core));
				phy_reg_write(pi, ACPHYREGC(Clip1Threshold, core), 0xffff);
			}
		} else {
			if (enable) {
				phy_reg_and(pi, ACPHYREGC(computeGainInfo, core),
				            ~ACPHY_Core0computeGainInfo_disableClip1detect_MASK);
			} else {
				phy_reg_or(pi, ACPHYREGC(computeGainInfo, core),
				           ACPHY_Core0computeGainInfo_disableClip1detect_MASK);
			}
		}
	}

}




void wlc_phy_lp_mode(phy_info_t *pi, int8 lp_mode)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	if (ACREV1X1_IS(pi->pubpi.phy_rev))	{
		if (lp_mode == 1) {
			/* AC MODE (Full Pwr mode) */
			pi_ac->acphy_lp_mode = 1;
		} else if (lp_mode == 2) {
			/* 11n MODE (VCO in 11n Mode) */
			pi_ac->acphy_lp_mode = 2;
		} else if (lp_mode == 3) {
			/* Low pwr MODE */
			pi_ac->acphy_lp_mode = 3;
		} else {
			return;
		}
	} else {
		return;
	}
}

static void
wlc_phy_crs_min_pwr_cal_acphy(phy_info_t *pi)
{
	int8   cmplx_pwr_dbm[PHY_CORE_MAX], idx[PHY_CORE_MAX];
	int8   offset_1 = 0, offset_2 = 0;
	int8   thresh_20[] = {45, 48, 51, 53, 55, 58, 60, 63, 66, 68,
			      70, 72, 75, 78, 80}; /* idx 0 --> -34 dBm */
	int8   thresh_40[] = {44, 46, 48, 50, 52, 54, 56, 58, 60, 63,
			      66, 69, 71, 74, 76}; /* idx 0 --> -33 dBm */
	int8   thresh_80[] = {45, 47, 49, 51, 53, 54, 55, 57, 58, 59,
			      60, 61, 63, 65, 67}; /* idx 0 --> -30 dBm */
	uint8  i, fp[PHY_CORE_MAX];


	bzero((uint8 *)cmplx_pwr_dbm, sizeof(cmplx_pwr_dbm));
	bzero((uint8 *)fp, sizeof(fp));
	bzero((uint8 *)pi->u.pi_acphy->phy_noise_in_crs_min,
	      sizeof(pi->u.pi_acphy->phy_noise_in_crs_min));

	/* Don't do crsminpwr cal if any desense(aci/bt) is on */
	if (pi->u.pi_acphy->total_desense.on) {
		pi->u.pi_acphy->crsmincal_run = 2;
		return;
	}

	/* Initialize */
	fp[0] = 0x36;

	wlc_phy_noise_sample_request_external((wlc_phy_t*)pi);


	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, i)  {
		cmplx_pwr_dbm[i] = pi->u.pi_acphy->phy_noise_all_core[i] + 1;
		pi->u.pi_acphy->phy_noise_in_crs_min[i] = cmplx_pwr_dbm[i];
		PHY_CAL(("%s: cmplx_pwr (%d) =======  %d\n", __FUNCTION__, i, cmplx_pwr_dbm[i]));

		if (pi->u.pi_acphy->phy_noise_all_core[i] == 0) {
			pi->u.pi_acphy->crsmincal_run = 10+i;
			return;
		}


		if (CHSPEC_IS20(pi->radio_chanspec)) {
		  idx[i] = cmplx_pwr_dbm[i] + 34;
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			idx[i] = cmplx_pwr_dbm[i] + 33;
		} else {
			idx[i] = cmplx_pwr_dbm[i] + 30;
		}

		/* out of bound */
		if ((idx[i] < 0) || (idx[i] > 14)) {
			PHY_CAL(("%s: calibration out of bound\n", __FUNCTION__));
			idx[i] = idx[i] < 0 ? 0 : 14;
		}

	}


	/* In case core0 is not part of active rx cores */
	if ((pi->sh->phyrxchain & 1) == 0) {
		wlc_phy_set_crs_min_pwr_acphy(pi, fp[0], 0, 0);
	}

	/* 20Mhz channels only */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, i)  {
		pi->u.pi_acphy->crsmincal_run = 1;
		if (CHSPEC_IS20(pi->radio_chanspec)) {
			fp[i] = thresh_20[idx[i]];
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			fp[i] = thresh_40[idx[i]];
		} else if (CHSPEC_IS80(pi->radio_chanspec)) {
			fp[i] = thresh_80[idx[i]];
		}

		if (i == 1) {
			offset_1 = fp[1] - fp[0];
		} else if (i == 2) {
			offset_2 = fp[2] - fp[0];
		}
	}
	wlc_phy_set_crs_min_pwr_acphy(pi, fp[0], offset_1, offset_2);
}

static void
wlc_phy_txpwrctrl_set_target_acphy(phy_info_t *pi, uint8 pwr_qtrdbm, uint8 core)
{
	/* set target powers in 6.2 format (in dBs) */
	switch (core) {
	case 0:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlTargetPwr_path0, targetPwr0, pwr_qtrdbm);
		break;
	case 1:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlTargetPwr_path1, targetPwr1, pwr_qtrdbm);
		break;
	case 2:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlTargetPwr_path2, targetPwr2, pwr_qtrdbm);
		break;
	}
}

static uint8
wlc_phy_txpwrctrl_get_cur_index_acphy(phy_info_t *pi, uint8 core)
{
	uint16 tmp = 0;

	switch (core) {
	case 0:
		tmp = READ_PHYREG(pi, ACPHY_TxPwrCtrlStatus_path0, baseIndex);
		break;
	case 1:
		tmp = READ_PHYREG(pi, ACPHY_TxPwrCtrlStatus_path1, baseIndex);
		break;
	case 2:
		tmp = READ_PHYREG(pi, ACPHY_TxPwrCtrlStatus_path2, baseIndex);
		break;
	}
	return (uint8)tmp;
}

static void
wlc_phy_txpwrctrl_set_cur_index_acphy(phy_info_t *pi, uint8 idx, uint8 core)
{
	switch (core) {
	case 0:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlInit_path0, pwrIndex_init_path0, idx);
		break;
	case 1:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlInit_path1, pwrIndex_init_path1, idx);
		break;
	case 2:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlInit_path2, pwrIndex_init_path2, idx);
		break;
	}
}

static bool
wlc_phy_txpwrctrl_ison_acphy(phy_info_t *pi)
{
	uint16 mask = (ACPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
	               ACPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
	               ACPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK);

	return ((phy_reg_read((pi), ACPHY_TxPwrCtrlCmd) & mask) == mask);
}

uint32
wlc_phy_txpwr_idx_get_acphy(phy_info_t *pi)
{
	uint8 core;
	uint32 pwr_idx[] = {0, 0, 0, 0};
	uint32 tmp = 0;

	if (wlc_phy_txpwrctrl_ison_acphy(pi)) {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			pwr_idx[core] = wlc_phy_txpwrctrl_get_cur_index_acphy(pi, core);
		}
	} else {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			pwr_idx[core] = (pi->u.pi_acphy->txpwrindex[core] & 0xff);
		}
	}
	tmp = (pwr_idx[3] << 24) | (pwr_idx[2] << 16) | (pwr_idx[1] << 8) | pwr_idx[0];

	return tmp;
}

void
wlc_phy_txpwrctrl_enable_acphy(phy_info_t *pi, uint8 ctrl_type)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	uint16 mask;
	uint8 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	/* check for recognized commands */
	switch (ctrl_type) {
	case PHY_TPC_HW_OFF:
	case PHY_TPC_HW_ON:
		pi->txpwrctrl = ctrl_type;
		break;
	default:
		PHY_ERROR(("wl%d: %s: Unrecognized ctrl_type: %d\n",
			pi->sh->unit, __FUNCTION__, ctrl_type));
		break;
	}

	if (ctrl_type == PHY_TPC_HW_OFF) {
		/* save previous txpwr index if txpwrctl was enabled */
		if (wlc_phy_txpwrctrl_ison_acphy(pi)) {
			FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
				pi_ac->txpwrindex_hw_save[core] =
					wlc_phy_txpwrctrl_get_cur_index_acphy(pi, (uint8)core);
			}
		}

		/* Disable hw txpwrctrl */
		mask = ACPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
		       ACPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
		       ACPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK;
		phy_reg_mod(pi, ACPHY_TxPwrCtrlCmd, mask, 0);

	} else {

		/* Enable hw txpwrctrl */
		mask = ACPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
		       ACPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
		       ACPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK;
		phy_reg_mod(pi, ACPHY_TxPwrCtrlCmd, mask, mask);
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			if (pi_ac->txpwrindex_hw_save[core] != 128) {
				wlc_phy_txpwrctrl_set_cur_index_acphy(pi,
					pi_ac->txpwrindex_hw_save[core], core);
			}
		}
	}
}

static void
wlc_phy_txpwrctrl_set_idle_tssi_acphy(phy_info_t *pi, int16 idle_tssi, uint8 core)
{
	/* set idle TSSI in 2s complement format (max is 0x1ff) */
	switch (core) {
	case 0:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlIdleTssi_path0, idleTssi0, idle_tssi);
		break;
	case 1:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlIdleTssi_path1, idleTssi1, idle_tssi);
		break;
	case 2:
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlIdleTssi_path2, idleTssi2, idle_tssi);
		break;
	}
}
static void
wlc_phy_tssi_radio_setup_acphy(phy_info_t *pi, uint8 core_mask)
{
	uint8  core;

	/* 2069_gpaio(clear) to pwr up the GPAIO and clean up al lthe otehr test pins */
	/* first powerup the CGPAIO block */
	MOD_RADIO_REG(pi, RF2, CGPAIO_CFG1, cgpaio_pu, 1);
	/* turn off all test points in cgpaio block to avoid conflict, disable tp0 to tp15 */
	write_radio_reg(pi, RF2_2069_CGPAIO_CFG2, 0);
	/* disable tp16 to tp31 */
	write_radio_reg(pi, RF2_2069_CGPAIO_CFG3, 0);
	/* Disable muxsel0 and muxsel1 test points */
	write_radio_reg(pi, RF2_2069_CGPAIO_CFG4, 0);
	/* Disable muxsel2 and muxselgpaio test points */
	write_radio_reg(pi, RF2_2069_CGPAIO_CFG5, 0);
	/* Powerdown rcal. This is one of the enable pins to AND gate in cgpaio block */
	MOD_RADIO_REG(pi, RF2, RCAL_CFG, pu, 0);



	FOREACH_ACTV_CORE(pi, core_mask, core) {

		if ((ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) &&
		    CHSPEC_IS5G(pi->radio_chanspec)) {

			MOD_RADIO_REGFLDC(pi, RF_2069_IQCAL_CFG1(core), IQCAL_CFG1, sel_sw, 0x3);
		} else {
			MOD_RADIO_REGFLDC(pi, RF_2069_IQCAL_CFG1(core), IQCAL_CFG1, sel_sw, 0x1);
		}

		MOD_RADIO_REGFLDC(pi, RF_2069_IQCAL_CFG1(core), IQCAL_CFG1, sel_ext_tssi, 0x1);

		switch (core) {
		case 0:
			MOD_RADIO_REGFLDC(pi, RF2_2069_CGPAIO_CFG4, CGPAIO_CFG4,
			cgpaio_tssi_muxsel0, 0x1);
			break;
		case 1:
			MOD_RADIO_REGFLDC(pi, RF2_2069_CGPAIO_CFG4, CGPAIO_CFG4,
			cgpaio_tssi_muxsel1, 0x1);
			break;
		case 2:
			MOD_RADIO_REGFLDC(pi, RF2_2069_CGPAIO_CFG5, CGPAIO_CFG5,
			cgpaio_tssi_muxsel2, 0x1);
			break;
		}

		MOD_RADIO_REGFLDC(pi, RF_2069_IQCAL_CFG1(core), IQCAL_CFG1, tssi_GPIO_ctrl, 0);
		MOD_RADIO_REGFLDC(pi, RF_2069_TESTBUF_CFG1(core), TESTBUF_CFG1, GPIO_EN, 0);

		/* Reg conflict with 2069 rev 16 */
		if (ACREV0) {
			MOD_RADIO_REGFLDC(pi, RF_2069_TX5G_TSSI(core), TX5G_TSSI,
			                  pa5g_ctrl_tssi_sel, 0);
			MOD_RADIO_REGFLDC(pi, RF_2069_OVR20(core), OVR20,
				ovr_pa5g_ctrl_tssi_sel, 1);
		} else {
			MOD_RADIO_REGFLDC(pi, RF_2069_TX5G_TSSI(core), TX5G_TSSI,
			                  pa5g_ctrl_tssi_sel, 0);
			MOD_RADIO_REGFLDC(pi, RF_2069_PA2G_TSSI(core), PA2G_TSSI,
			                  pa2g_ctrl_tssi_sel, 0);
			MOD_RADIO_REGFLDC(pi, RF_2069_GE16_OVR21(core),
				GE16_OVR21, ovr_pa5g_ctrl_tssi_sel, 1);
			MOD_RADIO_REGFLDC(pi, RF_2069_GE16_OVR21(core),
				GE16_OVR21, ovr_pa2g_ctrl_tssi_sel, 1);
		}

		if (ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
			MOD_RADIO_REGFLDC(pi, RF_2069_AUXPGA_CFG1(core), AUXPGA_CFG1,
				auxpga_i_vcm_ctrl, 0x0);
			/* This bit is supposed to be controlled by phy direct control line.
			 * Please check: http://jira.broadcom.com/browse/HW11ACRADIO-45
			 */
			MOD_RADIO_REGFLDC(pi, RF_2069_AUXPGA_CFG1(core), AUXPGA_CFG1,
				auxpga_i_sel_input, 0x2);
		}

	}
}

static void
wlc_phy_tssi_phy_setup_acphy(phy_info_t *pi)
{
	uint8 core;

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		MOD_PHYREGCE(pi, RfctrlOverrideAuxTssi, core, tssi_pu, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAuxTssi1, core, tssi_pu, 0);
	}
}

void
wlc_phy_gpiosel_acphy(phy_info_t *pi, uint16 sel, uint8 word_swap)
{
	uint32 mc;

	/* kill OutEn */
	phy_reg_write(pi, ACPHY_gpioLoOutEn, 0x0);
	phy_reg_write(pi, ACPHY_gpioHiOutEn, 0x0);


	/* clear the mac selects, disable mac oe */
	mc = R_REG(pi->sh->osh, &pi->regs->maccontrol);
	mc &= ~MCTL_GPOUT_SEL_MASK;
	W_REG(pi->sh->osh, &pi->regs->maccontrol, mc);
	W_REG(pi->sh->osh, &pi->regs->psm_gpio_oe, 0x0);

	/* set up acphy GPIO sel */
	phy_reg_write(pi, ACPHY_gpioSel, (word_swap<<8) | sel);
	phy_reg_write(pi, ACPHY_gpioLoOutEn, 0xffff);
	phy_reg_write(pi, ACPHY_gpioHiOutEn, 0xffff);
}

static void
wlc_phy_poll_adc_acphy(phy_info_t *pi, int32 *adc_buf, uint8 nsamps)
{
	uint core;
	uint8 samp = 0;
	uint8 word_swap_flag = 1;

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		adc_buf[2*core] = 0;
		adc_buf[2*core+1] = 0;
	}

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		wlc_phy_gpiosel_acphy(pi, 16+core, word_swap_flag);
		for (samp = 0; samp < nsamps; samp++) {
			/* read out the i-value */
			adc_buf[2*core] += phy_reg_read(pi, ACPHY_gpioHiOut);
			/* read out the q-value */
			adc_buf[2*core+1] += phy_reg_read(pi, ACPHY_gpioLoOut);
		}
	}
}


static void wlc_phy_poll_samps_acphy(phy_info_t *pi, int16 *samp, bool is_tssi)
{
	int32 adc_buf[2*PHY_CORE_MAX];
	int32 k, tmp_samp, samp_accum[PHY_CORE_MAX];
	uint8 core;
	uint8 log2_nsamps, swap;

	/* initialization */
	FOREACH_CORE(pi, core) {
		samp_accum[core] = 0;
	}

	if (is_tssi) {
		/* Taking a 256-samp average for 80mHz idle-tssi measuring.
		 * Note: ideally, we can apply the same averaging for 20/40mhz also,
		 *       but we don't want to change the existing 20/40mhz behavior to reduce risk.
		 */
		log2_nsamps = CHSPEC_IS80(pi->radio_chanspec) ? 8 : 0;
		swap = 1;
	} else {
		log2_nsamps = 3;
		swap = 0;
	}

	wlc_phy_pulse_adc_reset_acphy(pi);
	OSL_DELAY(100);

	/* tssi val is (adc >> 2) */
	for (k = 0; k < (1 << log2_nsamps); k++) {
		wlc_phy_poll_adc_acphy(pi, adc_buf, 1);
		FOREACH_CORE(pi, core) {
			tmp_samp = adc_buf[2*core+swap] >> 2;
			tmp_samp -= (tmp_samp < 512) ? 0 : 1024;
			samp_accum[core] += tmp_samp;
		}
	}

	FOREACH_CORE(pi, core) {
		samp[core] = (int16) (samp_accum[core] >> log2_nsamps);
	}
}

static void
wlc_phy_poll_samps_WAR_acphy(phy_info_t *pi, int16 *samp, bool is_tssi,
bool for_idle, txgain_setting_t *target_gains)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	uint16 core, save_afePuCtrl, save_gpio;
	uint16 txgain1_save[PHY_CORE_MAX];
	uint16 txgain2_save[PHY_CORE_MAX];
	uint16 dacgain_save[PHY_CORE_MAX];
	uint16 bq2gain_save[PHY_CORE_MAX];
	uint16 overridegains_save[PHY_CORE_MAX];
	uint16 fval2g_orig, fval5g_orig, fval2g, fval5g;
	uint32 save_chipc = 0;
	uint8  stall_val = 0;
	txgain_setting_t orig_txgain[4];

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	save_afePuCtrl = READ_PHYREG(pi, ACPHY_AfePuCtrl, tssiSleepEn);
	MOD_PHYREG(pi, ACPHY_AfePuCtrl, tssiSleepEn, 0);
	save_gpio = phy_reg_read(pi, ACPHY_gpioSel);

	if (pi_ac->poll_adc_WAR) {
		ACPHY_DISABLE_STALL(pi);

		MOD_PHYREGCE(pi, RfctrlIntc, 1, ext_2g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, 1, ext_5g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, 1, override_ext_pa, 1);

		save_chipc = si_corereg(pi->sh->sih, SI_CC_IDX,
		                        OFFSETOF(chipcregs_t, chipcontrol), 0, 0);
		si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
			0xffffffff, CCTRL4360_EXTRA_FEMCTRL_MODE);

		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, 41, 16, &fval2g_orig);
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, 57, 16, &fval5g_orig);

		fval2g = (fval2g_orig & 0xf0) << 1;
		fval5g = (fval5g_orig & 0xf);

		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, 41, 16, &fval2g);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, 57, 16, &fval5g);

		MOD_PHYREGCE(pi, RfctrlIntc, 1, ext_2g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, 1, ext_5g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, 1, override_ext_pa, 0);

		ACPHY_ENABLE_STALL(pi, stall_val);
	}

	if (is_tssi) {
		ACPHY_DISABLE_STALL(pi);

		/* Set TX gain to 0, so that LO leakage does not affect IDLE TSSI */
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			dacgain_save[core] = phy_reg_read(pi, ACPHY_Dac_gain(core));
			txgain1_save[core] = phy_reg_read(pi, ACPHY_RfctrlCoreTXGAIN1(core));
			txgain2_save[core] = phy_reg_read(pi, ACPHY_RfctrlCoreTXGAIN2(core));
			bq2gain_save[core] = phy_reg_read(pi, ACPHY_RfctrlCoreLpfGain(core));
			overridegains_save[core] =
				phy_reg_read(pi, ACPHY_RfctrlOverrideGains(core));

			/* This is to measure the idle tssi */
			if (for_idle) {
				phy_reg_write(pi, ACPHY_RfctrlCoreTXGAIN1(core), 0);
				phy_reg_write(pi, ACPHY_RfctrlCoreTXGAIN2(core), 0);
				phy_reg_write(pi, ACPHY_Dac_gain(core), 0);
				MOD_PHYREGCE(pi, RfctrlCoreLpfGain, core, lpf_bq2_gain, 0);
				MOD_PHYREGCE(pi, RfctrlOverrideGains, core, txgain, 1);
				MOD_PHYREGCE(pi, RfctrlOverrideGains, core, lpf_bq2_gain, 1);
			/* This is to measure the tone tssi */
			} else {
				/* Save the original Gain code */
				wlc_phy_txcal_txgain_setup_acphy(pi, target_gains, &orig_txgain[0]);
			}
		}

		ACPHY_ENABLE_STALL(pi, stall_val);

		/* Enable WLAN priority */
		wlc_btcx_override_enable(pi);

		OSL_DELAY(100);
		if (for_idle) {
			wlc_phy_tx_tone_acphy(pi, 2000, 0, 0, 0, FALSE);
		} else {
			wlc_phy_tx_tone_acphy(pi, 4000, 181, 0, 0, FALSE);
		}
		OSL_DELAY(100);

		wlc_phy_poll_samps_acphy(pi, samp, TRUE);
		wlc_phy_stopplayback_acphy(pi);

		if (!for_idle) {
			/* Restore the original Gain code */
			wlc_phy_txcal_txgain_cleanup_acphy(pi, &orig_txgain[0]);
		}

		/* Disable WLAN priority */
		wlc_phy_btcx_override_disable(pi);
	} else {
		wlc_phy_poll_samps_acphy(pi, samp, FALSE);
	}

	ACPHY_DISABLE_STALL(pi);

	phy_reg_write(pi, ACPHY_gpioSel, save_gpio);

	if (is_tssi) {

		/* Remove TX gain override */
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			phy_reg_write(pi, ACPHY_RfctrlCoreTXGAIN1(core), txgain1_save[core]);
			phy_reg_write(pi, ACPHY_RfctrlCoreTXGAIN2(core), txgain2_save[core]);
			phy_reg_write(pi, ACPHY_Dac_gain(core), dacgain_save[core]);
			phy_reg_write(pi, ACPHY_RfctrlOverrideGains(core),
				overridegains_save[core]);
			phy_reg_write(pi, ACPHY_RfctrlCoreLpfGain(core), bq2gain_save[core]);
		}

	}

	if (pi_ac->poll_adc_WAR) {
		MOD_PHYREGCE(pi, RfctrlIntc, 1, ext_2g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, 1, ext_5g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, 1, override_ext_pa, 1);

		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, 41, 16, &fval2g_orig);
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_FEMCTRLLUT, 1, 57, 16, &fval5g_orig);

		si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
		           0xffffffff, save_chipc);

		MOD_PHYREGCE(pi, RfctrlIntc, 1, ext_2g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, 1, ext_5g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, 1, override_ext_pa, 0);
	}

	MOD_PHYREG(pi, ACPHY_AfePuCtrl, tssiSleepEn, save_afePuCtrl);

	ACPHY_ENABLE_STALL(pi, stall_val);
}
#if defined(BCMDBG) || defined(WLTEST)
int
wlc_phy_freq_accuracy_acphy(phy_info_t *pi, int channel)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	int bcmerror = BCME_OK;

	if (channel == 0) {
		wlc_phy_stopplayback_acphy(pi);
		/* restore the old BBconfig, to restore resampler setting */
		phy_reg_write(pi, ACPHY_BBConfig, pi_ac->saved_bbconf);
		wlc_phy_resetcca_acphy(pi);
	} else {
		/* Disable the re-sampler (in case we are in spur avoidance mode) */
		pi_ac->saved_bbconf = phy_reg_read(pi, ACPHY_BBConfig);
		MOD_PHYREG(pi, ACPHY_BBConfig, resample_clk160, 0);
		/* use 151 since that should correspond to nominal tx output power */
		bcmerror = wlc_phy_tx_tone_acphy(pi, 0, 151, 0, 0, TRUE);

	}
	return bcmerror;
}
#endif 
/* measure idle TSSI by sending 0-magnitude tone */
#ifndef DSLCPE_C590068
static
#endif
void
wlc_phy_txpwrctrl_idle_tssi_meas_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	uint8 core;
	int16  idle_tssi[PHY_CORE_MAX] = {0};

	if (SCAN_RM_IN_PROGRESS(pi) || PLT_INPROG_PHY(pi) || PHY_MUTED(pi))
		/* skip idle tssi cal */
		return;

	wlc_phy_tssi_phy_setup_acphy(pi);
	/* The correct fix should be:
	 *   wlc_phy_tssi_radio_setup_acphy(pi, pi->sh->phyrxchain);
	 * Temporarily delay this correct fix till further verification
	 */

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		wlc_phy_tssi_radio_setup_acphy(pi, 1);
	} else {
		wlc_phy_tssi_radio_setup_acphy(pi, 7);
	}

	wlc_phy_poll_samps_WAR_acphy(pi, idle_tssi, TRUE, TRUE, NULL);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* wlc_phy_txpwrctrl_set_idle_tssi_acphy(pi, idle_tssi[core], core); */
		pi_ac->idle_tssi[core] = idle_tssi[core];
		PHY_TXPWR(("wl%d: %s: idle_tssi core%d: %d\n",
		           pi->sh->unit, __FUNCTION__, core, pi_ac->idle_tssi[core]));
	}
}

uint8
wlc_phy_set_txpwr_clamp_acphy(phy_info_t *pi)
{
	uint16 idle_tssi_shift, adj_tssi_min;
	int16 tssi_floor = 0;
	int16 idleTssi_2C = 0;
	int16 a1 = 0, b0 = 0, b1 = 0;
	uint8 pwr = 0;

	wlc_phy_get_tssi_floor_acphy(pi, &tssi_floor);
	wlc_phy_get_paparams_for_band_acphy(pi, &a1, &b0, &b1);

	idleTssi_2C = READ_PHYREG(pi, ACPHY_TxPwrCtrlIdleTssi_path0, idleTssi0);
	if (idleTssi_2C >= 512) {
		idle_tssi_shift = idleTssi_2C - 1023 - (-512);
	} else {
		idle_tssi_shift = 1023 + idleTssi_2C - 511;
	}
	idle_tssi_shift = idle_tssi_shift + 4;
	adj_tssi_min = MAX(tssi_floor, idle_tssi_shift);
	/* convert to 7 bits */
	adj_tssi_min = adj_tssi_min >> 3;

	pwr = wlc_phy_tssi2dbm_acphy(pi, adj_tssi_min, a1, b0, b1);

	return pwr;
}

uint8
wlc_phy_tssi2dbm_acphy(phy_info_t *pi, int32 tssi, int32 a1, int32 b0, int32 b1)
{
		int32 num, den;
		uint8 pwrest = 0;
		int8 core;

		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			num = 8*(16*b0+b1*tssi);
			den = 32768+a1*tssi;
			pwrest = MAX(((4*num+den/2)/den), -8);
			pwrest = MIN(pwrest, 0x7F);
		}
		return pwrest;
}


void
wlc_phy_get_paparams_for_band_acphy(phy_info_t *pi, int16 *a1, int16 *b0, int16 *b1)
{

	srom11_pwrdet_t *pwrdet = &pi->pwrdet_ac;
	uint8 chan_freq_range, core;

	/* Get pwrdet params from SROM for current subband */
	chan_freq_range = wlc_phy_get_chan_freq_range_acphy(pi, 0);

	FOREACH_CORE(pi, core) {
		switch (chan_freq_range) {
		case WL_CHAN_FREQ_RANGE_2G:
		case WL_CHAN_FREQ_RANGE_5G_BAND0:
		case WL_CHAN_FREQ_RANGE_5G_BAND1:
		case WL_CHAN_FREQ_RANGE_5G_BAND2:
		case WL_CHAN_FREQ_RANGE_5G_BAND3:
			*a1 =  (int16)pwrdet->pwrdet_a1[core][chan_freq_range];
			*b0 =  (int16)pwrdet->pwrdet_b0[core][chan_freq_range];
			*b1 =  (int16)pwrdet->pwrdet_b1[core][chan_freq_range];
			PHY_TXPWR(("wl%d: %s: pwrdet core%d: a1=%d b0=%d b1=%d\n",
				pi->sh->unit, __FUNCTION__, core,
				*a1, *b0, *b1));
			break;
		}
	}
}

static
void wlc_phy_get_tssi_floor_acphy(phy_info_t *pi, int16 *floor)
{
	srom11_pwrdet_t *pwrdet = &pi->pwrdet_ac;
	uint8 chan_freq_range, core;

	chan_freq_range = wlc_phy_get_chan_freq_range_acphy(pi, 0);

	FOREACH_CORE(pi, core) {
		switch (chan_freq_range) {
		case WL_CHAN_FREQ_RANGE_2G:
		case WL_CHAN_FREQ_RANGE_5G_BAND0:
		case WL_CHAN_FREQ_RANGE_5G_BAND1:
		case WL_CHAN_FREQ_RANGE_5G_BAND2:
		case WL_CHAN_FREQ_RANGE_5G_BAND3:
			*floor = pwrdet->tssifloor[core][chan_freq_range];
		break;
		}
	}

}

/* get the complex freq. if chan==0, use default radio channel */
uint8
wlc_phy_get_chan_freq_range_acphy(phy_info_t *pi, uint channel)
{
	int freq;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (channel == 0)
		channel = CHSPEC_CHANNEL(pi->radio_chanspec);

	if (RADIOID(pi->pubpi.radioid) == BCM2069_ID) {
		chan_info_radio2069_t *t;
		chan_info_radio2069revGE16_t *tGE16;

		if (!wlc_phy_chan2freq_acphy(pi, channel, &freq, &t, &tGE16)) {
			PHY_ERROR(("wl%d: %s: channel %d invalid\n",
				pi->sh->unit, __FUNCTION__, channel));
		    ASSERT(0);
		}
	} else {
		chan_info_radio20691_t *t;

		ASSERT(RADIOID(pi->pubpi.radioid) == BCM20691_ID);
		if (!wlc_phy_chan2freq_acdcphy(pi, channel, &freq, &t)) {
			PHY_ERROR(("wl%d: %s: channel %d invalid\n",
				pi->sh->unit, __FUNCTION__, channel));
		    ASSERT(0);
		}
	}

	if (channel <= CH_MAX_2G_CHANNEL)
		return WL_CHAN_FREQ_RANGE_2G;
		if (pi->sh->subband5Gver == PHY_SUBBAND_4BAND) {
			if ((freq >= PHY_SUBBAND_4BAND_BAND0) &&
				(freq < PHY_SUBBAND_4BAND_BAND1))
				return WL_CHAN_FREQ_RANGE_5G_BAND0;
			else if ((freq >= PHY_SUBBAND_4BAND_BAND1) &&
				(freq < PHY_SUBBAND_4BAND_BAND2))
				return WL_CHAN_FREQ_RANGE_5G_BAND1;
			else if ((freq >= PHY_SUBBAND_4BAND_BAND2) &&
				(freq < PHY_SUBBAND_4BAND_BAND3))
				return WL_CHAN_FREQ_RANGE_5G_BAND2;
			else
				return WL_CHAN_FREQ_RANGE_5G_BAND3;
		} else if (pi->sh->subband5Gver == PHY_SUBBAND_3BAND_EMBDDED) {
			if ((freq >= EMBEDDED_LOW_5G_CHAN) && (freq < EMBEDDED_MID_5G_CHAN)) {
				return WL_CHAN_FREQ_RANGE_5GL;
			} else if ((freq >= EMBEDDED_MID_5G_CHAN) &&
			           (freq < EMBEDDED_HIGH_5G_CHAN)) {
				return WL_CHAN_FREQ_RANGE_5GM;
			} else {
				return WL_CHAN_FREQ_RANGE_5GH;
			}
		} else if (pi->sh->subband5Gver == PHY_SUBBAND_3BAND_HIGHPWR) {
			if ((freq >= HIGHPWR_LOW_5G_CHAN) && (freq < HIGHPWR_MID_5G_CHAN)) {
				return WL_CHAN_FREQ_RANGE_5GL;
			} else if ((freq >= HIGHPWR_MID_5G_CHAN) && (freq < HIGHPWR_HIGH_5G_CHAN)) {
				return WL_CHAN_FREQ_RANGE_5GM;
			} else {
				return WL_CHAN_FREQ_RANGE_5GH;
			}
		} else /* Default PPR Subband subband5Gver = 7 */
			if ((freq >= JAPAN_LOW_5G_CHAN) && (freq < JAPAN_MID_5G_CHAN)) {
				return WL_CHAN_FREQ_RANGE_5GL;
			} else if ((freq >= JAPAN_MID_5G_CHAN) && (freq < JAPAN_HIGH_5G_CHAN)) {
				return WL_CHAN_FREQ_RANGE_5GM;
			} else {
				return WL_CHAN_FREQ_RANGE_5GH;
			}
}

static void
BCMATTACHFN(wlc_phy_txpwrctrl_config_acphy)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	pi->hwpwrctrl_capable = TRUE;
	pi->txpwrctrl = PHY_TPC_HW_ON;

	pi->phy_5g_pwrgain = TRUE;
}

static void
wlc_phy_txpwrctrl_pwr_setup_acphy(phy_info_t *pi)
{
	uint8 stall_val;
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	srom11_pwrdet_t *pwrdet = &pi->pwrdet_ac;
	int8   target_pwr_qtrdbm[PHY_CORE_MAX];
	int8   target_min_limit;
	int16  a1[PHY_CORE_MAX], b0[PHY_CORE_MAX], b1[PHY_CORE_MAX];
	uint8  chan_freq_range;
	uint8  iidx = 20;
	uint8  core;
	int32  num, den, pwr_est;
	uint32 tbl_len, tbl_offset, idx, shfttbl_len;
	uint16 regval[128];
	uint32 shfttblval[24];
	uint8  tssi_delay;
	uint32 pdoffs = 0;
	int16 tssifloor = 1023;
	uint8 maxpwr = 0;

	tbl_len = 128;
	tbl_offset = 0;
	shfttbl_len = 24;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	/* enable TSSI */
	MOD_PHYREG(pi, ACPHY_TSSIMode, tssiEn, 1);
	MOD_PHYREG(pi, ACPHY_TxPwrCtrlCmd, txPwrCtrl_en, 0);

	/* initialize a1, b0, b1 to be all zero */
	for (idx = 0; idx < PHY_CORE_MAX; idx++) {
		a1[idx] = 0;
		b0[idx] = 0;
		b1[idx] = 0;
	}

	/* Get pwrdet params from SROM for current subband */
	chan_freq_range = wlc_phy_get_chan_freq_range_acphy(pi, 0);

	FOREACH_CORE(pi, core) {
		switch (chan_freq_range) {
		case WL_CHAN_FREQ_RANGE_2G:
		case WL_CHAN_FREQ_RANGE_5G_BAND0:
		case WL_CHAN_FREQ_RANGE_5G_BAND1:
		case WL_CHAN_FREQ_RANGE_5G_BAND2:
		case WL_CHAN_FREQ_RANGE_5G_BAND3:
			a1[core] =  pwrdet->pwrdet_a1[core][chan_freq_range];
			b0[core] =  pwrdet->pwrdet_b0[core][chan_freq_range];
			b1[core] =  pwrdet->pwrdet_b1[core][chan_freq_range];
			PHY_TXPWR(("wl%d: %s: pwrdet core%d: a1=%d b0=%d b1=%d\n",
				pi->sh->unit, __FUNCTION__, core,
				a1[core], b0[core], b1[core]));
			break;
		}

		switch (core) {
			case 0:
				if (a1[0] == 0) {
					a1[0] = 0xFF49;
					b0[0] = 0x12D9;
					b1[0] = 0xFD99;
					PHY_ERROR(("#### using default pa params"
						"for core 0 #######\n"));
				}
				break;
			case 1:
				if (a1[1] == 0) {
					a1[1] = 0xFF54;
					b0[1] = 0x1212;
					b1[1] = 0xFD89;
					PHY_ERROR(("#### using default pa params"
						"for core 1 #######\n"));
				}
				break;
			case 2:
				if (a1[2] == 0) {
					a1[2] = 0xFF53;
					b0[2] = 0x11B7;
					b1[2] = 0xFDC0;
					PHY_ERROR(("#### using default pa params"
						"for core 2 #######\n"));
				}
				break;
			default:
				break;
		}
	}

	/* target power */
	target_min_limit = pi->min_txpower * WLC_TXPWR_DB_FACTOR;
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
#ifdef PPR_API
		target_pwr_qtrdbm[core] = (int8)pi->tx_power_max_per_core[core] +
		    pi_ac->txpwr_offset[core];
#else
		target_pwr_qtrdbm[core] = (int8)pi->tx_power_max +
		    pi_ac->txpwr_offset[core];
#endif
		/* never target below the min threashold */
		if (target_pwr_qtrdbm[core] < target_min_limit)
			target_pwr_qtrdbm[core] = target_min_limit;

		if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
			chan_freq_range = wlc_phy_get_chan_freq_range_acphy(pi, 0);

			FOREACH_CORE(pi, core) {
				tssifloor = (int16)pwrdet->tssifloor[core][chan_freq_range];
				if ((tssifloor != 0x3ff) && (tssifloor != 0)) {
					maxpwr = wlc_phy_set_txpwr_clamp_acphy(pi);
					if (maxpwr < target_pwr_qtrdbm[core]) {
						target_pwr_qtrdbm[core] = maxpwr;
					}
				}
			}
		}


	/*	PHY_ERROR(("####targetPwr: %d#######\n", target_pwr_qtrdbm[core])); */
	}
	/* determine pos/neg TSSI slope */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		MOD_PHYREG(pi, ACPHY_TSSIMode, tssiPosSlope, pi->srom_fem2g.tssipos);
	} else {
		MOD_PHYREG(pi, ACPHY_TSSIMode, tssiPosSlope, pi->srom_fem5g.tssipos);
	}
	MOD_PHYREG(pi, ACPHY_TSSIMode, tssiPosSlope, 1);

	/* disable txpwrctrl during idleTssi measurement, etc */
	MOD_PHYREG(pi, ACPHY_TxPwrCtrlCmd, txPwrCtrl_en, 0);

	/* set power index initial condition */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		wlc_phy_txpwrctrl_set_cur_index_acphy(pi, iidx, core);
	}
	/* MOD_PHYREG(pi, ACPHY_TxPwrCtrlIdleTssi, rawTssiOffsetBinFormat, 1); */

	/* set idle TSSI in 2s complement format (max is 0x1ff) */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		wlc_phy_txpwrctrl_set_idle_tssi_acphy(pi, pi_ac->idle_tssi[core], core);
	}

	/* sample TSSI at 7.5us */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		if (pi->u.pi_acphy->srom_2g_pdrange_id >= 5) {
			tssi_delay = 200;
		} else if (pi->u.pi_acphy->srom_2g_pdrange_id >= 4) {
			tssi_delay = 220;
		} else {
			tssi_delay = 150;
		}
	} else {
		if (pi->u.pi_acphy->srom_5g_pdrange_id >= 5) {
			tssi_delay = 200;
		} else if (pi->u.pi_acphy->srom_5g_pdrange_id >= 4) {
			tssi_delay = 220;
		} else {
			tssi_delay = 150;
		}
	}
	MOD_PHYREG(pi, ACPHY_TxPwrCtrlNnum, Ntssi_delay, tssi_delay);

	/* average over 16 = 2^4 packets */
	MOD_PHYREG(pi, ACPHY_TxPwrCtrlNnum, Npt_intg_log2, 4);

	/* decouple IQ comp and LOFT comp from Power Control */
	MOD_PHYREG(pi, ACPHY_TxPwrCtrlCmd, use_txPwrCtrlCoefsIQ, 0);
	if (ACREV_IS(pi->pubpi.phy_rev, 1)) {
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlCmd, use_txPwrCtrlCoefsLO, 1);
	} else {
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlCmd, use_txPwrCtrlCoefsLO, 0);
	}

	/* 4360B0 using 0.5dB-step gaintbl, bbmult interpolation enabled */
	if (RADIOREV(pi->pubpi.radiorev) == 4) {
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlCmd, bbMultInt_en, 1);
	} else {
	/* disable bbmult interpolation to work with a 0.25dB step txGainTbl */
		MOD_PHYREG(pi, ACPHY_TxPwrCtrlCmd, bbMultInt_en, 0);
	}
	/* adding maxCap for each Tx chain */
	if (0) {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			if (core == 0) {
				MOD_PHYREG(pi, ACPHY_TxPwrCapping_path0, maxTxPwrCap_path0, 80);
			} else if (core == 1) {
				MOD_PHYREG(pi, ACPHY_TxPwrCapping_path1, maxTxPwrCap_path1, 32);
			} else if (core == 2) {
				MOD_PHYREG(pi, ACPHY_TxPwrCapping_path2, maxTxPwrCap_path2, 32);
			}
		}
	}
#ifdef WL_SARLIMIT
	wlc_phy_set_sarlimit_acphy(pi);
#endif
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		wlc_phy_txpwrctrl_set_target_acphy(pi, target_pwr_qtrdbm[core], core);
	}

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {

		PHY_TXPWR(("wl%d: %s: txpwrctl[%d]: %d\n",
			pi->sh->unit, __FUNCTION__, core,
			target_pwr_qtrdbm[core]));
	}

	/* load estimated power tables (maps TSSI to power in dBm)
	 *    entries in tx power table 0000xxxxxx
	 */
	tbl_len = 128;
	tbl_offset = 0;
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		for (idx = 0; idx < tbl_len; idx++) {
			num = 8 * (16 * b0[core] + b1[core] * idx);
			den = 32768 + a1[core] * idx;
			pwr_est = MAX(((4 * num + den/2)/den), -8);
			pwr_est = MIN(pwr_est, 0x7F);
			regval[idx] = (uint16)(pwr_est&0xff);
		}
		/* Est Pwr Table is 128x8 Table. Limit Write to 8 bits */
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_ESTPWRLUTS(core), tbl_len,
		                          tbl_offset, 16, regval);
	}

	/* start to populate estPwrShftTbl */
	for (idx = 0; idx < 24; idx++) {
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			if ((idx == 0)||((idx > 1)&&(idx < 5))||((idx > 6)&&(idx < 10))) {
				shfttblval[idx] = 0;
			} else if ((idx == 1)||((idx > 4)||(idx < 7))) {
				pdoffs = 0;
				for (core = 0; core < 3; core ++) {
					pdoffs = wlc_phy_pdoffset_cal_acphy(pdoffs,
						pwrdet->pdoffset40[pi->pubpi.phy_corenum-core-1],
						chan_freq_range);
				}
				shfttblval[idx] = pdoffs & 0xffffff;
			} else if (idx == 10) {
				pdoffs = 0;
				for (core = 0; core < 3; core ++) {
					pdoffs = wlc_phy_pdoffset_cal_acphy(pdoffs,
						pwrdet->pdoffset80[pi->pubpi.phy_corenum-core-1],
						chan_freq_range);
				}
				shfttblval[idx] = pdoffs & 0xffffff;
			} else {
				shfttblval[idx] = 0;
			}
		} else {
			/* hardcoding for 4335 wlbga for now, will add nvram var later if needed */
			if (
				CHIPID(pi->sh->chip) == BCM4335_CHIP_ID) {
				if ((idx >= 16) && (pi->sh->boardtype == 0x064c)) {
					pdoffs = 0;
					FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
						pdoffs = wlc_phy_pdoffset_cal_2g_acphy(pdoffs,
						pwrdet->pdoffsetcck[pi->pubpi.phy_corenum-core-1]);
					}
					shfttblval[idx] = pdoffs & 0xffffff;
				} else {
					shfttblval[idx] = 0;
				}
			} else {
				if (pwrdet->pdoffset2g40_flag == 1) {
					shfttblval[idx] = 0;
				} else {
					shfttblval[idx] = 0;
					if (idx == 5) {
						pdoffs = 0;
						for (core = 0; core < 3; core ++) {
						pdoffs = wlc_phy_pdoffset_cal_2g_acphy(pdoffs,
							pwrdet->pdoffset2g40[2-core]);
						}
						shfttblval[idx] = pdoffs & 0xffffff;
					}
				}
			}
		}
	}
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_ESTPWRSHFTLUTS, shfttbl_len,
	                          tbl_offset, 32, shfttblval);

	ACPHY_ENABLE_STALL(pi, stall_val);
}
#ifdef WL_SARLIMIT
void
wlc_phy_set_sarlimit_acphy(phy_info_t *pi)
{
	IF_ACTV_CORE(pi, pi->sh->phyrxchain, 0)
		MOD_PHYREG(pi, ACPHY_TxPwrCapping_path0, maxTxPwrCap_path0, pi->sarlimit[0]);
	IF_ACTV_CORE(pi, pi->sh->phyrxchain, 1)
		MOD_PHYREG(pi, ACPHY_TxPwrCapping_path1, maxTxPwrCap_path1, pi->sarlimit[1]);
	IF_ACTV_CORE(pi, pi->sh->phyrxchain, 2)
		MOD_PHYREG(pi, ACPHY_TxPwrCapping_path2, maxTxPwrCap_path2, pi->sarlimit[2]);
}
#endif /* WL_SARLIMIT */
static uint32
wlc_phy_pdoffset_cal_acphy(uint32 pdoffs, uint16 pdoffset, uint8 band)
{
	uint8 pdoffs_t;
	pdoffs_t = (pdoffset >> ((band -1)*4))&0xf;
	pdoffs_t = (pdoffs_t > 7) ? (0xf0|pdoffs_t) : pdoffs_t;
	pdoffs = (pdoffs<<8)|pdoffs_t;
	return pdoffs;
}

static uint32
wlc_phy_pdoffset_cal_2g_acphy(uint32 pdoffs, uint8 pdoffset)
{
	uint8 pdoffs_t;
	pdoffs_t = (pdoffset > 7) ? (0xf0|pdoffset) : pdoffset;
	pdoffs = (pdoffs<<8)|pdoffs_t;
	return pdoffs;
}

/* set txgain in case txpwrctrl is disabled (fixed power) */
static void
wlc_phy_txpwr_fixpower_acphy(phy_info_t *pi)
{
	uint16 core;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		wlc_phy_txpwr_by_index_acphy(pi, (1 << core), pi->u.pi_acphy->txpwrindex[core]);
	}
}

static void
wlc_phy_rx_iq_comp_acphy(phy_info_t *pi, uint8 write, phy_iq_comp_t *pcomp, uint8 rx_core)
{
	/* write: 0 - fetch values from phyregs into *pcomp
	 *        1 - deposit values from *pcomp into phyregs
	 *        2 - set all coeff phyregs to 0
	 *
	 * rx_core: specify which core to fetch/deposit
	 */

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(write <= 2);

	/* write values */
	if (write == 0) {
		pcomp->a = phy_reg_read(pi, ACPHY_RxIQCompA(rx_core));
		pcomp->b = phy_reg_read(pi, ACPHY_RxIQCompB(rx_core));
	} else if (write == 1) {
		phy_reg_write(pi, ACPHY_RxIQCompA(rx_core), pcomp->a);
		phy_reg_write(pi, ACPHY_RxIQCompB(rx_core), pcomp->b);
	} else {
		phy_reg_write(pi, ACPHY_RxIQCompA(rx_core), 0);
		phy_reg_write(pi, ACPHY_RxIQCompB(rx_core), 0);
	}
}


#if defined(BCMDBG_RXCAL)
static void
wlc_phy_rxcal_snr_acphy(phy_info_t *pi, uint16 num_samps, uint8 core_mask)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	uint16 bbmult_orig[PHY_CORE_MAX], bbmult_zero = 0;
	phy_iq_est_t  noise_vals[PHY_CORE_MAX];
	uint8 core;

	/* take noise measurement (for SNR calc, for information purposes only) */
	FOREACH_ACTV_CORE(pi, core_mask, core) {
		wlc_phy_get_tx_bbmult_acphy(pi, &(bbmult_orig[core]), core);
		wlc_phy_set_tx_bbmult_acphy(pi, &bbmult_zero, core);
	}

	wlc_phy_rx_iq_est_acphy(pi, noise_vals, num_samps, 32, 0);

	FOREACH_ACTV_CORE(pi, core_mask, core) {
		/* Store the noise powers for SNR calculations later */
		pi_ac->rxcal_noise[core].i_pwr = noise_vals[core].i_pwr;
		pi_ac->rxcal_noise[core].q_pwr = noise_vals[core].q_pwr;
		pi_ac->rxcal_noise[core].iq_prod = noise_vals[core].iq_prod;
	}

	FOREACH_ACTV_CORE(pi, core_mask, core) {
		wlc_phy_set_tx_bbmult_acphy(pi, &(bbmult_orig[core]), core);
	}
}
#endif /* BCMDBG_RXCAL */

static void
wlc_phy_rxcal_phy_setup_acphy(phy_info_t *pi)
{

	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_rxcal_phyregs_t *porig = &(pi_ac->ac_rxcal_phyregs_orig);
	uint16 radio_rev_id;
	uint8 core;
	uint16 sdadc_config, addr_lo, val16;
	uint8 bw_idx;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	radio_rev_id = read_radio_reg(pi, RF_2069_REV_ID(0));

	if (CHSPEC_IS80(pi->radio_chanspec)) {
		bw_idx = 2;
		sdadc_config = sdadc_cfg80;
	} else if (CHSPEC_IS40(pi->radio_chanspec)) {
		bw_idx = 1;
		sdadc_config = sdadc_cfg40;
	} else {
		bw_idx = 0;
		sdadc_config = sdadc_cfg20;
	}

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(!porig->is_orig);
	porig->is_orig = TRUE;

	porig->AfePuCtrl = phy_reg_read(pi, ACPHY_AfePuCtrl);

	/* turn off tssi sleep feature during cal */
	MOD_PHYREG(pi, ACPHY_AfePuCtrl, tssiSleepEn, 0);


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, rxfe_bilge_cnt, 4);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 1);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 0);
	}

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		porig->txpwridx[core] = pi->u.pi_acphy->txpwrindex[core];

		porig->RfctrlOverrideTxPus[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlOverrideTxPus, core));
		porig->RfctrlOverrideRxPus[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlOverrideRxPus, core));
		porig->RfctrlOverrideGains[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlOverrideGains, core));
		porig->RfctrlOverrideLpfCT[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlOverrideLpfCT, core));
		porig->RfctrlOverrideLpfSwtch[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlOverrideLpfSwtch, core));
		porig->RfctrlOverrideAfeCfg[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlOverrideAfeCfg, core));
		porig->RfctrlOverrideLowPwrCfg[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlOverrideLowPwrCfg, core));
		porig->RfctrlOverrideAuxTssi[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlOverrideAuxTssi, core));

		porig->RfctrlCoreTxPus[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreTxPus, core));
		porig->RfctrlCoreRxPus[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreRxPus, core));
		porig->RfctrlCoreTXGAIN1[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreTXGAIN1, core));
		porig->RfctrlCoreTXGAIN2[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreTXGAIN2, core));
		porig->RfctrlCoreRXGAIN1[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreRXGAIN1, core));
		porig->RfctrlCoreRXGAIN2[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreRXGAIN2, core));
		porig->RfctrlCoreLpfGain[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreLpfGain, core));
		porig->RfctrlCoreLpfCT[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreLpfCT, core));
		porig->RfctrlCoreLpfGmult[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreLpfGmult, core));
		porig->RfctrlCoreRCDACBuf[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreRCDACBuf, core));
		porig->RfctrlCoreLpfSwtch[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreLpfSwtch, core));
		porig->RfctrlCoreAfeCfg1[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreAfeCfg1, core));
		porig->RfctrlCoreAfeCfg2[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreAfeCfg2, core));
		porig->RfctrlCoreLowPwr[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreLowPwr, core));
		porig->RfctrlCoreAuxTssi1[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreAuxTssi1, core));
		porig->RfctrlCoreAuxTssi2[core] = phy_reg_read(pi,
			ACPHYREGCE(RfctrlCoreAuxTssi2, core));
		porig->Dac_gain[core] = phy_reg_read(pi, ACPHYREGCE(Dac_gain, core));

		wlc_phy_get_tx_bbmult_acphy(pi, &(porig->bbmult[core]), core);
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x100 + core), 16,
			&porig->rfseq_txgain[core+0]);
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x103 + core), 16,
			&porig->rfseq_txgain[core+3]);
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x106 + core), 16,
			&porig->rfseq_txgain[core+6]);


		porig->RfctrlIntc[core] = phy_reg_read(pi, ACPHYREGCE(RfctrlIntc, core));
	}

	porig->RfseqCoreActv2059 = phy_reg_read(pi, ACPHY_RfseqCoreActv2059);


	MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, EnTx, pi->sh->phyrxchain);
	MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, DisRx, 0);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {

		MOD_PHYREGCE(pi, RfctrlIntc, core, ext_2g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, core, ext_5g_papu, 0);
		MOD_PHYREGCE(pi, RfctrlIntc, core, override_ext_pa, 1);


		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, fast_nap_bias_pu, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, fast_nap_bias_pu, 1);


		/* Setting the SD-ADC related stuff */
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, core, afe_iqadc_mode, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_iqadc_mode,
			sdadc_config & 0x7);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, core, afe_iqadc_pwrup, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg1, core, afe_iqadc_pwrup,
			(sdadc_config >> 3) & 0x3f);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, core, afe_iqadc_flashhspd, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_iqadc_flashhspd,
			(sdadc_config >> 9) & 0x1);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, core, afe_ctrl_flash17lvl, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_ctrl_flash17lvl,
			(sdadc_config >> 10) & 0x1);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, core, afe_iqadc_adc_bias, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_iqadc_adc_bias,
			(sdadc_config >> 11) & 0x3);

		/* Turning off all the RF component that are not needed */
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rxrf_lna2_pwrup, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, rxrf_lna2_pwrup, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rxrf_lna1_pwrup, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, rxrf_lna1_pwrup, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rssi_wb1g_pu, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, rssi_wb1g_pu, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, rxrf_lna2_wrssi2_pwrup, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, rxrf_lna2_wrssi2_pwrup, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, lpf_nrssi_pwrup, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, lpf_nrssi_pwrup, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, lpf_wrssi3_pwrup, 1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus, core, lpf_wrssi3_pwrup, 0);

		/* Turn on PA for iPA chip, turn off for ePA chip */
		radio_rev_id = read_radio_reg(pi, RF_2069_REV_ID(core));
		MOD_PHYREGCE(pi, RfctrlOverrideTxPus, core, pa_pwrup, 1);
		if (radio_rev_id == 0 || radio_rev_id == 1 ||
		    ACRADIO_IPA1X1_IS(radio_rev_id)) {
			MOD_PHYREGCE(pi, RfctrlCoreTxPus, core, pa_pwrup, 1);
		} else if (radio_rev_id == 2 || radio_rev_id == 3 ||
			radio_rev_id == 4 || ACRADIO_EPA1X1_IS(radio_rev_id)) {
			MOD_PHYREGCE(pi, RfctrlCoreTxPus, core, pa_pwrup, 0);
		}


		if (radio_rev_id == 4 || (ACREV1X1_IS(pi->pubpi.phy_rev))) {
			if (CHSPEC_IS5G(pi->radio_chanspec)) {
				MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, rxrf_lna2_pwrup, 1);
			}
		}
		/* 4335a0/b0 epa : turn on lna2 */
		if (ACRADIO_EPA1X1_IS(radio_rev_id)) {
			MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, rxrf_lna2_pwrup, 1);
		}


		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, tia_DC_loop_PU, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, tia_DC_loop_PU, 1);


		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_dac_adc, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_dac_adc, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_aux_bq1, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_aux_bq1, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_iqcal_bq1, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_iqcal_bq1, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_bq2_rc, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_bq2_rc, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_bq1_adc, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_bq1_adc, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_dac_bq2, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_dac_bq2, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_bq2_adc, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_bq2_adc, 1);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_dac_rc, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_dac_rc, 1);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_bq1_bq2, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_bq1_bq2, 1);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfSwtch, core, lpf_sw_tia_bq1, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfSwtch, core, lpf_sw_tia_bq1, 1);

		addr_lo = (bw_idx < 2) ? 0x140 + 0x10 * core + bw_idx :	0x441 + 0x2 * core;
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, addr_lo, 16, &val16);

		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT, core, lpf_bq1_bw, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfCT, core, lpf_bq1_bw, val16 & 7);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT, core, lpf_bq2_bw, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfCT, core, lpf_bq2_bw, (val16 >> 3) & 7);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT, core, lpf_g_mult, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfGmult, core, lpf_g_mult, (val16 >> 6) & 0xff);

		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT, core, lpf_dc_bypass, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfCT, core, lpf_dc_bypass, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideLpfCT, core, lpf_q_biq2, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfCT, core, lpf_q_biq2, 1);

		MOD_PHYREGCE(pi, RfctrlOverrideRxPus, core, lpf_pu_dc, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus, core, lpf_pu_dc, 1);

		porig->PapdEnable[core] = phy_reg_read(pi, ACPHYREGCE(PapdEnable, core));
		MOD_PHYREGCEE(pi, PapdEnable, core, papd_compEnb, 0);
	}

		ACPHY_ENABLE_STALL(pi, stall_val);
}


static void
wlc_phy_rxcal_phy_cleanup_acphy(phy_info_t *pi)
{

	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_rxcal_phyregs_t *porig = &(pi_ac->ac_rxcal_phyregs_orig);
	uint8 core;
	uint8 stall_val;

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(porig->is_orig);
	porig->is_orig = FALSE;

	phy_reg_write(pi, ACPHY_RfseqCoreActv2059, porig->RfseqCoreActv2059);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {

	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x100 + core), 16,
		&porig->rfseq_txgain[core + 0]);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x103 + core), 16,
		&porig->rfseq_txgain[core + 3]);
	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0x106 + core), 16,
		&porig->rfseq_txgain[core + 6]);


		wlc_phy_txpwr_by_index_acphy(pi, (1 << core), porig->txpwridx[core]);
		wlc_phy_set_tx_bbmult_acphy(pi, &(porig->bbmult[core]), core);

		phy_reg_write(pi, ACPHYREGCE(RfctrlIntc, core), porig->RfctrlIntc[core]);
		phy_reg_write(pi, ACPHYREGCE(PapdEnable, core), porig->PapdEnable[core]);

		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideTxPus, core),
			porig->RfctrlOverrideTxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideRxPus, core),
			porig->RfctrlOverrideRxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideGains, core),
			porig->RfctrlOverrideGains[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideLpfCT, core),
			porig->RfctrlOverrideLpfCT[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideLpfSwtch, core),
			porig->RfctrlOverrideLpfSwtch[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAfeCfg, core),
			porig->RfctrlOverrideAfeCfg[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAuxTssi, core),
			porig->RfctrlOverrideAuxTssi[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideLowPwrCfg, core),
			porig->RfctrlOverrideLowPwrCfg[core]);

		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreTxPus, core),
			porig->RfctrlCoreTxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRxPus, core),
			porig->RfctrlCoreRxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreTXGAIN1, core),
			porig->RfctrlCoreTXGAIN1[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreTXGAIN2, core),
			porig->RfctrlCoreTXGAIN2[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN1, core),
			porig->RfctrlCoreRXGAIN1[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN2, core),
			porig->RfctrlCoreRXGAIN2[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfGain, core),
			porig->RfctrlCoreLpfGain[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfCT, core),
			porig->RfctrlCoreLpfCT[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfGmult, core),
			porig->RfctrlCoreLpfGmult[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRCDACBuf, core),
			porig->RfctrlCoreRCDACBuf[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfSwtch, core),
			porig->RfctrlCoreLpfSwtch[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg1, core),
			porig->RfctrlCoreAfeCfg1[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg2, core),
			porig->RfctrlCoreAfeCfg2[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLowPwr, core),
			porig->RfctrlCoreLowPwr[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAuxTssi1, core),
			porig->RfctrlCoreAuxTssi1[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAuxTssi2, core),
			porig->RfctrlCoreAuxTssi2[core]);
		phy_reg_write(pi, ACPHYREGCE(Dac_gain, core), porig->Dac_gain[core]);
	}
	phy_reg_write(pi, ACPHY_AfePuCtrl, porig->AfePuCtrl);
	wlc_phy_force_rfseq_acphy(pi, ACPHY_RFSEQ_RESET2RX);

	ACPHY_ENABLE_STALL(pi, stall_val);
}

static void
wlc_phy_rxcal_radio_setup_acphy(phy_info_t *pi)
{

	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_rxcal_radioregs_t *porig = &(pi_ac->ac_rxcal_radioregs_orig);
	uint16 radio_rev_id;
	uint16 tx_atten, rx_atten;
	uint8 core;

	tx_atten = 0;
	rx_atten = 0;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	radio_rev_id = read_radio_reg(pi, RF_2069_REV_ID(0));

	ASSERT(!porig->is_orig);
	porig->is_orig = TRUE;

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		porig->RF_2069_TXRX2G_CAL_TX[core] =
			read_radio_reg(pi, RF_2069_TXRX2G_CAL_TX(core));
		porig->RF_2069_TXRX5G_CAL_TX[core] =
			read_radio_reg(pi, RF_2069_TXRX5G_CAL_TX(core));
		porig->RF_2069_TXRX2G_CAL_RX[core] =
			read_radio_reg(pi, RF_2069_TXRX2G_CAL_RX(core));
		porig->RF_2069_TXRX5G_CAL_RX[core] =
			read_radio_reg(pi, RF_2069_TXRX5G_CAL_RX(core));
		porig->RF_2069_RXRF2G_CFG2[core] =
			read_radio_reg(pi, RF_2069_RXRF2G_CFG2(core));
		porig->RF_2069_RXRF5G_CFG2[core] =
			read_radio_reg(pi, RF_2069_RXRF5G_CFG2(core));

		/* Disable all loopback options first */
		write_radio_reg(pi, RF_2069_TXRX2G_CAL_TX(core), 0);
		write_radio_reg(pi, RF_2069_TXRX5G_CAL_TX(core), 0);
		write_radio_reg(pi, RF_2069_TXRX2G_CAL_RX(core), 0);
		write_radio_reg(pi, RF_2069_TXRX5G_CAL_RX(core), 0);
		write_radio_reg(pi, RF_2069_RXRF2G_CFG2(core), 0);
		write_radio_reg(pi, RF_2069_RXRF5G_CFG2(core), 0);

		/* Disable PAPD paths
		 *  - Powerdown the papd loopback path on Rx side
		 *  - Disable the epapd
		 */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_RX(core),
				TXRX2G_CAL_RX, loopback2g_papdcal_pu, 0);
			MOD_RADIO_REGFLDC(pi, RF_2069_RXRF2G_CFG2(core),
				RXRF2G_CFG2, lna2g_epapd_en, 0);
		} else {
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_RX(core),
				TXRX5G_CAL_RX, loopback5g_papdcal_pu, 0);
			MOD_RADIO_REGFLDC(pi, RF_2069_RXRF5G_CFG2(core),
				RXRF5G_CFG2, lna5g_epapd_en, 0);
		}

		/* Disable RCCR Phase Shifter */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_RX(core),
				TXRX2G_CAL_RX, loopback2g_rxiqcal_cr_pu, 0);
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_RX(core),
				TXRX2G_CAL_RX, loopback2g_rxiqcal_rc_pu, 0);
		} else {
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_RX(core),
				TXRX5G_CAL_RX, loopback5g_rxiqcal_cr_pu, 0);
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_RX(core),
				TXRX5G_CAL_RX, loopback5g_rxiqcal_rc_pu, 0);
		}


		/* Enable Tx Path */
		radio_rev_id = read_radio_reg(pi, RF_2069_REV_ID(core));
		if (radio_rev_id == 0 || radio_rev_id == 1 ||
			ACRADIO_IPA1X1_IS(radio_rev_id)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_TX(core),
					TXRX2G_CAL_TX, i_calPath_pa2g_pu, 1);
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_TX(core),
					TXRX2G_CAL_TX, i_calPath_pad2g_pu, 0);
			} else {
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_TX(core),
					TXRX5G_CAL_TX, i_calPath_pa_pu_5g, 1);
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_TX(core),
					TXRX5G_CAL_TX, i_calPath_pad_pu_5g, 0);
			}
		} else if ((radio_rev_id == 2) || (radio_rev_id == 3) ||
		           (radio_rev_id == 4) || ACRADIO_EPA1X1_IS(radio_rev_id)) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_TX(core),
					TXRX2G_CAL_TX, i_calPath_pa2g_pu, 0);
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_TX(core),
					TXRX2G_CAL_TX, i_calPath_pad2g_pu, 1);
			} else {
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_TX(core),
					TXRX5G_CAL_TX, i_calPath_pa_pu_5g, 0);
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_TX(core),
					TXRX5G_CAL_TX, i_calPath_pad_pu_5g, 1);
			}
		}

		/* Enable Rx Path
		 *  - Powerup the master cal PU signal on Rx side (common to papd & rxiqcal).
		 *    Not needed for rx/cr rxiqcal PU.
		 *  - Powerup the rxiqcal loopback path on Rx side.
		 */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_RX(core),
				TXRX2G_CAL_RX, loopback2g_cal_pu, 1);
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_RX(core),
				TXRX2G_CAL_RX, loopback2g_rxiqcal_pu, 1);
		} else {


			if (radio_rev_id == 4 || (ACREV1X1_IS(pi->pubpi.phy_rev))) {
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_RX(core),
				TXRX5G_CAL_RX, loopback5g_lna12_mux, 1);

				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_RX(core),
				TXRX5G_CAL_RX, loopback5g_cal_pu, 1);
			} else {
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_RX(core),
				TXRX5G_CAL_RX, loopback5g_cal_pu, 1);
				MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_RX(core),
				TXRX5G_CAL_RX, loopback5g_rxiqcal_pu, 1);
			}
		}

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_TX(core),
				TXRX2G_CAL_TX, i_cal_pad_atten_2g, tx_atten);
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX2G_CAL_RX(core),
				TXRX2G_CAL_RX, loopback2g_rxiqcal_rx_attn, rx_atten);
		} else {
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_TX(core),
				TXRX5G_CAL_TX, i_cal_pad_atten_5g, tx_atten);
			MOD_RADIO_REGFLDC(pi, RF_2069_TXRX5G_CAL_RX(core),
				TXRX5G_CAL_RX, loopback5g_rxiqcal_rx_attn, rx_atten);
		}
	}

}

static void
wlc_phy_rxcal_radio_cleanup_acphy(phy_info_t *pi)
{

	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_rxcal_radioregs_t *porig = &(pi_ac->ac_rxcal_radioregs_orig);
	uint8 core;
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));


	ASSERT(porig->is_orig);
	porig->is_orig = FALSE;

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		write_radio_reg(pi, RF_2069_TXRX2G_CAL_TX(core),
			porig->RF_2069_TXRX2G_CAL_TX[core]);
		write_radio_reg(pi, RF_2069_TXRX5G_CAL_TX(core),
			porig->RF_2069_TXRX5G_CAL_TX[core]);
		write_radio_reg(pi, RF_2069_TXRX2G_CAL_RX(core),
			porig->RF_2069_TXRX2G_CAL_RX[core]);
		write_radio_reg(pi, RF_2069_TXRX5G_CAL_RX(core),
			porig->RF_2069_TXRX5G_CAL_RX[core]);
		write_radio_reg(pi, RF_2069_RXRF2G_CFG2(core),
			porig->RF_2069_RXRF2G_CFG2[core]);
		write_radio_reg(pi, RF_2069_RXRF5G_CFG2(core),
			porig->RF_2069_RXRF5G_CFG2[core]);
	}

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, rxfe_bilge_cnt, 0);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 1);
		MOD_PHYREG(pi, ACPHY_RxFeCtrl1, soft_sdfeFifoReset, 0);
	}

}


#define ACPHY_RXCAL_NUMGAINS 11
#define ACPHY_RXCAL_TONEAMP 181

typedef struct _acphy_rxcal_txrxgain {
	uint16 lpf_biq1;
	uint16 lpf_biq0;
	int8 txpwrindex;
} acphy_rxcal_txrxgain_t;

enum {
	ACPHY_RXCAL_GAIN_INIT = 0,
	ACPHY_RXCAL_GAIN_UP,
	ACPHY_RXCAL_GAIN_DOWN
};


/* see also: proc acphy_rx_iq_cal_txrxgain_control { core } */
static void
wlc_phy_rxcal_loopback_gainctrl_acphy(phy_info_t *pi)
{
	/*
	 * joint tx-rx gain control for Rx IQ calibration
	 */

	/* gain candidates tables,
	 * columns are: B1 B0 L2 Tx-Pwr-Idx
	 * rows are monotonically increasing gain
	 */
	uint8 stall_val;
	acphy_rxcal_txrxgain_t gaintbl_5G[ACPHY_RXCAL_NUMGAINS] =
		{
		{0, 0, 0},
		{0, 1, 0},
		{0, 2, 0},
		{0, 3, 0},
		{0, 4, 0},
		{1, 4, 0},
		{2, 4, 0},
		{3, 4, 0},
		{4, 4, 0},
		{5, 4, 0},
		{5, 5, 0}
		};
	acphy_rxcal_txrxgain_t gaintbl_2G[ACPHY_RXCAL_NUMGAINS] =
	 {
		{0, 0, 10},
		{0, 1, 10},
		{0, 2, 10},
		{0, 3, 10},
		{0, 4, 10},
		{0, 5, 10},
		{1, 5, 10},
		{2, 5, 10},
		{3, 5, 10},
		{4, 5, 10},
		{5, 5, 10}
	 };
	uint16 num_samps = 1024;
	uint32 thresh_pwr_hi = 5789 /* thresh_pwr (=4100)* 1.412 */;
	uint32 thresh_pwr_lo = 2903 /* thresh_pwr (=4100)/ 1.412 */;
	phy_iq_est_t est[PHY_CORE_MAX];
	/* threshold for "too high power"(313 mVpk, where clip = 400mVpk in 4322) */
	uint32 i_pwr, q_pwr, curr_pwr, optim_pwr = 0;
	uint32 curr_pwr_tmp;

	uint8 gainctrl_dirn[PHY_CORE_MAX];
	bool gainctrl_done[PHY_CORE_MAX];
	bool gainctrl_not_done;
	uint16 mix_tia_gain[PHY_CORE_MAX];
	int8 curr_gaintbl_index[PHY_CORE_MAX];

	acphy_rxcal_txrxgain_t *gaintbl;
	uint16 lpf_biq1_gain, lpf_biq0_gain;

	int8 txpwrindex;
	uint16 txgain_max_a[] = {0x00ff, 0xffff}; /* max txgain_LO & MI. Hi is set separately. */
	txgain_setting_t txgain_setting_tmp;
	uint16 txgain_txgm_val;

	uint8 core, lna2_gain = 0;
#if defined(BCMDBG_RXCAL)
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
#endif
	uint16 radio_rev_id;
	radio_rev_id = read_radio_reg(pi, RF_2069_REV_ID(0));
	if (CHSPEC_IS5G(pi->radio_chanspec)) {

		if (radio_rev_id == 4 || (ACREV1X1_IS(pi->pubpi.phy_rev))) {
			lna2_gain = 6;
		}
	}
	/* 4335a0/b0 epa : turn on lna2 */
	if (ACRADIO_EPA1X1_IS(radio_rev_id)) {
		lna2_gain = 6;
	}
	BCM_REFERENCE(optim_pwr);
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

#if defined(BCMDBG_RXCAL)
	printf("Rx IQCAL : Loopback Gain Control\n");
#endif /* BCMDBG_RXCAL */

	/* gain candidates */
	gaintbl = gaintbl_2G;
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		gaintbl = gaintbl_5G;
	}

	FOREACH_CORE(pi, core) {
		gainctrl_dirn[core] = ACPHY_RXCAL_GAIN_INIT;
		gainctrl_done[core] = FALSE;

		/* retrieve Rx Mixer/TIA gain from InitGain and via GainBits table */
		mix_tia_gain[core] = READ_PHYREGC(pi, InitGainCodeA, core, initmixergainIndex);

		curr_gaintbl_index[core] = 0;
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			curr_gaintbl_index[core] = 4;
		}
	}

	/* Retrieve Tx Mixer/Gm gain (so can inject below since this needs to be
	* invariant) -- (extract from Tx gain table)
	*/
	wlc_phy_get_txgain_settings_by_index_acphy(pi, &txgain_setting_tmp, 50);

	/* this is the bit-shifted TxGm gain code */
	txgain_txgm_val = txgain_setting_tmp.rad_gain_hi;


	do {

		FOREACH_CORE(pi, core) {
			if (!gainctrl_done[core]) {

				lpf_biq1_gain   = gaintbl[curr_gaintbl_index[core]].lpf_biq1;
				lpf_biq0_gain   = gaintbl[curr_gaintbl_index[core]].lpf_biq0;
				txpwrindex      = gaintbl[curr_gaintbl_index[core]].txpwrindex;

				/* rx */
				/* LNA1 bypass mode */
				phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN1, core),
				              mix_tia_gain[core] << 6 | lna2_gain << 3);

				if (CHSPEC_IS2G(pi->radio_chanspec)) {
					phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN2, core), 0);
				} else if (CHSPEC_IS5G(pi->radio_chanspec)) {
					phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN2, core), 4);
				}

				phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfGain, core),
					(lpf_biq1_gain << 3) | lpf_biq0_gain);

				MOD_PHYREGCE(pi, RfctrlOverrideGains, core, rxgain, 1);
				MOD_PHYREGCE(pi, RfctrlOverrideGains, core, lpf_bq1_gain, 1);
				MOD_PHYREGCE(pi, RfctrlOverrideGains, core, lpf_bq2_gain, 1);

				/* tx */
				if (txpwrindex == -1) {
					/* inject default TxGm value from above */
					wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
						(0x100 + core), 16, &txgain_max_a[0]);
					wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
						(0x103 + core), 16, &txgain_max_a[1]);
					wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1,
						(0x106 + core), 16, &txgain_txgm_val);
				} else {
					wlc_phy_txpwr_by_index_acphy(pi, (1 << core), txpwrindex);
				}
			}
		}

		/* turn on testtone (this will override bbmult, but that's ok) */
		wlc_phy_tx_tone_acphy(pi, (((CHSPEC_IS80(pi->radio_chanspec)) ?
			ACPHY_IQCAL_TONEFREQ_80MHz : (CHSPEC_IS40(pi->radio_chanspec)) ?
			ACPHY_IQCAL_TONEFREQ_40MHz : ACPHY_IQCAL_TONEFREQ_20MHz) >> 1),
			ACPHY_RXCAL_TONEAMP, 0, 0, FALSE);

		/* estimate digital power using rx_iq_est
		*/
		wlc_phy_rx_iq_est_acphy(pi, est, num_samps, 32, 0);

		/* Turn off the tone */
		wlc_phy_stopplayback_acphy(pi);

		gainctrl_not_done = FALSE;

		FOREACH_CORE(pi, core) {
			if (!gainctrl_done[core]) {

				i_pwr = (est[core].i_pwr + num_samps / 2) / num_samps;
				q_pwr = (est[core].q_pwr + num_samps / 2) / num_samps;
				curr_pwr = i_pwr + q_pwr;
			PHY_NONE(("core %u (gain idx %d): i_pwr = %u, q_pwr = %u, curr_pwr = %d\n",
				core, curr_gaintbl_index[core], i_pwr, q_pwr, curr_pwr));

#if defined(BCMDBG_RXCAL)
			printf("Core-%d : g_id=%d MX:%d LNA2: %d BQ0:%d BQ1:%d tx_id = %d Pwr=%d\n",
				core, curr_gaintbl_index[core], mix_tia_gain[core],
				lna2_gain,
				gaintbl[curr_gaintbl_index[core]].lpf_biq0,
				gaintbl[curr_gaintbl_index[core]].lpf_biq1,
				gaintbl[curr_gaintbl_index[core]].txpwrindex, curr_pwr);
#endif /* BCMDBG_RXCAL */

				switch (gainctrl_dirn[core]) {
				case ACPHY_RXCAL_GAIN_INIT:
					if (curr_pwr > thresh_pwr_hi) {
						gainctrl_dirn[core] = ACPHY_RXCAL_GAIN_DOWN;
						curr_pwr_tmp = curr_pwr;
						while ((curr_pwr_tmp > thresh_pwr_hi) &&
						(curr_gaintbl_index[core] > 1)) {
							curr_gaintbl_index[core]--;
							curr_pwr_tmp /= 2;
						}
					} else if  (curr_pwr < thresh_pwr_lo) {
						gainctrl_dirn[core] = ACPHY_RXCAL_GAIN_UP;
						curr_pwr_tmp = curr_pwr;
						if (curr_pwr_tmp != 0) {
							while ((curr_pwr_tmp < thresh_pwr_lo) &&
								(curr_gaintbl_index[core] <
								ACPHY_RXCAL_NUMGAINS - 3)) {
								curr_gaintbl_index[core]++;
								curr_pwr_tmp *= 2;
							}
						}
					} else {
						gainctrl_done[core] = TRUE;
						optim_pwr = curr_pwr;
					}
					break;

				case ACPHY_RXCAL_GAIN_UP:
					if (curr_pwr > thresh_pwr_lo) {
						gainctrl_done[core] = TRUE;
						optim_pwr = curr_pwr;
					} else {
						curr_gaintbl_index[core]++;
					}
					break;

				case ACPHY_RXCAL_GAIN_DOWN:
					if (curr_pwr > thresh_pwr_hi) {
						curr_gaintbl_index[core]--;
					} else {
						gainctrl_done[core] = TRUE;
						optim_pwr = curr_pwr;
					}
					break;

				default:
					PHY_ERROR(("Invalid gaintable direction id %d\n",
						gainctrl_dirn[core]));
					ASSERT(0);
				}

				if ((curr_gaintbl_index[core] < 0) ||
				(curr_gaintbl_index[core] >= ACPHY_RXCAL_NUMGAINS)) {
					gainctrl_done[core] = TRUE;
					optim_pwr = curr_pwr;
				}

				gainctrl_not_done = gainctrl_not_done || (!gainctrl_done[core]);

#if defined(BCMDBG_RXCAL)
				/* Store the signal powers for SNR calculations later */
				pi_ac->rxcal_signal[core].i_pwr = est[core].i_pwr;
				pi_ac->rxcal_signal[core].q_pwr = est[core].q_pwr;
				pi_ac->rxcal_signal[core].iq_prod = est[core].iq_prod;
#endif /* BCMDBG_RXCAL */
			}
		}

	} while (gainctrl_not_done);


	ACPHY_ENABLE_STALL(pi, stall_val);
}


enum {
	ACPHY_RXCAL_NORMAL = 0,
	ACPHY_RXCAL_LEAKAGE_COMP
};


static void
wlc_phy_calc_iq_mismatch_acphy(phy_iq_est_t *est, acphy_iq_mismatch_t *mismatch)
{

	/* angle = asin (-iq / sqrt( ii*qq ))
	* mag   = sqrt ( qq/ii )
	*/

	int32  iq = est->iq_prod;
	uint32 ii = est->i_pwr;
	uint32 qq = est->q_pwr;

	int16  iq_nbits, qq_nbits, ii_nbits;
	int32  tmp;
	int32  den, num;
	int32  angle;
	cint32 val;

	iq_nbits = wlc_phy_nbits(iq);
	qq_nbits = wlc_phy_nbits(qq);
	ii_nbits = wlc_phy_nbits(ii);
	if (ii_nbits > qq_nbits)
		qq_nbits = ii_nbits;

	if (30 >=  qq_nbits) {
		tmp = ii;
		tmp = tmp << (30 - qq_nbits);
		den = (int32) wlc_phy_sqrt_int((uint32) tmp);
		tmp = qq;
		tmp = tmp << (30 - qq_nbits);
		den *= (int32) wlc_phy_sqrt_int((uint32) tmp);
	} else {
		tmp = ii;
		tmp = tmp >> (qq_nbits - 30);
		den = (int32) wlc_phy_sqrt_int((uint32) tmp);
		tmp = qq;
		tmp = tmp >> (qq_nbits - 30);
		den *= (int32) wlc_phy_sqrt_int((uint32) tmp);
	}
	if (qq_nbits <= iq_nbits + 16) {
		den = den >> (16 + iq_nbits - qq_nbits);
	} else {
		den = den << (qq_nbits - (16 + iq_nbits));
	}

	tmp = -iq;
	num = (tmp << (30 - iq_nbits));
	if (num > 0)
		num += (den >> 1);
	else
		num -= (den >> 1);

	if (den == 0) {
		tmp = 0;
	} else {
		tmp = num / den; /* in X,16 */
	}

	mismatch->sin_angle = tmp;

	tmp = (tmp >> 1);
	tmp *= tmp;
	tmp = (1 << 30) - tmp;
	val.i = (int32) wlc_phy_sqrt_int((uint32) tmp);
	val.i = ( val.i << 1) ;

	val.q = mismatch->sin_angle;
	wlc_phy_inv_cordic(val, &angle);
	mismatch->angle = angle; /* in X,16 */


	iq_nbits = wlc_phy_nbits(qq - ii);
	if (iq_nbits % 2 == 1)
		iq_nbits++;

	den = ii;

	num = qq - ii;
	num = num << (30 - iq_nbits);
	if (iq_nbits > 10)
		den = den >> (iq_nbits - 10);
	else
		den = den << (10 - iq_nbits);
	if (num > 0)
		num += (den >> 1);
	else
		num -= (den >> 1);

	if (den == 0) {
		mismatch->mag = (1 << 10); /* in X,10 */
	} else {
		tmp = num / den + (1 << 20);
		mismatch->mag = (int32) wlc_phy_sqrt_int((uint32) tmp); /* in X,10 */
	}

#if defined(BCMDBG_RXCAL)
	printf("      Mag=%d, Angle=%d, cos(angle)=%d, sin(angle)=%d\n",
	(int)mismatch->mag, (int)mismatch->angle, (int)val.i, (int)val.q);
#endif /* BCMDBG_RXCAL */

}


static int
wlc_phy_cal_rx_fdiqi_acphy(phy_info_t *pi)
{

	acphy_rx_fdiqi_t freq_ang_mag[ACPHY_RXCAL_MAX_NUM_FREQ];
	int8 fdiqi_cal_freqs[ACPHY_RXCAL_MAX_NUM_FREQ];
	uint16 num_data;

	uint8 core;

	phy_iq_est_t loopback_rx_iq[PHY_CORE_MAX];
	phy_iq_est_t leakage_rx_iq[PHY_CORE_MAX];
	int32 angle;
	int32 mag;
	uint8 freq_idx;
	int16 tone_freq;
	uint16 radio_rev_id;

	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	radio_rev_id = read_radio_reg(pi, RF_2069_REV_ID(0));

	/* Enable leakage compensation by default */
	/* Disable leakage compensation for selected revisions only */
	/* LNA1 bypass mode */
	pi_ac->fdiqi.leakage_comp_mode = ACPHY_RXCAL_LEAKAGE_COMP;

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		pi_ac->fdiqi.leakage_comp_mode = ACPHY_RXCAL_NORMAL;
	}
	if (radio_rev_id == 4) {
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			pi_ac->fdiqi.leakage_comp_mode = ACPHY_RXCAL_NORMAL;
		}
	}

	wlc_phy_stay_in_carriersearch_acphy(pi, TRUE);
	wlc_phy_force_rfseq_acphy(pi, ACPHY_RFSEQ_RESET2RX);

	/* Zero Out coefficients */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		wlc_phy_rx_iq_comp_acphy(pi, 2, NULL, core);
	}
	wlc_phy_rx_fdiqi_comp_acphy(pi, FALSE);

	wlc_phy_rxcal_phy_setup_acphy(pi);
	wlc_phy_rxcal_radio_setup_acphy(pi);
	wlc_phy_rxcal_loopback_gainctrl_acphy(pi);

	wlc_phy_rx_fdiqi_freq_config(pi, fdiqi_cal_freqs, &num_data);

	for (freq_idx = 0; freq_idx < num_data; freq_idx++) {

		tone_freq = (int16)fdiqi_cal_freqs[freq_idx] * 1000;
		tone_freq = tone_freq >> 1;

		freq_ang_mag[freq_idx].freq = (int32)fdiqi_cal_freqs[freq_idx];
		wlc_phy_tx_tone_acphy(pi, (int32)tone_freq, ACPHY_RXCAL_TONEAMP, 0, 0, FALSE);

		/* get iq, ii, qq measurements from iq_est */
		if (pi->u.pi_acphy->fdiqi.enabled) {
			wlc_phy_rx_iq_est_acphy(pi, loopback_rx_iq, 0x3000, 32, 0);
		} else {
			wlc_phy_rx_iq_est_acphy(pi, loopback_rx_iq, 0x4000, 32, 0);
		}

		if (pi_ac->fdiqi.leakage_comp_mode == ACPHY_RXCAL_LEAKAGE_COMP) {
			wlc_phy_rxcal_radio_cleanup_acphy(pi);
			wlc_phy_rx_iq_est_acphy(pi, leakage_rx_iq, 0x4000, 32, 0);
			wlc_phy_rxcal_radio_setup_acphy(pi);
		}
		wlc_phy_stopplayback_acphy(pi);

#if defined(BCMDBG_RXCAL)
		printf("   %d ", freq_ang_mag[freq_idx].freq);
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			printf(" %d %d %d ", loopback_rx_iq[core].iq_prod,
				loopback_rx_iq[core].i_pwr, loopback_rx_iq[core].q_pwr);
		}
		if (pi_ac->fdiqi.leakage_comp_mode == ACPHY_RXCAL_LEAKAGE_COMP) {
			FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
				printf(" %d %d %d ", leakage_rx_iq[core].iq_prod,
					leakage_rx_iq[core].i_pwr, leakage_rx_iq[core].q_pwr);
			}
		}
		printf("\n");
#endif /* BCMDBG_RXCAL */
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			wlc_phy_rxcal_leakage_comp_acphy(pi, loopback_rx_iq[core],
				leakage_rx_iq[core], &angle, &mag);
			freq_ang_mag[ freq_idx ].angle[core] = angle;
			freq_ang_mag[ freq_idx ].mag[core] = mag;
		}
	}

	wlc_phy_rx_fdiqi_lin_reg_acphy(pi, freq_ang_mag, num_data);

	wlc_phy_rxcal_radio_cleanup_acphy(pi);
	wlc_phy_rxcal_phy_cleanup_acphy(pi);

#if defined(BCMDBG_RXCAL)
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* Measure the SNR in the Rx IQ cal feedback path */
		wlc_phy_rxcal_snr_acphy(pi, 0x4000, (1 << core));

		PHY_CAL(("wlc_phy_cal_rx_iq_acphy: core%d => "
		    "(S =%9d,  N =%9d,  K =%d)\n",
		    core,
		    pi_ac->rxcal_signal[core].i_pwr + pi_ac->rxcal_signal[core].q_pwr,
		    pi_ac->rxcal_noise[core].i_pwr + pi_ac->rxcal_noise[core].q_pwr,
		    0x4000));
	}
#endif /* BCMDBG_RXCAL */

	wlc_phy_stay_in_carriersearch_acphy(pi, FALSE);

	return BCME_OK;
}


static void
wlc_phy_rx_fdiqi_freq_config(phy_info_t *pi, int8 *fdiqi_cal_freqs, uint16 *num_data)
{

	fdiqi_cal_freqs[0] = (int8)((CHSPEC_IS80(pi->radio_chanspec) ? ACPHY_IQCAL_TONEFREQ_80MHz :
		CHSPEC_IS40(pi->radio_chanspec) ? ACPHY_IQCAL_TONEFREQ_40MHz :
		ACPHY_IQCAL_TONEFREQ_20MHz)/1000);
	fdiqi_cal_freqs[1] = - fdiqi_cal_freqs[0];
	*num_data = 2;

	/* rx_fdiqi is enabled in 80MHz channel by default unless it's forced OFF */
	pi->u.pi_acphy->fdiqi.enabled = (CHSPEC_IS80(pi->radio_chanspec));
#if defined(BCMDBG)
	if (pi->u.pi_acphy->fdiqi.forced) {
		switch (pi->u.pi_acphy->fdiqi.forced_val) {
		case 0:
			pi->u.pi_acphy->fdiqi.enabled = FALSE;
			break;
		case 1:
			pi->u.pi_acphy->fdiqi.enabled = (CHSPEC_IS80(pi->radio_chanspec));
			break;
		case 2:
			pi->u.pi_acphy->fdiqi.enabled = TRUE;
			break;
		}
	} else {
		pi->u.pi_acphy->fdiqi.enabled = (CHSPEC_IS80(pi->radio_chanspec));
	}
#endif /* BCMDBG */

	if (pi->u.pi_acphy->fdiqi.enabled) {
		if (CHSPEC_IS80(pi->radio_chanspec)) {
			fdiqi_cal_freqs[2] = 24;
			fdiqi_cal_freqs[3] = - fdiqi_cal_freqs[2];
			fdiqi_cal_freqs[4] = 32;
			fdiqi_cal_freqs[5] = - fdiqi_cal_freqs[4];
			*num_data = 6;
		} else if (CHSPEC_IS40(pi->radio_chanspec)) {
			fdiqi_cal_freqs[2] = 15;
			fdiqi_cal_freqs[3] = - fdiqi_cal_freqs[2];
			*num_data = 4;
		} else {
			fdiqi_cal_freqs[2] = 7;
			fdiqi_cal_freqs[3] = - fdiqi_cal_freqs[2];
			*num_data = 4;
		}
	}

}

#if defined(BCMDBG)
void wlc_phy_force_gainlevel_acphy(phy_info_t *pi, int16 int_val)
{
	uint8 core;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {

		/* disable clip2 */
		MOD_PHYREGC(pi, computeGainInfo, core, disableClip2detect, 1);
		phy_reg_write(pi, ACPHYREGC(Clip2Threshold, core), 0xffff);

	printf("wlc_phy_force_gainlevel_acphy (%d) : ", int_val);
	switch (int_val) {
	case 0:
		printf("initgain -- adc never clips.\n");
		if (ACREV_IS(pi->pubpi.phy_rev, 0)) {
			phy_reg_write(pi, ACPHYREGC(Clip1Threshold, core), 0xffff);
		} else {
			MOD_PHYREGC(pi, computeGainInfo, core, disableClip1detect, 1);
		}
		break;
	case 1:
		printf("clip hi -- adc always clips, nb never clips.\n");
		MOD_PHYREGC(pi, FastAgcClipCntTh, core, fastAgcNbClipCntTh, 0xff);
		break;
	case 2:
		printf("clip md -- adc/nb always clips, w1 never clips.\n");
		MOD_PHYREGC(pi, FastAgcClipCntTh, core, fastAgcNbClipCntTh, 0);
		MOD_PHYREGC(pi, FastAgcClipCntTh, core, fastAgcW1ClipCntTh, 0xff);
		break;
	case 3:
		printf("clip lo -- adc/nb/w1 always clips.\n");
		MOD_PHYREGC(pi, FastAgcClipCntTh, core, fastAgcNbClipCntTh, 0);
		MOD_PHYREGC(pi, FastAgcClipCntTh, core, fastAgcW1ClipCntTh, 0);
		break;
	case 4:
		printf("adc clip.\n");
		phy_reg_write(pi, ACPHYREGC(clipHiGainCodeA, core), 0x0);
		phy_reg_write(pi, ACPHYREGC(clipHiGainCodeB, core), 0x8);
		MOD_PHYREGC(pi, FastAgcClipCntTh, core, fastAgcNbClipCntTh, 0xff);
		break;
	case 5:
		printf("nb clip.\n");
		phy_reg_write(pi, ACPHYREGC(clipmdGainCodeA, core), 0xfffe);
		phy_reg_write(pi, ACPHYREGC(clipmdGainCodeB, core), 0x554);
		MOD_PHYREGC(pi, FastAgcClipCntTh, core, fastAgcW1ClipCntTh, 0xff);
		break;
	case 6:
		printf("w1 clip.\n");
		phy_reg_write(pi, ACPHYREGC(cliploGainCodeA, core), 0xfffe);
		phy_reg_write(pi, ACPHYREGC(cliploGainCodeB, core), 0x554);
		MOD_PHYREGC(pi, RssiClipMuxSel, core, fastAgcNbClipMuxSel, 0);
		MOD_RADIO_REGFLDC(pi, RF_2069_NBRSSI_CONFG(core), NBRSSI_CONFG,
			nbrssi_Refctrl_low, 1);
		break;
	}
	}

	ACPHY_ENABLE_STALL(pi, stall_val);
	printf("wlc_phy_force_gainlevel_acphy (%d)\n", int_val);
}
#endif 


#if defined(BCMDBG)
void wlc_phy_force_fdiqi_acphy(phy_info_t *pi, uint16 int_val)
{

	pi->u.pi_acphy->fdiqi.forced = TRUE;
	pi->u.pi_acphy->fdiqi.forced_val = int_val;
	wlc_phy_cals_acphy(pi, 0);

}
#endif


void
wlc_phy_cals_acphy(phy_info_t *pi, uint8 searchmode)
{

	uint8 tx_pwr_ctrl_state;
	uint8 phase_id = pi->cal_info->cal_phase_id;
	acphy_cal_result_t *accal = &pi->cal_info->u.accal;
	uint16 tbl_cookie = TXCAL_CACHE_VALID;
	PHY_CAL(("wl%d: Running ACPHY periodic calibration: Searchmode: %d \n",
	         pi->sh->unit, searchmode));

	/* -----------------
	 *  Initializations
	 * -----------------
	 */


	/* Exit immediately if we are running on Quickturn */
	if (ISSIM_ENAB(pi->sh->sih)) {
		wlc_phy_cal_perical_mphase_reset(pi);
		return;
	}

	/* skip cal if phy is muted */
	if (PHY_MUTED(pi)) {
		return;
	}


	/*
	 * Search-Mode Sanity Check for Tx-iqlo-Cal
	 *
	 * Notes: - "RESTART" means: start with 0-coeffs and use large search radius
	 *        - "REFINE"  means: start with latest coeffs and only search
	 *                    around that (faster)
	 *        - here, if channel has changed or no previous valid coefficients
	 *          are available, enforce RESTART search mode (this shouldn't happen
	 *          unless cal driver code is work-in-progress, so this is merely a safety net)
	 */
	if ((pi->radio_chanspec != accal->chanspec) ||
	    (accal->txiqlocal_coeffsvalid == 0)) {
		searchmode = PHY_CAL_SEARCHMODE_RESTART;
	}

	/*
	 * If previous phase of multiphase cal was on different channel,
	 * then restart multiphase cal on current channel (again, safety net)
	 */
	if ((phase_id > MPHASE_CAL_STATE_INIT)) {
		if (accal->chanspec != pi->radio_chanspec) {
			wlc_phy_cal_perical_mphase_restart(pi);
		}
	}


	/* Make the ucode send a CTS-to-self packet with duration set to 10ms. This
	 *  prevents packets from other STAs/AP from interfering with Rx IQcal
	 */

	/* Disable Power control */
	tx_pwr_ctrl_state = pi->txpwrctrl;
	wlc_phy_txpwrctrl_enable_acphy(pi, PHY_TPC_HW_OFF);

	/* If single phase cal send out CTS to self to ensure assoc/join */
	if (phase_id == MPHASE_CAL_STATE_IDLE)
		wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 29000);

	/* Prepare Mac and Phregs */
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	/* -------------------
	 *  Calibration Calls
	 * -------------------
	 */

	PHY_NONE(("wlc_phy_cals_acphy: Time=%d, LastTi=%d, SrchMd=%d, PhIdx=%d,"
		" Chan=%d, LastCh=%d, First=%d, vld=%d\n",
		pi->sh->now, pi->cal_info->last_cal_time, searchmode, phase_id,
		pi->radio_chanspec, accal->chanspec,
		pi->first_cal_after_assoc, accal->txiqlocal_coeffsvalid));

	if (phase_id == MPHASE_CAL_STATE_IDLE) {

		/*
		 * SINGLE-SHOT Calibrations
		 *
		 *    Call all Cals one after another
		 *
		 *    Notes:
		 *    - if this proc is called with the phase state in IDLE,
		 *      we know that this proc was called directly rather
		 *      than via the mphase scheduler (the latter puts us into
		 *      INIT state); under those circumstances, perform immediate
		 *      execution over all cal tasks
		 *    - for better code structure, we would use the below mphase code for
		 *      sphase case, too, by utilizing an appropriate outer for-loop
		 */

		/* TO-DO: Ensure that all inits and cleanups happen here */


		/* carry out all phases "en bloc", for comments see the various phases below */
		pi->cal_info->last_cal_time     = pi->sh->now;
		accal->chanspec = pi->radio_chanspec;
		if (pi->first_cal_after_assoc) {
		  wlc_phy_txpwrctrl_idle_tssi_meas_acphy(pi);
		}
		wlc_phy_precal_txgain_acphy(pi, accal->txcal_txgain);
		wlc_phy_cal_txiqlo_acphy(pi, searchmode, FALSE); /* request "Sphase" */
		wlc_phy_cal_rx_fdiqi_acphy(pi);
		if (pi->u.pi_acphy->crsmincal_enable) {
			PHY_CAL(("%s : crsminpwr cal\n", __FUNCTION__));
			wlc_phy_crs_min_pwr_cal_acphy(pi);
		}
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IQLOCAL, 1,
		                          IQTBL_CACHE_COOKIE_OFFSET, 16, &tbl_cookie);
		pi->first_cal_after_assoc = FALSE;

	} else {

		/*
		 * MULTI-PHASE CAL
		 *
		 *   Carry out next step in multi-phase execution of cal tasks
		 *
		 */

		switch (phase_id) {
		case ACPHY_CAL_PHASE_INIT:

			/*
			 *   Housekeeping & Pre-Txcal Tx Gain Adjustment
			 */

		  wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 60);
			/* remember time and channel of this cal event */
			pi->cal_info->last_cal_time     = pi->sh->now;
			accal->chanspec = pi->radio_chanspec;

			wlc_phy_precal_txgain_acphy(pi, accal->txcal_txgain);

			/* move on */
			pi->cal_info->cal_phase_id++;
			break;

		case ACPHY_CAL_PHASE_TX0:
		case ACPHY_CAL_PHASE_TX1:
		case ACPHY_CAL_PHASE_TX2:
		case ACPHY_CAL_PHASE_TX3:
		case ACPHY_CAL_PHASE_TX4:
		case ACPHY_CAL_PHASE_TX5:
		case ACPHY_CAL_PHASE_TX6:
		case ACPHY_CAL_PHASE_TX7:
		case ACPHY_CAL_PHASE_TX8:
		case ACPHY_CAL_PHASE_TX9:
		case ACPHY_CAL_PHASE_TX_LAST:

			/*
			 *   Tx-IQLO-Cal
			 */
		  if (!ACREV_IS(pi->pubpi.phy_rev, 1) && (phase_id > ACPHY_CAL_PHASE_TX8)) {
		    pi->cal_info->cal_phase_id++;
		    break;
		  }
		  wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 4400);
			/* to ensure radar detect is skipped during cals */
			if ((pi->radar_percal_mask & 0x10) != 0) {
				pi->u.pi_acphy->radar_cal_active = TRUE;
			}

			if (wlc_phy_cal_txiqlo_acphy(pi, searchmode, TRUE) != BCME_OK) {
				/* rare case, just reset */
				PHY_ERROR(("wlc_phy_cal_txiqlo_acphy failed\n"));
			    wlc_phy_cal_perical_mphase_reset(pi);
				break;
			}

			/* move on */
			pi->cal_info->cal_phase_id++;
			break;

		case ACPHY_CAL_PHASE_PAPDCAL:


			/* move on */
			pi->cal_info->cal_phase_id++;
			break;

		case ACPHY_CAL_PHASE_RXCAL:
			/*
			 *   Rx IQ Cal
			 */

		  wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 9500);
			if ((pi->radar_percal_mask & 0x1) != 0) {
				pi->u.pi_acphy->radar_cal_active = TRUE;
			}

			wlc_phy_cal_rx_fdiqi_acphy(pi);
			if (pi->u.pi_acphy->crsmincal_enable) {
				PHY_CAL(("%s : crsminpwr cal\n", __FUNCTION__));
				wlc_phy_crs_min_pwr_cal_acphy(pi);
			}

			pi->u.pi_acphy->txcal_cache_cookie = 0;

			/* cache cals for restore on return to home channel */
			wlc_phy_scanroam_cache_cal_acphy(pi, 1);

			/* move on */
			pi->cal_info->cal_phase_id++;
			break;

		case ACPHY_CAL_PHASE_RSSICAL:

			/*
			 *     RSSI Cal & VCO Cal
			 */

		  wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 300);
			if ((pi->radar_percal_mask & 0x4) != 0) {
			    pi->u.pi_acphy->radar_cal_active = TRUE;
			}

			/* RSSI & VCO cal (prevents VCO/PLL from losing lock with temp delta) */

			wlc_phy_radio2069_vcocal(pi);
			pi->cal_info->last_cal_time = pi->sh->now;
			accal->chanspec = pi->radio_chanspec;

			/* If this is the first calibration after association then we
			 * still have to do calibrate the idle-tssi, otherrwise done
			 */
			if (pi->first_cal_after_assoc) {
				pi->cal_info->cal_phase_id++;
			} else {
			  wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IQLOCAL, 1,
			    IQTBL_CACHE_COOKIE_OFFSET, 16, &tbl_cookie);

		wlc_phy_cal_perical_mphase_reset(pi);
			}
			break;

		case ACPHY_CAL_PHASE_IDLETSSI:

			/*
			 *     Idle TSSI & TSSI-to-dBm Mapping Setup
			 */


		  wlapi_bmac_write_shm(pi->sh->physhim, M_CTS_DURATION, 1550);
			if ((pi->radar_percal_mask & 0x8) != 0)
				pi->u.pi_acphy->radar_cal_active = TRUE;

			/* Idle TSSI determination once right after join/up/assoc */
			wlc_phy_txpwrctrl_idle_tssi_meas_acphy(pi);

			/* done with multi-phase cal, reset phase */
			pi->first_cal_after_assoc = FALSE;
			wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_IQLOCAL, 1,
			  IQTBL_CACHE_COOKIE_OFFSET, 16, &tbl_cookie);

		wlc_phy_cal_perical_mphase_reset(pi);
			break;

		default:
			PHY_ERROR(("wlc_phy_cals_phy: Invalid calibration phase %d\n", phase_id));
			ASSERT(0);
			wlc_phy_cal_perical_mphase_reset(pi);
			break;
		}
	}

	/* ----------
	 *  Cleanups
	 * ----------
	 */
	wlc_phy_txpwrctrl_enable_acphy(pi, tx_pwr_ctrl_state);

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);

}

static void
wlc_phy_pulse_adc_reset_acphy(phy_info_t *pi)
{
	uint8 core;
	uint16 curr_cfg1_val[PHY_CORE_MAX], curr_cfg2_val[PHY_CORE_MAX];
	uint16 curr_ovr_val[PHY_CORE_MAX];

	/* Set clamp using rfctrl override */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		curr_cfg1_val[core] = phy_reg_read(pi, ACPHYREGCE(RfctrlCoreAfeCfg1, core));
		curr_cfg2_val[core] = phy_reg_read(pi, ACPHYREGCE(RfctrlCoreAfeCfg2, core));
		curr_ovr_val[core]  = phy_reg_read(pi, ACPHYREGCE(RfctrlOverrideAfeCfg, core));

		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg1, core), curr_cfg1_val[core] |
		              ACPHY_RfctrlCoreAfeCfg10_afe_iqadc_reset_MASK);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg2, core), curr_cfg2_val[core] |
		              ACPHY_RfctrlCoreAfeCfg20_afe_iqadc_clamp_en_MASK);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAfeCfg, core), curr_ovr_val[core] |
		              ACPHY_RfctrlOverrideAfeCfg0_afe_iqadc_clamp_en_MASK |
		              ACPHY_RfctrlOverrideAfeCfg0_afe_iqadc_reset_MASK);
	}

	/* Wait for 1 us */
	OSL_DELAY(1);

	/* Restore values */
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAfeCfg, core), curr_ovr_val[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg1, core), curr_cfg1_val[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg2, core), curr_cfg2_val[core]);
	}

	/* Wait for 1 us */
	OSL_DELAY(1);
}

#ifdef PPR_API
void
wlc_phy_txpower_sromlimit_get_acphy(phy_info_t *pi, chanspec_t chanspec,
                                        ppr_t *max_pwr, uint8 core)
{
	srom11_pwrdet_t	*pwrdet  = &pi->pwrdet_ac;
	uint8 band, band_srom;
	uint8 tmp_max_pwr = 0;
	int8 deltaPwr;
	uint8 chan;

	chan = CHSPEC_CHANNEL(chanspec);

	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		PHY_ERROR(("wl%d: %s: FIXME\n", pi->sh->unit, __FUNCTION__));
	} else
		ASSERT(pi->sh->subband5Gver == PHY_SUBBAND_4BAND);

	/* to figure out which subband is in 5G */
	/* in the range of 0, 1, 2, 3, 4 */
	band = wlc_phy_get_chan_freq_range_acphy(pi, chan);

	tmp_max_pwr =
		MIN(pwrdet->max_pwr[0][band], pwrdet->max_pwr[1][band]);
	tmp_max_pwr =
		MIN(tmp_max_pwr, pwrdet->max_pwr[2][band]);
/*	--------  in 5g_ext case  -----------
 *	if 5170 <= freq < 5250, then band = 1;
 *	if 5250 <= freq < 5500, then band = 2;
 *	if 5500 <= freq < 5745, then band = 3;
 *	if 5745 <= freq,		then band = 4;

 *	--------  in 5g case  ---------------
 *	if 5170 <= freq < 5500, then band = 1;
 *	if 5500 <= freq < 5745, then band = 2;
 *	if 5745 <= freq,		then band = 3;
 */
/*  -------- 4 subband to 3 subband mapping --------
 *	subband#0 -> low
 *	subband#1 -> mid
 *	subband#2 -> high
 *	subband#3 -> high
 */
	if (band <= WL_CHAN_FREQ_RANGE_5G_BAND2)
		band_srom = band;
	else
		band_srom = band - 1;

	wlc_phy_txpwr_apply_srom11(pi, band_srom, chanspec, tmp_max_pwr, max_pwr);

	deltaPwr = pwrdet->max_pwr[core][band] - tmp_max_pwr;
	if (deltaPwr > 0)
		ppr_plus_cmn_val(max_pwr, deltaPwr);

	switch (band) {
	case WL_CHAN_FREQ_RANGE_2G:
	case WL_CHAN_FREQ_RANGE_5G_BAND0:
	case WL_CHAN_FREQ_RANGE_5G_BAND1:
	case WL_CHAN_FREQ_RANGE_5G_BAND2:
	case WL_CHAN_FREQ_RANGE_5G_BAND3:
		ppr_apply_max(max_pwr, pwrdet->max_pwr[core][band]);
		break;
	}

	if (0) {
		PHY_ERROR(("####band #%d######\n", band));
		ppr_dsss_printf(max_pwr);
		ppr_ofdm_printf(max_pwr);
		ppr_mcs_printf(max_pwr);
	}
}

#else

void
wlc_phy_txpower_sromlimit_get_acphy(phy_info_t *pi, uint chan, uint8 *max_pwr, uint8 txp_rate_idx)
{
	/* Incomplete code. Determine max power limit from SROM */
	*max_pwr = 15 * 4;
}
#endif	/* PPR_API */

#if defined(BCMDBG)
/* dump calibration regs/info */
void
wlc_phy_cal_dump_acphy(phy_info_t *pi, struct bcmstrbuf *b)
{

	uint8 core;
	int8  ac_reg, mf_reg, off1 = 0, off2 = 0;
	int16  a_reg, b_reg, a_int, b_int;
	uint16 ab_int[2], d_reg, eir, eqr, fir, fqr;
	int32 slope;

	if (!pi->sh->up) {
		return;
	}

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	bcm_bprintf(b, "Tx-IQ/LOFT-Cal:\n");
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ, ab_int,
			TB_OFDM_COEFFS_AB, core);
		wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ, &d_reg,
			TB_OFDM_COEFFS_D, core);
		eir = read_radio_reg(pi, RF_2069_TXGM_LOFT_FINE_I(core));
		eqr = read_radio_reg(pi, RF_2069_TXGM_LOFT_FINE_Q(core));
		fir = read_radio_reg(pi, RF_2069_TXGM_LOFT_COARSE_I(core));
		fqr = read_radio_reg(pi, RF_2069_TXGM_LOFT_COARSE_Q(core));
		bcm_bprintf(b, "   core-%d: a/b: (%4d,%4d), d: (%3d,%3d),"
			" e: (%3d,%3d), f: (%3d,%3d)\n",
			core, (int16) ab_int[0], (int16) ab_int[1],
			(int8)((d_reg & 0xFF00) >> 8), /* di */
			(int8)((d_reg & 0x00FF)),      /* dq */
			(int8)(-((eir & 0xF0) >> 4) + ((eir & 0xF))), /* ei */
			(int8)(-((eqr & 0xF0) >> 4) + ((eqr & 0xF))), /* eq */
			(int8)(-((fir & 0xF0) >> 4) + ((fir & 0xF))), /* fi */
			(int8)(-((fqr & 0xF0) >> 4) + ((fqr & 0xF))));  /* fq */
	}
	bcm_bprintf(b, "Rx-IQ-Cal:\n");
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		a_reg = (int16) phy_reg_read(pi, ACPHY_RxIQCompA(core));
		b_reg = (int16) phy_reg_read(pi, ACPHY_RxIQCompB(core));
		a_int = (a_reg >= 512) ? a_reg - 1024 : a_reg; /* s0.9 format */
		b_int = (b_reg >= 512) ? b_reg - 1024 : b_reg;
		if (pi->u.pi_acphy->fdiqi.enabled) {
			slope = pi->u.pi_acphy->fdiqi.slope[core];
			bcm_bprintf(b, "   core-%d: a/b = (%4d,%4d), S = %2d (%1d)\n",
				core, a_int, b_int, slope,
				READ_PHYREG(pi, ACPHY_rxfdiqImbCompCtrl, rxfdiqImbCompEnable));
		} else {
			bcm_bprintf(b, "   core-%d: a/b = (%4d,%4d), S = OFF (%1d)\n",
				core, a_int, b_int,
				READ_PHYREG(pi, ACPHY_rxfdiqImbCompCtrl, rxfdiqImbCompEnable));
		}
	}

	ac_reg =  READ_PHYREG(pi, ACPHY_crsminpoweru0, crsminpower0);
	mf_reg =  READ_PHYREG(pi, ACPHY_crsmfminpoweru0, crsmfminpower0);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		if (core == 1) {
			off1 =    READ_PHYREG(pi, ACPHY_crsminpoweroffset1, crsminpowerOffsetu);
		} else if (core == 2) {
			off2 =    READ_PHYREG(pi, ACPHY_crsminpoweroffset2, crsminpowerOffsetu);
		}
	}

	bcm_bprintf(b, "crs_min_pwr cal:\n");
	if (pi->u.pi_acphy->crsmincal_run != 1) {
		bcm_bprintf(b, "   crs_min_pwr cal DID NOT run (ErrCode : %d)\n",
			pi->u.pi_acphy->crsmincal_run);
	}
	bcm_bprintf(b, "   Noise power used for setting crs_min thresholds : ");
	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		bcm_bprintf(b, "Core-%d : %d, ", core, pi->u.pi_acphy->phy_noise_in_crs_min[core]);
	}

	bcm_bprintf(b, "\n");
	bcm_bprintf(b, "   AC-CRS = %d,", ac_reg);
	bcm_bprintf(b, "   MF-CRS = %d,", mf_reg);
	bcm_bprintf(b, "   Offset 1 = %d,", off1);
	bcm_bprintf(b, "   Offset 2 = %d\n", off2);

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);

	return;
}
#endif	

static uint8
wlc_poll_adc_clamp_status(phy_info_t *pi, uint8 core, uint8 do_reset)
{
	uint8  ovr_status;

	ovr_status = READ_RADIO_REGFLDC(pi, RF_2069_ADC_STATUS(core), ADC_STATUS,
	                                i_wrf_jtag_afe_iqadc_overload);

	if (ovr_status && do_reset) {
		/* MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg, 0, afe_iqadc_reset_ov_det, 1); */
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_iqadc_reset_ov_det,  1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2, core, afe_iqadc_reset_ov_det, 0);
	}

	return ovr_status;
}


int BCMFASTPATH
wlc_phy_rssi_compute_acphy(phy_info_t *pi, wlc_d11rxhdr_t *wlc_rxh)
{
	d11rxhdr_t *rxh = &wlc_rxh->rxhdr;
	int16 rxpwr, num_activecores;
	int16 rxpwr_core[WL_RSSI_ANT_MAX], rxpwr_activecore[WL_RSSI_ANT_MAX];
	int16 is_status_hacked;
	int i;
	int16 gain_err_temp_adj_for_rssi;

	/* mode = 0: rxpwr = max(rxpwr0, rxpwr1)
	 * mode = 1: rxpwr = min(rxpwr0, rxpwr1)
	 * mode = 2: rxpwr = (rxpwr0+rxpwr1)/2
	 */
	rxpwr = 0;
	rxpwr_core[0] = ACPHY_RXPWR_ANT0(rxh);
	rxpwr_core[1] = ACPHY_RXPWR_ANT1(rxh);
	rxpwr_core[2] = ACPHY_RXPWR_ANT2(rxh);
	rxpwr_core[3] = 0;
	is_status_hacked = ACPHY_HACK_PWR_STATUS(rxh);


	if ((ACREV1X1_IS(pi->pubpi.phy_rev)) && (is_status_hacked == 1)) {
		rxpwr_core[0] = wlc_phy_11b_rssi_WAR(pi, rxh);
		rxpwr_core[1] = 0x80;
		rxpwr_core[2] = 0x80;
		rxpwr_core[3] = 0x00;
	}

	/* Sign extend */
	for (i = 0; i < WL_RSSI_ANT_MAX; i ++) {
		if (rxpwr_core[i] > 127)
			rxpwr_core[i] -= 256;
	}
	wlc_phy_upd_gain_wrt_temp_phy(pi, &gain_err_temp_adj_for_rssi);

	/* Apply gain-error correction with temperature compensation: */
	for (i = 0; i < WL_RSSI_ANT_MAX; i ++) {
		int16 tmp;
	        tmp = pi->phy_rssi_gain_error[i]*2 - gain_err_temp_adj_for_rssi;
	        tmp = ((tmp >= 0) ? ((tmp + 2) >> 2) : -1*((-1*tmp + 2) >> 2));
	        rxpwr_core[i] -= tmp;
	}

	/* only 3 antennas are valid for now */
	for (i = 0; i < WL_RSSI_ANT_MAX; i ++) {
	  rxpwr_core[i] = MAX(-128, rxpwr_core[i]);
		wlc_rxh->rxpwr[i] = (int8)rxpwr_core[i];
	}
	wlc_rxh->do_rssi_ma = 0;

	/* legacy interface */
	num_activecores = 0;
	for (i = 0; i < WL_RSSI_ANT_MAX; i++) {
		if (((pi->sh->phyrxchain >> i) & 1) == 1) {
			rxpwr_activecore[num_activecores] = rxpwr_core[i];
			num_activecores ++;
		}
	}
	if (num_activecores == 0) {
		ASSERT(0);
	}
	rxpwr = rxpwr_activecore[0];
	for (i = 1; i < num_activecores; i ++) {
		if (pi->sh->rssi_mode == RSSI_ANT_MERGE_MAX) {
			rxpwr = MAX(rxpwr, rxpwr_activecore[i]);
		} else if (pi->sh->rssi_mode == RSSI_ANT_MERGE_MIN) {
			rxpwr = MIN(rxpwr, rxpwr_activecore[i]);
		} else if (pi->sh->rssi_mode == RSSI_ANT_MERGE_AVG) {
			rxpwr += rxpwr_activecore[i];
		}
		else
			ASSERT(0);
	}


	if (pi->sh->rssi_mode == RSSI_ANT_MERGE_AVG) {
		int16 qrxpwr;
		rxpwr = (int8)qm_div16(rxpwr, num_activecores, &qrxpwr);
	}
	return rxpwr;
}

uint8
wlc_phy_11b_rssi_WAR(phy_info_t *pi, d11rxhdr_t *rxh)
{
	int16 PhyStatsGainInfo0, Auxphystats0;
	int8 lna1, lna2, mixer, biq0, biq1, trpos, dvga;
	int8 elna;
	int8 trloss;
	int8 elna_byp_tr;
	int8 lna1_gain, lna2_gain, rxmix_gain,
		biq0_gain, biq1_gain, dvga_gain, fem_gain, total_rx_gain;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	PhyStatsGainInfo0 = ((ACPHY_RXPWR_ANT2(rxh) << 8) | (ACPHY_RXPWR_ANT1(rxh)));
	Auxphystats0 = ((ACPHY_RXPWR_ANT0(rxh) << 8) | (ACPHY_RXPWR_ANT4(rxh)));

	/* Parsing the gaininfo */
	lna1 = (PhyStatsGainInfo0 >> 0) & 0x7;
	lna2 = (PhyStatsGainInfo0 >> 3) & 0x7;
	mixer = (PhyStatsGainInfo0 >> 6) & 0xf;
	biq0 = (PhyStatsGainInfo0 >> 10) & 0x7;
	biq1 = (PhyStatsGainInfo0 >> 13) & 0x7;

	trpos = (Auxphystats0 >> 0) & 0x1;
	dvga = (Auxphystats0 >> 2) & 0xf;

	elna = pi_ac->fem_rxgains[0].elna;
	trloss = pi_ac->fem_rxgains[0].trloss;
	elna_byp_tr = pi_ac->fem_rxgains[0].elna_bypass_tr;

	/* get gains of each block */
	dvga_gain  = 3*dvga;
	lna1_gain  = pi_ac->rxgainctrl_params[0].gaintbl[1][lna1];
	lna2_gain  = pi_ac->rxgainctrl_params[0].gaintbl[2][lna2];
	rxmix_gain = pi_ac->rxgainctrl_params[0].gaintbl[3][mixer];
	biq0_gain = pi_ac->rxgainctrl_params[0].gaintbl[4][biq0];
	biq1_gain = pi_ac->rxgainctrl_params[0].gaintbl[5][biq1];

	/* Get fem gain */
	if (elna_byp_tr == 1) {
		if (trpos == 0) {
			fem_gain = elna;
		} else {
			fem_gain = elna - trloss;
		}
	} else {
		if (trpos == 0) {
			fem_gain = 0;
		} else {
			fem_gain = (-1*trloss);
		}
	}

	/* Total Rx gain */
	total_rx_gain = (lna1_gain + lna2_gain + rxmix_gain
		 + biq0_gain + biq1_gain + dvga_gain + fem_gain);

	return (-4 - total_rx_gain + 256);
}

static void
BCMATTACHFN(wlc_phy_srom_read_rxgainerr_acphy)(phy_info_t *pi)
{
	/* read and uncompress gain-error values for rx power reporting */

	int8 tmp[PHY_CORE_MAX];
	int16 tmp2;

	/* read in temperature at calibration time */
	tmp2 = (int16) (((int16)PHY_GETINTVAR(pi, "rawtempsense"))  << 7) >> 7;
	if (tmp2 == -1) {
		/* set to some bogus value, since nothing was written to SROM */
		pi->srom_rawtempsense = 255;
	} else {
		pi->srom_rawtempsense = tmp2;
	}
	pi->u.pi_acphy->current_temperature = pi->srom_rawtempsense;

	/* 2G: */
	/* read and sign-extend */
	tmp[0] = (int8)(((int8)PHY_GETINTVAR(pi, "rxgainerr2ga0")) << 2) >> 2;
	tmp[1] = (int8)(((int8)PHY_GETINTVAR(pi, "rxgainerr2ga1")) << 3) >> 3;
	tmp[2] = (int8)(((int8)PHY_GETINTVAR(pi, "rxgainerr2ga2")) << 3) >> 3;
	if ((tmp[0] == -1) && (tmp[1] == -1) && (tmp[2] == -1)) {
		/* If all srom values are -1, then possibly
		 * no gainerror info was written to srom
		 */
		tmp[0] = 0; tmp[1] = 0; tmp[2] = 0;
		pi->srom_rxgainerr2g_isempty = TRUE;
	} else {
		pi->srom_rxgainerr2g_isempty = FALSE;
	}
	/* gain errors for cores 1 and 2 are stored in srom as deltas relative to core 0: */
	pi->srom_rxgainerr_2g[0] = tmp[0];
	pi->srom_rxgainerr_2g[1] = tmp[0] + tmp[1];
	pi->srom_rxgainerr_2g[2] = tmp[0] + tmp[2];

	/* 5G low: */
	/* read and sign-extend */
	tmp[0] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga0", 0)) << 2) >> 2;
	tmp[1] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga1", 0)) << 3) >> 3;
	tmp[2] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga2", 0)) << 3) >> 3;
	if ((tmp[0] == -1) && (tmp[1] == -1) && (tmp[2] == -1)) {
		/* If all srom values are -1, then possibly
		 * no gainerror info was written to srom
		 */
		tmp[0] = 0; tmp[1] = 0; tmp[2] = 0;
		pi->srom_rxgainerr5gl_isempty = TRUE;
	} else {
		pi->srom_rxgainerr5gl_isempty = FALSE;
	}
	/* gain errors for cores 1 and 2 are stored in srom as deltas relative to core 0: */
	pi->srom_rxgainerr_5gl[0] = tmp[0];
	pi->srom_rxgainerr_5gl[1] = tmp[0] + tmp[1];
	pi->srom_rxgainerr_5gl[2] = tmp[0] + tmp[2];

	/* 5G mid: */
	/* read and sign-extend */
	tmp[0] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga0", 1)) << 2) >> 2;
	tmp[1] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga1", 1)) << 3) >> 3;
	tmp[2] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga2", 1)) << 3) >> 3;

	if ((tmp[0] == -1) && (tmp[1] == -1) && (tmp[2] == -1)) {
		/* If all srom values are -1, then possibly
		 * no gainerror info was written to srom
		 */
		tmp[0] = 0; tmp[1] = 0; tmp[2] = 0;
		pi->srom_rxgainerr5gm_isempty = TRUE;
	} else {
		pi->srom_rxgainerr5gm_isempty = FALSE;
	}
	/* gain errors for cores 1 and 2 are stored in srom as deltas relative to core 0: */
	pi->srom_rxgainerr_5gm[0] = tmp[0];
	pi->srom_rxgainerr_5gm[1] = tmp[0] + tmp[1];
	pi->srom_rxgainerr_5gm[2] = tmp[0] + tmp[2];

	/* 5G high: */
	/* read and sign-extend */
	tmp[0] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga0", 2)) << 2) >> 2;
	tmp[1] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga1", 2)) << 3) >> 3;
	tmp[2] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga2", 2)) << 3) >> 3;

	if ((tmp[0] == -1) && (tmp[1] == -1) && (tmp[2] == -1)) {
		/* If all srom values are -1, then possibly
		 * no gainerror info was written to srom
		 */
		tmp[0] = 0; tmp[1] = 0; tmp[2] = 0;
		pi->srom_rxgainerr5gh_isempty = TRUE;
	} else {
		pi->srom_rxgainerr5gh_isempty = FALSE;
	}
	/* gain errors for cores 1 and 2 are stored in srom as deltas relative to core 0: */
	pi->srom_rxgainerr_5gh[0] = tmp[0];
	pi->srom_rxgainerr_5gh[1] = tmp[0] + tmp[1];
	pi->srom_rxgainerr_5gh[2] = tmp[0] + tmp[2];

	/* 5G upper: */
	/* read and sign-extend */
	tmp[0] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga0", 3)) << 2) >> 2;
	tmp[1] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga1", 3)) << 3) >> 3;
	tmp[2] = (int8)(((int8)getintvararray(pi->vars, "rxgainerr5ga2", 3)) << 3) >> 3;

	if ((tmp[0] == -1) && (tmp[1] == -1) && (tmp[2] == -1)) {
		/* If all srom values are -1, then possibly
		 * no gainerror info was written to srom
		 */
		tmp[0] = 0; tmp[1] = 0; tmp[2] = 0;
		pi->srom_rxgainerr5gu_isempty = TRUE;
	} else {
		pi->srom_rxgainerr5gu_isempty = FALSE;
	}
	/* gain errors for cores 1 and 2 are stored in srom as deltas relative to core 0: */
	pi->srom_rxgainerr_5gu[0] = tmp[0];
	pi->srom_rxgainerr_5gu[1] = tmp[0] + tmp[1];
	pi->srom_rxgainerr_5gu[2] = tmp[0] + tmp[2];
}

#define ACPHY_SROM_NOISELVL_OFFSET (-70)

static void
BCMATTACHFN(wlc_phy_srom_read_noiselvl_acphy)(phy_info_t *pi)
{
	/* read noise levels from SROM */
	uint8 core;

	/* 2G: */
	FOREACH_CORE(pi, core) {
		pi->srom_noiselvl_2g[core] = ACPHY_SROM_NOISELVL_OFFSET;
	}
	pi->srom_noiselvl_2g[0] -= ((uint8)PHY_GETINTVAR(pi, "noiselvl2ga0"));
	pi->srom_noiselvl_2g[1] -= ((uint8)PHY_GETINTVAR(pi, "noiselvl2ga1"));
	pi->srom_noiselvl_2g[2] -= ((uint8)PHY_GETINTVAR(pi, "noiselvl2ga2"));

	/* 5G low: */
	FOREACH_CORE(pi, core) {
		pi->srom_noiselvl_5gl[core] = ACPHY_SROM_NOISELVL_OFFSET;
	}
	pi->srom_noiselvl_5gl[0] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga0", 0));
	pi->srom_noiselvl_5gl[1] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga1", 0));
	pi->srom_noiselvl_5gl[2] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga2", 0));

	/* 5G mid: */
	FOREACH_CORE(pi, core) {
		pi->srom_noiselvl_5gm[core] = ACPHY_SROM_NOISELVL_OFFSET;
	}
	pi->srom_noiselvl_5gm[0] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga0", 1));
	pi->srom_noiselvl_5gm[1] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga1", 1));
	pi->srom_noiselvl_5gm[2] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga2", 1));

	/* 5G high: */
	FOREACH_CORE(pi, core) {
		pi->srom_noiselvl_5gh[core] = ACPHY_SROM_NOISELVL_OFFSET;
	}
	pi->srom_noiselvl_5gh[0] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga0", 2));
	pi->srom_noiselvl_5gh[1] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga1", 2));
	pi->srom_noiselvl_5gh[2] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga2", 2));

	/* 5G upper: */
	FOREACH_CORE(pi, core) {
		pi->srom_noiselvl_5gu[core] = ACPHY_SROM_NOISELVL_OFFSET;
	}
	pi->srom_noiselvl_5gu[0] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga0", 3));
	pi->srom_noiselvl_5gu[1] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga1", 3));
	pi->srom_noiselvl_5gu[2] -= ((uint8)getintvararray(pi->vars, "rxgainerr5ga2", 3));
}

static bool
BCMATTACHFN(wlc_phy_srom_read_acphy)(phy_info_t *pi)
{
	/* Read rxgainctrl srom entries - elna gain, trloss */
	wlc_phy_srom_read_gainctrl_acphy(pi);

	if (!wlc_phy_txpwr_srom11_read(pi))
		return FALSE;

	wlc_phy_srom_read_rxgainerr_acphy(pi);

	wlc_phy_srom_read_noiselvl_acphy(pi);

	return TRUE;
}

void
wlc_phy_scanroam_cache_cal_acphy(phy_info_t *pi, bool set)
{
	uint16 ab_int[2];
	uint8 core;
	uint8 stall_val;

	/* Prepare Mac and Phregs */
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	PHY_TRACE(("wl%d: %s: in scan/roam set %d\n", pi->sh->unit, __FUNCTION__, set));

	if (set) {
		PHY_CAL(("wl%d: %s: save the txcal for scan/roam\n",
			pi->sh->unit, __FUNCTION__));
		/* save the txcal to cache */
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ,
				ab_int, TB_OFDM_COEFFS_AB, core);
			pi->u.pi_acphy->txcal_cache[core].txa = ab_int[0];
			pi->u.pi_acphy->txcal_cache[core].txb = ab_int[1];
			wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_READ,
			&pi->u.pi_acphy->txcal_cache[core].txd,
				TB_OFDM_COEFFS_D, core);
			pi->u.pi_acphy->txcal_cache[core].txei = (uint8)read_radio_reg(pi,
				RF_2069_TXGM_LOFT_FINE_I(core));
			pi->u.pi_acphy->txcal_cache[core].txeq = (uint8)read_radio_reg(pi,
				RF_2069_TXGM_LOFT_FINE_Q(core));
			pi->u.pi_acphy->txcal_cache[core].txfi = (uint8)read_radio_reg(pi,
				RF_2069_TXGM_LOFT_COARSE_I(core));
			pi->u.pi_acphy->txcal_cache[core].txfq = (uint8)read_radio_reg(pi,
				RF_2069_TXGM_LOFT_COARSE_Q(core));
			pi->u.pi_acphy->txcal_cache[core].rxa =
				phy_reg_read(pi, ACPHY_RxIQCompA(core));
			pi->u.pi_acphy->txcal_cache[core].rxb =
				phy_reg_read(pi, ACPHY_RxIQCompB(core));
			}

		/* mark the cache as valid */
		pi->u.pi_acphy->txcal_cache_cookie = TXCAL_CACHE_VALID;
	} else {
		if (pi->u.pi_acphy->txcal_cache_cookie == TXCAL_CACHE_VALID) {
			PHY_CAL(("wl%d: %s: restore the txcal after scan/roam\n",
				pi->sh->unit, __FUNCTION__));
			/* restore the txcal from cache */
			FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
				ab_int[0] = pi->u.pi_acphy->txcal_cache[core].txa;
				ab_int[1] = pi->u.pi_acphy->txcal_cache[core].txb;
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					ab_int, TB_OFDM_COEFFS_AB, core);
				wlc_phy_cal_txiqlo_coeffs_acphy(pi, CAL_COEFF_WRITE,
					&pi->u.pi_acphy->txcal_cache[core].txd,
					TB_OFDM_COEFFS_D, core);
				write_radio_reg(pi, RF_2069_TXGM_LOFT_FINE_I(core),
					pi->u.pi_acphy->txcal_cache[core].txei);
				write_radio_reg(pi, RF_2069_TXGM_LOFT_FINE_Q(core),
					pi->u.pi_acphy->txcal_cache[core].txeq);
				write_radio_reg(pi, RF_2069_TXGM_LOFT_COARSE_I(core),
					pi->u.pi_acphy->txcal_cache[core].txfi);
				write_radio_reg(pi, RF_2069_TXGM_LOFT_COARSE_Q(core),
					pi->u.pi_acphy->txcal_cache[core].txfq);
				phy_reg_write(pi, ACPHY_RxIQCompA(core),
					pi->u.pi_acphy->txcal_cache[core].rxa);
				phy_reg_write(pi, ACPHY_RxIQCompB(core),
					pi->u.pi_acphy->txcal_cache[core].rxb);
			}

		}
	}

	ACPHY_ENABLE_STALL(pi, stall_val);

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);
}

/* report estimated power and adjusted estimated power in quarter dBms */
void
wlc_phy_txpwr_est_pwr_acphy(phy_info_t *pi, uint8 *Pout, uint8 *Pout_act)
{
	uint8 core;
	int8 val;

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		val = READ_PHYREGCE(pi, EstPower_path, core, estPowerValid);

		/* Read the Actual Estimated Powers without adjustment */
		if (val) {
			Pout[core] = READ_PHYREGCE(pi, EstPower_path, core, estPower);
		} else {
			Pout[core] = 0;
		}

		val = READ_PHYREGCE(pi, TxPwrCtrlStatus_path, core, estPwrAdjValid);

		if (val) {
			Pout_act[core] = READ_PHYREGCE(pi, TxPwrCtrlStatus_path, core, estPwr_adj);
		} else {
			Pout_act[core] = 0;
		}
	}
}

int16
wlc_phy_tempsense_acphy(phy_info_t *pi)
{
	uint8 core = 0, core_cnt = 0;
	uint8 sel_Vb, swap_amp;
	int8 idx;
	uint16 auxPGA_Av = 0x3, auxPGA_Vmid = 0x91;
	int16 V[4][3] = {{0, 0}}, V_cores[3] = {0}, Vout[4] = {0, 0, 0, 0};
	int16 offset = (int16) pi->phy_tempsense_offset;
	int32 radio_temp = 0;
	int32 t_scale = 16384;
	int32 t_slope = 8766;
	int32 t_offset = 1902100;
	int32 avbq_scale = 256;
	int32 avbq_slope[3] = {527, 521, 522};

	 /* if (ACREV_IS(pi->pubpi.phy_rev, 2)) {
		 PHY_INFORM(("tempsense for 4335 hasn't been brought up!\n"));
		 return 50;
	 }
	 */

	/* Prepare Mac and Phregs */
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		auxPGA_Av = 0x3;
		auxPGA_Vmid = 0x91;
	}


	wlc_phy_tempsense_phy_setup_acphy(pi);
	wlc_phy_tempsense_radio_setup_acphy(pi, auxPGA_Av, auxPGA_Vmid);

	for (idx = 0; idx < 4; idx++) {
		switch (idx) {
		case 0:
			sel_Vb = 1;
			swap_amp = 0;
			break;
		case 1:
			sel_Vb = 0;
			swap_amp = 0;
			break;
		case 2:
			sel_Vb = 1;
			swap_amp = 1;
			break;
		case 3:
			sel_Vb = 0;
			swap_amp = 1;
			break;
		default:
			sel_Vb = 0;
			swap_amp = 0;
			break;
		}

		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			/* Reg conflict with 2069 rev 16 */
			if (ACREV0) {
				MOD_RADIO_REGFLDC(pi, RF_2069_OVR18(core), OVR18,
				                  ovr_tempsense_sel_Vbe_Vbg, 0x1);
			} else {
				MOD_RADIO_REGFLDC(pi, RF_2069_GE16_OVR19(core), GE16_OVR19,
				                  ovr_tempsense_sel_Vbe_Vbg, 0x1);
			}
			MOD_RADIO_REGFLDC(pi, RF_2069_TEMPSENSE_CFG(core), TEMPSENSE_CFG,
			                  tempsense_sel_Vbe_Vbg, sel_Vb);
			/* Reg conflict with 2069 rev 16 */
			if (ACREV0) {
				MOD_RADIO_REGFLDC(pi, RF_2069_OVR18(core), OVR18,
				                  ovr_tempsense_swap_amp, 0x1);
			} else {
				MOD_RADIO_REGFLDC(pi, RF_2069_GE16_OVR19(core), GE16_OVR19,
				                  ovr_tempsense_swap_amp, 0x1);
			}
			MOD_RADIO_REGFLDC(pi, RF_2069_TEMPSENSE_CFG(core), TEMPSENSE_CFG,
			                  swap_amp, swap_amp);
		}
		OSL_DELAY(100);
		wlc_phy_poll_samps_WAR_acphy(pi, V_cores, FALSE, FALSE, NULL);
		OSL_DELAY(10);

		core_cnt = 0;
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			V[idx][core] = V_cores[core];
			core_cnt++;
		}
		OSL_DELAY(100);
	}


	if (ACREV1X1_IS(pi->pubpi.phy_rev)) {
		t_slope 		= -5453;
		avbq_scale 		= 800;
		avbq_slope[0]	= 1024;
		t_scale			= 16384;
		t_offset		= 1748881;

		radio_temp += (int32)(((V[0][0] + V[2][0] - V[1][0] - V[3][0]) / 2)
				  * t_slope * avbq_scale) / avbq_slope[0];

		radio_temp = (radio_temp + t_offset)/t_scale;

		/* Forcing offset to zero as this is not characterized yet for 4335 */
		offset = 0;

	} else {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			radio_temp += (int32)(V[1][core] + V[3][core] - V[0][core] - V[2][core])
					  * t_slope / 2 * avbq_scale / avbq_slope[core] / core_cnt;
			Vout[0] += V[0][core] / core_cnt;
			Vout[1] += V[1][core] / core_cnt;
			Vout[2] += V[2][core] / core_cnt;
			Vout[3] += V[3][core] / core_cnt;
		}
		radio_temp = (radio_temp + t_offset) / t_scale;

	}

	PHY_THERMAL(("Tempsense\n\tAuxADC0 Av,Vmid = 0x%x,0x%x\n",
	             auxPGA_Av, auxPGA_Vmid));
	PHY_THERMAL(("\tVref1,Vref2,Vctat1,Vctat2 =%d,%d,%d,%d\n",
	             Vout[0], Vout[2], Vout[1], Vout[3]));
	PHY_THERMAL(("\t^C Formula: (%d*(Vctat1+Vctat2-Vref1-Vref2)/2/cal_AvBq1*800/1024+%d)/%d\n",
	             t_slope, t_offset, t_scale));
	PHY_THERMAL(("\t^C = %d, applied offset = %d\n",
	             radio_temp, offset));

	wlc_phy_tempsense_phy_cleanup_acphy(pi);
	wlc_phy_tempsense_radio_cleanup_acphy(pi);

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);

	/* Store temperature and return value */
	pi->u.pi_acphy->current_temperature = (int16) radio_temp + offset;
	return ((int16) radio_temp + offset);
}

static void
wlc_phy_tempsense_radio_setup_acphy(phy_info_t *pi, uint16 Av, uint16 Vmid)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_tempsense_radioregs_t *porig = &(pi_ac->ac_tempsense_radioregs_orig);
	uint8 core;

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* Reg conflict with 2069 rev 16 */
		if (ACREV0)
			porig->OVR18[core]         = read_radio_reg(pi, RF_2069_OVR18(core));
		else
			porig->OVR19[core]         = read_radio_reg(pi, RF_2069_GE16_OVR19(core));
		porig->tempsense_cfg[core] = read_radio_reg(pi, RF_2069_TEMPSENSE_CFG(core));
		porig->OVR5[core]          = read_radio_reg(pi, RF_2069_OVR5(core));
		porig->testbuf_cfg1[core]  = read_radio_reg(pi, RF_2069_TESTBUF_CFG1(core));
		porig->OVR3[core]          = read_radio_reg(pi, RF_2069_OVR3(core));
		porig->auxpga_cfg1[core]   = read_radio_reg(pi, RF_2069_AUXPGA_CFG1(core));
		porig->auxpga_vmid[core]   = read_radio_reg(pi, RF_2069_AUXPGA_VMID(core));

		MOD_RADIO_REGFLDC(pi, RF_2069_OVR5(core), OVR5,
		                  ovr_tempsense_pu, 0x1);
		MOD_RADIO_REGFLDC(pi, RF_2069_TEMPSENSE_CFG(core), TEMPSENSE_CFG,
		                  pu, 0x1);
		MOD_RADIO_REGFLDC(pi, RF_2069_OVR5(core), OVR5,
		                  ovr_testbuf_PU, 0x1);
		MOD_RADIO_REGFLDC(pi, RF_2069_TESTBUF_CFG1(core), TESTBUF_CFG1,
		                  PU, 0x1);
		MOD_RADIO_REGFLDC(pi, RF_2069_TESTBUF_CFG1(core), TESTBUF_CFG1,
		                  GPIO_EN, 0x0);
		MOD_RADIO_REGFLDC(pi, RF_2069_OVR3(core), OVR3,
		                  ovr_afe_auxpga_i_sel_vmid, 0x1);
		MOD_RADIO_REGFLDC(pi, RF_2069_AUXPGA_VMID(core), AUXPGA_VMID,
		                  auxpga_i_sel_vmid, Vmid);
		MOD_RADIO_REGFLDC(pi, RF_2069_OVR3(core), OVR3,
		                  ovr_auxpga_i_sel_gain, 0x1);
		MOD_RADIO_REGFLDC(pi, RF_2069_AUXPGA_CFG1(core), AUXPGA_CFG1,
		                  auxpga_i_sel_gain, Av);

		if (ACRADIO1X1_IS(RADIOREV(pi->pubpi.radiorev))) {
			MOD_RADIO_REGFLDC(pi, RF_2069_AUXPGA_CFG1(core), AUXPGA_CFG1,
			                  auxpga_i_vcm_ctrl, 0x0);
			/* This bit is supposed to be controlled by phy direct control line.
			 * Please check: http://jira.broadcom.com/browse/HW11ACRADIO-45
			 */
			MOD_RADIO_REGFLDC(pi, RF_2069_AUXPGA_CFG1(core), AUXPGA_CFG1,
			                  auxpga_i_sel_input, 0x1);
		}
	}
}

static void
wlc_phy_tempsense_phy_setup_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_tempsense_phyregs_t *porig = &(pi_ac->ac_tempsense_phyregs_orig);
	uint16 sdadc_config;
	uint8 core;

	if (CHSPEC_IS80(pi->radio_chanspec)) {
		sdadc_config = sdadc_cfg80;
	} else if (CHSPEC_IS40(pi->radio_chanspec)) {
		sdadc_config = sdadc_cfg40;
	} else {
		sdadc_config = sdadc_cfg20;
	}

	porig->RxFeCtrl1 = phy_reg_read(pi, ACPHY_RxFeCtrl1);
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, swap_iq0, 0);
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, swap_iq1, 0);
	MOD_PHYREG(pi, ACPHY_RxFeCtrl1, swap_iq2, 0);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		porig->RfctrlIntc[core] = phy_reg_read(pi, ACPHYREGCE(RfctrlIntc, core));
		phy_reg_write(pi, ACPHYREGCE(RfctrlIntc, core), 0);

		porig->RfctrlOverrideAuxTssi[core]  = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideAuxTssi, core));
		porig->RfctrlCoreAuxTssi1[core]     = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreAuxTssi1, core));
		porig->RfctrlOverrideRxPus[core]    = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideRxPus, core));
		porig->RfctrlCoreRxPus[core]        = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreRxPus, core));
		porig->RfctrlOverrideTxPus[core]    = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideTxPus, core));
		porig->RfctrlCoreTxPus[core]        = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreTxPus, core));
		porig->RfctrlOverrideLpfSwtch[core] = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideLpfSwtch, core));
		porig->RfctrlCoreLpfSwtch[core]     = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreLpfSwtch, core));
		porig->RfctrlOverrideAfeCfg[core]   = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideAfeCfg, core));
		porig->RfctrlCoreAfeCfg1[core]      = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreAfeCfg1, core));
		porig->RfctrlCoreAfeCfg2[core]      = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreAfeCfg2, core));
		porig->RfctrlOverrideGains[core]    = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlOverrideGains, core));
		porig->RfctrlCoreLpfGain[core]      = phy_reg_read(pi,
		                                      ACPHYREGCE(RfctrlCoreLpfGain, core));

		MOD_PHYREGCE(pi, RfctrlOverrideAuxTssi,  core, amux_sel_port, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAuxTssi1,     core, amux_sel_port, 1);
		MOD_PHYREGCE(pi, RfctrlOverrideAuxTssi,  core, afe_iqadc_aux_en, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAuxTssi1,     core, afe_iqadc_aux_en, 1);
		MOD_PHYREGCE(pi, RfctrlOverrideRxPus,    core, lpf_pu_dc, 1);
		MOD_PHYREGCE(pi, RfctrlCoreRxPus,        core, lpf_pu_dc, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideTxPus,    core, lpf_bq1_pu, 1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus,        core, lpf_bq1_pu, 1);
		MOD_PHYREGCE(pi, RfctrlOverrideTxPus,    core, lpf_bq2_pu, 1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus,        core, lpf_bq2_pu, 0);
		MOD_PHYREGCE(pi, RfctrlOverrideTxPus,    core, lpf_pu, 1);
		MOD_PHYREGCE(pi, RfctrlCoreTxPus,        core, lpf_pu, 1);

		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideLpfSwtch, core),     0x3ff);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfSwtch, core),         0x154);

		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_iqadc_mode,      1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,      core,
		             afe_iqadc_mode,      sdadc_config & 0x7);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_iqadc_pwrup,     1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg1,      core,
		             afe_iqadc_pwrup,     (sdadc_config >> 3) & 0x3f);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_iqadc_flashhspd, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,      core,
		             afe_iqadc_flashhspd, (sdadc_config >> 9) & 0x1);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_ctrl_flash17lvl, 1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,      core,
		             afe_ctrl_flash17lvl, (sdadc_config >> 10) & 0x1);
		MOD_PHYREGCE(pi, RfctrlOverrideAfeCfg,   core,
		             afe_iqadc_adc_bias,  1);
		MOD_PHYREGCE(pi, RfctrlCoreAfeCfg2,      core,
		             afe_iqadc_adc_bias,  (sdadc_config >> 11) & 0x3);

		MOD_PHYREGCE(pi, RfctrlOverrideGains,    core, lpf_bq1_gain, 1);
		MOD_PHYREGCE(pi, RfctrlCoreLpfGain,      core, lpf_bq1_gain, 0);
	}
}

static void
wlc_phy_tempsense_radio_cleanup_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_tempsense_radioregs_t *porig = &(pi_ac->ac_tempsense_radioregs_orig);
	uint8 core;

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		/* Reg conflict with 2069 rev 16 */
		if (ACREV0)
			write_radio_reg(pi, RF_2069_OVR18(core),         porig->OVR18[core]);
		else
			write_radio_reg(pi, RF_2069_GE16_OVR19(core),         porig->OVR19[core]);
		write_radio_reg(pi, RF_2069_TEMPSENSE_CFG(core), porig->tempsense_cfg[core]);
		write_radio_reg(pi, RF_2069_OVR5(core),          porig->OVR5[core]);
		write_radio_reg(pi, RF_2069_TESTBUF_CFG1(core),  porig->testbuf_cfg1[core]);
		write_radio_reg(pi, RF_2069_OVR3(core),          porig->OVR3[core]);
		write_radio_reg(pi, RF_2069_AUXPGA_CFG1(core),   porig->auxpga_cfg1[core]);
		write_radio_reg(pi, RF_2069_AUXPGA_VMID(core),   porig->auxpga_vmid[core]);
	}
}

static void
wlc_phy_tempsense_phy_cleanup_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_tempsense_phyregs_t *porig = &(pi_ac->ac_tempsense_phyregs_orig);
	uint8 core;

	phy_reg_write(pi, ACPHY_RxFeCtrl1, porig->RxFeCtrl1);

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		phy_reg_write(pi, ACPHYREGCE(RfctrlIntc, core), porig->RfctrlIntc[core]);

		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAuxTssi, core),
		              porig->RfctrlOverrideAuxTssi[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAuxTssi1, core),
		              porig->RfctrlCoreAuxTssi1[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideRxPus, core),
		              porig->RfctrlOverrideRxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRxPus, core),
		              porig->RfctrlCoreRxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideTxPus, core),
		              porig->RfctrlOverrideTxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreTxPus, core),
		              porig->RfctrlCoreTxPus[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideLpfSwtch, core),
		              porig->RfctrlOverrideLpfSwtch[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfSwtch, core),
		              porig->RfctrlCoreLpfSwtch[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideAfeCfg, core),
		              porig->RfctrlOverrideAfeCfg[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg1, core),
		              porig->RfctrlCoreAfeCfg1[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreAfeCfg2, core),
		              porig->RfctrlCoreAfeCfg2[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideGains, core),
		              porig->RfctrlOverrideGains[core]);
		phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfGain, core),
		              porig->RfctrlCoreLpfGain[core]);
	}
}

void
wlc_phy_rxcore_setstate_acphy(wlc_phy_t *pih, uint8 rxcore_bitmask)
{
	phy_info_t *pi = (phy_info_t*)pih;
	uint16 rfseqCoreActv_DisRx_save;
	uint16 rfseqMode_save;

	ASSERT((rxcore_bitmask > 0) && (rxcore_bitmask <= 7));
	pi->sh->phyrxchain = rxcore_bitmask;

	if (!pi->sh->clk)
		return;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);

	/* Save Registers */
	rfseqCoreActv_DisRx_save = READ_PHYREG(pi, ACPHY_RfseqCoreActv2059, DisRx);
	rfseqMode_save = phy_reg_read(pi, ACPHY_RfseqMode);

	/* Indicate to PHY of the Inactive Core */
	MOD_PHYREG(pi, ACPHY_CoreConfig, CoreMask, rxcore_bitmask);
	/* Indicate to RFSeq of the Inactive Core */
	MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, EnRx, rxcore_bitmask);
	/* Make sure Rx Chain gets shut off in Rx2Tx Sequence */
	MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, DisRx, 7);
	/* Make sure Tx Chain doesn't get turned off during this function */
	MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, EnTx, 0);
	MOD_PHYREG(pi, ACPHY_RfseqMode, CoreActv_override, 1);

	wlc_phy_force_rfseq_acphy(pi, ACPHY_RFSEQ_RX2TX);
	wlc_phy_force_rfseq_acphy(pi, ACPHY_RFSEQ_TX2RX);

	/* Make TxEn chains point to hwphytxchain & shoule be subset of rxchain */
	MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, EnTx, pi->sh->hw_phytxchain & rxcore_bitmask);

	/*  Restore Register */
	MOD_PHYREG(pi, ACPHY_RfseqCoreActv2059, DisRx, rfseqCoreActv_DisRx_save);
	phy_reg_write(pi, ACPHY_RfseqMode, rfseqMode_save);

#ifdef DSLCPE_C601911
	if (ACPHY_NEED_REINIT(pi)){
		ACPHY_NEED_REINIT(pi)=0;
		wlc_phy_init_acphy(pi);
	}
#endif
	wlapi_enable_mac(pi->sh->physhim);
}

uint8
wlc_phy_rxcore_getstate_acphy(wlc_phy_t *pih)
{
	uint16 rxen_bits;
	phy_info_t *pi = (phy_info_t*)pih;

	rxen_bits = READ_PHYREG(pi, ACPHY_RfseqCoreActv2059, EnRx);

	ASSERT(pi->sh->phyrxchain == rxen_bits);

	return ((uint8) rxen_bits);
}

static void wlc_phy_tx_gm_gain_boost(phy_info_t *pi)
{
	uint8 core;
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			MOD_RADIO_REGFLDC(pi, RF_2069_TXGM_CFG1(core), TXGM_CFG1, gc_res, 0x1);
		}
	} else {
		if (pi_ac->srom.gainboosta01) {
			/* Boost A0/1 radio gain */
			for (core = 0; core < 2; core++) {
				MOD_RADIO_REGFLDC(pi, RF_2069_TXMIX5G_CFG1(core),
				                  TXMIX5G_CFG1, gainboost, 0x6);
				MOD_RADIO_REGFLDC(pi, RF_2069_PGA5G_CFG1(core),
				                  PGA5G_CFG1, gainboost, 0x6);
			}
		}
		if (RADIOREV(pi->pubpi.radiorev) <= 3) {
			/* Boost A2 radio gain */
			core = 2;
			MOD_RADIO_REGFLDC(pi, RF_2069_TXMIX5G_CFG1(core),
			                          TXMIX5G_CFG1, gainboost, 0x6);
			MOD_RADIO_REGFLDC(pi, RF_2069_PGA5G_CFG1(core), PGA5G_CFG1, gainboost, 0x6);
		}
	}
}

static void acphy_load_txv_for_spexp(phy_info_t *pi)
{
	uint32 len = 243, offset = 1220;

	wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_BFMUSERINDEX,
	                          len, offset, 32, acphy_txv_for_spexp);
}

static int
wlc_phy_txpower_core_offset_set_acphy(phy_info_t *pi, struct phy_txcore_pwr_offsets *offsets)
{
	return BCME_OK;
}

static int
wlc_phy_txpower_core_offset_get_acphy(phy_info_t *pi, struct phy_txcore_pwr_offsets *offsets)
{
	return BCME_OK;
}


static void
wlc_phy_set_crs_min_pwr_acphy(phy_info_t *pi, uint8 ac_th, int8 offset_1, int8 offset_2)
{
	uint8 core;
	uint8 mf_th = ac_th;
	int8  mf_off1 = offset_1;

	if (ac_th == 0) {
		if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5)) {
			if (CHSPEC_IS20(pi->radio_chanspec)) {
				mf_th = 60;
				ac_th = 58;
			} else if (CHSPEC_IS40(pi->radio_chanspec)) {
				mf_th = 60;
				ac_th = 58;
			} else if (CHSPEC_IS80(pi->radio_chanspec)) {
				mf_th = 67;
				ac_th = 60;
			}
		} else {
			mf_th = ACPHY_CRSMIN_DEFAULT;
			ac_th = ACPHY_CRSMIN_DEFAULT;
		}
	} else {
		if (ACREV_IS(pi->pubpi.phy_rev, 2) || ACREV_IS(pi->pubpi.phy_rev, 5)) {
			if (CHSPEC_IS20(pi->radio_chanspec)) {
				mf_th = ((ac_th*101)/100) + 2;
			} else if (CHSPEC_IS40(pi->radio_chanspec)) {
				mf_th = ((ac_th*109)/100) - 2;
			} else if (CHSPEC_IS80(pi->radio_chanspec)) {
				mf_th = ((ac_th*17)/10) - 33;
			}
		}
	}

	/* Adjust offset values for 1-bit MF */
	/* Not needed for 4335 and 4360, will be needed for 4350, disabled for now */

	if (0)
	{
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			if (core == 1) {
				if (CHSPEC_IS5G(pi->radio_chanspec)) {
					if (CHSPEC_IS20(pi->radio_chanspec))
					{
						mf_off1 = ((((ac_th+offset_1)*101)/100) + 2)-mf_th;
					} else if (CHSPEC_IS40(pi->radio_chanspec)) {
						mf_off1 = ((((ac_th+offset_1)*109)/100) - 2)-mf_th;
					} else if (CHSPEC_IS80(pi->radio_chanspec)) {
						mf_off1 = ((((ac_th+offset_1)*17)/10) - 33)-mf_th;
					}
				}
			}
		}
	}

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {

		switch (core) {
			case 0:
				MOD_PHYREG(pi, ACPHY_crsminpoweru0, crsminpower0, ac_th);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweru0, crsmfminpower0, mf_th);
				MOD_PHYREG(pi, ACPHY_crsminpowerl0, crsminpower0, ac_th);
				MOD_PHYREG(pi, ACPHY_crsmfminpowerl0, crsmfminpower0, mf_th);
				MOD_PHYREG(pi, ACPHY_crsminpoweruSub10, crsminpower0, ac_th);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweruSub10, crsmfminpower0,  mf_th);
				MOD_PHYREG(pi, ACPHY_crsminpowerlSub10, crsminpower0, ac_th);
				MOD_PHYREG(pi, ACPHY_crsmfminpowerlSub10, crsmfminpower0,  mf_th);
				break;
			case 1:
				/* Force the offsets for core-1 */
				/* Core 1 */
				MOD_PHYREG(pi, ACPHY_crsminpoweroffset1,
				           crsminpowerOffsetu, offset_1);
				MOD_PHYREG(pi, ACPHY_crsminpoweroffset1,
				           crsminpowerOffsetl, offset_1);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweroffset1,
				           crsmfminpowerOffsetu, mf_off1);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweroffset1,
				           crsmfminpowerOffsetl, mf_off1);
				MOD_PHYREG(pi, ACPHY_crsminpoweroffsetSub11,
				           crsminpowerOffsetlSub1, offset_1);
				MOD_PHYREG(pi, ACPHY_crsminpoweroffsetSub11,
				           crsminpowerOffsetuSub1, offset_1);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweroffsetSub11,
				           crsmfminpowerOffsetlSub1, mf_off1);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweroffsetSub11,
				           crsmfminpowerOffsetuSub1, mf_off1);
				break;
			case 2:
				/* Force the offsets for core-2 */
				/* Core 2 */
				MOD_PHYREG(pi, ACPHY_crsminpoweroffset2,
				           crsminpowerOffsetu, offset_2);
				MOD_PHYREG(pi, ACPHY_crsminpoweroffset2,
				           crsminpowerOffsetl, offset_2);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweroffset2,
				           crsmfminpowerOffsetu, offset_2);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweroffset2,
				           crsmfminpowerOffsetl, offset_2);
				MOD_PHYREG(pi, ACPHY_crsminpoweroffsetSub12,
				           crsminpowerOffsetlSub1, offset_2);
				MOD_PHYREG(pi, ACPHY_crsminpoweroffsetSub12,
				           crsminpowerOffsetuSub1, offset_2);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweroffsetSub12,
				           crsmfminpowerOffsetlSub1, offset_2);
				MOD_PHYREG(pi, ACPHY_crsmfminpoweroffsetSub12,
				           crsmfminpowerOffsetuSub1, offset_2);
				break;
		default:
			break;
		}
	}
}

#if defined(WLTEST)

int16
wlc_phy_test_tssi_acphy(phy_info_t *pi, int8 ctrl_type, int8 pwr_offs)
{
	int16 tssi;

	switch (ctrl_type & 0x7) {
	case 0:
	case 1:
	case 2:
		tssi = phy_reg_read(pi, ACPHYREGCE(TssiVal_path, ctrl_type)) & 0x3ff;
		tssi -= (tssi >= 512)? 1024 : 0;
		break;
	default:
		tssi = -1024;
	}
	return (tssi);
}

void
wlc_phy_test_scraminit_acphy(phy_info_t *pi, int8 init)
{
	uint16 mask, value;


	if (init < 0) {
		/* auto: clear Mode bit so that scrambler LFSR will be free
		 * running.  ok to leave scramindexctlEn and initState in
		 * whatever current condition, since their contents are unused
		 * when free running, but for ease of reg diffs, just write
		 * 0x7f to them for repeatability.
		 */
		mask = (ACPHY_ScramSigCtrl_scramCtrlMode_MASK |
		        ACPHY_ScramSigCtrl_scramindexctlEn_MASK |
		        ACPHY_ScramSigCtrl_initStateValue_MASK);
		value = ((0 << ACPHY_ScramSigCtrl_scramCtrlMode_SHIFT) |
		         ACPHY_ScramSigCtrl_initStateValue_MASK);
		phy_reg_mod(pi, ACPHY_ScramSigCtrl, mask, value);
	} else {
		/* fixed init: set Mode bit, clear scramindexctlEn, and write
		 * init to initState, so that scrambler LFSR will be
		 * initialized with specified value for each transmission.
		 */
		mask = (ACPHY_ScramSigCtrl_scramCtrlMode_MASK |
		        ACPHY_ScramSigCtrl_scramindexctlEn_MASK |
		        ACPHY_ScramSigCtrl_initStateValue_MASK);
		value = (ACPHY_ScramSigCtrl_scramCtrlMode_MASK |
		         (ACPHY_ScramSigCtrl_initStateValue_MASK &
		          (init << ACPHY_ScramSigCtrl_initStateValue_SHIFT)));
		phy_reg_mod(pi, ACPHY_ScramSigCtrl, mask, value);
	}
}
#endif 

void
wlc_phy_init_test_acphy(phy_info_t *pi)
{
	/* Force WLAN antenna */
	wlc_btcx_override_enable(pi);
	/* Disable tx power control */
	wlc_phy_txpwrctrl_enable_acphy(pi, PHY_TPC_HW_OFF);
	/* Recalibrate for this channel */
	wlc_phy_cals_acphy(pi, PHY_CAL_SEARCHMODE_RESTART);
}

static void
wlc_phy_rxcal_leakage_comp_acphy(phy_info_t *pi, phy_iq_est_t loopback_rx_iq,
phy_iq_est_t leakage_rx_iq, int32 *angle, int32 *mag)
{

	acphy_iq_mismatch_t loopback_mismatch, leakage_mismatch;
	int32 loopback_sin_angle;
	int32 leakage_sin_angle;

	int32 den, num, tmp;
	int16 nbits;
	int32 weight = 0;
	cint32 val;

	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;

	wlc_phy_calc_iq_mismatch_acphy(&loopback_rx_iq, &loopback_mismatch);
	*mag = loopback_mismatch.mag;

	if (pi_ac->fdiqi.leakage_comp_mode == ACPHY_RXCAL_LEAKAGE_COMP) {
		wlc_phy_calc_iq_mismatch_acphy(&leakage_rx_iq, &leakage_mismatch);

		loopback_sin_angle = loopback_mismatch.sin_angle;
		leakage_sin_angle  = leakage_mismatch.sin_angle;

		den = loopback_rx_iq.i_pwr + loopback_rx_iq.q_pwr;
		num = leakage_rx_iq.i_pwr  + leakage_rx_iq.q_pwr;


		nbits = wlc_phy_nbits(num);
		if (nbits % 2 == 1) nbits++;

		num = num << (30 - nbits);
		if (nbits > 10)
			den = den >> (nbits - 10);
		else
			den = den << (10 - nbits);
		num += (den >> 1);

		if (den != 0) {
			weight = (int32) wlc_phy_sqrt_int((uint32)(num / den));
		}

		if (weight > 41) { /* 40.96 = 0.04 * 2^10 */
			tmp = (loopback_sin_angle-leakage_sin_angle) * weight;
			tmp = tmp >> 10;

			val.q = loopback_sin_angle + tmp;

			tmp = (val.q >> 1);
			tmp *= tmp;
			tmp = (1 << 30) - tmp;
			val.i = (int32) wlc_phy_sqrt_int((uint32) tmp);
			val.i = ( val.i << 1) ;

			wlc_phy_inv_cordic(val, angle);
		} else {
			*angle = loopback_mismatch.angle;
		}
#if defined(BCMDBG_RXCAL)
	printf("   Ang :: %d loopback %d leakage %d weight %d Mag :: %d\n",
	       *angle, loopback_mismatch.angle, leakage_mismatch.angle,
		weight, *mag);
#endif /* BCMDBG_RXCAL */

	} else {
		*angle = loopback_mismatch.angle;
#if defined(BCMDBG_RXCAL)
		printf("   Ang :: %d Mag :: %d\n", *angle, *mag);
#endif /* BCMDBG_RXCAL */
	}


}


static void
wlc_phy_rx_fdiqi_lin_reg_acphy(phy_info_t *pi, acphy_rx_fdiqi_t *freq_ang_mag, uint16 num_data)
{

	int32 Sf2 = 0;
	int32 Sfa[PHY_CORE_MAX], Sa[PHY_CORE_MAX], Sm[PHY_CORE_MAX];
	int32 intcp[PHY_CORE_MAX], mag[PHY_CORE_MAX];
	int32 refBW;

	int8 idx;
	uint8 core;

	phy_iq_comp_t coeffs[PHY_CORE_MAX];
	int32 sin_angle, cos_angle;
	cint32 cordic_out;
	int32  a, b, sign_sa;

	/* initialize array for all cores to prevent compile warning (UNINIT) */
	FOREACH_CORE(pi, core) {
		Sfa[core] = 0; Sa[core] = 0; Sm[core] = 0;
	}

	for (idx = 0; idx < num_data; idx++) {
		Sf2 += freq_ang_mag[idx].freq * freq_ang_mag[idx].freq;
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			Sfa[core] += freq_ang_mag[idx].freq * freq_ang_mag[idx].angle[core];
			Sa[core] += freq_ang_mag[idx].angle[core];
			Sm[core] += freq_ang_mag[idx].mag[core];
		}
	}

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		sign_sa = Sa[core] >= 0 ? 1 : -1;
		intcp[core] = (Sa[core] + sign_sa * (num_data >> 1)) / num_data;
		mag[core]   = (Sm[core] + (num_data >> 1)) / num_data;

		wlc_phy_cordic(intcp[core], &cordic_out);
		sin_angle = cordic_out.q;
		cos_angle = cordic_out.i;

		b = mag[core] * cos_angle;
		a = mag[core] * sin_angle;

		b = ((b >> 15) + 1) >> 1;
		b -= (1 << 10);  /* 10 bit */
		a = ((a >> 15) + 1) >> 1;

		coeffs[core].a = a & 0x3ff;
		coeffs[core].b = b & 0x3ff;

		if (pi->u.pi_acphy->fdiqi.enabled) {
			refBW = (CHSPEC_IS80(pi->radio_chanspec)) ? 30 :
				(CHSPEC_IS40(pi->radio_chanspec)) ? 15 : 8;
			pi->u.pi_acphy->fdiqi.slope[core] =
				(((-Sfa[core] * refBW / Sf2) >> 14) + 1 ) >> 1;
		}

#if defined(BCMDBG_RXCAL)
		printf("   a=%d b=%d :: ", a, b);
		if (pi->u.pi_acphy->fdiqi.enabled) {
			printf("   Slope = %d\n", pi->u.pi_acphy->fdiqi.slope[core]);
		} else {
			printf("   Slope = OFF\n");
		}
#endif /* BCMDBG_RXCAL */

	}

	FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
		wlc_phy_rx_iq_comp_acphy(pi, 1, &(coeffs[core]), core);
	}

	if (pi->u.pi_acphy->fdiqi.enabled) {
		wlc_phy_rx_fdiqi_comp_acphy(pi, TRUE);
	}

}


static void
wlc_phy_rx_fdiqi_comp_acphy(phy_info_t *pi, bool enable)
{
	uint8 core;
	int16 sign_slope;
	int8 idx;
	int32 slope;
#if defined(BCMDBG_RXCAL)
	int16 regval;
#endif /* BCMDBG_RXCAL */

	int16 filtercoeff[11][11] = {{0, 0, 0, 0, 0, 1024, 0, 0, 0, 0, 0},
		{-12, 15, -20, 30, -60, 1024, 61, -30, 20, -15, 12},
		{-24, 30, -40, 60, -120, 1024, 122, -61, 41, -30, 24},
		{-36, 45, -60, 91, -180, 1024, 184, -92, 61, -46, 37},
		{-42, 52, -69, 103, -206, 1024, 211, -105, 70, -52, 42},
		{-52, 65, -86, 129, -256, 1024, 264, -131, 87, -65, 52},
		{-62, 78, -103, 155, -307, 1023, 319, -158, 105, -78, 63},
		{-73, 91, -121, 180, -357, 1023, 373, -184, 122, -92, 73},
		{-83, 104, -138, 206, -407, 1023, 428, -211, 140, -105, 84},
		{-93, 117, -155, 231, -456, 1023, 483, -238, 158, -118, 94},
		{-104, 129, -172, 257, -506, 1022, 539, -265, 176, -132, 105}};

	/* enable: 0 - disable FDIQI comp
	 *         1 - program FDIQI comp filter and enable
	 */

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* write values */
	if (enable == FALSE) {
		MOD_PHYREG(pi, ACPHY_rxfdiqImbCompCtrl, rxfdiqImbCompEnable, 0);
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			pi->u.pi_acphy->fdiqi.slope[core] = 0;
		}
#if defined(BCMDBG_RXCAL)
	/*	printf("   FDIQI Disabled\n"); */
#endif /* BCMDBG_RXCAL */
		return;
	} else {
		FOREACH_ACTV_CORE(pi, pi->sh->phyrxchain, core) {
			slope = pi->u.pi_acphy->fdiqi.slope[core];
			sign_slope = slope >= 0 ? 1 : -1;
			slope *= sign_slope;
			if (slope > 10) slope = 10;

			MOD_PHYREG(pi, ACPHY_rxfdiqImbN_offcenter_scale, N_offcenter_scale, 4);
			MOD_PHYREG(pi, ACPHY_rxfdiqImbCompCtrl, rxfdiq_iorq, 0);
			MOD_PHYREG(pi, ACPHY_rxfdiqImbCompCtrl, calibration_notoperation, 0);
			MOD_PHYREG(pi, ACPHY_fdiqi_rx_comp_Nshift_out, Nshift_out, 10);

			for (idx = 0; idx < 11; idx++) {
				if (sign_slope == -1) {
					phy_reg_write(pi, ACPHY_RXFDIQCOMP_STR(core, idx),
						filtercoeff[slope][10-idx]);
				} else {
					phy_reg_write(pi, ACPHY_RXFDIQCOMP_STR(core, idx),
						filtercoeff[slope][idx]);
				}
			}

#if defined(BCMDBG_RXCAL)
			printf("   Core=%d, Slope= %d :: ", core, sign_slope*slope);
			for (idx = 0; idx < 11; idx++) {
				regval = phy_reg_read(pi, ACPHY_RXFDIQCOMP_STR(core, idx));
				if (regval > 1024) regval -= 2048;
				printf(" %d", regval);
			}
			printf("\n");
#endif /* BCMDBG_RXCAL */

		}
		MOD_PHYREG(pi, ACPHY_rxfdiqImbCompCtrl, rxfdiqImbCompEnable, 1);
	}

}


static void
wlc_phy_set_analog_rxgain(phy_info_t *pi, uint8 clipgain, uint8 *gain_idx, bool trtx, uint8 core)
{
	uint8 lna1, lna2, mix, bq0, bq1, tx, rx;
	uint16 gaincodeA, gaincodeB, final_gain;

	lna1 = gain_idx[1];
	lna2 = gain_idx[2];
	mix = gain_idx[3];
	bq0 = gain_idx[4];
	bq1 = gain_idx[5];

	if (trtx) {
		tx = 1; rx = 0;
	} else {
		tx = 0; rx = 1;
	}

	gaincodeA = ((mix << 7) | (lna2 << 4) | (lna1 << 1));
	gaincodeB = (bq1 << 8) | (bq0 << 4) | (tx << 3) | (rx << 2);

	if (clipgain == 0) {
		phy_reg_write(pi, ACPHYREGC(InitGainCodeA, core), gaincodeA);
		phy_reg_write(pi, ACPHYREGC(InitGainCodeB, core), gaincodeB);
		final_gain = ((bq1 << 13) | (bq0 << 10) | (mix << 6) | (lna2 << 3) | (lna1 << 0));
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, (0xf9 + core), 16,
		                          &final_gain);
	} else if (clipgain == 1) {
		phy_reg_write(pi, ACPHYREGC(clipHiGainCodeA, core), gaincodeA);
		phy_reg_write(pi, ACPHYREGC(clipHiGainCodeB, core), gaincodeB);
	} else if (clipgain == 2) {
		phy_reg_write(pi, ACPHYREGC(clipmdGainCodeA, core), gaincodeA);
		phy_reg_write(pi, ACPHYREGC(clipmdGainCodeB, core), gaincodeB);
	} else if (clipgain == 3) {
		phy_reg_write(pi, ACPHYREGC(cliploGainCodeA, core), gaincodeA);
		phy_reg_write(pi, ACPHYREGC(cliploGainCodeB, core), gaincodeB);
	} else if (clipgain == 4) {
		phy_reg_write(pi, ACPHYREGC(clip2GainCodeA, core), gaincodeA);
		phy_reg_write(pi, ACPHYREGC(clip2GainCodeB, core), gaincodeB);
	}
}

static void
wlc_phy_srom_read_gainctrl_acphy(phy_info_t *pi)
{
	uint8 core, srom_rx;
	char srom_name[30];
	phy_info_acphy_t *pi_ac;
	uint8 raw_elna, raw_trloss, raw_bypass;

	pi_ac = pi->u.pi_acphy;
	pi_ac->srom.elna2g_present = (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
	                              BFL_SROM11_EXTLNA) != 0;
	pi_ac->srom.elna5g_present = (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
	                              BFL_SROM11_EXTLNA_5GHz) != 0;

	FOREACH_CORE(pi, core) {
		pi_ac->srom.femrx_2g[core].elna = 0;
		pi_ac->srom.femrx_2g[core].trloss = 0;
		pi_ac->srom.femrx_2g[core].elna_bypass_tr = 0;

		pi_ac->srom.femrx_5g[core].elna = 0;
		pi_ac->srom.femrx_5g[core].trloss = 0;
		pi_ac->srom.femrx_5g[core].elna_bypass_tr = 0;

		pi_ac->srom.femrx_5gm[core].elna = 0;
		pi_ac->srom.femrx_5gm[core].trloss = 0;
		pi_ac->srom.femrx_5gm[core].elna_bypass_tr = 0;

		pi_ac->srom.femrx_5gh[core].elna = 0;
		pi_ac->srom.femrx_5gh[core].trloss = 0;
		pi_ac->srom.femrx_5gh[core].elna_bypass_tr = 0;

		/*  -------  2G -------  */
		if (pi_ac->srom.elna2g_present) {
			snprintf(srom_name, sizeof(srom_name),  "rxgains2gelnagaina%d", core);
			if (PHY_GETVAR(pi, srom_name) != NULL) {
				srom_rx = (uint8)PHY_GETINTVAR(pi, srom_name);
				pi_ac->srom.femrx_2g[core].elna = (2 * srom_rx) + 6;
			}

			snprintf(srom_name, sizeof(srom_name),  "rxgains2gtrelnabypa%d", core);
			if (PHY_GETVAR(pi, srom_name) != NULL) {
				pi_ac->srom.femrx_2g[core].elna_bypass_tr =
				        (uint8)PHY_GETINTVAR(pi, srom_name);
			}
		}

		snprintf(srom_name, sizeof(srom_name),  "rxgains2gtrisoa%d", core);
		if (PHY_GETVAR(pi, srom_name) != NULL) {
			srom_rx = (uint8)PHY_GETINTVAR(pi, srom_name);
			pi_ac->srom.femrx_2g[core].trloss = (2 * srom_rx) + 8;
		}


		/*  -------  5G -------  */
		if (pi_ac->srom.elna5g_present) {
			snprintf(srom_name, sizeof(srom_name),  "rxgains5gelnagaina%d", core);
			if (PHY_GETVAR(pi, srom_name) != NULL) {
				srom_rx = (uint8)PHY_GETINTVAR(pi, srom_name);
				pi_ac->srom.femrx_5g[core].elna = (2 * srom_rx) + 6;
			}

			snprintf(srom_name, sizeof(srom_name),  "rxgains5gtrelnabypa%d", core);
			if (PHY_GETVAR(pi, srom_name) != NULL) {
				pi_ac->srom.femrx_5g[core].elna_bypass_tr =
				        (uint8)PHY_GETINTVAR(pi, srom_name);
			}
		}

		snprintf(srom_name, sizeof(srom_name),  "rxgains5gtrisoa%d", core);
		if (PHY_GETVAR(pi, srom_name) != NULL) {
			srom_rx = (uint8)PHY_GETINTVAR(pi, srom_name);
			pi_ac->srom.femrx_5g[core].trloss = (2 * srom_rx) + 8;
		}

		/*  -------  5G (mid) -------  */
		raw_elna = 0; raw_trloss = 0; raw_bypass = 0;
		if (pi_ac->srom.elna5g_present) {
			snprintf(srom_name, sizeof(srom_name),  "rxgains5gmelnagaina%d", core);
			if (PHY_GETVAR(pi, srom_name) != NULL)
				raw_elna = (uint8)PHY_GETINTVAR(pi, srom_name);

			snprintf(srom_name, sizeof(srom_name),  "rxgains5gmtrelnabypa%d", core);
			if (PHY_GETVAR(pi, srom_name) != NULL)
				raw_bypass = (uint8)PHY_GETINTVAR(pi, srom_name);
		}
		snprintf(srom_name, sizeof(srom_name),  "rxgains5gmtrisoa%d", core);
		if (PHY_GETVAR(pi, srom_name) != NULL)
			raw_trloss = (uint8)PHY_GETINTVAR(pi, srom_name);

		if (((raw_elna == 0) && (raw_trloss == 0) && (raw_bypass == 0)) ||
		    ((raw_elna == 7) && (raw_trloss == 0xf) && (raw_bypass == 1))) {
			/* No entry in SROM, use generic 5g ones */
			pi_ac->srom.femrx_5gm[core].elna = pi_ac->srom.femrx_5g[core].elna;
			pi_ac->srom.femrx_5gm[core].elna_bypass_tr =
			        pi_ac->srom.femrx_5g[core].elna_bypass_tr;
			pi_ac->srom.femrx_5gm[core].trloss = pi_ac->srom.femrx_5g[core].trloss;
		} else {
			pi_ac->srom.femrx_5gm[core].elna = (2 * raw_elna) + 6;
			pi_ac->srom.femrx_5gm[core].elna_bypass_tr = raw_bypass;
			pi_ac->srom.femrx_5gm[core].trloss = (2 * raw_trloss) + 8;
		}

		/*  -------  5G (high) -------  */
		raw_elna = 0; raw_trloss = 0; raw_bypass = 0;
		if (pi_ac->srom.elna5g_present) {
			snprintf(srom_name, sizeof(srom_name),  "rxgains5ghelnagaina%d", core);
			if (PHY_GETVAR(pi, srom_name) != NULL)
				raw_elna = (uint8)PHY_GETINTVAR(pi, srom_name);

			snprintf(srom_name, sizeof(srom_name),  "rxgains5ghtrelnabypa%d", core);
			if (PHY_GETVAR(pi, srom_name) != NULL)
				raw_bypass = (uint8)PHY_GETINTVAR(pi, srom_name);
		}
		snprintf(srom_name, sizeof(srom_name),  "rxgains5ghtrisoa%d", core);
		if (PHY_GETVAR(pi, srom_name) != NULL)
			raw_trloss = (uint8)PHY_GETINTVAR(pi, srom_name);

		if (((raw_elna == 0) && (raw_trloss == 0) && (raw_bypass == 0)) ||
		    ((raw_elna == 7) && (raw_trloss == 0xf) && (raw_bypass == 1))) {
			/* No entry in SROM, use generic 5g ones */
			pi_ac->srom.femrx_5gh[core].elna = pi_ac->srom.femrx_5gm[core].elna;
			pi_ac->srom.femrx_5gh[core].elna_bypass_tr =
			        pi_ac->srom.femrx_5gm[core].elna_bypass_tr;
			pi_ac->srom.femrx_5gh[core].trloss = pi_ac->srom.femrx_5gm[core].trloss;
		} else {
			pi_ac->srom.femrx_5gh[core].elna = (2 * raw_elna) + 6;
			pi_ac->srom.femrx_5gh[core].elna_bypass_tr = raw_bypass;
			pi_ac->srom.femrx_5gh[core].trloss = (2 * raw_trloss) + 8;
		}
	}
}

static void
wlc_phy_rxgainctrl_gainctrl_acphy(phy_info_t *pi)
{
	bool elna_present, mdgain_trtx_allowed = TRUE;
	bool init_trtx, hi_trtx, md_trtx, lo_trtx;
	uint8 init_gain = ACPHY_INIT_GAIN, hi_gain = 48, mid_gain = 35, lo_gain, clip2_gain;
	uint8 hi_gain1, mid_gain1, lo_gain1;

	/* 1% PER point used for all the PER numbers */

	/* For bACI/ACI: max output pwrs {elna, lna1, lna2, mix, bq0, bq1} */
	uint8 maxgain_2g[] = {43, 43, 43, 52, 52, 100};
	uint8 maxgain_5g[] = {47, 47, 47, 52, 52, 100};

	uint8 i, core, elna_idx, stall_val;
	int8 md_low_end, hi_high_end, lo_low_end, md_high_end, max_himd_hi_end;
	int8 nbclip_pwrdBm, w1clip_pwrdBm;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	/* disable stall */
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		for (i = 0; i < ACPHY_MAX_RX_GAIN_STAGES; i++)
			pi_ac->rxgainctrl_maxout_gains[i] = maxgain_2g[i];
		elna_present = pi_ac->srom.elna2g_present;
	} else {
		for (i = 0; i < ACPHY_MAX_RX_GAIN_STAGES; i++)
			pi_ac->rxgainctrl_maxout_gains[i] = maxgain_5g[i];
		elna_present = pi_ac->srom.elna5g_present;
	}

	/* Keep track of it (used in interf_mitigation) */
	pi_ac->curr_desense.elna_bypass = wlc_phy_get_max_lna_index_acphy(pi, 0);
	init_trtx = elna_present & pi_ac->curr_desense.elna_bypass;
	hi_trtx = elna_present & pi_ac->curr_desense.elna_bypass;
	md_trtx = elna_present & (mdgain_trtx_allowed | pi_ac->curr_desense.elna_bypass);
	lo_trtx = TRUE;

	/* with elna if md_gain uses TR != T, then LO_gain needs to be higher */
	lo_gain = ((!elna_present) || md_trtx) ? 20 : 30;
	clip2_gain = md_trtx ? lo_gain : (mid_gain + lo_gain) >> 2;

	FOREACH_CORE(pi, core) {
		elna_idx = READ_PHYREGC(pi, InitGainCodeA, core, initExtLnaIndex);
		max_himd_hi_end = - 16 - pi_ac->rxgainctrl_params[core].gaintbl[0][elna_idx];
		if (pi_ac->curr_desense.elna_bypass == 1)
			max_himd_hi_end += pi_ac->fem_rxgains[core].trloss;

		/* 0,1,2,3 for Init, hi, md and lo gains respectively */
		wlc_phy_rxgainctrl_set_init_clip_gain_acphy(pi, 0, init_gain, init_trtx, core);
		hi_gain1 = wlc_phy_rxgainctrl_set_init_clip_gain_acphy(pi, 1, hi_gain,
		                                                      hi_trtx, core);
		mid_gain1 = wlc_phy_rxgainctrl_set_init_clip_gain_acphy(pi, 2, mid_gain,
		                                                       md_trtx, core);
		wlc_phy_rxgainctrl_set_init_clip_gain_acphy(pi, 4, clip2_gain, md_trtx, core);
		lo_gain1 = wlc_phy_rxgainctrl_set_init_clip_gain_acphy(pi, 3, lo_gain,
		                                                      lo_trtx, core);

		/* NB_CLIP */
		md_low_end = wlc_phy_rxgainctrl_calc_low_sens_acphy(pi, mid_gain1, md_trtx, core);
		hi_high_end = wlc_phy_rxgainctrl_calc_high_sens_acphy(pi, hi_gain1, hi_trtx, core);
		/* -1 times pwr to avoid rounding off error */
		nbclip_pwrdBm = (((-2*md_low_end)+(-3*hi_high_end)) * 13) >> 6;
		nbclip_pwrdBm *= -1;
		wlc_phy_rxgainctrl_nbclip_acphy(pi, core, nbclip_pwrdBm);

		/* w1_CLIP */
		lo_low_end = wlc_phy_rxgainctrl_calc_low_sens_acphy(pi, lo_gain1, lo_trtx, core);
		md_high_end = wlc_phy_rxgainctrl_calc_high_sens_acphy(pi, mid_gain1, md_trtx, core);
		/* -1 times pwr to avoid rounding off error */
		w1clip_pwrdBm = (((-2*lo_low_end) + (-3*md_high_end)) * 13) >> 6;
		w1clip_pwrdBm *= -1;
		wlc_phy_rxgainctrl_w1clip_acphy(pi, core, w1clip_pwrdBm);
	}

	/* Enable stall back */
	ACPHY_ENABLE_STALL(pi, stall_val);
}


static int8
wlc_phy_rxgainctrl_calc_low_sens_acphy(phy_info_t *pi, uint8 clipgain, bool trtx, uint8 core)
{
	int sens, sens_bw[] = {-66, -63, -60};   /* c9s1 1% sensitivity for 20/40/80 mhz */
	uint8 low_sen_adjust[] = {25, 22, 19};   /* low_end_sens = -clip_gain - low_sen_adjust */
	uint8 bw_idx, elna_idx, trloss, elna_bypass_tr;
	int8 elna, detection, demod;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	acphy_desense_values_t *desense = &pi_ac->total_desense;

	bw_idx = CHSPEC_IS20(pi->radio_chanspec) ? 0 : CHSPEC_IS40(pi->radio_chanspec) ? 1 : 2;
	sens =  sens_bw[bw_idx];

	elna_idx = READ_PHYREGC(pi, InitGainCodeA, core, initExtLnaIndex);
	elna = pi_ac->rxgainctrl_params[core].gaintbl[0][elna_idx];
	trloss = pi_ac->fem_rxgains[core].trloss;
	elna_bypass_tr = pi_ac->fem_rxgains[core].elna_bypass_tr;

	detection = 0 - (clipgain + low_sen_adjust[bw_idx]);
	demod = trtx ? (sens + trloss - (elna_bypass_tr * elna)) : sens;
	demod += desense->nf_hit_lna12;
	return MAX(detection, demod);
}

static int8
wlc_phy_rxgainctrl_calc_high_sens_acphy(phy_info_t *pi, uint8 clipgain, bool trtx, uint8 core)
{
	uint8 high_sen_adjust = 23;  /* high_end_sens = high_sen_adjust - clip_gain */
	uint8 elna_idx, trloss;
	int8 elna, saturation, clipped;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	elna_idx = READ_PHYREGC(pi, InitGainCodeA, core, initExtLnaIndex);
	elna = pi_ac->rxgainctrl_params[core].gaintbl[0][elna_idx];
	trloss = pi_ac->fem_rxgains[core].trloss;

	/* c9 needs lna1 input to be below -16dBm */
	saturation = trtx ? (trloss - 16 - elna) : 0 - (16 + elna);
	clipped = high_sen_adjust - clipgain;
	return MIN(saturation, clipped);
}

/* Wrapper to call encode_gain & set init/clip gains */
static uint8
wlc_phy_rxgainctrl_set_init_clip_gain_acphy(phy_info_t *pi, uint8 clipgain, uint8 gain_dB,
                                      bool trtx, uint8 core)
{
	uint8 gain_idx[ACPHY_MAX_RX_GAIN_STAGES];
	uint8 gain_applied;

	gain_applied = wlc_phy_rxgainctrl_encode_gain_acphy(pi, core, gain_dB, trtx, gain_idx);
	wlc_phy_set_analog_rxgain(pi, clipgain, gain_idx, trtx, core);

	return gain_applied;
}

static uint8
wlc_phy_rxgainctrl_encode_gain_acphy(phy_info_t *pi, uint8 core, uint8 gain_dB,
                                     bool trloss, uint8 *gidx)
{
	uint8 min_gains[ACPHY_MAX_RX_GAIN_STAGES], max_gains[ACPHY_MAX_RX_GAIN_STAGES];
	int8 k, maxgain_this_stage;
	uint16 sum_min_gains, gain_needed, tr;
	uint8 i, j;
	int8 *gaintbl_this_stage, gain_this_stage;
	int16 total_gain, gain_applied;
	uint8 *gainbitstbl_this_stage;
	uint8 gaintbl_len, lowest_idx;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	gain_applied = 0;
	total_gain = 0;

	if (trloss) {
		tr =  pi_ac->fem_rxgains[core].trloss;
		for (i = 0; i < ACPHY_MAX_RX_GAIN_STAGES; i++)
			max_gains[i] = pi_ac->rxgainctrl_maxout_gains[i] +
			        pi_ac->fem_rxgains[core].trloss;
	} else {
		tr = 0;
		for (i = 0; i < ACPHY_MAX_RX_GAIN_STAGES; i++)
			max_gains[i] = pi_ac->rxgainctrl_maxout_gains[i];
	}
	gain_needed = gain_dB + tr;

	for (i = 0; i < ACPHY_MAX_RX_GAIN_STAGES; i++)
		min_gains[i] = pi_ac->rxgainctrl_params[core].gaintbl[i][0];

	for (i = 0; i < ACPHY_MAX_RX_GAIN_STAGES; i++) {
		if (i == ACPHY_MAX_RX_GAIN_STAGES - 2) {
			if ((gain_needed % 3) == 2)
				gain_needed += 1;
			if (gain_needed > 30)
				gain_needed = 30;
		}

		sum_min_gains = 0;
		for (j = i+1; j < ACPHY_MAX_RX_GAIN_STAGES; j++)
			sum_min_gains += min_gains[j];

		maxgain_this_stage = gain_needed - sum_min_gains;
		gaintbl_this_stage = pi_ac->rxgainctrl_params[core].gaintbl[i];
		gainbitstbl_this_stage = pi_ac->rxgainctrl_params[core].gainbitstbl[i];
		gaintbl_len = pi_ac->rxgainctrl_stage_len[i];

		for (k = gaintbl_len - 1; k >= 0; k--) {
			gain_this_stage = gaintbl_this_stage[k];

			total_gain = gain_this_stage + gain_applied;
			lowest_idx = 0;

			if (gainbitstbl_this_stage[k] == gainbitstbl_this_stage[0])
				lowest_idx = 1;
			if ((lowest_idx == 1) ||
			    ((gain_this_stage <= maxgain_this_stage) && (total_gain
			                                                 <= max_gains[i]))) {
				gidx[i] = gainbitstbl_this_stage[k];
				gain_applied += gain_this_stage;
				gain_needed = gain_needed - gain_this_stage;

				break;
			}
		}
	}

	return (gain_applied - tr);
}

#define ACPHY_NUM_NB_THRESH 8
static void
wlc_phy_rxgainctrl_nbclip_acphy(phy_info_t *pi, uint8 core, int rxpwr_dBm)
{
	/* Multuply all pwrs by 10 to avoid floating point math */
	int rxpwrdBm_60mv, pwr;
	int pwr_60mv[] = {-40, -40, -40};     /* 20, 40, 80 */
	uint8 nb_thresh[] = {0, 35, 60, 80, 95, 120, 140, 156}; /* nb_thresh*10 to avoid float */
	const char *reg_name[ACPHY_NUM_NB_THRESH] = {"low", "low", "mid", "mid", "mid",
					       "mid", "high", "high"};
	uint8 mux_sel[] = {0, 0, 1, 1, 1, 1, 2, 2};
	uint8 reg_val[] = {1, 0, 1, 2, 0, 3, 1, 0};
	uint8 nb, i;
	int nb_thresh_bq[ACPHY_NUM_NB_THRESH];
	int v1, v2, vdiff1, vdiff2;
	uint8 idx[ACPHY_MAX_RX_GAIN_STAGES];
	uint16 initgain_codeA;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	rxpwrdBm_60mv = (CHSPEC_IS80(pi->radio_chanspec)) ? pwr_60mv[2] :
	        (CHSPEC_IS40(pi->radio_chanspec)) ? pwr_60mv[1] : pwr_60mv[0];

	for (i = 0; i < ACPHY_NUM_NB_THRESH; i++) {
		nb_thresh_bq[i] = rxpwrdBm_60mv + nb_thresh[i];
	}

	/* Get the INITgain code */
	initgain_codeA = phy_reg_read(pi, ACPHYREGC(InitGainCodeA, core));
	idx[0] = (initgain_codeA & ACPHY_Core0InitGainCodeA_initExtLnaIndex_MASK) >>
	        ACPHY_Core1InitGainCodeA_initExtLnaIndex_SHIFT;
	idx[1] = (initgain_codeA & ACPHY_Core0InitGainCodeA_initLnaIndex_MASK) >>
	        ACPHY_Core1InitGainCodeA_initLnaIndex_SHIFT;
	idx[2] = (initgain_codeA & ACPHY_Core0InitGainCodeA_initlna2Index_MASK) >>
	        ACPHY_Core1InitGainCodeA_initlna2Index_SHIFT;
	idx[3] = (initgain_codeA & ACPHY_Core0InitGainCodeA_initmixergainIndex_MASK) >>
	        ACPHY_Core1InitGainCodeA_initmixergainIndex_SHIFT;
	idx[4] = READ_PHYREGC(pi, InitGainCodeB, core, InitBiQ0Index);
	idx[5] = READ_PHYREGC(pi, InitGainCodeB, core, InitBiQ1Index);

	pwr = rxpwr_dBm;
	for (i = 0; i < ACPHY_MAX_RX_GAIN_STAGES-1; i++)
		pwr += pi_ac->rxgainctrl_params[core].gaintbl[i][idx[i]];
	if (pi_ac->curr_desense.elna_bypass == 1)
		pwr = pwr - pi_ac->fem_rxgains[core].trloss;
	pwr = pwr * 10;

	nb = 0;
	if (pwr < nb_thresh_bq[0]) {
		nb = 0;
	} else if (pwr > nb_thresh_bq[ACPHY_NUM_NB_THRESH - 1]) {
		nb = ACPHY_NUM_NB_THRESH - 1;

		/* Reduce the bq0 gain, if can't achieve nbclip with highest nbclip thresh */
		if ((pwr - nb_thresh_bq[ACPHY_NUM_NB_THRESH - 1]) > 20) {
			if ((idx[4] > 0) && (idx[5] < 7)) {
				MOD_PHYREGC(pi, InitGainCodeB, core, InitBiQ0Index, idx[4] - 1);
				MOD_PHYREGC(pi, InitGainCodeB, core, InitBiQ1Index, idx[5] + 1);
			}
		}
	} else {
		for (i = 0; i < ACPHY_NUM_NB_THRESH - 1; i++) {
			v1 = nb_thresh_bq[i];
			v2 = nb_thresh_bq[i + 1];
			if ((pwr >= v1) && (pwr <= v2)) {
				vdiff1 = pwr > v1 ? (pwr - v1) : (v1 - pwr);
				vdiff2 = pwr > v2 ? (pwr - v2) : (v2 - pwr);

				if (vdiff1 < vdiff2)
					nb = i;
				else
					nb = i+1;
				break;
			}
		}
	}

	MOD_PHYREGC(pi, RssiClipMuxSel, core, fastAgcNbClipMuxSel, mux_sel[nb]);

	if (strcmp(reg_name[nb], "low") == 0) {
		MOD_RADIO_REGFLDC(pi, RF_2069_NBRSSI_CONFG(core), NBRSSI_CONFG,
		                  nbrssi_Refctrl_low, reg_val[nb]);
	} else if (strcmp(reg_name[nb], "mid") == 0) {
		MOD_RADIO_REGFLDC(pi, RF_2069_NBRSSI_CONFG(core), NBRSSI_CONFG,
		                  nbrssi_Refctrl_mid, reg_val[nb]);
	} else {
		MOD_RADIO_REGFLDC(pi, RF_2069_NBRSSI_CONFG(core), NBRSSI_CONFG,
		                  nbrssi_Refctrl_high, reg_val[nb]);
	}
}


#define ACPHY_NUM_W1_THRESH 12
static void
wlc_phy_rxgainctrl_w1clip_acphy(phy_info_t *pi, uint8 core, int rxpwr_dBm)
{
	/* Multuply all pwrs by 10 to avoid floating point math */

	int lna1_rxpwrdBm_lo4;
	int lna1_pwrs_w1clip[] = {-340, -340, -340};   /* 20, 40, 80 */
	uint8 *w1_hi, w1_delta[] = {0, 19, 35, 49, 60, 70, 80, 88, 95, 102, 109, 115};
	uint8 w1_delta_hi2g[] = {0, 19, 35, 49, 60, 70, 80, 92, 105, 120, 130, 140};
	uint8 w1_delta_hi5g[] = {0, 19, 35, 49, 60, 70, 80, 96, 113, 130, 155, 180};
	int w1_thresh_low[ACPHY_NUM_W1_THRESH], w1_thresh_mid[ACPHY_NUM_W1_THRESH];
	int w1_thresh_high[ACPHY_NUM_W1_THRESH];
	int *w1_thresh;
	uint8 i, w1_muxsel, w1;
	uint8 elna, lna1_idx;
	int v1, v2, vdiff1, vdiff2, pwr, lna1_diff;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	w1_hi = CHSPEC_IS2G(pi->radio_chanspec) ? &w1_delta_hi2g[0] : &w1_delta_hi5g[0];

	lna1_rxpwrdBm_lo4 = (CHSPEC_IS80(pi->radio_chanspec)) ? lna1_pwrs_w1clip[2] :
	        (CHSPEC_IS40(pi->radio_chanspec)) ? lna1_pwrs_w1clip[1] : lna1_pwrs_w1clip[0];

	/* mid is 6dB higher than low, and high is 6dB higher than mid */
	for (i = 0; i < ACPHY_NUM_W1_THRESH; i++) {
		w1_thresh_low[i] = lna1_rxpwrdBm_lo4 + w1_delta[i];
		w1_thresh_mid[i] = 60 + w1_thresh_low[i];
		w1_thresh_high[i] = 120 + lna1_rxpwrdBm_lo4 + w1_hi[i];
	}

	elna = pi_ac->rxgainctrl_params[core].gaintbl[0][0];

	lna1_idx = READ_PHYREGC(pi, InitGainCodeA, core, initLnaIndex);

	lna1_diff = pi_ac->rxgainctrl_params[core].gaintbl[1][5] -
	        pi_ac->rxgainctrl_params[core].gaintbl[1][lna1_idx];

	pwr = rxpwr_dBm + elna - lna1_diff;
	if (pi_ac->curr_desense.elna_bypass == 1)
		pwr = pwr - pi_ac->fem_rxgains[core].trloss;
	pwr = pwr * 10;

	if (pwr <= w1_thresh_low[0]) {
		w1 = 0;
		w1_muxsel = 0;
	} else if (pwr >= w1_thresh_high[ACPHY_NUM_W1_THRESH - 1]) {
		w1 = 11;
		w1_muxsel = 2;
	} else {
		if (pwr > w1_thresh_mid[ACPHY_NUM_W1_THRESH - 1]) {
			w1_thresh = w1_thresh_high;
			w1_muxsel = 2;
		} else if (pwr < w1_thresh_mid[0]) {
			w1_thresh = w1_thresh_low;
			w1_muxsel = 0;
		} else {
			w1_thresh = w1_thresh_mid;
			w1_muxsel = 1;
		}

		for (w1 = 0; w1 < ACPHY_NUM_W1_THRESH - 1; w1++) {
			v1 = w1_thresh[w1];
			v2 = w1_thresh[w1 + 1];
			if ((pwr >= v1) && (pwr <= v2)) {
				vdiff1 = pwr > v1 ? (pwr - v1) : (v1 - pwr);
				vdiff2 = pwr > v2 ? (pwr - v2) : (v2 - pwr);

				if (vdiff2 <= vdiff1)
					w1 = w1 + 1;

				break;
			}
		}
	}

	/* the w1 thresh array is wrt w1 code = 4 */
	/*	MOD_PHYREGC(pi, FastAgcClipCntTh, core, fastAgcW1ClipCntTh, w1 + 4); */
	MOD_PHYREGC(pi, RssiClipMuxSel, core, fastAgcW1ClipMuxSel, w1_muxsel);
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		MOD_RADIO_REGFLDC(pi, RF_2069_LNA2G_RSSI(core), LNA2G_RSSI,
		                  dig_wrssi1_threshold, w1 + 4);
	} else {
		MOD_RADIO_REGFLDC(pi, RF_2069_LNA5G_RSSI(core), LNA5G_RSSI,
		                  dig_wrssi1_threshold, w1 + 4);
	}
}

static uint8
wlc_phy_get_max_lna_index_acphy(phy_info_t *pi, uint8 lna)
{
	uint8 max_idx;
	acphy_desense_values_t *desense = NULL;
	uint8 elna_bypass, lna1_backoff, lna2_backoff;
	uint8 core;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	desense = &pi_ac->total_desense;
	elna_bypass = desense->elna_bypass;
	lna1_backoff = desense->lna1_tbl_desense;
	lna2_backoff = desense->lna2_tbl_desense;

	/* Find default max_idx */
	if (lna == 0) {
		max_idx = elna_bypass;       /* elna */
	} else if (lna == 1) {               /* lna1 */
		max_idx = MAX(0, ACPHY_MAX_LNA1_IDX  - lna1_backoff);
	} else {                             /* lna2 */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			if (pi_ac->srom.elna2g_present && (elna_bypass == 0)) {
				core = 0;
				if (pi_ac->srom.femrx_2g[core].elna > 9) {
					max_idx = ACPHY_ELNA2G_MAX_LNA2_IDX;
				} else {
					max_idx = ACPHY_ELNA2G_MAX_LNA2_IDX_L;
				}
			} else {
				max_idx = ACPHY_ILNA2G_MAX_LNA2_IDX;
			}
		} else {
			max_idx = (pi_ac->srom.elna5g_present && (elna_bypass == 0)) ?
			        ACPHY_ELNA5G_MAX_LNA2_IDX : ACPHY_ILNA5G_MAX_LNA2_IDX;
		}
		max_idx = MAX(0, max_idx - lna2_backoff);
	}

	return max_idx;
}

static void
wlc_phy_upd_lna1_lna2_gaintbls_acphy(phy_info_t *pi, uint8 lna12)
{
	uint8 offset, sz, core;
	uint8 gaintbl[10], gainbitstbl[10];
	uint8 max_idx, *default_gaintbl = NULL;
	uint16 gain_tblid, gainbits_tblid;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	acphy_desense_values_t *desense = &pi_ac->total_desense;

	if ((lna12 < 1) || (lna12 > 2)) return;

	sz = pi_ac->rxgainctrl_stage_len[lna12];

	if (lna12 == 1) {          /* lna1 */
		offset = 8;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			default_gaintbl = ac_lna1_2g;
		} else {
			if (CHSPEC_IS20(pi->radio_chanspec))
				default_gaintbl = ac_lna1_5g_20mhz;
			else if (CHSPEC_IS40(pi->radio_chanspec))
				default_gaintbl = ac_lna1_5g_40mhz;
			else
				default_gaintbl = ac_lna1_5g_80mhz;
		}
	} else {  /* lna2 */
		offset = 16;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			default_gaintbl = ac_lna2_2g;
		} else {
			default_gaintbl = ac_lna2_5g;
		}
	}

	max_idx = wlc_phy_get_max_lna_index_acphy(pi, lna12);
	wlc_phy_limit_rxgaintbl_acphy(gaintbl, gainbitstbl, sz, default_gaintbl,
	                              ACPHY_MIN_LNA1_LNA2_IDX, max_idx);

	/* Update pi_ac->curr_desense (used in interf_mitigation) */
	if (lna12 == 1) {
		pi_ac->curr_desense.lna1_tbl_desense = desense->lna1_tbl_desense;
	} else {
		pi_ac->curr_desense.lna2_tbl_desense = desense->lna2_tbl_desense;
	}

	/* Update gaintbl */
	FOREACH_CORE(pi, core) {
		if (core == 0) {
			gain_tblid =  ACPHY_TBL_ID_GAIN0;
			gainbits_tblid =  ACPHY_TBL_ID_GAINBITS0;
		} else if (core == 1) {
			gain_tblid =  ACPHY_TBL_ID_GAIN1;
			gainbits_tblid =  ACPHY_TBL_ID_GAINBITS1;
		} else {
			gain_tblid =  ACPHY_TBL_ID_GAIN2;
			gainbits_tblid =  ACPHY_TBL_ID_GAINBITS2;
		}

		memcpy(pi_ac->rxgainctrl_params[core].gaintbl[lna12], gaintbl, sizeof(uint8)*sz);
		wlc_phy_table_write_acphy(pi, gain_tblid, sz, offset, 8, gaintbl);
		memcpy(pi_ac->rxgainctrl_params[core].gainbitstbl[lna12], gainbitstbl,
		       sizeof(uint8)*sz);
		wlc_phy_table_write_acphy(pi, gainbits_tblid, sz, offset, 8, gainbitstbl);
	}
}

static void
wlc_phy_upd_lna1_lna2_gainlimittbls_acphy(phy_info_t *pi, uint8 lna12)
{
	uint8 i, sz, max_idx;
	uint8 lna1_tbl[] = {11, 12, 14, 32, 36, 40};
	uint8 lna2_tbl[] = {0, 0, 0, 3, 3, 3, 3};
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	acphy_desense_values_t *desense = &pi_ac->total_desense;

	sz = (lna12 == 1) ? 6 : 7;

	/* Limit based on desense mitigation mode */
	if (lna12 == 1) {
		max_idx = MAX(0, (sz - 1) - desense->lna1_gainlmt_desense);
		pi_ac->curr_desense.lna1_gainlmt_desense = desense->lna1_gainlmt_desense;
	} else {
		max_idx = MAX(0, (sz - 1) - desense->lna2_gainlmt_desense);
		pi_ac->curr_desense.lna2_gainlmt_desense = desense->lna2_gainlmt_desense;
	}

	/* Write 0x7f to entries not to be used */
	for (i = (max_idx + 1); i < sz; i++) {
		if (lna12 == 1) {
			lna1_tbl[i] = 0x7f;
		} else {
			lna2_tbl[i] = 0x7f;
		}
	}

	if (lna12 == 1)
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_GAINLIMIT, sz, 8, 8,  lna1_tbl);
	else
		wlc_phy_table_write_acphy(pi, ACPHY_TBL_ID_GAINLIMIT, sz, 16, 8, lna2_tbl);
}


static void
wlc_phy_limit_rxgaintbl_acphy(uint8 gaintbl[], uint8 gainbitstbl[], uint8 sz,
                              uint8 default_gaintbl[], uint8 min_idx, uint8 max_idx)
{
	uint8 i;

	for (i = 0; i < sz; i++) {
		if (i < min_idx) {
			gaintbl[i] = default_gaintbl[min_idx];
			gainbitstbl[i] = min_idx;
		} else if (i > max_idx) {
			gaintbl[i] = default_gaintbl[max_idx];
			gainbitstbl[i] = max_idx;
		} else {
			gaintbl[i] = default_gaintbl[i];
			gainbitstbl[i] = i;
		}
	}
}


void
wlc_phy_stf_chain_temp_throttle_acphy(phy_info_t *pi)
{
	uint8 txcore_shutdown_lut[] = {1, 1, 2, 1, 4, 1, 2, 1};
	uint8 phyrxchain = pi->sh->phyrxchain;
	uint8 phytxchain = pi->sh->phytxchain;
	uint8 new_phytxchain;
	int16 currtemp;


	ASSERT(phytxchain);

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	currtemp = wlc_phy_tempsense_acphy(pi);
	wlapi_enable_mac(pi->sh->physhim);
#ifdef BCMDBG
	if (pi->tempsense_override)
		currtemp = pi->tempsense_override;
#endif
	if (!pi->txcore_temp.heatedup) {
		if (currtemp >= pi->txcore_temp.disable_temp) {
			new_phytxchain = txcore_shutdown_lut[phytxchain];
			pi->txcore_temp.heatedup = TRUE;
			pi->txcore_temp.bitmap = ((phyrxchain << 4) | new_phytxchain);
		}
	} else {
		if (currtemp <= pi->txcore_temp.enable_temp) {
			new_phytxchain = pi->sh->hw_phytxchain;
			pi->txcore_temp.heatedup = FALSE;
			pi->txcore_temp.bitmap = ((phyrxchain << 4) | new_phytxchain);
		}
	}
}


void wlc_phy_rfctrl_override_rxgain_acphy(phy_info_t *pi, uint8 restore,
                                           rxgain_t rxgain[], rxgain_ovrd_t rxgain_ovrd[])
{
	uint8 core, lna1_Rout, lna2_Rout;
	uint16 reg_rxgain, reg_rxgain2, reg_lpfgain;
	uint8 stall_val;

	if (restore == 1) {
		/* restore the stored values */
		FOREACH_CORE(pi, core) {
			phy_reg_write(pi, ACPHYREGCE(RfctrlOverrideGains, core),
				rxgain_ovrd[core].rfctrlovrd);
			phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN1, core),
				rxgain_ovrd[core].rxgain);
			phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN2, core),
				rxgain_ovrd[core].rxgain2);
			phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfGain, core),
				rxgain_ovrd[core].lpfgain);
			PHY_INFORM(("%s, Restoring RfctrlOverride(rxgain) values\n", __FUNCTION__));
		}
	} else {
		stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
		ACPHY_DISABLE_STALL(pi);
		FOREACH_CORE(pi, core) {
			/* Save the original values */
			rxgain_ovrd[core].rfctrlovrd =
			  phy_reg_read(pi, ACPHYREGCE(RfctrlOverrideGains, core));
			rxgain_ovrd[core].rxgain =
			  phy_reg_read(pi, ACPHYREGCE(RfctrlCoreRXGAIN1, core));
			rxgain_ovrd[core].rxgain2 =
			  phy_reg_read(pi, ACPHYREGCE(RfctrlCoreRXGAIN2, core));
			rxgain_ovrd[core].lpfgain =
			  phy_reg_read(pi, ACPHYREGCE(RfctrlCoreLpfGain, core));

			if (CHSPEC_IS2G(pi->radio_chanspec))
			  wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_LNAROUT,
			   1, 5 + (24 * core), 8, &lna1_Rout);
			else
			  wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_LNAROUT,
			   1, 13 + (24 * core), 8, &lna1_Rout);
			  wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_LNAROUT,
			   1, 22 + (24 * core), 8, &lna2_Rout);
			/* Write the rxgain override registers */
			phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN1, core),
			              (rxgain[core].dvga << 10) | (rxgain[core].mix << 6) |
			              (rxgain[core].lna2 << 3) | rxgain[core].lna1);
			phy_reg_write(pi, ACPHYREGCE(RfctrlCoreRXGAIN2, core),
			              (((lna2_Rout >> 3) & 0xf)<<4 | ((lna1_Rout >> 3) & 0xf)));
			phy_reg_write(pi, ACPHYREGCE(RfctrlCoreLpfGain, core),
			              (rxgain[core].lpf1 << 3) | rxgain[core].lpf0);
			MOD_PHYREGCE(pi, RfctrlOverrideGains, core, rxgain, 1);
			MOD_PHYREGCE(pi, RfctrlOverrideGains, core, lpf_bq1_gain, 1);
			MOD_PHYREGCE(pi, RfctrlOverrideGains, core, lpf_bq2_gain, 1);

			reg_rxgain = phy_reg_read(pi, ACPHYREGCE(RfctrlCoreRXGAIN1, core));
			reg_rxgain2 = phy_reg_read(pi, ACPHYREGCE(RfctrlCoreRXGAIN2, core));
			reg_lpfgain = phy_reg_read(pi, ACPHYREGCE(RfctrlCoreLpfGain, core));
			PHY_INFORM(("%s, core %d. rxgain_ovrd = 0x%x, lpf_ovrd = 0x%x\n",
				__FUNCTION__, core, reg_rxgain, reg_lpfgain));
			PHY_INFORM(("%s, core %d. rxgain_rout_ovrd = 0x%x\n",
				__FUNCTION__, core, reg_rxgain2));
			BCM_REFERENCE(reg_rxgain);
			BCM_REFERENCE(reg_rxgain2);
			BCM_REFERENCE(reg_lpfgain);
		}
		ACPHY_ENABLE_STALL(pi, stall_val);
	}
}


/* Override/Restore routine for Rx Digital LPF:
 * 1) Override: Save digital LPF config and set new LPF configuration
 * 2) Restore: Restore digital LPF config
 */
void
wlc_phy_dig_lpf_override_acphy(phy_info_t *pi, uint8 dig_lpf_ht)
{
	if ((dig_lpf_ht > 0) && !pi->phy_rx_diglpf_default_coeffs_valid) {
		pi->phy_rx_diglpf_default_coeffs[0] = phy_reg_read(pi, ACPHY_RxStrnFilt40Num00);
		pi->phy_rx_diglpf_default_coeffs[1] = phy_reg_read(pi, ACPHY_RxStrnFilt40Num01);
		pi->phy_rx_diglpf_default_coeffs[2] = phy_reg_read(pi, ACPHY_RxStrnFilt40Num02);
		pi->phy_rx_diglpf_default_coeffs[3] = phy_reg_read(pi, ACPHY_RxStrnFilt40Den00);
		pi->phy_rx_diglpf_default_coeffs[4] = phy_reg_read(pi, ACPHY_RxStrnFilt40Den01);
		pi->phy_rx_diglpf_default_coeffs[5] = phy_reg_read(pi, ACPHY_RxStrnFilt40Num10);
		pi->phy_rx_diglpf_default_coeffs[6] = phy_reg_read(pi, ACPHY_RxStrnFilt40Num11);
		pi->phy_rx_diglpf_default_coeffs[7] = phy_reg_read(pi, ACPHY_RxStrnFilt40Num12);
		pi->phy_rx_diglpf_default_coeffs[8] = phy_reg_read(pi, ACPHY_RxStrnFilt40Den10);
		pi->phy_rx_diglpf_default_coeffs[9] = phy_reg_read(pi, ACPHY_RxStrnFilt40Den11);
		pi->phy_rx_diglpf_default_coeffs_valid = TRUE;

	}

	switch (dig_lpf_ht) {
	case 0:  /* restore rx dig lpf */

		/* ASSERT(pi->phy_rx_diglpf_default_coeffs_valid); */
		if (!pi->phy_rx_diglpf_default_coeffs_valid) {
			break;
		}
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num00, pi->phy_rx_diglpf_default_coeffs[0]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num01, pi->phy_rx_diglpf_default_coeffs[1]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num02, pi->phy_rx_diglpf_default_coeffs[2]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den00, pi->phy_rx_diglpf_default_coeffs[3]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den01, pi->phy_rx_diglpf_default_coeffs[4]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num10, pi->phy_rx_diglpf_default_coeffs[5]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num11, pi->phy_rx_diglpf_default_coeffs[6]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num12, pi->phy_rx_diglpf_default_coeffs[7]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den10, pi->phy_rx_diglpf_default_coeffs[8]);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den11, pi->phy_rx_diglpf_default_coeffs[9]);

		pi->phy_rx_diglpf_default_coeffs_valid = FALSE;
		break;
	case 1:  /* set rx dig lpf to ltrn-lpf mode */

		phy_reg_write(pi, ACPHY_RxStrnFilt40Num00, phy_reg_read(pi, ACPHY_RxFilt40Num00));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num01, phy_reg_read(pi, ACPHY_RxFilt40Num01));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num02, phy_reg_read(pi, ACPHY_RxFilt40Num02));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num10, phy_reg_read(pi, ACPHY_RxFilt40Num10));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num11, phy_reg_read(pi, ACPHY_RxFilt40Num11));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num12, phy_reg_read(pi, ACPHY_RxFilt40Num12));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den00, phy_reg_read(pi, ACPHY_RxFilt40Den00));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den01, phy_reg_read(pi, ACPHY_RxFilt40Den01));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den10, phy_reg_read(pi, ACPHY_RxFilt40Den10));
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den11, phy_reg_read(pi, ACPHY_RxFilt40Den11));

		break;
	case 2:  /* bypass rx dig lpf */
		/* 0x2d4 = sqrt(2) * 512 */
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num00, 0x2d4);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num01, 0);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num02, 0);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den00, 0);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den01, 0);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num10, 0x2d4);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num11, 0);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Num12, 0);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den10, 0);
		phy_reg_write(pi, ACPHY_RxStrnFilt40Den11, 0);

		break;

	default:
		ASSERT((dig_lpf_ht == 2) || (dig_lpf_ht == 1) || (dig_lpf_ht == 0));
		break;
	}
}

/* Setup/Cleanup routine for high-pass corner (HPC) of LPF:
 * 1) Setup: Save LPF config and set HPC to lowest value (0x1)
 * 2) Cleanup: Restore HPC config
 */
void wlc_phy_lpf_hpc_override_acphy(phy_info_t *pi, bool setup_not_cleanup)
{
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	acphy_lpfCT_phyregs_t *porig = &(pi_ac->ac_lpfCT_phyregs_orig);
	uint8 core;
	uint16 tmp_tia_hpc, tmp_lpf_dc_bw;
	uint16 val_tia_hpc, val_lpf_dc_bw;
	uint8 stall_val;

	if (setup_not_cleanup) {

		ASSERT(!porig->is_orig);
		porig->is_orig = TRUE;
		stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
		ACPHY_DISABLE_STALL(pi)

		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x122, 16, &tmp_tia_hpc);
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 1, 0x125, 16, &tmp_lpf_dc_bw);

		ACPHY_ENABLE_STALL(pi, stall_val)

		FOREACH_CORE(pi, core) {
			porig->RfctrlOverrideLpfCT[core] =
				phy_reg_read(pi, ACPHY_RfctrlOverrideLpfCT(core));
			porig->RfctrlCoreLpfCT[core] =
				phy_reg_read(pi, ACPHY_RfctrlCoreLpfCT(core));

			val_tia_hpc = (tmp_tia_hpc >> (core * 4)) & 0xf;
			val_lpf_dc_bw = (tmp_lpf_dc_bw >> (core * 4)) & 0xf;

			MOD_PHYREGCE(pi, RfctrlOverrideLpfCT, core, tia_HPC,  1);
			MOD_PHYREGCE(pi, RfctrlCoreLpfCT,     core, tia_HPC, val_tia_hpc);

			MOD_PHYREGCE(pi, RfctrlOverrideLpfCT, core, lpf_dc_bw, 1);
			MOD_PHYREGCE(pi, RfctrlCoreLpfCT,     core, lpf_dc_bw, val_lpf_dc_bw);
		}
	} else {

		ASSERT(porig->is_orig);
		porig->is_orig = FALSE;

		FOREACH_CORE(pi, core) {
			phy_reg_write(pi, ACPHY_RfctrlOverrideLpfCT(core),
				porig->RfctrlOverrideLpfCT[core]);
			phy_reg_write(pi, ACPHY_RfctrlCoreLpfCT(core),
				porig->RfctrlCoreLpfCT[core]);
		}
	}
}

static void
wlc_phy_bt_on_gpio4_acphy(phy_info_t *pi)
{
	uint16 mask = 0x10;    /* gpio 4 = 0 */

	/* Force gpio4 to be 0 */
	si_gpioout(pi->sh->sih, (1 << 4), (0 << 4), GPIO_DRV_PRIORITY);
	si_gpioouten(pi->sh->sih, (1 << 4), (1 << 4), GPIO_DRV_PRIORITY);

	/* Take away gpio4 contorl from phy */
	si_gpiocontrol(pi->sh->sih, mask, 0, GPIO_DRV_PRIORITY);
}

static void
wlc_phy_compute_rssi_gainerror_acphy(phy_info_t *pi)
{
	int16 gainerr[PHY_CORE_MAX];
	int16 initgain_dB[PHY_CORE_MAX];
	int16 rxiqest_gain;
	uint8 core;
	bool srom_isempty = FALSE;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	/* Retrieve rxiqest gain error: */
	srom_isempty = wlc_phy_get_rxgainerr_phy(pi, gainerr);

	if (srom_isempty) {
		FOREACH_CORE(pi, core) {
			pi->phy_rssi_gain_error[core] = 0;
		}
		return;
	}

	/* Retrieve initgains in dB */
	wlc_phy_get_initgain_dB_acphy(pi, initgain_dB);

	/* Compute correction */
	FOREACH_CORE(pi, core) {
		int16 tmp;
		/* Retrieve rxiqest gain: */
		if (pi_ac->srom.femctrl == 2) {
		  if (CHSPEC_IS2G(pi->radio_chanspec)) {
			rxiqest_gain = (int16)(ACPHY_NOISE_INITGAIN_X29_2G);
		  } else {
			rxiqest_gain = (int16)(ACPHY_NOISE_INITGAIN_X29_5G);
		  }
		} else {
		rxiqest_gain = (int16)(ACPHY_NOISE_INITGAIN);
		}
		/* gainerr is in 0.5dB steps; round to nearest dB */
		tmp = gainerr[core];
		tmp = ((tmp >= 0) ? ((tmp + 1) >> 1) : -1*((-1*tmp + 1) >> 1));
		/* report rssi gainerr in 0.5dB steps */
		pi->phy_rssi_gain_error[core] =
			(int8)((rxiqest_gain << 1) - (initgain_dB[core] << 1) + gainerr[core]);
	}
}

static void
wlc_phy_get_initgain_dB_acphy(phy_info_t *pi, int16 *initgain_dB)
{
	uint16 initgain_code[PHY_CORE_MAX];
	uint8 core;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);
	/* Read initgain code from phy rfseq table */
	wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 3, 0xf9, 16, initgain_code);

	FOREACH_CORE(pi, core) {
	  initgain_dB[core] = wlc_phy_rxgaincode_to_dB_acphy(pi, initgain_code[core], core);
	}

	/* Add extLNA gain if extLNA is present */
	if (((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA) &&
	     (CHSPEC_IS2G(pi->radio_chanspec))) ||
	    ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_SROM11_EXTLNA_5GHz) &&
	     (CHSPEC_IS5G(pi->radio_chanspec)))) {
		uint8 indx;
		int8 gain[PHY_CORE_MAX];

		indx = (READ_PHYREG(pi, ACPHY_Core0InitGainCodeA, initExtLnaIndex) & 0x1);
		wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_GAIN0, 1, (0x0 + indx), 8, &(gain[0]));

		if (!ACREV1X1_IS(pi->pubpi.phy_rev)) {
			indx = (READ_PHYREG(pi, ACPHY_Core1InitGainCodeA, initExtLnaIndex) & 0x1);
			wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_GAIN1, 1, (0x0 + indx),
			          8, &(gain[1]));
			indx = (READ_PHYREG(pi, ACPHY_Core2InitGainCodeA, initExtLnaIndex) & 0x1);
			wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_GAIN2, 1, (0x0 + indx),
			          8, &(gain[2]));
		}

		FOREACH_CORE(pi, core) {
			initgain_dB[core] += gain[core];
		}
	}
	ACPHY_ENABLE_STALL(pi, stall_val);
}

static int16
wlc_phy_rxgaincode_to_dB_acphy(phy_info_t *pi, uint16 gain_code, uint8 core)
{
	int8 lna1_code, lna2_code, mixtia_code, biq0_code, biq1_code;
	int8 lna1_gain, lna2_gain, mixtia_gain, biq0_gain, biq1_gain;
	uint16 TR_loss;
	int16 total_gain;
	uint16 gain_tblid;

	/* Extract gain codes for each gain element from overall gain code: */
	lna1_code = gain_code & 0x7;
	lna2_code = (gain_code >> 3) & 0x7;
	mixtia_code = (gain_code >> 6) & 0xf;
	biq0_code = (gain_code >> 10) & 0x7;
	biq1_code = (gain_code >> 13) & 0x7;
	if (core == 0) {
		gain_tblid =  ACPHY_TBL_ID_GAIN0;
	} else if (core == 1) {
		gain_tblid =  ACPHY_TBL_ID_GAIN1;
	} else {
		gain_tblid =  ACPHY_TBL_ID_GAIN2;
	}

	/* Look up gains for lna1, lna2 and mixtia from indices: */
	wlc_phy_table_read_acphy(pi, gain_tblid, 1, (0x8 + lna1_code), 8, &lna1_gain);
	wlc_phy_table_read_acphy(pi, gain_tblid, 1, (0x10 + lna2_code), 8, &lna2_gain);
	wlc_phy_table_read_acphy(pi, gain_tblid, 1, (0x20 + mixtia_code), 8, &mixtia_gain);

	/* Biquad gains: */
	biq0_gain = 3 * biq0_code;
	biq1_gain = 3 * biq1_code;

	/* Need to subtract out TR_loss in Rx mode: */
	TR_loss = (phy_reg_read(pi, ACPHY_TRLossValue)) & 0x7f;
	/* Total gain: */
	total_gain = lna1_gain + lna2_gain + mixtia_gain + biq0_gain +  biq1_gain - TR_loss;
	return total_gain;
}

uint8 wlc_phy_calc_extra_init_gain_acphy(phy_info_t *pi, uint8 extra_gain_3dB,
 rxgain_t rxgain[])
{
	uint16 init_gain_code[4];
	uint8 core, MAX_DVGA, MAX_LPF, MAX_MIX;
	uint8 dvga, mix, lpf0, lpf1;
	uint8 dvga_inc, lpf0_inc, lpf1_inc;
	uint8 max_inc, gain_ticks = extra_gain_3dB;
	uint8 stall_val;
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi)

	MAX_DVGA = 4; MAX_LPF = 10; MAX_MIX = 4;
	wlc_phy_table_read_acphy(pi, ACPHY_TBL_ID_RFSEQ, 3, 0xf9, 16, &init_gain_code);
	ACPHY_ENABLE_STALL(pi, stall_val);
	/* Find if the requested gain increase is possible */
	FOREACH_CORE(pi, core) {
		dvga = 0;
		mix = (init_gain_code[core] >> 6) & 0xf;
		lpf0 = (init_gain_code[core] >> 10) & 0x7;
		lpf1 = (init_gain_code[core] >> 13) & 0x7;
		max_inc = MAX(0, MAX_DVGA - dvga) + MAX(0, MAX_LPF - lpf0 - lpf1) +
		        MAX(0, MAX_MIX - mix);
		gain_ticks = MIN(gain_ticks, max_inc);
	}
	if (gain_ticks != extra_gain_3dB) {
		PHY_INFORM(("%s: Unable to find enough extra gain. Using extra_gain = %d\n",
		            __FUNCTION__, 3 * gain_ticks));
	}
		/* Do nothing if no gain increase is required/possible */
	if (gain_ticks == 0) {
		return gain_ticks;
	}
	/* Find the mix, lpf0, lpf1 gains required for extra INITgain */
	FOREACH_CORE(pi, core) {
		uint8 gain_inc = gain_ticks;
		dvga = 0;
		mix = (init_gain_code[core] >> 6) & 0xf;
		lpf0 = (init_gain_code[core] >> 10) & 0x7;
		lpf1 = (init_gain_code[core] >> 13) & 0x7;
		dvga_inc = MIN((uint8) MAX(0, MAX_DVGA - dvga), gain_inc);
		dvga += dvga_inc;
		gain_inc -= dvga_inc;
		lpf1_inc = MIN((uint8) MAX(0, MAX_LPF - lpf1 - lpf0), gain_inc);
		lpf1 += lpf1_inc;
		gain_inc -= lpf1_inc;
		lpf0_inc = MIN((uint8) MAX(0, MAX_LPF - lpf1 - lpf0), gain_inc);
		lpf0 += lpf0_inc;
		gain_inc -= lpf0_inc;
		mix += MIN((uint8) MAX(0, MAX_MIX - mix), gain_inc);
		rxgain[core].lna1 = init_gain_code[core] & 0x7;
		rxgain[core].lna2 = (init_gain_code[core] >> 3) & 0x7;
		rxgain[core].mix  = mix;
		rxgain[core].lpf0 = lpf0;
		rxgain[core].lpf1 = lpf1;
		rxgain[core].dvga = dvga;
	}
	return gain_ticks;
}


static void
wlc_phy_set_bt_on_core1_acphy(phy_info_t *pi, uint8 bt_fem_val)
{
	/* *** NOTE : For boards with BT on sharead antenna, update code in
	   wlc_bmac_set_ctrl_bt_shd0() so that in down mode BT has control of fem
	   Also, BT needs control when insmod (but not up), in that case wlc_phy_ac.c
	   is not even called, and so need to have some code in wlc_bmac.
	*/

	MOD_PHYREG(pi, ACPHY_BT_SwControl, bt_sharing_en, 1);   /* chip_bandsel = bandsel */
	phy_reg_write(pi, ACPHY_shFemMuxCtrl, 0x555); /* Bring c1_2g ctrls on gpio/srmclk */

	/* Setup chipcontrol (chipc[3] for acphy_gpios) */
	si_corereg(pi->sh->sih, SI_CC_IDX, OFFSETOF(chipcregs_t, chipcontrol),
		CCTRL4360_BTSWCTRL_MODE, CCTRL4360_BTSWCTRL_MODE);

	/* point fem_bt to chip_bt control line */
	MOD_PHYREG(pi, ACPHY_BT_FemControl, bt_fem, bt_fem_val);

	if (pi->pubpi.phy_rev == 0) {
		/* PHY controls bits 5,6,7 of gpio for BT boards (only needed for A0) */
		si_gpiocontrol(pi->sh->sih, 0xffff, 0x00e0, GPIO_DRV_PRIORITY);

		/* acphy_gpios = mux(bt_fem, femctrl[7:4]) */
		wlc_phy_gpiosel_acphy(pi, 0xb, 0);
		pi->u.pi_acphy->poll_adc_WAR = TRUE;

		/* 4360A0 : Force in WLAN mode, as A0 does not have inv_btcx_prisel bit,
		   and we have to change top level MAC definition of prisel (too complicated)
		   We are not supporting BT on 4360A0 anyway
		*/
		MOD_PHYREG(pi, ACPHY_BT_FemControl, bt_en, 0);
		MOD_PHYREG(pi, ACPHY_BT_FemControl, bt_en_ovrd, 1);
	} else {
		pi->u.pi_acphy->poll_adc_WAR = FALSE;

		/* For MAC prisel = 0 means BT, and 1 means WLAN. PHY code assumes opp behavior */
		MOD_PHYREG(pi, ACPHY_BT_SwControl, inv_btcx_prisel, 1);
	}
}


/**********  DESENSE : ACI, NOISE, BT (start)  ******** */

/*********** Desnese (geneal) ********** */
/* IMP NOTE: make sure whatever regs are chagned here are either:
1. reset back to defualt below OR
2. Updated in gainctrl()
*/
static void
wlc_phy_desense_apply_acphy(phy_info_t *pi, bool apply_desense)
{
	/* reset:
	   1 --> clear aci settings (the ones that gainctrl does not clear)
	   0 --> apply aci_noise_bt mitigation
	*/
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	acphy_desense_values_t *desense;
	uint8 ofdm_desense, bphy_desense, initgain_desense;
	uint8 crsmin_desense, crsmin_thresh;
	uint8 bphy_minshiftbits[] = {0x77, 0x02, 0x03, 0x03, 0x03, 0x04, 0x04, 0x04, 0x04, 0x04,
	     0x04, 0x04, 0x04};
	uint16 bphy_peakenergy[]  = {0x10, 0x60, 0x10, 0x4c, 0x60, 0x30, 0x40, 0x40, 0x38, 0x2e,
	     0x40, 0x34, 0x40};
	uint8 bphy_initgain_backoff[] = {0, 0, 0, 0, 0, 0, 0, 3, 6, 9, 9, 12, 12};
	uint8 max_bphy_shiftbits = sizeof(bphy_minshiftbits) / sizeof(uint8);

	uint8 max_initgain_desense = 12;   /* only desnese bq0 */
	uint8 core, bphy_idx = 0, stall_val;
	bool call_gainctrl = FALSE;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	stall_val = READ_PHYREG(pi, ACPHY_RxFeCtrl1, disable_stalls);
	ACPHY_DISABLE_STALL(pi);

	if (!apply_desense) {
		/* when channel is changed, and the current channel is in mitigatation, then
		   we need to restore the values. wlc_phy_rxgainctrl_gainctrl_acphy() takes
		   care of all the gainctrl part, but we need to still restore back bphy regs
		*/

		wlc_phy_set_crs_min_pwr_acphy(pi, 0, 0, 0);

		wlc_phy_desense_mf_high_thresh_acphy(pi, FALSE);
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			phy_reg_write(pi, ACPHY_DigiGainLimit0, 0x4477);
			phy_reg_write(pi, ACPHY_PeakEnergyL, 0x10);
		}

		wlc_phy_desense_print_phyregs_acphy(pi, "restore");
	} else {
		/* Get total desense based on aci & bt */
		wlc_phy_desense_calc_total_acphy(pi);
		desense = &pi_ac->total_desense;

		ofdm_desense = MIN(ACPHY_ACI_MAX_DESENSE_DB, desense->ofdm_desense);
		bphy_desense = MIN(ACPHY_ACI_MAX_DESENSE_DB, desense->bphy_desense);

		/* Update current desense */
		pi_ac->curr_desense.ofdm_desense = ofdm_desense;
		pi_ac->curr_desense.bphy_desense = bphy_desense;

		/* if any ofdm desense is needed, first start using higher
		   mf thresholds (1dB sens loss)
		*/
		wlc_phy_desense_mf_high_thresh_acphy(pi, (ofdm_desense > 0));
		if (ofdm_desense > 0)
			ofdm_desense -= 1;

		/* Distribute desense between INITgain & crsmin(ofdm) & digigain(bphy) */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			/* round to 2, as bphy desnese table is in 2dB steps */
			bphy_idx = MIN((bphy_desense + 1) >> 1, max_bphy_shiftbits - 1);
			initgain_desense = bphy_initgain_backoff[bphy_idx];
		} else {
			initgain_desense = ofdm_desense;
		}

		initgain_desense = MIN(initgain_desense, max_initgain_desense);
		crsmin_desense = MAX(0, ofdm_desense - initgain_desense);

		PHY_ACI(("aci_debug, desense, init = %d, bphy_idx = %d, crsmin = %d\n",
		         initgain_desense, bphy_idx, crsmin_desense));

		if (pi_ac->curr_desense.elna_bypass != desense->elna_bypass)
			call_gainctrl = TRUE;

		/* update lna1 tables if limit changed OR elna bypassed */
		if ((pi_ac->curr_desense.elna_bypass != desense->elna_bypass) ||
		    (pi_ac->curr_desense.lna1_tbl_desense != desense->lna1_tbl_desense)) {
			wlc_phy_upd_lna1_lna2_gaintbls_acphy(pi, 1);
			call_gainctrl = TRUE;
		}

		/* update lna2 tables if limit changed OR elna bypassed */
		if ((pi_ac->curr_desense.elna_bypass != desense->elna_bypass) ||
		    (pi_ac->curr_desense.lna2_tbl_desense != desense->lna2_tbl_desense)) {
			wlc_phy_upd_lna1_lna2_gaintbls_acphy(pi, 2);
			call_gainctrl = TRUE;
		}

		/* update lna1 gainlimit tables if limit changed */
		if (pi_ac->curr_desense.lna1_gainlmt_desense != desense->lna1_gainlmt_desense) {
			wlc_phy_upd_lna1_lna2_gainlimittbls_acphy(pi, 1);
			call_gainctrl = TRUE;
		}

		/* update lna1 gainlimit tables if limit changed */
		if (pi_ac->curr_desense.lna2_gainlmt_desense != desense->lna2_gainlmt_desense) {
			wlc_phy_upd_lna1_lna2_gainlimittbls_acphy(pi, 2);
			call_gainctrl = TRUE;
		}

		/* if lna1/lna2 gaintable has changed, call gainctrl as it effects all clip gains */
		if (call_gainctrl)
			wlc_phy_rxgainctrl_gainctrl_acphy(pi);

		/* Update INITgain */
		FOREACH_CORE(pi, core) {
			wlc_phy_rxgainctrl_set_init_clip_gain_acphy(pi, 0,
			                                            ACPHY_INIT_GAIN
			                                            - initgain_desense,
			                                            desense->elna_bypass, core);
		}

		/* adjust crsmin threshold, 8 ticks increase gives 3dB rejection */
		crsmin_thresh = ACPHY_CRSMIN_DEFAULT + ((88 * crsmin_desense) >> 5);
		wlc_phy_set_crs_min_pwr_acphy(pi, crsmin_thresh, 0, 0);

		/* bphy desense */
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			phy_reg_write(pi, ACPHY_DigiGainLimit0,
			              0x4400 | bphy_minshiftbits[bphy_idx]);
			phy_reg_write(pi, ACPHY_PeakEnergyL, bphy_peakenergy[bphy_idx]);
		}
		wlc_phy_desense_print_phyregs_acphy(pi, "apply");
	}

	ACPHY_ENABLE_STALL(pi, stall_val);
	wlapi_enable_mac(pi->sh->physhim);
}

static void
wlc_phy_desense_calc_total_acphy(phy_info_t *pi)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	acphy_desense_values_t *bt, *aci;
	acphy_desense_values_t *total = &pi_ac->total_desense;

	aci = (pi_ac->aci == NULL) ? &pi_ac->zero_desense : &pi_ac->aci->desense;

	if (pi_ac->btc_mode == 0 || wlc_phy_is_scan_chan_acphy(pi) ||
	    CHSPEC_IS5G(pi->radio_chanspec)) {
		/* only consider aci desense */
		memcpy(total, aci, sizeof(acphy_desense_values_t));
	} else {
		/* Merge BT & ACI desense, take max */
		bt  = &pi_ac->bt_desense;
		total->ofdm_desense = MAX(aci->ofdm_desense, bt->ofdm_desense);
		total->bphy_desense = MAX(aci->bphy_desense, bt->bphy_desense);
		total->lna1_tbl_desense = MAX(aci->lna1_tbl_desense, bt->lna1_tbl_desense);
		total->lna2_tbl_desense = MAX(aci->lna2_tbl_desense, bt->lna2_tbl_desense);
		total->lna1_gainlmt_desense =
		        MAX(aci->lna1_gainlmt_desense, bt->lna1_gainlmt_desense);
		total->lna2_gainlmt_desense =
		        MAX(aci->lna2_gainlmt_desense, bt->lna2_gainlmt_desense);
		total->elna_bypass = MAX(aci->elna_bypass, bt->elna_bypass);
		total->nf_hit_lna12 =  MAX(aci->nf_hit_lna12, bt->nf_hit_lna12);
		total->on = aci->on | bt->on;
	}
}

/*
Default - High MF thresholds are used only if pktgain < 81dB.
To always use high mf thresholds, change this to 98dBs
*/
static void
wlc_phy_desense_mf_high_thresh_acphy(phy_info_t *pi, bool on)
{
	uint16 val;

	val = on ? 0x5f62 : 0x4e51;
	phy_reg_write(pi, ACPHY_crshighlowpowThresholdl, val);
	phy_reg_write(pi, ACPHY_crshighlowpowThresholdu, val);
	phy_reg_write(pi, ACPHY_crshighlowpowThresholdlSub1, val);
	phy_reg_write(pi, ACPHY_crshighlowpowThresholduSub1, val);
}


/********** DESENSE (ACI, CCI, Noise - glitch based) ******** */
void
wlc_phy_desense_aci_reset_params_acphy(phy_info_t *pi, bool call_gainctrl)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	if (pi_ac->aci == NULL) return;

	bzero(&pi_ac->aci->bphy_hist, sizeof(desense_history_t));
	bzero(&pi_ac->aci->ofdm_hist, sizeof(desense_history_t));
	pi_ac->aci->glitch_buff_idx = 0;
	pi_ac->aci->glitch_upd_wait = 1;
	bzero(&pi_ac->aci->desense, sizeof(acphy_desense_values_t));

	/* Call gainctrl to reset all the phy regs */
	if (call_gainctrl)
		wlc_phy_desense_apply_acphy(pi, TRUE);
}

static acphy_aci_params_t*
wlc_phy_desense_aci_getset_chanidx_acphy(phy_info_t *pi, chanspec_t chanspec, bool create)
{
	uint8 idx, oldest_idx;
	uint64 oldest_time;
	acphy_aci_params_t *ret = NULL;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;

	/* Find if this chan/bw already exists */
	for (idx = 0; idx < ACPHY_ACI_CHAN_LIST_SZ; idx++) {
		if ((pi_ac->aci_list[idx].chan == CHSPEC_CHANNEL(chanspec)) &&
		    (pi_ac->aci_list[idx].bw == CHSPEC_BW(chanspec))) {
			ret = &pi_ac->aci_list[idx];
			PHY_ACI(("aci_debug. *** old_chan. idx = %d, chan = %d, bw = %d\n",
			         idx, ret->chan, ret->bw));
			break;
		}
	}

	/* If doesn't exist & don't want to create one */
	if ((ret == NULL) && !create) return ret;

	if (ret == NULL) {
		/* Chan/BW does not exist on in the list of ACI channels.
		   Create a new one (based on oldest timestamp)
		*/
		oldest_idx = 0; oldest_time = pi_ac->aci_list[oldest_idx].last_updated;
		for (idx = 1; idx < ACPHY_ACI_CHAN_LIST_SZ; idx++) {
			if (pi_ac->aci_list[idx].last_updated < oldest_time) {
				oldest_time = pi_ac->aci_list[idx].last_updated;
				oldest_idx = idx;
			}
		}

		/* Clear the new aciinfo data */
		ret = &pi_ac->aci_list[oldest_idx];
		bzero(ret, sizeof(acphy_aci_params_t));
		ret->chan =  CHSPEC_CHANNEL(pi->radio_chanspec);
		ret->bw = pi->bw;
		PHY_ACI(("aci_debug, *** new_chan = %d %d, idx = %d\n",
		         CHSPEC_CHANNEL(pi->radio_chanspec), pi->bw, oldest_idx));
	}

	/* Only if the request came for creation */
	if (create) {
		ret->glitch_upd_wait = 2;
		ret->last_updated = wlc_phy_get_time_usec(pi);
	}

	return ret;
}


void
wlc_phy_desense_aci_engine_acphy(phy_info_t *pi)
{
	uint8 ma_idx, i, glitch_idx;
	bool call_mitigation = FALSE;
	uint16 total_glitches, ofdm_glitches;
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	uint16 avg_glitch_ofdm, avg_glitch_bphy;
	uint8 new_bphy_desense, new_ofdm_desense;
	acphy_desense_values_t *desense;
	acphy_aci_params_t *aci;

	if (pi_ac->aci == NULL)
		pi_ac->aci = wlc_phy_desense_aci_getset_chanidx_acphy(pi, pi->radio_chanspec, TRUE);

	aci = pi_ac->aci;
	desense = &aci->desense;

	if (aci->glitch_upd_wait > 0) {
		aci->glitch_upd_wait--;
		return;
	}

	/* Total glitches */
	ma_idx = (pi->interf.aci.ma_index == 0) ? MA_WINDOW_SZ - 1 : pi->interf.aci.ma_index - 1;
	total_glitches =  pi->interf.aci.ma_list[ma_idx];

	/* Ofdm glitches */
	ma_idx = (pi->interf.noise.ofdm_ma_index == 0) ? PHY_NOISE_MA_WINDOW_SZ - 1 :
	        pi->interf.noise.ofdm_ma_index - 1;
	ofdm_glitches =  pi->interf.noise.ofdm_glitch_ma_list[ma_idx];

	/* Invalid glitch value, discard */
	if ((total_glitches >= 32768) || (ofdm_glitches >= 32768) ||
	    (total_glitches < ofdm_glitches))
		return;

	/* Update glitch history */
	glitch_idx = aci->glitch_buff_idx;
	aci->ofdm_hist.glitches[glitch_idx] = ofdm_glitches;
	aci->bphy_hist.glitches[glitch_idx] = total_glitches - ofdm_glitches;
	aci->glitch_buff_idx = (glitch_idx + 1) % ACPHY_ACI_GLITCH_BUFFER_SZ;

	PHY_ACI(("aci_debug, bphy idx = %d, ", glitch_idx));
	for (i = 0; i < ACPHY_ACI_GLITCH_BUFFER_SZ; i++)
		PHY_ACI(("%d ", aci->bphy_hist.glitches[i]));
	PHY_ACI(("\n"));

	PHY_ACI(("aci_debug, ofdm idx = %d, ", glitch_idx));
	for (i = 0; i < ACPHY_ACI_GLITCH_BUFFER_SZ; i++)
		PHY_ACI(("%d ", aci->ofdm_hist.glitches[i]));
	PHY_ACI(("\n"));

	/* Find AVG of Max glitches in last N seconds */
	avg_glitch_bphy =
	        wlc_phy_desense_aci_get_avg_max_glitches_acphy(aci->bphy_hist.glitches);
	avg_glitch_ofdm =
	        wlc_phy_desense_aci_get_avg_max_glitches_acphy(aci->ofdm_hist.glitches);

	PHY_ACI(("aci_debug, max {bphy, ofdm} = {%d %d}, rssi = %d, aci_on = %d\n",
	         avg_glitch_bphy, avg_glitch_ofdm, aci->weakest_rssi, desense->on));

	/* Don't need to do anything is interference mitigation is off & glitches < thresh */
	if (!(desense->on || (avg_glitch_bphy > ACPHY_ACI_BPHY_HI_GLITCH_THRESH) ||
	      (avg_glitch_ofdm > ACPHY_ACI_OFDM_HI_GLITCH_THRESH)))
		return;

	new_bphy_desense = wlc_phy_desense_aci_calc_acphy(pi, &aci->bphy_hist,
	                                             desense->bphy_desense,
	                                             avg_glitch_bphy,
	                                             ACPHY_ACI_BPHY_LO_GLITCH_THRESH,
	                                             ACPHY_ACI_BPHY_HI_GLITCH_THRESH);

	new_ofdm_desense = wlc_phy_desense_aci_calc_acphy(pi, &aci->ofdm_hist,
	                                             desense->ofdm_desense,
	                                             avg_glitch_ofdm,
	                                             ACPHY_ACI_OFDM_LO_GLITCH_THRESH,
	                                             ACPHY_ACI_OFDM_HI_GLITCH_THRESH);

	/* Limit desnese */
	new_bphy_desense = MIN(new_bphy_desense, ACPHY_ACI_MAX_DESENSE_DB);
	new_ofdm_desense = MIN(new_ofdm_desense, ACPHY_ACI_MAX_DESENSE_DB);
	if (pi_ac->limit_desense_on_rssi) {
		new_bphy_desense = MIN(new_bphy_desense, MAX(0, aci->weakest_rssi + 90));
		new_ofdm_desense = MIN(new_ofdm_desense, MAX(0, aci->weakest_rssi + 85));
	}

	PHY_ACI(("aci_debug, old desense = {%d %d}, new = {%d %d}\n",
	         desense->bphy_desense,
	         desense->ofdm_desense,
	         new_bphy_desense, new_ofdm_desense));

	if (new_bphy_desense != desense->bphy_desense) {
		call_mitigation = TRUE;
		desense->bphy_desense = new_bphy_desense;

		/* Clear old glitch history when desnese changed */
		for (i = 0; i <  ACPHY_ACI_GLITCH_BUFFER_SZ; i++)
			aci->bphy_hist.glitches[i] = ACPHY_ACI_BPHY_LO_GLITCH_THRESH;
	}

	if (new_ofdm_desense != desense->ofdm_desense) {
		call_mitigation = TRUE;
		desense->ofdm_desense = new_ofdm_desense;

		/* Clear old glitch history when desnese changed */
		for (i = 0; i <  ACPHY_ACI_GLITCH_BUFFER_SZ; i++)
			aci->ofdm_hist.glitches[i] = ACPHY_ACI_OFDM_LO_GLITCH_THRESH;
	}

	desense->on = FALSE;
	desense->on = (memcmp(&pi_ac->zero_desense, desense, sizeof(acphy_desense_values_t)) != 0);

	if (call_mitigation) {
		PHY_ACI(("aci_debug : desense = %d %d\n",
		         desense->bphy_desense, desense->ofdm_desense));

		wlc_phy_desense_apply_acphy(pi, TRUE);

		/* After gain change, it takes a while for updated glitches to show up */
		aci->glitch_upd_wait = ACPHY_ACI_WAIT_POST_MITIGATION;
	}
}

static uint16
wlc_phy_desense_aci_get_avg_max_glitches_acphy(uint16 glitches[])
{
	uint8 i, j;
	uint16 max_glitch, glitch_cnt = 0;
	uint8 max_glitch_idx;
	uint16 glitches_sort[ACPHY_ACI_GLITCH_BUFFER_SZ];

	for (i = 0; i < ACPHY_ACI_GLITCH_BUFFER_SZ; i++)
		glitches_sort[i] = glitches[i];

	/* Get 2 max from the list */
	for (j = 0; j < ACPHY_ACI_NUM_MAX_GLITCH_AVG; j++) {
		max_glitch_idx = 0;
		max_glitch = glitches_sort[0];
		for (i = 1; i <  ACPHY_ACI_GLITCH_BUFFER_SZ; i++) {
			if (glitches_sort[i] > max_glitch) {
				max_glitch_idx = i;
				max_glitch = glitches_sort[i];
			}
		}
		glitches_sort[max_glitch_idx] = 0;
		glitch_cnt += max_glitch;
	}

	/* avg */
	glitch_cnt /= ACPHY_ACI_NUM_MAX_GLITCH_AVG;
	return glitch_cnt;
}


static uint8
wlc_phy_desense_aci_calc_acphy(phy_info_t *pi, desense_history_t *aci_desense, uint8 desense,
                          uint16 glitch_cnt, uint16 glitch_th_lo, uint16 glitch_th_hi)
{
	uint8 hi, lo, cnt, cnt_thresh = 0;

	hi = aci_desense->hi_glitch_dB;
	lo = aci_desense->lo_glitch_dB;
	cnt = aci_desense->no_desense_change_time_cnt;

	if (glitch_cnt > glitch_th_hi) {
		hi = desense;
		if (hi >= lo) desense += ACPHY_ACI_COARSE_DESENSE_UP;
		else desense = MAX(desense + 1, (hi + lo) >> 1);
		cnt = 0;
	} else {
		/* Sleep for different times under different conditions */
		if (glitch_cnt >= glitch_th_lo)
			cnt_thresh = ACPHY_ACI_MD_GLITCH_SLEEP;
		else
			cnt_thresh = (desense - hi == 1) ? ACPHY_ACI_BORDER_GLITCH_SLEEP :
			        ACPHY_ACI_LO_GLITCH_SLEEP;
		if (cnt > cnt_thresh) {
			lo = desense;
			if (lo <= hi) desense = MAX(0, desense - ACPHY_ACI_COARSE_DESENSE_DN);
			else desense = MAX(0, MIN(desense - 1, (hi + lo) >> 1));
			cnt = 0;
		}
	}

	PHY_ACI(("aci_debug, lo = %d, hi = %d, desense = %d, cnt = %d(%d)\n",  lo, hi, desense,
	         cnt, cnt_thresh));

	/* Update the values */
	aci_desense->hi_glitch_dB = hi;
	aci_desense->lo_glitch_dB = lo;
	aci_desense->no_desense_change_time_cnt = MIN(cnt + 1, 255);

	return desense;
}

/* Update chan stats offline, i.e. we might not be on this channel currently */
void
wlc_phy_desense_aci_upd_chan_stats_acphy(phy_info_t *pi, chanspec_t chanspec, int8 rssi)
{
	acphy_aci_params_t *aci;

	aci = (acphy_aci_params_t *)
	        wlc_phy_desense_aci_getset_chanidx_acphy(pi, chanspec, FALSE);

	if (aci == NULL) return;   /* not found in phy list of channels */

	aci->weakest_rssi = rssi;
}


/********** Desnese BT  ******** */
void
wlc_phy_desense_btcoex_acphy(phy_info_t *pi, int32 mode)
{
	phy_info_acphy_t *pi_ac = pi->u.pi_acphy;
	acphy_desense_values_t *desense = &pi_ac->bt_desense;
	int32 old_mode = pi_ac->btc_mode;

	/* Start with everything at 0 */
	bzero(desense, sizeof(acphy_desense_values_t));
	pi_ac->btc_mode = mode;
	desense->on = (mode > 0);

	switch (mode) {
	case 1: /* BT power =  -30dBm, -35dBm */
		desense->lna1_gainlmt_desense = 1;   /* 4 */
		desense->lna2_gainlmt_desense = 3;   /* 3 */
		desense->elna_bypass = 0;
		break;
	case 2: /* BT power = -20dBm , -25dB */
		desense->lna1_gainlmt_desense = 0;   /* 5 */
		desense->lna2_gainlmt_desense = 0;   /* 6 */
		desense->elna_bypass = 1;
		break;
	case 3: /* BT power = -15dBm */
		desense->lna1_gainlmt_desense = 0;   /* 5 */
		desense->lna2_gainlmt_desense = 2;   /* 4 */
		desense->elna_bypass = 1;
		desense->nf_hit_lna12 = 2;
		break;
	case 4: /* BT power = -10dBm */
		desense->lna1_gainlmt_desense = 1;   /* 4 */
		desense->lna2_gainlmt_desense = 2;   /* 4 */
		desense->elna_bypass = 1;
		desense->nf_hit_lna12 = 3;
		break;
	case 5: /* BT power = -5dBm */
		desense->lna1_gainlmt_desense = 3;   /* 2 */
		desense->lna2_gainlmt_desense = 0;   /* 6 */
		desense->elna_bypass = 1;
		desense->nf_hit_lna12 = 13;
		break;
	case 6: /* BT power = 0dBm */
		desense->lna1_gainlmt_desense = 3;   /* 2 */
		desense->lna2_gainlmt_desense = 4;   /* 2 */
		desense->elna_bypass = 1;
		desense->nf_hit_lna12 = 24;
		break;
	}

	/* Apply these settings if this is called while on an active 2g channel */
	if (CHSPEC_IS2G(pi->radio_chanspec) && !SCAN_RM_IN_PROGRESS(pi)) {
		/* If bt desense changed, then reset aci params. But, keep the aci settings intact
		   if bt is switched off (as you will still need aci desense)
		*/
		if ((mode != old_mode) && (mode > 0))
			wlc_phy_desense_aci_reset_params_acphy(pi, FALSE);

		wlc_phy_desense_apply_acphy(pi, TRUE);
	}
}

static void
wlc_phy_desense_print_phyregs_acphy(phy_info_t *pi, const char str[])
{
}
/**********  DESENSE : ACI, NOISE, BT (end)  ********** */

static bool
wlc_phy_is_scan_chan_acphy(phy_info_t *pi)
{
	return (SCAN_RM_IN_PROGRESS(pi) &&
	        (pi->interf.curr_home_channel != CHSPEC_CHANNEL(pi->radio_chanspec)));
}

#ifdef SAMPLE_COLLECT
/* channel to frequency conversion */
static int
wlc_phy_chan2fc_acphy(uint channel)
{
	/* go from channel number (such as 6) to carrier freq (such as 2442) */
	if (channel >= 184 && channel <= 228)
		return (channel*5 + 4000);
	else if (channel >= 32 && channel <= 180)
		return (channel*5 + 5000);
	else if (channel >= 1 && channel <= 13)
		return (channel*5 + 2407);
	else if (channel == 14)
		return (2484);
	else
		return -1;
}

static uint16
acphy_words_per_us(phy_info_t *pi, uint16 sd_adc_rate, uint16 mo)
{
	uint16 words_per_us;
	uint16 sampRate;
	words_per_us = sd_adc_rate/4;

	/* For more comments on how "words_per_us"is calculated, check the d11samples.tcl file */

	switch (mo) {
		case SC_MODE_0_sd_adc:

			sampRate = sd_adc_rate;
			switch (pi->bw) {
				case WL_CHANSPEC_BW_20:
					words_per_us = sampRate/2;
					break;
				case WL_CHANSPEC_BW_40:
					words_per_us = sampRate/4;
					break;
				case WL_CHANSPEC_BW_80:
					words_per_us = sampRate/4;
					break;
				default:
					/* Default 20MHz */
					words_per_us = sampRate/2;
					ASSERT(0); /* should never get here */
					break;
			}
			break;

		case SC_MODE_1_sd_adc_5bits:

			switch (pi->bw) {
				case WL_CHANSPEC_BW_20:
					sampRate = sd_adc_rate;
					break;
				case WL_CHANSPEC_BW_40:
					sampRate = sd_adc_rate/2;
					break;
				case WL_CHANSPEC_BW_80:
					sampRate = sd_adc_rate/2;
					break;
				default:
					/* Default 20MHz */
					sampRate = sd_adc_rate;
					ASSERT(0); /* should never get here */
					break;
			}
			words_per_us = sampRate / 2;
			break;

		case SC_MODE_2_cic0:

			sampRate  = sd_adc_rate / 2;

			words_per_us = sampRate;
			break;

		case SC_MODE_3_cic1:

			sampRate  = sd_adc_rate / 4;

			words_per_us = sampRate;

			break;
		case SC_MODE_4s_rx_farrow_1core:

			switch (pi->bw) {
				case WL_CHANSPEC_BW_20:
					sampRate = 20 * 2;
					break;
				case WL_CHANSPEC_BW_40:
					sampRate = 40 * 2;
					break;
				case WL_CHANSPEC_BW_80:
					sampRate = 80 * 2;
					break;
				default:
					/* Default 20MHz */
					sampRate = 20 * 2;
					ASSERT(0); /* should never get here */
					break;
			}
			words_per_us = sampRate;

			break;
		case SC_MODE_4m_rx_farrow:
		case SC_MODE_5_iq_comp:
		case SC_MODE_6_dc_filt:
		case SC_MODE_8_rssi:
		case SC_MODE_9_rssi_all:

			switch (pi->bw) {
				case WL_CHANSPEC_BW_20:
					sampRate = 20 * 2;
					break;
				case WL_CHANSPEC_BW_40:
					sampRate = 40 * 2;
					break;
				case WL_CHANSPEC_BW_80:
					sampRate = 80;
					break;
				default:
					/* Default 20MHz */
					sampRate = 20 * 2;
					ASSERT(0); /* should never get here */
					break;
			}
			words_per_us = sampRate * 2;

			break;
		case SC_MODE_7_rx_filt:

			switch (pi->bw) {
				case WL_CHANSPEC_BW_20:
					sampRate = 20;
					break;
				case WL_CHANSPEC_BW_40:
					sampRate = 40;
					break;
				case WL_CHANSPEC_BW_80:
					sampRate = 80 / 2;
					break;
				default:
					/* Default 20MHz */
					sampRate = 20;
					ASSERT(0); /* should never get here */
					break;
			}
			words_per_us = sampRate * 4;

			break;

		case SC_MODE_10_tx_farrow:
		case SC_MODE_11_gpio:

			switch (pi->bw) {
				case WL_CHANSPEC_BW_20:
					sampRate = 20 * 4;
					break;
				case WL_CHANSPEC_BW_40:
					sampRate = 40 * 4;
					break;
				case WL_CHANSPEC_BW_80:
					sampRate = 80 * 2;
					break;
				default:
					/* Default 20MHz */
					sampRate = 20;
					ASSERT(0); /* should never get here */
					break;
			}
			words_per_us = sampRate;


			break;

		case SC_MODE_12_gpio_trans:

			switch (pi->bw) {
				case WL_CHANSPEC_BW_20:
					sampRate = 20 * 4;
					break;
				case WL_CHANSPEC_BW_40:
					sampRate = 40 * 2;
					break;
				case WL_CHANSPEC_BW_80:
					sampRate = 80 / 2;
					break;
				default:
					/* Default 20MHz */
					sampRate = 20;
					ASSERT(0); /* should never get here */
					break;
			}
			words_per_us = sampRate * 2;

			break;
	}

	return words_per_us;

}

#define FILE_HDR_LEN 20 /* words */
/* (FIFO memory is 176kB = 45056 x 32bit) */

int
wlc_phy_sample_collect_acphy(phy_info_t *pi, wl_samplecollect_args_t *collect, uint32 *buf)
{
	uint32 szbytes;
	uint32 SC_BUFF_LENGTH;
	uint16 sd_adc_rate;
	uint16 save_gpio;
	int coreSel = 0;
	uint32 mo = 0xffff;
	int bitStartVal = 2;
	uint32 phy_ctl, timer, cnt;
	uint16 val;
	uint16 words_per_us = 0;
	uint16 fc = (uint16)wlc_phy_chan2fc_acphy(CHSPEC_CHANNEL(pi->radio_chanspec));
	uint8 core, gpio_collection = 0;
	uint32 *ptr;
	phy_info_acphy_t *pi_ac;
	wl_sampledata_t *sample_data;
	int16 agc_gain[PHY_CORE_MAX];
	bool downSamp = FALSE;
	uint index;

	/* subtract from ADC sample power to obtain true analog power in dBm */
	uint16 dBsample_to_dBm_sub;
	uint8 sample_rate, sample_bitwidth;

	ASSERT(pi);
	pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	if (!pi_ac)
		return BCME_ERROR;

	/* Initializing the agc gain */
	for (index = 0; index < PHY_CORE_MAX; index++) {
		agc_gain[index] = 0;
	}

	/* initial return info pointers */
	sample_data = (wl_sampledata_t *)buf;
	ptr = (uint32 *)&sample_data[1];
	bzero((uint8 *)sample_data, sizeof(wl_sampledata_t));
	sample_data->version = htol16(WL_SAMPLEDATA_T_VERSION);
	sample_data->size = htol16(sizeof(wl_sampledata_t));

	/* get Buffer length length in words */
	szbytes = 512*1024;
	SC_BUFF_LENGTH = szbytes/4;


	if (fc < 5180) {
		/* 2G channel */
		switch (pi->bw) {
			case WL_CHANSPEC_BW_20:
				sd_adc_rate = fc * 3/ 32;
				break;
			case WL_CHANSPEC_BW_40:
				sd_adc_rate = fc * 3/16;
				break;
			case WL_CHANSPEC_BW_80:
				sd_adc_rate = fc * 3/12;
				break;
			default:
				/* Default 20MHz */
				sd_adc_rate = fc * 3/ 32;
				ASSERT(0); /* should never get here */
				break;
		}
	} else {
		/* 5G channel */
		switch (pi->bw) {
			case WL_CHANSPEC_BW_20:
				sd_adc_rate = fc/24;
				break;
			case WL_CHANSPEC_BW_40:
				sd_adc_rate = fc/12;
				break;
			case WL_CHANSPEC_BW_80:
				sd_adc_rate = fc/9;
				break;
			default:
				/* Default 20MHz */
				sd_adc_rate = fc/24;
				ASSERT(0); /* should never get here */
				break;
		}
	}

	phy_reg_write(pi, ACPHY_AdcDataCollect, 0);
	phy_reg_write(pi, ACPHY_RxFeTesMmuxCtrl, 0);

	switch (collect->mode) {
		case SC_MODE_0_sd_adc:

			coreSel = collect->cores;
			if ((coreSel < 0) || (coreSel > 2)) {
				coreSel = 0;
			}

			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, samp_coll_core_sel, coreSel);
			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, rxfe_dbg_mux_sel, 0);

			/* 	Mode
				bits [15:12]  - coreSel
				bits [11:8]   - number of cores (always 1)
				bits [7:0]    - mode id=0
			*/
			mo = ((coreSel << 12) | (1 << 8) | 0);

			/*  	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us  = acphy_words_per_us(pi, sd_adc_rate, SC_MODE_0_sd_adc);

			break;

		case  SC_MODE_1_sd_adc_5bits:

			coreSel = collect->cores;
			if ((coreSel < 0) || (coreSel > 2)) {
				coreSel = 0;
			}

			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, samp_coll_core_sel, coreSel);
			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, rxfe_dbg_mux_sel, 1);

			if (pi->bw == WL_CHANSPEC_BW_40 || pi->bw == WL_CHANSPEC_BW_80) {
				downSamp = 1;
			}

			/* 	Mode
				bits [15:12]  - coreSel
				bits [11:8]   - number of cores (always 1)
				bits [7:0]    - mode id=1
			*/
			mo = ((coreSel << 12) | (1 << 8) | 1);

			/*  	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi, sd_adc_rate, SC_MODE_1_sd_adc_5bits);

			break;

		case  SC_MODE_2_cic0:

			coreSel = collect->cores;
			if ((coreSel < 0) || (coreSel > 2)) {
				coreSel = 0;
			}

			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, samp_coll_core_sel, coreSel);
			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, rxfe_dbg_mux_sel, 2);

			if (pi->bw == WL_CHANSPEC_BW_40 || pi->bw == WL_CHANSPEC_BW_80) {
				downSamp = 1;
			}

			/* 	Mode
				bits [15:12]  - coreSel
				bits [11:8]   - number of cores (always 1)
				bits [7:0]    - mode id=2
			*/
			mo = ((coreSel << 12) | (1 << 8) | 2);

			/*  	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi, sd_adc_rate, SC_MODE_2_cic0);

			break;

		case SC_MODE_3_cic1:

			if (pi->bw == WL_CHANSPEC_BW_80) {
				return BCME_ERROR;
			}

			coreSel = collect->cores;
			if ((coreSel < 0) || (coreSel > 2)) {
				coreSel = 0;
			}

			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, samp_coll_core_sel, coreSel);
			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, rxfe_dbg_mux_sel, 3);

			/*	Mode
				bits [15:12]  - coreSel
				bits [11:8]   - number of cores (always 1)
				bits [7:0]	  - mode id=3
			*/
			mo = ((coreSel << 12) | (1 << 8) | 3);

			/*	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi, sd_adc_rate, SC_MODE_3_cic1);

			break;

		case  SC_MODE_4s_rx_farrow_1core:

			coreSel = collect->cores;
			if ((coreSel < 0) || (coreSel > 2)) {
				coreSel = 0;
			}

			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, samp_coll_core_sel, coreSel);
			MOD_PHYREG(pi, ACPHY_RxFeTesMmuxCtrl, rxfe_dbg_mux_sel, 4);

			/*	Mode
				bits [15:12]  - coreSel
				bits [11:8]   - number of cores (always 1)
				bits [7:0]	  - mode id=4
			*/
			mo = ((coreSel << 12) | (1 << 8) | 4);

			/*	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi, sd_adc_rate,
				SC_MODE_4s_rx_farrow_1core);

			break;

		case SC_MODE_4m_rx_farrow:

			bitStartVal = collect->bitStart;
			if ((bitStartVal < 0) || (bitStartVal > 2)) {
				bitStartVal = 2;
			}

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, bitStart, bitStartVal);
			MOD_PHYREG(pi, ACPHY_AdcDataCollect, sampSel, 4);

			if (pi->bw == WL_CHANSPEC_BW_80) {
				MOD_PHYREG(pi, ACPHY_AdcDataCollect, downSample, 1);
				downSamp = 1;
			}

			/*	Mode
				bits [15:12]  - coreSel
				bits [11:8]   - number of cores (always 1)
				bits [7:0]	  - mode id=4
			*/
			mo = ((coreSel << 12) | (3 << 8) | 4);

			/*	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi, sd_adc_rate, SC_MODE_4m_rx_farrow);

			break;

		case  SC_MODE_5_iq_comp:

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, sampSel, 5);

			if (pi->bw == WL_CHANSPEC_BW_80) {
				MOD_PHYREG(pi, ACPHY_AdcDataCollect, downSample, 1);
				downSamp = 1;
			}

			/* 	Mode
				bits [15:12]  - always 0
				bits [11:8]   - number of cores (always 3)
				bits [7:0]	  - mode id=5
			*/
			mo = ((3 << 8) | 5);

			/* 	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi,  sd_adc_rate, SC_MODE_5_iq_comp);

			break;

		case  SC_MODE_6_dc_filt:

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, sampSel, 6);

			if (pi->bw == WL_CHANSPEC_BW_80) {
				MOD_PHYREG(pi, ACPHY_AdcDataCollect, downSample, 1);
				downSamp = 1;
			}

			/* 	Mode
				bits [15:12]  - always 0
				bits [11:8]   - number of cores (always 3)
				bits [7:0]	  - mode id=6
			*/
			mo = ((3 << 8) | 6);

			/* 	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi,  sd_adc_rate, SC_MODE_6_dc_filt);

			break;

		case  SC_MODE_7_rx_filt:

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, sampSel, 7);

			if (pi->bw == WL_CHANSPEC_BW_80) {
				MOD_PHYREG(pi, ACPHY_AdcDataCollect, downSample, 1);
				downSamp = 1;
			}

			/* 	Mode
				bits [15:12]  - always 0
				bits [11:8]   - number of cores (always 3)
				bits [7:0]	  - mode id=7
			*/
			mo = ((3 << 8) | 7);

			/* 	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi,  sd_adc_rate, SC_MODE_7_rx_filt);

			break;

		case  SC_MODE_8_rssi:

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, sampSel, 8);

			if (pi->bw == WL_CHANSPEC_BW_80) {
				MOD_PHYREG(pi, ACPHY_AdcDataCollect, downSample, 1);
				downSamp = 1;
			}

			/*	Mode
				bits [15:12]  - always 0
				bits [11:8]   - number of cores (always 3)
				bits [7:0]	  - mode id=8
			*/
			mo = ((3 << 8) | 8);

			/*	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi,  sd_adc_rate, SC_MODE_8_rssi);

			break;

		case  SC_MODE_9_rssi_all:

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, sampSel, 9);

			if (pi->bw == WL_CHANSPEC_BW_80) {
				MOD_PHYREG(pi, ACPHY_AdcDataCollect, downSample, 1);
				downSamp = 1;
			}

			/*	Mode
				bits [15:12]  - always 0
				bits [11:8]   - number of cores (always 3)
				bits [7:0]	  - mode id=9
			*/
			mo = ((3 << 8) | 9);

			/*	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi,  sd_adc_rate, SC_MODE_9_rssi_all);
			break;

		case  SC_MODE_10_tx_farrow:

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, sampSel, 10);

			coreSel = collect->cores;
			if ((coreSel < 0) || (coreSel > 2)) {
				coreSel = 0;
			}

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, txCoreSel, coreSel);

			if (pi->bw == WL_CHANSPEC_BW_80) {
				MOD_PHYREG(pi, ACPHY_AdcDataCollect, downSample, 1);
				downSamp = 1;
			}

			/* 	Mode
				bits [15:12]  - Tx coreSel
				bits [11:8]   - number of cores (always 1)
				bits [7:0]	  - mode id=10
			*/
			mo = ((coreSel << 12) | (1 << 8) | 10);

			/* 	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi,  sd_adc_rate,  SC_MODE_10_tx_farrow);

			break;

		case  SC_MODE_11_gpio:

			save_gpio = phy_reg_read(pi, ACPHY_gpioSel);

			/* Writing to lower 8 bits */
			phy_reg_write(pi, ACPHY_gpioSel, ((save_gpio & 0xFF00)| collect->gpio_sel));

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, gpioMode, 0);
			MOD_PHYREG(pi, ACPHY_AdcDataCollect, gpioSel,  1);

			gpio_collection = 1;

			mo = 0xFF;

			/* 	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi,  sd_adc_rate,  SC_MODE_11_gpio);

			break;

		case  SC_MODE_12_gpio_trans:

			save_gpio = phy_reg_read(pi, ACPHY_gpioSel);

			/* Writing to lower 8 bits */
			phy_reg_write(pi, ACPHY_gpioSel, ((save_gpio & 0xFF00)| collect->gpio_sel));

			phy_reg_write(pi, ACPHY_gpioCapMaskHigh, ((collect->gpioCapMask >> 16)
				& 0xFFFF));
			phy_reg_write(pi, ACPHY_gpioCapMaskLow, (collect->gpioCapMask  & 0xFFFF));

			MOD_PHYREG(pi, ACPHY_AdcDataCollect, gpioMode, 1);
			MOD_PHYREG(pi, ACPHY_AdcDataCollect, gpioSel,  1);

			gpio_collection = 1;

			mo = 0x1FF;

			/*	Determine how many words need to be placed in the MAC buffer
				to capture 1us of the signal, in this sample capture mode
			*/
			words_per_us = acphy_words_per_us(pi,  sd_adc_rate,  SC_MODE_12_gpio_trans);

			break;

		default:
			break;
	}

	if (gpio_collection == 0) {
		MOD_PHYREG(pi, ACPHY_AdcDataCollect, adcDataCollectEn, 1);
	}

	/* duration(s): length sanity check and mapping from "max" to usec values */
	if (((collect->pre_dur + collect->post_dur) * words_per_us) > SC_BUFF_LENGTH) {
		PHY_ERROR(("wl%d: %s Error: Bad Duration Option\n", pi->sh->unit, __FUNCTION__));
		return BCME_RANGE;
	}


	/* be deaf if requested (e.g. for spur measurement) */
	if (collect->be_deaf) {
		wlapi_suspend_mac_and_wait(pi->sh->physhim);
		wlc_phy_stay_in_carriersearch_acphy(pi, TRUE);
	}

	/* perform AGC if requested */
	if (collect->agc) {
		/* Backup gain config prior to gain control --- Not beng implemented at the moment.
		wlc_phy_agc_rxgain_config_acphy(pi, TRUE);
		wlc_phy_agc_acphy(pi, agc_gain);
		*/
	} else {
		/* Set reported agc gains to init_gain with gain_error correction */
		int16 gainerr[PHY_CORE_MAX];

		wlc_phy_get_rxgainerr_phy(pi, gainerr);
		FOREACH_CORE(pi, core) {
			/* gainerr is in 0.5dB steps; needs to be rounded to nearest dB */
			int16 tmp = gainerr[core];
			tmp = ((tmp >= 0) ? ((tmp + 1) >> 1) : -1*((-1*tmp + 1) >> 1));
			agc_gain[core] = ACPHY_NOISE_INITGAIN + tmp;
		}
	}

	/* Apply filter settings if requested */
	if (collect->filter) {
		/* Override the LPF high pass corners to their lowest values (0x1) */
		wlc_phy_lpf_hpc_override_acphy(pi, TRUE);
	}

	/* set Tx-FIFO collect start pointer to 0 */
	acphy_set_sc_startptr(pi, 0);


	PHY_TRACE(("wl%d: %s Start capture, trigger = %d\n", pi->sh->unit, __FUNCTION__,
		collect->trigger));

	timer = collect->timeout;

	/* immediate trigger */
	if (collect->trigger == TRIGGER_NOW) {

		/* compute and set stop pointer */
		pi_ac->pstop = (collect->pre_dur + collect->post_dur) * words_per_us;

		if (pi_ac->pstop >= SC_BUFF_LENGTH-1)
			pi_ac->pstop = SC_BUFF_LENGTH-1;


		acphy_set_sc_stopptr(pi, pi_ac->pstop);

		/* set Stop bit and Start bit (start capture) */
		phy_ctl = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param);
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, phy_ctl | (1 << 4) | (1 << 5));

		/* wait until done */
		do {
			OSL_DELAY(10);
			timer--;
		} while (acphy_is_sc_done(pi) != 1);

		/* clear start/stop bits */
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, phy_ctl & 0xFFCF);

		/* set start/stop pointers for readout */
		pi_ac->pfirst = acphy_get_sc_startptr(pi);
		pi_ac->plast = acphy_get_sc_curptr(pi);

	} else {
	        uint32 mac_ctl, dur_1_8th_us;

		/* enable mac and run psm */
		mac_ctl = R_REG(pi->sh->osh, &pi->regs->maccontrol);
		W_REG(pi->sh->osh, &pi->regs->maccontrol, mac_ctl | MCTL_PSM_RUN | MCTL_EN_MAC);


		/* set stop pointer */
		pi_ac->pstop = SC_BUFF_LENGTH-1;

		acphy_set_sc_stopptr(pi, pi_ac->pstop);

		/* set up post-trigger duration (expected by ucode in units of 1/8 us) */
		dur_1_8th_us = collect->post_dur << 3;
		W_REG(pi->sh->osh, &pi->regs->tsf_gpt2_ctr_l, dur_1_8th_us & 0x0000FFFF);
		W_REG(pi->sh->osh, &pi->regs->tsf_gpt2_ctr_h, dur_1_8th_us >> 16);

		/* start ucode trigger-based sample collect procedure */
		wlapi_bmac_write_shm(pi->sh->physhim, D11AC_M_SMPL_COL_BMP, 0x0);

		/* set Start bit (start capture) */
		phy_ctl = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param);
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, phy_ctl | (1 << 4));

		if (ISSIM_ENAB(pi->sh->sih)) {
		  OSL_DELAY(1000*collect->pre_dur);
		} else {
		  OSL_DELAY(collect->pre_dur);
		}

		wlapi_bmac_write_shm(pi->sh->physhim, D11AC_M_SMPL_COL_BMP, collect->trigger);
		wlapi_bmac_write_shm(pi->sh->physhim, D11AC_M_SMPL_COL_CTL, 1);

		PHY_TRACE(("wl%d: %s Wait for trigger ...\n", pi->sh->unit, __FUNCTION__));

		do {
		  OSL_DELAY(10);
		  val = R_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param) & 0x30;
		  timer--;
		} while ((val != 0) && (timer > 0));

		/* set first and last pointer indices for readout */
		pi_ac->plast = acphy_get_sc_curptr(pi);


		if (pi_ac->plast >= (collect->pre_dur + collect->post_dur) * words_per_us) {
		  pi_ac->pfirst =
		    pi_ac->plast - (collect->pre_dur + collect->post_dur)*words_per_us;
		} else {
		  pi_ac->pfirst = (pi_ac->pstop - pi_ac->pstart + 1) +
		    pi_ac->plast - (collect->pre_dur + collect->post_dur)*words_per_us;
		}

		/* restore mac_ctl */
		W_REG(pi->sh->osh, &pi->regs->maccontrol, mac_ctl);

		/* erase trigger setup */
		W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, phy_ctl & 0xFFCF);
		wlapi_bmac_write_shm(pi->sh->physhim, D11AC_M_SMPL_COL_BMP, 0);
	}

	/* CLEAN UP: */
	/* return from deaf if requested */
	if (collect->be_deaf) {
		wlc_phy_stay_in_carriersearch_acphy(pi, FALSE);
		wlapi_enable_mac(pi->sh->physhim);
	}

	/* revert to original gains if AGC was applied */
	if (collect->agc) {
	  /* wlc_phy_agc_rxgain_config_acphy(pi, FALSE); */
	}
	/* Restore filter settings if changed */
	if (collect->filter) {
		/* Restore LPF high pass corners to their original values */
		wlc_phy_lpf_hpc_override_acphy(pi, FALSE);
	}
	/* turn off sample collect config in PHY & MAC */
	phy_reg_write(pi, ACPHY_AdcDataCollect, 0);
	W_REG(pi->sh->osh, &pi->regs->psm_phy_hdr_param, 0x2);

	/* abort if timeout ocurred */
	if (timer == 0) {
		PHY_ERROR(("wl%d: %s Error: Timeout\n", pi->sh->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	PHY_TRACE(("wl%d: %s: Capture successful\n", pi->sh->unit, __FUNCTION__));


	if (pi_ac->pfirst > pi_ac->plast) {
		cnt = pi_ac->pstop - pi_ac->pfirst + 1;
		cnt += pi_ac->plast;
	} else {
		cnt = pi_ac->plast - pi_ac->pfirst;
	}

	sample_data->tag = htol16(WL_SAMPLEDATA_HEADER_TYPE);
	sample_data->length = htol16((WL_SAMPLEDATA_HEADER_SIZE));
	sample_data->flag = 0;		/* first sequence */
	sample_data->flag |= htol32(WL_SAMPLEDATA_MORE_DATA);

	sample_rate = CHSPEC_IS40(pi->radio_chanspec) ? 80 : 40;
	sample_bitwidth = 10;
	/* Hack in conversion factor for subtracting from adc sample
	 * power to obtain true analog power in dBm:
	 */
	dBsample_to_dBm_sub = 49;

	/* store header to buf */
	ptr[0] = htol32(0xACDC2009);
	ptr[1] = htol32(0xFFFF0000 | (FILE_HDR_LEN<<8));
	ptr[2] = htol32(cnt % (acphy_sc_length(pi) + 1));
	ptr[3] = htol32(0xFFFF0000 | (pi->pubpi.phy_rev<<8) | pi->pubpi.phy_type);
	ptr[4] = htol32(0xFFFFFF00);
	ptr[5] = htol32(((fc / 100) << 24) | ((fc % 100) << 16) | (3 << 8) |
		(CHSPEC_IS20(pi->radio_chanspec) ? 20 :
		(CHSPEC_IS40(pi->radio_chanspec) ? 40 : 80)));

	ptr[6] = htol32((collect->gpio_sel << 24) | (((mo >> 8)  & 0xff) << 16) |
	          ((mo & 0xff) << 8) | gpio_collection);
	ptr[7] = htol32(0xFFFF0000 | (downSamp << 8) | collect->trigger);
	ptr[8] = htol32(0xFFFFFFFF);
	ptr[9] = htol32((collect->post_dur << 16) | collect->pre_dur);
	FOREACH_CORE(pi, core) {
		ptr[10+core] = htol32((phy_reg_read(pi, ACPHY_RxIQCompA(core))) |
		                 (phy_reg_read(pi, ACPHY_RxIQCompB(core)) << 16));
	}
	ptr[13] = htol32(((collect->filter ? 1 : 0) << 24) | ((collect->agc ? 1 : 0) << 16) |
		(sample_rate << 8) | sample_bitwidth);
	ptr[14] = htol32(((dBsample_to_dBm_sub << 16) | agc_gain[0]));
	ptr[15] = htol32(((agc_gain[2] << 16) | agc_gain[1]));
	ptr[16] = htol32(0xFFFFFFFF);
	ptr[17] = htol32(0xFFFFFFFF);
	ptr[18] = htol32(0xFFFFFFFF);
	ptr[19] = htol32(0xFFFFFFFF);
	PHY_TRACE(("wl%d: %s: pfirst 0x%x plast 0x%x pstart 0x%x pstop 0x%x\n", pi->sh->unit,
		__FUNCTION__, pi_ac->pfirst, pi_ac->plast, pi_ac->pstart, pi_ac->pstop));
	PHY_TRACE(("wl%d: %s Capture length = %d words\n", pi->sh->unit, __FUNCTION__, cnt));

	return BCME_OK;
}

int
wlc_phy_sample_data_acphy(phy_info_t *pi, wl_sampledata_t *sample_data, void *b)
{
	uint32 data, cnt, bufsize, seq;
	phy_info_acphy_t *pi_ac = (phy_info_acphy_t *)pi->u.pi_acphy;
	uint8* head = (uint8 *)b;
	uint32* buf = (uint32 *)(head + sizeof(wl_sampledata_t));
	/* buf2 is used when sampledata is unpacked for
	 * version WL_SAMPLEDATA_T_VERSION_SPEC_AN
	 */

	int16* buf2 = (int16 *)(head + sizeof(wl_sampledata_t));
	int i;

	bufsize = ltoh32(sample_data->length) - sizeof(wl_sampledata_t);

	if (sample_data->version == (WL_SAMPLEDATA_T_VERSION_SPEC_AN)) {
		/* convert to # of (4*num_cores)--byte  words */
		bufsize = bufsize / (pi->pubpi.phy_corenum * 4);
	} else {
		/* convert to # of 4--byte words */
		bufsize = bufsize >> 2;
	}

	/* get the last sequence number */
	seq = ltoh32(sample_data->flag) & 0xff;

	/* Saturate sequence number to 0xff */
	seq = (seq < 0xff) ? (seq + 1) : 0xff;

	/* write back to data struct */
	sample_data->flag = htol32(seq);

	PHY_TRACE(("wl%d: %s: bufsize(words) %d flag 0x%x\n", pi->sh->unit, __FUNCTION__,
		bufsize, sample_data->flag));

	W_REG(pi->sh->osh, &pi->regs->tplatewrptr, pi_ac->pfirst << 2);

	/* Currently only 3 cores (and collect mode 0) are supported
	 * for version WL_SAMPLEDATA_T_VERSION_SPEC_AN
	 */

	if ((sample_data->version == WL_SAMPLEDATA_T_VERSION_SPEC_AN) &&
	    (pi->pubpi.phy_corenum != 3))  {
		/* No more data to read */
		sample_data->flag = sample_data->flag & 0xff;

		/* No bytes were read */
		sample_data->length = 0;

		bcopy((uint8 *)sample_data, head, sizeof(wl_sampledata_t));
		PHY_ERROR(("wl%d: %s: Number of cores not equal to 3! \n",
			pi->sh->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	/* Initialization for version WL_SAMPLEDATA_T_VERSION_SPEC_AN: */
	if ((sample_data->version == WL_SAMPLEDATA_T_VERSION_SPEC_AN) && (seq == 1)) {
		bool capture_start = FALSE;

		/* Search for and sync to a sample with a valid 2-bit '00' alignment pattern */
		while ((!capture_start) && (pi_ac->pfirst != pi_ac->plast)) {
			data = R_REG(pi->sh->osh, &pi->regs->tplatewrdata);
			/* wrap around end of fifo if necessary */
			if (pi_ac->pfirst == pi_ac->pstop) {
				W_REG(pi->sh->osh, &pi->regs->tplatewrptr, pi_ac->pstart << 2);
				pi_ac->pfirst = pi_ac->pstart;
				PHY_TRACE(("wl%d: %s TXFIFO wrap around\n",
					pi->sh->unit, __FUNCTION__));
			} else {
				pi_ac->pfirst++;
			}

			/* Check for alignment pattern 0x3 in the captured word */
			if (((data >> 30) & 0x3) == 0x3) {
				/* Read and discard one 32-bit word to
				 * move to where the next sample
				 * (with alignment pattern '00') starts
				 */
				data = R_REG(pi->sh->osh, &pi->regs->tplatewrdata);

				/* wrap around end of fifo if necessary */
				if (pi_ac->pfirst == pi_ac->pstop) {
					W_REG(pi->sh->osh, &pi->regs->tplatewrptr,
					      pi_ac->pstart << 2);
					pi_ac->pfirst = pi_ac->pstart;
					PHY_TRACE(("wl%d: %s TXFIFO wrap around\n",
						pi->sh->unit, __FUNCTION__));
				} else {
					pi_ac->pfirst++;
				}

				if (pi_ac->pfirst != pi_ac->plast) {
					capture_start = TRUE;
				}
			}
		}

		if (capture_start == FALSE) {
			/* ERROR: No starting pattern was found! */
			/* No more data to read */
			sample_data->flag = sample_data->flag & 0xff;

			/* No bytes were read */
			sample_data->length = 0;

			bcopy((uint8 *)sample_data, head, sizeof(wl_sampledata_t));
			PHY_ERROR(("wl%d: %s: Starting pattern not found! \n",
				pi->sh->unit, __FUNCTION__));
			return BCME_ERROR;
		}
	}

	/* start writing samples to buffer */
	cnt = 0;

	while ((cnt < bufsize) && (pi_ac->pfirst != pi_ac->plast)) {

		if (sample_data->version == WL_SAMPLEDATA_T_VERSION_SPEC_AN) {

			/* Unpack collected samples and write to buffer */
			uint32 data1[2];
			int16 isample[PHY_CORE_MAX];
			int16 qsample[PHY_CORE_MAX];

			for (i = 0; ((i < 2) && (pi_ac->pfirst != pi_ac->plast)); i++) {
				data1[i] = R_REG(pi->sh->osh, &pi->regs->tplatewrdata);
				/* wrap around end of fifo if necessary */
				if (pi_ac->pfirst == pi_ac->pstop) {
					W_REG(pi->sh->osh, &pi->regs->tplatewrptr,
					      pi_ac->pstart << 2);
					pi_ac->pfirst = pi_ac->pstart;
					PHY_TRACE(("wl%d: %s TXFIFO wrap around\n",
						pi->sh->unit, __FUNCTION__));
				} else {
					pi_ac->pfirst++;
				}
			}

			/* Unpack samples only if two 32-bit words have
			 * been successfully read from TX FIFO
			 */
			if (i == 2) {
				/* Unpack and perform sign extension: */
				uint16 temp;

				/* Core 0: */
				temp = (uint16)(data1[0] & 0x3ff);
				isample[0] = (temp & 0x200) ? (int16)(temp | 0xfc00) : temp;
				temp = (uint16)((data1[0] >> 10) & 0x3ff);
				qsample[0] = (temp & 0x200) ? (int16)(temp | 0xfc00) : temp;

				/* Core 1: */
				temp = (uint16)((data1[0] >> 20) & 0x3ff);
				isample[1] = (temp & 0x200) ? (int16)(temp | 0xfc00) : temp;
				temp = (uint16)(data1[1] & 0x3ff);
				qsample[1] = (temp & 0x200) ? (int16)(temp | 0xfc00) : temp;

				/* Core 2: */
				temp = (uint16)((data1[1] >> 10) & 0x3ff);
				isample[2] = (temp & 0x200) ? (int16)(temp | 0xfc00) : temp;
				temp = (uint16)((data1[1] >> 20) & 0x3ff);
				qsample[2] = (temp & 0x200) ? (int16)(temp | 0xfc00) : temp;

				/* Write to buffer in 2-byte words */
				buf2[6*cnt]     = isample[0];
				buf2[6*cnt + 1] = qsample[0];
				buf2[6*cnt + 2] = isample[1];
				buf2[6*cnt + 3] = qsample[1];
				buf2[6*cnt + 4] = isample[2];
				buf2[6*cnt + 5] = qsample[2];

				cnt++;
			}

		} else {
			/* Write collected samples as-is to buffer */

			data = R_REG(pi->sh->osh, &pi->regs->tplatewrdata);
			/* write one 4-byte word */
			buf[cnt++] = htol32(data);
			/* wrap around end of fifo if necessary */
			if (pi_ac->pfirst == pi_ac->pstop) {
				W_REG(pi->sh->osh, &pi->regs->tplatewrptr, pi_ac->pstart << 2);
				pi_ac->pfirst = pi_ac->pstart;
				PHY_TRACE(("wl%d: %s TXFIFO wrap around\n",
					pi->sh->unit, __FUNCTION__));
			} else {
				pi_ac->pfirst++;
			}
		}
	}

	PHY_TRACE(("wl%d: %s: Data fragment completed (pfirst %d plast %d)\n",
		pi->sh->unit, __FUNCTION__, pi_ac->pfirst, pi_ac->plast));
	if (pi_ac->pfirst != pi_ac->plast) {
		sample_data->flag |= htol32(WL_SAMPLEDATA_MORE_DATA);
	}

	/* update to # of bytes read */
	if (sample_data->version == WL_SAMPLEDATA_T_VERSION_SPEC_AN) {
		sample_data->length = htol16((cnt * 4 * pi->pubpi.phy_corenum));
	} else {
		sample_data->length = htol16((cnt << 2));
	}

	bcopy((uint8 *)sample_data, head, sizeof(wl_sampledata_t));
	PHY_TRACE(("wl%d: %s: Capture length = %d words\n", pi->sh->unit, __FUNCTION__, cnt));
	return BCME_OK;
}

void
acphy_set_sc_startptr(phy_info_t *pi, uint32 start_idx)
{

	W_REG(pi->sh->osh, &pi->regs->SampleCollectStartPtr, (start_idx & 0xFFFF));
	W_REG(pi->sh->osh, &pi->regs->SampleCollectPlayPtrHigh, ((start_idx >> 16) & 0xF));

}

uint32
acphy_get_sc_startptr(phy_info_t *pi)
{

	uint32 start_ptr_low, start_ptr_high, start_ptr;

	start_ptr_low = 	R_REG(pi->sh->osh, &pi->regs->SampleCollectStartPtr);
	start_ptr_high = 	(R_REG(pi->sh->osh, &pi->regs->SampleCollectPlayPtrHigh)) & 0xF;
	start_ptr = 		((start_ptr_high << 16) | start_ptr_low);

	return start_ptr;
}

void
acphy_set_sc_stopptr(phy_info_t *pi, uint32 stop_idx)
{

	uint32 stop_ptr_high;

	W_REG(pi->sh->osh, &pi->regs->SampleCollectStopPtr, (stop_idx & 0xFFFF));
	stop_ptr_high = ((stop_idx >> 16) & 0xF);
	W_REG(pi->sh->osh, &pi->regs->SampleCollectPlayPtrHigh, (stop_ptr_high << 4));

}

uint32
acphy_get_sc_stopptr(phy_info_t *pi)
{

	uint32 stop_ptr_low, stop_ptr_high, stop_ptr;

	stop_ptr_low = 	R_REG(pi->sh->osh, &pi->regs->SampleCollectStopPtr);
	stop_ptr_high = (R_REG(pi->sh->osh, &pi->regs->SampleCollectPlayPtrHigh) >> 4) & 0xF;
	stop_ptr = 		((stop_ptr_high << 16) | stop_ptr_low);

	return stop_ptr;
}

uint32
acphy_get_sc_curptr(phy_info_t *pi)
{

	uint32 cur_ptr_low, cur_ptr_high, cur_ptr;

	cur_ptr_low = 	R_REG(pi->sh->osh, &pi->regs->SampleCollectCurPtr);
	cur_ptr_high = (R_REG(pi->sh->osh, &pi->regs->SampleCollectCurPtrHigh)) & 0xF;
	cur_ptr = ((cur_ptr_high << 16) | cur_ptr_low);

	return cur_ptr;
}

uint32
acphy_is_sc_done(phy_info_t *pi)
{

	uint32 cur_ptr, stop_ptr;
	bool sc_done;

	cur_ptr  = acphy_get_sc_curptr(pi);
	stop_ptr = acphy_get_sc_stopptr(pi);

	if (cur_ptr == stop_ptr) {
		sc_done = TRUE;
	} else {
		sc_done = FALSE;
	}

	return sc_done;
}

int
acphy_sc_length(phy_info_t *pi)
{

	uint32 start_ptr, stop_ptr;
	int sc_len;
	start_ptr = acphy_get_sc_startptr(pi);
	stop_ptr = acphy_get_sc_stopptr(pi);
	sc_len = stop_ptr - start_ptr;

	return sc_len;
}

#endif /* SAMPLE_COLLECT */

void
wlc_phy_proprietary_mcs_acphy(phy_info_t *pi, bool enable_prop_mcs)
{

	if (((ACREV_IS(pi->pubpi.phy_rev, 1)) ||
	     (ACREV_IS(pi->pubpi.phy_rev, 3)) || (ACREV_GE(pi->pubpi.phy_rev, 6)))) {
	      MOD_PHYREG(pi, ACPHY_HTSigTones, ldpc_proprietary_mcs_vht, enable_prop_mcs);
	}
}

#ifdef WL_LPC

#define LPC_MIN_IDX 31
#define LPC_TOT_IDX (LPC_MIN_IDX + 1)
#define PWR_VALUE_BITS  0x3F

#ifdef WL_LPC_DEBUG
/*	table containing values of 0.5db difference,
	Each value is represented in S4.1 format
*/
static uint8 lpc_pwr_level[LPC_TOT_IDX] = {
		0x00, 0x01, 0x02, 0x03, 0x04, 0x05,
		0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B,
		0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11,
		0x12, 0x13, 0x14, 0x15, 0x16, 0x17,
		0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D,
		0x1E, 0x1F /* Max = Target - 15.5db */
		};
#endif /* WL_LPC_DEBUG */

uint8
wlc_acphy_lpc_getminidx(void)
{
	return LPC_MIN_IDX;
}

uint8
wlc_acphy_lpc_getoffset(uint8 index)
{
	return index;
	/* return lpc_pwr_level[index]; for PHYs which expect the actual offset
	 * for example, HT 4331.
	 */
}

#ifdef WL_LPC_DEBUG
uint8 *
wlc_acphy_lpc_get_pwrlevelptr(void)
{
	return lpc_pwr_level;
}
#endif
#endif /* WL_LPC */
