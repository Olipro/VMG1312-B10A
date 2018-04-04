/*
 * 802.11h CSA module source file
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_csa.c 375238 2012-12-18 03:22:39Z $
 */

#include <wlc_cfg.h>

#ifdef WLCSA

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_hw.h>
#include <wlc_alloc.h>
#include <wlc_ap.h>
#include <wlc_assoc.h>
#ifdef WLMCHAN
#include <wlc_mchan.h>
#endif
#include <wl_export.h>
#include <wlc_11h.h>
#include <wlc_csa.h>
#include <wlc_vht.h>

/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_CHANSPEC_SWITCH,	/* send CSA with chanspec as input */
};

static const bcm_iovar_t wlc_csa_iovars[] = {
#ifdef AP
	{"csa", IOV_CHANSPEC_SWITCH, (IOVF_SET_UP), IOVT_BUFFER, sizeof(wl_chan_switch_t)},
#endif
	{NULL, 0, 0, 0, 0}
};

/* CSA module info */
struct wlc_csa_info {
	wlc_info_t *wlc;
	int cfgh;			/* bsscfg cubby handle */
};

/* local functions */
/* module */
static int wlc_csa_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);

/* cubby */
static int wlc_csa_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_csa_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg);
#ifdef BCMDBG
static void wlc_csa_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_csa_bsscfg_dump NULL
#endif

/* up/down */
static void wlc_csa_bsscfg_up_down(void *ctx, bsscfg_up_down_event_data_t *evt_data);

/* timer */
static void wlc_csa_timeout(void *arg);

/* action frame */
static int wlc_send_action_switch_channel_ex(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg,
	const struct ether_addr *dst, wl_chan_switch_t *cs, uint8 action_id);
static void wlc_send_public_action_switch_channel(wlc_csa_info_t *csa, wlc_bsscfg_t *cfg,
  const struct ether_addr *dst);

/* channel switch */
#ifdef AP
static int wlc_csa_apply_channel_switch(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg);
static int wlc_csa_do_channel_switch(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg,
	chanspec_t chanspec, uint8 mode, uint8 count, uint8 reg_class, uint8 frame_type);
#endif

/* cubby structure and access macros */
typedef struct {
	struct wl_timer *csa_timer;	/* time to switch channel after last beacon */
	wl_chan_switch_t csa;
	struct {
		chanspec_t chanspec;		/* target chanspec */
		uint32	secs;			/* seconds until channel switch */
	} channel_sw;
} wlc_csa_t;
#define CSA_BSSCFG_CUBBY_LOC(csa, cfg) ((wlc_csa_t **)BSSCFG_CUBBY(cfg, (csa)->cfgh))
#define CSA_BSSCFG_CUBBY(csa, cfg) (*CSA_BSSCFG_CUBBY_LOC(csa, cfg))

/* module */
wlc_csa_info_t *
BCMATTACHFN(wlc_csa_attach)(wlc_info_t *wlc)
{
	wlc_csa_info_t *csam;

	if ((csam = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_csa_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	csam->wlc = wlc;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((csam->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(wlc_csa_t *),
	                wlc_csa_bsscfg_init, wlc_csa_bsscfg_deinit, wlc_csa_bsscfg_dump,
	                (void *)csam)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* register bsscfg up/down callbacks */
	if (wlc_bsscfg_updown_register(wlc, wlc_csa_bsscfg_up_down, csam) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* keep the module registration the last other add module unregistratin
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, wlc_csa_iovars, "csa", (void *)csam, wlc_csa_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	return csam;

	/* error handling */
fail:
	if (csam != NULL)
		MFREE(wlc->osh, csam, sizeof(wlc_csa_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_csa_detach)(wlc_csa_info_t *csam)
{
	wlc_info_t *wlc = csam->wlc;

	wlc_module_unregister(wlc->pub, "csa", csam);

	/* unregister bsscfg up/down callbacks */
	wlc_bsscfg_updown_unregister(wlc, wlc_csa_bsscfg_up_down, csam);

	MFREE(wlc->osh, csam, sizeof(wlc_csa_info_t));
}

static int
wlc_csa_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_csa_info_t *csam = (wlc_csa_info_t *)ctx;
	wlc_info_t *wlc = csam->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	bool bool_val2;
#ifdef AP
	wlc_bsscfg_t *apcfg;
	int idx;
#endif

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	bool_val = (int_val != 0) ? TRUE : FALSE;
	bool_val2 = (int_val2 != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);
	BCM_REFERENCE(bool_val2);

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {
#ifdef AP
	case IOV_SVAL(IOV_CHANSPEC_SWITCH): {
		wl_chan_switch_t *csa = (wl_chan_switch_t *)arg;

		if (wf_chspec_malformed(csa->chspec)) {
			err = BCME_BADCHAN;
			break;
		}

#ifdef BCMDBG
		if (BSSCFG_STA(bsscfg)) {
			if (!bsscfg->up) {
				err = BCME_NOTREADY;
				break;
			}

			wlc_send_action_switch_channel_ex(csam, bsscfg, &bsscfg->BSSID,
				csa, DOT11_SM_ACTION_CHANNEL_SWITCH);
			break;
		}
#endif /* BCMDBG */

		FOREACH_UP_AP(wlc, idx, apcfg) {
			err = wlc_csa_do_channel_switch(csam, apcfg,
				csa->chspec, csa->mode, csa->count, csa->reg, csa->frame_type);
		}

		break;
	}
#endif /* AP */
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* bsscfg cubby */
static int
wlc_csa_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_csa_info_t *csam = (wlc_csa_info_t *)ctx;
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t **pcsa = CSA_BSSCFG_CUBBY_LOC(csam, cfg);
	wlc_csa_t *csa;
	int err;

	if ((csa = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_csa_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		err = BCME_NOMEM;
		goto fail;
	}
	*pcsa = csa;

	/* init CSA timer */
	if ((csa->csa_timer =
	     wl_init_timer(wlc->wl, wlc_csa_timeout, (void *)cfg, "csa")) == NULL) {
		WL_ERROR(("wl%d: %s: wl_init_timer failed\n",
		          wlc->pub->unit, __FUNCTION__));
		err = BCME_NORESOURCE;
		goto fail;
	}

	return BCME_OK;

fail:
	if (csa != NULL)
		wlc_csa_bsscfg_deinit(ctx, cfg);
	return err;
}

static void
wlc_csa_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	wlc_csa_info_t *csam = (wlc_csa_info_t *)ctx;
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t **pcsa = CSA_BSSCFG_CUBBY_LOC(csam, cfg);
	wlc_csa_t *csa = *pcsa;

	if (csa == NULL) {
		WL_ERROR(("wl%d: %s: CSA info not found\n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* delete CSA timer */
	if (csa->csa_timer != NULL) {
		wl_free_timer(wlc->wl, csa->csa_timer);
		csa->csa_timer = NULL;
	}

	MFREE(wlc->osh, csa, sizeof(wlc_csa_t));
	*pcsa = NULL;
}

#ifdef BCMDBG
static void
wlc_csa_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_csa_info_t *csam = (wlc_csa_info_t *)ctx;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	/* CSA info */
	bcm_bprintf(b, "csa->csa_timer %p\n", csa->csa_timer);
	bcm_bprintf(b, "csa->csa.mode %d, csa->csa.count %d\n",
	            csa->csa.mode, csa->csa.count);
	bcm_bprintf(b, "csa->csa.chspec 0x%x, csa->csa.reg %d \n",
	            csa->csa.chspec, csa->csa.reg);
}
#endif /* BCMDBG */

static void
wlc_csa_bsscfg_up_down(void *ctx, bsscfg_up_down_event_data_t *evt_data)
{
	wlc_csa_info_t *csam = (wlc_csa_info_t *)ctx;
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, evt_data->bsscfg);

	/* Only process bsscfg down events. */
	if (!evt_data->up) {
		ASSERT(csa != NULL);

		/* cancel any csa timer */
		evt_data->callbacks_pending +=
		   (wl_del_timer(wlc->wl, csa->csa_timer) ? 0 : 1);
	}
}

static void
wlc_csa_timeout(void *arg)
{
	wlc_bsscfg_t *cfg = (wlc_bsscfg_t *)arg;
	wlc_info_t *wlc = cfg->wlc;
	wlc_csa_info_t *csam = wlc->csa;
#ifdef STA
	wlc_bsscfg_t *active_assoc_cfg = wlc->assoc_req[0];
#endif
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);
	struct scb_iter scbiter;
	struct scb *scb;
	bool switch_chnl = TRUE;

	ASSERT(csa != NULL);

	if (!wlc->pub->up)
		return;

	if (DEVICEREMOVED(wlc)) {
		WL_ERROR(("wl%d: %s: dead chip\n", wlc->pub->unit, __FUNCTION__));
		wl_down(wlc->wl);
		return;
	}

#ifdef STA
	if ((active_assoc_cfg != NULL) &&
	    ((active_assoc_cfg->assoc->state == AS_WAIT_FOR_AP_CSA) ||
	     (active_assoc_cfg->assoc->state == AS_WAIT_FOR_AP_CSA_ROAM_FAIL))) {
		wlc_11h_set_spect_state(wlc->m11h, active_assoc_cfg, NEED_TO_SWITCH_CHANNEL, 0);
		wlc_ap_mute(wlc, TRUE, active_assoc_cfg, -1);
		if (active_assoc_cfg->assoc->state == AS_WAIT_FOR_AP_CSA) {
			wlc_join_BSS(active_assoc_cfg,
			             wlc->join_targets->ptrs[wlc->join_targets_last]);
		}
		else {
			wlc_roam_complete(active_assoc_cfg, WLC_E_STATUS_FAIL,
			                  &active_assoc_cfg->BSSID,
			                  WLC_DOT11_BSSTYPE(active_assoc_cfg->target_bss->infra));
		}
		return;
	}
#endif /* STA */

	if (BSSCFG_AP(cfg) && (csa->csa.frame_type == CSA_UNICAST_ACTION_FRAME)) {
		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (!SCB_ISMULTI(scb) && SCB_ASSOCIATED(scb) &&
				(scb->psta_prim == NULL) &&
				(scb->dcs_relocation_state != WLMEDIA_DCS_RELOCATION_SUCCESS)) {
#if defined(BCMDBG)
				char eabuf[ETHER_ADDR_STR_LEN];
#endif
				WL_CHANINT(("dcs: csa ack pending from %s\n",
				            bcm_ether_ntoa(&scb->ea, eabuf)));
				switch_chnl = FALSE;
				break;
			}
		}
	}

	if (BSSCFG_AP(cfg) && (csa->csa.frame_type == CSA_UNICAST_ACTION_FRAME)) {
		if (switch_chnl) {
			/* time to switch channels... */
			wlc_do_chanswitch(cfg, csa->csa.chspec);
			wlc_11h_set_spect_state(wlc->m11h, cfg, NEED_TO_SWITCH_CHANNEL, 0);
		}
	}
	else if ((BSSCFG_AP(cfg) || csa->csa.count == 0 || csa->channel_sw.secs) &&
	    (wlc_11h_get_spect_state(wlc->m11h, cfg) & NEED_TO_SWITCH_CHANNEL)) {
		if (BSSCFG_STA(cfg))
			csa->channel_sw.secs = 0;
		/* time to switch channels... */
		wlc_do_chanswitch(cfg, csa->csa.chspec);
		wlc_11h_set_spect_state(wlc->m11h, cfg, NEED_TO_SWITCH_CHANNEL, 0);
	}

	if ((BSSCFG_AP(cfg) && (csa->csa.frame_type == CSA_UNICAST_ACTION_FRAME)) && !switch_chnl) {
		wlc_11h_set_spect_state(wlc->m11h, cfg, NEED_TO_SWITCH_CHANNEL, 0);
	}

	/* Send Channel Switch Indication to upper (OS) layer. */
	if (BSSCFG_STA(cfg))
		wlc_bss_mac_event(wlc, cfg, WLC_E_CSA_COMPLETE_IND, NULL,
			WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);
}

/* STA: Handle incoming Channel Switch Anouncement */
static void
wlc_csa_process_channel_switch(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = csam->wlc;
	wlc_bss_info_t *current_bss = cfg->current_bss;
	uint32 bi_us = (uint32)current_bss->beacon_period * DOT11_TU_TO_US;
	uint32 secs;
	wlcband_t *band;

#ifdef AP
	wlc_bsscfg_t *ap_bsscfg;
	int idx;
#endif /* AP */
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);
	band = wlc->bandstate[CHSPEC_IS2G(csa->csa.chspec) ? BAND_2G_INDEX : BAND_5G_INDEX];

	/* Convert a 40MHz AP channel to a 20MHz channel if we are not in NMODE or
	 * the locale does not allow 40MHz
	 * or the band is not configured for 40MHz operation
	 */
	if (band && CHSPEC_IS40(csa->csa.chspec) &&
	    (!N_ENAB(wlc->pub) ||
	     (wlc_channel_locale_flags_in_band(wlc->cmi, band->bandunit) & WLC_NO_40MHZ) ||
	     !WL_BW_CAP_40MHZ(band->bw_cap)))
		csa->csa.chspec = CH20MHZ_CHSPEC(wf_chspec_ctlchan((csa->csa.chspec)));

	if (!wlc_valid_chanspec_db(wlc->cmi, csa->csa.chspec)) {
		WL_ERROR(("wl%d: %s: Received invalid channel: %d\n",
			wlc->pub->unit, __FUNCTION__, CHSPEC_CHANNEL(csa->csa.chspec)));
		/* Received bogus, malformed or otherwise screwed CSA.
		 * Err on the side of caution and goto an active A band channel now
		 */
		csa->csa.chspec = wlc_next_chanspec(wlc->cmi,
			CH20MHZ_CHSPEC((CH_MAX_2G_CHANNEL+1)), CHAN_TYPE_CHATTY, 0);
		if (csa->csa.chspec == wlc->home_chanspec)
			csa->csa.chspec =
				wlc_next_chanspec(wlc->cmi, csa->csa.chspec, CHAN_TYPE_CHATTY, 0);
		csa->csa.count = 1;
	}

	secs = (bi_us * csa->csa.count)/1000000;

	if (csa->csa.mode && WL11H_ENAB(wlc) && !CHSPEC_IS2G(wlc->home_chanspec)) {
		wlc_set_quiet_chanspec(wlc->cmi, wlc->home_chanspec);
		if (WLC_BAND_PI_RADIO_CHANSPEC == wlc->home_chanspec) {
			WL_REGULATORY(("%s: Muting now\n", __FUNCTION__));
			wlc_mute(wlc, ON, 0);
		}
	}

	if (csa->channel_sw.secs != 0) {
		WL_REGULATORY(("%s: ignoring csa: mode %d, channel %d, count %d, secs %d\n",
			__FUNCTION__,
			csa->csa.mode, CHSPEC_CHANNEL(csa->csa.chspec), csa->csa.count, secs));
		return;
	}

	WL_REGULATORY(("%s: Recved CSA: mode %d, channel %d, count %d, secs %d\n",
	               __FUNCTION__,
	               csa->csa.mode, CHSPEC_CHANNEL(csa->csa.chspec), csa->csa.count, secs));

	/* If target < than 2 second and we need to be quiet until switch time
	 * then might as well switch now.
	 */
	if (secs < 2 && (csa->csa.mode || secs == 0)) {
		WL_REGULATORY(("%s: switch is only %d secs, switching now\n", __FUNCTION__, secs));
		wlc_do_chanswitch(cfg, csa->csa.chspec);

		/* Send Channel Switch Indication to upper (OS) layer. */
		wlc_bss_mac_event(wlc, cfg, WLC_E_CSA_COMPLETE_IND, NULL,
		                  WLC_E_STATUS_SUCCESS, 0, 0, 0, 0);

		return;
	}

	WL_REGULATORY(("%s: Scheduling channel switch in %d tbtts\n", __FUNCTION__,
	               csa->csa.count));

	/* Now we only use cfg->channel_sw.secs as a flag */
	csa->channel_sw.secs = secs;
	csa->channel_sw.chanspec = csa->csa.chspec;
	wlc_11h_set_spect_state(wlc->m11h, cfg, NEED_TO_SWITCH_CHANNEL, NEED_TO_SWITCH_CHANNEL);
	wl_add_timer(wlc->wl, csa->csa_timer, current_bss->beacon_period * csa->csa.count, 0);

#ifdef AP
	/* if we have an ap that is up and (sradar || ap_noradar_chan) enabled */
#ifdef WLMCHAN
	if (!MCHAN_ENAB(wlc->pub) ||
	    wlc_mchan_stago_is_disabled(wlc->mchan))
#endif
	{
	FOREACH_UP_AP(wlc, idx, ap_bsscfg) {
		bool special_ap = (WL11H_AP_ENAB(wlc) &&
		                   (BSSCFG_SRADAR_ENAB(ap_bsscfg) ||
		                    BSSCFG_AP_NORADAR_CHAN_ENAB(ap_bsscfg)));
		if (special_ap &&
		    (ap_bsscfg->current_bss->chanspec == cfg->current_bss->chanspec)) {
			wlc_csa_t *apcsa = CSA_BSSCFG_CUBBY(csam, ap_bsscfg);

			/* copy over the csa info */
			bcopy(&csa->csa, &apcsa->csa, sizeof(wl_chan_switch_t));
			/* decrement by 1 since this is for our next beacon */
			apcsa->csa.count--;
			/* send csa action frame and update bcn, prb rsp */
			wlc_csa_apply_channel_switch(csam, ap_bsscfg);
		}
	}
	}
#endif /* AP */
}

#ifdef WL11N
void
wlc_recv_public_csa_action(wlc_csa_info_t *csam, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	wlc_info_t *wlc = csam->wlc;
	wlc_bsscfg_t *cfg;
	struct dot11y_action_ext_csa *action_hdr;
	uint8 extch;
	wlc_csa_t *csa;

	if ((cfg = wlc_bsscfg_find_by_bssid(wlc, &hdr->bssid)) == NULL &&
	    (cfg = wlc_bsscfg_find_by_hwaddr(wlc, &hdr->da)) == NULL) {
#if defined(BCMDBG) || defined(BCMDBG_ERR)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif
		WL_ERROR(("wl%d: %s: Unable to find bsscfg for %s\n",
		          wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->sa, eabuf)));
		return;
	}

	if (!BSSCFG_STA(cfg)) {
		WL_ERROR(("wl%d.%d: %s: not a STA\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		return;
	}

	if (body_len != (sizeof(struct dot11y_action_ext_csa))) {
		WL_ERROR(("wl%d: %s: Invalid len %d != %d\n",	wlc->pub->unit, __FUNCTION__,
			body_len, (int)(TLV_HDR_LEN + sizeof(struct dot11_csa_body))));
		return;
	}

	action_hdr = (struct dot11y_action_ext_csa *)body;
	/* valid the IE in this action frame */
	WL_INFORM(("wl%d: %s: mode %d, reg %d, channel %d, count %d\n",
		wlc->pub->unit, __FUNCTION__, action_hdr->b.mode, action_hdr->b.reg,
		action_hdr->b.channel, action_hdr->b.count));

	csa = CSA_BSSCFG_CUBBY(csam, cfg);
	ASSERT(csa != NULL);

	csa->csa.mode = action_hdr->b.mode;
	csa->csa.count = action_hdr->b.count;
	extch = wlc_rclass_extch_get(wlc->cmi, action_hdr->b.reg);
	csa->csa.chspec = wlc_ht_chanspec(wlc, action_hdr->b.channel, extch);
	csa->csa.reg = action_hdr->b.reg;

#ifdef STA
	if (cfg->associated &&
	    bcmp(&hdr->bssid, &cfg->BSSID, ETHER_ADDR_LEN) == 0) {
		wlc_csa_process_channel_switch(csam, cfg);
	}
#endif /* STA */
}
#endif /* WL11N */

static bool
wlc_parse_csa_ie(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg, uint8 *params, int len)
{
	wlc_info_t *wlc = cfg->wlc;
	bcm_tlv_t *tag = (bcm_tlv_t*)params;
	uint8 *end = params + len;
	uint8 extch = DOT11_EXT_CH_NONE;
	uint8 type;
	bool csa_ie_found = FALSE;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	/* 11n & 11y csa takes president */
	if ((params < end) &&
	    (tag = bcm_parse_tlvs(params, (int)(end - params), DOT11_MNG_EXT_CSA_ID)) != NULL) {
		dot11_ext_csa_ie_t *csa_ie = (dot11_ext_csa_ie_t *)tag;
		if (tag->len >= DOT11_EXT_CSA_IE_LEN) {
			csa_ie_found = TRUE;
			csa->csa.mode = csa_ie->b.mode;
			csa->csa.count = csa_ie->b.count;
			extch = wlc_rclass_extch_get(wlc->cmi, csa_ie->b.reg);
			csa->csa.chspec = wlc_ht_chanspec(wlc, csa_ie->b.channel, extch);
			csa->csa.reg = csa_ie->b.reg;
		} else {
			WL_REGULATORY(("wl%d: %s: CSA IE length != 4\n",
				wlc->pub->unit, __FUNCTION__));
		}
	}

	if (!csa_ie_found && (params < end) &&
	    (tag = bcm_parse_tlvs(params, (int)(end - params),
	                          DOT11_MNG_CHANNEL_SWITCH_ID)) != NULL) {
		bool err = FALSE;
		dot11_chan_switch_ie_t *csa_ie = (dot11_chan_switch_ie_t *)tag;
		/* look for brcm extch IE first, if exist, use it,
		 * otherwise look for IEEE extch IE
		 */
		if (tag->len < DOT11_SWITCH_IE_LEN)
			err = TRUE;

		csa_ie_found = TRUE;
		type = BRCM_EXTCH_IE_TYPE;
		tag = (bcm_tlv_t *)wlc_find_vendor_ie((uchar *)tag, (uint)(end-(uint8 *)tag),
			BRCM_PROP_OUI, &type, sizeof(type));
		if (tag && tag->len == BRCM_EXTCH_IE_LEN)
			extch = ((dot11_brcm_extch_ie_t *)tag)->extch;
		else {
			tag = bcm_parse_tlvs(params, (int)(end - params),
				DOT11_MNG_EXT_CHANNEL_OFFSET);
			if (tag) {
				if (tag->len >= DOT11_EXTCH_IE_LEN) {
					extch = ((dot11_extch_ie_t *)tag)->extch;
					if (extch != DOT11_EXT_CH_LOWER &&
					    extch != DOT11_EXT_CH_UPPER &&
					    extch != DOT11_EXT_CH_NONE)
						extch = DOT11_EXT_CH_NONE;
				} else {
					WL_ERROR(("wl%d: wlc_parse_11h: extension channel offset"
						" len %d length too short\n",
						wlc->pub->unit, tag->len));
					csa_ie_found = FALSE;
					err = TRUE;
				}
			}
		}

		if (!err) {
			csa->csa.mode = csa_ie->mode;
			csa->csa.count = csa_ie->count;
			csa->csa.chspec = wlc_ht_chanspec(wlc, csa_ie->channel, extch);
			csa->csa.reg = 0;
		}
	}

	/* Should reset the csa.count back to zero if csa_ie not found */
	if (!csa_ie_found && csa->csa.count) {
		WL_REGULATORY(("wl%d.%d: %s: no csa ie found, reset csa.count %d to 0\n",
		          wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__, csa->csa.count));
		csa->csa.count = 0;
	}

	if (csa_ie_found)
		WL_REGULATORY(("%s: Found a CSA, count = %d\n", __FUNCTION__, csa->csa.count));

	return (csa_ie_found);
}

void
wlc_csa_recv_process_beacon(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg,
	uint8 *tag_params, int tag_params_len)
{
	if (wlc_parse_csa_ie(csam, cfg, tag_params, tag_params_len) == TRUE) {
		wlc_csa_process_channel_switch(csam, cfg);
	}
}

static uint8 *
wlc_write_csa_body(wl_chan_switch_t *cs, uint8 *cp)
{
	struct dot11_csa_body *b = (struct dot11_csa_body *)cp;

	b->mode = cs->mode;
	b->reg = cs->reg;
	b->channel = wf_chspec_ctlchan(cs->chspec);
	b->count = cs->count;
	cp += sizeof(struct dot11_csa_body);

	return cp;
}

static uint8 *
wlc_write_ext_csa_ie(wl_chan_switch_t *cs, uint8 *cp)
{
	dot11_ext_csa_ie_t *chan_switch_ie = (dot11_ext_csa_ie_t *)cp;

	chan_switch_ie->id = DOT11_MNG_EXT_CSA_ID;
	chan_switch_ie->len = DOT11_EXT_CSA_IE_LEN;
	cp += TLV_HDR_LEN;
	cp = wlc_write_csa_body(cs, cp);

	return cp;
}

static uint8 *
wlc_write_csa_ie(wl_chan_switch_t *cs, uint8 *cp, int buflen)
{
	dot11_chan_switch_ie_t *chan_switch_ie;

	/* perform buffer length check. */
	/* if not big enough, return buffer untouched */
	BUFLEN_CHECK_AND_RETURN((TLV_HDR_LEN + DOT11_SWITCH_IE_LEN), buflen, cp);

	chan_switch_ie = (dot11_chan_switch_ie_t *)cp;
	chan_switch_ie->id = DOT11_MNG_CHANNEL_SWITCH_ID;
	chan_switch_ie->len = DOT11_SWITCH_IE_LEN;
	chan_switch_ie->mode = cs->mode;
	chan_switch_ie->channel = wf_chspec_ctlchan(cs->chspec);
	chan_switch_ie->count = cs->count;
	cp += (TLV_HDR_LEN + DOT11_SWITCH_IE_LEN);

	return cp;
}

#ifdef WL11AC
static uint8 *
wlc_write_wide_bw_csa_ie(wl_chan_switch_t *cs, uint8 *cp, int buflen)
{
	dot11_wide_bw_chan_switch_ie_t *wide_bw_chan_switch_ie;
	uint8 center_chan;
	/* perform buffer length check. */
	/* if not big enough, return buffer untouched */
	BUFLEN_CHECK_AND_RETURN((TLV_HDR_LEN + DOT11_WIDE_BW_SWITCH_IE_LEN), buflen, cp);

	wide_bw_chan_switch_ie = (dot11_wide_bw_chan_switch_ie_t *)cp;
	wide_bw_chan_switch_ie->id = DOT11_MNG_WIDE_BW_CHANNEL_SWITCH_ID;
	wide_bw_chan_switch_ie->len = DOT11_WIDE_BW_SWITCH_IE_LEN;

	if (CHSPEC_IS80(cs->chspec))
		wide_bw_chan_switch_ie->channel_width = 1;
	else if (CHSPEC_IS160(cs->chspec))
		wide_bw_chan_switch_ie->channel_width = 2;
	else if (CHSPEC_IS8080(cs->chspec))
		wide_bw_chan_switch_ie->channel_width = 3;
	else
		wide_bw_chan_switch_ie->channel_width = 0;

	center_chan = CHSPEC_CHANNEL(cs->chspec) >> WL_CHANSPEC_CHAN_SHIFT;

	wide_bw_chan_switch_ie->center_frequency_segment_0 = center_chan;
	wide_bw_chan_switch_ie->center_frequency_segment_1 = 0;

	cp += (TLV_HDR_LEN + DOT11_WIDE_BW_SWITCH_IE_LEN);

	return cp;
}

static uint8 *
wlc_write_chan_switch_wrapper_ie(wlc_csa_info_t *csam, wl_chan_switch_t *cs,
	wlc_bsscfg_t *cfg, uint8 *cp, int buflen)
{
	dot11_chan_switch_wrapper_ie_t* chan_switch_wrapper;

	chan_switch_wrapper = (dot11_chan_switch_wrapper_ie_t *)cp;
	chan_switch_wrapper->id = DOT11_MNG_CHANNEL_SWITCH_WRAPPER_ID;
	chan_switch_wrapper->len = sizeof(dot11_vht_transmit_power_envelope_ie_t);
	cp += TLV_HDR_LEN;

	/* wb_csa_ie doesn't present in 20MHz channels */
	if (!CHSPEC_IS20(cs->chspec)) {
		cp = wlc_write_wide_bw_csa_ie(cs, cp, buflen);
		chan_switch_wrapper->len += sizeof(dot11_wide_bw_chan_switch_ie_t);
	}

	/* vht transmit power envelope IE length depends on channel width,
	 * update channel wrapper IE length
	 */
	if (CHSPEC_IS40(cs->chspec)) {
		chan_switch_wrapper->len += 1;
	}
	else if (CHSPEC_IS80(cs->chspec)) {
		chan_switch_wrapper->len += 2;
	}
	else if (CHSPEC_IS8080(cs->chspec) || CHSPEC_IS160(cs->chspec)) {
		chan_switch_wrapper->len += 3;
	}

	return wlc_write_vht_transmit_power_envelope_ie(csam->wlc, cs->chspec, cp, buflen);
}

uint8 *
wlc_csa_write_wide_bw_csa_ie(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg, uint8 *cp, int buflen)
{
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	return wlc_write_wide_bw_csa_ie(&csa->csa, cp, buflen);
}

uint8 *
wlc_csa_write_chan_switch_wrapper_ie(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg, uint8 *cp, int buflen)
{
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	return wlc_write_chan_switch_wrapper_ie(csam, &csa->csa, cfg, cp, buflen);
}
#endif /* WL11AC */

static uint8* wlc_write_extch_ie(chanspec_t chspec, uint8 *cp, int buflen)
{
	dot11_extch_ie_t *extch_ie = (dot11_extch_ie_t *)cp;

	/* length check */
	/* if buffer too small, return untouched buffer */
	BUFLEN_CHECK_AND_RETURN((TLV_HDR_LEN + DOT11_EXTCH_IE_LEN), buflen, cp);

	extch_ie->id = DOT11_MNG_EXT_CHANNEL_OFFSET;
	extch_ie->len = DOT11_EXTCH_IE_LEN;
	if (!CHSPEC_IS40(chspec))
		extch_ie->extch = DOT11_EXT_CH_NONE;
	else
		extch_ie->extch = CHSPEC_SB_UPPER(chspec) ? DOT11_EXT_CH_LOWER : DOT11_EXT_CH_UPPER;
	cp += (TLV_HDR_LEN + DOT11_EXTCH_IE_LEN);

	return cp;
}

uint8 *
wlc_csa_write_ext_csa_ie(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg, uint8 *cp)
{
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	return wlc_write_ext_csa_ie(&csa->csa, cp);
}

uint8 *
wlc_csa_write_csa_ie(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg, uint8 *cp, int buflen)
{
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	return wlc_write_csa_ie(&csa->csa, cp, buflen);
}


uint8 *
wlc_csa_write_extch_ie(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg, uint8 *cp, int buflen)
{
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	return wlc_write_extch_ie(csa->csa.chspec, cp, buflen);
}

void
wlc_recv_csa_action(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	wlc_info_t *wlc = csam->wlc;
	struct dot11_action_frmhdr *action_hdr;
	dot11_chan_switch_ie_t *csa_ie;
	bcm_tlv_t *ext_ie;
	bcm_tlv_t *ies;
	uint ies_len;
	uint8 extch = DOT11_EXT_CH_NONE;

	ASSERT(cfg != NULL);

	action_hdr = (struct dot11_action_frmhdr *)body;

	ies = (bcm_tlv_t*)action_hdr->data;
	ies_len = body_len - DOT11_ACTION_HDR_LEN;

	csa_ie = (dot11_chan_switch_ie_t*)
	        bcm_parse_tlvs(ies, ies_len, DOT11_MNG_CHANNEL_SWITCH_ID);

	if (csa_ie == NULL || csa_ie->len < DOT11_SWITCH_IE_LEN) {
		WL_REGULATORY(("wl%d:%s: Bad CSA Spectrum Mngmt Action frame\n",
		               wlc->pub->unit, __FUNCTION__));
		WLCNTINCR(wlc->pub->_cnt->rxbadproto);
		wlc_send_action_err(wlc, hdr, body, body_len);
		return;
	}

	/* check if we have an extension channel ie */
	if (N_ENAB(wlc->pub)) {
		/* Check for 11n spec IE first */
		ext_ie = bcm_parse_tlvs(ies, ies_len, DOT11_MNG_EXT_CHANNEL_OFFSET);
		if (ext_ie != NULL &&
		    ext_ie->len == DOT11_EXTCH_IE_LEN) {
			extch = ((dot11_extch_ie_t *)ext_ie)->extch;
		} else {
			uint8 extch_subtype = BRCM_EXTCH_IE_TYPE;

			/* Check for BRCM OUI format */
			ext_ie = wlc_find_vendor_ie(ies, ies_len, BRCM_PROP_OUI,
			                            &extch_subtype, 1);
			if (ext_ie != NULL &&
			    ext_ie->len == BRCM_EXTCH_IE_LEN) {
				extch = ((dot11_brcm_extch_ie_t *)ext_ie)->extch;
			}
		}
	}

	WL_REGULATORY(("wl%d: wlc_recv_csa_action: mode %d, channel %d, count %d, extension %d\n",
	               wlc->pub->unit, csa_ie->mode, csa_ie->channel, csa_ie->count, extch));
	BCM_REFERENCE(extch);

#if defined(BCMDBG) && defined(AP)
	if (BSSCFG_AP(cfg)) {
		chanspec_t chspec;
		if (extch == DOT11_EXT_CH_NONE) {
			chspec = CH20MHZ_CHSPEC(csa_ie->channel);
		} else if (extch == DOT11_EXT_CH_UPPER) {
			chspec = CH40MHZ_CHSPEC(csa_ie->channel, WL_CHANSPEC_CTL_SB_UPPER);
		} else {
			chspec = CH40MHZ_CHSPEC(csa_ie->channel, WL_CHANSPEC_CTL_SB_LOWER);
		}

		wlc_csa_do_channel_switch(csam, cfg, chspec, csa_ie->mode, csa_ie->count, 0,
			CSA_BROADCAST_ACTION_FRAME);
	}
#endif

	return;
}

void
wlc_recv_ext_csa_action(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	wlc_info_t *wlc = csam->wlc;
	struct dot11_action_ext_csa *action_hdr;
	dot11_ext_csa_ie_t *req_ie;

	(void)wlc;

	ASSERT(cfg != NULL);

	action_hdr = (struct dot11_action_ext_csa *)body;
	req_ie = &action_hdr->chan_switch_ie;

	/* valid the IE in this action frame */
	if (N_ENAB(wlc->pub) &&
	    body_len >= (int)(sizeof(struct dot11_action_ext_csa))) {
		if (req_ie->id == DOT11_MNG_EXT_CSA_ID) {
			WL_REGULATORY(("wl%d: wlc_recv_ext_csa_action: mode %d, reg %d, channel %d,"
				"count %d\n", wlc->pub->unit, req_ie->b.mode,
				req_ie->b.reg, req_ie->b.channel, req_ie->b.count));
			return;
		}
	}
	WL_REGULATORY(("wl%d: wlc_recv_ext_csa_action: unknown ID %d", wlc->pub->unit, req_ie->id));
}

bool
wlc_csa_quiet_mode(wlc_csa_info_t *csam, uint8 *tag, uint tag_len)
{
	bool quiet = FALSE;
	dot11_chan_switch_ie_t *csa_ie;

	if (!tag || tag_len <= DOT11_BCN_PRB_LEN)
		return quiet;

	tag_len = tag_len - DOT11_BCN_PRB_LEN;
	tag = tag + DOT11_BCN_PRB_LEN;

	csa_ie = (dot11_chan_switch_ie_t *)
	        bcm_parse_tlvs(tag, tag_len, DOT11_MNG_CHANNEL_SWITCH_ID);
	if (csa_ie && csa_ie->mode)
		quiet = TRUE;

	return quiet;
}

void
wlc_csa_reset_all(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	if (csa->channel_sw.secs != 0) {
		wl_del_timer(wlc->wl, csa->csa_timer);
		csa->channel_sw.secs = 0;
	}
	csa->csa.count = 0;
}

#ifdef AP
static int
wlc_csa_do_channel_switch(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg,
	chanspec_t chanspec, uint8 mode, uint8 count, uint8 reg_class, uint8 frame_type)
{
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);
	int bcmerror;

	ASSERT(csa != NULL);

	if (!BSSCFG_AP(cfg))
		return BCME_NOTAP;

	if (!cfg->up) {
		return BCME_NOTREADY;
	}

	ASSERT(!wf_chspec_malformed(chanspec));

	if (!wlc_valid_chanspec_db(wlc->cmi, chanspec)) {
		return BCME_BADCHAN;
	}

	csa->csa.mode = (mode != 0) ? 1 : 0;
	csa->csa.count = count;
	csa->csa.chspec = chanspec;
	if (reg_class != 0)
		csa->csa.reg = reg_class;
	else
		csa->csa.reg = wlc_get_regclass(wlc->cmi, chanspec);

	csa->csa.frame_type = (frame_type != CSA_UNICAST_ACTION_FRAME) ?
	  CSA_BROADCAST_ACTION_FRAME : CSA_UNICAST_ACTION_FRAME;

	/* and update beacon and probe response for the specified bsscfg */
	bcmerror = wlc_csa_apply_channel_switch(csam, cfg);

	/* adds NEED_TO_UPDATE_BCN to wlc->spect_state, send csa action frames, */
	if (!bcmerror)
		wlc_11h_set_spect_state(wlc->m11h, cfg, NEED_TO_SWITCH_CHANNEL,
		                        NEED_TO_SWITCH_CHANNEL);

	return BCME_OK;
}

/* This function applies the parameters of channel switch set else where */
/* It is assumed that the csa parameters have been set else where */
/* We send out the necessary csa action frames */
/* We mark wlc->spect_state with NEED_TO_UPDATE_BCN flag */
/* We only want the beacons and probe responses updated for the specified bss */
/* The actual channel switch will be initiated else where */
static int
wlc_csa_apply_channel_switch(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = csam->wlc;
	int bcmerror;

	if (!BSSCFG_AP(cfg))
		return BCME_NOTAP;

	if (!cfg->up) {
		return BCME_NOTREADY;
	}

	bcmerror = wlc_send_action_switch_channel(csam, cfg);
	if (bcmerror == BCME_OK) {
		wlc_11h_set_spect_state(wlc->m11h, cfg, NEED_TO_UPDATE_BCN, NEED_TO_UPDATE_BCN);
		wlc_bss_update_beacon(wlc, cfg);
		wlc_bss_update_probe_resp(wlc, cfg, TRUE);
	}

	return bcmerror;
}

void
wlc_csa_do_switch(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg, chanspec_t chspec)
{
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	/* Stop the current queue with flow control */
	wlc_txflowcontrol_override(wlc, cfg->wlcif->qi, ON, TXQ_STOP_FOR_PKT_DRAIN);

	csa->csa.mode = DOT11_CSA_MODE_ADVISORY;
	csa->csa.chspec = chspec;
	csa->csa.reg = wlc_get_regclass(wlc->cmi, chspec);
	csa->csa.count = cfg->current_bss->dtim_period + 1;

	wlc_csa_apply_channel_switch(csam, cfg);
}
#endif /* AP */

static int
wlc_send_action_switch_channel_ex(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg,
	const struct ether_addr *dst, wl_chan_switch_t *csa, uint8 action_id)
{
	wlc_info_t *wlc = csam->wlc;
	void *p;
	uint8* pbody;
	uint8* cp;
	uint body_len;
	struct dot11_action_frmhdr *action_hdr;
	bool ext_ie = FALSE;
	uint8 *bufend;
	bool ret;

	/* Action switch_channel */
	body_len = DOT11_ACTION_HDR_LEN + TLV_HDR_LEN;
	if (action_id == DOT11_SM_ACTION_CHANNEL_SWITCH) {
		body_len += DOT11_SWITCH_IE_LEN;
		/* account for extension channel IE if operate in 40MHz */
		if (N_ENAB(wlc->pub) && (CHSPEC_IS40(csa->chspec))) {
			body_len += (TLV_HDR_LEN + DOT11_EXTCH_IE_LEN);
			ext_ie = TRUE;
		}
	} else
		body_len += DOT11_EXT_CSA_IE_LEN;

	if ((p = wlc_frame_get_mgmt(wlc, FC_ACTION, dst, &cfg->cur_etheraddr,
	                            &cfg->BSSID, body_len, &pbody)) == NULL) {
		return BCME_NOMEM;
	}

	/* mark the end of buffer */
	bufend = pbody + body_len;

	action_hdr = (struct dot11_action_frmhdr *)pbody;
	action_hdr->category = DOT11_ACTION_CAT_SPECT_MNG;
	action_hdr->action = action_id;

	if (action_id == DOT11_SM_ACTION_CHANNEL_SWITCH) {
		cp = wlc_write_csa_ie(csa, action_hdr->data, (body_len - DOT11_ACTION_HDR_LEN));
		if (ext_ie)
			cp = wlc_write_extch_ie(csa->chspec, cp, BUFLEN(cp, bufend));
	} else
		cp = wlc_write_ext_csa_ie(csa, action_hdr->data);

	ASSERT((cp - pbody) == (int)body_len);

	WL_COEX(("wl%d: %s: Send CSA (id=%d) Action frame\n",
		wlc->pub->unit, __FUNCTION__, action_id));
	ret = wlc_sendmgmt(wlc, p, cfg->wlcif->qi, NULL);

	return (ret ? BCME_OK : BCME_ERROR);
}

static void
wlc_send_public_action_switch_channel(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg,
  const struct ether_addr *dst)
{
	wlc_info_t *wlc = csam->wlc;
	void *p;
	uint8* pbody;
	uint8* cp;
	uint body_len;
	struct dot11_action_frmhdr *action_hdr;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	/* Action switch_channel */
	body_len = sizeof(struct dot11y_action_ext_csa);

	if ((p = wlc_frame_get_mgmt(wlc, FC_ACTION, dst, &cfg->cur_etheraddr,
	                            &cfg->BSSID, body_len, &pbody)) == NULL) {
		return;
	}

	action_hdr = (struct dot11_action_frmhdr *)pbody;
	action_hdr->category = DOT11_ACTION_CAT_PUBLIC;
	action_hdr->action = DOT11_PUB_ACTION_CHANNEL_SWITCH;

	cp = wlc_write_csa_body(&csa->csa, action_hdr->data);

	ASSERT((cp - pbody) == (int)body_len);
	BCM_REFERENCE(cp);

	WL_COEX(("wl%d: %s: Send CSA Public Action frame\n", wlc->pub->unit, __FUNCTION__));
	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, NULL);
}

int
wlc_send_action_switch_channel(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);
	int bcmerror = BCME_ERROR;

	(void)wlc;

	ASSERT(csa != NULL);
	if (csa->csa.frame_type == CSA_UNICAST_ACTION_FRAME) {
		struct scb *scb;
		struct scb_iter scbiter;

		FOREACHSCB(wlc->scbstate, &scbiter, scb) {
			if (SCB_ASSOCIATED(scb) && !SCB_ISMULTI(scb) &&
				(scb->psta_prim == NULL)) {
				bcmerror = wlc_send_action_switch_channel_ex(csam,
				                                             scb->bsscfg,
				                                             &scb->ea,
					&csa->csa, DOT11_SM_ACTION_CHANNEL_SWITCH);

				if (bcmerror < 0) {
					break;
				} else {
					if (N_ENAB(wlc->pub)) {
						bcmerror = wlc_send_action_switch_channel_ex(csam,
						                                        scb->bsscfg,
						                                        &scb->ea,
							&csa->csa, DOT11_SM_ACTION_EXT_CSA);
						if (!bcmerror) {
							wlc_send_public_action_switch_channel(csam,
							  cfg, &scb->ea);
							scb->dcs_relocation_state =
							  WLMEDIA_DCS_RELOCATION_PENDING;
						}
					}
				}
			} else {
				scb->dcs_relocation_state = WLMEDIA_DCS_RELOCATION_NONE;
			}
		}
	} else {
		bcmerror = wlc_send_action_switch_channel_ex(csam, cfg, &ether_bcast,
			&csa->csa, DOT11_SM_ACTION_CHANNEL_SWITCH);
		if (!bcmerror && N_ENAB(wlc->pub))
			bcmerror = wlc_send_action_switch_channel_ex(csam, cfg, &ether_bcast,
				&csa->csa, DOT11_SM_ACTION_EXT_CSA);
		wlc_send_public_action_switch_channel(csam, cfg, &ether_bcast);
	}

	return bcmerror;
}

#define CSA_PRE_SWITCH_TIME	10	/* pre-csa switch time, in unit of ms */

void
wlc_csa_count_down(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);
	wlc_bss_info_t *current_bss = cfg->current_bss;

	ASSERT(csa != NULL);

	/* Resume tx */
	if (csa->csa.count == 0) {
		wlc->block_datafifo &= ~DATA_BLOCK_QUIET;
		return;
	}

	/* to updated channel switch count of csa ie. */
	if (--csa->csa.count == 0) {
		/* set up time to switch channel after beacon is sent */
		wl_del_timer(wlc->wl, csa->csa_timer);
		wl_add_timer(wlc->wl, csa->csa_timer,
		             (current_bss->beacon_period < CSA_PRE_SWITCH_TIME ?
		              current_bss->beacon_period : CSA_PRE_SWITCH_TIME), 0);
	}
}

void
wlc_csa_do_csa(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg, wl_chan_switch_t *cs, bool docs)
{
	wlc_info_t *wlc = csam->wlc;
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	csa->csa = *cs;

	/* need to send legacy CSA and new 11n Ext-CSA if is n-enabled */
	wlc_send_action_switch_channel(csam, cfg);

	if (docs) {
		wlc_do_chanswitch(cfg, cs->chspec);
	}
	else {
		/* dpc handles NEED_TO_UPDATE_BCN, NEED_TO_SWITCH_CHANNEL */
		wlc_11h_set_spect_state(wlc->m11h, cfg,
		                         NEED_TO_UPDATE_BCN | NEED_TO_SWITCH_CHANNEL,
		                         NEED_TO_UPDATE_BCN | NEED_TO_SWITCH_CHANNEL);
		/* block data traffic but allow control */
		wlc->block_datafifo |= DATA_BLOCK_QUIET;
	}

	wlc_bss_update_beacon(wlc, cfg);
	wlc_bss_update_probe_resp(wlc, cfg, TRUE);
}

/* accessor functions */
uint8
wlc_csa_get_csa_count(wlc_csa_info_t *csam, wlc_bsscfg_t *cfg)
{
	wlc_csa_t *csa = CSA_BSSCFG_CUBBY(csam, cfg);

	ASSERT(csa != NULL);

	return csa->csa.count;
}

#endif /* WLCSA */
