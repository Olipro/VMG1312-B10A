/*
 *-----------------------------------------------------------------------------
 *  Copyright 1999 - 2009, Broadcom Corporation
 *  All Rights Reserved.
 *  
 *  This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 *  the contents of this file may not be disclosed to third parties, copied or
 *  duplicated in any form, in whole or in part, without the prior written
 *  permission of Broadcom Corporation.
 *
 *-----------------------------------------------------------------------------
 *
 * by regdb.pl version @(#)$Id: wlc_phyreg_abg.h 241182 2011-02-17 21:50:03Z $
 * on Wed Apr 19 13:46:35 2006
*/
/* FILE-CSTYLED */

/* Bits in BPHY_RF_OVERRIDE(0x15): */
#define	RFO_FLTR_RX_CTRL_OVR	0x0080
#define	RFO_FLTR_RX_CTRL_VAL	0x0040

/* Bits in BPHY_RF_TR_LOOKUP1(0x16): */

/* Bits in BPHY_RF_TR_LOOKUP2(0x17): */

/* Bits in BPHY_REFRESH_MAIN(0x1a): */
#define	REF_RXPU_TRIG		0x8000
#define	REF_IDLE_TRIG		0x4000
#define	REF_TO0_TRIG		0x2000
#define	REF_TO1_TRIG		0x1000

/* Bits in BPHY_LNA_GAIN_RANGE(0x26): */
#define	LNA_DIGI_GAIN_ENABLE	0x8000
#define	LNA_ON_CTRL		0x4000
#define	LNA_PTR_THRESH		0x0f00
#define	LNA_GAIN_RANGE		0x00ff

/* Bits in BPHY_SYNC_CTL(0x35): */
#define	SYN_ANGLE_START		0x0f00
#define	SYN_TOGGLE_CUTOFF	0x0080
#define	SYN_WARMUP_DUR		0x007f

/* Bits in BPHY_OPTIONAL_MODES	(0x5d): */
#define	OPT_MODE_G		0x4000

/* Aphy regs offset in the gphy */
#define	GPHY_TO_APHY_OFF	0x400

#define	APHY_REG_OFT_BASE	0x0

/* offsets for indirect access to aphy registers */
#define	APHY_PHYVERSION		0x00
#define	APHY_BBCONFIG		0x01
#define	APHY_PWRDWN		0x03
#define	APHY_PHYCRSTH		0x06
#define	APHY_RF_OVERRIDE	0x10
#define	APHY_RF_OVERRIDE_VAL	0x11
#define	APHY_GPIO_OUTEN		0x12
#define	APHY_TR_LUT1		0x13
#define	APHY_TR_LUT2		0x14
#define	APHY_DIGI_GAIN1		0x15
#define	APHY_DIGI_GAIN2		0x16
#define	APHY_DIGI_GAIN3		0x17
#define	APHY_DESIRED_PWR	0x18
#define	APHY_PAR_GAIN_SEL	0x19
#define	APHY_MIN_MAX_GAIN	0x1a
#define	APHY_GAIN_INFO		0x1b
#define	APHY_INIT_GAIN_INDX	0x1c
#define	APHY_CLIP_GAIN_INDX	0x1d
#define	APHY_TRN_INFO		0x1e
#define	APHY_CLIP_BO_THRESH	0x1f
#define	APHY_LPF_GAIN_BO_THRESH	0x20
#define	APHY_ADC_VSQR		0x21
#define	APHY_CCK_CLIP_BO_THRESH	0x22
#define	APHY_CLIP_PWR_THRESH	0x24
#define	APHY_JSSI_OFFSET	0x25
#define	APHY_DC_B0		0x26
#define	APHY_DC_B1		0x27
#define	APHY_DC_A1		0x28
#define	APHY_CTHR_STHR_SHDIN	0x29
#define	APHY_MIN_PWR_GSETTL	0x2a
#define	APHY_ANT_DWELL		0x2b
#define	APHY_RESET_LEN		0x2c
#define	APHY_CLIP_CTR_INIT	0x2d
#define	APHY_ED_TO		0x2e
#define	APHY_CRS_HOLD		0x2f
#define	APHY_PLCP_TMT_STR0_MIN	0x30
#define	APHY_STRN_COLL_MAX_SAMP	0x31
#define	APHY_STRN_MIN_REAL	0x33
#define	APHY_COARSE_UPD_CTL	0x34
#define APHY_IqestEnWaitTime	0x34
#define	APHY_SCALE_FACT_I	0x35
#define APHY_IqestNumSamps	0x35
#define	APHY_SCALE_FACT_Q	0x36
#define APHY_IqestIqAccHi	0x36
#define	APHY_DC_OFFSET_I	0x37
#define APHY_IqestIqAccLo	0x37
#define	APHY_DC_OFFSET_Q	0x38
#define APHY_IqestIpwrAccHi	0x38
#define	APHY_FIX_VAL_OUT_I	0x39
#define APHY_IqestIpwrAccLo	0x39
#define	APHY_FIX_VAL_OUT_Q	0x3a
#define APHY_IqestQpwrAccHi	0x3a
#define	APHY_MAX_SAMP_FINE	0x3b
#define APHY_IqestQpwrAccLo	0x3b
#define	APHY_LTRN_MIN_OFFSET	0x3d
#define	APHY_COMP_CTL		0x3e
#define	APHY_HSQ_MIN_BPSK	0x41
#define	APHY_HSQ_MIN_QPSK	0x42
#define	APHY_HSQ_MIN_16QAM	0x43
#define	APHY_HSQ_MIN_64QAM	0x44
#define	APHY_QUANT_ST_BPSK	0x45
#define	APHY_QUANT_ST_QPSK	0x46
#define	APHY_QUANT_ST_16QAM	0x47
#define	APHY_QUANT_ST_64QAM	0x48
#define	APHY_VITERBI_OFFSET	0x49
#define	APHY_MAX_STEPS		0x4a
#define	APHY_ALPHA1		0x50
#define	APHY_ALPHA2		0x51
#define	APHY_BETA1		0x52
#define	APHY_BETA2		0x53
#define	APHY_NUM_LOOP		0x54
#define	APHY_MU			0x55
#define	APHY_THETA_I		0x56
#define	APHY_THETA_Q		0x57
#define	APHY_SCRAM_CTL_INIT_ST	0x58
#define	APHY_PKT_GAIN		0x59
#define	APHY_COARSE_ES		0x5a
#define	APHY_FINE_ES		0x5b
#define	APHY_TRN_OFFSET		0x5c
#define	APHY_NUM_PKT_CNT	0x5f
#define	APHY_STOP_PKT_CNT	0x60
#define	APHY_CTL		0x61
#define	APHY_PASS_TH_SAMPS	0x62
#define	APHY_RX_COMP_COEFF	0x63
#define	APHY_TC_PLCP_DELAY	0x68
#define	APHY_TX_COMP_COEFF	0x69
#define	APHY_TX_COMP_OFFSET	0x6a
#define	APHY_DC_BIAS		0x6b
#define	APHY_ROTATE_FACT	0x6e
#define	APHY_TABLE_ADDR		0x72
#define	APHY_TABLE_DATA_I	0x73
#define	APHY_TABLE_DATA_Q	0x74
#define	APHY_RSSI_FILT_B0	0x75
#define	APHY_RSSI_FILT_B1	0x76
#define	APHY_RSSI_FILT_B2	0x77
#define	APHY_RSSI_FILT_A1	0x78
#define	APHY_RSSI_FILT_A2	0x79
#define	APHY_RSSI_ADC_CTL	0x7a
#define	APHY_TSSI_STAT		0x7b
#define	APHY_TSSI_TEMP_CTL	0x7c
#define	APHY_TEMP_STAT		0x7d
#define	APHY_CRS_DELAY		0x7e
#define	APHY_WRSSI_NRSSI	0x7f
#define	APHY_P1_P2_GAIN_SETTLE	0x80
#define	APHY_N1_N2_GAIN_SETTLE	0x81
#define	APHY_N1_P1_GAIN_SETTLE	0x82
#define	APHY_P1_CLIP_CTR	0x83
#define	APHY_P2_CLIP_CTR	0x84
#define	APHY_N1_CLIP_CTR	0x85
#define	APHY_N2_CLIP_CTR	0x86
#define	APHY_P1_COMP_TIME	0x88
#define	APHY_N1_COMP_TIME	0x89
#define	APHY_N1_N2_THRESH	0x8a
#define	APHY_ANT2_DWELL		0x8b
#define	APHY_ANT_WR_SETTLE	0x8c
#define	APHY_ANT_COMP_TIME	0x8d
#define	APHY_AUX_CLIP_THRESH	0x8e
#define	APHY_DS_AUX_CLIP_THRESH	0x8f
#define	APHY_CLIP2RST_N1_P1	0x90
#define	APHY_P1_P2_EDDR_THRESH	0x91
#define	APHY_N1_N2_EDDR_THRESH	0x92
#define	APHY_CLIP_PWDN_THRESH	0x93
#define	APHY_SRCH_COMP_SETTLE	0x94
#define	APHY_ED_DROP_ENAB	0x95
#define	APHY_N1_P1_P2_COMP	0x96
#define	APHY_CCK_NUS_THRESH	0x9b
#define	APHY_CLIP_N1_P1_IDX	0xa0
#define	APHY_CLIP_P1_P2_IDX	0xa1
#define	APHY_CLIP_N1_N2_IDX	0xa2
#define	APHY_CLIP_THRESH	0xa3
#define	APHY_CCK_DESIRED_POW	0xa4
#define	APHY_CCK_GAIN_INFO	0xa5
#define	APHY_CCK_SHBITS_REF	0xa6
#define	APHY_CCK_SHBITS_GNREF	0xa7
#define	APHY_DIV_SEARCH_IDX	0xa8
#define	APHY_CLIP2_THRESH	0xa9
#define	APHY_CLIP3_THRESH	0xaa
#define	APHY_DIV_SEARCH_P1_P2	0xab
#define	APHY_CLIP_P1_P2_THRESH	0xac
#define	APHY_DIV_SEARCH_GN_BACK	0xad
#define	APHY_DIV_SEARCH_GN_CHANGE 0xae
#define	APHY_WB_PWR_THRESH	0xb0
#define	APHY_WW_CLIP0_THRESH	0xb1
#define	APHY_WW_CLIP1_THRESH	0xb2
#define	APHY_WW_CLIP2_THRESH	0xb3
#define	APHY_WW_CLIP3_THRESH	0xb4
#define	APHY_WW_CLIP0_IDX	0xb5
#define	APHY_WW_CLIP1_IDX	0xb6
#define	APHY_WW_CLIP2_IDX	0xb7
#define	APHY_WW_CLIP3_IDX	0xb8
#define	APHY_WW_CLIPWRSSI_IDX	0xb9
#define	APHY_WW_CLIPVAR_THRESH	0xba
#define	APHY_NB_WRRSI_WAIT	0xbb
#define	APHY_CRSON_THRESH	0xc0
#define	APHY_CRSOFF_THRESH	0xc1
#define	APHY_CRSMF_THRESH0	0xc2
#define	APHY_CRSMF_THRESH1	0xc3
#define	APHY_RADAR_BLANK_CTL	0xc4
#define	APHY_RADAR_FIFO_CTL	0xc5
#define	APHY_RADAR_FIFO		0xc6
#define	APHY_RADAR_THRESH0	0xc7
#define	APHY_RADAR_THRESH1	0xc8
#define	APHY_EDON_P1		0xc9
#define	APHY_FACT_RHOSQ		0xcc

#define APHY_RSSISELL1_TBL	0xdc /* RSSISelLookup1Table corerev >= 6 */

/* APHY_ANT_DWELL bits (FirstAntSecondAntDwellTime) */
#define	APHY_ANT_DWELL_FIRST_ANT	0x100

/* Bits in APHY_IqestEnWaitTime */
#define APHY_IqEnWaitTime_waitTime_SHIFT 0
#define APHY_IqEnWaitTime_waitTime_MASK (0xff << APHY_IqEnWaitTime_waitTime_SHIFT)
#define APHY_IqMode_SHIFT 8
#define APHY_IqMode (1 << APHY_IqMode_SHIFT)
#define APHY_IqStart_SHIFT 9
#define APHY_IqStart (1 << APHY_IqStart_SHIFT)

/* Bits in APHY_CTHR_STHR_SHDIN(0x29): */
#define APHY_CTHR_CRS1_ENABLE	0x4000

/* Gphy registers in the aphy :-( */

#define	GPHY_REG_OFT_BASE	0x800
/* Gphy registers */
#define	GPHY_PHY_VER		0x0800
#define	GPHY_CTRL		0x0801
#define	GPHY_CLASSIFY_CTRL	0x0802
#define	GPHY_TABLE_ADDR		0x0803
#define	GPHY_TRLUT1		0x0803		/* phyrev > 1 */
#define	GPHY_TABLE_DATA		0x0804
#define	GPHY_TRLUT2		0x0804		/* phyrev > 1 */
#define	GPHY_RSSI_B0		0x0805
#define	GPHY_RSSI_B1		0x0806
#define	GPHY_RSSI_B2		0x0807
#define	GPHY_RSSI_A0		0x0808
#define	GPHY_RSSI_A1		0x0809
#define	GPHY_TSSI_B0		0x080a
#define	GPHY_TSSI_B1		0x080b
#define	GPHY_TSSI_B2		0x080c
#define	GPHY_TSSI_A0		0x080d
#define	GPHY_TSSI_A1		0x080e
#define	GPHY_DC_OFFSET1		0x080f
#define	GPHY_DC_OFFSET2		0x0810
#define	GPHY_RF_OVERRIDE	0x0811
#define	GPHY_RF_OVERRIDE_VAL	0x0812
#define	GPHY_DBG_STATE		0x0813
#define	GPHY_ANA_OVERRIDE	0x0814
#define	GPHY_ANA_OVERRIDE_VAL	0x0815
