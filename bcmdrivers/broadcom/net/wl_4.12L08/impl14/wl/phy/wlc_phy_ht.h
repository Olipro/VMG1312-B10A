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
 * $Id: wlc_phy_ht.h 345185 2012-07-16 23:43:01Z $
 */

#ifndef _wlc_phy_ht_h_
#define _wlc_phy_ht_h_

#include <typedefs.h>

#define HTPHY_GAIN_VS_TEMP_SLOPE_2G 8   /* units: db/100C */
#define HTPHY_GAIN_VS_TEMP_SLOPE_5G 8   /* units: db/100C */
#define HTPHY_TEMPSENSE_TIMER 10

typedef struct _htphy_dac_adc_decouple_war {
	bool   is_on;
	uint16 PapdCalShifts[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
	uint16 PapdCalCorrelate;
	uint16 PapdEpsilonUpdateIterations;
	uint16 PapdCalSettle;
} htphy_dac_adc_decouple_war_t;

typedef struct _htphy_rxcal_radioregs {
	bool   is_orig;
	uint16 RF_TX_txrxcouple_2g_pwrup[PHY_CORE_MAX];
	uint16 RF_TX_txrxcouple_2g_atten[PHY_CORE_MAX];
	uint16 RF_TX_txrxcouple_5g_pwrup[PHY_CORE_MAX];
	uint16 RF_TX_txrxcouple_5g_atten[PHY_CORE_MAX];
	uint16 RF_afe_vcm_cal_master[PHY_CORE_MAX];
	uint16 RF_afe_set_vcm_i[PHY_CORE_MAX];
	uint16 RF_afe_set_vcm_q[PHY_CORE_MAX];
	uint16 RF_rxbb_vgabuf_idacs[PHY_CORE_MAX];
	uint16 RF_rxbuf_degen[PHY_CORE_MAX];
} htphy_rxcal_radioregs_t;

typedef struct _htphy_rxcal_phyregs {
	bool   is_orig;
	uint16 BBConfig;
	uint16 bbmult[PHY_CORE_MAX];
	uint16 rfseq_txgain[PHY_CORE_MAX];
	uint16 Afectrl[PHY_CORE_MAX];
	uint16 AfectrlOverride[PHY_CORE_MAX];
	uint16 RfseqCoreActv2059;
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 RfctrlCorePU[PHY_CORE_MAX];
	uint16 RfctrlOverride[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfPU[PHY_CORE_MAX];
	uint16 RfctrlOverrideLpfPU[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
} htphy_rxcal_phyregs_t;

typedef struct _htphy_txcal_radioregs {
	bool   is_orig;
	uint16 RF_TX_tx_ssi_master[PHY_CORE_MAX];
	uint16 RF_TX_tx_ssi_mux[PHY_CORE_MAX];
	uint16 RF_TX_tssia[PHY_CORE_MAX];
	uint16 RF_TX_tssig[PHY_CORE_MAX];
	uint16 RF_TX_iqcal_vcm_hg[PHY_CORE_MAX];
	uint16 RF_TX_iqcal_idac[PHY_CORE_MAX];
	uint16 RF_TX_tssi_misc1[PHY_CORE_MAX];
	uint16 RF_TX_tssi_vcm[PHY_CORE_MAX];
} htphy_txcal_radioregs_t;

typedef struct _htphy_txcal_phyregs {
	bool   is_orig;
	uint16 BBConfig;
	uint16 Afectrl[PHY_CORE_MAX];
	uint16 AfectrlOverride[PHY_CORE_MAX];
	uint16 Afectrl_AuxADCmode[PHY_CORE_MAX];
	uint16 RfctrlIntc[PHY_CORE_MAX];
	uint16 RfctrlPU[PHY_CORE_MAX];
	uint16 RfctrlOverride[PHY_CORE_MAX];
	uint16 PapdEnable[PHY_CORE_MAX];
} htphy_txcal_phyregs_t;

typedef struct _htphy_rxgain_phyregs {
	bool   is_orig;
	uint16 RfctrlOverride[PHY_CORE_MAX];
	uint16 RfctrlRXGAIN[PHY_CORE_MAX];
	uint16 Rfctrl_lpf_gain[PHY_CORE_MAX];
} htphy_rxgain_phyregs_t;

typedef struct _htphy_lpfCT_phyregs {
	bool   is_orig;
	uint16 RfctrlOverrideLpfCT[PHY_CORE_MAX];
	uint16 RfctrlCoreLpfCT[PHY_CORE_MAX];
} htphy_lpfCT_phyregs_t;

typedef struct _htphy_rcal_rccal_cache {
	bool cache_valid;
	uint16 rcal_val;
	uint16 rccal_bcap_val;
	uint16 rccal_scap_val;
	uint16 rccal_hpc_val;
} htphy_rcal_rccal_cache;

typedef struct _htphy_nshapetbl_mon {
	uint8 start_addr[NTONES_BW40];
	uint8 mod_length[NTONES_BW40];
	uint8 length;
} htphy_nshapetbl_mon_t;


/* Fast channel and band switch (FCBS) chip specific structures */
#ifdef ENABLE_FCBS

/* PHY specific on-chip RAM offset of the FCBS cache */
#define FCBS_HTPHY_TMPLRAM_STARTADDR	0x6b0

/* PHY specific shmem locations for specifying the length
 * of radio reg cache, phytbl cache, phyreg cache
 */
#define M_FCBS_HTPHY_RADIOREG			0x7a0
#define M_FCBS_HTPHY_PHYTBL16			0x7a2
#define M_FCBS_HTPHY_PHYTBL32			0x7a4
#define M_FCBS_HTPHY_PHYREG				0x7a6
#define M_FCBS_HTPHY_BPHYCTRL			0x7a8
#define M_FCBS_HTPHY_TEMPLATE_PTR		0x7aa

/* Total count of HT PHY registers that have to be updated for FCBS */
#define HTPHY_FCBS_PHYREG_CNT	128

/* Total count of HT PHY radio registers that have to be updated for FCBS */
#define HTPHY_FCBS_RADIOREG_CNT	128

/* Total length (in bytes) of HT PHY table buffers including the header */
#define HTPHY_FCBS_PHYTBL16_WORDS	1024
#define HTPHY_FCBS_PHYTBL32_WORDS	512

typedef struct _htphy_fcbs_info {
	fcbs_radioreg_buf_entry		radioreg_buf_ChanA[HTPHY_FCBS_RADIOREG_CNT];
	fcbs_radioreg_buf_entry		radioreg_buf_ChanB[HTPHY_FCBS_RADIOREG_CNT];
	fcbs_phyreg_buf_entry		phyreg_buf_ChanA[HTPHY_FCBS_PHYREG_CNT];
	fcbs_phyreg_buf_entry		bphyreg_buf_ChanA[HTPHY_FCBS_PHYREG_CNT];
	fcbs_phyreg_buf_entry		phyreg_buf_ChanB[HTPHY_FCBS_PHYREG_CNT];
	fcbs_phyreg_buf_entry		bphyreg_buf_ChanB[HTPHY_FCBS_PHYREG_CNT];
	uint16 				phytbl16_buf_ChanA[HTPHY_FCBS_PHYTBL16_WORDS];
	uint16 				phytbl16_buf_ChanB[HTPHY_FCBS_PHYTBL16_WORDS];
	uint16 				phytbl32_buf_ChanA[HTPHY_FCBS_PHYTBL32_WORDS];
	uint16 				phytbl32_buf_ChanB[HTPHY_FCBS_PHYTBL32_WORDS];
	uint8				txpwrCtrlIndex[MAX_FCBS_CHANS][PHY_CORE_MAX];
} htphy_fcbs_info;

#endif /* ENABLE_FCBS */

struct phy_info_htphy {
	uint16 classifier_state;
	uint16 clip_state[PHY_CORE_MAX];
	uint16 deaf_count;
	uint16 saved_bbconf;
	uint16 rfctrlIntc_save[PHY_CORE_MAX];
	uint16 bb_mult_save[PHY_CORE_MAX];
	uint8  bb_mult_save_valid;
	uint8  txpwrindex_hw_save[PHY_CORE_MAX]; /* txpwr start index for hwpwrctrl */
	int8   idle_tssi[PHY_CORE_MAX];
	int8   txpwr_offset[PHY_CORE_MAX];	/* qdBm signed offset for per-core tx pwr */

	uint32 pstart; /* sample collect fifo begins */
	uint32 pstop;  /* sample collect fifo ends */
	uint32 pfirst; /* sample collect trigger begins */
	uint32 plast;  /* sample collect trigger ends */

	bool   ht_rxldpc_override;	/* LDPC override for RX, both band */

	htphy_dac_adc_decouple_war_t ht_dac_adc_decouple_war_info;
	htphy_txcal_radioregs_t htphy_txcal_radioregs_orig;
	htphy_txcal_phyregs_t ht_txcal_phyregs_orig;
	htphy_rxcal_radioregs_t ht_rxcal_radioregs_orig;
	htphy_rxcal_phyregs_t  ht_rxcal_phyregs_orig;
	htphy_rxgain_phyregs_t ht_rxgain_phyregs_orig;
	htphy_lpfCT_phyregs_t ht_lpfCT_phyregs_orig;

	/* rcal and rccal caching */
	htphy_rcal_rccal_cache rcal_rccal_cache;

	/* nvnoiseshapingtbl monitor */
	htphy_nshapetbl_mon_t nshapetbl_mon;

	bool ht_ofdm20_filt_war_req;
	bool ht_ofdm20_filt_war_active;
	int8    txpwrindex[PHY_CORE_MAX]; 		/* index if hwpwrctrl if OFF */

	txcal_coeffs_t txcal_cache[PHY_CORE_MAX];
	uint16	txcal_cache_cookie;
	uint8   radar_cal_active; /* to mask radar detect during cal's tone-play */

	uint8	elna2g;
	uint8	elna5g;

#ifdef ENABLE_FCBS
	htphy_fcbs_info ht_fcbs;
#endif /* ENABLE_FCBS */
	int16 current_temperature;


	bool btc_restage_rxgain;          /* indicates if rxgain restaging is active */
	uint16 btc_saved_init_regs[PHY_CORE_MAX][2];  /* store phyreg values prior to
						       * restaging rxgain
						       */
	uint16 btc_saved_init_rfseq[PHY_CORE_MAX];    /* store rfseq table values prior to
						       * restaging rxgain
						       */
	uint16 btc_saved_nbclip[PHY_CORE_MAX];     /* store nbclip thresholds prior to
						    * restaging rxgain
						    */
	int8 btc_saved_elnagain[PHY_CORE_MAX][2];  /* store elna gains prior to
						    * restaging rxgain
						    */
	uint16 btc_saved_cliphi[PHY_CORE_MAX][2];  /* store cliphi gains prior to
						    * restaging rxgain
						    */
	uint16 btc_saved_cliplo[PHY_CORE_MAX][2];  /* store cliplo gains prior to
						    * restaging rxgain
						    */
	int8 btc_saved_lna1gains[PHY_CORE_MAX][4];  /* store lna1 gains prior to
						    * restaging rxgain
						    */
	int8 btc_saved_lna2gains[PHY_CORE_MAX][4];  /* store lna2 gains prior to
						    * restaging rxgain
						    */
	int8 btc_saved_lna1gainbits[PHY_CORE_MAX][4];  /* store lna1 gainbits prior to
							* restaging rxgain
							*/
	int8 btc_saved_lna2gainbits[PHY_CORE_MAX][4];  /* store lna2 gainbits prior to
							* restaging rxgain
							*/

};

#endif /* _wlc_phy_ht_h_ */
