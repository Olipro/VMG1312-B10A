/*
 *  * WLAN Ingress Qos Module
 *
 * Copyright 2010, Broadcom Corporation
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id$
 */

 #include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <wlioctl.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_bsscfg.h>
#include <wlc.h>

#include "wlc_iq.h"

#include <linux/blog.h>

enum {
	IOV_IQ_THRES_HI = 1,
	IOV_IQ_THRES_LO,
	IOV_IQ_HASH_TO,
	IOV_IQ_CLEANUP_TO,
	IOV_IQ_RX_HI_TO,
	IOV_IQ_ENABLE,
	IOV_IQ_HASH_SHOW,
	IOV_IQ_TRACE,
	IOV_IQ_CLEAN,
	IOV_LAST 		/* In case of a need to check max ID number */
};

static const bcm_iovar_t iq_iovars[] = {
	{"iq_thres_hi", IOV_IQ_THRES_HI,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"iq_thres_lo", IOV_IQ_THRES_LO,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"iq_hash_to", IOV_IQ_HASH_TO,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"iq_cleanup_to", IOV_IQ_CLEANUP_TO,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"iq_rx_hi_to", IOV_IQ_RX_HI_TO,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"iq_enable", IOV_IQ_ENABLE,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_BOOL, 0
	},
	{"iq_show", IOV_IQ_HASH_SHOW,
	(IOVF_NTRL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"iq_trace", IOV_IQ_TRACE,
	(IOVF_WHL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{"iq_clean", IOV_IQ_CLEAN,
	(IOVF_WHL|IOVF_OPEN_ALLOW), IOVT_UINT16, 0
	},
	{NULL, 0, 0, 0, 0}
};

unsigned char wl_iq_msg_level = 0;

#define WL_IQ_HASH_VAL	(1<<0)
#define WL_IQ_FLOW_VAL	(1<<1)

#ifdef BCMDBG
#define	WL_IQ_HASH_TRACE(args)	do { if (wl_iq_msg_level & WL_IQ_HASH_VAL) {args;}} while (0)
#define	WL_IQ_FLOW_TRACE(args)	do { if (wl_iq_msg_level & WL_IQ_FLOW_VAL) {args;}} while (0)
#else
#define	WL_IQ_HASH_TRACE(args)
#define	WL_IQ_FLOW_TRACE(args)
#endif /* BCMDBG */

static void printk_mac(unsigned char *src, unsigned char *dst)
{
	printk("src[%02x:%02x:%02x:%02x:%02x:%02x]     ", src[0],src[1],src[2],src[3],src[4],src[5]);
	printk("dst[%02x:%02x:%02x:%02x:%02x:%02x]\n",   dst[0],dst[1],dst[2],dst[3],dst[4],dst[5]);
}

static int wlc_iq_print(wlc_iq_info_t *dslcpe_iq)
{
	int key =0;
	wlc_mac_pair_t *ptr;

	printk("iq_enable =%u\n", dslcpe_iq ->enable);
	
	printk("last_thres_state    =%u\n", dslcpe_iq->last_thres_state);
	printk("last_dmarx_state  =%u\n", dslcpe_iq->last_dmarx_state);
	printk("rxactive               =%u\n", dslcpe_iq->rxactive);
	printk("last_rx_hi             =%lu\n", dslcpe_iq->last_rx_hi);
	printk("rx_hi_cnt              =%lu\n", dslcpe_iq->rx_hi_cnt);
	printk("last_rx                 =%lu\n", dslcpe_iq->last_rx);
	printk("rx_cnt                  =%lu\n", dslcpe_iq->rx_cnt);
	printk("cnt                       =%u\n", dslcpe_iq->cnt);
	printk("can_cleanup         =%u\n", dslcpe_iq->can_cleanup);
	printk("last_cleanup         =%lu\n", dslcpe_iq->last_cleanup);
	printk("iq_thres_hi           =%u\n", dslcpe_iq ->iq_thres_hi);
	printk("iq_thres_lo           =%u\n", dslcpe_iq ->iq_thres_lo);
	printk("iq_hash_to           =%u\n", dslcpe_iq ->iq_hash_to);
	printk("iq_cleanup_to       =%u\n", dslcpe_iq ->iq_cleanup_to);
	printk("iq_rx_hi_to           =%u\n", dslcpe_iq ->iq_rx_hi_to);
	printk("iq_dma_low[%u] iq_dma_hi[%u] iq_dma_lo_keep[%u] iq_search_cnt[%u]\n", 
		dslcpe_iq->iq_dma_lo, dslcpe_iq->iq_dma_hi, dslcpe_iq->iq_dma_lo_keep, dslcpe_iq->iq_search_cnt);

	printk("iq_dmacnt:\n");
	for (key=0; key<NRXBUFPOST; key++) {
		if (dslcpe_iq->iq_dmacnt[key])
			printk("[%d]=[%d]\n", key, dslcpe_iq->iq_dmacnt[key]);
	}

	printk("inQos Hash list@jiffies[%lu]:\n", jiffies);
	for (key =0; key <WL_IQ_HASH_CNT; key ++) {
		ptr= dslcpe_iq->mac_pair[key];
		while (ptr !=NULL) {
			printk("key=%x last_used=%lu rx_cnt=%lu rx_drop=%lu\n", 
				key, ptr->last_used, ptr->rx_cnt, ptr->rx_drop);
			printk_mac(ptr->src, ptr->dst);
			
			ptr = ptr->next;
		}
	}
	return 0;
}


/* calculate hash key */
inline unsigned short wlc_iq_hash_key(unsigned char *src, unsigned char *dst)
{
	int len=0;
	unsigned short sum=0;

	for (;len<ETHER_ADDR_LEN; len++)
		sum += src[len] + dst[len];
	
	return (sum%WL_IQ_HASH_CNT);
}


/* mark hi flow exist: called from wl_data_sendup when flow is marked as high*/
void wlc_iq_mark_hi_rx(wlc_info_t *wlc)
{
	wlc->dslcpe_iq->last_rx_hi = jiffies;
	(wlc->dslcpe_iq->rx_hi_cnt)++;
	WL_IQ_HASH_TRACE(printk("%s::last_rx_hi=%lu\n", __FUNCTION__, wlc->dslcpe_iq->last_rx_hi));
	return;
}

/* mark dmarx buffer state: called from dma_rxfill when rx pkt */
void wlc_iq_mark_dmarx_state(wlc_info_t *wlc, unsigned char state)
{
	if (state)
		wlc->dslcpe_iq->last_dmarx_state = WL_IQ_DMARX_RING_EMPTY;
	else
		wlc->dslcpe_iq->last_dmarx_state = WL_IQ_DMARX_RING_NONEMPTY;
	
	WL_IQ_HASH_TRACE(printk("%s::last_dmarx_state=%x\n", 
		__FUNCTION__, wlc->dslcpe_iq->last_dmarx_state));
		
	return;
}

/* update the rxdma rxactive data */
void wlc_iq_rxactive(wlc_info_t *wlc, int rxactive)
{
	wlc_iq_info_t *dslcpe_iq = wlc->dslcpe_iq;
	
	dslcpe_iq->rxactive = rxactive;

	(dslcpe_iq->iq_dmacnt[rxactive])++;
	
	WL_IQ_HASH_TRACE(printk("%s@%d rxactive=%x\n", __FUNCTION__, __LINE__, rxactive));
	
	/* last rx jiffies */
	dslcpe_iq->last_rx = jiffies;
	return;
}

/* serach mac pair : called from wl_iq_flow_process */
inline wlc_mac_pair_t *wlc_iq_search(wlc_info_t *wlc, unsigned char *src, unsigned char *dst)
{
	unsigned short hash_key = wlc_iq_hash_key(src, dst);
	wlc_iq_info_t *dslcpe_iq = wlc->dslcpe_iq;
	wlc_mac_pair_t *ptr = dslcpe_iq->mac_pair[hash_key];

	WL_IQ_HASH_TRACE(printk_mac(src,dst));
	
	dslcpe_iq->iq_search_cnt++;

	while (ptr != NULL) {
		if (!bcmp(ptr->src, src, ETHER_ADDR_LEN ) && !bcmp(ptr->dst, dst, ETHER_ADDR_LEN)) {
			ptr->last_used = jiffies;
			ptr->rx_cnt++;
			/* found pair*/
			return ptr;
		}
		ptr = ptr->next;
	}

	return (wlc_mac_pair_t *)NULL;
}

/* Add mac pair to hash. called from  wlc_recvdata_sendup to add */
int wlc_iq_add(wlc_info_t *wlc, unsigned char *src, unsigned char *dst)
{
	unsigned short hash_key;
	wlc_iq_info_t *dslcpe_iq = wlc->dslcpe_iq;
	wlc_mac_pair_t *ptr;

	hash_key = wlc_iq_hash_key(src, dst);
	ptr = dslcpe_iq->mac_pair[hash_key];

	WL_IQ_HASH_TRACE((printk("%s@%d", __FUNCTION__, __LINE__),printk_mac( src, dst)));

	/* search first */
	while (ptr != NULL) {
		if (!bcmp(ptr->src, src, ETHER_ADDR_LEN ) && !bcmp(ptr->dst, dst, ETHER_ADDR_LEN)) {
			ptr->last_used = jiffies;
			ptr->rx_cnt++;
			/* found pair*/
			return WL_IQ_HASH_RET_SUCC;
		}
		ptr = ptr->next;
	}

	/* add this item */
	ptr = dslcpe_iq->mac_pair_pool_list;
	if (!ptr) {
		WL_ERROR(("couldn't alloc IQ mac pair\n"));
		return WL_IQ_HASH_RET_FAILURE;
	}
	dslcpe_iq->mac_pair_pool_list = ptr->next;

	bzero(ptr, sizeof(wlc_mac_pair_t));

	ptr->last_used = jiffies;
	bcopy(src, ptr->src, ETHER_ADDR_LEN);
	bcopy(dst, ptr->dst, ETHER_ADDR_LEN);
	ptr->next = dslcpe_iq->mac_pair[hash_key];
				
	dslcpe_iq->mac_pair[hash_key] = ptr;
	dslcpe_iq->cnt++;

	WL_IQ_HASH_TRACE((printk("%s@%d", __FUNCTION__, __LINE__),wlc_iq_print(dslcpe_iq)));

	return WL_IQ_HASH_RET_SUCC;
}

#if 0
static void wlc_iq_reset(wlc_iq_info_t *dslcpe_iq)
{
	wlc_mac_pair_t *ptr;
	int key;

	dslcpe_iq ->last_cleanup = jiffies;
	dslcpe_iq->cnt = 0;
	
	/* Clean up timeout items */
	for (key =0; key <WL_IQ_HASH_CNT; key ++) {
		ptr = dslcpe_iq->mac_pair[key];
		while (ptr !=NULL) {
			/* Remove the first mac-pair item  */
			dslcpe_iq->mac_pair[key] = ptr->next;

			/* return mac pair to free list*/
			ptr->next = dslcpe_iq->mac_pair_pool_list;
			dslcpe_iq->mac_pair_pool_list = ptr;

			ptr= dslcpe_iq->mac_pair[key];
			ptr = ptr->next;
		}
	}
	WL_IQ_HASH_TRACE((printk("%s@%d iq hash reset. jiffies=%lu\n", __FUNCTION__, __LINE__, jiffies),\
		wlc_iq_print(dslcpe_iq)));
	return;
}
#endif

int wlc_iq_remove(wlc_info_t *wlc, unsigned char *src, unsigned char *dst)
{
	unsigned short hash_key = wlc_iq_hash_key(src, dst);
	wlc_iq_info_t *dslcpe_iq = wlc->dslcpe_iq;
	wlc_mac_pair_t *ptr = dslcpe_iq->mac_pair[hash_key], *last;
	unsigned char found =0, first = 1;
	
	last = ptr;

	/* search first */
	while (ptr != NULL) {
		if (!bcmp(ptr->src, src, ETHER_ADDR_LEN ) && !bcmp(ptr->dst, dst, ETHER_ADDR_LEN)) {
			ptr->last_used = jiffies;
			/* found pair*/
			found = 1;
			break;
		}
		last = ptr;
		ptr = ptr->next;
		first = 0;
	}

	if (found) {
		dslcpe_iq->cnt--;
		ASSERT(dslcpe_iq->cnt >=0);
		
		if (likely(first))
			dslcpe_iq->mac_pair[hash_key] = ptr->next;
		else
			last->next = ptr->next;

		/* return mac pair to free list*/
		ptr->next = dslcpe_iq->mac_pair_pool_list;
		dslcpe_iq->mac_pair_pool_list = ptr;
	}

	return WL_IQ_HASH_RET_SUCC;
}


/* make decision on flow drop/accept. called from wlc_recvdata */
int wlc_iq_flow_process(wlc_info_t *wlc, unsigned char *src, unsigned char *dst)
{
	wlc_iq_info_t *dslcpe_iq = wlc->dslcpe_iq;
	unsigned int rxactive = dslcpe_iq->rxactive;
	wlc_mac_pair_t *ptr;
	
	WL_IQ_FLOW_TRACE(printk("%s::rxactive=%x last_rx_hi=%lu jiffies=%lu\n", \
		 __FUNCTION__, rxactive, dslcpe_iq ->last_rx_hi, jiffies));
	WL_IQ_FLOW_TRACE(printk("last_thres_state=%x last_dmarx_state=%x\n", \
		dslcpe_iq->last_thres_state, dslcpe_iq->last_dmarx_state));

	dslcpe_iq->rx_cnt++;
	
	/* Step 1: Congetsion not occurs: if high thres or middle from high */
	if ((rxactive > dslcpe_iq ->iq_thres_hi) || 
		((rxactive <= dslcpe_iq ->iq_thres_hi) && (rxactive > dslcpe_iq ->iq_thres_lo) && 
		(dslcpe_iq->last_thres_state == WL_IQ_THRES_STATE_HI))) {
		
		dslcpe_iq->iq_dma_hi++;

		dslcpe_iq->can_cleanup = TRUE; /* can cleanup*/

		if (rxactive > dslcpe_iq ->iq_thres_hi)
			dslcpe_iq->last_thres_state = WL_IQ_THRES_STATE_HI;

		/* rxdma ring is empty (no buffer to filled in) ? */
		if (dslcpe_iq->last_dmarx_state == WL_IQ_DMARX_RING_EMPTY) {
				/* high flow exist and this is low pkt, drop it */
				if ((time_is_after_jiffies(dslcpe_iq ->last_rx_hi + dslcpe_iq ->iq_rx_hi_to))) {
					if ((ptr= wlc_iq_search(wlc,src,dst)) != NULL) {
						ptr->rx_drop++;
						WL_IQ_FLOW_TRACE(printk("%s@%d low pkt, RxDMA lack of buffer,"\
							 "drop Lo flow\n", __FUNCTION__, __LINE__));
						return WL_IQ_DROP;
					}
				}
		}
		
		WL_IQ_FLOW_TRACE(printk("accept pkt under no congestion\n"));
		return WL_IQ_CONT;
	}

	/* Step 2: Conjestion occurs: if reach low or middle from low */
	if ((rxactive <= dslcpe_iq ->iq_thres_lo) ||
		((rxactive <= dslcpe_iq ->iq_thres_hi) && (rxactive > dslcpe_iq ->iq_thres_lo) && 
		(dslcpe_iq->last_thres_state == WL_IQ_THRES_STATE_LO))) {

		dslcpe_iq->iq_dma_lo++;

		dslcpe_iq->can_cleanup = FALSE; /* not cleanup */

		if (rxactive <= dslcpe_iq ->iq_thres_lo)
			dslcpe_iq->last_thres_state = WL_IQ_THRES_STATE_LO;

		/* no high flow exist, goto rx next */
		if (time_is_before_jiffies(dslcpe_iq ->last_rx_hi + dslcpe_iq ->iq_rx_hi_to)) {
			dslcpe_iq->iq_dma_lo_keep++;
			WL_IQ_FLOW_TRACE(printk("%s@%d only low flow, Cont next step\n", __FUNCTION__, __LINE__));
			return WL_IQ_CONT;
		}

		if ((ptr=wlc_iq_search(wlc,src,dst))!=NULL ) {
			/* this is low flow, then drop it */
			ptr->rx_drop++;

			WL_IQ_FLOW_TRACE(printk("%s@%d Low, congestion. Drop\n", __FUNCTION__, __LINE__));
			WL_IQ_FLOW_TRACE((printk_mac(src, dst), wlc_iq_print(dslcpe_iq)));
			return WL_IQ_DROP;
		}
		
		/* rxdma ring is empty?: lots of pkt is in ring, drop or not leaves for next pkt handling */
		
		/* accept */
		WL_IQ_FLOW_TRACE(printk("%s@%d hi pkt under congestion, accept\n", __FUNCTION__, __LINE__));
		return WL_IQ_CONT;
	}

	return WL_IQ_CONT;			
		
}

/* check flow prio */
int wlc_iq_prio(wlc_info_t *wlc, void *p)
{
	return blog_iq_prio(p, NULL, TYPE_ETH, 0, BLOG_WLANPHY);
}
	
/* hash timeout: call back watchdog timer, 1s */
static int wlc_iq_watchdog(void *handle)
{
	wlc_iq_info_t *dslcpe_iq = (wlc_iq_info_t *)handle;
	wlc_mac_pair_t *ptr, *last;
	int key;
	unsigned char first = 1;

	/* allow cleanup?? */
	if (!(dslcpe_iq->can_cleanup) || !(dslcpe_iq->enable))
		return 0;

	/* timeout for cleanup? */
	if (time_is_after_jiffies((dslcpe_iq ->last_cleanup + dslcpe_iq ->iq_cleanup_to)))
		return 0;

	dslcpe_iq ->last_cleanup = jiffies;

	/* Clean up timeout items */
	for (key  = 0;  key  < WL_IQ_HASH_CNT; key++) {
		ptr = dslcpe_iq->mac_pair[key];
		last = ptr;
		first = 1;
		while (ptr  != NULL) {
			if (time_is_before_jiffies((ptr ->last_used + dslcpe_iq->iq_hash_to))) {
				dslcpe_iq->cnt--;
				ASSERT(dslcpe_iq->cnt >= 0);
				
				if (likely(first)) {
					/* Remove the first mac-pair item  */
	
					dslcpe_iq->mac_pair[key] = ptr->next;
					WL_IQ_HASH_TRACE((printk("%s@%d\n", __FUNCTION__, __LINE__), \
						printk_mac(ptr->src, ptr->dst)));
					/* return mac pair to free list*/
					ptr->next = dslcpe_iq->mac_pair_pool_list;
					dslcpe_iq->mac_pair_pool_list = ptr;

					ptr= dslcpe_iq->mac_pair[key];
					continue;
				}
				else {
					last->next = ptr->next;
					WL_IQ_HASH_TRACE((printk("%s@%d\n", __FUNCTION__, __LINE__), \
						printk_mac(ptr->src, ptr->dst)));
					/* return mac pair to free list*/
					ptr->next = dslcpe_iq->mac_pair_pool_list;
					dslcpe_iq->mac_pair_pool_list = ptr;

					ptr= last->next;
					continue;
				}
			}
			last = ptr;
			ptr = ptr->next;
			first = 0;
		}
	}
	WL_IQ_HASH_TRACE((printk("%s@%d jiffies=%lu\n", __FUNCTION__, __LINE__, jiffies),wlc_iq_print(dslcpe_iq)));
	return 0;
}

#if defined(BCMDBG) && defined(BCMDBG_DUMP)
static int wlc_iq_dump(wlc_iq_info_t *dslcpe_iq, struct bcmstrbuf *b)
{
	int key =0;
	wlc_mac_pair_t *ptr;

	bcm_bprintf(b, "iq_enable           =%u\n", dslcpe_iq ->enable);
	
	bcm_bprintf(b, "last_thres_state  =%u\n", dslcpe_iq->last_thres_state);
	bcm_bprintf(b, "last_dmarx_state=%u\n", dslcpe_iq->last_dmarx_state);
	bcm_bprintf(b, "rxactive             =%u\n", dslcpe_iq->rxactive);
	bcm_bprintf(b, "last_rx_hi          =%lu\n", dslcpe_iq->last_rx_hi);
	bcm_bprintf(b, "rx_hi_cnt          =%lu\n", dslcpe_iq->rx_hi_cnt);
	bcm_bprintf(b, "last_rx              =%lu\n", dslcpe_iq->last_rx);
	bcm_bprintf(b, "rx_cnt          =%lu\n", dslcpe_iq->rx_cnt);
	bcm_bprintf(b, "cnt                    =%u\n", dslcpe_iq->cnt);
	bcm_bprintf(b, "can_cleanup       =%u\n", dslcpe_iq->can_cleanup);
	bcm_bprintf(b, "last_cleanup      =%lu\n", dslcpe_iq->last_cleanup);

	bcm_bprintf(b, "iq_thres_hi        =%u\n", dslcpe_iq ->iq_thres_hi);
	bcm_bprintf(b, "iq_thres_lo        =%u\n", dslcpe_iq ->iq_thres_lo);
	bcm_bprintf(b, "iq_hash_to        =%u\n", dslcpe_iq ->iq_hash_to);
	bcm_bprintf(b, "iq_cleanup_to    =%u\n", dslcpe_iq ->iq_cleanup_to);
	bcm_bprintf(b, "iq_rx_hi_to        =%u\n", dslcpe_iq ->iq_rx_hi_to);

	bcm_bprintf(b, "inQos Hash list:\n");

	for (key =0; key <WL_IQ_HASH_CNT; key ++) {
		ptr= dslcpe_iq->mac_pair[key];
		while (ptr !=NULL) {
			bcm_bprintf(b, "[%x]last_used=%lu rx_cnt=%lu rx_drop=%lu\n",
				key, ptr->last_used, ptr->rx_cnt, ptr->rx_drop);
			bcm_bprintf(b, "src  %02x:%02x:%02x:%02x:%02x:%02x\n", 
				ptr->src[0], ptr->src[1],ptr->src[2],ptr->src[3],ptr->src[4],ptr->src[5]);
			bcm_bprintf(b, "src  %02x:%02x:%02x:%02x:%02x:%02x\n", 
				ptr->dst[0], ptr->dst[1],ptr->dst[2],ptr->dst[3],ptr->dst[4],ptr->dst[5]);
	
			ptr = ptr->next;
		}
	}
	return 0;
}
#endif /* BCMDBG && BCMDBG_DUMP */

static int
wlc_iq_down(void *handle)
{
	return 0;
}

static int
wlc_iq_doiovar(void *handle, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{

	wlc_iq_info_t *dslcpe_iq = (wlc_iq_info_t *)handle;

	int err = 0;
	int32 int_val = 0;
	bool bool_val = FALSE;
	int32 *ret_int_ptr;

	/* convenience int and bool vals for first 4 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));
	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_IQ_THRES_HI):
		*ret_int_ptr = (int32)dslcpe_iq->iq_thres_hi;
		break;

	case IOV_SVAL(IOV_IQ_THRES_HI):
		dslcpe_iq->iq_thres_hi = (int8)int_val;
		break;

	case IOV_GVAL(IOV_IQ_THRES_LO):
		*ret_int_ptr = (int32)dslcpe_iq->iq_thres_lo;
		break;

	case IOV_SVAL(IOV_IQ_THRES_LO):
		dslcpe_iq->iq_thres_lo = (int8)int_val;
		break;

	case IOV_GVAL(IOV_IQ_HASH_TO):
		*ret_int_ptr = (int32)dslcpe_iq->iq_hash_to;
		break;

	case IOV_SVAL(IOV_IQ_HASH_TO):
		dslcpe_iq->iq_hash_to = (int8)int_val;
		break;

	case IOV_GVAL(IOV_IQ_CLEANUP_TO):
		*ret_int_ptr = (int32)dslcpe_iq->iq_cleanup_to;
		break;

	case IOV_SVAL(IOV_IQ_CLEANUP_TO):
		dslcpe_iq->iq_cleanup_to = (int8)int_val;
		break;

	case IOV_GVAL(IOV_IQ_RX_HI_TO):
		*ret_int_ptr = (int32)dslcpe_iq->iq_rx_hi_to;
		break;

	case IOV_SVAL(IOV_IQ_RX_HI_TO):
		dslcpe_iq->iq_rx_hi_to = (int8)int_val;
		break;

	case IOV_GVAL(IOV_IQ_ENABLE):
		*ret_int_ptr = (int32)dslcpe_iq->enable;
		break;

	case IOV_SVAL(IOV_IQ_ENABLE):
		dslcpe_iq->enable = bool_val;
		break;

	
	case IOV_GVAL(IOV_IQ_HASH_SHOW):
		/* print iq hash contents */
		wlc_iq_print(dslcpe_iq);
		*ret_int_ptr = 0;
		break;

	case IOV_GVAL(IOV_IQ_TRACE):
		*ret_int_ptr = (int32)wl_iq_msg_level;
		break;

	case IOV_SVAL(IOV_IQ_TRACE):
		wl_iq_msg_level = (uint8)int_val;
		break;
	case IOV_GVAL(IOV_IQ_CLEAN):
		bzero(dslcpe_iq->iq_dmacnt, sizeof(unsigned int)*(NRXBUFPOST));
		dslcpe_iq->iq_search_cnt = 0;
		dslcpe_iq->iq_dma_hi = 0;
		dslcpe_iq->iq_dma_lo = 0;
		dslcpe_iq->iq_dma_lo_keep = 0;
		*ret_int_ptr = (uint8)0;
		break;
	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

void BCMATTACHFN(wlc_iq_detach)(wlc_iq_info_t *dslcpe_iq)
{
	wlc_mac_pair_t *ptr, *next;
	int key;

	if (dslcpe_iq==NULL)
		return;

	wlc_module_unregister(dslcpe_iq->wlc->pub, "inQos", dslcpe_iq);

	
	/* Free hash element */
	for (key =0; key <WL_IQ_HASH_CNT; key ++) {
		ptr = dslcpe_iq->mac_pair[key];
		while (ptr !=NULL) {
				next = ptr->next;
				MFREE(dslcpe_iq->wlc->osh, ptr, sizeof(wlc_mac_pair_t));
				ptr = next;
		}
	}

	ptr=dslcpe_iq->mac_pair_pool_list;
	while (ptr) {
		dslcpe_iq->mac_pair_pool_list = ptr->next;
		MFREE(dslcpe_iq->wlc->osh, ptr, sizeof(wlc_mac_pair_t));
		ptr = dslcpe_iq->mac_pair_pool_list;
	}
		

	MFREE(dslcpe_iq->wlc->osh, dslcpe_iq, sizeof(wlc_iq_info_t));

	return;

}

wlc_iq_info_t* BCMATTACHFN(wlc_iq_attach)(wlc_info_t *wlc)
{
	wlc_iq_info_t *dslcpe_iq;
	int cnt;
	wlc_mac_pair_t *ptr;
	
	if (!(dslcpe_iq = (wlc_iq_info_t *)MALLOC(wlc->osh, sizeof(wlc_iq_info_t)))) {
		WL_ERROR(("wl%d: wlc_iq_attach: out of memory, malloced %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero(dslcpe_iq, sizeof(wlc_iq_info_t));
	
	dslcpe_iq ->last_cleanup = jiffies;
	dslcpe_iq ->iq_thres_hi = WL_IQ_RXDMA_THRES_HI;
	dslcpe_iq ->iq_thres_lo = WL_IQ_RXDMA_THRES_LO;
	dslcpe_iq ->iq_hash_to = WL_IQ_HASH_TIMEOUT;
	dslcpe_iq ->iq_cleanup_to = WL_IQ_HASH_CLEANUP_TIMEOUT;
	dslcpe_iq ->iq_rx_hi_to = WL_IQ_HASH_RX_HI_TIMEOUT;
	dslcpe_iq ->can_cleanup = FALSE;
	dslcpe_iq->enable = FALSE;
	dslcpe_iq->mac_pair_pool_list = NULL;

	for (cnt=0; cnt < WL_IQ_MAC_PAIR_POOL_LEN; cnt++) {
		if (!(ptr = (wlc_mac_pair_t *)MALLOC(wlc->osh, sizeof(wlc_mac_pair_t)))) {
			WL_ERROR(("%s@%d wl%d: iq alloc mac_pair failed\n", __FUNCTION__, __LINE__, wlc->pub->unit));
			goto err;
		}
		ptr->next = dslcpe_iq->mac_pair_pool_list;
		dslcpe_iq->mac_pair_pool_list = ptr;
	}
			
	/* register module */
	if (wlc_module_register(wlc->pub, iq_iovars, "inQos",
		dslcpe_iq, wlc_iq_doiovar, wlc_iq_watchdog, NULL, wlc_iq_down)) {
		WL_ERROR(("wl%d: iq wlc_module_register() failed\n", wlc->pub->unit));
		goto err;
	}

	dslcpe_iq->wlc = wlc;

#if defined(BCMDBG) && defined(BCMDBG_DUMP)
	wlc_dump_register(wlc->pub, "inQos", (dump_fn_t)wlc_iq_dump, (void *)dslcpe_iq);
#endif /* BCMDBG && BCMDBG_DUMP */

	return dslcpe_iq;

err:
	wlc_iq_detach(dslcpe_iq);
	return NULL;
}

