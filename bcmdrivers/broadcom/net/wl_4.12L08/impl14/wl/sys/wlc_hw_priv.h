/*
 * Private H/W info of
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

#ifndef _wlc_hw_priv_h_
#define _wlc_hw_priv_h_

/* P2P ucode Support */
#ifdef WLP2P_UCODE
	#if !defined(WLP2P_UCODE_ONLY) || defined(WL_ENAB_RUNTIME_CHECK)
		#define DL_P2P_UC(wlc_hw)	((wlc_hw)->_p2p)
	#else /* WLP2P_UCODE_ONLY */
		#define DL_P2P_UC(wlc_hw)	1
	#endif /* WLP2P_UCODE_ONLY */
#else /* !WLP2P_UCODE */
	#define DL_P2P_UC(wlc_hw)	0
#endif /* !WLP2P_UCODE */

/* Interrupt bit error summary.  Don't include I_RU: we refill DMA at other
 * times; and if we run out, constant I_RU interrupts may cause lockup.  We
 * will still get error counts from rx0ovfl.
 */
#define	I_ERRORS	(I_PC | I_PD | I_DE | I_RO | I_XU)
/* default software intmasks */
#define	DEF_RXINTMASK	(I_RI)	/* enable rx int on rxfifo only */
#define	DEF_MACINTMASK	(MI_TXSTOP | MI_ATIMWINEND | MI_PMQ | \
			 MI_PHYTXERR | MI_DMAINT | MI_TFS | MI_BG_NOISE | \
			 MI_CCA | MI_TO | MI_GP0 | MI_RFDISABLE | MI_PWRUP | \
			 MI_BT_RFACT_STUCK | MI_BT_PRED_REQ)

/* Below interrupts can be delayed and piggybacked with other interrupts.
 * We don't want these interrupts to trigger an isr so we can save CPU & power.
 * They are not enabled, but handled when set in interrupt status
 */
#define	DELAYEDINTMASK  (0 /* | MI_BG_NOISE */)

/* rx interrupts default to 1/frame */
#ifndef WLC_INTRCVLAZY_DEFAULT
#define WLC_INTRCVLAZY_FRAME_CNT        1
#define WLC_INTRCVLAZY_USEC_CNT         0
#define WLC_INTRCVLAZY_DEFAULT ((WLC_INTRCVLAZY_FRAME_CNT << IRL_FC_SHIFT) | \
	                            WLC_INTRCVLAZY_USEC_CNT)
#endif

#ifdef WLC_LOW
typedef struct wlc_hwband {
	int		bandtype;		/* WLC_BAND_2G, WLC_BAND_5G */
	uint		bandunit;		/* bandstate[] index */
	uint16		mhfs[MHFMAX];		/* MHF array shadow */
	uint8		bandhw_stf_ss_mode;	/* HW configured STF type, 0:siso; 1:cdd */
	uint16		CWmin;
	uint16		CWmax;
	uint32          core_flags;

	uint16		phytype;		/* phytype */
	uint16		phyrev;
	uint16		radioid;
	uint16		radiorev;
	wlc_phy_t	*pi;			/* pointer to phy specific information */
	bool		abgphy_encore;
} wlc_hwband_t;

typedef struct wlc_hw_btc_info {
	int		mode;		/* Bluetooth Coexistence mode */
	int		wire;		/* BTC: 2-wire or 3-wire */
	uint16		flags;		/* BTC configurations */
	uint32		gpio_mask;	/* Resolved GPIO Mask */
	uint32		gpio_out;	/* Resolved GPIO out pins */
	bool		stuck_detected;	/* stuck BT_RFACT has been detected */
	bool		stuck_war50943;
	bool		bt_active;		/* bt is in active session */
	uint8		bt_active_asserted_cnt;	/* 1st bt_active assertion */
	uint8		btcx_aa;		/* a copy of aa2g:aa5g */
	uint16		bt_shm_addr;
	uint16		bt_period;
} wlc_hw_btc_info_t;

#if defined(BCMDBG)
typedef struct bmac_suspend_stats {
	uint32 suspend_count;
	uint32 suspended;
	uint32 unsuspended;
	uint32 suspend_start;
	uint32 suspend_end;
	uint32 suspend_max;
} bmac_suspend_stats_t;
#endif 
#endif /* WLC_LOW */

struct wlc_hw_info {
	wlc_info_t	*wlc;
	wlc_hw_t	*pub;			/* public API */
	osl_t		*osh;			/* pointer to os handle */
	uint		unit;			/* device instance number */

#ifdef WLC_SPLIT
	rpc_info_t	*rpc;			/* Handle to RPC module */
#endif

	bool		_piomode;		/* true if pio mode */
	bool		_p2p;			/* download p2p ucode */

	/* fifo info shadowed in wlc_hw_t.
	 * These data are "read" directly but and shall be modified via the following APIs:
	 * - wlc_hw_set_di()
	 * - wlc_hw_set_pio()
	 */
	hnddma_t	*di[NFIFO];		/* hnddma handles, per fifo */
	pio_t		*pio[NFIFO];		/* pio handlers, per fifo */

#ifdef WLC_LOW
	/* version info */
	uint16		vendorid;		/* PCI vendor id */
	uint16		deviceid;		/* PCI device id */
	uint		corerev;		/* core revision */
	uint8		sromrev;		/* version # of the srom */
	uint16		boardrev;		/* version # of particular board */
	uint32		boardflags;		/* Board specific flags from srom */
	uint32		boardflags2;		/* More board flags if sromrev >= 4 */

	/* interrupt */
	uint32		macintstatus;		/* bit channel between isr and dpc */
	uint32		macintmask;		/* sw runtime master macintmask value */
	uint32		defmacintmask;		/* default "on" macintmask value */
	uint32		tbttenablemask;		/* mask of tbtt interrupt clients */

	uint32		machwcap;		/* MAC capabilities (corerev >= 13) */
	uint32		machwcap_backup;	/* backup of machwcap (corerev >= 13) */
	uint16		ucode_dbgsel;		/* dbgsel for ucode debug(config gpio) */
	bool		ucode_loaded;		/* TRUE after ucode downloaded */

	wlc_hw_btc_info_t *btc;

	si_t		*sih;			/* SB handle (cookie for siutils calls) */
	char		*vars;			/* "environment" name=value */
	uint		vars_size;		/* size of vars, free vars on detach */
	d11regs_t	*regs;			/* pointer to device registers */
	void		*physhim;		/* phy shim layer handler */
	void		*phy_sh;		/* pointer to shared phy state */
	wlc_hwband_t	*band;			/* pointer to active per-band state */
	wlc_hwband_t	*bandstate[MAXBANDS];	/* per-band state (one per phy/radio) */
	uint16		bmac_phytxant;		/* cache of high phytxant state */
	bool		shortslot;		/* currently using 11g ShortSlot timing */
	uint16		SRL;			/* 802.11 dot11ShortRetryLimit */
	uint16		LRL;			/* 802.11 dot11LongRetryLimit */
	uint16		SFBL;			/* Short Frame Rate Fallback Limit */
	uint16		LFBL;			/* Long Frame Rate Fallback Limit */

	bool		up;			/* d11 hardware up and running */
	uint		now;			/* # elapsed seconds */
	uint		phyerr_cnt;		/* # continuous TXPHYERR counts */
	uint		_nbands;		/* # bands supported */
	chanspec_t	chanspec;		/* bmac chanspec shadow */

	uint		*txavail[NFIFO];	/* # tx descriptors available */
	uint16		*xmtfifo_sz;		/* fifo size in 256B for each xmt fifo */
	uint16		xmtfifo_frmmax[AC_COUNT];	/* max # of frames fifo size can hold */

	mbool		pllreq;			/* pll requests to keep PLL on */

	uint8		suspended_fifos;	/* Which TX fifo to remain awake for */
	uint32		maccontrol;		/* Cached value of maccontrol */
	uint		mac_suspend_depth;	/* current depth of mac_suspend levels */
	uint32		wake_override;		/* Various conditions to force MAC to WAKE mode */
	uint32		mute_override;		/* Prevent ucode from sending beacons */
	struct		ether_addr etheraddr;	/* currently configured ethernet address */
	uint32		led_gpio_mask;		/* LED GPIO Mask */
	bool		noreset;		/* true= do not reset hw, used by WLC_OUT */
	bool		forcefastclk;		/* true if the h/w is forcing the use of fast clk */
	bool		clk;			/* core is out of reset and has clock */
	bool		sbclk;			/* sb has clock */
	bmac_pmq_t	*bmac_pmq;		/*  bmac PM states derived from ucode PMQ */
	bool		phyclk;			/* phy is out of reset and has clock */
	bool		dma_lpbk;		/* core is in DMA loopback */
	uint16		fastpwrup_dly;		/* time in us needed to bring up d11 fast clock */

#ifdef WLLED
	bmac_led_info_t	*ledh;			/* pointer to led specific information */
#endif
#if defined(BCMDBG)
	bmac_suspend_stats_t* suspend_stats;	/* pointer to stats tracking track bmac suspend */
#endif
#ifdef WLC_LOW_ONLY
	struct wl_timer *wdtimer;		/* timer for watchdog routine */
	struct ether_addr orig_etheraddr;	/* original hw ethernet address */
	struct wl_timer *rpc_agg_wdtimer;	/* rpc agg timer */
	uint16		rpc_dngl_agg;		/* rpc agg control for dongle */
#ifdef DMA_TX_FREE
	wlc_txstatus_flags_t txstatus_ampdu_flags[NFIFO];
#endif
#ifdef WLEXTLOG
	wlc_extlog_info_t *extlog;		/* external log handle */
#endif
	uint32		mem_required_def;	/* memory required to replenish RX DMA ring */
	uint32		mem_required_lower;	/* memory required with lower RX bound */
	uint32		mem_required_least;	/* minimum memory requirement to handle RX */

#endif	/* WLC_LOW_ONLY */

	uint8		hw_stf_ss_opmode;	/* STF single stream operation mode */
	uint8		antsel_type;		/* Type of boardlevel mimo antenna switch-logic
						 * 0 = N/A, 1 = 2x4 board, 2 = 2x3 CB2 board
						 */
	uint32		antsel_avail;		/* put antsel_info_t here if more info is needed */

	bool		btclock_tune_war;	/* workaoround to stablilize bt clock  */
	uint16		noise_metric;		/* To enable/disable knoise measurement. */

	uint32		intrcvlazy;		/* D11 core INTRCVLAZY register setting */
	uint16		p2p_shm_base;		/* p2p SHM base offset in byte */
#endif /* WLC_LOW */

	/* MHF2_SKIP_ADJTSF ucode host flag manipulation */
	uint32		skip_adjtsf;	/* bitvec, IDs of users requesting to skip ucode TSF adj. */
	/* MCTL_AP maccontrol register bit manipulation when AP_ACTIVE() */
	uint32		mute_ap;	/* bitvec, IDs of users requesting to stop the AP func. */

	/* variables to store BT switch state/override settings */
	int8   btswitch_ovrd_state;
	uint8	antswctl2g;	/* extlna switch control read from SROM (2G) */
	uint8	antswctl5g;	/* extlna switch control read from SROM (5G) */
	uint32	vcoFreq_4360_pcie2_war; /* changing the avb vcoFreq */
#ifdef	WL_RXBUFF_EARLY_RC
	void *rc_pkt_head;
#endif
};

#endif /* !_wlc_hw_priv_h_ */
