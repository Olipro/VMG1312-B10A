/************************************************************
 *
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

#ifndef __FAP4KEMSG_H_INCLUDED__
#define __FAP4KEMSG_H_INCLUDED__


/******************************************************************************
* File Name  : fap4ke_msg.h                                                   *
*                                                                             *
* Description: This is the 4ke header file for communication from the         *
*              Host MIPS FapDrv to the 4ke MIPs HostDrv                       *
******************************************************************************/

typedef enum {
  FAP_MSG_FLW_ACTIVATE,
  FAP_MSG_FLW_DEACTIVATE,
  FAP_MSG_FLW_UPDATE,
  FAP_MSG_FLW_RESET_STATS,
  FAP_MSG_FLW_MCAST_ADD_CLIENT,
  FAP_MSG_FLW_MCAST_UPDATE_CLIENT,
  FAP_MSG_FLW_MCAST_DEL_CLIENT,
  FAP_MSG_MCAST_SET_MISS_BEHAVIOR,
#if defined(CONFIG_BCM_FAP_LAYER2)
  FAP_MSG_SET_FLOODING_MASK,
  FAP_MSG_ARL_PRINT,
  FAP_MSG_ARL_ADD,
  FAP_MSG_ARL_REMOVE,
  FAP_MSG_ARL_FLUSH,
#endif
  FAP_MSG_DRV_CTL,
  FAP_MSG_DRV_ENET_INIT,
  FAP_MSG_DRV_XTM_INIT,
  FAP_MSG_DRV_XTM_CREATE_DEVICE,
  FAP_MSG_DRV_XTM_LINK_UP,
  FAP_MSG_DRV_RESET_STATS,
  FAP_MSG_DBG_PRINT_FLOW,
  FAP_MSG_DBG_DUMP_IUDMA,
  FAP_MSG_DBG_DUMP_MEM,
  FAP_MSG_DBG_IRQ_STATS,
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
  FAP_MSG_BPM,
#endif
#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
  FAP_MSG_IQ,
#endif
  FAP_MSG_DBG_STACK,
  FAP_MSG_SET_MTU,
#if defined(CC_FAP4KE_TM)
  FAP_MSG_TM_ENABLE,
  FAP_MSG_TM_PORT_CONFIG,
  FAP_MSG_TM_QUEUE_CONFIG,
  FAP_MSG_TM_ARBITER_CONFIG,
  FAP_MSG_TM_STATS,
  FAP_MSG_TM_MAP_PORT_TO_SCHED,
  FAP_MSG_TM_MAP_TMQUEUE_TO_SWQUEUE,
  FAP_MSG_TM_MAP_DUMP,
#endif
  FAP_MSG_DO_4KE_TEST,
#if defined(CONFIG_BCM_GMAC)
  FAP_MSG_DRV_ENET_UNINIT,
#endif
  FAP_MSG_STATS,
} fapMsgGroups_t;

#define FAPMSG_CMD_RX_ENABLE        0
#define FAPMSG_CMD_RX_DISABLE	    1
#define FAPMSG_CMD_TX_ENABLE	    2
#define FAPMSG_CMD_TX_DISABLE	    3
#define FAPMSG_CMD_INIT_RX          4
#define FAPMSG_CMD_INIT_TX          5
#if (defined(CONFIG_BCM963268) || defined(CONFIG_BCM_6362_PORTS_INT_EXT_SW)) && defined(CONFIG_BCM_EXT_SWITCH)
#define FAPMSG_CMD_INIT_EXTSW       6
#endif
#if defined(CONFIG_BCM_GMAC)
#define FAPMSG_CMD_UNINIT_RX        7
#define FAPMSG_CMD_UNINIT_TX        8
#endif

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
#define FAPMSG_CMD_ALLOC_BUF_RESP       10
#define FAPMSG_CMD_FREE_BUF_RESP        11
#define FAPMSG_CMD_GET_BPM_STATUS       12
#define FAPMSG_CMD_SET_BPM_STATUS       13
#define FAPMSG_CMD_DUMP_BPM_STATUS      14
#define FAPMSG_CMD_GET_TXQ_BPM_THRESH	15
#define FAPMSG_CMD_SET_TXQ_BPM_THRESH	16
#define FAPMSG_CMD_DUMP_TXQ_BPM_THRESH  17
#define FAPMSG_CMD_GET_RX_BPM_THRESH	18
#define FAPMSG_CMD_SET_RX_BPM_THRESH	19
#define FAPMSG_CMD_DUMP_RX_BPM_THRESH   20
#define FAPMSG_CMD_SET_BPM_ETH_TXQ_THRESH   21
#define FAPMSG_CMD_DUMP_BPM_ETH_TXQ_THRESH  22
#endif
#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
#define FAPMSG_CMD_GET_IQ_STATUS        30
#define FAPMSG_CMD_SET_IQ_STATUS        31
#define FAPMSG_CMD_DUMP_IQ_STATUS       32
#define FAPMSG_CMD_GET_IQ_THRESH	    33
#define FAPMSG_CMD_SET_IQ_THRESH	    34
#define FAPMSG_CMD_DUMP_IQ_THRESH       35
#define FAPMSG_CMD_SET_IQ_DQM_THRESH	36
#define FAPMSG_CMD_IQ_ADD_PORT	        37
#define FAPMSG_CMD_IQ_REM_PORT	        38
#define FAPMSG_CMD_IQ_DUMP_PORTTBL	    39
#endif
#define FAPMSG_CMD_DRV_RESET_STATS      40

#define FAPMSG_DRV_ENET             0
#define FAPMSG_DRV_XTM              1

typedef struct {
   uint32    cmd;
   uint32    drv;
   int32     channel;
   uint32    params;   /* only used for xtm tx enable msgs */
} xmit2FapMsgDriverCtl_t;

typedef struct {
   uint32    cmd;
   uint32    drv;
   int32     channel;
   uint32    Bds;
   uint32    Dma;
   uint32    numBds;
} xmit2FapMsgDriverInit_t;

#if (defined(CONFIG_BCM963268) || defined(CONFIG_BCM_6362_PORTS_INT_EXT_SW)) && defined(CONFIG_BCM_EXT_SWITCH)
typedef struct {
   uint32    cmd;
   uint32    extSwConnPort;
} xmit2FapMsgExtSwInit_t;
#endif

typedef struct {
   uint32 flowId;
} xmit2FapMsgFlowCfg_t;

typedef struct {
   uint32 devId;
   uint32 encapType;
   uint32 headerLen;
   uint32 trailerLen;
} xmit2FapMsgXtmCreateDevice_t;

typedef struct {
   uint32 devId;
   uint32 matchId;
} xmit2FapMsgXtmLinkUp_t;

typedef struct {
   uint32 linkUsRate0;
   uint32 linkUsRate1;
   uint32 portMask;
} xmit2FapMsgXtmBondInfo_t;

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
typedef union {
   struct {
       uint32    cmd;
       union {
           struct {
               uint8     drv:4;
               uint8     channel:4;
           };
           uint8 fapIdx; /* used by Free Reqt */
       };
       uint8     seqId;
       uint16    numBufs;
       uint32    unused;
   };
   uint32 word[3];
} xmit2FapMsg_Buf_t;

typedef union {
   struct {
       uint32    cmd;
       uint8     drv:4;
       uint8     channel:4;
       uint8     seqId;
       uint16    thr[3];    /* space for 3 Qs only. Q0=Q1 */
   };
   uint32 word[3];
} xmit2FapMsg_TxDropThr_t;

#endif

typedef union {
   struct {
       uint32    cmd;
       uint8     drv:4;
       uint8     port:4;
       uint8     unused;
       uint16    unused16[3];
   };
   uint32 word[3];
} xmit2FapMsg_DrvStats_t;

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE) || defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
typedef union {
   struct {
       uint32    cmd;
       uint8     drv:4;
       uint8     channel:4;
       uint8     seqId;
       uint16    unused0;
       uint32    status;
   };
   uint32 word[3];
} xmit2FapMsg_Status_t;

typedef union {
   struct {
       uint32    cmd;
       uint8     drv:4;
       uint8     channel:4;
       uint8     proto;
       uint16    unused0;
       uint16    dport;
       uint8     ent;
       uint8     prio;
   };
   uint32 word[3];
} xmit2FapMsg_IqInfo_t;


typedef union {
   struct {
       uint32    cmd;
       uint8     drv:4;
       uint8     channel:4;
       uint8     seqId;
       uint16    unused0;
       uint16    hiThresh;
       uint16    loThresh;
   };
   uint32 word[3];
} xmit2FapMsg_Thresh_t;

typedef union {
   struct {
       uint32    cmd;
       uint8     drv:4;
       uint8     channel:4;
       uint8     seqId;
       uint16    unused0;
       uint16    allocTrig;
       uint16    bulkAlloc;
   };
   uint32 word[3];
} xmit2FapMsg_RxThresh_t;
#endif

typedef union {
   struct {
       int32     flowId;
       uint8     fapIdx;
       uint16    mtu;
   };
   uint32 word[3];
} xmit2FapMsg_Mtu_t;

#if defined(CC_FAP4KE_TM)
typedef union {
    struct {
        union {
            struct {
                uint8 masterEnable;
                uint8 enable;
            };
            struct {
                uint8 arbiterType;
                uint8 arbiterArg;
            };
        };
        uint8 port;
        uint8 schedulerIndex;
        uint8 queue;
        uint8 swQueue;
        uint8 shaperType;
        uint8 weight;
        uint16 tokens;
        uint16 bucketSize;
    };
    uint32 word[3];
} xmit2FapMsg_tm_t;
#endif

#if defined(CONFIG_BCM_FAP_LAYER2)
typedef union {
    struct {
        uint8 unused8;
        struct {
            uint8 drop   : 1;
            uint8 unused : 7;
        };
        uint8 channel;
        uint8 mask;
    };
    uint32 u32;
} fapMsg_Flooding_Mask_t;

typedef union {
    struct {
        const uint8 reserved; /* reserved for message type in FAP2HOST *only* */
        uint8 unused;
        uint8 destChannelMask;
        uint8 nbrOfTags;
        fap4keArl_tableEntryKey_t key;
        fap4keArl_tableEntryInfo_t info;
    };
    uint32 word[4];
} fapMsg_arlEntry_t;
#endif

typedef struct {
    uint32 word[3];
} xmit2FapMsg_generic_t;

typedef union {

   xmit2FapMsgDriverCtl_t drvCtl;

   xmit2FapMsgFlowCfg_t flowCfg;

   /* other message types */
   xmit2FapMsgDriverInit_t drvInit;

   xmit2FapMsgXtmCreateDevice_t xtmCreateDevice;

   xmit2FapMsgXtmLinkUp_t xtmLinkUp;
   
   xmit2FapMsgXtmBondInfo_t xtmBondInfo;

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
   xmit2FapMsg_Buf_t    allocBuf;
   xmit2FapMsg_Buf_t    freeBuf;
   xmit2FapMsg_RxThresh_t rxThresh;
   xmit2FapMsg_TxDropThr_t txDropThr;
#endif
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE) || defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
   xmit2FapMsg_Status_t    status;
   xmit2FapMsg_IqInfo_t    iqinfo;
   xmit2FapMsg_Thresh_t    threshInit;
#endif
#if (defined(CONFIG_BCM963268) || defined(CONFIG_BCM_6362_PORTS_INT_EXT_SW)) && defined(CONFIG_BCM_EXT_SWITCH)
   xmit2FapMsgExtSwInit_t extSwInit;
#endif
   xmit2FapMsg_Mtu_t       mtu;
#if defined(CC_FAP4KE_TM)
   xmit2FapMsg_tm_t        tm;
#endif
#if defined(CONFIG_BCM_FAP_LAYER2)
   fapMsg_Flooding_Mask_t  floodingMask;
   fapMsg_arlEntry_t       arlEntry;
#endif
   xmit2FapMsg_DrvStats_t stats;

   xmit2FapMsg_generic_t generic;
} xmit2FapMsg_t;

extern void fapDrv_Xmit2Fap( uint32 fapIdx, fapMsgGroups_t msgType, xmit2FapMsg_t *pMsg );

typedef enum {
  HOST_MSG_BPM,
  HOST_MSG_IQ,
  HOST_MSG_DBG_BPM_STATS,
  HOST_MSG_FREE_DYN_MEM,
#if defined(CONFIG_BCM_FAP_LAYER2)
  HOST_MSG_ARL_ADD,
  HOST_MSG_ARL_REMOVE
#endif
} hostMsgGroups_t;

#define HOSTMSG_CMD_ALLOC_BUF_REQT  1
#define HOSTMSG_CMD_FREE_BUF_REQT   2
#define HOSTMSG_CMD_FREE_DYN_MEM    3


#define HOSTMSG_DRV_ENET             0
#define HOSTMSG_DRV_XTM              1

typedef struct {
   uint32    cmd;
   union {
        struct {        // HOSTMSG_CMD_ALLOC_BUF_REQT / HOSTMSG_CMD_FREE_BUF_REQT
           union {
               struct {
                   uint8     drv: 4;
                   uint8     channel:4;
               };
               uint8    fapIdx; //used by HOSTMSG_CMD_FREE_BUF_REQT
           };
           uint8     seqId;
           uint16    numBufs;
           uint32    unused;
        };
        struct {        // HOSTMSG_CMD_FREE_DYN_MEM
           uint32    flowId;
        };  
   };
} xmit2HostMsg_Buf_t;

typedef struct {
   uint32    cmd;
   uint32    drv;
   uint32    numBufs;
} xmit2HostMsg_RxBuf_t;

typedef union {
   uint32                   word[4];
   xmit2HostMsg_Buf_t       allocBuf;
   xmit2HostMsg_Buf_t       freeBuf;
#if defined(CONFIG_BCM_FAP_LAYER2)
   fapMsg_arlEntry_t        arlEntry;
#endif
} xmit2HostMsg_t;

#if defined(FAP_4KE)
void fap4ke_Xmit2Host( hostMsgGroups_t msgType, xmit2HostMsg_t *pMsg );
#endif

#endif /* __FAP4KEMSG_H_INCLUDED__ */

