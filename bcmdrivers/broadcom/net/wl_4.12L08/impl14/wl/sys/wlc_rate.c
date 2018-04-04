/*
 * Common [OS-independent] rate management
 * 802.11 Networking Adapter Device Driver.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_rate.c 379768 2013-01-18 23:00:41Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>

#include <proto/802.11.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wl_dbg.h>
#include <wlc_pub.h>

/* Rate info per rate: It tells whether a rate is ofdm or not and its phy_rate value */
const uint8 rate_info[WLC_MAXRATE + 1] = {
	/*  0     1     2     3     4     5     6     7     8     9 */
/*   0 */ 0x00, 0x00, 0x0a, 0x00, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  10 */ 0x00, 0x37, 0x8b, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8f, 0x00,
/*  20 */ 0x00, 0x00, 0x6e, 0x00, 0x8a, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  30 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8e, 0x00, 0x00, 0x00,
/*  40 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x89, 0x00,
/*  50 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  60 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  70 */ 0x00, 0x00, 0x8d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  80 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
/*  90 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0x00, 0x00, 0x00,
/* 100 */ 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8c
};


/* rates are in units of Kbps */
const mcs_info_t mcs_table[MCS_TABLE_SIZE] = {
	/* MCS  0: SS 1, MOD: BPSK,  CR 1/2 */
	{6500,   13500,  CEIL(6500*10, 9),   CEIL(13500*10, 9),  0x00, WLC_RATE_6M},
	/* MCS  1: SS 1, MOD: QPSK,  CR 1/2 */
	{13000,  27000,  CEIL(13000*10, 9),  CEIL(27000*10, 9),  0x08, WLC_RATE_12M},
	/* MCS  2: SS 1, MOD: QPSK,  CR 3/4 */
	{19500,  40500,  CEIL(19500*10, 9),  CEIL(40500*10, 9),  0x0A, WLC_RATE_18M},
	/* MCS  3: SS 1, MOD: 16QAM, CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9),  0x10, WLC_RATE_24M},
	/* MCS  4: SS 1, MOD: 16QAM, CR 3/4 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x12, WLC_RATE_36M},
	/* MCS  5: SS 1, MOD: 64QAM, CR 2/3 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0x19, WLC_RATE_48M},
	/* MCS  6: SS 1, MOD: 64QAM, CR 3/4 */
	{58500,  121500, CEIL(58500*10, 9),  CEIL(121500*10, 9), 0x1A, WLC_RATE_54M},
	/* MCS  7: SS 1, MOD: 64QAM, CR 5/6 */
	{65000,  135000, CEIL(65000*10, 9),  CEIL(135000*10, 9), 0x1C, WLC_RATE_54M},
	/* MCS  8: SS 2, MOD: BPSK,  CR 1/2 */
	{13000,  27000,  CEIL(13000*10, 9),  CEIL(27000*10, 9),  0x40, WLC_RATE_6M},
	/* MCS  9: SS 2, MOD: QPSK,  CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9),  0x48, WLC_RATE_12M},
	/* MCS 10: SS 2, MOD: QPSK,  CR 3/4 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x4A, WLC_RATE_18M},
	/* MCS 11: SS 2, MOD: 16QAM, CR 1/2 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0x50, WLC_RATE_24M},
	/* MCS 12: SS 2, MOD: 16QAM, CR 3/4 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0x52, WLC_RATE_36M},
	/* MCS 13: SS 2, MOD: 64QAM, CR 2/3 */
	{104000, 216000, CEIL(104000*10, 9), CEIL(216000*10, 9), 0x59, WLC_RATE_48M},
	/* MCS 14: SS 2, MOD: 64QAM, CR 3/4 */
	{117000, 243000, CEIL(117000*10, 9), CEIL(243000*10, 9), 0x5A, WLC_RATE_54M},
	/* MCS 15: SS 2, MOD: 64QAM, CR 5/6 */
	{130000, 270000, CEIL(130000*10, 9), CEIL(270000*10, 9), 0x5C, WLC_RATE_54M},
	/* MCS 16: SS 3, MOD: BPSK,  CR 1/2 */
	{19500,  40500,  CEIL(19500*10, 9),  CEIL(40500*10, 9),  0x80, WLC_RATE_6M},
	/* MCS 17: SS 3, MOD: QPSK,  CR 1/2 */
	{39000,  81000,  CEIL(39000*10, 9),  CEIL(81000*10, 9),  0x88, WLC_RATE_12M},
	/* MCS 18: SS 3, MOD: QPSK,  CR 3/4 */
	{58500,  121500, CEIL(58500*10, 9),  CEIL(121500*10, 9), 0x8A, WLC_RATE_18M},
	/* MCS 19: SS 3, MOD: 16QAM, CR 1/2 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0x90, WLC_RATE_24M},
	/* MCS 20: SS 3, MOD: 16QAM, CR 3/4 */
	{117000, 243000, CEIL(117000*10, 9), CEIL(243000*10, 9), 0x92, WLC_RATE_36M},
	/* MCS 21: SS 3, MOD: 64QAM, CR 2/3 */
	{156000, 324000, CEIL(156000*10, 9), CEIL(324000*10, 9), 0x99, WLC_RATE_48M},
	/* MCS 22: SS 3, MOD: 64QAM, CR 3/4 */
	{175500, 364500, CEIL(175500*10, 9), CEIL(364500*10, 9), 0x9A, WLC_RATE_54M},
	/* MCS 23: SS 3, MOD: 64QAM, CR 5/6 */
	{195000, 405000, CEIL(195000*10, 9), CEIL(405000*10, 9), 0x9B, WLC_RATE_54M},
	/* MCS 24: SS 4, MOD: BPSK,  CR 1/2 */
	{26000,  54000,  CEIL(26000*10, 9),  CEIL(54000*10, 9), 0xC0, WLC_RATE_6M},
	/* MCS 25: SS 4, MOD: QPSK,  CR 1/2 */
	{52000,  108000, CEIL(52000*10, 9),  CEIL(108000*10, 9), 0xC8, WLC_RATE_12M},
	/* MCS 26: SS 4, MOD: QPSK,  CR 3/4 */
	{78000,  162000, CEIL(78000*10, 9),  CEIL(162000*10, 9), 0xCA, WLC_RATE_18M},
	/* MCS 27: SS 4, MOD: 16QAM, CR 1/2 */
	{104000, 216000, CEIL(104000*10, 9), CEIL(216000*10, 9), 0xD0, WLC_RATE_24M},
	/* MCS 28: SS 4, MOD: 16QAM, CR 3/4 */
	{156000, 324000, CEIL(156000*10, 9), CEIL(324000*10, 9), 0xD2, WLC_RATE_36M},
	/* MCS 29: SS 4, MOD: 64QAM, CR 2/3 */
	{208000, 432000, CEIL(208000*10, 9), CEIL(432000*10, 9), 0xD9, WLC_RATE_48M},
	/* MCS 30: SS 4, MOD: 64QAM, CR 3/4 */
	{234000, 486000, CEIL(234000*10, 9), CEIL(486000*10, 9), 0xDA, WLC_RATE_54M},
	/* MCS 31: SS 4, MOD: 64QAM, CR 5/6 */
	{260000, 540000, CEIL(260000*10, 9), CEIL(540000*10, 9), 0xDB, WLC_RATE_54M},
	/* MCS 32: SS 1, MOD: BPSK,  CR 1/2 */
	{0,      6000,   0, CEIL(6000*10, 9), 0x00, WLC_RATE_6M},
	};


/* phycfg for legacy OFDM frames: code rate, modulation scheme, spatial streams
 *   Number of spatial streams: always 1
 *   other fields: refer to table 78 of section 17.3.2.2 of the original .11a standard
 */
typedef struct legacy_phycfg {
	uint32	rate_ofdm;	/* ofdm mac rate */
	uint8	tx_phy_ctl3;	/* phy ctl byte 3, code rate, modulation type, # of streams */
} legacy_phycfg_t;

#define LEGACY_PHYCFG_TABLE_SIZE	12	/* Number of legacy_rate_cfg entries in the table */

/* In CCK mode LPPHY overloads OFDM Modulation bits with CCK Data Rate */
/* Eventually MIMOPHY would also be converted to this format */
/* 0 = 1Mbps; 1 = 2Mbps; 2 = 5.5Mbps; 3 = 11Mbps */
static const legacy_phycfg_t	legacy_phycfg_table[LEGACY_PHYCFG_TABLE_SIZE] = {
	{WLC_RATE_1M,  0x00},	/* CCK  1Mbps,  data rate  0 */
	{WLC_RATE_2M,  0x08},	/* CCK  2Mbps,  data rate  1 */
	{WLC_RATE_5M5,  0x10},	/* CCK  5.5Mbps,  data rate  2 */
	{WLC_RATE_11M,  0x18},	/* CCK  11Mbps,  data rate   3 */
	{WLC_RATE_6M,  0x00},	/* OFDM  6Mbps,  code rate 1/2, BPSK,   1 spatial stream */
	{WLC_RATE_9M,  0x02},	/* OFDM  9Mbps,  code rate 3/4, BPSK,   1 spatial stream */
	{WLC_RATE_12M, 0x08},	/* OFDM  12Mbps, code rate 1/2, QPSK,   1 spatial stream */
	{WLC_RATE_18M, 0x0A},	/* OFDM  18Mbps, code rate 3/4, QPSK,   1 spatial stream */
	{WLC_RATE_24M, 0x10},	/* OFDM  24Mbps, code rate 1/2, 16-QAM, 1 spatial stream */
	{WLC_RATE_36M, 0x12},	/* OFDM  36Mbps, code rate 3/4, 16-QAM, 1 spatial stream */
	{WLC_RATE_48M, 0x19},	/* OFDM  48Mbps, code rate 2/3, 64-QAM, 1 spatial stream */
	{WLC_RATE_54M, 0x1A},	/* OFDM  54Mbps, code rate 3/4, 64-QAM, 1 spatial stream */
};

/* Hardware rates (also encodes default basic rates) */

const wlc_rateset_t cck_ofdm_mimo_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_0_9_NSS3
};

const wlc_rateset_t ofdm_mimo_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{       0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_0_9_NSS3
};

/* Default ratesets that include MCS32 for 40BW channels */
const wlc_rateset_t cck_ofdm_40bw_mimo_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_0_9_NSS3
};

const wlc_rateset_t ofdm_40bw_mimo_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{	0xff, 0xff, 0xff, 0xff, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_0_9_NSS3
};

const wlc_rateset_t cck_ofdm_rates = {
	12,
	{ /*	1b,   2b,   5.5b, 6,    9,    11b,  12,   18,   24,   36,   48,   54 Mbps */
		0x82, 0x84, 0x8b, 0x0c, 0x12, 0x96, 0x18, 0x24, 0x30, 0x48, 0x60, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

const wlc_rateset_t gphy_legacy_rates = {
	4,
	{ /*	1b,   2b,   5.5b,  11b Mbps */
		0x82, 0x84, 0x8b, 0x96
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

const wlc_rateset_t ofdm_rates = {
	8,
	{ /*	6b,   9,    12b,  18,   24b,  36,   48,   54 Mbps */
		0x8c, 0x12, 0x98, 0x24, 0xb0, 0x48, 0x60, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

const wlc_rateset_t cck_rates = {
	4,
	{ /*	1b,   2b,   5.5,  11 Mbps */
		0x82, 0x84, 0x0b, 0x16
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

/* set of rates in supported rates elt when splitting rates between it and the Extended Rates elt */
const wlc_rateset_t wlc_lrs_rates = {
	8,
	{ /*	1,    2,    5.5,  11,   18,   24,   36,   54 Mbps */
		0x02, 0x04, 0x0b, 0x16, 0x24, 0x30, 0x48, 0x6c
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};


/* Rateset including only 1 and 2 Mbps, without basic rate annotation.
 * This is used for a wlc_rate_hwrs_filter_sort_validate param when limiting a rateset for
 * Japan Channel 14 regulatory compliance using early radio revs
 */
const wlc_rateset_t rate_limit_1_2 = {
	2,
	{ /*	1,    2  Mbps */
		0x02, 0x04
	},
	0x00,
	{ 	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00
	},
	VHT_CAP_MCS_MAP_NONE_ALL
};

static bool wlc_rateset_valid(wlc_rateset_t *rs, bool check_brate);

/* Modulation without spatial stream info */
uint8
wlc_ratespec_mcs(ratespec_t rspec)
{
	uint8 mcs;

	if (RSPEC_ISVHT(rspec)) {
		mcs = (uint8)(rspec & RSPEC_VHT_MCS_MASK);
	} else if (RSPEC_ISHT(rspec)) {
		if ((rspec & RSPEC_RATE_MASK) == 32)
			mcs = 32;
		else
			mcs = (uint8)(rspec & RSPEC_HT_MCS_MASK);
	} else {
		mcs = MCS_INVALID;
	}

	return mcs;
}

/* Number of spatial streams, Nss, specified by the ratespec */
uint
wlc_ratespec_nss(ratespec_t rspec)
{
	int Nss;

	if (RSPEC_ISVHT(rspec)) {

		/* VHT ratespec specifies Nss directly */
		Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

	} else if (RSPEC_ISHT(rspec)) {
		uint mcs = rspec & RSPEC_RATE_MASK;

		/* HT MCS encodes Nss in MCS index */

		ASSERT(mcs <= 32);

		if (mcs == 32) {
			Nss = 1;
		} else {
			Nss = 1 + (mcs / 8);
		}
	} else {
		/* Legacy rates are all Nss = 1, just add the expansion */
		Nss = 1;
	}

	return Nss;
}

/* Number of space time streams, Nss + Nstbc expansion, specified by the ratespec */
uint
wlc_ratespec_nsts(ratespec_t rspec)
{
	int Nss;
	int Nsts;

	if (RSPEC_ISVHT(rspec)) {
		/* VHT ratespec specifies Nss directly */
		Nss = (rspec & WL_RSPEC_VHT_NSS_MASK) >> WL_RSPEC_VHT_NSS_SHIFT;

		/* VHT STBC expansion always is Nsts = Nss*2, so STBC expansion == Nss */
		if (RSPEC_ISSTBC(rspec)) {
			Nsts = 2*Nss;
		} else {
			Nsts = Nss;
		}
	} else if (RSPEC_ISHT(rspec)) {
		uint mcs = rspec & RSPEC_RATE_MASK;

		ASSERT(mcs <= 32);

		if (mcs == 32) {
			/* 11n does not allow STBC for MCS 32 */
			ASSERT(RSPEC_ISSTBC(rspec) == FALSE);
			Nsts = 1;
		} else {
			/* HT MCS encodes Nss in MCS index */
			Nss = 1 + (mcs / 8);

			if (RSPEC_ISSTBC(rspec)) {
				ASSERT(Nss == 1);
				Nsts = Nss + 1;
			} else {
				Nsts = Nss;
			}
		}
	} else {
		/* Legacy rates are all Nss = 1, and no STBC expansion */
		Nsts = 1;
	}

	return Nsts;
}

/* Number of Tx chains, NTx, specified by the ratespec */
uint
wlc_ratespec_ntx(ratespec_t rspec)
{
	int Nsts;
	int NTx;

	/* calculate the underlying Space-Time-Streams */
	Nsts = wlc_ratespec_nsts(rspec);

	/* Tx chain expansion is encoded directly in ratespec */
	NTx = Nsts + RSPEC_TXEXP(rspec);

	return NTx;
}

struct ieee_80211_mcs_rate_info {
	uint8 constellation_bits;
	uint8 coding_q;
	uint8 coding_d;
};

static const struct ieee_80211_mcs_rate_info wlc_mcs_info[] = {
	{ 1, 1, 2 }, /* MCS  0: MOD: BPSK,   CR 1/2 */
	{ 2, 1, 2 }, /* MCS  1: MOD: QPSK,   CR 1/2 */
	{ 2, 3, 4 }, /* MCS  2: MOD: QPSK,   CR 3/4 */
	{ 4, 1, 2 }, /* MCS  3: MOD: 16QAM,  CR 1/2 */
	{ 4, 3, 4 }, /* MCS  4: MOD: 16QAM,  CR 3/4 */
	{ 6, 2, 3 }, /* MCS  5: MOD: 64QAM,  CR 2/3 */
	{ 6, 3, 4 }, /* MCS  6: MOD: 64QAM,  CR 3/4 */
	{ 6, 5, 6 }, /* MCS  7: MOD: 64QAM,  CR 5/6 */
	{ 8, 3, 4 }, /* MCS  8: MOD: 256QAM, CR 3/4 */
	{ 8, 5, 6 }  /* MCS  9: MOD: 256QAM, CR 5/6 */
};

uint
wlc_rate_mcs2rate(uint mcs, uint nss, uint bw, int sgi)
{
	const int ksps = 250; /* kilo symbols per sec, 4 us sym */
	const int Nsd_20MHz = 52;
	const int Nsd_40MHz = 108;
	const int Nsd_80MHz = 234;
	const int Nsd_160MHz = 468;
	uint rate;

	if (mcs == 32) {
		/* just return fixed values for mcs32 instead of trying to parametrize */
		rate = (sgi == 0) ? 6000 : 6778;
	} else if (mcs <= 9) {
		/* This calculation works for 11n HT and 11ac VHT if the HT mcs values
		 * are decomposed into a base MCS = MCS % 8, and Nss = 1 + MCS / 8.
		 * That is, HT MCS 23 is a base MCS = 7, Nss = 3
		 */

		/* find the number of complex numbers per symbol */
		if (RSPEC_IS20MHZ(bw)) {
			rate = Nsd_20MHz;
		} else if (RSPEC_IS40MHZ(bw)) {
			rate = Nsd_40MHz;
		} else if (bw == RSPEC_BW_80MHZ) {
			rate = Nsd_80MHz;
		} else if (bw == RSPEC_BW_160MHZ) {
			rate = Nsd_160MHz;
		} else {
			rate = 0;
		}

		/* multiply by bits per number from the constellation in use */
		rate = rate * wlc_mcs_info[mcs].constellation_bits;

		/* adjust for the number of spatial streams */
		rate = rate * nss;

		/* adjust for the coding rate given as a quotient and divisor */
		rate = (rate * wlc_mcs_info[mcs].coding_q) / wlc_mcs_info[mcs].coding_d;

		/* multiply by Kilo symbols per sec to get Kbps */
		rate = rate * ksps;

		/* adjust the symbols per sec for SGI
		 * symbol duration is 4 us without SGI, and 3.6 us with SGI,
		 * so ratio is 10 / 9
		 */
		if (sgi) {
			/* add 4 for rounding of division by 9 */
			rate = ((rate * 10) + 4) / 9;
		}
	} else {
		rate = 0;
	}

	return rate;
}

/* take a well formed ratespec_t arg and return phy rate in Kbps */
int
wlc_rate_rspec2rate(ratespec_t rspec)
{
	int rate = -1;

	if (RSPEC_ISLEGACY(rspec)) {
		rate = 500 * (rspec & RSPEC_RATE_MASK);
	} else if (RSPEC_ISHT(rspec)) {
		uint mcs = (rspec & RSPEC_RATE_MASK);

		ASSERT(mcs <= 32);

		if (mcs == 32) {
			rate = wlc_rate_mcs2rate(mcs, 1, RSPEC_BW_40MHZ, RSPEC_ISSGI(rspec));
		} else {
			uint nss = 1 + (mcs / 8);
			mcs = mcs % 8;

			rate = wlc_rate_mcs2rate(mcs, nss, RSPEC_BW(rspec), RSPEC_ISSGI(rspec));
		}
	} else if (RSPEC_ISVHT(rspec)) {
		uint mcs = (rspec & RSPEC_VHT_MCS_MASK);
		uint nss = (rspec & RSPEC_VHT_NSS_MASK) >> RSPEC_VHT_NSS_SHIFT;

		/* ASSERT(mcs <= 9); */
		ASSERT(nss <= 8);

		rate = wlc_rate_mcs2rate(mcs, nss, RSPEC_BW(rspec), RSPEC_ISSGI(rspec));
	} else {
		ASSERT(0);
	}

	return (rate == 0) ? -1 : rate;
}

uint
wlc_rate_rspec_reference_rate(ratespec_t rspec)
{
	uint legacy_rate;

	if (RSPEC_ISVHT(rspec)) {
		legacy_rate = wlc_rate_vht_basic_reference(rspec & RSPEC_VHT_MCS_MASK);
	} else if (RSPEC_ISHT(rspec)) {
		legacy_rate = wlc_rate_ht_basic_reference(rspec & RSPEC_RATE_MASK);
	} else {
		legacy_rate = (rspec & RSPEC_RATE_MASK);
	}

	return legacy_rate;
}

/* 802.11n-2009 "9.6.2 Non-HT basic rate calculation"
 * defines the calculation of a non-HT reference rate from an HT MCS
 * for use in determining the rate of a response frame.
 * The reference rate for MCS 0-7 is repeated for MCS 8-15, 16-23, and 24-31.
 * Only MCS 0-7 is in this table, so do MOD 8 values to look up.
 * Extending the table for 802.11ac VHT. VHT MCS 0-7 match HT MCS 0-7.
 * Adding rows for VHT MCS 8-9.
 */
static const uint basic_ref_rate[] = {
	/* MCS 0 */ WLC_RATE_6M,
	/* MCS 1 */ WLC_RATE_12M,
	/* MCS 2 */ WLC_RATE_18M,
	/* MCS 3 */ WLC_RATE_24M,
	/* MCS 4 */ WLC_RATE_36M,
	/* MCS 5 */ WLC_RATE_48M,
	/* MCS 6 */ WLC_RATE_54M,
	/* MCS 7 */ WLC_RATE_54M,
	/* VHT MCS 8 */ WLC_RATE_54M,
	/* VHT MCS 9 */ WLC_RATE_54M
};

/* Legacy reference rate for HT MCSs */
uint
wlc_rate_ht_basic_reference(uint ht_mcs)
{
	uint ref;

	/* Only handle MCS 0-32. None of the MCS>32 are supported by BRCM */
	ASSERT(ht_mcs <= 32);

	/* calculate the index for the basid_ref_rate[] table */
	if (ht_mcs >= 32) {
		/* use MCS0 for MCS32 (equivalent modulation and coding),
		 * or any out of range value
		 */
		ht_mcs = 0;
	} else {
		/* Take (MCS mod 8) to get equivalent modulation
		 * and coding in the 0-7 range.
		 */
		ht_mcs = ht_mcs % 8;
	}

	ref = basic_ref_rate[ht_mcs];

	return ref;
}

/* Legacy reference rate for VHT MCSs */
uint
wlc_rate_vht_basic_reference(uint vht_mcs)
{
	uint ref;

	/* Only MCS 0-9 are valid */
	ASSERT(vht_mcs <= 9);

	/* use MCS0 for any out of range value */
	if (vht_mcs > 9) {
		vht_mcs = 0;
	}

	ref = basic_ref_rate[vht_mcs];

	return ref;
}

#if defined(BCMDBG)
void
wlc_dump_rspec(void *context, ratespec_t rspec, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "\n\trspec 0x%x: ", rspec);
	/* vht, ht, legacy */
	if (RSPEC_ISHT(rspec)) {
		bcm_bprintf(b, " ht ");
	} else if (RSPEC_ISLEGACY(rspec)) {
		/* ofdm, cck */
		if (RSPEC_ISOFDM(rspec)) {
			bcm_bprintf(b, " ofdm ");
		} else if (RSPEC_ISCCK(rspec)) {
			bcm_bprintf(b, " cck ");
		}
	} else if (RSPEC_ISVHT(rspec)) {
		bcm_bprintf(b, " vht ");
	}

	/* ldpc, stbc, sgi */
	if (rspec & RSPEC_LDPC_CODING) {
		bcm_bprintf(b, " ldpc ");
	}
	if (rspec & RSPEC_STBC) {
		bcm_bprintf(b, " stbc ");
	}
	if (rspec & RSPEC_SHORT_GI) {
		bcm_bprintf(b, " sgi ");
	}
	/* rate */
	bcm_bprintf(b, "\n\tRate = %dkbps", wlc_rate_rspec2rate(rspec));
	if (RSPEC_ISVHT(rspec)) {
		bcm_bprintf(b, " mcs = %0x ", (rspec & RSPEC_VHT_MCS_MASK));
		bcm_bprintf(b, " nss = %d ", (rspec & RSPEC_VHT_NSS_MASK) >> RSPEC_VHT_NSS_SHIFT);
	} else if (RSPEC_ISHT(rspec)) {
		bcm_bprintf(b, " mcs = %0x ", (rspec & RSPEC_HT_MCS_MASK));
	}

	/* bandwidth */
	bcm_bprintf(b, " Bandwidth = ");
	switch (RSPEC_BW(rspec)) {
		case RSPEC_BW_UNSPECIFIED:
			bcm_bprintf(b, "unspecified");
			break;
		case RSPEC_BW_20MHZ:
			bcm_bprintf(b, "20MHz");
			break;
		case RSPEC_BW_40MHZ:
			bcm_bprintf(b, "40MHz");
			break;
		case RSPEC_BW_80MHZ:
			bcm_bprintf(b, "80MHz");
			break;
		case RSPEC_BW_160MHZ:
			bcm_bprintf(b, "160MHz");
			break;
		default:
			bcm_bprintf(b, "invalid");
			break;
	}
	bcm_bprintf(b, "\n");
}
#endif 

/* check if rateset is valid.
 * if check_brate is true, rateset without a basic rate is considered NOT valid.
 */
static bool
wlc_rateset_valid(wlc_rateset_t *rs, bool check_brate)
{
	uint idx;

	if (!rs->count)
		return FALSE;

	if (!check_brate)
		return TRUE;

	/* error if no basic rates */
	for (idx = 0; idx < rs->count; idx++) {
		if (rs->rates[idx] & WLC_RATE_FLAG)
			return TRUE;
	}
	return FALSE;
}

#ifdef WL11N
void
wlc_rateset_mcs_upd(wlc_rateset_t *rs, uint8 nstreams)
{
	int i;

	for (i = nstreams; i < MAX_STREAMS_SUPPORTED; i++)
		rs->mcs[i] = 0;
}

#ifdef WL11AC
/*
 * Clear out the unsupported rate sets, for non-existent streams.
 */
void
wlc_rateset_vhtmcs_upd(wlc_rateset_t *rs, uint8 nstreams)
{
	int nss;

	for (nss = nstreams + 1; nss <= VHT_CAP_MCS_MAP_NSS_MAX; nss++)
		VHT_MCS_MAP_SET_MCS_PER_SS(nss, VHT_CAP_MCS_MAP_NONE, rs->vht_mcsmap);
}
#endif /* WL11AC */
#endif /* WL11N */

/* filter BSS Membership from rateset
 */
bool
wlc_bss_membership_filter(wlc_rateset_t *rs)
{
	uint idx, rate_idx;
	wlc_rateset_t local;

	if (!rs->count)
		return FALSE;

	bzero(&local, sizeof(wlc_rateset_t));

	for (idx = rate_idx = 0; idx < rs->count; idx++) {
		if ((rs->rates[idx] & RATE_MASK) == WLC_HTPHY)
			rs->htphy_membership = rs->rates[idx];
		else {
			local.rates[rate_idx++] = rs->rates[idx];
			local.count++;
		}
	}
	rs->count = local.count;
	bcopy(local.rates, rs->rates, WLC_NUMRATES);

	return TRUE;
}

/* validate both rs1, rs2 and merge rs1 and rs2 and result into rs1 */
void
wlc_rateset_merge(wlc_rateset_t *rs1, wlc_rateset_t *rs2)
{
	uint8 rateset[WLC_MAXRATE+1];
	uint8 r;
	uint i;

	bzero(rateset, sizeof(rateset));

	WL_INFORM(("rs1 count is %d and rs2 ratecount is %d\n", rs1->count, rs2->count));

	/* validate both ratesets */
	for (i = 0; i < rs1->count; i++) {
		/* mask off "basic rate" bit, WLC_RATE_FLAG */
		r = (int)rs1->rates[i] & RATE_MASK;
		if ((r > WLC_MAXRATE) || (rate_info[r] == 0)) {
			WL_INFORM(("%s: bad rate in rs1 rate set 0x%x\n", __FUNCTION__, r));
			continue;
		}
		rateset[r] |= rs1->rates[i]; /* preserve basic bit! */
	}

	for (i = 0; i < rs2->count; i++) {
		/* mask off "basic rate" bit, WLC_RATE_FLAG */
		r = (int)rs2->rates[i] & RATE_MASK;
		if ((r > WLC_MAXRATE) || (rate_info[r] == 0)) {
			WL_INFORM(("%s: bad rate in rs2 rate set 0x%x\n", __FUNCTION__, r));
			continue;
		}
		rateset[r] |= rs2->rates[i]; /* preserve basic bit! */
	}

	/* merge it now */
	rs1->count = 0;
	for (i = 0; i < WLC_MAXRATE+1; i++) {
		if (rateset[i]) {
			rs1->rates[rs1->count] = rateset[i];
			rs1->count++;
			if (rs1->count > WLC_NUMRATES) {
				WL_ERROR(("%s: number of rates %d can't be more than %d\n",
					__FUNCTION__, rs1->count, WLC_NUMRATES));
				ASSERT(0);
				return;
			}
		}
	}
}

/* filter based on hardware rateset, and sort filtered rateset with basic bit(s) preserved,
 * and check if resulting rateset is valid.
*/
bool
wlc_rate_hwrs_filter_sort_validate(wlc_rateset_t *rs, const wlc_rateset_t *hw_rs, bool check_brate,
	uint8 txstreams)
{
	uint8 rateset[WLC_MAXRATE+1];
	uint8 r;
	uint count;
	uint i;

	bzero(rateset, sizeof(rateset));
	count = rs->count;

	for (i = 0; i < count; i++) {
		/* mask off "basic rate" bit, WLC_RATE_FLAG */
		r = (int)rs->rates[i] & RATE_MASK;
		if ((r > WLC_MAXRATE) || (rate_info[r] == 0)) {
			WL_INFORM(("wlc_rate_hwrs_filter_sort: bad rate in rate set 0x%x\n", r));
			continue;
		}
		rateset[r] = rs->rates[i]; /* preserve basic bit! */
	}

	/* fill out the rates in order, looking at only supported rates */
	count = 0;
	for (i = 0; i < hw_rs->count; i++) {
		r = hw_rs->rates[i] & RATE_MASK;
		ASSERT(r <= WLC_MAXRATE);
		if (rateset[r])
			rs->rates[count++] = rateset[r];
	}

	rs->count = count;

#ifdef WL11N
	/* only set the mcs rate bit if the equivalent hw mcs bit is set */
	for (i = 0; i < MCSSET_LEN; i++)
		rs->mcs[i] = (rs->mcs[i] & hw_rs->mcs[i]);
#endif /* WL11N */

#ifdef WL11AC
	{
		/*
		 * Force back the hw rate set if the caller asked for more than what we can do.
		 */
		for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
			uint8 mcs, hwrs_mcs;
			mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, rs->vht_mcsmap);
			hwrs_mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, hw_rs->vht_mcsmap);
			if ((mcs != VHT_CAP_MCS_MAP_NONE) && (mcs > hwrs_mcs)) {
				VHT_MCS_MAP_SET_MCS_PER_SS(i, hwrs_mcs, rs->vht_mcsmap);
			}
		}
	}
#endif /* WL11AC */

	if (wlc_rateset_valid(rs, check_brate))
		return TRUE;
	else
		return FALSE;
}

ratespec_t
wlc_vht_get_rspec_from_plcp(uint8 *plcp)
{
	uint8 rate;
	uint vht_sig_a1, vht_sig_a2;
	ratespec_t rspec;

	ASSERT(plcp);

	rate = (wlc_vht_get_rate_from_plcp(plcp) & 0x7f);

	vht_sig_a1 = plcp[0] | (plcp[1] << 8);
	vht_sig_a2 = plcp[3] | (plcp[4] << 8);

	rspec = VHT_RSPEC((rate & RSPEC_VHT_MCS_MASK),
		(rate >> RSPEC_VHT_NSS_SHIFT));
#if ((((VHT_SIGA1_20MHZ_VAL + 1) << RSPEC_BW_SHIFT)  != RSPEC_BW_20MHZ) || \
	(((VHT_SIGA1_40MHZ_VAL + 1) << RSPEC_BW_SHIFT)  != RSPEC_BW_40MHZ) || \
	(((VHT_SIGA1_80MHZ_VAL + 1) << RSPEC_BW_SHIFT)  != RSPEC_BW_80MHZ) || \
	(((VHT_SIGA1_160MHZ_VAL + 1) << RSPEC_BW_SHIFT)  != RSPEC_BW_160MHZ))
#error "VHT SIGA BW mapping to RSPEC BW needs correction"
#endif
	rspec |= ((vht_sig_a1 & VHT_SIGA1_160MHZ_VAL) + 1) << RSPEC_BW_SHIFT;
	if (vht_sig_a1 & VHT_SIGA1_STBC)
		rspec |= RSPEC_STBC;
	if (vht_sig_a2 & VHT_SIGA2_GI_SHORT)
		rspec |= RSPEC_SHORT_GI;
	if (vht_sig_a2 & VHT_SIGA2_CODING_LDPC)
		rspec |= RSPEC_LDPC_CODING;

	return rspec;
}

/* caluclate the rate of a rx'd frame and return it as a ratespec */
ratespec_t BCMFASTPATH
wlc_recv_compute_rspec(wlc_d11rxhdr_t *wrxh, uint8 *plcp)
{
	d11rxhdr_t *rxh = &wrxh->rxhdr;
	ratespec_t rspec;
	uint16 phy_ft;

	phy_ft = rxh->PhyRxStatus_0 & PRXS0_FT_MASK;

	switch (phy_ft) {
	case PRXS0_CCK:
		rspec = CCK_RSPEC(CCK_PHY2MAC_RATE(((cck_phy_hdr_t *)plcp)->signal));
		rspec |= RSPEC_BW_20MHZ;
		break;
	case PRXS0_OFDM:
		rspec = OFDM_RSPEC(OFDM_PHY2MAC_RATE(((ofdm_phy_hdr_t *)plcp)->rlpt[0]));
		rspec |= RSPEC_BW_20MHZ;
		break;
#ifdef WL11N
	case PRXS0_PREN: {
		uint ht_sig1, ht_sig2;
		uint8 stbc;

		ht_sig1 = plcp[0];	/* only interested in low 8 bits */
		ht_sig2 = plcp[3] | (plcp[4] << 8); /* only interestetd in low 10 bits */

		rspec = HT_RSPEC((ht_sig1 & HT_SIG1_MCS_MASK));
		if (ht_sig1 & HT_SIG1_CBW) {
			/* indicate rspec is for 40 MHz mode */
			rspec |= RSPEC_BW_40MHZ;
		} else {
			/* indicate rspec is for 20 MHz mode */
			rspec |= RSPEC_BW_20MHZ;
		}
		if (ht_sig2 & HT_SIG2_SHORT_GI)
			rspec |= RSPEC_SHORT_GI;
		if (ht_sig2 & HT_SIG2_FEC_CODING)
			rspec |= RSPEC_LDPC_CODING;
		stbc = ((ht_sig2 & HT_SIG2_STBC_MASK) >> HT_SIG2_STBC_SHIFT);
		if (stbc != 0) {
			rspec |= RSPEC_STBC;
		}
		break;
	}
	case PRXS0_STDN:
		rspec = wlc_vht_get_rspec_from_plcp(plcp);
		break;
#endif /* WL11N */
	default:
		ASSERT(0);
		/* return a valid rspec if not a debug/assert build */
		rspec = OFDM_RSPEC(6) | RSPEC_BW_20MHZ;
		break;
	}

	return rspec;
}

/* copy rateset src to dst as-is (no masking or sorting) */
void
wlc_rateset_copy(const wlc_rateset_t *src, wlc_rateset_t *dst)
{
	bcopy(src, dst, sizeof(wlc_rateset_t));
}

/*
 * Copy and selectively filter one rateset to another.
 * 'basic_only' means only copy basic rates.
 * 'rates' indicates cck (11b) and ofdm rates combinations.
 *    - 0: cck and ofdm
 *    - 1: cck only
 *    - 2: ofdm only
 * 'xmask' is the copy mask (typically 0x7f or 0xff).
 */
void
wlc_rateset_filter(wlc_rateset_t *src, wlc_rateset_t *dst, bool basic_only, uint8 rates,
	uint xmask, uint8 mcsallow)
{
	uint i;
	uint r;
	uint count;

	count = 0;
	for (i = 0; i < src->count; i++) {
		r = src->rates[i];
		if (basic_only && !(r & WLC_RATE_FLAG))
			continue;
		if ((rates == WLC_RATES_CCK) && IS_OFDM((r & RATE_MASK)))
			continue;
		if ((rates == WLC_RATES_OFDM) && IS_CCK((r & RATE_MASK)))
			continue;
		dst->rates[count++] = r & xmask;
	}
	dst->count = count;

	if (rates == WLC_RATES_OFDM && xmask == RATE_MASK_FULL)
		wlc_rateset_ofdm_fixup(dst);

	dst->htphy_membership = src->htphy_membership;

#ifdef WL11N
	/* Both WLC_MCS_ALLOW and WLC_MCS_ALLOW_VHT flags should be set
	 * for copying vht_mcsmap as this copy should not be
	 * done for non VHT devices and both HT and VHT rates
	 * need to be filled for 11ac devices.
	 */
	if ((mcsallow & WLC_MCS_ALLOW) && rates != WLC_RATES_CCK) {
		bcopy(&src->mcs[0], &dst->mcs[0], MCSSET_LEN);
#ifdef WL11AC
		if (mcsallow & WLC_MCS_ALLOW_VHT) {
			dst->vht_mcsmap = src->vht_mcsmap;
		} else {
			/* needed as VHT_CAP_MCS_MAP_NONE_ALL is not 0 */
			dst->vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
		}
#endif /* WL11AC */
	} else {
		wlc_rateset_mcs_clear(dst);
		dst->vht_mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
	}
#endif /* WL11N */
}

/* After filtering 2.4GHz hw rateset to include ofdm rates only
 * we lost basic rates info, so fixup it here.
 */
void
wlc_rateset_ofdm_fixup(wlc_rateset_t *rs)
{
	uint i;

	for (i = 0; i < rs->count; i ++) {
		uint j;
		bool basic = FALSE;

		for (j = 0; j < ofdm_rates.count; j ++) {
			if ((rs->rates[i] & RATE_MASK) == (ofdm_rates.rates[j] & RATE_MASK)) {
				if (ofdm_rates.rates[j] & WLC_RATE_FLAG)
					basic = TRUE;
				break;
			}
		}
		if (basic)
			rs->rates[i] |= WLC_RATE_FLAG;
	}
}

/* select rateset for a given phy_type and bandtype and filter it, sort it
 * and fill rs_tgt with result
 */
void
wlc_rateset_default(wlc_rateset_t *rs_tgt, const wlc_rateset_t *rs_hw,
	uint phy_type, int bandtype, bool cck_only, uint rate_mask,
	uint8 mcsallow, uint8 bw, uint8 txstreams, uint8 vht_ratemask)
{
	const wlc_rateset_t *rs_dflt;
	wlc_rateset_t rs_sel;

	if ((PHYTYPE_IS(phy_type, PHY_TYPE_HT)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_N)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_LCN)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_LCN40)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_AC)) ||
	    (PHYTYPE_IS(phy_type, PHY_TYPE_SSN))) {
		if (BAND_5G(bandtype)) {
			rs_dflt = (bw == WLC_20_MHZ ?
				&ofdm_mimo_rates : &ofdm_40bw_mimo_rates);
		} else {
			rs_dflt = (bw == WLC_20_MHZ ?
				&cck_ofdm_mimo_rates : &cck_ofdm_40bw_mimo_rates);
		}
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_LP)) {
		rs_dflt = (BAND_5G(bandtype)) ? &ofdm_rates : &cck_ofdm_rates;
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_A)) {
		rs_dflt = &ofdm_rates;
	} else if (PHYTYPE_IS(phy_type, PHY_TYPE_G)) {
		rs_dflt = &cck_ofdm_rates;
	} else {
		ASSERT(0); /* should not happen */
		rs_dflt = &cck_rates; /* force cck */
	}

	/* if hw rateset is not supplied, assign selected rateset to it */
	if (!rs_hw)
		rs_hw = rs_dflt;

	wlc_rateset_copy(rs_hw, &rs_sel);
#ifdef WL11N
	wlc_rateset_mcs_upd(&rs_sel, txstreams);
#endif
#ifdef WL11AC
	wlc_rateset_vhtmcs_upd(&rs_sel, txstreams);
#endif
	wlc_rateset_filter(&rs_sel, rs_tgt, FALSE,
	                   cck_only ? WLC_RATES_CCK : WLC_RATES_CCK_OFDM,
	                   rate_mask, mcsallow);
	wlc_rate_hwrs_filter_sort_validate(rs_tgt, rs_hw, FALSE,
	                                   mcsallow ? txstreams : 1);
}

int16 BCMFASTPATH
wlc_rate_legacy_phyctl(uint rate)
{
	uint i;

	for (i = 0; i < LEGACY_PHYCFG_TABLE_SIZE; i++)
		if (rate == legacy_phycfg_table[i].rate_ofdm)
			return legacy_phycfg_table[i].tx_phy_ctl3;

	return -1;
}

int
wlc_dump_rateset(const char *name, wlc_rateset_t *rateset, struct bcmstrbuf *b)
{
	uint r, i;
	bool bl;

	bcm_bprintf(b, "%s [ ", name ? name : "");
	for (i = 0; i < rateset->count; i++) {
		r = rateset->rates[i] & RATE_MASK;
		bl = rateset->rates[i] & WLC_RATE_FLAG;
		bcm_bprintf(b, "%d%s%s ", (r / 2), (r % 2) ? ".5" : "", bl ? "(b)" : "");
	}
	bcm_bprintf(b, "]");

	return 0;
}

int
wlc_dump_mcsset(const char *name, uchar *mcs, struct bcmstrbuf *b)
{
	int i;

	bcm_bprintf(b, "%s ", name ? name : "");
	for (i = 0; i < MCSSET_LEN; i++)
		bcm_bprintf(b, "%02x ", *mcs++);

	return 0;
}

#ifdef WL11AC
/*
 * Dump the VHT mode mcsmap. Pass the map in wlc_rateset_t format, using rateset_vht_map_t items.
 */
int
wlc_dump_vht_mcsmap(const char *name, uint16 vht_mcsmap, struct bcmstrbuf *b)
{
	int nss;

	bcm_bprintf(b, "%s ", name ? name : "");
	for (nss = 1; nss <= VHT_CAP_MCS_MAP_NSS_MAX; nss++) {
		bcm_bprintf(b, "%04x ",
		    VHT_MCS_CODE_TO_MCS_MAP(VHT_MCS_MAP_GET_MCS_PER_SS(nss, vht_mcsmap)));
	}
	return 0;

}

/* valid mcsmaps for bw=20MHZ */
const uint16 vht_mcsmap_20mhz[VHT_CAP_MCS_MAP_NSS_MAX] = {
	0x01ff, 0x01ff, 0x3ff, 0x01ff, 0x01ff, 0x03ff, 0x01ff, 0x01ff
};
/* valid mcsmaps for bw=80MHZ */
const uint16 vht_mcsmap_80mhz[VHT_CAP_MCS_MAP_NSS_MAX] = {
	0x03ff, 0x03ff, 0x3bf, 0x03ff, 0x03ff, 0x01ff, 0x023b, 0x037f
};

/*
 * This function returns a valid mcsmap (in wlc_rateset_t form) for the given bw and nss.
 * Inputs:
 *     bw: bandwidth
 *     nss: number of streams
 */
static uint16
wlc_get_valid_vht_mcs_map(uint8 bw, uint8 nss, uint8 ratemask)
{
	uint16 mcsmap = 0x3ff;

	ASSERT((nss >= 1) && (nss <= VHT_CAP_MCS_MAP_NSS_MAX));
	nss = LIMIT_TO_RANGE(nss, 1, VHT_CAP_MCS_MAP_NSS_MAX);

	if ((bw == BW_20MHZ) &&
		!(ratemask &(WL_VHT_FEATURES_2G_20M|WL_VHT_FEATURES_5G_20M))) {
		mcsmap = vht_mcsmap_20mhz[nss - 1];
	} else if ((bw == BW_80MHZ)&& !(ratemask & (WL_VHT_FEATURES_5G_80M))) {
		mcsmap = vht_mcsmap_80mhz[nss - 1];
	}
	return mcsmap;
}

/*
 * This function returns a valid mcsmap (in wlc_rateset_t form) for the given
 *  mcs_code, bw, and nss.
 * Inputs:
 *     bw: bandwidth
 *     nss: number of streams
 */
uint16
wlc_get_valid_vht_mcsmap(uint8 mcs_code, uint8 bw, bool ldpc, uint8 nss, uint8 ratemask)
{
	uint16 mcsmap;

	ratemask = ldpc ? ratemask : 0;

	mcsmap = wlc_get_valid_vht_mcs_map(bw, nss, ratemask);
	if (mcs_code == VHT_CAP_MCS_MAP_0_7) {
		mcsmap &= 0xff;
	} else if (mcs_code == VHT_CAP_MCS_MAP_0_8) {
		mcsmap &= 0x1ff;
	} else if (mcs_code == VHT_CAP_MCS_MAP_0_9) {
		mcsmap &= 0x3ff;
	} else {
		mcsmap = 0;
	}

	return mcsmap;
}

/*
 * This function returns true/false based on whether given
 *  mcs/nss/bw combination is valid or not.
 * Inputs:
 *     mcs: MCS index
 *     bw: bandwidth
 *     nss: number of streams
 */
bool
wlc_valid_vht_mcs(uint8 mcs, uint8 nss, uint8 bw, uint8 ratemask)
{
	if (((bw == BW_20MHZ) || (bw == BW_40MHZ) || (bw == BW_80MHZ)) &&
		(nss >= 1) && (nss <= VHT_CAP_MCS_MAP_NSS_MAX) && (mcs <= 9)) {
		uint16 mcsmap = wlc_get_valid_vht_mcs_map(bw, nss, ratemask);
		if (mcsmap & (1 << mcs))
			return TRUE;
	}

	return FALSE;
}

/*
 * This function returns the highest supported MCS index.
 * Inputs:
 *     mcs_code: code indicating which MCS set is used
 *     bw: bandwidth
 *     nss: number of streams
 * Returns: Highest and valid MCS index
 */
static uint8
wlc_get_highest_mcs_index(uint8 mcs_code, uint8 bw, uint8 nss, bool ldpc, uint8 ratemask)
{
	uint8 mcs_indx = 0;

	/* Proprietary rates only of use if ldpc is in use */
	ratemask = ldpc ? ratemask : 0;
	if (mcs_code == VHT_CAP_MCS_MAP_0_7) {
		mcs_indx = 7;
	} else if (mcs_code == VHT_CAP_MCS_MAP_0_8) {
		mcs_indx = 8;
	} else if (mcs_code == VHT_CAP_MCS_MAP_0_9) {
		mcs_indx = 9;
	}

	/* Find the highest valid index by checking from highest index.
	 * OK to check the params few times in the calls to
	 * wlc_valid_vht_mcs() as this is control path and allows
	 * code reuse.
	 */
	for (; mcs_indx > 0; mcs_indx--) {
		if (wlc_valid_vht_mcs(mcs_indx, nss, bw, ratemask))
			break;
	}

	return mcs_indx;
}
#endif /* WL11AC */

/*
 * This function returns the highest rate based on the info from rateset, bw,
 * sgi, and max number of txstreams.
 * Inputs:
 *     rateset: pointer to rateset info. This info includes the HT and VHT
 *        capabilites.
 *     bw: bandwidth
 *     sgi: is short gaurd interval used
 *     txstreams: max number of txstreams supported
 * Returns: ratespec_t for the highest rate determined.
 */
ratespec_t
wlc_get_highest_rate(wlc_rateset_t *rateset, uint8 bw, bool sgi,
	bool ldpc, uint8 vht_ratemask, uint8 txstreams)
{
	ratespec_t reprate;
	uint rate;
#ifdef WL11N
	ratespec_t mcs_reprate;
	int8 i, bit;
	uint8 mcs_indx;
	uint max_rate;
#endif /* WL11N */

	WL_TRACE(("%s inputs: vht_mcsmap = 0x%x; bw = %d; sgi = %d; txstreams = %d \n",
	         __FUNCTION__, rateset->vht_mcsmap, bw, sgi, txstreams));

	rate = rateset->rates[rateset->count - 1] & RATE_MASK;
	reprate = LEGACY_RSPEC(rate);

#ifdef WL11N
	max_rate = rate * 500;

	/* find the maximum rate based on mcs bit vector */
	for (i = 0; i < MCSSET_LEN; i++) {
		if (rateset->mcs[i] == 0)
			continue;
		if ((txstreams == 1) && (i > 0))
			break;
		if ((txstreams == 2) && (i > 1))
			break;
		for (bit = 0; bit < NBBY; bit++) {
			mcs_indx = (NBBY * i) + bit;
			if (mcs_indx > WLC_MAXMCS)
				break;
			mcs_reprate = mcs_indx | RSPEC_ENCODE_HT;

			if (bw == BW_20MHZ)
				mcs_reprate |= RSPEC_BW_20MHZ;
			else
				mcs_reprate |= RSPEC_BW_40MHZ;

			if (sgi)
				mcs_reprate |= RSPEC_SHORT_GI;

			rate = RSPEC2RATE(mcs_reprate);

			if (rate > max_rate) {
				max_rate = rate;
				reprate = mcs_reprate;
			}
		}
	}

#ifdef WL11AC
	if (rateset->vht_mcsmap != VHT_CAP_MCS_MAP_NONE_ALL) {
		uint8 nss;
		uint8 mcs_code;
		txstreams = MIN(txstreams, VHT_CAP_MCS_MAP_NSS_MAX);
		for (nss = 1; nss <= txstreams; nss++) {
			mcs_code = VHT_MCS_MAP_GET_MCS_PER_SS(nss, vht_ratemask);
			if (mcs_code == VHT_CAP_MCS_MAP_NONE) {
				/* continue to next stream */
				continue;
			}
			mcs_indx = wlc_get_highest_mcs_index(mcs_code, bw, nss, ldpc, vht_ratemask);
			mcs_reprate = mcs_indx | RSPEC_ENCODE_VHT;
			mcs_reprate |= (nss << RSPEC_VHT_NSS_SHIFT);
			if (bw == BW_80MHZ)
				mcs_reprate |= RSPEC_BW_80MHZ;
			else if (bw == BW_40MHZ)
				mcs_reprate |= RSPEC_BW_40MHZ;
			else
				mcs_reprate |= RSPEC_BW_20MHZ;

			if (sgi)
				mcs_reprate |= RSPEC_SHORT_GI;

			rate = RSPEC2RATE(mcs_reprate);

			if (rate > max_rate) {
				max_rate = rate;
				reprate = mcs_reprate;
			}
		}
	}
#endif /* WL11AC */
#endif /* WL11N */

	WL_TRACE(("%s outputs: reprate = 0x%x; rate = %d \n",
		__FUNCTION__, (uint32)reprate, (uint32)rate));

	return reprate;
}

#ifdef WL11N
void
wlc_rateset_mcs_clear(wlc_rateset_t *rateset)
{
	uint i;

	for (i = 0; i < MCSSET_LEN; i++)
		rateset->mcs[i] = 0;
}

void
wlc_rateset_mcs_build(wlc_rateset_t *rateset, uint8 nstreams)
{
	bcopy(&cck_ofdm_mimo_rates.mcs[0], &rateset->mcs[0], MCSSET_LEN);
	wlc_rateset_mcs_upd(rateset, nstreams);
}

/* Based on bandwidth passed, allow/disallow MCS 32 in the rateset */
void
wlc_rateset_bw_mcs_filter(wlc_rateset_t *rateset, uint8 bw)
{
	if (bw == WLC_40_MHZ)
		setbit(rateset->mcs, 32);
	else
		clrbit(rateset->mcs, 32);
}

/* convert mcs from ht format to vht format 0xAB : A = nss, B = mcs
 * input mcs format:
 *   = 0xff (MCS_INVALID): not a valid mcs
 *   bit 7 = 1 : vht, already using 0xAB format
 *   bit 7 = 0 :  ht, mcs23 --> hex 0x37
*/
uint8
wlc_rate_ht2vhtmcs(uint8 mcs)
{
	if (mcs != MCS_INVALID) {
		if (mcs & 0x80)
			/* is vht */
			mcs &= 0x7f;
		else {
			/* is ht */
			uint8 nss = ((mcs & RSPEC_HT_NSS_MASK) >> RSPEC_HT_NSS_SHIFT) + 1;
			mcs = (mcs & RSPEC_HT_MCS_MASK) | (nss << RSPEC_VHT_NSS_SHIFT);
		}
	}
	return mcs;
}

#ifdef WL11AC
void
wlc_rateset_vhtmcs_build(wlc_rateset_t *rateset, uint8 nstreams)
{
	rateset->vht_mcsmap = cck_ofdm_mimo_rates.vht_mcsmap;
	wlc_rateset_vhtmcs_upd(rateset, nstreams);
}
#endif /* WL11AC */
#endif /* WL11N */
