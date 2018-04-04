/*
 * Common interface to channel definitions.
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_channel.c 381850 2013-01-30 03:29:22Z $
 */


#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/wpa.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <epivers.h>
#ifdef BCMSUP_PSK
#include <proto/eapol.h>
#include <bcmwpa.h>
#endif /* BCMSUP_PSK */
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_phy_hal.h>
#include <wl_export.h>
#include <wlc_stf.h>
#include <wlc_channel.h>
#include "wlc_clm_data.h"
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_dfs.h>
#include <wlc_11d.h>
#include <wlc_cntry.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>
#ifdef DSLCPE
extern int requestRegRev;
#endif
typedef struct wlc_cm_band {
	uint8		locale_flags;		/* locale_info_t flags */
	chanvec_t	valid_channels;		/* List of valid channels in the country */
	chanvec_t	*radar_channels;	/* List of radar sensitive channels */
	struct wlc_channel_txchain_limits chain_limits;	/* per chain power limit */
	uint8		PAD[4];
} wlc_cm_band_t;

struct wlc_cm_info {
	wlc_pub_t	*pub;
	wlc_info_t	*wlc;
	char		srom_ccode[WLC_CNTRY_BUF_SZ];	/* Country Code in SROM */
	uint		srom_regrev;			/* Regulatory Rev for the SROM ccode */
	clm_country_t country;			/* Current country iterator for the CLM data */
	char		ccode[WLC_CNTRY_BUF_SZ];	/* current internal Country Code */
	uint		regrev;				/* current Regulatory Revision */
	char		country_abbrev[WLC_CNTRY_BUF_SZ];	/* current advertised ccode */
	wlc_cm_band_t	bandstate[MAXBANDS];	/* per-band state (one per phy/radio) */
	/* quiet channels currently for radar sensitivity or 11h support */
	chanvec_t	quiet_channels;		/* channels on which we cannot transmit */

	struct clm_data_header* clm_base_dataptr;
	int clm_base_data_len;

	void* clm_inc_dataptr;
	void* clm_inc_headerptr;
	int clm_inc_data_len;

	/* List of radar sensitive channels for the current locale */
	chanvec_t locale_radar_channels;

	/* restricted channels */
	chanvec_t	restricted_channels; 	/* copy of the global restricted channels of the */
						/* current local */
	bool		has_restricted_ch;

	/* regulatory class */
	rcvec_t		valid_rcvec;		/* List of valid regulatory class in the country */
	const rcinfo_t	*rcinfo_list[MAXRCLIST];	/* regulatory class info list */
	bool		bandedge_filter_apply;
	bool		sar_enable;		/* Use SAR as part of regulatory power calc */
#ifdef WL_SARLIMIT
	sar_limit_t	sarlimit;		/* sar limit per band/sub-band */
#endif
};

static int wlc_channels_init(wlc_cm_info_t *wlc_cm, clm_country_t country);
static void wlc_set_country_common(
	wlc_cm_info_t *wlc_cm, const char* country_abbrev, const char* ccode, uint regrev,
	clm_country_t country);
static int wlc_country_aggregate_map(
	wlc_cm_info_t *wlc_cm, const char *ccode, char *mapped_ccode, uint *mapped_regrev);
static clm_result_t wlc_countrycode_map(wlc_cm_info_t *wlc_cm, const char *ccode,
	char *mapped_ccode, uint *mapped_regrev, clm_country_t *country);
static void wlc_channels_commit(wlc_cm_info_t *wlc_cm);
static void wlc_chanspec_list(wlc_info_t *wlc, wl_uint32_list_t *list, chanspec_t chanspec_mask);
static bool wlc_buffalo_map_locale(struct wlc_info *wlc, const char* abbrev);
static bool wlc_japan_ccode(const char *ccode);
static bool wlc_us_ccode(const char *ccode);
#ifndef PPR_API
static void wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm_info_t *wlc_cm,
	txppr_t *txpwr, uint8 local_constraint_qdbm);
#endif
static void wlc_rcinfo_init(wlc_cm_info_t *wlc_cm);
static void wlc_regclass_vec_init(wlc_cm_info_t *wlc_cm);
static void wlc_upd_restricted_chanspec_flag(wlc_cm_info_t *wlc_cm);
#ifdef PPR_API
static int wlc_channel_update_txchain_offsets(wlc_cm_info_t *wlc_cm, ppr_t *txpwr);
#else
static int wlc_channel_update_txchain_offsets(wlc_cm_info_t *wlc_cm, txppr_t *txpwr);
#endif
#if defined(WL_SARLIMIT) && defined(BCMDBG)
static void wlc_channel_sarlimit_dump(wlc_cm_info_t *wlc_cm, sar_limit_t *sar);
#endif 
#if defined(BCMDBG)
static int wlc_channel_dump_reg_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_reg_local_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_srom_ppr(void *handle, struct bcmstrbuf *b);
static int wlc_channel_dump_margin(void *handle, struct bcmstrbuf *b);

static int wlc_dump_max_power_per_channel(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_2G_20M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_2G_40M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_2G_20in40M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_5G_20M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_5G_40M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);
static int wlc_dump_clm_limits_5G_20in40M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);

static int wlc_dump_country_aggregate_map(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);
static int wlc_channel_supported_country_regrevs(void *handle, struct bcmstrbuf *b);

const char fraction[4][4] = {"   ", ".25", ".5 ", ".75"};
#define QDB_FRAC(x)	(x) / WLC_TXPWR_DB_FACTOR, fraction[(x) % WLC_TXPWR_DB_FACTOR]
#define QDB_FRAC_TRUNC(x)	(x) / WLC_TXPWR_DB_FACTOR, \
	((x) % WLC_TXPWR_DB_FACTOR) ? fraction[(x) % WLC_TXPWR_DB_FACTOR] : ""
#endif 

#define	COPY_LIMITS(src, index, dst, cnt)	\
		bcopy(&src.limit[index], txpwr->dst, cnt)
#define	COPY_DSSS_LIMS(src, index, dst)	\
		bcopy(&src.limit[index], txpwr->dst, WL_NUM_RATES_CCK)
#define	COPY_OFDM_LIMS(src, index, dst)	\
		bcopy(&src.limit[index], txpwr->dst, WL_NUM_RATES_OFDM)
#define	COPY_MCS_LIMS(src, index, dst)	\
		bcopy(&src.limit[index], txpwr->dst, WL_NUM_RATES_MCS_1STREAM)
#ifdef WL11AC
#define	COPY_VHT_LIMS(src, index, dst)	\
		bcopy(&src.limit[index], txpwr->dst, WL_NUM_RATES_EXTRA_VHT)
#else
#define	COPY_VHT_LIMS(src, index, dst)
#endif

#ifdef PPR_API
#define CLM_DSSS_RATESET(src) ((const ppr_dsss_rateset_t*)&src.limit[WL_RATE_1X1_DSSS_1])
#define CLM_OFDM_1X1_RATESET(src) ((const ppr_ofdm_rateset_t*)&src.limit[WL_RATE_1X1_OFDM_6])
#define CLM_MCS_1X1_RATESET(src) ((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_1X1_MCS0])

#define CLM_DSSS_1X2_MULTI_RATESET(src) \
	((const ppr_dsss_rateset_t*)&src.limit[WL_RATE_1X2_DSSS_1])
#define CLM_OFDM_1X2_CDD_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src.limit[WL_RATE_1X2_CDD_OFDM_6])
#define CLM_MCS_1X2_CDD_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_1X2_CDD_MCS0])

#define CLM_DSSS_1X3_MULTI_RATESET(src) \
	((const ppr_dsss_rateset_t*)&src.limit[WL_RATE_1X3_DSSS_1])
#define CLM_OFDM_1X3_CDD_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src.limit[WL_RATE_1X3_CDD_OFDM_6])
#define CLM_MCS_1X3_CDD_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_1X3_CDD_MCS0])

#define CLM_MCS_2X2_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_2X2_SDM_MCS8])
#define CLM_MCS_2X2_STBC_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_2X2_STBC_MCS0])

#define CLM_MCS_2X3_STBC_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_2X3_STBC_MCS0])
#define CLM_MCS_2X3_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_2X3_SDM_MCS8])

#define CLM_MCS_3X3_SDM_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_3X3_SDM_MCS16])

#define CLM_OFDM_1X2_TXBF_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src.limit[WL_RATE_1X2_TXBF_OFDM_6])
#define CLM_MCS_1X2_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_1X2_TXBF_MCS0])
#define CLM_OFDM_1X3_TXBF_RATESET(src) \
	((const ppr_ofdm_rateset_t*)&src.limit[WL_RATE_1X3_TXBF_OFDM_6])
#define CLM_MCS_1X3_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_1X3_TXBF_MCS0])
#define CLM_MCS_2X2_TXBF_RATESET(src) \
	((const ppr_ht_mcs_rateset_t*)&src.limit[WL_RATE_2X2_TXBF_SDM_MCS8])
#define CLM_MCS_2X3_TXBF_RATESET(src) \
	((const ppr_vht_mcs_rateset_t*)&src.limit[WL_RATE_2X3_TXBF_SDM_MCS8])
#define CLM_MCS_3X3_TXBF_RATESET(src) \
	((const ppr_ht_mcs_rateset_t*)&src.limit[WL_RATE_3X3_TXBF_SDM_MCS16])
#endif /* PPR_API */

clm_result_t clm_aggregate_country_lookup(const ccode_t cc, unsigned int rev,
	clm_agg_country_t *agg);
clm_result_t clm_aggregate_country_map_lookup(const clm_agg_country_t agg,
	const ccode_t target_cc, unsigned int *rev);

static clm_result_t clm_power_limits(
	const clm_country_locales_t *locales, clm_band_t band,
	unsigned int chan, int ant_gain, clm_limits_type_t limits_type,
	const clm_limits_params_t *params, clm_power_limits_t *limits);

#ifndef PPR_API
typedef void (*wlc_channel_mapfn_t)(void *context, uint8 *a, uint8 *b);
#endif

/* QDB() macro takes a dB value and converts to a quarter dB value */
#ifdef QDB
#undef QDB
#endif
#define QDB(n) ((n) * WLC_TXPWR_DB_FACTOR)

/* Regulatory Matrix Spreadsheet (CLM) MIMO v3.8.6.4
 * + CLM v4.1.3
 * + CLM v4.2.4
 * + CLM v4.3.1 (Item-1 only EU/9 and Q2/4).
 * + CLM v4.3.4_3x3 changes(Skip changes for a13/14).
 * + CLMv 4.5.3_3x3 changes for Item-5(Cisco Evora (change AP3500i to Evora)).
 * + CLMv 4.5.3_3x3 changes for Item-3(Create US/61 for BCM94331HM, based on US/53 power levels).
 * + CLMv 4.5.5 3x3 (changes from Create US/63 only)
 * + CLMv 4.4.4 3x3 changes(Create TR/4 (locales Bn7, 3tn), EU/12 (locales 3s, 3sn) for Airties.)
 */

/*
 * Some common channel sets
 */

/* All 2.4 GHz HW channels */
const chanvec_t chanvec_all_2G = {
	{0xfe, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00}
};

/* All 5 GHz HW channels */
const chanvec_t chanvec_all_5G = {
	/* 35,36,38/40,42,44,46/48,52/56,60 */
	{0x00, 0x00, 0x00, 0x00, 0x54, 0x55, 0x11, 0x11,
	/* 64/-/-/-/100/104,108/112,116/120,124 */
	0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11,
#ifdef WL11AC
	/* /128,132/136,140/144,149/153,157/161,165... */
	0x11, 0x11, 0x21, 0x22, 0x22, 0x00, 0x00, 0x11,
#else
	/* /128,132/136,140/149/153,157/161,165... */
	0x11, 0x11, 0x20, 0x22, 0x22, 0x00, 0x00, 0x11,
#endif
	0x11, 0x11, 0x11, 0x01}
};

/*
 * Radar channel sets
 */

#ifdef BAND5G
static const chanvec_t radar_set1 = { /* Channels 52 - 64, 100 - 140 */
	{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x11,	/* 52 - 60 */
	0x01, 0x00, 0x00, 0x00, 0x10, 0x11, 0x11, 0x11, 	/* 64, 100 - 124 */
	0x11, 0x11, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,		/* 128 - 140 */
	0x00, 0x00, 0x00, 0x00}
};
#endif	/* BAND5G */

/*
 * Restricted channel sets
 */

#define WLC_REGCLASS_USA_2G_20MHZ	12
#define WLC_REGCLASS_EUR_2G_20MHZ	4
#define WLC_REGCLASS_JPN_2G_20MHZ	30
#define WLC_REGCLASS_JPN_2G_20MHZ_CH14	31

#ifdef WL11N
/*
 * bit map of supported regulatory class for USA, Europe & Japan
 */
static const rcvec_t rclass_us = { /* 1-5, 12, 22-25, 27-30, 32-33 */
	{0x3e, 0x10, 0xc0, 0x7b, 0x03}
};
static const rcvec_t rclass_eu = { /* 1-4, 5-12 */
	{0xfe, 0x1f, 0x00, 0x00, 0x00}
};
static const rcvec_t rclass_jp = { /* 1, 30-32 */
	{0x01, 0x00, 0x00, 0xc0, 0x01}
};
#endif /* WL11N */

#ifdef BAND5G
/*
 * channel to regulatory class map for USA
 */
static const rcinfo_t rcinfo_us_20 = {
	24,
	{
	{ 36,  1}, { 40,  1}, { 44,  1}, { 48,  1}, { 52,  2}, { 56,  2}, { 60,  2}, { 64,  2},
	{100,  4}, {104,  4}, {108,  4}, {112,  4}, {116,  4}, {120,  4}, {124,  4}, {128,  4},
	{132,  4}, {136,  4}, {140,  4}, {149,  3}, {153,  3}, {157,  3}, {161,  3}, {165,  5}
	}
};
#endif /* BAND5G */

#ifdef WL11N
/* control channel at lower sb */
static const rcinfo_t rcinfo_us_40lower = {
	18,
	{
	{  1, 32}, {  2, 32}, {  3, 32}, {  4, 32}, {  5, 32}, {  6, 32}, {  7, 32}, { 36, 22},
	{ 44, 22}, { 52, 23}, { 60, 23}, {100, 24}, {108, 24}, {116, 24}, {124, 24}, {132, 24},
	{149, 25}, {157, 25}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
/* control channel at upper sb */
static const rcinfo_t rcinfo_us_40upper = {
	18,
	{
	{  5, 33}, {  6, 33}, {  7, 33}, {  8, 33}, {  9, 33}, { 10, 33}, { 11, 33}, { 40, 27},
	{ 48, 27}, { 56, 28}, { 64, 28}, {104, 29}, {112, 29}, {120, 29}, {128, 29}, {136, 29},
	{153, 30}, {161, 30}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif /* WL11N */

#ifdef BAND5G
/*
 * channel to regulatory class map for Europe
 */
static const rcinfo_t rcinfo_eu_20 = {
	19,
	{
	{ 36,  1}, { 40,  1}, { 44,  1}, { 48,  1}, { 52,  2}, { 56,  2}, { 60,  2}, { 64,  2},
	{100,  3}, {104,  3}, {108,  3}, {112,  3}, {116,  3}, {120,  3}, {124,  3}, {128,  3},
	{132,  3}, {136,  3}, {140,  3}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif /* BAND5G */

#ifdef WL11N
static const rcinfo_t rcinfo_eu_40lower = {
	18,
	{
	{  1, 11}, {  2, 11}, {  3, 11}, {  4, 11}, {  5, 11}, {  6, 11}, {  7, 11}, {  8, 11},
	{  9, 11}, { 36,  5}, { 44,  5}, { 52,  6}, { 60,  6}, {100,  7}, {108,  7}, {116,  7},
	{124,  7}, {132,  7}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
static const rcinfo_t rcinfo_eu_40upper = {
	18,
	{
	{  5, 12}, {  6, 12}, {  7, 12}, {  8, 12}, {  9, 12}, { 10, 12}, { 11, 12}, { 12, 12},
	{ 13, 12}, { 40,  8}, { 48,  8}, { 56,  9}, { 64,  9}, {104, 10}, {112, 10}, {120, 10},
	{128, 10}, {136, 10}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif /* WL11N */

#ifdef BAND5G
/*
 * channel to regulatory class map for Japan
 */
static const rcinfo_t rcinfo_jp_20 = {
	8,
	{
	{ 34,  1}, { 38,  1}, { 42,  1}, { 46,  1}, { 52, 32}, { 56, 32}, { 60, 32}, { 64, 32},
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0},
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif /* BAND5G */

#ifdef WL11N
static const rcinfo_t rcinfo_jp_40 = {
	0,
	{
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0},
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0},
	{  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}, {  0,  0}
	}
};
#endif


clm_result_t
wlc_locale_get_channels(clm_country_locales_t *locales, clm_band_t band,
	chanvec_t *channels, chanvec_t *restricted)
{
	bzero(channels, sizeof(chanvec_t));
	bzero(restricted, sizeof(chanvec_t));

	return clm_country_channels(locales, band, (clm_channels_t *)channels,
		(clm_channels_t *)restricted);
}

clm_result_t wlc_get_flags(clm_country_locales_t *locales, clm_band_t band, uint8 *flags)
{
	unsigned long clm_flags = 0;

	clm_result_t result = clm_country_flags(locales, band, &clm_flags);

	*flags = 0;
	if (result == CLM_RESULT_OK) {
		switch (clm_flags & CLM_FLAG_DFS_MASK) {
		case CLM_FLAG_DFS_EU:
			*flags |= WLC_DFS_EU;
			break;
		case CLM_FLAG_DFS_US:
			*flags |= WLC_DFS_FCC;
			break;
		case CLM_FLAG_DFS_NONE:
		default:
			break;
		}

		if (clm_flags & CLM_FLAG_FILTWAR1)
			*flags |= WLC_FILT_WAR;

		if (clm_flags & CLM_FLAG_TXBF)
			*flags |= WLC_TXBF;

		if (clm_flags & CLM_FLAG_NO_MIMO)
			*flags |= WLC_NO_MIMO;
		else {
			if (clm_flags & CLM_FLAG_NO_40MHZ)
				*flags |= WLC_NO_40MHZ;
			if (clm_flags & CLM_FLAG_NO_80MHZ)
				*flags |= WLC_NO_80MHZ;
		}

		if ((band == CLM_BAND_2G) && (clm_flags & CLM_FLAG_HAS_DSSS_EIRP))
			*flags |= WLC_EIRP;
		if ((band == CLM_BAND_5G) && (clm_flags & CLM_FLAG_HAS_OFDM_EIRP))
			*flags |= WLC_EIRP;
	}

	return result;
}


clm_result_t wlc_get_locale(clm_country_t country, clm_country_locales_t *locales)
{
	return clm_country_def(country, locales);
}


/* 20MHz channel info for 40MHz pairing support */

struct chan20_info {
	uint8	sb;
	uint8	adj_sbs;
};

/* indicates adjacent channels that are allowed for a 40 Mhz channel and
 * those that permitted by the HT
 */
const struct chan20_info chan20_info[] = {
	/* 11b/11g */
/* 0 */		{1,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 1 */		{2,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 2 */		{3,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 3 */		{4,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 4 */		{5,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 5 */		{6,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 6 */		{7,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 7 */		{8,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 8 */		{9,	(CH_UPPER_SB | CH_LOWER_SB | CH_EWA_VALID)},
/* 9 */		{10,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 10 */	{11,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 11 */	{12,	(CH_LOWER_SB)},
/* 12 */	{13,	(CH_LOWER_SB)},
/* 13 */	{14,	(CH_LOWER_SB)},

/* 11a japan high */
/* 14 */	{34,	(CH_UPPER_SB)},
/* 15 */	{38,	(CH_LOWER_SB)},
/* 16 */	{42,	(CH_LOWER_SB)},
/* 17 */	{46,	(CH_LOWER_SB)},

/* 11a usa low */
/* 18 */	{36,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 19 */	{40,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 20 */	{44,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 21 */	{48,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 22 */	{52,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 23 */	{56,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 24 */	{60,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 25 */	{64,	(CH_LOWER_SB | CH_EWA_VALID)},

/* 11a Europe */
/* 26 */	{100,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 27 */	{104,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 28 */	{108,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 29 */	{112,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 30 */	{116,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 31 */	{120,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 32 */	{124,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 33 */	{128,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 34 */	{132,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 35 */	{136,	(CH_LOWER_SB | CH_EWA_VALID)},

#ifdef WL11AC
/* 36 */	{140,   (CH_UPPER_SB | CH_EWA_VALID)},
/* 37 */	{144,   (CH_LOWER_SB)},

/* 11a usa high, ref5 only */
/* The 0x80 bit in pdiv means these are REF5, other entries are REF20 */
/* 38 */	{149,   (CH_UPPER_SB | CH_EWA_VALID)},
/* 39 */	{153,   (CH_LOWER_SB | CH_EWA_VALID)},
/* 40 */	{157,   (CH_UPPER_SB | CH_EWA_VALID)},
/* 41 */	{161,   (CH_LOWER_SB | CH_EWA_VALID)},
/* 42 */	{165,   (CH_LOWER_SB)},

/* 11a japan */
/* 43 */	{184,   (CH_UPPER_SB)},
/* 44 */	{188,   (CH_LOWER_SB)},
/* 45 */	{192,   (CH_UPPER_SB)},
/* 46 */	{196,   (CH_LOWER_SB)},
/* 47 */	{200,   (CH_UPPER_SB)},
/* 48 */	{204,   (CH_LOWER_SB)},
/* 49 */	{208,   (CH_UPPER_SB)},
/* 50 */	{212,   (CH_LOWER_SB)},
/* 51 */	{216,   (CH_LOWER_SB)}
};

#else

/* 36 */	{140,	(CH_LOWER_SB)},

/* 11a usa high, ref5 only */
/* The 0x80 bit in pdiv means these are REF5, other entries are REF20 */
/* 37 */	{149,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 38 */	{153,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 39 */	{157,	(CH_UPPER_SB | CH_EWA_VALID)},
/* 40 */	{161,	(CH_LOWER_SB | CH_EWA_VALID)},
/* 41 */	{165,	(CH_LOWER_SB)},

/* 11a japan */
/* 42 */	{184,	(CH_UPPER_SB)},
/* 43 */	{188,	(CH_LOWER_SB)},
/* 44 */	{192,	(CH_UPPER_SB)},
/* 45 */	{196,	(CH_LOWER_SB)},
/* 46 */	{200,	(CH_UPPER_SB)},
/* 47 */	{204,	(CH_LOWER_SB)},
/* 48 */	{208,	(CH_UPPER_SB)},
/* 49 */	{212,	(CH_LOWER_SB)},
/* 50 */	{216,	(CH_LOWER_SB)}
};
#endif /* WL11AC */


/* country code mapping for SPROM rev 1 */
static const char def_country[][WLC_CNTRY_BUF_SZ] = {
	"AU",   /* Worldwide */
	"TH",   /* Thailand */
	"IL",   /* Israel */
	"JO",   /* Jordan */
	"CN",   /* China */
	"JP",   /* Japan */
	"US",   /* USA */
	"DE",   /* Europe */
	"US",   /* US Low Band, use US */
	"JP",   /* Japan High Band, use Japan */
};

/* autocountry default country code list */
static const char def_autocountry[][WLC_CNTRY_BUF_SZ] = {
	"XY",
	"XA",
	"XB",
	"X0",
	"X1",
	"X2",
	"X3",
	"XS",
	"XV"
};


static bool
wlc_autocountry_lookup(char *cc)
{
	uint i;

	for (i = 0; i < ARRAYSIZE(def_autocountry); i++)
		if (!strcmp(def_autocountry[i], cc))
			return TRUE;

	return FALSE;
}

static bool
wlc_lookup_advertised_cc(char* ccode, const clm_country_t country)
{
	ccode_t advertised_cc;
	bool rv = FALSE;
	if (CLM_RESULT_OK == clm_country_advertised_cc(country, advertised_cc)) {
		memcpy(ccode, advertised_cc, 2);
		ccode[2] = '\0';
		rv = TRUE;
	}

	return rv;
}

wlc_cm_info_t *
BCMATTACHFN(wlc_channel_mgr_attach)(wlc_info_t *wlc)
{
	clm_result_t result = CLM_RESULT_ERR;
	wlc_cm_info_t *wlc_cm;
	char country_abbrev[WLC_CNTRY_BUF_SZ];
	clm_country_t country;
	wlc_pub_t *pub = wlc->pub;
#ifdef PCOEM_LINUXSTA
	bool use_row = TRUE;
#endif

	WL_TRACE(("wl%d: wlc_channel_mgr_attach\n", wlc->pub->unit));

	if ((wlc_cm = (wlc_cm_info_t *)MALLOC(pub->osh, sizeof(wlc_cm_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes", pub->unit,
			__FUNCTION__, MALLOCED(pub->osh)));
		return NULL;
	}
	bzero((char *)wlc_cm, sizeof(wlc_cm_info_t));
	wlc_cm->pub = pub;
	wlc_cm->wlc = wlc;
	wlc->cmi = wlc_cm;

	/* init the per chain limits to max power so they have not effect */
	memset(&wlc_cm->bandstate[BAND_2G_INDEX].chain_limits, WLC_TXPWR_MAX,
	       sizeof(struct wlc_channel_txchain_limits));
	memset(&wlc_cm->bandstate[BAND_5G_INDEX].chain_limits, WLC_TXPWR_MAX,
	       sizeof(struct wlc_channel_txchain_limits));

	/* get the SPROM country code or local, required to initialize channel set below */
	bzero(country_abbrev, WLC_CNTRY_BUF_SZ);
	if (wlc->pub->sromrev > 1) {
		/* get country code */
		const char *ccode = getvar(wlc->pub->vars, "ccode");
		if (ccode)
			strncpy(country_abbrev, ccode, WLC_CNTRY_BUF_SZ - 1);
	} else {
		uint locale_num;

		/* get locale */
		locale_num = (uint)getintvar(wlc->pub->vars, "cc");
		/* get mapped country */
		if (locale_num < ARRAYSIZE(def_country))
			strncpy(country_abbrev, def_country[locale_num],
			        sizeof(country_abbrev) - 1);
	}

#if defined(BCMDBG) || defined(WLTEST)
	/* Convert "ALL" country code in nvram to "#a" */
	if (!strncmp(country_abbrev, "ALL", WLC_CNTRY_BUF_SZ)) {
		strncpy(country_abbrev, "#a", sizeof(country_abbrev) - 1);
	}
#endif

	strncpy(wlc_cm->srom_ccode, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
	wlc_cm->srom_regrev = getintvar(wlc->pub->vars, "regrev");

	/* For "some" apple boards with KR2, make them as KR3 as they have passed the
	 * FCC test but with wrong SROM contents
	 */
	if ((pub->sih->boardvendor == VENDOR_APPLE) &&
	    ((pub->sih->boardtype == BCM943224M93) ||
	     (pub->sih->boardtype == BCM943224M93A))) {
		if ((wlc_cm->srom_regrev == 2) &&
		    !strncmp(country_abbrev, "KR", WLC_CNTRY_BUF_SZ)) {
			wlc_cm->srom_regrev = 3;
		}
	}

	/* Correct SROM contents of an Apple board */
	if ((pub->sih->boardvendor == VENDOR_APPLE) &&
	    (pub->sih->boardtype == 0x93) &&
	    !strncmp(country_abbrev, "JP", WLC_CNTRY_BUF_SZ) &&
	    (wlc_cm->srom_regrev == 4)) {
		wlc_cm->srom_regrev = 6;
	}

	result = clm_init(&clm_header);
	ASSERT(result == CLM_RESULT_OK);

	/* these are initialised to zero until they point to malloced data */
	wlc_cm->clm_base_dataptr = NULL;
	wlc_cm->clm_base_data_len = 0;


	wlc_cm->clm_inc_dataptr = NULL;
	wlc_cm->clm_inc_headerptr = NULL;
	wlc_cm->clm_inc_data_len = 0;


	result = wlc_country_lookup(wlc, country_abbrev, &country);

	/* default to US if country was not specified or not found */
	if (result != CLM_RESULT_OK) {
		strncpy(country_abbrev, "US", sizeof(country_abbrev) - 1);
		result = wlc_country_lookup(wlc, country_abbrev, &country);
	}

	ASSERT(result == CLM_RESULT_OK);

	/* save default country for exiting 11d regulatory mode */
	wlc_cntry_set_default(wlc->cntry, country_abbrev);

	/* initialize autocountry_default to driver default */
	if (wlc_autocountry_lookup(country_abbrev))
		wlc_11d_set_autocountry_default(wlc->m11d, country_abbrev);
	else
		wlc_11d_set_autocountry_default(wlc->m11d, "XV");

	wlc_cm->bandedge_filter_apply = (CHIPID(pub->sih->chip) == BCM4331_CHIP_ID);

#ifdef PCOEM_LINUXSTA
	if ((CHIPID(pub->sih->chip) != BCM4311_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4312_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4313_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4321_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4322_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43224_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43225_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43421_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4342_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43131_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43217_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43227_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43228_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM4331_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43142_CHIP_ID) &&
	    (CHIPID(pub->sih->chip) != BCM43428_CHIP_ID)) {
		printf("Broadcom vers %s: Unsupported Chip (%x)\n",
			EPI_VERSION_STR, pub->sih->chip);
		wlc->cmi = NULL;
		wlc_channel_mgr_detach(wlc_cm);
		return NULL;
	}
	if ((pub->sih->boardvendor == VENDOR_HP) && (!bcmp(country_abbrev, "US", 2) ||
		!bcmp(country_abbrev, "JP", 2) || !bcmp(country_abbrev, "IL", 2)))
		use_row = FALSE;

	/* use RoW locale if set */
	if (use_row) {
		bzero(country_abbrev, WLC_CNTRY_BUF_SZ);
		strncpy(country_abbrev, "XW", WLC_CNTRY_BUF_SZ);
	}

	/* Enable Auto Country Discovery */
	wlc_11d_set_autocountry_default(wlc->m11d, country_abbrev);
#endif /* PCOEM_LINUXSTA */

	/* Calling set_countrycode() once do not generate any event, if called more than
	 * once generates COUNTRY_CODE_CHANGED event which will cause the driver to crash
	 * since bsscfg structure is still not initialized
	 */
	wlc_set_countrycode(wlc_cm, country_abbrev);

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "txpwr_reg",
	                  (dump_fn_t)wlc_channel_dump_reg_ppr, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "txpwr_local",
	                  (dump_fn_t)wlc_channel_dump_reg_local_ppr, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "txpwr_srom",
	                  (dump_fn_t)wlc_channel_dump_srom_ppr, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "txpwr_margin",
	                  (dump_fn_t)wlc_channel_dump_margin, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "country_regrevs",
	                  (dump_fn_t)wlc_channel_supported_country_regrevs, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "agg_map",
	                  (dump_fn_t)wlc_dump_country_aggregate_map, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "txpwr_reg_max",
	                  (dump_fn_t)wlc_dump_max_power_per_channel, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "clm_limits_2G_20M",
	                  (dump_fn_t)wlc_dump_clm_limits_2G_20M, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "clm_limits_2G_40M",
	                  (dump_fn_t)wlc_dump_clm_limits_2G_40M, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "clm_limits_2G_20in40M",
	                  (dump_fn_t)wlc_dump_clm_limits_2G_20in40M, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "clm_limits_5G_20M",
	                  (dump_fn_t)wlc_dump_clm_limits_5G_20M, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "clm_limits_5G_40M",
	                  (dump_fn_t)wlc_dump_clm_limits_5G_40M, (void *)wlc_cm);
	wlc_dump_register(wlc->pub, "clm_limits_5G_20in40M",
	                  (dump_fn_t)wlc_dump_clm_limits_5G_20in40M, (void *)wlc_cm);
#endif 

	return wlc_cm;
}


static struct clm_data_header* download_dataptr = NULL;
static int download_data_len = 0;

int
wlc_handle_clm_dload(wlc_cm_info_t *wlc_cm, void* data_chunk, int clm_offset, int chunk_len,
	int clm_total_len, int ds_id)
{
	int result = BCME_OK;

	clm_country_t country;

	if (clm_offset == 0) {
		/* Clean up if a previous download didn't complete */
		if (download_dataptr != NULL) {
			MFREE(wlc_cm->pub->osh, download_dataptr, download_data_len);
			download_dataptr = NULL;
			download_data_len = 0;
		}

		if ((clm_total_len != 0) &&
			(download_dataptr = (struct clm_data_header*)MALLOC(wlc_cm->pub->osh,
			clm_total_len)) == NULL) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n", wlc_cm->pub->unit,
				__FUNCTION__, MALLOCED(wlc_cm->pub->osh)));
			return BCME_ERROR;
		}
		download_data_len = clm_total_len;
	} else if (download_data_len == 0) {
		WL_ERROR(("wl%d: %s: No memory allocated for chunk at %d\n", wlc_cm->pub->unit,
			__FUNCTION__, clm_offset));
		return BCME_ERROR;
	}

	if ((clm_offset + chunk_len) <= download_data_len) {
		if (download_dataptr != NULL)
			bcopy(data_chunk, (char*)download_dataptr + clm_offset, chunk_len);

		if ((clm_offset + chunk_len) == download_data_len) {
			/* Download complete. Install the new data */
			if (ds_id != 0) {
				/* Incremental CLM data */
				if (wlc_cm->clm_inc_dataptr != NULL) {
					MFREE(wlc_cm->pub->osh,
						wlc_cm->clm_inc_dataptr, wlc_cm->clm_inc_data_len);
				}
				if (clm_set_inc_data(download_dataptr) != CLM_RESULT_OK) {
					WL_ERROR(("wl%d: %s: Error loading incremental CLM data."
						" Revert to default data\n",
						wlc_cm->pub->unit, __FUNCTION__));
					result = BCME_ERROR;
					clm_set_inc_data(NULL);
					wlc_cm->clm_inc_data_len = 0;
					wlc_cm->clm_inc_dataptr = NULL;
					wlc_cm->clm_inc_headerptr = NULL;
					MFREE(wlc_cm->pub->osh, download_dataptr,
						download_data_len);
				} else {
					wlc_cm->clm_inc_dataptr = download_dataptr;
					wlc_cm->clm_inc_headerptr = download_dataptr;
					wlc_cm->clm_inc_data_len = download_data_len;
				}
			} else {	/* Replacement CLM base data */
				clm_result_t clm_result = CLM_RESULT_OK;
				/* Clear out incremental data */
				if (wlc_cm->clm_inc_dataptr != NULL) {
					clm_set_inc_data(NULL);
					MFREE(wlc_cm->pub->osh, wlc_cm->clm_inc_dataptr,
						wlc_cm->clm_inc_data_len);
					wlc_cm->clm_inc_data_len = 0;
					wlc_cm->clm_inc_dataptr = NULL;
					wlc_cm->clm_inc_headerptr = NULL;
				}
				/* Free any previously downloaded base data */
				if (wlc_cm->clm_base_dataptr != NULL) {
					MFREE(wlc_cm->pub->osh, wlc_cm->clm_base_dataptr,
						wlc_cm->clm_base_data_len);
				}
				if (download_dataptr != NULL) {
					WL_NONE(("wl%d: Pointing API at new base data: v%s\n",
						wlc_cm->pub->unit, download_dataptr->clm_version));
					clm_result = clm_init(download_dataptr);
					if (clm_result != CLM_RESULT_OK) {
						WL_ERROR(("wl%d: %s: Error loading new base CLM"
							" data.\n",
							wlc_cm->pub->unit, __FUNCTION__));
						result = BCME_ERROR;
						MFREE(wlc_cm->pub->osh, download_dataptr,
							download_data_len);
					}
				}
				if ((download_dataptr == NULL) || (clm_result != CLM_RESULT_OK)) {
					WL_NONE(("wl%d: %s: Reverting to base data.\n",
						wlc_cm->pub->unit, __FUNCTION__));
					clm_init(&clm_header);
					wlc_cm->clm_base_data_len = 0;
					wlc_cm->clm_base_dataptr = NULL;
				} else {
					wlc_cm->clm_base_dataptr = download_dataptr;
					wlc_cm->clm_base_data_len = download_data_len;
				}
			}
			if (wlc_country_lookup_direct(wlc_cm->ccode, wlc_cm->regrev, &country) ==
				CLM_RESULT_OK)
				wlc_cm->country = country;
			else
				wlc_cm->country = 0;

			/* download complete - tidy up */
			download_dataptr = NULL;
			download_data_len = 0;

		}
	} else {
		WL_ERROR(("wl%d: %s: incremental download too big for allocated memory"
			"(clm_offset %d, clm_offset + chunk_len %d, download_data_len %d)\n",
			wlc_cm->pub->unit, __FUNCTION__, clm_offset, clm_offset + chunk_len,
			download_data_len));
		MFREE(wlc_cm->pub->osh, download_dataptr, download_data_len);
		download_dataptr = NULL;
		download_data_len = 0;
		result = BCME_ERROR;
	}
	return result;
}


void
BCMATTACHFN(wlc_channel_mgr_detach)(wlc_cm_info_t *wlc_cm)
{
	if (wlc_cm) {
		if (wlc_cm->clm_inc_dataptr != NULL) {
			MFREE(wlc_cm->pub->osh, wlc_cm->clm_inc_dataptr, wlc_cm->clm_inc_data_len);
		}
		if (wlc_cm->clm_base_dataptr != NULL) {
			MFREE(wlc_cm->pub->osh, wlc_cm->clm_base_dataptr,
				wlc_cm->clm_base_data_len);
		}
		MFREE(wlc_cm->pub->osh, wlc_cm, sizeof(wlc_cm_info_t));
	}
}

const char* wlc_channel_country_abbrev(wlc_cm_info_t *wlc_cm)
{
	return wlc_cm->country_abbrev;
}

const char* wlc_channel_ccode(wlc_cm_info_t *wlc_cm)
{
	return wlc_cm->ccode;
}

uint wlc_channel_regrev(wlc_cm_info_t *wlc_cm)
{
	return wlc_cm->regrev;
}

uint8 wlc_channel_locale_flags(wlc_cm_info_t *wlc_cm)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return wlc_cm->bandstate[wlc->band->bandunit].locale_flags;
}

uint8 wlc_channel_locale_flags_in_band(wlc_cm_info_t *wlc_cm, uint bandunit)
{
	return wlc_cm->bandstate[bandunit].locale_flags;
}

/*
 * return the first valid chanspec for the locale, if one is not found and hw_fallback is true
 * then return the first h/w supported chanspec.
 */
chanspec_t
wlc_default_chanspec(wlc_cm_info_t *wlc_cm, bool hw_fallback)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	chanspec_t  chspec;

	chspec = wlc_create_chspec(wlc, 0);
	/* try to find a chanspec that's valid in this locale */
	if ((chspec = wlc_next_chanspec(wlc_cm, chspec, CHAN_TYPE_ANY, 0)) == INVCHANSPEC)
		/* try to find a chanspec valid for this hardware */
		if (hw_fallback)
			chspec = wlc_phy_chanspec_band_firstch(wlc_cm->wlc->band->pi,
				wlc->band->bandtype);

	return chspec;
}

/*
 * Return the next channel's chanspec.
 */
chanspec_t
wlc_next_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t cur_chanspec, int type, bool any_band)
{
	uint8 ch;
	uint8 cur_chan = CHSPEC_CHANNEL(cur_chanspec);
	chanspec_t chspec;

	/* 0 is an invalid chspec, routines trying to find the first available channel should
	 * now be using wlc_default_chanspec (above)
	 */
	ASSERT(cur_chanspec);

	/* Try all channels in current band */
	ch = cur_chan + 1;
	for (; ch <= MAXCHANNEL; ch++) {
		if (ch == MAXCHANNEL)
			ch = 0;
		if (ch == cur_chan)
			break;
		/* create the next channel spec */
		chspec = cur_chanspec & ~WL_CHANSPEC_CHAN_MASK;
		chspec |= ch;
		if (wlc_valid_chanspec(wlc_cm, chspec)) {
			if ((type == CHAN_TYPE_ANY) ||
			(type == CHAN_TYPE_CHATTY && !wlc_quiet_chanspec(wlc_cm, chspec)) ||
			(type == CHAN_TYPE_QUIET && wlc_quiet_chanspec(wlc_cm, chspec)))
				return chspec;
		}
	}

	if (!any_band)
		return ((chanspec_t)INVCHANSPEC);

	/* Couldn't find any in current band, try other band */
	ch = cur_chan + 1;
	for (; ch <= MAXCHANNEL; ch++) {
		if (ch == MAXCHANNEL)
			ch = 0;
		if (ch == cur_chan)
			break;

		/* create the next channel spec */
		chspec = cur_chanspec & ~(WL_CHANSPEC_CHAN_MASK | WL_CHANSPEC_BAND_MASK);
		chspec |= ch;
		if (CHANNEL_BANDUNIT(wlc, ch) == BAND_2G_INDEX)
			chspec |= WL_CHANSPEC_BAND_2G;
		else
			chspec |= WL_CHANSPEC_BAND_5G;
		if (wlc_valid_chanspec_db(wlc_cm, chspec)) {
			if ((type == CHAN_TYPE_ANY) ||
			(type == CHAN_TYPE_CHATTY && !wlc_quiet_chanspec(wlc_cm, chspec)) ||
			(type == CHAN_TYPE_QUIET && wlc_quiet_chanspec(wlc_cm, chspec)))
				return chspec;
		}
	}

	return ((chanspec_t)INVCHANSPEC);
}

/* return chanvec for a given country code and band */
bool
wlc_channel_get_chanvec(struct wlc_info *wlc, const char* country_abbrev,
	int bandtype, chanvec_t *channels)
{
	clm_band_t band;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_t country;
	clm_country_locales_t locale;
	chanvec_t unused;

	result = wlc_country_lookup(wlc, country_abbrev, &country);
	if (result != CLM_RESULT_OK)
		return FALSE;

	result = wlc_get_locale(country, &locale);
	if (bandtype != WLC_BAND_2G && bandtype != WLC_BAND_5G)
		return FALSE;

	band = (bandtype == WLC_BAND_2G) ? CLM_BAND_2G : CLM_BAND_5G;
	wlc_locale_get_channels(&locale, band, channels, &unused);
	return TRUE;
}

/* set the driver's current country and regulatory information using a country code
 * as the source. Lookup built in country information found with the country code.
 */
int
wlc_set_countrycode(wlc_cm_info_t *wlc_cm, const char* ccode)
{
	WL_NONE(("wl%d: %s: ccode \"%s\"\n", wlc_cm->wlc->pub->unit, __FUNCTION__, ccode));
	return wlc_set_countrycode_rev(wlc_cm, ccode, -1);
}

int
wlc_set_countrycode_rev(wlc_cm_info_t *wlc_cm, const char* ccode, int regrev)
{
#ifdef BCMDBG
	wlc_info_t *wlc = wlc_cm->wlc;
#endif
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_t country;
	char mapped_ccode[WLC_CNTRY_BUF_SZ];
	uint mapped_regrev = 0;
	char country_abbrev[WLC_CNTRY_BUF_SZ] = { 0 };

	WL_NONE(("wl%d: %s: (country_abbrev \"%s\", ccode \"%s\", regrev %d) SPROM \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__,
	         country_abbrev, ccode, regrev, wlc_cm->srom_ccode, wlc_cm->srom_regrev));

	/* if regrev is -1, lookup the mapped country code,
	 * otherwise use the ccode and regrev directly
	 */
	if (regrev == -1) {
		/* map the country code to a built-in country code, regrev, and country_info */
		result = wlc_countrycode_map(wlc_cm, ccode, mapped_ccode, &mapped_regrev, &country);
		if (result == CLM_RESULT_OK)
			WL_NONE(("wl%d: %s: mapped to \"%s\"/%u\n",
			         wlc->pub->unit, __FUNCTION__, ccode, mapped_regrev));
		else
			WL_NONE(("wl%d: %s: failed lookup\n",
			        wlc->pub->unit, __FUNCTION__));
	} else {
		/* find the matching built-in country definition */
		result = wlc_country_lookup_direct(ccode, regrev, &country);
		strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ-1);
		mapped_ccode[WLC_CNTRY_BUF_SZ-1] = '\0';
		mapped_regrev = regrev;
	}

	if (result != CLM_RESULT_OK)
		return BCME_BADARG;

	/* Set the driver state for the country.
	 * Getting the advertised country code from CLM.
	 * Else use the one comes from ccode.
	 */
	if (wlc_lookup_advertised_cc(country_abbrev, country))
		wlc_set_country_common(wlc_cm, country_abbrev,
		mapped_ccode, mapped_regrev, country);
	else
		wlc_set_country_common(wlc_cm, ccode,
		mapped_ccode, mapped_regrev, country);

	return 0;
}

/* set the driver's current country and regulatory information using a country code
 * as the source. Look up built in country information found with the country code.
 */
static void
wlc_set_country_common(wlc_cm_info_t *wlc_cm,
                       const char* country_abbrev,
                       const char* ccode, uint regrev, clm_country_t country)
{
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale;
	uint8 flags;

	wlc_info_t *wlc = wlc_cm->wlc;
	char prev_country_abbrev[WLC_CNTRY_BUF_SZ];

	/* save current country state */
	wlc_cm->country = country;

	bzero(&prev_country_abbrev, WLC_CNTRY_BUF_SZ);
	strncpy(prev_country_abbrev, wlc_cm->country_abbrev, WLC_CNTRY_BUF_SZ - 1);

	strncpy(wlc_cm->country_abbrev, country_abbrev, WLC_CNTRY_BUF_SZ-1);
	strncpy(wlc_cm->ccode, ccode, WLC_CNTRY_BUF_SZ-1);
	wlc_cm->regrev = regrev;

	result = wlc_get_locale(country, &locale);
	ASSERT(result == CLM_RESULT_OK);

	result = wlc_get_flags(&locale, CLM_BAND_2G, &flags);
	ASSERT(result == CLM_RESULT_OK);
	BCM_REFERENCE(result);

#ifdef WL_BEAMFORMING
	if (flags & WLC_TXBF) {
		wlc_stf_set_txbf(wlc, TRUE);
	} else {
		wlc_stf_set_txbf(wlc, FALSE);
	}
#endif

#ifdef WL11N
	/* disable/restore nmode based on country regulations */
	if ((flags & WLC_NO_MIMO) && ((NBANDS(wlc) == 2) || IS_SINGLEBAND_5G(wlc->deviceid))) {
		result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);
	}
	if (flags & WLC_NO_MIMO) {
		wlc_set_nmode(wlc, OFF);
		wlc->stf->no_cddstbc = TRUE;
	} else {
		wlc->stf->no_cddstbc = FALSE;
		wlc_prot_n_mode_reset(wlc->prot_n, FALSE);
	}

	wlc_stf_ss_update(wlc, wlc->bandstate[BAND_2G_INDEX]);
	wlc_stf_ss_update(wlc, wlc->bandstate[BAND_5G_INDEX]);
#endif /* WL11N */

#if defined(AP) && defined(RADAR)
	if ((NBANDS(wlc) == 2) || IS_SINGLEBAND_5G(wlc->deviceid)) {
		phy_radar_detect_mode_t mode;
		result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);

		mode = ISDFS_EU(flags) ? RADAR_DETECT_MODE_EU:RADAR_DETECT_MODE_FCC;
		wlc_phy_radar_detect_mode_set(wlc->band->pi, mode);
	}
#endif /* AP && RADAR */

	wlc_channels_init(wlc_cm, country);

	/* Country code changed */
	if (strlen(prev_country_abbrev) > 1 &&
	    strncmp(wlc_cm->country_abbrev, prev_country_abbrev,
	            strlen(wlc_cm->country_abbrev)) != 0)
		wlc_mac_event(wlc, WLC_E_COUNTRY_CODE_CHANGED, NULL,
		              0, 0, 0, wlc_cm->country_abbrev, strlen(wlc_cm->country_abbrev) + 1);

	return;
}

#if defined(AP) && defined(RADAR)
extern bool
wlc_is_european_weather_radar_channel(struct wlc_info *wlc, chanspec_t chanspec)
{
	clm_result_t result;
	clm_country_locales_t locale;
	clm_country_t country;
	uint8 flags;

	if (!((NBANDS(wlc) == 2) || IS_SINGLEBAND_5G(wlc->deviceid)))
		return FALSE;

	result = wlc_country_lookup_direct(wlc->cmi->ccode, wlc->cmi->regrev, &country);
	if (result != CLM_RESULT_OK)
		return FALSE;

	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK)
		return FALSE;

	result = wlc_get_flags(&locale, CLM_BAND_5G, &flags);
	if (result != CLM_RESULT_OK)
		return FALSE;
//CSP 666865 weather channel need CAC600.
	if (!ISDFS_EU(flags))
		return FALSE;

	if (CHSPEC_IS80(chanspec)) {
		if ((chanspec == CH80MHZ_CHSPEC(122, WL_CHANSPEC_CTL_SB_LL)) || /* 116-80 */
			(chanspec == CH80MHZ_CHSPEC(122, WL_CHANSPEC_CTL_SB_LU)) || /* 120-80 */
			(chanspec == CH80MHZ_CHSPEC(122, WL_CHANSPEC_CTL_SB_UL)) || /* 124-80 */
			(chanspec == CH80MHZ_CHSPEC(122, WL_CHANSPEC_CTL_SB_UU))) /* 128-80 */
			{
				return TRUE;
			}
	}
	else if (CHSPEC_IS40(chanspec)) {
		if ((chanspec == CH40MHZ_CHSPEC(118, WL_CHANSPEC_CTL_SB_LOWER)) || /* 118l */
			(chanspec == CH40MHZ_CHSPEC(118,WL_CHANSPEC_CTL_SB_UPPER)) || /* 118u */
			(chanspec == CH40MHZ_CHSPEC(126,WL_CHANSPEC_CTL_SB_LOWER)) || /* 126l */
			(chanspec == CH40MHZ_CHSPEC(126, WL_CHANSPEC_CTL_SB_UPPER))) /* 126u */
			{
				return TRUE;
			}
	}
	else if (CHSPEC_IS20(chanspec)) {
		if ((CHSPEC_CHANNEL(chanspec) == 120) ||
			(CHSPEC_CHANNEL(chanspec) == 124) ||
			(CHSPEC_CHANNEL(chanspec) == 128))
			{
				return TRUE;
	}
	}

	return FALSE;
}
#endif /* defined(AP) && defined(RADAR) */

/* Lookup a country info structure from a null terminated country code
 * The lookup is case sensitive.
 */
clm_result_t
wlc_country_lookup(struct wlc_info *wlc, const char* ccode, clm_country_t *country)
{
	clm_result_t result = CLM_RESULT_ERR;
	char mapped_ccode[WLC_CNTRY_BUF_SZ];
	uint mapped_regrev;

	WL_NONE(("wl%d: %s: ccode \"%s\", SPROM \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__, ccode, wlc->cmi->srom_ccode, wlc->cmi->srom_regrev));

	/* map the country code to a built-in country code, regrev, and country_info struct */
	result = wlc_countrycode_map(wlc->cmi, ccode, mapped_ccode, &mapped_regrev, country);

	if (result == CLM_RESULT_OK)
		WL_NONE(("wl%d: %s: mapped to \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, mapped_ccode, mapped_regrev));
	else
		WL_NONE(("wl%d: %s: failed lookup\n",
		         wlc->pub->unit, __FUNCTION__));

	return result;
}

static clm_result_t
wlc_countrycode_map(wlc_cm_info_t *wlc_cm, const char *ccode,
	char *mapped_ccode, uint *mapped_regrev, clm_country_t *country)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	clm_result_t result = CLM_RESULT_ERR;
	uint srom_regrev = wlc_cm->srom_regrev;
	const char *srom_ccode = wlc_cm->srom_ccode;
	int mapped;

	/* check for currently supported ccode size */
	if (strlen(ccode) > (WLC_CNTRY_BUF_SZ - 1)) {
		WL_ERROR(("wl%d: %s: ccode \"%s\" too long for match\n",
		          wlc->pub->unit, __FUNCTION__, ccode));
		return CLM_RESULT_ERR;
	}

	/* default mapping is the given ccode and regrev 0 */
	strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ);
	*mapped_regrev = 0;

	/* Map locale for buffalo if needed */
	if (wlc_buffalo_map_locale(wlc, ccode)) {
		strncpy(mapped_ccode, "J10", WLC_CNTRY_BUF_SZ);
		result = wlc_country_lookup_direct(mapped_ccode, *mapped_regrev, country);
		if (result == CLM_RESULT_OK)
			return result;
	}

	/* If the desired country code matches the srom country code,
	 * then the mapped country is the srom regulatory rev.
	 * Otherwise look for an aggregate mapping.
	 */
	if (!strcmp(srom_ccode, ccode)) {
		WL_NONE(("wl%d: %s: srom ccode and ccode \"%s\" match\n",
		         wlc->pub->unit, __FUNCTION__, ccode));
		*mapped_regrev = srom_regrev;
		mapped = 0;
	} else {
		mapped = wlc_country_aggregate_map(wlc_cm, ccode, mapped_ccode, mapped_regrev);
		if (mapped)
			WL_NONE(("wl%d: %s: found aggregate mapping \"%s\"/%u\n",
			         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));
	}

	/* CLM 8.2, JAPAN
	 * Use the regrev=1 Japan country definition by default for chips newer than
	 * our d11 core rev 5 4306 chips, or for AP's of any vintage.
	 * For STAs with a 4306, use the legacy Japan country def "JP/0".
	 * Only do the conversion if JP/0 was not specified by a mapping or by an
	 * sprom with a regrev:
	 * Sprom has no specific info about JP's regrev if it's less than rev 3 or it does
	 * not specify "JP" as its country code =>
	 * (strcmp("JP", srom_ccode) || (wlc->pub->sromrev < 3))
	 */
	if (!strcmp("JP", mapped_ccode) && *mapped_regrev == 0 &&
	    !mapped && (strcmp("JP", srom_ccode) || (wlc->pub->sromrev < 3)) &&
	    (AP_ENAB(wlc->pub) || D11REV_GT(wlc->pub->corerev, 5))) {
		*mapped_regrev = 1;
		WL_NONE(("wl%d: %s: Using \"JP/1\" instead of legacy \"JP/0\" since we %s\n",
		         wlc->pub->unit, __FUNCTION__,
		         AP_ENAB(wlc->pub) ? "are operating as an AP" : "are newer than 4306"));
	}

#ifdef DSLCPE
	if (*mapped_regrev == 0) {
		*mapped_regrev = requestRegRev;
	}
#endif

	WL_NONE(("wl%d: %s: searching for country using ccode/rev \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));

	/* find the matching built-in country definition */
	result = wlc_country_lookup_direct(mapped_ccode, *mapped_regrev, country);

	/* if there is not an exact rev match, default to rev zero */
	if (result != CLM_RESULT_OK && *mapped_regrev != 0) {
		*mapped_regrev = 0;
		WL_NONE(("wl%d: %s: No country found, use base revision \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, mapped_ccode, *mapped_regrev));
		result = wlc_country_lookup_direct(mapped_ccode, *mapped_regrev, country);
	}

	if (result != CLM_RESULT_OK)
		WL_NONE(("wl%d: %s: No country found, failed lookup\n",
		         wlc->pub->unit, __FUNCTION__));

	return result;
}

clm_result_t
clm_aggregate_country_lookup(const ccode_t cc, unsigned int rev, clm_agg_country_t *agg)
{
	return clm_agg_country_lookup(cc, rev, agg);
}

clm_result_t
clm_aggregate_country_map_lookup(const clm_agg_country_t agg, const ccode_t target_cc,
	unsigned int *rev)
{
	return clm_agg_country_map_lookup(agg, target_cc, rev);
}

static int
wlc_country_aggregate_map(wlc_cm_info_t *wlc_cm, const char *ccode,
                          char *mapped_ccode, uint *mapped_regrev)
{
#ifdef BCMDBG
	wlc_info_t *wlc = wlc_cm->wlc;
#endif
	clm_result_t result;
	clm_agg_country_t agg = 0;
	const char *srom_ccode = wlc_cm->srom_ccode;
	uint srom_regrev = (uint8)wlc_cm->srom_regrev;

	/* Use "ww", WorldWide, for the lookup value for '\0\0' */
	if (srom_ccode[0] == '\0')
		srom_ccode = "ww";

	/* Check for a match in the aggregate country list */
	WL_NONE(("wl%d: %s: searching for agg map for srom ccode/rev \"%s\"/%u\n",
	         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));

	result = clm_aggregate_country_lookup(srom_ccode, srom_regrev, &agg);

	if (result != CLM_RESULT_OK)
		WL_NONE(("wl%d: %s: no map for \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));
	else
		WL_NONE(("wl%d: %s: found map for \"%s\"/%u\n",
		         wlc->pub->unit, __FUNCTION__, srom_ccode, srom_regrev));


	if (result == CLM_RESULT_OK) {
		result = clm_aggregate_country_map_lookup(agg, ccode, mapped_regrev);
		strncpy(mapped_ccode, ccode, WLC_CNTRY_BUF_SZ);
	}

	return (result == CLM_RESULT_OK);
}


#if defined(BCMDBG)
static int
wlc_dump_country_aggregate_map(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	const char *cur_ccode = wlc_cm->ccode;
	uint cur_regrev = (uint8)wlc_cm->regrev;
	clm_agg_country_t agg = 0;
	clm_result_t result;
	int agg_iter;

	/* Use "ww", WorldWide, for the lookup value for '\0\0' */
	if (cur_ccode[0] == '\0')
		cur_ccode = "ww";

	clm_iter_init(&agg_iter);
	if ((result = clm_aggregate_country_lookup(cur_ccode, cur_regrev, &agg)) == CLM_RESULT_OK) {
		clm_agg_map_t map_iter;
		ccode_t cc;
		unsigned int rev;

		bcm_bprintf(b, "Map for %s/%u ->\n", cur_ccode, cur_regrev);
		clm_iter_init(&map_iter);
		while ((result = clm_agg_map_iter(agg, &map_iter, cc, &rev)) == CLM_RESULT_OK) {
			bcm_bprintf(b, "%c%c/%u\n", cc[0], cc[1], rev);
		}
	} else {
		bcm_bprintf(b, "No lookaside table for %s/%u\n", cur_ccode, cur_regrev);
	}
	return 0;

}
#endif 


/* Lookup a country info structure from a null terminated country
 * abbreviation and regrev directly with no translation.
 */
clm_result_t
wlc_country_lookup_direct(const char* ccode, uint regrev, clm_country_t *country)
{
	return clm_country_lookup(ccode, regrev, country);
}


#if defined(STA) && defined(WL11D)
/* Lookup a country info structure considering only legal country codes as found in
 * a Country IE; two ascii alpha followed by " ", "I", or "O".
 * Do not match any user assigned application specifc codes that might be found
 * in the driver table.
 */
clm_result_t
wlc_country_lookup_ext(wlc_info_t *wlc, const char *ccode, clm_country_t *country)
{
	clm_result_t result = CLM_RESULT_NOT_FOUND;
	char country_str_lookup[WLC_CNTRY_BUF_SZ] = { 0 };

	/* only allow ascii alpha uppercase for the first 2 chars, and " ", "I", "O" for the 3rd */
	if (!((0x80 & ccode[0]) == 0 && bcm_isupper(ccode[0]) &&
	      (0x80 & ccode[1]) == 0 && bcm_isupper(ccode[1]) &&
	      (ccode[2] == ' ' || ccode[2] == 'I' || ccode[2] == 'O')))
		return result;

	/* for lookup in the driver table of country codes, only use the first
	 * 2 chars, ignore the 3rd character " ", "I", "O" qualifier
	 */
	country_str_lookup[0] = ccode[0];
	country_str_lookup[1] = ccode[1];

	/* do not match ISO 3166-1 user assigned country codes that may be in the driver table */
	if (!strcmp("AA", country_str_lookup) ||	/* AA */
	    !strcmp("ZZ", country_str_lookup) ||	/* ZZ */
	    country_str_lookup[0] == 'X' ||		/* XA - XZ */
	    (country_str_lookup[0] == 'Q' &&		/* QM - QZ */
	     (country_str_lookup[1] >= 'M' && country_str_lookup[1] <= 'Z')))
		return result;

#ifdef MACOSX
	if (!strcmp("NA", country_str_lookup))
		return result;
#endif /* MACOSX */

	return wlc_country_lookup(wlc, country_str_lookup, country);
}
#endif /* STA && WL11D */

static int
wlc_channels_init(wlc_cm_info_t *wlc_cm, clm_country_t country)
{
	clm_country_locales_t locale;
	uint8 flags;
	wlc_info_t *wlc = wlc_cm->wlc;
	uint i, j;
	wlcband_t * band;
	chanvec_t sup_chan, temp_chan;

	if (wlc->dfs)
		wlc_dfs_reset_all(wlc->dfs);

	bzero(&wlc_cm->restricted_channels, sizeof(chanvec_t));
	bzero(&wlc_cm->locale_radar_channels, sizeof(chanvec_t));

	band = wlc->band;
	for (i = 0; i < NBANDS(wlc); i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {
		clm_result_t result = wlc_get_locale(country, &locale);
		clm_band_t tmp_band;
		if (result == CLM_RESULT_OK) {
			if (BAND_5G(band->bandtype)) {
				tmp_band = CLM_BAND_5G;
			} else {
				tmp_band = CLM_BAND_2G;
			}
			result = wlc_get_flags(&locale, tmp_band, &flags);
			wlc_cm->bandstate[band->bandunit].locale_flags = flags;

			wlc_locale_get_channels(&locale, tmp_band,
				&wlc_cm->bandstate[band->bandunit].valid_channels,
				&temp_chan);
			/* initialize restricted channels */
			for (j = 0; j < sizeof(chanvec_t); j++) {
				wlc_cm->restricted_channels.vec[j] |= temp_chan.vec[j];
			}
#ifdef BAND5G     /* RADAR */
			wlc_cm->bandstate[band->bandunit].radar_channels =
				&wlc_cm->locale_radar_channels;
			if (BAND_5G(band->bandtype) && (flags & WLC_DFS_TPC)) {
				for (j = 0; j < sizeof(chanvec_t); j++) {
					wlc_cm->bandstate[band->bandunit].radar_channels->vec[j] =
						radar_set1.vec[j] &
						wlc_cm->bandstate[band->bandunit].
						valid_channels.vec[j];
				}
			}
#endif /* BAND5G */

			/* set the channel availability,
			 * masking out the channels that may not be supported on this phy
			 */
			wlc_phy_chanspec_band_validch(band->pi, band->bandtype, &sup_chan);
			wlc_locale_get_channels(&locale, tmp_band,
				&wlc_cm->bandstate[band->bandunit].valid_channels,
				&temp_chan);
			for (j = 0; j < sizeof(chanvec_t); j++)
				wlc_cm->bandstate[band->bandunit].valid_channels.vec[j] &=
					sup_chan.vec[j];
		}
	}

	wlc_upd_restricted_chanspec_flag(wlc_cm);
	wlc_quiet_channels_reset(wlc_cm);
	wlc_channels_commit(wlc_cm);
	wlc_rcinfo_init(wlc_cm);
	wlc_regclass_vec_init(wlc_cm);

	return (0);
}

/* Update the radio state (enable/disable) and tx power targets
 * based on a new set of channel/regulatory information
 */
static void
wlc_channels_commit(wlc_cm_info_t *wlc_cm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	uint chan;
#ifdef PPR_API
	ppr_t* txpwr;
#else
	txppr_t txpwr;
#endif

	/* search for the existence of any valid channel */
	for (chan = 0; chan < MAXCHANNEL; chan++) {
		if (VALID_CHANNEL20_DB(wlc, chan)) {
			break;
		}
	}
	if (chan == MAXCHANNEL)
		chan = INVCHANNEL;

	/* based on the channel search above, set or clear WL_RADIO_COUNTRY_DISABLE */
	if (chan == INVCHANNEL) {
		/* country/locale with no valid channels, set the radio disable bit */
		mboolset(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE);
		WL_ERROR(("wl%d: %s: no valid channel for \"%s\" nbands %d bandlocked %d\n",
		          wlc->pub->unit, __FUNCTION__,
		          wlc_cm->country_abbrev, NBANDS(wlc), wlc->bandlocked));
	} else if (mboolisset(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE)) {
		/* country/locale with valid channel, clear the radio disable bit */
		mboolclr(wlc->pub->radio_disabled, WL_RADIO_COUNTRY_DISABLE);
	}

	/* Now that the country abbreviation is set, if the radio supports 2G, then
	 * set channel 14 restrictions based on the new locale.
	 */
	if (NBANDS(wlc) > 1 || BAND_2G(wlc->band->bandtype)) {
		wlc_phy_chanspec_ch14_widefilter_set(wlc->band->pi, wlc_japan(wlc) ? TRUE : FALSE);
	}

	if (wlc->pub->up && chan != INVCHANNEL) {
		/* recompute tx power for new country info */


		/* Where do we get a good chanspec? wlc, phy, set it ourselves? */
#ifdef PPR_API
		if ((txpwr = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
			return;
		}

		wlc_channel_reg_limits(wlc_cm, wlc->chanspec, txpwr);
#else
		wlc_channel_reg_limits(wlc_cm, wlc->chanspec, &txpwr);
#endif

#ifdef PPR_API
		ppr_apply_max(txpwr, WLC_TXPWR_MAX);
		/* Where do we get a good chanspec? wlc, phy, set it ourselves? */
		wlc_phy_txpower_limit_set(wlc->band->pi, txpwr, wlc->chanspec);

		ppr_delete(wlc_cm->pub->osh, txpwr);
#else
		wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, &txpwr, WLC_TXPWR_MAX);
		/* Where do we get a good chanspec? wlc, phy, set it ourselves? */
		wlc_phy_txpower_limit_set(wlc->band->pi, &txpwr, wlc->chanspec);
#endif
	}
}

/* reset the quiet channels vector to the union of the restricted and radar channel sets */
void
wlc_quiet_channels_reset(wlc_cm_info_t *wlc_cm)
{
#ifdef BAND5G
	wlc_info_t *wlc = wlc_cm->wlc;
	uint i;
	wlcband_t *band;
#endif /* BAND5G */

	/* initialize quiet channels for restricted channels */
	bcopy(&wlc_cm->restricted_channels, &wlc_cm->quiet_channels, sizeof(chanvec_t));

#ifdef BAND5G     /* RADAR */
	band = wlc->band;
	for (i = 0; i < NBANDS(wlc); i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {
		/* initialize quiet channels for radar if we are in spectrum management mode */
		if (WL11H_ENAB(wlc)) {
			uint j;
			const chanvec_t *chanvec;

			chanvec = wlc_cm->bandstate[band->bandunit].radar_channels;
			for (j = 0; j < sizeof(chanvec_t); j++)
				wlc_cm->quiet_channels.vec[j] |= chanvec->vec[j];
		}
	}
#endif /* BAND5G */
}
//CSP666865  in DFS channel, need CAC 60s.
bool
wlc_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
#if 1 /* Ares */
       if(VHT_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS80(chspec))
       return (isset(wlc_cm->quiet_channels.vec, LL_20_SB(CHSPEC_CHANNEL(chspec))) ||
               isset(wlc_cm->quiet_channels.vec, LOWER_40_SB(CHSPEC_CHANNEL(chspec))) ||
               isset(wlc_cm->quiet_channels.vec, LU_20_SB(CHSPEC_CHANNEL(chspec))) ||
               isset(wlc_cm->quiet_channels.vec, UL_20_SB(CHSPEC_CHANNEL(chspec))) ||
               isset(wlc_cm->quiet_channels.vec, UPPER_40_SB(CHSPEC_CHANNEL(chspec))) ||
                isset(wlc_cm->quiet_channels.vec, UU_20_SB(CHSPEC_CHANNEL(chspec))));
#endif         
	return (N_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS40(chspec) ?
		(isset(wlc_cm->quiet_channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chspec))) ||
		isset(wlc_cm->quiet_channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chspec)))) :
		isset(wlc_cm->quiet_channels.vec, CHSPEC_CHANNEL(chspec)));
}

void
wlc_set_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
#if 1 /* Ares */
       if (VHT_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS80(chspec)) {
               setbit(wlc_cm->quiet_channels.vec, LL_20_SB(CHSPEC_CHANNEL(chspec)));
               setbit(wlc_cm->quiet_channels.vec, LOWER_40_SB(CHSPEC_CHANNEL(chspec)));
               setbit(wlc_cm->quiet_channels.vec, LU_20_SB(CHSPEC_CHANNEL(chspec)));

               setbit(wlc_cm->quiet_channels.vec, UL_20_SB(CHSPEC_CHANNEL(chspec)));
               setbit(wlc_cm->quiet_channels.vec, UPPER_40_SB(CHSPEC_CHANNEL(chspec)));
               setbit(wlc_cm->quiet_channels.vec, UU_20_SB(CHSPEC_CHANNEL(chspec)));
       } else
#endif
	if (N_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS40(chspec)) {
		setbit(wlc_cm->quiet_channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chspec)));
		setbit(wlc_cm->quiet_channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chspec)));
	} else
		setbit(wlc_cm->quiet_channels.vec, CHSPEC_CHANNEL(chspec));
}

void
wlc_clr_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
#if 1 /* Ares */
               if (VHT_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS80(chspec)) {
                       clrbit(wlc_cm->quiet_channels.vec, LL_20_SB(CHSPEC_CHANNEL(chspec)));
                       clrbit(wlc_cm->quiet_channels.vec, LOWER_40_SB(CHSPEC_CHANNEL(chspec)));
                       clrbit(wlc_cm->quiet_channels.vec, LU_20_SB(CHSPEC_CHANNEL(chspec)));
                       
                       clrbit(wlc_cm->quiet_channels.vec, UL_20_SB(CHSPEC_CHANNEL(chspec)));
                       clrbit(wlc_cm->quiet_channels.vec, UPPER_40_SB(CHSPEC_CHANNEL(chspec)));
                       clrbit(wlc_cm->quiet_channels.vec, UU_20_SB(CHSPEC_CHANNEL(chspec)));
               } else
#endif

	if (N_ENAB(wlc_cm->wlc->pub) && CHSPEC_IS40(chspec)) {
		clrbit(wlc_cm->quiet_channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chspec)));
		clrbit(wlc_cm->quiet_channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chspec)));
	} else
		clrbit(wlc_cm->quiet_channels.vec, CHSPEC_CHANNEL(chspec));
}

/* Is the channel valid for the current locale? (but don't consider channels not
 *   available due to bandlocking)
 */
bool
wlc_valid_channel20_db(wlc_cm_info_t *wlc_cm, uint val)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return (VALID_CHANNEL20(wlc, val) ||
		(!wlc->bandlocked && VALID_CHANNEL20_IN_BAND(wlc, OTHERBANDUNIT(wlc), val)));
}

/* Is the channel valid for the current locale and specified band? */
bool
wlc_valid_channel20_in_band(wlc_cm_info_t *wlc_cm, uint bandunit, uint val)
{
	return ((val < MAXCHANNEL) && isset(wlc_cm->bandstate[bandunit].valid_channels.vec, val));
}

/* Is the channel valid for the current locale and current band? */
bool
wlc_valid_channel20(wlc_cm_info_t *wlc_cm, uint val)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return ((val < MAXCHANNEL) &&
		isset(wlc_cm->bandstate[wlc->band->bandunit].valid_channels.vec, val));
}

/* Is the 40 MHz allowed for the current locale and specified band? */
bool
wlc_valid_40chanspec_in_band(wlc_cm_info_t *wlc_cm, uint bandunit)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return (((wlc_cm->bandstate[bandunit].locale_flags & (WLC_NO_MIMO | WLC_NO_40MHZ)) == 0) &&
	        WL_BW_CAP_40MHZ(wlc->bandstate[bandunit]->bw_cap));
}

/* Is 80 MHz allowed for the current locale and specified band? */
bool
wlc_valid_80chanspec_in_band(wlc_cm_info_t *wlc_cm, uint bandunit)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	return (((wlc_cm->bandstate[bandunit].locale_flags & (WLC_NO_MIMO | WLC_NO_80MHZ)) == 0) &&
	        WL_BW_CAP_80MHZ(wlc->bandstate[bandunit]->bw_cap));
}

#ifndef PPR_API
/* Set tx power limits */
/* BMAC_NOTE: this only needs a chanspec so that it can choose which 20/40 limits
 * to save in phy state. Would not need this if we either saved all the limits and
 * applied them only when we were on the correct channel, or we restricted this fn
 * to be called only when on the correct channel.
 */
static void
wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm_info_t *wlc_cm,
	txppr_t *txpwr, uint8 local_constraint_qdbm)
{
	int j;
	for (j = 0; j < WL_TX_POWER_RATES; j++) {
		if (((int8*)txpwr)[j] > (int8)local_constraint_qdbm)
			((int8*)txpwr)[j] = (int8)local_constraint_qdbm;
	}
	/* We don't need to worry about WL_RATE_DISABLED cases since its defined
	 * as -128 anyway, which means that any index that its set at will not be
	 * changed by this function.
	 */
}
#endif /* !PPR_API */

static void
#ifdef PPR_API
wlc_channel_txpower_limits(wlc_cm_info_t *wlc_cm, ppr_t *txpwr)
#else
wlc_channel_txpower_limits(wlc_cm_info_t *wlc_cm, txppr_t *txpwr)
#endif
{
	uint8 local_constraint;
	wlc_info_t *wlc = wlc_cm->wlc;

	local_constraint = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);

	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, txpwr);

#ifdef PPR_API
	ppr_apply_max(txpwr, local_constraint);
#else
	wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, txpwr, local_constraint);
#endif
}

#ifdef PPR_API
void
wlc_channel_set_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chanspec, uint8 local_constraint_qdbm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	ppr_t* txpwr;

	if ((txpwr = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(chanspec))) == NULL) {
		return;
	}


	wlc_channel_reg_limits(wlc_cm, chanspec, txpwr);

/* wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, &txpwr, local_constraint_qdbm); */
	ppr_apply_max(txpwr, local_constraint_qdbm);

	wlc_channel_update_txchain_offsets(wlc_cm, txpwr);

	wlc_bmac_set_chanspec(wlc->hw, chanspec, (wlc_quiet_chanspec(wlc_cm, chanspec) != 0),
		txpwr);

	ppr_delete(wlc_cm->pub->osh, txpwr);
}

int
wlc_channel_set_txpower_limit(wlc_cm_info_t *wlc_cm, uint8 local_constraint_qdbm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	ppr_t *txpwr;



	if ((txpwr = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
		return BCME_ERROR;
	}

	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, txpwr);
/* wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, txpwr, local_constraint_qdbm); */
	ppr_apply_max(txpwr, local_constraint_qdbm);

	wlc_channel_update_txchain_offsets(wlc_cm, txpwr);

	wlc_phy_txpower_limit_set(wlc->band->pi, txpwr, wlc->chanspec);

	ppr_delete(wlc_cm->pub->osh, txpwr);
	return 0;
}


#else

void
wlc_channel_set_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chanspec, uint8 local_constraint_qdbm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr;

	wlc_channel_reg_limits(wlc_cm, chanspec, &txpwr);

	wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, &txpwr, local_constraint_qdbm);

	wlc_channel_update_txchain_offsets(wlc_cm, &txpwr);

	wlc_bmac_set_chanspec(wlc->hw, chanspec, (wlc_quiet_chanspec(wlc_cm, chanspec) != 0),
		&txpwr);
}

int
wlc_channel_set_txpower_limit(wlc_cm_info_t *wlc_cm, uint8 local_constraint_qdbm)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr;

	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, &txpwr);
	wlc_channel_min_txpower_limits_with_local_constraint(wlc_cm, &txpwr, local_constraint_qdbm);

	wlc_channel_update_txchain_offsets(wlc_cm, &txpwr);

	wlc_phy_txpower_limit_set(wlc->band->pi, &txpwr, wlc->chanspec);

	return 0;
}
#endif /* PPR_API */

#ifdef WL11AC
static clm_limits_type_t clm_sideband_to_limits_type(uint sb)
{
	clm_limits_type_t lt = CLM_LIMITS_TYPE_CHANNEL;

	switch (sb) {
	case WL_CHANSPEC_CTL_SB_LL:
		lt = CLM_LIMITS_TYPE_SUBCHAN_LL;
		break;
	case WL_CHANSPEC_CTL_SB_LU:
		lt = CLM_LIMITS_TYPE_SUBCHAN_LU;
		break;
	case WL_CHANSPEC_CTL_SB_UL:
		lt = CLM_LIMITS_TYPE_SUBCHAN_UL;
		break;
	case WL_CHANSPEC_CTL_SB_UU:
		lt = CLM_LIMITS_TYPE_SUBCHAN_UU;
		break;
	default:
		break;
	}
	return lt;
}
#endif /* WL11AC */

#ifdef WL_SARLIMIT
#define MAXNUM_SAR_ENTRY	(sizeof(sar_default)/sizeof(sar_default[0]))
const struct {
	uint16	boardtype;
	sar_limit_t sar;
} sar_default[] = {
	{BCM94331X29B, {{QDB(17)+2, QDB(16), QDB(17)+2, WLC_TXPWR_MAX}, /* 2g SAR limits */
	{{QDB(14)+2, QDB(14)+2, QDB(15)+2, WLC_TXPWR_MAX}, /* 5g subband 0 SAR limits */
	{QDB(15), QDB(14)+2, QDB(15), WLC_TXPWR_MAX}, /* 5g subband 1 SAR limits */
	{QDB(17), QDB(15), QDB(17), WLC_TXPWR_MAX}, /* 5g subband 2 SAR limits */
	{QDB(18), QDB(15), QDB(18), WLC_TXPWR_MAX}  /* 5g subband 3 SAR limits */
	}}},
	{BCM94331X29D, {{QDB(18), QDB(18), QDB(18), WLC_TXPWR_MAX}, /* 2g SAR limits */
	{{QDB(16)+2, QDB(16)+2, QDB(16)+2, WLC_TXPWR_MAX}, /* 5g subband 0 SAR limits */
	{QDB(16), QDB(17), QDB(17), WLC_TXPWR_MAX}, /* 5g subband 1 SAR limits */
	{QDB(16)+2, QDB(16)+2, QDB(16)+2, WLC_TXPWR_MAX}, /* 5g subband 2 SAR limits */
	{QDB(17)+2, QDB(16)+2, QDB(17)+2, WLC_TXPWR_MAX}  /* 5g subband 3 SAR limits */
	}}},
	{BCM94360X29C, {{QDB(18), QDB(18), QDB(18), WLC_TXPWR_MAX}, /* 2g SAR limits */
	{{QDB(16)+2, QDB(16)+2, QDB(16)+2, WLC_TXPWR_MAX}, /* 5g subband 0 SAR limits */
	{QDB(16), QDB(17), QDB(17), WLC_TXPWR_MAX}, /* 5g subband 1 SAR limits */
	{QDB(16)+2, QDB(16)+2, QDB(16)+2, WLC_TXPWR_MAX}, /* 5g subband 2 SAR limits */
	{QDB(17)+2, QDB(16)+2, QDB(17)+2, WLC_TXPWR_MAX}  /* 5g subband 3 SAR limits */
	}}},
};

static void
wlc_channel_sarlimit_get_default(wlc_cm_info_t *wlc_cm, sar_limit_t *sar)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	uint idx;

	for (idx = 0; idx < MAXNUM_SAR_ENTRY; idx++) {
		if (sar_default[idx].boardtype == wlc->pub->sih->boardtype) {
			memcpy((uint8 *)sar, (uint8 *)&(sar_default[idx].sar), sizeof(sar_limit_t));
			break;
		}
	}
}

void
wlc_channel_sar_init(wlc_cm_info_t *wlc_cm)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	memset((uint8 *)wlc_cm->sarlimit.band2g,
	       wlc->bandstate[BAND_2G_INDEX]->sar,
	       WLC_TXCORE_MAX);
	memset((uint8 *)wlc_cm->sarlimit.band5g,
	       wlc->bandstate[BAND_5G_INDEX]->sar,
	       (WLC_TXCORE_MAX * WLC_SUBBAND_MAX));

	wlc_channel_sarlimit_get_default(wlc_cm, &wlc_cm->sarlimit);
#ifdef BCMDBG
	wlc_channel_sarlimit_dump(wlc_cm, &wlc_cm->sarlimit);
#endif /* BCMDBG */
}

#ifdef BCMDBG
void
wlc_channel_sarlimit_dump(wlc_cm_info_t *wlc_cm, sar_limit_t *sar)
{
	int i;

	WL_ERROR(("\t2G:    %2d%s %2d%s %2d%s %2d%s\n",
	          QDB_FRAC(sar->band2g[0]), QDB_FRAC(sar->band2g[1]),
	          QDB_FRAC(sar->band2g[2]), QDB_FRAC(sar->band2g[3])));
	for (i = 0; i < WLC_SUBBAND_MAX; i++) {
		WL_ERROR(("\t5G[%1d]  %2d%s %2d%s %2d%s %2d%s\n", i,
		          QDB_FRAC(sar->band5g[i][0]), QDB_FRAC(sar->band5g[i][1]),
		          QDB_FRAC(sar->band5g[i][2]), QDB_FRAC(sar->band5g[i][3])));
	}
}
#endif /* BCMDBG */
int
wlc_channel_sarlimit_get(wlc_cm_info_t *wlc_cm, sar_limit_t *sar)
{
	memcpy((uint8 *)sar, (uint8 *)&wlc_cm->sarlimit, sizeof(sar_limit_t));
	return 0;
}

int
wlc_channel_sarlimit_set(wlc_cm_info_t *wlc_cm, sar_limit_t *sar)
{
	memcpy((uint8 *)&wlc_cm->sarlimit, (uint8 *)sar, sizeof(sar_limit_t));
	return 0;
}

/* given chanspec and return the subband index */
static uint
wlc_channel_sarlimit_subband_idx(wlc_cm_info_t *wlc_cm, chanspec_t chanspec)
{
	uint8 chan = CHSPEC_CHANNEL(chanspec);
	if (chan < CHANNEL_5G_MID_START)
		return 0;
	else if (chan >= CHANNEL_5G_MID_START && chan < CHANNEL_5G_HIGH_START)
		return 1;
	else if (chan >= CHANNEL_5G_HIGH_START && chan < CHANNEL_5G_UPPER_START)
		return 2;
	else
		return 3;
}

/* Get the sar limit for the subband containing this channel */
void
wlc_channel_sarlimit_subband(wlc_cm_info_t *wlc_cm, chanspec_t chanspec, uint8 *sar)
{
	int idx = 0;
	wlc_info_t *wlc = wlc_cm->wlc;

	if (BAND_5G(wlc->band->bandtype)) {
		idx = wlc_channel_sarlimit_subband_idx(wlc_cm, chanspec);
		memcpy((uint8 *)sar, (uint8 *)wlc_cm->sarlimit.band5g[idx], WLC_TXCORE_MAX);
	} else {
		memcpy((uint8 *)sar, (uint8 *)wlc_cm->sarlimit.band2g, WLC_TXCORE_MAX);
	}
}
#endif /* WL_SARLIMIT */

bool
wlc_channel_sarenable_get(wlc_cm_info_t *wlc_cm)
{
	return (wlc_cm->sar_enable);
}

void
wlc_channel_sarenable_set(wlc_cm_info_t *wlc_cm, bool state)
{
	wlc_cm->sar_enable = state ? TRUE : FALSE;
}

#ifdef PPR_API
void
wlc_channel_reg_limits(wlc_cm_info_t *wlc_cm, chanspec_t chanspec, ppr_t *txpwr)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	unsigned int chan;
	clm_country_t country;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale;
	clm_power_limits_t limits;
	uint8 flags;
	clm_band_t bandtype;
	wlcband_t * band;
	bool filt_war = FALSE;
	int ant_gain;
	clm_limits_params_t lim_params;
#ifdef WL_SARLIMIT
	uint8 sarlimit[WLC_TXCORE_MAX];
#endif

/* memset(txpwr, (unsigned char)WL_RATE_DISABLED, WL_TX_POWER_RATES); */
	ppr_clear(txpwr);

	if (clm_limits_params_init(&lim_params) != CLM_RESULT_OK)
		return;

	/* Lookup channel in autocountry_default if not in current country */
	if (!wlc_valid_chanspec_db(wlc_cm, chanspec)) {
		if (WLC_AUTOCOUNTRY_ENAB(wlc)) {
			const char *def = wlc_11d_get_autocountry_default(wlc->m11d);
			result = wlc_country_lookup(wlc, def, &country);
		}
		if (result != CLM_RESULT_OK)
			return;
	} else
		country = wlc_cm->country;

	chan = CHSPEC_CHANNEL(chanspec);
	band = wlc->bandstate[CHSPEC_WLCBANDUNIT(chanspec)];
	bandtype = BAND_5G(band->bandtype) ? CLM_BAND_5G : CLM_BAND_2G;
	ant_gain = band->antgain;
	lim_params.sar = WLC_TXPWR_MAX;
	band->sar = band->sar_cached;
	if (wlc_cm->sar_enable) {
#ifdef WL_SARLIMIT
		/* when WL_SARLIMIT is enabled, update band->sar = MAX(sarlimit[i]) */
		wlc_channel_sarlimit_subband(wlc_cm, chanspec, sarlimit);
		if (wlc->pub->sih->boardtype != BCM94360CS) {
			uint i;
			band->sar = 0;
			for (i = 0; i < WLC_BITSCNT(wlc->stf->hw_txchain); i++)
				band->sar = MAX(band->sar, sarlimit[i]);

			WL_NONE(("%s: in %s Band, SAR %d apply\n", __FUNCTION__,
				(BAND_5G(wlc->band->bandtype) ? "5G" : "2G"), band->sar));
		}
		wlc_iovar_setint(wlc, "phy_sarlimit",
			(uint32)(sarlimit[0] | sarlimit[1] << 8 |
		                 sarlimit[2] << 16 | sarlimit[3] << 24));
#endif /* WL_SARLIMIT */
		lim_params.sar = band->sar;
	}

#ifdef WLTEST
	if (strcmp(wlc_cm->country_abbrev, "#a") == 0) {
		band->sar = WLC_TXPWR_MAX;
		lim_params.sar = WLC_TXPWR_MAX;
		wlc_iovar_setint(wlc, "phy_sarlimit",
		        ((WLC_TXPWR_MAX | 0xff) | (WLC_TXPWR_MAX | 0xff) << 8 |
		         (WLC_TXPWR_MAX | 0xff) << 16 | (WLC_TXPWR_MAX | 0xff) << 24));
	}
#endif
	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK)
		return;

	result = wlc_get_flags(&locale, bandtype, &flags);
	if (result != CLM_RESULT_OK)
		return;

	if (wlc_cm->bandedge_filter_apply &&
	    (flags & WLC_FILT_WAR) &&
	    (chan == 1 || chan == 13))
		filt_war = TRUE;
	wlc_bmac_filter_war_upd(wlc->hw, filt_war);

	/* Need to set the txpwr_local_max to external reg max for
	 * this channel as per the locale selected for AP.
	 */
#ifdef AP
	if (AP_ONLY(wlc->pub)) {
		uint8 ch = CHSPEC_CHANNEL(wlc->chanspec);
		uint8 pwr;
		pwr = wlc_get_reg_max_power_for_channel(wlc->cmi, ch, TRUE);
		wlc_tpc_set_local_max(wlc->tpc, pwr);
	}
#endif

	/* Get 20MHz limits */
	if (CHSPEC_IS20(chanspec)) {
		lim_params.bw = CLM_BW_20;
		if (clm_power_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
			&lim_params, &limits) == CLM_RESULT_OK) {

			/* Port the 20MHz values */
			ppr_set_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_1, CLM_DSSS_RATESET(limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(limits));

#ifdef WL11N
			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_MCS_1X1_RATESET(limits));


			ppr_set_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_2,
				CLM_DSSS_1X2_MULTI_RATESET(limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, CLM_MCS_1X2_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, CLM_MCS_2X2_STBC_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, CLM_MCS_2X2_SDM_RATESET(limits));

			ppr_set_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_3,
				CLM_DSSS_1X3_MULTI_RATESET(limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3, CLM_MCS_1X3_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_3, CLM_MCS_2X3_STBC_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_2X3_SDM_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_3X3_SDM_RATESET(limits));
#if defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1)
			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_1X2_TXBF_RATESET(limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_2X2_TXBF_RATESET(limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_1X3_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_2X3_TXBF_RATESET(limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_3X3_TXBF_RATESET(limits));
#endif /* defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1) */
#endif /* WL11N */
		}

	} else if (CHSPEC_IS40(chanspec)) {
	/* Get 40MHz and 20in40MHz limits */
		clm_power_limits_t bw20in40_limits;

		lim_params.bw = CLM_BW_40;

		if ((clm_power_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
			&lim_params, &limits) == CLM_RESULT_OK) &&

			(clm_power_limits(&locale, bandtype, chan, ant_gain,
			CHSPEC_SB_UPPER(chanspec) ?
			CLM_LIMITS_TYPE_SUBCHAN_U : CLM_LIMITS_TYPE_SUBCHAN_L,
			&lim_params, &bw20in40_limits) == CLM_RESULT_OK)) {

			/* Port the 40MHz values */


			ppr_set_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(limits));

#ifdef WL11N
			/* Skip WL_RATE_1X1_DSSS_1 - not valid for 40MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_MCS_1X1_RATESET(limits));

			/* Skip WL_RATE_1X2_DSSS_1 - not valid for 40MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, CLM_MCS_1X2_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, CLM_MCS_2X2_STBC_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, CLM_MCS_2X2_SDM_RATESET(limits));

			/* Skip WL_RATE_1X3_DSSS_1 - not valid for 40MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3, CLM_MCS_1X3_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_3, CLM_MCS_2X3_STBC_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_2X3_SDM_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_3X3_SDM_RATESET(limits));

#if defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1)
			ppr_set_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_1X2_TXBF_RATESET(limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_2X2_TXBF_RATESET(limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_1X3_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_2X3_TXBF_RATESET(limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_3X3_TXBF_RATESET(limits));

#endif /* defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1) */
			/* Port the 20in40 values */


			ppr_set_dsss(txpwr, WL_TX_BW_20,
				WL_TX_CHAINS_1, CLM_DSSS_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_OFDM_1X1_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_MCS_1X1_RATESET(bw20in40_limits));


			ppr_set_dsss(txpwr, WL_TX_BW_20,
				WL_TX_CHAINS_2, CLM_DSSS_1X2_MULTI_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_CDD_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, CLM_MCS_1X2_CDD_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, CLM_MCS_2X2_STBC_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, CLM_MCS_2X2_SDM_RATESET(bw20in40_limits));

			ppr_set_dsss(txpwr, WL_TX_BW_20,
				WL_TX_CHAINS_3, CLM_DSSS_1X3_MULTI_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3, CLM_MCS_1X3_CDD_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_3, CLM_MCS_2X3_STBC_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_2X3_SDM_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_3X3_SDM_RATESET(bw20in40_limits));


			ppr_set_dsss(txpwr, WL_TX_BW_20IN40,
				WL_TX_CHAINS_1, CLM_DSSS_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_MCS_1X1_RATESET(bw20in40_limits));


			ppr_set_dsss(txpwr, WL_TX_BW_20IN40,
				WL_TX_CHAINS_2, CLM_DSSS_1X2_MULTI_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_CDD_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, CLM_MCS_1X2_CDD_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, CLM_MCS_2X2_STBC_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, CLM_MCS_2X2_SDM_RATESET(bw20in40_limits));

			ppr_set_dsss(txpwr, WL_TX_BW_20IN40,
				WL_TX_CHAINS_3, CLM_DSSS_1X3_MULTI_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3, CLM_MCS_1X3_CDD_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_3, CLM_MCS_2X3_STBC_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_2X3_SDM_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_3, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_3X3_SDM_RATESET(bw20in40_limits));

#if defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1)
			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_TXBF_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_1X2_TXBF_RATESET(bw20in40_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_2X2_TXBF_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_1X3_TXBF_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_2X3_TXBF_RATESET(bw20in40_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_3X3_TXBF_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_TXBF_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_1X2_TXBF_RATESET(bw20in40_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_2X2_TXBF_RATESET(bw20in40_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_1X3_TXBF_RATESET(bw20in40_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_2X3_TXBF_RATESET(bw20in40_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_3, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_3X3_TXBF_RATESET(bw20in40_limits));
#endif /* defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1) */
#endif /* WL11N */
		}

	} else {
#ifdef WL11AC
	/* Get 80MHz, 40in80MHz and 20in40MHz limits */
		clm_power_limits_t bw20in80_limits;
		clm_power_limits_t bw40in80_limits;
		clm_limits_type_t sb40 = CLM_LIMITS_TYPE_SUBCHAN_L;

		uint sb = CHSPEC_CTL_SB(chanspec);

		lim_params.bw = CLM_BW_80;

		if ((sb == WL_CHANSPEC_CTL_SB_UU) || (sb == WL_CHANSPEC_CTL_SB_UL))
			/* Primary 40MHz is on upper side */
			sb40 = CLM_LIMITS_TYPE_SUBCHAN_U;


		if ((clm_power_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
			&lim_params, &limits) == CLM_RESULT_OK) &&

			(clm_power_limits(&locale, bandtype, chan, ant_gain,
			sb40, &lim_params, &bw40in80_limits) == CLM_RESULT_OK) &&

			(clm_power_limits(&locale, bandtype, chan, ant_gain,
			clm_sideband_to_limits_type(sb), &lim_params,
			&bw20in80_limits) == CLM_RESULT_OK)) {

			/* Port the 80MHz values */

			ppr_set_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(limits));

#ifdef WL11N
			/* Skip WL_RATE_1X1_DSSS_1 - not valid for 80MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_MCS_1X1_RATESET(limits));

			/* Skip WL_RATE_1X2_DSSS_1 - not valid for 80MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, CLM_MCS_1X2_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, CLM_MCS_2X2_STBC_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, CLM_MCS_2X2_SDM_RATESET(limits));

			/* Skip WL_RATE_1X3_DSSS_1 - not valid for 80MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3, CLM_MCS_1X3_CDD_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_3, CLM_MCS_2X3_STBC_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_2X3_SDM_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_3X3_SDM_RATESET(limits));


#if defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1)
			ppr_set_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_1X2_TXBF_RATESET(limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_2X2_TXBF_RATESET(limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_1X3_TXBF_RATESET(limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_2X3_TXBF_RATESET(limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_3X3_TXBF_RATESET(limits));
#endif /* defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1) */
			/* Port the 40in80 values */

			/* Skip WL_RATE_1X1_DSSS_1 - not valid for 40MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_MCS_1X1_RATESET(bw40in80_limits));


			/* Skip WL_RATE_1X2_DSSS_1 - not valid for 40MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_CDD_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, CLM_MCS_1X2_CDD_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, CLM_MCS_2X2_STBC_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, CLM_MCS_2X2_SDM_RATESET(bw40in80_limits));

			/* Skip WL_RATE_1X3_DSSS_1 - not valid for 40MHz */
			ppr_set_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3, CLM_MCS_1X3_CDD_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_3, CLM_MCS_2X3_STBC_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_2X3_SDM_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_3, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_3X3_SDM_RATESET(bw40in80_limits));

#if defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1)
			ppr_set_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_TXBF_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_1X2_TXBF_RATESET(bw40in80_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_2X2_TXBF_RATESET(bw40in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_1X3_TXBF_RATESET(bw40in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_2X3_TXBF_RATESET(bw40in80_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_3, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_3X3_TXBF_RATESET(bw40in80_limits));
#endif /* defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1) */
			/* Port the 20in80 values */

			ppr_set_dsss(txpwr, WL_TX_BW_20,
				WL_TX_CHAINS_1, CLM_DSSS_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_MCS_1X1_RATESET(bw20in80_limits));


			ppr_set_dsss(txpwr, WL_TX_BW_20,
				WL_TX_CHAINS_2, CLM_DSSS_1X2_MULTI_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_CDD_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, CLM_MCS_1X2_CDD_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, CLM_MCS_2X2_STBC_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, CLM_MCS_2X2_SDM_RATESET(bw20in80_limits));

			ppr_set_dsss(txpwr, WL_TX_BW_20,
				WL_TX_CHAINS_3, CLM_DSSS_1X3_MULTI_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3, CLM_MCS_1X3_CDD_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_3, CLM_MCS_2X3_STBC_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_2X3_SDM_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_3X3_SDM_RATESET(bw20in80_limits));


			ppr_set_dsss(txpwr, WL_TX_BW_20IN80,
				WL_TX_CHAINS_1, CLM_DSSS_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_NONE, WL_TX_CHAINS_1,
				CLM_OFDM_1X1_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_NONE,
				WL_TX_CHAINS_1, CLM_MCS_1X1_RATESET(bw20in80_limits));


			ppr_set_dsss(txpwr, WL_TX_BW_20IN80,
				WL_TX_CHAINS_2, CLM_DSSS_1X2_MULTI_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_CDD_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, CLM_MCS_1X2_CDD_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_2, CLM_MCS_2X2_STBC_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_2, CLM_MCS_2X2_SDM_RATESET(bw20in80_limits));

			ppr_set_dsss(txpwr, WL_TX_BW_20IN80,
				WL_TX_CHAINS_3, CLM_DSSS_1X3_MULTI_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_CDD_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_3, CLM_MCS_1X3_CDD_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
				WL_TX_CHAINS_3, CLM_MCS_2X3_STBC_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_2X3_SDM_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_3, WL_TX_MODE_NONE,
				WL_TX_CHAINS_3, CLM_MCS_3X3_SDM_RATESET(bw20in80_limits));

#if defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1)
			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_TXBF_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_1X2_TXBF_RATESET(bw20in80_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_2X2_TXBF_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_1X3_TXBF_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_2X3_TXBF_RATESET(bw20in80_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_3X3_TXBF_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_TXBF, WL_TX_CHAINS_2,
				CLM_OFDM_1X2_TXBF_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_1X2_TXBF_RATESET(bw20in80_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_2, CLM_MCS_2X2_TXBF_RATESET(bw20in80_limits));

			ppr_set_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_TXBF, WL_TX_CHAINS_3,
				CLM_OFDM_1X3_TXBF_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_1X3_TXBF_RATESET(bw20in80_limits));

			ppr_set_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_2X3_TXBF_RATESET(bw20in80_limits));

			ppr_set_ht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_3, WL_TX_MODE_TXBF,
				WL_TX_CHAINS_3, CLM_MCS_3X3_TXBF_RATESET(bw20in80_limits));
#endif /* defined(WL_BEAMFORMING) && (PPR_MAX_TX_CHAINS > 1) */
#endif /* WL11N */
		}
#endif /* WL11AC */

	}

	WL_NONE(("Channel(chanspec) %d (0x%4.4x)\n", chan, chanspec));
#ifdef WLC_LOW
	/* Convoluted WL debug conditional execution of function to avoid warnings. */
	WL_NONE(("%s", (wlc_phy_txpower_limits_dump(txpwr, WLCISHTPHY(wlc->band)), "")));
#endif /* WLC_LOW */

}

#else
void
wlc_channel_reg_limits(wlc_cm_info_t *wlc_cm, chanspec_t chanspec, txppr_t *txpwr)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	unsigned int chan;
	clm_country_t country;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale;
	clm_power_limits_t limits;
	uint8 flags;
	clm_band_t bandtype;
	wlcband_t * band;
	bool filt_war = FALSE;
	int ant_gain;
	clm_limits_params_t lim_params;

	if (clm_limits_params_init(&lim_params) != CLM_RESULT_OK)
		return;

	memset(txpwr, (unsigned char)WL_RATE_DISABLED, WL_TX_POWER_RATES);
	/* Lookup channel in autocountry_default if not in current country */
	if (!wlc_valid_chanspec_db(wlc_cm, chanspec)) {
		if (WLC_AUTOCOUNTRY_ENAB(wlc)) {
			const char *def = wlc_11d_get_autocountry_default(wlc->m11d);
			result = wlc_country_lookup(wlc, def, &country);
		}
		if (result != CLM_RESULT_OK)
			return;
	} else
		country = wlc_cm->country;

	chan = CHSPEC_CHANNEL(chanspec);
	band = wlc->bandstate[CHSPEC_WLCBANDUNIT(chanspec)];
	bandtype = BAND_5G(band->bandtype) ? CLM_BAND_5G : CLM_BAND_2G;
	ant_gain = band->antgain;
	lim_params.sar = WLC_TXPWR_MAX;
	if (wlc_cm->sar_enable)
		lim_params.sar = band->sar;

	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK)
		return;

	result = wlc_get_flags(&locale, bandtype, &flags);
	if (result != CLM_RESULT_OK)
		return;

	if (wlc_cm->bandedge_filter_apply &&
	    (flags & WLC_FILT_WAR) &&
	    (chan == 1 || chan == 13))
		filt_war = TRUE;
	wlc_bmac_filter_war_upd(wlc->hw, filt_war);

	/* Need to set the txpwr_local_max to external reg max for
	 * this channel as per the locale selected for AP.
	 */
#ifdef AP
	if (AP_ONLY(wlc->pub)) {
		uint8 ch = CHSPEC_CHANNEL(wlc->chanspec);
		uint8 pwr = wlc_get_reg_max_power_for_channel(wlc->cmi, ch, TRUE);
		wlc_tpc_set_local_max(wlc->tpc, pwr);
	}
#endif

	if (CHSPEC_IS20(chanspec)) {
		/* Get 20MHz limits */
		lim_params.bw = CLM_BW_20;
		if (clm_power_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
			&lim_params, &limits) == CLM_RESULT_OK) {

			/* Port the 20MHz values */
			COPY_DSSS_LIMS(limits, WL_RATE_1X1_DSSS_1, b20_1x1dsss);
			COPY_OFDM_LIMS(limits, WL_RATE_1X1_OFDM_6, b20_1x1ofdm);

#ifdef WL11N
			COPY_MCS_LIMS(limits, WL_RATE_1X1_MCS0, b20_1x1mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X1_VHT8SS1, b20_1x1vht);

			COPY_DSSS_LIMS(limits, WL_RATE_1X2_DSSS_1, b20_1x2dsss);
			COPY_OFDM_LIMS(limits, WL_RATE_1X2_CDD_OFDM_6, b20_1x2cdd_ofdm);
			COPY_MCS_LIMS(limits, WL_RATE_1X2_CDD_MCS0, b20_1x2cdd_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X2_VHT8SS1, b20_1x2cdd_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X2_STBC_MCS0, b20_2x2stbc_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_2X2_STBC_VHT8SS1, b20_2x2stbc_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X2_SDM_MCS8, b20_2x2sdm_mcs8);
			COPY_VHT_LIMS(limits, WL_RATE_2X2_VHT8SS2, b20_2x2sdm_vht);

			COPY_DSSS_LIMS(limits, WL_RATE_1X3_DSSS_1, b20_1x3dsss);
			COPY_OFDM_LIMS(limits, WL_RATE_1X3_CDD_OFDM_6, b20_1x3cdd_ofdm);
			COPY_MCS_LIMS(limits, WL_RATE_1X3_CDD_MCS0, b20_1x3cdd_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X3_VHT8SS1, b20_1x3cdd_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X3_STBC_MCS0, b20_2x3stbc_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_2X3_STBC_VHT8SS1, b20_2x3stbc_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X3_SDM_MCS8, b20_2x3sdm_mcs8);
			COPY_VHT_LIMS(limits, WL_RATE_2X3_VHT8SS2, b20_2x3sdm_vht);
			COPY_MCS_LIMS(limits, WL_RATE_3X3_SDM_MCS16, b20_3x3sdm_mcs16);
			COPY_VHT_LIMS(limits, WL_RATE_3X3_VHT8SS3, b20_3x3sdm_vht);
		}

	} else if (CHSPEC_IS40(chanspec)) {
		/* Get 40MHz and 20in40MHz limits */
		clm_power_limits_t bw20in40_limits;

		lim_params.bw = CLM_BW_40;

		if ((clm_power_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
			&lim_params, &limits) == CLM_RESULT_OK) &&

			(clm_power_limits(&locale, bandtype, chan, ant_gain,
			CHSPEC_SB_UPPER(chanspec) ?
			CLM_LIMITS_TYPE_SUBCHAN_U : CLM_LIMITS_TYPE_SUBCHAN_L,
			&lim_params, &bw20in40_limits) == CLM_RESULT_OK)) {


			/* Port the 40MHz values */
			/* Skip WL_RATE_1X1_DSSS_1 - not valid for 40MHz */
			COPY_OFDM_LIMS(limits, WL_RATE_1X1_OFDM_6, b40_1x1ofdm);
			COPY_MCS_LIMS(limits, WL_RATE_1X1_MCS0, b40_1x1mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X1_VHT8SS1, b40_1x1vht);

			/* Skip WL_RATE_1X2_DSSS_1 - not valid for 40MHz */
			COPY_OFDM_LIMS(limits, WL_RATE_1X2_CDD_OFDM_6, b40_1x2cdd_ofdm);
			COPY_MCS_LIMS(limits, WL_RATE_1X2_CDD_MCS0, b40_1x2cdd_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X2_VHT8SS1, b40_1x2cdd_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X2_STBC_MCS0, b40_2x2stbc_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_2X2_STBC_VHT8SS1, b40_2x2stbc_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X2_SDM_MCS8, b40_2x2sdm_mcs8);
			COPY_VHT_LIMS(limits, WL_RATE_2X2_VHT8SS2, b40_2x2sdm_vht);

			/* Skip WL_RATE_1X3_DSSS_1 - not valid for 40MHz */
			COPY_OFDM_LIMS(limits, WL_RATE_1X3_CDD_OFDM_6, b40_1x3cdd_ofdm);
			COPY_MCS_LIMS(limits, WL_RATE_1X3_CDD_MCS0, b40_1x3cdd_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X3_VHT8SS1, b40_1x3cdd_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X3_STBC_MCS0, b40_2x3stbc_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_2X3_STBC_VHT8SS1, b40_2x3stbc_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X3_SDM_MCS8, b40_2x3sdm_mcs8);
			COPY_VHT_LIMS(limits, WL_RATE_2X3_VHT8SS2, b40_2x3sdm_vht);
			COPY_MCS_LIMS(limits, WL_RATE_3X3_SDM_MCS16, b40_3x3sdm_mcs16);
			COPY_VHT_LIMS(limits, WL_RATE_3X3_VHT8SS3, b40_3x3sdm_vht);


			/* Port the 20in40 values */
			COPY_DSSS_LIMS(bw20in40_limits, WL_RATE_1X1_DSSS_1, b20_1x1dsss);
			COPY_OFDM_LIMS(bw20in40_limits, WL_RATE_1X1_OFDM_6, b20_1x1ofdm);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_1X1_MCS0, b20_1x1mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_1X1_VHT8SS1, b20_1x1vht);

			COPY_DSSS_LIMS(bw20in40_limits, WL_RATE_1X2_DSSS_1, b20_1x2dsss);
			COPY_OFDM_LIMS(bw20in40_limits, WL_RATE_1X2_CDD_OFDM_6, b20_1x2cdd_ofdm);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_1X2_CDD_MCS0, b20_1x2cdd_mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_1X2_VHT8SS1, b20_1x2cdd_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_2X2_STBC_MCS0, b20_2x2stbc_mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_2X2_STBC_VHT8SS1, b20_2x2stbc_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_2X2_SDM_MCS8, b20_2x2sdm_mcs8);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_2X2_VHT8SS2, b20_2x2sdm_vht);

			COPY_DSSS_LIMS(bw20in40_limits, WL_RATE_1X3_DSSS_1, b20_1x3dsss);
			COPY_OFDM_LIMS(bw20in40_limits, WL_RATE_1X3_CDD_OFDM_6, b20_1x3cdd_ofdm);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_1X3_CDD_MCS0, b20_1x3cdd_mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_1X3_VHT8SS1, b20_1x3cdd_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_2X3_STBC_MCS0, b20_2x3stbc_mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_2X3_STBC_VHT8SS1, b20_2x3stbc_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_2X3_SDM_MCS8, b20_2x3sdm_mcs8);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_2X3_VHT8SS2, b20_2x3sdm_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_3X3_SDM_MCS16, b20_3x3sdm_mcs16);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_3X3_VHT8SS3, b20_3x3sdm_vht);


			COPY_DSSS_LIMS(bw20in40_limits, WL_RATE_1X1_DSSS_1, b20in40_1x1dsss);
			COPY_OFDM_LIMS(bw20in40_limits, WL_RATE_1X1_OFDM_6, b20in40_1x1ofdm);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_1X1_MCS0, b20in40_1x1mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_1X1_VHT8SS1, b20in40_1x1vht);

			COPY_DSSS_LIMS(bw20in40_limits, WL_RATE_1X2_DSSS_1, b20in40_1x2dsss);
			COPY_OFDM_LIMS(bw20in40_limits, WL_RATE_1X2_CDD_OFDM_6,
				b20in40_1x2cdd_ofdm);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_1X2_CDD_MCS0,
				b20in40_1x2cdd_mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_1X2_VHT8SS1,
				b20in40_1x2cdd_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_2X2_STBC_MCS0,
				b20in40_2x2stbc_mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_2X2_STBC_VHT8SS1,
				b20in40_2x2stbc_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_2X2_SDM_MCS8, b20in40_2x2sdm_mcs8);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_2X2_VHT8SS2, b20in40_2x2sdm_vht);

			COPY_DSSS_LIMS(bw20in40_limits, WL_RATE_1X3_DSSS_1, b20in40_1x3dsss);
			COPY_OFDM_LIMS(bw20in40_limits, WL_RATE_1X3_CDD_OFDM_6,
				b20in40_1x3cdd_ofdm);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_1X3_CDD_MCS0, b20in40_1x3cdd_mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_1X3_VHT8SS1, b20in40_1x3cdd_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_2X3_STBC_MCS0,
				b20in40_2x3stbc_mcs0);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_2X3_STBC_VHT8SS1,
				b20in40_2x3stbc_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_2X3_SDM_MCS8, b20in40_2x3sdm_mcs8);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_2X3_VHT8SS2, b20in40_2x3sdm_vht);
			COPY_MCS_LIMS(bw20in40_limits, WL_RATE_3X3_SDM_MCS16,
				b20in40_3x3sdm_mcs16);
			COPY_VHT_LIMS(bw20in40_limits, WL_RATE_3X3_VHT8SS3, b20in40_3x3sdm_vht);

#ifdef WL11AC
		}
	} else {
		/* Get 80MHz 20in80MHz and 40in80MHz limits */
		clm_power_limits_t bw20in80_limits;
		clm_power_limits_t bw40in80_limits;
		clm_limits_type_t sb40 = CLM_LIMITS_TYPE_SUBCHAN_L;

		uint sb = CHSPEC_CTL_SB(chanspec);

		lim_params.bw = CLM_BW_80;

		if ((sb == WL_CHANSPEC_CTL_SB_UU) || (sb == WL_CHANSPEC_CTL_SB_UL))
			/* Primary 40MHz is on upper side */
			sb40 = CLM_LIMITS_TYPE_SUBCHAN_U;

		if ((clm_power_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
			&lim_params, &limits) == CLM_RESULT_OK) &&

			(clm_power_limits(&locale, bandtype, chan, ant_gain,
			sb40, &lim_params, &bw40in80_limits) == CLM_RESULT_OK) &&

			(clm_power_limits(&locale, bandtype, chan, ant_gain,
			clm_sideband_to_limits_type(sb), &lim_params,
			&bw20in80_limits) == CLM_RESULT_OK)) {

			/* Port the 80MHz values */
			/* Skip WL_RATE_1X1_DSSS_1 - not valid for 80MHz */
			COPY_OFDM_LIMS(limits, WL_RATE_1X1_OFDM_6, b80_1x1ofdm);
			COPY_MCS_LIMS(limits, WL_RATE_1X1_MCS0, b80_1x1mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X1_VHT8SS1, b80_1x1vht);

			/* Skip WL_RATE_1X2_DSSS_1 - not valid for 80MHz */
			COPY_OFDM_LIMS(limits, WL_RATE_1X2_CDD_OFDM_6, b80_1x2cdd_ofdm);
			COPY_MCS_LIMS(limits, WL_RATE_1X2_CDD_MCS0, b80_1x2cdd_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X2_VHT8SS1, b80_1x2cdd_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X2_STBC_MCS0, b80_2x2stbc_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_2X2_STBC_VHT8SS1, b80_2x2stbc_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X2_SDM_MCS8, b80_2x2sdm_mcs8);
			COPY_VHT_LIMS(limits, WL_RATE_2X2_VHT8SS2, b80_2x2sdm_vht);

			/* Skip WL_RATE_1X3_DSSS_1 - not valid for 80MHz */
			COPY_OFDM_LIMS(limits, WL_RATE_1X3_CDD_OFDM_6, b80_1x3cdd_ofdm);
			COPY_MCS_LIMS(limits, WL_RATE_1X3_CDD_MCS0, b80_1x3cdd_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_1X3_VHT8SS1, b80_1x3cdd_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X3_STBC_MCS0, b80_2x3stbc_mcs0);
			COPY_VHT_LIMS(limits, WL_RATE_2X3_STBC_VHT8SS1, b80_2x3stbc_vht);
			COPY_MCS_LIMS(limits, WL_RATE_2X3_SDM_MCS8, b80_2x3sdm_mcs8);
			COPY_VHT_LIMS(limits, WL_RATE_2X3_VHT8SS2, b80_2x3sdm_vht);
			COPY_MCS_LIMS(limits, WL_RATE_3X3_SDM_MCS16, b80_3x3sdm_mcs16);
			COPY_VHT_LIMS(limits, WL_RATE_3X3_VHT8SS3, b80_3x3sdm_vht);


			/* Port the 20in80 values */
			COPY_DSSS_LIMS(bw20in80_limits, WL_RATE_1X1_DSSS_1, b20in80_1x1dsss);
			COPY_OFDM_LIMS(bw20in80_limits, WL_RATE_1X1_OFDM_6, b20in80_1x1ofdm);
			COPY_MCS_LIMS(bw20in80_limits, WL_RATE_1X1_MCS0, b20in80_1x1mcs0);
			COPY_VHT_LIMS(bw20in80_limits, WL_RATE_1X1_VHT8SS1, b20in80_1x1vht);

			COPY_DSSS_LIMS(bw20in80_limits, WL_RATE_1X2_DSSS_1, b20in80_1x2dsss);
			COPY_OFDM_LIMS(bw20in80_limits, WL_RATE_1X2_CDD_OFDM_6,
				b20in80_1x2cdd_ofdm);
			COPY_MCS_LIMS(bw20in80_limits, WL_RATE_1X2_CDD_MCS0, b20in80_1x2cdd_mcs0);
			COPY_VHT_LIMS(bw20in80_limits, WL_RATE_1X2_VHT8SS1, b20in80_1x2cdd_vht);
			COPY_MCS_LIMS(bw20in80_limits, WL_RATE_2X2_STBC_MCS0,
				b20in80_2x2stbc_mcs0);
			COPY_VHT_LIMS(bw20in80_limits, WL_RATE_2X2_STBC_VHT8SS1,
				b20in80_2x2stbc_vht);
			COPY_MCS_LIMS(bw20in80_limits, WL_RATE_2X2_SDM_MCS8, b20in80_2x2sdm_mcs8);
			COPY_VHT_LIMS(bw20in80_limits, WL_RATE_2X2_VHT8SS2, b20in80_2x2sdm_vht);

			COPY_DSSS_LIMS(bw20in80_limits, WL_RATE_1X3_DSSS_1, b20in80_1x3dsss);
			COPY_OFDM_LIMS(bw20in80_limits, WL_RATE_1X3_CDD_OFDM_6,
				b20in80_1x3cdd_ofdm);
			COPY_MCS_LIMS(bw20in80_limits, WL_RATE_1X3_CDD_MCS0, b20in80_1x3cdd_mcs0);
			COPY_VHT_LIMS(bw20in80_limits, WL_RATE_1X3_VHT8SS1, b20in80_1x3cdd_vht);
			COPY_MCS_LIMS(bw20in80_limits, WL_RATE_2X3_STBC_MCS0,
				b20in80_2x3stbc_mcs0);
			COPY_VHT_LIMS(bw20in80_limits, WL_RATE_2X3_STBC_VHT8SS1,
				b20in80_2x3stbc_vht);
			COPY_MCS_LIMS(bw20in80_limits, WL_RATE_2X3_SDM_MCS8, b20in80_2x3sdm_mcs8);
			COPY_VHT_LIMS(bw20in80_limits, WL_RATE_2X3_VHT8SS2, b20in80_2x3sdm_vht);
			COPY_MCS_LIMS(bw20in80_limits, WL_RATE_3X3_SDM_MCS16,
				b20in80_3x3sdm_mcs16);
			COPY_VHT_LIMS(bw20in80_limits, WL_RATE_3X3_VHT8SS3, b20in80_3x3sdm_vht);


			/* Port the 40in80 values */
			/* Skip WL_RATE_1X1_DSSS_1 - not valid for 40in80 */
			COPY_OFDM_LIMS(bw40in80_limits, WL_RATE_1X1_OFDM_6, b40in80_1x1ofdm);
			COPY_MCS_LIMS(bw40in80_limits, WL_RATE_1X1_MCS0, b40in80_1x1mcs0);
			COPY_VHT_LIMS(bw40in80_limits, WL_RATE_1X1_VHT8SS1, b40in80_1x1vht);

			/* Skip WL_RATE_1X2_DSSS_1 - not valid for 40in80 */
			COPY_OFDM_LIMS(bw40in80_limits, WL_RATE_1X2_CDD_OFDM_6,
				b40in80_1x2cdd_ofdm);
			COPY_MCS_LIMS(bw40in80_limits, WL_RATE_1X2_CDD_MCS0, b40in80_1x2cdd_mcs0);
			COPY_VHT_LIMS(bw40in80_limits, WL_RATE_1X2_VHT8SS1, b40in80_1x2cdd_vht);
			COPY_MCS_LIMS(bw40in80_limits, WL_RATE_2X2_STBC_MCS0,
				b40in80_2x2stbc_mcs0);
			COPY_VHT_LIMS(bw40in80_limits, WL_RATE_2X2_STBC_VHT8SS1,
				b40in80_2x2stbc_vht);
			COPY_MCS_LIMS(bw40in80_limits, WL_RATE_2X2_SDM_MCS8, b40in80_2x2sdm_mcs8);
			COPY_VHT_LIMS(bw40in80_limits, WL_RATE_2X2_VHT8SS2, b40in80_2x2sdm_vht);

			/* Skip WL_RATE_1X3_DSSS_1 - not valid for 40in80 */
			COPY_OFDM_LIMS(bw40in80_limits, WL_RATE_1X3_CDD_OFDM_6,
				b40in80_1x3cdd_ofdm);
			COPY_MCS_LIMS(bw40in80_limits, WL_RATE_1X3_CDD_MCS0, b40in80_1x3cdd_mcs0);
			COPY_VHT_LIMS(bw40in80_limits, WL_RATE_1X3_VHT8SS1, b40in80_1x3cdd_vht);
			COPY_MCS_LIMS(bw40in80_limits, WL_RATE_2X3_STBC_MCS0,
				b40in80_2x3stbc_mcs0);
			COPY_VHT_LIMS(bw40in80_limits, WL_RATE_2X3_STBC_VHT8SS1,
				b40in80_2x3stbc_vht);
			COPY_MCS_LIMS(bw40in80_limits, WL_RATE_2X3_SDM_MCS8, b40in80_2x3sdm_mcs8);
			COPY_VHT_LIMS(bw40in80_limits, WL_RATE_2X3_VHT8SS2, b40in80_2x3sdm_vht);
			COPY_MCS_LIMS(bw40in80_limits, WL_RATE_3X3_SDM_MCS16,
				b40in80_3x3sdm_mcs16);
			COPY_VHT_LIMS(bw40in80_limits, WL_RATE_3X3_VHT8SS3, b40in80_3x3sdm_vht);

#endif /* WL11AC */
#endif /* WL11N */
		}
	} /* 80MHz */


	WL_NONE(("Channel(chanspec) %d (0x%4.4x)\n", chan, chanspec));
#ifdef WLC_LOW
	/* Convoluted WL debug conditional execution of function to avoid warnings. */
	WL_NONE(("%s", (wlc_phy_txpower_limits_dump(txpwr, WLCISHTPHY(wlc->band)), "")));
#endif /* WLC_LOW */

}
#endif /* PPR_API */

static clm_result_t
clm_power_limits(
	const clm_country_locales_t *locales, clm_band_t band,
	unsigned int chan, int ant_gain, clm_limits_type_t limits_type,
	const clm_limits_params_t *params, clm_power_limits_t *limits)
{
	return clm_limits(locales, band, chan, ant_gain, limits_type, params, limits);
}


/* Returns TRUE if currently set country is Japan or variant */
bool
wlc_japan(struct wlc_info *wlc)
{
	return wlc_japan_ccode(wlc->cmi->country_abbrev);
}

/* JP, J1 - J10 are Japan ccodes */
static bool
wlc_japan_ccode(const char *ccode)
{
	return (ccode[0] == 'J' &&
		(ccode[1] == 'P' || (ccode[1] >= '1' && ccode[1] <= '9')));
}

/* Q2 is an alternate USA ccode */
static bool
wlc_us_ccode(const char *ccode)
{
	return (!strncmp("US", ccode, 3) ||
		!strncmp("Q1", ccode, 3) ||
		!strncmp("Q2", ccode, 3) ||
		!strncmp("ALL", ccode, 3));
}

void
wlc_rcinfo_init(wlc_cm_info_t *wlc_cm)
{
	if (wlc_us_ccode(wlc_cm->country_abbrev)) {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_us_20;
#endif
#ifdef WL11N
		if (N_ENAB(wlc_cm->wlc->pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_us_40lower;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_us_40upper;
		}
#endif
	} else if (wlc_japan_ccode(wlc_cm->country_abbrev)) {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_jp_20;
#endif
#ifdef WL11N
		if (N_ENAB(wlc_cm->wlc->pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_jp_40;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_jp_40;
		}
#endif
	} else {
#ifdef BAND5G
		wlc_cm->rcinfo_list[WLC_RCLIST_20] = &rcinfo_eu_20;
#endif
#ifdef WL11N
		if (N_ENAB(wlc_cm->wlc->pub)) {
			wlc_cm->rcinfo_list[WLC_RCLIST_40L] = &rcinfo_eu_40lower;
			wlc_cm->rcinfo_list[WLC_RCLIST_40U] = &rcinfo_eu_40upper;
		}
#endif
	}
}

static void
wlc_regclass_vec_init(wlc_cm_info_t *wlc_cm)
{
	uint8 i, idx;
	chanspec_t chanspec;
#ifdef WL11N
	wlc_info_t *wlc = wlc_cm->wlc;
	bool saved_cap_40, saved_db_cap_40 = TRUE;
#endif
	rcvec_t *rcvec = &wlc_cm->valid_rcvec;

#ifdef WL11N
	/* save 40 MHz cap */
	saved_cap_40 = WL_BW_CAP_40MHZ(wlc->band->bw_cap);
	wlc->band->bw_cap |= WLC_BW_40MHZ_BIT;
	if (NBANDS(wlc) > 1) {
		saved_db_cap_40 = WL_BW_CAP_40MHZ(wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap);
		wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap |= WLC_BW_40MHZ_BIT;
	}
#endif

	bzero(rcvec, MAXRCVEC);
	for (i = 0; i < MAXCHANNEL; i++) {
		chanspec = CH20MHZ_CHSPEC(i);
		if (wlc_valid_chanspec_db(wlc_cm, chanspec)) {
			if ((idx = wlc_get_regclass(wlc_cm, chanspec)))
				setbit((uint8 *)rcvec, idx);
		}
#if defined(WL11N) && !defined(WL11N_20MHZONLY)
		if (N_ENAB(wlc->pub)) {
			chanspec = CH40MHZ_CHSPEC(i, WL_CHANSPEC_CTL_SB_LOWER);
			if (wlc_valid_chanspec_db(wlc_cm, chanspec)) {
				if ((idx = wlc_get_regclass(wlc_cm, chanspec)))
					setbit((uint8 *)rcvec, idx);
			}
			chanspec = CH40MHZ_CHSPEC(i, WL_CHANSPEC_CTL_SB_UPPER);
			if (wlc_valid_chanspec_db(wlc_cm, chanspec)) {
				if ((idx = wlc_get_regclass(wlc_cm, chanspec)))
					setbit((uint8 *)rcvec, idx);
			}
		}
#endif /* defined(WL11N) && !defined(WL11N_20MHZONLY) */
	}
#ifdef WL11N
	/* restore 40 MHz cap */
	if (saved_cap_40) {
		wlc->band->bw_cap |= WLC_BW_40MHZ_BIT;
	} else {
		wlc->band->bw_cap &= ~WLC_BW_40MHZ_BIT;
	}

	if (NBANDS(wlc) > 1) {
		if (saved_db_cap_40) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap |= WLC_BW_40MHZ_BIT;
		} else {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap &= ~WLC_BW_40MHZ_BIT;
		}
	}
#endif
}

#ifdef WL11N
uint8
wlc_rclass_extch_get(wlc_cm_info_t *wlc_cm, uint8 rclass)
{
	const rcinfo_t *rcinfo;
	uint8 i, extch = DOT11_EXT_CH_NONE;

	if (!isset(wlc_cm->valid_rcvec.vec, rclass)) {
		WL_ERROR(("wl%d: %s %d regulatory class not supported\n",
			wlc_cm->wlc->pub->unit, wlc_cm->country_abbrev, rclass));
		return extch;
	}

	/* rcinfo consist of control channel at lower sb */
	rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40L];
	for (i = 0; rcinfo && i < rcinfo->len; i++) {
		if (rclass == rcinfo->rctbl[i].rclass) {
			/* ext channel is opposite of control channel */
			extch = DOT11_EXT_CH_UPPER;
			goto exit;
		}
	}

	/* rcinfo consist of control channel at upper sb */
	rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40U];
	for (i = 0; rcinfo && i < rcinfo->len; i++) {
		if (rclass == rcinfo->rctbl[i].rclass) {
			/* ext channel is opposite of control channel */
			extch = DOT11_EXT_CH_LOWER;
			break;
		}
	}
exit:
	WL_INFORM(("wl%d: %s regulatory class %d has ctl chan %s\n",
		wlc_cm->wlc->pub->unit, wlc_cm->country_abbrev, rclass,
		((!extch) ? "NONE" : (((extch == DOT11_EXT_CH_LOWER) ? "LOWER" : "UPPER")))));

	return extch;
}

/* get the ordered list of supported reg class, with current reg class
 * as first element
 */
uint8
wlc_get_regclass_list(wlc_cm_info_t *wlc_cm, uint8 *rclist, uint lsize,
	chanspec_t chspec, bool ie_order)
{
	uint8 i, cur_rc = 0, idx = 0;

	ASSERT(rclist != NULL);
	ASSERT(lsize > 1);

	if (ie_order) {
		cur_rc = wlc_get_regclass(wlc_cm, chspec);
		if (!cur_rc) {
			WL_ERROR(("wl%d: current regulatory class is not found\n",
				wlc_cm->wlc->pub->unit));
			return 0;
		}
		rclist[idx++] = cur_rc;	/* first element is current reg class */
	}

	for (i = 0; i < MAXREGCLASS && idx < lsize; i++) {
		if (i != cur_rc && isset(wlc_cm->valid_rcvec.vec, i))
			rclist[idx++] = i;
	}

	if (i < MAXREGCLASS && idx == lsize) {
		WL_ERROR(("wl%d: regulatory class list full %d\n", wlc_cm->wlc->pub->unit, idx));
		ASSERT(0);
	}

	return idx;
}
#endif /* WL11N */

static uint8
wlc_get_2g_regclass(wlc_cm_info_t *wlc_cm, uint8 chan)
{
	if (wlc_us_ccode(wlc_cm->country_abbrev))
		return WLC_REGCLASS_USA_2G_20MHZ;
	else if (wlc_japan_ccode(wlc_cm->country_abbrev)) {
		if (chan < 14)
			return WLC_REGCLASS_JPN_2G_20MHZ;
		else
			return WLC_REGCLASS_JPN_2G_20MHZ_CH14;
	} else
		return WLC_REGCLASS_EUR_2G_20MHZ;
}

uint8
wlc_get_regclass(wlc_cm_info_t *wlc_cm, chanspec_t chanspec)
{
	const rcinfo_t *rcinfo = NULL;
	uint8 i;
	uint8 chan;

#ifdef WL11N
	if (CHSPEC_IS40(chanspec)) {
		chan = wf_chspec_ctlchan(chanspec);
		if (CHSPEC_SB_UPPER(chanspec))
			rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40U];
		else
			rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_40L];
	} else
#endif /* WL11N */
	{
		chan = CHSPEC_CHANNEL(chanspec);
		if (CHSPEC_IS2G(chanspec))
			return (wlc_get_2g_regclass(wlc_cm, chan));
		rcinfo = wlc_cm->rcinfo_list[WLC_RCLIST_20];
	}

	for (i = 0; rcinfo != NULL && i < rcinfo->len; i++) {
		if (chan == rcinfo->rctbl[i].chan)
			return (rcinfo->rctbl[i].rclass);
	}

	WL_INFORM(("wl%d: No regulatory class assigned for %s channel %d\n",
		wlc_cm->wlc->pub->unit, wlc_cm->country_abbrev, chan));

	return 0;
}

#if defined(BCMDBG)
int
wlc_dump_rclist(const char *name, uint8 *rclist, uint8 rclen, struct bcmstrbuf *b)
{
	uint i;

	if (!rclen)
		return 0;

	bcm_bprintf(b, "%s [ ", name ? name : "");
	for (i = 0; i < rclen; i++) {
		bcm_bprintf(b, "%d ", rclist[i]);
	}
	bcm_bprintf(b, "]");
	bcm_bprintf(b, "\n");

	return 0;
}

/* format a qdB value as integer and decimal fraction in a bcmstrbuf */
static void
wlc_channel_dump_qdb(struct bcmstrbuf *b, int qdb)
{
	if ((qdb >= 0) || (qdb % WLC_TXPWR_DB_FACTOR == 0))
		bcm_bprintf(b, "%2d%s", QDB_FRAC(qdb));
	else
		bcm_bprintf(b, "%2d%s",
			qdb / WLC_TXPWR_DB_FACTOR + 1,
			fraction[WLC_TXPWR_DB_FACTOR - (qdb % WLC_TXPWR_DB_FACTOR)]);
}

/* helper function for wlc_channel_dump_txppr() to print one set of power targets with label */
static void
wlc_channel_dump_pwr_range(struct bcmstrbuf *b, const char *label, int8 *ptr, uint count)
{
	uint i;

	bcm_bprintf(b, "%s ", label);
	for (i = 0; i < count; i++) {
		if (ptr[i] != WL_RATE_DISABLED) {
			wlc_channel_dump_qdb(b, ptr[i]);
			bcm_bprintf(b, " ");
		} else
			bcm_bprintf(b, "-     ");
	}
	bcm_bprintf(b, "\n");
}

/* helper function to print a target range line with the typical 8 targets */
static void
wlc_channel_dump_pwr_range8(struct bcmstrbuf *b, const char *label, void *ptr)
{
	wlc_channel_dump_pwr_range(b, label, (int8*)ptr, 8);
}

#ifndef PPR_API
/* helper function to print a target range line with the typical 4 targets */
static void
wlc_channel_dump_pwr_range4(struct bcmstrbuf *b, const char *label, uint8 *ptr)
{
	wlc_channel_dump_pwr_range(b, label, (int8*)ptr, 4);
}

/* helper function to print a target range line with the typical 2 targets */
static void
wlc_channel_dump_pwr_range2(struct bcmstrbuf *b, const char *label, uint8 *ptr)
{
#ifdef WL11AC
	wlc_channel_dump_pwr_range(b, label, (int8*)ptr, 2);
#endif
}
#endif  /* !PPR_API */

#ifdef PPR_API

#ifdef WL11AC

#define NUM_MCS_RATES WL_NUM_RATES_VHT
#define CHSPEC_TO_TX_BW(c)	(CHSPEC_IS80(c) ? WL_TX_BW_80 : \
	(CHSPEC_IS40(c) ? WL_TX_BW_40 : WL_TX_BW_20))

#else

#define NUM_MCS_RATES WL_NUM_RATES_MCS_1STREAM
#define CHSPEC_TO_TX_BW(c)	(CHSPEC_IS40(c) ? WL_TX_BW_40 : WL_TX_BW_20)

#endif

/* helper function to print a target range line with the typical 8 targets */
static void
wlc_channel_dump_pwr_range_mcs(struct bcmstrbuf *b, const char *label, int8 *ptr)
{
	wlc_channel_dump_pwr_range(b, label, (int8*)ptr, NUM_MCS_RATES);
}


/* format the contents of a ppr_t structure for a bcmstrbuf */
static void
wlc_channel_dump_txppr(struct bcmstrbuf *b, ppr_t *txpwr, wl_tx_bw_t bw)
{
	ppr_dsss_rateset_t dsss_limits;
	ppr_ofdm_rateset_t ofdm_limits;
	ppr_vht_mcs_rateset_t mcs_limits;

	if (bw == WL_TX_BW_20) {
		bcm_bprintf(b, "\n20MHz:\n");
		ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS              ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_2, &dsss_limits);
		wlc_channel_dump_pwr_range(b, "DSSS_MULTI1       ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

		ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_3, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS_MULTI2       ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_3, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ", mcs_limits.pwr);
	} else if (bw == WL_TX_BW_40) {

		bcm_bprintf(b, "\n40MHz:\n");
		ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

		ppr_get_ofdm(txpwr, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_3, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ", mcs_limits.pwr);

		bcm_bprintf(b, "\n20in40MHz:\n");
		ppr_get_dsss(txpwr, WL_TX_BW_20IN40, WL_TX_CHAINS_1, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS              ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		ppr_get_dsss(txpwr, WL_TX_BW_20IN40, WL_TX_CHAINS_2, &dsss_limits);
		wlc_channel_dump_pwr_range(b, "DSSS_MULTI1       ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

		ppr_get_dsss(txpwr, WL_TX_BW_20IN40, WL_TX_CHAINS_3, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS_MULTI2       ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN40, WL_TX_MODE_CDD, WL_TX_CHAINS_3, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ", mcs_limits.pwr);

#ifdef WL11AC
	} else if (bw == WL_TX_BW_80) {
		bcm_bprintf(b, "\n80MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

		ppr_get_ofdm(txpwr, WL_TX_BW_80, WL_TX_MODE_CDD, WL_TX_CHAINS_3, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_80, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ", mcs_limits.pwr);

		bcm_bprintf(b, "\n20in80MHz:\n");
		ppr_get_dsss(txpwr, WL_TX_BW_20IN80, WL_TX_CHAINS_1, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS              ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		ppr_get_dsss(txpwr, WL_TX_BW_20IN80, WL_TX_CHAINS_2, &dsss_limits);
		wlc_channel_dump_pwr_range(b, "DSSS_MULTI1       ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

		ppr_get_dsss(txpwr, WL_TX_BW_20IN80, WL_TX_CHAINS_3, &dsss_limits);
		wlc_channel_dump_pwr_range(b,  "DSSS_MULTI2       ", dsss_limits.pwr,
			WL_RATESET_SZ_DSSS);
		ppr_get_ofdm(txpwr, WL_TX_BW_20IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_3, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_20IN80, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ", mcs_limits.pwr);

		bcm_bprintf(b, "\n40in80MHz:\n");

		ppr_get_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM              ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7            ", mcs_limits.pwr);

		ppr_get_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_2, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD1         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD1       ", mcs_limits.pwr);

		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15           ", mcs_limits.pwr);

		ppr_get_ofdm(txpwr, WL_TX_BW_40IN80, WL_TX_MODE_CDD, WL_TX_CHAINS_3, &ofdm_limits);
		wlc_channel_dump_pwr_range8(b, "OFDM_CDD2         ", ofdm_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_CDD2       ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS0_7_STBC_SPEXP1", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS8_15_SPEXP1    ", mcs_limits.pwr);
		ppr_get_vht_mcs(txpwr, WL_TX_BW_40IN80, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3, &mcs_limits);
		wlc_channel_dump_pwr_range_mcs(b, "MCS16_23          ", mcs_limits.pwr);
#endif /* WL11AC */
	}

	bcm_bprintf(b, "\n");
}

#else /* PPR_API */

/* format the contents of a txppr_t struction for a bcmstrbuf */
static void
wlc_channel_dump_txppr(struct bcmstrbuf *b, txppr_t *txppr, int is_ht_format)
{
	bcm_bprintf(b, "20MHz:\n");
	wlc_channel_dump_pwr_range4(b, "DSSS                 ", txppr->b20_1x1dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM                 ", txppr->b20_1x1ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7               ", txppr->b20_1x1mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1            ", txppr->b20_1x1vht);

	wlc_channel_dump_pwr_range4(b, "DSSS_MULTI1          ", txppr->b20_1x2dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM_CDD1            ", txppr->b20_1x2cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD1          ", txppr->b20_1x2cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD1       ", txppr->b20_1x2cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC          ", txppr->b20_2x2stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC       ", txppr->b20_2x2stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15              ", txppr->b20_2x2sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2            ", txppr->b20_2x2sdm_vht);

	wlc_channel_dump_pwr_range4(b, "DSSS_MULTI2          ", txppr->b20_1x3dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM_CDD2            ", txppr->b20_1x3cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD2          ", txppr->b20_1x3cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD2       ", txppr->b20_1x3cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC_SPEXP1   ", txppr->b20_2x3stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC_SPEXP1", txppr->b20_2x3stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15_SPEXP1       ", txppr->b20_2x3sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2_SPEXP1     ", txppr->b20_2x3sdm_vht);
	wlc_channel_dump_pwr_range8(b, "MCS16_23             ", txppr->b20_3x3sdm_mcs16);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS3            ", txppr->b20_3x3sdm_vht);

	bcm_bprintf(b, "\n40MHz:\n");
	wlc_channel_dump_pwr_range8(b, "OFDM                 ", txppr->b40_1x1ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7               ", txppr->b40_1x1mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1            ", txppr->b40_1x1vht);

	wlc_channel_dump_pwr_range8(b, "OFDM_CDD1            ", txppr->b40_1x2cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD1          ", txppr->b40_1x2cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD1       ", txppr->b40_1x2cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC          ", txppr->b40_2x2stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC       ", txppr->b40_2x2stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15              ", txppr->b40_2x2sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2            ", txppr->b40_2x2sdm_vht);

	wlc_channel_dump_pwr_range8(b, "OFDM_CDD2            ", txppr->b40_1x3cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD2          ", txppr->b40_1x3cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD2       ", txppr->b40_1x3cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC_SPEXP1   ", txppr->b40_2x3stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC_SPEXP1", txppr->b40_2x3stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15_SPEXP1       ", txppr->b40_2x3sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2_SPEXP1     ", txppr->b40_2x3sdm_vht);
	wlc_channel_dump_pwr_range8(b, "MCS16_23             ", txppr->b40_3x3sdm_mcs16);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS3            ", txppr->b40_3x3sdm_vht);

	bcm_bprintf(b, "\n20 in 40MHz:\n");
	wlc_channel_dump_pwr_range4(b, "DSSS                 ", txppr->b20in40_1x1dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM                 ", txppr->b20in40_1x1ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7               ", txppr->b20in40_1x1mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1            ", txppr->b20in40_1x1vht);

	wlc_channel_dump_pwr_range4(b, "DSSS_MULTI1          ", txppr->b20in40_1x2dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM_CDD1            ", txppr->b20in40_1x2cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD1          ", txppr->b20in40_1x2cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD1       ", txppr->b20in40_1x2cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC          ", txppr->b20in40_2x2stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC       ", txppr->b20in40_2x2stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15              ", txppr->b20in40_2x2sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2            ", txppr->b20in40_2x2sdm_vht);

	wlc_channel_dump_pwr_range4(b, "DSSS_MULTI2          ", txppr->b20in40_1x3dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM_CDD2            ", txppr->b20in40_1x3cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD2          ", txppr->b20in40_1x3cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD2       ", txppr->b20in40_1x3cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC_SPEXP1   ", txppr->b20in40_2x3stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC_SPEXP1", txppr->b20in40_2x3stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15_SPEXP1       ", txppr->b20in40_2x3sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2_SPEXP1     ", txppr->b20in40_2x3sdm_vht);
	wlc_channel_dump_pwr_range8(b, "MCS16_23             ", txppr->b20in40_3x3sdm_mcs16);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS3            ", txppr->b20in40_3x3sdm_vht);

#ifdef WL11AC
	bcm_bprintf(b, "\n80MHz:\n");
	wlc_channel_dump_pwr_range8(b, "OFDM                 ", txppr->b80_1x1ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7               ", txppr->b80_1x1mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1            ", txppr->b80_1x1vht);

	wlc_channel_dump_pwr_range8(b, "OFDM_CDD1            ", txppr->b80_1x2cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD1          ", txppr->b80_1x2cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD1       ", txppr->b80_1x2cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC          ", txppr->b80_2x2stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC       ", txppr->b80_2x2stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15              ", txppr->b80_2x2sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2            ", txppr->b80_2x2sdm_vht);

	wlc_channel_dump_pwr_range8(b, "OFDM_CDD2            ", txppr->b80_1x3cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD2          ", txppr->b80_1x3cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD2       ", txppr->b80_1x3cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC_SPEXP1   ", txppr->b80_2x3stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC_SPEXP1", txppr->b80_2x3stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15_SPEXP1       ", txppr->b80_2x3sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2_SPEXP1     ", txppr->b80_2x3sdm_vht);
	wlc_channel_dump_pwr_range8(b, "MCS16_23             ", txppr->b80_3x3sdm_mcs16);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS3            ", txppr->b80_3x3sdm_vht);

	bcm_bprintf(b, "\n20 in 80MHz:\n");
	wlc_channel_dump_pwr_range4(b, "DSSS                 ", txppr->b20in80_1x1dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM                 ", txppr->b20in80_1x1ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7               ", txppr->b20in80_1x1mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1            ", txppr->b20in80_1x1vht);

	wlc_channel_dump_pwr_range4(b, "DSSS_MULTI1          ", txppr->b20in80_1x2dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM_CDD1            ", txppr->b20in80_1x2cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD1          ", txppr->b20in80_1x2cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD1       ", txppr->b20in80_1x2cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC          ", txppr->b20in80_2x2stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC       ", txppr->b20in80_2x2stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15              ", txppr->b20in80_2x2sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2            ", txppr->b20in80_2x2sdm_vht);

	wlc_channel_dump_pwr_range4(b, "DSSS_MULTI2          ", txppr->b20in80_1x3dsss);
	wlc_channel_dump_pwr_range8(b, "OFDM_CDD2            ", txppr->b20in80_1x3cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD2          ", txppr->b20in80_1x3cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD2       ", txppr->b20in80_1x3cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC_SPEXP1   ", txppr->b20in80_2x3stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC_SPEXP1", txppr->b20in80_2x3stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15_SPEXP1       ", txppr->b20in80_2x3sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2_SPEXP1     ", txppr->b20in80_2x3sdm_vht);
	wlc_channel_dump_pwr_range8(b, "MCS16_23             ", txppr->b20in80_3x3sdm_mcs16);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS3            ", txppr->b20in80_3x3sdm_vht);

	bcm_bprintf(b, "\n40 in 80MHz:\n");
	wlc_channel_dump_pwr_range8(b, "OFDM                 ", txppr->b40in80_1x1ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7               ", txppr->b40in80_1x1mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1            ", txppr->b40in80_1x1vht);

	wlc_channel_dump_pwr_range8(b, "OFDM_CDD1            ", txppr->b40in80_1x2cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD1          ", txppr->b40in80_1x2cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD1       ", txppr->b40in80_1x2cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC          ", txppr->b40in80_2x2stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC       ", txppr->b40in80_2x2stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15              ", txppr->b40in80_2x2sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2            ", txppr->b40in80_2x2sdm_vht);

	wlc_channel_dump_pwr_range8(b, "OFDM_CDD2            ", txppr->b40in80_1x3cdd_ofdm);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_CDD2          ", txppr->b40in80_1x3cdd_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_CDD2       ", txppr->b40in80_1x3cdd_vht);
	wlc_channel_dump_pwr_range8(b, "MCS0_7_STBC_SPEXP1   ", txppr->b40in80_2x3stbc_mcs0);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS1_STBC_SPEXP1", txppr->b40in80_2x3stbc_vht);
	wlc_channel_dump_pwr_range8(b, "MCS8_15_SPEXP1       ", txppr->b40in80_2x3sdm_mcs8);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS2_SPEXP1     ", txppr->b40in80_2x3sdm_vht);
	wlc_channel_dump_pwr_range8(b, "MCS16_23             ", txppr->b40in80_3x3sdm_mcs16);
	wlc_channel_dump_pwr_range2(b, "VHT8_9SS3            ", txppr->b40in80_3x3sdm_vht);
#endif /* WL11AC */

	bcm_bprintf(b, "\n");
}
#endif /* PPR_API */
#endif 

/*
 * 	if (wlc->country_list_extended) all country listable.
 *	else J1 - J10 is excluded.
 */
static bool
wlc_country_listable(struct wlc_info *wlc, const char *countrystr)
{
	bool listable = TRUE;

	if (wlc->country_list_extended == FALSE) {
		if (countrystr[0] == 'J' &&
			(countrystr[1] >= '1' && countrystr[1] <= '9'))
			listable = FALSE;
	}

	return listable;
}

static bool
wlc_buffalo_map_locale(struct wlc_info *wlc, const char* abbrev)
{
	if ((wlc->pub->sih->boardvendor == VENDOR_BUFFALO) &&
	    D11REV_GT(wlc->pub->corerev, 5) && !strcmp("JP", abbrev))
		return TRUE;
	else
		return FALSE;
}

clm_country_t
wlc_get_country(struct wlc_info *wlc)
{
	return wlc->cmi->country;
}

int
wlc_get_channels_in_country(struct wlc_info *wlc, void *arg)
{
	chanvec_t channels;
	wl_channels_in_country_t *cic = (wl_channels_in_country_t *)arg;
	chanvec_t sup_chan;
	uint count, need, i;

	if ((cic->band != WLC_BAND_5G) && (cic->band != WLC_BAND_2G)) {
		WL_ERROR(("Invalid band %d\n", cic->band));
		return BCME_BADBAND;
	}

	if ((NBANDS(wlc) == 1) && (cic->band != (uint)wlc->band->bandtype)) {
		WL_ERROR(("Invalid band %d for card\n", cic->band));
		return BCME_BADBAND;
	}

	if (wlc_channel_get_chanvec(wlc, cic->country_abbrev, cic->band, &channels) == FALSE) {
		WL_ERROR(("Invalid country %s\n", cic->country_abbrev));
		return BCME_NOTFOUND;
	}

	wlc_phy_chanspec_band_validch(wlc->band->pi, cic->band, &sup_chan);
	for (i = 0; i < sizeof(chanvec_t); i++)
		sup_chan.vec[i] &= channels.vec[i];

	/* find all valid channels */
	for (count = 0, i = 0; i < sizeof(sup_chan.vec)*NBBY; i++) {
		if (isset(sup_chan.vec, i))
			count++;
	}

	need = sizeof(wl_channels_in_country_t) + count*sizeof(cic->channel[0]);

	if (need > cic->buflen) {
		/* too short, need this much */
		WL_ERROR(("WLC_GET_COUNTRY_LIST: Buffer size: Need %d Received %d\n",
			need, cic->buflen));
		cic->buflen = need;
		return BCME_BUFTOOSHORT;
	}

	for (count = 0, i = 0; i < sizeof(sup_chan.vec)*NBBY; i++) {
		if (isset(sup_chan.vec, i))
			cic->channel[count++] = i;
	}

	cic->count = count;
	return 0;
}

int
wlc_get_country_list(struct wlc_info *wlc, void *arg)
{
	chanvec_t channels;
	chanvec_t unused;
	wl_country_list_t *cl = (wl_country_list_t *)arg;
	clm_country_locales_t locale;
	chanvec_t sup_chan;
	uint count, need, j;
	clm_country_t country_iter;

	ccode_t cc;
	unsigned int regrev;

	if (cl->band_set == FALSE) {
		/* get for current band */
		cl->band = wlc->band->bandtype;
	}

	if ((cl->band != WLC_BAND_5G) && (cl->band != WLC_BAND_2G)) {
		WL_ERROR(("Invalid band %d\n", cl->band));
		return BCME_BADBAND;
	}

	if ((NBANDS(wlc) == 1) && (cl->band != (uint)wlc->band->bandtype)) {
		WL_INFORM(("Invalid band %d for card\n", cl->band));
		cl->count = 0;
		return 0;
	}

	wlc_phy_chanspec_band_validch(wlc->band->pi, cl->band, &sup_chan);

	count = 0;
	if (clm_iter_init(&country_iter) == CLM_RESULT_OK) {
		while (clm_country_iter(&country_iter, cc, &regrev) == CLM_RESULT_OK) {
			if (wlc_get_locale(country_iter, &locale) == CLM_RESULT_OK) {
				if (cl->band == WLC_BAND_5G) {
					wlc_locale_get_channels(&locale, CLM_BAND_5G, &channels,
						&unused);
				} else {
					wlc_locale_get_channels(&locale, CLM_BAND_2G, &channels,
						&unused);
				}
				for (j = 0; j < sizeof(sup_chan.vec); j++) {
					if (sup_chan.vec[j] & channels.vec[j]) {
						count++;
						break;
					}
				}
			}
		}
	}

	need = sizeof(wl_country_list_t) + count*WLC_CNTRY_BUF_SZ;

	if (need > cl->buflen) {
		/* too short, need this much */
		WL_ERROR(("WLC_GET_COUNTRY_LIST: Buffer size: Need %d Received %d\n",
			need, cl->buflen));
		cl->buflen = need;
		return BCME_BUFTOOSHORT;
	}

	count = 0;
	if (clm_iter_init(&country_iter) == CLM_RESULT_OK) {
		ccode_t prev_cc = "";
		while (clm_country_iter(&country_iter, cc, &regrev) == CLM_RESULT_OK) {
			if (wlc_get_locale(country_iter, &locale) == CLM_RESULT_OK) {
				if (cl->band == WLC_BAND_5G) {
					wlc_locale_get_channels(&locale, CLM_BAND_5G, &channels,
						&unused);
				} else {
					wlc_locale_get_channels(&locale, CLM_BAND_2G, &channels,
						&unused);
				}
				for (j = 0; j < sizeof(sup_chan.vec); j++) {
					if (sup_chan.vec[j] & channels.vec[j]) {
						if ((wlc_country_listable(wlc, cc)) &&
							memcmp(cc, prev_cc,
							sizeof(ccode_t))) {
							strncpy(&cl->country_abbrev
								[count*WLC_CNTRY_BUF_SZ],
								cc, sizeof(ccode_t));
							/* terminate the string */
							cl->country_abbrev[count*WLC_CNTRY_BUF_SZ +
								sizeof(ccode_t)] = 0;
							bcopy(cc, prev_cc, sizeof(ccode_t));
							count++;
						}
						break;
					}
				}
			}
		}
	}

	cl->count = count;
	return 0;
}

/* Get regulatory max power for a given channel in a given locale.
 * for external FALSE, it returns limit for brcm hw
 * ---- for 2.4GHz channel, it returns cck limit, not ofdm limit.
 * for external TRUE, it returns 802.11d Country Information Element -
 * 	Maximum Transmit Power Level.
 */
int8
wlc_get_reg_max_power_for_channel(wlc_cm_info_t *wlc_cm, int chan, bool external)
{
	int8 maxpwr = WL_RATE_DISABLED;

	clm_country_locales_t locales;
	clm_country_t country;
	wlc_info_t *wlc = wlc_cm->wlc;

	clm_result_t result = wlc_country_lookup_direct(wlc_cm->ccode, wlc_cm->regrev, &country);

	if (result != CLM_RESULT_OK) {
		return WL_RATE_DISABLED;
	}

	result = wlc_get_locale(country, &locales);
	if (result != CLM_RESULT_OK) {
		return WL_RATE_DISABLED;
	}

	if (external) {
		int int_limit;
		if (clm_regulatory_limit(&locales, BAND_5G(wlc->band->bandtype) ?
			CLM_BAND_5G : CLM_BAND_2G, chan, &int_limit) == CLM_RESULT_OK) {
			maxpwr = (uint8)int_limit;
		}
	} else {
		clm_power_limits_t limits;
		clm_limits_params_t lim_params;

		if (clm_limits_params_init(&lim_params) == CLM_RESULT_OK) {

			if (clm_limits(&locales,
				chan <= CH_MAX_2G_CHANNEL ? CLM_BAND_2G : CLM_BAND_5G,
				chan, 0, CLM_LIMITS_TYPE_CHANNEL, &lim_params,
				&limits) == CLM_RESULT_OK) {
				int i;

				for (i = 0; i < WL_NUMRATES; i++) {
					if (maxpwr < limits.limit[i])
						maxpwr = limits.limit[i];
				}
			}
		}
	}

	return (maxpwr);
}

#if defined(BCMDBG)
static int wlc_dump_max_power_per_channel(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = wlc_cm->wlc;

	int8 ext_pwr = wlc_get_reg_max_power_for_channel(wlc_cm,
		CHSPEC_CHANNEL(wlc->chanspec), TRUE);
	/* int8 int_pwr = wlc_get_reg_max_power_for_channel(wlc_cm,
	   CHSPEC_CHANNEL(wlc->chanspec), FALSE);
	*/

	/* bcm_bprintf(b, "Reg Max Power: %d External: %d\n", int_pwr, ext_pwr); */
	bcm_bprintf(b, "Reg Max Power (External) %d\n", ext_pwr);
	return 0;
}

static int wlc_get_clm_limits(wlc_cm_info_t *wlc_cm, clm_band_t bandtype,
	clm_bandwidth_t bw, clm_power_limits_t *limits,
	clm_power_limits_t *limits20in40)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	chanspec_t chanspec = wlc->chanspec;
	unsigned int chan;
	clm_country_t country;
	clm_result_t result = CLM_RESULT_ERR;
	clm_country_locales_t locale;
	wlcband_t* band;
	int ant_gain;
	clm_limits_params_t lim_params;

	memset(limits, (unsigned char)WL_RATE_DISABLED, sizeof(clm_power_limits_t));
	memset(limits20in40, (unsigned char)WL_RATE_DISABLED, sizeof(clm_power_limits_t));

	country = wlc_cm->country;
	chan = CHSPEC_CHANNEL(chanspec);
	band = wlc->bandstate[CHSPEC_WLCBANDUNIT(chanspec)];
	ant_gain = band->antgain;

	result = wlc_get_locale(country, &locale);
	if (result != CLM_RESULT_OK)
		WL_ERROR(("wlc_get_locale failed\n"));

	if ((result == CLM_RESULT_OK) && (clm_limits_params_init(&lim_params) == CLM_RESULT_OK)) {
		lim_params.sar = band->sar;
		lim_params.bw = bw;

		result = clm_limits(&locale, bandtype, chan, ant_gain, CLM_LIMITS_TYPE_CHANNEL,
			&lim_params, limits);
	}

	if ((result == CLM_RESULT_OK) && (bw == CLM_BW_40) && (limits20in40 != NULL))
		result = clm_limits(&locale, bandtype, chan, ant_gain,
			CHSPEC_SB_UPPER(chanspec) ?
			CLM_LIMITS_TYPE_SUBCHAN_U : CLM_LIMITS_TYPE_SUBCHAN_L,
			&lim_params, limits20in40);
	if (result != CLM_RESULT_OK)
		WL_ERROR(("clm_limits failed\n"));

	return 0;
}

static int wlc_dump_clm_limits_2G_20M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	clm_power_limits_t limits;
	clm_power_limits_t limits20in40;
	int i;

	wlc_get_clm_limits(wlc_cm, CLM_BAND_2G, CLM_BW_20, &limits, &limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits.limit[i]);
	}

	return 0;
}
static int wlc_dump_clm_limits_2G_40M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	clm_power_limits_t limits;
	clm_power_limits_t limits20in40;
	int i;

	wlc_get_clm_limits(wlc_cm, CLM_BAND_2G, CLM_BW_40, &limits, &limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits.limit[i]);
	}

	return 0;
}
static int wlc_dump_clm_limits_2G_20in40M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	clm_power_limits_t limits;
	clm_power_limits_t limits20in40;
	int i;

	wlc_get_clm_limits(wlc_cm, CLM_BAND_2G, CLM_BW_40, &limits, &limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits20in40.limit[i]);
	}

	return 0;
}
static int wlc_dump_clm_limits_5G_20M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	clm_power_limits_t limits;
	clm_power_limits_t limits20in40;
	int i;

	wlc_get_clm_limits(wlc_cm, CLM_BAND_5G, CLM_BW_20, &limits, &limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits.limit[i]);
	}

	return 0;
}
static int wlc_dump_clm_limits_5G_40M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	clm_power_limits_t limits;
	clm_power_limits_t limits20in40;
	int i;

	wlc_get_clm_limits(wlc_cm, CLM_BAND_5G, CLM_BW_40, &limits, &limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits.limit[i]);
	}

	return 0;
}
static int wlc_dump_clm_limits_5G_20in40M(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	clm_power_limits_t limits;
	clm_power_limits_t limits20in40;
	int i;

	wlc_get_clm_limits(wlc_cm, CLM_BAND_5G, CLM_BW_40, &limits, &limits20in40);

	for (i = 0; i < WL_NUMRATES; i++) {
		bcm_bprintf(b, "%d\n", limits20in40.limit[i]);
	}

	return 0;
}
#endif 

static bool
wlc_channel_defined_80MHz_channel(uint channel)
{
	int i;
	/* 80MHz channels in 5GHz band */
	static const uint8 defined_5g_80m_chans[] =
	        {42, 58, 106, 122, 138, 155};

	for (i = 0; i < ARRAYSIZE(defined_5g_80m_chans); i++) {
		if (channel == defined_5g_80m_chans[i]) {
			return TRUE;
		}
	}
	return FALSE;
}

/*
 * Validate the chanspec for this locale, for 40MHz we need to also check that the sidebands
 * are valid 20MHz channels in this locale and they are also a legal HT combination
 */
static bool
wlc_valid_chanspec_ext(wlc_cm_info_t *wlc_cm, chanspec_t chspec, bool dualband)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	uint8 channel = CHSPEC_CHANNEL(chspec);

	/* check the chanspec */
	if (wf_chspec_malformed(chspec)) {
		WL_ERROR(("wl%d: malformed chanspec 0x%x\n", wlc->pub->unit, chspec));
		ASSERT(0);
		return FALSE;
	}

	/* check channel range is in band range */
	if (CHANNEL_BANDUNIT(wlc_cm->wlc, channel) != CHSPEC_WLCBANDUNIT(chspec)) {
		return FALSE;
	}
	/* Check a 20Mhz channel */
	if (CHSPEC_IS20(chspec)) {
		if (dualband)
			return (VALID_CHANNEL20_DB(wlc_cm->wlc, channel));
		else
			return (VALID_CHANNEL20(wlc_cm->wlc, channel));
	}

	/* Check a 40Mhz channel */
	if (CHSPEC_IS40(chspec)) {
		uint8 upper_sideband = 0, idx;
		uint8 num_ch20_entries = sizeof(chan20_info)/sizeof(struct chan20_info);

		if (!wlc->pub->phy_bw40_capable) {
			return FALSE;
		}

		if (!VALID_40CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec)))
			return FALSE;

		if (dualband) {
			if (!VALID_CHANNEL20_DB(wlc, LOWER_20_SB(channel)) ||
			    !VALID_CHANNEL20_DB(wlc, UPPER_20_SB(channel)))
				return FALSE;
		} else {
			if (!VALID_CHANNEL20(wlc, LOWER_20_SB(channel)) ||
			    !VALID_CHANNEL20(wlc, UPPER_20_SB(channel)))
				return FALSE;
		}

		/* find the lower sideband info in the sideband array */
		for (idx = 0; idx < num_ch20_entries; idx++) {
			if (chan20_info[idx].sb == LOWER_20_SB(channel))
				upper_sideband = chan20_info[idx].adj_sbs;
		}
		/* check that the lower sideband allows an upper sideband */
		if ((upper_sideband & (CH_UPPER_SB | CH_EWA_VALID)) == (CH_UPPER_SB | CH_EWA_VALID))
			return TRUE;
		return FALSE;
	}

	/* Check a 80MHz channel - only 5G band supports 80MHz */

	if (CHSPEC_IS80(chspec)) {
		chanspec_t chspec40;

		/* Only 5G supports 80MHz
		 * Check the chanspec band with BAND_5G() instead of the more straightforward
		 * CHSPEC_IS5G() since BAND_5G() is conditionally compiled on BAND5G support. This
		 * check will turn into a constant check when compiling without BAND5G support.
		 */
		if (!BAND_5G(CHSPEC2WLC_BAND(chspec))) {
			return FALSE;
		}

		/* Make sure that the phy is 80MHz capable and that
		 * we are configured for 80MHz on the band
		 */
		if (!wlc->pub->phy_bw80_capable ||
		    !WL_BW_CAP_80MHZ(wlc->bandstate[BAND_5G_INDEX]->bw_cap)) {
			return FALSE;
		}

		/* Ensure that vhtmode is enabled if applicable */
		if (!VHT_ENAB_BAND(wlc->pub, WLC_BAND_5G)) {
			return FALSE;
		}

		if (!VALID_80CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec)))
			return FALSE;

		/* Check that the 80MHz center channel is a defined channel */
		if (!wlc_channel_defined_80MHz_channel(channel)) {
			return FALSE;
		}

		/* Make sure both 40 MHz side channels are valid
		 * Create a chanspec for each 40MHz side side band and check
		 */
		chspec40 = (chanspec_t)((channel - CH_20MHZ_APART) |
		                        WL_CHANSPEC_CTL_SB_L |
		                        WL_CHANSPEC_BW_40 |
		                        WL_CHANSPEC_BAND_5G);
		if (!wlc_valid_chanspec_ext(wlc_cm, chspec40, dualband)) {
			WL_TMP(("wl%d: %s: 80MHz: chanspec %0X -> chspec40 %0X "
			        "failed valid check\n",
			        wlc->pub->unit, __FUNCTION__, chspec, chspec40));

			return FALSE;
		}

		chspec40 = (chanspec_t)((channel + CH_20MHZ_APART) |
		                        WL_CHANSPEC_CTL_SB_L |
		                        WL_CHANSPEC_BW_40 |
		                        WL_CHANSPEC_BAND_5G);
		if (!wlc_valid_chanspec_ext(wlc_cm, chspec40, dualband)) {
			WL_TMP(("wl%d: %s: 80MHz: chanspec %0X -> chspec40 %0X "
			        "failed valid check\n",
			        wlc->pub->unit, __FUNCTION__, chspec, chspec40));

			return FALSE;
		}

		return TRUE;
	}

	return FALSE;
}

bool
wlc_valid_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	return wlc_valid_chanspec_ext(wlc_cm, chspec, FALSE);
}

bool
wlc_valid_chanspec_db(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	return wlc_valid_chanspec_ext(wlc_cm, chspec, TRUE);
}

/*
 *  Fill in 'list' with validated chanspecs, looping through channels using the chanspec_mask.
 */
static void
wlc_chanspec_list(wlc_info_t *wlc, wl_uint32_list_t *list, chanspec_t chanspec_mask)
{
	uint8 channel;
	chanspec_t chanspec;

	for (channel = 0; channel < MAXCHANNEL; channel++) {
		chanspec = (chanspec_mask | channel);
		if (!wf_chspec_malformed(chanspec) &&
		    ((NBANDS(wlc) > 1) ? wlc_valid_chanspec_db(wlc->cmi, chanspec) :
		     wlc_valid_chanspec(wlc->cmi, chanspec))) {
			list->element[list->count] = chanspec;
			list->count++;
		}
	}
}

/*
 * Returns a list of valid chanspecs meeting the provided settings
 */
void
wlc_get_valid_chanspecs(wlc_cm_info_t *wlc_cm, wl_uint32_list_t *list, uint bw, bool band2G,
                        const char *abbrev)
{
	wlc_info_t *wlc = wlc_cm->wlc;
	chanspec_t chanspec;
	clm_country_t country;
	clm_result_t result = CLM_RESULT_ERR;
	clm_result_t flag_result = CLM_RESULT_ERR;
	uint8 flags;
	clm_country_locales_t locale;
	chanvec_t saved_valid_channels, saved_db_valid_channels, unused;
#ifdef WL11N
	uint8 saved_locale_flags = 0,  saved_db_locale_flags = 0;
	bool saved_cap_40 = TRUE, saved_db_cap_40 = TRUE;
#endif /* WL11N */
#ifdef WL11AC
	bool saved_cap_80;
#endif /* WL11AC */

	/* Check if this is a valid band for this card */
	if ((NBANDS(wlc) == 1) &&
	    (BAND_5G(wlc->band->bandtype) == band2G))
		return;

	/* see if we need to look up country. Else, current locale */
	if (strcmp(abbrev, "")) {
		result = wlc_country_lookup(wlc, abbrev, &country);
		if (result != CLM_RESULT_OK) {
			WL_ERROR(("Invalid country \"%s\"\n", abbrev));
			return;
		}
		result = wlc_get_locale(country, &locale);

		flag_result = wlc_get_flags(&locale, band2G ? CLM_BAND_2G : CLM_BAND_5G, &flags);
		BCM_REFERENCE(flag_result);
	}

	/* Save current locales */
	if (result == CLM_RESULT_OK) {
		clm_band_t tmp_band = band2G ? CLM_BAND_2G : CLM_BAND_5G;
		bcopy(&wlc->cmi->bandstate[wlc->band->bandunit].valid_channels,
			&saved_valid_channels, sizeof(chanvec_t));
		wlc_locale_get_channels(&locale, tmp_band,
			&wlc->cmi->bandstate[wlc->band->bandunit].valid_channels, &unused);
		if (NBANDS(wlc) > 1) {
			bcopy(&wlc->cmi->bandstate[OTHERBANDUNIT(wlc)].valid_channels,
			      &saved_db_valid_channels, sizeof(chanvec_t));
			wlc_locale_get_channels(&locale, tmp_band,
			      &wlc->cmi->bandstate[OTHERBANDUNIT(wlc)].valid_channels, &unused);
		}
	}

#ifdef WL11N
	if (result == CLM_RESULT_OK) {
		saved_locale_flags = wlc_cm->bandstate[wlc->band->bandunit].locale_flags;
		wlc_cm->bandstate[wlc->band->bandunit].locale_flags = flags;

		if (NBANDS(wlc) > 1) {
			saved_db_locale_flags = wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags;
			wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags = flags;
		}
	}

	/* save 40 MHz cap */
	saved_cap_40 = WL_BW_CAP_40MHZ(wlc->band->bw_cap);
	wlc->band->bw_cap |= WLC_BW_40MHZ_BIT;
	if (NBANDS(wlc) > 1) {
		saved_db_cap_40 = WL_BW_CAP_40MHZ(wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap);
		wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap |= WLC_BW_40MHZ_BIT;
	}

#ifdef WL11AC
	/* save 80 MHz cap */
	saved_cap_80 = WL_BW_CAP_80MHZ(wlc->bandstate[BAND_5G_INDEX]->bw_cap);
	wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_80MHZ_BIT;
#endif /* WL11AC */

#endif /* WL11N */

	/* Go through 2G 20MHZ chanspecs */
	if (band2G && bw == WL_CHANSPEC_BW_20) {
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_20;
		wlc_chanspec_list(wlc, list, chanspec);
	}

	/* Go through 5G 20 MHZ chanspecs */
	if (!band2G && bw == WL_CHANSPEC_BW_20) {
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_20;
		wlc_chanspec_list(wlc, list, chanspec);
	}

#ifdef WL11N
	/* Go through 2G 40MHZ chanspecs only if N mode and PHY is capable of 40MHZ */
	if (band2G && bw == WL_CHANSPEC_BW_40 &&
	    N_ENAB(wlc->pub) && wlc->pub->phy_bw40_capable) {
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_UPPER;
		wlc_chanspec_list(wlc, list, chanspec);
		chanspec = WL_CHANSPEC_BAND_2G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_LOWER;
		wlc_chanspec_list(wlc, list, chanspec);
	}

	/* Go through 5G 40MHZ chanspecs only if N mode and PHY is capable of 40MHZ  */
	if (!band2G && bw == WL_CHANSPEC_BW_40 &&
	    N_ENAB(wlc->pub) && ((NBANDS(wlc) > 1) || IS_SINGLEBAND_5G(wlc->deviceid)) &&
	    wlc->pub->phy_bw40_capable) {
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_UPPER;
		wlc_chanspec_list(wlc, list, chanspec);
		chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_40 | WL_CHANSPEC_CTL_SB_LOWER;
		wlc_chanspec_list(wlc, list, chanspec);
	}

#ifdef WL11AC
	/* Go through 5G 80MHZ chanspecs only if VHT mode and PHY is capable of 80MHZ  */
	if (!band2G && bw == WL_CHANSPEC_BW_80 && VHT_ENAB_BAND(wlc->pub, WLC_BAND_5G) &&
		((NBANDS(wlc) > 1) || IS_SINGLEBAND_5G(wlc->deviceid)) &&
		wlc->pub->phy_bw80_capable) {

		int i;
		uint16 ctl_sb[] = {
			WL_CHANSPEC_CTL_SB_LL,
			WL_CHANSPEC_CTL_SB_LU,
			WL_CHANSPEC_CTL_SB_UL,
			WL_CHANSPEC_CTL_SB_UU
		};

		for (i = 0; i < 4; i++) {
			chanspec = WL_CHANSPEC_BAND_5G | WL_CHANSPEC_BW_80 | ctl_sb[i];
			wlc_chanspec_list(wlc, list, chanspec);
		}
	}

	/* restore 80 MHz cap */
	if (saved_cap_80)
		wlc->bandstate[BAND_5G_INDEX]->bw_cap |= WLC_BW_80MHZ_BIT;
	else
		wlc->bandstate[BAND_5G_INDEX]->bw_cap &= ~WLC_BW_80MHZ_BIT;
#endif /* WL11AC */

	/* restore 40 MHz cap */
	if (saved_cap_40)
		wlc->band->bw_cap |= WLC_BW_40MHZ_BIT;
	else
		wlc->band->bw_cap &= ~WLC_BW_40MHZ_BIT;

	if (NBANDS(wlc) > 1) {
		if (saved_db_cap_40) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap |= WLC_BW_CAP_40MHZ;
		} else {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->bw_cap &= ~WLC_BW_CAP_40MHZ;
		}
	}

	if (result == CLM_RESULT_OK) {
		wlc_cm->bandstate[wlc->band->bandunit].locale_flags = saved_locale_flags;
		if ((NBANDS(wlc) > 1))
			wlc_cm->bandstate[OTHERBANDUNIT(wlc)].locale_flags = saved_db_locale_flags;
	}
#endif /* WL11N */

	/* Restore the locales if switched */
	if (result == CLM_RESULT_OK) {
		bcopy(&saved_valid_channels,
			&wlc->cmi->bandstate[wlc->band->bandunit].valid_channels,
			sizeof(chanvec_t));
		if ((NBANDS(wlc) > 1))
			bcopy(&saved_db_valid_channels,
			      &wlc->cmi->bandstate[OTHERBANDUNIT(wlc)].valid_channels,
			      sizeof(chanvec_t));
	}
}

/* query the channel list given a country and a regulatory class */
uint8
wlc_rclass_get_channel_list(wlc_cm_info_t *cmi, const char *abbrev, uint8 rclass,
	bool bw20, wl_uint32_list_t *list)
{
	const rcinfo_t *rcinfo = NULL;
	uint8 ch2g_start = 0, ch2g_end = 0;
	int i;

	if (wlc_us_ccode(abbrev)) {
		if (rclass == WLC_REGCLASS_USA_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 11;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_us_20;
#endif
	} else if (wlc_japan_ccode(abbrev)) {
		if (rclass == WLC_REGCLASS_JPN_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 13;
		}
		else if (rclass == WLC_REGCLASS_JPN_2G_20MHZ_CH14) {
			ch2g_start = 14;
			ch2g_end = 14;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_jp_20;
#endif
	} else {
		if (rclass == WLC_REGCLASS_EUR_2G_20MHZ) {
			ch2g_start = 1;
			ch2g_end = 13;
		}
#ifdef BAND5G
		else
			rcinfo = &rcinfo_eu_20;
#endif
	}

	list->count = 0;
	if (rcinfo == NULL) {
		for (i = ch2g_start; i <= ch2g_end; i ++)
			list->element[list->count ++] = i;
	}
	else {
		for (i = 0; i < rcinfo->len; i ++) {
			if (rclass == rcinfo->rctbl[i].rclass)
				list->element[list->count ++] = rcinfo->rctbl[i].chan;
		}
	}

	return (uint8)list->count;
}

/* Return true if the channel is a valid channel that is radar sensitive
 * in the current country/locale
 */
bool
wlc_radar_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
#ifdef BAND5G     /* RADAR */
	uint channel = CHSPEC_CHANNEL(chspec);
	const chanvec_t *radar_channels;

	/* The radar_channels chanvec may be a superset of valid channels,
	 * so be sure to check for a valid channel first.
	 */

	if (!chspec || !wlc_valid_chanspec_db(wlc_cm, chspec)) {
		return FALSE;
	}

	if (CHSPEC_IS5G(chspec)) {
		radar_channels = wlc_cm->bandstate[BAND_5G_INDEX].radar_channels;

		if (CHSPEC_IS80(chspec)) {
			int i;

			/* start at the lower edge 20MHz channel */
			channel = LOWER_40_SB(channel); /* low 40MHz sb of the 80 */
			channel = LOWER_20_SB(channel); /* low 20MHz sb of the 40 */

			/* work through each 20MHz channel in the 80MHz */
			for (i = 0; i < 4; i++, channel += CH_20MHZ_APART) {
				if (isset(radar_channels->vec, channel)) {
					return TRUE;
				}
			}
		} else if (CHSPEC_IS40(chspec)) {
			if (isset(radar_channels->vec, LOWER_20_SB(channel)) ||
			    isset(radar_channels->vec, UPPER_20_SB(channel)))
				return TRUE;
		} else if (isset(radar_channels->vec, channel)) {
			return TRUE;
		}
	}
#endif	/* BAND5G */
	return FALSE;
}

/* Return true if the channel is a valid channel that is radar sensitive
 * in the current country/locale
 */
bool
wlc_restricted_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	uint channel = CHSPEC_CHANNEL(chspec);
	chanvec_t *restricted_channels;

	/* The restriced_channels chanvec may be a superset of valid channels,
	 * so be sure to check for a valid channel first.
	 */

	if (!chspec || !wlc_valid_chanspec_db(wlc_cm, chspec)) {
		return FALSE;
	}

	restricted_channels = &wlc_cm->restricted_channels;

	if (CHSPEC_IS80(chspec)) {
		if (isset(restricted_channels->vec, LL_20_SB(channel)) ||
		    isset(restricted_channels->vec, LU_20_SB(channel)) ||
			isset(restricted_channels->vec, UL_20_SB(channel)) ||
		    isset(restricted_channels->vec, UU_20_SB(channel)))
			return TRUE;
	} else if (CHSPEC_IS40(chspec)) {
		if (isset(restricted_channels->vec, LOWER_20_SB(channel)) ||
		    isset(restricted_channels->vec, UPPER_20_SB(channel)))
			return TRUE;
	} else if (isset(restricted_channels->vec, channel)) {
		return TRUE;
	}

	return FALSE;
}

void
wlc_clr_restricted_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec)
{
	if (CHSPEC_IS40_UNCOND(chspec)) {
		clrbit(wlc_cm->restricted_channels.vec, LOWER_20_SB(CHSPEC_CHANNEL(chspec)));
		clrbit(wlc_cm->restricted_channels.vec, UPPER_20_SB(CHSPEC_CHANNEL(chspec)));
	} else
		clrbit(wlc_cm->restricted_channels.vec, CHSPEC_CHANNEL(chspec));

	wlc_upd_restricted_chanspec_flag(wlc_cm);
}

static void
wlc_upd_restricted_chanspec_flag(wlc_cm_info_t *wlc_cm)
{
	uint j;

	for (j = 0; j < (int)sizeof(chanvec_t); j++)
		if (wlc_cm->restricted_channels.vec[j]) {
			wlc_cm->has_restricted_ch = TRUE;
			return;
		}

	wlc_cm->has_restricted_ch = FALSE;
}

bool
wlc_has_restricted_chanspec(wlc_cm_info_t *wlc_cm)
{
	return wlc_cm->has_restricted_ch;
}

#if defined(BCMDBG)
const bcm_bit_desc_t fc_flags[] = {
	{WLC_EIRP, "EIRP"},
	{WLC_DFS_TPC, "DFS/TPC"},
	{WLC_NO_80MHZ, "No 80MHz"},
	{WLC_NO_40MHZ, "No 40MHz"},
	{WLC_NO_MIMO, "No MIMO"},
	{WLC_RADAR_TYPE_EU, "EU_RADAR"},
	{WLC_FILT_WAR, "FILT_WAR"},
	{WLC_TXBF, "TxBF"},
	{0, NULL}
};

/* FTODO need to add 80mhz to this function */
int
wlc_channel_dump_locale(void *handle, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t*)handle;
#ifdef PPR_API
	ppr_t *txpwr;
#else
	txppr_t txpwr;
#endif
	char max_ofdm_str[32];
	char max_ht20_str[32];
	char max_ht40_str[32];
	char max_cck_str[32];
	int chan, i;
	int restricted;
	int radar = 0;
	int max_cck, max_ofdm;
	int max_ht20 = 0, max_ht40 = 0;
	char flagstr[64];
	uint8 rclist[MAXRCLISTSIZE], rclen;
	chanspec_t chanspec;
	int quiet;

#ifdef PPR_API
	ppr_dsss_rateset_t dsss_limits;
	ppr_ofdm_rateset_t ofdm_limits;
#ifdef WL11N
	ppr_ht_mcs_rateset_t mcs_limits;
#endif
#endif /* PPR_API */

	clm_country_locales_t locales;
	clm_country_t country;

	clm_result_t result = wlc_country_lookup_direct(wlc->cmi->ccode,
		wlc->cmi->regrev, &country);

	if (result != CLM_RESULT_OK) {
		return -1;
	}
#ifdef PPR_API
	if ((txpwr = ppr_create(wlc->pub->osh, WL_TX_BW_80)) == NULL) {
		return BCME_ERROR;
	}
#endif
	bcm_bprintf(b, "srom_ccode \"%s\" srom_regrev %u\n",
	            wlc->cmi->srom_ccode, wlc->cmi->srom_regrev);

	result = wlc_get_locale(country, &locales);
#ifdef PPR_API
	if (result != CLM_RESULT_OK) {
		ppr_delete(wlc->pub->osh, txpwr);
		return -1;
	}
#endif
	if (NBANDS(wlc) > 1) {
		uint8 flags;
		wlc_get_flags(&locales, CLM_BAND_2G, &flags);
		bcm_format_flags(fc_flags, flags, flagstr, 64);
		bcm_bprintf(b, "2G Flags: %s\n", flagstr);
		wlc_get_flags(&locales, CLM_BAND_5G, &flags);
		bcm_format_flags(fc_flags, flags, flagstr, 64);
		bcm_bprintf(b, "5G Flags: %s\n", flagstr);
	} else {
		uint8 flags;
		if (BAND_2G(wlc->band->bandtype))
			wlc_get_flags(&locales, CLM_BAND_2G, &flags);
		else
			wlc_get_flags(&locales, CLM_BAND_5G, &flags);
		bcm_format_flags(fc_flags, flags, flagstr, 64);
		bcm_bprintf(b, "%dG Flags: %s\n", BAND_2G(wlc->band->bandtype)?2:5, flagstr);
	}

	if (N_ENAB(wlc->pub))
		bcm_bprintf(b, "  Ch Rdr/reS DSSS  OFDM   HT    20/40\n");
	else
		bcm_bprintf(b, "  Ch Rdr/reS DSSS  OFDM\n");

	for (chan = 0; chan < MAXCHANNEL; chan++) {
		chanspec = CH20MHZ_CHSPEC(chan);
		if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
			chanspec = CH40MHZ_CHSPEC(chan, WL_CHANSPEC_CTL_SB_LOWER);
			if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
				chanspec = CH80MHZ_CHSPEC(chan, WL_CHANSPEC_CTL_SB_LOWER);
				if (!wlc_valid_chanspec_db(wlc->cmi, chanspec))
					continue;
			}
		}

		radar = wlc_radar_chanspec(wlc->cmi, chanspec);
		restricted = wlc_restricted_chanspec(wlc->cmi, chanspec);
		quiet = wlc_quiet_chanspec(wlc->cmi, chanspec);

#ifdef PPR_API
		wlc_channel_reg_limits(wlc->cmi, chanspec, txpwr);

		ppr_get_dsss(txpwr, WL_TX_BW_20, WL_TX_CHAINS_1, &dsss_limits);
		max_cck = dsss_limits.pwr[0];

		ppr_get_ofdm(txpwr, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1, &ofdm_limits);
		max_ofdm = ofdm_limits.pwr[0];

#ifdef WL11N
		if (WLCISHTPHY(wlc->band) && CHSPEC_IS40(chanspec))
			ppr_get_ht_mcs(txpwr, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
		else
			ppr_get_ht_mcs(txpwr, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
				WL_TX_CHAINS_2, &mcs_limits);
		max_ht20 = mcs_limits.pwr[0];

		ppr_get_ht_mcs(txpwr, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2, &mcs_limits);
		max_ht40 = mcs_limits.pwr[0];
#endif /* WL11N */

#else
		wlc_channel_reg_limits(wlc->cmi, chanspec, &txpwr);

		max_cck = (int8)txpwr.b20_1x1dsss[0];
		max_ofdm = (int8)txpwr.b20_1x1ofdm[0];
#ifdef WL11N

		/* FTODO need to add 80mhz to this block */
		if (PHYTYPE_HT_CAP(wlc->band)) {
			if (CHSPEC_IS40(chanspec))
				max_ht20 = (int8)txpwr.b20in40_1x2cdd_mcs0[0];
			else
				max_ht20 = (int8)txpwr.b20_1x2cdd_mcs0[0];
		}
		else
			max_ht20 = (int8)txpwr.b20_1x2cdd_mcs0[0];

		max_ht40 = (int8)txpwr.b40_1x2cdd_mcs0[0];
#endif /* WL11N */
#endif /* PPR_API */
		if (CHSPEC_IS2G(chanspec))
			if (max_cck != WL_RATE_DISABLED)
			snprintf(max_cck_str, sizeof(max_cck_str),
			         "%2d%s", QDB_FRAC(max_cck));
			else
				strncpy(max_cck_str, "-    ", sizeof(max_cck_str));

		else
			strncpy(max_cck_str, "     ", sizeof(max_cck_str));

		if (max_ofdm != WL_RATE_DISABLED)
			snprintf(max_ofdm_str, sizeof(max_ofdm_str),
		         "%2d%s", QDB_FRAC(max_ofdm));
		else
			strncpy(max_ofdm_str, "-    ", sizeof(max_ofdm_str));

		if (N_ENAB(wlc->pub)) {

			if (max_ht20 != WL_RATE_DISABLED)
				snprintf(max_ht20_str, sizeof(max_ht20_str),
			         "%2d%s", QDB_FRAC(max_ht20));
			else
				strncpy(max_ht20_str, "-    ", sizeof(max_ht20_str));

			if (max_ht40 != WL_RATE_DISABLED)
				snprintf(max_ht40_str, sizeof(max_ht40_str),
			         "%2d%s", QDB_FRAC(max_ht40));
			else
				strncpy(max_ht40_str, "-    ", sizeof(max_ht40_str));

			bcm_bprintf(b, "%s%3d %s%s%s     %s %s  %s %s\n",
			            (CHSPEC_IS40(chanspec)?">":" "), chan,
			            (radar ? "R" : "-"), (restricted ? "S" : "-"),
			            (quiet ? "Q" : "-"),
			            max_cck_str, max_ofdm_str,
			            max_ht20_str, max_ht40_str);
		}
		else
			bcm_bprintf(b, "%s%3d %s%s%s     %s %s\n",
			            (CHSPEC_IS40(chanspec)?">":" "), chan,
			            (radar ? "R" : "-"), (restricted ? "S" : "-"),
			            (quiet ? "Q" : "-"),
			            max_cck_str, max_ofdm_str);
	}

	bzero(rclist, MAXRCLISTSIZE);
	chanspec = wlc->pub->associated ?
	        wlc->home_chanspec : WLC_BAND_PI_RADIO_CHANSPEC;
	rclen = wlc_get_regclass_list(wlc->cmi, rclist, MAXRCLISTSIZE, chanspec, FALSE);
	if (rclen) {
		bcm_bprintf(b, "supported regulatory class:\n");
		for (i = 0; i < rclen; i++)
			bcm_bprintf(b, "%d ", rclist[i]);
		bcm_bprintf(b, "\n");
	}

	bcm_bprintf(b, "has_restricted_ch %s\n", wlc->cmi->has_restricted_ch ? "TRUE" : "FALSE");

#if HTCONF
	{
	struct wlc_channel_txchain_limits *lim;

	lim = &wlc->cmi->bandstate[BAND_2G_INDEX].chain_limits;
	bcm_bprintf(b, "chain limits 2g:");
	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++)
		bcm_bprintf(b, " %2d%s", QDB_FRAC(lim->chain_limit[i]));
	bcm_bprintf(b, "\n");

	lim = &wlc->cmi->bandstate[BAND_5G_INDEX].chain_limits;
	bcm_bprintf(b, "chain limits 5g:");
	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++)
		bcm_bprintf(b, " %2d%s", QDB_FRAC(lim->chain_limit[i]));
	bcm_bprintf(b, "\n");
	}
#endif /* HTCONF */
#ifdef PPR_API
	ppr_delete(wlc->pub->osh, txpwr);
#endif
	return 0;
}
#endif 

#ifndef INT8_MIN
#define INT8_MIN 0x80
#endif
#ifndef INT8_MAX
#define INT8_MAX 0x7F
#endif

#ifndef PPR_API
/* Perform an element by element min of txppr structs a and b, and
 * store the result in a.
 */
static void
wlc_channel_txpwr_vec_combine_min(txppr_t *a, txppr_t *b)
{
	uint i;

	for (i = 0; i < WL_TX_POWER_RATES; i++)
		((uint8*)a)[i] = MIN(((uint8*)a)[i], ((uint8*)b)[i]);
}
#endif
static void
wlc_channel_margin_summary_mapfn(void *context, uint8 *a, uint8 *b)
{
	uint8 margin;
	uint8 *pmin = (uint8*)context;

	if (*a > *b)
		margin = *a - *b;
	else
		margin = 0;

	*pmin = MIN(*pmin, margin);
}

static void
wlc_channel_max_summary_mapfn(void *context, uint8 *a, uint8 *ignore)
{
	uint8 *pmax = (uint8*)context;

	*pmax = MAX(*pmax, *a);
}

#ifndef PPR_API
/* Map the given function with its context value over the two
 * uint8 vectors
 */
static void
wlc_channel_map_uint8_vec_binary(ppr_mapfn_t fn, void* context, uint len,
	uint8 *vec_a, uint8 *vec_b)
{
	uint i;

	for (i = 0; i < len; i++)
		(fn)(context, vec_a + i, vec_b + i);
}

#else

/* Map the given function with its context value over the power targets
 * appropriate for the given band and bandwidth in two txppr structs.
 * If the band is 2G, DSSS/CCK rates will be included.
 * If the bandwidth is 20MHz, only 20MHz targets are included.
 * If the bandwidth is 40MHz, both 40MHz and 20in40 targets are included.
 */
static void
wlc_channel_map_txppr_binary(ppr_mapfn_t fn, void* context, uint bandtype, uint bw,
	ppr_t *a, ppr_t *b)
{

/* macro for the typical 8 rates in a group (OFDM, MCS0-7, 8-15, 16-23) */
#define MAP_GROUP(member) \
	wlc_channel_map_uint8_vec_binary(fn, context, 8, a->member, b->member)
/* macro for the less-typical 4 rates in a group (CCK) */
#define MAP_GROUP_CCK(member) \
	wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_CCK, a->member, b->member)

	if (bw == WL_CHANSPEC_BW_20) {
		if (bandtype == WL_CHANSPEC_BAND_2G)
			ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20, WL_TX_CHAINS_1);
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20, WL_TX_MODE_NONE, WL_TX_CHAINS_1);
	}

#ifdef WL11N
	/* map over 20MHz rates for 20MHz channels */
	if (bw == WL_CHANSPEC_BW_20) {
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20, WL_TX_CHAINS_2);
			ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20, WL_TX_CHAINS_3);
		}

		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_2);
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20, WL_TX_MODE_CDD, WL_TX_CHAINS_3);

		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_1,
			WL_TX_MODE_NONE, WL_TX_CHAINS_1);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3);

		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3);

		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3);
	} else
	/* map over 40MHz and 20in40 rates for 40MHz channels */
	{
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_40, WL_TX_MODE_NONE, WL_TX_CHAINS_1);
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_2);
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_40, WL_TX_MODE_CDD, WL_TX_CHAINS_3);

		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3);

		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2);

		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3);

		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_40, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3);

		/* 20in40 legacy */
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_CHAINS_1);
			ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_CHAINS_2);
			ppr_map_vec_dsss(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_CHAINS_3);
		}
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1);
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2);
		ppr_map_vec_ofdm(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3);

		/* 20in40 HT */
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_NONE,
			WL_TX_CHAINS_1);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_2);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_1, WL_TX_MODE_CDD,
			WL_TX_CHAINS_3);

		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_2);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_2);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_STBC,
			WL_TX_CHAINS_3);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_2, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3);
		ppr_map_vec_ht_mcs(fn, context, a, b, WL_TX_BW_20IN40, WL_TX_NSS_3, WL_TX_MODE_NONE,
			WL_TX_CHAINS_3);
	}
#endif /* WL11N */
}

#endif /* PPR_API */

#ifndef PPR_API

/* Map the given function with its context value over the power targets
 * appropriate for the given band and bandwidth in two txppr structs.
 * If the band is 2G, DSSS/CCK rates will be included.
 * If the bandwidth is 20MHz, only 20MHz targets are included.
 * If the bandwidth is 40MHz, both 40MHz and 20in40 targets are included.
 */
static void
wlc_channel_map_txppr_binary(wlc_channel_mapfn_t fn, void* context, uint bandtype, uint bw,
	txppr_t *a, txppr_t *b)
{

/* macro for the typical 8 rates in a group (OFDM, MCS0-7, 8-15, 16-23) */
#define MAP_GROUP(member) \
	wlc_channel_map_uint8_vec_binary(fn, context, 8, a->member, b->member)
/* macro for the less-typical 4 rates in a group (CCK) */
#define MAP_GROUP_CCK(member) \
	wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_CCK, a->member, b->member)
#define MAP_GROUP_VHT(member) \
	wlc_channel_map_uint8_vec_binary(fn, context, WL_NUM_RATES_EXTRA_VHT, a->member, b->member)

	if (bw == WL_CHANSPEC_BW_20) {
		if (bandtype == WL_CHANSPEC_BAND_2G)
			MAP_GROUP_CCK(b20_1x1dsss);
		MAP_GROUP(b20_1x1ofdm);
	}

#ifdef WL11N
	/* map over 20MHz rates for 20MHz channels */
	if (bw == WL_CHANSPEC_BW_20) {
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			MAP_GROUP_CCK(b20_1x2dsss);
			MAP_GROUP_CCK(b20_1x3dsss);
		}

		MAP_GROUP(b20_1x2cdd_ofdm);

		MAP_GROUP(b20_1x1mcs0);
		MAP_GROUP(b20_1x2cdd_mcs0);
		MAP_GROUP(b20_1x3cdd_ofdm);
		MAP_GROUP(b20_1x3cdd_mcs0);

		MAP_GROUP(b20_2x2stbc_mcs0);
		MAP_GROUP(b20_2x2sdm_mcs8);
		MAP_GROUP(b20_2x3stbc_mcs0);
		MAP_GROUP(b20_2x3sdm_mcs8);

		MAP_GROUP(b20_3x3sdm_mcs16);

	} else if (bw == WL_CHANSPEC_BW_40) {
		/* map over 40MHz and 20in40 rates for 40MHz channels */
		MAP_GROUP(b40_1x1ofdm);
		MAP_GROUP(b40_1x2cdd_ofdm);

		MAP_GROUP(b40_1x1mcs0);
		MAP_GROUP(b40_1x2cdd_mcs0);
		MAP_GROUP(b40_2x2stbc_mcs0);
		MAP_GROUP(b40_1x3cdd_ofdm);
		MAP_GROUP(b40_1x3cdd_mcs0);
		MAP_GROUP(b40_2x3stbc_mcs0);

		MAP_GROUP(b40_2x2sdm_mcs8);
		MAP_GROUP(b40_2x3sdm_mcs8);

		MAP_GROUP(b40_3x3sdm_mcs16);

		/* 20in40 legacy */
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			MAP_GROUP_CCK(b20in40_1x1dsss);
			MAP_GROUP_CCK(b20in40_1x2dsss);
			MAP_GROUP_CCK(b20in40_1x3dsss);
		}
		MAP_GROUP(b20in40_1x1ofdm);
		MAP_GROUP(b20in40_1x2cdd_ofdm);
		MAP_GROUP(b20in40_1x3cdd_ofdm);

		/* 20in40 HT */
		MAP_GROUP(b20in40_1x1mcs0);
		MAP_GROUP(b20in40_1x2cdd_mcs0);
		MAP_GROUP(b20in40_2x2stbc_mcs0);
		MAP_GROUP(b20in40_2x2sdm_mcs8);
		MAP_GROUP(b20in40_3x3sdm_mcs16);
		MAP_GROUP(b20in40_1x3cdd_mcs0);
		MAP_GROUP(b20in40_2x3stbc_mcs0);
		MAP_GROUP(b20in40_2x3sdm_mcs8);
	}
#ifdef WL11AC
	else {
		/* map over 40MHz and 20in40 rates for 40MHz channels */
		MAP_GROUP(b80_1x1ofdm);
		MAP_GROUP(b80_1x2cdd_ofdm);

		MAP_GROUP(b80_1x1mcs0);
		MAP_GROUP(b80_1x2cdd_mcs0);
		MAP_GROUP(b80_2x2stbc_mcs0);
		MAP_GROUP(b80_1x3cdd_ofdm);
		MAP_GROUP(b80_1x3cdd_mcs0);
		MAP_GROUP(b80_2x3stbc_mcs0);

		MAP_GROUP(b80_2x2sdm_mcs8);
		MAP_GROUP(b80_2x3sdm_mcs8);

		MAP_GROUP(b80_3x3sdm_mcs16);

		/* 20in80 legacy */
		if (bandtype == WL_CHANSPEC_BAND_2G) {
			MAP_GROUP_CCK(b20in80_1x1dsss);
			MAP_GROUP_CCK(b20in80_1x2dsss);
			MAP_GROUP_CCK(b20in80_1x3dsss);
		}
		MAP_GROUP(b20in80_1x1ofdm);
		MAP_GROUP(b20in80_1x2cdd_ofdm);
		MAP_GROUP(b20in80_1x3cdd_ofdm);

		/* 20in40 HT */
		MAP_GROUP(b20in80_1x1mcs0);
		MAP_GROUP(b20in80_1x2cdd_mcs0);
		MAP_GROUP(b20in80_2x2stbc_mcs0);
		MAP_GROUP(b20in80_2x2sdm_mcs8);
		MAP_GROUP(b20in80_3x3sdm_mcs16);
		MAP_GROUP(b20in80_1x3cdd_mcs0);
		MAP_GROUP(b20in80_2x3stbc_mcs0);
		MAP_GROUP(b20in80_2x3sdm_mcs8);

		MAP_GROUP(b40in80_1x1ofdm);
		MAP_GROUP(b40in80_1x2cdd_ofdm);
		MAP_GROUP(b40in80_1x3cdd_ofdm);

		/* 40in80 HT */
		MAP_GROUP(b40in80_1x1mcs0);
		MAP_GROUP(b40in80_1x2cdd_mcs0);
		MAP_GROUP(b40in80_2x2stbc_mcs0);
		MAP_GROUP(b40in80_2x2sdm_mcs8);
		MAP_GROUP(b40in80_3x3sdm_mcs16);
		MAP_GROUP(b40in80_1x3cdd_mcs0);
		MAP_GROUP(b40in80_2x3stbc_mcs0);
		MAP_GROUP(b40in80_2x3sdm_mcs8);
	}

	if (bw == WL_CHANSPEC_BW_20) {
		MAP_GROUP_VHT(b20_1x1vht);
		MAP_GROUP_VHT(b20_1x1vht);
		MAP_GROUP_VHT(b20_1x2cdd_vht);
		MAP_GROUP_VHT(b20_2x2stbc_vht);
		MAP_GROUP_VHT(b20_2x2sdm_vht);
		MAP_GROUP_VHT(b20_1x3cdd_vht);
		MAP_GROUP_VHT(b20_2x3stbc_vht);
		MAP_GROUP_VHT(b20_2x3sdm_vht);
		MAP_GROUP_VHT(b20_3x3sdm_vht);
	} else if (bw == WL_CHANSPEC_BW_40) {
		MAP_GROUP_VHT(b40_1x1vht);
		MAP_GROUP_VHT(b40_1x1vht);
		MAP_GROUP_VHT(b40_1x2cdd_vht);
		MAP_GROUP_VHT(b40_2x2stbc_vht);
		MAP_GROUP_VHT(b40_2x2sdm_vht);
		MAP_GROUP_VHT(b40_1x3cdd_vht);
		MAP_GROUP_VHT(b40_2x3stbc_vht);
		MAP_GROUP_VHT(b40_2x3sdm_vht);
		MAP_GROUP_VHT(b40_3x3sdm_vht);
		MAP_GROUP_VHT(b20in40_1x1vht);
		MAP_GROUP_VHT(b20in40_1x1vht);
		MAP_GROUP_VHT(b20in40_1x2cdd_vht);
		MAP_GROUP_VHT(b20in40_2x2stbc_vht);
		MAP_GROUP_VHT(b20in40_2x2sdm_vht);
		MAP_GROUP_VHT(b20in40_1x3cdd_vht);
		MAP_GROUP_VHT(b20in40_2x3stbc_vht);
		MAP_GROUP_VHT(b20in40_2x3sdm_vht);
		MAP_GROUP_VHT(b20in40_3x3sdm_vht);
	} else if (bw == WL_CHANSPEC_BW_80) {
		MAP_GROUP_VHT(b80_1x1vht);
		MAP_GROUP_VHT(b80_1x1vht);
		MAP_GROUP_VHT(b80_1x2cdd_vht);
		MAP_GROUP_VHT(b80_2x2stbc_vht);
		MAP_GROUP_VHT(b80_2x2sdm_vht);
		MAP_GROUP_VHT(b80_1x3cdd_vht);
		MAP_GROUP_VHT(b80_2x3stbc_vht);
		MAP_GROUP_VHT(b80_2x3sdm_vht);
		MAP_GROUP_VHT(b80_3x3sdm_vht);
		MAP_GROUP_VHT(b20in80_1x1vht);
		MAP_GROUP_VHT(b20in80_1x1vht);
		MAP_GROUP_VHT(b20in80_1x2cdd_vht);
		MAP_GROUP_VHT(b20in80_2x2stbc_vht);
		MAP_GROUP_VHT(b20in80_2x2sdm_vht);
		MAP_GROUP_VHT(b20in80_1x3cdd_vht);
		MAP_GROUP_VHT(b20in80_2x3stbc_vht);
		MAP_GROUP_VHT(b20in80_2x3sdm_vht);
		MAP_GROUP_VHT(b20in80_3x3sdm_vht);
		MAP_GROUP_VHT(b40in80_1x1vht);
		MAP_GROUP_VHT(b40in80_1x1vht);
		MAP_GROUP_VHT(b40in80_1x2cdd_vht);
		MAP_GROUP_VHT(b40in80_2x2stbc_vht);
		MAP_GROUP_VHT(b40in80_2x2sdm_vht);
		MAP_GROUP_VHT(b40in80_1x3cdd_vht);
		MAP_GROUP_VHT(b40in80_2x3stbc_vht);
		MAP_GROUP_VHT(b40in80_2x3sdm_vht);
		MAP_GROUP_VHT(b40in80_3x3sdm_vht);
	}
#endif /* WL11AC */
#endif /* WL11N */
}
#endif /* PPR_API */

/* calculate the offset from each per-rate power target in txpwr to the supplied
 * limit (or zero if txpwr[x] is less than limit[x]), and return the smallest
 * offset of relevant rates for bandtype/bw.
 */
static uint8
#ifdef PPR_API
wlc_channel_txpwr_margin(ppr_t *txpwr, ppr_t *limit, uint bandtype, uint bw)
#else
wlc_channel_txpwr_margin(txppr_t *txpwr, txppr_t *limit, uint bandtype, uint bw)
#endif
{
	uint8 margin = 0xff;

	wlc_channel_map_txppr_binary(wlc_channel_margin_summary_mapfn, &margin,
	                             bandtype, bw, txpwr, limit);

	return margin;
}

/* calculate the max power target of relevant rates for bandtype/bw */
static uint8
#ifdef PPR_API
wlc_channel_txpwr_max(ppr_t *txpwr, uint bandtype, uint bw)
#else
wlc_channel_txpwr_max(txppr_t *txpwr, uint bandtype, uint bw)
#endif
{
	uint8 pwr_max = 0;

	wlc_channel_map_txppr_binary(wlc_channel_max_summary_mapfn, &pwr_max,
	                             bandtype, bw, txpwr, NULL);

	return pwr_max;
}

#ifndef PPR_API

struct txp_range {
	int start;
	int end;
};

static const struct txp_range txp_ranges_20[] = {
	/* All 20MHz rates */
	{WL_TX_POWER_CCK_FIRST, (WL_TX_POWER_20_S3x3_FIRST + WL_NUM_RATES_MCS_1STREAM - 1)},
	{-1, -1}
};

static const struct txp_range txp_ranges_40[] = {
	/* All 40MHz rates */
	{WL_TX_POWER_OFDM40_FIRST, (WL_TX_POWER_40_S3x3_FIRST + WL_NUM_RATES_MCS_1STREAM - 1)},
	/* All 20 in 40MHz rates */
	{WL_TX_POWER_20UL_CCK_FIRST, (WL_TX_POWER_20UL_S3x3_FIRST + WL_NUM_RATES_MCS_1STREAM - 1)},
	{-1, -1}
};
#endif /* PPR_API */

/* return a ppr_t struct with the phy srom limits for the given channel */
static void
wlc_channel_srom_limits(wlc_cm_info_t *wlc_cm, chanspec_t chanspec,
#ifdef PPR_API
	ppr_t *srommin, ppr_t *srommax)
#else
	txppr_t *srommin, txppr_t *srommax)
#endif
{
	wlc_info_t *wlc = wlc_cm->wlc;
	wlc_phy_t *pi = wlc->band->pi;
	uint8 min_srom;
#ifndef PPR_API
	const struct txp_range *txp_ranges;
	int range_idx, start, end, txp_rate_idx;
	uint8 max_srom;
	uint channel = wf_chspec_ctlchan(chanspec);
#endif

#ifdef PPR_API

	if (srommin != NULL)
		ppr_clear(srommin);
	if (srommax != NULL)
		ppr_clear(srommax);

	if (!PHYTYPE_HT_CAP(wlc_cm->wlc->band))
		return;

	wlc_phy_txpower_sromlimit(pi, chanspec, &min_srom, srommax, 0);
	if (srommin != NULL)
		ppr_set_cmn_val(srommin, min_srom);

#else

	if (srommin != NULL)
		memset(srommin, (unsigned char)WL_RATE_DISABLED, WL_TX_POWER_RATES);
	if (srommax != NULL)
		memset(srommax, (unsigned char)WL_RATE_DISABLED, WL_TX_POWER_RATES);

	if (!PHYTYPE_HT_CAP(wlc_cm->wlc->band))
		return;

	if (CHSPEC_IS20(chanspec))
		txp_ranges = txp_ranges_20;
	else
		txp_ranges = txp_ranges_40;

	for (range_idx = 0; txp_ranges[range_idx].start != -1; range_idx++) {
		start = txp_ranges[range_idx].start;
		end = txp_ranges[range_idx].end;

		for (txp_rate_idx = start; txp_rate_idx <= end; txp_rate_idx++) {
			wlc_phy_txpower_sromlimit(pi, channel, &min_srom, &max_srom, txp_rate_idx);

			if (srommin != NULL)
				((uint8*)srommin)[txp_rate_idx] = min_srom;
			if (srommax != NULL)
				((uint8*)srommax)[txp_rate_idx] = max_srom;
		}
	}
#endif /* PPR_API */
}

/* Set a per-chain power limit for the given band
 * Per-chain offsets will be used to make sure the max target power does not exceed
 * the per-chain power limit
 */
int
wlc_channel_band_chain_limit(wlc_cm_info_t *wlc_cm, uint bandtype,
                             struct wlc_channel_txchain_limits *lim)
{
	wlc_info_t *wlc = wlc_cm->wlc;
#ifdef PPR_API
	ppr_t* txpwr;
#else
	txppr_t txpwr;
#endif
	int bandunit = (bandtype == WLC_BAND_2G) ? BAND_2G_INDEX : BAND_5G_INDEX;

	if (!PHYTYPE_HT_CAP(wlc_cm->wlc->band))
		return BCME_UNSUPPORTED;

	wlc_cm->bandstate[bandunit].chain_limits = *lim;

	if (CHSPEC_WLCBANDUNIT(wlc->chanspec) != bandunit)
		return 0;
#ifdef PPR_API

	if ((txpwr = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
		return 0;
	}

	/* update the current tx chain offset if we just updated this band's limits */
	wlc_channel_txpower_limits(wlc_cm, txpwr);
	wlc_channel_update_txchain_offsets(wlc_cm, txpwr);

	ppr_delete(wlc_cm->pub->osh, txpwr);
#else
	/* update the current tx chain offset if we just updated this band's limits */
	wlc_channel_txpower_limits(wlc_cm, &txpwr);
	wlc_channel_update_txchain_offsets(wlc_cm, &txpwr);
#endif /* PPR_API */
	return 0;
}

/* update the per-chain tx power offset given the current power targets to implement
 * the correct per-chain tx power limit
 */
static int
#ifdef PPR_API
wlc_channel_update_txchain_offsets(wlc_cm_info_t *wlc_cm, ppr_t *txpwr)
#else
wlc_channel_update_txchain_offsets(wlc_cm_info_t *wlc_cm, txppr_t *txpwr)
#endif
{
	wlc_info_t *wlc = wlc_cm->wlc;
	struct wlc_channel_txchain_limits *lim;
	wl_txchain_pwr_offsets_t offsets;
	chanspec_t chanspec;
#ifdef PPR_API
	ppr_t* srompwr;
#else
	txppr_t srompwr;
#endif
	int i, err;
	int max_pwr;
	int band, bw;
	int limits_present = FALSE;
	uint8 delta, margin, err_margin;
	wl_txchain_pwr_offsets_t cur_offsets;
#ifdef BCMDBG
	char chanbuf[CHANSPEC_STR_LEN];
#endif

	if (!PHYTYPE_HT_CAP(wlc->band))
		return BCME_UNSUPPORTED;

	chanspec = wlc->chanspec;
	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);
#ifdef PPR_API
	if ((srompwr = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(chanspec))) == NULL) {
		return BCME_ERROR;
	}
#endif
	/* initialize the offsets to a default of no offset */
	memset(&offsets, 0, sizeof(wl_txchain_pwr_offsets_t));

	lim = &wlc_cm->bandstate[CHSPEC_WLCBANDUNIT(chanspec)].chain_limits;

	/* see if there are any chain limits specified */
	for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
		if (lim->chain_limit[i] < WLC_TXPWR_MAX) {
			limits_present = TRUE;
			break;
		}
	}

	/* if there are no limits, we do not need to do any calculations */
	if (limits_present) {

		/* find the max power target for this channel and impose
		 * a txpwr delta per chain to meet the specified chain limits
		 * Bound the delta by the tx power margin
		 */
#ifdef PPR_API
		/* get the srom min powers */
		wlc_channel_srom_limits(wlc_cm, wlc->chanspec, srompwr, NULL);

		/* find the dB margin we can use to adjust tx power */
		margin = wlc_channel_txpwr_margin(txpwr, srompwr, band, bw);

		/* reduce the margin by the error margin 1.5dB backoff */
		err_margin = 6;	/* 1.5 dB in qdBm */
		margin = (margin >= err_margin) ? margin - err_margin : 0;

		/* get the srom max powers */
		wlc_channel_srom_limits(wlc_cm, wlc->chanspec, NULL, srompwr);

		/* combine the srom limits with the given regulatory limits
		 * to find the actual channel max
		 */
		/* wlc_channel_txpwr_vec_combine_min(srompwr, txpwr); */
		ppr_apply_vector_ceiling(srompwr, txpwr);

		/* int8 ppr_get_max_for_bw(ppr_t* pprptr, tx_bw_t bw) */
		max_pwr = (int)wlc_channel_txpwr_max(srompwr, band, bw);
#else

		/* get the srom min powers */
		wlc_channel_srom_limits(wlc_cm, wlc->chanspec, &srompwr, NULL);

		/* find the dB margin we can use to adjust tx power */
		margin = wlc_channel_txpwr_margin(txpwr, &srompwr, band, bw);

		/* reduce the margin by the error margin 1.5dB backoff */
		err_margin = 6;	/* 1.5 dB in qdBm */
		margin = (margin >= err_margin) ? margin - err_margin : 0;

		/* get the srom max powers */
		wlc_channel_srom_limits(wlc_cm, wlc->chanspec, NULL, &srompwr);

		/* combine the srom limits with the given regulatory limits
		 * to find the actual channel max
		 */
		wlc_channel_txpwr_vec_combine_min(&srompwr, txpwr);

		max_pwr = (int)wlc_channel_txpwr_max(&srompwr, band, bw);
#endif /* PPR_API */

		WL_NONE(("wl%d: %s: channel %s max_pwr %d margin %d\n",
		         wlc->pub->unit, __FUNCTION__,
		         wf_chspec_ntoa(wlc->chanspec, chanbuf), max_pwr, margin));

		/* for each chain, calculate an offset that keeps the max tx power target
		 * no greater than the chain limit
		 */
		for (i = 0; i < WLC_CHAN_NUM_TXCHAIN; i++) {
			WL_NONE(("wl%d: %s: chain_limit[%d] %d",
			         wlc->pub->unit, __FUNCTION__,
			         i, lim->chain_limit[i]));
			if (lim->chain_limit[i] < max_pwr) {
				delta = max_pwr - lim->chain_limit[i];

				WL_NONE((" desired delta -%u lim delta -%u",
				         delta, MIN(delta, margin)));

				/* limit to the margin allowed for our adjustmets */
				delta = MIN(delta, margin);

				offsets.offset[i] = -delta;
			}
			WL_NONE(("\n"));
		}
	} else {
		WL_NONE(("wl%d: %s skipping limit calculation since limits are MAX\n",
		         wlc->pub->unit, __FUNCTION__));
	}

	err = wlc_iovar_op(wlc, "txchain_pwr_offset", NULL, 0,
	                   &cur_offsets, sizeof(wl_txchain_pwr_offsets_t), IOV_GET, NULL);

	if (!err && bcmp(&cur_offsets.offset, &offsets.offset, WL_NUM_TXCHAIN_MAX)) {

		err = wlc_iovar_op(wlc, "txchain_pwr_offset", NULL, 0,
			&offsets, sizeof(wl_txchain_pwr_offsets_t), IOV_SET, NULL);
	}

	if (err) {
		WL_ERROR(("wl%d: txchain_pwr_offset failed: error %d\n",
		          wlc->pub->unit, err));
	}
#ifdef PPR_API
	ppr_delete(wlc_cm->pub->osh, srompwr);
#endif
	return err;
}

#if defined(BCMDBG)
static int
wlc_channel_dump_reg_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
#ifdef PPR_API
	ppr_t* txpwr;
#else
	txppr_t txpwr;
#endif
	char chanbuf[CHANSPEC_STR_LEN];
	int ant_gain;
	int sar;

	wlcband_t* band = wlc->band;

	ant_gain = band->antgain;
	sar = band->sar;
#ifdef PPR_API
	if ((txpwr = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
		return BCME_ERROR;
	}
	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, txpwr);

#else
	wlc_channel_reg_limits(wlc_cm, wlc->chanspec, &txpwr);
#endif

	bcm_bprintf(b, "Regulatory Limits for channel %s (SAR:",
		wf_chspec_ntoa(wlc->chanspec, chanbuf));

	if (sar == WLC_TXPWR_MAX)
		bcm_bprintf(b, " -  ");
	else
		bcm_bprintf(b, "%2d%s", QDB_FRAC_TRUNC(sar));
	bcm_bprintf(b, " AntGain: %2d%s)\n", QDB_FRAC_TRUNC(ant_gain));
#ifdef PPR_API
	wlc_channel_dump_txppr(b, txpwr, CHSPEC_TO_TX_BW(wlc->chanspec));
	ppr_delete(wlc_cm->pub->osh, txpwr);
#else
	wlc_channel_dump_txppr(b, &txpwr, PHYTYPE_HT_CAP(band));
#endif
	return 0;
}

#ifdef PPR_API

/* dump of regulatory power with local constraint factored in for the current channel */
static int
wlc_channel_dump_reg_local_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	ppr_t* txpwr;
	char chanbuf[CHANSPEC_STR_LEN];

	if ((txpwr = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(wlc->chanspec))) == NULL) {
		return BCME_ERROR;
	}

	wlc_channel_txpower_limits(wlc_cm, txpwr);

	bcm_bprintf(b, "Regulatory Limits with constraint for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, txpwr, CHSPEC_TO_TX_BW(wlc->chanspec));

	ppr_delete(wlc_cm->pub->osh, txpwr);
	return 0;
}

/* dump of srom per-rate max/min values for the current channel */
static int
wlc_channel_dump_srom_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	ppr_t* srompwr;
	char chanbuf[CHANSPEC_STR_LEN];

	if ((srompwr = ppr_create(wlc_cm->pub->osh, WL_TX_BW_80)) == NULL) {
		return BCME_ERROR;
	}

	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, NULL, srompwr);

	bcm_bprintf(b, "PHY/SROM Max Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, srompwr, CHSPEC_TO_TX_BW(wlc->chanspec));

	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, srompwr, NULL);

	bcm_bprintf(b, "PHY/SROM Min Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, srompwr, CHSPEC_TO_TX_BW(wlc->chanspec));

	ppr_delete(wlc_cm->pub->osh, srompwr);
	return 0;
}

#else

/* dump of regulatory power with local constraint factored in for the current channel */
static int
wlc_channel_dump_reg_local_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t txpwr;
	char chanbuf[CHANSPEC_STR_LEN];

	wlc_channel_txpower_limits(wlc_cm, &txpwr);

	bcm_bprintf(b, "Regulatory Limits with constraint for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, &txpwr, PHYTYPE_HT_CAP(wlc->band));

	return 0;
}

/* dump of srom per-rate max/min values for the current channel */
static int
wlc_channel_dump_srom_ppr(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	txppr_t srompwr;
	char chanbuf[CHANSPEC_STR_LEN];

	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, NULL, &srompwr);

	bcm_bprintf(b, "PHY/SROM Max Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, &srompwr, PHYTYPE_HT_CAP(wlc->band));

	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, &srompwr, NULL);

	bcm_bprintf(b, "PHY/SROM Min Limits for channel %s\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf));
	wlc_channel_dump_txppr(b, &srompwr, PHYTYPE_HT_CAP(wlc->band));

	return 0;
}
#endif /* PPR_API */

static void
wlc_channel_margin_calc_mapfn(void *ignore, uint8 *a, uint8 *b)
{
	if (*a > *b)
		*a = *a - *b;
	else
		*a = 0;
}

#ifdef PPR_API
/* dumps dB margin between a rate an the lowest allowable power target, and
 * summarize the min of the margins for the current channel
 */
static int
wlc_channel_dump_margin(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	ppr_t* txpwr;
	ppr_t* srommin;
	chanspec_t chanspec;
	int band, bw;
	uint8 margin;
	char chanbuf[CHANSPEC_STR_LEN];

	chanspec = wlc->chanspec;
	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);

	if ((txpwr = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(chanspec))) == NULL) {
		return 0;
	}
	if ((srommin = ppr_create(wlc_cm->pub->osh, PPR_CHSPEC_BW(chanspec))) == NULL) {
		ppr_delete(wlc_cm->pub->osh, txpwr);
		return 0;
	}

	wlc_channel_txpower_limits(wlc_cm, txpwr);

	/* get the srom min powers */
	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, srommin, NULL);

	/* find the dB margin we can use to adjust tx power */
	margin = wlc_channel_txpwr_margin(txpwr, srommin, band, bw);

	/* calulate the per-rate margins */
	wlc_channel_map_txppr_binary(wlc_channel_margin_calc_mapfn, NULL,
	                             band, bw, txpwr, srommin);

	bcm_bprintf(b, "Power margin for channel %s, min = %u\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf), margin);
	wlc_channel_dump_txppr(b, txpwr, CHSPEC_TO_TX_BW(wlc->chanspec));

	ppr_delete(wlc_cm->pub->osh, srommin);
	ppr_delete(wlc_cm->pub->osh, txpwr);
	return 0;
}
#else
/* dumps dB margin between a rate an the lowest allowable power target, and
 * summarize the min of the margins for the current channel
 */
txppr_t txpwr_farbod, srommin_farbod;
static int
wlc_channel_dump_margin(void *handle, struct bcmstrbuf *b)
{
	wlc_cm_info_t *wlc_cm = (wlc_cm_info_t*)handle;
	wlc_info_t *wlc = wlc_cm->wlc;
	chanspec_t chanspec;
	int band, bw;
	uint8 margin;
	char chanbuf[CHANSPEC_STR_LEN];

	chanspec = wlc->chanspec;
	band = CHSPEC_BAND(chanspec);
	bw = CHSPEC_BW(chanspec);

	memset(&txpwr_farbod, (unsigned char)WL_RATE_DISABLED, WL_TX_POWER_RATES);
	memset(&srommin_farbod, (unsigned char)WL_RATE_DISABLED, WL_TX_POWER_RATES);

	wlc_channel_txpower_limits(wlc_cm, &txpwr_farbod);

	/* get the srom min powers */
	wlc_channel_srom_limits(wlc_cm, wlc->chanspec, &srommin_farbod, NULL);

	/* find the dB margin we can use to adjust tx power */
	margin = wlc_channel_txpwr_margin(&txpwr_farbod, &srommin_farbod, band, bw);

	/* calulate the per-rate margins */
	wlc_channel_map_txppr_binary(wlc_channel_margin_calc_mapfn, NULL,
	                             band, bw, &txpwr_farbod, &srommin_farbod);

	bcm_bprintf(b, "Power margin for channel %s, min = %u\n",
	            wf_chspec_ntoa(wlc->chanspec, chanbuf), margin);
	wlc_channel_dump_txppr(b, &txpwr_farbod, PHYTYPE_HT_CAP(wlc->band));

	return 0;
}
#endif /* PPR_API */

static int
wlc_channel_supported_country_regrevs(void *handle, struct bcmstrbuf *b)
{
	int iter;
	ccode_t cc;
	unsigned int regrev;

	if (clm_iter_init(&iter) == CLM_RESULT_OK) {
		while (clm_country_iter((clm_country_t*)&iter, cc, &regrev) == CLM_RESULT_OK) {
			bcm_bprintf(b, "%c%c/%u\n", cc[0], cc[1], regrev);

		}
	}
	return 0;
}

#endif 

void
wlc_dump_clmver(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b)
{
	struct clm_data_header* clm_inc_headerptr = wlc_cm->clm_inc_headerptr;
	const struct clm_data_header* clm_base_headerptr = wlc_cm->clm_base_dataptr;
	const char* verstrptr;

	if (clm_base_headerptr == NULL)
		clm_base_headerptr = &clm_header;

	bcm_bprintf(b, "API: %d.%d\nData: %s\nCompiler: %s\n%s\n",
		clm_base_headerptr->format_major, clm_base_headerptr->format_minor,
		clm_base_headerptr->clm_version, clm_base_headerptr->compiler_version,
		clm_base_headerptr->generator_version);
	verstrptr = clm_get_base_app_version_string();
	if (verstrptr != NULL)
		bcm_bprintf(b, "Customization: %s\n", verstrptr);

	if (clm_inc_headerptr != NULL) {
		bcm_bprintf(b, "Inc Data: %s\nInc Compiler: %s\nInc %s\n",
			clm_inc_headerptr->clm_version,
			clm_inc_headerptr->compiler_version, clm_inc_headerptr->generator_version);
		verstrptr = clm_get_inc_app_version_string();
		if (verstrptr != NULL)
			bcm_bprintf(b, "Inc Customization: %s\n", verstrptr);
	}
}
