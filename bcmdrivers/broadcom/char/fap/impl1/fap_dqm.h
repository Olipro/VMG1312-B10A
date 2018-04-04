/************************************************************
 * <:copyright-BRCM:2009:DUAL/GPL:standard
 * 
 *    Copyright (c) 2009 Broadcom Corporation
 *    All Rights Reserved
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * :>
 ************************************************************/

#ifndef __FAPDQM_H_INCLUDED__ 
#define __FAPDQM_H_INCLUDED__


/******************************************************************************
* File Name  : fap_dqm.h                                                      *
*                                                                             *
* Description: This is the main header file of the DQM implementation for     * 
*              the FAP MIPS core.                                             *
******************************************************************************/

#include    "bcmtypes.h"
#include    "fap_hw.h"

/* Queue Definitions */
#define DQM_MAX_QUEUE                   32
#define DQM_MAX_HANDLER                 8

#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
typedef struct {
    uint32 pBuf;
    uint32 dmaWord0;
    uint32 na1;
    uint32 na2;
} fapDqm_XtmRx_t;


#define DQM_FAP2HOST_XTM_RX_Q_SIZE		    2
#define DQM_IUDMA_XTM_RX_BUDGET		        100

#define DQM_FAP2HOST_XTM_RX_Q_LOW         	0
    #define DQM_FAP2HOST_XTM_RX_DEPTH_LOW   104
       

#define DQM_FAP2HOST_XTM_RX_Q_RSVD1	        1

#define DQM_FAP2HOST_XTM_RX_Q_HI   	        2
    #define DQM_FAP2HOST_XTM_RX_DEPTH_HI    40

#define DQM_FAP2HOST_XTM_RX_Q_RSVD2          3

typedef struct {
    uint8   *pBuf;           // 32  word0
    uint8    source;         // 8   word1
    uint8    channel;        // 8   word1
    uint16   len;            // 16  word1
    uint32   key;            // 32  word2
    uint16   dmaStatus;      // 16  word3
    uint16   param1;         // 16  word3
} fapDqm_XtmTx_t;

#define DQM_HOST2FAP_XTM_XMIT_Q_SIZE           4
#define DQM_HOST2FAP_XTM_XMIT_BUDGET           80

#define DQM_HOST2FAP_XTM_XMIT_Q_LOW        	   4
    #define DQM_HOST2FAP_XTM_XMIT_DEPTH_LOW    50


#define DQM_HOST2FAP_XTM_XMIT_Q_RSVD1          5

#define DQM_HOST2FAP_XTM_XMIT_Q_HI             6
    #define DQM_HOST2FAP_XTM_XMIT_DEPTH_HI     16
       

#define DQM_HOST2FAP_XTM_FREE_RXBUF_Q           7
    #define DQM_HOST2FAP_XTM_FREE_RXBUF_Q_SIZE  2
    #define DQM_HOST2FAP_XTM_FREE_RXBUF_DEPTH  \
                (DQM_HOST2FAP_XTM_XMIT_DEPTH_LOW + DQM_HOST2FAP_XTM_XMIT_DEPTH_HI)
    #define DQM_HOST2FAP_XTM_FREE_RXBUF_BUDGET  32
       
#define DQM_FAP2HOST_XTM_FREE_TXBUF_Q           8
    #define DQM_FAP2HOST_XTM_FREE_TXBUF_Q_SIZE  1
    /* Keep ahead of XTM_NR_TX_BDS to be able to bring iuDMA channels down */
#if defined(CONFIG_BCM_DSL_GINP_RTX) || defined(SUPPORT_DSL_GINP_RTX)
    #define DQM_FAP2HOST_XTM_FREE_TXBUF_DEPTH  405
#else
    #define DQM_FAP2HOST_XTM_FREE_TXBUF_DEPTH  (FAP_XTM_NR_TXBDS + 5)
#endif
    #define DQM_FAP2HOST_XTM_FREE_TXBUF_BUDGET  32
#else
#define DQM_FAP2HOST_XTM_RX_Q_SIZE		0
#define DQM_FAP2HOST_XTM_RX_Q_LOW         	0
#define DQM_FAP2HOST_XTM_RX_DEPTH_LOW           0
#define DQM_FAP2HOST_XTM_RX_Q_HI   	        0
#define DQM_FAP2HOST_XTM_RX_DEPTH_HI            0
#define DQM_HOST2FAP_XTM_XMIT_Q_SIZE            0
#define DQM_HOST2FAP_XTM_XMIT_Q_LOW        	0
#define DQM_HOST2FAP_XTM_XMIT_DEPTH_LOW         0
#define DQM_HOST2FAP_XTM_XMIT_Q_HI              0
#define DQM_HOST2FAP_XTM_XMIT_DEPTH_HI          0
#define DQM_HOST2FAP_XTM_FREE_RXBUF_Q_SIZE      0
#define DQM_HOST2FAP_XTM_FREE_RXBUF_DEPTH       0
#define DQM_FAP2HOST_XTM_FREE_TXBUF_Q_SIZE      0
#define DQM_FAP2HOST_XTM_FREE_TXBUF_DEPTH       0
#endif
       
typedef enum {
	FAP2HOST_ETH_RX_MSGID_ENET=0,
	FAP2HOST_ETH_RX_MSGID_WLAN_TX,  // Normal WLAN Tx (WAN->WLAN, etc) - Routed WLAN acceleration (future)
	FAP2HOST_ETH_RX_MSGID_WLAN_TX_CHAIN,  // Chained WLAN Tx
	FAP2HOST_ETH_RX_MSGID_WLAN_GSO,  // WLAN GSO support
	FAP2HOST_ETH_RX_MSGID_MAX
} fap4ke_fap2HostEthRxContextMsgId;

typedef union {
struct {
        uint32 reserved       : 8;
        uint32 phyPortId      : 4;
        uint32 priority       : 4;
        uint32 offSetSign     : 1;
        uint32 pBufOffset     : 7;
        uint32 wlTxChainIdx   : 8;
	};
	uint32 context;
} fap4ke_fap2HostEthRxContext1;

// Write in steps of 16 bits as FAP will lock up if the address is not 32 bit aligned(word aligned)
#define FILL_CONTEXT1_IN_PKT(Cxt1Address, Cxt1Data)  (*((uint32 *)(Cxt1Address)) = Cxt1Data)

#define GET_CONTEXT1_FROM_PKT(Cxt1Address, Cxt1Data)  (Cxt1Data = *((uint32 *)(Cxt1Address)))

#define ETH_RX_LENGTH_STATUS_MASK 0xFFFFFFF
typedef union {
struct {
        uint32 msgId  :  4;
        uint32 length : 12;
        uint32 status : 16;
    };
    uint32 data;
} fap4ke_fap2HostEthRxParam1;

typedef struct {
    uint32                      pBuf;
    fap4ke_fap2HostEthRxParam1  rxParam1;
    uint32 na1;
    uint32 na2;
} fapDqm_EthRx_t;


#define DQM_FAP2HOST_ETH_RX_Q_SIZE              2

#define DQM_IUDMA_ETH_RX_BUDGET                 100


#define DQM_FAP2HOST_ETH_RX_Q_HI                11
    #define DQM_FAP2HOST_ETH_RX_DEPTH_HI        140 

#define DQM_FAP2HOST_ETH_RX_Q_RSVD1             10

#define DQM_FAP2HOST_ETH_RX_Q_LOW               9
    #define DQM_FAP2HOST_ETH_RX_DEPTH_LOW    \
	   (((FAP_ENET_NR_RXBDS - DQM_FAP2HOST_ETH_RX_DEPTH_HI) + 3) & (~3)) /*round to multiple of 4 */


#define DQM_FAP2HOST_ETH_RX_Q_RSVD2             12

typedef struct {
    uint8   *pBuf;           // 32  word0
    uint8    source;         // 8   word1
    union {
        struct {
            uint8 virtDestPort : 4;
            uint8 destQueue    : 4;
        };
        uint8 tm;            // 8   word1
    };
    uint16   len;            // 16  word1
    uint32   key;            // 32  word2
    uint16   dmaStatus;      // 16  word3
    uint16   param1;         // 16  word3
} fapDqm_EthTx_t;
       
#define DQM_HOST2FAP_ETH_XMIT_Q_SIZE       4
#define DQM_HOST2FAP_ETH_XMIT_BUDGET       80

#define DQM_HOST2FAP_ETH_XMIT_Q_LOW                 13
    #define DQM_HOST2FAP_ETH_XMIT_DEPTH_LOW         100
       

#define DQM_HOST2FAP_ETH_XMIT_Q_RSVD                 14

#define DQM_HOST2FAP_ETH_XMIT_Q_HI                  15
    #define DQM_HOST2FAP_ETH_XMIT_DEPTH_HI          20 

#define DQM_HOST2FAP_ETH_FREE_RXBUF_Q               16
    #define DQM_HOST2FAP_ETH_FREE_RXBUF_Q_SIZE      2
    #define DQM_HOST2FAP_ETH_FREE_RXBUF_DEPTH      \
                (DQM_HOST2FAP_ETH_XMIT_Q_HI + DQM_HOST2FAP_ETH_XMIT_DEPTH_LOW)
    #define DQM_HOST2FAP_ETH_FREE_RXBUF_BUDGET      32
       
#define DQM_FAP2HOST_ETH_FREE_TXBUF_Q               17
    #define DQM_FAP2HOST_ETH_FREE_TXBUF_Q_SIZE      1
    #define DQM_FAP2HOST_ETH_FREE_TXBUF_DEPTH       HOST_ENET_NR_TXBDS
    #define DQM_FAP2HOST_ETH_FREE_TXBUF_BUDGET      32
       

#define DQM_FAP2HOST_HOSTIF_Q                       30
    #define DQM_FAP2HOST_HOSTIF_Q_SIZE              4
    #define DQM_FAP2HOST_HOSTIF_Q_DEPTH             16
    #define DQM_FAP2HOST_HOSTIF_Q_BUDGET            16

#define DQM_HOST2FAP_HOSTIF_Q                       31
    #define DQM_HOST2FAP_HOSTIF_Q_SIZE              4
    #define DQM_HOST2FAP_HOSTIF_Q_DEPTH             100
    #define DQM_HOST2FAP_HOSTIF_Q_BUDGET            100



#define FAP_TOTAL_DQM_SIZE (( \
      ( DQM_FAP2HOST_ETH_RX_Q_SIZE * DQM_FAP2HOST_ETH_RX_DEPTH_LOW ) \
    + ( DQM_FAP2HOST_ETH_RX_Q_SIZE * DQM_FAP2HOST_ETH_RX_DEPTH_HI ) \
    + ( DQM_HOST2FAP_ETH_FREE_RXBUF_Q_SIZE * DQM_HOST2FAP_ETH_FREE_RXBUF_DEPTH ) \
    + ( DQM_HOST2FAP_ETH_XMIT_Q_SIZE * DQM_HOST2FAP_ETH_XMIT_DEPTH_LOW ) \
    + ( DQM_HOST2FAP_ETH_XMIT_Q_SIZE * DQM_HOST2FAP_ETH_XMIT_DEPTH_HI ) \
    + ( DQM_FAP2HOST_ETH_FREE_TXBUF_Q_SIZE * DQM_FAP2HOST_ETH_FREE_TXBUF_DEPTH ) \
    + ( DQM_HOST2FAP_HOSTIF_Q_SIZE * DQM_HOST2FAP_HOSTIF_Q_DEPTH ) \
    + ( DQM_FAP2HOST_HOSTIF_Q_SIZE * DQM_FAP2HOST_HOSTIF_Q_DEPTH ) \
    + ( DQM_FAP2HOST_XTM_RX_Q_SIZE * DQM_FAP2HOST_XTM_RX_DEPTH_LOW ) \
    + ( DQM_FAP2HOST_XTM_RX_Q_SIZE * DQM_FAP2HOST_XTM_RX_DEPTH_HI ) \
    + ( DQM_HOST2FAP_XTM_FREE_RXBUF_Q_SIZE * DQM_HOST2FAP_XTM_FREE_RXBUF_DEPTH ) \
    + ( DQM_HOST2FAP_XTM_XMIT_Q_SIZE * DQM_HOST2FAP_XTM_XMIT_DEPTH_LOW ) \
    + ( DQM_HOST2FAP_XTM_XMIT_Q_SIZE * DQM_HOST2FAP_XTM_XMIT_DEPTH_HI ) \
    + ( DQM_FAP2HOST_XTM_FREE_TXBUF_Q_SIZE * DQM_FAP2HOST_XTM_FREE_TXBUF_DEPTH ) \
    )*4)


#endif /* __FAPDQM_H_INCLUDED__ */
