/*
 * ACPHY module header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_ac.h 379781 2013-01-18 23:43:30Z $
 */

#ifndef _wlc_phy_ac_h_
#define _wlc_phy_ac_h_

#include <typedefs.h>
#include <wlc_phy_int.h>

#ifdef ENABLE_ACPHY
extern int acphychipid;
#ifdef BCMCHIPID
#define  ACREV0 (BCMCHIPID == BCM4360_CHIP_ID || BCMCHIPID == BCM4352_CHIP_ID || \
		 BCMCHIPID == BCM43460_CHIP_ID || BCMCHIPID == BCM43526_CHIP_ID)
#else
#define ACREV0 (acphychipid == BCM4360_CHIP_ID || acphychipid == BCM4352_CHIP_ID ||\
		acphychipid == BCM43460_CHIP_ID || acphychipid == BCM43526_CHIP_ID)
#endif
#else
#define ACREV0 0
#endif /* ENABLE_ACPHY */

#define ACPHY_GAIN_VS_TEMP_SLOPE_2G 7   /* units: db/100C */
#define ACPHY_GAIN_VS_TEMP_SLOPE_5G 4   /* units: db/100C */

/* ACPHY tables */
#define ACPHY_TBL_ID_MCS                          1
#define ACPHY_TBL_ID_TXEVMTBL                     2
#define ACPHY_TBL_ID_NVNOISESHAPINGTBL	          3
#define ACPHY_TBL_ID_NVRXEVMSHAPINGTBL	          4
#define ACPHY_TBL_ID_PHASETRACKTBL                5
#define ACPHY_TBL_ID_SQTHRESHOLD                  6
#define ACPHY_TBL_ID_RFSEQ                        7
#define ACPHY_TBL_ID_RFSEQEXT                     8
#define ACPHY_TBL_ID_ANTSWCTRLLUT                 9
#define ACPHY_TBL_ID_FEMCTRLLUT                  10
#define ACPHY_TBL_ID_GAINLIMIT                   11
#define ACPHY_TBL_ID_IQLOCAL                     12
#define ACPHY_TBL_ID_PAPR                        13
#define ACPHY_TBL_ID_SAMPLEPLAY                  14
#define ACPHY_TBL_ID_DUPSTRNTBL                  15
#define ACPHY_TBL_ID_BFMUSERINDEX                16
#define ACPHY_TBL_ID_BFECONFIG                   17
#define ACPHY_TBL_ID_BFEMATRIX                   18
#define ACPHY_TBL_ID_FASTCHSWITCH                19
#define ACPHY_TBL_ID_RFSEQBUNDLE                 20
#define ACPHY_TBL_ID_LNAROUT                     21
#define ACPHY_TBL_ID_MCDSNRVAL                   22
#define ACPHY_TBL_ID_BFRRPT                      23
#define ACPHY_TBL_ID_BFERPT                      24
#define ACPHY_TBL_ID_GAINCTRLBBMULTLUTS          32
#define ACPHY_TBL_ID_ESTPWRSHFTLUTS              33
#define ACPHY_TBL_ID_ESTPWRLUTS0                 64
#define ACPHY_TBL_ID_IQCOEFFLUTS0                65
#define ACPHY_TBL_ID_LOFTCOEFFLUTS0              66
#define ACPHY_TBL_ID_RFPWRLUTS0                  67
#define ACPHY_TBL_ID_GAIN0                       68
#define ACPHY_TBL_ID_GAINBITS0                   69
#define ACPHY_TBL_ID_RSSICLIPGAIN0               70
#define ACPHY_TBL_ID_EPSILON0                    71
#define ACPHY_TBL_ID_SCALAR0                     72
#define ACPHY_TBL_ID_CORE0CHANESTTBL             73
#define ACPHY_TBL_ID_SNOOPAGC                    80
#define ACPHY_TBL_ID_SNOOPPEAK                   81
#define ACPHY_TBL_ID_SNOOPCCKLMS                 82
#define ACPHY_TBL_ID_SNOOPLMS                    83
#define ACPHY_TBL_ID_SNOOPDCCMP                  84
#define ACPHY_TBL_ID_ESTPWRLUTS1                 96
#define ACPHY_TBL_ID_IQCOEFFLUTS1                97
#define ACPHY_TBL_ID_LOFTCOEFFLUTS1              98
#define ACPHY_TBL_ID_RFPWRLUTS1                  99
#define ACPHY_TBL_ID_GAIN1                      100
#define ACPHY_TBL_ID_GAINBITS1                  101
#define ACPHY_TBL_ID_RSSICLIPGAIN1              102
#define ACPHY_TBL_ID_EPSILON1                   103
#define ACPHY_TBL_ID_SCALAR1                    104
#define ACPHY_TBL_ID_CORE1CHANESTTBL            105
#define ACPHY_TBL_ID_ESTPWRLUTS2                128
#define ACPHY_TBL_ID_IQCOEFFLUTS2               129
#define ACPHY_TBL_ID_LOFTCOEFFLUTS2             130
#define ACPHY_TBL_ID_RFPWRLUTS2                 131
#define ACPHY_TBL_ID_GAIN2                      132
#define ACPHY_TBL_ID_GAINBITS2                  133
#define ACPHY_TBL_ID_RSSICLIPGAIN2              134
#define ACPHY_TBL_ID_EPSILON2                   135
#define ACPHY_TBL_ID_SCALAR2                    136
#define ACPHY_TBL_ID_CORE2CHANESTTBL            137


/* When the broadcast bit is set in the PHY reg address
 * it writes to the corresponding registers in all the cores
 */
#define ACPHY_REG_BROADCAST (ACREV0 ? 0x1000 : 0)

/* ACPHY RFSeq Commands */
#define ACPHY_RFSEQ_RX2TX		0x0
#define ACPHY_RFSEQ_TX2RX		0x1
#define ACPHY_RFSEQ_RESET2RX		0x2
#define ACPHY_RFSEQ_UPDATEGAINH		0x3
#define ACPHY_RFSEQ_UPDATEGAINL		0x4
#define ACPHY_RFSEQ_UPDATEGAINU		0x5

#define ACPHY_SPINWAIT_RFSEQ_STOP		1000
#define ACPHY_SPINWAIT_RFSEQ_FORCE		200000
#define ACPHY_SPINWAIT_RUNSAMPLE		1000
#define ACPHY_SPINWAIT_TXIQLO			20000
#define ACPHY_SPINWAIT_IQEST			10000

#define ACPHY_NUM_BW                    3
#define ACPHY_NUM_CHANS                 123

#define ACPHY_ClassifierCtrl_classifierSel_MASK 0x7

/* Board types */
#define BCM94360MCI_SSID 0x61a
#define BCM94360CS_SSID  0x61b

/* gainctrl */
/* stages: elna, lna1, lna2, mix, bq0, bq1 */
#define ACPHY_MAX_RX_GAIN_STAGES 6
#define ACPHY_INIT_GAIN 69
#define ACPHY_CRSMIN_DEFAULT 54
#define ACPHY_MAX_LNA1_IDX 5
#define ACPHY_ILNA2G_MAX_LNA2_IDX 6
#define ACPHY_ELNA2G_MAX_LNA2_IDX 4
#define ACPHY_ELNA2G_MAX_LNA2_IDX_L 5
#define ACPHY_ILNA5G_MAX_LNA2_IDX 6
#define ACPHY_ELNA5G_MAX_LNA2_IDX 6

/* JIRA(CRDOT11ACPHY-142) - Don't use idx = 0 of lna1/lna2 */
#define ACPHY_MIN_LNA1_LNA2_IDX 1

/* ACI (start) */
#define ACPHY_ACI_CHAN_LIST_SZ 5

#define ACPHY_ACI_MAX_DESENSE_DB 24
#define ACPHY_ACI_COARSE_DESENSE_UP 4
#define ACPHY_ACI_COARSE_DESENSE_DN 4

#define ACPHY_ACI_GLITCH_BUFFER_SZ 8
#define ACPHY_ACI_NUM_MAX_GLITCH_AVG 2
#define ACPHY_ACI_WAIT_POST_MITIGATION 1
#define ACPHY_ACI_OFDM_HI_GLITCH_THRESH 600
#define ACPHY_ACI_OFDM_LO_GLITCH_THRESH 300
#define ACPHY_ACI_BPHY_HI_GLITCH_THRESH 300
#define ACPHY_ACI_BPHY_LO_GLITCH_THRESH 100
#define ACPHY_ACI_LO_GLITCH_SLEEP 8
#define ACPHY_ACI_BORDER_GLITCH_SLEEP 20
#define ACPHY_ACI_MD_GLITCH_SLEEP 15
/* ACI (end) */

#define ACPHY_TBL_ID_ESTPWRLUTS(core)	\
	(((core == 0) ? ACPHY_TBL_ID_ESTPWRLUTS0 : \
	((core == 1) ? ACPHY_TBL_ID_ESTPWRLUTS1 : ACPHY_TBL_ID_ESTPWRLUTS2)))

#define ACPHY_TBL_ID_CHANEST(core)	\
	(((core == 0) ? ACPHY_TBL_ID_CORE0CHANESTTBL : \
	((core == 1) ? ACPHY_TBL_ID_CORE1CHANESTTBL : ACPHY_TBL_ID_CORE2CHANESTTBL)))


#define ACREV1X1_IS(phy_rev) \
	((ACREV_IS(phy_rev, 2) || ACREV_IS(phy_rev, 5) || ACREV_IS(phy_rev, 6)))

/* Check Major Radio Revid is 1 or not */
#define ACRADIO1X1_IS(radio_rev_id) \
	(radio_rev_id >> 4 == 1)

/* Check Minor Radio Revid */
#define ACRADIO_IPA1X1_IS(radio_rev_id) \
	((radio_rev_id >> 4 == 1) && \
	(((radio_rev_id & 0xF) == 0x1) || ((radio_rev_id & 0xF) == 0x7)))

/* Check Minor Radio Revid */
#define ACRADIO_EPA1X1_IS(radio_rev_id) \
	((radio_rev_id >> 4 == 1) && \
	(((radio_rev_id & 0xF) == 0x2) || ((radio_rev_id & 0xF) == 0x8)))

#ifdef DSLCPE_C601911
#define ACPHY_NEED_REINIT(pi) \
	(((phy_info_acphy_t *)(pi)->u.pi_acphy)->need_reinit)
#endif

#ifdef ENABLE_FCBS
/* FCBS */
/* time to spinwait while waiting for the FCBS
 * switch trigger bit to go low after FCBS
 */
#define ACPHY_SPINWAIT_FCBS_SWITCH 2000
#define ACPHY_FCBS_PHYTBL16_LEN 400

/* PHY specific on-chip RAM offset of the FCBS cache */
#define FCBS_ACPHY_TMPLRAM_STARTADDR	0x1000

/* PHY specific shmem locations for specifying the length
 * of radio reg cache, phytbl cache, phyreg cache
 */
#define M_FCBS_ACPHY_RADIOREG			0x922
#define M_FCBS_ACPHY_PHYTBL16			0x924
#define M_FCBS_ACPHY_PHYTBL32			0x926
#define M_FCBS_ACPHY_PHYREG				0x928
#define M_FCBS_ACPHY_BPHYCTRL			0x92a
#define M_FCBS_ACPHY_TEMPLATE_PTR		0x92c

typedef struct _acphy_fcbs_info {
	uint16 				phytbl16_buf_ChanA[ACPHY_FCBS_PHYTBL16_LEN];
	uint16 				phytbl16_buf_ChanB[ACPHY_FCBS_PHYTBL16_LEN];
} acphy_fcbs_info;

#endif /* ENABLE_FCBS */

typedef struct _chan_info_radio2069 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 RFP_pll_vcocal5;
	uint16 RFP_pll_vcocal6;
	uint16 RFP_pll_vcocal2;
	uint16 RFP_pll_vcocal1;
	uint16 RFP_pll_vcocal11;
	uint16 RFP_pll_vcocal12;
	uint16 RFP_pll_frct2;
	uint16 RFP_pll_frct3;
	uint16 RFP_pll_vcocal10;
	uint16 RFP_pll_xtal3;
	uint16 RFP_pll_vco2;
	uint16 RF0_logen5g_cfg1;
	uint16 RFP_pll_vco8;
	uint16 RFP_pll_vco6;
	uint16 RFP_pll_vco3;
	uint16 RFP_pll_xtalldo1;
	uint16 RFP_pll_hvldo1;
	uint16 RFP_pll_hvldo2;
	uint16 RFP_pll_vco5;
	uint16 RFP_pll_vco4;
	uint16 RFP_pll_lf4;
	uint16 RFP_pll_lf5;
	uint16 RFP_pll_lf7;
	uint16 RFP_pll_lf2;
	uint16 RFP_pll_lf3;
	uint16 RFP_pll_cp4;
	uint16 RFP_pll_dsp1;
	uint16 RFP_pll_dsp2;
	uint16 RFP_pll_dsp3;
	uint16 RFP_pll_dsp4;
	uint16 RFP_pll_dsp6;
	uint16 RFP_pll_dsp7;
	uint16 RFP_pll_dsp8;
	uint16 RFP_pll_dsp9;
	uint16 RF0_logen2g_tune;
	uint16 RFX_lna2g_tune;
	uint16 RFX_txmix2g_cfg1;
	uint16 RFX_pga2g_cfg2;
	uint16 RFX_pad2g_tune;
	uint16 RF0_logen5g_tune1;
	uint16 RF0_logen5g_tune2;
	uint16 RFX_logen5g_rccr;
	uint16 RFX_lna5g_tune;
	uint16 RFX_txmix5g_cfg1;
	uint16 RFX_pga5g_cfg2;
	uint16 RFX_pad5g_tune;
	uint16 RFP_pll_cp5;
	uint16 RF0_afediv1;
	uint16 RF0_afediv2;
	uint16 RFX_adc_cfg5;

	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_radio2069_t;

typedef struct _chan_info_radio2069revGE16 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 RFP_pll_vcocal5;
	uint16 RFP_pll_vcocal6;
	uint16 RFP_pll_vcocal2;
	uint16 RFP_pll_vcocal1;
	uint16 RFP_pll_vcocal11;
	uint16 RFP_pll_vcocal12;
	uint16 RFP_pll_frct2;
	uint16 RFP_pll_frct3;
	uint16 RFP_pll_vcocal10;
	uint16 RFP_pll_xtal3;
	uint16 RFP_pll_vco2;
	uint16 RFP_logen5g_cfg1;
	uint16 RFP_pll_vco8;
	uint16 RFP_pll_vco6;
	uint16 RFP_pll_vco3;
	uint16 RFP_pll_xtalldo1;
	uint16 RFP_pll_hvldo1;
	uint16 RFP_pll_hvldo2;
	uint16 RFP_pll_vco5;
	uint16 RFP_pll_vco4;
	uint16 RFP_pll_lf4;
	uint16 RFP_pll_lf5;
	uint16 RFP_pll_lf7;
	uint16 RFP_pll_lf2;
	uint16 RFP_pll_lf3;
	uint16 RFP_pll_cp4;
	uint16 RFP_pll_lf6;
	uint16 RFP_logen2g_tune;
	uint16 RF0_lna2g_tune;
	uint16 RF0_txmix2g_cfg1;
	uint16 RF0_pga2g_cfg2;
	uint16 RF0_pad2g_tune;
	uint16 RFP_logen5g_tune1;
	uint16 RFP_logen5g_tune2;
	uint16 RF0_logen5g_rccr;
	uint16 RF0_lna5g_tune;
	uint16 RF0_txmix5g_cfg1;
	uint16 RF0_pga5g_cfg2;
	uint16 RF0_pad5g_tune;
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} chan_info_radio2069revGE16_t;

typedef struct acphy_sfo_cfg {
	uint16 PHY_BW1a;
	uint16 PHY_BW2;
	uint16 PHY_BW3;
	uint16 PHY_BW4;
	uint16 PHY_BW5;
	uint16 PHY_BW6;
} acphy_sfo_cfg_t;

typedef struct _chan_info_rx_farrow {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 deltaphase_lo;
	uint16 deltaphase_hi;
	uint16 drift_period;
	uint16 farrow_ctrl;
} chan_info_rx_farrow;


typedef struct _chan_info_tx_farrow {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

	uint16 MuDelta_l;
	uint16 MuDelta_u;
	uint16 MuDeltaInit_l;
	uint16 MuDeltaInit_u;
} chan_info_tx_farrow;

typedef struct _acphy_txcal_radioregs {
	bool   is_orig;
	uint16 iqcal_cfg1[PHY_CORE_MAX];
	uint16 pa2g_tssi[PHY_CORE_MAX];
	uint16 OVR20[PHY_CORE_MAX];
	uint16 OVR21[PHY_CORE_MAX];
	uint16 tx5g_tssi[PHY_CORE_MAX];
	uint16 iqcal_cfg2[PHY_CORE_MAX];
	uint16 iqcal_cfg3[PHY_CORE_MAX];
	uint16 auxpga_cfg1[PHY_CORE_MAX];
} acphy_txcal_radioregs_t;

typedef struct _acphy_txcal_phyregs {
	bool   is_orig;
	uint16 BBConfig;
	uint16 RxFeCtrl1;
	uint16 AfePuCtrl;

	uint16 RfctrlOverrideAfeCfg[PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg1[PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg2[PHY_CORE_MAX];
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 RfctrlOverrideRxPus[PHY_CORE_MAX];
	uint16 RfctrlCoreRxPus[PHY_CORE_MAX];
	uint16 RfctrlOverrideTxPus[PHY_CORE_MAX];
	uint16 RfctrlCoreTxPus[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfSwtch[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfSwtch[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfGmult[PHY_CORE_MAX];
	uint16 RfctrlCoreRCDACBuf[PHY_CORE_MAX];
	uint16 RfctrlOverrideAuxTssi[PHY_CORE_MAX];
	uint16 RfctrlCoreAuxTssi1[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
	uint16 RfseqCoreActv2059;
} acphy_txcal_phyregs_t;


typedef struct _acphy_rxcal_radioregs {
	bool   is_orig;
	uint16 RF_2069_TXRX2G_CAL_TX[PHY_CORE_MAX];
	uint16 RF_2069_TXRX5G_CAL_TX[PHY_CORE_MAX];
	uint16 RF_2069_TXRX2G_CAL_RX[PHY_CORE_MAX];
	uint16 RF_2069_TXRX5G_CAL_RX[PHY_CORE_MAX];
	uint16 RF_2069_RXRF2G_CFG2[PHY_CORE_MAX];
	uint16 RF_2069_RXRF5G_CFG2[PHY_CORE_MAX];
} acphy_rxcal_radioregs_t;

typedef struct _acphy_rxcal_phyregs {
	bool   is_orig;
	uint16 RfctrlOverrideTxPus [PHY_CORE_MAX];
	uint16 RfctrlCoreTxPus [PHY_CORE_MAX];
	uint16 RfctrlOverrideRxPus [PHY_CORE_MAX];
	uint16 RfctrlCoreRxPus [PHY_CORE_MAX];
	uint16 RfctrlOverrideGains [PHY_CORE_MAX];
	uint16 Dac_gain [PHY_CORE_MAX];
	uint16 RfctrlCoreTXGAIN1 [PHY_CORE_MAX];
	uint16 RfctrlCoreTXGAIN2 [PHY_CORE_MAX];
	uint16 RfctrlCoreRXGAIN1 [PHY_CORE_MAX];
	uint16 RfctrlCoreRXGAIN2 [PHY_CORE_MAX];
	uint16 RfctrlCoreLpfGain [PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfCT [PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT [PHY_CORE_MAX];
	uint16 RfctrlCoreLpfGmult [PHY_CORE_MAX];
	uint16 RfctrlCoreRCDACBuf [PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfSwtch [PHY_CORE_MAX];
	uint16 RfctrlCoreLpfSwtch [PHY_CORE_MAX];
	uint16 RfctrlOverrideAfeCfg [PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg1 [PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg2 [PHY_CORE_MAX];
	uint16 RfctrlOverrideLowPwrCfg [PHY_CORE_MAX];
	uint16 RfctrlCoreLowPwr [PHY_CORE_MAX];
	uint16 RfctrlOverrideAuxTssi [PHY_CORE_MAX];
	uint16 RfctrlCoreAuxTssi1 [PHY_CORE_MAX];
	uint16 RfctrlCoreAuxTssi2[PHY_CORE_MAX];
	uint16 RfctrlOverrideGlobalPus;
	uint16 RfctrlCoreGlobalPus;
	uint16 bbmult[PHY_CORE_MAX];
	uint16 rfseq_txgain[3 * PHY_CORE_MAX];
	uint16 RfseqCoreActv2059;
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
	uint16 AfePuCtrl;

	uint8 txpwridx[PHY_CORE_MAX];
} acphy_rxcal_phyregs_t;

typedef struct _acphy_tempsense_phyregs {
	bool   is_orig;

	uint16 RxFeCtrl1;
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 RfctrlOverrideAuxTssi[PHY_CORE_MAX];
	uint16 RfctrlCoreAuxTssi1[PHY_CORE_MAX];
	uint16 RfctrlOverrideRxPus[PHY_CORE_MAX];
	uint16 RfctrlCoreRxPus[PHY_CORE_MAX];
	uint16 RfctrlOverrideTxPus[PHY_CORE_MAX];
	uint16 RfctrlCoreTxPus[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfSwtch[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfSwtch[PHY_CORE_MAX];
	uint16 RfctrlOverrideGains[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfGain[PHY_CORE_MAX];
	uint16 RfctrlOverrideAfeCfg[PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg1[PHY_CORE_MAX];
	uint16 RfctrlCoreAfeCfg2[PHY_CORE_MAX];
} acphy_tempsense_phyregs_t;

typedef struct _acphy_tempsense_radioregs {
	bool   is_orig;
	uint16 OVR18[PHY_CORE_MAX];
	uint16 OVR19[PHY_CORE_MAX];
	uint16 OVR5[PHY_CORE_MAX];
	uint16 OVR3[PHY_CORE_MAX];
	uint16 tempsense_cfg[PHY_CORE_MAX];
	uint16 testbuf_cfg1[PHY_CORE_MAX];
	uint16 auxpga_cfg1[PHY_CORE_MAX];
	uint16 auxpga_vmid[PHY_CORE_MAX];
} acphy_tempsense_radioregs_t;

typedef struct acphy_rx_fdiqi_struct {
	int8 freq;
	int32 angle[PHY_CORE_MAX];
	int32 mag[PHY_CORE_MAX];
} acphy_rx_fdiqi_t;


typedef struct acphy_rx_fdiqi_ctl_struct {
	bool forced;
	uint16 forced_val;
	bool enabled;
	int32 slope[PHY_CORE_MAX];
	uint8 leakage_comp_mode;
} acphy_rx_fdiqi_ctl_t;

typedef struct acphy_iq_mismatch_struct {
	int32 angle;
	int32 mag;
	int32 sin_angle;
} acphy_iq_mismatch_t;

typedef struct {
	uint8 elna;
	uint8 trloss;
	uint8 elna_bypass_tr;
} acphy_fem_rxgains_t;

typedef struct {
	bool elna2g_present, elna5g_present;
	uint8 femctrl, femctrl_sub;
	uint8 rfpll_5g;
	uint8 rcal_war;
	uint8 txgaintbl_id;
	uint8 gainboosta01;
	uint8 bt_coex;
	acphy_fem_rxgains_t femrx_2g[PHY_CORE_MAX];
	acphy_fem_rxgains_t femrx_5g[PHY_CORE_MAX];
	acphy_fem_rxgains_t femrx_5gm[PHY_CORE_MAX];
	acphy_fem_rxgains_t femrx_5gh[PHY_CORE_MAX];
} acphy_srom_t;

typedef struct acphy_rxgainctrl_params {
	int8  gaintbl[ACPHY_MAX_RX_GAIN_STAGES][10];
	uint8 gainbitstbl[ACPHY_MAX_RX_GAIN_STAGES][10];
} acphy_rxgainctrl_t;

typedef struct acphy_lpfCT_phyregs {
	bool   is_orig;
	uint16 RfctrlOverrideLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT[PHY_CORE_MAX];
} acphy_lpfCT_phyregs_t;


typedef struct acphy_desense_values
{
	uint8 ofdm_desense, bphy_desense;      /* in dBs */
	uint8 lna1_tbl_desense, lna2_tbl_desense;   /* in ticks */
	uint8 lna1_gainlmt_desense, lna2_gainlmt_desense;   /* in ticks */
	uint8 elna_bypass;
	uint8 nf_hit_lna12;      /* (mostly to adjust nb/w1 clip for bt cases */
	bool on;
}  acphy_desense_values_t;

typedef struct desense_history {
	uint16 glitches[ACPHY_ACI_GLITCH_BUFFER_SZ];
	uint8 hi_glitch_dB;
	uint8 lo_glitch_dB;
	uint8 no_desense_change_time_cnt;
} desense_history_t;

typedef struct acphy_aci_params {
	/* array is indexed by chan/bw */
	uint8 chan;
	uint16 bw;
	uint64 last_updated;

	acphy_desense_values_t desense;
	int8 weakest_rssi;

	desense_history_t bphy_hist;
	desense_history_t ofdm_hist;
	uint8 glitch_buff_idx, glitch_upd_wait;
} acphy_aci_params_t;

struct phy_info_acphy {
	uint8  dac_mode;
	uint16 bb_mult_save[PHY_CORE_MAX];
	uint8  bb_mult_save_valid;
	uint16 classifier_state;
	uint16 clip_state[PHY_CORE_MAX];
	uint16 deaf_count;
	uint16 saved_bbconf;
	int8   txpwrindex[PHY_CORE_MAX]; 		/* index if hwpwrctrl if OFF */
	int8   phy_noise_all_core[PHY_CORE_MAX]; /* noise power in dB for all cores */
	int8   phy_noise_in_crs_min[PHY_CORE_MAX]; /* noise power in dB for all cores */
	acphy_txcal_radioregs_t ac_txcal_radioregs_orig;
	acphy_txcal_phyregs_t   ac_txcal_phyregs_orig;
	acphy_rxcal_radioregs_t ac_rxcal_radioregs_orig;
	acphy_rxcal_phyregs_t   ac_rxcal_phyregs_orig;
	acphy_tempsense_radioregs_t ac_tempsense_radioregs_orig;
	acphy_tempsense_phyregs_t   ac_tempsense_phyregs_orig;
	acphy_lpfCT_phyregs_t 	ac_lpfCT_phyregs_orig;
	uint32 pstart; /* sample collect fifo begins */
	uint32 pstop;  /* sample collect fifo ends */
	uint32 pfirst; /* sample collect trigger begins */
	uint32 plast;  /* sample collect trigger ends */

#if defined(BCMDBG_RXCAL)
	phy_iq_est_t  rxcal_noise[PHY_CORE_MAX];
	phy_iq_est_t  rxcal_signal[PHY_CORE_MAX];
#endif

	bool init;
	bool init_done;
	uint8 curr_band2g;
	uint32 curr_bw;

	/* result of radio rccal */
	uint8 rccal_gmult;
	uint8 rccal_gmult_rc;
	uint8 rccal_dacbuf;

	/* Flag for enabling auto crsminpower cal */
	bool crsmincal_enable;
	uint8  crsmincal_run;

	/* ACPHY FEM value from SROM */
	acphy_srom_t srom;

	/* pdet_range_id */
	uint8 srom_2g_pdrange_id;
	uint8 srom_5g_pdrange_id;

	txcal_coeffs_t txcal_cache[PHY_CORE_MAX];
	uint16	txcal_cache_cookie;
	uint8   radar_cal_active; /* to mask radar detect during cal's tone-play */

	int16 idle_tssi[PHY_CORE_MAX];
	int8  txpwr_offset[PHY_CORE_MAX];	/* qdBm signed offset for per-core tx pwr */
	uint8 txpwrindex_hw_save[PHY_CORE_MAX]; /* txpwr start index for hwpwrctrl */

	/* rx gainctrl */
	acphy_fem_rxgains_t fem_rxgains[PHY_CORE_MAX];
	acphy_rxgainctrl_t rxgainctrl_params[PHY_CORE_MAX];
	uint8 rxgainctrl_stage_len[ACPHY_MAX_RX_GAIN_STAGES];
	uint8 rxgainctrl_maxout_gains[ACPHY_MAX_RX_GAIN_STAGES];

	/* desense */
	acphy_desense_values_t curr_desense, zero_desense, total_desense;
	bool limit_desense_on_rssi;

	/* aci (aci, cci, noise) */
	acphy_aci_params_t aci_list[ACPHY_ACI_CHAN_LIST_SZ];
	acphy_aci_params_t *aci;

	/* bt */
	int32 btc_mode;
	acphy_desense_values_t bt_desense;

	acphy_rx_fdiqi_ctl_t fdiqi;

	int16 current_temperature;

	bool poll_adc_WAR;

	#ifdef ENABLE_FCBS
	acphy_fcbs_info ac_fcbs;
	#endif /* ENABLE_FCBS */

	uint16 rfldo;
	uint8 acphy_lp_mode;	/* To select the low pweor mode */
	uint8 acphy_prev_lp_mode;
	uint8 acphy_lp_status;
	uint8 acphy_4335_radio_pd_status;
	uint16 rxRfctrlCoreRxPus0, rxRfctrlOverrideRxPus0;
	uint16 afeRfctrlCoreAfeCfg10, afeRfctrlCoreAfeCfg20, afeRfctrlOverrideAfeCfg0;
	uint16 txRfctrlCoreTxPus0, txRfctrlOverrideTxPus0;
	uint16 radioRfctrlCmd, radioRfctrlCoreGlobalPus, radioRfctrlOverrideGlobalPus;
	uint8 acphy_cck_dig_filt_type;
	bool   ac_rxldpc_override;	/* LDPC override for RX, both band */
#ifdef DSLCPE_C601911
	bool need_reinit;		/*WAR for rxchain_pwrsave*/
#endif
};

#endif /* _wlc_phy_ac_h_ */
