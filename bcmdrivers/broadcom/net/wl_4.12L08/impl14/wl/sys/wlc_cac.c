/*
 * 802.11e CAC protocol implementation
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cac.c 365823 2012-10-31 04:24:30Z $
 */

#include <wlc_cfg.h>

#ifndef WLCAC
#error "WLCAC is not defined"
#endif	/* WLCAC */

/* CAC only works when WME is defined */
#ifndef WME
#error "WME is not defined"
#endif	/* WME */


#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.1d.h>
#include <proto/802.11.h>
#include <proto/802.11e.h>
#include <proto/vlan.h>
#include <wlioctl.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_frmutil.h>
#include <wlc_phy_hal.h>
#include <wl_export.h>
#include <wlc_rm.h>
#include <wlc_cac.h>

#ifdef WLC_HIGH_ONLY
#include <wlc_rpctx.h>
#endif


#define DOT11E_TSPEC_IE	(WME_OUI"\x02\x02\x01")	/* oui, type, subtype, ver */
#define DOT11E_TSPEC_OUI_TYPE_LEN (DOT11_OUI_LEN + 3) /* include oui, type, subtype, ver */

#define USEC_PER_MSEC		1000L	/* convert usec time to ms */

#define WLC_CAC_TSPECARG_COPYSIZE	44	/* TSPEC from nom_msdu_size to delay_bound */

/* Minimum phy rate for b/ag mode */
#define CAC_B_MIN_PHYRATE_BPS	1000000	/* b mode minimum phy rate (bps) */
#define CAC_AG_MIN_PHYRATE_BPS	6000000	/* ag mode minimum phy rate (bps) */

/* lower and upper limit for bandwidth managed by admission control */
#define	AC_BW_DEFAULT 70		/* in terms of percentage */


/* ADDTS Timeout fudge factor when STA is in power save mode */
#define	CAC_ADDTS_RESP_TIMEOUT_FF	700

/* Suggested surplus bandwidth
 * surplus BW, 3 bits for integer & 13 bits for decimal (surplus BW
 * includes collision, retries, rate algorithm, etc)
 */
#define CAC_SURPLUS_BW_11	0x2200	/* surplus bandwidth fudge factor 1.1 */
#define CAC_SURPLUS_BW_13	0x2600	/* surplus bandwidth fudge factor 1.3 */
#define CAC_SURPLUS_BW_14	0x2800	/* surplus bandwidth fudge factor 1.4 */

#define	WLC_CAC_GET_INACTIVITY_INTERVAL(ts) \
	((ts)->inactivity_interval/1000000)

/* structure used to housekeeping TSPEC */
typedef struct tsentry {
	struct tsentry *next;	/* pointer to next structure */
	int ts_state;		/* state of TSPCE accept, reject, pending, etc. */
	tspec_t tspec;		/* tspec structure */
	uchar dialog_token;	/* dialog token */
} tsentry_t;

/* possible states for ts_state variable */
#define CAC_STATE_TSPEC_PENDING		0	/* TSPEC in pending state */
#define CAC_STATE_TSPEC_ACCEPTED	1	/* TSPEC in accepted state */
#define CAC_STATE_TSPEC_REJECTED	2	/* TSPEC got rejected */
#define CAC_STATE_TSPEC_WAIT_RESP	7	/* TSPEC in wait for ADDTS resp. state */
#define	CAC_STATE_TSPEC_UPDATE_PENDING	8	/* TSPEC awaiting update state */
#define	CAC_STATE_TSPEC_UPDATE_WAIT_RESP	9	/* TSPEC Update awaiting resp */
#define CAC_STATE_TSPEC_ANY		-1	/* TSPEC any state */

#define	CAC_MAX_TSPEC_PER_AC	1

/* 1 second interval represented in 32us units */
#define	AVAILABLE_MEDIUM_TIME_32US	(1000000 >> 5)

/* in 32us units */
#define MEDIUM_TIME_BE_32US \
	((AVAILABLE_MEDIUM_TIME_32US * (100 - AC_BW_DEFAULT))/100)

#define TSPEC_NOM_MSDU_MASK		0x7fff

#define USEC32_TO_USEC(x)	((x) << 5)
#define USEC_TO_USEC32(x)	((x) >> 5)

/* internal parameters for each AC */
typedef struct wlc_cac_ac {
	uint tot_medium_time;		/* total medium time AP granted pre AC (us) */
	uint used_time;			/* amount medium time STA used (us) */
	uint cached_dur;		/* cached duration */
	uint8 nom_phy_rate;		/* negotiated nominal phy rate */
	bool admitted;			/* Admission state */
	uint inactivity_interval;	/* Denotes inactivity interval in seconds */
	uint inactivity_limit;		/* Inactivity limit as given by the TSPEC */
	uint8 inactivity_tid;			/* Inactive TID */
} wlc_cac_ac_t;

/* CAC main structure */
struct wlc_cac {
	wlc_info_t *wlc;					/* access to wlc_info_t structure */
	wlc_cac_ac_t ac_settings[AC_COUNT];	/* Admission control info each ac */
	bool waiting_resp;					/* tspec waiting for addts resp */
	uint ts_delay;						/* minimum inter- ADDTS time (TU) */
	uint addts_timeout;				/* timeout before sending DELTS (ms) */
	struct wl_timer *addts_timer;	/* timer for addts response timeout */
	struct ether_addr curr_bssid;	/* current bssid */
	wlc_bsscfg_t	*curr_bsscfg;	/* current bsscfg */
	int	scb_handle;					/* scb cubby handle */
	uint32	flags;					/* bit map for indicating various states */
	uint16	available_medium_time;	/* Total available medium time AP specific 32us units */
	uint16	admctl_bw_percent;		/* Percentage of bw that is admission controlled */
	uint	cached_medium_time;		/* previously alloted medium time before update */
	tsentry_t *tsentryq;			/* pointer to tspec list, present only for STA */
	int	rfaware_lifetime;		/* RF awareness lifetime when no TSPEC */
};

/* iovar table */
enum {
	IOV_CAC,
	IOV_CAC_ADDTS_TIMEOUT,
	IOV_CAC_ADDTS,
	IOV_CAC_DELTS,
	IOV_CAC_TSLIST,
	IOV_CAC_TSPEC,
	IOV_CAC_TSLIST_EA,
	IOV_CAC_TSPEC_EA,
	IOV_CAC_DELTS_EA,
	IOV_CAC_AC_BW
};

/* Admission control information */
struct cac_scb_acinfo {
	wlc_cac_ac_t cac_ac[AC_COUNT];	/* Admission control info each ac */
#ifdef	AP
	tsentry_t *tsentryq;			/* pointer to tspec list */
#endif
};

typedef struct cac_scb_acinfo cac_scb_acinfo_t;

#define SCB_ACINFO(acinfo, scb) ((struct cac_scb_acinfo *)SCB_CUBBY(scb, (acinfo)->scb_handle))

static const bcm_iovar_t cac_iovars[] = {
	{"cac", IOV_CAC, (IOVF_SET_DOWN), IOVT_BOOL, 0},
	{"cac_addts", IOV_CAC_ADDTS, 0, IOVT_BUFFER, sizeof(tspec_arg_t)},
	{"cac_delts", IOV_CAC_DELTS, 0, IOVT_BUFFER, sizeof(tspec_arg_t)},
	{"cac_tslist", IOV_CAC_TSLIST, 0, IOVT_BUFFER, 0},
	{"cac_tspec", IOV_CAC_TSPEC, 0, IOVT_BUFFER, sizeof(tspec_arg_t)},
	{"cac_tslist_ea", IOV_CAC_TSLIST_EA, 0, IOVT_BUFFER, 0},
	{"cac_tspec_ea", IOV_CAC_TSPEC_EA, 0, IOVT_BUFFER, sizeof(tspec_per_sta_arg_t)},
	{"cac_delts_ea", IOV_CAC_DELTS_EA, 0, IOVT_BUFFER, sizeof(tspec_per_sta_arg_t)},
	{"cac_ac_bw", IOV_CAC_AC_BW, (IOVF_SET_DOWN), IOVT_INT8, 0},
	{NULL, 0, 0, 0, 0}
};

static int wlc_cac_iovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static int wlc_cac_down(void *hdl);
static tsentry_t *wlc_cac_tsentry_find(tsentry_t *ts, uint8 tid);
static void wlc_cac_tsentry_append(wlc_cac_t *cac, tsentry_t *ts, tsentry_t **ptsentryq);
static int wlc_cac_tsentry_removefree(wlc_cac_t *cac, uint8 tid, uint event_status, uint code,
	tsentry_t **ptsentryq, bool call_event);
static void wlc_cac_medium_time_recal(wlc_cac_t *cac, struct cac_scb_acinfo *scb_acinfo);
static int wlc_cac_tspec_req_send(wlc_cac_t *cac, tspec_t *ts, uint8 action,
	uint8 dialog_token, int reason_code, void **pkt, struct scb *scb);
static int wlc_cac_addts_req_send(wlc_cac_t *cac, tspec_t *ts, uint8 dialog_token);
static int wlc_cac_tspec_send(wlc_cac_t *cac, tsentry_t *tsentryq);
static uint8 *wlc_cac_ie_find(uint8 *tlvs, uint *tlvs_len, uint8 id, uint max_len,
const	int8 *str, uint str_len);
static int wlc_cac_sta_addts(wlc_cac_t *cac, tspec_arg_t *tspec_arg);
static int wlc_cac_sta_delts(wlc_cac_t *cac, tspec_arg_t *tspec_arg, struct scb *scb,
	tsentry_t *tsentryq);
static uint8 *wlc_cac_tspec_rx_process(wlc_cac_t *cac, uint8 *body, uint *body_len,
	uint8 *tid, uint8 *user_prio, struct cac_scb_acinfo *scb_acinfo, int *err);
static int wlc_cac_ie_process(wlc_cac_t *cac, uint8 *tlvs, uint tlvs_len,
	uint8 *tid, uint8 *user_prio, struct scb *scb);
static void wlc_cac_addts_resp(wlc_cac_t *cac, uint8 *body, uint body_len,
	struct scb *scb);
static void wlc_cac_delts_req(wlc_cac_t *cac, uint8 *body, uint body_len,
	struct scb *scb);
static void wlc_cac_addts_timeout(void *arg);
static void wlc_cac_addts_ind_complete(wlc_info_t *wlc, uint status, struct ether_addr *addr,
        uint wme_status, uint tid, uint ts_delay);
static void wlc_cac_delts_ind_complete(wlc_info_t *wlc, uint status, struct ether_addr *addr,
        uint wme_status, uint tid);
static void wlc_cac_tspec_state_change(wlc_cac_t *cac, int old_state,
	uint new_state, tsentry_t *tsentryq);
static void wlc_cac_ac_param_reset(wlc_cac_t *cac, uint8 ac,
	struct cac_scb_acinfo *scb_acinfo);
static int wlc_cac_scb_acinfo_init(void *context, struct scb *scb);
static void wlc_cac_scb_acinfo_deinit(void *context, struct scb *scb);
static void wlc_cac_scb_acinfo_dump(void *context, struct scb *scb, struct bcmstrbuf *b);
#ifdef	AP
static bool wlc_cac_is_tsparam_valid(wlc_cac_t *cac, tspec_t *ts, int ac, struct scb *scb);
static int wlc_cac_addts_resp_send(wlc_cac_t *cac, tspec_t *ts,
	struct scb *scb, uint32 status, uint8 dialog_token);
static int wlc_cac_check_tspec_for_admission(wlc_cac_t *cac,
	tspec_t *ts, uint16 tspec_medium_time, struct scb *scb);
static int wlc_cac_tspec_resp_send(wlc_cac_t *cac, tspec_t *ts, uint8 action,
	int reason_code, uint8 dialog_token, void **pkt, struct scb *scb);
static int wlc_cac_process_addts_req(wlc_cac_t *cac, uint8 *body,
	uint body_len, struct scb *scb);
static uint16 wlc_cac_generate_medium_time(wlc_cac_t *cac, tspec_t *ts, struct scb *scb);
#endif /* AP */

#ifdef BCMDBG
void wlc_print_tspec(tspec_t *ts);
#endif /* BCMDBG */

wlc_cac_t *
BCMATTACHFN(wlc_cac_attach)(wlc_info_t *wlc)
{
	wlc_cac_t *cac;
	int i;

	if (!(cac = (wlc_cac_t *)MALLOC(wlc->osh, sizeof(wlc_cac_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)cac, sizeof(wlc_cac_t));
	cac->wlc = wlc;

	WL_CAC(("wl%d: wlc_cac_attach: registering CAC module\n", wlc->pub->unit));
	if (wlc_module_register(wlc->pub, cac_iovars, "cac", cac, wlc_cac_iovar,
	                        wlc_cac_watchdog, NULL, wlc_cac_down)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "cac", (dump_fn_t)wlc_dump_cac, (void *)cac);
#endif
	/* initialize nominal rate based on band type and mode */
	for (i = 0; i < AC_COUNT; i++) {
		if ((wlc->band->bandtype == WLC_BAND_2G) &&
			(wlc->band->gmode == GMODE_LEGACY_B))
			cac->ac_settings[i].nom_phy_rate = WLC_RATE_11M;
		else
			cac->ac_settings[i].nom_phy_rate = WLC_RATE_24M;
	}

	if (!(cac->addts_timer = wl_init_timer(wlc->wl, wlc_cac_addts_timeout, wlc, "addts"))) {
		WL_ERROR(("wl%d: wlc_cac_attach: wl_init_timer for addts-timer failed\n",
			wlc->pub->unit));
		MFREE(wlc->osh, cac, sizeof(wlc_cac_t));
		return NULL;
	}

	cac->addts_timeout = CAC_ADDTS_RESP_TIMEOUT;
	cac->tsentryq = NULL;

	cac->available_medium_time = AVAILABLE_MEDIUM_TIME_32US - MEDIUM_TIME_BE_32US;
	cac->admctl_bw_percent = AC_BW_DEFAULT ;	/* 70% by default */

	/* reserve the cubby in the scb container for per-scb private data */
	cac->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(struct cac_scb_acinfo),
		wlc_cac_scb_acinfo_init, wlc_cac_scb_acinfo_deinit,
		(scb_cubby_dump_t) wlc_cac_scb_acinfo_dump, (void *)wlc);

	if (cac->scb_handle < 0) {
		WL_ERROR(("wl%d: %s: wlc_scb_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		wlc_cac_detach(cac);
		return NULL;
	}

	return cac;
}

void
BCMATTACHFN(wlc_cac_detach)(wlc_cac_t *cac)
{
	tsentry_t *ts;
#ifdef	AP
	struct scb *scb = NULL;
	struct scb_iter scbiter;
	struct cac_scb_acinfo *scb_acinfo = NULL;
#endif

	if (!cac)
		return;

	if (cac->addts_timer) {
		wl_free_timer(cac->wlc->wl, cac->addts_timer);
		cac->addts_timer = NULL;
	}

#ifdef	AP
	/* free all TSPEC for AP */
	if (AP_ENAB(cac->wlc->pub)) {
		FOREACHSCB(cac->wlc->scbstate, &scbiter, scb) {
			if (SCB_WME(scb)) {
				scb_acinfo = SCB_ACINFO(cac, scb);
				while ((ts = scb_acinfo->tsentryq)) {
					scb_acinfo->tsentryq = ts->next;
					MFREE(cac->wlc->osh, ts, sizeof(tsentry_t));
				}
			}
		}
	}
#endif /* AP */

#ifdef	STA
	/* free all TSPEC for STA */
	while ((ts = cac->tsentryq)) {
		cac->tsentryq = ts->next;
		MFREE(cac->wlc->osh, ts, sizeof(tsentry_t));
	}
#endif

	wlc_module_unregister(cac->wlc->pub, "cac", cac);
	MFREE(cac->wlc->osh, cac, sizeof(wlc_cac_t));
}

static int
wlc_cac_down(void *hdl)
{
	int callbacks = 0;
	wlc_cac_t *cac = (wlc_cac_t *)hdl;

	if (cac->addts_timer && !wl_del_timer(cac->wlc->wl, cac->addts_timer))
		callbacks = 1;

	return callbacks;
}

/* count number of accepted TSPEC in queue */
static INLINE int
wlc_cac_num_ts_accepted(wlc_cac_t *cac, tsentry_t *tsentryq)
{
	tsentry_t *ts;
	int n = 0;

	for (ts = tsentryq; ts; ts = ts->next) {
		if (ts->ts_state == CAC_STATE_TSPEC_ACCEPTED)
			n++;
	}
	return n;
}

static int
wlc_cac_get_tspec(wlc_cac_t *cac, tsentry_t *tsentryq, tspec_arg_t *tspec_arg, uint8 tid)
{
	tsentry_t *ts;

	if (!(ts = wlc_cac_tsentry_find(tsentryq, tid)))
		return BCME_NOTFOUND;

	tspec_arg->version = TSPEC_ARG_VERSION;
	tspec_arg->length = sizeof(tspec_arg_t) - (sizeof(uint16) * 2);
	tspec_arg->flag = ts->ts_state;

	/*
	* copy from packed structure to un-packed structure, need
	* to break up into two separate bcopy
	*/
	bcopy((uint8 *)&ts->tspec.tsinfo, (uint8 *)&tspec_arg->tsinfo,
		sizeof(tsinfo_t));
	bcopy((uint8 *)&ts->tspec.nom_msdu_size, (uint8 *)&tspec_arg->nom_msdu_size,
		TSPEC_ARG_LENGTH - sizeof(tsinfo_t));

	return BCME_OK;

}


/* copies all the tsinfo info into the destination tslist */
static int
wlc_cac_get_tsinfo_list(wlc_cac_t *cac, tsentry_t *ts, void *buff, uint32 bufflen)
{
	int i = 0;
	struct tslist *tslist = (struct tslist *)buff;
	int max_tsinfo = (bufflen - sizeof(int))/sizeof(tsinfo_t);
	int num_ts_accepted = wlc_cac_num_ts_accepted(cac, ts);

	if (max_tsinfo < num_ts_accepted)
		return BCME_BUFTOOSHORT;

	tslist->count = num_ts_accepted;
	for (i = 0; ts && i < tslist->count;
			ts = ts->next, i++) {
		bcopy((uint8 *)&ts->tspec.tsinfo, (uint8 *)&tslist->tsinfo[i],
			sizeof(struct tsinfo));
	}

	return BCME_OK;
}

static int
wlc_cac_iovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_cac_t *cac = (wlc_cac_t *)hdl;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr = (int32 *)a;
	wlc_bsscfg_t *bsscfg;

	/* update bsscfg pointer */
	bsscfg = wlc_bsscfg_find_by_wlcif(cac->wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	switch (actionid) {
		case IOV_GVAL(IOV_CAC):
			*ret_int_ptr = cac->wlc->pub->_cac;
			break;

		case IOV_SVAL(IOV_CAC):
			if (!WME_ENAB(cac->wlc->pub))
				return BCME_WME_NOT_ENABLED;
			cac->wlc->pub->_cac = (int_val != 0);	/* set CAC enable/disable */
			break;


		case IOV_SVAL(IOV_CAC_ADDTS):
			err = wlc_cac_sta_addts(cac, (tspec_arg_t*)a);
			break;

		case IOV_SVAL(IOV_CAC_DELTS):
			{
				struct scb *scb;

				if (!(scb = wlc_scbfind(cac->wlc, bsscfg, &cac->curr_bssid)))
					return BCME_NOTFOUND;

				err = wlc_cac_sta_delts(cac, (tspec_arg_t*)a, scb, cac->tsentryq);
			}
			break;

		case IOV_GVAL(IOV_CAC_TSLIST):
		{
			if (!bsscfg->BSS)
				return BCME_ERROR;

			if (BSSCFG_STA(bsscfg))
				err = wlc_cac_get_tsinfo_list(cac, cac->tsentryq, a, alen);
			else
				err = BCME_UNSUPPORTED;

			break;
		}
#ifdef AP
		case IOV_GVAL(IOV_CAC_TSLIST_EA):
		{
			struct ether_addr *ea;
			struct scb *scb;
			struct cac_scb_acinfo *scb_acinfo;

			/* extract ea */
			ea = (struct ether_addr *)p;

			if (ETHER_ISMULTI(ea))
				return BCME_BADARG;

			if (!(scb = wlc_scbfind(cac->wlc, bsscfg, ea)))
				return BCME_NOTFOUND;

			scb_acinfo = SCB_ACINFO(cac, scb);
			err = wlc_cac_get_tsinfo_list(cac, scb_acinfo->tsentryq, a, alen);
		}
		break;

		case IOV_GVAL(IOV_CAC_TSPEC_EA):
		{
			struct ether_addr *ea;
			struct scb *scb;
			struct cac_scb_acinfo *scb_acinfo;
			tspec_per_sta_arg_t *tsea;
			tspec_arg_t *tspec_arg;

			/* extract ea and tspec */
			tsea = (tspec_per_sta_arg_t *)p;
			ea = &tsea->ea;
			tspec_arg = &tsea->ts;

			if (ETHER_ISMULTI(ea))
				return BCME_BADARG;

			if (!(scb = wlc_scbfind(cac->wlc, bsscfg, ea)))
				return BCME_NOTFOUND;

			scb_acinfo = SCB_ACINFO(cac, scb);

			err = wlc_cac_get_tspec(cac, scb_acinfo->tsentryq, (tspec_arg_t *)a,
				WLC_CAC_GET_TID(tspec_arg->tsinfo));

		}
		break;

		case IOV_GVAL(IOV_CAC_DELTS_EA):
		{
			struct ether_addr *ea;
			struct scb *scb;
			struct cac_scb_acinfo *scb_acinfo;
			tspec_per_sta_arg_t *tsea;
			tspec_arg_t tspec_arg;

			/* extract ea and tspec */
			tsea = (tspec_per_sta_arg_t *)p;
			ea = &tsea->ea;

			if (ETHER_ISMULTI(ea))
				return BCME_BADARG;

			if (!(scb = wlc_scbfind(cac->wlc, bsscfg, ea)))
				return BCME_NOTFOUND;

			bcopy((uint8*)&tsea->ts, (uint8*)&tspec_arg, sizeof(tspec_arg));
			scb_acinfo = SCB_ACINFO(cac, scb);
			err = wlc_cac_sta_delts(cac, &tspec_arg, scb, scb_acinfo->tsentryq);
		}
		break;

		case IOV_SVAL(IOV_CAC_AC_BW):
			if (AP_ENAB(cac->wlc->pub)) {
				if ((int_val < 0) && (int_val > 100))
					return BCME_BADARG;
				cac->admctl_bw_percent = (uint8)int_val;
				cac->available_medium_time = AVAILABLE_MEDIUM_TIME_32US -
					(AVAILABLE_MEDIUM_TIME_32US * cac->admctl_bw_percent)/100;
			} else {
				return BCME_UNSUPPORTED;
			}
			break;

		case IOV_GVAL(IOV_CAC_AC_BW):
			if (AP_ENAB(cac->wlc->pub))
				*ret_int_ptr = cac->admctl_bw_percent;
			else
				return BCME_UNSUPPORTED;
			break;
#endif /* AP */

		case IOV_GVAL(IOV_CAC_TSPEC):
		{
			tspec_arg_t *tspec_arg;

			if (!bsscfg->BSS)
				return BCME_ERROR;

			if (BSSCFG_STA(bsscfg) && bsscfg->associated) {
				tspec_arg = (tspec_arg_t *)p;

				err = wlc_cac_get_tspec(cac, cac->tsentryq, (tspec_arg_t *)a,
					WLC_CAC_GET_TID(tspec_arg->tsinfo));

			} else {
				return BCME_NOTASSOCIATED;
			}

			break;
		}


		default:
			err = BCME_BADARG;
	}
	return err;
}

static tsentry_t *
wlc_cac_tsentry_find(tsentry_t *ts, uint8 tid)
{
	for (; ts; ts = ts->next) {
		if (WLC_CAC_GET_TID(ts->tspec.tsinfo) == tid)
			return ts;		/* found */
	}
	return NULL;
}

/* find first TSPEC in given state */
static tsentry_t *
wlc_cac_tspec_state_find(tsentry_t *ts, int state)
{
	for (; ts; ts = ts->next) {
		if (ts->ts_state == state)
			return ts;		/* found */
	}
	return NULL;
}

static void
wlc_cac_tsentry_append(wlc_cac_t *cac, tsentry_t *ts, tsentry_t **ptsentryq)
{
	tsentry_t **ts_ptr;

	ts->next = NULL;

	/* goto end of list */
	for (ts_ptr = ptsentryq; *ts_ptr != NULL; ts_ptr = &(*ts_ptr)->next);

	*ts_ptr = ts;
}

static int
wlc_cac_tsentry_removefree(wlc_cac_t *cac, uint8 tid, uint event_status, uint code,
	tsentry_t **ptsentryq, bool call_event)
{
	tsentry_t *ts;
	tsentry_t **prev;

	for (prev = ptsentryq; *prev != NULL; prev = &(*prev)->next)
		if (WLC_CAC_GET_TID((*prev)->tspec.tsinfo) == tid)
			break;

	if (!(ts = *prev))
		return BCME_NOTFOUND;

	/* try to remove and free a TSPEC that has outstanding ADDTS
	* request. Disregard this action.
	* This can happen when Application issue ADDTS just before DELTS
	* is received by the STA. Let the ADDTS state machine to deal with it.
	*/
	if (ts->ts_state == CAC_STATE_TSPEC_WAIT_RESP)
		return BCME_ERROR;

	*prev = ts->next;

#ifdef	AP
	/* restore the global available medium time for all non downlink traffic */
	if (AP_ENAB(cac->wlc->pub))
		cac->available_medium_time += ts->tspec.medium_time;
#endif

	MFREE(cac->wlc->osh, ts, sizeof(tsentry_t));
	WL_CAC(("wl%d: %s: remove and free TSPEC tid %d"
	        " status %d code %d\n", cac->wlc->pub->unit, __FUNCTION__,
	        tid, event_status, code));
	if (call_event)
		wlc_cac_delts_ind_complete(cac->wlc, event_status,
		        &cac->wlc->cfg->target_bss->BSSID,
			code, tid);

	return 0;
}

/* recalculate medium time for each AC */
static void
wlc_cac_medium_time_recal(wlc_cac_t *cac, struct cac_scb_acinfo *scb_acinfo)
{
	uint8 ac;
	tsentry_t *ts;
	int i;

	for (i = 0; i < AC_COUNT; i++)
		scb_acinfo->cac_ac[i].tot_medium_time = 0;

	for (ts = cac->tsentryq; ts; ts = ts->next) {
		ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(ts->tspec.tsinfo));
		scb_acinfo->cac_ac[ac].tot_medium_time =
			USEC32_TO_USEC(ltoh16_ua(&ts->tspec.medium_time));
	}
}

/* Find the IE in tlvs (pattern pointed by str with strlen) */
static uint8 *
wlc_cac_ie_find(uint8 *tlvs, uint *tlvs_len, uint8 id, uint max_len,
const 	int8 *str, uint str_len)
{
	while ((tlvs = (uint8 *)bcm_parse_tlvs(tlvs, *tlvs_len, id)) != NULL) {
		if (tlvs[TLV_LEN_OFF] >= max_len &&
			!bcmp(&tlvs[TLV_BODY_OFF], str, str_len))
			return tlvs;
		/* calculate the length of the rest of the buffer */
		*tlvs_len -= tlvs[TLV_LEN_OFF] + TLV_HDR_LEN;
		/* point to the next ie */
		tlvs += tlvs[TLV_LEN_OFF] + TLV_HDR_LEN;
	}

	return NULL;
}

/* Send a action frame (tspec) request containing single WME TSPEC element */
static int
wlc_cac_tspec_req_send(wlc_cac_t *cac, tspec_t *ts,	uint8 action,
	uint8 dialog_token, int reason_code, void **pkt, struct scb *scb)
{
	uint len = 0;
	struct dot11_management_notification *mgmt_hdr;
	uint8 *body, *cp, tid, user_prio;
	wlc_info_t *wlc = cac->wlc;
	struct ether_addr *da = (struct ether_addr *)&scb->ea;
	uint16 orig_medium_time;

	WL_TRACE(("wl%d: wlc_cac_tspec_req_send\n", wlc->pub->unit));

	*pkt = NULL;

	if (!(wlc->pub->associated))
		return BCME_NOTASSOCIATED;

	ASSERT(scb);

	if (!SCB_WME(scb)) {
		WL_CAC(("wl%d: wlc_cac_tspec_req_send: WME Not Enabled\n",
			wlc->pub->unit));
		return BCME_NOT_WME_ASSOCIATION;
	}


	/* TSPEC Request frame is 4 bytes Management Notification frame
	 * follow by WME TSPEC element
	 */
	len = DOT11_MGMT_NOTIFICATION_LEN +
		(TLV_HDR_LEN + WME_TSPEC_LEN);

	if ((*pkt = wlc_frame_get_mgmt(wlc, FC_ACTION, da, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, len, &body)) == NULL)
		return BCME_ERROR;

	len = 0;
	mgmt_hdr = (struct dot11_management_notification *)body;
	mgmt_hdr->category = DOT11_ACTION_NOTIFICATION;
	mgmt_hdr->action = action;

	/* token must be non-zero for ADDTS - set high bit */
	if (action == WME_ADDTS_REQUEST)
		mgmt_hdr->token = dialog_token;
	else
		mgmt_hdr->token = 0;

	mgmt_hdr->status = 0;	/* always zero for STA */
	len += DOT11_MGMT_NOTIFICATION_LEN;
	cp = mgmt_hdr->data;

	WL_CAC(("wl%d: %s: construct action frame %s\n", wlc->pub->unit,
		__FUNCTION__, (action == WME_ADDTS_REQUEST)?"ADDTS":"DELTS"));

	tid = WLC_CAC_GET_TID(ts->tsinfo);
	user_prio = WLC_CAC_GET_USER_PRIO(ts->tsinfo);

	/* Fill in TSPEC values
	 * WMM Section 3.5.3 mandates that medium_time in an ADDTS TSPEC must be 0.
	 */
	orig_medium_time = ts->medium_time;	/* save */
	ts->medium_time = 0;
	cp = wlc_write_info_elt(cp, DOT11_MNG_PROPR_ID, WME_TSPEC_LEN, ts);
	len += (TLV_HDR_LEN + WME_TSPEC_LEN);
	ts->medium_time = orig_medium_time;	/* restore */


	ASSERT((cp - body) == (int)len);
	/* adjust the packet length */
	PKTSETLEN(wlc->osh, *pkt,
		(uint)(body - (uint8 *)PKTDATA(cac->wlc->osh, *pkt)) + len);


	if (wlc_sendmgmt(wlc, *pkt, SCB_WLCIFP(scb)->qi, NULL))
		return 0;

	return BCME_ERROR;
}

/* This function send ADDTS request */
static int
wlc_cac_addts_req_send(wlc_cac_t *cac, tspec_t *ts, uint8 dialog_token)
{
	int err;
	void *pkt;
	struct scb *scb;

#ifdef MBSS
	if (MBSS_ENAB(cac->wlc->pub)) {
		/* There's a BSS id in wlc_cac_t; maybe use that? */
		if (!(scb = wlc_scbfind(cac->wlc, cac->curr_bsscfg, &cac->curr_bssid)) &&
		    !(scb = wlc_scbfind(cac->wlc, cac->wlc->cfg, &cac->wlc->cfg->BSSID))) {
			return BCME_NOTFOUND;
		}
	} else
#endif /* MBSS */
	if (!(scb = wlc_scbfind(cac->wlc, cac->wlc->cfg, &cac->wlc->cfg->BSSID))) {
		return BCME_NOTFOUND;
	}

	if ((err = wlc_cac_tspec_req_send(cac, ts, WME_ADDTS_REQUEST, dialog_token, 0, &pkt, scb)))
		return err;

	return 0;
}

/* send next pending TSPEC in the tsentry queue */
static int
wlc_cac_tspec_send(wlc_cac_t *cac, tsentry_t *tsentryq)
{
	int err = 0;
	tsentry_t *ts;

	/* check if there is a TSPEC in progress */
	if (cac->waiting_resp)
		return 0;

	WL_CAC(("wl%d: %s: find next TSPEC pending queue\n",
		cac->wlc->pub->unit, __FUNCTION__));

	/* find next pending TSPEC */
	for (ts = tsentryq; ts; ts = ts->next) {
		if ((ts->ts_state == CAC_STATE_TSPEC_PENDING) ||
			(ts->ts_state == CAC_STATE_TSPEC_UPDATE_PENDING))
			break;
	}

	if (ts == NULL)
		return 0;

	if (!(err = wlc_cac_addts_req_send(cac, &ts->tspec, ts->dialog_token))) {
		uint32 timeout = cac->addts_timeout;
		wlc_bsscfg_t *cfg = cac->wlc->cfg;
		cac->waiting_resp = TRUE;
		if (cfg->pm->PMenabled)
			timeout += CAC_ADDTS_RESP_TIMEOUT_FF;
		if (ts->ts_state == CAC_STATE_TSPEC_UPDATE_PENDING)
			ts->ts_state = CAC_STATE_TSPEC_UPDATE_WAIT_RESP;
		else
			ts->ts_state = CAC_STATE_TSPEC_WAIT_RESP;

		WL_CAC(("wl%d: %s: start addts"
			" response timer\n", cac->wlc->pub->unit, __FUNCTION__));
		wl_add_timer(cac->wlc->wl, cac->addts_timer, cac->addts_timeout, 0);
	} else {
		uint8 tid = WLC_CAC_GET_TID(ts->tspec.tsinfo);

		WL_CAC(("wl%d: %s: tid %d Err %d\n",
			cac->wlc->pub->unit, __FUNCTION__, tid, err));
		ts->ts_state = CAC_STATE_TSPEC_REJECTED;
		wlc_cac_tsentry_removefree(cac, tid, WLC_E_STATUS_UNSOLICITED,
			DOT11E_STATUS_UNKNOWN_TS, &cac->tsentryq, TRUE);
	}
	return err;
}

/* IOVAR ADDTS. Construct TSPEC from tspec_arg
 * append the TSPEC to tsentry queue and
 * transmit action frame
 */
static int
wlc_cac_sta_addts(wlc_cac_t *cac, tspec_arg_t *tspec_arg)
{
	tspec_t *tspec;
	tsentry_t *ts;
	int err = 0;
	uint8 tid;
	wlc_bsscfg_t *cfg = cac->wlc->cfg;

	if (tspec_arg->version != TSPEC_ARG_VERSION)
		return BCME_BADARG;

	if (!tspec_arg->dialog_token)
		return BCME_BADARG;

	if (tspec_arg->length < sizeof(tspec_arg_t) - (sizeof(uint16) * 2))
		return BCME_BADLEN;



	/* check if we support APSD */
	if (WLC_CAC_GET_PSB(tspec_arg->tsinfo) && !cfg->wme->wme_apsd) {
		WL_CAC(("No support for APSD\n"));
		return BCME_BADARG;
	}

	tid = WLC_CAC_GET_TID(tspec_arg->tsinfo);
	if ((ts = wlc_cac_tsentry_find(cac->tsentryq, tid))) {
		/* check if target TSPEC is currently waiting for resp or roaming */
		if (ts->ts_state == CAC_STATE_TSPEC_WAIT_RESP)
			return BCME_BUSY;


		if (ts->ts_state == CAC_STATE_TSPEC_ACCEPTED) {
			/* TSPEC is already accepted and this request
			 * is an update to the existing one
			 */
			cac->cached_medium_time = (uint)ts->tspec.medium_time;
			tspec = &ts->tspec;
			WL_CAC(("Update the existing tspec : Previous medium time ix %d\n",
				cac->cached_medium_time));
		} else {
			return BCME_ERROR;
		}

	} else {
		/* not found, malloc and construct TSPEC frame */
		if (!(ts = (tsentry_t *)MALLOC(cac->wlc->osh, sizeof(tsentry_t)))) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				cac->wlc->pub->unit, __FUNCTION__, MALLOCED(cac->wlc->osh)));
			return BCME_NORESOURCE;
		}
		bzero((char *)ts, sizeof(tsentry_t));

		tspec = &ts->tspec;
		bcopy(WME_OUI, tspec->oui, WME_OUI_LEN);
		tspec->type = WME_OUI_TYPE;
		tspec->subtype = WME_SUBTYPE_TSPEC;
		tspec->version = WME_VER;

		wlc_cac_tsentry_append(cac, ts, &cac->tsentryq);
	}

	/* convert input to 802.11 little-endian order */
	tspec_arg->nom_msdu_size = htol16(tspec_arg->nom_msdu_size);
	tspec_arg->max_msdu_size = htol16(tspec_arg->max_msdu_size);
	tspec_arg->min_srv_interval = htol32(tspec_arg->min_srv_interval);
	tspec_arg->max_srv_interval = htol32(tspec_arg->max_srv_interval);
	tspec_arg->inactivity_interval = htol32(tspec_arg->inactivity_interval);
	tspec_arg->suspension_interval = htol32(tspec_arg->suspension_interval);
	tspec_arg->srv_start_time = htol32(tspec_arg->srv_start_time);
	tspec_arg->min_data_rate = htol32(tspec_arg->min_data_rate);
	tspec_arg->mean_data_rate = htol32(tspec_arg->mean_data_rate);
	tspec_arg->peak_data_rate = htol32(tspec_arg->peak_data_rate);
	tspec_arg->max_burst_size = htol32(tspec_arg->max_burst_size);
	tspec_arg->delay_bound = htol32(tspec_arg->delay_bound);
	tspec_arg->min_phy_rate = htol32(tspec_arg->min_phy_rate);
	tspec_arg->surplus_bw = htol16(tspec_arg->surplus_bw);

	bcopy(tspec_arg->tsinfo.octets, tspec->tsinfo.octets, sizeof(tsinfo_t));
	bcopy((uint8 *)&tspec_arg->nom_msdu_size, (uint8 *)&tspec->nom_msdu_size,
		WLC_CAC_TSPECARG_COPYSIZE);

	bcopy(&tspec_arg->min_phy_rate, &tspec->min_phy_rate, sizeof(uint));
	bcopy(&tspec_arg->surplus_bw, &tspec->surplus_bw, sizeof(uint16));

	ts->dialog_token = tspec_arg->dialog_token;

	WL_CAC(("wl%d: wlc_cac_sta_addts: IOVAR ADDTS tid %d,\n",
		cac->wlc->pub->unit, tid));
	if (ts->ts_state == CAC_STATE_TSPEC_ACCEPTED)
		ts->ts_state = CAC_STATE_TSPEC_UPDATE_PENDING;
	else
		ts->ts_state = CAC_STATE_TSPEC_PENDING;

	err = wlc_cac_tspec_send(cac, cac->tsentryq);

	return err;
}

/* IOVAR DELTS. Send TSPEC action frame DELTS and remove & free the TSPEC
 * from ts entry queue regardless if the send is successful.
 */
static int
wlc_cac_sta_delts(wlc_cac_t *cac, tspec_arg_t *tspec_arg, struct scb *scb,
	tsentry_t *tsentryq)
{
	tsentry_t *ts;
	uint8 tid, ac;
	void *pkt;
	struct cac_scb_acinfo *scb_acinfo;
	tsentry_t **ptsentryq = NULL;

	ASSERT(scb);

	scb_acinfo = SCB_ACINFO(cac, scb);

	if (tspec_arg->version != TSPEC_ARG_VERSION)
		return BCME_BADARG;

	if (tspec_arg->length < sizeof(tspec_arg_t) - (sizeof(uint16) * 2))
		return BCME_BADLEN;

	tid = WLC_CAC_GET_TID(tspec_arg->tsinfo);
	ts = wlc_cac_tsentry_find(tsentryq, tid);

	if (!ts)
		return BCME_NOTFOUND;

	if (ts->ts_state == CAC_STATE_TSPEC_WAIT_RESP)
		return BCME_BUSY;

	/* direction setting should match */
	if (WLC_CAC_GET_DIR(tspec_arg->tsinfo) !=
		WLC_CAC_GET_DIR(ts->tspec.tsinfo))
		return BCME_BADARG;

	wlc_cac_tspec_req_send(cac, &ts->tspec, WME_DELTS_REQUEST,
		ts->dialog_token, DOT11E_STATUS_QSTA_LEAVE_QBSS, &pkt, scb);

	/* get the ac before free the TSPEC */
	ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(ts->tspec.tsinfo));

	/* removing a TSPEC that is not waiting on response, therefore no
	 * need to reset waiting_resp or restart TSPEC send.
	 */

	if (!AP_ENAB(cac->wlc->pub)) {
		ptsentryq = &cac->tsentryq;
	}
#ifdef AP
	else {
		ptsentryq = &scb_acinfo->tsentryq;
	}
#endif

	wlc_cac_tsentry_removefree(cac, tid, WLC_E_STATUS_SUCCESS,
		DOT11E_STATUS_QSTA_LEAVE_QBSS, ptsentryq, TRUE);


	if (CAC_ENAB(cac->wlc->pub)) {
		/* Reset the power save to default settings */
		if (scb->apsd.ac_defl & (1 << ac)) {
			scb->apsd.ac_delv |= (1 << ac);
			scb->apsd.ac_trig |= (1 << ac);
			WL_CAC(("Restoring the original APSD settings\n"));
		} else {
			scb->apsd.ac_delv &= ~(1 << ac);
			scb->apsd.ac_trig &= ~(1 << ac);
			WL_CAC(("Restoring the original Legacy PS settings\n"));
		}

		scb->flags &= ~SCB_APSDCAP;
		if (scb->apsd.ac_trig & AC_BITMAP_ALL)
			scb->flags |= SCB_APSDCAP;
	}

	/* reset the admitted flag */
	scb_acinfo->cac_ac[ac].admitted = FALSE;
	wlc_cac_ac_param_reset(cac, ac, scb_acinfo);

	return 0;
}

/* parse TSPEC and update medium time received tspec */
static uint8 *
wlc_cac_tspec_rx_process(wlc_cac_t *cac, uint8 *body, uint *body_len,
	uint8 *tid, uint8 *user_prio, struct cac_scb_acinfo *scb_acinfo, int *err)
{
	uint8 ac;
	tsentry_t *ts;
	tspec_t *tspec;

	WL_CAC(("%s: Entering\n", __FUNCTION__));

	if (*body_len < WME_TSPEC_LEN) {
		*err = BCME_ERROR;
		return body;
	}

	*err = 0;

	tspec = (tspec_t *)&body[TLV_BODY_OFF];		/* skip over ID & len field */

	*tid = WLC_CAC_GET_TID(tspec->tsinfo);
	*user_prio = WLC_CAC_GET_USER_PRIO(tspec->tsinfo);

	ac = WME_PRIO2AC(*user_prio);
	BCM_REFERENCE(ac);

	ts = wlc_cac_tsentry_find(cac->tsentryq, *tid);

#ifdef	BCMDBG
	if (ts && (ts->ts_state == CAC_STATE_TSPEC_UPDATE_WAIT_RESP))
		WL_CAC(("Previous alloted medium time is %d\n",
			cac->cached_medium_time));
#endif	/* BCMDBG */

	/* need to filter receiving duplicated response */
	if (ts && (ts->ts_state != CAC_STATE_TSPEC_ACCEPTED))
		ts->ts_state = CAC_STATE_TSPEC_ACCEPTED;

	if (ts != NULL)
		bcopy(tspec, &ts->tspec, WME_TSPEC_LEN);

	/* recal total medium time */
	wlc_cac_medium_time_recal(cac, scb_acinfo);

	WL_CAC(("wl%d: wlc_cac_tspec_rx_process: add TSPEC to AC %d queue,"
		" tid %d, user priority %d, total medium time %d (microseconds)\n",
		cac->wlc->pub->unit, ac, *tid, *user_prio, scb_acinfo->cac_ac[ac].tot_medium_time));

	/* update the length of the buffer */
	*body_len -= body[TLV_LEN_OFF] + TLV_HDR_LEN;
		/* point to the next ie */
	body += body[TLV_LEN_OFF] + TLV_HDR_LEN;

	return body;
}

/* handle TSPEC IE from ADDTS response and (re)assoc response
 * parse TSPEC IE and other IEs. Return error code
 */
static int
wlc_cac_ie_process(wlc_cac_t *cac, uint8 *tlvs, uint tlvs_len, uint8 *tid,
	uint8 *user_prio, struct scb *scb)
{
	int err;
	struct cac_scb_acinfo *scb_acinfo;

	scb_acinfo = SCB_ACINFO(cac, scb);
	ASSERT(scb_acinfo);

	while (tlvs_len > 0 && tlvs) {
		err = 0;
		tlvs = wlc_cac_ie_find(tlvs, &tlvs_len, DOT11_MNG_PROPR_ID, WME_TSPEC_LEN,
		(const int8 *)DOT11E_TSPEC_IE, DOT11E_TSPEC_OUI_TYPE_LEN);
		/* process tspec + all other IEs */
		if (tlvs) {
			uint8 ac;
			tlvs = wlc_cac_tspec_rx_process(cac, tlvs, &tlvs_len, tid,
				user_prio, scb_acinfo, &err);
			if (err)
				return err;

			ac = WME_PRIO2AC(*user_prio);


			AC_BITMAP_SET(cac->flags, ac);

		}
	}

	return 0;
}


/* processes ADDTS response frame from the AP */
static void
wlc_cac_addts_resp(wlc_cac_t *cac, uint8 *body, uint body_len, struct scb *scb)
{
	uint8 ts_status, status;
	tsentry_t *ts;
	uint8 user_prio = 0;
	uint8 tid = -1;
	uint ts_delay = 0;
	struct cac_scb_acinfo *scb_acinfo = SCB_ACINFO(cac, scb);

	if (body_len < (WME_TSPEC_LEN + DOT11_MGMT_NOTIFICATION_LEN))
		return;

	/* If reject due to invalid param, check and see if is related to
	 * the nominal phy rate not supported by the AP
	 */
	ts_status = body[WME_STATUS_CODE_OFFSET];
	body += DOT11_MGMT_NOTIFICATION_LEN;
	body_len -= DOT11_MGMT_NOTIFICATION_LEN;


	/* no TSPEC waiting for ADDTS response */
	if ((ts = wlc_cac_tspec_state_find(cac->tsentryq,
		CAC_STATE_TSPEC_WAIT_RESP)) == NULL) {

		/* no TSPEC waiting for update */
		if ((ts = wlc_cac_tspec_state_find(cac->tsentryq,
			CAC_STATE_TSPEC_UPDATE_WAIT_RESP)) == NULL) {
			return;
		}
	}


	/* handle specific error */
	if (ts_status == DOT11E_STATUS_ADDTS_INVALID_PARAM) {
		WL_CAC(("wl%d: wlc_cac_addts_resp: CAC_ADDTS_INVALID_PARAM"
			" status %d\n", cac->wlc->pub->unit, ts_status));
	}


	/* handle all errors */
	if (ts_status != DOT11E_STATUS_ADMISSION_ACCEPTED) {
		status = WLC_E_STATUS_FAIL;
		goto event;
	}

	WL_CAC(("wl%d: wlc_cac_addts_resp: CAC_ADMISSION_ACCEPTED"
		" status %d\n", cac->wlc->pub->unit, ts_status));


	/* TSPEC admission accepted */
	if (wlc_cac_ie_process(cac, body, body_len, &tid, &user_prio, scb) == 0) {
		/* cancel timer only if we received the TSPEC with matching TID */
		if (tid == WLC_CAC_GET_TID(ts->tspec.tsinfo)) {
			wl_del_timer(cac->wlc->wl, cac->addts_timer);
			WL_CAC(("wl%d: wlc_cac_addts_resp: Kill timer tid %d"
				" Admission Accepted\n", cac->wlc->pub->unit, tid));
			cac->waiting_resp = FALSE;
			scb_acinfo->cac_ac[WME_PRIO2AC(user_prio)].admitted = TRUE;

			/* update the power save behavior */
			if (CAC_ENAB(cac->wlc->pub)) {
				int ac;
				wlc_bsscfg_t *cfg;
				cfg = SCB_BSSCFG(scb);
				ASSERT(cfg != NULL);
				if (WLC_CAC_GET_PSB(ts->tspec.tsinfo) && cfg->wme->wme_apsd) {
					ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(ts->tspec.tsinfo));
					switch (WLC_CAC_GET_DIR(ts->tspec.tsinfo)) {
						case (TS_INFO_UPLINK >> TS_INFO_DIRECTION_SHIFT):
							AC_BITMAP_SET(scb->apsd.ac_trig, ac);
							AC_BITMAP_RESET(scb->apsd.ac_delv, ac);
							WL_CAC(("AC[%d] : Trigger enabled\n", ac));
							break;

						case (TS_INFO_DOWNLINK >> TS_INFO_DIRECTION_SHIFT):
							AC_BITMAP_SET(scb->apsd.ac_delv, ac);
							AC_BITMAP_RESET(scb->apsd.ac_trig, ac);
							WL_CAC(("AC[%d] : Delivery enabled\n", ac));
							break;

						case (TS_INFO_BIDIRECTIONAL >>
							TS_INFO_DIRECTION_SHIFT):
							AC_BITMAP_SET(scb->apsd.ac_trig, ac);
							AC_BITMAP_SET(scb->apsd.ac_delv, ac);
							WL_CAC(("AC[%d] : Trig & Delv enabled\n",
								ac));
							break;
						}
				} else {
					ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(ts->tspec.tsinfo));
					AC_BITMAP_RESET(scb->apsd.ac_delv, ac);
					AC_BITMAP_RESET(scb->apsd.ac_trig, ac);
					WL_CAC(("AC [%d] : Legacy Power save\n", ac));
				}
			}
		}

		status = WLC_E_STATUS_SUCCESS;
	} else {
		status = WLC_E_STATUS_FAIL;
	}

event:
	wlc_cac_addts_ind_complete(cac->wlc, status, &cac->wlc->cfg->target_bss->BSSID,
		body[WME_STATUS_CODE_OFFSET], tid, ts_delay);

	/* if status != WLC_E_STATUS_SUCCESS need to check if this response is for
	 * outstanding TSPEC that are waiting for response
	 */
	if (status != WLC_E_STATUS_SUCCESS) {
		body = wlc_cac_ie_find(body, &body_len, DOT11_MNG_PROPR_ID, WME_TSPEC_LEN,
		(const int8 *)DOT11E_TSPEC_IE, DOT11E_TSPEC_OUI_TYPE_LEN);
		if (body) {
			tspec_t *tspec = (tspec_t *)&body[TLV_BODY_OFF];
			tid = WLC_CAC_GET_TID(tspec->tsinfo);
			/* cancel timer only if we received the TSPEC with matching TID */
			if (tid == WLC_CAC_GET_TID(ts->tspec.tsinfo)) {
				wl_del_timer(cac->wlc->wl, cac->addts_timer);
				WL_CAC(("wl%d: wlc_cac_addts_resp: Kill timer tid %d"
					" Admission Failed\n", cac->wlc->pub->unit, tid));

				if (ts->ts_state != CAC_STATE_TSPEC_UPDATE_WAIT_RESP) {
					/* change ts_state so that removefree
					 * can get rid of it
					 */
					ts->ts_state = CAC_STATE_TSPEC_REJECTED;
					wlc_cac_tsentry_removefree(cac, tid,
						WLC_E_STATUS_UNSOLICITED, ts_status,
						&cac->tsentryq, TRUE);
					cac->waiting_resp = FALSE;
				}
				else {
					WL_CAC(("Restore the previous tspec settings\n"));
					ASSERT(cac->cached_medium_time);
					scb_acinfo->cac_ac[WME_PRIO2AC(user_prio)].tot_medium_time =
						cac->cached_medium_time;
					ts->tspec.medium_time = (uint16)cac->cached_medium_time;
					ts->ts_state = CAC_STATE_TSPEC_ACCEPTED;
					cac->cached_medium_time = 0;
				}
			}
		}
		WL_CAC(("wl%d: wlc_cac_addts_resp: addts admission not accepted"
			" tid %d\n", cac->wlc->pub->unit, tid));
	}

	/* send next tspec pending */
	wlc_cac_tspec_send(cac, cac->tsentryq);
}

/* On DELTS request, delete the TSPEC */
static void
wlc_cac_delts_req(wlc_cac_t *cac, uint8 *body, uint body_len, struct scb *scb)
{
	struct dot11_management_notification* mgmt_hdr;
	struct cac_scb_acinfo *scb_acinfo;
	tspec_t *tspec;
	tsentry_t **ptsentryq;
	uint8 tid, ac;

	scb_acinfo = SCB_ACINFO(cac, scb);

	if (body_len < WME_TSPEC_LEN) {
		WLCNTINCR(cac->wlc->pub->_cnt->rxbadcm);
		return;
	}

	WL_CAC(("wl%d: %s: receive DELTS\n", cac->wlc->pub->unit, __FUNCTION__));

	mgmt_hdr = (struct dot11_management_notification *)body;
	tspec = (tspec_t *)&mgmt_hdr->data[TLV_BODY_OFF];	/* skip ID & len */
	tid = WLC_CAC_GET_TID(tspec->tsinfo);
	ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(tspec->tsinfo));

	wlc_cac_ac_param_reset(cac, ac, scb_acinfo);

	/* Reset the power save to default settings */
	if (scb->apsd.ac_defl & (1 << ac)) {
		scb->apsd.ac_delv |= (1 << ac);
		scb->apsd.ac_trig |= (1 << ac);
	} else {
		scb->apsd.ac_delv &= ~(1 << ac);
		scb->apsd.ac_trig &= ~(1 << ac);
	}

	scb->flags &= ~SCB_APSDCAP;
	if (scb->apsd.ac_trig & AC_BITMAP_ALL)
		scb->flags |= SCB_APSDCAP;

	if (!AP_ENAB(cac->wlc->pub)) {
		ptsentryq = &cac->tsentryq;
	}
#ifdef	AP
	else {
		ptsentryq = &scb_acinfo->tsentryq;
	}
#endif

	/* not found or try to remove TSPEC in progress, exit */
	if (wlc_cac_tsentry_removefree(cac, tid, WLC_E_STATUS_UNSOLICITED,
		DOT11E_STATUS_END_TS, ptsentryq, TRUE))
		return;

	wlc_cac_medium_time_recal(cac, scb_acinfo);
}

/* addts response timeout handler. This function shall send a DELTS to
 * the AP to clean-up the TSPEC. This may happen when AP received the
 * ADDTS successfully, but the ACK is lost
 */
static void
wlc_cac_addts_timeout(void *arg)
{
	wlc_info_t *wlc = (wlc_info_t*)arg;
	uint err;
	uint8 tid;
	tsentry_t *ts;
	void *pkt;
	struct scb *scb;

	WL_TRACE(("wl%d: %s\n", wlc->pub->unit, __FUNCTION__));

	if (!wlc->pub->up)
		return;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

	WL_CAC(("wl%d: %s: send DELTS\n", wlc->pub->unit, __FUNCTION__));

	if (!(scb = wlc_scbfind(wlc, wlc->cac->curr_bsscfg, &wlc->cac->curr_bssid)))
		return;

	/* ADDTS timeout triggered; get the TSPEC waiting for response */
	ts = wlc_cac_tspec_state_find(wlc->cac->tsentryq, CAC_STATE_TSPEC_WAIT_RESP);
	if (!ts) {
		wlc_cac_tspec_send(wlc->cac, wlc->cac->tsentryq);
		return;
	}

	/* send delts TS */
	err = wlc_cac_tspec_req_send(wlc->cac, &ts->tspec, WME_DELTS_REQUEST,
		ts->dialog_token, DOT11E_STATUS_QSTA_REQ_TIMEOUT, &pkt, scb);
	BCM_REFERENCE(err);

	/* get ts tid before removing ts */
	tid = WLC_CAC_GET_TID(ts->tspec.tsinfo);

	/* change ts entry state inorder to removefree */
	ts->ts_state = CAC_STATE_TSPEC_REJECTED;
	wlc_cac_tsentry_removefree(wlc->cac, tid, 0, 0, &wlc->cac->tsentryq, FALSE);
	wlc->cac->waiting_resp = FALSE;

	wlc_cac_addts_ind_complete(wlc, WLC_E_STATUS_TIMEOUT, &wlc->cfg->target_bss->BSSID,
		0, tid, 0);

	wlc_cac_tspec_send(wlc->cac, wlc->cac->tsentryq);
}

static void
wlc_cac_addts_ind_complete(wlc_info_t *wlc, uint status, struct ether_addr *addr,
        uint wme_status, uint tid, uint ts_delay)
{
	if (status == WLC_E_STATUS_SUCCESS)
		WL_CAC(("ADDTS: success ...\n"));
	else if (status == WLC_E_STATUS_TIMEOUT)
		WL_CAC(("ADDTS: timeout waiting for ADDTS response\n"));
	else if (status == WLC_E_STATUS_FAIL)
		WL_CAC(("ADDTS: failure, TSPEC rejected\n"));
	else
		WL_ERROR(("MACEVENT: ADDTS, unexpected status param %d\n", (int)status));

	wlc_mac_event(wlc, WLC_E_ADDTS_IND, addr, status, wme_status,
		tid, &ts_delay, sizeof(ts_delay));
}

static void wlc_cac_delts_ind_complete(wlc_info_t *wlc, uint status, struct ether_addr *addr,
        uint wme_status, uint tid)
{
	if (status == WLC_E_STATUS_SUCCESS)
		WL_CAC(("DELTS: success ...\n"));
	else if (status == WLC_E_STATUS_UNSOLICITED)
		WL_CAC(("DELTS: unsolicited\n"));
	else
		WL_ERROR(("MACEVENT: DELTS, unexpected status param %d\n", (int)status));

	wlc_mac_event(wlc, WLC_E_DELTS_IND, addr, status, wme_status, tid, 0, 0);
}

/* Change all TSPEC with from_state to pending state */
static void
wlc_cac_tspec_state_change(wlc_cac_t *cac, int old_state, uint new_state, tsentry_t *tsentryq)
{
	tsentry_t *ts;

	for (ts = tsentryq; ts; ts = ts->next) {
		if (old_state == CAC_STATE_TSPEC_ANY)
			ts->ts_state = new_state;
		else if (ts->ts_state == old_state)
			ts->ts_state = new_state;
	}
}

/* reset all ac parameters */
static void
wlc_cac_ac_param_reset(wlc_cac_t *cac, uint8 ac, struct cac_scb_acinfo *scb_acinfo)
{
	wlc_cac_ac_t *cac_ac;
	cac_ac = &scb_acinfo->cac_ac[ac];

	cac_ac->tot_medium_time = 0;
	cac_ac->used_time = 0;
	cac_ac->cached_dur = 0;

	/* reset the admitted flag */
	cac_ac->admitted = FALSE;

	/* reset nominal phy rate */
	if ((cac->wlc->band->bandtype == WLC_BAND_2G) &&
		(cac->wlc->band->gmode == GMODE_LEGACY_B))
		cac_ac->nom_phy_rate = WLC_RATE_11M;
	else
		cac_ac->nom_phy_rate = WLC_RATE_24M;

	/* if there is a TS */
	if (AC_BITMAP_TST(cac->flags, ac)) {
		wl_lifetime_t lifetime;
		/* clear lifetime */
		lifetime.ac = (uint32)ac;
		lifetime.lifetime = 0;
		if (wlc_iovar_op(cac->wlc, "lifetime", NULL, 0,
			&lifetime, sizeof(lifetime), IOV_SET, NULL))
			WL_ERROR(("wl%d: %s: setting lifetime failed\n",
				cac->wlc->pub->unit, __FUNCTION__));

		AC_BITMAP_RESET(cac->flags, ac);

		/* if no more TS */
		if (!(cac->flags & AC_BITMAP_ALL) && cac->rfaware_lifetime) {
			/* restore RF awareness */
			if (wlc_iovar_setint(cac->wlc, "rfaware_lifetime", cac->rfaware_lifetime))
				WL_ERROR(("wl%d: %s: setting RF awareness lifetime failed\n",
					cac->wlc->pub->unit, __FUNCTION__));
		}
	}
}

/* change all roaming ts_state to pending when roam failed
 * and have the ADDTS state machine to handle TSPEC rejects
 */
void
wlc_cac_tspec_state_reset(wlc_cac_t *cac)
{
	WL_CAC(("wl%d: wlc_cac_roam_tspec: change all roaming state to pending"
		" state\n", cac->wlc->pub->unit));
	/* change all TSPEC from ROAMING to PENDING */
	wlc_cac_tspec_state_change(cac, CAC_STATE_TSPEC_ACCEPTED,
		CAC_STATE_TSPEC_PENDING, cac->tsentryq);
	wlc_cac_tspec_send(cac, cac->tsentryq);
}

/* Reset all cac parameters after disassoc, including release all
 * tspec. Don't required to send delts when disassoc, AP will do
 * its cleanup of tspec.
 */
void
wlc_cac_param_reset_all(wlc_cac_t *cac, struct scb *scb)
{
	uint8 ac;
	tsentry_t *ts;
	uint8 tid;
	struct cac_scb_acinfo *scb_acinfo;

	scb_acinfo = SCB_ACINFO(cac, scb);

	WL_CAC(("wl%d: %s: free all TSPEC & reset AC parameters\n",
		cac->wlc->pub->unit, __FUNCTION__));

	/* timer start when any TSPEC is in state WAIT_RESP */
	if ((ts = wlc_cac_tspec_state_find(cac->tsentryq,
		CAC_STATE_TSPEC_WAIT_RESP)) == NULL)
		wl_del_timer(cac->wlc->wl, cac->addts_timer);

	for (ac = 0; ac < AC_COUNT; ac++)
		wlc_cac_ac_param_reset(cac, ac, scb_acinfo);

	while ((ts = cac->tsentryq)) {
		tid = WLC_CAC_GET_TID(ts->tspec.tsinfo);
		/* indicated up all TSPEC has been deleted */
		wlc_cac_delts_ind_complete(cac->wlc, WLC_E_STATUS_UNSOLICITED,
			&cac->wlc->cfg->target_bss->BSSID, DOT11E_STATUS_QSTA_LEAVE_QBSS, tid);
		cac->tsentryq = ts->next;
		MFREE(cac->wlc->osh, ts, sizeof(tsentry_t));
	}
}

/* CAC watchdog routine, call by wlc_watchdog */
int
wlc_cac_watchdog(void *hdl)
{
	uint8 ac;
	wlc_cac_ac_t *cac_ac;
	wlc_cac_t *cac = (wlc_cac_t *)hdl;
	struct scb *scb = NULL;
	struct cac_scb_acinfo *scb_acinfo = NULL;
#ifdef	AP
	struct scb_iter scbiter;
#endif	/* AP */

	if (!(WME_ENAB(cac->wlc->pub) && CAC_ENAB(cac->wlc->pub)))
		return BCME_OK;

	if (!AP_ENAB(cac->wlc->pub)) {
		if (cac->curr_bsscfg &&
			(scb = wlc_scbfind(cac->wlc, cac->curr_bsscfg, &cac->curr_bssid)) &&
		    SCB_WME(scb)) {
			scb_acinfo = SCB_ACINFO(cac, scb);

			/* reset used time for each AC */
			for (ac = 0; ac < AC_COUNT; ac++) {
				cac_ac = &scb_acinfo->cac_ac[ac];

				if (cac_ac->used_time <= cac_ac->tot_medium_time) {
					cac_ac->used_time = 0;
				} else {
					/* carry over-used time to next tx window */
					cac_ac->used_time -= cac_ac->tot_medium_time;
					/* Indicate to host that used time has been exceeded */
					wlc_mac_event(cac->wlc, WLC_E_EXCEEDED_MEDIUM_TIME,
						&cac->wlc->cfg->target_bss->BSSID, ac, 0, 0, 0, 0);
				}

			}

		}
	}
#ifdef AP

	if (AP_ENAB(cac->wlc->pub)) {
		FOREACHSCB(cac->wlc->scbstate, &scbiter, scb) {
			if (!SCB_WME(scb) || !SCB_ASSOCIATED(scb))
				continue;
			scb_acinfo = SCB_ACINFO(cac, scb);
			if (!scb_acinfo)
				continue;
			/* reset used time for each AC */
			for (ac = 0; ac < AC_COUNT; ac++) {
				if (!AC_BITMAP_TST(scb->bsscfg->wme->wme_admctl, ac))
					continue;
				cac_ac = &scb_acinfo->cac_ac[ac];

				if (cac_ac->used_time <= cac_ac->tot_medium_time) {
					cac_ac->used_time = 0;
				} else {
					/* carry over-used time to next tx window */
					cac_ac->used_time -= cac_ac->tot_medium_time;
				}
				wlc_cac_handle_inactivity(cac, ac, scb);
			}
		}
	}
#endif	/* AP */

	return BCME_OK;
}

/* function to update used time. return TRUE if run out of time */
bool
wlc_cac_update_used_time(wlc_cac_t *cac, int ac, int dur, struct scb *scb)
{
	wlc_cac_ac_t *cac_ac;
	struct cac_scb_acinfo *scb_acinfo;
	wlc_bsscfg_t *cfg;

	ASSERT(scb != NULL);

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* Check for admission control */
	if (!AC_BITMAP_TST(cfg->wme->wme_admctl, ac))
		return FALSE;

	scb_acinfo = SCB_ACINFO(cac, scb);
	cac_ac = &scb_acinfo->cac_ac[ac];

	/* use cached duration if dur is -1 */
	if (dur == -1)
		dur = cac_ac->cached_dur;
	else
		/* cache duration */
		cac_ac->cached_dur = dur;

	/* update used time */
	cac_ac->used_time += dur;



	if (cac_ac->tot_medium_time && (cac_ac->used_time > cac_ac->tot_medium_time)) {
		WL_CAC(("wl%d: used time(%d us) is over admitted time(%d us)\n",
			cac->wlc->pub->unit, cac_ac->used_time,
			cac_ac->tot_medium_time));

		return TRUE;
	}

	WL_CAC(("%s: AC[%d] = %d\n", __FUNCTION__, ac, cac_ac->used_time));

	return FALSE;
}


/*
 * Frame received, frame type FC_ACTION,
 * action_category DOT11_ACTION_NOTIFICATION
 */
void
wlc_frameaction_cac(wlc_bsscfg_t *cfg, uint action_id, wlc_cac_t *cac,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	struct scb *scb;

	if (!(scb = wlc_scbfind(cac->wlc, cfg, &hdr->sa))) {
		WL_ERROR(("%s: SCB not found\n", __FUNCTION__));
		return;
	}

	wlc_cac_action_frame(cac, action_id, hdr, body, body_len, scb);

}

/* handle received action frame, frame type == FC_ACTION,
 * action_category = DOT11_ACTION_NOTIFICATION
 */
void
wlc_cac_action_frame(wlc_cac_t *cac, uint action_id,
	struct dot11_management_header *hdr, uint8 *body, int body_len, struct scb *scb)
{
	struct cac_scb_acinfo *scb_acinfo;

	scb_acinfo = SCB_ACINFO(cac, scb);
	ASSERT(scb_acinfo);

	switch (action_id) {
		case WME_ADDTS_REQUEST:		/* AP only */
#ifdef	AP
			if (AP_ENAB(cac->wlc->pub))
				wlc_cac_process_addts_req(cac, body, body_len, scb);
#endif	/* AP */
			break;
		case WME_ADDTS_RESPONSE:	/* STA only */
			if (!AP_ENAB(cac->wlc->pub))
				wlc_cac_addts_resp(cac, body, body_len, scb);
			break;
		case WME_DELTS_REQUEST:		/* AP and STA */
			wlc_cac_delts_req(cac, body, body_len, scb);
			break;
		default:
			WL_ERROR(("FC_ACTION, Invalid WME action code 0x%x\n", action_id));
			break;
	}
}

/* calculate total medium time granted by the AP */
uint32
wlc_cac_medium_time_total(wlc_cac_t *cac, struct scb *scb)
{
	int ac;
	struct cac_scb_acinfo *scb_acinfo;
	uint32 total = 0;

	scb_acinfo = SCB_ACINFO(cac, scb);

	/* calculate total medium time */
	for (ac = 0; ac < AC_COUNT; ac++)
		total += scb_acinfo->cac_ac[ac].tot_medium_time;
	return total;
}


static int
wlc_cac_scb_acinfo_init(void *context, struct scb *scb)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	struct cac_scb_acinfo *scb_acinfo;
	uint32 i = 0;

	WL_CAC(("%s: Entering\n", __FUNCTION__));

	scb_acinfo = SCB_ACINFO(wlc->cac, scb);
	ASSERT(scb_acinfo);

	if (CAC_ENAB(wlc->pub) && !SCB_ISMULTI(scb) && !SCB_ISPERMANENT(scb) &&
		!ETHER_ISNULLADDR(scb->ea.octet)) {
		for (i = 0; i < AC_COUNT; i++) {
			scb_acinfo->cac_ac[i].admitted = FALSE;
			if ((wlc->band->bandtype == WLC_BAND_2G) &&
				(wlc->band->gmode == GMODE_LEGACY_B))
				scb_acinfo->cac_ac[i].nom_phy_rate = WLC_RATE_11M;
			else
				scb_acinfo->cac_ac[i].nom_phy_rate = WLC_RATE_24M;
		}
	}
	return 0;
}

static void
wlc_cac_scb_acinfo_deinit(void *context, struct scb *scb)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
#ifdef	AP
	tsentry_t *ts;
#endif
	struct cac_scb_acinfo *scb_acinfo;

	WL_CAC(("%s: Entering\n", __FUNCTION__));

	if (!(scb_acinfo = SCB_ACINFO(wlc->cac, scb)))
		return;

#ifdef	AP
	/* free the tsentryq if present */
	while ((ts = scb_acinfo->tsentryq)) {
		scb_acinfo->tsentryq = ts->next;
		MFREE(wlc->osh, ts, sizeof(tsentry_t));
	}
#endif
}

static void
wlc_cac_scb_acinfo_dump(void *context, struct scb *scb, struct bcmstrbuf *b)
{
	wlc_info_t *wlc = (wlc_info_t *)context;
	struct cac_scb_acinfo *scb_acinfo;
#ifdef BCMDBG
	wlc_cac_ac_t *ac;
	uint32 i = 0;
#endif	/* BCMDBG */

	scb_acinfo = SCB_ACINFO(wlc->cac, scb);
	ASSERT(scb_acinfo);

#ifdef	BCMDBG
	for (i = 0; i < AC_COUNT; i++) {
		ac = &scb_acinfo->cac_ac[i];
		if (ac->admitted) {
			bcm_bprintf(b, "%s: admt time 0x%x :used time 0x%x "
					":cached time 0x%x :nom phy rate 0x%x ac %d\n",
					aci_names[i],
					ac->tot_medium_time, ac->used_time,
					ac->cached_dur, ac->nom_phy_rate, i);
		}
	}
#endif /* BCMDBG */
}

#ifdef	BCMDBG
int
wlc_dump_cac(wlc_cac_t *cac, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "Use dump scb to get per scb related cac information\n");
	return 0;
}
#endif	/* BCMDBG */

/*
 * AP feature : Process the ADDTS request from the associated STA.
 */

#ifdef	AP
static int
wlc_cac_process_addts_req(wlc_cac_t *cac, uint8 *body, uint body_len, struct scb *scb)
{
	struct dot11_management_notification *mgmt_hdr;
	struct cac_scb_acinfo *scb_acinfo;
	uint8 tid, ac, dialog_token;
	uint16 tspec_medium_time = 0;
	tspec_t *ts = NULL;
	uint32	status = WME_ADMISSION_REFUSED;
	tspec_t ts_param;
	wlc_info_t *wlc = cac->wlc;
	wlc_bsscfg_t *cfg;
#ifdef	BCMDBG
	char buff[ETHER_ADDR_STR_LEN];

	WL_CAC(("%s: ADDTS Request from %s\n", __FUNCTION__, bcm_ether_ntoa(&scb->ea, buff)));
#endif

	if (body_len < (WME_TSPEC_LEN + DOT11_MGMT_NOTIFICATION_LEN))
		return -1;

	/* init the various pointers and length */
	mgmt_hdr = (struct dot11_management_notification *)body;
	body += DOT11_MGMT_NOTIFICATION_LEN;
	body_len -= DOT11_MGMT_NOTIFICATION_LEN;

	/* look for tspec ie */
	if (!(body = wlc_cac_ie_find(body, &body_len, DOT11_MNG_PROPR_ID, WME_TSPEC_LEN,
		(const int8 *)DOT11E_TSPEC_IE, DOT11E_TSPEC_OUI_TYPE_LEN)))
		return -1;


	/* extract the tspec and related info */
	ts = (tspec_t *)&body[TLV_BODY_OFF];
	tid = WLC_CAC_GET_TID(ts->tsinfo);
	dialog_token = mgmt_hdr->token;
	ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(ts->tsinfo));

	scb_acinfo = SCB_ACINFO(cac, scb);

	/* copy the received tspec to local tspec */
	bcopy(ts, &ts_param, sizeof(tspec_t));

	/* Check tspec params for validity */
	if (wlc_cac_is_tsparam_valid(cac, ts, ac, scb) == FALSE) {
		status = WME_INVALID_PARAMETERS;
		goto done;
	}

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* Is admission enabled on given ac ? */
	if (!AC_BITMAP_TST(cfg->wme->wme_admctl, ac)) {
		status = WME_ADMISSION_ACCEPTED;
		tspec_medium_time = 0;
	} else {
		/* calculate the required medium time for this tspec */
		tspec_medium_time = wlc_cac_generate_medium_time(cac, ts, scb);
		/* check if new tspec can be allowed ? */
		status = wlc_cac_check_tspec_for_admission(cac, ts, tspec_medium_time, scb);
	}

	if (status == WME_ADMISSION_ACCEPTED) {
		tsentry_t *new_ts_entry;

		/* mark the admitted status */
		scb_acinfo->cac_ac[ac].admitted = TRUE;

		/* check if TSPEC already exists for given tid */
		if (!(new_ts_entry = wlc_cac_tsentry_find(scb_acinfo->tsentryq, tid))) {

			if (!(new_ts_entry =
				(tsentry_t *)MALLOC(wlc->osh, sizeof(tsentry_t)))) {
				WL_ERROR(("wl%d: %s: out of memory %d bytes\n",
				          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				return -1;
			}

			/* change the TSPEC state */
			bcopy(ts, &new_ts_entry->tspec, sizeof(tspec_t));
			new_ts_entry->ts_state = CAC_STATE_TSPEC_ACCEPTED;
			wlc_cac_tsentry_append(cac, new_ts_entry, &scb_acinfo->tsentryq);
		} else {
			/* update the old entry */
			bcopy(ts, &new_ts_entry->tspec, sizeof(tspec_t));
			new_ts_entry->ts_state = CAC_STATE_TSPEC_ACCEPTED;
			/* restore the available medium time */
			cac->available_medium_time +=
				USEC_TO_USEC32((uint16)scb_acinfo->cac_ac[ac].tot_medium_time);
		}

		/* update the medium time for the tspec */
		new_ts_entry->tspec.medium_time = tspec_medium_time;
		ts_param.medium_time = new_ts_entry->tspec.medium_time;

		/* get the direction of traffic stream */
		if (WLC_CAC_GET_DIR(ts->tsinfo) == (TS_INFO_DOWNLINK >> TS_INFO_DIRECTION_SHIFT))
			ts_param.medium_time = 0;

		/* update the remote station cac information */
		scb_acinfo->cac_ac[ac].inactivity_limit = WLC_CAC_GET_INACTIVITY_INTERVAL(ts);
		scb_acinfo->cac_ac[ac].inactivity_tid = WLC_CAC_GET_TID(ts->tsinfo);
		scb_acinfo->cac_ac[ac].tot_medium_time = USEC32_TO_USEC(ts_param.medium_time);

		/* decrease the global medium time */
		cac->available_medium_time -= ts_param.medium_time;

		/* update the power save behavior */
		if (WLC_CAC_GET_PSB(ts->tsinfo) && cfg->wme->wme_apsd) {
			switch (WLC_CAC_GET_DIR(ts->tsinfo)) {
				case (TS_INFO_UPLINK >> TS_INFO_DIRECTION_SHIFT):
					WL_CAC(("AC trigger enabled\n"));
					AC_BITMAP_SET(scb->apsd.ac_trig, ac);
					AC_BITMAP_RESET(scb->apsd.ac_delv, ac);
					break;

				case (TS_INFO_DOWNLINK >> TS_INFO_DIRECTION_SHIFT):
					WL_CAC(("AC delivery enabled\n"));
					AC_BITMAP_SET(scb->apsd.ac_delv, ac);
					AC_BITMAP_RESET(scb->apsd.ac_trig, ac);
					break;

				case (TS_INFO_BIDIRECTIONAL >> TS_INFO_DIRECTION_SHIFT):
					WL_CAC(("AC trigger and delivery enabled\n"));
					AC_BITMAP_SET(scb->apsd.ac_trig, ac);
					AC_BITMAP_SET(scb->apsd.ac_delv, ac);
					break;
				}
		}

		WL_CAC(("Inactivity interval %d\n", scb_acinfo->cac_ac[ac].inactivity_limit));
		WL_CAC(("Total medium time %d\n", scb_acinfo->cac_ac[ac].tot_medium_time));
		WL_CAC(("Available medium time %d\n", cac->available_medium_time));
		WL_CAC(("Tspec medium time %d\n", ts_param.medium_time));
	}


done:
	/* send the addts response to the associated sta */
	return	wlc_cac_addts_resp_send(cac, &ts_param, scb, status, dialog_token);
}

/*
 * check to see if new TSPEC maintains the existing QoS
 *
 * Use TOKEN value in mgmt frame to check resend/new
 *
 */

static int
wlc_cac_check_tspec_for_admission(wlc_cac_t *cac, tspec_t *ts,
	uint16 tspec_medium_time, struct scb *scb)
{
	struct cac_scb_acinfo *scb_acinfo;
	uint8 ac = WME_PRIO2AC(WLC_CAC_GET_USER_PRIO(ts->tsinfo));


	scb_acinfo = SCB_ACINFO(cac, scb);


	/* Is admission enabled on given ac ? */
	if (!AC_BITMAP_TST(scb->bsscfg->wme->wme_admctl, ac))
		return WME_ADMISSION_REFUSED;

	ASSERT(tspec_medium_time);

	/* refuse admission if already a TSPEC is admitted */
	if (scb_acinfo->cac_ac[ac].admitted) {
		if (!(wlc_cac_tsentry_find(scb_acinfo->tsentryq, WLC_CAC_GET_TID(ts->tsinfo)))) {
			WL_CAC(("%s: Admission Refused\n", __FUNCTION__));
			return WME_ADMISSION_REFUSED;
		} else {
			/* check if tspec update is possible */
			if (tspec_medium_time >
					(cac->available_medium_time +
					 USEC_TO_USEC32(scb_acinfo->cac_ac[ac].tot_medium_time)))
				return WME_ADMISSION_REFUSED;
			else
				return WME_ADMISSION_ACCEPTED;
		}
	}

	/* early protection : reject if we cannot support the tspec */
	if (tspec_medium_time >= cac->available_medium_time) {
		WL_CAC(("%s: Not enough budget resource\n", __FUNCTION__));
		return WME_ADMISSION_REFUSED;
	}

	/* check if the max limit is reached for the given ac */
	return WME_ADMISSION_ACCEPTED;
}


/*
 * AP feature : Send ADDTS Response with the TSPEC
 * TODO : Prune this function.
 *
 */
static int
wlc_cac_addts_resp_send(wlc_cac_t *cac, tspec_t *ts, struct scb *scb,
	uint32 status, uint8 dialog_token)
{
	void *pkt;
#ifdef	BCMDBG
	char buff[ETHER_ADDR_STR_LEN];
	WL_CAC(("%s: ADDTS response (%s)\n", __FUNCTION__, bcm_ether_ntoa(&scb->ea, buff)));
#endif
	return wlc_cac_tspec_resp_send(cac, ts, WME_ADDTS_RESPONSE,
		status, dialog_token, &pkt, scb);
}

/*
 * AP feature : Send a action frame (tspec) response
 * containing a single WME TSPEC element
 */

static int
wlc_cac_tspec_resp_send(wlc_cac_t *cac, tspec_t *ts, uint8 action,
	int reason_code, uint8 dialog_token, void **pkt, struct scb *scb)
{
	uint len = 0;
	uint8 *body, *cp;
	wlc_info_t *wlc = cac->wlc;
	struct dot11_management_notification *mgmt_hdr;

	WL_CAC(("%s: Entering\n", __FUNCTION__));

	ASSERT(AP_ENAB(wlc->pub));	/* AP support only */
	ASSERT(scb);

	*pkt = NULL;

	ASSERT(wlc->pub->associated);

	if (!SCB_WME(scb)) {
		WL_CAC(("wl%d: %s : WME Not Enabled\n",
			wlc->pub->unit, __FUNCTION__));
		return BCME_NOT_WME_ASSOCIATION;
	}

/*
 * Management Action frame
 * -----------------------------------------------------------------------------------
 * | MAC hdr | Category code | Action code | Dialog token | Status | Elements | FCS
 * -----------------------------------------------------------------------------------
 *	24/30         1                 1             1            1                 4
 *
 * WMM Tspec element
 * -----------------------------------------------------------------------------------
 * ID | length | OUI | OUI type | OUI subtype | version | Tspec body
 * -----------------------------------------------------------------------------------
 * 1	1		3		1			1			1
 *
 */


	/* TSPEC Response frame is 4 bytes Management Notification frame
	 * follow by WME TSPEC element
	 */
	len = DOT11_MGMT_NOTIFICATION_LEN +
		(TLV_HDR_LEN + WME_TSPEC_LEN);

	/* Format the TSPEC ADDTS response frame */
	if ((*pkt = wlc_frame_get_mgmt(wlc, FC_ACTION, &scb->ea, &wlc->pub->cur_etheraddr,
	                               &wlc->cfg->BSSID, len, &body)) == NULL)
		return BCME_ERROR;

	/* update the mgmt notification header fields */
	mgmt_hdr = (struct dot11_management_notification *)body;
	mgmt_hdr->category = DOT11_ACTION_NOTIFICATION;
	mgmt_hdr->action = action;

	/* Update the dialog token from the received frame */
	mgmt_hdr->token = dialog_token;

	mgmt_hdr->status = (uint8)reason_code;
	cp = mgmt_hdr->data;

	/* Fill in TSPEC values */
	cp = wlc_write_info_elt(cp, DOT11_MNG_PROPR_ID, WME_TSPEC_LEN, ts);

	ASSERT((cp - body) == (int)len);
	/* adjust the packet length */
	PKTSETLEN(wlc->osh, *pkt, (DOT11_MGMT_HDR_LEN+len));


	if (wlc_sendmgmt(wlc, *pkt, SCB_WLCIFP(scb)->qi, scb))
		return 0;

	return BCME_ERROR;
}

/* get the new medium time
 *
 * TODO : Directly update the medium_time in TSPEC rather
 * than returning the result
 */

#define SBW_FP_PRECISION 13

static uint32
do_fpm(uint32 duration, uint32 sbw)
{
	ASSERT(sbw);
	return ((duration * sbw)/(2 << (SBW_FP_PRECISION - 1)));
}

/*
 * Recommended method to derive the medium time (wmmac spec, Appendix A.3)
 *
 * medium_time = surplus_bandwidth * pps * medium_time_per_frame_exchange
 * where
 * - pps = ceiling(mean_data_rate/8)/(nominal_msdu_size)
 * - medium_time_per_frame_exchange =
 *	duration(nominal_msdu_size, min_phy_rate) + SIFS + ACK time
 * - surplus_bandwidth, mean_data_rate, nominal_msdu_size, min_phy_rate
 *	are obtained from tspec
 *
 */

static uint16
wlc_cac_generate_medium_time(wlc_cac_t *cac, tspec_t *ts, struct scb *scb)
{

	uint32 medium_time;
	uint16 duration;
	uint32 min_phy_rate;
	uint8 preamble_type;

	ASSERT(ts);
	ASSERT(scb);

	preamble_type = ((scb->flags & SCB_SHORTPREAMBLE)? WLC_SHORT_PREAMBLE:
	                 WLC_LONG_PREAMBLE);

	/* convert bps to 500kbps */
	min_phy_rate = (ts->min_phy_rate / (500000));
	duration = wlc_wme_get_frame_medium_time(cac->wlc, min_phy_rate,
		preamble_type, ts->nom_msdu_size & TSPEC_NOM_MSDU_MASK);

	ASSERT(ts->nom_msdu_size & TSPEC_NOM_MSDU_MASK);
	medium_time = CEIL((ts->mean_data_rate/8),
		(ts->nom_msdu_size & TSPEC_NOM_MSDU_MASK)) * duration;

	/* scale it up */
	medium_time = do_fpm(medium_time, (uint32)ts->surplus_bw);

	/* convert to 32us units */
	medium_time = USEC_TO_USEC32(medium_time);
	return (uint16)medium_time;
}

static bool
wlc_cac_is_tsparam_valid(wlc_cac_t *cac, tspec_t *ts, int ac, struct scb *scb)
{
	uint min_phy_rate;
	uint8 i;
	bool found;
	wlc_bsscfg_t *cfg;

	/* convert bps to 500kbps */
	min_phy_rate = (ts->min_phy_rate / (500000));
	/* Must be in scb's association rateset */
	for (i = 0, found = FALSE; i < scb->rateset.count; i++) {
		if ((scb->rateset.rates[i] & RSPEC_RATE_MASK) == min_phy_rate) {
			found = TRUE;
			break;
		}
	}
	if (!found)
		return FALSE;

	/* Validate the rates in tspec */
	if ((ts->mean_data_rate == 0) ||
		((ts->nom_msdu_size & TSPEC_NOM_MSDU_MASK) == 0) ||
		(ts->surplus_bw == 0))
		return FALSE;

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* Reject if we do not support APSD */
	if (WLC_CAC_GET_PSB(ts->tsinfo) && !cfg->wme->wme_apsd)
		return FALSE;

	return TRUE;

}

/*
 * If the remote STA is inactive for more the interval we rip
 * the admitted TSPEC. An AP feature
 */
void wlc_cac_handle_inactivity(wlc_cac_t *cac, int ac,
	struct scb *scb)
{
	wlc_cac_ac_t *cac_ac;
	struct cac_scb_acinfo *scb_acinfo = NULL;
	scb_acinfo = SCB_ACINFO(cac, scb);
	cac_ac = &scb_acinfo->cac_ac[ac];

	if (AP_ENAB(cac->wlc->pub) && cac_ac->admitted &&
		cac_ac->inactivity_limit) {
		/* Increment the inactivity interval */
		cac_ac->inactivity_interval++;

		if (cac_ac->inactivity_interval >=
			cac_ac->inactivity_limit) {
			tspec_arg_t tspec_arg;
			tsentry_t * ts = wlc_cac_tsentry_find(scb_acinfo->tsentryq,
				scb_acinfo->cac_ac[ac].inactivity_tid);
			WL_CAC(("Inactivity limit reached for TID %d\n",
				scb_acinfo->cac_ac[ac].inactivity_tid));
			ASSERT(ts);
			ASSERT(ac < AC_COUNT);

			/* Format the tspec */
			tspec_arg.version = TSPEC_ARG_VERSION;
			tspec_arg.length = sizeof(tspec_arg_t) - (2 * sizeof(uint16));
			bcopy((uint8 *)&ts->tspec.tsinfo, (uint8 *)&tspec_arg.tsinfo,
				sizeof(struct tsinfo));

			/* Send DELTS request to the STA */
			wlc_cac_sta_delts(cac, &tspec_arg, scb, scb_acinfo->tsentryq);

		}
	}
}

#endif	/* AP */


bool
wlc_cac_is_traffic_admitted(wlc_cac_t *cac, int ac, struct scb *scb)
{
	struct cac_scb_acinfo *scb_acinfo;
	wlc_cac_ac_t *cac_ac;

	if ((!SCB_BSSCFG(scb)->BSS) || SCB_ISMULTI(scb) || !SCB_WME(scb))
		return TRUE;

	ASSERT(ac < AC_COUNT);
	scb_acinfo = SCB_ACINFO(cac, scb);
	cac_ac = &scb_acinfo->cac_ac[ac];

	if (!AC_BITMAP_TST(scb->bsscfg->wme->wme_admctl, ac))
		return TRUE;

	if (!scb_acinfo->cac_ac[ac].admitted) {
		WL_CAC(("%s: acm on for ac %d but admitted 0x%x\n",
			__FUNCTION__, ac, scb_acinfo->cac_ac[ac].admitted));
		return FALSE;
	}

	if ((cac_ac->tot_medium_time) &&
	    (cac_ac->used_time > cac_ac->tot_medium_time)) {
		WL_CAC(("%s: used_time 0x%x for ac %d, exceeds tot_medium_time 0x%x\n",
			__FUNCTION__, cac_ac->used_time, ac, cac_ac->tot_medium_time));
		return FALSE;
	}

	return TRUE;
}


/*
 * Reset the inactivity interval on any tx/rx activity
 */
void
wlc_cac_reset_inactivity_interval(wlc_cac_t *cac, int ac, struct scb *scb)
{
	struct cac_scb_acinfo *scb_acinfo;

	ASSERT(scb);
	ASSERT(ac < AC_COUNT);
	ASSERT(!SCB_ISMULTI(scb));

	scb_acinfo = SCB_ACINFO(cac, scb);
	scb_acinfo->cac_ac[ac].inactivity_interval = 0;
}

void
wlc_cac_on_join_bss(wlc_cac_t *cac, wlc_bsscfg_t *bsscfg, struct ether_addr *bssid, bool roam)
{

	/* update curr_bsscfg */
	cac->curr_bsscfg = wlc_bsscfg_find_by_bssid(cac->wlc, bssid);
	ASSERT(cac->curr_bsscfg != NULL);


	/* update curr_bssid */
	bcopy((uint8*)bssid, (uint8*)&cac->curr_bssid, ETHER_ADDR_LEN);
}

void
wlc_cac_on_leave_bss(wlc_cac_t *cac)
{
	tsentry_t *ts;

	while ((ts = cac->tsentryq)) {
		cac->tsentryq = ts->next;
		MFREE(cac->wlc->osh, ts, sizeof(tsentry_t));
	}
}

#ifdef	BCMDBG
void
wlc_print_tspec(tspec_t *ts)
{

	ASSERT(ts);

	if (ts->version != TSPEC_ARG_VERSION) {
		printf("\tIncorrect version of TSPEC struct: expected %d; got %d\n",
			TSPEC_ARG_VERSION, ts->version);
	}

	/* TODO : Change to bcm_bprintf */

	printf("TID %d \n", WLC_CAC_GET_TID(ts->tsinfo));
	printf("tsinfo 0x%02x 0x%02x 0x%02x\n", ts->tsinfo.octets[0],
		ts->tsinfo.octets[1], ts->tsinfo.octets[2]);
	printf("nom_msdu_size %d %s\n", (ts->nom_msdu_size & 0x7fff),
		(ts->nom_msdu_size & 0x8000) ? "fixed size" : "");
	printf("max_msdu_size %d\n", ts->max_msdu_size);
	printf("min_srv_interval %d\n", ts->min_srv_interval);
	printf("max_srv_interval %d\n", ts->max_srv_interval);
	printf("inactivity_interval %d\n", ts->inactivity_interval);
	printf("suspension_interval %d\n", ts->suspension_interval);
	printf("srv_start_time %d\n", ts->srv_start_time);
	printf("min_data_rate %d\n", ts->min_data_rate);
	printf("mean_data_rate %d\n", ts->mean_data_rate);
	printf("peak_data_rate %d\n", ts->peak_data_rate);
	printf("max_burst_size %d\n", ts->max_burst_size);
	printf("delay_bound %d\n", ts->delay_bound);
	printf("min_phy_rate %d\n", ts->min_phy_rate);
	printf("surplus_bw %d\n", ts->surplus_bw);
	printf("medium_time %d\n", ts->medium_time);
}
#endif /* DEBUG */


#if defined(WLFBT)
uint8 *
wlc_cac_write_ricreq(wlc_info_t *wlc, int *bufsize, int *ric_ie_count)
{
	wlc_cac_t *cac;
	tsentry_t *ts;
	uint8 *buf = NULL;

	cac = wlc->cac;
	if (CAC_ENAB(cac->wlc->pub) && cac != NULL) {
		int count = 0;
		for (ts = cac->tsentryq; ts != NULL; ts = ts->next) {
			count += 1;
		}

		/* TODO Include BlockACK RDIEs. Not required for VE tests yet. */
		if (count) {
			uint8 *cp;
			int totalsize = 0;
			dot11_rde_ie_t* rdptr;
			unsigned int i;

			/* Length of RDE, one of these must prefix each set of resource types. */
			totalsize += DOT11_MNG_RDE_IE_LEN * count;

			/* Add total required size for TSPEC IEs. */
			totalsize += (TLV_HDR_LEN + WME_TSPEC_LEN) * count;

			buf = MALLOC(wlc->osh, totalsize);
			if (buf == NULL) {
				WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
					wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				return buf;
			}
			cp = buf;
			*bufsize = totalsize;
			*ric_ie_count = count;

			/* Write out the RDE, followed by TSPEC entries. */
			for (i = 1, ts = cac->tsentryq; ts != NULL; i++, ts = ts->next) {
				tspec_t *copied_tspec;
				uint8 *bptr;
				const tspec_t *tspec = &ts->tspec;

				/* RDE. */
				rdptr = (dot11_rde_ie_t*)cp;
				rdptr->id = DOT11_MNG_RDE_ID;
				rdptr->length = DOT11_MNG_RDE_IE_LEN - TLV_HDR_LEN;
				rdptr->rde_id = i;
				rdptr->rd_count = 1;
				*ric_ie_count += 1;
				rdptr->status = htol16(0);	/* Always 0 for STA. */
				cp += DOT11_MNG_RDE_IE_LEN;

				/* TSPECs. */
				copied_tspec = (tspec_t*)(((bcm_tlv_t*)cp)->data);
				cp = wlc_write_info_elt(cp,
					DOT11_MNG_PROPR_ID, WME_TSPEC_LEN, tspec);
				/* make sure medium time is zero, avoiding alignment hassles */
				bptr = (uint8*)&copied_tspec->medium_time;
				*bptr++ = 0;
				*bptr = 0;
			}
		} else {
			WL_INFORM(("No TSPECs found.\n"));
		}
	} else {
		WL_INFORM(("CAC empty or not enabled.\n"));
	}

	return buf;
}


/*
 * For V-E fast transition any TSPECs are transferred from the previous AP to the new one.
 * So copy the CAC state.
 */
void wlc_cac_copy_state(wlc_cac_t *cac, struct scb *prev_scb, struct scb *scb)
{
	if (scb != prev_scb) {
		struct cac_scb_acinfo *prev_scb_acinfo = SCB_ACINFO(cac, prev_scb);
		struct cac_scb_acinfo *scb_acinfo = SCB_ACINFO(cac, scb);

		*scb_acinfo = *prev_scb_acinfo;
		bzero(prev_scb_acinfo, sizeof(*prev_scb_acinfo));
	}
}

#endif /* WLFBT */
