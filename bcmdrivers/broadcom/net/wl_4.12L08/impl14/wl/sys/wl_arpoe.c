/*
 * ARP Offload
 *
 * The dongle should be able to handle as much of the ARP request and reply traffic as possible
 * without having to wake up the host.  There are 3 ways of offloading the ARP traffic.
 *
 * 1. Partial ARP offload (ARP filtering)
 *	Only the host-ip/host-mac is maintained on the dongle and non-relevant ARP requests
 *	to the host are filtered.
 * 2. ARP Agent
 *	Host-ip/host-mac and arp-table are maintained on the dongle. The dongle may reply to
 *	ARP requests from either the host or peers.
 * 3. Full ARP offload, complete ARP handling is offloaded to the dongle. The dongle must
 *	manage the ethernet SA/DA field of all packets.
 *
 * The code below implements the ARP Agent method of arp-offloading.
 * It supports multihoming hosts and link-local addressing.
 *
 * Supported protocol families: IPV4.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wl_arpoe.c 358531 2012-09-24 12:30:22Z $
 */


#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <proto/ethernet.h>
#include <proto/802.3.h>
#include <proto/bcmip.h>
#include <proto/bcmarp.h>
#include <proto/bcmudp.h>
#include <proto/vlan.h>
#include <bcmendian.h>

#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wl_export.h>

#include <wl_arpoe.h>

#if defined(BCMDBG) || defined(WLMSG_INFORM)
#define WL_MSGBUF
#endif

#define ARP_TABLE_SIZE			16		/* Number of peer host ip addresses */
#define ARP_MAX_AGE_DEFAULT		(20 * 60) 	/* 20 minutes */
#define ARP_PROBE_SIP			0x0		/* arp-probe sip is 0.0.0.0 */
#define ARP_LINK_LOCAL_IPADDR		0xA9FE		/* 169.254 link local address range */
#define ALIGN_ADJ_BUFLEN		2		/* Adjust for ETHER_HDR_LEN pull in linux
							 * which makes pkt nonaligned
							 */

#define LINKLOCAL(ipa)			((uint16)((ntoh32((ipa))) >> 16))
#define ISLINKLOCAL_IPADDR(ipa)		(LINKLOCAL(ipa) == ARP_LINK_LOCAL_IPADDR)
#define ARP_ENTRY_VALID()		(arpi->arp_table[i].age > 0)

/* Arp-Table/Host-Cache lookup results */

#define SAT	0x8	/* Source IP found in Arp-Table */
#define DAT	0x4	/* Dest IP found in Arp-Table */
#define SHC	0x2	/* Source IP found in Host-Cache */
#define DHC	0x1	/* Dest IP found in Host-Cache */

struct arp_entry {
	struct ipv4_addr	ipa;
	struct ether_addr	ea;
	uint32			age;		/* Entry valid when age > 0 */
};

/* ARP private info structure */
struct wl_arp_info {
	wlc_info_t		*wlc;		/* Pointer back to wlc structure */
	uint32			ol_cmpnts;	/* Offload (ol) components OR'd together */
	struct ipv4_addr	host_ip[ARP_MULTIHOMING_MAX];
	struct ether_addr	host_mac;
	struct arp_entry	arp_table[ARP_TABLE_SIZE];
	struct arp_ol_stats_t	arp_ol_stats;
	uint32			arp_max_age;
	bool			fake_arp_reply;	/* Used in rx to distinguish betn real
						 * and fake arp_reply
						 */
#ifdef ARP_ERRTEST
	uint32			errtest;	/* Error injection test modes */
#endif
	wlc_if_t        *wlcif;
};

/* forward declarations */
static int arp_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
                       const char *name, void *p, uint plen, void *a, int alen,
                       int vsize, struct wlc_if *wlcif);

/*
 * IPv4 handler. 'ph' points to protocol specific header,
 * for example, it points to UDP header if it is UDP packet.
 */

/* wlc_pub_t struct access macros */
#define WLCUNIT(arpi)	((arpi)->wlc->pub->unit)
#define WLCOSH(arpi)	((arpi)->wlc->osh)

/* special values */
/* 802.3 llc/snap header */
static const uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

/* IOVar table */
enum {
	IOV_ARP_OL,		/* Flags for enabling/disabling ARP offload sub-features */
	IOV_ARP_PEERAGE,	/* Set peer's age of the ip-mac-address in the arp-table */
	IOV_ARP_TABLE_CLEAR,	/* Clear all entries in the arp_table[] */
	IOV_ARP_HOSTIP,		/* Add local ip address to host_ip[] table */
	IOV_ARP_HOSTIP_CLEAR,	/* Clear all entries in the host_ip[] table */
	IOV_ARP_STATS,		/* Display arp offload statistics. */
	IOV_ARP_STATS_CLEAR,	/* Clear arp offload statistics. */
	IOV_ARP_ERRTEST		/* Error test modes */
};

static const bcm_iovar_t arp_iovars[] = {
	{"arp_ol", IOV_ARP_OL,
	(0), IOVT_UINT32, 0
	},
	{"arp_peerage", IOV_ARP_PEERAGE,
	(0), IOVT_UINT32, 0
	},
	{"arp_table_clear", IOV_ARP_TABLE_CLEAR,
	(0), IOVT_VOID, 0
	},
	{"arp_hostip", IOV_ARP_HOSTIP,
	(0), IOVT_UINT32, 0
	},
	{"arp_hostip_clear", IOV_ARP_HOSTIP_CLEAR,
	(0), IOVT_VOID, 0
	},
	{"arp_stats", IOV_ARP_STATS,
	(0), IOVT_BUFFER, sizeof(struct arp_ol_stats_t)
	},
	{"arp_stats_clear", IOV_ARP_STATS_CLEAR,
	(0), IOVT_VOID, 0
	},
#ifdef ARP_ERRTEST
	{"arp_errtest", IOV_ARP_ERRTEST,
	(0), IOVT_UINT32, 0
	},
#endif
	{NULL, 0, 0, 0, 0 }
};


static void
arp_clean_aged_entries(wl_arp_info_t *arpi)
{
	uint8 i;
#ifdef WL_MSGBUF
	char buf[32];
#endif

	/* cleanup expired entries from the ARP table */
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ARP_ENTRY_VALID()) {
			arpi->arp_table[i].age--;
			/* expire old entry */
			if (arpi->arp_table[i].age == 0) {
				WL_INFORM(("arp_watchdog:AO:ipaddr %s ix %d\n",
				           bcm_ip_ntoa(&arpi->arp_table[i].ipa, buf), i));
			}
		}
	}
}

/*
 * Age entries from the arp-table.
 */
static int
arp_watchdog(void *cntxt)
{
	wl_arp_info_t *arpi = (wl_arp_info_t *)cntxt;
	wlc_if_t *wlcif = arpi->wlc->wlcif_list;
	struct wl_info *wl = arpi->wlc->wl;

	/* WL_INFORM(("wl%d: arp_watchdog()\n", WLCUNIT(arpi))); */

	if (ARPOE_ENAB(arpi->wlc->pub)) {
		/* cleanup expired entries from the primary STA ARP table */
		arp_clean_aged_entries(arpi);

		/* clean entries corr to other iF's also. */
		while (wlcif != NULL) {
			if (wlcif->wlif != NULL) {
				arpi = wl_get_ifctx(wl, IFCTX_ARPI, wlcif->wlif);
				if (arpi != NULL)
					arp_clean_aged_entries(arpi);
			}

			wlcif = wlcif->next;
		}
	}

	return BCME_OK;
}

/*
 * Initialize ARP private context.
 * Returns a pointer to the ARP private context, NULL on failure.
 */
wl_arp_info_t *
BCMATTACHFN(wl_arp_attach)(wlc_info_t *wlc)
{
	wl_arp_info_t *arpi;

	/* allocate arp private info struct */
	arpi = MALLOC(wlc->osh, sizeof(wl_arp_info_t));
	if (!arpi) {
		WL_ERROR(("wl%d: %s: MALLOC failed; total mallocs %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* init arp private info struct */
	bzero(arpi, sizeof(wl_arp_info_t));
	arpi->wlc = wlc;

	/* register module */
	if (wlc_module_register(wlc->pub, arp_iovars, "arp", arpi, arp_doiovar,
	                        arp_watchdog, NULL, NULL)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	arpi->arp_max_age = ARP_MAX_AGE_DEFAULT;
	arpi->fake_arp_reply = FALSE;

	return arpi;
}

void
BCMATTACHFN(wl_arp_detach)(wl_arp_info_t *arpi)
{

	WL_INFORM(("wl%d: arp_detach()\n", WLCUNIT(arpi)));

	if (!arpi)
		return;

	wlc_module_unregister(arpi->wlc->pub, "arp", arpi);
	MFREE(WLCOSH(arpi), arpi, sizeof(wl_arp_info_t));
}

/* Handling ARP-related iovars */
static int
arp_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
            void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wl_arp_info_t *arpi = hdl;
	int err = 0;
	int i;
	uint32 ipaddr;
	uint32 *ret_int_ptr = (uint32 *)a;

	WL_INFORM(("wl%d: arp_doiovar()\n", WLCUNIT(arpi)));

	/* donot use this ioctl if the module is not yet enabled */
	if (!ARPOE_ENAB(arpi->wlc->pub)) {
		return BCME_UNSUPPORTED;
	}

	/* change arpi if wlcif is corr to a virtualIF */
	if (wlcif != NULL) {
		if (wlcif->wlif != NULL) {
			arpi = (wl_arp_info_t *)wl_get_ifctx(arpi->wlc->wl, IFCTX_ARPI,
			                                     wlcif->wlif);
		}
	}

	switch (actionid) {

	case IOV_SVAL(IOV_ARP_OL):
		bcopy(a, &arpi->ol_cmpnts, sizeof(arpi->ol_cmpnts));
		break;

	case IOV_GVAL(IOV_ARP_OL):
		bcopy(&arpi->ol_cmpnts, a, sizeof(arpi->ol_cmpnts));
		break;

	case IOV_SVAL(IOV_ARP_PEERAGE):
		bcopy(a, &arpi->arp_max_age, sizeof(uint32));
		break;

	case IOV_GVAL(IOV_ARP_PEERAGE):
		*ret_int_ptr = arpi->arp_max_age;
		break;

	case IOV_SVAL(IOV_ARP_TABLE_CLEAR):
		/* Invalidate all entries (set age to zero) */
		bzero(arpi->arp_table, sizeof(struct arp_entry) * ARP_TABLE_SIZE);
		break;

	case IOV_SVAL(IOV_ARP_HOSTIP_CLEAR):
		bzero(arpi->host_ip, sizeof(struct ipv4_addr) * ARP_MULTIHOMING_MAX);
		break;

	case IOV_SVAL(IOV_ARP_HOSTIP):
	{
		/* Add one IP address to the host IP table. */
		if (plen < sizeof(struct ipv4_addr))
			return BCME_BUFTOOSHORT;

		if (ETHER_ISNULLADDR(&arpi->host_mac))
			bcopy(&arpi->wlc->pub->cur_etheraddr, &arpi->host_mac, ETHER_ADDR_LEN);

		/*
		 * Requested ip-addr not found in the host_ip[] table; add it.
		 * Use link-local static address setting ONLY for testing
		 * and warn.  Link-local is used for dynamic addr configuration.
		 */
		bcopy(a, &ipaddr, IPV4_ADDR_LEN);
		if (ISLINKLOCAL_IPADDR(ipaddr)) {
			WL_ERROR(("wl%d: link local addresses being set! watch out!!\n",
			          WLCUNIT(arpi)));
			/* return BCME_BADADDR; */
		}

		/* copy the host-requested ip-addr into an empty entry of host_ip[] */
		err = BCME_NORESOURCE;

		for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
			if (IPV4_ADDR_NULL(arpi->host_ip[i].addr)) {
				bcopy(a, arpi->host_ip[i].addr, IPV4_ADDR_LEN);
				err = 0;
				break;
			}
		}

		break;
	}

	case IOV_GVAL(IOV_ARP_HOSTIP):
	{
		uint8 *hst_ip = (uint8 *)a;

		/*
		 * Return all IP addresses from host table.
		 * The return buffer is a list of valid IP addresses terminated by an address
		 * of all zeroes.
		 */

		for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
			if (!IPV4_ADDR_NULL(arpi->host_ip[i].addr)) {
				if (alen < sizeof(struct ipv4_addr))
					return BCME_BUFTOOSHORT;
				bcopy(arpi->host_ip[i].addr, hst_ip, IPV4_ADDR_LEN);
				hst_ip += IPV4_ADDR_LEN;
				alen -= sizeof(struct ipv4_addr);
			}
		}

		if (alen < sizeof(struct ipv4_addr))
			return BCME_BUFTOOSHORT;

		bzero(hst_ip, IPV4_ADDR_LEN);
		break;
	}

	case IOV_SVAL(IOV_ARP_STATS_CLEAR):
		bzero(&arpi->arp_ol_stats, sizeof(struct arp_ol_stats_t));
		break;

	case IOV_GVAL(IOV_ARP_STATS):
		if (alen < sizeof(struct arp_ol_stats_t))
			return BCME_BUFTOOSHORT;

		/* find number of valid arp entries */
		arpi->arp_ol_stats.arp_table_entries = 0;
		for (i = 0; i < ARP_TABLE_SIZE; i++) {
			if (ARP_ENTRY_VALID())
				arpi->arp_ol_stats.arp_table_entries++;
		}

		/* find number of valid host_ip entries */
		arpi->arp_ol_stats.host_ip_entries = 0;
		for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
			if (!IPV4_ADDR_NULL(arpi->host_ip[i].addr))
				arpi->arp_ol_stats.host_ip_entries++;
		}

		bcopy(&arpi->arp_ol_stats, a, sizeof(struct arp_ol_stats_t));
		break;

#ifdef ARP_ERRTEST
	case IOV_SVAL(IOV_ARP_ERRTEST):
		bcopy(a, &arpi->errtest, sizeof(arpi->errtest));
		break;

	case IOV_GVAL(IOV_ARP_ERRTEST):
		bcopy(&arpi->errtest, a, sizeof(arpi->errtest));
		break;
#endif

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

static bool
lookuparptable(wl_arp_info_t *arpi, uint8 *ipa, struct ether_addr **ea)
{
	int i;
#ifdef WL_MSGBUF
	char eabuf[32], ipbuf[32];
#endif

	WL_INFORM(("wl%d: lookuparptable: ip=%s\n",
	           WLCUNIT(arpi),
	           bcm_ip_ntoa((struct ipv4_addr *)ipa, ipbuf)));

	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ARP_ENTRY_VALID() &&
		    (bcmp(arpi->arp_table[i].ipa.addr, ipa, IPV4_ADDR_LEN) == 0)) {
			*ea = &arpi->arp_table[i].ea;
			WL_INFORM(("wl%d: lookuparptable: found; ea=%s\n",
			           WLCUNIT(arpi),
			           bcm_ether_ntoa((struct ether_addr *)*ea, eabuf)));
			return TRUE;
		}
	}

	WL_INFORM(("wl%d: lookuparptable: not found\n", WLCUNIT(arpi)));

	*ea = NULL;
	return FALSE;
}

static bool
lookuphstcache(wl_arp_info_t *arpi, uint8 *ipa)
{
	int i;
#ifdef WL_MSGBUF
	char ipbuf[32];
#endif

	WL_INFORM(("wl%d: lookuphstcache: ip=%s\n",
	           WLCUNIT(arpi),
	           bcm_ip_ntoa((struct ipv4_addr *)ipa, ipbuf)));

	for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
		if (bcmp(arpi->host_ip[i].addr, ipa, IPV4_ADDR_LEN) == 0) {
			WL_INFORM(("wl%d: lookuphstcache: found\n", WLCUNIT(arpi)));
			return TRUE;
		}
	}

	WL_INFORM(("wl%d: lookuphstcache: not found\n", WLCUNIT(arpi)));

	return FALSE;
}

static int
savehostipmac(wl_arp_info_t *arpi, uint8 *ipa, uint8 *ea)
{
	int i;
#ifdef WL_MSGBUF
	char ipbuf[32], eabuf[32];
#endif

	WL_INFORM(("wl%d: savehostipmac: ip=%s ea=%s\n",
	           WLCUNIT(arpi),
	           bcm_ip_ntoa((struct ipv4_addr *)ipa, ipbuf),
	           bcm_ether_ntoa((struct ether_addr *)ea, eabuf)));

	/*
	 * Allow only non-null and non-broadcast addresses to be
	 * stored in the tables
	 */
	if ((IPV4_ADDR_NULL(ipa)) || (IPV4_ADDR_BCAST(ipa)) ||
	    (ETHER_ISNULLADDR(ea)) || (ETHER_ISBCAST(ea))) {
		WL_INFORM(("wl%d: savehostipmac: null or bcast ignored\n",
		           WLCUNIT(arpi)));
		return -1;
	}

	/* The first entry to be set in the host table determines the host MAC address. */
	if (ETHER_ISNULLADDR(&arpi->host_mac)) {
		bcopy(ipa, arpi->host_ip[0].addr, IPV4_ADDR_LEN);
		bcopy(ea, &arpi->host_mac, ETHER_ADDR_LEN);
		WL_INFORM(("wl%d: savehostipmac: first entry; set ea\n",
		           WLCUNIT(arpi)));
		return 0;
	}

	/* if mac-addr is set then it should be equal to what is passed by the caller */
	if (bcmp(&arpi->host_mac, ea, ETHER_ADDR_LEN) != 0) {
		WL_ERROR(("wl%d: %s: not adding host IP with different MAC\n",
		          WLCUNIT(arpi), __FUNCTION__));
		return -1; /* mac address error */
	}

	/* see if ip address already exists in host_ip[] table */
	for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
		if (bcmp(arpi->host_ip[i].addr, ipa, IPV4_ADDR_LEN) == 0) {
			WL_INFORM(("wl%d: savehostipmac: ip-addr already in table\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	}

	/* save the ip-addr in host_ip[] table */
	for (i = 0; i < ARP_MULTIHOMING_MAX; i++) {
		if (IPV4_ADDR_NULL(arpi->host_ip[i].addr)) {
			bcopy(ipa, arpi->host_ip[i].addr, IPV4_ADDR_LEN);
			WL_INFORM(("wl%d: savehostipmac: added\n", WLCUNIT(arpi)));
			return 0;
		}
	}

	arpi->arp_ol_stats.host_ip_overflow++;

	WL_ERROR(("wl%d: %s: overflow\n", WLCUNIT(arpi), __FUNCTION__));

	return -1; /* couldn't save ip-address in the host_ip[] table */
}

static int
savearptable(wl_arp_info_t *arpi, uint8 *ipa, uint8 *ea)
{
	int i;
#ifdef WL_MSGBUF
	char eabuf[32], ipbuf[32];
#endif

	WL_INFORM(("wl%d: savearptable: ip=%s ea=%s\n",
	           WLCUNIT(arpi),
	           bcm_ip_ntoa((struct ipv4_addr *)ipa, ipbuf),
	           bcm_ether_ntoa((struct ether_addr *)ea, eabuf)));

	/* Allow only non-null and non-broadcast addresses to be
	 * stored in the tables
	 */
	if ((IPV4_ADDR_NULL(ipa)) || (IPV4_ADDR_BCAST(ipa)) ||
	    (ETHER_ISNULLADDR(ea)) || (ETHER_ISBCAST(ea)))
		return -1;

	/* check if ipaddr-ethaddr entry already exists in the arp-table
	 * and it is not fake arp-reply, renew the age.
	 */

	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ARP_ENTRY_VALID() &&
		    (bcmp(&arpi->arp_table[i].ea, ea, ETHER_ADDR_LEN) == 0) &&
		    (bcmp(arpi->arp_table[i].ipa.addr, ipa, IPV4_ADDR_LEN) == 0)) {

			/* Refresh the arp entry.  Ideally, should be done
			 * in rx-path ONLY but we also do it in tx arp-reply
			 * as its anyway a response to arp-request
			 * that had refreshed the arp-table "recently".
			 */
			arpi->arp_table[i].age = arpi->arp_max_age;

			WL_INFORM(("wl%d: savearptable: refreshed age of existing entry\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	}

	/* add new entry to arp-table */
	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (arpi->arp_table[i].age == 0) {
			bcopy(ipa, arpi->arp_table[i].ipa.addr, IPV4_ADDR_LEN);
			bcopy(ea, &arpi->arp_table[i].ea, ETHER_ADDR_LEN);
			arpi->arp_table[i].age = arpi->arp_max_age;
			WL_INFORM(("wl%d: savearptable: added\n", WLCUNIT(arpi)));
			return 0;
		}
	}

	WL_INFORM(("wl%d: savearptable: overflow\n", WLCUNIT(arpi)));

	arpi->arp_ol_stats.arp_table_overflow++;

	return -1;

}

static bool
peer_smac_changed(wl_arp_info_t *arpi, struct bcmarp *arp)
{
	int i;
#ifdef WL_MSGBUF
	char eabuf[32], ipbuf[32];
#endif

	WL_INFORM(("wl%d: peer_smac_changed: ip=%s ea=%s\n",
	           WLCUNIT(arpi),
	           bcm_ether_ntoa((struct ether_addr *)arp->src_eth, eabuf),
	           bcm_ip_ntoa((struct ipv4_addr *)arp->src_ip, ipbuf)));

	for (i = 0; i < ARP_TABLE_SIZE; i++) {
		if (ARP_ENTRY_VALID() && (bcmp(arpi->arp_table[i].ipa.addr,
		                               arp->src_ip, IPV4_ADDR_LEN) == 0)) {
			if (bcmp(&arpi->arp_table[i].ea, arp->src_eth,
			         ETHER_ADDR_LEN) != 0) {
				WL_INFORM(("wl%d: update existing entry\n", WLCUNIT(arpi)));
				/* rfc2002.  Update arp-table with new mac address */
				bcopy(arp->src_eth, &arpi->arp_table[i].ea, ETHER_ADDR_LEN);
				return TRUE;
			}
		}
	}

	WL_INFORM(("wl%d: peer_smac_changed: no change\n", WLCUNIT(arpi)));

	return FALSE;
}

static int
arp_reply_host(wl_arp_info_t *arpi, struct bcmarp *arp_req, struct ether_addr *dst_mac, bool snap)
{
	void *pkt;
	uint8 *frame;
	struct bcmarp *arp_reply;
	uint16 pktlen = (ETHER_HDR_LEN +
	                 ARP_DATA_LEN +
	                 ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	uint32 ipaddr;
	uint16 header_overhead = BCMDONGLEOVERHEAD*3;

	WL_INFORM(("wl%d: arp_reply_host()\n", WLCUNIT(arpi)));

	if (!(pkt = PKTGET(WLCOSH(arpi), pktlen + ALIGN_ADJ_BUFLEN + header_overhead, FALSE))) {
		WL_ERROR(("wl%d: cannot do local arp-resp: unable to alloc\n",
		          WLCUNIT(arpi)));
		WLCNTINCR(arpi->wlc->pub->_cnt->rxnobuf);
		return -1;
	}

	/*
	 * Adjust for pkt alignment problems when pkt is pulled by
	 * 14bytes of ETHER_HDR_LEN in linux osl
	 */
	PKTPULL(WLCOSH(arpi), pkt, ALIGN_ADJ_BUFLEN + header_overhead);

	WL_INFORM(("wl%d: arp_reply_host: servicing request from host\n", WLCUNIT(arpi)));

	frame = PKTDATA(WLCOSH(arpi), pkt);

	/* Create 14-byte eth header, plus snap header if applicable */

	/* broadcast reply if ip-addr is link-local */
	bcopy(arp_req->dst_ip, &ipaddr, IPV4_ADDR_LEN);

	if (ISLINKLOCAL_IPADDR(ipaddr))
		bcopy(&ether_bcast, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	else
		bcopy(arp_req->src_eth, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);

#ifdef ARP_ERRTEST
	/* Corrupt peer mac-addr for testing */
	if (arpi->errtest & ARP_ERRTEST_REPLY_HOST)
		dst_mac->octet[0]++;
#endif

	bcopy(dst_mac, frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);

#ifdef ARP_ERRTEST
	/* Restore corrupted peer mac-addr for testing */
	if (arpi->errtest & ARP_ERRTEST_REPLY_HOST)
		dst_mac->octet[0]--;
#endif

	if (snap) {
		hton16_ua_store(pktlen, frame + ETHER_TYPE_OFFSET);
		bcopy(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN);
		hton16_ua_store(ETHER_TYPE_ARP, frame + ETHER_HDR_LEN + SNAP_HDR_LEN);
	} else
		hton16_ua_store(ETHER_TYPE_ARP, frame + ETHER_TYPE_OFFSET);

	/* move past eth/eth-snap header */
	arp_reply = (struct bcmarp *)(frame + pktlen - ARP_DATA_LEN);

	/* Create 28-byte arp-reply data frame */

	/* copy first 6 bytes as-is; i.e., htype, ptype, hlen, plen */
	bcopy(arp_req, arp_reply, ARP_OPC_OFFSET);
	arp_reply->oper = HTON16(ARP_OPC_REPLY);
	/* Copy dst eth and ip addresses */
	bcopy(dst_mac, arp_reply->src_eth, ETHER_ADDR_LEN);
	bcopy(arp_req->dst_ip, arp_reply->src_ip, IPV4_ADDR_LEN);
	bcopy(arp_req->src_eth, arp_reply->dst_eth, ETHER_ADDR_LEN);
	bcopy(arp_req->src_ip, arp_reply->dst_ip, IPV4_ADDR_LEN);

#ifdef BCMDBG
	if (WL_PRPKT_ON())
		prpkt("arp_reply_host: response packet", WLCOSH(arpi), pkt);
#endif

	arpi->arp_ol_stats.host_service++;

	arpi->fake_arp_reply = TRUE;
	wl_sendup(arpi->wlc->wl, ((arpi->wlcif != NULL) ? arpi->wlcif->wlif : NULL), pkt, 1);
	arpi->fake_arp_reply = FALSE;

	return ARP_REPLY_HOST;
}

static int
arp_reply_peer(wl_arp_info_t *arpi, struct bcmarp *arp_req, bool snap)
{
	void *pkt;
	uint8 *frame;
	struct bcmarp *arp_reply;
	uint16 pktlen = (ETHER_HDR_LEN +
	                 ARP_DATA_LEN +
	                 ((snap == TRUE) ? (SNAP_HDR_LEN + ETHER_TYPE_LEN) : 0));
	uint32 ipaddr;

	WL_INFORM(("wl%d: arp_reply_peer()\n", WLCUNIT(arpi)));

	if (!(pkt = PKTGET(WLCOSH(arpi), pktlen, TRUE))) {
		WL_ERROR(("wl%d: %s: alloc failed; dropped\n",
		          WLCUNIT(arpi), __FUNCTION__));
		WLCNTINCR(arpi->wlc->pub->_cnt->rxnobuf);
		return -1;
	}

	WL_INFORM(("wl%d: arp_reply_peer: servicing request from peer\n", WLCUNIT(arpi)));

	frame = PKTDATA(WLCOSH(arpi), pkt);

	/* Create 14-byte eth header, plus snap header if applicable */

	/* broadcast reply if ip-addr is link-local */
	bcopy(arp_req->src_ip, &ipaddr, IPV4_ADDR_LEN);

	if (ISLINKLOCAL_IPADDR(ipaddr))
		bcopy(&ether_bcast, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);
	else
		bcopy(arp_req->src_eth, frame + ETHER_DEST_OFFSET, ETHER_ADDR_LEN);

#ifdef ARP_ERRTEST
	/* corrupt host mac addr for testing */
	if (arpi->errtest & ARP_ERRTEST_REPLY_PEER)
		arpi->host_mac.octet[0]++;
#endif

	bcopy(&arpi->host_mac, frame + ETHER_SRC_OFFSET, ETHER_ADDR_LEN);

#ifdef ARP_ERRTEST
	/* Restore hosts corrupted mac-addr used for testing */
	if (arpi->errtest & ARP_ERRTEST_REPLY_PEER)
		arpi->host_mac.octet[0]--;
#endif

	if (snap) {
		hton16_ua_store(pktlen, frame + ETHER_TYPE_OFFSET);
		bcopy(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN);
		hton16_ua_store(ETHER_TYPE_ARP, frame + ETHER_HDR_LEN + SNAP_HDR_LEN);
	} else
		hton16_ua_store(ETHER_TYPE_ARP, frame + ETHER_TYPE_OFFSET);

	/* move past eth/eth-snap header */
	arp_reply = (struct bcmarp *)(frame + pktlen - ARP_DATA_LEN);

	/* Create 28-byte arp-reply data frame */

	/* copy first 6 bytes as-is: htype, ptype, hlen, plen */
	bcopy(arp_req, arp_reply, ARP_OPC_OFFSET);
	arp_reply->oper = HTON16(ARP_OPC_REPLY);
	/* Copy dst eth and ip addresses */
	bcopy(&arpi->host_mac, arp_reply->src_eth, ETHER_ADDR_LEN);
	bcopy(arp_req->dst_ip, arp_reply->src_ip, IPV4_ADDR_LEN);
	bcopy(arp_req->src_eth, arp_reply->dst_eth, ETHER_ADDR_LEN);
	bcopy(arp_req->src_ip, arp_reply->dst_ip, IPV4_ADDR_LEN);

#ifdef BCMDBG
	if (WL_PRPKT_ON())
		prpkt("arp_reply_peer: response packet", WLCOSH(arpi), pkt);
#endif

	arpi->arp_ol_stats.peer_service++;

	wlc_sendpkt(arpi->wlc, pkt, arpi->wlcif);

	return ARP_REPLY_PEER;
}

/*
 *  The received packet formats are different when EXT_STA is enabled. In case
 *  of EXT_STA the received packets are in 802.11 format, where as in other
 *  case the received packets have Ethernet II format
 *
 *  1. 802.11 frames
 *  -------------------------------------------------------------------------------------------
 *  | FC(2) | DID(2) |  A1(6) |  A2(6) | A3(6) | SID(2) | SNAP(6) | type(2) | data(46 - 1500) |
 *  -------------------------------------------------------------------------------------------
 *
 *  2. Ethernet II frames
 *  ---------------------------------------------
 *  | DA(6) | SA(6) | type(2) | data(46 - 1500) |
 *  ---------------------------------------------
 */

/* Returns -1 if frame is not IP; otherwise, returns pointer/length of IP portion */
static int
wl_arp_parse(wl_arp_info_t *arpi, void *sdu,
             struct bcmarp **arp_out, bool *snap_out)
{
	uint8 *frame = PKTDATA(WLCOSH(arpi), sdu);
	int length = PKTLEN(WLCOSH(arpi), sdu);
	uint8 *pt;
	uint16 ethertype;
	struct bcmarp *arp;
	int arplen;
	bool snap = FALSE;
#ifdef WL_MSGBUF
	char ipbuf[32], eabuf[32];
#endif

	/* Process Ethernet II or SNAP-encapsulated 802.3 frames */
	if (length < ETHER_HDR_LEN) {
		WL_ERROR(("wl%d: %s: short eth frame (%d)\n",
		          WLCUNIT(arpi), __FUNCTION__, length));
		return -1;
	} else if (ntoh16_ua((const void *)(frame + ETHER_TYPE_OFFSET)) >= ETHER_TYPE_MIN) {
		/* Frame is Ethernet II */
		pt = frame + ETHER_TYPE_OFFSET;
	} else if (length >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN &&
	           !bcmp(llc_snap_hdr, frame + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
		WL_INFORM(("wl%d: wl_arp_parse: 802.3 LLC/SNAP\n", WLCUNIT(arpi)));
		pt = frame + ETHER_HDR_LEN + SNAP_HDR_LEN;
		snap = TRUE;
	} else {
		WL_ERROR(("wl%d: %s: non-SNAP 802.3 frame\n",
		          WLCUNIT(arpi), __FUNCTION__));
		return -1;
	}

	ethertype = ntoh16_ua((const void *)pt);

	/* Skip VLAN tag, if any */
	if (ethertype == ETHER_TYPE_8021Q) {
		pt += VLAN_TAG_LEN;

		if (pt + ETHER_TYPE_LEN > frame + length) {
			WL_ERROR(("wl%d: %s: short VLAN frame (%d)\n",
			          WLCUNIT(arpi), __FUNCTION__, length));
			return -1;
		}

		ethertype = ntoh16_ua((const void *)pt);
	}

	if (ethertype != ETHER_TYPE_ARP) {
		WL_INFORM(("wl%d: wl_arp_parse: non-ARP frame (ethertype 0x%x, length %d)\n",
		           WLCUNIT(arpi), ethertype, length));
		return -1;
	}

	arp = (struct bcmarp *)(pt + ETHER_TYPE_LEN);
	arplen = length - (pt + ETHER_TYPE_LEN - frame);

	if (arplen > ARP_DATA_LEN) {
		WL_ERROR(("wl%d: %s: extra frame length ignored (%d)\n",
		          WLCUNIT(arpi), __FUNCTION__, arplen - ARP_DATA_LEN));
		arplen = ARP_DATA_LEN;
	} else if (arplen < ARP_DATA_LEN) {
		WL_ERROR(("wl%d: %s: truncated ARP packet (%d)\n",
		          WLCUNIT(arpi), __FUNCTION__, arplen));
		return -1;
	}

	/* non probe pkt; could be link-local or routable arp-request */

	WL_INFORM(("wl%d: wl_arp_parse: ARP %s, %sSNAP\n",
	           WLCUNIT(arpi),
	           (arp->oper == HTON16(ARP_OPC_REQUEST)) ? "request" : "response",
	           snap ? "" : "non-"));
	WL_INFORM(("wl%d: wl_arp_parse:   SA %s  SIP %s\n",
	           WLCUNIT(arpi),
	           bcm_ether_ntoa((struct ether_addr *)arp->src_eth, eabuf),
	           bcm_ip_ntoa((struct ipv4_addr *)arp->src_ip, ipbuf)));
	WL_INFORM(("wl%d: wl_arp_parse:   DA %s  DIP %s\n",
	           WLCUNIT(arpi),
	           bcm_ether_ntoa((struct ether_addr *)arp->dst_eth, eabuf),
	           bcm_ip_ntoa((struct ipv4_addr *)arp->dst_ip, ipbuf)));

	*arp_out = arp;
	*snap_out = snap;

	return 0;
}

int
wl_arp_send_proc(wl_arp_info_t *arpi, void *sdu)
{
	struct bcmarp *arp;
	bool snap;
	bool sip_in_arp_table, dip_in_arp_table, sip_in_host_cache, dip_in_host_cache;
	uint32 sat_dat_shc_dhc;
	struct ether_addr *dst_mac, *src_mac;
	uint32 ipaddr;

	WL_INFORM(("wl%d: wl_arp_send_proc()\n", WLCUNIT(arpi)));

	/* if no arp offload components are enabled then quit */
	if ((arpi->ol_cmpnts & ARP_OL_AGENT) == 0 ||
	    (!(arpi->ol_cmpnts & ARP_OL_HOST_AUTO_REPLY) && !(arpi->ol_cmpnts & ARP_OL_SNOOP))) {
		WL_INFORM(("wl%d: wl_arp_send_proc: no components enabled\n", WLCUNIT(arpi)));
		return 0;
	}

#ifdef BCMDBG
	if (WL_PRPKT_ON())
		prpkt("wl_arp_send_proc: arp txpkt (MPDU)", WLCOSH(arpi), sdu);
#endif

	/* Parse ARP packet and do table lookups */
	if (wl_arp_parse(arpi, sdu, &arp, &snap) < 0)
		return -1;

	/*
	 * Create a 4-bit value by collating return values if sip/dip exists in arp-table[]
	 * or host_ip[] table.  We will switch case on these 16 values to take various actions.
	 */

	sip_in_arp_table = lookuparptable(arpi, arp->src_ip, &src_mac);		/* sat */
	dip_in_arp_table = lookuparptable(arpi, arp->dst_ip, &dst_mac);		/* dat */
	sip_in_host_cache = lookuphstcache(arpi, arp->src_ip);			/* shc */
	dip_in_host_cache = lookuphstcache(arpi, arp->dst_ip);			/* dhc */

	sat_dat_shc_dhc = ((sip_in_arp_table ? SAT : 0) +
	                   (dip_in_arp_table ? DAT : 0) +
	                   (sip_in_host_cache ? SHC : 0) +
	                   (dip_in_host_cache ? DHC : 0));


	if (arp->oper == HTON16(ARP_OPC_REQUEST)) {

		WL_INFORM(("wl%d: wl_arp_send_proc: arp-tx-request\n", WLCUNIT(arpi)));
		arpi->arp_ol_stats.host_request++;

		/* sip for link-local arp-probe is 0.0.0.0, 3 such probes are expected in
		 * 3 seconds.
		 */
		bcopy(arp->src_ip, &ipaddr, IPV4_ADDR_LEN);
		if (ipaddr == ARP_PROBE_SIP) {
			WL_INFORM(("wl%d: wl_arp_send_proc: tx-arp-probe for link-local ip\n",
			           WLCUNIT(arpi)));
			return 0;
		}

		switch (sat_dat_shc_dhc) {
		case 0:
			if (arpi->ol_cmpnts & ARP_OL_SNOOP) {
				if (savehostipmac(arpi, arp->src_ip, arp->src_eth) != 0)
					return -1; /* bypass complete arp-agent */

				WL_INFORM(("wl%d: wl_arp_send_proc: saved host-ip/mac\n",
				           WLCUNIT(arpi)));
				return 0;
			}
			return 0;
		case DAT:
			if (arpi->ol_cmpnts & ARP_OL_SNOOP) {
				if (savehostipmac(arpi, arp->src_ip, arp->src_eth) == 0) {
					WL_INFORM(("wl%d: wl_arp_send_proc: saved host-ip/mac\n",
					           WLCUNIT(arpi)));
					if (arpi->ol_cmpnts & ARP_OL_HOST_AUTO_REPLY)
						return arp_reply_host(arpi, arp, dst_mac, snap);
				} else
					return -1; /* bypass complete arp-agent */
			}
			return 0;

		case DAT + SHC:
			if (arpi->ol_cmpnts & ARP_OL_HOST_AUTO_REPLY) {
				WL_INFORM(("wl%d: wl_arp_send_proc: "
				           "dip in arp table, sip in hostcache \n",
				           WLCUNIT(arpi)));
				return arp_reply_host(arpi, arp, dst_mac, snap);
			}
			/* Fall Through */

		default:
			WL_INFORM(("wl%d: wl_arp_send_proc: conflicting arp-req case\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	} else if (arp->oper == HTON16(ARP_OPC_REPLY)) {

		WL_INFORM(("wl%d: wl_arp_send_proc: arp-tx-reply\n", WLCUNIT(arpi)));
		arpi->arp_ol_stats.host_reply++;

		switch (sat_dat_shc_dhc) {
		case 0:
			if (arpi->ol_cmpnts & ARP_OL_SNOOP)
				savehostipmac(arpi, arp->src_ip, arp->src_eth);

			if (savearptable(arpi, arp->dst_ip, arp->dst_eth) != 0)
				WL_ERROR(("wl%d: %s: savearptable unsuccessful!\n",
				          WLCUNIT(arpi), __FUNCTION__));

			return 0; /* Behaves as if arp-table was not there */

		case SHC:
			savearptable(arpi, arp->dst_ip, arp->dst_eth);
			return 0;
		case DAT:
			if (arpi->ol_cmpnts & ARP_OL_SNOOP)
				savehostipmac(arpi, arp->src_ip, arp->src_eth);

			return 0;
		default:
			WL_INFORM(("wl%d: wl_arp_send_proc: conflicting arp-reply case\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	} else {
		WL_ERROR(("wl%d: %s: unknown ARP oper 0x%x\n",
		          WLCUNIT(arpi), __FUNCTION__, ntoh16_ua((const void *)&arp->oper)));
		return -1;
	}

	return 0;
}

int
wl_arp_recv_proc(wl_arp_info_t *arpi, void *sdu)
{
	struct bcmarp *arp;
	bool snap;
	bool sip_in_arp_table, dip_in_arp_table, sip_in_host_cache, dip_in_host_cache;
	uint32 sat_dat_shc_dhc;
	struct ether_addr *dst_mac, *src_mac;
	uint32 ipaddr;

	WL_INFORM(("wl%d: wl_arp_recv_proc()\n", WLCUNIT(arpi)));

	if ((arpi->ol_cmpnts & ARP_OL_AGENT) == 0)
		return -1;

	/* Parse ARP packet and do table lookups */
	if (wl_arp_parse(arpi, sdu, &arp, &snap) < 0)
		return -1;

	/* Give fake arp-reply to host directly without getting into arp-offload-engine. */
	if (arpi->fake_arp_reply == TRUE)
		return 0;

	/*
	 * Create a 4-bit value by collating return values if sip/dip exists in arp-table[]
	 * or host_ip[] table.  We will switch case on these 16 values to take various actions.
	 */

	sip_in_arp_table = lookuparptable(arpi, arp->src_ip, &src_mac);		/* sat */
	dip_in_arp_table = lookuparptable(arpi, arp->dst_ip, &dst_mac);		/* dat */
	sip_in_host_cache = lookuphstcache(arpi, arp->src_ip);			/* shc */
	dip_in_host_cache = lookuphstcache(arpi, arp->dst_ip);			/* dhc */

	sat_dat_shc_dhc = ((sip_in_arp_table ? SAT : 0) +
	                   (dip_in_arp_table ? DAT : 0) +
	                   (sip_in_host_cache ? SHC : 0) +
	                   (dip_in_host_cache ? DHC : 0));

	bcopy(arp->src_ip, &ipaddr, IPV4_ADDR_LEN);

	if (arp->oper == HTON16(ARP_OPC_REQUEST)) {
		WL_INFORM(("wl%d: wl_arp_recv_proc: arp-rx-request\n", WLCUNIT(arpi)));
		arpi->arp_ol_stats.peer_request++;

		/* If host_mac value is not set, should not arp_reply_peer() */
		if (ETHER_ISNULLADDR(&arpi->host_mac))
			return ARP_FORCE_FORWARD;

		/* arp probe pkt */
		if (ipaddr == ARP_PROBE_SIP) {
			WL_INFORM(("wl%d: wl_arp_recv_proc: rx-arp-probe for link-local ip\n",
			           WLCUNIT(arpi)));
			return 0;
		}

		/*
		 * Multi-homing host sends arp-req from its primary ipaddr,
		 * not extra multihomed addresses, resulting in host_ip[] table
		 * having only ONE ipaddr. This table will be updated with
		 * multi-homed address when peer tries to talk to host's
		 * multi-homed addresses and then host sends an arp-req/resp.
		 */
		switch (sat_dat_shc_dhc) {
/*
 * Host to accept unknown arp requests which need not be destined to it or
 * host_ip[] table is empty.  Flag ARP_OL_SNOOP which is normally used in
 * tx direction is overloaded for this purpose in rx direction.
 */
#define ARP_ACCEPT_UNKNOWN	ARP_OL_SNOOP
		case 0:
			if (arpi->ol_cmpnts & ARP_ACCEPT_UNKNOWN)
				return 0;
			else {
				arpi->arp_ol_stats.peer_request_drop++;
				return ARP_REQ_SINK;
			}

		case DHC:
			savearptable(arpi, arp->src_ip, arp->src_eth);
			WL_INFORM(("wl%d: wl_arp_recv_proc: saved peer-ip/mac\n", WLCUNIT(arpi)));
			if (arpi->ol_cmpnts & ARP_OL_PEER_AUTO_REPLY)
				arp_reply_peer(arpi, arp, snap);
			/* Give the arp-request pkt to the host so that he updates his arp-table */
			return 0;

		case DAT:
			/*
			 * rfc2002, dont care about the dip, check only sip in arp-table.
			 * Discard ARP from known peer but not to our host
			 */
			arpi->arp_ol_stats.peer_request_drop++;
			return ARP_REQ_SINK;

		case SAT:
		case DAT + SAT:
			/*
			 * -Gratuitous arps go to host to update host's arpcache.
			 * -Arps with multihoming ON (ARP_ACCEPT_UNKNOWN) also go to host.
			 * -Arps where 2 peers are talking to eachother and multihoming OFF
			 * get dropped.
			 */
			if (peer_smac_changed(arpi, arp) == TRUE)
				return 0;

			if (bcmp(arp->src_ip, arp->dst_ip, IPV4_ADDR_LEN) &&
				!(arpi->ol_cmpnts & ARP_ACCEPT_UNKNOWN)) {
				arpi->arp_ol_stats.peer_request_drop++;
				return ARP_REQ_SINK;
			} else
				return 0;

		case SAT + DHC:
			if (peer_smac_changed(arpi, arp) == TRUE)
				return 0;	/* Give request to host to update his arp-table */

			/* don't check peer_dmac_changed() in arp-req-received as the
			 * dmac will be 0
			 */
			return ((arpi->ol_cmpnts & ARP_OL_PEER_AUTO_REPLY) ?
			        arp_reply_peer(arpi, arp, snap) : 0);

		default:
			WL_INFORM(("wl%d: wl_arp_recv_proc: conflicting arp-req case\n",
			           WLCUNIT(arpi)));
			return 0;
		}

	}
	else if (arp->oper == HTON16(ARP_OPC_REPLY)) {

		WL_INFORM(("wl%d: wl_arp_recv_proc: arp-rx-reply\n", WLCUNIT(arpi)));
		arpi->arp_ol_stats.peer_reply++;

		switch (sat_dat_shc_dhc) {


		case 0:
			/*
			 * Drop arp-reply between 2 peers, linklocal reply is broadcast.
			 *
			 * Caution : If, for linklocal host,
			 * -'wl arp_*_clear' are used after the connection was established and
			 * - host's arp-cache is cleared and
			 * - snooping is OFF,
			 * connection will not go through.  The arp-reply from peer will be dropped.
			 */

			if (ISLINKLOCAL_IPADDR(ipaddr)) {
				if (arpi->ol_cmpnts & ARP_ACCEPT_UNKNOWN)
					return 0;
				else {
					arpi->arp_ol_stats.peer_reply_drop++;
					return ARP_REQ_SINK;
				}
			} else
				return 0;


		case DHC:
			/*
			 * dip is present in host_ip table, not in arp-table.
			 * sip isn't in host_ip cache nor in arp-table.
			 *
			 * check for mac-id match. if it matches, update the arp-table
			 * with sip, else conflict.
			 */
			if (bcmp(arp->dst_eth, &arpi->host_mac, ETHER_ADDR_LEN) == 0)
				savearptable(arpi, arp->src_ip, arp->src_eth);

			return 0;

		case SAT:
			/* sip in arp-table.  link-local reply; conflict if mac-id is not same. */
			/* fall through */
		case SAT + DHC:
			/* sip in arp-table, dip in host_ip[] table */
			if (peer_smac_changed(arpi, arp) == TRUE)
				return 0;
			/* entry is already in arp-table; refresh age */
			savearptable(arpi, arp->src_ip, arp->src_eth);
			return 0;

		case SAT + DAT:
			/* 2 link-local peers talking to each other; arp-reply not for us */
			if (peer_smac_changed(arpi, arp) == TRUE)
				return 0;

			arpi->arp_ol_stats.peer_reply_drop++;
			return ARP_REQ_SINK;

		case DAT + SHC:
			/*
			 * sip in host-cache & dip in ARP table.
			 * this is our own arp reply to a peer in the same BSS
			 * and since promisc is enabled packet reached till here.
			 * no need to forward this packet to host.
			 */
			return ARP_REQ_SINK;

		default:
			WL_INFORM(("wl%d: wl_arp_recv_proc: conflicting arp-reply case\n",
			           WLCUNIT(arpi)));
			return 0;
		}
	} else {
		WL_ERROR(("wl%d: %s: unknown ARP oper 0x%x\n",
		          WLCUNIT(arpi), __FUNCTION__, ntoh16_ua((const void *)&arp->oper)));
		return -1;
	}

	return 0;
}

/* called when a new virtual IF is created.
 *	i/p: primary ARPIIF [arpi_p] and the new wlcif,
 *	o/p: new arpi structure populated with inputs and
 *		the global parameters duplicated from arpi_p
 *	side-effects: arpi for a new IF will inherit properties of arpi_p till
 *		the point new arpi is created. After that, for any change in
 *		arpi_p will NOT change the arpi corr to new IF. To change property
 *		of new IF, wl -i wl0.x has to be used.
*/
wl_arp_info_t *
wl_arp_alloc_ifarpi(wl_arp_info_t *arpi_p, wlc_if_t *wlcif)
{
	wl_arp_info_t *arpi;
	wlc_info_t *wlc = arpi_p->wlc;

	/* allocate arp private info struct */
	arpi = MALLOC(wlc->osh, sizeof(wl_arp_info_t));
	if (!arpi) {
		WL_ERROR(("wl%d: wl_arp_alloc_ifarpi: MALLOC failed; total mallocs %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	/* init arp private info struct */
	bzero(arpi, sizeof(wl_arp_info_t));
	arpi->wlc = wlc;
	arpi->wlcif = wlcif;

	/* copy global info from primary arpi structure */
	arpi->arp_max_age = arpi_p->arp_max_age;
	arpi->fake_arp_reply = arpi_p->fake_arp_reply;
	arpi->ol_cmpnts = arpi_p->ol_cmpnts;

	return arpi;
}

void
wl_arp_free_ifarpi(wl_arp_info_t *arpi)
{
	if (arpi != NULL)
		MFREE(WLCOSH(arpi), arpi, sizeof(wl_arp_info_t));
}
