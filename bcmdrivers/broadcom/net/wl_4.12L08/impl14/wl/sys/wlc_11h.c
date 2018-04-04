/*
 * 802.11h module source file (top level and spectrum management)
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
 * $Id: wlc_11h.c 300516 2011-12-04 17:39:44Z $
 */


#include <wlc_cfg.h>

#ifdef WL11H

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
#include <wlc_alloc.h>
#include <wlc_scb.h>
#include <wlc_tpc.h>
#include <wlc_csa.h>
#include <wlc_quiet.h>
#include <wlc_11h.h>

/* IOVar table */
enum {
	IOV_MEASURE,
	IOV_LAST
};

static const bcm_iovar_t wlc_11h_iovars[] = {
	{"measure", IOV_MEASURE, (0), IOVT_BUFFER, sizeof(uint32)+sizeof(struct ether_addr)},
	{NULL, 0, 0, 0, 0}
};

/* ioctl table */
static const wlc_ioctl_cmd_t wlc_11h_ioctls[] = {
	{WLC_SET_SPECT_MANAGMENT, WLC_IOCF_DRIVER_DOWN, sizeof(int)},
	{WLC_GET_SPECT_MANAGMENT, 0, sizeof(int)},
	{WLC_MEASURE_REQUEST, 0, sizeof(uint32)+sizeof(struct ether_addr)}
};

/* 11h module info */
struct wlc_11h_info {
	wlc_info_t *wlc;
	int cfgh;			/* bsscfg cubby handle */
	uint _spect_management;
};

/* local functions */
/* module */
static int wlc_11h_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
#ifdef BCMDBG
static int wlc_11h_dump(void *ctx, struct bcmstrbuf *b);
#endif
static int wlc_11h_doioctl(void *ctx, int cmd, void *arg, int len, struct wlc_if *wlcif);

/* cubby */
static int wlc_11h_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg);
static void wlc_11h_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg);
#ifdef BCMDBG
static void wlc_11h_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_11h_bsscfg_dump NULL
#endif

#ifdef BCMDBG
static void wlc_11h_ibss(wlc_info_t *wlc, dot11_ibss_dfs_t *tag);
#endif

/* spectrum management */
static void wlc_recv_measure_request(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, uint8 *body, int body_len);
static void wlc_recv_measure_report(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, uint8 *body, int body_len);
static void wlc_send_measure_request(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct ether_addr *da,
	uint8 measure_type);
static void wlc_send_measure_report(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct ether_addr *da,
	uint8 token, uint8 *report, uint report_len);
#ifdef BCMDBG
static void wlc_print_measure_req_rep(wlc_info_t *wlc, struct dot11_management_header *hdr,
	uint8 *body, int body_len);
#endif

typedef struct {
	uint8 spect_state;
} wlc_11h_t;
#define IEEE11H_BSSCFG_CUBBY(m11h, cfg) ((wlc_11h_t *)BSSCFG_CUBBY(cfg, (m11h)->cfgh))

wlc_11h_info_t *
BCMATTACHFN(wlc_11h_attach)(wlc_info_t *wlc)
{
	wlc_11h_info_t *m11h;

	if ((m11h = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_11h_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	m11h->wlc = wlc;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((m11h->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(wlc_11h_t),
	                wlc_11h_bsscfg_init, wlc_11h_bsscfg_deinit, wlc_11h_bsscfg_dump,
	                m11h)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

#ifdef BCMDBG
	if (wlc_dump_register(wlc->pub, "11h", wlc_11h_dump, m11h) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif

	if (wlc_module_register(wlc->pub, wlc_11h_iovars, "11h", m11h, wlc_11h_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	if (wlc_module_add_ioctl_fn(wlc->pub, m11h, wlc_11h_doioctl,
	                            ARRAYSIZE(wlc_11h_ioctls), wlc_11h_ioctls) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_add_ioctl_fn() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	m11h->_spect_management = SPECT_MNGMT_LOOSE_11H;	/* 802.11h */
	wlc->pub->_11h = TRUE;

	return m11h;

	/* error handling */
fail:
	wlc_11h_detach(m11h);
	return NULL;
}

void
BCMATTACHFN(wlc_11h_detach)(wlc_11h_info_t *m11h)
{
	wlc_info_t *wlc;

	if (m11h == NULL)
		return;

	wlc = m11h->wlc;

	wlc_module_remove_ioctl_fn(wlc->pub, m11h);
	wlc_module_unregister(wlc->pub, "11h", m11h);

	MFREE(wlc->osh, m11h, sizeof(wlc_11h_info_t));
}

static int
wlc_11h_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_11h_info_t *m11h = (wlc_11h_info_t *)ctx;
	wlc_info_t *wlc = m11h->wlc;
	wlc_bsscfg_t *bsscfg;
	int err = 0;
	int32 int_val = 0;
	int32 *ret_int_ptr;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_SVAL(IOV_MEASURE): {
		struct ether_addr *ea = (struct ether_addr *)((uint32 *)arg + 1);
		switch (int_val) {
		case WLC_MEASURE_TPC:
			wlc_send_tpc_request(wlc->tpc, bsscfg, ea);
			break;

		case WLC_MEASURE_CHANNEL_BASIC:
			wlc_send_measure_request(wlc, bsscfg, ea, DOT11_MEASURE_TYPE_BASIC);
			break;

		case WLC_MEASURE_CHANNEL_CCA:
			wlc_send_measure_request(wlc, bsscfg, ea, DOT11_MEASURE_TYPE_CCA);
			break;

		case WLC_MEASURE_CHANNEL_RPI:
			wlc_send_measure_request(wlc, bsscfg, ea, DOT11_MEASURE_TYPE_RPI);
			break;

		default:
			err = BCME_RANGE; /* unknown measurement type */
			break;
		}
		break;
	}

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static int
wlc_11h_doioctl(void *ctx, int cmd, void *arg, int len, struct wlc_if *wlcif)
{
	wlc_11h_info_t *m11h = (wlc_11h_info_t *)ctx;
	wlc_info_t *wlc = m11h->wlc;
	int val = 0, *pval;
	int err = BCME_OK;

	/* default argument is generic integer */
	pval = (int *)arg;

	/* This will prevent the misaligned access */
	if (pval && (uint32)len >= sizeof(val))
		bcopy(pval, &val, sizeof(val));

	switch (cmd) {
	case WLC_SET_SPECT_MANAGMENT:
		err = wlc_11h_set_spect(wlc->m11h, (uint)val);
		break;

	case WLC_GET_SPECT_MANAGMENT: {
		uint spect = SPECT_MNGMT_OFF;
		spect = wlc_11h_get_spect(wlc->m11h);
		if (spect == SPECT_MNGMT_OFF &&
		    WL11D_ENAB(wlc))
			spect = SPECT_MNGMT_STRICT_11D;
		*pval = (int)spect;
		break;
	}

	case WLC_MEASURE_REQUEST:
		err = wlc_iovar_op(wlc, "measure", NULL, 0, arg, len, IOV_SET, wlcif);
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

#ifdef BCMDBG
static int
wlc_11h_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_11h_info_t *m11h = (wlc_11h_info_t *)ctx;

	bcm_bprintf(b, "spect_mngmt:%d\n", m11h->_spect_management);

	return BCME_OK;
}
#endif /* BCMDBG */

/* bsscfg cubby */
static int
wlc_11h_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	return BCME_OK;
}

static void
wlc_11h_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
}

#ifdef BCMDBG
static void
wlc_11h_bsscfg_dump(void *ctx, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_11h_info_t *m11h = (wlc_11h_info_t *)ctx;
	wlc_11h_t *p11h = IEEE11H_BSSCFG_CUBBY(m11h, cfg);

	ASSERT(p11h != NULL);

	bcm_bprintf(b, "spect_state: %x\n", p11h->spect_state);
}
#endif

void
wlc_11h_tbtt(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = m11h->wlc;
	wlc_11h_t *p11h = IEEE11H_BSSCFG_CUBBY(m11h, cfg);

	ASSERT(p11h != NULL);

	/* If we have previously detected radar and scheduled a channel switch in
	 * the future, then every TBTT interval we come here and decrement the
	 * 'TBTT time to chan switch' field in the outgoing beacons.
	 */
	if (BSSCFG_AP(cfg) &&
	    (p11h->spect_state & NEED_TO_UPDATE_BCN)) {
		/* If all counts are 0, no further need to update outgoing beacons */
		if (wlc_quiet_get_quiet_count(wlc->quiet, cfg) == 0 &&
		    wlc_csa_get_csa_count(wlc->csa, cfg) == 0) {
			p11h->spect_state &= ~NEED_TO_UPDATE_BCN;
		}

		/* Count down for switch channels... */
		wlc_csa_count_down(wlc->csa, cfg);

		WL_APSTA_BCN(("wl%d: wlc_tbtt 11h -> wlc_update_beacon()\n", wlc->pub->unit));
		wlc_bss_update_beacon(wlc, cfg);
		wlc_bss_update_probe_resp(wlc, cfg, TRUE);
	}
}

#ifdef BCMDBG
static void
wlc_11h_ibss(wlc_info_t *wlc, dot11_ibss_dfs_t *tag)
{
}
#endif

void
wlc_11h_process_beacon(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg,
	uint8 *params, int len, struct dot11_bcn_prb *bcn)
{
	wlc_info_t *wlc = m11h->wlc;
	bcm_tlv_t *tag = (bcm_tlv_t*)params;
	uint8 *end = params + len;

	/* Perform channel switch if csa_ie found */
	wlc_csa_recv_process_beacon(wlc->csa, cfg, params, len);

	if ((params < end) && (tag = bcm_parse_tlvs(params, (int)(end - params),
		DOT11_MNG_PWR_CONSTRAINT_ID)) != NULL) {
		wlc_tpc_set_local_constraint(wlc->tpc, (dot11_power_cnst_t *)tag);
	}

	if ((params < end) && (tag = bcm_parse_tlvs(params, (int)(end - params),
		DOT11_MNG_QUIET_ID)) != NULL) {
		wlc_11h_quiet(wlc->quiet, cfg, (dot11_quiet_t *)tag, bcn);
	}

#ifdef BCMDBG
	if ((params < end) && (tag = bcm_parse_tlvs(params, (int)(end - params),
		DOT11_MNG_IBSS_DFS_ID)) != NULL) {
		wlc_11h_ibss(wlc, (dot11_ibss_dfs_t *)tag);
	}

	if ((params < end) && (tag = bcm_parse_tlvs(params, (int)(end - params),
		DOT11_MNG_TPC_REPORT_ID)) != NULL) {
		wlc_tpc_report(wlc->tpc, (dot11_tpc_rep_t *)tag);
	}
#endif /* BCMDBG */
}

/* Supported Channels IE */
uint8 *
wlc_11h_write_sup_chan_ie(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg, uint8 *cp, int buflen)
{
	wlc_info_t *wlc = m11h->wlc;
	uint8 run_count = 0, len = 0;
	bcm_tlv_t *sup_channel_ie = (bcm_tlv_t*)cp;
	uint8 cur_chan, first_chan = 0, seen_valid = 0, max_channel, ch_sep;
	bool valid_channel;
	uint subband_idx = 0, end_subband, j;

	/* 1-14, 34-46, 36-48, 52-64, 100-140, 149-161, 165, 184-196 */
	const struct {
		uint8 start;
		uint8 cnt;
	} subbands[] = {
		{1, 14}, {34, 4}, {36, 4}, {52, 4},
		{100, 11}, {149, 4}, {165, 1}, {184, 4}
	};

	sup_channel_ie->id = DOT11_MNG_SUPP_CHANNELS_ID;
	cp += 2; /* Skip over ID and Len */

	if (wlc->band->bandtype == WLC_BAND_2G) {
		subband_idx = 0;
		end_subband = 1;
		max_channel = CH_MAX_2G_CHANNEL + 1;
		ch_sep = CH_5MHZ_APART;
	} else {
		end_subband = ARRAYSIZE(subbands);
		max_channel = MAXCHANNEL;
		ch_sep = CH_20MHZ_APART;

		/* Handle special case for JP where subband could be 34-46 or 36-48 */
		/* Arbitrarily decided to give 36-48 priority as 34-46 is legacy passive anyway */
		if (VALID_CHANNEL20_IN_BAND(wlc, BAND_5G_INDEX, 34) &&
		    !VALID_CHANNEL20_IN_BAND(wlc, BAND_5G_INDEX, 36))
			subband_idx = 1;
		else
			subband_idx = 2;
	}

	for (; subband_idx < end_subband; subband_idx++) {
		run_count = 0;
		seen_valid = 0;

		for (cur_chan = subbands[subband_idx].start, j = 0;
		     (j < subbands[subband_idx].cnt) && (cur_chan < max_channel);
		     j++, cur_chan += ch_sep) {

			valid_channel = VALID_CHANNEL20_IN_BAND(wlc,
			                                        wlc->band->bandunit, cur_chan);
			if (valid_channel) {
				if (!seen_valid) {
					first_chan = cur_chan;
					seen_valid = 1;
				}
				run_count++;
			}
		}

		if (seen_valid) {
			*cp++ = first_chan;
			*cp++ = run_count;
			len += 2;
			run_count = 0;
			seen_valid = 0;
		}

		/* If subband 34-46 was present then skip over 36-48 */
		if (subband_idx == 1)
			subband_idx = 2;
	}

	/* This would happen if CH_MAX_2G_CHANNEL is valid */
	if (seen_valid) {
		*cp++ = first_chan;
		*cp++ = run_count;
		len += 2;
	}

	sup_channel_ie->len = len;
	return cp;
}

/*
 * Frame received, frame type FC_ACTION,
 *  action_category DOT11_ACTION_CAT_SPECT_MNG
 */
void
wlc_recv_frameaction_specmgmt(wlc_11h_info_t *m11h, struct dot11_management_header *hdr,
	uint8 *body, int body_len, int8 rssi, ratespec_t rspec)
{
	wlc_info_t *wlc = m11h->wlc;
	wlc_bsscfg_t *cfg;

	ASSERT(WL11H_ENAB(wlc));
	ASSERT(body_len >= DOT11_ACTION_HDR_LEN);
	ASSERT(body[DOT11_ACTION_CAT_OFF] == DOT11_ACTION_CAT_SPECT_MNG);

	if ((cfg = wlc_bsscfg_find_by_bssid(wlc, &hdr->bssid)) == NULL) {
#if defined(BCMDBG) || defined(WLMSG_INFORM)
		char eabuf[ETHER_ADDR_STR_LEN];
#endif

		WL_INFORM(("wl%d: %s: ignoring request from %s since we are not in a BSS or IBSS\n",
		           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->sa, eabuf)));
		return;
	}

	/* Spectrum Management action_id's */
	switch (body[DOT11_ACTION_ACT_OFF]) {
	case DOT11_SM_ACTION_M_REQ:
		if (wlc_validate_measure_req(m11h, cfg, hdr) == FALSE)
			return;
		wlc_recv_measure_request(wlc, cfg, hdr, body, body_len);
		break;
	case DOT11_SM_ACTION_M_REP:
		wlc_recv_measure_report(wlc, cfg, hdr, body, body_len);
		break;
	case DOT11_SM_ACTION_TPC_REQ:
		wlc_recv_tpc_request(wlc->tpc, cfg, hdr, body, body_len, rssi, rspec);
		break;
	case DOT11_SM_ACTION_TPC_REP:
		wlc_recv_tpc_report(wlc->tpc, cfg, hdr, body, body_len, rssi, rspec);
		break;
	case DOT11_SM_ACTION_CHANNEL_SWITCH:
		wlc_recv_csa_action(wlc->csa, cfg, hdr, body, body_len);
		break;
	case DOT11_SM_ACTION_EXT_CSA:
		wlc_recv_ext_csa_action(wlc->csa, cfg, hdr, body, body_len);
		break;
	default:
		wlc_send_action_err(wlc, hdr, body, body_len);
		break;
	}
}

/* Validate the source of a measurement request */
bool
wlc_validate_measure_req(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr)
{
	wlc_info_t *wlc = m11h->wlc;
	struct scb *scb;
	char eabuf[ETHER_ADDR_STR_LEN];
	char eabuf1[ETHER_ADDR_STR_LEN];

	BCM_REFERENCE(eabuf);
	BCM_REFERENCE(eabuf1);

	ASSERT(cfg != NULL);

	/* is this a request from someone we should care about? */
	if (!cfg->associated) {
		WL_INFORM(("wl%d: %s: ignoring request from %s since we are not in a BSS or IBSS\n",
		           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->sa, eabuf)));
		return FALSE;
	} else if (BSSCFG_AP(cfg)) {
		if (ETHER_ISMULTI(&hdr->sa) ||
		    (scb = wlc_scbfind(wlc, cfg, &hdr->sa)) == NULL ||
		    !SCB_ASSOCIATED(scb)) {
			/* AP only accepts reqs from associated STAs */
			WL_INFORM(("wl%d: %s: ignoring request from unassociated STA %s\n",
			           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->sa, eabuf)));
			return FALSE;
		} else if (ETHER_ISMULTI(&hdr->da)) {
			/* AP only accepts unicast reqs */
			WL_INFORM(("wl%d: %s: ignoring bc/mct request %s from associated STA %s\n",
			           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->da, eabuf),
			           bcm_ether_ntoa(&hdr->sa, eabuf1)));
			return FALSE;
		}
	} else if (cfg->BSS) {
		if (bcmp(hdr->sa.octet, cfg->BSSID.octet, ETHER_ADDR_LEN)) {
			/* STAs should only get requests from the AP */
			WL_INFORM(("wl%d: %s: ignoring request from %s since it is not our AP\n",
			           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->sa, eabuf)));
			return FALSE;
		}
	} else {
		/* IBSS STAs should only get requests from other IBSS members */
		WL_INFORM(("wl%d: %s: ignoring request from %s since it is in a foreign IBSS %s\n",
		           wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->sa, eabuf),
		           bcm_ether_ntoa(&hdr->bssid, eabuf1)));
			return FALSE;
	}

	return TRUE;
}

static void
wlc_recv_measure_request(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	struct dot11_action_measure * action_hdr;
	int len;
	int ie_tot_len;
	int report_len;
	uint8 *report;
	dot11_meas_req_t* ie;
	dot11_meas_rep_t* report_ie;
#ifdef BCMDBG_ERR
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG_ERR */

#ifdef BCMDBG
	if (WL_INFORM_ON())
		wlc_print_measure_req_rep(wlc, hdr, body, body_len);
#endif /* BCMDBG */

	ASSERT(cfg != NULL);

	if (body_len < 3) {
		WL_ERROR(("wl%d: %s: got Measure Request from %s, "
			  "but frame body len was %d, expected > 3\n",
			  wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&hdr->sa, eabuf), body_len));
		return;
	}

	action_hdr = (struct dot11_action_measure *)body;

	/* calculate the length of the report */
	report_len = 0;
	ie = (dot11_meas_req_t*)action_hdr->data;
	len = body_len - DOT11_ACTION_MEASURE_LEN;

	/* for each measurement request, calc the length of the report in the response */
	while (len > 2) {
		ie_tot_len = TLV_HDR_LEN + ie->len;

		if (ie->id != DOT11_MNG_MEASURE_REQUEST_ID ||
		    ie->len < DOT11_MNG_IE_MREQ_LEN ||
		    (ie->len >= 3 && (ie->mode & DOT11_MEASURE_MODE_ENABLE))) {
			/* ignore non-measure ie, short, or Mode == ENABLED requests */
		} else if (ie->type == DOT11_MEASURE_TYPE_BASIC) {
			/* Basic report with Unmeasured set */
			report_len += TLV_HDR_LEN + DOT11_MNG_IE_MREP_FIXED_LEN +
				DOT11_MEASURE_BASIC_REP_LEN;
		} else {
			/* CCA, RPI, or other req: Measure report with Incapable */
			report_len += TLV_HDR_LEN + DOT11_MNG_IE_MREP_FIXED_LEN;
		}

		ie = (dot11_meas_req_t*)((int8*)ie + ie_tot_len);
		len -= ie_tot_len;
	}

	/* allocate space and create the report */
	report = (uint8*)MALLOC(wlc->osh, report_len);
	if (report == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return;
	}

	report_ie = (dot11_meas_rep_t*)report;
	ie = (dot11_meas_req_t*)action_hdr->data;
	len = body_len - DOT11_ACTION_MEASURE_LEN;

	/* for each measurement request, generate the report for the response */
	while (len > 2) {
		ie_tot_len = TLV_HDR_LEN + ie->len;
		if (ie->id != DOT11_MNG_MEASURE_REQUEST_ID) {
			WL_INFORM(("wl%d: %s: got unexpected IE (id %d len"
				" %d), ignoring\n",
				wlc->pub->unit, __FUNCTION__, ie->id, ie->len));
		} else if (ie->len >= 3 && (ie->mode & DOT11_MEASURE_MODE_ENABLE)) {
			WL_INFORM(("wl%d: %s: got Measure Request mode Enable"
				" bit, ignoring\n",
				wlc->pub->unit, __FUNCTION__));
		} else if (ie->len < DOT11_MNG_IE_MREQ_LEN) {
			WL_ERROR(("wl%d: %s: got short Measure Request IE len"
				" %d, ignoring\n",
				wlc->pub->unit, __FUNCTION__, ie->len));
		} else {
			if (ie->type == DOT11_MEASURE_TYPE_BASIC) {
				/* Basic report with Unmeasured set */
				report_ie->id = DOT11_MNG_MEASURE_REPORT_ID;
				report_ie->len = DOT11_MNG_IE_MREP_FIXED_LEN +
					DOT11_MEASURE_BASIC_REP_LEN;
				bzero((uint8*)&report_ie->token, report_ie->len);
				report_ie->token = ie->token;
				report_ie->mode = 0;
				report_ie->type = ie->type;
				report_ie->rep.basic.channel = ie->channel;
				bcopy(ie->start_time, report_ie->rep.basic.start_time, 8);
				bcopy(&ie->duration, &report_ie->rep.basic.duration, 2);
				report_ie->rep.basic.map = DOT11_MEASURE_BASIC_MAP_UNMEAS;
			} else {
				/* CCA, RPI, or other req: Measure report with Incapable */
				report_ie->id = DOT11_MNG_MEASURE_REPORT_ID;
				report_ie->len = DOT11_MNG_IE_MREP_FIXED_LEN;
				bzero((uint8*)&report_ie->token, report_ie->len);
				report_ie->token = ie->token;
				report_ie->mode = DOT11_MEASURE_MODE_INCAPABLE;
				report_ie->type = ie->type;
			}
			report_ie = (dot11_meas_rep_t*)((int8*)report_ie + TLV_HDR_LEN +
				report_ie->len);
		}

		ie = (dot11_meas_req_t*)((int8*)ie + ie_tot_len);
		len -= ie_tot_len;
	}

	ASSERT(((uint8*)report_ie - report) == (int)report_len);

	wlc_send_measure_report(wlc, cfg, &hdr->sa, action_hdr->token, report, report_len);
	MFREE(wlc->osh, report, report_len);
}

static void
wlc_recv_measure_report(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
#ifdef BCMDBG
	if (WL_INFORM_ON())
		wlc_print_measure_req_rep(wlc, hdr, body, body_len);
#endif /* BCMDBG */

	ASSERT(cfg != NULL);
}

#ifdef BCMDBG
static void
wlc_print_measure_req_rep(wlc_info_t *wlc, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	struct dot11_action_measure * action_hdr;
	int len;
	int ie_tot_len;
	dot11_meas_req_t* req_ie;
	uint32 start_h, start_l;
	uint16 dur;
	const char *action_name;
	uint8 legal_id;
	bool is_request;
	char da[ETHER_ADDR_STR_LEN];
	char sa[ETHER_ADDR_STR_LEN];
	char bssid[ETHER_ADDR_STR_LEN];

	printf("Action Frame: DA %s SA %s BSSID %s\n",
	       bcm_ether_ntoa(&hdr->da, da), bcm_ether_ntoa(&hdr->sa, sa),
	       bcm_ether_ntoa(&hdr->bssid, bssid));

	if (body_len < 3) {
		printf("Action frame body len was %d, expected > 3\n", body_len);
		return;
	}

	action_hdr = (struct dot11_action_measure *)body;
	req_ie = (dot11_meas_req_t*)action_hdr->data;
	len = body_len - DOT11_ACTION_MEASURE_LEN;

	printf("Action Frame: category %d action %d dialog token %d\n",
	       action_hdr->category, action_hdr->action, action_hdr->token);

	if (action_hdr->category != DOT11_ACTION_CAT_SPECT_MNG) {
		printf("Unexpected category, expected Spectrum Management %d\n",
			DOT11_ACTION_CAT_SPECT_MNG);
		return;
	}

	if (action_hdr->action == DOT11_SM_ACTION_M_REQ) {
		action_name = "Measurement Request";
		legal_id = DOT11_MNG_MEASURE_REQUEST_ID;
		is_request = TRUE;
	} else if (action_hdr->action == DOT11_SM_ACTION_M_REP) {
		action_name = "Measurement Report";
		legal_id = DOT11_MNG_MEASURE_REPORT_ID;
		is_request = FALSE;
	} else {
		printf("Unexpected action type, expected Measurement Request (%d) or Report (%d)\n",
		       DOT11_MNG_MEASURE_REQUEST_ID, DOT11_MNG_MEASURE_REPORT_ID);
		return;
	}

	while (len > 0) {
		if (len < 2) {
			printf("Malformed Action frame, less that an IE header length (2 bytes)"
				" remaining in buffer\n");
			break;
		}
		if (req_ie->id != legal_id) {
			printf("Unexpected IE (id %d len %d):\n", req_ie->id, req_ie->len);
			prhex(NULL, (uint8*)req_ie + TLV_HDR_LEN, req_ie->len);
			goto next_ie;
		}
		if (req_ie->len < DOT11_MNG_IE_MREQ_FIXED_LEN) {
			printf("%s (id %d len %d): len less than minimum of %d\n",
			       action_name, req_ie->id, req_ie->len, DOT11_MNG_IE_MREQ_FIXED_LEN);
			prhex("IE data", (uint8*)req_ie + TLV_HDR_LEN, req_ie->len);
			goto next_ie;
		}
		printf("%s (id %d len %d): measure token %d mode 0x%02x type %d%s\n",
		       action_name, req_ie->id, req_ie->len, req_ie->token, req_ie->mode,
		       req_ie->type,
		       (req_ie->type == DOT11_MEASURE_TYPE_BASIC) ? " \"Basic\"" :
		       ((req_ie->type == DOT11_MEASURE_TYPE_CCA) ? " \"CCA\"" :
			((req_ie->type == DOT11_MEASURE_TYPE_RPI) ? " \"RPI Histogram\"" : "")));

		/* more data past fixed length portion of request/report? */

		if (req_ie->len <= DOT11_MNG_IE_MREP_FIXED_LEN) {
			/* just the fixed bytes of request/report present */
			goto next_ie;
		}

		/* here if more than fixed length portion of request/report */

		if (is_request && (req_ie->mode & DOT11_MEASURE_MODE_ENABLE)) {
			prhex("Measurement Request variable data (should be null since mode Enable"
				" is set)",
				&req_ie->channel, req_ie->len - 3);
			goto next_ie;
		}

		if (!is_request &&
		    (req_ie->mode & (DOT11_MEASURE_MODE_LATE |
			DOT11_MEASURE_MODE_INCAPABLE |
			DOT11_MEASURE_MODE_REFUSED))) {
			prhex("Measurement Report variable data (should be null since mode"
				" Late|Incapable|Refused is set)",
				&req_ie->channel, req_ie->len - DOT11_MNG_IE_MREP_FIXED_LEN);
			goto next_ie;
		}

		if (req_ie->type != DOT11_MEASURE_TYPE_BASIC &&
		    req_ie->type != DOT11_MEASURE_TYPE_CCA &&
		    req_ie->type != DOT11_MEASURE_TYPE_RPI) {
			prhex("variable data", &req_ie->channel, req_ie->len -
				DOT11_MNG_IE_MREP_FIXED_LEN);
			goto next_ie;
		}

		bcopy(req_ie->start_time, &start_l, 4);
		bcopy(&req_ie->start_time[4], &start_h, 4);
		bcopy(&req_ie->duration, &dur, 2);
		start_l = ltoh32(start_l);
		start_h = ltoh32(start_h);
		dur = ltoh16(dur);

		printf("%s variable data: channel %d start time %08X:%08X dur %d TU\n",
		       action_name, req_ie->channel, start_h, start_l, dur);

		if (req_ie->len > DOT11_MNG_IE_MREQ_LEN) {
			prhex("additional data", (uint8*)req_ie + TLV_HDR_LEN +
				DOT11_MNG_IE_MREQ_LEN, req_ie->len - DOT11_MNG_IE_MREQ_LEN);
		}

	next_ie:
		ie_tot_len = TLV_HDR_LEN + req_ie->len;
		req_ie = (dot11_meas_req_t*)((int8*)req_ie + ie_tot_len);
		len -= ie_tot_len;
	}
}
#endif /* BCMDBG */

static void
wlc_send_measure_request(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct ether_addr *da,
	uint8 measure_type)
{
	void *p;
	uint8* pbody;
	uint body_len;
	struct dot11_action_measure * action_hdr;
	dot11_meas_req_t *req;
	uint32 tsf_l, tsf_h;
	uint32 measure_tsf_l, measure_tsf_h;
	uint16 duration;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */

	WL_INFORM(("wl%d: %s: sending Measure Request type %d to %s\n",
	           wlc->pub->unit, __FUNCTION__, measure_type, bcm_ether_ntoa(da, eabuf)));

	ASSERT(cfg != NULL);

	/* Channel Measure Request frame is
	 * 3 bytes Action Measure Req frame
	 * 16 bytes Measure Request IE
	 */
	body_len = DOT11_ACTION_MEASURE_LEN + TLV_HDR_LEN + DOT11_MNG_IE_MREQ_LEN;

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, da, &cfg->cur_etheraddr, &cfg->BSSID,
	                       body_len, &pbody);
	if (p == NULL) {
		WL_INFORM(("wl%d: %s: no memory for Measure Request\n",
		           wlc->pub->unit, __FUNCTION__));
		return;
	}

	/* read the tsf from our chip */
	wlc_read_tsf(wlc, &tsf_l, &tsf_h);
	/* set the measure time to now + 100ms */
	measure_tsf_l = tsf_l + 100 * 1000;
	measure_tsf_h = tsf_h;
	if (measure_tsf_l < tsf_l)
		measure_tsf_h++; /* carry from addition */

	action_hdr = (struct dot11_action_measure *)pbody;
	action_hdr->category = DOT11_ACTION_CAT_SPECT_MNG;
	action_hdr->action = DOT11_SM_ACTION_M_REQ;
	/* Token needs to be non-zero, so burn the high bit */
	action_hdr->token = (uint8)(wlc->counter | 0x80);
	req = (dot11_meas_req_t *)action_hdr->data;
	req->id = DOT11_MNG_MEASURE_REQUEST_ID;
	req->len = DOT11_MNG_IE_MREQ_LEN;
	req->token = (uint8)(action_hdr->token + 1);
	req->mode = 0;
	req->type = measure_type;
	req->channel = CHSPEC_CHANNEL(WLC_BAND_PI_RADIO_CHANSPEC);
	measure_tsf_l = htol32(measure_tsf_l);
	measure_tsf_h = htol32(measure_tsf_h);
	bcopy(&measure_tsf_l, req->start_time, 4);
	bcopy(&measure_tsf_h, &req->start_time[4], 4);
	duration = htol16(50);
	bcopy(&duration, &req->duration, 2);

	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, NULL);
}

static void
wlc_send_measure_report(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct ether_addr *da,
	uint8 token, uint8 *report, uint report_len)
{
	void *p;
	uint8* pbody;
	uint body_len;
	struct dot11_action_measure * action_hdr;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */

	WL_INFORM(("wl%d: %s: sending Measure Report (token %d) to %s\n",
		wlc->pub->unit, __FUNCTION__, token, bcm_ether_ntoa(da, eabuf)));

	ASSERT(cfg != NULL);

	/* Measure Report frame is
	 * 3 bytes Action Measure Req frame
	 * variable len report
	 */
	body_len = DOT11_ACTION_MEASURE_LEN + report_len;

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, da, &cfg->cur_etheraddr, &cfg->BSSID,
	                       body_len, &pbody);
	if (p == NULL) {
		WL_INFORM(("wl%d: %s: no memory for Measure Report\n",
		           wlc->pub->unit, __FUNCTION__));
		return;
	}

	action_hdr = (struct dot11_action_measure *)pbody;
	action_hdr->category = DOT11_ACTION_CAT_SPECT_MNG;
	action_hdr->action = DOT11_SM_ACTION_M_REP;
	action_hdr->token = token;

	bcopy(report, action_hdr->data, report_len);

	wlc_sendmgmt(wlc, p, cfg->wlcif->qi, NULL);
}

int
wlc_11h_set_spect(wlc_11h_info_t *m11h, uint spect)
{
	wlc_info_t *wlc = m11h->wlc;

	ASSERT(!wlc->pub->up);

	if ((spect != SPECT_MNGMT_OFF) &&
	    (spect != SPECT_MNGMT_LOOSE_11H) &&
	    (spect != SPECT_MNGMT_STRICT_11H))
		return BCME_RANGE;

	if (m11h->_spect_management == spect)
		return BCME_OK;

	m11h->_spect_management = spect;
	wlc->pub->_11h = spect != SPECT_MNGMT_OFF;
	wlc_quiet_channels_reset(wlc->cmi);

	return BCME_OK;
}

/* accessors */
uint
wlc_11h_get_spect(wlc_11h_info_t *m11h)
{
	return m11h->_spect_management;
}

void
wlc_11h_set_spect_state(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg, uint mask, uint val)
{
	wlc_11h_t *p11h = IEEE11H_BSSCFG_CUBBY(m11h, cfg);
	uint spect_state;

	ASSERT(p11h != NULL);
	ASSERT((~mask & val) == 0);

	spect_state = p11h->spect_state;
	p11h->spect_state = (uint8)((spect_state & ~mask) | val);
}

uint
wlc_11h_get_spect_state(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg)
{
	wlc_11h_t *p11h = IEEE11H_BSSCFG_CUBBY(m11h, cfg);

	ASSERT(p11h != NULL);

	return p11h->spect_state;
}

#endif /* WL11H */
