/*
 * Broadcom 802.11 host offload module
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_offloads.c $
 */

#ifdef WLOFFLD

#include <wlc_cfg.h>
#include <typedefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <bcmdevs.h>
#include <pcicfg.h>
#include <pcie_core.h>
#include <siutils.h>
#include <bcmendian.h>
#include <nicpci.h>
#include <wlioctl.h>
#include <pcie_core.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_bmac.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wl_dbg.h>
#include <bcm_ol_msg.h>
#include <wlc_offloads.h>
#include <olbin_4360.h>

#define __BCM_IE_NOTIFICATION__

typedef struct wlc_arp_ol_info {
	struct ipv4_addr host_ip;
} wlc_arp_ol_info_t;

typedef struct wlc_nd_ol_info {
	struct ipv6_addr host_ipv6;
} wlc_nd_ol_info_t;

#ifdef  __BCM_IE_NOTIFICATION__
typedef struct wlc_ie_ol_info {
	uint8		iemask[CEIL((OLMSG_BCN_MAX_IE+1), 8)];
	vndriemask_info	vndriemask[MAX_VNDR_IE];
} wlc_ie_ol_info_t;
#endif

struct wlc_ol_info_t {
	wlc_info_t		*wlc;
	sbpcieregs_t		*pcieregs;
	osl_t			*osh;
	si_t			*sih;
	uint16			ol_flags;
	uint8			*msgbuf;
	uint32			msgbuf_sz;
	dmaaddr_t		msgbufpa;
	osldma_t		*dmah;
	uint 			alloced;
	uint32			mb_intstatus;
	olmsg_info		*msg_info;
	uint32 			console_addr;
	uint16			bcnmbsscount;
	uint16			rx_deferral_cnt;
	uint32			ramsize;
	uchar* 			dlarray;
	uint32 			dlarray_len;
	uchar*			bar1_addr;
	uint32			bar1_length;
	bool			ol_up;
	wlc_arp_ol_info_t	arp_info;
	wlc_nd_ol_info_t	nd_info;
#ifdef  __BCM_IE_NOTIFICATION__
	wlc_ie_ol_info_t	ie_info;
#endif
	bool			updn;
	bool			frame_del;
	bool			num_bsscfg_allow;
	uint32			disablemask;	/* Deferral disable mask */
};

#define VNDR_IE_ID              (221)

#ifdef  __BCM_IE_NOTIFICATION__
struct beacon_ie_notify_cmd {
	uint32		id;
	uint32		enable;
	struct ipv4_addr vndriemask;
};
#endif

enum {
	IOV_OL,
	IOV_ARP_HOSTIP,
	IOV_ND_HOSTIP,
	IOV_OL_DEFER_RXCNT,
	IOV_OL_FRAME_DEL,
#ifdef  __BCM_IE_NOTIFICATION__
	IOV_OL_IE_NOTIFICATION,
#endif
	IOV_OL_LAST
};

static const bcm_iovar_t ol_iovars[] = {
	{"offloads", IOV_OL, IOVF_SET_DOWN, IOVT_BOOL, 0},
	{"arp_hostip", IOV_ARP_HOSTIP,
	(0), IOVT_UINT32, 0
	},
	{"nd_hostip", IOV_ND_HOSTIP,
	(0), IOVT_BUFFER, IPV6_ADDR_LEN
	},
	{"ol_deferral_cnt", IOV_OL_DEFER_RXCNT, IOVF_SET_UP, IOVT_UINT16, 0},
	{"ol_bcn_del", IOV_OL_FRAME_DEL, IOVF_SET_DOWN, IOVT_BOOL, 0},
#ifdef  __BCM_IE_NOTIFICATION__
	{"ol_notify_bcn_ie", IOV_OL_IE_NOTIFICATION, (0), IOVT_BUFFER, 0},
#endif
	{NULL, 0, 0, 0, 0}
};

#ifdef  __BCM_IE_NOTIFICATION__
#define SET_ID(a, id)	(a[id/8] |= (1 << (id%8)))
#define GET_ID(a, id)	(a[id/8] & (1 << (id%8)))
#define RESET_ID(a, id)	(a[id/8] &= (~(1 << (id%8))))
#endif

#define ALIGN_BITS    2
/* Dongle Offload Features */
#define WL_OFFLOAD_ENAB_BCN		0x1
#define WL_OFFLOAD_ENAB_ARP		0x2
#define WL_OFFLOAD_ENAB_ND		0x4
#define WL_OFFLOAD_ENAB_IE_NOTIFICATION	0x8

#if defined(BCMDBG)
static int wlc_ol_dump(void *context, struct bcmstrbuf *b);
static int wlc_ol_cons_dump(void *context, struct bcmstrbuf *b);
#endif
static bool wlc_ol_msg_q_create(wlc_ol_info_t *ol);
static void wlc_ol_msg_q_init(wlc_ol_info_t *ol);
static void wlc_ol_armreset(wlc_ol_info_t *ol);
static int wlc_ol_go(wlc_ol_info_t * ol);
static void
wlc_ol_download_fw(wlc_ol_info_t *ol);
static void
wlc_ol_msg_send(wlc_ol_info_t *ol, void* msg, uint16 len);
static void
wlc_ol_int_arm(wlc_ol_info_t *ol);
static int
wlc_ol_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
static void wlc_ol_bsscfg_updn(void *ctx, bsscfg_up_down_event_data_t *evt);
/* Process messages sent by CR4 */
static int
wlc_ol_msg_receive(wlc_ol_info_t *ol);

static int wlc_ol_bcn_enable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg);

static int wlc_ol_nd_enable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg, struct ipv6_addr *host_ip);
static int wlc_ol_nd_setip(wlc_ol_info_t *ol, struct ipv6_addr *host_ip);
static int wlc_ol_nd_disable(wlc_ol_info_t *ol);

static int wlc_ol_arp_enable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg, struct ipv4_addr *host_ip);
static int wlc_ol_arp_setip(wlc_ol_info_t *ol, struct ipv4_addr *host_ip);
static int wlc_ol_arp_disable(wlc_ol_info_t *ol);
static void wlc_ol_send_resetmsg(wlc_ol_info_t *ol);

#ifdef __BCM_IE_NOTIFICATION__
static int wlc_notification_set_id(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg, int id, bool enable);
static int wlc_notification_set_flag(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg, bool enable);
#endif

/* The device is offload capable only if hardware allows it */
bool
wlc_ol_cap(wlc_info_t *wlc)
{
	uchar *bar1va = NULL;
	uint32 bar1_size = 0;

	if (wlc->ol == NULL) {
		if (D11REV_LT(wlc->pub->corerev, 40) || D11REV_IS(wlc->pub->corerev, 41) ||
			D11REV_IS(wlc->pub->corerev, 44)) {
			WL_ERROR(("%s: Offload support not present for core %d\n",
			__FUNCTION__, wlc->pub->corerev));
			return FALSE;
		}
		if (wlc->wl) {
			bar1_size = wl_pcie_bar1(wlc->wl, &bar1va);
			if (bar1va == NULL || bar1_size == 0) {
				WL_ERROR(("%s: Offload support disabled since PCI BAR1 not found\n",
				__FUNCTION__));
				return FALSE;
			}
		}
	}
	return TRUE;
}


/* module attach/detach */
wlc_ol_info_t *
BCMATTACHFN(wlc_ol_attach)(wlc_info_t *wlc)
{
	wlc_ol_info_t *ol;
	void *regsva = (void *) wlc->regs;

	/* sanity check */
	ASSERT(wlc != NULL);
	WL_TRACE(("%s: wlc_ol_attach %p \n", __FUNCTION__, regsva));

	/* module states */
	if ((ol = (wlc_ol_info_t *)MALLOC(wlc->osh, sizeof(wlc_ol_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		return NULL;
	}
	bzero(ol, sizeof(wlc_ol_info_t));

	ol->wlc = wlc;
	ol->osh = wlc->osh;
	ol->sih = wlc->pub->sih;
	ol->pcieregs = (sbpcieregs_t *)((uchar *)regsva + PCI_16KB0_PCIREGS_OFFSET);

	/* bsscfg up/down callback */
	if (wlc_bsscfg_updown_register(wlc, wlc_ol_bsscfg_updn, ol) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_updown_register() failed\n",
			wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	ol->updn = TRUE;
	/* register module up/down, watchdog, and iovar callbacks */
	if (wlc_module_register(wlc->pub, ol_iovars, "offloads", ol, wlc_ol_doiovar,
	                        NULL, NULL, wlc_ol_down)) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
	if ((ol->bar1_length = wl_pcie_bar1(wlc->wl, &ol->bar1_addr)) == 0) {
		WL_ERROR(("bar1 size is zero\n"));
		goto fail;
	}
	if (ol->bar1_addr == NULL) {
		WL_ERROR(("bar1 address is NULL\n"));
		goto fail;
	}
	ol->dlarray = dlarray;
	ol->dlarray_len = sizeof(dlarray);

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "offloads", wlc_ol_dump, ol);
	wlc_dump_register(wlc->pub, "offloads_cons", wlc_ol_cons_dump, ol);
#endif
	if (wlc_ol_msg_q_create(ol) == FALSE)
		goto fail;
	ol->frame_del = TRUE;

	/* Download the CR4 image as part of wl up process */
	return ol;

fail:
	/* error handling */
	wlc_ol_detach(ol);
	return NULL;
}

void
BCMATTACHFN(wlc_ol_detach)(wlc_ol_info_t *ol)
{
	wlc_info_t *wlc;

	ASSERT(ol != NULL);

	if (ol == NULL)
		return;

	wlc = ol->wlc;

	ASSERT(wlc != NULL);

	if (ol->msgbuf != NULL) {
		DMA_FREE_CONSISTENT(ol->osh, ol->msgbuf, ol->alloced, ol->msgbufpa, NULL);
		ol->msgbuf = NULL;
	}
	if (ol->msg_info != NULL) {
		MFREE(ol->osh, ol->msg_info, sizeof(olmsg_info));
	}
	wlc_module_unregister(wlc->pub, "offloads", ol);

	if (ol->updn == TRUE)
		wlc_bsscfg_updown_unregister(ol->wlc, wlc_ol_bsscfg_updn, ol);

	ol->updn = FALSE;
	MFREE(ol->osh, ol, sizeof(wlc_ol_info_t));
}

static bool wlc_ol_msg_q_create(wlc_ol_info_t *ol)
{
	ol->msgbuf_sz = OLMSG_BUF_SZ;

	if ((ol->msgbuf = (uint8 *)DMA_ALLOC_CONSISTENT(ol->osh,
		ol->msgbuf_sz, ALIGN_BITS, &ol->alloced,
		&ol->msgbufpa, &ol->dmah)) == NULL) {
		return FALSE;
	}
	ol->msg_info = MALLOC(ol->osh, sizeof(olmsg_info));
	if (ol->msg_info == NULL) {
		return FALSE;
	}
	bzero(ol->msg_info, sizeof(olmsg_info));

	/* Initialize the message buffer */
	bcm_olmsg_create(ol->msgbuf, ol->msgbuf_sz);
	bcm_olmsg_init(ol->msg_info, ol->msgbuf,
		ol->msgbuf_sz, OLMSG_READ_HOST_INDEX, OLMSG_WRITE_HOST_INDEX);
	return TRUE;
}

#if defined(BCMDBG)
static int
wlc_ol_dump(void *context, struct bcmstrbuf *b)
{
	olmsg_shared_info *shared_info;
	uchar *paddr;
	uint i, j, k;
	wlc_ol_info_t *ol = (wlc_ol_info_t *)context;
	wlc_info_t *wlc = ol->wlc;

	/* Return now if the chip is off */
	if (!wlc->clk)
		return BCME_NOCLK;

	if (!ol->ol_up || (!ol->ol_flags)) {
		WL_ERROR(("Offload not enabled\n"));
		return BCME_ERROR;
	}

	paddr = ol->bar1_addr;
	shared_info = (olmsg_shared_info *)(paddr +
		ol->ramsize - OLMSG_SHARED_INFO_SZ);

	bcm_bprintf(b,
	            "bcncount %u cleardefcnt %u\n"
	            "bssidmiss_cnt %u capmiss_cnt %u\n"
	            "bimiss_cnt %u bcnlosscnt %u\n",
	            ltoh32(shared_info->stats.rxoe_bcncount),
	            ltoh32(shared_info->stats.rxoe_cleardefcnt),
	            ltoh32(shared_info->stats.rxoe_bssidmiss_cnt),
	            ltoh32(shared_info->stats.rxoe_capmiss_cnt),
	            ltoh32(shared_info->stats.rxoe_bimiss_cnt),
	            ltoh32(shared_info->stats.rxoe_bcnlosscnt));

	for (i = 0; i < OLMSG_BCN_MAX_IE; i++) {
		if (shared_info->stats.iechanged[i]) {
			bcm_bprintf(b, "ie %u iechangedcnt %u\n",
			            i, ltoh16(shared_info->stats.iechanged[i]));
		}
	}

	if (WLOFFLD_ARP_ENAB(ol->wlc->pub)) {

		i = shared_info->stats.rxoe_arpcnt;
		bcm_bprintf(b, "\nARP STATS\n");
		bcm_bprintf(b, "src ip\t\tdest ip\t\toperation\tstatus\n");
		for (j = 0; j < MAX_STAT_ENTRIES; j++) {
			if (IPV4_ADDR_NULL(shared_info->stats.arp_stats[i].dest_ip.addr) &&
				IPV4_ADDR_NULL(shared_info->stats.arp_stats[i].src_ip.addr)) {
				i = NEXT_STAT(i);
				continue;
			}
			for (k = 0; k < IPV4_ADDR_LEN; k++)
				bcm_bprintf(b, "%d.",
				shared_info->stats.arp_stats[i].src_ip.addr[k]);
				bcm_bprintf(b, "\t");
			for (k = 0; k < IPV4_ADDR_LEN; k++)
				bcm_bprintf(b, "%d.",
				shared_info->stats.arp_stats[i].dest_ip.addr[k]);

				bcm_bprintf(b, "\t%s\t\t%s\n",
				shared_info->stats.arp_stats[i].is_request ? "REQUEST" : "REPLY",
				shared_info->stats.arp_stats[i].suppressed ?
					"suppressed" : "not suppressed");
				i = NEXT_STAT(i);
		}

	}
	if (WLOFFLD_ND_ENAB(ol->wlc->pub)) {

		i = shared_info->stats.rxoe_ndcnt;
		bcm_bprintf(b, "\nND STATS\n");
		bcm_bprintf(b, "dest ip \t\t\t operation \t status\n");
		for (j = 0; j < MAX_STAT_ENTRIES; j++) {
			if (IPV6_ADDR_NULL(shared_info->stats.nd_stats[i].dest_ip.addr)) {
				i = NEXT_STAT(i);
				continue;
			}
			for (k = 0; k < MAX_STAT_ENTRIES; k++) {
				bcm_bprintf(b, "%x",
					shared_info->stats.nd_stats[i].dest_ip.addr[k]);
				if (k%2)
					bcm_bprintf(b, ":");
			}
			bcm_bprintf(b, "\t%s\t%s\n",
				shared_info->stats.nd_stats[i].is_request ? "NS" : "NA",
				shared_info->stats.nd_stats[i].suppressed ?
					"suppressed" : "not suppressed");
				i = NEXT_STAT(i);
		}

	}
	return BCME_OK;
}

/*
 * Dump on-chip console buffer
 */
static int
wlc_ol_cons_dump(void *context, struct bcmstrbuf *b)
{
	olmsg_shared_info *shared_info;
	uchar *paddr;
	uint32 cons_addr;
	char *local_buf;
	uint32 *cons;
	char *buf;
	uint32	buf_size;
	uint32	buf_idx;
	int err = BCME_OK;
	uint32 i;
	wlc_ol_info_t *ol = (wlc_ol_info_t *)context;
	wlc_info_t *wlc = ol->wlc;

	/* Return now if the chip is off */
	if (!wlc->clk)
		return BCME_NOCLK;

	if (!ol->ol_up || (!ol->ol_flags)) {
		WL_ERROR(("Offload not enabled\n"));
		return BCME_ERROR;
	}

	paddr = ol->bar1_addr;
	shared_info = (olmsg_shared_info *)(paddr +
		ol->ramsize - OLMSG_SHARED_INFO_SZ);

	cons_addr = ltoh32(shared_info->console_addr);
	cons = (uint32 *)(ol->bar1_addr + cons_addr);

	/* read the hndrte_log info from device memory */
	buf = (char *)(ol->bar1_addr + ltoh32(cons[0]));
	buf_size = ltoh32(cons[1]);
	buf_idx = ltoh32(cons[2]);

	/* allocate a local buffer and copy the log contents */
	local_buf = (char*)MALLOC(ol->osh, buf_size + 1);
	if (local_buf == NULL) {
		return BCME_NOMEM;
	}

	/* unwrap the circular buffer as we copy */

	/* copy from start index to the end first */
	bcopy(buf + buf_idx, local_buf, buf_size - buf_idx);

	/* then copy remainder from the begining of the buffer up to the startindex */
	if (buf_idx > 0) {
		bcopy(buf, local_buf + (buf_size - buf_idx), buf_idx);
	}

	/* skip leading nulls */
	for (i = 0; i < buf_size; i++) {
		if (local_buf[i] != '\0')
			break;
	}

	/* null terminate the entire local buffer and print */
	local_buf[buf_size] = '\0';
	bcm_bprintf(b, "%s", &local_buf[i]);

	MFREE(ol->osh, local_buf, buf_size + 1);

	return err;
}
#endif 

void
wlc_ol_enable_intrs(wlc_ol_info_t *ol, bool enable)
{
	uint retval = 0xFFFFFFFF;
	sbpcieregs_t *pcieregs = ol->pcieregs;

	if (!ol->ol_up)
		return;

	/*
	 * intmask : Bits 8 & 9 to enable PCIe to  SB interrupts
	 * mailboxintmsk: Bits 8 & 9 to enable SB to PCIe interrupts
	 */
	/* Enable PCIe to  SB Interrupts */
	if (enable)
		retval |= PCIE_MB_TOPCIE_FN0_0;
	else
		retval &= ~PCIE_MB_TOPCIE_FN0_0;

	W_REG(ol->osh, &pcieregs->intmask, retval);

}
static int wlc_ol_go(wlc_ol_info_t *ol)
{
	int ret = BCME_OK;
	uint origidx;

	/* Remember the original index */
	origidx = si_coreidx(ol->sih);
	wlc_ol_enable_intrs(ol, FALSE);
	ol->ramsize = si_tcm_size(ol->sih);
	wlc_ol_armreset(ol);
	wlc_ol_download_fw(ol);
	/* Messaging initialization */
	/* Update msgbuffer registers with shared buffer address */
	wlc_ol_msg_q_init(ol);

	/* Run ARM  */
	si_core_reset(ol->sih, 0, 0);

	/* Enable the interrupts */
	wlc_ol_enable_intrs(ol, TRUE);

	/* Restore original core index */
	si_setcoreidx(ol->sih, origidx);

	return ret;
}
static void
wlc_ol_download_fw(wlc_ol_info_t *ol)
{
	unsigned int index = 0;
	for (index = 0; index < ol->bar1_length; index++) {
		W_REG(ol->osh, &ol->bar1_addr[index], 0);
	}

	/* Write firmware image */
	for (index = 0; index < ol->dlarray_len; index++) {
		W_REG(ol->osh, &ol->bar1_addr[index], ol->dlarray[index]);
	}

}

static void wlc_ol_msg_q_init(wlc_ol_info_t *ol)
{
	olmsg_shared_info *shared_info;

	shared_info = (olmsg_shared_info *)(ol->bar1_addr +
		ol->ramsize - OLMSG_SHARED_INFO_SZ);
	W_REG(ol->osh, &shared_info->msgbufaddr_low, PHYSADDRLO(ol->msgbufpa));
	W_REG(ol->osh, &shared_info->msgbufaddr_high, PHYSADDRHI(ol->msgbufpa));
	W_REG(ol->osh, &shared_info->msgbuf_sz, OLMSG_BUF_SZ);
}
int wlc_ol_down(void *hdl)
{
	wlc_ol_info_t *ol = (wlc_ol_info_t *)hdl;
	uint origidx;

	if (!ol->ol_up)
	    return BCME_OK;

	origidx = si_coreidx(ol->sih);
	wlc_ol_enable_intrs(ol, FALSE);
	wlc_ol_armreset(ol);

	/* Restore original core index */
	si_setcoreidx(ol->sih, origidx);

	bcm_olmsg_deinit(ol->msg_info);
	ol->ol_flags = 0;
	ol->ol_up = FALSE;
	return BCME_OK;
}

int wlc_ol_up(void *hdl)
{
	uint32 macintmask;
	int err = BCME_OK;
	wlc_ol_info_t *ol = (wlc_ol_info_t *)hdl;
	wlc_info_t *wlc = ol->wlc;
	d11regs_t *regs;
	regs = wlc->regs;

	if (ol->ol_up)
	   return err;

	bcm_olmsg_create(ol->msgbuf, ol->msgbuf_sz);
	bcm_olmsg_init(ol->msg_info, ol->msgbuf,
		ol->msgbuf_sz, OLMSG_READ_HOST_INDEX, OLMSG_WRITE_HOST_INDEX);
	macintmask = wl_intrsoff(wlc->wl);
	W_REG(ol->osh, &regs->intrcvlazy[1], (1 << IRL_FC_SHIFT));
	if (wlc_ol_go(ol) != BCME_OK) {
		WL_ERROR(("%s: Error in running ol firmware \n", __FUNCTION__));
		err = BCME_ERROR;
	}
	ol->ol_up = TRUE;
	ol->rx_deferral_cnt = wlc_read_shm(wlc, M_DEFER_RXCNT);

	/* restore macintmask */
	wl_intrsrestore(wlc->wl, macintmask);

	return err;
}

static void wlc_ol_send_resetmsg(wlc_ol_info_t *ol)
{
	olmsg_reset	ol_reset;

	if (!ol->ol_flags)
		return;

	wlc_ol_rx_deferral(ol, OL_CFG_MASK, OL_CFG_MASK);
	ol_reset.hdr.type = BCM_OL_RESET;
	ol_reset.hdr.seq = 0;
	ol_reset.hdr.len = sizeof(olmsg_reset) - sizeof(olmsg_header);
	wlc_ol_msg_send(ol, (uint8 *)&ol_reset, sizeof(olmsg_reset));
	ol->ol_flags = 0;

}
void wlc_ol_clear(wlc_ol_info_t *ol)
{
	if (!ol->ol_up)
	   return;
	wlc_ol_send_resetmsg(ol);
	wlc_ol_enable_intrs(ol, FALSE);
	ol->ol_up = FALSE;
}

static void
wlc_ol_bsscfg_updn(void *ctx, bsscfg_up_down_event_data_t *evt)
{
	wlc_ol_info_t *ol = (wlc_ol_info_t *)ctx;
	wlc_info_t *wlc = ol->wlc;
	wlc_bsscfg_t   *cfg;
	int idx, ap = 0;
	uint16 total = 0;
	bool enable = FALSE;

	ASSERT(ctx != NULL);
	ASSERT(evt != NULL);

	if (!WLOFFLD_ENAB(ol->wlc->pub))
	    return;

	FOREACH_BSS(wlc, idx, cfg) {
		if (cfg->up) {
			total++;
			if (BSSCFG_AP(cfg))
				ap++;
		}
	}

	if ((evt->bsscfg->up) && (evt->up == FALSE)) {
		/* This BSS is coming down */
		total--;
		if (BSSCFG_AP(evt->bsscfg))
			ap--;
	}

	cfg = wlc->cfg;

	if ((total == 1) &&
		(ap == 0) &&
		(cfg != NULL) &&
		(cfg->up) &&
		(cfg->BSS) &&
		(!BSSCFG_AP(cfg))) {
		enable = TRUE;
	}

	ol->num_bsscfg_allow = enable;

	if (enable == FALSE) {
	 wlc_ol_send_resetmsg(ol);
	} else	{
		wlc_ol_restart(wlc->ol);
	}
}

static void wlc_ol_armreset(wlc_ol_info_t *ol)
{
	/* Reset ARM
	* flags: SICF_CPUHALT: 0x0020
	*/
	si_setcore(ol->sih, ARMCR4_CORE_ID, 0);
	si_core_reset(ol->sih, SICF_CPUHALT, 0);
}

void wlc_ol_restart(wlc_ol_info_t *ol)
{
	wlc_info_t *wlc = ol->wlc;
	wlc_bsscfg_t *cfg = wlc->cfg;

	if (cfg != NULL) {
		if (cfg->up && cfg->associated && !ETHER_ISNULLADDR(&(cfg)->BSSID))
			wlc_ol_enable(ol, cfg);
	}
}

/*
 * Checks whether interrupt is from CR4
 */
bool
wlc_ol_intstatus(wlc_ol_info_t *ol)
{
	sbpcieregs_t *pcieregs;
	uint retval = 0xFFFFFFFF;

	if (!ol->ol_up)
		return FALSE;

	pcieregs = ol->pcieregs;
	retval = R_REG(ol->osh, &pcieregs->mailboxint);

	if (retval & PCIE_INT_MB_FN0_0) {
		ol->mb_intstatus = retval;
		W_REG(ol->osh, &pcieregs->mailboxint, PCIE_INT_MB_FN0_0);
		/* Disable interrupt also */
		wlc_ol_enable_intrs(ol, FALSE);
		return TRUE;
	}

	return FALSE;
}
static void
wlc_ol_msg_send(wlc_ol_info_t *ol, void* msg, uint16 len)
{
	/* write the message into the message ring */
	bcm_olmsg_writemsg(ol->msg_info, msg, len);

	/* interrupt to signal new message to on chip processor */
	wlc_ol_int_arm(ol);
}


/* Process messages posted by CR4 */
static int
wlc_ol_msg_receive(wlc_ol_info_t *ol)
{
	uint8 buf[256];
	olmsg_header *hdr;

	while (bcm_olmsg_readmsg(ol->msg_info, &buf[0], sizeof(buf))) {
		hdr = (olmsg_header *)&buf[0];
		switch (hdr->type) {
			case BCM_OL_MSG_TEST:
				WL_ERROR(("%s: BCM_OL_TEST_MSG message \n", __FUNCTION__));
				break;
			default:
				WL_ERROR(("%s: Unhandled message \n", __FUNCTION__));
				bcm_olmsg_dump_msg(ol->msg_info, (olmsg_header *)&buf[0]);
		}
	}
	return BCME_OK;
}

/*
 * DPC: Process mailbox interrupts. Read message written by dongle
 */
void
wlc_ol_dpc(wlc_ol_info_t *ol)
{
	if (ol && ol->mb_intstatus & PCIE_INT_MB_FN0_0) {
		wlc_ol_msg_receive(ol);
		ol->mb_intstatus = 0;
	}
}

/*
 * Generate Mailbox interrupt PCIe -> SB
 */
static void
wlc_ol_int_arm(wlc_ol_info_t *ol)
{
	sbpcieregs_t *pcieregs = NULL;

	if (ol) {
		pcieregs = ol->pcieregs;
		OR_REG(ol->osh, &pcieregs->mailboxint, PCIE_MB_TOSB_FN0_0);
	}
}

static int
wlc_ol_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_ol_info_t *ol = (wlc_ol_info_t *)context;
	wlc_info_t *wlc = ol->wlc;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	wlc_bsscfg_t *cfg  = wlc->cfg;
	int err = BCME_OK;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	ret_int_ptr = (int32 *)a;

	switch (actionid) {
		case IOV_GVAL(IOV_OL):
			*ret_int_ptr = wlc->pub->_ol;
			break;
		case IOV_SVAL(IOV_OL):
			wlc->pub->_ol = int_val;
			break;
		case IOV_SVAL(IOV_ARP_HOSTIP):
			if (plen < sizeof(struct ipv4_addr))
				err = BCME_BUFTOOSHORT;
			else
			{
				if (WLOFFLD_ARP_ENAB(wlc->pub) &&
					!(WSEC_TKIP_ENABLED(wlc->cfg->wsec)))
				{
					bcopy(a, ol->arp_info.host_ip.addr, IPV4_ADDR_LEN);
					if (!wlc->cfg->associated) {
						err = BCME_NOTASSOCIATED;
						return err;
					}
					if (ol->ol_flags & WL_OFFLOAD_ENAB_ARP)
						err = wlc_ol_arp_setip(ol,
							&(ol->arp_info.host_ip));
					else
						err = wlc_ol_arp_enable(ol,
							wlc->cfg, &(ol->arp_info.host_ip));
				}
				else
					err = BCME_UNSUPPORTED;
			}
			break;
		case IOV_SVAL(IOV_ND_HOSTIP):
				if (plen < sizeof(struct ipv6_addr))
					err = BCME_BUFTOOSHORT;
			else
			{
				if (WLOFFLD_ND_ENAB(wlc->pub) &&
					!(WSEC_TKIP_ENABLED(wlc->cfg->wsec)))
				 {
					bcopy(a, ol->nd_info.host_ipv6.addr, IPV6_ADDR_LEN);
					if (!wlc->cfg->associated) {
						err = BCME_NOTASSOCIATED;
						return err;
					}
					if (ol->ol_flags & WL_OFFLOAD_ENAB_ND)
						err = wlc_ol_nd_setip(ol,
							&(ol->nd_info.host_ipv6));
					else
						err = wlc_ol_nd_enable(ol,
							wlc->cfg, &(ol->nd_info.host_ipv6));
				}
				else
					err = BCME_UNSUPPORTED;
			}
			break;
		case IOV_GVAL(IOV_OL_DEFER_RXCNT):
			*ret_int_ptr = (int32)ol->rx_deferral_cnt;
			break;
		case IOV_SVAL(IOV_OL_DEFER_RXCNT):
			ol->rx_deferral_cnt = (uint16)int_val;
			wlc_write_shm(wlc, M_DEFER_RXCNT, ol->rx_deferral_cnt);
			break;
		case IOV_GVAL(IOV_OL_FRAME_DEL):
			*ret_int_ptr = ol->frame_del;
			break;
		case IOV_SVAL(IOV_OL_FRAME_DEL):
			ol->frame_del = (int_val != 0);
			break;

#ifdef  __BCM_IE_NOTIFICATION__

		case IOV_GVAL(IOV_OL_IE_NOTIFICATION): {
				struct	beacon_ie_notify_cmd *param =
					(struct beacon_ie_notify_cmd *)p;
				char	*ret_ptr = (char *)a;
				uint32	id;
				/* struct	ipv4_addr vndriemask; */

				if (plen < (int)sizeof(struct beacon_ie_notify_cmd)) {
					WL_ERROR(("%s: Parameter Error\n", __FUNCTION__));
					err = BCME_UNSUPPORTED;
				}
				id = param->id;

				WL_ERROR(("%s: GET IOVar for ID: %d\n", __FUNCTION__, id));

				if (id == -1) {
					int i;
					snprintf(ret_ptr, 40,
						"IE Notification Flag: %s\n",
						(ol->ol_flags & WL_OFFLOAD_ENAB_IE_NOTIFICATION) ?
						"enable" : "disable");
					strncat(ret_ptr, "List of enabled IE: ", 20);
					for (i = 0; i < OLMSG_BCN_MAX_IE; i++) {
						if (GET_ID(ol->ie_info.iemask, i)) {
							char t[8];
							snprintf(t, 8, "%d ", i);
							strncat(ret_ptr, t, 8);
						}
					}
					strncat(ret_ptr, "\n", 4);
				} else if (id != VNDR_IE_ID && id < OLMSG_BCN_MAX_IE) {
					snprintf(ret_ptr, 40,
						"IE Notification Flag for ID %d : %s\n",
						id,
						GET_ID(ol->ie_info.iemask, id) ?
						"enable" : "disable");
				} else if (id == VNDR_IE_ID) {
					snprintf(ret_ptr, 40,
						"Vendor IE notification is not implemented!\n");
				} else {
					err = BCME_UNSUPPORTED;
				}
			}
			break;

		case IOV_SVAL(IOV_OL_IE_NOTIFICATION): {
				struct beacon_ie_notify_cmd *param =
					(struct beacon_ie_notify_cmd *)p;
				uint32		id;
				uint32		enable;
				/* struct ipv4_addr	vndriemask; */

				if (plen < (int)sizeof(struct beacon_ie_notify_cmd)) {
					WL_ERROR(("%s: Parameter Error\n", __FUNCTION__));
					err = BCME_UNSUPPORTED;
				}

				id = param->id;
				enable = param->enable;

				WL_ERROR(("%s: SET IOVar for ID: %d to %s\n", __FUNCTION__, id,
					(enable ? "enable" : "disable")));

				if (WLOFFLD_IE_NOTIFICATION_ENAB(wlc->pub)) {
					if (id == -1) {
						WL_ERROR(("%s: Setting IE notification: %s\n",
							__FUNCTION__,
							(enable ? "enable" : "disable")));
						err = wlc_notification_set_flag(ol, cfg,
							(bool)enable);
					} else if (id != VNDR_IE_ID && id < OLMSG_BCN_MAX_IE) {
						if (ol->ol_flags &
							WL_OFFLOAD_ENAB_IE_NOTIFICATION) {
							WL_ERROR(("%s: Setting IE id: %d to %s\n",
								__FUNCTION__, id,
								(enable ? "enable" : "disable")));
							err = wlc_notification_set_id(ol, cfg, id,
							(bool)enable);
						}
					} else if (id == VNDR_IE_ID) {
						err = BCME_UNSUPPORTED;
					} else {
						err = BCME_UNSUPPORTED;
					}
				} else {
					err = BCME_UNSUPPORTED;
				}
			}
			break;
#endif /* #ifdef  __BCM_IE_NOTIFICATION__ */
		default:
			err = BCME_UNSUPPORTED;
			break;
	}

	return err;
}

void wlc_ol_rx_deferral(wlc_ol_info_t *ol, uint32 mask, uint32 val)
{
	int flags = 0;

	ol->disablemask = (ol->disablemask & ~mask) | (val & mask);

	if (ol->disablemask == 0) { /* Enable */
		if (ol->ol_flags & WL_OFFLOAD_ENAB_BCN) {
			wlc_suspend_mac_and_wait(ol->wlc);
			flags = wlc_read_shm(ol->wlc, M_PSO_ENBL_FLAGS);
			if (!(flags & WL_OFFLOAD_ENAB_BCN))
				wlc_write_shm(ol->wlc,
					M_PSO_ENBL_FLAGS,
					flags | WL_OFFLOAD_ENAB_BCN);
			wlc_enable_mac(ol->wlc);
		}
		if ((ol->ol_flags & WL_OFFLOAD_ENAB_ARP) || (ol->ol_flags & WL_OFFLOAD_ENAB_ND)) {
			wlc_suspend_mac_and_wait(ol->wlc);
			flags = wlc_read_shm(ol->wlc, M_PSO_ENBL_FLAGS);
			if (!(flags & WL_OFFLOAD_ENAB_ARP))
				wlc_write_shm(ol->wlc,
					M_PSO_ENBL_FLAGS,
					flags | WL_OFFLOAD_ENAB_ARP);
			wlc_enable_mac(ol->wlc);
		}
	} else { /* Disable */

			if ((ol->ol_flags & WL_OFFLOAD_ENAB_BCN) ||
				(ol->ol_flags & WL_OFFLOAD_ENAB_ARP) ||
				(ol->ol_flags & WL_OFFLOAD_ENAB_ND))
				wlc_write_shm(ol->wlc, M_PSO_ENBL_FLAGS, 0);
	}

}
/* Enable beacon offload */
static int
wlc_ol_bcn_enable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg)
{
	int err = BCME_OK;
	olmsg_bcn_enable *p_bcn_enb;
	wlc_info_t *wlc = NULL;
	uint16 ie_len;
	wlc_bss_info_t *current_bss = NULL;

	/* Check for valid state to enable beacon offloads */
	if (!ol || !cfg || !BSSCFG_STA(cfg) || !cfg->BSS || !cfg->up ||
		cfg != ol->wlc->cfg || !cfg->associated || !ol->num_bsscfg_allow ||
		(ol->ol_flags & WL_OFFLOAD_ENAB_BCN) || !(ol->ol_up)) {
		WL_ERROR(("%s: Inavlid params/state. Not enabling beacon offloads \n",
			__FUNCTION__));
		return BCME_BADARG;
	}

	wlc = ol->wlc;

	/* Enable beacon offloads */
	ol->ol_flags |= WL_OFFLOAD_ENAB_BCN;

	wlc_ol_rx_deferral(ol, OL_CFG_MASK, 0);

	current_bss = cfg->current_bss;
	ie_len = current_bss->bcn_prb_len - DOT11_BCN_PRB_FIXED_LEN;

	p_bcn_enb = (olmsg_bcn_enable *)MALLOC(ol->osh, sizeof(olmsg_bcn_enable) + ie_len);
	bzero(p_bcn_enb, sizeof(olmsg_bcn_enable) + ie_len);
	/* Fill up offload beacon enable message */
	p_bcn_enb->hdr.type = BCM_OL_BEACON_ENABLE;
	p_bcn_enb->hdr.len = sizeof(olmsg_bcn_enable) - sizeof(p_bcn_enb->hdr) + ie_len;
	p_bcn_enb->defcnt = wlc_read_shm(wlc, M_DEFER_RXCNT);
	p_bcn_enb->bcn_length = current_bss->bcn_prb_len;
	bcopy(&cfg->BSSID, &p_bcn_enb->BSSID, sizeof(struct ether_addr));
	bcopy(&cfg->cur_etheraddr,  &p_bcn_enb->cur_etheraddr, sizeof(struct ether_addr));
	p_bcn_enb->bi =  wlc->cfg->current_bss->beacon_period;
	p_bcn_enb->capability = current_bss->bcn_prb->capability;
	p_bcn_enb->rxchannel = CHSPEC_CHANNEL(current_bss->chanspec);
	p_bcn_enb->aid = cfg->AID;
	p_bcn_enb->frame_del = ol->frame_del;


	p_bcn_enb->vndriemask[0].oui.b.id[0] = 0x00;
	p_bcn_enb->vndriemask[0].oui.b.id[1] = 0x50;
	p_bcn_enb->vndriemask[0].oui.b.id[2] = 0xf2;
	p_bcn_enb->vndriemask[0].oui.b.type = 0x2;
	/* Copy IEs */
	p_bcn_enb->iedatalen = ie_len;
	bcopy(((uint8 *)current_bss->bcn_prb)+ DOT11_BCN_PRB_LEN, &p_bcn_enb->iedata[0], ie_len);

	/* Send beacon offload enable message to CR4 */
	wlc_ol_msg_send(ol, (uint8 *)p_bcn_enb, sizeof(olmsg_bcn_enable) + ie_len);

	MFREE(ol->osh, p_bcn_enb, sizeof(olmsg_bcn_enable) + ie_len);
	return err;
}

static int
wlc_ol_bcn_disable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg)
{
	olmsg_bcn_disable bcn_disable;
	int err = BCME_OK;

	if ((cfg != NULL) && (ol->ol_flags & WL_OFFLOAD_ENAB_BCN)) {
		bcn_disable.hdr.type = BCM_OL_BEACON_DISABLE;
		bcn_disable.hdr.seq = 0;
		bcn_disable.hdr.len = sizeof(olmsg_bcn_disable) - sizeof(olmsg_header);
		bcopy(&cfg->BSSID, &bcn_disable.BSSID, sizeof(struct ether_addr));

		wlc_ol_msg_send(ol, (uint8 *)&bcn_disable, sizeof(olmsg_bcn_disable));

		wlc_ol_rx_deferral(ol, OL_CFG_MASK, OL_CFG_MASK);
		ol->ol_flags &= ~WL_OFFLOAD_ENAB_BCN;
	}
	else {
		WL_TRACE(("%s: Beacon offload is not active\n", __FUNCTION__));
		err = BCME_ERROR;
	}
	return err;
}

bool wlc_ol_time_since_bcn(wlc_ol_info_t *ol)
{

	wlc_info_t *wlc;
	uint16 bcncnt;
	bool ret = FALSE;
	wlc = ol->wlc;
	if (!(ol->ol_flags & WL_OFFLOAD_ENAB_BCN))
		return FALSE;
	bcncnt = wlc_bmac_read_shm(wlc->hw, M_UCODE_BSSBCNCNT);

	if (ol->bcnmbsscount != bcncnt)
		ret = TRUE;
	else
		ret = FALSE;

	ol->bcnmbsscount = bcncnt;

	return ret;
}

static int8
wlc_ol_get_iv_len(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg)
{
	if (WSEC_WEP_ENABLED(cfg->wsec))
		return DOT11_IV_LEN;
	else if (WSEC_TKIP_ENABLED(cfg->wsec))
		return DOT11_IV_TKIP_LEN;
	else if (WSEC_AES_ENABLED(cfg->wsec))
		return DOT11_IV_AES_CCM_LEN;
	else
		return 0;
}
int
wlc_ol_arp_enable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg, struct ipv4_addr *host_ip)
{
	int err = BCME_OK;
	olmsg_arp_enable arp_enable;

	/* Check for valid state to enable arp offloads */
	if (!ol || !cfg || !BSSCFG_STA(cfg) || !cfg->BSS || !cfg->up ||
		cfg != ol->wlc->cfg || !cfg->associated || !ol->num_bsscfg_allow ||
		IPV4_ADDR_NULL(host_ip->addr) ||
		(ol->ol_flags & WL_OFFLOAD_ENAB_ARP) || !(ol->ol_up)) {
		WL_ERROR(("%s: Inavlid params/state. Not enabling ARP offloads \n",
			__FUNCTION__));
		return BCME_BADARG;
	}

	ol->ol_flags |= WL_OFFLOAD_ENAB_ARP;

	arp_enable.hdr.type = BCM_OL_ARP_ENABLE;
	arp_enable.hdr.seq = 0;
	arp_enable.hdr.len = sizeof(olmsg_arp_enable) - sizeof(olmsg_header);
	bcopy(&cfg->cur_etheraddr, &arp_enable.host_mac, sizeof(struct ether_addr));
	bcopy(host_ip->addr, arp_enable.host_ip.addr, IPV4_ADDR_LEN);
	bcopy(host_ip->addr, ol->arp_info.host_ip.addr, IPV4_ADDR_LEN);
	arp_enable.iv_len = wlc_ol_get_iv_len(ol, cfg);

	wlc_ol_msg_send(ol, (uint8 *)&arp_enable, sizeof(olmsg_arp_enable));
	wlc_ol_rx_deferral(ol, OL_CFG_MASK, 0);

	return err;
}


static int
wlc_ol_arp_disable(wlc_ol_info_t *ol)
{
	int err = BCME_OK;
	olmsg_arp_disable arp_disable;

	if (ol->ol_flags & WL_OFFLOAD_ENAB_ARP) {
		arp_disable.hdr.type = BCM_OL_ARP_DISABLE;
		arp_disable.hdr.seq = 0;
		arp_disable.hdr.len = sizeof(olmsg_arp_disable) - sizeof(olmsg_header);

		wlc_ol_msg_send(ol, (uint8 *)&arp_disable, sizeof(olmsg_arp_disable));

		wlc_ol_rx_deferral(ol, OL_CFG_MASK, OL_CFG_MASK);
		ol->ol_flags &= ~WL_OFFLOAD_ENAB_ARP;

	} else {
		WL_TRACE(("%s: ARP offload is not active\n", __FUNCTION__));
		err = BCME_ERROR;
	}

	return err;
}

static int
wlc_ol_arp_setip(wlc_ol_info_t *ol, struct ipv4_addr *host_ip)
{
	int err = BCME_OK;
	olmsg_arp_setip arp_setip;

	if (ol->ol_flags & WL_OFFLOAD_ENAB_ARP) {
		arp_setip.hdr.type = BCM_OL_ARP_SETIP;
		arp_setip.hdr.seq = 0;
		arp_setip.hdr.len = sizeof(olmsg_arp_setip) - sizeof(olmsg_header);
		bcopy(host_ip, &(arp_setip.host_ip), sizeof(struct ipv4_addr));

		wlc_ol_msg_send(ol, (uint8 *)&arp_setip, sizeof(olmsg_arp_setip));
	}
	else {
		WL_TRACE(("%s: ARP offload is not active\n", __FUNCTION__));
		ASSERT(FALSE);
		err = BCME_ERROR;
	}

	return err;
}

int
wlc_ol_nd_enable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg, struct ipv6_addr *host_ip)
{
	int err = BCME_OK;
	olmsg_nd_enable nd_enable;

	/* Check for valid state to enable arp offloads */
	if (!ol || !cfg || !BSSCFG_STA(cfg) || !cfg->BSS || !cfg->up ||
		cfg != ol->wlc->cfg || !cfg->associated || !ol->num_bsscfg_allow ||
		IPV6_ADDR_NULL(host_ip->addr) ||
		(ol->ol_flags & WL_OFFLOAD_ENAB_ND) || !(ol->ol_up)) {
		WL_ERROR(("%s: Inavlid params/state. Not enabling ND offloads \n",
			__FUNCTION__));
		return BCME_BADARG;
	}

	ol->ol_flags |= WL_OFFLOAD_ENAB_ND;

	nd_enable.hdr.type = BCM_OL_ND_ENABLE;
	nd_enable.hdr.seq = 0;
	nd_enable.hdr.len = sizeof(olmsg_nd_enable) - sizeof(olmsg_header);
	bcopy(&cfg->cur_etheraddr, &nd_enable.host_mac, sizeof(struct ether_addr));
	bcopy(host_ip->addr, nd_enable.host_ip.addr, IPV6_ADDR_LEN);
	bcopy(host_ip->addr, ol->nd_info.host_ipv6.addr, IPV6_ADDR_LEN);
	nd_enable.iv_len = wlc_ol_get_iv_len(ol, cfg);

	wlc_ol_msg_send(ol, (uint8 *)&nd_enable, sizeof(olmsg_nd_enable));
	wlc_ol_rx_deferral(ol, OL_CFG_MASK, 0);

	return err;
}


static int
wlc_ol_nd_disable(wlc_ol_info_t *ol)
{
	int err = BCME_OK;
	olmsg_nd_disable nd_disable;

	if (ol->ol_flags & WL_OFFLOAD_ENAB_ND) {
		nd_disable.hdr.type = BCM_OL_ND_DISABLE;
		nd_disable.hdr.seq = 0;
		nd_disable.hdr.len = sizeof(olmsg_nd_disable) - sizeof(olmsg_header);

		wlc_ol_msg_send(ol, (uint8 *)&nd_disable, sizeof(olmsg_nd_disable));

		wlc_ol_rx_deferral(ol, OL_CFG_MASK, OL_CFG_MASK);
		ol->ol_flags &= ~WL_OFFLOAD_ENAB_ND;
	} else {
		WL_TRACE(("%s: ND offload is not active\n", __FUNCTION__));
		err = BCME_ERROR;
	}

	return err;
}

static int
wlc_ol_nd_setip(wlc_ol_info_t *ol, struct ipv6_addr *host_ip)
{
	int err = BCME_OK;
	olmsg_nd_setip nd_setip;

	if (ol->ol_flags & WL_OFFLOAD_ENAB_ND) {
		nd_setip.hdr.type = BCM_OL_ND_SETIP;
		nd_setip.hdr.seq = 0;
		nd_setip.hdr.len = sizeof(olmsg_nd_setip) - sizeof(olmsg_header);
		bcopy(host_ip, &(nd_setip.host_ip), sizeof(struct ipv6_addr));
		bcopy(host_ip, &(ol->nd_info.host_ipv6), sizeof(struct ipv6_addr));

		wlc_ol_msg_send(ol, (uint8 *)&nd_setip, sizeof(olmsg_nd_setip));
	}
	else {
		WL_TRACE(("%s: ARP offload is not active\n", __FUNCTION__));
		ASSERT(FALSE);
		err = BCME_ERROR;
	}

	return err;
}
void wlc_ol_enable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg)
{

		/* Check for valid state to enable beacon offloads */
	if (!ol || !cfg || !BSSCFG_STA(cfg) || !cfg->BSS || !cfg->up ||
		cfg != ol->wlc->cfg || !cfg->associated || !ol->num_bsscfg_allow ||
		!(ol->ol_up)) {
		WL_ERROR(("%s: Inavlid params/state. Not enabling any offloads \n",
			__FUNCTION__));
		return;
	}

	if (WLOFFLD_BCN_ENAB(ol->wlc->pub))
		wlc_ol_bcn_enable(ol, cfg);
	if ((WLOFFLD_ARP_ENAB(ol->wlc->pub)) &&
		(!WSEC_TKIP_ENABLED(cfg->wsec)))
		wlc_ol_arp_enable(ol, cfg, &(ol->arp_info.host_ip));
	if ((WLOFFLD_ND_ENAB(ol->wlc->pub)) &&
		(!WSEC_TKIP_ENABLED(cfg->wsec)))
		wlc_ol_nd_enable(ol, cfg, &(ol->nd_info.host_ipv6));
	if (WLOFFLD_IE_NOTIFICATION_ENAB(ol->wlc->pub))
		wlc_notification_set_flag(ol, cfg, (bool)1);
}

void wlc_ol_disable(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg)
{
	if (WLOFFLD_BCN_ENAB(ol->wlc->pub))
		wlc_ol_bcn_disable(ol, cfg);
	if (WLOFFLD_ARP_ENAB(ol->wlc->pub))
		wlc_ol_arp_disable(ol);
	if (WLOFFLD_ND_ENAB(ol->wlc->pub))
		wlc_ol_nd_disable(ol);
	if (WLOFFLD_IE_NOTIFICATION_ENAB(ol->wlc->pub))
		wlc_notification_set_flag(ol, cfg, (bool)0);
}

bool wlc_ol_chkintstatus(wlc_ol_info_t *ol)
{
	if (ol->mb_intstatus)
		return TRUE;
	else
		return FALSE;

}

#ifdef  __BCM_IE_NOTIFICATION__

static int
wlc_notification_set_id(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg, int id, bool enable)
{
	int err = BCME_OK;
	olmsg_ie_notification_enable  *p_ie_notification;
	/* wlc_info_t *wlc = NULL; */
	/* wlc_bss_info_t *current_bss = NULL; */

	/* Check for valid state to enable beacon offloads */
	if (!ol || !cfg || !BSSCFG_STA(cfg) || !cfg->BSS) {
		WL_ERROR(("%s: Inavlid params/state."
			" Not enabling beacon IE notification offloads\n",
			__FUNCTION__));
		return BCME_BADARG;
	}

	if ((id < 0) || (id > OLMSG_BCN_MAX_IE)) {
		WL_ERROR(("%s: Invalid params/state while changing IE notification offloads\n",
			__FUNCTION__));
		return BCME_BADARG;
	} else {
		WL_ERROR(("%s: IE (%d) notification offloads: %s\n",
			__FUNCTION__, id, (enable) ? "Enable" : "Disable"));
	}

	/* wlc = ol->wlc; */

	/* current_bss = cfg->current_bss; */

	p_ie_notification = (olmsg_ie_notification_enable *)
				MALLOC(ol->osh, sizeof(olmsg_ie_notification_enable));
	if (p_ie_notification == NULL) {
		WL_ERROR(("%s: Out of Memory. Not enabling beacon IE notification offloads \n",
			__FUNCTION__));
		return BCME_BADARG;
	}
	bzero(p_ie_notification, sizeof(olmsg_ie_notification_enable));

	/* Fill up notification enable/disable message */

	p_ie_notification->hdr.type = BCM_OL_MSG_IE_NOTIFICATION;
	p_ie_notification->hdr.seq = 0;
	p_ie_notification->hdr.len = sizeof(olmsg_ie_notification_enable)
					- sizeof(p_ie_notification->hdr);
	bcopy(&cfg->BSSID, &p_ie_notification->BSSID, sizeof(struct ether_addr));
	bcopy(&cfg->cur_etheraddr, &p_ie_notification->cur_etheraddr, sizeof(struct ether_addr));

	p_ie_notification->id = id; /* Which IE */
	p_ie_notification->enable = enable; /* Enable or disable */

	if (enable) {
		SET_ID(ol->ie_info.iemask, id);
	} else {
		RESET_ID(ol->ie_info.iemask, id);
	}

	/* Send ie notification offload enable message to CR4 */
	wlc_ol_msg_send(ol, (uint8 *)p_ie_notification, sizeof(olmsg_ie_notification_enable));

	MFREE(ol->osh, p_ie_notification, sizeof(olmsg_ie_notification_enable));
	return err;
}

static int
wlc_notification_set_flag(wlc_ol_info_t *ol, wlc_bsscfg_t *cfg, bool enable)
{
	int err = BCME_OK;
	olmsg_ie_notification_enable  *p_ie_notification;
	/* wlc_info_t *wlc = NULL; */
	/* wlc_bss_info_t *current_bss = NULL; */

	/* Check for valid state to enable beacon offloads */
	if (!ol || !cfg || !BSSCFG_STA(cfg) || (!cfg->BSS)) {
		WL_ERROR(("%s: Inavlid params/state. Not changing beacon IE notification flag \n",
			__FUNCTION__));
		return BCME_BADARG;
	} else {
		WL_ERROR(("%s: IE notification offloads flag: %s\n",
			__FUNCTION__, (enable) ? "Enable" : "Disable"));
	}

	/* wlc = ol->wlc; */

	if (enable) {
		ol->ol_flags |= WL_OFFLOAD_ENAB_IE_NOTIFICATION;
	}

	/* current_bss = cfg->current_bss; */

	p_ie_notification = (olmsg_ie_notification_enable *)
				MALLOC(ol->osh, sizeof(olmsg_ie_notification_enable));
	if (p_ie_notification == NULL) {
		WL_ERROR(("%s: Out of Memory. Not enabling beacon IE notification offloads \n",
			__FUNCTION__));
		return BCME_BADARG;
	}
	bzero(p_ie_notification, sizeof(olmsg_ie_notification_enable));

	/* Fill up notification enable/disable message */

	p_ie_notification->hdr.type = BCM_OL_MSG_IE_NOTIFICATION_FLAG;
	p_ie_notification->hdr.seq = 0;
	p_ie_notification->hdr.len = sizeof(olmsg_ie_notification_enable)
					- sizeof(p_ie_notification->hdr);
	bcopy(&cfg->BSSID, &p_ie_notification->BSSID, sizeof(struct ether_addr));
	bcopy(&cfg->cur_etheraddr,  &p_ie_notification->cur_etheraddr, sizeof(struct ether_addr));


	p_ie_notification->id = -1; /* THIS IS NOT FOR IE */
	p_ie_notification->enable = enable; /* Enable or disable */

	/* Send ie notification offload enable message to CR4 */
	wlc_ol_msg_send(ol, (uint8 *)p_ie_notification, sizeof(olmsg_ie_notification_enable));

	if (enable == 0)
		ol->ol_flags &= ~WL_OFFLOAD_ENAB_IE_NOTIFICATION;

	MFREE(ol->osh, p_ie_notification, sizeof(olmsg_ie_notification_enable));
	return err;
}

#endif /* #ifdef  __BCM_IE_NOTIFICATION__ */

#endif /* WLOFFLD */
