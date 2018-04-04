/*
 * WLAN Ingress Qos Module
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

 
#ifndef _wlc_dslcpe_iq_h_
#define _wlc_dslcpe_iq_h_

#ifdef DSLCPE_WL_IQ

/* default parameters */
#define WL_IQ_HASH_CNT			31	/* hash item number */
#define WL_IQ_HASH_TIMEOUT		5*(HZ)	/* hash item timeout: 5s */
#define WL_IQ_HASH_CLEANUP_TIMEOUT	30*(HZ)	/* hash table cleanup timeout: 30s */
#define WL_IQ_HASH_RX_HI_TIMEOUT	(HZ)/50	/* Last hi flow timeout: 20ms */

#define WL_IQ_RXDMA_THRES_LO	((NRXBUFPOST)/4)
#define WL_IQ_RXDMA_THRES_HI	((NRXBUFPOST)*3/4)

/* ret from hash table action */
#define WL_IQ_HASH_RET_SUCC	0
#define WL_IQ_HASH_RET_FAILURE	1

/* ret for flow process */
#define WL_IQ_CONT	0
#define WL_IQ_DROP	1

/* RXDMA Threshold state */
#define WL_IQ_THRES_STATE_LO	0
#define WL_IQ_THRES_STATE_HI	1

/* RXDMA buffer state */
#define WL_IQ_DMARX_RING_EMPTY		0		/* No enough buffer to refill dma RX */
#define WL_IQ_DMARX_RING_NONEMPTY	1		/* enough buffer to refill DMARX */

#define WL_IQ_MAC_PAIR_POOL_LEN		512		/* mac pair pool size */

/* Mac Pair */
typedef struct wlc_mac_pair {
	unsigned char src[ETHER_ADDR_LEN];		/* src mac */
	unsigned char dst[ETHER_ADDR_LEN];		/* dst amc */
	unsigned long last_used;					/* last used for this item */
	unsigned long rx_cnt;						/* no. of pkts received */
	unsigned long rx_drop;						/* no. of pkts dropped */
	struct wlc_mac_pair *next;
} wlc_mac_pair_t;

/* hash tab */
struct wlc_iq_info {
	wlc_info_t *wlc;
	bool			enable;
	unsigned char last_thres_state;		/* last rxdma active state */
	unsigned long last_rx_hi;				/* last hi rx jiffies */
	unsigned long rx_hi_cnt;
	unsigned long last_rx;					/* last rx jiffies */
	unsigned long rx_cnt;	
	unsigned long last_tx;					/* last tx jiffies */
	unsigned char last_dmarx_state;		/* last dma buffer state: enough buffer from pre-alloc pool */
	unsigned int 	rxactive;				/* last rxdma active number */
	unsigned short cnt;					/* hash item cnt */
	bool	 can_cleanup;					/* enable hash cleanup ? */
	unsigned long last_cleanup;			/* last hash cleanup jiffies */
	unsigned int iq_thres_hi;				/* Threshold high */
	unsigned int iq_thres_lo;				/* Threshold lo */
	unsigned int iq_hash_to;				/* Hash item Timeout */
	unsigned int iq_cleanup_to;			/* Hash cleanup periodical */
	unsigned int iq_rx_hi_to;				/* RX High flow timeout */

	struct wlc_mac_pair *mac_pair[WL_IQ_HASH_CNT];		/* hash table */
	struct wlc_mac_pair *mac_pair_pool_list;		/* mac_pair list */

	/* traffic status */
	unsigned int iq_dma_lo;
	unsigned int iq_dma_hi;
	unsigned int iq_dma_lo_keep;
	unsigned int iq_search_cnt;
	unsigned int iq_dmacnt[NRXBUFPOST];	
};

#define WL_IQ_ENAB(wlc)	((wlc)->dslcpe_iq->enable)
#define DSLCPE_IQ_PRIO(x)	(((x)&0x80000)?1:0)


extern wlc_iq_info_t *wlc_iq_attach(wlc_info_t *wlc);
extern void wlc_iq_detach(wlc_iq_info_t *dslcpe_iq);

extern void wlc_iq_mark_hi_rx(wlc_info_t *wlc);
extern void wlc_iq_rxactive(wlc_info_t *wlc, int rxactive);
extern void wlc_iq_mark_dmarx_state(wlc_info_t *wlc, unsigned char state);
extern int wlc_iq_add(wlc_info_t *wlc, unsigned char *src, unsigned char *dst);
extern int wlc_iq_remove(wlc_info_t *wlc, unsigned char *src, unsigned char *dst);
extern int wlc_iq_flow_process(wlc_info_t *wlc, unsigned char *src, unsigned char *dst);
extern void wlc_iq_hash_timeout(wlc_info_t *wlc);
extern int wlc_iq_prio(wlc_info_t *wlc, void *p);
#else

#define DSLCPE_IQ_PRIO(a)	(0)
#define WL_IQ_ENAB(wlc)		(0)

#define wlc_iq_attach(a)		do {} while (0)
#define wlc_iq_detach(a)		do {} while (0)

#define  wlc_iq_mark_hi_rx(a)	do {} while (0)
#define wlc_iq_rxactive(a,b)	do {} while (0)
#define wlc_iq_mark_dmarx_state(a,b)	do {} while (0)
#define wlc_iq_rxactive(a)	do {} while (0)
#define wlc_iq_search(a,b,c)	do {} while (0)
#define wlc_iq_add(a,b,c)		do {} while (0)
#define wlc_iq_remove(a,b,c)	do {} while (0)
#define wlc_iq_flow_process(a,b,c)	do {} while (0)
#define wlc_iq_hash_timeout(a)	do {} while (0)
#define wlc_iq_prio(a,b)		do {} while (0)
#endif /* DSLCPE_WL_IQ */

#endif /* _wlc_dslcpe_iq_h_ */
