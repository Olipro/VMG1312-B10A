/*
 * Common (OS-independent) portion of
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_scan.c 364406 2012-10-23 23:26:51Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.1d.h>
#include <proto/802.11.h>
#include <proto/802.11e.h>
#include <proto/wpa.h>
#include <proto/vlan.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <epivers.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc_channel.h>
#include <wlc_scandb.h>
#include <wlc.h>
#include <wlc_hw.h>
#ifdef AP
#include <wlc_apps.h>
#endif
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#ifdef WLLED
#include <wlc_led.h>
#endif
#include <wlc_frmutil.h>
#include <wlc_security.h>
#ifdef WLAMSDU
#include <wlc_amsdu.h>
#endif
#include <wlc_event.h>
#ifdef WLDIAG
#include <wlc_diag.h>
#endif /* WLDIAG */
#include <wl_export.h>
#if defined(BCMSUP_PSK)
#include <proto/eapol.h>
#include <wlc_sup.h>
#endif 
#ifdef WET
#include <wlc_wet.h>
#endif /* WET */
#if defined(BCMNVRAMW) || defined(WLTEST)
#include <sbchipc.h>
#include <bcmotp.h>
#endif 
#ifdef BCMCCMP
#include <bcmcrypto/aes.h>
#endif /* BCMCCMP */
#include <wlc_rm.h>
#ifdef BCM_WL_EMULATOR
#include <wl_bcm57emu.h>
#endif
#include <wlc_ap.h>
#ifdef AP
#include <wlc_apcs.h>
#endif
#include <wlc_assoc.h>
#include <wlc_scan.h>
#ifdef WLP2P
#include <wlc_p2p.h>
#endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif
#include <wlc_11h.h>
#include <wlc_11d.h>
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif
#ifdef WL_BCN_COALESCING
#include <wlc_bcn_clsg.h>
#endif /* WL_BCN_COALESCING */
#ifdef WLOFFLD
#include <wlc_offloads.h>
#endif
#ifdef BCMDBG
/* redefine WL_INFORM to print if either INFORM or SCAN level messaging is on */
#undef WL_INFORM
#define	WL_INFORM(args)									\
	do {										\
		if ((wl_msg_level & WL_INFORM_VAL) || (wl_msg_level2 & WL_SCAN_VAL))	\
		    WL_PRINT(args);					\
	} while (0)
#undef WL_INFORM_ON
#define WL_INFORM_ON()	((wl_msg_level & WL_INFORM_VAL) || (wl_msg_level2 & WL_SCAN_VAL))
#endif /* BCMDBG */

/* scan times in milliseconds */
#define WLC_SCAN_MIN_PROBE_TIME	10	/* minimum useful time for an active probe */
#define WLC_SCAN_HOME_TIME	45	/* time for home channel processing */
#define WLC_SCAN_ASSOC_TIME 20 /* time to listen on a channel for probe resp while associated */

#ifdef BCMQT_CPU
#define WLC_SCAN_UNASSOC_TIME 400	/* qt is slow */
#else
#define WLC_SCAN_UNASSOC_TIME 40	/* listen on a channel for prb rsp while unassociated */
#endif
#define WLC_SCAN_NPROBES	2	/* do this many probes on each channel for an active scan */

/* scan_pass state values */
#define WLC_SCAN_SUCCESS	0 /* scan success */
#define WLC_SCAN_ABORT		-2 /* Abort the scan */
#define WLC_SCAN_START		-1 /* Start the scan */
#define WLC_SCAN_CHANNEL_PREP	0  /* Prepare the channel for the scan */

/* Enables the iovars */
#define WLC_SCAN_IOVARS

#if defined(BRINGUP_BUILD)
#undef WLC_SCAN_IOVARS
#endif

typedef struct scan_info scan_info_t;

#ifdef WLSCANCACHE
static int wlc_scan_watchdog(void *hdl);
#endif
static int wlc_scan_down(void *hdl);

static void wlc_scan_channels(wlc_info_t *wlc, chanspec_t *chanspec_list, int *pchannel_num,
	int channel_max, chanspec_t chanspec_start, int channel_type);

static void wlc_scantimer(void *arg);

#ifdef WLC_SCAN_IOVARS
static int wlc_scan_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *params, uint p_len, void *arg,
	int len, int val_size, struct wlc_if *wlcif);
#endif /* WLC_SCAN_IOVARS */

static void wlc_scan_return_home_channel(wlc_scan_info_t *wlc_scan_info);
static void wlc_scan_callback(wlc_scan_info_t *wlc_scan_info, uint status);

static uint wlc_scan_prohibited_channels(wlc_scan_info_t *wlc_scan_info,
	chanspec_t *chanspec_list, int channel_max);
static void wlc_scan_do_pass(scan_info_t *scan_info, chanspec_t chanspec);
static void wlc_scan_sendprobe(wlc_info_t *wlc, void *scan_info);
static void wlc_scan_ssidlist_free(scan_info_t *scan_info);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
static void wlc_scan_print_ssids(wlc_ssid_t *ssid, int nssid);
#endif
#if defined(BCMDBG)
static int wlc_scan_dump(scan_info_t *si, struct bcmstrbuf *b);
#endif

#ifdef WL11N
static void wlc_ht_obss_scan_update(scan_info_t *scan_info, int status);
#else /* WL11N */
#define wlc_ht_obss_scan_update(a, b) do {} while (0)
#endif /* WL11N */


#ifdef WLSCANCACHE
static void wlc_scan_merge_cache(wlc_scan_info_t *scan_pub, uint current_timestamp,
                                 const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                                 int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                                 wlc_bss_list_t *bss_list);
static void wlc_scan_fill_cache(scan_info_t *scan_info, uint current_timestamp);
static void wlc_scan_build_cache_list(void *arg1, void *arg2, uint timestamp,
                                      struct ether_addr *BSSID, wlc_ssid_t *SSID,
                                      int BSS_type, chanspec_t chanspec, void *data, uint datalen);
static void wlc_scan_cache_result(scan_info_t *scan_info);
#else
#define wlc_scan_fill_cache(si, ts)	do {} while (0)
#define wlc_scan_merge_cache(si, ts, BSSID, nssid, SSID, BSS_type, c_list, c_num, bss_list)
#define wlc_scan_cache_result(si)
#endif

#ifdef EXTENDED_SCAN
#define SCAN_TYPE_FOREGROUND(scan_info) 	\
	(((scan_info)->extdscan && 	\
	  ((scan_info)->scan_type == EXTDSCAN_FOREGROUND_SCAN)) ? TRUE : FALSE)
#define SCAN_TYPE_FBACKGROUND(scan_info) 	\
	(((!(scan_info)->extdscan) || (scan_info->extdscan && 	\
	((scan_info)->scan_type == EXTDSCAN_FORCEDBACKGROUND_SCAN))) ? TRUE : FALSE)
#define SCAN_TYPE_BACKGROUND(scan_info) 	\
	(((scan_info)->extdscan && 	\
	((scan_info)->scan_type == EXTDSCAN_BACKGROUND_SCAN)) ? TRUE : FALSE)

#define CHANNEL_PASSIVE_DWELLTIME(s)	\
	((s)->extdscan && (s)->chan_list[(s)->channel_idx].channel_maxtime) ?	\
	((s)->chan_list[(s)->channel_idx].channel_maxtime) : (s)->passive_time

#define CHANNEL_ACTIVE_DWELLTIME(s)	\
	((s)->extdscan && (s)->chan_list[(s)->channel_idx].channel_maxtime) ?	\
	((s)->chan_list[(s)->channel_idx].channel_maxtime) : (s)->active_time

#define SCAN_PROBE_TXRATE(scan_info)	\
	(((scan_info)->extdscan) ? (scan_info)->max_txrate : 0)
#else /* EXTENDED_SCAN */
#define SCAN_TYPE_FOREGROUND(scan_info) 	FALSE
#define SCAN_TYPE_BACKGROUND(scan_info) 	FALSE
#define SCAN_TYPE_FBACKGROUND(scan_info) 	TRUE

#define CHANNEL_PASSIVE_DWELLTIME(s) ((s)->passive_time)
#define CHANNEL_ACTIVE_DWELLTIME(s) ((s)->active_time)
#define SCAN_PROBE_TXRATE(scan_info)	0
#endif /* EXTENDED_SCAN */

#ifdef WLC_SCAN_IOVARS
/* IOVar table */

/* Parameter IDs, for use only internally to wlc -- in the wlc_iovars
 * table and by the wlc_doiovar() function.  No ordering is imposed:
 * the table is keyed by name, and the function uses a switch.
 */
enum {
	IOV_PASSIVE = 1,
	IOV_SCAN_ASSOC_TIME,
	IOV_SCAN_UNASSOC_TIME,
	IOV_SCAN_PASSIVE_TIME,
	IOV_SCAN_HOME_TIME,
	IOV_SCAN_NPROBES,
	IOV_SCAN_EXTENDED,
	IOV_SCAN_NOPSACK,
	IOV_SCANCACHE,
	IOV_SCANCACHE_TIMEOUT,
	IOV_SCANCACHE_CLEAR,
	IOV_SCAN_FORCE_ACTIVE,	/* force passive to active conversion in radar/restricted channel */
	IOV_SCAN_ASSOC_TIME_DEFAULT,
	IOV_SCAN_UNASSOC_TIME_DEFAULT,
	IOV_SCAN_PASSIVE_TIME_DEFAULT,
	IOV_SCAN_HOME_TIME_DEFAULT,
	IOV_SCAN_DBG,
	IOV_LAST 		/* In case of a need to check max ID number */
};

/* AP IO Vars */
static const bcm_iovar_t wlc_scan_iovars[] = {
	{"passive", IOV_PASSIVE,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#ifdef STA
	{"scan_assoc_time", IOV_SCAN_ASSOC_TIME,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"scan_unassoc_time", IOV_SCAN_UNASSOC_TIME,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#endif /* STA */
	{"scan_passive_time", IOV_SCAN_PASSIVE_TIME,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	/* unlike the other scan times, home_time can be zero */
	{"scan_home_time", IOV_SCAN_HOME_TIME,
	(IOVF_WHL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#ifdef STA
	{"scan_nprobes", IOV_SCAN_NPROBES,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_INT8, 0
	},
	{"scan_force_active", IOV_SCAN_FORCE_ACTIVE,
	0, IOVT_BOOL, 0
	},
#ifdef EXTENDED_SCAN
	{"extdscan", IOV_SCAN_EXTENDED,
	(IOVF_NTRL), IOVT_BUFFER, 0
	},
#ifdef BCMDBG
	{"scan_nopsack", IOV_SCAN_NOPSACK,
	(IOVF_NTRL), IOVT_UINT8, 0
	},
#endif /* BCMDBG */
#endif /* EXTENDED_SCAN */
#ifdef WLSCANCACHE
	{"scancache", IOV_SCANCACHE,
	(IOVF_OPEN_ALLOW), IOVT_BOOL, 0
	},
	{"scancache_timeout", IOV_SCANCACHE_TIMEOUT,
	(IOVF_OPEN_ALLOW), IOVT_INT32, 0
	},
	{"scancache_clear", IOV_SCANCACHE_CLEAR,
	(IOVF_OPEN_ALLOW), IOVT_VOID, 0
	},
#endif /* WLSCANCACHE */
#endif /* STA */
#ifdef STA
	{"scan_assoc_time_default", IOV_SCAN_ASSOC_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"scan_unassoc_time_default", IOV_SCAN_UNASSOC_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#endif /* STA */
	{"scan_passive_time_default", IOV_SCAN_PASSIVE_TIME_DEFAULT,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	/* unlike the other scan times, home_time can be zero */
	{"scan_home_time_default", IOV_SCAN_HOME_TIME_DEFAULT,
	(IOVF_WHL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
#ifdef BCMDBG
	{"scan_dbg", IOV_SCAN_DBG, 0, IOVT_UINT8, 0},
#endif
	{NULL, 0, 0, 0, 0 }
};
#endif /* WLC_SCAN_IOVARS */

struct scan_info {
	struct wlc_scan_info	*scan_pub;
	uint		unit;
	wlc_info_t	*wlc;
	osl_t		*osh;
	wlc_scandb_t	*sdb;
	uint		memsize;	/* allocated size of this structure (for freeing) */

	int		channel_idx;	/* index in chanspec_list of current channel we are
					 * scanning
					 */
	/* scan times are in milliseconds */
	chanspec_t	chanspec_list[MAXCHANNEL];	/* list of channels to scan */
	int		channel_num;	/* length of chanspec_list */
	int8		pass;		/* current scan pass or scan state */
	int8		nprobes;	/* number of probes per channel */
	int8		npasses;	/* number of passes per channel */
	uint16		home_time;	/* dwell time for the home channel between channel
					 * scans
					 */
	uint16		active_time;	/* dwell time per channel for active scanning */
	uint16		passive_time;	/* dwell time per channel for passive scanning */
	int		away_channels_limit;	/* number of non-home channels to scan before
					 * returning to home
					 */
	int		away_channels_cnt;	/* number of non-home channels we have scanned
					 * consecutively
					 */
	uint32		start_tsf;	/* TSF read from chip at start of channel scan */
	struct wl_timer *timer;		/* timer for BSS scan operations */
	scancb_fn_t	cb;		/* function to call when scan is done */
	void		*cb_arg;	/* arg to cb fn */
	/* scan defaults */
	struct {
		uint16	home_time;	/* dwell time for the home channel between channel
					 * scans
					 */
		uint16	unassoc_time;	/* dwell time per channel when unassociated */
		uint16	assoc_time;	/* dwell time per channel when associated */
		uint16	passive_time;	/* dwell time per channel for passive scanning */
		int8	nprobes;	/* number of probes per channel */
		int8	passive;	/* scan type: 1 -> passive, 0 -> active */
	} defaults;

	bool		extdscan;
	int 		nssid;		/* number off ssids in the ssid list */
	wlc_ssid_t	*ssid_list;	/* ssids to look for in scan (could be dynamic) */
	wlc_ssid_t	*ssid_prealloc;	/* pointer to preallocated (non-dynamic) store */
	int		nssid_prealloc;	/* number of preallocated entries */

#ifdef EXTENDED_SCAN
#ifdef BCMDBG
	uint8 		test_nopsack;
#endif	/* BCMDBG */
	chan_scandata_t *chan_list;	/* channel list with extended scan list */
	chan_scandata_t *chan_prealloc; /* pointer to preallocated (non-dynamic) store */
	int		nchan_prealloc; /* number of preallocated entries */
	ratespec_t	max_txrate;	/* txrate for the probes */
	wl_scan_type_t	scan_type;	/* scan type foreground/background/forcedbackground */
#endif /* EXTENDED_SCAN */
	uint8   ssid_wildcard_enabled;
	wlc_bsscfg_t	*bsscfg;
	bool	force_active;	/* Force passive to active conversion on radar/restricted channel */
#ifdef BCMDBG
	uint8	debug;
#endif
	actcb_fn_t	act_cb;		/* function to call when scan is done */
	void		*act_cb_arg;	/* arg to cb fn */
};

typedef struct scan_iter_params {
	wlc_bss_list_t *bss_list;	/* list on which cached items will be added */
	int merge;			/* if TRUE, merge cached entries with different timestamp
					 * to existing entries on bss_list
					 */
	uint current_ts;		/* timestamp of most recent cache additions */
} scan_iter_params_t;

/* debug timer used in scan module */
/* #define DEBUG_SCAN_TIMER */
#ifdef DEBUG_SCAN_TIMER
static void _wlc_scan_add_timer(wlc_info_t *wlc, scan_info_t *scan, uint to, bool prd,
	const char *func, int line)
{
	WL_SCAN(("wl%d: %s(%d): wl_add_timer: timeout %u tsf %u\n",
	         wlc->pub->unit, func, line, to, R_REG(wlc->osh, &wlc->regs->tsf_timerlow)));
	wl_add_timer(wlc->wl, scan->timer, to, prd);
}
static bool _wlc_scan_del_timer(wlc_info_t *wlc, scan_info_t *scan,
	const char *func, int line)
{
	WL_SCAN(("wl%d: %s(%d): wl_del_timer: tsf %u\n",
	         wlc->pub->unit, func, line, R_REG(wlc->osh, &wlc->regs->tsf_timerlow)));
	return wl_del_timer(wlc->wl, scan->timer);
}
#define wlc_scan_add_timer(wlc, scan, to, prd) _wlc_scan_add_timer(wlc, scan, to, prd, \
	__FUNCTION__, __LINE__)
#define wlc_scan_del_timer(wlc, scan) _wlc_scan_del_timer(wlc, scan, \
	__FUNCTION__, __LINE__)
#else /* !DEBUG_SCAN_TIMER */
#define wlc_scan_add_timer(wlc, scan, to, prd) wl_add_timer(wlc->wl, scan->timer, to, prd)
#define wlc_scan_del_timer(wlc, scan) wl_del_timer(wlc->wl, scan->timer)
#endif /* !DEBUG_SCAN_TIMER */

#ifdef BCMDBG
#define SCAN_DBG_ENT	0x1
#define WL_SCAN_ENT(scan, x)	do {if ((scan)->debug & SCAN_DBG_ENT) printf x;} while (0)
#else /* !BCMDBG */
#define WL_SCAN_ENT(scan, x)
#endif /* !BCMDBG */

wlc_scan_info_t*
BCMATTACHFN(wlc_scan_attach)(wlc_info_t *wlc, void *wl, osl_t *osh, uint unit)
{
	scan_info_t *scan_info;
	watchdog_fn_t watchdog_fn = NULL;
	iovar_fn_t iovar_fn = NULL;
	const bcm_iovar_t *iovars = NULL;
	int	err = 0;

	uint	scan_info_size = (uint)sizeof(scan_info_t);
	uint	ssid_offs, chan_offs;

	ssid_offs = scan_info_size = ROUNDUP(scan_info_size, sizeof(uint32));
	scan_info_size += sizeof(wlc_ssid_t) * WLC_SCAN_NSSID_PREALLOC;
	chan_offs = scan_info_size = ROUNDUP(scan_info_size, sizeof(uint32));
	scan_info_size += sizeof(chan_scandata_t) * WLC_SCAN_NCHAN_PREALLOC;

	scan_info = (scan_info_t *)MALLOC(osh, scan_info_size);
	if (scan_info == NULL)
		return NULL;

	bzero((char*)scan_info, scan_info_size);

	scan_info->scan_pub = (struct wlc_scan_info *)MALLOC(osh, sizeof(struct wlc_scan_info));
	if (scan_info->scan_pub == NULL) {
		MFREE(osh, scan_info, scan_info_size);
		return NULL;
	}
	bzero((char*)scan_info->scan_pub, sizeof(struct wlc_scan_info));
	scan_info->scan_pub->scan_priv = (void *)scan_info;

	scan_info->memsize = scan_info_size;
	scan_info->wlc = wlc;
	scan_info->osh = osh;
	scan_info->unit = unit;

	scan_info->channel_idx = -1;
	scan_info->scan_pub->in_progress = FALSE;

	scan_info->defaults.assoc_time = WLC_SCAN_ASSOC_TIME;
	scan_info->defaults.unassoc_time = WLC_SCAN_UNASSOC_TIME;
	scan_info->defaults.home_time = WLC_SCAN_HOME_TIME;
	scan_info->defaults.passive_time = WLC_SCAN_PASSIVE_TIME;
	scan_info->defaults.nprobes = WLC_SCAN_NPROBES;
	scan_info->defaults.passive = FALSE;

	scan_info->timer = wl_init_timer((struct wl_info *)wl,
	                                 wlc_scantimer, scan_info, "scantimer");
	if (scan_info->timer == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer for scan timer failed\n", unit, __FUNCTION__));
		goto error;
	}

#ifdef WLSCANCACHE
	scan_info->sdb = wlc_scandb_create(osh, unit);
	if (scan_info->sdb == NULL) {
		WL_ERROR(("wl%d: %s: wlc_create_scandb failed\n", unit, __FUNCTION__));
		goto error;
	}
	scan_info->scan_pub->_scancache = TRUE;	/* enabled by default */
	watchdog_fn = wlc_scan_watchdog;
#endif /* WLSCANCACHE */

#ifdef WLC_SCAN_IOVARS
	iovar_fn = wlc_scan_doiovar;
	iovars = wlc_scan_iovars;
#endif /* WLC_SCAN_IOVARS */

	scan_info->ssid_prealloc = (wlc_ssid_t*)((uintptr)scan_info + ssid_offs);
	scan_info->nssid_prealloc = WLC_SCAN_NSSID_PREALLOC;
	scan_info->ssid_list = scan_info->ssid_prealloc;

#ifdef EXTENDED_SCAN
	scan_info->chan_prealloc = (chan_scandata_t*)((uintptr)scan_info + chan_offs);
	scan_info->nchan_prealloc = WLC_SCAN_NCHAN_PREALLOC;
	scan_info->chan_list = scan_info->chan_prealloc;
#else
	BCM_REFERENCE(chan_offs);
#endif /* EXTENDED_SCAN */

	err = wlc_module_register(wlc->pub, iovars, "scan", scan_info, iovar_fn,
	                          watchdog_fn, NULL, wlc_scan_down);
	if (err) {
		WL_ERROR(("wl%d: %s: wlc_module_register err=%d\n",
		          unit, __FUNCTION__, err));
		goto error;
	}

	err = wlc_module_add_ioctl_fn(wlc->pub, (void *)scan_info->scan_pub,
	                              (wlc_ioctl_fn_t)wlc_scan_ioctl, 0, NULL);
	if (err) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn err=%d\n",
		          unit, __FUNCTION__, err));
		goto error;
	}

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "scan", (dump_fn_t)wlc_scan_dump, (void *)scan_info);
#ifdef WLSCANCACHE
	wlc_dump_register(wlc->pub, "scancache", wlc_scandb_dump, scan_info->sdb);
#endif
#endif 

#ifdef BCMDBG
	/* scan_info->debug = SCAN_DBG_ENT; */
#endif

	return scan_info->scan_pub;

error:
	if (scan_info) {
		if (scan_info->timer != NULL)
			wl_free_timer(wl, scan_info->timer);
		wlc_scandb_free(scan_info->sdb);
		if (scan_info->scan_pub)
			MFREE(osh, scan_info->scan_pub, sizeof(struct wlc_scan_info));
		MFREE(osh, scan_info, scan_info_size);
	}

	return NULL;
}

static void
wlc_scan_ssidlist_free(scan_info_t *scan_info)
{
	if (scan_info->ssid_list != scan_info->ssid_prealloc) {
		MFREE(scan_info->wlc->osh, scan_info->ssid_list,
		      scan_info->nssid * sizeof(wlc_ssid_t));
		scan_info->ssid_list = scan_info->ssid_prealloc;
		scan_info->nssid = scan_info->nssid_prealloc;
	}
}

static int
BCMUNINITFN(wlc_scan_down)(void *hdl)
{
	scan_info_t *scan_info = (scan_info_t *)hdl;
	wlc_info_t *wlc = scan_info->wlc;
	int callbacks = 0;

	if (!wlc_scan_del_timer(wlc, scan_info))
		callbacks ++;

	scan_info->pass = WLC_SCAN_START;
	scan_info->channel_idx = -1;
	scan_info->scan_pub->in_progress = FALSE;
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_SCAN, FALSE);

	wlc_scan_ssidlist_free(scan_info);

	return callbacks;
}

void
BCMATTACHFN(wlc_scan_detach)(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc;

	if (scan_info) {
		wlc = scan_info->wlc;

		if (scan_info->timer) {
			wl_free_timer(wlc->wl, scan_info->timer);
			scan_info->timer = NULL;
		}

		wlc_scandb_free(scan_info->sdb);

		wlc_module_unregister(wlc->pub, "scan", scan_info);

		wlc_module_remove_ioctl_fn(wlc->pub, (void *)scan_info->scan_pub);

		ASSERT(scan_info->ssid_list == scan_info->ssid_prealloc);
		if (scan_info->ssid_list != scan_info->ssid_prealloc) {
			WL_ERROR(("wl%d: %s: ssid_list not set to prealloc\n",
			          wlc->pub->unit, __FUNCTION__));
		}
		if (scan_info->scan_pub)
			MFREE(wlc->osh, scan_info->scan_pub, sizeof(struct wlc_scan_info));
		MFREE(wlc->osh, scan_info, scan_info->memsize);
	}
	return;
}

#if defined(BCMDBG) || defined(WLMSG_INFORM)
static void
wlc_scan_print_ssids(wlc_ssid_t *ssid, int nssid)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	int linelen = 0;
	int len;
	int i;

	for (i = 0; i < nssid; i++) {
		len = wlc_format_ssid(ssidbuf, ssid[i].SSID, ssid[i].SSID_len);
		/* keep the line length under 80 cols */
		if (linelen + (len + 2) > 80) {
			printf("\n");
			linelen = 0;
		}
		printf("\"%s\" ", ssidbuf);
		linelen += len + 3;
	}
	printf("\n");
}
#endif /* BCMDBG || WLMSG_INFORM */

#define SCAN_USER(wlc, cfg) (cfg != NULL ? cfg : wlc->cfg)

bool
wlc_scan_in_scan_chanspec_list(wlc_scan_info_t *wlc_scan_info, chanspec_t chanspec)
{
	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	int i;
	uint8 chan;

	/* scan chanspec list not setup, return no match */
	if (scan_info->channel_idx == -1) {
		WL_INFORM(("scan chanspec list not setup, no match1\n"));
		return FALSE;
	}

	chan = wf_chspec_ctlchan(chanspec);
	for (i = 0; i < scan_info->channel_num; i++) {
		if (wf_chspec_ctlchan(scan_info->chanspec_list[i]) == chan) {
			return TRUE;
		}
	}

	return FALSE;
}

/* Caution: when passing a non-primary bsscfg to this function the caller
 * must make sure to abort the scan before freeing the bsscfg!
 */
int
wlc_scan(
	wlc_scan_info_t *wlc_scan_info,
	int bss_type,
	const struct ether_addr* bssid,
	int nssid,
	wlc_ssid_t *ssid,
	int scan_type,
	int nprobes,
	int active_time,
	int passive_time,
	int home_time,
	const chanspec_t* chanspec_list,
	int channel_num,
	chanspec_t chanspec_start,
	bool save_prb,
	scancb_fn_t fn, void* arg,
	int away_channels_limit, bool extdscan,
	bool suppress_ssid,
	bool include_cache,
	bool chan_prohibit,
	wlc_bsscfg_t *cfg,
	uint8 usage, actcb_fn_t act_cb, void *act_cb_arg)
{
	bool scan_in_progress;
	bool scan_timer_set;
	int i, num;

	scan_info_t *scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char *ssidbuf;
	char eabuf[ETHER_ADDR_STR_LEN];
#endif

	ASSERT(nssid);
	ASSERT(ssid != NULL);
	ASSERT(bss_type == DOT11_BSSTYPE_INFRASTRUCTURE ||
	       bss_type == DOT11_BSSTYPE_INDEPENDENT ||
	       bss_type == DOT11_BSSTYPE_ANY);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	ssidbuf = (char *) MALLOC(wlc->osh, SSID_FMT_BUF_LEN);

	if (nssid == 1) {
			WL_INFORM(("wl%d: %s: scanning for SSID \"%s\"\n", wlc->pub->unit,
				__FUNCTION__, ssidbuf ? (wlc_format_ssid(ssidbuf, ssid->SSID,
				ssid->SSID_len), ssidbuf) : "???"));
	} else {
		WL_INFORM(("wl%d: %s: scanning for SSIDs:\n", wlc->pub->unit, __FUNCTION__));
		if (WL_INFORM_ON())
			wlc_scan_print_ssids(ssid, nssid);
	}
	WL_INFORM(("wl%d: %s: scanning for BSSID \"%s\"\n", wlc->pub->unit, __FUNCTION__,
	           (bcm_ether_ntoa(bssid, eabuf), eabuf)));
#endif /* BCMDBG || WLMSG_INFORM */

	/* enforce valid argument */
	scan_info->ssid_wildcard_enabled = 0;
	for (i = 0; i < nssid; i++) {
		if (ssid[i].SSID_len > DOT11_MAX_SSID_LEN) {
			WL_ERROR(("wl%d: %s: invalid SSID len %d, capping\n",
			          wlc->pub->unit, __FUNCTION__, ssid[i].SSID_len));
			ssid[i].SSID_len = DOT11_MAX_SSID_LEN;
		}
		if (ssid[i].SSID_len == 0)
			scan_info->ssid_wildcard_enabled = 1;
	}

	scan_in_progress = SCAN_IN_PROGRESS(wlc_scan_info);
	scan_timer_set = (scan_info->pass == WLC_SCAN_ABORT ||
	                  (scan_in_progress &&
	                   0 == (wlc_scan_info->state &
	                         (SCAN_STATE_WSUSPEND | SCAN_STATE_PSPEND))));

	/* clear or set optional params to default */
	/* keep persistent scan suppress flag */
	wlc_scan_info->state &= SCAN_STATE_SUPPRESS;
	scan_info->nprobes = scan_info->defaults.nprobes;
	if (wlc->pub->associated) {
		scan_info->active_time = scan_info->defaults.assoc_time;
		scan_info->home_time = scan_info->defaults.home_time;
	} else {
		scan_info->active_time = scan_info->defaults.unassoc_time;
		scan_info->home_time = 0;
	}
	scan_info->passive_time = scan_info->defaults.passive_time;
	if (scan_info->defaults.passive)
		wlc_scan_info->state |= SCAN_STATE_PASSIVE;

	if (scan_type == DOT11_SCANTYPE_ACTIVE) {
		wlc_scan_info->state &= ~SCAN_STATE_PASSIVE;
	} else if (scan_type == DOT11_SCANTYPE_PASSIVE) {
		wlc_scan_info->state |= SCAN_STATE_PASSIVE;
	}
	/* passive scan always has nprobes to 1 */
	if (wlc_scan_info->state & SCAN_STATE_PASSIVE) {
		scan_info->nprobes = 1;
	}
	if (active_time > 0)
		scan_info->active_time = (uint16)active_time;

	if (passive_time >= 0)
		scan_info->passive_time = (uint16)passive_time;

	if (home_time >= 0 && wlc->pub->associated)
		scan_info->home_time = (uint16)home_time;
	if (nprobes > 0 && (wlc_scan_info->state & SCAN_STATE_PASSIVE) == 0)
		scan_info->nprobes = (uint8)nprobes;
	if (save_prb)
		wlc_scan_info->state |= SCAN_STATE_SAVE_PRB;
	if (include_cache && SCANCACHE_ENAB(wlc_scan_info))
		wlc_scan_info->state |= SCAN_STATE_INCLUDE_CACHE;

	WL_INFORM(("wl%d: %s: wlc_scan params: nprobes %d dwell active/passive %dms/%dms home %dms"
		" flags %d\n",
		wlc->pub->unit, __FUNCTION__, scan_info->nprobes, scan_info->active_time,
		scan_info->passive_time,
		scan_info->home_time, wlc_scan_info->state));

	wlc_scan_default_channels(wlc_scan_info, scan_info->chanspec_list,
	                          &scan_info->channel_num);

	if (chan_prohibit) {
		scan_info->scan_pub->state |= SCAN_STATE_PROHIBIT;
		num = wlc_scan_prohibited_channels((wlc_scan_info_t *)scan_info,
			&scan_info->chanspec_list[scan_info->channel_num],
			(MAXCHANNEL - scan_info->channel_num));
		scan_info->channel_num += num;
	} else
		scan_info->scan_pub->state &= ~SCAN_STATE_PROHIBIT;

	if (ISSIM_ENAB(wlc->pub->sih)) {
		/* QT hack: abort scan since full scan may take forever */
		scan_info->channel_num = 1;
	}

	/* set required and optional params */
	/* If IBSS Lock Out feature is turned on, set the scan type to BSS only */
	wlc_scan_info->bss_type =
		(wlc->ibss_allowed == FALSE)?DOT11_BSSTYPE_INFRASTRUCTURE:bss_type;
	bcopy((const char*)bssid, (char*)&wlc_scan_info->bssid, ETHER_ADDR_LEN);

	/* allocate memory for ssid list, using prealloc if sufficient */
	ASSERT(scan_info->ssid_list == scan_info->ssid_prealloc);
	if (scan_info->ssid_list != scan_info->ssid_prealloc) {
		WL_ERROR(("wl%d: %s: ssid_list not set to prealloc\n",
		          wlc->pub->unit, __FUNCTION__));
	}
	if (nssid > scan_info->nssid_prealloc) {
		scan_info->ssid_list = MALLOC(scan_info->wlc->osh,
		                              nssid * sizeof(wlc_ssid_t));
		/* failed, cap at prealloc (best effort) */
		if (scan_info->ssid_list == NULL) {
			nssid = scan_info->nssid_prealloc;
			scan_info->ssid_list = scan_info->ssid_prealloc;
		}
	}
	/* Now ssid_list is the right size for [current] nssid count */

	bcopy(ssid, scan_info->ssid_list, (sizeof(wlc_ssid_t) * nssid));
	scan_info->nssid = nssid;

#ifdef WLP2P
	if (P2P_ENAB(wlc->pub)) {
		scan_info->ssid_wildcard_enabled = FALSE;
		for (i = 0; i < nssid; i ++) {
			if (scan_info->ssid_list[i].SSID_len == 0)
				wlc_p2p_fixup_SSID(wlc->p2p, cfg, &scan_info->ssid_list[i]);
			if (scan_info->ssid_list[i].SSID_len == 0)
				scan_info->ssid_wildcard_enabled = TRUE;
		}
	}
#endif

	/* channel list validation */
	if (channel_num > MAXCHANNEL) {
		WL_ERROR(("wl%d: %s: wlc_scan bad param channel_num %d greater than max %d\n",
			wlc->pub->unit, __FUNCTION__, channel_num, MAXCHANNEL));
		channel_num = 0;
	}
	if (channel_num > 0 && chanspec_list == NULL) {
		WL_ERROR(("wl%d: %s: wlc_scan bad param channel_list was NULL with channel_num ="
			" %d\n",
			wlc->pub->unit, __FUNCTION__, channel_num));
		channel_num = 0;
	}
	for (i = 0; i < channel_num; i++) {
		if (chan_prohibit) {
			if (wlc_channel2freq(CHSPEC_CHANNEL(chanspec_list[i])) == 0)
				channel_num = 0;
		}
		else if (!wlc_valid_chanspec_db(wlc->cmi, chanspec_list[i])) {
			channel_num = 0;
		}
	}

	if (channel_num > 0) {
		for (i = 0; i < channel_num; i++)
			scan_info->chanspec_list[i] = chanspec_list[i];
		scan_info->channel_num = channel_num;
	}

#ifdef EXTENDED_SCAN
	/* no dynamic allocation for extended-scan chan_list yet, validate against prealloc */
	if (extdscan && (scan_info->channel_num > scan_info->nchan_prealloc)) {
		WL_ERROR(("wl%d: wlc_scan channel_num %d exceeds prealloc %d (arg %d)\n",
		          wlc->pub->unit, scan_info->channel_num,
		          scan_info->nchan_prealloc, channel_num));
		scan_info->channel_num = scan_info->nchan_prealloc;
	}
#endif /* EXTENDED_SCAN */

#ifdef BCMDBG
	if (WL_INFORM_ON()) {
		char chan_list_buf[128];
		struct bcmstrbuf b;

		bcm_binit(&b, chan_list_buf, sizeof(chan_list_buf));

		for (i = 0; i < scan_info->channel_num; i++) {
			bcm_bprintf(&b, " %d", CHSPEC_CHANNEL(scan_info->chanspec_list[i]));

			if ((i % 8) == 7 || (i + 1) == scan_info->channel_num) {
				WL_INFORM(("wl%d: wlc_scan: scan channels %s\n", wlc->pub->unit,
					chan_list_buf));
				bcm_binit(&b, chan_list_buf, sizeof(chan_list_buf));
			}
		}
	}
#endif /* BCMDBG */

	if ((wlc_scan_info->state & SCAN_STATE_SUPPRESS) || (!scan_info->channel_num)) {
		int status;

		WL_INFORM(("wl%d: %s: scan->state %d scan->channel_num %d\n",
			wlc->pub->unit, __FUNCTION__,
			wlc_scan_info->state, scan_info->channel_num));

		if (wlc_scan_info->state & SCAN_STATE_SUPPRESS)
			status = WLC_E_STATUS_SUPPRESS;
		else
			status = WLC_E_STATUS_NOCHANS;

		if (scan_in_progress)
			wlc_scan_abort(wlc_scan_info, status);

		/* no scan now, but free any earlier leftovers */
		wlc_bss_list_free(wlc, wlc->scan_results);

		if (fn != NULL)
			(fn)(arg, status, SCAN_USER(wlc, cfg));

		wlc_scan_ssidlist_free(scan_info);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
		if (ssidbuf != NULL)
			 MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif
		return BCME_EPERM;
	}

#ifdef STA
	if (!WLEXTSTA_ENAB(wlc->pub))
		if (scan_in_progress && !AS_IN_PROGRESS(wlc))
			wlc_scan_callback(wlc_scan_info, WLC_E_STATUS_ABORT);
#endif /* STA */

	scan_info->bsscfg = SCAN_USER(wlc, cfg);

	scan_info->cb = fn;
	scan_info->cb_arg = arg;

	scan_info->act_cb = act_cb != NULL ? act_cb : wlc_scan_sendprobe;
	scan_info->act_cb_arg = act_cb != NULL ? act_cb_arg : scan_info;

	wlc_scan_info->usage = (uint8)usage;

	/* start the scan with the results cleared */
	scan_info->away_channels_cnt = 0;
	if (!away_channels_limit)
		away_channels_limit = MAX(1, WLC_SCAN_AWAY_LIMIT / scan_info->active_time);

	scan_info->away_channels_limit = away_channels_limit;
	scan_info->extdscan = extdscan;


	/* extd scan for nssids one ssid per each pass..  */
	scan_info->npasses = (scan_info->extdscan && nssid) ? nssid : scan_info->nprobes;
	scan_info->channel_idx = 0;
	if (chanspec_start != 0) {
		for (i = 0; i < scan_info->channel_num; i++) {
			if (scan_info->chanspec_list[i] == chanspec_start) {
				scan_info->channel_idx = i;
				WL_INFORM(("starting new iscan on channel %d\n",
				           CHSPEC_CHANNEL(chanspec_start)));
				break;
			}
		}
	}

	/* Need to turn off BTCoex desense */
	if (wlc->btch->restage_rxgain_active) {
		if ((i = wlc_iovar_setint(wlc, "phy_btc_restage_rxgain", 0)) == BCME_OK) {
			wlc->btch->restage_rxgain_active = 0;
		}
		WL_ASSOC(("wl%d: BTC restage rxgain OFF for scan (err %d)\n", wlc->pub->unit, i));
	}

	wlc_scan_info->in_progress = TRUE;
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_SCAN, TRUE);

#ifdef WL_BCN_COALESCING
	wlc_bcn_clsg_disable(wlc->bc, BCN_CLSG_SCAN_MASK, BCN_CLSG_SCAN_MASK);
#endif /* WL_BCN_COALESCING */

#ifdef WLOFFLD
	if (WLOFFLD_BCN_ENAB(wlc->pub))
		wlc_ol_rx_deferral(wlc->ol, OL_SCAN_MASK, OL_SCAN_MASK);
#endif
	scan_info->pass = WLC_SCAN_START;
	/* ...and free any leftover responses from before */
	wlc_bss_list_free(wlc, wlc->scan_results);


	/* keep core awake to receive solicited probe responses, SCAN_IN_PROGRESS is TRUE */
	ASSERT(STAY_AWAKE(wlc));
	wlc_set_wake_ctrl(wlc);

	if (!scan_timer_set) {
		/* call wlc_scantimer to get the scan state machine going */
		/* DUALBAND - Don't call wlc_scantimer() directly from DPC... */
		wlc_scan_add_timer(wlc, scan_info, 0, 0);
	} else if (ACT_FRAME_IN_PROGRESS(wlc->scan)) {
		/* send out AF as soon as possible to aid reliability of GON */
		wlc_scan_del_timer(wlc, scan_info);
		wlc_scan_add_timer(wlc, scan_info, 0, 0);
	}

	if (wlc_scan_info->state & SCAN_STATE_PASSIVE)
		wlc->scan_results->beacon = TRUE;
	else
		wlc->scan_results->beacon = FALSE;

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	if (ssidbuf != NULL)
		 MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif
	/* if a scan is in progress, allow the next callback to restart the scan state machine */
	return BCME_OK;
}

void
wlc_scan_timer_update(wlc_scan_info_t *wlc_scan_info, uint32 ms)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;

	wlc_scan_del_timer(wlc, scan_info);
	wlc_scan_add_timer(wlc, scan_info, ms, 0);
}


/* abort the current scan, and return to home channel */
void
wlc_scan_abort(wlc_scan_info_t *wlc_scan_info, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
#if defined(WLP2P)
	wlc_bsscfg_t *scan_cfg = scan_info->bsscfg;
#endif 

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	WL_INFORM(("wl%d: %s: aborting scan in progress\n", wlc->pub->unit, __FUNCTION__));

	if (SCANCACHE_ENAB(wlc_scan_info) &&
#ifdef WLP2P
	    !BSS_P2P_DISC_ENAB(wlc, scan_cfg) &&
#endif
		TRUE) {
		wlc_scan_cache_result(scan_info);
	}

	wlc_bss_list_free(wlc, wlc->scan_results);
	wlc_scan_terminate(wlc_scan_info, status);

	if (N_ENAB(wlc->pub) && COEX_ENAB(wlc->pub))
		wlc_ht_obss_scan_update(scan_info, WLC_SCAN_ABORT);

}

void
wlc_scan_abort_ex(wlc_scan_info_t *wlc_scan_info, wlc_bsscfg_t *cfg, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	if (scan_info->bsscfg == cfg)
		wlc_scan_abort(wlc_scan_info, status);
}

void
wlc_scan_terminate(wlc_scan_info_t *wlc_scan_info, int status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	/* abort the current scan, and return to home channel */
	WL_INFORM(("wl%d: %s: terminating scan in progress\n", wlc->pub->unit, __FUNCTION__));

	/* return to home channel */
	wlc_scan_return_home_channel(wlc_scan_info);

#ifdef STA
	/* When ending scan, PM2 timer was likely off: if we're configured
	 * for PM2 and eligible (BSS associated) force timer restart.
	 */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->BSS && cfg->pm->PM == PM_FAST)
			wlc_pm2_sleep_ret_timer_start(cfg);
	}
#endif /* STA */

	/* clear scan ready flag */
	wlc_scan_info->state &= ~SCAN_STATE_READY;

	scan_info->pass = WLC_SCAN_ABORT;
	scan_info->channel_idx = -1;
	wlc_scan_info->in_progress = FALSE;
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_SCAN, FALSE);

#ifdef WL_BCN_COALESCING
	wlc_bcn_clsg_disable(wlc->bc, BCN_CLSG_SCAN_MASK, 0);
#endif /* WL_BCN_COALESCING */
#ifdef WLOFFLD
	if (WLOFFLD_BCN_ENAB(wlc->pub))
		wlc_ol_rx_deferral(wlc->ol, OL_SCAN_MASK, 0);
#endif
	wlc_scan_ssidlist_free(scan_info);

#ifdef STA
	wlc_set_wake_ctrl(wlc);
	WL_MPC(("wl%d: %s: SCAN_IN_PROGRESS==FALSE, update mpc\n", wlc->pub->unit, __FUNCTION__));
	wlc_radio_mpc_upd(wlc);
#endif /* STA */

	if (wlc_scan_info->state & SCAN_STATE_WSUSPEND)
		wlc_scan_del_timer(wlc, scan_info);

	/* check for states that indicate the scan timer is not scheduled */
	if (wlc_scan_info->state & (SCAN_STATE_WSUSPEND | SCAN_STATE_PSPEND)) {
		/* wlc_scantimer would have been scheduled by either a tx fifo
		 * suspend or PS indication, both of which we are canceling.
		 * Call wlc_scantimer directly to abort.
		 */
		wlc_scan_info->state &= ~(SCAN_STATE_WSUSPEND | SCAN_STATE_PSPEND);
		wlc_scantimer(scan_info);
	}

#ifdef STA
	wlc_scan_callback(wlc_scan_info, status);
#endif /* STA */
}

static void
wlc_scan_tx_suspend(wlc_info_t *wlc, wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	WL_SCAN(("wl%d: %s: suspending tx...\n", wlc->pub->unit, __FUNCTION__));
	/* suspend the data fifos to avoid sending
	 * data packets during scan
	 */
	wlc_tx_suspend(wlc);
	wlc_scan_info->state |= SCAN_STATE_WSUSPEND;
	wlc_scan_add_timer(wlc, scan_info, 10, 0);
}

static void
wlc_scan_prepare_tx(wlc_info_t *wlc, wlc_scan_info_t *wlc_scan_info)
{
	/* block any tx traffic */
	wlc->block_datafifo |= DATA_BLOCK_SCAN;
	if (!wlc_tx_suspended(wlc)) {
		/* If PSPEND completed immediately in the callback then it starts a timer
		 * if that's the case, then only set SUSPEND_REQ for wlc_scantimer to
		 * take care of
		 */
		if ((wlc_scan_info->state & (SCAN_STATE_PSPEND |
		                             SCAN_STATE_PSPEND_TIMER)) == 0) {
			wlc_scan_tx_suspend(wlc, wlc_scan_info);
		} else {
			/* wlc_scantimer() will do the wlc_tx_suspend() call
			 * when the PS announce completes
			 */
			wlc_scan_info->state |= SCAN_STATE_SUSPEND_REQ;
		}
	}
}

#ifdef STA
/* prepare to leave home channel */
static void
wlc_scan_prepare_off_channel(wlc_info_t *wlc)
{
	int idx;
	wlc_bsscfg_t *cfg;

	/* Must leave IBSS first before going away from home channel */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (!cfg->BSS)
			wlc_ibss_disable(cfg);
	}

	wlc_mhf(wlc, MHF2, MHF2_SKIP_CFP_UPDATE, MHF2_SKIP_CFP_UPDATE, WLC_BAND_ALL);
	wlc_skip_adjtsf(wlc, TRUE, NULL, WLC_SKIP_ADJTSF_SCAN, WLC_BAND_ALL);

	/* Must disable AP beacons and probe responses first before going away from home channel */
	wlc_ap_mute(wlc, TRUE, NULL, WLC_AP_MUTE_SCAN);
}

static void
wlc_scan_prepare_pm_mode(wlc_info_t *wlc, wlc_scan_info_t *wlc_scan_info)
{
	int idx;
	wlc_bsscfg_t *cfg;
#ifdef WLTDLS
	bool tdls_pmpending;
#endif

	bool quiet_channel = FALSE;

	if (WL11H_ENAB(wlc) &&
		wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC))
		quiet_channel = TRUE;

	if (wlc->exptime_cnt == 0 &&
	    !SCAN_TYPE_FOREGROUND((scan_info_t *)wlc_scan_info->scan_priv) && !quiet_channel) {
		/* set our PS callback flag so that wlc_scantimer is called
		 * when the PM State Null Data packet completes
		 */
		wlc_scan_info->state |= SCAN_STATE_PSPEND;
		/* announce PS mode to the AP if we are not already in PS mode */
		FOREACH_STA(wlc, idx, cfg) {
			/* block any PSPoll operations for holding off AP traffic */
			mboolset(cfg->pm->PMblocked, WLC_PM_BLOCK_SCAN);
			if (cfg->associated) {
				if (cfg->pm->PMenabled)
					continue;
#ifdef WLMCHAN
				/* For mchan operation, only enable PS for STA on our home */
				/* channel */
				if (MCHAN_ENAB(wlc->pub) && cfg->chan_context &&
					(cfg->chan_context->chanspec != wlc->home_chanspec)) {
					WL_MCHAN(("wl%d.%d: %s: skip pmpend for cfg on other"
						"channel\n",
						wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
					continue;
				}
#endif
				WL_SCAN(("wl%d.%d: wlc_scan_prepare_pm_mode: entering "
					"PM mode...\n",
					wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
				wlc_set_pmstate(cfg, TRUE);
			}
#ifdef WLTDLS
			else if (BSS_TDLS_ENAB(wlc, cfg) && !cfg->pm->PMenabled)
				wlc_tdls_notify_pm_state(wlc->tdls, cfg, TRUE);
#endif
		}
		/* We are supposed to wait for PM0->PM1 transition to finish but in case
		 * we failed to send PM indications or failed to receive ACKs fake a PM0->PM1
		 * transition so that anything depending on the transition to finish can move
		 * forward i.e. scan engine can continue.
		 * N.B.: to get wlc->PMpending updated in case all BSSs have done above
		 * fake PM0->PM1 transitions.
		 */
		wlc_pm_pending_complete(wlc);

#ifdef WLTDLS
		tdls_pmpending = FALSE;
		FOREACH_STA(wlc, idx, cfg) {
			if (BSS_TDLS_ENAB(wlc, cfg) && cfg->pm->PMpending) {
				tdls_pmpending = TRUE;
				break;
			}
		}
#endif

		/* clear our callback flag if PMpending is false, indicating
		 * that no PM State Null Data packet was sent by
		 * wlc_set_pmstate()
		 */
		if (!wlc->PMpending &&
#ifdef WLTDLS
			!tdls_pmpending &&
#endif
		    TRUE)
			wlc_scan_info->state &= ~SCAN_STATE_PSPEND;
	}
	wlc_set_wake_ctrl(wlc);
}
#endif /* STA */

static void
wlc_scantimer(void *arg)
{
	scan_info_t *scan_info = (scan_info_t *)arg;
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;
	wlc_info_t *wlc = scan_info->wlc;
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif /* STA */
	wlc_bsscfg_t *scan_cfg;
	chanspec_t chanspec;
	chanspec_t next_chanspec;
	int8 passes;
#if defined(BCMDBG) || defined(WLMSG_SCAN)
	uint32 tsf_l = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
#endif

	wlc_scan_info->state |= SCAN_STATE_IN_TMR_CB;

	WL_SCAN_ENT(scan_info, ("wl%d: %s: state 0x%x tsf %u\n",
	                        wlc->pub->unit, __FUNCTION__,
	                        wlc_scan_info->state, tsf_l));

	/* Store for later use */
	scan_cfg = scan_info->bsscfg;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		if (SCAN_IN_PROGRESS(wlc_scan_info)) {
			wlc_bss_list_free(wlc, wlc->scan_results);
			wlc_scan_callback(wlc_scan_info, WLC_E_STATUS_ABORT);

			/* Post a BSS event if an interface is attached to it */
			wlc_bss_mac_event(wlc, scan_cfg, WLC_E_SCAN_COMPLETE, NULL,
			                  WLC_E_STATUS_ABORT, 0, 0, NULL, 0);
		}
		wl_down(wlc->wl);
		goto exit;
	}

	/* got called back from the time so clear it */
	if ((wlc_scan_info->state & SCAN_STATE_PSPEND_TIMER) == SCAN_STATE_PSPEND_TIMER) {
		ASSERT((wlc_scan_info->state & SCAN_STATE_PSPEND) == 0);
		wlc_scan_info->state &= ~SCAN_STATE_PSPEND_TIMER;
	}

	if (scan_info->pass == WLC_SCAN_ABORT) {
		scan_info->pass = WLC_SCAN_START;
		goto exit;
	}

	if (!wlc->pub->up)
		goto exit;

	if (wlc_scan_info->state & SCAN_STATE_WSUSPEND) {
		WL_SCAN(("wl%d: %s: suspending tx timeout\n", wlc->pub->unit, __FUNCTION__));
		wlc_scan_info->state &= ~SCAN_STATE_WSUSPEND;
	}

	/* check if we are still waiting on an event to continue */
	if (wlc_scan_info->state & SCAN_STATE_SUSPEND_REQ) {
		wlc_scan_info->state &= ~SCAN_STATE_SUSPEND_REQ;
		wlc->block_datafifo |= DATA_BLOCK_SCAN;
		if (!wlc_tx_suspended(wlc)) {
			wlc_scan_tx_suspend(wlc, wlc_scan_info);
			goto exit;
		}
	}

	scan_info->pass++;
	chanspec = scan_info->chanspec_list[scan_info->channel_idx];
#ifdef STA
	wlc->iscan_chanspec_last = chanspec;
#endif
	passes = (wlc_quiet_chanspec(wlc->cmi, chanspec) ||
	          !wlc_valid_chanspec_db(wlc->cmi, chanspec)) ? 1 : scan_info->npasses;

	if (scan_info->pass > passes) {
		if (scan_info->pass == passes + 1) {
			/* scan passes complete for the current channel */
			WL_SCAN(("wl%d: %s: %sscanned channel %d, total responses %d, tsf %u\n",
			         wlc->pub->unit, __FUNCTION__,
			         ((wlc_quiet_chanspec(wlc->cmi, chanspec) &&
			           !(wlc_scan_info->state & SCAN_STATE_RADAR_CLEAR)) ?
			          "passively ":""),
			         CHSPEC_CHANNEL(chanspec), wlc->scan_results->count,
			         tsf_l));

			/* reset the radar clear flag since we will be leaving the channel */
			wlc_scan_info->state &= ~SCAN_STATE_RADAR_CLEAR;
		}

		if (scan_info->channel_idx < scan_info->channel_num - 1)
			next_chanspec = scan_info->chanspec_list[scan_info->channel_idx + 1];
		else
			next_chanspec = INVCHANSPEC;

		/* keep track of the number of channels scanned since the last
		 * time we returned to the home channel
		 */
		if (wlc_scan_info->state & SCAN_STATE_READY)
			scan_info->away_channels_cnt++;
		else
			scan_info->away_channels_cnt = 0;

		/* If the home_time is non-zero,
		 * and there are more channels to scan,
		 * and we reached the away channel limit or the channel
		 *	we just scanned or are about to scan is a passive scan channel,
		 * return to the home channel before scanning the next channel
		 */
		if (scan_info->pass == (passes + 1) &&
		    scan_info->home_time > 0 &&
		    next_chanspec != INVCHANSPEC &&
		    (scan_info->away_channels_cnt >= scan_info->away_channels_limit ||
		     wlc_quiet_chanspec(wlc->cmi, chanspec) ||
		     wlc_quiet_chanspec(wlc->cmi, next_chanspec))) {
			/* return to home channel */
			wlc_scan_return_home_channel(wlc_scan_info);

			/* clear scan ready flag */
			wlc_scan_info->state &= ~SCAN_STATE_READY;

			/* Allow normal traffic before next channel scan */
			wlc_scan_add_timer(wlc, scan_info, scan_info->home_time, 0);
			goto exit;
		}

		/* if there are more channels to scan ... */
		if (next_chanspec != INVCHANSPEC) {
			/* ... continue scanning */
			scan_info->channel_idx++;
			chanspec = next_chanspec;
			scan_info->pass = WLC_SCAN_CHANNEL_PREP;
		}
		/* ... otherwise the scan is done,
		 * scan.pass > passes and we fall through to the end of this function
		 */
	}

	if (scan_info->pass == WLC_SCAN_CHANNEL_PREP) {
		/* do off-home-channel setup if we are on the home channel */
		if (!(wlc_scan_info->state & SCAN_STATE_READY) &&
		    (WLC_BAND_PI_RADIO_CHANSPEC == wlc->home_chanspec)) {
#ifdef STA
			wlc_scan_prepare_off_channel(wlc);

			wlc_scan_prepare_pm_mode(wlc, wlc_scan_info);
#endif /* STA */

			if (wlc->pub->associated) {
				wlc_scan_prepare_tx(wlc, wlc_scan_info);
				/* if there is any prep work, return here and let wlc_scantimer be
				 * called back
				 */
				if (wlc_scan_info->state &
				    (SCAN_STATE_PSPEND | SCAN_STATE_WSUSPEND))
					goto exit;
			}
		}
		scan_info->pass = 1;
	}

	/* scan the channel */
	if (scan_info->pass >= 1 && scan_info->pass <= passes) {

		if (wf_chspec_ctlchan(chanspec) != wf_chspec_ctlchan(WLC_BAND_PI_RADIO_CHANSPEC)) {
			wlc_suspend_mac_and_wait(wlc);
#ifdef STA
			/* Must leave IBSS/AP first before going to away channel
			 * This is needed as driver could have come back to home channel and
			 * renabled IBSS/AP
			 */
			wlc_scan_prepare_off_channel(wlc);
#endif /* STA */
			/* suspend normal tx queue operation for channel excursion */
			wlc_excursion_start(wlc);

			wlc_set_chanspec(wlc, chanspec);
			wlc_enable_mac(wlc);

			WL_SCAN(("wl%d: %s: switched to channel %d tsf %u\n",
			         wlc->pub->unit, __FUNCTION__, CHSPEC_CHANNEL(chanspec),
			         tsf_l));
		}

		wlc_scan_do_pass(scan_info, chanspec);
		goto exit;
	}

	/*
	 * wraps up scan process.
	 */

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	if (scan_info->nssid == 1) {
		char ssidbuf[SSID_FMT_BUF_LEN];
		wlc_ssid_t *ssid = scan_info->ssid_list;

		if (WL_INFORM_ON())
			wlc_format_ssid(ssidbuf, ssid->SSID, ssid->SSID_len);
		WL_INFORM(("wl%d: %s: %s scan done, %d total responses for SSID \"%s\"\n",
		           wlc->pub->unit, __FUNCTION__,
		           (wlc_scan_info->state & SCAN_STATE_PASSIVE) ? "Passive" : "Active",
		           wlc->scan_results->count, ssidbuf));
	} else {
		WL_INFORM(("wl%d: %s: %s scan done, %d total responses for SSIDs:\n",
		           wlc->pub->unit, __FUNCTION__,
		           (wlc_scan_info->state & SCAN_STATE_PASSIVE) ? "Passive" : "Active",
		           wlc->scan_results->count));
		if (WL_INFORM_ON())
			wlc_scan_print_ssids(scan_info->ssid_list, scan_info->nssid);
	}
#endif /* BCMDBG || WLMSG_INFORM */

	/* return to home channel */
	wlc_scan_return_home_channel(wlc_scan_info);

	wlc_scan_info->state &= ~SCAN_STATE_READY;
	scan_info->channel_idx = -1;
	wlc_scan_info->in_progress = FALSE;
	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_SCAN, FALSE);

#ifdef WL_BCN_COALESCING
	wlc_bcn_clsg_disable(wlc->bc, BCN_CLSG_SCAN_MASK, 0);
#endif /* WL_BCN_COALESCING */
#ifdef WLOFFLD
	if (WLOFFLD_BCN_ENAB(wlc->pub))
		wlc_ol_rx_deferral(wlc->ol, OL_SCAN_MASK, 0);
#endif
	wlc_scan_ssidlist_free(scan_info);

	/* allow core to sleep again (no more solicited probe responses) */
	wlc_set_wake_ctrl(wlc);

#ifdef WLCQ
	/* resume any channel quality measurement */
	if (wlc->channel_qa_active)
		wlc_lq_channel_qa_sample_req(wlc);
#endif /* WLCQ */

#ifdef STA
	/* disable radio for non-association scan.
	 * Association scan will continue with JOIN process and
	 * end up at MACEVENT: WLC_E_SET_SSID
	 */
	WL_MPC(("wl%d: scan done, SCAN_IN_PROGRESS==FALSE, update mpc\n", wlc->pub->unit));
	wlc_radio_mpc_upd(wlc);
	wlc_radio_upd(wlc); /* Bring down the radio immediately */
#ifdef WL11D
	/* If we are in 802.11D mode and we are still waiting to find a
	 * valid Country IE, then take this opportunity to parse these
	 * scan results for one.
	 */
	if (WLC_AUTOCOUNTRY_ENAB(wlc))
		wlc_11d_scan_complete(wlc->m11d, WLC_E_STATUS_SUCCESS);
#endif /* WL11D */
#endif /* STA */

	if (N_ENAB(wlc->pub) && COEX_ENAB(wlc->pub))
		wlc_ht_obss_scan_update(scan_info, WLC_SCAN_SUCCESS);

	/* Don't fill the cache with results from a P2P discovery scan since these entries
	 * are short-lived. Also, a P2P association cannot use Scan cache
	 */
	if (SCANCACHE_ENAB(wlc_scan_info) &&
#ifdef WLP2P
	    !BSS_P2P_DISC_ENAB(wlc, scan_cfg) &&
#endif
	    TRUE) {
		wlc_scan_cache_result(scan_info);

#if defined(NDIS) && (NDISVER == 0x0620)
		/* check wakeup fast channels scan results */
		if (wlc_scan_info->state & SCAN_STATE_INCLUDE_CACHE &&
			wl_fast_scan_enabled(wlc->wl, NULL) && wlc->wakeup_scan)
			wl_fast_scan_result_search(wlc->wl, wlc->scan_results);
#endif /* (NDIS) && (NDISVER == 0x0620) */
	}

	wlc_scan_callback(wlc_scan_info, WLC_E_STATUS_SUCCESS);

#ifdef STA
	/* if this was a broadcast scan across all channels, update the roam cache, if possible */
	if (ETHER_ISBCAST(&wlc_scan_info->bssid) &&
	    wlc_scan_info->bss_type == DOT11_BSSTYPE_ANY) {
		FOREACH_AS_STA(wlc, idx, cfg) {
			wlc_roam_t *roam = cfg->roam;
			if (roam->roam_scan_piggyback && roam->active && !roam->fullscan_count) {
				WL_ASSOC(("wl%d: %s: Building roam cache with scan results from "
				          "broadcast scan\n", wlc->pub->unit, __FUNCTION__));
				/* this counts as a full scan */
				roam->fullscan_count = 1;
				/* update the roam cache */
				wlc_build_roam_cache(cfg, wlc->scan_results);
			}
		}
	}
#endif /* STA */

	wlc_bss_list_free(wlc, wlc->scan_results);

#if defined(MACOSX)
	/* scan completion event */
		/* Post a BSS event if an interface is attached to it */
		wlc_bss_mac_event(wlc, scan_cfg, WLC_E_SCAN_COMPLETE, NULL,
		                  WLC_E_STATUS_SUCCESS, 0, 0, NULL, 0);
#endif 

#ifdef STA
	/* If in PM2 and associated and not in PS mode, start the return to sleep
	 * timer to make sure we eventually go back to power save mode.
	 */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (cfg->BSS && cfg->pm->PM == PM_FAST && !cfg->pm->PMenabled) {
			WL_RTDC(wlc, "wlc_scantimer end: start FRTS timer", 0, 0);
			wlc_pm2_sleep_ret_timer_start(cfg);
		}
	}
#endif /* STA */

exit:
	wlc_scan_info->state &= ~SCAN_STATE_IN_TMR_CB;
}

static void
wlc_scan_return_home_channel(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
#ifdef STA
	int idx;
	wlc_bsscfg_t *cfg;
#endif
#if defined(BCMDBG) || defined(WLMSG_SCAN)
	char chanbuf[CHANSPEC_STR_LEN];
#endif

	wlc_suspend_mac_and_wait(wlc);

	/* resume normal tx queue operation */
	wlc_excursion_end(wlc);

	wlc_set_chanspec(wlc, wlc->home_chanspec);
	WL_SCAN(("wl%d: %s: switched to home_chanspec %s\n", wlc->pub->unit, __FUNCTION__,
		wf_chspec_ntoa(wlc->home_chanspec, chanbuf)));

#ifdef STA
	/* Return to IBSS and re-enable AP */
	FOREACH_AS_STA(wlc, idx, cfg) {
		if (!cfg->BSS)
			wlc_ibss_enable(cfg);
	}
	if (AP_ACTIVE(wlc)) {
		/* validate the phytxctl for the beacon before turning it on */
		wlc_validate_bcn_phytxctl(wlc, NULL);
	}
	wlc_ap_mute(wlc, FALSE, NULL, WLC_AP_MUTE_SCAN);

	/* enable CFP and TSF update */
	wlc_mhf(wlc, MHF2, MHF2_SKIP_CFP_UPDATE, 0, WLC_BAND_ALL);
	wlc_skip_adjtsf(wlc, FALSE, NULL, WLC_SKIP_ADJTSF_SCAN, WLC_BAND_ALL);
#endif /* STA */

	/* Restore promisc behavior for beacons and probes */
	wlc->bcnmisc_scan = FALSE;
	wlc_mac_bcn_promisc(wlc);

	wlc_enable_mac(wlc);

	/* Clear the tssi values */
	wlc_phy_clear_tssi(wlc->band->pi);

	/* un-suspend the DATA fifo now that we are back on the home channel */
	wlc_tx_resume(wlc);

	/* re-enable txq processing now that we are back on the home channel */
	wlc->block_datafifo &= ~DATA_BLOCK_SCAN;

#ifdef STA
	/* un-block PSPoll operations and restore PS state */
	FOREACH_STA(wlc, idx, cfg) {
		mboolclr(cfg->pm->PMblocked, WLC_PM_BLOCK_SCAN);
		/* come out of PS mode if appropriate */
		if (cfg->associated) {
			if ((cfg->pm->PM != PM_MAX || cfg->pm->WME_PM_blocked) &&
				cfg->pm->PMenabled) {
#ifdef WLMCHAN
				/* For mchan operation, only disable PS for STA on our home */
				/* channel */
				if (MCHAN_ENAB(wlc->pub) && cfg->chan_context &&
					(cfg->chan_context->chanspec != wlc->home_chanspec)) {
					WL_MCHAN(("wl%d.%d: %s: skip pmclr for cfg on other "
						"channel\n",
						wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
					continue;
				}
#endif
				WL_RTDC(wlc, "wlc_scan_return_home_channel: exit PS", 0, 0);
				wlc_set_pmstate(cfg, FALSE);
			}
		}
#ifdef WLTDLS
		else if (BSS_TDLS_ENAB(wlc, cfg) && cfg->pm->PMenabled)
			wlc_tdls_notify_pm_state(wlc->tdls, cfg, FALSE);
#endif
	}
	wlc_set_wake_ctrl(wlc);
#endif /* STA */

	/* run txq if not empty */
	if (!pktq_empty(&wlc->active_queue->q))
		wlc_send_q(wlc, wlc->active_queue);
}

void
wlc_scan_radar_clear(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t	*scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	wlc_info_t	*wlc = scan_info->wlc;
	uint32		channel_dwelltime;

	uint32 cur_l, cur_h;
	uint32 elapsed_time, remaining_time, active_time;

	/* if we are not on a radar quiet channel,
	 * or a passive scan was requested,
	 * or we already processed the radar clear signal,
	 * or it is not a prohibited channel,
	 * then do nothing
	 */
	if ((wlc_valid_chanspec_db(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) &&
	     !wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC)) ||
	    (wlc_scan_info->state & (SCAN_STATE_PASSIVE | SCAN_STATE_RADAR_CLEAR)))
		return;

	/* if we are not in the channel scan portion of the scan, do nothing */
	if (scan_info->pass != 1)
		return;

	/* if there is not enough time remaining for a probe,
	 * do nothing unless explicitly enabled
	 */
	if (scan_info->force_active == FALSE) {
		wlc_read_tsf(wlc, &cur_l, &cur_h);

		elapsed_time = (cur_l - scan_info->start_tsf) / 1000;

		channel_dwelltime = CHANNEL_PASSIVE_DWELLTIME(scan_info);

		if (elapsed_time > channel_dwelltime)
			remaining_time = 0;
		else
			remaining_time = channel_dwelltime - elapsed_time;

		if (remaining_time < WLC_SCAN_MIN_PROBE_TIME)
			return;

		active_time = MIN(remaining_time, CHANNEL_ACTIVE_DWELLTIME(scan_info));
	}
	else
		active_time = CHANNEL_ACTIVE_DWELLTIME(scan_info);

	/* everything is ok to switch to an active scan */
	wlc_scan_info->state |= SCAN_STATE_RADAR_CLEAR;

	wlc_scan_del_timer(wlc, scan_info);
	wlc_scan_add_timer(wlc, scan_info, active_time, 0);

	wlc_mute(wlc, OFF, 0);

	(scan_info->act_cb)(wlc, scan_info->act_cb_arg);

	WL_REGULATORY(("wl%d: wlc_scan_radar_clear: rcvd beacon on radar channel %d,"
		" converting to active scan, %d ms left\n",
		wlc->pub->unit, CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC), active_time));
}

#ifdef BCMDBG
static void
print_valid_channel_error(wlc_info_t	*wlc, chanspec_t chspec)
{
	uint8 channel = CHSPEC_CHANNEL(chspec);
	WL_ERROR(("CHSPEC_CHANNEL(chspec)=%x\n", CHSPEC_CHANNEL(chspec)));

	if (CHANNEL_BANDUNIT(wlc, CHSPEC_CHANNEL(chspec)) !=
		CHSPEC_WLCBANDUNIT(chspec)) {
			WL_ERROR(("CHANNEL_BANDUNIT(wlc, CHSPEC_CHANNEL(chspec))=%x\n",
			CHANNEL_BANDUNIT(wlc->cmi->wlc, CHSPEC_CHANNEL(chspec))));
		return;
	}

	/* Check a 20Mhz channel -- always assumed to be dual-band */
	if (CHSPEC_IS20(chspec)) {
		if (!VALID_CHANNEL20_DB(wlc, chspec)) {
			WL_ERROR(("VALID_CHANNEL20_DB = %d\n",
			VALID_CHANNEL20_DB(wlc, chspec)));
		} else {
			WL_ERROR(("%s: no error found\n", __FUNCTION__));
		}
		return;
	} else if (CHSPEC_IS40(chspec)) {
		/* Check a 40Mhz channel */
		if (!wlc->pub->phy_bw40_capable) {
			WL_ERROR(("phy not bw40 capable\n"));
			return;
		}

		if (!VALID_40CHANSPEC_IN_BAND(wlc, CHSPEC_WLCBANDUNIT(chspec))) {
			WL_ERROR(("!VALID_40CHANSPEC_IN_BAND(%p, %d)\n", wlc, chspec));
			return;
		}
		if (!VALID_CHANNEL20_DB(wlc, LOWER_20_SB(channel)) ||
			!VALID_CHANNEL20_DB(wlc, UPPER_20_SB(channel))) {
			WL_ERROR(("dual bands not both valid = [%x, %x]\n",
				LOWER_20_SB(channel), UPPER_20_SB(channel)));
			return;
		}

		/* check that the lower sideband allows an upper sideband */
			WL_ERROR(("%s: lower sideband not allow upper one OR error not found\n",
				__FUNCTION__));

	} else if (CHSPEC_IS80(chspec)) {
		/* Check a 80MHz channel - only 5G band supports 80MHz */

		chanspec_t chspec40;

		/* Only 5G supports 80MHz
		 * Check the chanspec band with BAND_5G() instead of the more straightforward
		 * CHSPEC_IS5G() since BAND_5G() is conditionally compiled on BAND5G support. This
		 * check will turn into a constant check when compiling without BAND5G support.
		 */
		if (!BAND_5G(CHSPEC2WLC_BAND(chspec))) {
			WL_ERROR(("band not 5g for 80MHz\n"));
			return;
		}

		/* Make sure that the phy is 80MHz capable and that
		 * we are configured for 80MHz on the band
		 */
		if (!wlc->pub->phy_bw80_capable ||
		    !WL_BW_CAP_80MHZ(wlc->bandstate[BAND_5G_INDEX]->bw_cap)) {
			WL_ERROR(("!phy_bw80_capable (%x) || !mimo_cap_80 (%x)\n",
				!wlc->pub->phy_bw80_capable,
			          !WL_BW_CAP_80MHZ(wlc->bandstate[BAND_5G_INDEX]->bw_cap)));
			return;
		}
		/* Check that the 80MHz center channel is a defined channel */
		/* Make sure both 40 MHz side channels are valid
		 * Create a chanspec for each 40MHz side side band and check
		 */
		chspec40 = (chanspec_t)((channel - CH_20MHZ_APART) |
			WL_CHANSPEC_CTL_SB_L |
			WL_CHANSPEC_BW_40 |
			WL_CHANSPEC_BAND_5G);

		if (!wlc_valid_chanspec_db(wlc->cmi, chspec40)) {
			WL_ERROR(("wl%d: %s: 80MHz: chanspec %0X -> chspec40 %0X "
					"failed valid check\n",
					wlc->pub->unit, __FUNCTION__, chspec, chspec40));

			return;
		}
		chspec40 = (chanspec_t)((channel + CH_20MHZ_APART) |
			WL_CHANSPEC_CTL_SB_L |
			WL_CHANSPEC_BW_40 |
			WL_CHANSPEC_BAND_5G);

		if (!wlc_valid_chanspec_db(wlc->cmi, chspec40)) {
			WL_ERROR(("wl%d: %s: 80MHz: chanspec %0X -> chspec40 %0X "
					"failed valid check\n",
					wlc->pub->unit, __FUNCTION__, chspec, chspec40));
			return;
		}
		WL_ERROR(("%s: err not found or 80MHz has no channel %d\n", __FUNCTION__, channel));
		return;
	}
}
#endif /* BCMDBG */

void
wlc_scan_default_channels(wlc_scan_info_t *wlc_scan_info, chanspec_t *chanspec_list,
	int *channel_count)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t	*wlc = scan_info->wlc;
	chanspec_t chanspec_start;

	int num;

	/* start the scan on the current home channel */
	chanspec_start = wf_chspec_ctlchspec(wlc->home_chanspec);

#ifdef BCMDBG
	if (!wlc_valid_chanspec_db(wlc->cmi, chanspec_start)) {
		WL_ERROR(("wlc_valid_chanspec_db(%p, %x)==FALSE\n",
			wlc->cmi, chanspec_start));
		print_valid_channel_error(wlc, chanspec_start);
	}
#endif
	ASSERT(wlc_valid_chanspec_db(wlc->cmi, chanspec_start));

	/* enumerate all the active (non-quiet) channels first */
	wlc_scan_channels(wlc, chanspec_list, &num, MAXCHANNEL,
		chanspec_start, CHAN_TYPE_CHATTY);
	*channel_count = num;

	/* if scan_info->passive_time = 0, skip the passive channels */
	if (!scan_info->passive_time)
		return;

	/* enumerate all the passive (quiet) channels second */
	wlc_scan_channels(wlc, &chanspec_list[num], &num,
		(MAXCHANNEL - *channel_count), chanspec_start, CHAN_TYPE_QUIET);
	*channel_count += num;
}

/*
 * Scan channels are always 20MHZ, so return the valid set of 20MHZ channels for this locale.
 */
static void
wlc_scan_channels(wlc_info_t *wlc, chanspec_t *chanspec_list,
	int *pchannel_num, int channel_max,
	chanspec_t chanspec_start, int channel_type)
{
	uint bandunit;
	uint channel;
	chanspec_t chanspec;
	int num = 0;
	uint i;

	/* chanspec start should be for a 20MHZ channel */
	ASSERT(CHSPEC_IS20(chanspec_start));
	bandunit = CHSPEC_WLCBANDUNIT(chanspec_start);
	for (i = 0; i < NBANDS(wlc); i++) {
		channel = CHSPEC_CHANNEL(chanspec_start);
		chanspec = CH20MHZ_CHSPEC(channel);
		while (num < channel_max) {
			if (VALID_CHANNEL20_IN_BAND(wlc, bandunit, channel) &&
			    ((channel_type == CHAN_TYPE_CHATTY &&
				!wlc_quiet_chanspec(wlc->cmi, chanspec)) ||
			     (channel_type == CHAN_TYPE_QUIET &&
				wlc_quiet_chanspec(wlc->cmi, chanspec))))
					chanspec_list[num++] = chanspec;
			channel = (channel + 1) % MAXCHANNEL;
			chanspec = CH20MHZ_CHSPEC(channel);
			if (chanspec == chanspec_start)
				break;
		}

		/* only find channels for one band */
		if (!IS_MBAND_UNLOCKED(wlc))
			break;

		/* prepare to find the other band's channels */
		bandunit = ((bandunit == 1) ? 0 : 1);
		chanspec_start = CH20MHZ_CHSPEC(0);
	}

	*pchannel_num = num;
}

static uint
wlc_scan_prohibited_channels(wlc_scan_info_t *wlc_scan_info, chanspec_t *chanspec_list,
	int channel_max)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info;
	wlc_info_t *wlc = scan_info->wlc;
	wlcband_t *band;
	uint channel, maxchannel, i, j;
	chanvec_t sup_chanvec, chanvec;
	int num = 0;

	if (!WLC_AUTOCOUNTRY_ENAB(wlc))
		return 0;

	band = wlc->band;
	for (i = 0; i < NBANDS(wlc); i++, band = wlc->bandstate[OTHERBANDUNIT(wlc)]) {
		const char *acdef = wlc_11d_get_autocountry_default(wlc->m11d);

		bzero(&sup_chanvec, sizeof(chanvec_t));
		/* Get the list of all the channels in autocountry_default
		 * and supported by phy
		 */
		wlc_phy_chanspec_band_validch(band->pi, band->bandtype, &sup_chanvec);
		if (!wlc_channel_get_chanvec(wlc, acdef, band->bandtype, &chanvec))
			return 0;

		for (j = 0; j < sizeof(chanvec_t); j++)
			sup_chanvec.vec[j] &= chanvec.vec[j];

		maxchannel = BAND_2G(band->bandtype) ? (CH_MAX_2G_CHANNEL + 1) : MAXCHANNEL;
		for (channel = 0; channel < maxchannel; channel++) {
			if (isset(sup_chanvec.vec, channel) &&
			    !VALID_CHANNEL20_IN_BAND(wlc, band->bandunit, channel)) {
				chanspec_list[num++] = CH20MHZ_CHSPEC(channel);
				if (num >= channel_max)
					return num;
			}
		}
	}

	return num;
}

bool
wlc_scan_inprog(wlc_info_t *wlc_info)
{
	return SCAN_IN_PROGRESS(wlc_info->scan);
}

void
wlc_scan_fifo_suspend_complete(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	WL_SCAN_ENT(scan_info, ("wl%d: %s: state 0x%x tsf %u\n",
	                        wlc->pub->unit, __FUNCTION__,
	                        wlc_scan_info->state,
	                        R_REG(wlc->osh, &wlc->regs->tsf_timerlow)));

	if (!(wlc_scan_info->state & SCAN_STATE_WSUSPEND))
		return;

	wlc_scan_info->state &= ~SCAN_STATE_WSUSPEND;
	wlc_scan_info->state |= SCAN_STATE_READY;
	wlc_scan_del_timer(wlc, scan_info);
	wlc_scan_add_timer(wlc, scan_info, 0, 0);
}

void
wlc_scan_pm_pending_complete(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;

	if (!SCAN_IN_PROGRESS(wlc_scan_info))
		return;

	WL_SCAN_ENT(scan_info, ("wl%d: %s: state 0x%x tsf %u\n",
	                        wlc->pub->unit, __FUNCTION__,
	                        wlc_scan_info->state,
	                        R_REG(wlc->osh, &wlc->regs->tsf_timerlow)));

	if (wlc_scan_info->state & SCAN_STATE_IN_TMR_CB)
		return;

	if (!(wlc_scan_info->state & SCAN_STATE_PSPEND))
		return;

	if (wlc->PMpending ||
#if defined(EXTENDED_SCAN) && defined(BCMDBG)
	    scan_info->test_nopsack ||
#endif
	    FALSE) {
		WL_INFORM(("wl%d: No ACK for PS null frame\n", wlc->pub->unit));
		/* abort scan if background scan and PS didn't make it to AP */
		if ((SCAN_TYPE_BACKGROUND(scan_info))) {
			WL_ERROR(("Aborting the scan\n"));
			wlc_scan_abort(wlc_scan_info, WLC_E_STATUS_ABORT);
			return;
		}
	}
	wlc_scan_info->state &= ~SCAN_STATE_PSPEND;
	wlc_scan_info->state |= SCAN_STATE_PSPEND_TIMER;
	/* delay moving to off channel by pm2_radio_shutoff_dly if configured */
	wlc_scan_add_timer(wlc, scan_info, wlc->pm2_radio_shutoff_dly, 0);
}

static void
wlc_scan_callback(wlc_scan_info_t *wlc_scan_info, uint status)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	scancb_fn_t cb = scan_info->cb;
	void *cb_arg = scan_info->cb_arg;
	wlc_bsscfg_t *cfg = scan_info->bsscfg;

	scan_info->bsscfg = NULL;

	scan_info->cb = NULL;
	scan_info->cb_arg = NULL;

	if (cb != NULL)
		(cb)(cb_arg, status, cfg);

	/* reset scan engine usage */
	wlc_scan_info->usage = SCAN_ENGINE_USAGE_NORM;
}

chanspec_t
wlc_scan_get_current_chanspec(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;

	return scan_info->chanspec_list[scan_info->channel_idx];
}

int
wlc_scan_ioctl(wlc_scan_info_t *wlc_scan_info,
	int cmd, void *arg, int len, struct wlc_if *wlcif)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;
	int bcmerror = 0;
	int val = 0, *pval;
	bool bool_val;

	/* default argument is generic integer */
	pval = (int *) arg;
	/* This will prevent the misaligned access */
	if (pval && (uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));

	/* bool conversion to avoid duplication below */
	bool_val = (val != 0);
	BCM_REFERENCE(bool_val);

	switch (cmd) {
#ifdef STA
	case WLC_SET_PASSIVE_SCAN:
		scan_info->defaults.passive = (bool_val ? 1 : 0);
		break;

	case WLC_GET_PASSIVE_SCAN:
		ASSERT(pval != NULL);
		if (pval != NULL)
			*pval = scan_info->defaults.passive;
		else
			bcmerror = BCME_BADARG;
		break;

	case WLC_GET_SCANSUPPRESS:
		ASSERT(pval != NULL);
		if (pval != NULL)
			*pval = wlc_scan_info->state & SCAN_STATE_SUPPRESS ? 1 : 0;
		else
			bcmerror = BCME_BADARG;
		break;

	case WLC_SET_SCANSUPPRESS:
		if (val) {
			wlc_scan_info->state |= SCAN_STATE_SUPPRESS;
		} else
			wlc_scan_info->state &= ~SCAN_STATE_SUPPRESS;
		break;

	case WLC_GET_SCAN_CHANNEL_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_assoc_time", NULL, 0, arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCAN_CHANNEL_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_assoc_time", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	case WLC_GET_SCAN_UNASSOC_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_unassoc_time", NULL, 0, arg, len, IOV_GET,
			wlcif);
		break;

	case WLC_SET_SCAN_UNASSOC_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_unassoc_time", NULL, 0, arg, len, IOV_SET,
			wlcif);
		break;
#endif /* STA */

	case WLC_GET_SCAN_PASSIVE_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_passive_time", NULL, 0, arg, len, IOV_GET,
			wlcif);
		break;

	case WLC_SET_SCAN_PASSIVE_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_passive_time", NULL, 0, arg, len, IOV_SET,
			wlcif);
		break;

	case WLC_GET_SCAN_HOME_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_home_time", NULL, 0, arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCAN_HOME_TIME:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_home_time", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	case WLC_GET_SCAN_NPROBES:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_nprobes", NULL, 0, arg, len, IOV_GET, wlcif);
		break;

	case WLC_SET_SCAN_NPROBES:
		ASSERT(arg != NULL);
		bcmerror = wlc_iovar_op(wlc, "scan_nprobes", NULL, 0, arg, len, IOV_SET, wlcif);
		break;
	default:
		bcmerror = BCME_UNSUPPORTED;
		break;
	}
	return bcmerror;
}

#ifdef WLC_SCAN_IOVARS
static int
wlc_scan_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	scan_info_t *scan_info = (scan_info_t *)hdl;
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;

	int err = 0;
	int32 int_val = 0;
	bool bool_val = FALSE;
	int32 *ret_int_ptr;

	(void)wlc_scan_info;

	/* convenience int and bool vals for first 4 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));
	bool_val = (int_val != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_PASSIVE):
		*ret_int_ptr = (int32)scan_info->defaults.passive;
		break;

	case IOV_SVAL(IOV_PASSIVE):
		scan_info->defaults.passive = (int8)int_val;
		break;

#ifdef STA
	case IOV_GVAL(IOV_SCAN_ASSOC_TIME):
		*ret_int_ptr = (int32)scan_info->defaults.assoc_time;
		break;

	case IOV_SVAL(IOV_SCAN_ASSOC_TIME):
		scan_info->defaults.assoc_time = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_SCAN_UNASSOC_TIME):
		*ret_int_ptr = (int32)scan_info->defaults.unassoc_time;
		break;

	case IOV_SVAL(IOV_SCAN_UNASSOC_TIME):
		scan_info->defaults.unassoc_time = (uint16)int_val;
		break;
#endif /* STA */

	case IOV_GVAL(IOV_SCAN_PASSIVE_TIME):
		*ret_int_ptr = (int32)scan_info->defaults.passive_time;
		break;

	case IOV_SVAL(IOV_SCAN_PASSIVE_TIME):
		scan_info->defaults.passive_time = (uint16)int_val;
		break;

#ifdef STA
	case IOV_GVAL(IOV_SCAN_HOME_TIME):
		*ret_int_ptr = (int32)scan_info->defaults.home_time;
		break;

	case IOV_SVAL(IOV_SCAN_HOME_TIME):
		scan_info->defaults.home_time = (uint16)int_val;
		break;

	case IOV_GVAL(IOV_SCAN_NPROBES):
		*ret_int_ptr = (int32)scan_info->defaults.nprobes;
		break;

	case IOV_SVAL(IOV_SCAN_NPROBES):
		scan_info->defaults.nprobes = (int8)int_val;
		break;

	case IOV_GVAL(IOV_SCAN_FORCE_ACTIVE):
		*ret_int_ptr = (int32)scan_info->force_active;
		break;

	case IOV_SVAL(IOV_SCAN_FORCE_ACTIVE):
		scan_info->force_active = (int_val != 0);
		break;

#ifdef EXTENDED_SCAN
	case IOV_SVAL(IOV_SCAN_EXTENDED):
		err = wlc_extdscan_request(wlc_scan_info, arg, len, NULL, NULL);
		break;
#ifdef BCMDBG
	case IOV_SVAL(IOV_SCAN_NOPSACK):
		scan_info->test_nopsack = (int8)int_val;
		break;
	case IOV_GVAL(IOV_SCAN_NOPSACK):
		*ret_int_ptr = (int32)scan_info->test_nopsack;
		break;
#endif /* BCMDBG */

#endif /* EXTENDED_SCAN */

#ifdef WLSCANCACHE
	case IOV_GVAL(IOV_SCANCACHE):
		*ret_int_ptr = wlc_scan_info->_scancache;
		break;

	case IOV_SVAL(IOV_SCANCACHE):
		wlc_scan_info->_scancache = bool_val;
		break;

	case IOV_GVAL(IOV_SCANCACHE_TIMEOUT):
		*ret_int_ptr = (int32)wlc_scandb_timeout_get(scan_info->sdb);
		break;

	case IOV_SVAL(IOV_SCANCACHE_TIMEOUT):
		wlc_scandb_timeout_set(scan_info->sdb, (uint)int_val);
		break;

	case IOV_SVAL(IOV_SCANCACHE_CLEAR):
		wlc_scandb_clear(scan_info->sdb);
		break;
#endif /* WLSCANCACHE */

	case IOV_GVAL(IOV_SCAN_ASSOC_TIME_DEFAULT):
		*ret_int_ptr = WLC_SCAN_ASSOC_TIME;
		break;

	case IOV_GVAL(IOV_SCAN_UNASSOC_TIME_DEFAULT):
		*ret_int_ptr = WLC_SCAN_UNASSOC_TIME;
		break;

#endif /* STA */

	case IOV_GVAL(IOV_SCAN_HOME_TIME_DEFAULT):
		*ret_int_ptr = WLC_SCAN_HOME_TIME;
		break;

	case IOV_GVAL(IOV_SCAN_PASSIVE_TIME_DEFAULT):
		*ret_int_ptr = WLC_SCAN_PASSIVE_TIME;
		break;
#ifdef BCMDBG
	case IOV_SVAL(IOV_SCAN_DBG):
		scan_info->debug = (uint8)int_val;
		break;
#endif

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}
#endif /* WLC_SCAN_IOVARS */

static void
wlc_scan_sendprobe(wlc_info_t *wlc, void *arg)
{
	scan_info_t *scan_info = (scan_info_t *)arg;
	int i;
	wlc_ssid_t *ssid;
	int n;
	wlc_bsscfg_t *cfg = scan_info->bsscfg;
	const struct ether_addr *da = &ether_bcast;
	const struct ether_addr *bssid = &ether_bcast;

	ASSERT(scan_info->pass >= 1);

	if (scan_info->extdscan) {
		ssid = &scan_info->ssid_list[scan_info->pass - 1];
		n = scan_info->nprobes;
	}
	else {
		ssid = scan_info->ssid_list;
		n = scan_info->nssid;
	}


	for (i = 0; i < n; i++) {
		wlc_sendprobe(wlc, cfg, ssid->SSID, ssid->SSID_len,
		              da, bssid, SCAN_PROBE_TXRATE(scan_info), NULL, 0);
		if (!scan_info->extdscan)
			ssid++;
	}
}

static void
wlc_scan_do_pass(scan_info_t *scan_info, chanspec_t chanspec)
{
	bool 	passive_scan = FALSE;
	uint32	channel_dwelltime = 0;
	uint32  dummy_tsf_h, start_tsf;
	wlc_info_t *wlc = scan_info->wlc;

	wlc->bcnmisc_scan = !ACT_FRAME_IN_PROGRESS(wlc->scan);
	wlc_mac_bcn_promisc(wlc);

	if (scan_info->scan_pub->state & SCAN_STATE_PASSIVE)
		passive_scan = TRUE;

	wlc_read_tsf(wlc, &start_tsf, &dummy_tsf_h);

	if (passive_scan || wlc_quiet_chanspec(wlc->cmi, chanspec) ||
	    !wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
		channel_dwelltime = CHANNEL_PASSIVE_DWELLTIME(scan_info);

		WL_SCAN(("wl%d: setting passive scantimer to %d ms, channel %d, tsf %u\n",
			wlc->pub->unit, channel_dwelltime, CHSPEC_CHANNEL(chanspec),
		        start_tsf));
	}
	else {
		channel_dwelltime = CHANNEL_ACTIVE_DWELLTIME(scan_info)/scan_info->npasses;

		WL_SCAN(("wl%d: setting active scantimer to %d ms, channel %d, tsf %u\n",
			wlc->pub->unit, channel_dwelltime, CHSPEC_CHANNEL(chanspec),
		        start_tsf));

		(scan_info->act_cb)(wlc, scan_info->act_cb_arg);
	}

	wlc_scan_add_timer(wlc, scan_info, channel_dwelltime, 0);
	scan_info->start_tsf = start_tsf;

	/* record phy noise for the scan channel */

	if (D11REV_LT(wlc->pub->corerev, 40))
		wlc_lq_noise_sample_request(wlc, WLC_NOISE_REQUEST_SCAN, CHSPEC_CHANNEL(chanspec));
}

bool
wlc_scan_ssid_match(wlc_scan_info_t *scan_pub, bcm_tlv_t *ssid_ie, bool filter)
{
	scan_info_t 	*scan_info = (scan_info_t *)scan_pub->scan_priv;
	wlc_info_t	*wlc = scan_info->wlc;
	wlc_ssid_t 	*ssid;
	int 		i;
	char		*c;

	(void)wlc;

	if ((scan_pub->usage == SCAN_ENGINE_USAGE_RM) ||
	    ((scan_info->nssid == 1) && ((scan_info->ssid_list[0]).SSID_len == 0))) {
		return TRUE;
	}

	if (scan_info->ssid_wildcard_enabled)
		return TRUE;

	if (!ssid_ie || ssid_ie->len > DOT11_MAX_SSID_LEN) {
		return FALSE;
	}

	/* filter out beacons which have all spaces or nulls as ssid */
	if (filter) {
		if (ssid_ie->len == 0)
			return FALSE;
		c = (char *)&ssid_ie->data[0];
		for (i = 0; i < ssid_ie->len; i++) {
			if ((*c != 0) && (*c != ' '))
				break;
			c++;
		}
		if (i == ssid_ie->len)
			return FALSE;
	}

	/* do not do ssid matching if we are sending out bcast SSIDs
	 * do the filtering before the scan_complete callback
	 */

	ssid = scan_info->ssid_list;
	for (i = 0; i < scan_info->nssid; i++) {
		if (WLC_IS_MATCH_SSID(wlc, ssid->SSID, ssid_ie->data,
		                      ssid->SSID_len, ssid_ie->len))
			return TRUE;
#ifdef WLP2P
		if (P2P_ENAB(wlc->pub) &&
		    wlc_p2p_ssid_match(wlc->p2p, scan_info->bsscfg,
		                       ssid->SSID, ssid->SSID_len,
		                       ssid_ie->data, ssid_ie->len))
			return TRUE;
#endif
		ssid++;
	}
	return FALSE;
}

bool
wlc_scan_recv_parse_bcn_prb(wlc_scan_info_t *si, bool beacon,
	wlc_bss_info_t *bi, uint8 *body, int body_len)
{
	scan_info_t *scan_info = (scan_info_t *)si->scan_priv;
	wlc_info_t *wlc = scan_info->wlc;

	(void)wlc;

	ASSERT(SCAN_IN_PROGRESS(si));

#ifdef WLP2P
	/* Reject Beacon/probe response if needed */
	if (P2P_ENAB(wlc->pub) &&
	    wlc_p2p_recv_parse_bcn_prb(wlc->p2p, scan_info->bsscfg, beacon,
	                &bi->rateset, body, body_len) != BCME_OK) {
		WL_INFORM(("wl%d: %s: wlc_p2p_parse_bcn_prb failed\n",
		           wlc->pub->unit, __FUNCTION__));
		return FALSE;
	}
#endif

	return TRUE;
}

#ifdef WL11N
static void
wlc_ht_obss_scan_update(scan_info_t *scan_info, int status)
{
	wlc_info_t *wlc = scan_info->wlc;
	wlc_bsscfg_t *cfg;
	wlc_obss_info_t *obss;
	obss_params_t *params;
	uint8 chanvec[OBSS_CHANVEC_SIZE]; /* bitvec of channels in 2G */
	uint8 chan, i;
	uint8 num_chan = 0;

	(void)wlc;

	WL_TRACE(("wl%d: wlc_ht_obss_scan_update\n", wlc->pub->unit));

	cfg = scan_info->bsscfg;

	/* checking  for valid fields */
	if (cfg == NULL || cfg->obss == NULL)
	{
		WL_COEX(("%s: %s is null; aborting obss scan update",
			__FUNCTION__, (cfg == NULL) ? "bsscfg" : "cfg->obss"));
		return;
	}
	/* abort if STA is not OBSS feature enabled */
	if (!COEX_ACTIVE(cfg))
		return;

	obss = cfg->obss;

	params = &obss->params;

	/* need to reset obss scan timer if expired and scan abort */
	if (status != WLC_SCAN_SUCCESS && !obss->scan_countdown) {
		obss->scan_countdown = 5;
		return;
	}
	if (scan_info->active_time < (int)params->active_dwell) {
		WL_COEX(("wl%d: %s: active time %d < %d\n", wlc->pub->unit, __FUNCTION__,
			scan_info->active_time, params->active_dwell));
		return;
	}
	if (scan_info->passive_time < (int)params->passive_dwell) {
		WL_COEX(("wl%d: %s: passive time %d < %d\n", wlc->pub->unit, __FUNCTION__,
			scan_info->passive_time, params->passive_dwell));
		return;
	}

	bzero(chanvec, OBSS_CHANVEC_SIZE);
	for (i = 0; i < scan_info->channel_num; i++) {
		chan = CHSPEC_CHANNEL(scan_info->chanspec_list[i]);
		if (chan <= CH_MAX_2G_CHANNEL) {
			setbit(chanvec, chan);
			num_chan++;
		}
	}
	if (num_chan < wlc->obss->num_chan) {
		WL_COEX(("wl%d: %s: scanned channel [%d] < OBSS Scan Params [%d]\n", wlc->pub->unit,
			__FUNCTION__, num_chan, wlc->obss->num_chan));
		return;
	}
	if (bcmp(chanvec, wlc->obss->chanvec, OBSS_CHANVEC_SIZE)) {
		WL_COEX(("wl%d: %s: scanned channel list don't match to OBSS Scan requirement\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	if (cfg->associated && COEX_ACTIVE(cfg)) {
		/* valid scan, analysis the result */
		wlc_ht_coex_filter_scanresult(cfg);
		obss->scan_countdown = params->bss_widthscan_interval;
		WL_COEX(("wl%d: %s: Arm scan for %dsec\n", wlc->pub->unit, __FUNCTION__,
			obss->scan_countdown));
	}

}
#endif /* WL11N */

#ifdef WLSCANCACHE

/* Add the current wlc->scan_results to the scancache */
static void
wlc_scan_fill_cache(scan_info_t *scan_info, uint current_timestamp)
{
	uint index;
	wlc_bss_list_t *scan_results;
	wlc_bss_info_t *bi;
	wlc_ssid_t SSID;
	uint8 bsstype;
	size_t datalen = 0;
	uint8* data = NULL;
	size_t bi_len;
	wlc_bss_info_t *new_bi;

	scan_results = scan_info->wlc->scan_results;

	wlc_scandb_ageout(scan_info->sdb, current_timestamp);

	/* walk the list of scan resutls, adding each to the cache */
	for (index = 0; index < scan_results->count; index++) {
		bi = scan_results->ptrs[index];
		if (bi == NULL) continue;

		bi_len = sizeof(wlc_bss_info_t);
		if (bi->bcn_prb)
			bi_len += bi->bcn_prb_len;

		/* allocate a new buffer if the current one is not big enough */
		if (data == NULL || bi_len > datalen) {
			if (data != NULL)
				MFREE(scan_info->osh, data, datalen);

			datalen = ROUNDUP(bi_len, 64);

			data = MALLOC(scan_info->osh, datalen);
			if (data == NULL)
				continue;
		}

		new_bi = (wlc_bss_info_t*)data;

		memcpy(new_bi, bi, sizeof(wlc_bss_info_t));
		if (bi->bcn_prb) {
			new_bi->bcn_prb = (struct dot11_bcn_prb*)(data + sizeof(wlc_bss_info_t));
			memcpy(new_bi->bcn_prb, bi->bcn_prb, bi->bcn_prb_len);
		}
		new_bi->flags |= WLC_BSS_CACHE;

		bsstype = bi->infra ? DOT11_BSSTYPE_INFRASTRUCTURE : DOT11_BSSTYPE_INDEPENDENT;
		SSID.SSID_len = bi->SSID_len;
		memcpy(SSID.SSID, bi->SSID, DOT11_MAX_SSID_LEN);

		wlc_scandb_add(scan_info->sdb,
		               &bi->BSSID, &SSID, bsstype, bi->chanspec, current_timestamp,
		               new_bi, bi_len);
	}

	if (data != NULL)
		MFREE(scan_info->osh, data, datalen);
}

/* Return the contents of the scancache in the 'bss_list' param.
 *
 * Return only those scan results that match the criteria specified by the other params:
 *
 * BSSID:	match the provided BSSID exactly unless BSSID is a NULL pointer or FF:FF:FF:FF:FF:FF
 * nssid:	nssid number of ssids in the array pointed to by SSID
 * SSID:	match [one of] the provided SSIDs exactly unless SSID is a NULL pointer,
 *		SSID[0].SSID_len == 0 (broadcast SSID), or nssid = 0 (no SSIDs to match)
 * BSS_type:	match the 802.11 infrastructure type. Should be one of the values:
 *		{DOT11_BSSTYPE_INFRASTRUCTURE, DOT11_BSSTYPE_INDEPENDENT, DOT11_BSSTYPE_ANY}
 * chanspec_list, chanspec_num: if chanspec_num == 0, no channel filtering is done. Otherwise
 *		the chanspec list should contain 20MHz chanspecs. Only BSSs with a matching channel,
 *		or for a 40MHz BSS, with a matching control channel, will be returned.
 */
void
wlc_scan_get_cache(wlc_scan_info_t *scan_pub,
                   const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                   int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                   wlc_bss_list_t *bss_list)
{
	scan_iter_params_t params;
	scan_info_t *scan_info = (scan_info_t *)scan_pub->scan_priv;

	params.merge = FALSE;
	params.bss_list = bss_list;
	params.current_ts = 0;

	memset(bss_list, 0, sizeof(wlc_bss_list_t));

	/* ageout any old entries */
	wlc_scandb_ageout(scan_info->sdb, OSL_SYSUPTIME());

	wlc_scandb_iterate(scan_info->sdb,
	                   BSSID, nssid, SSID, BSS_type, chanspec_list, chanspec_num,
	                   wlc_scan_build_cache_list, scan_info, &params);
}

/* Merge the contents of the scancache with entries already in the 'bss_list' param.
 *
 * Return only those scan results that match the criteria specified by the other params:
 *
 * current_timestamp: timestamp matching the most recent additions to the cache. Entries with
 *		this timestamp will not be added to bss_list.
 * BSSID:	match the provided BSSID exactly unless BSSID is a NULL pointer or FF:FF:FF:FF:FF:FF
 * nssid:	nssid number of ssids in the array pointed to by SSID
 * SSID:	match [one of] the provided SSIDs exactly unless SSID is a NULL pointer,
 *		SSID[0].SSID_len == 0 (broadcast SSID), or nssid = 0 (no SSIDs to match)
 * BSS_type:	match the 802.11 infrastructure type. Should be one of the values:
 *		{DOT11_BSSTYPE_INFRASTRUCTURE, DOT11_BSSTYPE_INDEPENDENT, DOT11_BSSTYPE_ANY}
 * chanspec_list, chanspec_num: if chanspec_num == 0, no channel filtering is done. Otherwise
 *		the chanspec list should contain 20MHz chanspecs. Only BSSs with a matching channel,
 *		or for a 40MHz BSS, with a matching control channel, will be returned.
 */
static void
wlc_scan_merge_cache(wlc_scan_info_t *scan_pub, uint current_timestamp,
                   const struct ether_addr *BSSID, int nssid, const wlc_ssid_t *SSID,
                   int BSS_type, const chanspec_t *chanspec_list, uint chanspec_num,
                   wlc_bss_list_t *bss_list)
{
	scan_iter_params_t params;
	scan_info_t *scan_info = (scan_info_t *)scan_pub->scan_priv;

	params.merge = TRUE;
	params.bss_list = bss_list;
	params.current_ts = current_timestamp;

	wlc_scandb_iterate(scan_info->sdb,
	                   BSSID, nssid, SSID, BSS_type, chanspec_list, chanspec_num,
	                   wlc_scan_build_cache_list, scan_info, &params);
}

static void
wlc_scan_build_cache_list(void *arg1, void *arg2, uint timestamp,
                          struct ether_addr *BSSID, wlc_ssid_t *SSID,
                          int BSS_type, chanspec_t chanspec, void *data, uint datalen)
{
	scan_info_t *scan_info = (scan_info_t*)arg1;
	scan_iter_params_t *params = (scan_iter_params_t*)arg2;
	wlc_bss_list_t *bss_list = params->bss_list;
	wlc_bss_info_t *bi;
	wlc_bss_info_t *cache_bi;

	/* skip the most recent batch of results when merging the cache to a bss_list */
	if (params->merge &&
	    params->current_ts == timestamp)
		return;

	if (bss_list->count >= (uint) scan_info->wlc->pub->tunables->maxbss)
		return;

	bi = MALLOC(scan_info->osh, sizeof(wlc_bss_info_t));
	if (!bi) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          scan_info->unit, __FUNCTION__, MALLOCED(scan_info->osh)));
		return;
	}

	ASSERT(data != NULL);
	ASSERT(datalen >= sizeof(wlc_bss_info_t));

	cache_bi = (wlc_bss_info_t*)data;

	memcpy(bi, cache_bi, sizeof(wlc_bss_info_t));
	if (cache_bi->bcn_prb_len) {
		ASSERT(datalen >= sizeof(wlc_bss_info_t) + bi->bcn_prb_len);
		if (!(bi->bcn_prb = MALLOC(scan_info->osh, bi->bcn_prb_len))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				scan_info->unit, __FUNCTION__, MALLOCED(scan_info->osh)));
			MFREE(scan_info->osh, bi, sizeof(wlc_bss_info_t));
			return;
		}
		/* Source is a flattened out structure but its bcn_prb pointer is not fixed
		 * when the entry was added to scancache db. So find out the new location.
		 */
		cache_bi->bcn_prb = (struct dot11_bcn_prb*)((uchar *) data +
		                                            sizeof(wlc_bss_info_t));

		memcpy(bi->bcn_prb, cache_bi->bcn_prb, bi->bcn_prb_len);
	}

	bss_list->ptrs[bss_list->count++] = bi;

}

static void
wlc_scan_cache_result(scan_info_t *scan_info)
{
	wlc_scan_info_t	*wlc_scan_info = scan_info->scan_pub;
	wlc_info_t *wlc = scan_info->wlc;
	uint timestamp = OSL_SYSUPTIME();

	/* if we have scan caching enabled, enter these results in the cache */
	wlc_scan_fill_cache(scan_info, timestamp);

	/* filter to just the desired SSID if we did a bcast scan for suppress */

	/* Provide the latest results plus cached results if they were requested. */
	if (wlc_scan_info->state & SCAN_STATE_INCLUDE_CACHE) {
		/* Merge cached results with current results */
		wlc_scan_merge_cache(wlc_scan_info, timestamp,
		                     &wlc_scan_info->bssid,
		                     scan_info->nssid, &scan_info->ssid_list[0],
		                     wlc_scan_info->bss_type,
		                     scan_info->chanspec_list, scan_info->channel_num,
		                     wlc->scan_results);

		WL_SCAN(("wl%d: %s: Merged scan results with cache, new total %d\n",
		         wlc->pub->unit, __FUNCTION__, wlc->scan_results->count));
	}
}

static int
wlc_scan_watchdog(void *hdl)
{
	scan_info_t *scan_info = (scan_info_t *)hdl;

	/* ageout any old entries to free up memory */
	wlc_scandb_ageout(scan_info->sdb, OSL_SYSUPTIME());

	return 0;
}
#endif /* WLSCANCACHE */

#ifdef EXTENDED_SCAN
void wlc_extdscan(wlc_info_t *wlc, int max_txrate,
	int nchan, chan_scandata_t *channel_list, wl_scan_type_t scan_type,
	int nprobes, bool split_scan, int nssid, wlc_ssid_t *ssid, scancb_fn_t fn, void *arg)
{
	chanspec_t	chanspec_list_arg[MAXCHANNEL];
	uint32 	scan_mode;
	int	i;
	chan_scandata_t	*cur_chandata;
	wlc_scan_info_t	*wlc_scan_info = wlc->scan;
	scan_info_t	*scan_info = (scan_info_t *) wlc_scan_info->scan_priv;
	int	away_channels_limit = 0;

	if (nchan == 0)
		return;

	/* split scan only one away channel to scan before coming back to home channel */
	if (split_scan)
		away_channels_limit = 1;

	/* passive/active scan */
	if (nprobes == 0)
		scan_mode = DOT11_SCANTYPE_PASSIVE;
	else
		scan_mode = DOT11_SCANTYPE_ACTIVE;

	scan_info->scan_type = scan_type;
	scan_info->max_txrate = max_txrate;

	/* channel list -- for now assume prealloc store in use */
	if (nchan > scan_info->nchan_prealloc)
		nchan = scan_info->nchan_prealloc;

	bzero(scan_info->chan_list, sizeof(chan_scandata_t) * scan_info->nchan_prealloc);
	bzero(chanspec_list_arg, sizeof(chanspec_list_arg));
	for (i = 0; i < nchan; i++) {
		cur_chandata = &channel_list[i];
		bcopy(cur_chandata, &scan_info->chan_list[i], sizeof(chan_scandata_t));
		chanspec_list_arg[i] = CH20MHZ_CHSPEC(cur_chandata->channel);
		WL_SCAN(("%d: Channel 0x%x, max_time %d, min_time %d\n", i, chanspec_list_arg[i],
			cur_chandata->channel_maxtime, cur_chandata->channel_mintime));
	}
	/* call the wlc_scan with the common API */
	wlc_scan(wlc_scan_info, DOT11_BSSTYPE_ANY, &ether_bcast, nssid, ssid,
		scan_mode, nprobes, 0, 0, -1,
		chanspec_list_arg, nchan, -1, FALSE, fn,
		arg, away_channels_limit, TRUE, FALSE, SCANCACHE_ENAB(wlc_scan_info),
		FALSE, wlc->cfg, SCAN_ENGINE_USAGE_NORM, NULL, NULL);
}

int
wlc_extdscan_request(wlc_scan_info_t *wlc_scan_info, void *param, int len,
	scancb_fn_t fn, void* arg)
{
	scan_info_t		*scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	chanspec_t		chanspec_list[MAXCHANNEL];
	wl_extdscan_params_t 	*extdscan_params = NULL; /* to avoid alignment issues */
	int 			i, nchan;
	int 			ssid_count = 0, chandata_list_size = 0;
	chan_scandata_t		*chandata_list = NULL, *chanptr, *chanarg;
	wlc_ssid_t		 *ssid;
	wlc_info_t		 *wlc = scan_info->wlc;
	int 			bcmerror = 0;

	/* validate the user passed arguments */
	if (len < WL_EXTDSCAN_PARAMS_FIXED_SIZE) {
		WL_ERROR(("%s: %d len is %d\n", __FUNCTION__, __LINE__, len));
		bcmerror = BCME_BADARG;
		goto done;
	}

	bzero(chanspec_list, sizeof(chanspec_list));
	nchan = 0;
	extdscan_params = (wl_extdscan_params_t *)MALLOC(wlc->osh, len);
	if (extdscan_params == NULL) {
		WL_ERROR(("%s: %d\n", __FUNCTION__, __LINE__));
		bcmerror = BCME_NORESOURCE;
		goto done;
	}
	bcopy(param, (void *)extdscan_params, len);

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	{
		bool n_ssid = FALSE;

		WL_INFORM(("Scan Params are \n"));
		WL_INFORM(("txrate is %d\n", extdscan_params->tx_rate));
		WL_INFORM(("nprobes is %d\n", extdscan_params->nprobes));
		WL_INFORM(("scan_type is %d\n", extdscan_params->scan_type));
		WL_INFORM(("split_scan is %d\n", extdscan_params->split_scan));
		WL_INFORM(("band is %d\n", extdscan_params->band));
		WL_INFORM(("channel_num is %d\n", extdscan_params->channel_num));
		chanarg = &extdscan_params->channel_list[0];
		for (i = 0; i < extdscan_params->channel_num; i++) {
			WL_INFORM(("Channel %d, txpower %d, chanmaxtime %d, chanmintime %d\n",
				chanarg->channel, chanarg->txpower,
				chanarg->channel_maxtime, chanarg->channel_mintime));
			chanarg++;
		}
		for (i = 0; i < WLC_EXTDSCAN_MAX_SSID; i++) {
			char ssidbuf[128];
			ssid = &extdscan_params->ssid[i];
			if (ssid->SSID_len) {
				if (!n_ssid) {
					n_ssid = TRUE;
					WL_INFORM(("ssids are \n"));
				}
				wlc_format_ssid(ssidbuf, ssid->SSID, ssid->SSID_len);
				WL_INFORM(("%d: \"%s\"\n", i, ssidbuf));
			}
		}
		if (!n_ssid) {
			WL_INFORM(("SSID List Empty: broadcast Scan\n"));
		}
	}
#endif /* BCMDBG || WLMSG_INFORM */

	if (extdscan_params->channel_num == 0)
		chandata_list_size = MAXCHANNEL * sizeof(chan_scandata_t);
	else
		chandata_list_size = extdscan_params->channel_num * sizeof(chan_scandata_t);

	chandata_list = (chan_scandata_t*)MALLOC(wlc->osh, chandata_list_size);
	if (chandata_list == NULL) {
		WL_ERROR(("%s: %d\n", __FUNCTION__, __LINE__));
		bcmerror = BCME_NORESOURCE;
		goto done;
	}
	chanptr = chandata_list;

	/* one band yet a time for now */
	if (extdscan_params->channel_num == 0) {
		uint32 			band, bd;
		int 			channel_count;
		chanspec_t		chanspec;

		wlc_scan_default_channels(scan_info->scan_pub, chanspec_list,
		                          &channel_count);
		band = extdscan_params->band;
		for (i = 0; i < channel_count; i++) {
			chanspec = chanspec_list[i];
			bd = chanspec & WL_CHANSPEC_BAND_MASK;
			if ((band == WLC_BAND_ALL) ||
			    ((band == WLC_BAND_2G) && (bd == WL_CHANSPEC_BAND_2G)) ||
			    ((band == WLC_BAND_5G) && (bd == WL_CHANSPEC_BAND_5G))) {
				chanptr->channel = CHSPEC_CHANNEL(chanspec);
				chanptr->channel_mintime = WLC_SCAN_ASSOC_TIME;
				chanptr->channel_maxtime = WLC_SCAN_ASSOC_TIME;
				chanptr++;
				nchan++;
			}
		}
	}
	else {
		chanarg = &extdscan_params->channel_list[0];
		nchan = extdscan_params->channel_num;
		bcopy((void *)chanarg, (void *)chanptr, ((sizeof(chan_scandata_t)) * nchan));
	}

	/* for now, limited to prealloced channel area, bail if too many */
	if (nchan > scan_info->nchan_prealloc) {
		WL_ERROR(("Extd scan: %s chan count %d exceeds prealloc %d\n",
		          (extdscan_params->channel_num ? "requested" : "default"),
		          extdscan_params->channel_num, scan_info->nchan_prealloc));
		bcmerror = extdscan_params->channel_num ? BCME_BADARG : BCME_RANGE;
		goto done;
	}

	/* validate the user passed arguments */
	for (i = 0; i < WLC_EXTDSCAN_MAX_SSID; i++) {
		ssid = &(extdscan_params->ssid[i]);
		if (ssid->SSID_len > 0)
			ssid_count++;
	}
	/* Broadcast Scan */
	if (!ssid_count) {
		WL_INFORM(("Broadcast Scan\n"));
		ssid = &(extdscan_params->ssid[0]);
		ssid->SSID_len = 0;
		ssid->SSID[0] = 0x00;
		ssid_count = 1;
	}

	/* for now, limited to prealloced ssid area, bail if too many */
	if (ssid_count > scan_info->nssid_prealloc) {
		WL_ERROR(("Extd scan: %d ssids exceeds prealloc of %d\n",
		          ssid_count, scan_info->nssid_prealloc));
		bcmerror = BCME_BADARG;
		goto done;
	}

	/* make the extd scan call */
	if (!fn) {
		fn = wlc_custom_scan_complete;
		arg = (void *)scan_info->wlc;
	}
	wlc_extdscan(scan_info->wlc, extdscan_params->tx_rate, nchan, chandata_list,
		extdscan_params->scan_type, extdscan_params->nprobes,
		extdscan_params->split_scan, ssid_count, extdscan_params->ssid, fn, arg);

done:
	if (bcmerror && fn)
		(fn)(arg, WLC_E_STATUS_FAIL, NULL);

	if (extdscan_params)
		MFREE(wlc->osh, extdscan_params, len);
	if (chandata_list)
		MFREE(wlc->osh, chandata_list, chandata_list_size);
	return bcmerror;
}
#endif /* EXTENDED_SCAN */

wlc_bsscfg_t *
wlc_scan_bsscfg(wlc_scan_info_t *wlc_scan_info)
{
	scan_info_t *scan_info = (scan_info_t *)wlc_scan_info->scan_priv;
	return scan_info->bsscfg;
}

#if defined(BCMDBG)
static const bcm_bit_desc_t scan_flags[] = {
	{SCAN_STATE_SUPPRESS, "SUPPRESS"},
	{SCAN_STATE_SAVE_PRB, "SAVE_PRB"},
	{SCAN_STATE_PASSIVE, "PASSIVE"},
	{SCAN_STATE_WSUSPEND, "WSUSPEND"},
	{SCAN_STATE_RADAR_CLEAR, "RADAR_CLEAR"},
	{SCAN_STATE_PSPEND, "PSPEND"},
	{SCAN_STATE_SUSPEND_REQ, "SUSPEND_REQ"},
	{SCAN_STATE_READY, "READY"},
	{SCAN_STATE_INCLUDE_CACHE, "INC_CACHE"},
	{0, NULL}
};

static const char *scan_usage[] = {
	"normal",
	"escan",
	"af",
	"rm"
};

static int
wlc_scan_dump(scan_info_t *si, struct bcmstrbuf *b)
{
	char state_str[64];
	char ssidbuf[SSID_FMT_BUF_LEN];
	char eabuf[ETHER_ADDR_STR_LEN];
	const char *bss_type_str;
	uint32 tsf_l, tsf_h;
	struct wlc_scan_info *scan_pub = si->scan_pub;

	wlc_format_ssid(ssidbuf, si->ssid_list[0].SSID, si->ssid_list[0].SSID_len);
	bcm_format_flags(scan_flags, scan_pub->state, state_str, 64);

	if (scan_pub->bss_type == DOT11_BSSTYPE_INFRASTRUCTURE)
		bss_type_str = "Infra";
	else if (scan_pub->bss_type == DOT11_BSSTYPE_INDEPENDENT)
		bss_type_str = "IBSS";
	else
		bss_type_str = "any";

	bcm_bprintf(b, "in_progress %d SSID \"%s\" type %s BSSID %s state 0x%x [%s] "
	            "usage %u [%s]\n",
	            scan_pub->in_progress, ssidbuf, bss_type_str,
	            bcm_ether_ntoa(&scan_pub->bssid, eabuf),
	            scan_pub->state, state_str,
	            scan_pub->usage, scan_pub->usage < ARRAYSIZE(scan_usage) ?
	                             scan_usage[scan_pub->usage] : "unknown");

	bcm_bprintf(b, "extdscan %d\n", si->extdscan);

	bcm_bprintf(b, "away_channels_cnt %d pass %d scan_results %d\n",
	            si->away_channels_cnt, si->pass, si->wlc->scan_results->count);

	if (SCAN_IN_PROGRESS(scan_pub))
		bcm_bprintf(b, "wlc->home_chanspec: %x chanspec_current %x\n",
		            si->wlc->home_chanspec, si->chanspec_list[si->channel_idx]);


	if (si->wlc->pub->up) {
		wlc_read_tsf(si->wlc, &tsf_l, &tsf_h);
		bcm_bprintf(b, "start_tsf 0x%08x current tsf 0x%08x\n", si->start_tsf, tsf_l);
	} else {
		bcm_bprintf(b, "start_tsf 0x%08x current tsf <not up>\n", si->start_tsf);
	}

	return 0;
}
#endif 
