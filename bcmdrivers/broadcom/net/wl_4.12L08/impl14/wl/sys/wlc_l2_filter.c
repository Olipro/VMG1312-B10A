/*
 * L2 filter source file
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
 * $Id:$
 */
#ifndef L2_FILTER
#error "L2_FILTER is not defined"
#endif	/* L2_FILTER */

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
#include <proto/bcmarp.h>
#include <proto/bcmip.h>
#include <proto/bcmipv6.h>
#include <proto/bcmicmp.h>
#include <proto/bcmudp.h>
#include <proto/bcmdhcp.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_l2_filter.h>
#include <wlc_tdls.h>
#include <wlc_scb.h>
#include <wl_export.h>

#define DEBUG_PROXY_ARP

/* The length of the option including the type and length fields in units of 8 octets */
#define ND_OPTION_LEN_ETHER	1
#define ALIGN_ADJ_BUFLEN		2		/* Adjust for ETHER_HDR_LEN pull in linux
							 * which makes pkt nonaligned
							 */

/* iovar table */
enum {
	IOV_GRAT_ARP_ENABLE,	/* drop gratuitous ARP */
#ifndef L2_FILTER_STA
	IOV_BLOCK_PING,	/* drop ping Echo request packets for RX */
	IOV_BLOCK_TDLS,	/* drop TDLS discovery/setup request packets for RX */
	IOV_BLOCK_STA,		/* drop STA to STA packets for TX */
	IOV_BLOCK_MULTICAST,	/* drop multicast packets for TX */
	IOV_DHCP_UNICAST,	/* dhcp response broadcast to unicast conversion for TX */
	IOV_GTK_PER_STA,	/* unique GTK per STA */
#endif /* L2_FILTER_STA */
	IOV_LAST
	};

static const bcm_iovar_t l2_filter_iovars[] = {
	{"grat_arp", IOV_GRAT_ARP_ENABLE, 0, IOVT_BOOL, 0},
#ifndef L2_FILTER_STA
	{"block_ping", IOV_BLOCK_PING, 0, IOVT_BOOL, 0},
	{"block_tdls", IOV_BLOCK_TDLS, 0, IOVT_BOOL, 0},
	{"block_sta", IOV_BLOCK_STA, 0, IOVT_BOOL, 0},
	{"block_multicast", IOV_BLOCK_MULTICAST, 0, IOVT_BOOL, 0},
	{"dhcp_unicast", IOV_DHCP_UNICAST, 0, IOVT_BOOL, 0},
	{"gtk_per_sta", IOV_GTK_PER_STA, 0, IOVT_BOOL, 0},
#endif /* L2_FILTER_STA */
	{NULL, 0, 0, 0, 0}
};

/* L2 filter module specific state */
struct l2_filter_info {
	wlc_info_t *wlc;	/* pointer to main wlc structure */
	wlc_pub_t *pub;
	osl_t *osh;
	int	scb_handle;		/* scb cubby handle to retrieve data from scb */
	bool grat_arp_enable;
	bool block_ping;
	bool block_tdls;
	bool block_sta;
	bool block_multicast;
	bool dhcp_unicast;
	bool gtk_per_sta;
};

static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

/* local prototypes */
static int wlc_l2_filter_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
	const char *name, void *p, uint plen, void *a, int alen, int vsize,
	struct wlc_if *wlcif);

l2_filter_info_t *
BCMATTACHFN(wlc_l2_filter_attach)(wlc_info_t *wlc)
{
	l2_filter_info_t *l2_filter;

	if (!(l2_filter = (l2_filter_info_t *)MALLOC(wlc->osh, sizeof(l2_filter_info_t)))) {
		WL_ERROR(("wl%d: wlc_l2_filter_attach: out of mem, malloced %d bytes\n",
			wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)l2_filter, sizeof(l2_filter_info_t));

	l2_filter->wlc = wlc;
	l2_filter->pub = wlc->pub;
	l2_filter->osh = wlc->pub->osh;

	/* register module */
	if (wlc_module_register(wlc->pub, l2_filter_iovars, "l2_filter",
		l2_filter, wlc_l2_filter_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: L2 filter wlc_module_register() failed\n", wlc->pub->unit));
		goto fail;
	}

	return l2_filter;

fail:
	MFREE(wlc->osh, l2_filter, sizeof(l2_filter_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_l2_filter_detach)(l2_filter_info_t *l2_filter)
{
	if (!l2_filter)
		return;

	/* sanity */
	wlc_module_unregister(l2_filter->pub, "l2_filter", l2_filter);

	MFREE(l2_filter->wlc->osh, l2_filter, sizeof(l2_filter_info_t));
}

/* handle L2 filter related iovars */
static int
wlc_l2_filter_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	l2_filter_info_t *l2_filter = (l2_filter_info_t *)hdl;
	wlc_info_t *wlc = l2_filter->wlc;
	wlc_bsscfg_t *bsscfg;
	int32 int_val = 0;
	bool bool_val;
	uint32 *ret_uint_ptr;
	int err = 0;

	ASSERT(l2_filter == wlc->l2_filter);

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_uint_ptr = (uint32 *)a;

	switch (actionid) {

	case IOV_GVAL(IOV_GRAT_ARP_ENABLE):
		*ret_uint_ptr = l2_filter->grat_arp_enable;
		break;

	case IOV_SVAL(IOV_GRAT_ARP_ENABLE):
		l2_filter->grat_arp_enable = bool_val;
		break;

#ifndef L2_FILTER_STA
	case IOV_GVAL(IOV_BLOCK_PING):
		*ret_uint_ptr = l2_filter->block_ping;
		break;

	case IOV_SVAL(IOV_BLOCK_PING):
		l2_filter->block_ping = bool_val;
		break;

	case IOV_GVAL(IOV_BLOCK_TDLS):
		*ret_uint_ptr = l2_filter->block_tdls;
		break;

	case IOV_SVAL(IOV_BLOCK_TDLS):
		l2_filter->block_tdls = bool_val;
		break;

	case IOV_GVAL(IOV_BLOCK_STA):
		*ret_uint_ptr = l2_filter->block_sta;
		break;

	case IOV_SVAL(IOV_BLOCK_STA):
		l2_filter->block_sta = bool_val;
		break;

	case IOV_GVAL(IOV_BLOCK_MULTICAST):
		*ret_uint_ptr = l2_filter->block_multicast;
		break;

	case IOV_SVAL(IOV_BLOCK_MULTICAST):
		l2_filter->block_multicast = bool_val;
		break;

	case IOV_GVAL(IOV_DHCP_UNICAST):
		*ret_uint_ptr = l2_filter->dhcp_unicast;
		break;

	case IOV_SVAL(IOV_DHCP_UNICAST):
		l2_filter->dhcp_unicast = bool_val;
		break;

	case IOV_GVAL(IOV_GTK_PER_STA):
		*ret_uint_ptr = l2_filter->gtk_per_sta;
		break;

	case IOV_SVAL(IOV_GTK_PER_STA): {
		eapol_hdr_t *eapolh;
		struct ether_addr ea;
		uint8 eapol[EAPOL_HDR_LEN+1];
		l2_filter->gtk_per_sta = bool_val;
		bzero(&ea, ETHER_ADDR_LEN);
		eapolh = (eapol_hdr_t *)eapol;
		eapolh->version = WPA2_EAPOL_VERSION;
		eapolh->type = 0xFF;
		eapolh->length = HTON16(1);
		eapol[EAPOL_HDR_LEN] = (uint8)bool_val;
		/* notify NAS about unique gtk per STA */
		wlc_bss_eapol_event(wlc, bsscfg, &ea, eapol, EAPOL_HDR_LEN+1);
		break;
	}
#endif /* L2_FILTER_STA */

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

static int
wlc_l2_filter_get_ether_type(wlc_info_t *wlc, void *sdu,
	uint8 **data_ptr, int *len_ptr, uint16 *et_ptr, bool *snap_ptr)
{
	uint8 *frame = PKTDATA(wlc->osh, sdu);
	int length = PKTLEN(wlc->osh, sdu);
	uint8 *pt;			/* Pointer to type field */
	uint16 ethertype;
	bool snap = FALSE;
	/* Process Ethernet II or SNAP-encapsulated 802.3 frames */
	if (length < ETHER_HDR_LEN) {
		WL_ERROR(("wl%d: %s: short eth frame (%d)\n",
		          wlc->pub->unit, __FUNCTION__, length));
		return -1;
	} else if (ntoh16_ua((const void *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		/* Frame is Ethernet II */
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		WL_INFORM(("wl%d: %s: 802.3 LLC/SNAP\n", wlc->pub->unit, __FUNCTION__));
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
		snap = TRUE;
	} else {
		WL_INFORM(("wl%d: %s: non-SNAP 802.3 frame\n",
		          wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	ethertype = ntoh16_ua((const void *)pt);

	/* Skip VLAN tag, if any */
	if (ethertype == ETHER_TYPE_8021Q) {
		pt += VLAN_TAG_LEN;

		if (pt + ETHER_TYPE_LEN > frame + length) {
			WL_ERROR(("wl%d: %s: short VLAN frame (%d)\n",
			          wlc->pub->unit, __FUNCTION__, length));
			return -1;
		}

		ethertype = ntoh16_ua((const void *)pt);
	}

	*data_ptr = pt + ETHER_TYPE_LEN;
	*len_ptr = length - (pt + ETHER_TYPE_LEN - frame);
	*et_ptr = ethertype;
	*snap_ptr = snap;
	return 0;
}

#ifndef L2_FILTER_STA
static int
wlc_l2_filter_get_ip_type(wlc_info_t *wlc, void *sdu,
	uint8 **data_ptr, int *len_ptr, uint8 *prot_ptr)
{
	uint8 *data;
	struct ipv4_hdr *iph;		/* IP frame pointer */
	int ipl;			/* IP frame length */
	uint16 ethertype, ihl, ippktlen;
	uint16 iph_frag;
	uint8 prot;
	bool snap;

	if (wlc_l2_filter_get_ether_type(wlc, sdu, &data, &ipl, &ethertype, &snap) != 0)
		return -1;
	iph = (struct ipv4_hdr *)data;

	if (ethertype != ETHER_TYPE_IP) {
		return -1;
	}

	/* We support IPv4 only */
	if (ipl < IPV4_OPTIONS_OFFSET || (IP_VER(iph) != IP_VER_4)) {
		return -1;
	}

	/* Header length sanity */
	ihl = IPV4_HLEN(iph);

	/*
	 * Packet length sanity; sometimes we receive eth-frame size bigger
	 * than the IP content, which results in a bad tcp chksum
	 */
	ippktlen = ntoh16(iph->tot_len);
	if (ippktlen < ipl) {
		WL_INFORM(("wl%d: %s: extra frame length ignored (%d)\n",
		          wlc->pub->unit, __FUNCTION__, ipl - ippktlen));
		ipl = ippktlen;
	} else if (ippktlen > ipl) {
		WL_ERROR(("wl%d: %s: truncated IP packet (%d)\n",
		          wlc->pub->unit, __FUNCTION__, ippktlen - ipl));
		return -1;
	}

	if (ihl < IPV4_OPTIONS_OFFSET || ihl > ipl) {
		WL_ERROR(("wl%d: %s: IP-header-len (%d) out of range (%d-%d)\n",
		          wlc->pub->unit, __FUNCTION__, ihl, IPV4_OPTIONS_OFFSET, ipl));
		return -1;
	}

	/*
	 * We don't handle fragmented IP packets.  A first frag is indicated by the MF
	 * (more frag) bit and a subsequent frag is indicated by a non-zero frag offset.
	 */
	iph_frag = ntoh16(iph->frag);

	if ((iph_frag & IPV4_FRAG_MORE) || (iph_frag & IPV4_FRAG_OFFSET_MASK) != 0) {
		WL_INFORM(("wl%d: IP fragment not handled\n",
		           wlc->pub->unit));
		return -1;
	}

	prot = IPV4_PROT(iph);

	*data_ptr = (((uint8 *)iph) + ihl);
	*len_ptr = ipl - ihl;
	*prot_ptr = prot;
	return 0;
}
#endif /* L2_FILTER_STA */

/* returns 0 if gratuitous ARP or unsolicited neighbour advertisement */
static int
wl_gratuitous_arp(wlc_info_t *wlc, void *sdu)
{
	uint8 *frame = PKTDATA(wlc->osh, sdu);
	uint16 ethertype;
#ifdef WL_MSGBUF
	char ipbuf[32], eabuf[32];
#endif
	int send_ip_offset, target_ip_offset;
	int iplen;
	int minlen;
	uint8 *data;
	int datalen;
	bool snap;

	if (wlc_l2_filter_get_ether_type(wlc, sdu, &data, &datalen, &ethertype, &snap) != 0)
		return -1;

	if (!ETHER_ISBCAST(frame + ETHER_DEST_OFFSET) &&
		bcmp(&ether_ipv6_mcast, frame + ETHER_DEST_OFFSET, sizeof(ether_ipv6_mcast))) {
		return -1;
	}

	if (ethertype == ETHER_TYPE_ARP) {
		WL_PRUSR("ARP RX", data, datalen);
		send_ip_offset = ARP_SRC_IP_OFFSET;
		target_ip_offset = ARP_TGT_IP_OFFSET;
		iplen = IPV4_ADDR_LEN;
		minlen = ARP_DATA_LEN;
	}
	else if (ethertype == ETHER_TYPE_IPV6) {
		WL_PRUSR("Neighbour advertisement RX", data, datalen);
		send_ip_offset = NEIGHBOR_ADVERTISE_SRC_IPV6_OFFSET;
		target_ip_offset = NEIGHBOR_ADVERTISE_TGT_IPV6_OFFSET;
		iplen = IPV6_ADDR_LEN;
		minlen = target_ip_offset + iplen;

		/* check for neighbour advertisement */
		if (datalen >= minlen && (data[IPV6_NEXT_HDR_OFFSET] != IP_PROT_ICMP6 ||
			data[NEIGHBOR_ADVERTISE_TYPE_OFFSET] != NEIGHBOR_ADVERTISE_TYPE))
			return -1;
	}
	else {
		return -1;
	}

	if (datalen < minlen) {
		WL_ERROR(("wl%d: wl_gratuitous_arp: truncated packet (%d)\n",
		          wlc->pub->unit, datalen));
		return -1;
	}

	if (bcmp(data + send_ip_offset, data + target_ip_offset, iplen) == 0) {
		WL_PRUSR("gratuitous ARP or unsolicitated neighbour advertisement rx'ed",
			data, datalen);
		return 0;
	}

	return -1;
}

#ifndef L2_FILTER_STA
static int
wlc_l2_filter_ping(wlc_info_t *wlc, void *sdu)
{
	uint8 *data;
	struct bcmicmp_hdr *icmph;
	int icmpl;
	uint8 prot;

	if (wlc_l2_filter_get_ip_type(wlc, sdu, &data, &icmpl, &prot) != 0)
		return -1;
	icmph = (struct bcmicmp_hdr *)data;

	if (prot == IP_PROT_ICMP) {
		WL_L2FILTER(("wl%d: recv ICMP %d\n",
		           wlc->pub->unit, icmph->type));

		if (icmph->type == ICMP_TYPE_ECHO_REQUEST)
			return 0;
	}

	return -1;
}

static int
wlc_l2_filter_tdls(wlc_info_t *wlc, void *sdu)
{
	uint8 *pdata;
	int datalen;
	uint16 ethertype;
	uint8 action_field;
	bool snap;

	if (wlc_l2_filter_get_ether_type(wlc, sdu, &pdata, &datalen, &ethertype, &snap) != 0)
		return -1;

	if (ethertype != ETHER_TYPE_89_0D)
		return -1;

	/* validate payload type */
	if (datalen < TDLS_PAYLOAD_TYPE_LEN + 2) {
		WL_ERROR(("wl%d:%s: wrong length for 89-0d eth frame %d\n",
			wlc->pub->unit, __FUNCTION__, datalen));
		return -1;
	}

	/* validate payload type */
	if (*pdata != TDLS_PAYLOAD_TYPE) {
		WL_ERROR(("wl%d:%s: wrong payload type for 89-0d eth frame %d\n",
			wlc->pub->unit, __FUNCTION__, *pdata));
		return -1;
	}
	pdata += TDLS_PAYLOAD_TYPE_LEN;

	/* validate TDLS action category */
	if (*pdata != TDLS_ACTION_CATEGORY_CODE) {
		WL_ERROR(("wl%d:%s: wrong TDLS Category %d\n", wlc->pub->unit,
			__FUNCTION__, *pdata));
		return -1;
	}
	pdata++;

	action_field = *pdata;

	if ((action_field == TDLS_SETUP_REQ) || (action_field == TDLS_DISCOVERY_REQ))
		return 0;

	return -1;
}

static int
wlc_l2_filter_sta(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu)
{
	struct ether_header *eh;
	struct scb *src;
	eh = (struct ether_header*) PKTDATA(wlc->osh, sdu);

	src = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)eh->ether_shost);
	if (src && SCB_ASSOCIATED(src) && (src->bsscfg == bsscfg))
		return 0;

	return -1;
}

static int
wlc_l2_filter_dhcp_unicast(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu)
{
	struct scb *client;
	uint8 *eh = PKTDATA(wlc->osh, sdu);
	uint8 *udph;
	uint8 *dhcp;
	uint8 *chaddr;
	int udpl;
	int dhcpl;
	uint16 port;
	uint8 prot;

	if (!ETHER_ISMULTI(eh + ETHER_DEST_OFFSET))
		return -1;

	if (wlc_l2_filter_get_ip_type(wlc, sdu, &udph, &udpl, &prot) != 0)
		return -1;

	if (prot != IP_PROT_UDP)
		return -1;

	/* check frame length, at least UDP_HDR_LEN */
	if (udpl < UDP_HDR_LEN) {
		WL_ERROR(("wl%d: %s: short UDP frame, ignored\n",
			wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	port = ntoh16_ua((const void *)(udph + UDP_DEST_PORT_OFFSET));
	/* only process DHCP packets from server to client */
	if (port != DHCP_PORT_CLIENT)
		return -1;

	dhcp = udph + UDP_HDR_LEN;
	dhcpl = udpl - UDP_HDR_LEN;

	if (dhcpl < DHCP_CHADDR_OFFSET + ETHER_ADDR_LEN) {
		WL_ERROR(("wl%d: %s: short DHCP frame, ignored\n",
			wlc->pub->unit, __FUNCTION__));
		return -1;
	}

	/* only process DHCP reply(offer/ack) packets */
	if (*(dhcp + DHCP_TYPE_OFFSET) != DHCP_TYPE_REPLY)
		return -1;

	chaddr = dhcp + DHCP_CHADDR_OFFSET;
	client = wlc_scbfind(wlc, bsscfg, (struct ether_addr *)chaddr);

	if (client && SCB_ASSOCIATED(client)) {
		/* replace the Ethernet destination MAC with unicast client HW address */
		bcopy(chaddr, eh + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	}

	return -1;
}

static int
wlc_l2_filter_multicast(wlc_info_t *wlc, void *sdu)
{
	struct ether_header *eh;
	eh = (struct ether_header*) PKTDATA(wlc->osh, sdu);

	if (ETHER_ISMULTI(eh->ether_dhost)) {
		return 0;
	}

	return -1;
}
#endif /* L2_FILTER_STA */

int
wlc_l2_filter_rcv_data_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu)
{
	if (wlc->l2_filter == NULL)
		return -1;

	if (wlc->l2_filter->grat_arp_enable && BSSCFG_STA(bsscfg)) {
		if (wl_gratuitous_arp(wlc, sdu) == 0) {
			PKTFREE(wlc->osh, sdu, FALSE);
			/* packet dropped */
			return 0;
		}
	}

#ifndef L2_FILTER_STA
	if (wlc->l2_filter->block_ping) {
		if (wlc_l2_filter_ping(wlc, sdu) == 0) {
			PKTFREE(wlc->osh, sdu, FALSE);
			WL_L2FILTER(("wl%d: %s: drop ping packet\n",
				wlc->pub->unit, __FUNCTION__));
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->block_tdls) {
		if (wlc_l2_filter_tdls(wlc, sdu) == 0) {
			PKTFREE(wlc->osh, sdu, FALSE);
			/* packet dropped */
			return 0;
		}
	}
#endif /* L2_FILTER_STA */

	/* packet ignored */
	return -1;
}

int
wlc_l2_filter_send_data_frame(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, void *sdu)
{
	if (wlc->l2_filter == NULL)
		return -1;

#ifndef L2_FILTER_STA
	if (wlc->l2_filter->block_sta) {
		if (wlc_l2_filter_sta(wlc, bsscfg, sdu) == 0) {
			WL_L2FILTER(("wl%d: %s: drop STA2STA packet\n",
				wlc->pub->unit, __FUNCTION__));
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->dhcp_unicast) {
		if (wlc_l2_filter_dhcp_unicast(wlc, bsscfg, sdu) == 0) {
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->block_multicast) {
		if (wlc_l2_filter_multicast(wlc, sdu) == 0) {
			/* packet dropped */
			return 0;
		}
	}

	if (wlc->l2_filter->grat_arp_enable && BSSCFG_AP(bsscfg)) {
		if (wl_gratuitous_arp(wlc, sdu) == 0) {
			/* packet dropped */
			return 0;
		}
	}
#endif /* L2_FILTER_STA */

	/* packet ignored */
	return -1;
}
