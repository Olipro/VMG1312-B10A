/*
<:label-BRCM:2011:DUAL/GPL:standard

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License, version 2, as published by
the Free Software Foundation (the "GPL").

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.


A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.

:>
*/
/**************************************************************************
 * File Name  : bcmxtmrtbond.c
 *
 * Description: This file implements BCM6368 ATM/PTM bonding network device driver
 *              runtime processing - sending and receiving data.
 *              Current implementation pertains to PTM bonding. Broadcom ITU G.998.2 
 *              solution.
 ***************************************************************************/


/* Includes. */
#ifndef FAP_4KE
//#define DUMP_DATA
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/interrupt.h>
#include <linux/ioport.h>
#include <linux/version.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/skbuff.h>
#include <linux/rtnetlink.h>
#include <linux/ethtool.h>
#include <linux/if_arp.h>
#include <linux/ppp_channel.h>
#include <linux/ppp_defs.h>
#include <linux/if_ppp.h>
#include <linux/atm.h>
#include <linux/atmdev.h>
#include <linux/atmppp.h>
#include <linux/blog.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/nbuff.h>
#include "bcmxtmrtimpl.h"
#include "bcmPktDma.h"
#endif   /* FAP_4KE */

#ifdef FAP_4KE
#include "fap4ke_local.h"
#include "fap4ke_printer.h"
#include "bcmxtmrtbond.h"
#endif

#ifdef PERF_MON_BONDING_US
#include <asm/pmonapi.h>
#endif

#ifdef PERF_MON_BONDING_US
#define PMON_US_LOG(x)        pmon_log(x)
#define PMON_US_CLR(x)        pmon_clr(x)
#define PMON_US_BGN           pmon_bgn()
#define PMON_US_END(x)        pmon_end(x)
#define PMON_US_REG(x,y)      pmon_reg(x,y)
#else
#define PMON_US_LOG(x)        
#define PMON_US_CLR(x)        
#define PMON_US_BGN
#define PMON_US_END(x)        
#define PMON_US_REG(x,y)
#endif

#if defined(FAP_4KE) || !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))

static inline int getPtmFragmentLen(uint32 len, int32* pilCredit) ;
static int constructPtmBondHdr(XtmRtPtmTxBondHeader *ptmBondHdr_p, uint32 ulPtmPrioIdx, uint32 len) ;


/***************************************************************************
 * Function Name: getPtmFragmentLen
 * Description  : Compute next fragment length
 * Returns      : fragment length.
 ***************************************************************************/
static inline int getPtmFragmentLen(uint32 len, int32* pilCredit)
{
	int fragmentLen ;
	int leftOver ;
   int32 credit = *((int32 *) pilCredit), availCredit ;

   if (credit >= XTMRT_PTM_BOND_TX_MIN_FRAGMENT_SIZE) {

      if (credit > XTMRT_PTM_BOND_TX_MAX_FRAGMENT_SIZE) {
         availCredit = XTMRT_PTM_BOND_TX_MAX_FRAGMENT_SIZE ;
         credit -= availCredit ;
      }
      else {
         availCredit = credit ;
         credit = 0 ;
      }
	}
	else {
		availCredit = XTMRT_PTM_BOND_TX_MIN_FRAGMENT_SIZE ;
		credit -= availCredit ;
	}

	if (len <= availCredit) {
		fragmentLen = len;
	}
	else
	{
		/* send as much as possible unless we don't have
			enough data left anymore for a full fragment */
		fragmentLen = availCredit ;
		leftOver    = len - fragmentLen;
		if (leftOver < XTMRT_PTM_BOND_TX_MIN_FRAGMENT_SIZE)
		{
			fragmentLen = len - XTMRT_PTM_BOND_TX_MIN_FRAGMENT_SIZE ;
			if (fragmentLen < XTMRT_PTM_BOND_TX_MIN_FRAGMENT_SIZE) 
				fragmentLen = len ;
		}
	}

	availCredit -= fragmentLen ;
	credit += availCredit ;

   *pilCredit = credit ;

	return fragmentLen;
   
}  /* getPtmFragmentLen() */

/***************************************************************************
 * Function Name: constructPtmBondHdr
 * Description  : Calculates the PTM bonding hdr information and fills it up
 *                in the global buffer to be used for the packet.
 * Returns      : NoofFragments.
 ***************************************************************************/
static int constructPtmBondHdr(XtmRtPtmTxBondHeader *pPtmBondHdr, uint32 ptmPrioIdx, uint32 len)
{
   int reloadCount, serviced ;
   int fragNo, fragLen, fragLenWithOvh ;
#ifdef FAP_4KE
   XtmRtPtmBondInfo     *pBondInfo = &(p4kePsmGbl->ptmBondInfo);
#else   
   PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo ;
   volatile XtmRtPtmBondInfo     *pBondInfo = &pGi->ptmBondInfo ;
#endif
   XtmRtPtmTxBondHeader  *pPtmHeader = pPtmBondHdr ;
   volatile uint16       *pPortDataMask = (uint16 *) &pBondInfo->portMask ;
   uint32       *pulLinkWts    = (uint32 *) &pBondInfo->ulLinkUsWt[0] ;
   int32        *pilLinkCredit = (int32 *) &pBondInfo->ilLinkUsCredit[0] ;
   uint16       *pu16ConfWtPortDist = (uint16 *) &pBondInfo->u16ConfWtPortDist[0] ;
   uint16       *pulCurrWtPortDistRunIndex   = (uint16 *) &pBondInfo->ulCurrWtPortDistRunIndex ;
   uint8        portCount ;

   fragNo = 0 ;
   len += ETH_FCS_LEN ;    /* Original Packet Len + 4 bytes of Eth FCS Len */
   
   while (len != 0) {

#ifndef PTMBOND_US_PRIO_TRAFFIC_SPLIT

      serviced = reloadCount = 0 ;
      do {

         portCount = pu16ConfWtPortDist [*pulCurrWtPortDistRunIndex] ;

         if (((*pPortDataMask >> portCount) & 0x1) != 0) {

            fragLen = getPtmFragmentLen (len, (int32*)&pilLinkCredit[portCount]) ;

            fragLenWithOvh = fragLen + XTMRT_PTM_BOND_FRAG_HDR_SIZE + XTMRT_PTM_CRC_SIZE ;

            if (pulLinkWts[portCount] >= fragLenWithOvh) { /* port is enabled with valid weights */

               len -= fragLen ;
               pPtmHeader = &pPtmBondHdr [fragNo] ;
               pPtmHeader->sVal.FragSize = fragLenWithOvh ; /* With actual hdrs/trailers */
               pPtmHeader->sVal.portSel = portCount ;
               pulLinkWts[portCount] -= fragLenWithOvh ;
               serviced = 1 ;
            }
            else {
               //fap4kePrt_Print ("P %d, W %lu W1 %lu \n", portCount, pulLinkWts[portCount],
                                 //pulLinkWts [portCount^1]) ;
               pilLinkCredit[portCount] += fragLen ;
					pulLinkWts[portCount] = 0 ;
            }
         } /* if (*pPortDataMask) */

         if (serviced == 1) {
            if (pilLinkCredit [portCount] > 0)
               break ;
            else
               pilLinkCredit[portCount] += pBondInfo->ilConfLinkUsCredit[portCount] ;
         }

			*pulCurrWtPortDistRunIndex = ((*pulCurrWtPortDistRunIndex+1) >= pBondInfo->totalWtPortDist) ? 0 :
				*pulCurrWtPortDistRunIndex+1 ;

         if (serviced == 1)
            break ;
         else { /* No port selection happened  for this fragment, reload the weights with unused credits */

				if (*pPortDataMask == 0) {
					return 0 ;
            }
				else if ((pulLinkWts[0] == 0) && (pulLinkWts[1] == 0)) {
               pulLinkWts[0] = pBondInfo->ulConfLinkUsWt[0] ;
               pulLinkWts[1] = pBondInfo->ulConfLinkUsWt[1] ;

               pilLinkCredit[0] = pBondInfo->ilConfLinkUsCredit [0] ;
               pilLinkCredit[1] = pBondInfo->ilConfLinkUsCredit [1] ;
               reloadCount++ ;
               if (reloadCount >= 2) {
                  return 0 ;
					}
            }
         }

      } while (1) ;
#else

      fragLen = getPtmFragmentLen (len, NULL) ;
      len -= fragLen ;
      pPtmHeader = &pPtmBondHdr [fragNo] ;

      fragLen += XTMRT_PTM_BOND_FRAG_HDR_SIZE + XTMRT_PTM_CRC_SIZE ;  /* With actual hdrs/trailers */
      pPtmHeader->sVal.FragSize = fragLen ;

      pPtmHeader->sVal.portSel = (ulPtmPrioIdx == PTM_FLOW_PRI_LOW) ? PHY_PORTID_0 : PHY_PORTID_1 ;

#endif /* #ifndef PTMBOND_US_PRIO_TRAFFIC_SPLIT */

      pPtmHeader->sVal.PktEop = XTMRT_PTM_BOND_HDR_NON_EOP ;
      fragNo++ ;

   } /* while (len != 0) */

   pPtmHeader->sVal.PktEop = XTMRT_PTM_BOND_FRAG_HDR_EOP ;

   return (fragNo) ;
   
}  /* constructPtmBondHdr() */

#endif   /* #if defined(FAP_4KE) || !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)) */

#if !defined(FAP_4KE)
/***************************************************************************
 * Function Name: bcmxtmrt_ptmbond_calculate_link_weights
 * Description  : Calculates the ptm bonding link weights, based on the
 *                link availability. For host execution, also forwards the
 *                the upstream rate of each link and the portMask to 4KE
 *                with a DQM message.
 * Returns      : 0
 * Note: Apply the FAP_4KE compiler flag to allow this function source shared
 * by Host and 4KE.
 ***************************************************************************/
int bcmxtmrt_ptmbond_calculate_link_weights(uint32 *pulLinkUSRates, uint32 portMask)
{
   int i, max, min, nRet = 0, mod, quot ;
   XtmRtPtmBondInfo      *pBondInfo ;
   PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo ;
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
   volatile fap4kePsm_global_t * pPsmGbl = (volatile fap4kePsm_global_t *)pHostPsmGbl(PKTDMA_US_FAP_INDEX) ;
   volatile XtmRtPtmBondInfo *p4keBondInfo_p = &(pPsmGbl->ptmBondInfo);
#endif

   pBondInfo = &pGi->ptmBondInfo ;

   for (i=0; i< MAX_BOND_PORTS; i++) {
      if (pulLinkUSRates[i] != 0)
         pBondInfo->ulConfLinkUsWt [i] = pulLinkUSRates[i]/8 ;  /* bytes/sec */
      else
         pBondInfo->ulConfLinkUsWt [i] = 0 ;
   }

   memcpy (&pBondInfo->ulLinkUsWt[0], &pBondInfo->ulConfLinkUsWt[0], (MAX_BOND_PORTS*sizeof (UINT32))) ;
   pBondInfo->portMask = portMask ;

   /* Calculate the wt port distribution */

   if (pulLinkUSRates[0] >= pulLinkUSRates[1]) {
      max = 0; min = 1 ;
   }
   else {
      max = 1; min = 0 ;
   }
            
   printk("[HOST] link0=%u link1=%u mask=0x%x \n",
                   (unsigned int)pulLinkUSRates[0], (unsigned int)pulLinkUSRates[1], (unsigned int)portMask) ;

   if (pulLinkUSRates[max] != 0) {

      if (pulLinkUSRates[min] != 0) {
         pBondInfo->ilConfLinkUsCredit [min] = XTMRT_PTM_BOND_TX_MAX_FRAGMENT_SIZE ;
			quot = ((pBondInfo->ulConfLinkUsWt [max] * XTMRT_PTM_BOND_TX_MAX_FRAGMENT_SIZE)/
			                          pBondInfo->ulConfLinkUsWt [min]) ;
			mod = ((pBondInfo->ulConfLinkUsWt [max] * XTMRT_PTM_BOND_TX_MAX_FRAGMENT_SIZE)%
			                          pBondInfo->ulConfLinkUsWt [min]) ;
			mod = ((mod*10)/pBondInfo->ulConfLinkUsWt[min]) ;
			quot = (mod>0x5) ? quot+1 : quot ;
         pBondInfo->ilConfLinkUsCredit [max] = quot ;
         pBondInfo->totalWtPortDist = 0x2 ;
         pBondInfo->u16ConfWtPortDist[min]    = min ;
         pBondInfo->u16ConfWtPortDist[max]    = max ;
      }
      else {
         pBondInfo->totalWtPortDist = 0x1 ;
         pBondInfo->u16ConfWtPortDist[0]    = max ;
         pBondInfo->ilConfLinkUsCredit[max] = XTMRT_PTM_BOND_TX_MAX_FRAGMENT_SIZE ;
         pBondInfo->ilConfLinkUsCredit[min] = 0 ;
      }
   }
   else {
      if (pulLinkUSRates[min] != 0) {
         pBondInfo->totalWtPortDist = 0x1 ;
         pBondInfo->u16ConfWtPortDist[0]    = min ;
         pBondInfo->ilConfLinkUsCredit[min] = XTMRT_PTM_BOND_TX_MAX_FRAGMENT_SIZE ;
         pBondInfo->ilConfLinkUsCredit[max] = 0 ;
      }
      else {
         pBondInfo->totalWtPortDist = 0x0 ;
         pBondInfo->u16ConfWtPortDist[0]    = 0xFF ;
         pBondInfo->ilConfLinkUsCredit[0] = 0 ;
         pBondInfo->ilConfLinkUsCredit[1] = 0 ;
      }
   }

   memcpy (&pBondInfo->ilLinkUsCredit[0], &pBondInfo->ilConfLinkUsCredit[0], (MAX_BOND_PORTS*sizeof (INT32))) ;
   pBondInfo->ulCurrWtPortDistRunIndex   = 0 ;

   pBondInfo->bonding   = 1; /* set bonding to true */

   printk("[HOST] link wt0=%u link wt1=%u mask=0x%x \n",
                   (unsigned int)pBondInfo->ulConfLinkUsWt[0], (unsigned int)pBondInfo->ulConfLinkUsWt[1], 
                   (unsigned int)pBondInfo->portMask) ;

#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
   memcpy ((XtmRtPtmBondInfo *) p4keBondInfo_p, (XtmRtPtmBondInfo *) pBondInfo, sizeof (XtmRtPtmBondInfo)) ;
   
   /* Wait for FAP to finish re-calculating the weights. */
   udelay(100);

   printk("[FAP] link wt0=%u link wt1=%u mask=0x%x \n",
                   (unsigned int)p4keBondInfo_p->ulConfLinkUsWt[0], (unsigned int)p4keBondInfo_p->ulConfLinkUsWt[1], 
                   (unsigned int)p4keBondInfo_p->portMask) ;

#endif

   return (nRet) ;
}
#endif

/***************************************************************************
 * Function Name: bcmxtmrt_ptmbond_add_hdr
 * Description  : Adds the PTM Bonding Tx header to a packet before transmitting
 *                it. The header is composed of 8 16-bit words of fragment info.
 *                Each fragment info word contains the length of the fragment and
 *                its tx port (link).
 *                The number of fragments is calculated from the packet length.
 *                The end-of-packet bit is set in the last fragment info word.
 *                The tx port for each fragment is selected based on the tx
 *                bandwidth of each link so that both links are load balanced.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     
 * Returns      : 0: fail, others: success.
 ***************************************************************************/
#ifdef FAP_4KE

int bcmxtmrt_ptmbond_addHdr_4ke(uint8 **packetTx_pp, uint16 *len_p, uint32 ptmPrioIdx, uint32 isNetPacket)
{
   XtmRtPtmTxBondHeader *ptmBondHdr = &(p4kePsmGbl->ptmBondHdr[0]) ;
   int i, frags;

   PMON_US_BGN;

   PMON_US_LOG(1);
   frags = constructPtmBondHdr(&ptmBondHdr[0], ptmPrioIdx, *len_p);
   if (frags)
   {
      *len_p +=
         (sizeof(XtmRtPtmTxBondHeader) * XTMRT_PTM_BOND_MAX_FRAG_PER_PKT);
      *packetTx_pp -=
         (sizeof(XtmRtPtmTxBondHeader) * XTMRT_PTM_BOND_MAX_FRAG_PER_PKT);
      if ((uint32)(*packetTx_pp) & 0x3)
      {
         uint16 *packet_p ;
         uint16 *hdr_p    = (uint16*)(&ptmBondHdr[0]);

         if (isNetPacket)
            packet_p = (uint16*)mCacheToNonCacheVirtAddr(*packetTx_pp);
         else
            packet_p = (uint16*)(*packetTx_pp);

         *packet_p = *hdr_p;  /* each fragment header is 2 bytes */
         for (i = 2; i <= frags; i++)
            *(++packet_p) = *(++hdr_p);
      }
      else
      {
         uint32 *packet_p ;
         uint32 *hdr_p    = (uint32*)(&ptmBondHdr[0]);
         if (isNetPacket)
            packet_p = (uint32*)mCacheToNonCacheVirtAddr(*packetTx_pp);
         else
            packet_p = (uint32*)(*packetTx_pp);
         *packet_p = *hdr_p;  /* each fragment header is 2 bytes */
         for (i = 3; i <= frags; i += 2)
            *(++packet_p) = *(++hdr_p);
      }
   }
   PMON_US_LOG(2);
   PMON_US_END(2);

   return frags;
   
} /* bcmxtmrt_ptmbond_addHdr_4ke() */

#else /* #ifdef FAP_4KE */

/* this function will be called by host regardless FAP is compiled in or not.
 */
int bcmxtmrt_ptmbond_add_hdr (PBCMXTMRT_DEV_CONTEXT pDevCtx, UINT32 ulPtmPrioIdx,
                               pNBuff_t *ppNBuff, struct sk_buff **ppNBuffSkb, 
                               UINT8 **ppData, int *pLen)
{
   int                   frags = 0 ;
#if !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
   XtmRtPtmTxBondHeader  *pPtmBondHdr ;
#endif
   int                   headroom ;
   int                   len ;
   int                   minheadroom ;
   PBCMXTMRT_GLOBAL_INFO pGi ;
   XtmRtPtmBondInfo      *pBondInfo ;
   XtmRtPtmTxBondHeader  *pPtmSrcBondHdr ;

   PMON_US_BGN ;

   minheadroom  = sizeof (XtmRtPtmTxBondHeader) * XTMRT_PTM_BOND_MAX_FRAG_PER_PKT ;
   pGi = &g_GlobalInfo ;
   pBondInfo = &pGi->ptmBondInfo ;
   pPtmSrcBondHdr = &pGi->ptmBondHdr[0] ;

   PMON_US_LOG(1) ;

   len = *pLen ;

   if ( *ppNBuffSkb == NULL) {

      struct fkbuff *fkb = PNBUFF_2_FKBUFF(*ppNBuff);
      headroom = fkb_headroom (fkb) ;

      if (headroom >= minheadroom) {
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
         /* When FAP is compiled in, just make room for the bonding header
          * without actually copy the header to the packet. The header
          * will be added in by FAP 4KE.
          */
         *ppData = fkb_push(fkb, minheadroom);
         *pLen  += minheadroom;
         frags   = 1;
#else
         frags = constructPtmBondHdr (pPtmSrcBondHdr, ulPtmPrioIdx, len) ;

			if (frags != 0) {
				PMON_US_LOG(2) ;
				*ppData = fkb_push (fkb, minheadroom) ;
				*pLen   = len + minheadroom ;
				pPtmBondHdr = (XtmRtPtmTxBondHeader *) *ppData ;
				PMON_US_LOG(3) ;
				u16cpy (pPtmBondHdr, pPtmSrcBondHdr, sizeof (XtmRtPtmTxBondHeader) * frags) ;
            DUMP_PKT ((UINT8 *) pPtmBondHdr, 16) ;
				PMON_US_LOG(4) ;
			}
#endif
      }
      else
         printk(KERN_ERR CARDNAME "bcmxtmrt_xmit: ALARM. FKB not enough headroom.\n") ;
   }
   else {
      struct sk_buff *skb = *ppNBuffSkb ;
      headroom = skb_headroom (skb) ;

      if (headroom < minheadroom) {
         struct sk_buff *skb2 = skb_realloc_headroom(skb, minheadroom);
#if 0
         XTMRT_BOND_LOG2 ("bcmxtmrt: Warning!!, headroom (%d) is less than min headroom (%d) \n",
               headroom, minheadroom) ;
#endif
         dev_kfree_skb_any(skb);
         if (skb2 == NULL) {
            printk (CARDNAME ": Fatal!!, NULL Skb \n") ;
            skb = NULL ;
         }
         else
            skb = skb2 ;
      }

      if( skb ) {
#if defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE)
         /* When FAP is compiled in, just make room for the bonding header
          * without actually copy the header to the packet. The header
          * will be added in by FAP 4KE.
          */
         *ppData = skb_push(skb, minheadroom);
         *pLen  += minheadroom;
         frags   = 1;
#else
         frags = constructPtmBondHdr (pPtmSrcBondHdr, ulPtmPrioIdx, len) ;

			if (frags != 0) {
				PMON_US_LOG(2) ;
				*ppData = skb_push (skb, minheadroom) ;
				*pLen   = len + minheadroom ;
				pPtmBondHdr = (XtmRtPtmTxBondHeader *) *ppData ;
				PMON_US_LOG(3) ;
				u16cpy (pPtmBondHdr, pPtmSrcBondHdr, sizeof (XtmRtPtmTxBondHeader) * frags) ;
            DUMP_PKT ((UINT8 *) pPtmBondHdr, 16) ;
				PMON_US_LOG(4) ;
			}
#endif
      }

      *ppNBuffSkb = skb ;
      *ppNBuff = SKBUFF_2_PNBUFF(skb) ;
   }

   PMON_US_LOG(5) ;
   PMON_US_END(5) ;

   return frags ;

} /* bcmxtmrt_ptmbond_add_hdr() */

/***************************************************************************
 * Function Name: ProcTxBondInfo
 * Description  : Displays information about Bonding Tx side counters.
 *                Currently for PTM bonding.
 * Returns      : 0 if successful or error status
 ***************************************************************************/
int ProcTxBondInfo(char *page, char **start, off_t off, int cnt,
                   int *eof, void *data)
{
   PBCMXTMRT_GLOBAL_INFO pGi = &g_GlobalInfo;
   PBCMXTMRT_DEV_CONTEXT pDevCtx;
#if !(defined(CONFIG_BCM_FAP) || defined(CONFIG_BCM_FAP_MODULE))
   volatile XtmRtPtmTxBondHeader *bondHdr_p  = &pGi->ptmBondHdr[0];
   volatile XtmRtPtmBondInfo     *bondInfo_p = &pGi->ptmBondInfo;
#else
   volatile fap4kePsm_global_t * pPsmGbl = (volatile fap4kePsm_global_t *)pHostPsmGbl(PKTDMA_US_FAP_INDEX) ;
   volatile XtmRtPtmTxBondHeader *bondHdr_p       = &pPsmGbl->ptmBondHdr[0] ;
   volatile XtmRtPtmBondInfo *bondInfo_p = &(pPsmGbl->ptmBondInfo) ;
#endif
   uint32 i;
   int sz = 0, port, eopStatus, fragSize;

   if (pGi->bondConfig.sConfig.ptmBond == BC_PTM_BONDING_ENABLE)
	{
		sz += sprintf(page + sz, "\nPTM Tx Bonding Information \n");
		sz += sprintf(page + sz, "\n========================== \n");

		sz += sprintf(page + sz, "\nPTM Header Information"); 

		for (i = 0; i < XTMRT_PTM_BOND_MAX_FRAG_PER_PKT; i++)
		{
			port       = bondHdr_p[i].sVal.portSel;
			eopStatus  = bondHdr_p[i].sVal.PktEop;
			fragSize   = bondHdr_p[i].sVal.FragSize;
			fragSize   -= (XTMRT_PTM_BOND_FRAG_HDR_SIZE+
					XTMRT_PTM_CRC_SIZE);
			if (eopStatus == 1)
			{
				sz += sprintf(page + sz, "\nFragment[%u]: port<%d> eopStatus<%d> size<%d>\n",
						(unsigned int)i, port, eopStatus, fragSize-ETH_FCS_LEN);
				break;
			}
			else
			{
				sz += sprintf(page + sz, "\nFragment[%u]: port<%u> eopStatus<%u> size<%u>\n",
						(unsigned int)i, port, eopStatus, fragSize);
			}
		} /* for (i) */

		for (i = 0; i < MAX_DEV_CTXS; i++)
		{
			pDevCtx = pGi->pDevCtxs[i];
			if ( pDevCtx != (PBCMXTMRT_DEV_CONTEXT) NULL )
			{
				sz += sprintf(page + sz, "dev: %s, ActivePortMask<%u>\n",
						pDevCtx->pDev->name, (unsigned int)pDevCtx->ulPortDataMask);
			}
		}

		for (i = 0; i < MAX_BOND_PORTS; i++ )
		{
			sz += sprintf(page + sz, "Port[%u]: ConfWt<%u> CurrWt<%u> CurrCredit<%i>\n",
					(unsigned int)i, (unsigned int)bondInfo_p->ulConfLinkUsWt[i],
					(unsigned int)bondInfo_p->ulLinkUsWt[i],
					(signed int)bondInfo_p->ilLinkUsCredit[i]);
		}

		sz += sprintf(page + sz, "totalWtPortDist : %u CurrWtPortDistRunIndex : %u \n", 
				(unsigned int) bondInfo_p->totalWtPortDist, 
				(unsigned int) bondInfo_p->ulCurrWtPortDistRunIndex) ;
				

		sz += sprintf(page + sz, "All Wt Port Distributions ==>") ;

		for( i = 0; i < bondInfo_p->totalWtPortDist; i++ ) {
			sz += sprintf(page + sz, " %u,", bondInfo_p->u16ConfWtPortDist[i]) ;
		}

		sz += sprintf(page + sz, "\n");
	}
	else
	{
		sz += sprintf(page + sz, "No Bonding Information \n");
	}

    *eof = 1;
    return( sz );
        
} /* ProcTxBondInfo() */

#endif   /* FAP_4KE */
