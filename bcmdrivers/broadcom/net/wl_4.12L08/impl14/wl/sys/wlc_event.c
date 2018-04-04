/*
 * Event mechanism
 *
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_event.c 286394 2011-09-27 18:45:36Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <wlioctl.h>
#include <wl_dbg.h>

#include <wlc_pub.h>
#include <wlc_key.h>
#include <wl_export.h>
#include <wlc_event.h>
#include <bcm_mpool_pub.h>

/* For wlc.h */
#include <d11.h>
#include <wlc_bsscfg.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc.h>
#include <wlc_rate_sel.h>
#ifdef MSGTRACE
#include <msgtrace.h>
#endif

/* Local prototypes */
#ifndef WLNOEIND
static void wlc_event_sendup(wlc_eventq_t *eq, const wlc_event_t *e,
	struct ether_addr *da, struct ether_addr *sa, uint8 *data, uint32 len);
#endif /* WLNOEIND */
static void wlc_timer_cb(void *arg);

/* Private data structures */
struct wlc_eventq
{
	wlc_event_t		*head;
	wlc_event_t		*tail;
	struct wlc_info		*wlc;
	void			*wl;
	wlc_pub_t 		*pub;
	bool			tpending;
	bool			workpending;
	struct wl_timer		*timer;
	wlc_eventq_cb_t		cb;
	bcm_mp_pool_h		mpool_h;
	uint8			event_inds_mask[ROUNDUP(WLC_E_LAST, NBBY)/NBBY];
};

/*
 * Export functions
 */
wlc_eventq_t*
BCMATTACHFN(wlc_eventq_attach)(wlc_pub_t *pub, struct wlc_info *wlc, void *wl, wlc_eventq_cb_t cb)
{
	wlc_eventq_t *eq;

	eq = (wlc_eventq_t*)MALLOC(pub->osh, sizeof(wlc_eventq_t));
	if (eq == NULL)
		return NULL;

	bzero(eq, sizeof(wlc_eventq_t));

	/* Create memory pool for 'wlc_event_t' data structs. */
	if (bcm_mpm_create_heap_pool(wlc->mem_pool_mgr, sizeof(wlc_event_t),
	                             "event", &eq->mpool_h) != BCME_OK) {
		WL_ERROR(("wl%d: bcm_mpm_create_heap_pool failed\n", pub->unit));
		MFREE(pub->osh, eq, sizeof(wlc_eventq_t));
		return NULL;
	}

	eq->cb = cb;
	eq->wlc = wlc;
	eq->wl = wl;
	eq->pub = pub;

	if (!(eq->timer = wl_init_timer(eq->wl, wlc_timer_cb, eq, "eventq"))) {
		WL_ERROR(("wl%d: wlc_eventq_attach: timer failed\n", pub->unit));
		MFREE(eq->pub->osh, eq, sizeof(wlc_eventq_t));
		return NULL;
	}

	return eq;
}

int
BCMATTACHFN(wlc_eventq_detach)(wlc_eventq_t *eq)
{
	/* Clean up pending events */
	wlc_eventq_down(eq);

	if (eq->timer) {
		if (eq->tpending) {
			wl_del_timer(eq->wl, eq->timer);
			eq->tpending = FALSE;
		}
		wl_free_timer(eq->wl, eq->timer);
		eq->timer = NULL;
	}

	ASSERT(wlc_eventq_avail(eq) == FALSE);

	bcm_mpm_delete_heap_pool(eq->wlc->mem_pool_mgr, &eq->mpool_h);

	MFREE(eq->pub->osh, eq, sizeof(wlc_eventq_t));
	return 0;
}

int
BCMUNINITFN(wlc_eventq_down)(wlc_eventq_t *eq)
{
	int callbacks = 0;
	if (eq->tpending && !eq->workpending) {
		if (!wl_del_timer(eq->wl, eq->timer))
			callbacks++;

		ASSERT(wlc_eventq_avail(eq) == TRUE);
		ASSERT(eq->workpending == FALSE);
		eq->workpending = TRUE;
		if (eq->cb)
			eq->cb(eq->wlc);

		ASSERT(eq->workpending == TRUE);
		eq->workpending = FALSE;
		eq->tpending = FALSE;
	}
	else {
		ASSERT(eq->workpending || wlc_eventq_avail(eq) == FALSE);
	}
	return callbacks;
}

wlc_event_t*
wlc_event_alloc(wlc_eventq_t *eq)
{
	wlc_event_t *e;

	e = (wlc_event_t *) bcm_mp_alloc(eq->mpool_h);

	if (e == NULL)
		return NULL;

	bzero(e, sizeof(wlc_event_t));
	return e;
}

void
wlc_event_free(wlc_eventq_t *eq, wlc_event_t *e)
{
	ASSERT(e->data == NULL);
	ASSERT(e->next == NULL);
	bcm_mp_free(eq->mpool_h, e);
}

void
wlc_eventq_enq(wlc_eventq_t *eq, wlc_event_t *e)
{
	ASSERT(e->next == NULL);
	e->next = NULL;

	if (eq->tail) {
		eq->tail->next = e;
		eq->tail = e;
	}
	else
		eq->head = eq->tail = e;

	if (!eq->tpending) {
		eq->tpending = TRUE;
		/* Use a zero-delay timer to trigger
		 * delayed processing of the event.
		 */
		wl_add_timer(eq->wl, eq->timer, 0, 0);
	}
}

wlc_event_t*
wlc_eventq_deq(wlc_eventq_t *eq)
{
	wlc_event_t *e;

	e = eq->head;
	if (e) {
		eq->head = e->next;
		e->next = NULL;

		if (eq->head == NULL)
			eq->tail = eq->head;
	}
	return e;
}

wlc_event_t*
wlc_eventq_next(wlc_eventq_t *eq, wlc_event_t *e)
{
#ifdef BCMDBG
	wlc_event_t *etmp;

	for (etmp = eq->head; etmp; etmp = etmp->next) {
		if (etmp == e)
			break;
	}
	ASSERT(etmp != NULL);
#endif

	return e->next;
}

int
wlc_eventq_cnt(wlc_eventq_t *eq)
{
	wlc_event_t *etmp;
	int cnt = 0;

	for (etmp = eq->head; etmp; etmp = etmp->next)
		cnt++;

	return cnt;
}

bool
wlc_eventq_avail(wlc_eventq_t *eq)
{
	return (eq->head != NULL);
}

#ifndef WLNOEIND
int
wlc_eventq_register_ind(wlc_eventq_t *eq, void *bitvect)
{
	bcopy(bitvect, eq->event_inds_mask, sizeof(eq->event_inds_mask));

	wlc_enable_probe_req(
		eq->wlc,
		PROBE_REQ_EVT_MASK,
		wlc_eventq_test_ind(eq, WLC_E_PROBREQ_MSG)? PROBE_REQ_EVT_MASK:0);
#ifdef MSGTRACE
	if (isset(eq->event_inds_mask, WLC_E_TRACE)) {
		msgtrace_start();
	} else {
		msgtrace_stop();
	}
#endif /* MSGTRACE */
	return 0;
}

int
wlc_eventq_query_ind(wlc_eventq_t *eq, void *bitvect)
{
	bcopy(eq->event_inds_mask, bitvect, sizeof(eq->event_inds_mask));
	return 0;
}

int
wlc_eventq_test_ind(wlc_eventq_t *eq, int et)
{
	return isset(eq->event_inds_mask, et);
}

int
wlc_eventq_handle_ind(wlc_eventq_t *eq, wlc_event_t *e)
{
	wlc_bsscfg_t *cfg;
	struct ether_addr *da;
	struct ether_addr *sa;

	cfg = wlc_bsscfg_find_by_wlcif(eq->wlc, e->wlcif);
	ASSERT(cfg != NULL);

	da = &cfg->cur_etheraddr;
	sa = &cfg->cur_etheraddr;

	if (wlc_eventq_test_ind(eq, e->event.event_type))
		wlc_event_sendup(eq, e, da, sa, e->data, e->event.datalen);
	return 0;
}

void
wlc_eventq_flush(wlc_eventq_t *eq)
{
	if (eq == NULL)
		return;

	if (eq->cb)
		eq->cb(eq->wlc);
	if (eq->tpending) {
		wl_del_timer(eq->wl, eq->timer);
		eq->tpending = FALSE;
	}
}
#endif /* !WLNOEIND */

/*
 * Local Functions
 */
static void
wlc_timer_cb(void *arg)
{
	struct wlc_eventq* eq = (struct wlc_eventq*)arg;

	ASSERT(eq->tpending == TRUE);
	ASSERT(wlc_eventq_avail(eq) == TRUE);
	ASSERT(eq->workpending == FALSE);
	eq->workpending = TRUE;

	if (eq->cb)
		eq->cb(eq->wlc);

	ASSERT(wlc_eventq_avail(eq) == FALSE);
	ASSERT(eq->tpending == TRUE);
	eq->workpending = FALSE;
	eq->tpending = FALSE;
}


#ifndef WLNOEIND
/* Abandonable helper function for PROP_TXSTATUS */
static void
wlc_event_mark_packet(wlc_info_t *wlc, void *p)
{
#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		PKTSETTYPEEVENT(wlc->pub->osh, p);
		/* this is implied for event packets anyway */
		PKTSETNODROP(wlc->pub->osh, p);
	}
#endif
}

void
wlc_assign_event_msg(wlc_info_t *wlc, wl_event_msg_t *msg, const wlc_event_t *e,
	uint8 *data, uint32 len)
{
	void *databuf;

	ASSERT(msg && e);

	/* translate the wlc event into bcm event msg */
	msg->version = hton16(BCM_EVENT_MSG_VERSION);
	msg->event_type = hton32(e->event.event_type);
	msg->status = hton32(e->event.status);
	msg->reason = hton32(e->event.reason);
	msg->auth_type = hton32(e->event.auth_type);
	msg->datalen = hton32(e->event.datalen);
	msg->flags = hton16(e->event.flags);
	bzero(msg->ifname, sizeof(msg->ifname));
	strncpy(msg->ifname, e->event.ifname, sizeof(msg->ifname) - 1);
	msg->ifidx = e->event.ifidx;
	msg->bsscfgidx = e->event.bsscfgidx;

	if (e->addr)
		bcopy(e->event.addr.octet, msg->addr.octet, ETHER_ADDR_LEN);

	databuf = (char *)(msg + 1);
	if (len)
		bcopy(data, databuf, len);
}

static void
wlc_event_sendup(wlc_eventq_t *eq, const wlc_event_t *e,
	struct ether_addr *da, struct ether_addr *sa, uint8 *data, uint32 len)
{
	wlc_info_t *wlc = eq->wlc;

	BCM_REFERENCE(wlc);

	ASSERT(e != NULL);
	ASSERT(e->wlcif != NULL);

	{
		void *p;
		char *ptr;
		bcm_event_t *msg;
		uint pktlen;

		pktlen = (BCMEXTRAHDROOM + 2) + sizeof(bcm_event_t) + len + 2;
		if ((p = PKTGET(wlc->osh, pktlen, FALSE)) == NULL) {
			WL_ERROR(("wl%d: wlc_event_sendup: failed to get a pkt\n", wlc->pub->unit));
			return;
		}

		ASSERT(ISALIGNED(PKTDATA(wlc->osh, p), sizeof(uint32)));

		/* make room for headers; ensure we start on an odd 16 bit offset */
		PKTPULL(wlc->osh, p, BCMEXTRAHDROOM + 2);

		msg = (bcm_event_t *) PKTDATA(wlc->osh, p);

		bcopy(da, &msg->eth.ether_dhost, ETHER_ADDR_LEN);
		bcopy(sa, &msg->eth.ether_shost, ETHER_ADDR_LEN);

		/* Set the locally administered bit on the source mac address if both
		 * SRC and DST mac addresses are the same. This prevents the downstream
		 * bridge from dropping the packet.
		 * Clear it if both addresses are the same and it's already set.
		 */
		if (!bcmp(&msg->eth.ether_shost, &msg->eth.ether_dhost, ETHER_ADDR_LEN))
			ETHER_TOGGLE_LOCALADDR(&msg->eth.ether_shost);

		msg->eth.ether_type = hton16(ETHER_TYPE_BRCM);

		/* BCM Vendor specific header... */
		msg->bcm_hdr.subtype = hton16(BCMILCP_SUBTYPE_VENDOR_LONG);
		msg->bcm_hdr.version = BCMILCP_BCM_SUBTYPEHDR_VERSION;
		bcopy(BRCM_OUI, &msg->bcm_hdr.oui[0], DOT11_OUI_LEN);
		/* vendor spec header length + pvt data length (private indication
		 * hdr + actual message itself)
		 */
		msg->bcm_hdr.length = hton16(BCMILCP_BCM_SUBTYPEHDR_MINLENGTH +
		                             BCM_MSG_LEN +
		                             (uint16)len);
		msg->bcm_hdr.usr_subtype = hton16(BCMILCP_BCM_SUBTYPE_EVENT);

		/* update the event struct */
		wlc_assign_event_msg(wlc, &msg->event, e, data, len);

		/* fixup lengths */
		msg->bcm_hdr.length = ntoh16(msg->bcm_hdr.length);
		msg->bcm_hdr.length += sizeof(wl_event_msg_t);
		msg->bcm_hdr.length = hton16(msg->bcm_hdr.length);

		PKTSETLEN(wlc->osh, p, (sizeof(bcm_event_t) + len + 2));

		ptr = (char *)(msg + 1);
		/* Last 2 bytes of the message are 0x00 0x00 to signal that there are
		 * no ethertypes which are following this
		 */
		ptr[len + 0] = 0x00;
		ptr[len + 1] = 0x00;

		wlc_event_mark_packet(wlc, p);

		wl_sendup(eq->wl, e->wlcif->wlif, p, 1);
	}
}

#ifdef MSGTRACE
void
wlc_event_sendup_trace(wlc_info_t *wlc, hndrte_dev_t *wl_rtedev, uint8 *hdr, uint16 hdrlen,
                       uint8 *buf, uint16 buflen)
{
	void *p;
	bcm_event_t *msg;
	char *ptr, *databuf;
	struct lbuf *lb;
	uint16 len;
	osl_t *osh = wlc->osh;
	hndrte_dev_t *busdev = wl_rtedev->chained;

	if (busdev == NULL)
		return;

	if (! wlc_eventq_test_ind(wlc->eventq, WLC_E_TRACE))
		return;

	len = hdrlen + buflen;
	ASSERT(len < (wlc->pub->tunables->rxbufsz - sizeof(bcm_event_t) - 2));

	if ((p = PKTGET(osh, wlc->pub->tunables->rxbufsz, FALSE)) == NULL) {
		return;
	}

	ASSERT(ISALIGNED(PKTDATA(osh, p), sizeof(uint32)));

	/* make room for headers; ensure we start on an odd 16 bit offset */
	PKTPULL(osh, p, BCMEXTRAHDROOM + 2);

	msg = (bcm_event_t *) PKTDATA(osh, p);

	msg->eth.ether_type = hton16(ETHER_TYPE_BRCM);

	/* BCM Vendor specific header... */
	msg->bcm_hdr.subtype = hton16(BCMILCP_SUBTYPE_VENDOR_LONG);
	msg->bcm_hdr.version = BCMILCP_BCM_SUBTYPEHDR_VERSION;
	bcopy(BRCM_OUI, &msg->bcm_hdr.oui[0], DOT11_OUI_LEN);
	/* vendor spec header length + pvt data length (private indication hdr + actual message
	 * itself)
	 */
	msg->bcm_hdr.length = hton16(BCMILCP_BCM_SUBTYPEHDR_MINLENGTH + BCM_MSG_LEN + (uint16)len);
	msg->bcm_hdr.usr_subtype = hton16(BCMILCP_BCM_SUBTYPE_EVENT);

	PKTSETLEN(osh, p, (sizeof(bcm_event_t) + len + 2));

	/* update the event struct */
	/* translate the wlc event into bcm event msg */
	msg->event.version = hton16(BCM_EVENT_MSG_VERSION);
	msg->event.event_type = hton32(WLC_E_TRACE);
	msg->event.status = hton32(WLC_E_STATUS_SUCCESS);
	msg->event.reason = 0;
	msg->event.auth_type = 0;
	msg->event.datalen = hton32(len);
	msg->event.flags = 0;
	bzero(msg->event.ifname, sizeof(msg->event.ifname));

	/* fixup lengths */
	msg->bcm_hdr.length = ntoh16(msg->bcm_hdr.length);
	msg->bcm_hdr.length += sizeof(wl_event_msg_t);
	msg->bcm_hdr.length = hton16(msg->bcm_hdr.length);

	PKTSETLEN(osh, p, (sizeof(bcm_event_t) + len + 2));

	/* Copy the data */
	databuf = (char *)(msg + 1);
	bcopy(hdr, databuf, hdrlen);
	bcopy(buf, databuf+hdrlen, buflen);

	ptr = (char *)databuf;

	PKTSETMSGTRACE(p, TRUE);

	/* Last 2 bytes of the message are 0x00 0x00 to signal that there are no ethertypes which
	 * are following this
	 */
	ptr[len+0] = 0x00;
	ptr[len+1] = 0x00;
	lb = PKTTONATIVE(osh, p);

	if (busdev->funcs->xmit(NULL, busdev, lb) != 0) {
		lb_free(lb);
	}
}
#endif /* MSGTRACE */

int
wlc_eventq_set_ind(wlc_eventq_t* eq, uint et, bool enab)
{
	if (et >= WLC_E_LAST)
		return -1;
	if (enab)
		setbit(eq->event_inds_mask, et);
	else
		clrbit(eq->event_inds_mask, et);

	if (et == WLC_E_PROBREQ_MSG)
		wlc_enable_probe_req(eq->wlc, PROBE_REQ_EVT_MASK, enab? PROBE_REQ_EVT_MASK:0);

	return 0;
}
#endif /* !WLNOEIND */
