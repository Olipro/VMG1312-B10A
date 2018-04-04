/*
 * Required functions exported by the PHY module (phy-dependent)
 * to common (os-independent) driver code.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_phy_hal.h 365871 2012-10-31 07:02:06Z $
 */

#ifndef _wlc_phy_h_
#define _wlc_phy_h_

#include <typedefs.h>
#include <bcmwifi_channels.h>
#include <wlioctl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <d11.h>
#include <wlc_phy_shim.h>
#ifdef PPR_API
#include <wlc_ppr.h>
#endif

/* Noise Cal enable */
#define NOISE_CAL_LCNXNPHY
#define ENABLE_LISTEN_GAIN_CHANGE

#define ENABLE_FDS

/* Radio macros */

/* Radio ID */
#define	IDCODE_VER_MASK		0x0000000f
#define	IDCODE_VER_SHIFT	0
#define	IDCODE_MFG_MASK		0x00000fff
#define	IDCODE_MFG_SHIFT	0
#define	IDCODE_ID_MASK		0x0ffff000
#define	IDCODE_ID_SHIFT		12
#define	IDCODE_REV_MASK		0xf0000000
#define	IDCODE_REV_SHIFT	28

#define IDCODE_ACPHY_ID_MASK   0xffff
#define IDCODE_ACPHY_ID_SHIFT  0
#define IDCODE_ACPHY_REV_MASK  0xffff0000
#define IDCODE_ACPHY_REV_SHIFT 16

#define	NORADIO_ID		0xe4f5
#define	NORADIO_IDCODE		0x4e4f5246

#define	BCM2050_ID		0x2050
#define	BCM2050_IDCODE		0x02050000
#define	BCM2050A0_IDCODE	0x1205017f
#define	BCM2050A1_IDCODE	0x2205017f
#define	BCM2050R8_IDCODE	0x8205017f

#define BCM2055_ID		0x2055
#define BCM2055_IDCODE		0x02055000
#define BCM2055A0_IDCODE	0x1205517f

#define BCM2056_ID		0x2056
#define BCM2056_IDCODE		0x02056000
#define BCM2056A0_IDCODE	0x1205617f

#define BCM2057_ID		0x2057
#define BCM2057_IDCODE		0x02057000
#define BCM2057A0_IDCODE	0x1205717f

#define BCM2059_ID		0x2059
#define BCM2059A0_IDCODE	0x0205917f

#define	BCM2060_ID		0x2060
#define	BCM2060_IDCODE		0x02060000
#define	BCM2060WW_IDCODE	0x1206017f

#define BCM2062_ID		0x2062
#define BCM2062_IDCODE		0x02062000
#define BCM2062A0_IDCODE	0x0206217f

#define BCM2063_ID		0x2063
#define BCM2063_IDCODE		0x02063000
#define BCM2063A0_IDCODE	0x0206317f

#define BCM2064_ID		0x2064
#define BCM2064_IDCODE		0x02064000
#define BCM2064A0_IDCODE	0x0206417f

#define BCM2065_ID		0x2065
#define BCM2065_IDCODE		0x02065000
#define BCM2065A0_IDCODE	0x0206517f

#define BCM2067_ID		0x2067
#define BCM2067_IDCODE		0x02067000
#define BCM2067A0_IDCODE	0x0206717f

#define BCM2066_ID		0x2066
#define BCM2066_IDCODE		0x02066000
#define BCM2066A0_IDCODE	0x0206617f

#define BCM20671_ID		0x022e
#define BCM20671_IDCODE		0x0022e000
#define BCM20671A0_IDCODE	0x0022e17f
#define BCM20671A1_IDCODE	0x1022e17f
#define BCM20671B0_IDCODE	0x11022e17f

#define BCM2069_ID		0x2069
#define BCM2069A0_IDCODE	0x02069000

#define BCM20691_ID		0x30b

/* PHY macros */
#define PHY_MAX_CORES		4	/* max number of cores supported by PHY HAL */

#define PHY_TPC_HW_OFF		FALSE
#define PHY_TPC_HW_ON		TRUE

#define PHY_PERICAL_DRIVERUP	1	/* periodic cal for driver up */
#define PHY_PERICAL_WATCHDOG	2	/* periodic cal for watchdog */
#define PHY_PERICAL_PHYINIT	3	/* periodic cal for phy_init */
#define PHY_PERICAL_JOIN_BSS	4	/* periodic cal for join BSS */
#define PHY_PERICAL_START_IBSS	5	/* periodic cal for join IBSS */
#define PHY_PERICAL_UP_BSS	6	/* periodic cal for up BSS */
#define PHY_PERICAL_CHAN	7 	/* periodic cal for channel change */
#define PHY_PERICAL_DCS	    11 	/* periodic cal for DCS */
#define PHY_FULLCAL	8		/* full calibration routine */
#define PHY_PAPDCAL	10		/* papd calibration routine */
#ifdef WLOTA_EN
#define PHY_FULLCAL_SPHASE	PHY_PERICAL_JOIN_BSS /* full cal ina single phase */
#endif /* WLOTA_EN */
#ifdef DSLCPE_C601911
#define PHY_PERICAL_RXCHAIN	12
#endif
#define PHY_PERICAL_DISABLE	0	/* periodic cal disabled */
#define PHY_PERICAL_SPHASE	1	/* periodic cal enabled, single phase only */
#define PHY_PERICAL_MPHASE	2	/* periodic cal enabled, can do multiphase */
#define PHY_PERICAL_MANUAL	3	/* disable periodic cal, only run it from iovar */

#define PHY_PERICAL_MAXINTRVL (15*60) /* Maximum time interval in sec between PHY calibrations */

#define PHY_HOLD_FOR_ASSOC	1	/* hold PHY activities(like cal) during association */
#define PHY_HOLD_FOR_SCAN	2	/* hold PHY activities(like cal) during scan */
#define PHY_HOLD_FOR_RM		4	/* hold PHY activities(like cal) during radio measure */
#define PHY_HOLD_FOR_PLT	8	/* hold PHY activities(like cal) during plt */
#define PHY_HOLD_FOR_MUTE		0x10
#define PHY_HOLD_FOR_NOT_ASSOC	0x20
#define PHY_HOLD_FOR_ACI_SCAN	0x40 /* hold PHY activities(like cal) during ACI scan */
#define PHY_HOLD_FOR_PKT_ENG	0x80 /* hold PHY activities(like cal) during PKTENG mode */
#define PHY_HOLD_FOR_DCS	    0x100 /* hold PHY activities(like cal) during DCS */
#define PHY_HOLD_FOR_MPC_SCAN	0x200 /* hold PHY activities(like cal) during scan in mpc 1 mode */


#define PHY_MUTE_FOR_PREISM	1
#define PHY_MUTE_ALL		0xffffffff

/* Fixed Noise for APHY/BPHY/GPHY */
#define PHY_NOISE_FIXED_VAL 		(-95)	/* reported fixed noise */
#define PHY_NOISE_FIXED_VAL_NPHY       	(-92)	/* reported fixed noise */
#define PHY_NOISE_FIXED_VAL_LCNPHY     	(-92)	/* reported fixed noise */
#define PHY_NOISE_FIXED_VAL_SSLPNPHY    (-92)   /* reported fixed noise */

/* phy_mode bit defs for high level phy mode state information */
#define PHY_MODE_ACI		0x0001	/* set if phy is in ACI mitigation mode */
#define PHY_MODE_CAL		0x0002	/* set if phy is in Calibration mode */
#define PHY_MODE_NOISEM		0x0004	/* set if phy is in Noise Measurement mode */

#define WLC_TXPWR_DB_FACTOR	4	/* most txpower parameters are in 1/4 dB unit */

/* phy capabilities */
#define PHY_CAP_40MHZ		0x00000001	/* 40MHz channels supported */
#define PHY_CAP_80MHZ		0x00000008	/* 80MHz channels supported */
#define PHY_CAP_STBC		0x00000002
#define PHY_CAP_SGI			0x00000004
#define PHY_CAP_LDPC		0x00000010

/* channel bitvec */
typedef struct {
	uint8   vec[MAXCHANNEL/NBBY];   /* bitvec of channels */
} chanvec_t;

/* iovar table */
enum {
	/* OLD PHYTYPE specific ones, to phase out, use unified ones at the end
	 * For each unified, mark the original one as "depreciated".
	 * User scripts should stop using them for new testcases
	 */
	IOV_LCNPHY_LDOVOLT,
	IOV_LPPHY_ACI_CHAN_SCAN_CNT,
	IOV_LPPHY_ACI_CHAN_SCAN_CNT_THRESH,
	IOV_LPPHY_ACI_CHAN_SCAN_PWR_THRESH,
	IOV_LPPHY_ACI_CHAN_SCAN_TIMEOUT,
	IOV_LPPHY_ACI_GLITCH_TIMEOUT,
	IOV_LPPHY_ACI_OFF_THRESH,
	IOV_LPPHY_ACI_OFF_TIMEOUT,
	IOV_LPPHY_ACI_ON_THRESH,
	IOV_LPPHY_ACI_ON_TIMEOUT,
	IOV_LPPHY_CAL_DELTA_TEMP,
	IOV_LPPHY_CCK_ANALOG_FILT_BW_OVERRIDE,
	IOV_LPPHY_CCK_DIG_FILT_TYPE,
	IOV_LPPHY_CCK_RCCAL_OVERRIDE,
	IOV_LPPHY_CRS,
	IOV_LPPHY_FULLCAL,		/* depreciated */
	IOV_LPPHY_IDLE_TSSI_UPDATE_DELTA_TEMP,
	IOV_LPPHY_NOISE_SAMPLES,
	IOV_LPPHY_OFDM_ANALOG_FILT_BW_OVERRIDE,
	IOV_LPPHY_OFDM_DIG_FILT_TYPE,
	IOV_LPPHY_OFDM_RCCAL_OVERRIDE,
	IOV_LPPHY_PAPDCAL,
	IOV_LPPHY_PAPDCALTYPE,
	IOV_LPPHY_PAPDEPSTBL,
	IOV_LPPHY_PAPD_RECAL_COUNTER,
	IOV_LPPHY_PAPD_RECAL_ENABLE,
	IOV_LPPHY_PAPD_RECAL_GAIN_DELTA,
	IOV_LPPHY_PAPD_RECAL_MAX_INTERVAL,
	IOV_LPPHY_PAPD_RECAL_MIN_INTERVAL,
	IOV_LPPHY_PAPD_SLOW_CAL,
	IOV_LPPHY_RXIQCAL,
	IOV_LPPHY_RX_GAIN_TEMP_ADJ_METRIC,
	IOV_LPPHY_RX_GAIN_TEMP_ADJ_TEMPSENSE,
	IOV_LPPHY_RX_GAIN_TEMP_ADJ_THRESH,
	IOV_LPPHY_TEMPSENSE,		/* depreciated */
	IOV_LPPHY_TXIQCC,			/* depreciated */
	IOV_LPPHY_TXIQLOCAL,
	IOV_LPPHY_TXLOCC,			/* depreciated */
	IOV_LPPHY_TXPWRCTRL,		/* depreciated */
	IOV_LPPHY_TXPWRINDEX,		/* depreciated */
	IOV_PHY_OCLSCDENABLE,
	IOV_PHY_RXANTSEL,
	IOV_LPPHY_TXRF_SP_9_OVR,
	IOV_LPPHY_TX_TONE,
	IOV_LPPHY_VBATSENSE,
	IOV_NPHY_5G_PWRGAIN,
	IOV_NPHY_ACI_SCAN,
	IOV_NPHY_BPHY_EVM,
	IOV_NPHY_BPHY_RFCS,
	IOV_NPHY_CALTXGAIN,
	IOV_NPHY_CAL_RESET,
	IOV_NPHY_CAL_SANITY,
	IOV_NPHY_ELNA_GAIN_CONFIG,
	IOV_NPHY_ENABLERXCORE,
	IOV_NPHY_EST_TONEPWR,
	IOV_NPHY_FORCECAL,		/* depreciated */
	IOV_NPHY_GAIN_BOOST,
	IOV_NPHY_GPIOSEL,
	IOV_NPHY_HPVGA1GAIN,
	IOV_NPHY_INITGAIN,
	IOV_NPHY_PAPDCAL,
	IOV_NPHY_PAPDCALINDEX,
	IOV_NPHY_PAPDCALTYPE,
	IOV_NPHY_PERICAL,		/* depreciated */
	IOV_NPHY_PHYREG_SKIPCNT,
	IOV_NPHY_PHYREG_SKIPDUMP,
	IOV_NPHY_RFSEQ,
	IOV_NPHY_RFSEQ_TXGAIN,
	IOV_NPHY_RSSICAL,
	IOV_NPHY_RSSISEL,
	IOV_NPHY_RXCALPARAMS,
	IOV_NPHY_RXIQCAL,
	IOV_NPHY_SCRAMINIT,
	IOV_NPHY_SKIPPAPD,
	IOV_NPHY_TBLDUMP_MAXIDX,
	IOV_NPHY_TBLDUMP_MINIDX,
	IOV_NPHY_TEMPOFFSET,
	IOV_NPHY_TEMPSENSE,		/* depreciated */
	IOV_NPHY_TEMPTHRESH,
	IOV_NPHY_TEST_TSSI,
	IOV_NPHY_TEST_TSSI_OFFS,
	IOV_NPHY_TXIQLOCAL,
	IOV_NPHY_TXPWRCTRL,		/* depreciated */
	IOV_NPHY_TXPWRINDEX,		/* depreciated */
	IOV_NPHY_TX_TEMP_TONE,
	IOV_NPHY_TX_TONE,
	IOV_NPHY_VCOCAL,
	IOV_NPHY_CCK_PWR_OFFSET,
	IOV_SSLPNPHY_CARRIER_SUPPRESS,
	IOV_SSLPNPHY_CRS,
	IOV_SSLPNPHY_FULLCAL,
	IOV_SSLPNPHY_NOISE_SAMPLES,
	IOV_SSLPNPHY_PAPARAMS,
	IOV_SSLPNPHY_PAPDCAL,
	IOV_SSLPNPHY_PAPD_DEBUG,
	IOV_SSLPNPHY_RXIQCAL,
	IOV_SSLPNPHY_TXIQLOCAL,
	IOV_SSLPNPHY_TXPWRCTRL,		/* depreciated */
	IOV_SSLPNPHY_TXPWRINDEX,	/* depreciated */
	IOV_SSLPNPHY_TX_TONE,
	IOV_SSLPNPHY_UNMOD_RSSI,
	IOV_SSLPNPHY_SPBRUN,
	IOV_SSLPNPHY_SPBDUMP,
	IOV_SSLPNPHY_SPBRANGE,
	IOV_SSLPNPHY_PKTENG_STATS,
	IOV_SSLPNPHY_PKTENG,
	IOV_SSLPNPHY_CGA,
	IOV_SSLPNPHY_TX_IQCC,
	IOV_SSLPNPHY_TX_LOCC,
	IOV_SSLPNPHY_PER_CAL,
	IOV_SSLPNPHY_VCO_CAL,
	IOV_SSLPNPHY_TXPWRINIT,

	/* ==========================================
	 * unified phy iovar, independent of PHYTYPE
	 * ==========================================
	 */
	IOV_PHYHAL_MSG = 300,
	IOV_PHY_WATCHDOG,
	IOV_RADAR_ARGS,
	IOV_RADAR_ARGS_40MHZ,
	IOV_RADAR_THRS,
	IOV_PHY_DFS_LP_BUFFER,
	IOV_FAST_TIMER,
	IOV_SLOW_TIMER,
	IOV_GLACIAL_TIMER,
	IOV_TXINSTPWR,
	IOV_PHY_FIXED_NOISE,
	IOV_PHYNOISE_POLL,
	IOV_PHY_SPURAVOID,
	IOV_CARRIER_SUPPRESS,
	IOV_UNMOD_RSSI,
	IOV_PKTENG_STATS,
	IOV_ACI_EXIT_CHECK_PERIOD,
	IOV_PHY_RXIQ_EST,
	IOV_PHYTABLE,
	IOV_NUM_STREAM,
	IOV_BAND_RANGE,
	IOV_PHYWREG_LIMIT,
	IOV_MIN_TXPOWER,
	IOV_PHY_MUTED,
	IOV_PHY_SAMPLE_COLLECT,
	IOV_PHY_SAMPLE_COLLECT_GAIN_ADJUST,
	IOV_PHY_SAMPLE_DATA,
	IOV_PHY_TXPWRCTRL,
	IOV_PHY_RESETCCA,
	IOV_PHY_GLITCHK,
	IOV_PHY_NOISE_UP,
	IOV_PHY_NOISE_DWN,
	IOV_PHY_TXPWRINDEX,
	IOV_PHY_AVGTSSI_REG,
	IOV_PHY_IDLETSSI_REG,
	IOV_PHY_IDLETSSI,
	IOV_PHY_CAL_DISABLE,
	IOV_PHY_PERICAL,	/* Periodic calibration cofig */
#ifdef DSLCPE_PHYCAL_OVERRIDE_ON_TRAFFIC
	IOV_PHY_WATCHDOG_OVERRIDE_ON_TRAFFIC,
	IOV_PHY_WATCHDOG_TRAFFIC_LIMIT,
#endif /* DSLCPE_PHYCAL_OVERRIDE_ON_TRAFFIC */
	IOV_PHY_FORCECAL,
	IOV_PHY_TXRX_CHAIN,
	IOV_PHY_BPHY_EVM,
	IOV_PHY_BPHY_RFCS,
	IOV_PHY_ENABLERXCORE,
	IOV_PHY_EST_TONEPWR,
	IOV_PHY_GPIOSEL,
	IOV_PHY_5G_PWRGAIN,
	IOV_PHY_RFSEQ,
	IOV_PHY_SCRAMINIT,
	IOV_PHY_TEMPOFFSET,
	IOV_PHY_TEMPTHRESH,
	IOV_PHY_TEMPSENSE,
	IOV_PHY_VBATSENSE,
	IOV_PHY_TEST_TSSI,
	IOV_PHY_TEST_TSSI_OFFS,
	IOV_PHY_TX_TONE,
	IOV_PHY_TX_TONE_HZ,
	IOV_PHY_TX_TONE_STOP,
	IOV_PHY_FEM2G,
	IOV_PHY_FEM5G,
	IOV_PHY_MAXP,
	IOV_PAVARS,
	IOV_POVARS,
	IOV_PHYCAL_STATE,
	IOV_LCNPHY_PAPDEPSTBL,
	IOV_PHY_TXIQCC,
	IOV_PHY_TXLOCC,
	IOV_PHYCAL_TEMPDELTA,
	IOV_PHY_PAPD_DEBUG,
	IOV_PHY_ACTIVECAL,
	IOV_NOISE_MEASURE,
	IOV_NOISE_MEASURE_DISABLE,
	IOV_PHY_PACALIDX0,
	IOV_PHY_PACALIDX1,
	IOV_PHY_IQLOCALIDX,
	IOV_PHY_PACALIDX,
	IOV_PHY_LOWPOWER_BEACON_MODE,
#ifdef WLMEDIA_TXFILTER_OVERRIDE
	IOV_PHY_TXFILTER_SM_OVERRIDE,
#endif
	IOV_TSSICAL_START_IDX,
	IOV_TSSICAL_START,
	IOV_TSSICAL_POWER,
	IOV_TSSICAL_PARAMS,
	IOV_PHY_DEAF,
	IOV_PHY_TSSITXDELAY,
#if defined(WLMEDIA_N2DEV) || defined(WLMEDIA_N2DBG)
	IOV_PHY_RXDESENS,
	IOV_NTD_GDS_LOWTXPWR,
	IOV_PAPDCAL_INDEXDELTA,
#endif
	IOV_LCNPHY_CWTXPWRCTRL,
	IOV_PHYNOISE_SROM,
	IOV_LCNPHY_SAMP_CAP,
	IOV_PHY_SUBBAND5GVER,
	IOV_PHY_FCBSINIT,
	IOV_PHY_FCBS,
	IOV_PHY_CRS_WAR,
	IOV_LCNPHY_RXIQGAIN,
	IOV_LCNPHY_RXIQGSPOWER,
	IOV_LCNPHY_RXIQPOWER,
	IOV_LCNPHY_RXIQSTATUS,
	IOV_LCNPHY_RXIQSTEPS,
	IOV_PHY_TEMPSENSE_OVERRIDE,
	IOV_PHY_TEMP_HYSTERESIS,
	IOV_LCNPHY_TXPWRCLAMP_DIS,
	IOV_LCNPHY_TXPWRCLAMP_OFDM,
	IOV_LCNPHY_TXPWRCLAMP_CCK,
	IOV_LCNPHY_TSSI_MAXPWR,
	IOV_LCNPHY_TSSI_MINPWR,
	IOV_PHY_CGA_5G,
	IOV_PHY_CGA_2G,

	IOV_PHY_FORCE_FDIQI,
	IOV_PHY_FORCE_GAINLEVEL,
	IOV_PHY_BBMULT,
	IOV_LNLDO2,
	IOV_PHY_FORCE_CRSMIN,
	IOV_PHY_BTCOEX_DESENSE,
	IOV_PAVARS2,
	IOV_PHY_BTC_RESTAGE_RXGAIN,
	IOV_ED_THRESH,
#ifdef WL_SARLIMIT
	IOV_PHY_SAR_LIMIT,
	IOV_PHY_TXPWR_CORE,
#endif /* WL_SARLIMIT */
	IOV_EDCRS,
	IOV_LP_MODE,
	IOV_RADIO_PD,
	IOV_PHY_FORCECAL_OBT,
	IOV_PHY_LAST	/* insert before this one */
};

typedef enum  phy_radar_detect_mode {
	RADAR_DETECT_MODE_FCC,
	RADAR_DETECT_MODE_EU,
	RADAR_DETECT_MODE_MAX
} phy_radar_detect_mode_t;

/* forward declaration */
struct rpc_info;
typedef struct shared_phy shared_phy_t;

/* Public phy info structure */
struct phy_pub;

#ifdef WLC_HIGH_ONLY
typedef struct wlc_rpc_phy wlc_phy_t;
#else
typedef struct phy_pub wlc_phy_t;
#endif

typedef struct shared_phy_params {
	void 	*osh;		/* pointer to OS handle */
	si_t	*sih;		/* si handle (cookie for siutils calls) */
	void	*physhim;	/* phy <-> wl shim layer for wlapi */
	uint	unit;		/* device instance number */
	uint	corerev;	/* core revision */
	uint	bustype;	/* SI_BUS, PCI_BUS  */
	uint	buscorerev; 	/* buscore rev */
	char	*vars;		/* phy attach time only copy of vars */
	uint16	vid;		/* vendorid */
	uint16	did;		/* deviceid */
	uint	chip;		/* chip number */
	uint	chiprev;	/* chip revision */
	uint	chippkg;	/* chip package option */
	uint	sromrev;	/* srom revision */
	uint	boardtype;	/* board type */
	uint	boardrev;	/* board revision */
	uint	boardvendor;	/* board vendor */
	uint32	boardflags;	/* board specific flags from srom */
	uint32	boardflags2;	/* more board flags if sromrev >= 4 */
} shared_phy_params_t;

/* parameter structure for wlc_phy_txpower_core_offset_get/set functions */
struct phy_txcore_pwr_offsets {
	int8 offset[PHY_MAX_CORES];	/* quarter dBm signed offset for each chain */
};

#ifdef PPR_API
/* phy_tx_power_t.flags bits */
#define WL_TX_POWER_F_ENABLED	1
#define WL_TX_POWER_F_HW		2
#define WL_TX_POWER_F_MIMO		4
#define WL_TX_POWER_F_SISO		8
#define WL_TX_POWER_F_HT		0x10

typedef struct {
	uint32 flags;
	chanspec_t chanspec;		/* txpwr report for this channel */
	chanspec_t local_chanspec;	/* channel on which we are associated */
	uint8 rf_cores;				/* count of RF Cores being reported */
	uint8 display_core;			/* the displayed core in curpower */
	uint8 est_Pout[4];			/* Latest tx power out estimate per RF chain */
	uint8 est_Pout_act[4]; /* Latest tx power out estimate per RF chain w/o adjustment */
	uint8 est_Pout_cck;			/* Latest CCK tx power out estimate */
	uint8 tx_power_max[4];		/* Maximum target power among all rates */
	uint tx_power_max_rate_ind[4];		/* Index of the rate with the max target power */
	ppr_t *ppr_board_limits;
	ppr_t *ppr_target_powers;
#ifdef WL_SARLIMIT
	int8 SARLIMIT[PHY_MAX_CORES];
#endif
} phy_tx_power_t;
#else
typedef tx_power_t phy_tx_power_t;
#endif /* PPR_API */

/* attach/detach/init/deinit */
extern void wlc_beacon_loss_war_lcnxn(wlc_phy_t *ppi, uint8 enable);

extern void wlc_sslpnphy_noise_measure(wlc_phy_t *ppi);

extern shared_phy_t *wlc_phy_shared_attach(shared_phy_params_t *shp);
extern void  wlc_phy_shared_detach(shared_phy_t *phy_sh);
extern wlc_phy_t *wlc_phy_attach(shared_phy_t *sh, void *regs, int bandtype, char *vars);
extern void  wlc_phy_detach(wlc_phy_t *ppi);

extern bool wlc_phy_get_phyversion(wlc_phy_t *pih, uint16 *phytype, uint16 *phyrev,
	uint16 *radioid, uint16 *radiover);
extern bool wlc_phy_get_encore(wlc_phy_t *pih);
extern uint32 wlc_phy_get_coreflags(wlc_phy_t *pih);

extern void  wlc_phy_hw_clk_state_upd(wlc_phy_t *ppi, bool newstate);
extern void  wlc_phy_hw_state_upd(wlc_phy_t *ppi, bool newstate);
extern void  wlc_phy_init(wlc_phy_t *ppi, chanspec_t chanspec);
extern int32  wlc_phy_watchdog(wlc_phy_t *ppi);
extern int   wlc_phy_down(wlc_phy_t *ppi);
extern uint32 wlc_phy_clk_bwbits(wlc_phy_t *pih);
extern void wlc_phy_cal_init(wlc_phy_t *ppi);
extern void wlc_phy_antsel_init(wlc_phy_t *ppi, bool lut_init);

/* bandwidth/chanspec */
extern void wlc_phy_chanspec_set(wlc_phy_t *ppi, chanspec_t chanspec);
extern chanspec_t wlc_phy_chanspec_get(wlc_phy_t *ppi);
extern void wlc_phy_chanspec_radio_set(wlc_phy_t *ppi, chanspec_t newch);
extern uint16 wlc_phy_bw_state_get(wlc_phy_t *ppi);
extern void wlc_phy_bw_state_set(wlc_phy_t *ppi, uint16 bw);
extern bool wlc_phy_get_filt_war(wlc_phy_t *ppi);
extern void wlc_phy_set_filt_war(wlc_phy_t *ppi, bool filt_war);

/* states sync */
extern void wlc_phy_rssi_compute(wlc_phy_t *pih, void *ctx);
extern void wlc_phy_por_inform(wlc_phy_t *ppi);
extern void wlc_phy_noise_sample_intr(wlc_phy_t *ppi);
extern bool wlc_phy_bist_check_phy(wlc_phy_t *ppi);

extern void wlc_phy_set_deaf(wlc_phy_t *ppi, bool user_flag);
extern void wlc_phy_clear_deaf(wlc_phy_t *ppi, bool user_flag);
#if defined(BCMDBG)
#if defined(DBG_PHY_IOV)
extern int wlc_phydump_reg(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern int wlc_phydump_radioreg(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern int wlc_phydump_tbl1(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern int wlc_phydump_tbl2(wlc_phy_t *ppi, struct bcmstrbuf *b);
#endif
extern void wlc_phydump_phycal(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_aci(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_papd(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_state(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_noise(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_measlo(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_lnagain(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_initgain(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_hpf1tbl(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_lpphytbl0(wlc_phy_t *ppi, struct bcmstrbuf *b);
extern void wlc_phydump_chanest(wlc_phy_t *ppi, struct bcmstrbuf *b);
#ifdef ENABLE_FCBS
extern void wlc_phydump_fcbs(wlc_phy_t *ppi, struct bcmstrbuf *b);
#endif /* ENABLE_FCBS */
extern void wlc_phydump_txv0(wlc_phy_t *ppi, struct bcmstrbuf *b);
#endif 
#ifdef WLTEST
extern void wlc_phydump_ch4rpcal(wlc_phy_t *ppi, struct bcmstrbuf *b);
#endif /* WLTEST */
#if defined(DBG_BCN_LOSS)
extern void wlc_phydump_phycal_rx_min(wlc_phy_t *ppi, struct bcmstrbuf *b);
#endif

extern void wlc_phy_switch_radio(wlc_phy_t *ppi, bool on);
extern void wlc_phy_anacore(wlc_phy_t *ppi, bool on);

/* flow */
extern void wlc_phy_BSSinit(wlc_phy_t *ppi, bool bonlyap, int rssi);
extern int  wlc_phy_ioctl(wlc_phy_t *ppi, int cmd, int len, void *arg, bool *ta_ok);
extern int  wlc_phy_iovar_dispatch(wlc_phy_t *ppi, uint32 id, uint16, void *, uint, void *,
	int, int);

#ifdef ENABLE_FCBS
/* Fast channel/band switch  (FCBS) function prototypes */
extern bool wlc_phy_fcbs_init(wlc_phy_t *ppi, int chanidx);
extern bool wlc_phy_fcbs_uninit(wlc_phy_t *ppi, chanspec_t chanspec);
extern int wlc_phy_fcbs(wlc_phy_t *ppi, int chanidx, bool set);
extern void wlc_phy_fcbs_exit(wlc_phy_t *ppi);
extern bool wlc_phy_fcbs_arm(wlc_phy_t *ppi, chanspec_t chanspec, int chanidx);
#endif /* ENABLE_FCBS */

/* chanspec */
extern void wlc_phy_chanspec_ch14_widefilter_set(wlc_phy_t *ppi, bool wide_filter);
extern void wlc_phy_chanspec_band_validch(wlc_phy_t *ppi, uint band, chanvec_t *channels);
extern chanspec_t wlc_phy_chanspec_band_firstch(wlc_phy_t *ppi, uint band);

#ifdef PPR_API
extern void wlc_phy_txpower_sromlimit(wlc_phy_t *ppi, chanspec_t chanspec,
    uint8 *min_pwr, ppr_t *max_pwr, uint8 core);
#else
extern void wlc_phy_txpower_sromlimit(wlc_phy_t *ppi, uint chan,
	uint8 *_min_, uint8 *_max_, int rate);
#endif
extern void wlc_phy_txpower_sromlimit_max_get(wlc_phy_t *ppi, uint chan,
	uint8 *_max_, uint8 *_min_);
extern void wlc_phy_txpower_boardlimit_band(wlc_phy_t *ppi, uint band, int32 *, int32 *, uint32 *);
#ifdef PPR_API
extern void wlc_phy_txpower_limit_set(wlc_phy_t *ppi, ppr_t* txpwr, chanspec_t chanspec);
#else
extern void wlc_phy_txpower_limit_set(wlc_phy_t *ppi, txppr_t* txpwr, chanspec_t chanspec);
#endif
extern int  wlc_phy_txpower_get(wlc_phy_t *ppi, uint *qdbm, bool *override);

#ifdef PPR_API
extern int  wlc_phy_txpower_set(wlc_phy_t *ppi, uint qdbm, bool override, ppr_t *reg_pwr);
#else
extern int  wlc_phy_txpower_set(wlc_phy_t *ppi, uint qdbm, bool override);
#endif
extern int  wlc_phy_neg_txpower_set(wlc_phy_t *ppi, uint qdbm);
extern bool wlc_phy_txpower_hw_ctrl_get(wlc_phy_t *ppi);
extern void wlc_phy_txpower_hw_ctrl_set(wlc_phy_t *ppi, bool hwpwrctrl);
extern uint8 wlc_phy_txpower_get_target_min(wlc_phy_t *ppi);
extern uint8 wlc_phy_txpower_get_target_max(wlc_phy_t *ppi);
extern bool wlc_phy_txpower_ipa_ison(wlc_phy_t *pih);
extern int  wlc_phy_txpower_core_offset_set(wlc_phy_t *ppi, struct phy_txcore_pwr_offsets *offsets);
extern int  wlc_phy_txpower_core_offset_get(wlc_phy_t *ppi, struct phy_txcore_pwr_offsets *offsets);

extern void wlc_phy_stf_chain_init(wlc_phy_t *pih, uint8 txchain, uint8 rxchain);
extern void wlc_phy_stf_chain_set(wlc_phy_t *pih, uint8 txchain, uint8 rxchain);
extern void wlc_phy_stf_chain_get(wlc_phy_t *pih, uint8 *txchain, uint8 *rxchain);
extern uint8 wlc_phy_rssi_ant_compare(wlc_phy_t *pih);
extern uint8 wlc_phy_stf_chain_active_get(wlc_phy_t *pih);
extern int8 wlc_phy_stf_ssmode_get(wlc_phy_t *pih, chanspec_t chanspec);
extern void wlc_phy_ldpc_override_set(wlc_phy_t *ppi, bool val);

extern void wlc_phy_cal_perical(wlc_phy_t *ppi, uint8 reason);
extern int8 wlc_phy_noise_avg(wlc_phy_t *wpi);
extern void wlc_phy_noise_sample_request_external(wlc_phy_t *ppi);
extern void wlc_phy_interference_set(wlc_phy_t *ppi, bool init);
extern void wlc_phy_acimode_noisemode_reset(wlc_phy_t *ppi, uint channel,
	bool clear_aci_state, bool clear_noise_state, bool disassoc);
extern void wlc_phy_edcrs_lock(wlc_phy_t *pih, bool lock);
extern void wlc_phy_cal_papd_recal(wlc_phy_t *ppi);

/* misc */
extern int8 wlc_phy_preamble_override_get(wlc_phy_t *ppi);
extern void wlc_phy_preamble_override_set(wlc_phy_t *ppi, int8 override);
extern void wlc_phy_ant_rxdiv_set(wlc_phy_t *ppi, uint8 val);
extern bool wlc_phy_ant_rxdiv_get(wlc_phy_t *ppi, uint8 *pval);
extern void wlc_phy_clear_tssi(wlc_phy_t *ppi);
extern void wlc_phy_hold_upd(wlc_phy_t *ppi, mbool id, bool val);
extern bool wlc_phy_ismuted(wlc_phy_t *pih);
extern void wlc_phy_mute_upd(wlc_phy_t *ppi, bool val, mbool flags);
extern bool wlc_phy_get_tempsense_degree(wlc_phy_t *ppi, int8 *pval);

extern void wlc_phy_radar_detect_enable(wlc_phy_t *ppi, bool on);
extern int  wlc_phy_radar_detect_run(wlc_phy_t *ppi);
extern void wlc_phy_radar_detect_mode_set(wlc_phy_t *pih, phy_radar_detect_mode_t mode);

extern void wlc_phy_antsel_type_set(wlc_phy_t *ppi, uint8 antsel_type);

#if defined(PHYCAL_CACHING) || defined(WLMCHAN)
extern int  wlc_phy_cal_cache_init(wlc_phy_t *ppi);
extern void wlc_phy_cal_cache_deinit(wlc_phy_t *ppi);
extern int wlc_phy_create_chanctx(wlc_phy_t *ppi, chanspec_t chanspec);
extern void wlc_phy_destroy_chanctx(wlc_phy_t *ppi, chanspec_t chanspec);
extern int wlc_phy_invalidate_chanctx(wlc_phy_t *ppi, chanspec_t chanspec);
extern int wlc_phy_reuse_chanctx(wlc_phy_t *ppi, chanspec_t chanspec);
extern void wlc_phy_get_cachedchans(wlc_phy_t *ppi, chanspec_t *chanlist);
extern int32 wlc_phy_get_est_chanset_time(wlc_phy_t *ppi, chanspec_t chanspec);
extern bool wlc_phy_chan_iscached(wlc_phy_t *ppi, chanspec_t chanspec);
extern void wlc_phy_set_est_chanset_time(wlc_phy_t *ppi, chanspec_t chanspec,
	bool wascached,	bool inband, int32 rectime);
extern int8 wlc_phy_get_max_cachedchans(wlc_phy_t *ppi);
#endif

#if defined(PHYCAL_CACHING) || defined(PHYCAL_CACHE_SMALL)
extern void wlc_phy_cal_cache_set(wlc_phy_t *ppi, bool state);
extern bool wlc_phy_cal_cache_get(wlc_phy_t *ppi);
#endif

#ifdef WLTEST
extern void wlc_phy_boardflag_upd(wlc_phy_t *phi);
#endif
#if defined(WLTEST)
extern void wlc_phy_resetcntrl_regwrite(wlc_phy_t *phi);
#endif

#ifdef PPR_API
extern void wlc_phy_txpower_get_current(wlc_phy_t *ppi, ppr_t *reg_pwr, phy_tx_power_t *power);
#else
extern void wlc_phy_txpower_get_current(wlc_phy_t *ppi, phy_tx_power_t *power, uint channel);
#endif

#ifdef BCMDBG
#ifdef PPR_API
extern void wlc_phy_txpower_limits_dump(ppr_t* txpwr, bool isht);
#else
extern void wlc_phy_txpower_limits_dump(txppr_t *txpwr, bool isht);
#endif
#endif /* BCM_DBG */

extern void wlc_phy_initcal_enable(wlc_phy_t *pih, bool initcal);
extern bool wlc_phy_test_ison(wlc_phy_t *ppi);
extern void wlc_phy_txpwr_percent_set(wlc_phy_t *ppi, uint8 txpwr_percent);
extern void wlc_phy_ofdm_rateset_war(wlc_phy_t *pih, bool war);
extern void wlc_phy_bf_preempt_enable(wlc_phy_t *pih, bool bf_preempt);
extern void wlc_phy_machwcap_set(wlc_phy_t *ppi, uint32 machwcap);

extern void wlc_phy_runbist_config(wlc_phy_t *ppi, bool start_end);

/* old GPHY stuff */
extern void wlc_phy_freqtrack_start(wlc_phy_t *ppi);
extern void wlc_phy_freqtrack_end(wlc_phy_t *ppi);

extern const uint8 * wlc_phy_get_ofdm_rate_lookup(void);

extern void wlc_phy_tkip_rifs_war(wlc_phy_t *ppi, uint8 rifs);

extern int8 wlc_phy_get_tx_power_offset_by_mcs(wlc_phy_t *ppi, uint8 mcs_offset);
extern int8 wlc_phy_get_tx_power_offset(wlc_phy_t *ppi, uint8 tbl_offset);
extern void wlc_phy_btclock_war(wlc_phy_t *ppi, bool enable);
extern uint32 wlc_phy_cap_get(wlc_phy_t *pi);
extern void wlc_phy_tx_pwr_limit_check(wlc_phy_t *pih);
#if defined(WLC_LOWPOWER_BEACON_MODE)
extern void wlc_phy_lowpower_beacon_mode(wlc_phy_t *pih, int lowpower_beacon_mode);
#endif /* WLC_LOWPOWER_BEACON_MODE */
#if defined(WLMEDIA_N2DEV) || defined(WLMEDIA_N2DBG)
extern int wlc_nphy_get_rxdesens(wlc_phy_t *ppi, int32 *ret_int_ptr);
extern int wlc_nphy_set_rxdesens(wlc_phy_t *ppi, int32 int_val);
extern int wlc_nphy_get_lowtxpwr(wlc_phy_t *ppi, int32 *ret_int_ptr);
extern int wlc_nphy_set_lowtxpwr(wlc_phy_t *ppi, int32 int_val);
#endif

#ifdef WLTEST
extern void wlc_lcnphy_iovar_samp_cap(wlc_phy_t *ppi, int32 gain, int32 *ret);
extern void wlc_phy_pkteng_boostackpwr(wlc_phy_t *ppi);
#endif

#ifdef WLSRVSDB
#define CHANNEL_UNDEFINED       0xEF
#define SR_MEMORY_BANK	2

extern void wlc_phy_force_vsdb_chans(wlc_phy_t *pi, uint16 * chans, uint8 set);
extern void wlc_phy_detach_srvsdb_module(wlc_phy_t *pi);
extern uint8 wlc_phy_attach_srvsdb_module(wlc_phy_t *ppi, chanspec_t chan0, chanspec_t chan1);
extern void wlc_phy_chanspec_shm_set_vsdb(wlc_phy_t *pi, chanspec_t chanspec);
extern uint8 wlc_set_chanspec_sr_vsdb(wlc_phy_t *pi, chanspec_t chanspec, uint8 * last_chan_saved);
#endif /* end WLSRVSDB */

#ifdef BCMDBG
extern void wlc_acphy_txerr_dump(uint32 PhyErr);
#endif

#ifdef WL_LPC
/* Remove the following #defs once branches have been updated */
#define wlc_phy_pwrsel_getminidx	wlc_phy_lpc_getminidx
#define wlc_phy_pwrsel_getoffset	wlc_phy_lpc_getoffset
#define wlc_phy_pwrsel_get_txcpwrval	wlc_phy_lpc_get_txcpwrval
#define wlc_phy_pwrsel_set_txcpwrval	wlc_phy_lpc_set_txcpwrval
#define wlc_phy_powersel_algo_set	wlc_phy_lpc_algo_set

uint8 wlc_phy_lpc_getminidx(wlc_phy_t *ppi);
uint8 wlc_phy_lpc_getoffset(wlc_phy_t *ppi, uint8 index);
uint8 wlc_phy_lpc_get_txcpwrval(wlc_phy_t *ppi, uint16 phytxctrlword);
void wlc_phy_lpc_set_txcpwrval(wlc_phy_t *ppi, uint16 *phytxctrlword, uint8 txcpwrval);
void wlc_phy_lpc_algo_set(wlc_phy_t *ppi, bool enable);
#ifdef WL_LPC_DEBUG
uint8 * wlc_phy_lpc_get_pwrlevelptr(wlc_phy_t *ppi);
#endif
#endif /* WL_LPC */
#ifdef DSLCPE_C601911
extern void wlc_phy_need_reinit(wlc_phy_t *pih);
#endif
extern void wlc_phy_block_bbpll_change(wlc_phy_t *pih, bool block, bool going_down);
extern void wlc_phy_interf_rssi_update(wlc_phy_t *pi, chanspec_t chanNum, int8 leastRSSI);

#endif	/* _wlc_phy_h_ */
