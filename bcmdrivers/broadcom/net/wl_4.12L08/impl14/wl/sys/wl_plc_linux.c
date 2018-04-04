/*
 * PLC Failover support to dynamically select the link. WLAN or PLC which ever
 * provides optimum performance will be used.
 *
 * Copyright (C) 2010, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id$
 */
#include <typedefs.h>
#include <linuxver.h>
#include <wlc_cfg.h>
#include <osl.h>

#include <linux/types.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/if_vlan.h>

#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <wlc_pub.h>
#include <wl_dbg.h>
#ifdef HNDCTF
#include <ctf/hndctf.h>
#endif
#ifdef WLC_HIGH_ONLY
#include "bcm_rpc_tp.h"
#include "bcm_rpc.h"
#include "bcm_xdr.h"
#include "wlc_rpc.h"
#endif
#include <wl_linux.h>
#include <wl_plc_linux.h>

#define WL_INFO(dev) ((wl_info_t*)(WL_DEV_IF(dev)->wl))

void
wl_plc_sendpkt(wl_if_t *wlif, struct sk_buff *skb, struct net_device *dev)
{
#ifdef CTFPOOL
	uint users;
#endif /* CTFPOOL */

	ASSERT(wlif->plc != NULL);

	WL_PLC("%s: skb %p dev %p\n", __FUNCTION__, skb, dev);

	skb->dev = dev;

#if defined(BCMDBG) && defined(PLCDBG)
	if (dev != wlif->plc->plc_rxdev3)
		prhex("->PLC", skb->data, 64);
#endif /* BCMDBG && PLCDBG */

	/* Frames are first queued to tx_vid (vlan3) to be sent out to
	 * the plc port.
	 */
#ifdef CTFPOOL
	users = atomic_read(&skb->users);
	atomic_inc(&skb->users);
	dev_queue_xmit(skb);
	if (atomic_read(&skb->users) == users) {
		skb = PKTFRMNATIVE(wlif->wl->osh, skb);
		PKTFREE(wlif->wl->osh, skb, TRUE);
	} else
		atomic_dec(&skb->users);
#else /* CTFPOOL */
	dev_queue_xmit(skb);
#endif /* CTFPOOL */

	return;
}

struct sk_buff *
wl_plc_tx_prep(wl_if_t *wlif, struct sk_buff *skb)
{
	struct ethervlan_header *evh;
	uint16 headroom, prio = PKTPRIO(skb) & VLAN_PRI_MASK;
	osl_t *osh = wlif->wl->osh;

	headroom = PKTHEADROOM(osh, skb);

	WL_PLC("%s: shared %d headroom %d\n", __FUNCTION__, PKTSHARED(skb), headroom);

	if (PKTSHARED(skb) || (headroom < (3 * VLAN_TAG_LEN))) {
		struct sk_buff *tmp = skb;
		skb = skb_copy_expand(tmp, headroom + (3 * VLAN_TAG_LEN),
		                      skb_tailroom(tmp), GFP_ATOMIC);
		if (skb == NULL) {
			WL_ERROR(("wl%d: %s: Out of memory while copying bcast frame\n",
			          wlif->wl->pub->unit, __FUNCTION__));
			return NULL;
		}
#ifdef CTFPOOL
		PKTCLRFAST(osh, skb);
		CTFPOOLPTR(osh, skb) = NULL;
#endif /* CTFPOOL */

		skb = PKTFRMNATIVE(osh, skb);
	}

	evh = (struct ethervlan_header *)skb_push(skb, 3 * VLAN_TAG_LEN);
	memmove(skb->data, skb->data + (3 * VLAN_TAG_LEN), 2 * ETHER_ADDR_LEN);

	/* Initialize dummy outer vlan tag */
	evh->vlan_type = HTON16(VLAN_TPID);
	evh->vlan_tag = HTON16(wlif->plc->tx_vid);

	/* Initialize outer dummy vlan tag */
	evh = (struct ethervlan_header *)(skb->data + VLAN_TAG_LEN);
	evh->vlan_type = HTON16(VLAN_TPID);
	evh->vlan_tag = HTON16((prio << VLAN_PRI_SHIFT) | wlif->plc->tx_vid);

	/* Initialize dummy inner tag before sending to PLC */
	evh = (struct ethervlan_header *)(skb->data + 2 * VLAN_TAG_LEN);
	evh->vlan_type = HTON16(VLAN_TPID);
	evh->vlan_tag = HTON16((prio << VLAN_PRI_SHIFT) | WL_PLC_DUMMY_VID);

	return skb;
}

/* Called when the frame is recieved on any of the PLC rxvifs (vlan4,
 * vlan5 or vlan6) or WDS interface. Based on the forwarding information
 * received in the vlan tag we send the frame up the bridge or send it
 * over the WDS link.
 */
int32
wl_plc_recv(struct sk_buff *skb, struct net_device *dev, wl_plc_t *plc, uint16 if_in)
{
	int32 vid_in = -1, vid_in_in = -1, err = 0, vid_out = -1, action = -1;
	int16 if_out = -1;
	struct ethervlan_header *evh;
	bool sendup = FALSE;
	struct sk_buff *nskb = skb;
	struct net_device *dev_out = NULL;
	osl_t *osh;

	ASSERT(plc != NULL);

	if (!plc->inited) {
		PKTFREE(NULL, skb, FALSE);
		return BCME_OK;
	}

	evh = (struct ethervlan_header *)skb->data;

	/* Read the outer tag */
	if (evh->vlan_type == HTON16(ETHER_TYPE_8021Q)) {
		vid_in = NTOH16(evh->vlan_tag) & VLAN_VID_MASK;

		/* See if there is an inner tag */
		evh = (struct ethervlan_header *)(skb->data + VLAN_TAG_LEN);
		if (evh->vlan_type == HTON16(ETHER_TYPE_8021Q))
			vid_in_in = NTOH16(evh->vlan_tag) & VLAN_VID_MASK;
	}

	if (vid_in == plc->rx_vid1) {
		/* Frame received from WDS* with VID 4 or PLC with VID 4,1.
		 * Send it up to the local bridge untagged.
		 */
		if (((if_in == WL_PLC_IF_WDS) && (vid_in_in == -1)) ||
		    ((if_in == WL_PLC_IF_PLC) && (vid_in_in == 1))) {
			action = WL_PLC_ACTION_UNTAG;
			vid_out = -1;
			if_out = WL_PLC_IF_BR;
			sendup = TRUE;
		}
	} else if (vid_in == plc->rx_vid2) {
		/* Frame received from WDS* with VID 5. Send it down
		 * to PLC. Use the same VID, no untagging necessary.
		 */
		if (if_in == WL_PLC_IF_WDS) {
			if (vid_in_in == -1) {
				action = WL_PLC_ACTION_NONE;
				vid_out = plc->rx_vid2;
				dev_out = plc->plc_rxdev2;
				if_out = WL_PLC_IF_PLC;
			}
		} else if (if_in == WL_PLC_IF_PLC) {
			/* Frame received from PLC with VID 5,4.
			 * Send it down to WDS with VID 4. Only
			 * remove the outer header.
			 */
			if (vid_in_in == plc->rx_vid1) {
				action = WL_PLC_ACTION_UNTAG;
				vid_out = plc->rx_vid1;
				if_out = WL_PLC_IF_WDS;
			} else if (vid_in_in == plc->rx_vid2) {
				/* Frame received from PLC with VID 5,5.
				 * Send it down to WDS with VID 5. Only
				 * remove the outer header.
				 */
				action = WL_PLC_ACTION_UNTAG;
				vid_out = plc->rx_vid2;
				if_out = WL_PLC_IF_WDS;
			}
		}
	} else if (vid_in == plc->rx_vid3) {
		if ((if_in == WL_PLC_IF_WDS) && (vid_in_in == -1)) {
			/* Frame received from WDS* with VID 6. Send it
			 * to PLC. Use the same VID, no modification reqd.
			 */
			action = WL_PLC_ACTION_NONE;
			vid_out = plc->rx_vid3;
			dev_out = plc->plc_rxdev3;
			if_out = WL_PLC_IF_PLC;
		} else if ((if_in == WL_PLC_IF_PLC) && (vid_in_in == -1)) {
			/* Frame received from PLC with VID 6. Send it
			 * to WDS. Use the same VID, no modification reqd.
			 */
			action = WL_PLC_ACTION_NONE;
			vid_out = plc->rx_vid3;
			dev_out = plc->plc_rxdev3;
			if_out = WL_PLC_IF_WDS;
		}
	} else if (vid_in == -1) {
		/* Untagged frame received from WDS*. */
		if (if_in == WL_PLC_IF_WDS) {
			/* Send it up to bridge and send it down to PLC
			 * with VID 5.
			 */
			action = WL_PLC_ACTION_TAG;
			vid_out = plc->rx_vid2;
			dev_out = plc->plc_rxdev2;
			if_out = WL_PLC_IF_PLC;
			sendup = TRUE;
		} else if (if_in == WL_PLC_IF_PLC) {
			/* Untagged frame received from PLC.
			 * Send it up to bridge and send it down to WDS
			 * with VID 4.
			 */
			action = WL_PLC_ACTION_TAG;
			vid_out = plc->rx_vid1;
			if_out = WL_PLC_IF_WDS;
			sendup = TRUE;
		}
	}

	osh = WL_DEV_IF(plc->wds_dev)->wl->osh;

	/* Send up the bridge if requested */
	if (sendup) {
		/* Make sure the received frame has no tag */
		if (if_out != WL_PLC_IF_BR) {
			nskb = skb_copy(skb, GFP_ATOMIC);
			if (nskb == NULL)
				return -ENOMEM;
#ifdef CTFPOOL
			PKTCLRFAST(osh, nskb);
			CTFPOOLPTR(osh, nskb) = NULL;
#endif /* CTFPOOL */
		}

		skb->dev = plc->wds_dev;

		if (vid_in != -1) {
			uint32 pull = VLAN_TAG_LEN;

			if (vid_in_in != -1)
				pull += VLAN_TAG_LEN;

			memmove(skb->data + pull, skb->data, ETHER_ADDR_LEN * 2);
			skb_pull(skb, pull);
		}
		err = -1;

		/* No processing needed since the frame is only going up to bridge */
		if (if_out == WL_PLC_IF_BR) {
			WL_PLC("%s: pkt %p length: %d\n", __FUNCTION__, skb, skb->len);
#if defined(BCMDBG) && defined(PLCDBG)
			prhex("->BRIDGE", skb->data, 64);
#endif /* BCMDBG && PLCDBG */
			return err;
		}
	}

	/* Perform the tag/untag action first */
	if (action == WL_PLC_ACTION_UNTAG) {
		if (vid_in_in != -1) {
			uint32 pull = VLAN_TAG_LEN;

			/* Remove outer/both the tags of double tagged
			 * vlan frames.
			 */
			if (vid_out == -1)
				pull += VLAN_TAG_LEN;

			memmove(nskb->data + pull, nskb->data, ETHER_ADDR_LEN * 2);
			skb_pull(nskb, pull);

			/* Modify the tag if needed. */
			if (vid_out != -1) {
				evh = (struct ethervlan_header *)nskb->data;
				evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
				evh->vlan_tag |= HTON16(vid_out);
			}
		} else {
			/* Untag a single tagged frame */
			memmove(nskb->data + VLAN_TAG_LEN, nskb->data,
			        ETHER_ADDR_LEN * 2);
			skb_pull(nskb, VLAN_TAG_LEN);
		}
	} else if (action == WL_PLC_ACTION_TAG) {
		if (vid_in_in != -1) {
			/* Remove the outer tag and modify the inner tag */
			evh = (struct ethervlan_header *)skb_pull(nskb, VLAN_TAG_LEN);
			evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
			evh->vlan_tag |= HTON16(vid_out);
		} else if (vid_in != -1) {
			/* Modify the tag as the frame has one tag */
			evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
			evh->vlan_tag |= HTON16(vid_out);
		} else {
			/* Add vlan header */
			uint16 prio = PKTPRIO(nskb) & VLAN_PRI_MASK;

			evh = (struct ethervlan_header *)skb_push(nskb, VLAN_TAG_LEN);
			memmove(nskb->data, nskb->data + VLAN_TAG_LEN, 2 * ETHER_ADDR_LEN);
			evh->vlan_type = HTON16(VLAN_TPID);
			evh->vlan_tag &= ~HTON16(VLAN_VID_MASK);
			evh->vlan_tag |= HTON16((prio << VLAN_PRI_SHIFT) | vid_out);
		}
	}

#ifdef PLCDBG
	if (vid_out != 6) {
		WL_PLC("%s: Rcvd from %s with VID: %d,%d\n", __FUNCTION__,
		       if_in == WL_PLC_IF_WDS ? "WDS" : "PLC", vid_in, vid_in_in);

		WL_PLC("%s: Sending to %s with VID: %d\n", __FUNCTION__,
		       if_out == WL_PLC_IF_WDS ? "WDS" :
		       if_out == WL_PLC_IF_PLC ? "PLC" : "BRIDGE", vid_out);

		if ((vid_out != 6) && (if_out == WL_PLC_IF_WDS)) {
			WL_PLC("%s: pkt %p length: %d\n", __FUNCTION__, nskb, nskb->len);
#ifdef BCMDBG
			prhex("->WDS", nskb->data, 64);
#endif /* BCMDBG */
		}
	}
#endif /* PLCDBG */

	/* Send out on the specified WDS or PLC interface */
	if (if_out == WL_PLC_IF_WDS) {
		/* Don't send out frames on WDS when the interface
		 * is down.
		 */
		nskb = PKTFRMNATIVE(osh, nskb);
		if (!(WL_DEV_IF(plc->wds_dev)->wl->pub->up)) {
			/* Free the skb */
			PKTFREE(osh, nskb, TRUE);
			return BCME_OK;
		}
		WL_LOCK(WL_DEV_IF(plc->wds_dev)->wl);
		wlc_sendpkt(WL_INFO(plc->wds_dev)->wlc, (void *)nskb,
		            WL_DEV_IF(plc->wds_dev)->wlcif);
		WL_UNLOCK(WL_DEV_IF(plc->wds_dev)->wl);
	} else if (if_out == WL_PLC_IF_PLC) {
		ASSERT(if_in == WL_PLC_IF_WDS);
		ASSERT(dev_out != NULL);
		/* Add another tag so that switch will remove outer tag */
		evh = (struct ethervlan_header *)skb_push(nskb, VLAN_TAG_LEN);
		memmove(nskb->data, nskb->data + VLAN_TAG_LEN,
		        (2 * ETHER_ADDR_LEN) + VLAN_TAG_LEN);
		evh->vlan_tag &= ~HTON16(VLAN_PRI_MASK << VLAN_PRI_SHIFT);
		wl_plc_sendpkt(WL_DEV_IF(plc->wds_dev), nskb, dev_out);
	}

	return err;
}

#ifdef AP
/* This function is called when frames are received on one of the VLANs
 * setup for receiving PLC traffic.
 */
static int32
wl_plc_master_hook(struct sk_buff *skb, struct net_device *dev, void *arg)
{
	WL_PLC("%s: From PLC skb %p data %p mac %p len %d\n",
	       __FUNCTION__, skb, skb->data, eth_hdr(skb), skb->len);

	/* Frame received from PLC on one of rxvifs 1/2/3 */
	if (wl_plc_recv(skb, dev, (wl_plc_t *)arg, WL_PLC_IF_PLC) < 0) {
		WL_PLC("%s: Sending %p up to bridge\n", __FUNCTION__, skb);
		return -1;
	}

	return BCME_OK;
}

static void
wl_plc_master_set(wl_plc_t *plc, struct net_device *plc_dev, wl_if_t *wlif)
{
	ASSERT(plc_dev != NULL);

	plc_dev->master = wlif->dev;
	plc_dev->flags |= IFF_SLAVE;
	plc_dev->master_hook = wl_plc_master_hook;
	plc_dev->master_hook_arg = plc;

	/* Disable vlan header removal */
	VLAN_DEV_INFO(plc_dev)->flags &= ~1;
}

int32
wl_plc_init(wl_if_t *wlif)
{
	int8 *var;
	struct net_device *plc_dev;
	wl_info_t *wl;
	char if_name[IFNAMSIZ] = { 0 };
	wl_plc_t *plc = NULL;

	ASSERT(wlif != NULL);

	wl = wlif->wl;

	WL_PLC("wl%d: %s: Is WDS %d\n", wl->pub->unit, __FUNCTION__, WLIF_IS_WDS(wlif));

	if (!WLIF_IS_WDS(wlif))
		return 0;

	/* See if we need to initialize PLC for this interface */
	if (!PLC_ENAB(wl->pub))
		return 0;

	WL_PLC("wl%d: %s: Initializing the PLC VIFs\n", wl->pub->unit, __FUNCTION__);

	/* Read plc_vifs to initialize the VIDs to use for receiving
	 * and forwarding the frames.
	 */
	var = getvar(NULL, "plc_vifs");

	if (var == NULL) {
		WL_ERROR(("wl%d: %s: PLC vifs not configured\n",
		          wl->pub->unit, __FUNCTION__));
		return 0;
	}

	WL_PLC("wl%d: %s: plc_vifs = %s\n", wl->pub->unit, __FUNCTION__, var);

	/* Allocate plc info structure */
	plc = MALLOC(wl->osh, sizeof(wl_plc_t));
	if (plc == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          wl->pub->unit, __FUNCTION__, MALLOCED(wl->osh)));
		return -ENOMEM;
	}
	bzero(plc, sizeof(wl_plc_t));

	plc->tx_vid = 3;
	plc->rx_vid1 = 4;
	plc->rx_vid2 = 5;
	plc->rx_vid3 = 6;

	/* Initialize the VIDs to use for PLC rx and tx */
	sscanf(var, "vlan%d vlan%d vlan%d vlan%d",
	       &plc->tx_vid, &plc->rx_vid1, &plc->rx_vid2, &plc->rx_vid3);

	WL_PLC("wl%d: %s: tx_vid %d rx_vid1: %d rx_vid2: %d rx_vid3 %d\n",
	       wl->pub->unit, __FUNCTION__, plc->tx_vid, plc->rx_vid1,
	       plc->rx_vid2, plc->rx_vid3);

	/* Save the plc dev pointer for sending the frames */
	sprintf(if_name, "vlan%d", plc->tx_vid);
	plc_dev = dev_get_by_name(if_name);
	ASSERT(plc_dev != NULL);
	dev_put(plc_dev);

	plc->plc_dev = plc_dev;

	WL_PLC("wl%d: %s: PLC dev: %s, WDS dev: %p\n", wl->pub->unit, __FUNCTION__,
	       plc->plc_dev->name, plc->wds_dev);

	/* Register WDS device as master for the PLC so that any
	 * thing received on PLC is sent first to WDS. Frames sent
	 * up to the netif layer will appear as if they are coming
	 * from WDS.
	 */
	wl_plc_master_set(plc, plc_dev, wlif);

	sprintf(if_name, "vlan%d", plc->rx_vid1);
	plc->plc_rxdev1 = dev_get_by_name(if_name);
	ASSERT(plc->plc_rxdev1 != NULL);
	dev_put(plc->plc_rxdev1);
	wl_plc_master_set(plc, plc->plc_rxdev1, wlif);

	sprintf(if_name, "vlan%d", plc->rx_vid2);
	plc->plc_rxdev2 = dev_get_by_name(if_name);
	ASSERT(plc->plc_rxdev2 != NULL);
	dev_put(plc->plc_rxdev2);
	wl_plc_master_set(plc, plc->plc_rxdev2, wlif);

	sprintf(if_name, "vlan%d", plc->rx_vid3);
	plc->plc_rxdev3 = dev_get_by_name(if_name);
	ASSERT(plc->plc_rxdev3 != NULL);
	dev_put(plc->plc_rxdev3);
	wl_plc_master_set(plc, plc->plc_rxdev3, wlif);

	/* Save the wds interface */
	plc->wds_dev = wlif->dev;

	wlif->plc = plc;

	plc->inited = TRUE;

	WL_PLC("wl%d: %s: Initialized PLC dev %p\n", wl->pub->unit, __FUNCTION__, wlif->plc);

	return 0;
}
#endif /* AP */

static void
wl_plc_master_clear(wl_plc_t *plc, struct net_device *plc_dev)
{
	ASSERT(plc_dev != NULL);

	plc_dev->master = NULL;
	plc_dev->flags &= ~IFF_SLAVE;
	plc_dev->master_hook = NULL;
	plc_dev->master_hook_arg = NULL;

	return;
}

void
wl_plc_cleanup(wl_if_t *wlif)
{
	wl_info_t*wl;
	wl_plc_t *plc;

	wl = wlif->wl;

	if (!WLIF_IS_WDS(wlif) || !PLC_ENAB(wl->pub))
		return;

	plc = wlif->plc;
	wl_plc_master_clear(plc, plc->plc_dev);
	wl_plc_master_clear(plc, plc->plc_rxdev1);
	wl_plc_master_clear(plc, plc->plc_rxdev2);
	wl_plc_master_clear(plc, plc->plc_rxdev3);

	MFREE(wl->osh, plc, sizeof(wl_plc_t));

	return;
}
