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
 * $Id: wlc_channel.h 380365 2013-01-22 18:49:30Z $
 */

#ifndef _WLC_CHANNEL_H_
#define _WLC_CHANNEL_H_

#include <wlc_phy_hal.h>
#include <wlc_clm.h>

#define WLC_TXPWR_DB_FACTOR 4 /* conversion for phy txpwr cacluations that use .25 dB units */

struct wlc_info;

#define MAX_CLMVER_STRLEN 128


/*
 * CHSPEC_IS40()/CHSPEC_IS40_UNCOND
 * Use the conditional form of this macro, CHSPEC_IS40(), in code that is checking
 * chanspecs that have already been cleaned for an operational bandwidth supported by the
 * driver compile, such as the current radio channel or the currently associated BSS's
 * chanspec.
 * Use the unconditional form of this macro, CHSPEC_IS40_UNCOND(), in code that is
 * checking chanspecs that may not have a bandwidth supported as an operational bandwidth
 * by the driver compile, such as chanspecs that are specified in incoming ioctls or
 * chanspecs parsed from received packets.
 */
#define CHSPEC_IS40_UNCOND(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_40)
#ifdef CHSPEC_IS40
#undef CHSPEC_IS40
#endif
#ifdef WL11N
#define CHSPEC_IS40(chspec)	CHSPEC_IS40_UNCOND(chspec)
#else /* WL11N */
#define CHSPEC_IS40(chspec)	0
#endif /* WL11N */

#define CHSPEC_IS80_UNCOND(chspec)	(((chspec) & WL_CHANSPEC_BW_MASK) == WL_CHANSPEC_BW_80)
#ifdef CHSPEC_IS80
#undef CHSPEC_IS80
#endif
#ifdef WL11AC
#define CHSPEC_IS80(chspec)	CHSPEC_IS80_UNCOND(chspec)
#else /* WL11AC */
#define CHSPEC_IS80(chspec)	0
#endif /* WL11AC */


#define MAXRCLISTSIZE	32
#define MAXREGCLASS	40
#define MAXRCTBL	24
#define MAXRCVEC	(MAXREGCLASS/NBBY)

/* Regulatory Class list */
#define MAXRCLIST	3
#define WLC_RCLIST_20	0
#define WLC_RCLIST_40L	1
#define WLC_RCLIST_40U	2

/* regulatory bitvec */
typedef struct {
	uint8	vec[MAXRCVEC];	/* bitvec of regulatory class */
} rcvec_t;

/* channel & regulatory class pair */
typedef struct {
	uint8 chan;		/* channel */
	uint8 rclass;		/* regulatory class */
} chan_rc_t;

/* regclass_t */
typedef struct {
	uint8 len;		/* number of entry */
	chan_rc_t rctbl[MAXRCTBL];	/* regulatory class table */
} rcinfo_t;

/* maxpwr mapping to 5GHz band channels:
 * maxpwr[0] - channels [34-48]
 * maxpwr[1] - channels [52-60]
 * maxpwr[2] - channels [62-64]
 * maxpwr[3] - channels [100-140]
 * maxpwr[4] - channels [149-165]
 */
#define BAND_5G_PWR_LVLS	5 /* 5 power levels for 5G */

/* power level in group of 2.4GHz band channels:
 * maxpwr[0] - CCK  channels [1]
 * maxpwr[1] - CCK  channels [2-10]
 * maxpwr[2] - CCK  channels [11-14]
 * maxpwr[3] - OFDM channels [1]
 * maxpwr[4] - OFDM channels [2-10]
 * maxpwr[5] - OFDM channels [11-14]
 */

/* macro to get 2.4 GHz channel group index for tx power */
#define CHANNEL_POWER_IDX_2G_CCK(c) (((c) < 2) ? 0 : (((c) < 11) ? 1 : 2)) /* cck index */
#define CHANNEL_POWER_IDX_2G_OFDM(c) (((c) < 2) ? 3 : (((c) < 11) ? 4 : 5)) /* ofdm index */

/* macro to get 5 GHz channel group index for tx power */
#define CHANNEL_POWER_IDX_5G(c) \
	(((c) < 52) ? 0 : (((c) < 62) ? 1 :(((c) < 100) ? 2 : (((c) < 149) ? 3 : 4))))

#define WLC_MAXPWR_TBL_SIZE		6 /* max of BAND_5G_PWR_LVLS and 6 for 2.4 GHz */
#define WLC_MAXPWR_MIMO_TBL_SIZE	14 /* max of BAND_5G_PWR_LVLS and 14 for 2.4 GHz */

/* locale channel and power info. */
typedef struct {
	uint32	valid_channels;
	uint8	radar_channels;		/* List of radar sensitive channels */
	uint8	restricted_channels;	/* List of channels used only if APs are detected */
	int8	maxpwr[WLC_MAXPWR_TBL_SIZE];	/* Max tx pwr in qdBm for each sub-band */
	int8	pub_maxpwr[BAND_5G_PWR_LVLS];	/* Country IE advertised max tx pwr in dBm
						 * per sub-band
						 */
	uint8	flags;
} locale_info_t;

/* bits for locale_info flags */
#define WLC_PEAK_CONDUCTED	0x00 /* Peak for locals */
#define WLC_EIRP			0x01 /* Flag for EIRP */
#define WLC_DFS_TPC			0x02 /* Flag for DFS TPC */
#define WLC_NO_80MHZ		0x04 /* Flag for No 80MHz channels */
#define WLC_NO_40MHZ		0x08 /* Flag for No MIMO 40MHz */
#define WLC_NO_MIMO			0x10 /* Flag for No MIMO, 20 or 40 MHz */
#define WLC_RADAR_TYPE_EU	0x20 /* Flag for EU */
#define WLC_TXBF			0x40 /* Flag for Tx beam forming */
#define WLC_FILT_WAR		0x80 /* Flag for apply filter war */
#define WLC_DFS_FCC			WLC_DFS_TPC /* Flag for DFS FCC */
#define WLC_DFS_EU			(WLC_DFS_TPC | WLC_RADAR_TYPE_EU) /* Flag for DFS EU */

#define ISDFS_EU(fl)		(((fl) & WLC_DFS_EU) == WLC_DFS_EU)

/* locale per-channel tx power limits for MIMO frames
 * maxpwr arrays are index by channel for 2.4 GHz limits, and
 * by sub-band for 5 GHz limits using CHANNEL_POWER_IDX_5G(channel)
 */
typedef struct {
	int8	maxpwr20[WLC_MAXPWR_MIMO_TBL_SIZE];	/* tx 20 MHz power limits, qdBm units */
	int8	maxpwr40[WLC_MAXPWR_MIMO_TBL_SIZE];	/* tx 40 MHz power limits, qdBm units */
	uint8	flags;
} locale_mimo_info_t;

/* dfs setting */
#define WLC_DFS_RADAR_CHECK_INTERVAL	150 	/* radar check interval in ms */
#define WLC_DFS_CAC_TIME_USE_DEFAULTS	-1	/* use default CAC times */
#define WLC_DFS_CAC_TIME_SEC_DEFAULT	60	/* default CAC time in seconds */
#define WLC_DFS_CAC_TIME_SEC_DEF_EUWR	600	/* default CAC time for european weather radar */
#define WLC_DFS_CAC_TIME_SEC_MAX	1000	/* max CAC time in second */
#define WLC_DFS_NOP_SEC_DEFAULT		(1800 + 60) 	/* in second.
							 * plus 60 is a margin to ensure a mininum
							 * of 1800 seconds
							 */
#define WLC_DFS_CSA_MSEC	4000	/* mininum 4 seconds of csa process */
#define WLC_DFS_CSA_BEACONS	9	/* minimum 9 beacons for csa */
#define WLC_CHANBLOCK_FOREVER   0xffffffff  /* special define for specific nop requirements */

extern const chanvec_t chanvec_all_2G;
extern const chanvec_t chanvec_all_5G;

/*
 * Country names and abbreviations with locale defined from ISO 3166
 */
struct country_info {
	const uint8 locale_2G;		/* 2.4G band locale */
	const uint8 locale_5G;		/* 5G band locale */
#ifdef WL11N
	const uint8 locale_mimo_2G;	/* 2.4G mimo info */
	const uint8 locale_mimo_5G;	/* 5G mimo info */
#endif /* WL11N */
};

typedef struct country_info country_info_t;

typedef struct wlc_cm_info wlc_cm_info_t;

#define WLC_CHAN_NUM_TXCHAIN	4
struct wlc_channel_txchain_limits {
	int8	chain_limit[WLC_CHAN_NUM_TXCHAIN];	/* quarter dBm limit for each chain */
};

extern wlc_cm_info_t *wlc_channel_mgr_attach(struct wlc_info *wlc);
extern void wlc_channel_mgr_detach(wlc_cm_info_t *wlc_cm);

extern int wlc_set_countrycode(wlc_cm_info_t *wlc_cm, const char* ccode);
extern int wlc_set_countrycode_rev(
	wlc_cm_info_t *wlc_cm, const char* ccode, int regrev);

extern const char* wlc_channel_country_abbrev(wlc_cm_info_t *wlc_cm);
extern const char* wlc_channel_ccode(wlc_cm_info_t *wlc_cm);
extern uint wlc_channel_regrev(wlc_cm_info_t *wlc_cm);
extern uint8 wlc_channel_locale_flags(wlc_cm_info_t *wlc_cm);
extern uint8 wlc_channel_locale_flags_in_band(wlc_cm_info_t *wlc_cm, uint bandunit);

extern int wlc_handle_clm_dload(wlc_cm_info_t *wlc_cm, void* data_chunk, int clm_offset,
	int chunk_len, int clm_total_len, int ds_id);


/* Specify channel types for get_first and get_next channel routines */
#define	CHAN_TYPE_ANY		0	/* Dont care */
#define	CHAN_TYPE_CHATTY	1	/* Normal, non-radar channel */
#define	CHAN_TYPE_QUIET		2	/* Radar channel */

extern void wlc_quiet_channels_reset(wlc_cm_info_t *wlc_cm);
extern bool wlc_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec);
extern void wlc_set_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec);
extern void wlc_clr_quiet_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec);

#define	VALID_CHANNEL20_DB(wlc, val) wlc_valid_channel20_db((wlc)->cmi, val)
#define	VALID_CHANNEL20_IN_BAND(wlc, bandunit, val) \
	wlc_valid_channel20_in_band((wlc)->cmi, bandunit, val)
#define	VALID_CHANNEL20(wlc, val) wlc_valid_channel20((wlc)->cmi, val)
#define VALID_40CHANSPEC_IN_BAND(wlc, bandunit) wlc_valid_40chanspec_in_band((wlc)->cmi, bandunit)
#define VALID_80CHANSPEC_IN_BAND(wlc, bandunit) wlc_valid_80chanspec_in_band((wlc)->cmi, bandunit)

extern bool wlc_valid_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec);
extern bool wlc_valid_chanspec_db(wlc_cm_info_t *wlc_cm, chanspec_t chspec);
extern bool wlc_valid_channel20_db(wlc_cm_info_t *wlc_cm, uint val);
extern bool wlc_valid_channel20_in_band(wlc_cm_info_t *wlc_cm, uint bandunit, uint val);
extern bool wlc_valid_channel20(wlc_cm_info_t *wlc_cm, uint val);
extern bool wlc_valid_40chanspec_in_band(wlc_cm_info_t *wlc_cm, uint bandunit);
extern bool wlc_valid_80chanspec_in_band(wlc_cm_info_t *wlc_cm, uint bandunit);

extern chanspec_t wlc_default_chanspec(wlc_cm_info_t *wlc_cm, bool hw_fallback);
extern chanspec_t wlc_next_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t cur_ch, int type, bool db);
extern bool wlc_radar_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chanspec);
extern bool wlc_restricted_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec);
extern void wlc_clr_restricted_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chspec);
extern bool wlc_has_restricted_chanspec(wlc_cm_info_t *wlc_cm);


extern void wlc_channel_reg_limits(wlc_cm_info_t *wlc_cm,
#ifdef PPR_API
	chanspec_t chanspec, ppr_t *txpwr);
#else
	chanspec_t chanspec, txppr_t *txpwr);
#endif
extern void wlc_channel_set_chanspec(wlc_cm_info_t *wlc_cm, chanspec_t chanspec,
	uint8 local_constraint_qdbm);
extern int wlc_channel_set_txpower_limit(wlc_cm_info_t *wlc_cm, uint8 local_constraint_qdbm);
#ifdef WL11N
extern uint8 wlc_get_regclass_list(wlc_cm_info_t *wlc_cm, uint8 *rclist, uint lsize,
	chanspec_t chanspec, bool ie_order);
extern uint8 wlc_rclass_extch_get(wlc_cm_info_t *wlc_cm, uint8 rclass);
#else
#define wlc_get_regclass_list(a, b, c, d, e) (0)
#define wlc_rclass_extch_get(a, b) (0)
#endif /* WL11N */

#if defined(AP) && defined(RADAR)
extern bool wlc_is_european_weather_radar_channel(struct wlc_info *wlc, chanspec_t chanspec);
#endif

extern clm_result_t wlc_country_lookup_direct(const char* ccode, uint regrev,
	clm_country_t *country);
extern clm_result_t wlc_country_lookup(struct wlc_info *wlc, const char* ccode,
	clm_country_t *country);
clm_result_t wlc_locale_get_channels(clm_country_locales_t *locales, clm_band_t band,
	chanvec_t *valid_channels, chanvec_t *restricted_channels);
extern clm_result_t wlc_get_locale(clm_country_t country, clm_country_locales_t *locales);
extern clm_result_t wlc_get_flags(clm_country_locales_t *locales, clm_band_t band, uint8 *flags);
#if defined(STA) && defined(WL11D)
clm_result_t wlc_country_lookup_ext(struct wlc_info *wlc, const char* ccode,
	clm_country_t *country);
#endif
extern bool wlc_japan(struct wlc_info *wlc);
extern clm_country_t wlc_get_country(struct wlc_info *wlc);
extern int wlc_get_channels_in_country(struct wlc_info *wlc, void *arg);
extern int wlc_get_country_list(struct wlc_info *wlc, void *arg);
extern int8 wlc_get_reg_max_power_for_channel(wlc_cm_info_t *wlc_cm, int chan, bool external);
extern void wlc_get_valid_chanspecs(wlc_cm_info_t *wlc_cm, wl_uint32_list_t *list,
	uint bw, bool band2G, const char *abbrev);
extern int wlc_channel_dump_locale(void *handle, struct bcmstrbuf *b);

#ifdef DSLCPE
#if defined(AP) && defined(RADAR)
extern bool wlc_is_EU_country(struct wlc_info *wlc);
#endif
#endif

extern uint8 wlc_get_regclass(wlc_cm_info_t *wlc_cm, chanspec_t chanspec);
extern uint8 wlc_rclass_get_channel_list(wlc_cm_info_t *wlc_cm, const char *abbrev, uint8 rclass,
	bool bw20, wl_uint32_list_t *list);
extern int wlc_dump_rclist(const char *name, uint8 *rclist, uint8 rclen, struct bcmstrbuf *b);
extern bool wlc_channel_get_chanvec(struct wlc_info *wlc, const char* country_abbrev,
	int bandtype, chanvec_t *channels);
extern int wlc_channel_band_chain_limit(wlc_cm_info_t *wlc_cm, uint bandtype,
	struct wlc_channel_txchain_limits *lim);
extern void wlc_dump_clmver(wlc_cm_info_t *wlc_cm, struct bcmstrbuf *b);

extern bool wlc_channel_sarenable_get(wlc_cm_info_t *wlc_cm);
extern void wlc_channel_sarenable_set(wlc_cm_info_t *wlc_cm, bool state);
#ifdef WL_SARLIMIT
extern void wlc_channel_sar_init(wlc_cm_info_t *wlc_cm);
extern void wlc_channel_sarlimit_subband(wlc_cm_info_t *wlc_cm, chanspec_t chanspec, uint8 *sar);
extern int wlc_channel_sarlimit_get(wlc_cm_info_t *wlc_cm, sar_limit_t *sar);
extern int wlc_channel_sarlimit_set(wlc_cm_info_t *wlc_cm, sar_limit_t *sar);
#else
#define wlc_channel_sar_init(a) do { } while (0)
#define wlc_channel_sarlimit_subband(a, b, c) do { } while (0)
#define wlc_channel_sarlimit_get(a, b) (0)
#define wlc_channel_sarlimit_set(a, b) (0)
#endif /* WL_SARLIMIT */
#endif /* _WLC_CHANNEL_H */
