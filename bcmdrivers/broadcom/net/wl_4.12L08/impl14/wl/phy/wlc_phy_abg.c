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
 * $Id: wlc_phy_abg.c 365817 2012-10-31 03:42:31Z $
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
#include <bcmsrom_fmt.h>

#include <wlc_phy_hal.h>
#include <wlc_phy_int.h>
#include <wlc_phy_abg.h>
#include <wlc_phyreg_abg.h>

/* Max Radio tx attenuation settings */
#define MAX_TX_ATTN_2050	9	/* 2050 */
#define MAX_TX_ATTN_2060	4	/* 2060 */
#define MAX_TX_ATTN_4318	0x1f	/* 4318 == radio rev 8 */

#define BB_TX_ATTEN_MIN	1 /* Minimum Baseband Attenuation */
#define BB_TX_ATTEN_MAX	5 /* Maximum Baseband Attenuation */

/* Max BB tx attenuation settings */
#define MAX_TX_ATTN		11	/* All analog FE revs */

/* Default tx attenuation settings */
#define DEF_TX_ATTN		2	/* Tx Attenuation setting */

/* Read/Write baseband attenuation */
#define	BBATT_11G_MASK2		0x3c	/* > ANA_11G_018 */
#define	BBATT_11G_SHIFT2	2       /* Shift 2 bits */
#define	BBATT_11G_MASK		0x78	/* ANA_11G_018 */
#define	BBATT_11G_SHIFT		3	/* Shift 3 bits */
#define	BBATT_11B_MASK		0xf	/* ANA_43XX */

#define NUM_SNAPS	20 /* Number of snapshots to be taken */
#define TEMP_WAIT_MAX	10 /* Maximum iterations to attempt to read APHY_TEMP_STAT */

#define MAX_DAC		11 /* Maximum DAC value for bbpga_lut table */
#define MAX_BBPGA	6  /* Max BBPGA value for bbpga_lut table */

/* LO FT cancellation via radio reg RF_tx_vos_ctl.  Iterate over ALL 256 tx_vos_ctl settings. */
#define	LOF_CAL_ALL_VOS	0 /* LO FT All VOS setting */
#define	LOF_CAL_TX_BB	1	/* LO FT cancellation via tx baseband dc bias register */
/* LO FT cancellation via radio reg RF_tx_vos_ctl. Iterate over a smaller subset of tx_vos_ctl */
#define	LOF_CAL_SOME_VOS	2 /* LO FT some VOS */
/* LO FT cancellation - search over specific tx_vos_ctl settings
 * followed by search over tx baseband dc bias register
 */
#define	LOF_CAL_VOS_AND_TX_BB	3 /* LO FT TX BB setting */
#define	MAX_VOS_COUNT		32	/* Max size for loft arrays */
#define MAX_BB_TX_DC		0x0f	/* Max DC voltage bias */
#define N_LOFTMIN		15	/* min for iVos */
#define	N_IQ_ITER 		2	/* Number of iterations in computation */

#define TXC1_5DB (TXC1_PA_GAIN_2DB | TXC1_TX_MIX_GAIN)

#define SLOPE_DELAY	10	/* # seconds offset between nrssislope and nrssioffset */

/* gphy LO computation */
#define INFINITY 0x7fffffff /* Max value for gphy LO computation */
#define NUM_NEIGHBORS 8 /* 8 positions NE, E, SE, S, SW, W, NW, N. NE == 0 */

/* The BB DC offset search boundaries are set to +/-16, but later
 * if HW_PWRCTRL_ENABLED, the final biases must be limited to -8 to +7
 * They will be saturated when downloading the values to phy registers.
 */
#define MAX_IQ_DC 16 /* BB DC offset search boundary */

/* For Periodic re-cal of baseband lo comp DC values, accum values should be low,
 * so check them against THDs to see if loopback rx gain should be increased
 */
#define INC_BY6DB_THD	600 /* 6dB increment for THD */
#define INC_BY12DB_THD	300 /* 12dB increment for THD */

#define TX_GAIN_TABLE_LENGTH    64      /* Table size for gain_table */

/* channel info structure */
typedef struct _chan_info_abgphy {
	uint16	freq;		/* in Mhz */
	uint8	chan;		/* channel number */
	uint8	radiocode;	/* radio-chip-specific code for this channel */
	uint8	pdiv;		/* pdiv for 2060WW */
	uint8	sdiv;		/* sdiv for 2060WW */
	uint8	cal_val;	/* cal_val for 2060WW */
	uint8	rxiqidx:4;	/* Index into rx iq compensation table for 2060WW */
	uint8	txiqidx:4;	/* Index into tx iq compensation table for 2060WW */
	uint8	hwtxpwr;	/* Hardware limit for TX power for 2060WW */
	int8	pwr_est_delta;	/* Board dep output power estimate adj. */
} chan_info_abgphy_t;

static chan_info_abgphy_t chan_info_abgphy[] = {
	/* 11b/11g */
/* 0 */		{2412,	1,	12,	0,	0,	0,	0,	0,	0,	0},
/* 1 */		{2417,	2,	17,	0,	0,	0,	0,	0,	0,	0},
/* 2 */		{2422,	3,	22,	0,	0,	0,	0,	0,	0,	0},
/* 3 */		{2427,	4,	27,	0,	0,	0,	0,	0,	0,	0},
/* 4 */		{2432,	5,	32,	0,	0,	0,	0,	0,	0,	0},
/* 5 */		{2437,	6,	37,	0,	0,	0,	0,	0,	0,	0},
/* 6 */		{2442,	7,	42,	0,	0,	0,	0,	0,	0,	0},
/* 7 */		{2447,	8,	47,	0,	0,	0,	0,	0,	0,	0},
/* 8 */		{2452,	9,	52,	0,	0,	0,	0,	0,	0,	0},
/* 9 */		{2457,	10,	57,	0,	0,	0,	0,	0,	0,	0},
/* 10 */	{2462,	11,	62,	0,	0,	0,	0,	0,	0,	0},
/* 11 */	{2467,	12,	67,	0,	0,	0,	0,	0,	0,	0},
/* 12 */	{2472,	13,	72,	0,	0,	0,	0,	0,	0,	0},
/* 13 */	{2484,	14,	84,	0,	0,	0,	0,	0,	0,	0},

#ifdef BAND5G
/* 11a japan high */
/* The 0x80 bit in pdiv means these are REF5, other entries are REF20 */
/* 14 */	{5170,	34,	0, 0x80 + 65,	27,	0x17,	0,	0,	56,	0},
/* 15 */	{5190,	38,	0, 0x80 + 65,	31,	0x17,	0,	0,	56,	0},
/* 16 */	{5210,	42,	0, 0x80 + 65,	35,	0x17,	0,	0,	56,	0},
/* 17 */	{5230,	46,	0, 0x80 + 65,	39,	0x17,	0,	0,	56,	0},

/* 11a usa low */
/* 18 */	{5180,	36,	0,	15,	2,	0x17,	0,	0,	56,	0},
/* 19 */	{5200,	40,	0,	15,	3,	0x17,	0,	0,	56,	0},
/* 20 */	{5220,	44,	0,	15,	4,	0x17,	0,	0,	56,	0},
/* 21 */	{5240,	48,	0,	15,	5,	0x17,	0,	0,	56,	0},
/* 22 */	{5260,	52,	0,	15,	6,	0x16,	0,	0,	56,	0},
/* 23 */	{5280,	56,	0,	15,	7,	0x16,	0,	0,	56,	0},
/* 24 */	{5300,	60,	0,	15,	8,	0x16,	0,	0,	56,	0},
/* 25 */	{5320,	64,	0,	15,	9,	0x16,	0,	0,	56,	0},

/* 11a Europe */
/* 26 */	{5500,	100,	0,	16,	3,	0x99,	1,	1,	52,	2},
/* 27 */	{5520,	104,	0,	16,	4,	0x99,	1,	1,	52,	2},
/* 28 */	{5540,	108,	0,	16,	5,	0x99,	1,	1,	52,	2},
/* 29 */	{5560,	112,	0,	16,	6,	0x98,	1,	1,	52,	2},
/* 30 */	{5580,	116,	0,	16,	7,	0x98,	1,	1,	52,	2},
/* 31 */	{5600,	120,	0,	16,	8,	0x98,	1,	1,	52,	2},
/* 32 */	{5620,	124,	0,	16,	9,	0x97,	1,	1,	52,	2},
/* 33 */	{5640,	128,	0,	16,	10,	0x97,	1,	1,	52,	2},
/* 34 */	{5660,	132,	0,	16,	11,	0x97,	1,	0,	52,	2},
/* 35 */	{5680,	136,	0,	16,	12,	0x96,	1,	0,	52,	2},
/* 36 */	{5700,	140,	0,	16,	13,	0x96,	1,	0,	52,	2},

/* 11a usa high, ref5 only */
/* The 0x80 bit in pdiv means these are REF5, other entries are REF20 */
/* 37 */	{5745,	149,	0, 0x80 + 73,	22,	0x95,	1,	0,	52,	2},
/* 38 */	{5765,	153,	0, 0x80 + 73,	26,	0x95,	4,	0,	52,	2},
/* 39 */	{5785,	157,	0, 0x80 + 73,	30,	0x95,	4,	0,	52,	2},
/* 40 */	{5805,	161,	0, 0x80 + 73,	34,	0x95,	4,	0,	52,	2},
/* 41 */	{5825,	165,	0, 0x80 + 73,	38,	0x95,	4,	0,	52,	2},

/* 11a japan */
/* 42 */	{4920,	184,	0,	14,	4,	0x57,	2,	1,	56,	0},
/* 43 */	{4940,	188,	0,	14,	5,	0x57,	2,	1,	56,	0},
/* 44 */	{4960,	192,	0,	14,	6,	0x57,	2,	1,	56,	0},
/* 45 */	{4980,	196,	0,	14,	7,	0x56,	2,	1,	56,	0},
/* 46 */	{5000,	200,	0,	14,	8,	0x56,	2,	1,	56,	0},
/* 47 */	{5020,	204,	0,	14,	9,	0x56,	2,	1,	56,	0},
/* 48 */	{5040,	208,	0,	14,	10,	0x55,	3,	1,	56,	0},
/* 49 */	{5060,	212,	0,	14,	11,	0x55,	3,	1,	56,	0},
/* 50 */	{5080,	216,	0,	14,	12,	0x55,	3,	1,	56,	0}
#endif /* BAND5G */
};

/* tcl starts BBPGA index at 1, not 0 */
static uint8 bbpga_lut[MAX_DAC+1][MAX_BBPGA] = {
/* bbpga 1    2    3  4    5   6 */
/* 0 */ {0, 119, 86, 63, 46, 34},
/* 1 */ {0, 126, 91, 66, 49, 36},
/* 2 */ {0, 134, 97, 70, 51, 38},
/* 3 */ {0, 142, 103, 74, 54, 40},
/* 4 */ {0, 150, 109, 79, 58, 43},
/* 5 */ {0, 159, 115, 83, 61, 45},
/* 6 */ {0, 168, 122, 88, 65, 48},
/* 7 */ {0, 178, 129, 94, 69, 51},
/* 8 */ {0, 189, 137, 99, 73, 54},
/* 9 */ {0, 200, 145, 105, 77, 57},
/* 10 */{0, 212, 154, 111, 82, 60},
/* 11 */{0, 224, 163, 118, 86, 64}
};

static const uint16 rxiqcomp[5] = {0xefc3, 0xefc2, 0xf1c2, 0xf0c2, 0xf0c1};
static const uint16 txiqcomp[2] = {0x0300, 0x0400};


static void wlc_bphy6_init(phy_info_t *pi);
static void wlc_bphy5_init(phy_info_t *pi);
static void wlc_aphy_lof_cal(phy_info_t *pi, int cal_type);


static void aphy_crs0(phy_info_t *pi, bool on);
static void aphy_all_crs(phy_info_t *pi, bool on);
static void aphy_init_gain(phy_info_t *pi, uchar lna, uchar hpf1, uchar lpf, uchar hpf2,
                                    uchar hpf3, uchar lnaatten);
static void aphy_set_tx_iq_based_on_vos(phy_info_t *pi, bool iq_rebal_flag);
static void aphy_rssiadc_war(phy_info_t *pi);
static void gphy_complo(phy_info_t *pi, int rf_attn, int bb_attn, uint16 txctl1);
static void gphy_tr_switch(phy_info_t *pi, d11regs_t *regs, int tr, bool n1n2no6);
static void gphy_classifyctl(phy_info_t *pi, bool cck, bool ofdm);
static void gphy_all_gains(phy_info_t *pi, uint16 lna, uint16 pga, int tr, bool issue_tx);
static void gphy_orig_gains(phy_info_t *pi, bool issue_tx);
static void gphy_update_nrssi_tbl(phy_info_t *pi, int nrssi_table_delta);
static int8 wlc_phy_compute_est_pout_abgphy(phy_info_t *pi, chan_info_abgphy_t *ci, int8 ave_tssi);
static int gen_tssi_to_pwr_tbl(phy_info_t *pi, int8 *tssi_to_pwr_tbl, int table_size,
                               int16 b0, int16 b1, int16 a1, int idle_tssi);
static int wlc_phy_gen_tssi_tables(phy_info_t *pi, char cg, int16 txpa[], int8 itssit, uint8 txpwr);
static void wlc_gphy_dc_lut(phy_info_t *pi);
static void wlc_ucode_to_bb_rf(phy_info_t *pi, int indx, uint16 *bb, uint16 *rf, uint16 *pmix_en);


static void wlc_bgphy_pwrctrl_init(phy_info_t *pi);
static void wlc_g_hw_pwr_ctrl_init(phy_info_t *pi);
static void wlc_a_hw_pwr_ctrl_init(phy_info_t *pi);
static void wlc_aphy_pwrctrl_init(phy_info_t *pi);
static void wlc_set_11a_tpi_set(phy_info_t *pi, int8 tpi);
static uint16 wlc_aphypwr_get_dc_bias(phy_info_t *pi, int dig, int txbb);

static void wlc_aphypwr_one(phy_info_t *pi, bool enable);
static void wlc_aphypwr_three(phy_info_t *pi);
static void wlc_aphypwr_four(phy_info_t *pi);
static void wlc_aphypwr_five(phy_info_t *pi);
static void wlc_aphypwr_six(phy_info_t *pi);
static void wlc_aphypwr_seven(phy_info_t *pi);
static void wlc_aphypwr_nine(phy_info_t *pi, bool enable);

static void wlc_aphypwr_set_tssipwr_LUT(phy_info_t *pi, uint chan);
#if GCONF
static void wlc_phy_txpwrctrl_hw_gphy(phy_info_t *pi, bool enable);
#endif

#if ACONF
#if defined(AP) && defined(RADAR)
static void wlc_radar_attach_aphy(phy_info_t *pi);
#endif
static uint16 wlc_add_iq_comp_delta(uint16 iq_comp_val, uint16 iq_comp_delta);
static void wlc_phy_txpwrctrl_hw_aphy(phy_info_t *pi, bool enable);
#endif /* ACONF */

static void wlc_set_11b_bphy_dac_ctrl_set(phy_info_t *pi, uint16 dacatten);
static void wlc_set_11b_txpower(phy_info_t *pi, uint16 bbatten, uint16 radiopwr, uint16 txctl1);
static bool wlc_txpower_11a_compute_est_Pout(phy_info_t *pi);
static bool wlc_txpower_11a_recalc(phy_info_t *pi);
static bool wlc_txpower_11b_compute_est_Pout(phy_info_t *pi);
static bool wlc_txpower_11b_recalc(phy_info_t *pi);
static void wlc_txpower_11b_inc(phy_info_t *pi, uint16 bbatten, uint16 radiopwr, uint16 txctl1);

static uint16 wlc_cal_2050_rc_init(phy_info_t *pi, uint16 highfreq);
static void wlc_phy_gphy_find_lpback_gain(phy_info_t *pi);
static void wlc_cal_2050_nrssislope_gmode1(phy_info_t *pi);
static void wlc_nrssi_tbl_gphy_mod(phy_info_t *pi);
static void wlc_nrssi_thresh_gphy_set(phy_info_t *pi);
static void wlc_radio_2060_init(phy_info_t *pi);
static void wlc_phy_cal_gphy_measurelo(phy_info_t *pi);
static bool wlc_aci_detect_rssi_power(void *cpi, bool wrssi, int fail_thresh, int channel);
static void bgphy_set_bbattn(phy_info_t *pi, int bbatten);
static int  bgphy_get_bbattn(phy_info_t *pi);
static void write_gphy_table_ent(phy_info_t *pi, int table, int offset, uint16 data);
static uint16 read_gphy_table_ent(phy_info_t *pi, int table, int offset);
static void WRITE_APHY_TABLE_ENT(phy_info_t *pi, int table, int offset, uint16 data);
static uint16 read_aphy_table_ent(phy_info_t *pi, int table, int offset);

#if defined(BCMDBG) || defined(WLTEST)
static void wlc_pa_on(phy_info_t *pi, bool control);
#endif

void
write_gphy_table_ent(phy_info_t *pi, int table, int offset, uint16 data)
{
	phy_reg_write(pi, GPHY_TABLE_ADDR, (((table) << 10) | (offset)));
	phy_reg_write(pi, GPHY_TABLE_DATA, (data));
}

uint16
read_gphy_table_ent(phy_info_t *pi, int table, int offset)
{
	phy_reg_write(pi, GPHY_TABLE_ADDR, (((table) << 10) | (offset)));
	return (phy_reg_read(pi, GPHY_TABLE_DATA));
}

void
WRITE_APHY_TABLE_ENT(phy_info_t *pi, int table, int offset, uint16 data)
{
	uint16 goff = 0;

	if (table >= GPHY_TO_APHY_OFF) {
		table -= GPHY_TO_APHY_OFF;
		goff = GPHY_TO_APHY_OFF;
	}
	phy_reg_write(pi, (APHY_TABLE_ADDR + goff), (((table) << 10) | (offset)));
	phy_reg_write(pi, (APHY_TABLE_DATA_I + goff), (data));
}

uint16
read_aphy_table_ent(phy_info_t *pi, int table, int offset)
{
	uint16 goff = 0;

	if (table >= GPHY_TO_APHY_OFF) {
		table -= GPHY_TO_APHY_OFF;
		goff = GPHY_TO_APHY_OFF;
	}
	phy_reg_write(pi, (APHY_TABLE_ADDR + goff), (((table) << 10) | (offset)));
	return (phy_reg_read(pi, (APHY_TABLE_DATA_I + goff)));
}


#if defined(DBG_PHY_IOV)
#if defined(BCMDBG)
void
wlc_phytable_read_abgphy(phy_info_t *pi, phy_table_info_t *ti, uint16 addr, uint16 *val,
	uint16 *qval)
{
	if (ISGPHY(pi)) {

		*qval = 0;
		if (ti->q) {
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_TABLE_ADDR),
				((ti->table << 10) | addr));
			*qval = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_TABLE_DATA_Q));
			*val = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_TABLE_DATA_I));
		} else if (ti->table == 0x80)
			*val = read_gphy_table_ent(pi, 0, addr);
		else
			*val = read_aphy_table_ent(pi, GPHY_TO_APHY_OFF + ti->table, addr);

	} else if (ISAPHY(pi)) {

		*qval = 0;
		if (ti->q) {
			phy_reg_write(pi, APHY_TABLE_ADDR, ((ti->table << 10) | addr));
			*qval = phy_reg_read(pi, APHY_TABLE_DATA_Q);
			*val = phy_reg_read(pi, APHY_TABLE_DATA_I);
		} else
			*val = read_aphy_table_ent(pi, ti->table, addr);
	}
}
#endif 
#endif 

/* Here starts the contents of d11ag_workarounds: */

static void
WLBANDINITFN(gphy_trlut_orig)(phy_info_t *pi)
{
	PHY_REG_LIST_START_WLBANDINITDATA
		/* Set GPHY TR LUT values to proper defaults */
		PHY_REG_WRITE_RAW_ENTRY(GPHY_TRLUT1, 0xba98)
		PHY_REG_WRITE_RAW_ENTRY(GPHY_TRLUT2, 0x7654)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
WLBANDINITFN(gphy_wrssi_off)(phy_info_t *pi)
{
	int k;

	/* Set WRSSI thresholds in gain control to -32
	 * This ensures that WRSSI has no impact on limiting LNA gain
	 */
	if (GREV_IS(pi->pubpi.phy_rev, 1)) {
		for (k = 0; k < 4; k++) {
			/* g_dsss_wrssi_lna_low_gain_change_tbl */
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x15), k + 4, 0x20);
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x15), k + 8, 0x20);
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x15), k + 12, 0x20);
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x15), k + 16, 0x20);
		}
	} else {
		/* This is an overkill, not all entries need to be written to */
		for (k = 0; k < 48; k++)
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x4), k, 0x820);
	}
}

static void
WLBANDINITFN(gphy_alt_agc)(phy_info_t *pi)
{
	uint phyrev = pi->pubpi.phy_rev;
	uint16 table;

	/* Set alternative NRSSI thresholds and
	 * AGC clip gains/ settling times
	 */

	ASSERT(ISGPHY(pi));

	/* Set LNA gain table based on 2050 measurements
	 * old: g_lna_gain_table = [26 20 14 -6]
	 * new: g_lna_gain_table = [25 19 13 -2]
	 */
	if (GREV_IS(phyrev, 1))
		table = GPHY_TO_APHY_OFF + 0x13;
	else
		table = GPHY_TO_APHY_OFF + 0;

	WRITE_APHY_TABLE_ENT(pi, table, 0, 0x100 - 2);
	WRITE_APHY_TABLE_ENT(pi, table, 1, 13);
	WRITE_APHY_TABLE_ENT(pi, table, 2, 19);
	WRITE_APHY_TABLE_ENT(pi, table, 3, 25);

	if (GREV_IS(phyrev, 1)) {
		WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x6), 0, 0x2710);
		WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x6), 1, 0x9b83);
		WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x6), 2, 0x9b83);
		WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x6), 3, 0x0f8d);

		/* The intended LMS update is 1/128 and not 1/512 as the default
		 * value suggests.
		 */
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_MU), 0x4);
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Set g_dsss_shift_bits_ref_gain = 87 instead of 88
		* since the LNA max_gain = 25 instead of 26
		*/
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CCK_GAIN_INFO), 0xff00, 0x5700)
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_MIN_MAX_GAIN), 0x007f, 0x000f)
		/* Fix maximum fine gain = 87 dB */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_MIN_MAX_GAIN), 0x3f80, 0x2b80)
		/* Increase WRSSI wait time to 3 clk20s (default = 2 clk20s) since
		* that is the minimum wait-time value required by the gain state machine
		* to incorporate WRSSI info in the gain computation
		*/
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_ANT_WR_SETTLE), 0x0f00, 0x0300)
	PHY_REG_LIST_EXECUTE(pi);

	/* Set LNA_LOAD = 1; LNA gain = -2 dB for LNA_gaincode = 00 */
	or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x08);

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Set INIT gain = 66 dB */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CLIP_N1_P1_IDX), 0x000f, 0x0008)
		/* Set POW1 clip gain = 60 dB */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_IDX), 0x0f00, 0x0600)
		/* Set NRSSI1 clip gain = 36 dB */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CLIP_N1_N2_IDX), 0x0f00, 0x0700)
		/* Set NRSSI1_POW1 clip gain = 18 dB */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CLIP_N1_P1_IDX), 0x0f00, 0x0100)
	PHY_REG_LIST_EXECUTE(pi);

	if (GREV_IS(phyrev, 1)) {
		/* Set NRSSI2 clip gain = 36 dB */
		phy_reg_mod(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_N2_IDX), 0x000f, 0x0007);
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Change POW1 compare_pwr enable time = 1.4 us (default = 1.7 us) */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_P1_COMP_TIME), 0x00ff, 0x001c)
		/* Change POW1 compare_pwr disable time = 0.1 us (default = 0.2 us) */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_P1_COMP_TIME), 0x3f00, 0x0200)
		/* Change POW2 compare_pwr enable time = 1.4 us (default = 1.7 us) */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_N1_P1_P2_COMP), 0x00ff, 0x001c)
		/* Change NRSSI1 compare_pwr enable time = 1.6 us */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_N1_COMP_TIME), 0x00ff, 0x0020)
		/* Change NRSSI1 compare_pwr disable time = 0.1 us (default = 0.2 us */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_N1_COMP_TIME), 0x3f00, 0x0200)
		/* Change NRSSI1_POW1 gain_settling time = 2.3 us */
		/* Register is now 16bits, preserve high byte */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_N1_P1_GAIN_SETTLE), 0x00ff, 0x002e)
		/* Change NRSSI1_POW1 compare_pwr enable time = 1.3 us (default = 1.7 us) */
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_N1_P1_P2_COMP), 0xff00, 0x1a00)
		/* Change NRSSI2 gain_settle_time = 2.0 us
		* Reduced time from 2.7 us to 2.0 us to allow for increased
		* str0_ctr_min value
		*/
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_N1_N2_GAIN_SETTLE), 0x00ff, 0x0028)
		/* Change NRSSI1 gain_settle_time = 2.2 us
		* Reduced time from 2.7 us to 2.2 us to allow for increased
		* str0_ctr_min value
		*/
		PHY_REG_MOD_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_N1_N2_GAIN_SETTLE), 0xff00, 0x2c00)
	PHY_REG_LIST_EXECUTE(pi);

	if (GREV_IS(phyrev, 1)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* Needed for selection diversity */
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_PLCP_TMT_STR0_MIN, 0x92b)
			/* Needed for selection diversity */
			/*
			* Set gain_backoff to 1 dB
			* to reduce 48/54 Mbps PER ripple - helps on BU board #3
			*/
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_GAIN_INFO, 0x001e, 0x0002)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* setting it back to 0 puts the signal at the [-5 -2] dBm
			* range instead of at the [-8 -5] dBm range.
			*/
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_GAIN_INFO, 0x001e, 0)
			/* Barely-clip threshold at +5 dBm */
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_BO_THRESH, 0x287a)
			/* Barely-clip backoff to 4 dB */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_LPF_GAIN_BO_THRESH,
				0x000f, 0x0004)
		PHY_REG_LIST_EXECUTE(pi);

		if (GREV_GE(phyrev, 6)) {
			PHY_REG_LIST_START_WLBANDINITDATA
				/* CCK barely-clip threshold at +5 dBm */
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CCK_CLIP_BO_THRESH,
					0x287a)
				/* CCK barely-clip backoff to 4 dB */
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_LPF_GAIN_BO_THRESH,
					0xf000, 0x3000)
			PHY_REG_LIST_EXECUTE(pi);
		}
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Needed for selection diversity */
		PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_IDX, 0x7f7f, 0x7874)
		/* Needed for selection diversity */
		/* decrease clip-on-aux threshold */
		PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_AUX_CLIP_THRESH, 0x1c00)
	PHY_REG_LIST_EXECUTE(pi);

	if (GREV_IS(phyrev, 1)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* Needed for selection diversity
			* Make g_dsss_antdiv_pow[12]gains the same as g_clip_pow[12] gains
			*/
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_P1_P2,
				0x0f00, 0x0600)
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_ANT2_DWELL, 0x5e)
			/* Change g_dsss_ant_trans_settle time = 1.5 us (default = 2.2 us) */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_ANT_WR_SETTLE,
				0x00ff, 0x001e)
			/* Change g_dsss_ant_comp_pwr_dis time = 0.1 us (default = 0.3 us) */
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_ANT_COMP_TIME, 0x2)
		PHY_REG_LIST_EXECUTE(pi);
	}

	/* lna_gain_change table */
	if (GREV_IS(phyrev, 1))
		table = GPHY_TO_APHY_OFF + 0x15;
	else
		table = GPHY_TO_APHY_OFF + 2;

	WRITE_APHY_TABLE_ENT(pi, table, 0, 0);
	WRITE_APHY_TABLE_ENT(pi, table, 1, 0x7);
	WRITE_APHY_TABLE_ENT(pi, table, 2, 0x10);
	WRITE_APHY_TABLE_ENT(pi, table, 3, 0x1c);

	/* activate the AGC improvements
	 * avoid going into dcest low bw mode, unless the signal is detected
	 */
	if (GREV_GE(phyrev, 6)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			/*  dcEst2_N1toN2 & dcEst2_N1P1toN2 set to 0, i.e. high bw mode */
			PHY_REG_AND_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DC_B0, ~0x3)
			/*  dcEst2_PwrCompDis set to 0, i.e. high bw mode */
			PHY_REG_AND_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DC_B0, ~0x1000)
		PHY_REG_LIST_EXECUTE(pi);
	}
}

/*
 * TR/Ant look-up table for 11g composite front-end module
 *   Should work for Murata FEM, corrected SiGe FEM, and others
 *
 * Eight nibbles (least->most sig) in 0x803, 0x804
 *   output       seln & selp & tx_pu & rx_pu  ==> RxMain & RxAux & TxAux & TxMain
 *   indexed by   selp & tx_pu & rx_pu
 */
static void
WLBANDINITFN(gphy_trlut_fem_war)(phy_info_t *pi)
{
	ASSERT(ISGPHY(pi));

	PHY_REG_LIST_START_WLBANDINITDATA
		/* selp == 0 ==> Main */
		PHY_REG_WRITE_RAW_ENTRY(GPHY_TRLUT1, 0x3120)
		/* selp == 1 ==> Aux */
		PHY_REG_WRITE_RAW_ENTRY(GPHY_TRLUT2, 0xc480)
	PHY_REG_LIST_EXECUTE(pi);
}


static void
WLBANDINITFN(aphy_crs_ed_war)(phy_info_t *pi)
{
	ASSERT(ISGPHY(pi));

	if (GREV_IS(pi->pubpi.phy_rev, 1)) {
		/* WAR Raise both energy on and energy off thresholds by ~9 dB */
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_PHYCRSTH), 0x4f19);
		return;
	} else if (GREV_IS(pi->pubpi.phy_rev, 2)) {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* For rev2, the ed_on and ed_off thresholds are separate
			 * registers. Further these new registers hold power and not the rms
			 * values. So program these register to ^2 of rev1 values
			 */
			PHY_REG_WRITE_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CRSON_THRESH), 0x1861)
			PHY_REG_WRITE_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CRSOFF_THRESH), 0x0271)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* Rev3 (rev4 aphy) needs these parameters in logarithmic units
			* apply round( 2^2*10*log10() ) to get the new values.
			*/
			PHY_REG_WRITE_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CRSON_THRESH), 0x98)
			PHY_REG_WRITE_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_CRSOFF_THRESH), 0x70)
			/* Rev4 aphy has a separate EdOn threshold for P1 state
			 * this value is based on crsOnThreshold and init_gain - clip_p1_gain
			 * of 6db
			 */
			PHY_REG_WRITE_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_EDON_P1), 0x80)
		PHY_REG_LIST_EXECUTE(pi);
	}

	phy_reg_or(pi, (GPHY_TO_APHY_OFF + APHY_ANT_DWELL), 0x0800);
}

static void
WLBANDINITFN(aphy_crs_thresh_war)(phy_info_t *pi)
{
	phy_reg_mod(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN), 0x03c0, 0x0340);
}

static void
WLBANDINITFN(aphy_crs_blank_war)(phy_info_t *pi)
{
	phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_RESET_LEN), 0x5a);
}

static void
WLBANDINITFN(aphy_aux_clip_thr_war)(phy_info_t *pi)
{
	phy_reg_write(pi, APHY_AUX_CLIP_THRESH, 0x3800);
}

static void
WLBANDINITFN(aphy_afc_dac_war)(phy_info_t *pi)
{
	ASSERT(ISAPHY(pi));

	PHY_REG_LIST_START_WLBANDINITDATA
		PHY_REG_WRITE_RAW_ENTRY(APHY_SCALE_FACT_I, 0x03ff)
		PHY_REG_WRITE_RAW_ENTRY(APHY_SCALE_FACT_Q, 0x0400)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
WLBANDINITFN(aphy_tx_dc_offs_war)(phy_info_t *pi)
{
	/* reduce tx_phy_start time since it causes false CRS0
	 * really should be delaying 2060 tx on and TR switch time
	 */
	WRITE_APHY_TABLE_ENT(pi, 0xe, 7, 0x51);
}

static void
WLBANDINITFN(aphy_init_gain_war)(phy_info_t *pi)
{
	/*
	 * increase dynamic range of 11a CRS
	 *
	 * increase init gain from      111 111 110 000  = ff0
	 *                      to      111 111 111 001  = ff9
	 *
	 * increase tx->rx gain from    000 101 0 111 111 = 2bf
	 *                        to    000 110 0 111 111 = 33f
	 */
	aphy_init_gain(pi, 7, 7, 0, 7, 1, 1);

	if (NORADIO_ENAB(pi->pubpi))
		return;

	PHY_REG_LIST_START_WLBANDINITDATA
		/* originally, increase clip thresh by 1.5 dB: 14336 -> 20271
		* But after testing with freq. sel. channels, should only
		* increase clip thresh by 1 dB: 14336 -> 18048
		*/
		PHY_REG_WRITE_RAW_ENTRY(APHY_CLIP_PWR_THRESH, 0x4680)
		/* change "just under clip" backoff from 2 dB to 3 dB */
		PHY_REG_WRITE_RAW_ENTRY(APHY_LPF_GAIN_BO_THRESH, 3)
		/* increase clip gain by 6 db */
		PHY_REG_WRITE_RAW_ENTRY(APHY_CLIP_GAIN_INDX, 0xf40)
		/* reduce "barely-clip" threshold by 3 dB: 14336 -> 7168 */
		PHY_REG_WRITE_RAW_ENTRY(APHY_CLIP_BO_THRESH, 0x1c00)
	PHY_REG_LIST_EXECUTE(pi);

	/* increase crs_pwr_min level from 3 to 4 */
	if (AREV_LE(pi->pubpi.phy_rev, 3)) {
		phy_reg_mod(pi, APHY_MIN_PWR_GSETTL, 0xff00, 0x400);
	} else {
		if (AREV_IS(pi->pubpi.phy_rev, 5)) {
			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_MOD_RAW_ENTRY(APHY_MIN_PWR_GSETTL, 0xff00, 0x1a00)
				PHY_REG_WRITE_RAW_ENTRY(APHY_FACT_RHOSQ, 0x2121)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			/* power on default settings are the right ones no need to change */
		}
	}

	if (AREV_GE(pi->pubpi.phy_rev, 3))
		phy_reg_write(pi, APHY_WW_CLIPVAR_THRESH, 0x3ed5);
}

static void
WLBANDINITFN(aphy_div_war)(phy_info_t *pi)
{
	/* turn crs-on-aux off (bit 10 of reg addr 0x02b)
	 * increase aux-clip-pwr-thresh by ~ 2.2 dB: 14336 -> 22721
	 */
	PHY_REG_LIST_START_WLBANDINITDATA
		PHY_REG_AND_RAW_ENTRY(APHY_ANT_DWELL, 0xfbff)
		PHY_REG_WRITE_RAW_ENTRY(APHY_AUX_CLIP_THRESH, 0x58c1)
	PHY_REG_LIST_EXECUTE(pi);
}

static void
WLBANDINITFN(aphy_gain_tbl_war)(phy_info_t *pi)
{
	ASSERT(ISAPHY(pi));
	if (AREV_IS(pi->pubpi.phy_rev, 2)) {
		/* rework LNA gain change table for better multipath mitigation */
		WRITE_APHY_TABLE_ENT(pi, 2, 3, 15);
		WRITE_APHY_TABLE_ENT(pi, 2, 4, 31);
		WRITE_APHY_TABLE_ENT(pi, 2, 5, 42);
		WRITE_APHY_TABLE_ENT(pi, 2, 6, 48);
		WRITE_APHY_TABLE_ENT(pi, 2, 7, 58);

		/* fix LNA gain values corresponding to LNA gain bits = 000 */
		WRITE_APHY_TABLE_ENT(pi, 0, 0, 19);
		WRITE_APHY_TABLE_ENT(pi, 0, 1, 19);
		WRITE_APHY_TABLE_ENT(pi, 0, 2, 19);
		WRITE_APHY_TABLE_ENT(pi, 0, 3, 19);
		/* add 2 LNA=21 entries */
		WRITE_APHY_TABLE_ENT(pi, 0, 4, 21);
		WRITE_APHY_TABLE_ENT(pi, 0, 5, 21);
		/* add 1 more LNA=25 entry */
		WRITE_APHY_TABLE_ENT(pi, 0, 6, 25);

		/* put in corresponding change for LNA gain bits table */
		WRITE_APHY_TABLE_ENT(pi, 1, 4, 3);
		WRITE_APHY_TABLE_ENT(pi, 1, 5, 3);
		WRITE_APHY_TABLE_ENT(pi, 1, 6, 7);
	} else {
		/* fix LNA gain values corresponding to LNA gain bits = 000 */
		WRITE_APHY_TABLE_ENT(pi, 0, 0, 19);
		WRITE_APHY_TABLE_ENT(pi, 0, 1, 19);
		WRITE_APHY_TABLE_ENT(pi, 0, 2, 19);
		WRITE_APHY_TABLE_ENT(pi, 0, 3, 19);
		WRITE_APHY_TABLE_ENT(pi, 0, 4, 19);
		WRITE_APHY_TABLE_ENT(pi, 0, 5, 19);
	}
}

static void
WLBANDINITFN(aphy_rssi_lut_war)(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	int k;

	if (!NORADIO_ENAB(pi->pubpi)) {
		phy_reg_write(pi, APHY_TABLE_ADDR, 0x4000);
		W_REG(pi->sh->osh, &regs->phyregaddr, APHY_TABLE_DATA_I);
		(void)R_REG(pi->sh->osh, &regs->phyregaddr);
		for (k = 0; k < 16; k++) {
			W_REG(pi->sh->osh, &regs->phyregdata, (k + 8) & 0x0f);
		}
	}
}

static void
WLBANDINITFN(aphy_rssiadc_war)(phy_info_t *pi)
{
	/*
	 * This WAR is meant for chips using .13 mixed signal block ( ana11g_013 )
	 * The RSSI ADC path has more delay compared to the .18 version
	 * Hence, the WRSSI to NRSSI transitions take longer to settle.
	 * We see PER humps due to this There are two ways to fix the PER humps
	 * (1) Increase nrssiDwellTime parameter increase the wait time
	 *  after wrssi->nrssi switch 	- Affects the preamble processing time
	 * (2) Use rssiSel LUT, to map wrssiSel to nrssiSel
	 * - wrssi->nrssi transition will become nrssi->nrssi and hence
	 * no settling issues We'll go with (2), as we don't want to
	 * increase preamble processing time
	*/
	if (pi->pubpi.ana_rev == ANA_11G_013)
		phy_reg_write(pi, APHY_RSSISELL1_TBL, 0x7454);
}

static void
WLBANDINITFN(aphy_rssi_lut)(phy_info_t *pi)
{
	uint16 k, table;

	/* RSSI table needs to be initialized for a an identity mapping */
	if (ISAPHY(pi))
		table = 0x10;
	else
		table = GPHY_TO_APHY_OFF + 0x10;
	for (k = 0; k < 64; k++)
		WRITE_APHY_TABLE_ENT(pi, table, k, k);
}

static void
WLBANDINITFN(aphy_analog_war)(phy_info_t *pi)
{
	uint16 aphyrev, table;

	if (ISAPHY(pi)) {
		aphyrev = (uint16)pi->pubpi.phy_rev;
		table = 0xc;
	} else {
		aphyrev = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_PHYVERSION)) & PV_PV_MASK;
		table = GPHY_TO_APHY_OFF + 0xc;
	}

	if (aphyrev < 3) {
		/* turn on power to RSSI block.
		 * We may not want all of these to start with!!!
		 * RSSIADC control high
		 */
		WRITE_APHY_TABLE_ENT(pi, table, 0x3, 0x1044);

		/* AFCDAC control Increase AFCDAC gain */
		WRITE_APHY_TABLE_ENT(pi, table, 0x4, 0x7201);

		/* RCO control high */
		WRITE_APHY_TABLE_ENT(pi, table, 0x6, 0x0040);
	} else {
		if (ISAPHY(pi)) {
			/* override all the pdn controls & medium_power control */
			phy_reg_write(pi, APHY_PWRDWN, 0x1808);
		} else {
			/* override all the pdn controls & medium_power control */
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_PWRDWN), 0x1000);
		}
	}
}

static void
WLBANDINITFN(aphy_cck_shiftbits_war)(phy_info_t *pi)
{
	phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CCK_SHBITS_GNREF), 0x01a);
}

static void
WLBANDINITFN(aphy_mixedsig_war)(phy_info_t *pi)
{
	/* This WAR needs to be called for rev 3 a-phy
	 * make sure that this function is called before any other
	 * function modifies the mixed signal control registers
	 */

	ASSERT(pi->pubpi.phy_rev == 3);
	ASSERT(pi->pubpi.ana_rev == ANA_11G_018_ALL);

	WRITE_APHY_TABLE_ENT(pi, 0xc, 1, 3);
}

static void
WLBANDINITFN(aphy_dac_war)(phy_info_t *pi)
{
	uint16 val;

	val = read_aphy_table_ent(pi, 0xc, 1);
	if (pi->pubpi.ana_rev == ANA_11G_018)
		val = (val & ~BBATT_11G_MASK) | (uint16)(2 << BBATT_11G_SHIFT);
	else
		val = (val & ~BBATT_11G_MASK2) | (uint16)(2 << BBATT_11G_SHIFT2);
	WRITE_APHY_TABLE_ENT(pi, 0xc, 1, val);
}

static void
WLBANDINITFN(aphy_txpuoff_rxpuon_war)(phy_info_t *pi)
{
	uint16 table;

	if (ISGPHY(pi))
		table = GPHY_TO_APHY_OFF + 0xf;
	else
		table = 0xf;

	/* set T_TX_OFF (table 0xf offset 2) to 0xf */
	WRITE_APHY_TABLE_ENT(pi, table, 2, 0xf);

	/* set T_RX_ON (table 0xf offset 3) to 0x14 */
	WRITE_APHY_TABLE_ENT(pi, table, 3, 0x14);
}

static void
WLBANDINITFN(aphy_papd_war)(phy_info_t *pi)
{
	uint16 pwr_dyn_ctrl_val_2;

	ASSERT(ISAPHY(pi));

	/* store locally modified table */
	pwr_dyn_ctrl_val_2 = read_aphy_table_ent(pi, 0xe, 0xc);

	/* don't let 2060's tx to get turned on for the dummy transmit issued in this proc */
	WRITE_APHY_TABLE_ENT(pi, 0xe, 0xc, 0x7);

	/* Initialize ext_pa_control value used at the end of transmit */
	WRITE_APHY_TABLE_ENT(pi, 0xf, 0x7, 0x0);

	/* issue a dummy transmit to set pa_pd in the right state */
	wlc_phy_do_dummy_tx(pi, TRUE, ON);

	/* restore the locally modified table value */
	WRITE_APHY_TABLE_ENT(pi, 0xe, 0xc, pwr_dyn_ctrl_val_2);
}

static void
WLBANDINITFN(aphy_iqadc_war)(phy_info_t *pi)
{
	uint16 tmp1;

	/*
	 * This WAR is meant for chips using .13 mixed signal block ( ana11g_013 )
	 * The IQ ADC FS is 0.95 Vppd compared to 1.2 Vppd in the .18 version
	 * Hence, for analog_013, the IQ ADC FS is set to 125% setting
	*/
	if (pi->pubpi.ana_rev == ANA_11G_013) {
		tmp1 = read_aphy_table_ent(pi, 0xc, 0);
		WRITE_APHY_TABLE_ENT(pi, 0xc, 0, (tmp1 & 0x0fff));
	}
}

static uint16 WLBANDINITDATA(aphy_long_train)[] = {
	0x0082, 0x0082, 0x0102, 0x0182, 0x0202, 0x0282, 0x0302, 0x0382,
	0x0402, 0x0482, 0x0502, 0x0582, 0x05e2, 0x0662, 0x06e2, 0x0762,
	0x07e2, 0x0842, 0x08c2, 0x0942, 0x09c2, 0x0a22, 0x0aa2, 0x0b02,
	0x0b82, 0x0be2, 0x0c62, 0x0cc2, 0x0d42, 0x0da2, 0x0e02, 0x0e62,
	0x0ee2, 0x0f42, 0x0fa2, 0x1002, 0x1062, 0x10c2, 0x1122, 0x1182,
	0x11e2, 0x1242, 0x12a2, 0x12e2, 0x1342, 0x13a2, 0x1402, 0x1442,
	0x14a2, 0x14e2, 0x1542, 0x1582, 0x15e2, 0x1622, 0x1662, 0x16c1,
	0x1701, 0x1741, 0x1781, 0x17e1, 0x1821, 0x1861, 0x18a1, 0x18e1,
	0x1921, 0x1961, 0x19a1, 0x19e1, 0x1a21, 0x1a61, 0x1aa1, 0x1ac1,
	0x1b01, 0x1b41, 0x1b81, 0x1ba1, 0x1be1, 0x1c21, 0x1c41, 0x1c81,
	0x1ca1, 0x1ce1, 0x1d01, 0x1d41, 0x1d61, 0x1da1, 0x1dc1, 0x1e01,
	0x1e21, 0x1e61, 0x1e81, 0x1ea1, 0x1ee1, 0x1f01, 0x1f21, 0x1f41,
	0x1f81, 0x1fa1, 0x1fc1, 0x1fe1, 0x2001, 0x2041, 0x2061, 0x2081,
	0x20a1, 0x20c1, 0x20e1, 0x2101, 0x2121, 0x2141, 0x2161, 0x2181,
	0x21a1, 0x21c1, 0x21e1, 0x2201, 0x2221, 0x2241, 0x2261, 0x2281,
	0x22a1, 0x22c1, 0x22c1, 0x22e1, 0x2301, 0x2321, 0x2341, 0x2361,
	0x2361, 0x2381, 0x23a1, 0x23c1, 0x23e1, 0x23e1, 0x2401, 0x2421,
	0x2441, 0x2441, 0x2461, 0x2481, 0x2481, 0x24a1, 0x24c1, 0x24c1,
	0x24e1, 0x2501, 0x2501, 0x2521, 0x2541, 0x2541, 0x2561, 0x2561,
	0x2581, 0x25a1, 0x25a1, 0x25c1, 0x25c1, 0x25e1, 0x2601, 0x2601,
	0x2621, 0x2621, 0x2641, 0x2641, 0x2661, 0x2661, 0x2681, 0x2681,
	0x26a1, 0x26a1, 0x26c1, 0x26c1, 0x26e1, 0x26e1, 0x2701, 0x2701,
	0x2721, 0x2721, 0x2740, 0x2740, 0x2760, 0x2760, 0x2780, 0x2780,
	0x2780, 0x27a0, 0x27a0, 0x27c0, 0x27c0, 0x27e0, 0x27e0, 0x27e0,
	0x2800, 0x2800, 0x2820, 0x2820, 0x2820, 0x2840, 0x2840, 0x2840,
	0x2860, 0x2860, 0x2880, 0x2880, 0x2880, 0x28a0, 0x28a0, 0x28a0,
	0x28c0, 0x28c0, 0x28c0, 0x28e0, 0x28e0, 0x28e0, 0x2900, 0x2900,
	0x2900, 0x2920, 0x2920, 0x2920, 0x2940, 0x2940, 0x2940, 0x2960,
	0x2960, 0x2960, 0x2960, 0x2980, 0x2980, 0x2980, 0x29a0, 0x29a0,
	0x29a0, 0x29a0, 0x29c0, 0x29c0, 0x29c0, 0x29e0, 0x29e0, 0x29e0,
	0x29e0, 0x2a00, 0x2a00, 0x2a00, 0x2a00, 0x2a20, 0x2a20, 0x2a20,
	0x2a20, 0x2a40, 0x2a40, 0x2a40, 0x2a40, 0x2a60, 0x2a60, 0x2a60,
	0xffff
};

static uint16 WLBANDINITDATA(gphy_long_train)[] = {
	0x0089, 0x02e9, 0x0409, 0x04e9, 0x05a9, 0x0669, 0x0709, 0x0789,
	0x0829, 0x08a9, 0x0929, 0x0989, 0x0a09, 0x0a69, 0x0ac9, 0x0b29,
	0x0ba9, 0x0be9, 0x0c49, 0x0ca9, 0x0d09, 0x0d69, 0x0da9, 0x0e09,
	0x0e69, 0x0ea9, 0x0f09, 0x0f49, 0x0fa9, 0x0fe9, 0x1029, 0x1089,
	0x10c9, 0x1109, 0x1169, 0x11a9, 0x11e9, 0x1229, 0x1289, 0x12c9,
	0x1309, 0x1349, 0x1389, 0x13c9, 0x1409, 0x1449, 0x14a9, 0x14e9,
	0x1529, 0x1569, 0x15a9, 0x15e9, 0x1629, 0x1669, 0x16a9, 0x16e8,
	0x1728, 0x1768, 0x17a8, 0x17e8, 0x1828, 0x1868, 0x18a8, 0x18e8,
	0x1928, 0x1968, 0x19a8, 0x19e8, 0x1a28, 0x1a68, 0x1aa8, 0x1ae8,
	0x1b28, 0x1b68, 0x1ba8, 0x1be8, 0x1c28, 0x1c68, 0x1ca8, 0x1ce8,
	0x1d28, 0x1d68, 0x1dc8, 0x1e08, 0x1e48, 0x1e88, 0x1ec8, 0x1f08,
	0x1f48, 0x1f88, 0x1fe8, 0x2028, 0x2068, 0x20a8, 0x2108, 0x2148,
	0x2188, 0x21c8, 0x2228, 0x2268, 0x22c8, 0x2308, 0x2348, 0x23a8,
	0x23e8, 0x2448, 0x24a8, 0x24e8, 0x2548, 0x25a8, 0x2608, 0x2668,
	0x26c8, 0x2728, 0x2787, 0x27e7, 0x2847, 0x28c7, 0x2947, 0x29a7,
	0x2a27, 0x2ac7, 0x2b47, 0x2be7, 0x2ca7, 0x2d67, 0x2e47, 0x2f67,
	0x3247, 0x3526, 0x3646, 0x3726, 0x3806, 0x38a6, 0x3946, 0x39e6,
	0x3a66, 0x3ae6, 0x3b66, 0x3bc6, 0x3c45, 0x3ca5, 0x3d05, 0x3d85,
	0x3de5, 0x3e45, 0x3ea5, 0x3ee5, 0x3f45, 0x3fa5, 0x4005, 0x4045,
	0x40a5, 0x40e5, 0x4145, 0x4185, 0x41e5, 0x4225, 0x4265, 0x42c5,
	0x4305, 0x4345, 0x43a5, 0x43e5, 0x4424, 0x4464, 0x44c4, 0x4504,
	0x4544, 0x4584, 0x45c4, 0x4604, 0x4644, 0x46a4, 0x46e4, 0x4724,
	0x4764, 0x47a4, 0x47e4, 0x4824, 0x4864, 0x48a4, 0x48e4, 0x4924,
	0x4964, 0x49a4, 0x49e4, 0x4a24, 0x4a64, 0x4aa4, 0x4ae4, 0x4b23,
	0x4b63, 0x4ba3, 0x4be3, 0x4c23, 0x4c63, 0x4ca3, 0x4ce3, 0x4d23,
	0x4d63, 0x4da3, 0x4de3, 0x4e23, 0x4e63, 0x4ea3, 0x4ee3, 0x4f23,
	0x4f63, 0x4fc3, 0x5003, 0x5043, 0x5083, 0x50c3, 0x5103, 0x5143,
	0x5183, 0x51e2, 0x5222, 0x5262, 0x52a2, 0x52e2, 0x5342, 0x5382,
	0x53c2, 0x5402, 0x5462, 0x54a2, 0x5502, 0x5542, 0x55a2, 0x55e2,
	0x5642, 0x5682, 0x56e2, 0x5722, 0x5782, 0x57e1, 0x5841, 0x58a1,
	0x5901, 0x5961, 0x59c1, 0x5a21, 0x5aa1, 0x5b01, 0x5b81, 0x5be1,
	0x5c61, 0x5d01, 0x5d80, 0x5e20, 0x5ee0, 0x5fa0, 0x6080, 0x61c0,
	0xffff
};

static void
WLBANDINITFN(aphy_fine_freq_tbl)(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	uint16 table_addr, table_data, v, *t = NULL;

	table_addr = APHY_TABLE_ADDR;
	table_data = APHY_TABLE_DATA_I;
	if (ISGPHY(pi)) {
		table_addr += GPHY_TO_APHY_OFF;
		table_data += GPHY_TO_APHY_OFF;
		t = gphy_long_train;
	} else
		t = aphy_long_train;

	/* long training table */
	phy_reg_write(pi, table_addr, 0x5800);
	W_REG(pi->sh->osh, &regs->phyregaddr, table_data);
	(void)R_REG(pi->sh->osh, &regs->phyregaddr);
	while ((v = *t++) != 0xffff) {
		W_REG(pi->sh->osh, &regs->phyregdata, v);
	}
}

/* 4/29/03: changed to values that worked well with freq. sel channels */
static uint16 WLBANDINITDATA(aphy2_noise)[] = {
	0x0001,
	0x0001,
	0x0001,
	0xfffe,
	0xfffe,
	0x3fff,
	0x1000,
	0x0393,
	0xffff
};

static uint16 WLBANDINITDATA(aphy3_noise)[] = {
	0x5e5e,
	0x5e5e,
	0x5e5e,
	0x3f48,
	0x4c4c,		/* last 8 entries are with TR switch at T */
	0x4c4c,		/*  (assume 18dB loss from TR switch) */
	0x4c4c,
	0x2d36,
	0xffff
};

static uint16 WLBANDINITDATA(gphy1_noise)[] = {
	0x013c,
	0x01f5,
	0x031a,
	0x0631,
	0x0001,
	0x0001,
	0x0001,
	0x0001,
	0xffff
};

static uint16 WLBANDINITDATA(gphy2_noise)[] = {
	0x5484,
	0x3c40,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0x0000,
	0xffff
};

static void
WLBANDINITFN(aphy_noise_fig_tbl)(phy_info_t *pi)
{
	uint16 table_addr, table_data_i, table_data_q, v, *t;
	d11regs_t *regs = pi->regs;

	table_addr = APHY_TABLE_ADDR;
	table_data_i = APHY_TABLE_DATA_I;
	table_data_q = APHY_TABLE_DATA_Q;

	if (ISAPHY(pi)) {
		if (AREV_IS(pi->pubpi.phy_rev, 2))
			t = aphy2_noise;
		else
			t = aphy3_noise;
	} else {
		table_addr += GPHY_TO_APHY_OFF;
		table_data_i += GPHY_TO_APHY_OFF;
		table_data_q += GPHY_TO_APHY_OFF;
		if (GREV_IS(pi->pubpi.phy_rev, 1))
			t = gphy1_noise;
		else
			t = gphy2_noise;
	}

	/* noise figure table */
	phy_reg_write(pi, table_addr, 0x1800);
	W_REG(pi->sh->osh, &regs->phyregaddr, table_data_i);
	(void)R_REG(pi->sh->osh, &regs->phyregaddr);
	while ((v = *t++) != 0xffff) {
		W_REG(pi->sh->osh, &regs->phyregdata, v);
	}
}

static uint16 WLBANDINITDATA(min_sig_sq)[] = {
	0x007a, 0x0075, 0x0071, 0x006c, 0x0067, 0x0063, 0x005e, 0x0059,
	0x0054, 0x0050, 0x004b, 0x0046, 0x0042, 0x003d, 0x003d, 0x003d,
	0x003d, 0x003d, 0x003d, 0x003d, 0x003d, 0x003d, 0x003d, 0x003d,
	0x003d, 0x003d, 0x0000, 0x003d, 0x003d, 0x003d, 0x003d, 0x003d,
	0x003d, 0x003d, 0x003d, 0x003d, 0x003d, 0x003d, 0x003d, 0x003d,
	0x0042, 0x0046, 0x004b, 0x0050, 0x0054, 0x0059, 0x005e, 0x0063,
	0x0067, 0x006c, 0x0071, 0x0075, 0x007a, 0xffff
};

static uint16 WLBANDINITDATA(min_sig_sq3)[] = {
	0x00de, 0x00dc, 0x00da, 0x00d8, 0x00d6, 0x00d4, 0x00d2, 0x00cf,
	0x00cd, 0x00ca, 0x00c7, 0x00c4, 0x00c1, 0x00be, 0x00be, 0x00be,
	0x00be, 0x00be, 0x00be, 0x00be, 0x00be, 0x00be, 0x00be, 0x00be,
	0x00be, 0x00be, 0x0000, 0x00be, 0x00be, 0x00be, 0x00be, 0x00be,
	0x00be, 0x00be, 0x00be, 0x00be, 0x00be, 0x00be, 0x00be, 0x00be,
	0x00c1, 0x00c4, 0x00c7, 0x00ca, 0x00cd, 0x00cf, 0x00d2, 0x00d4,
	0x00d6, 0x00d8, 0x00da, 0x00dc, 0x00de, 0xffff
};

static void
WLBANDINITFN(aphy_min_sig_sq_tbl)(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	uint16 table_addr, table_data, v, *t = NULL;

	table_addr = APHY_TABLE_ADDR;
	table_data = APHY_TABLE_DATA_I;

	if (ISGPHY(pi)) {
		table_addr += GPHY_TO_APHY_OFF;
		table_data += GPHY_TO_APHY_OFF;
		if (GREV_IS(pi->pubpi.phy_rev, 2))
			t = min_sig_sq;
		else {
			t = min_sig_sq3;
		}
	} else {
		if (AREV_IS(pi->pubpi.phy_rev, 3))
			t = min_sig_sq;
		else {
			t = min_sig_sq3;
		}
	}

	/* Min sig sq table */
	phy_reg_write(pi, table_addr, 0x5000);
	W_REG(pi->sh->osh, &regs->phyregaddr, table_data);
	(void)R_REG(pi->sh->osh, &regs->phyregaddr);
	while ((v = *t++) != 0xffff) {
		W_REG(pi->sh->osh, &regs->phyregdata, v);
	}
}

typedef struct _complex_type {
	uint16	q;
	uint16	i;
} complex_type;

static complex_type WLBANDINITDATA(aphy_rotor_table)[] = {
	{0xfeb9, 0x3ffd},
	{0xfec6, 0x3ffd},
	{0xfed2, 0x3ffd},
	{0xfedf, 0x3ffd},
	{0xfeec, 0x3ffe},
	{0xfef8, 0x3ffe},
	{0xff05, 0x3ffe},
	{0xff11, 0x3ffe},
	{0xff1e, 0x3ffe},
	{0xff2a, 0x3fff},
	{0xff37, 0x3fff},
	{0xff44, 0x3fff},
	{0xff50, 0x3fff},
	{0xff5d, 0x3fff},
	{0xff69, 0x3fff},
	{0xff76, 0x3fff},
	{0xff82, 0x4000},
	{0xff8f, 0x4000},
	{0xff9b, 0x4000},
	{0xffa8, 0x4000},
	{0xffb5, 0x4000},
	{0xffc1, 0x4000},
	{0xffce, 0x4000},
	{0xffda, 0x4000},
	{0xffe7, 0x4000},
	{0xfff3, 0x4000},
	{0x0000, 0x4000},
	{0x000d, 0x4000},
	{0x0019, 0x4000},
	{0x0026, 0x4000},
	{0x0032, 0x4000},
	{0x003f, 0x4000},
	{0x004b, 0x4000},
	{0x0058, 0x4000},
	{0x0065, 0x4000},
	{0x0071, 0x4000},
	{0x007e, 0x4000},
	{0x008a, 0x3fff},
	{0x0097, 0x3fff},
	{0x00a3, 0x3fff},
	{0x00b0, 0x3fff},
	{0x00bc, 0x3fff},
	{0x00c9, 0x3fff},
	{0x00d6, 0x3fff},
	{0x00e2, 0x3ffe},
	{0x00ef, 0x3ffe},
	{0x00fb, 0x3ffe},
	{0x0108, 0x3ffe},
	{0x0114, 0x3ffe},
	{0x0121, 0x3ffd},
	{0x012e, 0x3ffd},
	{0x013a, 0x3ffd},
	{0x0147, 0x3ffd},
	{0xffff, 0xffff}
};

static void
WLBANDINITFN(aphy_rotor_tbl)(phy_info_t *pi)
{
	uint16 table_addr, table_data_i, table_data_q;
	complex_type *c;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	table_addr = APHY_TABLE_ADDR;
	table_data_i = APHY_TABLE_DATA_I;
	table_data_q = APHY_TABLE_DATA_Q;
	if (pi_abg->sbtml_gm) {
		table_addr += GPHY_TO_APHY_OFF;
		table_data_i += GPHY_TO_APHY_OFF;
		table_data_q += GPHY_TO_APHY_OFF;
	}

	/* rotor table */
	phy_reg_write(pi, table_addr, 0x2000);
	c = aphy_rotor_table;
	while ((c->q != 0xffff) && (c->i != 0xffff)) {
		phy_reg_write(pi, table_data_q, c->q);
		phy_reg_write(pi, table_data_i, c->i);
		c++;
	}
}

/* Modify noise-scale to account for non-uniform SINR across the
 * receive frequency band
 * accounting for I/Q imbalance and 1/f noise effects
 */
static uint16 WLBANDINITDATA(aphy_noise_scale)[] = {
	0x6c77, 0x5162, 0x3b40, 0x3335, 0x2f2d, 0x2a2a, 0x2527, 0x1f21,
	0x1a1d, 0x1719, 0x1616, 0x1414, 0x1414, 0x1400, 0x1414, 0x1614,
	0x1716, 0x1a19, 0x1f1d, 0x2521, 0x2a27, 0x2f2a, 0x332d, 0x3b35,
	0x5140, 0x6c62, 0x0077, 0xffff
};

static uint16 WLBANDINITDATA(aphy3_noise_scale)[] = {
	0xd8dd, 0xcbd4, 0xbcc0, 0xb6b7, 0xb2b0, 0xadad, 0xa7a9, 0x9fa1,
	0x969b, 0x9195, 0x8f8f, 0x8a8a, 0x8a8a, 0x8a00, 0x8a8a, 0x8f8a,
	0x918f, 0x9695, 0x9f9b, 0xa7a1, 0xada9, 0xb2ad, 0xb6b0, 0xbcb7,
	0xcbc0, 0xd8d4, 0x00dd, 0xffff
};

static uint16 WLBANDINITDATA(aphy_encore_noise_scale)[] = {
	0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4,
	0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa400, 0xa4a4, 0xa4a4,
	0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4, 0xa4a4,
	0xa4a4, 0xa4a4, 0x00a4, 0xffff
};

static void
WLBANDINITFN(aphy_noise_scale_tbl)(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	uint16 table_addr, table_data, *t;
	uint16 v = 0, v0 = 0, v1 = 0;
	int i;
	uint phyrev = pi->pubpi.phy_rev;

	table_addr = APHY_TABLE_ADDR;
	table_data = APHY_TABLE_DATA_I;

	if (ISAPHY(pi)) {
		if (AREV_IS(phyrev, 2)) {
			/* increased from 0x23 to 0x67 to help mitigate
			 * frequency selectivity of channels
			 */
			v = 0x6767;
			v0 = 0x6700;
			v1 = 0x0067;
		} else if (AREV_IS(phyrev, 3)) {
			v = 0x2323;
			v0 = 0x2300;
			v1 = 0x0023;
		} else {
			v = 0xa4a4;
			v0 = 0xa400;
			v1 = 0x00a4;
		}

		/* noise scale table */
		phy_reg_write(pi, table_addr, 0x1400);
		W_REG(pi->sh->osh, &regs->phyregaddr, table_data);
		(void)R_REG(pi->sh->osh, &regs->phyregaddr);

		for (i = 0; i < 13; i++)
			W_REG(pi->sh->osh, &regs->phyregdata, v);
		W_REG(pi->sh->osh, &regs->phyregdata, v0);
		for (i = 14; i < 26; i++)
			W_REG(pi->sh->osh, &regs->phyregdata, v);
		W_REG(pi->sh->osh, &regs->phyregdata, v1);
	} else {
		/* PHY_TYPE_G */
		table_addr += GPHY_TO_APHY_OFF;
		table_data += GPHY_TO_APHY_OFF;

		/* noise scale table */
		if (GREV_GE(phyrev, 6)) {
			if (pi->pubpi.abgphy_encore)
				t = aphy_encore_noise_scale;
			else
				t = aphy3_noise_scale;
		} else
			t = aphy_noise_scale;

		phy_reg_write(pi, table_addr, 0x1400);
		W_REG(pi->sh->osh, &regs->phyregaddr, table_data);
		(void)R_REG(pi->sh->osh, &regs->phyregaddr);

		while ((v = *t++) != 0xffff)
			W_REG(pi->sh->osh, &regs->phyregdata, v);
	}
}

static complex_type WLBANDINITDATA(aphy_retard_table)[] = {
	{0xdb93, 0xcb87},
	{0xd666, 0xcf64},
	{0xd1fd, 0xd358},
	{0xcda6, 0xd826},
	{0xca38, 0xdd9f},
	{0xc729, 0xe2b4},
	{0xc469, 0xe88e},
	{0xc26a, 0xee2b},
	{0xc0de, 0xf46c},
	{0xc073, 0xfa62},
	{0xc01d, 0x00d5},
	{0xc076, 0x0743},
	{0xc156, 0x0d1e},
	{0xc2e5, 0x1369},
	{0xc4ed, 0x18ff},
	{0xc7ac, 0x1ed7},
	{0xcb28, 0x23b2},
	{0xcefa, 0x28d9},
	{0xd2f6, 0x2d3f},
	{0xd7bb, 0x3197},
	{0xdce5, 0x3568},
	{0xe1fe, 0x3875},
	{0xe7d1, 0x3b35},
	{0xed66, 0x3d35},
	{0xf39b, 0x3ec4},
	{0xf98e, 0x3fa7},
	{0x0000, 0x4000},
	{0x0672, 0x3fa7},
	{0x0c65, 0x3ec4},
	{0x129a, 0x3d35},
	{0x182f, 0x3b35},
	{0x1e02, 0x3875},
	{0x231b, 0x3568},
	{0x2845, 0x3197},
	{0x2d0a, 0x2d3f},
	{0x3106, 0x28d9},
	{0x34d8, 0x23b2},
	{0x3854, 0x1ed7},
	{0x3b13, 0x18ff},
	{0x3d1b, 0x1369},
	{0x3eaa, 0x0d1e},
	{0x3f8a, 0x0743},
	{0x3fe3, 0x00d5},
	{0x3f8d, 0xfa62},
	{0x3f22, 0xf46c},
	{0x3d96, 0xee2b},
	{0x3b97, 0xe88e},
	{0x38d7, 0xe2b4},
	{0x35c8, 0xdd9f},
	{0x325a, 0xd826},
	{0x2e03, 0xd358},
	{0x299a, 0xcf64},
	{0x246d, 0xcb87},
	{0xffff, 0xffff}
};

static void
WLBANDINITFN(aphy_adv_ret_tbl)(phy_info_t *pi)
{
	uint16 table_addr, table_data_i, table_data_q;
	complex_type *c;

	table_addr = APHY_TABLE_ADDR;
	table_data_i = APHY_TABLE_DATA_I;
	table_data_q = APHY_TABLE_DATA_Q;
	if (ISGPHY(pi)) {
		table_addr += GPHY_TO_APHY_OFF;
		table_data_i += GPHY_TO_APHY_OFF;
		table_data_q += GPHY_TO_APHY_OFF;
	}

	/* adv retard table */
	phy_reg_write(pi, table_addr, 0x2400);
	c = aphy_retard_table;
	while ((c->q != 0xffff) && (c->i != 0xffff)) {
		phy_reg_write(pi, table_data_q, c->q);
		phy_reg_write(pi, table_data_i, c->i);
		c++;
	}
}

static void
WLBANDINITFN(aphy_board_based_war)(phy_info_t *pi)
{
	if (ISGPHY(pi)) {
		/* gphy */
		if ((pi->sh->boardvendor != VENDOR_BROADCOM) ||
		    (pi->sh->boardtype != BU4306_BOARD) ||
		    (pi->sh->boardrev != 0x17)) {
			if (GREV_GT(pi->pubpi.phy_rev, 1)) {
				WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 1), 1, 2);
				WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 1), 2, 1);
				/* ff the external lna controls are enabled,
				 * remove overrides and init tables.
				 */
				if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
				    (GREV_GE(pi->pubpi.phy_rev, 7))) {
					phy_reg_and(pi, GPHY_RF_OVERRIDE, ~0x0800);
					WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 1), 0x20, 1);
					WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 1), 0x21, 1);
					WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 1), 0x22, 1);
					WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 1), 0x23, 0);
					WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 1), 0, 0);
					WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 1), 3, 2);
				}
			} else {
				WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x14), 1, 2);
				WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x14), 2, 1);
			}
		}
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_FEM) {
			gphy_trlut_fem_war(pi);
		}
	} else {
		/* aphy */
		if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
		    (pi->sh->boardtype == BU4306_BOARD) &&
		    (pi->sh->boardrev < 0x30)) {

			PHY_REG_LIST_START_WLBANDINITDATA
				/* T/R and antenna switch controls */
				/* override PA control */
				PHY_REG_WRITE_RAW_ENTRY(APHY_RF_OVERRIDE, 0xe000)
				/* TR switch LUT */
				PHY_REG_WRITE_RAW_ENTRY(APHY_TR_LUT1, 0x0140)
				/* TR switch LUT */
				PHY_REG_WRITE_RAW_ENTRY(APHY_TR_LUT2, 0x0280)
			PHY_REG_LIST_EXECUTE(pi);

		} else {
			/* TR switch controls on 4318 dual band Rev 1.X are hoarked */
			if ((pi->sh->boardtype == MP4318_BOARD) && (pi->sh->boardrev < 0x20)) {
				phy_reg_write(pi, APHY_TR_LUT1, 0x0210);
				phy_reg_write(pi, APHY_TR_LUT2, 0x0840);
			} else {
				/* default super switch LUT values */
				phy_reg_write(pi, APHY_TR_LUT1, 0x0140);
				phy_reg_write(pi, APHY_TR_LUT2, 0x0280);
			}
			/* override PA control and clear synth_pu_xor bit */
			if (AREV_GE(pi->pubpi.phy_rev, 5)) {
				phy_reg_write(pi, APHY_RF_OVERRIDE, 0x2000);
			} else {
				phy_reg_write(pi, APHY_RF_OVERRIDE, 0xe000);
			}
			WRITE_APHY_TABLE_ENT(pi, 0xe, 0x8, 0x39);
			WRITE_APHY_TABLE_ENT(pi, 0xf, 0x7, 0x40);
		}
	}
}

static void
aphy_tx_lna_gain_war(phy_info_t *pi)
{
	/* Set 2060 LNA gain to 0x0 during .11a transmission
	 * WAR active for all aphy revs
	 */
	WRITE_APHY_TABLE_ENT(pi, 0xe, 0xd, 0);
}

static void
aphy_crs_reset_war(phy_info_t *pi)
{

	phy_reg_write(pi, APHY_RESET_LEN, 0x0064);
}

static void
bcm2060_tx_lna_gain_war(phy_info_t *pi)
{
	ASSERT(ISAPHY(pi));

	wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_A2060WAR, MHF1_A2060WAR, WLC_BAND_5G);
}

static void
aphy_lms_war(phy_info_t *pi)
{
	ASSERT(ISAPHY(pi));

	/* Please see PRs 11342 and 16934
	 * The intended LMS update rate is 1/128 and not 1/512
	 * For gmode == 1, the WAR is already done in gphy_alt_agc
	 */
	phy_reg_mod(pi, APHY_MU, 0x3f, 4);
}

/* Set non-pilot to pilot ratio for CPLL for 24Mbps/36Mbps
 * else, we sometimes have trouble receiving data from an AP
 * with unstable carrier frequency. This is a big issue for 24Mbps as
 * 802.11 ACKs are received on that.
 */
static void
WLBANDINITFN(aphy_cpll_nonpilot_war)(phy_info_t *pi)
{
	int offset = 0;

	if (ISGPHY(pi))
		offset = GPHY_TO_APHY_OFF;

	/* rate = 24 Mbps, of table 0x11, i.e., "cpll_frac_non_pilots AND
	 * rate = 36 Mbps, of table 0x11 should be set to 0
	 * It's the ratio of non-pilot (i.e., data sub-carrier) to pilot
	 * (known a-priori at tx and rx) phase error used to update CPLL
	 * (carrier frequency offset tracking PLL)
	 */
	WRITE_APHY_TABLE_ENT(pi, offset + 0x11, 0x4, 0);
	WRITE_APHY_TABLE_ENT(pi, offset + 0x11, 0x5, 0);
}

static void
WLBANDINITFN(wlc_aphy_setup)(phy_info_t *pi)
{
	uint phy_rev = pi->pubpi.phy_rev;

	if (ISAPHY(pi)) {
		/* aphy doing 11a */
		if (AREV_IS(phy_rev, 2)) {
			/* 4306b0 */
			aphy_papd_war(pi);
			aphy_aux_clip_thr_war(pi);
			aphy_afc_dac_war(pi);
			aphy_tx_dc_offs_war(pi);
			aphy_init_gain_war(pi);
			aphy_div_war(pi);
			aphy_gain_tbl_war(pi);
			aphy_rssi_lut_war(pi);
			aphy_analog_war(pi);
			aphy_dac_war(pi);
			aphy_fine_freq_tbl(pi);
			aphy_noise_fig_tbl(pi);
			aphy_rotor_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_adv_ret_tbl(pi);
			aphy_tx_lna_gain_war(pi);
			aphy_crs_reset_war(pi);
			bcm2060_tx_lna_gain_war(pi);
			aphy_lms_war(pi);
		} else if (AREV_IS(phy_rev, 3)) {
			/* 4306c0, 4712 */
			aphy_papd_war(pi);
			aphy_mixedsig_war(pi);
			aphy_rssi_lut(pi);
			aphy_tx_dc_offs_war(pi);
			aphy_init_gain_war(pi);
			aphy_dac_war(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			aphy_analog_war(pi);
			aphy_gain_tbl_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
			aphy_tx_lna_gain_war(pi);
		} else if (AREV_IS(phy_rev, 5)) {
			/* 4320a2 */
			aphy_iqadc_war(pi);
			aphy_papd_war(pi);
			aphy_rssi_lut(pi);
			aphy_tx_dc_offs_war(pi);
			aphy_init_gain_war(pi);
			aphy_dac_war(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			aphy_analog_war(pi);
			aphy_gain_tbl_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
			aphy_tx_lna_gain_war(pi);
		} else if (AREV_IS(phy_rev, 6)) {
			/* 4318b0 */
			aphy_papd_war(pi);
			aphy_rssi_lut(pi);
			aphy_tx_dc_offs_war(pi);
			aphy_init_gain_war(pi);
			aphy_dac_war(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			aphy_analog_war(pi);
			aphy_gain_tbl_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
			aphy_tx_lna_gain_war(pi);
		} else if (AREV_IS(phy_rev, 7)) {
			/* 5352a0, 4311a0 */
			aphy_iqadc_war(pi);
			aphy_papd_war(pi);
			aphy_rssi_lut(pi);
			aphy_tx_dc_offs_war(pi);
			aphy_init_gain_war(pi);
			aphy_dac_war(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			aphy_analog_war(pi);
			aphy_gain_tbl_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
			aphy_tx_lna_gain_war(pi);
			aphy_rssiadc_war(pi);
		} else if (AREV_IS(phy_rev, 8)) {
			/* 4311b0 */
			aphy_iqadc_war(pi);
			aphy_papd_war(pi);
			aphy_rssi_lut(pi);
			aphy_tx_dc_offs_war(pi);
			aphy_init_gain_war(pi);
			aphy_dac_war(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			aphy_analog_war(pi);
			aphy_gain_tbl_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
			aphy_tx_lna_gain_war(pi);
			aphy_rssiadc_war(pi);
		} else {
			ASSERT((const char*)"Bad aphy rev" == NULL);
		}
	} else {
		/* gmode */
		if (GREV_IS(phy_rev, 1)) {
			/* 4306b0 */
			aphy_crs_ed_war(pi);
			aphy_crs_thresh_war(pi);
			aphy_crs_blank_war(pi);
			aphy_cck_shiftbits_war(pi);
			aphy_fine_freq_tbl(pi);
			aphy_noise_fig_tbl(pi);
			aphy_rotor_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_adv_ret_tbl(pi);
			gphy_wrssi_off(pi);
			gphy_alt_agc(pi);
		} else if (GREV_IS(phy_rev, 2)) {
			/* 4306c0 */
			gphy_trlut_orig(pi);
			aphy_crs_ed_war(pi);
			aphy_rssi_lut(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			gphy_wrssi_off(pi);
			gphy_alt_agc(pi);
			aphy_analog_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
		} else if (GREV_IS(phy_rev, 6)) {
			/* 4320A2 */
			gphy_trlut_orig(pi);
			aphy_crs_ed_war(pi);
			aphy_rssi_lut(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			gphy_wrssi_off(pi);
			gphy_alt_agc(pi);
			aphy_analog_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
		} else if (GREV_IS(phy_rev, 7)) {
			/* 5352A0 & 4318 B0 */
			gphy_trlut_orig(pi);
			aphy_crs_ed_war(pi);
			aphy_rssi_lut(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			gphy_wrssi_off(pi);
			gphy_alt_agc(pi);
			aphy_analog_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
		} else if (GREV_IS(phy_rev, 8)) {
			/* 4311a0 */
			gphy_trlut_orig(pi);
			aphy_crs_ed_war(pi);
			aphy_rssi_lut(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			gphy_wrssi_off(pi);
			gphy_alt_agc(pi);
			aphy_analog_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
		} else if (GREV_IS(phy_rev, 9)) {
			/* 4311b0 */
			gphy_trlut_orig(pi);
			aphy_crs_ed_war(pi);
			aphy_rssi_lut(pi);
			aphy_noise_fig_tbl(pi);
			aphy_noise_scale_tbl(pi);
			aphy_min_sig_sq_tbl(pi);
			gphy_wrssi_off(pi);
			gphy_alt_agc(pi);
			aphy_analog_war(pi);
			aphy_txpuoff_rxpuon_war(pi);
		} else {
			ASSERT((const char*)"Bad gphy rev" == NULL);
		}
	}
	aphy_board_based_war(pi);
	aphy_cpll_nonpilot_war(pi);
}

/* Thus ends d11ag_workarounds.tcl */

static void
WLBANDINITFN(aphy_init_gain)(phy_info_t *pi, uchar lna, uchar hpf1, uchar lpf, uchar hpf2,
	uchar hpf3, uchar lnaatten)
{
	uint16 lpf_gain_val = (lpf > 0) ? 6 : 0;
	uint16 gain_bits;


	if (AREV_IS(pi->pubpi.phy_rev, 2)) {
		/* 13bit gain register */
		gain_bits = (((hpf3 & 0x7) << 10) |
		             ((hpf2 & 0x7) << 7) |
		             ((lpf & 1) << 6) |
		             ((hpf1 & 0x7) << 3) |
		             (lna & 0x7));
	} else {
		/* aphy_ww makes sure the 2060ww is set up for 14bit gain on C0 */
		gain_bits = (((hpf3 & 0x7) << 11) |
		             ((hpf2 & 0x7) << 8) |
		             ((lpf & 1) << 7) |
		             ((hpf1 & 0x7) << 4) |
		             ((lnaatten & 0x1) << 3) |
		             (lna & 0x7));
	}

	/* set init_gain indices */
	phy_reg_write(pi, APHY_INIT_GAIN_INDX, (lnaatten << 12) | (lna << 9) |
	              (hpf1 << 6) | (hpf2 << 3) | hpf3);

	/* set LPF gain value */
	phy_reg_mod(pi, APHY_LPF_GAIN_BO_THRESH, 0x00f0, (lpf_gain_val << 4));

	if (AREV_IS(pi->pubpi.phy_rev, 2)) {
		/* set tx-rx control register (init_gain) for 4306b0 */
		WRITE_APHY_TABLE_ENT(pi, 0xf, 0xc, gain_bits);
	}

	write_radio_reg(pi, RADIO_2060WW_RXGAINCTL, gain_bits);
}


static int32
wlc_u_to_s(uint32 val, int bits)
{
	if (val < (uint32)(1 << (bits - 1)))
		return (val);
	return (val - (1 << bits));
}

static uint32
wlc_s_to_u(int32 val, int bits)
{
	if (val >= 0)
		return (val);
	return (val + (1 << bits));
}

/* Proc to adjust baseband DC LOFT cancel value. */
static void
wlc_adjust_bb_loft_cancel(phy_info_t *pi, int curr_tx_gaintable_indx)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	int orig_bb_dc_bias, orig_bb_dc_bias_i, orig_bb_dc_bias_q;
	int new_bb_dc_bias_factor, new_bb_dc_bias_i, new_bb_dc_bias_q;
	int new_bb_dc_bias;
	int tx_bb_dc_bias_loft_factor[] = {
		34, 36, 38, 40, 43, 45, 48, 51, 54, 57, 60, 64, 43, 45, 48, 51, 54, 57,
		60, 64, 51, 54, 57, 60, 64, 45, 48, 51, 54, 57, 60, 64, 34, 36, 38, 40,
		43, 45, 48, 51, 54, 57, 60, 64, 69, 73, 77, 82, 86, 94, 99, 105, 111,
		118, 126, 134, 142, 150, 159, 168, 178, 189, 200, 212
	};

	orig_bb_dc_bias = pi_abg->global_tx_bb_dc_bias_loft;
	orig_bb_dc_bias_i = wlc_u_to_s(orig_bb_dc_bias >> 8, 8);
	orig_bb_dc_bias_q = wlc_u_to_s(orig_bb_dc_bias & 0xff, 8);

	/* table lookup LOFT adjustment factor */
	ASSERT(curr_tx_gaintable_indx < (int)ARRAYSIZE(tx_bb_dc_bias_loft_factor));
	new_bb_dc_bias_factor = tx_bb_dc_bias_loft_factor[curr_tx_gaintable_indx];

	new_bb_dc_bias_i = (new_bb_dc_bias_factor * orig_bb_dc_bias_i + 32) >> 6;
	new_bb_dc_bias_i = MIN(new_bb_dc_bias_i, MAX_BB_TX_DC);
	new_bb_dc_bias_i = MAX(new_bb_dc_bias_i, -MAX_BB_TX_DC);
	new_bb_dc_bias_q = (new_bb_dc_bias_factor * orig_bb_dc_bias_q + 32) >> 6;
	new_bb_dc_bias_q = MIN(new_bb_dc_bias_q, MAX_BB_TX_DC);
	new_bb_dc_bias_q = MAX(new_bb_dc_bias_q, -MAX_BB_TX_DC);

	PHY_TMP(("wl%d: %s: new_bb_dc_bias_q 0x%x, new_bb_dc_bias_i 0x%x\n",
	         pi->sh->unit, __FUNCTION__, new_bb_dc_bias_q, new_bb_dc_bias_i));

	new_bb_dc_bias_i = wlc_s_to_u(new_bb_dc_bias_i, 8);
	new_bb_dc_bias_q = wlc_s_to_u(new_bb_dc_bias_q, 8);
	new_bb_dc_bias   = (new_bb_dc_bias_i << 8) | new_bb_dc_bias_q;

	PHY_TMP(("wl%d: %s: orig = 0x%x,txgain_indx = %2d, new_bb_factr = 0x%x new = 0x%4x\n",
	         pi->sh->unit, __FUNCTION__, orig_bb_dc_bias, curr_tx_gaintable_indx,
	         new_bb_dc_bias_factor, new_bb_dc_bias));

	phy_reg_write(pi, APHY_TX_COMP_OFFSET, new_bb_dc_bias & 0xffff);
}

static int
wlc_loft_collect_tssi(phy_info_t *pi)
{
	int16 tssi_val;
	int iter_ctr, tssi_accumval = 0;

	for (iter_ctr = 0; iter_ctr < 2; iter_ctr++) {
		wlc_phy_do_dummy_tx(pi, TRUE, ON);
		tssi_val = phy_reg_read(pi, APHY_TSSI_STAT) & 0x00ff;

		if (tssi_val > 127)
			tssi_val -= 256;

		tssi_accumval += tssi_val;
	}
	return (tssi_accumval);
}

/**********************************************************************************
 * 4 phases.
 * Phase 1: Radio: Find combination(s) of i & q that yield lowest power settings.
 * Phase 2: Radio: Resolve any tiebreakers from Phase 1.
 * Phase 3: Baseband: Find combination(s) of i & q that yield lowest power settings.
 * Phase 4: Baseband: Resolve any tiebreakers from Phase 3.
 * ********************************************************************************
 */
static void
WLBANDINITFN(wlc_aphy_lof_cal)(phy_info_t *pi, int cal_type)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	uchar full_imax_tx_vos_list[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc,
	                                 0xd, 0xe, 0xf};
	uchar full_qmax_tx_vos_list[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0xa, 0xb, 0xc,
	                                 0xd, 0xe, 0xf};
	uchar partial_imax_tx_vos_list[] = {0, 4, 8, 9, 0xd};
	uchar partial_qmax_tx_vos_list[] = {0, 1, 5, 6, 0xa};
	uchar rf_vec[] = {0x20, 0x60, 0x61, 0x62, 0x91, 0x92, 0x93, 0x94, 0x9c};
	uchar rf_vec1[] = {0x60, 0x61, 0x62, 0x91, 0x9a, 0x9b, 0x9c};
	int rf_vec_index;
	uchar *qmax_RF_tx_vos_list, *imax_RF_tx_vos_list;
	uchar *max_RF_tx_vos_list;
	int orig_channel;

	int regval;
	int max_tssi_accumval, init_max_tssi_accumval, tssi_accumval, min_good_tssi;
	int16 max_RF_tx_vos, min_RF_tx_vos, init_min_RF_tx_vos, prev_RF_tx_vos;
	int16 iqval, tmp;
	uint16 old_test, old_rfovr, old_rfovrval, old_rx_pa, old_1st_rx,
		old_2nd_rx, old_1st_tx, old_adc_ctl, old_dac_ctl, old_rot_fac,
		old_tssi_en, old_tx_rf, old_tx_bb, old_tx_dc, old_rx_gm_updn;
	int iq_iter_ctr, ctr;
	int imin_idx, qmin_idx, imin_n, qmin_n, ictr, qctr;
	int16 imin_loft_list[MAX_VOS_COUNT], qmin_loft_list[MAX_VOS_COUNT];
	d11regs_t *regs = pi->regs;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	orig_channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(64));

	/* save off old state */
	old_test = R_REG(pi->sh->osh, &regs->phytest);
	old_rfovr = phy_reg_read(pi, APHY_RF_OVERRIDE);
	old_rfovrval = phy_reg_read(pi, APHY_RF_OVERRIDE_VAL);
	old_rx_pa = read_aphy_table_ent(pi, 0xf, 7);
	old_1st_rx = read_aphy_table_ent(pi, 0xf, 9);
	old_2nd_rx = read_aphy_table_ent(pi, 0xf, 0xa);
	old_1st_tx = read_aphy_table_ent(pi, 0xe, 0xb);
	old_adc_ctl = phy_reg_read(pi, APHY_RSSI_ADC_CTL);
	old_rot_fac = phy_reg_read(pi, APHY_ROTATE_FACT);
	old_tssi_en = phy_reg_read(pi, APHY_TSSI_TEMP_CTL);
	old_tx_bb = read_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN);
	old_dac_ctl = read_aphy_table_ent(pi, 0xc, 1);
	old_tx_rf = read_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN);
	old_tx_dc = phy_reg_read(pi, APHY_DC_BIAS);
	old_rx_gm_updn = read_radio_reg(pi, RADIO_2060WW_RX_GM_UPDN);

	/* set the engineering bit for 2060 13b mode to 0. This reduces
	 * the receive side gain and thus the the RX coupling into TX
	 */
	 and_radio_reg(pi, RADIO_2060WW_RX_GM_UPDN, ~0x40);

	/**************************************************************************
	 * Phase 1: Find i & q settings n radio that yield minimum output power.
	 * Algorithm:  Set q to 0, and iterate i from -15,+15 finding minimum i.
	 * Using that minimum i, iterate q from -15,+15 finding minimal power.
	 * The i & q we just found is the Phase 1 answer.
	 ***********************************************************************
	 */
	if ((cal_type == LOF_CAL_ALL_VOS) || (cal_type == LOF_CAL_SOME_VOS) ||
	    cal_type == LOF_CAL_VOS_AND_TX_BB) {

		/****************************************
		 * Prep work for iterating over i & q's.
		 **************************************
		 */
		/* 2060_init does not seem to clear reg(RF_tx_vos_ctl) */
		write_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL, 0);

		aphy_all_crs(pi, FALSE);

		/* Force I/Q imbalance compensation OFF */
		W_REG(pi->sh->osh, &regs->phytest, 1);

		/* Force tx_pu ON, rx_pu OFF
		 * Force PA_pd to 1
		 */
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_OR_RAW_ENTRY(APHY_RF_OVERRIDE, 0x0c08)
			PHY_REG_MOD_RAW_ENTRY(APHY_RF_OVERRIDE_VAL, 0x0c08, 0x0408)
		PHY_REG_LIST_EXECUTE(pi);

		/* Force PA output in RX mode to be the same as that in TX mode */
		tmp = read_aphy_table_ent(pi, 0xe, 8);
		WRITE_APHY_TABLE_ENT(pi, 0xf, 7, tmp);

		/* Force tx/rx power regs. such that tx is always ON
		 * set 1st rx command to tx/rx ON
		 */
		WRITE_APHY_TABLE_ENT(pi, 0xf, 9, 1);

		/* set 2nd rx command to rx OFF */
		WRITE_APHY_TABLE_ENT(pi, 0xf, 0xa, 1);

		/* set 1st tx command to tx ON */
		WRITE_APHY_TABLE_ENT(pi, 0xe, 0xb, 1);

		PHY_REG_LIST_START_WLBANDINITDATA
			/* Set TSSI measurement values
			 * TSSI init delay from txframe = 12 us
			 * TSSI delay between consecutive measurements = 1 usec
			 */
			PHY_REG_WRITE_RAW_ENTRY(APHY_RSSI_ADC_CTL, 0xc111)
			/* Enable TSSI measurement */
			PHY_REG_WRITE_RAW_ENTRY(APHY_TSSI_TEMP_CTL, 2)
		PHY_REG_LIST_EXECUTE(pi);

		if (AREV_IS(pi->pubpi.phy_rev, 2)) {
			/* Ensure RSSI LUT has correct mapping */
			aphy_rssi_lut_war(pi);
		}

		phy_reg_write(pi, APHY_DC_BIAS, 0);

		/* Fix TX BB gain and DAC attenuation to enable proper TSSI measurements */
		write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, 0x26);
		tmp = read_aphy_table_ent(pi, 0xc, 1);
		if (pi->pubpi.ana_rev == ANA_11G_018)
			tmp = (tmp & ~BBATT_11G_MASK) | (uint16)(2 << BBATT_11G_SHIFT);
		else
			tmp = (tmp & ~BBATT_11G_MASK2) | (uint16)(2 << BBATT_11G_SHIFT2);
		WRITE_APHY_TABLE_ENT(pi, 0xc, 1, tmp);

		/* Increase TX RF gain to enable proper TSSI measurements
		 * RF_tx_rf_gain = 9.5-9.5+2.1 = 2.1 dB
		 */
		write_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, 0x60);

		/* We can do either all values of i & q or just a partial
		 * list, depending on cal_type. When would we only do a partial?
		 */
		if (cal_type == LOF_CAL_ALL_VOS) {
			max_RF_tx_vos = sizeof(full_qmax_tx_vos_list);
			qmax_RF_tx_vos_list = full_qmax_tx_vos_list;
			imax_RF_tx_vos_list = full_imax_tx_vos_list;
		} else {
			max_RF_tx_vos = sizeof(partial_qmax_tx_vos_list);
			qmax_RF_tx_vos_list = partial_qmax_tx_vos_list;
			imax_RF_tx_vos_list = partial_imax_tx_vos_list;
		}

		min_RF_tx_vos = -1;
		max_tssi_accumval = -999999;

		/* Start with clear lists */
		imin_idx = 0;
		qmin_idx = 0;

		/*******************************************************
		 * FIRST LOOP
		 * Go through iVOS list and then through qVOS list.
		 * First calculate the min iVOS while holding qVOS to zero and
		 * find the iVOS yielding min power.
		 * Then hold iVOS at that place and vary qVOS to find min power.
		 *****************************************************
		 */

		for (iq_iter_ctr = 0; iq_iter_ctr < N_IQ_ITER; iq_iter_ctr++) {

			/* Even iterations => hunt for best iVOS .
			 * Odd iterations => hunt for best qVOS.
			 */
			if (iq_iter_ctr & 1) {
				/* Clear q list in prep to traverse q's */
				qmin_idx = 0;
				/* Iterate over q, while holding i */
				max_RF_tx_vos_list = qmax_RF_tx_vos_list;
				PHY_CAL(("wl%d: %s: Starting search for best RF q\n",
				         pi->sh->unit, __FUNCTION__));
			} else {
				/* Iterate over i, while holding q */
				max_RF_tx_vos_list = imax_RF_tx_vos_list;
				PHY_CAL(("wl%d: %s: Starting search for best RF i\n",
				         pi->sh->unit, __FUNCTION__));
			}
			/* Traverse list hunting for setting that produces lowest power */
			for (ctr = 0; ctr < max_RF_tx_vos; ctr++) {
				int val = max_RF_tx_vos_list[ctr]; 	/* next setting */

				/* previous setting */
				prev_RF_tx_vos = read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL);

				if (iq_iter_ctr & 1) {
					/* Load new q */
					write_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL,
					                val | (prev_RF_tx_vos & 0xf0));
				} else {
					/* Load new i */
					write_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL,
					                (val << 4) | (prev_RF_tx_vos & 0x0f));
				}

				OSL_DELAY(5);	/* Wait for radio write to finish */

				/*
				 * Send packets and measure total tssi (inverse of power).
				 */
				tssi_accumval = wlc_loft_collect_tssi(pi);

				PHY_TMP(("wl%d: %s: RF_TX_VOS = 0x%2x TSSI_accumval = %d\n",
				         pi->sh->unit, __FUNCTION__,
				         read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL),
				         tssi_accumval));

				/*
				 * Check for new max tssi (lowest power value)
				 */
				if (tssi_accumval > max_tssi_accumval) {
					/* Store winning register & tssi value */
					min_RF_tx_vos = read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL);
					max_tssi_accumval = tssi_accumval;

					PHY_CAL(("wl%d: %s: New RF winner: 0x%x, accum tssi = %d\n",
					         pi->sh->unit, __FUNCTION__,
					         min_RF_tx_vos, tssi_accumval));
					/*
					 * We have a new winner, so erase any previous winners.
					 */
					if (iq_iter_ctr & 1) {
						qmin_idx = 0;
						qmin_loft_list[qmin_idx++] = min_RF_tx_vos;
					} else {
						imin_idx = 0;
						imin_loft_list[imin_idx++] = min_RF_tx_vos;
					}
				} else if (tssi_accumval == max_tssi_accumval) {

					PHY_CAL(("wl%d: %s: New RF winner tie: 0x%x, accum tssi ="
					         " %d\n",
					         pi->sh->unit, __FUNCTION__,
					         read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL),
					         tssi_accumval));
					/*
					 * We have a tie.
					 * Multiple reg settings yield the same tssi readings.
					 * Save each of these reg settings.  We will resolve them
					 * below.
					 */
					if (iq_iter_ctr & 1)
						qmin_loft_list[qmin_idx++] =
						        read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL);
					else
						imin_loft_list[imin_idx++] =
						        read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL);
				}
			}

			/*
			 * We have now traversed the values of ether the i or q lists and found the
			 * vos reg setting that gives use lowest power (ie highest tssi). Stored
			 * in min_RF_tx_vos.
			 */
			PHY_TMP(("wl%d: %s: RF I/Q search minimum: RF_TX_VOS = 0x%2x, accum tssi ="
			         " %d\n",
			         pi->sh->unit, __FUNCTION__, min_RF_tx_vos, max_tssi_accumval));

			if (min_RF_tx_vos < 0) {
				PHY_ERROR(("wl%d: %s: NO Radio setting to minimize LO FT found for"
				          " reg(RF_tx_vos_ctl)",
				          pi->sh->unit, __FUNCTION__));
				ASSERT(min_RF_tx_vos >= 0);
			}
			/* Write the 'best' setting */
			write_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL, min_RF_tx_vos);
		}


		/********************************************************************************
		 * At this point we have traversed both lists of i & q settings, searching
		 * for the settings that give us lowest power.  There may have been
		 * several settings that gave identical minimal results.   Now choose which of
		 * those to use by increasing RF_tx_rf_gain and again searching for minimal
		 * power.
		 *******************************************************************************
		 */

		/*
		 * Increase gain: RF_tx_rf_gain = 13-0.8+5.6 = 17.8 dB
		 */
		write_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, 0x91);

		/* Its possible that no q's gave us minimal power, so set up
		 * a default list of q.
		 */
		if (qmin_idx == 0)
			qmin_loft_list[qmin_idx++] = 0;

		/* Sanity check lists */
		imin_n = MIN(imin_idx, N_LOFTMIN);
		qmin_n = MIN(qmin_idx, N_LOFTMIN);

		init_min_RF_tx_vos = min_RF_tx_vos;
		init_max_tssi_accumval = max_tssi_accumval;
		min_RF_tx_vos = -1;
		max_tssi_accumval = -999999;

		/* SECOND LOOP */
		/*
		 * Resolve tiebreakers.  Run through combinations of all previous winner
		 * to see who wins with increased RF_gain.
		 */
		for (ictr = 0; ictr < imin_n; ictr++) {
			for (qctr = 0; qctr < qmin_n; qctr++) {
				/*
				 * Set current vos setting
				 */
				iqval = imin_loft_list[ictr] & 0xf0;
				iqval |= qmin_loft_list[qctr] & 0x0f;
				write_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL, iqval);
				OSL_DELAY(5);

				/*
				 * Collect the power for this radio setting
				 */
				tssi_accumval = wlc_loft_collect_tssi(pi);
				PHY_CAL(("wl%d: %s: Evaluating RF Tiebreaker: VOS = 0x%2x, accum"
				         " tssi = %d\n",
				         pi->sh->unit, __FUNCTION__,
				         read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL),
				         tssi_accumval));

				/* Found a new winner - store reg and tssi values */
				if (tssi_accumval > max_tssi_accumval) {
					min_RF_tx_vos = read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL);
					max_tssi_accumval = tssi_accumval;
					PHY_CAL(("wl%d: %s: RF Tiebreaker: New winner: RF_TX_VOS ="
					         " 0x%2x\n",
					         pi->sh->unit, __FUNCTION__,
					         read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL)));
				}
			}
		}

		/* Sanity check: Retain the min setting from the initial search
		 * just in case we have no results.
		 */
		if (min_RF_tx_vos < 0) {
			min_RF_tx_vos = init_min_RF_tx_vos;
			max_tssi_accumval = init_max_tssi_accumval;
		}

		/*************************************************
		 * We now have final radio settings.  Set & store.
		 *************************************************
		 */
		write_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL, min_RF_tx_vos);
		pi_abg->tx_vos = min_RF_tx_vos;

		/* We normally do cal_type 3 */
		if (cal_type == 0 || cal_type == 2) {
			PHY_TMP(("wl%d: %s: Final minimum: RF_TX_VOS = 0x%x, MAX_TSSI_accumval ="
			         " %d\n",
			         pi->sh->unit, __FUNCTION__, min_RF_tx_vos, max_tssi_accumval));

			/* Restore old register values */
			W_REG(pi->sh->osh, &regs->phytest, old_test);
			phy_reg_write(pi, APHY_RF_OVERRIDE, old_rfovr);
			phy_reg_write(pi, APHY_RF_OVERRIDE_VAL, old_rfovrval);
			WRITE_APHY_TABLE_ENT(pi, 0xc, 1, old_dac_ctl);
			write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, old_tx_bb);
			write_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, old_tx_rf);
			phy_reg_write(pi, APHY_DC_BIAS, old_tx_dc);
			/* OSL_DELAY(5000); */
			WRITE_APHY_TABLE_ENT(pi, 0xf, 7, old_rx_pa);
			WRITE_APHY_TABLE_ENT(pi, 0xf, 9, old_1st_rx);
			WRITE_APHY_TABLE_ENT(pi, 0xf, 0xa, old_2nd_rx);
			WRITE_APHY_TABLE_ENT(pi, 0xe, 0xb, old_1st_tx);
			phy_reg_write(pi, APHY_RSSI_ADC_CTL, old_adc_ctl);
			phy_reg_write(pi, APHY_ROTATE_FACT, old_rot_fac);
			phy_reg_write(pi, APHY_TSSI_TEMP_CTL, old_tssi_en);
			if (AREV_IS(pi->pubpi.phy_rev, 2)) {
				aphy_rssi_lut_war(pi);
			}
			/* make sure new 2060/RF controls are activated by issuing a transmit */
			wlc_phy_do_dummy_tx(pi, TRUE, ON);
			aphy_all_crs(pi, TRUE);
		}
	}
	/********************************************************
	 * End of cal_type 0/2/3: Search over tx_vos_ctl register
	 *******************************************************
	 */

	 /* We now have the min iVOS and min qVOS */
	PHY_CAL(("wl%d: %s: Final RF_TX_VOS = 0x%x\n", pi->sh->unit, __FUNCTION__, pi_abg->tx_vos));


	/********************************************************
	 * Start of cal_type 1/3: Search over bb_tx_dc_bias
	 * Currently valid only for reg(RF_tx_bb_gain) == 0x26
	 * DAC attenuation in 0.5 dB/tick
	 * Phase 3.
	 *******************************************************
	 */

	if (cal_type == 1 || cal_type == 3) {
		uint16 dac_ctrl, min_bb_tx_dc, init_min_bb_tx_dc, prev_bb_tx_dc;
		uint16 ival, qval;
		bool change_flag;

		PHY_TMP(("wl%d: %s: Start initial search for Lowest LO feedthrough over BB TX DC"
		         " Bias register\n",
		         pi->sh->unit, __FUNCTION__));

		/* initial value */
		phy_reg_write(pi, APHY_DC_BIAS, 0);

		/* Fix TX BB gain to enable proper TSSI measurements */
		write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, 0x26);

		dac_ctrl = read_aphy_table_ent(pi, 0xc, 1);
		if (pi->pubpi.ana_rev <= ANA_11G_018)
			dac_ctrl = (dac_ctrl & ~BBATT_11G_MASK) | (uint16)(11 << BBATT_11G_SHIFT);
		else
			dac_ctrl = (dac_ctrl & ~BBATT_11G_MASK2) | (uint16)(11 << BBATT_11G_SHIFT2);
		WRITE_APHY_TABLE_ENT(pi, 0xc, 0x1, dac_ctrl);

		/* Set TX RF gain to enable proper TSSI measurements
		 * RF_tx_rf_gain = 3.8-9.5+2.1 = -3.6 dB  0x20
		 * RF_tx_rf_gain = 9.5-9.5+2.1 =  2.1 dB  0x60
		 * RF_tx_rf_gain = 9.5-9.5+5.6 =  5.6 dB  0x61
		 * RF_tx_rf_gain = 9.5-9.5+7.0 =  7.0 dB  0x62
		 * RF_tx_rf_gain = 13-0.8+5.6  = 17.8 dB  0x91
		 * RF_tx_rf_gain = 13-0.8+7.0  = 19.2 dB  0x92
		 * RF_tx_rf_gain = 13-0.8+8.9  = 21.1 dB  0x93
		 * RF_tx_rf_gain = 13-0.8+10.6 = 22.8 dB  0x94
		 * RF_tx_rf_gain = 13+1.5+10.6 = 25.1 dB  0x9c
		 */

		min_bb_tx_dc = 0;
		max_tssi_accumval = -999999;

		/*****************************************************
		 * Phase 3: Loop over i & q in Baseband reg
		 *	 1. go over i, with q == 0 and find best i
		 *	 2. with minima i, go over q and find minima q
		 ****************************************************
		 */

		qmin_idx = imin_idx = 0;
		for (iq_iter_ctr = 0; iq_iter_ctr < N_IQ_ITER; iq_iter_ctr++) {
			/* Clear the qmin list before going over qBB */
			if (iq_iter_ctr == 1) {
				qmin_idx = 0;
				PHY_CAL(("wl%d: %s: Starting search for optimum BB q\n",
				         pi->sh->unit, __FUNCTION__));
			} else {
				PHY_CAL(("wl%d: %s: Starting search for optimum BB i\n",
				         pi->sh->unit, __FUNCTION__));
			}

			/* start with rf_gain = 0x92
			 * if max accum_TSSI < -250 and aphy_rev <= 2, drop rf_gain and try again --
			 * 8-bit TSSI
			 * if max accum_TSSI < -61 and aphy_rev >= 3, drop rf_gain and try again --
			 * 6-bit TSSI
			 */
			if (AREV_IS(pi->pubpi.phy_rev, 2)) {
				min_good_tssi = -250;
			} else {
				min_good_tssi = -61;
			}

			rf_vec_index = 5; /* Start out at 0x92 */

			do {
				write_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, rf_vec[rf_vec_index]);

				/* Traverse from -15 up to 15 and write to iBB and qBB */
				for (ctr = -15; ctr <= 15; ctr++) {
					/* if ctr is negative then add 0x100 to it to make sure
					 * it is 8 bit sign extended.
					 */
					regval = ctr;
					if (ctr < 0)
						regval += 0x100;

					prev_bb_tx_dc = phy_reg_read(pi, APHY_DC_BIAS);
					if (iq_iter_ctr & 1) {
						/* Change q */
						phy_reg_write(pi, APHY_DC_BIAS, regval |
						              (prev_bb_tx_dc & 0xff00));
					} else {
						/* Change i */
						phy_reg_write(pi, APHY_DC_BIAS,
						              ((regval << 8) |
						               (prev_bb_tx_dc & 0x00ff)));
					}

					/* collect the power value for this ibb and qbb setting */
					tssi_accumval = wlc_loft_collect_tssi(pi);

					PHY_TMP(("wl%d: %s: Accum: BB_TX_DC = 0x%2x TSSI_accumval"
					         " = %d\n", pi->sh->unit, __FUNCTION__,
					         phy_reg_read(pi, APHY_DC_BIAS),
					         tssi_accumval));

					/* New winner */
					if (tssi_accumval > max_tssi_accumval) {
						min_bb_tx_dc = phy_reg_read(pi, APHY_DC_BIAS);
						max_tssi_accumval = tssi_accumval;

						PHY_TMP(("wl%d: %s: New BB winner: BB_TX_DC = "
						         "0x%2x accum tssi = %d\n",
						         pi->sh->unit, __FUNCTION__,
						         phy_reg_read(pi, APHY_DC_BIAS),
						         max_tssi_accumval));

						/*
						 * We have a new winner, so erase all previous
						 * winners.
						 */
						if (iq_iter_ctr & 1) {
							qmin_idx = 0;
							qmin_loft_list[qmin_idx++] =
							       phy_reg_read(pi, APHY_DC_BIAS);
						} else {
							imin_idx = 0;
							imin_loft_list[imin_idx++] =
							       phy_reg_read(pi, APHY_DC_BIAS);
						}
					} else {
						/*
						 * We have a tie.  Multiple reg settings yield the
						 * same tssi readings.
						 * Save each of these reg settings.  Resolve them
						 * below.
						 */
						if (tssi_accumval == max_tssi_accumval) {
							PHY_CAL(("wl%d: %s: New BB winner tie: "
							         "BB_TX_DC = 0x%2x accum tssi ="
							         " %d\n",
							         pi->sh->unit, __FUNCTION__,
							         phy_reg_read(pi,
							                      APHY_DC_BIAS),
							         max_tssi_accumval));
							if (iq_iter_ctr & 1)
								qmin_loft_list[qmin_idx++] =
								        phy_reg_read(pi,
								                     APHY_DC_BIAS);
							else
								imin_loft_list[imin_idx++] =
								        phy_reg_read(pi,
								                     APHY_DC_BIAS);
						}
					}
				}
				/*
				 * Now we have a list of i and q that seems to work for a
				 * tx_rf_tx gain of 0x92. If max tssi is less than reliable
				 * value that means there is too much power so reduce the gain.
				 */
				if (max_tssi_accumval < min_good_tssi) {
					PHY_TMP(("wl%d: %s: rf gain (0x%x) is to high, try"
					         " again.\n", pi->sh->unit, __FUNCTION__,
					         rf_vec[rf_vec_index]));
					rf_vec_index--;

					/*  Clear imin_idx and qmin_idx appropriately. We need this
					 *  since when we repeat LOFT search with lower RF gains,
					 *  the imin/qmin_loft_list arrays can be written
					 *  out-of-bounds trashing memory and causing BSOD
					 */
					if ((iq_iter_ctr == 0) && (rf_vec_index >= 0)) {
						imin_idx = 0;
					} else if ((iq_iter_ctr == 1) && (rf_vec_index >= 0)) {
						qmin_idx = 0;
					}
				} else {
					PHY_TMP(("wl%d: %s: Keeping good rf_gain of 0x%x\n",
					         pi->sh->unit, __FUNCTION__,
					         rf_vec[rf_vec_index]));
				}
			} while ((max_tssi_accumval < min_good_tssi) && (rf_vec_index >= 0));

			if (max_tssi_accumval < min_good_tssi) {
				PHY_ERROR(("wl%d: %s: Could not find good rf_gain\n",
				          pi->sh->unit, __FUNCTION__));
			}

			/* found the value which works for tx_rf_tx for 0x92 */
			phy_reg_write(pi, APHY_DC_BIAS, min_bb_tx_dc);

			PHY_TMP(("wl%d: %s: BB I/Q winner: BB_TX_DC = 0x%2x, accum tssi = %d\n",
			         pi->sh->unit, __FUNCTION__, phy_reg_read(pi, APHY_DC_BIAS),
			         max_tssi_accumval));
		}
		PHY_TMP(("wl%d: %s: Start final search for Lowest LO feedthrough over BB TX DC "
		         "Bias control register\n", pi->sh->unit, __FUNCTION__));


		/***************************************************************
		 * Phase 4: Tiebreaker resolution for BB. Increase RF_tx_rf_gain
		 * and search through winners looking for new winner.
		 ************************************************************
		 */

		/*************************************************
		 *  Increase RF_tx_rf_gain
		 * RF_tx_rf_gain = 9.5-9.5+2.1 =  2.1 dB 0x60
		 * RF_tx_rf_gain = 9.5-9.5+5.6 =  5.6 dB 0x61
		 * RF_tx_rf_gain = 9.5-9.5+7.0 =  7.0 dB 0x62
		 * RF_tx_rf_gain = 13-0.8+5.6  = 17.8 dB 0x91
		 * RF_tx_rf_gain = 13+1.5+7.0  = 21.5 dB 0x9a
		 * RF_tx_rf_gain = 13+1.5+8.9  = 23.4 dB 0x9b
		 * RF_tx_rf_gain = 13+1.5+10.6 = 25.1 dB 0x9c
		 *************************************************
		 */
		rf_vec_index = 5; /* Start out rf_vec1[5] == 0x9b */

		do {
			/* write new tx_rf gain */
			write_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, rf_vec1[rf_vec_index]);

			/* Its possible that no q's gave us minimal power, so set up
			 * a default list of q.
			 */
			if (qmin_idx == 0)
				qmin_loft_list[qmin_idx++] = 0;

			imin_n = MIN(imin_idx, N_LOFTMIN);
			qmin_n = MIN(qmin_idx, N_LOFTMIN);

			PHY_TMP(("wl%d: %s: imin_loft_list len = %d  qmin_loft_list len = %d\n",
			         pi->sh->unit, __FUNCTION__, imin_n, qmin_n));

			init_min_bb_tx_dc = min_bb_tx_dc;
			init_max_tssi_accumval = max_tssi_accumval;
			change_flag = FALSE;
			max_tssi_accumval = -999999;

			/*
			 * Traverse list of min iBB and min qBB settings
			 */
			for (ictr = 0; ictr < imin_n; ictr++) {
				for (qctr = 0; qctr < qmin_n; qctr++) {
					ival = imin_loft_list[ictr] & 0xff00;
					qval = qmin_loft_list[qctr] & 0xff;

					/* write the settings in the aphy_dc register */
					phy_reg_write(pi, APHY_DC_BIAS, (ival | qval));

					/* collect the power for this setting */
					tssi_accumval = wlc_loft_collect_tssi(pi);

					PHY_TMP(("wl%d: %s: Evaluating Tiebreaker BB: "
					         "BB_TX_DC = 0x%2x  accum tssi = %d\n",
					         pi->sh->unit, __FUNCTION__,
					         phy_reg_read(pi, APHY_DC_BIAS),
					         tssi_accumval));

					/* New winner */
					if (tssi_accumval > max_tssi_accumval) {
						min_bb_tx_dc = phy_reg_read(pi, APHY_DC_BIAS);
						change_flag = TRUE;
						max_tssi_accumval = tssi_accumval;
						PHY_TMP(("wl%d: %s: BB Tiebreaker winner: "
						         "BB_TX_DC = 0x%2x, accum tssi = %d\n",
						         pi->sh->unit, __FUNCTION__,
						         phy_reg_read(pi, APHY_DC_BIAS),
						         max_tssi_accumval));
					}
				}
			}

			/*
			 * If max tssi is not good enough then reduce the gain
			 * and try again.
			 */
			if (max_tssi_accumval < min_good_tssi) {
				PHY_TMP(("wl%d: %s: 2nd loop RF gain (0x%x) too high try again\n",
				         pi->sh->unit, __FUNCTION__, rf_vec1[rf_vec_index]));
				rf_vec_index--;
			}

		} while ((max_tssi_accumval < min_good_tssi) && (rf_vec_index >= 0));

		if (change_flag == FALSE) {
			/* Retain the min setting from the initial search */
			min_bb_tx_dc = init_min_bb_tx_dc;
			max_tssi_accumval = init_max_tssi_accumval;
		}

		PHY_TMP(("wl%d: %s: Final LOFT minimum: BB_TX_DC = 0x%2x  MAX_TSSI_accumval = %d\n",
		         pi->sh->unit, __FUNCTION__, min_bb_tx_dc, max_tssi_accumval));

		/* Restore old register values */
		W_REG(pi->sh->osh, &regs->phytest, old_test);
		phy_reg_write(pi, APHY_RF_OVERRIDE, old_rfovr);
		phy_reg_write(pi, APHY_RF_OVERRIDE_VAL, old_rfovrval);
		WRITE_APHY_TABLE_ENT(pi, 0xc, 1, old_dac_ctl);
		write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, old_tx_bb);
		write_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, old_tx_rf);
		phy_reg_write(pi, APHY_TX_COMP_OFFSET, min_bb_tx_dc);

		pi_abg->global_tx_bb_dc_bias_loft = min_bb_tx_dc;	/* Store the final winner */

		phy_reg_write(pi, APHY_DC_BIAS, old_tx_dc);
		OSL_DELAY(10);
		WRITE_APHY_TABLE_ENT(pi, 0xf, 7, old_rx_pa);
		WRITE_APHY_TABLE_ENT(pi, 0xf, 9, old_1st_rx);
		WRITE_APHY_TABLE_ENT(pi, 0xf, 0xa, old_2nd_rx);
		WRITE_APHY_TABLE_ENT(pi, 0xe, 0xb, old_1st_tx);
		phy_reg_write(pi, APHY_RSSI_ADC_CTL, old_adc_ctl);
		phy_reg_write(pi, APHY_ROTATE_FACT, old_rot_fac);
		phy_reg_write(pi, APHY_TSSI_TEMP_CTL, old_tssi_en);

		if (AREV_IS(pi->pubpi.phy_rev, 2))
			aphy_rssi_lut_war(pi);

		/* make sure new 2060/RF controls are activated by issuing a transmit */
		wlc_phy_do_dummy_tx(pi, TRUE, ON);
		aphy_all_crs(pi, TRUE);
	}

	PHY_CAL(("wl%d: %s: Final RF_TX_VOS = 0x%x, Final BB_TX_DC = 0x%2x\n",
	         pi->sh->unit, __FUNCTION__, pi_abg->tx_vos, pi_abg->global_tx_bb_dc_bias_loft));

	write_radio_reg(pi, RADIO_2060WW_RX_GM_UPDN, old_rx_gm_updn);

	wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(orig_channel));
}

static void
aphy_set_tx_iq_based_on_vos(phy_info_t *pi, bool iq_rebal_flag)
{
	uchar imax_RF_tx_vos_list[] = {0x0, 0x4, 0x8, 0x9, 0xd};
	uchar qmax_RF_tx_vos_list[] = {0x0, 0x1, 0x5, 0x6, 0xa};
	uint16 opt_iq_rebal_list[] = {0x00c0, 0xffc0, 0xfec0, 0xfdc0, 0xfcc0,
		0x01c0, 0x00c0, 0xffc0, 0xfec0, 0xfdc0,
		0x02c0, 0x00c0, 0x00c0, 0xfec0, 0xfec0,
		0x03c0, 0x02c0, 0x01c0, 0x00c0, 0xffc0,
		0x04c0, 0x03c0, 0x02c0, 0x01c0, 0x00c0};
	int max_rf_tx_vos = sizeof(imax_RF_tx_vos_list);
	int ictr, qctr, ctr;
	uchar tx_vos_cand, ival, qval;
	uint16 tx_vos_val;

	tx_vos_val = read_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL);
	for (ictr = 0; ictr < max_rf_tx_vos; ictr++) {
		for (qctr = 0; qctr < max_rf_tx_vos; qctr++) {
			ival = imax_RF_tx_vos_list[ictr];
			qval = qmax_RF_tx_vos_list[qctr];
			tx_vos_cand = (ival << 4) | (qval & 0x0f);

			if (tx_vos_val == tx_vos_cand) {
				ctr = ictr * max_rf_tx_vos + qctr;
				phy_reg_write(pi, APHY_TX_COMP_COEFF, opt_iq_rebal_list[ctr]);
			}
		}
	}
	PHY_TMP(("wl%d: %s: TX IQ rebalance reg 0x69 = 0x%x\n",
	         pi->sh->unit, __FUNCTION__, phy_reg_read(pi, APHY_TX_COMP_COEFF)));
}

/*
 * For a given gain setting (rf,bb attens and padmix gain [txctl1]),
 * find rf and bb indexes for accessing the baseband lo comp table
 */
static void
gphy_complo_map_txpower(phy_info_t *pi, int rf_attn, int bb_attn, uint16 txctl1,
	int *prfgainid, int *pbbattnid)
{
	int padmix_en;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	PHY_TMP(("map: rf_attn = %d, global_bb_attn = %d, txctl1 = 0x%x\n",
	         rf_attn, bb_attn, txctl1));
	padmix_en = ((txctl1 == 0) ? 0 : 1);

	/* 0 <= rf_attn <= pi_abg->rf_max */
	rf_attn = MAX(0, rf_attn);
	rf_attn = MIN(pi_abg->rf_max, rf_attn);

	/* 0 <= bb_attn <= pi_abg->bb_max */
	bb_attn = MAX(0, bb_attn);
	bb_attn = MIN(pi_abg->bb_max, bb_attn);

	PHY_TMP(("map: rf_attn = %d, bb_attn = %d, padmix_en = 0x%x pi_abg->rf_max: %d\n",
	         rf_attn, bb_attn, padmix_en, pi_abg->rf_max));

	if ((rf_attn < 1 || rf_attn > pi_abg->rf_max) && padmix_en == 1)
		rf_attn = pi_abg->rf_max;

	*prfgainid = PHY_GET_RFGAINID(rf_attn, padmix_en, pi_abg->rf_max);
	*pbbattnid = bb_attn;

	PHY_TMP(("map power: prfgainid 0x%x, pbbattnid 0x%x\n", *prfgainid, *pbbattnid));

	ASSERT((*prfgainid >= 0) && (*prfgainid < STATIC_NUM_RF));
	ASSERT((*pbbattnid >= 0) && (*pbbattnid < STATIC_NUM_BB));
}

/*
 * For a given gain setting (rf,bb attens and padmix gain [txctl1]),
 * write the baseband LO compensation DC offsets to the DC offset phy reg.
 */
static void
gphy_complo(phy_info_t *pi, int rf_attn, int bb_attn, uint16 txctl1)
{
	int rfgainid, bbattnid;
	abgphy_lo_complex_info_t min_pos;
	uint16 minset;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	gphy_complo_map_txpower(pi, rf_attn, bb_attn, txctl1, &rfgainid, &bbattnid);
	min_pos = pi_abg->gphy_locomp_iq[rfgainid][bbattnid];
	minset = ((min_pos.i & 0xff) << 8) | (min_pos.q & 0xff);

	PHY_CAL(("wl%d: %s: Using rf %d, bb %d, secret %d => 0x%x; reg 0x52 = 0x%x\n",
	         pi->sh->unit, __FUNCTION__, rf_attn, bb_attn, txctl1,
	         minset, read_radio_reg(pi, 0x52)));

	phy_reg_write(pi,
	              ISGPHY(pi) ? GPHY_DC_OFFSET2 : BPHY_TX_DC_OFF2, minset);
}

/* ------------------------------------------------------------------------
 * gphy_measurelo, lo-leakage procedure called for gphy and 4318
 *
 *   There are 3 primary routines (presented in the following order):
 *     gphy_measurelo_iter() to compute leakage power for given tx settings.
 *     gphy_measurelo_gains() to determine rx path loopback gains
 *     wlc_phy_cal_gphy_measurelo(): Main Routine
 * ------------------------------------------------------------------------
 *
 * For GPHY rev (=2, 4306 C0), [=3, 4318a0]:
 * -----------------------------------------
 * phyreg 0x811: gphy rfOverrideAddress (GPHY_RF_OVERRIDE)
 * [  11] ext_lna_gain_ovr
 * [  10] tr_ant_sel_from_aphy_ovr
 * [   9] tx_pu_muxselaphy_ovr
 * (   8) agc_ctl_ovr
 * (   7) synth_pu_ovr
 * (   6) ant_selp_ovr
 * (   5) trsw_rx_pu_ovr
 * (   4) trsw_tx_pu_ovr
 * (   3) rx_pu_ovr
 * (   2) tx_pu_ovr
 * (   1) fltr_rx_ctl_lpf_ovr
 * (   0) fltr_rx_ctl_ovr
 *
 * phyreg 0x812: gphy rfOverridevalAddress (GPHY_RF_OVERRIDE_VAL)
 * [  15] ext_lna_gain_ovr_val
 * [  14] tx_pu_mux_selaphy_ovr_val
 * (13:8) agc_ctl_ovr_val
 * (   7) synth_pu_ovr_val
 * (   6) ant_selp_ovr_val
 * (   5) trsw_rx_pu_ovr_val
 * (   4) trsw_tx_pu_ovr_val
 * (   3) rx_pu_ovr_val
 * (   2) tx_pu_ovr_val
 * (   1) fltr_rx_ctl_lpf_ovr_val
 * (   0) fltr_rx_ctl_ovr_val
 *
 * phyreg 0x015 bphy rfOverride (BPHY_RF_OVERRIDE)
 * (  15) rx_pu_ovr
 * (  14) rx_pu_ovr_val
 * (  13) tx_pu_ovr
 * (  12) tx_pu_ovr_val
 * (  11) synth_pu_ovr
 * (  10) synth_pu_ovr_val
 * (   9) pa_ramp_ovr
 * (   8) pa_ramp_ovr_val
 * (   7) fltr_rx_ctrl_ovr
 * (   6) fltr_rx_ctrl_ovr_val
 * (   5) agc_ovr
 * ( 4:0) agc_ovr_val
 */

/* Compute an iteration of "IQ accum"
 * Accumulate abs(I)+abs(Q) data captured by the 2050
 */
static uint16
gphy_measurelo_iter(phy_info_t *pi, bool gmode, int TRSW_RX, int lnaval, int pgaval)
{
	uint16     reg812_hibw, reg812_lobw, reg812_lobw_lpf;
	int        k;
	int        extlna, niter, iqaccum;

	/* rx_pu is high always, (otherwise DC canceller bandwidth toggling
	 * is moot); synth_pu is high always; pa_ramp is high always;
	 * both trsw_rx_pu and trsw_tx_pu high to couple tx path back to rx
	 * path for LO measurement
	 *
	 * for gmode 1, only leave tx_pu, rx_pu, and pa_ramp controlled by
	 * bphy rfOverride, simply because the bphy IQ accum triggering
	 * mechanism needs to be coupled; leave all else to gphy rfOverride
	 */

	if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
	    (GREV_GE(pi->pubpi.phy_rev, 7))) {
		extlna = 0x8000;
	} else {
		extlna = 0;
	}
	niter   = 1;
	iqaccum = 0;

	for (k = 0; k < niter; k++) {
		if (gmode) {
			/* first, toggle tx_pu low to end previous tx LO measurement, and
			 * toggle fltr_rx_ctrl and fltr_rx_ctrl_lpf low (high BW mode)
			 * to flush any prior rx DC ("filter refresh cycle")
			 * -------------------------------------------------------------
			 */
			reg812_hibw = extlna | (lnaval << 12) | (pgaval << 8)
				| TRSW_RX | 0x0010;

			reg812_lobw_lpf = reg812_hibw | 0x2;
			reg812_lobw     = reg812_hibw | 0x3;
			phy_reg_write(pi, BPHY_RF_OVERRIDE,     0xe300);
			phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, reg812_hibw);

			/* second, raise fltr_rx ctrls high (slow mode) so that any rx DC
			 * from next tx LO will be slow to decay and can be measured.
			 * -------------------------------------------------------------
			 */
			OSL_DELAY(10);
			phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, reg812_lobw_lpf);
			OSL_DELAY(10);
			phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, reg812_lobw);

			/* finally, raise tx_pu high, tx LO begins again, and since rx_pu
			 * and tx_pu are both high, BPHY RX IQ accum will be collected
			 * after delay trig_delay, for length cap_len samples.
			 * -------------------------------------------------------------
			 */
			OSL_DELAY(10);
			phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xf300);

		} else {
			/* first, toggle tx_pu fltr_rx_ctrl and fltr_rx_ctrl_lpf low
			 * second, raise fltr_rx ctrls high (slow mode)
			 * finally, raise tx_pu high, tx LO begins again
			 * -------------------------------------------------------------
			 */
			phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xefa0 | pgaval);
			OSL_DELAY(10);
			phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xefe0 | pgaval);
			OSL_DELAY(10);
			phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xffe0 | pgaval);
		}

		/* read result from BPHY RX IQ accum
		 * delay:
		 *   length of capture (typ=16us) (see reg 0x2b: cap_len)
		 *   trigger delay (typ 3us) (see reg 0x2b)
		 *   2us guard time
		 * ------------------------------------------------------
		 */
		OSL_DELAY(16 + 3 + 2);
		iqaccum = iqaccum + phy_reg_read(pi, BPHY_LO_IQMAG_ACC);
	}

	return ((uint16)iqaccum);
}

/* For a desired overall RX path gain (spec'd by input max_rx_gain)
 * determine optimum RX gain settings (lna_load, lna, pga).
 * Also, account for TRSW_RX "gain" if it is enabled.
 * This routine actually sets/clears the lnalod register bit.
 */
static void
gphy_measurelo_gains(phy_info_t *pi, int max_rx_gain, bool USE_TRSW_RX,
	int *pTRSW_RX, int *plnalod, int *plnaval, int *ppgaval)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	int TRSW_RX = 0, lnalod = 0, lnaval = 0, pgaval = 0;
	int TRSW_RX_GAIN = 0;
	int adj_rx_gain;

	max_rx_gain = MAX(0, max_rx_gain);

	/* Distribute gain to TR switch_RX, lnalod, lna and pgaval */
	if (pi_abg->loopback_gain) {

		TRSW_RX     = 0x00;
		adj_rx_gain = max_rx_gain;

		if (USE_TRSW_RX) {
			TRSW_RX_GAIN = pi_abg->trsw_rx_gain_hdB / 2;
			if (adj_rx_gain >= TRSW_RX_GAIN) {
				TRSW_RX     = 0x20;
				adj_rx_gain = adj_rx_gain - TRSW_RX_GAIN;
			}
		}

		/* Set lnalod=1 whenever remaining gain > 8dB */
		lnalod = 0;
		if (adj_rx_gain > 8) {
			lnalod      = 1;
			adj_rx_gain = adj_rx_gain - 8;
		}
		adj_rx_gain = MIN(45, MAX(0, adj_rx_gain));
		pgaval      = adj_rx_gain / 3;

		/* When pgaval (gain setting) is greater than 5 (5x3=15dB),
		 * decrease pgaval by 5 and set lnaval to 2 (15dB lna gain)
		 */
		if (pgaval >= 5) {
			pgaval = pgaval - 5;
			lnaval = 2;
		}

	} else {
		/* (should get here only for 4306b0, bphy, ...)
		 * Note only lnalod and pga are used for gain adjustment
		 * The TR switch_RX is always enabled
		 */
		TRSW_RX = 0x20;
		if (max_rx_gain >= 20) {
			lnalod = 1;
			pgaval = 2;
		} else if (max_rx_gain >= 18) {
			lnalod = 1;
			pgaval = 1;
		} else if (max_rx_gain >= 15) {
			lnalod = 1;
			pgaval = 0;
		} else {
			lnalod = 0;
			pgaval = 0;
		}
	}

	/* LNA load bit(3): set for less rx attn, clear for more rx attn */
	if (lnalod) {
		mod_radio_reg(pi, RADIO_2050_RX_CTL0, 0x08, 0x08);
	} else {
		mod_radio_reg(pi, RADIO_2050_RX_CTL0, 0x08, ~0x08);
	}

	PHY_CAL(("max_rx_gain= %d, TRSW_RX= %x (TRSW_RX_GAIN= %d)  LNALD= %d, LNA= %d PGA= %d\n",
	         max_rx_gain, TRSW_RX, TRSW_RX_GAIN, lnalod, lnaval, pgaval));

	/* Return computed gain settings */
	/* ----------------------------- */
	*pTRSW_RX = TRSW_RX;
	*plnalod  = lnalod;
	*plnaval  = lnaval;
	*ppgaval  = pgaval;
}

/* Main Routine for G-MODE LO leakage compensation
 * This routine invokes
 *    gphy_measurelo_gains() to determine rx path loopback gains
 *    gphy_measurelo_iter() to compute leakage power for given tx settings.
 */
void
wlc_phy_cal_gphy_measurelo(phy_info_t *pi)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	d11regs_t *regs = pi->regs;

	uint16 old_bbcfg, old_tx_tst, old_brfovr, old_trloss_ctl, old_sync_ctl,
		old_rx_ctl0, old_pwr_ctl, old_dac_ctl,
		old_corr_str0_shift_din = 0, old_classifyctl = 0;
	uint16 old_gphy_analog_ovr = 0, old_gphy_analog_ovr_val = 0,
		old_gphy_rf_ovr = 0, old_gphy_rf_ovr_val = 0;
	uint16 old_peakCnt = 0, old_crsth = 0, old_tx_ctl1_high_nibble = 0;
	uint16 old_phyreg80f = 0, old_phyreg801 = 0, old_phyreg3e = 0,
		old_phyreg60 = 0, old_phyreg14 = 0, old_phyreg478 = 0;
	uint16 dc_offset1_reg = 0, dc_offset1_val = 0, dc_offset2_reg = 0;
	uint16 dac_ctrl_reg = 0;

	int16 rfattn_mixr = 0, padmix_gain = 0, max_rx_gain = 0;
	int16 mintxbias, valtxdc, rf_attn, bb_attn;
	int stepcnt, meascnt, min_idx, rf_ind;
	int rfsize, bbsize;
	int ii;

	uint16 txbias, padmix_en;
	uint16 txbias_list[] = {9, 8, 10,  1, 0, 2,  5, 4, 6};
	uint   maglist_idx, txbias_idx;
	uint16 mintxmag = 0, reg811_cntl = 0;
	uint16 txmag;
	uint32 iqaccum, miniqaccum, min_val = 0;

	int j, z, calmode = -1;
	int8 i, q;
	bool gmode, is_initial;
	abgphy_lo_complex_info_t cur_pos, min_pos, sav_pos, *cur_stp;

	bool USE_TRSW_RX;
	uint NPASSES, INITIAL_GRID, IQ_LOTHD, IQ_HITHD, PGASTEP = 0;
	int TRSW_RX;
	int totattn, lnalod, lnaval, pgaval, done, orig_channel, maxRFAttnCode = 0;
	int padmix_mixr, bbattn_mixr, lnalod_mixr, lnaval_mixr, pgaval_mixr;
	int bkoffdB, bkoffdB_mixr, LPBACK_GAIN = 0;

	/* BB IQ delta position tables for Baseband LO Compensation
	 * Entries are { I_delta Q_delta } relative to position { 0 0 }
	 * Delta entries {0 0} cause previously computed position to be skipped.
	 * Each step_x is for a new "min position" relative to min pos for step_0
	 * (ie, step_1 is for min pos at { 1  1} relative to {0 0} for step_0,
	 * (ie, step_2 is for min pos at { 1  0} relative to {0 0} for step_0,
	 * (ie, step_3 is for min pos at { 1 -1} relative to {0 0} for step_0,...)
	 */
	static abgphy_lo_complex_info_t steps[NUM_NEIGHBORS + 1][NUM_NEIGHBORS] = {
		{{ 1, 1}, { 1, 0}, { 1, -1}, { 0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, { 0, 1}},
		{{ 1, 1}, { 1, 0}, { 1, -1}, { 0, 0}, { 0, 0}, { 0, 0}, {-1, 1}, { 0, 1}},
		{{ 1, 1}, { 1, 0}, { 1, -1}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}},
		{{ 1, 1}, { 1, 0}, { 1, -1}, { 0, -1}, {-1, -1}, { 0, 0}, { 0, 0}, { 0, 0}},
		{{ 0, 0}, { 0, 0}, { 1, -1}, { 0, -1}, {-1, -1}, { 0, 0}, { 0, 0}, { 0, 0}},
		{{ 0, 0}, { 0, 0}, { 1, -1}, { 0, -1}, {-1, -1}, {-1, 0}, {-1, 1}, { 0, 0}},
		{{ 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}, {-1, -1}, {-1, 0}, {-1, 1}, { 0, 0}},
		{{ 1, 1}, { 0, 0}, { 0, 0}, { 0, 0}, {-1, -1}, {-1, 0}, {-1, 1}, { 0, 1}},
		{{ 1, 1}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}, { 0, 0}, {-1, 1}, { 0, 1}},
		};

	/* Lists of RF and BB attens at which Baseband LO Comp is computed.
	 * For RF atten list elements, high nibble is padmix enable, low nibble is rf attn
	 */
	/* rf attn lists for all sw power control */
	static uint16 rf_attn_list_other_sw[] = {0x03, 0x01, 0x05, 0x07, 0x09, 0x02, 0x00,
		0x04, 0x06, 0x08, 0x11, 0x12, 0x13, 0x14};

	/* rf attn lists for hw pwr control 4318 and 4320 */
	static uint16 rf_attn_list18_hw[] = {0x12, 0x14, 0x16, 0x18, 0x1a, 0x1c, 0x1e};
	static uint16 rf_attn_list20_hw[] = {0x10, 0x12, 0x14, 0x16, 0x18, 0x19, 0x19};

	/* bb attn lists for sw and hw power control */
	static uint16 bb_attn_list_sw[] = {0, 1, 2, 3, 4, 5, 6, 7, 8 };
	static uint16 bb_attn_list_hw[] = {0, 1, 2, 3, 4, 5, 6, 7, 8 };

	/* list of bias magnitude settings */
	static uint16 tx_mag_list[] = {7, 3, 0};
	static uint16 tx_mag_list_empty[] = {0};



	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* check D11 is running on Fast Clock */
	if (D11REV_GE(pi->sh->corerev, 5))
		ASSERT(si_core_sflags(pi->sh->sih, 0, 0) & SISF_FCLKA);

	is_initial = (pi_abg->stats_11b_txpower[0][0] < 0);
	gmode = pi_abg->sbtml_gm;

	/**************************************************************
	 * Parameter setup.
	 * Select rf,bb attenuation lists for Baseband LO Comp
	 **************************************************************
	 */
	if (is_initial) {
		if (pi->hwpwrctrl) {
			if ((pi->pubpi.radioid == BCM2050_ID) &&
			     (pi->pubpi.radiorev == 8)) {
				pi_abg->rf_attn_list = rf_attn_list18_hw;
				rfsize = ARRAYSIZE(rf_attn_list18_hw);
			} else {
				pi_abg->rf_attn_list = rf_attn_list20_hw;
				rfsize = ARRAYSIZE(rf_attn_list20_hw);
			}
			pi_abg->bb_attn_list = bb_attn_list_hw;
			bbsize = ARRAYSIZE(bb_attn_list_hw);
		} else {
			pi_abg->rf_attn_list = rf_attn_list_other_sw;
			rfsize = ARRAYSIZE(rf_attn_list_other_sw);
			pi_abg->bb_attn_list = bb_attn_list_sw;
			bbsize = ARRAYSIZE(bb_attn_list_sw);
		}

		pi_abg->rf_max = 0;
		for (ii = 0; ii < rfsize; ii++) {
			if (pi_abg->rf_max < PHY_GET_RFATTN(pi_abg->rf_attn_list[ii]))
				pi_abg->rf_max = PHY_GET_RFATTN(pi_abg->rf_attn_list[ii]);
		}
		pi_abg->rf_list_size = rfsize;

		pi_abg->bb_max = 0;
		for (ii = 0; ii < bbsize; ii++) {
			if (pi_abg->bb_max < pi_abg->bb_attn_list[ii])
				pi_abg->bb_max = pi_abg->bb_attn_list[ii];
		}
		pi_abg->bb_list_size = bbsize;
	}

	/* Default: sw power control for non 4318/4320 chips */
	lnalod = 0;
	pgaval = 0;
	rf_attn = 0;
	pi_abg->padmix_reg   = 0;
	pi_abg->padmix_mask  = 0;
	pi_abg->txmag_list   = tx_mag_list_empty;
	pi_abg->txmag_len    = ARRAYSIZE(tx_mag_list_empty);
	pi_abg->txmag_enable = 0;

	/*************************************************************************
	 * HW power control is possible in GPHY chips with phy_revid >= 3
	 *  - Power control logic controls LO comp dc-offset, dac attenuation,
	 *    radio transmit gain, digi gain & mixer bias
	 * For bgphy_measurelo to work properly, give it control of these functions
	 *************************************************************************
	 */
	if (pi->hwpwrctrl) {
		old_phyreg80f = phy_reg_read(pi, GPHY_DC_OFFSET1);
		old_phyreg801 = phy_reg_read(pi, GPHY_CTRL);
		old_phyreg60  = phy_reg_read(pi, BPHY_DAC_CONTROL);
		old_phyreg14  = phy_reg_read(pi, BPHY_TX_POWER_OVERRIDE);
		old_phyreg478 = phy_reg_read(pi, GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1);

		PHY_REG_LIST_START
			/*  Take the control away from HW PWRCTRL */
			/* txDigScalerOvr : override the digital scalar
			* (which is usually driven by rate based gain offset)
			* Enable the override on the gphy scalar
			*/
			PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1, 0x100)
			PHY_REG_OR_RAW_ENTRY(GPHY_CTRL, 0x40)
			/* dc-offset control must be taken away from HW power control
			*  (will be done later in the function)
			* disable HW power control writes to rfAtten & padmix gain registers
			* take dac attenuation control (bbattn) away from HW power control
			*/
			PHY_REG_OR_RAW_ENTRY(BPHY_DAC_CONTROL,       0x40)
			PHY_REG_OR_RAW_ENTRY(BPHY_TX_POWER_OVERRIDE, 0x200)
		PHY_REG_LIST_EXECUTE(pi);
	}

	/*************************************************************************
	 * There are five major categories of chips that this proc supports
	 *
	 * GPHY_GMODE_INTRADIO
	 *  4318A0, A1, B0 etc
	 *      Must be called in g-mode, for reasons given above
	 *  2050 rev 8
	 *      Radio register map different
	 *      Mixer bias magnitude control present and used
	 *  Loopback gain calibrated
	 *
	 * GPHY_GMODE_EXTRADIO
	 *  4306C0, 4712, 4320, 5352 etc
	 *      Must be called in g-mode
	 *      G-phy radio overrides bug has been fixed & b-mode not possible.
	 *  2050A1 standalone radio
	 *      2050A1 radio register map
	 *      Mixer bias magnitude control not present
	 *  Loop back gain calibrated
	 *
	 * As noted above, mixer bias magnitude control is searched only for 4318
	 * for the rest of the chips, initialize to a dummy string
	 */

	/* Find which categories the chip falls into & set variables */
	if (ISGPHY(pi)) {

		/* PHY TYPE is G
		 *
		 * G-phy's dc-offset registers are used for LO compensation
		 */
		dc_offset1_reg = 0x80f;
		dc_offset2_reg = 0x810;

		/* take dc-offset control away from HW power control
		 * For revid 3 onward
		 * bit 14: 1=> dcoffset from dc_offset2_reg
		 *         0=> hwpwr control
		 */
		if (GREV_LE(pi->pubpi.phy_rev, 2)) {
			dc_offset1_val = 0x8078;
		} else {
			dc_offset1_val = 0xc078;
		}

		/* When this proc is run in G-mode, some rfInterface signals are
		 * controlled through G-phy overrides. These include:
		 *    ext_lna_gain_ovr - Bit 11 of phyreg 0x811
		 *    agc_rx_ctrl[5:0] - Bit  8 of phyreg 0x811
		 *    tr_sw_rx_pu      - Bit  5 of phyreg 0x811
		 *    tr_sw_tx_pu      - Bit  4 of phyreg 0x811
		 *    fltr_rx_ctrl_lpf - Bit  1 of phyreg 0x811
		 *    fltr_rx_ctrl     - Bit  0 of phyreg 0x811
		 * if external lna gain controls are on chip, activate override
		 */
		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
		    GREV_GE(pi->pubpi.phy_rev, 7)) {
			reg811_cntl = 0x0933;
		} else {
			reg811_cntl = 0x0133;
		}

		if (GREV_IS(pi->pubpi.phy_rev, 1)) {
			/* GPHY_BMODE_EXTRADIO */
			calmode        = 1;
			gmode          = FALSE;
			maxRFAttnCode  = 9;
			pi_abg->padmix_reg = 0x52;
			pi_abg->padmix_mask = 0x30;
			padmix_gain    = 5;
		} else if ((pi->pubpi.radioid == BCM2050_ID) && (pi->pubpi.radiorev == 8)) {
			/* GPHY_GMODE_INTRADIO
			 * includes search over mixer bias magnitude values
			 */
			pi_abg->txmag_list = tx_mag_list;
			pi_abg->txmag_len = ARRAYSIZE(tx_mag_list);
			pi_abg->txmag_enable       = 1;
			calmode        = 3;
			gmode          = TRUE;
			maxRFAttnCode  = 15;
			pi_abg->padmix_reg = 0x43;
			pi_abg->padmix_mask = 0x10;
			padmix_gain    = 2;

		} else {
			/* GPHY_GMODE_EXTRADIO */
			calmode        = 2;
			gmode          = TRUE;
			maxRFAttnCode  = 9;
			pi_abg->padmix_reg = 0x52;
			pi_abg->padmix_mask = 0x30;
			padmix_gain    = 5;
		}
	} else {
		/* Only PHY type G is supported */
		PHY_ERROR(("wl%d: %s: PHY_TYPE= %d is Unsupported ",
		          pi->sh->unit, __FUNCTION__, pi->pubpi.phy_type));
		return;
	}

	/****************************************************************
	 * Finally, done with setup.  Now start real work.
	 *
	 * Periodic glacial timer with hwpwrctrl enters here.
	 * Under sw based power control, the driver kept track of the
	 * tx power attenuations it had used since the last glacial
	 * timer and based measlo on those attenuation settings.
	 * Under hw based pwr control, the ucode sets the attenuations
	 * and the driver is unaware of what was used.  To get around that,
	 * The ucode saves a bitmap of the attens it used. Fetch the
	 * bit map now and then clear so its ready for re-use.
	 ****************************************************************
	 */
	if (!is_initial && pi->hwpwrctrl) {
		uint16 rf = 0, bb = 0;
		int idx;
		uint16 tmp;
		uint8  vec[8];

		padmix_en = 0;
		/* Read 4 16 bit shmem entries */
		for (idx = 0; idx < 4; idx++) {
			tmp = wlapi_bmac_read_shm(pi->sh->physhim, M_PWRIND_BLKS + (2 * idx));
			vec[idx * 2] = (uint8)(tmp & 0x00ff);
			vec[idx * 2 + 1] = (uint8)(tmp & 0xff00);
		}

		/* Zero out entries and start over for next time through. */
		for (idx = 0; idx < 4; idx++)
			wlapi_bmac_write_shm(pi->sh->physhim, M_PWRIND_BLKS + (2 * idx), 0);

		/* If no new indexes, keep using old vec, otherwise use this new vec */
		for (idx = 0; idx < TX_GAIN_TABLE_LENGTH; idx++) {
			if (isset(vec, idx)) {
				PHY_CAL(("measlo: index %d is set, using new vec\n", idx));
				bcopy(vec, pi_abg->power_vec, sizeof(vec));
				break;
			}
		}
		if (idx == TX_GAIN_TABLE_LENGTH) {
			PHY_CAL(("measlo: no new index found, keeping old vec\n"));
		}

		/* Process 64 bits worth of indexes */
		for (idx = 0; idx < TX_GAIN_TABLE_LENGTH; idx++) {
			if (isset(pi_abg->power_vec, idx)) {
				wlc_ucode_to_bb_rf(pi, idx, &bb, &rf, &padmix_en);
				wlc_txpower_11b_inc(pi, bb, rf, padmix_en * pi_abg->padmix_mask);
				PHY_CAL(("measlo: index %d: bb %d, rf %d, ctrl %d\n",
				         idx, bb, rf, padmix_en * pi_abg->padmix_mask));
			}
		}
	}
	/* Cache away reg values we mess with so they can be restored,
	 * disable aphy crs_fsm state machine, disable the classifier
	 * do early to avoid APHY claim that can result in 4-wire contention
	 */
	if (gmode) {
		old_gphy_analog_ovr     = phy_reg_read(pi, GPHY_ANA_OVERRIDE);
		old_gphy_analog_ovr_val = phy_reg_read(pi, GPHY_ANA_OVERRIDE_VAL);
		old_gphy_rf_ovr         = phy_reg_read(pi, GPHY_RF_OVERRIDE);
		old_gphy_rf_ovr_val     = phy_reg_read(pi, GPHY_RF_OVERRIDE_VAL);
		old_classifyctl		= phy_reg_read(pi, GPHY_CLASSIFY_CTRL);
		old_phyreg3e   		= phy_reg_read(pi, BPHY_RFDC_CANCEL_CTL);
		old_corr_str0_shift_din = phy_reg_read(pi,
		                                       (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN));

		/* do early, since we do not want to be interrupted by a packet rx */
		phy_reg_write(pi, GPHY_CLASSIFY_CTRL, old_classifyctl & 0xfffc);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN),
			old_corr_str0_shift_din & 0x7fff);

		/* force ADC, DAC power downs to 0 (ie enable both ADC & DAC) */
		phy_reg_write(pi, GPHY_ANA_OVERRIDE,     old_gphy_analog_ovr | 3);
		phy_reg_write(pi, GPHY_ANA_OVERRIDE_VAL, old_gphy_analog_ovr_val & 0xfffc);

		/* enable g-phy overrides for 2050 gain, fltr_rx_ctrl,
		 * fltr_rx_ctrl_lpf, tr_switch & synth_pu
		 */
		phy_reg_write(pi, GPHY_RF_OVERRIDE, reg811_cntl);

		/* disable DC canceller WAR (only under g-mode) */
		phy_reg_write(pi, BPHY_RFDC_CANCEL_CTL, 0);
	}

	dac_ctrl_reg   = 0x60;
	old_bbcfg      = R_REG(pi->sh->osh, &regs->phybbconfig);
	old_tx_tst     = R_REG(pi->sh->osh, &regs->phytest);
	old_brfovr     = phy_reg_read(pi, BPHY_RF_OVERRIDE);
	old_trloss_ctl = phy_reg_read(pi, BPHY_TR_LOSS_CTL);
	old_sync_ctl   = phy_reg_read(pi, BPHY_SYNC_CTL);
	old_dac_ctl    = phy_reg_read(pi, dac_ctrl_reg);
	old_pwr_ctl    = read_radio_reg(pi, RADIO_2050_PWR_CTL);
	old_rx_ctl0    = read_radio_reg(pi, RADIO_2050_RX_CTL0);

	/*  store the upper nibble of radio register 0x52 for 2050A1
	 *  This contains the PA & Mixer gain bits
	 *  (txmag_enable == 0 ) ==> 2050A1
	 */
	if (pi_abg->txmag_enable == 0)
		old_tx_ctl1_high_nibble = read_radio_reg(pi, RADIO_2050_TX_CTL1) & 0xf0;

	orig_channel   = CHSPEC_CHANNEL(pi->radio_chanspec);

	/* Force BPHY receiver state machine in reset state */
	if (calmode >= 1) {
		OR_REG(pi->sh->osh, &regs->phybbconfig, 0x8000);
	} else {
		old_peakCnt = phy_reg_read(pi, BPHY_PEAK_CNT_THRESH);
		old_crsth   = phy_reg_read(pi, BPHY_PHYCRSTH);
		PHY_REG_LIST_START
			PHY_REG_WRITE_RAW_ENTRY(BPHY_PEAK_CNT_THRESH, 0xff)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_PHYCRSTH, 0x3f3f)
		PHY_REG_LIST_EXECUTE(pi);
	}

	/* make sure carrier suppression test is off */
	W_REG(pi->sh->osh, &regs->phytest, R_REG(pi->sh->osh, &regs->phytest) & 0xf000);

	/* turn off BB DC offset(bit 15) during sweep of RF TX BIAS setting */
	phy_reg_write(pi, dc_offset1_reg, 0x007f);

	/* clear toggle sync cutoff bit to allow forcing of fltr_rx_ctrl */
	phy_reg_write(pi, BPHY_SYNC_CTL, (old_sync_ctl & ~SYN_TOGGLE_CUTOFF));

	/* clear lna load bit and
	 * set DC canceller slow mode to be **really** slow (120 Hz)
	 */
	write_radio_reg(pi, RADIO_2050_RX_CTL0, (old_rx_ctl0 & (~0x0f)));

	/* disable trLossCtl mechanism */
	phy_reg_write(pi, BPHY_TR_LOSS_CTL, 0x8a3);

	/* set cap_len [13:8] (capture length in us) and trigger delay [5:0]
	 * 16bit reg for accum vals (at 22MHz) (so 8us => 176 samples)
	 * sum of 2*( abs( 8bit number )/4 ) => max accum is 128 x 176
	 */
	if (calmode == 0)
		phy_reg_write(pi, BPHY_LO_LEAKAGE, 0x0802);
	else
		phy_reg_write(pi, BPHY_LO_LEAKAGE, 0x1003);

	if (gmode) {
		/* IQ accumulation measurement in BPHY is triggered when both
		 * TxPu and RxPu from bphy go up. Further, TxPu to 2050 needs to be
		 * controlled from BPHY. This is because the dc-offset in the receive
		 * path decays with time and Txpu and IQACCUM needs to be synchronized.
		 * Doing a dummy cck transmit ensures that BPHY controls TxPu and RxPu
		 * going to 2050. (FALSE means "not ofdm")
		 * Also, by this time (before the dummy tx)
		 *   aphy and bphy carrier sense should be disabled
		 *   gphy classifier should also be disabled
		 */
		wlc_phy_do_dummy_tx(pi, FALSE, ON);
	}

	/* Set the channel
	 * if channel 6 is not valid for current locale, temporarily make
	 * mark it valid.  Re-invalidate it following the channel change.
	 */
	wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(6));

	/*********************************************************************
	 * Mixer Bias LO compensation
	 * Find the optimal phase & magnitude values for the radio mixer bias
	 ********************************************************************
	 */
	padmix_mixr  = 0;
	bbattn_mixr  = 2;
	TRSW_RX      = 0;
	lnalod_mixr  = read_radio_reg(pi, RADIO_2050_TX_CTL0) & 0x08;
	BCM_REFERENCE(lnalod_mixr);
	lnaval_mixr  = 0;
	if (pi_abg->loopback_gain) {
		/* bkoffdB_mixr: rx gain backoff when searching for tx mixr bias
		 * LPBACK_GAIN is in dB  (max_lpback_gain_hdB is 0.5dB steps)
		 */
		bkoffdB_mixr = 6;
		LPBACK_GAIN  = pi_abg->max_lpback_gain_hdB / 2;

		/* find rfattn & pgaval from totattn(0.5dB steps) (padmix gain is 0) */
		totattn = 0 - bbattn_mixr - 2 * LPBACK_GAIN + 2 * bkoffdB_mixr;
		if (totattn >= 0) {
			pgaval_mixr = 0;
			totattn     = MIN(totattn, 4 * maxRFAttnCode);
			rfattn_mixr = MIN((totattn + 3) / 4, maxRFAttnCode);
		} else {
			pgaval_mixr = MIN((-totattn) / 6, 15);
			rfattn_mixr = 0;
		}

	} else {
		TRSW_RX      = 0x20;
		LPBACK_GAIN  = 0;
		bkoffdB_mixr = 0;
		pgaval_mixr  = 0;
		rfattn_mixr  = 6;
	}

	/* For gphy chips, "bphy" DC offset must be zeroed as well */

	if (pi->pubpi.phy_type == PHY_TYPE_G) {
		phy_reg_write(pi, BPHY_TX_DC_OFF2, 0);
	}

	/* Mixer-bias compensation is only done at start-up
	 * It is not done for periodic re-cal
	 */
	if (is_initial) {
		/* set the transmit gains just found for mixer bias search */
		mod_radio_reg(pi, RADIO_2050_PWR_CTL, 0x0f, rfattn_mixr);
		bgphy_set_bbattn(pi, bbattn_mixr);
		mod_radio_reg(pi, pi_abg->padmix_reg, pi_abg->padmix_mask,
		 pi_abg->padmix_mask * padmix_mixr);

		PHY_CAL(("Mixer-bias: bb_attn= %d, rf_attn= %d, padmix= 0, LPBACK_GAIN= %d,"
		         " bkoffdB= %d\n",
		         bbattn_mixr, rfattn_mixr, LPBACK_GAIN, bkoffdB_mixr));
		PHY_CAL(("TRSW_RX= %x LNALD= %d, LNA= %d PGA= %d\n",
		         TRSW_RX, lnalod_mixr, lnaval_mixr, pgaval_mixr));

		mintxbias  = 0;
		miniqaccum = INFINITY;

		/* for 4318 loop over 3 "bias current magnitude" settings
		 * (for other chips, there is only 1 magnitude setting)
		 */
		for (maglist_idx = 0; maglist_idx < pi_abg->txmag_len; maglist_idx++) {
			txmag = pi_abg->txmag_list[maglist_idx];
			if (pi_abg->txmag_enable)
				mod_radio_reg(pi, RADIO_2050_TX_CTL1, 0xf0, txmag << 4);

			for (txbias_idx = 0; txbias_idx < ARRAYSIZE(txbias_list); txbias_idx++) {
				txbias = txbias_list[txbias_idx];
				mod_radio_reg(pi, RADIO_2050_TX_CTL1, 0x0f, txbias);
				iqaccum = gphy_measurelo_iter(pi, gmode,
				                              TRSW_RX, lnaval_mixr, pgaval_mixr);
				if (iqaccum < miniqaccum) {
					mintxbias  = txbias;
					miniqaccum = iqaccum;
					mintxmag   = txmag;
				}
				PHY_CAL(("%8d", iqaccum));
				if (((txbias_idx + 1) % 3) == 0) PHY_CAL(("\n"));
			}

			/* If the min is at 0, then no need to do other txmag values */
			if (mintxbias == 0)
				break;
		}

		/* Write optimal mixer txbias settings to radio reg 0x52. */
		if (pi_abg->txmag_enable)
			write_radio_reg(pi, RADIO_2050_TX_CTL1, mintxmag << 4 | mintxbias);
		else
			mod_radio_reg(pi, RADIO_2050_TX_CTL1, 0x0f, mintxbias);

		/* Save these settings */
		pi_abg->mintxbias = mintxbias;
		pi_abg->mintxmag  = mintxmag;

		PHY_CAL(("Mixer-bias: RF TXBIAS (reg 0x52) is 0x%x, miniqaccum is %d\n\n",
		         (mintxmag <<4) | mintxbias, miniqaccum));

	} /* endif is_initial */

	/*********************************************************************
	 * Baseband LO compensation
	 * After setting the optimal 2050 LO suppression (TX mixer bias),
	 * compute BB LO compensation (via BB DC offsets)
	 ********************************************************************
	 */
	if (pi_abg->loopback_gain) {
		/* bkoffdB: rx gain backoff when searching for BB DC lo compensation
		 *    allow 6dB more gain since mixr setting will have been found
		 */
		bkoffdB      = bkoffdB_mixr - 6;

		/* grid spacing and iqaccum thresholds for BB DC lo comp */
		NPASSES      = 4;
		INITIAL_GRID = 3;
		IQ_LOTHD     = 1500;
		IQ_HITHD     = 4500;
		PGASTEP      = 1;
		USE_TRSW_RX  = TRUE;

	} else {
		bkoffdB      = 0;
		NPASSES      = 1;
		INITIAL_GRID = 1;
		IQ_LOTHD     = INFINITY;
		IQ_HITHD     = 4500;
		USE_TRSW_RX  = FALSE;
	}

	/* Set phy params via dc_offset1_reg
	 *   Scale GPHY 8-bit TX outputs by 120/128 such that the result of
	 *     BB TX DC offset addition of -8:8 won't saturate.
	 *   Bit 15 enables DC offset for LO compensation in the baseband.
	 *   Enable offset_ovr bit (4318)
	 */
	phy_reg_write(pi, dc_offset1_reg, dc_offset1_val);

	/* Print rf and bb atten lists */
	/* --------------------------- */
	if (is_initial) {
		PHY_CAL(("wl%d: %s: rf_attn and bb_attn lists:\n",
		         pi->sh->unit, __FUNCTION__));

		for (z = 0; z < pi_abg->rf_list_size; z++)
			PHY_CAL((" {%2d%2d}",
			         PHY_GET_RFATTN(pi_abg->rf_attn_list[z]),	/* rf_attn0 */
			         PHY_GET_PADMIX(pi_abg->rf_attn_list[z])));	/* padmix_en */
		PHY_CAL(("\n"));

		for (j = 0; j < pi_abg->bb_list_size; j++)
			PHY_CAL(("%2d,", pi_abg->bb_attn_list[j]));
		PHY_CAL(("\n"));
	}

	/* --------------------------------------- */
	/* Loop over (rf_attn, padmix_en), bb_attn */
	/* --------------------------------------- */
	min_pos.i = min_pos.q = 0;

	for (z = 0; z < pi_abg->rf_list_size; z++) {
		rf_attn   = PHY_GET_RFATTN(pi_abg->rf_attn_list[z]); /* rf_attn0 */
		padmix_en = PHY_GET_PADMIX(pi_abg->rf_attn_list[z]); /* padmix_en */

		/* Select a starting position for the lo comp search */
		if (is_initial && !padmix_en) {
			/* if padmix gain is not enabled ... */
			if (z == 0) {
				/* if first rf_attn, start iq locomp values from 0,0 */
				min_pos.i = min_pos.q = 0;
			} else if ((rf_attn % 2) !=
			           (PHY_GET_RFATTN(pi_abg->rf_attn_list[z-1]) % 2)) {
				/* odds vals are complete, pick min_pos for first even val
				 * as bb_attn 0 of first odd rf_attn
				 */
				int rf = PHY_GET_RFATTN(pi_abg->rf_attn_list[0]);
				int pm = PHY_GET_PADMIX(pi_abg->rf_attn_list[0]);
				min_pos = pi_abg->gphy_locomp_iq[PHY_GET_RFGAINID(rf, pm,
					pi_abg->rf_max)][0];
			} else {
				/* if parity of rf_attn is the same as for prev_rf_attn, use
				 * the min_pos from prev_rf_attn (bb_attn=0) since the lsb controls
				 * the PAD attn (before the mixer) vs LPF attn (after the mixer)
				 * Same parity implies atten before the mixer has not changed
				 * hence the prev_rf_attn min_pos makes a good starting location
				 */
				int rf = PHY_GET_RFATTN(pi_abg->rf_attn_list[z-1]);
				int pm = PHY_GET_PADMIX(pi_abg->rf_attn_list[z-1]);
				min_pos = pi_abg->gphy_locomp_iq[PHY_GET_RFGAINID(rf, pm,
					pi_abg->rf_max)][0];
			}
		}

		/* Write the rf_atten and padmix_en settings to the radio regs */
		mod_radio_reg(pi, RADIO_2050_PWR_CTL, 0x0f, rf_attn);
		mod_radio_reg(pi, pi_abg->padmix_reg, pi_abg->padmix_mask,
		 pi_abg->padmix_mask * padmix_en);
		for (j = 0; j < pi_abg->bb_list_size; j++) {
			uint grid, pass;

			bb_attn = pi_abg->bb_attn_list[j];

			if (!is_initial) {
				/* Periodic re-cal: check if this atten setting can be skipped */
				if (pi_abg->stats_11b_txpower[PHY_GET_RFGAINID(rf_attn, padmix_en,
					pi_abg->rf_max)][bb_attn] == 0) {
					continue;
				}

				/* For periodic re-cal, we should be in the neighborhood
				 * of the correct DC offsets, so set the search grid size
				 * and number of passes to minimum to reduce search time.
				 */
				NPASSES = 1;
				INITIAL_GRID = 1;
				PHY_CAL(("measlo recal: previous mintxbias %d, radio 0x52 = 0x%x\n",
				         pi_abg->mintxbias,
					read_radio_reg(pi, RADIO_2050_TX_CTL1)));
			}

			/* if lo comp without padmix gain enabled is done,
			 * then for the initial minpos with padmix gain enabled,
			 * use the final minpos without padmix enabled
			 * otherwise, get the next min position from the table.
			 */
			if (is_initial && padmix_en)
				min_pos = pi_abg->gphy_locomp_iq[PHY_GET_RFGAINID(rf_attn, 0,
				                                          pi_abg->rf_max)][bb_attn];
			else
				min_pos = pi_abg->gphy_locomp_iq[PHY_GET_RFGAINID(rf_attn,
					padmix_en, pi_abg->rf_max)][bb_attn];

			/* Max gain is computed from total attn and max loopback gain
			 * Determine optimum RX gain settings via gphy_measurelo_gains()
			 */
			max_rx_gain = (2 * rf_attn) + (bb_attn / 2)
			        - (padmix_gain * padmix_en) + LPBACK_GAIN - bkoffdB;

			gphy_measurelo_gains(pi, max_rx_gain, USE_TRSW_RX,
			                     &TRSW_RX, &lnalod, &lnaval, &pgaval);

			PHY_CAL(("bb_attn= %d, rf_attn= %d, padmix= %d LPBACK_GAIN= %d, bkoffdB="
			         " %d\n", bb_attn, rf_attn, padmix_en, LPBACK_GAIN, bkoffdB));

			bgphy_set_bbattn(pi, bb_attn);

			stepcnt = 0;
			meascnt = 0;
			cur_pos = min_pos;

			/* Multiple pass LOOP (but NPASSES > 1 for g-mode only)
			 * First pass:
			 *  use a conservatively low rx gain setting
			 *  use a larger "position step" (grid spacing)
			 *   (INITIAL_GRID is typically > 1 for g-mode)
			 * Second pass: use a 12dB higher rx gain setting
			 * Third  pass: use a grid spacing of 1
			 * ------------------------------------------------------
			 */
			grid = INITIAL_GRID;
			for (pass = 1; pass <= NPASSES; pass++) {
				i = cur_pos.i;
				q = cur_pos.q;

				valtxdc = ((i & 0xff) << 8) | (q & 0xff);
				phy_reg_write(pi, dc_offset2_reg, valtxdc);

				/* compute initial absI+absQ accumulation */
				iqaccum = gphy_measurelo_iter(pi, gmode, TRSW_RX, lnaval, pgaval);
				meascnt++;

				/* Periodic re-cal: Increase the loopback rx path gain and
				 * re-compute absI+absQ accumulation if iqaccum is low
				 */
				if (!is_initial && (iqaccum < INC_BY6DB_THD)) {
					if (iqaccum < INC_BY12DB_THD)
						max_rx_gain = max_rx_gain + 12;
					else if (iqaccum < INC_BY6DB_THD)
						max_rx_gain = max_rx_gain + 6;
					PHY_CAL(("measlo recal: iqaccum= %d, max_rx_gain= %d\n",
					         iqaccum, max_rx_gain));
					iqaccum = gphy_measurelo_iter(pi, gmode, TRSW_RX, lnaval,
					                              pgaval);
					meascnt++;
				}

				PHY_CAL(("*** pass= %d, cur_pos (%2d %2d), iqaccum= %d, grid= %d\n",
				         pass, cur_pos.i, cur_pos.q, iqaccum, grid));

				min_val = iqaccum;
				min_idx = -1;
				cur_stp = &steps[0][0];

				done = 0;
				while ((!done) && (stepcnt < 24)) {
					abgphy_lo_complex_info_t nxt_pos, delta_pos;
					abgphy_lo_complex_info_t nxtp, piq[9];
					uint32 nxt_val;
					int    aiq[9];
					int    idx, nxta;

					/* arrays for debug printout of position and accum */
					aiq[0] = min_val;
					piq[0] = min_pos;
					BCM_REFERENCE(aiq);
					BCM_REFERENCE(piq);

					for (idx = 0; idx < NUM_NEIGHBORS; idx++) {
						delta_pos = cur_stp[idx];
						i = cur_pos.i + grid * delta_pos.i;
						q = cur_pos.q + grid * delta_pos.q;
						nxt_pos.i = i; nxt_pos.q = q;
						if (((nxt_pos.i == cur_pos.i) &&
						     (nxt_pos.q == cur_pos.q)) ||
						     (ABS(i) > MAX_IQ_DC) ||
						     (ABS(q) > MAX_IQ_DC)) {
							nxt_val = INFINITY;
						} else {
							valtxdc = ((i & 0xff) << 8) | (q & 0xff);
							phy_reg_write(pi, dc_offset2_reg,
							              valtxdc);
							iqaccum = gphy_measurelo_iter(pi, gmode,
							                              TRSW_RX,
							                              lnaval,
							                              pgaval);
							meascnt++;
							nxt_val = iqaccum;
						}

						nxta = nxt_val;
						nxtp.i = i; nxtp.q = q;
						if (nxta == INFINITY) {
							nxta = -1;
							nxtp.i = 111; nxtp.q = 111;
						}
						aiq[idx+1] = nxta;
						piq[idx+1] = nxtp;

						if (nxt_val < min_val) {
							min_val = nxt_val;
							min_pos = nxt_pos;
							min_idx = idx;
							if ((pass <= 2) && (NPASSES > 1) &&
							    (min_val < IQ_LOTHD)) {
								int ix;
								PHY_CAL(("vvv WILL SKIP to pass %d"
								         " AT (%3d%3d)"
								         " with smaller grid, more"
								         " gain\n",
								         pass+1, min_pos.i,
								         min_pos.q));
								for (ix = idx + 2; ix < 9; ix++) {
									aiq[ix] = -9;
									piq[ix].i = 99;
									piq[ix].q = 99;
								}

								cur_pos = min_pos;
								break;
							}
						}
					}

					PHY_CAL(("vvv pass= %d, step= %d, min_val(%2d %2d) = %d\n",
					         pass, stepcnt, min_pos.i, min_pos.q, min_val));
					PHY_CAL(("   %6d %6d %6d   (%3d%3d) (%3d%3d) (%3d%3d)\n",
					         aiq[7],            aiq[8],            aiq[1],
					         piq[7].i, piq[7].q, piq[8].i, piq[8].q, piq[1].i,
					         piq[1].q));
					PHY_CAL(("   %6d %6d %6d   (%3d%3d) (%3d%3d) (%3d%3d)\n",
					         aiq[6],            aiq[0],            aiq[2],
					         piq[6].i, piq[5].q, piq[0].i, piq[0].q, piq[2].i,
					         piq[2].q));
					PHY_CAL(("   %6d %6d %6d   (%3d%3d) (%3d%3d) (%3d%3d)\n",
					         aiq[5],            aiq[4],            aiq[3],
					         piq[5].i, piq[5].q, piq[4].i, piq[4].q, piq[3].i,
					         piq[3].q));

					if (min_pos.i == cur_pos.i && min_pos.q == cur_pos.q) {
						done = 1;
					} else {
						cur_pos = min_pos;
						cur_stp = steps [min_idx + 1];
					}

					stepcnt++;
				}

				/* End of a pass: heuristics for adjusting gain and grid spacing */
				if (min_val < IQ_LOTHD) {
					max_rx_gain = max_rx_gain + PGASTEP * 3;
				} else if (min_val > IQ_HITHD) {
					max_rx_gain = max_rx_gain - 2 * 3;
				}
				if (pass == 1) {
					if (min_val > IQ_LOTHD) {
						PHY_CAL(("SKIPPING pass 2\n"));
						grid = 1;
						pass = 2;
					} else {
						grid = 2;
					}
				}
				if (pass == NPASSES - 1)
					grid = 1;

				/* Update the RX path gain settings */
				gphy_measurelo_gains(pi, max_rx_gain, USE_TRSW_RX,
				                     &TRSW_RX, &lnalod, &lnaval, &pgaval);

				if (stepcnt >= 24) {
					PHY_ERROR(("wl%d: %s: STEPCNT > 24: rf attn: %d, bb attn:"
					          " %d, padmix_en: %d, stepcnt: %d, meascnt: %d,"
					          " minpos: i=%d q=%d, miniqacc: %d\n",
					          pi->sh->unit, __FUNCTION__, rf_attn, bb_attn,
					          padmix_en, stepcnt, meascnt, min_pos.i,
					          min_pos.q, min_val));
				}
			}
			/* ## END multi-pass LOOP (for grid/pgaval settings) ** */

			rf_ind = PHY_GET_RFGAINID(rf_attn, padmix_en, pi_abg->rf_max);

			if (!is_initial &&
			    (pi_abg->gphy_locomp_iq[rf_ind][bb_attn].i != min_pos.i ||
			     pi_abg->gphy_locomp_iq[rf_ind][bb_attn].q != min_pos.q)) {
				PHY_CAL(("wl%d: %s: Changing lo comp tbl at stepcnt= %d, meascnt="
				         " %d:\nbb= %d, rf= %d, pdmx= %d, from:(%3d%3d) to "
				         "minpos:(%3d%3d) miniqacc= %d\n",
				         pi->sh->unit, __FUNCTION__, stepcnt, meascnt,
				         rf_attn, bb_attn, padmix_en,
				         pi_abg->gphy_locomp_iq[rf_ind][bb_attn].i,
				         pi_abg->gphy_locomp_iq[rf_ind][bb_attn].q,
				         min_pos.i, min_pos.q, min_val));
			}

			sav_pos = min_pos;
			if (calmode <= 0) {
				sav_pos.i = min_pos.i + 1;
				sav_pos.q = min_pos.q + 1;
			}
			pi_abg->gphy_locomp_iq[rf_ind][bb_attn].i = sav_pos.i;
			pi_abg->gphy_locomp_iq[rf_ind][bb_attn].q = sav_pos.q;

			PHY_CAL(("wl%d:GPHY_BBDC: MINPOS: (%3d%3d), MINIQACC: %d(%d, %d, %d),"
				"rf_attn is %d, rf_max %d\n", pi->sh->unit, min_pos.i,
				min_pos.q, min_val, padmix_en, rf_ind, bb_attn,
				rf_attn, pi_abg->rf_max));
			PHY_CAL(("    PD: %d, RF: %d, BB: %d, STEPCNT: %d, MEASCNT: %d\n",
			         padmix_en, rf_attn, bb_attn, stepcnt, meascnt));
		}
	} /* End of Loop over (rf_attn, padmix_en), bb_attn */
	/* Write the DC offsets to the DC offset register */
	valtxdc = ((min_pos.i & 0xff) << 8) | (min_pos.q & 0xff);
	phy_reg_write(pi, dc_offset2_reg, valtxdc);

	/* Finally, toggle tx_pu low to end last tx LO measurement, and
	 * toggle fltr_rx_ctrl low (high BW mode) to flush rx DC from prev
	 * measurement
	 */
	if (gmode) {
		phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xe300);
		phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, (pgaval << 8) | 0x00a0);
		OSL_DELAY(5);
		phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, (pgaval << 8) | 0x00a2);
		OSL_DELAY(2);
		phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, (pgaval << 8) | 0x00a3);
	} else {
		phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xefa0 | pgaval);
	}

	/* if HW power control, download DC bias results to phy registers
	 *   (wlc_gphy_dc_lut will saturate the results to [-8 to +7])
	 */
	if (pi->hwpwrctrl) {
		wlc_gphy_dc_lut(pi);   /* download results to phy registers */
	} else {
		if (is_initial) {
			gphy_complo(pi, 3, 2, 0);
		} else {
			/* use the current atten state */
			gphy_complo(pi, pi_abg->radiopwr, pi_abg->bb_atten, pi_abg->txctl1);
		}
	}

	/* The BPHY filter near the end of the tx path truncates its output,
	 * introducing a systematic DC offset.
	 * GPHY revid >= 2 (eg 4306c0) BPHY revid >= 6 means
	 * BPHY revid >= 6 gives DC offsets of -2,-2
	 * Older versions of BPHY give DC offsets of -1,-1
	 */
	if (calmode >= 1) {
		uint16 dc_correction;

		/* chip will ultimately run in g-mode, but some cals may be in b-mode */
		phy_reg_write(pi, BPHY_TX_DC_OFF1, dc_offset1_val);
		if (pi->pubpi.phy_rev >= 2) {
			dc_correction = 0x0202;
		} else {
			dc_correction = 0x0101;
		}
		phy_reg_write(pi, BPHY_TX_DC_OFF2, dc_correction);
	}

	/* ------------------------------------ */
	/* Restore previous values of registers */
	/* ------------------------------------ */
	W_REG(pi->sh->osh, &regs->phytest, old_tx_tst);
	phy_reg_write(pi, BPHY_RF_OVERRIDE,   old_brfovr);
	phy_reg_write(pi, BPHY_TR_LOSS_CTL,   old_trloss_ctl);
	phy_reg_write(pi, BPHY_SYNC_CTL,      old_sync_ctl);
	phy_reg_write(pi, dac_ctrl_reg,       old_dac_ctl);
	write_radio_reg(pi, RADIO_2050_PWR_CTL, old_pwr_ctl);
	write_radio_reg(pi, RADIO_2050_RX_CTL0, old_rx_ctl0);

	/*  restore the upper nibble of radio register 0x52 for 2050A1
	 *  This contains the PA & Mixer gain bits
	 * ( txmag_enable == 0 ) ==> 2050A1
	 */
	if (pi_abg->txmag_enable == 0)
		mod_radio_reg(pi, RADIO_2050_TX_CTL1, 0xf0, old_tx_ctl1_high_nibble);

	/* re-enable DC canceller WAR (only under g-mode) */
	W_REG(pi->sh->osh, &regs->phybbconfig, old_bbcfg);
	if (calmode == 0) {
		phy_reg_write(pi, BPHY_PEAK_CNT_THRESH, old_peakCnt);
		phy_reg_write(pi, BPHY_PHYCRSTH,        old_crsth);
	}

	if (gmode) {
		phy_reg_write(pi, GPHY_ANA_OVERRIDE,     old_gphy_analog_ovr);
		phy_reg_write(pi, GPHY_ANA_OVERRIDE_VAL, old_gphy_analog_ovr_val);
		phy_reg_write(pi, GPHY_CLASSIFY_CTRL,    old_classifyctl);
		phy_reg_write(pi, GPHY_RF_OVERRIDE,      old_gphy_rf_ovr);
		phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL,  old_gphy_rf_ovr_val);
		phy_reg_write(pi, BPHY_RFDC_CANCEL_CTL,  old_phyreg3e);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN),
			old_corr_str0_shift_din);
	}

	/* Return HWreceiver state machine to normal operation */
	if (pi->hwpwrctrl) {
		phy_reg_write(pi, GPHY_DC_OFFSET1,        old_phyreg80f & 0xbfff);
		phy_reg_write(pi, GPHY_CTRL,              old_phyreg801);
		phy_reg_write(pi, BPHY_DAC_CONTROL,       old_phyreg60);
		phy_reg_write(pi, BPHY_TX_POWER_OVERRIDE, old_phyreg14);
		phy_reg_write(pi, GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1, old_phyreg478);
	}

	/* Revert back to original channel */
	wlc_synth_pu_war(pi, orig_channel);
	wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(orig_channel));
}


void
WLBANDINITFN(wlc_phy_init_gphy)(phy_info_t *pi)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Initialize the bphy part */
	if (GREV_IS(pi->pubpi.phy_rev, 1))
		wlc_bphy5_init(pi);
	else
		wlc_bphy6_init(pi);

	if (GREV_GE(pi->pubpi.phy_rev, 2) && pi_abg->sbtml_gm)
		pi_abg->loopback_gain = TRUE;

	if (GREV_GE(pi->pubpi.phy_rev, 2) || pi_abg->sbtml_gm) {
		ASSERT(phy_reg_read(pi, BPHY_OPTIONAL_MODES) & OPT_MODE_G);
		wlc_phy_init_aphy(pi);
	}

	/* override analog power down controls so that the ADC & DAC are always on */
	if (GREV_GE(pi->pubpi.phy_rev, 2)) {
		/* Force IQADC, DAC always on from g-phy */
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_WRITE_RAW_ENTRY(GPHY_ANA_OVERRIDE, 0)
			PHY_REG_WRITE_RAW_ENTRY(GPHY_ANA_OVERRIDE_VAL, 0)
		PHY_REG_LIST_EXECUTE(pi);
	}

	if (GREV_IS(pi->pubpi.phy_rev, 2)) {
		/* Remove the RF overrides */
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_WRITE_RAW_ENTRY(GPHY_RF_OVERRIDE, 0)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_RF_OVERRIDE, 0x00c0)
		PHY_REG_LIST_EXECUTE(pi);
	} else if (GREV_GE(pi->pubpi.phy_rev, 6)) {
		/* Remove the RF overrides */
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_WRITE_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x400)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_RF_OVERRIDE, 0x00c0)
		PHY_REG_LIST_EXECUTE(pi);
	}

	/* New crs0 detector enabled. */
	if (pi_abg->sbtml_gm) {
		uint16 aphy_rev = phy_reg_read(pi,
		                               (GPHY_TO_APHY_OFF + APHY_PHYVERSION)) & 0xff;
		if (aphy_rev == 3) {
			PHY_REG_LIST_START_WLBANDINITDATA
				/* short_sym_mf_thd1 = 22, short_sym_mf_thd2 = 24 */
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CRSMF_THRESH0,
					0x1816)
				/* short_sym_mf_thd3 = 6, factRhoSq2 = 6, enable MF detector */
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CRSMF_THRESH1,
					0x8606)
			PHY_REG_LIST_EXECUTE(pi);
		} else if (aphy_rev == 5) {
			PHY_REG_LIST_START_WLBANDINITDATA
				/* short_sym_mf_thd1 = 22, short_sym_mf_thd2 = 24 */
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CRSMF_THRESH0,
					0x1816)
				/* short_sym_mf_thd3 = 6, enable MF detector */
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CRSMF_THRESH1,
					0x8006)
				/* factRhoSq2 = 6, which in new units of 10*log10() with 2
				 * fractional bits is 31
				 */
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_FACT_RHOSQ,
					0xff00, 0x1f00)
			PHY_REG_LIST_EXECUTE(pi);
		} else if (aphy_rev >= 6) {
			/* crs uses log2() arithmetic */
		}
	}

	if ((GREV_LE(pi->pubpi.phy_rev, 2)) && pi_abg->sbtml_gm)
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CRS_DELAY), 0x78);

	/* for 4318 (2050 rev8), we need to flip the IQ ADC output */
	if (pi->pubpi.radiorev == 8) {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_OR_RAW_ENTRY(GPHY_CTRL, 0x80)
			PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_COMP_CTL, 4)
		PHY_REG_LIST_EXECUTE(pi);
	}

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* if capable, calibrate the loopback path.
	 * wlc_find_lpback_gain finds and sets global variables:
	 *   pi->max_lpback_gain_hdB  max available rx gain before clipping
	 *   pi->trsw_rx_gain_hdB     added rx path gain if trsw_rx enabled
	 */
	if (pi_abg->loopback_gain)
		wlc_phy_gphy_find_lpback_gain(pi);

	/* If not done yet, do rc_cal except for 4318 */
	if (pi->pubpi.radiorev != 8) {
		if (pi_abg->rc_cal == 0xffff) {
			/* Do RX filter RC calibration, and cache away results */
			pi_abg->rc_cal = wlc_cal_2050_rc_init(pi, 27);
			PHY_CAL(("wl%d: %s: Saving RC_CAL_OVR as 0x%x\n",
			           pi->sh->unit, __FUNCTION__, pi_abg->rc_cal));
		} else {
			PHY_CAL(("wl%d: %s: Setting RC_CAL_OVR to 0x%x\n",
			           pi->sh->unit, __FUNCTION__, pi_abg->rc_cal));
			write_radio_reg(pi, RADIO_2050_RC_CAL_OVR, pi_abg->rc_cal);
		}
	}

	/* If not done yet, do measure_lo */
	if (pi_abg->mintxbias == 0xffff) {
		/* Do LO leakage trim optimization, and cache away results */
		wlc_phy_cal_gphy_measurelo(pi);
		if (pi->hwpwrctrl)
			wlc_phy_cal_txpower_stats_clr_gphy(pi);
		PHY_CAL(("wl%d: First LOFT: %s: LO Leakage: mintxbias %d; \n",
		           pi->sh->unit, __FUNCTION__, pi_abg->mintxbias));
	} else {
		/* Use the saved values for LO leakage trim */
		PHY_CAL(("wl%d: LOFT: %s: LO Leakage: mintxbias %d; \n",
		           pi->sh->unit, __FUNCTION__, pi_abg->mintxbias));

		if (pi_abg->txmag_enable)
			write_radio_reg(pi, RADIO_2050_TX_CTL1,
				pi_abg->mintxmag << 4 | pi_abg->mintxbias);
		else
			mod_radio_reg(pi, RADIO_2050_TX_CTL1, (TXC1_OFF_I_MASK | TXC1_OFF_Q_MASK),
				pi_abg->mintxbias);

		if (GREV_GE(pi->pubpi.phy_rev, 6))
			phy_reg_mod(pi, BPHY_TX_PWR_CTRL, 0xf000, pi_abg->mintxbias << 12);

		PHY_CAL(("wl%d: %s: Restoring previous mintxbias %d, reg 0x52 = 0x%x \n",
		         pi->sh->unit, __FUNCTION__, pi_abg->mintxbias,
		         read_radio_reg(pi, RADIO_2050_TX_CTL1)));

		/* Init other registers that measlo sets */
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_PACTRL)
			phy_reg_write(pi, BPHY_TX_DC_OFF1, 0x8075);
		else
			phy_reg_write(pi, BPHY_TX_DC_OFF1, 0x807f);
		if (GREV_IS(pi->pubpi.phy_rev, 1))
			phy_reg_write(pi, BPHY_TX_DC_OFF2, 0x0101);
		else
			phy_reg_write(pi, BPHY_TX_DC_OFF2, 0x0202);
	}

	if (pi_abg->sbtml_gm) {
		/* This does not run in gmode0 */
		gphy_complo(pi, pi_abg->radiopwr, pi_abg->bb_atten, pi_abg->txctl1);
		phy_reg_write(pi, GPHY_DC_OFFSET1, 0x8078);
	}

	/* Do NRSSI slope calibration */
	if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_ADCDIV) {
		if (ISGPHY(pi) && pi_abg->sbtml_gm) {
			/* Only once at load */
			if ((pi_abg->abgphy_cal.min_rssi == 0) &&
				(pi_abg->abgphy_cal.max_rssi == 0)) {
				wlc_phy_cal_radio2050_nrssioffset_gmode1(pi);
				wlc_cal_2050_nrssislope_gmode1(pi);
			} else {
				wlc_nrssi_thresh_gphy_set(pi);
			}
		}
	} else {
		gphy_update_nrssi_tbl(pi, pi_abg->nrssi_table_delta);
		wlc_nrssi_thresh_gphy_set(pi);
	}

	/* initialize the RSSISel lookup table for 4318B0 onward */
	if (pi->pubpi.radiorev == 8)
		phy_reg_write(pi, GPHY_RSSI_B0, 0x3230);

	/* init for the pwr ctrl algorithm */
	wlc_bgphy_pwrctrl_init(pi);
}

static void
gphy_classifyctl(phy_info_t *pi, bool cck, bool ofdm)
{
	uint16	cc;

	cc = phy_reg_read(pi, GPHY_CLASSIFY_CTRL);
	cc &= 0xfffc;
	if (cck) cc |= 1;
	if (ofdm) cc |= 2;
	phy_reg_write(pi, GPHY_CLASSIFY_CTRL, cc);
}

static void
gphy_orig_gains(phy_info_t *pi, bool issue_tx)
{
	int ctr, addr, off;
	d11regs_t *regs = pi->regs;

	if (GREV_IS(pi->pubpi.phy_rev, 1)) {
		addr = GPHY_TO_APHY_OFF + 0x14;
		off = 16;
	} else {
		addr = GPHY_TO_APHY_OFF + 1;
		off = 8;
	}

	if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
	    (GREV_GE(pi->pubpi.phy_rev, 7)))
		phy_reg_and(pi, GPHY_RF_OVERRIDE, ~0x0800);

	for (ctr = 0; ctr < 4; ctr++) {
		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
		    (GREV_GE(pi->pubpi.phy_rev, 7)) && (ctr == 3))
			WRITE_APHY_TABLE_ENT(pi, addr, ctr, 2);
		else
			WRITE_APHY_TABLE_ENT(pi, addr, ctr, (uint16)((ctr & 1) << 1) |
			                     ((ctr >> 1) & 1));
	}

	for (ctr = 0; ctr < 16; ctr++) {
		WRITE_APHY_TABLE_ENT(pi, addr, ctr + off, (uint16)ctr);
	}

	/* tcl inlines this: */
	gphy_tr_switch(pi, regs, 1, TRUE);

	/* Issue a cck Tx to get back in crs_reset & issue gain command */
	if (issue_tx) {
		/* override txPu to avoid spewing power */
		uint16 old_phyreg811 = phy_reg_read(pi, 0x811);
		uint16 old_phyreg812 = phy_reg_read(pi, 0x812);
		PHY_REG_LIST_START
			PHY_REG_AND_RAW_ENTRY(0x812, ~0x4)
			PHY_REG_OR_RAW_ENTRY(0x811, 0x4)
		PHY_REG_LIST_EXECUTE(pi);

		wlc_phy_do_dummy_tx(pi, FALSE, ON);

		phy_reg_write(pi, 0x811, old_phyreg811);
		phy_reg_write(pi, 0x812, old_phyreg812);
	}
}

static void
WLBANDINITFN(wlc_phy_gphy_find_lpback_gain)(phy_info_t *pi)
{
	int16 rfAttnr;
	uint16 old_phy429, old_phy001, old_phy811, old_phy812, old_phy814, old_phy815,
	old_phy05a, old_phy059, old_phy058, old_phy00a, old_phy003, old_phy80f, old_phy810,
	old_phy02b, old_phy015, old_rad52, old_rad43, old_rad7a;
	int old_bbattn;
	uint16 k = 0, iqval, pgar, trsw_rx_gain;

	/*  Save... */
	old_phy429 = phy_reg_read(pi, GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN);
	old_phy001 = phy_reg_read(pi, BPHY_BB_CONFIG);
	old_phy811 = phy_reg_read(pi, GPHY_RF_OVERRIDE);
	old_phy812 = phy_reg_read(pi, GPHY_RF_OVERRIDE_VAL);
	old_phy814 = phy_reg_read(pi, GPHY_ANA_OVERRIDE);
	old_phy815 = phy_reg_read(pi, GPHY_ANA_OVERRIDE_VAL);
	old_phy05a = phy_reg_read(pi, BPHY_FREQ_CONTROL);
	old_phy059 = phy_reg_read(pi, BPHY_PHASE_SCALE);
	old_phy058 = phy_reg_read(pi, BPHY_DDFS_ENABLE);
	old_phy00a = phy_reg_read(pi, BPHY_TEST);
	old_phy003 = phy_reg_read(pi, BPHY_ANACORE);
	old_phy80f = phy_reg_read(pi, GPHY_DC_OFFSET1);
	old_phy810 = phy_reg_read(pi, GPHY_DC_OFFSET2);
	old_phy02b = phy_reg_read(pi, BPHY_LO_LEAKAGE);
	old_phy015 = phy_reg_read(pi, BPHY_RF_OVERRIDE);
	old_bbattn = bgphy_get_bbattn(pi);
	old_rad52 = read_radio_reg(pi, RADIO_2050_TX_CTL1);
	old_rad43 = read_radio_reg(pi, RADIO_2050_PWR_CTL);
	old_rad7a = read_radio_reg(pi, RADIO_2050_RX_CTL0);

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Disable aphy crs and reset bphy rx state machine */
		PHY_REG_AND_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN, (uint16)~0xC000)
		PHY_REG_OR_RAW_ENTRY(BPHY_BB_CONFIG, 0x8000)
		/* set the dc-cancellers in high bw mode as we want to see just the tone
		 * on the rx
		 */
		PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x2)
		PHY_REG_AND_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, ~0x2)
		PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x1)
		PHY_REG_AND_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, ~0x1)
		/* over-ride the DAC & ADC to be on */
		PHY_REG_OR_RAW_ENTRY(GPHY_ANA_OVERRIDE, 0x1)
		PHY_REG_AND_RAW_ENTRY(GPHY_ANA_OVERRIDE_VAL, ~0x1)
		PHY_REG_OR_RAW_ENTRY(GPHY_ANA_OVERRIDE, 0x2)
		PHY_REG_AND_RAW_ENTRY(GPHY_ANA_OVERRIDE_VAL, ~0x2)
		/* override txpu & rxpu to be on */
		PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE, 0xc)
		PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, 0xc)
		/* override trsw txpu to be on and trsw rxpu to be off */
		PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x30, 0x30)
		PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, 0x30, 0x10)
		/* b-phy DDFS on and sending a 5MHz tone */
		PHY_REG_WRITE_RAW_ENTRY(BPHY_FREQ_CONTROL, 0xf << 7)
		PHY_REG_WRITE_RAW_ENTRY(BPHY_PHASE_SCALE, 0xc810)
		PHY_REG_WRITE_RAW_ENTRY(BPHY_DDFS_ENABLE, 0x000d)
		PHY_REG_OR_RAW_ENTRY(BPHY_TEST, 0x2000)
		PHY_REG_OR_RAW_ENTRY(GPHY_ANA_OVERRIDE, 0x4)
		PHY_REG_AND_RAW_ENTRY(GPHY_ANA_OVERRIDE_VAL, ~0x4)
		PHY_REG_MOD_RAW_ENTRY(BPHY_ANACORE, 0x60, 0x40)
	PHY_REG_LIST_EXECUTE(pi);

	/*
	 * DDFS puts sinusoid with amplitude of 45 at the DAC input
	 * i.e. I & Q will both be in [-45 45]
	 * with the parameters chosen above
	 * however it gets scaled down further by the scaler in g-phy
	 */

	rfAttnr = 0;
	/*  back off the transmit gain to the minima */
	if (pi->pubpi.radiorev == 8) {
		rfAttnr = 0xf;
		write_radio_reg(pi, RADIO_2050_PWR_CTL, rfAttnr);
	} else {
		write_radio_reg(pi, RADIO_2050_TX_CTL1, 0);
		rfAttnr = MAX_TX_ATTN_2050;
		mod_radio_reg(pi, RADIO_2050_PWR_CTL, 0x0f, rfAttnr);
	}

	ASSERT(rfAttnr);

	bgphy_set_bbattn(pi, 0xb);

	if (GREV_LE(pi->pubpi.phy_rev, 2))
		phy_reg_write(pi, GPHY_DC_OFFSET1, 0x8020);
	else
		phy_reg_write(pi, GPHY_DC_OFFSET1, 0xc020);
	phy_reg_write(pi, GPHY_DC_OFFSET2, 0x0);

	PHY_REG_LIST_START_WLBANDINITDATA
		/*  setup for the power measurement */
		PHY_REG_MOD_RAW_ENTRY(BPHY_LO_LEAKAGE, 0x3f, 0x1)
		PHY_REG_MOD_RAW_ENTRY(BPHY_LO_LEAKAGE, 0x3f00, 0x0800)
		/*
		* loop over the receive gain values till the IQ power is in the range of interest
		* set the LNA to be in low gain state with lna load set to 0
		* this is the setting we would like to use in the calibration procs
		*/
		PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x100)
		PHY_REG_AND_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, ~0x3000)
	PHY_REG_LIST_EXECUTE(pi);

	if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
	    (GREV_GE(pi->pubpi.phy_rev, 7))) {
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x800)
			PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, 0x8000)
		PHY_REG_LIST_EXECUTE(pi);
	}
	and_radio_reg(pi, RADIO_2050_RX_CTL0, 0xf7);

	iqval = 0;
	pgar = 0;
	while (TRUE) {
		/*  set the transmit & receive gain */
		write_radio_reg(pi, RADIO_2050_PWR_CTL, rfAttnr);
		phy_reg_mod(pi, GPHY_RF_OVERRIDE_VAL, 0x0f00, pgar << 8);

		/* trigger the measurement */
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_MOD_RAW_ENTRY(BPHY_RF_OVERRIDE, 0xf000, 0xa000)
			PHY_REG_MOD_RAW_ENTRY(BPHY_RF_OVERRIDE, 0xf000, 0xf000)
		PHY_REG_LIST_EXECUTE(pi);

		/* wait for the measurement to be complete */
		OSL_DELAY(20);
		/* read the IQ accumulator */
		iqval = phy_reg_read(pi, BPHY_LO_IQMAG_ACC);
		PHY_TMP(("%s: IQ accumulator value 0x%x\n", __FUNCTION__, iqval));

		/* is the right gain reached ( sinusoid of amplitude 64 ? ) */
		if (iqval > 0xdfb)
			break;

		if (pgar < 15)
			pgar++;
		else if (rfAttnr > 0)
			rfAttnr--;
		else
			break;
	}

	if (pgar >= 8) {
		/* Set R switch on */
		phy_reg_or(pi, GPHY_RF_OVERRIDE_VAL, 0x30);
		/* Initialize the loop */
		trsw_rx_gain = 27;
		for (k = pgar - 8; k < 16; k++, trsw_rx_gain -= 3) {
			/* set the receive gain */
			phy_reg_mod(pi, GPHY_RF_OVERRIDE_VAL, 0x0f00, k << 8);

			/* trigger the measurement */
			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_MOD_RAW_ENTRY(BPHY_RF_OVERRIDE, 0xf000, 0xa000)
				PHY_REG_MOD_RAW_ENTRY(BPHY_RF_OVERRIDE, 0xf000, 0xf000)
			PHY_REG_LIST_EXECUTE(pi);

			/* wait for the measurement to be complete */
			OSL_DELAY(20);

			/* read the IQ accumulator */
			iqval = phy_reg_read(pi, BPHY_LO_IQMAG_ACC);
			PHY_TMP(("%s: IQ accumulator value 0x%x\n", __FUNCTION__, iqval));

			/* is the right gain reached ( sinusoid of amplitude 64 ? ) */
			if (iqval > 0xdfb)
				break;
		}
	} else {
		trsw_rx_gain = 24;
		PHY_TMP(("%s: Can not compute TR switch R gain, assuming 0x%x\n",
		         __FUNCTION__, trsw_rx_gain));
	}


	/* ... and restore */
	phy_reg_write(pi, GPHY_ANA_OVERRIDE, old_phy814);
	phy_reg_write(pi, GPHY_ANA_OVERRIDE_VAL, old_phy815);
	phy_reg_write(pi, BPHY_FREQ_CONTROL, old_phy05a);
	phy_reg_write(pi, BPHY_PHASE_SCALE, old_phy059);
	phy_reg_write(pi, BPHY_DDFS_ENABLE, old_phy058);
	phy_reg_write(pi, BPHY_TEST, old_phy00a);
	phy_reg_write(pi, BPHY_ANACORE, old_phy003);
	phy_reg_write(pi, GPHY_DC_OFFSET1, old_phy80f);
	phy_reg_write(pi, GPHY_DC_OFFSET2, old_phy810);
	phy_reg_write(pi, BPHY_LO_LEAKAGE, old_phy02b);
	phy_reg_write(pi, BPHY_RF_OVERRIDE, old_phy015);
	bgphy_set_bbattn(pi, old_bbattn);
	write_radio_reg(pi, RADIO_2050_TX_CTL1, old_rad52);
	write_radio_reg(pi, RADIO_2050_PWR_CTL, old_rad43);
	write_radio_reg(pi, RADIO_2050_RX_CTL0, old_rad7a);

	/* at this point all the other registers have been restored,
	 * disable the gphy rf overrides except the two DC cancellers
	 * both the DC cancellers are in high BW mode
	 */
	phy_reg_write(pi, GPHY_RF_OVERRIDE, old_phy811 | 3);

	/* after disabling the RF overrides, the phys take control of txpu, rxpu etc
	 * wait for 10 us for the DC cancellers to cancel all the DC in high BW mode
	 */
	OSL_DELAY(10);

	/* after waiting 10 us, disable the DC canceller RF overrides */
	phy_reg_write(pi, GPHY_RF_OVERRIDE, old_phy811);
	phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, old_phy812);
	phy_reg_write(pi, GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN, old_phy429);
	phy_reg_write(pi, BPHY_BB_CONFIG, old_phy001);

	/* the equation to use for computing the PGA gain is
	 *	pga * 6 <= bb_attn + 4*rf_attn-sg*10+$temp
	 */
	rfAttnr = (6 * pgar) - 11 - (4 * rfAttnr);
	PHY_CAL(("%s: pgar = %d, returning %d, trsw = %d\n", __FUNCTION__, k, rfAttnr,
	         trsw_rx_gain));
	pi->u.pi_abgphy->trsw_rx_gain_hdB = 2 * trsw_rx_gain;
	pi->u.pi_abgphy->max_lpback_gain_hdB = rfAttnr;
}

/* Set init, clips, and packet gains (LNA, PGA, and T/R switch) to fixed values
 * NOTE: tr of 1 means "R", 0 means "T", -1 means leave it alone
 */
static void
gphy_all_gains(phy_info_t *pi, uint16 lna, uint16 pga, int tr, bool issue_tx)
{
	int addr, off, ctr;
	d11regs_t *regs = pi->regs;

	if (GREV_IS(pi->pubpi.phy_rev, 1)) {
		addr = GPHY_TO_APHY_OFF + 0x14;
		off = 16;
	} else {
		addr = GPHY_TO_APHY_OFF + 1;
		off = 8;
	}

	if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
	    (GREV_GE(pi->pubpi.phy_rev, 7))) {
		/* enable the override for the external lna */
		phy_reg_or(pi, GPHY_RF_OVERRIDE, 0x0800);
		if (lna == 3) {
			/* ext lna on + int lna at "10" */
			lna = 2;
			phy_reg_and(pi, GPHY_RF_OVERRIDE_VAL, 0x7fff);
		} else {
			/* ext lna off */
			phy_reg_or(pi, GPHY_RF_OVERRIDE_VAL, 0x8000);
		}
	}

	for (ctr = 0; ctr < 4; ctr++) {
		WRITE_APHY_TABLE_ENT(pi, addr, ctr, lna);
	}

	for (ctr = 0; ctr < 16; ctr++) {
		WRITE_APHY_TABLE_ENT(pi, addr, (ctr + off), pga);
	}

	if (tr == -1) {
		/* tr of "1" means "R", "0" means "T", -1 (unused so far)
		 * means leave T/R switch settings unchanged.
		 */
	} else {
		gphy_tr_switch(pi, regs, tr, FALSE);
	}

	/* Issue a cck Tx to get back in crs_reset & issue gain command */
	if (issue_tx) {
		/* override txPu to avoid spewing power */
		uint16 old_phyreg811 = phy_reg_read(pi, 0x811);
		uint16 old_phyreg812 = phy_reg_read(pi, 0x812);
		PHY_REG_LIST_START
			PHY_REG_AND_RAW_ENTRY(0x812, ~0x4)
			PHY_REG_OR_RAW_ENTRY(0x811, 0x4)
		PHY_REG_LIST_EXECUTE(pi);

		wlc_phy_do_dummy_tx(pi, FALSE, ON);

		phy_reg_write(pi, 0x811, old_phyreg811);
		phy_reg_write(pi, 0x812, old_phyreg812);
	}
}

/* Fix T/R switch setting independent of gain settings
 * NOTE: tr of "1" means "R", "0" means "T"
 */
static void
gphy_tr_switch(phy_info_t *pi, d11regs_t *regs, int tr, bool n1n2no6)
{
	uint16 tmp, trbits;

	ASSERT((tr == 0) || (tr == 1));

	trbits = (tr << 14) | (tr << 6);

	/* Force T/R bit in gclipnrssipow1initIndex */
	tmp = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_P1_IDX));
	tmp &= 0xbfbf;
	tmp |= trbits;
	phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_P1_IDX), tmp);

	/* Force T/R bit in gclippow1pow2Index */
	tmp = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_IDX));
	tmp &= 0xbfbf;
	tmp |= trbits;
	phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_IDX), tmp);

	/* Force T/R bit in gclipnrssi1nrssi2Index */
	tmp = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_N2_IDX));
	tmp &= 0xbfbf;
	if (n1n2no6)
		tmp |= (tr << 14);
	else
		tmp |= trbits;
	phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_N2_IDX), tmp);
}

/* Turn on/off aphy digital frequency control */
static void
aphy_dcfc(phy_info_t *pi, bool on)
{
	if (AREV_GT(pi->pubpi.phy_rev, 2)) {
		/* only dcfc possible in revid 3 aphy */
		return;
	}

	if (on) {
		/* Turn on digital freq corrxn */
		phy_reg_write(pi, APHY_COARSE_UPD_CTL, 1);
	} else {
		/* Turn on analog freq corrxn */
		phy_reg_write(pi, APHY_COARSE_UPD_CTL, 0);
	}
}

static uint16 WLBANDINITDATA(aphy_lna_hpf1_gain_table)[] = {
	0xfff8,
	0xfff8,
	0xfff8,
	0xfff8,
	0xfff8,
	0xfff9,
	0xfffc,
	0xfffe,
	0xfff8,
	0xfff8,
	0xfff8,
	0xfff8,
	0xfff8,
	0xfff8,
	0xfff8,
	0xfff8,
	0xffff
};

static uint16 WLBANDINITDATA(aphy_lna_hpf1_gain_table_rev3)[] = {
	0x0820,		/* Set WRSSI table for LNA gain for init_gain */
	0x0820,
	0x0920,
	0x0c38,
	0x0820,		/* Set WRSSI table for HPVGA1 gain for init_gain */
	0x0820,
	0x0820,
	0x0820,
	0x0820,		/* Set WRSSI table for LNA gain for clip_low */
	0x0820,
	0x0920,
	0x0a38,
	0x0820,		/* Set WRSSI table for HPVGA1 gain for clip_low */
	0x0820,
	0x0820,
	0x0820,
	/*  JUST COPIED FROM CLIP_LOW -- needs to be corrected */
	0x0820,		/* Set WRSSI table for LNA gain for clip_med_low */
	0x0820,
	0x0920,
	0x0a38,
	/*  JUST COPIED FROM CLIP_LOW -- needs to be corrected */
	0x0820,		/* Set WRSSI table for HPVGA1 gain for clip_med_low */
	0x0820,
	0x0820,
	0x0820,
	/*  JUST COPIED FROM CLIP_LOW -- needs to be corrected */
	0x0820,		/* Set WRSSI table for LNA gain for clip_med_high */
	0x0820,
	0x0920,
	0x0a38,
	/*  JUST COPIED FROM CLIP_LOW -- needs to be corrected */
	0x0820,		/* Set WRSSI table for HPVGA1 gain for clip_med_high */
	0x0820,
	0x0820,
	0x0820,
	/*  JUST COPIED FROM CLIP_LOW -- needs to be corrected */
	0x0820,		/* Set WRSSI table for LNA gain for clip_high */
	0x0820,
	0x0920,
	0x0a38,
	/*  JUST COPIED FROM CLIP_LOW -- needs to be corrected */
	0x0820,		/* Set WRSSI table for HPVGA1 gain for clip_high */
	0x0820,
	0x0820,
	0x0820,
	/*  JUST COPIED FROM CLIP_LOW -- needs to be corrected */
	0x0820,		/* Set WRSSI table for LNA gain for clip_wb */
	0x0820,
	0x0920,
	0x0a38,
	/*  JUST COPIED FROM CLIP_LOW -- needs to be corrected */
	0x0820,		/* Set WRSSI table for HPVGA1 gain for clip_wb */
	0x0820,
	0x0820,
	0x0820,
	0xffff
};

/* Turn on/off aphy RSSI-based gain control */
static void
WLBANDINITFN(aphy_rssi_agc)(phy_info_t *pi, bool on)
{
	int k;
	uint16 v, *t;
	d11regs_t *regs = (d11regs_t *)pi->regs;

	if (on) {
		if (AREV_IS(pi->pubpi.phy_rev, 2)) {
			/*
			 * Set LNA, HPF1 gain regs which depend on RSSI
			 *	lna	= [-8 -8 -8 -8 -8 -7 -4 -2]
			 *	hpf1	= [-8 -8 -8 -8 -8 -8 -8 -8]
			 *
			 * (WRSSILnaHpf1GainTableAddress)
			 */
			phy_reg_write(pi, APHY_TABLE_ADDR, 0x0c00);
			W_REG(pi->sh->osh, &regs->phyregaddr, APHY_TABLE_DATA_I);
			(void)R_REG(pi->sh->osh, &regs->phyregaddr);
			t = aphy_lna_hpf1_gain_table;
			while ((v = *t++) != 0xffff) {
				W_REG(pi->sh->osh, &regs->phyregdata, v);
			}

			/*
			 * Set CLIP LNA, HPF1 gain regs which depend on RSSI
			 *	lna	= [-8 -8 -8 -8 -8 -7 -6 -2]
			 *	hpf1	= [-8 -8 -8 -8 -8 -8 -8 -8]
			 *
			 * (ClipWRSSILnaHpf1GainTableAddress)
			 */
			t = aphy_lna_hpf1_gain_table;
		} else {
			t = aphy_lna_hpf1_gain_table_rev3;
		}
		phy_reg_write(pi, APHY_TABLE_ADDR, 0x1000);
		W_REG(pi->sh->osh, &regs->phyregaddr, APHY_TABLE_DATA_I);
		(void)R_REG(pi->sh->osh, &regs->phyregaddr);
		while ((v = *t++) != 0xffff) {
			W_REG(pi->sh->osh, &regs->phyregdata, v);
		}
	} else {
		if (AREV_IS(pi->pubpi.phy_rev, 2)) {
			/*
			 * Set LNA, HPF1 gain regs which depend on RSSI to -8
			 * (WRSSILnaHpf1GainTableAddress)
			 */
			v = 0xfff8;
			phy_reg_write(pi, APHY_TABLE_ADDR, 0x0c00);
			W_REG(pi->sh->osh, &regs->phyregaddr, APHY_TABLE_DATA_I);
			(void)R_REG(pi->sh->osh, &regs->phyregaddr);
			for (k = 0; k < 16; k++) {
				W_REG(pi->sh->osh, &regs->phyregdata, v);
			}

			/*
			 * Set LNA, HPF1 gain regs which depend on RSSI to -8
			 * (ClipWRSSILnaHpf1GainTableAddress)
			 */
		} else {
			/* write all 12 tables (4 entries each) with -32,-32 */
			v = 0x0820;
		}
		phy_reg_write(pi, APHY_TABLE_ADDR, 0x1000);
		W_REG(pi->sh->osh, &regs->phyregaddr, APHY_TABLE_DATA_I);
		(void)R_REG(pi->sh->osh, &regs->phyregaddr);
		for (k = 0; k < 16; k++) {
			W_REG(pi->sh->osh, &regs->phyregdata, v);
		}
	}
}

static void
aphy_all_crs(phy_info_t *pi, bool on)
{
	uint16 tmp, regaddr;

	/* for Rev > 0 aphy/gphy, turn off main crs_enable */
	regaddr = (ISGPHY(pi)) ? (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN)
			: APHY_CTHR_STHR_SHDIN;
	tmp = phy_reg_read(pi, regaddr);

	if (on) {
		phy_reg_write(pi, regaddr, tmp | 0x8000);
	} else {
		phy_reg_write(pi, regaddr, tmp & 0x7fff);
	}
}

static void
aphy_crs0(phy_info_t *pi, bool on)
{
	uint16 tmp;

	/* for Rev 1 aphy, control crs with enable bits */

	tmp = phy_reg_read(pi, APHY_CTHR_STHR_SHDIN);

	phy_reg_write(pi, APHY_CTHR_STHR_SHDIN,
		(on ? (tmp | APHY_CTHR_CRS1_ENABLE) : (tmp & ~APHY_CTHR_CRS1_ENABLE)));
}


static void
WLBANDINITFN(aphy_ww)(phy_info_t *pi, bool on)
{
	uint16 old_pwrdwn, ictr, samp, best_samp, best_cm;
	uint i;

	aphy_all_crs(pi, FALSE);

	if (on) {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* turn on dot11a_ww flag */
			PHY_REG_OR_RAW_ENTRY(APHY_GAIN_INFO, 0x1000)
			/* change num_crs0_needed to 3 */
			PHY_REG_MOD_RAW_ENTRY(APHY_N1_P1_GAIN_SETTLE, 0x0f00, 0x300)
		PHY_REG_LIST_EXECUTE(pi);

		or_radio_reg(pi, RADIO_2060WW_RX_GM_UPDN, 0x80);
		/* turn on NB direct out */
		or_radio_reg(pi, RADIO_2060WW_RX_SP_REG1, 0x10);
		/* set WRSSI direct_out=0, range_ctrl=1 */
		mod_radio_reg(pi, RADIO_2060WW_RXRSSI_DACC, 3, 2);

		/* set init gain */
		aphy_init_gain(pi, 7, 7, 0, 7, 1, 1);

		/* this one gets rid of hump around -57 dBm... we reduced
		 * the clip var threshold, get out of ED quicker
		 */
		phy_reg_write(pi, APHY_WW_CLIPVAR_THRESH, 0x3ed5);

		/* calibrate the nrssi common mode voltage */
		/* 2060ww_nrssicm_cal: */

		/* from pg. 15 of regs_ww pdf
		 *
		 * assume that since we are in ww, we are in NRSSI direct out mode
		 * aphy_ww is the only thing that currently calls this function
		 */

		/* turn off crs */
		/* aphy_all_crs(pi, FALSE); just done above */

		/* set aphy_all_gains to the gain we want to use */
		/* aphy_init_gain(pi, 7, 7, 0, 7, 1, 1); just done above */

		/* set mux override to NRSSI but save old value first */
		old_pwrdwn = phy_reg_read(pi, APHY_PWRDWN);
		phy_reg_mod(pi, APHY_PWRDWN, 7, 5);

		/* turn off the rxmixpd */
		or_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, 4);

		/* sample collect and add up the values for each possible gm_updn
		 * min_val is 16 because we want bit 4 on (direct out NB)
		 * for RF_rx_sp_reg1
		 */
		best_cm = 32;
		best_samp = 0xffff;
		for (ictr = 16; ictr < 32; ictr++) {
			/* set common mode values to different values */
			write_radio_reg(pi, RADIO_2060WW_RX_SP_REG1, ictr);

			/* sample collect at this value */
			samp = phy_reg_read(pi, APHY_WRSSI_NRSSI) & 0xff;
			if (samp == 0) {
				best_cm = ictr;
				break;
			}

			/* Absolute value */
			if (samp > 127)
				samp = 256 - samp;

			if (samp < best_samp) {
				best_cm = ictr;
				best_samp = samp;
			}
		}

		/* restore old phyreg 0x3 value */
		phy_reg_write(pi, APHY_PWRDWN, old_pwrdwn);

		/* turn back on the rxmixpd */
		and_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, ~4);

		/* Set the best value */
		write_radio_reg(pi, RADIO_2060WW_RX_SP_REG1, best_cm);

		/* turn back on crs */
		/* aphy_all_crs(pi, TRUE); done below */

		/* for all lna atten tables, set to nominal -20 dB although
		 * there is a frequency dependence.  Currently using the T/R switch for
		 * isolation, not the LNA attenuator bit in the 2060.
		 */
		WRITE_APHY_TABLE_ENT(pi, 0x13, 0, -20);

		PHY_REG_LIST_START_WLBANDINITDATA
			/* tweak of clip power gains */
			PHY_REG_WRITE_RAW_ENTRY(APHY_WW_CLIP2_IDX, 0x1e80)
			PHY_REG_WRITE_RAW_ENTRY(APHY_WW_CLIP1_IDX, 0x1c00)
			PHY_REG_WRITE_RAW_ENTRY(APHY_WW_CLIP0_IDX, 0x0ec0)
			/* set nrssi high power threshold (value to throw T/R switch) */
			PHY_REG_WRITE_RAW_ENTRY(APHY_WW_CLIP1_THRESH, 0xc0)
			/* turn off wrssi threshold by maxing out, not using dwell on wrssi
			* to make agc decisions
			*/
			PHY_REG_WRITE_RAW_ENTRY(APHY_WW_CLIPWRSSI_IDX, 0x1fff)
			/* set nb pwr dwell time to 19 clk20s
			* set wrssi dwell time to 2 clk20s
			* set dwell on wrssi enable to 0
			*/
			PHY_REG_MOD_RAW_ENTRY(APHY_NB_WRRSI_WAIT, 0x0fff, 0x053)
			/* change the TR LUT so that T is tied to both
			* antennas when we go to high clip state
			*/
			/* new values for mPCI
			*   atten_ctrl  = 001 (0 -> LNA bit, 0 -> R switch, 1 -> T switch)
			*   atten_fixed =   1
			*/
			PHY_REG_MOD_RAW_ENTRY(APHY_CTL, 0x01e0, 0x0120)
			/* for both main & aux:
			*     when tx_pu, rx_pu both high, make CTL2, CTL4 high
			*/
			PHY_REG_MOD_RAW_ENTRY(APHY_TR_LUT1, 0xf000, 0x3000)
			PHY_REG_MOD_RAW_ENTRY(APHY_TR_LUT2, 0xf000, 0x3000)
		PHY_REG_LIST_EXECUTE(pi);

		/* modify gain tables to match measured 2060 values */
		WRITE_APHY_TABLE_ENT(pi, 0x0, 6, 23);
		for (i = 0; i < 6; i++)
			WRITE_APHY_TABLE_ENT(pi, 0x0, i, 15);

		/* hpf1 values are a little low */
		WRITE_APHY_TABLE_ENT(pi, 0x0, 13, 14);
		WRITE_APHY_TABLE_ENT(pi, 0x0, 14, 17);
		WRITE_APHY_TABLE_ENT(pi, 0x0, 15, 19);

		/* change max_samp_coarse_3 and str0_min_ctr3
		 * doesn't have to be reduced if not dwelling on wrssi
		 */
		phy_reg_write(pi, APHY_STRN_MIN_REAL, 0x5030);
	} else {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* turn off dot11a_ww flag */
			PHY_REG_AND_RAW_ENTRY(APHY_GAIN_INFO, ~0x1000)
			/* change num_crs0_needed to 1 */
			PHY_REG_MOD_RAW_ENTRY(APHY_N1_P1_GAIN_SETTLE, 0x0f00, 0x100)
		PHY_REG_LIST_EXECUTE(pi);

		mod_radio_reg(pi, RADIO_2060WW_RX_GM_UPDN, 0xc0, 0x40);
	}

	aphy_all_crs(pi, TRUE);
}

/* aphy, all revs (including gmode) */
void
WLBANDINITFN(wlc_phy_init_aphy)(phy_info_t *pi)
{
	phy_info_abgphy_t *pi_abg;
	pi_abg = pi->u.pi_abgphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(PHY_TYPE(R_REG(pi->sh->osh, &pi->regs->phyversion)) != PHY_TYPE_NULL);

	if (AREV_GE(pi->pubpi.phy_rev, 6)) {
		int offset = ISAPHY(pi) ? 0 : GPHY_TO_APHY_OFF;

		/* reset dot11a_ww, as 2060 is in 13-bit gain mode initially
		 * the reset default value is 1, for aphy_revid >= 4.
		 */
		if (ISAPHY(pi)) {
			phy_reg_and(pi, 0x1b, ~0x1000);
		}

		/* read encore enable bit */
		if (pi->pubpi.abgphy_encore) {
			/* enable signal field based averaging */
			phy_reg_or(pi, offset + APHY_VITERBI_OFFSET, 0x10);
		} else {
			/* disable signal field based averaging */
			phy_reg_and(pi, offset + APHY_VITERBI_OFFSET, ~0x10);
		}
	}

	wlc_aphy_setup(pi);

	if (ISAPHY(pi)) {
		/* This is basic mode: */
		aphy_dcfc(pi, TRUE);
		aphy_rssi_agc(pi, FALSE);
		aphy_crs0(pi, TRUE);

		if (!NORADIO_ENAB(pi->pubpi)) {
			/* radio present, not QT */
			wlc_radio_2060_init(pi);

			/* Bringup boards do not have the necessary hardware to do loft. */
			if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
			    ((pi->sh->boardtype == BU4306_BOARD) ||
			     (pi->sh->boardtype == BU4309_BOARD))) {
			} else {
				/* Run lof cal once */
				if (pi_abg->tx_vos == 0xffff) {
					wlc_aphy_lof_cal(pi, LOF_CAL_VOS_AND_TX_BB);
					aphy_set_tx_iq_based_on_vos(pi, 1);
				} else {
					PHY_CAL(("wl%d: %s: Setting TX_VOS_CTL to 0x%x\n",
					         pi->sh->unit, __FUNCTION__, pi_abg->tx_vos));
					write_radio_reg(pi, RADIO_2060WW_TX_VOS_CTL,
						pi_abg->tx_vos);
				}
			}
		}

		if (AREV_GT(pi->pubpi.phy_rev, 2))
			aphy_ww(pi, TRUE);

		/* Get idle tssi if we are going to do pwr control */
		if (pi_abg->m_tssi_to_dbm != NULL || pi_abg->a_tssi_to_dbm != NULL) {
			wlc_aphy_pwrctrl_init(pi);
		}

#if defined(AP) && defined(RADAR)
		/* Initialize Radar detect, on or off */
		wlc_phy_radar_detect_init(pi, pi->sh->radar);
#endif
	} else {
		ASSERT((pi->pubpi.phy_type == PHY_TYPE_G) && pi_abg->sbtml_gm);

		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_PACTRL) {
			phy_reg_mod(pi, (GPHY_TO_APHY_OFF + APHY_ROTATE_FACT), 0x1fff, 0x3cf);
		}
	}
}


void
wlc_phy_cal_radio2050_nrssislope(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* check D11 is running on Fast Clock */
	if (D11REV_GE(pi->sh->corerev, 5))
		ASSERT(si_core_sflags(pi->sh->sih, 0, 0) & SISF_FCLKA);

	switch (pi->pubpi.phy_type) {
	case PHY_TYPE_G:
		CASECHECK(PHYTYPE, PHY_TYPE_G);
		wlc_cal_2050_nrssislope_gmode1(pi);
		break;
	case PHY_TYPE_A:
	case PHY_TYPE_N:
		break;
	default:
		ASSERT(0);
		break;
	}
}

void
wlc_phy_cal_radio2050_nrssioffset_gmode1(phy_info_t *pi)
{
	uint16 old_phyreg001, old_radioreg7a, old_radioreg811, old_radioreg812, k,
		old_radioreg814, old_radioreg815, old_radioreg05a, old_radioreg059,
		old_radioreg058, old_radioreg00a, old_radioreg003, old_radioreg43,
		old_phyreg2e = 0, old_phyreg2f = 0, old_phyreg80f = 0, old_phyreg810 = 0,
		old_phyreg801 = 0, old_phyreg60 = 0, old_phyreg14 = 0, old_phyreg478 = 0;
	int done, maxRSSI, minRSSI, rssiOffset = 0;

	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if ((pi->pubpi.phy_type != PHY_TYPE_G) || (pi->pubpi.radiorev != 8))
		return;

	/* check D11 is running on Fast Clock */
	if (D11REV_GE(pi->sh->corerev, 5))
		ASSERT(si_core_sflags(pi->sh->sih, 0, 0) & SISF_FCLKA);

	PHY_CAL(("%s: Entering, now = %d\n", __FUNCTION__, pi->sh->now));

	/*  store the locally modified registers */
	old_phyreg001 = phy_reg_read(pi, BPHY_BB_CONFIG);
	old_radioreg7a = read_radio_reg(pi, RADIO_2050_RX_CTL0);
	old_radioreg811 = phy_reg_read(pi, GPHY_RF_OVERRIDE);
	old_radioreg812 = phy_reg_read(pi, GPHY_RF_OVERRIDE_VAL);

	old_radioreg814 = phy_reg_read(pi, GPHY_ANA_OVERRIDE);
	old_radioreg815 = phy_reg_read(pi, GPHY_ANA_OVERRIDE_VAL);
	old_radioreg05a = phy_reg_read(pi, BPHY_FREQ_CONTROL);
	old_radioreg059 = phy_reg_read(pi, BPHY_PHASE_SCALE);
	old_radioreg058 = phy_reg_read(pi, BPHY_DDFS_ENABLE);
	old_radioreg00a = phy_reg_read(pi, BPHY_TEST);
	old_radioreg003 = phy_reg_read(pi, BPHY_ANACORE);
	old_radioreg43  = read_radio_reg(pi, RADIO_2050_PWR_CTL);

	/* stop aphy and bphy state machines */
	aphy_all_crs(pi, FALSE);
	PHY_REG_LIST_START
		PHY_REG_MOD_RAW_ENTRY(BPHY_BB_CONFIG, 0xC000, 0x4000)
		/* set rf and analog overrides */
		PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE, 0xc)
		PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, 0xc, 0x4)
	PHY_REG_LIST_EXECUTE(pi);

	/* disable packet classification */
	gphy_classifyctl(pi, FALSE, FALSE);

	/* save more registers if HW power control */
	if (pi->hwpwrctrl) {
		old_phyreg2e = phy_reg_read(pi, 0x2e);
		old_phyreg2f = phy_reg_read(pi, 0x2f);
		old_phyreg80f = phy_reg_read(pi, 0x80f);
		old_phyreg810 = phy_reg_read(pi, 0x810);
		old_phyreg801 = phy_reg_read(pi, 0x801);
		old_phyreg60 = phy_reg_read(pi, 0x60);
		old_phyreg14 = phy_reg_read(pi, 0x14);
		old_phyreg478 = phy_reg_read(pi, 0x478);

		PHY_REG_LIST_START
			PHY_REG_WRITE_RAW_ENTRY(0x2e, 0x00)
			PHY_REG_WRITE_RAW_ENTRY(0x2f, 0x0)
			PHY_REG_WRITE_RAW_ENTRY(0x80f, 0x0)
			PHY_REG_WRITE_RAW_ENTRY(0x810, 0x0)
			/* txDigScalerOvr : override the digital scalar which is
			* driven by rate based gain offset
			*/
			PHY_REG_OR_RAW_ENTRY(0x478, 0x100)
			/* Enable the override on the gphy scalar */
			PHY_REG_OR_RAW_ENTRY(0x801, 0x40)
			PHY_REG_OR_RAW_ENTRY(0x60, 0x40)
			PHY_REG_OR_RAW_ENTRY(0x14, 0x200)
		PHY_REG_LIST_EXECUTE(pi);
	}


	/* Set radio in 2050 mode for RSSI swing, DC estimator cancellation, and */
	/* I/Q ADC common-mode voltage */
	or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x70);

	/* find MaxRSSI */
	or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x80);
	OSL_DELAY(30);
	maxRSSI = (phy_reg_read(pi, GPHY_TO_APHY_OFF + APHY_WRSSI_NRSSI) >> 8) & 0x3f;
	if (maxRSSI > 31)
		maxRSSI -= 64;

	PHY_CAL(("%s: default offset value = %d\n", __FUNCTION__, maxRSSI));
	/* "Finding the optimal offset to avoid clipping at RSSI ADC input ..." */

	/* default rssiOffset value */
	rssiOffset = 0;

	/* adjust the offset, till maxRSSI is in the limit */
	if (maxRSSI == 31) {
		/* "maxRSSI clipping" */
		/* "Finding the minimum negative offset required" */
		done = 0;
		/* decrease the offset */
		for (k = 7; k > 3; k--) {
			write_radio_reg(pi, RADIO_2050SC_RX_CTL1, k);
			OSL_DELAY(20);
			maxRSSI = (phy_reg_read(pi, GPHY_TO_APHY_OFF + APHY_WRSSI_NRSSI)
			           >> 8) & 0x3f;
			if (maxRSSI > 31)
				maxRSSI -= 64;

			if ((maxRSSI < 31) && (done == 0)) {
				done = 1;
				rssiOffset = k;
			}
		}
		if (done == 0) {
			done = 1;
			rssiOffset = 4;
		}
	} else {
		PHY_CAL(("%s:  maxRSSI not clipping\n", __FUNCTION__));
		/*  remove RSSI Reset */
		and_radio_reg(pi, 0x7a, 0x7f);

		PHY_REG_LIST_START
			/*  override the DAC to be on */
			PHY_REG_OR_RAW_ENTRY(0x814, 0x1)
			PHY_REG_AND_RAW_ENTRY(0x815, 0xfffe)
			/*  override txpu & rxpu to be on */
			PHY_REG_OR_RAW_ENTRY(0x811, 0xc)
			PHY_REG_OR_RAW_ENTRY(0x812, 0xc)
			/*  override trsw txpu and trsw rxpu to be on */
			PHY_REG_OR_RAW_ENTRY(0x811, 0x30)
			PHY_REG_OR_RAW_ENTRY(0x812, 0x30)
			/*  b-phy DDFS on and sending a 3MHz tone */
			PHY_REG_WRITE_RAW_ENTRY(0x05a, 0x9 << 7)
			PHY_REG_WRITE_RAW_ENTRY(0x059, 0x810)
			PHY_REG_WRITE_RAW_ENTRY(0x058, 0x000d)
		PHY_REG_LIST_EXECUTE(pi);

		if (pi->pubpi.ana_rev > 0) {
			phy_reg_or(pi, 0x00a, 0x2000);
		} else {
			phy_reg_write(pi, 0x003, 0x122);
		}

		PHY_REG_LIST_START
			PHY_REG_OR_RAW_ENTRY(0x814, 0x4)
			PHY_REG_AND_RAW_ENTRY(0x815, ~0x4)
			PHY_REG_MOD_RAW_ENTRY(0x003, 0x60, 0x40)
		PHY_REG_LIST_EXECUTE(pi);

		/*  Set LNA_LOAD = 1; LNA gain = -2 dB */
		/*  Set dc_est2 slow-mode b/w to 230 kHz */
		or_radio_reg(pi, 0x7a, 0x0f);

		/*  LNA ON, PGA = -1 dB, T/R = 'R', and */
		/*  issue tx to ensure gain is forced	 */
		/*  Transmit a dummy B frame to switch GPHY DAC mux select */
		/*  to BPHY txresampler output */
		/*  This allows for DDFS to operate properly */
		gphy_all_gains(pi, 3, 0, 1, TRUE);

		mod_radio_reg(pi, RADIO_2050_PWR_CTL, ~0xf0, 0x0f);

		OSL_DELAY(30);

		/*  read minRSSI */
		minRSSI = (phy_reg_read(pi, 0x47f) >> 8) & 0x3f;
		if (minRSSI > 31)
			minRSSI -= 64;

		PHY_CAL(("%s: minRSSI is %d\n", __FUNCTION__, minRSSI));

		if (minRSSI == -32) {
			PHY_CAL(("minRSSI clipping. Finding min positive offset required\n"));

			/* Finding the maximum positive offset that can be used */
			/* increase the offset */
			done = 0;
			rssiOffset = 0;
			for (k = 0; k < 4; k++) {
				write_radio_reg(pi, RADIO_2050SC_RX_CTL1, k);
				OSL_DELAY(30);
				minRSSI = (phy_reg_read(pi,
				                        GPHY_TO_APHY_OFF + APHY_WRSSI_NRSSI) >> 8) &
				        0x3f;
				if (minRSSI > 31)
					minRSSI -= 64;

				PHY_CAL(("%s: minRSSI %d\n", __FUNCTION__, minRSSI));
				if ((minRSSI > -32) && (done == 0)) {
					rssiOffset = k;
					done = 1;
				}
			}
			if (done == 0) {
				done = 1;
				rssiOffset = 3;
			}
		} else {
			PHY_CAL(("minRSSI is also not clipping\n"));
		}
	}

	/* store the result */
	pi_abg->nrssi_slope_offset = rssiOffset;
	PHY_CAL(("%s: rssiOffset of %d is the optimal setting\n", __FUNCTION__, rssiOffset));

	/* restore the locally modified registers */
	if (pi->hwpwrctrl) {
		phy_reg_write(pi, 0x2e, old_phyreg2e);
		phy_reg_write(pi, 0x2f, old_phyreg2f);
		phy_reg_write(pi, 0x80f, old_phyreg80f);
		phy_reg_write(pi, 0x810, old_phyreg810);
	}

	phy_reg_write(pi, GPHY_ANA_OVERRIDE, old_radioreg814);
	phy_reg_write(pi, GPHY_ANA_OVERRIDE_VAL, old_radioreg815);
	phy_reg_write(pi, BPHY_FREQ_CONTROL, old_radioreg05a);
	phy_reg_write(pi, BPHY_PHASE_SCALE, old_radioreg059);
	phy_reg_write(pi, BPHY_DDFS_ENABLE, old_radioreg058);
	phy_reg_write(pi, BPHY_TEST, old_radioreg00a);
	phy_reg_write(pi, BPHY_ANACORE, old_radioreg003);
	write_radio_reg(pi, RADIO_2050_PWR_CTL, old_radioreg43);

	write_radio_reg(pi, RADIO_2050_RX_CTL0, old_radioreg7a);
	gphy_classifyctl(pi, TRUE, TRUE);
	aphy_all_crs(pi, TRUE);
	gphy_orig_gains(pi, TRUE);

	if (pi->hwpwrctrl) {
		phy_reg_write(pi, 0x801, old_phyreg801);
		phy_reg_write(pi, 0x60, old_phyreg60);
		phy_reg_write(pi, 0x14, old_phyreg14);
		phy_reg_write(pi, 0x478, old_phyreg478);
	}

	phy_reg_write(pi, BPHY_BB_CONFIG, old_phyreg001);
	phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, old_radioreg812);
	phy_reg_write(pi, GPHY_RF_OVERRIDE, old_radioreg811);

	/* Record time of this calibration */
	/* We want to separate offset and slope calibrations (to minimize the contiguous
	 * serious of reg writes on systems with slow busses). Introduce an offset
	 * between the two routines.
	 */
	pi_abg->abgphy_cal.abgphy_cal_noffset = pi->sh->now;
	pi_abg->abgphy_cal.abgphy_cal_nslope = pi->sh->now - pi->sh->slow_timer + SLOPE_DELAY;
}

static void
wlc_cal_2050_nrssislope_gmode1(phy_info_t *pi)
{
	uint16	old_rx_ctl0, old_bbconfig, old_anacore, old_rfoverride,
		old_testreg, old_tx_ctl1, old_pwr_ctl, old_freqctrl, old_phasescale,
		old_ddfsen, old_phyreg2e, old_phyreg2f, old_phyreg80f, old_phyreg810,
		old_phyreg801, old_phyreg60, old_phyreg14, old_phyreg478;
	int max_rssi, min_rssi, freq;
	d11regs_t *regs = pi->regs;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	ASSERT(pi->pubpi.phy_type == PHY_TYPE_G);

	if (pi->pubpi.radiorev > 8) {
		PHY_ERROR(("%s: Unsupported radiorev %d\n",
		          __FUNCTION__, pi->pubpi.radiorev));
		return;
	}
	PHY_CAL(("%s: Entering, now = %d\n", __FUNCTION__, pi->sh->now));

	/* Set the result from the previous run of nrssioffset() if appropriate */
	if (pi->pubpi.radiorev == 8) {
		write_radio_reg(pi, RADIO_2050SC_RX_CTL1,
			(uint16)pi_abg->nrssi_slope_offset & 0xffff);
	}

	/* Compiler happiness */
	old_phyreg2e = old_phyreg2f = old_phyreg80f = old_phyreg810 =
		old_phyreg801 = old_phyreg60 = old_phyreg14 = old_phyreg478 = 0;

	/* turn off carrier sense for Gphy/Aphy */
	aphy_all_crs(pi, FALSE);

	/* Turn off classification of OFDM/DSSS/CCK frames */
	gphy_classifyctl(pi, FALSE, FALSE);

	/* turn off bphy receiver state machine */
	old_bbconfig = R_REG(pi->sh->osh, &regs->phybbconfig);
	OR_REG(pi->sh->osh, &regs->phybbconfig, 0x8000);

	/* Save off old register values */
	old_rx_ctl0 = read_radio_reg(pi, RADIO_2050_RX_CTL0);
	old_rfoverride = phy_reg_read(pi, BPHY_RF_OVERRIDE);
	old_testreg = R_REG(pi->sh->osh, &regs->phytest);
	old_anacore = R_REG(pi->sh->osh, &regs->phyanacore);
	old_tx_ctl1 = read_radio_reg(pi, RADIO_2050_TX_CTL1);
	old_pwr_ctl = read_radio_reg(pi, RADIO_2050_PWR_CTL);
	old_freqctrl = phy_reg_read(pi, BPHY_FREQ_CONTROL);
	old_phasescale = phy_reg_read(pi, BPHY_PHASE_SCALE);
	old_ddfsen = phy_reg_read(pi, BPHY_DDFS_ENABLE);

	/* save more registers if HW power control */
	if (pi->hwpwrctrl) {
		old_phyreg2e  = phy_reg_read(pi, BPHY_TX_DC_OFF1);
		old_phyreg2f  = phy_reg_read(pi, BPHY_TX_DC_OFF2);
		old_phyreg80f = phy_reg_read(pi, GPHY_DC_OFFSET1);
		old_phyreg810 = phy_reg_read(pi, GPHY_DC_OFFSET2);
		old_phyreg801 = phy_reg_read(pi, GPHY_CTRL);
		old_phyreg60  = phy_reg_read(pi, BPHY_DAC_CONTROL);
		old_phyreg14  = phy_reg_read(pi, BPHY_TX_POWER_OVERRIDE);
		old_phyreg478 = phy_reg_read(pi, GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1);

		PHY_REG_LIST_START
			PHY_REG_WRITE_RAW_ENTRY(BPHY_TX_DC_OFF1, 0x0)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_TX_DC_OFF2, 0x0)
			PHY_REG_WRITE_RAW_ENTRY(GPHY_DC_OFFSET1, 0x0)
			PHY_REG_WRITE_RAW_ENTRY(GPHY_DC_OFFSET2, 0x0)
			/* txDigScalerOvr : override the digital scalar which is driven
			* by rate based gain offset
			*/
			PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1, 0x100)
			/* Enable the override on the gphy scalar */
			PHY_REG_OR_RAW_ENTRY(GPHY_CTRL, 0x40)
			PHY_REG_OR_RAW_ENTRY(BPHY_DAC_CONTROL, 0x40)
			PHY_REG_OR_RAW_ENTRY(BPHY_TX_POWER_OVERRIDE, 0x200)
		PHY_REG_LIST_EXECUTE(pi);
	}

	/* Set radio in 2050 mode for RSSI swing, DC estimator cancellation, and
	 * I/Q ADC common-mode voltage
	 */
	or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x70);

	/* LNA off, PGA = 8*3-1=23 dB, T/R = 'T', and
	 * issue tx to ensure gain is forced
	 */
	gphy_all_gains(pi, 0, 8, 0, TRUE);

	/* Set LNA_LOAD = 0; LNA gain = -10 dB
	 * This gives further more suppression of any spurious rx inputs
	 * into the rssi circuitry
	 */
	and_radio_reg(pi, RADIO_2050_RX_CTL0, 0xf7);

	/* Get maxRSSI (no signal input) */

	if (GREV_GT(pi->pubpi.phy_rev, 1)) {
		/* Force tr_sw_tx_pu and tr_sw_rx_pu on c0 */
		PHY_REG_LIST_START
			PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x30, 0x30)
			PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, 0x30, 0x10)
		PHY_REG_LIST_EXECUTE(pi);
	}

	/* Force no-input/low power (<-90 dBm) signal to RSSI circuitry */
	or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x80);

	/* Ensure AGC steady state before making a measurement */
	OSL_DELAY(20);

	max_rssi = (phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_WRSSI_NRSSI)) >> 8) & 0x3f;
	if (max_rssi > 31) max_rssi -= 64;

	and_radio_reg(pi, RADIO_2050_RX_CTL0, 0x7f);

	/* Get estimate of low end of RSSI vs. input power curve so we can
	 * calc slope by transmitting a very large signal.
	 */

	if (pi->pubpi.ana_rev > 1) {
		phy_reg_mod(pi, BPHY_ANACORE, 0x60, 0x40);
	}

	/* Set mux to test output (DDFS). */
	OR_REG(pi->sh->osh, &regs->phytest, 0x2000);

	/* Set LNA_LOAD = 1; LNA gain = -2 dB
	 * Set dc_est2 slow-mode b/w to 230 kHz
	 */
	or_radio_reg(pi, RADIO_2050_RX_CTL0, 0xf);

	/* Force tx_pu, rx_pu and pa_ramp high and LNA to max. */
	phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xf330);

	if (GREV_GT(pi->pubpi.phy_rev, 1)) {
		/* Force tr_sw_tx_pu and tr_sw_rx_pu on c0 */
		PHY_REG_LIST_START
			PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, 0x30, 0x20)
			PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x30, 0x20)
		PHY_REG_LIST_EXECUTE(pi);
	}

	/* LNA ON, PGA = -1 dB, T/R = 'R', and
	 * issue tx to ensure gain is forced
	 * Transmit a dummy B frame to switch GPHY DAC mux select
	 * to BPHY txresampler output
	 * This allows for DDFS to operate properly
	 */
	gphy_all_gains(pi, 3, 0, 1, TRUE);

	if (pi->pubpi.radiorev == 8) {
		/* force high output power writing to pwr ctrl regs in 2050 */
		write_radio_reg(pi, RADIO_2050_PWR_CTL, 0x1f);
	} else {
		/* force high output power writing to turn on extra pa_gains */
		mod_radio_reg(pi, RADIO_2050_TX_CTL1, 0xf0, 0x60);

		/* force high output power writing to pwr ctrl regs in 2050 */
		mod_radio_reg(pi, RADIO_2050_PWR_CTL, 0x0f, 0x9);
	}

	/* Configure DDFS to send out a +3 MHz complex sinusoid on I and Q outputs */
	freq = 9;
	phy_reg_write(pi, BPHY_FREQ_CONTROL, freq << 7);
	phy_reg_write(pi, BPHY_PHASE_SCALE, 0x0810);
	phy_reg_write(pi, BPHY_DDFS_ENABLE, 0xd);

	/* Ensure AGC steady state before making a measurement */
	OSL_DELAY(20);
	/* Get min RSSI value (max. signal). */
	min_rssi = (phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_WRSSI_NRSSI)) >> 8) & 0x3f;
	if (min_rssi > 31) min_rssi -= 64;

	if ((min_rssi >= 0 || max_rssi <= 0 || (max_rssi <= min_rssi))) {
		PHY_CAL(("wl%d: SKIPPING nrssislope_gmode1: min_rssi %d, max_rssi %d\n",
		          pi->sh->unit, min_rssi, max_rssi));
		goto bad;
	}

	if (max_rssi == min_rssi) {
		PHY_ERROR(("wl%d: %s: NRSSI cal failed: Max and Min RSSI both = %d, setting slope"
		          " = 1\n", pi->sh->unit, __FUNCTION__, min_rssi));
		pi_abg->abgphy_cal.nrssi_slope_scale = 65536;
	} else {
		pi_abg->abgphy_cal.nrssi_slope_scale = (64 * 65536) / (max_rssi - min_rssi);
	}

	PHY_CAL(("wl%d: %s: slope_scale %d min_rssi %d, max_rssi %d\n",
	          pi->sh->unit, __FUNCTION__, pi_abg->abgphy_cal.nrssi_slope_scale,
		min_rssi, max_rssi));

	/* safety: if max_rssi > -5, take it, otherwise skip */
	if (max_rssi > -5) {
		pi_abg->abgphy_cal.min_rssi = min_rssi;
		pi_abg->abgphy_cal.max_rssi = max_rssi;
	} else
		PHY_ERROR(("wl%d: %s: ignoring bad max_rssi: %d\n", pi->sh->unit, __FUNCTION__,
		          max_rssi));

bad:	/* Restore old register values */

	if (pi->hwpwrctrl) {
		phy_reg_write(pi, BPHY_TX_DC_OFF1, old_phyreg2e);
		phy_reg_write(pi, BPHY_TX_DC_OFF2, old_phyreg2f);
		phy_reg_write(pi, GPHY_DC_OFFSET1, old_phyreg80f);
		phy_reg_write(pi, GPHY_DC_OFFSET2, old_phyreg810);
	}
	write_radio_reg(pi, RADIO_2050_RX_CTL0, old_rx_ctl0);
	W_REG(pi->sh->osh, &regs->phybbconfig, old_bbconfig);
	phy_reg_write(pi, BPHY_RF_OVERRIDE, old_rfoverride);

	if (GREV_GT(pi->pubpi.phy_rev, 1)) {
		/* Unforce tr_sw_tx_pu and tr_sw_rx_pu on c0 */
		PHY_REG_LIST_START
			PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, 0x30, 0)
			PHY_REG_MOD_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x30, 0)
		PHY_REG_LIST_EXECUTE(pi);
	}

	W_REG(pi->sh->osh, &regs->phyanacore, old_anacore);
	W_REG(pi->sh->osh, &regs->phytest, old_testreg);
	write_radio_reg(pi, RADIO_2050_TX_CTL1, old_tx_ctl1);
	write_radio_reg(pi, RADIO_2050_PWR_CTL, old_pwr_ctl);
	phy_reg_write(pi, BPHY_FREQ_CONTROL, old_freqctrl);
	phy_reg_write(pi, BPHY_PHASE_SCALE, old_phasescale);
	phy_reg_write(pi, BPHY_DDFS_ENABLE, old_ddfsen);

	wlc_synth_pu_war(pi, CHSPEC_CHANNEL(pi->radio_chanspec));

	/* Turn ON classification of OFDM and DSSS/CCK frames */
	gphy_classifyctl(pi, TRUE, TRUE);
	/* The dummy_ofdm_tx in gphy_orig_gains ensures that init_gain is
	 * forced out to 2050 as well as the aphy_rfoverrides are now active
	 */
	gphy_orig_gains(pi, TRUE);
	aphy_all_crs(pi, TRUE);
	if (pi->hwpwrctrl) {
		phy_reg_write(pi, GPHY_CTRL, old_phyreg801);
		phy_reg_write(pi, BPHY_DAC_CONTROL, old_phyreg60);
		phy_reg_write(pi, BPHY_TX_POWER_OVERRIDE, old_phyreg14);
		phy_reg_write(pi, GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1, old_phyreg478);
	}

	/* All done, fix the nrssi table */
	wlc_nrssi_tbl_gphy_mod(pi);

	/* And reset the threshold */
	wlc_nrssi_thresh_gphy_set(pi);

	/* Record time of last calibration */
	pi_abg->abgphy_cal.abgphy_cal_nslope = pi->sh->now;
}

/* An array that does a nibble bit-reverse */
static const uint16 flipmap[] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

/* Table to statically modify the rc_cal_val in the 2050 radio
 * This is equivalent to:
 *	tmp = flipmap(rc_cal0);		Bit-flip it
 *	tmp += 4;			Add 4
 *	if (tmp > 15) tmp = 15;		Saturate
 *	rc_cal1 = flipmap(tmp);		Bit-flip it back
 */

static const uint16 WLBANDINITDATA(rc_cal_add_4)[] = {
/* rc0	swap	+4	sat	swap	*/
/* 0	0	4	4	2	*/	0x2,
/* 1	8	c	c	3	*/	0x3,
/* 2	4	8	8	1	*/	0x1,
/* 3	c	10	f	f	*/	0xf,
/* 4	2	6	6	6	*/	0x6,
/* 5	a	e	e	7	*/	0x7,
/* 6	6	a	a	5	*/	0x5,
/* 7	e	12	f	f	*/	0xf,
/* 8	1	5	5	a	*/	0xa,
/* 9	9	d	d	b	*/	0xb,
/* a	5	9	9	9	*/	0x9,
/* b	d	11	f	f	*/	0xf,
/* c	3	7	7	e	*/	0xe,
/* d	b	f	f	f	*/	0xf,
/* e	7	b	b	d	*/	0xd,
/* f	f	13	f	f	*/	0xf,
};

static uint16
WLBANDINITFN(wlc_cal_2050_rc_init)(phy_info_t *pi, uint16 highfreq)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	d11regs_t *regs = pi->regs;
	uint16	old_brfovr, radiopwr, old_tx_ctl1, rc4, anareg, testreg, syncreg,
		tmpreg, freq, old_freqctrl, old_phasescale, old_ddfsen,
		old_tx_ctl0, old_rfovr = 0, old_rfovr_val = 0, old_anaovr = 0,
		old_anaovr_val = 0, old_cthr_sthr = 0, old_classifyctl = 0,
		old_phyreg80f = 0, old_phyreg810 = 0;
	uint32	iqvalaccum, iqval, iqaccum;
	uint16 loff_pon_don = 0, loff_poff_don = 0, lon_poff_don = 0, lon_poff_doff = 0;
	uint16 gphy_reg811_val = 0;
	uint16 extLnaCtrl;
	int i, k;
	bool is_g1;

	/* check D11 is running on Fast Clock */
	if (D11REV_GE(pi->sh->corerev, 5))
		ASSERT(si_core_sflags(pi->sh->sih, 0, 0) & SISF_FCLKA);

	freq = 9;

	is_g1 = pi_abg->sbtml_gm;

	radiopwr = read_radio_reg(pi, RADIO_2050_PWR_CTL);
	old_tx_ctl0 = read_radio_reg(pi, RADIO_2050_TX_CTL0);
	old_tx_ctl1 = read_radio_reg(pi, RADIO_2050_TX_CTL1);
	old_brfovr = phy_reg_read(pi, BPHY_RF_OVERRIDE);
	old_freqctrl = phy_reg_read(pi, BPHY_FREQ_CONTROL);
	old_phasescale = phy_reg_read(pi, BPHY_PHASE_SCALE);
	old_ddfsen = phy_reg_read(pi, BPHY_DDFS_ENABLE);

	ASSERT(pi->pubpi.phy_type == PHY_TYPE_G);

	if (is_g1) {
		ASSERT(pi->pubpi.phy_rev > 1);

		old_rfovr = phy_reg_read(pi, GPHY_RF_OVERRIDE);
		old_rfovr_val = phy_reg_read(pi, GPHY_RF_OVERRIDE_VAL);
		old_anaovr = phy_reg_read(pi, GPHY_ANA_OVERRIDE);
		old_anaovr_val = phy_reg_read(pi, GPHY_ANA_OVERRIDE_VAL);
		old_cthr_sthr = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN));
		old_classifyctl = phy_reg_read(pi, GPHY_CLASSIFY_CTRL);

		/* force ADC, DAC power downs to 0 */
		phy_reg_write(pi, GPHY_ANA_OVERRIDE, old_anaovr | 3);
		phy_reg_write(pi, GPHY_ANA_OVERRIDE_VAL, old_anaovr_val & 0xfffc);

		/* disable a-phy crs_fsm state machine */
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN),
		              old_cthr_sthr & 0x7fff);

		/* disable the classifier */
		phy_reg_write(pi, GPHY_CLASSIFY_CTRL, old_classifyctl & 0xfffc);

		/* enable g-phy overrides for 2050 gain, fltr_rx_ctrl,
		 * fltr_rx_ctrl_lpf, tr_switch & synth_pu
		 * for external lna cases we need to enable ext_lna override also
		 */
		if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
		    (GREV_GE(pi->pubpi.phy_rev, 7))) {
			gphy_reg811_val = 0x09b3;
			loff_pon_don = 0x8fb2;
			loff_poff_don = 0x80b2;
			lon_poff_don = 0x20b2;
			lon_poff_doff = 0x20b3;
		} else {
			gphy_reg811_val = 0x01b3;
			loff_pon_don = 0x0fb2;
			loff_poff_don = 0x00b2;
			lon_poff_don = 0x30b2;
			lon_poff_doff = 0x30b3;
		}

		/* find the receive gain that can be used */
		if (pi_abg->loopback_gain) {
			int err, totgain;
			uint16 lnaval, pgaval;

			/* compute the PGA gain to be used */
			old_phyreg80f = phy_reg_read(pi, GPHY_DC_OFFSET1);
			old_phyreg810 = phy_reg_read(pi, GPHY_DC_OFFSET2);

			if (GREV_LE(pi->pubpi.phy_rev, 2))
				phy_reg_write(pi, GPHY_DC_OFFSET1, 0x8020);
			else
				phy_reg_write(pi, GPHY_DC_OFFSET1, 0xc020);

			phy_reg_write(pi, GPHY_DC_OFFSET2, 0x0);

			if (pi->pubpi.radiorev == 8)
				totgain = 2 + (4 * 15) - (10 * 0) + pi_abg->max_lpback_gain_hdB;
			else
				totgain = 2 + (4 * 9) - (10 * 0) + pi_abg->max_lpback_gain_hdB;

			PHY_CAL(("%s: Total gain is %d\n", __FUNCTION__, totgain));
			if (totgain >= 70) {
				lnaval = 0x3000;
				totgain = totgain - 70;
			} else if (totgain >= 58) {
				lnaval = 0x1000;
				totgain = totgain - 58;
			} else if (totgain >= 46) {
				lnaval = 0x2000;
				totgain = totgain - 46;
			} else {
				lnaval = 0x0000;
				totgain = totgain - 16;
			}

			err = 7;
			for (pgaval = 0; pgaval < 16 && err >= 6; pgaval++) {
				err = totgain - pgaval * 6;
			}

			PHY_CAL(("%s: New LOLkage : PGA & LNA Val used in RC cal are %d &"
			         " %d\n", __FUNCTION__, pgaval, lnaval));

			pgaval = pgaval << 8;

			if ((BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_EXTLNA) &&
			    (GREV_GE(pi->pubpi.phy_rev, 7))) {
				gphy_reg811_val = 0x09b3;
				if (lnaval == 0x3000)
					extLnaCtrl = 0x0000;
				else
					extLnaCtrl = 0x8000;
				loff_pon_don = 0x8f92;
				loff_poff_don = 0x8092 | pgaval | lnaval | extLnaCtrl;
				lon_poff_don = 0x2092 | pgaval | lnaval | extLnaCtrl;
				lon_poff_doff = 0x2093 | pgaval | lnaval | extLnaCtrl;
			} else {
				gphy_reg811_val = 0x01b3;
				loff_pon_don = 0x0f92;
				loff_poff_don = 0x0092 | pgaval | lnaval;
				lon_poff_don = 0x0092 | pgaval | lnaval;
				lon_poff_doff = 0x0093 | pgaval | lnaval;
			}

			PHY_CAL(("%s: loff_pon_don is 0x%x\n", __FUNCTION__, loff_pon_don));
			PHY_CAL(("%s: loff_poff_don is 0x%x\n", __FUNCTION__, loff_poff_don));
			PHY_CAL(("%s: lon_poff_don is 0x%x\n", __FUNCTION__, lon_poff_don));
			PHY_CAL(("%s: New LOLkage : lon_poff_doff is 0x%x\n", __FUNCTION__,
			         lon_poff_doff));
		}

		phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, loff_pon_don);
		phy_reg_write(pi, GPHY_RF_OVERRIDE, gphy_reg811_val);
	}

	/* Force receiver state machine in reset state */
	OR_REG(pi->sh->osh, &regs->phybbconfig, 0x8000);

	/* save old toggleCutoff */
	syncreg = phy_reg_read(pi, BPHY_SYNC_CTL);

	/* Force togglecutoff to 0 to enable fltr_rx_ctrl_ovr to be effective */
	phy_reg_write(pi, BPHY_SYNC_CTL, (syncreg & ~SYN_TOGGLE_CUTOFF));

	anareg = R_REG(pi->sh->osh, &regs->phyanacore);
	testreg = R_REG(pi->sh->osh, &regs->phytest);

	if (pi->pubpi.ana_rev > 1) {
		phy_reg_mod(pi, BPHY_ANACORE, 0x60, 0x40);
	}
	OR_REG(pi->sh->osh, &regs->phytest, 0x2000);

	/* Calculate rc+4 in case we don't find a better value */
	tmpreg = read_radio_reg(pi, RADIO_2050_RX_LPF);
	rc4 = rc_cal_add_4[(tmpreg >> 1) & 0xf];
	rc4 = 0x20 | (rc4 << 1) | (tmpreg & 1);

	if (is_g1) {
		/* for gmode gain, fltr_rx_ctrl, tr_switch & synth_pu are controlled
		 * through g-phy
		 */
		phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, loff_pon_don);
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		/* Transmit tone only */
		PHY_REG_WRITE_RAW_ENTRY(BPHY_RF_OVERRIDE, 0xbfaf)
		/* cap_len bit 13:8 and trig_del 5:0
		 * cap_len = 20 usec; trig_del = 3 usec
		 */
		PHY_REG_WRITE_RAW_ENTRY(BPHY_LO_LEAKAGE, 0x1403)
	PHY_REG_LIST_EXECUTE(pi);

	if (is_g1) {
		/* for gmode gain, fltr_rx_ctrl, tr_switch & synth_pu are controlled
		 * through g-phy
		 */
		phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, loff_poff_don);
	}

	/* init rx_tx_syn[11:10]_pa[9:8]_fltr[7:6]_agc[5:0] */
	phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xbfa0);

	/* Widen analog tx filter and set gains. */
	or_radio_reg(pi, RADIO_2050_TX_CTL0, 0x4);
	if (pi->pubpi.radiorev == 8) {
		write_radio_reg(pi, RADIO_2050_PWR_CTL, MAX_TX_ATTN_4318);
	} else {
		write_radio_reg(pi, RADIO_2050_TX_CTL1, 0);
		mod_radio_reg(pi, RADIO_2050_PWR_CTL, 0x0f, MAX_TX_ATTN_2050);
	}

	phy_reg_write(pi, BPHY_DDFS_ENABLE, 0);

	iqvalaccum = 0;
	for (k = 0; k < 16; k++) {
		/* init rx_tx_syn[11:10]_pa[9:8]_fltr[7:6]_agc[5:0] */
		phy_reg_write(pi, BPHY_FREQ_CONTROL, freq << 7);

		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_WRITE_RAW_ENTRY(BPHY_PHASE_SCALE, 0xc810)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_DDFS_ENABLE, 0xd)
		PHY_REG_LIST_EXECUTE(pi);

		if (is_g1)
			phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, lon_poff_don);
		phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xafb0);
		OSL_DELAY(10);
		if (is_g1)
			phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, lon_poff_don);
		phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xefb0);
		OSL_DELAY(10);
		if (is_g1)
			phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, lon_poff_doff);
		phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xfff0);
		OSL_DELAY(30);
		iqval = phy_reg_read(pi, BPHY_LO_IQMAG_ACC);
		phy_reg_write(pi, BPHY_DDFS_ENABLE, 0);
		iqvalaccum += iqval;
		if (is_g1)
			phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, lon_poff_don);
		phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xafb0);
	}

	/* shift right by 8 to get upper 8bits and by 1 more to divide by 2. */
	iqvalaccum++;
	iqvalaccum >>= 9;

	OSL_DELAY(10);

	phy_reg_write(pi, BPHY_DDFS_ENABLE, 0);

	for (i = 0; i < 16; i++) {
		write_radio_reg(pi, RADIO_2050_RC_CAL_OVR, 0x20 | (flipmap[i] << 1));
		tmpreg = read_radio_reg(pi, RADIO_2050_RC_CAL_OVR);

		OSL_DELAY(10);

		iqaccum = 0;
		for (k = 0; k < 16; k++) {
			/* init rx_tx_syn[11:10]_pa[9:8]_fltr[7:6]_agc[5:0] */
			phy_reg_write(pi, BPHY_FREQ_CONTROL, highfreq << 7);

			PHY_REG_LIST_START_WLBANDINITDATA
				PHY_REG_WRITE_RAW_ENTRY(BPHY_PHASE_SCALE, 0xc810)
				PHY_REG_WRITE_RAW_ENTRY(BPHY_DDFS_ENABLE, 0xd)
			PHY_REG_LIST_EXECUTE(pi);

			if (is_g1)
				phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, lon_poff_don);
			phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xafb0);
			OSL_DELAY(10);
			if (is_g1)
				phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, lon_poff_don);
			phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xefb0);
			OSL_DELAY(10);
			if (is_g1)
				phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, lon_poff_doff);
			phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xfff0);
			OSL_DELAY(30);
			iqval = phy_reg_read(pi, BPHY_LO_IQMAG_ACC);

			phy_reg_write(pi, BPHY_DDFS_ENABLE, 0);
			if (is_g1)
				phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, lon_poff_don);
			phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xafb0);

			iqaccum += iqval;
		}

		iqaccum++;
		iqaccum >>= 8;

		if (iqaccum > iqvalaccum) {
			goto out;
		}
	}

out:
	/* put everything back the way it was
	 * -- except radioreg RADIO_2050_RC_CAL_OVR
	 */
	phy_reg_write(pi, BPHY_RF_OVERRIDE, old_brfovr);

	write_radio_reg(pi, RADIO_2050_TX_CTL0, old_tx_ctl0);
	write_radio_reg(pi, RADIO_2050_TX_CTL1, old_tx_ctl1);
	write_radio_reg(pi, RADIO_2050_PWR_CTL, radiopwr);
	phy_reg_write(pi, BPHY_FREQ_CONTROL, old_freqctrl);
	phy_reg_write(pi, BPHY_PHASE_SCALE, old_phasescale);
	phy_reg_write(pi, BPHY_DDFS_ENABLE, old_ddfsen);
	W_REG(pi->sh->osh, &regs->phyanacore, anareg);
	W_REG(pi->sh->osh, &regs->phytest, testreg);

	/* Set togglecutoff back to 1 to enable syncFSM to drive fltr_rx_ctrl */
	phy_reg_write(pi, BPHY_SYNC_CTL, syncreg);

	wlc_synth_pu_war(pi, CHSPEC_CHANNEL(pi->radio_chanspec));

	/* Return receiver state machine to normal operation */
	AND_REG(pi->sh->osh, &regs->phybbconfig, ~0x8000);
	if (is_g1) {
		phy_reg_write(pi, GPHY_RF_OVERRIDE, old_rfovr);
		phy_reg_write(pi, GPHY_RF_OVERRIDE_VAL, old_rfovr_val);
		phy_reg_write(pi, GPHY_ANA_OVERRIDE, old_anaovr);
		phy_reg_write(pi, GPHY_ANA_OVERRIDE_VAL, old_anaovr_val);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN), old_cthr_sthr);
		phy_reg_write(pi, GPHY_CLASSIFY_CTRL, old_classifyctl);
		if (pi_abg->loopback_gain) {
			phy_reg_write(pi, GPHY_DC_OFFSET1, old_phyreg80f);
			phy_reg_write(pi, GPHY_DC_OFFSET2, old_phyreg810);
		}
	}

	if (i < 16) {
		PHY_CAL(("wl%d: %s: Found %d as rc_cal_val, reg. rc_cal_ovr = 0x%x\n",
		           pi->sh->unit, __FUNCTION__, i, tmpreg));
		return (tmpreg);
	} else {
		PHY_ERROR(("wl%d: %s: Cal failed, setting rc+4: 0x%x\n",
		          pi->sh->unit, __FUNCTION__, rc4));
		return (rc4);
	}
}

#if defined(BCMDBG) || defined(WLTEST)
static void
wlc_pa_on(phy_info_t *pi, bool control)
{
	uint16 tx_pa_val, pa_pddet, pa_powerdet, pa_pd, pa_ctrl,
		pa_ramp, ovr_vals, out_en, ovr_reg, ovr_mask;

	if (!ISAPHY(pi)) {
		PHY_ERROR(("wl%d: %s: Must be APHY\n",
			pi->sh->unit, __FUNCTION__));
		return;
	}

	if (control) {
		/* enable output of pa_powerdet, pa_pddet, pa_pd */
		if (AREV_GE(pi->pubpi.phy_rev, 5)) {
			/* set rx PA to tx mode
			 * pa_pddet, pa_powerdet, pa_ramp don't exist anymore
			 * get pa_pd & pa_ctrl values for TxOn
			 */

			tx_pa_val = read_aphy_table_ent(pi, 0xe, 0x8);
			pa_pd = (tx_pa_val >> 4) & 0x1;
			ovr_vals = pa_pd << 3;
			out_en = 0x2000;
			ovr_reg = 0x08;
		} else {
			/* set rx PA to tx mode */
			tx_pa_val = read_aphy_table_ent(pi, 0xe, 0x8);
			pa_pddet = (tx_pa_val >> 6) & 0x1;
			pa_powerdet = (tx_pa_val >> 5) & 0x1;
			pa_pd = (tx_pa_val >> 4) & 0x1;
			pa_ctrl = (tx_pa_val >> 1) & 0x7;
			pa_ramp = tx_pa_val & 0x1;
			ovr_vals = (pa_ramp << 8) | (pa_ctrl << 5);
			ovr_vals = ovr_vals | (pa_powerdet << 4) | (pa_pd << 3);
			ovr_vals = ovr_vals | (pa_pddet << 2);
			out_en = 0xe000;

			/* enable overrides on bits 8-2 */
			ovr_reg = 0x1fc;
		}
		ovr_mask = 0xffff ^ ovr_reg;
		phy_reg_mod(pi,  0x11, ~ovr_mask, ovr_vals);
		phy_reg_or(pi,  0x10, ovr_reg | out_en);
	} else {
		if (AREV_GE(pi->pubpi.phy_rev, 5))
			ovr_reg = 0x08;
		else
			ovr_reg = 0x1fc;
		ovr_mask = 0xffff ^ ovr_reg;
		phy_reg_and(pi, 0x10, ovr_mask);
		phy_reg_and(pi, 0x11, ovr_mask);
	}
}

void
wlc_phy_test_freq_accuracy_prep_abgphy(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	if (ISGPHY(pi)) {
		W_REG(pi->sh->osh, &regs->phyanacore, pi_abg->freq_anacore);
		W_REG(pi->sh->osh, &regs->phytest, pi_abg->freq_test);
		phy_reg_write(pi, BPHY_RF_OVERRIDE, pi_abg->freq_rf_override);
		phy_reg_write(pi, BPHY_FREQ_CONTROL, pi_abg->freq_freq_ctrl);
		phy_reg_write(pi, BPHY_PHASE_SCALE, pi_abg->freq_phase_scale);
		phy_reg_write(pi, BPHY_DDFS_ENABLE, pi_abg->freq_ddfs_enable);
		if (pi_abg->sbtml_gm)
			aphy_all_crs(pi, TRUE);
	}
	if (ISAPHY(pi)) {
		/*
		 * Restore registers we clobbered.
		 */
		W_REG(pi->sh->osh, &regs->phytest, pi_abg->freq_test);
		phy_reg_write(pi, APHY_RF_OVERRIDE, pi_abg->freq_rf_override);
		phy_reg_write(pi, APHY_RF_OVERRIDE_VAL, pi_abg->freq_pwr);
		write_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, pi_abg->freq_freq_ctrl);
		phy_reg_write(pi, APHY_DC_BIAS, pi_abg->freq_phase_scale);
	}
}

void
wlc_phy_test_freq_accuracy_run_abgphy(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	if (ISAPHY(pi)) {
		uint16 tx_pa_val, pa_pddet, pa_powerdet, pa_pd, pa_ctrl, pa_ramp;
		uint16 out_en, ovr_vals;
		uint16 pwr_dynctl, tx_on;

		/* Save registers we clobber. */
		pi_abg->freq_test = R_REG(pi->sh->osh, &regs->phytest);
		pi_abg->freq_rf_override = phy_reg_read(pi, APHY_RF_OVERRIDE);
		pi_abg->freq_pwr = phy_reg_read(pi, APHY_RF_OVERRIDE_VAL);
		pi_abg->freq_freq_ctrl = read_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL);
		pi_abg->freq_phase_scale = phy_reg_read(pi, APHY_DC_BIAS);

		/* Turn off all CRS */
		aphy_all_crs(pi, FALSE);

		/*
		 * force 2060 tx on, TR switch on, PA on
		 *   and send max constant value from DC offset
		 *   register APHY_DC_BIAS
		 */

		/* turn on "cal" mode which outputs DC value in reg 0x6b */
		phy_reg_write(pi, APHY_DC_BIAS, 0x8080);
		W_REG(pi->sh->osh, &regs->phytest, 0x1);

		if (AREV_GT(pi->pubpi.phy_rev, 2)) {
			/* for 4306c0 DAC doesn't get enabled until a frame is transmitted */
			wlc_phy_do_dummy_tx(pi, TRUE, ON);
		}

		PHY_REG_LIST_START
			/* tr_switch: */
			PHY_REG_OR_RAW_ENTRY(APHY_RF_OVERRIDE, 0xc00)
			/* set rx_pu_val = 0, tx_pu_val = 1 */
			PHY_REG_MOD_RAW_ENTRY(APHY_RF_OVERRIDE_VAL, 0x0c00, 0x400)
		PHY_REG_LIST_EXECUTE(pi);

		/* pa_on: */

		/* set rx PA to tx mode */
		tx_pa_val = read_aphy_table_ent(pi, 0xe, 0x8);
		pa_pddet = (tx_pa_val >> 6) & 0x1;
		pa_powerdet = (tx_pa_val >> 5) & 0x1;
		pa_pd = (tx_pa_val >> 4) & 0x1;
		pa_ctrl = (tx_pa_val >> 1) & 0x7;
		pa_ramp = tx_pa_val & 0x1;

		ovr_vals = (pa_ramp << 8) | (pa_ctrl << 5);
		ovr_vals |= (pa_powerdet << 4) | (pa_pd << 3);
		ovr_vals |= (pa_pddet << 2);

		/* enable output of pa_powerdet, pa_pddet, pa_pd */
		if (AREV_GE(pi->pubpi.phy_rev, 5))
			out_en = 0x2000;
		else
			out_en = 0xe000;

		phy_reg_mod(pi, APHY_RF_OVERRIDE_VAL, 0x1fc, ovr_vals);
		/* enable overrides on bits 8-2 */
		phy_reg_or(pi, APHY_RF_OVERRIDE, 0x1fc | out_en);

		/* 2060_tx_on: */

		pwr_dynctl = read_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL) & 0x1f;

		/* get 2060 info used for tx */
		tx_on = read_aphy_table_ent(pi, 0xe, 0xc);
		write_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, pwr_dynctl | (tx_on << 5));
	} else if (ISGPHY(pi)) {
		pi_abg->freq_anacore = R_REG(pi->sh->osh, &regs->phyanacore);
		pi_abg->freq_test = R_REG(pi->sh->osh, &regs->phytest);

		/* Turn off all CRS */
		if (pi_abg->sbtml_gm)
			aphy_all_crs(pi, FALSE);

		/* set mux to test output (DDFS). */
		OR_REG(pi->sh->osh, &regs->phytest, 0x2000);

		/* configure DDFS to send out DC on I and Q outputs */
		pi_abg->freq_rf_override = phy_reg_read(pi, BPHY_RF_OVERRIDE);
		pi_abg->freq_freq_ctrl = phy_reg_read(pi, BPHY_FREQ_CONTROL);
		pi_abg->freq_phase_scale = phy_reg_read(pi, BPHY_PHASE_SCALE);
		pi_abg->freq_ddfs_enable = phy_reg_read(pi, BPHY_DDFS_ENABLE);

		PHY_REG_LIST_START
			/* gain defaults: amp 5, dac 2. force tx_pu and pa_ramp high */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_RF_OVERRIDE, 0xb320)

			PHY_REG_WRITE_RAW_ENTRY(BPHY_FREQ_CONTROL, 0)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_PHASE_SCALE, 0x810)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_DDFS_ENABLE, 0x0d)
		PHY_REG_LIST_EXECUTE(pi);

		if (pi->pubpi.ana_rev > 1) {
			phy_reg_mod(pi, BPHY_ANACORE, 0x60, 0x40);
			wlc_phy_do_dummy_tx(pi, FALSE, ON);
		}

	}
}

int
wlc_phy_aphy_long_train(phy_info_t *pi, int channel)
{
	d11regs_t *regs = pi->regs;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;
	uint16 play_depth, play_wait, pwr_dynctl, tx_on;
	int ctr;

	if (!ISAPHY(pi)) {
		PHY_ERROR(("wl%d: %s: Must be APHY\n",
			pi->sh->unit, __FUNCTION__));
		return (1);
	}

	/* stop any test in progress */
	wlc_phy_test_stop(pi);

	/* channel 0 means restore original contents and end the test */
	if (channel == 0) {
		W_REG(pi->sh->osh, &regs->phytest, pi_abg->long_train_phytest);
		pi_abg->long_train_phytest = 0;
		phy_reg_write(pi, APHY_CTL, 0);
		wlc_pa_on(pi, FALSE);

		/* 2060_tx_on(0) */
		write_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, pi_abg->ltrn_reg04);

		/* tr_switch */;
		phy_reg_write(pi, APHY_RF_OVERRIDE, pi_abg->ltrn_phyreg10);
		phy_reg_write(pi, APHY_RF_OVERRIDE_VAL, pi_abg->ltrn_phyreg11);

		phy_reg_write(pi, APHY_CRSMF_THRESH1, pi_abg->ltrn_phyregc3);
		phy_reg_write(pi, APHY_CTHR_STHR_SHDIN, pi_abg->ltrn_phyreg29);


		return 0;
	}

	if (wlc_phy_test_init(pi, channel, TRUE)) {
		return 1;
	}

	if (pi_abg->long_train_phytest == 0)
		pi_abg->long_train_phytest = R_REG(pi->sh->osh, &regs->phytest);

	/*
	 * start transmitting the short preamble sequence
	 */

	/* store the registers that are being messed up */
	pi_abg->ltrn_phyreg29 = phy_reg_read(pi, APHY_CTHR_STHR_SHDIN);
	pi_abg->ltrn_phyregc3 = phy_reg_read(pi, APHY_CRSMF_THRESH1);
	pi_abg->ltrn_reg04 = read_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL);
	pi_abg->ltrn_phyreg10 = phy_reg_read(pi, APHY_RF_OVERRIDE);
	pi_abg->ltrn_phyreg11 = phy_reg_read(pi, APHY_RF_OVERRIDE_VAL);

	/* disable a-phy carrier sense state machine */
	aphy_crs0(pi, FALSE);

	/* aphy_crs_mf: */
	if (AREV_GT(pi->pubpi.phy_rev, 2))
		phy_reg_and(pi, APHY_CRSMF_THRESH1, 0x7fff);
	phy_reg_or(pi, APHY_CTHR_STHR_SHDIN, 0x8000),

	/* load the play buffer */
	play_depth = PHY_LTRN_LIST_LEN;
	for (ctr = 0; ctr < play_depth; ctr++) {
		WRITE_APHY_TABLE_ENT(pi, 0xd, ctr, ltrn_list[ctr]);
	}

	/* start the loopback mode */
	phy_reg_write(pi, 0x70, 0xfff);
	play_depth--;
	play_wait = 10;
	phy_reg_write(pi, 0x71, (play_wait<<6) | play_depth);
	PHY_REG_LIST_START
		PHY_REG_WRITE_RAW_ENTRY(APHY_STOP_PKT_CNT, 0x10)
		PHY_REG_OR_RAW_ENTRY(APHY_CTL, 0x1)
		PHY_REG_WRITE_RAW_ENTRY(APHY_PASS_TH_SAMPS, 0x100)
		PHY_REG_WRITE_RAW_ENTRY(APHY_CTL, 0x3)
	PHY_REG_LIST_EXECUTE(pi);
	wlc_pa_on(pi, TRUE);

	/* 2060_tx_on: */
	tx_on = read_aphy_table_ent(pi, 0xe, 0xc);
	pwr_dynctl = read_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL) & 0x1f;
	write_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, pwr_dynctl | (tx_on << 5));

	/* tr_switch t; */
	PHY_REG_LIST_START
		PHY_REG_OR_RAW_ENTRY(APHY_RF_OVERRIDE, 0xc00)
		PHY_REG_MOD_RAW_ENTRY(APHY_RF_OVERRIDE_VAL, 0xc00, 0x400)
	PHY_REG_LIST_EXECUTE(pi);

	return 0;
}

void
wlc_get_11b_txpower(phy_info_t *pi, atten_t *atten)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	if (pi->hwpwrctrl) {
#if defined(BCMDBG) || defined(WLTEST)
		if (pi->radiopwr_override == RADIOPWR_OVERRIDE_DEF)
			return;
#else
		return;
#endif /* BCMDBG || WLTEST */
	}

	atten->bb = pi_abg->bb_atten;
	atten->radio = pi_abg->radiopwr;
	atten->auto_ctrl = (pi->radiopwr_override == RADIOPWR_OVERRIDE_DEF);
	atten->txctl1 = pi_abg->txctl1;
}

void
wlc_phy_set_11b_txpower(phy_info_t *pi, atten_t *atten)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;
	if (pi->hwpwrctrl) {
#if defined(BCMDBG) || defined(WLTEST)
#if GCONF
		bool pcl_enable = (atten->auto_ctrl == WL_ATTEN_PCL_ON) ? TRUE : FALSE;
		wlc_phy_txpwrctrl_hw_gphy(pi, pcl_enable); /* configure hw power control loop */
#endif /* GCONF */
#else
		return;
#endif /* BCMDBG  || WLTEST */
	}

	if (atten->auto_ctrl == WL_ATTEN_PCL_ON) {
		pi->radiopwr_override = RADIOPWR_OVERRIDE_DEF;
		return;
	}
	else if (atten->auto_ctrl == WL_ATTEN_PCL_OFF) {
		pi->radiopwr_override = pi_abg->radiopwr;
		return;
	}

	ASSERT(atten->auto_ctrl == WL_ATTEN_APP_INPUT_PCL_OFF);

	/* TODO:
	 * Put comment here to explain why TRUE is not assigned here.
	 * What is the reason for carrying around atten->radio ?
	 */
	pi->radiopwr_override = atten->radio;

	/* PCL is off at this point. If input level is specified, applied it now. */
	if (atten->auto_ctrl == WL_ATTEN_APP_INPUT_PCL_OFF) {
		wlc_set_11b_txpower(pi, atten->bb, atten->radio, atten->txctl1);
	}
}
#endif 

static void
wlc_set_11b_bphy_dac_ctrl_set(phy_info_t *pi, uint16 dacatten)
{
	uint16	mask = (pi->pubpi.ana_rev == ANA_11G_018) ?
	        BBATT_11G_MASK : BBATT_11G_MASK2;
	int	shift = (pi->pubpi.ana_rev == ANA_11G_018) ?
	        BBATT_11G_SHIFT : BBATT_11G_SHIFT2;

	phy_reg_mod(pi, BPHY_DAC_CONTROL, mask, dacatten << shift);
}

static void
wlc_set_11b_txpower(phy_info_t *pi, uint16 bbatten, uint16 radiopwr, uint16 txctl1)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	if (bbatten == 0xffff)
		bbatten = pi_abg->bb_atten;

	wlc_set_11b_bphy_dac_ctrl_set(pi, bbatten);
	pi_abg->bb_atten = bbatten;

	if (radiopwr == 0xffff)
		radiopwr = pi_abg->radiopwr;
	if (txctl1 == 0xffff)
		txctl1 = pi_abg->txctl1;

	wlapi_bmac_write_shm(pi->sh->physhim, M_RADIO_PWR, radiopwr);
	pi_abg->radiopwr = radiopwr;

	if (pi->pubpi.radiorev == 8) {
		uint16 txctl1_rpwr;
		txctl1_rpwr = (txctl1 & 0x70) | (radiopwr & 0xf);
		write_radio_reg(pi, RADIO_2050_PWR_CTL, txctl1_rpwr);
	} else {
		mod_radio_reg(pi, RADIO_2050_PWR_CTL, 0x0f, radiopwr);
		mod_radio_reg(pi, RADIO_2050_TX_CTL1,
		              TXC1_PA_GAIN_MASK | TXC1_TX_MIX_GAIN, txctl1);
	}

	pi_abg->txctl1 = txctl1;

	/* Use the saved values for LO leakage trim */

	if (pi_abg->txmag_enable)
		write_radio_reg(pi, RADIO_2050_TX_CTL1, pi_abg->mintxmag << 4 | pi_abg->mintxbias);
	else
		mod_radio_reg(pi, RADIO_2050_TX_CTL1, (TXC1_OFF_I_MASK | TXC1_OFF_Q_MASK),
		              pi_abg->mintxbias);

	PHY_CAL(("wl%d: %s: Restoring previous mintxbias %d, radio 0x52 = 0x%x\n",
	         pi->sh->unit, __FUNCTION__, pi_abg->mintxbias,
	         read_radio_reg(pi, RADIO_2050_TX_CTL1)));

	if (ISGPHY(pi))
		gphy_complo(pi, radiopwr, bbatten, txctl1);

	PHY_TMP(("wl%d: %s: Set 11b power bb/radio/ctl1 = %d/%d/%d\n", pi->sh->unit,
	         __FUNCTION__, bbatten, radiopwr, txctl1));
}

static void
wlc_gphy_gain_lut(phy_info_t *pi)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;
	int rf_index, bb_index, cnt;
	uint16 val, rf, bb, padmix;

	if (pi->pubpi.radiorev == 8)
		padmix = 0x4;
	else
		padmix = 0x5;

	val = 0;
	cnt = 0;
	for (rf_index = 0; rf_index < pi_abg->rf_list_size; rf_index++) {
		rf = PHY_GET_RFATTN(pi_abg->rf_attn_list[rf_index]);
		for (bb_index = 0; bb_index < pi_abg->bb_list_size; bb_index++) {
			bb = pi_abg->bb_attn_list[bb_index];
			if (cnt < TX_GAIN_TABLE_LENGTH) {
				val = (bb << 8) | (padmix << 4) | rf;
				phy_reg_write(pi, BPHY_TXGAIN_LUT + cnt, val);
				pi_abg->gain_table[cnt] = val;
			}
			cnt++;
		}
	}
	phy_reg_write(pi, BPHY_TXGAIN_LUT + TX_GAIN_TABLE_LENGTH - 1, val);
	pi_abg->gain_table[TX_GAIN_TABLE_LENGTH - 1] = val;
}


/* input: index from ucode.  Output: rf, bb, padmix_en */
static void
wlc_ucode_to_bb_rf(phy_info_t *pi, int indx, uint16 *bb, uint16 *rf, uint16 *padmix_en)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;
	int rf_index, bb_index;

	ASSERT(indx < TX_GAIN_TABLE_LENGTH);
	/* MXXX Has to be changed in tcl so that value(63)=value(62) */
	/* We can now accept index 0? */
	if (indx) {
		rf_index = indx / pi_abg->rf_list_size;
		bb_index = indx % pi_abg->rf_list_size;

		*bb = pi_abg->bb_attn_list[bb_index];
		*rf = PHY_GET_RFATTN(pi_abg->rf_attn_list[rf_index]);
		*padmix_en = PHY_GET_PADMIX(pi_abg->rf_attn_list[rf_index]);
	}
}

/* Download BB LO compensation DC bias results to phy registers
 *   (will saturate the DC offsets to 4-bits: [-8 to +7])
 */
static void
wlc_gphy_dc_lut(phy_info_t *pi)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	int rf_index, bb_index, cnt;
	int rf_attn, bb_attn;
	int rfgainid, bbattnid;
	uint16 padmix_en;
	abgphy_lo_complex_info_t min_pos1, min_pos2;
	uint16 minset;

	rf_attn = 0;
	bb_attn = 0;
	padmix_en = 0;

	/* This loop assumes that rf_len * bb_len is not even,
	 * but each iteration we write a pair of values((i,q),(i,q)) into the phy table,
	 * so the very first value is hardwired
	 * and each time around the loop we calculate n-1 and n,
	 * so at the end we have an even number of values to write.
	 */
	ASSERT(((pi_abg->rf_list_size * pi_abg->bb_list_size) & 1) == 1);
	for (cnt = 0; cnt < (pi_abg->rf_list_size * pi_abg->bb_list_size); cnt += 2) {
		rf_index  = (cnt) / pi_abg->bb_list_size;
		bb_index  = (cnt) % pi_abg->bb_list_size;
		rf_attn   = PHY_GET_RFATTN(pi_abg->rf_attn_list[rf_index]);
		padmix_en = PHY_GET_PADMIX(pi_abg->rf_attn_list[rf_index]);
		bb_attn   = pi_abg->bb_attn_list[bb_index];

		gphy_complo_map_txpower(pi, rf_attn, bb_attn,
			padmix_en * pi_abg->padmix_mask, &rfgainid, &bbattnid);
		min_pos1 = pi_abg->gphy_locomp_iq[rfgainid][bbattnid];

		if ((cnt + 1) < (pi_abg->rf_list_size * pi_abg->bb_list_size)) {
			rf_index  = (cnt + 1) / pi_abg->bb_list_size;
			bb_index  = (cnt + 1) % pi_abg->bb_list_size;
			rf_attn   = PHY_GET_RFATTN(pi_abg->rf_attn_list[rf_index]);
			padmix_en = PHY_GET_PADMIX(pi_abg->rf_attn_list[rf_index]);
			bb_attn   = pi_abg->bb_attn_list[bb_index];
			gphy_complo_map_txpower(pi, rf_attn, bb_attn,
				padmix_en * pi_abg->padmix_mask,
				&rfgainid, &bbattnid);
			min_pos2 = pi_abg->gphy_locomp_iq[rfgainid][bbattnid];
		} else {
			min_pos2 = min_pos1;
		}

		minset = (((min_pos2.i & 0xf) << 12) | ((min_pos2.q & 0xf) << 8) |
		          ((min_pos1.i & 0xf) <<  4) |  (min_pos1.q & 0xf));
		phy_reg_write(pi, BPHY_LOCOMP_LUT + (cnt / 2), minset);
	}
}

static int8 tssi_to_dbm[PHY_TSSI_TABLE_SIZE];

static void
wlc_gphy_tssi_power_lut(phy_info_t *pi)
{
	int idx;
	uint16 value;

	/* (OFDM) write to aphy table 0x15, which is 8 bit wide and 64 deep */
	for (idx = 0; idx < TX_GAIN_TABLE_LENGTH; idx++) {
		if (idx < 32)
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x15), idx + 32,
			                     tssi_to_dbm[idx]);
		else
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x15), idx - 32,
			                     tssi_to_dbm[idx]);
	}

	/*  (DSSS) write to table BPHY_TSSI2PWR_LUT. 16 bit wide and 32 deep */
	for (idx = 0; idx < TX_GAIN_TABLE_LENGTH - 1; idx += 2) {
		value = ((tssi_to_dbm[idx + 1] << 8) & 0xff00) | (tssi_to_dbm[idx] & 0xff);
		phy_reg_write(pi, BPHY_TSSI2PWR_LUT + idx/2, value);
	}
}

/* see comments for wlc_a_hw_pwr_ctrl() for more info */
#if GCONF
static void
wlc_phy_txpwrctrl_hw_gphy(phy_info_t *pi, bool enable)
{
	uint16 g_dac_atten_idx;
	uint16 dacatten;

	PHY_INFORM(("wl%d:wlc_g_hw_pwr_ctrl(enable 0x%x)\n", pi->sh->unit, enable));

	if (enable == TRUE) {
		PHY_REG_LIST_START
			PHY_REG_AND_RAW_ENTRY(BPHY_DAC_CONTROL, ~(1 << 6))
			PHY_REG_AND_RAW_ENTRY(BPHY_TX_POWER_OVERRIDE, ~(1 << 9))
			/* do not modify bit 11 */
			PHY_REG_AND_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1), ~(1 << 8))
			PHY_REG_AND_RAW_ENTRY(GPHY_CTRL, ~(1 << 6))
		PHY_REG_LIST_EXECUTE(pi);
	}
	else {
		PHY_REG_LIST_START
			PHY_REG_OR_RAW_ENTRY(BPHY_DAC_CONTROL, (1 << 6))
			PHY_REG_OR_RAW_ENTRY(BPHY_TX_POWER_OVERRIDE, (1 << 9))
			/* do not modify bit 11 */
			PHY_REG_OR_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1), (1 << 8))
		PHY_REG_LIST_EXECUTE(pi);

		/* get power index for the current DAC override gain, and apply */
		g_dac_atten_idx = phy_reg_read(pi, BPHY_TX_PWR_BASE_IDX);
		if (g_dac_atten_idx >= TX_GAIN_TABLE_LENGTH) {
			PHY_ERROR(("wl%d: %s: power index 0x%x >= TX_GAIN_TABLE_LENGTH\n",
			          pi->sh->unit, __FUNCTION__, g_dac_atten_idx));
			return;
		}

		dacatten = (pi->u.pi_abgphy->gain_table[g_dac_atten_idx] >> 8) & 0xff;
		wlc_set_11b_bphy_dac_ctrl_set(pi, dacatten);
	}
}
#endif /* GCONF */

#if ACONF
/*
 * two hardware parts are involved in tx power control; one in baseband, the other
 * in analog chip. The tx power control loop controls these two parts.
 * In a system that has pi->hwpwrctrl set to TRUE, ucode runs tx power control
 * loop to maintain the actual output power at the set target power; in other systems,
 * driver runs the loop.
 * This routine provides a way to configure both baseband and analog chip to (1) ignore this
 * power control directives from tx power control loop and (2) maintain the set target power.
 */
static void
wlc_phy_txpwrctrl_hw_aphy(phy_info_t *pi, bool enable)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(pi->hwpwrctrl == TRUE);

	wlc_aphypwr_one(pi, enable);
	wlc_aphypwr_nine(pi, enable);
}
#endif /* ACONF */

static void
WLBANDINITFN(wlc_a_hw_pwr_ctrl_init)(phy_info_t *pi)
{
	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));
	if (!pi->hwpwrctrl)
		return;

	wlc_aphypwr_one(pi, TRUE);
	wlc_aphypwr_set_tssipwr_LUT(pi, CHSPEC_CHANNEL(pi->radio_chanspec));
	wlc_aphypwr_three(pi);
	wlc_aphypwr_four(pi);
	wlc_aphypwr_five(pi);
	wlc_aphypwr_six(pi);
	wlc_aphypwr_seven(pi);
	wlc_aphypwr_nine(pi, TRUE);
}

static void
WLBANDINITFN(wlc_g_hw_pwr_ctrl_init)(phy_info_t *pi)
{

	if (!pi->hwpwrctrl)
		return;

	/* CCK */
	phy_reg_mod(pi, BPHY_TX_PWR_CTRL, 0x3f,
	            pi->u.pi_abgphy->target_idle_tssi - pi->u.pi_abgphy->idle_tssi[G_ALL_CHANS]);

	/* OFDM */
	phy_reg_mod(pi, GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1, 0xff,
	            pi->u.pi_abgphy->target_idle_tssi - pi->u.pi_abgphy->idle_tssi[G_ALL_CHANS]);

	wlc_gphy_tssi_power_lut(pi);
	wlc_gphy_gain_lut(pi);

	PHY_REG_LIST_START_WLBANDINITDATA
		/*  dacCtrlOvr -- Reset dac_control override (bit 6)  */
		PHY_REG_AND_RAW_ENTRY(BPHY_DAC_CONTROL, ~(1 << 6))
		/*  Enable 4wire */
		PHY_REG_WRITE_RAW_ENTRY(BPHY_TX_POWER_OVERRIDE, 0x0)
		/* txRotorScaleOv : override the rotor scale factor */
		PHY_REG_OR_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1), 1 << 11)
		/* txDigScalerOvr : digital scalar is driven by rate based gain offset */
		PHY_REG_AND_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A1), ~(1 << 8))
		/* Disable the override on the gphy scalar
		* This is controlled by a LUT.
		*/
		PHY_REG_AND_RAW_ENTRY(GPHY_CTRL, ~(1 << 6))
	PHY_REG_LIST_EXECUTE(pi);

	wlc_gphy_dc_lut(pi);
}

static void
WLBANDINITFN(wlc_bgphy_pwrctrl_init)(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;
	uint16	radiopwr = 0, bb_atten = 0, txctl1 = 0;

	if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
	    (pi->sh->boardtype == BU4306_BOARD)) {
		/* Bringup boards do not have a power detector */
		return;
	}

	/* Initialize for pwr ctrl */
	phy_reg_write(pi, BPHY_TSSI_CTL, 0x8018);
	AND_REG(pi->sh->osh, &regs->phyanacore, 0xffdf);


	if (ISGPHY(pi)) {
		/* If we are doing the gphy-in-gmode0, just return */
		if (!pi_abg->sbtml_gm)
			return;

		if (pi->hwpwrctrl) {
			PHY_REG_LIST_START_WLBANDINITDATA
				/* in g-mode, b-phy's raw tssi is in 2's complement format */
				PHY_REG_AND_RAW_ENTRY(BPHY_TX_PWR_CTRL, 0xfeff)
				/* Set the bphy DC offset to a known value */
				PHY_REG_WRITE_RAW_ENTRY(BPHY_TX_DC_OFF2, 0x0202)
				/* enable TSSI measurement in a-phy */
				PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_TSSI_TEMP_CTL, 0x2)
				/* tssi measurement start delay is 7.5us ( 0xf / 2 us or )
				* delay between each of the 4-samples is 0us ( 0x0 / 2 us )
				*/
				PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_RSSI_ADC_CTL, 0xf000)
			PHY_REG_LIST_EXECUTE(pi);

			if (pi->pubpi.radiorev == 8) {
				/* 4318b0 */
				PHY_REG_LIST_START_WLBANDINITDATA
					PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_RSSI_ADC_CTL,
						0x00f0, 0x0010)
					/* set 4-wire to 44MHz (bit 15) */
					PHY_REG_OR_RAW_ENTRY(BPHY_OPTIONAL_MODES, 0x8000)
					/* txPwrCtrlBaseIndex- set the base index to 36 */
					PHY_REG_MOD_RAW_ENTRY(BPHY_TX_PWR_BASE_IDX, 0x3f, 0x24)
					/* now initial value for bphy scalar is set to 0 dB */
					PHY_REG_WRITE_RAW_ENTRY(BPHY_TX_DC_OFF1, 0xc07f)
					/* txPwrCtrlControlAddress : DAC and LOFT are set at end
					* of previous packet, incr. dig at beg. of packet
					*/
					PHY_REG_OR_RAW_ENTRY(BPHY_TX_PWR_CTRL, 0x400)
				PHY_REG_LIST_EXECUTE(pi);
			} else {
				PHY_REG_LIST_START_WLBANDINITDATA
					/* 2050 gain bits are split across 0x43 and 0x52 */
					PHY_REG_OR_RAW_ENTRY(BPHY_TX_PWR_CTRL, 0x200)
					/* txPwrCtrlControlAddress : DAC and LOFT are set at end
					* of previous packet, incr. dig at beg. of packet
					*/
					PHY_REG_OR_RAW_ENTRY(BPHY_TX_PWR_CTRL, 0x400)
					/* regs2fourwire.fastClkSel : set 4-wire to
					 * 22MHz (bit 15)
					 */
					PHY_REG_AND_RAW_ENTRY(BPHY_OPTIONAL_MODES, 0x7fff)
					/* regs2fourwire.slowRdClk : slow wire read is
					 * also at 22 MHz
					 */
					PHY_REG_AND_RAW_ENTRY(BPHY_OPTIONAL_MODES2, 0xfffe)
					/* txPwrCtrlBaseIndex- set the base index to 16 */
					PHY_REG_MOD_RAW_ENTRY(BPHY_TX_PWR_BASE_IDX, 0x3f, 0x10)
					/* now initial value for bphy scalar is set to 0 dB */
					PHY_REG_WRITE_RAW_ENTRY(BPHY_TX_DC_OFF1, 0xc07f)
					PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_RSSI_ADC_CTL,
						0x00f0, 0x0010)
				PHY_REG_LIST_EXECUTE(pi);
			}
		} else {
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_RSSI_ADC_CTL), 0xc111);
		}
	}

	/* Get the idle tssi if we haven't */
	if (pi->u.pi_abgphy->idle_tssi[G_ALL_CHANS] == 0) {
		uint16 max_rf, max_bb;

		/* Save current settings */
		radiopwr = pi_abg->radiopwr;
		bb_atten = pi_abg->bb_atten;
		txctl1 = pi_abg->txctl1;

		/* Set attenuation to maximum values */

		/* Max rf_atten based on radiorev */
		if (pi->pubpi.radiorev == 8)
			max_rf = MAX_TX_ATTN_4318;
		else
			max_rf = MAX_TX_ATTN_2050;

		/* Max baseband atten is based on analog core rev */
		max_bb = MAX_TX_ATTN;

		wlc_set_11b_txpower(pi, max_bb, max_rf, 0);

		/* send a cck packet */
		wlc_phy_do_dummy_tx(pi, FALSE, ON);

		/* Read idle tssi */
		pi->u.pi_abgphy->idle_tssi[G_ALL_CHANS] = (int8)phy_reg_read(pi, BPHY_TSSI);


		/* Restore original values */

		wlc_set_11b_txpower(pi, bb_atten, radiopwr, txctl1);

		PHY_TXPWR(("wl%d: %s: idle_tssi = 0x%x, bb/radio/ctl1 = %d/%d/%d\n",
		        pi->sh->unit, __FUNCTION__,
		        pi->u.pi_abgphy->idle_tssi[G_ALL_CHANS],
			pi_abg->bb_atten,
			pi_abg->radiopwr,
		        pi_abg->txctl1));
	}

	wlc_g_hw_pwr_ctrl_init(pi);

	/* Initialize the shared memory */
	wlc_phy_clear_tssi((wlc_phy_t*)pi);
}

static void
WLBANDINITFN(wlc_aphy_pwrctrl_init)(phy_info_t *pi)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	phy_reg_write(pi, APHY_RSSI_ADC_CTL, 0xf111);

	if (pi_abg->idle_tssi[A_MID_CHANS] == 0) {
		uint16 val, band, bb = 0x20;
		/* representative channels from lo/mid/hi */
		uint a_chans[CH_5G_GROUP] = {200, 64, 157};
		uint orig_channel = CHSPEC_CHANNEL(pi->radio_chanspec);

		val = read_aphy_table_ent(pi, 0xc, 1);
		if (pi->pubpi.ana_rev == ANA_11G_018)
			val = (val &  ~BBATT_11G_MASK) |
			        (uint16)(11 << BBATT_11G_SHIFT);
		else
			val = (val &  ~BBATT_11G_MASK2) |
			        (uint16)(11 << BBATT_11G_SHIFT2);
		WRITE_APHY_TABLE_ENT(pi, 0xc, 1, val);

		/* Compute idle_tssi for each channel band */
		for (band = 0; band < ARRAYSIZE(a_chans); band++) {
			wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(a_chans[band]));

			/* Set max attenuation.  Need to reset since set_channel sets power */
			write_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, 0);
			write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, bb);

			wlc_phy_do_dummy_tx(pi, TRUE, ON);
			pi_abg->idle_tssi[band] = phy_reg_read(pi, APHY_TSSI_STAT) & 0xff;
			PHY_TXPWR(("wl%d: %s: Read idle_tssi Aphy chan %d: 0x%x\n", pi->sh->unit,
			          __FUNCTION__, a_chans[band], pi_abg->idle_tssi[band]));
		}
		/* Restore channel */
		wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(orig_channel));
		WRITE_APHY_TABLE_ENT(pi, 0xc, 1, val); /* Reset the default */

		if (pi->hwpwrctrl) {
			PHY_TXPWR(("Recalculating tables now for APHY\n"));
			/* If using power control we need to re-generate accurate
			 * tssi LUTs now that idle_tssi has been calculated.
			 */
			ASSERT(pi->sh->sromrev >= 2);

			/* setup mid channel  tssi mapping tables */
			wlc_phy_gen_tssi_tables(pi, 'm', pi->txpa_5g_mid, pi_abg->target_idle_tssi,
			                             pi->tx_srom_max_5g_mid);

			/* setup the low channels tssi mapping tables */
			wlc_phy_gen_tssi_tables(pi, 'l', pi->txpa_5g_low, pi_abg->target_idle_tssi,
			                             pi->tx_srom_max_5g_low);

			/* setup the high channels tssi mapping tables */
			wlc_phy_gen_tssi_tables(pi, 'h', pi->txpa_5g_hi, pi_abg->target_idle_tssi,
			                             pi->tx_srom_max_5g_hi);
		} else {
			/* Set default power level */
			wlc_set_11a_txpower(pi, DEFAULT_11A_TXP_IDX, FALSE);
		}
	}

	/* Init A band/2060 hardware power control */
	if (pi->hwpwrctrl)
		wlc_a_hw_pwr_ctrl_init(pi);

	/* clear the tssi values in shared memory */
	wlc_phy_clear_tssi((wlc_phy_t*)pi);
}

/* bphy rev >= 6 (also in gphy rev >= 2y) */
static void
WLBANDINITFN(wlc_bphy6_init)(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	uint16	addr, val;
	int channel;
	uint tmp_chan;
	int radiorev = pi->pubpi.radiorev;


	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Enable the PHY-based dc-canceller 4-w fix
	 * the uc WAR is not enabled for rev > 1.
	 */
	phy_reg_write(pi, BPHY_RFDC_CANCEL_CTL, 0x817a);

	/* Set mode_cm, mode_rssi, and lna_load bits in RX_CTL0 register.
	 * lna_load sets the LNA=00 gain to -2 dB when set or -10 dB when
	 * cleared (nominally).
	 */
	or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x58);

	if (radiorev == 8) {
		write_radio_reg(pi, RADIO_2050_TX_CTL0, 0);
		write_radio_reg(pi, RADIO_2050_TX_CTL1, 0x40);
		write_radio_reg(pi, RADIO_2050SC_TX_CTL2, 0xb7);
		write_radio_reg(pi, RADIO_2050SC_TX_CTL3, 0x98);
		write_radio_reg(pi, RADIO_2050_PLL_CTL0, 0x88);
		write_radio_reg(pi, RADIO_2050_PLL_CTL1, 0x6b);
		write_radio_reg(pi, RADIO_2050_PLL_CTL2, 0xf);
		if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_ALTIQ) {
			write_radio_reg(pi, RADIO_2050_PLL_CTL3, 0xfa);
			write_radio_reg(pi, RADIO_2050SC_PLL_CTL4, 0xd8);
		} else {
			write_radio_reg(pi, RADIO_2050_PLL_CTL3, 0xf5);
			write_radio_reg(pi, RADIO_2050SC_PLL_CTL4, 0xb8);
		}
		write_radio_reg(pi, RADIO_2050SC_CRY_TRIM0, 0x3);
		write_radio_reg(pi, RADIO_2050SC_RX_CTL3, 0xa8);
		write_radio_reg(pi, RADIO_2050SC_RX_CTL2, 1);
		write_radio_reg(pi, RADIO_2050SC_LNA_BIAS, 0x8);
	}

	/* RSSI LUT is now a memory and must therefore be initialized */
	val = 0x1e1f;
	for (addr = BPHY_RSSI_LUT; addr <= BPHY_RSSI_LUT_END; addr++) {
		phy_reg_write(pi, addr, val);
		if (addr == 0x97)
			val = 0x3e3f;
		else
			val -= 0x0202;
	}

	/* TSSI LUT is now a memory and must therefore be initialized.
	 * with an ana_018g, the RSSI ADC is necessarily at 40 MHz,
	 * with a GPHY LUT mapped to 2s-comp, and a digital LPF; remap
	 * TSSI to be unsigned, and "upside-down";
	 */
	val = 0x2120;
	for (addr = BPHY_TSSI_LUT; addr <= BPHY_TSSI_LUT_END; addr++) {
		phy_reg_write(pi, addr, (val & 0x3f3f));
		val += 0x0202;
	}

	if (ISGPHY(pi)) {
		ASSERT(phy_reg_read(pi, BPHY_OPTIONAL_MODES) & OPT_MODE_G);

		/* Mode configuration for 802.11g vs. 802.11b */
		/* Set mode_off bit in RX_CTL0 register to enable
		 * new DC offset cancellation controls for .11g.
		 */
		or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x20);

		/* Set mode_txlpf bit in TX_CTL0 register to enable
		 * 15.6 MHz nominal fc TX LPF for .11g.
		 */
		or_radio_reg(pi, RADIO_2050_TX_CTL0, 0x04);

		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_OR_RAW_ENTRY(GPHY_CLASSIFY_CTRL, (1 << 8))
			PHY_REG_OR_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_ANT_DWELL), (1 << 13))
			/* fix the LNA gain tables */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_LNA_GAIN_RANGE_10, 0x0000)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_LNA_GAIN_RANGE_32, 0x0000)
		PHY_REG_LIST_EXECUTE(pi);
	}

	if (NORADIO_ENAB(pi->pubpi)) {
		/* only for use on QT */

		/* CRS thresholds */
		W_REG(pi->sh->osh, &regs->phycrsth, 0x3206);

		PHY_REG_LIST_START_WLBANDINITDATA
			/* RSSI thresholds */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_RSSI_TRESH, 0x281e)
			/* LNA gain range */
			PHY_REG_OR_RAW_ENTRY(BPHY_LNA_GAIN_RANGE, 0x1a)
		PHY_REG_LIST_EXECUTE(pi);

	} else	{
		channel = CHSPEC_CHANNEL(pi->radio_chanspec);
		tmp_chan = (channel > 7) ? 1 : 13;
		wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(tmp_chan));

		/* toggle rccal_en and bgcal_en from 0 to 1 to start RC and BG calibration manually
		 */
		write_radio_reg(pi, RADIO_2050_CAL_CTL, 0x20);
		write_radio_reg(pi, RADIO_2050_CAL_CTL, 0x23);

		/* wait for calibration to complete */
		OSL_DELAY(40);
		/* stop the calibration clock */
		if (radiorev == 8) {
			/* kimmer - this turns of the clk use for RC_cal */
			or_radio_reg(pi, RADIO_2050SC_RX_CTL2, 2);

			/* set calibration enables back to 0 */
			write_radio_reg(pi, RADIO_2050_CAL_CTL, 0x20);
		}

		if ((radiorev == 1) || (radiorev == 2)) {
			/* Stop RC calibration clock */
			write_radio_reg(pi, RADIO_2050_CAL_CTL, 0x20);

			/* phase noise improvements
			 * set prescaler bias current control to 7 (out of 15)
			 */
			write_radio_reg(pi, RADIO_2050_PLL_CTL0, 0x70);

			/* set charge pump current bias control to 7 (out of 15)
			 * and PLL PFD current bias control to 13 (out of 15)
			 */
			write_radio_reg(pi, RADIO_2050_PLL_CTL1, APHY_TSSI_STAT);

			/* set VCO current bias control to 13 (out of 15)
			 * This is about a 19% increase.
			 */
			write_radio_reg(pi, RADIO_2050_PLL_CTL2, 0xb0);
		}

		or_radio_reg(pi, RADIO_2050_RX_CTL0, 7);

		wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(channel));

		PHY_REG_LIST_START_WLBANDINITDATA
			/* Lock out automatic writing of tx power, must be done
			 * just before setting PWR_CTL (Change to 0x200).
			 */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_TX_POWER_OVERRIDE, 0x200)
			/* kimmer - add change from 0x88a3 to 88c2 */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_TR_LOSS_CTL, 0x88c2)
			/* kimmer - add change from 0x667 to x668 very slight improvement */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_STEP, 0x668)
		PHY_REG_LIST_EXECUTE(pi);

		/* Set TX power to saved values */
		wlc_set_11b_txpower(pi, 0xffff, 0xffff, 0xffff);

		if ((pi->pubpi.radiorev == 1) ||
		    (pi->pubpi.radiorev == 2)) {
			/* Increase LOGEN current in 2050's x2 to increase LO amplitude
			 * and reduce transmitter I-Q imbalance.
			 */
			write_radio_reg(pi, RADIO_2050_PLL_CTL3, 0xd);
		}

		/* Increase RSSI ADC I_ladder to 112.5% of default current.  This
		 * boosts the RSSI ADC full-scale range by 112.5%.
		 */
		if (pi->pubpi.ana_rev == ANA_11G_013)
			W_REG(pi->sh->osh, &regs->phyadcbias, 9);
		else
			phy_reg_mod(pi, BPHY_ADCBIAS, 0x3f, 0x04);

		/* for analog_013, Increase IQ ADC FS to 125% of default setting */
		if (pi->pubpi.ana_rev == ANA_11G_013) {
			/* ana11g_013 */
			phy_reg_mod(pi, APHY_CTL, 0xF000, 0x0);
		}

		if (ISGPHY(pi))
			W_REG(pi->sh->osh, &regs->phyanacore, 0);
		else
			ASSERT((const char*)"Phy_type != PHY_TYPE_G" == NULL);
	}
}

/* bphy rev 5 (aka gphy rev 1) */
static void
WLBANDINITFN(wlc_bphy5_init)(phy_info_t *pi)
{
	d11regs_t *regs = pi->regs;
	chanspec_t chspec;
	uint16	addr, val;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	ASSERT(pi->pubpi.radioid == BCM2050_ID);

	/* If we are using ana_018g... */
	if (pi->pubpi.ana_rev == ANA_11G_018) {
		/* We need to configure 2050 radio to accept common-mode input
		 * and to scale RSSI analog output for new AFE.
		 *
		 * Set mode_cm and mode_rssi bits in RX_CTL0 register.
		 */
		or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x50);
	}

	/* with an ana_018g, the RSSI ADC is necessarily at 40 MHz,
	 * with a GPHY LUT mapped to 2s-comp, and a digital LPF; remap
	 * TSSI to be unsigned, and "upside-down";
	 */
	/* HACK: This code is different than tcl, its effect is the same */
	val = 0x2120;
	for (addr = BPHY_TSSI_LUT; addr <= BPHY_TSSI_LUT_END; addr++) {
		phy_reg_write(pi, addr, (val & 0x3f3f));
		val += 0x0202;
	}

	PHY_REG_LIST_START_WLBANDINITDATA
		/* set angleStartPoint to 7 */
		PHY_REG_MOD_RAW_ENTRY(BPHY_SYNC_CTL, 0x0f00, 0x0700)
		/* Set LMS channel estimator step sizes to mu0 = 2^-6, mu1 = 2^-6,
		* mu2 = 2^-7 instead of the default mu0 = 2^-6, mu1 = 2^-8, and
		* mu2 = 2^- 9.  This increases the estimator misadjustment (bad) but
		* not by an amount that impacts 11 Mbps sensitivity by much.
		* The good effect is that badly-estimated carrier frequency offset is
		* tracked with the channel estimator; at the lowest rates (1 and 2
		* Mbps), this can vastly increase the sensitivity, as the initial
		* frequency offset estimate is made on a very low-SNR input, which
		* means the estimate is poor.  NOTE that
		* this is NOT the right way to solve the problem (we should increase
		* the PLL loop constants instead), but we're at the limit of what we
		* can do with the PLL loop constants.  Do this only if we're using
		* a BCM2050 as the radio.
		*/
		PHY_REG_WRITE_RAW_ENTRY(BPHY_STEP, 0x0667)
	PHY_REG_LIST_EXECUTE(pi);

	/* enable DC offset compensation */
	W_REG(pi->sh->osh, &regs->phytest, TST_DC_COMP_LOOP);

	if (pi_abg->sbtml_gm) {
		ASSERT(phy_reg_read(pi, BPHY_OPTIONAL_MODES) & OPT_MODE_G);

		/* Mode configuration for 802.11g vs. 802.11b */
		/* Set mode_off bit in RX_CTL0 register to enable
		 * new DC offset cancellation controls for .11g.
		 */
		or_radio_reg(pi, RADIO_2050_RX_CTL0, 0x20);

		/* Set mode_txlpf bit in TX_CTL0 register to enable
		 * 15.6 MHz nominal fc TX LPF for .11g.
		 */
		or_radio_reg(pi, RADIO_2050_TX_CTL0, 0x04);

		W_REG(pi->sh->osh, &regs->phybbconfig, 0);
		PHY_REG_LIST_START_WLBANDINITDATA
			PHY_REG_OR_RAW_ENTRY(GPHY_CLASSIFY_CTRL, (1 << 8))
			PHY_REG_OR_RAW_ENTRY((GPHY_TO_APHY_OFF + APHY_ANT_DWELL), (1 << 13))
			/* Refresh circuit, again only for gmode
			* Set the long timeout to a reasonable value (100ms)
			*/
			PHY_REG_WRITE_RAW_ENTRY(BPHY_REFRESH_TO1, 0x186a)
			PHY_REG_MOD_RAW_ENTRY(BPHY_RX_FLTR_TIME_UP, 0xff00, 0x1900)
			PHY_REG_MOD_RAW_ENTRY(BPHY_SYNC_CTL, 0x3f, 0x64)
			PHY_REG_MOD_RAW_ENTRY(BPHY_OPTIONAL_MODES, 0x7f, 0xa)
			/* fix the LNA gain tables */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_LNA_GAIN_RANGE_10, 0x0000)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_LNA_GAIN_RANGE_32, 0x0000)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
	}

	/* BMAC_NOTE: split driver will not support this 4306b0 Darwin Mode setting */
	if (pi_abg->bf_preempt_4306)
		phy_reg_or(pi, (GPHY_TO_APHY_OFF + APHY_BBCONFIG), BB_DARWIN);

	if (pi->pubpi.ana_rev == ANA_11G_018) {
		PHY_REG_LIST_START_WLBANDINITDATA
			/* RSSI curve is shifted too far left for LNA ON and new AFE */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_LNA_GAIN_RANGE, 0xce00)
			/* Also, it looks like the 2050 rxI and rxQ outputs are too big */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_IQ_TRESH_HH, 0x3763)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_IQ_TRESH_H,  0x1bc3)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_IQ_TRESH_L,  0x06f9)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_IQ_TRESH_LL, 0x037e)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		phy_reg_write(pi, BPHY_LNA_GAIN_RANGE, 0xcc00);
	}
	/* Look for 7 peaks to sync */
	phy_reg_write(pi, BPHY_PEAK_CNT_THRESH, 0xc6);

	/* jat1: crsthRegister energyOn = 0x3f, energyOff = 0x22 */
	W_REG(pi->sh->osh, &regs->phycrsth, 0x3f22);

	if (NORADIO_ENAB(pi->pubpi)) {
		/* only for use on QT */

		phy_reg_write(pi, BPHY_RSSI_TRESH, 0x281e);

	} else	{
		/* 2: adcRssiThreshold/LNAGainRange/PeakCntThresh registers.
		 *	Updated from rev1_phyreg_updates on 2/26
		 *	comment says that they are still under investigation
		 *	LNA "turn off" thd lowered from 0x33-- to 0x30-- 5/24/02 (fixes mp bd
		 *	failures)
		 */
		if (pi->pubpi.ana_rev == ANA_11G_018) {
			/* RSSI curve is shifted too far left for LNA ON */
			phy_reg_write(pi, BPHY_RSSI_TRESH, 0x3e1c);
		} else {
			phy_reg_write(pi, BPHY_RSSI_TRESH, 0x301c);
		}

		chspec = pi->radio_chanspec;
		wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(7));

		/* 8: set CAL_CTL */
		write_radio_reg(pi, RADIO_2050_CAL_CTL, 0x20);
		write_radio_reg(pi, RADIO_2050_CAL_CTL, 0x23);

		/* Stop RC calibration clock */
		write_radio_reg(pi, RADIO_2050_CAL_CTL, 0x20);

		/* phase noise improvements
		 * set prescaler bias current control to 7 (out of 15)
		 */
		write_radio_reg(pi, RADIO_2050_PLL_CTL0, 0x70);

		/* set charge pump current bias control to 7 (out of 15)
		 * and PLL PFD current bias control to 13 (out of 15)
		 */
		write_radio_reg(pi, RADIO_2050_PLL_CTL1, APHY_TSSI_STAT);

		/* set VCO current bias control to 13 (out of 15)
		 * This is about a 19% increase.
		 */
		write_radio_reg(pi, RADIO_2050_PLL_CTL2, 0xb0);

		or_radio_reg(pi, RADIO_2050_RX_CTL0, 7);

		wlc_phy_chanspec_set((wlc_phy_t*)pi, chspec);

		PHY_REG_LIST_START_WLBANDINITDATA
			/* 10: Lock out automatic writing of tx power, must be done
			 * just before setting PWR_CTL
			 */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_TX_POWER_OVERRIDE, 0x80)
			/* 9: set PWR_CTL done below */

			/* 10: Set antenna diversity mode 3, we do this in wlc_phy_init */

			/* 11: Increase compRSSIDelay from 3us and ant-div-diff-pwr-thd to 10 */
			PHY_REG_WRITE_RAW_ENTRY(BPHY_DIVERSITY_CTL, 0xca)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_TR_LOSS_CTL, 0x88a3)
		PHY_REG_LIST_EXECUTE(pi);
		/* Set TX power to saved values */
		wlc_set_11b_txpower(pi, 0xffff, 0xffff, 0xffff);

		/* Increase LOGEN current in 2050's x2 to increase LO amplitude
		 * and reduce transmitter I-Q imbalance.
		 */
		write_radio_reg(pi, RADIO_2050_PLL_CTL3, 0xd);

		/* Increase RSSI ADC I_ladder to 112.5% of default current.  This
		 * boosts the RSSI ADC full-scale range by 112.5%.
		 */
		W_REG(pi->sh->osh, &regs->phyadcbias,
		      (R_REG(pi->sh->osh, &regs->phyadcbias) & 0xffc0) | 0x04);
	}
}


static void
gphy_update_nrssi_tbl(phy_info_t *pi, int nrssi_table_delta)
{
	int ctr, val;

	PHY_CAL(("wl%d: %s: nrssi_table_delta = %d(0x%x)\n",
	          pi->sh->unit, __FUNCTION__, nrssi_table_delta, nrssi_table_delta));

	/* 4306c0 and later do NOT have this table */
	ASSERT((pi->pubpi.phy_type == PHY_TYPE_G) &&
	       (pi->pubpi.phy_rev == 1));

	/* correct NRSSI values for gphy -- works even when gmode == 0 */
	for (ctr = 0; ctr < PHY_RSSI_TABLE_SIZE; ctr++) {
		val = read_gphy_table_ent(pi, 0, ctr);
		if (val > 31) {
			val = val - 64;
		}
		val = val - nrssi_table_delta;
		if (val > 31) {
			val = 31;
		} else if (val < -32) {
			val = -32;
		}
		val &= 0x3f;
		write_gphy_table_ent(pi, 0, ctr, (int16)val);
	}
}

static uint8 nrssi_tbl[PHY_RSSI_TABLE_SIZE] = {
	0,  1,  2,  3,  4,  5,  6,  7,
	8,  9, 10, 11, 12, 13, 14, 15,
	16, 17, 18, 19, 20, 21, 22, 23,
	24, 25, 26, 27, 28, 29, 30, 31,
	32, 33, 34, 35, 36, 37, 38, 39,
	40, 41, 42, 43, 44, 45, 46, 47,
	48, 49, 50, 51, 52, 53, 54, 55,
	56, 57, 58, 59, 60, 61, 62, 63
};

static void
wlc_nrssi_tbl_gphy_mod(phy_info_t *pi)
{
	uint k;
	int val;
	int nrssi_slope_scale, min_rssi;
	int target_min = 58;


	nrssi_slope_scale =  pi->u.pi_abgphy->abgphy_cal.nrssi_slope_scale;
	min_rssi = 31 - pi->u.pi_abgphy->abgphy_cal.min_rssi;

	PHY_CAL(("wl%d: %s: nrssi_tbl[] =", pi->sh->unit, __FUNCTION__));

	/* Modify NRSSI table */
	for (k = 0; k < PHY_RSSI_TABLE_SIZE; k++) {
		val = k;

		/* Anchor bottom end of range */
		val = target_min + (val - min_rssi) * nrssi_slope_scale / 65536;

		if (val < 0) {
			val = 0;
		} else if (val > 63) {
			val = 63;
		}

		nrssi_tbl[k] = (uint8)val;

		if ((k & 7) == 0)
			PHY_CAL(("\n    [%2d]: ", k));
		PHY_CAL(("%d    ", nrssi_tbl[k]));
	}
	PHY_CAL(("\n"));
}

static void
wlc_nrssi_thresh_gphy_set(phy_info_t *pi)
{
	int val, n1th, n2th;
	int targmin, min1th, max1th, min2th, max2th, b1th, b2th;
	uint16 regval;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	b1th = -13;
	b2th = -10;
	if (pi_abg->sbtml_gm && (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_ADCDIV)) {
		ASSERT(pi->pubpi.phy_rev >= 1);

		targmin = -27;
		min1th = -32;
		max1th = 31;
		min2th = -32;
		max2th = 31;
		/* apply interference mitigation adjustment */
		switch (pi->cur_interference_mode) {
			case NON_WLAN:
				b1th = -13;
				b2th = -17;
				break;
			case WLAN_MANUAL:
			case WLAN_AUTO:
				if (pi->aci_state & ACI_ACTIVE) {
					b1th = -8;
					b2th = -9;
				}
				break;
			case INTERFERE_NONE:
			default:
				b1th = -13;
				b2th = -10;
				break;
		}

		/* Set thresholds based on slope_calibration results
		 * Thresholds have to be inversely mapped from those in gphy_mod_nrssi_tbl
		 */
		n1th = ((b1th - targmin) * (pi_abg->abgphy_cal.max_rssi -
			pi_abg->abgphy_cal.min_rssi) +
			64 * pi_abg->abgphy_cal.min_rssi);
		if (n1th >= 0)
			n1th = (n1th + 32) / 64;
		else
			n1th = (n1th - 32) /64;
		n2th = ((b2th - targmin) * (pi_abg->abgphy_cal.max_rssi -
			pi_abg->abgphy_cal.min_rssi) +
			64 * pi_abg->abgphy_cal.min_rssi);
		if (n2th >= 0)
			n2th = (n2th + 32) / 64;
		else
			n2th = (n2th - 32) /64;


		/* Ensure thresholds are within bounds */
		if (n1th < min1th) {
			PHY_CAL(("wl%d: %s: n1th %d is < min %d\n", pi->sh->unit, __FUNCTION__,
			          n1th, min1th));
			n1th = min1th;
		} else if (n1th > max1th) {
			PHY_CAL(("wl%d: %s: n1th %d is > max %d\n", pi->sh->unit, __FUNCTION__,
			          n1th, max1th));
			n1th = max1th;
		}
		if (n2th < min2th) {
			PHY_CAL(("wl%d: %s: n2th %d is < min %d\n", pi->sh->unit, __FUNCTION__,
			          n2th, min2th));
			n2th = min1th;
		} else if (n2th > max2th) {
			PHY_CAL(("wl%d: %s: n2th %d is > max %d\n", pi->sh->unit, __FUNCTION__,
			          n2th, max2th));
			n2th = max2th;
		}
	} else {
		/* Change NRSSI thresholds based on NRSSI Calibration */

		/* 4306c0 and later do NOT have this table */
		ASSERT((pi->pubpi.phy_type == PHY_TYPE_G) &&
		       (pi->pubpi.phy_rev == 1));
		val = read_gphy_table_ent(pi, 0, 32);
		if (val > 31) {
			val = val - 64;
		}
		/* phy_rev == 1 */
		n1th = -21;
		n2th = -19;

		if (val < 3) {
			n1th -= 4;
			n2th -= 2;
		}
	}

	regval = (uint16)(((0x3f & n2th) << 6) | (0x3f & n1th));

	PHY_CAL(("wl%d: %s: nrssi1_th = %d, nrssi2_th = %d, reg = 0x%x\n", pi->sh->unit,
	          __FUNCTION__, n1th, n2th, regval));
	/* proc gphy_nrssi: preserve bits 12-15 */
	phy_reg_mod(pi, (GPHY_TO_APHY_OFF + APHY_N1_N2_THRESH), 0x0fff, regval);
}


/*
 * Based on all the data taken from the lab at room temp. and 85
 * deg. C, here's the best fixed look-up table for the B0 boards.
 * The input (index) is the TSSI value from the 43xx, modified as
 * stated below.  The output is the power value (in dBm in steps of
 * 0.25 dB) that is used to drive the attenuator controls in the radio
 * and baseband.
 *
 * We see significant variation in the curves based on DC offset in
 * the detector output.  We need to compensate for this by taking one
 * idle (non-transmitting measurement) at the start and then doing the
 * following simple calculation:
 *
 * new_tssi (input to LUT) = tssi_from_43xx + correction
 *
 * where correction = target_idle_tssi - idle_tssi.
 */


static const int8 gphy_tssi_to_dbm[PHY_TSSI_TABLE_SIZE] = {
	77, 77, 77, 76, 76, 76, 75, 75,
	74, 74, 73, 73, 73, 72, 72, 71,
	71, 70, 70, 69, 68, 68, 67, 67,
	66, 65, 65, 64, 63, 63, 62, 61,
	60, 59, 58, 57, 56, 55, 54, 53,
	52, 50, 49, 47, 45, 43, 40, 37,
	33, 28, 22, 14,  5, -7, -20, -20,
	-20, -20, -20, -20, -20, -20, -20, -20
};


static const uint16 tx_gain_dac_table[TX_GAIN_TABLE_LENGTH] = {
	0,  1,  2,  3,  4,  5,  6,  7,
	8,  9, 10, 11,  4,  5,  6,  7,
	8,  9, 10, 11,  7,  8,  9, 10,
	11,  5,  6,  7,  8,  9, 10, 11,
	0,  1,  2,  3,  4,  5,  6,  7,
	8,  9, 10, 11,  7,  8,  9, 10,
	11,  7,  8,  9, 10, 11,  1,  2,
	3,  4,  5,  6,  7,  8,  9, 10
};

static const uint16 tx_gain_bb_table[TX_GAIN_TABLE_LENGTH] = {
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 6, 6, 6, 6,
	6, 6, 6, 6, 5, 5, 5, 5,
	5, 4, 4, 4, 4, 4, 2, 2,
	2, 2, 2, 2, 2, 2, 2, 2
};

static const uint16 tx_gain_rfpa_table[TX_GAIN_TABLE_LENGTH] = {
	4, 4, 4, 4, 4, 4, 4, 4,
	4, 4, 4, 4, 3, 3, 3, 3,
	3, 3, 3, 3, 2, 2, 2, 2,
	2, 1, 1, 1, 1, 1, 1, 1,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static const uint16 tx_gain_rfpad_table[TX_GAIN_TABLE_LENGTH] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static const uint16 tx_gain_rfpga_table[TX_GAIN_TABLE_LENGTH] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};


static const uint16 tx_gain_txmix_table[TX_GAIN_TABLE_LENGTH] = {
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0
};

static void
wlc_set_11a_tpi_set(phy_info_t *pi, int8 tpi)
{
	uint16 tmp;
	uint16	mask = (pi->pubpi.ana_rev == ANA_11G_018) ?
	        BBATT_11G_MASK : BBATT_11G_MASK2;
	int	shift = (pi->pubpi.ana_rev == ANA_11G_018) ?
	        BBATT_11G_SHIFT : BBATT_11G_SHIFT2;

	if ((tpi < 0) || (tpi >= TX_GAIN_TABLE_LENGTH)) {
		PHY_ERROR(("wl%d: %s: Bad txpower index %d\n", pi->sh->unit, __FUNCTION__, tpi));
	}

	if (tpi < 0) {
		tpi = 0;
	} else if (tpi >= TX_GAIN_TABLE_LENGTH) {
		tpi = TX_GAIN_TABLE_LENGTH - 1;
	}

	PHY_TMP(("wl%d: %s: Set idx %d,", pi->sh->unit, __FUNCTION__, tpi));

	tmp = (tx_gain_rfpa_table[tpi] << 5) & 0xe0;
	tmp |= (tx_gain_rfpad_table[tpi] << 3) & 0x18;
	tmp |= tx_gain_rfpga_table[tpi] & 7;
	write_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, tmp);
	PHY_TMP((" TX_RF_GAIN = 0x%x,", tmp));

	tmp = (tx_gain_bb_table[tpi] & 0x0f) | 0x20;
	tmp |= (tx_gain_txmix_table[tpi] << 4) & 0x10;
	write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, tmp);
	PHY_TMP((" TX_BB_GAIN = 0x%x,", tmp));

	tmp = read_aphy_table_ent(pi, 0xc, 1);
	tmp = (tmp & (~mask)) | (tx_gain_dac_table[tpi] << shift);
	WRITE_APHY_TABLE_ENT(pi, 0xc, 1, tmp);
	PHY_TMP((" TX_GAIN_DAC = 0x%x\n", tmp));

	pi->txpwridx = tpi;
	wlc_adjust_bb_loft_cancel(pi, pi->txpwridx);
}

void
wlc_set_11a_txpower(phy_info_t *pi, int8 tpi, bool override)
{
	uint16 pcl_pwridx = WL_PWRIDX_PCL_ON; /* current power control loop power index */

#ifdef BCMDBG
	if (tpi == WL_PWRIDX_PCL_ON) {
		if (override) {
			PHY_ERROR(("wl%d: %s: invalid input: override 0x%d tpi 0x%x\n",
				pi->sh->unit, __FUNCTION__, override, tpi));
			return;
		}
	}
	else if (tpi == WL_PWRIDX_PCL_OFF) {
		if (!override) {
			PHY_ERROR(("wl%d: %s: invalid input: override 0x%d tpi 0x%x\n",
				pi->sh->unit, __FUNCTION__, override, tpi));
			return;
		}
	}
#endif /* BCMDBG */

	if (pi->hwpwrctrl) {
#if defined(BCMDBG) || defined(WLTEST)
#if ACONF
		bool pcl_enable = (override == FALSE) ? TRUE : FALSE;

		pcl_pwridx = phy_reg_read(pi, APHY_RSSI_FILT_B0);
		wlc_phy_txpwrctrl_hw_aphy(pi, pcl_enable); /* configure */
#endif /* ACONF */
#else
		return;
#endif /* BCMDBG || WLTEST */
	}

	pi->u.pi_abgphy->txpwridx_override_aphy = override;

	if (tpi == WL_PWRIDX_PCL_ON) {
		/* nothing to do - PCL is on. */
		return;
	}
	else if (tpi == WL_PWRIDX_PCL_OFF) {
		if (pi->hwpwrctrl) {
			tpi = (int8)pcl_pwridx; /* apply HW PCL index */
		}
		else {
			return; /* nothing to do for soft PCL */
		}
	}

	wlc_set_11a_tpi_set(pi, tpi);
}

int
wlc_get_a_band_range(phy_info_t *pi)
{
	int range;
	uint8 channel;

	channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	range = -1;

#ifdef BAND5G
	if ((channel >= chan_info_abgphy[FIRST_LOW_5G_CHAN].chan) &&
		(channel <= chan_info_abgphy[LAST_LOW_5G_CHAN].chan))
			range = A_LOW_CHANS;
	else if ((channel >= chan_info_abgphy[FIRST_MID_5G_CHAN].chan) &&
		(channel <= chan_info_abgphy[LAST_MID_5G_CHAN].chan))
			range = A_MID_CHANS;
	else if ((channel >= chan_info_abgphy[FIRST_HIGH_5G_CHAN].chan) &&
		(channel <= chan_info_abgphy[LAST_HIGH_5G_CHAN].chan))
			range = A_HIGH_CHANS;
#endif

	return range;
}

/* gen_tssi_to_pwr_table generates the mapping table from a rational function
 * of the TSSI code:  P(C) = (b0 + b1*C) / (1 + a1*C).  b0, b1, and a1 are the
 * 16-bit coefficients extracted from SPROM.
 */
static int
WLBANDINITFN(gen_tssi_to_pwr_tbl)(phy_info_t *pi, int8 *tssi_to_pwr_tbl, int table_size,
	int16 b0, int16 b1, int16 a1, int idle_tssi)
{
	int k, m, iter, indx;
	int32 num, den, den_inv, epsilon, di_tmp;
	int titssi;

	PHY_TMP(("wl%d: %s: tssi_to_pwr_tbl[] =", pi->sh->unit, __FUNCTION__));

	/* For 4318 A phy power control, tcl suggests:
	 * target_idle_tssi - (32 + idle_tssi) = 120 - (32 + 24) = 64.
	 */

	titssi = pi->u.pi_abgphy->target_idle_tssi ? pi->u.pi_abgphy->target_idle_tssi : 120;
	if (!idle_tssi)
		idle_tssi = 24;

	for (indx = 0; indx < table_size; indx++) {
		k = indx;
		if (pi->hwpwrctrl && ISAPHY(pi)) {
			if (indx <= 31) {
				k = indx + titssi - idle_tssi;
			} else {
				k = indx - 64 + titssi - idle_tssi;
			}
		}
		m = (table_size == APHY_TSSI_TABLE_SIZE) ? (k - (APHY_TSSI_TABLE_SIZE >> 1)) : k;
		den_inv = 256;			/* ensures no overflow */
		num = PHY_SHIFT_ROUND(((16 * b0) + (m * b1)), 5);
		den = PHY_SHIFT_ROUND((32768 + (m * a1)), 8);
		if (den < 1) den = 1;		/* ensures no overflow */
		epsilon = 2;
		/* Newton iteration to find 1/x by root-finding. */
		iter = 0;
		while (ABS(epsilon) > 1) {
			if (++iter >= 16) {
				PHY_ERROR(("wl%d: %s: failed to converge\n", pi->sh->unit,
					__FUNCTION__));
				return (-1);
			}
			/* den_inv(k) = 2 * den_inv(k - 1) - den * den_inv(k - 1)^2 */
			di_tmp = PHY_SHIFT_ROUND((den_inv << 12) -
				PHY_SHIFT_ROUND((den * den_inv), 4) * den_inv, 11);
			epsilon = di_tmp - den_inv;
			den_inv = di_tmp;
		}
		/* Output power is expressed in S5.2 dBm. */
		tssi_to_pwr_tbl[indx] = PHY_SAT(PHY_SHIFT_ROUND((num * den_inv), 13), 8);

		if ((k & 7) == 0)
			PHY_TMP(("\n    "));

		PHY_TMP(("%d    ", tssi_to_pwr_tbl[indx]));
	}
	PHY_TMP(("\n"));
	return (0);
}

static int8
wlc_phy_compute_est_pout_abgphy(phy_info_t *pi, chan_info_abgphy_t *ci, int8 ave_tssi)
{
	int16 i;
	int8 dbm = 0;
	int8 *dbm_tbl = NULL;
	int band = 0;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	if (ISAPHY(pi)) {
		/* Find proper tssi2dbm table and idle_tssi */
		band = wlc_get_a_band_range(pi);
		switch (band) {
		case A_LOW_CHANS:
			dbm_tbl = pi_abg->l_tssi_to_dbm;
			break;

		case A_MID_CHANS:
			dbm_tbl = pi_abg->m_tssi_to_dbm;
			break;

		 case A_HIGH_CHANS:
			dbm_tbl = pi_abg->h_tssi_to_dbm;
			break;

		default:
			ASSERT(0);
			break;
		}

		i = ave_tssi + pi_abg->target_idle_tssi - pi->u.pi_abgphy->idle_tssi[band] + 128;
		if (i < 0) i = 0;
		if (i >= APHY_TSSI_TABLE_SIZE) i = APHY_TSSI_TABLE_SIZE - 1;

		/* Per channel adjustment */
		if (pi_abg->a_tssi_to_dbm) {
			/* Add per channel adjustment for srom 1 */
			dbm = pi_abg->a_tssi_to_dbm[i];
			dbm += ci->pwr_est_delta;
		} else {
			dbm = dbm_tbl[i];
		}
	} else {
		band = G_ALL_CHANS;
		i = ave_tssi + pi_abg->target_idle_tssi - pi->u.pi_abgphy->idle_tssi[band];

		if (i < 0) i = 0;
		if (i >= PHY_TSSI_TABLE_SIZE) i = PHY_TSSI_TABLE_SIZE - 1;
		dbm = tssi_to_dbm[i];
	}

	PHY_TXPWR(("wl%d: %s: %s ave_tssi %d titssi %d idle_tssi %d => index %d, dbm %d\n",
	        pi->sh->unit, __FUNCTION__, (pi->pubpi.phy_type == PHY_TYPE_A) ?
	        "Aphy" : "B/Gphy", ave_tssi, pi_abg->target_idle_tssi,
		pi->u.pi_abgphy->idle_tssi[band],
	        i, dbm));

	return (dbm);
}

static int
WLBANDINITFN(wlc_phy_gen_tssi_tables)(phy_info_t *pi, char cg, int16 txpa[],
	int8 itssit, uint8 txpwr)
{
	int terr = -1;
	int size;
	uint firstchan, lastchan, i;
	int idle_tssi;
	int8 **tssi_to_dbm_tmp = NULL;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	if (NORADIO_ENAB(pi->pubpi)) {
		PHY_TXPWR(("wl%d: %s: No radio: no txpwr control\n",
		          pi->sh->unit, __FUNCTION__));
		return (0);
	}

	if ((txpa[0] != 0) && (txpa[0] != -1) &&
	    (txpa[1] != 0) && (txpa[1] != -1) &&
	    (txpa[2] != 0) && (txpa[2] != -1)) {
		if ((itssit == 0) || (itssit == -1))
			pi_abg->target_idle_tssi = 62;
		else
			pi_abg->target_idle_tssi = itssit;

		switch (cg) {
		case 'b':
			terr = gen_tssi_to_pwr_tbl(pi, tssi_to_dbm,
			PHY_TSSI_TABLE_SIZE, txpa[0], txpa[1], txpa[2],
			pi->u.pi_abgphy->idle_tssi[G_ALL_CHANS]);
			break;

		case 'a':
			CASECHECK(PHYTYPE, PHY_TYPE_A);
			pi->a_band_high_disable = TRUE;
			firstchan = FIRST_5G_CHAN;
			lastchan = LAST_5G_CHAN;
			tssi_to_dbm_tmp = &pi_abg->a_tssi_to_dbm;
			idle_tssi = pi_abg->idle_tssi[A_MID_CHANS];
			goto do_cg;

		case 'l':
			CASECHECK(PHYTYPE, PHY_TYPE_A);
			firstchan = FIRST_LOW_5G_CHAN;
			lastchan = LAST_LOW_5G_CHAN;
			tssi_to_dbm_tmp = &pi_abg->l_tssi_to_dbm;
			idle_tssi = pi_abg->idle_tssi[A_LOW_CHANS];
			goto do_cg;

		case 'm':
			CASECHECK(PHYTYPE, PHY_TYPE_A);
			firstchan = FIRST_MID_5G_CHAN;
			lastchan = LAST_MID_5G_CHAN;
			tssi_to_dbm_tmp = &pi_abg->m_tssi_to_dbm;
			idle_tssi = pi_abg->idle_tssi[A_MID_CHANS];
			goto do_cg;

		case 'h':
			CASECHECK(PHYTYPE, PHY_TYPE_A);
			firstchan = FIRST_HIGH_5G_CHAN;
			lastchan = LAST_HIGH_5G_CHAN;
			idle_tssi = pi_abg->idle_tssi[A_HIGH_CHANS];
			tssi_to_dbm_tmp = &pi_abg->h_tssi_to_dbm;

do_cg:
			if (pi->hwtxpwr == NULL) {
				pi->hwtxpwr = MALLOC(pi->sh->osh, sizeof(uint8) *
					ARRAYSIZE(chan_info_abgphy));
			}

			if (!pi->hwtxpwr) {
				PHY_ERROR(("wlc_phy_gen_tssi_tables: "
					"out of memory, malloced %d bytes",
					MALLOCED(pi->sh->osh)));
				return (-1);
			}

			for (i = firstchan; i <= lastchan; i++) {
				if (cg == 'a')
					pi->hwtxpwr[i] = MIN(chan_info_abgphy[i].hwtxpwr, txpwr);
				else
					pi->hwtxpwr[i] = txpwr;
			}

			if (pi->hwpwrctrl)
				size = PHY_TSSI_TABLE_SIZE;
			else
				size = APHY_TSSI_TABLE_SIZE;

			if (*tssi_to_dbm_tmp == NULL)
				*tssi_to_dbm_tmp = MALLOC(pi->sh->osh, APHY_TSSI_TABLE_SIZE);

			if (*tssi_to_dbm_tmp != NULL) {
				terr = gen_tssi_to_pwr_tbl(pi, *tssi_to_dbm_tmp, size,
				                           txpa[0], txpa[1], txpa[2], idle_tssi);
			} else {
				PHY_ERROR(("wlc_phy_gen_tssi_tables: "
					"out of memory, malloced %d bytes",
					MALLOCED(pi->sh->osh)));
				return (-1);
			}
		}
	}

	PHY_TXPWR(("wl%d: %s: %cphy target idle tssi = %d, b0 = %d, b1 = %d, a1 = %d; %s\n",
	          pi->sh->unit, __FUNCTION__, cg, itssit, txpa[0], txpa[1], txpa[2],
	          (terr ? "No table generated" : "Table OK")));

	if (terr) {
		/* handle unprogrammed boards or failure to converge */

		PHY_TXPWR(("wl%d: %s: Unprogrammed boards or failure to converge\n",
		          pi->sh->unit, __FUNCTION__));
		switch (pi->pubpi.phy_type) {
		case PHY_TYPE_A:
			CASECHECK(PHYTYPE, PHY_TYPE_A);
			if (tssi_to_dbm_tmp && *tssi_to_dbm_tmp) {
				MFREE(pi->sh->osh, *tssi_to_dbm_tmp, APHY_TSSI_TABLE_SIZE);

				/* Mark it unused so we don't attempt to re-mfree ()
				 * it again later in phy_detach().
				 */
				*tssi_to_dbm_tmp = NULL;
				return (-1);
			}
			break;

		case PHY_TYPE_G:
			CASECHECK(PHYTYPE, PHY_TYPE_G);
			pi_abg->target_idle_tssi = 52;
			bcopy(gphy_tssi_to_dbm, tssi_to_dbm,
			      PHY_TSSI_TABLE_SIZE * sizeof(tssi_to_dbm[0]));
			break;
		}
	}

	return (0);
}

/* Calculate just the estimated Pout */
static bool
wlc_txpower_11a_compute_est_Pout(phy_info_t *pi)
{
	uint16 tmp;
	int32 t0, t1, t2, t3;
	uint i;
	int8 ave_tssi;
	chan_info_abgphy_t *ci;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Read the 4 tssi values saved by the ucode, sign extend them */
	tmp = wlapi_bmac_read_shm(pi->sh->physhim, M_A_TSSI_0);
	t0 = tmp >> 8;
	t0 <<= 24;
	t0 >>= 24;
	t1 = tmp & 0xff;
	t1 <<= 24;
	t1 >>= 24;
	tmp = wlapi_bmac_read_shm(pi->sh->physhim, M_A_TSSI_1);
	t2 = tmp >> 8;
	t2 <<= 24;
	t2 >>= 24;
	t3 = tmp & 0xff;
	t3 <<= 24;
	t3 >>= 24;

	PHY_TXPWR(("wl%d: %s: Read 11a tssi[0-3] = %d(0x%02x) %d(0x%02x) %d(0x%02x) %d(0x%02x)\n",
	          pi->sh->unit, __FUNCTION__, t0, t0, t1, t1, t2, t2, t3, t3));

	if ((t0 == NULL_TSSI) || (t1 == NULL_TSSI) ||
	    (t2 == NULL_TSSI) || (t3 == NULL_TSSI)) {
		PHY_TXPWR(("wl%d: %s: At least one is 0x%x, returning\n",
		          pi->sh->unit, __FUNCTION__, NULL_TSSI));
		return (FALSE);
	}

	/* clear the tssi values for next round */
	wlc_phy_clear_tssi((wlc_phy_t*)pi);

	ave_tssi = (t0 + t1 + t2 + t3 + 2) / 4;		/* +2 So it rounds up */

	/* lookup radio-chip-specific channel code */
	for (i = 0; i < ARRAYSIZE(chan_info_abgphy); i++)
		if (chan_info_abgphy[i].chan == CHSPEC_CHANNEL(pi->radio_chanspec))
			break;

	ASSERT(i < ARRAYSIZE(chan_info_abgphy));
	ci = &chan_info_abgphy[i];

	/* compute estimate Pout */
	pi->txpwr_est_Pout = wlc_phy_compute_est_pout_abgphy(pi, ci, ave_tssi);

	return (TRUE);
}

static bool
wlc_txpower_11a_recalc(phy_info_t *pi)
{
	int8 new_index, delta;

	if (pi->u.pi_abgphy->txpwridx_override_aphy)
		return (TRUE);

	/* Delta power */
#ifdef PPR_API
	delta = pi->b20_1x1_ofdm6 - pi->txpwr_est_Pout;
#else
	delta = pi->tx_power_target[TXP_FIRST_OFDM] - pi->txpwr_est_Pout;

	PHY_TXPWR(("wl%d: %s: target = %d, est_pout = %d, delta = %d\n", pi->sh->unit,
	          __FUNCTION__, pi->tx_power_target[TXP_FIRST_OFDM],
	          pi->txpwr_est_Pout, delta));
#endif /* PPR_API */

	delta = (delta + 1) / 2;	/* The steps in the table are dbm/2 */

	if (delta == 0)
		return (TRUE);

	new_index = MIN(47, pi->txpwridx - delta);

	PHY_TXPWR(("wl%d: %s: Old index %d, New index %d\n", pi->sh->unit, __FUNCTION__,
	          pi->txpwridx, new_index));

	/* We are going to access phy and radio */
	wlc_phyreg_enter((wlc_phy_t*)pi);
	wlc_radioreg_enter((wlc_phy_t*)pi);
	wlc_set_11a_txpower(pi, new_index, FALSE);
	wlc_radioreg_exit((wlc_phy_t*)pi);
	wlc_phyreg_exit((wlc_phy_t*)pi);

	return (TRUE);
}

void
wlc_phy_cal_txpower_stats_clr_gphy(phy_info_t *pi)
{
	/* reset stats_11b_txpower to 0 */
	bzero((char *)pi->u.pi_abgphy->stats_11b_txpower,
		sizeof(pi->u.pi_abgphy->stats_11b_txpower));
}

static void
wlc_txpower_11b_inc(phy_info_t *pi, uint16 bbatten, uint16 radiopwr, uint16 txctl1)
{
	int rfgainid, bbattnid;

	gphy_complo_map_txpower(pi, radiopwr, bbatten, txctl1, &rfgainid, &bbattnid);
	pi->u.pi_abgphy->stats_11b_txpower[rfgainid][bbattnid]++;
}

/* Calculate just the estimated Pout */
static bool
wlc_txpower_11b_compute_est_Pout(phy_info_t *pi)
{
	uint16 t0, t1, t2, t3, tmp;
	int8 ave_tssi;
	bool is_ofdm = FALSE;
#ifdef PPR_API
/*	ppr_ofdm_rateset_t ofdm_limits; */
#endif

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	/* Read the 4 CCK tssi values saved by the ucode */
	tmp = wlapi_bmac_read_shm(pi->sh->physhim, M_B_TSSI_0);
	t0 = tmp >> 8;
	t1 = tmp & 0xff;
	tmp = wlapi_bmac_read_shm(pi->sh->physhim, M_B_TSSI_1);
	t2 = tmp >> 8;
	t3 = tmp & 0xff;

	if ((t0 == NULL_TSSI) || (t1 == NULL_TSSI) || (t2 == NULL_TSSI) || (t3 == NULL_TSSI)) {
		/* Read the 4 OFDM tssi values saved by the ucode */
		tmp = wlapi_bmac_read_shm(pi->sh->physhim, M_G_TSSI_0);
		t0 = tmp >> 8;
		t1 = tmp & 0xff;
		tmp = wlapi_bmac_read_shm(pi->sh->physhim, M_G_TSSI_1);
		t2 = tmp >> 8;
		t3 = tmp & 0xff;

		if ((t0 == NULL_TSSI) || (t1 == NULL_TSSI) || (t2 == NULL_TSSI) || (t3 ==
			NULL_TSSI)) {
			PHY_TMP(("wl%d: %s: At least one OFDM is 0x%x tssi[0-3] = 0x%02x 0x%02x"
			         " 0x%02x 0x%02x\n",
			         pi->sh->unit, __FUNCTION__, NULL_TSSI, t0, t1, t2, t3));
			return (FALSE);
		}

		/* Normalize the values to be like the cck values */
		t0 += 0x20;
		t0 &= 0x3f;
		t1 += 0x20;
		t1 &= 0x3f;
		t2 += 0x20;
		t2 &= 0x3f;
		t3 += 0x20;
		t3 &= 0x3f;
		is_ofdm = TRUE;

		PHY_TXPWR(("wl%d: %s: Using OFDM tssi[0-3] = 0x%02x 0x%02x 0x%02x 0x%02x\n",
		          pi->sh->unit, __FUNCTION__, t0, t1, t2, t3));
	} else {
		PHY_TXPWR(("wl%d: %s: Using CCK tssi[0-3] = 0x%02x 0x%02x 0x%02x 0x%02x\n",
		          pi->sh->unit, __FUNCTION__, t0, t1, t2, t3));
	}

	/* Clear all the values for next period */
	wlc_phy_clear_tssi((wlc_phy_t*)pi);

	ave_tssi = (t0 + t1 + t2 + t3 + 2) / 4;		/* +2 So it rounds up */

	/* compute estimate Pout */
	pi->txpwr_est_Pout = wlc_phy_compute_est_pout_abgphy(pi, NULL, ave_tssi);

	if (is_ofdm) {
#ifdef PPR_API
		ppr_ofdm_rateset_t ofdm_offsets;
		ppr_get_ofdm(pi->tx_power_offset, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
			&ofdm_offsets);

		pi->txpwr_est_Pout += ofdm_offsets.pwr[0];
#else
		pi->txpwr_est_Pout += pi->tx_power_offset[TXP_FIRST_OFDM];
#endif
	}

	return (TRUE);
}

static bool
wlc_txpower_11b_recalc(phy_info_t *pi)
{
	uint16 tx_ctl1;
	int8 delta, delta_radio, delta_baseband;
	int8 baseband_tx_attn, radio_tx_attn, max_radio_attn;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	/* Delta power in .25 dbm units */
#ifdef PPR_API
	delta = pi->b20_1x1_dsss1 - pi->txpwr_est_Pout;
#else
	delta = pi->tx_power_target[TXP_FIRST_CCK] - pi->txpwr_est_Pout;

	PHY_TXPWR(("wl%d: %s: target = %d, est_pout = %d, delta = %d\n", pi->sh->unit,
	          __FUNCTION__, pi->tx_power_target[TXP_FIRST_CCK],
	          pi->txpwr_est_Pout, delta));
#endif /* PPR_API */
	max_radio_attn = MAX_TX_ATTN_2050;

	/* compute change in baseband and radio tx gains */
	/* Delta_radio is 2 db steps */
	/* Delta baseband is .5 db steps */
	if (delta < 0) {
		/* Guard against delta taking us below 0 */
		delta_radio = MAX(-(delta / 8), -((int8)pi_abg->radiopwr));
	} else {
		/* Guard against delta taking us up past MAX_TX_ATTN */
		delta_radio = MIN(-(delta / 8), (max_radio_attn - (int8)pi_abg->radiopwr));
	}

	/* Orig delta is .25 db, delta_radio is 2 db.  Convert both to
	 * .5 db for delta_baseband
	 */
	delta_baseband = -(delta/2) - (delta_radio * 4);

	/* Nothing to do, just record setting used */
	if ((delta_radio == 0) && (delta_baseband == 0)) {
		wlc_txpower_11b_inc(pi, pi_abg->bb_atten, pi_abg->radiopwr, pi_abg->txctl1);
		return (TRUE);
	}

	/* Cannot read the value fom the radio, since the ucode might
	 * just have changed it for an ofdm frame. So we just use our
	 * cached value.
	 */
	baseband_tx_attn = (int8)pi_abg->bb_atten;
	radio_tx_attn = (int8)pi_abg->radiopwr;
	tx_ctl1 = pi_abg->txctl1;
	PHY_TXPWR(("wl%d: %s: current bb/radio/ctl1 = %d/%d/%d, delta bb/radio = %d/%d\n",
	          pi->sh->unit, __FUNCTION__, baseband_tx_attn, radio_tx_attn, tx_ctl1,
	          delta_baseband, delta_radio));

	baseband_tx_attn += delta_baseband;
	radio_tx_attn += delta_radio;

	/* min. baseband attn. should probably be 1, if possible */
	while ((baseband_tx_attn < BB_TX_ATTEN_MIN) && (radio_tx_attn > 0)) {
		/* Reduce radio attenuation */
		baseband_tx_attn += 4;
		radio_tx_attn -= 1;
	}
	while ((baseband_tx_attn > BB_TX_ATTEN_MAX) && (radio_tx_attn < max_radio_attn)) {
		/* Push more of the attenuation to the radio */
		baseband_tx_attn -= 4;
		radio_tx_attn += 1;
	}

	/* Only radiorev 2 has power boost */
	if (pi->pubpi.radiorev == 2) {

		if (radio_tx_attn < 0)
			radio_tx_attn = 0;

		if (radio_tx_attn < 2) {
			if (tx_ctl1 == 0) {
				/* turn on the PA_GAIN[0] and TX_MIX_GAIN bits, boosting gain by 5
				 */
				tx_ctl1 = TXC1_5DB;
				radio_tx_attn += 2;	/* decrease tx path gain by 4 dB */
				baseband_tx_attn += 2;	/* decrease tx path gain by 1 dB */
				PHY_TXPWR(("wl%d: %s: Boosting 2050 5db, new bb/radio/ctl1 ="
				          " %d/%d/%d\n", pi->sh->unit, __FUNCTION__,
				          baseband_tx_attn, radio_tx_attn, tx_ctl1));
			} else {
				if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_PACTRL) {
					/* Hidden bits already on, nowhere to go, just make radio 2
					 */
					baseband_tx_attn -= 4 * (2 - radio_tx_attn);
					radio_tx_attn = 2;
				}
				PHY_TXPWR(("wl%d: %s: 5db already on, new bb/radio/ctl1 ="
				          " %d/%d/%d\n", pi->sh->unit, __FUNCTION__,
				          baseband_tx_attn, radio_tx_attn, tx_ctl1));
			}
		} else {
			if ((radio_tx_attn > 4) && (tx_ctl1 != 0)) {
				/* turn off pa_gain[0] and tx_mix_gain bits, reducing tx path gain
				 * by 5dB total
				 */
				tx_ctl1 = 0;
				if (baseband_tx_attn > 2) {
					radio_tx_attn -= 2;
					baseband_tx_attn -= 2;
				} else {
					radio_tx_attn -= 3;
					baseband_tx_attn += 2;
				}
				PHY_TXPWR(("wl%d: %s: Lowering 2050 5db, new bb/radio/ctl1 ="
				          " %d/%d/%d\n", pi->sh->unit, __FUNCTION__,
				          baseband_tx_attn, radio_tx_attn, tx_ctl1));
			}
		}
	}

	if (baseband_tx_attn < 0)
		baseband_tx_attn = 0;
	if (baseband_tx_attn > MAX_TX_ATTN)
		baseband_tx_attn = MAX_TX_ATTN;

	if (radio_tx_attn < 0)
		radio_tx_attn = 0;
	if (radio_tx_attn > max_radio_attn)
		radio_tx_attn = max_radio_attn;

	PHY_TXPWR(("wl%d: %s:   Final bb/radio/ctl1 = %d/%d/%d\n", pi->sh->unit,
	          __FUNCTION__, baseband_tx_attn, radio_tx_attn, tx_ctl1));

	/* We are going to access phy and radio regs */
	wlc_phyreg_enter((wlc_phy_t*)pi);
	wlc_radioreg_enter((wlc_phy_t*)pi);

	/* Finally actually set the registers */
	wlc_set_11b_txpower(pi, baseband_tx_attn, radio_tx_attn, tx_ctl1);

	/* record setting used */
	wlc_txpower_11b_inc(pi, baseband_tx_attn, radio_tx_attn, tx_ctl1);

	wlc_radioreg_exit((wlc_phy_t*)pi);
	wlc_phyreg_exit((wlc_phy_t*)pi);
	return (TRUE);
}

uint16
WLBANDINITFN(wlc_default_radiopwr_gphy)(phy_info_t *pi)
{
	uint8 radiorev = pi->pubpi.radiorev;
	uint16 radiopwr = (uint16)pi->radiopwr_override;

	if (pi->radiopwr_override != RADIOPWR_OVERRIDE_DEF) {
		/* Use stored value */
		return radiopwr;
	}

	/* Default radio power (actually, attenuation). Note
	 * that if dynamic power control is working, this
	 * default is in effect for only 15 seconds.
	 */

	if (ISAPHY(pi)) {
		/* return value for register write */
		return 0x60;
	}

	radiopwr = 5;

	/* exceptions to the default */
	switch (radiorev) {
	case 8:
		radiopwr = 0x1a;
		break;

	case 1:
		if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
		    (pi->sh->boardtype == BCM94309G_BOARD) &&
		    (pi->sh->boardrev >= 0x30)) {
			/* The bcm94309g miniPCI card uses a MMPA742S PA,
			 * which has higher P1dB and hence allows > + 13 dBm
			 * at the antenna connector.  Note that the
			 * bcm94309g miniPCI board uses a 2.0V regulator
			 * for the radios in place of the 1.8V regulator
			 * for 2060 PLL lock reasons.  This gives us 5 dB
			 * (!) more output power from the radio; so, we
			 * don't need to reduce the attenuator setting
			 * relative to the other boards in gmode.
			 * 11/4/02: back down 4dB (allows 2dB margin)
			 */
			radiopwr = 3;
		} else {
			/* Otherwise, the PA can't support that much power.
			 * It's either a SiGe PA (BU4306) or a MMPA742.
			 */
			if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
			    (pi->sh->boardtype == BU4306_BOARD)) {
				radiopwr = 3;
			} else {
				radiopwr = 1;
			}
		}
		break;

	case 2:
		if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
		    (pi->sh->boardtype == BCM94309G_BOARD) &&
		    (pi->sh->boardrev >= 0x30)) {
			/* The bcm94309g miniPCI card uses a MMPA742S PA,
			 * which has higher P1dB and hence allows > + 13 dBm
			 * at the antenna connector.  Note that the
			 * bcm94309g miniPCI board uses a 2.0V regulator
			 * for the radios in place of the 1.8V regulator
			 * for 2060 PLL lock reasons.  This gives us 5 dB
			 * (!) more output power from the radio; so, we
			 * don't need to reduce the attenuator setting
			 * relative to the other boards in gmode.
			 * 11/4/02: back down 4dB (allows 2dB margin)
			 */
			radiopwr = 3;
		} else {
			/* Otherwise, the PA can't support that much power.
			 * It's either a SiGe PA (BU4306) or a MMPA742.
			 */
			if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
			    (pi->sh->boardtype == BU4306_BOARD)) {
				radiopwr = 5;
			} else if (CHIPID(pi->sh->chip) == BCM4320_CHIP_ID) {
				radiopwr = 4;
			} else {
				radiopwr = 3;
			}
		}
		break;
	}

	return (radiopwr);
}

static void
wlc_radio_2060_init(phy_info_t *pi)
{
	uint16 rc_cal;

	PHY_TRACE(("wl%d: %s\n", pi->sh->unit, __FUNCTION__));

	if (NORADIO_ENAB(pi->pubpi))
		return;


	/* preliminary WW init code (10/28/02) */
	write_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, 0xc0);
	write_radio_reg(pi, RADIO_2060WW_PWR_STACTL, 0x08);
	write_radio_reg(pi, RADIO_2060WW_RX_GM_UPDN, 0x40);
	write_radio_reg(pi, RADIO_2060WW_PLL_ERRTHR, 0xaa);

	/* 2060ww_logen_curr: Reduce IQ imbalance */
	write_radio_reg(pi, RADIO_2060WW_LOGEN_CUR1, 0x8f);
	write_radio_reg(pi, RADIO_2060WW_RXLNA_DACC, 0x8f);
	write_radio_reg(pi, RADIO_2060WW_LOGEN_CUR3, 0x8f);
	write_radio_reg(pi, RADIO_2060WW_PLL_GM_CTL, 0x07);

	/* 2060ww_rc_cal */

	/* Set rc_cal control to
	 *	1) calc & update cal code @ posedge of ext_cal_trig
	 *	2) count 2 RC cycles & settle for 2 RC cycles
	 *	3) calculate cal code 4 times per algo step
	 *	4) use binary search at power-up and one-step thereafter
	 *	5) freq step 1 for analog RC osc
	 */
	write_radio_reg(pi, RADIO_2060WW_RC_CALVAL1, 0);

	/* Set rc_cal control : corresponds to 2 RC cycles
	 * 2 * 20MHz / 183 kHz
	 */
	write_radio_reg(pi, RADIO_2060WW_RC_CALVAL0, 218);

	/* power up calib circuitry */
	mod_radio_reg(pi, RADIO_2060WW_PWR_STACTL, 8, 0);

	/* MUST ensure ext_cal_trig is reset to 0 */
	mod_radio_reg(pi, RADIO_2060WW_RST_PLLCAL, 0x10, 0);

	/* give rst_n_out_rccal a rising edge */
	mod_radio_reg(pi, RADIO_2060WW_RST_PLLCAL, 0x20, 0);
	mod_radio_reg(pi, RADIO_2060WW_RST_PLLCAL, 0x20, 0x20);

	/* give ext_cal_trig a rising edge */
	OSL_DELAY(400);
	mod_radio_reg(pi, RADIO_2060WW_RST_PLLCAL, 0x10, 0x10);
	OSL_DELAY(300);

	/* power down calib circuitry */
	mod_radio_reg(pi, RADIO_2060WW_PWR_STACTL, 8, 8);

	/* bottom 5 bits contain auto rc_cal value */
	rc_cal = read_radio_reg(pi, RADIO_2060WW_CALIB_CODE) & 0x1f;

	/* If rc_cal value == 0x1f, declare RC calibration failed and
	 * hence force rc_cal value to 0x10 (best-guess value)
	 */
	if (rc_cal == 0x1f) {
		PHY_ERROR(("wl%d: %s: Using default value for 2060 RC calibration.\n",
		          pi->sh->unit, __FUNCTION__));
		/* Keep the forced RC_cal value to a low value.
		 * This ensures that we err on the higher LPF b/w side (low rc_cal value),
		 *  instead of on the lower LPF b/w side (high rc_cal value)
		 */
		rc_cal = 0xa;
	}
	/* Force RC_CAL value */
	/* set cal_ctrl[1:0] = '11' */
	mod_radio_reg(pi, RADIO_2060WW_RC_CALVAL1, 0x3, 0x3);

	/* Set expt_cnt[4:0] = rc_cal = forced_rc_calib_code */
	mod_radio_reg(pi, RADIO_2060WW_RC_CALVAL0, 0x1f, rc_cal);

	PHY_CAL(("wl%d: %s: 2060 rc_cal = 0x%x\n", pi->sh->unit, __FUNCTION__, rc_cal));

	/* 2060ww_r_cal */

	/* use R-autocal value instead of override val */
	mod_radio_reg(pi, RADIO_2060WW_RCALIB_OVR, 0x10, 0);

	/* power up calib circuitry */
	mod_radio_reg(pi, RADIO_2060WW_PWR_STACTL, 8, 0);

	/* give rst_n_out_rccal a rising edge */
	mod_radio_reg(pi, RADIO_2060WW_RST_PLLCAL, 0x40, 0);
	mod_radio_reg(pi, RADIO_2060WW_RST_PLLCAL, 0x40, 0x40);

	/* power down calib circuitry */
	mod_radio_reg(pi, RADIO_2060WW_PWR_STACTL, 8, 8);

	/* top nibble contains auto r_cal value */
	PHY_CAL(("wl%d: %s: 2060 RCAL_BGVAL 0x%x\n", pi->sh->unit, __FUNCTION__,
	           (read_radio_reg(pi, RADIO_2060WW_RCAL_BGVAL) >> 4) & 0xf));

	/* 2060_improvements: */

	PHY_REG_LIST_START
		/* Rx, Tx I/Q compensation */
		PHY_REG_WRITE_RAW_ENTRY(APHY_RX_COMP_COEFF, 0xddc6)
		PHY_REG_WRITE_RAW_ENTRY(APHY_TX_COMP_COEFF, 0x07be)
		/* LO Feedthrough cancellation */
		PHY_REG_WRITE_RAW_ENTRY(APHY_TX_COMP_OFFSET, 0)
	PHY_REG_LIST_EXECUTE(pi);

	/* drop RF gain: No point, it will be reset
	 * in set_channel
	 */

	/* set MSB of reg(RF_cry_control)
	 * for use with 2-pin XO mode
	 */
	or_radio_reg(pi, RADIO_2060WW_CRY_CONTROL, 0x80);

	/* End of 2060_improvements for 2060ww */

	wlc_phy_chanspec_set((wlc_phy_t*)pi, pi->radio_chanspec);
	OSL_DELAY(1000);
}


bool
BCMATTACHFN(wlc_phy_txpwr_srom_read_gphy)(phy_info_t *pi)
{
	char varname[32];
	char *val;
	uint i;
	int8 itssit, txpwr;

	/* Power settings for 2.4GHz Band */
	itssit = (int8)PHY_GETINTVAR(pi, "pa0itssit");

	/* G Band SROM rev 1: A single max tx pwr for all channels, specified in
	 * quarter dbm
	 */
	pi->tx_srom_max_2g = txpwr = (int8)PHY_GETINTVAR(pi, "pa0maxpwr");

	/* get the pa characteristics */
	for (i = 0; i < 3; i++) {
		snprintf(varname, sizeof(varname), "pa0b%d", i);
		pi->txpa_2g[i] = (int16)PHY_GETINTVAR(pi, varname);
	}
	PHY_TXPWR(("wl%d: %s: 2.4 GHz band tx maxpwr from srom: %d(0x%x)\n",
		pi->sh->unit, __FUNCTION__, txpwr, txpwr));

	val = PHY_GETVAR(pi, "cckpo");
	if (val) {
		/* Like for a band, the *po variable adds Power Offset for each rate.
		 * Instead of a single opo for all OFDM rates, there will be 12 power
		 * offsets.  One for each rate.  The 4 CCK offsets in one 32 bit word
		 * and the 8 OFDM offsets in another word. All offsets are based off
		 * of the maxtxpwr variable.  Offsets in SROM are in half dbm units.
		 *  need to be converted to qdb first
		 */
		uint16 offset;
		uint32 offset_ofdm;

#ifdef PPR_API
#else
		uint max_pwr_chan = txpwr;
#endif /* PPR_API */

		offset = (int16)bcm_strtoul(val, NULL, 0);
#ifdef PPR_API
		pi->ppr.srlgcy.cckpo = offset;
#else
		for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
			pi->tx_srom_max_rate_2g[i] = max_pwr_chan - ((offset & 0xf) * 2);
			offset >>= 4;
		}
#endif /* PPR_API */

		offset_ofdm = (uint32)PHY_GETINTVAR(pi, "ofdmgpo");
#ifdef PPR_API
		pi->ppr.srlgcy.ofdmgpo = offset_ofdm;
#else
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_2g[i] = max_pwr_chan - ((offset_ofdm & 0xf) * 2);
			offset_ofdm >>= 4;
		}
#endif /* PPR_API */
	} else {
		/* G Band SROM rev 2: Adds OFDM Power Offset (opo). Newer
		 * HW allows us higher txpower for CCK but not OFDM. Max txpwr
		 * is interpreted for CCK for rates.  OPO is subtracted from that
		 * to give max txpwr for OFDM rates. OPO is given in quarter dbm.
		 */
		uint8 opo = (uint8) PHY_GETINTVAR_DEFAULT(pi, "opo", 255);	/* Bogus value */

		PHY_TXPWR(("reading opo = %d\n", opo));
		/* Its possible we have a bogus value or non-existent
		 * value (0xff). In either case, if the board has power
		 * boost boardflag, give a 4 dbm boost.  Otherwise,
		 * no boost
		 */
		if (opo > 20) {
			if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_CCKHIPWR)
				opo = 16;
			else
				opo = 0;
		}
		PHY_TXPWR(("setting opo = %d\n", opo));
#ifdef PPR_API
		pi->ppr.srlgcy.opo = opo;
#else
		/* Populate max power array  for CCK rates */
		for (i = TXP_FIRST_CCK; i <= TXP_LAST_CCK; i++) {
			pi->tx_srom_max_rate_2g[i] = txpwr;
		}

		/* Populate max power array  for OFDM rates */
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM;  i++) {
			pi->tx_srom_max_rate_2g[i] = txpwr - opo;
		}
#endif /* PPR_API */
	}

#ifndef PPR_API
	PHY_TXPWR(("srom max is:\n"));
	for (i = 0; i < TXP_NUM_RATES; i++)
		PHY_TXPWR(("%d ", pi->tx_srom_max_rate_2g[i]));
	PHY_TXPWR(("\n"));
#endif

	if (wlc_phy_gen_tssi_tables(pi, 'b', pi->txpa_2g, itssit, txpwr)) {
		/* pi and other allocated memory will be released in detach time */
		return FALSE;
	}

	return TRUE;
}

bool
BCMATTACHFN(wlc_phy_txpwr_srom_read_aphy)(phy_info_t *pi)
{
	char varname[32];
	uint32 offset;
	uint i;
	int8 itssit, txpwr;

	itssit = (int8)PHY_GETINTVAR(pi, "pa1itssit");
	txpwr = (int8)PHY_GETINTVAR(pi, "pa1maxpwr");

	/* get the pa characteristics */
	for (i = 0; i < 3; i++) {
		snprintf(varname, sizeof(varname), "pa1b%d", i);
		pi->txpa_5g_mid[i] = (int16)PHY_GETINTVAR(pi, varname);
	}

	if (pi->sh->sromrev <= 1) {
		/* SROM rev 1 has a single maxpower for all A band channels. */
		pi->tx_srom_max_5g_mid = txpwr = (int8)PHY_GETINTVAR(pi, "pa1maxpwr");
		PHY_INFORM(("wl%d: %s: 5 GHz band tx maxpwr from srom: %d(0x%x)\n",
			pi->sh->unit, __FUNCTION__, txpwr, txpwr));
#ifndef PPR_API
		/* Only a single max power across all channels */
		for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
			pi->tx_srom_max_rate_5g_mid[i] = txpwr;	/* mid */
			pi->tx_srom_max_rate_5g_low[i] = txpwr;	/* low */
			pi->tx_srom_max_rate_5g_hi[i] = txpwr;	/* hi */
		}
#endif /* PPR_API */
		/* Reuse the single set of parms for all channels */
		if (wlc_phy_gen_tssi_tables(pi, 'a', pi->txpa_5g_mid, itssit, txpwr)) {
			/* pi and other allocated memory will be released in detach time */
			return FALSE;
		}

		return TRUE;
	}

	/* SROM rev 2 introduced A band channel bands of lo, mid & hi and
	 * separate max tx powers for each.
	 * The *po variables introduce a separate max tx power for reach rate
	 * for each of the 3 subbands. Each per-rate txpower is specified as
	 * offset from the maxtxpower from the maxtxpwr in that band (lo,mid,hi).
	 * The offsets in the variables is stored in half dbm units to save
	 * srom space, which need to be doubled to convert to __quarter dbm__ units
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
	PHY_TXPWR(("wl: %s: A Band mid-band maxpwr from srom: ", __FUNCTION__));
	for (i = 0; i < TXP_NUM_RATES; i++)
		PHY_TXPWR(("%d ", pi->tx_srom_max_rate_5g_mid[i]));
	PHY_TXPWR(("\n"));
#endif /* PPR_API */
	if (wlc_phy_gen_tssi_tables(pi, 'm', pi->txpa_5g_mid, itssit, txpwr)) {
		/* pi and other allocated memory will be released in detach time */
		return FALSE;
	}

	/* Low channels */
	/* Extract 8 OFDM rates for low channels */
	offset = (uint32)PHY_GETINTVAR(pi, "ofdmalpo");
	pi->tx_srom_max_5g_low = txpwr = (int8)PHY_GETINTVAR(pi, "pa1lomaxpwr");

#ifdef PPR_API
	pi->ppr.srlgcy.ofdmalpo = offset;
#else
	for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
		pi->tx_srom_max_rate_5g_low[i] = txpwr - ((offset & 0xf) * 2);
		offset >>= 4;
	}
	PHY_TXPWR(("wl: %s: A Band lo channels maxpwr from srom: ", __FUNCTION__));
	for (i = 0; i < TXP_NUM_RATES; i++) {
		PHY_TXPWR(("%d ", pi->tx_srom_max_rate_5g_low[i]));
	}
	PHY_TXPWR(("\n"));
#endif /* PPR_API */

	for (i = 0; i < 3; i++) {
		snprintf(varname, sizeof(varname), "pa1lob%d", i);
		pi->txpa_5g_low[i] = (int16)PHY_GETINTVAR(pi, varname);
	}
	if (wlc_phy_gen_tssi_tables(pi, 'l', pi->txpa_5g_low, itssit, txpwr)) {
		/* pi and other allocated memory will be released in detach time */
		return FALSE;
	}

	/* High channels */
	for (i = 0; i < 3; i++) {
		snprintf(varname, sizeof(varname), "pa1hib%d", i);
		pi->txpa_5g_hi[i] = (int16)PHY_GETINTVAR(pi, varname);
	}

	/* Extract 8 OFDM rates for hi channels */
	offset = (uint32)PHY_GETINTVAR(pi, "ofdmahpo");
	pi->tx_srom_max_5g_hi = txpwr = (int8)PHY_GETINTVAR(pi, "pa1himaxpwr");
	if (wlc_phy_gen_tssi_tables(pi, 'h', pi->txpa_5g_hi, itssit, txpwr)) {
		/* pi and other allocated memory will be released in detach time */
		return FALSE;
	}

#ifdef PPR_API
	pi->ppr.srlgcy.ofdmahpo = offset;
#else
	for (i = TXP_FIRST_OFDM; i <= TXP_LAST_OFDM; i++) {
		pi->tx_srom_max_rate_5g_hi[i] = txpwr - ((offset & 0xf) * 2);
		offset >>= 4;
	}
	PHY_TXPWR(("wl: %s: A Band hi channels maxpwr from srom: ", __FUNCTION__));
	for (i = 0; i < TXP_NUM_RATES; i++)
		PHY_TXPWR(("%d ", pi->tx_srom_max_rate_5g_hi[i]));
	PHY_TXPWR(("\n"));
#endif /* PPR_API */

	return TRUE;
}

#if ACONF
static uint16
wlc_add_iq_comp_delta(uint16 iq_comp_val, uint16 iq_comp_delta)
{
	int i_val, q_val, i_delta, q_delta;

	/* Retrieve I and Q values */
	i_val = (int)((char)iq_comp_val);
	q_val = (int)((char)(iq_comp_val >> 8));
	i_delta = (int)((char)iq_comp_delta);
	q_delta = (int)((char)iq_comp_delta >> 8);

	i_val += i_delta;
	q_val += q_delta;
	if (q_val > 127) q_val = 127;
	if (q_val < -128) q_val = -128;
	if (i_val > 127) i_val = 127;
	if (i_val < -128) i_val = -128;

	return (((q_val & 0xff) << 8) | (i_val & 0xff));
}
#endif /* ACONF */

static void
wlc_aphypwr_set_tssipwr_LUT(phy_info_t *pi, uint chan)
{
	/* Need to rewrite table whenever switching bands of channels */
	int idx;
	int8 *tbl = NULL;
	int range;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;


	range = wlc_get_a_band_range(pi);
	switch (range) {
	case A_LOW_CHANS:
		tbl = pi_abg->l_tssi_to_dbm;
		break;

	case A_MID_CHANS:
		tbl = pi_abg->m_tssi_to_dbm;
		break;

	 case A_HIGH_CHANS:
		tbl = pi_abg->h_tssi_to_dbm;
		break;

	default:
		PHY_ERROR(("%s: Bogus channel %d\n", __FUNCTION__, chan));
		ASSERT(0);
		break;
	}

	if (tbl == NULL)
		return;

	for (idx = 0; idx < 64; idx++) {
		WRITE_APHY_TABLE_ENT(pi, 0x15, idx, tbl[idx]);
	}
}

/*
 * tph_dac_ctrl_override bit[7] 1: overrides hw and passes bit[6:0]
 * as the dac_Ctrl value
 */
static void
wlc_aphypwr_one(phy_info_t *pi, bool enable)
{
	uint16 val;

	val = read_aphy_table_ent(pi, 0xc, 1);
	if (enable == TRUE) {
		val &= 0xff7f;
	}
	else {
		val |= (~0xff7f);
	}

	WRITE_APHY_TABLE_ENT(pi, 0xc, 1, val);
}

static void
wlc_aphypwr_three(phy_info_t *pi)
{
	int i;
	uint16 val;
	for (i = 0; i < 64; i++) {
		val = ((tx_gain_dac_table[i] & 0xf) << 7) |
		        ((tx_gain_rfpa_table[i] & 0x7) << 4) |
		        (tx_gain_bb_table[i] & 0xf);
		WRITE_APHY_TABLE_ENT(pi, 0x16, i, val);
	}
}

/*
 * 16 entries
 * aphy pwr_offset to gain_offset LUT
 * We only utilize 14 entries in the table. The last two are set to zero.
 * Use this table:
 *	 0/20 = 0.000000, pow(10, 0.000000) = 1.000000, * 32 = 32.000000
 *	 1/20 = 0.050000, pow(10, 0.050000) = 1.122018, * 32 = 35.904591
 *	 2/20 = 0.100000, pow(10, 0.100000) = 1.258925, * 32 = 40.285613
 *	 3/20 = 0.150000, pow(10, 0.150000) = 1.412538, * 32 = 45.201201
 *	 4/20 = 0.200000, pow(10, 0.200000) = 1.584893, * 32 = 50.716582
 *	 5/20 = 0.250000, pow(10, 0.250000) = 1.778279, * 32 = 56.904941
 *	 6/20 = 0.300000, pow(10, 0.300000) = 1.995262, * 32 = 63.848394
 */
static void
wlc_aphypwr_four(phy_info_t *pi)
{
	int dig_scalar_list[] = {32, 36, 40, 45, 51, 57, 64};
	int dig_scalar, i;
	uint16 entry, dac_offs, dig_index;

	i = 0;
	for (dig_index = 0; dig_index < sizeof(dig_scalar_list)/sizeof(dig_scalar_list[0]);
	     dig_index++) {
		dig_scalar = dig_scalar_list[dig_index];
		for (dac_offs = 0; dac_offs < 2; dac_offs++) {
			entry = (dac_offs << 7) | dig_scalar;
			WRITE_APHY_TABLE_ENT(pi, 0x17, i++, entry);
		}
	}
	/*  make last two entries same as the 3rd last entry
	 *  as we only need 6dB of range
	 */
	WRITE_APHY_TABLE_ENT(pi, 0x17, i++, entry);
	WRITE_APHY_TABLE_ENT(pi, 0x17, i++, entry);
}

static void
wlc_aphypwr_five(phy_info_t *pi)
{
	int dac_indx, bbpga_indx, table_indx;
	uint16 minset;

	for (dac_indx = 0; dac_indx <= MAX_DAC; dac_indx++) {
		for (bbpga_indx = 2; bbpga_indx <= MAX_BBPGA; bbpga_indx++) {
			minset = wlc_aphypwr_get_dc_bias(pi, dac_indx, bbpga_indx);
			table_indx = (dac_indx * 5) + bbpga_indx;
			WRITE_APHY_TABLE_ENT(pi, 0x18, table_indx, minset);
		}
	}
}

static void
wlc_aphypwr_six(phy_info_t *pi)
{
	if (pi->pubpi.radiorev == 8)
		phy_reg_write(pi, APHY_RSSI_FILT_B0, 0x24);
	else
		phy_reg_write(pi, APHY_RSSI_FILT_B0, 0x0);
}

static void
wlc_aphypwr_seven(phy_info_t *pi)
{
	uint16 vc_type_ivospad, rfpad, rfpga, txmix;
	uint16 bb, rf;

	bb = read_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN);
	rf = read_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN);

	vc_type_ivospad = (bb & 0x60) >> 5;
	txmix = (bb & 0x10) >> 4;
	rfpad = (rf & 0x18) >> 3;
	rfpga = (rf & 0x7);
	phy_reg_write(pi, APHY_RSSI_FILT_B1,
	              (vc_type_ivospad << 6) | (rfpad << 4) | (rfpga << 1) | txmix);
}

static void
wlc_aphypwr_nine(phy_info_t *pi, bool enable)
{
	uint16 reg_val;

	if (AREV_GE(pi->pubpi.phy_rev, 6)) {
		reg_val = 0x900;
	} else {
		if (AREV_IS(pi->pubpi.phy_rev, 5)) {
			reg_val = 0xb00;
		} else {
			reg_val = 0x200;
		}
	}

	if (enable == FALSE) {
		/* bits[11:8] as 1,
		 * bit 11= rotor override,
		 * bit 10= 4wire override,
		 * bit 9 = dc override,
		 * bit 8 = dig_scaler override
		 */
		reg_val = 0xf00;
	}

	phy_reg_write(pi, APHY_RSSI_FILT_A1, reg_val);

}

/* DC_offs LUT */
/* tph_dc_offs_aphy_fname */

static uint16
wlc_aphypwr_get_dc_bias(phy_info_t *pi, int dig, int txbb)
{
	int32 new_bb_dc_bias_factor;
	uint16 orig_bb_dc_bias;
	int32 orig_bb_dc_bias_i, orig_bb_dc_bias_q;
	int32 new_bb_dc_bias_ii, new_bb_dc_bias_i;
	int32 new_bb_dc_bias_qq, new_bb_dc_bias_q;
	int32 new_bb_dc_bias_i8, new_bb_dc_bias_q8;
	uint16 new_bb_dc_bias;
	int32 new_bb_dc_bias_i2, new_bb_dc_bias_if, new_bb_dc_bias_q2;
	int32 new_bb_dc_bias_qf;
	int32 new_bb_dc_bias_i4, new_bb_dc_bias_q4;

	new_bb_dc_bias_factor = bbpga_lut[dig][txbb - 1];

	orig_bb_dc_bias = pi->u.pi_abgphy->global_tx_bb_dc_bias_loft;
	orig_bb_dc_bias_i = wlc_u_to_s(orig_bb_dc_bias >> 8, 8);
	orig_bb_dc_bias_q = wlc_u_to_s(orig_bb_dc_bias & 0xff, 8);

	new_bb_dc_bias_ii = (new_bb_dc_bias_factor * orig_bb_dc_bias_i + 32) >> 6;
	new_bb_dc_bias_i = MIN(new_bb_dc_bias_ii, MAX_BB_TX_DC);
	new_bb_dc_bias_i = MAX(new_bb_dc_bias_i, -MAX_BB_TX_DC);
	new_bb_dc_bias_qq = (new_bb_dc_bias_factor * orig_bb_dc_bias_q + 32) >> 6;
	new_bb_dc_bias_q = MIN(new_bb_dc_bias_qq, MAX_BB_TX_DC);
	new_bb_dc_bias_q = MAX(new_bb_dc_bias_q, -MAX_BB_TX_DC);

	if (AREV_GE(pi->pubpi.phy_rev, 6)) {
		new_bb_dc_bias_i = wlc_s_to_u(new_bb_dc_bias_i, 6);
		new_bb_dc_bias_q = wlc_s_to_u(new_bb_dc_bias_q, 6);
		new_bb_dc_bias   = ((new_bb_dc_bias_i << 6) | new_bb_dc_bias_q) & 0xffff;
	} else {
		new_bb_dc_bias_i2 = (new_bb_dc_bias_i + 1) >> 1;
		new_bb_dc_bias_i4 = MIN(new_bb_dc_bias_i2, 7);
		new_bb_dc_bias_if = MAX(new_bb_dc_bias_i4, -8);

		new_bb_dc_bias_q2 = (new_bb_dc_bias_q + 1) >> 1;
		new_bb_dc_bias_q4 = MIN(new_bb_dc_bias_q2, 7);
		new_bb_dc_bias_qf = MAX(new_bb_dc_bias_q4, -8);

		new_bb_dc_bias_i8 = wlc_s_to_u(new_bb_dc_bias_if, 4);
		new_bb_dc_bias_q8 = wlc_s_to_u(new_bb_dc_bias_qf, 4);
		new_bb_dc_bias   = ((new_bb_dc_bias_i8 << 4) | new_bb_dc_bias_q8) & 0xffff;
	}
	return (new_bb_dc_bias);
}

void
wlc_phy_aci_ctl_gphy(phy_info_t *pi, bool enable)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

#define PEAK_ENERGY_VAL 0x800 /* Peak energy value for PHY */
	if (((phy_reg_read(pi, BPHY_PEAK_ENERGY_LO) == PEAK_ENERGY_VAL)) == enable) {
		return;
	}

	if (enable) {
		PHY_CAL(("%s: Enabling\n", __FUNCTION__));
		pi->aci_state |= ACI_ACTIVE;

		/* Save reg values */
		pi_abg->interference_save.bbconfig =
		        phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_BBCONFIG));
		pi_abg->interference_save.cthr_sthr_shdin =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN));
		if (GREV_IS(pi->pubpi.phy_rev, 1)) {
			pi_abg->interference_save.phycrsth1 =
				phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_PHYCRSTH));
		} else {
			pi_abg->interference_save.phycrsth1 =
			        phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CRSON_THRESH));
			pi_abg->interference_save.phycrsth2 =
			        phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CRSOFF_THRESH));
		}
		pi_abg->interference_save.energy =
			phy_reg_read(pi, BPHY_PEAK_ENERGY_LO);
		pi_abg->interference_save.cckshbits_gnref =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CCK_SHBITS_GNREF));
		pi_abg->interference_save.clip_thresh =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_THRESH));
		pi_abg->interference_save.clip2_thresh =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP2_THRESH));
		pi_abg->interference_save.clip3_thresh =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP3_THRESH));
		pi_abg->interference_save.clip_p1_p2_thresh =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_THRESH));
		pi_abg->interference_save.clip_pwdn_thresh =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_PWDN_THRESH));
		pi_abg->interference_save.p1_p2_gain =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_IDX));
		pi_abg->interference_save.n1_p1_gain =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_P1_IDX));
		pi_abg->interference_save.n1_n2_gain =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_N2_IDX));
		pi_abg->interference_save.threshold =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_N1_N2_THRESH));
		pi_abg->interference_save.div_srch_idx =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_IDX));
		pi_abg->interference_save.div_srch_p1_p2 =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_P1_P2));
		if (GREV_GE(pi->pubpi.phy_rev, 2)) {
			pi_abg->interference_save.div_srch_gn_back =
			        phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_GN_BACK));
			if (GREV_IS(pi->pubpi.phy_rev, 2)) {
				pi_abg->interference_save.div_search_gn_change =
				        phy_reg_read(pi, (GPHY_TO_APHY_OFF +
				                          APHY_DIV_SEARCH_GN_CHANGE));
			} else {
				/* phy_rev >= 3: 4318 & 4320 */
				pi_abg->interference_save.reg15 =
				        phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_DIGI_GAIN1));
				pi_abg->interference_save.reg16 =
				        phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_DIGI_GAIN2));
				pi_abg->interference_save.reg17 =
				        phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_DIGI_GAIN3));

				pi_abg->interference_save.table_7_2 =
				        read_aphy_table_ent(pi, (GPHY_TO_APHY_OFF + 0x7), 0x2);
				pi_abg->interference_save.table_7_3 =
				        read_aphy_table_ent(pi, (GPHY_TO_APHY_OFF + 0x7), 0x3);
			}
		}
		pi_abg->interference_save.ant_dwell =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_ANT_DWELL));
		pi_abg->interference_save.ant_wr_settle =
			phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_ANT_WR_SETTLE));

		/* Enter ACI Mode */

		PHY_REG_LIST_START
			/* Darwin off */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_BBCONFIG, BB_DARWIN, 0)
			/* Set CRS shift_din to 2 from default value of 0 */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN, 3, 2)
			PHY_REG_WRITE_RAW_ENTRY(BPHY_PEAK_ENERGY_LO, PEAK_ENERGY_VAL)
			/* Set CLIP_POW1 clipping threshold to +4 dBm at ADC instead of +1 dBm */
			/* round((10^((4-30)/10) * 50)*(128/0.5)^2) */
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_THRESH, 0x2027)
			/* Set CLIP_POW2 clipping threshold to +3.5 dBm at ADC instead of +1 dBm */
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP2_THRESH, 0x1ca8)
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_PWDN_THRESH, 0x287a)
			/* Set CLIP_NRSSI1_POW1 clipping threshold to  +3.5 dBm at ADC
			 * instead of +1 dBm
			 */
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP3_THRESH, 0x1ca8)
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_THRESH, 0x287a)
			/* Set INIT gain = 60 dB = 13+6+12-1+3*10 */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_N1_P1_IDX,
				0x003f, 0x001a)
			/* Set registers dependent on init-gain change:
			 * Init_gain dropped by 6 dB
			 * Adjust cckshiftbitsgainrefvar down by 6 dB
			 */
			PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CCK_SHBITS_GNREF, 0xd)
		PHY_REG_LIST_EXECUTE(pi);

		if (GREV_IS(pi->pubpi.phy_rev, 1)) {
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_PHYCRSTH),
			              (0xff << 8) | 0xd);
		} else if (GREV_IS(pi->pubpi.phy_rev, 2)) {
			PHY_REG_LIST_START
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CRSON_THRESH,
					0xffff)
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CRSOFF_THRESH,
					0xa9)
			PHY_REG_LIST_EXECUTE(pi);
		} else {
			/* gphy_rev > 2: 4318 & 4320 */
			PHY_REG_LIST_START
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CRSON_THRESH,
					0xc1)
				PHY_REG_WRITE_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CRSOFF_THRESH,
					0x59)
			PHY_REG_LIST_EXECUTE(pi);
		}

		PHY_REG_LIST_START
			/* Set POW1 clip gain  = 54 dB */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_IDX,
				0x3f00, 0x1800)
			/* Set POW2 clip gain  = 45 dB */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_IDX,
				0x003f, 0x0015)
			/* See PRs 9346 and 13601
			 * need to ensure the following:
			 * g_dsss_divs_init_gain      = init_gain
			 * g_dsss_divs_clip_pow1_gain = clip_pow1_gain
			 * g_dsss_divs_clip_pow2_gain = clip_pow2_gain
			 * Set g_dsss_divs_init_gain  = init_gain = 60 dB
			 */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_IDX,
				0x3000, 0x1000)
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_IDX,
				0x0f00, 0x0a00)
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_P1_P2,
				0x3000, 0x1000)
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_P1_P2,
				0x0f00, 0x0800)
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_P1_P2,
				0x0030, 0x0010)
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_P1_P2,
				0x000f, 0x0005)
			/* Set ofdm diversity search gain to have
			 * LNA = 13 dB instead of 25 dB
			 * Drop ofdm div search gain to 48 dB
			 */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_IDX,
				0x0030, 0x10)
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_IDX,
				0x000f, 0x6)
			/* Set NRSSI1 clip gain  = 39 dB */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_N1_N2_IDX,
				0x0f00, 0x0800)
			/* Set NRSSI1_POW1 clip gain  = 30 dB */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_N1_P1_IDX,
				0x0f00, 0x0500)
			/* Set NRSSI2 clip gain = 48 dB */
			PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CLIP_N1_N2_IDX,
				0x000f, 0x000b)
		PHY_REG_LIST_EXECUTE(pi);

		if (GREV_LE(pi->pubpi.phy_rev, 2)) {
			/* Force shift bits
			 * This register will be appropriately overridden by the Ucode
			 */
			PHY_REG_LIST_START
				PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_N1_N2_THRESH, 0x1000)
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_N1_N2_THRESH,
					0x6000, 0x2000)
			PHY_REG_LIST_EXECUTE(pi);

			/* Activate Ucode WAR to force shift bits */
			wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_ACIWAR, MHF1_ACIWAR,
				WLC_BAND_2G);
		} else {
			/* Relieve microcode from the ACI WAR from phy_revid >=3.
			 * Instead use GPHY to do the ACI WAR using
			 * phyregs 0x415, 0x416, and 0x417 to force shift-bits
			 * Shift_bits forced to 1 in the following OFDM RX CRS states:
			 * reset, search, P1, P2, N1, N1P1, N2, gain-settle,
			 * wait_ed_drop and wait_nclk20s
			 * shift_bits forced to 2 in the following OFDM RX CRS states:
			 * cckdivsearch, cckwaitforWRSSI, crs_dssscck_phy
			 */

			/* Ensure that the global digigain override is set to 0 */
			PHY_REG_LIST_START
				PHY_REG_AND_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_N1_N2_THRESH,
					~0x1000)
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIGI_GAIN1,
					0x7fff, 0x36db)
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIGI_GAIN2,
					0x7fff, 0x36db)
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIGI_GAIN3,
					0x01ff, 0x016d)
			PHY_REG_LIST_EXECUTE(pi);
		}

		/* Set crs_ed_disable if 4306C0 and higher */
		if (GREV_GE(pi->pubpi.phy_rev, 2))
			phy_reg_or(pi, (GPHY_TO_APHY_OFF + APHY_ANT_DWELL), 0x800);

		/* Set wrssi settle time */
		phy_reg_mod(pi, (GPHY_TO_APHY_OFF + APHY_ANT_WR_SETTLE), 0x0f00, 0x200);

		if (GREV_IS(pi->pubpi.phy_rev, 2)) {
			PHY_REG_LIST_START
				/* Set smart diversity LNA gain change to a very high value
				* in effect forcing LNA = 19 dB for the second antenna
				*/
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_GN_CHANGE,
					0x00ff, 0x7f)
				/* Limit LNA min gain to 19 dB */
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_GN_BACK,
					0xff00, 0x1300)
			PHY_REG_LIST_EXECUTE(pi);
		} else if (GREV_GE(pi->pubpi.phy_rev, 6)) {
			/* Change smart diversity LNA gain change table to allow
			 * only LNA <= 13 dB gain settings
			 */
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x7), 0x3, 0x7f);
			WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x7), 0x2, 0x7f);

			/* Limit min divsearch gain to 0 dB */
			phy_reg_mod(pi, (GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_GN_BACK), 0xff00, 0);
		}

		/* Compute N1 and N2 clip thresholds */
		wlc_cal_2050_nrssislope_gmode1(pi);
		PHY_CAL(("%s: slope_scale = %d  minRSSI = %d  maxRSSI = %d\n",
			__FUNCTION__, pi_abg->abgphy_cal.nrssi_slope_scale,
			pi_abg->abgphy_cal.min_rssi,
			pi_abg->abgphy_cal.max_rssi));
		PHY_CAL(("%s: aphy Addr/Val: 0x48a 0x%x\n", __FUNCTION__,
			phy_reg_read(pi, 0x48a)));

	} else {

		PHY_CAL(("%s: Disabling\n", __FUNCTION__));

		pi->aci_state &= ~ACI_ACTIVE;

		/* restore regs */
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_BBCONFIG),
			pi_abg->interference_save.bbconfig);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN),
			pi_abg->interference_save.cthr_sthr_shdin);
		phy_reg_write(pi, BPHY_PEAK_ENERGY_LO, pi_abg->interference_save.energy);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_THRESH),
			pi_abg->interference_save.clip_thresh);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP2_THRESH),
			pi_abg->interference_save.clip2_thresh);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_PWDN_THRESH),
			pi_abg->interference_save.clip_pwdn_thresh);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP3_THRESH),
			pi_abg->interference_save.clip3_thresh);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_THRESH),
			pi_abg->interference_save.clip_p1_p2_thresh);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_P1_IDX),
			pi_abg->interference_save.n1_p1_gain);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CCK_SHBITS_GNREF),
			pi_abg->interference_save.cckshbits_gnref);
		if (GREV_IS(pi->pubpi.phy_rev, 1)) {
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_PHYCRSTH),
				pi_abg->interference_save.phycrsth1);
		} else {
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CRSON_THRESH),
				pi_abg->interference_save.phycrsth1);
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CRSOFF_THRESH),
				pi_abg->interference_save.phycrsth2);
		}
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_P1_P2_IDX),
			pi_abg->interference_save.p1_p2_gain);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_P1_P2),
			pi_abg->interference_save.div_srch_p1_p2);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_IDX),
			pi_abg->interference_save.div_srch_idx);
		if (GREV_GE(pi->pubpi.phy_rev, 2)) {
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_GN_BACK),
				pi_abg->interference_save.div_srch_gn_back);
			if (GREV_IS(pi->pubpi.phy_rev, 2)) {
				phy_reg_write(pi,
					(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_GN_CHANGE),
					pi_abg->interference_save.div_search_gn_change);
			} else {
				phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_DIGI_GAIN1),
					pi_abg->interference_save.reg15);
				phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_DIGI_GAIN2),
					pi_abg->interference_save.reg16);
				phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_DIGI_GAIN3),
					pi_abg->interference_save.reg17);

				WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x7), 0x2,
					pi_abg->interference_save.table_7_2);
				WRITE_APHY_TABLE_ENT(pi, (GPHY_TO_APHY_OFF + 0x7), 0x3,
					pi_abg->interference_save.table_7_3);
			}
		}

		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CLIP_N1_N2_IDX),
			pi_abg->interference_save.n1_n2_gain);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_N1_N2_THRESH),
			pi_abg->interference_save.threshold);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_ANT_DWELL),
			pi_abg->interference_save.ant_dwell);
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_ANT_WR_SETTLE),
			pi_abg->interference_save.ant_wr_settle);

		/* Deactivate Ucode WAR to force shift bits */
		wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_ACIWAR, 0, WLC_BAND_AUTO);

		/* Compute N1 and N2 clip thresholds */
		wlc_cal_2050_nrssislope_gmode1(pi);

		PHY_CAL(("%s: slope_scale = %d  minRSSI = %d  maxRSSI = %d\n",
			__FUNCTION__, pi_abg->abgphy_cal.nrssi_slope_scale,
			pi_abg->abgphy_cal.min_rssi,
			pi_abg->abgphy_cal.max_rssi));
	}
}

static void
bgphy_set_bbattn(phy_info_t *pi, int bbatten)
{
	uint16	mask = (pi->pubpi.ana_rev == ANA_11G_018) ?
		BBATT_11G_MASK : BBATT_11G_MASK2;
	int	shift = (pi->pubpi.ana_rev == ANA_11G_018) ?
		BBATT_11G_SHIFT : BBATT_11G_SHIFT2;

	phy_reg_mod(pi, BPHY_DAC_CONTROL, mask, bbatten << shift);
}

static int
bgphy_get_bbattn(phy_info_t *pi)
{
	if (pi->pubpi.ana_rev == ANA_11G_018)
		return ((phy_reg_read(pi, BPHY_DAC_CONTROL) >> BBATT_11G_SHIFT) & 0xf);
	else
		return ((phy_reg_read(pi, BPHY_DAC_CONTROL) >> BBATT_11G_SHIFT2) & 0xf);
}

int8
wlc_phy_noise_sample_aphy_meas(phy_info_t *pi)
{
	uint16 oldrx_sp_reg1, oldpwrdwn = 0, oldtssi_temp_ctl = 0, oldrssi_adc_ctl = 0;
	uint16 temp = 0;
	uint i, j, phy_rev = pi->pubpi.phy_rev;
	int32 acc, t;
	int rssi, dbm;

	ASSERT(ISAPHY(pi));

	wlc_phyreg_enter((wlc_phy_t*)pi);
	wlc_radioreg_enter((wlc_phy_t*)pi);

	oldrx_sp_reg1 = read_radio_reg(pi, RADIO_2060WW_RX_SP_REG1);

	/* clear NB direct_out bit (use NRSSI, not direct out) */
	and_radio_reg(pi, RADIO_2060WW_RX_SP_REG1, ~0x10);

	/* aphy_meas_nrssi: */

	oldpwrdwn = phy_reg_read(pi, APHY_PWRDWN);

	if (AREV_IS(phy_rev, 2)) {
		oldrssi_adc_ctl = phy_reg_read(pi, APHY_RSSI_ADC_CTL);

		PHY_REG_LIST_START
			/* force nrssi (01) into rssi mux (instead of temp sense)
			* and enable rssi mux override
			*/
			PHY_REG_MOD_RAW_ENTRY(APHY_PWRDWN, 7, 5)
			/* don't wait 1us between each of the phy's 4 nrssi looks */
			PHY_REG_AND_RAW_ENTRY(APHY_RSSI_ADC_CTL, ~0xf00)
		PHY_REG_LIST_EXECUTE(pi);

		oldtssi_temp_ctl = phy_reg_read(pi, APHY_TSSI_TEMP_CTL);
	}

	for (acc = 0, i = 0; i < NUM_SNAPS; i++) {
		if (AREV_IS(phy_rev, 2)) {
			phy_reg_write(pi, APHY_TSSI_TEMP_CTL, oldtssi_temp_ctl | 1);
			for (j = 0;
			     ((j < TEMP_WAIT_MAX) &&
			      (((temp = phy_reg_read(pi, APHY_TEMP_STAT)) & 0x0100) == 0));
			     j++)
				OSL_DELAY(10);

			if (j < TEMP_WAIT_MAX) {
				temp &= 0xff;
			} else {
				PHY_CAL(("wl%d: %s: Failed to read temp reg after %d uS.\n",
				          pi->sh->unit, __FUNCTION__, i * TEMP_WAIT_MAX));
				temp = 0;
			}

			phy_reg_write(pi, APHY_TSSI_TEMP_CTL, oldtssi_temp_ctl);
		} else {
			PHY_REG_LIST_START
				PHY_REG_MOD_RAW_ENTRY(APHY_PWRDWN, 4, 4)
				PHY_REG_MOD_RAW_ENTRY(APHY_PWRDWN, 3, 1)
			PHY_REG_LIST_EXECUTE(pi);
			temp = phy_reg_read(pi, APHY_WRSSI_NRSSI) & 0xff;
		}

		/* Sign extend */
		t = temp << 24;
		t >>= 24;

		if (AREV_GE(phy_rev, 3))
			t *= 4;

		PHY_CAL(("wl%d: %s: nrssi = %d\n", pi->sh->unit, __FUNCTION__, t));

		acc += t;
	}

	phy_reg_write(pi, APHY_PWRDWN, oldpwrdwn);
	if (AREV_IS(phy_rev, 2)) {
		phy_reg_write(pi, APHY_RSSI_ADC_CTL, oldrssi_adc_ctl);
	}

	/* restore NB direct_out bit to previous value */
	write_radio_reg(pi, RADIO_2060WW_RX_SP_REG1, oldrx_sp_reg1);

	/* Undo safeguards */
	wlc_radioreg_exit((wlc_phy_t*)pi);
	wlc_phyreg_exit((wlc_phy_t*)pi);

	/* Return rounded average */
	t = (acc + (NUM_SNAPS / 2)) / NUM_SNAPS;

	PHY_CAL(("wl%d: %s: acum nrssi = %d, ave = %d\n", pi->sh->unit, __FUNCTION__, acc, t));

	rssi = ((acc + (NUM_SNAPS / 2)) / NUM_SNAPS);
	/* MULT_FACT = -5 Multiplication factor
	 * MULT_SHIFT =	4 # of bits
	 * MULT_ROUND =	((1 << MULT_SHIFT) >> 1)
	 * RSSI_OFFSET	-67 Offset for RSSI
	 */
	dbm = (((rssi * (-5)) + ((1 << 4) >> 1)) >> 4) + (-67);
	/* limit to [-90, -25] (range of detectable noise power) */
	if (dbm > -25)
		dbm = -25;
	else if (dbm < -90)
		dbm = -90;

	return (int8)dbm;
}

int8
wlc_phy_noise_sample_gphy(phy_info_t *pi)
{
	uint16 val;
	int phy_noise;

	val = wlapi_bmac_read_shm(pi->sh->physhim, M_PHY_NOISE);
	phy_noise = val & PHY_NOISE_MASK;

	if (BOARDFLAGS(GENERIC_PHY_INFO(pi)->boardflags) & BFL_ADCDIV) {
		/* Correct for the nrssi slope */
		if (phy_noise >= PHY_RSSI_TABLE_SIZE)
			phy_noise = PHY_RSSI_TABLE_SIZE - 1;
		phy_noise = nrssi_tbl[phy_noise];
		phy_noise = (-131 * (31 - phy_noise) >> 7) - 67;
	} else {
		phy_noise = (-149 * (31 - phy_noise) >> 7) - 68;
	}
	if (phy_noise > -35)
		phy_noise += 15;

	return (int8)phy_noise;
}

int8
wlc_jssi_to_rssi_dbm_abgphy(phy_info_t *pi, int crs_state, int *jssi, int jssi_count)
{
	int i;
	int noise_dbm = 0;
	int rssi_avg;
	int lna_gain[] = {-2, 19, 14, 25};
	int tr_bit;
	int lna_bits;

	if (ISAPHY(pi)) {
		/* aphy parameters for converting RSSI to received power in dBm */
		int mult_fact = -5;
		int mult_shift = 4;
		int mult_round = ((1 << mult_shift) >> 1);
		int offset = -62;
		noise_dbm = ((jssi[0] * mult_fact + mult_round) >> mult_shift) + offset;
	} else {
		rssi_avg = 0;

		for (i = 0; i < jssi_count; i++) {
			if (jssi[i] < 0)
				continue;
			else if (jssi[i] >= PHY_RSSI_TABLE_SIZE)
				rssi_avg += nrssi_tbl[PHY_RSSI_TABLE_SIZE-1];
			else
				rssi_avg += nrssi_tbl[jssi[i]];
		}
		rssi_avg = (rssi_avg + jssi_count/2) / jssi_count;

		tr_bit = 0;
		lna_bits = 3;
		if (crs_state == APHY_CRS_G_CLIP_NRSSI2)
			tr_bit = 1;
		if ((crs_state == APHY_CRS_G_CLIP_NRSSI1_POW1) ||
		    (crs_state == APHY_CRS_G_CLIP_NRSSI1) ||
		    (crs_state == APHY_CRS_G_CLIP_NRSSI2))
			lna_bits = 0;

		noise_dbm = ((125 * rssi_avg + 64) >> 7) - 43 - (2 + lna_gain[lna_bits])
			+ (tr_bit ? 24 : 0) - 27;
	}

	return (int8)noise_dbm;
}

void
wlc_phy_chanspec_set_abgphy(phy_info_t *pi, chanspec_t chanspec)
{
	uint i;
	uint16 old_radar = 0;
	uint16 radioreg_5c = 0;
	d11regs_t *regs = pi->regs;
	const chan_info_abgphy_t *ci;
	uint8 channel = CHSPEC_CHANNEL(chanspec);

	/* lookup radio-chip-specific channel code */
	for (i = 0; i < ARRAYSIZE(chan_info_abgphy); i++)
		if (chan_info_abgphy[i].chan == channel)
			break;

	if (i >= ARRAYSIZE(chan_info_abgphy)) {
		PHY_ERROR(("wl%d: %s: channel %d not found in channel table\n",
		          pi->sh->unit, __FUNCTION__, channel));
		ASSERT(i < ARRAYSIZE(chan_info_abgphy));
		return;
	}

	ci = &chan_info_abgphy[i];

	wlc_phy_chanspec_radio_set((wlc_phy_t *)pi, chanspec);


	if (NORADIO_ENAB(pi->pubpi))
		return;

	/* Disable radar detection during the channel change and clear out FIFO */
	if (pi->sh->radar && (ISAPHY(pi)) && (AREV_GT(pi->pubpi.phy_rev, 2))) {
		old_radar = phy_reg_read(pi, APHY_RADAR_BLANK_CTL);
		phy_reg_write(pi, APHY_RADAR_BLANK_CTL, old_radar & ((uint16)(~(1 << 15))));
	}

	if (pi->pubpi.radioid == BCM2060_ID) {
#if ACONF
		uint fc;
		uint16 vv, rxiq, txiq, v = 0;
		uint16 pll_errthr_val, pll_calovr_val;

		ASSERT(ISAPHY(pi));
		/* Write phychannel, but save and restore the radio reg it writes to */
		if (AREV_IS(pi->pubpi.phy_rev, 2))
			v = read_radio_reg(pi, RADIO_2060WW_RX_MXCMVFC);
		write_phy_channel_reg(pi, channel);
		if (AREV_IS(pi->pubpi.phy_rev, 2))
			write_radio_reg(pi, RADIO_2060WW_RX_MXCMVFC, v);
		write_radio_reg(pi, RADIO_2060WW_CCCP_PSCTL, 0x88);
		write_radio_reg(pi, RADIO_2060WW_PLL_REFDIV, ci->pdiv);
		mod_radio_reg(pi, RADIO_2060WW_PLL_SD_CTL, 0xff7f, ci->sdiv);

		/* 2060_freq_tuning: */

		/* linearly interpolate frequency tuning value
		 *	using endpts:	4920 MHz -> 15
		 *			5500 MHz ->  0
		 */
		fc = ci->freq;
		if (fc < 4920)
			v = 0xf;
		else if (fc >= 5500)
			v = 0;
		else
			v = (15 * (5500 - fc)) / 580;
		v &= 0xf;
		vv = (v << 4) | v;

		write_radio_reg(pi, RADIO_2060WW_RX_VFC_LNA, vv);
		write_radio_reg(pi, RADIO_2060WW_TX_PAPADTN, vv);
		write_radio_reg(pi, RADIO_2060WW_TX_LOMIXTN, vv);
		mod_radio_reg(pi, RADIO_2060WW_TX_RFPGATN, 0xfff0, (v << 4));
		write_radio_reg(pi, RADIO_2060WW_VFC_VCOTAL, vv);
		write_radio_reg(pi, RADIO_2060WW_VFC_LGNMIX, vv);

		mod_radio_reg(pi, RADIO_2060WW_RX_MXCMVFC, 0xff0f, v);

		/* End of 2060_freq_tuning */

#ifdef MANUALCAL
		/* 2060ww_manual_pll_cal: */

		/* set based on mean values in band
		 * of freq_setting.xls (seema, 10/31/02)
		 */
		write_radio_reg(pi, RADIO_2060WW_RST_PLLCAL, 0x80);
		write_radio_reg(pi, RADIO_2060WW_PLL_CALOVR, ci->cal_val);

		/* End of 2060ww_manual_pll_cal */
#else
		/* 2060ww_vco_cal: */

		/* set default values */
		pll_errthr_val = 0xaa;
		pll_calovr_val = 0x40;

		if ((fc >= 4920) && (fc <= 5080)) {
			pll_errthr_val = 0xaa;
			pll_calovr_val = 0x40;
		} else if ((fc >= 5170) && (fc <= 5320)) {
			pll_errthr_val = 0xaa;
			pll_calovr_val = 0x00;
		} else if ((fc >= 5500) && (fc <= 5700)) {
			pll_errthr_val = 0xaa;
			pll_calovr_val = 0x80;
		} else if ((fc >= 5745) && (fc <= 5825)) {
			/* set this to "1" (high current [2mA]) for now also */
			/* pll_errthr_val = 0xa2; */
			pll_errthr_val = 0xaa;
			pll_calovr_val = 0x80;
		}

		/* set PLL calibration timeout (top nibble) */
		mod_radio_reg(pi, RADIO_2060WW_PLL_TMOUT, 0xf0, 0xb0);

		/* set V error level */
		write_radio_reg(pi, RADIO_2060WW_PLL_ERRTHR, pll_errthr_val);

		/* set V threshold */
		write_radio_reg(pi, RADIO_2060WW_PLL_CALTHR, 0x85);

		/* set cal sequence init value */
		mod_radio_reg(pi, RADIO_2060WW_PLL_CALOVR, 0xdf, pll_calovr_val);

		/* set bin search mode; clock for pll on */
		mod_radio_reg(pi, RADIO_2060WW_PLL_SP_REG3, 0x38, 0x0);

		/* turn off PLL reset_n */
		mod_radio_reg(pi, RADIO_2060WW_RST_PLLCAL, 0x80, 0x80);

		/* issue posedge on cal_start bit */
		mod_radio_reg(pi, RADIO_2060WW_PLL_ERRTHR, 0x10, 0x00);
		mod_radio_reg(pi, RADIO_2060WW_PLL_ERRTHR, 0x10, 0x10);

		/* get calibration value */
		PHY_CAL(("wl%d: %s: 2060 vco cal value = 0x%x\n", pi->sh->unit, __FUNCTION__,
		           (read_radio_reg(pi, RADIO_2060WW_PLL_CTLCODE) >> 2) & 0xf));

		/* End 2060ww_vco_cal: */
#endif /* MANUALCAL */

		aphy_set_tx_iq_based_on_vos(pi, 1);

		/* set appropriate Rx IQ rebalancing coeffs */

		rxiq = rxiqcomp[ci->rxiqidx];
		txiq = txiqcomp[ci->txiqidx];

		phy_reg_write(pi, APHY_RX_COMP_COEFF, rxiq);
		phy_reg_write(pi, APHY_TX_COMP_COEFF,
		              wlc_add_iq_comp_delta(phy_reg_read(pi, APHY_TX_COMP_COEFF),
		                                    txiq));

		PHY_CAL(("wl%d: %s: TX IQ rebalance (phyreg 0x69) = 0x%x\n",
		           pi->sh->unit, __FUNCTION__,
		           phy_reg_read(pi, APHY_TX_COMP_COEFF)));

		if (pi->u.pi_abgphy->m_tssi_to_dbm == NULL &&
			pi->u.pi_abgphy->a_tssi_to_dbm == NULL) {
			if (fc <= 5500) {
				write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, 0x26);
				mod_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, 0xe0, 0x20);
			} else if (fc <= 5600) {
				write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, 0x26);
				mod_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, 0xe0, 0);
			} else if (fc <= 5700) {
				write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, 0x25);
				mod_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, 0xe0, 0);
			} else {
				write_radio_reg(pi, RADIO_2060WW_TX_BB_GAIN, 0x24);
				mod_radio_reg(pi, RADIO_2060WW_TX_RF_GAIN, 0xe0, 0);
			}
		} else {
			/* Set the power to the last value calculated by power control */
			if ((pi->txpwridx != -1) && !pi->hwpwrctrl)
				wlc_set_11a_txpower(pi, pi->txpwridx, FALSE);
		}

		/* Reload power table */
		if (pi->hwpwrctrl)
			wlc_aphypwr_set_tssipwr_LUT(pi, channel);

		PHY_CAL(("wl%d: %s(ww): ft val %d, pdiv %d, sdiv %d, cal_val 0x%x, rxiq 0x%x,"
			" txiq 0x%x\n", pi->sh->unit, __FUNCTION__, v,
			chan_info_abgphy[i].pdiv, chan_info_abgphy[i].sdiv,
			chan_info_abgphy[i].cal_val, rxiq, txiq));
#endif /* ACONF */
	} else {	/* 2050 Radio */
		/* no need to delay VCO cal on channel changes */
		/* delay required only when powering up the VCO */
		if (pi->pubpi.radiorev == 8) {
			radioreg_5c = read_radio_reg(pi, RADIO_2050_PLL_CTL2);

			/* reduce vco_cal delay to 5 to keep channel
			 * change time within 2ms. vco_cal delay of 0xf is not required
			 * when vco has been settled for long
			 */
			write_radio_reg(pi, RADIO_2050_PLL_CTL2, (0xF0 & radioreg_5c) | 0x05);
		}

		/* 205x radios */
		write_phy_channel_reg(pi, chan_info_abgphy[i].radiocode);

		wlapi_bmac_write_shm(pi->sh->physhim, M_CUR_2050_RADIOCODE,
			chan_info_abgphy[i].radiocode);

		/* Japan Channel 14 restrictions */
		if (ISGPHY(pi)) {
			wlapi_bmac_mhf(pi->sh->physhim, MHF1, MHF1_ACPRWAR,
				((pi->u.pi_abgphy->channel_14_wide_filter &&
				(channel == 14)) ? 0 : MHF1_ACPRWAR), WLC_BAND_2G);
		}
		if (channel == 14) {
			OR_REG(pi->sh->osh, &regs->phytest, 0x800);

			or_radio_reg(pi, RADIO_2050_TX_CTL0, 0x04);
		} else {
			AND_REG(pi->sh->osh, &regs->phytest, ~0x840);
		}
	}

	if (pi->sh->radar && (ISAPHY(pi)) && (AREV_GT(pi->pubpi.phy_rev, 2))) {
		phy_reg_write(pi, APHY_RADAR_BLANK_CTL, old_radar);
	}

	OSL_DELAY(2000);
	if ((pi->pubpi.radioid == BCM2050_ID) && (pi->pubpi.radiorev == 8))
		write_radio_reg(pi, RADIO_2050_PLL_CTL2, radioreg_5c);

}

void
wlc_synth_pu_war(phy_info_t *pi, uint channel)
{
	uint i;
	uint tmp_chan = (channel > 10) ? 1 : channel + 4;

	/* this WAR only applies to 2050 rev <=5 */
	if ((pi->pubpi.radioid != BCM2050_ID) || (pi->pubpi.radiorev > 5))
		return;

	for (i = 0; i < ARRAYSIZE(chan_info_abgphy); i++)
		if (chan_info_abgphy[i].chan == tmp_chan)
			break;
	ASSERT(i < ARRAYSIZE(chan_info_abgphy));
	write_phy_channel_reg(pi, chan_info_abgphy[i].radiocode);
	OSL_DELAY(250);
	for (i = 0; i < ARRAYSIZE(chan_info_abgphy); i++)
		if (chan_info_abgphy[i].chan == channel)
			break;
	ASSERT(i < ARRAYSIZE(chan_info_abgphy));
	write_phy_channel_reg(pi, chan_info_abgphy[i].radiocode);
	OSL_DELAY(250);
}

void
wlc_phy_cal_init_gphy(phy_info_t *pi)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;
	uint8 radiorev = pi->pubpi.radiorev;

	pi_abg->radiopwr = wlc_default_radiopwr_gphy(pi);
	pi_abg->bb_atten = DEF_TX_ATTN;

	/* set PWR_CTL */
	if (radiorev == 1)
		pi_abg->txctl1 = TXC1_5DB;
	else if (radiorev == 8) {
		/* Not used to set power, but set so
		 * complo can test it.
		 */
		pi_abg->txctl1 = 0x1;
	} else
		pi_abg->txctl1 = 0;

	pi->interf.aci.exit_thresh = 200;
	pi->interf.aci.enter_thresh = 1000;
	pi->interf.aci.usec_spintime = 10;
	pi->interf.aci.glitch_delay = 20;
	pi->interf.aci.countdown = 0;

	/* For gphy revs 0 & 1, the rxfilt calibration for 2050 needs
	 * to be done with gmode == 0, so we do a chipinit
	 * here with gmode off and then again in the normal place.
	 */
	if (GREV_IS(pi->pubpi.phy_rev, 1)) {
		ASSERT(pi_abg->sbtml_gm);
		pi_abg->sbtml_gm = FALSE;
		wlapi_bmac_corereset(pi->sh->physhim, 0);
		wlc_phy_init_gphy(pi);
		pi_abg->sbtml_gm = TRUE;
		wlapi_bmac_corereset(pi->sh->physhim, SICF_GMODE);
	}
}

/* new function after split */
void
BCMATTACHFN(wlc_phy_detach_abgphy)(phy_info_t *pi)
{

	if (pi->u.pi_abgphy)
	{
		if (pi->u.pi_abgphy->a_tssi_to_dbm)
			MFREE(pi->sh->osh, pi->u.pi_abgphy->a_tssi_to_dbm, APHY_TSSI_TABLE_SIZE);
		if (pi->u.pi_abgphy->l_tssi_to_dbm)
			MFREE(pi->sh->osh, pi->u.pi_abgphy->l_tssi_to_dbm, APHY_TSSI_TABLE_SIZE);
		if (pi->u.pi_abgphy->m_tssi_to_dbm)
			MFREE(pi->sh->osh, pi->u.pi_abgphy->m_tssi_to_dbm, APHY_TSSI_TABLE_SIZE);
		if (pi->u.pi_abgphy->h_tssi_to_dbm)
			MFREE(pi->sh->osh, pi->u.pi_abgphy->h_tssi_to_dbm, APHY_TSSI_TABLE_SIZE);
		if (pi->hwtxpwr)
			MFREE(pi->sh->osh, pi->hwtxpwr, sizeof(uint8) *
			ARRAYSIZE(chan_info_abgphy));
		MFREE(pi->sh->osh, pi->u.pi_abgphy, sizeof(phy_info_abgphy_t));
	}
}

bool
wlc_phy_cal_txpower_recalc_sw_abgphy(phy_info_t *pi)
{
	bool ret = TRUE;

	switch (pi->pubpi.phy_type) {
	case PHY_TYPE_A:
		CASECHECK(PHYTYPE, PHY_TYPE_A);
		if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
		    ((pi->sh->boardtype == BU4306_BOARD) ||
		     (pi->sh->boardtype == BU4309_BOARD))) {
			/* Bringup boards do NOT have a power detector */
			return (ret);
		}

		/* Do nothing if we don't have paparms or idle_tssi */
		if ((pi->u.pi_abgphy->m_tssi_to_dbm == NULL &&
			pi->u.pi_abgphy->a_tssi_to_dbm == NULL) ||
		    (pi->u.pi_abgphy->idle_tssi[A_MID_CHANS] == 0))
			return (ret);

		/* Just compute the est. Pout to update current power */
		ret = wlc_txpower_11a_compute_est_Pout(pi);

		/* Do nothing if radio pwr is being overridden */
		if (pi->radiopwr_override != RADIOPWR_OVERRIDE_DEF)
			return (ret);

		/* Recalc only if compute_est_Pout was successful */
		if (ret)
			ret = wlc_txpower_11a_recalc(pi);
		break;

	case PHY_TYPE_G:
		CASECHECK(PHYTYPE, PHY_TYPE_G);
		if ((pi->sh->boardvendor == VENDOR_BROADCOM) &&
		    (pi->sh->boardtype == BU4306_BOARD)) {
			/* Bringup boards do NOT have a power detector */
			return (ret);
		}

		if (pi->u.pi_abgphy->idle_tssi[G_ALL_CHANS] == 0)
			return (ret);

		/* Just compute the est. Pout to update current power */
		ret = wlc_txpower_11b_compute_est_Pout(pi);

		/* Do nothing if radio pwr is being overridden */
		if (pi->radiopwr_override != RADIOPWR_OVERRIDE_DEF)
			return (ret);

		/* Recalc only if compute_est_Pout was successful */
		if (ret)
			ret = wlc_txpower_11b_recalc(pi);
		break;
	}

	wlc_phy_txpower_update_shm(pi);
	return ret;
}

void
wlc_phy_switch_radio_abgphy(phy_info_t *pi, bool on)
{
	if (ISAPHY(pi)) {
		if (on) {
			write_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, 0xc0);
			write_radio_reg(pi, RADIO_2060WW_PWR_STACTL, 0x08);
			PHY_REG_LIST_START
				PHY_REG_AND_RAW_ENTRY(APHY_RF_OVERRIDE, ~0x8)
				PHY_REG_AND_RAW_ENTRY(APHY_RF_OVERRIDE_VAL, ~0x8)
			PHY_REG_LIST_EXECUTE(pi);
			wlc_radio_2060_init(pi);
		} else {
			write_radio_reg(pi, RADIO_2060WW_PWR_DYNCTL, 0xff);
			write_radio_reg(pi, RADIO_2060WW_PWR_STACTL, 0xfb);
			PHY_REG_LIST_START
				PHY_REG_OR_RAW_ENTRY(APHY_RF_OVERRIDE, 8)
				PHY_REG_OR_RAW_ENTRY(APHY_RF_OVERRIDE_VAL, 8)
			PHY_REG_LIST_EXECUTE(pi);
		}

	} else {	/* GPHY */
		if (on) {
			if (D11REV_IS(pi->sh->corerev, 4)) {
				PHY_REG_LIST_START
					PHY_REG_WRITE_RAW_ENTRY(BPHY_RF_OVERRIDE, 0x0800)
					PHY_REG_WRITE_RAW_ENTRY(BPHY_RF_OVERRIDE, 0x0c00)
				PHY_REG_LIST_EXECUTE(pi);
				OSL_DELAY(250);
				PHY_REG_LIST_START
					PHY_REG_WRITE_RAW_ENTRY(BPHY_RF_OVERRIDE, 0x8000)
					PHY_REG_WRITE_RAW_ENTRY(BPHY_RF_OVERRIDE, 0xcc00)
				PHY_REG_LIST_EXECUTE(pi);
				if (pi->u.pi_abgphy->sbtml_gm) {
					/* Use filter control override */
					phy_reg_write(pi, BPHY_RF_OVERRIDE,
					              (RFO_FLTR_RX_CTRL_OVR |
					               RFO_FLTR_RX_CTRL_VAL));
				} else {
					phy_reg_write(pi, BPHY_RF_OVERRIDE, 0);
				}
			} else {
				/* Use the gphy overrides */
				phy_reg_write(pi, GPHY_RF_OVERRIDE, 0);

				/* There is a settling time that needs to be
				 * respected after turning the radio on, but
				 * since we call wlc_set_channel below, and
				 * it has a delay after setting the channel;
				 * that is enough.
				 */
			}
			/*
			 * The VCO settling time for 2050A1 is 800us with a 0.1us cap on
			 * biassvcovar. So, we need to have a delay of 600us before forcing
			 * vco cal. wlc_synth_pu_war itself implements a delay of 250us, so
			 * implement
			 * the rest of 550us delay before calling wlc_synth_pu_war
			 */
			OSL_DELAY(550);
			/* The phy seems to do this for us automatically;
			 * but Jason thinks we should be explicit about it
			 */
			wlc_synth_pu_war(pi, CHSPEC_CHANNEL(pi->radio_chanspec));
			wlc_phy_chanspec_set((wlc_phy_t*)pi, pi->radio_chanspec);
		} else {
			if (D11REV_IS(pi->sh->corerev, 4)) {
				/* shut off rx and tx path, pa, adn freq synthesizer */
				phy_reg_write(pi, BPHY_RF_OVERRIDE, 0xaa00);
			} else {
				/* rx_pu, tx_pu, synth_pu overrides */
				PHY_REG_LIST_START
					PHY_REG_OR_RAW_ENTRY(GPHY_RF_OVERRIDE, 0x008c)
					PHY_REG_AND_RAW_ENTRY(GPHY_RF_OVERRIDE_VAL, ~0x008c)
				PHY_REG_LIST_EXECUTE(pi);
			}
		}
	}
}

void
wlc_phy_txpower_get_instant_abgphy(phy_info_t *pi, void *pwr)
{
	uint16 tssi;
	tx_inst_power_t *power = (tx_inst_power_t *)pwr;
	if (!pi->hwpwrctrl) {
		/* Read instantaneous TSSI w/o clearing/destroying it */
		/* Use only t3 from 11x_compute_est_Pout */
		if (ISGPHY(pi)) {
			tssi = (phy_reg_read(pi, BPHY_TSSI) & 0xff);
			PHY_TXPWR(("wl%d: %s: Using tssi = 0x%02x\n",
			          pi->sh->unit, __FUNCTION__, tssi));

			power->txpwr_est_Pout[0] =
			        (uint8)wlc_phy_compute_est_pout_abgphy(pi, NULL, (int8)tssi);
			tssi = (phy_reg_read(pi,
			                     (GPHY_TO_APHY_OFF + APHY_TSSI_STAT)) & 0xff);
			/* Normalize the TSSI for G */
			tssi += 0x20;
			tssi &= 0x3f;
			power->txpwr_est_Pout_gofdm =
			        (uint8)wlc_phy_compute_est_pout_abgphy(pi, NULL, (int8)tssi);
		} else if (ISAPHY(pi)) {
			chan_info_abgphy_t *ci;
			uint i;
			int32 t3;
			t3 = (phy_reg_read(pi, APHY_TSSI_STAT) & 0xff);
			/* sign extend it */
			t3 <<= 24;
			t3 >>= 24;
			/* lookup radio-chip-specific channel code */
			for (i = 0; i < ARRAYSIZE(chan_info_abgphy); i++)
				if (chan_info_abgphy[i].chan == CHSPEC_CHANNEL(pi->radio_chanspec))
					break;

			ASSERT(i < ARRAYSIZE(chan_info_abgphy));
			ci = &chan_info_abgphy[i];

			/* compute estimate Pout */
			power->txpwr_est_Pout[1] = (uint8)wlc_phy_compute_est_pout_abgphy(pi, ci,
				(int8)t3);
		}
	} else {
		if (ISGPHY(pi)) {
			power->txpwr_est_Pout[0] = phy_reg_read(pi, BPHY_TX_EST_PWR) & 0xff;
			power->txpwr_est_Pout_gofdm =
			        phy_reg_read(pi, GPHY_TO_APHY_OFF + APHY_RSSI_FILT_A2) & 0xff;
		} else if (ISAPHY(pi)) {
			power->txpwr_est_Pout[1] = phy_reg_read(pi, APHY_RSSI_FILT_A2) & 0xff;
		}
	}
}

#if defined(AP) && defined(RADAR)
#if ACONF
static void
wlc_radar_attach_aphy(phy_info_t *pi)
{
	pi->rargs[0].min_tint = 4000;
	pi->rargs[0].max_tint = 100000;
	pi->rargs[0].min_blen = 100000;
	pi->rargs[0].max_blen = 4200000;
	pi->rargs[0].radar_args.quant = 128;
	pi->rargs[0].sdepth_extra_pulses = 1;
	pi->rargs[0].radar_args.npulses = 5;
	pi->rargs[0].radar_args.ncontig = 3;
	pi->rargs[0].radar_args.min_pw = 4;
	pi->rargs[0].radar_args.max_pw = 100;
	pi->rargs[0].radar_args.thresh0 = 0x2b0;
	pi->rargs[0].radar_args.thresh1 = 0x2f0;
	pi->rargs[0].radar_args.fmdemodcfg = 0x0;
	pi->rargs[0].radar_args.autocorr = 0x0;
	pi->rargs[0].radar_args.st_level_time = 0x0;
	pi->rargs[0].radar_args.t2_min = 0;
	pi->rargs[0].radar_args.npulses_lp = 8;
	pi->rargs[0].radar_args.min_pw_lp = 500;
	pi->rargs[0].radar_args.max_pw_lp = 4000;
	pi->rargs[0].radar_args.min_fm_lp = 10;
	pi->rargs[0].radar_args.max_span_lp = 240000000;
	pi->rargs[0].radar_args.min_deltat = 1000;
	pi->rargs[0].radar_args.max_deltat = 3000000;
	pi->rargs[0].radar_args.blank = 0x0;
	pi->rargs[0].radar_args.version = WL_RADAR_ARGS_VERSION;
	pi->rargs[0].radar_args.fra_pulse_err = 60;
	pi->rargs[0].radar_args.npulses_fra = 2;
	pi->rargs[0].radar_args.npulses_stg2 = 4;
	pi->rargs[0].radar_args.npulses_stg3 = 5;

	/* aphy should never use [1] states */
	pi->rparams = &pi->rargs[0];
}
#endif /* ACONF */
#endif /* defined(AP) && defined(RADAR) */


bool
wlc_phy_attach_abgphy(phy_info_t *pi, int bandtype)
{
	phy_info_abgphy_t *pi_abg = NULL;

	pi->u.pi_abgphy = (phy_info_abgphy_t*)MALLOC(pi->sh->osh, sizeof(phy_info_abgphy_t));
	if (pi->u.pi_abgphy == NULL) {
	PHY_ERROR(("wl%d: %s: MALLOC failure\n", pi->sh->unit, __FUNCTION__));
		return FALSE;
	}
	printf("malloc in abgphy done\n");
	bzero((char *)pi->u.pi_abgphy, sizeof(phy_info_abgphy_t));
	pi_abg = pi->u.pi_abgphy;

	if (ISAPHY(pi)) {
		ASSERT(bandtype == WLC_BAND_5G);

		if (AREV_GE(pi->pubpi.phy_rev, 5)) {
			pi->hwpwrctrl = TRUE;
			pi->hwpwrctrl_capable = TRUE;

			/* init hardware power control index, */
			pi->hwpwr_txcur = 23;	/* About 10.5 db */
			PHY_TXPWR(("wl: hwpwrctrl, init band txpwr index %d\n",
			          pi->hwpwr_txcur));
		}

		if (AREV_GE(pi->pubpi.phy_rev, 6) &&
		    (((phy_reg_read(pi, APHY_VITERBI_OFFSET)) >> 9) & 1))
			pi->pubpi.abgphy_encore = TRUE;

		pi->pi_fptr.init = wlc_phy_init_aphy;

#if defined(BCMDBG) || defined(WLTEST)
		pi->pi_fptr.longtrn = wlc_phy_aphy_long_train;
#endif

		if (!wlc_phy_txpwr_srom_read_aphy(pi))
			return FALSE;
#if ACONF
#if defined(AP) && defined(RADAR)
		wlc_radar_attach_aphy(pi);
#endif
#endif /* ACONF */

	} else if (ISGPHY(pi)) {

		ASSERT(bandtype == WLC_BAND_2G);

		/* Set the sbtml gmode indicator */
		pi_abg->sbtml_gm = TRUE;

		if (GREV_GE(pi->pubpi.phy_rev, 6)) {
			pi->hwpwrctrl = TRUE;
			pi->hwpwrctrl_capable = TRUE;

			pi->hwpwr_txcur = (pi->pubpi.radiorev == 8) ? 36 : 16;
			PHY_TXPWR(("wl: hwpwrctrl, init band txpwr index %d\n",
			          pi->hwpwr_txcur));

			if (GREV_GE(pi->pubpi.phy_rev, 7) &&
			    ((phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_VITERBI_OFFSET))>> 9) & 1))
				pi->pubpi.abgphy_encore = TRUE;
		}

		pi->pi_fptr.init = wlc_phy_init_gphy;
		pi->pi_fptr.calinit = wlc_phy_cal_init_gphy;

		if (!wlc_phy_txpwr_srom_read_gphy(pi))
			return FALSE;

	}
	pi->pi_fptr.chanset = wlc_phy_chanspec_set_abgphy;
	pi->pi_fptr.txpwrrecalc = wlc_phy_txpower_update_shm;

	return TRUE;
}

void
wlc_set_uninitted_abgphy(phy_info_t *pi)
{
	int i, j;
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	pi_abg->tx_vos = 0xffff;
	pi_abg->nrssi_table_delta = 0x7fffffff;
	pi_abg->rc_cal = 0xffff;
	pi_abg->mintxbias = 0xffff;
	pi_abg->radiopwr = 0xffff;
	for (i = 0; i < STATIC_NUM_RF; i++) {
		for (j = 0; j < STATIC_NUM_BB; j++) {
			pi_abg->stats_11b_txpower[i][j] = -1;
		}
	}
}

void
wlc_phy_txpower_hw_ctrl_set_abgphy(phy_info_t *pi)
{
	uint16 pcl_pwridx; /* current power control loop (pcl) power index */
	if (ISAPHY(pi)) {
		pcl_pwridx = phy_reg_read(pi, APHY_RSSI_FILT_B0);
#if ACONF
		wlc_phy_txpwrctrl_hw_aphy(pi, pi->hwpwrctrl);
#endif

		/* if hwpwrctrl is off, apply current pcl power inex */
		if (!pi->hwpwrctrl) {
			wlc_set_11a_tpi_set(pi, (int8)pcl_pwridx);
		}
	} else if (ISGPHY(pi)) {
#if GCONF
		wlc_phy_txpwrctrl_hw_gphy(pi, pi->hwpwrctrl);
#endif
	}
}

void
wlc_phy_ant_rxdiv_set_abgphy(phy_info_t *pi, uint8 val)
{
	uint16 bb, ad;
	uint aphy_offset;
	uint phyrev = pi->pubpi.phy_rev;

	aphy_offset = (ISAPHY(pi)) ? 0 : GPHY_TO_APHY_OFF;

	/* Control OFDM selection diversity */

	/* set bbConfig rx diversity setting */
	bb = phy_reg_read(pi, (aphy_offset + APHY_BBCONFIG));
	bb &= ~PHY_BBC_ANT_MASK;			/* clear ANT_DIV field */
	if (val > ANT_RX_DIV_FORCE_1) {
		/* Enable Rx diversity */
		bb |= (ANT_RX_DIV_ENABLE << PHY_BBC_ANT_SHIFT) & PHY_BBC_ANT_MASK;
	} else {
		/* Force Rx to ant 0 or 1 */
		bb |= (((uint16)val) << PHY_BBC_ANT_SHIFT) & PHY_BBC_ANT_MASK;
	}
	phy_reg_write(pi, (aphy_offset + APHY_BBCONFIG), bb);

	/* set the Rx diversity First Antenna field if diversity is enabled */
	if (val > ANT_RX_DIV_FORCE_1) {
		ad = phy_reg_read(pi, (aphy_offset + APHY_ANT_DWELL));
		ad &= ~APHY_ANT_DWELL_FIRST_ANT;
		if (val == ANT_RX_DIV_START_1)
			ad |= APHY_ANT_DWELL_FIRST_ANT;
		phy_reg_write(pi, (aphy_offset + APHY_ANT_DWELL), ad);
	}

	if (ISGPHY(pi)) {
		/* Control CCK/DSSS selection diversity in G mode */
		bb = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_ANT_WR_SETTLE));
		bb &= ~PHY_AWS_ANTDIV;
		bb |= (val > ANT_RX_DIV_FORCE_1) ? PHY_AWS_ANTDIV : 0;
		phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_ANT_WR_SETTLE), bb);
		if (GREV_GT(phyrev, 1)) {
			PHY_REG_LIST_START
				/* turn on smart diversity (bit 4) in G mode for C0 */
				PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CTL, 0x10)
				/* increase div_search_gain_backoff (bits 0 to 7)
				 * from 15 to 21 to cover isolation of 25 dB seen on rev72 boards.
				 */
				PHY_REG_MOD_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_DIV_SEARCH_GN_BACK,
					0xff, 21)
			PHY_REG_LIST_EXECUTE(pi);
			/* increase 2nd antenna min level to 8 */
			if (GREV_IS(phyrev, 2))
				phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_DC_B1), 8);
			else
				phy_reg_mod(pi, (GPHY_TO_APHY_OFF + APHY_DC_B1), 0xff, 8);
		}
		/* var_cck_Nus_thresh -- determines when to average over 4us
		 * for power estimation.
		 * helps with CCK's low-end sensitivity when diversity is on
		 * and when both antennas see very small SNRs.
		 */
		if (GREV_GE(phyrev, 6)) {
			phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_CCK_NUS_THRESH), 220);
		}
	} else {
		/* Must be APHY */
		if (AREV_GT(phyrev, 2)) {
			/* turn on smart diversity (bit 4) for C0 */
			phy_reg_or(pi, APHY_CTL, 0x10);
			/* set a_div_pwr_thresh to 29 */
			/* set 2nd antenna min level to 8 */
			if (AREV_IS(phyrev, 3)) {
				PHY_REG_LIST_START
					PHY_REG_WRITE_RAW_ENTRY(APHY_CLIP_PWDN_THRESH, 0x1d)
					PHY_REG_WRITE_RAW_ENTRY(APHY_DC_B1, 8)
				PHY_REG_LIST_EXECUTE(pi);
			} else {
				PHY_REG_LIST_START
					PHY_REG_WRITE_RAW_ENTRY(APHY_CLIP_PWDN_THRESH, 0x3a)
					PHY_REG_MOD_RAW_ENTRY(APHY_DC_B1, 0xff, 8)
				PHY_REG_LIST_EXECUTE(pi);
			}
		} else {
			/* For B0
			 * increase second ant dwell time (bits 0-7) to 36 clk20s.
			 * this was found necessary for the matched rev7.1 B0 boards.
			 */
			phy_reg_mod(pi, APHY_ANT_DWELL, 0xff, 36);
		}
	}
}

/* Return whether or not number of samples exceeded threshold */
static bool
wlc_aci_detect_rssi_power(void *cpi, bool wrssi, int fail_thresh, int channel)
{
	phy_info_t *pi = (phy_info_t *)cpi;
	uint16 reg_val;
	int rssi, rssi_thresh;
	int num_exceeded, num_pts;
	uint16 old_reg = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_PWRDWN));

	ASSERT((channel > 0) && (channel <= CH_MAX_2G_CHANNEL));
	wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(channel));

	reg_val = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_PWRDWN)) & 0xfff8;
	reg_val |= wrssi ? 0x4 : 0x5;
	phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_PWRDWN), reg_val);

	num_exceeded = 0;
	if (pi->aci_state & ACI_ACTIVE)
		rssi_thresh = pi->u.pi_abgphy->interference_save.threshold & 0x3f;
	else
		rssi_thresh = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_N1_N2_THRESH)) & 0x3f;

	if (rssi_thresh > 31)
		rssi_thresh -= 64;

	for (num_pts = 0; num_pts < ACI_SAMPLES; num_pts++) {
		rssi = phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_WRSSI_NRSSI));
		if (!wrssi)
			rssi >>= 8;
		rssi = rssi & 0x3f;
		if (rssi > 31)
			rssi -= 64;

		pi->interf.aci.rssi_buf[channel - 1][num_pts] = rssi & 0xff;

		if (rssi < rssi_thresh) {
			num_exceeded++;
			if (num_exceeded > fail_thresh) {
				PHY_CAL(("wlc_aci_detect_rssi_power: chan %d: rssi %d (0x%x) <"
					"%d %d times\n",
					channel, rssi, rssi, rssi_thresh, fail_thresh));
				break;
			}
		}
		OSL_DELAY(pi->interf.aci.usec_spintime);
	}

	phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_PWRDWN), old_reg);
	pi->interf.aci.rssi_buf[channel - 1][num_pts] = (char)0xff;
	return (num_exceeded > fail_thresh);
}

/* Return whether ot not ACI is present on current channel */
bool
wlc_phy_aci_scan_gphy(phy_info_t *pi)
{
	int orig_channel = CHSPEC_CHANNEL(pi->radio_chanspec);
	int chan, i, start, end;
	uint32 tmp_vector, chan_vector;

	wlc_phyreg_enter((wlc_phy_t*)pi);
	wlc_radioreg_enter((wlc_phy_t*)pi);

	/* Clear classifiers */
	gphy_classifyctl(pi, FALSE, FALSE);
	aphy_all_crs(pi, FALSE);
	gphy_all_gains(pi, 3, 8, 1, TRUE);

	/* Init rssi buffer */
	for (chan = ACI_FIRST_CHAN - 1; chan < ACI_LAST_CHAN; chan++)
		pi->interf.aci.rssi_buf[chan][0] = (char)0xff;

	chan_vector = 0;
	start = MAX(ACI_FIRST_CHAN, orig_channel - ACI_CHANNEL_DELTA);
	end = MIN(ACI_LAST_CHAN, orig_channel + ACI_CHANNEL_DELTA);

	/* Cut down on the number of channels we need to scan.
	 * Don't want to measure the noise that we are, ourselves, creating
	 * on this channel, nor the 2 immediate channels on either side.
	 * Also, there is no point in scanning channels so far out that
	 * a signal won't bleed that far.
	 */
	for (chan = start; chan <= end; chan++) {
		if ((chan < (orig_channel - ACI_CHANNEL_SKIP)) ||
		    (chan > (orig_channel + ACI_CHANNEL_SKIP))) {
			if (wlc_aci_detect_rssi_power(pi, FALSE, 20, chan)) {
				chan_vector |= (1 << chan);
			}
		}
	}
	PHY_CAL(("wl%d: %s: Raw vector is 0x%x\n",
	        pi->sh->unit, __FUNCTION__, chan_vector));

	/* Revert back to original channel */
	wlc_phy_chanspec_set((wlc_phy_t*)pi, CH20MHZ_CHSPEC(orig_channel));

	gphy_classifyctl(pi, TRUE, TRUE);

	phy_reg_write(pi, (GPHY_TO_APHY_OFF + APHY_PWRDWN),
		phy_reg_read(pi, (GPHY_TO_APHY_OFF + APHY_PWRDWN)) & 0xfff8);


	aphy_all_crs(pi, TRUE);
	gphy_orig_gains(pi, TRUE);

	wlc_radioreg_exit((wlc_phy_t*)pi);
	wlc_phyreg_exit((wlc_phy_t*)pi);

	/* Add in the bleed, but only if we need to. */
	if ((chan_vector & (1 << CHSPEC_CHANNEL(pi->radio_chanspec))) == 0) {
		tmp_vector = chan_vector;
		for (chan = ACI_FIRST_CHAN; chan <= ACI_LAST_CHAN; chan++) {
			if (tmp_vector & (1 << chan)) {
				/* Turn on all channels plus and minus 5 channels */
				/* Make sure to stay with valid channels */
				for (i = MAX(1, chan-ACI_CHANNEL_DELTA);
				     i <= MIN(ACI_LAST_CHAN, (chan + ACI_CHANNEL_DELTA)); i++) {
					chan_vector |= (1 << i);
				}
			}
		}
	}

	PHY_CAL(("wl%d: %s: Cooked vector is 0x%x\n",
	        pi->sh->unit, __FUNCTION__, chan_vector));

	/* Is current channel in the vector? */
	if (chan_vector & (1 << CHSPEC_CHANNEL(pi->radio_chanspec))) {
		PHY_CAL(("wl%d: %s: Found ACI on this channel (%d)\n",
		        pi->sh->unit, __FUNCTION__, CHSPEC_CHANNEL(pi->radio_chanspec)));
		return (TRUE);
	}

	PHY_CAL(("wl%d: %s: No ACI on this channel (%d)\n",
	        pi->sh->unit, __FUNCTION__, CHSPEC_CHANNEL(pi->radio_chanspec)));
	return (FALSE);
}

void
wlc_phy_aci_interf_nwlan_set_gphy(phy_info_t *pi, bool on)
{
	if (on) {
		PHY_REG_LIST_START
			/* Disable ED phycrs transitions. */
			PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_ANT_DWELL, 0x800)
			/* Disable APHY crs0 */
			PHY_REG_AND_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN,
				~APHY_CTHR_CRS1_ENABLE)
		PHY_REG_LIST_EXECUTE(pi);
	} else {
		PHY_REG_LIST_START
			/* Enable ED phycrs transitions. */
			PHY_REG_AND_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_ANT_DWELL, (
				uint16)~0x800)
			/* Enable APHY crs0 */
			PHY_REG_OR_RAW_ENTRY(GPHY_TO_APHY_OFF + APHY_CTHR_STHR_SHDIN,
				APHY_CTHR_CRS1_ENABLE)
		PHY_REG_LIST_EXECUTE(pi);
	}
}

void
wlc_phy_cal_measurelo_gphy(phy_info_t *pi)
{
	phy_info_abgphy_t *pi_abg = pi->u.pi_abgphy;

	wlapi_suspend_mac_and_wait(pi->sh->physhim);
	wlc_phy_cal_gphy_measurelo(pi);
	pi_abg->abgphy_cal.abgphy_cal_mlo = pi->sh->now;
	wlapi_enable_mac(pi->sh->physhim);


	if (pi->hwpwrctrl)
		wlc_phy_cal_txpower_stats_clr_gphy(pi);
}
