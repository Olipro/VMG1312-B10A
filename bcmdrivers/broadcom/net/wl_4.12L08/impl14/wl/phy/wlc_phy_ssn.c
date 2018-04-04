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
 * $Id: wlc_phy_ssn.c 345233 2012-07-17 06:12:10Z $
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
#include <bitfuncs.h>
#include <bcmdevs.h>
#include <proto/802.11.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <hndpmu.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#ifndef WLC_NET80211
#include <wlc_bsscfg.h>
#endif
#include <wl_dbg.h>
#include <wl_export.h>
#ifdef PPR_API
#include <wlc_ppr.h>
#endif
#include <wlc_phy_int.h>
#ifdef ROMTERMPHY
#include <bcm20xx.h>
#include <wlc.h>
#include <wlc_phy.h>
#include <sslpnphytbls.h>
#include <sslpnphyregs.h>
#else
#include <wlc_phy_radio.h>
#include <wlc_phytbl_ssn.h>
#include <wlc_phyreg_ssn.h>
#include <wlc_phy_ssn.h>
#endif /* PHYHAL */
#include <bcmnvram.h>

#ifdef WLMINIOCTL
#include <wl_minioctl.h>
#endif


#include <sbsprom.h>
#include <sbchipc.h>

#if !defined(ROMTERMPHY)
#define WL_SUSPEND_MAC_AND_WAIT(pi) wlapi_suspend_mac_and_wait(pi->sh->physhim)
#define WL_ENABLE_MAC(pi) wlapi_enable_mac(pi->sh->physhim)
#define WL_WRITE_SHM(pi, addr, val)  wlapi_bmac_write_shm(pi->sh->physhim, addr, val)
#define WL_READ_SHM(pi, addr)  wlapi_bmac_read_shm(pi->sh->physhim, addr)
#define WL_MCTRL(pi, addr, val) wlapi_bmac_mctrl(pi->sh->physhim, addr, val)
#define WL_PHYCAL PHY_TRACE
#define SCAN_IN_PROGRESS(pi) SCAN_INPROG_PHY(pi)
#define WLC_RM_IN_PROGRESS(pi) RM_INPROG_PHY(pi)
#define BCMECICOEX_ENAB(pi) BCMECICOEX_ENAB_PHY(pi)
#define PHY_CHANNEL2FREQ(channel)    wlc_phy_channel2freq(channel)
#define PHY_CHSPEC_BW(chanspec) CHSPEC_BW(chanspec)
#ifdef BAND5G
#define WL_PHY_BAND_RANGE(_pi, _ch)  wlc_phy_chanspec_bandrange_get(_pi, _ch)
#else
#define WL_PHY_BAND_RANGE(_pi, _ch) WL_CHAN_FREQ_RANGE_2G
#endif

#define wlc_stop_test(pi) wlc_phy_test_stop(pi)
#define wlc_init_test(pi, channel, txpkt) wlc_phy_test_init(pi, channel, txpkt)

#define wlc_pi (pi)
#define NON_BT_CHIP(wlc) 0

#ifdef WLSINGLE_ANT
#define ANT_AVAIL(_ant) 1
#else
#define ANT_AVAIL(_ant) (_ant)
#endif
#ifdef OLYMPIC
#define IS_OLYMPIC(_pi) TRUE
#else
#define IS_OLYMPIC(_pi) ((_pi->u.pi_sslpnphy->sslpnphy_OLYMPIC) && SSLPNREV_LT(pi->pubpi.phy_rev, 2))
#endif
#define si_pmu_res_4319_swctrl_war(sih, osh, on)

#ifdef WLPLT
#define PLT_IN_PROGRESS(pi) PLT_INPROG_PHY(pi)
#else
#define PLT_IN_PROGRESS(pi) (FALSE)
#endif
#ifdef STA
#define ASSOC_IN_PROGRESS(pi)	ASSOC_INPROG_PHY(pi)
#else
#define ASSOC_IN_PROGRESS(pi)	(FALSE)
#endif /* STA */

#define VBAT_RIPPLE_CHECK(_pi) 0
/* ALERT!!! for phyhal lcnphy and sslpnphy OFDM_CDD is not used!!! */
/* TX Power indexes */
#define TXP_FIRST_MCS_20	TXP_FIRST_MCS_20_SS	/* Index for first MCS at 20 MHz */
#define TXP_LAST_MCS_SISO_20	TXP_LAST_MCS_20_SISO_SS	/* Index for last SISO MCS at 20 MHz */

#define TXP_FIRST_MCS_40	 TXP_FIRST_MCS_40_SISO	/* Index for first MCS at 40 MHz */
#define TXP_LAST_MCS_SISO_40	 TXP_FIRST_MCS_40_SISO	/* Index for last SISO MCS at 40 MHz */
#define TXP_LAST_MCS_40		TXP_LAST_MCS_40_SS	/* Index for last MCS at 40 MHz */

#else  /* PHYHAL */
#define sslpnphy_specific (pi)
#define GENERIC_PHY_INFO(pi) ((pi)->pub)
#define WL_SUSPEND_MAC_AND_WAIT(pi) wlc_suspend_mac_and_wait(pi->wlc)
#define WL_ENABLE_MAC(pi) wlc_enable_mac(pi->wlc)
#define WL_WRITE_SHM(pi, addr, val)  wlc_write_shm(pi->wlc, addr, val)
#define WL_READ_SHM(pi, addr)  wlc_read_shm(pi->wlc, addr)
#define WL_MCTRL(pi, addr, val) wlc_mctrl(pi->wlc, addr, val)
#define PHY_GETVAR(pi, name) getvar(pi->pub->vars, name)
#define PHY_GETINTVAR(pi, name) getintvar(pi->pub->vars, name)
#define PHY_GETINTVAR_DEFAULT(pi, name, default_value) phy_getintvar_default(pi, name, default_value)
#define PHY_GETINTVAR_ARRAY(name, type, idx) read_nvram_array(name, type, idx)
#define PHY_CHANNEL2FREQ(channel)    wlc_channel2freq(channel)
#define PHY_CHSPEC_BW(chanspec) CHSPEC_WLC_BW(chanspec)
#ifdef BAND5G
extern int wlc_get_band_range(phy_info_t*, chanspec_t);
#define WL_PHY_BAND_RANGE(_pi, _ch)  wlc_get_band_range(_pi, _ch)
#else
#define WL_PHY_BAND_RANGE(_pi, _ch) WL_CHAN_FREQ_RANGE_2G
#endif

#define SCAN_IN_PROGRESS(wlc)	(wlc_scan_inprog(wlc))
#define WLC_RM_IN_PROGRESS(wlc)	(wlc_rminprog(wlc))

#undef IS20MHZ
#undef IS40MHZ
#ifdef WL20MHZ_ONLY
#define IS20MHZ(pi) (TRUE)
#define IS40MHZ(pi) (FALSE)
#else
#define IS20MHZ(pi)	((pi)->bw == WLC_20_MHZ)
#define IS40MHZ(pi)	((pi)->bw == WLC_40_MHZ)
#endif
#ifdef WLPLT
#define PLT_IN_PROGRESS(wlc) (wlc_pltinprog(wlc))
#else
#define PLT_IN_PROGRESS(wlc) (FALSE)
#endif
#ifdef STA
#define ASSOC_IN_PROGRESS(wlc)	(wlc_associnprog(wlc))
#else
#define ASSOC_IN_PROGRESS(wlc)	(FALSE)
#endif /* STA */
#define VBAT_RIPPLE_CHECK(_pi) 0
#define BCM5356_CHIP_ID 0		/* not supported */

/* TX Power indexes */
#define TXP_FIRST_CCK		0	/* Index for first CCK rate */
#define TXP_LAST_CCK		3	/* Index for last CCK rate */
#define TXP_FIRST_OFDM		4	/* Index for first OFDM rate */
#define TXP_LAST_OFDM		11	/* Index for last OFDM rate */
#define TXP_FIRST_MCS_20	12	/* Index for first MCS at 20 MHz */
#define TXP_LAST_MCS_SISO_20	19	/* Index for last SISO MCS at 20 MHz */
#define TXP_FIRST_MCS_SISO_20_CDD 20 /* Index for first MCS_CDD at 20 MHz */
#define TXP_LAST_MCS_SISO_20_CDD 27 /* Index for last MCS_CDD at 20 MHz */
#define TXP_FIRST_MCS_40	28	/* Index for first MCS at 40 MHz */
#define TXP_LAST_MCS_SISO_40	35	/* Index for last SISO MCS at 40 MHz */
#define TXP_FIRST_MCS_SISO_40_CDD  36 /* Index for first MCS_CDD at 40 MHz */
#define TXP_LAST_MCS_40		44	/* Index for last MCS at 40 MHz */
#endif /* PHYHAL */

#define MAX_PREDICT_CAL 10

/* This is to avoid build panic */
#if !defined(BCM94319USBB_SSID)
#define BCM94319USBB_SSID 0x04ee
#endif

#if !defined(BCM94319SDNA_SSID)
#define BCM94319SDNA_SSID 0x058b
#endif

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  inter-module connection					*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

#define wlc_radio_2063_rc_cal_done(pi) (0 != (read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_6) & 0x02))

/* %%%%%%%%%%%%%%%%%%%% */
/*  common function	*/
/* %%%%%%%%%%%%%%%%%%%% */
extern uint wlc_phy_channel2idx(uint channel);

extern void wlc_phy_do_dummy_tx(phy_info_t *pi, bool ofdm, bool pa_on);
void wlc_sslpnphy_periodic_cal_top(phy_info_t *pi); /* ROMTERM */
void wlc_sslpnphy_auxadc_measure(wlc_phy_t *ppi, bool readVal);	/* ROMTERM */
int8 wlc_sslpnphy_get_rx_pwr_offset(phy_info_t *pi);	/* ROMTERM */
void wlc_sslpnphy_rx_offset_init(phy_info_t *pi); /* ROMTERM only */
void wlc_sslpnphy_cck_filt_load(phy_info_t *pi, uint8 filtsel);	/* ROMTERM only */
void wlc_sslpnphy_channel_gain_adjust(phy_info_t *pi);	/* ROMTERM only */
void wlc_sslpnphy_CmRxAciGainTbl_Tweaks(void *args);	/* ROMTERM only */
void BCMROMFN(wlc_sslpnphy_clear_tx_power_offsets)(phy_info_t *pi);	/* ROMTERM only */
bool wlc_sslpnphy_btcx_override_enable(phy_info_t *pi);	/* ROMTERM only */
void wlc_sslpnphy_recalc_tssi2dbm_tbl(phy_info_t *pi, int32 a1, int32 b0, int32 b1); /* ROMTERM only */
void BCMROMFN(wlc_sslpnphy_set_tx_locc)(phy_info_t *pi, uint16 didq); /* ROMTERM only */
void wlc_sslpnphy_txpwrtbl_iqlo_cal(phy_info_t *pi); /* ROMTERM only */
int32 wlc_sslpnphy_vbatsense(phy_info_t *pi); /* ROMTERM only */
void wlc_sslpnphy_save_papd_calibration_results(phy_info_t *pi); /* ROMTERM only */
int wlc_sslpnphy_tempsense(phy_info_t *pi); /* ROMTERM only */
void wlc_sslpnphy_temp_adj(phy_info_t *pi); /* ROMTERM only */
void wlc_sslpnphy_set_chanspec_tweaks(phy_info_t *pi, chanspec_t chanspec); /* ROMTERM only */
int BCMROMFN(wlc_get_ssn_lp_band_range)(uint); /* ROMTERM only */
void wlc_2063_vco_cal(phy_info_t *pi); /* ROMTERM only */
void wlc_sslpnphy_setchan_cal(phy_info_t *pi, int32 int_val); /* ROMTERM only */
static void wlc_sslpnphy_aci_init(phy_info_t *pi);
void wlc_sslpnphy_aci(phy_info_t *pi, bool on);
static void wlc_sslpnphy_aci_upd(phy_info_t *pi);
/* %%%%%%%%%%%%%%%%%%%% */
/*  debugging		*/
/* %%%%%%%%%%%%%%%%%%%% */
#if defined(BCMDBG) || defined(WLTEST)
extern int wlc_init_test(phy_info_t *pi, int channel, bool txpkt);
extern int wlc_stop_test(phy_info_t *pi);
#endif  

#if defined(WLTEST)
void wlc_sslpnphy_reset_radio_loft(phy_info_t *pi);
void wlc_sslpnphy_rssi_snr_calc(phy_info_t *pi, void * hdl, int32 * rssi, int32 * snr);
#endif


/* %%%%%%%%%%%%%%%%%%%% */
/*  radio control	*/
/* %%%%%%%%%%%%%%%%%%%% */

static void wlc_radio_2063_init_sslpnphy(phy_info_t *pi);

static void wlc_sslpnphy_radio_2063_channel_tune(phy_info_t *pi, uint8 channel);
static void wlc_sslpnphy_txpower_recalc_target_5356(phy_info_t *pi);


/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  macro							*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */


#define LPPHY_IQLOCC_READ(val) ((uint8)(-(int8)(((val) & 0xf0) >> 4) + (int8)((val) & 0x0f)))
#define PLL_2063_LOW_END_VCO 	3000
#define PLL_2063_LOW_END_KVCO 	27
#define PLL_2063_HIGH_END_VCO	4200
#define PLL_2063_HIGH_END_KVCO	68
#define PLL_2063_LOOP_BW			300
#define PLL_2063_LOOP_BW_ePA			500
#define PLL_2063_D30				3000
#define PLL_2063_D30_ePA			1500
#define PLL_2063_CAL_REF_TO		8
#define PLL_2063_MHZ				1000000
#define PLL_2063_OPEN_LOOP_DELAY	5
#define PLL_2063_LOOP_BW_SDNA_1		500
#define PLL_2063_D30_SDNA_1			3000
#define PLL_2063_LOOP_BW_SDNA_2		300
#define PLL_2063_D30_SDNA_2			4000

/* %%%%%%%%%%%%%%% */
/* SSLPNPHY macros */
/* %%%%%%%%%%%%%%% */


#define SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(SSLPNPHY_txgainctrlovrval1_txgainctrl_ovr_val1_SHIFT + 8)
#define SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << SSLPNPHY_txgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define SSLPNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT \
	(SSLPNPHY_stxtxgainctrlovrval1_stxtxgainctrl_ovr_val1_SHIFT + 8)
#define SSLPNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_MASK \
	(0x7f << SSLPNPHY_stxtxgainctrlovrval1_pagain_ovr_val1_SHIFT)

#define wlc_sslpnphy_enable_tx_gain_override(pi) \
	wlc_sslpnphy_set_tx_gain_override(pi, TRUE)
#define wlc_sslpnphy_disable_tx_gain_override(pi) \
	wlc_sslpnphy_set_tx_gain_override(pi, FALSE)

#define wlc_sslpnphy_set_start_tx_pwr_idx(pi, idx) \
	PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlCmd, pwrIndex_init, (uint16)(idx))

#define wlc_sslpnphy_set_tx_pwr_npt(pi, npt) \
	PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlNnum, Npt_intg_log2, (uint16)(npt))

#define wlc_sslpnphy_get_tx_pwr_ctrl(pi) \
	(phy_reg_read((pi), SSLPNPHY_TxPwrCtrlCmd) & \
			(SSLPNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK | \
			SSLPNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK | \
			SSLPNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK))

#define wlc_sslpnphy_get_tx_pwr_npt(pi) \
	(PHY_REG_READ(pi, SSLPNPHY, TxPwrCtrlNnum, Npt_intg_log2))

#define wlc_sslpnphy_get_current_tx_pwr_idx(pi) \
	(PHY_REG_READ(pi, SSLPNPHY, TxPwrCtrlStatus, baseIndex))

#define wlc_sslpnphy_get_target_tx_pwr(pi) \
	(PHY_REG_READ(pi, SSLPNPHY, TxPwrCtrlTargetPwr, targetPwr0))

#if !defined(ROMTERMPHY)
#define wlc_sslpnphy_validated_tssi_pwr(pi, pwr) \
	MIN((pi->u.pi_sslpnphy)->sslpnphy_tssi_max_pwr_limit, \
	MAX((pi->u.pi_sslpnphy)->sslpnphy_tssi_min_pwr_limit, (pwr)))
#else
#define  wlc_sslpnphy_validated_tssi_pwr(pi, pwr) \
	MIN((pi)->sslpnphy_tssi_max_pwr_limit, \
	MAX((pi)->sslpnphy_tssi_min_pwr_limit, (pwr)))
#endif /* PHYHAL */

#define wlc_sslpnphy_force_target_tx_pwr(pi, target) \
	PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlTargetPwr, targetPwr0, (uint16)(target))

#define wlc_sslpnphy_set_target_tx_pwr(pi, target) \
	wlc_sslpnphy_force_target_tx_pwr(pi, wlc_sslpnphy_validated_tssi_pwr(pi, target))

/* Turn off all the crs signals to the MAC */
#define wlc_sslpnphy_set_deaf(pi)	wlc_sslpnphy_deaf_mode(pi, TRUE)

/* Restore all the crs signals to the MAC */
#define wlc_sslpnphy_clear_deaf(pi)	 wlc_sslpnphy_deaf_mode(pi, FALSE)

#define wlc_sslpnphy_iqcal_active(pi)	\
	(phy_reg_read((pi), SSLPNPHY_iqloCalCmd) & \
	(SSLPNPHY_iqloCalCmd_iqloCalCmd_MASK | SSLPNPHY_iqloCalCmd_iqloCalDFTCmd_MASK))

#define wlc_sslpnphy_tssi_enabled(pi) \
	(SSLPNPHY_TX_PWR_CTRL_OFF != wlc_sslpnphy_get_tx_pwr_ctrl((pi)))

#define SWCTRL_BT_TX		0x18
#define SWCTRL_OVR_DISABLE	0x40

#define	AFE_CLK_INIT_MODE_TXRX2X	1
#define	AFE_CLK_INIT_MODE_PAPD		0

#define SSLPNPHY_TX_PWR_CTRL_OFF	0
#define SSLPNPHY_TX_PWR_CTRL_SW SSLPNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK
#define SSLPNPHY_TX_PWR_CTRL_HW \
	(SSLPNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK | \
	SSLPNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK | \
	SSLPNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK)

#define SSLPNPHY_TBL_ID_IQLOCAL			0x00
#define SSLPNPHY_TBL_ID_TXPWRCTL 		0x07
#define SSLPNPHY_TBL_ID_GAIN_IDX		0x0d
#define SSLPNPHY_TBL_ID_GAIN_TBL		0x12
#define SSLPNPHY_TBL_ID_GAINVALTBL_IDX		0x11
#define SSLPNPHY_TBL_ID_SW_CTRL			0x0f
#define SSLPNPHY_TBL_ID_SPUR			0x14
#define SSLPNPHY_TBL_ID_SAMPLEPLAY		0x15
#define SSLPNPHY_TBL_ID_SAMPLEPLAY1		0x16
#define SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL	0x18

#define SSLPNPHY_TX_PWR_CTRL_RATE_OFFSET 	64
#define SSLPNPHY_TX_PWR_CTRL_MAC_OFFSET 	128
#define SSLPNPHY_TX_PWR_CTRL_GAIN_OFFSET 	192
#define SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET		320
#define SSLPNPHY_TX_PWR_CTRL_LO_OFFSET		448
#define SSLPNPHY_TX_PWR_CTRL_PWR_OFFSET		576

#define SSLPNPHY_TX_PWR_CTRL_START_INDEX_2G	60
#define SSLPNPHY_TX_PWR_CTRL_START_INDEX_5G	70
#define SSLPNPHY_TX_PWR_CTRL_START_INDEX_2G_PAPD	100
#define SSLPNPHY_TX_PWR_CTRL_START_INDEX_5G_PAPD	70

#define SSLPNPHY_LOW_BAND_ZERO_DBM_INDEX	 106
#define SSLPNPHY_MID_BAND_ZERO_DBM_INDEX	 114
#define SSLPNPHY_HIGH_BAND_ZERO_DBM_INDEX	 116

/* channel 36 = 5180 MHz, channel 44 = 5220 MHz */
#define SSLPNPHY_LOW_LISTEN_GAIN	5180
#define SSLPNPHY_HIGH_LISTEN_GAIN	5220

/* channel 36 = 5260 MHz, channel 64 = 5320 MHz */
#define SSLPNPHY_LOW_START_FREQ		5180
#define SSLPNPHY_LOW_END_FREQ		5320

/* channel 100 = 5500 MHz, channel 140 = 5700 MHz */
#define SSLPNPHY_MID_START_FREQ		5500
#define SSLPNPHY_MID_END_FREQ		5700

/* channel 149 = 5745 MHz, channel 165 = 5825 MHz */
#define SSLPNPHY_HIGH_START_FREQ	5745
#define SSLPNPHY_HIGH_END_FREQ		5825

#define SSLPNPHY_TX_PWR_CTRL_START_NPT		1
#define SSLPNPHY_TX_PWR_CTRL_MAX_NPT		1

#define SSLPNPHY_NUM_DIG_FILT_COEFFS 9

#define SSLPNPHY_TX_POWER_TABLE_SIZE	128
#define SSLPNPHY_MAX_TX_POWER_INDEX	(SSLPNPHY_TX_POWER_TABLE_SIZE - 1)

#define SSLPNPHY_NOISE_SAMPLES_DEFAULT 5000

#define SSLPNPHY_ACI_DETECT_START      1
#define SSLPNPHY_ACI_DETECT_PROGRESS   2
#define SSLPNPHY_ACI_DETECT_STOP       3

#define SSLPNPHY_ACI_CRSHIFRMLO_TRSH 100
#define SSLPNPHY_ACI_GLITCH_TRSH 2000
#define	SSLPNPHY_ACI_TMOUT 250		/* Time for CRS HI and FRM LO (in micro seconds) */
#define SSLPNPHY_ACI_DETECT_TIMEOUT  2	/* in  seconds */
#define SSLPNPHY_ACI_START_DELAY 0

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

#define wlc_sslpnphy_tx_gain_override_enabled(pi) \
	(0 != (phy_reg_read((pi), SSLPNPHY_AfeCtrlOvr) & SSLPNPHY_AfeCtrlOvr_dacattctrl_ovr_MASK))

#define wlc_sslpnphy_total_tx_frames(pi) \
	WL_READ_SHM(pi, M_UCODE_MACSTAT + OFFSETOF(macstat_t, txallfrm))

typedef struct {
	uint16 gm_gain;
	uint16 pga_gain;
	uint16 pad_gain;
	uint16 dac_gain;
} sslpnphy_txgains_t;

typedef struct {
	sslpnphy_txgains_t gains;
	bool useindex;
	int8 index;
} sslpnphy_txcalgains_t;

typedef struct {
	uint8 chan;
	int16 a;
	int16 b;
} sslpnphy_rx_iqcomp_t;

typedef enum {
	SSLPNPHY_CAL_FULL,
	SSLPNPHY_CAL_RECAL,
	SSLPNPHY_CAL_CURRECAL,
	SSLPNPHY_CAL_DIGCAL,
	SSLPNPHY_CAL_GCTRL
} sslpnphy_cal_mode_t;

typedef enum {
	SSLPNPHY_PAPD_CAL_CW,
	SSLPNPHY_PAPD_CAL_OFDM
} sslpnphy_papd_cal_type_t;


#if defined(SSLPNPHYCAL_CACHING)
int valid_channel_list[] = { 36, 40, 44, 48, 108, 149, 153, 157, 161, 165 };
int wlc_phy_cal_cache_restore_sslpnphy(phy_info_t *pi);
void wlc_phy_cal_cache_sslpnphy(phy_info_t *pi);
void wlc_phy_destroy_sslpnphy_chanctx(uint16 chanspec);
sslpnphy_calcache_t * wlc_phy_create_sslpnphy_chanctx(uint16 chanspec);
sslpnphy_calcache_t * wlc_phy_get_sslpnphy_chanctx(uint16 chanspec);
void wlc_phy_reset_sslpnphy_chanctx(uint16 chanspec);
int check_valid_channel_to_cache(uint16 chanspec);
#endif

/* SSLPNPHY IQCAL parameters for various Tx gain settings */
/* table format: */
/*	target, gm, pga, pad, ncorr for each of 5 cal types */
typedef uint16 iqcal_gain_params_sslpnphy[9];

STATIC const iqcal_gain_params_sslpnphy tbl_iqcal_gainparams_sslpnphy_2G[] = {
	{0, 0, 0, 0, 0, 0, 0, 0, 0},
	};

STATIC const iqcal_gain_params_sslpnphy tbl_iqcal_gainparams_sslpnphy_5G[] = {
	{0x7ef, 7, 0xe, 0xe, 0, 0, 0, 0, 0},
	};

STATIC const iqcal_gain_params_sslpnphy *tbl_iqcal_gainparams_sslpnphy[2] = {
	tbl_iqcal_gainparams_sslpnphy_2G,
	tbl_iqcal_gainparams_sslpnphy_5G
	};

STATIC const uint16 iqcal_gainparams_numgains_sslpnphy[2] = {
	sizeof(tbl_iqcal_gainparams_sslpnphy_2G) / sizeof(*tbl_iqcal_gainparams_sslpnphy_2G),
	sizeof(tbl_iqcal_gainparams_sslpnphy_5G) / sizeof(*tbl_iqcal_gainparams_sslpnphy_5G),
	};

/* LO Comp Gain ladder. Format: {m genv} */
STATIC CONST
uint16 BCMROMDATA(sslpnphy_iqcal_loft_gainladder)[]  = {
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
STATIC CONST
uint16 BCMROMDATA(sslpnphy_iqcal_ir_gainladder)[] = {
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

/* Autogenerated by 2063_chantbl_tcl2c.tcl */
STATIC const
sslpnphy_rx_iqcomp_t sslpnphy_rx_iqcomp_table_rev0[] = {
	{ 1, 0, 0 },
	{ 2, 0, 0 },
	{ 3, 0, 0 },
	{ 4, 0, 0 },
	{ 5, 0, 0 },
	{ 6, 0, 0 },
	{ 7, 0, 0 },
	{ 8, 0, 0 },
	{ 9, 0, 0 },
	{ 10, 0, 0 },
	{ 11, 0, 0 },
	{ 12, 0, 0 },
	{ 13, 0, 0 },
	{ 14, 0, 0 },
	{ 34, 0, 0 },
	{ 38, 0, 0 },
	{ 42, 0, 0 },
	{ 46, 0, 0 },
	{ 36, 0, 0 },
	{ 40, 0, 0 },
	{ 44, 0, 0 },
	{ 48, 0, 0 },
	{ 52, 0, 0 },
	{ 56, 0, 0 },
	{ 60, 0, 0 },
	{ 64, 0, 0 },
	{ 100, 0, 0 },
	{ 104, 0, 0 },
	{ 108, 0, 0 },
	{ 112, 0, 0 },
	{ 116, 0, 0 },
	{ 120, 0, 0 },
	{ 124, 0, 0 },
	{ 128, 0, 0 },
	{ 132, 0, 0 },
	{ 136, 0, 0 },
	{ 140, 0, 0 },
	{ 149, 0, 0 },
	{ 153, 0, 0 },
	{ 157, 0, 0 },
	{ 161, 0, 0 },
	{ 165, 0, 0 },
	{ 184, 0, 0 },
	{ 188, 0, 0 },
	{ 192, 0, 0 },
	{ 196, 0, 0 },
	{ 200, 0, 0 },
	{ 204, 0, 0 },
	{ 208, 0, 0 },
	{ 212, 0, 0 },
	{ 216, 0, 0 },
	};

static const uint32 sslpnphy_gaincode_table[] = {
	0x100800,
	0x100050,
	0x100150,
	0x100250,
	0x100950,
	0x100255,
	0x100955,
	0x100a55,
	0x110a55,
	0x1009f5,
	0x10095f,
	0x100a5f,
	0x110a5f,
	0x10305,
	0x10405,
	0x10b05,
	0x1305,
	0x35a,
	0xa5a,
	0xb5a,
	0x125a,
	0x135a,
	0x10b5f,
	0x135f,
	0x1135f,
	0x1145f,
	0x1155f,
	0x33af,
	0x132ff,
	0x232ff,
	0x215ff,
	0x216ff,
	0x235ff,
	0x236ff,
	0x255ff,
	0x256ff,
	0x2d5ff
};

static const uint8 sslpnphy_gain_table[] = {
	-14,
	-11,
	-8,
	-6,
	-2,
	0,
	4,
	6,
	9,
	12,
	16,
	18,
	21,
	25,
	27,
	31,
	34,
	37,
	39,
	43,
	45,
	49,
	52,
	55,
	58,
	60,
	63,
	65,
	68,
	71,
	74,
	77,
	80,
	83,
	86,
	89,
	92
};

static const int8 sslpnphy_gain_index_offset_for_rssi[] = {
	7,	/* 0 */
	7,	/* 1 */
	7,	/* 2 */
	7,	/* 3 */
	7,	/* 4 */
	7,	/* 5 */
	7,	/* 6 */
	8,	/* 7 */
	7,	/* 8 */
	7,	/* 9 */
	6,	/* 10 */
	7,	/* 11 */
	7,	/* 12 */
	4,	/* 13 */
	4,	/* 14 */
	4,	/* 15 */
	4,	/* 16 */
	4,	/* 17 */
	4,	/* 18 */
	4,	/* 19 */
	4,	/* 20 */
	3,	/* 21 */
	3,	/* 22 */
	3,	/* 23 */
	3,	/* 24 */
	3,	/* 25 */
	3,	/* 26 */
	4,	/* 27 */
	2,	/* 28 */
	2,	/* 29 */
	2,	/* 30 */
	2,	/* 31 */
	2,	/* 32 */
	2,	/* 33 */
	-1,	/* 34 */
	-2,	/* 35 */
	-2,	/* 36 */
	-2	/* 37 */
};

static const int8 sslpnphy_gain_index_offset_for_pkt_rssi[] = {
	8,	/* 0 */
	8,	/* 1 */
	8,	/* 2 */
	8,	/* 3 */
	8,	/* 4 */
	8,	/* 5 */
	8,	/* 6 */
	9,	/* 7 */
	10,	/* 8 */
	8,	/* 9 */
	8,	/* 10 */
	7,	/* 11 */
	7,	/* 12 */
	1,	/* 13 */
	2,	/* 14 */
	2,	/* 15 */
	2,	/* 16 */
	2,	/* 17 */
	2,	/* 18 */
	2,	/* 19 */
	2,	/* 20 */
	2,	/* 21 */
	2,	/* 22 */
	2,	/* 23 */
	2,	/* 24 */
	2,	/* 25 */
	2,	/* 26 */
	2,	/* 27 */
	2,	/* 28 */
	2,	/* 29 */
	2,	/* 30 */
	2,	/* 31 */
	1,	/* 32 */
	1,	/* 33 */
	0,	/* 34 */
	0,	/* 35 */
	0,	/* 36 */
	0	/* 37 */
};


extern CONST uint8 spur_tbl_rev0[];
extern CONST uint8 spur_tbl_rev2[];

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  typedef, enum, structure, global variable			*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

/* channel info type for 2063 radio */
typedef struct _chan_info_2063 {
	uint   chan;            /* channel number */
	uint   freq;            /* in Mhz */
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

/* channel info type for 2063 radio used in sslpnphy */
typedef struct _chan_info_2063_sslpnphy {
#ifdef BCM4329B1
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */
#else
	uint chan;            /* channel number */
	uint freq;            /* in Mhz */
#endif
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
	uint8 dummy2;
	uint8 RF_txrf_ctrl_2;
	uint8 RF_txrf_ctrl_5;
	uint8 RF_pa_ctrl_11;
	uint8 RF_arx_mix_4;
	uint8 dummy4;
} chan_info_2063_sslpnphy_t;

#ifdef BAND5G
typedef struct _chan_info_2063_sslpnphy_manhattan_aband_tweaks {
	uint freq;
	uint16 valid_tweak;
	uint8 RF_PA_CTRL_2;
	uint8 RF_PA_CTRL_5;
	uint8 RF_PA_CTRL_7;
	uint8 RF_PA_CTRL_11;
	uint8 RF_TXRF_CTRL_2;
	uint8 RF_TXRF_CTRL_5;
} chan_info_2063_sslpnphy_manhattan_aband_tweaks_t;
typedef struct _chan_info_2063_sslpnphy_ninja_aband_tweaks {
	uint freq;
	uint16 valid_tweak;
	uint8 RF_PA_CTRL_2;
	uint8 RF_PA_CTRL_5;
	uint8 RF_PA_CTRL_7;
	uint8 RF_PA_CTRL_11;
	uint8 RF_TXRF_CTRL_2;
} chan_info_2063_sslpnphy_ninja_aband_tweaks_t;
typedef struct _chan_info_2063_sslpnphy_X17_aband_tweaks {
	uint freq;
	uint16 valid_tweak;
	uint8 BCM_RF_PA_CTRL_5;
	uint8 BCM_RF_TXRF_CTRL_8;
	uint8 BCM_RF_TXRF_CTRL_5;
	uint8 BCM_RF_TXRF_CTRL_2;
	uint8 MRT_RF_PA_CTRL_5;
	uint8 MRT_RF_TXRF_CTRL_8;
	uint8 MRT_RF_TXRF_CTRL_5;
	uint8 MRT_RF_TXRF_CTRL_2;
	uint8 ePA_RF_PA_CTRL_11;
	uint8 ePA_RF_TXRF_CTRL_8;
	uint8 ePA_RF_TXRF_CTRL_5;
	uint8 ePA_RF_TXRF_CTRL_2;
} chan_info_2063_sslpnphy_X17_aband_tweaks_t;
typedef struct _chan_info_2063_sslpnphy_aband_tweaks {
	uint freq;
	uint16 valid_tweak;
	uint8 RF_PA_CTRL_11;
	uint8 RF_TXRF_CTRL_8;
	uint8 RF_TXRF_CTRL_5;
	uint8 RF_TXRF_CTRL_2;
	uint8 RF_TXRF_CTRL_4;
	uint8 RF_TXRF_CTRL_7;
	uint8 RF_TXRF_CTRL_6;
	uint8 RF_PA_CTRL_2;
	uint8 RF_PA_CTRL_5;
	uint8 RF_TXRF_CTRL_15;
	uint8 RF_TXRF_CTRL_14;
	uint8 RF_PA_CTRL_7;
	uint8 RF_PA_CTRL_15;
} chan_info_2063_sslpnphy_aband_tweaks_t;
typedef struct _chan_info_2063_sslpnphy_X17_epa_tweaks {
	uint freq;
	uint8 ePA_JTAG_PLL_CP_2;
	uint8 ePA_JTAG_PLL_CP_3;
} chan_info_2063_sslpnphy_X17_epa_tweaks_t;

STATIC chan_info_2063_sslpnphy_X17_epa_tweaks_t chan_info_2063_sslpnphy_X17_epa_tweaks[] = {
	{5180, 0x6d, 0x12},
	{5200, 0x6d, 0x12},
	{5220, 0x6d, 0x12},
	{5240, 0x6c, 0x11},
	{5260, 0x6c, 0x11},
	{5280, 0x6c, 0x11},
	{5300, 0x6b, 0x10},
	{5320, 0x6b, 0x10},
	{5500, 0x68, 0xf},
	{5520, 0x68, 0xf},
	{5540, 0x67, 0xe},
	{5560, 0x68, 0xf},
	{5580, 0x67, 0xe},
	{5600, 0x67, 0xe},
	{5620, 0x67, 0xe},
	{5640, 0x66, 0xe},
	{5660, 0x66, 0xe},
	{5680, 0x66, 0xe},
	{5700, 0x66, 0xe},
	{5745, 0x65, 0xd},
	{5765, 0x65, 0xd},
	{5785, 0x65, 0xd},
	{5805, 0x65, 0xd},
	{5825, 0x64, 0xd}
};

/* it is important to have the same channels as in chan_info_2063_sslpnphy_aband_tweaks */
/* manhattan has been fixed, ninja still needs this */
STATIC chan_info_2063_sslpnphy_manhattan_aband_tweaks_t chan_info_2063_sslpnphy_manhattan_aband_tweaks[] = {
	{5170, 0x3f, 0x60, 0x90, 0x02, 0x70, 0xf4, 0xf8 },
	{5180, 0x3f, 0x60, 0x90, 0x02, 0x70, 0xf4, 0xf8 },
	{5190, 0x3f, 0x60, 0x90, 0x02, 0x70, 0xf4, 0xf8 },
	{5200, 0x3f, 0x60, 0x90, 0x02, 0x70, 0xf4, 0xf8 },
	{5210, 0x3f, 0x60, 0x90, 0x02, 0x70, 0xf4, 0xf8 },
	{5220, 0x3f, 0x60, 0x90, 0x02, 0x70, 0xd4, 0xf8 },
	{5230, 0x3f, 0x60, 0x90, 0x02, 0x70, 0xd4, 0xf8 },
	{5240, 0x3f, 0x60, 0x90, 0x02, 0x70, 0xb4, 0xf8 },
	{5260, 0x3f, 0x60, 0x90, 0x02, 0x70, 0x94, 0xf8 },
	{5270, 0x3f, 0x60, 0x90, 0x02, 0x70, 0x94, 0xf8 },
	{5280, 0x3f, 0x60, 0x90, 0x02, 0x70, 0x84, 0xf8 },
	{5300, 0x3f, 0x60, 0x90, 0x02, 0x70, 0x84, 0xf8 },
	{5310, 0x3f, 0x60, 0x90, 0x02, 0x70, 0x84, 0xf8 },
	{5320, 0x3f, 0x60, 0x90, 0x02, 0x70, 0x84, 0xf8 },

	{5500, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5510, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5520, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5540, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5550, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5560, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5580, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5590, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5600, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5620, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5630, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5640, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5660, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5670, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5680, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },
	{5700, 0x3f, 0x60, 0x50, 0x02, 0x50, 0x64, 0xa8 },

	{5745, 0x3f, 0x60, 0x60, 0x02, 0x20, 0x44, 0x48 },
	{5755, 0x3f, 0x60, 0x50, 0x02, 0x30, 0x44, 0x48 },
	{5765, 0x3f, 0x60, 0x50, 0x02, 0x30, 0x44, 0x48 },
	{5785, 0x3f, 0x60, 0x50, 0x02, 0x30, 0x44, 0x48 },
	{5795, 0x3f, 0x60, 0x50, 0x02, 0x30, 0x44, 0x48 },
	{5805, 0x3f, 0x60, 0x50, 0x02, 0x30, 0x44, 0x48 },
	{5825, 0x3f, 0x60, 0x50, 0x02, 0x20, 0x44, 0x48 }
};
STATIC chan_info_2063_sslpnphy_ninja_aband_tweaks_t chan_info_2063_sslpnphy_ninja_aband_tweaks[] = {
	{5180, 0x1f, 0x90, 0x90, 0x02, 0x50, 0xf8 },
	{5190, 0x1f, 0x90, 0x90, 0x02, 0x50, 0xf8 },
	{5200, 0x1f, 0x90, 0x80, 0x02, 0x40, 0xf8 },
	{5220, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5230, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5240, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5260, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5270, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5280, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5300, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5310, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5320, 0x1f, 0x90, 0x70, 0x02, 0x40, 0xf8 },
	{5500, 0x1f, 0x80, 0x70, 0x02, 0x10, 0x94 },
	{5510, 0x1f, 0x80, 0x70, 0x02, 0x10, 0x94 },
	{5520, 0x1f, 0x70, 0x70, 0x02, 0x70, 0x94 },
	{5540, 0x1f, 0x90, 0x60, 0x02, 0x10, 0x94 },
	{5550, 0x1f, 0x90, 0x60, 0x02, 0x10, 0x94 },
	{5560, 0x1f, 0x90, 0x60, 0x02, 0x30, 0x84 },
	{5580, 0x1f, 0x90, 0x60, 0x02, 0x30, 0x84 },
	{5590, 0x1f, 0x90, 0x60, 0x02, 0x30, 0x84 },
	{5600, 0x1f, 0x90, 0x60, 0x02, 0x30, 0x84 },
	{5620, 0x1f, 0x90, 0x60, 0x02, 0x30, 0x84 },
	{5630, 0x1f, 0x90, 0x60, 0x02, 0x30, 0x74 },
	{5640, 0x1f, 0x90, 0x60, 0x02, 0x20, 0x74 },
	{5660, 0x1f, 0x90, 0x50, 0x02, 0x30, 0x74 },
	{5670, 0x1f, 0x90, 0x50, 0x02, 0x30, 0x74 },
	{5680, 0x1f, 0x90, 0x50, 0x02, 0x30, 0x74 },
	{5700, 0x1f, 0x90, 0x50, 0x02, 0x20, 0x74 },
	{5745, 0x1f, 0x90, 0x50, 0x02, 0x40, 0x64 },
	{5755, 0x1f, 0x90, 0x50, 0x02, 0x40, 0x64 },
	{5765, 0x1f, 0x90, 0x50, 0x02, 0x40, 0x64 },
	{5785, 0x1f, 0x90, 0x50, 0x02, 0x20, 0x64 },
	{5795, 0x1f, 0x90, 0x50, 0x02, 0x20, 0x64 },
	{5805, 0x1f, 0x90, 0x50, 0x02, 0x20, 0x64 },
	{5825, 0x1f, 0x90, 0x50, 0x02, 0x20, 0x64 }
};

STATIC chan_info_2063_sslpnphy_X17_aband_tweaks_t chan_info_2063_sslpnphy_X17_aband_tweaks[] = {
	{5170, 0xfff, 0x30, 0x6a, 0xe8, 0xf8, 0x20, 0x6a, 0xf9, 0xf8, 0x60, 0x60, 0xc0, 0xd0},
	{5180, 0xfff, 0x30, 0x6a, 0xe8, 0xf8, 0x20, 0x6a, 0xf9, 0xf8, 0x60, 0x60, 0xc0, 0xd0},
	{5190, 0xfff, 0x30, 0x6a, 0xe8, 0xf8, 0x20, 0x6a, 0xf9, 0xf8, 0x60, 0x60, 0xc0, 0xd0},
	{5200, 0xfff, 0x30, 0x6a, 0xd8, 0xf8, 0x20, 0x6a, 0xd8, 0xf8, 0x60, 0x60, 0xb0, 0xd0},
	{5210, 0xfff, 0x30, 0x6a, 0xc8, 0xf8, 0x20, 0x6a, 0xc8, 0xf8, 0x60, 0x60, 0xb0, 0xd0},
	{5220, 0xfff, 0x30, 0x6a, 0xc8, 0xf8, 0x20, 0x6a, 0xc8, 0xf8, 0x60, 0x60, 0xb0, 0xd0},
	{5230, 0xfff, 0x30, 0x6a, 0xc8, 0xf8, 0x20, 0x6a, 0xc8, 0xf8, 0x60, 0x60, 0xb0, 0xd0},
	{5240, 0xfff, 0x30, 0x6a, 0xc8, 0xf8, 0x20, 0x6a, 0xc8, 0xf8, 0x60, 0x60, 0xb0, 0xc0},
	{5260, 0xfff, 0x30, 0x6a, 0xb8, 0xf8, 0x20, 0x6a, 0xb8, 0xf8, 0x60, 0x60, 0xb0, 0xc0},
	{5280, 0xfff, 0x30, 0x6a, 0xb7, 0xf8, 0x20, 0x6a, 0xb7, 0xf8, 0x60, 0x60, 0xb0, 0xc0},
	{5300, 0xfff, 0x30, 0x6a, 0xb7, 0xf8, 0x20, 0x6a, 0xb7, 0xf8, 0x60, 0x60, 0xb0, 0xc0},
	{5320, 0xfff, 0x30, 0x6a, 0xb7, 0xf8, 0x20, 0x6a, 0xb7, 0xf8, 0x60, 0x60, 0xb0, 0xc0},

	{5500, 0xfff, 0x30, 0x6a, 0x66, 0x94, 0x30, 0x6a, 0x66, 0x94, 0x30, 0x60, 0x60, 0x90},
	{5520, 0xfff, 0x30, 0x6a, 0x66, 0x94, 0x30, 0x6a, 0x66, 0x94, 0x30, 0x60, 0x60, 0x90},
	{5540, 0xfff, 0x30, 0x6a, 0x56, 0x94, 0x30, 0x6a, 0x56, 0x94, 0x30, 0x60, 0x50, 0x90},
	{5560, 0xfff, 0x30, 0x6a, 0x56, 0x84, 0x30, 0x6a, 0x56, 0x84, 0x30, 0x60, 0x50, 0x80},
	{5580, 0xfff, 0x30, 0x6a, 0x56, 0x84, 0x30, 0x6a, 0x56, 0x84, 0x30, 0x60, 0x50, 0x80},
	{5600, 0xfff, 0x30, 0x6a, 0x56, 0x84, 0x30, 0x6a, 0x56, 0x84, 0x30, 0x60, 0x50, 0x80},
	{5620, 0xfff, 0x30, 0x6a, 0x56, 0x84, 0x30, 0x6a, 0x56, 0x84, 0x30, 0x60, 0x50, 0x80},
	{5640, 0xfff, 0x30, 0x6a, 0x56, 0x74, 0x30, 0x6a, 0x36, 0x74, 0x30, 0x60, 0x30, 0x70},
	{5660, 0xfff, 0x30, 0x6a, 0x56, 0x74, 0x30, 0x6a, 0x36, 0x74, 0x30, 0x60, 0x30, 0x70},
	{5680, 0xfff, 0x30, 0x6a, 0x56, 0x74, 0x30, 0x6a, 0x36, 0x74, 0x30, 0x60, 0x30, 0x70},
	{5700, 0xfff, 0x30, 0x6a, 0x56, 0x74, 0x40, 0x6a, 0x36, 0x74, 0x20, 0x60, 0x30, 0x70},

	{5745, 0xfff, 0x30, 0x6a, 0xe8, 0x48, 0x40, 0x6a, 0x36, 0x04, 0x20, 0x60, 0x30, 0x00},
	{5765, 0xfff, 0x30, 0x6a, 0xe8, 0x48, 0x40, 0x6a, 0x36, 0x04, 0x20, 0x60, 0x30, 0x00},
	{5785, 0xfff, 0x30, 0x6a, 0xe8, 0x48, 0x40, 0x6a, 0x36, 0x04, 0x20, 0x60, 0x30, 0x00},
	{5805, 0xfff, 0x30, 0x6a, 0xe8, 0x48, 0x40, 0x6a, 0x36, 0x04, 0x20, 0x60, 0x30, 0x00},
	{5825, 0xfff, 0x30, 0x6a, 0xe8, 0x48, 0x40, 0x6a, 0x36, 0x04, 0x20, 0x60, 0x30, 0x00}
};

STATIC chan_info_2063_sslpnphy_aband_tweaks_t chan_info_2063_sslpnphy_aband_tweaks[] = {
	{5170, 0x1e3f, 0x40, 0x68, 0xf9, 0xf9, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5180, 0x1e3f, 0x40, 0x68, 0xf9, 0xf9, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5190, 0x1e3f, 0x40, 0x68, 0xf9, 0xf9, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5200, 0x1e3f, 0x40, 0x68, 0xf8, 0xf9, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5210, 0x1e3f, 0x40, 0x68, 0xf7, 0xf8, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5220, 0x1e3f, 0x40, 0x68, 0xf7, 0xf8, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5230, 0x1e3f, 0x40, 0x68, 0xf7, 0xf8, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5240, 0x1e3f, 0x40, 0x68, 0xf7, 0xf8, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5260, 0x1e3f, 0x40, 0x68, 0xf7, 0xf8, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5280, 0x1e3f, 0x40, 0x68, 0xe6, 0xf8, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5300, 0x1e3f, 0x40, 0x68, 0xe6, 0xf7, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5320, 0x1e3f, 0x40, 0x68, 0xe6, 0xf6, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},

	{5500, 0x1e3f, 0x10, 0x68, 0xb6, 0xf4, 0xb8, 0x79, 0x77, 0x90, 0x30, 0x80, 0x80, 0x2, 0xee},
	{5520, 0x1e3f, 0x10, 0x68, 0xb6, 0xf4, 0xb8, 0x79, 0x77, 0x90, 0x30, 0x80, 0x80, 0x2, 0xee},
	{5540, 0x1e3f, 0x10, 0x68, 0xa6, 0xf4, 0xb8, 0x79, 0x77, 0x90, 0x30, 0x80, 0x80, 0x2, 0xee},
	{5560, 0x1e3f, 0x30, 0x68, 0x96, 0xe4, 0xb8, 0x79, 0x77, 0x90, 0x30, 0x80, 0x80, 0x2, 0xee},
	{5580, 0x1e3f, 0x30, 0x68, 0x96, 0xe4, 0xb8, 0x79, 0x77, 0x90, 0x30, 0x80, 0x80, 0x2, 0xee},
	{5600, 0x1e3f, 0x30, 0x68, 0x96, 0xd4, 0xb8, 0x79, 0x77, 0x90, 0x30, 0x80, 0x80, 0x2, 0xee},
	{5620, 0x1e3f, 0x30, 0x68, 0xa6, 0xd4, 0xb8, 0x79, 0x77, 0x90, 0x30, 0x80, 0x80, 0x2, 0xee},
	{5640, 0x1e3f, 0x20, 0x68, 0x96, 0xd4, 0xb8, 0x79, 0x77, 0x90, 0x30, 0x80, 0x80, 0x2, 0xee},
	{5660, 0x1e3f, 0x30, 0x68, 0x90, 0xe2, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5680, 0x1e3f, 0x30, 0x68, 0x90, 0xe2, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5700, 0x1e3f, 0x20, 0x68, 0x80, 0xd2, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},

	{5745, 0x1e3f, 0x40, 0x68, 0xe6, 0xf6, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5765, 0x1e3f, 0x40, 0x68, 0xe6, 0xf6, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5785, 0x1e3f, 0x40, 0x68, 0xe6, 0xf6, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5805, 0x1e3f, 0x40, 0x68, 0xe6, 0xf6, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee},
	{5825, 0x1e3f, 0x40, 0x68, 0xe6, 0xf6, 0xb8, 0x79, 0x77, 0x90, 0x50, 0x80, 0x80, 0x2, 0xee}
	};

STATIC chan_info_2063_sslpnphy_t chan_info_2063_sslpnphy_aband[] = {
	{  34, 5170, 0xfa, 0x05, 0x0d, 0x05, 0x05, 0x55, 0x0F, 0x08,
	0x0F, 0x07, 0x77, 0xdd, 0xCF, 0x70, 0x10, 0xF3, 0x00 },
	{  36, 5180, 0xf9, 0x05, 0x0d, 0x05, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xdd, 0xCF, 0x70, 0x10, 0xF3, 0x55 },
	{  38, 5190, 0xf9, 0x05, 0x0d, 0x05, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xdd, 0xCF, 0x70, 0x10, 0xF3, 0x55 },
	{  40, 5200, 0xf9, 0x05, 0x0d, 0x05, 0x05, 0x55, 0x0F, 0x07,
	0x0F, 0x07, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0x55 },
	{  42, 5210, 0xf9, 0x05, 0x0d, 0x05, 0x05, 0x55, 0x0E, 0x06,
	0x0E, 0x06, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0x55 },
	{  44, 5220, 0xf9, 0x05, 0x0d, 0x05, 0x05, 0x55, 0x0E, 0x06,
	0x0E, 0x06, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0x55 },
	{  46, 5230, 0xf9, 0x05, 0x0d, 0x05, 0x05, 0x55, 0x0E, 0x06,
	0x0E, 0x06, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0x55 },
	{  48, 5240, 0xf9, 0x04, 0x0C, 0x05, 0x05, 0x55, 0x0D, 0x05,
	0x0D, 0x05, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0x55 },
	{  52, 5260, 0xf8, 0x04, 0x0C, 0x05, 0x05, 0x55, 0x0C, 0x04,
	0x0C, 0x04, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0x55 },
	{  54, 5270, 0xf8, 0x04, 0x0C, 0x05, 0x05, 0x55, 0x0C, 0x04,
	0x0C, 0x04, 0x77, 0xdd, 0xBF, 0x70, 0x10, 0xF3, 0x55 },
	{  56, 5280, 0xf8, 0x04, 0x0b, 0x05, 0x05, 0x55, 0x0B, 0x03,
	0x0B, 0x03, 0x77, 0xdd, 0xAF, 0x70, 0x10, 0xF3, 0x55 },
	{  60, 5300, 0xf8, 0x03, 0x0b, 0x05, 0x05, 0x55, 0x0A, 0x02,
	0x0A, 0x02, 0x77, 0xdd, 0xAF, 0x60, 0x10, 0xF3, 0x55 },
	{  62, 5310, 0xf8, 0x03, 0x0b, 0x05, 0x05, 0x55, 0x0A, 0x02,
	0x0A, 0x02, 0x77, 0xdd, 0xAF, 0x60, 0x10, 0xF3, 0x55 },
	{  64, 5320, 0xf7, 0x03, 0x0a, 0x05, 0x05, 0x55, 0x09, 0x01,
	0x09, 0x01, 0x77, 0xdd, 0xAF, 0x60, 0x10, 0xF3, 0x55 },
	{ 100, 5500, 0xf4, 0x01, 0x06, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x8F, 0x50, 0x00, 0x03, 0x66 },
	{ 102, 5510, 0xf4, 0x01, 0x06, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x8F, 0x50, 0x00, 0x03, 0x66 },
	{ 104, 5520, 0xf4, 0x01, 0x06, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0x66 },
	{ 108, 5540, 0xf3, 0x01, 0x05, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0x66 },
	{ 110, 5550, 0xf3, 0x01, 0x05, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0x66 },
	{ 112, 5560, 0xf3, 0x01, 0x05, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0x66 },
	{ 116, 5580, 0xf3, 0x00, 0x05, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0x66 },
	{ 118, 5590, 0xf3, 0x00, 0x04, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0x66 },
	{ 120, 5600, 0xf3, 0x00, 0x04, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x7F, 0x50, 0x00, 0x03, 0x66 },
	{ 124, 5620, 0xf3, 0x00, 0x04, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0x66 },
	{ 126, 5630, 0xf3, 0x00, 0x04, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0x66 },
	{ 128, 5640, 0xf3, 0x00, 0x03, 0x06, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0x66 },
	{ 132, 5660, 0xf2, 0x00, 0x03, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0x77 },
	{ 134, 5670, 0xf2, 0x00, 0x03, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0x77 },
	{ 136, 5680, 0xf2, 0x00, 0x03, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x6F, 0x40, 0x00, 0x03, 0x77 },
	{ 140, 5700, 0xf2, 0x00, 0x02, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x5F, 0x40, 0x00, 0x03, 0x77 },
	{ 149, 5745, 0xf1, 0x00, 0x01, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x5F, 0x40, 0x00, 0x03, 0x77 },
	{ 151, 5755, 0xf1, 0x00, 0x01, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x5F, 0x40, 0x00, 0x03, 0x77 },
	{ 153, 5765, 0xf1, 0x00, 0x01, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x4F, 0x30, 0x00, 0x03, 0x77 },
	{ 157, 5785, 0xf0, 0x00, 0x01, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x4F, 0x30, 0x00, 0x03, 0x77 },
	{ 159, 5795, 0xf0, 0x00, 0x01, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x4F, 0x30, 0x00, 0x03, 0x77 },
	{ 161, 5805, 0xf0, 0x00, 0x00, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x4F, 0x30, 0x00, 0x03, 0x77 },
	{ 165, 5825, 0xf0, 0x00, 0x00, 0x07, 0x05, 0x55, 0x00, 0x00,
	0x00, 0x00, 0x77, 0xdd, 0x3F, 0x30, 0x00, 0x03, 0x77 }
};
#endif /* BAND5G */

/* Autogenerated by 2063_chantbl_tcl2c.tcl */
STATIC chan_info_2063_sslpnphy_t BCMROMDATA(chan_info_2063_sslpnphy)[] = {
	{1, 2412, 0xff, 0x3c, 0x3c, 0x04, 0x0e, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x0c },
	{2, 2417, 0xff, 0x3c, 0x3c, 0x04, 0x0e, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x0b },
	{3, 2422, 0xff, 0x3c, 0x3c, 0x04, 0x0e, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x09 },
	{4, 2427, 0xff, 0x2c, 0x2c, 0x04, 0x0d, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x08 },
	{5, 2432, 0xff, 0x2c, 0x2c, 0x04, 0x0d, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x07 },
	{6, 2437, 0xff, 0x2c, 0x2c, 0x04, 0x0c, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x06 },
	{7, 2442, 0xff, 0x2c, 0x2c, 0x04, 0x0b, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x05 },
	{8, 2447, 0xff, 0x2c, 0x2c, 0x04, 0x0b, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x04 },
	{9, 2452, 0xff, 0x1c, 0x1c, 0x04, 0x0a, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x04 },
	{10, 2457, 0xff, 0x1c, 0x1c, 0x04, 0x09, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x03 },
	{11, 2462, 0xfe, 0x1c, 0x1c, 0x04, 0x08, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x03 },
	{12, 2467, 0xfe, 0x1c, 0x1c, 0x04, 0x07, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x02 },
	{13, 2472, 0xfe, 0x1c, 0x1c, 0x04, 0x06, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x02 },
	{14, 2484, 0xfe, 0x0c, 0x0c, 0x04, 0x02, 0x55, 0x05, 0x05, 0x05, 0x05,
	0x77, 0x44, 0x80, 0x80, 0x70, 0xf3, 0x01 }
};

sslpnphy_radio_regs_t WLBANDINITDATA(sslpnphy_radio_regs_2063)[] = {
	{ 0x4000,		0,		0 },
	{ 0x800A,		0x1,		0 },
	{ 0x4010,		0,		0 },
	{ 0x4011,		0,		0 },
	{ 0x4012,		0,		0 },
	{ 0x4013,		0,		0 },
	{ 0x4014,		0,		0 },
	{ 0x4015,		0,		0 },
	{ 0x4016,		0,		0 },
	{ 0x4017,		0,		0 },
	{ 0x4018,		0,		0 },
	{ 0xC01C,		0xe8,		0xd4 },
	{ 0xC01D,		0xa7,		0x53 },
	{ 0xC01F,		0xf0,		0xf },
	{ 0xC021,		0x5e,		0x5e },
	{ 0xC022,		0x7e,		0x7e },
	{ 0xC023,		0xf0,		0xf0 },
	{ 0xC026,		0x2,		0x2 },
	{ 0xC027,		0x7f,		0x7f },
	{ 0xC02A,		0xc,		0xc },
	{ 0x802C,		0x3c,		0x3f },
	{ 0x802D,		0xfc,		0xfe },
	{ 0xC032,		0x8,		0x8 },
	{ 0xC036,		0x60,		0x60 },
	{ 0xC03A,		0x30,		0x30 },
	{ 0xC03D,		0xc,		0xb },
	{ 0xC03E,		0x10,		0xf },
	{ 0xC04C,		0x3d,		0xfd },
	{ 0xC053,		0x2,		0x2 },
	{ 0xC057,		0x56,		0x56 },
	{ 0xC076,		0xf7,		0xf7 },
	{ 0xC0B2,		0xf0,		0xf0 },
	{ 0xC0C4,		0x71,		0x71 },
	{ 0xC0C5,		0x71,		0x71 },
	{ 0x80CF,		0xf0,		0x30 },
	{ 0xC0DF,		0x77,		0x77 },
	{ 0xC0E3,		0x3,		0x3 },
	{ 0xC0E4,		0xf,		0xf },
	{ 0xC0E5,		0xf,		0xf },
	{ 0xC0EC,		0x77,		0x77 },
	{ 0xC0EE,		0x77,		0x77 },
	{ 0xC0F3,		0x4,		0x4 },
	{ 0xC0F7,		0x9,		0x9 },
	{ 0x810B,		0,		0x4 },
	{ 0xC11D,		0x3,		0x3 },
	{ 0xFFFF,		0,		0}
};

#ifdef ROMTERMPHY
typedef struct {
	/* TX IQ LO cal results */
	uint16 txiqlocal_bestcoeffs[11];
	uint txiqlocal_bestcoeffs_valid;
	/* PAPD results */
	uint32 papd_compdelta_tbl[64];
	uint papd_table_valid;
	uint16 analog_gain_ref, lut_begin, lut_end, lut_step, rxcompdbm, papdctrl;

	/* RX IQ cal results */
	uint16 rxiqcal_coeffa0;
	uint16 rxiqcal_coeffb0;
	uint16 rxiq_enable;
	uint8 rxfe;
	uint8 loopback2, loopback1;

} sslpnphy_cal_results_t;

#endif /* ROMTERMPHY */

/* %%%%%%%%%%%%%%%%%%%%%%%% */
/* SSLPNPHY local functions */
/* %%%%%%%%%%%%%%%%%%%%%%%% */

static void wlc_sslpnphy_reset_auxadc(phy_info_t *pi);
static void wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(phy_info_t *pi);

static void wlc_sslpnphy_load_filt_coeff(phy_info_t *pi, uint16 reg_address,
	uint16 *coeff_val, uint count);
STATIC void wlc_sslpnphy_set_radio_loft(phy_info_t *pi, uint8 ei0,
	uint8 eq0, uint8 fi0, uint8 fq0);
static void wlc_sslpnphy_restore_calibration_results(phy_info_t *pi);
static void wlc_sslpnphy_restore_papd_calibration_results(phy_info_t *pi);
void wlc_sslpnphy_periodic_cal(phy_info_t *pi);	/* nonstatic in PHYBOM */

static void wlc_sslpnphy_noise_init(phy_info_t *pi);

static void wlc_sslpnphy_set_rx_iq_comp(phy_info_t *pi, uint16 a0, uint16 b0);
static void wlc_sslpnphy_get_rx_iq_comp(phy_info_t *pi, uint16 *a0, uint16 *b0);

STATIC void BCMROMFN(wlc_sslpnphy_pktengtx)(wlc_phy_t *ppi, wl_pkteng_t *pkteng,
	uint8 rate,	struct ether_addr *sa, uint32 wait_delay);

static void wlc_sslpnphy_papd_cal_txpwr(phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl,
	bool frcTxGnCtrl,
	uint16 frcTxidx);

STATIC void BCMROMFN(wlc_sslpnphy_set_rx_gain_by_distribution)(phy_info_t *pi,
	uint16 pga, uint16 biq2, uint16 pole1, uint16 biq1, uint16 tia, uint16 lna2,
	uint16 lna1);
STATIC void BCMROMFN(wlc_sslpnphy_set_swctrl_override)(phy_info_t *pi, uint8 indx);
void BCMROMFN(wlc_sslpnphy_get_radio_loft)(phy_info_t *pi, uint8 *ei0,
	uint8 *eq0, uint8 *fi0, uint8 *fq0);	/* nonstatic in PHYBOM */

STATIC uint32 BCMROMFN(wlc_lpphy_qdiv_roundup)(uint32 divident, uint32 divisor, uint8 precision);
STATIC void BCMROMFN(wlc_sslpnphy_set_pa_gain)(phy_info_t *pi, uint16 gain);
STATIC void BCMROMFN(wlc_sslpnphy_set_trsw_override)(phy_info_t *pi, bool tx, bool rx);
STATIC void BCMROMFN(wlc_sslpnphy_stop_ddfs)(phy_info_t *pi);
STATIC void BCMROMFN(wlc_sslpnphy_set_bbmult)(phy_info_t *pi, uint8 m0);
STATIC uint8 BCMROMFN(wlc_sslpnphy_get_bbmult)(phy_info_t *pi);

STATIC void BCMROMFN(wlc_sslpnphy_get_tx_gain)(phy_info_t *pi,  sslpnphy_txgains_t *gains);
STATIC void BCMROMFN(wlc_sslpnphy_set_tx_gain_override)(phy_info_t *pi, bool bEnable);
STATIC void BCMROMFN(wlc_sslpnphy_toggle_afe_pwdn)(phy_info_t *pi);
STATIC void BCMROMFN(wlc_sslpnphy_set_tx_gain)(phy_info_t *pi,  sslpnphy_txgains_t *target_gains);
STATIC void BCMROMFN(wlc_sslpnphy_set_rx_gain)(phy_info_t *pi, uint32 gain);
STATIC void BCMROMFN(wlc_sslpnphy_saveIntpapdlut)(phy_info_t *pi, int8 Max, int8 Min,
	uint32 *papdIntlut, uint8 *papdIntlutVld);
STATIC void BCMROMFN(wlc_sslpnphy_GetpapdMaxMinIdxupdt)(phy_info_t *pi,
	int8 *maxUpdtIdx, int8 *minUpdtIdx);
STATIC int BCMROMFN(wlc_sslpnphy_aux_adc_accum)(phy_info_t *pi, uint32 numberOfSamples,
    uint32 waitTime, int32 *sum, int32 *prod);
STATIC void BCMROMFN(wlc_sslpnphy_rx_pu)(phy_info_t *pi, bool bEnable);


static void wlc_sslpnphy_afe_clk_init(phy_info_t *pi, uint8 mode);
void wlc_sslpnphy_set_tx_pwr_ctrl(phy_info_t *pi, uint16 mode);	/* nonstatic in PHYBOM */
STATIC bool BCMROMFN(wlc_sslpnphy_calc_rx_iq_comp)(phy_info_t *pi,  uint16 num_samps);
#ifdef ROMTERMPHY
void wlc_sslpnphy_write_shm_tssiCalEn(phy_info_t *pi, bool tssiCalEnVal);
#endif
/* %%%%%%%%%%%%%%%%%%%% */
/*  power control	*/
/* %%%%%%%%%%%%%%%%%%%% */

static bool wlc_sslpnphy_txpwr_srom_read(phy_info_t *pi);
static void wlc_sslpnphy_store_tbls(phy_info_t *pi);
#ifndef PPR_API
static void wlc_sslpnphy_txpwr_srom_convert(uint8 *srom_max, uint16 *pwr_offset, uint8 tmp_max_pwr,
	uint8 rate_start, uint8 rate_end);
#else
static void wlc_sslpnphy_txpwr_srom_convert(int8 *srom_max, uint16 *pwr_offset, uint8 tmp_max_pwr,
	uint rate_start, uint rate_end);
#endif /* PPR_API */

STATIC uint16 wlc_sslpnphy_get_pa_gain(phy_info_t *pi);

static void wlc_sslpnphy_lock_ucode_phyreg(phy_info_t *pi, int wait);
static void wlc_sslpnphy_unlock_ucode_phyreg(phy_info_t *pi);

typedef enum {
	SSLPNPHY_TSSI_PRE_PA,
	SSLPNPHY_TSSI_POST_PA,
	SSLPNPHY_TSSI_EXT
} sslpnphy_tssi_mode_t;

STATIC void wlc_sslpnphy_idle_tssi_est(phy_info_t *pi);
STATIC uint16 sslpnphy_iqlocc_write(phy_info_t *pi, uint8 data);
STATIC void wlc_sslpnphy_run_samples(phy_info_t *pi, uint16 num_samps, uint16 num_loops,
                              uint16 wait, bool iqcalmode);
void wlc_sslpnphy_start_tx_tone(phy_info_t *pi, int32 f_kHz, uint16 max_val,
                                bool iqcalmode);
STATIC bool wlc_sslpnphy_iqcal_wait(phy_info_t *pi);
STATIC void wlc_sslpnphy_clear_trsw_override(phy_info_t *pi);
STATIC void wlc_sslpnphy_tx_iqlo_cal(phy_info_t *pi, sslpnphy_txgains_t *target_gains,
                              sslpnphy_cal_mode_t cal_mode, bool keep_tone);
void wlc_sslpnphy_get_tx_iqcc(phy_info_t *pi, uint16 *a, uint16 *b);	/* nonstatic in PHYBOM */
uint16 wlc_sslpnphy_get_tx_locc(phy_info_t *pi);	/* nonstatic in PHYBOM */

STATIC void wlc_sslpnphy_set_tx_filter_bw(phy_info_t *pi, uint16 bw);
STATIC void wlc_sslpnphy_papd_cal_setup_cw(phy_info_t *pi);
STATIC void wlc_sslpnphy_papd_cal_core(phy_info_t *pi, sslpnphy_papd_cal_type_t calType,
                                bool rxGnCtrl, uint8 num_symbols4lpgn, bool init_papd_lut,
                                uint16 papd_bbmult_init, uint16 papd_bbmult_step,
                                bool papd_lpgn_ovr, uint16 LPGN_I, uint16 LPGN_Q);
STATIC uint32 wlc_sslpnphy_papd_rxGnCtrl(phy_info_t *pi, sslpnphy_papd_cal_type_t cal_type,
                                  bool frcRxGnCtrl, uint8 CurTxGain);
STATIC void InitIntpapdlut(uint8 Max, uint8 Min, uint8 *papdIntlutVld);
STATIC void wlc_sslpnphy_compute_delta(phy_info_t *pi);
STATIC void genpapdlut(phy_info_t *pi, uint32 *papdIntlut, uint8 *papdIntlutVld);
STATIC void wlc_sslpnphy_papd_cal(phy_info_t *pi, sslpnphy_papd_cal_type_t cal_type,
                           sslpnphy_txcalgains_t *txgains, bool frcRxGnCtrl,
                           uint16 num_symbols, uint8 papd_lastidx_search_mode);
STATIC void wlc_sslpnphy_vbatsense_papd_cal(phy_info_t *pi, sslpnphy_papd_cal_type_t cal_type,
                                     sslpnphy_txcalgains_t *txgains);
STATIC int8 wlc_sslpnphy_gain_based_psat_detect(phy_info_t *pi, sslpnphy_papd_cal_type_t cal_type,
                                    bool frcRxGnCtrl, sslpnphy_txcalgains_t *txgains,
                                    uint8 cur_pwr);
STATIC void wlc_sslpnphy_min_pd_search(phy_info_t *pi, sslpnphy_papd_cal_type_t cal_type,
                                bool frcRxGnCtrl, sslpnphy_txcalgains_t *txgains);
STATIC int8 wlc_sslpnphy_psat_detect(phy_info_t *pi, uint8 cur_index, uint8 cur_pwr);
STATIC void wlc_sslpnphy_run_ddfs(phy_info_t *pi, int i_on, int q_on, int incr1, int incr2,
                           int scale_index);
STATIC bool wlc_sslpnphy_rx_iq_cal(phy_info_t *pi, const sslpnphy_rx_iqcomp_t *iqcomp,
                            int iqcomp_sz, bool use_noise, bool tx_switch, bool rx_switch,
                            bool pa, int tx_gain_idx);
void wlc_sslpnphy_full_cal(phy_info_t *pi);

void wlc_sslpnphy_detection_disable(phy_info_t *pi, bool mode);
STATIC void wlc_sslpnphy_noise_fifo_init(phy_info_t *pi);
STATIC void wlc_sslpnphy_noise_measure_setup(phy_info_t *pi);
STATIC uint32 wlc_sslpnphy_get_rxiq_accum(phy_info_t *pi);
STATIC uint32 wlc_sslpnphy_abs_time(uint32 end, uint32 start);
STATIC void wlc_sslpnphy_noise_measure_time_window(phy_info_t *pi, uint32 window_time, uint32 *minpwr,
                                            uint32 *maxpwr, bool *measurement_valid);
STATIC uint32 wlc_sslpnphy_noise_fifo_min(phy_info_t *pi);
STATIC void wlc_sslpnphy_noise_fifo_avg(phy_info_t *pi, uint32 *avg_noise);
STATIC void wlc_sslpnphy_noise_measure_chg_listen_gain(phy_info_t *pi, int8 change_sign);
STATIC void wlc_sslpnphy_noise_measure_change_rxpo(phy_info_t *pi, uint32 avg_noise);
STATIC void wlc_sslpnphy_noise_measure_computeNf(phy_info_t *pi);
STATIC uint8 wlc_sslpnphy_rx_noise_lut(phy_info_t *pi, uint8 noise_val, uint8 ptr[][2], uint8 array_size);
STATIC void wlc_sslpnphy_reset_radioctrl_crsgain(phy_info_t *pi);

STATIC void wlc_sslpnphy_disable_pad(phy_info_t *pi);
#ifdef BAND5G
STATIC void wlc_sslpnphy_pll_aband_tune(phy_info_t *pi, uint8 channel);
#endif /* BAND5G */
STATIC void wlc_sslpnphy_papd_cal_txpwr(phy_info_t *pi, sslpnphy_papd_cal_type_t cal_type,
                                        bool frcRxGnCtrl, bool frcTxGnCtrl, uint16 frcTxidx);

STATIC void wlc_sslpnphy_get_rx_iq_comp(phy_info_t *pi, uint16 *a0, uint16 *b0);
void wlc_sslpnphy_periodic_cal(phy_info_t *pi);	/* nonstatic in PHYBOM */
STATIC void wlc_sslpnphy_restore_papd_calibration_results(phy_info_t *pi);

#ifdef PPR_API
#ifdef BAND5G
static void wlc_phy_txpwr_sromssn_read_5g_ppr_parameters(phy_info_t *pi);
#endif
static void wlc_phy_txpwr_sromssn_read_2g_ppr_parameters(phy_info_t *pi);
static void wlc_phy_txpwr_sromssn_apply_ppr_parameters(phy_info_t *pi, uint8 band, ppr_t *tx_srom_max_pwr);
static void wlc_phy_txpwr_sromssn_apply_2g_ppr_parameters(phy_info_t *pi, ppr_t *tx_srom_max_pwr);
#else
#ifdef BAND5G
static void srom_read_and_apply_5g_tx_ppr_parameters(phy_info_t *pi);
#endif /* BAND5G */
static void srom_read_and_apply_2g_tx_ppr_parameters(phy_info_t *pi);
#endif /* PPR_API */

/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */
/*  function implementation   					*/
/* %%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%%% */

static void
wlc_sslpnphy_reset_auxadc(phy_info_t *pi)
{
	/* reset auxadc */
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, auxadcCtrl, auxadcreset, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, auxadcCtrl, auxadcreset, 0)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(phy_info_t *pi)
{
	/* Reset radio ctrl and crs gain */
	PHY_REG_LIST_START
		PHY_REG_OR_ENTRY(SSLPNPHY, resetCtrl, 0x44)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, resetCtrl, 0x80)
	PHY_REG_LIST_EXECUTE(pi);
}

bool
wlc_phy_tpc_isenabled_sslpnphy(phy_info_t *pi)
{
	return SSLPNPHY_TX_PWR_CTRL_HW == wlc_sslpnphy_get_tx_pwr_ctrl(pi);
}

static void
wlc_sslpnphy_lock_ucode_phyreg(phy_info_t *pi, int wait)
{
	WL_MCTRL(pi, MCTL_PHYLOCK, MCTL_PHYLOCK);
	(void)R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol);

	OSL_DELAY(wait);
}

static void
wlc_sslpnphy_unlock_ucode_phyreg(phy_info_t *pi)
{
	(void) R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->phyversion);
	WL_MCTRL(pi, MCTL_PHYLOCK, 0);
}

bool
wlc_phy_attach_sslpnphy(phy_info_t *pi)
{
	int i;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific;

	pi->u.pi_sslpnphy = (phy_info_sslpnphy_t*)MALLOC(pi->sh->osh, sizeof(phy_info_sslpnphy_t));
	if (pi->u.pi_sslpnphy == NULL) {
	PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return FALSE;
	}
	bzero((char *)pi->u.pi_sslpnphy, sizeof(phy_info_sslpnphy_t));

	sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

	if (((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_NOPA) == 0) &&
	    !NORADIO_ENAB(pi->pubpi)) {
		pi->hwpwrctrl = TRUE;
		pi->hwpwrctrl_capable = TRUE;
	}

	/* Get xtal frequency from PMU */
	pi->xtalfreq = si_alp_clock(GENERIC_PHY_INFO(pi)->sih);
	ASSERT((PHY_XTALFREQ(pi->xtalfreq) % 1000) == 0);

	/* set papd_rxGnCtrl_init to 0 */
	sslpnphy_specific->sslpnphy_papd_rxGnCtrl_init = 0;

	WL_INFORM(("wl%d: %s: using %d.%d MHz xtalfreq for RF PLL\n",
		GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
		PHY_XTALFREQ(pi->xtalfreq) / 1000000, PHY_XTALFREQ(pi->xtalfreq) % 1000000));
#if !defined(ROMTERMPHY)
	pi->pi_fptr.init = wlc_phy_init_sslpnphy;
	pi->pi_fptr.calinit = wlc_phy_cal_init_sslpnphy;
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_sslpnphy;
	pi->pi_fptr.txpwrrecalc = wlc_sslpnphy_txpower_recalc_target;
#if defined(BCMDBG) || defined(WLTEST)
	pi->pi_fptr.longtrn = wlc_phy_sslpnphy_long_train;
#endif
	pi->pi_fptr.txiqccget = wlc_sslpnphy_get_tx_iqcc;
	pi->pi_fptr.txiqccset = wlc_sslpnphy_set_tx_iqcc;
	pi->pi_fptr.txloccget = wlc_sslpnphy_get_tx_locc;
	pi->pi_fptr.radioloftget = (radioloftgetfn_t)wlc_sslpnphy_get_radio_loft;
	pi->pi_fptr.phywatchdog = wlc_phy_watchdog_sslpnphy;
#if defined(WLTEST)
	pi->pi_fptr.carrsuppr = wlc_phy_carrier_suppress_sslpnphy;
	pi->pi_fptr.rxsigpwr = wlc_sslpnphy_rx_signal_power;
#endif
	pi->pi_fptr.detach = wlc_phy_detach_sslpnphy;
#endif /* PHYHAL */
	if (!wlc_sslpnphy_txpwr_srom_read(pi))
		return FALSE;
	wlc_sslpnphy_store_tbls(pi);

	/* Initialize default power indexes */
	for (i = 0; i <= LAST_5G_CHAN; i++) {
		sslpnphy_specific->sslpnphy_tssi_idx_ch[i] =  (i >= FIRST_5G_CHAN) ?
			SSLPNPHY_TX_PWR_CTRL_START_INDEX_5G : SSLPNPHY_TX_PWR_CTRL_START_INDEX_2G;
	}
#if defined(SSLPNPHYCAL_CACHING)
{
	int count;
	for (count = 0; count < SSLPNPHY_MAX_CAL_CACHE; count++) {
		wlc_phy_destroy_sslpnphy_chanctx(valid_channel_list[count]);
	}
}
#endif
	return TRUE;
}

void
wlc_phy_detach_sslpnphy(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)					    /* noise timer is removed in PHYHAL */
	MFREE(pi->sh->osh, pi->u.pi_sslpnphy, sizeof(phy_info_sslpnphy_t));
#else
	if (pi->phynoise_timer) {
		wl_free_timer(((wlc_info_t *)pi->wlc)->wl, pi->phynoise_timer);
		pi->phynoise_timer = NULL;
	}
#endif /* PHYHAL */

}

void
#if !defined(ROMTERMPHY)
wlc_sslpnphy_write_table(phy_info_t *pi, const phytbl_info_t *pti)
#else
wlc_sslpnphy_write_table(phy_info_t *pi, CONST phytbl_info_t *pti)
#endif /* PHYHAL */
{
	wlc_phy_write_table(pi, pti, SSLPNPHY_TableAddress,
	                    SSLPNPHY_TabledataHi, SSLPNPHY_TabledataLo);
}

void
#if !defined(ROMTERMPHY)
wlc_sslpnphy_read_table(phy_info_t *pi, phytbl_info_t *pti)
#else
wlc_sslpnphy_read_table(phy_info_t *pi, CONST phytbl_info_t *pti)
#endif /* PHYHAL */
{
	wlc_phy_read_table(pi, pti, SSLPNPHY_TableAddress,
	                   SSLPNPHY_TabledataHi, SSLPNPHY_TabledataLo);
}

static uint
wlc_sslpnphy_init_radio_regs(phy_info_t *pi, sslpnphy_radio_regs_t *radioregs, uint16 core_offset)
{
	uint i = 0;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	do {
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			if (radioregs[i].address & 0x8000) {
				write_radio_reg(pi, (radioregs[i].address & 0x3fff) | core_offset,
					(uint16)radioregs[i].init_a);
			}
		} else {
			if (radioregs[i].address & 0x4000) {
				write_radio_reg(pi, (radioregs[i].address & 0x3fff) | core_offset,
					(uint16)radioregs[i].init_g);
			}
		}

		i++;
	} while (radioregs[i].address != 0xffff);
	if ((sslpnphy_specific->sslpnphy_fabid == 2) || (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
		write_radio_reg(pi, RADIO_2063_GRX_SP_6, 0);
		write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0x33);
		/* write_radio_reg(pi, RADIO_2063_LOCAL_OVR_1, 0xc0);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_4, 0x0);
		*/
	}
#ifdef SSLPNLOWPOWER
	write_radio_reg(pi, RADIO_2063_LOCAL_OVR_1, 0xc0);
	write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_4, 0x0);
#endif
	return i;
}


#if defined(WLTEST)

void
wlc_sslpnphy_reset_radio_loft(phy_info_t *pi)
{
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I, 0x88);
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q, 0x88);
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I, 0x88);
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q, 0x88);
}

#endif 
static void
wlc_sslpnphy_common_write_table(phy_info_t *pi, uint32 tbl_id,
	CONST void *tbl_ptr, uint32 tbl_len, uint32 tbl_width,
	uint32 tbl_offset);
static void
wlc_sslpnphy_load_tx_gain_table(phy_info_t *pi,
        CONST sslpnphy_tx_gain_tbl_entry * gain_table)
{
	uint16 j;
	uint32 val;
	uint16 pa_gain;

	if (CHSPEC_IS5G(pi->radio_chanspec))
		pa_gain = 0x70;
	else
		pa_gain = 0x70;
	if (CHSPEC_IS5G(pi->radio_chanspec) && (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
		pa_gain = 0x10;
	}
	/* 5356: External PA support */
	if ((CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) &&
		(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
		pa_gain = 0x20;
	}

	for (j = 0; j < 128; j++) {
		val = ((uint32)pa_gain << 24) |
			(gain_table[j].pad << 16) |
			(gain_table[j].pga << 8) |
			(gain_table[j].gm << 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_GAIN_OFFSET + j);

		val = (gain_table[j].dac << 28) |
			(gain_table[j].bb_mult << 20);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + j);
	}
}
static void
wlc_sslpnphy_load_rfpower(phy_info_t *pi)
{
	uint32 val;
	uint8 indx;
	uint8 ref_indx;
	uint8 gain_step;
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);

	/* The portion of the tx table where analog gains are constant excluding bbmult gain, */
	/* the rfpower lut should be filled up with 0-s as papd sees a constant analog gain.   */
	/* This is being achieved by firmware variable: ref_indx, the value of which is highly coupled */
	/* with the txgain tbl. An algo can be developed to automatically find that out but right now we */
	/* are hardcoding the index pos. */

	if (sdna_board_flag) {
	   ref_indx = 41;
	   gain_step = 40; /* corresponds to 0.25 gain step */
	} else {
	   ref_indx = 0;
	   gain_step = 32; /* corresponds to ~0.23 gain step */
	}

	for (indx = 0; indx < 128; indx++) {
	    if (indx < ref_indx)
			val = 0;
		else
			val = (indx - ref_indx) * gain_step / 10;

		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_PWR_OFFSET + indx);
	}
}
static uint16 wlc_sslpnphy_papd_fixpwr_calidx_find(phy_info_t *pi, int target_Calpwr)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	int current_txidx = wlc_sslpnphy_get_current_tx_pwr_idx(pi);
	int current_txpwr = wlc_sslpnphy_get_target_tx_pwr(pi);
	int initcal_txidx = sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_tgtpwr_idx;
	int initcal_txpwr = sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_tgtpwr;
	int initcal_papdCalidx = sslpnphy_specific->sslpnphy_papd_peakcurmode.tgtPapdCalpwr_idx;
	int initcal_papdCalpwr = sslpnphy_specific->sslpnphy_papd_peakcurmode.tgtPapdCalpwr;
	int idx_delta, idx_delta_pwr, idx_delta_calpwr;

	idx_delta_pwr = (initcal_txpwr - current_txpwr);
	idx_delta_calpwr = (initcal_papdCalpwr - target_Calpwr);
	idx_delta = (initcal_txidx - current_txidx);
	current_txidx = initcal_papdCalidx - idx_delta - idx_delta_pwr + idx_delta_calpwr;

	return (uint16)current_txidx;
}
static void wlc_sslpnphy_clear_papd_comptable(phy_info_t *pi)
{
	uint32 j;
	uint32 temp_offset[128];

	bzero(temp_offset, sizeof(temp_offset));
	for (j = 1; j < 128; j += 2)
		temp_offset[j] = 0x80000;

	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
		temp_offset, 128, 32, 0);
}

extern CONST uint16 sw_ctrl_tbl_rev02[];

/* initialize all the tables defined in auto-generated sslpnphytbls.c,
 * see sslpnphyprocs.tcl, proc sslpnphy_tbl_init
 */
static void
WLBANDINITFN(wlc_sslpnphy_restore_tbls)(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
			sslpnphy_specific->sslpnphy_gain_idx_2g, 152, 32, 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
			sslpnphy_specific->sslpnphy_gain_tbl_2g, 96, 16, 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
			sslpnphy_specific->sslpnphy_swctrl_lut_2g, 64, 16, 0);
	}
#ifdef BAND5G
	else {
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
			sslpnphy_specific->sslpnphy_gain_idx_5g, 152, 32, 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
			sslpnphy_specific->sslpnphy_gain_tbl_5g, 96, 16, 0);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
			sslpnphy_specific->sslpnphy_swctrl_lut_5g, 64, 16, 0);

	}
#endif
}

static void
wlc_sslpnphy_store_tbls(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
#ifdef BAND5G
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif /* BAND5G */
	bool x17_board_flag = ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID ||
		BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ? 1 : 0);
	bool N90_board_flag = ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90M_SSID ||
		BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90U_SSID) ? 1 : 0);

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		/* Allocate software memory for the 2G tables */
		sslpnphy_specific->sslpnphy_gain_idx_2g =
			(uint32 *)MALLOC(GENERIC_PHY_INFO(pi)->osh, 152 * sizeof(uint32));
		sslpnphy_specific->sslpnphy_gain_tbl_2g =
			(uint16 *)MALLOC(GENERIC_PHY_INFO(pi)->osh, 96 * sizeof(uint16));
		sslpnphy_specific->sslpnphy_swctrl_lut_2g =
			(uint16 *)MALLOC(GENERIC_PHY_INFO(pi)->osh, 64 * sizeof(uint16));

		bcopy(gain_idx_tbl_rev0, sslpnphy_specific->sslpnphy_gain_idx_2g,
			gain_idx_tbl_rev0_sz);
		bcopy(gain_tbl_rev0, sslpnphy_specific->sslpnphy_gain_tbl_2g,
			gain_tbl_rev0_sz);
		bcopy(sw_ctrl_tbl_rev0, sslpnphy_specific->sslpnphy_swctrl_lut_2g,
			sw_ctrl_tbl_rev0_sz);
		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {
			bcopy(sslpnphy_gain_idx_extlna_cmrxaci_tbl, sslpnphy_specific->sslpnphy_gain_idx_2g,
				sslpnphy_gain_idx_extlna_cmrxaci_tbl_sz);
			bcopy(sslpnphy_gain_extlna_cmrxaci_tbl, sslpnphy_specific->sslpnphy_gain_tbl_2g,
				sslpnphy_gain_extlna_cmrxaci_tbl_sz);
			if (x17_board_flag || N90_board_flag ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329MOTOROLA_SSID)) {
				bcopy(sslpnphy_gain_idx_extlna_2g_x17, sslpnphy_specific->sslpnphy_gain_idx_2g,
					sslpnphy_gain_idx_extlna_2g_x17_sz);
				bcopy(sslpnphy_gain_tbl_extlna_2g_x17, sslpnphy_specific->sslpnphy_gain_tbl_2g,
					sslpnphy_gain_tbl_extlna_2g_x17_sz);

			}
			if ((x17_board_flag) || (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) ==
				BCM94329MOTOROLA_SSID)) {
				bcopy(sw_ctrl_tbl_rev0_olympic_x17_2g, sslpnphy_specific->sslpnphy_swctrl_lut_2g,
					sw_ctrl_tbl_rev0_olympic_x17_2g_sz);
			}
		}
#ifdef BAND5G
		/* Allocate software memory for the 5G tables */
		sslpnphy_specific->sslpnphy_gain_idx_5g =
			(uint32 *)MALLOC(GENERIC_PHY_INFO(pi)->osh, 152 * sizeof(uint32));
		sslpnphy_specific->sslpnphy_gain_tbl_5g =
			(uint16 *)MALLOC(GENERIC_PHY_INFO(pi)->osh, 96 * sizeof(uint16));
		sslpnphy_specific->sslpnphy_swctrl_lut_5g =
			(uint16 *)MALLOC(GENERIC_PHY_INFO(pi)->osh, 64 * sizeof(uint16));
		sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_midband = (sslpnphy_tx_gain_tbl_entry *)
			MALLOC(GENERIC_PHY_INFO(pi)->osh, 128 * 5 * sizeof(uchar));
		sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_hiband = (sslpnphy_tx_gain_tbl_entry *)
			MALLOC(GENERIC_PHY_INFO(pi)->osh, 128 * 5 * sizeof(uchar));

		/* Copy all rx tables */
		bcopy(dot11lpphy_rx_gain_init_tbls_A_tbl, sslpnphy_specific->sslpnphy_gain_idx_5g,
			dot11lpphy_rx_gain_init_tbls_A_tbl_sz);
		bcopy(gain_tbl_rev0, sslpnphy_specific->sslpnphy_gain_tbl_5g,
			gain_tbl_rev0_sz);
		bcopy(sw_ctrl_tbl_rev1_5Ghz_tbl, sslpnphy_specific->sslpnphy_swctrl_lut_5g,
			sw_ctrl_tbl_rev1_5Ghz_tbl_sz);
		if (x17_board_flag) {
			bcopy(sslpnphy_gain_idx_extlna_5g_x17, sslpnphy_specific->sslpnphy_gain_idx_5g,
				sslpnphy_gain_idx_extlna_5g_x17_sz);
			bcopy(sslpnphy_gain_tbl_extlna_5g_x17, sslpnphy_specific->sslpnphy_gain_tbl_5g,
				sslpnphy_gain_tbl_extlna_5g_x17_sz);
		}
		if ((x17_board_flag) || (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) ==
			BCM94329MOTOROLA_SSID)) {
			bcopy(sw_ctrl_tbl_rev0_olympic_x17_5g, sslpnphy_specific->sslpnphy_swctrl_lut_5g,
				sw_ctrl_tbl_rev0_olympic_x17_5g_sz);
		}

		/* Copy tx gain tables */
		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) {
			bcopy(dot11sslpnphy_2GHz_gaintable_rev0, sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_midband,
				128 * 5 * sizeof(uchar));
			bcopy(dot11sslpnphy_2GHz_gaintable_rev0, sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_hiband,
				128 * 5 * sizeof(uchar));
		} else {
			bcopy(dot11lpphy_5GHz_gaintable_MidBand, sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_midband,
				128 * 5 * sizeof(uchar));
			bcopy(dot11lpphy_5GHz_gaintable_HiBand, sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_hiband,
				128 * 5 * sizeof(uchar));
		}
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA) {
			bcopy(dot11lpphy_5GHz_gaintable_X17_ePA,
				sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_midband,
				128 * 5 * sizeof(uchar));
			bcopy(dot11lpphy_5GHz_gaintable_X17_ePA,
				sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_hiband,
				128 * 5 * sizeof(uchar));
		}
#endif /* BAND 5G */
	}
#ifdef BAND5G
	else {
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
			/* Allocate software memory for the 5G tables */
			sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_midband = (sslpnphy_tx_gain_tbl_entry *)
			    MALLOC(GENERIC_PHY_INFO(pi)->osh, 128 * 5 * sizeof(uchar));
			sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_hiband = (sslpnphy_tx_gain_tbl_entry *)
			    MALLOC(GENERIC_PHY_INFO(pi)->osh, 128 * 5 * sizeof(uchar));

			/* Copy tx gain tables */
			if (sdna_board_flag) {
				bcopy(dot11lpphy_5GHz_gaintable_4319_midband_gmboost,
					sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_midband,
					128 * 5 * sizeof(uchar));
				bcopy(dot11lpphy_5GHz_gaintable_4319_hiband_gmboost,
					sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_hiband,
					128 * 5 * sizeof(uchar));
			} else {
			bcopy(dot11lpphy_5GHz_gaintable_4319_midband,
			      sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_midband,
			      128 * 5 * sizeof(uchar));
			bcopy(dot11lpphy_5GHz_gaintable_4319_hiband,
			      sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_hiband,
			      128 * 5 * sizeof(uchar));
			}
		}
	}
#endif /* BAND 5G */


}
#ifdef PALM
/* This routine is to load palm's btcx fem. */
/* This routine is to load palm's btcx fem. */
/* force_update will ensure that the control lines are driven correctly */
void
wlc_load_bt_fem_combiner_sslpnphy(phy_info_t *pi, bool force_update)
{
	bool suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	uint8 band_idx;
	uint16 tempsense;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	/* Skip update when called during init, called repeatedly with the same value,
	 * and when the band isn't 2.4G
	 */
	if ((!force_update &&
		sslpnphy_specific->fem_combiner_target_state ==
		sslpnphy_specific->fem_combiner_current_state) ||
		CHSPEC_IS5G(pi->radio_chanspec)) {
		return;
	}

	if (!suspend)
		WL_SUSPEND_MAC_AND_WAIT(pi);
	if (sslpnphy_specific->fem_combiner_target_state) {
		tempsense = phy_reg_read(pi, SSLPNPHY_TempSenseCorrection);
		PHY_REG_WRITE(pi, SSLPNPHY, TempSenseCorrection, tempsense + 6);
		/* Program front-end control lines for 'combine' mode */
		si_pmu_res_4319_swctrl_war(GENERIC_PHY_INFO(pi)->sih, GENERIC_PHY_INFO(pi)->osh, TRUE);
		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319BHEMU3_SSID)
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
				sw_ctrl_tbl_rev02_shared_mlap_emu3_combiner, 32, 16, 0);
		else
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
				sw_ctrl_tbl_rev02_shared_mlap_combiner, 32, 16, 0);
	} else {
		/* Adjust power index back to neutal for 'bypass' mode */
		tempsense = phy_reg_read(pi, SSLPNPHY_TempSenseCorrection);
		PHY_REG_WRITE(pi, SSLPNPHY, TempSenseCorrection,
			tempsense > 6 ? tempsense - 6 : 0);
		/* Program front-end control lines for 'bypass' mode */
		si_pmu_res_4319_swctrl_war(GENERIC_PHY_INFO(pi)->sih, GENERIC_PHY_INFO(pi)->osh, FALSE);
		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319BHEMU3_SSID)
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
				sw_ctrl_tbl_rev02_shared_mlap_emu3, 32, 16, 0);
		else
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
				sw_ctrl_tbl_rev02_shared_mlap, 32, 16, 0);
	}

	sslpnphy_specific->fem_combiner_current_state = sslpnphy_specific->fem_combiner_target_state;

	if (!force_update) {
		/* force_update is really being used as, 'suppress_cal',
		 * so that we will skip calibration during init, and only
		 * calibrate when the user executes coex_profile iovar
		 */
		band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);
		wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(pi->radio_chanspec));
	}

	if (!suspend)
		WL_ENABLE_MAC(pi);
}
#endif /* PALM */
#if defined(ROMTERMPHY)
extern CONST dot11sslpnphytbl_info_t dot11sslpnphy_gain_tbl_40Mhz_smic;
#endif

static void
WLBANDINITFN(wlc_sslpnphy_tbl_init)(phy_info_t *pi)
{
	uint idx, val;
	uint16 j;
	uint32 tbl_val[2];
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif

	uint8 phybw40 = IS40MHZ(pi);
	bool x17_board_flag = ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID ||
		BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ? 1 : 0);
	bool N90_board_flag = ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90M_SSID ||
		BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90U_SSID) ? 1 : 0);
	bool ninja_board_flag = (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDELNA6L_SSID);
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);

	/* Resetting the txpwrctrl tbl */
	val = 0;
	for (j = 0; j < 703; j++) {
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, j);
	}
	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		for (idx = 0; idx < dot11sslpnphytbl_info_sz_rev0; idx++) {
			wlc_sslpnphy_write_table(pi, &dot11sslpnphytbl_info_rev0[idx]);
		}
		/* Restore the tables which were reclaimed */
		wlc_sslpnphy_restore_tbls(pi);
	} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		for (idx = 0; idx < dot11sslpnphytbl_info_sz_rev2; idx++)
			wlc_sslpnphy_write_table(pi, &dot11sslpnphytbl_info_rev3[idx]);
	} else {
		for (idx = 0; idx < dot11sslpnphytbl_info_sz_rev2; idx++)
			wlc_sslpnphy_write_table(pi, &dot11sslpnphytbl_info_rev2[idx]);

		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_FEM_BT) {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
#ifdef PALM
				if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) >= BCM94319WINDSOR_SSID) &&
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) <= BCM94319BHEMU3_SSID))
					wlc_load_bt_fem_combiner_sslpnphy(pi, TRUE);
				else
#endif /* PALM */
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
						sw_ctrl_tbl_rev02_shared, 64, 16, 0);
			}
#if (defined(PALM) && defined(BAND5G))
			else {
				if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319WINDSOR_SSID) &&
					(GENERIC_PHY_INFO(pi)->boardrev <= 0x1101))
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
						sw_ctrl_tbl_rev02_shared_mlap_windsor_5g,
						64, 16, 0);
				else if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319BHEMU3_SSID)
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
						sw_ctrl_tbl_rev02_shared_mlap_emu3_5g, 32, 16, 0);
				else
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
						sw_ctrl_tbl_rev02_shared_mlap_5g, 32, 16, 0);
			}
#endif /* PALM */
		}
		/* load NINJA board trsw table */
		if (ninja_board_flag) {
			wlc_sslpnphy_write_table(pi, &sw_ctrl_tbl_info_ninja6l);
		}
		if (sdna_board_flag) {
			int count;
			for (count = 0; count < 8; count++) {
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
					sw_ctrl_tbl_sdna, 8, 16, count*8);
			}
		}

		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDHMB_SSID ||
			BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319USBSDB_SSID ||
			BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDBREF_SSID) {
			wlc_sslpnphy_write_table(pi, &sw_ctrl_tbl_info_4319_sdio);
		}
	}

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		wlc_sslpnphy_load_tx_gain_table(pi, dot11sslpnphy_2GHz_gaintable_rev0);
		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
			(SSLPNREV_LT(pi->pubpi.phy_rev, 2))) {
			if (x17_board_flag || N90_board_flag || ninja_board_flag || sdna_board_flag ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329MOTOROLA_SSID)) {
				tbl_val[0] = 0xee;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
					tbl_val, 1, 32, 65);
			}
		}
#ifndef BAND5G
	}
#else
	} else {
		tbl_val[0] = 0x00E38208;
		tbl_val[1] = 0x00E38208;
		wlc_sslpnphy_common_write_table(pi, 12, tbl_val, 2, 32, 0);
		tbl_val[0] = 0xfa;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
			tbl_val, 1, 32, 64);
		if (x17_board_flag) {
			tbl_val[0] = 0xf2;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
				tbl_val, 1, 32, 65);
		}

	}
#endif /* BAND5G */
	if ((SSLPNREV_GE(pi->pubpi.phy_rev, 2)) && (phybw40 == 1)) {
		for (idx = 0; idx < dot11lpphy_rx_gain_init_tbls_40Mhz_sz; idx++) {
			wlc_sslpnphy_write_table(pi, &dot11lpphy_rx_gain_init_tbls_40Mhz[idx]);
		}
	}

	/* 5356 Specific 40Mhz Rx Gain Table */
	if ((SSLPNREV_IS(pi->pubpi.phy_rev, 3)) && (phybw40 == 1)) {
		for (idx = 0; idx < dot11lpphy_rx_gain_init_tbls_40Mhz_sz_rev3; idx++) {
			wlc_sslpnphy_write_table(pi, &dot11lpphy_rx_gain_init_tbls_40Mhz_rev3[idx]);
		}
	}

	/* 5356 SMIC changes */
	if ((SSLPNREV_IS(pi->pubpi.phy_rev, 3)) && (phybw40 == 1) &&
		(sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4)) {
		wlc_sslpnphy_write_table(pi, &dot11sslpnphy_gain_tbl_40Mhz_smic);
	}


#ifdef BAND5G
	/* 4319 (REV4) 5G gaintable (ninja board) */
	if (CHSPEC_IS5G(pi->radio_chanspec) && SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
		if (phybw40 == 1) {
			for (idx = 0; idx < dot11lpphy_rx_gain_extlna_tbls_A_40Mhz_sz; idx++) {
				wlc_sslpnphy_write_table(pi,
				     &dot11lpphy_rx_gain_extlna_tbls_A_40Mhz[idx]);
			}
		} else {
			for (idx = 0; idx < dot11lpphy_rx_gain_extlna_tbls_A_sz; idx++) {
				wlc_sslpnphy_write_table(pi,
				     &dot11lpphy_rx_gain_extlna_tbls_A[idx]);
			}
		}
	}
#endif

	wlc_sslpnphy_load_rfpower(pi);
	/* clear our PAPD Compensation table */
	wlc_sslpnphy_clear_papd_comptable(pi);
}

/* Reclaimable strings used by wlc_phy_txpwr_srom_read_sslpnphy */
static const char BCMATTACHDATA(rstr_opo)[] = "opo";
static const char BCMATTACHDATA(rstr_mcs5gpo)[] = "mcs5gpo";
#ifdef BAND5G
static const char BCMATTACHDATA(rstr_tri5gl)[] = "tri5gl";
static const char BCMATTACHDATA(rstr_tri5g)[] = "tri5g";
static const char BCMATTACHDATA(rstr_tri5gh)[] = "tri5gh";
static const char BCMATTACHDATA(rstr_bxa5g)[] = "bxa5g";
static const char BCMATTACHDATA(rstr_rxpo5g)[] = "rxpo5g";
static const char BCMATTACHDATA(rstr_rssismf5g)[] = "rssismf5g";
static const char BCMATTACHDATA(rstr_rssismc5g)[] = "rssismc5g";
static const char BCMATTACHDATA(rstr_rssisav5g)[] = "rssisav5g";
static const char BCMATTACHDATA(rstr_pa1maxpwr)[] = "pa1maxpwr";
static const char BCMATTACHDATA(rstr_pa1b_d)[] = "pa1b%d";
static const char BCMATTACHDATA(rstr_pa1lob_d)[] = "pa1lob%d";
static const char BCMATTACHDATA(rstr_pa1hib_d)[] = "pa1hib%d";
static const char BCMATTACHDATA(rstr_ofdmapo)[] = "ofdmapo";
static const char BCMATTACHDATA(rstr_ofdmalpo)[] = "ofdmalpo";
static const char BCMATTACHDATA(rstr_ofdm5gpo)[] = "ofdm5gpo";
static const char BCMATTACHDATA(rstr_ofdm5glpo)[] = "ofdm5glpo";
static const char BCMATTACHDATA(rstr_ofdm5ghpo)[] = "ofdm5ghpo";
static const char BCMATTACHDATA(rstr_maxp5g)[] = "maxp5g";
static const char BCMATTACHDATA(rstr_maxp5gl)[] = "maxp5gl";
static const char BCMATTACHDATA(rstr_maxp5gh)[] = "maxp5gh";
static const char BCMATTACHDATA(rstr_mcs5gpo0)[] = "mcs5gpo0";
static const char BCMATTACHDATA(rstr_mcs5gpo4)[] = "mcs5gpo4";
static const char BCMATTACHDATA(rstr_mcs5glpo0)[] = "mcs5glpo0";
static const char BCMATTACHDATA(rstr_mcs5glpo4)[] = "mcs5glpo4";
static const char BCMATTACHDATA(rstr_mcs5ghpo0)[] = "mcs5ghpo0";
static const char BCMATTACHDATA(rstr_mcs5ghpo4)[] = "mcs5ghpo4";
static const char BCMATTACHDATA(rstr_bwduppo)[] = "bwduppo";
#endif /* BAND5G */
static const char BCMATTACHDATA(rstr_pa1lomaxpwr)[] = "pa1lomaxpwr";
static const char BCMATTACHDATA(rstr_ofdmahpo)[] = "ofdmahpo";
static const char BCMATTACHDATA(rstr_pa1himaxpwr)[] = "pa1himaxpwr";
static const char BCMATTACHDATA(rstr_tri2g)[] = "tri2g";
static const char BCMATTACHDATA(rstr_bxa2g)[] = "bxa2g";
static const char BCMATTACHDATA(rstr_rxpo2g)[] = "rxpo2g";
static const char BCMATTACHDATA(rstr_cckdigfilttype)[] = "cckdigfilttype";
static const char BCMATTACHDATA(rstr_ofdmdigfilttype)[] = "ofdmdigfilttype";
static const char BCMATTACHDATA(rstr_rxpo2gchnflg)[] = "rxpo2gchnflg";
static const char BCMATTACHDATA(rstr_forcepercal)[] = "forcepercal";
static const char BCMATTACHDATA(rstr_rssismf2g)[] = "rssismf2g";
static const char BCMATTACHDATA(rstr_rssismc2g)[] = "rssismc2g";
static const char BCMATTACHDATA(rstr_rssisav2g)[] = "rssisav2g";
static const char BCMATTACHDATA(rstr_rssismf2g_low0)[] = "rssismf2g_low0";
static const char BCMATTACHDATA(rstr_rssismc2g_low1)[] = "rssismc2g_low1";
static const char BCMATTACHDATA(rstr_rssisav2g_low2)[] = "rssisav2g_low2";
static const char BCMATTACHDATA(rstr_rssismf2g_hi0)[] = "rssismf2g_hi0";
static const char BCMATTACHDATA(rstr_rssismc2g_hi1)[] = "rssismc2g_hi1";
static const char BCMATTACHDATA(rstr_rssisav2g_hi2)[] = "rssisav2g_hi2";
static const char BCMATTACHDATA(rstr_pa0maxpwr)[] = "pa0maxpwr";
static const char BCMATTACHDATA(rstr_pa0b_d)[] = "pa0b%d";
static const char BCMATTACHDATA(rstr_cckpo)[] = "cckpo";
static const char BCMATTACHDATA(rstr_ofdm2gpo)[] = "ofdm2gpo";
static const char BCMATTACHDATA(rstr_ofdmpo)[] = "ofdmpo";
static const char BCMATTACHDATA(rstr_mcs2gpo0)[] = "mcs2gpo0";
static const char BCMATTACHDATA(rstr_mcs2gpo1)[] = "mcs2gpo1";
static const char BCMATTACHDATA(rstr_mcs2gpo4)[] = "mcs2gpo4";
static const char BCMATTACHDATA(rstr_mcs2gpo5)[] = "mcs2gpo5";
static const char BCMATTACHDATA(rstr_mcs2gpo2)[] = "mcs2gpo2";
static const char BCMATTACHDATA(rstr_mcs2gpo3)[] = "mcs2gpo3";
static const char BCMATTACHDATA(rstr_5g_cga)[] = "5g_cga";
static const char BCMATTACHDATA(rstr_2g_cga)[] = "2g_cga";
static const char BCMATTACHDATA(rstr_tssi_min)[] = "tssi_min";
static const char BCMATTACHDATA(rstr_tssi_max)[] = "tssi_max";
static const char BCMATTACHDATA(rstr_nc_offset)[] = "nc_offset";

#if defined(ROMTERMPHY)
int
phy_getintvar_default(phy_info_t *pi, const char *name, int default_value)
{
#ifdef _MINOSL_
	return 0;
#else
	char *val = PHY_GETVAR(pi, name);
	if (val != NULL)
		return (bcm_strtoul(val, NULL, 0));

	return (default_value);
#endif /* _MINOSL_ */
}
#endif /* !ROMTERMPHY */

/* Read band specific data from the SROM */
static bool
BCMATTACHFN(wlc_sslpnphy_txpwr_srom_read)(phy_info_t *pi)
{
	char varname[32];
	int i;
#ifdef BAND5G
#endif /* BAND5G */
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	/* Fab specific Tuning */
	if (!(si_otp_fabid(GENERIC_PHY_INFO(pi)->sih, &sslpnphy_specific->sslpnphy_fabid_otp, TRUE) == BCME_OK))
	{
		WL_ERROR(("Reading fabid from otp failed.\n"));
	}

	/* Noise cal offset */
	sslpnphy_specific->sslpnphy_noise_cal_offset = (int8)PHY_GETINTVAR(pi, rstr_nc_offset);

	/* Optional TSSI limits */
	sslpnphy_specific->sslpnphy_tssi_max_pwr_nvram = (int8)PHY_GETINTVAR_DEFAULT(pi, rstr_tssi_max, 127);
	sslpnphy_specific->sslpnphy_tssi_min_pwr_nvram = (int8)PHY_GETINTVAR_DEFAULT(pi, rstr_tssi_min, -128);

#if !defined(ROMTERMPHY)
	pi->aa2g = (uint8)PHY_GETINTVAR(pi, "aa2g");
#endif /* PHYHAL */

/* Band specific setup */
#ifdef BAND5G
#if !defined(ROMTERMPHY)
	pi->aa5g = (uint8)PHY_GETINTVAR(pi, "aa5g");
#endif /* PHYHAL */

	/* TR switch isolation */
	sslpnphy_specific->sslpnphy_tr_isolation_low = (uint8)PHY_GETINTVAR(pi, rstr_tri5gl);
	sslpnphy_specific->sslpnphy_tr_isolation_mid = (uint8)PHY_GETINTVAR(pi, rstr_tri5g);
	sslpnphy_specific->sslpnphy_tr_isolation_hi = (uint8)PHY_GETINTVAR(pi, rstr_tri5gh);
	/* Board switch architecture */
	sslpnphy_specific->sslpnphy_bx_arch = (uint8)PHY_GETINTVAR(pi, rstr_bxa5g);

	/* Input power offset */
	sslpnphy_specific->sslpnphy_rx_power_offset_5g = (uint8)PHY_GETINTVAR(pi, rstr_rxpo5g);

	/* RSSI */
	sslpnphy_specific->sslpnphy_rssi_5g_vf = (uint8)PHY_GETINTVAR(pi, rstr_rssismf5g);
	sslpnphy_specific->sslpnphy_rssi_5g_vc = (uint8)PHY_GETINTVAR(pi, rstr_rssismc5g);
	sslpnphy_specific->sslpnphy_rssi_5g_gs = (uint8)PHY_GETINTVAR(pi, rstr_rssisav5g);

	/* PA coeffs */
	for (i = 0; i < 3; i++) {
		snprintf(varname, sizeof(varname), rstr_pa1b_d, i);
		pi->txpa_5g_mid[i] = (int16)PHY_GETINTVAR(pi, varname);
	}

	/* Low channels */
	for (i = 0; i < 3; i++) {
		snprintf(varname, sizeof(varname), rstr_pa1lob_d, i);
		pi->txpa_5g_low[i] = (int16)PHY_GETINTVAR(pi, varname);
	}

	/* High channels */
	for (i = 0; i < 3; i++) {
		snprintf(varname, sizeof(varname), rstr_pa1hib_d, i);
		pi->txpa_5g_hi[i] = (int16)PHY_GETINTVAR(pi, varname);
	}

#ifdef PPR_API
	wlc_phy_txpwr_sromssn_read_5g_ppr_parameters(pi);
#else /* !defined(PPR_API) */
	srom_read_and_apply_5g_tx_ppr_parameters(pi);
#endif /* PPR_API */

	for (i = 0; i < 24; i++) {
#if !defined(ROMTERMPHY)
		sslpnphy_specific->sslpnphy_cga_5g[i] = (int8)PHY_GETINTVAR_ARRAY(pi, rstr_5g_cga, i);
#else
		sslpnphy_specific->sslpnphy_cga_5g[i] = (int8)PHY_GETINTVAR_ARRAY(rstr_5g_cga, IOVT_UINT16, i);
#endif /* !defined(ROMTERMPHY) */
	}
#endif /* BAND5G */


	/* TR switch isolation */
	sslpnphy_specific->sslpnphy_tr_isolation_mid = (uint8)PHY_GETINTVAR(pi, rstr_tri2g);

	/* Board switch architecture */
	sslpnphy_specific->sslpnphy_bx_arch = (uint8)PHY_GETINTVAR(pi, rstr_bxa2g);

	/* Input power offset */
	sslpnphy_specific->sslpnphy_rx_power_offset = (uint8)PHY_GETINTVAR(pi, rstr_rxpo2g);

	sslpnphy_specific->sslpnphy_fabid = (uint8)PHY_GETINTVAR(pi, "fabid");

	/* Sslpnphy  filter select */
	sslpnphy_specific->sslpnphy_cck_filt_sel = (uint8)PHY_GETINTVAR(pi, rstr_cckdigfilttype);
	/* sslpnphy filter select */
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3) && (!sslpnphy_specific->sslpnphy_cck_filt_sel)) {
		/* fab id is already read, so it is safe to access
		 * sslpnphy_fabid_otp variable now.
		 */
		if (sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4) {
			sslpnphy_specific->sslpnphy_cck_filt_sel = 9;
		} else {
			sslpnphy_specific->sslpnphy_cck_filt_sel = 8;
		}
	}
	sslpnphy_specific->sslpnphy_ofdm_filt_sel = (uint8)PHY_GETINTVAR(pi, rstr_ofdmdigfilttype);

	/* Channel based selection for rxpo2g */
	sslpnphy_specific->sslpnphy_rxpo2gchnflg = (uint16)PHY_GETINTVAR(pi, rstr_rxpo2gchnflg);


	/* force periodic cal */
	sslpnphy_specific->sslpnphy_force_percal = (uint8)PHY_GETINTVAR(pi, rstr_forcepercal);

	/* RSSI */
	sslpnphy_specific->sslpnphy_rssi_vf = (uint8)PHY_GETINTVAR(pi, rstr_rssismf2g);
	sslpnphy_specific->sslpnphy_rssi_vc = (uint8)PHY_GETINTVAR(pi, rstr_rssismc2g);
	sslpnphy_specific->sslpnphy_rssi_gs = (uint8)PHY_GETINTVAR(pi, rstr_rssisav2g);
	sslpnphy_specific->sslpnphy_rssi_vf_lowtemp = sslpnphy_specific->sslpnphy_rssi_vf;
	sslpnphy_specific->sslpnphy_rssi_vc_lowtemp = sslpnphy_specific->sslpnphy_rssi_vc;
	sslpnphy_specific->sslpnphy_rssi_gs_lowtemp = sslpnphy_specific->sslpnphy_rssi_gs;

	sslpnphy_specific->sslpnphy_rssi_vf_hightemp = sslpnphy_specific->sslpnphy_rssi_vf;
	sslpnphy_specific->sslpnphy_rssi_vc_hightemp = sslpnphy_specific->sslpnphy_rssi_vc;
	sslpnphy_specific->sslpnphy_rssi_gs_hightemp = sslpnphy_specific->sslpnphy_rssi_gs;

	/* PA coeffs */
	for (i = 0; i < PWRTBL_NUM_COEFF; i++) {
		snprintf(varname, sizeof(varname), rstr_pa0b_d, i);
		pi->txpa_2g[i] = (int16)PHY_GETINTVAR(pi, varname);
	}
	if ((GENERIC_PHY_INFO(pi)->boardrev == 0x1307) || (GENERIC_PHY_INFO(pi)->boardrev == 0x1306)) {
		pi->txpa_2g[0] = 5779;
		pi->txpa_2g[1] = 64098;
		pi->txpa_2g[2] = 65140;
	}
	for (i = 0; i < PWRTBL_NUM_COEFF; i++) {
		pi->txpa_2g_low_temp[i] = pi->txpa_2g[i];
		pi->txpa_2g_high_temp[i] = pi->txpa_2g[i];
	}

#ifdef PPR_API
	wlc_phy_txpwr_sromssn_read_2g_ppr_parameters(pi);
#else /* !PPR_API */
	srom_read_and_apply_2g_tx_ppr_parameters(pi);
#endif /* PPR_API */

	for (i = 0; i < 14; i++) {
#if !defined(ROMTERMPHY)
		sslpnphy_specific->sslpnphy_cga_2g[i] = (int8)PHY_GETINTVAR_ARRAY(pi, rstr_2g_cga, i);
#else
		sslpnphy_specific->sslpnphy_cga_2g[i] = (int8)PHY_GETINTVAR_ARRAY(rstr_2g_cga, IOVT_UINT16, i);
#endif /* PHYHAL */
	}
	return TRUE;
}

#ifdef PPR_API

#ifdef BAND5G
static void
wlc_phy_txpwr_sromssn_read_5g_ppr_parameters(phy_info_t *pi)
{
	pi->ppr.sr_ssn.mcs5gpo = (uint32)PHY_GETINTVAR(pi, rstr_mcs5gpo);

	/* Max tx power */
	pi->ppr.sr_ssn.pa1maxpwr = (int8)PHY_GETINTVAR(pi, rstr_pa1maxpwr);

	/* Mid band channels */
	/* Extract 8 OFDM rates for mid channels */
	pi->ppr.sr_ssn.ofdmapo = (uint32)PHY_GETINTVAR(pi, rstr_ofdmapo);

	/* MCS32 power offset for each of the 5G sub-bands */
	pi->ppr.sr_ssn.maxp5g = (int8)PHY_GETINTVAR(pi, rstr_maxp5g);
	pi->ppr.sr_ssn.ofdm5gpo = (uint32)PHY_GETINTVAR(pi, rstr_ofdm5gpo);
	pi->ppr.sr_ssn.mcs5gpo0 = (uint32)PHY_GETINTVAR(pi, rstr_mcs5gpo0);

	/* 5GHz 40MHz MCS rates */
	pi->ppr.sr_ssn.mcs5gpo4 = (uint32)PHY_GETINTVAR(pi, rstr_mcs5gpo4);

	/* Extract 8 OFDM rates for low channels */
	pi->ppr.sr_ssn.ofdmalpo = (uint32)PHY_GETINTVAR(pi, rstr_ofdmalpo);
	pi->ppr.sr_ssn.mcs5gpo = (uint32)PHY_GETINTVAR(pi, rstr_mcs5gpo);

	pi->ppr.sr_ssn.maxp5gl = (int8)PHY_GETINTVAR(pi, rstr_maxp5gl);
	pi->ppr.sr_ssn.ofdm5glpo = (uint32)PHY_GETINTVAR(pi, rstr_ofdm5glpo);
	pi->ppr.sr_ssn.mcs5glpo0 = (uint32)PHY_GETINTVAR(pi, rstr_mcs5glpo0);

	/* 5GHz 40MHz MCS rates */
	pi->ppr.sr_ssn.mcs5glpo4 = (uint32)PHY_GETINTVAR(pi, rstr_mcs5glpo4);

	/* Extract 8 OFDM rates for hi channels */
	pi->ppr.sr_ssn.ofdmahpo = (uint32)PHY_GETINTVAR(pi, rstr_ofdmahpo);
	pi->ppr.sr_ssn.mcs5gpo = (uint32)PHY_GETINTVAR(pi, rstr_mcs5gpo);

	pi->ppr.sr_ssn.maxp4gh = (int8)PHY_GETINTVAR(pi, rstr_maxp5gh);
	pi->ppr.sr_ssn.ofdm5ghpo = (uint32)PHY_GETINTVAR(pi, rstr_ofdm5ghpo);
	pi->ppr.sr_ssn.mcs5ghpo0 = (uint32)PHY_GETINTVAR(pi, rstr_mcs5ghpo0);

	/* 5GHz 40MHz MCS rates */
	pi->ppr.sr_ssn.mcs5ghpo4 = (uint32)PHY_GETINTVAR(pi, rstr_mcs5ghpo4);
}
#endif /* BAND5G */

static void
wlc_phy_txpwr_sromssn_apply_ppr_parameters(phy_info_t *pi, uint8 band, ppr_t *tx_srom_max_pwr)
{
#ifdef BAND5G
	int8 txpwr;
	uint32 offset_mcs;
	uint32 saved_offset_mcs;
	int8 saved_txpwr;
	uint32 offset;
	uint32 ofdm_20_offsets;
	uint32 mcs_20_offsets;
	uint32 mcs_40_offsets;

	offset_mcs = pi->ppr.sr_ssn.mcs5gpo;
	saved_offset_mcs = offset_mcs;

	/* Max tx power */
	txpwr = pi->ppr.sr_ssn.pa1maxpwr;
	saved_txpwr = txpwr;
#endif /* BAND5G */

	/* The *po variables introduce a separate max tx power for reach rate.
	 * Each per-rate txpower is specified as offset from the maxtxpower
	 * from the maxtxpwr in that band (lo,mid,hi).
	 * The offsets in the variables is stored in half dbm units to save
	 * srom space, which need to be doubled to convert to quarter dbm units
	 * before using.
	 * For small 1Kbit sroms of PCI/PCIe cards, the getintav will always return 0;
	 * For bigger sroms or NVRAM or CIS, they are present
	 */

	switch (band)
	{
		case WL_CHAN_FREQ_RANGE_2G:
		{
			wlc_phy_txpwr_sromssn_apply_2g_ppr_parameters(pi, tx_srom_max_pwr);
			break;
		}
#ifdef BAND5G
		case WL_CHAN_FREQ_RANGE_5GM:
		{
			/* Extract 8 OFDM rates for mid channels */
			/* Override the maxpwr and offset for 5G mid-band if the SROM
			 * entry exists, otherwise use the default txpwr & offset setting
			 * from above
			 */
			offset = pi->ppr.sr_ssn.ofdmapo;
			txpwr = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.maxp5g, txpwr);
			ofdm_20_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.ofdm5gpo, offset);
			mcs_20_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.mcs5gpo0, offset_mcs);
			mcs_40_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.mcs5gpo4, saved_offset_mcs);

			wlc_phy_txpwr_apply_srom_5g_subband(txpwr, tx_srom_max_pwr, ofdm_20_offsets, mcs_20_offsets,
				mcs_40_offsets);
			break;
		}
		case WL_CHAN_FREQ_RANGE_5GL:
		{
			/* Extract 8 OFDM rates for low channels */
			offset = pi->ppr.sr_ssn.ofdmalpo;

			/* Override the maxpwr and offset for 5G low-band if the SROM
			 * entry exists, otherwise use the default txpwr & offset setting
			 * from above
			 */
			txpwr = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.maxp5gl, saved_txpwr);
			ofdm_20_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.ofdm5glpo, offset);
			mcs_20_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.mcs5glpo0, offset_mcs);
			mcs_40_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.mcs5glpo4, saved_offset_mcs);

			wlc_phy_txpwr_apply_srom_5g_subband(txpwr, tx_srom_max_pwr, ofdm_20_offsets, mcs_20_offsets,
				mcs_40_offsets);
			break;
		}
		case WL_CHAN_FREQ_RANGE_5GH:
		{
			/* Extract 8 OFDM rates for hi channels */
			offset = pi->ppr.sr_ssn.ofdmahpo;

			/* Override the maxpwr and offset for 5G high-band if the SROM
			 * entry exists, otherwise use the default txpwr & offset setting
			 * from above
			 */
			txpwr = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.maxp4gh, saved_txpwr);
			ofdm_20_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.ofdm5ghpo, offset);
			mcs_20_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.mcs5ghpo0, offset_mcs);
			mcs_40_offsets = PHY_GET_NONZERO_OR_DEFAULT(pi->ppr.sr_ssn.mcs5glpo4, saved_offset_mcs);

			wlc_phy_txpwr_apply_srom_5g_subband(txpwr, tx_srom_max_pwr, ofdm_20_offsets, mcs_20_offsets,
				mcs_40_offsets);
			break;
		}
#endif /* BAND5G */
	}
}

static void
BCMATTACHFN(wlc_phy_txpwr_sromssn_read_2g_ppr_parameters)(phy_info_t *pi)
{
	int8 txpwr;

	/* Max tx power */
	txpwr = (int8)PHY_GETINTVAR(pi, rstr_pa0maxpwr);
	/* Make sure of backward compatibility for bellatrix OLD NVRAM's */
	/* add dB to compensate for 1.5dBbackoff (that will be done) in in older boards */
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2) &&
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319WLUSBN4L_SSID)) {
			if ((GENERIC_PHY_INFO(pi)->boardrev) <= 0x1512)
				txpwr = txpwr + 6;
	}
	pi->tx_srom_max_2g = txpwr;

	pi->ppr.sr_ssn.cck202gpo = (uint16)PHY_GETINTVAR(pi, rstr_cckpo);

	/* Extract offsets for 8 OFDM rates */
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		pi->ppr.sr_ssn.ofdmbw202gpo = (uint32)PHY_GETINTVAR(pi, rstr_ofdm2gpo);
		pi->sslpnphy_mcs20_po = (((uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo1) << 16) |
			(uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo0));
		pi->sslpnphy_mcs40_po = (((uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo3) << 16) |
			(uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo2));
	} else {
		pi->ppr.sr_ssn.ofdmpo = (uint32)PHY_GETINTVAR(pi, rstr_ofdmpo);
		/* Now MCS2GPO is only 2 Bytes, ajust accordingly */
		pi->ppr.sr_ssn.mcs2gpo0 = (uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo0);
		pi->ppr.sr_ssn.mcs2gpo1 = (uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo1);
		pi->ppr.sr_ssn.mcs2gpo4 = (uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo4);
		pi->ppr.sr_ssn.mcs2gpo5 = (uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo5);
	}
}

static void
wlc_phy_txpwr_sromssn_apply_2g_ppr_parameters(phy_info_t *pi, ppr_t *tx_srom_max_pwr)
{
	int8 txpwr;
	uint16 pwr_offsets[2], pwr_offsets_40m[2];
	uint16 cckpo;
	uint32 offset_ofdm;
	uint32 offset_mcs;
	uint32 offset_mcs40;
	ppr_ofdm_rateset_t	ppr_ofdm;
	ppr_ht_mcs_rateset_t	ppr_mcs;
	ppr_dsss_rateset_t	ppr_dsss;

	/* Max tx power */
	txpwr = pi->tx_srom_max_2g;

	/* 2G - CCK_20 */
	cckpo = pi->ppr.sr_ssn.cck202gpo;
	if (cckpo) {
		wlc_phy_txpwr_srom_convert_cck(cckpo, txpwr, &ppr_dsss);
		ppr_set_dsss(tx_srom_max_pwr, WL_TX_BW_20, WL_TX_CHAINS_1, &ppr_dsss);
		/* Infer 20in40 DSSS from this limit */
		ppr_set_dsss(tx_srom_max_pwr, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &ppr_dsss);
	} else {
		ppr_set_same_dsss(tx_srom_max_pwr, WL_TX_BW_20, WL_TX_CHAINS_1, txpwr);
	}

	/* Extract offsets for 8 OFDM rates */
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		/* 2G - OFDM_20 */
		offset_ofdm = pi->ppr.sr_ssn.ofdmbw202gpo;
		wlc_phy_txpwr_srom_convert_ofdm(offset_ofdm, txpwr, &ppr_ofdm);
		ppr_set_ofdm(tx_srom_max_pwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ppr_ofdm);

		/* 2G - MCS_20 */
		offset_mcs = pi->sslpnphy_mcs20_po;
		wlc_phy_txpwr_srom_convert_mcs(offset_mcs, txpwr, &ppr_mcs);
		ppr_set_ht_mcs(tx_srom_max_pwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ppr_mcs);

		/* 2G - MCS_40 */
		offset_mcs40 = PHY_GET_NONZERO_OR_DEFAULT(pi->sslpnphy_mcs40_po, offset_mcs);
		wlc_phy_txpwr_srom_convert_mcs(offset_mcs40, txpwr, &ppr_mcs);
		ppr_set_ht_mcs(tx_srom_max_pwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ppr_mcs);
		/* Infer 20in40 MCS from this limit */
		ppr_set_ht_mcs(tx_srom_max_pwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ppr_mcs);
		/* Infer 20in40 OFDM from this limit */
		ppr_set_ofdm(tx_srom_max_pwr, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			(ppr_ofdm_rateset_t*)&ppr_mcs);
		/* Infer 40MHz OFDM from this limit */
		ppr_set_ofdm(tx_srom_max_pwr, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			(ppr_ofdm_rateset_t*)&ppr_mcs);

	} else {
		offset_ofdm = pi->ppr.sr_ssn.ofdmpo;

		wlc_phy_txpwr_srom_convert_ofdm(offset_ofdm, txpwr, &ppr_ofdm);
		ppr_set_ofdm(tx_srom_max_pwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ppr_ofdm);
		/* Now MCS2GPO is only 2 Bytes, ajust accordingly */
		pwr_offsets[0] = pi->ppr.sr_ssn.mcs2gpo0;
		pwr_offsets[1] = pi->ppr.sr_ssn.mcs2gpo1;

		ppr_get_ht_mcs(pi->tx_power_offset, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ppr_mcs);
		wlc_sslpnphy_txpwr_srom_convert(ppr_mcs.pwr, pwr_offsets, txpwr, 0, WL_RATESET_SZ_HT_MCS-1);
		ppr_set_ht_mcs(tx_srom_max_pwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ppr_mcs);

		pwr_offsets_40m[0] = pi->ppr.sr_ssn.mcs2gpo4;
		pwr_offsets_40m[1] = pi->ppr.sr_ssn.mcs2gpo5;

		/* If 40Mhz srom entries not available use 20Mhz mcs power offsets */
		if (pwr_offsets_40m[0] == 0) {
			ppr_get_ht_mcs(pi->tx_power_offset, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				&ppr_mcs);
			wlc_sslpnphy_txpwr_srom_convert(ppr_mcs.pwr, pwr_offsets, txpwr, 0, WL_RATESET_SZ_HT_MCS-1);
			ppr_set_ht_mcs(tx_srom_max_pwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				&ppr_mcs);
		} else {
			ppr_get_ht_mcs(pi->tx_power_offset, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				&ppr_mcs);
			wlc_sslpnphy_txpwr_srom_convert(ppr_mcs.pwr, pwr_offsets_40m, txpwr, 0, WL_RATESET_SZ_HT_MCS-1);
			ppr_set_ht_mcs(tx_srom_max_pwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				&ppr_mcs);
		}
	}
}

void
wlc_phy_txpower_sromlimit_get_ssnphy(phy_info_t *pi, uint channel, ppr_t *max_pwr)
{
	uint8 band;

	band = wlc_phy_get_band_from_channel(pi, channel);
	wlc_phy_txpwr_sromssn_apply_ppr_parameters(pi, band, max_pwr);
}

#else /* !PPR_API */

#ifdef BAND5G
static void
BCMATTACHFN(srom_read_and_apply_5g_tx_ppr_parameters)(phy_info_t *pi)
{
	int8 txpwr;
	int i;
	uint32 offset_mcs;
	uint32 saved_offset_mcs;
	int8 saved_txpwr;
	uint32 offset;
	uint16 bwduppo = 0;

	offset_mcs = (uint32)PHY_GETINTVAR(pi, rstr_mcs5gpo);
	saved_offset_mcs = offset_mcs;

	/* Max tx power */
	txpwr = (int8)PHY_GETINTVAR(pi, rstr_pa1maxpwr);
	saved_txpwr = txpwr;

	/* The *po variables introduce a seperate max tx power for reach rate.
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
	offset = (uint32)PHY_GETINTVAR(pi, rstr_ofdmapo);

	/* MCS32 power offset for each of the 5G sub-bands */
	if (PHY_GETVAR(pi, rstr_bwduppo)) {
		bwduppo = (uint16)PHY_GETINTVAR(pi, rstr_bwduppo);
	}
	/* Override the maxpwr and offset for 5G mid-band if the SROM
	 * entry exists, otherwise use the default txpwr & offset setting
	 * from above
	 */
	txpwr = (int8)PHY_GETINTVAR_DEFAULT(pi, rstr_maxp5g, txpwr);
	offset = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_ofdm5gpo, offset);
	offset_mcs = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_mcs5gpo0, offset_mcs);

	pi->tx_srom_max_5g_mid = txpwr;

	for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
		pi->tx_srom_max_rate_5g_mid[i] = txpwr - ((offset & 0xf) * 2);
		offset >>= 4;
	}
	for (i = TXP_FIRST_MCS_20; i <= TXP_LAST_MCS_SISO_20;  i++) {
		pi->tx_srom_max_rate_5g_mid[i] = txpwr -
			((offset_mcs & 0xf) * 2);
		offset_mcs >>= 4;
	}
	/* 5GHz 40MHz MCS rates */
	offset_mcs = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_mcs5gpo4, saved_offset_mcs);

	for (i = TXP_FIRST_MCS_40; i <= TXP_LAST_MCS_SISO_40;  i++) {
		pi->tx_srom_max_rate_5g_mid[i] = txpwr -
			((offset_mcs & 0xf) * 2);
		offset_mcs >>= 4;
	}

	/* MCS32 5G mid-band */
	pi->tx_srom_max_rate_5g_mid[TXP_LAST_MCS_40] = txpwr - (((bwduppo >> 4) & 0xf) * 2);

	/* Extract 8 OFDM rates for low channels */
	offset = (uint32)PHY_GETINTVAR(pi, rstr_ofdmalpo);
	offset_mcs = (uint32)PHY_GETINTVAR(pi, rstr_mcs5gpo);

	/* Override the maxpwr and offset for 5G low-band if the SROM
	 * entry exists, otherwise use the default txpwr & offset setting
	 * from above
	 */
	txpwr = (int8)PHY_GETINTVAR_DEFAULT(pi, rstr_maxp5gl, saved_txpwr);
	offset = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_ofdm5glpo, offset);
	offset_mcs = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_mcs5glpo0, offset_mcs);

	for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
		pi->tx_srom_max_rate_5g_low[i] = txpwr - ((offset & 0xf) * 2);
		offset >>= 4;
	}
	for (i = TXP_FIRST_MCS_20; i <= TXP_LAST_MCS_SISO_20;  i++) {
		pi->tx_srom_max_rate_5g_low[i] = txpwr -
			((offset_mcs & 0xf) * 2);
		offset_mcs >>= 4;
	}

	/* 5GHz 40MHz MCS rates */
	offset_mcs = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_mcs5glpo4, saved_offset_mcs);

	for (i = TXP_FIRST_MCS_40; i <= TXP_LAST_MCS_SISO_40;  i++) {
		pi->tx_srom_max_rate_5g_low[i] = txpwr -
			((offset_mcs & 0xf) * 2);
		offset_mcs >>= 4;
	}

	/* MCS32 5G low-band */
	pi->tx_srom_max_rate_5g_low[TXP_LAST_MCS_40] = txpwr - (((bwduppo >> 8) & 0xf) * 2);

	/* Extract 8 OFDM rates for hi channels */
	offset = (uint32)PHY_GETINTVAR(pi, rstr_ofdmahpo);
	offset_mcs = (uint32)PHY_GETINTVAR(pi, rstr_mcs5gpo);

	/* Override the maxpwr and offset for 5G high-band if the SROM
	 * entry exists, otherwise use the default txpwr & offset setting
	 * from above
	 */
	txpwr = (int8)PHY_GETINTVAR_DEFAULT(pi, rstr_maxp5gh, saved_txpwr);
	offset = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_ofdm5ghpo, offset);
	offset_mcs = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_mcs5ghpo0, offset_mcs);


	for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
		pi->tx_srom_max_rate_5g_hi[i] = txpwr - ((offset & 0xf) * 2);
		offset >>= 4;
	}
	for (i = TXP_FIRST_MCS_20; i <= TXP_LAST_MCS_SISO_20;  i++) {
		pi->tx_srom_max_rate_5g_hi[i] = txpwr -
			((offset_mcs & 0xf) * 2);
		offset_mcs >>= 4;
	}
	/* 5GHz 40MHz MCS rates */
	offset_mcs = (uint32)PHY_GETINTVAR_DEFAULT(pi, rstr_mcs5ghpo4, saved_offset_mcs);

	for (i = TXP_FIRST_MCS_40; i <= TXP_LAST_MCS_SISO_40;  i++) {
		pi->tx_srom_max_rate_5g_hi[i] = txpwr -
			((offset_mcs & 0xf) * 2);
		offset_mcs >>= 4;
	}

	/* MCS32 5G high-band */
	pi->tx_srom_max_rate_5g_hi[TXP_LAST_MCS_40] = txpwr - (((bwduppo >> 12) & 0xf) * 2);
}
#endif /* BAND5G */

static void
BCMATTACHFN(srom_read_and_apply_2g_tx_ppr_parameters)(phy_info_t *pi)
{
	int8 txpwr;
	int i;
	uint16 pwr_offsets[2], pwr_offsets_40m[2];
	uint16 cckpo;
	uint32 offset_ofdm;
	uint32 offset_mcs;

	/* Max tx power */
	txpwr = (int8)PHY_GETINTVAR(pi, rstr_pa0maxpwr);
	/* Make sure of backward compatibility for bellatrix OLD NVRAM's */
	/* add dB to compensate for 1.5dBbackoff (that will be done) in in older boards */
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2) &&
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319WLUSBN4L_SSID)) {
			if ((GENERIC_PHY_INFO(pi)->boardrev) <= 0x1512)
				txpwr = txpwr + 6;
	}
	pi->tx_srom_max_2g = txpwr;

	cckpo = (uint16)PHY_GETINTVAR(pi, rstr_cckpo);
	if (cckpo) {
		uint max_pwr_chan = txpwr;

		/* Extract offsets for 4 CCK rates. Remember to convert from
		 * .5 to .25 dbm units
		 */
		for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
			pi->tx_srom_max_rate_2g[i] = max_pwr_chan - ((cckpo & 0xf) * 2);
			cckpo >>= 4;
		}
	} else {
		/* Populate max power array for CCK rates */
		for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
			pi->tx_srom_max_rate_2g[i] = txpwr;
		}
	}

	/* Extract offsets for 8 OFDM rates */
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		offset_ofdm = (uint32)PHY_GETINTVAR(pi, rstr_ofdm2gpo);
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_2g[i] = txpwr - ((offset_ofdm & 0xf) * 2);
			offset_ofdm >>= 4;
		}

		pi->sslpnphy_mcs20_po = offset_mcs = (((uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo1) << 16) |
			(uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo0));
		for (i = TXP_FIRST_MCS_20; i <= TXP_LAST_MCS_SISO_20;  i++) {
			pi->tx_srom_max_rate_2g[i] = txpwr - ((offset_mcs & 0xf) * 2);
			offset_mcs >>= 4;
		}

		pi->sslpnphy_mcs40_po = (((uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo3) << 16) |
			(uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo2));
	} else {
		offset_ofdm = (uint32)PHY_GETINTVAR(pi, rstr_ofdmpo);
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_2g[i] = txpwr - ((offset_ofdm & 0xf) * 2);
			offset_ofdm >>= 4;
		}
		/* Now MCS2GPO is only 2 Bytes, ajust accordingly */
		pwr_offsets[0] = (uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo0);
		pwr_offsets[1] = (uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo1);
		wlc_sslpnphy_txpwr_srom_convert(pi->tx_srom_max_rate_2g, pwr_offsets, txpwr, TXP_FIRST_MCS_20,
			TXP_LAST_MCS_SISO_20);

		pwr_offsets_40m[0] = (uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo4);
		pwr_offsets_40m[1] = (uint16)PHY_GETINTVAR(pi, rstr_mcs2gpo5);

		/* If 40Mhz srom entries not available use 20Mhz mcs power offsets */
		if (pwr_offsets_40m[0] == 0) {
			wlc_sslpnphy_txpwr_srom_convert(pi->tx_srom_max_rate_2g, pwr_offsets, txpwr, TXP_FIRST_MCS_40,
				TXP_LAST_MCS_SISO_40);
		} else {
			wlc_sslpnphy_txpwr_srom_convert(pi->tx_srom_max_rate_2g, pwr_offsets_40m, txpwr,
				TXP_FIRST_MCS_40, TXP_LAST_MCS_SISO_40);
		}

		/* for MCS32 select power same as mcs7 40Mhz rate */
		pi->tx_srom_max_rate_2g[TXP_LAST_MCS_40] = pi->tx_srom_max_rate_2g[TXP_LAST_MCS_SISO_40];
	}
}
#endif /* PPR_API */

static void
WLBANDINITFN(wlc_sslpnphy_rev0_baseband_init)(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_OR_ENTRY(SSLPNPHY, lpphyCtrl, SSLPNPHY_lpphyCtrl_muxGmode_MASK)
			PHY_REG_OR_ENTRY(SSLPNPHY, crsgainCtrl, SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_MASK)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_AND_ENTRY(SSLPNPHY, lpphyCtrl, (uint16)~SSLPNPHY_lpphyCtrl_muxGmode_MASK)
			PHY_REG_AND_ENTRY(SSLPNPHY, crsgainCtrl, (uint16)~SSLPNPHY_crsgainCtrl_DSSSDetectionEnable_MASK)
		PHY_REG_LIST_EXECUTE(pi);
	}

	PHY_REG_MOD(pi, SSLPNPHY, lpphyCtrl, txfiltSelect, 1);

	/* Enable DAC/ADC and disable rf overrides */
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2))
		PHY_REG_WRITE(pi, SSLPNPHY, AfeDACCtrl, 0x54);
	else
		PHY_REG_WRITE(pi, SSLPNPHY, AfeDACCtrl, 0x50);

	PHY_REG_LIST_START_WLBANDINITDATA
		PHY_REG_WRITE_ENTRY(SSLPNPHY, AfeCtrl, 0x8800)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, AfeCtrlOvr, 0x0000)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, AfeCtrlOvrVal, 0x0000)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, RFinputOverride, 0x0000)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, RFOverride0, 0x0000)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, rfoverride2, 0x0000)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, rfoverride3, 0x0000)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, swctrlOvr, 0x0000)
		PHY_REG_MOD_ENTRY(SSLPNPHY, RxIqCoeffCtrl, RxIqCrsCoeffOverRide, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, RxIqCoeffCtrl, RxIqCrsCoeffOverRide11b, 1)
		/* Reset radio ctrl and crs gain */
		PHY_REG_OR_ENTRY(SSLPNPHY, resetCtrl, 0x44)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, resetCtrl, 0x80)
		/* RSSI settings */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, AfeRSSICtrl0, 0xA954)
	PHY_REG_LIST_EXECUTE(pi);

	PHY_REG_WRITE(pi, SSLPNPHY, AfeRSSICtrl1,
		((uint16)sslpnphy_specific->sslpnphy_rssi_vf_lowtemp << 0) | /* selmid_rssi: RSSI Vmid fine */
		((uint16)sslpnphy_specific->sslpnphy_rssi_vc_lowtemp << 4) | /* selmid_rssi: RSSI Vmid coarse */
		(0x00 << 8) | /* selmid_rssi: default value from AMS */
		((uint16)sslpnphy_specific->sslpnphy_rssi_gs_lowtemp << 10) | /* selav_rssi: RSSI gain select */
		(0x01 << 13)); /* slpinv_rssi */

}

static uint16 SMIC_ELNA_535640Mhz_OFDM_GAINTBL_TWEAKS [][2] = {
	        {0,  0x400},
	        {1,  0x403},
	        {2,  0x480},
	        {3,  0x483},
	        {4,  0x4DE},
	        {5,  0x4CC},
	        {6,  0x499},
	        {7,  0x49d},
	        {9,  0x004},
	        {14, 0x0BE},
	        {18, 0x182}
};
uint8 SMIC_ELNA_535640Mhz_OFDM_GAINTBL_TWEAKS_sz =
        ARRAYSIZE(SMIC_ELNA_535640Mhz_OFDM_GAINTBL_TWEAKS);

static void
wlc_sslpnphy_common_read_table(phy_info_t *pi, uint32 tbl_id,
	CONST void *tbl_ptr, uint32 tbl_len, uint32 tbl_width, uint32 tbl_offset);


int8
wlc_sslpnphy_get_rx_pwr_offset(phy_info_t *pi)
{
	int16 temp;

	if (!IS40MHZ(pi)) {
		temp = (int16)PHY_REG_READ(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb);
	} else {
		temp = (int16)PHY_REG_READ(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb);
	}

	if (temp > 127)
		temp -= 256;

	return (int8)temp;
}

void
wlc_sslpnphy_rx_offset_init(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	sslpnphy_specific->sslpnphy_input_pwr_offset_db = wlc_sslpnphy_get_rx_pwr_offset(pi);
}


static void
wlc_sslpnphy_agc_temp_init(phy_info_t *pi)
{
	uint32 tableBuffer[2];
	uint8 phybw40 = IS40MHZ(pi);
	int8 delta_T_change;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	/* reference ofdm gain index table offset */
	sslpnphy_specific->sslpnphy_ofdmgainidxtableoffset =
		(int16)PHY_REG_READ(pi, SSLPNPHY, gainidxoffset, ofdmgainidxtableoffset);
	if (sslpnphy_specific->sslpnphy_ofdmgainidxtableoffset > 127)
		sslpnphy_specific->sslpnphy_ofdmgainidxtableoffset -= 256;

	/* reference dsss gain index table offset */
	sslpnphy_specific->sslpnphy_dsssgainidxtableoffset =
		(int16)PHY_REG_READ(pi, SSLPNPHY, gainidxoffset, dsssgainidxtableoffset);
	if (sslpnphy_specific->sslpnphy_dsssgainidxtableoffset > 127)
		sslpnphy_specific->sslpnphy_dsssgainidxtableoffset -= 256;

	wlc_sslpnphy_common_read_table(pi, 17, tableBuffer, 2, 32, 64);

	/* reference value of gain_val_tbl at index 64 */
	if (tableBuffer[0] > 63) tableBuffer[0] -= 128;
	sslpnphy_specific->sslpnphy_tr_R_gain_val = tableBuffer[0];

	/* reference value of gain_val_tbl at index 65 */
	if (tableBuffer[1] > 63) tableBuffer[1] -= 128;
	sslpnphy_specific->sslpnphy_tr_T_gain_val = tableBuffer[1];
	if (phybw40 == 0) {
		sslpnphy_specific->sslpnphy_Med_Low_Gain_db =
			PHY_REG_READ(pi, SSLPNPHY, LowGainDB, MedLowGainDB);
		sslpnphy_specific->sslpnphy_Very_Low_Gain_db =
			PHY_REG_READ(pi, SSLPNPHY, VeryLowGainDB, veryLowGainDB);
	} else {
		sslpnphy_specific->sslpnphy_Med_Low_Gain_db =
			PHY_REG_READ(pi, SSLPNPHY_Rev2, LowGainDB_40, MedLowGainDB);
		sslpnphy_specific->sslpnphy_Very_Low_Gain_db =
			PHY_REG_READ(pi, SSLPNPHY_Rev2, VeryLowGainDB_40, veryLowGainDB);
	}

	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
		tableBuffer, 2, 32, 28);

	sslpnphy_specific->sslpnphy_gain_idx_14_lowword = tableBuffer[0];
	sslpnphy_specific->sslpnphy_gain_idx_14_hiword = tableBuffer[1];
	/* tr isolation adjustments */
	if (sslpnphy_specific->sslpnphy_tr_isolation_mid &&
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) != BCM94319WLUSBN4L_SSID)) {

		tableBuffer[0] = sslpnphy_specific->sslpnphy_tr_R_gain_val;
		tableBuffer[1] = sslpnphy_specific->sslpnphy_tr_isolation_mid;
		if (tableBuffer[1] > 63) {
			if (CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID)
				tableBuffer[1] = tableBuffer[1] - 128 + 15;
			else
				tableBuffer[1] -= 128;
		} else {
			if (CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID)
				tableBuffer[1] += 15;
		}
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
			tableBuffer, 2, 32, 64);

		delta_T_change = sslpnphy_specific->sslpnphy_tr_T_gain_val - tableBuffer[1];

		sslpnphy_specific->sslpnphy_Very_Low_Gain_db += delta_T_change;
		sslpnphy_specific->sslpnphy_tr_T_gain_val = tableBuffer[1];

		if (phybw40) {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, VeryLowGainDB_40, veryLowGainDB,
				sslpnphy_specific->sslpnphy_Very_Low_Gain_db);
		} else {
			PHY_REG_MOD(pi, SSLPNPHY, VeryLowGainDB, veryLowGainDB,
				sslpnphy_specific->sslpnphy_Very_Low_Gain_db);
		}

	}
	/* Added To Increase The 1Mbps Sense for Temps @Around */
	/* -15C Temp With CmRxAciGainTbl */
	sslpnphy_specific->sslpnphy_gain_idx_27_lowword = 0xf1e64d96;
	sslpnphy_specific->sslpnphy_gain_idx_27_hiword  = 0xf1e60018;

	/* Storing Input rx offset */
	wlc_sslpnphy_rx_offset_init(pi);

	wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(pi);
}

static void
wlc_sslpnphy_bu_tweaks(phy_info_t *pi)
{

	uint8 phybw40 = IS40MHZ(pi);
	int8 aa;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	if (NORADIO_ENAB(pi->pubpi)) {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2))
			PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 500);
		return;
	}

	/* CRS Parameters tuning */
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, gaindirectMismatch, medGainGmShftVal, 3)
		PHY_REG_MOD2_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, ClipCtrThreshHiGain, 30, 20)
		PHY_REG_MOD2_ENTRY(SSLPNPHY, HiGainDB, HiGainDB, MedHiGainDB, 70, 45)
		PHY_REG_MOD2_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, NominalPwrDB, 6, 95)
		PHY_REG_MOD2_ENTRY(SSLPNPHY, radioTRCtrlCrs1, gainReqTrAttOnEnByCrs, trGainThresh, 1, 25)
		PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs2, trTransAddrLmtOfdm, 12)
		PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatch, GainMismatchHigain, 10)
		PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh1, LargeGainMismatchThresh, 9)
		PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatchMedGainEx, medHiGainDirectMismatchOFDMDet, 3)
		PHY_REG_MOD_ENTRY(SSLPNPHY, crsMiscCtrl2, eghtSmplFstPwrLogicEn, 0)
		PHY_REG_MOD2_ENTRY(SSLPNPHY, crsTimingCtrl, gainThrsh4Timing, gainThrsh4MF, 0, 73)
		PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmSyncThresh1, ofdmSyncThresh2, 2)
		PHY_REG_MOD_ENTRY(SSLPNPHY, SyncPeakCnt, MaxPeakCntM1, 7)
		PHY_REG_MOD_ENTRY(SSLPNPHY, DSSSConfirmCnt, DSSSConfirmCntHiGain, 3)
		PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 255)
		PHY_REG_MOD_ENTRY(SSLPNPHY, MinPwrLevel, ofdmMinPwrLevel, 162)
		PHY_REG_MOD_ENTRY(SSLPNPHY, LowGainDB, MedLowGainDB, 29)
		PHY_REG_MOD_ENTRY(SSLPNPHY, gainidxoffset, dsssgainidxtableoffset, 244)
		PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 10)
		PHY_REG_MOD_ENTRY(SSLPNPHY, crsMiscCtrl0, usePreFiltPwr, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmPwrThresh1, ofdmPwrThresh3, 48)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6033)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipThresh, 108)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, SgiprgReg, 3)
	PHY_REG_LIST_EXECUTE(pi);

	if (phybw40 == 1)
		PHY_REG_MOD(pi, SSLPNPHY, radioTRCtrl, gainrequestTRAttnOnEn, 0);
	else
		PHY_REG_MOD(pi, SSLPNPHY, radioTRCtrl, gainrequestTRAttnOnEn, 1);

	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrl, gainrequestTRAttnOnEn, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, gainReqTrAttOnEnByCrs, 0)
			/* WAR to the Higher A-band Channels Rxper Hump @-60 to -70dBm Signal Levels
			From Aniritsu8860C Tester
			*/
			PHY_REG_MOD_ENTRY(SSLPNPHY, crsMiscCtrl0, cfoCalcEn, 0)
		PHY_REG_LIST_EXECUTE(pi);

#if !defined(ROMTERMPHY)
		aa = (int8)ANT_AVAIL(pi->aa5g);
#else
		aa = (int8)ANT_AVAIL(pi->sh->ant_avail_aa5g);
#endif /* PHYHAL */
	} else {
#if !defined(ROMTERMPHY)
		aa = (int8)ANT_AVAIL(pi->aa2g);
#else
		aa = (int8)ANT_AVAIL(pi->sh->ant_avail_aa2g);
#endif /* PHYHAL */
		/* Dflt Value */
		PHY_REG_MOD(pi, SSLPNPHY, crsMiscCtrl0, cfoCalcEn, 1);
	}
	if (aa > 1) {

		/* Antenna diveristy related changes */
		PHY_REG_LIST_START
			PHY_REG_MOD2_ENTRY(SSLPNPHY, crsgainCtrl, wlpriogainChangeEn, preferredAntEn, 0, 0)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, lnaputable, 0x5555)
			PHY_REG_MOD_ENTRY(SSLPNPHY, radioCtrl, auxgaintblEn, 0)
		PHY_REG_LIST_EXECUTE(pi);

		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, slnanoisetblreg0, 0x7fff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, slnanoisetblreg1, 0x7fff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, slnanoisetblreg2, 0x03ff)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, slnanoisetblreg0, 0x4210)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, slnanoisetblreg1, 0x4210)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, slnanoisetblreg2, 0x0270)
			PHY_REG_LIST_EXECUTE(pi);
		}

		/* phy_reg_mod(pi, SSLPNPHY_PwrThresh1, */
		/*	SSLPNPHY_PwrThresh1_LoPwrMismatchThresh_MASK, */
		/*	20 << SSLPNPHY_PwrThresh1_LoPwrMismatchThresh_SHIFT); */
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_MOD2(pi, SSLPNPHY_Rev2, crsgainCtrl_40, wlpriogainChangeEn, preferredAntEn, 0, 0);
		}
		/* Change default antenna to 2 */
		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) {
			if (aa == 2) {
				if (phybw40)
					PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsgainCtrl_40, DefaultAntenna, 0x01);
				else
					PHY_REG_MOD(pi, SSLPNPHY, crsgainCtrl, DefaultAntenna, 0x01);
			}
		}
		/* enable Diversity for  Dual Antenna Boards */
		if (aa > 2) {
			if (phybw40)
				PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsgainCtrl_40, DiversityChkEnable, 0x01);
			else
				PHY_REG_MOD(pi, SSLPNPHY, crsgainCtrl, DiversityChkEnable, 0x01);
		}
	}
	if (IS_OLYMPIC(pi)) {
		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID)
			PHY_REG_MOD(pi, SSLPNPHY, BphyControl3, bphyScale, 0x6);
		else
			PHY_REG_MOD(pi, SSLPNPHY, BphyControl3, bphyScale, 0x7);
	} else
		PHY_REG_MOD(pi, SSLPNPHY, BphyControl3, bphyScale, 0xc);
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipThresh, 72)
			PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh1, LargeGainMismatchThresh, 4)
		PHY_REG_LIST_EXECUTE(pi);
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
			if (phybw40)
			{
				PHY_REG_MOD(pi, SSLPNPHY, BphyControl3, bphyScale, 0x8);
			} else {
				PHY_REG_MOD(pi, SSLPNPHY, BphyControl3, bphyScale, 0xa);
			}
		} else {
			if (phybw40) {
				PHY_REG_MOD(pi, SSLPNPHY, BphyControl3, bphyScale, 0x11);
			} else {
				PHY_REG_MOD(pi, SSLPNPHY, BphyControl3, bphyScale, 0x13);
			}
		}

		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, ClipCtrThreshHiGain, 18)
			PHY_REG_MOD_ENTRY(SSLPNPHY, MinPwrLevel, dsssMinPwrLevel, 158)
			PHY_REG_MOD_ENTRY(SSLPNPHY, crsgainCtrl, phycrsctrl, 11)
			PHY_REG_MOD_ENTRY(SSLPNPHY, SyncPeakCnt, MaxPeakCntM1, 7)
			PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs2, trTransAddrLmtOfdm, 11)
			PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 20)
			PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh1, LoPwrMismatchThresh, 18)
			PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatchMedGainEx, medHiGainDirectMismatchOFDMDet, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam1, ofdmSyncConfirmAdjst, 5)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6366)
		PHY_REG_LIST_EXECUTE(pi);

		if (phybw40 == 1) {
			PHY_REG_LIST_START
				PHY_REG_MOD2_ENTRY(SSLPNPHY_Rev2, radioCtrl_40mhz,
					round_control_40mhz, gainReqTrAttOnEnByCrs40, 0, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, gaindirectMismatch_40, medGainGmShftVal, 3)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ClipCtrThresh_40, clipCtrThreshLoGain, 36)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, radioTRCtrl, gainrequestTRAttnOnEn, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, HiGainDB_40, HiGainDB, 70)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, VeryLowGainDB_40, veryLowGainDB, 9)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, gainMismatch_40, GainMismatchHigain, 10)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, crsMiscCtrl2_40, eghtSmplFstPwrLogicEn, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, PwrThresh1_40, LargeGainMismatchThresh, 9)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, MinPwrLevel_40, ofdmMinPwrLevel, 164)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, MinPwrLevel_40, dsssMinPwrLevel, 159)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, gainBackOffVal_40, 0x6366)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, VeryLowGainDB_40, NominalPwrDB, 103)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, LowGainDB_40, MedLowGainDB, 29)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 255)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, PwrThresh0_40, SlowPwrLoThresh, 11)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, SlowPwrLoThresh, 11)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, SlowPwrLoThresh, 11)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, gainMismatchMedGainEx_40,
					medHiGainDirectMismatchOFDMDet, 3)
				/* SGI -56 to -64dBm Hump Fixes */
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ClipThresh_40, 72)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ClipCtrThresh_40, ClipCtrThreshHiGain, 44)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, HiGainDB_40, MedHiGainDB, 45)
				/* SGI -20 to -32dBm Hump Fixes */
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, crsTimingCtrl_40, gainThrsh4MF, 73)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, crsTimingCtrl_40, gainThrsh4Timing, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, crsMiscParams_40, incSyncCntVal, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, lpParam2_40, gainSettleDlySmplCnt, 60)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, crsMiscCtrl0_40, usePreFiltPwr, 0)
			PHY_REG_LIST_EXECUTE(pi);

			if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, syncParams2_20U, gainThrsh4MF, 66)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, syncParams2_20U, gainThrsh4Timing, 0)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, syncParams1_20U, incSyncCntVal, 0)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, syncParams2_20L, gainThrsh4MF, 66)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, syncParams2_20L, gainThrsh4Timing, 0)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, syncParams1_20L, incSyncCntVal, 0)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, bndWdthClsfy2_40, bwClsfyGainThresh, 66)
				PHY_REG_LIST_EXECUTE(pi);
			}

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ofdmPwrThresh0_20L, ofdmPwrThresh0, 3)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ofdmPwrThresh1_20L, ofdmPwrThresh3, 48)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ofdmPwrThresh0_20U, ofdmPwrThresh0, 3)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ofdmPwrThresh1_20U, ofdmPwrThresh3, 48)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, radioTRCtrl, gainrequestTRAttnOnOffset, 7)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, trGainthresh_40, trGainThresh, 20)
				/* TO REDUCE PER HUMPS @HIGH Rx Powers */
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ofdmSyncThresh1_40, ofdmSyncThresh2, 2)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ofdmSyncThresh1_20U, ofdmSyncThresh2, 2)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ofdmSyncThresh1_20L, ofdmSyncThresh2, 2)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, DSSSConfirmCnt_40, DSSSConfirmCntHiGain, 4)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, DSSSConfirmCnt_40, DSSSConfirmCntLoGain, 4)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, DSSSConfirmCnt_40, DSSSConfirmCntHiGainCnfrm, 2)
			PHY_REG_LIST_EXECUTE(pi);
		}
		/* enable extlna */
		if ((CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) &&
			(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, radioCtrl, extlnaen, 1)
				PHY_REG_MOD_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, 253)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, VeryLowGainDB_40, veryLowGainDB, 253)
				PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 20)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ClipCtrThresh_40, clipCtrThreshLoGain, 30)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipThresh, 96)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ClipThresh_40, 96)
				PHY_REG_MOD_ENTRY(SSLPNPHY, LowGainDB, MedLowGainDB, 26)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, LowGainDB_40, MedLowGainDB, 26)
				PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh1, LoPwrMismatchThresh, 24)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6666)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, gainBackOffVal_40, 0x6666)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipCtrDefThresh, 12)
				PHY_REG_MOD_ENTRY(SSLPNPHY, gainidxoffset, ofdmgainidxtableoffset, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, gainidxoffset, dsssgainidxtableoffset, 3)
			PHY_REG_LIST_EXECUTE(pi);

			{
				phytbl_info_t tab;
				uint32 tableBuffer[2] = {18, -3};
				tab.tbl_ptr = tableBuffer;	/* ptr to buf */
				tab.tbl_len = 2;			/* # values   */
				tab.tbl_id = 17;			/* gain_val_tbl_rev3 */
				tab.tbl_offset = 64;		/* tbl offset */
				tab.tbl_width = 32;			/* 32 bit wide */
				wlc_sslpnphy_write_table(pi, &tab);
			}

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, LowGainDB, MedLowGainDB, 41)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, LowGainDB_40, MedLowGainDB, 41)
				PHY_REG_MOD_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, 12)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, VeryLowGainDB_40, veryLowGainDB, 12)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, gainMismatch, GainmisMatchPktRx, 9)
				PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 12)
				PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh1, PktRxSignalDropThresh, 15)
			PHY_REG_LIST_EXECUTE(pi);
		}

		 if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			PHY_REG_MOD(pi, SSLPNPHY, crsMiscCtrl0, matchFiltEn, 0);
			PHY_REG_MOD(pi, SSLPNPHY, bphyacireg, enBphyAciFilt, (phybw40 ? 0 : 1));
			PHY_REG_MOD(pi, SSLPNPHY, bphyacireg, enDmdBphyAciFilt, (phybw40 ? 0 : 1));

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam1, ofdmSyncConfirmAdjst, 7)
				PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, gainReqTrAttOnEnByCrs, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrl, gainrequestTRAttnOnEn, 0)
				PHY_REG_MOD3_ENTRY(SSLPNPHY, DSSSConfirmCnt,
					DSSSConfirmCntHiGain, DSSSConfirmCntLoGain, DSSSConfirmCntHiGainCnfrm, 4, 4, 2)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, DSSSConfirmCnt_40, DSSSConfirmCntHiGain, 4)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, DSSSConfirmCnt_40, DSSSConfirmCntLoGain, 4)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, DSSSConfirmCnt_40, DSSSConfirmCntHiGainCnfrm, 2)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}
	/* Change timing to 11.5us */
	wlc_sslpnphy_set_tx_pwr_by_index(pi, 40);
	sslpnphy_specific->sslpnphy_current_index = 40;
	PHY_REG_LIST_START
		PHY_REG_WRITE_ENTRY(SSLPNPHY, TxMacIfHoldOff, 23)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, TxMacDelay, 1002)
	PHY_REG_LIST_EXECUTE(pi);
	/* Adjust RIFS timings */
	if (!SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		if (phybw40 == 0) {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, rifsSttimeout, 0x1214)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, readsym2resetCtrl, 0x7800)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}
	/* Set Tx Index delta and Temp delta thresholds to default values */
	if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) {
		pi->txidx_delta_threshold = 6;
		pi->temp_delta_threshold = 10;
		pi->papd_txidx_delta_threshold = 20;
		pi->papd_temp_delta_threshold = 25;
		#ifdef WLMEDIA_APCS
		pi->dcs_papd_delay = 20;
		#endif /* WLMEDIA_APCS */
	}
}

static void
WLBANDINITFN(wlc_sslpnphy_baseband_init)(phy_info_t *pi)
{
	/* Initialize SSLPNPHY tables */
	wlc_sslpnphy_tbl_init(pi);
	wlc_sslpnphy_rev0_baseband_init(pi);
	wlc_sslpnphy_bu_tweaks(pi);
}


typedef struct {
	uint16 phy_addr;
	uint8 phy_shift;
	uint8 rf_addr;
	uint8 rf_shift;
	uint8 mask;
} sslpnphy_extstxdata_t;

static sslpnphy_extstxdata_t
WLBANDINITDATA(sslpnphy_extstxdata)[] = {
	{SSLPNPHY_extstxctrl0 + 2, 6, 0x3d, 3, 0x1},
	{SSLPNPHY_extstxctrl0 + 1, 12, 0x4c, 1, 0x1},
	{SSLPNPHY_extstxctrl0 + 1, 8, 0x50, 0, 0x7f},
	{SSLPNPHY_extstxctrl0 + 0, 8, 0x44, 0, 0xff},
	{SSLPNPHY_extstxctrl0 + 1, 0, 0x4a, 0, 0xff},
	{SSLPNPHY_extstxctrl0 + 0, 4, 0x4d, 0, 0xff},
	{SSLPNPHY_extstxctrl0 + 1, 4, 0x4e, 0, 0xff},
	{SSLPNPHY_extstxctrl0 + 0, 12, 0x4f, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 1, 0, 0x4f, 4, 0xf},
	{SSLPNPHY_extstxctrl0 + 3, 0, 0x49, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 4, 3, 0x46, 4, 0x7},
	{SSLPNPHY_extstxctrl0 + 3, 15, 0x46, 0, 0x1},
	{SSLPNPHY_extstxctrl0 + 4, 0, 0x46, 1, 0x7},
	{SSLPNPHY_extstxctrl0 + 3, 8, 0x48, 4, 0x7},
	{SSLPNPHY_extstxctrl0 + 3, 11, 0x48, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 3, 4, 0x49, 4, 0xf},
	{SSLPNPHY_extstxctrl0 + 2, 15, 0x45, 0, 0x1},
	{SSLPNPHY_extstxctrl0 + 5, 13, 0x52, 4, 0x7},
	{SSLPNPHY_extstxctrl0 + 6, 0, 0x52, 7, 0x1},
	{SSLPNPHY_extstxctrl0 + 5, 3, 0x41, 5, 0x7},
	{SSLPNPHY_extstxctrl0 + 5, 6, 0x41, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 5, 10, 0x42, 5, 0x7},
	{SSLPNPHY_extstxctrl0 + 4, 15, 0x42, 0, 0x1},
	{SSLPNPHY_extstxctrl0 + 5, 0, 0x42, 1, 0x7},
	{SSLPNPHY_extstxctrl0 + 4, 11, 0x43, 4, 0xf},
	{SSLPNPHY_extstxctrl0 + 4, 7, 0x43, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 4, 6, 0x45, 1, 0x1},
	{SSLPNPHY_extstxctrl0 + 2, 7, 0x40, 4, 0xf},
	{SSLPNPHY_extstxctrl0 + 2, 11, 0x40, 0, 0xf},
	{SSLPNPHY_extstxctrl0 + 1, 14, 0x3c, 3, 0x3},
	{SSLPNPHY_extstxctrl0 + 2, 0, 0x3c, 5, 0x7},
	{SSLPNPHY_extstxctrl0 + 2, 3, 0x3c, 0, 0x7},
	{SSLPNPHY_extstxctrl0 + 0, 0, 0x52, 0, 0xf},
	};

static void
WLBANDINITFN(wlc_sslpnphy_synch_stx)(phy_info_t *pi)
{
	uint i;

	mod_radio_reg(pi, RADIO_2063_COMMON_04, 0xf8, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_05, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_06, 0xff);
	write_radio_reg(pi, RADIO_2063_COMMON_07, 0xff);
	mod_radio_reg(pi, RADIO_2063_COMMON_08, 0x7, 0xff);

	for (i = 0; i < ARRAYSIZE(sslpnphy_extstxdata); i++) {
		PHY_REG_MOD_RAW(pi,
			sslpnphy_extstxdata[i].phy_addr,
			(uint16)sslpnphy_extstxdata[i].mask << sslpnphy_extstxdata[i].phy_shift,
			(uint16)(read_radio_reg(pi, sslpnphy_extstxdata[i].rf_addr) >>
			sslpnphy_extstxdata[i].rf_shift) << sslpnphy_extstxdata[i].phy_shift);
	}

	mod_radio_reg(pi, RADIO_2063_COMMON_04, 0xf8, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_05, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_06, 0);
	write_radio_reg(pi, RADIO_2063_COMMON_07, 0);
	mod_radio_reg(pi, RADIO_2063_COMMON_08, 0x7, 0);
}

static void
WLBANDINITFN(sslpnphy_run_jtag_rcal)(phy_info_t *pi)
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

	/* RCAL is done, now power down RCAL to save 100uA leakage during IEEE PS
	 * sleep, last RCAL value will remain valid even if RCAL is powered down
	*/
	write_radio_reg(pi, RADIO_2063_COMMON_16, 0x10);
}

static void
WLBANDINITFN(wlc_sslpnphy_radio_init)(phy_info_t *pi)
{
	uint32 macintmask;
	uint8 phybw40;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif

	phybw40 = IS40MHZ(pi);

	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* Toggle radio reset */
	PHY_REG_OR(pi, SSLPNPHY, fourwireControl, SSLPNPHY_fourwireControl_radioReset_MASK);
	OSL_DELAY(1);
	PHY_REG_AND(pi, SSLPNPHY, fourwireControl, ~SSLPNPHY_fourwireControl_radioReset_MASK);
	OSL_DELAY(1);

	/* Initialize 2063 radio */
	wlc_radio_2063_init_sslpnphy(pi);

	/* Synchronize phy overrides for RF registers that are mapped through the CLB */
	wlc_sslpnphy_synch_stx(pi);

	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		or_radio_reg(pi, RADIO_2063_COMMON_04, 0x40);
		or_radio_reg(pi, RADIO_2063_TXRF_SP_3, 0x08);
	} else
		and_radio_reg(pi, RADIO_2063_COMMON_04, ~(uint8)0x40);

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Shared RX clb signals */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, extslnactrl0, 0x5f80)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, extslnactrl1, 0x0)
		/* Set Tx Filter Bandwidth */
		PHY_REG_MOD_ENTRY(SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 3)
		PHY_REG_MOD_ENTRY(SSLPNPHY, lpfbwlutreg1, lpfbwlut5, 2)
	PHY_REG_LIST_EXECUTE(pi);

	if (IS_OLYMPIC(pi))
		PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 1);
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (phybw40 == 1) {
			PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg1, lpfbwlut5, 4);
		} else {
			PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 0);
		}
	}
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		if (phybw40) {
	                PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 2);
	        } else {
			if ((sslpnphy_specific->sslpnphy_cck_filt_sel == 10) ||
				(sslpnphy_specific->sslpnphy_cck_filt_sel == 11)) {
	                        PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 0);
	                } else {
	                        PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 3);
	                }
	        }
	}
	write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xee);
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		if (sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4)
			write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xde);
		else
			write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xee);
	}

	/* Run RCal */
#ifdef BCMRECLAIM
	if (!bcmreclaimed) {
#endif /* BCMRECLAIM */
#if !defined(ROMTERMPHY)
	    macintmask = wlapi_intrsoff(pi->sh->physhim);
#else
		macintmask = wl_intrsoff(((wlc_info_t *)(pi->wlc))->wl);
#endif
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			sslpnphy_run_jtag_rcal(pi);
		} else {
			si_pmu_rcal(GENERIC_PHY_INFO(pi)->sih, GENERIC_PHY_INFO(pi)->osh);
		}
#if !defined(ROMTERMPHY)
		wlapi_intrsrestore(pi->sh->physhim, macintmask);
#else
		wl_intrsrestore(((wlc_info_t *)(pi->wlc))->wl, macintmask);
#endif
#ifdef BCMRECLAIM
	}
#endif
}
static void
wlc_sslpnphy_rc_cal(phy_info_t *pi)
{
	uint8 rxbb_sp8, txbb_sp_3;
	uint8 save_pll_jtag_pll_xtal;
	uint16 epa_ovr, epa_ovr_val;

	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* RF_PLL_jtag_pll_xtal_1_2 */
	save_pll_jtag_pll_xtal = (uint8) read_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, 0x4);

	/* Save old cap value incase RCCal fails */
	rxbb_sp8 = (uint8)read_radio_reg(pi, RADIO_2063_RXBB_SP_8);

	/* Save RF overide values */
	epa_ovr = phy_reg_read(pi, SSLPNPHY_RFOverride0);
	epa_ovr_val = phy_reg_read(pi, SSLPNPHY_RFOverrideVal0);

	/* Switch off ext PA */
	if (CHSPEC_IS5G(pi->radio_chanspec) &&
		(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, amode_tx_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, amode_tx_pu_ovr_val, 0)
		PHY_REG_LIST_EXECUTE(pi);
	}
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

	/* set Trc1 */

	if ((PHY_XTALFREQ(pi->xtalfreq) == 38400000) || (PHY_XTALFREQ(pi->xtalfreq) == 37400000)) {
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0xa0);
	} else if (PHY_XTALFREQ(pi->xtalfreq) == 30000000) {
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x52);   /* only for 30MHz in 4319 */
	} else if (PHY_XTALFREQ(pi->xtalfreq) == 25000000) {
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x18);   /* 25MHz in 5356 */
	} else if (PHY_XTALFREQ(pi->xtalfreq) == 26000000) {
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x32);   /* For 26MHz Xtal */
	}
	/* set Trc2 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_5, 0x1);

	/* Start rx RCCAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7d);

	/* Wait for rx RCCAL completion */
	OSL_DELAY(50);
	SPINWAIT(!wlc_radio_2063_rc_cal_done(pi), 10 * 1000 * 1000);

	if (!wlc_radio_2063_rc_cal_done(pi)) {
		WL_ERROR(("wl%d: %s: Rx RC Cal failed\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
		write_radio_reg(pi, RADIO_2063_RXBB_SP_8, rxbb_sp8);
		/* Put an infinite while loop and get into a time out error */
		/* instead of proceeding  with R cal failure */
		while (1) {
		}
	} else
		WL_INFORM(("wl%d: %s:  Rx RC Cal completed: N0: %x%x, N1: %x%x, code: %x\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
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

	if (PHY_XTALFREQ(pi->xtalfreq) == 26000000) {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x30);
	} else if ((PHY_XTALFREQ(pi->xtalfreq) == 38400000) || (PHY_XTALFREQ(pi->xtalfreq) == 37400000)) {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x96);
	} else if (PHY_XTALFREQ(pi->xtalfreq) == 30000000) {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x3d);  /* for 30Mhz 4319 */
	} else {
		/* set Trc1 */
		write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_4, 0x30);
	}
	/* set Trc2 */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_5, 0x1);

	/* Start tx RCCAL */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7d);

	/* Wait for tx RCCAL completion */
	OSL_DELAY(50);
	SPINWAIT(!wlc_radio_2063_rc_cal_done(pi), 10 * 1000 * 1000);

	if (!wlc_radio_2063_rc_cal_done(pi)) {
		WL_ERROR(("wl%d: %s: Tx RC Cal failed\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
		write_radio_reg(pi, RADIO_2063_TXBB_SP_3, txbb_sp_3);
		/* Put an infinite while loop and get into a time out error */
		/* instead of proceeding  with R cal failure */
		while (1) {
		}
	} else
		WL_INFORM(("wl%d: %s:  Tx RC Cal completed: N0: %x%x, N1: %x%x, code: %x\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_8),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_7),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_10),
			read_radio_reg(pi, RADIO_2063_RCCAL_CTRL_9),
			read_radio_reg(pi, RADIO_2063_COMMON_12) & 0x1f));

	/* Power down RCCAL after it is done */
	write_radio_reg(pi, RADIO_2063_RCCAL_CTRL_1, 0x7e);

	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_XTAL_1_2, save_pll_jtag_pll_xtal);
	/* Restore back amode tx pu */
	PHY_REG_WRITE(pi, SSLPNPHY, RFOverride0, epa_ovr);
	PHY_REG_WRITE(pi, SSLPNPHY, RFOverrideVal0, epa_ovr_val);
}

STATIC void
BCMROMFN(wlc_sslpnphy_toggle_afe_pwdn)(phy_info_t *pi)
{
	uint16 save_AfeCtrlOvrVal, save_AfeCtrlOvr;

	save_AfeCtrlOvrVal = phy_reg_read(pi, SSLPNPHY_AfeCtrlOvrVal);
	save_AfeCtrlOvr = phy_reg_read(pi, SSLPNPHY_AfeCtrlOvr);

	PHY_REG_WRITE(pi, SSLPNPHY, AfeCtrlOvrVal, save_AfeCtrlOvrVal | 0x1);
	PHY_REG_WRITE(pi, SSLPNPHY, AfeCtrlOvr, save_AfeCtrlOvr | 0x1);

	PHY_REG_WRITE(pi, SSLPNPHY, AfeCtrlOvrVal, save_AfeCtrlOvrVal & 0xfffe);
	PHY_REG_WRITE(pi, SSLPNPHY, AfeCtrlOvr, save_AfeCtrlOvr & 0xfffe);

	PHY_REG_WRITE(pi, SSLPNPHY, AfeCtrlOvrVal, save_AfeCtrlOvrVal);
	PHY_REG_WRITE(pi, SSLPNPHY, AfeCtrlOvr, save_AfeCtrlOvr);
}
static void
wlc_sslpnphy_common_read_table(phy_info_t *pi, uint32 tbl_id,
	CONST void *tbl_ptr, uint32 tbl_len, uint32 tbl_width,
	uint32 tbl_offset) {

	phytbl_info_t tab;
	tab.tbl_id = tbl_id;
	tab.tbl_ptr = tbl_ptr;	/* ptr to buf */
	tab.tbl_len = tbl_len;			/* # values   */
	tab.tbl_width = tbl_width;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = tbl_offset;		/* tbl offset */
	wlc_sslpnphy_read_table(pi, &tab);
}
static void
wlc_sslpnphy_common_write_table(phy_info_t *pi, uint32 tbl_id, CONST void *tbl_ptr,
	uint32 tbl_len, uint32 tbl_width, uint32 tbl_offset) {

	phytbl_info_t tab;
	tab.tbl_id = tbl_id;
	tab.tbl_ptr = tbl_ptr;	/* ptr to buf */
	tab.tbl_len = tbl_len;			/* # values   */
	tab.tbl_width = tbl_width;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = tbl_offset;		/* tbl offset */
	wlc_sslpnphy_write_table(pi, &tab);
}
static void
wlc_sslpnphy_set_chanspec_default_tweaks(phy_info_t *pi)
{

	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);

	if (!NORADIO_ENAB(pi->pubpi)) {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
				spur_tbl_rev2, 192, 8, 0);
		} else {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
				spur_tbl_rev0, 64, 8, 0);
		}

		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 0xfff, ((0x8 << 0) | (0x1f << 6)));
			if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
				si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 3,
					((1 << 26) | (1 << 21)), ((1 << 26) | (1 << 21)));
				si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 5, (0x1ff << 9), (0x1ff << 9));
			}
		}
		/* sdna: xtal changes */
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 4) && sdna_board_flag) {
			/* xtal buffer size */
			/* si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 0x3f, 0x3F); */
			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 0x3f, 0x20);
			/* xtalcore after XTAL becomes stable */
			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 2, 0x3f000, 0x20 << 12);
			/* xtal delay */
			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 0x18000, 0x2 << 15);
		}

		PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 255);

		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam1, ofMaxPThrUpdtThresh, 11)
				PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam2, oFiltSyncCtrShft, 1)
				PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 25)
				PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmSyncThresh0, ofdmSyncThresh0, 100)
				PHY_REG_MOD_ENTRY(SSLPNPHY, HiGainDB, HiGainDB, 70)
			PHY_REG_LIST_EXECUTE(pi);
		}
		PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 360);
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 500);
		}

		wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(pi);
	}
}

void
wlc_sslpnphy_channel_gain_adjust(phy_info_t *pi)
{
	uint8 i;
	uint freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	uint8 pwr_correction = 0;
	uint16 tempsense = 0;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
#ifdef BAND5G
	uint16 chan_info_sslpnphy_cga_5g[24] = { 5180, 5200, 5220, 5240, 5260, 5280, 5300, 5320,
		5500, 5520, 5540, 5560, 5580, 5600, 5620, 5640,
		5660, 5680, 5700, 5745, 5765, 5785, 5805, 5825,
		};
#endif

	if (sslpnphy_specific->fem_combiner_current_state) {
		/* Saving tempsense value for combiner mode */
		tempsense = phy_reg_read(pi, SSLPNPHY_TempSenseCorrection);
	}

	/* Reset the tempsense offset */
	PHY_REG_WRITE(pi, SSLPNPHY, TempSenseCorrection, 0);
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy); i++)
			if (chan_info_2063_sslpnphy[i].freq == freq) {
				pwr_correction = (uint8)sslpnphy_specific->sslpnphy_cga_2g[i];
				break;
			}
	}
#ifdef BAND5G
	 else {
		for (i = 0; i < ARRAYSIZE(chan_info_sslpnphy_cga_5g); i++)
			if (freq <= chan_info_sslpnphy_cga_5g[i]) {
				pwr_correction = (uint8)sslpnphy_specific->sslpnphy_cga_5g[i];
				break;
			}
	}
#endif

	/* Apply the channel based offset to each 5G channel + original tempsense */
	PHY_REG_WRITE(pi, SSLPNPHY, TempSenseCorrection, pwr_correction + tempsense);

}
#ifdef BAND5G
static void wlc_sslpnphy_radio_2063_channel_tweaks_A_band(phy_info_t *pi, uint freq);
#endif

static bool wlc_sslpnphy_fcc_chan_check(phy_info_t *pi, uint channel);
void
wlc_phy_chanspec_set_sslpnphy(phy_info_t *pi, chanspec_t chanspec)
{
	uint8 channel = CHSPEC_CHANNEL(chanspec); /* see wlioctl.h */
	uint16 bw = PHY_CHSPEC_BW(chanspec);
	uint32 centreTs20, centreFactor;
	uint freq = 0;
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
	uint32 boardtype = BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;

	if (!pi->init_in_progress) {
#ifdef PS4319XTRA
		if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)
			WL_WRITE_SHM(pi, M_PS4319XTRA, 0);
#endif /* PS4319XTRA */
		wlc_sslpnphy_percal_flags_off(pi);
	}

#endif /* PHYHAL */

	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	/* Resetting OLYMPIC flag */
	WL_WRITE_SHM(pi, M_SSLPN_OLYMPIC, 0);

	wlc_phy_chanspec_radio_set((wlc_phy_t *)pi, chanspec);
	/* Set the phy bandwidth as dictated by the chanspec */
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (bw != pi->bw) {
#if !defined(ROMTERMPHY)
			wlapi_bmac_bw_set(pi->sh->physhim, bw);
#else
			wlc_set_bw(pi->wlc, bw);
#endif
		}
	}

	wlc_sslpnphy_set_chanspec_default_tweaks(pi);

	freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	/* Tune radio for the channel */
	if (!NORADIO_ENAB(pi->pubpi)) {
		wlc_sslpnphy_radio_2063_channel_tune(pi, channel);

		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			if (IS40MHZ(pi)) {
				if (CHSPEC_SB_UPPER(chanspec)) {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U,
							DSSSDetectionEnable, 1)
						PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L,
							DSSSDetectionEnable, 0)
					PHY_REG_LIST_EXECUTE(pi);
				} else {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U,
							DSSSDetectionEnable, 0)
						PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L,
							DSSSDetectionEnable, 1)
					PHY_REG_LIST_EXECUTE(pi);
				}
			}
		}
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			if (CHSPEC_IS40(chanspec)) {
				if (sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4) {
					if (channel == 3) {
						write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xda);
					} else {
						write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xde);
					}
				} else {
					if (channel == 3 || channel == 9) {
						PHY_REG_LIST_START
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl0,
								0xfff << 4, (0xe << 12) | (0x0 << 4))
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl1,
								0xfff, (0x18 << 4) | (0x2 << 0))
						PHY_REG_LIST_EXECUTE(pi);
					} else {
						PHY_REG_LIST_START
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl0,
								0xfff << 4, (0x0 << 12) | (0x2e << 4))
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl1,
								0xfff, (0x0b << 4) | (0x0 << 0))
						PHY_REG_LIST_EXECUTE(pi);
					}
				}
			} else {
				if (sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4) {
					if (channel == 1) {
						write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xda);
					} else {
						write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xde);
					}
				}
			}
			if (channel == 14) {
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 5)
					PHY_REG_MOD_ENTRY(SSLPNPHY, lpphyCtrl, txfiltSelect, 2)
				PHY_REG_LIST_EXECUTE(pi);
				wlc_sslpnphy_cck_filt_load(pi, 12);
			} else {
				PHY_REG_MOD(pi, SSLPNPHY, lpphyCtrl, txfiltSelect, 1);
				wlc_sslpnphy_cck_filt_load(pi, sslpnphy_specific->sslpnphy_cck_filt_sel);
			}
		} else {
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
			if (channel == 14) {
				PHY_REG_LIST_START
					PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl1, 0xfff, 0x15 << 4)
					PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl0, 0xfff << 4, 0x240 << 4)
					PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl4, 0xff << 7, 0x80 << 7)
					PHY_REG_MOD_ENTRY(SSLPNPHY, lpphyCtrl, txfiltSelect, 2)
					PHY_REG_MOD_ENTRY(SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 2)
				PHY_REG_LIST_EXECUTE(pi);
				wlc_sslpnphy_cck_filt_load(pi, 4);
			} else {
				PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl0, sslpnphy_specific->sslpnphy_extstxctrl0);
				PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl1, sslpnphy_specific->sslpnphy_extstxctrl1);
				PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl4, sslpnphy_specific->sslpnphy_extstxctrl4);
				PHY_REG_MOD(pi, SSLPNPHY, lpphyCtrl, txfiltSelect, 1);
				wlc_sslpnphy_cck_filt_load(pi, sslpnphy_specific->sslpnphy_cck_filt_sel);
			}
			/* Do not do these FCC tunings for 40Mhz */
			if (SSLPNREV_GE(pi->pubpi.phy_rev, 2) && IS20MHZ(pi) && (channel != 14)) {
				/* Gurus FCC changes */
				if ((channel == 1) || (channel == 11)) {
					PHY_REG_LIST_START
						PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl0, 0xfff << 4, 0x040 << 4)
						PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl1, 0xf, 0)
					PHY_REG_LIST_EXECUTE(pi);
				} else {
					PHY_REG_LIST_START
						PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl0, 0xfff << 4, 0x035 << 4)
						PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl1, 0xf, 0)
					PHY_REG_LIST_EXECUTE(pi);
				}
			}
			if (SSLPNREV_LE(pi->pubpi.phy_rev, 1) && (channel != 14)) {
				if (wlc_sslpnphy_fcc_chan_check(pi, channel)) {
					PHY_REG_LIST_START
						PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl0, 0xfff << 4, 0xf00 << 4)
						PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl1, 0xfff, 0x0c3)
						PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl3, 0xff, 0x90)
					PHY_REG_LIST_EXECUTE(pi);

					if (boardtype == BCM94329OLYMPICX17M_SSID ||
					    boardtype == BCM94329OLYMPICX17U_SSID) {

						PHY_REG_LIST_START
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl0,
								0xfff << 4, 0x800 << 4)
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl1, 0xfff, 0x254)
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl3, 0xff, 0xc0)
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl2,
								0xf << 7, 0xa << 7)
							PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl4,
								0xf << 11, 0x9 << 11)
							PHY_REG_MOD_ENTRY(SSLPNPHY, lpfbwlutreg1, lpfbwlut5, 1)
						PHY_REG_LIST_EXECUTE(pi);

						wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
							sslpnphy_rev1_cx_ofdm_fcc, 10);
						wlc_sslpnphy_load_filt_coeff(pi,
							SSLPNPHY_txrealfilt_ofdm_tap0,
							sslpnphy_rev1_real_ofdm_fcc, 5);
						wlc_sslpnphy_load_filt_coeff(pi,
							SSLPNPHY_txrealfilt_ht_tap0,
							sslpnphy_rev1_real_ht_fcc, 5);

						/* ch 1 and ch11 is set to class A and requires special PAPD tweaks */
						sslpnphy_specific->sslpnphy_radio_classA = TRUE;
						sslpnphy_specific->sslpnphy_papd_tweaks_enable = TRUE;
						sslpnphy_specific->sslpnphy_papd_tweaks.final_idx_thresh = 42000;
						/* 1.6 max in PAPD LUT */
						sslpnphy_specific->sslpnphy_papd_tweaks.papd_track_pa_lut_begin = 5500;
						sslpnphy_specific->sslpnphy_papd_tweaks.papd_track_pa_lut_step = 0x222;
						/* 0.5dB step to cover broader range */
						sslpnphy_specific->sslpnphy_papd_tweaks.min_final_idx_thresh = 5000;
						/* lower limit for papd cal index search */

					} else if ((boardtype == BCM94329OLYMPICN90U_SSID) ||
					           (boardtype == BCM94329OLYMPICLOCO_SSID) ||
					           (boardtype == BCM94329OLYMPICN90M_SSID)) {
						/* Moving to more class A radio settings */
						sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
						WL_WRITE_SHM(pi, M_SSLPN_OLYMPIC, 2);
						PHY_REG_LIST_START
							PHY_REG_WRITE_ENTRY(SSLPNPHY, extstxctrl2, 0x82d8)
							PHY_REG_WRITE_ENTRY(SSLPNPHY, extstxctrl4, 0x405c)
						PHY_REG_LIST_EXECUTE(pi);
						WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
							M_SSLPNPHY_REG_4F2_CCK)), 0x2280);
						WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
							M_SSLPNPHY_REG_4F3_CCK)), 0x3150);

						if (channel == 1) {
							PHY_REG_LIST_START
								PHY_REG_WRITE_ENTRY(SSLPNPHY, extstxctrl0, 0xA290)
								PHY_REG_WRITE_ENTRY(SSLPNPHY, extstxctrl1, 0x3330)
								PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl3, 0xff, 0xF0)
								PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl5,
									0xf << 8, 0xC << 8)
							PHY_REG_LIST_EXECUTE(pi);
							WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
								M_SSLPNPHY_REG_4F2_16_64)), 0xA290);
							WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
								M_SSLPNPHY_REG_4F3_16_64)), 0x3330);
							WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
								M_SSLPNPHY_REG_4F2_2_4)), 0xF000);
							WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
								M_SSLPNPHY_REG_4F3_2_4)), 0x33A3);
							/* ch 1 is set to class A and requires special PAPD tweaks */
							sslpnphy_specific->
							        sslpnphy_radio_classA = TRUE;
							sslpnphy_specific->
							        sslpnphy_papd_tweaks_enable = TRUE;
							sslpnphy_specific->
							        sslpnphy_papd_tweaks.final_idx_thresh = 40000;
							/* 1.55 max in PAPD LUT */
							sslpnphy_specific->
							        sslpnphy_papd_tweaks.papd_track_pa_lut_begin = 5500;
							sslpnphy_specific->
							        sslpnphy_papd_tweaks.papd_track_pa_lut_step = 0x222;
							/* 0.5dB step to cover broader range */
							sslpnphy_specific->
							        sslpnphy_papd_tweaks.min_final_idx_thresh = 10000;
							/* lower limit for papd cal index search */
						}
						else if (channel == 11) {
							PHY_REG_LIST_START
								PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl3, 0xff, 0xC0)
								PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_extstxctrl5,
									0xf << 8, 0xc << 8)
								PHY_REG_WRITE_ENTRY(SSLPNPHY, extstxctrl0, 0x9210)
								PHY_REG_WRITE_ENTRY(SSLPNPHY, extstxctrl1, 0x3150)
							PHY_REG_LIST_EXECUTE(pi);
							WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
								M_SSLPNPHY_REG_4F2_16_64)), 0x9210);
							WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
								M_SSLPNPHY_REG_4F3_16_64)), 0x3150);
							WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
								M_SSLPNPHY_REG_4F2_2_4)), 0x1000);
							WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
								M_SSLPNPHY_REG_4F3_2_4)), 0x30c3);
						}
					}

				} else {
					PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl0,
						sslpnphy_specific->sslpnphy_extstxctrl0);
					PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl1,
						sslpnphy_specific->sslpnphy_extstxctrl1);
					PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl3,
						sslpnphy_specific->sslpnphy_extstxctrl3);
					PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl2,
						sslpnphy_specific->sslpnphy_extstxctrl2);
					PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl4,
						sslpnphy_specific->sslpnphy_extstxctrl4);
					PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl5,
						sslpnphy_specific->sslpnphy_extstxctrl5);
					PHY_REG_WRITE(pi, SSLPNPHY, lpfbwlutreg1,
						sslpnphy_specific->sslpnphy_ofdm_filt_bw);

					if (boardtype == BCM94329OLYMPICX17M_SSID ||
					    boardtype == BCM94329OLYMPICX17U_SSID) {
						if ((sslpnphy_specific->sslpnphy_fabid == 2) ||
						    (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
							wlc_sslpnphy_load_filt_coeff(pi,
								SSLPNPHY_ofdm_tap0_i,
								sslpnphy_rev1_cx_ofdm_sec, 10);
							wlc_sslpnphy_load_filt_coeff(pi,
								SSLPNPHY_txrealfilt_ofdm_tap0,
								sslpnphy_rev1_real_ofdm_sec, 5);
							wlc_sslpnphy_load_filt_coeff(pi,
								SSLPNPHY_txrealfilt_ht_tap0,
								sslpnphy_rev1_real_ht_sec, 5);
						} else {
							wlc_sslpnphy_load_filt_coeff(pi,
								SSLPNPHY_ofdm_tap0_i,
								sslpnphy_rev1_cx_ofdm, 10);
							wlc_sslpnphy_load_filt_coeff(pi,
								SSLPNPHY_txrealfilt_ofdm_tap0,
								sslpnphy_rev1_real_ofdm, 5);
							wlc_sslpnphy_load_filt_coeff(pi,
								SSLPNPHY_txrealfilt_ht_tap0,
								sslpnphy_rev1_real_ht, 5);
						}
					}
				}
			}
			}
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (freq < 5725)
			wlc_sslpnphy_load_tx_gain_table(pi, sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_midband);
		else
			wlc_sslpnphy_load_tx_gain_table(pi, sslpnphy_specific->sslpnphy_tx_gaintbl_5GHz_hiband);

		wlc_sslpnphy_radio_2063_channel_tweaks_A_band(pi, freq);
	}
#endif /* BAND5G */
		OSL_DELAY(1000);
	}
	}
	/* apply hannel based power offset to reduce power control errors */
	wlc_sslpnphy_channel_gain_adjust(pi);
	/* toggle the afe whenever we move to a new channel */
	wlc_sslpnphy_toggle_afe_pwdn(pi);

#ifdef ROMTERMPHY
	pi->radio_code = channel;
#endif
	centreTs20 = wlc_lpphy_qdiv_roundup(freq * 2, 5, 0);
	centreFactor = wlc_lpphy_qdiv_roundup(2621440, freq, 0);
	PHY_REG_WRITE(pi, SSLPNPHY, ptcentreTs20, (uint16)centreTs20);
	PHY_REG_WRITE(pi, SSLPNPHY, ptcentreFactor, (uint16)centreFactor);

	write_phy_channel_reg(pi, channel);

	/* Indicate correct antdiv register offset to ucode */
	if (CHSPEC_IS40(chanspec))
		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr + M_SSLPNPHY_ANTDIV_REG)),
			SSLPNPHY_Rev2_crsgainCtrl_40);
	else
		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr + M_SSLPNPHY_ANTDIV_REG)),
			SSLPNPHY_Rev2_crsgainCtrl);

	/* Enable Dmof Bhy ACI filter for warrior alone */
	if (boardtype == BCM94319LCSDN4L_SSID)
		PHY_REG_WRITE(pi, SSLPNPHY, bphyacireg, 0x4);

	sslpnphy_specific->sslpnphy_papd_cal_done = 0;

#if !defined(ROMTERMPHY)
	if (!pi->init_in_progress) {
		wlc_sslpnphy_set_chanspec_tweaks(pi, pi->radio_chanspec);
		/* Common GainTable For Rx/ACI Tweaks Adding Here */
		wlc_sslpnphy_CmRxAciGainTbl_Tweaks(pi);

		wlc_sslpnphy_rx_offset_init(pi);
		if (!(SCAN_IN_PROGRESS(pi) || WLC_RM_IN_PROGRESS(pi))) {
			wlc_sslpnphy_setchan_cal(pi, chanspec);
		}
		wlc_sslpnphy_temp_adj(pi);

#ifdef PS4319XTRA
		if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)
			WL_WRITE_SHM(pi, M_PS4319XTRA, PS4319XTRA);

#endif /* PS4319XTRA */
	}
#endif /* #if !defined(ROMTERMPHY) */

}
static void
wlc_sslpnphy_RxNvParam_Adj(phy_info_t *pi)
{
	int8 path_loss = 0, temp1;
	uint8 channel = CHSPEC_CHANNEL(pi->radio_chanspec); /* see wlioctl.h */
	uint8 phybw40 = IS40MHZ(pi);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		path_loss = (int8)sslpnphy_specific->sslpnphy_rx_power_offset;
#ifdef BAND5G
	} else {
		path_loss = (int8)sslpnphy_specific->sslpnphy_rx_power_offset_5g;
#endif
	}
	temp1 = (int8)PHY_REG_READ(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (phybw40)
			temp1 = (int8)PHY_REG_READ(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb);
	}
	if ((sslpnphy_specific->sslpnphy_rxpo2gchnflg) && (CHSPEC_IS2G(pi->radio_chanspec))) {
		if (sslpnphy_specific->sslpnphy_rxpo2gchnflg & (0x1 << (channel - 1)))
			temp1 -= path_loss;
	} else {
		temp1 -= path_loss;
	}
	PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, temp1);
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (phybw40)
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, (temp1));
	}


}

STATIC uint16 ELNA_CCK_ACI_GAINTBL_TWEAKS [][2] = {
	{50,  0x24C},
	{51,  0x233},
	{52,  0x24D},
	{53,  0x24E},
	{54,  0x240},
	{55,  0x24F},
	{56,  0x2C0},
	{57,  0x2D0},
	{58,  0x2D8},
	{59,  0x2AF},
	{60,  0x2B9},
	{61,  0x32F},
	{62,  0x33A},
	{63,  0x33B},
	{64,  0x33C},
	{65,  0x33D},
	{66,  0x33E},
	{67,  0x3BF},
	{68,  0x3BE},
	{69,  0x3C2},
	{70,  0x3C1},
	{71,  0x3C4},
	{72,  0x3B0},
	{73,  0x3B1}
};
STATIC uint16 ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS [][2] = {
	{23,  0x381},
	{24,  0x385},
	{25,  0x3D4},
	{26,  0x3D5},
	{27,  0x389},
	{28,  0x3AB},
	{29,  0x3D6},
	{30,  0x3D8},
	{31,  0x38E},
	{32,  0x38C},
	{33,  0x38A}
};
STATIC uint16 ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS [][2] = {
	{61,  0x28F },
	{62,  0x32C },
	{63,  0x33E },
	{64,  0x33F },
	{65,  0x3C3 },
	{66,  0x3AE },
	{67,  0x3CA },
	{68,  0x3C9 },
	{69,  0x387 },
	{70,  0x3CB },
	{71,  0x3CE },
	{72,  0x3CF },
	{73,  0x3D0 }
};
STATIC uint16 ELNA_OLYMPIC_OFDM_5G_GAINTBL_TWEAKS [][2] = {
	{0,  0x200 },
	{1,  0x200 },
	{2,  0x200 },
	{3,  0x200 },
	{4,  0x200 },
	{5,  0x200 },
	{6,  0x200 },
	{7,  0x200 },
	{8,  0x200 },
	{9,  0x200 },
	{10,  0x251 },
	{11,  0x252 },
	{12,  0x2D1 },
	{13,  0x2D2 },
	{14,  0x2CC },
	{15,  0x2B3 },
	{16,  0x2A7 },
	{17,  0x286 },
	{18,  0x28B },
	{19,  0x290 },
	{20,  0x38F },
	{21,  0x393 },
	{22,  0x38B },
	{23,  0x390 },
	{24,  0x38C },
	{25,  0x38E },
	{26,  0x38D },
	{27,  0x3A3 },
	{28,  0x388 }
};
STATIC uint16 ELNA_OLYMPIC_OFDM_5G_GAINTBL_TSMC_TWEAKS [][2] = {
	{18,  0x387 },
	{19,  0x38A },
	{20,  0x38F },
	{21,  0x3A6 },
	{22,  0x3BE },
	{23,  0x3BF },
	{24,  0x3C0 },
	{25,  0x3C5 },
	{26,  0x397 }
};
STATIC uint16 UNO_LOCO_ELNA_CCK_GAINTBL_TWEAKS [][2] = {
	{39,  0x651},
	{40,  0x680},
	{41,  0x6D1},
	{42,  0x700},
	{43,  0x751},
	{44,  0x780},
	{45,  0x7D1},
	{46,  0x7D2}
};
STATIC uint16 NINJA_SDNA_5G_20MHZ_GAINTBL_TWEAKS [][2] = {
	{0,   0x600},
	{1,   0x000},
	{2,   0x051},
	{3,   0x080},
	{4,   0x0D1},
	{5,   0x0D2},
	{6,   0x100},
	{7,   0x151},
	{8,   0x08A},
	{9,   0x08B},
	{10,  0x10D},
	{11,  0x280},
	{12,  0x2D1},
	{13,  0x2D2},
	{14,  0x2D3}
};
STATIC uint16 NINJA_SDNA_5G_40MHZ_GAINTBL_TWEAKS [][2] = {
	{0,   0x600},
	{1,   0x051},
	{2,   0x080},
	{3,   0x0D1},
	{4,   0x0D2},
	{5,   0x100},
	{6,   0x151},
	{7,   0x08A},
	{8,   0x08B},
	{9,   0x10A},
	{10,  0x10B},
	{20,  0x382},
	{21,  0x383},
	{22,  0x3d7},
	{23,  0x3ab},
	{24,  0x38c},
	{25,  0x38e},
	{26,  0x397},
	{27,  0x39f},
	{28,  0x39e},
	{29,  0x3b0},
	{30,  0x3dd},
	{31,  0x3de},
	{32,  0x3df},
	{33,  0x392},
	{34,  0x399},
	{35,  0x3a5},
	{36,  0x3bb}
};
uint8 ELNA_CCK_ACI_GAINTBL_TWEAKS_sz = ARRAYSIZE(ELNA_CCK_ACI_GAINTBL_TWEAKS);
uint8 ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS_sz = ARRAYSIZE(ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS);
uint8 ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS_sz = ARRAYSIZE(ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS);
uint8 ELNA_OLYMPIC_OFDM_5G_GAINTBL_TWEAKS_sz = ARRAYSIZE(ELNA_OLYMPIC_OFDM_5G_GAINTBL_TWEAKS);
uint8 ELNA_OLYMPIC_OFDM_5G_GAINTBL_TSMC_TWEAKS_sz = ARRAYSIZE(ELNA_OLYMPIC_OFDM_5G_GAINTBL_TSMC_TWEAKS);
uint8 UNO_LOCO_ELNA_CCK_GAINTBL_TWEAKS_sz = ARRAYSIZE(UNO_LOCO_ELNA_CCK_GAINTBL_TWEAKS);
uint8 NINJA_SDNA_5G_20MHZ_GAINTBL_TWEAKS_sz = ARRAYSIZE(NINJA_SDNA_5G_20MHZ_GAINTBL_TWEAKS);
uint8 NINJA_SDNA_5G_40MHZ_GAINTBL_TWEAKS_sz = ARRAYSIZE(NINJA_SDNA_5G_40MHZ_GAINTBL_TWEAKS);
static void
wlc_sslpnphy_rx_gain_table_tweaks(phy_info_t *pi, uint8 idx, uint16 ptr[][2], uint8 array_size)
{
	uint8 i = 0, tbl_entry;
	uint16 wlprio_11bit_code;
	uint32 tableBuffer[2];
	for (i = 0; i < array_size; i++) {
		if (ptr[i][0] == idx) {
			wlprio_11bit_code = ptr[i][1];
			tbl_entry = idx << 1;
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, tbl_entry);
			tableBuffer[0] = (tableBuffer[0] & 0x0FFFFFFF)
				| ((wlprio_11bit_code & 0x00F) << 28);
			tableBuffer[1] = (tableBuffer[1] & 0xFFFFFF80)
				| ((wlprio_11bit_code & 0x7F0) >> 4);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, tbl_entry);
		}
	}
}

void
wlc_sslpnphy_CmRxAciGainTbl_Tweaks(void *arg)
{
	phy_info_t *pi = (phy_info_t *)arg;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

	uint32 tblBuffer[2];
	uint32 lnaBuffer[2] = {0, 9};
	uint8 phybw40 = IS40MHZ(pi), i;

	bool extlna = ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA_5GHz) &&
		CHSPEC_IS5G(pi->radio_chanspec)) ||
		((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
		CHSPEC_IS2G(pi->radio_chanspec));

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, ClipCtrThreshHiGain, 18)
			PHY_REG_MOD_ENTRY(SSLPNPHY, MinPwrLevel, ofdmMinPwrLevel, 162)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6333)
			PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 8)
			PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatchMedGainEx, medHiGainDirectMismatchOFDMDet, 6)
			PHY_REG_MOD_ENTRY(SSLPNPHY, crsMiscCtrl2, eghtSmplFstPwrLogicEn, 1)
		PHY_REG_LIST_EXECUTE(pi);

		if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17_SSID) ||
			(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID) ||
			(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID)) {
			lnaBuffer[0] = 0xDD;
			lnaBuffer[1] = 0x9;

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 5)
				PHY_REG_MOD_ENTRY(SSLPNPHY, gaindirectMismatch, MedHigainDirectMismatch, 12)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6363)
			PHY_REG_LIST_EXECUTE(pi);

			for (i = 0; i <= 28; i++) {
				wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
					ELNA_OLYMPIC_OFDM_5G_GAINTBL_TWEAKS,
					ELNA_OLYMPIC_OFDM_5G_GAINTBL_TWEAKS_sz);
			}
			if ((sslpnphy_specific->sslpnphy_fabid == 2) ||
			    (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
				PHY_REG_LIST_START
					PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6666)
					PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh1, PktRxSignalDropThresh, 15)
					PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatch, GainmisMatchPktRx, 9)
				PHY_REG_LIST_EXECUTE(pi);
				for (i = 18; i <= 26; i++) {
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						ELNA_OLYMPIC_OFDM_5G_GAINTBL_TSMC_TWEAKS,
						ELNA_OLYMPIC_OFDM_5G_GAINTBL_TSMC_TWEAKS_sz);
				}
			}
		}
		if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDELNA6L_SSID) ||
		      (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, 9)
				PHY_REG_MOD_ENTRY(SSLPNPHY, crsgainCtrl, phycrsctrl, 0xf)
				PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 0x14)
				PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatchMedGainEx, medHiGainDirectMismatchOFDMDet, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, MinPwrLevel, ofdmMinPwrLevel, 0xa4)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 0)
			PHY_REG_LIST_EXECUTE(pi);

			if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) {
				lnaBuffer[0] = -7;
				lnaBuffer[1] = 13;

				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, HiGainDB_40, HiGainDB, 67)
					PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatchMedGainEx,
						medHiGainDirectMismatchOFDMDet, 3)
					/* PR: 90993 */
					PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_Rev2_gainMismatchMedGainEx_40,
						SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_MASK,
						5 <<
						SSLPNPHY_gainMismatchMedGainEx_medHiGainDirectMismatchOFDMDet_SHIFT)
					PHY_REG_MOD_ENTRY(SSLPNPHY, MinPwrLevel, ofdmMinPwrLevel, -95)
					PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_Rev2_MinPwrLevel_40,
						SSLPNPHY_MinPwrLevel_ofdmMinPwrLevel_MASK,
						-95 << SSLPNPHY_MinPwrLevel_ofdmMinPwrLevel_SHIFT)
				PHY_REG_LIST_EXECUTE(pi);

				if (phybw40) { /* 40 MHz */
					for (i = 0; i <= 10; i++) {
						wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						    NINJA_SDNA_5G_40MHZ_GAINTBL_TWEAKS,
						    NINJA_SDNA_5G_40MHZ_GAINTBL_TWEAKS_sz);
				    }

					for (i = 20; i <= 36; i++) {
						wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						    NINJA_SDNA_5G_40MHZ_GAINTBL_TWEAKS,
						    NINJA_SDNA_5G_40MHZ_GAINTBL_TWEAKS_sz);
				    }

					tblBuffer[0] = 0x5;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
						tblBuffer, 1, 16, 10);

					tblBuffer[0] = 0x9;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
						tblBuffer, 1, 16, 11);

				} else { /* 20 MHz */
					for (i = 0; i <= 14; i++) {
						wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						    NINJA_SDNA_5G_20MHZ_GAINTBL_TWEAKS,
						    NINJA_SDNA_5G_20MHZ_GAINTBL_TWEAKS_sz);
				    }

					tblBuffer[0] = 0x5;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
						tblBuffer, 1, 16, 10);

					tblBuffer[0] = 0x9;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
						tblBuffer, 1, 16, 11);

					tblBuffer[0] = 0x1;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_TBL,
						tblBuffer, 1, 16, 13);
				}
			}
		}
	}
#endif /* BAND5G */
	if ((SSLPNREV_IS(pi->pubpi.phy_rev, 1)) && (CHSPEC_IS2G(pi->radio_chanspec))) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, DSSSConfirmCnt, DSSSConfirmCntLoGain, 4)
			PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 11)
			PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatchMedGainEx, medHiGainDirectMismatchOFDMDet, 6)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6666)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 360)
		PHY_REG_LIST_EXECUTE(pi);
		if ((sslpnphy_specific->sslpnphy_fabid == 2) || (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
			write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0x33);
			PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 340);
		}

		if (!(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {
			if ((sslpnphy_specific->sslpnphy_fabid != 2) &&
			    (sslpnphy_specific->sslpnphy_fabid_otp != TSMC_FAB12)) {
				/* write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0xF6); */
				PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 8);
				/* PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 330); */
			}
			if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329TDKMDL11_SSID) {
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 12)
					PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipCtrDefThresh, 12)
				PHY_REG_LIST_EXECUTE(pi);
			}
		} else if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) {
			PHY_REG_LIST_START
				PHY_REG_MOD2_ENTRY(SSLPNPHY, radioTRCtrlCrs2,
					trTransAddrLmtOfdm, trTransAddrLmtDsss, 6, 6)
				PHY_REG_MOD_ENTRY(SSLPNPHY, LowGainDB, MedLowGainDB, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 18)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipCtrDefThresh, 12)
			PHY_REG_LIST_EXECUTE(pi);

			/* Rx ELNA Boards ACI Improvements W/o uCode Interventions */
			if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) != BCM94329OLYMPICN18_SSID) {
				if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) != BCM94329OLYMPICLOCO_SSID) &&
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) != BCM94329OLYMPICUNO_SSID)) {
					PHY_REG_LIST_START
						PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipThresh, 63)
						PHY_REG_MOD_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, 4)
					PHY_REG_LIST_EXECUTE(pi);
				} else {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, 7)
						PHY_REG_MOD_ENTRY(SSLPNPHY, gainidxoffset, dsssgainidxtableoffset, 247)
						PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs2, trTransAddrLmtDsss, 9)
					PHY_REG_LIST_EXECUTE(pi);
					for (i = 39; i <= 46; i++) {
						wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
							UNO_LOCO_ELNA_CCK_GAINTBL_TWEAKS,
							UNO_LOCO_ELNA_CCK_GAINTBL_TWEAKS_sz);
					}
				}
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY, DSSSConfirmCnt, DSSSConfirmCntLoGain, 2)
					PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipCtrDefThresh, 20)
					PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 12)
				PHY_REG_LIST_EXECUTE(pi);
			}
			if (!sslpnphy_specific->sslpnphy_OLYMPIC) {
				/* Gain idx tweaking for 2g band of dual band board */
				tblBuffer[0] = 0xc0000001;
				tblBuffer[1] = 0x0000006c;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 14);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 88);
				tblBuffer[0] = 0x70000002;
				tblBuffer[1] = 0x0000006a;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 16);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 90);
				tblBuffer[0] = 0x20000002;
				tblBuffer[1] = 0x0000006b;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 18);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 92);
				tblBuffer[0] = 0xc020c287;
				tblBuffer[1] = 0x0000002c;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 30);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 104);
				tblBuffer[0] = 0x30410308;
				tblBuffer[1] = 0x0000002b;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 32);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 106);
				PHY_REG_LIST_START
					PHY_REG_MOD2_ENTRY(SSLPNPHY, radioTRCtrlCrs2,
						trTransAddrLmtOfdm, trTransAddrLmtDsss, 9, 9)
					PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 22)
					PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmSyncThresh0, ofdmSyncThresh0, 120)
					PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, ClipCtrThreshHiGain, 18)
				PHY_REG_LIST_EXECUTE(pi);
				tblBuffer[0] = 0x51e64d96;
				tblBuffer[1] = 0x0000003c;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 54);
				tblBuffer[0] = 0x6204ca9e;
				tblBuffer[1] = 0x0000003c;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 56);
				for (i = 50; i <= 73; i++) {
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						ELNA_CCK_ACI_GAINTBL_TWEAKS,
						ELNA_CCK_ACI_GAINTBL_TWEAKS_sz);
				}
			} else if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90M_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90U_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329MOTOROLA_SSID)) {
				lnaBuffer[0] = 0;
				lnaBuffer[1] = 12;

				for (i = 23; i <= 33; i++) {
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS,
						ELNA_OLYMPIC_OFDM_ACI_GAINTBL_TWEAKS_sz);
				}
				for (i = 61; i <= 71; i++) {
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS,
						ELNA_OLYMPIC_CCK_ACI_GAINTBL_TWEAKS_sz);
				}
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY, HiGainDB, HiGainDB, 73)
					PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 360)
					PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatchMedGainEx,
						medHiGainDirectMismatchOFDMDet, 3)
					PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, gainReqTrAttOnEnByCrs, 0)
					PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrl, gainrequestTRAttnOnEn, 0)
					PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh1, PktRxSignalDropThresh, 15)
					PHY_REG_MOD_ENTRY(SSLPNPHY, gainMismatch, GainmisMatchPktRx, 9)
					PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 8)
					PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 15)
					PHY_REG_MOD_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, 9)
				PHY_REG_LIST_EXECUTE(pi);
				/* if ((sslpnphy_specific->sslpnphy_fabid == 2) ||
					(sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
				tblBuffer[0] = 0x651123C7;
				tblBuffer[1] = 0x00000008;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 30);
				}
				*/
			}
		}
	} else if ((SSLPNREV_GE(pi->pubpi.phy_rev, 2)) &&
		(CHSPEC_IS2G(pi->radio_chanspec))) {
		if (!phybw40) {
			PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 360);
			write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0xF0); /* Dflt Value */
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, gainReqTrAttOnEnByCrs, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrl, gainrequestTRAttnOnEn, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 11)
				PHY_REG_MOD3_ENTRY(SSLPNPHY, DSSSConfirmCnt,
					DSSSConfirmCntHiGain, DSSSConfirmCntLoGain, DSSSConfirmCntHiGainCnfrm, 4, 4, 2)
				PHY_REG_MOD_ENTRY(SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 20)
				PHY_REG_MOD_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, 9)
				PHY_REG_MOD2_ENTRY(SSLPNPHY, lnsrOfParam4, ofMaxPThrUpdtThresh, oFiltSyncCtrShft, 0, 2)
				PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmSyncThresh0, ofdmSyncThresh0, 120)
			PHY_REG_LIST_EXECUTE(pi);
		}
		else if (phybw40) {
		        PHY_REG_WRITE(pi, SSLPNPHY_Rev2, nfSubtractVal_40, 320);
			write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0xF6);
		}
		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) != BCM94319WLUSBN4L_SSID) {
			tblBuffer[0] = 0x0110;
			tblBuffer[1] = 0x0101;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SW_CTRL,
				tblBuffer, 2, 32, 0);

			tblBuffer[0] = 0xb0000000;
			tblBuffer[1] = 0x00000040;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 0);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 74);
			tblBuffer[0] = 0x00000000;
			tblBuffer[1] = 0x00000048;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 2);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 76);
			tblBuffer[0] = 0xb0000000;
			tblBuffer[1] = 0x00000048;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 4);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 78);
			tblBuffer[0] = 0xe0000000;
			tblBuffer[1] = 0x0000004d;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 6);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 80);
			tblBuffer[0] = 0xc0000000;
			tblBuffer[1] = 0x0000004c;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 8);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 82);
			tblBuffer[0] = 0x00000000;
			tblBuffer[1] = 0x00000058;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tblBuffer, 2, 32, 10);
			if (phybw40) {
				tblBuffer[0] = 0xb0000000;
				tblBuffer[1] = 0x00000058;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 12);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tblBuffer, 2, 32, 86);
			}
			PHY_REG_MOD(pi, SSLPNPHY, ClipCtrThresh, clipCtrThreshLoGain, 12);
		}
		if  (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, VeryLowGainDB, veryLowGainDB, 0xc)
				PHY_REG_MOD2_ENTRY(SSLPNPHY, lnsrOfParam4,
					ofMaxPThrUpdtThresh, oFiltSyncCtrShft, 0xb, 1)
				PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmSyncThresh0, ofdmSyncThresh0, 100)
			PHY_REG_LIST_EXECUTE(pi);
			lnaBuffer[0] = 0;
			lnaBuffer[1] = 0;
		}
	}
	if (extlna) {
		PHY_REG_MOD(pi, SSLPNPHY, radioCtrl, extlnaen, 1);
		PHY_REG_MOD(pi, SSLPNPHY, extlnagainvalue0, extlnagain0, lnaBuffer[0]);
		PHY_REG_MOD(pi, SSLPNPHY, extlnagainvalue0, extlnagain1, lnaBuffer[1]);
		wlc_sslpnphy_common_write_table(pi, 17, lnaBuffer, 2, 32, 66);
	}
#ifdef SSLPNLOWPOWER
	write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0x0f);
#endif

	wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(pi);
}

static void
wlc_sslpnphy_set_dac_gain(phy_info_t *pi, uint16 dac_gain)
{
	uint16 dac_ctrl;

	dac_ctrl = PHY_REG_READ(pi, SSLPNPHY, AfeDACCtrl, dac_ctrl);
	dac_ctrl &= 0xc7f;
	dac_ctrl |= (dac_gain << 7);
	PHY_REG_MOD(pi, SSLPNPHY, AfeDACCtrl, dac_ctrl, dac_ctrl);
}

STATIC void
BCMROMFN(wlc_sslpnphy_set_tx_gain_override)(phy_info_t *pi, bool bEnable)
{
	uint16 bit = bEnable ? 1 : 0;

	PHY_REG_MOD(pi, SSLPNPHY, rfoverride2, txgainctrl_ovr, bit);
	PHY_REG_MOD(pi, SSLPNPHY, rfoverride2, stxtxgainctrl_ovr, bit);
	PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvr, dacattctrl_ovr, bit);
}

STATIC uint16
wlc_sslpnphy_get_pa_gain(phy_info_t *pi)
{
	return (uint16)PHY_REG_READ(pi, SSLPNPHY, txgainctrlovrval1, pagain_ovr_val1);
}

STATIC void
BCMROMFN(wlc_sslpnphy_set_tx_gain)(phy_info_t *pi,  sslpnphy_txgains_t *target_gains)
{
	uint16 pa_gain = wlc_sslpnphy_get_pa_gain(pi);

	PHY_REG_MOD(pi, SSLPNPHY, txgainctrlovrval0, txgainctrl_ovr_val0,
		target_gains->gm_gain | (target_gains->pga_gain << 8));
	PHY_REG_MOD(pi, SSLPNPHY, txgainctrlovrval1, txgainctrl_ovr_val1,
		target_gains->pad_gain | (pa_gain << 8));

	PHY_REG_MOD(pi, SSLPNPHY, stxtxgainctrlovrval0, stxtxgainctrl_ovr_val0,
		target_gains->gm_gain | (target_gains->pga_gain << 8));
	PHY_REG_MOD(pi, SSLPNPHY, stxtxgainctrlovrval1, stxtxgainctrl_ovr_val1,
		target_gains->pad_gain | (pa_gain << 8));

	wlc_sslpnphy_set_dac_gain(pi, target_gains->dac_gain);

	/* Enable gain overrides */
	wlc_sslpnphy_enable_tx_gain_override(pi);
}

STATIC void
BCMROMFN(wlc_sslpnphy_set_bbmult)(phy_info_t *pi, uint8 m0)
{
	uint16 m0m1 = (uint16)m0 << 8;
	phytbl_info_t tab;

	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = SSLPNPHY_TBL_ID_IQLOCAL;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	wlc_sslpnphy_write_table(pi, &tab);
}

void
BCMROMFN(wlc_sslpnphy_clear_tx_power_offsets)(phy_info_t *pi)
{
	uint32 data_buf[64];
	phytbl_info_t tab;

	/* Clear out buffer */
	bzero(data_buf, sizeof(data_buf));

	/* Preset txPwrCtrltbl */
	tab.tbl_id = SSLPNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_ptr = data_buf; /* ptr to buf */

	/* Per rate power offset */
	tab.tbl_len = 24; /* # values   */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_RATE_OFFSET;
	wlc_sslpnphy_write_table(pi, &tab);

	/* Per index power offset */
	tab.tbl_len = 64; /* # values   */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_MAC_OFFSET;
	wlc_sslpnphy_write_table(pi, &tab);
}


static void
wlc_sslpnphy_set_tssi_mux(phy_info_t *pi, sslpnphy_tssi_mode_t pos)
{
	if (SSLPNPHY_TSSI_EXT == pos) {
		PHY_REG_AND(pi, SSLPNPHY, extstxctrl1, 0x0 << 12);
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			write_radio_reg(pi, RADIO_2063_EXTTSSI_CTRL_2, 0x20);
		} else {
			write_radio_reg(pi, RADIO_2063_EXTTSSI_CTRL_2, 0x21);
		}
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvr, rssi_muxsel_ovr, 0x01)
			PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 0x02)
		PHY_REG_LIST_EXECUTE(pi);

		write_radio_reg(pi, RADIO_2063_EXTTSSI_CTRL_1, 0x51);

	} else {
		/* Power up internal TSSI */
		PHY_REG_OR(pi, SSLPNPHY, extstxctrl1, 0x01 << 12);
#ifdef BAND5G
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			mod_radio_reg(pi, RADIO_2063_PA_CTRL_1, 0x1 << 2, 0 << 2);
			write_radio_reg(pi, RADIO_2063_PA_CTRL_10, 0x51);

			or_radio_reg(pi, RADIO_2063_PA_SP_1, 0x2);
			or_radio_reg(pi, RADIO_2063_COMMON_07, 0x10);
		} else
#endif
		{
			mod_radio_reg(pi, RADIO_2063_PA_CTRL_1, 0x1 << 2, 1 << 2);
			write_radio_reg(pi, RADIO_2063_PA_CTRL_10, 0x51);
		}

		/* Set TSSI/RSSI mux */
		if (SSLPNPHY_TSSI_POST_PA == pos) {
			PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 0x00);
		} else {
			mod_radio_reg(pi, RADIO_2063_PA_SP_1, 0x01, 1);
			PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 0x04);
		}
	}
}

#define BTCX_FLUSH_WAIT_MAX_MS	  500

bool
wlc_sslpnphy_btcx_override_enable(phy_info_t *pi)
{
#ifdef ROMTERMPHY
	wlc_info_t * wlc_pi = pi->wlc;
	#define wlc_hw wlc_pi
#else
	#define wlc_hw pi->sh
#endif

	bool val = TRUE;
	int delay_val, eci_busy_cnt;
	uint32 eci_m = 0, a2dp;

	/* PAPD_CAL_SEQ_FIX : */
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFinputOverrideVal, BTPriority_ovr_val, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFinputOverride, BTPriority_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);
	}

	if (CHSPEC_IS2G(pi->radio_chanspec) && (wlc_hw->machwcap & MCAP_BTCX)) {
		/* Ucode better be suspended when we mess with BTCX regs directly */
		ASSERT(!(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC));

		/* Enable manual BTCX mode */
		OR_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->btcx_ctrl, BTCX_CTRL_EN | BTCX_CTRL_SW);

		/* Set BT priority & antenna to allow A2DP to catchup */
		AND_REG(GENERIC_PHY_INFO(pi)->osh,
			&pi->regs->btcx_trans_ctrl, ~(BTCX_TRANS_TXCONF | BTCX_TRANS_ANTSEL));

		if (BCMECICOEX_ENAB(wlc_pi)) {
			/* Wait for A2DP to flush all pending data */
			W_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->btcx_eci_addr, 3);
			for (delay_val = 0, eci_busy_cnt = 0;
				delay_val < BTCX_FLUSH_WAIT_MAX_MS * 10; delay_val++) {
				/* Make sure ECI update is not in progress */
				if ((eci_m = R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->btcx_eci_data))
					& 0x4000) {
					if (!(a2dp = (eci_m & 0xf))) {
						/* All A2DP data is flushed */
						goto pri_wlan;
					}
					eci_busy_cnt = 0;
				} else {
					if (++eci_busy_cnt > 1)
						goto pri_wlan;
				}
				OSL_DELAY(100);
			}
			if (delay_val == (BTCX_FLUSH_WAIT_MAX_MS * 10)) {
				WL_ERROR(("wl%d: %s: A2DP flush failed, eci_m: 0x%x\n",
					GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, eci_m));
				val = FALSE;
			}
		} else {
		}

pri_wlan:
		/* Set WLAN priority */
		OR_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->btcx_trans_ctrl, BTCX_TRANS_TXCONF);

		/* Wait for BT activity to finish */
		delay_val = 0;
		while (R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->btcx_stat) & BTCX_STAT_RA) {
			if (delay_val++ > BTCX_FLUSH_WAIT_MAX_MS) {
				WL_ERROR(("wl%d: %s: BT still active\n",
					GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
				val = FALSE;
				break;
			}
			OSL_DELAY(100);
		}

		/* Set WLAN antenna & priority */
		OR_REG(GENERIC_PHY_INFO(pi)->osh,
			&pi->regs->btcx_trans_ctrl, BTCX_TRANS_ANTSEL | BTCX_TRANS_TXCONF);
	}

#ifdef WLMSG_INFORM
	{
		uint32 tsf_l = R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->tsf_timerlow);

		WL_INFORM(("wl%d: %s: Waited %u us\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
			R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->tsf_timerlow) - tsf_l));
	}
#endif /* WLMSG_INFORM */

	return val;
}

void
wlc_sslpnphy_tx_pwr_update_npt(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	if (wlc_phy_tpc_isenabled_sslpnphy(pi)) {
		uint16 tx_cnt, tx_total, npt;

		tx_total = wlc_sslpnphy_total_tx_frames(pi);
		tx_cnt = tx_total - sslpnphy_specific->sslpnphy_tssi_tx_cnt;
		npt = sslpnphy_specific->sslpnphy_tssi_npt;

		if (tx_cnt > (1 << npt)) {
			/* Set new NPT */
			if (npt < SSLPNPHY_TX_PWR_CTRL_MAX_NPT) {
				npt++;
				wlc_sslpnphy_set_tx_pwr_npt(pi, npt);
				sslpnphy_specific->sslpnphy_tssi_npt = npt;
			}
			/* Update power index cache */
			sslpnphy_specific->sslpnphy_tssi_idx = wlc_sslpnphy_get_current_tx_pwr_idx(pi);
			sslpnphy_specific->sslpnphy_tssi_idx_ch[wlc_phy_channel2idx(CHSPEC_CHANNEL(pi->radio_chanspec))]
				= (uint8)sslpnphy_specific->sslpnphy_tssi_idx;

			/* Reset frame counter */
			sslpnphy_specific->sslpnphy_tssi_tx_cnt = tx_total;
		}
		WL_INFORM(("wl%d: %s: Index: %d\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, sslpnphy_specific->sslpnphy_tssi_idx));
	}
}

int32
wlc_sslpnphy_tssi2dbm(int32 tssi, int32 a1, int32 b0, int32 b1)
{
	int32 a, b, p;

	a = 32768 + (a1 * tssi);
	b = (512 * b0) + (32 * b1 * tssi);
	p = ((2 * b) + a) / (2 * a);

	if (p > 127)
		p = 127;
	else if (p < -128)
		p = -128;

	return p;
}

static void
wlc_sslpnphy_txpower_reset_npt(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	sslpnphy_specific->sslpnphy_tssi_tx_cnt = wlc_sslpnphy_total_tx_frames(pi);
	sslpnphy_specific->sslpnphy_tssi_npt = SSLPNPHY_TX_PWR_CTRL_START_NPT;

	if ((CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) &&
		(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
		sslpnphy_specific->sslpnphy_tssi_idx = 74;
	}
}

#if defined(PPR_API)

static void
wlc_sslpnphy_rate_table_build(phy_info_t *pi, uint32 *rate_table)
{
	ppr_dsss_rateset_t dsss_limits;
	ppr_ofdm_rateset_t ofdm_limits;
	ppr_ht_mcs_rateset_t mcs_limits;
	ppr_ht_mcs_rateset_t mcs40_limits;
	uint j, i;
	wl_tx_bw_t bw_mcs;

	if (pi->tx_power_offset == NULL)
		return;

	bw_mcs = CHSPEC_IS40(pi->radio_chanspec) ? WL_TX_BW_40 : WL_TX_BW_20;

	ppr_get_dsss(pi->tx_power_offset, WL_TX_BW_20, WL_TX_CHAINS_1,
	             &dsss_limits);

	if (CHSPEC_IS40(pi->radio_chanspec)) {
		ppr_get_ht_mcs(pi->tx_power_offset, bw_mcs, WL_TX_NSS_1,
		               WL_TX_MODE_NONE, WL_TX_CHAINS_1,
		               &mcs40_limits);
	} else {
		ppr_get_ofdm(pi->tx_power_offset, WL_TX_BW_20, WL_TX_MODE_NONE,
		             WL_TX_CHAINS_1, &ofdm_limits);
	}
	ppr_get_ht_mcs(pi->tx_power_offset, WL_TX_BW_20, WL_TX_NSS_1,
	               WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_limits);


	j = 0;
	for (i = 0; i < WL_RATESET_SZ_DSSS; i++, j++) {
		rate_table[j] = (uint32)((int32)(-dsss_limits.pwr[i]));
		PHY_TMP((" Rate %d, offset %d\n", j, rate_table[j]));
	}

	if (CHSPEC_IS40(pi->radio_chanspec)) {
		for (i = 0; i < WL_RATESET_SZ_HT_MCS; i++, j++) {
			rate_table[j] = (uint32)((int32)(-mcs40_limits.pwr[i]));
			PHY_TMP((" Rate %d, offset %d\n", j, rate_table[j]));
		}
	} else {
		for (i = 0; i < WL_RATESET_SZ_OFDM; i++, j++) {
			rate_table[j] = (uint32)((int32)(-ofdm_limits.pwr[i]));
			PHY_TMP((" Rate %d, offset %d\n", j, rate_table[j]));
		}
	}

	for (i = 0; i < WL_RATESET_SZ_HT_MCS; i++, j++) {
		rate_table[j] = (uint32)((int32)(-mcs_limits.pwr[i]));
		PHY_TMP((" Rate %d, offset %d\n", j, rate_table[j]));
	}
}

#endif /* PPR_API */

static void
wlc_sslpnphy_txpower_recalc_target_5356(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif
	phytbl_info_t tab;
	uint idx;
	bool reset_npt = FALSE;
	int8 mac_pwr;
	int8 mac_pwr_high_index = -12;
#ifdef PPR_API
	uint32 rate_table[WL_RATESET_SZ_DSSS + WL_RATESET_SZ_OFDM + WL_RATESET_SZ_HT_MCS];
#else
	uint i;
	uint32 rate_table[24] = {0};
#endif

	/* Preset txPwrCtrltbl */
	tab.tbl_id = SSLPNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;     /* 32 bit wide  */
#ifdef PPR_API
	wlc_sslpnphy_rate_table_build(pi, rate_table);
#else

	/* Adjust rate based power offset */
	for (i = 0, idx = 0; i < 4; i++) {
		rate_table[idx] =  (uint32)((int32)(-pi->tx_power_offset[i]));
		idx = idx + 1;
		WL_NONE((" Rate %d, offset %d\n", i, rate_table[i]));
	}
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i++]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i++]));

	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i++]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i++]));

	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i++]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i++]));

	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i++]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i]));
	rate_table[idx++] =  (uint32)((int32)(-pi->tx_power_offset[i+8]));

#endif /* PPR_API */

	tab.tbl_len = ARRAYSIZE(rate_table); /* # values   */
	tab.tbl_ptr = rate_table; /* ptr to buf */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_RATE_OFFSET;
	wlc_sslpnphy_write_table(pi, &tab);

	tab.tbl_ptr = &mac_pwr; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_width = 8;      /* 32 bit wide  */
	tab.tbl_offset = 128;
	mac_pwr = 0;

	for (idx = 0; idx < 64; idx++) {
		if (idx <= 31) {
			wlc_sslpnphy_write_table(pi,  &tab);
			tab.tbl_offset++;
			mac_pwr++;
		} else {
			tab.tbl_ptr = &mac_pwr_high_index; /* ptr to buf */
			wlc_sslpnphy_write_table(pi,  &tab);
			tab.tbl_offset++;
			mac_pwr_high_index++;
		}
	}
	/* Set new target power */
#ifdef PPR_API
	if (wlc_sslpnphy_get_target_tx_pwr(pi) != wlc_phy_txpower_get_target_min((wlc_phy_t*)pi)) {
		wlc_sslpnphy_set_target_tx_pwr(pi, wlc_phy_txpower_get_target_min((wlc_phy_t*)pi));
#else
	if (wlc_sslpnphy_get_target_tx_pwr(pi) != pi->tx_power_min) {
		wlc_sslpnphy_set_target_tx_pwr(pi, pi->tx_power_min);
#endif
		/* Should reset power index cache */
		reset_npt = TRUE;
	}

	/* Invalidate power index and NPT if new power targets */
	if (reset_npt || sslpnphy_specific->sslpnphy_papd_cal_done)
		wlc_sslpnphy_txpower_reset_npt(pi);

}
void
wlc_sslpnphy_txpower_recalc_target(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif
	uint8 plcp_pwr_offset_order[] = {3, 1, 2, 0, 7, 11, 6, 10, 5, 9, 4, 8, 12, 13, 14, 15, 16, 17, 18, 19};
	uint idx;
	int8 mac_pwr;
	uint16 sslpnphy_shm_ptr;
	int target_pwr;
#ifdef PPR_API
	uint32 rate_table[WL_RATESET_SZ_DSSS + WL_RATESET_SZ_OFDM + WL_RATESET_SZ_HT_MCS];
	ppr_ht_mcs_rateset_t ppr_mcs;
#else
	uint32 rate_table[24] = {0};
	uint offset, rate;
#endif

	bool ninja_board_flag = (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDELNA6L_SSID);
	bool sdna_board_flag =
	  (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);

	/* The processing for Ninja boards (5GHz band) is different since we are using positive
	 * Tx power offsets for the rate based tx pwrctrl.
	 */

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		wlc_sslpnphy_txpower_recalc_target_5356(pi);
		return;
	}

	if ((ninja_board_flag || sdna_board_flag) && (CHSPEC_IS5G(pi->radio_chanspec))) {
#if defined(PPR_API)
		wlc_sslpnphy_rate_table_build(pi, rate_table);
#else


		/* Rate Table part of the Tx pwrctrl table has the following entries:
		 * 0  -> CCK
		 * 4  -> BPSK  R=1/2
		 * 5  -> BPSK  R=2/3
		 * 6  -> BPSK  R=3/4
		 * 7  -> BPSK  R=5/6
		 * 8  -> QPSK  R=1/2
		 * 9  -> QPSK  R=2/3
		 * 10 -> QPSK  R=3/4
		 * 11 -> QPSK  R=5/6
		 * 12 -> QAM16 R=1/2
		 * 13 -> QAM16 R=2/3
		 * 14 -> QAM16 R=3/4
		 * 15 -> QAM16 R=5/6
		 * 16 -> QAM64 R=1/2
		 * 17 -> QAM64 R=2/3
		 * 18 -> QAM64 R=3/4
		 * 19 -> QAM64 R=5/6
		 */
		for (rate = 0; rate < ARRAYSIZE(rate_table); rate++) {
			rate_table[rate] = 0;
		}

		offset = (IS40MHZ(pi))? TXP_FIRST_MCS_40 : TXP_FIRST_MCS_20;
		/* MCS 0 */
		rate_table[4] = pi->tx_power_offset[offset]; /* BPSK R=1/2 */
		rate_table[5] = pi->tx_power_offset[offset]; /* BPSK R=2/3 */
		rate_table[6] = pi->tx_power_offset[offset]; /* BPSK R=3/4 */
		rate_table[7] = pi->tx_power_offset[offset]; /* BPSK R=5/6 */

		/* MCS 1 */
		rate_table[8] = pi->tx_power_offset[offset+1]; /* QPSK R=1/2 */
		rate_table[9] = pi->tx_power_offset[offset+1]; /* QPSK R=2/3 */

		/* MCS 2 */
		rate_table[10] = pi->tx_power_offset[offset+2]; /* QPSK R=3/4 */
		rate_table[11] = pi->tx_power_offset[offset+2]; /* QPSK R=5/6 */

		/* MCS 3 */
		rate_table[12] = pi->tx_power_offset[offset+3]; /* QAM16 R=1/2 */
		rate_table[13] = pi->tx_power_offset[offset+3]; /* QAM16 R=2/3 */

		/* MCS 4 */
		rate_table[14] = pi->tx_power_offset[offset+4]; /* QAM16 R=3/4 */
		rate_table[15] = pi->tx_power_offset[offset+4]; /* QAM16 R=5/6 */

		/* MCS 5 */
		rate_table[16] = pi->tx_power_offset[offset+5]; /* QAM64 R=1/2 */
		rate_table[17] = pi->tx_power_offset[offset+5]; /* QAM64 R=2/3 */

		/* MCS 6 */
		rate_table[18] = pi->tx_power_offset[offset+6]; /* QAM64 R=3/4 */

		/* MCS 7 */
		rate_table[19] = pi->tx_power_offset[offset+7]; /* QAM64 R=5/6 */

		/*
		for (rate = 0; rate < ARRAYSIZE(rate_table); rate++) {
			printf("rate_table index=%d, offset=%d\n", rate, rate_table[rate]);
		}
		*/

#endif /* PPR_API */
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, rate_table,
		    ARRAYSIZE(rate_table), 32, SSLPNPHY_TX_PWR_CTRL_RATE_OFFSET);
#ifdef PPR_API
		wlc_sslpnphy_set_target_tx_pwr(pi, wlc_phy_txpower_get_target_max((wlc_phy_t*)pi));
#else
		wlc_sslpnphy_set_target_tx_pwr(pi, pi->tx_power_max);
#endif
		/* Ninja board processing completes here */
		return;
	}

	sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
#ifdef PPR_API
	target_pwr = wlc_sslpnphy_validated_tssi_pwr(pi, wlc_phy_txpower_get_target_min((wlc_phy_t*)pi));
#else
	target_pwr = wlc_sslpnphy_validated_tssi_pwr(pi, pi->tx_power_min);
#endif
	ASSERT(0 != sslpnphy_shm_ptr);

	/* Fill MAC offset table to support offsets from -8dBm to 7.75dBm relative to the target power */
	for (idx = 1, mac_pwr = 32; idx < 64; idx++) {
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&mac_pwr, 1, 8, SSLPNPHY_TX_PWR_CTRL_MAC_OFFSET + idx);
		mac_pwr--;
	}

	/* Initializing ant1 max pwr to tssi max pwr limit */
	sslpnphy_specific->sslpnphy_ant1_max_pwr = sslpnphy_specific->sslpnphy_tssi_max_pwr_limit;


	/* Calculate offset for each rate relative to the target power */
#ifdef PPR_API
	ppr_get_ht_mcs(pi->tx_power_offset, WL_TX_BW_20, WL_TX_NSS_1,
	               WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ppr_mcs);

	for (idx = 0; idx < WL_RATESET_SZ_HT_MCS; idx++) {
#else
	for (idx = 0; idx <= TXP_LAST_MCS_SISO_20; idx++) {
#endif
		uint16 ant0_offset, ant1_offset;
		int ant0_pwr, ant1_pwr;

		/* Calculate separate offsets for ant0 and ant1 as they might be different */
#ifdef PPR_API
		ant0_pwr = (int)wlc_phy_txpower_get_target_min((wlc_phy_t*)pi) +
		        (int)(-(int32)ppr_mcs.pwr[idx]);
#else
		ant0_pwr = (int)pi->tx_power_min + (int)pi->tx_power_offset[idx];
#endif
		ant0_pwr = wlc_sslpnphy_validated_tssi_pwr(pi, ant0_pwr);
		ant0_offset = (uint16)MAX(1, MIN(63, (33 + (ant0_pwr - target_pwr))));

		ant1_pwr = MIN(ant0_pwr, (int)sslpnphy_specific->sslpnphy_ant1_max_pwr);
		ant1_pwr = wlc_sslpnphy_validated_tssi_pwr(pi, ant1_pwr);
		ant1_offset = (uint16)MAX(1, MIN(63, (33 + (ant1_pwr - target_pwr))));

		/* In shm upper byte is offset for ant1 and lower byte is offset for ant0 */
		WL_WRITE_SHM(pi,
			2 * (sslpnphy_shm_ptr + M_SSLPNPHY_TXPWR_BLK + plcp_pwr_offset_order[idx]),
			(ant1_offset << 8) | ant0_offset);
	}


	/* Set new target power */
	wlc_sslpnphy_set_target_tx_pwr(pi, target_pwr);
}

void
wlc_sslpnphy_set_tx_pwr_ctrl(phy_info_t *pi, uint16 mode)
{
	uint16 old_mode = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
#ifdef ROMTERMPHY
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif

	ASSERT(
		(SSLPNPHY_TX_PWR_CTRL_OFF == mode) ||
		(SSLPNPHY_TX_PWR_CTRL_SW == mode) ||
		(SSLPNPHY_TX_PWR_CTRL_HW == mode));
#ifdef ROMTERMPHY
	/* idelTssi estimation needs the txFrontEndClkEn ON */
	/* When TxPwrCtrl is OFF/SW switches OFF txFrontEndClkEn */
	/* So switch OFF the uCode based idelTssi estimation when txPwrCtrl state is SW/OFF */
	/* Doing idelTssi estimation with txFrontEndClkEn OFF will result in a PSM_WD */
	if (sdna_board_flag) {
		if (!(SSLPNPHY_TX_PWR_CTRL_HW == mode)) {
			wlc_sslpnphy_write_shm_tssiCalEn(pi, 0);
		}
	}
#endif
	/* Setting txfront end clock also along with hwpwr control */
	PHY_REG_MOD(pi, SSLPNPHY, sslpnCalibClkEnCtrl, txFrontEndCalibClkEn,
		(SSLPNPHY_TX_PWR_CTRL_HW == mode) ? 1 : 0);

	/* Feed back RF power level to PAPD block */
	PHY_REG_MOD(pi, SSLPNPHY, papd_control2, papd_analog_gain_ovr,
		(SSLPNPHY_TX_PWR_CTRL_HW == mode) ? 0 : 1);

	if (old_mode != mode) {
		if (SSLPNPHY_TX_PWR_CTRL_HW == old_mode) {
			/* Clear out all power offsets */
			wlc_sslpnphy_clear_tx_power_offsets(pi);
		} else if (SSLPNPHY_TX_PWR_CTRL_HW == mode) {
			/* Recalculate target power to restore power offsets */
			wlc_sslpnphy_txpower_recalc_target(pi);

			/* Set starting index to the best known value for that target */
			wlc_sslpnphy_set_start_tx_pwr_idx(pi,
				sslpnphy_specific->sslpnphy_tssi_idx_ch
			          [wlc_phy_channel2idx(CHSPEC_CHANNEL(pi->radio_chanspec))]);
			/* Reset NPT */
			wlc_sslpnphy_txpower_reset_npt(pi);
			wlc_sslpnphy_set_tx_pwr_npt(pi, sslpnphy_specific->sslpnphy_tssi_npt);

			/* Disable any gain overrides */
			wlc_sslpnphy_disable_tx_gain_override(pi);
			sslpnphy_specific->sslpnphy_tx_power_idx_override = -1;
		}

		/* Set requested tx power control mode */
		PHY_REG_MOD_RAW(pi, SSLPNPHY_TxPwrCtrlCmd,
			SSLPNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			SSLPNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
			SSLPNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK,
			mode);

		WL_INFORM(("wl%d: %s: %s \n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
			mode ? ((SSLPNPHY_TX_PWR_CTRL_HW == mode) ? "Auto" : "Manual") : "Off"));
	}
}


STATIC void
wlc_sslpnphy_idle_tssi_est(phy_info_t *pi)
{
	uint16 status, tssi_val;
	wl_pkteng_t pkteng;
	struct ether_addr sa;
	wlc_phy_t *ppi = (wlc_phy_t *)pi;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
#ifdef ROMTERMPHY
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif

	sa.octet[0] = 10;
	sslpnphy_specific->sslpnphy_tssi_val = 0;

	WL_ERROR(("Pkteng TX Start Called\n"));
	pkteng.flags = WL_PKTENG_PER_TX_START;
	if (sslpnphy_specific->sslpnphy_recal)
		pkteng.delay = 2;              /* Inter packet delay */
	else
		pkteng.delay = 50;              /* Inter packet delay */
	pkteng.nframes = 50;            /* number of frames */
	pkteng.length = 0;              /* packet length */
	pkteng.seqno = FALSE;                   /* enable/disable sequence no. */
	wlc_sslpnphy_pktengtx(ppi, &pkteng, 108, &sa, (1000*10));

	status = phy_reg_read(pi, SSLPNPHY_TxPwrCtrlStatus);
	if (status & SSLPNPHY_TxPwrCtrlStatus_estPwrValid_MASK) {
		tssi_val = (status & SSLPNPHY_TxPwrCtrlStatus_estPwr_MASK) >>
			SSLPNPHY_TxPwrCtrlStatus_estPwr_SHIFT;
		WL_INFORM(("wl%d: %s: Measured idle TSSI: %d\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, (int8)tssi_val - 32));
		sslpnphy_specific->sslpnphy_tssi_val = (uint8)tssi_val;
	} else {
		WL_INFORM(("wl%d: %s: Failed to measure idle TSSI\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
		}
#ifdef ROMTERMPHY
	if (sdna_board_flag) {
		wlc_sslpnphy_write_shm_tssiCalEn(pi, 0);
		wlc_sslpnphy_auxadc_measure((wlc_phy_t *) pi, 0);
	} else {
#else
	{
#endif
		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
			M_SSLPNPHY_TSSICAL_EN)), 0x0);
		wlc_sslpnphy_auxadc_measure((wlc_phy_t *) pi, 0);
	}
}

void
wlc_sslpnphy_recalc_tssi2dbm_tbl(phy_info_t *pi, int32 a1, int32 b0, int32 b1)
{
	int32 tssi, pwr;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	/* Convert tssi to power LUT */
	for (tssi = 0; tssi < 64; tssi++) {
		pwr = wlc_sslpnphy_tssi2dbm(tssi, a1, b0, b1);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&pwr, 1, 32, tssi);
	}

	/* For max power limit we need to account for idle tssi */
	sslpnphy_specific->sslpnphy_tssi_max_pwr_limit =
		(int8)wlc_sslpnphy_tssi2dbm((64 - sslpnphy_specific->sslpnphy_tssi_val), a1, b0, b1) - 1;
	sslpnphy_specific->sslpnphy_tssi_min_pwr_limit =
		(int8)MIN((8 * 4), (wlc_sslpnphy_tssi2dbm(60, a1, b0, b1) + 1));

	/* Validate against NVRAM limits */
	sslpnphy_specific->sslpnphy_tssi_max_pwr_limit =
		MIN(sslpnphy_specific->sslpnphy_tssi_max_pwr_limit, sslpnphy_specific->sslpnphy_tssi_max_pwr_nvram);
	sslpnphy_specific->sslpnphy_tssi_min_pwr_limit =
		MAX(sslpnphy_specific->sslpnphy_tssi_min_pwr_limit, sslpnphy_specific->sslpnphy_tssi_min_pwr_nvram);


	/* Final sanity check */
	ASSERT(sslpnphy_specific->sslpnphy_tssi_max_pwr_limit > sslpnphy_specific->sslpnphy_tssi_min_pwr_limit);
}

void
wlc_sslpnphy_tx_pwr_ctrl_init(phy_info_t *pi)
{
	sslpnphy_txgains_t tx_gains;
	sslpnphy_txgains_t ltx_gains;
	uint8 bbmult;
	int32 a1, b0, b1;
	uint freq;
	bool suspend;
	uint32 ind;
	uint16 tssi_val = 0;
	uint8 phybw40 = IS40MHZ(pi);
	uint16 tempsense;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	/* Saving tempsense value before starting idle tssi est */
	/* Reset Tempsese to 0 . Idle tssi est expects it to be 0 */
	tempsense = phy_reg_read(pi, SSLPNPHY_TempSenseCorrection);
	PHY_REG_WRITE(pi, SSLPNPHY, TempSenseCorrection, 0);

	suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		WL_SUSPEND_MAC_AND_WAIT(pi);
#ifdef PS4319XTRA
	if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)
		WL_WRITE_SHM(pi, M_PS4319XTRA, 0);
#endif /* PS4319XTRA */
	a1 = b0 = b1 = 0;
	if (NORADIO_ENAB(pi->pubpi)) {
		wlc_sslpnphy_set_bbmult(pi, 0x30);
		return;
	}

	freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
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
		wlc_sslpnphy_set_tx_gain(pi, &tx_gains);
		wlc_sslpnphy_set_bbmult(pi, bbmult);
	} else {
		/* Adjust power LUT's */
		if (sslpnphy_specific->sslpnphy_target_tx_freq != (uint16)freq) {
			if (freq < 2500) {
			/* 2.4 GHz */
					b0 = pi->txpa_2g_low_temp[0];
					b1 = pi->txpa_2g_low_temp[1];
					a1 = pi->txpa_2g_low_temp[2];

					PHY_REG_WRITE(pi, SSLPNPHY, AfeRSSICtrl1,
						((uint16)sslpnphy_specific->sslpnphy_rssi_vf_lowtemp << 0) |
						((uint16)sslpnphy_specific->sslpnphy_rssi_vc_lowtemp << 4) |
						(0x00 << 8) |
						((uint16)sslpnphy_specific->sslpnphy_rssi_gs_lowtemp << 10) |
						(0x01 << 13));
			}
#ifdef BAND5G
			if (CHSPEC_IS5G(pi->radio_chanspec)) {
				if (freq <= 5320) {
					/* 5 GHz low */
					b0 = pi->txpa_5g_low[0];
					b1 = pi->txpa_5g_low[1];
					a1 = pi->txpa_5g_low[2];
				} else if (freq <= 5700) {
					/* 5 GHz medium */
					b0 = pi->txpa_5g_mid[0];
					b1 = pi->txpa_5g_mid[1];
					a1 = pi->txpa_5g_mid[2];
				} else {
					/* 5 GHz high */
					b0 = pi->txpa_5g_hi[0];
					b1 = pi->txpa_5g_hi[1];
					a1 = pi->txpa_5g_hi[2];
				}
				PHY_REG_WRITE(pi, SSLPNPHY, AfeRSSICtrl1,
					((uint16)sslpnphy_specific->sslpnphy_rssi_5g_vf << 0) |
					((uint16)sslpnphy_specific->sslpnphy_rssi_5g_vc << 4) |
					(0x00 << 8) |
					((uint16)sslpnphy_specific->sslpnphy_rssi_5g_gs << 10) |
					(0x01 << 13));
			}
#endif /* BAND5G */
			/* Save new target frequency */
			sslpnphy_specific->sslpnphy_target_tx_freq = (uint16)freq;
			sslpnphy_specific->sslpnphy_last_tx_freq = (uint16)freq;
		}
		/* Clear out all power offsets */
		wlc_sslpnphy_clear_tx_power_offsets(pi);

		/* Setup estPwrLuts for measuring idle TSSI */
		for (ind = 0; ind < 64; ind++) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
				&ind, 1, 32, ind);
		}

		PHY_REG_LIST_START
			PHY_REG_MOD3_ENTRY(SSLPNPHY, TxPwrCtrlNnum,
				Ntssi_delay, Ntssi_intg_log2, Npt_intg_log2, 255, 5, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, TxPwrCtrlIdleTssi, idleTssi0, 0x1f)
		PHY_REG_LIST_EXECUTE(pi);

		{
			uint8 iqcal_ctrl2;

			PHY_REG_MOD3(pi, SSLPNPHY, auxadcCtrl, rssifiltEn, rssiformatConvEn, txpwrctrlEn, 0, 1, 1);

			/* Set IQCAL mux to TSSI */
			iqcal_ctrl2 = (uint8)read_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2);
			iqcal_ctrl2 &= (uint8)~(0x0c);
			iqcal_ctrl2 |= 0x01;
			write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, iqcal_ctrl2);

			/* Use PA output for TSSI */
			if ((CHSPEC_IS5G(pi->radio_chanspec) ||
				CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) &&
				(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
				mod_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, 0xf, 0x8);
				wlc_sslpnphy_set_tssi_mux(pi, SSLPNPHY_TSSI_EXT);
			} else {
				/* Use PA output for TSSI */
				wlc_sslpnphy_set_tssi_mux(pi, SSLPNPHY_TSSI_POST_PA);
			}

		}
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, TxPwrCtrlIdleTssi, rawTssiOffsetBinFormat, 1)
			/* Synch up with tcl */
			PHY_REG_MOD_ENTRY(SSLPNPHY, crsgainCtrl, crseddisable, 1)
			/* CCK calculation offset */
			PHY_REG_MOD_ENTRY(SSLPNPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 0)
		PHY_REG_LIST_EXECUTE(pi);

		/* Set starting index & NPT to 0 for idle TSSI measurments */
		wlc_sslpnphy_set_start_tx_pwr_idx(pi, 0);
		wlc_sslpnphy_set_tx_pwr_npt(pi, 0);

		/* Force manual power control */
		PHY_REG_MOD_RAW(pi, SSLPNPHY_TxPwrCtrlCmd,
			(SSLPNPHY_TxPwrCtrlCmd_txPwrCtrl_en_MASK |
			SSLPNPHY_TxPwrCtrlCmd_hwtxPwrCtrl_en_MASK |
			SSLPNPHY_TxPwrCtrlCmd_use_txPwrCtrlCoefs_MASK),
			SSLPNPHY_TX_PWR_CTRL_SW);

		{
			/* Force WLAN antenna */
			wlc_sslpnphy_btcx_override_enable(pi);
			wlc_sslpnphy_set_tx_gain_override(pi, TRUE);
		}
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, internalrftxpu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, internalrftxpu_ovr_val, 0)
		PHY_REG_LIST_EXECUTE(pi);


	if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			/* save */
			wlc_sslpnphy_get_tx_gain(pi, &ltx_gains);

			/* Idle tssi WAR */
			tx_gains.gm_gain = 0;
			tx_gains.pga_gain = 0;
			tx_gains.pad_gain = 0;
			tx_gains.dac_gain = 0;

			wlc_sslpnphy_set_tx_gain(pi, &tx_gains);
		}
	}

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
			b0 = pi->txpa_2g[0];
			b1 = pi->txpa_2g[1];
			a1 = pi->txpa_2g[2];
			if (!(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID)) {
			if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
				/* save */
				wlc_sslpnphy_get_tx_gain(pi, &ltx_gains);

				/* Idle tssi WAR */
				tx_gains.gm_gain = 0;
				tx_gains.pga_gain = 0;
				tx_gains.pad_gain = 0;
				tx_gains.dac_gain = 0;

				wlc_sslpnphy_set_tx_gain(pi, &tx_gains);
			}
			}
			PHY_REG_WRITE(pi, SSLPNPHY, AfeRSSICtrl1,
				((uint16)sslpnphy_specific->sslpnphy_rssi_vf << 0) |
				((uint16)sslpnphy_specific->sslpnphy_rssi_vc << 4) |
				(0x00 << 8) |
				((uint16)sslpnphy_specific->sslpnphy_rssi_gs << 10) |
				(0x01 << 13));
			wlc_sslpnphy_idle_tssi_est(pi);

			while ((sslpnphy_specific->sslpnphy_tssi_val > 60) &&
			       (sslpnphy_specific->sslpnphy_rssi_vc >= 6)) {
				sslpnphy_specific->sslpnphy_rssi_vc =
				        sslpnphy_specific->sslpnphy_rssi_vc - 1;
				PHY_REG_WRITE(pi, SSLPNPHY, AfeRSSICtrl1,
					((uint16)sslpnphy_specific->sslpnphy_rssi_vf << 0) |
					((uint16)sslpnphy_specific->sslpnphy_rssi_vc << 4) |
					(0x00 << 8) |
					((uint16)sslpnphy_specific->sslpnphy_rssi_gs << 10) |
					(0x01 << 13));
				wlc_sslpnphy_idle_tssi_est(pi);
			}
	} else if (CHSPEC_IS5G(pi->radio_chanspec)) {
		wlc_sslpnphy_idle_tssi_est(pi);
	}
		tssi_val = sslpnphy_specific->sslpnphy_tssi_val;
		tssi_val -= 32;
		/* Write measured idle TSSI value */
		PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlIdleTssi, idleTssi0, tssi_val);

		PHY_REG_LIST_START
			/* Sych up with tcl */
			PHY_REG_MOD_ENTRY(SSLPNPHY, crsgainCtrl, crseddisable, 0)
			/* Clear tx PU override */
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, internalrftxpu_ovr, 0)
		PHY_REG_LIST_EXECUTE(pi);

		/* Invalidate target frequency */
		sslpnphy_specific->sslpnphy_target_tx_freq = 0;

		/* CCK calculation offset */
		if (IS_OLYMPIC(pi)) {
			/* TSMC requires 0.5 dB lower power due to its RF tuning */
			if ((sslpnphy_specific->sslpnphy_fabid == 2) ||
			    (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12))
				PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 7);
			else
				PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 6);
		}
		else if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
			if (phybw40)
				PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 2);
			else
				PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 1);
		}

		else
			PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 3);

		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDBREF_SSID ||
			BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDHMB_SSID ||
			BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319USBSDB_SSID) {
				PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlDeltaPwrLimit, cckPwrOffset, 5);
		}


		/* Restore back Tempsense */
		PHY_REG_WRITE(pi, SSLPNPHY, TempSenseCorrection, tempsense);

		/* Program TSSI lookup table */
		wlc_sslpnphy_recalc_tssi2dbm_tbl(pi, a1, b0, b1);

		/* Initialize default NPT */
		wlc_sslpnphy_txpower_reset_npt(pi);

		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			wlc_sslpnphy_set_tx_gain(pi, &ltx_gains);
		}
		/* Enable hardware power control */
		wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_HW);
	}

	if (!suspend)
		WL_ENABLE_MAC(pi);
#ifdef PS4319XTRA
	if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)
		WL_WRITE_SHM(pi, M_PS4319XTRA, PS4319XTRA);
#endif /* PS4319XTRA */

#if !defined(ROMTERMPHY)
	if ((CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) &&
		(GENERIC_PHY_INFO(pi)->chiprev == 0)) {
		/* tssi does not work on 5356a0; hard code tx power */
		wlc_sslpnphy_set_tx_pwr_by_index(pi, 50);

		if (phybw40) {
			phytbl_info_t tab;
			tab.tbl_ptr = fltr_ctrl_tbl_40Mhz;
			tab.tbl_len = 10;
			tab.tbl_id = 0xb;
			tab.tbl_offset = 0;
			tab.tbl_width = 32;
			wlc_sslpnphy_write_table(pi, &tab);
		}
	}
#endif /* ROMTERMPHY */
}


STATIC uint8
BCMROMFN(wlc_sslpnphy_get_bbmult)(phy_info_t *pi)
{
	uint16 m0m1;
	phytbl_info_t tab;

	tab.tbl_ptr = &m0m1; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = SSLPNPHY_TBL_ID_IQLOCAL;         /* iqloCaltbl      */
	tab.tbl_offset = 87; /* tbl offset */
	tab.tbl_width = 16;     /* 16 bit wide */
	wlc_sslpnphy_read_table(pi, &tab);

	return (uint8)((m0m1 & 0xff00) >> 8);
}

STATIC void
BCMROMFN(wlc_sslpnphy_set_pa_gain)(phy_info_t *pi, uint16 gain)
{
	PHY_REG_MOD(pi, SSLPNPHY, txgainctrlovrval1, pagain_ovr_val1, gain);
	PHY_REG_MOD(pi, SSLPNPHY, stxtxgainctrlovrval1, pagain_ovr_val1, gain);

}
STATIC uint16
sslpnphy_iqlocc_write(phy_info_t *pi, uint8 data)
{
	int32 data32 = (int8)data;
	int32 rf_data32;
	int32 ip, in;
	ip = 8 + (data32 >> 1);
	in = 8 - ((data32+1) >> 1);
	rf_data32 = (in << 4) | ip;
	return (uint16)(rf_data32);
}
STATIC void
wlc_sslpnphy_set_radio_loft(phy_info_t *pi,
	uint8 ei0,
	uint8 eq0,
	uint8 fi0,
	uint8 fq0)
{
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I, sslpnphy_iqlocc_write(pi, ei0));
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q, sslpnphy_iqlocc_write(pi, eq0));
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I, sslpnphy_iqlocc_write(pi, fi0));
	write_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q, sslpnphy_iqlocc_write(pi, fq0));

}
void
BCMROMFN(wlc_sslpnphy_get_radio_loft) (phy_info_t *pi,
	uint8 *ei0,
	uint8 *eq0,
	uint8 *fi0,
	uint8 *fq0)
{
	*ei0 = LPPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_I));
	*eq0 = LPPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_BB_Q));
	*fi0 = LPPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_I));
	*fq0 = LPPHY_IQLOCC_READ(
		read_radio_reg(pi, RADIO_2063_TXRF_IDAC_LO_RF_Q));
}

STATIC void
BCMROMFN(wlc_sslpnphy_get_tx_gain)(phy_info_t *pi,  sslpnphy_txgains_t *gains)
{
	uint16 dac_gain;

	dac_gain = PHY_REG_READ(pi, SSLPNPHY, AfeDACCtrl, dac_ctrl);
	gains->dac_gain = (dac_gain & 0x380) >> 7;

	{
		uint16 rfgain0, rfgain1;

		rfgain0 = PHY_REG_READ(pi, SSLPNPHY, txgainctrlovrval0, txgainctrl_ovr_val0);
		rfgain1 = PHY_REG_READ(pi, SSLPNPHY, txgainctrlovrval1, txgainctrl_ovr_val1);

		gains->gm_gain = rfgain0 & 0xff;
		gains->pga_gain = (rfgain0 >> 8) & 0xff;
		gains->pad_gain = rfgain1 & 0xff;
	}
}

void
wlc_sslpnphy_set_tx_iqcc(phy_info_t *pi, uint16 a, uint16 b)
{
	uint16 iqcc[2];

	/* Fill buffer with coeffs */
	iqcc[0] = a;
	iqcc[1] = b;

	/* Update iqloCaltbl */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		iqcc, 2, 16, 80);
}

#ifdef WLSINGLE_ANT

#define wlc_phy_get_txant 0
#define wlc_sslpnphy_set_ant_override(pi, ant) 0
#define wlc_sslpnphy_restore_ant_override(pi, ant_ovr) do {} while (0)

#else
static  uint16
wlc_phy_get_txant(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	return (wlapi_bmac_get_txant(pi->sh->physhim) == ANT_TX_FORCE_1) ? 1: 0;
#else
	wlc_info_t *wlc = (wlc_info_t *)pi->wlc;

	return ((wlc->txant == ANT_TX_FORCE_1) ? 1 : 0);
#endif /* PHYHAL */
}

static uint32
wlc_sslpnphy_set_ant_override(phy_info_t *pi, uint16 ant)
{
	uint16 val, ovr;
	uint32 ret;

	ASSERT(ant < 2);

	/* Save original values */
	val = phy_reg_read(pi, SSLPNPHY_RFOverrideVal0);
	ovr = phy_reg_read(pi, SSLPNPHY_RFOverride0);
	ret = ((uint32)ovr << 16) | val;

	/* Write new values */
	val &= ~SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_MASK;
	val |= (ant << SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_SHIFT);
	ovr |= SSLPNPHY_RFOverride0_ant_selp_ovr_MASK;
	PHY_REG_WRITE(pi, SSLPNPHY, RFOverrideVal0, val);
	PHY_REG_WRITE(pi, SSLPNPHY, RFOverride0, ovr);

	return ret;
}

static void
wlc_sslpnphy_restore_ant_override(phy_info_t *pi, uint32 ant_ovr)
{
	uint16 ovr, val;

	ovr = (uint16)(ant_ovr >> 16);
	val = (uint16)(ant_ovr & 0xFFFF);

	phy_reg_mod(pi,
		SSLPNPHY_RFOverrideVal0,
		SSLPNPHY_RFOverrideVal0_ant_selp_ovr_val_MASK,
		val);
	phy_reg_mod(pi,
		SSLPNPHY_RFOverride0,
		SSLPNPHY_RFOverride0_ant_selp_ovr_MASK,
		ovr);
}

#endif /* WLSINGLE_ANT */

void
BCMROMFN(wlc_sslpnphy_set_tx_locc)(phy_info_t *pi, uint16 didq)
{
	phytbl_info_t tab;

	/* Update iqloCaltbl */
	tab.tbl_id = SSLPNPHY_TBL_ID_IQLOCAL;			/* iqloCaltbl	*/
	tab.tbl_width = 16;	/* 16 bit wide	*/
	tab.tbl_ptr = &didq;
	tab.tbl_len = 1;
	tab.tbl_offset = 85;
	wlc_sslpnphy_write_table(pi, &tab);
}

/* only disable function exists */
STATIC void
wlc_sslpnphy_disable_pad(phy_info_t *pi)
{
	sslpnphy_txgains_t current_gain;

	wlc_sslpnphy_get_tx_gain(pi, &current_gain);
	current_gain.pad_gain = 0;
	wlc_sslpnphy_set_tx_gain(pi, &current_gain);
}

void
BCMROMFN(wlc_sslpnphy_set_tx_pwr_by_index)(phy_info_t *pi, int indx)
{
	phytbl_info_t tab;
	uint16 a, b;
	uint8 bb_mult;
	uint32 bbmultiqcomp, txgain, locoeffs, rfpower;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

	ASSERT(indx <= SSLPNPHY_MAX_TX_POWER_INDEX);

	/* Save forced index */
	sslpnphy_specific->sslpnphy_tx_power_idx_override = (int8)indx;
	sslpnphy_specific->sslpnphy_current_index = (uint8)indx;


	/* Preset txPwrCtrltbl */
	tab.tbl_id = SSLPNPHY_TBL_ID_TXPWRCTL;
	tab.tbl_width = 32;	/* 32 bit wide	*/
	tab.tbl_len = 1;        /* # values   */

	/* Turn off automatic power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	/* Read index based bb_mult, a, b from the table */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + indx; /* iqCoefLuts */
	tab.tbl_ptr = &bbmultiqcomp; /* ptr to buf */
	wlc_sslpnphy_read_table(pi,  &tab);

	/* Read index based tx gain from the table */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_GAIN_OFFSET + indx; /* gainCtrlLuts */
	tab.tbl_ptr = &txgain; /* ptr to buf */
	wlc_sslpnphy_read_table(pi,  &tab);

	/* Apply tx gain */
	{
		sslpnphy_txgains_t gains;

		gains.gm_gain = (uint16)(txgain & 0xff);
		gains.pga_gain = (uint16)(txgain >> 8) & 0xff;
		gains.pad_gain = (uint16)(txgain >> 16) & 0xff;
		gains.dac_gain = (uint16)(bbmultiqcomp >> 28) & 0x07;

		wlc_sslpnphy_set_tx_gain(pi, &gains);
		wlc_sslpnphy_set_pa_gain(pi,  (uint16)(txgain >> 24) & 0x7f);
	}

	/* Apply bb_mult */
	bb_mult = (uint8)((bbmultiqcomp >> 20) & 0xff);
	wlc_sslpnphy_set_bbmult(pi, bb_mult);

	/* Apply iqcc */
	a = (uint16)((bbmultiqcomp >> 10) & 0x3ff);
	b = (uint16)(bbmultiqcomp & 0x3ff);
	wlc_sslpnphy_set_tx_iqcc(pi, a, b);

	/* Read index based di & dq from the table */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_LO_OFFSET + indx; /* loftCoefLuts */
	tab.tbl_ptr = &locoeffs; /* ptr to buf */
	wlc_sslpnphy_read_table(pi,  &tab);

	/* Apply locc */
	wlc_sslpnphy_set_tx_locc(pi, (uint16)locoeffs);

	/* Apply PAPD rf power correction */
	tab.tbl_offset = SSLPNPHY_TX_PWR_CTRL_PWR_OFFSET + indx;
	tab.tbl_ptr = &rfpower; /* ptr to buf */
	wlc_sslpnphy_read_table(pi,  &tab);

	PHY_REG_MOD(pi, SSLPNPHY, papd_analog_gain_ovr_val, papd_analog_gain_ovr_val, rfpower * 8);
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		wlc_sslpnphy_set_pa_gain(pi, 116);
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)
			wlc_sslpnphy_set_pa_gain(pi, 0x10);
	}
#endif

	/* Enable gain overrides */
	wlc_sslpnphy_enable_tx_gain_override(pi);
}

STATIC void
BCMROMFN(wlc_sslpnphy_set_trsw_override)(phy_info_t *pi, bool tx, bool rx)
{
	/* Set TR switch */
	PHY_REG_MOD2(pi, SSLPNPHY, RFOverrideVal0,
		trsw_tx_pu_ovr_val, trsw_rx_pu_ovr_val, (tx ? 1 : 0), (rx ? 1 : 0));

	/* Enable overrides */
	PHY_REG_OR(pi, SSLPNPHY, RFOverride0,
		SSLPNPHY_RFOverride0_trsw_tx_pu_ovr_MASK | SSLPNPHY_RFOverride0_trsw_rx_pu_ovr_MASK);
}

STATIC void
BCMROMFN(wlc_sslpnphy_set_swctrl_override)(phy_info_t *pi, uint8 indx)
{
	phytbl_info_t tab;
	uint16 swctrl_val;

	if (indx == SWCTRL_OVR_DISABLE)
	{
		PHY_REG_WRITE(pi, SSLPNPHY, swctrlOvr, 0);
	} else {
		tab.tbl_id = SSLPNPHY_TBL_ID_SW_CTRL;
		tab.tbl_width = 16;	/* 16 bit wide	*/
		tab.tbl_ptr = &swctrl_val ; /* ptr to buf */
		tab.tbl_len = 1;        /* # values   */
		tab.tbl_offset = indx; /* tbl offset */
		wlc_sslpnphy_read_table(pi, &tab);

		PHY_REG_WRITE(pi, SSLPNPHY, swctrlOvr, 0xff);
		PHY_REG_MOD(pi, SSLPNPHY, swctrlOvr_val, swCtrl_p_ovr_val, (swctrl_val & 0xf));
		PHY_REG_MOD(pi, SSLPNPHY, swctrlOvr_val, swCtrl_n_ovr_val, ((swctrl_val >> 4) & 0xf));
	}
}

STATIC void
BCMROMFN(wlc_sslpnphy_set_rx_gain_by_distribution)(phy_info_t *pi,
	uint16 pga,
	uint16 biq2,
	uint16 pole1,
	uint16 biq1,
	uint16 tia,
	uint16 lna2,
	uint16 lna1)
{
	uint16 gain0_15, gain16_19;

	gain16_19 = pga & 0xf;
	gain0_15 = ((biq2 & 0x1) << 15) |
		((pole1 & 0x3) << 13) |
		((biq1 & 0x3) << 11) |
		((tia & 0x7) << 8) |
		((lna2 & 0x3) << 6) |
		((lna2 & 0x3) << 4) |
		((lna1 & 0x3) << 2) |
		((lna1 & 0x3) << 0);

	PHY_REG_MOD(pi, SSLPNPHY, rxgainctrl0ovrval, rxgainctrl_ovr_val0, gain0_15);
	PHY_REG_MOD(pi, SSLPNPHY, rxlnaandgainctrl1ovrval, rxgainctrl_ovr_val1, gain16_19);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		PHY_REG_MOD(pi, SSLPNPHY, rfoverride2val, slna_gain_ctrl_ovr_val, lna1);
		PHY_REG_MOD(pi, SSLPNPHY, RFinputOverrideVal, wlslnagainctrl_ovr_val, lna1);
	}
}

void
BCMROMFN(wlc_sslpnphy_rx_gain_override_enable)(phy_info_t *pi, bool enable)
{
	uint16 ebit = enable ? 1 : 0;
	PHY_REG_MOD3(pi, SSLPNPHY, rfoverride2,
		rxgainctrl_ovr, gmode_ext_lna_gain_ovr, amode_ext_lna_gain_ovr, ebit, ebit, ebit);
	PHY_REG_MOD3(pi, SSLPNPHY, RFOverride0,
		trsw_rx_pu_ovr, gmode_rx_pu_ovr, amode_rx_pu_ovr, ebit, ebit, ebit);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		PHY_REG_MOD(pi, SSLPNPHY, rfoverride2, slna_gain_ctrl_ovr, ebit);
		PHY_REG_MOD(pi, SSLPNPHY, RFinputOverride, wlslnagainctrl_ovr, ebit);
	}
}

STATIC void
BCMROMFN(wlc_sslpnphy_rx_pu)(phy_info_t *pi, bool bEnable)
{
	if (!bEnable) {
		PHY_REG_LIST_START
			PHY_REG_AND_ENTRY(SSLPNPHY, RFOverride0,
				~(uint16)(SSLPNPHY_RFOverride0_internalrfrxpu_ovr_MASK))
			PHY_REG_AND_ENTRY(SSLPNPHY, rfoverride2,
				~(uint16)(SSLPNPHY_rfoverride2_rxgainctrl_ovr_MASK))
		PHY_REG_LIST_EXECUTE(pi);
		wlc_sslpnphy_rx_gain_override_enable(pi, FALSE);
	} else {
		/* Force on the transmit chain */
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, internalrfrxpu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, internalrfrxpu_ovr_val, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, rxgainctrl_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);

		wlc_sslpnphy_set_rx_gain_by_distribution(pi, 15, 1, 3, 3, 7, 3, 3);
		wlc_sslpnphy_rx_gain_override_enable(pi, TRUE);
	}
}

void
BCMROMFN(wlc_sslpnphy_tx_pu)(phy_info_t *pi, bool bEnable)
{
	if (!bEnable) {
		PHY_REG_LIST_START
			/* Clear overrides */
			PHY_REG_AND_ENTRY(SSLPNPHY, AfeCtrlOvr,
				~(uint16)(SSLPNPHY_AfeCtrlOvr_pwdn_dac_ovr_MASK |
				SSLPNPHY_AfeCtrlOvr_dac_clk_disable_ovr_MASK))
			PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvrVal, pwdn_dac_ovr_val, 1)
			PHY_REG_AND_ENTRY(SSLPNPHY, RFOverride0,
				~(uint16)(SSLPNPHY_RFOverride0_gmode_tx_pu_ovr_MASK |
				SSLPNPHY_RFOverride0_internalrftxpu_ovr_MASK |
				SSLPNPHY_RFOverride0_trsw_rx_pu_ovr_MASK |
				SSLPNPHY_RFOverride0_trsw_tx_pu_ovr_MASK |
				SSLPNPHY_RFOverride0_ant_selp_ovr_MASK))
			/* Switch off A band PA ( ePA) */
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, amode_tx_pu_ovr, 0)
			PHY_REG_AND_ENTRY(SSLPNPHY, RFOverrideVal0,
				~(uint16)(SSLPNPHY_RFOverrideVal0_gmode_tx_pu_ovr_val_MASK |
				SSLPNPHY_RFOverrideVal0_internalrftxpu_ovr_val_MASK))
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, ant_selp_ovr_val, 1)
			/* Set TR switch */
			PHY_REG_MOD2_ENTRY(SSLPNPHY, RFOverrideVal0,
				trsw_tx_pu_ovr_val, trsw_rx_pu_ovr_val,
				0, 1)
			PHY_REG_AND_ENTRY(SSLPNPHY, rfoverride3,
				~(uint16)(SSLPNPHY_rfoverride3_stxpapu_ovr_MASK |
				SSLPNPHY_rfoverride3_stxpadpu2g_ovr_MASK |
				SSLPNPHY_rfoverride3_stxpapu2g_ovr_MASK))
			PHY_REG_AND_ENTRY(SSLPNPHY, rfoverride3_val,
				~(uint16)(SSLPNPHY_rfoverride3_val_stxpapu_ovr_val_MASK |
				SSLPNPHY_rfoverride3_val_stxpadpu2g_ovr_val_MASK |
				SSLPNPHY_rfoverride3_val_stxpapu2g_ovr_val_MASK))
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		uint32 ant_ovr;

		PHY_REG_LIST_START
			/* Force on DAC */
			PHY_REG_MOD2_ENTRY(SSLPNPHY, AfeCtrlOvr, pwdn_dac_ovr, dac_clk_disable_ovr, 1, 1)
			PHY_REG_MOD2_ENTRY(SSLPNPHY, AfeCtrlOvrVal, pwdn_dac_ovr_val, dac_clk_disable_ovr_val, 0, 0)
			/* Force on the transmit chain */
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, internalrftxpu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, internalrftxpu_ovr_val, 1)
		PHY_REG_LIST_EXECUTE(pi);

		/* Force the TR switch to transmit */
		wlc_sslpnphy_set_trsw_override(pi, TRUE, FALSE);

		/* Force default antenna */
		ant_ovr = wlc_sslpnphy_set_ant_override(pi, wlc_phy_get_txant(pi));
		BCM_REFERENCE(ant_ovr);

		/* PAD PU */ /* PGA PU */ /* PA PU */
		PHY_REG_MOD3(pi, SSLPNPHY, rfoverride3,
			stxpadpu2g_ovr,
			stxpapu2g_ovr,
			stxpapu_ovr,
			1,
			1,
			1);

		/* PAD PU */ /* PGA PU */ /* PA PU */
		PHY_REG_MOD3(pi, SSLPNPHY, rfoverride3_val,
			stxpadpu2g_ovr_val,
			stxpapu2g_ovr_val,
			stxpapu_ovr_val,
			CHSPEC_IS2G(pi->radio_chanspec) ? 1 : 0,
			CHSPEC_IS2G(pi->radio_chanspec) ? 1 : 0,
			CHSPEC_IS2G(pi->radio_chanspec) ? 1 : 0);

		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			/* Switch on PA for g band */
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, gmode_tx_pu_ovr, 1)
				PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, gmode_tx_pu_ovr_val, 1)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			/* Switch on A band PA ( ePA) */
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, amode_tx_pu_ovr, 1)
				PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, amode_tx_pu_ovr_val, 1)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}
}

/*
 * Play samples from sample play buffer
 */
STATIC void
wlc_sslpnphy_run_samples(phy_info_t *pi,
                         uint16 num_samps,
                         uint16 num_loops,
                         uint16 wait,
                         bool iqcalmode)
{
	PHY_REG_MOD(pi, SSLPNPHY, sslpnCalibClkEnCtrl, forceaphytxFeclkOn, 1);

	PHY_REG_MOD(pi, SSLPNPHY, sampleDepthCount, DepthCount, (num_samps - 1));

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_MOD(pi, SSLPNPHY, sslpnCalibClkEnCtrl, papdTxBbmultPapdifClkEn, 1);
	}

	if (num_loops != 0xffff)
		num_loops--;
	PHY_REG_MOD(pi, SSLPNPHY, sampleLoopCount, LoopCount, num_loops);

	PHY_REG_MOD(pi, SSLPNPHY, sampleInitWaitCount, InitWaitCount, wait);

	if (iqcalmode) {
		/* Enable calibration */
		PHY_REG_LIST_START
			PHY_REG_AND_ENTRY(SSLPNPHY, iqloCalCmdGctl, (uint16)~SSLPNPHY_iqloCalCmdGctl_iqlo_cal_en_MASK)
			PHY_REG_OR_ENTRY(SSLPNPHY, iqloCalCmdGctl, SSLPNPHY_iqloCalCmdGctl_iqlo_cal_en_MASK)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_WRITE(pi, SSLPNPHY, sampleCmd, 1);
		wlc_sslpnphy_tx_pu(pi, 1);
	}
}

void
wlc_sslpnphy_detection_disable(phy_info_t *pi, bool mode)
{
	uint8 phybw40 = IS40MHZ(pi);

	wlc_sslpnphy_lock_ucode_phyreg(pi, 5);

	if (phybw40 == 0) {
		PHY_REG_MOD2(pi, SSLPNPHY, crsgainCtrl, DSSSDetectionEnable, OFDMDetectionEnable,
			(CHSPEC_IS2G(pi->radio_chanspec) ? !mode : 0), !mode);
		PHY_REG_MOD(pi, SSLPNPHY, crsgainCtrl, crseddisable, mode);
	} else {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsgainCtrl_40, crseddisable, (mode));
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, eddisable20ul, crseddisable_20U, (mode));
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, eddisable20ul, crseddisable_20L, (mode));

			PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsgainCtrl_40, OFDMDetectionEnable, (!mode));
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, transFreeThresh_20U, OFDMDetectionEnable, (!mode));
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, transFreeThresh_20L, OFDMDetectionEnable, (!mode));

			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				if (CHSPEC_SB_UPPER(pi->radio_chanspec)) {
					PHY_REG_MOD(pi, SSLPNPHY_Rev2, transFreeThresh_20U, DSSSDetectionEnable, !mode);
					PHY_REG_MOD(pi, SSLPNPHY_Rev2, transFreeThresh_20L, DSSSDetectionEnable, 0);
				} else {
					PHY_REG_MOD(pi, SSLPNPHY_Rev2, transFreeThresh_20U, DSSSDetectionEnable, 0);
					PHY_REG_MOD(pi, SSLPNPHY_Rev2, transFreeThresh_20L, DSSSDetectionEnable, !mode);
				}
			} else {
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, DSSSDetectionEnable, 0x00)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, DSSSDetectionEnable, 0x00)
				PHY_REG_LIST_EXECUTE(pi);			}
		}
	}

	wlc_sslpnphy_unlock_ucode_phyreg(pi);
}

void
BCMROMFN(wlc_sslpnphy_deaf_mode)(phy_info_t *pi, bool mode)
{
	uint8 phybw40 = IS40MHZ(pi);
	PHY_REG_MOD(pi, SSLPNPHY, rfoverride2, gmode_ext_lna_gain_ovr, (mode));
	PHY_REG_MOD(pi, SSLPNPHY, rfoverride2val, gmode_ext_lna_gain_ovr_val, 0);
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		PHY_REG_MOD(pi, SSLPNPHY, rfoverride2, amode_ext_lna_gain_ovr, (mode));
		PHY_REG_MOD(pi, SSLPNPHY, rfoverride2val, amode_ext_lna_gain_ovr_val, 0);
	}
#endif
	if (phybw40 == 0) {
		PHY_REG_MOD2(pi, SSLPNPHY, crsgainCtrl, DSSSDetectionEnable, OFDMDetectionEnable,
			(CHSPEC_IS2G(pi->radio_chanspec) ? !mode : 0), !mode);
		PHY_REG_MOD(pi, SSLPNPHY, crsgainCtrl, crseddisable, (mode));
	} else {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsgainCtrl_40, crseddisable, (mode));
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, eddisable20ul, crseddisable_20U, (mode));
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, eddisable20ul, crseddisable_20L, (mode));
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				if (CHSPEC_SB_UPPER(pi->radio_chanspec)) {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U,
							DSSSDetectionEnable, 1)
						PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L,
							DSSSDetectionEnable, 0)
					PHY_REG_LIST_EXECUTE(pi);
				} else {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U,
							DSSSDetectionEnable, 0)
						PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L,
							DSSSDetectionEnable, 1)
					PHY_REG_LIST_EXECUTE(pi);
				}
			} else {
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, DSSSDetectionEnable, 0x00)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, DSSSDetectionEnable, 0x00)
				PHY_REG_LIST_EXECUTE(pi);
			}

			PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsgainCtrl_40, OFDMDetectionEnable, (!mode));
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, transFreeThresh_20U, OFDMDetectionEnable, (!mode));
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, transFreeThresh_20L, OFDMDetectionEnable, (!mode));
		}
	}
}
/*
* Given a test tone frequency, continuously play the samples. Ensure that num_periods
* specifies the number of periods of the underlying analog signal over which the
* digital samples are periodic
*/
void
wlc_sslpnphy_start_tx_tone(phy_info_t *pi, int32 f_kHz, uint16 max_val, bool iqcalmode)
{
	uint8 phy_bw;
	uint16 num_samps, t, k;
	uint32 bw;
	fixed theta = 0, rot = 0;
	cint32 tone_samp;
	uint32 data_buf[64];
	uint16 i_samp, q_samp;

	/* Save active tone frequency */
	pi->phy_tx_tone_freq = f_kHz;

	/* Turn off all the crs signals to the MAC */
	wlc_sslpnphy_set_deaf(pi);

	phy_bw = 40;

	/* allocate buffer */
	if (f_kHz) {
		k = 1;
		do {
			bw = phy_bw * 1000 * k;
			num_samps = bw / ABS(f_kHz);
			ASSERT(num_samps <= ARRAYSIZE(data_buf));
			k++;
		} while ((num_samps * (uint32)(ABS(f_kHz))) !=  bw);
	} else
		num_samps = 2;

	WL_INFORM(("wl%d: %s: %d kHz, %d samples\n",
		GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
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
		i_samp = (uint16)(FLOAT(tone_samp.i * max_val) & 0x3ff);
		q_samp = (uint16)(FLOAT(tone_samp.q * max_val) & 0x3ff);
		data_buf[t] = (i_samp << 10) | q_samp;
	}

	/* in SSLPNPHY, we need to bring SPB out of standby before using it */
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCtrl3, sram_stby, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, samplePlayClkEn, 1)
	PHY_REG_LIST_EXECUTE(pi);

	/* load sample table */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
		data_buf, num_samps, 32, 0);

	/* run samples */
	wlc_sslpnphy_run_samples(pi, num_samps, 0xffff, 0, iqcalmode);
}

STATIC bool
wlc_sslpnphy_iqcal_wait(phy_info_t *pi)
{
	uint delay_count = 0;

	while (wlc_sslpnphy_iqcal_active(pi)) {
		OSL_DELAY(100);
		delay_count++;

		if (delay_count > (10 * 500)) /* 500 ms */
			break;
	}

	WL_NONE(("wl%d: %s: %u us\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, delay_count * 100));

	return (wlc_sslpnphy_iqcal_active(pi) == 0);
}

void
BCMROMFN(wlc_sslpnphy_stop_tx_tone)(phy_info_t *pi)
{
	int16 playback_status;

	pi->phy_tx_tone_freq = 0;

	/* Stop sample buffer playback */
	playback_status = phy_reg_read(pi, SSLPNPHY_sampleStatus);
	if (playback_status & SSLPNPHY_sampleStatus_NormalPlay_MASK) {
		wlc_sslpnphy_tx_pu(pi, 0);
		PHY_REG_MOD(pi, SSLPNPHY, sampleCmd, stop, 1);
	} else if (playback_status & SSLPNPHY_sampleStatus_iqlocalPlay_MASK) {
		PHY_REG_MOD(pi, SSLPNPHY, iqloCalCmdGctl, iqlo_cal_en, 0);
	}

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, samplePlayClkEn, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, forceaphytxFeclkOn, 0)
	PHY_REG_LIST_EXECUTE(pi);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_MOD(pi, SSLPNPHY, sslpnCalibClkEnCtrl, papdTxBbmultPapdifClkEn, 0);
	}
	/* in SSLPNPHY, we need to bring SPB out of standby before using it */
	PHY_REG_MOD(pi, SSLPNPHY, sslpnCtrl3, sram_stby, 1);



	/* Restore all the crs signals to the MAC */
	wlc_sslpnphy_clear_deaf(pi);
}

STATIC void
wlc_sslpnphy_clear_trsw_override(phy_info_t *pi)
{
	/* Clear overrides */
	PHY_REG_AND(pi, SSLPNPHY, RFOverride0,
		(uint16)~(SSLPNPHY_RFOverride0_trsw_tx_pu_ovr_MASK | SSLPNPHY_RFOverride0_trsw_rx_pu_ovr_MASK));
}

/*
 * TX IQ/LO Calibration
 *
 * args: target_gains = Tx gains *for* which the cal is done, not necessarily *at* which it is done
 *       If not specified, will use current Tx gains as target gains
 *
 */
STATIC void
wlc_sslpnphy_tx_iqlo_cal(
	phy_info_t *pi,
	sslpnphy_txgains_t *target_gains,
	sslpnphy_cal_mode_t cal_mode,
	bool keep_tone)
{
	/* starting values used in full cal
	 * -- can fill non-zero vals based on lab campaign (e.g., per channel)
	 * -- format: a0,b0,a1,b1,ci0_cq0_ci1_cq1,di0_dq0,di1_dq1,ei0_eq0,ei1_eq1,fi0_fq0,fi1_fq1
	 */
	sslpnphy_txgains_t cal_gains, temp_gains;
	uint16 hash;
	uint8 band_idx;
	int j;
	uint16 ncorr_override[5];
	uint16 syst_coeffs[] =
		{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000,
		0x0000, 0x0000, 0x0000, 0x0000, 0x0000};

	/* cal commands full cal and recal */
	uint16 commands_fullcal[] =  { 0x8434, 0x8334, 0x8084, 0x8267, 0x8056, 0x8234 };
	uint16 commands_recal[] =  { 0x8312, 0x8055, 0x8212 };

	/* calCmdNum register: log2 of settle/measure times for search/gain-ctrl, 4 bits each */
	uint16 command_nums_fullcal[] = { 0x7a97, 0x7a97, 0x7a97, 0x7a87, 0x7a87, 0x7b97 };
	uint16 command_nums_recal[] = {  0x7997, 0x7987, 0x7a97 };
	uint16 *command_nums = command_nums_fullcal;


	uint16 *start_coeffs = NULL, *cal_cmds = NULL, cal_type, diq_start;
	uint16 tx_pwr_ctrl_old, rssi_old;
	uint16 papd_ctrl_old = 0, auxadc_ctrl_old = 0;
	uint16 muxsel_old, pa_ctrl_1_old, extstxctrl1_old;
	uint8 iqcal_old;
	bool tx_gain_override_old;
	sslpnphy_txgains_t old_gains = {0, 0, 0, 0};
	uint i, n_cal_cmds = 0, n_cal_start = 0;
	uint16 ccktap0 = 0, ccktap1 = 0, ccktap2 = 0, ccktap3 = 0, ccktap4 = 0;
	uint16 epa_ovr, epa_ovr_val;
#ifdef BAND5G
	uint16 sslpnCalibClkEnCtrl_old = 0;
	uint16 Core1TxControl_old = 0;
#endif
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	if (NORADIO_ENAB(pi->pubpi))
		return;

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		/* Saving the default states of realfilter coefficients */
		ccktap0 = phy_reg_read(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap0);
		ccktap1 = phy_reg_read(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap1);
		ccktap2 = phy_reg_read(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap2);
		ccktap3 = phy_reg_read(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap3);
		ccktap4 = phy_reg_read(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap4);

		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_cck_tap0, 255)
			PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_cck_tap1, 0)
			PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_cck_tap2, 0)
			PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_cck_tap3, 0)
			PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_cck_tap4, 0)
		PHY_REG_LIST_EXECUTE(pi);
	}

	switch (cal_mode) {
		case SSLPNPHY_CAL_FULL:
			start_coeffs = syst_coeffs;
			cal_cmds = commands_fullcal;
			n_cal_cmds = ARRAYSIZE(commands_fullcal);
			break;

		case SSLPNPHY_CAL_RECAL:
			ASSERT(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs_valid);
			start_coeffs = sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs;
			cal_cmds = commands_recal;
			n_cal_cmds = ARRAYSIZE(commands_recal);
			command_nums = command_nums_recal;
			break;

		default:
			ASSERT(FALSE);
	}

	/* Fill in Start Coeffs */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		start_coeffs, 11, 16, 64);

	/* Save original tx power control mode */
	tx_pwr_ctrl_old = wlc_sslpnphy_get_tx_pwr_ctrl(pi);

	/* Save RF overide values */
	epa_ovr = phy_reg_read(pi, SSLPNPHY_RFOverride0);
	epa_ovr_val = phy_reg_read(pi, SSLPNPHY_RFOverrideVal0);

	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	/* Save old and apply new tx gains if needed */
	tx_gain_override_old = wlc_sslpnphy_tx_gain_override_enabled(pi);
	if (tx_gain_override_old)
		wlc_sslpnphy_get_tx_gain(pi, &old_gains);

	if (!target_gains) {
		if (!tx_gain_override_old)
			wlc_sslpnphy_set_tx_pwr_by_index(pi, sslpnphy_specific->sslpnphy_tssi_idx);
		wlc_sslpnphy_get_tx_gain(pi, &temp_gains);
		target_gains = &temp_gains;
	}

	hash = (target_gains->gm_gain << 8) |
		(target_gains->pga_gain << 4) |
		(target_gains->pad_gain);

	cal_gains = *target_gains;
	bzero(ncorr_override, sizeof(ncorr_override));
	for (j = 0; j < iqcal_gainparams_numgains_sslpnphy[band_idx]; j++) {
		if (hash == tbl_iqcal_gainparams_sslpnphy[band_idx][j][0]) {
			cal_gains.gm_gain = tbl_iqcal_gainparams_sslpnphy[band_idx][j][1];
			cal_gains.pga_gain = tbl_iqcal_gainparams_sslpnphy[band_idx][j][2];
			cal_gains.pad_gain = tbl_iqcal_gainparams_sslpnphy[band_idx][j][3];
			bcopy(&tbl_iqcal_gainparams_sslpnphy[band_idx][j][3], ncorr_override,
				sizeof(ncorr_override));
			break;
		}
	}

	wlc_sslpnphy_set_tx_gain(pi, &cal_gains);

	WL_INFORM(("wl%d: %s: target gains: %d %d %d %d, cal_gains: %d %d %d %d\n",
		GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
		target_gains->gm_gain,
		target_gains->pga_gain,
		target_gains->pad_gain,
		target_gains->dac_gain,
		cal_gains.gm_gain,
		cal_gains.pga_gain,
		cal_gains.pad_gain,
		cal_gains.dac_gain));

	PHY_REG_MOD(pi, SSLPNPHY, sslpnCalibClkEnCtrl, txFrontEndCalibClkEn, 1);

	/* Open TR switch */
	wlc_sslpnphy_set_swctrl_override(pi, SWCTRL_BT_TX);

	muxsel_old = phy_reg_read(pi, SSLPNPHY_AfeCtrlOvrVal);
	pa_ctrl_1_old = read_radio_reg(pi, RADIO_2063_PA_CTRL_1);
	extstxctrl1_old = phy_reg_read(pi, SSLPNPHY_extstxctrl1);
	/* Removing all the radio reg intervention in selecting the mux */
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvr, rssi_muxsel_ovr, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 4)
	PHY_REG_LIST_EXECUTE(pi);
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		or_radio_reg(pi, RADIO_2063_PA_SP_1, 0x2);
		or_radio_reg(pi, RADIO_2063_COMMON_07, 0x10);

		mod_radio_reg(pi, RADIO_2063_PA_CTRL_1, 0x1 << 2, 0 << 2);
	} else
#endif
		mod_radio_reg(pi, RADIO_2063_PA_CTRL_1, 0x1 << 2, 1 << 2);

	PHY_REG_OR(pi, SSLPNPHY, extstxctrl1, 0x1000);

	/* Adjust ADC common mode */
	rssi_old = phy_reg_read(pi, SSLPNPHY_AfeRSSICtrl1);

	/* crk: sync up with tcl */
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec))
		PHY_REG_MOD(pi, SSLPNPHY, AfeRSSICtrl1, rssi_ctrl1, 0x28af);
	else
#endif
	PHY_REG_MOD_RAW(pi, SSLPNPHY_AfeRSSICtrl1, 0x3ff, 0xaf);

	/* Set tssi switch to use IQLO */
	iqcal_old = (uint8)read_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2);
	and_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, (uint8)~0x0d);

	/* Power on IQLO block */
	and_radio_reg(pi, RADIO_2063_IQCAL_GVAR, (uint8)~0x80);

	/* Turn off PAPD */

	papd_ctrl_old = phy_reg_read(pi, SSLPNPHY_papd_control);
	PHY_REG_MOD(pi, SSLPNPHY, papd_control, papdCompEn, 0);
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			sslpnCalibClkEnCtrl_old = phy_reg_read(pi, SSLPNPHY_sslpnCalibClkEnCtrl);
			Core1TxControl_old = phy_reg_read(pi, SSLPNPHY_Core1TxControl);
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, papdTxClkEn, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, Core1TxControl, txcomplexfilten, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, papdFiltClkEn, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, papdRxClkEn, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, Core1TxControl, txrealfilten, 0)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}
#endif /* BAND5G */
	auxadc_ctrl_old = phy_reg_read(pi, SSLPNPHY_auxadcCtrl);
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, auxadcCtrl, iqlocalEn, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, auxadcCtrl, rssiformatConvEn, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Load the LO compensation gain table */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		sslpnphy_iqcal_loft_gainladder, ARRAYSIZE(sslpnphy_iqcal_loft_gainladder),
		16, 0);

	/* Load the IQ calibration gain table */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		sslpnphy_iqcal_ir_gainladder, ARRAYSIZE(sslpnphy_iqcal_ir_gainladder),
		16, 32);

	/* Set Gain Control Parameters */
	/* iqlocal_en<15> / start_index / thresh_d2 / ladder_length_d2 */
	PHY_REG_WRITE(pi, SSLPNPHY, iqloCalCmdGctl, 0x0aa9);

	/* Send out calibration tone */
	if (!pi->phy_tx_tone_freq) {
		wlc_sslpnphy_start_tx_tone(pi, 3750, 88, 1);
	}
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride3, stxpapu_ovr, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride3_val, stxpapu_ovr_val, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Disable epa during calibrations */
	if ((CHSPEC_IS5G(pi->radio_chanspec)) &&
		(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, amode_tx_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, amode_tx_pu_ovr_val, 0)
		PHY_REG_LIST_EXECUTE(pi);
	}

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

		PHY_REG_WRITE(pi, SSLPNPHY, iqloCalCmdNnum, command_num);

		WL_NONE(("wl%d: %s: running cmd: %x, cmd_num: %x\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, cal_cmds[i], command_nums[i]));

		/* need to set di/dq to zero if analog LO cal */
		if ((cal_type == 3) || (cal_type == 4)) {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
				&diq_start, 1, 16, 69);

			/* Set to zero during analog LO cal */
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL, &zero_diq,
				1, 16, 69);
		}

		/* Issue cal command */
		PHY_REG_WRITE(pi, SSLPNPHY, iqloCalCmd, cal_cmds[i]);

		/* Wait until cal command finished */
		if (!wlc_sslpnphy_iqcal_wait(pi)) {
			WL_ERROR(("wl%d: %s: tx iqlo cal failed to complete\n",
				GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
			/* No point to continue */
			goto cleanup;
		}

		/* Copy best coefficients to start coefficients */
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
			best_coeffs, ARRAYSIZE(best_coeffs), 16, 96);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL, best_coeffs,
			ARRAYSIZE(best_coeffs), 16, 64);

		/* restore di/dq in case of analog LO cal */
		if ((cal_type == 3) || (cal_type == 4)) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
				&diq_start, 1, 16, 69);
		}
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
			sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs,
			ARRAYSIZE(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs), 16, 96);
	}

	/*
	 * Apply Results
	 */

	/* Save calibration results */
	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs,
		ARRAYSIZE(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs), 16, 96);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs_valid = TRUE;

	/* Apply IQ Cal Results */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		&sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[0], 4, 16, 80);

	/* Apply Digital LOFT Comp */
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_IQLOCAL,
		&sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[5], 2, 16, 85);

	/* Dump results */
	WL_INFORM(("wl%d: %s %d complete, IQ %d %d LO %d %d %d %d %d %d\n",
		GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, pi->radio_chanspec,
		(int16)sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[0],
		(int16)sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[1],
		(int8)((sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[5] & 0xff00) >> 8),
		(int8)(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[5] & 0x00ff),
		(int8)((sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[7] & 0xff00) >> 8),
		(int8)(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[7] & 0x00ff),
		(int8)((sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[9] & 0xff00) >> 8),
		(int8)(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[9] & 0x00ff)));

cleanup:
	/* Switch off test tone */
	if (!keep_tone)
		wlc_sslpnphy_stop_tx_tone(pi);

	PHY_REG_LIST_START
		/* Reset calibration  command register */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, iqloCalCmdGctl, 0)
		/* RSSI ADC selection */
		PHY_REG_MOD_ENTRY(SSLPNPHY, auxadcCtrl, iqlocalEn, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Power off IQLO block */
	or_radio_reg(pi, RADIO_2063_IQCAL_GVAR, 0x80);

	/* Adjust ADC common mode */
	PHY_REG_WRITE(pi, SSLPNPHY, AfeRSSICtrl1, rssi_old);

	/* Restore tssi switch */
	write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, iqcal_old);

	/* Restore PAPD */
	PHY_REG_WRITE(pi, SSLPNPHY, papd_control, papd_ctrl_old);

	/* Restore epa after cal */
	PHY_REG_WRITE(pi, SSLPNPHY, RFOverride0, epa_ovr);
	PHY_REG_WRITE(pi, SSLPNPHY, RFOverrideVal0, epa_ovr_val);

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			PHY_REG_WRITE(pi, SSLPNPHY, sslpnCalibClkEnCtrl, sslpnCalibClkEnCtrl_old);
			PHY_REG_WRITE(pi, SSLPNPHY, Core1TxControl, Core1TxControl_old);
		}
	}
#endif
	/* Restore ADC control */
	PHY_REG_WRITE(pi, SSLPNPHY, auxadcCtrl, auxadc_ctrl_old);

	PHY_REG_WRITE(pi, SSLPNPHY, AfeCtrlOvrVal, muxsel_old);
	write_radio_reg(pi, RADIO_2063_PA_CTRL_1, pa_ctrl_1_old);
	PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl1, extstxctrl1_old);

	/* TR switch */
	wlc_sslpnphy_set_swctrl_override(pi, SWCTRL_OVR_DISABLE);

	PHY_REG_LIST_START
		/* RSSI on/off */
		PHY_REG_AND_ENTRY(SSLPNPHY, AfeCtrlOvr, (uint16)~SSLPNPHY_AfeCtrlOvr_pwdn_rssi_ovr_MASK)
		PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, txFrontEndCalibClkEn, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Restore tx power and reenable tx power control */
	if (tx_gain_override_old)
		wlc_sslpnphy_set_tx_gain(pi, &old_gains);
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl_old);
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		/* Restoring the original values */
		PHY_REG_WRITE(pi, SSLPNPHY_Rev2, txrealfilt_cck_tap0, ccktap0);
		PHY_REG_WRITE(pi, SSLPNPHY_Rev2, txrealfilt_cck_tap1, ccktap1);
		PHY_REG_WRITE(pi, SSLPNPHY_Rev2, txrealfilt_cck_tap2, ccktap2);
		PHY_REG_WRITE(pi, SSLPNPHY_Rev2, txrealfilt_cck_tap3, ccktap3);
		PHY_REG_WRITE(pi, SSLPNPHY_Rev2, txrealfilt_cck_tap4, ccktap4);
	}
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvr, rssi_muxsel_ovr, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride3, stxpapu_ovr, 0)
	PHY_REG_LIST_EXECUTE(pi);
}

void
wlc_sslpnphy_get_tx_iqcc(phy_info_t *pi, uint16 *a, uint16 *b)
{
	uint16 iqcc[2];

	wlc_sslpnphy_common_read_table(pi, 0, iqcc, 2, 16, 80);

	*a = iqcc[0];
	*b = iqcc[1];
}

uint16
wlc_sslpnphy_get_tx_locc(phy_info_t *pi)
{
	uint16 didq;

	/* Update iqloCaltbl */
	wlc_sslpnphy_common_read_table(pi, 0, &didq, 1, 16, 85);

	return didq;
}

/* Run iqlo cal and populate iqlo portion of tx power control table */
void
wlc_sslpnphy_txpwrtbl_iqlo_cal(phy_info_t *pi)
{
	sslpnphy_txgains_t target_gains;
	uint8 save_bb_mult;
	uint16 a, b, didq, save_pa_gain = 0;
	uint idx;
	uint32 val;
	uint8 gm, pga, pad;
	uint16 tx_pwr_ctrl;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
#ifdef BAND5G
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif /* BAND5G */
	/* Store state */
	save_bb_mult = wlc_sslpnphy_get_bbmult(pi);

	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);

	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	/* Set up appropriate target gains */
	{
		/* PA gain */
		save_pa_gain = wlc_sslpnphy_get_pa_gain(pi);

		wlc_sslpnphy_set_pa_gain(pi, 0x10);

#ifdef BAND5G
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			/* Since we can not switch off pa from phy put gain to 0 */
			wlc_sslpnphy_set_pa_gain(pi, 0x00);
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 200;
			target_gains.pad_gain = 245;
			target_gains.dac_gain = 0;
			/* doing txiqlo at gm_gain = 31, coupled to txgain tbl */
			if (sdna_board_flag) {
				target_gains.gm_gain = 31;
				target_gains.pga_gain = 180;
				target_gains.pad_gain = 240;
				target_gains.dac_gain = 0;
			}
			if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA) {
				target_gains.gm_gain = 3;
				target_gains.pga_gain = 105;
				target_gains.pad_gain = 240;
				target_gains.dac_gain = 0;
			}
		} else
#endif /* BAND5G */
		{
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 76;
			target_gains.pad_gain = 241;
			target_gains.dac_gain = 0;

			/* Do the second tx iq cal with gains corresponding to 14dBm */
			if (sslpnphy_specific->sslpnphy_papd_cal_done) {
				/* Read gains corresponding to 14dBm index */
				wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
					&val, 1, 32,
					(SSLPNPHY_TX_PWR_CTRL_GAIN_OFFSET +
					sslpnphy_specific->sslpnphy_start_idx));
				gm = (val & 0xff);
				pga = ((val & 0xff00) >> 8);
				pad = ((val & 0xff0000) >> 16);
				target_gains.gm_gain = gm;
				target_gains.pga_gain = pga;
				target_gains.pad_gain = pad;
				target_gains.dac_gain = 0;
			}
		}
	}

	/* Run cal */
	wlc_sslpnphy_tx_iqlo_cal(pi, &target_gains, (sslpnphy_specific->sslpnphy_recal ?
		SSLPNPHY_CAL_RECAL : SSLPNPHY_CAL_FULL), FALSE);

	{
		uint8 ei0, eq0, fi0, fq0;

		wlc_sslpnphy_get_radio_loft(pi, &ei0, &eq0, &fi0, &fq0);
		if ((ABS((int8)fi0) == 15) && (ABS((int8)fq0) == 15)) {
			WL_ERROR(("wl%d: %s: tx iqlo cal failed, retrying...\n",
				GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
#ifdef BAND5G
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			target_gains.gm_gain = 255;
			target_gains.pga_gain = 255;
			target_gains.pad_gain = 0xf0;
			target_gains.dac_gain = 0;
		} else
#endif
		{
			target_gains.gm_gain = 7;
			target_gains.pga_gain = 45;
			target_gains.pad_gain = 186;
			target_gains.dac_gain = 0;
		}
			/* Re-run cal */
			wlc_sslpnphy_tx_iqlo_cal(pi, &target_gains, SSLPNPHY_CAL_FULL, FALSE);
		}
	}

	/* Get calibration results */
	wlc_sslpnphy_get_tx_iqcc(pi, &a, &b);
	didq = wlc_sslpnphy_get_tx_locc(pi);

	/* Populate tx power control table with coeffs */
	for (idx = 0; idx < 128; idx++) {
		/* iq */
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);
		val = (val & 0xfff00000) |
			((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);

		/* loft */
		val = didq;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&val, 1, 32, SSLPNPHY_TX_PWR_CTRL_LO_OFFSET + idx);
	}

	/* Restore state */
	wlc_sslpnphy_set_bbmult(pi, save_bb_mult);
	wlc_sslpnphy_set_pa_gain(pi, save_pa_gain);
	/* Restore power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);
}

STATIC void
wlc_sslpnphy_set_tx_filter_bw(phy_info_t *pi, uint16 bw)
{
	uint8 idac_setting;

	/* cck/all non-ofdm setting */
	PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg0, lpfbwlut0, bw);
	/* ofdm setting */
	PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg1, lpfbwlut5, bw);

	if (0) {
		if (bw <= 1)
			idac_setting = 0x0e;
		else if (bw <= 3)
			idac_setting = 0x13;
		else
			idac_setting = 0x1b;
		mod_radio_reg(pi, RADIO_2063_TXBB_CTRL_2, 0x1f, idac_setting);
	}
}

STATIC void
BCMROMFN(wlc_sslpnphy_set_rx_gain)(phy_info_t *pi, uint32 gain)
{
	uint16 trsw, ext_lna, lna1, lna2, gain0_15, gain16_19;

	trsw = (gain & ((uint32)1 << 20)) ? 0 : 1;
	ext_lna = (uint16)(gain >> 21) & 0x01;
	lna1 = (uint16)(gain >> 2) & 0x03;
	lna2 = (uint16)(gain >> 6) & 0x03;
	BCM_REFERENCE(lna2);
	gain0_15 = (uint16)gain & 0xffff;
	gain16_19 = (uint16)(gain >> 16) & 0x0f;

	PHY_REG_MOD(pi, SSLPNPHY, RFOverrideVal0, trsw_rx_pu_ovr_val, trsw);
	PHY_REG_MOD(pi, SSLPNPHY, rfoverride2val, gmode_ext_lna_gain_ovr_val, ext_lna);
	PHY_REG_MOD(pi, SSLPNPHY, rfoverride2val, amode_ext_lna_gain_ovr_val, ext_lna);
	PHY_REG_MOD(pi, SSLPNPHY, rxgainctrl0ovrval, rxgainctrl_ovr_val0, gain0_15);
	PHY_REG_MOD(pi, SSLPNPHY, rxlnaandgainctrl1ovrval, rxgainctrl_ovr_val1, gain16_19);

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		PHY_REG_MOD(pi, SSLPNPHY, rfoverride2val, slna_gain_ctrl_ovr_val, lna1);
		PHY_REG_MOD(pi, SSLPNPHY, RFinputOverrideVal, wlslnagainctrl_ovr_val, lna1);
	}
	wlc_sslpnphy_rx_gain_override_enable(pi, TRUE);
}

STATIC void
wlc_sslpnphy_papd_cal_setup_cw(phy_info_t *pi)
{
	static const uint32 papd_buf[] = {0x7fc00, 0x5a569, 0x1ff, 0xa5d69, 0x80400, 0xa5e97, 0x201, 0x5a697};
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	PHY_REG_LIST_START
		/* Tune the hardware delay */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_spb2papdin_dly, 33)
		/* Set samples/cycle/4 for q delay */
		PHY_REG_MOD2_ENTRY(SSLPNPHY, papd_variable_delay,
			papd_pre_int_est_dly, papd_int_est_ovr_or_cw_dly, 4-1, 0)
	PHY_REG_LIST_EXECUTE(pi);

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		if  (sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4)
			PHY_REG_WRITE(pi, SSLPNPHY, papd_rx_gain_comp_dbm, 0);
		else
			PHY_REG_WRITE(pi, SSLPNPHY, papd_rx_gain_comp_dbm, 200);
	} else {
		PHY_REG_WRITE(pi, SSLPNPHY, papd_rx_gain_comp_dbm, 100);
	}
	/* Set LUT begin gain, step gain, and size (Reset values, remove if possible) */
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		uint freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
		if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID) ||
			(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID)) {
			PHY_REG_WRITE(pi, SSLPNPHY, papd_rx_gain_comp_dbm, 100);
			if (freq >= 5520)
				PHY_REG_WRITE(pi, SSLPNPHY, papd_rx_gain_comp_dbm, 100);
			if (freq >= 5600)
				PHY_REG_WRITE(pi, SSLPNPHY, papd_rx_gain_comp_dbm, 200);
			if (freq >= 5745)
				PHY_REG_WRITE(pi, SSLPNPHY, papd_rx_gain_comp_dbm, 300);
		}
		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_rx_gain_comp_dbm, 500)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_begin, 6000)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_step, 0x444)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_begin, 5000)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_step, 0x222)
			PHY_REG_LIST_EXECUTE(pi);
		}
	} else
#endif /* BAND 5G */
	{
		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_begin, 6000)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_step, 0x444)
		PHY_REG_LIST_EXECUTE(pi);
	}

	if (sslpnphy_specific->sslpnphy_papd_tweaks_enable) {
		PHY_REG_WRITE(pi, SSLPNPHY, papd_track_pa_lut_begin,
			sslpnphy_specific->sslpnphy_papd_tweaks.papd_track_pa_lut_begin);
		PHY_REG_WRITE(pi, SSLPNPHY, papd_track_pa_lut_step,
			sslpnphy_specific->sslpnphy_papd_tweaks.papd_track_pa_lut_step);
	}

	PHY_REG_LIST_START
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_end, 0x3f)
		/* Set papd constants (reset values, remove if possible) */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_dbm_offset, 0x681)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_dbm_adj_mult_factor, 0xcd8)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_dbm_adj_add_factor_lsb, 0xc15c)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_dbm_adj_add_factor_msb, 0x1b)
		/* Dc estimation samples */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_ofdm_dc_est, 0x49)
		/* Processing parameters */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_num_skip_count, 0x27)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_num_samples_count, 255)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_sync_count, 319)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_ofdm_index_num_cnt, 255)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_ofdm_corelator_run_cnt, 1)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, smoothenLut_max_thr, 0x7ff)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_ofdm_sync_clip_threshold, 0)
		/* Overide control Params */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_ofdm_loop_gain_offset_ovr_15_0, 0x0000)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_ofdm_loop_gain_offset_ovr_18_16, 0x0007)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_dcest_i_ovr, 0x0000)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_dcest_q_ovr, 0x0000)
		/* PAPD Update */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_lut_update_beta, 0x0008)
		/* Spb parameters */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_spb_num_vld_symbols_n_dly, 0x60)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, sampleDepthCount, 8-1)
		/* Load Spb - Remove it latter when CW waveform gets a fixed place inside SPB. */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_spb_rd_address, 0x0000)
	PHY_REG_LIST_EXECUTE(pi);

	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
		&papd_buf, 8, 32, 0);

	/* BBMULT parameters */
	PHY_REG_LIST_START
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_bbmult_num_symbols, 1-1)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_rx_sm_iqmm_gain_comp, 0x100)
	PHY_REG_LIST_EXECUTE(pi);
}

STATIC void
wlc_sslpnphy_papd_cal_core(
	phy_info_t *pi,
	sslpnphy_papd_cal_type_t calType,
	bool rxGnCtrl,
	uint8 num_symbols4lpgn,
	bool init_papd_lut,
	uint16 papd_bbmult_init,
	uint16 papd_bbmult_step,
	bool papd_lpgn_ovr,
	uint16 LPGN_I,
	uint16 LPGN_Q)
{
	PHY_REG_MOD(pi, SSLPNPHY, papd_control2, papd_loop_gain_cw_ovr, papd_lpgn_ovr);

	/* Load papd comp delta table */

	PHY_REG_MOD2(pi, SSLPNPHY, papd_control, papdCompEn, papd_use_pd_out4learning, 0, 0);

	/* Reset the PAPD Hw to reset register values */
	/* Check if this is what tcl meant */

	if (calType == SSLPNPHY_PAPD_CAL_CW) {

		/* Overide control Params */
		/* PHY_REG_WRITE(pi, SSLPNPHY, papd_control2, 0); */
		PHY_REG_WRITE(pi, SSLPNPHY, papd_loop_gain_ovr_cw_i, LPGN_I);
		PHY_REG_WRITE(pi, SSLPNPHY, papd_loop_gain_ovr_cw_q, LPGN_Q);

		/* Spb parameters */
		PHY_REG_WRITE(pi, SSLPNPHY, papd_track_num_symbols_count, num_symbols4lpgn);
		PHY_REG_WRITE(pi, SSLPNPHY, sampleLoopCount, (num_symbols4lpgn+1)*20-1);

		/* BBMULT parameters */
		PHY_REG_WRITE(pi, SSLPNPHY, papd_bbmult_init, papd_bbmult_init);
		PHY_REG_WRITE(pi, SSLPNPHY, papd_bbmult_step, papd_bbmult_step);
		/* Run PAPD HW Cal */
		PHY_REG_WRITE(pi, SSLPNPHY, papd_control, 0xa021);

#ifndef SSLPNPHY_PAPD_OFDM
	}
#else
	} else {

		PHY_REG_LIST_START
			/* Number of Sync and Training Symbols */
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_num_symbols_count, 255)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_sync_symbol_count, 49)
			/* Load Spb */
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_spb_rd_address, 0x0000)
		PHY_REG_LIST_EXECUTE(pi);


		for (j = 0; j < 16; j++) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
				&sslpnphy_papd_cal_ofdm_tbl[j][0], 64, 32, j * 64);
		}

		for (; j < 25; j++) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
				&sslpnphy_papd_cal_ofdm_tbl[j][0], 64, 32, j * 64);
		}

		PHY_REG_LIST_START
			/* Number of CW samples in spb - 1; Num of OFDM samples per symbol in SPB */
			PHY_REG_WRITE_ENTRY(SSLPNPHY, sampleDepthCount, 160-1)
			/* Number of loops - 1 for CW; 2-1 for replay with rotation by -j in OFDM */
			PHY_REG_WRITE_ENTRY(SSLPNPHY, sampleLoopCount,  1)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_bbmult_init, 20000)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_bbmult_step, 22000)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_bbmult_ofdm_sync, 8192)
			/* If Cal is done at a gain other than the ref gain
			* (incremental cal over an earlier cal)
			* then gain difference needs to be subracted here
			*/
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_begin, 6700)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_step, 0x222)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_track_pa_lut_end,  0x3f)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_lut_update_beta, 0x8)
			/* [3:0] vld sym in spb         -1, ofdm; [7:4] spb delay */
			PHY_REG_WRITE_ENTRY(SSLPNPHY, papd_spb_num_vld_symbols_n_dly, 0x69)
		PHY_REG_LIST_EXECUTE(pi);

		if (rxGnCtrl) {
			/* Only run the synchronizer - no tracking */
			PHY_REG_WRITE(pi, SSLPNPHY, papd_track_num_symbols_count, 0);
		}

		/* Run PAPD HW Cal */
		PHY_REG_WRITE(pi, SSLPNPHY, papd_control, 0xb083);
	}
#endif /* SSLPNPHY_PAPD_OFDM */

	/* Wait for completion, around 1ms */
	SPINWAIT(
		PHY_REG_READ(pi, SSLPNPHY, papd_control, papd_cal_run),
		1 * 1000);

}

STATIC uint32
wlc_sslpnphy_papd_rxGnCtrl(
	phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl,
	uint8 CurTxGain)
{
	/* Square of Loop Gain (inv) target for CW (reach as close to tgt, but be more than it) */
	/* dB Loop gain (inv) target for OFDM (reach as close to tgt,but be more than it) */
	int32 rxGnInit = 8;
	uint8  bsStep = 4; /* Binary search initial step size */
	uint8  bsDepth = 5; /* Binary search depth */
	uint8  bsCnt;
	int16  lgI, lgQ;
	int32  cwLpGn2;
	int32  cwLpGn2_min = 8192, cwLpGn2_max = 16384;
	uint8  num_symbols4lpgn;
	int32 volt_start, volt_end;
	uint8 counter = 0;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) &&
		(FALSE == sslpnphy_specific->peak_current_mode)) {
		rxGnInit = 4;
		bsStep = 2;
		bsDepth = 4;
		cwLpGn2_min = 12288;
		cwLpGn2_max = 24576;
	}

	for (bsCnt = 0; bsCnt < bsDepth; bsCnt++) {
		if (rxGnInit > 15)
			rxGnInit = 15; /* out-of-range correction */

		wlc_sslpnphy_set_rx_gain_by_distribution(pi, (uint16)rxGnInit, 0, 0, 0, 0, 0, 0);

		num_symbols4lpgn = 90;
		/* Boarb type SDNA and PAPD peak current mode on or not a SDNA board */
		if (((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) &&
			(VBAT_RIPPLE_CHECK(pi))) ||
			(!(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID))) {
			counter = 0;
			do {
				if (counter >= 5)
					break;
				volt_start = wlc_sslpnphy_vbatsense(pi);
				wlc_sslpnphy_papd_cal_core(pi, cal_type,
					TRUE,
					num_symbols4lpgn,
					1,
					1400,
					16640,
					0,
					128,
					0);
				volt_end = wlc_sslpnphy_vbatsense(pi);
				if ((volt_start < sslpnphy_specific->sslpnphy_volt_winner) ||
					(volt_end < sslpnphy_specific->sslpnphy_volt_winner))
					counter ++;
			} while ((volt_start < sslpnphy_specific->sslpnphy_volt_winner) ||
				(volt_end < sslpnphy_specific->sslpnphy_volt_winner));
		} else {
			num_symbols4lpgn = 15;
			wlc_sslpnphy_papd_cal_core(pi, cal_type,
				TRUE,
				num_symbols4lpgn,
				1,
				1400,
				16900,
				0,
				128,
				0);
		}
		if (cal_type == SSLPNPHY_PAPD_CAL_CW)
		{
			lgI = ((int16) phy_reg_read(pi, SSLPNPHY_papd_loop_gain_cw_i)) << 6;
			lgI = lgI >> 6;
			lgQ = ((int16) phy_reg_read(pi, SSLPNPHY_papd_loop_gain_cw_q)) << 6;
			lgQ = lgQ >> 6;
			cwLpGn2 = (lgI * lgI) + (lgQ * lgQ);

			if ((FALSE == sslpnphy_specific->peak_current_mode) && (cwLpGn2 > 0)) {
				counter = 0;
				do {
					if (cwLpGn2 > cwLpGn2_max) {
						rxGnInit++;
						cwLpGn2 >>= 1;
					} else if (cwLpGn2 < cwLpGn2_min) {
						rxGnInit--;
						cwLpGn2 <<= 1;
					}
					counter++;
					if ((cwLpGn2 <= cwLpGn2_max) && (cwLpGn2 >= cwLpGn2_min))
						break; /* break out of while loop */
				} while (counter < 15);
				if ((cwLpGn2 <= cwLpGn2_max) && (cwLpGn2 >= cwLpGn2_min)) {
					break; /* break out of for loop */
				}
			} else {
			if (cwLpGn2 < cwLpGn2_min) {
				rxGnInit = rxGnInit - bsStep;
				if (bsCnt == 4)
					rxGnInit = rxGnInit - 1;
			} else if (cwLpGn2 >= cwLpGn2_max) {
				rxGnInit = rxGnInit + bsStep;
			} else {
				break;
			}
			}
#ifndef SSLPNPHY_PAPD_OFDM
		}
#else
		} else {
			int32 lgLow, lgHigh;
			int32 ofdmLpGnDb, ofdmLpGnDbTgt = 0;

			/* is this correct ? */
			lgLow = (uint32)PHY_REG_READ(pi, SSLPNPHY, papd_ofdm_loop_gain_offset_15_0,
				papd_ofdm_loop_gain_offset_15_0);
			if (lgLow < 0)
				lgLow = lgLow + 65536;

			lgHigh = (int16)PHY_REG_READ(pi, SSLPNPHY, papd_ofdm_loop_gain_offset_18_16,
				papd_ofdm_loop_gain_offset_18_16);

			ofdmLpGnDb = lgHigh*65536 + lgLow;
			if (ofdmLpGnDb < ofdmLpGnDbTgt) {
				rxGnInit = rxGnInit - bsStep;
				if (bsCnt == 4)
					rxGnInit = rxGnInit - 1;
			} else {
				rxGnInit = rxGnInit + bsStep;
			}

		}
#endif /* SSLPNPHY_PAPD_OFDM */
		bsStep = bsStep >> 1;
	}
	if (rxGnInit < 0)
		rxGnInit = 0; /* out-of-range correction */

	sslpnphy_specific->sslpnphy_papdRxGnIdx = rxGnInit;
	return rxGnInit;
}

static void
wlc_sslpnphy_afe_clk_init(phy_info_t *pi, uint8 mode)
{
	uint8 phybw40 = IS40MHZ(pi);

	if (0) {
		/* Option 1 : IQ SWAP @ ADC OUTPUT */
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, adcsync, flip_adcsyncoutiq, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, adcsync, flip_adcsyncoutvlds, 1)
		PHY_REG_LIST_EXECUTE(pi);
	}

	if (1) {
		if (!NORADIO_ENAB(pi->pubpi)) {
			/* Option 2 : NO IQ SWAP for QT @ ADC INPUT */
			PHY_REG_MOD(pi, SSLPNPHY, rxfe, swap_rxfiltout_iq, 1);
		} else {
			/* Option 2 : IQ SWAP @ ADC INPUT */
			PHY_REG_MOD(pi, SSLPNPHY, rxfe, swap_rxfiltout_iq, 0);
		}
	}

	if (!mode && (phybw40 == 1)) {
		PHY_REG_WRITE(pi, SSLPNPHY, adc_2x, 0);
	} else {
		/* Setting adc in 2x mode */
		PHY_REG_WRITE(pi, SSLPNPHY, adc_2x, 0x7);
	}

#ifdef SSLPNLOWPOWER
	if (!mode && (phybw40 == 0)) {
		PHY_REG_WRITE(pi, SSLPNPHY, adc_2x, 0x7);
	} else {
		/* Setting adc in 1x mode */
		PHY_REG_WRITE(pi, SSLPNPHY, adc_2x, 0x0);
	}
#endif
	/* Selecting pos-edge of dac clock for driving the samples to dac */
	PHY_REG_MOD(pi, SSLPNPHY, sslpnCtrl4, flip_dacclk_edge, 0);

	/* Selecting neg-edge of adc clock for sampling the samples from adc (in adc-presync),
	 * to meet timing
	*/
	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		PHY_REG_MOD2(pi, SSLPNPHY, sslpnCtrl4, flip_adcclk2x_edge, flip_adcclk1x_edge, 1, 1);
	} else {
		PHY_REG_MOD2(pi, SSLPNPHY, sslpnCtrl4, flip_adcclk2x_edge, flip_adcclk1x_edge, 0, 0);
	}

	PHY_REG_LIST_START
		/* Selecting pos-edge of 80Mhz phy clock for sampling the samples
		* from adc (in adc-presync)
		*/
		PHY_REG_MOD2_ENTRY(SSLPNPHY, sslpnCtrl4, flip_adcclk2x_80_edge, flip_adcclk1x_80_edge, 0, 0)
		/* Selecting pos-edge of aux-adc clock, 80Mhz phy clock for sampling the samples
		* from aux adc (in auxadc-presync)
		*/
		PHY_REG_MOD3_ENTRY(SSLPNPHY, sslpnCtrl4,
			flip_auxadcclk_edge, flip_auxadcclkout_edge, flip_auxadcclk80_edge,
			0, 0, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Setting the adc-presync mux to select the samples registered with adc-clock */
	PHY_REG_MOD2(pi, SSLPNPHY, sslpnAdcCtrl, sslpnAdcCtrlMuxAdc2x, sslpnAdcCtrlMuxAdc1x, mode, mode);

	 /* Setting the auxadc-presync muxes to select
	  * the samples registered with auxadc-clockout
	  */
	PHY_REG_MOD3(pi, SSLPNPHY, sslpnAuxAdcCtrl,
		sslpnAuxAdcMuxCtrl0, sslpnAuxAdcMuxCtrl1, sslpnAuxAdcMuxCtrl2,
		0, 1, 1);

	wlc_sslpnphy_reset_auxadc(pi);

	wlc_sslpnphy_toggle_afe_pwdn(pi);
}


STATIC void
InitIntpapdlut(uint8 Max, uint8 Min, uint8 *papdIntlutVld)
{
	uint16 a;

	for (a = Min; a <= Max; a++) {
		papdIntlutVld[a] = 0;
	}
}

STATIC void
BCMROMFN(wlc_sslpnphy_saveIntpapdlut)(phy_info_t *pi, int8 Max,
	int8 Min, uint32 *papdIntlut, uint8 *papdIntlutVld)
{
	phytbl_info_t tab;
	uint16 a;

	tab.tbl_id = SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL;
	tab.tbl_width = 32;     /* 32 bit wide */

	/* Max should be in range of 0 to 127 */
	/* Min should be in range of 0 to 126 */
	/* else no updates are available */

	if ((Min < 64) && (Max >= 0)) {
		Max = Max * 2 + 1;
		Min = Min * 2;

		tab.tbl_ptr = papdIntlut + Min; /* ptr to buf */
		tab.tbl_len = Max - Min + 1;        /* # values   */
		tab.tbl_offset = Min; /* tbl offset */
		wlc_sslpnphy_read_table(pi, &tab);

		for (a = Min; a <= Max; a++) {
			papdIntlutVld[a] = 1;
		}
	}
}

STATIC void
BCMROMFN(wlc_sslpnphy_GetpapdMaxMinIdxupdt)(phy_info_t *pi,
	int8 *maxUpdtIdx,
	int8 *minUpdtIdx)
{
	uint16 papd_lut_index_updt_63_48, papd_lut_index_updt_47_32;
	uint16 papd_lut_index_updt_31_16, papd_lut_index_updt_15_0;
	int8 MaxIdx, MinIdx;
	uint8 MaxIdxUpdated, MinIdxUpdated;
	uint8 i;

	papd_lut_index_updt_63_48 = phy_reg_read(pi, SSLPNPHY_papd_lut_index_updated_63_48);
	papd_lut_index_updt_47_32 = phy_reg_read(pi, SSLPNPHY_papd_lut_index_updated_47_32);
	papd_lut_index_updt_31_16 = phy_reg_read(pi, SSLPNPHY_papd_lut_index_updated_31_16);
	papd_lut_index_updt_15_0  = phy_reg_read(pi, SSLPNPHY_papd_lut_index_updated_15_0);

	MaxIdx = 63;
	MinIdx = 0;
	MinIdxUpdated = 0;
	MaxIdxUpdated = 0;

	for (i = 0; i < 16 && MinIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_15_0 & (1 << i)) == 0) {
				if (MinIdxUpdated == 0)
					MinIdx = MinIdx + 1;
			} else {
				MinIdxUpdated = 1;
			}
	}
	for (; i < 32 && MinIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_31_16 & (1 << (i - 16))) == 0) {
				if (MinIdxUpdated == 0)
					MinIdx = MinIdx + 1;
			} else {
				MinIdxUpdated = 1;
			}
	}
	for (; i < 48 && MinIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_47_32 & (1 << (i - 32))) == 0) {
				if (MinIdxUpdated == 0)
					MinIdx = MinIdx + 1;
			} else {
				MinIdxUpdated = 1;
			}
	}
	for (; i < 64 && MinIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_63_48 & (1 << (i - 48))) == 0) {
				if (MinIdxUpdated == 0)
					MinIdx = MinIdx + 1;
			} else {
				MinIdxUpdated = 1;
			}
	}

	/* loop for getting max index updated */
	for (i = 0; i < 16 && MaxIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_63_48 & (1 << (15 - i))) == 0) {
				if (MaxIdxUpdated == 0)
					MaxIdx = MaxIdx - 1;
			} else {
				MaxIdxUpdated = 1;
			}
	}
	for (; i < 32 && MaxIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_47_32 & (1 << (31 - i))) == 0) {
				if (MaxIdxUpdated == 0)
					MaxIdx = MaxIdx - 1;
			} else {
				MaxIdxUpdated = 1;
			}
	}
	for (; i < 48 && MaxIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_31_16 & (1 << (47 - i))) == 0) {
				if (MaxIdxUpdated == 0)
					MaxIdx = MaxIdx - 1;
			} else {
				MaxIdxUpdated = 1;
			}
	}
	for (; i < 64 && MaxIdxUpdated == 0; i++) {
			if ((papd_lut_index_updt_15_0 & (1 << (63 - i))) == 0) {
				if (MaxIdxUpdated == 0)
					MaxIdx = MaxIdx - 1;
			} else {
				MaxIdxUpdated = 1;
			}
	}
	*maxUpdtIdx = MaxIdx;
	*minUpdtIdx = MinIdx;
}

STATIC void
wlc_sslpnphy_compute_delta(phy_info_t *pi)
{
	uint32 papdcompdeltatblval;
	uint8 b;
	uint8 present, next;
	uint32 present_comp, next_comp;
	int32 present_comp_I, present_comp_Q;
	int32 next_comp_I, next_comp_Q;
	int32 delta_I, delta_Q;

	/* Writing Deltas */
	for (b = 0; b <= 124; b = b + 2) {
		present = b + 1;
		next = b + 3;

		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, present);
		present_comp = papdcompdeltatblval;

		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, next);
		next_comp = papdcompdeltatblval;

		present_comp_I = (present_comp & 0x00fff000) << 8;
		present_comp_Q = (present_comp & 0x00000fff) << 20;

		present_comp_I = present_comp_I >> 20;
		present_comp_Q = present_comp_Q >> 20;

		next_comp_I = (next_comp & 0x00fff000) << 8;
		next_comp_Q = (next_comp & 0x00000fff) << 20;

		next_comp_I = next_comp_I >> 20;
		next_comp_Q = next_comp_Q >> 20;

		delta_I = next_comp_I - present_comp_I;
		delta_Q = next_comp_Q - present_comp_Q;

		if (delta_I > 2048)
			delta_I = 2048;
		else if (delta_I < -2048)
			delta_I = -2048;

		if (delta_Q > 2048)
			delta_Q = 2048;
		else if (delta_Q < -2048)
			delta_Q = -2048;

		papdcompdeltatblval = ((delta_I << 12) & 0xfff000) | (delta_Q & 0xfff);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, b);
	}
}

STATIC void
genpapdlut(phy_info_t *pi, uint32 *papdIntlut, uint8 *papdIntlutVld)
{
	uint32 papdcompdeltatblval;
	uint8 a;

	papdcompdeltatblval = 128 << 12;

	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
		&papdcompdeltatblval, 1, 32, 1);

	for (a = 3; a < 128; a = a + 2) {
		if (papdIntlutVld[a] == 1) {
			papdcompdeltatblval = papdIntlut[a];
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&papdcompdeltatblval, 1, 32, a);
		} else {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&papdcompdeltatblval, 1, 32, a - 2);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&papdcompdeltatblval, 1, 32, a);
		}
	}
	/* Writing Delta */
	wlc_sslpnphy_compute_delta(pi);
}
static void
wlc_sslpnphy_pre_papd_cal_setup(phy_info_t *pi, sslpnphy_txcalgains_t *txgains, bool restore);

STATIC void
wlc_sslpnphy_papd_cal(
	phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	sslpnphy_txcalgains_t *txgains,
	bool frcRxGnCtrl,
	uint16 num_symbols,
	uint8 papd_lastidx_search_mode)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint16 AphyControl_old;
	uint32 rxGnIdx;
	uint32 tmpVar;
	uint32 refTxAnGn;
	uint8 lpgn_ovr;
	uint8 peak_curr_num_symbols_th;
	uint16 bbmult_init, bbmult_step;
	int8 maxUpdtIdx, minUpdtIdx;
	uint16 LPGN_I, LPGN_Q;
	uint16 tmp;
	uint32 bbmult_init_tmp;
	uint16 rem_symb;
	int32 volt_start, volt_end;
	uint8 counter = 0;
	ASSERT((cal_type == SSLPNPHY_PAPD_CAL_CW) || (cal_type == SSLPNPHY_PAPD_CAL_OFDM));

	WL_PHYCAL(("Running papd cal, channel: %d cal type: %d\n",
		CHSPEC_CHANNEL(pi->radio_chanspec),
		cal_type));


	if (0) {
	/* Disable CRS */
	wlc_sslpnphy_set_deaf(pi);

	/* Force WLAN antenna */
	if (!NON_BT_CHIP(wlc))
		wlc_sslpnphy_btcx_override_enable(pi);
	}

	if (0) {
		/* enables phy loopback */
		AphyControl_old = phy_reg_read(pi, SSLPNPHY_AphyControlAddr);
		PHY_REG_MOD(pi, SSLPNPHY, AphyControlAddr, phyloopbackEn, 1);
	}


	wlc_sslpnphy_pre_papd_cal_setup(pi, txgains, FALSE);
	/* Do Rx Gain Control */
	wlc_sslpnphy_papd_cal_setup_cw(pi);
	rxGnIdx = wlc_sslpnphy_papd_rxGnCtrl(pi, cal_type, frcRxGnCtrl,
		sslpnphy_specific->sslpnphy_store.CurTxGain);

	/* Set Rx Gain */
	wlc_sslpnphy_set_rx_gain_by_distribution(pi, (uint16)rxGnIdx, 0, 0, 0, 0, 0, 0);

	/* clear our PAPD Compensation table */
	if (!(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) ||
		(TRUE == sslpnphy_specific->peak_current_mode))
	wlc_sslpnphy_clear_papd_comptable(pi);

	/* Do PAPD Operation */
	if (!(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) ||
		(TRUE == sslpnphy_specific->peak_current_mode))
	{
		lpgn_ovr = 0;
		peak_curr_num_symbols_th = 70;
		bbmult_init = 1400;
		bbmult_step = 16640;
		if (VBAT_RIPPLE_CHECK(pi)) {
			counter = 0;
			do {
				if (counter >= 5)
					break;
				volt_start = wlc_sslpnphy_vbatsense(pi);
				wlc_sslpnphy_papd_cal_core(pi, cal_type,
					FALSE,
					peak_curr_num_symbols_th,
					1,
					bbmult_init,
					bbmult_step,
					0,
					128,
					0);

				volt_end = wlc_sslpnphy_vbatsense(pi);
				if ((volt_start < sslpnphy_specific->sslpnphy_volt_winner) ||
					(volt_end < sslpnphy_specific->sslpnphy_volt_winner)) {
					OSL_DELAY(300);
					counter ++;
				}
			} while ((volt_start < sslpnphy_specific->sslpnphy_volt_winner) ||
				(volt_end < sslpnphy_specific->sslpnphy_volt_winner));
		} else {
			wlc_sslpnphy_papd_cal_core(pi, cal_type,
				FALSE,
				peak_curr_num_symbols_th,
				1,
				bbmult_init,
				bbmult_step,
				0,
				128,
				0);
		}

		{
			wlc_sslpnphy_GetpapdMaxMinIdxupdt(pi, &maxUpdtIdx, &minUpdtIdx);

			InitIntpapdlut(127, 0, sslpnphy_specific->sslpnphy_papdIntlutVld);
			wlc_sslpnphy_saveIntpapdlut(pi, maxUpdtIdx, minUpdtIdx,
				sslpnphy_specific->sslpnphy_papdIntlut, sslpnphy_specific->sslpnphy_papdIntlutVld);

			LPGN_I = phy_reg_read(pi, SSLPNPHY_papd_loop_gain_cw_i);
			LPGN_Q = phy_reg_read(pi, SSLPNPHY_papd_loop_gain_cw_q);

			if (papd_lastidx_search_mode == 1) {
				for (tmp = 0; tmp < 219; tmp++) {
					bbmult_init_tmp = (bbmult_init * bbmult_step) >> 14;
					if (bbmult_init_tmp >= 65535) {
						bbmult_init = 65535;
					} else {
						bbmult_init = (uint16) bbmult_init_tmp;
					}
				}
				rem_symb = 1;
			} else {
				for (tmp = 0; tmp < peak_curr_num_symbols_th; tmp++) {
					bbmult_init_tmp = (bbmult_init * bbmult_step) >> 14;
					if (bbmult_init_tmp >= 65535) {
						bbmult_init = 65535;
					} else {
						bbmult_init = (uint16) bbmult_init_tmp;
					}
				}
				rem_symb = num_symbols- peak_curr_num_symbols_th;
			}
			while (rem_symb != 0) {
				lpgn_ovr = 1;
				bbmult_init_tmp = (bbmult_init * bbmult_step) >> 14;
				if (bbmult_init_tmp >= 65535) {
					bbmult_init = 65535;
				} else {
					bbmult_init = (uint16) bbmult_init_tmp;
				}
				if (VBAT_RIPPLE_CHECK(pi)) {
					counter = 0;
					do {
						if (counter >= 5)
							break;
						volt_start = wlc_sslpnphy_vbatsense(pi);
						wlc_sslpnphy_papd_cal_core(pi, cal_type,
							FALSE,
							0,
							1,
							bbmult_init,
							bbmult_step,
							lpgn_ovr,
							LPGN_I,
							LPGN_Q);

						volt_end = wlc_sslpnphy_vbatsense(pi);
						if ((volt_start < sslpnphy_specific->sslpnphy_volt_winner) ||
							(volt_end < sslpnphy_specific->sslpnphy_volt_winner)) {
							OSL_DELAY(600);
							counter ++;
						}
					} while ((volt_start < sslpnphy_specific->sslpnphy_volt_winner) ||
						(volt_end < sslpnphy_specific->sslpnphy_volt_winner));
				} else {
					wlc_sslpnphy_papd_cal_core(pi, cal_type,
						FALSE,
						0,
						1,
						bbmult_init,
						bbmult_step,
						lpgn_ovr,
						LPGN_I,
						LPGN_Q);
				}

				wlc_sslpnphy_GetpapdMaxMinIdxupdt(pi, &maxUpdtIdx, &minUpdtIdx);
				wlc_sslpnphy_saveIntpapdlut(pi, maxUpdtIdx, minUpdtIdx,
					sslpnphy_specific->sslpnphy_papdIntlut,
				        sslpnphy_specific->sslpnphy_papdIntlutVld);
				maxUpdtIdx = 2 * maxUpdtIdx + 1;
				minUpdtIdx = 2 * minUpdtIdx;
				if (maxUpdtIdx > 0) {
				wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
					&refTxAnGn, 1, 32, maxUpdtIdx);
				}
				if (maxUpdtIdx == 127)
					break;

				rem_symb = rem_symb - 1;
			}
			genpapdlut(pi,
			           sslpnphy_specific->sslpnphy_papdIntlut,
			           sslpnphy_specific->sslpnphy_papdIntlutVld);
		}
	}

	if ((FALSE == sslpnphy_specific->peak_current_mode) &&
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID)) {
		wlc_sslpnphy_papd_cal_core(pi, cal_type,
			FALSE,
			(uint8)num_symbols,
			1,
			1400,
			16900,
			0,
			128,
			0);

		if (cal_type == SSLPNPHY_PAPD_CAL_CW) {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&tmpVar, 1, 32, 125);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&tmpVar, 1, 32, 127);

			tmpVar = 0;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&tmpVar, 1, 32, 124);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&tmpVar, 1, 32, 126);


			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&tmpVar, 1, 32, 0);
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&tmpVar, 1, 32, 3);
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&tmpVar, 1, 32, 1);
		}
	}

	WL_PHYCAL(("wl%d: %s: PAPD cal completed\n",
		GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	BCM_REFERENCE(AphyControl_old);

	wlc_sslpnphy_pre_papd_cal_setup(pi, txgains, TRUE);
}

STATIC void
wlc_sslpnphy_pre_papd_cal_setup(phy_info_t *pi,
                                sslpnphy_txcalgains_t *txgains,
                                bool restore)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint16  rf_common_02_old, rf_common_07_old;
	uint32 refTxAnGn;
#ifdef BAND5G
	uint freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
#endif
	if (!restore) {
		sslpnphy_specific->sslpnphy_store.bb_mult_old = wlc_sslpnphy_get_bbmult(pi);
		wlc_sslpnphy_tx_pu(pi, TRUE);
		wlc_sslpnphy_rx_pu(pi, TRUE);
		sslpnphy_specific->sslpnphy_store.lpfbwlut0 = phy_reg_read(pi, SSLPNPHY_lpfbwlutreg0);
		sslpnphy_specific->sslpnphy_store.lpfbwlut1 = phy_reg_read(pi, SSLPNPHY_lpfbwlutreg1);
		sslpnphy_specific->sslpnphy_store.rf_txbb_sp_3 = read_radio_reg(pi, RADIO_2063_TXBB_SP_3);
		sslpnphy_specific->sslpnphy_store.rf_pa_ctrl_14 = read_radio_reg(pi, RADIO_2063_PA_CTRL_14);
		/* Widen tx filter */
		wlc_sslpnphy_set_tx_filter_bw(pi, 5);
		sslpnphy_specific->sslpnphy_store.CurTxGain = 0; /* crk: Need to fill this correctly */
		/* Set tx gain */
		if (txgains) {
			if (txgains->useindex) {
				wlc_sslpnphy_set_tx_pwr_by_index(pi, txgains->index);
				sslpnphy_specific->sslpnphy_store.CurTxGain = txgains->index;
			} else {
				wlc_sslpnphy_set_tx_gain(pi, &txgains->gains);
			}
		}
		/* Set TR switch to transmit */
		/* wlc_sslpnphy_set_trsw_override(pi, FALSE, FALSE); */
		/* Set Rx path mux to PAPD and turn on PAPD mixer */
		sslpnphy_specific->sslpnphy_store.rxbb_ctrl2_old = read_radio_reg(pi, RADIO_2063_RXBB_CTRL_2);
		{
			int aa;
#if !defined(ROMTERMPHY)
			aa = (int8)ANT_AVAIL(pi->aa2g);
#else
			aa = (int8)ANT_AVAIL(pi->sh->ant_avail_aa2g);
#endif /* PHYHAL */
			if (CHSPEC_IS2G(pi->radio_chanspec)) {
				if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) ||
				    (aa >= 2) ||
				    /* Askey case where there is -no TR switch, dedicated */
				    /* antenna for TX and RX */
				    ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) ==
				      BCM94319WLUSBN4L_SSID) && (aa == 1))) {
					mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2,
						(3 << 3), (uint8)(2 << 3));
					mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2,
						(1 << 1), (uint8)(1 << 1));
				} else {
					mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, (3 << 3),
						(uint8)(3 << 3));
				}
#ifndef BAND5G
		}
#else
			} else {
				if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) ==
					BCM94329MOTOROLA_SSID) ||
					(CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID) ||
					(freq >= 5500)) {
					mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2,
						(3 << 3), (uint8)(2 << 3));
					mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2,
						(1 << 1), (uint8)(1 << 1));
				} else {
					mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, (3 << 3),
						(uint8)(3 << 3));
				}
			}
#endif /* BAND5G */
		}
		/* turn on PAPD mixer */
		/* no overide for bit 4 & 5 */
		rf_common_02_old = read_radio_reg(pi, RADIO_2063_COMMON_02);
		rf_common_07_old = read_radio_reg(pi, RADIO_2063_COMMON_07);
		or_radio_reg(pi, RADIO_2063_COMMON_02, 0x1);
		or_radio_reg(pi, RADIO_2063_COMMON_07, 0x18);
		sslpnphy_specific->sslpnphy_store.pa_sp1_old_5_4 = (read_radio_reg(pi,
			RADIO_2063_PA_SP_1)) & (3 << 4);
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			mod_radio_reg(pi, RADIO_2063_PA_SP_1, (3 << 4), (uint8)(2 << 4));
		} else {
			mod_radio_reg(pi, RADIO_2063_PA_SP_1, (3 << 4), (uint8)(1 << 4));
		}
		write_radio_reg(pi, RADIO_2063_COMMON_02, rf_common_02_old);
		write_radio_reg(pi, RADIO_2063_COMMON_07, rf_common_07_old);
		wlc_sslpnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_PAPD);
		sslpnphy_specific->sslpnphy_store.Core1TxControl_old = phy_reg_read(pi, SSLPNPHY_Core1TxControl);
		PHY_REG_MOD3(pi, SSLPNPHY, Core1TxControl, BphyFrqBndSelect, iqImbCompEnable, loft_comp_en, 1, 1, 1);

		/* in SSLPNPHY, we need to bring SPB out of standby before using it */
		sslpnphy_specific->sslpnphy_store.sslpnCtrl3_old = phy_reg_read(pi, SSLPNPHY_sslpnCtrl3);
		PHY_REG_MOD(pi, SSLPNPHY, sslpnCtrl3, sram_stby, 0);
		sslpnphy_specific->sslpnphy_store.SSLPNPHY_sslpnCalibClkEnCtrl_old = phy_reg_read(pi,
			SSLPNPHY_sslpnCalibClkEnCtrl);
		PHY_REG_OR(pi, SSLPNPHY, sslpnCalibClkEnCtrl, 0x8f);
		/* Set PAPD reference analog gain */
		if (txgains) {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL,
			&refTxAnGn, 1, 32,
			SSLPNPHY_TX_PWR_CTRL_PWR_OFFSET + txgains->index);
		} else {
			refTxAnGn = 0;
		}

		refTxAnGn = refTxAnGn * 8;
		PHY_REG_WRITE(pi, SSLPNPHY, papd_tx_analog_gain_ref, (uint16)refTxAnGn);
		/* Turn off LNA */
		sslpnphy_specific->sslpnphy_store.rf_common_03_old = read_radio_reg(pi, RADIO_2063_COMMON_03);
		rf_common_02_old = read_radio_reg(pi, RADIO_2063_COMMON_02);
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			or_radio_reg(pi, RADIO_2063_COMMON_03, 0x18);
			or_radio_reg(pi, RADIO_2063_COMMON_02, 0x2);
			sslpnphy_specific->sslpnphy_store.rf_grx_sp_1_old = read_radio_reg(pi,
				RADIO_2063_GRX_SP_1);
			write_radio_reg(pi, RADIO_2063_GRX_SP_1, 0x1e);
			/* sslpnphy_rx_pu sets some bits which needs */
			/* to be override here for papdcal . so dont reset common_03 */
			write_radio_reg(pi, RADIO_2063_COMMON_02, rf_common_02_old);
		}

	} else {

		/* restore saved registers */
		write_radio_reg(pi, RADIO_2063_COMMON_03, sslpnphy_specific->sslpnphy_store.rf_common_03_old);
		rf_common_02_old = read_radio_reg(pi, RADIO_2063_COMMON_02);
		or_radio_reg(pi, RADIO_2063_COMMON_03, 0x18);
		or_radio_reg(pi, RADIO_2063_COMMON_02, 0x2);
		write_radio_reg(pi, RADIO_2063_GRX_SP_1, sslpnphy_specific->sslpnphy_store.rf_grx_sp_1_old);
		write_radio_reg(pi, RADIO_2063_COMMON_03, sslpnphy_specific->sslpnphy_store.rf_common_03_old);
		write_radio_reg(pi, RADIO_2063_COMMON_02, rf_common_02_old);
		PHY_REG_WRITE(pi, SSLPNPHY, lpfbwlutreg0, sslpnphy_specific->sslpnphy_store.lpfbwlut0);
		PHY_REG_WRITE(pi, SSLPNPHY, lpfbwlutreg1, sslpnphy_specific->sslpnphy_store.lpfbwlut1);
		write_radio_reg(pi, RADIO_2063_TXBB_SP_3, sslpnphy_specific->sslpnphy_store.rf_txbb_sp_3);
		write_radio_reg(pi, RADIO_2063_PA_CTRL_14, sslpnphy_specific->sslpnphy_store.rf_pa_ctrl_14);
		PHY_REG_WRITE(pi, SSLPNPHY, Core1TxControl, sslpnphy_specific->sslpnphy_store.Core1TxControl_old);
		PHY_REG_WRITE(pi, SSLPNPHY, sslpnCtrl3, sslpnphy_specific->sslpnphy_store.sslpnCtrl3_old);
		/* restore calib ctrl clk */
		/* switch on PAPD clk */
		PHY_REG_WRITE(pi, SSLPNPHY, sslpnCalibClkEnCtrl,
			sslpnphy_specific->sslpnphy_store.SSLPNPHY_sslpnCalibClkEnCtrl_old);
		wlc_sslpnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_TXRX2X);
		/* TR switch */
		wlc_sslpnphy_clear_trsw_override(pi);
		/* Restore rx path mux and turn off PAPD mixer */
		rf_common_02_old = read_radio_reg(pi, RADIO_2063_COMMON_02);
		rf_common_07_old = read_radio_reg(pi, RADIO_2063_COMMON_07);
		or_radio_reg(pi, RADIO_2063_COMMON_02, 0x1);
		or_radio_reg(pi, RADIO_2063_COMMON_07, 0x18);
		mod_radio_reg(pi, RADIO_2063_PA_SP_1, (3 << 4), sslpnphy_specific->sslpnphy_store.pa_sp1_old_5_4);
		write_radio_reg(pi, RADIO_2063_COMMON_02, rf_common_02_old);
		write_radio_reg(pi, RADIO_2063_COMMON_07, rf_common_07_old);
		write_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, sslpnphy_specific->sslpnphy_store.rxbb_ctrl2_old);
		/* Clear rx PU override */
		PHY_REG_MOD(pi, SSLPNPHY, RFOverride0, internalrfrxpu_ovr, 0);
		wlc_sslpnphy_rx_pu(pi, FALSE);
		wlc_sslpnphy_tx_pu(pi, FALSE);
		/* Clear rx gain override */
		wlc_sslpnphy_rx_gain_override_enable(pi, FALSE);
		/* Clear ADC override */
		PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvr, pwdn_adc_ovr, 0);
		/* restore bbmult */
		wlc_sslpnphy_set_bbmult(pi, sslpnphy_specific->sslpnphy_store.bb_mult_old);

	}
}

STATIC void
wlc_sslpnphy_vbatsense_papd_cal(
	phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	sslpnphy_txcalgains_t *txgains)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	int32 cnt;
	int32 volt_avg;
	int32 voltage_samples[50];
	int32 volt_high_thresh, volt_low_thresh;

	wlc_sslpnphy_pre_papd_cal_setup(pi, txgains, FALSE);

	{
		volt_avg = 0;

		for (cnt = 0; cnt < 32; cnt++) {
			voltage_samples[cnt] = wlc_sslpnphy_vbatsense(pi);
			volt_avg += voltage_samples[cnt];
			OSL_DELAY(120);
			/* assuming a 100us time for executing wlc_sslpnphy_vbatsense */
		}
		volt_avg = volt_avg >> 5;

		volt_high_thresh = 0;
		volt_low_thresh = volt_avg;
		for (cnt = 0; cnt < 32; cnt++) {
			if (voltage_samples[cnt] > volt_high_thresh)
				volt_high_thresh = voltage_samples[cnt];
			if (voltage_samples[cnt] < volt_low_thresh)
				volt_low_thresh = voltage_samples[cnt];
		}
		/* for taking care of vhat dip conditions */

		sslpnphy_specific->sslpnphy_volt_low = (uint8)volt_low_thresh;
		sslpnphy_specific->sslpnphy_volt_winner = (uint8)(volt_high_thresh - 2);

	}


	sslpnphy_specific->sslpnphy_last_cal_voltage = volt_low_thresh;
	wlc_sslpnphy_pre_papd_cal_setup(pi, txgains, TRUE);
}

STATIC int8
wlc_sslpnphy_gain_based_psat_detect(phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type, bool frcRxGnCtrl,
	sslpnphy_txcalgains_t *txgains,	uint8 cur_pwr)
{
	phytbl_info_t tab;
	uint8 papd_lastidx_search_mode = 0;
	int32 Re_div_Im = 60000;
	int32 lowest_gain_diff_local = 59999;
	int32 thrsh_gain = 67600;
	uint16 thrsh_pd = 180;
	int32 gain, gain_diff, psat_check_gain = 0;
	uint32 temp_offset;
	uint32 temp_read[128];
	uint32 papdcompdeltatblval;
	int32 papdcompRe, psat_check_papdcompRe = 0;
	int32 papdcompIm, psat_check_papdcompIm = 0;
	uint8 max_gain_idx = 97;
	uint8 psat_thrsh_num, psat_detected = 0;
	uint8 papdlut_endidx = 97;
	uint8 cur_index = txgains->index;
	uint freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	tab.tbl_id = SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL;
	tab.tbl_ptr = temp_read;  /* ptr to buf */
	tab.tbl_width = 32;     /* 32 bit wide */
	tab.tbl_len = 87;        /* # values   */
	tab.tbl_offset = 11;

	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		tab.tbl_len = 47;        /* # values   */
		tab.tbl_offset = 81;
		papdlut_endidx = 127;
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2))
			thrsh_gain = 25600;
		else
			thrsh_gain = 40000;
		thrsh_pd = 350;
	}

	if (CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) {
		max_gain_idx = 65;
		tab.tbl_len = 47;        /* # values   */
		tab.tbl_offset = 81;
		papdlut_endidx = 127;
		thrsh_gain = 36100;
		thrsh_pd = 350;
	}
	wlc_sslpnphy_papd_cal(pi, cal_type, txgains,
		frcRxGnCtrl, 219,
		papd_lastidx_search_mode);
	wlc_sslpnphy_read_table(pi, &tab);
	for (temp_offset = 0; temp_offset < tab.tbl_len; temp_offset += 2) {
		papdcompdeltatblval = temp_read[temp_offset];
		papdcompRe = (papdcompdeltatblval & 0x00fff000) << 8;
		papdcompIm = (papdcompdeltatblval & 0x00000fff) << 20;
		papdcompRe = (papdcompRe >> 20);
		papdcompIm = (papdcompIm >> 20);
		gain = papdcompRe * papdcompRe + papdcompIm * papdcompIm;
		if (temp_offset == (tab.tbl_len - 1)) {
			psat_check_gain = gain;
			psat_check_papdcompRe = papdcompRe;
			psat_check_papdcompIm = papdcompIm;
		}
		gain_diff = gain - thrsh_gain;
		if (gain_diff < 0) {
			gain_diff = gain_diff * (-1);
		}
		if ((gain_diff < lowest_gain_diff_local) || (temp_offset == 0)) {
			sslpnphy_specific->sslpnphy_max_gain = gain;
			max_gain_idx = tab.tbl_offset + temp_offset;
			lowest_gain_diff_local = gain_diff;
		}
	}
	/* Psat Calculation based on gain threshold */
	if (psat_check_gain >= thrsh_gain)
		psat_detected = 1;
	if (psat_detected == 0) {
		/* Psat Calculation based on PD threshold */
		if (psat_check_papdcompIm != 0) {
			if (psat_check_papdcompIm < 0)
				psat_check_papdcompIm = psat_check_papdcompIm * -1;
			Re_div_Im = (psat_check_papdcompRe / psat_check_papdcompIm) * 100;
		} else {
			Re_div_Im = 60000;
		}
		if (Re_div_Im < thrsh_pd)
			psat_detected = 1;
	}
	if ((psat_detected == 0) && (cur_index <= 4)) {
		psat_detected = 1;
	}
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID)
			|| (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)) { /* ninja */
			max_gain_idx = max_gain_idx + 16;
		} else {
			if (freq < 5640)
				max_gain_idx = max_gain_idx + 6;
			else if (freq <= 5825)
				max_gain_idx = max_gain_idx + 10;
		}
	}
	if (psat_detected) {
		sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr;
		sslpnphy_specific->sslpnphy_psat_indx = cur_index;
		psat_thrsh_num = (papdlut_endidx - max_gain_idx)/ 2;
		if (psat_thrsh_num > 6) {
			sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr - 2;
			sslpnphy_specific->sslpnphy_psat_indx = cur_index + 8;
		} else if (psat_thrsh_num > 2) {
			sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr - 1;
			sslpnphy_specific->sslpnphy_psat_indx = cur_index + 4;
		}
	}

	if ((lowest_gain_diff_local < sslpnphy_specific->sslpnphy_lowest_gain_diff) || (cur_pwr == 17)) {
		if (CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) {
			sslpnphy_specific->sslpnphy_final_papd_cal_idx = txgains->index +
				(papdlut_endidx - max_gain_idx) * 3 / 8;
		} else {
			sslpnphy_specific->sslpnphy_final_papd_cal_idx = txgains->index +
				(papdlut_endidx - max_gain_idx)/2;
		}
		sslpnphy_specific->sslpnphy_lowest_gain_diff = lowest_gain_diff_local;
	}
	return psat_detected;
}

STATIC void
wlc_sslpnphy_min_pd_search(phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl,
	sslpnphy_txcalgains_t *txgains)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint8 papd_lastidx_search_mode = 0;
	int32 Re_div_Im = 60000;
	int32 lowest_Re_div_Im_local = 59999;
	int32 temp_offset;
	uint32 temp_read[30];
	uint32 papdcompdeltatblval;
	int32 papdcompRe;
	int32 papdcompIm;
	uint8 MinPdIdx = 127;
	uint8 tbl_offset = 101;
	wlc_sslpnphy_papd_cal(pi, cal_type, txgains,
		frcRxGnCtrl, 219,
		papd_lastidx_search_mode);
	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
		temp_read, 27, 32, 101);
	for (temp_offset = 0; temp_offset < 27; temp_offset += 2) {
		papdcompdeltatblval = temp_read[temp_offset];
		papdcompRe = (papdcompdeltatblval & 0x00fff000) << 8;
		papdcompIm = (papdcompdeltatblval & 0x00000fff) << 20;
		papdcompRe = papdcompRe >> 20;
		papdcompIm = papdcompIm >> 20;
		if (papdcompIm < 0) {
			Re_div_Im = papdcompRe * 100 / papdcompIm * -1;
			if (Re_div_Im < lowest_Re_div_Im_local) {
				lowest_Re_div_Im_local = Re_div_Im;
				MinPdIdx = tbl_offset + temp_offset;
			}
		}
	}
	if (!sslpnphy_specific->sslpnphy_force_1_idxcal) {
		if (lowest_Re_div_Im_local < sslpnphy_specific->sslpnphy_lowest_Re_div_Im) {
			sslpnphy_specific->sslpnphy_final_papd_cal_idx = txgains->index + (127 - MinPdIdx)/2;
			sslpnphy_specific->sslpnphy_lowest_Re_div_Im = lowest_Re_div_Im_local;
		}
	} else {
		sslpnphy_specific->sslpnphy_final_papd_cal_idx = (uint8)sslpnphy_specific->sslpnphy_papd_nxt_cal_idx;
		sslpnphy_specific->sslpnphy_lowest_Re_div_Im = lowest_Re_div_Im_local;
	}
}

STATIC int8
wlc_sslpnphy_psat_detect(phy_info_t *pi,
	uint8 cur_index,
	uint8 cur_pwr)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint8 pd_ph_cnt = 0;
	int8 psat_detected = 0;
	uint32 temp_read[50];
	int32 temp_offset;
	uint32 papdcompdeltatblval;
	int32 papdcompRe;
	int32 papdcompIm;
	int32 voltage;
	bool gain_psat_det_in_phase_papd = 0;
	int32 thrsh_gain = 40000;
	int32 gain;
	uint thrsh1, thrsh2, pd_thresh = 0;
	uint gain_xcd_cnt = 0;
	uint32 num_elements = 0;
	voltage = wlc_sslpnphy_vbatsense(pi);
	if (voltage < 52)
		num_elements = 17;
	else
		num_elements = 39;
	thrsh1 = thrsh2 = 0;
	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL, temp_read,
		num_elements, 32, (128 - num_elements));
	if (voltage < 52)
		num_elements = 17;
	else
		num_elements = 39;
	num_elements = num_elements -1;
	for (temp_offset = num_elements; temp_offset >= 0; temp_offset -= 2) {
		papdcompdeltatblval = temp_read[temp_offset];
		papdcompRe = (papdcompdeltatblval & 0x00fff000) << 8;
		papdcompIm = (papdcompdeltatblval & 0x00000fff) << 20;
		papdcompRe = papdcompRe >> 20;
		papdcompIm = papdcompIm >> 20;
		if (papdcompIm >= 0) {
			pd_ph_cnt ++;
			psat_detected = 1;
		}
		gain = (papdcompRe * papdcompRe) + (papdcompIm * papdcompIm);
		if (gain > thrsh_gain) {
			gain_xcd_cnt ++;
			psat_detected = 1;
		}

	}
	if (cur_pwr <= 21)
		pd_thresh = 12;
	if (cur_pwr <= 19)
		pd_thresh = 10;
	if (cur_pwr <= 17)
		pd_thresh = 8;
	if ((voltage > 52) && (cur_pwr >= 17) && (psat_detected == 1)) {
		gain_psat_det_in_phase_papd = 1;
		if (cur_pwr == 21) {
			thrsh1 = 8;
			thrsh2 = 12;
		} else {
			thrsh1 = 2;
			thrsh2 =  5;
		}
	}


	if (psat_detected == 1) {

		if (gain_psat_det_in_phase_papd == 0) {
			sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr;
			sslpnphy_specific->sslpnphy_psat_indx = cur_index;
			if (pd_ph_cnt > 2) {
				sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr - 1;
				sslpnphy_specific->sslpnphy_psat_indx = cur_index + 4;
			}
			if (pd_ph_cnt > 6) {
				sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr - 2;
				sslpnphy_specific->sslpnphy_psat_indx = cur_index + 8;
			}
		} else {
			if ((gain_xcd_cnt >  0) || (pd_ph_cnt > pd_thresh))  {
				sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr;
				sslpnphy_specific->sslpnphy_psat_indx = cur_index;
			} else {
				psat_detected = 0;
			}
			if ((gain_xcd_cnt >  thrsh1) || (pd_ph_cnt > (pd_thresh + 2))) {
				sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr - 1;
				sslpnphy_specific->sslpnphy_psat_indx = cur_index + 4;
			}
			if ((gain_xcd_cnt >  thrsh2) || (pd_ph_cnt > (pd_thresh + 6))) {
				sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr - 2;
				sslpnphy_specific->sslpnphy_psat_indx = cur_index + 8;
			}

		}
	}
	if ((psat_detected == 0) && (cur_index <= 4)) {
		psat_detected = 1;
		sslpnphy_specific->sslpnphy_psat_pwr = cur_pwr;
		sslpnphy_specific->sslpnphy_psat_indx = cur_index;
	}
	return (psat_detected);
}


/* Run PAPD cal at power level appropriate for tx gain table */
STATIC void
wlc_sslpnphy_papd_cal_txpwr(phy_info_t *pi,
	sslpnphy_papd_cal_type_t cal_type,
	bool frcRxGnCtrl,
	bool frcTxGnCtrl,
	uint16 frcTxidx)
{
	sslpnphy_txcalgains_t txgains = {{0, 0, 0, 0}, 0, 0};
	bool tx_gain_override_old;
	sslpnphy_txgains_t old_gains = {0, 0, 0, 0};

	uint8 bbmult_old;
	uint16 tx_pwr_ctrl_old;
	uint8 papd_lastidx_search_mode = 0;
	uint8 psat_detected = 0;
	uint8 psat_pwr = 255;
	uint8 TxIdx_14;
	int32 lowest_Re_div_Im;
	uint8 flag = 0;  /* keeps track of upto what dbm can the papd calib be done. */
	uint8 papd_gain_based = 0;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));

	if ((CHSPEC_IS5G(pi->radio_chanspec)) && (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA))
		return;

	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319LCUSBSDN4L_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319LCSDN4L_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDELNA6L_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319MLAP_SSID) ||
		(CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID))
		papd_gain_based = 1;

	/* Initial gain based scheme enabled only for 4319 now */
	/* Verify if 4319 5G performance improve with new cal for class A operation */
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec))
		papd_gain_based = 1;
	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDELNA6L_SSID) ||
	    (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319MLAP_SSID)) {
		papd_gain_based = 1;
	}
#endif

	sslpnphy_specific->sslpnphy_lowest_Re_div_Im = 60000;
	sslpnphy_specific->sslpnphy_final_papd_cal_idx = 30;
	sslpnphy_specific->sslpnphy_psat_indx = 255;
	if (!sslpnphy_specific->sslpnphy_force_1_idxcal)
		sslpnphy_specific->sslpnphy_psat_pwr = 25;

	/* Save current bbMult and txPwrCtrl settings and turn txPwrCtrl off. */
	bbmult_old  = wlc_sslpnphy_get_bbmult(pi);

	/* Save original tx power control mode */
	tx_pwr_ctrl_old = wlc_sslpnphy_get_tx_pwr_ctrl(pi);

	/* Save old tx gains if needed */
	tx_gain_override_old = wlc_sslpnphy_tx_gain_override_enabled(pi);
	if (tx_gain_override_old)
		wlc_sslpnphy_get_tx_gain(pi, &old_gains);

	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);
	txgains.useindex = TRUE;
	if (!sslpnphy_specific->sslpnphy_force_1_idxcal) {

		/* If radio is tuned with class A settings, go for AM-AM based papd cals */
		if (!sslpnphy_specific->sslpnphy_radio_classA) {
			if (frcTxGnCtrl)
				txgains.index = (uint8) frcTxidx;
			TxIdx_14 = txgains.index;
	#ifdef BAND5G
			if (CHSPEC_IS5G(pi->radio_chanspec)) {
				if (freq <= 5320)
					TxIdx_14 = 30;
				else
					TxIdx_14 = 45;

				if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) ||
					(CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)) { /* ninja */
					TxIdx_14 = txgains.index + 20;
				}
			}
	#endif
			if (TxIdx_14 <= 0)
				flag = 13;
			else if (TxIdx_14 <= 28)
				flag = 14 + (int) ((TxIdx_14-1) >> 2);
			else if (TxIdx_14 > 28)
				flag = 21;

			/* Cal at 17dBm */
			if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) != BCM94319WLUSBN4L_SSID) {
				if (flag >= 17) {
					txgains.index = TxIdx_14 - 12;
					if (papd_gain_based) {
						psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
							cal_type, FALSE, &txgains, 17);
					} else
					{
					wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
					psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 17);
					}
					/* Calib for 18dbm */
					if ((psat_detected == 0) && (flag == 18)) {
						txgains.index = TxIdx_14 - 16;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 18);
						} else
						{
						wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
						psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 18);
						}
					}
					/* Calib for 19dbm */
					if ((psat_detected == 0) && (flag >= 19)) {
						txgains.index = TxIdx_14 - 20;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 19);
						} else
						{
						wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
						psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 19);
						}
						/* Calib for 20dbm */
						if ((psat_detected == 0) && (flag == 20)) {
							txgains.index = TxIdx_14 - 24;
							if (papd_gain_based) {
								psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
									cal_type, FALSE, &txgains, 20);
							} else
							{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 20);
							}
						}
						/* Calib for 21dBm */
						if (psat_detected == 0 && flag >= 21) {
							txgains.index = TxIdx_14 - 28;
							if (papd_gain_based) {
								psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
									cal_type, FALSE, &txgains, 21);
							} else
							{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 21);
							}
						}
					} else {
						/* Calib for 13dBm */
						if ((psat_detected == 1) && (flag >= 13)) {
							txgains.index = TxIdx_14 + 4;
							if (papd_gain_based) {
								psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
									cal_type, FALSE, &txgains, 13);
							} else
							{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 13);
							}
						}
						/* Calib for 14dBm */
						if ((psat_detected == 0) && (flag == 14)) {
							txgains.index = TxIdx_14;
							if (papd_gain_based) {
								psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
									cal_type, FALSE, &txgains, 14);
							} else
							{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 14);
							}
						}
						/* Calib for 15dBm */
						if ((psat_detected == 0) && (flag >= 15)) {
							txgains.index = TxIdx_14 - 4;
							if (papd_gain_based) {
								psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
									cal_type, FALSE, &txgains, 15);
							} else
							{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 15);
							}
						}
					}
				} else {
					/* Calib for 13dBm */
					if ((flag >= 13) && (psat_detected == 0)) {
						if (TxIdx_14 < 2)
							txgains.index = TxIdx_14 + 1;
						else
							txgains.index = TxIdx_14 + 4;
						sslpnphy_specific->sslpnphy_psat_pwr = 13;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 13);
						} else
						{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 13);
						}
					}
					/* Calib for 14dBm */
					if ((flag >= 14) && (psat_detected == 0)) {
						txgains.index = TxIdx_14;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 14);
						} else
						{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 14);
						}
					}
					/* Calib for 15dBm */
					if ((flag >= 15) && (psat_detected == 0)) {
						txgains.index = TxIdx_14 - 4;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 15);
						} else
						{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 15);
						}
					}
					/* Calib for 16dBm */
					if ((flag == 16) && (psat_detected == 0)) {
						txgains.index = TxIdx_14 - 8;
						if (papd_gain_based) {
							psat_detected = wlc_sslpnphy_gain_based_psat_detect(pi,
								cal_type, FALSE, &txgains, 16);
						} else
						{
							wlc_sslpnphy_min_pd_search(pi, cal_type, FALSE, &txgains);
							psat_detected = wlc_sslpnphy_psat_detect(pi, txgains.index, 16);
						}
					}
				}
				/* Final PAPD Cal with selected Tx Gain */
				txgains.index =  sslpnphy_specific->sslpnphy_final_papd_cal_idx;
			} else {
				txgains.index = TxIdx_14 - 12;
			}
			wlc_sslpnphy_papd_cal(pi, cal_type, &txgains,
				frcRxGnCtrl, 219,
				papd_lastidx_search_mode);

		} else { /* sslpnphy_specific->sslpnphy_radio_classA */

			if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID)) {
				uint16 num_symbols = 219;
				txgains.index =  sslpnphy_specific->sslpnphy_start_idx;
				wlc_sslpnphy_papd_cal(pi, cal_type, &txgains,
					frcRxGnCtrl, num_symbols,
					papd_lastidx_search_mode);
			} else if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) &&
				(FALSE == sslpnphy_specific->peak_current_mode)) {
				uint16 num_symbols = 70;
				txgains.index =  sslpnphy_specific->sslpnphy_start_idx;
				wlc_sslpnphy_papd_cal(pi, cal_type, &txgains,
					frcRxGnCtrl, num_symbols,
					papd_lastidx_search_mode);
			} else {
				uint32 lastval;
				int32 lreal, limag;
				uint32 mag;
				uint32 final_idx_thresh = 32100;
				uint16 min_final_idx_thresh = 10000;
				uint8 start, stop, mid;
				final_idx_thresh = 37000; /* 1.5 */

				if (freq >= 5180)
					final_idx_thresh = 42000; /* 1.6 */

				if (sslpnphy_specific->sslpnphy_papd_tweaks_enable) {
					final_idx_thresh =
					        sslpnphy_specific->sslpnphy_papd_tweaks.final_idx_thresh;
					min_final_idx_thresh =
					        sslpnphy_specific->sslpnphy_papd_tweaks.min_final_idx_thresh;
				}

				txgains.useindex = TRUE;
				start = 0;
				stop = 90;
				while (1) {
					mid = (start + stop) / 2;
					txgains.index = mid;
					wlc_sslpnphy_papd_cal(pi, cal_type, &txgains,
						frcRxGnCtrl, 219,
						0);

					wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
						&lastval, 1, 32, 127);

					lreal = lastval & 0x00fff000;
					limag = lastval & 0x00000fff;
					lreal = lreal << 8;
					limag = limag << 20;
					lreal = lreal >> 20;
					limag = limag >> 20;

					mag = (lreal * lreal) + (limag * limag);
					if (mag <= final_idx_thresh) {
						stop = mid;
					} else {
						start = mid;
					}
					if (CHSPEC_IS2G(pi->radio_chanspec)) {
						if ((mag > (final_idx_thresh -
						            min_final_idx_thresh)) &&
						    (mag < final_idx_thresh))
							break;
					}
					if ((stop - start) < 2)
						break;
				}
			}
		}
	} else {
		sslpnphy_specific->sslpnphy_psat_indx = (uint8)sslpnphy_specific->sslpnphy_papd_nxt_cal_idx;
		txgains.index =  (uint8)sslpnphy_specific->sslpnphy_papd_nxt_cal_idx;
		psat_pwr = flag;
		wlc_sslpnphy_papd_cal(pi, cal_type, &txgains,
			frcRxGnCtrl, 219,
			papd_lastidx_search_mode);
	}
	sslpnphy_specific->sslpnphy_11n_backoff = 0;
	sslpnphy_specific->sslpnphy_lowerofdm = 0;
	sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 0;
	sslpnphy_specific->sslpnphy_cck = 0;
	/* New backoff scheme */
	psat_pwr = sslpnphy_specific->sslpnphy_psat_pwr;
	lowest_Re_div_Im = sslpnphy_specific->sslpnphy_lowest_Re_div_Im;
	/* Taking a snap shot for debugging purpose */
	sslpnphy_specific->sslpnphy_psat_pwr = psat_pwr;
	sslpnphy_specific->sslpnphy_min_phase = lowest_Re_div_Im;
	sslpnphy_specific->sslpnphy_final_idx = txgains.index;

	/* Save papd lut and regs */
	wlc_sslpnphy_save_papd_calibration_results(pi);

	/* Restore tx power and reenable tx power control */
	if (tx_gain_override_old)
		wlc_sslpnphy_set_tx_gain(pi, &old_gains);
	wlc_sslpnphy_set_bbmult(pi, bbmult_old);
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl_old);
}

/*
* Get Rx IQ Imbalance Estimate from modem
*/
bool
BCMROMFN(wlc_sslpnphy_rx_iq_est)(phy_info_t *pi,
	uint16 num_samps,
	uint8 wait_time,
	sslpnphy_iq_est_t *iq_est)
{
	int wait_count = 0;
	bool result = TRUE;
	uint8 phybw40 = IS40MHZ(pi);

	PHY_REG_LIST_START
		/* Turn on clk to Rx IQ */
		PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, iqEstClkEn, 1)
		/* Force OFDM receiver on */
		PHY_REG_MOD_ENTRY(SSLPNPHY, crsgainCtrl, APHYGatingEnable, 0)
	PHY_REG_LIST_EXECUTE(pi);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (phybw40 == 1) {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsgainCtrl_40, APHYGatingEnable, 0);
		}
	}

	PHY_REG_MOD(pi, SSLPNPHY, IQNumSampsAddress, numSamps, num_samps);
	PHY_REG_MOD(pi, SSLPNPHY, IQEnableWaitTimeAddress, waittimevalue, (uint16)wait_time);
	PHY_REG_MOD(pi, SSLPNPHY, IQEnableWaitTimeAddress, iqmode, 0);
	PHY_REG_MOD(pi, SSLPNPHY, IQEnableWaitTimeAddress, iqstart, 1);

	/* Wait for IQ estimation to complete */
	while (PHY_REG_READ(pi, SSLPNPHY, IQEnableWaitTimeAddress, iqstart)) {
		/* Check for timeout */
		if (wait_count > (10 * 500)) { /* 500 ms */
			WL_ERROR(("wl%d: %s: IQ estimation failed to complete\n",
				GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
			result = FALSE;
			goto cleanup;
		}
		OSL_DELAY(100);
		wait_count++;
	}

	/* Save results */
	iq_est->iq_prod = ((uint32)phy_reg_read(pi, SSLPNPHY_IQAccHiAddress) << 16) |
		(uint32)phy_reg_read(pi, SSLPNPHY_IQAccLoAddress);
	iq_est->i_pwr = ((uint32)phy_reg_read(pi, SSLPNPHY_IQIPWRAccHiAddress) << 16) |
		(uint32)phy_reg_read(pi, SSLPNPHY_IQIPWRAccLoAddress);
	iq_est->q_pwr = ((uint32)phy_reg_read(pi, SSLPNPHY_IQQPWRAccHiAddress) << 16) |
		(uint32)phy_reg_read(pi, SSLPNPHY_IQQPWRAccLoAddress);
	WL_NONE(("wl%d: %s: IQ estimation completed in %d us,"
		"i_pwr: %d, q_pwr: %d, iq_prod: %d\n",
		GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
		wait_count * 100, iq_est->i_pwr, iq_est->q_pwr, iq_est->iq_prod));

cleanup:
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, crsgainCtrl, APHYGatingEnable, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, iqEstClkEn, 0)
	PHY_REG_LIST_EXECUTE(pi);

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (phybw40 == 1) {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsgainCtrl_40, APHYGatingEnable, 1);
		}
	}
	return result;
}

STATIC void
wlc_sslpnphy_get_rx_iq_comp(phy_info_t *pi, uint16 *a0, uint16 *b0)
{
	*a0 = PHY_REG_READ(pi, SSLPNPHY, RxCompcoeffa0, a0);
	*b0 = PHY_REG_READ(pi, SSLPNPHY, RxCompcoeffb0, b0);
}

static void
wlc_sslpnphy_set_rx_iq_comp(phy_info_t *pi, uint16 a0, uint16 b0)
{
	/* Apply new coeffs */
	PHY_REG_MOD(pi, SSLPNPHY, RxCompcoeffa0, a0, a0);
	PHY_REG_MOD(pi, SSLPNPHY, RxCompcoeffb0, b0, b0);

	/* Fill ANT1 and MRC coeffs as well */
	PHY_REG_MOD(pi, SSLPNPHY, RxCompcoeffa1, a1, a0);
	PHY_REG_MOD(pi, SSLPNPHY, RxCompcoeffb1, b1, b0);
	PHY_REG_MOD(pi, SSLPNPHY, RxCompcoeffa2, a2, a0);
	PHY_REG_MOD(pi, SSLPNPHY, RxCompcoeffb2, b2, b0);
}

/*
* Compute Rx compensation coeffs
*   -- run IQ est and calculate compensation coefficients
*/
STATIC bool
BCMROMFN(wlc_sslpnphy_calc_rx_iq_comp)(phy_info_t *pi,  uint16 num_samps)
{
#define SSLPNPHY_MAX_RXIQ_PWR 30000000
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	sslpnphy_iq_est_t iq_est;
	bool result;
	uint16 a0_new, b0_new;
	int32  a, b, temp;
	int16  iq_nbits, qq_nbits, arsh, brsh;
	int32  iq = 0;
	uint32 ii, qq;
	uint8  band_idx;

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	bzero(&iq_est, sizeof(iq_est));

	/* Get original a0 & b0 */
	wlc_sslpnphy_get_rx_iq_comp(pi, &a0_new, &b0_new);

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, rxfe, bypass_iqcomp, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, RxIqCoeffCtrl, RxIqComp11bEn, 1)
	PHY_REG_LIST_EXECUTE(pi);

	/* Zero out comp coeffs and do "one-shot" calibration */
	wlc_sslpnphy_set_rx_iq_comp(pi, 0, 0);

	if (!(result = wlc_sslpnphy_rx_iq_est(pi, num_samps, 32, &iq_est)))
		goto cleanup;

	iq = (int32)iq_est.iq_prod;
	ii = iq_est.i_pwr;
	qq = iq_est.q_pwr;

	/* bounds check estimate info */
	if ((ii + qq) > SSLPNPHY_MAX_RXIQ_PWR) {
		WL_ERROR(("wl%d: %s: RX IQ imbalance estimate power too high\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
		result = FALSE;
		goto cleanup;
	}

	/* Calculate new coeffs */
	iq_nbits = wlc_phy_nbits(iq);
	qq_nbits = wlc_phy_nbits(qq);

	arsh = 10-(30-iq_nbits);
	if (arsh >= 0) {
		a = (-(iq << (30 - iq_nbits)) + (ii >> (1 + arsh)));
		temp = (int32) (ii >>  arsh);
		if (temp == 0) {
			WL_ERROR(("Aborting Rx IQCAL! ii=%d, arsh=%d\n", ii, arsh));
			return FALSE;
		}
	} else {
		a = (-(iq << (30 - iq_nbits)) + (ii << (-1 - arsh)));
		temp = (int32) (ii << -arsh);
		if (temp == 0) {
			WL_ERROR(("Aborting Rx IQCAL! ii=%d, arsh=%d\n", ii, arsh));
			return FALSE;
		}
	}
	a /= temp;

	brsh = qq_nbits-31+20;
	if (brsh >= 0) {
		b = (qq << (31-qq_nbits));
		temp = (int32) (ii >>  brsh);
		if (temp == 0) {
			WL_ERROR(("Aborting Rx IQCAL! ii=%d, brsh=%d\n", ii, brsh));
			return FALSE;
		}
	} else {
		b = (qq << (31-qq_nbits));
		temp = (int32) (ii << -brsh);
		if (temp == 0) {
			WL_ERROR(("Aborting Rx IQCAL! ii=%d, brsh=%d\n", ii, brsh));
			return FALSE;
		}
	}
	b /= temp;
	b -= a*a;
	b = (int32)wlc_phy_sqrt_int((uint32) b);
	b -= (1 << 10);

	a0_new = (uint16)(a & 0x3ff);
	b0_new = (uint16)(b & 0x3ff);
	/* Save calibration results */
	sslpnphy_specific->sslpnphy_cal_results[band_idx].rxiqcal_coeffa0 = a0_new;
	sslpnphy_specific->sslpnphy_cal_results[band_idx].rxiqcal_coeffb0 = b0_new;
	sslpnphy_specific->sslpnphy_cal_results[band_idx].rxiq_enable = phy_reg_read(pi, SSLPNPHY_RxIqCoeffCtrl);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].rxfe = (uint8)phy_reg_read(pi, SSLPNPHY_rxfe);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].loopback1 = (uint8)read_radio_reg(pi,
		RADIO_2063_TXRX_LOOPBACK_1);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].loopback2 = (uint8)read_radio_reg(pi,
		RADIO_2063_TXRX_LOOPBACK_2);

cleanup:
	/* Apply new coeffs */
	wlc_sslpnphy_set_rx_iq_comp(pi, a0_new, b0_new);

	return result;
}

STATIC void
BCMROMFN(wlc_sslpnphy_stop_ddfs)(phy_info_t *pi)
{
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, afe_ddfs, playoutEn, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, lpphyCtrl, afe_ddfs_en, 0)
		/* switch ddfs clock off */
		PHY_REG_AND_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, 0xffef)
	PHY_REG_LIST_EXECUTE(pi);
}

STATIC void
wlc_sslpnphy_run_ddfs(phy_info_t *pi, int i_on, int q_on,
	int incr1, int incr2, int scale_index)
{
	wlc_sslpnphy_stop_ddfs(pi);

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, afe_ddfs_pointer_init, lutPointer1Init, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, afe_ddfs_pointer_init, lutPointer2Init, 0)
	PHY_REG_LIST_EXECUTE(pi);

	PHY_REG_MOD(pi, SSLPNPHY, afe_ddfs_incr_init, lutIncr1Init, incr1);
	PHY_REG_MOD(pi, SSLPNPHY, afe_ddfs_incr_init, lutIncr2Init, incr2);

	PHY_REG_MOD(pi, SSLPNPHY, afe_ddfs, chanIEn, i_on);
	PHY_REG_MOD(pi, SSLPNPHY, afe_ddfs, chanQEn, q_on);
	PHY_REG_MOD(pi, SSLPNPHY, afe_ddfs, scaleIndex, scale_index);

	PHY_REG_LIST_START
		/* Single tone */
		PHY_REG_MOD_ENTRY(SSLPNPHY, afe_ddfs, twoToneEn, 0x0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, afe_ddfs, playoutEn, 0x1)
		/* switch ddfs clock on */
		PHY_REG_OR_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, 0x10)
		PHY_REG_MOD_ENTRY(SSLPNPHY, lpphyCtrl, afe_ddfs_en, 1)
	PHY_REG_LIST_EXECUTE(pi);
}


/*
* RX IQ Calibration
*/
bool
wlc_sslpnphy_rx_iq_cal(phy_info_t *pi, const sslpnphy_rx_iqcomp_t *iqcomp, int iqcomp_sz,
	bool use_noise, bool tx_switch, bool rx_switch, bool pa, int tx_gain_idx)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	sslpnphy_txgains_t old_gains = {0, 0, 0, 0};
	uint16 tx_pwr_ctrl;
	uint ddfs_scale;
	bool result = FALSE, tx_gain_override_old = FALSE;
#ifdef BAND5G
	uint16 papd_ctrl_old = 0;
	uint16 Core1TxControl_old = 0;
	uint16 sslpnCalibClkEnCtrl_old = 0;
#endif
#define	MAX_IQ_PWR_LMT		536870912
#define	RX_PWR_THRSH_MAX	30000000
#define	RX_PWR_THRSH_MIN	4200000

	if (iqcomp) {
		ASSERT(iqcomp_sz);

		while (iqcomp_sz--) {
			if (iqcomp[iqcomp_sz].chan == CHSPEC_CHANNEL(pi->radio_chanspec)) {
				/* Apply new coeffs */
				wlc_sslpnphy_set_rx_iq_comp(pi,
					(uint16)iqcomp[iqcomp_sz].a, (uint16)iqcomp[iqcomp_sz].b);
				result = TRUE;
				break;
			}
		}
		ASSERT(result);
		goto cal_done;
	}
	/* PA driver override PA Over ride */
	PHY_REG_LIST_START
		PHY_REG_MOD2_ENTRY(SSLPNPHY, rfoverride3, stxpadpu2g_ovr, stxpapu_ovr, 1, 1)
		PHY_REG_MOD2_ENTRY(SSLPNPHY, rfoverride3_val, stxpadpu2g_ovr_val, stxpapu_ovr_val, 0, 0)
	PHY_REG_LIST_EXECUTE(pi);

	if (use_noise) {
		tx_switch = TRUE;
		rx_switch = FALSE;
		pa = FALSE;
	}

	/* Set TR switch */
	wlc_sslpnphy_set_trsw_override(pi, tx_switch, rx_switch);

	/* turn on PA */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2val, slna_pu_ovr_val, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, slna_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rxlnaandgainctrl1ovrval, lnapuovr_Val, 0x20)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, lna_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFinputOverrideVal, wlslnapu_ovr_val, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFinputOverride, wlslnapu_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);

		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1, 0x8c);
		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2, 0);
#ifndef BAND5G
	}
#else
	} else {
		/* In A-band As Play_Tone being used,Tx-Pu Override Regs inside */
			/* that proc used To turn on Tx RF Chain. */

		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, amode_tx_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, amode_tx_pu_ovr_val, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2val, slna_pu_ovr_val, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, slna_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rxlnaandgainctrl1ovrval, lnapuovr_Val, 0x04)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, lna_pu_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFinputOverrideVal, wlslnapu_ovr_val, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, RFinputOverride, wlslnapu_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);

		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1, 0);
		write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2, 0x8c);
	}
#endif /* BAND5G */
	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			papd_ctrl_old = phy_reg_read(pi, SSLPNPHY_papd_control);
			PHY_REG_MOD(pi, SSLPNPHY, papd_control, papdCompEn, 0);
			sslpnCalibClkEnCtrl_old = phy_reg_read(pi, SSLPNPHY_sslpnCalibClkEnCtrl);
			Core1TxControl_old = phy_reg_read(pi, SSLPNPHY_Core1TxControl);
			PHY_REG_LIST_START
				PHY_REG_MOD3_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl,
					papdTxClkEn, papdFiltClkEn, papdRxClkEn, 0, 0, 0)
				PHY_REG_MOD2_ENTRY(SSLPNPHY, Core1TxControl, txcomplexfilten, txrealfilten, 0, 0)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}
#endif /* BAND5G */
	if (use_noise) {
		 wlc_sslpnphy_set_rx_gain(pi, 0x2d5d);
	} else {

		/* crk: papd ? */

		/* Save old tx gain settings */
		tx_gain_override_old = wlc_sslpnphy_tx_gain_override_enabled(pi);
		if (tx_gain_override_old) {
			wlc_sslpnphy_get_tx_gain(pi, &old_gains);
		}
		/* Apply new tx gain */
		wlc_sslpnphy_set_tx_pwr_by_index(pi, tx_gain_idx);
		wlc_sslpnphy_set_rx_gain_by_distribution(pi, 0, 0, 0, 0, 7, 3, 0);
		wlc_sslpnphy_rx_gain_override_enable(pi, TRUE);
	}

	PHY_REG_LIST_START
		/* Force ADC on */
		PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvr, pwdn_adc_ovr, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvrVal, pwdn_adc_ovr_val, 0)
		/* Force Rx on	 */
		PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, internalrfrxpu_ovr, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, internalrfrxpu_ovr_val, 1)
	PHY_REG_LIST_EXECUTE(pi);

	if (read_radio_reg(pi, RADIO_2063_TXBB_CTRL_1) == 0x10)
		ddfs_scale = 2;
	else
		ddfs_scale = 0;

	/* Run calibration */
	if (use_noise) {
		wlc_sslpnphy_set_deaf(pi);
		result = wlc_sslpnphy_calc_rx_iq_comp(pi, 0xfff0);
		wlc_sslpnphy_clear_deaf(pi);
	} else {
		int tx_idx = 80;
		uint8 tia_gain = 8, lna2_gain = 3, vga_gain = 0;
		/* Needs some patch from 5.10.56 ? */
		if (CHSPEC_IS2G(pi->radio_chanspec) && (sslpnphy_specific->sslpnphy_fabid_otp != SMIC_FAB4)) {
			wlc_sslpnphy_run_ddfs(pi, 1, 1, 5, 5, ddfs_scale);
			if (sslpnphy_specific->sslpnphy_recal)
				tia_gain = sslpnphy_specific->sslpnphy_last_cal_tia_gain;

			while (tia_gain > 0) {
			wlc_sslpnphy_set_rx_gain_by_distribution(pi, 0, 0, 0, 0, tia_gain-1, 3, 0);
				result = wlc_sslpnphy_calc_rx_iq_comp(pi, 0xffff);
				if (result)
					break;
				tia_gain--;
			}
			wlc_sslpnphy_stop_ddfs(pi);
		} else {
			int tx_idx_init, tx_idx_low_lmt;
			uint32 pwr;
			sslpnphy_iq_est_t iq_est;

			bzero(&iq_est, sizeof(iq_est));

			wlc_sslpnphy_start_tx_tone(pi, 4000, 100, 1);

			if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)
				tx_idx_init = 23;
			else
				tx_idx_init = 60;

			tx_idx = tx_idx_init;
			tx_idx_low_lmt = tx_idx_init - 28;
			if (sslpnphy_specific->sslpnphy_recal) {
				tx_idx = sslpnphy_specific->sslpnphy_last_cal_tx_idx;
				tia_gain = sslpnphy_specific->sslpnphy_last_cal_tia_gain;
				lna2_gain = sslpnphy_specific->sslpnphy_last_cal_lna2_gain;
				vga_gain = sslpnphy_specific->sslpnphy_last_cal_vga_gain;
				if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) {
					tx_idx += 8;
					if (!vga_gain) {
						if (tia_gain != 0)
							tia_gain++;
						else if (tia_gain == 0)
							lna2_gain++;
					}
				}
			}
			while ((tx_idx > tx_idx_low_lmt) && ((tx_idx - 8) > 0)) {
				tx_idx -= 8;
				wlc_sslpnphy_set_tx_pwr_by_index(pi, tx_idx);
				wlc_sslpnphy_disable_pad(pi);

				wlc_sslpnphy_set_rx_gain_by_distribution(pi, 0, 0, 0, 0, 7, 3, 0);

				if (!(wlc_sslpnphy_rx_iq_est(pi, 0xffff, 32, &iq_est)))
					break;
				pwr = iq_est.i_pwr + iq_est.q_pwr;

				if (pwr > MAX_IQ_PWR_LMT) {
					tx_idx += 40;
					if (tx_idx > 127) {
						tx_idx = 127;
						wlc_sslpnphy_set_tx_pwr_by_index(pi, tx_idx);
						wlc_sslpnphy_disable_pad(pi);

						break;
					}
				} else if (pwr > RX_PWR_THRSH_MIN) {
					break;
				}
			}
			while (((tia_gain > 0) || (lna2_gain > 1)) && (vga_gain < 10)) {
				if (!vga_gain) {
					if (tia_gain != 0)
						tia_gain--;
					else if (tia_gain == 0)
						lna2_gain--;
				}

				wlc_sslpnphy_set_rx_gain_by_distribution(pi, vga_gain, 0, 0, 0,
					tia_gain, lna2_gain, 0);

				if (!(wlc_sslpnphy_rx_iq_est(pi, 0xffff, 32, &iq_est)))
					break;
				pwr = iq_est.i_pwr + iq_est.q_pwr;

				if (pwr < RX_PWR_THRSH_MIN)
					vga_gain++;
				else if (pwr < RX_PWR_THRSH_MAX)
					break;
			}
			wlc_sslpnphy_calc_rx_iq_comp(pi, 0xffff);
			wlc_sslpnphy_stop_tx_tone(pi);
		}
		sslpnphy_specific->sslpnphy_last_cal_tx_idx = (int8) tx_idx;
		sslpnphy_specific->sslpnphy_last_cal_tia_gain = tia_gain;
		sslpnphy_specific->sslpnphy_last_cal_lna2_gain = lna2_gain;
		sslpnphy_specific->sslpnphy_last_cal_vga_gain = vga_gain;
	}

	/* Resore TR switch */
	wlc_sslpnphy_clear_trsw_override(pi);

	/* Restore PA */
	PHY_REG_LIST_START
		PHY_REG_MOD2_ENTRY(SSLPNPHY, RFOverride0, gmode_tx_pu_ovr, amode_tx_pu_ovr, 0, 0)
		PHY_REG_MOD2_ENTRY(SSLPNPHY, rfoverride3, stxpadpu2g_ovr, stxpapu_ovr, 0, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* Resore Tx gain */
	if (!use_noise) {
		if (tx_gain_override_old) {
			wlc_sslpnphy_set_tx_gain(pi, &old_gains);
		} else
			wlc_sslpnphy_disable_tx_gain_override(pi);
	}
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			PHY_REG_WRITE(pi, SSLPNPHY, papd_control, papd_ctrl_old);
			PHY_REG_WRITE(pi, SSLPNPHY, sslpnCalibClkEnCtrl, sslpnCalibClkEnCtrl_old);
			PHY_REG_WRITE(pi, SSLPNPHY, Core1TxControl, Core1TxControl_old);
		}
	}
#endif
	/* Clear various overrides */
	wlc_sslpnphy_rx_gain_override_enable(pi, FALSE);

	PHY_REG_LIST_START
		PHY_REG_MOD3_ENTRY(SSLPNPHY, rfoverride2, slna_pu_ovr, lna_pu_ovr, ps_ctrl_ovr, 0, 0, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, RFinputOverride, wlslnapu_ovr, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvr, pwdn_adc_ovr, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, internalrfrxpu_ovr, 0)
	PHY_REG_LIST_EXECUTE(pi);

cal_done:
	WL_INFORM(("wl%d: %s: Rx IQ cal complete, coeffs: A0: %d, B0: %d\n",
		GENERIC_PHY_INFO(pi)->unit, __FUNCTION__,
		(int16)PHY_REG_READ(pi, SSLPNPHY, RxCompcoeffa0, a0),
		(int16)PHY_REG_READ(pi, SSLPNPHY, RxCompcoeffb0, b0)));

	return result;
}

static int
wlc_sslpnphy_wait_phy_reg(phy_info_t *pi, uint16 addr,
    uint32 val, uint32 mask, int shift,
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

STATIC int
BCMROMFN(wlc_sslpnphy_aux_adc_accum)(phy_info_t *pi, uint32 numberOfSamples,
    uint32 waitTime, int32 *sum, int32 *prod)
{
	uint32 save_pwdn_rssi_ovr, term0, term1;
	int done;

	save_pwdn_rssi_ovr = PHY_REG_READ(pi, SSLPNPHY, AfeCtrlOvr, pwdn_rssi_ovr);

	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvr, pwdn_rssi_ovr, 1)
		PHY_REG_MOD_ENTRY(SSLPNPHY, AfeCtrlOvrVal, pwdn_rssi_ovr_val, 0)
		/* clear accumulators */
		PHY_REG_MOD_ENTRY(SSLPNPHY, auxadcCtrl, auxadcreset, 1)
		PHY_REG_MOD2_ENTRY(SSLPNPHY, auxadcCtrl, auxadcreset, rssiestStart, 0, 1)
	PHY_REG_LIST_EXECUTE(pi);

	PHY_REG_MOD(pi, SSLPNPHY, NumrssiSamples, numrssisamples, numberOfSamples);

	PHY_REG_MOD(pi, SSLPNPHY, rssiwaittime, rssiwaittimeValue, 100);
	done = wlc_sslpnphy_wait_phy_reg(pi, SSLPNPHY_auxadcCtrl, 0,
		SSLPNPHY_auxadcCtrl_rssiestStart_MASK,
		SSLPNPHY_auxadcCtrl_rssiestStart_SHIFT,
		1000);

	if (done) {
		term0 = PHY_REG_READ(pi, SSLPNPHY, rssiaccValResult0, rssiaccResult0);
		term1 = PHY_REG_READ(pi, SSLPNPHY, rssiaccValResult1, rssiaccResult1);
		*sum = (term1 << 16) + term0;

		term0 = PHY_REG_READ(pi, SSLPNPHY, rssiprodValResult0, rssiProdResult0);
		term1 = PHY_REG_READ(pi, SSLPNPHY, rssiprodValResult1, rssiProdResult1);
		*prod = (term1 << 16) + term0;
	}
	else {
		*sum = 0;
		*prod = 0;
	}

	/* restore result */
	PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvr, pwdn_rssi_ovr, save_pwdn_rssi_ovr);

	return done;
}

int32
wlc_sslpnphy_vbatsense(phy_info_t *pi)
{
	uint16 save_rssi_settings;
	uint16 save_rssiformat;
	uint16 sslpnCalibClkEnCtr;
	uint32 savemux;
	int32 sum, prod, x, voltage;
	uint32 save_reg0 = 0, save_reg5 = 0;
	uint16 save_iqcal_ctrl_2;
	uint16 num_wait;
	uint16 num_rssi;

#define numsamps 40	/* 40 samples can be accumulated in 1us timeout */
#define one_by_numsamps 26214	/* 1/40 in q.20 format */
#define qone_by_numsamps 20	/* q format of one_by_numsamps */

#define c1 (int16)((0.0580833 * (1<<19)) + 0.5)	/* polynomial coefficient in q.19 format */
#define qc1 19									/* qformat of c1 */
#define c0 (int16)((3.9591333 * (1<<13)) + 0.5) 	/* polynomial coefficient in q.14 format */
#define qc0 13									/* qformat of c0 */

	num_wait = phy_reg_read(pi, SSLPNPHY_rssiwaittime);
	num_rssi = phy_reg_read(pi, SSLPNPHY_NumrssiSamples);
	if (CHIPID(GENERIC_PHY_INFO(pi)->chip) != BCM5356_CHIP_ID) {
		save_reg0 = si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 0, 0);
		save_reg5 = si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 5, 0, 0);
		si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 1, 1);
		si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 5, (1 << 31), (1 << 31));
	}

	sslpnCalibClkEnCtr = phy_reg_read(pi, SSLPNPHY_sslpnCalibClkEnCtrl);
	PHY_REG_MOD(pi, SSLPNPHY, sslpnCalibClkEnCtrl, txFrontEndCalibClkEn, 1);

	save_rssi_settings = phy_reg_read(pi, SSLPNPHY_AfeRSSICtrl1);
	save_rssiformat = PHY_REG_READ(pi, SSLPNPHY, auxadcCtrl, rssiformatConvEn);

	PHY_REG_LIST_START
		/* set the "rssiformatConvEn" field in the auxadcCtrl to 1 */
		PHY_REG_MOD_ENTRY(SSLPNPHY, auxadcCtrl, rssiformatConvEn, 1)
		/* slpinv_rssi */
		PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_AfeRSSICtrl1, (1<<13), (0<<13))
		PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_AfeRSSICtrl1, (0xf<<0), (13<<0))
		PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_AfeRSSICtrl1, (0xf<<4), (8<<4))
		PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_AfeRSSICtrl1, (0x7<<10), (4<<10))
	PHY_REG_LIST_EXECUTE(pi);

	/* set powerdetector before PA and rssi mux to tempsense */
	savemux = PHY_REG_READ(pi, SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val);
	PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 4);

	/* set iqcal mux to select VBAT */
	save_iqcal_ctrl_2 = read_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2);
	mod_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, (0xF<<0), (0x4<<0));

	wlc_sslpnphy_reset_auxadc(pi);

	wlc_sslpnphy_aux_adc_accum(pi, numsamps, 0, &sum, &prod);

	/* restore rssi settings */
	phy_reg_write(pi, SSLPNPHY_AfeRSSICtrl1, save_rssi_settings);
	PHY_REG_MOD(pi, SSLPNPHY, auxadcCtrl, rssiformatConvEn, save_rssiformat);
	PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val, savemux);

	write_radio_reg(pi, RADIO_2063_IQCAL_CTRL_2, save_iqcal_ctrl_2);
	PHY_REG_WRITE(pi, SSLPNPHY, sslpnCalibClkEnCtrl, sslpnCalibClkEnCtr);
	/* sum = sum/numsamps in qsum=0+qone_by_numsamps format
	 *as the accumulated values are always less than 200, 6 bit values, the
	 *sum always fits into 16 bits
	 */
	x = qm_mul321616((int16)sum, one_by_numsamps);

	/* compute voltagte = c1*sum + co */
	voltage = qm_mul323216(x, c1); /* volatage in q.qone_by_numsamps+qc1-16 format */

	/* bring sum to qc0 format */
	voltage = voltage >> (qone_by_numsamps+qc1-16 - qc0);

	/* comute c1*x + c0 */
	voltage = voltage + c0;

	/* bring voltage to q.4 format */
	voltage = voltage >> (qc0 - 4);

	if (CHIPID(GENERIC_PHY_INFO(pi)->chip) != BCM5356_CHIP_ID) {
		si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 0, ~0, save_reg0);
		si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 5, ~0, save_reg5);
	}

	wlc_sslpnphy_reset_auxadc(pi);

	PHY_REG_WRITE(pi, SSLPNPHY, rssiwaittime, num_wait);
	PHY_REG_WRITE(pi, SSLPNPHY, NumrssiSamples, num_rssi);

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
wlc_sslpnphy_tempsense(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	  uint16 save_rssi_settings, save_rssiformat;
	  uint16  sslpnCalibClkEnCtr;
	  uint32 rcalvalue, savemux;
	  int32 sum0, prod0, sum1, prod1, sum;
	  int32 temp32 = 0;
	  bool suspend;
	  uint16 num_rssi;
	  uint16 num_wait;

#define m 461.5465

	  /* b = -12.0992 in q11 format */
#define b ((int16)(-12.0992*(1<<11)))
#define qb 11

	  /* thousand_by_m = 1000/m = 1000/461.5465 in q13 format */
#define thousand_by_m ((int16)((1000/m)*(1<<13)))
#define qthousand_by_m 13

#define numsamps 400	/* 40 samples can be accumulated in 1us timeout */
#define one_by_numsamps 2621	/* 1/40 in q.20 format */
#define qone_by_numsamps 20	/* q format of one_by_numsamps */

	num_wait = phy_reg_read(pi, SSLPNPHY_rssiwaittime);
	num_rssi = phy_reg_read(pi, SSLPNPHY_NumrssiSamples);
	/* suspend the mac if it is not already suspended */
	suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend)
		WL_SUSPEND_MAC_AND_WAIT(pi);

	sslpnCalibClkEnCtr = phy_reg_read(pi, SSLPNPHY_sslpnCalibClkEnCtrl);
	PHY_REG_MOD(pi, SSLPNPHY, sslpnCalibClkEnCtrl, txFrontEndCalibClkEn, 1);

	save_rssi_settings = phy_reg_read(pi, SSLPNPHY_AfeRSSICtrl1);
	save_rssiformat = PHY_REG_READ(pi, SSLPNPHY, auxadcCtrl, rssiformatConvEn);

	PHY_REG_LIST_START
		/* set the "rssiformatConvEn" field in the auxadcCtrl to 1 */
		PHY_REG_MOD_ENTRY(SSLPNPHY, auxadcCtrl, rssiformatConvEn, 1)
		/* slpinv_rssi */
		PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_AfeRSSICtrl1, (1<<13), (0<<13))
		PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_AfeRSSICtrl1, (0xf<<0), (0<<0))
		PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_AfeRSSICtrl1, (0xf<<4), (11<<4))
		PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_AfeRSSICtrl1, (0x7<<10), (5<<10))
	PHY_REG_LIST_EXECUTE(pi);

	/* read the rcal value */
	rcalvalue = read_radio_reg(pi, RADIO_2063_COMMON_13) & 0xf;

	/* set powerdetector before PA and rssi mux to tempsense */
	savemux = PHY_REG_READ(pi, SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val);

	PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val, 5);

	wlc_sslpnphy_reset_auxadc(pi);

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

	wlc_sslpnphy_aux_adc_accum(pi, numsamps, 0, &sum0, &prod0);

	wlc_sslpnphy_reset_auxadc(pi);

	/* set TPSENSE swap to 1 */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<4), (1<<4));

	wlc_sslpnphy_aux_adc_accum(pi, numsamps, 0, &sum1, &prod1);

	sum = (sum0 + sum1) >> 1;

	/* restore rssi settings */
	phy_reg_write(pi, SSLPNPHY_AfeRSSICtrl1, save_rssi_settings);
	PHY_REG_MOD(pi, SSLPNPHY, auxadcCtrl, rssiformatConvEn, save_rssiformat);
	PHY_REG_MOD(pi, SSLPNPHY, AfeCtrlOvrVal, rssi_muxsel_ovr_val, savemux);

	/* powerdown tempsense */
	mod_radio_reg(pi, RADIO_2063_TEMPSENSE_CTRL_1, (1<<6), (1<<6));

	/* sum = sum/numsamps in qsum=0+qone_by_numsamps format
	 *as the accumulated values are always less than 200, 6 bit values, the
	 *sum always fits into 16 bits
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

	PHY_REG_WRITE(pi, SSLPNPHY, sslpnCalibClkEnCtrl, sslpnCalibClkEnCtr);

	/* enable the mac if it is suspended by tempsense function */
	if (!suspend)
		WL_ENABLE_MAC(pi);

	wlc_sslpnphy_reset_auxadc(pi);

	PHY_REG_WRITE(pi, SSLPNPHY, rssiwaittime, num_wait);
	PHY_REG_WRITE(pi, SSLPNPHY, NumrssiSamples, num_rssi);

	sslpnphy_specific->sslpnphy_lastsensed_temperature = (int8)temp32;
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
static void
wlc_sslpnphy_temp_adj_offset(phy_info_t *pi, int8 temp_adj)
{
	uint32 tableBuffer[2];
	uint8 phybw40 = IS40MHZ(pi);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	/* adjust the reference ofdm gain index table offset */
	PHY_REG_MOD(pi, SSLPNPHY, gainidxoffset, ofdmgainidxtableoffset,
		sslpnphy_specific->sslpnphy_ofdmgainidxtableoffset + temp_adj);

	/* adjust the reference dsss gain index table offset */
	PHY_REG_MOD(pi, SSLPNPHY, gainidxoffset, dsssgainidxtableoffset,
		sslpnphy_specific->sslpnphy_dsssgainidxtableoffset + temp_adj);

	/* adjust the reference gain_val_tbl at index 64 and 65 in gain_val_tbl */
	tableBuffer[0] = sslpnphy_specific->sslpnphy_tr_R_gain_val + temp_adj;
	tableBuffer[1] = sslpnphy_specific->sslpnphy_tr_T_gain_val + temp_adj;

	wlc_sslpnphy_common_write_table(pi, 17, tableBuffer, 2, 32, 64);

	if (phybw40 == 0) {
		PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb,
			sslpnphy_specific->sslpnphy_input_pwr_offset_db + temp_adj);
		PHY_REG_MOD(pi, SSLPNPHY, LowGainDB, MedLowGainDB,
			sslpnphy_specific->sslpnphy_Med_Low_Gain_db + temp_adj);
		PHY_REG_MOD(pi, SSLPNPHY, VeryLowGainDB, veryLowGainDB,
			sslpnphy_specific->sslpnphy_Very_Low_Gain_db + temp_adj);
	} else {
		PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb,
			sslpnphy_specific->sslpnphy_input_pwr_offset_db + temp_adj);
		PHY_REG_MOD(pi, SSLPNPHY_Rev2, LowGainDB_40, MedLowGainDB,
			sslpnphy_specific->sslpnphy_Med_Low_Gain_db + temp_adj);
		PHY_REG_MOD(pi, SSLPNPHY_Rev2, VeryLowGainDB_40, veryLowGainDB,
			sslpnphy_specific->sslpnphy_Very_Low_Gain_db + temp_adj);
	}
}
STATIC uint8 chan_spec_spur_85degc_38p4Mhz [][3] = {
	{0, 1, 2},
	{0, 1, 22},
	{0, 1, 6},
	{0, 1, 54},
	{0, 1, 26},
	{0, 1, 38}
};
STATIC uint16 UNO_LOW_TEMP_OFDM_GAINTBL_TWEAKS [][2] = {
	{25,  0x3AC},
	{26,  0x3C3}
};
uint8 UNO_LOW_TEMP_OFDM_GAINTBL_TWEAKS_sz = ARRAYSIZE(UNO_LOW_TEMP_OFDM_GAINTBL_TWEAKS);
uint8 chan_spec_spur_85degc_38p4Mhz_sz = ARRAYSIZE(chan_spec_spur_85degc_38p4Mhz);
static void
wlc_sslpnphy_chanspec_spur_weight(phy_info_t *pi, uint channel, uint8 ptr[][3], uint8 array_size);

static void
wlc_sslpnphy_temp_adj_5356(phy_info_t *pi)
{
	int32 temperature, temp_adj;
	phytbl_info_t tab;
	uint32 tableBuffer[2];
	uint freq;
	int8 path_loss, temp1;
	uint8 phybw40;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

	phybw40 = CHSPEC_IS40(pi->radio_chanspec);

	tab.tbl_ptr = tableBuffer;	/* ptr to buf */
	tab.tbl_len = 2;			/* # values   */
	tab.tbl_id = SSLPNPHY_TBL_ID_GAIN_IDX;			/* gain_val_tbl_rev3 */
	tab.tbl_width = 32;			/* 32 bit wide */

	freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));

	temperature = sslpnphy_specific->sslpnphy_lastsensed_temperature;


	if ((temperature - 15) < -30) {
		temp_adj = 6;
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope = ((((temperature - 15) + 30) * 286) >> 12) - 4;

		tab.tbl_offset = 26;		/* tbl offset */
		wlc_sslpnphy_read_table(pi, &tab);
		tab.tbl_offset = 28;		/* tbl offset */
		wlc_sslpnphy_write_table(pi, &tab);
		wlc_sslpnphy_read_table(pi, &tab);
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6333)
				PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 27)
			PHY_REG_LIST_EXECUTE(pi);
		}
	} else if ((temperature - 15) < 15) {
		temp_adj = 3;
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope = ((((temperature - 15) - 4) * 286) >> 12) - 1;
	} else if ((temperature - 15) < 70) {
		temp_adj = 0;
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope = (((temperature - 15) - 25) * 286) >> 12;
	} else {
		temp_adj = -3;
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope = ((((temperature - 10) - 55) * 286) >> 12) - 2;

		tab.tbl_offset = 28;		/* tbl offset */
		tableBuffer[0] = sslpnphy_specific->sslpnphy_gain_idx_14_lowword;
		tableBuffer[1] = sslpnphy_specific->sslpnphy_gain_idx_14_hiword;
		wlc_sslpnphy_write_table(pi, &tab);
		wlc_sslpnphy_read_table(pi, &tab);
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
			PHY_REG_MOD(pi, SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 23);
		}
	}

	/* adjust the reference ofdm gain index table offset */
	PHY_REG_MOD(pi, SSLPNPHY, gainidxoffset, ofdmgainidxtableoffset,
		sslpnphy_specific->sslpnphy_ofdmgainidxtableoffset +  temp_adj);

	/* adjust the reference dsss gain index table offset */
	PHY_REG_MOD(pi, SSLPNPHY, gainidxoffset, dsssgainidxtableoffset,
		sslpnphy_specific->sslpnphy_dsssgainidxtableoffset +  temp_adj);

	/* adjust the reference gain_val_tbl at index 64 and 65 in gain_val_tbl */
	tab.tbl_ptr = tableBuffer;	/* ptr to buf */
	tableBuffer[0] = sslpnphy_specific->sslpnphy_tr_R_gain_val + temp_adj;
	tableBuffer[1] = sslpnphy_specific->sslpnphy_tr_T_gain_val + temp_adj;
	tab.tbl_len = 2;			/* # values   */
	tab.tbl_id = 17;			/* gain_val_tbl_rev3 */
	tab.tbl_offset = 64;		/* tbl offset */
	tab.tbl_width = 32;			/* 32 bit wide */

	wlc_sslpnphy_write_table(pi, &tab);

	if (phybw40 == 0) {
		PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb,
			sslpnphy_specific->sslpnphy_input_pwr_offset_db + temp_adj);
	} else {
		PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb,
			sslpnphy_specific->sslpnphy_input_pwr_offset_db + temp_adj);
	}

	if (((temperature) >= -15) && ((temperature) < 30)) {
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) {
			if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
				if (freq <= 2432) {
					PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 4);
				}
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 5)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, SlowPwrLoThresh, 10)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, SlowPwrLoThresh, 10)
				PHY_REG_LIST_EXECUTE(pi);
			}
		}
	} else if (((temperature) >= 65) && ((temperature) < 85)) {
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope =
			sslpnphy_specific->sslpnphy_pkteng_rssi_slope - 2;
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) {
			if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 4)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 4)
					PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 7)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, SlowPwrLoThresh, 8)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, SlowPwrLoThresh, 8)
					PHY_REG_MOD_ENTRY(SSLPNPHY, HiGainDB, HiGainDB, 73)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, HiGainDB_40, HiGainDB, 73)
				PHY_REG_LIST_EXECUTE(pi);
			}
		}
	} else if ((temperature) >= 85) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 3)
			PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 3)
		PHY_REG_LIST_EXECUTE(pi);
	}

	if (phybw40 == 0) {
		PHY_REG_MOD(pi, SSLPNPHY, LowGainDB, MedLowGainDB,
			sslpnphy_specific->sslpnphy_Med_Low_Gain_db + temp_adj);
		PHY_REG_MOD(pi, SSLPNPHY, VeryLowGainDB, veryLowGainDB,
			sslpnphy_specific->sslpnphy_Very_Low_Gain_db + temp_adj);
	} else {
		PHY_REG_MOD(pi, SSLPNPHY_Rev2, LowGainDB_40, MedLowGainDB,
			sslpnphy_specific->sslpnphy_Med_Low_Gain_db + temp_adj);
		PHY_REG_MOD(pi, SSLPNPHY_Rev2, VeryLowGainDB_40, veryLowGainDB,
			sslpnphy_specific->sslpnphy_Very_Low_Gain_db + temp_adj);
	}

#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec))
		path_loss = (int8)sslpnphy_specific->sslpnphy_rx_power_offset_5g;
	else
#endif /* BAND5G */
		path_loss = (int8)sslpnphy_specific->sslpnphy_rx_power_offset;
	if (phybw40)
		temp1 = (int8)PHY_REG_READ(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb);
	else
		temp1 = (int8)PHY_REG_READ(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb);
	temp1 -= path_loss;
	if (phybw40)
		PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, temp1);
	else
		PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, temp1);

	wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(pi);

	wlc_sslpnphy_txpower_recalc_target(pi);
}
void
wlc_sslpnphy_temp_adj(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	int32 temperature;
	uint32 tableBuffer[2];
	uint freq;
	int16 thresh1, thresh2;
	uint8 spur_weight;
	uint8 i;
	uint16 minsig = 0x01bc;
	if (CHSPEC_IS5G(pi->radio_chanspec))
		minsig = 0x0184;

	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		thresh1 = -23;
		thresh2 = 60;
	} else {
		thresh1 = -30;
		thresh2 = 55;
	}
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		wlc_sslpnphy_temp_adj_5356(pi);
		return;
	}
	temperature = sslpnphy_specific->sslpnphy_lastsensed_temperature;
	freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	if ((temperature - 15) <= thresh1) {
		wlc_sslpnphy_temp_adj_offset(pi, 6);
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope = ((((temperature - 15) + 30) * 286) >> 12) - 0;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
				wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 26);
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 28);
			}
			if (!(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {
				/* for 4329 only */
				tableBuffer[0] = 0xd1a4099c;
				tableBuffer[1] = 0xd1a40018;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 52);
				tableBuffer[0] = 0xf1e64d96;
				tableBuffer[1] = 0xf1e60018;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 54);
				if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
					if ((sslpnphy_specific->sslpnphy_fabid == 2) ||
						(sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
						tableBuffer[0] = 0x204ca9e;
						tableBuffer[1] = 0x2040019;
						wlc_sslpnphy_common_write_table(pi,
							SSLPNPHY_TBL_ID_GAIN_IDX, tableBuffer, 2, 32, 56);
						tableBuffer[0] = 0xa246cea1;
						tableBuffer[1] = 0xa246001c;
						wlc_sslpnphy_common_write_table(pi,
							SSLPNPHY_TBL_ID_GAIN_IDX, tableBuffer, 2, 32, 132);
					}
				}
				PHY_REG_LIST_START
					PHY_REG_WRITE_ENTRY(SSLPNPHY, gainBackOffVal, 0x6666)
					PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 31)
					PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh1, PktRxSignalDropThresh, 15)
				PHY_REG_LIST_EXECUTE(pi);

				if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, (1))
						PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 8)
					PHY_REG_LIST_EXECUTE(pi);
				} else {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, (0))
						PHY_REG_WRITE_ENTRY(SSLPNPHY, ClipCtrDefThresh, 12)
					PHY_REG_LIST_EXECUTE(pi);
				}
			} else {
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 30)
					PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 6)
					PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, (4))
				PHY_REG_LIST_EXECUTE(pi);
				if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329MOTOROLA_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90M_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90U_SSID)) {
					for (i = 0; i < 64; i++)
						wlc_sslpnphy_common_write_table(pi, 2, &minsig, 1, 16, i);
				}
				if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICUNO_SSID) ||
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICLOCO_SSID)) {
					for (i = 25; i <= 26; i++) {
						wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
							UNO_LOW_TEMP_OFDM_GAINTBL_TWEAKS,
							UNO_LOW_TEMP_OFDM_GAINTBL_TWEAKS_sz);
					}
				}
			}
		} else {
			if (((sslpnphy_specific->sslpnphy_fabid == 2) ||
			     (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) &&
			    (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA_5GHz)) {
				for (i = 0; i < 64; i++)
					wlc_sslpnphy_common_write_table(pi, 2, &minsig, 1, 16, i);
			}
		}
	} else if ((temperature - 15) < 4) {
		wlc_sslpnphy_temp_adj_offset(pi, 3);
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope = ((((temperature - 15) - 4) * 286) >> 12) - 0;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
		tableBuffer[0] = sslpnphy_specific->sslpnphy_gain_idx_14_lowword;
		tableBuffer[1] = sslpnphy_specific->sslpnphy_gain_idx_14_hiword;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
			tableBuffer, 2, 32, 28);
		if (!(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {
			/* Added To Increase The 1Mbps Sense for Temps @Around */
			/* -15C Temp With CmRxAciGainTbl */
			tableBuffer[0] = sslpnphy_specific->sslpnphy_gain_idx_27_lowword;
			tableBuffer[1] = sslpnphy_specific->sslpnphy_gain_idx_27_hiword;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, 54);
			if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
				if (freq <= 2427) {
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 253);
				} else if (freq < 2472) {
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 0);
				} else {
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 254);
				}
				if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329TDKMDL11_SSID)
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 0)
						PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 27)
					PHY_REG_LIST_EXECUTE(pi);
			} else if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
				for (i = 63; i <= 73; i++) {
					wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
						tableBuffer, 2, 32, ((i - 37) *2));
					wlc_sslpnphy_common_write_table(pi,
						SSLPNPHY_TBL_ID_GAIN_IDX,
						tableBuffer, 2, 32, (i * 2));
				}
				PHY_REG_LIST_START
					PHY_REG_WRITE_ENTRY(SSLPNPHY, slnanoisetblreg2, 0x03F0)
					PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, (2))
				PHY_REG_LIST_EXECUTE(pi);

			}
		} else {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, (2))
				PHY_REG_MOD_ENTRY(SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 27)
			PHY_REG_LIST_EXECUTE(pi);
			if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329MOTOROLA_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90M_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90U_SSID)) {
				for (i = 0; i < 64; i++)
					wlc_sslpnphy_common_write_table(pi, 2, &minsig, 1, 16, i);
			}
			if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICUNO_SSID) ||
				(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICLOCO_SSID)) {
				for (i = 25; i <= 26; i++) {
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						UNO_LOW_TEMP_OFDM_GAINTBL_TWEAKS,
						UNO_LOW_TEMP_OFDM_GAINTBL_TWEAKS_sz);
				}
			}
		}
		} else {
			if (((sslpnphy_specific->sslpnphy_fabid == 2) ||
			     (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) &&
			    (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA_5GHz)) {
				for (i = 0; i < 64; i++)
					wlc_sslpnphy_common_write_table(pi, 2, &minsig, 1, 16, i);
			}
		}
	} else if ((temperature - 15) < thresh2) {
		wlc_sslpnphy_temp_adj_offset(pi, 0);
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope = (((temperature - 15) - 25) * 286) >> 12;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
		tableBuffer[0] = sslpnphy_specific->sslpnphy_gain_idx_14_lowword;
		tableBuffer[1] = sslpnphy_specific->sslpnphy_gain_idx_14_hiword;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
			tableBuffer, 2, 32, 28);
		if (((temperature) >= 50) & ((temperature) < (thresh2 + 15))) {
			if (!(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {
				PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, (freq <= 2427) ? 255 : 0);
			} else {
				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 3)
					PHY_REG_MOD_RAW_ENTRY(SSLPNPHY_PwrThresh0,
						SSLPNPHY_PwrThresh0_SlowPwrLoThresh_MASK,
						7 << SSLPNPHY_Rev2_PwrThresh0_SlowPwrLoThresh_SHIFT)
				PHY_REG_LIST_EXECUTE(pi);
			}
		}
		}
	} else {
		wlc_sslpnphy_temp_adj_offset(pi, -3);
		sslpnphy_specific->sslpnphy_pkteng_rssi_slope = ((((temperature - 10) - 55) * 286) >> 12) - 2;
		if (CHSPEC_IS2G(pi->radio_chanspec)) {
			PHY_REG_MOD(pi, SSLPNPHY, radioTRCtrlCrs1, trGainThresh, 23);
		if (!(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {
			PHY_REG_WRITE(pi, SSLPNPHY, ClipCtrDefThresh, 12);
			tableBuffer[0] = 0xd0008206;
			tableBuffer[1] = 0xd0000009;
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
				tableBuffer, 2, 32, 28);
			 if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
				tableBuffer[0] = 0x41a4099c;
				tableBuffer[1] = 0x41a4001d;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 52);
				tableBuffer[0] = 0x51e64d96;
				tableBuffer[1] = 0x51e6001d;
				wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_GAIN_IDX,
					tableBuffer, 2, 32, 54);
			} else {
				PHY_REG_MOD(pi, SSLPNPHY, PwrThresh1, PktRxSignalDropThresh, 15);
			}
			if (PHY_XTALFREQ(pi->xtalfreq) == 38400000) {
				/* Special Tuning As The 38.4Mhz Xtal Boards */
				/* SpurProfile Changes Drastically At Very High */
				/* Temp(Especially @85degC) */
				wlc_sslpnphy_chanspec_spur_weight(pi, 0,
					chan_spec_spur_85degc_38p4Mhz,
					chan_spec_spur_85degc_38p4Mhz_sz);
				if (freq == 2452 || freq == 2462) {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, (253))
						PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh0, dsssPwrThresh0, 21)
					PHY_REG_LIST_EXECUTE(pi);

					spur_weight = 4;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
						&spur_weight, 1, 8, ((freq == 2452) ? 18 : 50));
					if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329TDKMDL11_SSID) {
						PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 254);
						spur_weight = 2;
						wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
							&spur_weight, 1, 8, ((freq == 2452) ? 18 : 50));
					}
				} else if (freq == 2467) {
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 253);
					if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329TDKMDL11_SSID) {
						PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 254);
					}
				} else {
					if (freq >= 2472)
						PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 253);
					else
						PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 254);
				}
			} else if (PHY_XTALFREQ(pi->xtalfreq) == 26000000) {
				if (freq <= 2467) {
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 254);
				} else if ((freq > 2467) && (freq <= 2484)) {
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 253);
				}
			} else {
				PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 253);
			}
		} else {
			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 0);
			if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN18_SSID)
				PHY_REG_WRITE(pi, SSLPNPHY, ClipCtrDefThresh, 12);
		}
		}
	}
	wlc_sslpnphy_RxNvParam_Adj(pi);

	WL_INFORM(("InSide TempAdj: Temp = %d:Init_noise_cal = %d\n", temperature,
		sslpnphy_specific->sslpnphy_init_noise_cal_done));

	wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(pi);

	wlc_sslpnphy_txpower_recalc_target(pi);
}

void
wlc_sslpnphy_periodic_cal_top(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	bool suspend;
	int8 current_temperature;
	int32 current_voltage;
	int txidx_drift, volt_drift, temp_drift, temp_drift1, temp_delta_threshold, txidx_delta_threshold;
	int32 cnt;
	int32 volt_avg;
	int32 voltage_samples[50];
	int32 volt_high_thresh, volt_low_thresh = 0;
	uint cal_done = 0;
	wl_pkteng_t pkteng;
	struct ether_addr sa;
	uint16 max_pwr_idx, min_pwr_idx;
	uint twopt3_detected = 0;
	uint8 band_idx;

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);

	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		WL_WRITE_SHM(pi, M_CTS_DURATION, 10000);
		WL_SUSPEND_MAC_AND_WAIT(pi);
	}

	if (!NON_BT_CHIP(wlc))
		wlc_sslpnphy_btcx_override_enable(pi);

	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	pi->phy_lastcal = GENERIC_PHY_INFO(pi)->now;
	sslpnphy_specific->sslpnphy_restore_papd_cal_results = 0;
	sslpnphy_specific->sslpnphy_recal = 1;
	pi->phy_forcecal = FALSE;
	sslpnphy_specific->sslpnphy_full_cal_channel[band_idx] = CHSPEC_CHANNEL(pi->radio_chanspec);
	sslpnphy_specific->sslpnphy_full_cal_chanspec[band_idx] = pi->radio_chanspec;

	if (sslpnphy_specific->sslpnphy_percal_ctr == 0) {
		sslpnphy_specific->sslpnphy_tx_idx_prev_cal = (uint8)((phy_reg_read(pi, 0x4ab) & 0x7f00) >> 8);
	}

	current_temperature = (int8)wlc_sslpnphy_tempsense(pi);
	current_voltage = wlc_sslpnphy_vbatsense(pi);
	sslpnphy_specific->sslpnphy_cur_idx = (uint8)((phy_reg_read(pi, 0x4ab) & 0x7f00) >> 8);

#ifdef BCMDBG
	if (sslpnphy_specific->sslpnphy_force_percal) {
		if (sslpnphy_specific->sslpnphy_percal_ctr == 3)
			sslpnphy_specific->sslpnphy_percal_ctr = 0;
		switch (sslpnphy_specific->sslpnphy_percal_ctr) {

			case 0:
				wlc_sslpnphy_periodic_cal(pi);
				break;
			case 1:
				wlc_sslpnphy_tx_pwr_ctrl_init(pi);
				break;
			case 2:
				sslpnphy_specific->sslpnphy_force_1_idxcal = 1;
				sslpnphy_specific->sslpnphy_papd_nxt_cal_idx = sslpnphy_specific->sslpnphy_final_idx;
				wlc_sslpnphy_papd_recal(pi);
#if !defined(ROMTERMPHY)
#ifndef PPR_API
				wlc_phy_txpower_recalc_target(pi);
#endif /* PPR_API */
#else
				wlc_phy_txpower_recalc_target((wlc_phy_t *)pi, -1, NULL);
#endif /* PHYHAL */
				break;
		}
	}
#endif /* BCMDBG */
	sslpnphy_specific->sslpnphy_percal_ctr ++;

	temp_delta_threshold = 10;
	txidx_delta_threshold = 6;
	/* Update Tx Index delta and Temp delta thresholds based on input from iovar */
	if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) {
		txidx_delta_threshold = (int) pi->txidx_delta_threshold;
		temp_delta_threshold = (int) pi->temp_delta_threshold;
	}
	if ((sslpnphy_specific->sslpnphy_force_1_idxcal == 0) && (sslpnphy_specific->sslpnphy_force_percal == 0)) {
		temp_drift = current_temperature - sslpnphy_specific->sslpnphy_last_cal_temperature;
		temp_drift1 = current_temperature - sslpnphy_specific->sslpnphy_last_full_cal_temperature;
		/* Temperature change of 25 degrees or at an interval of 20 minutes do a full cal */
		if ((temp_drift1 < - 25) || (temp_drift1 > 25) ||
			(sslpnphy_specific->sslpnphy_percal_ctr == 100)) {
			wlc_2063_vco_cal(pi);
			wlc_sslpnphy_periodic_cal(pi);
#if defined(SSLPNPHYCAL_CACHING)
			wlc_phy_reset_sslpnphy_chanctx(CHSPEC_CHANNEL(pi->radio_chanspec));
			wlc_phy_cal_cache_sslpnphy(pi);
#endif
			wlc_sslpnphy_tx_pwr_ctrl_init(pi);
			wlc_sslpnphy_papd_recal(pi);
			wlc_sslpnphy_temp_adj(pi);
			sslpnphy_specific->sslpnphy_percal_ctr = 0;
			sslpnphy_specific->sslpnphy_last_full_cal_temperature = current_temperature;
			cal_done = 1;
		}
		/* Monitor vcocal refresh bit and relock PLL if out of range */
		if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) &&
			(read_radio_reg(pi, RADIO_2063_PLL_JTAGIN_PLL_1) & 0x8) && (cal_done == 0)) {
			wlc_2063_vco_cal(pi);
		}
		if (((temp_drift > temp_delta_threshold) || (temp_drift < -temp_delta_threshold)) && (cal_done == 0)) {
			wlc_sslpnphy_tx_pwr_ctrl_init(pi);
			sslpnphy_specific->sslpnphy_force_1_idxcal = 1;
			sslpnphy_specific->sslpnphy_papd_nxt_cal_idx = sslpnphy_specific->sslpnphy_final_idx -
				((current_temperature - sslpnphy_specific->sslpnphy_last_cal_temperature) / 3);
			if (sslpnphy_specific->sslpnphy_papd_nxt_cal_idx >= 128)
				sslpnphy_specific->sslpnphy_papd_nxt_cal_idx = 1;
			wlc_sslpnphy_papd_recal(pi);
			wlc_sslpnphy_temp_adj(pi);
			cal_done = 1;
		}
		if (!cal_done) {
			volt_drift = current_voltage - sslpnphy_specific->sslpnphy_last_cal_voltage;
			txidx_drift = sslpnphy_specific->sslpnphy_cur_idx - sslpnphy_specific->sslpnphy_tx_idx_prev_cal;
			if ((txidx_drift > txidx_delta_threshold) || (txidx_drift < -txidx_delta_threshold)) {
				if (((volt_drift < 3) && (volt_drift > -3)) ||
					/* For SDNA board do not check voltage drift condition, trigger */
					/* PAPD recal just based on tx index drift */
					(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID)) {
					sslpnphy_specific->sslpnphy_papd_nxt_cal_idx =
					        sslpnphy_specific->sslpnphy_final_idx +
					        (sslpnphy_specific->sslpnphy_cur_idx -
					         sslpnphy_specific->sslpnphy_tx_idx_prev_cal);
					if (sslpnphy_specific->sslpnphy_papd_nxt_cal_idx >= 128)
						sslpnphy_specific->sslpnphy_papd_nxt_cal_idx = 1;
					sslpnphy_specific->sslpnphy_force_1_idxcal = 1;
					wlc_sslpnphy_papd_recal(pi);
					if (txidx_drift < 0)
						sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 4;
					else
						sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 0;
					sslpnphy_specific->sslpnphy_tx_idx_prev_cal =
					        (uint8)sslpnphy_specific->sslpnphy_cur_idx;
					cal_done = 1;
				}
			}
		}
		if (!cal_done) {
			if (!IS_OLYMPIC(pi)) {
				volt_avg = 0;
				for (cnt = 0; cnt < 32; cnt++) {
					voltage_samples[cnt] = wlc_sslpnphy_vbatsense(pi);
					volt_avg += voltage_samples[cnt];
					OSL_DELAY(120);
				/* assuming a 100us time for executing wlc_sslpnphy_vbatsense */
				}
				volt_avg = volt_avg >> 5;
				volt_high_thresh = 0;
				volt_low_thresh = volt_avg;
				for (cnt = 0; cnt < 32; cnt++) {
					if (voltage_samples[cnt] > volt_high_thresh)
						volt_high_thresh = voltage_samples[cnt];
					if (voltage_samples[cnt] < volt_low_thresh)
						volt_low_thresh = voltage_samples[cnt];
				}
				sslpnphy_specific->sslpnphy_volt_winner = (uint8)(volt_high_thresh - 2);
				if ((volt_high_thresh - volt_low_thresh) > 5)
					sslpnphy_specific->sslpnphy_vbat_ripple = 1;
				else
					sslpnphy_specific->sslpnphy_vbat_ripple = 0;

				if (sslpnphy_specific->sslpnphy_vbat_ripple) {
					sa.octet[0] = 10;
					pkteng.flags = WL_PKTENG_PER_TX_START;
					pkteng.delay = 30;	/* Inter packet delay */
					pkteng.length = 0;	/* packet length */
					pkteng.seqno = FALSE;	/* enable/disable sequence no. */
					/* vbat ripple detetction */
					twopt3_detected = 0;
					/* to clear out min and max readings after 30 frames */
					WL_WRITE_SHM(pi, M_SSLPN_PWR_IDX_MAX, 0);
					WL_WRITE_SHM(pi, M_SSLPN_PWR_IDX_MIN, 127);

					/* sending out 100 frames to caluclate min & max index */
					pkteng.nframes = 100;		/* number of frames */
					wlc_sslpnphy_pktengtx((wlc_phy_t *)pi, &pkteng,
						108, &sa, (1000*10));
					max_pwr_idx = WL_READ_SHM(pi, M_SSLPN_PWR_IDX_MAX);
					min_pwr_idx = WL_READ_SHM(pi, M_SSLPN_PWR_IDX_MIN);
					/* 10 is the value chosen for a start power of 14dBm */
					if (!((max_pwr_idx == 0) && (min_pwr_idx == 127))) {
						if (((max_pwr_idx - min_pwr_idx) > 10) ||
							(min_pwr_idx == 0)) {
							twopt3_detected = 1;
						}
					}
					if (twopt3_detected) {
						sslpnphy_specific->sslpnphy_psat_2pt3_detected = 1;
						sslpnphy_specific->sslpnphy_11n_backoff = 17;
						sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 26;
						sslpnphy_specific->sslpnphy_lowerofdm = 20;
						sslpnphy_specific->sslpnphy_cck = 20;
						sslpnphy_specific->sslpnphy_last_cal_voltage = volt_low_thresh;
						sslpnphy_specific->sslpnphy_tx_idx_prev_cal = (uint8)max_pwr_idx;
					} else if (volt_low_thresh >
						sslpnphy_specific->sslpnphy_last_cal_voltage + 6) {
						sslpnphy_specific->sslpnphy_psat_2pt3_detected = 0;
						sslpnphy_specific->sslpnphy_lowerofdm = 0;
						sslpnphy_specific->sslpnphy_cck = 0;
						if (volt_low_thresh > 57) {
							sslpnphy_specific->sslpnphy_11n_backoff = 0;
							sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 0;
						} else {
							sslpnphy_specific->sslpnphy_11n_backoff = 8;
							sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 10;
						}
						sslpnphy_specific->sslpnphy_last_cal_voltage = volt_low_thresh;
						sslpnphy_specific->sslpnphy_tx_idx_prev_cal = (uint8)max_pwr_idx;
					} else if ((volt_low_thresh <= 57) && (volt_low_thresh <
								sslpnphy_specific->sslpnphy_last_cal_voltage)) {
						sslpnphy_specific->sslpnphy_psat_2pt3_detected = 0;
						if (current_temperature >= 50) {
							sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 10;
							sslpnphy_specific->sslpnphy_11n_backoff = 6;
						} else {
							sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 10;
							sslpnphy_specific->sslpnphy_11n_backoff = 4;
						}
						sslpnphy_specific->sslpnphy_last_cal_voltage = volt_low_thresh;
						sslpnphy_specific->sslpnphy_tx_idx_prev_cal = (uint8)max_pwr_idx;
					}
				} else {
					sslpnphy_specific->sslpnphy_psat_2pt3_detected = 0;
					if (current_voltage > sslpnphy_specific->sslpnphy_last_cal_voltage + 6) {
						if (current_voltage > 57) {
							sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 0;
							sslpnphy_specific->sslpnphy_11n_backoff = 0;
						} else {
							sslpnphy_specific->sslpnphy_11n_backoff = 8;
							sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 10;
							sslpnphy_specific->sslpnphy_lowerofdm = 0;
							sslpnphy_specific->sslpnphy_cck = 0;
						}
						sslpnphy_specific->sslpnphy_last_cal_voltage
						        = current_voltage;
						sslpnphy_specific->sslpnphy_tx_idx_prev_cal
						        = (uint8)sslpnphy_specific->sslpnphy_cur_idx;
					} else if ((current_voltage <= 57) &&
					           (current_voltage < sslpnphy_specific->sslpnphy_last_cal_voltage)) {
						if (current_temperature >= 50) {
							sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 10;
							sslpnphy_specific->sslpnphy_11n_backoff = 6;
						} else {
							sslpnphy_specific->sslpnphy_54_48_36_24mbps_backoff = 10;
							sslpnphy_specific->sslpnphy_11n_backoff = 4;
						}
						sslpnphy_specific->sslpnphy_last_cal_voltage =
						        current_voltage;
						sslpnphy_specific->sslpnphy_tx_idx_prev_cal =
						        (uint8)sslpnphy_specific->sslpnphy_cur_idx;
					}

				}
			}
		}
	}
	if (sslpnphy_specific->sslpnphy_force_1_idxcal) {
		sslpnphy_specific->sslpnphy_last_cal_temperature = current_temperature;
		sslpnphy_specific->sslpnphy_tx_idx_prev_cal = (uint8)sslpnphy_specific->sslpnphy_cur_idx;
		sslpnphy_specific->sslpnphy_last_cal_voltage = current_voltage;
		sslpnphy_specific->sslpnphy_force_1_idxcal = 0;
	}
	sslpnphy_specific->sslpnphy_recal = 0;
#if !defined(ROMTERMPHY)
#ifndef PPR_API
	wlc_phy_txpower_recalc_target(pi);
#endif /* PPR_API */
#else
	wlc_phy_txpower_recalc_target((wlc_phy_t *)pi, -1, NULL);
#endif /* PHYHAL */
	sslpnphy_specific->sslpnphy_restore_papd_cal_results = 1;
	if (!suspend)
		WL_ENABLE_MAC(pi);
	if (NORADIO_ENAB(pi->pubpi))
		return;

}


void
wlc_sslpnphy_periodic_cal(phy_info_t *pi)
{
	bool suspend;
	uint16 tx_pwr_ctrl;
	uint8 band_idx;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	pi->phy_lastcal = GENERIC_PHY_INFO(pi)->now;
	pi->phy_forcecal = FALSE;
	sslpnphy_specific->sslpnphy_full_cal_channel[band_idx] = CHSPEC_CHANNEL(pi->radio_chanspec);
	sslpnphy_specific->sslpnphy_full_cal_chanspec[band_idx] = pi->radio_chanspec;

	if (NORADIO_ENAB(pi->pubpi))
		return;

	suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		WL_WRITE_SHM(pi, M_CTS_DURATION, 10000);
		WL_SUSPEND_MAC_AND_WAIT(pi);
	}
	if (!NON_BT_CHIP(wlc))
		wlc_sslpnphy_btcx_override_enable(pi);


	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	/* Tx iqlo calibration */
	wlc_sslpnphy_txpwrtbl_iqlo_cal(pi);

	/* Restore tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);


	/* Rx iq calibration */


		wlc_sslpnphy_set_deaf(pi);
		wlc_sslpnphy_rx_iq_cal(pi,
			NULL,
			0,
			FALSE, TRUE, FALSE, TRUE, 127);
		wlc_sslpnphy_clear_deaf(pi);


	if (!suspend)
		WL_ENABLE_MAC(pi);
	return;
}

void
wlc_sslpnphy_full_cal(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint8 band_idx;

	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
#if defined(SSLPNPHYCAL_CACHING)
	sslpnphy_calcache_t * ctx =
		wlc_phy_get_sslpnphy_chanctx(CHSPEC_CHANNEL(pi->radio_chanspec));
#endif

#ifdef ROMTERMPHY
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif

	WL_TRACE(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	wlc_sslpnphy_set_deaf(pi);

	/* Force full calibration run */
	sslpnphy_specific->sslpnphy_full_cal_channel[band_idx] = 0;
	sslpnphy_specific->sslpnphy_full_cal_chanspec[band_idx] = 0;
#if defined(SSLPNPHYCAL_CACHING)
	if (check_valid_channel_to_cache(CHSPEC_CHANNEL(pi->radio_chanspec))) {
		if (!ctx) {
			ctx = wlc_phy_create_sslpnphy_chanctx(CHSPEC_CHANNEL(pi->radio_chanspec));
		}
		if (ctx) {
			if (!ctx->valid) {
				/* Run sslpnphy cals */
				wlc_sslpnphy_periodic_cal(pi);
				/* Do not cache cal coeffs here as we redo Tx IQ LO cal later */
			} else {
				/* Restore cached cal coefficients */
				wlc_phy_cal_cache_restore_sslpnphy(pi);
			}
		} else {
			/* Run sslpnphy cals */
			wlc_sslpnphy_periodic_cal(pi);
		}
	} else {
		/* Run sslpnphy cals */
		wlc_sslpnphy_periodic_cal(pi);
	}
#else
	/* Run sslpnphy cals */
	wlc_sslpnphy_periodic_cal(pi);
#endif /* SSLPNPHYCAL_CACHING */
	wlc_sslpnphy_clear_deaf(pi);

	/* Trigger uCode for doing AuxADC measurements */
#ifdef ROMTERMPHY
	if (sdna_board_flag) {
		wlc_sslpnphy_write_shm_tssiCalEn(pi, 0);
		wlc_sslpnphy_auxadc_measure((wlc_phy_t *) pi, 0);
	} else {
#else
	{
#endif
		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
			M_SSLPNPHY_TSSICAL_EN)), 0x0);
		wlc_sslpnphy_auxadc_measure((wlc_phy_t *) pi, 0);
	}
	return;
}
void wlc_sslpnphy_force_adj_gain(phy_info_t *pi, bool on, int mode)
{
	/* ACI forced mitigation */

#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

	if (on) {
		uint16 higain, pwroff;
		int16 nfs_adj;

		if (mode == WLAN_MANUAL) {
			higain = sslpnphy_specific->Listen_GaindB_BASE - 6;
			pwroff = sslpnphy_specific->RxpowerOffset_Required_BASE - 3;
			nfs_adj = sslpnphy_specific->NfSubtractVal_BASE - 66;
		} else {
			higain = sslpnphy_specific->Listen_GaindB_BASE - 3;
			pwroff = sslpnphy_specific->RxpowerOffset_Required_BASE - 2;
			nfs_adj = sslpnphy_specific->NfSubtractVal_BASE - 33;
		}

		if (IS40MHZ(pi)) {

			WL_PHYCAL(("Base_higain40:0x%x Base_pwroff40:0x%x\n",
				sslpnphy_specific->Listen_GaindB_BASE,
				sslpnphy_specific->RxpowerOffset_Required_BASE));

			PHY_REG_MOD(pi, SSLPNPHY_Rev2, HiGainDB_40, HiGainDB, higain);
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, pwroff);
			PHY_REG_WRITE(pi, SSLPNPHY_Rev2, nfSubtractVal_40, nfs_adj);

			WL_PHYCAL(("Set higain40:0x%x pwroff40:0x%x\n",
				phy_reg_read(pi, SSLPNPHY_Rev2_HiGainDB_40),
				phy_reg_read(pi, SSLPNPHY_Rev2_InputPowerDB_40)));
		} else {

			WL_PHYCAL(("Base_higain20:0x%x Base_pwroff20:0x%x\n",
				sslpnphy_specific->Listen_GaindB_BASE,
				sslpnphy_specific->RxpowerOffset_Required_BASE));

			PHY_REG_MOD(pi, SSLPNPHY, HiGainDB, HiGainDB, higain);
			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, pwroff);
			PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, nfs_adj);

			WL_PHYCAL(("Set higain20:0x%x pwroff20:0x%x\n",
				phy_reg_read(pi, SSLPNPHY_HiGainDB),
				phy_reg_read(pi, SSLPNPHY_InputPowerDB)));
		}
	} else if (!on) {
		if (IS40MHZ(pi)) {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, HiGainDB_40, HiGainDB,
				sslpnphy_specific->Listen_GaindB_BASE);
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb,
				sslpnphy_specific->RxpowerOffset_Required_BASE);
			PHY_REG_WRITE(pi, SSLPNPHY_Rev2, nfSubtractVal_40,
				sslpnphy_specific->NfSubtractVal_BASE);

			WL_PHYCAL(("reset higain40:0x%x pwroff40:0x%x\n",
				phy_reg_read(pi, SSLPNPHY_Rev2_HiGainDB_40),
				phy_reg_read(pi, SSLPNPHY_Rev2_InputPowerDB_40)));
		} else {
			PHY_REG_MOD(pi, SSLPNPHY, HiGainDB, HiGainDB,
				sslpnphy_specific->Listen_GaindB_BASE);
			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb,
				sslpnphy_specific->RxpowerOffset_Required_BASE);
			PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal,
				sslpnphy_specific->NfSubtractVal_BASE);

			WL_PHYCAL(("reset higain20:0x%x pwroff20:0x%x\n",
				phy_reg_read(pi, SSLPNPHY_HiGainDB),
				phy_reg_read(pi, SSLPNPHY_InputPowerDB)));
		}
	}
}
static void
wlc_sslpnphy_aci_init(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	PHY_REG_OR(pi, SSLPNPHY, failCtrCtrl, 0x02);
	pi->aci_state = 0;
	sslpnphy_specific->sslpnphy_aci.gain_backoff = 0;
	sslpnphy_specific->sslpnphy_aci.on_thresh = 800;
	sslpnphy_specific->sslpnphy_aci.on_timeout = 1;
	sslpnphy_specific->sslpnphy_aci.off_thresh = 500;
	sslpnphy_specific->sslpnphy_aci.off_timeout = 20;
	/* pi->sslpnphy_aci.glitch_timeout = 500; */
	wlc_sslpnphy_aci(pi, FALSE);
}

void
wlc_sslpnphy_aci(phy_info_t *pi, bool on)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint8 phybw40 = IS40MHZ(pi);

	if (on && !(pi->aci_state & ACI_ACTIVE)) {
		WL_PHYCAL(("ACI ON\n"));
		pi->aci_state |= ACI_ACTIVE;
		/* set the registers for ACI */
		sslpnphy_specific->sslpnphy_aci.gain_backoff = 1;
	} else if (!on && (pi->aci_state & ACI_ACTIVE)) {
		WL_PHYCAL(("ACI OFF\n"));

		pi->aci_state &= ~ACI_ACTIVE;
		sslpnphy_specific->sslpnphy_aci.gain_backoff = 0;
		if (phybw40 == 0) {
			PHY_REG_MOD(pi, SSLPNPHY, HiGainDB, HiGainDB,
				sslpnphy_specific->Listen_GaindB_AfrNoiseCal);
			PHY_REG_MOD(pi, SSLPNPHY, crsedthresh, edonthreshold,
				sslpnphy_specific->EdOn_Thresh20_BASE);
		} else {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, HiGainDB_40, HiGainDB,
				sslpnphy_specific->Listen_GaindB_AfrNoiseCal);
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, crsedthresh_40, edonthreshold,
				sslpnphy_specific->EdOn_Thresh40_BASE);
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, edthresh20ul, edonthreshold20L,
				sslpnphy_specific->EdOn_Thresh20L_BASE);
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, edthresh20ul, edonthreshold20U,
				sslpnphy_specific->EdOn_Thresh20U_BASE);

		}

		wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(pi);
	}

	sslpnphy_specific->sslpnphy_aci.ts = GENERIC_PHY_INFO(pi)->now;
	PHY_REG_OR(pi, SSLPNPHY, failCtrCtrl, 0x01);
	sslpnphy_specific->sslpnphy_aci.glitch_cnt = 0;
	sslpnphy_specific->sslpnphy_aci.ts = (int)GENERIC_PHY_INFO(pi)->now;
}

static void
wlc_sslpnphy_aci_upd(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	int cnt, delta, t;

	cnt = (int)phy_reg_read(pi, SSLPNPHY_Rev2_crsDetFailCtr);
	delta = (cnt - sslpnphy_specific->sslpnphy_aci.glitch_cnt) & 0xffff;

	sslpnphy_specific->sslpnphy_aci.glitch_cnt = cnt;
	if ((!(pi->aci_state & ACI_ACTIVE) && (delta < sslpnphy_specific->sslpnphy_aci.on_thresh)) ||
	     ((pi->aci_state & ACI_ACTIVE) && (delta > sslpnphy_specific->sslpnphy_aci.off_thresh)))
		sslpnphy_specific->sslpnphy_aci.ts = (int)GENERIC_PHY_INFO(pi)->now;

	if (pi->aci_state & ACI_ACTIVE) {
		if (delta > sslpnphy_specific->sslpnphy_aci.off_thresh)
			sslpnphy_specific->sslpnphy_aci.gain_backoff = 1;
		else
			sslpnphy_specific->sslpnphy_aci.gain_backoff = 0;
	}

	t = (int)GENERIC_PHY_INFO(pi)->now - sslpnphy_specific->sslpnphy_aci.ts;
	if (!(pi->aci_state & ACI_ACTIVE) && (t >= sslpnphy_specific->sslpnphy_aci.on_timeout))
		wlc_sslpnphy_aci(pi, TRUE);
	else if ((pi->aci_state & ACI_ACTIVE) && (t >= sslpnphy_specific->sslpnphy_aci.off_timeout))
		wlc_sslpnphy_aci(pi, FALSE);

	WL_PHYCAL(("sslpnphy aci: %s %d %d %d\n",
		((pi->aci_state & ACI_ACTIVE)?"ON":"OFF"), cnt, delta, t));
	WL_PHYCAL(("sslpnphy aci: Gain_Backoff = %d Gain_Before_AciDetect = %d"
		" Gain_After_AciDetect = %d Base_Gain = %d\n", sslpnphy_specific->sslpnphy_aci.gain_backoff,
		sslpnphy_specific->Listen_GaindB_AfrNoiseCal, sslpnphy_specific->Latest_Listen_GaindB_Latch,
		sslpnphy_specific->Listen_GaindB_BASE));
}

#if defined(WLTEST)

static uint32
wlc_sslpnphy_measure_digital_power(phy_info_t *pi, uint16 nsamples)
{
	sslpnphy_iq_est_t iq_est;

	bzero(&iq_est, sizeof(iq_est));
	if (!wlc_sslpnphy_rx_iq_est(pi, nsamples, 32, &iq_est))
	    return 0;

	return (iq_est.i_pwr + iq_est.q_pwr) / nsamples;
}

static uint32
wlc_sslpnphy_get_receive_power(phy_info_t *pi, int32 *gain_index)
{
	uint32 received_power = 0;
	int32 max_index = 0;
	uint32 gain_code = 0;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	max_index = 36;
	if (*gain_index >= 0)
		gain_code = sslpnphy_gaincode_table[*gain_index];

	/* wlc_sslpnphy_set_deaf(pi); */
	if (*gain_index == -1) {
		*gain_index = 0;
		while ((*gain_index <= (int32)max_index) && (received_power < 700)) {
			wlc_sslpnphy_set_rx_gain(pi, sslpnphy_gaincode_table[*gain_index]);
			received_power =
				wlc_sslpnphy_measure_digital_power(pi, sslpnphy_specific->sslpnphy_noise_samples);
			(*gain_index) ++;
		}
		(*gain_index) --;
	}
	else {
		wlc_sslpnphy_set_rx_gain(pi, gain_code);
		received_power = wlc_sslpnphy_measure_digital_power(pi, sslpnphy_specific->sslpnphy_noise_samples);
	}

	/* wlc_sslpnphy_clear_deaf(pi); */
	return received_power;
}

#if !defined(ROMTERMPHY)
int32
#else
uint32
#endif
wlc_sslpnphy_rx_signal_power(phy_info_t *pi, int32 gain_index)
{
	uint32 gain = 0;
	uint32 nominal_power_db;
	uint32 log_val, gain_mismatch, desired_gain, input_power_offset_db, input_power_db;
	int32 received_power, temperature;
	uint freq;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	received_power = wlc_sslpnphy_get_receive_power(pi, &gain_index);

	gain = sslpnphy_gain_table[gain_index];

	nominal_power_db = PHY_REG_READ(pi, SSLPNPHY, VeryLowGainDB, NominalPwrDB);

	{
		uint32 power = (received_power*16);
		uint32 msb1, msb2, val1, val2, diff1, diff2;
		msb1 = find_msbit(power);
		msb2 = msb1 + 1;
		val1 = 1 << msb1;
		val2 = 1 << msb2;
		diff1 = (power - val1);
		diff2 = (val2 - power);
		if (diff1 < diff2)
			log_val = msb1;
		else
			log_val = msb2;
	}

	log_val = log_val * 3;

	gain_mismatch = (nominal_power_db/2) - (log_val);

	desired_gain = gain + gain_mismatch;

	input_power_offset_db = PHY_REG_READ(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb);
	if (input_power_offset_db > 127)
		input_power_offset_db -= 256;

	input_power_db = input_power_offset_db - desired_gain;

	/* compensation from PHY team */
	/* includes path loss of 2dB from Murata connector to chip input */
	input_power_db = input_power_db + sslpnphy_gain_index_offset_for_rssi[gain_index];
	/* Channel Correction Factor */
	freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	if ((freq > 2427) && (freq <= 2467))
		input_power_db = input_power_db - 1;

	/* temperature correction */
	temperature = sslpnphy_specific->sslpnphy_lastsensed_temperature;
	/* printf(" CW_RSSI Temp %d \n",temperature); */
	if ((temperature - 15) < -30) {
		input_power_db = input_power_db + (((temperature - 10 - 25) * 286) >> 12) - 7;
	} else if ((temperature - 15) < 4) {
		input_power_db = input_power_db + (((temperature - 10 - 25) * 286) >> 12) - 3;
	} else {
		input_power_db = input_power_db + (((temperature - 10 - 25) * 286) >> 12);
	}

	wlc_sslpnphy_rx_gain_override_enable(pi, 0);

	return input_power_db;
}

#if defined(ROMTERMPHY)
int8
wlc_sslpnphy_samp_collect_agc(phy_info_t *pi, bool agc_en)
{
	uint32 received_power = 0;
	int8 tableBuffer;
	uint8 tia_gain = 0, lna2_gain = 0, biq1_gain = 0;
	int8 tia_gain_dB, lna2_gain_dB, biq1_gain_dB, total_gain_dB;

	bool extlna = ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA_5GHz) &&
		CHSPEC_IS5G(pi->radio_chanspec)) ||
		((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
		CHSPEC_IS2G(pi->radio_chanspec));

	/* Put in deaf mode and overriding gain */
	wlc_sslpnphy_detection_disable(pi, TRUE);

	/* Fix the hpf and lpf values */
	PHY_REG_LIST_START
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2val, hpf1_ctrl_ovr_val, 0x01)
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2val, hpf2_ctrl_ovr_val, 0x01)
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2val, lpf_lq_ovr_val, 0x07)
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, hpf1_ctrl_ovr, 0x01)
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, hpf2_ctrl_ovr, 0x01)
		PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, lpf_lq_ovr, 0x01)
	PHY_REG_LIST_EXECUTE(pi);

	if (extlna) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2val, gmode_ext_lna_gain_ovr_val, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2val, amode_ext_lna_gain_ovr_val, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, gmode_ext_lna_gain_ovr, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, rfoverride2, amode_ext_lna_gain_ovr, 1)
		PHY_REG_LIST_EXECUTE(pi);
	}
	wlc_sslpnphy_set_trsw_override(pi, FALSE, TRUE);
	wlc_sslpnphy_rx_gain_override_enable(pi, TRUE);
	if (agc_en) {
		while (((biq1_gain <= 2) && (tia_gain <= 6) && (lna2_gain <= 3)) && (received_power < 700)) {
			wlc_sslpnphy_set_rx_gain_by_distribution(pi, 0, 0, 0, biq1_gain, tia_gain, lna2_gain, 3);
			received_power =  wlc_sslpnphy_measure_digital_power(pi, 1024);
			if (received_power < 700) {
				if (lna2_gain < 3) {
					lna2_gain++;
				} else {
					if (tia_gain < 6) {
						tia_gain++;
					} else {
						biq1_gain++;
					}
				}
			}
		}
	} else {
		if (wlc_cur_bandtype(pi->wlc) == WLC_BAND_2G) {
			lna2_gain = 2;
			tia_gain = 3;
			biq1_gain = 2;
		} else if (wlc_cur_bandtype(pi->wlc) == WLC_BAND_5G) {
			lna2_gain = 3;
			tia_gain = 5;
			biq1_gain = 2;
		}
		wlc_sslpnphy_set_rx_gain_by_distribution(pi, 0, 0, 0, biq1_gain, tia_gain, lna2_gain, 3);
	}

	wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(pi);

	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
		&tableBuffer, 1, 8, lna2_gain + 4);
	if (tableBuffer > 63)
		tableBuffer -= 128;
	lna2_gain_dB = tableBuffer;
	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
		&tableBuffer, 1, 8, tia_gain + 8);
	if (tableBuffer > 63)
		tableBuffer -= 128;
	tia_gain_dB = tableBuffer;
	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
		&tableBuffer, 1, 8, (biq1_gain >= 3 ? 2 : biq1_gain) + 16);
	if (tableBuffer > 63)
		tableBuffer -= 128;
	biq1_gain_dB = tableBuffer;
	wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_GAINVALTBL_IDX,
		&tableBuffer, 1, 8, 67);
	if (tableBuffer > 63)
		tableBuffer -= 128;
	total_gain_dB = tableBuffer + 0 + 25 + lna2_gain_dB + tia_gain_dB + biq1_gain_dB + 0;

	return total_gain_dB;
}
#endif /* ROMTERMPHY */
#endif 

#if defined(BCMDBG) || defined(WLTEST) || defined(WLCURPOWER) || !defined(ROMTERMPHY)
void
wlc_sslpnphy_get_tssi(phy_info_t *pi, int8 *ofdm_pwr, int8 *cck_pwr)
{
	int8 cck_offset;
	uint16 status;

	if (wlc_sslpnphy_tssi_enabled(pi) &&
		(status = phy_reg_read(pi, SSLPNPHY_TxPwrCtrlStatus)) & SSLPNPHY_TxPwrCtrlStatus_estPwrValid_MASK) {
		*ofdm_pwr = (int8)((status &
			SSLPNPHY_TxPwrCtrlStatus_estPwr_MASK) >>
			SSLPNPHY_TxPwrCtrlStatus_estPwr_SHIFT);
		if (wlc_phy_tpc_isenabled_sslpnphy(pi)) {
#ifdef PPR_API
			ppr_dsss_rateset_t dsss;
			ppr_get_dsss(pi->tx_power_offset, WL_TX_BW_20,
			             WL_TX_CHAINS_1, &dsss);
			cck_offset = dsss.pwr[0];
#else
			cck_offset = pi->tx_power_offset[TXP_FIRST_CCK];
#endif
		}
		else
			cck_offset = 0;
		*cck_pwr = *ofdm_pwr + cck_offset;
	} else {
		*ofdm_pwr = 0;
		*cck_pwr = 0;
	}
}
#endif 

void
WLBANDINITFN(wlc_phy_cal_init_sslpnphy)(phy_info_t *pi)
{
}
STATIC uint8 chan_spec_spur_nokref_38p4Mhz [][3] = {
	{1, 5, 23},
	{2, 5, 7},
	{3, 5, 55},
	{4, 5, 39},
	{9, 3, 18},
	{10, 2, 2},
	{10, 3, 22},
	{11, 2, 50},
	{11, 3, 6},
	{12, 3, 54},
	{13, 3, 38},
	{13, 3, 26}
};
uint8 chan_spec_spur_nokref_38p4Mhz_sz = ARRAYSIZE(chan_spec_spur_nokref_38p4Mhz);
STATIC uint8 chan_spec_spur_tdkmdl_38p4Mhz [][3] = {
	{1, 3, 23},
	{2, 3, 7},
	{3, 3, 55},
	{4, 3, 39},
	{13, 3, 26}
};
uint8 chan_spec_spur_tdkmdl_38p4Mhz_sz = ARRAYSIZE(chan_spec_spur_tdkmdl_38p4Mhz);
STATIC uint8 chan_spec_spur_26Mhz [][3] = {
	{1, 3, 19},
	{2, 5, 3},
	{3, 4, 51},
	{11, 3, 26},
	{12, 3, 10},
	{13, 3, 58}
};
uint8 chan_spec_spur_26Mhz_sz = ARRAYSIZE(chan_spec_spur_26Mhz);
STATIC uint8 chan_spec_spur_37p4Mhz [][3] = {
	{4, 3, 13},
	{5, 3, 61},
	{6, 3, 45},
	{11, 3, 20},
	{12, 3, 4},
	{13, 3, 52}
};
uint8 chan_spec_spur_37p4Mhz_sz = ARRAYSIZE(chan_spec_spur_37p4Mhz);
STATIC uint8 chan_spec_spur_xtlna38p4Mhz [][3] = {
	{7, 2, 57},
	{7, 2, 58},
	{11, 2, 6},
	{11, 2, 7},
	{13, 2, 25},
	{13, 2, 26},
	{13, 2, 38},
	{13, 2, 39}
};
uint8 chan_spec_spur_xtlna38p4Mhz_sz = ARRAYSIZE(chan_spec_spur_xtlna38p4Mhz);
STATIC uint8 chan_spec_spur_xtlna26Mhz [][3] = {
	{1, 2, 19},
	{2, 2, 3},
	{3, 2, 51},
	{11, 2, 26},
	{12, 2, 10},
	{13, 2, 58}
};
uint8 chan_spec_spur_xtlna26Mhz_sz = ARRAYSIZE(chan_spec_spur_xtlna26Mhz);
STATIC uint8 chan_spec_spur_xtlna37p4Mhz [][3] = {
	{4, 2, 13},
	{5, 2, 61},
	{6, 2, 45}
};
uint8 chan_spec_spur_xtlna37p4Mhz_sz = ARRAYSIZE(chan_spec_spur_xtlna37p4Mhz);
static void
wlc_sslpnphy_chanspec_spur_weight(phy_info_t *pi, uint channel, uint8 ptr[][3], uint8 array_size)
{
	uint8 i = 0;
	for (i = 0; i < array_size; i++) {
		if (ptr[i][0] == channel) {
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
				&ptr[i][1], 1, 8, ptr[i][2]);
		}
	}
}

static void
wlc_sslpnphy_set_chanspec_tweaks_5356(phy_info_t *pi, chanspec_t chanspec)
{
	phytbl_info_t tab;
	uint8 spur_weight;
	uint8 channel = CHSPEC_CHANNEL(chanspec); /* see wlioctl.h */
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif


	tab.tbl_ptr = &spur_weight; /* ptr to buf */
	tab.tbl_len = 1;        /* # values   */
	tab.tbl_id = SSLPNPHY_TBL_ID_SPUR;
	tab.tbl_width = 8;     /* bit width */

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {

		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {

			uint32 curr_clocks, macintmask;

			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 400)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, nfSubtractVal_40, 380)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 3)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 3)
				PHY_REG_MOD_ENTRY(SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 5)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, PwrThresh0_40, SlowPwrLoThresh, 8)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, SlowPwrLoThresh, 6)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, SlowPwrLoThresh, 6)
			PHY_REG_LIST_EXECUTE(pi);

#if !defined(ROMTERMPHY)
			macintmask = wlapi_intrsoff(pi->sh->physhim);
#else
			macintmask = wl_intrsoff(((wlc_info_t *)(pi->wlc))->wl);
#endif
			curr_clocks = si_pmu_cpu_clock(GENERIC_PHY_INFO(pi)->sih,
				GENERIC_PHY_INFO(pi)->osh);
#if !defined(ROMTERMPHY)
			wlapi_intrsrestore(pi->sh->physhim, macintmask);
#else
			wl_intrsrestore(((wlc_info_t *)(pi->wlc))->wl, macintmask);
#endif
			if (curr_clocks == 300000000) {
				/* For cpu freq of 300MHz */
				if (channel == 3) {
					spur_weight = 3;
					tab.tbl_offset = 58; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 5) {
					spur_weight = 4;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 6) {
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 6);
				} else if (channel == 7) {
					spur_weight = 3;
					tab.tbl_offset = 186; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 26; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 6);
				} else if (channel == 8) {
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 4);
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 170; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 6);
				} else if (channel == 9) {
					spur_weight = 3;
					tab.tbl_offset = 90; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 122; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 6);
				} else if (channel == 10) {
					spur_weight = 3;
					tab.tbl_offset = 106; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 11) {
					spur_weight = 3;
					tab.tbl_offset = 90; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 13) {
					spur_weight = 3;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 6);
				} else if (channel == 14) {
					spur_weight = 4;
					tab.tbl_offset = 179; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 6);
				}
			} else {
				/* For cpu freq of 333MHz and rest of the
				 * frequencies.
				 */
				if (channel <= 8 || channel >= 13) {
					PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 7);
				} else {
					PHY_REG_MOD(pi, SSLPNPHY, PwrThresh0, SlowPwrLoThresh, 5);
				}

				if (channel == 1) {
					spur_weight = 3;
					tab.tbl_offset = 143; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 3) {
					spur_weight = 3;
					tab.tbl_offset = 175; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 111; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 4) {
					spur_weight = 3;
					tab.tbl_offset = 95; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 42; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 5) {
					spur_weight = 3;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 26; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 6) {
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 10; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 42; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 7) {
					spur_weight = 3;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 186; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 122; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 26; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 8) {
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 170; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 106; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 10; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 42; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 9) {
					spur_weight = 3;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 186; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 90; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 122; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 47; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 26; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 10) {
					spur_weight = 3;
					tab.tbl_offset = 138; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 170; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 74; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 106; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 10; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 31; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 11) {
					spur_weight = 3;
					tab.tbl_offset = 186; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 143; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 90; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 122; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					tab.tbl_offset = 15; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 12) {
					spur_weight = 3;
					tab.tbl_offset = 170; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 13) {
					spur_weight = 4;
					tab.tbl_offset = 154; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					spur_weight = 3;
					tab.tbl_offset = 175; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				} else if (channel == 14) {
					spur_weight = 4;
					tab.tbl_offset = 179; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
					spur_weight = 3;
					tab.tbl_offset = 190; /* tbl offset */
					wlc_sslpnphy_write_table(pi, &tab);
				}

			}
		} else {
		/* need to optimize this */
		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 360)
			PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, nfSubtractVal_40, 320)
		PHY_REG_LIST_EXECUTE(pi);

		if (channel == 1) {
			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252);
		} else if (channel == 2) {
			spur_weight = 5;
			tab.tbl_offset = 154; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 166; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
			PHY_REG_LIST_EXECUTE(pi);

		} else if (channel == 3) {
			spur_weight = 5;
			tab.tbl_offset = 138; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
			PHY_REG_LIST_EXECUTE(pi);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 5;
				tab.tbl_offset = 10; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, dsssPwrThresh1_20U, dsssPwrThresh2, 32)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 252)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, PwrThresh0_40, SlowPwrLoThresh, 11)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, SlowPwrLoThresh, 12)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, SlowPwrLoThresh, 11)
				PHY_REG_LIST_EXECUTE(pi);
			}
		} else if (channel == 4) {
			spur_weight = 5;
			tab.tbl_offset = 185; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
			PHY_REG_LIST_EXECUTE(pi);
		} else if (channel == 5) {
			spur_weight = 5;
			tab.tbl_offset = 170; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
			PHY_REG_LIST_EXECUTE(pi);
		} else if (channel == 6) {
			spur_weight = 2;
			tab.tbl_offset = 138; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 253);
		} else if (channel == 7) {
			spur_weight = 4;
			tab.tbl_offset = 154; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 3;
			tab.tbl_offset = 185; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 166; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
			PHY_REG_LIST_EXECUTE(pi);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 5;
				tab.tbl_offset = 26; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 4;
				tab.tbl_offset = 54; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 5;
				tab.tbl_offset = 74; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, dsssPwrThresh1_20U, dsssPwrThresh2, 32)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, dsssPwrThresh1_20L, dsssPwrThresh2, 32)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 251)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, PwrThresh0_40, SlowPwrLoThresh, 11)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, SlowPwrLoThresh, 12)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, SlowPwrLoThresh, 12)
				PHY_REG_LIST_EXECUTE(pi);
			}
		} else if (channel == 8) {
			spur_weight = 5;
			tab.tbl_offset = 138; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 170; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
			PHY_REG_LIST_EXECUTE(pi);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 3;
				tab.tbl_offset = 10; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 3;
				tab.tbl_offset = 106; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);
			}
		} else if (channel == 9) {
			spur_weight = 5;
			tab.tbl_offset = 185; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
			PHY_REG_LIST_EXECUTE(pi);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 5;
				tab.tbl_offset = 121; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, dsssPwrThresh1_20L, dsssPwrThresh2, 32)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 252)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, PwrThresh0_40, SlowPwrLoThresh, 11)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, SlowPwrLoThresh, 11)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, SlowPwrLoThresh, 12)
				PHY_REG_LIST_EXECUTE(pi);
			}
		} else if (channel == 10) {
			spur_weight = 5;
			tab.tbl_offset = 170; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 150; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
			PHY_REG_LIST_EXECUTE(pi);
		} else if (channel == 11) {
			spur_weight = 2;
			tab.tbl_offset = 143; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 253);

			if (CHSPEC_IS40(chanspec)) {
				spur_weight = 5;
				tab.tbl_offset = 42; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 2;
				tab.tbl_offset = 38; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 5;
				tab.tbl_offset = 90; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				spur_weight = 2;
				tab.tbl_offset = 86; /* tbl offset */
				wlc_sslpnphy_write_table(pi, &tab);

				PHY_REG_LIST_START
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, dsssPwrThresh1_20U, dsssPwrThresh2, 32)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, dsssPwrThresh1_20L, dsssPwrThresh2, 32)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 252)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, PwrThresh0_40, SlowPwrLoThresh, 11)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20U, SlowPwrLoThresh, 12)
					PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, transFreeThresh_20L, SlowPwrLoThresh, 12)
				PHY_REG_LIST_EXECUTE(pi);
			}
		} else if (channel == 12) {
			spur_weight = 5;
			tab.tbl_offset = 154; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 166; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 251)
			PHY_REG_LIST_EXECUTE(pi);
		} else if (channel == 13) {
			spur_weight = 5;
			tab.tbl_offset = 154; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			spur_weight = 2;
			tab.tbl_offset = 138; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 251)
			PHY_REG_LIST_EXECUTE(pi);
		} else if (channel == 14) {
			spur_weight = 3;
			tab.tbl_offset = 179; /* tbl offset */
			wlc_sslpnphy_write_table(pi, &tab);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, dsssPwrThresh1, dsssPwrThresh2, 32)
				PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 251)
			PHY_REG_LIST_EXECUTE(pi);
		}

	}


	if (sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4) {
		if (CHSPEC_IS40(chanspec)) {
			uint8 i;

			write_radio_reg(pi, RADIO_2063_GRX_1ST_1, 0x33);

			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, gainMismatchMedGainEx_40,
					medHiGainDirectMismatchOFDMDet, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY_Rev2, ClipCtrThresh_40, ClipCtrThreshHiGain, 30)
			PHY_REG_LIST_EXECUTE(pi);

			if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA))
				PHY_REG_MOD(pi, SSLPNPHY, gainidxoffset, ofdmgainidxtableoffset, 3);
			else
				PHY_REG_MOD(pi, SSLPNPHY, gainidxoffset, ofdmgainidxtableoffset, 244);

			for (i = 0; i <= 18; i++) {
				if ((i <= 7) || (i == 9) || (i == 14) || (i == 18))
					wlc_sslpnphy_rx_gain_table_tweaks(pi, i,
						SMIC_ELNA_535640Mhz_OFDM_GAINTBL_TWEAKS,
						SMIC_ELNA_535640Mhz_OFDM_GAINTBL_TWEAKS_sz);
			}
		} else {
			PHY_REG_MOD(pi, SSLPNPHY, HiGainDB, HiGainDB, 67);
			if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA))
				PHY_REG_MOD(pi, SSLPNPHY, LowGainDB, MedLowGainDB, 38);
		}
	}

	}
}
void
wlc_sslpnphy_set_chanspec_tweaks(phy_info_t *pi, chanspec_t chanspec)
{
	uint8 spur_weight;
	uint8 channel = CHSPEC_CHANNEL(chanspec); /* see wlioctl.h */
	uint8 phybw40 = IS40MHZ(pi);
	uint32 boardtype = BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype);
#ifdef BAND5G
	uint freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif
	/* Below are some of the settings required for reducing
	   the spur levels on the 4329 reference board
	 */

	if (!NORADIO_ENAB(pi->pubpi)) {
		if ((SSLPNREV_LT(pi->pubpi.phy_rev, 2)) &&
			(CHSPEC_IS2G(pi->radio_chanspec))) {
			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 0xfff, ((0x8 << 0) | (0x1f << 6)));
		}
#ifdef BAND5G
	if (CHSPEC_IS5G(pi->radio_chanspec)) {
		if (!sdna_board_flag)
			PHY_REG_MOD(pi, SSLPNPHY, HiGainDB, HiGainDB, 70);
		PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 0);
	if (!sdna_board_flag) {
		if (freq < 5000) {
			 PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 350);
		} else if (freq < 5180) {
			PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 320);
		} else if (freq < 5660) {
			if (freq <= 5500)
				PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 320);
			else
				PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 300);
		} else {
			    PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, 240);
		}
	} else {
		/* Putting static nfSubtractVal as 350 for all 5G channels in sdna board */
		/* This helps us fix the 4dB sens loss in higher 5G channels */
		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 350)
			PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, nfSubtractVal_40, 350)
		PHY_REG_LIST_EXECUTE(pi);
	}

		if (boardtype == BCM94329OLYMPICX17_SSID ||
		    boardtype == BCM94329OLYMPICX17M_SSID ||
		    boardtype == BCM94329OLYMPICX17U_SSID) {
			if (freq <= 5500) {
				PHY_REG_LIST_START
					PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 400)
					PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 7)
				PHY_REG_LIST_EXECUTE(pi);
				if (boardtype == BCM94329OLYMPICX17U_SSID) {
					PHY_REG_LIST_START
						PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 320)
						PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 3)
					PHY_REG_LIST_EXECUTE(pi);
				}
			} else if (freq < 5660) {
				PHY_REG_LIST_START
					PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 320)
					PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 2)
				PHY_REG_LIST_EXECUTE(pi);
				if (boardtype == BCM94329OLYMPICX17U_SSID) {
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 254);
				}
			} else if (freq < 5775) {
				if (boardtype == BCM94329OLYMPICX17M_SSID) {
					PHY_REG_LIST_START
						PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 300)
						PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252)
					PHY_REG_LIST_EXECUTE(pi);
				} else if (boardtype == BCM94329OLYMPICX17U_SSID) {
					PHY_REG_LIST_START
						PHY_REG_WRITE_ENTRY(SSLPNPHY, nfSubtractVal, 280)
						PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, 0)
					PHY_REG_LIST_EXECUTE(pi);
				}
			} else if (freq >= 5775) {
				PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 252);
			}
		}
	} else
#endif /* BAND5G */
	 {
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
			if (!(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {
				if (PHY_XTALFREQ(pi->xtalfreq) == 38400000) {
					if (boardtype == BCM94329TDKMDL11_SSID)
						wlc_sslpnphy_chanspec_spur_weight(pi, channel,
							chan_spec_spur_tdkmdl_38p4Mhz,
							chan_spec_spur_tdkmdl_38p4Mhz_sz);
					else
						wlc_sslpnphy_chanspec_spur_weight(pi, channel,
							chan_spec_spur_nokref_38p4Mhz,
							chan_spec_spur_nokref_38p4Mhz_sz);
				} else if (PHY_XTALFREQ(pi->xtalfreq) == 26000000) {
					wlc_sslpnphy_chanspec_spur_weight(pi, channel,
						chan_spec_spur_26Mhz, chan_spec_spur_26Mhz_sz);
				} else if (PHY_XTALFREQ(pi->xtalfreq) == 37400000)
					wlc_sslpnphy_chanspec_spur_weight(pi, channel,
						chan_spec_spur_37p4Mhz,
						chan_spec_spur_37p4Mhz_sz);

				PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 253);
				if (PHY_XTALFREQ(pi->xtalfreq) == 38400000) {
				if (channel <= 4) {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam1, ofMaxPThrUpdtThresh, 0)
						PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam2, oFiltSyncCtrShft, 2)
						PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmSyncThresh0, ofdmSyncThresh0, 120)
					PHY_REG_LIST_EXECUTE(pi);
				} else
					PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 254);

				} else if (PHY_XTALFREQ(pi->xtalfreq) == 37400000) {
				if ((channel >= 3) && (channel <= 6)) {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam1, ofMaxPThrUpdtThresh, 0)
						PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam2, oFiltSyncCtrShft, 2)
						PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmSyncThresh0, ofdmSyncThresh0, 120)
					PHY_REG_LIST_EXECUTE(pi);
				}
				} else {
				if (((channel <= 4) && (channel != 2)) ||
					((channel >= 11) && (channel <= 13))) {
					PHY_REG_LIST_START
						PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam1, ofMaxPThrUpdtThresh, 0)
						PHY_REG_MOD_ENTRY(SSLPNPHY, lnsrOfParam2, oFiltSyncCtrShft, 2)
						PHY_REG_MOD_ENTRY(SSLPNPHY, ofdmSyncThresh0, ofdmSyncThresh0, 120)
						PHY_REG_MOD_ENTRY(SSLPNPHY, InputPowerDB, inputpwroffsetdb, (252))
					PHY_REG_LIST_EXECUTE(pi);
				}
				}
			} else if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) {
				if (PHY_XTALFREQ(pi->xtalfreq) == 38400000)
					wlc_sslpnphy_chanspec_spur_weight(pi, channel,
						chan_spec_spur_xtlna38p4Mhz,
						chan_spec_spur_xtlna38p4Mhz_sz);
				else if (PHY_XTALFREQ(pi->xtalfreq) == 26000000)
					wlc_sslpnphy_chanspec_spur_weight(pi, channel,
						chan_spec_spur_xtlna26Mhz,
						chan_spec_spur_xtlna26Mhz_sz);
				else if (PHY_XTALFREQ(pi->xtalfreq) == 37400000)
					wlc_sslpnphy_chanspec_spur_weight(pi, channel,
						chan_spec_spur_xtlna37p4Mhz,
						chan_spec_spur_xtlna37p4Mhz_sz);
				PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, ((channel != 14) ? 1 : 0));
			}

		} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			wlc_sslpnphy_set_chanspec_tweaks_5356(pi, pi->radio_chanspec);
		} else if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) { 	/* 4319 SSLPNPHY REV > 2 */
			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 254);

			if (phybw40 == 1) {
				PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 1);
			}
			if (PHY_XTALFREQ(pi->xtalfreq) == 30000000) {
				if (channel == 13) {
					spur_weight = 2;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
						&spur_weight, 1, 8, 153);
					spur_weight = 2;
					wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SPUR,
						&spur_weight, 1, 8, 154);
				}
			}
		}
	}
	}
}

void
BCMROMFN(wlc_sslpnphy_pktengtx)(wlc_phy_t *ppi, wl_pkteng_t *pkteng, uint8 rate,
	struct ether_addr *sa, uint32 wait_delay)
{
	phy_info_t *pi = (phy_info_t *)ppi;
	uint8 counter = 0;
	uint16 max_pwr_idx = 0;
	uint16 min_pwr_idx = 127;
	uint16 current_txidx = 0;
	uint32 ant_ovr;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	sslpnphy_specific->sslpnphy_psat_2pt3_detected = 0;
	wlc_sslpnphy_btcx_override_enable(pi);
	wlc_sslpnphy_set_deaf(pi);

	/* Force default antenna */
	ant_ovr = wlc_sslpnphy_set_ant_override(pi, wlc_phy_get_txant(pi));

	for (counter = 0; counter < pkteng->nframes; counter ++) {
		wlc_phy_do_dummy_tx(pi, TRUE, OFF);
		OSL_DELAY(pkteng->delay);
		current_txidx = wlc_sslpnphy_get_current_tx_pwr_idx(pi);
		if (current_txidx > max_pwr_idx)
			max_pwr_idx = current_txidx;
		if (current_txidx < min_pwr_idx)
			min_pwr_idx = current_txidx;
	}
	wlc_sslpnphy_clear_deaf(pi);

	/* Restore antenna override */
	wlc_sslpnphy_restore_ant_override(pi, ant_ovr);

	if (pkteng->nframes == 100) {
		/* 10 is the value chosen for a start power of 14dBm */
		if (!((max_pwr_idx == 0) && (min_pwr_idx == 127))) {
			if (((max_pwr_idx - min_pwr_idx) > 10) ||
			(min_pwr_idx == 0)) {
				sslpnphy_specific->sslpnphy_psat_2pt3_detected = 1;
				current_txidx =  max_pwr_idx;
			}
		}
	}
	sslpnphy_specific->sslpnphy_start_idx = (uint8)current_txidx; 	/* debug information */

	WL_INFORM(("wl%d: %s: Max idx %d  Min idx %d \n", GENERIC_PHY_INFO(pi)->unit,
		__FUNCTION__, max_pwr_idx, min_pwr_idx));
}

void
wlc_sslpnphy_papd_recal(phy_info_t *pi)
{

#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	wlc_phy_t * ppi = (wlc_phy_t *)pi;
	uint16 tx_pwr_ctrl;
	bool suspend;
	uint16 current_txidx = 0;
	wl_pkteng_t pkteng;
	struct ether_addr sa;
	uint8 phybw40;
	uint8 channel = CHSPEC_CHANNEL(pi->radio_chanspec); /* see wlioctl.h */
	sslpnphy_txcalgains_t txgains;
	phybw40 = IS40MHZ(pi);

	suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend) {
#if defined(ROMTERMPHY) && defined(WLMEDIA_APCS)
		/* Set non-zero duration for CTS-to-self */
		WL_WRITE_SHM(pi, M_CTS_DURATION, 6500);
#else
		/* Set non-zero duration for CTS-to-self */
		WL_WRITE_SHM(pi, M_CTS_DURATION, 10000);
#endif /* defined(ROMTERMPHY) && defined(WLMEDIA_APCS) */
		WL_SUSPEND_MAC_AND_WAIT(pi);
	}

	/* temporary arrays needed in child functions of papd cal */
	sslpnphy_specific->sslpnphy_papdIntlut = (uint32 *)MALLOC(GENERIC_PHY_INFO(pi)->osh, 128 * sizeof(uint32));
	sslpnphy_specific->sslpnphy_papdIntlutVld = (uint8 *)MALLOC(GENERIC_PHY_INFO(pi)->osh, 128 * sizeof(uint8));

	/* if we dont have enough memory, then exit gracefully */
	if ((sslpnphy_specific->sslpnphy_papdIntlut == NULL) || (sslpnphy_specific->sslpnphy_papdIntlutVld == NULL)) {
		if (sslpnphy_specific->sslpnphy_papdIntlut != NULL) {
			MFREE(GENERIC_PHY_INFO(pi)->osh,
			      sslpnphy_specific->sslpnphy_papdIntlut, 128 * sizeof(uint32));
		}
		if (sslpnphy_specific->sslpnphy_papdIntlutVld != NULL) {
			MFREE(GENERIC_PHY_INFO(pi)->osh,
			      sslpnphy_specific->sslpnphy_papdIntlutVld, 128 * sizeof(uint8));
		}
		WL_ERROR(("wl%d: %s: MALLOC failure\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));
		return;
	}

	if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4329_CHIP_ID)
		si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 2, 0x00000007, 0x0);
	if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)
		si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 2, 0x00000007, 0x6);
	if (NORADIO_ENAB(pi->pubpi)) {
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, txfefilterctrl, txfefilterconfig_en, 0)
				PHY_REG_MOD2_ENTRY(SSLPNPHY, txfefilterconfig,
					cmpxfilt_use_ofdmcoef_4ht, realfilt_use_ofdmcoef_4ht, 1, 1)
			PHY_REG_LIST_EXECUTE(pi);
		}
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, Core1TxControl, txcomplexfilten, 0)
			PHY_REG_MOD_ENTRY(SSLPNPHY, papd_control, papdCompEn, 0)
		PHY_REG_LIST_EXECUTE(pi);

		return;
	}

#ifdef PS4319XTRA
	if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)
		 WL_WRITE_SHM(pi, M_PS4319XTRA, 0);
#endif /* PS4319XTRA */
	if ((SSLPNREV_LT(pi->pubpi.phy_rev, 2)) && (CHSPEC_IS2G(pi->radio_chanspec))) {
		/* cellular emission fixes */
		write_radio_reg(pi, RADIO_2063_LOGEN_BUF_1, sslpnphy_specific->sslpnphy_logen_buf_1);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVR_2, sslpnphy_specific->sslpnphy_local_ovr_2);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_6, sslpnphy_specific->sslpnphy_local_oval_6);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_5, sslpnphy_specific->sslpnphy_local_oval_5);
		write_radio_reg(pi, RADIO_2063_LOGEN_MIXER_1, sslpnphy_specific->sslpnphy_logen_mixer_1);
	}
	if ((channel != 14) && (!wlc_sslpnphy_fcc_chan_check(pi, channel)) && IS_OLYMPIC(pi)) {
		/* Resetting all Olympic related microcode settings */
		WL_WRITE_SHM(pi, M_SSLPN_OLYMPIC, 0);
		PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl0, sslpnphy_specific->sslpnphy_extstxctrl0);
		PHY_REG_WRITE(pi, SSLPNPHY, extstxctrl1, sslpnphy_specific->sslpnphy_extstxctrl1);
	}

	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) &&
		(sslpnphy_specific->sslpnphy_papd_peakcurmode.predict_cal_count >= MAX_PREDICT_CAL)) {
		sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_at_init_done = FALSE;
		sslpnphy_specific->sslpnphy_papd_peakcurmode.predict_cal_count = 0;
	}

	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) &&
		(FALSE == sslpnphy_specific->peak_current_mode) &&
		(TRUE == sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_at_init_done)) {
		current_txidx = wlc_sslpnphy_papd_fixpwr_calidx_find(pi, 56);
		sslpnphy_specific->sslpnphy_start_idx = (uint8)current_txidx; 	/* debug information */
		sslpnphy_specific->sslpnphy_papd_peakcurmode.predict_cal_count++;
	} else {
	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);
	/* Restore pwr ctrl */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);

	wlc_sslpnphy_clear_tx_power_offsets(pi);
	wlc_sslpnphy_set_target_tx_pwr(pi, 56);
	/* Setting npt to 0 for index settling with 30 frames */
	wlc_sslpnphy_set_tx_pwr_npt(pi, 0);

	if (!sslpnphy_specific->sslpnphy_force_1_idxcal) {
		/* Enabling Complex filter before transmitting dummy frames */
		/* Check if this is redundant because ucode already does this */
		PHY_REG_MOD3(pi, SSLPNPHY, sslpnCalibClkEnCtrl, papdRxClkEn, papdTxClkEn, papdFiltClkEn, 1, 1, 1);
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 0)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, Core1TxControl, txcomplexfilten, 1)
				PHY_REG_MOD_ENTRY(SSLPNPHY, papd_control, papdCompEn, 0)
			PHY_REG_LIST_EXECUTE(pi);
		}

		if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
			PHY_REG_LIST_START
				PHY_REG_MOD3_ENTRY(SSLPNPHY, Core1TxControl,
					txrealfilten, txcomplexfilten, txcomplexfiltb4papd,
					1, 1, 1)
				PHY_REG_MOD_ENTRY(SSLPNPHY, papd_control, papdCompEn, 0)
			PHY_REG_LIST_EXECUTE(pi);
		}
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			PHY_REG_LIST_START
				PHY_REG_MOD_ENTRY(SSLPNPHY, txfefilterctrl, txfefilterconfig_en, 1)
				PHY_REG_MOD3_ENTRY(SSLPNPHY, txfefilterconfig,
					ofdm_cmpxfilten, ofdm_realfilten, ofdm_papden,
					1, 1, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverrideVal0, ant_selp_ovr_val, 0)
				PHY_REG_MOD_ENTRY(SSLPNPHY, RFOverride0, ant_selp_ovr, 0)
			PHY_REG_LIST_EXECUTE(pi);

		}
		/* clear our PAPD Compensation table */
		wlc_sslpnphy_clear_papd_comptable(pi);

		PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlDeltaPwrLimit, DeltaPwrLimit, 3);

		if (SSLPNREV_LE(pi->pubpi.phy_rev, 1)) {
			write_radio_reg(pi, RADIO_2063_PA_CTRL_14, 0xee);
			PHY_REG_MOD(pi, SSLPNPHY, Core1TxControl, txrealfiltcoefsel, 1);
		}
		current_txidx = wlc_sslpnphy_get_current_tx_pwr_idx(pi);
		if (!sslpnphy_specific->sslpnphy_restore_papd_cal_results) {
			{
				sa.octet[0] = 10;

				pkteng.flags = WL_PKTENG_PER_TX_START;
				pkteng.delay = 2;		/* Inter packet delay */
				pkteng.nframes = 50;		/* number of frames */
				pkteng.length = 0;		/* packet length */
				pkteng.seqno = FALSE;	/* enable/disable sequence no. */

				wlc_sslpnphy_pktengtx(ppi, &pkteng, 108, &sa, (1000*10));
				sslpnphy_specific->sslpnphy_dummy_tx_done = 1;
			}
			/* sending out 100 frames to caluclate min & max index */
			if (VBAT_RIPPLE_CHECK(pi)) {
				pkteng.delay = 30;		/* Inter packet delay */
				pkteng.nframes = 100;		/* number of frames */
				wlc_sslpnphy_pktengtx(ppi, &pkteng, 108, &sa, (1000*10));
			}
			current_txidx = sslpnphy_specific->sslpnphy_start_idx; 	/* debug information */
				sslpnphy_specific->sslpnphy_papd_peakcurmode.tgtPapdCalpwr_idx =
					sslpnphy_specific->sslpnphy_start_idx;
				sslpnphy_specific->sslpnphy_papd_peakcurmode.tgtPapdCalpwr =
					wlc_sslpnphy_get_target_tx_pwr(pi);
		}
	}
		/* Setting npt to 1 for normal transmission */
		wlc_sslpnphy_set_tx_pwr_npt(pi, 1);
	}
	/* disabling complex filter for PAPD calibration */
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 0)) {
		PHY_REG_MOD(pi, SSLPNPHY, Core1TxControl, txcomplexfilten, 0);
	}

	PHY_REG_MOD3(pi, SSLPNPHY, sslpnCalibClkEnCtrl, papdFiltClkEn, papdTxClkEn, papdRxClkEn, 0, 0, 0);

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		PHY_REG_MOD3(pi, SSLPNPHY, Core1TxControl,
			txcomplexfilten, txrealfilten, txcomplexfiltb4papd,
			0, 0, 0);
	}
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, txfefilterctrl, txfefilterconfig_en, 0)
			PHY_REG_MOD2_ENTRY(SSLPNPHY, txfefilterconfig, ofdm_cmpxfilten, ofdm_realfilten, 0, 0)
		PHY_REG_LIST_EXECUTE(pi);
	}
	wlc_sslpnphy_set_deaf(pi);

	if (!sslpnphy_specific->sslpnphy_restore_papd_cal_results) {
		if (!NON_BT_CHIP(wlc))
			wlc_sslpnphy_btcx_override_enable(pi);
	}

	/* Save tx power control mode */
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	sslpnphy_specific->sslpnphy_papd_rxGnCtrl_init = 0;

	txgains.useindex = TRUE;
	txgains.index = (uint8) current_txidx;
	if (!sslpnphy_specific->sslpnphy_restore_papd_cal_results) {
		if (!sslpnphy_specific->sslpnphy_force_1_idxcal)
			if (((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) &&
				VBAT_RIPPLE_CHECK(pi)) ||
				(!(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID)))
			wlc_sslpnphy_vbatsense_papd_cal(pi, SSLPNPHY_PAPD_CAL_CW, &txgains);

		wlc_sslpnphy_papd_cal_txpwr(pi, SSLPNPHY_PAPD_CAL_CW, FALSE, TRUE, current_txidx);
	} else {
		wlc_sslpnphy_restore_papd_calibration_results(pi);
	}
	/* Restore tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);

	PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlDeltaPwrLimit, DeltaPwrLimit, 1);

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 0)) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, Core1TxControl, txcomplexfilten, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, papd_control, papdCompEn, 0)
		PHY_REG_LIST_EXECUTE(pi);
	}

	PHY_REG_MOD3(pi, SSLPNPHY, sslpnCalibClkEnCtrl, papdFiltClkEn, papdTxClkEn, papdRxClkEn, 1, 1, 1);

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		PHY_REG_LIST_START
			PHY_REG_MOD3_ENTRY(SSLPNPHY, Core1TxControl,
				txrealfilten, txcomplexfiltb4papd, txcomplexfilten,
				1, 1, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, papd_control, papdCompEn, 1)
		PHY_REG_LIST_EXECUTE(pi);

		if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329TDKMDL11_SSID) {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipBpsk, 0x078f)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipQpsk, 0x078f)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_MOD(pi, SSLPNPHY, txfiltctrl, txcomplexfiltb4papd, 1);

		phy_reg_mod(pi, SSLPNPHY_txfefilterconfig,
			(SSLPNPHY_txfefilterconfig_cmpxfilt_use_ofdmcoef_4ht_MASK |
			SSLPNPHY_txfefilterconfig_realfilt_use_ofdmcoef_4ht_MASK |
			SSLPNPHY_txfefilterconfig_ofdm_papden_MASK |
			SSLPNPHY_txfefilterconfig_ht_papden_MASK |
			SSLPNPHY_txfefilterconfig_cck_realfilten_MASK |
			SSLPNPHY_txfefilterconfig_cck_cmpxfilten_MASK |
			SSLPNPHY_txfefilterconfig_ofdm_cmpxfilten_MASK |
			SSLPNPHY_txfefilterconfig_ofdm_realfilten_MASK |
			SSLPNPHY_txfefilterconfig_ht_cmpxfilten_MASK |
			SSLPNPHY_txfefilterconfig_ht_realfilten_MASK),
			(((!phybw40) << SSLPNPHY_txfefilterconfig_cmpxfilt_use_ofdmcoef_4ht_SHIFT) |
			((!phybw40) << SSLPNPHY_txfefilterconfig_realfilt_use_ofdmcoef_4ht_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ofdm_papden_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ht_papden_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_cck_realfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_cck_cmpxfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ofdm_cmpxfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ofdm_realfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ht_cmpxfilten_SHIFT) |
			(1 << SSLPNPHY_txfefilterconfig_ht_realfilten_SHIFT)));

		PHY_REG_MOD(pi, SSLPNPHY, txfefilterctrl, txfefilterconfig_en, 1);

		if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) &&
			(FALSE == sslpnphy_specific->peak_current_mode)) {
			PHY_REG_MOD2(pi, SSLPNPHY, papd_control, papdCompEn, papd_lut_update_with_pd_in_tx, 0, 0);
		} else {
			PHY_REG_MOD(pi, SSLPNPHY, papd_control, papdCompEn, 0);
		}

		if (CHIPID(GENERIC_PHY_INFO(pi)->chip) != BCM5356_CHIP_ID) {
			PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 0);
		}

		PHY_REG_MOD(pi, SSLPNPHY, lpfbwlutreg1, lpfbwlut5, 2);
	}

	sslpnphy_specific->sslpnphy_papd_cal_done = 1;
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		PHY_REG_MOD(pi, SSLPNPHY, Core1TxControl, txClipEnable_ofdm, 1);

		if (phybw40 == 1) {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipBpsk, 0x0aff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipQpsk, 0x0bff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClip16Qam, 0x7fff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClip64Qam, 0x7fff)
			PHY_REG_LIST_EXECUTE(pi);
		} else { /* No clipping for 20Mhz */
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipBpsk, 0x7fff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipQpsk, 0x7fff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClip16Qam, 0x7fff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClip64Qam, 0x7fff)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}
	if (CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) {
		PHY_REG_MOD(pi, SSLPNPHY, Core1TxControl, txClipEnable_ofdm, 1);

		if (phybw40 == 1) {
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipBpsk, 0x0aff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipQpsk, 0x0bff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClip16Qam, 0x7fff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClip64Qam, 0x7fff)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			if ((SSLPNREV_IS(pi->pubpi.phy_rev, 3)) &&
				(sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4)) {
					PHY_REG_WRITE(pi, SSLPNPHY, txClipBpsk, 0x07ff);
			} else {
					PHY_REG_WRITE(pi, SSLPNPHY, txClipBpsk, 0x063f);
			}
			PHY_REG_LIST_START
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipQpsk, 0x071f)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClip16Qam, 0x7fff)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, txClip64Qam, 0x7fff)
			PHY_REG_LIST_EXECUTE(pi);
		}

	}
#if !defined(ROMTERMPHY)
	if ((CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) &&
		(GENERIC_PHY_INFO(pi)->chiprev == 0)) {
		/* tssi does not work on 5356a0; hard code tx power */
		wlc_sslpnphy_set_tx_pwr_by_index(pi, 50);
	}
#endif /* ROMTERMPHY */
	if ((SSLPNREV_LT(pi->pubpi.phy_rev, 2)) && (CHSPEC_IS2G(pi->radio_chanspec))) {
		/* cellular emission fixes */
		write_radio_reg(pi, RADIO_2063_LOGEN_BUF_1, 0x06);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVR_2, 0x0f);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_6, 0xff);
		write_radio_reg(pi, RADIO_2063_LOCAL_OVAL_5, 0xff);
		write_radio_reg(pi, RADIO_2063_LOGEN_MIXER_1, 0x66);
	}
	if (IS_OLYMPIC(pi)) {
		uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
		uint16 olympic_flag;
		olympic_flag = WL_READ_SHM(pi, M_SSLPN_OLYMPIC);
		WL_WRITE_SHM(pi, M_SSLPN_OLYMPIC, olympic_flag | 1);
		if (channel != 14) {
			if (!(wlc_sslpnphy_fcc_chan_check(pi, channel))) {
				if ((sslpnphy_specific->sslpnphy_fabid == 2) ||
					(sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_16_64)), 0x1600);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_16_64)), 0x3300);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_2_4)), 0x1550);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_2_4)), 0x3300);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_CCK)), 0x2500);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_CCK)), 0x30c0);
				} else {
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_16_64)), 0x9210);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_16_64)), 0x3150);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_2_4)), 0xf000);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_2_4)), 0x30c3);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F2_CCK)), 0x2280);
					WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
						M_SSLPNPHY_REG_4F3_CCK)), 0x3150);
				}

				olympic_flag = WL_READ_SHM(pi, M_SSLPN_OLYMPIC);
				WL_WRITE_SHM(pi, M_SSLPN_OLYMPIC,
					(olympic_flag | (0x1 << 1)));
			}
		}

	}

	if (channel == 14) {
		PHY_REG_LIST_START
			PHY_REG_MOD_ENTRY(SSLPNPHY, lpfbwlutreg0, lpfbwlut0, 2)
			PHY_REG_MOD_ENTRY(SSLPNPHY, papd_control, papdCompEn, 0)
		PHY_REG_LIST_EXECUTE(pi);

		/* Disable complex filter for 4329 and 4319 */
		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2))
			PHY_REG_MOD(pi, SSLPNPHY, Core1TxControl, txcomplexfilten, 0);
		else
			PHY_REG_MOD(pi, SSLPNPHY, txfefilterconfig, cck_cmpxfilten, 0);
	} else
		PHY_REG_WRITE(pi, SSLPNPHY, lpfbwlutreg0, sslpnphy_specific->sslpnphy_filt_bw);

	wlc_sslpnphy_tempsense(pi);
	sslpnphy_specific->sslpnphy_last_cal_temperature = sslpnphy_specific->sslpnphy_lastsensed_temperature;
	sslpnphy_specific->sslpnphy_last_full_cal_temperature = sslpnphy_specific->sslpnphy_lastsensed_temperature;

	wlc_sslpnphy_reset_radioctrl_crsgain_nonoverlay(pi);

	if ((CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4329_CHIP_ID) ||
		(CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID))
		si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 2, 0x00000007, 0x00000005);
#ifdef PS4319XTRA
	if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)
		WL_WRITE_SHM(pi, M_PS4319XTRA, PS4319XTRA);
#endif /* PS4319XTRA */

	if (sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_at_init_done == FALSE) {
		PHY_REG_MOD(pi, SSLPNPHY, Core1TxControl, txClipEnable_ofdm, 3);
		/* Setting npt to 0 for index settling with 30 frames */
		wlc_sslpnphy_set_tx_pwr_npt(pi, 0);

		current_txidx = wlc_sslpnphy_get_current_tx_pwr_idx(pi);
		sa.octet[0] = 10;
		pkteng.flags = WL_PKTENG_PER_TX_START;
		pkteng.delay = 2;		/* Inter packet delay */
		pkteng.nframes = 50;		/* number of frames */
		pkteng.length = 0;		/* packet length */
		pkteng.seqno = FALSE;	/* enable/disable sequence no. */

		wlc_sslpnphy_pktengtx(ppi, &pkteng, 108, &sa, (1000*10));
		sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_tgtpwr_idx =
			sslpnphy_specific->sslpnphy_start_idx;
		sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_tgtpwr = wlc_sslpnphy_get_target_tx_pwr(pi);
		/* Setting npt to 0 for index settling with 30 frames */
		wlc_sslpnphy_set_tx_pwr_npt(pi, 1);
		PHY_REG_MOD(pi, SSLPNPHY, Core1TxControl, txClipEnable_ofdm, 1);

		sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_at_init_done = TRUE;
	}

	if (!suspend)
		WL_ENABLE_MAC(pi);
	wlc_sslpnphy_clear_deaf(pi);

	MFREE(GENERIC_PHY_INFO(pi)->osh, sslpnphy_specific->sslpnphy_papdIntlut, 128 * sizeof(uint32));
	MFREE(GENERIC_PHY_INFO(pi)->osh, sslpnphy_specific->sslpnphy_papdIntlutVld, 128 * sizeof(uint8));
}
static void
wlc_sslpnphy_load_filt_coeff(phy_info_t *pi, uint16 reg_address, uint16 *coeff_val, uint count)
{
	uint i;
	for (i = 0; i < count; i++)
		phy_reg_write(pi, reg_address + i, coeff_val[i]);
}
static void wlc_sslpnphy_ofdm_filt_load(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
			sslpnphy_cx_ofdm[sslpnphy_specific->sslpnphy_ofdm_filt_sel], 10);
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_ofdm_tap0,
			sslpnphy_real_ofdm[sslpnphy_specific->sslpnphy_ofdm_filt_sel], 5);
	}
}

void wlc_sslpnphy_cck_filt_load(phy_info_t *pi, uint8 filtsel)
{
	if (SSLPNREV_GT(pi->pubpi.phy_rev, 0)) {
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_cck_tap0_i,
			sslpnphy_cx_cck[filtsel], 10);
		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_cck_tap0,
				sslpnphy_real_cck[filtsel], 5);
		} else {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_cck_tap0,
				sslpnphy_real_cck[filtsel], 5);
		}
	}

}
static void
wlc_sslpnphy_restore_txiqlo_calibration_results(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint16 a, b;
	uint16 didq;
	uint32 val;
	uint idx;
	uint8 ei0, eq0, fi0, fq0;
	uint8 band_idx;

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	ASSERT(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs_valid);

	a = sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[0];
	b = sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[1];
	didq = sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[5];

	wlc_sslpnphy_set_tx_iqcc(pi, a, b);
	wlc_sslpnphy_set_tx_locc(pi, didq);

	/* restore iqlo portion of tx power control tables */
	/* remaining element */
	for (idx = 0; idx < 128; idx++) {
		/* iq */
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);
		val = (val & 0xfff00000) |
			((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);
		/* loft */
		val = didq;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_LO_OFFSET + idx);
	}
	/* Do not move the below statements up */
	/* We need atleast 2us delay to read phytable after writing radio registers */
	/* Apply analog LO */
	ei0 = (uint8)(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[7] >> 8);
	eq0 = (uint8)(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[7]);
	fi0 = (uint8)(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[9] >> 8);
	fq0 = (uint8)(sslpnphy_specific->sslpnphy_cal_results[band_idx].txiqlocal_bestcoeffs[9]);
	wlc_sslpnphy_set_radio_loft(pi, ei0, eq0, fi0, fq0);
}

void
wlc_sslpnphy_save_papd_calibration_results(phy_info_t *pi)
{
	uint8 band_idx;
	uint8 a, i;
	uint32 papdcompdeltatblval;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);
	/* Save papd calibration results */
	sslpnphy_specific->sslpnphy_cal_results[band_idx].analog_gain_ref = phy_reg_read(pi,
		SSLPNPHY_papd_tx_analog_gain_ref);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].lut_begin = phy_reg_read(pi,
		SSLPNPHY_papd_track_pa_lut_begin);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].lut_end = phy_reg_read(pi,
		SSLPNPHY_papd_track_pa_lut_end);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].lut_step = phy_reg_read(pi,
		SSLPNPHY_papd_track_pa_lut_step);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].rxcompdbm = phy_reg_read(pi,
		SSLPNPHY_papd_rx_gain_comp_dbm);
	sslpnphy_specific->sslpnphy_cal_results[band_idx].papdctrl = phy_reg_read(pi, SSLPNPHY_papd_control);
	/* Save papdcomp delta table */
	for (a = 1, i = 0; a < 128; a = a + 2, i ++) {
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
			&papdcompdeltatblval, 1, 32, a);
		sslpnphy_specific->sslpnphy_cal_results[band_idx].papd_compdelta_tbl[i] = papdcompdeltatblval;
	}
	sslpnphy_specific->sslpnphy_cal_results[band_idx].papd_table_valid = 1;
}

STATIC void
wlc_sslpnphy_restore_papd_calibration_results(phy_info_t *pi)
{
	uint8 band_idx;
	uint8 a, i;
	uint32 papdcompdeltatblval;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);
	if (sslpnphy_specific->sslpnphy_cal_results[band_idx].papd_table_valid) {
		/* Apply PAPD cal results */
		for (a = 1, i = 0; a < 128; a = a + 2, i ++) {
			papdcompdeltatblval = sslpnphy_specific->sslpnphy_cal_results
				[band_idx].papd_compdelta_tbl[i];
			wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_PAPDCOMPDELTATBL,
				&papdcompdeltatblval, 1, 32, a);
		}
		/* Writing the deltas */
		wlc_sslpnphy_compute_delta(pi);

		/* Restore saved papd regs */
		PHY_REG_WRITE(pi, SSLPNPHY, papd_tx_analog_gain_ref,
			sslpnphy_specific->sslpnphy_cal_results[band_idx].analog_gain_ref);
		PHY_REG_WRITE(pi, SSLPNPHY, papd_track_pa_lut_begin,
			sslpnphy_specific->sslpnphy_cal_results[band_idx].lut_begin);
		PHY_REG_WRITE(pi, SSLPNPHY, papd_track_pa_lut_end,
			sslpnphy_specific->sslpnphy_cal_results[band_idx].lut_end);
		PHY_REG_WRITE(pi, SSLPNPHY, papd_track_pa_lut_step,
			sslpnphy_specific->sslpnphy_cal_results[band_idx].lut_step);
		PHY_REG_WRITE(pi, SSLPNPHY, papd_rx_gain_comp_dbm,
			sslpnphy_specific->sslpnphy_cal_results[band_idx].rxcompdbm);
		PHY_REG_WRITE(pi, SSLPNPHY, papd_control,
			sslpnphy_specific->sslpnphy_cal_results[band_idx].papdctrl);
	}
}

static void
wlc_sslpnphy_restore_calibration_results(phy_info_t *pi)
{
	uint8 band_idx;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);

	wlc_sslpnphy_restore_txiqlo_calibration_results(pi);

	/* restore rx iq cal results */
	wlc_sslpnphy_set_rx_iq_comp(pi,
		sslpnphy_specific->sslpnphy_cal_results[band_idx].rxiqcal_coeffa0,
		sslpnphy_specific->sslpnphy_cal_results[band_idx].rxiqcal_coeffb0);

	PHY_REG_WRITE(pi, SSLPNPHY, RxIqCoeffCtrl,
		sslpnphy_specific->sslpnphy_cal_results[band_idx].rxiq_enable);
	PHY_REG_WRITE(pi, SSLPNPHY, rxfe, sslpnphy_specific->sslpnphy_cal_results[band_idx].rxfe);
	write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_1,
		sslpnphy_specific->sslpnphy_cal_results[band_idx].loopback1);
	write_radio_reg(pi, RADIO_2063_TXRX_LOOPBACK_2,
		sslpnphy_specific->sslpnphy_cal_results[band_idx].loopback2);

}

void
wlc_sslpnphy_percal_flags_off(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
	sslpnphy_specific->sslpnphy_recal = 0;
	sslpnphy_specific->sslpnphy_force_1_idxcal = 0;
	sslpnphy_specific->sslpnphy_vbat_ripple = 0;
	sslpnphy_specific->sslpnphy_percal_ctr = 0;
	sslpnphy_specific->sslpnphy_papd_nxt_cal_idx = 0;
	sslpnphy_specific->sslpnphy_tx_idx_prev_cal = 0;
	sslpnphy_specific->sslpnphy_txidx_drift = 0;
	sslpnphy_specific->sslpnphy_cur_idx = 0;
	sslpnphy_specific->sslpnphy_restore_papd_cal_results = 0;
	sslpnphy_specific->sslpnphy_dummy_tx_done = 0;
	sslpnphy_specific->sslpnphy_papd_cal_done = 0;
	sslpnphy_specific->sslpnphy_init_noise_cal_done = FALSE;
	sslpnphy_specific->sslpnphy_papd_tweaks_enable = FALSE;

	if ((sslpnphy_specific->sslpnphy_fabid == 2) || (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12))
		sslpnphy_specific->sslpnphy_radio_classA = TRUE;
	else
		sslpnphy_specific->sslpnphy_radio_classA = FALSE;

	/* For 4319, radio is set to classA at "wlc_sslpnphy_radio_2063_channel_tweaks_A_band" */
	/* and this flag is only used for selecting PAPD algorithm. FALSE is AM-PM. */
	if (sdna_board_flag)
		sslpnphy_specific->sslpnphy_radio_classA = TRUE;

}
static bool wlc_sslpnphy_fcc_chan_check(phy_info_t *pi, uint channel)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

	/* No tunings required currently for TSMC */
	if ((sslpnphy_specific->sslpnphy_fabid == 2) || (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12))
		return FALSE;

	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90U_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90M_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICLOCO_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN18_SSID)) {

		if ((channel == 1) || (channel == 11))
			return TRUE;
	} else if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGB_SSID)) {
		if ((channel == 1) || (channel == 11) || (channel == 13))
			return TRUE;
	}
	return FALSE;

}

void
WLBANDINITFN(wlc_phy_init_sslpnphy)(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#else
	wlc_info_t * wlc_pi = pi->wlc;
#endif /* PHYHAL */
#ifndef PPR_API
	uint8 i;
#endif /* PPR_API */
	uint8 phybw40;
	uint8 band_idx;
	uint channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
	/* To enable peak current mode set default value to 0 */
	sslpnphy_specific->peak_current_mode = TRUE;
	sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_at_init_done = FALSE;
	if (sdna_board_flag) {
		sslpnphy_specific->peak_current_mode = FALSE;
		sslpnphy_specific->sslpnphy_papd_peakcurmode.predict_cal_count = 0;
	}

	band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);
	sslpnphy_specific->sslpnphy_OLYMPIC =
	        ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90_SSID ||
	          BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90U_SSID ||
	          BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN90M_SSID ||
	          BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICN18_SSID ||
	          BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICUNO_SSID ||
	          BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICLOCO_SSID ||
	          ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) &&
	           (CHSPEC_IS5G(pi->radio_chanspec))) ||
	          BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17_SSID ||
	          BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID ||
	          BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ? 1 : 0);
	phybw40 = IS40MHZ(pi);

	wlc_sslpnphy_percal_flags_off(pi);

	/* sdna: xtal changes; This code is in init() as well as set channel() */
	if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID) {
		if (sdna_board_flag) {
			/* xtal buffer size */
			/* si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 0x3f, 0x3F); */
			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 0, 0x3f, 0x20);
			/* xtalcore after XTAL becomes stable */
			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 2, 0x3f000, 0x20 << 12);
			/* xtal delay is programmed only after enabling doubler in set_channel */

			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, PMU1_PLL0_CHIPCTL0, 0x20000, 0x00000);
		} else {
			si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 2, 0x0003f000, (0xa << 12));
		}
	}

	if ((CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4329_CHIP_ID) ||
		(CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID))
		si_pmu_regcontrol(GENERIC_PHY_INFO(pi)->sih, 2, 0x00000007, 0x00000005);
	/* enable extlna */
	if ((CHIPID(GENERIC_PHY_INFO(pi)->chip) == BCM5356_CHIP_ID) &&
		(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA)) {

		si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, 2, (1 << 26), (1 << 26));
	}

	/* initializing the adc-presync and auxadc-presync for 2x sampling */
	wlc_sslpnphy_afe_clk_init(pi, AFE_CLK_INIT_MODE_TXRX2X);

	/* Initialize baseband */
	wlc_sslpnphy_baseband_init(pi);

	/* Initialize radio */
	wlc_sslpnphy_radio_init(pi);

	/* Run RC Cal */
	wlc_sslpnphy_rc_cal(pi);

	if (!NORADIO_ENAB(pi->pubpi)) {
		write_radio_reg(pi, RADIO_2063_TXBB_SP_3, 0x3f);
	}
	sslpnphy_specific->sslpnphy_filt_bw = phy_reg_read(pi, SSLPNPHY_lpfbwlutreg0);
	sslpnphy_specific->sslpnphy_ofdm_filt_bw = phy_reg_read(pi, SSLPNPHY_lpfbwlutreg1);
	sslpnphy_specific->sslpnphy_extstxctrl0 = phy_reg_read(pi, SSLPNPHY_extstxctrl0);
	sslpnphy_specific->sslpnphy_extstxctrl1 = phy_reg_read(pi, SSLPNPHY_extstxctrl1);
	sslpnphy_specific->sslpnphy_extstxctrl2 = phy_reg_read(pi, SSLPNPHY_extstxctrl2);
	sslpnphy_specific->sslpnphy_extstxctrl3 = phy_reg_read(pi, SSLPNPHY_extstxctrl3);
	sslpnphy_specific->sslpnphy_extstxctrl4 = phy_reg_read(pi, SSLPNPHY_extstxctrl4);
	sslpnphy_specific->sslpnphy_extstxctrl5 = phy_reg_read(pi, SSLPNPHY_extstxctrl5);

	/* Tune to the current channel */
	wlc_phy_chanspec_set_sslpnphy(pi, pi->radio_chanspec);

	/* Some of the CRS/AGC values are dependent on Channel and VT. So initialise here
	 *  to known values
	*/
	wlc_sslpnphy_set_chanspec_tweaks(pi, pi->radio_chanspec);

	if ((SSLPNREV_LE(pi->pubpi.phy_rev, 2)) || (SSLPNREV_GE(pi->pubpi.phy_rev, 4))) {
		wlc_sslpnphy_CmRxAciGainTbl_Tweaks(pi);
	}

	wlc_sslpnphy_agc_temp_init(pi);

#ifndef PPR_API
	{
		for (i = 0; i < TXP_NUM_RATES; i++)
			sslpnphy_specific->sslpnphy_saved_tx_user_target[i] = pi->txpwr_limit[i];
	}
#endif

	/* Run initial calibration */
	if (sslpnphy_specific->sslpnphy_full_cal_chanspec[band_idx] != pi->radio_chanspec) {
		wlc_sslpnphy_full_cal(pi);
	} else {
		wlc_sslpnphy_restore_calibration_results(pi);
		sslpnphy_specific->sslpnphy_restore_papd_cal_results = 1;
	}

	wlc_sslpnphy_tempsense(pi);
	wlc_sslpnphy_temp_adj(pi);
	wlc_sslpnphy_cck_filt_load(pi, sslpnphy_specific->sslpnphy_cck_filt_sel);
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 0)) {
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_cck_tap0_i,
			sslpnphy_rev0_cx_cck, 10);
		wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
			sslpnphy_rev0_cx_ofdm, 10);

	}
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 1)) {
		if (!NORADIO_ENAB(pi->pubpi)) {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
				sslpnphy_rev1_cx_ofdm, 10);
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_ofdm_tap0,
				sslpnphy_rev1_real_ofdm, 5);
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_ht_tap0,
				sslpnphy_rev1_real_ht, 5);

			/* NOK ref board sdagb  and TDK module Es2.11 requires */
			/*  special tuning for spectral flatness */
			if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGB_SSID) ||
				((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) &&
				(CHSPEC_IS2G(pi->radio_chanspec))))
				wlc_sslpnphy_ofdm_filt_load(pi);
			if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329TDKMDL11_SSID)
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_txrealfilt_ofdm_tap0,
					sslpnphy_tdk_mdl_real_ofdm, 5);

			if (IS_OLYMPIC(pi)) {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
				sslpnphy_olympic_cx_ofdm, 10);
				if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) ||
				    (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ||
				    (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID))
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
						sslpnphy_rev1_cx_ofdm, 10);
			}
			if (wlc_sslpnphy_fcc_chan_check(pi, channel)) {
			    if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID ||
			        BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) {
					wlc_sslpnphy_load_filt_coeff(pi,
					    SSLPNPHY_ofdm_tap0_i,
					    sslpnphy_rev1_cx_ofdm_fcc, 10);
					wlc_sslpnphy_load_filt_coeff(pi,
					    SSLPNPHY_txrealfilt_ofdm_tap0,
					    sslpnphy_rev1_real_ofdm_fcc, 5);
					wlc_sslpnphy_load_filt_coeff(pi,
					    SSLPNPHY_txrealfilt_ht_tap0,
					    sslpnphy_rev1_real_ht_fcc, 5);
			    }
			}

			if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID ||
				BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) {
				if ((sslpnphy_specific->sslpnphy_fabid == 2) ||
					(sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
					wlc_sslpnphy_load_filt_coeff(pi,
					    SSLPNPHY_ofdm_tap0_i,
					    sslpnphy_rev1_cx_ofdm_sec, 10);
					wlc_sslpnphy_load_filt_coeff(pi,
					    SSLPNPHY_txrealfilt_ofdm_tap0,
					    sslpnphy_rev1_real_ofdm_sec, 5);
					wlc_sslpnphy_load_filt_coeff(pi,
					    SSLPNPHY_txrealfilt_ht_tap0,
					    sslpnphy_rev1_real_ht_sec, 5);
			    }
			}
		}
	}
	if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
		if (!NORADIO_ENAB(pi->pubpi)) {
			if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
				if (phybw40 == 1)
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap0,
					sslpnphy_rev4_phybw40_real_ofdm, 5);
				else
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap0,
					sslpnphy_rev4_real_ofdm, 5);
			} else {
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ofdm_tap0,
					sslpnphy_rev2_real_ofdm, 5);
			}
			if (phybw40 == 1) {
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 4))
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
						sslpnphy_rev4_phybw40_cx_ofdm, 10);
				else
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
						sslpnphy_rev2_phybw40_cx_ofdm, 10);
			} else {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
				sslpnphy_rev4_cx_ofdm, 10);
			}

			if (phybw40 == 1) {
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 4))
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_ht_tap0_i,
						sslpnphy_rev4_phybw40_cx_ht, 10);
				else
					wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_ht_tap0_i,
						sslpnphy_rev2_phybw40_cx_ht, 10);
			} else {
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_ht_tap0_i,
					sslpnphy_rev2_cx_ht, 10);
			}
			if (phybw40 == 1) {
				if (SSLPNREV_IS(pi->pubpi.phy_rev, 4))
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap0,
					sslpnphy_rev4_phybw40_real_ht, 5);
				else
				wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_Rev2_txrealfilt_ht_tap0,
					sslpnphy_rev2_phybw40_real_ht, 5);
			}
		} else {
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_cck_tap0_i,
				sslpnphy_rev2_default, 10);
			wlc_sslpnphy_load_filt_coeff(pi, SSLPNPHY_ofdm_tap0_i,
				sslpnphy_rev2_default, 10);

		}
	}
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		/* 5356 */
		if (phybw40) {
			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap0, 179)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap1, 172)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap2, (uint16)(-28))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap3, (uint16)(-92))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap4, 26)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap0, 52)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap1, 31)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap2, (uint16)(-9))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap3, (uint16)(-15))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ofdm_tap4, 256)
			PHY_REG_LIST_EXECUTE(pi);
		}

		if (phybw40) {
			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap0_i, 66)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap0_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap1_i, 91)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap1_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap2_i, (uint16)(-6))
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap2_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap3_i, (uint16)(-28))
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap3_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap4_i, 6)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap4_q, 0)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap0_i, 65)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap0_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap1_i, (uint16)(-20))
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap1_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap2_i, (uint16)(-162))
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap2_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap3_i, 127)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap3_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap4_i, 200)
				PHY_REG_WRITE_ENTRY(SSLPNPHY, ofdm_tap4_q, 0)
			PHY_REG_LIST_EXECUTE(pi);
		}

		/* Load cck filters */
		wlc_sslpnphy_cck_filt_load(pi, sslpnphy_specific->sslpnphy_cck_filt_sel);

		if (phybw40 == 1) {
			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap0, 179)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap1, 172)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap2, (uint16)(-28))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap3, (uint16)(-92))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap4, 26)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap0_i, 66)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap1_i, 91)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap2_i, (uint16)(-6))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap3_i, (uint16)(-28))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap4_i, 6)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap0_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap1_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap2_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap3_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap4_q, 0)
			PHY_REG_LIST_EXECUTE(pi);
			sslpnphy_specific->sslpnphy_extstxctrl4 = phy_reg_read(pi, SSLPNPHY_extstxctrl4);
			sslpnphy_specific->sslpnphy_extstxctrl0 = phy_reg_read(pi, SSLPNPHY_extstxctrl0);
			sslpnphy_specific->sslpnphy_extstxctrl1 = phy_reg_read(pi, SSLPNPHY_extstxctrl1);
		} else {
			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap0, 52)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap1, 31)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap2, (uint16)(-9))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap3, (uint16)(-15))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, txrealfilt_ht_tap4, 256)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap0_i, 47)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap1_i, 17)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap2_i, (uint16)(-27))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap3_i, (uint16)(-20))
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap4_i, 256)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap0_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap1_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap2_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap3_q, 0)
				PHY_REG_WRITE_ENTRY(SSLPNPHY_Rev2, ht_tap4_q, 0)
			PHY_REG_LIST_EXECUTE(pi);
		}
		/* 5356 */
	}
	PHY_REG_LIST_START_WLBANDINITDATA
		PHY_REG_MOD_ENTRY(SSLPNPHY, Core1TxControl, txClipEnable_ofdm, 0)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipBpsk, 0x7fff)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, txClipQpsk, 0x7fff)
	PHY_REG_LIST_EXECUTE(pi);

	if (channel == 14)
		wlc_sslpnphy_cck_filt_load(pi, 4);

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		if (phybw40) {
			phytbl_info_t tab;
			tab.tbl_ptr = fltr_ctrl_tbl_40Mhz;
			tab.tbl_len = 10;
			tab.tbl_id = 0xb;
			tab.tbl_offset = 0;
			tab.tbl_width = 32;
			wlc_sslpnphy_write_table(pi, &tab);
		}
	}

	sslpnphy_specific->sslpnphy_noise_samples = SSLPNPHY_NOISE_SAMPLES_DEFAULT;

	if ((SSLPNREV_LT(pi->pubpi.phy_rev, 2)) && (CHSPEC_IS2G(pi->radio_chanspec))) {
		/* cellular emission fixes */
		sslpnphy_specific->sslpnphy_logen_buf_1 = read_radio_reg(pi, RADIO_2063_LOGEN_BUF_1);
		sslpnphy_specific->sslpnphy_local_ovr_2 = read_radio_reg(pi, RADIO_2063_LOCAL_OVR_2);
		sslpnphy_specific->sslpnphy_local_oval_6 = read_radio_reg(pi, RADIO_2063_LOCAL_OVAL_6);
		sslpnphy_specific->sslpnphy_local_oval_5 = read_radio_reg(pi, RADIO_2063_LOCAL_OVAL_5);
		sslpnphy_specific->sslpnphy_logen_mixer_1 = read_radio_reg(pi, RADIO_2063_LOGEN_MIXER_1);
	}

	/* Switch on the power control */
	WL_INFORM(("init pre  t=%d, %d \n",
	           sslpnphy_specific->sslpnphy_auxadc_val, sslpnphy_specific->sslpnphy_tssi_val));
	wlc_sslpnphy_tx_pwr_ctrl_init(pi);
	WL_INFORM(("init post  t=%d, %d \n",
	           sslpnphy_specific->sslpnphy_auxadc_val, sslpnphy_specific->sslpnphy_tssi_val));

	/* PAPD Calibration during init time */
	if (!(SCAN_IN_PROGRESS(wlc_pi) || WLC_RM_IN_PROGRESS(wlc_pi))) {
		wlc_sslpnphy_papd_recal(pi);
		/* Skip tx iq if init is happening on same channel (Time savings) */
		if (!sslpnphy_specific->sslpnphy_restore_papd_cal_results)
			wlc_sslpnphy_txpwrtbl_iqlo_cal(pi);
	} else {
		WL_INFORM((" %s : Not doing a full cal: Restoring the "
			"previous cal results for channel %d ",
			__FUNCTION__, sslpnphy_specific->sslpnphy_full_cal_channel[band_idx]));
		sslpnphy_specific->sslpnphy_restore_papd_cal_results = 1;
		wlc_sslpnphy_papd_recal(pi);
	}

	wlc_sslpnphy_noise_init(pi);
	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		wlc_sslpnphy_aci_init(pi);
	}

	/* For olympic UNO Boards, control the turning off of eLNA during Tx */
	/* This code can be moved to a better place */
	if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA_TX)
		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
			M_SSLPNPHY_LNA_TX)), 1);
#ifdef SSLPNLOWPOWER
	mod_radio_reg(pi, RADIO_2063_COMMON_03, 0x20, 0x20);
	mod_radio_reg(pi, RADIO_2063_GRX_SP_3, 0xf0, 0x00);
	write_radio_reg(pi, RADIO_2063_RXBB_CTRL_4, 0x00);
	write_radio_reg(pi, RADIO_2063_RXBB_CTRL_3, 0x00);
	mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_7, 0xfc, 0x00);
	write_radio_reg(pi, RADIO_2063_GRX_PS_1, 0x00);
	write_radio_reg(pi, RADIO_2063_RXBB_CTRL_1, 0xf4);
	/* ADC Low Power Mode */
	PHY_REG_LIST_START_WLBANDINITDATA
		PHY_REG_WRITE_ENTRY(SSLPNPHY, AfeADCCtrl0, 0x8022)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, AfeADCCtrl1, 0x422)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, AfeADCCtrl2, 0x0040)
	PHY_REG_LIST_EXECUTE(pi);
#endif
/* for gds support */
#ifdef ROMTERMPHY
	/* average of lab-measured listen gain with dynamic noise-cal across splits */
	if (((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) >= SSLPNPHY_LOW_LISTEN_GAIN) &&
		((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) <= SSLPNPHY_HIGH_LISTEN_GAIN))
		pi->sslpn_nom_listen_gain = 0x49;
	else
		pi->sslpn_nom_listen_gain = 0x46;

#endif /* ROMTERMPHY */

#if defined(WLTEST) && defined(ROMTERMPHY)
	if (sdna_board_flag) {
		wlc_sslpnphy_set_crs(pi, SSLPNPHY_CRS_DESENSE_VAL);
	}
#endif 

#ifdef ROMTERMPHY
	/* Trigger idle TSSI measurement to ensure on expiry of watchdog for frst time */
	/* we get valid idleTSSI value from ucode/HW */
	if (sdna_board_flag)
		wlc_sslpnphy_write_shm_tssiCalEn(pi, 1);
#endif
}

static void
wlc_sslpnphy_noise_init(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint8 phybw40 = IS40MHZ(pi);

	sslpnphy_specific->sslpnphy_NPwr_MinLmt = SSLPNPHY_NPWR_MINLMT;
	sslpnphy_specific->sslpnphy_NPwr_MaxLmt = SSLPNPHY_NPWR_MAXLMT_2G;
	sslpnphy_specific->sslpnphy_noise_measure_window = SSLPNPHY_NOISE_MEASURE_WINDOW_2G;
	sslpnphy_specific->sslpnphy_max_listen_gain_change_lmt = SSLPNPHY_MAX_GAIN_CHANGE_LMT_2G;
	sslpnphy_specific->sslpnphy_max_rxpo_change_lmt = SSLPNPHY_MAX_RXPO_CHANGE_LMT_2G;
	if (phybw40 || (CHSPEC_IS5G(pi->radio_chanspec))) {
		sslpnphy_specific->sslpnphy_NPwr_LGC_MinLmt = SSLPNPHY_NPWR_LGC_MINLMT_40MHZ;
		sslpnphy_specific->sslpnphy_NPwr_LGC_MaxLmt = SSLPNPHY_NPWR_LGC_MAXLMT_40MHZ;
		if (CHSPEC_IS5G(pi->radio_chanspec)) {
			sslpnphy_specific->sslpnphy_NPwr_MaxLmt = SSLPNPHY_NPWR_MAXLMT_5G;
			sslpnphy_specific->sslpnphy_noise_measure_window = SSLPNPHY_NOISE_MEASURE_WINDOW_5G;
			sslpnphy_specific->sslpnphy_max_listen_gain_change_lmt = SSLPNPHY_MAX_GAIN_CHANGE_LMT_5G;
			sslpnphy_specific->sslpnphy_max_rxpo_change_lmt = SSLPNPHY_MAX_RXPO_CHANGE_LMT_5G;
		}
	} else {
		sslpnphy_specific->sslpnphy_NPwr_LGC_MinLmt = SSLPNPHY_NPWR_LGC_MINLMT_20MHZ;
		sslpnphy_specific->sslpnphy_NPwr_LGC_MaxLmt = SSLPNPHY_NPWR_LGC_MAXLMT_20MHZ;
	}
}

STATIC uint8 NOISE_ARRAY [][2] = {
	{1, 62 },
	{2, 65 },
	{3, 67 },
	{4, 68 },
	{5, 69 },
	{6, 70 },
	{7, 70 },
	{8, 71 },
	{9, 72 },
	{10, 72 },
	{11, 72 },
	{12, 73 },
	{13, 73 },
	{14, 74 },
	{15, 74 },
	{16, 74 },
	{17, 74 },
	{18, 75 },
	{19, 75 },
	{20, 75 },
	{21, 75 },
	{22, 75 },
	{23, 76 },
	{24, 76 },
	{25, 76 },
	{26, 76 },
	{27, 76 },
	{28, 77 },
	{29, 77 },
	{30, 77 },
	{31, 77 },
	{32, 77 },
	{33, 77 },
	{34, 77 },
	{35, 77 },
	{36, 78 },
	{37, 78 },
	{38, 78 },
	{39, 78 },
	{40, 78 },
	{41, 78 },
	{42, 78 },
	{43, 78 },
	{44, 78 },
	{45, 79 },
	{46, 79 },
	{47, 79 },
	{48, 79 },
	{49, 79 },
	{50, 79 }
};
uint8 NOISE_ARRAY_sz = ARRAYSIZE(NOISE_ARRAY);

STATIC uint8
wlc_sslpnphy_rx_noise_lut(phy_info_t *pi, uint8 noise_val, uint8 ptr[][2], uint8 array_size)
{
	uint8 i = 1;
	uint8 rxpoWoListenGain = 0;
	for (i = 1; i < array_size; i++) {
		if (ptr[i][0] == noise_val) {
			rxpoWoListenGain = ptr[i][1];
		}
	}
	return rxpoWoListenGain;
}

STATIC void
wlc_sslpnphy_reset_radioctrl_crsgain(phy_info_t *pi)
{
	/* Reset radio ctrl and crs gain */
	PHY_REG_LIST_START
		PHY_REG_OR_ENTRY(SSLPNPHY, resetCtrl, 0x44)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, resetCtrl, 0x80)
	PHY_REG_LIST_EXECUTE(pi);
}

STATIC void
wlc_sslpnphy_noise_fifo_init(phy_info_t *pi)
{
	uint8 i;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	sslpnphy_specific->sslpnphy_noisepwr_fifo_filled = 0;

	for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
		sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i] = 32767;
		sslpnphy_specific->sslpnphy_noisepwr_fifo_Max[i] = 0;
	}
	/* Initialising the noise-computation variables */

	sslpnphy_specific->noisedBm_ma = 0;
	sslpnphy_specific->noisedBm_ma_count = 0;
	sslpnphy_specific->noisedBm_ma_win_sz = MA_WINDOW_SZ;
	sslpnphy_specific->noisedBm_index = 0;
	sslpnphy_specific->noisedBm_ma_avg = 0;

	for (i = 0; i < MA_WINDOW_SZ; i++)
		sslpnphy_specific->noisedBm_window[i] = 0;

}

STATIC void
wlc_sslpnphy_noise_measure_setup(phy_info_t *pi)
{
	int16 temp;
	uint8 phybw40 = IS40MHZ(pi);
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

	if (phybw40 == 0) {
		sslpnphy_specific->Listen_GaindB_BfrNoiseCal =
			(uint8)PHY_REG_READ(pi, SSLPNPHY, HiGainDB, HiGainDB);
		sslpnphy_specific->NfSubtractVal_BfrNoiseCal =
			PHY_REG_READ(pi, SSLPNPHY, nfSubtractVal, nfSubVal);
	} else {
		sslpnphy_specific->Listen_GaindB_BfrNoiseCal =
			(uint8)PHY_REG_READ(pi, SSLPNPHY_Rev2, HiGainDB_40, HiGainDB);
		sslpnphy_specific->NfSubtractVal_BfrNoiseCal =
			PHY_REG_READ(pi, SSLPNPHY_Rev2, nfSubtractVal_40, nfSubVal);
	}

	sslpnphy_specific->Listen_GaindB_AfrNoiseCal = sslpnphy_specific->Listen_GaindB_BfrNoiseCal;

	if (sslpnphy_specific->sslpnphy_init_noise_cal_done == 0) {
		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
			M_SSLPNPHY_NOISE_SAMPLES)), 128 << phybw40);
		if (phybw40 == 0) {
			sslpnphy_specific->Listen_GaindB_BASE = (uint8)(phy_reg_read(pi, SSLPNPHY_HiGainDB) &
				SSLPNPHY_HiGainDB_HiGainDB_MASK) >>
				SSLPNPHY_HiGainDB_HiGainDB_SHIFT;

			temp = (int16)(phy_reg_read(pi, SSLPNPHY_InputPowerDB)
				& SSLPNPHY_InputPowerDB_inputpwroffsetdb_MASK);

			sslpnphy_specific->NfSubtractVal_BASE = (phy_reg_read(pi, SSLPNPHY_nfSubtractVal) &
				0x3ff);
		} else {
			sslpnphy_specific->Listen_GaindB_BASE =
				(uint8)((phy_reg_read(pi, SSLPNPHY_Rev2_HiGainDB_40) &
				SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_MASK) >>
				SSLPNPHY_Rev2_HiGainDB_40_HiGainDB_SHIFT);

			temp = (int16)(phy_reg_read(pi, SSLPNPHY_Rev2_InputPowerDB_40)
				& SSLPNPHY_Rev2_InputPowerDB_40_inputpwroffsetdb_MASK);

			sslpnphy_specific->NfSubtractVal_BASE =
				(phy_reg_read(pi, SSLPNPHY_Rev2_nfSubtractVal_40) & 0x3ff);
		}
		temp = temp << 8;
		temp = temp >> 8;

		sslpnphy_specific->RxpowerOffset_Required_BASE = (int8)temp;

		wlc_sslpnphy_detection_disable(pi, TRUE);
		wlc_sslpnphy_reset_radioctrl_crsgain(pi);

		wlc_sslpnphy_noise_fifo_init(pi);
	}

	sslpnphy_specific->rxpo_required_AfrNoiseCal = sslpnphy_specific->RxpowerOffset_Required_BASE;
}

STATIC uint32
wlc_sslpnphy_get_rxiq_accum(phy_info_t *pi)
{
	uint32 IPwr, QPwr, IQ_Avg_Pwr = 0;
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);

	IPwr = ((uint32)phy_reg_read(pi, SSLPNPHY_IQIPWRAccHiAddress) << 16) |
		(uint32)phy_reg_read(pi, SSLPNPHY_IQIPWRAccLoAddress);
	QPwr = ((uint32)phy_reg_read(pi, SSLPNPHY_IQQPWRAccHiAddress) << 16) |
		(uint32)phy_reg_read(pi, SSLPNPHY_IQQPWRAccLoAddress);

	IQ_Avg_Pwr = (uint32)wlc_lpphy_qdiv_roundup((IPwr + QPwr),
		WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPNPHY_NOISE_SAMPLES))), 0);

	return IQ_Avg_Pwr;
}

STATIC uint32
wlc_sslpnphy_abs_time(uint32 end, uint32 start)
{
	uint32 timediff;
	uint32 max32 = (uint32)((int)(0) - (int)(1));

	if (end >= start)
		timediff = end - start;
	else
		timediff = (1 + end) + (max32 - start);

	return timediff;
}

STATIC void
wlc_sslpnphy_noise_measure_time_window(phy_info_t *pi, uint32 window_time, uint32 *minpwr,
	uint32 *maxpwr, bool *measurement_valid)
{
	uint32 start_time;
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	*measurement_valid = FALSE;

	*minpwr = 32767;
	*maxpwr = 0;

	start_time = R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->tsf_timerlow);

	WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
		M_55f_REG_VAL)), 0);
	OR_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccommand, MCMD_BG_NOISE);

	while (wlc_sslpnphy_abs_time(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->tsf_timerlow),
		start_time) < window_time) {

		if (R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccommand) & MCMD_BG_NOISE) {
			OSL_DELAY(8);
		} else {
			sslpnphy_specific->IQ_Avg_Pwr = wlc_sslpnphy_get_rxiq_accum(pi);

			*minpwr = MIN(*minpwr, sslpnphy_specific->IQ_Avg_Pwr);
			*maxpwr = MAX(*maxpwr, sslpnphy_specific->IQ_Avg_Pwr);

			OSL_DELAY(6);
			WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
				M_55f_REG_VAL)), 0);
			OR_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccommand, MCMD_BG_NOISE);
		}
	}

	if ((*minpwr >= sslpnphy_specific->sslpnphy_NPwr_MinLmt) &&
	    (*minpwr <= sslpnphy_specific->sslpnphy_NPwr_MaxLmt))
		*measurement_valid = TRUE;
}

STATIC uint32
wlc_sslpnphy_noise_fifo_min(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint8 i;
	uint32 minpwr = 32767;

	for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
		WL_PHYCAL(("I is %d:MIN_FIFO = %d:MAX_FIFO = %d \n", i,
			sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i],
			sslpnphy_specific->sslpnphy_noisepwr_fifo_Max[i]));
		minpwr = MIN(minpwr, sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i]);
	}

	return minpwr;
}

STATIC void
wlc_sslpnphy_noise_fifo_avg(phy_info_t *pi, uint32 *avg_noise)
{
	uint8 i;
	uint8 max_min_Idx_1 = 0, max_min_Idx_2 = 0;
	uint32 Min_Min = 65535, Max_Max = 0, Max_Min_2 = 0,  Max_Min_1 = 0;
	uint32 Sum = 0;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	if (sslpnphy_specific->sslpnphy_init_noise_cal_done) {
		*avg_noise = wlc_sslpnphy_noise_fifo_min(pi);
		return;
	}

	for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
		WL_PHYCAL(("I is %d:MIN_FIFO = %d:MAX_FIFO = %d \n", i,
			sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i],
			sslpnphy_specific->sslpnphy_noisepwr_fifo_Max[i]));

		Min_Min = MIN(Min_Min, sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i]);
		Max_Min_1 = MAX(Max_Min_1, sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i]);
		Max_Max = MAX(Max_Max, sslpnphy_specific->sslpnphy_noisepwr_fifo_Max[i]);
	}

	if (Max_Max >= ((Min_Min * 5) >> 1))
		*avg_noise = Min_Min;
	else {
		for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
			if (Max_Min_1 == sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i])
				max_min_Idx_1 = i;
		}

		for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
			if (i != max_min_Idx_1)
				Max_Min_2 = MAX(Max_Min_2, sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i]);
		}

		for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
			if ((Max_Min_2 == sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i]) &&
				(i != max_min_Idx_1))
				max_min_Idx_2 = i;
		}

		for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
			if ((i != max_min_Idx_1) && (i != max_min_Idx_2))
				Sum += sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i];
		}

		/* OutOf Six values of MinFifo,Two big values are eliminated
		and averaged out remaining four values of Min-FIFO
		*/

		*avg_noise = wlc_lpphy_qdiv_roundup(Sum, 4, 0);

		WL_PHYCAL(("Sum = %d: Max_Min_1 = %d: Max_Min_2 = %d"
			" max_min_Idx_1 = %d: max_min_Idx_2 = %d\n", Sum,
			Max_Min_1, Max_Min_2, max_min_Idx_1, max_min_Idx_2));
	}
	WL_PHYCAL(("Avg_Min_NoisePwr = %d\n", *avg_noise));
}

typedef enum {
	INIT_FILL_FIFO = 0,
	CHK_LISTEN_GAIN_CHANGE,
	CHANGE_RXPO,
	PERIODIC_CAL
} sslpnphy_noise_measure_t;


void
wlc_sslpnphy_noise_measure_chg_listen_gain(phy_info_t *pi, int8 change_sign)
{
	uint8 phybw40 = IS40MHZ(pi);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);

	sslpnphy_specific->Listen_GaindB_AfrNoiseCal =
	        sslpnphy_specific->Listen_GaindB_AfrNoiseCal + (3 * change_sign);

	if (sslpnphy_specific->Listen_GaindB_AfrNoiseCal <= (sslpnphy_specific->Listen_GaindB_BASE -
		sslpnphy_specific->sslpnphy_max_listen_gain_change_lmt))
		sslpnphy_specific->Listen_GaindB_AfrNoiseCal = sslpnphy_specific->Listen_GaindB_BASE -
			sslpnphy_specific->sslpnphy_max_listen_gain_change_lmt;
	else if (sslpnphy_specific->Listen_GaindB_AfrNoiseCal >= (sslpnphy_specific->Listen_GaindB_BASE +
		sslpnphy_specific->sslpnphy_max_listen_gain_change_lmt))
		sslpnphy_specific->Listen_GaindB_AfrNoiseCal = sslpnphy_specific->Listen_GaindB_BASE +
			sslpnphy_specific->sslpnphy_max_listen_gain_change_lmt;
	if (sdna_board_flag) {
		if (sslpnphy_specific->Listen_GaindB_AfrNoiseCal >= 0x49)
			sslpnphy_specific->Listen_GaindB_AfrNoiseCal =  0x49;
	}

	if (phybw40 == 0)
		PHY_REG_MOD(pi, SSLPNPHY, HiGainDB, HiGainDB, sslpnphy_specific->Listen_GaindB_AfrNoiseCal);
	else
		PHY_REG_MOD(pi, SSLPNPHY_Rev2, HiGainDB_40, HiGainDB, sslpnphy_specific->Listen_GaindB_AfrNoiseCal);

	wlc_sslpnphy_reset_radioctrl_crsgain(pi);
	OSL_DELAY(10);
}

STATIC void
wlc_sslpnphy_noise_measure_change_rxpo(phy_info_t *pi, uint32 avg_noise)
{
	uint8 rxpo_Wo_Listengain;
	uint8 phybw40 = IS40MHZ(pi);
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
	if (sdna_board_flag)
		return;
	if (!sslpnphy_specific->sslpnphy_init_noise_cal_done) {
		sslpnphy_specific->Listen_RF_Gain = PHY_REG_READ(pi, SSLPNPHY, crsGainRespVal, crsGainVal);
	} else {
		sslpnphy_specific->Listen_RF_Gain = (WL_READ_SHM(pi,
			(2 * (sslpnphy_shm_ptr + M_55f_REG_VAL))) & 0xff);
	}

	if ((sslpnphy_specific->Listen_RF_Gain > 45) && (sslpnphy_specific->Listen_RF_Gain < 85) &&
		(ABS(sslpnphy_specific->Listen_GaindB_AfrNoiseCal - sslpnphy_specific->Listen_RF_Gain) < 12)) {

		rxpo_Wo_Listengain = wlc_sslpnphy_rx_noise_lut(pi, (uint8)avg_noise, NOISE_ARRAY,
			NOISE_ARRAY_sz);

		sslpnphy_specific->rxpo_required_AfrNoiseCal =
		        (int8)(sslpnphy_specific->Listen_RF_Gain - rxpo_Wo_Listengain);

		if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) {
			if (phybw40 == 0)
				sslpnphy_specific->rxpo_required_AfrNoiseCal =
				        sslpnphy_specific->rxpo_required_AfrNoiseCal - 2 +
					sslpnphy_specific->sslpnphy_noise_cal_offset;
			else
				sslpnphy_specific->rxpo_required_AfrNoiseCal =
				        sslpnphy_specific->rxpo_required_AfrNoiseCal + 2 +
					sslpnphy_specific->sslpnphy_noise_cal_offset;
		}

		if (sslpnphy_specific->rxpo_required_AfrNoiseCal <=
		    (-1 * sslpnphy_specific->sslpnphy_max_rxpo_change_lmt))
			sslpnphy_specific->rxpo_required_AfrNoiseCal =
			        -1 * sslpnphy_specific->sslpnphy_max_rxpo_change_lmt;
		else if (sslpnphy_specific->rxpo_required_AfrNoiseCal >=
		         sslpnphy_specific->sslpnphy_max_rxpo_change_lmt)
			sslpnphy_specific->rxpo_required_AfrNoiseCal =
			        sslpnphy_specific->sslpnphy_max_rxpo_change_lmt;

		if (phybw40 == 0)
		{
			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb,
				sslpnphy_specific->rxpo_required_AfrNoiseCal);
		} else {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40,
				inputpwroffsetdb, sslpnphy_specific->rxpo_required_AfrNoiseCal);
		}
	}
}

STATIC void
wlc_sslpnphy_noise_measure_computeNf(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	int8 Delta_Listen_GaindB_Change, Delta_NfSubtractVal_Change;
	uint8 phybw40 = IS40MHZ(pi);

	if (phybw40 == 0)
	{
		sslpnphy_specific->Listen_GaindB_AfrNoiseCal = (uint8)PHY_REG_READ(pi, SSLPNPHY, HiGainDB, HiGainDB);
	} else {
		sslpnphy_specific->Listen_GaindB_AfrNoiseCal =
			(uint8)PHY_REG_READ(pi, SSLPNPHY_Rev2, HiGainDB_40, HiGainDB);
	}

	Delta_Listen_GaindB_Change = (int8)(sslpnphy_specific->Listen_GaindB_AfrNoiseCal -
		sslpnphy_specific->Listen_GaindB_BfrNoiseCal);

	if ((CHSPEC_IS2G(pi->radio_chanspec)) && ((Delta_Listen_GaindB_Change > 3) ||
		(Delta_Listen_GaindB_Change < -3)))
		Delta_NfSubtractVal_Change = 4 * Delta_Listen_GaindB_Change;
	else
		Delta_NfSubtractVal_Change = 0;

	sslpnphy_specific->NfSubtractVal_AfrNoiseCal = (sslpnphy_specific->NfSubtractVal_BfrNoiseCal +
		Delta_NfSubtractVal_Change) & 0x3ff;

	if (phybw40 == 0)
		PHY_REG_WRITE(pi, SSLPNPHY, nfSubtractVal, sslpnphy_specific->NfSubtractVal_AfrNoiseCal);
	else
		PHY_REG_WRITE(pi, SSLPNPHY_Rev2, nfSubtractVal_40, sslpnphy_specific->NfSubtractVal_AfrNoiseCal);
}

/* coverity[dead_code] */
/* Intentional: to avoid ifdefs, 20MHz-only builds depend on the */
/* optimizer to remove code based on the constant assignment. */

static int8
wlc_sslpnphy_get_noise_level(phy_info_t *pi, uint8 gain, uint32 received_power)
{
	uint8 nominal_power_db;
	int32 log_val, gain_mismatch;
	int16  noise_level_dBm;
	uint8 phybw40 = IS40MHZ(pi);
#ifdef BAND5G
	uint8 channel = CHSPEC_CHANNEL(pi->radio_chanspec); /* see wlioctl.h */
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif /* BAND5G */
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

	if (phybw40 == 0) {
		nominal_power_db = PHY_REG_READ(pi, SSLPNPHY, VeryLowGainDB, NominalPwrDB);
	} else {
		nominal_power_db = PHY_REG_READ(pi, SSLPNPHY_Rev2, VeryLowGainDB_40, NominalPwrDB);
	}

	{
		uint32 power = (received_power*16);
		uint32 msb1, msb2, val1, val2, diff1, diff2;
		msb1 = find_msbit(power);
		msb2 = msb1 + 1;
		val1 = 1 << msb1;
		val2 = 1 << msb2;
		diff1 = (power - val1);
		diff2 = (val2 - power);
		if (diff1 < diff2)
			log_val = msb1;
		else
			log_val = msb2;
	}

	log_val = log_val * 3;

	gain_mismatch = (nominal_power_db/2) - (log_val);

	noise_level_dBm = (int16) (-1 * (gain + gain_mismatch));
	if (noise_level_dBm > 127)
		noise_level_dBm -= 256;
#ifdef BAND5G
	/* an offset added to noise reported as theoritical listen gain is different from actual listen gain */
	if (sdna_board_flag) {
		if (channel >= 36 && channel <= 48)
			noise_level_dBm += 10;
		else
			noise_level_dBm += 6;
	}
#endif /* BAND5G */
	sslpnphy_specific->noise_level_dBm = noise_level_dBm;
	return (int8)noise_level_dBm;
}
void
wlc_sslpnphy_noise_measure(wlc_phy_t *ppi)
{

	uint32 start_time = 0, timeout = 0;
	phy_info_t *pi = (phy_info_t *)ppi;

	uint8 noise_measure_state, i;
	uint32 min_anp, max_anp, avg_noise = 0;
	bool measurement_valid;
	bool measurement_done = FALSE;
	uint8 phybw40 = IS40MHZ(pi);
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	WL_TRACE(("wl%d: %s: begin\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;
#ifdef ROMTERMPHY
	/* wlc.c calls the noise-measure() at wlc_init */
	if (pi->sslpnphy_disable_noise_percal)
		return;
#endif /* ROMTERMPHY */

	if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
		if ((pi->cur_interference_mode != WLAN_AUTO_W_NOISE) &&
			(pi->cur_interference_mode != WLAN_AUTO)) {
			return;
		}

		if (sslpnphy_specific->sslpnphy_init_noise_cal_done && (pi->cur_interference_mode == WLAN_AUTO_W_NOISE))
	                wlc_sslpnphy_aci_upd(pi);
	}

	sslpnphy_specific->sslpnphy_last_noise_cal = GENERIC_PHY_INFO(pi)->now;

	PHY_REG_MOD(pi, SSLPNPHY, sslpnCalibClkEnCtrl, iqEstClkEn, 1);

	if (sslpnphy_specific->sslpnphy_init_noise_cal_done == 0) {
		noise_measure_state = INIT_FILL_FIFO;
		timeout = SSLPNPHY_INIT_NOISE_CAL_TMOUT;
	} else {
		noise_measure_state = PERIODIC_CAL;
		timeout = 0; /* a very small value for just one iteration */
	}

	wlc_sslpnphy_noise_measure_setup(pi);

	start_time = R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->tsf_timerlow);

	do {
		switch (noise_measure_state) {

		case INIT_FILL_FIFO:
			wlc_sslpnphy_noise_measure_time_window(pi,
				sslpnphy_specific->sslpnphy_noise_measure_window, &min_anp, &max_anp,
				&measurement_valid);

			if (measurement_valid) {
				sslpnphy_specific->sslpnphy_noisepwr_fifo_Min
				        [sslpnphy_specific->sslpnphy_noisepwr_fifo_filled] = min_anp;
				sslpnphy_specific->sslpnphy_noisepwr_fifo_Max
				        [sslpnphy_specific->sslpnphy_noisepwr_fifo_filled] = max_anp;

				sslpnphy_specific->sslpnphy_noisepwr_fifo_filled++;
			}

			if (sslpnphy_specific->sslpnphy_noisepwr_fifo_filled == SSLPNPHY_NOISE_PWR_FIFO_DEPTH) {
				noise_measure_state = CHK_LISTEN_GAIN_CHANGE;
				sslpnphy_specific->sslpnphy_noisepwr_fifo_filled = 0;
			}

			break;
		case PERIODIC_CAL:
			if (!(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccommand) & MCMD_BG_NOISE)) {
				sslpnphy_specific->IQ_Avg_Pwr = wlc_sslpnphy_get_rxiq_accum(pi);
				if ((sslpnphy_specific->IQ_Avg_Pwr >= sslpnphy_specific->sslpnphy_NPwr_MinLmt) &&
				    (sslpnphy_specific->IQ_Avg_Pwr <= sslpnphy_specific->sslpnphy_NPwr_MaxLmt)) {
					sslpnphy_specific->sslpnphy_noisepwr_fifo_Min
						[sslpnphy_specific->sslpnphy_noisepwr_fifo_filled] =
						sslpnphy_specific->IQ_Avg_Pwr;
					sslpnphy_specific->sslpnphy_noisepwr_fifo_Max
						[sslpnphy_specific->sslpnphy_noisepwr_fifo_filled] =
						sslpnphy_specific->IQ_Avg_Pwr;
				sslpnphy_specific->sslpnphy_noisepwr_fifo_filled++;
				if (sslpnphy_specific->sslpnphy_noisepwr_fifo_filled ==
					SSLPNPHY_NOISE_PWR_FIFO_DEPTH)
					sslpnphy_specific->sslpnphy_noisepwr_fifo_filled = 0;
				}
			}
			break;

		case CHK_LISTEN_GAIN_CHANGE:
			wlc_sslpnphy_noise_fifo_avg(pi, &avg_noise);


			if ((avg_noise < sslpnphy_specific->sslpnphy_NPwr_LGC_MinLmt) &&
				(avg_noise >= sslpnphy_specific->sslpnphy_NPwr_MinLmt)) {

				wlc_sslpnphy_noise_measure_chg_listen_gain(pi, +1);

				wlc_sslpnphy_noise_fifo_init(pi);
				noise_measure_state = INIT_FILL_FIFO;

			} else if ((avg_noise > sslpnphy_specific->sslpnphy_NPwr_LGC_MaxLmt) &&
				(avg_noise <= sslpnphy_specific->sslpnphy_NPwr_MaxLmt)) {

				wlc_sslpnphy_noise_measure_chg_listen_gain(pi, -1);

				wlc_sslpnphy_noise_fifo_init(pi);
				noise_measure_state = INIT_FILL_FIFO;

			} else if ((avg_noise >= sslpnphy_specific->sslpnphy_NPwr_LGC_MinLmt) &&
				(avg_noise <= sslpnphy_specific->sslpnphy_NPwr_LGC_MaxLmt)) {

				noise_measure_state = CHANGE_RXPO;
			}

			break;

		case CHANGE_RXPO:
			wlc_sslpnphy_noise_measure_change_rxpo(pi, avg_noise);
			measurement_done = TRUE;
			break;

		default:
			break;

		}
	} while ((wlc_sslpnphy_abs_time(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->tsf_timerlow), start_time) <=
		timeout) && (!measurement_done));

	sslpnphy_specific->Listen_RF_Gain = (WL_READ_SHM(pi,
		(2 * (sslpnphy_shm_ptr + M_55f_REG_VAL))) & 0xff);

	if (!measurement_done) {

		if (!sslpnphy_specific->sslpnphy_init_noise_cal_done &&
		    (sslpnphy_specific->sslpnphy_noisepwr_fifo_filled == 0) &&
			(noise_measure_state == 0)) {
			WL_PHYCAL(("Init Noise Cal Timedout After T %d uS And Noise_Cmd = %d:\n",
				wlc_sslpnphy_abs_time(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->tsf_timerlow),
				start_time), (R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccommand) &
				MCMD_BG_NOISE)));
		} else {

			avg_noise = wlc_sslpnphy_noise_fifo_min(pi);

			if ((avg_noise < sslpnphy_specific->sslpnphy_NPwr_LGC_MinLmt) &&
				(avg_noise >= sslpnphy_specific->sslpnphy_NPwr_MinLmt)) {

				wlc_sslpnphy_noise_measure_chg_listen_gain(pi, +1);

				wlc_sslpnphy_noise_fifo_init(pi);

			} else if ((avg_noise > sslpnphy_specific->sslpnphy_NPwr_LGC_MaxLmt) &&
				(avg_noise <= sslpnphy_specific->sslpnphy_NPwr_MaxLmt)) {

				wlc_sslpnphy_noise_measure_chg_listen_gain(pi, -1);

				wlc_sslpnphy_noise_fifo_init(pi);

			} else if ((avg_noise >= sslpnphy_specific->sslpnphy_NPwr_LGC_MinLmt) &&
				(avg_noise <= sslpnphy_specific->sslpnphy_NPwr_LGC_MaxLmt)) {

				wlc_sslpnphy_noise_measure_change_rxpo(pi, avg_noise);
			}
		}
	}

	sslpnphy_specific->noise_level_dBm = wlc_sslpnphy_get_noise_level(pi,
		sslpnphy_specific->Listen_RF_Gain, sslpnphy_specific->IQ_Avg_Pwr);
	/* this function doesn't change nfSubtractVal for 5G */
	wlc_sslpnphy_noise_measure_computeNf(pi);

	WL_PHYCAL(("Phy Bw40:%d Noise Cal Stats After T %d uS:Npercal = %d"
	           " FifoFil = %d Npwr = %d Rxpo = %d Gain_Set = %d"
	           " Delta_Gain_Change = %d\n",
	           IS40MHZ(pi),
	           wlc_sslpnphy_abs_time(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->tsf_timerlow),
	                                 start_time),
	           sslpnphy_specific->sslpnphy_init_noise_cal_done,
	           sslpnphy_specific->sslpnphy_noisepwr_fifo_filled,
	           avg_noise,
	           sslpnphy_specific->rxpo_required_AfrNoiseCal,
	           sslpnphy_specific->Listen_GaindB_AfrNoiseCal,
	           (int8)(sslpnphy_specific->Listen_GaindB_AfrNoiseCal -
	                  sslpnphy_specific->Listen_GaindB_BfrNoiseCal)));

	WL_PHYCAL(("Get_RF_Gain = %d NfVal_Set = %d Base_Gain = %d Base_Rxpo = %d"
	           " Base_NfVal = %d Noise_Measure_State = %d\n",
	           sslpnphy_specific->Listen_RF_Gain,
	           sslpnphy_specific->NfSubtractVal_AfrNoiseCal,
	           sslpnphy_specific->Listen_GaindB_BASE,
	           sslpnphy_specific->RxpowerOffset_Required_BASE,
	           sslpnphy_specific->NfSubtractVal_BASE,
	           noise_measure_state));

	for (i = 0; i < SSLPNPHY_NOISE_PWR_FIFO_DEPTH; i++) {
		WL_PHYCAL(("I is %d:MIN_FIFO = %d:MAX_FIFO = %d \n", i,
			sslpnphy_specific->sslpnphy_noisepwr_fifo_Min[i],
			sslpnphy_specific->sslpnphy_noisepwr_fifo_Max[i]));
	}

	if (!sslpnphy_specific->sslpnphy_init_noise_cal_done) {
		wlc_sslpnphy_detection_disable(pi, FALSE);

		sslpnphy_specific->sslpnphy_init_noise_cal_done = TRUE;
	}

	if ((sslpnphy_specific->sslpnphy_init_noise_cal_done == 1) &&
	    !sslpnphy_specific->sslpnphy_disable_noise_percal) {
		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
			M_SSLPNPHY_NOISE_SAMPLES)), 80 << phybw40);

		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
			M_55f_REG_VAL)), 0);
		OR_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccommand, MCMD_BG_NOISE);
	}
}

void
wlc_sslpnphy_auxadc_measure(wlc_phy_t *ppi, bool readVal)
{
	uint16 tssi_val;
	phy_info_t *pi = (phy_info_t *)ppi;
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
#if defined(ROMTERMPHY)
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif

	if (WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr + M_SSLPNPHY_TSSICAL_EN))) == 0) {
		if (readVal) {
			tssi_val = ((phy_reg_read(pi, SSLPNPHY_rssiaccValResult0) << 0) +
			            (phy_reg_read(pi, SSLPNPHY_rssiaccValResult1) << 16));
			tssi_val = tssi_val >> 6;
			if (tssi_val > 31) {
				tssi_val = 31;
			}
			sslpnphy_specific->sslpnphy_auxadc_val = tssi_val+32;
			/* Write measured idle TSSI value */
			PHY_REG_MOD(pi, SSLPNPHY, TxPwrCtrlIdleTssi, idleTssi0, tssi_val);
		}

		/* write the registers */
		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(SSLPNPHY, NumrssiSamples, 0x40)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, rssiwaittime, 0x50)
		PHY_REG_LIST_EXECUTE(pi);

		/* trigger the ucode again */
#ifdef ROMTERMPHY
		if (sdna_board_flag)
			wlc_sslpnphy_write_shm_tssiCalEn(pi, 1);
		else
#endif
			WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr +
				M_SSLPNPHY_TSSICAL_EN)), 0x1);
		sslpnphy_specific->sslpnphy_last_idletssi_cal = GENERIC_PHY_INFO(pi)->now;
	}
}


/* don't use this directly. use wlc_get_band_range whenever possible */
int
BCMROMFN(wlc_get_ssn_lp_band_range)(uint freq)
{
	int range = -1;

	if (freq < 2500)
		range = WL_CHAN_FREQ_RANGE_2G;
	else if (freq <= 5320)
		range = WL_CHAN_FREQ_RANGE_5GL;
	else if (freq <= 5700)
		range = WL_CHAN_FREQ_RANGE_5GM;
	else
		range = WL_CHAN_FREQ_RANGE_5GH;

	return range;
}

static void
WLBANDINITFN(wlc_radio_2063_init_sslpnphy)(phy_info_t *pi)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint8 phybw40 = IS40MHZ(pi);

	WL_INFORM(("wl%d: %s\n", GENERIC_PHY_INFO(pi)->unit, __FUNCTION__));

	/* Load registers from the table */
	wlc_sslpnphy_init_radio_regs(pi, sslpnphy_radio_regs_2063, RADIO_DEFAULT_CORE);

	/* Set some PLL registers overridden by DC/CLB */
	write_radio_reg(pi, RADIO_2063_LOGEN_SP_5, 0x0);

	or_radio_reg(pi, RADIO_2063_COMMON_08, (0x07 << 3));
	write_radio_reg(pi, RADIO_2063_BANDGAP_CTRL_1, 0x56);

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, 0x1 << 1, 0);
	} else {
		if (phybw40 == 0) {
			/* Set rx lpf bw to 9MHz */
			mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, 0x1 << 1, 0);
		} else if (phybw40 == 1) {
			/* Set rx lpf bw to 19MHz for 40Mhz operation */
			mod_radio_reg(pi, RADIO_2063_RXBB_CTRL_2, 3 << 1, 1 << 1);
			or_radio_reg(pi, RADIO_2063_COMMON_02, (0x1 << 1));
			mod_radio_reg(pi, RADIO_2063_RXBB_SP_4, 7 << 4, 0x30);
			and_radio_reg(pi, RADIO_2063_COMMON_02, (0x0 << 1));
		}
	}

	/*
	 * Apply rf reg settings to mitigate 2063 spectrum
	 * asymmetry problems, including setting
	 * PA and PAD in class A mode
	 */
	write_radio_reg(pi, RADIO_2063_PA_SP_7, 0);
	/* pga/pad */
	write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0x20);

	write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0x40);
	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		/*  pa cascode voltage */
		write_radio_reg(pi, RADIO_2063_COMMON_05, 0x82);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0x50);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0x80);
	}

	/*  PA, PAD class B settings */
	write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x15);

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
		write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x09);
		write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x21);

		if ((sslpnphy_specific->sslpnphy_fabid == 2) ||
			(sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
			write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x30);
			write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x60);
			write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x1);
			write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x00);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0x00);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0xF0);
			sslpnphy_specific->sslpnphy_radio_classA = TRUE;
		}
	} else /* if (SSLPNREV_GE(pi->pubpi.phy_rev, 2)) */{
		write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x09);
		write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x21);
		write_radio_reg(pi, RADIO_2063_TXRF_SP_15, 0xc8);
		if (phybw40 == 1) {
			/*  PA, PAD class B settings */
			write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x10);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0xF0);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0xF0);
			write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x10);
			write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x1);
			write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x30);
		}
		if (SSLPNREV_IS(pi->pubpi.phy_rev, 3)) {
			write_radio_reg(pi, RADIO_2063_COMMON_06, 0x30);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0xff);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0x00);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_12,
			        read_radio_reg(pi, RADIO_2063_TXRF_SP_12) | 0x0f);
			write_radio_reg(pi, RADIO_2063_TXRF_SP_15, 0xc8);

		    if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA) {
		        write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x0);
		        write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x30);
		        write_radio_reg(pi, RADIO_2063_PA_SP_3, 0xf0);
		        write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x30);
		    } else {
		        write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x10);
		        write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x00);
		        write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x0b);
		        write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x2e);
			if (sslpnphy_specific->sslpnphy_fabid_otp == SMIC_FAB4) {
				if (phybw40) {
					write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x30);
					write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x50);
					write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x0);
				} else {
					write_radio_reg(pi, RADIO_2063_PA_SP_2, 0x28);
					write_radio_reg(pi, RADIO_2063_PA_SP_3, 0x27);
					write_radio_reg(pi, RADIO_2063_PA_SP_4, 0x0);
				}
				write_radio_reg(pi, RADIO_2063_TXRF_SP_6, 0xf5);
				write_radio_reg(pi, RADIO_2063_TXRF_SP_9, 0x08);
			}
		    }
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

STATIC uint32
BCMROMFN(wlc_lpphy_qdiv_roundup)(uint32 divident, uint32 divisor, uint8 precision)
{
	uint32 quotient, remainder, roundup, rbit;

	ASSERT(divisor);

	quotient = divident / divisor;
	remainder = divident % divisor;
	rbit = divisor & 1;
	roundup = (divisor >> 1) + rbit;

	while (precision--) {
		quotient <<= 1;
		if (remainder >= roundup) {
			quotient++;
			remainder = ((remainder - roundup) << 1) + rbit;
		} else {
			remainder <<= 1;
		}
	}

	/* Final rounding */
	if (remainder >= roundup)
		quotient++;

	return quotient;
}

void
wlc_2063_vco_cal(phy_info_t *pi)
{
	uint8 calnrst;

	/* Power up VCO cal clock */
	mod_radio_reg(pi, RADIO_2063_PLL_SP_1, 1 << 6, 0);

	calnrst = read_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST) & 0xf8;
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST, calnrst);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST, calnrst | 0x04);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST, calnrst | 0x06);
	OSL_DELAY(1);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_CALNRST, calnrst | 0x07);
	OSL_DELAY(300);

	/* Power down VCO cal clock */
	mod_radio_reg(pi, RADIO_2063_PLL_SP_1, 1 << 6, 1 << 6);
}
#ifdef BAND5G
static void
aband_tune_radio_reg(phy_info_t *pi, uint16 address, uint8 val, uint valid)
{
	if (valid) {
		write_radio_reg(pi, address, val);
	} else {
		return;
	}
}
static void
wlc_sslpnphy_radio_2063_channel_tweaks_A_band(phy_info_t *pi, uint freq)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint8 i;
	const chan_info_2063_sslpnphy_aband_tweaks_t *ci;
	const chan_info_2063_sslpnphy_X17_aband_tweaks_t * ci_x17;
	const chan_info_2063_sslpnphy_ninja_aband_tweaks_t * ci_ninja;
	const chan_info_2063_sslpnphy_manhattan_aband_tweaks_t * ci_manhattan;
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);

	write_radio_reg(pi, RADIO_2063_PA_SP_6, 0x7f);
	write_radio_reg(pi, RADIO_2063_TXRF_SP_17, 0xff);
	write_radio_reg(pi, RADIO_2063_TXRF_SP_13, 0xff);
	write_radio_reg(pi, RADIO_2063_TXRF_SP_5, 0xff);
	write_radio_reg(pi, RADIO_2063_PA_CTRL_5, 0x50);
	wlc_sslpnphy_set_pa_gain(pi, 116);
	if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)
		wlc_sslpnphy_set_pa_gain(pi, 0x10);

	for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy_aband_tweaks); i++) {
		if (freq <= chan_info_2063_sslpnphy_aband_tweaks[i].freq)
			break;
	}
	ASSERT(i < ARRAYSIZE(chan_info_2063_sslpnphy_aband_tweaks));
	ci = &chan_info_2063_sslpnphy_aband_tweaks[i];
	ASSERT(i < ARRAYSIZE(chan_info_2063_sslpnphy_X17_aband_tweaks));
	ci_x17 = &chan_info_2063_sslpnphy_X17_aband_tweaks[i];
	ASSERT(i < ARRAYSIZE(chan_info_2063_sslpnphy_ninja_aband_tweaks));
	ci_ninja = &chan_info_2063_sslpnphy_ninja_aband_tweaks[i];

	/* fix to make sure the correct entries are picked up from the radio tuning structure */
	for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy_manhattan_aband_tweaks); i++) {
		if (freq <= chan_info_2063_sslpnphy_manhattan_aband_tweaks[i].freq)
			break;
	}
	ASSERT(i < ARRAYSIZE(chan_info_2063_sslpnphy_manhattan_aband_tweaks));
	ci_manhattan = &chan_info_2063_sslpnphy_manhattan_aband_tweaks[i];

	i = 12;
	aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_11,
		ci->RF_PA_CTRL_11, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_8,
		ci->RF_TXRF_CTRL_8, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_5,
		ci->RF_TXRF_CTRL_5, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_2,
		ci->RF_TXRF_CTRL_2, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_4,
		ci->RF_TXRF_CTRL_4, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_7,
		ci->RF_TXRF_CTRL_7, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_6,
		ci->RF_TXRF_CTRL_6, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_2,
		ci->RF_PA_CTRL_2, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_5,
		ci->RF_PA_CTRL_5, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_15,
		ci->RF_TXRF_CTRL_15, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_14,
		ci->RF_TXRF_CTRL_14, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_7,
		ci->RF_PA_CTRL_7, (ci->valid_tweak & (0x1 << i--)));
	aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_15,
		ci->RF_PA_CTRL_15, (ci->valid_tweak & (0x1 << i)));

	write_radio_reg(pi, RADIO_2063_PA_CTRL_5, 0x40);

	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17M_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329OLYMPICX17U_SSID) ||
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329MOTOROLA_SSID)) {
		i = 7;
		aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_5,
			ci_x17->MRT_RF_PA_CTRL_5, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_8,
			ci_x17->MRT_RF_TXRF_CTRL_8, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_5,
			ci_x17->MRT_RF_TXRF_CTRL_5, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_2,
			ci_x17->MRT_RF_TXRF_CTRL_2, (ci_x17->valid_tweak & (0x1 << i)));
	}
	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) ||
		(CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID)) { /* 4319 5G */
		i = 3;
		aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_5,
			ci_x17->BCM_RF_PA_CTRL_5, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_8,
			ci_x17->BCM_RF_TXRF_CTRL_8, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_5,
			ci_x17->BCM_RF_TXRF_CTRL_5, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_2,
			ci_x17->BCM_RF_TXRF_CTRL_2, (ci_x17->valid_tweak & (0x1 << i)));
	}
	if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA) {
		i = 11;
		aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_11,
			ci_x17->ePA_RF_PA_CTRL_11, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_8,
			ci_x17->ePA_RF_TXRF_CTRL_8, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_5,
			ci_x17->ePA_RF_TXRF_CTRL_5, (ci_x17->valid_tweak & (0x1 << i--)));
		aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_2,
			ci_x17->ePA_RF_TXRF_CTRL_2, (ci_x17->valid_tweak & (0x1 << i--)));

	}
	write_radio_reg(pi, RADIO_2063_PA_CTRL_7, 0x2);
	if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) {
		if (freq == 5600)
			write_radio_reg(pi, RADIO_2063_PA_CTRL_7, 0x10);
	}

	write_radio_reg(pi, RADIO_2063_PA_CTRL_2, 0x90);
	write_radio_reg(pi, RADIO_2063_PA_CTRL_7, 0x0);
	if (freq == 5680)
		write_radio_reg(pi, RADIO_2063_TXRF_CTRL_1, 0xa1);
	write_radio_reg(pi, RADIO_2063_TXBB_CTRL_1, 0x10);
	if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA) {
		write_radio_reg(pi, RADIO_2063_PA_CTRL_7, 0x20);
		write_radio_reg(pi, RADIO_2063_PA_CTRL_2, 0x20);
	}

	sslpnphy_specific->sslpnphy_radio_classA = TRUE;
	if (CHIPID(GENERIC_PHY_INFO(pi)->sih->chip) == BCM4319_CHIP_ID) {
		/* 4319 5G iPA tuning */
	/* radio register tunings for Manhattan, currently same as Ninja */
	if (sdna_board_flag) {
		i = 5;
		    aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_2,
		        ci_manhattan->RF_PA_CTRL_2, (ci_manhattan->valid_tweak & (0x1 << i--)));
		    aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_5,
		        ci_manhattan->RF_PA_CTRL_5, (ci_manhattan->valid_tweak & (0x1 << i--)));
		    aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_7,
		        ci_manhattan->RF_PA_CTRL_7, (ci_manhattan->valid_tweak & (0x1 << i--)));
		    aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_11,
		        ci_manhattan->RF_PA_CTRL_11, (ci_manhattan->valid_tweak & (0x1 << i--)));
		    aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_2,
		        ci_manhattan->RF_TXRF_CTRL_2, (ci_manhattan->valid_tweak & (0x1 << i--)));
		    aband_tune_radio_reg(pi, RADIO_2063_TXRF_CTRL_5,
		        ci_manhattan->RF_TXRF_CTRL_5, (ci_manhattan->valid_tweak & (0x1 << i--)));
	} else {
		i = 4;
		    aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_2,
		        ci_ninja->RF_PA_CTRL_2, (ci_ninja->valid_tweak & (0x1 << i--)));
		    aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_5,
		        ci_ninja->RF_PA_CTRL_5, (ci_ninja->valid_tweak & (0x1 << i--)));
		    aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_7,
		        ci_ninja->RF_PA_CTRL_7, (ci_ninja->valid_tweak & (0x1 << i--)));
		    aband_tune_radio_reg(pi, RADIO_2063_PA_CTRL_11,
		        ci_ninja->RF_PA_CTRL_11, (ci_ninja->valid_tweak & (0x1 << i--)));
	}

		/* 4319 5G RX LNA tuning */
		if (freq <= 5300) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xF);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0xF);
		} else if (freq < 5500) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xf);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0xc);
		} else if (freq < 5520) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xf);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x9);
		} else if (freq < 5560) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xf);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x8);
		} else if (freq < 5580) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xf);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x7);
		} else if (freq < 5600) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xf);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x6);
		} else if (freq < 5620) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xf);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x3);
		} else if (freq < 5640) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xf);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x2);
		} else if (freq < 5680) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xf);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x1);
		} else if (freq < 5700) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xd);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x0);
		} else if (freq < 5745) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0xc);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x0);
		} else if (freq < 5765) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0x7);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x0);
		} else if (freq < 5785) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0x5);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x0);
		} else if (freq < 5805) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0x4);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x0);
		} else if (freq < 5825) {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0x2);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x0);
		} else {
			write_radio_reg(pi, RADIO_2063_ARX_1ST_3, 0x1);
			write_radio_reg(pi, RADIO_2063_ARX_2ND_1, 0x0);
		}
	}
}
STATIC void
wlc_sslpnphy_pll_aband_tune(phy_info_t *pi, uint8 channel)
{
	uint8 i;
	uint freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	const chan_info_2063_sslpnphy_X17_epa_tweaks_t * ci_x17;
	for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy_X17_epa_tweaks); i++) {
		if (freq <= chan_info_2063_sslpnphy_X17_epa_tweaks[i].freq)
			break;
	}
	if (i >= ARRAYSIZE(chan_info_2063_sslpnphy_X17_epa_tweaks)) {
		WL_ERROR(("wl%d: %s: freq %d not found in channel table\n",
			GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, freq));
		return;
	}
	ci_x17 = &chan_info_2063_sslpnphy_X17_epa_tweaks[i];

	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_2, ci_x17->ePA_JTAG_PLL_CP_2);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_3, ci_x17->ePA_JTAG_PLL_CP_3);
}
#endif /* BAND5G */
static void
wlc_sslpnphy_radio_2063_channel_tune(phy_info_t *pi, uint8 channel)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	uint i;
	const chan_info_2063_sslpnphy_t *ci;
	uint8 rfpll_doubler = 1;
	uint16 rf_common15;
	fixed qFref, qFvco, qFcal, qVco, qVal;
	uint8  to, refTo, cp_current, kpd_scale, ioff_scale, offset_current;
	uint32 setCount, div_int, div_frac, iVal, fvco3, fref, fref3, fcal_div;
	uint16 loop_bw = 0;
	uint16 d30 = 0;
	uint16 temp_pll, temp_pll_1;
	uint16  h29, h30;
	bool e44, e45;
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);

	ci = &chan_info_2063_sslpnphy[0];

	if ((BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94329AGBF_SSID) &&
	    CHSPEC_IS5G(pi->radio_chanspec) && !(BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) &
	    BFL_HGPA)) {
	    rfpll_doubler  = 0;
	}
	/* sdna: doubler enabling */
	if (sdna_board_flag)
	    rfpll_doubler  = 0;

	if (rfpll_doubler)
	    si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, PMU1_PLL0_CHIPCTL0, 0x20000, 0x00000);
	else
	    si_pmu_chipcontrol(GENERIC_PHY_INFO(pi)->sih, PMU1_PLL0_CHIPCTL0, 0x20000, 0x20000);

	/* lookup radio-chip-specific channel code */
	if (CHSPEC_IS2G(pi->radio_chanspec)) {
		for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy); i++)
			if (chan_info_2063_sslpnphy[i].chan == channel)
				break;

		if (i >= ARRAYSIZE(chan_info_2063_sslpnphy)) {
			WL_ERROR(("wl%d: %s: channel %d not found in channel table\n",
				GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, channel));
			return;
		}
		ci = &chan_info_2063_sslpnphy[i];
#ifndef BAND5G
	}
#else
	} else {
		for (i = 0; i < ARRAYSIZE(chan_info_2063_sslpnphy_aband); i++)
			if (chan_info_2063_sslpnphy_aband[i].chan == channel)
				break;
		if (i >= ARRAYSIZE(chan_info_2063_sslpnphy_aband)) {
			WL_ERROR(("wl%d: %s: channel %d not found in channel table\n",
				GENERIC_PHY_INFO(pi)->unit, __FUNCTION__, channel));
			return;
		}
		ci = &chan_info_2063_sslpnphy_aband[i];
	}
#endif
	/* Radio tunables */
	write_radio_reg(pi, RADIO_2063_LOGEN_VCOBUF_1, ci->RF_logen_vcobuf_1);
	write_radio_reg(pi, RADIO_2063_LOGEN_MIXER_2, ci->RF_logen_mixer_2);
	write_radio_reg(pi, RADIO_2063_LOGEN_BUF_2, ci->RF_logen_buf_2);
	write_radio_reg(pi, RADIO_2063_LOGEN_RCCR_1, ci->RF_logen_rccr_1);
	write_radio_reg(pi, RADIO_2063_GRX_1ST_3, ci->RF_grx_1st_3);
	if ((sslpnphy_specific->sslpnphy_fabid == 2) || (sslpnphy_specific->sslpnphy_fabid_otp == TSMC_FAB12)) {
		if (channel == 4)
			write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x0b);
		else if (channel == 5)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x0b);
		else if (channel == 6)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x0a);
		else if (channel == 7)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x09);
		else if (channel == 8)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x09);
		else if (channel == 9)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x08);
		else if (channel == 10)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x05);
		else if (channel == 11)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x04);
		else if (channel == 12)
		        write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x03);
		else if (channel == 13)
			write_radio_reg(pi, RADIO_2063_GRX_1ST_3, 0x02);
	}
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

	/* write_radio_reg(pi, RADIO_2063_LOGEN_SPARE_2, ci->dummy4); */
	/* Turn on PLL power supplies */
	rf_common15 = read_radio_reg(pi, RADIO_2063_COMMON_15);
	write_radio_reg(pi, RADIO_2063_COMMON_15, rf_common15 | (0x0f << 1));

	/* Calculate various input frequencies */
	fref = rfpll_doubler ? PHY_XTALFREQ(pi->xtalfreq) : (PHY_XTALFREQ(pi->xtalfreq) << 1);
	if (rfpll_doubler == 0) {
	    e44 = 1;
	} else {
	    if (PHY_XTALFREQ(pi->xtalfreq) > 26000000)
	        e44 = 1;
	    else
	        e44 = 0;
	}

	if (e44 == 0) {
	    e45 = 0;
	} else {
	    if (fref > 52000000)
	        e45 = 1;
	    else
	        e45 = 0;
	}

	if (e44 == 0) {
	    fcal_div = 1;
	} else {
	    if (e45 == 0)
	        fcal_div = 2;
	    else
	        fcal_div = 4;
	}

	if (ci->freq > 2484)
		fvco3 = (ci->freq << 1);
	else
		fvco3 = (ci->freq << 2);
	fref3 = 3 * fref;

	/* Convert into Q16 MHz */
	qFref =  wlc_lpphy_qdiv_roundup(fref, PLL_2063_MHZ, 16);
	qFcal = wlc_lpphy_qdiv_roundup(fref, fcal_div * PLL_2063_MHZ, 16);
	qFvco = wlc_lpphy_qdiv_roundup(fvco3, 3, 16);

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
		(fixed)wlc_lpphy_qdiv_roundup(qFvco, qFcal * 16, 16) * (refTo + 1) * (to + 1)) - 1;
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
	div_frac = wlc_lpphy_qdiv_roundup(div_frac, fref3, 20);

	/* Program PLL */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_1, (0x1f << 0), (uint8)(div_int >> 4));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_2, (0x1f << 4), (uint8)(div_int << 4));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_2, (0x0f << 0), (uint8)(div_frac >> 16));
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_3, (uint8)(div_frac >> 8) & 0xff);
	write_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_SG_4, (uint8)div_frac & 0xff);

	/* REmoving the hard coded values for PLL registers and make it */
	/* programmable with loop bw and d30 */

	/* PLL_cp_current */
	qVco = ((PLL_2063_HIGH_END_KVCO - PLL_2063_LOW_END_KVCO) *
		((qFvco - FIXED(PLL_2063_LOW_END_VCO)) /
		(PLL_2063_HIGH_END_VCO - PLL_2063_LOW_END_VCO))) +
		FIXED(PLL_2063_LOW_END_KVCO);
	if ((CHSPEC_IS5G(pi->radio_chanspec) && (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA)) ||
		(!rfpll_doubler)) {
		/* sdna: d30 register = 3000 instead of 1500 if doubler enabled */
		if (sdna_board_flag) {
			/* Channel 5500: loop_bw = 300 & d30=4000; All others: loop_bw=500 & d30=3000 */
			if (channel == 100) {
				loop_bw = PLL_2063_LOOP_BW_SDNA_2;
				d30 = PLL_2063_D30_SDNA_2;
			} else {
				loop_bw = PLL_2063_LOOP_BW_SDNA_1;
				d30 = PLL_2063_D30_SDNA_1;
			}
		} else {
			loop_bw = PLL_2063_LOOP_BW_ePA;
			d30 = PLL_2063_D30_ePA;
		}
	} else {
		loop_bw = PLL_2063_LOOP_BW;
		d30 = PLL_2063_D30;
	}
	h29 = (uint16) wlc_lpphy_qdiv_roundup(loop_bw * 10, 270, 0); /* h29 * 10 */
	h30 = (uint16) wlc_lpphy_qdiv_roundup(d30 * 10, 2640, 0); /* h30 * 10 */

	/* PLL_lf_r1 */
	temp_pll = (uint16) wlc_lpphy_qdiv_roundup((d30 - 680), 490, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_3, 0x1f << 3, temp_pll << 3);

	/* PLL_lf_r2 */
	temp_pll = (uint16) wlc_lpphy_qdiv_roundup((1660 * h30 - 6800), 4900, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_3, 0x7, (temp_pll >> 2));
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_4, 0x3 << 5, (temp_pll & 0x3) << 5);

	/* PLL_lf_r3 */
	temp_pll = (uint16) wlc_lpphy_qdiv_roundup((1660 * h30 - 6800), 4900, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_4, 0x1f, temp_pll);

	/* PLL_lf_c1 */
	temp_pll = (uint16) wlc_lpphy_qdiv_roundup(1046500, h30 * h29, 0);
	temp_pll_1 = (uint16) wlc_lpphy_qdiv_roundup((temp_pll - 1775), 555, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_1, 0xf << 4, temp_pll_1 << 4);

	/* PLL_lf_c2 */
	temp_pll = (uint16) wlc_lpphy_qdiv_roundup(61700, h29 * h30, 0);
	temp_pll_1 = (uint16) wlc_lpphy_qdiv_roundup((temp_pll - 123), 38, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_1, 0xf, temp_pll_1);

	/* PLL_lf_c3 */
	temp_pll = (uint16) wlc_lpphy_qdiv_roundup(27000, h29 * h30, 0);
	temp_pll_1 = (uint16) wlc_lpphy_qdiv_roundup((temp_pll - 61), 19, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_2, 0xf << 4, temp_pll_1 << 4);

	/* PLL_lf_c4 */
	temp_pll = (uint16) wlc_lpphy_qdiv_roundup(26400, h29 * h30, 0);
	temp_pll_1 = (uint16) wlc_lpphy_qdiv_roundup((temp_pll - 55), 19, 0);
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_LF_2, 0xf, temp_pll_1);


	iVal = ((d30 - 680)  + (490 >> 1))/ 490;
	qVal = wlc_lpphy_qdiv_roundup(
		440 * loop_bw * div_int,
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
	qVal = wlc_lpphy_qdiv_roundup(100 * qFref, qFvco, 16) * (cp_current + 8) * (kpd_scale + 1);
	ioff_scale = (qVal > FIXED(150)) ? 1 : 0;
	qVal = (qVal / (6 * (ioff_scale + 1))) - FIXED(2);
	if (qVal < 0)
		offset_current = 0;
	else
		offset_current = FLOAT(qVal + (FIXED(1) >> 1));
	/* sdna: offset_current is doubled if doubler enabled */
	if (sdna_board_flag)
	    offset_current = offset_current*2;
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_3, 0x1f, offset_current);

	/*  PLL_ioff_scale2 */
	mod_radio_reg(pi, RADIO_2063_PLL_JTAG_PLL_CP_3, 1 << 5, ioff_scale << 5);
#ifdef BAND5G
	/* for sdna boards, no more pll_aband_tune is required */
	if ((CHSPEC_IS5G(pi->radio_chanspec)) && (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_HGPA) &&
		!(sdna_board_flag))
		wlc_sslpnphy_pll_aband_tune(pi, channel);
#endif
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
	wlc_2063_vco_cal(pi);

	/* Restore state */
	write_radio_reg(pi, RADIO_2063_COMMON_15, rf_common15);
}

#if !defined(ROMTERMPHY)
/* these two routines are obsolete */
int8
wlc_phy_get_tx_power_offset(wlc_phy_t *ppi, uint8 tbl_offset)
{
	return 0;
}

int8
wlc_phy_get_tx_power_offset_by_mcs(wlc_phy_t *ppi, uint8 mcs_offset)
{
	return 0;
}
#endif /* PHYHAL */

#if defined(BCMDBG) || defined(WLTEST)
STATIC uint32 ltrn_list_sslpnphy[] = {
	0x40000, 0x2839e, 0xfdf3b, 0xf370a, 0x1074a, 0x2cfef, 0x27888, 0x0f89f, 0x0882e,
	0x16fa5, 0x18b70, 0xf8f8e, 0xd0fa6, 0xcbf81, 0xf0752, 0x1a76e, 0x283d6, 0x1e826,
	0x15c07, 0x12393, 0x00b44, 0xddf60, 0xc83b2, 0xdb7cf, 0x0a3a0, 0x26787, 0x183e8,
	0xfa09b, 0xf6d07, 0x148bd, 0x30ff9, 0x2f372, 0x19b9a, 0x0d434, 0x0f0a1, 0x07890,
	0xe8840, 0xc882b, 0xca46b, 0xf30ab, 0x21c97, 0x31045, 0x1c817, 0xfc043, 0xe7485,
	0xe346c, 0xe8fdc, 0xf073b, 0xf1b09, 0xe5b62, 0xce3e5, 0xbe017, 0xcbfde, 0xf6f8e,
	0x1ef87, 0x21fdf, 0xfec58, 0xda8a9, 0xda4bd, 0xff0b2, 0x258ae, 0x294b1, 0x050a0,
	0xd5862, 0xc0000, 0xd5b9e, 0x05360, 0x2974f, 0x25b52, 0xff34e, 0xda743, 0xdaf57,
	0xfefa8, 0x21c21, 0x1ec79, 0xf6c72, 0xcbc22, 0xbe3e9, 0xce01b, 0xe589e, 0xf1cf7,
	0xf04c6, 0xe8c24, 0xe3794, 0xe777b, 0xfc3bd, 0x1cbe9, 0x313bb, 0x21f69, 0xf3755,
	0xca795, 0xc8bd5, 0xe8bbf, 0x07b71, 0x0f35f, 0x0d7cc, 0x19866, 0x2f08e, 0x30c07,
	0x14b43, 0xf6ef9, 0xfa365, 0x18019, 0x26479, 0x0a060, 0xdb431, 0xc804e, 0xddca0,
	0x008bc, 0x1206c, 0x15ff9, 0x1ebd9, 0x2802a, 0x1a492, 0xf04ae, 0xcbc7f, 0xd0c5a,
	0xf8c72, 0x18890, 0x16c5b, 0x08fd2, 0x0ff62, 0x27f78, 0x2cc11, 0x104b6, 0xf34f6,
	0xfe0c5, 0x28462

};


int
wlc_phy_sslpnphy_long_train(phy_info_t *pi, int channel)
{
	uint16 num_samps;
#ifdef ROMTERMPHY
	/* stop any test in progress */
	if (GENERIC_PHY_INFO(pi)->phytest_on)
#endif
		wlc_stop_test(pi);

	/* channel 0 means restore original contents and end the test */
	if (channel == 0) {
		wlc_sslpnphy_stop_tx_tone(pi);
		wlc_sslpnphy_clear_deaf(pi);
		PHY_REG_LIST_START
			PHY_REG_WRITE_ENTRY(SSLPNPHY, sslpnCtrl3, 1)
			PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, samplePlayClkEn, 0)
		PHY_REG_LIST_EXECUTE(pi);
		WL_ENABLE_MAC(pi);
		return 0;
	}

	wlc_sslpnphy_set_deaf(pi);
	WL_SUSPEND_MAC_AND_WAIT(pi);

	if (wlc_init_test(pi, channel, TRUE)) {
		return 1;
	}

	num_samps = sizeof(ltrn_list_sslpnphy)/sizeof(*ltrn_list_sslpnphy);
	/* load sample table */
	PHY_REG_LIST_START
		PHY_REG_WRITE_ENTRY(SSLPNPHY, sslpnCtrl3, 0)
		PHY_REG_MOD_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, samplePlayClkEn, 1)
	PHY_REG_LIST_EXECUTE(pi);
	wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY,
		ltrn_list_sslpnphy, num_samps, 32, 0);

	wlc_sslpnphy_run_samples(pi, num_samps, 0xffff, 0, 0);

	return 0;
}
#endif 

#if defined(WLTEST)
/* This function calculates RSSI and SNR based on the statistics */
/* dumped by the ucode into SHM */
#if !defined(ROMTERMPHY)
void
wlc_sslpnphy_pkteng_stats_get(phy_info_t *pi, wl_pkteng_stats_t *stats)
{
	wlc_sslpnphy_rssi_snr_calc(pi, NULL, &stats->rssi, &stats->snr);
}

#endif
void
wlc_sslpnphy_rssi_snr_calc(phy_info_t *pi, void * hdl, int32 * rssi, int32 * snr)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif
	wl_pkteng_stats_t stats;

	int16 rssi_sslpn1 = 0, rssi_sslpn2 = 0, rssi_sslpn3 = 0, rssi_sslpn4 = 0;
	int16 snr_a_sslpn1, snr_a_sslpn2, snr_a_sslpn3, snr_a_sslpn4;
	int16 snr_b_sslpn1, snr_b_sslpn2, snr_b_sslpn3, snr_b_sslpn4;
	uint8 gidx1, gidx2, gidx3, gidx4;
	int8 snr1, snr2, snr3, snr4;
	uint freq;
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);

	rssi_sslpn1 = (int8)WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_0)));
	rssi_sslpn2 = (int8)WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_1)));
	rssi_sslpn3 = (int8)WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_2)));
	rssi_sslpn4 = (int8)WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_3)));

	gidx1 = (WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_0))) & 0xfe00) >> 9;
	gidx2 = (WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_1))) & 0xfe00) >> 9;
	gidx3 = (WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_2))) & 0xfe00) >> 9;
	gidx4 = (WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_3))) & 0xfe00) >> 9;

	rssi_sslpn1 = rssi_sslpn1 + sslpnphy_gain_index_offset_for_pkt_rssi[gidx1];
	if (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_0)))) & 0x0100) &&
		(gidx1 == 12))
		rssi_sslpn1 = rssi_sslpn1 - 4;
	if ((rssi_sslpn1 > -46) && (gidx1 > 18))
		rssi_sslpn1 = rssi_sslpn1 + 6;
	rssi_sslpn2 = rssi_sslpn2 + sslpnphy_gain_index_offset_for_pkt_rssi[gidx2];
	if (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_1)))) & 0x0100) &&
		(gidx2 == 12))
		rssi_sslpn2 = rssi_sslpn2 - 4;
	if ((rssi_sslpn2 > -46) && (gidx2 > 18))
		rssi_sslpn2 = rssi_sslpn2 + 6;
	rssi_sslpn3 = rssi_sslpn3 + sslpnphy_gain_index_offset_for_pkt_rssi[gidx3];
	if (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_2)))) & 0x0100) &&
		(gidx3 == 12))
		rssi_sslpn3 = rssi_sslpn3 - 4;
	if ((rssi_sslpn3 > -46) && (gidx3 > 18))
		rssi_sslpn3 = rssi_sslpn3 + 6;
	rssi_sslpn4 = rssi_sslpn4 + sslpnphy_gain_index_offset_for_pkt_rssi[gidx4];
	if (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_3)))) & 0x0100) &&
		(gidx4 == 12))
		rssi_sslpn4 = rssi_sslpn4 - 4;
	if ((rssi_sslpn4 > -46) && (gidx4 > 18))
		rssi_sslpn4 = rssi_sslpn4 + 6;

	stats.rssi = (rssi_sslpn1 + rssi_sslpn2 + rssi_sslpn3 + rssi_sslpn4) >> 2;

	/* Channel Correction Factor */
	freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
	if ((freq > 2427) && (freq <= 2467))
		stats.rssi = stats.rssi - 1;
	/* temperature compensation */
	stats.rssi = stats.rssi + sslpnphy_specific->sslpnphy_pkteng_rssi_slope;

	/* 2dB compensation of path loss for 4329 on Ref Boards */
	stats.rssi = stats.rssi + 2;
	if (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_RSSI_1)))) & 0x100) &&
		(stats.rssi > -84) && (stats.rssi < -33))
		stats.rssi = stats.rssi + 3;

	/* SNR */
	snr_a_sslpn1 = WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_SNR_A_0)));
	snr_b_sslpn1 = WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_SNR_B_0)));
	snr1 = ((snr_a_sslpn1 - snr_b_sslpn1)* 3) >> 5;
	if ((stats.rssi < -92) || (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +
		M_SSLPN_RSSI_0)))) & 0x100)))
		snr1 = stats.rssi + 94;

	if (snr1 > 31)
		snr1 = 31;

	snr_a_sslpn2 = WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_SNR_A_1)));
	snr_b_sslpn2 = WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_SNR_B_1)));
	snr2 = ((snr_a_sslpn2 - snr_b_sslpn2)* 3) >> 5;
	if ((stats.rssi < -92) || (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +
		M_SSLPN_RSSI_1)))) & 0x100)))
		snr2 = stats.rssi + 94;

	if (snr2 > 31)
		snr2 = 31;

	snr_a_sslpn3 = WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_SNR_A_2)));
	snr_b_sslpn3 = WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_SNR_B_2)));
	snr3 = ((snr_a_sslpn3 - snr_b_sslpn3)* 3) >> 5;
	if ((stats.rssi < -92) || (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +
		M_SSLPN_RSSI_2)))) & 0x100)))
		snr3 = stats.rssi + 94;

	if (snr3 > 31)
		snr3 = 31;

	snr_a_sslpn4 = WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_SNR_A_3)));
	snr_b_sslpn4 = WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +  M_SSLPN_SNR_B_3)));
	snr4 = ((snr_a_sslpn4 - snr_b_sslpn4)* 3) >> 5;
	if ((stats.rssi < -92) || (((WL_READ_SHM(pi, (2 * (sslpnphy_shm_ptr +
		M_SSLPN_RSSI_3)))) & 0x100)))
		snr4 = stats.rssi + 94;

	if (snr4 > 31)
		snr4 = 31;

	stats.snr = ((snr1 + snr2 + snr3 + snr4)/4);
	*snr = stats.snr;
	*rssi = stats.rssi;
}
#endif 

#define WAIT_FOR_SCOPE	4000000 /* in unit of us */

/* BCMATTACHFN like wlc_phy_txpwr_srom_read_nphy because it used exclusively by it. */
static void
#ifdef PPR_API
wlc_sslpnphy_txpwr_srom_convert(int8 *srom_max, uint16 *pwr_offset, uint8 tmp_max_pwr,
#else
BCMATTACHFN(wlc_sslpnphy_txpwr_srom_convert)(uint8 *srom_max, uint16 *pwr_offset, uint8 tmp_max_pwr,
#endif
#ifndef PPR_API
	uint8 rate_start, uint8 rate_end)
{
	uint8 rate;
	uint8 word_num, nibble_num;
#else
	uint rate_start, uint rate_end)
{
	uint rate, word_num;
	uint8 nibble_num;
#endif /* PPR_API */
	uint8 tmp_nibble;

	for (rate = rate_start; rate <= rate_end; rate++) {
		word_num = (rate - rate_start) >> 2;
		nibble_num = (rate - rate_start) & 0x3;
		tmp_nibble = (pwr_offset[word_num] >> 4 * nibble_num) & 0xf;
		/* nibble info indicates offset in 0.5dB units */
#ifdef PPR_API
		srom_max[rate] = (int8)(tmp_max_pwr - 2 * tmp_nibble);
#else
		srom_max[rate] = tmp_max_pwr - 2 * tmp_nibble;
#endif
	}
}

void
wlc_sslpnphy_setchan_cal(phy_info_t *pi, int32 int_val)
{
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#else
	wlc_info_t * wlc_pi = pi->wlc;
#if defined(WLTEST)
	bool sdna_board_flag =
		(BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID);
#endif 
#endif /* !ROMTERMPHY */
#if defined(SSLPNPHYCAL_CACHING) && defined(WLMEDIA_APCS)
	sslpnphy_calcache_t * ctx =
		wlc_phy_get_sslpnphy_chanctx(CHSPEC_CHANNEL(pi->radio_chanspec));
#endif

	if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) {
		sslpnphy_specific->sslpnphy_papd_peakcurmode.cal_at_init_done = FALSE;
		sslpnphy_specific->sslpnphy_papd_peakcurmode.predict_cal_count = 0;
		sslpnphy_specific->peak_current_mode = FALSE;
	}

	if (int_val != pi->radio_chanspec) {
		WL_ERROR(("%s: chanspec 0x%x != radio chanspec 0x%x\n",
		       __FUNCTION__, int_val, pi->radio_chanspec));
		return;
	}

	if (!(SCAN_IN_PROGRESS(wlc_pi) || WLC_RM_IN_PROGRESS(wlc_pi))) {
		uint freq;

		WL_INFORM(("%s : Doing a full cal for channel %d ",
		           __FUNCTION__, CHSPEC_CHANNEL(pi->radio_chanspec)));
		wlc_sslpnphy_full_cal(pi);
		freq = PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec));
		if (sslpnphy_specific->sslpnphy_last_tx_freq != (uint16)freq) {
			int old_band, new_band;
			old_band = wlc_get_ssn_lp_band_range
			    (sslpnphy_specific->sslpnphy_last_tx_freq);
			new_band = WL_PHY_BAND_RANGE(pi, pi->radio_chanspec);
#if defined(ROMTERMPHY)
			if ((old_band != new_band) || (pi->sslpnphy_ntd_gds_lowtxpwr == 1)) {
				pi->sslpnphy_ntd_gds_lowtxpwr = 0;
				wlc_sslpnphy_tx_pwr_ctrl_init(pi);
			}
#else
			if (old_band != new_band)
				wlc_sslpnphy_tx_pwr_ctrl_init(pi);
#endif /* ROMTERMPHY */
		}
#ifdef WLMEDIA_APCS
		if (!pi->dcs_skip_papd_recal)
#endif
		wlc_sslpnphy_papd_recal(pi);
		/* Tx iqlo calibration */
#if defined(SSLPNPHYCAL_CACHING) && defined(WLMEDIA_APCS)
		if (!pi->dcs_skip_papd_recal) {
			if (check_valid_channel_to_cache(CHSPEC_CHANNEL(pi->radio_chanspec))) {
				if (!ctx) {
					ctx = wlc_phy_create_sslpnphy_chanctx
						(CHSPEC_CHANNEL(pi->radio_chanspec));
				}
				if (ctx) {
					if (!ctx->valid) {
						wlc_sslpnphy_txpwrtbl_iqlo_cal(pi);
						/* Cache cal coefficients */
						wlc_phy_cal_cache_sslpnphy(pi);
					} else {
						/* Restore cached cal coefficients */
						wlc_phy_cal_cache_restore_sslpnphy(pi);
					}
				} else {
					wlc_sslpnphy_txpwrtbl_iqlo_cal(pi);
				}
			} else {
				wlc_sslpnphy_txpwrtbl_iqlo_cal(pi);
			}
		}
#else
		wlc_sslpnphy_txpwrtbl_iqlo_cal(pi);
#endif /* SSLPNPHYCAL_CACHING */
		WL_INFORM(("IOV_SSLPNPHY_SETCHAN_CAL: 0x%x\n", int_val));
	}
#if defined(WLTEST) && defined(ROMTERMPHY)
	/* average of lab-measured listen gain with dynamic noise-cal across splits */
	if (((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) >= SSLPNPHY_LOW_LISTEN_GAIN) &&
		((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) <= SSLPNPHY_HIGH_LISTEN_GAIN))
		pi->sslpn_nom_listen_gain = 0x49;
	else
		pi->sslpn_nom_listen_gain = 0x46;
	if (sdna_board_flag) {
		wlc_sslpnphy_set_crs(pi, SSLPNPHY_CRS_DESENSE_VAL);
	}
#endif 
}

#ifdef ROMTERMPHY
#if defined(BCMDBG) || defined(WLTEST) || defined(WLNINTENDO2DBG)
int
wlc_sslpnphy_txpwrindex_iovar(phy_info_t *pi, int32 int_val, uint8 band_idx)
{
	int err = 0;
	if (int_val == -1) {
		wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_HW);
		/* Reset calibration */
		sslpnphy_specific->sslpnphy_full_cal_channel[band_idx] = 0;
		pi->phy_forcecal = TRUE;
	} else if ((int_val >= 0) && (int_val <= SSLPNPHY_MAX_TX_POWER_INDEX)) {
		wlc_sslpnphy_set_tx_pwr_by_index(pi, (int)int_val);
	} else
		err = BCME_RANGE;

	return err;
}
#endif 
#endif /* ROMTERMPHY */

#if defined(BCMDBG) || defined(WLTEST)
#ifdef ROMTERMPHY
void
wlc_sslpnphy_percal_iovar(phy_info_t *pi, int32 int_val)
{
	if (int_val) {
		sslpnphy_specific->sslpnphy_force_1_idxcal = 1;
		sslpnphy_specific->sslpnphy_papd_nxt_cal_idx = int_val;
	}
	if (!NORADIO_ENAB(pi->pubpi))
		if (!(SCAN_IN_PROGRESS(pi->wlc) || WLC_RM_IN_PROGRESS(pi->wlc) ||
		      PLT_IN_PROGRESS(pi->wlc) ||
		      ASSOC_IN_PROGRESS(pi->wlc) || pi->carrier_suppr_disable ||
		      pi->pkteng_in_progress || pi->disable_percal)) {
			wlc_sslpnphy_periodic_cal_top(pi);
			WL_INFORM(("IOV_SSLPNPHY_PER_CAL: %d\n", int_val));
		}
}

void
wlc_sslpnphy_noise_measure_iovar(phy_info_t *pi)
{
	if (!NORADIO_ENAB(pi->pubpi))
		if (!(SCAN_IN_PROGRESS(pi->wlc) || WLC_RM_IN_PROGRESS(pi->wlc) ||
		      PLT_IN_PROGRESS(pi->wlc) ||
		      ASSOC_IN_PROGRESS(pi->wlc) || pi->carrier_suppr_disable ||
		      pi->pkteng_in_progress || pi->disable_percal ||
		      sslpnphy_specific->sslpnphy_disable_noise_percal)) {
			wlc_sslpnphy_noise_measure((wlc_phy_t *) pi);
			WL_INFORM(("IOV_SSLPNPHY_NOISE_MEASURE\n"));
		}
}

void
wlc_sslpnphy_papd_recal_iovar(phy_info_t *pi)
{
	if (!(SCAN_IN_PROGRESS(pi->wlc) || WLC_RM_IN_PROGRESS(pi->wlc))) {
		wlc_sslpnphy_papd_recal(pi);
		/* Skip tx iq if init is happening on same channel (Time savings) */
		if (!sslpnphy_specific->sslpnphy_restore_papd_cal_results)
			wlc_sslpnphy_txpwrtbl_iqlo_cal(pi);
		WL_INFORM(("IOV_SSLPNPHY_PAPD_RECAL\n"));
	} else {
		WL_INFORM((" %s : Not doing a full cal: Restoring the "
			"previous cal results for channel %d ", __FUNCTION__,
			sslpnphy_specific->sslpnphy_full_cal_channel[(CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0)]));
		sslpnphy_specific->sslpnphy_restore_papd_cal_results = 1;
		wlc_sslpnphy_papd_recal(pi);
	}
}
#endif /* ROMTERMPHY */
#endif /* defined(BCMDBG) || defined(WLTEST) */

#if defined(BCMDBG) || defined(WLTEST)
#ifdef ROMTERMPHY
void
wlc_sslpnphy_rxiqcal_iovar(phy_info_t *pi)
{
	uint16 tx_pwr_ctrl;
	bool suspend;
	suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		WL_WRITE_SHM(pi, M_CTS_DURATION, 10000);
		WL_SUSPEND_MAC_AND_WAIT(pi);
	}
	if (!NON_BT_CHIP(wlc))
		wlc_sslpnphy_btcx_override_enable(pi);
	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	wlc_sslpnphy_rx_iq_cal(pi,
		NULL,
		0,
		FALSE, TRUE, FALSE, TRUE, 127);

	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);
	if (!suspend)
		WL_ENABLE_MAC(pi);
}
#endif /* ROMTERMPHY */
#endif /* defined(BCMDBG) || defined(WLTEST) */

#if defined(BCMDBG) || defined(WLTEST)
#if !defined(ROMTERMPHY)
void
wlc_phy_init_test_sslpnphy(phy_info_t *pi)
{
	/* Force WLAN antenna */
	wlc_sslpnphy_btcx_override_enable(pi);
	/* Disable tx power control */
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);
	PHY_REG_MOD(pi, SSLPNPHY, papd_control, forcepapdClkOn, 1);
	/* Recalibrate for this channel */
	wlc_sslpnphy_full_cal(pi);
}
#endif /* !defined(ROMTERMPHY) */
#endif 


#if defined(WLTEST)
#if !defined(ROMTERMPHY)
int
wlc_sslpnphy_txpwr_idx_get(phy_info_t *pi)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	int32 ret_int;

	if (wlc_phy_tpc_isenabled_sslpnphy(pi)) {
		/* Update current power index */
		wlc_sslpnphy_tx_pwr_update_npt(pi);
		ret_int = ph->sslpnphy_tssi_idx;
	} else
		ret_int = ph->sslpnphy_tx_power_idx_override;

	return ret_int;
}
void
wlc_sslpnphy_iovar_txpwrctrl(phy_info_t *pi, int32 int_val)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phyreg_enter((wlc_phy_t *)pi);

	wlc_sslpnphy_set_tx_pwr_ctrl(pi,
		int_val ? SSLPNPHY_TX_PWR_CTRL_HW : SSLPNPHY_TX_PWR_CTRL_SW);
	/* Reset calibration */
	ph->sslpnphy_full_cal_channel[CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0] = 0;
	pi->phy_forcecal = TRUE;

	wlc_phyreg_exit((wlc_phy_t *)pi);
	wlapi_enable_mac(pi->sh->physhim);
}

void
wlc_phy_carrier_suppress_sslpnphy(phy_info_t *pi)
{
	if (ISSSLPNPHY(pi)) {
		wlc_sslpnphy_reset_radio_loft(pi);
		if (wlc_phy_tpc_isenabled_sslpnphy(pi))
			wlc_sslpnphy_clear_tx_power_offsets(pi);
		else
			wlc_sslpnphy_set_tx_locc(pi, 0);
	} else
		ASSERT(0);

}

#else

void
wlc_sslpnphy_spbrun_iovar(phy_info_t *pi, bool bool_val)
{
	uint16  i;
	uint16 sc_mode = 1;
	uint16 sc_block = 2;
	uint16 strt_trigger = 0;
	uint16 strtmac_trigger = 0;
	uint16 endmac_trigger = 0;
	uint16 crs_state1 = 0;
	uint16 crs_state2 = 0;
	uint init = 1;
	uint16 strt_is_trig, strtmac_is_trig, endmac_is_trig;
	uint16 samplerate, trigConfig;
	uint16 conv_40vlds_to_80;
	uint16 mac_phy_xfer_rate, strt_timer, strtmac_timer, endmac_timer;
	uint16 sc_control_word, strt_trigger_loc, strtmac_trigger_loc;
	uint16 endmac_trigger_loc;
	uint32 tbl_offset = 0xf7bad;

	/* Force WLAN antenna */
	wlc_sslpnphy_btcx_override_enable(pi);

	pi->samp_collect_agc_gain = wlc_sslpnphy_samp_collect_agc(pi, bool_val);

	wlc_sslpnphy_detection_disable(pi, TRUE); /* Doing Again for safety */

	PHY_REG_LIST_START
		/*  Bringing sampleplaybuffer out of standby */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, sslpnCtrl3, 0)
		/* Enabling clocks to sampleplaybuffer and debugblocks */
		PHY_REG_OR_ENTRY(SSLPNPHY, sslpnCalibClkEnCtrl, 0x2008)
		/* Resetting statistics registers */
		PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_reset_sts, 1)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_reset_sts, 0)
	PHY_REG_LIST_EXECUTE(pi);

	for (i = 0; i < 1024; i ++) {
		if (init == 1) {
			wlc_sslpnphy_common_write_table(pi,
				SSLPNPHY_TBL_ID_SAMPLEPLAY, &tbl_offset, 1, 32, i);
		}
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY1,
			&tbl_offset, 1, 32, i);

	}
	PHY_REG_LIST_START
		PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_reload_state, 0x15a)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_spb_reload_state_add1, 0)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_spb_reload_state_add2, 0)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_samp_coll_reload_state_mac_xfer_cnt_15_0, 0)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_samp_coll_reload_state_mac_xfer_cnt_31_16, 0)
		PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_reset_sts, 0)
	PHY_REG_LIST_EXECUTE(pi);

	strt_trigger_loc = strt_trigger;
	strtmac_trigger_loc = strtmac_trigger;
	endmac_trigger_loc = endmac_trigger;
	strt_timer = 0;
	strtmac_timer = 0;
	endmac_timer = 0;
	strt_is_trig = 0;
	strtmac_is_trig = 0;
	endmac_is_trig = 0;

	if (sc_mode == 1) {
		/* Allowing regular data to be sent to MAC */
		PHY_REG_WRITE(pi, SSLPNPHY, dbg_samp_coll_mux_sel, 0);

		if (strt_trigger == 0)
			strt_timer = 0;
		else
			strt_is_trig = 1;

		if (strtmac_trigger == 0) {
			if ((sc_block == 2) && IS40MHZ(pi))
				strtmac_timer = 2040;
			else
				strtmac_timer = 2040 *2 - 1;
		} else {
			strtmac_is_trig = 1;
		}

		PHY_REG_LIST_START
			/* Wait for 14 80-Mhz clocks before reloading */
			/* state after start trigger */
			PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_reload_state, 0x15e)
			/* Reloading of state during mac transfer disabled */
			PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_samp_coll_ctrl2, 0x07)
			/*  reload at 0 and 1/2-way points, in circular mode */
			PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_spb_reload_state_add1, 0xc)
			PHY_REG_WRITE_ENTRY(SSLPNPHY, dbg_spb_reload_state_add2, 0x40c)
		PHY_REG_LIST_EXECUTE(pi);
	}

	PHY_REG_WRITE(pi, SSLPNPHY, force_adc_lsb_zero, 0);
	/* Progrmming crs state trigger parameters */
	PHY_REG_OR(pi, SSLPNPHY, crsMiscCtrl0, crs_state2 << 10);
	PHY_REG_OR(pi, SSLPNPHY, gpioTriggerConfig, crs_state1 << 6);

	/* Programming the triggers */
	trigConfig = (strt_trigger_loc | (strtmac_trigger_loc << 5) |
		(endmac_trigger_loc << 10));
	PHY_REG_WRITE(pi, SSLPNPHY, triggerConfiguration, trigConfig);

	/* Programming the trigger delays */
	/* All state machine triggers are aligned to 2pt5phasecount 28 */
	/* by this the phase of the 40Mhz capture */
	/* will be always same to that used in chip */
	/* Needs adjustment for 1x,2x,different block captures  */
	PHY_REG_WRITE(pi, SSLPNPHY, gpioDlyConfig, 0x24f);

	/* Programming the timers */
	/* After timers are programmed, reset is required to put them to effect */
	PHY_REG_WRITE(pi, SSLPNPHY, dbg_samp_coll_start_trig_timer_15_0,
		strt_timer);
	PHY_REG_WRITE(pi, SSLPNPHY, dbg_samp_coll_start_mac_xfer_trig_timer_15_0,
		strtmac_timer);
	PHY_REG_WRITE(pi, SSLPNPHY, dbg_samp_coll_end_mac_xfer_trig_timer_15_0,
		endmac_timer);
	/* Programming the block for which sample-collection is enabled */
	PHY_REG_WRITE(pi, SSLPNPHY, dbg_samp_coll_module_sel, sc_block);
	if ((sc_block == 0) || (sc_block == 1) || (sc_block == 2) ||
		(sc_block == 6) ||(sc_block == 11) || (sc_block == 12) ||
		(sc_block == 13)) {
		samplerate = 1;
		conv_40vlds_to_80 = 0;
	}

	mac_phy_xfer_rate = 1;

	/* Programming the sample-collection-control */
	sc_control_word = (((sc_mode - 1) << 1) | (strt_is_trig << 4) |
		(strtmac_is_trig << 5) | (endmac_is_trig << 6) |
		(samplerate << 9) | (conv_40vlds_to_80 << 15) |
		(mac_phy_xfer_rate << 10));
	PHY_REG_WRITE(pi, SSLPNPHY, dbg_samp_coll_ctrl, sc_control_word);
	sc_control_word = sc_control_word | 1;
	PHY_REG_WRITE(pi, SSLPNPHY, dbg_samp_coll_ctrl, sc_control_word);

	sslpnphy_specific->sslpnphy_start_location = 0;
	sslpnphy_specific->sslpnphy_end_location = 255;

	{
		int32 k, i_acc = 0, q_acc = 0, i_dc, q_dc, i_dc_corr, q_dc_corr;
		uint32 iq_pwr_acc = 0, iq_pwr_avg;
		int32 i_d, i, q;
		uint32 d2;
		uint32 msb1, msb2, val1, val2, diff1, diff2, log_val;
		GET_GATE(IOV_SSLPNPHY_REPORT_SPBDUMP_PWR);
		GET_GATE(IOV_SSLPNPHY_REPORT_SPBDUMP_PWR_RAW);

		/* compute the DC */
		for (k = 0; k < 1024; k++) {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY, &d2, 1, 32, k);

			/* unpack 20bits into 10bit signed complex numbers */
			i_d = (d2 >> 10) & 0x3ff;
			i = i_d;
			q = d2 & 0x3ff;
			if (i > 511)
				i = i - 1024;
			if (q > 511)
				q = q - 1024;
			i_acc += i;
			q_acc += q;
		}
		/* final average over 2^10 samples */
		i_dc = i_acc >> 10;
		q_dc = q_acc >> 10;

		for (k = 0; k < 1024; k++) {
			wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_SAMPLEPLAY, &d2, 1, 32, k);

			/* unpack 20bits into 10bit signed complex numbers */
			i_d = (d2 >> 10) & 0x3ff;
			i = i_d;
			q = d2 & 0x3ff;
			if (i > 511)
				i = i - 1024;
			if (q > 511)
				q = q - 1024;

			i_dc_corr = i - i_dc;
			q_dc_corr = q - q_dc;
			iq_pwr_acc += ((i_dc_corr * i_dc_corr) + (q_dc_corr * q_dc_corr));
		}
		iq_pwr_avg = iq_pwr_acc >> 11;

		sslpnphy_specific->sslpnphy_spbdump_raw_pwr = iq_pwr_acc;
		/* convert to log10 domain */
		msb1 = find_msbit(iq_pwr_avg);
		msb2 = msb1 + 1;
		val1 = 1 << msb1;
		val2 = 1 << msb2;
		diff1 = (iq_pwr_avg - val1);
		diff2 = (val2 - iq_pwr_avg);
		if (diff1 < diff2)
			log_val = msb1;
		else
			log_val = msb2;

		log_val = log_val * 3;

		/* convert to dBm */
		sslpnphy_specific->sslpnphy_spbdump_pwr_dBm = (int32) (log_val - 49 - pi->samp_collect_agc_gain);
	}
}

void
wlc_sslpnphy_spbdump_iovar(phy_info_t *pi, void *a)
{
	wl_sslpnphy_spbdump_data_t debug_data;

	uint16 idx, index;
	int32 i_d, i, q;
	uint32 d1, d2;
	uint16 tbl_length;

	tbl_length = sslpnphy_specific->sslpnphy_end_location - sslpnphy_specific->sslpnphy_start_location + 1;

	for (idx = 0, index = 0; ((idx < tbl_length) &&
		(idx + sslpnphy_specific->sslpnphy_start_location) < 1024);
		idx ++, index++) {

		wlc_sslpnphy_common_read_table(pi,
			SSLPNPHY_TBL_ID_SAMPLEPLAY,
			&d1, 1, 32, idx +
			sslpnphy_specific->sslpnphy_start_location);
		i_d = (d1 >> 10) & 0x3ff;
		i = i_d;
		q = d1 & 0x3ff;
		if (i > 511)
			i = i - 1024;
		if (q > 511)
			q = q - 1024;
		debug_data.spbreal[index] = i;
		debug_data.spbimg[index] = q;
	}
	for (; idx < tbl_length; idx ++, index++) {
		wlc_sslpnphy_common_read_table(pi,
			SSLPNPHY_TBL_ID_SAMPLEPLAY1,
			&d2, 1, 32, idx + (sslpnphy_specific->sslpnphy_start_location
			- 1024));
		i_d = (d2 >> 10) & 0x3ff;
		i = i_d;
		q = d2 & 0x3ff;
		if (i > 511)
			i = i - 1024;
		if (q > 511)
			q = q - 1024;
		debug_data.spbreal[index] = i;
		debug_data.spbimg[index] = q;
	}

	if (sslpnphy_specific->sslpnphy_start_location == 0) {
		debug_data.spbreal[0] = pi->samp_collect_agc_gain;
		debug_data.spbimg[0] = pi->samp_collect_agc_gain;
	}

	debug_data.tbl_length = tbl_length;
	bcopy(&debug_data, a, sizeof(wl_sslpnphy_spbdump_data_t));

	sslpnphy_specific->sslpnphy_start_location = sslpnphy_specific->sslpnphy_start_location + 256;
	sslpnphy_specific->sslpnphy_end_location = sslpnphy_specific->sslpnphy_end_location + 256;
	if (sslpnphy_specific->sslpnphy_start_location == 1792) {
		sslpnphy_specific->sslpnphy_end_location = 2039;
	} else if (sslpnphy_specific->sslpnphy_start_location > 1792) {
		sslpnphy_specific->sslpnphy_start_location = 0;
		sslpnphy_specific->sslpnphy_end_location = 255;
		wlc_sslpnphy_detection_disable(pi, FALSE);
		PHY_REG_WRITE(pi, SSLPNPHY, rfoverride2, 0x0000);
	}

}
#endif /* ROMTERMPHY */
#endif 
#if !defined(ROMTERMPHY)
int BCMFASTPATH
wlc_sslpnphy_rssi_compute(phy_info_t *pi, int rssi, d11rxhdr_t *rxh)
{
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	uint8 gidx = (ltoh16(rxh->PhyRxStatus_2) & 0xFC00) >> 10;

	if (rssi > 127)
		rssi -= 256;

	/* RSSI adjustment */
	rssi = rssi + sslpnphy_gain_index_offset_for_pkt_rssi[gidx];
	if ((rssi > -46) && (gidx > 18))
		rssi = rssi + 7;

	/* temperature compensation */
	rssi = rssi + ph->sslpnphy_pkteng_rssi_slope;

	/* 2dB compensation of path loss for 4329 on Ref Boards */
	rssi = rssi + 2;

	return rssi;
}

#ifdef PPR_API


#define TA_MODE_DSSS	0
#define TA_MODE_OFDM	1
#define TA_MODE_MCS		2

static void
wlc_target_adjust_group(phy_info_t *pi, int8 *limits, int count, int mode)
{
	phy_info_sslpnphy_t *pi_sslpn = pi->u.pi_sslpnphy;
	int i;

	for (i = 0; i < count; i++) {
#ifdef WLPLT
		if (limits[i] < 40)
			limits[i] -= 4;
		else
#endif /* WLPLT */
		{
			if (mode == TA_MODE_MCS)
				limits[i] -= pi_sslpn->sslpnphy_11n_backoff;
			else if (mode == TA_MODE_DSSS)
				limits[i] -= pi_sslpn->sslpnphy_cck;
			else if (mode == TA_MODE_OFDM && i >= 4)
				limits[i] -= pi_sslpn->sslpnphy_54_48_36_24mbps_backoff;
			else
				limits[i] -= pi_sslpn->sslpnphy_lowerofdm;
		}
	}
}

void
wlc_sslpnphy_txpwr_target_adj(phy_info_t *pi, ppr_t *tx_pwr_target)
{
	uint8 cur_channel = CHSPEC_CHANNEL(pi->radio_chanspec); /* see wlioctl.h */
	if ((SSLPNREV_LT(pi->pubpi.phy_rev, 2)) || (SSLPNREV_IS(pi->pubpi.phy_rev, 4)))
	{
		ppr_dsss_rateset_t dsss_20, dsss_20in40;
		ppr_ofdm_rateset_t ofdm_20, ofdm_20in40, ofdm_40;
		ppr_ht_mcs_rateset_t mcs_20, mcs_20in40, mcs_40;

		ppr_get_dsss(tx_pwr_target, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_20);
		ppr_get_dsss(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &dsss_20in40);

		ppr_get_ofdm(tx_pwr_target, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_20);
		ppr_get_ofdm(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_20in40);
		ppr_get_ofdm(tx_pwr_target, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_40);

		ppr_get_ht_mcs(tx_pwr_target, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_20);
		ppr_get_ht_mcs(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_NSS_1,
		               WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_20in40);
		ppr_get_ht_mcs(tx_pwr_target, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_40);

		if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
	#if defined(WLPLT)
			if (cur_channel == 7)	/* addressing corner lot power issues */
				ppr_plus_cmn_val(tx_pwr_target, 2);
	#endif /* WLPLT */

			wlc_target_adjust_group(pi, dsss_20.pwr, sizeof(dsss_20.pwr), TA_MODE_DSSS);
			ppr_set_dsss(tx_pwr_target, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_20);
			wlc_target_adjust_group(pi, dsss_20in40.pwr, sizeof(dsss_20in40.pwr), TA_MODE_DSSS);
			ppr_set_dsss(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &dsss_20in40);

			wlc_target_adjust_group(pi, ofdm_20.pwr, sizeof(ofdm_20.pwr), TA_MODE_DSSS);
			ppr_set_ofdm(tx_pwr_target, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_20);
			wlc_target_adjust_group(pi, ofdm_20in40.pwr, sizeof(ofdm_20in40.pwr), TA_MODE_DSSS);
			ppr_set_ofdm(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_20in40);
			wlc_target_adjust_group(pi, ofdm_40.pwr, sizeof(ofdm_40.pwr), TA_MODE_DSSS);
			ppr_set_ofdm(tx_pwr_target, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_40);

			wlc_target_adjust_group(pi, mcs_20.pwr, sizeof(mcs_20.pwr), TA_MODE_DSSS);
			ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_20, WL_TX_NSS_1,
			               WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_20);
			wlc_target_adjust_group(pi, mcs_20in40.pwr, sizeof(mcs_20in40.pwr), TA_MODE_DSSS);
			ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_20IN40,
			               WL_TX_NSS_1, WL_TX_MODE_NONE,
			               WL_TX_CHAINS_1, &mcs_20in40);
			wlc_target_adjust_group(pi, mcs_40.pwr, sizeof(mcs_40.pwr), TA_MODE_DSSS);
			ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_40, WL_TX_NSS_1,
			               WL_TX_MODE_NONE, WL_TX_CHAINS_1, &mcs_40);
		} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {
			int i;

			if (cur_channel == 1) {
				for (i = 0; i < WL_RATESET_SZ_DSSS; i++) {
					dsss_20.pwr[i] = MIN(dsss_20.pwr[i], 68);
					dsss_20in40.pwr[i] = MIN(dsss_20.pwr[i], 68);
				}
				ppr_set_dsss(tx_pwr_target, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_20);
				ppr_set_dsss(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &dsss_20in40);

				ASSERT(WL_RATESET_SZ_OFDM == WL_RATESET_SZ_HT_MCS);
				for (i = 0; i < WL_RATESET_SZ_OFDM; i++) {
					ofdm_20.pwr[i] = MIN(ofdm_20.pwr[i], 70);
					ofdm_20in40.pwr[i] = MIN(ofdm_20in40.pwr[i], 70);
					ofdm_40.pwr[i] = MIN(ofdm_40.pwr[i], 70);
					mcs_20.pwr[i] = MIN(mcs_20.pwr[i], 70);
					mcs_20in40.pwr[i] = MIN(mcs_20in40.pwr[i], 70);
					mcs_40.pwr[i] = MIN(mcs_40.pwr[i], 70);
				}
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_20);
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
					&ofdm_20in40);
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_40);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_20,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_20);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_20IN40,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_20in40);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_40,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_40);
			} else if (cur_channel == 11) {
				for (i = 0; i < WL_RATESET_SZ_DSSS; i++) {
					dsss_20.pwr[i] = MIN(dsss_20.pwr[i], 70);
					dsss_20in40.pwr[i] = MIN(dsss_20.pwr[i], 70);
				}
				ppr_set_dsss(tx_pwr_target, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_20);
				ppr_set_dsss(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &dsss_20in40);

				ASSERT(WL_RATESET_SZ_OFDM == WL_RATESET_SZ_HT_MCS);
				for (i = 0; i < WL_RATESET_SZ_OFDM; i++) {
					ofdm_20.pwr[i] = MIN(ofdm_20.pwr[i], 64);
					ofdm_20in40.pwr[i] = MIN(ofdm_20in40.pwr[i], 64);
					ofdm_40.pwr[i] = MIN(ofdm_40.pwr[i], 64);
					mcs_20.pwr[i] = MIN(mcs_20.pwr[i], 64);
					mcs_20in40.pwr[i] = MIN(mcs_20in40.pwr[i], 64);
					mcs_40.pwr[i] = MIN(mcs_40.pwr[i], 64);
				}
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_20);
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
					&ofdm_20in40);
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_40);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_20,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_20);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_20IN40,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_20in40);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_40,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_40);
			} else {
				for (i = 0; i < WL_RATESET_SZ_DSSS; i++) {
					dsss_20.pwr[i] = MIN(dsss_20.pwr[i], 72);
					dsss_20in40.pwr[i] = MIN(dsss_20.pwr[i], 72);
				}
				ppr_set_dsss(tx_pwr_target, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_20);
				ppr_set_dsss(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &dsss_20in40);

				ASSERT(WL_RATESET_SZ_OFDM == WL_RATESET_SZ_HT_MCS);
				for (i = 0; i < WL_RATESET_SZ_OFDM; i++) {
					ofdm_20.pwr[i] = MIN(ofdm_20.pwr[i], 72);
					ofdm_20in40.pwr[i] = MIN(ofdm_20in40.pwr[i], 72);
					ofdm_40.pwr[i] = MIN(ofdm_40.pwr[i], 72);
					mcs_20.pwr[i] = MIN(mcs_20.pwr[i], 72);
					mcs_20in40.pwr[i] = MIN(mcs_20in40.pwr[i], 72);
					mcs_40.pwr[i] = MIN(mcs_40.pwr[i], 72);
				}
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_20);
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
					&ofdm_20in40);
				ppr_set_ofdm(tx_pwr_target, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_40);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_20,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_20);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_20IN40,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_20in40);
				ppr_set_ht_mcs(tx_pwr_target, WL_TX_BW_40,
				               WL_TX_NSS_1, WL_TX_MODE_NONE,
				               WL_TX_CHAINS_1, &mcs_40);
			}
		}
	}
}

#else /* PPR_API */

void
wlc_sslpnphy_txpwr_target_adj(phy_info_t *pi, uint8 *tx_pwr_target, uint8 rate)
{

	uint8 cur_channel = CHSPEC_CHANNEL(pi->radio_chanspec); /* see wlioctl.h */
	phy_info_sslpnphy_t *pi_sslpn = pi->u.pi_sslpnphy;

	if (SSLPNREV_LT(pi->pubpi.phy_rev, 2)) {
#if defined(WLPLT)
		if (cur_channel == 7)	/* addressing corner lot power issues */
			tx_pwr_target[rate] = tx_pwr_target[rate] + 2;
		if (tx_pwr_target[rate] < 40) {
			tx_pwr_target[rate] = tx_pwr_target[rate] - 4;
		} else
#endif /* WLPLT */
		{
			if (rate > 11)
				tx_pwr_target[rate] = tx_pwr_target[rate] -
					pi_sslpn->sslpnphy_11n_backoff;
			else if ((rate >= 8) && (rate <= 11))
				tx_pwr_target[rate] = tx_pwr_target[rate] -
					pi_sslpn->sslpnphy_54_48_36_24mbps_backoff;
			else if (rate <= 3)
				tx_pwr_target[rate] = tx_pwr_target[rate] -
					pi_sslpn->sslpnphy_cck;
			else
				tx_pwr_target[rate] = tx_pwr_target[rate] -
					pi_sslpn->sslpnphy_lowerofdm;
		}
	} else if (SSLPNREV_IS(pi->pubpi.phy_rev, 4)) {

		if (cur_channel == 1) {
			if (rate > 3)
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 68);
			else
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 70);
		} else if (cur_channel == 11) {
			if (rate <= 3)
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 70);
			else
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 64);
		} else {
			if (rate <= 3)
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 72);
			else
				tx_pwr_target[rate] = MIN(tx_pwr_target[rate], 72);
		}
	}
}

#endif /* PPR_API */

void
wlc_sslpnphy_iovar_papd_debug(phy_info_t *pi, void *a)
{

	wl_sslpnphy_papd_debug_data_t papd_debug_data;
	phy_info_sslpnphy_t *ph = pi->u.pi_sslpnphy;
	papd_debug_data.psat_pwr = ph->sslpnphy_psat_pwr;
	papd_debug_data.psat_indx = ph->sslpnphy_psat_indx;
	papd_debug_data.min_phase = ph->sslpnphy_min_phase;
	papd_debug_data.final_idx = ph->sslpnphy_final_idx;
	papd_debug_data.start_idx = ph->sslpnphy_start_idx;
	papd_debug_data.voltage = 0;
	papd_debug_data.temperature = 0;

	bcopy(&papd_debug_data, a, sizeof(wl_sslpnphy_papd_debug_data_t));
}
#endif /* PHYHAL */

static int8
wlc_phy_update_noisedBm(wlc_phy_t *pih, int nval)
{
	phy_info_t *pi = (phy_info_t*)pih;
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */

#ifdef ROMTERMPHY
	/* wlc.c calls the noise-measure() at wlc_init */
	if (pi->sslpnphy_disable_noise_percal)
		return nval;
#endif /* ROMTERMPHY */

	/* evict old value */
	sslpnphy_specific->noisedBm_ma -=
		sslpnphy_specific->noisedBm_window[sslpnphy_specific->noisedBm_index];
	/* admit new value */
	sslpnphy_specific->noisedBm_ma += nval;
	sslpnphy_specific->noisedBm_window[sslpnphy_specific->noisedBm_index] = nval;
	sslpnphy_specific->noisedBm_index = MODINC_POW2(sslpnphy_specific->noisedBm_index,
		sslpnphy_specific->noisedBm_ma_win_sz);
	if (sslpnphy_specific->noisedBm_ma_count < sslpnphy_specific->noisedBm_ma_win_sz)
		sslpnphy_specific->noisedBm_ma_count++;
	sslpnphy_specific->noisedBm_ma_avg =
		(sslpnphy_specific->noisedBm_ma / sslpnphy_specific->noisedBm_ma_count);
	return (int8)sslpnphy_specific->noisedBm_ma_avg;
}

void
wlc_phy_watchdog_sslpnphy(phy_info_t *pi)
{
	uint8 band_idx = (CHSPEC_IS5G(pi->radio_chanspec) ? 1 : 0);
#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#else
	wlc_info_t * wlc_pi = pi->wlc;
#endif /* PHYHAL */
	uint16 papd_txidx_delta_threshold, papd_temp_delta_threshold;
	int8 current_temperature;
	int temp_drift, txidx_drift;

	if (BOARDTYPE(GENERIC_PHY_INFO(pi)->sih->boardtype) == BCM94319SDNA_SSID) {
		papd_txidx_delta_threshold = pi->papd_txidx_delta_threshold;
		papd_temp_delta_threshold = pi->papd_temp_delta_threshold;
		current_temperature = (int8)wlc_sslpnphy_tempsense(pi);
		sslpnphy_specific->sslpnphy_cur_idx = (uint8)((phy_reg_read(pi, 0x4ab) & 0x7f00) >> 8);
		if (!(SCAN_IN_PROGRESS(wlc_pi) || WLC_RM_IN_PROGRESS(wlc_pi) ||
			PLT_IN_PROGRESS(wlc_pi) ||
			ASSOC_IN_PROGRESS(wlc_pi) || pi->carrier_suppr_disable ||
			sslpnphy_specific->pkteng_in_progress || pi->disable_percal)) {
			temp_drift = current_temperature - sslpnphy_specific->sslpnphy_last_cal_temperature;
			txidx_drift = sslpnphy_specific->sslpnphy_cur_idx - sslpnphy_specific->sslpnphy_tx_idx_prev_cal;
			if ((txidx_drift > papd_txidx_delta_threshold) || (txidx_drift < -papd_txidx_delta_threshold) ||
				(temp_drift > papd_temp_delta_threshold) || (temp_drift < -papd_temp_delta_threshold))
			{
				wlc_sslpnphy_papd_recal(pi);
			}
		}
	}

	if (pi->phy_forcecal ||
		(sslpnphy_specific->sslpnphy_full_cal_chanspec[band_idx] !=
		pi->radio_chanspec) ||
		((GENERIC_PHY_INFO(pi)->now - pi->phy_lastcal) >= pi->sh->glacial_timer)) {
		if (!(SCAN_IN_PROGRESS(wlc_pi) || WLC_RM_IN_PROGRESS(wlc_pi) ||
			PLT_IN_PROGRESS(wlc_pi) ||
			ASSOC_IN_PROGRESS(wlc_pi) || pi->carrier_suppr_disable ||
			sslpnphy_specific->pkteng_in_progress || pi->disable_percal)) {
			WL_INFORM(("p cal!\n"));
			wlc_sslpnphy_periodic_cal_top(pi);
			wlc_sslpnphy_auxadc_measure((wlc_phy_t *) pi, 0);
		}
	} else if (((GENERIC_PHY_INFO(pi)->now - sslpnphy_specific->sslpnphy_last_noise_cal) >=
		(pi->sh->fast_timer * 2)) && (pi->sh->fast_timer != 0)) {
		if (!(SCAN_IN_PROGRESS(wlc_pi) || WLC_RM_IN_PROGRESS(wlc_pi) ||
			PLT_IN_PROGRESS(wlc_pi) ||
			ASSOC_IN_PROGRESS(wlc_pi) || pi->carrier_suppr_disable ||
			sslpnphy_specific->pkteng_in_progress || pi->disable_percal)) {

			if (!sslpnphy_specific->sslpnphy_disable_noise_percal) {
				WL_INFORM(("p n cal!\n"));
				wlc_sslpnphy_noise_measure((wlc_phy_t *) pi);
			}
			/* noise updates should happen even when disable_noise_percal flag is off */
			wlc_phy_update_noisedBm((wlc_phy_t *) pi, sslpnphy_specific->noise_level_dBm);
		}
	} else {
		if (!(SCAN_IN_PROGRESS(wlc_pi) || WLC_RM_IN_PROGRESS(wlc_pi) ||
			PLT_IN_PROGRESS(wlc_pi) ||
			ASSOC_IN_PROGRESS(wlc_pi) || pi->carrier_suppr_disable ||
			sslpnphy_specific->pkteng_in_progress || pi->disable_percal)) {
			wlc_sslpnphy_auxadc_measure((wlc_phy_t *) pi, 1);
		}
	}

	WL_INFORM(("t=%d, %d", sslpnphy_specific->sslpnphy_auxadc_val, sslpnphy_specific->sslpnphy_tssi_val));
	WL_INFORM((", %x, %x", phy_reg_read(pi, SSLPNPHY_AfeCtrlOvr), phy_reg_read(pi, SSLPNPHY_AfeCtrlOvrVal)));
	WL_INFORM((", %x, %x\n", phy_reg_read(pi, SSLPNPHY_RFOverride0), phy_reg_read(pi, SSLPNPHY_RFOverrideVal0)));

}
int8
wlc_sslpnphy_noise_avg(phy_info_t *pi)
{

#if !defined(ROMTERMPHY)
	phy_info_sslpnphy_t *sslpnphy_specific = pi->u.pi_sslpnphy;
#endif /* PHYHAL */
	int noise = 0;
	if (sslpnphy_specific->noisedBm_ma_count)
		noise = sslpnphy_specific->noisedBm_ma_avg;
	else
		noise = PHY_NOISE_FIXED_VAL_SSLPNPHY;

	return (int8)noise;

}
#ifdef ROMTERMPHY

int8
wlc_sslpnphy_get_crs(phy_info_t *pi)
{
	uint8 temp;
	uint8 delta_listengain = -1;
	/* if noise-cal is dynamic, return -1 */
	if (pi->sslpnphy_disable_noise_percal) {
		if (!IS40MHZ(pi)) {
			temp = PHY_REG_READ(pi, SSLPNPHY, HiGainDB, HiGainDB);
			delta_listengain = pi->sslpn_nom_listen_gain - temp;
		} else {
			temp = PHY_REG_READ(pi, SSLPNPHY_Rev2, HiGainDB_40, HiGainDB);
			delta_listengain = pi->sslpn_nom_listen_gain - temp;
		}
	}
	return delta_listengain;
}


void
wlc_sslpnphy_set_crs(phy_info_t *pi, int8 int_val)
{
	uint8 temp;
	if (int_val < -1)
		return;
	if (int_val >= 0) {
		/* disable the dynamic algo for listen gain change */
		pi->sslpnphy_disable_noise_percal = TRUE;
		temp = pi->sslpn_nom_listen_gain - int_val;
		if (!IS40MHZ(pi)) {
			PHY_REG_MOD(pi, SSLPNPHY, HiGainDB, HiGainDB, temp);
			PHY_REG_MOD(pi, SSLPNPHY, InputPowerDB, inputpwroffsetdb, 0);
		} else {
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, HiGainDB_40, HiGainDB, temp);
			PHY_REG_MOD(pi, SSLPNPHY_Rev2, InputPowerDB_40, inputpwroffsetdb, 0);
		}
	} else {
		/* enable the dynamic algo for listen gain change */
		pi->sslpnphy_disable_noise_percal = FALSE;
		wlc_sslpnphy_noise_measure((wlc_phy_t *) pi);
	}

}

int
wlc_sslpnphy_set_ntd_gds_lowtxpwr(phy_info_t *pi, int32 int_val)
{
	pi->sslpnphy_ntd_gds_lowtxpwr = int_val;

	/* Turn on HW Tx power control if phy_txpwrbckoff == 0 */
	if (int_val == 0) {
		wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_HW);
		return 1;
	}

	/* Transmit 0 dBm Tx power if phy_txpwrbckoff == 1 */
	if (int_val == 1) {
		if (((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) >= SSLPNPHY_LOW_START_FREQ) &&
		((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) <= SSLPNPHY_LOW_END_FREQ)) {
			wlc_sslpnphy_set_tx_pwr_by_index(pi, SSLPNPHY_LOW_BAND_ZERO_DBM_INDEX);
		} else if (((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) >= SSLPNPHY_MID_START_FREQ) &&
		((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) <= SSLPNPHY_MID_END_FREQ)) {
			wlc_sslpnphy_set_tx_pwr_by_index(pi, SSLPNPHY_MID_BAND_ZERO_DBM_INDEX);
		} else if (((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) >= SSLPNPHY_HIGH_START_FREQ) &&
		((PHY_CHANNEL2FREQ(CHSPEC_CHANNEL(pi->radio_chanspec))) <= SSLPNPHY_HIGH_END_FREQ)) {
			wlc_sslpnphy_set_tx_pwr_by_index(pi, SSLPNPHY_HIGH_BAND_ZERO_DBM_INDEX);
		}
		return 1;
	}
	return 0;
}


void
wlc_sslpnphy_write_shm_tssiCalEn(phy_info_t *pi, bool tssiCalEnVal)
{
	uint16 txPwrCtrlState;
	bool suspend;
	uint16 sslpnphy_shm_ptr = WL_READ_SHM(pi, M_SSLPNPHYREGS_PTR);
	txPwrCtrlState = wlc_sslpnphy_get_tx_pwr_ctrl(pi);

	/* This function is used to "gracefully" turn ON/OFF the TSSICAL_EN shm register */
	/* To avoid PSM_WD, care is taken to turn OFF the MAC before switching OFF the TSSICAL_EN shm */
	if (tssiCalEnVal) {
		if (txPwrCtrlState) {
			WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr + M_SSLPNPHY_TSSICAL_EN)), 0x1);
			sslpnphy_specific->sslpnphy_last_idletssi_cal = GENERIC_PHY_INFO(pi)->now;
		} else {
			/* If HW TxPwr Ctrl is OFF, there is NO need to do idelTssi estimation */
			/* Ignore the request to turn ON uCode based TSSI estimation */
		}
	} else {
		/* Suspend the mac if it is not already suspended */
		suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
		if (!suspend)
			WL_SUSPEND_MAC_AND_WAIT(pi);
		/* Trigger OFF TSSI_CAL_EN */
		WL_WRITE_SHM(pi, (2 * (sslpnphy_shm_ptr + M_SSLPNPHY_TSSICAL_EN)), 0x0);
		/* Enable MAC */
		if (!suspend)
			WL_ENABLE_MAC(pi);
	}
}

#if defined(SSLPNPHYCAL_CACHING)
sslpnphy_calcache_t *
wlc_phy_get_sslpnphy_chanctx(uint16 chanspec)
{
	int count;
	sslpnphy_calcache_t *ctx = NULL;

	for (count = 0; count < SSLPNPHY_MAX_CAL_CACHE; count++) {
		if (sslpnphy_calcache[count].chanspec == chanspec) {
			return &sslpnphy_calcache[count];
		}
	}
	return ctx;
}

void
wlc_phy_reset_sslpnphy_chanctx(uint16 chanspec)
{
	int count;
	sslpnphy_calcache_t *ctx = NULL;

	for (count = 0; count < SSLPNPHY_MAX_CAL_CACHE; count++) {
		if (sslpnphy_calcache[count].chanspec == chanspec) {
			ctx = &sslpnphy_calcache[count];
			ctx->valid = FALSE;
			break;
		}
	}
}


int
check_valid_channel_to_cache(uint16 chanspec)
{
	int count, valid_ch = 0;

	/* Validate channel */
	for (count = 0; count < SSLPNPHY_MAX_CAL_CACHE; count++) {
		if (chanspec == valid_channel_list[count])
			valid_ch = 1;
	}
	return valid_ch;
}

sslpnphy_calcache_t *
wlc_phy_create_sslpnphy_chanctx(uint16 chanspec)
{
	int count;
	sslpnphy_calcache_t *ctx_cache = NULL, *temp_ctx_cache;

	/* Check for existing context "double check" */
	/* we should never enter this function if context is already set */
	ctx_cache = wlc_phy_get_sslpnphy_chanctx(chanspec);
	if (ctx_cache)
		return (ctx_cache);

	ctx_cache = NULL;

	for (count = 0; count < SSLPNPHY_MAX_CAL_CACHE; count++) {
		temp_ctx_cache = &sslpnphy_calcache[count];
		if (temp_ctx_cache->chanspec == 0) {
			ctx_cache = &sslpnphy_calcache[count];
			bzero(ctx_cache, sizeof(sslpnphy_calcache_t));
			ctx_cache->chanspec = chanspec;
			break;
		}
	}
	return (ctx_cache);
}

void
wlc_phy_destroy_sslpnphy_chanctx(uint16 chanspec)
{
	sslpnphy_calcache_t *ctx_cache;
	int count;

	for (count = 0; count < SSLPNPHY_MAX_CAL_CACHE; count++) {
		ctx_cache = &sslpnphy_calcache[count];
		if (ctx_cache->chanspec == chanspec) {
			bzero(ctx_cache, sizeof(sslpnphy_calcache_t));
			break;
		}
	}
}

void
wlc_phy_cal_cache_sslpnphy(phy_info_t *pi)
{
	sslpnphy_calcache_t *ctx_cache;
	bool suspend;
	uint16 didq, a, b;
	uint8 ei0, eq0, fi0, fq0;
	ctx_cache = wlc_phy_get_sslpnphy_chanctx(CHSPEC_CHANNEL(pi->radio_chanspec));

	/* A context must have been created before reaching here */
	ASSERT(ctx_cache != NULL);
	if (ctx_cache == NULL)
		return;

	suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		WL_WRITE_SHM(pi, M_CTS_DURATION, 1000);
		WL_SUSPEND_MAC_AND_WAIT(pi);
	}

	wlc_phyreg_enter((wlc_phy_t *)pi);

	if (FALSE == ctx_cache->valid) {
		ctx_cache->valid = TRUE;

		/* Cache Rx IQ cal coefficients values */
		ctx_cache->rxcal_coeffs.a0 = (int16)PHY_REG_READ(pi, SSLPNPHY, RxCompcoeffa0, a0);
		ctx_cache->rxcal_coeffs.b0 = (int16)PHY_REG_READ(pi, SSLPNPHY, RxCompcoeffb0, b0);

		/* Cache Tx IQ cal coefficients values */
		wlc_sslpnphy_get_tx_iqcc(pi, &a, &b);
		ctx_cache->txcal_coeffs[0] = a; /* a */
		ctx_cache->txcal_coeffs[1] = b; /* b */

		/* Cache fine and coarse LOFT compensation values */
		didq = wlc_sslpnphy_get_tx_locc(pi);
		ctx_cache->txcal_coeffs[2] = didq; /* didq */

		wlc_sslpnphy_get_radio_loft(pi, &ei0, &eq0, &fi0, &fq0);

		ctx_cache->txcal_coeffs[3] = (uint16)(ei0 << 8 | eq0); /* eieq */
		ctx_cache->txcal_coeffs[4] = (uint16)(fi0 << 8 | fq0); /* fifq */

	}

	wlc_phyreg_exit((wlc_phy_t *)pi);
	/* unsuspend mac */
	if (!suspend)
		WL_ENABLE_MAC(pi);
}

int
wlc_phy_cal_cache_restore_sslpnphy(phy_info_t *pih)
{
	uint16 tx_pwr_ctrl, didq;
	bool suspend;
	phy_info_t *pi = (phy_info_t *) pih;
	sslpnphy_calcache_t *ctx_cache;
	uint8 ei0, eq0, fi0, fq0;
	uint16 a, b, idx;
	uint32 val;

	ctx_cache = wlc_phy_get_sslpnphy_chanctx(CHSPEC_CHANNEL(pi->radio_chanspec));

	if (!ctx_cache) {
		PHY_ERROR(("wl%d: %s: Chanspec 0x%x not found in calibration cache\n",
		           pi->sh->unit, __FUNCTION__, pi->radio_chanspec));
		return BCME_ERROR;
	}

	if (!ctx_cache->valid) {
		PHY_ERROR(("wl%d: %s: Chanspec 0x%x found, but not valid in phycal cache\n",
		           pi->sh->unit, __FUNCTION__, ctx_cache->chanspec));
		return BCME_ERROR;
	}

	PHY_INFORM(("wl%d: %s: Restoring all cal coeffs from calibration cache for chanspec 0x%x\n",
	           pi->sh->unit, __FUNCTION__, pi->radio_chanspec));

	suspend = !(R_REG(GENERIC_PHY_INFO(pi)->osh, &pi->regs->maccontrol) & MCTL_EN_MAC);
	if (!suspend) {
		/* Set non-zero duration for CTS-to-self */
		WL_WRITE_SHM(pi, M_CTS_DURATION, 1500);
		WL_SUSPEND_MAC_AND_WAIT(pi);
	}

	wlc_phyreg_enter((wlc_phy_t *)pi);

	tx_pwr_ctrl = wlc_sslpnphy_get_tx_pwr_ctrl(pi);
	wlc_sslpnphy_set_tx_pwr_ctrl(pi, SSLPNPHY_TX_PWR_CTRL_OFF);

	/* Restore Tx IQ CC coefficients */
	a = ctx_cache->txcal_coeffs[0];
	b = ctx_cache->txcal_coeffs[1];

	/* Restore fine LOFT compensation */
	didq = ctx_cache->txcal_coeffs[2];

	/* Restore iqlo portion of tx power control tables */
	for (idx = 0; idx < 128; idx++) {
		/* iq */
		wlc_sslpnphy_common_read_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);
		val = (val & 0xfff00000) |
			((uint32)(a & 0x3FF) << 10) | (b & 0x3ff);
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_IQ_OFFSET + idx);
		/* loft */
		val = didq;
		wlc_sslpnphy_common_write_table(pi, SSLPNPHY_TBL_ID_TXPWRCTL, &val,
			1, 32, SSLPNPHY_TX_PWR_CTRL_LO_OFFSET + idx);
	}

	/* Restore coarse LOFT compensation */
	ei0 = (uint8)(ctx_cache->txcal_coeffs[3] >> 8);
	eq0 = (uint8)(ctx_cache->txcal_coeffs[3]);
	fi0 = (uint8)(ctx_cache->txcal_coeffs[4] >> 8);
	fq0 = (uint8)(ctx_cache->txcal_coeffs[4]);
	wlc_sslpnphy_set_radio_loft(pi, ei0, eq0, fi0, fq0);

	/* Restore Rx IQ coefficients values */
	wlc_sslpnphy_set_rx_iq_comp(pi, ctx_cache->rxcal_coeffs.a0, ctx_cache->rxcal_coeffs.b0);

	wlc_sslpnphy_set_tx_pwr_ctrl(pi, tx_pwr_ctrl);

	wlc_phyreg_exit((wlc_phy_t *)pi);

	/* unsuspend mac */
	if (!suspend)
		WL_ENABLE_MAC(pi);

	return BCME_OK;
}
#endif /* SSLPNPHYCAL_CACHING */

#endif /* ROMTERMPHY */

#ifdef WLMEDIA_APCS
void
wlc_sslpnphy_set_skip_papd_recal_flag(phy_info_t *pi)
{
	pi->dcs_skip_papd_recal = TRUE;
}

void
wlc_sslpnphy_reset_skip_papd_recal_flag(phy_info_t *pi)
{
	pi->dcs_skip_papd_recal = FALSE;
}

int
wlc_sslpnphy_get_dcs_papd_delay(phy_info_t *pi)
{
	return (pi->dcs_papd_delay);
}
#endif /* WLMEDIA_APCS */
