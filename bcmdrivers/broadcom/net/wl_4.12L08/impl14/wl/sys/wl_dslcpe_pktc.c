
/*
 * Linux-specific portion of
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
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
 * $Id: wl_dslcpe_pktc.c $
 */

#if defined(PKTC)

#include <typedefs.h>
#include <linuxver.h>
#include <osl.h>

#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>

#include <wlc_cfg.h>

#if defined(CONFIG_BLOG)
#include <linux/nbuff.h>  /* must be included before bcmutils.h */
#include <linux/blog.h>
#endif /* CONFIG_BLOG */

#include <proto/ethernet.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <wlc_key.h>

#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>

#if defined(DSLCPE) && (defined(DSLCPE_WL_IQ) || defined(DSLCPE_TX_PRIO))
#include "wlc_iq.h"
#endif /* DSLCPE && DSLCPE_WL_IQ */

#include <wl_linux.h>


const unsigned int bitmap[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
const unsigned int bitpos[] = {1, 2, 4, 8, 16};

inline int get_32bitmap_pos( uint32 v )
{
    int i;

    register unsigned int r = 0; // result of log2(v) will go here

    if (!v)
    {
        return -1;
    }
    for (i = 4; i >= 0; i--) // unroll for speed...
    {
      if (v & bitmap[i])
      {
        v >>= bitpos[i];
        r |= bitpos[i];
      } 
    }
    return r;
}
	
inline int get_16bitmap_pos( uint16 v )
{
    int i;

    register unsigned int r = 0; // result of log2(v) will go here

    if (!v)
    {
        return -1;
    }
    for (i = 3; i >= 0; i--) // unroll for speed...
    {
      if (v & bitmap[i])
      {
        v >>= bitpos[i];
        r |= bitpos[i];
      } 
    }
    return r;
}	

/* This becomes netdev->priv and is the link between netdev and wlif struct */
typedef struct priv_link {
	wl_if_t *wlif;
} priv_link_t;
#ifndef WL_DEV_IF
#define WL_DEV_IF(dev)          ((wl_if_t*)((priv_link_t*)DEV_PRIV(dev))->wlif)
#endif

#ifdef AP
extern wlc_bsscfg_t *wl_bsscfg_find(wl_if_t *wlif);
#endif

#ifdef DSLCPE_CACHE_SMARTFLUSH
extern uint dsl_tx_pkt_flush_len;
#endif
#ifdef DSLCPE_TX_PRIO
/* map prio/lvl to fifo index. fifo 0 to be lowest priority queue */
extern const uint8 priolvl2fifo[];
#endif

extern void wl_txchain_lock(wl_pktc_info_t *wl_pktci);
extern void wl_txchain_unlock(wl_pktc_info_t *wl_pktci);
extern int wl_schedule_work(wl_pktc_info_t *wl_pktci);
extern void wl_callbacks_inc(wl_pktc_info_t *wl_pktci);
extern void wl_callbacks_dec(wl_pktc_info_t *wl_pktci);
extern void BCMFASTPATH wl_start_pktc(wl_pktc_info_t *wl_pktci, struct net_device *dev, struct sk_buff *skb);

void wl_write_to_chain_table(wl_pktc_info_t *wl_pktci, uint16 chainIdx, struct sk_buff *chead, struct sk_buff *ctail, uint8 fifo)
{
    pktc_entry_t *pPktc;
	
    wl_txchain_lock(wl_pktci);
    pPktc = &(wl_pktci->pktc_table[0]);
    PKTCENQCHAINTAIL(pPktc[fifo].chain[chainIdx].chead, pPktc[fifo].chain[chainIdx].ctail, chead, ctail);
    pPktc[fifo].chainidx_bitmap |= (1 << chainIdx);
    wl_pktci->prio_bitmap |= (1 << fifo);
    wl_txchain_unlock(wl_pktci);

    return;
}

void BCMFASTPATH 
wl_start_txchain_txqwork(struct wl_task *task)
{
    wl_pktc_info_t *wl_pktci = (wl_pktc_info_t *)task->context;
    struct sk_buff *chead_tmp, *skb;
    int chainIdx, cnt, fifo;
    uint32 tmpChainIdxBitmap=0;
    pktc_entry_t *pPktc;

    wl_txchain_lock(wl_pktci);
    pPktc = &(wl_pktci->pktc_table[0]);
    while ((fifo=get_16bitmap_pos(wl_pktci->prio_bitmap)) != -1) {
        tmpChainIdxBitmap = pPktc[fifo].chainidx_bitmap;
        while ((chainIdx = get_32bitmap_pos(tmpChainIdxBitmap)) != -1) {
            cnt = 0;
            chead_tmp = skb = pPktc[fifo].chain[chainIdx].chead;
            while (skb != NULL) {
                cnt++;
                if (cnt >= wl_pktci->pub->tunables->txsbnd) {
                    pPktc[fifo].chain[chainIdx].chead = PKTCLINK(skb); //point to the next skb
                    if (pPktc[fifo].chain[chainIdx].chead == NULL)
                        pPktc[fifo].chain[chainIdx].ctail = NULL;
                    PKTSETCLINK(skb, NULL); //break the chain
                    break;
                }
                skb = PKTCLINK(skb);
            }

            if ((cnt >= 0) && (cnt < wl_pktci->pub->tunables->txsbnd)) { // less than threshold
                pPktc[fifo].chain[chainIdx].chead = pPktc[fifo].chain[chainIdx].ctail = NULL;
                pPktc[fifo].chainidx_bitmap &= ~(1<<chainIdx);    
            }

            wl_txchain_unlock(wl_pktci);

#if defined(BCMDBG)
            if (cnt != 0) {
                if (cnt <= 5)
                    WLCNTINCR(wl_pktci->pub->_cnt->txchainsz5);
                else if (cnt <= 16)
                    WLCNTINCR(wl_pktci->pub->_cnt->txchainsz16);
                else if (cnt <= 32)
                    WLCNTINCR(wl_pktci->pub->_cnt->txchainsz32);
                else if (cnt <= 64)
                    WLCNTINCR(wl_pktci->pub->_cnt->txchainsz64);
                else if (cnt <= 128)
                    WLCNTINCR(wl_pktci->pub->_cnt->txchainsz128);
                else
                    WLCNTINCR(wl_pktci->pub->_cnt->txchainsz128up);
            }
#endif

            if (chead_tmp != NULL){
                PKTCSETCNT(chead_tmp, cnt);
                wl_start_pktc(wl_pktci, chead_tmp->dev, chead_tmp);
            }

            tmpChainIdxBitmap &= ~(1<<chainIdx);
            wl_txchain_lock(wl_pktci);
        } //inner while
        if (!(pPktc[fifo].chainidx_bitmap)) {
            wl_pktci->prio_bitmap &= ~(1<<fifo);
        }
    } //outer while
    wl_txchain_unlock(wl_pktci);
		
    wl_callbacks_dec(wl_pktci);
    return;
}


/* this is for transmit path */
static uint32 chainidx_bitmap=0;
void wl_txchainhandler(struct sk_buff *skb, uint32_t brc_hot_ptr, uint32 chainIdx)
{
    wl_pktc_info_t *wl_pktci = NULL;
    ctf_brc_hot_t *brc_hot = (ctf_brc_hot_t*)brc_hot_ptr;
    int prio=0, lvl=0, cur_prio=0;
    uint8 fifo;

    /* brc_hot_ptr is validated before passing to this function and wl_handle as well by blog.c */
    wl_pktci = (wl_pktc_info_t *)(brc_hot->wl_handle);
    chainidx_bitmap |= (1<<chainIdx);

    skb->dev = brc_hot->tx_dev;
    PKTSETCLINK(skb, NULL);

#ifdef DSLCPE_CACHE_SMARTFLUSH
{
    wlc_bsscfg_t *cfg = NULL;
    wl_if_t *wlif;

    wlif = WL_DEV_IF(skb->dev);
    if (wlif != NULL) 
        cfg = wl_bsscfg_find(wlif);
    if (cfg != NULL) {
        if ((dsl_tx_pkt_flush_len == 0) || (cfg->wsec & TKIP_ENABLED)) /* to avoid MIC failure in TKIP */
        {
            PKTSETDIRTYP(wl->osh, skb, NULL);
        } else {
            uint8_t *dirty_p = PKTGETDIRTYP(wl_pktci->osh, skb);
            uint8_t *deepest = PKTDATA(NULL, skb) + 32; //dsl_tx_pkt_flush_len;
            if (dirty_p > deepest)
                deepest = dirty_p;
            if (deepest > skb->tail)
                deepest = skb->tail;
            SET_SKBSHINFO_DIRTYP_ACK(deepest);
            PKTSETDIRTYP(wl_pktci->osh, skb, deepest);
        }
    }
}
#endif /* DSLCPE_CACHE_SMARTFLUSH */

    PKT_PREALLOCINC(wl_pktci->osh, skb, 1);

    /* find skb priority */
    prio = PKTPRIO(skb)&0x7;
    lvl = DSLCPE_IQ_PRIO(skb->mark);
    cur_prio = prio << 1 | lvl;
    fifo = priolvl2fifo[cur_prio];
    brc_hot->prio_bitmap |= (1 << fifo);

    PKTSETCHAINED(wl_pktci->osh, skb);
    /* chain SKBs based on priority */			
    PKTCENQTAIL(brc_hot->chain[fifo].chead, brc_hot->chain[fifo].ctail, skb);

    brc_hot->hits++;
    return;
}

void wl_txchainhandler_complete(void)
{
    wl_pktc_info_t *wl_pktci = NULL;
    ctf_brc_hot_t *brc_hot;
    chain_pair_t  *pChain;
    int chainIdx, fifo;

    while ((chainIdx=get_32bitmap_pos(chainidx_bitmap)) != -1) {
        brc_hot = (ctf_brc_hot_t *)blog_pktc(BRC_HOT_GET_BY_IDX, NULL, chainIdx, 0);
        if (brc_hot) {
            wl_pktci = (wl_pktc_info_t *)(brc_hot->wl_handle);
            if (!wl_pktci) {
                printk("Error - wl_pktci is NULL!\n");
                return;
            }
            while ((fifo=get_16bitmap_pos(brc_hot->prio_bitmap)) != -1) {
                pChain = &(brc_hot->chain[fifo]);
                wl_write_to_chain_table(wl_pktci, brc_hot->idx, pChain->chead, pChain->ctail, fifo);
                pChain->chead = pChain->ctail = NULL;
                brc_hot->prio_bitmap &= ~(1<<fifo);
            }

            if (wl_schedule_work(wl_pktci)) {
                wl_callbacks_inc(wl_pktci);
            }
        }
        else {
            printk("Error - txchain_comp : Invalid ChainIdx %d bitMap 0x%08x\n",chainIdx,chainidx_bitmap);
            /* What about the loss of SKBs/BDs : TBD ??? */
        }
        chainidx_bitmap &= ~(1<<chainIdx);
    }

    return;
}

/* this is for receive path */
int32 wl_rxchainhandler(wl_info_t *wl, struct sk_buff *skb)
{
    ctf_brc_hot_t *brc_hot;
    uint32_t dev_xmit;

    if (PKTISCHAINED(skb)) {
        struct ether_header *eh = (struct ether_header *)PKTDATA(wl->osh, skb);
        brc_hot = (ctf_brc_hot_t *)blog_pktc(BRC_HOT_GET_BY_DA, NULL, (uint32_t)(eh->ether_dhost), 0);
        if (brc_hot && brc_hot->tx_dev != NULL) {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,30)
            dev_xmit = (uint32_t)(brc_hot->tx_dev->netdev_ops->ndo_start_xmit);
#else
            dev_xmit = (uint32_t)(brc_hot->tx_dev->hard_start_xmit);
#endif
            if (dev_xmit) {
                WLCNTADD(wl->pub->_cnt->enet, PKTCCNT(skb));
                brc_hot->hits ++;
                /* call enet xmit directly */
                ((HardStartXmitFuncP)dev_xmit)(skb, brc_hot->tx_dev);
                return (BCME_OK);
            }
        }
    }
    return (BCME_ERROR);
}

#endif // PKTC

