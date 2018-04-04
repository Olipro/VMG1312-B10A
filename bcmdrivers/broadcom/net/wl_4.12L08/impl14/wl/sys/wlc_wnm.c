/*
 * 802.11v protocol implementation for
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
 * $Id: wlc_wnm.c 365823 2012-10-31 04:24:30Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>

#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/802.3.h>
#include <proto/vlan.h>
#include <proto/bcmip.h>
#include <proto/bcmipv6.h>
#include <proto/bcmarp.h>
#include <proto/bcmicmp.h>
#include <proto/bcmudp.h>
#include <proto/bcmproto.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_rate.h>
#include <wlc_scb.h>
#include <wlc_scb_ratesel.h>
#include <wlc_scan.h>
#include <wlc_wnm.h>
#include <wlc_assoc.h>
#include <wl_dbg.h>
#include <wlc_alloc.h>
#include <wlc_tpc.h>
#include <wl_export.h>
#ifdef PROXYARP
#include <proxyarp/proxyarp.h>
#endif /* PROXYARP */

/* support 32 candidates */
#define WLC_TRANS_CANDIDATE_MAX	32

typedef struct trans_candidate {
	struct ether_addr bssid;
	uint8 preference;	/* order of preference as indicated by current AP */
	uint8 channel;
} trans_candidate_t;

typedef struct transition_candidate_list {
	uint	count;			/* nmbr of elements in trans_candidate_list */
	uint8	mode;	/* mode field from AP's transition request */
	trans_candidate_t trans_candidate[WLC_TRANS_CANDIDATE_MAX];
} transition_candidate_list_t;

#define WLC_WNM_REQUEST_ID_MAX 20

typedef struct wnm_bcnreq_ {
	uint8 token;
	uint8 reg;
	uint8 channel;
	uint8 rep_detail;		/* Reporting Detail */
	uint8 req_eid[WLC_WNM_REQUEST_ID_MAX];	/* Request elem id */
	uint8 req_eid_num;
	uint16 duration;
	chanspec_t chanspec_list[MAXCHANNEL];
	uint16 channel_num;
	uint32 start_tsf_l;
	uint32 start_tsf_h;
} wnm_bcnreq_t;

typedef struct wnm_tclas {
	struct wnm_tclas *next;
	uint8 user_priority;
	uint8 fc_len;
	dot11_tclas_fc_t fc;
} wnm_tclas_t;

typedef struct wnm_tfs_req_se {
	struct wnm_tfs_req_se *next;
	uint8 subelem_id;
	wnm_tclas_t *tclas_head;
	uint32 tclas_num;	/* total tclas number */
	dot11_tclas_proc_ie_t tclas_proc;
	uint32 tclas_len;	/* tclas processing element included */
} wnm_tfs_req_se_t;

typedef struct wnm_tfs_req {
	struct wnm_tfs_req *next;
	uint8 tfs_id;
	uint8 tfs_actcode;
	wnm_tfs_req_se_t *subelem_head;
	uint32 subelem_num;
	uint32 subelem_len;
#ifdef STA
	int tfs_resp_st;
#endif /* STA */
} wnm_tfs_req_t;

#define WNM_TCLAS_ENTRY_MAX		32

#define WNM_ELEMENT_LIST_ADD(s, h, e) \
	do { \
		if ((s)->h == NULL) { \
			(s)->h = (e); \
		} else { \
			(e)->next = (s)->h; \
			(s)->h = (e); \
		} \
	} while (0)

#define WNM_TCLAS_DEL(wnm, info, start, end) \
	do { \
		if (((end) >= (info)->tclas_num) || \
		    (((end) - (start)) >= (info)->tclas_num)) \
			return BCME_RANGE; \
		while ((end)-- >= (start)) { \
			wlc_wnm_tclas_delete((wnm), &(info)->tclas_head, (start)); \
				(info)->tclas_num--; \
		} \
	} while (0)

#define WNM_TCLAS_FREE(wnm, curr, prev) \
	do { \
		while (curr != NULL) { \
			prev = curr; \
			curr = curr->next; \
			MFREE((wnm)->wlc->osh, prev, sizeof(wnm_tclas_t)); \
		} \
	} while (0)

typedef struct wnm_tfs_info {
	wnm_tclas_t *tclas_head;	/* for tclas IOVAR command */
	uint32 tclas_num;
	wnm_tfs_req_t *tfs_req_head;
	uint32 tfs_req_num;
} wnm_tfs_info_t;

#define TFS_NOTIFY_IDLIST_MAX	255

typedef struct dms_scb {
	struct dms_scb *next;
	struct scb *scb;
} dms_scb_t;

typedef struct dms_desc {
	struct dms_desc *next;
	uint8 dms_id;
	wnm_tclas_t *tclas_head;
	uint32 tclas_num;	/* total tclas number */
	dot11_tclas_proc_ie_t tclas_proc;
	uint32 tclas_len;	/* tclas processing element included */
	bcm_tlv_t *tspec;
	bcm_tlv_t *subelem;
	uint32 subelem_len;
#ifdef AP
	struct dms_scb *dms_scb_head;
#endif /* STA */
#ifdef STA
	uint8 req_type;
	int resp_type;
#endif /* STA */
	uint16 lsc;
} dms_desc_t;

typedef struct wnm_dms_info {
#ifdef STA
	wnm_tclas_t *tclas_head;	/* for tclas IOVAR command */
	uint32 tclas_num;
	uint32 dms_desc_num;
#endif /* STA */
#ifdef AP
	uint8 dms_id;
#endif /* AP */
	dms_desc_t *dms_desc_head;
} wnm_dms_info_t;

#ifdef AP

#define WLWNM_FRAME_MCAST(fp)	\
		(fp.l3_t == FRAME_L3_IP_H && 	\
		IPV4_ISMULTI(ntoh32(*((uint32 *)(fp.l3 + IPV4_DEST_IP_OFFSET)))))
#endif /* AP */

typedef struct {
	transition_candidate_list_t* trans_candidate_list;
	uint32	cap;			/* 802.11v capabilities */
	uint16	bss_max_idle_period;	/* 802.11v BSS Max Idle Period(Unit: 1000TU) */
	uint8	bss_idle_opt;		/* 802.11v BSS Max Idle Period Options */
	int32	tim_bcast_offset;	/* 802.11v TIM Broadcast offset(Uint: 1ms) */
#ifdef AP
	struct wl_timer	*tim_bcast_timer;	/* timer period of tim_bcast_offset  */
	uint16 tim_bcast_high_rate;		/* high rate for TIM broadcast frame */
	bool perm_cb;			/* permanent check_beacon value for specific BSS */
	bool temp_cb;			/* temperary check_beacon value for specific BSS */
	wnm_dms_info_t dms_info;	/* DMS descriptor for each specific BSS */
#endif /* AP */
#ifdef STA
	uint32 tim_bcast_interval;
	uint32 tim_bcast_status;
#endif /* STA */
} wnm_bsscfg_cubby_t;

#define WNM_BSSCFG_CUBBY(wnm, cfg) ((wnm_bsscfg_cubby_t*)BSSCFG_CUBBY(cfg, (wnm)->cfgh))

#define WLC_WNM_UPDATE_TOKEN(i) \
	{ ++i; if (i == 0) ++i; }

#define WLC_WNM_UPDATE_DMSID(i) \
	{ ++i; if (i == 0) ++i; }

struct wlc_wnm_info {
	wlc_info_t *wlc;
	bool state;
	uint8 req_token;		/* token used in measure requests from us */
	uint8 dialog_token;		/* Dialog token received in measure req */
	struct ether_addr da;
	wnm_bcnreq_t *bcnreq;
	int cfgh;			/* wnm bsscfg cubby handle */
	uint8 url_len;		/* session information URL length */
	uint8 *url;		/* session information URL */
#ifdef AP
	struct wl_timer	*bss_idle_timer;	/* timer for 100ms */
	uint32 timestamp;		/* increate each 100ms */
#endif /* AP */
#ifdef STA
	wnm_tfs_info_t tfs_info;
	wnm_dms_info_t dms_info;
#endif /* STA */
};

/* iovar table */
enum {
	IOV_WNM, /* enable/dsiable WNM */
	IOV_WNM_BSSTRANS_URL, /* config session information URL */
	IOV_WNM_BSSTRANS_QUERY, /* send bss transition management query frame */
	IOV_WNM_BSS_MAX_IDLE_PERIOD, /* 802.11v-2011 11.22.12 BSS Max Idle Period */
	IOV_WNM_TIM_BCAST_OFFSET, /* 802.11v-2011 11.2.1.15 TIM broadcast */
#ifdef STA
	IOV_WNM_TIM_BCAST_INTERVAL, /* 802.11v-2011 11.2.1.15 TIM broadcast */
	IOV_WNM_TIM_BCAST_STATUS, /* 802.11v-2011 11.2.1.15 TIM broadcast */
	IOV_WNM_TCLAS_ADD, /* add one tclas element */
	IOV_WNM_TCLAS_DEL, /* delete one tclas element */
	IOV_WNM_TCLAS_LIST, /* list all added tclas element */
	IOV_WNM_TFSREQ_ADD, /* add one tfs request element */
	IOV_WNM_DMSDESC_ADD, /* add one dms descriptor */
#endif /* STA */
	IOV_LAST
};

static const bcm_iovar_t wnm_iovars[] = {
	{"wnm", IOV_WNM, (0), IOVT_UINT32, 0},
	{"wnm_url", IOV_WNM_BSSTRANS_URL, (0), IOVT_BUFFER, 0},
	{"wnm_bsstq", IOV_WNM_BSSTRANS_QUERY, (IOVF_SET_UP), IOVT_BUFFER, 0},
	{"wnm_bss_max_idle_period", IOV_WNM_BSS_MAX_IDLE_PERIOD, (0), IOVT_UINT16, 0},
	{"wnm_tim_bcast_offset", IOV_WNM_TIM_BCAST_OFFSET, (0), IOVT_UINT16, 0},
#ifdef STA
	{"wnm_tim_bcast_interval", IOV_WNM_TIM_BCAST_INTERVAL, (0), IOVT_UINT8, 0},
	{"wnm_tim_bcast_status", IOV_WNM_TIM_BCAST_STATUS, (0), IOVT_UINT8, 0},
	{"wnm_tclas_add", IOV_WNM_TCLAS_ADD, (0), IOVT_BUFFER, 0},
	{"wnm_tclas_del", IOV_WNM_TCLAS_DEL, (0), IOVT_BUFFER, 0},
	{"wnm_tclas_list", IOV_WNM_TCLAS_LIST, (0), IOVT_BUFFER, 0},
	{"wnm_tfsreq_add", IOV_WNM_TFSREQ_ADD, (IOVF_SET_UP), IOVT_BUFFER, 0},
	{"wnm_dmsdesc_add", IOV_WNM_DMSDESC_ADD, (IOVF_SET_UP), IOVT_BUFFER, 0},
#endif /* STA */
	{NULL, 0, 0, 0, 0}
};

#ifdef STA
static void wlc_wnm_trans_query_scancb(void *arg, int status, wlc_bsscfg_t *cfg);
static void wlc_wnm_handle_bss_trans_query_req(wlc_wnm_info_t *wnm, wlc_ssid_t *ssid);
static void wlc_wnm_send_bss_trans_query(wlc_wnm_info_t *wnm, wlc_bss_list_t *bsslist);
#endif /* STA */
static int wlc_wnm_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);

static wnm_bsscfg_cubby_t* wlc_wnm_trans_candidate_list_alloc(wlc_wnm_info_t *wnm,
	wlc_bsscfg_t *cfg);
static int wlc_wnm_bsscfg_init(void *context, wlc_bsscfg_t *cfg);
static void wlc_wnm_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg);
#ifdef BCMDBG
static void wlc_wnm_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_wnm_bsscfg_dump NULL
#endif /* BCMDBG */

static void wlc_wnm_tfs_req_free(wlc_wnm_info_t *wnm, void *head, int tfs_id);

#ifdef AP
static void wlc_wnm_bss_idle_timer(void *context);
static int wlc_wnm_send_tim_bcast_resp_frame(wlc_wnm_info_t *wnm_info,
	struct scb *scb, uint8 token);
static void wlc_wnm_tim_bcast_timer(void *context);
static uint8 wlc_wnm_verdict_tfs_req(dot11_tfs_req_ie_t *ie);
static int wlc_wnm_send_tfs_resp_frame(wlc_info_t *wlc, struct scb *scb, uint8 *tfs_resp_ie,
	int tfs_resp_ie_len, uint8 token);
static int wlc_wnm_parse_tfs_req_tclas(wlc_info_t *wlc, wnm_tfs_req_se_t *tfs_req_subelem,
	uint8 *body, uint8 body_len);
static int wlc_wnm_parse_tfs_req_subelem(wlc_info_t *wlc, wnm_tfs_req_t *tfs_req, uint8 *body,
	uint8 body_len);
static int wlc_wnm_parse_tfs_req_ie(wlc_info_t *wlc, struct scb *scb, uint8 *body,
	int body_len, dot11_tfs_resp_ie_t *tfs_resp_ie, int *tfs_resp_ie_len);
static int wlc_wnm_send_tfs_notify_frame(wlc_info_t *wlc, struct scb *scb, bcm_tlv_t *idlist);
static uint8 wlc_wnm_tfs_packet_handle(wlc_info_t *wlc, struct scb *scb, frame_proto_t *fp);
static dms_desc_t *wlc_wnm_find_dms_desc_by_tclas(dms_desc_t *dms_desc_head,
	uint8 *data, int data_len);
static dms_desc_t *wlc_wnm_find_dms_desc_by_id(dms_desc_t *dms_desc_head, uint8 id);
static int wlc_wnm_parse_dms_req_desc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, uint8 body_len, uint8 *buf, uint8 *buf_len);
static int wlc_wnm_parse_dms_req_ie(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, int body_len, uint8 *buf, int *buf_len);
static int wlc_wnm_send_dms_resp_frame(wlc_info_t *wlc, struct scb *scb, uint8 *data,
	int len, uint8 token);
static int wlc_wnm_dms_housekeeping(struct scb *scb, int dms_id);
#endif /* AP */

#ifdef STA
static int wlc_wnm_send_tim_bcast_req_frame(wlc_wnm_info_t *wnm, int int_val);
static int wlc_wnm_prep_tclas_ie(wnm_tclas_t *tclas_head, uint8 *buf);
static int wlc_wnm_tclas_delete(wlc_wnm_info_t *wnm_info, wnm_tclas_t **head, int32 idx);
static void wlc_wnm_tfs_free(wlc_wnm_info_t *wnm);
static int wlc_wnm_parse_tfs_resp_subelem(wlc_wnm_info_t *wnm, uint8 *body, int body_len);
static int wlc_wnm_parse_tfs_resp_ie(wlc_wnm_info_t *wnm, uint8 *p, int len);
static int wlc_wnm_prep_tfs_subelem_ie(wnm_tfs_req_t *tfs_req, uint8 *buf);
static int wlc_wnm_prep_tfs_req_ie(wnm_tfs_info_t *tfs_info, uint8 *buf);
static int wlc_wnm_get_tfs_req_ie_len(wnm_tfs_info_t *tfs_info);
static int wlc_wnm_send_tfs_req_frame(wlc_wnm_info_t *wnm);
static int wlc_wnm_send_dms_req_frame(wlc_wnm_info_t *wnm);
#endif /* STA */
wlc_wnm_info_t *
BCMATTACHFN(wlc_wnm_attach)(wlc_info_t *wlc)
{
	wlc_wnm_info_t *wnm = NULL;
	bool mreg = FALSE;

	if ((wnm = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_wnm_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}

	wnm->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, wnm_iovars, "wnm", wnm, wlc_wnm_doiovar,
	                        NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n", wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	mreg = TRUE;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((wnm->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(wnm_bsscfg_cubby_t),
		wlc_wnm_bsscfg_init, wlc_wnm_bsscfg_deinit, wlc_wnm_bsscfg_dump,
		(void *)wnm)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	wnm->url_len = 0;
#ifdef AP
	/* init internal BSS Max Idle Period timer */
	if ((wnm->bss_idle_timer = wl_init_timer(wlc->wl, wlc_wnm_bss_idle_timer, wnm,
		"wnm_bss_idle_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: bss_idle_timer init failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* 100 ms periodically wakeup to check stuff */
	wl_add_timer(wlc->wl, wnm->bss_idle_timer, 100, TRUE);
#endif /* AP */
	return wnm;

	/* error handling */
fail:
	if (wnm != NULL) {
		if (mreg)
			wlc_module_unregister(wlc->pub, "wnm", wnm);
		MFREE(wlc->osh, wnm, sizeof(wlc_wnm_info_t));
	}
	return NULL;
}

void
BCMATTACHFN(wlc_wnm_detach)(wlc_wnm_info_t *wnm)
{
	if (!wnm)
		return;
#ifdef AP
	if (wnm->bss_idle_timer)
		wl_free_timer(wnm->wlc->wl, wnm->bss_idle_timer);
#endif /* AP */
#ifdef STA
	wlc_wnm_tfs_free(wnm);
#endif /* STA */
	if (wnm->url_len) {
		MFREE(wnm->wlc->osh, wnm->url, wnm->url_len);
	}

	wlc_module_unregister(wnm->wlc->pub, "wnm", wnm);
	MFREE(wnm->wlc->osh, wnm, sizeof(wlc_wnm_info_t));
}
#ifdef STA
static void
wlc_wnm_trans_query_scancb(void *arg, int status, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t *wnm = arg;
	wlc_info_t *wlc = wnm->wlc;

	if (status != WLC_E_STATUS_SUCCESS) {
		WL_ERROR(("trans_query scan failure %d\n", wlc->scan_results->count));
	}

	wlc_wnm_send_bss_trans_query(wnm, wlc->scan_results);
}

static void
wlc_wnm_handle_bss_trans_query_req(wlc_wnm_info_t *wnm, wlc_ssid_t *ssid)
{
	wlc_info_t *wlc = wnm->wlc;
	wlc_ssid_t curr_ssid;
	wlc_bss_list_t bss_list;
	bool use_scan_cache = TRUE;

	if ((ssid != NULL) && (ssid->SSID_len != 0))
		WL_ERROR(("wlc_wnm_send_bss_trans_query: %s\n", ssid->SSID));
	else {
		WL_ERROR(("wlc_wnm_send_bss_trans_query: no SSID. Use %s\n", wlc->cfg->SSID));
		bcopy(wlc->cfg->SSID, curr_ssid.SSID, wlc->cfg->SSID_len);
		curr_ssid.SSID_len = wlc->cfg->SSID_len;
		ssid = &curr_ssid;
	}

	wlc_scan_get_cache(wlc->scan, NULL, 1, ssid, DOT11_BSSTYPE_ANY, NULL, 0, &bss_list);
	WL_ERROR(("bss_list.count: %d\n", bss_list.count));

	/* Do not use the scan cache if the only entry is the AP we're associated with. */
	if (bss_list.count == 1) {
		wlc_bss_info_t *bi = bss_list.ptrs[0];

		if (memcmp(&bi->BSSID, &wlc->cfg->BSSID, ETHER_ADDR_LEN) == 0) {
			use_scan_cache = FALSE;
		}
	}

	if (use_scan_cache && bss_list.count >= 1) {
		WL_ERROR(("wlc_wnm_send_bss_trans_query from cache\n"));
		wlc_wnm_send_bss_trans_query(wnm, &bss_list);
	}
	else {
		WL_ERROR(("wlc_wnm_send_bss_trans_query from scan\n"));
		wlc_scan_request(wnm->wlc, DOT11_BSSTYPE_ANY, &ether_bcast,
			1, ssid, DOT11_SCANTYPE_ACTIVE,
			-1, -1, -1, -1, NULL, 0, FALSE, wlc_wnm_trans_query_scancb, wnm);
	}
	if (bss_list.count)
		wlc_bss_list_free(wlc, &bss_list);
}

static void
wlc_wnm_send_bss_trans_query(wlc_wnm_info_t *wnm, wlc_bss_list_t *bss_list)
{
	wlc_info_t *wlc = wnm->wlc;
	int buflen;
	void *p;
	uint8 *pbody;
	unsigned int i;
	dot11_bss_trans_query_t *rmqry;
	wlc_bss_info_t *bi;
	uint32 bssid_info, n_bssid_info;
	uint8 *bufptr;
	dot11_rmrep_nbr_t *nbr_rep;
	bcm_tlv_t *frm_body_tlv;

	WL_ERROR(("%s: bss_list->count: %d\n", __FUNCTION__, bss_list->count));

	/* rm frame action header + neighbor reports */
	buflen = DOT11_BSS_TRANS_QUERY_LEN +
		(bss_list->count) * (TLV_HDR_LEN + DOT11_RMREP_NBR_LEN);

	if ((p = wlc_frame_get_mgmt(wlc, FC_ACTION, &wlc->cfg->BSSID,
		&wlc->pub->cur_etheraddr, &wlc->cfg->BSSID, buflen, &pbody)) == NULL) {
		return;
	}

	rmqry = (dot11_bss_trans_query_t*)pbody;
	rmqry->category = DOT11_ACTION_CAT_WNM;
	rmqry->action = DOT11_WNM_ACTION_BSS_TRANS_QURY;
	WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	rmqry->token = wnm->req_token;
	rmqry->reason = 0;  /* unspecified */
	bufptr = &rmqry->data[0];
	for (i = 0; i < bss_list->count; i++) {
		bi = bss_list->ptrs[i];

		if (memcmp(&bi->BSSID, &wlc->cfg->BSSID, ETHER_ADDR_LEN)) {
			frm_body_tlv = (bcm_tlv_t *)bufptr;
			frm_body_tlv->id = DOT11_MNG_NBR_REP_ID;
			frm_body_tlv->len = DOT11_RMREP_NBR_LEN;
			nbr_rep = (dot11_rmrep_nbr_t *)&frm_body_tlv->data[0];
			bcopy(&bi->BSSID, &nbr_rep->bssid, ETHER_ADDR_LEN);
			bssid_info = 0;
			if (bi->capability & DOT11_CAP_SPECTRUM)
				bssid_info |= DOT11_NBR_RPRT_BSSID_INFO_CAP_SPEC_MGMT;
			n_bssid_info = hton32(bssid_info);
			bcopy(&n_bssid_info, &nbr_rep->bssid_info, sizeof(nbr_rep->bssid_info));
			nbr_rep->reg = wlc_get_regclass(wnm->wlc->cmi, bi->chanspec);
			nbr_rep->channel = CHSPEC_CHANNEL(bi->chanspec);
			nbr_rep->phytype = 0;
			bufptr += TLV_HDR_LEN + DOT11_RMREP_NBR_LEN;
		}
	}
	buflen = (uint)bufptr - (uint)pbody;
	prhex("Raw Trans Query", (uchar*)pbody, buflen);

	/* Fix up packet length */
	PKTSETLEN(wlc->osh, p, ((uint)bufptr - (uint)pbody) + DOT11_MGMT_HDR_LEN);

	prhex("Raw Trans Query", (uchar*)pbody - DOT11_MGMT_HDR_LEN, PKTLEN(wlc->osh, p));

	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, NULL);
}
#endif /* STA */


/* Iterates through the transition candidate list looking for the AP with the highest preference
 * score.
 * Requires that transition candidate list is non-NULL and has a valid count value.
 * Note that a NULL may be returned if the candidate list contains only excluded candidates.
 */
static const trans_candidate_t *
wlc_wnm_get_pref_cand(const transition_candidate_list_t *tcl)
{
	const trans_candidate_t *pref;
	int i;

	for (i = 0, pref = NULL; i < tcl->count; i++) {
		/* 11.22.6.3 states that the higher the number, the more preferred the AP is and
		 * that a preference of 0 indicates that the BSS is excluded.
		 */
		if (tcl->trans_candidate[i].preference > 0) {
			if (pref == NULL ||
				pref->preference < tcl->trans_candidate[i].preference) {
				pref = &tcl->trans_candidate[i];
			}
		}
	}

	return pref;
}


static void
wlc_send_bssmgmt_response(wlc_info_t *wlc, struct ether_addr *da, wlc_bsscfg_t *bsscfg,
	uint8 *body, int body_len)
{
	void *p;
	uint8* pbody;
	uint8* bodyptr;
	dot11_bss_trans_req_t* transreq;
	dot11_bss_trans_res_t* transres;
	bcm_tlv_t *tlvs;
	dot11_rmrep_nbr_t *nbr_rep;
	int tlv_len;
	int subelem_len;
	bcm_tlv_t* pref_subelem;
#if defined(BCMDBG) || defined(WLMSG_INFORM)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_INFORM */
	transition_candidate_list_t* trans_candidate_list;
	wnm_bsscfg_cubby_t* wnm_cfg;
	bool remove_target_bssid_field = TRUE;

	transreq = (dot11_bss_trans_req_t*)body;

	p = wlc_frame_get_mgmt(wlc, FC_ACTION, da, &bsscfg->cur_etheraddr, &bsscfg->BSSID,
		DOT11_BSS_TRANS_RES_LEN + ETHER_ADDR_LEN, &pbody);
	if (p == NULL) {
		WL_ERROR(("wl%d: %s: no memory for BSS Management response\n",
		           wlc->pub->unit, __FUNCTION__));
		return;
	}

	transres = (dot11_bss_trans_res_t*)pbody;

	transres->category = DOT11_ACTION_CAT_WNM;
	transres->action = DOT11_WNM_ACTION_BSS_TRANS_RESP;
	transres->status = DOT11_BSS_TRNS_RES_STATUS_ACCEPT;
	transres->term_delay = 0;

#if defined(BCMDBG) || defined(WLMSG_INFORM)
	prhex("Raw Trans Req", (uchar*)body, body_len);
#endif /* BCMDBG || WLMSG_INFORM */

	transres->token = transreq->token;

	WL_INFORM(("wl%d: %s: sending BSS Management response (token %d, reqmode 0x%x) to %s\n",
		wlc->pub->unit, __FUNCTION__, transres->token, transreq->reqmode,
		bcm_ether_ntoa(da, eabuf)));

	wnm_cfg = wlc_wnm_trans_candidate_list_alloc(wlc->wnm_info, bsscfg);
	trans_candidate_list = wnm_cfg->trans_candidate_list;

	/* Populate the local transition candidate list if provided in the request. */
	if (transreq->reqmode & DOT11_BSS_TRNS_REQMODE_PREF_LIST_INCL) {
		if (transreq->reqmode & DOT11_BSS_TRNS_REQMODE_BSS_TERM_INCL) {
			bodyptr = (uint8*)&transreq->data[DOT11_BSS_TERM_DUR_LEN];
			WL_INFORM(("duration included\n"));
		}
		else {
			bodyptr = (uint8*)&transreq->data;
			WL_INFORM(("duration not included\n"));
		}

		trans_candidate_list->mode = transreq->reqmode;
		/* move past the optional URL */
		if (transreq->reqmode & DOT11_BSS_TRNS_REQMODE_ESS_DISASSOC_IMNT)
			bodyptr += *bodyptr + 1;	/* first byte is length */

		tlv_len = body_len - ((uint)bodyptr - (uint)body);
		tlvs = (bcm_tlv_t*)bodyptr;

		while (tlvs && tlvs->id == DOT11_MNG_NBR_REP_ID) {
			trans_candidate_t *tcptr = NULL;
			tcptr = &trans_candidate_list->trans_candidate
				[trans_candidate_list->count];

			nbr_rep = (dot11_rmrep_nbr_t *)&tlvs->data[0];
			WL_INFORM(("%s: bssid %s, bssinfo 0x%x, reg %d, channel %d, phytype %d\n",
				__FUNCTION__, bcm_ether_ntoa(&nbr_rep->bssid, eabuf),
				ntoh32_ua(&nbr_rep->bssid_info), nbr_rep->reg,
				nbr_rep->channel, nbr_rep->phytype));

			subelem_len = tlvs->data[1] - OFFSETOF(dot11_rmrep_nbr_t, sub_elements)
				+ TLV_HDR_LEN;
			pref_subelem = (bcm_tlv_t*)bcm_parse_tlvs(&nbr_rep->sub_elements,
				subelem_len, DOT11_NBR_RPRT_SUBELEM_BSS_CANDDT_PREF_ID);
			bcopy(&nbr_rep->bssid, &tcptr->bssid, ETHER_ADDR_LEN);
			if (pref_subelem != NULL)
				tcptr->preference = pref_subelem->data[0];
			else
				tcptr->preference = 1;
			tcptr->channel = nbr_rep->channel;

			wnm_cfg->trans_candidate_list->count++;
			tlvs = bcm_next_tlv(tlvs, &tlv_len);
		}
	}

	/* Reassoc if we have to, or if a list is provided */
	WL_INFORM(("Transition req list length %d\n", wnm_cfg->trans_candidate_list->count));
	if ((transreq->reqmode & DOT11_BSS_TRNS_REQMODE_DISASSOC_IMMINENT) ||
		(wnm_cfg->trans_candidate_list->count > 0)) {
		const trans_candidate_t *tcptr = NULL;
		/* Use the preference list from the AP to reassoc. */
		if (trans_candidate_list->count > 0) {
			tcptr = wlc_wnm_get_pref_cand(trans_candidate_list);
		}

		if (tcptr) {
			wl_reassoc_params_t reassoc_params;

			WL_INFORM(("Transition req: Reassoc with %s channel=%d\n",
				bcm_ether_ntoa(&tcptr->bssid, eabuf), tcptr->channel));
			bcopy((void *)&tcptr->bssid, (void *)&reassoc_params.bssid, ETHER_ADDR_LEN);
			if (tcptr->channel) {
				reassoc_params.chanspec_num = 1;
				reassoc_params.chanspec_list[0] = CH20MHZ_CHSPEC(tcptr->channel);
			} else {
				reassoc_params.chanspec_num = 0;
			}

			bcopy((void *)&tcptr->bssid, pbody + DOT11_BSS_TRANS_RES_LEN,
				ETHER_ADDR_LEN);
			remove_target_bssid_field = FALSE;
#ifdef STA
			wlc_reassoc(bsscfg, &reassoc_params);
#endif
		} else {
			/* No preference list provided, do full transition ourselves. */
			WL_INFORM(("Transition req: Request roam scan\n"));
#ifdef STA
			wlc_roam_scan(bsscfg, WLC_E_REASON_REQUESTED_ROAM);
#endif
		}
	} else {
		WL_INFORM(("Transition req: Rejecting transition request (list len %d)\n",
			wnm_cfg->trans_candidate_list->count));
		transres->status = DOT11_BSS_TRNS_RES_STATUS_REJECT;
	}

	if (remove_target_bssid_field) {
		/* Remove target BSSID field from packet since we won't be transitioning or we don't
		 * yet know which BSS we're transitioning to.
		 */
		PKTSETLEN(wlc->osh, p, PKTLEN(wlc->osh, p) - ETHER_ADDR_LEN);
	}
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, NULL);
}

static void
wlc_wnm_send_bssmgmt_request(wlc_wnm_info_t *wnm_info, struct ether_addr *da,
	wlc_bsscfg_t *bsscfg, uint8 *body, int body_len, struct scb *scb)
{
	wlc_info_t *wlc;
	dot11_bss_trans_query_t *transquery;
	dot11_bss_trans_req_t *transreq;
	void *p;
	uint8* pbody;
	int len;
	wnm_url_t *url;

	wlc = wnm_info->wlc;

	if (BSSCFG_STA(bsscfg))
		return;
	if (body == NULL)
		return;
	if (wnm_info->url_len == 0)
		return;
	transquery = (dot11_bss_trans_query_t *)body;
	len = DOT11_BSS_TRANS_REQ_LEN + wnm_info->url_len + 1;
	p = wlc_frame_get_mgmt(wlc, FC_ACTION, da, &bsscfg->cur_etheraddr, &bsscfg->BSSID,
	                       len, &pbody);
	if (p == NULL) {
		WL_ERROR(("wl%d: %s: no memory for BSS Management request\n",
		           wlc->pub->unit, __FUNCTION__));
		return;
	}

	transreq = (dot11_bss_trans_req_t *)pbody;
	transreq->category = DOT11_ACTION_CAT_WNM;
	transreq->action = DOT11_WNM_ACTION_BSS_TRANS_REQ;
	transreq->token = transquery->token;
	transreq->reqmode = DOT11_BSS_TRNS_REQMODE_ESS_DISASSOC_IMNT;
	transreq->disassoc_tmr = 0;
	transreq->validity_intrvl = 0;
	url = (wnm_url_t *)&transreq->data[0];
	url->len = wnm_info->url_len;
	if (wnm_info->url_len) {
		bcopy(wnm_info->url, &url->data[0], wnm_info->url_len);
	}

	PKTSETLEN(wlc->osh, p, len + DOT11_MGMT_HDR_LEN);
	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, NULL);
}

void wlc_wnm_recv_process_wnm(wlc_wnm_info_t *wnm_info, wlc_bsscfg_t *bsscfg,
	uint action_id, struct scb *scb, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	wlc_info_t *wlc;
	wnm_bsscfg_cubby_t *wnm_cfg = NULL;

	if (wnm_info == NULL)
		return;

	if (bsscfg == NULL)
		return;

	if (scb == NULL)
		return;

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm_info, bsscfg);

	wlc = wnm_info->wlc;

	if (bsscfg != wlc_bsscfg_find_by_hwaddr(wlc, &hdr->da))
		return;

	switch (action_id) {
	case DOT11_WNM_ACTION_BSS_TRANS_REQ:
		if (body_len < DOT11_BSS_TRANS_REQ_LEN)
			return;
		wlc_send_bssmgmt_response(wlc, &hdr->sa, bsscfg, body, body_len);
		break;
	case DOT11_WNM_ACTION_BSS_TRANS_QURY:
		if (body_len < DOT11_BSS_TRANS_QUERY_LEN)
			return;
		wlc_wnm_send_bssmgmt_request(wnm_info, &hdr->sa, bsscfg, body, body_len, scb);
		break;
	case DOT11_WNM_ACTION_BSS_TRANS_RESP:
		if (body_len < DOT11_BSS_TRANS_RES_LEN)
			return;
		break;
#ifdef AP
	case DOT11_WNM_ACTION_TIM_BCAST_REQ: {
		dot11_tim_bcast_req_t *req;

		if (!WLWNM_ENAB(wlc->pub) || !WNM_TIM_BCAST_ENABLED(wnm_cfg->cap)) {
			WL_ERROR(("WNM TIM Broadcast enabled\n"));
			return;
		}
		if (body_len < DOT11_TIM_BCAST_REQ_LEN) {
			WL_ERROR(("WNM TIM Broadcast length error\n"));
			return;
		}
		if (!SCB_TIM_BCAST(scb)) {
			WL_ERROR(("STA unsupport TIM Broadcast function\n"));
			return;
		}
#ifdef BCMDBG
		prhex("Raw TIM Bcast Req frame", (uchar *)body, body_len);
#endif /* BCMDBG */

		req = (dot11_tim_bcast_req_t *)body;

		wlc_process_tim_bcast_req_ie(wlc, (uint8 *)&req->data[0],
			TLV_HDR_LEN + DOT11_TIM_BCAST_REQ_IE_LEN, scb);
		wlc_wnm_send_tim_bcast_resp_frame(wnm_info, scb, req->token);
		break;
	}
	case DOT11_WNM_ACTION_TFS_REQ: {
		dot11_tfs_req_t *tfs_req_frame;
		uint8 *buf = NULL;
		int buf_len = 0;

		if (!WLWNM_ENAB(wlc->pub) || !WNM_TFS_ENABLED(wnm_cfg->cap)) {
			WL_ERROR(("WNM TFS not enabled\n"));
			return;
		}
		if (body_len < DOT11_TFS_REQ_LEN) {
			WL_ERROR(("WNM TFS request Error\n"));
			return;
		}
		if (!SCB_TFS(scb)) {
			WL_ERROR(("STA unsupport TFS mode\n"));
			return;
		}
#ifdef BCMDBG
		prhex("Raw TFS Req frame", (uchar *)body, body_len);
#endif /* BCMDBG */

		/* A valid TFS request frame, disregard the previous TFS request elements */
		wlc_wnm_tfs_req_free(wnm_info, &scb->tfs_list, -1);
		ASSERT(scb->tfs_list == NULL);

		tfs_req_frame = (dot11_tfs_req_t *)body;
		body += DOT11_TFS_REQ_LEN;
		body_len -= DOT11_TFS_REQ_LEN;


		if (!(buf = MALLOC(wlc->osh, DOT11_MAX_MPDU_BODY_LEN))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			break;
		}

		/* process tfs request and generate tfs response IE */
		if (wlc_wnm_parse_tfs_req_ie(wlc, scb, body, body_len,
			(dot11_tfs_resp_ie_t *)buf, &buf_len) != BCME_OK) {
			WL_ERROR(("WNM TFS req param Error\n"));
			MFREE(wlc->osh, buf, DOT11_MAX_MPDU_BODY_LEN);
			break;
		}

		/* ACKed TFS response null frame in response to TFS request null frame */
		if (body_len == 0)
			WL_ERROR(("ACKed TFS resp null frame to TFS req null frame\n"));

		wlc_wnm_send_tfs_resp_frame(wlc, scb, buf, buf_len, tfs_req_frame->token);
		MFREE(wlc->osh, buf, DOT11_MAX_MPDU_BODY_LEN);

		break;
	}
	case DOT11_WNM_ACTION_DMS_REQ: {
		dot11_dms_req_t *dms_req_frame;
		uint8 *buf = NULL;
		int buf_len = 0;

		if (!WLWNM_ENAB(wlc->pub) || !WNM_DMS_ENABLED(wnm_cfg->cap)) {
			WL_ERROR(("WNM DMS not enabled\n"));
			return;
		}
		if (body_len < DOT11_DMS_REQ_LEN) {
			WL_ERROR(("WNM DMS request Error\n"));
			return;
		}
		if (!SCB_DMS(scb)) {
			WL_ERROR(("STA unsupport DMS mode\n"));
			return;
		}
#ifdef BCMDBG
		prhex("Raw DMS Req frame", (uchar *)body, body_len);
#endif /* BCMDBG */
		dms_req_frame = (dot11_dms_req_t *)body;
		body += DOT11_DMS_REQ_LEN;
		body_len -= DOT11_DMS_REQ_LEN;

		if (!(buf = MALLOC(wlc->osh, DOT11_MAX_MPDU_BODY_LEN))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			break;
		}

		if (wlc_wnm_parse_dms_req_ie(wlc, bsscfg, scb, body, body_len, buf, &buf_len)) {
			WL_ERROR(("WNM DMS req param Error\n"));
			MFREE(wlc->osh, buf, DOT11_MAX_MPDU_BODY_LEN);
			return;
		}

		wlc_wnm_send_dms_resp_frame(wlc, scb, buf, buf_len, dms_req_frame->token);
		MFREE(wlc->osh, buf, DOT11_MAX_MPDU_BODY_LEN);

		break;
	}
#endif /* AP */
#ifdef STA
	case DOT11_WNM_ACTION_TFS_RESP: {
		dot11_tfs_resp_t *frame;

		if (!WLWNM_ENAB(wlc->pub) || !WNM_TFS_ENABLED(wnm_cfg->cap)) {
			WL_ERROR(("WNM TFS not enabled\n"));
			return;
		}
		if (body_len < DOT11_TFS_RESP_LEN)
			return;
#ifdef BCMDBG
		prhex("Raw TFS Resp frame", (uchar *)body, body_len);
#endif /* BCMDBG */

		frame = (dot11_tfs_resp_t *)body;

		if (frame->token != wnm_info->req_token) {
			WL_ERROR(("wl%d: %s: unmatched req_token 0x%02x, resp_token 0x%02x\n",
				wlc->pub->unit, __FUNCTION__, wnm_info->req_token,
				frame->token));
			return;
		}

		body += DOT11_TFS_RESP_LEN;
		body_len -= DOT11_TFS_RESP_LEN;

		if (wlc_wnm_parse_tfs_resp_ie(wnm_info, body, body_len)) {
			WL_ERROR(("WNM TFS resp param Error\n"));
			return;
		}

		break;
	}
	case DOT11_WNM_ACTION_TFS_NOTIFY: {
		if (!WLWNM_ENAB(wlc->pub) || !WNM_TFS_ENABLED(wnm_cfg->cap)) {
			WL_ERROR(("WNM TFS not enabled\n"));
			return;
		}

		if (body_len < DOT11_TFS_NOTIFY_LEN) {
			WL_ERROR(("WNM TFS-notify invalid length\n"));
			return;
		}
#ifdef BCMDBG
		prhex("Raw TFS-Notify frame", (uchar *)body, body_len);
#endif /* BCMDBG */

		break;
	}
#endif /* STA */
	default:
		break;
	}
}

static int
wlc_wnm_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_wnm_info_t *wnm = (wlc_wnm_info_t *)hdl;
	wlc_info_t * wlc = wnm->wlc;
	wlc_bsscfg_t *bsscfg;
	wnm_bsscfg_cubby_t *wnm_cfg;
	int err = BCME_OK;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
	ASSERT(wnm_cfg != NULL);

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;


	/* update wlcif pointer */
	if (wlcif == NULL)
		wlcif = bsscfg->wlcif;
	ASSERT(wlcif != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_WNM):
		*ret_int_ptr = (int32)wnm_cfg->cap;
#ifdef PROXYARP
		if (proxyarp_get())
			*ret_int_ptr |= WL_WNM_PROXYARP;
		else
			*ret_int_ptr &= ~WL_WNM_PROXYARP;
#endif /* !PROXYARP */
		break;
	case IOV_SVAL(IOV_WNM): {
		if (int_val)
			wlc->pub->_wnm = TRUE;
		else
			wlc->pub->_wnm = FALSE;

#ifndef PROXYARP
		int_val &= ~WL_WNM_PROXYARP;
#endif /* !PROXYARP */

		/* update to per bsscfg cubby */
		wlc_wnm_set_cap(wlc, bsscfg, int_val);

		break;
	}
	case IOV_GVAL(IOV_WNM_BSSTRANS_URL): {
		wnm_url_t *url = (wnm_url_t *)arg;
		if (len < wnm->url_len + sizeof(wnm_url_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		url->len = wnm->url_len;
		if (wnm->url_len) {
			bcopy(wnm->url, &url->data[0], wnm->url_len);
		}
		break;
	}
	case IOV_SVAL(IOV_WNM_BSSTRANS_URL): {
		wnm_url_t *url = (wnm_url_t *)arg;
		if (len < (int)sizeof(wnm_url_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (len < url->len + sizeof(wnm_url_t) - 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}
		if (url->len != wnm->url_len) {
			if (wnm->url_len) {
				MFREE(wlc->osh, wnm->url, wnm->url_len);
				wnm->url_len = 0;
			}
			if (url->len == 0)
				break;
			if (!(wnm->url = MALLOC(wlc->osh, url->len))) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				err = BCME_NOMEM;
				break;
			}
			wnm->url_len = url->len;
		}
		if (url->len)
			bcopy(&url->data[0], wnm->url, wnm->url_len);
		break;
	}
#ifdef STA
	case IOV_SVAL(IOV_WNM_BSSTRANS_QUERY):	{
		wlc_ssid_t ssid;

		if (!WLWNM_ENAB(wlc->pub) || !WNM_BSSTRANS_ENABLED(wnm_cfg->cap)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (!wlc->cfg->associated)
			return BCME_NOTASSOCIATED;

		if (len == sizeof(wlc_ssid_t)) {
			bcopy(arg, &ssid, sizeof(wlc_ssid_t));
			wlc_wnm_handle_bss_trans_query_req(wnm, &ssid);
		}
		else {
			wlc_wnm_handle_bss_trans_query_req(wnm, NULL);
		}
		break;
	}
#endif /* STA */
	case IOV_GVAL(IOV_WNM_BSS_MAX_IDLE_PERIOD): {
		*ret_int_ptr = (int32)(wnm_cfg->bss_max_idle_period);
		break;
	}
#ifdef AP
	case IOV_SVAL(IOV_WNM_BSS_MAX_IDLE_PERIOD): {
		if (!BSSCFG_AP(bsscfg) || !WNM_BSS_MAX_IDLE_PERIOD_ENABLED(wnm_cfg->cap)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Range only support for unsigned 16-bit width */
		if (int_val > 65535) {
			err = BCME_RANGE;
			break;
		}

		wnm_cfg->bss_max_idle_period = int_val;

		break;
	}
#endif /* AP */
	case IOV_GVAL(IOV_WNM_TIM_BCAST_OFFSET): {
		*ret_int_ptr = wnm_cfg->tim_bcast_offset;
		break;
	}
#ifdef AP
	case IOV_SVAL(IOV_WNM_TIM_BCAST_OFFSET): {
		if (!BSSCFG_AP(bsscfg) || !WNM_TIM_BCAST_ENABLED(wnm_cfg->cap)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		wnm_cfg->tim_bcast_offset = int_val;

		break;
	}
#endif /* AP */
#ifdef STA
	case IOV_GVAL(IOV_WNM_TIM_BCAST_INTERVAL): {
		*ret_int_ptr = (int32)(wnm_cfg->tim_bcast_interval);
		break;
	}
	case IOV_SVAL(IOV_WNM_TIM_BCAST_INTERVAL): {
		if (!WNM_TIM_BCAST_ENABLED(wnm_cfg->cap)) {
			err = BCME_UNSUPPORTED;
			break;
		}
		/* Range only support for 8-bit width */
		if (int_val > 255) {
			err = BCME_RANGE;
			break;
		}

		wnm_cfg->tim_bcast_interval = int_val;

		/* send TIM Broadcast request if already associated */
		if (wlc->cfg->associated) {
			wlc_wnm_send_tim_bcast_req_frame(wnm, int_val);
		}

		break;
	}
	case IOV_GVAL(IOV_WNM_TIM_BCAST_STATUS): {
		*ret_int_ptr = (int32)(wnm_cfg->tim_bcast_status);

		break;
	}
	case IOV_SVAL(IOV_WNM_TCLAS_ADD): {
		uint8 *ptr = (uint8 *)arg;
		uint8 service_type, user_priority;
		wnm_tclas_t *tclas;

		if (!BSSCFG_STA(bsscfg) || !WLWNM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Check service type for this tclas element */
		service_type = *ptr++;
		switch (service_type) {
		case TCLAS_SERVICE_DMS:
			if (wnm->dms_info.tclas_num > WNM_TCLAS_ENTRY_MAX)
				err = BCME_RANGE;
			break;
		case TCLAS_SERVICE_TFS:
			if (wnm->tfs_info.tclas_num > WNM_TCLAS_ENTRY_MAX)
				err = BCME_RANGE;
			break;
		case TCLAS_SERVICE_FMS:
			err = BCME_UNSUPPORTED;
			break;
		default:
			err = BCME_UNSUPPORTED;
			break;
		}

		if (err)
			break;

		/* Get the user priority */
		user_priority = *ptr++;

		tclas = MALLOC(wlc->osh, sizeof(wnm_tclas_t));
		if (tclas == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			err = BCME_NOMEM;
			break;
		}
		bzero(tclas, sizeof(wnm_tclas_t));

		tclas->user_priority = user_priority;
		bcopy((void *)ptr, tclas->fc.fc_data, len - 2);
		tclas->fc_len = len - 2;
		tclas->next = NULL;

		if (service_type == TCLAS_SERVICE_DMS) {
			wnm_dms_info_t *dms_info = &wnm->dms_info;

			WNM_ELEMENT_LIST_ADD(dms_info, tclas_head, tclas);
			dms_info->tclas_num++;
		} else if (service_type == TCLAS_SERVICE_TFS) {
			wnm_tfs_info_t *tfs_info = &wnm->tfs_info;

			WNM_ELEMENT_LIST_ADD(tfs_info, tclas_head, tclas);
			tfs_info->tclas_num++;
		}
		break;
	}
	case IOV_SVAL(IOV_WNM_TCLAS_DEL): {
		uint8 *ptr = (uint8 *)arg;
		uint8 service_type, idx_len;
		int32 start_idx, end_idx;

		if (!BSSCFG_STA(bsscfg) || !WLWNM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Check service type for this tclas element */
		service_type = *ptr++;
		if ((service_type != TCLAS_SERVICE_DMS) &&
		    (service_type != TCLAS_SERVICE_TFS) &&
		    (service_type != TCLAS_SERVICE_FMS)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		idx_len = *ptr++;
		start_idx = (int32)*ptr++;
		end_idx = (idx_len == 2) ? (int32)*ptr : start_idx;

		if (service_type == TCLAS_SERVICE_DMS) {
			wnm_dms_info_t *dms_info = &wnm->dms_info;
			WNM_TCLAS_DEL(wnm, dms_info, start_idx, end_idx);

		} else if (service_type == TCLAS_SERVICE_TFS) {
			wnm_tfs_info_t *tfs_info = &wnm->tfs_info;
			WNM_TCLAS_DEL(wnm, tfs_info, start_idx, end_idx);
		}
		break;
	}
	case IOV_GVAL(IOV_WNM_TCLAS_LIST): {
		wl_tclas_fc_param_list_t *tclas_list = (wl_tclas_fc_param_list_t *)arg;
		wl_tclas_fc_param_t *tclas_fc;
		uint8 *ptr;
		wnm_tclas_t *_tclas, *tclas;
		uint32 totlen = 0, tclas_len;

		if (!BSSCFG_STA(bsscfg) || !WLWNM_ENAB(wlc->pub)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* Check service type */
		if (*(uint8 *)params == TCLAS_SERVICE_DMS) {
			tclas_list->num = wnm->dms_info.tclas_num;
			tclas = wnm->dms_info.tclas_head;
		} else if (*(uint8 *)params == TCLAS_SERVICE_TFS) {
			tclas_list->num = wnm->tfs_info.tclas_num;
			tclas = wnm->tfs_info.tclas_head;
		} else {
			err = BCME_BADARG;
			break;
		}

		if (tclas_list->num == 0)
			break;

		/* Check the total length to be returned.
		 * The content format to be returned is as follows:
		 * tclas_num, one or more [user_priority, tclas_param_len, tclas_param]
		 */
		totlen += sizeof(tclas_list->num);
		for (_tclas = tclas; tclas; tclas = tclas->next) {
			totlen += sizeof(tclas_fc->user_priority) +
				sizeof(tclas_fc->len) +	tclas->fc_len;
		}

		if (totlen > (uint32)len) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		ptr = (uint8 *)&tclas_list->param[0];

		for (tclas = _tclas; tclas; tclas = tclas->next) {
			tclas_fc = (wl_tclas_fc_param_t *)ptr;

			tclas_len = sizeof(tclas_fc->user_priority) +
				sizeof(tclas_fc->len) +	tclas->fc_len;

			tclas_fc->user_priority = tclas->user_priority;
			tclas_fc->len = tclas->fc_len;
			bcopy(tclas->fc.fc_data, tclas_fc->param.fc_data, tclas->fc_len);

			ptr += tclas_len;
		}

		break;
	}
	case IOV_SVAL(IOV_WNM_TFSREQ_ADD): {
		wl_tfs_req_t wl_tfs_req;
		wnm_tfs_info_t *tfs_info = &wnm->tfs_info;
		wnm_tfs_req_t *tfs_req_curr, *tfs_req;
		wnm_tfs_req_se_t *tfs_req_subelem;

		if (!BSSCFG_STA(bsscfg) || !WNM_TFS_ENABLED(wnm_cfg->cap)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (!bsscfg->associated) {
			err = BCME_NOTASSOCIATED;
			break;
		}

		bcopy(arg, &wl_tfs_req, sizeof(wl_tfs_req_t));
		if (tfs_info->tclas_head == NULL && wl_tfs_req.tfs_subelem_id) {
			err = BCME_UNSUPPORTED;
			break;
		}

		/* get tfs_req if we had specify tfs_id and the tfs_req with this id had created */
		tfs_req_curr = tfs_info->tfs_req_head;
		while (tfs_req_curr != NULL) {
			if (tfs_req_curr->tfs_id == wl_tfs_req.tfs_id &&
			    tfs_req_curr->tfs_actcode == wl_tfs_req.tfs_actcode)
				break;
			tfs_req_curr = tfs_req_curr->next;
		}

		if (tfs_req_curr != NULL) {
			tfs_req = tfs_req_curr;
		}
		else {
			if ((tfs_req = MALLOC(wlc->osh, sizeof(wnm_tfs_req_t))) == NULL) {
				WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				err = BCME_NOMEM;
				break;
			}
			bzero(tfs_req, sizeof(wnm_tfs_req_t));

			/* Mark this tfs_req is not ACKed by tfs resp from AP */
			tfs_req->tfs_resp_st = -1;

			tfs_req->tfs_id = wl_tfs_req.tfs_id;
			tfs_req->tfs_actcode = wl_tfs_req.tfs_actcode;

			WNM_ELEMENT_LIST_ADD(tfs_info, tfs_req_head, tfs_req);
			tfs_info->tfs_req_num++;
		}

		/* Add 1 TFS subelement with all stored TCLAS if ID != 0 */
		if (wl_tfs_req.tfs_subelem_id) {
			tfs_req_subelem = MALLOC(wlc->osh, sizeof(wnm_tfs_req_se_t));
			if (tfs_req_subelem == NULL) {
				WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				err = BCME_NOMEM;
				break;
			}
			bzero(tfs_req_subelem, sizeof(wnm_tfs_req_se_t));

			tfs_req_subelem->subelem_id = wl_tfs_req.tfs_subelem_id;

			/* Add this tfs_req_subelem into list */
			WNM_ELEMENT_LIST_ADD(tfs_req, subelem_head, tfs_req_subelem);
			tfs_req->subelem_num++;

			/* Move all tclas in tfs_info to tfs_req_subelem */
			if (tfs_info->tclas_head != NULL && tfs_info->tclas_num > 0) {
				wnm_tclas_t *tclas_curr;

				tfs_req_subelem->tclas_head = tfs_info->tclas_head;
				tfs_req_subelem->tclas_num = tfs_info->tclas_num;

				/* Reset the values */
				tfs_info->tclas_head = NULL;
				tfs_info->tclas_num = 0;

				/* Calculate the total length for this tfs_req_subelem */
				tfs_req_subelem->tclas_len = 0;
				tclas_curr = tfs_req_subelem->tclas_head;

				while (tclas_curr != NULL) {
					tfs_req_subelem->tclas_len += tclas_curr->fc_len +
						DOT11_TCLAS_IE_LEN;
					tclas_curr = tclas_curr->next;
				}

				if (tfs_req_subelem->tclas_num > 1) {
					tfs_req_subelem->tclas_proc.id = DOT11_MNG_TCLAS_PROC_ID;
					tfs_req_subelem->tclas_proc.length =
						DOT11_TCLAS_PROC_IE_LEN - TLV_HDR_LEN;
					tfs_req_subelem->tclas_proc.process =
						DOT11_TCLAS_PROC_MATCHONE;

					tfs_req_subelem->tclas_len += DOT11_TCLAS_PROC_IE_LEN;
				}
			}
		}

		if (wl_tfs_req.send && tfs_info->tfs_req_head != NULL) {
			err = wlc_wnm_send_tfs_req_frame(wnm);
		}

		break;
	}
	case IOV_SVAL(IOV_WNM_DMSDESC_ADD): {
		wnm_dms_info_t *dms_info = &wnm->dms_info;
		dms_desc_t *dms_desc;
		bool send = *(bool *)arg;

		if (!BSSCFG_STA(bsscfg) || !WNM_DMS_ENABLED(wnm_cfg->cap)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if (!bsscfg->associated) {
			err = BCME_NOTASSOCIATED;
			break;
		}

		if (dms_info->tclas_head == NULL) {
			err = BCME_UNSUPPORTED;
			break;
		}

		if ((dms_desc = MALLOC(wlc->osh, sizeof(dms_desc_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		bzero(dms_desc, sizeof(dms_desc_t));

		dms_desc->req_type = DOT11_DMS_REQ_TYPE_ADD;

		/* Mark this dms_desc since it's not ACKed by dms resp from AP */
		dms_desc->resp_type = -1;

		WNM_ELEMENT_LIST_ADD(dms_info, dms_desc_head, dms_desc);
		dms_info->dms_desc_num++;

		/* Move all tclas in dms_info to dms_desc */
		if (dms_info->tclas_head != NULL && dms_info->tclas_num > 0) {
			wnm_tclas_t *tclas_curr;

			dms_desc->tclas_head = dms_info->tclas_head;
			dms_desc->tclas_num = dms_info->tclas_num;

			/* Reset the values */
			dms_info->tclas_head = NULL;
			dms_info->tclas_num = 0;

			/* Calculate the total tclas length in this dms_desc */
			dms_desc->tclas_len = 0;
			tclas_curr = dms_desc->tclas_head;
			while (tclas_curr != NULL) {
				dms_desc->tclas_len += tclas_curr->fc_len +
					DOT11_TCLAS_IE_LEN;
				tclas_curr = tclas_curr->next;
			}

			if (dms_desc->tclas_num > 1) {
				dms_desc->tclas_proc.id = DOT11_MNG_TCLAS_PROC_ID;
				dms_desc->tclas_proc.length = 1;
				dms_desc->tclas_proc.process = 1;

				dms_desc->tclas_len += DOT11_TCLAS_PROC_IE_LEN;
			}
		}

		if (send && dms_info->dms_desc_head != NULL) {
			err = wlc_wnm_send_dms_req_frame(wnm);
		}

		break;
	}
#endif /* STA */
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static wnm_bsscfg_cubby_t*
wlc_wnm_trans_candidate_list_alloc(wlc_wnm_info_t *wnm, wlc_bsscfg_t *cfg)
{
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);

	if (wnm_cfg && (wnm_cfg->trans_candidate_list == NULL)) {
		wnm_cfg->trans_candidate_list = (transition_candidate_list_t*)MALLOC(wnm->wlc->osh,
			sizeof(transition_candidate_list_t));
		wnm_cfg->trans_candidate_list->count = 0;
	}
	return wnm_cfg;
}

int
wlc_wnm_get_trans_candidate_list_pref(wlc_info_t *wlc, wlc_bsscfg_t *cfg, struct ether_addr *bssid)
{
	wlc_wnm_info_t* wnm = wlc->wnm_info;
	uint32 i;
	int pref = -1;
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;

	if (wnm != NULL)
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);

	if ((wnm_cfg != NULL) && (wnm_cfg->trans_candidate_list != NULL)) {
		transition_candidate_list_t* trans_candidate_list;
		trans_candidate_list = wnm_cfg->trans_candidate_list;

		for (i = 0; i < trans_candidate_list->count; i++) {
			if (bcmp((void *)bssid,
				(void*)&trans_candidate_list->trans_candidate[i].bssid,
				ETHER_ADDR_LEN) == 0) {
				pref = trans_candidate_list->trans_candidate[i].preference;
				break;
			}
		}
		/* not found in list */
		if (trans_candidate_list->mode & DOT11_BSS_TRNS_REQMODE_ABRIDGED) {
			pref = 0;
		}
	}

	return pref;
}


/* bsscfg cubby */
static int
wlc_wnm_bsscfg_init(void *context, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t* wnm = (wlc_wnm_info_t*)context;
	wlc_info_t *wlc = wnm->wlc;
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
	uint32 cap = 0;
	int err = BCME_OK;

	wnm_cfg->trans_candidate_list = NULL;

	/*
	 * init default setup.  Add default capability here
	 */
	/* enabling BSS transition */
	cap |= WL_WNM_BSSTRANS;

	/* BSS Max Idle Period derault disabled */
	wnm_cfg->bss_max_idle_period = 0;
	wnm_cfg->bss_idle_opt = 0;

	wnm_cfg->tim_bcast_offset = 0;
#ifdef AP
	/* init TIM Broadcast timer */
	if ((wnm_cfg->tim_bcast_timer = wl_init_timer(wlc->wl, wlc_wnm_tim_bcast_timer, cfg,
		"wnm_tim_bcast_timer")) == NULL) {
		WL_ERROR(("wl%d: %s: tim_bcast_timer init failed\n",
			wlc->pub->unit, __FUNCTION__));
		err = BCME_ERROR;
		goto done;
	}
	wnm_cfg->perm_cb = 0;
	wnm_cfg->temp_cb = 0;
#endif /* AP */
#ifdef STA
	wnm_cfg->tim_bcast_interval = 0;
	wnm_cfg->tim_bcast_status = 0;
#endif /* STA */

	if (WLWNM_ENAB(wlc->pub)) {
		/* enable extend capability */
		wlc_wnm_set_cap(wlc, cfg, cap);
	}
#ifdef AP
done:
#endif /* AP */
	return err;
}

static void
wlc_wnm_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t* wnm = (wlc_wnm_info_t*)context;

	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);

	if (wnm_cfg && (wnm_cfg->trans_candidate_list != NULL)) {
		MFREE(wnm->wlc->osh, wnm_cfg->trans_candidate_list,
			sizeof(transition_candidate_list_t));
		wnm_cfg->trans_candidate_list = NULL;
	}
#ifdef AP
	if (wnm_cfg->tim_bcast_timer) {
		wl_free_timer(wnm->wlc->wl, wnm_cfg->tim_bcast_timer);
		wnm_cfg->tim_bcast_timer = NULL;
	}
#endif /* AP */
}

void
wlc_wnm_clear_scbstate(void *p)
{
#if defined(PROXYARP) || defined(AP)
	struct scb *scb = (struct scb *)p;
#endif /* PROXYARP || AP */
#ifdef PROXYARP
	_proxyarp_watchdog(FALSE, (uint8 *)&scb->ea);
#endif /* PROXYARP */
#ifdef AP
	scb->rx_tstamp = 0;
	scb->tim_bcast_status = 0;
	scb->tim_bcast_interval = 0;
	scb->tim_bcast_high_rate = 0;
	wlc_wnm_tfs_req_free(scb->bsscfg->wlc->wnm_info, &scb->tfs_list, -1);
	wlc_wnm_dms_housekeeping(scb, -1);
#endif /* AP */
}

uint32
wlc_wnm_get_cap(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{

	wlc_wnm_info_t* wnm;
	wnm_bsscfg_cubby_t *wnm_cfg;
	uint32 cap = 0;

	wnm = wlc->wnm_info;
	if (wnm != NULL) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
		ASSERT(wnm_cfg != NULL);

		cap = wnm_cfg->cap;
	}
	return cap;
}

uint32
wlc_wnm_set_cap(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint32 cap)
{
	wlc_wnm_info_t* wnm;
	wnm_bsscfg_cubby_t *wnm_cfg;

	wnm = wlc->wnm_info;
	if (wnm != NULL) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg);
		ASSERT(wnm_cfg != NULL);

		wnm_cfg->cap = cap;

		/* ext_cap 11-th bit for FMS service */
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_FMS, WNM_FMS_ENABLED(cap));
#ifdef PROXYARP
		/* ext_cap 12-th bit for proxyarp */
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_PROXY_ARP, WNM_PROXYARP_ENABLED(cap));
		if (proxyarp_get() != WNM_PROXYARP_ENABLED(cap)) {
			proxyarp_set(WNM_PROXYARP_ENABLED(cap));

			/* clean up cache if proxyarp was turned off */
			if (WNM_PROXYARP_ENABLED(cap) == FALSE)
				_proxyarp_watchdog(TRUE, NULL);
		}
#endif /* PROXYARP */
		/* ext_cap 16-th bit for TFS service */
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_TFS, WNM_TFS_ENABLED(cap));

		/* ext_cap 17-th bit for WNM-Sleep */
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_WNM_SLEEP, WNM_WNM_SLEEP_ENABLED(cap));

		/* ext_cap 18-th bit for TIM Broadcast */
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_TIM_BCAST, WNM_TIM_BCAST_ENABLED(cap));

		/* ext_cap 19-th bit for BSS Transition */
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_BSS_TRANSITION_MGMT,
			WNM_BSSTRANS_ENABLED(cap));

		/* ext_cap 26-th bit for DMS service */
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_DMS, WNM_DMS_ENABLED(cap));

		/* ext_cap 46-th bit for WNM-Notification */
		wlc_bsscfg_set_ext_cap(bsscfg, DOT11_EXT_CAP_WNM_NOTIF, WNM_NOTIF_ENABLED(cap));

		/* finall setup beacon and probe response ext cap */
		if (bsscfg->up &&
		    (BSSCFG_AP(bsscfg) || (!bsscfg->BSS && !BSS_TDLS_ENAB(wlc, bsscfg)))) {
			/* update AP or IBSS beacons */
			wlc_bss_update_beacon(wlc, bsscfg);
			/* update AP or IBSS probe responses */
			wlc_bss_update_probe_resp(wlc, bsscfg, TRUE);
		}
	}

	return BCME_OK;
}

#ifdef BCMDBG
static void
wlc_wnm_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	wlc_wnm_info_t* wnm = (wlc_wnm_info_t*)context;
	wnm_bsscfg_cubby_t *wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);

	ASSERT(wnm_cfg != NULL);

	if (wnm_cfg->trans_candidate_list)
		bcm_bprintf(b, "     trans_candidate_list: %p trans_candidate_list->count: %d\n",
			wnm_cfg->trans_candidate_list, wnm_cfg->trans_candidate_list->count);
}
#endif
#ifdef PROXYARP
bool
wlc_wnm_proxyarp(wlc_info_t *wlc)
{
	return proxyarp_get();
}
#endif /* PROXYARP */

uint16
wlc_wnm_bss_max_idle_prd(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t* wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;

	if (wnm != NULL) {
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);
		if (wnm_cfg != NULL)
			return wnm_cfg->bss_max_idle_period;
	}

	return 0;
}

static void
wlc_wnm_tfs_req_free(wlc_wnm_info_t *wnm, void *head, int tfs_id)
{
	wnm_tfs_req_t **tfs_req_head = (wnm_tfs_req_t **)head;
	wnm_tfs_req_t *tfs_req_curr = *tfs_req_head;
	wnm_tfs_req_t *tfs_req_prev = NULL;
	wnm_tfs_req_t *tfs_req_d = NULL;

	/* Free all tfs req or the matched tfs req */
	while (tfs_req_curr != NULL) {
		if (tfs_id < 0 || tfs_id == tfs_req_curr->tfs_id) {
			tfs_req_d = tfs_req_curr;
			if (tfs_req_prev == NULL)
				*tfs_req_head = tfs_req_curr->next;
			else
				tfs_req_prev->next = tfs_req_curr->next;
		}
		else {
			tfs_req_prev = tfs_req_curr;
		}

		tfs_req_curr = tfs_req_curr->next;

		if (tfs_req_d) {
			wnm_tfs_req_se_t *tfs_subelem_curr, *tfs_subelem_prev;
			wnm_tclas_t *curr, *prev;

			WL_ERROR(("wl%d: %s: free tfs req %p with tfs id %d\n",
				wnm->wlc->pub->unit, __FUNCTION__, tfs_req_d, tfs_req_d->tfs_id));

			tfs_subelem_curr = tfs_req_d->subelem_head;

			while (tfs_subelem_curr != NULL) {
				/* Free tclas in tfs_subelem */
				curr = tfs_subelem_curr->tclas_head;
				WNM_TCLAS_FREE(wnm, curr, prev);

				/* Move to the next tfs_subelem and free the current one */
				tfs_subelem_prev = tfs_subelem_curr;
				tfs_subelem_curr = tfs_subelem_curr->next;
				MFREE(wnm->wlc->osh, tfs_subelem_prev, sizeof(wnm_tfs_req_se_t));
			}
			MFREE(wnm->wlc->osh, tfs_req_d, sizeof(wnm_tfs_req_t));
			tfs_req_d = NULL;

			if (tfs_id > 0)
				return;
		}
	}
}

#ifdef AP
uint32
wlc_wnm_timestamp(wlc_wnm_info_t *wnm)
{
	if (wnm)
		return wnm->timestamp;
	return 0;
}

void
wlc_process_tim_bcast_req_ie(wlc_info_t *wlc, uint8 *tlvs, int len, struct scb *scb)
{
	dot11_tim_bcast_req_ie_t *ie_tlv;
	wnm_bsscfg_cubby_t *wnm_cfg;

	if (wlc->wnm_info == NULL)
		return;

	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, scb->bsscfg);
	ASSERT(wnm_cfg != NULL);

	ASSERT(scb != NULL);

	ie_tlv = (dot11_tim_bcast_req_ie_t *)
		 bcm_parse_tlvs(tlvs, len, DOT11_MNG_TIM_BCAST_REQ_ID);

	if (!ie_tlv)
		return;
	if (ie_tlv->id != DOT11_MNG_TIM_BCAST_REQ_ID)
		return;
	if (ie_tlv->length != DOT11_TIM_BCAST_REQ_IE_LEN)
		return;

	/*
	 * If interval == 0, driver should accept this request anyway.
	 * Otherwise, check if driver had setup offset already.
	 */

	if (wnm_cfg->tim_bcast_offset == 0) {
		scb->tim_bcast_status = DOT11_TIM_BCAST_STATUS_DENY;
		scb->tim_bcast_interval = 0;
	}
	else {
		if (ie_tlv->tim_bcast_interval == 0)
			scb->tim_bcast_status = DOT11_TIM_BCAST_STATUS_ACCEPT;
		else
			scb->tim_bcast_status = DOT11_TIM_BCAST_STATUS_ACCEPT_TSTAMP;

		scb->tim_bcast_interval = ie_tlv->tim_bcast_interval;
		scb->tim_bcast_high_rate = wlc_scb_ratesel_get_primary(wlc, scb, NULL);
	}
}

int
wlc_wnm_get_tim_bcast_resp_ie(wlc_info_t *wlc, uint8 *p, int *plen, struct scb *scb)
{
	dot11_tim_bcast_resp_ie_t *ie = (dot11_tim_bcast_resp_ie_t *)p;
	int err = BCME_OK;

	ie->id = DOT11_MNG_TIM_BCAST_RESP_ID;
	ie->status = scb->tim_bcast_status;

	switch (ie->status) {
	case DOT11_TIM_BCAST_STATUS_ACCEPT:
	case DOT11_TIM_BCAST_STATUS_DENY:
		ie->length = DOT11_TIM_BCAST_DENY_RESP_IE_LEN;
		break;
	case DOT11_TIM_BCAST_STATUS_ACCEPT_TSTAMP:
	case DOT11_TIM_BCAST_STATUS_OVERRIDEN: {
		wnm_bsscfg_cubby_t *wnm_cfg;
		uint16 high_rate = wlc_rate_rspec2rate(scb->tim_bcast_high_rate)/500;

		wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, scb->bsscfg);
		ASSERT(wnm_cfg != NULL);

		ie->length = DOT11_TIM_BCAST_ACCEPT_RESP_IE_LEN;
		ie->tim_bcast_interval = scb->tim_bcast_interval;
		htol32_ua_store(wnm_cfg->tim_bcast_offset, (uint8*)&(ie->tim_bcast_offset));
		/* in unit of 0.5 Mb/s */
		htol16_ua_store(high_rate, (uint8*)&(ie->high_rate));
		/* in unit of 0.5 Mb/s */
		htol16_ua_store(2, (uint8*)&(ie->low_rate));

		break;
	}
	default:
		err = BCME_ERROR;
		break;
	}

	*plen = ie->length;

	return err;
}

static int
wlc_wnm_tbttcnt_mod(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, int interval)
{
	uint32 bp, tsf_h, tsf_l;
	uint32 k, btklo, btkhi, offset;

	wlc_read_tsf(wlc, &tsf_l, &tsf_h);
	bp = bsscfg->current_bss->beacon_period * interval;

	btklo = (tsf_h << 22) | (tsf_l >> 10);
	btkhi = tsf_h >> 10;

	/* offset = BTk % BP */
	offset = btklo % bp;

	/* K[2] = ((2^16 % BP) * 2^16) % BP */
	k = (uint32)(1<<16) % bp;
	k = (uint32)(k * 1<<16) % (uint32)bp;

	/* offset += (BTk[2] * K[2]) % BP */
	offset += ((btkhi & 0xffff) * k) % bp;

	/* BTk[3] */
	btkhi = btkhi >> 16;

	/* k[3] = (K[2] * 2^16) % BP */
	k = (k << 16) % bp;

	/* offset += (BTk[3] * K[3]) % BP */
	offset += ((btkhi & 0xffff) * k) % bp;

	offset = offset % bp;
	offset = offset / 100;

	return offset;
}

void
wlc_wnm_tbtt(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *scb;
	struct scb_iter scbiter;
	wnm_bsscfg_cubby_t *wnm_cfg;
	bool flush_tim = FALSE;
	uint16 high_rate = 65535;
	ratespec_t high_ratespec;

	if (wlc->wnm_info == NULL)
		return;

	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	ASSERT(wnm_cfg != NULL);

	/* no need to send tim broadcast since it is aligned to TBTT */
	if (wnm_cfg->tim_bcast_offset == 0)
		return;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (scb->tim_bcast_interval) {
			if (wlc_wnm_tbttcnt_mod(wlc, bsscfg, scb->tim_bcast_interval) == 0) {
				int scb_rate = wlc_rate_rspec2rate(scb->tim_bcast_high_rate)/500;

				flush_tim = TRUE;
				if (high_rate > scb_rate) {
					high_rate = scb_rate;
					high_ratespec = scb->tim_bcast_high_rate;
				}
			}
		}
	}

	if (flush_tim) {
		wnm_bsscfg_cubby_t *wnm_cfg;

		wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
		ASSERT(wnm_cfg != NULL);
		wnm_cfg->tim_bcast_high_rate = high_rate;

		wl_add_timer(wlc->wl, wnm_cfg->tim_bcast_timer, wnm_cfg->tim_bcast_offset, FALSE);
	}
}

void
wlc_wnm_update_checkbeacon(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool perm_cb, bool temp_cb)
{
	wnm_bsscfg_cubby_t *wnm_cfg;

	if (wlc->wnm_info == NULL)
		return;

	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	ASSERT(wnm_cfg != NULL);
	wnm_cfg->perm_cb = perm_cb;
	wnm_cfg->temp_cb = temp_cb;
}

bool
wlc_wnm_dms_amsdu_on(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;
	if (wnm && cfg)
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);

	/* 802.11v-dms requests to aggregate frame in amsdu */
	if (AMSDU_ENAB(wlc->pub) &&
	    WLWNM_ENAB(wlc->pub) &&
	    wnm_cfg &&
	    WNM_DMS_ENABLED(wnm_cfg->cap) &&
	    wnm_cfg->dms_info.dms_desc_head != NULL) {
		return TRUE;
	}

	return FALSE;
}

static void
wlc_wnm_bss_idle_timer(void *context)
{
	wlc_wnm_info_t *wnm = (wlc_wnm_info_t *)context;
	wlc_info_t *wlc = wnm->wlc;
	struct scb_iter scbiter;
	struct scb *scb;

	wnm->timestamp++;

	if (wlc->pub->_wnm != TRUE)
		return;

	/* BSS Max Idle Period check in 100ms scale */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_bsscfg_t *cfg = SCB_BSSCFG(scb);
		/* 1000TU in 100ms scale */
		uint32 bss_max_idle_prd = (wlc_wnm_bss_max_idle_prd(wlc, cfg) * 1280) / 125;

		if (BSSCFG_AP(cfg) && SCB_ASSOCIATED(scb) && bss_max_idle_prd &&
			(wnm->timestamp - scb->rx_tstamp) > bss_max_idle_prd) {
#ifdef BCMDBG
			char eabuf[ETHER_ADDR_STR_LEN];
			WL_ASSOC(("wl%d: BSS Max Idle Period timeout(%d)(%d - %d)."
				" Disassoc(%s)\n", wlc->pub->unit, bss_max_idle_prd,
				scb->rx_tstamp, wnm->timestamp,
				bcm_ether_ntoa(&scb->ea, eabuf)));
#endif /* BCMDBG */
			wlc_senddisassoc(wlc, &scb->ea, &cfg->BSSID,
				&cfg->cur_etheraddr, scb, DOT11_RC_INACTIVITY);
			wlc_scb_resetstate(scb);
			wlc_scb_setstatebit(scb, AUTHENTICATED);

			wlc_bss_mac_event(wlc, cfg, WLC_E_DISASSOC_IND, &scb->ea,
				WLC_E_STATUS_SUCCESS, DOT11_RC_INACTIVITY, 0, 0, 0);
		}
	}
}

static int
wlc_wnm_send_tim_bcast_resp_frame(wlc_wnm_info_t *wnm_info, struct scb *scb, uint8 token)
{
	wlc_info_t *wlc = wnm_info->wlc;
	void *p;
	uint8 *pbody;
	int bodylen;
	int resp_len;
	dot11_tim_bcast_resp_t *resp;
	dot11_tim_bcast_resp_ie_t *resp_ie;
	int err = BCME_OK;

	bodylen = DOT11_TIM_BCAST_RESP_LEN + TLV_HDR_LEN + DOT11_TIM_BCAST_ACCEPT_RESP_IE_LEN;

	if ((p = wlc_frame_get_action(wlc, FC_ACTION, &scb->ea, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, bodylen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		err = BCME_ERROR;
		goto done;
	}

	/* Prepare TIM Broadcast frame fields */
	resp = (dot11_tim_bcast_resp_t *)pbody;
	resp->category = DOT11_ACTION_CAT_WNM;
	resp->action = DOT11_WNM_ACTION_TIM_BCAST_RESP;
	resp->token = token;

	resp_ie = (dot11_tim_bcast_resp_ie_t *)&resp->data[0];
	resp_len = 0;

	if (wlc_wnm_get_tim_bcast_resp_ie(wlc, (uint8 *)resp_ie, &resp_len, scb) == BCME_OK) {
		PKTSETLEN(wlc->osh, p, DOT11_TIM_BCAST_RESP_LEN + TLV_HDR_LEN +
			resp_len + DOT11_MGMT_HDR_LEN);
	}
	else {
		/* error case.  free allocated packet */
		PKTFREE(wlc->osh, p, FALSE);
		err = BCME_ERROR;
		goto done;
	}

	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, scb);
done:
	return err;
}

static void
wlc_wnm_tim_bcast_timer(void *context)
{
	wlc_bsscfg_t *bsscfg = (wlc_bsscfg_t *)context;
	wlc_info_t *wlc = bsscfg->wlc;
	dot11_tim_bcast_t *tim;
	uint32 tsf_l, tsf_h;
	void *p, *p_low;
	uint8 *pbody, *pbody_low;
	int bodylen;
	int tim_ie_len = 0;
	uint8 tim_ie[256] = {0};
	wnm_bsscfg_cubby_t *wnm_cfg;

	if (wlc->wnm_info == NULL)
		return;

	wnm_cfg = WNM_BSSCFG_CUBBY(wlc->wnm_info, bsscfg);
	ASSERT(wnm_cfg != NULL);

	tim_ie_len = wlc_get_tim(wlc, tim_ie, 256, WLC_BSSCFG_IDX(bsscfg));
	bodylen = DOT11_TIM_BCAST_LEN + tim_ie_len;

	if ((p = wlc_frame_get_action(wlc, FC_ACTION, &ether_bcast, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, bodylen, &pbody, DOT11_ACTION_CAT_UWNM)) == NULL) {
		return;
	}
	if ((p_low = wlc_frame_get_action(wlc, FC_ACTION, &ether_bcast, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, bodylen, &pbody_low, DOT11_ACTION_CAT_UWNM)) == NULL) {
		return;
	}

	/* Prepare TIM Broadcast frame fields */
	tim = (dot11_tim_bcast_t *)pbody;
	tim->category = DOT11_ACTION_CAT_UWNM;
	tim->action = DOT11_UWNM_ACTION_TIM;
	tim->check_beacon += ((wnm_cfg->perm_cb | wnm_cfg->temp_cb)? 1: 0);
	/* clear temporary check-beacon if already updated check-beacon value */
	wnm_cfg->temp_cb = 0;
	bcopy(tim_ie, &tim->data[0], tim_ie_len);

	/* put tfs read in the end to reduce inaccuracy */
	wlc_read_tsf(wlc, &tsf_l, &tsf_h);
	htol32_ua_store(tsf_l, (uint8 *)&tim->tsf[0]);
	htol32_ua_store(tsf_h, (uint8 *)&tim->tsf[4]);

	/* spec request to send high rate TIM Bcast first */
	wlc_queue_80211_frag(wlc, p, wlc->cfg->wlcif->qi, NULL, NULL, FALSE, NULL,
		wnm_cfg->tim_bcast_high_rate);

	bcopy(pbody, pbody_low, bodylen);
	wlc_sendmgmt(wlc, p_low, wlc->cfg->wlcif->qi, NULL);

	return;
}

static uint8
wlc_wnm_verdict_tfs_req(dot11_tfs_req_ie_t *ie)
{
	return DOT11_TFS_RESP_ST_ACCEPT;
}

static int
wlc_wnm_send_tfs_resp_frame(wlc_info_t *wlc, struct scb *scb, uint8 *tfs_resp_ie,
	int tfs_resp_ie_len, uint8 token)
{
	int maxlen;
	void *p;
	uint8 *pbody;
	int bodylen;
	dot11_tfs_resp_t *resp;

	maxlen = tfs_resp_ie_len + DOT11_TFS_RESP_LEN;

	if ((p = wlc_frame_get_action(wlc, FC_ACTION, &scb->ea, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, maxlen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		return BCME_ERROR;
	}

	/* Prepare TFS response frame fields */
	resp = (dot11_tfs_resp_t *)pbody;
	resp->category = DOT11_ACTION_CAT_WNM;
	resp->action = DOT11_WNM_ACTION_TFS_RESP;
	resp->token = token;

	bodylen = DOT11_TFS_RESP_LEN;

	/* Copy tfs response ie */
	if (tfs_resp_ie && tfs_resp_ie_len > 0) {
		bcopy(tfs_resp_ie, resp->data, tfs_resp_ie_len);
		bodylen += tfs_resp_ie_len;
	}
#ifdef BCMDBG
	prhex("Raw TFS Resp body", (uchar *)pbody, bodylen);
#endif /* BCMDBG */


#ifdef BCMDBG
	prhex("Raw TFS Resp", (uchar *)pbody - DOT11_MGMT_HDR_LEN, bodylen + DOT11_MGMT_HDR_LEN);
#endif /* BCMDBG */

	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, scb);

	return BCME_OK;
}

static int
wlc_wnm_parse_tfs_req_tclas(wlc_info_t *wlc, wnm_tfs_req_se_t *tfs_req_subelem, uint8 *body,
	uint8 body_len)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	bcm_tlv_t *tlv;
	dot11_tclas_ie_t *ie;
	dot11_tclas_proc_ie_t *proc_ie;
	wnm_tclas_t *tclas;

	while (p < p_end) {
		tlv = bcm_parse_tlvs(p, p_end - p, DOT11_MNG_TCLAS_ID);
		if (tlv == NULL)
			break;
		if ((p_end - p) < tlv->len)
			return BCME_ERROR;
		ie = (dot11_tclas_ie_t *)tlv;

		if ((tclas = MALLOC(wlc->osh, sizeof(wnm_tclas_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		bzero(tclas, sizeof(wnm_tclas_t));

		tclas->user_priority = ie->user_priority;
		/* Exclude user priority byte */
		tclas->fc_len = ie->length - 1;
		bcopy(ie->data, tclas->fc.fc_data, tclas->fc_len);

		tfs_req_subelem->tclas_num++;
		tfs_req_subelem->tclas_len += ie->length + TLV_HDR_LEN;

		WNM_ELEMENT_LIST_ADD(tfs_req_subelem, tclas_head, tclas);

		p += ie->length + TLV_HDR_LEN;
	}

	proc_ie = (dot11_tclas_proc_ie_t *)p;
	if (proc_ie->id == DOT11_MNG_TCLAS_PROC_ID && tfs_req_subelem->tclas_num > 1) {
		bcopy(p, &tfs_req_subelem->tclas_proc, DOT11_TCLAS_PROC_IE_LEN);
		tfs_req_subelem->tclas_len += DOT11_TCLAS_PROC_IE_LEN;
	}

	return BCME_OK;
}

static int
wlc_wnm_parse_tfs_req_subelem(wlc_info_t *wlc, wnm_tfs_req_t *tfs_req, uint8 *body, uint8 body_len)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	bcm_tlv_t *tlv;
	dot11_tfs_req_se_t *ie;
	wnm_tfs_req_se_t *subelem;

	while (p < p_end) {
		tlv = (bcm_tlv_t *)p;
		if (tlv->id == DOT11_TFS_SUBELEM_ID_TFS) {
			ie = (dot11_tfs_req_se_t *)p;

			if ((subelem = MALLOC(wlc->osh, sizeof(wnm_tfs_req_se_t))) == NULL) {
				WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
					wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				return BCME_NOMEM;
			}
			bzero(subelem, sizeof(wnm_tfs_req_se_t));

			subelem->subelem_id = ie->sub_id;

			wlc_wnm_parse_tfs_req_tclas(wlc, subelem, ie->data, ie->length);

			WNM_ELEMENT_LIST_ADD(tfs_req, subelem_head, subelem);

			tfs_req->subelem_num++;
			tfs_req->subelem_len += subelem->tclas_len;
		} else if (tlv->id == DOT11_TFS_SUBELEM_ID_VENDOR) {
			WL_ERROR(("wl%d: %s: Got TFS req vendor subelement from STA\n",
				wlc->pub->unit, __FUNCTION__));
		}

		p += tlv->len + TLV_HDR_LEN;
	}

	return BCME_OK;
}

static int
wlc_wnm_parse_tfs_req_ie(wlc_info_t *wlc, struct scb *scb, uint8 *body,
	int body_len, dot11_tfs_resp_ie_t *tfs_resp_ie, int *tfs_resp_ie_len)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	bcm_tlv_t *tlv;
	dot11_tfs_req_ie_t *req_ie;
	dot11_tfs_status_se_t *st_ie = (dot11_tfs_status_se_t *)tfs_resp_ie->data;
	wnm_tfs_req_t *tfs_req;
	wnm_tfs_req_t *tfs_req_head = NULL;
	uint8 status;

	*tfs_resp_ie_len = 0;
	while (p < p_end) {
		tlv = bcm_parse_tlvs(p, p_end - p, DOT11_MNG_TFS_REQUEST_ID);
		if (tlv == NULL)
			break;
		if ((p_end - p) < tlv->len)
			return BCME_ERROR;
		req_ie = (dot11_tfs_req_ie_t *)tlv;

		/* Verdict this tfs_req */
		status = wlc_wnm_verdict_tfs_req(req_ie);

		if (status == DOT11_TFS_RESP_ST_ACCEPT) {
			if ((tfs_req = MALLOC(wlc->osh, sizeof(wnm_tfs_req_t))) == NULL) {
				WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
				wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
				if (tfs_req_head)
					wlc_wnm_tfs_req_free(wlc->wnm_info, &tfs_req_head, -1);

				return BCME_NOMEM;
			}
			bzero(tfs_req, sizeof(wnm_tfs_req_t));

			tfs_req->tfs_id = req_ie->tfs_id;
			tfs_req->tfs_actcode = req_ie->tfs_actcode;

			wlc_wnm_parse_tfs_req_subelem(wlc, tfs_req, req_ie->data,
				req_ie->length - 2);

			/* Add this tfs_req into tfs_req_head */
			tfs_req->next = tfs_req_head;
			tfs_req_head = tfs_req;
		}

		/* length field only have one octec.  Can't greater than 256 */
		*tfs_resp_ie_len += 4;
		ASSERT(*tfs_resp_ie_len < 255);

		/* Fill the content of tfs resp ie including one status subelement */
		st_ie->id = DOT11_TFS_STATUS_SE_ID_TFS_ST;
		st_ie->length = 2;
		st_ie->resp_st = (int)status & 0xFF;
		st_ie->tfs_id = req_ie->tfs_id;

		/* Move to the next TFS Status subelement */
		st_ie += 1;

		p += req_ie->length + TLV_HDR_LEN;
	}

	tfs_resp_ie->id = DOT11_MNG_TFS_RESPONSE_ID;
	tfs_resp_ie->length = (uint8)*tfs_resp_ie_len;
	if (*tfs_resp_ie_len)
		*tfs_resp_ie_len += DOT11_TFS_RESP_IE_LEN;

	/* assigning tfs_req to appropriate tfs node */
	scb->tfs_list = (uint8 *)tfs_req_head;

	return BCME_OK;
}

static int
wlc_wnm_send_tfs_notify_frame(wlc_info_t *wlc, struct scb *scb, bcm_tlv_t *idlist)
{
	void *p;
	uint8 *pbody;
	int bodylen;
	dot11_tfs_notify_t *notify;
	bodylen = DOT11_TFS_NOTIFY_LEN + idlist->len;

	/* allocate action frame */
	if ((p = wlc_frame_get_action(wlc, FC_ACTION, &scb->ea, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, bodylen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		WL_ERROR(("WNM-Sleep allocate management frame Error\n"));
		return BCME_ERROR;
	}

	/* Construct TFS notify frame fields */
	notify = (dot11_tfs_notify_t *)pbody;
	notify->category = DOT11_ACTION_CAT_WNM;
	notify->action = DOT11_WNM_ACTION_TFS_NOTIFY;
	notify->num_tfs_id = idlist->len;
	bcopy(&idlist->data, &notify->data, idlist->len);

#ifdef BCMDBG
	prhex("Raw TFS notify body", (uchar *)pbody, bodylen);
#endif /* BCMDBG */

	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, scb);

	return BCME_OK;
}

static bool
wlc_wnm_tclas_comp_type0(uint8 mask, struct ether_header *eh, dot11_tclas_fc_0_eth_t *fc)
{
	bool conflict = FALSE;
	/* check SRC EA */
	if (mboolisset(mask, DOT11_TCLAS_MASK_0) &&
	    bcmp((void *)eh->ether_shost, (void *)fc->sa, ETHER_ADDR_LEN)) {
		conflict = TRUE;
		goto done_0;
	}
	/* check DST EA */
	if (mboolisset(mask, DOT11_TCLAS_MASK_1) &&
	    bcmp((void *)eh->ether_dhost, (void *)fc->da, ETHER_ADDR_LEN)) {
		conflict = TRUE;
		goto done_0;
	}
	/* check Next Proto */
	if (mboolisset(mask, DOT11_TCLAS_MASK_2) && eh->ether_type != fc->eth_type)
		conflict = TRUE;
done_0:
	return conflict;
}

static bool
wlc_wnm_tclas_comp_type1(uint8 mask, struct ipv4_hdr *iph, struct bcmudp_hdr *udph,
	dot11_tclas_fc_1_ipv4_t *fc)
{
	bool conflict = FALSE;

	if (mboolisset(mask, DOT11_TCLAS_MASK_0) && IP_VER(iph) != fc->version) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_1) &&
	    bcmp((void *)iph->src_ip, (void *)&fc->src_ip, IPV4_ADDR_LEN)) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_2) &&
	    bcmp((void *)iph->dst_ip, (void *)&fc->dst_ip, IPV4_ADDR_LEN)) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_3) && udph->src_port != fc->src_port) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_4) && udph->dst_port != fc->dst_port) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_5) && ((iph->tos >> 2) != (fc->dscp & 0x3f))) {
		conflict = TRUE;
		goto done_1;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_6) && iph->prot != fc->protocol)
		conflict = TRUE;
done_1:
	return conflict;
}

static bool
wlc_wnm_tclas_comp_type2(uint8 mask, uint16 tci, dot11_tclas_fc_2_8021q_t *fc)
{

	bool conflict = FALSE;
	/* check SRC EA */
	if (mboolisset(mask, DOT11_TCLAS_MASK_0) && tci != fc->tci)
		conflict = TRUE;

	return conflict;
}

static bool
wlc_wnm_tclas_comp_type3(uint8 *frame_data, int frame_len, uint16 filter_len,
	dot11_tclas_fc_3_filter_t *fc)
{
	bool conflict = FALSE;
	uint8 *filter_pattern = fc->data;
	uint8 *filter_mask = (uint8 *)(&fc->data[0] + filter_len);
	uint8 pattern;
	int idx;

	/* check frame length first */
	if ((filter_len + fc->offset) > frame_len) {
		conflict = 1;
		goto done_3;
	}

	frame_data += fc->offset;
	for (idx = 0; idx < filter_len; idx++) {
		pattern = filter_pattern[idx] & filter_mask[idx];
		if (frame_data[idx] != pattern) {
			conflict = TRUE;
			break;
		}
	}

done_3:
	return conflict;
}

#define wlc_wnm_tclas_comp_type4_v4(mask, iph, udph, pa4h) \
	wlc_wnm_tclas_comp_type1(mask, iph, udph, pa4h)

static bool
wlc_wnm_tclas_comp_type4_v6(uint8 mask, struct ipv6_hdr *ip6h, struct bcmudp_hdr *udph,
	dot11_tclas_fc_4_ipv6_t *fc)
{
	bool conflict = FALSE;

	if (mboolisset(mask, DOT11_TCLAS_MASK_0) && IP_VER(ip6h) != fc->version) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_1) &&
	    bcmp((void *)&ip6h->saddr, (void *)fc->saddr, IPV6_ADDR_LEN)) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_2) &&
	    bcmp((void *)&ip6h->daddr, (void *)fc->daddr, IPV6_ADDR_LEN)) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_3) && udph->src_port != fc->src_port) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_4) && udph->dst_port != fc->dst_port) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_5) &&
	    (((ip6h->priority << 4) | (ip6h->flow_lbl[0] & 0xf)) >> 2) != (fc->dscp & 0x3f)) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_6) && ip6h->nexthdr != fc->nexthdr) {
		conflict = TRUE;
		goto done_4;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_7) &&
	    bcmp((void *)ip6h->flow_lbl, (void *)fc->flow_lbl, 3)) {
		conflict = TRUE;
		goto done_4;
	}

done_4:
	return conflict;
}

static bool
wlc_wnm_tclas_comp_type5(uint8 mask, uint16 vlan_tag, dot11_tclas_fc_5_8021d_t *fc)
{

	bool conflict = FALSE;
	/* check SRC EA */
	if (mboolisset(mask, DOT11_TCLAS_MASK_0) && (vlan_tag >> 13) != fc->pcp) {
		conflict = TRUE;
		goto done_5;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_1) && (vlan_tag & 0x1000) != fc->cfi) {
		conflict = TRUE;
		goto done_5;
	}
	if (mboolisset(mask, DOT11_TCLAS_MASK_2) && (vlan_tag & 0xfff) != fc->vid) {
		conflict = TRUE;
	}

done_5:
	return conflict;
}

static bool
wlc_wnm_tclas_match(wnm_tclas_t *tclas, frame_proto_t *fp, bool allmatch)
{
	uint8 matched = 0;
	uint8 type, mask;
	bool conflict;

	while (tclas) {
		conflict = FALSE;
		type = tclas->fc.fc_data[0];
		mask = tclas->fc.fc_data[1];

		/* real matching frame and TCLAS parameters */
		switch (type) {
		case DOT11_TCLAS_FC_TYPE_0_ETH: {
			struct ether_header *eh = (struct ether_header *)fp->l2;
			dot11_tclas_fc_0_eth_t *fc = &tclas->fc.fc0_eth;

			/* skip non ether header */
			if (fp->l2_t != FRAME_L2_ETH_H) {
				conflict = TRUE;
				break;
			}

			conflict = wlc_wnm_tclas_comp_type0(mask, eh, fc);

			break;
		}
		case DOT11_TCLAS_FC_TYPE_1_IP: {
			struct ipv4_hdr *iph = (struct ipv4_hdr *)fp->l3;
			/* we use udhp here just because tcp/udp have the same port offset */
			struct bcmudp_hdr *udph = (struct bcmudp_hdr *)fp->l4;
			dot11_tclas_fc_1_ipv4_t *fc = &tclas->fc.fc1_ipv4;

			/* skip non ipv4 header */
			if (fp->l3_t != FRAME_L3_IP_H) {
				conflict = TRUE;
				break;
			}

			conflict = wlc_wnm_tclas_comp_type1(mask, iph, udph, fc);

			break;
		}
		case DOT11_TCLAS_FC_TYPE_2_8021Q: {
			struct ethervlan_header *evh = (struct ethervlan_header *)fp->l2;
			struct dot3_mac_llc_snapvlan_header *svh =
				(struct dot3_mac_llc_snapvlan_header *)fp->l2;
			dot11_tclas_fc_2_8021q_t *fc = &tclas->fc.fc2_8021q;
			uint16 tci;

			if ((fp->l2_t != FRAME_L2_ETHVLAN_H) && (fp->l2_t != FRAME_L2_SNAPVLAN_H)) {
				conflict = TRUE;
				break;
			}

			if (fp->l2_t == FRAME_L2_ETHVLAN_H)
				tci = evh->vlan_tag;
			else
				tci = svh->vlan_tag;

			conflict = wlc_wnm_tclas_comp_type2(mask, tci, fc);

			break;
		}
		case DOT11_TCLAS_FC_TYPE_3_OFFSET: {
			uint16 filter_len = (tclas->fc_len - 5)/2;
			dot11_tclas_fc_3_filter_t *fc = &tclas->fc.fc3_filter;

			/* reduce half to get real pattern length */
			conflict = wlc_wnm_tclas_comp_type3(fp->l3, fp->l3_len, filter_len, fc);

			break;
		}
		case DOT11_TCLAS_FC_TYPE_4_IP_HIGHER: {
			struct ipv4_hdr *iph = (struct ipv4_hdr *)fp->l3;
			struct ipv6_hdr *ip6h = (struct ipv6_hdr *)fp->l3;
			struct bcmudp_hdr *udph = (struct bcmudp_hdr *)fp->l4;
			dot11_tclas_fc_4_ipv4_t *pa4h = &tclas->fc.fc4_ipv4;
			dot11_tclas_fc_4_ipv6_t *pa6h = &tclas->fc.fc4_ipv6;

			if ((fp->l3_t != FRAME_L3_IP_H) && (fp->l3_t != FRAME_L3_IP6_H)) {
				conflict = TRUE;
				break;
			}

			if (pa4h->version == IP_VER_4)
				conflict = wlc_wnm_tclas_comp_type4_v4(mask, iph, udph, pa4h);
			else
				conflict = wlc_wnm_tclas_comp_type4_v6(mask, ip6h, udph, pa6h);

			break;
		}
		case DOT11_TCLAS_FC_TYPE_5_8021D: {
			struct ethervlan_header *evh = (struct ethervlan_header *)fp->l2;
			struct dot3_mac_llc_snapvlan_header *svh =
				(struct dot3_mac_llc_snapvlan_header *)fp->l2;
			dot11_tclas_fc_5_8021d_t *fc = &tclas->fc.fc5_8021d;
			uint16 vlan_tag;

			if ((fp->l2_t != FRAME_L2_ETHVLAN_H) && (fp->l2_t != FRAME_L2_SNAPVLAN_H)) {
				conflict = TRUE;
				break;
			}
			if (fp->l2_t == FRAME_L2_ETHVLAN_H)
				vlan_tag = ntoh16(evh->vlan_tag);
			else
				vlan_tag = ntoh16(svh->vlan_tag);

			conflict = wlc_wnm_tclas_comp_type5(mask, vlan_tag, fc);

			break;
		}
		default:
			conflict = TRUE;
			break;
		}

		if (conflict) {
			/* request to match all TCLAS but we missed it */
			if (allmatch)
				return FALSE;
		}
		else
			matched |= 1;

		tclas = tclas->next;
	}

	if (matched)
		return TRUE;

	return FALSE;
}

/* return true if the frame should be bypass */
static uint8
wlc_wnm_tfs_packet_handle(wlc_info_t *wlc, struct scb *scb, frame_proto_t *fp)
{
	uint8 buf[TFS_NOTIFY_IDLIST_MAX + 2];
	wnm_tfs_req_se_t *sub;
	int remove_id = -1;
	uint8 send_frame;
	bcm_tlv_t *idlist = (bcm_tlv_t *)buf;
	uint8 *idptr = idlist->data;
	wnm_tfs_req_t *tfs = NULL;

	tfs = (wnm_tfs_req_t *)scb->tfs_list;

	bzero(idlist, TFS_NOTIFY_IDLIST_MAX + 2);

	if (tfs)
		send_frame = 0;
	else
		send_frame = 1;

	/* iterate TFS requests */
	while (tfs) {
		/* init first TFS sub-emement */
		sub = tfs->subelem_head;

		/* iterate TFS subelements */
		while (sub) {
			/* iterate TCLAS with all match case */
			if (wlc_wnm_tclas_match(sub->tclas_head, fp, TRUE)) {
				send_frame = 1;
				/* save id for notify frame */
				if (tfs->tfs_actcode & DOT11_TFS_NOTIFY_ACT_NOTIFY) {
					*idptr++ = tfs->tfs_id;
					idlist->len++;
				}

				/* remove target tfs request */
				if (tfs->tfs_actcode & DOT11_TFS_NOTIFY_ACT_DEL)
					remove_id = tfs->tfs_id;

				/* go to next tfs request */
				break;
			}
			sub = sub->next;
		}

		tfs = tfs->next;

		/* remove target tfs request */
		if (remove_id > 0) {
			wlc_wnm_tfs_req_free(wlc->wnm_info, &scb->tfs_list, remove_id);
			remove_id = -1;
		}
	}

	/* send out notify frame if we have catch frame matching tfs rule with notify action */
	if (idlist->len)
		wlc_wnm_send_tfs_notify_frame(wlc, scb, idlist);

	return send_frame;
}

static int
wlc_wnm_tclas_num(uint8 *buf, int buf_len)
{
	uint8 *p = buf;
	uint8 *p_end = buf + buf_len;
	bcm_tlv_t *tlv;
	int num = 0;

	while (p < p_end) {
		tlv = bcm_parse_tlvs(p, (uint32)p_end - (uint32)p, DOT11_MNG_TCLAS_ID);
		if (tlv == NULL)
			break;
		num++;
		p += tlv->len + TLV_HDR_LEN;
	}
	return num;
}

static bool
wlc_wnm_match_tclas(wnm_tclas_t *tclas_head, dot11_tclas_ie_t *tclas_ie)
{
	wnm_tclas_t *tclas = tclas_head;

	while (tclas != NULL) {
		if ((bcmp(tclas_ie->data, tclas->fc.fc_data, tclas_ie->length - 1) == 0) &&
			tclas_ie->user_priority == tclas->user_priority)
			return TRUE;
		tclas = tclas->next;
	}
	return FALSE;
}

static dms_desc_t *
wlc_wnm_find_dms_desc_by_tclas(dms_desc_t *dms_desc_head, uint8 *data, int data_len)
{
	dms_desc_t *dms_desc = NULL;
	int tclas_num;
	int i;

	tclas_num = wlc_wnm_tclas_num(data, data_len);

	dms_desc = dms_desc_head;
	/* DMS descriptor exists and have tclas to do matching */
	while (dms_desc != NULL && tclas_num != 0) {
		if (dms_desc->tclas_num != tclas_num)
			goto next;

		for (i = 0; i < tclas_num; i++) {
			dot11_tclas_ie_t *tclas_ie = (dot11_tclas_ie_t *)data;

			if (wlc_wnm_match_tclas(dms_desc->tclas_head, tclas_ie) == FALSE)
				goto next;

			data += tclas_ie->length + TLV_HDR_LEN;
		}

		/* we found matched dms_descriptor */
		if (tclas_num > 1) {
			dot11_tclas_proc_ie_t *tclas_proc_ie;
			tclas_proc_ie = (dot11_tclas_proc_ie_t *)data;

			if (tclas_proc_ie->process != dms_desc->tclas_proc.process)
				goto next;
		}

		break;
next:
		/* Move to the next one */
		dms_desc = dms_desc->next;
	}

	return dms_desc;
}

static dms_desc_t *
wlc_wnm_find_dms_desc_by_id(dms_desc_t *dms_desc_head, uint8 id)
{
	dms_desc_t *dms_desc = dms_desc_head;

	/* check DMS id exist or not */
	while (dms_desc) {
		if (dms_desc->dms_id == id)
			break;
		dms_desc = dms_desc->next;
	}
	return dms_desc;
}

static int
wlc_wnm_verdict_dms_tclas_type(uint8 *data, int len)
{
	dot11_tclas_ie_t *ie;
	dot11_tclas_fc_t *fc;
	uint8 *p = data;
	uint8 *p_end = data + len;
	int result = BCME_OK;
	bool first_try = TRUE;

	while (p < p_end) {
		ie = (dot11_tclas_ie_t *)p;
		if (ie->id != DOT11_MNG_TCLAS_ID)
			break;

		/* disable first time check */
		first_try = FALSE;

		fc = (dot11_tclas_fc_t *)ie->data;

		if (fc->fchdr.type == DOT11_TCLAS_FC_TYPE_0_ETH) {
			/* deny non multicast request */
			if (!ETHER_ISMULTI(fc->fc0_eth.da)) {
				result = BCME_ERROR;
				break;
			}
		}
		else if (fc->fchdr.type == DOT11_TCLAS_FC_TYPE_1_IP) {
			uint32 ipv4 = ntoh32(fc->fc1_ipv4.dst_ip);

			/* deny non multicast request */
			if (!IPV4_ISMULTI(ipv4)) {
				result = BCME_ERROR;
				break;
			}
		}
		else if (fc->fchdr.type == DOT11_TCLAS_FC_TYPE_4_IP_HIGHER) {
			if (fc->fc4_ipv4.version == IP_VER_4) {
				uint32 ipv4 = ntoh32(fc->fc4_ipv4.dst_ip);

				/* deny non multicast request */
				if (!IPV4_ISMULTI(ipv4)) {
					result = BCME_ERROR;
					break;
				}
			}
			else if (fc->fc4_ipv6.version == IP_VER_6) {
				/* deny non multicast request */
				if (fc->fc4_ipv6.daddr[0] != 0xff) {
					result = BCME_ERROR;
					break;
				}
			}
			else {
				result = BCME_ERROR;
				break;
			}
		}
		else {
			result = BCME_ERROR;
			break;
		}

		p += ie->length + TLV_HDR_LEN;
	}

	/* No valid TCLAS entry exist, return ERROR */
	if (first_try)
		result = BCME_ERROR;

	return result;
}

static uint8
wlc_wnm_verdict_dms_req_desc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	wnm_dms_info_t *dms_info, dot11_dms_req_desc_t *desc_req)
{
	uint8 status = DOT11_DMS_RESP_TYPE_ACCEPT;
	switch (desc_req->type) {
	case DOT11_DMS_REQ_TYPE_ADD:
		/* WFA request to deny DMS when scb auth by CCMP */
		if (WSEC_ENABLED(bsscfg->wsec) && WSEC_AES_ENABLED(scb->wsec) &&
		    (scb->key != NULL) && (scb->key->algo == CRYPTO_ALGO_AES_CCM)) {
			status = DOT11_DMS_RESP_TYPE_DENY;
			break;
		}

		if (wlc_wnm_verdict_dms_tclas_type(desc_req->data, desc_req->length) != BCME_OK) {
			status = DOT11_DMS_RESP_TYPE_DENY;
			break;
		}

		break;
	case DOT11_DMS_REQ_TYPE_REMOVE:
		if (desc_req->length == 1 &&
		    wlc_wnm_find_dms_desc_by_id(dms_info->dms_desc_head, desc_req->id) != NULL)
			status = DOT11_DMS_RESP_TYPE_TERM;
		else
			status = DOT11_DMS_RESP_TYPE_DENY;

		break;
	case DOT11_DMS_REQ_TYPE_CHANGE: {
		dms_desc_t *target_dms;
		target_dms = wlc_wnm_find_dms_desc_by_id(dms_info->dms_desc_head, desc_req->id);

		if (target_dms == NULL) {
			status = DOT11_DMS_RESP_TYPE_DENY;
			break;
		}
		else {
			struct dms_scb *dmscb = target_dms->dms_scb_head;
			uint8 cnt = 0;
			/* avoid changing DMS rule if more than one SCB subscribing to it */
			while (dmscb) {
				dmscb = dmscb->next;
				cnt++;
			}
			if (cnt > 1) {
				status = DOT11_DMS_RESP_TYPE_DENY;
				break;
			}
		}

		if (wlc_wnm_verdict_dms_tclas_type(desc_req->data, desc_req->length) != BCME_OK) {
			status = DOT11_DMS_RESP_TYPE_DENY;
			break;
		}

		break;
	}
	default:
		status = DOT11_DMS_RESP_TYPE_DENY;

		break;
	}

	return status;
}

static void
wlc_wnm_free_dmsdesc(wlc_wnm_info_t *wnm, dms_desc_t *dms_desc)
{
	wnm_tclas_t *tclas_curr, *tclas_prev;
#ifdef AP
	struct dms_scb *dms_scb;
#endif /* AP */

	tclas_curr = dms_desc->tclas_head;
	WNM_TCLAS_FREE(wnm, tclas_curr, tclas_prev);

	if (dms_desc->tspec != NULL)
		MFREE(wnm->wlc->osh, dms_desc->tspec, DOT11_TSPEC_IE_LEN);

	if (dms_desc->subelem != NULL && dms_desc->subelem_len > 0)
		MFREE(wnm->wlc->osh, dms_desc->subelem, dms_desc->subelem_len);
#ifdef AP
	dms_scb = dms_desc->dms_scb_head;
	while (dms_scb) {
		struct dms_scb *next = dms_scb->next;
		MFREE(wnm->wlc->osh, dms_scb, sizeof(dms_scb_t));
		dms_scb = next;
	}
#endif /* AP */

	MFREE(wnm->wlc->osh, dms_desc, sizeof(dms_desc_t));
}

static int
wlc_wnm_parse_dms_desc_tclas(wlc_info_t *wlc, dms_desc_t *dms_desc, uint8 *body,
	uint8 body_len)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	bcm_tlv_t *tlv;
	dot11_tclas_ie_t *ie;
	dot11_tclas_proc_ie_t *proc_ie;
	wnm_tclas_t *tclas;

	while (p < p_end) {
		tlv = bcm_parse_tlvs(p, p_end - p, DOT11_MNG_TCLAS_ID);
		if (tlv == NULL)
			break;
		ie = (dot11_tclas_ie_t *)tlv;

		if ((tclas = MALLOC(wlc->osh, sizeof(wnm_tclas_t))) == NULL) {
			WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return BCME_NOMEM;
		}
		bzero(tclas, sizeof(wnm_tclas_t));

		tclas->user_priority = ie->user_priority;
		/* Exclude user priority byte */
		tclas->fc_len = ie->length - 1;
		bcopy(ie->data, tclas->fc.fc_data, tclas->fc_len);

		dms_desc->tclas_num++;
		dms_desc->tclas_len += ie->length + TLV_HDR_LEN;

		WNM_ELEMENT_LIST_ADD(dms_desc, tclas_head, tclas);

		p += ie->length + TLV_HDR_LEN;
	}

	proc_ie = (dot11_tclas_proc_ie_t *)p;
	if (proc_ie->id == DOT11_MNG_TCLAS_PROC_ID && dms_desc->tclas_num > 1) {
		bcopy(p, &dms_desc->tclas_proc, DOT11_TCLAS_PROC_IE_LEN);
		dms_desc->tclas_len += DOT11_TCLAS_PROC_IE_LEN;
	}

	/* Check TSPEC element */
	p = body + dms_desc->tclas_len;
	if (p < p_end) {
		tlv = bcm_parse_tlvs(p, p_end - p, DOT11_MNG_TSPEC_ID);
		if (tlv != NULL) {
			bcopy((void *)tlv, (void *)dms_desc->tspec, tlv->len + TLV_HDR_LEN);
			p = body + tlv->len + TLV_HDR_LEN;
		}
	}

	/* Check optional subelements */
	if (p < p_end) {
		bcopy((void *)p, (void *)dms_desc->subelem, (uint32)p_end - (uint32)p);
	}

	return BCME_OK;
}

static dms_desc_t *
wlc_wnm_add_dmsdesc(wlc_info_t *wlc, wlc_wnm_info_t *wnm_info, wnm_bsscfg_cubby_t* wnm_cfg,
	dot11_dms_req_desc_t *dms_req_desc)
{
	wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
	dms_desc_t *dms_desc;
	int st;

	if ((dms_desc = MALLOC(wlc->osh, sizeof(dms_desc_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __func__, MALLOCED(wlc->osh)));
		return NULL;
	}
	bzero(dms_desc, sizeof(dms_desc_t));

	if ((st = wlc_wnm_parse_dms_desc_tclas(wlc, dms_desc, dms_req_desc->data,
	    dms_req_desc->length - 1)) != BCME_OK) {
		wlc_wnm_free_dmsdesc(wnm_info, dms_desc);
		WL_ERROR(("wl%d: %s: ERROR parsing TCLAS within DMS Descriptor (%x)\n",
			wlc->pub->unit, __func__, st));

		return NULL;
	}

	WLC_WNM_UPDATE_DMSID(wnm_cfg->dms_info.dms_id);
	dms_desc->dms_id = wnm_cfg->dms_info.dms_id;
	/* currently we do not support lsc check in DMS response frame */
	dms_desc->lsc = DOT11_DMS_RESP_LSC_UNSUPPORTED;

	/* Add this dms_desc into dms_desc_head */
	WNM_ELEMENT_LIST_ADD(dms_info, dms_desc_head, dms_desc);

	return dms_desc;
}

static int
wlc_wnm_add_dmsdesc_scb(wlc_info_t *wlc, dms_desc_t *dms_desc, struct scb *scb)
{
	struct dms_scb *dms_scb;

	dms_scb = dms_desc->dms_scb_head;
	while (dms_scb) {
		if (bcmp(&dms_scb->scb->ea, &scb->ea, ETHER_ADDR_LEN) == 0)
			return BCME_OK;
		dms_scb = dms_scb->next;
	}

	/* This scb is not in the dms_desc's dms_scb list */
	if ((dms_scb = MALLOC(wlc->osh, sizeof(dms_scb_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_NOMEM;
	}
	bzero(dms_scb, sizeof(dms_scb_t));

	/* Attach this dms_scb into dms_desc's dms_scb list */
	dms_scb->scb = scb;
	WNM_ELEMENT_LIST_ADD(dms_desc, dms_scb_head, dms_scb);

	return BCME_OK;
}

static int
wlc_wnm_parse_dms_req_desc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, uint8 body_len, uint8 *buf, uint8 *buf_len)
{
	wlc_wnm_info_t *wnm_info = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = WNM_BSSCFG_CUBBY(wnm_info, bsscfg);
	wnm_dms_info_t *dms_info = &wnm_cfg->dms_info;
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	dot11_dms_req_desc_t *dms_req_desc;
	dms_desc_t *dms_desc;
	dot11_dms_resp_st_t *dms_resp_st = (dot11_dms_resp_st_t *)buf;
	uint8 status;
	int resp_len = 0;

	while (p < p_end) {
		dms_req_desc = (dot11_dms_req_desc_t *)p;

		status = wlc_wnm_verdict_dms_req_desc(wlc, bsscfg, scb, dms_info, dms_req_desc);

		switch (dms_req_desc->type) {
		case DOT11_DMS_REQ_TYPE_ADD: {
			WL_ERROR(("wl%d: dms descriptor req type ADD\n", wlc->pub->unit));
			if (status == DOT11_DMS_RESP_TYPE_ACCEPT) {
				/* Check if this dms_req_desc is already existing or not */
				dms_desc = wlc_wnm_find_dms_desc_by_tclas(dms_info->dms_desc_head,
					dms_req_desc->data, dms_req_desc->length - 1);

				/* create DMS rule and its associated TCLAS fc */
				if (dms_desc == NULL) {
					dms_desc = wlc_wnm_add_dmsdesc(wlc, wnm_info,
						wnm_cfg, dms_req_desc);
				}

				if (dms_desc != NULL) {
					/* associate SCB to specific DMS rule */
					wlc_wnm_add_dmsdesc_scb(wlc, dms_desc, scb);
					dms_resp_st->id = dms_desc->dms_id;
				}
				else {
					status = DOT11_DMS_RESP_TYPE_DENY;
					dms_resp_st->id = 0;
				}
			}
			else {
				dms_resp_st->id = 0;
			}

			dms_resp_st->type = status;
			dms_resp_st->lsc = DOT11_DMS_RESP_LSC_UNSUPPORTED;

			/* As the spec says, copy tclas, tclas processing, tspec and
			 * optional subelement from dms request desc.
			 */
			bcopy(dms_req_desc->data, dms_resp_st->data,
				dms_req_desc->length - 1);
			dms_resp_st->length = 3 + (dms_req_desc->length - 1);
			resp_len += dms_resp_st->length + TLV_HDR_LEN;

			break;
		}
		case DOT11_DMS_REQ_TYPE_REMOVE: {
			WL_ERROR(("wl%d: dms descriptor req type REMOVE\n", wlc->pub->unit));

			dms_resp_st->id = dms_req_desc->id;


			if (dms_resp_st->type == DOT11_DMS_RESP_TYPE_TERM) {
				dms_desc = wlc_wnm_find_dms_desc_by_id(dms_info->dms_desc_head,
					dms_req_desc->id);
				if (dms_desc != NULL) {
					dms_resp_st->lsc = dms_desc->lsc;
					wlc_wnm_dms_housekeeping(scb, dms_req_desc->id);
				}
				else {
					dms_resp_st->lsc = DOT11_DMS_RESP_LSC_UNSUPPORTED;
					status = DOT11_DMS_RESP_TYPE_DENY;
				}
			}
			else
				dms_resp_st->lsc = DOT11_DMS_RESP_LSC_UNSUPPORTED;

			dms_resp_st->type = status;
			dms_resp_st->length = 3;
			resp_len += DOT11_DMS_RESP_STATUS_LEN;

			break;
		}
		case DOT11_DMS_REQ_TYPE_CHANGE: {
			WL_ERROR(("wl%d: dms descriptor req type CHANGE\n", wlc->pub->unit));
			dms_resp_st->id = dms_req_desc->id;

			if (status == DOT11_DMS_RESP_TYPE_ACCEPT) {
				dms_desc_t *new_dmsdesc;

				dms_desc = wlc_wnm_find_dms_desc_by_id(dms_info->dms_desc_head,
					dms_req_desc->id);

				new_dmsdesc = wlc_wnm_add_dmsdesc(wlc, wnm_info, wnm_cfg,
					dms_req_desc);

				if (new_dmsdesc != NULL) {
					wlc_wnm_add_dmsdesc_scb(wlc, new_dmsdesc, scb);
					wlc_wnm_dms_housekeeping(scb, dms_resp_st->id);
					new_dmsdesc->dms_id = dms_resp_st->id;
				}
				else {
					dms_resp_st->lsc = DOT11_DMS_RESP_LSC_UNSUPPORTED;
					status = DOT11_DMS_RESP_TYPE_DENY;
				}
			}
			else
				dms_resp_st->lsc = DOT11_DMS_RESP_LSC_UNSUPPORTED;

			dms_resp_st->type = status;
			dms_resp_st->length = 3;
			resp_len += DOT11_DMS_RESP_STATUS_LEN;
			break;
		}
		default:
			WL_ERROR(("wl%d: reserved dms descriptor req_type %d\n",
				wlc->pub->unit, dms_req_desc->type));
			break;
		} /* switch (dms_req_desc->type) */

		ASSERT(resp_len < 255);

		/* Move to the next dms resp status */
		dms_resp_st = (dot11_dms_resp_st_t *)(buf + resp_len);

		p += dms_req_desc->length + TLV_HDR_LEN;
	}
	*buf_len = (uint8)resp_len;

	return BCME_OK;
}

static int
wlc_wnm_parse_dms_req_ie(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb,
	uint8 *body, int body_len, uint8 *buf, int *buf_len)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	bcm_tlv_t *tlv;
	dot11_dms_req_ie_t *req_ie;
	dot11_dms_resp_ie_t *resp_ie;
	int err = BCME_OK;

	*buf_len = 0;

	while (p < p_end) {
		tlv = bcm_parse_tlvs(p, p_end - p, DOT11_MNG_DMS_REQUEST_ID);
		if (tlv == NULL)
			break;
		req_ie = (dot11_dms_req_ie_t *)tlv;

		/* Fill dms response element id */
		resp_ie = (dot11_dms_resp_ie_t *)buf;
		resp_ie->id = DOT11_MNG_DMS_RESPONSE_ID;

		if (wlc_wnm_parse_dms_req_desc(wlc, bsscfg, scb, req_ie->data, req_ie->length,
			resp_ie->data, &resp_ie->length) != BCME_OK) {
			err = BCME_ERROR;
			break;
		}

		p += req_ie->length + DOT11_DMS_REQ_IE_LEN;

		buf += (DOT11_DMS_RESP_IE_LEN + resp_ie->length);
		*buf_len += (DOT11_DMS_RESP_IE_LEN + resp_ie->length);
	}

	return err;
}

static int
wlc_wnm_send_dms_resp_frame(wlc_info_t *wlc, struct scb *scb, uint8 *data, int len, uint8 token)
{
	int maxlen;
	void *p;
	uint8 *pbody;
	int bodylen;
	dot11_dms_resp_frm_t *resp;

	maxlen = len + DOT11_DMS_RESP_FRM_LEN;

	if ((p = wlc_frame_get_mgmt(wlc, FC_ACTION, &scb->ea, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, maxlen, &pbody)) == NULL) {
		return BCME_ERROR;
	}

	/* Prepare DMS response frame fields */
	resp = (dot11_dms_resp_frm_t *)pbody;
	resp->category = DOT11_ACTION_CAT_WNM;
	resp->action = DOT11_WNM_ACTION_DMS_RESP;
	resp->token = token;

	bodylen = DOT11_DMS_RESP_FRM_LEN;

	/* Copy dms response ie */
	bcopy(data, resp->data, len);
	bodylen += len;

#ifdef BCMDBG
	prhex("Raw DMS Resp body", (uchar *)pbody, bodylen);
#endif /* BCMDBG */

	/* Fix up packet length */
	PKTSETLEN(wlc->osh, p, bodylen + DOT11_MGMT_HDR_LEN);

#ifdef BCMDBG
	prhex("Raw DMS Resp", (uchar *)pbody - DOT11_MGMT_HDR_LEN, PKTLEN(wlc->osh, p));
#endif /* BCMDBG */

	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, scb);

	return BCME_OK;
}

static int
wlc_wnm_dms_housekeeping(struct scb *scb, int dms_id)
{
	wlc_wnm_info_t *wnm;
	wnm_bsscfg_cubby_t* wnm_cfg;
	wnm_dms_info_t *dms_info;
	dms_desc_t *prev_desc, *curr_desc, *del_desc;
	struct dms_scb *prev_dmscb, *curr_dmscb, *del_dmscb;

	if ((wnm = scb->bsscfg->wlc->wnm_info) == NULL)
		return BCME_OK;

	if ((wnm_cfg = WNM_BSSCFG_CUBBY(wnm, scb->bsscfg)) == NULL)
		return BCME_OK;

	if ((dms_info = &wnm_cfg->dms_info) == NULL)
		return BCME_OK;

	prev_desc = NULL;
	curr_desc = dms_info->dms_desc_head;

	while (curr_desc) {
		prev_dmscb = NULL;
		curr_dmscb = curr_desc->dms_scb_head;

		if (dms_id == -1 || curr_desc->dms_id == (uint8)dms_id) {
			/* iterative check and remove SCB that was removed */
			while (curr_dmscb) {
				if (curr_dmscb->scb == scb) {
					del_dmscb = curr_dmscb;
					if (prev_dmscb == NULL)
						curr_desc->dms_scb_head = curr_dmscb->next;
					else
						prev_dmscb = curr_dmscb;
					curr_dmscb = curr_dmscb->next;
					MFREE(wnm->wlc->osh, del_dmscb, sizeof(dms_scb_t));
				}
				else {
					prev_dmscb = curr_dmscb;
					curr_dmscb = curr_dmscb->next;
				}
			}
		}

		/* no scb subscribing to this DMS descriptor, free it */
		if (curr_desc->dms_scb_head == NULL) {
			del_desc = curr_desc;
			if (prev_desc == NULL)
				dms_info->dms_desc_head = curr_desc->next;
			else
				prev_desc->next = curr_desc->next;
			curr_desc = curr_desc->next;

			wlc_wnm_free_dmsdesc(wnm, del_desc);
		}
		else {
			/* switch to next descriptor */
			prev_desc = curr_desc;
			curr_desc = curr_desc->next;
		}
	}

	return BCME_OK;
}
#endif /* AP */
#ifdef STA
void
wlc_process_tim_bcast_resp_ie(wlc_info_t *wlc, uint8 *tlvs, int len, struct scb *scb)
{
}

int
wlc_wnm_set_bss_max_idle_prd(wlc_info_t *wlc, wlc_bsscfg_t *cfg, uint16 new_prd, uint8 new_opt)
{
	int err = BCME_OK;
#ifdef KEEP_ALIVE
	wlc_wnm_info_t* wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;

	if (wnm == NULL)
		err = BCME_ERROR;
	else
		wnm_cfg = WNM_BSSCFG_CUBBY(wnm, cfg);

	if (wnm_cfg != NULL) {
		uint8 buf[WL_KEEP_ALIVE_FIXED_LEN + sizeof(struct dot3_mac_llc_snap_header)] = {0};
		wl_keep_alive_pkt_t *ka = (wl_keep_alive_pkt_t *) buf;
		wnm_cfg->bss_max_idle_period = new_prd; /* in 1000TUs */
		wnm_cfg->bss_idle_opt = new_opt;

		/* TU to ms + reduce period by 5% to send null frame earlier (1024 * 0.95) */
		ka->period_msec = new_prd * 973;

		if (new_opt & DOT11_BSS_MAX_IDLE_PERIOD_OPT_PROTECTED) {
			/* LCC Test (802.2 5.4.1.1.3) as requested by WFA NPS testplan 6.3.2 */
			struct dot3_mac_llc_snap_header *pkt = (struct dot3_mac_llc_snap_header *)
				(buf + WL_KEEP_ALIVE_FIXED_LEN);
			/* OUI + type useless, but added to prevent runt frames discard */
			ka->len_bytes = sizeof(struct dot3_mac_llc_snap_header);
			memcpy(pkt->ether_dhost, &cfg->target_bss->BSSID, ETHER_ADDR_LEN);
			memcpy(pkt->ether_shost, &cfg->cur_etheraddr, ETHER_ADDR_LEN);

			pkt->length = hton16(8); /* LLC + OUI + Ethertype */
			pkt->ctl = 0xF3;
		}

		err = wlc_iovar_op(wlc, "keep_alive", NULL, 0, ka,
			WL_KEEP_ALIVE_FIXED_LEN + ka->len_bytes, IOV_SET, cfg->wlcif);
	}
#endif /* KEEP_ALIVE */
	return err;
}

void
wlc_process_bss_max_idle_period_ie(wlc_info_t *wlc, uint8 *tlvs, int len, struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	dot11_bss_max_idle_period_ie_t *ie_tlv;
	uint16 idle_period;

	ASSERT(scb != NULL);
	ASSERT(bsscfg != NULL);

	ie_tlv = (dot11_bss_max_idle_period_ie_t *)
		 bcm_parse_tlvs(tlvs, len, DOT11_MNG_BSS_MAX_IDLE_PERIOD_ID);

	if (!ie_tlv)
		return;

	if (ie_tlv->length != DOT11_BSS_MAX_IDLE_PERIOD_IE_LEN)
		return;

	idle_period = ltoh16_ua(&(ie_tlv->max_idle_period));
	wlc_wnm_set_bss_max_idle_prd(wlc, bsscfg, idle_period, ie_tlv->idle_opt);
}

static int
wlc_wnm_send_tim_bcast_req_frame(wlc_wnm_info_t *wnm, int int_val)
{
	wlc_info_t *wlc = wnm->wlc;
	wlc_bsscfg_t *bsscfg = wnm->wlc->cfg;
	void *p;
	uint8 *pbody;
	int bodylen;
	struct scb *scb;
	dot11_tim_bcast_req_t *req;
	dot11_tim_bcast_req_ie_t *req_ie;

	bodylen = DOT11_TIM_BCAST_REQ_LEN + TLV_HDR_LEN + DOT11_TIM_BCAST_REQ_IE_LEN;

	if ((p = wlc_frame_get_action(wlc, FC_ACTION, &wlc->cfg->BSSID, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, bodylen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		return BCME_ERROR;
	}

	/* Prepare TIM Broadcast frame fields */
	req = (dot11_tim_bcast_req_t *)pbody;
	req->category = DOT11_ACTION_CAT_WNM;
	req->action = DOT11_WNM_ACTION_TIM_BCAST_REQ;
	WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	req->token = wnm->req_token;

	req_ie = (dot11_tim_bcast_req_ie_t *)&req->data[0];
	req_ie->id = DOT11_MNG_TIM_BCAST_REQ_ID;
	req_ie->length = DOT11_TIM_BCAST_REQ_IE_LEN;
	req_ie->tim_bcast_interval = (uint8)int_val;

#ifdef BCMDBG
	prhex("Raw TIM-Broadcast Req body", (uchar *)pbody, bodylen);
#endif /* BCMDBG */


#ifdef BCMDBG
	prhex("Raw TIM-Broadcast Req", (uchar *)pbody - DOT11_MGMT_HDR_LEN,
		bodylen + DOT11_MGMT_HDR_LEN);
#endif /* BCMDBG */

	scb = wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID);
	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, scb);

	return BCME_OK;
}

static int
wlc_wnm_prep_tclas_ie(wnm_tclas_t *tclas_head, uint8 *buf)
{
	wnm_tclas_t *tclas = tclas_head;
	uint8 *p = buf;
	dot11_tclas_ie_t *ie;
	int retlen = 0;

	while (tclas != NULL) {
		ie = (dot11_tclas_ie_t *)p;
		ie->id = DOT11_MNG_TCLAS_ID;
		ie->length = tclas->fc_len + 1;
		ie->user_priority = tclas->user_priority;

		bcopy(tclas->fc.fc_data, ie->data, tclas->fc_len);

		p += ie->length + TLV_HDR_LEN;
		tclas = tclas->next;
		retlen += ie->length + TLV_HDR_LEN;
	}

	return retlen;
}

static int
wlc_wnm_tclas_delete(wlc_wnm_info_t *wnm_info, wnm_tclas_t **head, int32 idx)
{
	wnm_tclas_t *entry = *head;
	wnm_tclas_t *prev = NULL;
	int32 entry_idx = 0;

	while (entry != NULL) {
		if (entry_idx++ != idx) {
			prev = entry;
			entry = entry->next;
			continue;
		}

		if (prev != NULL)
			prev->next = entry->next;
		else
			*head = entry->next;

		MFREE(wnm_info->wlc->osh, entry, sizeof(wnm_tclas_t));

		return BCME_OK;
	}
	return BCME_ERROR;
}

static void
wlc_wnm_tfs_free(wlc_wnm_info_t *wnm)
{
	wnm_tclas_t *curr, *prev;

	/* Free tclas in tfs_info */
	curr = wnm->tfs_info.tclas_head;
	prev = NULL;
	WNM_TCLAS_FREE(wnm, curr, prev);

	/* Free tfs_req in tfs_info */
	wlc_wnm_tfs_req_free(wnm, &wnm->tfs_info.tfs_req_head, -1);

	ASSERT(wnm->tfs_info.tfs_req_head == NULL);
}

static int
wlc_wnm_parse_tfs_resp_subelem(wlc_wnm_info_t *wnm, uint8 *body, int body_len)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	bcm_tlv_t *tlv;
	dot11_tfs_status_se_t *ie;
	wnm_tfs_info_t *tfs_info = &wnm->tfs_info;
	wnm_tfs_req_t *tfs_req;

	while (p < p_end) {
		tlv = (bcm_tlv_t *)p;
		if (tlv->id == DOT11_TFS_STATUS_SE_ID_TFS_ST) {
			ie = (dot11_tfs_status_se_t *)p;

			/* Look for the tfs_req by tfs_id */
			tfs_req = tfs_info->tfs_req_head;
			while (tfs_req != NULL) {
				if (tfs_req->tfs_id == ie->tfs_id)
					break;
				tfs_req = tfs_req->next;
			}

			if (tfs_req != NULL) {
				WL_ERROR(("wl%d: %s: Got tfs resp st %d from AP for tfs id %d\n",
					wnm->wlc->pub->unit, __FUNCTION__, ie->resp_st,
					ie->tfs_id));
				/* Update the tfs resp status */
				tfs_req->tfs_resp_st = (int)ie->resp_st & 0xFF;
			} else {
				WL_ERROR(("wl%d: %s: No matched tfs req for tfs id %d from AP\n",
					wnm->wlc->pub->unit, __FUNCTION__, ie->tfs_id));
			}
		} else {
			WL_ERROR(("wl%d: %s: Got tfs status subelement id %d from AP\n",
				wnm->wlc->pub->unit, __FUNCTION__, tlv->id));
		}

		p += tlv->len + TLV_HDR_LEN;
	}

	return BCME_OK;
}

static int
wlc_wnm_parse_tfs_resp_ie(wlc_wnm_info_t *wnm, uint8 *body, int body_len)
{
	uint8 *p = body;
	uint8 *p_end = body + body_len;
	bcm_tlv_t *tlv;
	dot11_tfs_resp_ie_t *ie;

	while (p < p_end) {
		tlv = bcm_parse_tlvs(p, p_end - p, DOT11_MNG_TFS_RESPONSE_ID);
		if (tlv == NULL)
			break;
		if ((p_end - p) < tlv->len)
			return BCME_ERROR;
		ie = (dot11_tfs_resp_ie_t *)tlv;

		wlc_wnm_parse_tfs_resp_subelem(wnm, ie->data, ie->length);

		p += ie->length + TLV_HDR_LEN;
	}

	return BCME_OK;
}

static int
wlc_wnm_prep_tfs_subelem_ie(wnm_tfs_req_t *tfs_req, uint8 *buf)
{
	uint8 *p = buf;
	wnm_tfs_req_se_t *tfs_subelem;
	dot11_tfs_req_se_t *ie;
	int retlen = 0, tclas_retlen;

	tfs_subelem = tfs_req->subelem_head;
	while (tfs_subelem != NULL) {
		ie = (dot11_tfs_req_se_t *)p;

		ie->sub_id = tfs_subelem->subelem_id;
		ie->length = (uint8)tfs_subelem->tclas_len;

		tclas_retlen = wlc_wnm_prep_tclas_ie(tfs_subelem->tclas_head, ie->data);

		if (tfs_subelem->tclas_num > 1) {
			bcopy(&tfs_subelem->tclas_proc, p + 2 + tclas_retlen,
				DOT11_TCLAS_PROC_IE_LEN);
		}

		p += ie->length + TLV_HDR_LEN;
		tfs_subelem = tfs_subelem->next;
		retlen += ie->length + TLV_HDR_LEN;
	}

	return retlen;
}

static int
wlc_wnm_prep_tfs_req_ie(wnm_tfs_info_t *tfs_info, uint8 *buf)
{
	wnm_tfs_req_t *tfs_req;
	dot11_tfs_req_ie_t *ie;
	uint8 *p = buf;
	int retlen = 0;

	tfs_req = tfs_info->tfs_req_head;
	while (tfs_req != NULL) {
		/* Skip this tfs_req since it's already ACKed by tfs resp from AP */
		if (tfs_req->tfs_resp_st >= 0)
			break;
		ie = (dot11_tfs_req_ie_t *)p;
		ie->id = DOT11_MNG_TFS_REQUEST_ID;
		/* Include two bytes (tfs id and tfs action code) */
		ie->length = tfs_req->subelem_len + 2;
		ie->tfs_id = tfs_req->tfs_id;
		ie->tfs_actcode = tfs_req->tfs_actcode;

		/* Prepare tfs subelement IEs */
		wlc_wnm_prep_tfs_subelem_ie(tfs_req, ie->data);

		p += ie->length + TLV_HDR_LEN;
		tfs_req = tfs_req->next;

		retlen += ie->length + TLV_HDR_LEN;
	}

	return retlen;
}

static int
wlc_wnm_get_tfs_req_ie_len(wnm_tfs_info_t *tfs_info)
{
	int retlen = 0;
	wnm_tfs_req_t *tfs_req;
	wnm_tfs_req_se_t *tfs_subelem;

	/* Calculate the total length */
	tfs_req = tfs_info->tfs_req_head;
	while (tfs_req != NULL) {
		/* Skip this tfs_req since it's already ACKed by AP */
		if (tfs_req->tfs_resp_st >= 0)
			break;

		tfs_req->subelem_len = 0;

		tfs_subelem = tfs_req->subelem_head;
		while (tfs_subelem != NULL) {
			tfs_req->subelem_len += tfs_subelem->tclas_len +
				DOT11_TFS_REQ_SUBELEM_LEN;
			tfs_subelem = tfs_subelem->next;
		}

		retlen += tfs_req->subelem_len + DOT11_TFS_REQ_IE_LEN;
		tfs_req = tfs_req->next;
	}

	return retlen;
}

static int
wlc_wnm_send_tfs_req_frame(wlc_wnm_info_t *wnm)
{
	wlc_info_t *wlc = wnm->wlc;
	wlc_bsscfg_t *bsscfg = wnm->wlc->cfg;
	wnm_tfs_info_t *tfs_info = &wnm->tfs_info;
	struct scb *scb;
	int maxlen;
	void *p;
	uint8 *pbody;
	int bodylen, retlen;
	dot11_tfs_req_t *req;

	maxlen = DOT11_TFS_REQ_LEN;
	maxlen += wlc_wnm_get_tfs_req_ie_len(tfs_info);

	if ((p = wlc_frame_get_action(wlc, FC_ACTION, &wlc->cfg->BSSID, &wlc->pub->cur_etheraddr,
		&wlc->cfg->BSSID, maxlen, &pbody, DOT11_ACTION_CAT_WNM)) == NULL) {
		return BCME_ERROR;
	}

	/* Prepare TFS request frame fields */
	req = (dot11_tfs_req_t *)pbody;
	req->category = DOT11_ACTION_CAT_WNM;
	req->action = DOT11_WNM_ACTION_TFS_REQ;
	WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	req->token = wnm->req_token;

	bodylen = DOT11_TFS_REQ_LEN;

	retlen = wlc_wnm_prep_tfs_req_ie(tfs_info, (uint8 *)pbody + bodylen);
	bodylen += retlen;
	ASSERT(bodylen == maxlen);

#ifdef BCMDBG
	prhex("Raw TFS Req body", (uchar *)pbody, bodylen);
#endif /* BCMDBG */


#ifdef BCMDBG
	prhex("Raw TFS Req", (uchar *)pbody - DOT11_MGMT_HDR_LEN, bodylen + DOT11_MGMT_HDR_LEN);
#endif /* BCMDBG */

	scb = wlc_scbfind(wlc, bsscfg, &bsscfg->BSSID);
	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, scb);

	return BCME_OK;
}

static int
wlc_wnm_get_dms_desc_list_len(wnm_dms_info_t *dms_info)
{
	int retlen = 0;
	dms_desc_t *dms_desc;

	/* Calculate the total length of dms request descriptor list */
	dms_desc = dms_info->dms_desc_head;
	while (dms_desc != NULL) {
		/* Skip this dms_desc since it's already ACKed by AP */
		if (dms_desc->resp_type >= 0)
			break;

		retlen += DOT11_DMS_REQ_DESC_LEN + dms_desc->tclas_len;

		if (dms_desc->tspec != NULL)
			retlen += DOT11_TSPEC_IE_LEN;

		if (dms_desc->subelem != NULL)
			retlen += dms_desc->subelem_len;

		dms_desc = dms_desc->next;
	}

	return retlen;
}

static int
wlc_wnm_prep_dms_desc_list(wnm_dms_info_t *dms_info, uint8 *buf)
{
	dms_desc_t *dms_desc;
	dot11_dms_req_desc_t *desc;
	uint8 *p = buf;
	int retlen = 0;

	dms_desc = dms_info->dms_desc_head;
	while (dms_desc != NULL) {
		int offset;

		/* Skip this dms_desc since it's already ACKed by dms resp from AP */
		if (dms_desc->resp_type >= 0)
			break;

		desc = (dot11_dms_req_desc_t *)p;
		desc->id = dms_desc->dms_id;
		desc->length = 1 + dms_desc->tclas_len; /* Include request type byte */
		if (dms_desc->tspec != NULL)
			desc->length += DOT11_TSPEC_IE_LEN;
		if (dms_desc->subelem != NULL && dms_desc->subelem_len > 0)
			desc->length += dms_desc->subelem_len;

		desc->type = dms_desc->req_type;

		offset = wlc_wnm_prep_tclas_ie(dms_desc->tclas_head, desc->data);

		if (dms_desc->tclas_num > 1) {
			bcopy(&dms_desc->tclas_proc, desc->data + offset,
				DOT11_TCLAS_PROC_IE_LEN);
			offset += DOT11_TCLAS_PROC_IE_LEN;
		}

		if (dms_desc->tspec != NULL) {
			bcopy(dms_desc->tspec, desc->data + offset, DOT11_TSPEC_IE_LEN);
			offset += DOT11_TSPEC_IE_LEN;
		}

		if (dms_desc->subelem != NULL && dms_desc->subelem_len > 0) {
			bcopy(dms_desc->subelem, desc->data + offset, dms_desc->subelem_len);
		}

		p += desc->length + TLV_HDR_LEN;
		dms_desc = dms_desc->next;

		retlen += desc->length + TLV_HDR_LEN;
	}

	return retlen;
}

static int
wlc_wnm_send_dms_req_frame(wlc_wnm_info_t *wnm)
{
	wlc_info_t *wlc = wnm->wlc;
	wnm_dms_info_t *dms_info = &wnm->dms_info;
	int dms_desc_list_len, maxlen;
	void *p;
	uint8 *pbody;
	int bodylen, retlen;
	dot11_dms_req_t *req;
	dot11_dms_req_ie_t *req_ie;

	dms_desc_list_len = wlc_wnm_get_dms_desc_list_len(dms_info);

	maxlen = DOT11_DMS_REQ_LEN + DOT11_DMS_REQ_IE_LEN + dms_desc_list_len;

	if ((p = wlc_frame_get_mgmt(wlc, FC_ACTION, &wlc->cfg->BSSID,
		&wlc->pub->cur_etheraddr, &wlc->cfg->BSSID, maxlen, &pbody)) == NULL) {
		return BCME_ERROR;
	}

	/* Prepare DMS request frame fields */
	req = (dot11_dms_req_t *)pbody;
	req->category = DOT11_ACTION_CAT_WNM;
	req->action = DOT11_WNM_ACTION_DMS_REQ;
	WLC_WNM_UPDATE_TOKEN(wnm->req_token);
	req->token = wnm->req_token;

	/* Prepare DMS request element fields */
	req_ie = (dot11_dms_req_ie_t *)req->data;
	req_ie->id = DOT11_MNG_DMS_REQUEST_ID;
	req_ie->length = dms_desc_list_len;

	bodylen = DOT11_DMS_REQ_LEN + DOT11_DMS_REQ_IE_LEN;

	retlen = wlc_wnm_prep_dms_desc_list(dms_info, (uint8 *)pbody + bodylen);

	bodylen += retlen;
	ASSERT(bodylen == maxlen);

#ifdef BCMDBG
	prhex("Raw DMS Req body", (uchar *)pbody, bodylen);
#endif /* BCMDBG */

	/* Fix up packet length */
	PKTSETLEN(wlc->osh, p, bodylen + DOT11_MGMT_HDR_LEN);

#ifdef BCMDBG
	prhex("Raw DMS Req", (uchar *)pbody - DOT11_MGMT_HDR_LEN, PKTLEN(wlc->osh, p));
#endif /* BCMDBG */

	wlc_sendmgmt(wlc, p, wlc->cfg->wlcif->qi, NULL);

	return BCME_OK;
}
#endif /* STA */

#ifdef AP
#ifdef PROXYARP
int BCMFASTPATH
wlc_wnm_proxyarp_packets_handle(wlc_bsscfg_t *bsscfg, void *sdu, frame_proto_t *fp, bool frombss)
{
	void *reply = NULL;
	wlc_info_t *wlc = bsscfg->wlc;
	uint8 result;

	result = proxyarp_packets_handle(wlc->osh, sdu, (void *)fp, frombss, &reply);

	switch (result) {
	case FRAME_TAKEN:
		if (reply != NULL) {
			struct scb *src_scb;
			struct scb *dst_scb;
			uint8 *p = PKTDATA(osh, reply);

			/* The src_ea is manipulated arp_reply's sender ether address */
			struct ether_addr *src_ea = (struct ether_addr *)(p + ETHER_SRC_OFFSET);
			struct ether_addr *dst_ea = (struct ether_addr *)(p + ETHER_DEST_OFFSET);
			/*
			 * we're in sending path and we need to reply this frame.  If the STA
			 * belong to this bsscfg, send up the reply frame as we received it from
			 * this bss. Otherwise drop the reply frame.
			 */
			if ((src_scb = wlc_scbfind(wlc, bsscfg, src_ea)) != NULL) {
				dst_scb = wlc_scbfind(wlc, bsscfg, dst_ea);
				if (dst_scb != NULL && src_scb->bsscfg == dst_scb->bsscfg) {
					WL_ERROR(("wl%d: sendout proxy-reply frame\n",
						wlc->pub->unit));
					wlc_sendpkt(wlc, reply, bsscfg->wlcif);
				}
				else {
					WL_ERROR(("wl%d: sendup proxy-reply frame\n",
						wlc->pub->unit));
					wl_sendup(wlc->wl, bsscfg->wlcif->wlif, reply, 1);
				}
			}
			else {
				PKTFREE(wlc->osh, reply, TRUE);
			}

			/* return OK to drop original packet */
			return BCME_OK;
		}

		break;
	case FRAME_DROP:
		return BCME_OK;

		break;
	default:
		break;
	}

	/* return fail to let original packet keep traverse */
	return BCME_ERROR;
}
#endif /* PROXYARP */

/*
 * Process WNM related packets.
 * return BCME_OK if the original packet need to be drop
 * return BCME_ERROR to let original packet go through
 */
int BCMFASTPATH
wlc_wnm_packets_handle(wlc_bsscfg_t *bsscfg, void *sdu, bool frombss)
{
	wlc_info_t *wlc = bsscfg->wlc;
	wlc_wnm_info_t *wnm = wlc->wnm_info;
	wnm_bsscfg_cubby_t* wnm_cfg = NULL;
	frame_proto_t fp;
	int err = BCME_ERROR;

	/* wnm not enabled */
	if (wlc->pub->_wnm != TRUE)
		goto done;
	if (wnm == NULL)
		goto done;
	if ((wnm_cfg = WNM_BSSCFG_CUBBY(wnm, bsscfg)) == NULL)
		goto done;
	/* get frame type */
	if (hnd_frame_proto(PKTDATA(wlc->osh, sdu), PKTLEN(wlc->osh, sdu), &fp) != BCME_OK)
		goto done;

#ifdef PROXYARP
	/* process proxyarp if it turned on */
	if (proxyarp_get() &&
	    wlc_wnm_proxyarp_packets_handle(bsscfg, sdu, &fp, frombss) == BCME_OK) {
		/* drop original frame should return BCME_OK */
		err = BCME_OK;
		goto done;
	}
#endif /* PROXYARP */

	/* process TFS in sending path if the scb's TFS turned on or scb's Sleep turned on */
	if (frombss && WNM_TFS_ENABLED(wnm_cfg->cap) &&
	    !ETHER_ISNULLADDR((struct ether_addr *)fp.l2)) {
		struct scb_iter scbiter;
		struct scb *scb = NULL;


		/* multicast/broadcast frame, need to iterate all scb in bss */
		if (ETHER_ISBCAST((struct ether_addr *)fp.l2) ||
		    ETHER_ISMULTI((struct ether_addr *)fp.l2)) {
			/* process TFS of each scb and bypass original frame */
			FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
				if (SCB_TFS(scb) && scb->tfs_list != NULL) {
					wlc_wnm_tfs_packet_handle(wlc, scb, &fp);
				}
			}
		}
		else {
			scb = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)fp.l2);
			/* this scb have subscribe TFS service, process this frame */
			if (SCB_TFS(scb) && scb->tfs_list != NULL) {
				/* send original frame if returned true */
				if (wlc_wnm_tfs_packet_handle(wlc, scb, &fp))
					goto done;

				/* if this frame is not matching tfs rule, drop it */
				err = BCME_OK;
				goto done;
			}
		}
	}

	/*
	 * process DMS in sending path if the packet is multicast after TFS,
	 * not a cloned frame, and bss have DMS rule
	 */
	if (frombss &&
	    wlc_wnm_dms_amsdu_on(wlc, bsscfg) &&
	    WLPKTTAGSCBGET(sdu) == NULL &&
	    WLWNM_FRAME_MCAST(fp)) {
		dms_desc_t *dms = wnm_cfg->dms_info.dms_desc_head;
		uint8 assoc = wlc_bss_assocscb_getcnt(wlc, bsscfg);
		bool drop_mcast = TRUE;

		while (dms) {
			/* matching cur DMS rule, clone packets and send to registered scbs */
			if (wlc_wnm_tclas_match(dms->tclas_head, &fp, TRUE)) {
				void *sdu_clone;
				struct dms_scb *cur = dms->dms_scb_head;
				uint8 dmscb_cnt = 0;

				while (cur) {
					if (cur->scb == NULL ||
					    (sdu_clone = PKTDUP(wlc->osh, sdu)) == NULL) {
						WL_ERROR(("dms clone error\n"));
						break;
					}

					/* save current frame with target scb */
					WLPKTTAGSCBSET(sdu_clone, cur->scb);

					/* Send the packet using bsscfg wlcif */
					wlc_sendpkt(wlc, sdu_clone, bsscfg->wlcif);

					cur = cur->next;
					dmscb_cnt++;
				}
				/*
				 * Keep multicast frame if there's SCB
				 * not requesting the streaming via DMS
				 */
				if (assoc != dmscb_cnt)
					drop_mcast = FALSE;
			}
			dms = dms->next;
		}

		if (drop_mcast)
			err = BCME_OK;
	}
done:
	return err;
}
#endif /* AP */
