/*
 * Association/Roam related routines
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_assoc.c 365823 2012-10-31 04:24:30Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmwifi_channels.h>
#include <siutils.h>
#include <sbchipc.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/802.11e.h>
#include <proto/wpa.h>
#include <bcmwpa.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_cca.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc_channel.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_bmac.h>
#include <wlc_scb.h>
#include <wlc_led.h>
#ifdef WLTDLS
#include <wlc_tdls.h>
#endif
#include <wlc_scb_ratesel.h>
#include <wl_export.h>
#if defined(BCMSUP_PSK)
#include <wlc_sup.h>
#endif
#include <wlc_rm.h>
#include <wlc_cac.h>
#include <wlc_extlog.h>
#include <wlc_ap.h>
#include <wlc_apps.h>
#include <wlc_scan.h>
#include <wlc_assoc.h>
#ifdef WLMCNX
#include <wlc_mcnx.h>
#endif
#ifdef WLP2P
#include <wlc_p2p.h>
#endif
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif
#ifdef WLWNM
#include <wlc_wnm.h>
#endif
#ifdef PSTA
#include <wlc_psta.h>
#endif

#include <wlioctl.h>
#ifdef PROP_TXSTATUS
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#endif
#include <wlc_11h.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wlc_11d.h>
#include <wlc_cntry.h>
#include <wlc_prot_g.h>
#include <wlc_prot_n.h>
#include <wlc_utils.h>

#if defined(BCMWAPI_WPI) || defined(BCMWAPI_WAI)
#include <wlc_wapi.h>
#endif

#include <wlc_vht.h>
#include <wlc_txbf.h>

#ifdef WL_BCN_COALESCING
#include <wlc_bcn_clsg.h>
#endif /* WL_BCN_COALESCING */

#ifdef WLPFN
#include <wl_pfn.h>
#endif

#ifdef WL11K
#include <wlc_rrm.h>
#endif /* WL11K */

#ifdef STA
/* join pref width in bits */
#define WLC_JOIN_PREF_BITS_TRANS_PREF	8 /* # of bits in weight for AP Transition Join Pref */
#define WLC_JOIN_PREF_BITS_RSSI		8 /* # of bits in weight for RSSI Join Pref */
#define WLC_JOIN_PREF_BITS_WPA		4 /* # of bits in weight for WPA Join Pref */
#define WLC_JOIN_PREF_BITS_BAND		1 /* # of bits in weight for BAND Join Pref */
#define WLC_JOIN_PREF_BITS_RSSI_DELTA	0 /* # of bits in weight for RSSI Delta Join Pref */

/* Fixed join pref start bits */
#define WLC_JOIN_PREF_START_TRANS_PREF	(32 - WLC_JOIN_PREF_BITS_TRANS_PREF)

/* join pref formats */
#define WLC_JOIN_PREF_OFF_COUNT		1 /* Tuple count field offset in WPA Join Pref TLV value */
#define WLC_JOIN_PREF_OFF_BAND		1 /* Band field offset in BAND Join Pref TLV value */
#define WLC_JOIN_PREF_OFF_DELTA_RSSI	0 /* RSSI delta value offset */

/* join pref rssi delta minimum cutoff
 * The rssi delta will only be applied if the target on the preferred band has an RSSI
 * of at least this minimum value. Below this RSSI value, the preferred band target gets
 * no RSSI preference.
 */
#ifndef WLC_JOIN_PREF_RSSI_BOOST_MIN
#define WLC_JOIN_PREF_RSSI_BOOST_MIN	-65	/* Targets must be at least -65dBm to get pref */
#endif

/* handy macros */
#define WLCAUTOWPA(cfg)		((cfg)->join_pref != NULL && (cfg)->join_pref->wpas > 0)
#define WLCTYPEBMP(type)	(1 << (type))
#define WLCMAXCNT(type)		(1 << (type))
#define WLCMAXVAL(bits)		((1 << (bits)) - 1)
#define WLCBITMASK(bits)	((1 << (bits)) - 1)

#if MAXWPACFGS != WLCMAXCNT(WLC_JOIN_PREF_BITS_WPA)
#error "MAXWPACFGS != (1 << WLC_JOIN_PREF_BITS_WPA)"
#endif

/* roaming trigger step value */
#define WLC_ROAM_TRIGGER_STEP	10	/* roaming trigger step in dB */

#define WLC_SCAN_RECREATE_TIME	    50	/* default assoc recreate scan time */
#define WLC_NLO_SCAN_RECREATE_TIME  30	/* nlo enabled assoc recreate scan time */

#ifdef BCMQT_CPU
#define WECA_ASSOC_TIMEOUT	1500	/* qt is slow */
#else
#define WECA_ASSOC_TIMEOUT	300	/* authentication or association timeout in ms */
#endif
#define WLC_IE_WAIT_TIMEOUT	200	/* assoc. ie waiting timeout in ms */

/* local routine declarations */
static void wlc_setssid_disassoc_complete(wlc_info_t *wlc, uint txstatus, void *arg);
static int wlc_assoc_bsstype(wlc_bsscfg_t *cfg);
static void wlc_assoc_scan_prep(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	const struct ether_addr *bssid, const chanspec_t* chanspec_list, int channel_num);
static void wlc_assoc_scan_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	const struct ether_addr *bssid, const chanspec_t* chanspec_list, int channel_num);
#ifdef WLSCANCACHE
static void wlc_assoc_cache_eval(wlc_info_t *wlc,
	const struct ether_addr *BSSID, const wlc_ssid_t *SSID,
	int bss_type, const chanspec_t *chanspec_list, uint chanspec_num,
	wlc_bss_list_t *bss_list, chanspec_t **target_list, uint *target_num);
static void wlc_assoc_cache_fail(wlc_bsscfg_t *cfg);
#else
#define wlc_assoc_cache_eval(wlc, BSSID, SSID, bt, cl, cn, bl, tl, tn)	((void)tl, (void)tn)
#define wlc_assoc_cache_fail(cfg)	((void)cfg)
#endif
static void wlc_assoc_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg);
static void wlc_assoc_success(wlc_bsscfg_t *cfg, struct scb *scb);
#ifdef WL_ASSOC_RECREATE
static void wlc_speedy_recreate_fail(wlc_bsscfg_t *cfg);
#endif /* WL_ASSOC_RECREATE */
static void wlc_auth_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint auth_status, uint auth_type);
static void wlc_assoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint assoc_status, bool reassoc, uint bss_type);
static void wlc_set_ssid_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint bss_type);
static void wlc_assoc_recreate_timeout(wlc_bsscfg_t *cfg);
static void wlc_assoc_init(wlc_bsscfg_t *cfg, uint type);

static void wlc_roamscan_complete(wlc_bsscfg_t *cfg);
static void wlc_roam_set_env(wlc_bsscfg_t *cfg, uint entries);
static void wlc_roam_release_flow_cntrl(wlc_bsscfg_t *cfg);

#ifdef AP
static bool wlc_join_check_ap_need_csa(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	chanspec_t chanspec, uint state);
static bool wlc_join_ap_do_csa(wlc_info_t *wlc, chanspec_t tgt_chanspec);
#endif /* AP */
static void wlc_join_attempt(wlc_bsscfg_t *cfg);
static void wlc_join_bss_start(wlc_bsscfg_t *cfg);
static bool wlc_join_chanspec_filter(wlc_bsscfg_t *cfg, chanspec_t chanspec);
static void wlc_cook_join_targets(wlc_bsscfg_t *cfg, bool roam, int cur_rssi);
static int wlc_join_start_ibss(wlc_bsscfg_t *cfg, const uint8 ssid[], int ssid_len);
static bool wlc_join_basicrate_supported(wlc_info_t *wlc, wlc_rateset_t *rs, int band);
static void wlc_join_adopt_bss(wlc_bsscfg_t *cfg);
static void wlc_join_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params);
static int wlc_join_pref_tlv_len(wlc_info_t *wlc, uint8 *pref, int len);
static int wlc_bss_list_expand(wlc_bsscfg_t *cfg, wlc_bss_list_t *from, wlc_bss_list_t *to);

static int wlc_assoc_req_add_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint req, bool top);
static int wlc_assoc_req_remove_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static void wlc_assoc_req_process_next(wlc_info_t *wlc);

static void wlc_merge_bcn_prb(wlc_info_t *wlc, struct dot11_bcn_prb *p1, int p1_len,
	struct dot11_bcn_prb *p2, int p2_len, struct dot11_bcn_prb **merged, int *merged_len);
static int wlc_merged_ie_len(wlc_info_t *wlc, uint8 *tlvs1, int tlvs1_len, uint8 *tlvs2,
	int tlvs2_len);
static bool wlc_find_ie_match(bcm_tlv_t *ie, bcm_tlv_t *ies, int len);
static void wlc_merge_ies(uint8 *tlvs1, int tlvs1_len, uint8 *tlvs2, int tlvs2_len, uint8* merge);

static bool wlc_clear_tkip_cm_bt(uint32 currentTime, uint32 refTime);
#ifdef WLFBT
static bool wlc_parse_ric_resp(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len);
#endif /* WLFBT */

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
static void wlc_print_roam_status(wlc_bsscfg_t *cfg, uint roam_reason, bool printcache);
#endif
#endif /* STA */

#ifdef STA
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
static const char *join_pref_name[] = {"rsvd", "rssi", "wpa", "band", "band_rssi_delta"};
#define WLCJOINPREFN(type)	join_pref_name[type]
#endif	/* BCMDBG || WLMSG_ASSOC */

#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_INFORM)
static const char *as_type_name[] = {
	"NONE", "JOIN", "ROAM", "RECREATE"
};
#define WLCASTYPEN(type)	as_type_name[type]
#endif /* BCMDBG || WLMSG_ASSOC || WLMSG_INFORM */

#if defined(BCMDBG) || defined(WLMSG_ASSOC)

/* When an AS_ state is added, add a string translation to the table below */
#if AS_LAST_STATE != 22     /* don't change this without adding to the table below!!! \
	*/
#error "You need to add an assoc state name string to as_st_names for the new assoc state"
#endif

static const struct {
	uint state;
	char name[28];
} as_st_names[] = {
	{AS_IDLE, "IDLE"},
	{AS_SCAN, "SCAN"},
	{AS_SENT_AUTH_1, "SENT_AUTH_1"},
	{AS_SENT_AUTH_2, "SENT_AUTH_2"},
	{AS_SENT_AUTH_3, "SENT_AUTH_3"},
	{AS_SENT_ASSOC, "SENT_ASSOC"},
	{AS_REASSOC_RETRY, "REASSOC_RETRY"},
	{AS_WAIT_RCV_BCN, "WAIT_RCV_BCN"},
	{AS_SYNC_RCV_BCN, "SYNC_RCV_BCN"},
	{AS_WAIT_DISASSOC, "WAIT_DISASSOC"},
	{AS_WAIT_TX_DRAIN, "WAIT_TX_DRAIN"},
	{AS_WAIT_TX_DRAIN_TIMEOUT, "WAIT_TX_DRAIN_TIMEOUT"},
	{AS_JOIN_START, "JOIN_START"},
	{AS_WAIT_PASSHASH, "WAIT_PASSHASH"},
	{AS_RECREATE_WAIT_RCV_BCN, "RECREATE_WAIT_RCV_BCN"},
	{AS_ASSOC_VERIFY, "ASSOC_VERIFY"},
	{AS_WAIT_IE, "WAIT_IE"},
	{AS_WAIT_IE_TIMEOUT, "WAIT_IE_TIMEOUT"},
	{AS_LOSS_ASSOC, "LOSS_ASSOC"},
	{AS_JOIN_CACHE_DELAY, "JOIN_CACHE_DELAY"},
	{AS_WAIT_FOR_AP_CSA, "WAIT_FOR_AP_CSA"},
	{AS_WAIT_FOR_AP_CSA_ROAM_FAIL, "WAIT_FOR_AP_CSA_ROAM_FAIL"},
	{AS_SENT_FTREQ, "AS_SENT_FTREQ"}
};

static const char *
wlc_as_st_name(uint state)
{
	uint i;

	for (i = 0; i < ARRAYSIZE(as_st_names); i++) {
		if (as_st_names[i].state == state)
			return as_st_names[i].name;
	}

	return "UNKNOWN";
}
#endif /* BCMDBG || WLMSG_ASSOC */

/* state(s) that can yield to other association requests */
#define AS_CAN_YIELD(bss, st)	(((bss) && (st) == AS_WAIT_RCV_BCN) || \
				 (st) == AS_SYNC_RCV_BCN || \
				 (st) == AS_RECREATE_WAIT_RCV_BCN)

static int
wlc_assoc_req_add_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint type, bool top)
{
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif
	int i, j;
	wlc_bsscfg_t *bc;
	wlc_assoc_t *as;
	bool as_in_progress_already;

	/* check the current state of assoc_req array */
	/* mark to see whether it's null or not */
	as_in_progress_already = AS_IN_PROGRESS(wlc);

	if (type == AS_ROAM) {
		if (AS_IN_PROGRESS(wlc))
			return BCME_BUSY;
		goto find_entry;
	}
	/* else if (type == AS_ASSOCIATION || type == AS_RECREATE) { */
	/* remove all other low priority requests */
	for (i = 0; i < WLC_MAXBSSCFG; i ++) {
		if ((bc = wlc->assoc_req[i]) == NULL)
			break;
		as = bc->assoc;
		if (as->type == AS_ASSOCIATION || as->type == AS_RECREATE)
			continue;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		wlc_format_ssid(ssidbuf, bc->SSID, bc->SSID_len);
#endif
		WL_ASSOC(("wl%d.%d: remove %s request in state %s for SSID '%s "
		          "from assoc_req list slot %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), WLCASTYPEN(type),
		          wlc_as_st_name(as->state), ssidbuf, i));
		wlc->assoc_req[i] = NULL;
	}
	/* } */

find_entry:
	/* find the first empty entry or entry with bsscfg in state AS_CAN_YIELD() */
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
#endif

	for (i = 0; i < WLC_MAXBSSCFG; i ++) {
		if ((bc = wlc->assoc_req[i]) == NULL)
			goto ins_entry;
		if (bc == cfg) {
			WL_ASSOC(("wl%d.%d: %s request in state %s for SSID '%s' exists "
			          "in assoc_req list at slot %d\n", wlc->pub->unit,
			          WLC_BSSCFG_IDX(cfg), WLCASTYPEN(type),
			          wlc_as_st_name(cfg->assoc->state), ssidbuf, i));
			return i;
		}
		else if (AS_CAN_YIELD(bc->BSS, bc->assoc->state) &&
			!AS_CAN_YIELD(cfg->BSS, cfg->assoc->state))
			goto ins_entry;
	}
	ASSERT(i < WLC_MAXBSSCFG);

ins_entry:
	/* insert the bsscfg in the list at slot i */
	WL_ASSOC(("wl%d.%d: insert %s request in state %s for SSID '%s' "
	          "in assoc_req list at slot %d\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
	          WLCASTYPEN(type), wlc_as_st_name(cfg->assoc->state), ssidbuf, i));

	j = i;
	bc = cfg;
	do {
		wlc_bsscfg_t *temp = wlc->assoc_req[i];
		wlc->assoc_req[i] = bc;
		bc = temp;
		if (bc == NULL || ++i >= WLC_MAXBSSCFG)
			break;
		as = bc->assoc;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		wlc_format_ssid(ssidbuf, bc->SSID, bc->SSID_len);
#endif
		WL_ASSOC(("wl%d.%d: move %s request in state %s for SSID '%s' "
		          "in assoc_req list to slot %d\n", wlc->pub->unit,
		          WLC_BSSCFG_IDX(cfg), WLCASTYPEN(as->type),
		          wlc_as_st_name(as->state), ssidbuf, i));
	}
	while (TRUE);
	ASSERT(i < WLC_MAXBSSCFG);

	/* as_in_progress now changed, update ps_ctrl */
	if (as_in_progress_already != AS_IN_PROGRESS(wlc)) {
		wlc_set_wake_ctrl(wlc);
	}

	return j;
}

static void
wlc_assoc_req_process_next(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif
	wlc_assoc_t *as;

	if ((cfg = wlc->assoc_req[0]) == NULL) {
		WL_ASSOC(("wl%d: all assoc requests in assoc_req list have been processed\n",
		          wlc->pub->unit));
		wlc_set_wake_ctrl(wlc);
		return;
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
#endif

	as = cfg->assoc;
	ASSERT(as != NULL);

	WL_ASSOC(("wl%d.%d: process %s request in assoc_req list for SSID '%s'\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), WLCASTYPEN(as->type), ssidbuf));

	switch (as->type) {
	case AS_ASSOCIATION:
		wlc_join_start(cfg, wlc_bsscfg_scan_params(cfg), wlc_bsscfg_assoc_params(cfg));
		break;
	default:
		ASSERT(0);
		break;
	}
}

static int
wlc_assoc_req_remove_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char ssidbuf[SSID_FMT_BUF_LEN];
#endif
	int i;
	bool as_in_progress_already;

	/* check the current state of assoc_req array */
	/* mark to see whether it's null or not */
	as_in_progress_already = AS_IN_PROGRESS(wlc);

	for (i = 0; i < WLC_MAXBSSCFG; i ++) {
		if (wlc->assoc_req[i] != cfg)
			continue;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
#endif
		WL_ASSOC(("wl%d.%d: remove %s request in state %s in assoc_req list "
		          "for SSID %s at slot %d\n", wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
		          WLCASTYPEN(cfg->assoc->type), wlc_as_st_name(cfg->assoc->state),
		          ssidbuf, i));
		/* move assoc requests up the list by 1 and stop at the first empty entry */
		for (; i < WLC_MAXBSSCFG - 1; i ++) {
			if ((wlc->assoc_req[i] = wlc->assoc_req[i + 1]) == NULL) {
				i = i + 1;
				break;
			}
		}
		wlc->assoc_req[i] = NULL;
		/* if we cleared the wlc->assoc_req[] list, update ps_ctrl */
		if (as_in_progress_already != AS_IN_PROGRESS(wlc)) {
			wlc_set_wake_ctrl(wlc);
		}

		return BCME_OK;
	}

	return BCME_ERROR;
}

/* start a join process by broadcast scanning all channels */
void
wlc_set_ssid(wlc_info_t *wlc, uchar SSID[], int len)
{
	wlc_join(wlc, wlc->cfg, SSID, len, NULL, NULL, 0);
}

/* prepare and start a join process including:
 * - abort any ongoing association or scan process if necessary
 * - enable the bsscfg (set the flags, ..., not much for STA)
 * - start the disassoc process if already associated to a different SSID, otherwise
 * - start the scan process (broadcast on all channels, or direct on specific channels)
 * - start the association process (assoc with a BSS, start an IBSS, or coalesce with an IBSS)
 * - mark the bsscfg to be up if the association succeeds otherwise try the next BSS
 *
 * bsscfg stores the desired SSID and/or bssid and/or chanspec list for later use in
 * a different execution context for example timer callback.
 */
void
wlc_join(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 *SSID, int len,
	wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params, int assoc_params_len)
{
	wlc_assoc_t *as;

	if (bsscfg != wlc->cfg &&
	    wlc->pub->corerev < 15) {
		WL_ERROR(("wl%d.%d: JOIN on non-primary bsscfg is not supported for d11 revid %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), wlc->pub->corerev));
		return;
	}

	ASSERT(bsscfg);
	ASSERT(BSSCFG_STA(bsscfg));

	as = bsscfg->assoc;
	ASSERT(as != NULL);

	/* abort any association state machine in process */
	if (as->state != AS_IDLE && as->state != AS_WAIT_DISASSOC)
		wlc_assoc_abort(bsscfg);

	as->ess_retries = 0;
	as->type = AS_ASSOCIATION;

	/* save SSID and assoc params for later use in a different context and retry */
	wlc_bsscfg_SSID_set(bsscfg, SSID, len);
	wlc_bsscfg_scan_params_set(wlc, bsscfg, scan_params);
	wlc_bsscfg_assoc_params_set(wlc, bsscfg, assoc_params, assoc_params_len);

#ifdef WME
	wlc_wme_initparams_sta(wlc, &bsscfg->wme->wme_param_ie);
#endif

	/* add the join request in the assoc_req list and let the wlc_assoc_change_state()
	 * from someone else when transitioning to IDLE state to kick us off later on.
	 */
	if (wlc_mac_request_entry(wlc, bsscfg, WLC_ACTION_ASSOC) != BCME_OK)
		return;

	wlc_join_start(bsscfg, wlc_bsscfg_scan_params(bsscfg), wlc_bsscfg_assoc_params(bsscfg));
}

static void
wlc_assoc_timer_del(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_assoc_t *as = cfg->assoc;

	wl_del_timer(wlc->wl, as->timer);
	as->rt = FALSE;
}

/* start a join process. SSID is saved in bsscfg. */
static void
wlc_join_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	wl_join_assoc_params_t *assoc_params)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_WSEC)
	char *ssidbuf;
	const char *ssidstr;
#endif

#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_WSEC)
	ssidbuf = (char *) MALLOC(wlc->osh, SSID_FMT_BUF_LEN);
	if (ssidbuf) {
		wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
		ssidstr = ssidbuf;
	} else
		ssidstr = "???";
#endif

	as->ess_retries ++;

	if (cfg->SSID_len == 0)
		WL_ASSOC(("wl%d: SCAN: wlc_join_start, Setting SSID to NULL...\n", WLCWLUNIT(wlc)));
	else
		WL_ASSOC(("wl%d: SCAN: wlc_join_start, Setting SSID to \"%s\"...\n", WLCWLUNIT(wlc),
		          ssidstr));

	ASSERT(as->state == AS_IDLE || as->state == AS_WAIT_DISASSOC);

	/* adopt the default BSS params as the target's BSS params */
	bcopy(wlc->default_bss, target_bss, sizeof(wlc_bss_info_t));

	/* update the target_bss with the ssid */
	target_bss->SSID_len = cfg->SSID_len;
	if (cfg->SSID_len > 0)
		bcopy(cfg->SSID, target_bss->SSID, cfg->SSID_len);

	/* this is STA only, no aps_associated issues */
	wlc_bsscfg_enable(wlc, cfg);

#if defined(BCMSUP_PSK)
	cfg->sup_type = SUP_UNUSED;
#endif

#ifdef BCMSUP_PSK
	if (SUP_ENAB(wlc->pub) && cfg->sup_enable_wpa &&
	    (cfg->WPA_auth != WPA_AUTH_DISABLED || WLCAUTOWPA(cfg)))
		cfg->sup_type = SUP_WPAPSK;
#endif


	/* clear PMKID cache */
	if (cfg->WPA_auth == WPA2_AUTH_UNSPECIFIED || WLCAUTOWPA(cfg)) {
		WL_WSEC(("wl%d: wlc_join_start: clearing PMKID cache and candidate list\n",
			wlc->pub->unit));
		cfg->npmkid = 0;
		cfg->npmkid_cand = 0;

#ifdef BCMSUP_PSK
		if (SUP_ENAB(wlc->pub) && (cfg->sup_enable_wpa))
			wlc_sup_clear_pmkid_store(cfg->sup);
#endif /* BCMSUP_PSK */
	}

	/* initialize ucode default key flag MHF1_DEFKEYVALID */
	wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, wlc_key_defkeyflag(wlc), WLC_BAND_ALL);

	/* abort any current scan, and return to home channel */
	wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWASSOC);

	if (as->state == AS_WAIT_DISASSOC) {
		/* we are interrupting an earlier wlc_set_ssid call and wlc_assoc_scan_start is
		 * scheduled to run as the wlc_disassociate_client callback. The target_bss has
		 * been updated above so just allow the wlc_assoc_scan_start callback to pick up
		 * the set_ssid work with the new target.
		 */
	} else {
		wlc_bss_info_t *current_bss = cfg->current_bss;
		wlc_assoc_init(cfg, AS_ASSOCIATION);
		if (cfg->associated &&
		    (cfg->SSID_len != current_bss->SSID_len ||
		     bcmp(cfg->SSID, (char*)current_bss->SSID, cfg->SSID_len))) {

			WL_ASSOC(("wl%d: SCAN: wlc_join_start, Disassociating from %s first\n",
			          WLCWLUNIT(wlc), bcm_ether_ntoa(&cfg->prev_BSSID, eabuf)));
			wlc_assoc_change_state(cfg, AS_WAIT_DISASSOC);
			wlc_disassociate_client(cfg, TRUE, wlc_setssid_disassoc_complete,
			                        (void *)(uintptr)cfg->ID);
		}
		else {
			const struct ether_addr *bssid = NULL;
			const chanspec_t *chanspec_list = NULL;
			int chanspec_num = 0;
			/* make sure the association timer is not pending */
			wlc_assoc_timer_del(wlc, cfg);
			/* use assoc params if any to limit the scan hence to speed up
			 * the join process.
			 */
			if (assoc_params != NULL) {
				bssid = &assoc_params->bssid;
				chanspec_list = assoc_params->chanspec_list;
				chanspec_num = assoc_params->chanspec_num;
			}
			/* continue the join process by starting the association scan */
			wlc_assoc_scan_prep(cfg, scan_params, bssid, chanspec_list, chanspec_num);
		}
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLMSG_WSEC)
	if (ssidbuf != NULL)
		 MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif
}

/* disassoc from the associated BSS first and then start a join process */
static void
wlc_setssid_disassoc_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	const struct ether_addr *bssid = NULL;
	const chanspec_t *chanspec_list = NULL;
	int chanspec_num = 0;
	wl_join_scan_params_t *scan_params;
	wl_join_assoc_params_t *assoc_params;
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_ID(wlc, (uint16)(uintptr)arg);
	wlc_assoc_t *as;

	/* in case bsscfg is freed before this callback is invoked */
	if (cfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, arg));
		return;
	}

	as = cfg->assoc;

	/* Check for aborted scans */
	if (as->type != AS_ASSOCIATION) {
		WL_ASSOC(("wl%d: wlc_setssid_disassoc_complete, as->type "
		          "was changed to %d\n", WLCWLUNIT(wlc), as->type));
		return;
	}

	if (as->state != AS_WAIT_DISASSOC) {
		WL_ASSOC(("wl%d: wlc_setssid_disassoc_complete, as->state "
		          "was changed to %d\n", WLCWLUNIT(wlc), as->state));
		return;
	}

	WL_ASSOC(("wl%d: SCAN: disassociation complete, call wlc_join_start\n", WLCWLUNIT(wlc)));

	/* use assoc params if any to limit the scan hence to speed up
	 * the join process.
	 */
	scan_params = wlc_bsscfg_scan_params(cfg);
	if ((assoc_params = wlc_bsscfg_assoc_params(cfg)) != NULL) {
		bssid = &assoc_params->bssid;
		chanspec_list = assoc_params->chanspec_list;
		chanspec_num = assoc_params->chanspec_num;
	}
	/* continue the join process by starting the association scan */
	wlc_assoc_scan_prep(cfg, scan_params, bssid, chanspec_list, chanspec_num);
}

static uint32
wlc_bss_pref_score(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	int j;
	int16 rssi;
	uint32 weight, value;
	uint chband;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
#ifdef WLWNM
	int trans_candidate_list_pref;
#endif /* WLWNM */
	(void)wlc;

	/* clamp RSSI to the range 0 > rssi > WLC_RSSI_MINVAL */
	rssi = MIN(0, bi->RSSI);
	rssi = MAX(rssi, WLC_RSSI_MINVAL);

	chband = CHSPEC2WLC_BAND(bi->chanspec);

	/* Give target a better RSSI based on delta preference as long as it is already
	 * has a strong enough signal.
	 */
	if (cfg->join_pref_rssi_delta.band == chband &&
	    rssi >= WLC_JOIN_PREF_RSSI_BOOST_MIN) {
		WL_ASSOC(("wl%d: Boost RSSI for AP on band %d by %d db from %d db to %d db\n",
		          wlc->pub->unit,
		          cfg->join_pref_rssi_delta.band,
		          cfg->join_pref_rssi_delta.rssi,
		          rssi, MIN(0, rssi+cfg->join_pref_rssi_delta.rssi)));

		rssi += cfg->join_pref_rssi_delta.rssi;

		/* clamp RSSI again to the range 0 > rssi > WLC_RSSI_MINVAL */
		rssi = MIN(0, rssi);
	}

	for (j = 0, weight = 0; j < (int)join_pref->fields; j ++) {
		switch (join_pref->field[j].type) {
		case WL_JOIN_PREF_RSSI:
			/* convert negative value to positive */
			value = WLCMAXVAL(WLC_JOIN_PREF_BITS_RSSI) + rssi;
			break;
		case WL_JOIN_PREF_WPA:
			/* use index as preference weight */
			value = bi->wpacfg;
			break;
		case WL_JOIN_PREF_BAND:
			/* use 1 for preferred band */
			if (join_pref->band == WLC_BAND_AUTO) {
				value = 0;
				break;
			}
			value = (chband == join_pref->band) ? 1 : 0;
			break;

		default:
			/* quiet compiler, should not come to here! */
			value = 0;
			break;
		}
		value &= WLCBITMASK(join_pref->field[j].bits);
		WL_ASSOC(("wl%d: wlc_bss_pref_score: field %s entry %d value 0x%x offset %d\n",
			WLCWLUNIT(wlc), WLCJOINPREFN(join_pref->field[j].type),
			j, value, join_pref->field[j].start));
		weight += value << join_pref->field[j].start;
	}

	/* pref fields may not be set; use rssi only to get the positive number */
	if (join_pref->fields == 0)
		weight = WLCMAXVAL(WLC_JOIN_PREF_BITS_RSSI) + rssi;

#ifdef WLWNM
	trans_candidate_list_pref = wlc_wnm_get_trans_candidate_list_pref(wlc, cfg, &bi->BSSID);

	if (trans_candidate_list_pref >= 0) {
		weight += (trans_candidate_list_pref & WLCBITMASK(WLC_JOIN_PREF_BITS_TRANS_PREF))
			<< WLC_JOIN_PREF_START_TRANS_PREF;
	}
#endif /* WLWNM */

	WL_ASSOC(("wl%d: %s: RSSI is %d in BSS %s with preference score 0x%x (qbss_load_aac 0x%x "
	          "and qbss_load_chan_free 0x%x)\n", WLCWLUNIT(wlc), __FUNCTION__, bi->RSSI,
	          bcm_ether_ntoa(&bi->BSSID, eabuf), weight, bi->qbss_load_aac,
	          bi->qbss_load_chan_free));

	return weight;
}

static void
wlc_cook_join_targets(wlc_bsscfg_t *cfg, bool for_roam, int cur_rssi)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	int i, j, k;
	wlc_bss_info_t **bip, *tmp_bi;
	uint orig_roam_delta, roam_delta = 0;
	bool done;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	uint32 *join_pref_score;
	uint32 join_pref_score_size = wlc->join_targets->count * sizeof(uint32);
	uint32 tmp_score, cur_pref_score;

	WL_ASSOC(("wl%d: %s: RSSI is %d; %d roaming %s; Join preference fields "
		"are 0x%x\n", WLCWLUNIT(wlc), __FUNCTION__, cur_rssi, wlc->join_targets->count,
		wlc->join_targets->count > 1 ? "target" : "targets", cfg->join_pref->fields));

	join_pref_score = (uint32 *)MALLOC(wlc->osh, join_pref_score_size);
	if (!join_pref_score) {
		WL_ERROR(("wl%d: %s: MALLOC failure\n",
			WLCWLUNIT(wlc), __FUNCTION__));
		return;
	}

	bip = wlc->join_targets->ptrs;

	for (i = 0; i < (int)wlc->join_targets->count; i ++) {
		if ((roam->reason == WLC_E_REASON_REQUESTED_ROAM) &&
			(WLC_IS_CURRENT_BSSID(cfg, &bip[i]->BSSID))) {
			join_pref_score[i] = 0;
		}
		else
			join_pref_score[i] = wlc_bss_pref_score(cfg, bip[i]);
	}

	/* no need to sort if we only see one AP */
	if (wlc->join_targets->count > 1) {
		/* sort join_targets by join preference score in increasing order */
		for (k = (int)wlc->join_targets->count; --k >= 0;) {
			if ((roam->reason == WLC_E_REASON_REQUESTED_ROAM) &&
				WLC_IS_CURRENT_BSSID(cfg, &bip[k]->BSSID) &&
				(bip[k]->chanspec == cfg->current_bss->chanspec)) {
				/* rule out current AP if specifically asked to roam */
				join_pref_score[k] = 0;
			}
			done = TRUE;
			for (j = 0; j < k; j++) {
				if (join_pref_score[j] > join_pref_score[j+1]) {
					/* swap score */
					tmp_score = join_pref_score[j];
					join_pref_score[j] = join_pref_score[j+1];
					join_pref_score[j+1] = tmp_score;
					/* swap bip */
					tmp_bi = bip[j];
					bip[j] = bip[j+1];
					bip[j+1] = tmp_bi;
					done = FALSE;
				}
			}
			if (done)
				break;
		}
	}

	if (for_roam &&
#ifdef OPPORTUNISTIC_ROAM
	    (roam->reason != WLC_E_REASON_BETTER_AP) &&
#endif /* OPPORTUNISTIC_ROAM */
	    TRUE) {
		wlc_bss_info_t *current_bss = cfg->current_bss;
		/*
		 * - We're here because our current AP's JSSI fell below some threshold
		 *   or we haven't heard from him for a while.
		 */

		/*
		 * Use input cur_rssi if we didn't get a probe response because we were
		 * unlucky or we're out of contact; otherwise, use the RSSI of the probe
		 * response. The channel of the probe response/beacon is checked in case
		 * the AP switched channels. In that case we do not want to update cur_rssi
		 * and instead consider the AP as a roam target.
		 */
		if (wlc_bss_connected(cfg) && roam->reason == WLC_E_REASON_LOW_RSSI) {
			cur_pref_score = wlc_bss_pref_score(cfg, current_bss);
		} else {
			cur_pref_score = 0;
		}

		/* search join target in decreasing order to pick the highest score */
		for (i = (int)wlc->join_targets->count - 1; i >= 0; i--) {
			if (WLC_IS_CURRENT_BSSID(cfg, &bip[i]->BSSID) &&
			    (bip[i]->chanspec == current_bss->chanspec)) {
				cur_pref_score = join_pref_score[i];
				cur_rssi = bip[i]->RSSI;
				break;
			}
		}


		/* Iterate through the roam cache and check that the target is valid */
		for (i = 0; i < (int)wlc->join_targets->count; i++) {
			struct ether_addr* bssid = &bip[i]->BSSID;
			for (j = 0; j < (int) roam->cache_numentries; j++) {
				if (!bcmp(bssid, &roam->cached_ap[j].BSSID, ETHER_ADDR_LEN) &&
				    (roam->cached_ap[j].chanspec == bip[i]->chanspec)) {
					if (roam->cached_ap[j].time_left_to_next_assoc > 0) {
						join_pref_score[i] = 0;
						WL_ASSOC(("wl%d: ROAM: %s: AP with BSSID %s marked "
						          "as an invalid roam target\n",
						          wlc->pub->unit,  __FUNCTION__,
						          bcm_ether_ntoa(bssid, eabuf)));
					}
				}
			}
		}

		WL_ASSOC(("wl%d: ROAM: wlc_cook_join_targets, roam_metric[%s] = 0x%x\n",
			WLCWLUNIT(wlc), bcm_ether_ntoa(&(cfg->BSSID), eabuf),
			cur_pref_score));

		/* Prune candidates not "significantly better" than our current AP.
		 * Notes:
		 * - The definition of "significantly better" may not be the same for
		 *   all candidates.  For example, Jason suggests using a different
		 *   threshold for our previous AP, if we roamed before.
		 * - The metric will undoubtedly need to change.  First pass, just use
		 *   the RSSI of the probe response.
		 */

		/* Try to make roam_delta adaptive. lower threshold when our AP is very weak,
		 *  falling the edge of sensitivity. Hold threshold when our AP is relative strong.
		 * It should really be a lookup table when segment gets "tiny".
		 *
		 *	roam_trigger+10		roam_trigger	roam_trigger-10	roam_trigger-20
		 *		|               ||		|		|
		 *   roam_delta	| roam_delta    |   roam_delta	| roam_delta-10 |roam_delta-10
		 */
		orig_roam_delta = wlc->band->roam_delta;

		if (cur_rssi > wlc->band->roam_trigger - 10)
			roam_delta = orig_roam_delta;
		else
			roam_delta =
				(orig_roam_delta > 10) ? (orig_roam_delta - 10) : orig_roam_delta;

		/* TODO
		 * It may be advisable not to apply roam delta-based prune once
		 * preference score is above a certain threshold.
		 */

		WL_ASSOC(("wl%d: ROAM: wlc_cook_join_targets, roam_delta = %d\n",
		      WLCWLUNIT(wlc), roam_delta));

		/* find cutoff point for pruning.
		 * already sorted in increasing order of join_pref_score
		 */
		for (i = 0; i < (int)wlc->join_targets->count; i++) {
			if (join_pref_score[i] > (cur_pref_score + roam_delta))
				break;
		}

		/* Prune, finally.
		 * - move qualifying APs to the beginning of list
		 * - note the boundary of the qualifying AP list
		 * - ccx-based pruning is done in wlc_join_attempt()
		 */
		for (j = 0; i < (int)wlc->join_targets->count; i++, j++) {
			/* move join_pref_score[i] to join_pref_score[j] */
			join_pref_score[j] = join_pref_score[i];
			/* swap bip[i] with bip[j]
			 * moving bip[i] to bip[j] alone without swapping causes memory leak.
			 * by swapping, the ones below threshold are still left in bip
			 * so that they can be freed at later time.
			 */
			tmp_bi = bip[j];
			bip[j] = bip[i];
			bip[i] = tmp_bi;
			WL_ASSOC(("wl%d: ROAM: cook_join_targets, after prune, roam_metric[%s] "
				"= 0x%x\n", WLCWLUNIT(wlc),
				bcm_ether_ntoa(&bip[j]->BSSID, eabuf), join_pref_score[j]));
		}
		wlc->join_targets_last = j;
	}


	MFREE(wlc->osh, join_pref_score, join_pref_score_size);

	/* Now sort pruned list using per-port criteria */
	if (wlc->pub->wlfeatureflag & WL_SWFL_WLBSSSORT)
		(void)wl_sort_bsslist(wlc->wl, bip);

#ifdef OPPORTUNISTIC_ROAM
	if (memcmp(cfg->join_bssid.octet, BSSID_INVALID, sizeof(cfg->join_bssid.octet)) != 0) {
		j = (int)wlc->join_targets->count - 1;
		for (i = j; i >= 0; i--) {
			if (memcmp(cfg->join_bssid.octet, bip[i]->BSSID.octet,
			           sizeof(bip[i]->BSSID.octet)) == 0) {
				tmp_bi = bip[i];

				for (k = i; k < j; k++)
				{
					bip[k] = bip[k+1];
				}
				bip[j] = tmp_bi;
				break;
			}
		}
		memcpy(cfg->join_bssid.octet, BSSID_INVALID, sizeof(cfg->join_bssid.octet));
	}
#endif /* OPPORTUNISTIC_ROAM */
}

/* use to reassoc to a BSSID on a particular channel list */
int
wlc_reassoc(wlc_bsscfg_t *cfg, wl_reassoc_params_t *reassoc_params)
{
	wlc_info_t *wlc = cfg->wlc;
	chanspec_t* chanspec_list = NULL;
	int channel_num = reassoc_params->chanspec_num;
	struct ether_addr *bssid = &(reassoc_params->bssid);
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	wlc_assoc_t *as = cfg->assoc;

	if (!BSSCFG_STA(cfg) || !cfg->BSS) {
		WL_ERROR(("wl%d: %s: bad argument STA %d BSS %d\n",
		          wlc->pub->unit, __FUNCTION__, BSSCFG_STA(cfg), cfg->BSS));
		return BCME_BADARG;
	}

	if (wlc_mac_request_entry(wlc, cfg, WLC_ACTION_REASSOC) != BCME_OK) {
		WL_ASSOC(("wl%d: %s: Blocked by other activity\n",
		          wlc->pub->unit, __FUNCTION__));
		return BCME_BUSY;
	}

	WL_ASSOC(("wl%d : %s: Attempting association to %s\n",
	          wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(bssid, eabuf)));

	/* fix dangling pointers */
	if (channel_num)
		chanspec_list = reassoc_params->chanspec_list;

	wlc_assoc_init(cfg, AS_ROAM);

	if (!bcmp(cfg->BSSID.octet, bssid->octet, ETHER_ADDR_LEN)) {
		/* Clear the current BSSID info to allow a roam-to-self */
		wlc_bss_clear_bssid(cfg);
	}

	/* Since doing a directed roam, use the cache if available */
	if (ASSOC_CACHE_ASSIST_ENAB(wlc))
		as->flags |= AS_F_CACHED_ROAM;

	wlc_assoc_scan_prep(cfg, NULL, bssid, chanspec_list, channel_num);
	return BCME_OK;
}

static void
wlc_assoc_scan_prep(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	const struct ether_addr *bssid, const chanspec_t *chanspec_list, int channel_num)
{

	wlc_assoc_scan_start(cfg, scan_params, bssid, chanspec_list, channel_num);
}


static int
wlc_assoc_bsstype(wlc_bsscfg_t *cfg)
{
	if (cfg->assoc->type == AS_ASSOCIATION)
		if (cfg->target_bss->infra == 0)
			return DOT11_BSSTYPE_INDEPENDENT;
		else if (cfg->target_bss->infra == 1)
			return DOT11_BSSTYPE_INFRASTRUCTURE;
		else
			return DOT11_BSSTYPE_ANY;
	else
		return DOT11_BSSTYPE_INFRASTRUCTURE;
}

/* chanspec_list being NULL/channel_num being 0 means all avaiable chanspecs */
static void
wlc_assoc_scan_start(wlc_bsscfg_t *cfg, wl_join_scan_params_t *scan_params,
	const struct ether_addr *bssid, const chanspec_t *chanspec_list, int channel_num)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	int bss_type = wlc_assoc_bsstype(cfg), idx;
	int err = BCME_ERROR;
	bool assoc = (as->type == AS_ASSOCIATION);
	wlc_ssid_t ssid;
	chanspec_t* chanspecs = NULL;
	uint chanspecs_size = 0;
#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLEXTLOG)
	char *ssidbuf;
#endif /* BCMDBG || WLMSG_ASSOC || WLEXTLOG */
	chanspec_t *target_list = NULL;
	uint target_num = 0;

	/* specific bssid is optional */
	if (bssid == NULL)
		bssid = &ether_bcast;

#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLEXTLOG)
	ssidbuf = (char *)MALLOC(wlc->osh, SSID_FMT_BUF_LEN);
#endif /* BCMDBG || WLMSG_ASSOC || WLEXTLOG */

	if (assoc) {
		/* standard association */
		ssid.SSID_len = target_bss->SSID_len;
		bcopy(target_bss->SSID, ssid.SSID, ssid.SSID_len);
	} else {
		wlc_bss_info_t *current_bss = cfg->current_bss;

		/* roaming */
		ssid.SSID_len = current_bss->SSID_len;
		bcopy(current_bss->SSID, ssid.SSID, ssid.SSID_len);

		/* Roam scan only on selected channels */
		if (roam->cache_valid && roam->cache_numentries > 0) {
			uint chidx = 0;

			chanspecs_size = roam->cache_numentries * sizeof(chanspec_t);
			chanspecs = (chanspec_t *)MALLOC(wlc->pub->osh, chanspecs_size);
			if (chanspecs == NULL) {
				err = BCME_NOMEM;
				goto fail;
			}
			/* We have exactly one AP in our roam candidate list, so scan it
			 * whether or not we are blocked from associated to it due to a prior
			 * roam event
			 */
			if (roam->cache_numentries == 1) {
				WL_ASSOC(("wl%d: %s: Adding chanspec 0x%x to scan list\n",
				          wlc->pub->unit, __FUNCTION__,
				          roam->cached_ap[0].chanspec));
				chanspecs[0] = roam->cached_ap[0].chanspec;
				chidx = 1;
			}
			/* can have multiple entries on one channel */
			else {
				for (idx = 0; idx < (int)roam->cache_numentries; idx++) {
					/* If not valid, don't add it to the chanspecs to scan */
					if (!roam->cached_ap[idx].time_left_to_next_assoc) {
						WL_ASSOC(("wl%d: %s: Adding chanspec 0x%x to scan "
						          "list\n", wlc->pub->unit, __FUNCTION__,
						          roam->cached_ap[idx].chanspec));
						chanspecs[chidx++] = roam->cached_ap[idx].chanspec;
					}
				}
			}
			chanspec_list = chanspecs;
			channel_num = chidx;

			WL_ASSOC(("wl%d: SCAN: using the cached scan results list (%d channels)\n",
			          WLCWLUNIT(wlc), channel_num));
		}
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	WL_ASSOC(("wl%d: SCAN: wlc_assoc_scan_start, starting a %s%s scan for SSID %s\n",
	          WLCWLUNIT(wlc), !ETHER_ISMULTI(bssid) ? "Directed " : "", assoc ? "JOIN" : "ROAM",
	          ssidbuf ? (wlc_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len), ssidbuf) : "???"));
#endif

#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLEXTLOG)
	/* DOT11_MAX_SSID_LEN check added so that we do not create ext logs for bogus
	 * joins of garbage ssid issued on XP
	 */
	if (assoc && (ssid.SSID_len != DOT11_MAX_SSID_LEN)) {
		WLC_EXTLOG(wlc, LOG_MODULE_ASSOC, FMTSTR_JOIN_START_ID,
			WL_LOG_LEVEL_ERR, 0, 0, ssidbuf ?
			(wlc_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len), ssidbuf)
			: "???");
	}
#endif /* BCMDBG || WLMSG_ASSOC || WLEXTLOG */

	/* if driver is down due to mpc, turn it on, otherwise abort */
	wlc->mpc_scan = TRUE;
	wlc_radio_mpc_upd(wlc);

	if (!wlc->pub->up) {
		WL_ASSOC(("wl%d: wlc_assoc_scan_start, can't scan while driver is down\n",
		          wlc->pub->unit));
		goto stop;
	}

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub))
		;	/* empty */
	else
#endif /* WLMCHAN */
	if (wlc->stas_associated > 0 &&
	    (wlc->stas_associated > 1 || !cfg->associated)) {
		uint32 ctlchan;
		for (idx = 0; idx < channel_num; idx ++) {
			ctlchan = wf_chspec_ctlchan(wlc->home_chanspec);
			if (chanspec_list[idx] == CH20MHZ_CHSPEC(ctlchan))
				break;
		}
		if (channel_num == 0 || idx < channel_num) {
			WL_ASSOC(("wl%d: wlc_assoc_scan_start: use shared chanspec "
			          "wlc->home_chanspec 0x%x\n",
			          wlc->pub->unit, wlc->home_chanspec));
			chanspec_list = &wlc->home_chanspec;
			channel_num = 1;
		}
		else {
			WL_ASSOC(("wl%d: wlc_assoc_scan_start, no chanspec\n",
			          wlc->pub->unit));
			goto stop;
		}
	}

	/* clear scan_results in case there are some left over from a prev scan */
	wlc_bss_list_free(wlc, wlc->scan_results);

	wlc_set_mac(cfg);

#ifdef WLP2P
	/* init BSS block and ADDR_BMP entry to allow ucode to follow
	 * the necessary chains of states in the transmit direction
	 * prior to association.
	 */
	if (BSS_P2P_ENAB(wlc, cfg))
		wlc_p2p_prep_bss(wlc->p2p, cfg);
#endif

	/* If association scancache use is enabled, check for a hit in the scancache.
	 * Do not check the cache if we are re-running the assoc scan after a cached
	 * assoc failure
	 */
	if (ASSOC_CACHE_ASSIST_ENAB(wlc) &&
	    (assoc || (as->flags & AS_F_CACHED_ROAM)) &&
	    !(as->flags & (AS_F_CACHED_CHANNELS | AS_F_CACHED_RESULTS))) {
		wlc_assoc_cache_eval(wlc, bssid, &ssid, bss_type,
		                     chanspec_list, channel_num,
		                     wlc->scan_results, &target_list, &target_num);
	}

	/* reset the assoc cache flags since this may be a retry of a cached attempt */
	as->flags &= ~(AS_F_CACHED_CHANNELS | AS_F_CACHED_RESULTS);

	/* narrow down the channel list if the cache eval came up with a short list */
	if (ASSOC_CACHE_ASSIST_ENAB(wlc) && target_num > 0) {
		WL_ASSOC(("wl%d: JOIN: using the cached scan results "
		          "to create a channel list len %u\n",
		          WLCWLUNIT(wlc), target_num));
		as->flags |= AS_F_CACHED_CHANNELS;
		chanspec_list = target_list;
		channel_num = target_num;
	}

	/* Use cached results if there was a hit instead of performing a scan */
	if (ASSOC_CACHE_ASSIST_ENAB(wlc) && wlc->scan_results->count > 0) {
		WL_ASSOC(("wl%d: JOIN: using the cached scan results for assoc (%d hits)\n",
		          WLCWLUNIT(wlc), wlc->scan_results->count));

		as->flags |= AS_F_CACHED_RESULTS;
#if 0 && (NDISVER < 0x0630)
		if (WLEXTSTA_ENAB(wlc->pub) &&
		    wlc_wps_pbcactive(wlc)) {
			WL_INFORM(("%s(): delay 0.5sec...\n", __FUNCTION__));
			wlc_assoc_change_state(cfg, AS_JOIN_CACHE_DELAY);
			wl_add_timer(wlc->wl, as->timer, 500, 0);
		}
		else
#endif 
		{
			wlc_assoc_change_state(cfg, AS_SCAN);
			wlc_assoc_scan_complete(wlc, WLC_E_STATUS_SUCCESS, cfg);
		}

		err = BCME_OK;
	} else {
		bool scan_suppress_ssid = FALSE;
		int passive_time = -1;
		int home_time = -1;
		int active_time = -1;
		int nprobes = -1;
		int scan_type = 0;
		/* override default scan params */
		if (scan_params != NULL) {
			scan_type = scan_params->scan_type;
			nprobes = scan_params->nprobes;
			active_time = scan_params->active_time;
			passive_time = scan_params->passive_time;
			home_time = scan_params->home_time;
		}

		/* active time for association recreation */
#if defined(NDIS) && (NDISVER >= 0x0630)
		if (cfg->nlo && as->type == AS_RECREATE)
			active_time	= WLC_NLO_SCAN_RECREATE_TIME;
		else
#endif /* (NDIS) && (NDISVER >= 0x0630) */
		if (as->flags & AS_F_SPEEDY_RECREATE)
			active_time = WLC_SCAN_RECREATE_TIME;

		/* kick off a scan for the requested SSID, possibly broadcast SSID. */
		err = wlc_scan(wlc->scan, bss_type, bssid, 1, &ssid, scan_type, nprobes,
			active_time, passive_time, home_time, chanspec_list, channel_num,
			0, TRUE, wlc_assoc_scan_complete, wlc, 0, FALSE, scan_suppress_ssid,
			FALSE, FALSE, cfg, SCAN_ENGINE_USAGE_NORM, NULL, NULL);

		if (err == BCME_OK) {
			wlc_assoc_change_state(cfg, AS_SCAN);
#ifdef CCA_STATS
			if (as->type == AS_ROAM)
				/* update cca info before roam scan */
				cca_stats_upd(wlc, 1);
#endif /* CCA_STATS */
		}

		/* when the scan is done, wlc_assoc_scan_complete() will be called to copy
		 * scan_results to join_targets and continue the association process
		 */
	}

	/* clean up short channel list if one was returned from wlc_assoc_cache_eval() */
	if (target_list != NULL)
		MFREE(wlc->osh, target_list, target_num * sizeof(chanspec_t));

stop:
	if (chanspecs != NULL)
		MFREE(wlc->pub->osh, chanspecs, chanspecs_size);

	wlc->mpc_scan = FALSE;
	wlc_radio_mpc_upd(wlc);

fail:
	if (err != BCME_OK) {
		wlc_assoc_req_remove_entry(wlc, cfg);
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC) || defined(WLEXTLOG)
	if (ssidbuf != NULL)
		MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif
	return;
}

#ifdef WL_ASSOC_RECREATE
static void
wlc_speedy_recreate_fail(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;

	as->flags &= ~AS_F_SPEEDY_RECREATE;

	wlc_bss_list_free(wlc, wlc->join_targets);

	WL_ASSOC(("wl%d: %s: ROAM: Doing full scan since assoc-recreate failed\n",
	          WLCWLUNIT(wlc), __FUNCTION__));

	wlc_iovar_setint(wlc, "scan_passive_time", 125);
	wlc_iovar_setint(wlc, "scan_unassoc_time", 75);
	wlc_iovar_setint(wlc, "scan_assoc_time", 75);
	wlc_iovar_setint(wlc, "scan_home_time", 0);

#if defined(NDIS) && (NDISVER >= 0x0630)
	/* event to get nlo assoc and scan params into bsscfg if nlo enabled */
	wlc_bss_mac_event(wlc, cfg, WLC_E_SPEEDY_RECREATE_FAIL, NULL, 0, 0, 0, 0, 0);
	if (cfg->nlo) {
		/* start assoc full scan using nlo parameters */
		wlc_assoc_scan_start(cfg, NULL, NULL,
			(cfg->assoc_params ? cfg->assoc_params->chanspec_list : NULL),
			(cfg->assoc_params ? cfg->assoc_params->chanspec_num : 0));
		wlc_bsscfg_assoc_params_reset(wlc, cfg);
	} else
#endif /* (NDIS) && (NDISVER >= 0x0630) */

	wlc_assoc_scan_start(cfg, NULL, NULL, NULL, 0);
}
#endif /* WL_ASSOC_RECREATE */

#ifdef WLSCANCACHE
static void
wlc_assoc_cache_eval(wlc_info_t *wlc, const struct ether_addr *BSSID, const wlc_ssid_t *SSID,
                     int bss_type, const chanspec_t *chanspec_list, uint chanspec_num,
                     wlc_bss_list_t *bss_list, chanspec_t **target_list, uint *target_num)
{
	chanspec_t *target_chanspecs;
	uint target_chanspec_alloc_num;
	uint target_chanspec_num;
	uint i, j;
	osl_t *osh;

	osh = wlc->osh;

	*target_list = NULL;
	*target_num = 0;

	wlc_scan_get_cache(wlc->scan, BSSID, 1, SSID, bss_type,
	                   chanspec_list, chanspec_num, bss_list);

	/* if there are no hits, just return with no cache assistance */
	if (bss_list->count == 0)
		return;

	if (wlc_assoc_cache_validate_timestamps(wlc, bss_list))
		return;

	WL_ASSOC(("wl%d: %s: %d hits, creating a channel list\n",
	          wlc->pub->unit, __FUNCTION__, bss_list->count));

	/* If the results are too old they might have stale information, so use a chanspec
	 * list instead to speed association.
	 */
	target_chanspec_num = 0;
	target_chanspec_alloc_num = bss_list->count;
	target_chanspecs = MALLOC(osh, sizeof(chanspec_t) * bss_list->count);
	if (target_chanspecs == NULL) {
		/* out of memory, skip cache assistance */
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(osh)));
		goto cleanup;
	}

	for (i = 0; i < bss_list->count; i++) {
		chanspec_t chanspec;
		uint8 ctl_ch;

		chanspec = bss_list->ptrs[i]->chanspec;

		/* convert a 40MHz or 80/160/8080Mhz chanspec to a 20MHz chanspec of the
		 * control channel since this is where we should scan for the BSS.
		 */
		if (CHSPEC_IS40(chanspec) || CHSPEC_IS80(chanspec) ||
			CHSPEC_IS160(chanspec) || CHSPEC_IS8080(chanspec)) {
			ctl_ch = wf_chspec_ctlchan(chanspec);
			chanspec = (chanspec_t)(ctl_ch | WL_CHANSPEC_BW_20 |
			                        CHSPEC_BAND(chanspec));
		}

		/* look for this bss's chanspec in the list we are building */
		for (j = 0; j < target_chanspec_num; j++)
			if (chanspec == target_chanspecs[j])
				break;

		/* if the chanspec is not already on the list, add it */
		if (j == target_chanspec_num)
			target_chanspecs[target_chanspec_num++] = chanspec;
	}

	/* Resize the chanspec list to exactly what it needed */
	if (target_chanspec_num != target_chanspec_alloc_num) {
		chanspec_t *new_list;

		new_list = MALLOC(osh, sizeof(chanspec_t) * target_chanspec_num);
		if (new_list != NULL) {
			memcpy(new_list, target_chanspecs,
			       sizeof(chanspec_t) * target_chanspec_num);
		} else {
			/* out of memory, skip cache assistance */
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(osh)));
			target_chanspec_num = 0;
		}

		MFREE(osh, target_chanspecs, sizeof(chanspec_t) * target_chanspec_alloc_num);
		target_chanspecs = new_list;
	}

	*target_list = target_chanspecs;
	*target_num = target_chanspec_num;

cleanup:
	/* clear stale scan_results */
	wlc_bss_list_free(wlc, wlc->scan_results);

	return;
}

/* If a cache assisted association attempt fails, retry with a regular assoc scan */
static void
wlc_assoc_cache_fail(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wl_join_scan_params_t *scan_params;
	const wl_join_assoc_params_t *assoc_params;
	const struct ether_addr *bssid = NULL;
	const chanspec_t *chanspec_list = NULL;
	int chanspec_num = 0;

	WL_ASSOC(("wl%d: %s: Association from cache failed, starting regular association scan\n",
	          WLCWLUNIT(wlc), __FUNCTION__));

	/* reset join_targets for new join attempt */
	wlc_bss_list_free(wlc, wlc->join_targets);

	scan_params = wlc_bsscfg_scan_params(cfg);
	if ((assoc_params = wlc_bsscfg_assoc_params(cfg)) != NULL) {
		bssid = &assoc_params->bssid;
		chanspec_list = assoc_params->chanspec_list;
		chanspec_num = assoc_params->chanspec_num;
	}

	wlc_assoc_scan_start(cfg, scan_params, bssid, chanspec_list, chanspec_num);
}

#endif /* WLSCANCACHE */

void
wlc_pmkid_build_cand_list(wlc_bsscfg_t *cfg, bool check_SSID)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	uint i, j, mark;
	wlc_bss_info_t *bi;

	WL_WSEC(("wl%d: building PMKID candidate list\n", wlc->pub->unit));

	/* Merge scan results and pmkid cand list */
	for (i = 0; i < wlc->scan_results->count; i++) {
#if defined(BCMDBG) || defined(WLMSG_WSEC)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif	/* BCMDBG || WLMSG_WSEC */

		bi = wlc->scan_results->ptrs[i];
		mark = MAXPMKID;

		/* right network? if not, move on */
		if (check_SSID &&
		    ((bi->SSID_len != target_bss->SSID_len) ||
		     bcmp(bi->SSID, target_bss->SSID, bi->SSID_len)))
			continue;

		WL_WSEC(("wl%d: wlc_pmkid_build_cand_list(): processing %s...",
			wlc->pub->unit, bcm_ether_ntoa(&bi->BSSID, eabuf)));

		/* already in candidate list? */
		for (j = 0; j < cfg->npmkid_cand; j++) {
			if (bcmp((void *)&bi->BSSID, (void *)&cfg->pmkid_cand[j].BSSID,
			         ETHER_ADDR_LEN) == 0) {
				WL_WSEC(("already in candidate list\n"));
				mark = j;
				break;
			}
		}

		/* already in candidate list at the end, move on */
		if (mark != MAXPMKID && mark == (cfg->npmkid_cand - 1))
			continue;

		/* not already in candidate list, add */
		if (mark == MAXPMKID) {
			WL_WSEC(("add to candidate list\n"));

			if (cfg->npmkid_cand == (MAXPMKID - 1)) {
				WL_WSEC(("wl%d: wlc_pmkid_build_cand_list(): no room..."
					 "replace oldest\n", wlc->pub->unit));
				mark = 0;
			} else
				mark = cfg->npmkid_cand++;
		}

		/* bubble each item up, overwriting item at mark */
		for (j = mark + 1; j < cfg->npmkid_cand; j++)
			cfg->pmkid_cand[j - 1] = cfg->pmkid_cand[j];

		/* new or updated entry gets added at or moved to end of list */
		bcopy(bi->BSSID.octet, cfg->pmkid_cand[cfg->npmkid_cand - 1].BSSID.octet,
		      ETHER_ADDR_LEN);
		cfg->pmkid_cand[cfg->npmkid_cand - 1].preauth =
			(((bi->wpa2.flags & RSN_FLAGS_PREAUTH) != 0) ? 1 : 0);
	}

	/* if port's open, request PMKID cache plumb */
	ASSERT(cfg->WPA_auth == WPA2_AUTH_UNSPECIFIED);
	if (cfg->wsec_portopen && cfg->npmkid_cand != 0) {
		WL_WSEC(("wl%d: wlc_pmkid_build_cand_list(): requesting PMKID cache plumb...\n",
			wlc->pub->unit));

#ifdef BCMSUP_PSK
		if (SUP_ENAB(wlc->pub) && (cfg->sup_enable_wpa))
			wlc_sup_pmkid_cache_req(cfg->sup);
		else
#endif /* BCMSUP_PSK */
			wlc_pmkid_event(cfg);
	}
}

/* Assoc/roam completion routine - must be called at the end of an assoc/roam process
 * no matter it finishes with success or failure or it doesn't finish due to abort.
 * wlc_set_ssid_complate() and wlc_roam_complete can be called individually whenever
 * it fits.
 */
static void
wlc_assoc_done(wlc_bsscfg_t *cfg, int status)
{
	uint type = cfg->assoc->type;
	int8 infra = cfg->target_bss->infra;

	if (type == AS_ASSOCIATION)
		wlc_set_ssid_complete(cfg, status, &cfg->BSSID, WLC_DOT11_BSSTYPE(infra));
	else
		wlc_roam_complete(cfg, status, &cfg->BSSID, WLC_DOT11_BSSTYPE(infra));
}

static void
wlc_assoc_scan_complete(void *arg, int status, wlc_bsscfg_t *cfg)
{
	int idx;
	wlc_bsscfg_t *valid_cfg;
	bool cfg_ok = FALSE;
	wlc_info_t *wlc;
	wlc_assoc_t *as;
	wlc_roam_t *roam;
	wlc_bss_info_t *target_bss;
	bool for_roam;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char *ssidbuf = NULL;
	const char *ssidstr;
	const char *msg_name;
	const char *msg_pref;
#endif /* BCMDBG || WLMSG_ASSOC */

	/* We have seen instances of where this function was called on a cfg that was freed.
	 * Verify cfg first before proceeding.
	 */
	wlc = (wlc_info_t *)arg;
	/* Must find a match in global bsscfg array before continuing */
	FOREACH_BSS(wlc, idx, valid_cfg) {
		if (valid_cfg == cfg) {
			cfg_ok = TRUE;
			break;
		}
	}
	if (!cfg_ok) {
		WL_ERROR(("wl%d: %s: no valid bsscfg matches cfg %p, exit\n",
		          WLCWLUNIT(wlc), __FUNCTION__, cfg));
		goto exit;
	}
	/* cfg has been validated, continue with rest of function */
	as = cfg->assoc;
	roam = cfg->roam;
	target_bss = cfg->target_bss;
	for_roam = (as->type != AS_ASSOCIATION);
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	msg_name = for_roam ? "ROAM" : "JOIN";
	msg_pref = !(ETHER_ISMULTI(&wlc->scan->bssid)) ? "Directed " : "";
	ssidbuf = (char *)MALLOC(wlc->osh, SSID_FMT_BUF_LEN);
	if (ssidbuf) {
		wlc_format_ssid(ssidbuf, target_bss->SSID, target_bss->SSID_len);
		ssidstr = ssidbuf;
	} else
		ssidstr = "???";
#endif /* BCMDBG || WLMSG_ASSOC */

	WL_ASSOC(("wl%d: SCAN: wlc_assoc_scan_complete\n", WLCWLUNIT(wlc)));

	/* Delay roam scan by 1 watchdog tick to allow other events like calibrations to run */
	if (!roam->scan_block)
		roam->scan_block = WLC_ROAM_SCAN_PERIOD;

	if (status != WLC_E_STATUS_SUCCESS) {
		if (target_bss->infra == 1 || status != WLC_E_STATUS_SUPPRESS) {
			if (status != WLC_E_STATUS_ABORT)
				wlc_assoc_done(cfg, status);
			goto exit;
		}
	}

	/* register scan results as possible PMKID candidates */
	if (cfg->WPA_auth == WPA2_AUTH_UNSPECIFIED)
		wlc_pmkid_build_cand_list(cfg, FALSE);

	/* copy scan results to join_targets for the reassociation process */
	if (WLCAUTOWPA(cfg)) {
		if (wlc_bss_list_expand(cfg, wlc->scan_results, wlc->join_targets)) {
			WL_ERROR(("wl%d: wlc_bss_list_expand failed\n", WLCWLUNIT(wlc)));
			wlc_assoc_done(cfg, WLC_E_STATUS_FAIL);
			goto exit;
		}
	}
	else
		wlc_bss_list_xfer(wlc->scan_results, wlc->join_targets);
	wlc->join_targets_last = wlc->join_targets->count;

	if (wlc->join_targets->count > 0) {
		WL_ASSOC(("wl%d: SCAN for %s%s: SSID scan complete, %d results for \"%s\"\n",
			WLCWLUNIT(wlc), msg_pref, msg_name,
			wlc->join_targets->count, ssidstr));
	} else if (target_bss->SSID_len > 0) {
		WL_ASSOC(("wl%d: SCAN for %s%s: SSID scan complete, no matching SSIDs found "
			"for \"%s\"\n", WLCWLUNIT(wlc), msg_pref, msg_name, ssidstr));
	} else {
		WL_ASSOC(("wl%d: SCAN for %s: SSID scan complete, no SSIDs found for broadcast "
			"scan\n", WLCWLUNIT(wlc), msg_name));
	}

	/* sort join targets by signal strength */
	/* for roam, prune targets if they do not have signals stronger than our current AP */
	if (wlc->join_targets->count > 0) {
		/* No pruning to be done if this is a directed, cache assisted ROAM */
		if (for_roam && !(as->flags & AS_F_CACHED_ROAM)) {
			wlc_cook_join_targets(cfg, TRUE, cfg->link->rssi);
			WL_ASSOC(("wl%d: ROAM: %d roam target%s after pruning\n",
				WLCWLUNIT(wlc), wlc->join_targets_last,
				(wlc->join_targets_last == 1) ? "" : "s"));
		} else {
			wlc_cook_join_targets(cfg, FALSE, 0);
		}
		/* no results */
	} else if (for_roam && roam->cache_valid && roam->cache_numentries > 0) {
		/* We need to blow away our cache if we were roaming for entries that ended
		 * not existing. Maybe our AP changed channels?
		 */
		WL_ASSOC(("wl%d: %s: Forcing a new roam scan becasue we found no APs "
		          "from our partial scan results list\n",
		          wlc->pub->unit, __FUNCTION__));
		roam->cache_valid = FALSE;
		roam->active = FALSE;
		roam->scan_block = 0;
	}

	/* Clear the flag */
	as->flags &= ~AS_F_CACHED_ROAM;

	if (wlc->join_targets_last > 0) {
		wlc_ap_mute(wlc, TRUE, cfg, -1);
		wlc_join_attempt(cfg);
	} else if (target_bss->infra != 1 &&
	           !wlc->IBSS_join_only &&
	           target_bss->SSID_len > 0) {
		/* Create an IBSS if we are IBSS or AutoUnknown mode,
		 * and we had a non-Null SSID
		 */
		WL_ASSOC(("wl%d: JOIN: creating an IBSS with SSID \"%s\"\n",
			WLCWLUNIT(wlc), ssidstr));

		if (!wlc_join_start_ibss(cfg, target_bss->SSID, target_bss->SSID_len)) {
			WL_APSTA_UPDN(("wl%d: Reporting link up on config 0 for IBSS starting "
				"a BSS\n", WLCWLUNIT(wlc)));
			wlc_link(wlc, TRUE, &cfg->BSSID, cfg, 0);
			wlc_set_ssid_complete(cfg, WLC_E_STATUS_SUCCESS, &cfg->BSSID,
				WLC_DOT11_BSSTYPE(target_bss->infra));
		} else {
			wlc_set_ssid_complete(cfg, WLC_E_STATUS_FAIL, NULL,
				DOT11_BSSTYPE_INDEPENDENT);
		}
	} else {
		/* no join targets */
		/* see if the target channel information could be cached, if caching is desired */
		if (for_roam && roam->active && roam->partialscan_period)
			wlc_roamscan_complete(cfg);

		/* Retry the scan if we used the scan cache for the initial attempt,
		 * Otherwise, report the failure
		 */
		if (!for_roam && ASSOC_CACHE_ASSIST_ENAB(wlc) &&
		    (as->flags & AS_F_CACHED_CHANNELS))
			wlc_assoc_cache_fail(cfg);
#ifdef WL_ASSOC_RECREATE
		else if (as->flags & AS_F_SPEEDY_RECREATE)
			wlc_speedy_recreate_fail(cfg);
#endif /* WL_ASSOC_RECREATE */
		else {
			WL_ASSOC(("scan found no target to send WLC_E_DISASSOC\n"));

#ifdef WLP2P
			if (BSS_P2P_ENAB(wlc, cfg) &&
				for_roam &&
				(as->type == AS_ROAM)) {

				/* indicate DISASSOC due to unreachability */
				wlc_handle_ap_lost(wlc, cfg);

				WL_ASSOC(("wl%d %s: terminating roaming for p2p\n",
					WLCWLUNIT(wlc), __FUNCTION__));
				wlc_assoc_done(cfg, WLC_E_STATUS_NO_NETWORKS);
				wlc_bsscfg_disable(wlc, cfg);
				goto exit;
			}
#endif /* WLP2P */

			wlc_assoc_done(cfg, WLC_E_STATUS_NO_NETWORKS);

#ifdef WLEXTLOG
			if (!for_roam &&
			    target_bss->SSID_len != DOT11_MAX_SSID_LEN) {
				WLC_EXTLOG(wlc, LOG_MODULE_ASSOC, FMTSTR_NO_NETWORKS_ID,
				           WL_LOG_LEVEL_ERR, 0, 0, NULL);
			}
#endif
		}
	}

exit:
#ifdef WLLED
	wlc_led_event(wlc->ledh);
#endif
	/* This return is explicitly added so the "led:" label has something that follows. */

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	if (ssidbuf != NULL)
		MFREE(wlc->osh, (void *)ssidbuf, SSID_FMT_BUF_LEN);
#endif
	return;
}

/* cases to roam
 * - wlc_watchdog() found beacon lost for too long
 * - wlc_recvdata found low RSSI in received frames
 * - AP DEAUTH/DISASSOC sta
 * - CCX initiated roam - directed roam, TSPEC rejection
 * it will end up in wlc_assoc_scan_complete and WLC_E_ROAM
 */
int
wlc_roam_scan(wlc_bsscfg_t *cfg, uint reason)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;

	if (roam->original_reason == WLC_E_REASON_INITIAL_ASSOC) {
		WL_ASSOC(("wl%d %s: update original roam reason to %u\n", WLCWLUNIT(wlc),
			__FUNCTION__, reason));
		roam->original_reason = reason;
	}

	if (roam->off) {
		WL_INFORM(("wl%d: roam scan is disabled\n", wlc->pub->unit));
		return BCME_EPERM;
	}

	if (!cfg->associated)
		return BCME_NOTASSOCIATED;

	if (wlc_mac_request_entry(wlc, cfg, WLC_ACTION_ROAM) != BCME_OK)
		return BCME_EPERM;

	wlc_assoc_init(cfg, AS_ROAM);
	roam->reason = reason;


	/* start the re-association process by starting the association scan */
	wlc_assoc_scan_prep(cfg, NULL, NULL, NULL, 0);

	return 0;
}

/*
 * check encryption settings: return TRUE if security mismatch
 *
 * if (WSEC_AES_ENABLED(cfg->wsec)) {
 *    if (((AP ucast is None) && (AP mcast is AES)) ||
 *         (!(AP mcast is None) && (AP ucast includes AES))) {
 *         keep();
 *     }
 * } else if (WSEC_TKIP_ENABLED(cfg->wsec)) {
 *     if (((AP ucast is None) && (AP mcast is TKIP)) ||
 *         (!(AP mcast is None) && (AP ucast includes TKIP))) {
 *         keep();
 *     }
 * } else if (WSEC_WEP_ENABLED(cfg->wsec)) {
 *     if ((AP ucast is None) && (AP mcast is WEP)) {
 *         keep();
 *     }
 * }
 * prune();
 *
 * TKIP countermeasures:
 * - prune non-WPA
 * - prune WPA with encryption <= TKIP
 */
static bool
wlc_join_wsec_filter(wlc_info_t *wlc, wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	struct rsn_parms *rsn;
	bool prune = TRUE;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif
#ifdef WLFBT
	bool akm_match = FALSE;
	uint i;

	/* Check AKM suite in target AP against the one STA is currently configured for */
	if (IS_WPA2_AUTH(cfg->WPA_auth) && (bi->wpa2.flags & RSN_FLAGS_SUPPORTED)) {
		for (i = 0; i < bi->wpa2.acount && (akm_match == FALSE); i++) {
			if (WLFBT_ENAB(wlc->pub) && (cfg->wpa2_auth_ft)) {
				if (((bi->wpa2.auth[i] == RSN_AKM_FBT_1X) &&
					(cfg->WPA_auth & WPA2_AUTH_UNSPECIFIED)) ||
					((bi->wpa2.auth[i] == RSN_AKM_FBT_PSK) &&
					(cfg->WPA_auth & WPA2_AUTH_PSK))) {
					WL_ASSOC(("wl%d: JOIN: FBT AKM match\n", WLCWLUNIT(wlc)));
					akm_match = TRUE;
				}
			}
			else if (!(cfg->wpa2_auth_ft)) {
				if (((bi->wpa2.auth[i] == RSN_AKM_UNSPECIFIED) &&
					(cfg->WPA_auth & WPA2_AUTH_UNSPECIFIED)) ||
					((bi->wpa2.auth[i] == RSN_AKM_PSK) &&
					(cfg->WPA_auth & WPA2_AUTH_PSK))) {
					WL_ASSOC(("wl%d: JOIN: WPA AKM match\n", WLCWLUNIT(wlc)));
					akm_match = TRUE;
				}
			}
		}
	}
#endif /* WLFBT */

	/* check authentication mode */
	if (IS_WPA_AUTH(cfg->WPA_auth) && (bi->wpa.flags & RSN_FLAGS_SUPPORTED))
		rsn = &(bi->wpa);
	else
	if (
#if defined(WLFBT)
	/* Fix for pruning BSSID when STA is moving to a different security type */
		WLFBT_ENAB(wlc->pub) && akm_match &&
#endif /* WLFBT */
		IS_WPA2_AUTH(cfg->WPA_auth) && (bi->wpa2.flags & RSN_FLAGS_SUPPORTED))
		rsn = &(bi->wpa2);
#ifdef BCMWAPI_WAI
	else
	if (IS_WAPI_AUTH(cfg->WPA_auth) && (bi->wapi.flags & RSN_FLAGS_SUPPORTED))
		rsn = &(bi->wapi);
#endif /* BCMWAPI_WAI */
	else
	{
		WL_ASSOC(("wl%d: JOIN: BSSID %s pruned for security reasons\n",
			WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
		wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
			WLC_E_RSN_MISMATCH, 0, 0, 0);
		return prune;
	}

#ifdef MFP
	/* do MFP checking before more complicated algorithm checks below */
	if ((cfg->wsec & MFP_REQUIRED) && !(bi->wpa2.flags & RSN_FLAGS_MFPC)) {
		/* We require MFP , but peer is not MFP capable */
		WL_ASSOC(("wl%d: %s: BSSID %s pruned, MFP required but peer does"
			" not advertise MFP\n",
			WLCWLUNIT(wlc), __FUNCTION__, bcm_ether_ntoa(&bi->BSSID, eabuf)));
		return prune;
	}
	if ((bi->wpa2.flags & RSN_FLAGS_MFPR) && !(cfg->wsec & MFP_CAPABLE)) {
		/* Peer requires MFP , but we don't have MFP enabled */
		WL_ASSOC(("wl%d: %s: BSSID %s pruned, peer requires MFP but MFP not"
			" enabled locally\n",
			WLCWLUNIT(wlc), __FUNCTION__, bcm_ether_ntoa(&bi->BSSID, eabuf)));
		return prune;
	}
#endif /* MFP */

	/* Clear TKIP Countermeasures Block Timer, if timestamped earlier
	 * than WPA_TKIP_CM_BLOCK (=60 seconds).
	 */

	if (cfg->tk_cm_bt) {
		uint32 currentTime = OSL_SYSUPTIME();
		if (wlc_clear_tkip_cm_bt(currentTime, cfg->tk_cm_bt_tmstmp) == TRUE) {
			cfg->tk_cm_bt = 0;
		}
	}

	if (WSEC_AES_ENABLED(cfg->wsec)) {

		if ((UCAST_NONE(rsn) && MCAST_AES(rsn)) || (MCAST_AES(rsn) && UCAST_AES(rsn)) ||
			((cfg->tk_cm_bt == 0) && (!MCAST_NONE(rsn) && UCAST_AES(rsn))))
			prune = FALSE;
		else {
			WL_ASSOC(("wl%d: JOIN: BSSID %s AES: no AES support or TKIP cm bt %d\n",
				WLCWLUNIT(wlc),	bcm_ether_ntoa(&bi->BSSID, eabuf), cfg->tk_cm_bt));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_ENCR_MISMATCH,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
		}

	} else if (WSEC_TKIP_ENABLED(cfg->wsec)) {

		if ((cfg->tk_cm_bt == 0) && ((UCAST_NONE(rsn) && MCAST_TKIP(rsn)) ||
			(!MCAST_NONE(rsn) && UCAST_TKIP(rsn))))
			prune = FALSE;
		else {
			WL_ASSOC(("wl%d: JOIN: BSSID %s TKIP: no TKIP support or TKIP cm bt %d\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf), cfg->tk_cm_bt));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_ENCR_MISMATCH,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
		}

	} else
#if defined(BCMWAPI_WAI)
	if (WSEC_SMS4_ENABLED(cfg->wsec)) {
		if (WAPI_RSN_UCAST_LOOKUP(rsn) && WAPI_RSN_MCAST_LOOKUP(rsn))
			prune = FALSE;
		else {
			WL_ASSOC(("wl%d: JOIN: BSSID %s SMS4: no SMS4 support\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_ENCR_MISMATCH,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
		}
	} else
#endif /* BCMWAPI_WAI */
	{
		if ((cfg->tk_cm_bt == 0) && (UCAST_NONE(rsn) && MCAST_WEP(rsn)))
			prune = FALSE;
		else {
			WL_ASSOC(("wl%d: JOIN: BSSID %s no WEP support or TKIP cm %d\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf), cfg->tk_cm_bt));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_ENCR_MISMATCH,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
		}
	}

	if (prune) {
		WL_ASSOC(("wl%d: JOIN: BSSID %s pruned for security reasons\n",
			WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
	}

	return prune;
}

/* check the channel against the channels in chanspecs list in assoc params */
static bool
wlc_join_chanspec_filter(wlc_bsscfg_t *bsscfg, chanspec_t chanspec)
{
	wl_join_assoc_params_t *assoc_params = wlc_bsscfg_assoc_params(bsscfg);
	int i;

	if (assoc_params != NULL && assoc_params->chanspec_num > 0) {
		for (i = 0; i < assoc_params->chanspec_num; i ++)
			if (chanspec == assoc_params->chanspec_list[i])
				return TRUE;
		return FALSE;
	}

	return TRUE;
}

/* scan finished with valid join_targets
 * loop through join targets and run all prune conditions
 *    if there is a one surviving, start join process.
 *    this function will be called if the join fails so next target(s) will be tried.
 */
static void
wlc_join_attempt(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	wlc_bss_info_t *bi;
	uint i;
	wlcband_t *target_band;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char ssidbuf[SSID_FMT_BUF_LEN];
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	uint32 wsec, WPA_auth;

	/* keep core awake until join finishes, as->state != AS_IDLE */
	ASSERT(STAY_AWAKE(wlc));
	wlc_set_wake_ctrl(wlc);

	wlc_assoc_timer_del(wlc, cfg);

	/* walk the list until there is a valid join target */
	for (; wlc->join_targets_last > 0; wlc->join_targets_last--) {
		wlc_rateset_t rateset;
		chanspec_t chanspec;
		wlc_bsscfg_t *bc = NULL;

		bi = wlc->join_targets->ptrs[wlc->join_targets_last - 1];
		target_band = wlc->bandstate[CHSPEC_WLCBANDUNIT(bi->chanspec)];
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
		wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);
#endif

		/* prune invalid chanspec based on STA capibility:
		 * not in NMODE or the locale does not allow 40MHz
		 * or the band is not configured for 40MHz operation
		 * Note that the unconditional version of the CHSPEC_IS40 is used so that
		 * code compiled without MIMO support will still recognize and convert
		 * a 40MHz chanspec.
		 */
		chanspec = bi->chanspec;

		/* Sanitize user setting for 80MHz against current settings
		 * Reduce an 80MHz chanspec to 40MHz if needed.
		 */
		if (CHSPEC_IS80_UNCOND(chanspec) &&
		    (!VHT_ENAB_BAND(wlc->pub, target_band->bandtype) ||
		     (wlc_channel_locale_flags_in_band(wlc->cmi, target_band->bandunit) &
		      WLC_NO_80MHZ) ||
		     !WL_BW_CAP_80MHZ(target_band->bw_cap))) {
			/* select the 40MHz primary channel in case 40 is allowed */
			chanspec = wf_chspec_primary40_chspec(chanspec);
		}

		if (CHSPEC_IS40_UNCOND(chanspec) &&
		    (!N_ENAB(wlc->pub) ||
		     (wlc_channel_locale_flags_in_band(wlc->cmi, target_band->bandunit) &
		      WLC_NO_40MHZ) || !WL_BW_CAP_40MHZ(target_band->bw_cap) ||
		     (BAND_2G(target_band->bandtype) && WLC_INTOL40_DET(cfg)))) {
			uint channel = wf_chspec_ctlchan(chanspec);
			chanspec = CH20MHZ_CHSPEC(channel);
		}
		if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
			WL_ASSOC(("wl%d: JOIN: Skipping invalid chanspec(0x%x):"
				" Control Channel %d; Bandwidth %s\n",
				WLCWLUNIT(wlc), chanspec, CHSPEC_CHANNEL(chanspec),
				CHSPEC_IS40(chanspec)?"40MHz":"20MHz"));
			continue;
		}

		/* validate BSS channel */
		if (!wlc_join_chanspec_filter(cfg,
			CH20MHZ_CHSPEC(wf_chspec_ctlchan(bi->chanspec)))) {
			WL_ASSOC(("wl%d: JOIN: Skipping BSS %s, mismatch chanspec %x\n",
			          WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf),
			          bi->chanspec));
			continue;
		}

		/* mSTA: Check here to make sure we don't end up with multiple bsscfgs
		 * associated to the same BSS. Skip this when PSTA mode is enabled.
		 */
		if (!PSTA_ENAB(wlc->pub)) {
			FOREACH_AS_STA(wlc, i, bc) {
				if (bc != cfg &&
				    bcmp(&bi->BSSID, &bc->BSSID, ETHER_ADDR_LEN) == 0)
					break;
				bc = NULL;
			}
			if (bc != NULL) {
				WL_ASSOC(("wl%d.%d: JOIN: Skipping BSS %s, "
				          "associated by bsscfg %d\n",
				          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
				          bcm_ether_ntoa(&bi->BSSID, eabuf),
				          WLC_BSSCFG_IDX(bc)));
				continue;
			}
		}

		/* derive WPA config if WPA auto config is on */
		if (WLCAUTOWPA(cfg)) {
			ASSERT(bi->wpacfg < join_pref->wpas);
			/* auth */
			cfg->auth = DOT11_OPEN_SYSTEM;
			/* WPA_auth */
			bcmwpa_akm2WPAauth(join_pref->wpa[bi->wpacfg].akm, &WPA_auth, FALSE);
			cfg->WPA_auth = (uint16)WPA_auth;
			/* wsec - unicast */
			bcmwpa_cipher2wsec(join_pref->wpa[bi->wpacfg].ucipher, &wsec);
			/*
			 * use multicast cipher only when unicast cipher is none, otherwise
			 * the next block (OID_802_11_ENCRYPTION_STATUS related) takes care of it
			 */
			if (!wsec) {
				uint8 mcs[WPA_SUITE_LEN];
				bcopy(join_pref->wpa[bi->wpacfg].mcipher, mcs, WPA_SUITE_LEN);
				if (!bcmp(mcs, WL_WPA_ACP_MCS_ANY, WPA_SUITE_LEN)) {
						mcs[DOT11_OUI_LEN] = bi->mcipher;
				}
				if (!bcmwpa_cipher2wsec(mcs, &wsec)) {
					WL_ASSOC(("JOIN: Skip BSS %s WPA cfg %d, cipher2wsec"
						" failed\n",
						bcm_ether_ntoa(&bi->BSSID, eabuf),
						bi->wpacfg));
					wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID,
						0, WLC_E_PRUNE_CIPHER_NA,
						CHSPEC_CHANNEL(bi->chanspec), 0, 0);
					continue;
				}
			}

			if (SUP_ENAB(wlc->pub) && cfg->sup_enable_wpa) {
				if (WSEC_AES_ENABLED(wsec))
					wsec |= TKIP_ENABLED;
				if (WSEC_TKIP_ENABLED(wsec))
					wsec |= WEP_ENABLED;
				/* merge rest flags */
#ifdef BCMWAPI_WPI
				wsec |= cfg->wsec & ~(AES_ENABLED | TKIP_ENABLED |
				                      WEP_ENABLED | SMS4_ENABLED);
#else
				wsec |= cfg->wsec & ~(AES_ENABLED | TKIP_ENABLED |
				                      WEP_ENABLED);
#endif /* BCMWAPI_WPI */
			}
			wlc_iovar_op(wlc, "wsec", NULL, 0, &wsec, sizeof(wsec),
				IOV_SET, cfg->wlcif);
			WL_ASSOC(("wl%d: JOIN: BSS %s wpa cfg %d WPA_auth 0x%x wsec 0x%x\n",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf),
				bi->wpacfg, cfg->WPA_auth, cfg->wsec));
			wlc_bss_mac_event(wlc, cfg, WLC_E_AUTOAUTH, &bi->BSSID,
				WLC_E_STATUS_ATTEMPT, 0, bi->wpacfg, 0, 0);
		}

		/* check Privacy (encryption) in target BSS */
		/*
		 * WiFi: A STA with WEP off should never attempt to associate with
		 * an AP that has WEP on
		 */
		if ((bi->capability & DOT11_CAP_PRIVACY) && !WSEC_ENABLED(cfg->wsec)) {
			if (cfg->is_WPS_enrollee) {
				WL_ASSOC(("wl%d: JOIN: Assuming join to BSSID %s is for WPS "
				          " credentials, so allowing unencrypted EAPOL frames\n",
				          WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
			} else {
				WL_ASSOC(("wl%d: JOIN: Skipping BSSID %s, encryption mandatory "
				          "in BSS, but encryption off for us.\n", WLCWLUNIT(wlc),
				          bcm_ether_ntoa(&bi->BSSID, eabuf)));
				wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				                  WLC_E_PRUNE_ENCR_MISMATCH,
				                  CHSPEC_CHANNEL(bi->chanspec), 0, 0);
				continue;
			}
		}
		/* skip broadcast bssid */
		if (ETHER_ISBCAST(bi->BSSID.octet)) {
			WL_ASSOC(("wl%d: JOIN: Skipping BSS with broadcast BSSID\n",
				WLCWLUNIT(wlc)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_BCAST_BSSID,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}
		/* prune join_targets based on allow/deny list */
		if (cfg->macmode) {
			/* check the allow/deny list */
			for (i = 0; i < cfg->nmac; i++) {
				if (bcmp(bi->BSSID.octet, (void*)&cfg->maclist[i],
					ETHER_ADDR_LEN) == 0)
					break;
			}
			/* prune if target is on the deny list or not on the allow list */
			if ((cfg->macmode == WLC_MACMODE_DENY && i < cfg->nmac) ||
			    (cfg->macmode == WLC_MACMODE_ALLOW && i >= cfg->nmac)) {
				if (cfg->macmode == WLC_MACMODE_DENY) {
					WL_ASSOC(("wl%d: JOIN: pruning BSSID %s because it "
					      "was on the MAC Deny list\n", WLCWLUNIT(wlc),
					      bcm_ether_ntoa(&bi->BSSID, eabuf)));
					wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID,
						0, WLC_E_PRUNE_MAC_DENY,
						CHSPEC_CHANNEL(bi->chanspec), 0, 0);
				} else {
					WL_ASSOC(("wl%d: JOIN: pruning BSSID %s because it "
					      "was not on the MAC Allow list\n", WLCWLUNIT(wlc),
					      bcm_ether_ntoa(&bi->BSSID, eabuf)));
					wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID,
						0, WLC_E_PRUNE_MAC_NA,
						CHSPEC_CHANNEL(bi->chanspec), 0, 0);
				}
				continue;
			}
		}


		/* prune if we are in strict SpectrumManagement mode, the AP is not advertising
		 * support, and the locale requires 802.11h SpectrumManagement
		 */
		if (WL11H_ENAB(wlc) && (wlc_11h_get_spect(wlc->m11h) == SPECT_MNGMT_STRICT_11H) &&
			bi->infra && (bi->capability & DOT11_CAP_SPECTRUM) == 0 &&
		    (wlc_channel_locale_flags_in_band(wlc->cmi, target_band->bandunit) &
		     WLC_DFS_TPC)) {
			WL_ASSOC(("wl%d: JOIN: pruning AP %s (SSID \"%s\", channel %d). "
			          "Current locale \"%s\" requires spectrum management "
			          "but AP does not have SpectrumManagement.\n",
			          WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf), ssidbuf,
			          CHSPEC_CHANNEL(bi->chanspec),
			          wlc_channel_country_abbrev(wlc->cmi)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_SPCT_MGMT,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}

		if (WL11H_ENAB(wlc) && bi->infra &&
		    wlc_csa_quiet_mode(wlc->csa, (uint8 *)bi->bcn_prb, bi->bcn_prb_len)) {
			WL_ASSOC(("JOIN: pruning AP %s (SSID \"%s\", channel %d). "
				"AP is CSA quiet period.\n",
				  bcm_ether_ntoa(&bi->BSSID, eabuf), ssidbuf,
				  CHSPEC_CHANNEL(bi->chanspec)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_SPCT_MGMT,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}

		/* prune if in 802.11h SpectrumManagement mode and the IBSS is on radar channel */
		if ((WL11H_ENAB(wlc)) && (bi->infra == 0) &&
			wlc_radar_chanspec(wlc->cmi, bi->chanspec)) {
			WL_ASSOC(("wl%d: JOIN: pruning IBSS \"%s\" channel %d since "
			      "we are in 802.11h mode and IBSS is on a radar channel in "
			      "locale \"%s\"\n", WLCWLUNIT(wlc),
			      ssidbuf, CHSPEC_CHANNEL(bi->chanspec),
			      wlc_channel_country_abbrev(wlc->cmi)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_RADAR,
			        CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}
		/* prune if the IBSS is on a restricted channel */
		if ((bi->infra == 0) && wlc_restricted_chanspec(wlc->cmi, bi->chanspec)) {
			WL_ASSOC(("wl%d: JOIN: pruning IBSS \"%s\" channel %d since is is "
			          "on a restricted channel in locale \"%s\"\n",
			          WLCWLUNIT(wlc), ssidbuf,
			          CHSPEC_CHANNEL(bi->chanspec),
			          wlc_channel_country_abbrev(wlc->cmi)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_REG_PASSV,
				CHSPEC_CHANNEL(bi->chanspec), 0, 0);
			continue;
		}

		/* prune join targets based on security settings */
		if (cfg->WPA_auth != WPA_AUTH_DISABLED && WSEC_ENABLED(cfg->wsec)) {
			if (wlc_join_wsec_filter(wlc, cfg, bi))
				continue;
		}

		/* prune based on rateset, bail out if no common rates with the BSS/IBSS
		 *  copy rateset to preserve original (CCK only if LegacyB)
		 *  Adjust Hardware rateset for target channel based on the channel bandwidth
		 *  filter-out unsupported rates
		 */
		if (BAND_2G(target_band->bandtype) && (target_band->gmode == GMODE_LEGACY_B))
			wlc_rateset_filter(&bi->rateset, &rateset, FALSE, WLC_RATES_CCK,
			                   RATE_MASK_FULL, 0);
		else
			wlc_rateset_filter(&bi->rateset, &rateset, FALSE, WLC_RATES_CCK_OFDM,
			                   RATE_MASK_FULL, wlc_get_mcsallow(wlc, cfg));
#ifdef WL11N
		wlc_rateset_bw_mcs_filter(&target_band->hw_rateset,
			WL_BW_CAP_40MHZ(wlc->band->bw_cap)?CHSPEC_WLC_BW(bi->chanspec):0);
#endif
		if (!wlc_rate_hwrs_filter_sort_validate(&rateset, &target_band->hw_rateset,
			FALSE, wlc->stf->txstreams)) {
			WL_ASSOC(("wl%d: JOIN: BSSID %s pruned because we don't have any rates "
			          "in common with the BSS\n", WLCWLUNIT(wlc),
			          bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_NO_COMMON_RATES, 0, 0, 0);
			continue;
		}

		/* pruned if device don't supported HT PHY membership */
		if (rateset.htphy_membership != wlc->htphy_membership) {
			WL_ASSOC(("wl%d: JOIN: BSSID %s pruned because HT PHY support don't match",
				WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
			continue;
		}

		/* skip any IBSS having basic rates which we do not support */
		if (bi->infra == 0 &&
		    !wlc_join_basicrate_supported(wlc, &bi->rateset,
			CHSPEC2WLC_BAND(bi->chanspec))) {
			WL_ASSOC(("wl%d: JOIN: BSSID %s pruned because we do not support all "
			      "Basic Rates of the BSS\n", WLCWLUNIT(wlc),
			      bcm_ether_ntoa(&bi->BSSID, eabuf)));
			wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				WLC_E_PRUNE_BASIC_RATES, 0, 0, 0);
			continue;
		}

		/* APSTA prune associated or peer STA2710s (no ambiguous roles) */
		if (APSTA_ENAB(wlc->pub) && (!WIN7_AND_UP_OS(wlc->pub) || AP_ACTIVE(wlc))) {
			struct scb *scb;
			scb = wlc_scbfind(wlc, cfg, &bi->BSSID);
			if ((scb == NULL) && (NBANDS(wlc) > 1))
				scb = wlc_scbfindband(wlc, cfg, &bi->BSSID, OTHERBANDUNIT(wlc));
			if (scb && SCB_ASSOCIATED(scb) && !(scb->flags & SCB_MYAP)) {
				WL_ASSOC(("wl%d: JOIN: BSSID %s pruned, it's a known STA\n",
				            WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
				wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
					WLC_E_PRUNE_KNOWN_STA, 0, 0, 0);
				continue;
			}
			if (scb && SCB_LEGACY_WDS(scb)) {
				WL_ASSOC(("wl%d: JOIN: BSSID %s pruned, it's a WDS peer\n",
				          WLCWLUNIT(wlc), bcm_ether_ntoa(&bi->BSSID, eabuf)));
				wlc_bss_mac_event(wlc, cfg, WLC_E_PRUNE, &bi->BSSID, 0,
				              WLC_E_PRUNE_WDS_PEER, 0, 0, 0);
				continue;
			}
		}

		/* reach here means BSS passed all the pruning tests, so break the loop to join */
		break;
	}

	if (wlc->join_targets_last > 0) {
		as->bss_retries = 0;

		bi = wlc->join_targets->ptrs[--wlc->join_targets_last];
		WL_ASSOC(("wl%d: JOIN: attempting BSSID: %s\n", WLCWLUNIT(wlc),
		            bcm_ether_ntoa(&bi->BSSID, eabuf)));

		if (as->type != AS_ASSOCIATION)
			wlc_bss_mac_event(wlc, cfg, WLC_E_ROAM_PREP, &bi->BSSID, 0, 0, 0, NULL, 0);


		{
#ifdef BCMSUP_PSK
			if (SUP_ENAB(wlc->pub) && (cfg->sup_type == SUP_WPAPSK)) {
				switch (wlc_sup_set_ssid(cfg->sup, bi->SSID, bi->SSID_len)) {
				case 1:
					/* defer association till psk passhash is done */
					wlc_assoc_change_state(cfg, AS_WAIT_PASSHASH);
					return;
				case 0:
					/* psk supplicant is config'd so continue */
					break;
				case -1:
				default:
					WL_ASSOC(("wl%d: wlc_sup_set_ssid failed, stop assoc\n",
					          wlc->pub->unit));
					wlc_assoc_done(cfg, WLC_E_STATUS_FAIL);
					return;
				}
			}
#endif /* BCMSUP_PSK */
			wlc_join_bss_prep(cfg);
		}
		return;
	}

	WL_ASSOC(("wl%d.%d: JOIN: no more join targets available\n",
	          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));

#ifdef WLP2P
	/* reset p2p assoc states only for non-roam cases */
	if (BSS_P2P_ENAB(wlc, cfg) && !cfg->associated)
		wlc_p2p_reset_bss(wlc->p2p, cfg);
#endif

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub) && (cfg->chan_context != NULL)) {
		WL_MCHAN(("wl%d.%d: %s: Delete chanctx for join failure\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		wlc_mchan_delete_bss_chan_context(wlc, cfg);
	}
#endif /* WLMCHAN */

	/* handle reassociation failure */
	if (cfg->associated) {
		wlc_roam_complete(cfg, WLC_E_STATUS_FAIL, NULL,
			WLC_DOT11_BSSTYPE(target_bss->infra));
	}
	/* handle association failure from a cache assisted assoc */
	else if (ASSOC_CACHE_ASSIST_ENAB(wlc) &&
	         (as->flags & (AS_F_CACHED_CHANNELS | AS_F_CACHED_RESULTS))) {
		/* on a cached assoc failure, retry after a full scan */
		wlc_assoc_cache_fail(cfg);
		return;
	}
	/* handle association failure */
	else {
		wlc_set_ssid_complete(cfg, WLC_E_STATUS_FAIL, NULL,
			WLC_DOT11_BSSTYPE(target_bss->infra));
	}

#if defined(BCMSUP_PSK)
	if (SUP_ENAB(wlc->pub) && (cfg->sup_type == SUP_WPAPSK)) {
		wlc_wpa_send_sup_status(cfg->sup, WLC_E_SUP_OTHER);
	}
#endif /* defined(BCMSUP_PSK) */

	wlc->block_datafifo &= ~DATA_BLOCK_JOIN;
	wlc_roam_release_flow_cntrl(cfg);
}

void
wlc_join_bss_prep(wlc_bsscfg_t *cfg)
{
	wlc_assoc_change_state(cfg, AS_JOIN_START);
	wlc_join_bss_start(cfg);
}

static void
wlc_join_bss_start(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_txq_info_t *qi = cfg->wlcif->qi;
	struct pktq *q = &qi->q;


	/* if roaming, make sure tx queue is drain out */
	if (as->type == AS_ROAM && wlc_bss_connected(cfg) &&
	    !wlc->block_datafifo &&
	    (!pktq_empty(q) || TXPKTPENDTOT(wlc) > 0)) {
		/* block tx path and roam to a new target AP only after all
		 * the pending tx packets sent out
		 */
		wlc_txflowcontrol_override(wlc, qi, ON, TXQ_STOP_FOR_PKT_DRAIN);
		wlc_assoc_change_state(cfg, AS_WAIT_TX_DRAIN);
		wl_add_timer(wlc->wl, as->timer, WLC_TXQ_DRAIN_TIMEOUT, 0);
		WL_ASSOC(("ROAM: waiting for %d tx packets drained out before roam\n",
			pktq_len(q) + TXPKTPENDTOT(wlc)));
	} else
		wlc_join_BSS(cfg, wlc->join_targets->ptrs[wlc->join_targets_last]);
}

#ifdef AP
/* Checks to see if we need to send out CSA for local ap cfgs before attempting to join a new AP.
 * Parameter description:
 * wlc - global wlc structure
 * cfg - the STA cfg that is about to perform a join a new AP
 * chanspec - the new chanspec the new AP is on
 * state - can be either AS_WAIT_FOR_AP_CSA or AS_WAIT_FOR_AP_CSA_ROAM_FAIL
 */
static bool
wlc_join_check_ap_need_csa(wlc_info_t *wlc, wlc_bsscfg_t *cfg, chanspec_t chanspec, uint state)
{
	wlc_assoc_t *as = cfg->assoc;
	bool need_csa = FALSE;
	chanspec_t curr_chanspec = WLC_BAND_PI_RADIO_CHANSPEC;

	/* If we're not in AS_WAIT_FOR_AP_CSA states, about to change channel
	 * and AP is active, allow AP to send CSA before performing the channel switch.
	 */
	if ((as->state != state) &&
	    (curr_chanspec != chanspec) &&
	    AP_ACTIVE(wlc)) {
		/* check if any stations associated to our APs */
		if (!wlc_scb_associated_to_ap(wlc, NULL)) {
			/* no stations associated to our APs, return false */
			WL_ASSOC(("wl%d.%d: %s: not doing ap CSA, no stas associated to ap(s)\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			return (FALSE);
		}
		if (wlc_join_ap_do_csa(wlc, chanspec)) {
			wlc_ap_mute(wlc, FALSE, cfg, -1);
			WL_ASSOC(("wl%d.%d: %s: doing ap CSA\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			wlc_assoc_change_state(cfg, state);
			need_csa = TRUE;
		}
	}
	else {
		WL_ASSOC(("wl%d.%d: %s: ap CSA not needed\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
	}
	return need_csa;
}

static bool
wlc_join_ap_do_csa(wlc_info_t *wlc, chanspec_t tgt_chanspec)
{
	wlc_bsscfg_t *apcfg;
	int apidx;
	bool ap_do_csa = FALSE;

	if (WL11H_ENAB(wlc)) {
		FOREACH_UP_AP(wlc, apidx, apcfg) {
			/* find all ap's on current channel */
			if ((WLC_BAND_PI_RADIO_CHANSPEC == apcfg->current_bss->chanspec) &&
#ifdef WLMCHAN
				(!MCHAN_ENAB(wlc->pub) || wlc_mchan_stago_is_disabled(wlc->mchan) ||
				BSSCFG_AP_MCHAN_DISABLED(wlc, apcfg))) {
#else
				1) {
#endif
				wlc_csa_do_switch(wlc->csa, apcfg, tgt_chanspec);
				ap_do_csa = TRUE;
				WL_ASSOC(("wl%d.%d: apply channel switch\n", wlc->pub->unit,
					apidx));
			}
		}
	}

	return ap_do_csa;
}
#endif /* AP */

void
wlc_join_BSS(wlc_bsscfg_t *cfg, wlc_bss_info_t *bi)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	struct scb *scb;
	void *pkt;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	bool cck_only;
	wlc_rateset_t *rs_hw;
	chanspec_t chanspec;
	wlcband_t *band;
	bool ht_wsec = TRUE;
	bool bss_ht, bss_vht;
	bool ch_changed = FALSE;
#if defined(BCMSUP_PSK) && defined(BCMINTSUP) && defined(WLFBT)
	bool ft_band_changed = TRUE;
#endif /* BCMSUP_PSK && BCMINTSUP && WLFBT */
	uint8 mcsallow = 0;

	BCM_REFERENCE(bss_vht);

	WL_ASSOC(("wl%d.%d: JOIN: %s: selected BSSID: %s\n", WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
	      __FUNCTION__, bcm_ether_ntoa(&bi->BSSID, eabuf)));

	if (bi->flags & WLC_BSS_HT) {
		/* HT and TKIP(WEP) cannot be used for HT STA. When AP advertises HT and TKIP */
		/* (or WEP)  only,	downgrade to leagacy STA */
		if (!WSEC_AES_ENABLED(cfg->wsec)) {
			if (WSEC_TKIP_ENABLED(cfg->wsec)) {
				if (wlc->ht_wsec_restriction & WLC_HT_TKIP_RESTRICT)
					ht_wsec = FALSE;
			} else if (WSEC_WEP_ENABLED(cfg->wsec)) {
				if (wlc->ht_wsec_restriction & WLC_HT_WEP_RESTRICT)
					ht_wsec = FALSE;
			}
		}
		/* safe mode: if AP is HT capable, downgrade to legacy mode because */
		/* NDIS won't associate to HT AP */
		if (!ht_wsec || BSSCFG_SAFEMODE(cfg)) {
			if (BSSCFG_SAFEMODE(cfg)) {
				WL_INFORM(("%s(): wl%d.%d: safe mode enabled, disable HT!\n",
					__FUNCTION__, WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
			}
			bi->flags &= ~(WLC_BSS_HT | WLC_BSS_40INTOL | WLC_BSS_SGI_20 |
				WLC_BSS_SGI_40 | WLC_BSS_40MHZ);
			bi->chanspec = CH20MHZ_CHSPEC(wf_chspec_ctlchan(bi->chanspec));

			/* disable VHT and SGI80 also */
			bi->flags2 &= ~(WLC_BSS_VHT | WLC_BSS_SGI_80);
		}
	}

#ifdef AP
	if (wlc_join_check_ap_need_csa(wlc, cfg, bi->chanspec, AS_WAIT_FOR_AP_CSA)) {
		WL_ASSOC(("wl%d.%d: JOIN: %s delayed due to ap active, wait for ap CSA\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return;
	}
#endif /* AP */


	chanspec = bi->chanspec;

	band = wlc->bandstate[CHSPEC_IS2G(chanspec) ? BAND_2G_INDEX : BAND_5G_INDEX];

	/* Sanitize user setting for 80MHz against current settings
	 * Reduce an 80MHz chanspec to 40MHz if needed.
	 */
	if (CHSPEC_IS80_UNCOND(chanspec) &&
	    (!(VHT_ENAB_BAND(wlc->pub, band->bandtype)) ||
	     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) & WLC_NO_80MHZ) ||
	     !WL_BW_CAP_80MHZ(band->bw_cap))) {
		/* select the 40MHz primary channel in case 40 is allowed */
		chanspec = wf_chspec_primary40_chspec(chanspec);
	}

	/* Convert a 40MHz AP channel to a 20MHz channel if we are not in NMODE or
	 * the locale does not allow 40MHz
	 * or the band is not configured for 40MHz operation
	 * Note that the unconditional version of the CHSPEC_IS40 is used so that
	 * code compiled without MIMO support will still recognize and convert
	 * a 40MHz chanspec.
	 */
	if (CHSPEC_IS40_UNCOND(chanspec) &&
	    (!N_ENAB(wlc->pub) ||
	     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) & WLC_NO_40MHZ) ||
	     !WL_BW_CAP_40MHZ(band->bw_cap) ||
	     (BAND_2G(band->bandtype) && WLC_INTOL40_DET(cfg)))) {
		uint channel;
		channel = wf_chspec_ctlchan(chanspec);
		chanspec = CH20MHZ_CHSPEC(channel);
	}

	wlc->block_datafifo |= DATA_BLOCK_JOIN;


#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub)) {
		WL_MCHAN(("wl%d.%d: %s: Creating chanctx for 0x%x\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, chanspec));
		if (wlc_mchan_create_bss_chan_context(wlc, cfg, chanspec)) {
			WL_ERROR(("wl%d: JOIN: Creating a new bss_chan_context failed\n",
				WLCWLUNIT(wlc)));
			return;
		}
	}
#endif

	/* Change the radio channel to match the target_bss */
	if ((WLC_BAND_PI_RADIO_CHANSPEC != chanspec) &&
		((as->type != AS_ROAM) ||
#ifdef WLFBT
		 /* Change the channel while switching to FBT over-the-air if no response
		  * received for FT Request frame from the current AP or if current AP
		  * is not reachable.
		  */
		 (WLFBT_ENAB(wlc->pub) && (target_bss->flags2 & WLC_BSS_OVERDS_FBT) &&
		  ((as->state == AS_SENT_FTREQ) || (roam->reason == WLC_E_REASON_BCNS_LOST))) ||
#endif /* WLFBT */
		 !WLFBT_ENAB(wlc->pub) || !(target_bss->flags2 & WLC_BSS_OVERDS_FBT))) {
		/* clear the quiet bit on the dest channel */
		wlc_clr_quiet_chanspec(wlc->cmi, chanspec);
		wlc_suspend_mac_and_wait(wlc);

#if defined(BCMSUP_PSK) && defined(BCMINTSUP) && defined(WLFBT)
		/* If we're likely to do FT reassoc, check if we're going to change bands */
		if (WLFBT_ENAB(wlc->pub) &&
			(as->type == AS_ROAM) && (cfg->auth == DOT11_OPEN_SYSTEM) &&
			wlc_sup_is_cur_mdid(cfg->sup, bi)) {
			if (CHSPEC_BAND(cfg->current_bss->chanspec) == CHSPEC_BAND(chanspec))
				ft_band_changed = FALSE;
		}
#endif /* BCMSUP_PSK && BCMINTSUP && WLFBT */
		wlc_set_chanspec(wlc, chanspec);
#ifdef WLMCHAN
		if (MCHAN_ENAB(wlc->pub) && cfg->chan_context->qi != wlc->primary_queue) {
			wlc_primary_queue_set(wlc, cfg->chan_context->qi);
		}
#endif
		wlc_enable_mac(wlc);
		ch_changed = TRUE;
	} else if (wlc_quiet_chanspec(wlc->cmi, chanspec)) {
		/* clear the quiet bit on our channel and un-quiet */
		wlc_clr_quiet_chanspec(wlc->cmi, chanspec);
		wlc_mute(wlc, OFF, 0);
	}

#ifdef WLMCHAN
	/* if we did not change channels we still might need to update
	 * the primay and active queue
	 */
	if (MCHAN_ENAB(wlc->pub) && cfg->chan_context->qi != wlc->primary_queue) {
		wlc_suspend_mac_and_wait(wlc);
		wlc_primary_queue_set(wlc, cfg->chan_context->qi);
		wlc_enable_mac(wlc);
	}
#endif

	rs_hw = &wlc->band->hw_rateset;

	/* select target bss info */
	bcopy((char*)bi, (char*)target_bss, sizeof(wlc_bss_info_t));

	/* update the target_bss->chanspec after possibe narrowing to 20MHz */
	target_bss->chanspec = chanspec;

	/* Keep only CCK if gmode == GMODE_LEGACY_B */
	if (BAND_2G(wlc->band->bandtype) && wlc->band->gmode == GMODE_LEGACY_B)
		cck_only = TRUE;
	else
		cck_only = FALSE;

	bss_ht = ((bi->flags & WLC_BSS_HT)) && BSS_N_ENAB(wlc, cfg);
	bss_vht = ((bi->flags2 & WLC_BSS_VHT)) && BSS_VHT_ENAB(wlc, cfg);

	if (bss_ht)
		mcsallow |= WLC_MCS_ALLOW;
	if (bss_vht)
		mcsallow |= WLC_MCS_ALLOW_VHT;

	wlc_rateset_filter(&target_bss->rateset, &target_bss->rateset,
	                   FALSE, cck_only ? WLC_RATES_CCK : WLC_RATES_CCK_OFDM,
	                   RATE_MASK_FULL, cck_only ? 0 : mcsallow);

	if (CHIPID(wlc->pub->sih->chip) == BCM43142_CHIP_ID) {
		uint i;
		for (i = 0; i < target_bss->rateset.count; i++) {
			uint8 rate;
			rate = target_bss->rateset.rates[i] & RATE_MASK;
			if ((rate == WLC_RATE_36M) || (rate == WLC_RATE_48M) ||
				(rate == WLC_RATE_54M))
				target_bss->rateset.rates[i] &= ~WLC_RATE_FLAG;
		}
	}

	/* apply default rateset to invalid rateset */
	if (!wlc_rate_hwrs_filter_sort_validate(&target_bss->rateset, rs_hw, TRUE,
		wlc->stf->txstreams)) {
		WL_RATE(("wl%d: %s: invalid rateset in target_bss. bandunit 0x%x phy_type 0x%x "
			"gmode 0x%x\n", wlc->pub->unit, __FUNCTION__, wlc->band->bandunit,
			wlc->band->phytype, wlc->band->gmode));
#ifdef BCMDBG
		wlc_rateset_show(wlc, &target_bss->rateset, &bi->BSSID);
#endif

		wlc_rateset_default(&target_bss->rateset, rs_hw, wlc->band->phytype,
			wlc->band->bandtype, cck_only, RATE_MASK_FULL,
			cck_only ? 0 : wlc_get_mcsallow(wlc, NULL),
			CHSPEC_WLC_BW(target_bss->chanspec), wlc->stf->op_rxstreams,
			WLC_VHT_FEATURES_RATES(wlc->pub));
	}

#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, cfg)) {
		wlc_rateset_filter(&target_bss->rateset, &target_bss->rateset, FALSE,
		                   WLC_RATES_OFDM, RATE_MASK_FULL, wlc_get_mcsallow(wlc, cfg));
	}
#endif

#ifdef BCMDBG
	wlc_rateset_show(wlc, &target_bss->rateset, &bi->BSSID);
#endif

	wlc_rate_lookup_init(wlc, &target_bss->rateset);

	if (WLCISACPHY(wlc->band)) {
		if (!(cfg->BSS && as->type == AS_RECREATE &&
			as->flags & AS_F_SPEEDY_RECREATE)) {
			WL_ASSOC(("wl%d: Call full phy cal from join_BSS\n",
				WLCWLUNIT(wlc)));
			wlc_full_phy_cal(wlc, cfg, PHY_PERICAL_JOIN_BSS);
		}
	}

	if ((WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band)) &&
#if defined(BCMSUP_PSK) && defined(BCMINTSUP) && defined(WLFBT)
		(WLFBT_ENAB(wlc->pub) && ft_band_changed) &&
#endif /* BCMSUP_PSK && BCMINTSUP && WLFBT */
		(as->type != AS_ROAM || ch_changed)) {
		/* phy full cal takes time and slow down reconnection after sleep.
		 * avoid double phy full cal if assoc recreation already did so
		 * and no channel changed(if AS_F_SPEEDY_RECREATE is still set)
		 */
		if (!(cfg->BSS && as->type == AS_RECREATE &&
		        as->flags & AS_F_SPEEDY_RECREATE))
			wlc_full_phy_cal(wlc, cfg, PHY_PERICAL_JOIN_BSS);
		if (NREV_GE(wlc->band->phyrev, 3) || WLCISHTPHY(wlc->band)) {
			wlc_phy_interference_set(wlc->band->pi, TRUE);

			wlc_phy_acimode_noisemode_reset(wlc->band->pi,
				CHSPEC_CHANNEL(chanspec), FALSE, TRUE, FALSE);
		}
	}

#ifdef WLP2P
	/* P2P: configure the BSS TSF, NoA, CTWindow, etc params of BSS */
	if (BSS_P2P_ENAB(wlc, cfg)) {
		wlc_p2p_adopt_bss(wlc->p2p, cfg, target_bss);
	}
#endif

	/* set infra mode */
	wlc_bsscfg_set_infra_mode(wlc, cfg, bi->infra != 0);

	/* attempt to associate with the selected BSS */
	if (bi->infra == 0) {
		/* IBSS join */

		/*
		 * nothing more to do from a protocol point of view...
		 * join the BSS when the next beacon is received...
		 */
		WL_ASSOC(("wl%d: JOIN: IBSS case, wait for a beacon\n", WLCWLUNIT(wlc)));
		/* configure the BSSID addr for STA w/o promisc beacon */
		if (D11REV_LT(wlc->pub->corerev, 40)) {
			wlc_bmac_set_addrmatch(wlc->hw, RCM_BSSID_OFFSET, &bi->BSSID);
		} else {
			wlc_bmac_write_amt(wlc->hw, AMT_IDX_BSSID, &bi->BSSID,
			                   (AMT_ATTR_VALID | AMT_ATTR_A3));
		}
		wlc_assoc_change_state(cfg, AS_WAIT_RCV_BCN);
		wlc_bsscfg_up(wlc, cfg);

	} else {
		/* BSS join */

		/* running out of scbs is fatal here */
		if (!(scb = wlc_scblookup(wlc, cfg, &bi->BSSID))) {
			WL_ERROR(("wl%d: %s: out of scbs\n", wlc->pub->unit, __FUNCTION__));
			return;
		}
		wlc_scb_set_bsscfg(scb, cfg);

		/* the cap and additional IE are only in the bcn/prb response pkts,
		 * when joining a bss parse the bcn_prb to check for these IE's.
		 * else check if SCB state needs to be cleared if AP might have changed its mode
		 */
		{
			ht_cap_ie_t *cap_ie = NULL;
			ht_add_ie_t *add_ie = NULL;

			if (bss_ht) {
				obss_params_t *obss_ie = NULL;
				uint len = target_bss->bcn_prb_len - sizeof(struct dot11_bcn_prb);
				uint8 *parse = (uint8*)target_bss->bcn_prb +
					sizeof(struct dot11_bcn_prb);

				/* extract ht cap and additional ie */
				cap_ie = wlc_read_ht_cap_ies(wlc, parse, len);
				add_ie = wlc_read_ht_add_ies(wlc, parse, len);
				if (COEX_ENAB(wlc->pub))
					obss_ie = wlc_ht_read_obss_scanparams_ie(wlc, parse, len);
				wlc_ht_update_scbstate(wlc, scb, cap_ie, add_ie, obss_ie);
			} else if ((scb->flags & SCB_HTCAP) &&
			           ((bi->flags & WLC_BSS_HT) != WLC_BSS_HT)) {
				wlc_ht_update_scbstate(wlc, scb, NULL, NULL, NULL);
			}
#ifdef WL11AC
			if (bss_vht) {
				vht_cap_ie_t *vht_cap_ie_p;
				vht_cap_ie_t vht_cap_ie;

				vht_op_ie_t *vht_op_ie_p;
				vht_op_ie_t vht_op_ie;

				uint8 *prop_tlv = NULL;
				int prop_tlv_len = 0;
				uint8 vht_ratemask = 0;
				int target_bss_band = CHSPEC2WLC_BAND(chanspec);

				uint len = target_bss->bcn_prb_len - sizeof(struct dot11_bcn_prb);
				uint8 *parse = (uint8*)target_bss->bcn_prb +
					sizeof(struct dot11_bcn_prb);

				/*
				 * Extract ht cap and additional ie
				 * Encapsulated Prop VHT IE appears if we are running VHT in 2.4G or
				 * the extended rates are enabled
				 */
				if (BAND_2G(target_bss_band) || WLC_VHT_FEATURES_RATES(wlc->pub)) {
					prop_tlv = wlc_read_vht_features_ie(wlc,
						parse, len, &vht_ratemask, &prop_tlv_len);
				}

				if  (prop_tlv) {
					vht_cap_ie_p = wlc_read_vht_cap_ie(wlc, prop_tlv,
						prop_tlv_len, &vht_cap_ie);
					vht_op_ie_p = wlc_read_vht_op_ie(wlc, prop_tlv,
						prop_tlv_len, &vht_op_ie);
				} else {
					vht_cap_ie_p =
						wlc_read_vht_cap_ie(wlc, parse, len, &vht_cap_ie);
					vht_op_ie_p =
						wlc_read_vht_op_ie(wlc, parse, len, &vht_op_ie);
				}
				wlc_vht_update_scbstate(wlc, target_bss_band, scb, cap_ie,
					vht_cap_ie_p, vht_op_ie_p, vht_ratemask);
			} else if ((scb->flags2 & SCB2_VHTCAP) &&
			           ((bi->flags2 & WLC_BSS_VHT) != WLC_BSS_VHT)) {
					wlc_vht_update_scbstate(wlc,
					CHSPEC2WLC_BAND(chanspec), scb, NULL, NULL, NULL, 0);
			}
#endif /* WL11AC */
		}

		/* just created or assigned an SCB for the AP, flag as MYAP */
		ASSERT(!(SCB_ASSOCIATED(scb) && !(scb->flags & SCB_MYAP)));
		scb->flags |= SCB_MYAP;

		/* replace any old scb rateset with new target rateset */
#ifdef WLP2P
		if (BSS_P2P_ENAB(wlc, cfg))
			wlc_rateset_filter(&target_bss->rateset, &scb->rateset, FALSE,
			                   WLC_RATES_OFDM, RATE_MASK, mcsallow);
		else
#endif
		wlc_rateset_filter(&target_bss->rateset, &scb->rateset, FALSE,
		                   WLC_RATES_CCK_OFDM, RATE_MASK, mcsallow);
		wlc_scb_ratesel_init(wlc, scb);

		wlc_assoc_timer_del(wlc, cfg);

#ifdef NOT_YET
		/* send (re)association request */
		if ((scb->state & (AUTHENTICATED | PENDING_ASSOC)) == AUTHENTICATED) {
			WL_ASSOC(("wl%d: JOIN: BSS case, sending %s REQ ...\n", WLCWLUNIT(wlc),
				(wlc->pub->associated ? "REASSOC" : "ASSOC")));
			wlc_scb_setstatebit(scb, PENDING_ASSOC);
			wlc_assoc_change_state(cfg, AS_SENT_ASSOC);
			pkt = wlc_sendassocreq(wlc, bi, scb, cfg->associated);
			if (pkt != NULL)
				wlc_pkt_callback_register(wlc, wlc_assocreq_complete,
				                          (void *)(uintptr)cfg->ID, pkt);
			else
				wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT + 10, 0);
		}
		else
#endif /* NOT_YET */

		/* send authentication request */
		if (!(scb->state & (PENDING_AUTH | PENDING_ASSOC))) {
			int auth = cfg->auth;
			cfg->auth_atmptd = auth;
#if defined(BCMSUP_PSK) && defined(BCMINTSUP) && defined(WLFBT)
			/* Make sure STA is associated to a valid BSSID before doing
			 * a fast transition.
			 */
			if (SUP_ENAB(wlc->pub) && WLFBT_ENAB(wlc->pub) &&
				(as->type == AS_ROAM) &&
				(auth == DOT11_OPEN_SYSTEM) &&
				wlc_sup_is_cur_mdid(cfg->sup, bi) &&
				!ETHER_ISNULLADDR(&cfg->current_bss->BSSID)) {
				auth = cfg->auth_atmptd = DOT11_FAST_BSS;
				wlc_sup_set_ea(cfg->sup, &bi->BSSID);
			}
#endif /* BCMSUP_PSK */
			WL_ASSOC(("wl%d: JOIN: BSS case, sending AUTH REQ alg=%d ...\n",
			            WLCWLUNIT(wlc), auth));
			wlc_scb_setstatebit(scb, PENDING_AUTH | PENDING_ASSOC);


			/* if we were roaming, mark the old BSSID so we don't thrash back to it */
			if (as->type == AS_ROAM &&
			    (roam->reason == WLC_E_REASON_MINTXRATE ||
			     roam->reason == WLC_E_REASON_TXFAIL)) {
				uint idx;
				for (idx = 0; idx < roam->cache_numentries; idx++) {
					if (!bcmp(&roam->cached_ap[idx].BSSID,
					          &cfg->current_bss->BSSID,
					          ETHER_ADDR_LEN)) {
						roam->cached_ap[idx].time_left_to_next_assoc =
						        ROAM_REASSOC_TIMEOUT;
						WL_ASSOC(("wl%d: %s: Marking current AP as "
						          "unavailable for reassociation for %d "
						          "seconds due to roam reason %d\n",
						          wlc->pub->unit, __FUNCTION__,
						          ROAM_REASSOC_TIMEOUT, roam->reason));
						roam->thrash_block_active = TRUE;
					}
				}
			}

#if defined(BCMSUP_PSK) && defined(WLFBT) && !defined(WLFBT_DISABLED)
			/* Skip FBT over-the-DS if current AP is not reachable or does not
			 * respond to FT Request frame and instead send auth frame to target AP.
			 */
			if ((as->type == AS_ROAM) && (as->state != AS_JOIN_START) &&
				(auth == DOT11_FAST_BSS) &&
				WLFBT_ENAB(wlc->pub) && (bi->flags2 & WLC_BSS_OVERDS_FBT) &&
				(roam->reason != WLC_E_REASON_BCNS_LOST) &&
				(as->state != AS_SENT_FTREQ)) {
				/* If not initial assoc (as->state != JOIN_START)
				   and AP is Over-the-DS-capable
				*/
				if (PS_ALLOWED(cfg))
					wlc_set_pmstate(cfg, FALSE);

				wlc_assoc_change_state(cfg, AS_SENT_FTREQ);

				pkt = wlc_sendftreq(cfg, &cfg->current_bss->BSSID, scb,
					(WSEC_SCB_KEY_VALID(scb) ? scb->key : NULL),
					((bi->capability & DOT11_CAP_SHORT) != 0));
			}
			else
#endif /* BCMSUP_PSK && WLFBT && !WLFBT_DISABLED */
				{
				wlc_assoc_change_state(cfg, AS_SENT_AUTH_1);
				pkt = wlc_sendauth(cfg, &scb->ea, &bi->BSSID, scb,
					auth, 1, DOT11_SC_SUCCESS, NULL, NULL,
					((bi->capability & DOT11_CAP_SHORT) != 0));
			}

#if defined(WLP2P) && defined(BCMDBG)
			if (WL_P2P_ON()) {
				int bss = wlc_mcnx_d11cb_idx(wlc->mcnx, cfg);
				uint16 state = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_ST(bss));
				uint16 next_noa = wlc_mcnx_read_shm(wlc->mcnx,
				                                   M_P2P_BSS_N_NOA(bss));
				uint16 hps = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_HPS);

				WL_P2P(("wl%d: %s: queue AUTH at tick 0x%x ST 0x%04X "
				        "N_NOA 0x%X HPS 0x%04X\n",
				        wlc->pub->unit, __FUNCTION__,
				        R_REG(wlc->osh, &wlc->regs->tsf_timerlow),
				        state, next_noa, hps));
			}
#endif /* WLP2P && BCMDBG */
			if (pkt != NULL)
				wlc_pkt_callback_register(wlc, wlc_auth_tx_complete,
				                          (void *)(uintptr)cfg->ID, pkt);
			else
				wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT + 10, 0);
		}
	}

	as->bss_retries ++;
}


void
wlc_assoc_timeout(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;

	WL_TRACE(("wl%d: wlc_timer", wlc->pub->unit));

	if (!wlc->pub->up)
		return;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

	if (ASSOC_RECREATE_ENAB(wlc->pub) &&
	    as->type == AS_RECREATE && as->state == AS_RECREATE_WAIT_RCV_BCN) {
		/* We reached the timeout waiting for a beacon from our former AP so take
		 * further action to reestablish our association.
		 */
		wlc_assoc_recreate_timeout(cfg);
	} else if (ASSOC_RECREATE_ENAB(wlc->pub) &&
	           as->type == AS_RECREATE && as->state == AS_ASSOC_VERIFY) {
		/* reset the association state machine */
		as->type = AS_IDLE;
		wlc_assoc_change_state(cfg, AS_IDLE);
#if defined(NDIS) && (NDISVER >= 0x0630)
		wlc_bss_mac_event(wlc, cfg, WLC_E_ASSOC_RECREATED, NULL,
			WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);
#endif /* (NDIS) && (NDISVER >= 0x0630) */
	} else if (as->state == AS_SENT_AUTH_1 ||
	           as->state == AS_SENT_AUTH_3 ||
	           as->state == AS_SENT_FTREQ) {
		wlc_auth_complete(cfg, WLC_E_STATUS_TIMEOUT, &target_bss->BSSID,
			0, cfg->auth);
	} else if (as->state == AS_SENT_ASSOC || as->state == AS_REASSOC_RETRY) {
		wlc_assoc_complete(cfg, WLC_E_STATUS_TIMEOUT, &target_bss->BSSID, 0,
			as->type != AS_ASSOCIATION,
			WLC_DOT11_BSSTYPE(target_bss->infra));
	} else if (as->state == AS_IDLE && wlc->sta_retry_time &&
	           BSSCFG_STA(cfg) && cfg->enable && !cfg->associated) {
		wlc_ssid_t ssid;
		/* STA retry: armed from wlc_set_ssid_complete() */
		WL_ASSOC(("wl%d: Retrying failed association\n", wlc->pub->unit));
		WL_APSTA_UPDN(("wl%d: wlc_assoc_timeout -> wlc_join()\n", wlc->pub->unit));
		ssid.SSID_len = cfg->SSID_len;
		bcopy(cfg->SSID, ssid.SSID, ssid.SSID_len);
		wlc_join(wlc, cfg, ssid.SSID, ssid.SSID_len, NULL, NULL, 0);
	} else if (as->state == AS_WAIT_TX_DRAIN) {
		WL_ASSOC(("ROAM: abort txq drain after %d ms\n", WLC_TXQ_DRAIN_TIMEOUT));
		wlc_assoc_change_state(cfg, AS_WAIT_TX_DRAIN_TIMEOUT);
		wlc_join_BSS(cfg, wlc->join_targets->ptrs[wlc->join_targets_last]);
	}
	return;
}

static void
wlc_assoc_recreate_timeout(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;

	WL_ASSOC(("wl%d: JOIN: RECREATE timeout waiting for former AP's beacon\n",
	          WLCWLUNIT(wlc)));

	/* Clear software BSSID information so that the current AP will be a valid roam target */
	wlc_bss_clear_bssid(cfg);

	wlc_update_bcn_info(cfg, FALSE);

	cfg->roam->reason = WLC_E_REASON_BCNS_LOST;

	if (WOWL_ENAB(wlc->pub) && cfg == wlc->cfg)
		cfg->roam->roam_on_wowl = TRUE;
	wlc_mac_request_entry(wlc, cfg, WLC_ACTION_RECREATE_ROAM);

	/* start the roam scan */
	wlc_assoc_scan_prep(cfg, NULL, NULL, NULL, 0);

}

void
wlc_join_recreate(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb = NULL;
	wlc_assoc_t *as = bsscfg->assoc;
	wlc_bss_info_t *bi = bsscfg->current_bss;
	int beacon_wait_time;
	wlc_roam_t *roam = bsscfg->roam;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
	char ssidbuf[SSID_FMT_BUF_LEN];
	char chanbuf[CHANSPEC_STR_LEN];
#endif

	/* validate that bsscfg and current_bss infrastructure/ibss mode flag match */
	ASSERT((bsscfg->BSS == (bi->infra == 1)));
	if (!(bsscfg->BSS == (bi->infra == 1))) {
		return;
	}

	/* these should already be set */
	if (!wlc->pub->associated || wlc->stas_associated == 0) {
		WL_ERROR(("wl%d: %s: both should have been TRUE: stas_assoc %d associated %d\n",
		          wlc->pub->unit, __FUNCTION__,
		          wlc->stas_associated, wlc->pub->associated));
	}

	/* Declare that driver is preserving the assoc */
	as->preserved = TRUE;

	if (bsscfg->BSS) {
		scb = wlc_scbfindband(wlc, bsscfg, &bi->BSSID, CHSPEC_WLCBANDUNIT(bi->chanspec));

		if (scb == NULL || (WLEXTSTA_ENAB(wlc->pub) && bsscfg->ar_disassoc)) {
			/* we lost our association - the AP no longer exists */
			wlc_assoc_init(bsscfg, AS_RECREATE);
			wlc_assoc_change_state(bsscfg, AS_LOSS_ASSOC);
		}
	}

	/* recreating an association to an AP */
	if (bsscfg->BSS && scb != NULL) {
		WL_ASSOC(("wl%d: JOIN: %s: recreating BSS association to ch %s %s %s\n",
		          WLCWLUNIT(wlc), __FUNCTION__,
		          wf_chspec_ntoa(bi->chanspec, chanbuf),
		          bcm_ether_ntoa(&bi->BSSID, eabuf),
		          (wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len), ssidbuf)));

		WL_ASSOC(("wl%d: %s: scb %s found\n", wlc->pub->unit, __FUNCTION__, eabuf));

		WL_ASSOC(("wl%d: JOIN: scb     State:%d Used:%d(%d)\n",
		          WLCWLUNIT(wlc),
		          scb->state, scb->used, (int)(scb->used - wlc->pub->now)));

		WL_ASSOC(("wl%d: JOIN: scb     Band:%s Flags:0x%x Key:%d  Cfg %p\n",
		          WLCWLUNIT(wlc),
		          ((scb->bandunit == BAND_2G_INDEX) ? BAND_2G_NAME : BAND_5G_NAME),
		          scb->flags, (scb->key ? scb->key->idx : -1), scb->bsscfg));

		WL_ASSOC(("wl%d: JOIN: scb     WPA_auth %d\n",
		          WLCWLUNIT(wlc), scb->WPA_auth));

		/* update scb state */
		/* 	scb->flags &= ~SCB_SHORTPREAMBLE; */
		/* 	if ((bi->capability & DOT11_CAP_SHORT) != 0) */
		/* 		scb->flags |= SCB_SHORTPREAMBLE; */
		/* 	scb->flags |= SCB_MYAP; */
		/* 	wlc_scb_setstatebit(scb, AUTHENTICATED | ASSOCIATED); */

		if (!(scb->flags & SCB_MYAP))
			WL_ERROR(("wl%d: %s: SCB_MYAP 0x%x not set in flags 0x%x!\n",
			          WLCWLUNIT(wlc), __FUNCTION__, SCB_MYAP, scb->flags));
		if ((scb->state & (AUTHENTICATED | ASSOCIATED)) != (AUTHENTICATED | ASSOCIATED))
			WL_ERROR(("wl%d: %s: (AUTHENTICATED | ASSOCIATED) 0x%x "
				"not set in scb->state 0x%x!\n",
				WLCWLUNIT(wlc), __FUNCTION__,
				(AUTHENTICATED | ASSOCIATED), scb->state));

		WL_ASSOC(("wl%d: JOIN: AID 0x%04x\n", WLCWLUNIT(wlc), bsscfg->AID));

		/*
		 * security setup
		 */
		if (scb->WPA_auth != bsscfg->WPA_auth)
			WL_ERROR(("wl%d: %s: scb->WPA_auth 0x%x "
				  "does not match bsscfg->WPA_auth 0x%x!",
				  WLCWLUNIT(wlc), __FUNCTION__, scb->WPA_auth, bsscfg->WPA_auth));

		/* disable txq processing until we are fully resynced with AP */
		wlc->block_datafifo |= DATA_BLOCK_JOIN;
	} else if (!bsscfg->BSS) {
		WL_ASSOC(("wl%d: JOIN: %s: recreating IBSS association to ch %s %s %s\n",
		          WLCWLUNIT(wlc), __FUNCTION__,
		          wf_chspec_ntoa(bi->chanspec, chanbuf),
		          bcm_ether_ntoa(&bi->BSSID, eabuf),
		          (wlc_format_ssid(ssidbuf, bi->SSID, bi->SSID_len), ssidbuf)));

		/* keep IBSS link indication up during recreation by resetting
		 * time_since_bcn
		 */
		roam->time_since_bcn = 0;
		roam->bcns_lost = FALSE;
	}

	/* initialize ucode default key flag MHF1_DEFKEYVALID */
	wlc_mhf(wlc, MHF1, MHF1_DEFKEYVALID, wlc_key_defkeyflag(wlc), WLC_BAND_ALL);

	/* suspend the MAC and configure the BSS/IBSS */
	wlc_suspend_mac_and_wait(wlc);
	wlc_BSSinit(wlc, bi, bsscfg, bsscfg->BSS ? WLC_BSS_JOIN : WLC_BSS_START);
	wlc_enable_mac(wlc);


#ifdef WLMCNX
	/* init multi-connection assoc states */
	if (MCNX_ENAB(wlc->pub) && bsscfg->BSS)
		wlc_mcnx_assoc_upd(wlc->mcnx, bsscfg, TRUE);
#endif

	wlc_scb_ratesel_init_bss(wlc, bsscfg);

	/* clean up assoc recreate preserve flag */
	bsscfg->flags &= ~WLC_BSSCFG_PRESERVE;

	/* force a PHY cal on the current BSS/IBSS channel (channel set in wlc_BSSinit() */
	if (WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) {
		WL_ASSOC(("wl%d: Calling Full PHY cal on recreating association\n",
		          WLCWLUNIT(wlc)));
		wlc_full_phy_cal(wlc, bsscfg,
		                 (bsscfg->BSS ? PHY_PERICAL_JOIN_BSS : PHY_PERICAL_START_IBSS));
	}

	wlc_bsscfg_up(wlc, bsscfg);

	/* if we are recreating an IBSS, we are done */
	if (!bsscfg->BSS) {
		/* should bump the first beacon TBTT out a few BIs to give the opportunity
		 * of a coalesce before our first beacon
		 */
		return;
	}

	if (as->type == AS_RECREATE && as->state == AS_LOSS_ASSOC) {
		/* Clear software BSSID information so that the current AP
		 * will be a valid roam target
		 */
		wlc_bss_clear_bssid(bsscfg);

		wlc_update_bcn_info(bsscfg, FALSE);

		roam->reason = WLC_E_REASON_INITIAL_ASSOC;

		wlc_mac_request_entry(wlc, bsscfg, WLC_ACTION_RECREATE);

		/* make sure the association retry timer is not pending */
		wlc_assoc_timer_del(wlc, bsscfg);

		/* start the roam scan */
#if defined(NDIS) && (NDISVER == 0x0620)
		/* to speed up fast-reconnect, should skip assoc-recreate full scan here */
		if (!wl_fast_scan_enabled(wlc->wl, NULL))
#endif
		as->flags |= AS_F_SPEEDY_RECREATE;
		wlc_assoc_scan_prep(bsscfg, NULL, NULL, &bi->chanspec, 1);
	}
	else {
		/* if we are recreating a BSS, wait for the first beacon */

		wlc_assoc_init(bsscfg, AS_RECREATE);
		wlc_assoc_change_state(bsscfg, AS_RECREATE_WAIT_RCV_BCN);

		/* update PS control for any PM changes and to force wake for assoc state machine */
		wlc_set_ps_ctrl(bsscfg);

		/* Set the timer to move the assoc recreate along
		 * if we do not see a beacon in our BSS/IBSS.
		 * Allow assoc_recreate_bi_timeout beacon intervals plus some slop to allow
		 * for medium access delay for the last beacon.
		 */
		beacon_wait_time = as->recreate_bi_timeout *
			(bi->beacon_period * DOT11_TU_TO_US) / 1000;
		beacon_wait_time += wlc->bcn_wait_prd; /* allow medium access delay */

		WL_ASSOC(("wl%d: %s: as type %d, as state %d, initial wait time %d\n",
		    WLCWLUNIT(wlc), __FUNCTION__,
		    as->type,
		    as->state,
		    beacon_wait_time));

		wlc_assoc_timer_del(wlc, bsscfg);
		wl_add_timer(wlc->wl, as->timer, beacon_wait_time, 0);
	}
}

int
wlc_assoc_abort(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	struct scb *scb;
	int callbacks = 0;
#ifdef MACOSX
	bool is_macos = TRUE;
#endif
#if defined(MACOSX)
	bool is_recreate;
#endif
	bool do_cmplt = FALSE;
	/* Save current band unit */
	int curr_bandunit = (int)wlc->band->bandunit;

	if (as->state == AS_IDLE)
		return 0;

	if (as->type == AS_ASSOCIATION)
		WL_ASSOC(("wl%d: JOIN: Aborting association in progress\n", WLCWLUNIT(wlc)));
	else if (as->type == AS_ROAM)
		WL_ASSOC(("wl%d: ROAM: Aborting roam in progress\n", WLCWLUNIT(wlc)));
	else if (as->type == AS_RECREATE)
		WL_ASSOC(("wl%d: ASSOC_RECREATE: Aborting association re-creation in progress\n",
		          WLCWLUNIT(wlc)));

	if (wlc_scan_inprog(wlc)) {
		WL_ASSOC(("wl%d: wlc_assoc_abort: aborting association scan in process\n",
		            WLCWLUNIT(wlc)));
		wlc_scan_abort(wlc->scan, WLC_E_STATUS_ABORT);
	}

	if (as->state == AS_SENT_AUTH_1 ||
	    as->state == AS_SENT_AUTH_3 ||
		as->state == AS_SENT_FTREQ ||
	    as->state == AS_SENT_ASSOC ||
	    as->state == AS_REASSOC_RETRY ||
	    (ASSOC_RECREATE_ENAB(wlc->pub) &&
	     (as->state == AS_RECREATE_WAIT_RCV_BCN ||
	      as->state == AS_ASSOC_VERIFY))) {
		if (!wl_del_timer(wlc->wl, as->timer)) {
			as->rt = FALSE;
			callbacks ++;
		}
	}
#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	else if (SUP_ENAB(wlc->pub) && (as->state == AS_WAIT_PASSHASH))
		callbacks += wlc_sup_down(cfg->sup);
#endif

#if defined(MACOSX)
	if (WLEXTSTA_ENAB(wlc->pub) || is_macos) {
		is_recreate = (ASSOC_RECREATE_ENAB(wlc->pub) &&
		               as->type == AS_RECREATE &&
		               (as->state == AS_RECREATE_WAIT_RCV_BCN ||
		                as->state == AS_ASSOC_VERIFY));
		if (!is_recreate) {
			if (as->state > AS_SCAN && as->state < AS_WAIT_DISASSOC) {
				/* indicate association completion */
				/* N.B.: assoc_state check passed through assoc_status parameter */
				wlc_assoc_complete(cfg, WLC_E_STATUS_ABORT, &target_bss->BSSID,
					as->state > AS_WAIT_TX_DRAIN && as->state < AS_WAIT_RCV_BCN,
					as->type != AS_ASSOCIATION,
					WLC_DOT11_BSSTYPE(target_bss->infra));
			}

			/* indicate connection or roam completion */
			wlc_assoc_done(cfg, WLC_E_STATUS_ABORT);
			do_cmplt = TRUE;

		}
	}
	else
#endif 
	if (as->type == AS_ASSOCIATION) {
		if (as->state == AS_SENT_AUTH_1 ||
			as->state == AS_SENT_AUTH_3 ||
			as->state == AS_SENT_FTREQ) {
			wlc_auth_complete(cfg, WLC_E_STATUS_ABORT, &target_bss->BSSID,
			                  0, cfg->auth);
		} else if (as->state == AS_SENT_ASSOC ||
		           as->state == AS_REASSOC_RETRY) {
			wlc_assoc_complete(cfg, WLC_E_STATUS_ABORT, &target_bss->BSSID,
			                   0, as->type != AS_ASSOCIATION, 0);
		}
	}

	if (curr_bandunit != (int)wlc->band->bandunit) {
		WL_INFORM(("wl%d: wlc->band changed since entering %s\n",
		   WLCWLUNIT(wlc), __FUNCTION__));
		WL_INFORM(("wl%d: %s: curr_bandunit = %d, wlc->band->bandunit = %d\n",
		   WLCWLUNIT(wlc), __FUNCTION__, curr_bandunit, wlc->band->bandunit));
	}
	/* Use wlc_scbfindband here in case wlc->band has changed since entering this function */
	/*
	 * When roaming, function wlc_assoc_done() called above can change the wlc->band *
	 * if AP and APSTA are both enabled. *
	 * In such a case, we will not be able to locate the correct scb entry if we use *
	 * wlc_scbfind() instead of wlc_scbfindband(). *
	 */
	if ((scb = wlc_scbfindband(wlc, cfg, &target_bss->BSSID, curr_bandunit)))
		/* Clear pending bits */
		wlc_scb_clearstatebit(scb, PENDING_AUTH | PENDING_ASSOC);

	wlc_roam_release_flow_cntrl(cfg);

	if (!do_cmplt) {
		wlc_bss_list_free(wlc, wlc->join_targets);

		as->type = AS_IDLE;
		wlc_assoc_change_state(cfg, AS_IDLE);

		/* APSTA: complete any deferred AP bringup */
		if (AP_ENAB(wlc->pub) && APSTA_ENAB(wlc->pub))
			wlc_restart_ap(wlc->ap);
	}
	/* check for join data block, if set, clear it */
	if (wlc->block_datafifo & DATA_BLOCK_JOIN) {
		wlc->block_datafifo &= ~DATA_BLOCK_JOIN;
	}

	return callbacks;
}

static void
wlc_assoc_init(wlc_bsscfg_t *cfg, uint type)
{
	wlc_assoc_t *as = cfg->assoc;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_info_t *wlc = cfg->wlc;

	ASSERT(type == AS_ROAM || type == AS_ASSOCIATION || type == AS_RECREATE);

	WL_ASSOC(("wl%d.%d: %s: assoc state machine init to assoc->type %d %s\n",
	          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__, type, WLCASTYPEN(type)));
#endif /* BCMDBG || WLMSG_ASSOC */

	as->type = type;
	as->flags = 0;

#ifdef PSTA
	/* Clean up the proxy STAs before starting scan */
	if (PSTA_ENAB(cfg->wlc->pub) && (type == AS_ROAM) &&
	    (cfg == wlc_bsscfg_primary(cfg->wlc)))
		wlc_psta_disassoc_all(cfg->wlc->psta);
#endif /* PSTA */

#ifdef WL_BCN_COALESCING
	wlc_bcn_clsg_disable(cfg->wlc->bc, BCN_CLSG_ASSOC_MASK, BCN_CLSG_ASSOC_MASK);
#endif /* WL_BCN_COALESCING */

	return;
}

void
wlc_assoc_change_state(wlc_bsscfg_t *cfg, uint newstate)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	uint oldstate = as->state;

	if (newstate > AS_LAST_STATE) {
		WL_ERROR(("wl%d.%d: %s: out of bounds assoc state %d\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__, newstate));
		ASSERT(0);
	}

	if (newstate == oldstate)
		return;

	WL_ASSOC(("wl%d.%d: wlc_assoc_change_state: change assoc_state from %s to %s\n",
	          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg),
	          wlc_as_st_name(oldstate), wlc_as_st_name(newstate)));

	as->state = newstate;

	/* optimization: nothing to be done between AS_IDLE and AS_CAN_YIELD() states */
	if ((oldstate == AS_IDLE && AS_CAN_YIELD(cfg->BSS, newstate)) ||
	    (AS_CAN_YIELD(cfg->BSS, oldstate) && newstate == AS_IDLE))
		return;

	/* transition from IDLE or from a state equavilent to IDLE. */
	if (oldstate == AS_IDLE || AS_CAN_YIELD(cfg->BSS, oldstate)) {
		/* move out of IDLE */
		wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_ASSOC, TRUE);

		/* add the new request on top, or move existing request back on top
		 * in case the abort operation has removed it
		 */
		wlc_assoc_req_add_entry(wlc, cfg, as->type, TRUE);
	}
	/* transition to IDLE or to a state equavilent to IDLE */
	else if ((newstate == AS_IDLE || AS_CAN_YIELD(cfg->BSS, newstate)) &&
	         wlc_assoc_req_remove_entry(wlc, cfg) == BCME_OK) {
		/* the current assoc process does no longer need these variables/states,
		 * reset them to prepare for the next one.
		 * - join targets
		 */
		wlc_bss_list_free(wlc, wlc->join_targets);

		/* move into IDLE */
		wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_ASSOC, FALSE);

		/* start the next request processing if any */
		wlc_assoc_req_process_next(wlc);

		/* allow the core to sleep if we were waiting on assoc_state */
		wlc_set_ps_ctrl(cfg);
	}
}

/* given the current chanspec, select a non-radar chansepc */
static chanspec_t
wlc_sradar_ap_select_channel(wlc_info_t *wlc, chanspec_t curr_chanspec)
{
	uint rand_idx;
	int listlen, noradar_listlen = 0, i;
	chanspec_t newchspec = 0;
	const char *abbrev = wlc_channel_country_abbrev(wlc->cmi);
	wl_uint32_list_t *list, *noradar_list = NULL;
	bool bw20 = FALSE, ch2g = FALSE;

	/* if curr_chanspec is non-radar, just return it and do nothing */
	if (!wlc_radar_chanspec(wlc->cmi, curr_chanspec)) {
		return (curr_chanspec);
	}

	/* use current chanspec to determine which valid */
	/* channels to look for */
	if (CHSPEC_IS2G(curr_chanspec) || (curr_chanspec == 0)) {
		ch2g = TRUE;
	}
	if (CHSPEC_IS5G(curr_chanspec) || (curr_chanspec == 0)) {
		ch2g = FALSE;
	}
	if (CHSPEC_IS20(curr_chanspec) || (curr_chanspec == 0)) {
		bw20 = TRUE;
	}
	if (CHSPEC_IS40(curr_chanspec) || (curr_chanspec == 0)) {
		bw20 = FALSE;
	}
	/* allocate memory for list */
	listlen =
		OFFSETOF(wl_uint32_list_t, element)
		+ sizeof(list->element[0]) * MAXCHANNEL;

	if ((list = MALLOC(wlc->osh, listlen)) == NULL) {
		WL_ERROR(("wl%d: %s(%d): failed to allocate list\n",
		  wlc->pub->unit, __FUNCTION__, __LINE__));
	} else {
		/* get a list of valid channels */
		list->count = 0;
		wlc_get_valid_chanspecs(wlc->cmi, list,
			bw20 ? WL_CHANSPEC_BW_20 : WL_CHANSPEC_BW_40, ch2g, abbrev);
	}

	/* This code builds a non-radar channel list out of the valid list */
	/* and picks one randomly (preferred option) */
	if (list && list->count) {
		/* build a noradar_list */
		noradar_listlen =
			OFFSETOF(wl_uint32_list_t, element) +
			sizeof(list->element[0]) * list->count;

		if ((noradar_list = MALLOC(wlc->osh, noradar_listlen)) == NULL) {
			WL_ERROR(("wl%d: %s(%d): failed to allocate noradar_list\n",
			  wlc->pub->unit, __FUNCTION__, __LINE__));
		} else {
			noradar_list->count = 0;
			for (i = 0; i < (int)list->count; i++) {
				if (!wlc_radar_chanspec(wlc->cmi,
					(chanspec_t)list->element[i])) {
					/* add to noradar_list */
					noradar_list->element[noradar_list->count++]
						= list->element[i];
				}
			}
		}
		if (noradar_list && noradar_list->count) {
			/* randomly pick a channel in noradar_list */
			rand_idx = R_REG(wlc->osh, &wlc->regs->u.d11regs.tsf_random)
				% noradar_list->count;
			newchspec = (chanspec_t)noradar_list->element[rand_idx];
		}
	}
	if (list) {
		/* free list */
		MFREE(wlc->osh, list, listlen);
	}
	if (noradar_list) {
		/* free noradar_list */
		MFREE(wlc->osh, noradar_list, noradar_listlen);
	}

	return (newchspec);
}

/* checks for sradar enabled AP. */
/* if one found, check current_bss chanspec */
/* if chanspec on radar channel, randomly select non-radar channel */
/* and move to it */
static void
wlc_sradar_ap_update(wlc_info_t *wlc)
{
	wlc_bsscfg_t *cfg;
	int idx;
	chanspec_t newchspec, chanspec = 0;
	bool move_ap = FALSE;

	/* See if we have a sradar ap on radar channel */
	FOREACH_UP_AP(wlc, idx, cfg) {
		if (BSSCFG_SRADAR_ENAB(cfg) &&
		    wlc_radar_chanspec(wlc->cmi, cfg->current_bss->chanspec)) {
			/* use current chanspec to determine which valid */
			/* channels to look for */
			chanspec = cfg->current_bss->chanspec;
			/* set flag to move ap */
			move_ap = TRUE;
			/* only need to do this for first matching up ap */
			break;
		}
	}

	if (move_ap == FALSE) {
		/* no sradar ap on radar channel, do nothing */
		return;
	}

	/* find a non-radar channel to move to */
	newchspec = wlc_sradar_ap_select_channel(wlc, chanspec);
	/* if no non-radar channel found, disable sradar ap on radar channel */
	FOREACH_UP_AP(wlc, idx, cfg) {
		if (BSSCFG_SRADAR_ENAB(cfg) &&
		    wlc_radar_chanspec(wlc->cmi, cfg->current_bss->chanspec)) {
			if (newchspec) {
				/* This code performs a channel switch immediately */
				/* w/o sending csa action frame and csa IE in beacon, prb_resp */
				wlc_do_chanswitch(cfg, newchspec);
			} else {
				/* can't find valid non-radar channel */
				/* shutdown ap */
				WL_ERROR(("%s: no radar channel found, disable ap\n",
				          __FUNCTION__));
				wlc_bsscfg_disable(wlc, cfg);
			}
		}
	}
}

/* update STA association state */
void
wlc_sta_assoc_upd(wlc_bsscfg_t *cfg, bool state)
{
	wlc_info_t *wlc = cfg->wlc;
	int idx;
	wlc_bsscfg_t *bc;
	bool prev_associated = cfg->associated;

	cfg->associated = state;

#ifdef WLMCHAN
	if (MCHAN_ENAB(wlc->pub)) {
		if (state == FALSE) {
			wlc_mchan_delete_bss_chan_context(wlc, cfg);
		}
		/* If chan_context doesn't exist at this point, create one. */
		else if (cfg->chan_context == NULL) {
			wlc_mchan_create_bss_chan_context(wlc, cfg, cfg->current_bss->chanspec);
		}
	}
#endif /* WLMCHAN */

	cfg->associated = prev_associated;

	/* STA is no longer associated, reset related states.
	 */
	if (!state) {
		wlc_reset_pmstate(cfg);
	}
	/* STA is assoicated, set related states that could have been
	 * missing before cfg->associated is set...
	 */
	else {
		wlc_set_pmawakebcn(cfg, wlc->PMawakebcn);
	}

	cfg->associated = state;

	wlc->stas_associated = 0;
	wlc->ibss_bsscfgs = 0;
	FOREACH_AS_STA(wlc, idx, bc) {
	        wlc->stas_associated ++;
		if (!bc->BSS)
			wlc->ibss_bsscfgs ++;
	}

	wlc->pub->associated = (wlc->stas_associated != 0 || wlc->aps_associated != 0);

	wlc_phy_hold_upd(wlc->band->pi, PHY_HOLD_FOR_NOT_ASSOC,
	                 wlc->pub->associated ? FALSE : TRUE);

	wlc->stas_connected = wlc_stas_connected(wlc);

	/* change the watchdog driver */
	wlc_watchdog_upd(cfg, PS_ALLOWED(cfg));

	wlc_enable_btc_ps_protection(wlc, cfg, state);

	/* if no station associated and we have ap up, check channel */
	/* if radar channel, move off to non-radar channel */
	if (WL11H_ENAB(wlc) && AP_ACTIVE(wlc) && wlc->stas_associated == 0) {
		wlc_sradar_ap_update(wlc);
	}

}

static void
wlc_join_adopt_bss(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	wlcband_t *band;
	uint reason = 0;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char ssidbuf[SSID_FMT_BUF_LEN];
	wlc_format_ssid(ssidbuf, target_bss->SSID, target_bss->SSID_len);
#endif /* BCMDBG || WLMSG_ASSOC */

	if (target_bss->infra) {
		WL_ASSOC(("wl%d: JOIN: join BSS \"%s\" on channel %d\n", WLCWLUNIT(wlc),
		            ssidbuf, CHSPEC_CHANNEL(target_bss->chanspec)));
	} else {
		WL_ASSOC(("wl%d: JOIN: join IBSS \"%s\" on channel %d\n", WLCWLUNIT(wlc),
		            ssidbuf, CHSPEC_CHANNEL(target_bss->chanspec)));
	}

	wlc->block_datafifo &= ~DATA_BLOCK_JOIN;

	roam->RSSIref = 0; /* this will be reset on the first incoming frame */

	if (as->type == AS_ASSOCIATION) {
		roam->reason = WLC_E_REASON_INITIAL_ASSOC;
		WL_ASSOC(("wl%d: ROAM: roam_reason cleared to 0x%x\n",
			wlc->pub->unit, WLC_E_REASON_INITIAL_ASSOC));
	}

	/* restore roam parameters to default */
	band = wlc->band;
	band->roam_trigger = band->roam_trigger_def;
	band->roam_delta = band->roam_delta_def;
	if (NBANDS(wlc) > 1) {
		band = wlc->bandstate[OTHERBANDUNIT(wlc)];
		band->roam_trigger = band->roam_trigger_def;
		band->roam_delta = band->roam_delta_def;
	}


#ifdef WLMCNX
	/* stop TSF adjustment - do it before setting BSSID to prevent ucode
	 * from adjusting TSF before we program the TSF block
	 */
	if (MCNX_ENAB(wlc->pub) && cfg->BSS)
		wlc_skip_adjtsf(wlc, TRUE, cfg, -1, WLC_BAND_ALL);
#endif

	/* suspend the MAC and join the BSS */
	wlc_suspend_mac_and_wait(wlc);
	wlc_BSSinit(wlc, target_bss, cfg, WLC_BSS_JOIN);
	wlc_enable_mac(wlc);

	wlc_sta_assoc_upd(cfg, TRUE);
	WL_RTDC(wlc, "wlc_join_adopt_bss: associated", 0, 0);

	/* Apply the STA AC params sent by AP */
	if (BSS_WME_AS(wlc, cfg)) {
		WL_ASSOC(("wl%d.%d: adopting WME AC params...\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));
		wlc_edcf_acp_apply(wlc, cfg, TRUE);
	}

#ifdef WLMCNX
	/* init p2p assoc states */
	if (MCNX_ENAB(wlc->pub) && cfg->BSS)
		wlc_mcnx_assoc_upd(wlc->mcnx, cfg, TRUE);
#endif

	if (N_ENAB(wlc->pub) && COEX_ENAB(wlc->pub))
		wlc_ht_obss_scan_reset(cfg);

	WL_APSTA_UPDN(("wl%d: Reporting link up on config 0 for STA joining a BSS\n",
	            WLCWLUNIT(wlc)));

#if defined(WLFBT)
	if (WLFBT_ENAB(wlc->pub) && (IS_WPA2_AUTH(cfg->WPA_auth)) &&
		(cfg->auth_atmptd == DOT11_FAST_BSS)) {
		reason = WLC_E_REASON_REQUESTED_ROAM;  /* any non-zero reason is okay here */
	}
#endif /* WLFBT */

	wlc_link(wlc, TRUE, &cfg->BSSID, cfg, reason);


#ifdef DWDS
	if (DWDS_ENAB(cfg)) {
		struct scb *scb;

		scb = wlc_scbfind(wlc, cfg, &cfg->BSSID);
		if ((scb != NULL) && SCB_DWDS(scb)) {
			/* make this scb to do 4-addr data frame from now */
			SCB_A4_DATA_ENABLE(scb);
		}
	}
#endif

#if defined(BCMSUP_PSK) && defined(BCMINTSUP)
	if (SUP_ENAB(wlc->pub) && (cfg->sup_type != SUP_UNUSED)) {
		bool sup_stat;
		bool fast_reassoc = FALSE;

#ifdef BCMSUP_PSK
#if defined(WLFBT)
		if (WLFBT_ENAB(wlc->pub) && (IS_WPA2_AUTH(cfg->WPA_auth))) {
			fast_reassoc = wlc_sup_ft_reassoc(cfg->sup);
		}
#endif /* WLFBT */

#endif /* BCMSUP_PSK */
		if (!fast_reassoc) {
			uint8 *sup_ies, *auth_ies;
			uint sup_ies_len, auth_ies_len;

			WL_WSEC(("wl%d: wlc_adopt_bss: calling set sup\n",
				wlc->pub->unit));
			wlc_find_sup_auth_ies(cfg->sup, &sup_ies, &sup_ies_len,
				&auth_ies, &auth_ies_len);
			wlc_sup_set_ea(cfg->sup, &cfg->BSSID);
			sup_stat = wlc_set_sup(cfg->sup, cfg->sup_type,
				sup_ies, sup_ies_len, auth_ies, auth_ies_len);
			BCM_REFERENCE(sup_stat);
		}
	}
#endif	

	if (target_bss->infra) {
		/* infrastructure join needs a BCN for TBTT coordination */
		WL_ASSOC(("wl%d: JOIN: BSS case...waiting for the next beacon...\n",
		            WLCWLUNIT(wlc)));
		wlc_assoc_change_state(cfg, AS_WAIT_RCV_BCN);
	}

	wlc_roam_release_flow_cntrl(cfg);

	wlc_bsscfg_SSID_set(cfg, target_bss->SSID, target_bss->SSID_len);
	wlc_roam_set_env(cfg, wlc->join_targets->count);

#ifdef WLLED
	wlc_led_event(wlc->ledh);
#endif
}

/* return true if we support all of the target basic rates */
static bool
wlc_join_basicrate_supported(wlc_info_t *wlc, wlc_rateset_t *rs, int band)
{
	uint i;
	uint8 rate;
	bool only_cck = FALSE;

#ifdef FULL_LEGACY_B_RATE_CHECK
	wlcband_t* pband;

	/* determine if we need to do addtional gmode == GMODE_LEGACY_B checking of rates */
	if (band == WLC_BAND_2G) {
		if (BAND_2G(wlc->band->bandtype))
			pband = wlc->band;
		else if (NBANDS(wlc) > 1 && BAND_2G(wlc->bandstate[OTHERBANDUNIT(wlc)]->bandtype))
			pband = wlc->bandstate[OTHERBANDUNIT(wlc)];
		else
			pband = NULL;

		if (pband && pband->gmode == GMODE_LEGACY_B)
			only_cck = TRUE;
	}
#endif
	for (i = 0; i < rs->count; i++)
		if (rs->rates[i] & WLC_RATE_FLAG) {
			rate = rs->rates[i] & RATE_MASK;
			if (!wlc_valid_rate(wlc, rate, band, FALSE))
				return (FALSE);
			if (only_cck && !IS_CCK(rate))
				return (FALSE);
		}
	return (TRUE);
}


void
wlc_join_recreate_complete(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn, int bcn_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	uint8* tlvs;
	int tlvs_len;
	bcm_tlv_t *tim;

	/* In a standard join attempt this is done in wlc_join_BSS() as the
	 * process begins. For a recreate, we wait until we have confirmation that
	 * we are in the presence of our former AP before unmuting a channel.
	 */
	if (wlc_quiet_chanspec(wlc->cmi, current_bss->chanspec)) {
		wlc_clr_quiet_chanspec(wlc->cmi, current_bss->chanspec);
		if (WLC_BAND_PI_RADIO_CHANSPEC == current_bss->chanspec) {
			wlc_mute(wlc, OFF, 0);
		}
	}

	/* In a standard join attempt this is done on the WLC_E_ASSOC/REASSOC event to
	 * allow traffic to flow to the newly associated AP. For a recreate, we wait until
	 * we have confirmation that we are in the presence of our former AP.
	 */
	wlc->block_datafifo &= ~DATA_BLOCK_JOIN;

	/* TSF adoption and PS transition is done just as in a standard association in
	 * wlc_join_complete()
	 */
	wlc_tsf_adopt_bcn(cfg, wrxh, plcp, bcn);

	tlvs = (uint8*)bcn + DOT11_BCN_PRB_LEN;
	tlvs_len = bcn_len - DOT11_BCN_PRB_LEN;

	tim = NULL;
	if (cfg->BSS) {
		/* adopt DTIM period for PS-mode support */
		tim = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_TIM_ID);

		if (tim) {
			current_bss->dtim_period = tim->data[DOT11_MNG_TIM_DTIM_PERIOD];
#ifdef WLMCNX
			if (MCNX_ENAB(wlc->pub))
				; /* empty */
			else
#endif
			if (cfg == wlc->cfg)
				wlc_write_shm(wlc, M_DOT11_DTIMPERIOD, current_bss->dtim_period);
			wlc_update_bcn_info(cfg, TRUE);
#ifdef WLMCNX
			if (MCNX_ENAB(wlc->pub))
				wlc_mcnx_dtim_upd(wlc->mcnx, cfg, TRUE);
#endif
		}else { /*  (!tim) illed AP, prevent going to power-save mode */
			wlc_set_pmoverride(cfg, TRUE);
		}
	}


	if (cfg->BSS) {
		void *pkt;

		/* when recreating an association, send a null data to the AP to verify
		 * that we are still associated and wait a generous amount amount of time
		 * to allow the AP to send a disassoc/deauth if it does not consider us
		 * associated.
		 */
		pkt = wlc_sendnulldata(wlc, cfg, &cfg->current_bss->BSSID, 0, 0, PRIO_8021D_BE);
		if (pkt == NULL)
			WL_ERROR(("wl%d: %s: wlc_sendnulldata() failed\n",
			          wlc->pub->unit, __FUNCTION__));

		/* kill the beacon timeout timer and reset for association verification */
		wlc_assoc_timer_del(wlc, cfg);
		wl_add_timer(wlc->wl, as->timer, as->verify_timeout, FALSE);

		wlc_assoc_change_state(cfg, AS_ASSOC_VERIFY);
	} else {
		/* for an IBSS recreate, we are done */
		as->type = AS_IDLE;
		wlc_assoc_change_state(cfg, AS_IDLE);
	}
}

static void
wlc_merge_bcn_prb(wlc_info_t *wlc,
	struct dot11_bcn_prb *p1, int p1_len,
	struct dot11_bcn_prb *p2, int p2_len,
	struct dot11_bcn_prb **merged, int *merged_len)
{
	uint8 *tlvs1, *tlvs2;
	int tlvs1_len, tlvs2_len;

	*merged = NULL;
	*merged_len = 0;

	if (p1) {
		ASSERT(p1_len >= DOT11_BCN_PRB_LEN);
		/* fixup for non-assert builds */
		if (p1_len < DOT11_BCN_PRB_LEN)
			p1 = NULL;
	}
	if (p2) {
		ASSERT(p2_len >= DOT11_BCN_PRB_LEN);
		/* fixup for non-assert builds */
		if (p2_len < DOT11_BCN_PRB_LEN)
			p2 = NULL;
	}

	/* check for simple cases of one or the other of the source packets being null */
	if (p1 == NULL && p2 == NULL) {
		return;
	} else if (p2 == NULL) {
		*merged = (struct dot11_bcn_prb *) MALLOC(wlc->osh, p1_len);
		if (*merged != NULL) {
			*merged_len = p1_len;
			bcopy((char*)p1, (char*)*merged, p1_len);
		}
		return;
	} else if (p1 == NULL) {
		*merged = (struct dot11_bcn_prb *) MALLOC(wlc->osh, p2_len);
		if (*merged != NULL) {
			*merged_len = p2_len;
			bcopy((char*)p2, (char*)*merged, p2_len);
		}
		return;
	}

	/* both source packets are present, so do the merge work */
	tlvs1 = (uint8*)p1 + DOT11_BCN_PRB_LEN;
	tlvs1_len = p1_len - DOT11_BCN_PRB_LEN;

	tlvs2 = (uint8*)p2 + DOT11_BCN_PRB_LEN;
	tlvs2_len = p2_len - DOT11_BCN_PRB_LEN;

	/* allocate a buffer big enough for the merged ies */
	*merged_len = DOT11_BCN_PRB_LEN + wlc_merged_ie_len(wlc, tlvs1, tlvs1_len, tlvs2,
		tlvs2_len);
	*merged = (struct dot11_bcn_prb *) MALLOC(wlc->osh, *merged_len);
	if (*merged == NULL) {
		*merged_len = 0;
		return;
	}

	/* copy the fixed portion of the second packet so the latest TSF, cap, etc is kept */
	bcopy(p2, *merged, DOT11_BCN_PRB_LEN);

	/* merge the ies from both packets */
	wlc_merge_ies(tlvs1, tlvs1_len, tlvs2, tlvs2_len, (uint8*)*merged + DOT11_BCN_PRB_LEN);
}

static int
wlc_merged_ie_len(wlc_info_t *wlc, uint8 *tlvs1, int tlvs1_len, uint8 *tlvs2, int tlvs2_len)
{
	bcm_tlv_t *tlv1 = (bcm_tlv_t*)tlvs1;
	bcm_tlv_t *tlv2 = (bcm_tlv_t*)tlvs2;
	int total;
	int len;

	/* treat an empty list or malformed list as empty */
	if (!bcm_valid_tlv(tlv1, tlvs1_len)) {
		tlv1 = NULL;
		tlvs1 = NULL;
	}
	if (!bcm_valid_tlv(tlv2, tlvs2_len)) {
		tlv2 = NULL;
		tlvs2 = NULL;
	}

	total = 0;

	len = tlvs2_len;
	while (tlv2) {
		total += TLV_HDR_LEN + tlv2->len;
		tlv2 = bcm_next_tlv(tlv2, &len);
	}

	len = tlvs1_len;
	while (tlv1) {
		if (!wlc_find_ie_match(tlv1, (bcm_tlv_t*)tlvs2, tlvs2_len))
			total += TLV_HDR_LEN + tlv1->len;

		tlv1 = bcm_next_tlv(tlv1, &len);
	}

	return total;
}

static bool
wlc_find_ie_match(bcm_tlv_t *ie, bcm_tlv_t *ies, int len)
{
	uint8 ie_len;

	ie_len = ie->len;

	while (ies) {
		if (ie_len == ies->len && !bcmp(ie, ies, (TLV_HDR_LEN + ie_len))) {
			return TRUE;
		}
		ies = bcm_next_tlv(ies, &len);
	}

	return FALSE;
}

static void
wlc_merge_ies(uint8 *tlvs1, int tlvs1_len, uint8 *tlvs2, int tlvs2_len, uint8* merge)
{
	bcm_tlv_t *tlv1 = (bcm_tlv_t*)tlvs1;
	bcm_tlv_t *tlv2 = (bcm_tlv_t*)tlvs2;
	int len;

	/* treat an empty list or malformed list as empty */
	if (!bcm_valid_tlv(tlv1, tlvs1_len)) {
		tlv1 = NULL;
		tlvs1 = NULL;
	}
	if (!bcm_valid_tlv(tlv2, tlvs2_len)) {
		tlv2 = NULL;
		tlvs2 = NULL;
	}

	/* copy in the ies from the second set */
	len = tlvs2_len;
	while (tlv2) {
		bcopy(tlv2, merge, TLV_HDR_LEN + tlv2->len);
		merge += TLV_HDR_LEN + tlv2->len;

		tlv2 = bcm_next_tlv(tlv2, &len);
	}

	/* merge in the ies from the first set */
	len = tlvs1_len;
	while (tlv1) {
		if (!wlc_find_ie_match(tlv1, (bcm_tlv_t*)tlvs2, tlvs2_len)) {
			bcopy(tlv1, merge, TLV_HDR_LEN + tlv1->len);
			merge += TLV_HDR_LEN + tlv1->len;
		}
		tlv1 = bcm_next_tlv(tlv1, &len);
	}
}

void
wlc_join_complete(wlc_bsscfg_t *cfg, wlc_d11rxhdr_t *wrxh, uint8 *plcp,
	struct dot11_bcn_prb *bcn, int bcn_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	uint8* tlvs;
	int tlvs_len;
	bcm_tlv_t *tim;
	struct dot11_bcn_prb *merged;
	int merged_len;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	wlc_pm_st_t *pm = cfg->pm;

#ifdef DEBUG_TBTT
	wlc->bad_TBTT = FALSE;
#endif /* DEBUG_TBTT */


	wlc_tsf_adopt_bcn(cfg, wrxh, plcp, bcn);

	tlvs = (uint8*)bcn + DOT11_BCN_PRB_LEN;
	tlvs_len = bcn_len - DOT11_BCN_PRB_LEN;

	tim = NULL;
	if (target_bss->infra == 1) {
		/* adopt DTIM period for PS-mode support */
		tim = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_TIM_ID);

		if (tim) {
			current_bss->dtim_period = tim->data[DOT11_MNG_TIM_DTIM_PERIOD];
#ifdef WLMCNX
			if (MCNX_ENAB(wlc->pub))
				; /* empty */
			else
#endif
			if (cfg == wlc->cfg)
				wlc_write_shm(wlc, M_DOT11_DTIMPERIOD, current_bss->dtim_period);
			wlc_update_bcn_info(cfg, TRUE);
#ifdef WLMCNX
			if (MCNX_ENAB(wlc->pub))
				wlc_mcnx_dtim_upd(wlc->mcnx, cfg, TRUE);
#endif
		}
	}

	if (target_bss->infra == 1) {
		/* illed AP, prevent going to power-save mode */
		if (!tim) {
			wlc_set_pmoverride(cfg, TRUE);
		}
	} else {
		/* adopt the IBSS parameters */
		wlc_join_adopt_bss(cfg);
		cfg->roam->time_since_bcn = 1;
		cfg->roam->bcns_lost = TRUE;
	}

	/* Merge the current saved probe response IEs with the beacon's in case the
	 * beacon is missing some info.
	 */
	wlc_merge_bcn_prb(wlc, current_bss->bcn_prb, current_bss->bcn_prb_len,
	                  bcn, bcn_len, &merged, &merged_len);

	/* save bcn's fixed and tagged parameters in current_bss */
	if (merged != NULL) {
		if (current_bss->bcn_prb)
			MFREE(wlc->osh, current_bss->bcn_prb, current_bss->bcn_prb_len);
		current_bss->bcn_prb = merged;
		current_bss->bcn_prb_len = (uint16)merged_len;
	} else {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
	}

	/* Grab and use the Country IE from the AP we are joining and override any Country IE
	 * we may have obtained from somewhere else.
	 */
	if ((BSS_WL11H_ENAB(wlc, cfg) || WLC_AUTOCOUNTRY_ENAB(wlc)) &&
	    (target_bss->infra == 1)) {
		wlc_cntry_adopt_country_ie(wlc->cntry, cfg, tlvs, tlvs_len);
	}


	/* If the PM2 Receive Throttle Duty Cycle feature is active, reset it */
	if (PM2_RCV_DUR_ENAB(cfg) && pm->PM == PM_FAST) {
		WL_RTDC(wlc, "wlc_join_complete: PMep=%02u AW=%02u",
			(pm->PMenabled ? 10 : 0) | pm->PMpending,
			(PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
		wlc_pm2_rcv_reset(cfg);
	}

	/* 802.11 PM can be entered now that we have synchronized with
	 * the BSS's TSF and DTIM count.
	 */
	if (PS_ALLOWED(cfg))
		wlc_set_pmstate(cfg, TRUE);
	/* sync/reset h/w */
	else {
		if (pm->PM == PM_FAST) {
			WL_RTDC(wlc, "wlc_join_complete: start srtmr, PMep=%02u AW=%02u",
			        (pm->PMenabled ? 10 : 0) | pm->PMpending,
			        (PS_ALLOWED(cfg) ? 10 : 0) | STAY_AWAKE(wlc));
			wlc_pm2_sleep_ret_timer_start(cfg);
		}
		wlc_set_ps_ctrl(cfg);
	}

	/* If assoc_state is not AS_IDLE, then this join event is completing a roam or a
	 * set_ssid operation.
	 * If assoc_state is AS_IDLE, then we are here due to an IBSS coalesce and there is
	 * no more follow on work.
	 */
	if (as->state != AS_IDLE) {
		/* send event to indicate final step in joining BSS */
		wlc_bss_mac_event(
			wlc, cfg, WLC_E_JOIN, &cfg->BSSID, WLC_E_STATUS_SUCCESS, 0,
			WLC_DOT11_BSSTYPE(target_bss->infra), 0, 0);
		wlc_assoc_done(cfg, WLC_E_STATUS_SUCCESS);
	}
	else {
		/* Upon IBSS coalescence, send ROAMING_START and ROAMING_COMPLETION events */
		if (WIN7_AND_UP_OS(wlc->pub)) {
			wlc_bss_mac_event(wlc, cfg, WLC_E_IBSS_COALESCE, &cfg->BSSID,
			                  WLC_E_STATUS_SUCCESS, WLC_E_STATUS_SUCCESS,
			                  WLC_DOT11_BSSTYPE(cfg->target_bss->infra),
			                  cfg->SSID, cfg->SSID_len);
		}
	}

#ifdef WLEXTLOG
	{
	char log_eabuf[ETHER_ADDR_STR_LEN];

	bcm_ether_ntoa(&cfg->BSSID, log_eabuf);

	WLC_EXTLOG(wlc, LOG_MODULE_ASSOC, FMTSTR_JOIN_COMPLETE_ID,
	           WL_LOG_LEVEL_ERR, 0, 0, log_eabuf);
	}
#endif
}

static int
wlc_join_pref_tlv_len(wlc_info_t *wlc, uint8 *pref, int len)
{
	if (len < TLV_HDR_LEN)
		return 0;

	switch (pref[TLV_TAG_OFF]) {
	case WL_JOIN_PREF_RSSI:
	case WL_JOIN_PREF_BAND:
	case WL_JOIN_PREF_RSSI_DELTA:
		return TLV_HDR_LEN + pref[TLV_LEN_OFF];
	case WL_JOIN_PREF_WPA:
		if (len < TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED) {
			WL_ERROR(("wl%d: mulformed WPA cfg in join pref\n", WLCWLUNIT(wlc)));
			return -1;
		}
		return TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED +
		        pref[TLV_HDR_LEN + WLC_JOIN_PREF_OFF_COUNT] * 12;
	default:
		WL_ERROR(("wl%d: unknown join pref type\n", WLCWLUNIT(wlc)));
		return -1;
	}

	return 0;
}

int
wlc_join_pref_parse(wlc_bsscfg_t *cfg, uint8 *pref, int len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	int tlv_pos, tlv_len;
	uint8 type;
	uint8 bits;
	uint8 start;
	uint i;
	uint8 f, c;
	uint p;
	uint8 band;

	WL_TRACE(("wl%d: wlc_join_pref_parse: pref len = %d\n", WLCWLUNIT(wlc), len));

	if (join_pref == NULL)
		return BCME_ERROR;

	band = join_pref->band;
	bzero(join_pref, sizeof(wlc_join_pref_t));
	join_pref->band = band;
	if (!len) {
		wlc_join_pref_band_upd(cfg);
		return 0;
	}

	/*
	* Each join target carries a 'weight', consisting of a number
	* of info such as akm and cipher. The weight is represented by
	* a N bit number. The bigger the number is the more likely the
	* target becomes the join candidate. Each info in the weight
	* is called a field, which is made of a type defined in wlioctl.h
	* and a bit offset assigned by parsing user-supplied "join_pref"
	* iovar. The fields are ordered from the most significant field to
	* the least significant field.
	*/
	/* count # tlvs # bits first */
	for (tlv_pos = 0, f = 0, start = 0, p = 0;
	     (tlv_len = wlc_join_pref_tlv_len(wlc, &pref[tlv_pos],
	                                      len - tlv_pos)) >= TLV_HDR_LEN &&
	             tlv_pos + tlv_len <= len;
	     tlv_pos += tlv_len) {
		type = pref[tlv_pos + TLV_TAG_OFF];
		if (p & WLCTYPEBMP(type)) {
			WL_ERROR(("wl%d: multiple join pref type %d\n", WLCWLUNIT(wlc), type));
			goto err;
		}
		switch (type) {
		case WL_JOIN_PREF_RSSI:
			bits = WLC_JOIN_PREF_BITS_RSSI;
			break;
		case WL_JOIN_PREF_WPA:
			bits = WLC_JOIN_PREF_BITS_WPA;
			break;
		case WL_JOIN_PREF_BAND:
			bits = WLC_JOIN_PREF_BITS_BAND;
			break;
		case WL_JOIN_PREF_RSSI_DELTA:
			bits = WLC_JOIN_PREF_BITS_RSSI_DELTA;
			break;
		default:
			WL_ERROR(("wl%d: invalid join pref type %d\n", WLCWLUNIT(wlc), type));
			goto err;
		}
		f++;
		start += bits;
		p |= WLCTYPEBMP(type);
	}
	/* rssi field is mandatory! */
	if (!(p & WLCTYPEBMP(WL_JOIN_PREF_RSSI))) {
		WL_ERROR(("wl%d: WL_JOIN_PREF_RSSI (type %d) is not present\n",
			WLCWLUNIT(wlc), WL_JOIN_PREF_RSSI));
		goto err;
	}

	/* RSSI Delta is not maintained in the join_pref fields */
	if (p & WLCTYPEBMP(WL_JOIN_PREF_RSSI_DELTA))
		f--;

	/* other sanity checks */
	if (start > sizeof(wlc->join_targets->ptrs[0]->RSSI) * 8) {
		WL_ERROR(("wl%d: too many bits %d max %u\n", WLCWLUNIT(wlc), start,
			(uint)sizeof(wlc->join_targets->ptrs[0]->RSSI) * 8));
		goto err;
	}
	if (f > MAXJOINPREFS) {
		WL_ERROR(("wl%d: too many tlvs/prefs %d\n", WLCWLUNIT(wlc), f));
		goto err;
	}
	WL_ASSOC(("wl%d: wlc_join_pref_parse: total %d fields %d bits\n", WLCWLUNIT(wlc), f,
		start));
	/* parse user-supplied join pref list */
	/* reverse the order so that most significant pref goes to the last entry */
	join_pref->fields = f;
	join_pref->prfbmp = p;
	for (tlv_pos = 0;
	     (tlv_len = wlc_join_pref_tlv_len(wlc, &pref[tlv_pos],
	                                      len - tlv_pos)) >= TLV_HDR_LEN &&
	             tlv_pos + tlv_len <= len;
	     tlv_pos += tlv_len) {
		bits = 0;
		switch ((type = pref[tlv_pos + TLV_TAG_OFF])) {
		case WL_JOIN_PREF_RSSI:
			bits = WLC_JOIN_PREF_BITS_RSSI;
			break;
		case WL_JOIN_PREF_WPA:
			c = pref[tlv_pos + TLV_HDR_LEN + WLC_JOIN_PREF_OFF_COUNT];
			bits = WLC_JOIN_PREF_BITS_WPA;
			/* sanity check */
			if (c > WLCMAXCNT(bits)) {
				WL_ERROR(("wl%d: two many wpa configs %d max %d\n",
					WLCWLUNIT(wlc), c, WLCMAXCNT(bits)));
				goto err;
			}
			else if (!c) {
				WL_ERROR(("wl%d: no wpa config specified\n", WLCWLUNIT(wlc)));
				goto err;
			}
			/* user-supplied list is from most favorable to least favorable */
			/* reverse the order so that the bigger the index the more favorable the
			 * config is
			 */
			for (i = 0; i < c; i ++)
				bcopy(&pref[tlv_pos + TLV_HDR_LEN + WLC_JOIN_PREF_LEN_FIXED +
				            i * sizeof(join_pref->wpa[0])],
				      &join_pref->wpa[c - 1 - i],
				      sizeof(join_pref->wpa[0]));
			join_pref->wpas = c;
			break;
		case WL_JOIN_PREF_BAND:
			bits = WLC_JOIN_PREF_BITS_BAND;
			/* honor use what WLC_SET_ASSOC_PREFER says first */
			if (pref[tlv_pos + TLV_HDR_LEN + WLC_JOIN_PREF_OFF_BAND] ==
				WLJP_BAND_ASSOC_PREF)
				break;
			/* overwrite with this setting */
			join_pref->band = pref[tlv_pos + TLV_HDR_LEN + WLC_JOIN_PREF_OFF_BAND];
			break;
		case WL_JOIN_PREF_RSSI_DELTA:
			bits = WLC_JOIN_PREF_BITS_RSSI_DELTA;
			cfg->join_pref_rssi_delta.rssi = pref[tlv_pos + TLV_HDR_LEN +
			                                 WLC_JOIN_PREF_OFF_DELTA_RSSI];

			cfg->join_pref_rssi_delta.band = pref[tlv_pos + TLV_HDR_LEN +
			                                 WLC_JOIN_PREF_OFF_BAND];
			break;
		}
		if (!bits)
			continue;

		f--;
		start -= bits;

		join_pref->field[f].type = type;
		join_pref->field[f].start = start;
		join_pref->field[f].bits = bits;
		WL_ASSOC(("wl%d: wlc_join_pref_parse: added field %s entry %d offset %d bits %d\n",
			WLCWLUNIT(wlc), WLCJOINPREFN(type), f, start, bits));
	}

	/* band preference can be from a different source */
	if (!(p & WLCTYPEBMP(WL_JOIN_PREF_BAND)))
		wlc_join_pref_band_upd(cfg);

	return 0;

	/* error handling */
err:
	band = join_pref->band;
	bzero(join_pref, sizeof(wlc_join_pref_t));
	join_pref->band = band;
	return BCME_ERROR;
}


void
wlc_join_pref_band_upd(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	uint i;
	uint8 start = 0;
	uint p = 0;

	(void)wlc;

	WL_ASSOC(("wl%d: wlc_join_pref_band_upd: band pref is %d\n",
		WLCWLUNIT(wlc), join_pref->band));
	if (join_pref->band == WLC_BAND_AUTO)
		return;
	/* find band field first. rssi field should be set too if found */
	for (i = 0; i < join_pref->fields; i ++) {
		if (join_pref->field[i].type == WL_JOIN_PREF_BAND) {
			WL_ASSOC(("wl%d: found field %s entry %d\n",
				WLCWLUNIT(wlc), WLCJOINPREFN(WL_JOIN_PREF_BAND), i));
			return;
		}
		start += join_pref->field[i].bits;
		p |= WLCTYPEBMP(join_pref->field[i].type);
	}
	/* rssi field is mandatory. fields should be empty when rssi field is not set */
	if (!(p & WLCTYPEBMP(WL_JOIN_PREF_RSSI))) {
		ASSERT(join_pref->fields == 0);
		join_pref->field[0].type = WL_JOIN_PREF_RSSI;
		join_pref->field[0].start = 0;
		join_pref->field[0].bits = WLC_JOIN_PREF_BITS_RSSI;
		WL_ASSOC(("wl%d: wlc_join_pref_band_upd: added field %s entry 0 offset 0\n",
			WLCWLUNIT(wlc), WLCJOINPREFN(WL_JOIN_PREF_RSSI)));
		start = WLC_JOIN_PREF_BITS_RSSI;
		p |= WLCTYPEBMP(WL_JOIN_PREF_RSSI);
		i = 1;
	}
	/* add band field */
	join_pref->field[i].type = WL_JOIN_PREF_BAND;
	join_pref->field[i].start = start;
	join_pref->field[i].bits = WLC_JOIN_PREF_BITS_BAND;
	WL_ASSOC(("wl%d: wlc_join_pref_band_upd: added field %s entry %d offset %d\n",
		WLCWLUNIT(wlc), WLCJOINPREFN(WL_JOIN_PREF_BAND), i, start));
	p |= WLCTYPEBMP(WL_JOIN_PREF_BAND);
	join_pref->prfbmp = p;
	join_pref->fields = i + 1;
}

void
wlc_join_pref_reset(wlc_bsscfg_t *cfg)
{
	uint8 band;
	wlc_join_pref_t *join_pref = cfg->join_pref;

	band = join_pref->band;
	bzero(join_pref, sizeof(wlc_join_pref_t));
	join_pref->band = band;
	wlc_join_pref_band_upd(cfg);
}

void
wlc_auth_tx_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_ID(wlc, (uint16)(uintptr)arg);
	wlc_assoc_t *as;
	wlc_bss_info_t *target_bss;

	/* in case bsscfg is freed before this callback is invoked */
	if (cfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, arg));
		return;
	}

	as = cfg->assoc;
	target_bss = cfg->target_bss;

	/* assoc aborted? */
	if (!(as->state == AS_SENT_AUTH_1 || as->state == AS_SENT_AUTH_3 ||
		as->state == AS_SENT_FTREQ))
		return;

	/* no ack */
	if (!(txstatus & TX_STATUS_ACK_RCV)) {
		wlc_auth_complete(cfg, WLC_E_STATUS_NO_ACK, &target_bss->BSSID, 0, 0);
		return;
	}
	wlc_assoc_timer_del(wlc, cfg);
	wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT + 10, 0);
}

void
wlc_assocreq_complete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_ID(wlc, (uint16)(uintptr)arg);
	wlc_assoc_t *as;
	wlc_bss_info_t *target_bss;

	/* in case bsscfg is freed before this callback is invoked */
	if (cfg == NULL) {
		WL_ERROR(("wl%d: %s: unable to find bsscfg by ID %p\n",
		          wlc->pub->unit, __FUNCTION__, arg));
		return;
	}

	as = cfg->assoc;
	target_bss = cfg->target_bss;

	/* assoc aborted? */
	if (!(as->state == AS_SENT_ASSOC || as->state == AS_REASSOC_RETRY))
		return;

	/* no ack */
	if (!(txstatus & TX_STATUS_ACK_RCV)) {
		wlc_assoc_complete(cfg, WLC_E_STATUS_NO_ACK, &target_bss->BSSID, 0,
			as->type != AS_ASSOCIATION, 0);
		return;
	}
	wlc_assoc_timer_del(wlc, cfg);
	wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT + 10, 0);
}

static wlc_bss_info_t *
wlc_bss_info_dup(wlc_info_t *wlc, wlc_bss_info_t *bi)
{
	wlc_bss_info_t *bss = MALLOC(wlc->osh, sizeof(wlc_bss_info_t));
	if (!bss) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	bcopy(bi, bss, sizeof(wlc_bss_info_t));
	if (bi->bcn_prb) {
		if (!(bss->bcn_prb = MALLOC(wlc->osh, bi->bcn_prb_len))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				WLCWLUNIT(wlc), __FUNCTION__, MALLOCED(wlc->osh)));
			MFREE(wlc->osh, bss, sizeof(wlc_bss_info_t));
			return NULL;
		}
		bcopy(bi->bcn_prb, bss->bcn_prb, bi->bcn_prb_len);
	}
	return bss;
}

static int
wlc_bss_list_expand(wlc_bsscfg_t *cfg, wlc_bss_list_t *from, wlc_bss_list_t *to)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_join_pref_t *join_pref = cfg->join_pref;
	uint i, j, k, c;
	wlc_bss_info_t *bi;
	struct rsn_parms *rsn;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	wpa_suite_t *akm, *uc, *mc;

	WL_ASSOC(("wl%d: wlc_bss_list_expand: scan results %d\n", WLCWLUNIT(wlc), from->count));

	ASSERT(to->count == 0);
	if (WLCAUTOWPA(cfg)) {
		/* duplicate each bss to multiple copies, one for each wpa config */
		for (i = 0, c = 0; i < from->count && c < (uint) wlc->pub->tunables->maxbss; i ++) {
			/* ignore the bss if it does not support wpa */
			if (!(from->ptrs[i]->flags & (WLC_BSS_WPA | WLC_BSS_WPA2)))
			{
				WL_ASSOC(("wl%d: ignored BSS %s, it does not do WPA!\n",
					WLCWLUNIT(wlc),
					bcm_ether_ntoa(&from->ptrs[i]->BSSID, eabuf)));
				continue;
			}

			/*
			* walk thru all wpa configs, move/dup the bss to join targets list
			* if it supports the config.
			*/
			for (j = 0, rsn = NULL, bi = NULL;
			    j < join_pref->wpas && c < (uint) wlc->pub->tunables->maxbss; j ++) {
				WL_ASSOC(("wl%d: WPA cfg %d:"
				          " %02x%02x%02x%02x-%02x%02x%02x%02x-%02x%02x%02x%02x\n",
				          WLCWLUNIT(wlc), j,
				          join_pref->wpa[j].akm[0],
				          join_pref->wpa[j].akm[1],
				          join_pref->wpa[j].akm[2],
				          join_pref->wpa[j].akm[3],
				          join_pref->wpa[j].ucipher[0],
				          join_pref->wpa[j].ucipher[1],
				          join_pref->wpa[j].ucipher[2],
				          join_pref->wpa[j].ucipher[3],
				          join_pref->wpa[j].mcipher[0],
				          join_pref->wpa[j].mcipher[1],
				          join_pref->wpa[j].mcipher[2],
				          join_pref->wpa[j].mcipher[3]));
				/* check if the AP supports the wpa config */
				akm = (wpa_suite_t*)join_pref->wpa[j].akm;
				uc = (wpa_suite_t*)join_pref->wpa[j].ucipher;

				if (!bcmp(akm, WPA_OUI, DOT11_OUI_LEN))
					rsn = bi ? &bi->wpa : &from->ptrs[i]->wpa;
				else if (!bcmp(akm, WPA2_OUI, DOT11_OUI_LEN))
					rsn = bi ? &bi->wpa2 : &from->ptrs[i]->wpa2;
				else {
				/*
				* something has gone wrong, or need to add
				* new code to handle the new akm here!
				*/
					WL_ERROR(("wl%d: unknown akm suite %02x%02x%02x%02x in WPA"
						" cfg\n",
						WLCWLUNIT(wlc),
						akm->oui[0], akm->oui[1], akm->oui[2], akm->type));
					continue;
				}
#ifdef BCMDBG
				if (WL_ASSOC_ON()) {
					prhex("rsn parms", (uint8 *)rsn, sizeof(*rsn));
				}
#endif /* BCMDBG */
				/* check if the AP offers the akm */
				for (k = 0; k < rsn->acount; k ++) {
					if (akm->type == rsn->auth[k])
						break;
				}
				/* the AP does not offer the akm! */
				if (k >= rsn->acount) {
					WL_ASSOC(("wl%d: skip WPA cfg %d: akm not match\n",
						WLCWLUNIT(wlc), j));
					continue;
				}
				/* check if the AP offers the unicast cipher */
				for (k = 0; k < rsn->ucount; k ++) {
					if (uc->type == rsn->unicast[k])
						break;
				}
				/* AP does not offer the cipher! */
				if (k >= rsn->ucount)
					continue;
				/* check if the AP offers the multicast cipher */
				mc = (wpa_suite_t*)join_pref->wpa[j].mcipher;
				if (bcmp(mc, WL_WPA_ACP_MCS_ANY, WPA_SUITE_LEN)) {
					if (mc->type != rsn->multicast) {
						WL_ASSOC(("wl%d: skip WPA cfg %d: mc not match\n",
							WLCWLUNIT(wlc), j));
						continue;
					}
				}
				/* move/duplicate the BSS */
				if (!bi) {
					to->ptrs[c] = bi = from->ptrs[i];
					from->ptrs[i] = NULL;
				}
				else if (!(to->ptrs[c] = wlc_bss_info_dup(wlc, bi))) {
					WL_ERROR(("wl%d: failed to duplicate bss info\n",
						WLCWLUNIT(wlc)));
					goto err;
				}
				WL_ASSOC(("wl%d: BSS %s and WPA cfg %d match\n", WLCWLUNIT(wlc),
					bcm_ether_ntoa(&bi->BSSID, eabuf), j));
				/* save multicast cipher for WPA config derivation */
				if (!bcmp(mc, WL_WPA_ACP_MCS_ANY, WPA_SUITE_LEN))
					to->ptrs[c]->mcipher = rsn->multicast;
				/* cache the config index as preference weight */
				to->ptrs[c]->wpacfg = (uint8)j;
				/* mask off WPA or WPA2 flag to match the selected entry */
				if (!bcmp(uc->oui, WPA2_OUI, DOT11_OUI_LEN))
					to->ptrs[c]->flags &= ~WLC_BSS_WPA;
				else
					to->ptrs[c]->flags &= ~WLC_BSS_WPA2;
				c ++;
			}
			/* the BSS does not support any of our wpa configs */
			if (!bi) {
				WL_ASSOC(("wl%d: ignored BSS %s, it does not offer expected WPA"
					" cfg!\n",
					WLCWLUNIT(wlc),
					bcm_ether_ntoa(&from->ptrs[i]->BSSID, eabuf)));
				continue;
			}
		}
	}
	else {
		c = 0;
		WL_ERROR(("wl%d: don't know how to expand the list\n", WLCWLUNIT(wlc)));
		goto err;
	}

	/* what if the join_target list is too big */
	if (c >= (uint) wlc->pub->tunables->maxbss) {
		WL_ERROR(("wl%d: two many entries, scan results may not be fully expanded\n",
			WLCWLUNIT(wlc)));
	}

	/* done */
	to->count = c;
	WL_ASSOC(("wl%d: wlc_bss_list_expand: join targets %d\n", WLCWLUNIT(wlc), c));

	/* clean up the source list */
	wlc_bss_list_free(wlc, from);
	return 0;

	/* error handling */
err:
	to->count = c;
	wlc_bss_list_free(wlc, to);
	wlc_bss_list_free(wlc, from);
	return BCME_ERROR;
}

void
wlc_assocresp_client(wlc_bsscfg_t *cfg, struct dot11_management_header *hdr, void *body,
	uint body_len, struct scb *scb)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	struct dot11_assoc_resp *assoc = (struct dot11_assoc_resp *) body;
	uchar *tlvs = NULL;
	uint tlvs_len, parse_len;
	uint8 *parse = NULL;
	uint16 fk;
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];

	bcm_ether_ntoa(&hdr->sa, eabuf);
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_ASSOC */

	ASSERT(BSSCFG_STA(cfg));

	if (!(as->state == AS_SENT_ASSOC || as->state == AS_REASSOC_RETRY) ||
		bcmp(hdr->bssid.octet, target_bss->BSSID.octet, ETHER_ADDR_LEN)) {
		WL_ERROR(("wl%d.%d: unsolicited association response from %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), eabuf));
		fk = ltoh16(hdr->fc) & FC_KIND_MASK;
		wlc_assoc_complete(cfg, WLC_E_STATUS_UNSOLICITED, &hdr->sa, 0,
			fk != FC_ASSOC_RESP, 0);
		return;
	}

	/* capability */
#ifdef BCMDBG
	if (WL_ERROR_ON()) {
		if (!(ltoh16(assoc->capability) & DOT11_CAP_ESS)) {
			WL_ERROR(("wl%d: association response without ESS set from %s\n",
				wlc->pub->unit, eabuf));
		}
		if (ltoh16(assoc->capability) & DOT11_CAP_IBSS) {
			WL_ERROR(("wl%d: association response with IBSS set from %s\n",
				wlc->pub->unit, eabuf));
		}
	}
#endif /* BCMDBG */

	/* save last (re)association response */
	if (as->resp) {
		MFREE(wlc->osh, as->resp, as->resp_len);
		as->resp_len = 0;
	}
	if (!(as->resp = MALLOC(wlc->osh, body_len)))
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
	else {
		as->resp_len = body_len;
		bcopy((char*)assoc, (char*)as->resp, body_len);
	}


	/* association denied */
	if (ltoh16(assoc->status) == DOT11_SC_REASSOC_FAIL &&
	    as->type != AS_ASSOCIATION &&
	    as->state != AS_REASSOC_RETRY) {
		ASSERT(scb != NULL);

		wlc_sendassocreq(wlc, target_bss, scb, FALSE);
		wlc_assoc_change_state(cfg, AS_REASSOC_RETRY);
		WL_ASSOC(("wl%d: Retrying with Assoc Req frame "
			"due to Reassoc Req failure (DOT11_SC_REASSOC_FAIL) from %s\n",
			wlc->pub->unit, eabuf));
		return;
	} else if (ltoh16(assoc->status) != DOT11_SC_SUCCESS) {
		wlc_assoc_complete(cfg, WLC_E_STATUS_FAIL, &target_bss->BSSID,
			ltoh16(assoc->status), as->type != AS_ASSOCIATION,
			WLC_DOT11_BSSTYPE(target_bss->infra));
		return;
	}

	ASSERT(scb != NULL);

	/* Mark assoctime for use in roam calculations */
	scb->assoctime = wlc->pub->now;

	tlvs = (uint8 *) &assoc[1];
	tlvs_len = body_len - sizeof(struct dot11_assoc_resp);

	wlc_bss_mac_event(wlc, cfg, WLC_E_ASSOC_RESP_IE, NULL, 0, 0, 0,
		tlvs, (int)tlvs_len);


#ifdef WLP2P
	if (BSS_P2P_ENAB(wlc, cfg)) {
		wlc_p2p_process_assocresp(wlc->p2p, scb, tlvs, tlvs_len);
	}
#endif

	/* Is this a BRCM AP */
	wlc_process_brcm_ie(wlc, scb, (brcm_ie_t*)wlc_find_vendor_ie(tlvs, tlvs_len,
		BRCM_OUI, NULL, 0));
#if defined(WLTDLS) || defined(WLWNM)
	if (TDLS_ENAB(wlc->pub) || WLWNM_ENAB(wlc->pub))
		wlc_process_extcap_ie(wlc, tlvs, tlvs_len, scb);
#endif /* WLTDLS || WLWNM */
#ifdef WLWNM
	if (WLWNM_ENAB(wlc->pub)) {
		wlc_process_bss_max_idle_period_ie(wlc, tlvs, tlvs_len, scb);
		wlc_process_tim_bcast_resp_ie(wlc, tlvs, tlvs_len, scb);
	}
#endif /* WLWNM */

	/* If WME is enabled, check if response indicates WME association */
	scb->flags &= ~SCB_WMECAP;
	cfg->flags &= ~WLC_BSSCFG_WME_ASSOC;
	if (BSS_WME_ENAB(wlc, cfg)) {
		bcm_tlv_t *wme_ie;
		bool reassoc_to_self = FALSE;

		parse = tlvs;
		parse_len = tlvs_len;
		wme_ie = wlc_find_wme_ie(parse, parse_len);

		/* WMM-AC defines reassoc to self as special case. Dynamic AC params must be
		 * maintained on reassoc to same AP. See WMM Specification section A.9 for more
		 * info.
		 */
		fk = ltoh16(hdr->fc) & FC_KIND_MASK;
		if (fk == FC_REASSOC_RESP &&
			bcmp(cfg->prev_BSSID.octet, target_bss->BSSID.octet, ETHER_ADDR_LEN) == 0) {
			reassoc_to_self = TRUE;
		}
		if (reassoc_to_self == FALSE)
			wlc_qosinfo_update(scb, 0);	/* Clear Qos Info */
		if (wme_ie && (reassoc_to_self == FALSE)) {
			wlc_pm_st_t *pm = cfg->pm;
			wlc_wme_t *wme = cfg->wme;

			scb->flags |= SCB_WMECAP;
			cfg->flags |= WLC_BSSCFG_WME_ASSOC;

			/* save the new IE, or params IE which is superset of IE */
			bcopy(wme_ie->data, &wme->wme_param_ie, wme_ie->len);
			/* Apply the STA AC params sent by AP,
			 * will be done in wlc_join_adopt_bss()
			 */
			/* wlc_edcf_acp_apply(wlc, cfg, TRUE); */
			/* Use locally-requested APSD config if AP advertised APSD */
			/* STA is in AUTO WME mode,
			 *     AP has UAPSD enabled, then allow STA to use wlc->PM
			 *            else, don't allow STA to sleep based on wlc->PM only
			 *                  if it's BRCM AP not capable of handling
			 *                                  WME STAs in PS,
			 *                  and leave PM mode if already set
			 */
			if (wme->wme_param_ie.qosinfo & WME_QI_AP_APSD_MASK) {
				wlc_qosinfo_update(scb, wme->apsd_sta_qosinfo);
				pm->WME_PM_blocked = FALSE;
				if (pm->PM == PM_MAX)
					wlc_set_pmstate(cfg, TRUE);
			} else if (WME_AUTO(wlc) &&
			           (scb->flags & SCB_BRCM)) {
				if (!(scb->flags & SCB_WMEPS)) {
					pm->WME_PM_blocked = TRUE;
					WL_RTDC(wlc, "wlc_recvctl: exit PS", 0, 0);
					wlc_set_pmstate(cfg, FALSE);
				} else {
					pm->WME_PM_blocked = FALSE;
					if (pm->PM == PM_MAX)
						wlc_set_pmstate(cfg, TRUE);
				}
			}
		}
	}

#ifdef WL11AC
	if (SCB_HT_CAP(scb) || SCB_VHT_CAP(scb)) {
		uint8 ext_cap[DOT11_EXTCAP_LEN_MAX];
		uint8* ext_cap_ie_p = NULL;
		dot11_oper_mode_notif_ie_t oper_mode_ie, *oper_mode_ie_p = NULL;

		ext_cap_ie_p = wlc_read_ext_cap_ie(wlc, tlvs, tlvs_len, ext_cap);
		if (ext_cap_ie_p && isset(ext_cap_ie_p, DOT11_EXT_CAP_OPER_MODE_NOTIF))
			scb->flags3 |= SCB3_OPER_MODE_NOTIF;

		oper_mode_ie_p = wlc_read_oper_mode_notif_ie(wlc, tlvs, tlvs_len,
			&oper_mode_ie);
		if (oper_mode_ie_p)
			wlc_scb_update_oper_mode(wlc, scb, oper_mode_ie_p->mode);
	}
#endif


#ifdef MFP
	/* we have a valid combination of MFP flags */
	scb->flags2 &= ~(SCB2_MFP | SCB2_SHA256);
	if ((cfg->wsec & MFP_CAPABLE) && (target_bss->wpa2.flags & RSN_FLAGS_MFPC))
		scb->flags2 |= SCB2_MFP;
	if ((cfg->wsec & MFP_CAPABLE) &&
		(target_bss->wpa2.flags & RSN_FLAGS_SHA256))
		scb->flags2 |= SCB2_SHA256;
	WL_ASSOC(("wl%d: %s: turn MFP on %s\n", wlc->pub->unit, __FUNCTION__,
		((scb->flags2 |= SCB2_SHA256) ? "with sha256" : "")));
#endif

#if defined(WLFBT)
	if (WLFBT_ENAB(wlc->pub)) {
		/* Parse ftie (if any) in reassoc response. */
		fk = ltoh16(hdr->fc) & FC_KIND_MASK;
		if (fk == FC_REASSOC_RESP &&
			(cfg->WPA_auth & WPA2_AUTH_FT) && cfg->sup) {
			if (wlc_sup_ft_parse_reassocresp(cfg->sup, tlvs, tlvs_len)) {
				/* Process any RIC responses. */
				if (CAC_ENAB(wlc->pub) && wlc->cac != NULL) {
					bool accepted = wlc_parse_ric_resp(wlc, tlvs, tlvs_len);
					if (accepted == FALSE) {
						WL_ERROR(("wl%d: %s: RIC request denied\n",
							wlc->pub->unit, __FUNCTION__));
						return;
					}
				}
			} else {
				WL_ERROR(("wl%d: %s failed, ignoring reassoc response.\n",
					wlc->pub->unit, __FUNCTION__));
				return;
			}
		}
	}
#endif /* WLFBT */
	/* Association success */
	cfg->AID = ltoh16(assoc->aid);
	wlc->AID = cfg->AID;


	{
		wlc_assoc_complete(cfg, WLC_E_STATUS_SUCCESS, &target_bss->BSSID,
			ltoh16(assoc->status), as->type != AS_ASSOCIATION,
			WLC_DOT11_BSSTYPE(target_bss->infra));

		wlc_assoc_success(cfg, scb);


	}

	WL_ASSOC(("wl%d.%d: Checking if key needs to be inserted\n",
	          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg)));
	/* If Multi-SSID is enabled, and Legacy WEP is in use for this bsscfg,
	 * a "pairwise" key must be created by copying the default key from the bsscfg.
	 */

	if (cfg->WPA_auth == WPA_AUTH_DISABLED)
		WL_ASSOC(("wl%d: WPA disabled\n", WLCWLUNIT(wlc)));
	if (WSEC_WEP_ENABLED(cfg->wsec))
		WL_ASSOC(("wl%d: WEP enabled\n", WLCWLUNIT(wlc)));
	if (MBSS_ENAB(wlc->pub))
		WL_ASSOC(("wl%d: MBSS on\n", WLCWLUNIT(wlc)));
	if ((MBSS_ENAB(wlc->pub) || PSTA_ENAB(wlc->pub) || cfg != wlc->cfg) &&
	    cfg->WPA_auth == WPA_AUTH_DISABLED && WSEC_WEP_ENABLED(cfg->wsec)) {
#ifdef PSTA
		wsec_key_t *defkey = WSEC_BSS_DEFAULT_KEY(BSSCFG_PSTA(cfg) ?
		                              wlc_bsscfg_primary(wlc) : cfg);
#else /* PSTA */
		wsec_key_t *defkey = WSEC_BSS_DEFAULT_KEY(cfg);
#endif /* PSTA */
		if (defkey)
			WL_ASSOC(("wl%d: Def key installed\n", WLCWLUNIT(wlc)));
		if (defkey &&
		    ((defkey->algo == CRYPTO_ALGO_WEP1) ||
		     (defkey->algo == CRYPTO_ALGO_WEP128))) {
			int bcmerror;
			WL_ASSOC(("wl%d: Inserting key for %s\n", wlc->pub->unit, eabuf));
			bcmerror = wlc_key_insert(wlc, cfg, defkey->len, defkey->id,
			                          defkey->flags, defkey->algo,
			                          defkey->data, &target_bss->BSSID,
			                          NULL, NULL);
			ASSERT(!bcmerror);
		}
	}
}

void
wlc_authresp_client(wlc_bsscfg_t *cfg, struct dot11_management_header *hdr, void *body,
	uint body_len, bool short_preamble)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
#ifdef BCMDBG_ERR
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif /* BCMDBG_ERR */
	struct dot11_auth *auth = (struct dot11_auth *) body;
	uint16 auth_alg, auth_seq, auth_status;
	uint8 *challenge = (uint8 *) &auth[1];
	uint cfg_auth_alg;
	struct scb *scb;
	void *pkt;

	WL_TRACE(("wl%d: wlc_authresp_client\n", wlc->pub->unit));

	auth_alg = ltoh16(auth->alg);
	auth_seq = ltoh16(auth->seq);
	auth_status = ltoh16(auth->status);

	/* ignore authentication frames from other stations */
	if (bcmp((char*)&hdr->sa, (char*)&target_bss->BSSID, ETHER_ADDR_LEN) ||
	    (scb = wlc_scbfind(wlc, cfg, (struct ether_addr *)&hdr->sa)) == NULL ||
	    (as->state != AS_SENT_AUTH_1 && as->state != AS_SENT_AUTH_3)) {
		WL_ERROR(("wl%d.%d: unsolicited authentication response from %s\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), sa));
		wlc_auth_complete(cfg, WLC_E_STATUS_UNSOLICITED, &hdr->sa, 0, 0);
		return;
	}

	ASSERT(scb != NULL);

#if defined(AP) && defined(STA)
	if (SCB_ASSOCIATED(scb) || !(scb->flags & SCB_MYAP)) {
		WL_APSTA(("wl%d: got AUTH from %s, associated but not AP, forcing AP flag\n",
			wlc->pub->unit, sa));
	}
#endif /* APSTA */
	scb->flags |= SCB_MYAP;

	if ((as->state == AS_SENT_AUTH_1 && auth_seq != 2) ||
	    (as->state == AS_SENT_AUTH_3 && auth_seq != 4)) {
		WL_ERROR(("wl%d: out-of-sequence authentication response from %s\n",
		          wlc->pub->unit, sa));
		wlc_auth_complete(cfg, WLC_E_STATUS_UNSOLICITED, &hdr->sa, 0, 0);
		return;
	}

	/* authentication error */
	if (auth_status != DOT11_SC_SUCCESS) {
		wlc_auth_complete(cfg, WLC_E_STATUS_FAIL, &hdr->sa, auth_status, auth_alg);
		return;
	}

	/* invalid authentication algorithm number */
#ifdef WLFBT
	if (!WLFBT_ENAB(wlc->pub)) {
#endif
		cfg_auth_alg = cfg->auth;

	if (auth_alg != cfg_auth_alg && !cfg->openshared) {
		WL_ERROR(("wl%d: invalid authentication algorithm number, got %d, expected %d\n",
		          wlc->pub->unit, auth_alg, cfg_auth_alg));
		wlc_auth_complete(cfg, WLC_E_STATUS_FAIL, &hdr->sa, auth_status, auth_alg);
		return;
	}
#ifdef WLFBT
	}
#endif

	if (auth_alg == DOT11_SHARED_KEY && auth_seq == 2) {
		/* respond to the challenge */
		if (body_len < (sizeof(struct dot11_auth) + 2 + challenge[1])) {
			WL_ERROR(("wl%d: shared key auth pkt too short, got %d, expected %u\n",
				wlc->pub->unit, body_len,
				(uint)(sizeof(struct dot11_auth) + 2 + challenge[1])));
			return;
		}
		WL_ASSOC(("wl%d: JOIN: got authentication response seq 2 ...\n",
		            WLCWLUNIT(wlc)));
		wlc_assoc_change_state(cfg, AS_SENT_AUTH_3);
		pkt = wlc_sendauth(cfg, &scb->ea, &target_bss->BSSID, scb, DOT11_SHARED_KEY, 3,
			DOT11_SC_SUCCESS, (WSEC_SCB_KEY_VALID(scb) ? scb->key : NULL), challenge,
			short_preamble);
		wlc_assoc_timer_del(wlc, cfg);
		if (pkt != NULL)
			wlc_pkt_callback_register(wlc, wlc_auth_tx_complete,
			                          (void *)(uintptr)cfg->ID, pkt);
		else
			wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT + 10, 0);
	} else {
		/* authentication success */
		ASSERT(auth_seq == 2 || (auth_alg == DOT11_SHARED_KEY && auth_seq == 4));
#if defined(WLFBT)
		if (WLFBT_ENAB(wlc->pub) && SUP_ENAB(wlc->pub) &&
			(cfg->auth_atmptd == DOT11_FAST_BSS)) {
			if (!wlc_sup_ft_authresp(cfg->sup, (uint8 *)&auth[1],
				(body_len - DOT11_AUTH_FIXED_LEN)))
				wlc_auth_complete(cfg, WLC_E_STATUS_FAIL, &hdr->sa, auth_status,
					auth_alg);
		}
#endif /* WLFBT */
		wlc_auth_complete(cfg, WLC_E_STATUS_SUCCESS, &hdr->sa, auth_status, auth_alg);
	}
}


#if defined(WLFBT)
void
wlc_ftresp_client(wlc_bsscfg_t *cfg, struct dot11_management_header *hdr, void *body,
	uint body_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	uint16 resp_status;
#ifdef BCMDBG_ERR
	char eabuf[ETHER_ADDR_STR_LEN], *sa = bcm_ether_ntoa(&hdr->sa, eabuf);
#endif /* BCMDBG_ERR */
	dot11_ft_res_t* ft_resp = (dot11_ft_res_t*)body;
	struct scb *scb;
	bool ft_band_changed = TRUE;

	WL_TRACE(("wl%d: wlc_ftresp_client\n", wlc->pub->unit));

	/* Is this request for an AP config or a STA config? */
	if (!BSSCFG_STA(cfg) || (!cfg->BSS)) {
		WL_ASSOC(("wl%d: %s: FC_ACTION FT Response: unknown bsscfg _ap %d"
			" bsscfg->BSS %d\n", WLCWLUNIT(wlc), __FUNCTION__,
			cfg->_ap, cfg->BSS));
		return;  /* We couldn't match the incoming frame to a BSS config */
	}

	/* ignore ft_resp frames from other stations */
	if ((ft_resp->action != DOT11_FT_ACTION_FT_RES) ||
		(as->state != AS_SENT_FTREQ) ||
		bcmp((char*)&hdr->sa, (char*)&cfg->current_bss->BSSID, ETHER_ADDR_LEN) ||
		bcmp((char*)&ft_resp->tgt_ap_addr, (char*)&target_bss->BSSID, ETHER_ADDR_LEN) ||
		!wlc_scbfind(wlc, cfg, &cfg->current_bss->BSSID) ||
		!(scb = wlc_scbfind(wlc, cfg, &target_bss->BSSID))) {
		WL_ERROR(("wl%d.%d: unsolicited FT response from %s",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), sa));
		WL_ERROR((" for  %s\n", bcm_ether_ntoa((struct ether_addr *)&ft_resp->tgt_ap_addr,
			eabuf)));

		wlc_auth_complete(cfg, WLC_E_STATUS_UNSOLICITED, &target_bss->BSSID, 0, 0);
		return;
	}

	resp_status = ltoh16(ft_resp->status);

	/* authentication error */
	if (resp_status != DOT11_SC_SUCCESS) {
		wlc_auth_complete(cfg, WLC_E_STATUS_FAIL, &target_bss->BSSID, resp_status,
			DOT11_FAST_BSS);
		return;
	}

#if defined(BCMSUP_PSK)
	if (!wlc_sup_ft_authresp(cfg->sup, (uint8 *)ft_resp->data,
		(body_len - DOT11_FT_RES_FIXED_LEN))) {
		wlc_auth_complete(cfg, WLC_E_STATUS_FAIL, &target_bss->BSSID, resp_status,
			DOT11_FAST_BSS);
		return;
	}
#endif

	{
		/* authentication success */
		wlcband_t *band;
		bool ch_changed = FALSE;
		chanspec_t chanspec = target_bss->chanspec;
		bool switch_scb_band;

		/* Set switch_scb_band before wlc_set_chanspec has a chance to change
		 * wlc->band->bandunit.
		 */
		switch_scb_band = wlc->band->bandunit != CHSPEC_WLCBANDUNIT(chanspec);

		band = wlc->bandstate[CHSPEC_IS2G(chanspec) ? BAND_2G_INDEX : BAND_5G_INDEX];

		/* Sanitize user setting for 80MHz against current settings
		 * Reduce an 80MHz chanspec to 40MHz if needed.
		 */
		if (CHSPEC_IS80_UNCOND(chanspec) &&
		    (!VHT_ENAB_BAND(wlc->pub, band->bandtype) ||
		     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) & WLC_NO_80MHZ) ||
		     !WL_BW_CAP_80MHZ(band->bw_cap))) {
			/* select the 40MHz primary channel in case 40 is allowed */
			chanspec = wf_chspec_primary40_chspec(chanspec);
		}

		/* Check if we're going to change bands */
		if (CHSPEC_BAND(cfg->current_bss->chanspec) == CHSPEC_BAND(chanspec))
			ft_band_changed = FALSE;

		/* Convert a 40MHz AP channel to a 20MHz channel if we are not in NMODE or
		 * the locale does not allow 40MHz
		 * or the band is not configured for 40MHz operation
		 * Note that the unconditional version of the CHSPEC_IS40 is used so that
		 * code compiled without MIMO support will still recognize and convert
		 * a 40MHz chanspec.
		 */
		if (CHSPEC_IS40_UNCOND(chanspec) &&
			(!N_ENAB(wlc->pub) ||
			(wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) &
			WLC_NO_40MHZ) ||
		         !WL_BW_CAP_40MHZ(band->bw_cap) ||
			(BAND_2G(band->bandtype) && WLC_INTOL40_DET(cfg)))) {
			uint channel;
			channel = wf_chspec_ctlchan(chanspec);
			chanspec = CH20MHZ_CHSPEC(channel);
		}

		/* Change the radio channel to match the target_bss */
		if ((WLC_BAND_PI_RADIO_CHANSPEC != chanspec)) {
			/* clear the quiet bit on the dest channel */
			wlc_clr_quiet_chanspec(wlc->cmi, chanspec);
			wlc_suspend_mac_and_wait(wlc);
			wlc_set_chanspec(wlc, chanspec);
#ifdef WLMCHAN
			if (MCHAN_ENAB(wlc->pub) && cfg->chan_context->qi != wlc->primary_queue) {
				wlc_primary_queue_set(wlc, cfg->chan_context->qi);
			}
#endif
			wlc_enable_mac(wlc);
			ch_changed = TRUE;
		}

		wlc_rate_lookup_init(wlc, &target_bss->rateset);

		if ((WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band)) &&
			ft_band_changed &&
			(as->type != AS_ROAM || ch_changed)) {
			wlc_full_phy_cal(wlc, cfg, PHY_PERICAL_JOIN_BSS);
			if (NREV_GE(wlc->band->phyrev, 3) || WLCISHTPHY(wlc->band)) {
				wlc_phy_interference_set(wlc->band->pi, TRUE);

				wlc_phy_acimode_noisemode_reset(wlc->band->pi,
					CHSPEC_CHANNEL(chanspec), FALSE, TRUE, FALSE);
			}
		}
		/* The scb for target_bss was created in wlc_join_BSS on the channel for the
		 * current_bss. We may need to switch the target_bss scb to the new band if we've
		 * successfully performed an FT over-the-DS reassoc.
		 */
		if (switch_scb_band) {
			wlc_scb_switch_band(wlc, scb, wlc->band->bandunit, cfg);
			wlc_rateset_filter(&wlc->band->hw_rateset, &scb->rateset, FALSE,
				WLC_RATES_CCK_OFDM, RATE_MASK, BSS_N_ENAB(wlc, scb->bsscfg));
			wlc_scb_ratesel_init(wlc, scb);
		}
	}
	wlc_auth_complete(cfg, WLC_E_STATUS_SUCCESS, &target_bss->BSSID, resp_status,
		DOT11_FAST_BSS);

}
#endif /* WLFBT */


void
wlc_clear_hw_association(wlc_bsscfg_t *cfg, bool mute_mode)
{
	wlc_info_t *wlc = cfg->wlc;
	d11regs_t *regs = wlc->regs;
	wlc_rateset_t rs;

	wlc_set_pmoverride(cfg, FALSE);

	/* zero the BSSID so the core will not process TSF updates */
	wlc_bss_clear_bssid(cfg);

	/* Clear any possible Channel Switch Announcement */
	if (WL11H_ENAB(wlc))
		wlc_csa_reset_all(wlc->csa, cfg);

	cfg->assoc->rt = FALSE;

	/* reset quiet channels to the unassociated state */
	wlc_quiet_channels_reset(wlc->cmi);

	if (!DEVICEREMOVED(wlc)) {
		wlc_suspend_mac_and_wait(wlc);

		if (wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) &&
		    mute_mode == ON)
			wlc_mute(wlc, ON, 0);

		/* clear BSSID in PSM */
		wlc_clear_bssid(cfg);

		/* write the beacon interval to the TSF block */
		W_REG(wlc->osh, &regs->tsf_cfprep, 0x80000000);

		/* gphy, aphy use the same CWMIN */
		wlc_bmac_set_cwmin(wlc->hw, APHY_CWMIN);
		wlc_bmac_set_cwmax(wlc->hw, PHY_CWMAX);

		if (BAND_2G(wlc->band->bandtype))
			wlc_bmac_set_shortslot(wlc->hw, wlc->shortslot);

		/* Restore the frequency tracking bandwidth of the PHY, if we modified it */
		wlc_freqtrack_reset(wlc);

		wlc_enable_mac(wlc);

		/* Reset the basic rate lookup table to phy mandatory rates */
		rs.count = 0;
		wlc_rate_lookup_init(wlc, &rs);
		wlc_set_ratetable(wlc);
	}
}

static int
wlc_join_start_ibss(wlc_bsscfg_t *cfg, const uint8 ssid[], int ssid_len)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_bss_info_t bi;
	wlcband_t *band;
	uint channel;

	ASSERT(ssid_len > 0);
	ASSERT(ssid_len <= DOT11_MAX_SSID_LEN);

	/* set up default BSS params */
	bcopy(wlc->default_bss, &bi, sizeof(bi));

	/* create IBSS BSSID using a random number */
	wlc_getrand(wlc, &bi.BSSID.octet[0], ETHER_ADDR_LEN);

	/* Set the first 2 MAC addr bits to "locally administered" */
	ETHER_SET_LOCALADDR(&bi.BSSID.octet[0]);
	ETHER_SET_UNICAST(&bi.BSSID.octet[0]);


	/* adopt previously specified params */
	bi.infra = 0;
	bzero(bi.SSID, sizeof(bi.SSID));
	bi.SSID_len = (uint8)ssid_len;
	bcopy(ssid, bi.SSID, MIN((uint)ssid_len, sizeof(bi.SSID)));

	/* Check if 40MHz channel bandwidth is allowed in current locale and band and
	 * convert to 20MHz if not allowed
	 */
	band = wlc->bandstate[CHSPEC_IS2G(bi.chanspec) ? BAND_2G_INDEX : BAND_5G_INDEX];
	if (CHSPEC_IS40(bi.chanspec) &&
	    (!N_ENAB(wlc->pub) ||
	     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) & WLC_NO_40MHZ) ||
	     !WL_BW_CAP_40MHZ(band->bw_cap))) {
		channel = wf_chspec_ctlchan(bi.chanspec);
		bi.chanspec = CH20MHZ_CHSPEC(channel);
	}

	/*
	   Validate or fixup default channel value.
	   Don't want to create ibss on quiet channel since it hasn't be
	   verified as radar-free.
	*/
	if (!wlc_valid_chanspec_db(wlc->cmi, bi.chanspec) ||
	    wlc_quiet_chanspec(wlc->cmi, bi.chanspec)) {
		chanspec_t chspec;
		chspec = wlc_next_chanspec(wlc->cmi, bi.chanspec, CHAN_TYPE_CHATTY, TRUE);
		if (chspec == INVCHANSPEC) {
			wlc_set_ssid_complete(cfg, WLC_E_STATUS_NO_NETWORKS, &cfg->BSSID,
				DOT11_BSSTYPE_INDEPENDENT);
			return BCME_ERROR;
		}
		bi.chanspec = chspec;
	}

#ifdef CREATE_IBSS_ON_QUIET_WITH_11H
	/* For future reference, if we do support ibss creation
	 * on radar channels, need to unmute and clear quiet bit.
	 */
#ifdef BAND5G     /* RADAR */
	if (WLC_BAND_PI_RADIO_CHANSPEC != bi.chanspec) {
		wlc_clr_quiet_chanspec(wlc->cmi, bi.chanspec);
		/* set_channel() will unmute */
	} else if (wlc_quiet_chanspec(wlc->cmi, bi.chanspec)) {
		wlc_clr_quiet_chanspec(wlc->cmi, bi.chanspec);
		wlc_mute(wlc, OFF, 0);
	}
#endif /* BAND5G */
#endif /* CREATE_IBSS_ON_QUIET_WITH_11H */

#ifdef WL11N
	/* BSS rateset needs to be adjusted to account for channel bandwidth */
	wlc_rateset_bw_mcs_filter(&bi.rateset,
		WL_BW_CAP_40MHZ(wlc->band->bw_cap)?CHSPEC_WLC_BW(bi.chanspec):0);
#endif

	if (WSEC_ENABLED(cfg->wsec) && (cfg->wsec_restrict)) {
		WL_WSEC(("%s(): set bi->capability DOT11_CAP_PRIVACY bit.\n", __FUNCTION__));
		bi.capability |= DOT11_CAP_PRIVACY;
	}
	 /* Update capability to reflect IBSS */
	 bi.capability |= DOT11_CAP_IBSS;

	wlc_suspend_mac_and_wait(wlc);
	wlc_BSSinit(wlc, &bi, cfg, WLC_BSS_START);
	wlc_enable_mac(wlc);

	/* initialize link state tracking to the lost beacons condition */
	cfg->roam->time_since_bcn = 1;
	cfg->roam->bcns_lost = TRUE;

	wlc_sta_assoc_upd(cfg, TRUE);
	WL_RTDC(wlc, "wlc_join_start_ibss: associated", 0, 0);

	/* force a PHY cal on the current IBSS channel */
	if (WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band) || WLCISACPHY(wlc->band))
		wlc_full_phy_cal(wlc, cfg, PHY_PERICAL_START_IBSS);

	wlc_bsscfg_up(wlc, cfg);

	WL_ERROR(("wl%d: IBSS started\n", wlc->pub->unit));
	/* N.B.: bss_type passed through auth_type event field */
	wlc_bss_mac_event(wlc, cfg, WLC_E_START, &cfg->BSSID,
	            WLC_E_STATUS_SUCCESS, 0, DOT11_BSSTYPE_INDEPENDENT,
	            NULL, 0);
	return 0;
}

static void
wlc_disassoc_done_quiet_chl(wlc_info_t *wlc, uint txstatus, void *arg)
{

	if (wlc_quiet_chanspec(wlc->cmi, WLC_BAND_PI_RADIO_CHANSPEC) &&
		wlc->pub->up) {
			WL_ASSOC(("%s: muting the channel \n", __FUNCTION__));
			wlc_mute(wlc, ON, 0);
	}
}
int
wlc_disassociate_client(wlc_bsscfg_t *cfg, bool send_disassociate, pkcb_fn_t fn, void *arg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	struct scb *scb;
	struct ether_addr BSSID;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	uint bsstype;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	bool mute_mode = ON;

	WL_TRACE(("wl%d: wlc_disassociate_client\n", wlc->pub->unit));

	if (wlc->pub->associated == FALSE)
		return (-1);
	if (!cfg->associated)
		return (-1);

	if (cfg->BSS) {
		bcopy(&cfg->prev_BSSID, &BSSID, ETHER_ADDR_LEN);
	} else {
		bcopy(&current_bss->BSSID, &BSSID, ETHER_ADDR_LEN);
	}
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	bcm_ether_ntoa(&BSSID, eabuf);
#endif /* BCMDBG || WLMSG_ASSOC */



#ifdef	WLCAC
	if (CAC_ENAB(wlc->pub))
		wlc_cac_on_leave_bss(wlc->cac);
#endif	/* WLCAC */


#ifdef WLTDLS
	/* cleanup the TDLS peers which require an association */
	if (TDLS_ENAB(wlc->pub))
		wlc_tdls_cleanup(wlc->tdls, cfg);
#endif 

#ifdef WLMCNX
	/* reset p2p assoc states */
	if (MCNX_ENAB(wlc->pub) && cfg->BSS)
		wlc_mcnx_assoc_upd(wlc->mcnx, cfg, FALSE);
#endif

	if (DEVICEREMOVED(wlc)) {
		wlc_sta_assoc_upd(cfg, FALSE);
		wlc_clear_hw_association(cfg, mute_mode);
		wlc_disassoc_complete(cfg, WLC_E_STATUS_SUCCESS, &BSSID,
			DOT11_RC_DISASSOC_LEAVING, WLC_DOT11_BSSTYPE(cfg->BSS));
		return (0);
	}

	/* BSS STA */
	if (cfg->BSS) {
		/* cache BSS type for disassoc indication */
		bsstype = DOT11_BSSTYPE_INFRASTRUCTURE;

		/* abort any association state machine in process */
		if ((as->state != AS_IDLE) && (as->state != AS_WAIT_DISASSOC))
			wlc_assoc_abort(cfg);

		scb = wlc_scbfind(wlc, cfg, &BSSID);

		/* Clear any pending countermeasure activations - don't want to activate
		 * countermeasures on the next 802.1x frame from a different BSS
		 */
		cfg->tk_cm_activate = FALSE;

		/* clear PM state, (value used for wake shouldn't matter) */
		wlc_update_bcn_info(cfg, FALSE);

		wlc_pspoll_timer_upd(cfg, FALSE);
		wlc_apsd_trigger_upd(cfg, FALSE);

#ifdef QUIET_DISASSOC
		if (send_disassociate) {
			send_disassociate = FALSE;
		}
#endif /* QUIET_DISASSOC */
#ifdef WLTDLS
		if (TDLS_ENAB(wlc->pub) && wlc_tdls_quiet_down(wlc->tdls)) {
			WL_ASSOC(("wl%d: JOIN: skipping DISASSOC to %s since we are "
					    "quite down.\n", WLCWLUNIT(wlc), eabuf));
			send_disassociate = FALSE;
		}
#endif
		/* Send disassociate packet and (attempt to) schedule callback */
		if (send_disassociate) {
			if (ETHER_ISNULLADDR(cfg->BSSID.octet)) {
				/* a NULL BSSID indicates that we have given up on our AP connection
				 * to the point that we will reassociate to it if we ever see it
				 * again. In this case, we should not send a disassoc
				 */
				WL_ASSOC(("wl%d: JOIN: skipping DISASSOC to %s since we lost "
					    "contact.\n", WLCWLUNIT(wlc), eabuf));
			} else if (wlc_radar_chanspec(wlc->cmi, current_bss->chanspec) ||
			        wlc_restricted_chanspec(wlc->cmi, current_bss->chanspec)) {
				/* note that if the channel is a radar or restricted channel,
				 * Permit sending disassoc packet if no subsequent processing
				 * is waiting  (indicated by the presence of callbcak routine)
				 */
				if (fn != NULL) {

					WL_ASSOC(("wl%d: JOIN: skipping DISASSOC to %s"
						"since channel %d is quiet and call back\n",
						WLCWLUNIT(wlc), eabuf,
						CHSPEC_CHANNEL(current_bss->chanspec)));

				} else if (!wlc_quiet_chanspec(wlc->cmi, current_bss->chanspec) &&
					(current_bss->chanspec == WLC_BAND_PI_RADIO_CHANSPEC)) {

						void *pkt;

						WL_ASSOC(("wl%d: JOIN: sending DISASSOC to %s on "
							"radar/restricted channel \n",
							WLCWLUNIT(wlc), eabuf));

						pkt = wlc_senddisassoc(wlc, &BSSID, &BSSID,
							&cfg->cur_etheraddr, scb,
							DOT11_RC_DISASSOC_LEAVING);

						if (wlc_pkt_callback_register(wlc,
							wlc_disassoc_done_quiet_chl, NULL, pkt)) {
							WL_ERROR(("wl%d: %s out of pkt callbacks\n",
								wlc->pub->unit, __FUNCTION__));
						} else {
							mute_mode = OFF;
						}
				} else {
					WL_ASSOC(("wl%d: JOIN: Skipping DISASSOC to %s since "
						"present channel not home channel \n",
						WLCWLUNIT(wlc), eabuf));
				}
			} else if (current_bss->chanspec == WLC_BAND_PI_RADIO_CHANSPEC) {
				void *pkt;

				WL_ASSOC(("wl%d: JOIN: sending DISASSOC to %s\n",
				    WLCWLUNIT(wlc), eabuf));

				pkt = wlc_senddisassoc(wlc, &BSSID, &BSSID,
				                       &cfg->cur_etheraddr,
				                       scb, DOT11_RC_DISASSOC_LEAVING);
				if (pkt == NULL)
					WL_ASSOC(("wl%d: JOIN: error sending DISASSOC\n",
					          WLCWLUNIT(wlc)));
				else if (fn != NULL) {
					if (wlc_pkt_callback_register(wlc, fn, arg, pkt)) {
						WL_ERROR(("wl%d: %s out of pkt callbacks\n",
							wlc->pub->unit, __FUNCTION__));
					} else {
						WLPKTTAGBSSCFGSET(pkt, WLC_BSSCFG_IDX(cfg));
						/* the callback was registered, so clear fn local
						 * so it will not be called at the end of this
						 * function
						 */
						fn = NULL;
					}
				}
			}
		}

		/* reset scb state */
		if (scb) {

			wlc_scb_clearstatebit(scb, ASSOCIATED | PENDING_AUTH | PENDING_ASSOC);



			wlc_scb_disassoc_cleanup(wlc, scb);
		}

#if NCONF
		if (WLCISHTPHY(wlc->band) ||
		    (WLCISNPHY(wlc->band) && (D11REV_GE(wlc->pub->corerev, 16)))) {
			wlc_bmac_ifsctl_edcrs_set(wlc->hw, TRUE);
		}
#endif /* NCONF */

	}
	/* IBSS STA */
	else {
		/* cache BSS type for disassoc indication */
		bsstype = DOT11_BSSTYPE_INDEPENDENT;

		wlc_ibss_disable(cfg);
	}

	/* Switch to BSS STA mode */
	wlc_bsscfg_set_infra_mode(wlc, cfg, TRUE);

	/* update association states */
#ifdef PSTA
	if (PSTA_ENAB(wlc->pub) && (cfg == wlc_bsscfg_primary(wlc))) {
		wlc_bsscfg_t *bsscfg;
		int32 idx;

		FOREACH_PSTA(wlc, idx, bsscfg) {
			/* Cleanup the proxy client state */
			wlc_sta_assoc_upd(bsscfg, FALSE);
		}
	}
#endif /* PSTA */
	wlc_sta_assoc_upd(cfg, FALSE);

	if (!wlc->pub->associated) {
		/* if auto shortslot, switch back to 11b compatible long slot */
		if (wlc->shortslot_override == WLC_SHORTSLOT_AUTO)
			wlc->shortslot = FALSE;
	}

	/* init protection configuration */
	wlc_prot_g_cfg_init(wlc->prot_g, cfg);
#ifdef WL11N
	if (N_ENAB(wlc->pub))
		wlc_prot_n_cfg_init(wlc->prot_n, cfg);
#endif

	if (!ETHER_ISNULLADDR(cfg->BSSID.octet)) {
		wlc_disassoc_complete(cfg, WLC_E_STATUS_SUCCESS, &BSSID,
			DOT11_RC_DISASSOC_LEAVING, bsstype);
	}

	if (!AP_ACTIVE(wlc) && cfg == wlc->cfg) {
		WL_APSTA_UPDN(("wl%d: wlc_disassociate_client: wlc_clear_hw_association\n",
			wlc->pub->unit));
		wlc_clear_hw_association(cfg, mute_mode);
	} else {
		WL_APSTA_BSSID(("wl%d: wlc_disassociate_client -> wlc_clear_bssid\n",
			wlc->pub->unit));
		wlc_clear_bssid(cfg);
		wlc_bss_clear_bssid(cfg);
	}

	if (current_bss->bcn_prb) {
		MFREE(wlc->osh, current_bss->bcn_prb, current_bss->bcn_prb_len);
		current_bss->bcn_prb = NULL;
		current_bss->bcn_prb_len = 0;
	}

	WL_APSTA_UPDN(("wl%d: Reporting link down on config 0 (STA disassociating)\n",
	               WLCWLUNIT(wlc)));

	if (WLCISNPHY(wlc->band) || WLCISHTPHY(wlc->band)) {
		if (NREV_GE(wlc->band->phyrev, 3) || WLCISHTPHY(wlc->band)) {
			wlc_phy_acimode_noisemode_reset(wlc->band->pi,
				CHSPEC_CHANNEL(current_bss->chanspec), TRUE, FALSE, TRUE);
		}
	}

	wlc_link(wlc, FALSE, &BSSID, cfg, WLC_E_LINK_DISASSOC);

	/* reset rssi moving average */
	wlc_lq_rssi_reset_ma(cfg, WLC_RSSI_INVALID);
	wlc_lq_rssi_event_update(cfg);

#ifdef WLLED
	wlc_led_event(wlc->ledh);
#endif

	/* disable radio due to end of association */
	WL_MPC(("wl%d: disassociation wlc->pub->associated==FALSE, update mpc\n", wlc->pub->unit));
	wlc_radio_mpc_upd(wlc);

	/* call the given callback fn if it has not been taken care of with
	 * a disassoc packet callback.
	 */
	if (fn)
		(*fn)(wlc, TX_STATUS_NO_ACK, arg);

	/* clean up... */
	bzero(target_bss->BSSID.octet, ETHER_ADDR_LEN);
	bzero(cfg->BSSID.octet, ETHER_ADDR_LEN);

	return (0);
}

static void
wlc_assoc_success(wlc_bsscfg_t *cfg, struct scb *scb)
{
	wlc_info_t *wlc = cfg->wlc;
	struct scb *prev_scb;

	ASSERT(scb != NULL);

#ifdef PROP_TXSTATUS
	/* allocate handle only if not allocated already for this scb */
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		if (scb->mac_address_handle == 0)
			scb->mac_address_handle = wlfc_allocate_MAC_descriptor_handle(
				cfg->wlc->wlfc_data);
		if (BCME_OK != wlfc_MAC_table_update(cfg->wlc->wl, &scb->ea.octet[0],
			WLFC_CTL_TYPE_MACDESC_ADD, scb->mac_address_handle,
			((scb->bsscfg == NULL) ? 0 : scb->bsscfg->wlcif->index))) {
			WLFC_DBGMESG(("ERROR: %s() wlfc_MAC_table_update() failed.\n",
				__FUNCTION__));
		}
		WLFC_DBGMESG(("STA: MAC-ADD for [%02x:%02x:%02x:%02x:%02x:%02x], "
			"handle: [%d], if:%d, t_idx:%d\n",
			scb->ea.octet[0], scb->ea.octet[1], scb->ea.octet[2],
			scb->ea.octet[3], scb->ea.octet[4], scb->ea.octet[5],
			scb->mac_address_handle,
			((scb->bsscfg == NULL) ? 0 : scb->bsscfg->wlcif->index),
			WLFC_MAC_DESC_GET_LOOKUP_INDEX(scb->mac_address_handle)));
	}
#endif /* PROP_TXSTATUS */

	wlc_scb_clearstatebit(scb, PENDING_AUTH | PENDING_ASSOC);

	wlc_assoc_timer_del(wlc, cfg);

	/* clean up before leaving the BSS */
	if (cfg->BSS && cfg->associated) {
		prev_scb = wlc_scbfindband(wlc, cfg, &cfg->prev_BSSID,
			CHSPEC_WLCBANDUNIT(cfg->current_bss->chanspec));
		if (prev_scb) {

#ifdef WLFBT
			if (WLFBT_ENAB(wlc->pub) && (cfg->WPA_auth & WPA2_AUTH_FT) && cfg->sup &&
				CAC_ENAB(wlc->pub) && (wlc->cac != NULL)) {
				wlc_cac_copy_state(wlc->cac, prev_scb, scb);
			}
#endif /* WLFBT */
			wlc_scb_clearstatebit(prev_scb, ASSOCIATED | AUTHORIZED);


			/* delete old AP's pairwise key */
			wlc_scb_disassoc_cleanup(wlc, prev_scb);
		}
	}

	/* clear PM state */
	wlc_set_pmoverride(cfg, FALSE);
	wlc_update_bcn_info(cfg, FALSE);

	/* update scb state */
	wlc_scb_setstatebit(scb, ASSOCIATED);

	/* init per scb WPA_auth */
	scb->WPA_auth = cfg->WPA_auth;
	WL_WSEC(("wl%d: WPA_auth 0x%x\n", wlc->pub->unit, scb->WPA_auth));

	/* adopt the BSS parameters */
	wlc_join_adopt_bss(cfg);

	if (((scb->flags & SCB_HTCAP) == 0) && (scb->wsec == TKIP_ENABLED)) {
		WL_INFORM(("Activating MHF4_CISCOTKIP_WAR\n"));
		wlc_mhf(wlc, MHF4, MHF4_CISCOTKIP_WAR, MHF4_CISCOTKIP_WAR, WLC_BAND_ALL);
	} else {
		WL_INFORM(("Deactivating MHF4_CISCOTKIP_WAR\n"));
		wlc_mhf(wlc, MHF4, MHF4_CISCOTKIP_WAR, 0, WLC_BAND_ALL);
	}
	/* 11g hybrid coex cause jerky mouse, disable for now. do not apply for ECI chip for now */
	if (!SCB_HT_CAP(scb) && !BCMECICOEX_ENAB(wlc))
		wlc_mhf(wlc, MHF3, MHF3_BTCX_SIM_RSP, 0, WLC_BAND_2G);

#if NCONF
	/* temporary WAR to improve Tx throughput in a non-N mode association */
	/* to share the medium with other B-only STA: Enable EDCRS when assoc */
	if (WLCISHTPHY(wlc->band) ||
	    (WLCISNPHY(wlc->band) && (D11REV_GE(wlc->pub->corerev, 16)))) {
		wlc_bmac_ifsctl_edcrs_set(wlc->hw, ((scb->flags & SCB_HTCAP) != 0));
	}
#endif /* NCONF */

#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub) && scb) {
		wlc_txbf_init_link(wlc->txbf, scb);
	}
#endif /*  WL_BEAMFORMING */

#ifdef PLC_WET
	/* Add node so that we can route the frames appropriately */
	if (PLC_ENAB(wlc->pub))
		wlc_plc_node_add(wlc, NODE_TYPE_WIFI_ONLY, scb);
#endif /* PLC_WET */
}

static void
wlc_auth_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr* addr,
	uint auth_status, uint auth_type)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_bss_info_t *target_bss = cfg->target_bss;
	bool more_to_do_after_event = FALSE;
	struct scb *scb;
	void *pkt;
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || defined(WLEXTLOG)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_ASSOC */
#ifdef WLFBT
	bool allow_reassoc = FALSE;
#endif /* WLFBT */

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || defined(WLEXTLOG)
	if (addr != NULL)
		bcm_ether_ntoa(addr, eabuf);
	else
		strncpy(eabuf, "<NULL>", sizeof(eabuf) - 1);
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_ASSOC */

	if (status == WLC_E_STATUS_UNSOLICITED)
		goto do_event;

	if (!(as->state == AS_SENT_AUTH_1 || as->state == AS_SENT_AUTH_3 ||
		as->state == AS_SENT_FTREQ))
		goto do_event;

	/* Clear pending bits */
	scb = addr ? wlc_scbfind(wlc, cfg, addr): NULL;
	if (scb)
		wlc_scb_clearstatebit(scb, PENDING_AUTH | PENDING_ASSOC);

	if (status != WLC_E_STATUS_TIMEOUT) {
		wlc_assoc_timer_del(wlc, cfg);
	}
	if (status == WLC_E_STATUS_SUCCESS) {
		WL_ASSOC(("wl%d: JOIN: authentication success\n",
		          WLCWLUNIT(wlc)));
		ASSERT(scb != NULL);
		wlc_scb_setstatebit(scb, AUTHENTICATED | PENDING_ASSOC);
		scb->flags &= ~SCB_SHORTPREAMBLE;
		if ((target_bss->capability & DOT11_CAP_SHORT) != 0)
			scb->flags |= SCB_SHORTPREAMBLE;
		WL_APSTA(("wl%d: WLC_E_AUTH for %s, forcing MYAP flag.\n",
		          wlc->pub->unit, eabuf));
		scb->flags |= SCB_MYAP;
		wlc_assoc_change_state(cfg, AS_SENT_ASSOC);
#ifdef WLFBT
		/* Send out assoc request instead of reassoc request when
		 * - transitioning to an AP with different security type.The right
		 *   way to check for security mismatch is by comparing the AKM suite
		 *   to be used for new association(cfg->wpa2_auth_ft) with the one used
		 *   for previous association.(However current_bss stores the
		 *   list of all target AKM suites which does not indicate the one
		 *   actually used for association).
		 * - there is link down or disassoc, as indicated by null current_bss->BSSID.
		 * - switching to a different FT mobilty domain
		 */
		if (!ETHER_ISNULLADDR(&cfg->current_bss->BSSID) &&
			(target_bss->wpa2.flags & RSN_FLAGS_SUPPORTED) &&
			(cfg->current_bss->wpa2.flags & RSN_FLAGS_SUPPORTED) &&
			((WLFBT_ENAB(wlc->pub) &&
			wlc_sup_is_cur_mdid(cfg->sup, target_bss) && (cfg->wpa2_auth_ft)) ==
			((cfg->current_bss->wpa2.flags & RSN_FLAGS_FBT) != 0))) {
				allow_reassoc = TRUE;
		}
		pkt = wlc_sendassocreq(wlc, target_bss, scb, cfg->associated && allow_reassoc);
#else
		pkt = wlc_sendassocreq(wlc, target_bss, scb, cfg->associated);
#endif /* WLFBT */

#if defined(WLP2P) && defined(BCMDBG)
		if (WL_P2P_ON()) {
			int bss = wlc_mcnx_d11cb_idx(wlc->mcnx, cfg);
			uint16 state = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_ST(bss));
			uint16 next_noa = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_BSS_N_NOA(bss));
			uint16 hps = wlc_mcnx_read_shm(wlc->mcnx, M_P2P_HPS);

			WL_P2P(("wl%d: %s: queue ASSOC at tick 0x%x ST 0x%04X "
			        "N_NOA 0x%X HPS 0x%04X\n",
			        wlc->pub->unit, __FUNCTION__,
			        R_REG(wlc->osh, &wlc->regs->tsf_timerlow),
			        state, next_noa, hps));
		}
#endif /* WLP2P && BCMDBG */
		wlc_assoc_timer_del(wlc, cfg);
		if (pkt != NULL)
			wlc_pkt_callback_register(wlc, wlc_assocreq_complete,
			                          (void *)(uintptr)cfg->ID, pkt);
		else
			wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT + 10, 0);
		goto do_event;
	} else if (status == WLC_E_STATUS_TIMEOUT) {
		WL_ASSOC(("wl%d: JOIN: timeout waiting for authentication "
			"response, assoc_state %d\n",
			WLCWLUNIT(wlc), as->state));
	} else if (status == WLC_E_STATUS_NO_ACK) {
		WL_ASSOC(("wl%d.%d: JOIN: authentication failure, no ack from %s\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), eabuf));
#ifdef WLFBT
		/* No response from current AP for FT Request frame,
		 * join target AP using FBT over-the-air.
		 */
		if (WLFBT_ENAB(wlc->pub) && (target_bss->flags2 & WLC_BSS_OVERDS_FBT) &&
		    (as->state == AS_SENT_FTREQ)) {
			wlc_join_BSS(cfg, wlc->join_targets->ptrs[wlc->join_targets_last]);
			return;
		}
#endif /* WLFBT */
	} else if (status == WLC_E_STATUS_FAIL) {
#ifdef PSTA
		wsec_key_t *defkey = WSEC_BSS_DEFAULT_KEY(BSSCFG_PSTA(cfg) ?
		                              wlc_bsscfg_primary(wlc) : cfg);
#else /* PSTA */
		wsec_key_t *defkey = WSEC_BSS_DEFAULT_KEY(cfg);
#endif /* PSTA */
		WL_ASSOC(("wl%d: JOIN: authentication failure status %d from %s\n",
		          WLCWLUNIT(wlc), (int)auth_status, eabuf));
		ASSERT(scb != NULL);
		wlc_scb_clearstatebit(scb, AUTHENTICATED);

#if defined(WLFBT)
		if (WLFBT_ENAB(wlc->pub) &&
			((auth_status == DOT11_SC_ASSOC_R0KH_UNREACHABLE) ||
			(auth_status == DOT11_SC_INVALID_PMKID))) {
			wlc_sup_ftauth_clear_ies(wlc, (supplicant_t*)cfg->sup);
			if (auth_status == DOT11_SC_ASSOC_R0KH_UNREACHABLE)
				wlc_assoc_abort(cfg);
			return;
		}
#endif /* WLFBT */

		if (cfg->openshared && cfg->auth_atmptd == DOT11_OPEN_SYSTEM &&
		    auth_status == DOT11_SC_AUTH_MISMATCH &&
		    (target_bss->capability & DOT11_CAP_PRIVACY) &&
		    WSEC_WEP_ENABLED(cfg->wsec) && (defkey != NULL) &&
		    (defkey->algo == CRYPTO_ALGO_WEP1 ||
		    defkey->algo == CRYPTO_ALGO_WEP128)) {
			wlc_bss_info_t* bi = wlc->join_targets->ptrs[wlc->join_targets_last];
			/* Try the current target BSS with DOT11_SHARED_KEY */
			cfg->auth_atmptd = DOT11_SHARED_KEY;
			wlc_assoc_change_state(cfg, AS_SENT_AUTH_1);
			pkt = wlc_sendauth(cfg, &scb->ea, &bi->BSSID, scb,
			                   DOT11_SHARED_KEY, 1, DOT11_SC_SUCCESS,
			                   NULL, NULL, ((bi->capability & DOT11_CAP_SHORT) != 0));
			if (pkt != NULL)
				wlc_pkt_callback_register(wlc, wlc_auth_tx_complete,
				                          (void *)(uintptr)cfg->ID, pkt);
			else
				wl_add_timer(wlc->wl, as->timer, WECA_ASSOC_TIMEOUT + 10, 0);
			return;
		}
	} else if (status == WLC_E_STATUS_ABORT) {
		WL_ASSOC(("wl%d: JOIN: authentication aborted\n", WLCWLUNIT(wlc)));
		goto do_event;
	} else {
		WL_ERROR(("wl%d: %s, unexpected status %d\n",
		          WLCWLUNIT(wlc), __FUNCTION__, (int)status));
		goto do_event;
	}
	more_to_do_after_event = TRUE;

do_event:
	wlc_bss_mac_event(wlc, cfg, WLC_E_AUTH, addr, status, auth_status, auth_type, 0, 0);

	if (!more_to_do_after_event)
		return;

	/* This is when status != WLC_E_STATUS_SUCCESS... */


	/* Try current BSS again */
	if ((status == WLC_E_STATUS_NO_ACK || status == WLC_E_STATUS_TIMEOUT) &&
	    (as->bss_retries < as->retry_max)) {
		WL_ASSOC(("wl%d: Retrying authentication (%d)...\n",
		          WLCWLUNIT(wlc), as->bss_retries));
		wlc_join_bss_start(cfg);
	}
	else { /* Try next BSS */
		WLC_EXTLOG(wlc, LOG_MODULE_ASSOC, FMTSTR_AUTH_FAIL_ID,
		           WL_LOG_LEVEL_ERR, 0, status, eabuf);
		wlc_join_attempt(cfg);
	}
}

static void
wlc_assoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr* addr,
	uint assoc_status, bool reassoc, uint bss_type)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	bool more_to_do_after_event = FALSE;
	struct scb *scb;
#if defined(BCMDBG_ERR) || defined(WLMSG_ASSOC)
	const char* action = (reassoc)?"reassociation":"association";
#endif
#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || defined(WLEXTLOG)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_ASSOC */

#if defined(BCMDBG) || defined(BCMDBG_ERR) || defined(WLMSG_ASSOC) || defined(WLEXTLOG)
	if (addr != NULL)
		bcm_ether_ntoa(addr, eabuf);
	else
		strncpy(eabuf, "<NULL>", sizeof(eabuf) - 1);
#endif /* BCMDBG || BCMDBG_ERR || WLMSG_ASSOC */

	if (status == WLC_E_STATUS_UNSOLICITED)
		goto do_event;

	if (status == WLC_E_STATUS_SUCCESS) {
		WL_ASSOC(("wl%d: JOIN: %s success ...\n", WLCWLUNIT(wlc), action));
		if (!reassoc)
			wlc_bsscfg_up(wlc, cfg);

		if (WOWL_ENAB(wlc->pub) && cfg == wlc->cfg)
			cfg->roam->roam_on_wowl = FALSE;
		/* Restart the ap's in case of a band change */
		if (AP_ACTIVE(wlc)) {
			/* performing a channel change,
			 * all up ap scb's need to be cleaned up
			 */
			wlc_bsscfg_t *apcfg;
			int idx;
			bool mchan_stago_disab =
#ifdef WLMCHAN
				!MCHAN_ENAB(wlc->pub) ||
				wlc_mchan_stago_is_disabled(wlc->mchan);
#else
				TRUE;
#endif

#ifdef AP
#ifdef WLMCHAN
			if (!MCHAN_ENAB(wlc->pub))
#endif
				FOREACH_UP_AP(wlc, idx, apcfg) {
					/* Clean up scbs only when there is a chanspec change */
					if (WLC_BAND_PI_RADIO_CHANSPEC !=
						apcfg->current_bss->chanspec)
							wlc_ap_bsscfg_scb_cleanup(wlc, apcfg);
				}
#endif /* AP */

			FOREACH_UP_AP(wlc, idx, apcfg) {
				if (BSSCFG_AP_MCHAN_DISABLED(wlc, apcfg) || mchan_stago_disab) {
					wlc_txflowcontrol_override(wlc, apcfg->wlcif->qi, OFF,
						TXQ_STOP_FOR_PKT_DRAIN);
					wlc_scb_update_band_for_cfg(wlc, apcfg,
						WLC_BAND_PI_RADIO_CHANSPEC);
				}
			}
			wlc_restart_ap(wlc->ap);
		}
		goto do_event;
	}

	if (!(as->state == AS_SENT_ASSOC || as->state == AS_REASSOC_RETRY))
		goto do_event;

	/* Clear pending bits */
	scb = addr ? wlc_scbfind(wlc, cfg, addr) : NULL;
	if (scb)
		wlc_scb_clearstatebit(scb, PENDING_AUTH | PENDING_ASSOC);

	if (status != WLC_E_STATUS_TIMEOUT) {
		wlc_assoc_timer_del(wlc, cfg);
	}
	if (status == WLC_E_STATUS_TIMEOUT) {
		WL_ASSOC(("wl%d: JOIN: timeout waiting for %s response\n",
		    WLCWLUNIT(wlc), action));
	} else if (status == WLC_E_STATUS_NO_ACK) {
		WL_ASSOC(("wl%d: JOIN: association failure, no ack from %s\n",
		    WLCWLUNIT(wlc), eabuf));
	} else if (status == WLC_E_STATUS_FAIL) {
		WL_ASSOC(("wl%d: JOIN: %s failure %d\n",
		    WLCWLUNIT(wlc), action, (int)assoc_status));
	} else if (status == WLC_E_STATUS_ABORT) {
		WL_ASSOC(("wl%d: JOIN: %s aborted\n", wlc->pub->unit, action));
		goto do_event;
	} else {
		WL_ERROR(("wl%d: %s: %s, unexpected status %d\n",
		    WLCWLUNIT(wlc), __FUNCTION__, action, (int)status));
		goto do_event;
	}
	more_to_do_after_event = TRUE;

do_event:
	wlc_bss_mac_event(wlc, cfg, reassoc ? WLC_E_REASSOC : WLC_E_ASSOC, addr,
		status, assoc_status, bss_type, as->resp, as->resp_len);

	if (!more_to_do_after_event)
		return;

	/* This is when status != WLC_E_STATUS_SUCCESS... */


	/* Try current BSS again */
	if ((status == WLC_E_STATUS_NO_ACK || status == WLC_E_STATUS_TIMEOUT) &&
	    (as->bss_retries < as->retry_max)) {
		WL_ASSOC(("wl%d: Retrying association (%d)...\n",
		          WLCWLUNIT(wlc), as->bss_retries));
		wlc_join_bss_start(cfg);
	}
	/* Try next BSS */
	else {
		WLC_EXTLOG(wlc, LOG_MODULE_ASSOC, FMTSTR_ASSOC_FAIL_ID,
			WL_LOG_LEVEL_ERR, 0, status, eabuf);
		wlc_join_attempt(cfg);
	}
}

static void
wlc_set_ssid_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr, uint bss_type)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	bool retry = FALSE;
	bool assoc_state = FALSE;

	/* Association state machine is halting, clear state */
	wlc_bss_list_free(wlc, wlc->join_targets);

	/* flag to indicate connection completion when abort */
	if (status == WLC_E_STATUS_ABORT)
		assoc_state = ((bss_type == DOT11_BSSTYPE_INDEPENDENT) ?
		               TRUE :
		               (cfg->assoc->state > AS_IDLE &&
		                cfg->assoc->state < AS_WAIT_RCV_BCN));

	/* Association state machine is halting, clear state and allow core to sleep */
	as->type = AS_IDLE;
	wlc_assoc_change_state(cfg, AS_IDLE);

	if ((status != WLC_E_STATUS_SUCCESS) && (status != WLC_E_STATUS_FAIL) &&
	    (status != WLC_E_STATUS_NO_NETWORKS) && (status != WLC_E_STATUS_ABORT))
		WL_ERROR(("wl%d: %s: unexpected status %d\n",
		          WLCWLUNIT(wlc), __FUNCTION__, (int)status));

	if (status != WLC_E_STATUS_SUCCESS) {
		WL_ASSOC(("wl%d: %s: failed status %u\n",
		          WLCWLUNIT(wlc), __FUNCTION__, status));

		/*
		 * If we are here because of a status abort don't check mpc,
		 * it is responsibility of the caller
		*/
		if (status != WLC_E_STATUS_ABORT)
			wlc_radio_mpc_upd(wlc);

		if (status == WLC_E_STATUS_NO_NETWORKS &&
		    cfg->enable) {
			/* retry if configured */

			if (as->ess_retries < as->retry_max) {
				WL_ASSOC(("wl%d: Retrying join (%d)...\n",
				          WLCWLUNIT(wlc), as->ess_retries));
				wlc_join_start(cfg, wlc_bsscfg_scan_params(cfg),
				               wlc_bsscfg_assoc_params(cfg));
				retry = TRUE;
			}
			else if (wlc->sta_retry_time > 0) {
				wl_del_timer(wlc->wl, as->timer);
				wl_add_timer(wlc->wl, as->timer, wlc->sta_retry_time*1000, 0);
				as->rt = TRUE;
			}
		}
	}

	/* no more processing if we are going to retry */
	if (retry)
		return;


	/* free join scan/assoc params */
	if (status == WLC_E_STATUS_SUCCESS) {
		wlc_bsscfg_scan_params_reset(wlc, cfg);
		wlc_bsscfg_assoc_params_reset(wlc, cfg);
	}

	/* APSTA: complete any deferred AP bringup */
	if (AP_ENAB(wlc->pub) && APSTA_ENAB(wlc->pub))
		wlc_restart_ap(wlc->ap);

	/* allow AP to beacon and respond to probe requests */
	if (AP_ACTIVE(wlc)) {
		/* validate the phytxctl for the beacon before turning it on */
		wlc_validate_bcn_phytxctl(wlc, NULL);
	}
	wlc_ap_mute(wlc, FALSE, cfg, -1);

	/* N.B.: assoc_state check passed through status event field */
	/* N.B.: bss_type passed through auth_type event field */
	wlc_bss_mac_event(wlc, cfg, WLC_E_SET_SSID, addr, status, assoc_state, bss_type,
	                  cfg->SSID, cfg->SSID_len);

}

void
wlc_roam_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr, uint bss_type)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	bool assoc_recreate = FALSE;
#ifdef WLMCHAN
	int ret = 0;
#endif


	if (status == WLC_E_STATUS_SUCCESS) {
		WL_ASSOC(("wl%d: JOIN: roam success\n", WLCWLUNIT(wlc)));
	} else if (status == WLC_E_STATUS_FAIL) {
		WL_ASSOC(("wl%d: JOIN: roam failure\n", WLCWLUNIT(wlc)));
#ifdef AP
		if (wlc_join_check_ap_need_csa(wlc, cfg, cfg->current_bss->chanspec,
		                               AS_WAIT_FOR_AP_CSA_ROAM_FAIL)) {
			WL_ASSOC(("wl%d.%d: ROAM FAIL: "
			          "%s delayed due to ap active, wait for ap CSA\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			return;
		}
#endif /* AP */
	} else if (status == WLC_E_STATUS_NO_NETWORKS) {
		WL_ASSOC(("wl%d: JOIN: roam found no networks\n", WLCWLUNIT(wlc)));
	} else if (status == WLC_E_STATUS_ABORT) {
		WL_ASSOC(("wl%d: JOIN: roam aborted\n", WLCWLUNIT(wlc)));
	} else {
		WL_ERROR(("wl%d: %s: unexpected status %d\n",
		    WLCWLUNIT(wlc), __FUNCTION__, (int)status));
	}


	/* Association state machine is halting, clear state */
	wlc_bss_list_free(wlc, wlc->join_targets);

	if (ASSOC_RECREATE_ENAB(wlc->pub))
		assoc_recreate = (as->type == AS_RECREATE);

	as->type = AS_IDLE;
	wlc_assoc_change_state(cfg, AS_IDLE);
#ifdef CCA_STATS
	/* update cca stats since no measurement during roaming */
	cca_stats_upd(wlc, 0);
#endif /* CCA_STATS */


	/* If a roam fails, restore state to that of our current association */
	if (status == WLC_E_STATUS_FAIL) {
		wlc_bss_info_t *current_bss = cfg->current_bss;
		chanspec_t chanspec = current_bss->chanspec;

		/* restore old channel */
#ifdef WLMCHAN
		if (MCHAN_ENAB(wlc->pub)) {
			WL_MCHAN(("wl%d.%d: %s: Restore chanctx for 0x%x\n",
			          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, chanspec));
			ret = wlc_mchan_create_bss_chan_context(wlc, cfg, chanspec);
		}
#endif
		if ((WLC_BAND_PI_RADIO_CHANSPEC != chanspec)) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_set_chanspec(wlc, chanspec);
#ifdef WLMCHAN
			if (!ret && MCHAN_ENAB(wlc->pub) &&
				cfg->chan_context->qi != wlc->primary_queue) {
				wlc_primary_queue_set(wlc, cfg->chan_context->qi);
			}
#endif
			wlc_enable_mac(wlc);
		}
#ifdef WLMCHAN
		/* if we did not change channels we still might need to update
		 * the primay and active queue
		 */
		if (!ret && MCHAN_ENAB(wlc->pub) &&
			cfg->chan_context->qi != wlc->primary_queue) {
			wlc_suspend_mac_and_wait(wlc);
			wlc_primary_queue_set(wlc, cfg->chan_context->qi);
			wlc_enable_mac(wlc);
		}
#endif

		/* restore old basic rates */
		wlc_rate_lookup_init(wlc, &current_bss->rateset);
	}

	/* APSTA: complete any deferred AP bringup */
	if (AP_ENAB(wlc->pub) && APSTA_ENAB(wlc->pub))
		wlc_restart_ap(wlc->ap);

	/* allow AP to beacon and respond to probe requests */
	if (AP_ACTIVE(wlc)) {
		/* validate the phytxctl for the beacon before turning it on */
		wlc_validate_bcn_phytxctl(wlc, NULL);
	}
	wlc_ap_mute(wlc, FALSE, cfg, -1);


	/* N.B.: roam reason passed through status event field */
	/* N.B.: bss_type passed through auth_type event field */
	wlc_bss_mac_event(wlc, cfg, WLC_E_ROAM, addr, status, roam->reason, bss_type, 0, 0);


	/* if this was the roam scan for an association recreation, then
	 * declare link down immediately instead of letting bcn_timeout
	 * happen later.
	 */
	if (ASSOC_RECREATE_ENAB(wlc->pub) && assoc_recreate) {
	    if (status != WLC_E_STATUS_SUCCESS) {
			WL_ASSOC(("wl%d: ROAM: RECREATE failed, link down\n", WLCWLUNIT(wlc)));

			wlc_link(wlc, FALSE, &cfg->prev_BSSID, cfg, WLC_E_LINK_ASSOC_REC);
			roam->bcns_lost = TRUE;
			roam->time_since_bcn = 1;
		}

#if defined(NDIS) && (NDISVER >= 0x0630)
		wlc_bss_mac_event(wlc, cfg, WLC_E_ASSOC_RECREATED, NULL, status, 0, 0, 0, 0);
#endif /* (NDIS) && (NDISVER >= 0x0630) */
	}
}

void
wlc_disassoc_complete(wlc_bsscfg_t *cfg, uint status, struct ether_addr *addr,
	uint disassoc_reason, uint bss_type)
{
	wlc_info_t *wlc = cfg->wlc;

#ifdef WLRM
	if (wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: abort RM due to disassoc\n", WLCWLUNIT(wlc)));
		wlc_rm_stop(wlc);
	}
#endif /* WLRM */

#ifdef WL11K
	if (wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: abort RRM due to disassoc\n", WLCWLUNIT(wlc)));
		wlc_rrm_stop(wlc);
	}
#endif /* WL11K */

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		struct scb *scbd;

		if ((scbd = wlc_scbfind(wlc, cfg, addr)) == NULL)
			WL_ERROR(("%s: null SCB\n", __FUNCTION__));
		else {
			wlfc_MAC_table_update(wlc->wl, &addr->octet[0],
			                      WLFC_CTL_TYPE_MACDESC_DEL,
			                      scbd->mac_address_handle,
			                      ((cfg->wlcif == NULL) ? 0 : cfg->wlcif->index));
			wlfc_release_MAC_descriptor_handle(wlc->wlfc_data,
			                                   scbd->mac_address_handle);
			WLFC_DBGMESG(("STA: MAC-DEL for [%02x:%02x:%02x:%02x:%02x:%02x], "
			              "handle: [%d], if:%d\n",
			              addr->octet[0], addr->octet[1], addr->octet[2],
			              addr->octet[3], addr->octet[4], addr->octet[5],
			              scbd->mac_address_handle,
			              ((cfg->wlcif == NULL) ? 0 : cfg->wlcif->index)));
			scbd->mac_address_handle = 0;
		}
	}
#endif /* PROP_TXSTATUS */

	wlc_bss_mac_event(wlc, cfg, WLC_E_DISASSOC, addr, status,
	                  disassoc_reason, bss_type, 0, 0);
}

void
wlc_roamscan_start(wlc_bsscfg_t *cfg, uint roam_reason)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_roam_t *roam = cfg->roam;
	bool roamscan_full, roamscan_new;
	int err;


	if (roam_reason == WLC_E_REASON_DEAUTH || roam_reason == WLC_E_REASON_DISASSOC) {

		wlc_update_bcn_info(cfg, FALSE);

		/* Don't block this scan */
		roam->scan_block = 0;

		/* bzero(cfg->BSSID.octet, ETHER_ADDR_LEN); */
		wlc_bss_clear_bssid(cfg);
	}

	/* Turning off partials scans will restore original 'dumb' roaming algorithms */
	if (roam->partialscan_period) {
		uint idx, num_invalid_aps = 0;
		/* make sure that we don't have a valid cache full of APs we have just tried to
		 * associate with - this prevents thrashing
		 * If there's only one AP in the cache, we should consider it a valid target even
		 * if we are "blocked" from it
		 */
		if (roam->cache_numentries > 1 &&
		    (roam_reason == WLC_E_REASON_MINTXRATE || roam_reason == WLC_E_REASON_TXFAIL)) {
			for (idx = 0; idx < roam->cache_numentries; idx++)
				if (roam->cached_ap[idx].time_left_to_next_assoc > 0)
					num_invalid_aps++;

			if (roam->cache_numentries > 0 &&
			    num_invalid_aps == roam->cache_numentries) {
				WL_ASSOC(("wl%d: %s: Unable start roamscan because there are no "
				          "valid entries in the roam cache\n", wlc->pub->unit,
				          __FUNCTION__));
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
				wlc_print_roam_status(cfg, roam_reason, TRUE);
#endif
				return;
			}
		}

		/* We are above the threshold and should stop roaming now */
		if (roam_reason == WLC_E_REASON_LOW_RSSI) {
		    if (cfg->link->rssi >=  wlc->band->roam_trigger) {
			/* Stop roam scans */
			if (roam->active) {
				/* Ignore RSSI thrashing about the roam_trigger */
				if ((cfg->link->rssi - wlc->band->roam_trigger) <
				    wlc->roam_rssi_cancel_hysteresis)
					return;

				WL_ASSOC(("wl%d: %s: Finished with roaming\n",
				          wlc->pub->unit, __FUNCTION__));


				if (as->type == AS_ROAM && SCAN_IN_PROGRESS(wlc->scan)) {
					WL_ASSOC(("wl%d: %s: Aborting the roam scan in progress "
					          "since we received a strong signal RSSI: %d\n",
					          wlc->pub->unit, __FUNCTION__, cfg->link->rssi));

					wlc_assoc_abort(cfg);
					/* clear reason */
					roam->reason = WLC_E_REASON_INITIAL_ASSOC;
				}

				roam->cache_valid = FALSE;
				roam->active = FALSE;
				roam->scan_block = 0;
			}

			return;
		}
#if defined(NDIS) && (NDISVER >= 0x0630)
		else {
			if (cfg->nlo &&
			ASSOC_RECREATE_ENAB(wlc->pub) &&
			((as->state == AS_RECREATE_WAIT_RCV_BCN) ||
			(as->state == AS_ASSOC_VERIFY))) {
				/*
				* If NLO is enabled and we're trying to recreate the assoc following
				* a resume to D0, don't start a roam for low RSSI but just complete
				* assoc recreation and NLO jobs.  Roam could be started shortly for
				* low RSSI when receiving next beacon or data pkt. after assoc is
				* recreated.
				*/
				WL_ASSOC(("wl%d: %s: Bypassing roamscan when recreating assoc: "
				"State: %d, RSSI: %d, trigger %d \n",
				wlc->pub->unit, __FUNCTION__,
				as->state,
				cfg->link->rssi,
				wlc->band->roam_trigger));

				return;
			}
		}
#endif /* defined(NDIS) && (NDISVER >= 0x0630) */
	}

		if (roam->reason == WLC_E_REASON_MINTXRATE ||
		    roam->reason == WLC_E_REASON_TXFAIL) {
			if (roam->txpass_cnt > roam->txpass_thresh) {
				WL_ASSOC(("wl%d: %s: Canceling the roam action because we have "
				          "sent %d packets at a good rate\n",
				          wlc->pub->unit, __FUNCTION__, roam->txpass_cnt));

				if (as->type == AS_ROAM && SCAN_IN_PROGRESS(wlc->scan)) {
					WL_ASSOC(("wl%d: %s: Aborting the roam scan in progress\n",
					          wlc->pub->unit, __FUNCTION__));

					wlc_assoc_abort(cfg);
				}
				roam->cache_valid = FALSE;
				roam->active = FALSE;
				roam->scan_block = 0;
				roam->txpass_cnt = 0;
				roam->reason = WLC_E_REASON_INITIAL_ASSOC; /* clear reason */

				return;
			}
		}

		/* Already roaming, come back in another watchdog tick  */
		if (roam->scan_block) {
			return;
		}

		/* Should initiate the roam scan now */
		roamscan_full = FALSE;

		if (!roam->active)
			roamscan_new = TRUE;
		else
			roamscan_new = FALSE;

		{
			if (roam->time_since_upd >= roam->fullscan_period)
				roamscan_full = TRUE;

			/* wlc_roam_scan() uses this info to decide on a full scan or a partial
			 * scan.
			 */
			if (roamscan_full || roamscan_new)
				roam->cache_valid = FALSE;
		}

		if (as->type == AS_ROAM) {
			WL_ASSOC(("wl%d: %s: Not starting roam scan for reason %u because roaming "
			          "already in progress for reason %u\n", wlc->pub->unit,
			          __FUNCTION__, roam_reason, roam->reason));
			return;
		}

		WL_ASSOC(("wl%d: %s: Start roam scan: Doing a %s scan with a scan period of %d "
		          "seconds for reason %u\n", wlc->pub->unit, __FUNCTION__,
		          roamscan_new ? "new" :
		          !(roam->cache_valid && roam->cache_numentries > 0) ? "full" : "partial",
		          roamscan_new || !(roam->cache_valid && roam->cache_numentries > 0) ?
		          roam->fullscan_period : roam->partialscan_period,
		          roam_reason));

		/* Kick off the roam scan */
		if (!(err = wlc_roam_scan(cfg, roam_reason))) {
			roam->active = TRUE;
			roam->scan_block = roam->partialscan_period;
			if (roamscan_full || roamscan_new) {
				if (roamscan_new) {
					/* Do RSSI_ROAMSCAN_FULL_NTIMES number of scans before */
					/* kicking in partial scans */
					roam->fullscan_count = ROAM_FULLSCAN_NTIMES;
				}
				else
					roam->fullscan_count = 1;
				roam->time_since_upd = 0;
			}
		}
		else {
			if (err == BCME_EPERM)
				WL_ASSOC(("wl%d: %s: Couldn't start the roam with error %d\n",
				          wlc->pub->unit, __FUNCTION__, err));
			else
				WL_ERROR(("wl%d: %s: Couldn't start the roam with error %d\n",
				          wlc->pub->unit,  __FUNCTION__, err));
		}
		return;
	}
	/* Original roaming */
	else {
		int roam_metric;
		if (roam_reason == WLC_E_REASON_LOW_RSSI)
			roam_metric = cfg->link->rssi;
		else
			roam_metric = WLC_RSSI_MINVAL;

		if (roam_metric < wlc->band->roam_trigger) {
			if (roam->scan_block || roam->off) {
				WL_ASSOC(("ROAM: roam_metric=%d; block roam scan request(%u,%d)\n",
				          roam_metric, roam->scan_block, roam->off));
			} else {
				WL_ASSOC(("ROAM: RSSI = %d; request roam scan\n",
				          roam_metric));

				roam->scan_block = roam->fullscan_period;
				wlc_roam_scan(cfg, WLC_E_REASON_LOW_RSSI);
			}
		}
	}
}

static void
wlc_roamscan_complete(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;

	WL_TRACE(("wl%d: %s: enter\n", wlc->pub->unit, __FUNCTION__));

	/* Not going to cache channels for any other roam reason but these two */
	if (!((roam->reason == WLC_E_REASON_LOW_RSSI) ||
	      (roam->reason == WLC_E_REASON_BCNS_LOST) ||
	      (roam->reason == WLC_E_REASON_MINTXRATE) ||
	      (roam->reason == WLC_E_REASON_TXFAIL)))
		return;

	/* Still doing full scans, so don't cache channels yet */
	if (--roam->fullscan_count) {
		WL_ASSOC(("wl%d: %s: Not building roam cache because we still have %d full scans "
		          "left\n", wlc->pub->unit, __FUNCTION__, roam->fullscan_count));
		return;
	}

	WL_ASSOC(("wl%d.%d: %s: Building roam candidate cache from driver roam scans\n",
	          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
	wlc_build_roam_cache(cfg, wlc->join_targets);
}

void
wlc_build_roam_cache(wlc_bsscfg_t *cfg, wlc_bss_list_t *candidates)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	uint nentries = 0, join_targets;
	chanspec_t chanspec;

	(void)wlc;

	/*
	 * add the currently associated AP channel to the list
	*/

	WL_TRACE(("wl%d: %s: enter\n", wlc->pub->unit, __FUNCTION__));

	if (cfg->associated && !ETHER_ISNULLADDR(&cfg->BSSID)) {
		WL_ASSOC(("wl%d: %s: Adding current AP to cached candidate list\n", wlc->pub->unit,
		          __FUNCTION__));
		bcopy(&cfg->current_bss->BSSID, &roam->cached_ap[0].BSSID, ETHER_ADDR_LEN);
		roam->cached_ap[0].chanspec = cfg->current_bss->chanspec;
		roam->cached_ap[0].time_left_to_next_assoc = 0;
		nentries = 1;
	}

	/* fill in the cache entries, avoid duplicates */
	for (join_targets = 0; join_targets < candidates->count; join_targets++) {
		uint k;
		struct ether_addr* bssid = &candidates->ptrs[join_targets]->BSSID;
		bool already_in = FALSE;
		chanspec = candidates->ptrs[join_targets]->chanspec;

		for (k = 0; k < nentries; k++) {
			if (chanspec == roam->cached_ap[k].chanspec &&
			    !bcmp(bssid, &roam->cached_ap[k].BSSID, ETHER_ADDR_LEN))
				already_in = TRUE;
		}
		if (!already_in) {
			bcopy(&candidates->ptrs[join_targets]->BSSID,
			      &roam->cached_ap[nentries].BSSID, ETHER_ADDR_LEN);
			roam->cached_ap[nentries].chanspec = chanspec;
			roam->cached_ap[nentries].time_left_to_next_assoc = 0;
			nentries++;
			if (nentries == ROAM_CACHELIST_SIZE)
				goto done;
		}
	}

done:
	/* Full scans are done */
	if (!roam->fullscan_count) {
		WL_ASSOC(("wl%d: %s: Full roam scans completed, starting partial scans with %d "
		          "cached entries \n", wlc->pub->unit, __FUNCTION__,  nentries));

		roam->cache_numentries = nentries;
		roam->cache_valid = TRUE;
		roam->fullscan_count = 1;
		wlc_roam_set_env(cfg, nentries);
	}

	/* print status */
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	wlc_print_roam_status(cfg, roam->reason, TRUE);
#endif
}

/* Set the AP environment */
static void
wlc_roam_set_env(wlc_bsscfg_t *cfg, uint nentries)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	wlcband_t *this_band = wlc->band;
	wlcband_t *other_band = wlc->bandstate[OTHERBANDUNIT(wlc)];

	if (roam->ap_environment != AP_ENV_DETECT_NOT_USED) {
		if (nentries > 1 && roam->ap_environment != AP_ENV_DENSE) {
			roam->ap_environment = AP_ENV_DENSE;
			WL_ASSOC(("wl%d: %s: Auto-detecting dense AP environment with "
			          "%d targets\n", wlc->pub->unit, __FUNCTION__, nentries));
				/* if the roam trigger isn't set to the default, or to the value
				 * of the sparse environment setting, don't change it -- we don't
				 * want to overrride manually set triggers
				 */
			/* this means we are transitioning from a sparse environment */
			if (this_band->roam_trigger == WLC_AUTO_ROAM_TRIGGER -
			    WLC_ROAM_TRIGGER_STEP) {
				this_band->roam_trigger = WLC_AUTO_ROAM_TRIGGER;
				WL_ASSOC(("wl%d: %s: Setting roam trigger on bandunit %u "
				          "to %d\n", wlc->pub->unit, __FUNCTION__,
				          this_band->bandunit, WLC_AUTO_ROAM_TRIGGER));
				}
			else if (this_band->roam_trigger != WLC_AUTO_ROAM_TRIGGER)
				WL_ASSOC(("wl%d: %s: Not modifying manually-set roam "
				          "trigger on bandunit %u from %d to %d\n",
				          wlc->pub->unit, __FUNCTION__, this_band->bandunit,
				          this_band->roam_trigger, WLC_AUTO_ROAM_TRIGGER));

			/* do the same for the other band */
			if ((NBANDS(wlc) > 1 && other_band->roam_trigger == WLC_AUTO_ROAM_TRIGGER -
			     WLC_ROAM_TRIGGER_STEP)) {
				other_band->roam_trigger = WLC_AUTO_ROAM_TRIGGER;
				WL_ASSOC(("wl%d: %s: Setting roam trigger on bandunit %u to %d\n",
				          wlc->pub->unit, __FUNCTION__, other_band->bandunit,
				          WLC_AUTO_ROAM_TRIGGER));
			} else if (NBANDS(wlc) > 1 && other_band->roam_trigger !=
			           WLC_AUTO_ROAM_TRIGGER)
				WL_ASSOC(("wl%d: %s: Not modifying manually-set band roam "
				          "trigger on bandunit %u from %d to %d\n",
				          wlc->pub->unit, __FUNCTION__, other_band->bandunit,
				          other_band->roam_trigger, WLC_AUTO_ROAM_TRIGGER));

			/* this means we are transitioning into a sparse environment
			 * from either an INDETERMINATE or dense one
			 */
		} else if (nentries == 1 && roam->ap_environment != AP_ENV_SPARSE) {
			WL_ASSOC(("wl%d: %s: Auto-detecting sparse AP environment with "
			          "one roam target\n", wlc->pub->unit, __FUNCTION__));
			roam->ap_environment = AP_ENV_SPARSE;

			if (this_band->roam_trigger == WLC_AUTO_ROAM_TRIGGER) {
				this_band->roam_trigger -= WLC_ROAM_TRIGGER_STEP;
				WL_ASSOC(("wl%d: %s: Setting roam trigger on bandunit %u "
				          "to %d\n", wlc->pub->unit, __FUNCTION__,
				          this_band->bandunit, this_band->roam_trigger));
			} else
				WL_ASSOC(("wl%d: %s: Not modifying manually-set roam "
				          "trigger on bandunit %u from %d to %d\n",
				          wlc->pub->unit, __FUNCTION__, this_band->bandunit,
				          this_band->roam_trigger,
				          this_band->roam_trigger - WLC_ROAM_TRIGGER_STEP));

			if (NBANDS(wlc) > 1 && other_band->roam_trigger == WLC_AUTO_ROAM_TRIGGER) {
				other_band->roam_trigger -= WLC_ROAM_TRIGGER_STEP;
				WL_ASSOC(("wl%d: %s: Setting roam trigger on bandunit %u "
				          "to %d\n", wlc->pub->unit, __FUNCTION__,
				          other_band->bandunit, other_band->roam_trigger));
			} else if (NBANDS(wlc) > 1)
				WL_ASSOC(("wl%d: %s: Not modifying manually-set band roam "
				          "trigger on bandunit %u from %d to %d\n",
				          wlc->pub->unit, __FUNCTION__, other_band->bandunit,
				          other_band->roam_trigger,
				          other_band->roam_trigger - WLC_ROAM_TRIGGER_STEP));
		}
	}
}

void
wlc_roam_motion_detect(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;

	/* Check for motion */
	if (!roam->motion &&
	    ABS(cfg->current_bss->RSSI - roam->RSSIref) >= roam->motion_rssi_delta) {
		roam->motion = TRUE;

		/* force a full scan on the next iteration */
		roam->cache_valid = FALSE;
		roam->scan_block = 0;

		wlc->band->roam_trigger += MOTION_DETECT_TRIG_MOD;
		wlc->band->roam_delta -= MOTION_DETECT_DELTA_MOD;

		if (NBANDS(wlc) > 1) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_delta += MOTION_DETECT_TRIG_MOD;
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_trigger -=
			        MOTION_DETECT_DELTA_MOD;
		}

		WL_ASSOC(("wl%d.%d: Motion detected, invalidating roaming cache and "
		          "moving roam_delta to %d and roam_trigger to %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), wlc->band->roam_delta,
		          wlc->band->roam_trigger));
	}

	if (roam->motion && ++roam->motion_dur > roam->motion_timeout) {
		roam->motion = FALSE;
		/* update RSSIref in next watchdog */
		roam->RSSIref = 0;
		roam->motion_dur = 0;

		wlc->band->roam_trigger -= MOTION_DETECT_TRIG_MOD;
		wlc->band->roam_delta += MOTION_DETECT_DELTA_MOD;

		if ((wlc->band->roam_trigger <= wlc->band->roam_trigger_def) ||
			(wlc->band->roam_delta >= wlc->band->roam_delta_def))
			return;

		if (NBANDS(wlc) > 1) {
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_delta -= MOTION_DETECT_TRIG_MOD;
			wlc->bandstate[OTHERBANDUNIT(wlc)]->roam_trigger +=
			        MOTION_DETECT_DELTA_MOD;
		}

		WL_ASSOC(("wl%d.%d: Motion timeout, restoring default values of "
		          "roam_delta to %d and roam_trigger to %d, new RSSI ref is %d\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), wlc->band->roam_delta,
		          wlc->band->roam_trigger, roam->RSSIref));
	}
}

static void
wlc_roam_release_flow_cntrl(wlc_bsscfg_t *cfg)
{
	wlc_txflowcontrol_override(cfg->wlc, cfg->wlcif->qi, OFF, TXQ_STOP_FOR_PKT_DRAIN);
}

static bool
wlc_clear_tkip_cm_bt(uint32 currentTime, uint32 refTime)
{
	uint32 delta = 0;

	if (currentTime == refTime) {
		return FALSE;
	}

	delta = currentTime > refTime ? currentTime - refTime :
		(uint32)~0 - refTime + currentTime + 1;

	if (delta >= (WPA_TKIP_CM_BLOCK * 1000)) {
		return TRUE;
	}

	return FALSE;
}

void
wlc_assoc_roam(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_assoc_t *as = cfg->assoc;
	wlc_join_pref_t *join_pref = cfg->join_pref;

	if (!cfg->roam->assocroam)
		return;

	/* if assocroam is enabled, need to consider roam to band pref band */
	WL_ASSOC(("wlc_assoc_roam, assocroam enabled, band pref = %d\n", join_pref->band));

	if (BSSCFG_STA(cfg) &&
	    (as->type != AS_ROAM && as->type != AS_RECREATE) &&
	    cfg->associated && (join_pref->band != WLC_BAND_AUTO)) {
		uint ssid_cnt = 0, i;
		wlc_bss_info_t **bip, *tmp;
		wlc_bss_info_t *current_bss = cfg->current_bss;

		if (join_pref->band ==
		    CHSPEC2WLC_BAND(current_bss->chanspec))
			return;

		bip = wlc->scan_results->ptrs;

		/* sort current_bss->ssid to the front */
		for (i = 0; i < wlc->scan_results->count; i++) {
			if (WLC_IS_CURRENT_SSID(cfg, (char*)bip[i]->SSID, bip[i]->SSID_len) &&
			    join_pref->band == CHSPEC2WLC_BAND(bip[i]->chanspec)) {
				if (i > ssid_cnt) {	/* swap if not self */
					tmp = bip[ssid_cnt];
					bip[ssid_cnt] = bip[i];
					bip[i] = tmp;
				}
				ssid_cnt++;
			}
		}

		if (ssid_cnt > 0) {
			WL_ASSOC(("assoc_roam: found %d matching ssid with current bss\n",
				ssid_cnt));
			/* prune itself */
			for (i = 0; i < ssid_cnt; i++)
				if (WLC_IS_CURRENT_BSSID(cfg, &bip[i]->BSSID) &&
				    current_bss->chanspec == bip[i]->chanspec) {
					ssid_cnt--;

					tmp = bip[ssid_cnt];
					bip[ssid_cnt] = bip[i];
					bip[i] = tmp;
				}
		}

		if (ssid_cnt > 0) {
			/* hijack this scan to start roaming completion after free memory */
			uint indx;
			wlc_bss_info_t *bi;

			WL_ASSOC(("assoc_roam: consider asoc roam with %d targets\n", ssid_cnt));

			/* free other scan_results since scan_results->count will be reduced */
			for (indx = ssid_cnt; indx < wlc->scan_results->count; indx++) {
				bi = wlc->scan_results->ptrs[indx];
				if (bi) {
					if (bi->bcn_prb)
						MFREE(wlc->osh, bi->bcn_prb, bi->bcn_prb_len);

					MFREE(wlc->osh, bi, sizeof(wlc_bss_info_t));
					wlc->scan_results->ptrs[indx] = NULL;
				}
			}

			wlc->scan_results->count = ssid_cnt;

			wlc_assoc_init(cfg, AS_ROAM);
			wlc_assoc_change_state(cfg, AS_SCAN);
			wlc_assoc_scan_complete(wlc, WLC_E_STATUS_SUCCESS, cfg);
		}
	}
}

void
wlc_roam_bcns_lost(wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;


	if (!roam->off &&
	    ((cfg->assoc->state == AS_IDLE) || AS_CAN_YIELD(cfg->BSS, cfg->assoc->state)) &&
	    !SCAN_IN_PROGRESS(wlc->scan)) {
		WL_ASSOC(("wl%d.%d: ROAM: time_since_bcn %d, request roam scan\n",
		          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), roam->time_since_bcn));

		/* No longer need to track PM states since we may have lost our AP */
		wlc_reset_pmstate(cfg);

		/* Allow consecutive roam scans without delay for the first
		 * ROAM_CONSEC_BCNS_LOST_THRESH beacon lost events after
		 * initially losing beacons
		 */
		if (roam->consec_roam_bcns_lost <= ROAM_CONSEC_BCNS_LOST_THRESH) {
			/* clear scan_block so that the roam scan will happen w/o delay */
			roam->scan_block = 0;
			roam->consec_roam_bcns_lost++;
			WL_ASSOC(("wl%d.%d: ROAM %s #%d w/o delay, setting scan_block to 0\n",
			          WLCWLUNIT(wlc), WLC_BSSCFG_IDX(cfg), __FUNCTION__,
			          roam->consec_roam_bcns_lost));
		}


		wlc_roamscan_start(cfg, WLC_E_REASON_BCNS_LOST);
	}
}

int
wlc_roam_trigger_logical_dbm(wlc_info_t *wlc, wlcband_t *band, int val)
{
	int trigger_dbm = WLC_NEVER_ROAM_TRIGGER;

	if (val == WLC_ROAM_TRIGGER_DEFAULT)
		trigger_dbm = band->roam_trigger_init_def;
	else if (val == WLC_ROAM_TRIGGER_BANDWIDTH)
		trigger_dbm = band->roam_trigger_init_def + WLC_ROAM_TRIGGER_STEP;
	else if (val == WLC_ROAM_TRIGGER_DISTANCE)
		trigger_dbm = band->roam_trigger_init_def - WLC_ROAM_TRIGGER_STEP;
	else if (val == WLC_ROAM_TRIGGER_AUTO)
		trigger_dbm = WLC_AUTO_ROAM_TRIGGER;
	else
		ASSERT(0);

	return trigger_dbm;
}

/* Make decisions about roaming based upon feedback from the tx rate */
void
wlc_txrate_roam(wlc_info_t *wlc, struct scb *scb, tx_status_t *txs, bool pkt_sent,
	bool pkt_max_retries)
{
	wlc_bsscfg_t *cfg;
	wlc_roam_t *roam;

	/* this code doesn't work if we have an override rate */
	if (wlc->band->rspec_override || wlc->band->mrspec_override)
		return;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	roam = cfg->roam;

	/* prevent roaming for tx rate issues too frequently */
	if (roam->ratebased_roam_block > 0)
		return;

	if (pkt_sent && !wlc_scb_ratesel_minrate(wlc->wrsi, scb, txs))
		roam->txpass_cnt++;
	else
		roam->txpass_cnt = 0;

	/* should we roam on too many packets at the min tx rate? */
	if (roam->minrate_txpass_thresh) {
		if (pkt_sent) {
			if (wlc_scb_ratesel_minrate(wlc->wrsi, scb, txs))
				roam->minrate_txpass_cnt++;
			else
				roam->minrate_txpass_cnt = 0;

			if (roam->minrate_txpass_cnt > roam->minrate_txpass_thresh &&
			    !roam->active) {
				WL_ASSOC(("wl%d: %s: Starting roam scan due to %d "
				          "packets at the most basic rate\n",
				          WLCWLUNIT(wlc), __FUNCTION__,
				          roam->minrate_txpass_cnt));
#ifdef WLP2P
				if (!BSS_P2P_ENAB(wlc, cfg))
#endif
					wlc_roamscan_start(cfg, WLC_E_REASON_MINTXRATE);
				roam->minrate_txpass_cnt = 0;
				roam->ratebased_roam_block = ROAM_REASSOC_TIMEOUT;
			}
		}
	}

	/* should we roam on too many tx failures at the min rate? */
	if (roam->minrate_txfail_thresh) {
		if (pkt_sent)
			roam->minrate_txfail_cnt = 0;
		else if (pkt_max_retries && wlc_scb_ratesel_minrate(wlc->wrsi, scb, txs)) {
			if (++roam->minrate_txfail_cnt > roam->minrate_txfail_thresh &&
			    !roam->active) {
				WL_ASSOC(("wl%d: starting roamscan for txfail\n",
				          WLCWLUNIT(wlc)));
#ifdef WLP2P
				if (!BSS_P2P_ENAB(wlc, cfg))
#endif
					wlc_roamscan_start(cfg, WLC_E_REASON_TXFAIL);
				roam->minrate_txfail_cnt = 0; /* throttle roaming */
				roam->ratebased_roam_block = ROAM_REASSOC_TIMEOUT;
			}
		}
	}
}

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
static void
wlc_print_roam_status(wlc_bsscfg_t *cfg, uint roam_reason, bool printcache)
{
	wlc_info_t *wlc = cfg->wlc;
	wlc_roam_t *roam = cfg->roam;
	uint idx;
	const char* event_name;
	char eabuf[ETHER_ADDR_STR_LEN];

	static struct {
		uint event;
		const char *event_name;
	} event_names[] = {
		{WLC_E_REASON_INITIAL_ASSOC, "INITIAL ASSOCIATION"},
		{WLC_E_REASON_LOW_RSSI, "LOW_RSSI"},
		{WLC_E_REASON_DEAUTH, "RECEIVED DEAUTHENTICATION"},
		{WLC_E_REASON_DISASSOC, "RECEIVED DISASSOCATION"},
		{WLC_E_REASON_BCNS_LOST, "BEACONS LOST"},
		{WLC_E_REASON_FAST_ROAM_FAILED, "FAST ROAM FAILED"},
		{WLC_E_REASON_DIRECTED_ROAM, "DIRECTED ROAM"},
		{WLC_E_REASON_TSPEC_REJECTED, "TSPEC REJECTED"},
		{WLC_E_REASON_BETTER_AP, "BETTER AP FOUND"},
		{WLC_E_REASON_MINTXRATE, "STUCK AT MIN TX RATE"},
		{WLC_E_REASON_REQUESTED_ROAM, "REQUESTED ROAM"},
		{WLC_E_REASON_TXFAIL, "TOO MANY TXFAILURES"}
	};

	event_name = "UNKNOWN";
	for (idx = 0; idx < ARRAYSIZE(event_names); idx++) {
		if (event_names[idx].event == roam_reason)
		    event_name = event_names[idx].event_name;
	}

	WL_ASSOC(("wl%d: Current roam reason is %s. The cache has %u entries.\n", wlc->pub->unit,
	          event_name, roam->cache_numentries));
	if (printcache) {
		for (idx = 0; idx < roam->cache_numentries; idx++) {
			WL_ASSOC(("\t Entry %u => chanspec 0x%x (BSSID: %s)\t", idx,
			          roam->cached_ap[idx].chanspec,
			          bcm_ether_ntoa(&roam->cached_ap[idx].BSSID, eabuf)));
			if (roam->cached_ap[idx].time_left_to_next_assoc)
				WL_ASSOC(("association blocked for %d more seconds\n",
				          roam->cached_ap[idx].time_left_to_next_assoc));
			else
				WL_ASSOC(("assocation not blocked\n"));
		}
	}
}
#endif /* defined(BCMDBG) || defined(WLMSG_ASSOC) */
#endif /* STA */

/* Returns BCME_OK if the request can proceed, or a specific BCME_ code if the
 * request is blocked. This routine will make sure any lower priority MAC action
 * is aborted.
 */
int
wlc_mac_request_entry(wlc_info_t *wlc, wlc_bsscfg_t *cfg, int req)
{
	int err = BCME_OK;
#ifdef STA
	wlc_bsscfg_t *bc = wlc->assoc_req[0];
	wlc_assoc_t *as = bc != NULL ? bc->assoc : NULL;
	uint type = AS_IDLE;
#endif

	switch (req) {
#ifdef STA
	case WLC_ACTION_RECREATE_ROAM:
		/* Roam as part of an assoc_recreate process.
		 * Happens when the former AP was not found and we need to look for
		 * another AP supporting the same SSID.
		 */
		ASSERT(cfg != NULL);
		ASSERT(cfg->assoc != NULL);
		ASSERT(cfg->assoc->type == AS_RECREATE);
		ASSERT(cfg->assoc->state == AS_RECREATE_WAIT_RCV_BCN);
		/* FALLSTHRU */
	case WLC_ACTION_ASSOC:
	case WLC_ACTION_RECREATE:
		/* The Association code, wlc_join(), handles canceling scans, associations,
		 * and quiet periods, and takes precedence over all other actions
		 */
#ifdef WLRM
		if (wlc_rminprog(wlc)) {
			WL_INFORM(("wl%d: association request while radio "
				   "measurement is in progress, aborting measurement\n",
				   wlc->pub->unit));
			wlc_rm_stop(wlc);
		}
#endif /* WLRM */

#ifdef WL11K
		if (wlc_rrm_inprog(wlc)) {
			WL_INFORM(("wl%d: association request while radio "
				   "measurement is in progress, aborting RRM\n",
				   wlc->pub->unit));
			wlc_rrm_stop(wlc);
		}
#endif /* WL11K */
		break;

	case WLC_ACTION_ROAM:
		if (as != NULL) {
			WL_INFORM(("wl%d: roam scan blocked for association in progress\n",
				wlc->pub->unit));
			err = BCME_ERROR;
		}
		else if (SCAN_IN_PROGRESS(wlc->scan) || ACT_FRAME_IN_PROGRESS(wlc->scan)) {
			WL_INFORM(("wl%d: roam scan blocked for scan in progress\n",
				wlc->pub->unit));
			err = BCME_ERROR;
		}
		else if (cfg->pm->PSpoll) {
			WL_INFORM(("wl%d: roam scan blocked for outstanding PS-Poll\n",
				wlc->pub->unit));
			err = BCME_ERROR;
		}
		else if (BSS_QUIET_STATE(wlc->quiet, cfg) & SILENCE) {
			WL_INFORM(("wl%d: roam scan blocked for 802.11h Quiet Period\n",
				wlc->pub->unit));
			err = BCME_ERROR;
		}
#ifdef WLRM
		else if (wlc_rminprog(wlc)) {
			WL_INFORM(("wl%d: roam scan while radio measurement is in progress,"
				" aborting measurement\n",
				wlc->pub->unit));
			wlc_rm_stop(wlc);
		}
#endif /* WLRM */

#ifdef WL11K
		else if (wlc_rrm_inprog(wlc)) {
			WL_INFORM(("wl%d: roam scan while radio measurement is in progress,"
				" aborting RRM\n",
				wlc->pub->unit));
			wlc_rrm_stop(wlc);
		}
#endif /* WL11K */
		break;
#endif /* STA */

#ifdef STA
#ifdef WIFI_ACT_FRAME
	case WLC_ACTION_ACTFRAME:
		/*
			This part of checking is the similar for the scan. Duplicate here
			for easier scale up later on.
		*/
		if (cfg != NULL && (BSS_QUIET_STATE(wlc->quiet, cfg) & SILENCE)) {
			WL_INFORM(("wl%d: af request blocked for 802.11h Quiet Period\n",
				wlc->pub->unit));
			err = BCME_EPERM;
		}
		else if (as != NULL && (as->type == AS_ASSOCIATION || as->type == AS_RECREATE)) {
			WL_ERROR(("wl%d: af request blocked for association in progress\n",
				wlc->pub->unit));
			err = BCME_BUSY;
		}
		else if (as != NULL && as->type == AS_ROAM) {
			{
				WL_INFORM(("wl%d: af request while roam is in progress, aborting"
					" roam\n", wlc->pub->unit));
				wlc_assoc_abort(bc);
			}
		}
#ifdef WLRM
		else if (wlc_rminprog(wlc)) {
			WL_INFORM(("wl%d: scan request while radio measurement is in progress,"
				" aborting measurement\n",
				wlc->pub->unit));
			wlc_rm_stop(wlc);
		}
#endif /* WLRM */

#ifdef WL11K
		else if (wlc_rrm_inprog(wlc)) {
			WL_INFORM(("wl%d: scan request while radio measurement is in progress,"
				" aborting RRM\n",
				wlc->pub->unit));
			wlc_rrm_stop(wlc);
		}
#endif /* WL11K */
		else if (SCAN_IN_PROGRESS(wlc->scan)) {
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
		}
		else if (ACT_FRAME_IN_PROGRESS(wlc->scan)) {
			WL_ERROR(("wl%d: af request blocked for af in progress\n",
				wlc->pub->unit));
			err = BCME_BUSY;
		}
		break;
#endif /* WIFI_ACT_FRAME */
#endif /* STA */

	case WLC_ACTION_SCAN:
	case WLC_ACTION_ISCAN:
	case WLC_ACTION_ESCAN:
#ifdef STA
	case WLC_ACTION_PNOSCAN:
	case WLC_ACTION_REASSOC:
		if (cfg != NULL && (BSS_QUIET_STATE(wlc->quiet, cfg) & SILENCE)) {
			WL_INFORM(("wl%d: scan request blocked for 802.11h Quiet Period\n",
				wlc->pub->unit));
			err = BCME_EPERM;
		}
		else if (as != NULL && (as->type == AS_ASSOCIATION || as->type == AS_RECREATE)) {
			WL_ERROR(("wl%d: scan request blocked for association in progress\n",
				wlc->pub->unit));
			err = BCME_BUSY;
		}
		else if (as != NULL && as->type == AS_ROAM) {
			{
				WL_INFORM(("wl%d: scan request while roam is in progress, aborting"
					" roam\n", wlc->pub->unit));
				wlc_assoc_abort(bc);
			}
		}
#ifdef WLRM
		else if (wlc_rminprog(wlc)) {
			WL_INFORM(("wl%d: scan request while radio measurement is in progress,"
				" aborting measurement\n",
				wlc->pub->unit));
			wlc_rm_stop(wlc);
		}
#endif /* WLRM */

#ifdef WL11K
		else if (wlc_rrm_inprog(wlc)) {
			WL_INFORM(("wl%d: scan request while radio measurement is in progress,"
				" aborting RRM\n",
				wlc->pub->unit));
			wlc_rrm_stop(wlc);
		}
#endif /* WL11K */
		else if (ACT_FRAME_IN_PROGRESS(wlc->scan)) {
			WL_ERROR(("wl%d: scan request blocked for af in progress\n",
				wlc->pub->unit));
			err = BCME_BUSY;
		}
		else if ((req == WLC_ACTION_ISCAN) || (req == WLC_ACTION_ESCAN)) {
			if (SCAN_IN_PROGRESS(wlc->scan)) {
				if (ISCAN_IN_PROGRESS(wlc) || ESCAN_IN_PROGRESS(wlc->scan)) {
					/* iscans preempt in-progress iscans */
					WL_INFORM(("e/iscan aborting e/iscan\n"));
					wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
#ifdef WLPFN
				} else if (WLPFN_ENAB(wlc->pub) &&
					wl_pfn_scan_in_progress(wlc->pfn)) {
					/* iscan/escan also preempts PNO scan */
					WL_INFORM(("e/iscan aborting pfn scan\n"));
					wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
#endif
				} else {
					/* other scans have precedence over e/iscans */
					WL_ERROR(("e/iscan blocked due to another kind of scan\n"));
					err = BCME_NOTREADY;
				}
			}
		} else
#endif /* STA */
			if (SCAN_IN_PROGRESS(wlc->scan)) {
#ifdef STA
				if (req == WLC_ACTION_SCAN &&
					(ISCAN_IN_PROGRESS(wlc) || ESCAN_IN_PROGRESS(wlc->scan))) {
					WL_INFORM(("scan aborting e/iscan\n"));
					wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
				}
#ifdef WLPFN
				else if (WLPFN_ENAB(wlc->pub) && wl_pfn_scan_in_progress(wlc->pfn))
					wlc_scan_abort(wlc->scan, WLC_E_STATUS_NEWSCAN);
#endif /* WLPFN */
				else
#endif /* STA */
				{
					WL_ERROR(("wl%d:scan req blocked for scan in progress\n",
						wlc->pub->unit));
					err = BCME_NOTREADY;
				}
			}
		break;

#if defined(STA) && defined(WL11H)
	case WLC_ACTION_QUIET:
		if (as != NULL && as->type == AS_ASSOCIATION) {
			WL_ERROR(("wl%d: should not be attempting to enter Quiet Period "
			          "while an association is in progress, blocking Quiet\n",
			          wlc->pub->unit));
			err = BCME_ERROR;
			ASSERT(0);
		}
		else if (as != NULL && as->type == AS_ROAM) {
			WL_INFORM(("wl%d: Quiet Period starting while roam is in progress, aborting"
				" roam\n",
				wlc->pub->unit));
			wlc_assoc_abort(bc);
		}
		else if (SCAN_IN_PROGRESS(wlc->scan)) {
			WL_INFORM(("wl%d: Quiet Period starting while scan is in progress, aborting"
				" scan\n",
				wlc->pub->unit));
			wlc_scan_abort(wlc->scan, WLC_E_STATUS_11HQUIET);
		}
		break;
#endif /* STA && WL11H */

	case WLC_ACTION_RM:
		if (SCAN_IN_PROGRESS(wlc->scan)) {
			WL_INFORM(("wl%d: radio measure blocked for scan in progress\n",
				wlc->pub->unit));
			err = BCME_ERROR;
		}
#ifdef STA
		else if (as != NULL && as->type == AS_ASSOCIATION) {
			WL_INFORM(("wl%d: radio measure blocked for association in progress\n",
				wlc->pub->unit));
			err = BCME_ERROR;
		}
		else if (as != NULL && as->type == AS_ROAM) {
			WL_INFORM(("wl%d: radio measure blocked for roam in progress\n",
				wlc->pub->unit));
			err = BCME_ERROR;
		}
#endif /* STA */
		break;

	default:
		err = BCME_ERROR;
		ASSERT(0);
	}

#ifdef STA
	if (err != BCME_OK)
		return err;

	/* we are granted MAC access! */
	switch (req) {
	case WLC_ACTION_ASSOC:
		type = AS_ASSOCIATION;
		goto request;
	case WLC_ACTION_RECREATE:
		type = AS_RECREATE;
		goto request;
	case WLC_ACTION_ROAM:
	case WLC_ACTION_RECREATE_ROAM:
	case WLC_ACTION_REASSOC:
		type = AS_ROAM;
		/* FALLSTHRU */
	request: {
		int i;

		ASSERT(cfg != NULL);
		/* add the request into assoc req list */
		if ((i = wlc_assoc_req_add_entry(wlc, cfg, type, FALSE)) < 0) {
#if defined(BCMDBG) || defined(WLMSG_INFORM)
			char ssidbuf[SSID_FMT_BUF_LEN];
			wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
			WL_INFORM(("wl%d.%d: %s request not granted for SSID %s\n",
			           wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
			           WLCASTYPEN(cfg->assoc->type), ssidbuf));
#endif
			err = BCME_ERROR;
		}
		else if (i > 0) {
			err = BCME_ERROR;
		}
	}
	}
#endif /* STA */

	return err;
}

/* get roam default parameters */
void
BCMATTACHFN(wlc_roam_defaults)(wlc_info_t *wlc, wlcband_t *band, int *roam_trigger,
	uint *roam_delta)
{
	/* set default roam parameters */
	switch (band->radioid) {
	case BCM2050_ID:
		if (band->radiorev == 1) {
			*roam_trigger = WLC_2053_ROAM_TRIGGER;
			*roam_delta = WLC_2053_ROAM_DELTA;
		} else {
			*roam_trigger = WLC_2050_ROAM_TRIGGER;
			*roam_delta = WLC_2050_ROAM_DELTA;
		}
		if (wlc->pub->boardflags & BFL_EXTLNA) {
			*roam_trigger -= 2;
		}
		break;
	case BCM2055_ID:
	case BCM2056_ID:
		*roam_trigger = WLC_2055_ROAM_TRIGGER;
		*roam_delta = WLC_2055_ROAM_DELTA;
		break;
	case BCM2060_ID:
		*roam_trigger = WLC_2060WW_ROAM_TRIGGER;
		*roam_delta = WLC_2060WW_ROAM_DELTA;
		break;

	case NORADIO_ID:
		*roam_trigger = WLC_NEVER_ROAM_TRIGGER;
		*roam_delta = WLC_NEVER_ROAM_DELTA;
		break;

	default:
		*roam_trigger = BAND_5G(band->bandtype) ? WLC_5G_ROAM_TRIGGER :
		        WLC_2G_ROAM_TRIGGER;
		*roam_delta = BAND_5G(band->bandtype) ? WLC_5G_ROAM_DELTA :
		        WLC_2G_ROAM_DELTA;
		WL_INFORM(("wl%d: wlc_roam_defaults: USE GENERIC ROAM THRESHOLD "
		           "(%d %d) FOR RADIO %04x IN BAND %s\n", wlc->pub->unit,
		           *roam_trigger, *roam_delta, band->radioid,
		           BAND_5G(band->bandtype) ? "5G" : "2G"));
		break;
	}
}

void
wlc_deauth_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	const struct ether_addr *addr, uint deauth_reason, uint bss_type)
{
#if defined(STA) && defined(WLRM)
	if ((bsscfg && BSSCFG_STA(bsscfg)) && wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: abort RM due to deauth\n", WLCWLUNIT(wlc)));
		wlc_rm_stop(wlc);
	}
#endif /* STA && WLRM */

#if defined(STA) && defined(WL11K)
	if ((bsscfg && BSSCFG_STA(bsscfg)) && wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: abort RRM due to deauth\n", WLCWLUNIT(wlc)));
		wlc_rrm_stop(wlc);
	}
#endif /* STA && WL11K */
#ifdef PSTA
	/* If the deauthenticated client is downstream, then deauthenticate
	 * the corresponding proxy client from our AP.
	 */
	if (PSTA_ENAB(wlc->pub))
		wlc_psta_deauth_client(wlc->psta, (struct ether_addr *)addr);
#endif /* PSTA */
	wlc_bss_mac_event(wlc, bsscfg, WLC_E_DEAUTH, addr, status, deauth_reason,
		bss_type, 0, 0);
}

void
wlc_deauth_sendcomplete(wlc_info_t *wlc, uint txstatus, void *arg)
{
	struct scb *scb;
	wlc_bsscfg_t *bsscfg;
	wlc_deauth_send_cbargs_t *cbarg = arg;

	/* Is this scb still around */
	bsscfg = WLC_BSSCFG(wlc, cbarg->_idx);
	if (bsscfg == NULL)
		return;

	if ((scb = wlc_scbfind(wlc, bsscfg, &cbarg->ea)) == NULL) {
		MFREE(wlc->osh, arg, sizeof(wlc_deauth_send_cbargs_t));
		return;
	}

#ifdef AP
	/* Reset PS state if needed */
	if (SCB_PS(scb))
		wlc_apps_scb_ps_off(wlc, scb, TRUE);
#endif /* AP */

	WL_ASSOC(("wl%d: %s: deauth complete\n", wlc->pub->unit, __FUNCTION__));
	wlc_mac_event(wlc, WLC_E_DEAUTH, (struct ether_addr *)arg,
	              WLC_E_STATUS_SUCCESS, DOT11_RC_DEAUTH_LEAVING, 0, 0, 0);
	MFREE(wlc->osh, arg, sizeof(wlc_deauth_send_cbargs_t));
	/* Do this last: ea_arg points inside scb */
	wlc_scbfree(wlc, scb);
}

void
wlc_disassoc_ind_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	struct ether_addr *addr, uint disassoc_reason, uint bss_type,
	uint8 *body, int body_len)
{

#if defined(STA) && defined(WLRM)
	if (wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: abort RM due to receiving disassoc request\n",
		           WLCWLUNIT(wlc)));
		wlc_rm_stop(wlc);
	}
#endif /* STA && WLRM */

#if defined(STA) && defined(WL11K)
	if (wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: abort RRM due to receiving disassoc request\n",
		           WLCWLUNIT(wlc)));
		wlc_rrm_stop(wlc);
	}
#endif /* STA && WL11K */

#ifdef AP
	if (BSSCFG_AP(bsscfg)) {
#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(wlc->pub)) {
			struct scb *scbd;

			if ((scbd = wlc_scbfind(wlc, bsscfg, addr)) == NULL)
				WL_ERROR(("%s: null SCB\n", __FUNCTION__));
			else {
				wlfc_MAC_table_update(wlc->wl, &addr->octet[0],
				                      WLFC_CTL_TYPE_MACDESC_DEL,
				                      scbd->mac_address_handle,
				                      ((bsscfg->wlcif == NULL) ? 0 :
				                       bsscfg->wlcif->index));
				wlfc_release_MAC_descriptor_handle(wlc->wlfc_data,
				                                   scbd->mac_address_handle);
				WLFC_DBGMESG(("AP: MAC-DEL for [%02x:%02x:%02x:%02x:%02x:%02x], "
				              "handle: [%d], if:%d\n",
				              addr->octet[0], addr->octet[1], addr->octet[2],
				              addr->octet[3], addr->octet[4], addr->octet[5],
				              scbd->mac_address_handle,
				              ((bsscfg->wlcif == NULL) ? 0 :
				               bsscfg->wlcif->index)));
				scbd->mac_address_handle = 0;
			}
		}
#endif /* PROP_TXSTATUS */

		wlc_enable_btc_ps_protection(wlc, bsscfg, FALSE);

#ifdef WLP2P
		/* reenable P2P in case a non-P2P STA leaves the BSS and
		 * all other associated STAs are P2P client
		 */
		if (BSS_P2P_ENAB(wlc, bsscfg))
			wlc_p2p_enab_upd(wlc->p2p, bsscfg);
#endif

		if (DYNBCN_ENAB(bsscfg) && wlc_bss_assocscb_getcnt(wlc, bsscfg) == 0)
			wlc_bsscfg_bcn_disable(wlc, bsscfg);
	}
#endif /* AP */

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_DISASSOC_IND, addr, status, disassoc_reason,
	                  bss_type, body, body_len);

#ifdef BCMWAPI_WAI
	wlc_wapi_station_event(wlc->wapi, bsscfg, addr, NULL, NULL, WAPI_STA_AGING);
#endif
}

void
wlc_deauth_ind_complete(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint status,
	struct ether_addr *addr, uint deauth_reason, uint bss_type,
	uint8 *body, int body_len)
{

#if defined(STA) && defined(WLRM)
	if (wlc_rminprog(wlc)) {
		WL_INFORM(("wl%d: abort RM due to receiving deauth request\n",
		           WLCWLUNIT(wlc)));
		wlc_rm_stop(wlc);
	}
#endif /* STA && WLRM */

#if defined(STA) && defined(WL11K)
	if (wlc_rrm_inprog(wlc)) {
		WL_INFORM(("wl%d: abort RRM due to receiving deauth request\n",
		           WLCWLUNIT(wlc)));
		wlc_rrm_stop(wlc);
	}
#endif /* STA && WL11K */

#ifdef AP
	if (BSSCFG_AP(bsscfg)) {
		wlc_enable_btc_ps_protection(wlc, bsscfg, FALSE);
#ifdef WLP2P
		/* reenable P2P in case a non-P2P STA leaves the BSS and
		 * all other associated STAs are P2P client
		 */
		if (BSS_P2P_ENAB(wlc, bsscfg))
			wlc_p2p_enab_upd(wlc->p2p, bsscfg);
#endif
	}
#endif /* AP */

	wlc_bss_mac_event(wlc, bsscfg, WLC_E_DEAUTH_IND, addr, status, deauth_reason,
	                  bss_type, body, body_len);

#ifdef BCMWAPI_WAI
	wlc_wapi_station_event(wlc->wapi, bsscfg, addr, NULL, NULL, WAPI_STA_AGING);
#endif
}

#ifdef WLFBT
/*
 * Looks for 802.11r RIC response TLVs.
 */
static bool
wlc_parse_ric_resp(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len)
{
	bool accepted = TRUE;
	bcm_tlv_t *ric_ie;
	ric_ie = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_RDE_ID);
	while (ric_ie && accepted) {
		dot11_rde_ie_t *rdeptr = (dot11_rde_ie_t*)ric_ie;
		accepted = (rdeptr->status == 0);
		if (accepted) {
			/* May contain more than one RDE, so look for more. */
			tlvs += DOT11_MNG_RDE_IE_LEN;
			tlvs_len -= DOT11_MNG_RDE_IE_LEN;
			ric_ie = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_RDE_ID);
		}
		/* TODO Handle non-zero status and non-zero rd_count. */
		/* TODO Handle when RDE is missing altogether. */
	}
	return accepted;
}
#endif /* WLFBT */
