#ifndef __FAP4KE_MEMORY_H_INCLUDED__
#define __FAP4KE_MEMORY_H_INCLUDED__

/*

 Copyright (c) 2007 Broadcom Corporation
 All Rights Reserved

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

/*
 *******************************************************************************
 * File Name  : fap4ke_memory.h
 *
 * Description: This file contains ...
 *
 *******************************************************************************
 */

#include "fap4ke_init.h"
#include "fap4ke_timers.h"
#include "fap4ke_irq.h"
#include "fap_dqm.h"
#include "fap4ke_packet.h"
#include "fap4ke_iopDma.h"
#include "fap4ke_mailBox.h"
#include "bcmPktDma_structs.h"
#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
#include "fap4ke_xtmrt.h"
#endif
#include "fap4ke_gso.h"
#include "fap4ke_tm.h"
#if defined(CONFIG_BCM963268)
#include "bcmxtmrtbond.h"
#endif
#include "fap_dynmem.h"
#include "fap_slob.h"


/***************************************************
 * Debugging Allocations
 ***************************************************/

//#define CC_FAP4KE_PMON

#if defined(CC_FAP4KE_PMON)
/*
 * fap4keDbg_pmon_t: Performance Monitoring variables in PSM
 */
typedef enum {
    FAP4KE_PMON_ID_NOP50 = 0,
    FAP4KE_PMON_ID_REG_WR,
    FAP4KE_PMON_ID_REG_RD,

    FAP4KE_PMON_ID_ENET_RECV,
    FAP4KE_PMON_ID_ENET_DMA_IN,
    FAP4KE_PMON_ID_ENET_CLASSIFY,
    FAP4KE_PMON_ID_ENET_HIT,
    FAP4KE_PMON_ID_ENET_XMIT_PREP,
    FAP4KE_PMON_ID_ENET_RECYCLE,
    FAP4KE_PMON_ID_ENET_XMIT,
    FAP4KE_PMON_ID_ENET_EXIT,

    FAP4KE_PMON_ID_XTM_RX_BEGIN,
    FAP4KE_PMON_ID_XTM_RX_HEADER,
    FAP4KE_PMON_ID_XTM_RX_CLASSIFY,
    FAP4KE_PMON_ID_XTM_RX_XMIT,
    FAP4KE_PMON_ID_XTM_RX_END,

    FAP4KE_PMON_ID_DMA_RX_START,
    FAP4KE_PMON_ID_DMA_RX_FINISH,
    FAP4KE_PMON_ID_DMA_TX_START,
    FAP4KE_PMON_ID_DMA_TX_FINISH,

    FAP4KE_PMON_ID_IUDMA_XMIT,
    FAP4KE_PMON_ID_IUDMA_RECV,
    FAP4KE_PMON_ID_IUDMA_FREEXMITBUFGET,
    FAP4KE_PMON_ID_IUDMA_FREERECVBUF,

    FAP4KE_PMON_ID_ENET_IQ,
    FAP4KE_PMON_ID_XTM_IQ,

    FAP4KE_PMON_ID_ENET_BPM_ALLOC,
    FAP4KE_PMON_ID_ENET_BPM_FREE,
    FAP4KE_PMON_ID_XTM_BPM_ALLOC,
    FAP4KE_PMON_ID_XTM_BPM_FREE,

    FAP4KE_PMON_ID_PKT_PARSE_VLAN,
    FAP4KE_PMON_ID_PKT_PARSE,
    FAP4KE_PMON_ID_PKT_ARL_LKUP,
    FAP4KE_PMON_ID_PKT_ARL_HIT,
    FAP4KE_PMON_ID_PKT_ARL_MISS,
    FAP4KE_PMON_ID_PKT_PARSE_IPv4,
    FAP4KE_PMON_ID_PKT_MCAST_IPv4,
    FAP4KE_PMON_ID_PKT_UCAST_IPv4,
    FAP4KE_PMON_ID_PKT_L2_PARSE,
    FAP4KE_PMON_ID_PKT_L2_HASH,
    FAP4KE_PMON_ID_PKT_L2_MATCH,
    FAP4KE_PMON_ID_PKT_L2_LEARN,
    FAP4KE_PMON_ID_PKT_L2_MOD,
    FAP4KE_PMON_ID_PKT_L2_FWD,
    FAP4KE_PMON_ID_PKT_FLOW_HIT,

    FAP4KE_PMON_ID_MAX
} fap4kePsm_pmonId_t;

#undef FAP_DECL
#define FAP_DECL(x) #x,

#define FAP4KE_PMON_ID_NAME \
    {                                                   \
        FAP_DECL(FAP4KE_PMON_ID_NOP50)                  \
        FAP_DECL(FAP4KE_PMON_ID_REG_WR)                 \
        FAP_DECL(FAP4KE_PMON_ID_REG_RD)                 \
        FAP_DECL(FAP4KE_PMON_ID_ENET_RECV)          \
        FAP_DECL(FAP4KE_PMON_ID_ENET_DMA_IN)          \
        FAP_DECL(FAP4KE_PMON_ID_ENET_CLASSIFY)         \
        FAP_DECL(FAP4KE_PMON_ID_ENET_HIT)       \
        FAP_DECL(FAP4KE_PMON_ID_ENET_XMIT_PREP)           \
        FAP_DECL(FAP4KE_PMON_ID_ENET_RECYCLE)           \
        FAP_DECL(FAP4KE_PMON_ID_ENET_XMIT)           \
        FAP_DECL(FAP4KE_PMON_ID_ENET_EXIT)            \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_BEGIN)           \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_HEADER)          \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_CLASSIFY)        \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_XMIT)            \
        FAP_DECL(FAP4KE_PMON_ID_XTM_RX_END)             \
        FAP_DECL(FAP4KE_PMON_ID_DMA_RX_START)           \
        FAP_DECL(FAP4KE_PMON_ID_DMA_RX_FINISH)          \
        FAP_DECL(FAP4KE_PMON_ID_DMA_TX_START)           \
        FAP_DECL(FAP4KE_PMON_ID_DMA_TX_FINISH)          \
        FAP_DECL(FAP4KE_PMON_ID_IUDMA_XMIT)             \
        FAP_DECL(FAP4KE_PMON_ID_IUDMA_RECV)             \
        FAP_DECL(FAP4KE_PMON_ID_IUDMA_FREEXMITBUFGET)   \
        FAP_DECL(FAP4KE_PMON_ID_IUDMA_FREERECVBUF)      \
        FAP_DECL(FAP4KE_PMON_ID_ENET_IQ) \
        FAP_DECL(FAP4KE_PMON_ID_XTM_IQ) \
        FAP_DECL(FAP4KE_PMON_ID_ENET_BPM_ALLOC) \
        FAP_DECL(FAP4KE_PMON_ID_ENET_BPM_FREE)  \
        FAP_DECL(FAP4KE_PMON_ID_XTM_BPM_ALLOC)  \
        FAP_DECL(FAP4KE_PMON_ID_XTM_BPM_FREE)   \
        FAP_DECL(FAP4KE_PMON_ID_PKT_PARSE_VLAN)  \
        FAP_DECL(FAP4KE_PMON_ID_PKT_PARSE)  \
        FAP_DECL(FAP4KE_PMON_ID_PKT_ARL_LKUP) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_ARL_HIT) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_ARL_MISS) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_PARSE_IPv4) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_MCAST_IPv4) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_UCAST_IPv4) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_L2_PARSE) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_L2_HASH) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_L2_MATCH) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_L2_LEARN) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_L2_MOD) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_L2_FWD) \
        FAP_DECL(FAP4KE_PMON_ID_PKT_FLOW_HIT) \
    }

typedef struct {
    uint32 globalIrqs;
    uint32 halfCycles[FAP4KE_PMON_ID_MAX];
    uint32 instncomplete[FAP4KE_PMON_ID_MAX];
    uint32 icachehit[FAP4KE_PMON_ID_MAX];
    uint32 icachemiss[FAP4KE_PMON_ID_MAX];
    uint32 interrupts[FAP4KE_PMON_ID_MAX];
} fap4keDbg_pmon_t;

#if defined(CC_FAP4KE_PMON) && defined(CONFIG_BCM96362)
#define p4kePmon ( &p4kePsmGbl->pmon )
#else
#define p4kePmon ( &p4keQsmGbl->pmon )
#endif

#endif /* CC_FAP4KE_PMON */

#define ENET_MAX_VPORTS 16  // Max Enet Virtual Ports
#define XTM_MAX_VPORTS  16  // MAX_DEV_CTXS      

/***************************************************
 * 4ke Data Scratch Pad Ram (DSPRAM) Mappings
 ***************************************************/

/* In the 6362 we will always keep the XTM Tx BDs in DDR because the upstream
   bandwidth is fairly small, and we do not have enough space in the PSM to
   allocate the BDs required for 16 channels */
#define ENET_RX_BDS_IN_PSM
#define ENET_TX_BDS_IN_PSM
#define XTM_RX_BDS_IN_PSM
//#define XTM_TX_BDS_IN_PSM


#define DBGVAL p4keSdram->alloc.dbgVals

// Uncomment to enable enet rx polling in the 4ke task loop
//#define ENABLE_ENET_RX_POLLING

//#define FAP4KE_GBL_NULL_CMD_LIST (&(p4kePsmGbl->nullCmdList))

#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE) || defined(CONFIG_BCM96818)
#define FAP_PSM_MANAGED_MEMORY_SIZE    11808
#else
/* NOTE: this is temporary.  Eventually make this dynamic */
#define FAP_PSM_MANAGED_MEMORY_SIZE    (11808-2176)
#endif

#define p4keDspram ( (fap4keDspram_alloc_t *)(DSPRAM_VBASE) )

#define p4keDspramGbl ( (fap4keDspram_global_t *)(&p4keDspram->global) )


/*
 * fap4keDspram_timers_t: all Timers must be declared here
 */
typedef struct {
    /* Timer Management */
    fap4keTmr_Ctrl_t ctrl;

    /* CPU Utilization */
    fap4keTmr_cpuSample_t cpu;

    /* User-defined timers */
    fap4keTmr_timer_t keepAlive;
    fap4keTmr_timer_t flowStatsTimer;
} fap4keDspram_timers_t;

/*
 * fap4keDspram_tasks_t: all Tasks must be declared here
 */
typedef struct {
    /* Task Management */
    fap4keTsk_scheduler_t scheduler;

    /* User-defined Tasks */
    fap4keTsk_task_t enetRecv0;
    fap4keTsk_task_t enetRecv1;
    fap4keTsk_task_t xtmRecv0;
    fap4keTsk_task_t xtmRecv1;
    fap4keTsk_task_t *hostIfRecvTask_p;
} fap4keDspram_tasks_t;

/*
 * fap4keDspram_irq_t: Interrupt Management variables
 */
typedef struct {
    /* Interrupt Management */
    uint32 handlerCount;
    uint32 wait_pc;
    uint32 epc_jump;
    fap4keIrq_handlerInfo_t handlerInfo[FAP4KE_IRQ_HANDLERS_MAX];
} fap4keDspram_irq_t;

/*
 * fap4keDqm_handlerInfo_t: DQM queue handler structure
 */
typedef struct {
   uint32                   mask;
   uint32                   enable;
   fap4keTsk_taskPriority_t taskPriority;
   fap4keTsk_task_t         task;
} fap4keDqm_handlerInfo_t;

/*
 * fap4keDspram_dqm_t: DQM variables
 */
typedef struct{
    /* Queue Handlers */
    uint32 handlerCount;
    fap4keDqm_handlerInfo_t handlerInfo[DQM_MAX_HANDLER];
} fap4keDspram_dqm_t;

/*
 * fap4keDspram_enet_t: ENET variables
 */
typedef struct{
    fap4keTsk_task_t *xmitFromHostTask;
    fap4keTsk_task_t *recvFreeFromHostTask;
    /* Channel Bitmap used to enable/disable Tx cleanup for each iuDMA channel */
    uint32 txCleanupChannelMap;
    int      enetTxChannel;		/* enet iuDMA channel that FAP should use when TX_SPLITTING enabled - Aug 2010 */
    uint32_t enetRxToss;		/* number of times rx pkts dropped so 1 rx iuDMA ch not limit rx on another - Sept 2010 */
#if (defined(CONFIG_BCM963268)|| defined(CONFIG_BCM_6362_PORTS_INT_EXT_SW)) && defined(CONFIG_BCM_EXT_SWITCH)
    int extSwConnPort; /* internal switch port that is connected to external switch - Aug 2011 */
    uint32 extSwConnPortMask; /* Bitmask of internal switch port that is connected to external switch - Aug 2011 */
#endif
} fap4keDspram_enet_t;

/*
 * fap4keDspram_xtm_t: XTM variables
 */
#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
typedef struct{
    fap4keTsk_task_t *xmitFromHostTask;
    fap4keTsk_task_t *recvFreeFromHostTask;
    /* Channel Bitmap used to enable/disable Tx cleanup for each iuDMA channel */
    uint32 txCleanupChannelMap;
    fap4keXtm_devMap_t devMap;
    fap4keXtm_qos_t qos;
    int                xtmTxChannel;		/* xmt iuDMA channel that FAP should use when TX_SPLITTING enabled - Aug 2010 */
} fap4keDspram_xtm_t;
#endif

/*
 * fap4keDspram_packet_t: Packet processing variables
 */
typedef struct {
    fap4kePkt_runtime_t runtime;
} fap4keDspram_packet_t;


#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
/*
 * fap4keDspram_iq_t: Ingress Qos
 */
typedef struct {
    uint32 enable;          /* IQ feature enable */
    uint32 cpu_cong;        /* IQ CPU congestion */
} fap4keDspram_iq_t;
#define p4keDspIq ( &p4keDspramGbl->iq )
#endif



typedef enum {
    W2S_state_enetStopIudmas,
    W2S_state_enetFlushIudmaRings, 
    W2S_state_xtmStopIudmas,
    W2S_state_xtmFlushIudmaRings, 
    W2S_state_flushFreeCache, 
    W2S_state_flushFreeCacheCont, 
    W2S_state_stopFfe, 
} W2S_state_t;


#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
#define FAP_BPM_ENET_BULK_ALLOC_MAX     128
#define FAP_BPM_XTM_BULK_ALLOC_MAX      128

#define FAP_BPM_FREE_TRIG_THRESH        512
#define FAP_BPM_BULK_FREE_COUNT         512
#define FAP_BPM_BULK_FREE_MAX_REQ       2

#define CC_FAP_BPM_EXT_FREE
#if defined(CONFIG_BCM_DSL_GINP_RTX) || defined(SUPPORT_DSL_GINP_RTX)
#define FAP_BPM_EXT_FREE_MAX_REQ        64
#else
#define FAP_BPM_EXT_FREE_MAX_REQ        16
#endif
/*
 * fap4keDspram_bpm_t: BPM info.
 * Note: local free cache is shared by all Eth and XTM channels
 */
typedef struct {
    uint32 enable;              /* BPM feature enable */
    uint32 pend_buf_free;       /* number of buffers pending free */
    uint32 freeReqIdx;          /* the active local free cache index  */
    uint32 *freeCachePtr;       /* the pointer to active local free cache */
    uint32 free;                /* count of number of buffers freed */
#if defined(CC_FAP_BPM_EXT_FREE)
    uint16 extFreeWrIdx;
    uint16 extFreeRdIdx;
#endif
    /* waiting response from BPM for the free requests. */
    uint32 buf_free_wait[FAP_BPM_BULK_FREE_MAX_REQ]; /* 1=wait, 0=no wait */
} fap4keDspram_bpm_t;
#define p4keDspBpm ( &p4keDspramGbl->bpm )
#endif

#if defined(CONFIG_BCM_FAP_LAYER2)
typedef struct {
    fap4keArl_Ctrl_t ctrl;
} fap4keDspram_arl_t;
#define p4keArlCtrl ( &p4keDspramGbl->arl.ctrl )
#endif

/*
 * fap4keDspram_global_t: contains all global variables stored in DSPRAM
 */
typedef struct {
    uint32 scribble0;
    /* Timers */
    fap4keDspram_timers_t timers;
    /* Tasks */
    fap4keDspram_tasks_t tasks;
    /* Interrupts */
    fap4keDspram_irq_t irq;
    /* DQM */
    fap4keDspram_dqm_t dqm;
    /* ENET */
    fap4keDspram_enet_t enet;
#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
    /* XTM */
    fap4keDspram_xtm_t xtm;
#endif
    /* Packet */
    fap4keDspram_packet_t packet;
    /* Eth Iudma */
    BcmPktDma_LocalEthRxDma EthRxDma[ENET_RX_CHANNELS_MAX];
    BcmPktDma_LocalEthTxDma EthTxDma[ENET_TX_CHANNELS_MAX];

#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
    fap4keDspram_iq_t  iq;
#endif
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    fap4keDspram_bpm_t  bpm;
#endif
#if defined(CONFIG_BCM_FAP_LAYER2)
    fap4keDspram_arl_t arl;
#endif
    Mchdr_t *mchdrFreeList;

//#if (defined(CC_FAP4KE_TM) && defined(CONFIG_BCM96362))
#if defined(CC_FAP4KE_TM)
    fap4keTm_ctrl_t tmCtrl;
#endif

    uint8  fapGsoEnabled;
    uint8  useIudma2Bd;
    uint8  repLowStack;
    uint32 scribble1;
} fap4keDspram_global_t;

/*
 * fap4keDspram_alloc_t: used to manage the overall 4ke DSPRAM allocation
 */
typedef struct {
    /* 4ke stack: Never write to this area!!! */
    volatile const uint8 stack4ke[FAP_INIT_4KE_STACK_SIZE];

    union {
        volatile const uint8 global_u8[DSPRAM_SIZE - FAP_INIT_4KE_STACK_SIZE];

        fap4keDspram_global_t global;
    };
} fap4keDspram_alloc_t;


/***************************************************
 * SDRAM Mappings
 ***************************************************/

/*
 * We need to avoid global allocations in SDRAM to be cached by the Host MIPS,
 * by aligning all SDRAM allocations to a Host D$ line size, and making the
 * allocations to be an integer multiple of the Host D$ line size
 */

typedef struct {
    fap4keTmr_timer_t testTimer;
} fap4keSdram_main_t;

typedef struct {
    uint16 printCount4ke;
    uint16 keepAliveCount4ke;
} fap4keSdram_mailBox_t;

typedef struct {
    /* used in QSM memory management */
    uint32 availableMemory;
    uint32 nextAddress;         /* next available address (byte address) */

    /* test code */
    fap4keTmr_timer_t dqmTestTimer;
    uint32 inc;
} fap4keSdram_dqm_t;

typedef struct {
    fap4kePkt_flowInfo_t flowInfoPool[FAP4KE_PKT_MAX_FLOWS];
    fap4kePkt_flowStats_t stats[FAP4KE_PKT_MAX_FLOWS];
    fap4kePkt_learn_t learn;
} fap4keSdram_packet_t;

typedef struct {
    uint8 *gsoBuffer;
#if defined(CC_FAP4KE_TM)
    fap4keTm_sdram_t *tmSdram_p;
#endif
} fap4keSdram_initParams_t;

#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
 
typedef struct {
    uint32 dbg;        /* IQ debug flag */
} fap4keSdram_iq_t;
#endif /* (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE)) */

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
typedef struct {
    volatile int enabled;
    uint16 reqt;
    uint16 resp;
    /* storage for buffer address requesed for Eth RX channel */
    uint32 bufAddr[FAP_BPM_ENET_BULK_ALLOC_MAX];
} fap4keSdram_enetBpm_t;

#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
typedef struct {
    uint16 reqt;
    uint16 resp;

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    /* storage for buffer address requesed for XTM RX channel */
    uint32 bufAddr[FAP_BPM_XTM_BULK_ALLOC_MAX];
#endif
} fap4keSdram_xtmBpm_t;
#endif

typedef struct {
    uint32 dbg;        /* BPM debug flag */
    fap4keSdram_enetBpm_t enet[ENET_RX_CHANNELS_MAX];
#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
    fap4keSdram_xtmBpm_t  xtm[XTM_RX_CHANNELS_MAX];
#endif
#if defined(CC_FAP_BPM_EXT_FREE)
    uint32 extFreeCache[FAP_BPM_EXT_FREE_MAX_REQ][FAP_BPM_BULK_FREE_COUNT];
#endif
} fap4keSdram_bpm_t;
#endif /* (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE)) */

typedef struct
{
    fapDm_BlockId cmdListBids[FAP4KE_PKT_MAX_FLOWS];
    fapslob_Region regions[FAP_DM_REGION_MAX];
    fapDm_RegionIdx regionOrders[FAP_DM_REGION_ORDER_MAX][FAP_DM_REGION_MAX+1];
    fapDm_BlockId flows[FAP4KE_PKT_MAX_FLOWS];
} fap4keSdram_dyn_mem_t;

typedef struct {
#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
    /* storage for buffer address requesed for XTM RX channel */
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    uint32 bufAddr[FAP_BPM_XTM_BULK_ALLOC_MAX];
#endif
#endif
} fap4keSdram__t;

#if defined(CC_FAP4KE_TM)
typedef struct {
    fap4keTm_rateStats_t rateStats;
} fap4keSdram_tm_t;
#endif

typedef struct {
        uint8 resv1[16];
        uint32 dbgVals[10];
        fap4keSdram_mailBox_t mailBox;
        fap4keSdram_main_t main;
        fap4keSdram_dqm_t dqm;
        fap4keSdram_packet_t packet;
        uint8 localPrintBuf[FAP_MAILBOX_PRINTBUF_SIZE];
        volatile fap4keSdram_initParams_t initParams;
        W2S_state_t w2s_state;
#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
        fap4keSdram_iq_t iq;
#endif
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
        fap4keSdram_bpm_t bpm;
#endif
#if defined(CC_FAP4KE_TM)
        fap4keSdram_tm_t tm;
#endif

        fap4keSdram_dyn_mem_t dynMem;
        McCfglog_t mcCfglogpool[FAP_MAX_MCCFGLOGS];
        Mchdr_t mchdrPool[FAP_MAX_MCHDRS];
        uint8 resv2[16];
} __attribute__((aligned(16))) fap4keSdram_alloc_t;

#ifdef FAP_4KE

/* declared in fap4ke_main.c */
extern fap4keSdram_alloc_t fap4keSdram_g;
#define p4keSdram ( (fap4keSdram_alloc_t *)(mCacheToNonCacheVirtAddr(&fap4keSdram_g)) )

#define CONVERT_DDR_FAP2HOST(_p4keSdramField)                           \
    ( (typeof(_p4keSdramField))((uint32)(p4kePsmGbl->pHostSdram_p) +    \
                                ((uint32)(_p4keSdramField) - (uint32)(p4keSdram))) )

#endif

/***************************************************
 * Packet Shared Memory (PSM) Mappings
 ***************************************************/
#define p4kePsm ( (fap4kePsm_alloc_t *)(FAP_4KE_PSM_BASE) )
#define p4kePsmGbl ( &p4kePsm->global )

/* In the 63268, the PSM has a different base address for each port.
   This is the PSM address of the port connected to the IOP */
#define p4kePsmIop ( (fap4kePsm_alloc_t *)(FAP_4KE_PSM_BASE_IOP) )
#define p4kePsmIopGbl ( &p4kePsmIop->global )

/*
 * fap4kePsm_packet_t: Packet Manager Variables, including flow definitions
 */
typedef struct {
    uint8 headerPool[FAP4KE_PKT_MAX_HEADERS][FAP4KE_PKT_HEADER_SIZE_MAX];
} fap4kePsm_packet_t;

/*
 * fap4kePsm_timers_t: Timers variables in PSM
 */
typedef struct {
    fap4keTmr_cpuHistory_t cpu;
} fap4kePsm_timers_t;

//#define CC_FAP4KE_TRACE

#if defined(CC_FAP4KE_TRACE)
#define FAP4KE_TRACE_HISTORY_SIZE 300

#undef FAP4KE_DECL
#define FAP4KE_DECL(x) #x,

#define FAP4KE_TRACE_TYPE_NAME       \
    {                                \
        FAP4KE_DECL(RX_BEGIN)        \
        FAP4KE_DECL(RX_PACKET)       \
        FAP4KE_DECL(RX_END)          \
        FAP4KE_DECL(TX_BEGIN)        \
        FAP4KE_DECL(TX_FREE)         \
        FAP4KE_DECL(TX_PACKET)       \
        FAP4KE_DECL(TX_END)          \
        FAP4KE_DECL(IRQ_BEGIN)       \
        FAP4KE_DECL(IRQ_CALL_START)  \
        FAP4KE_DECL(IRQ_CALL_END)    \
        FAP4KE_DECL(IRQ_END)         \
        FAP4KE_DECL(TASK)            \
        FAP4KE_DECL(WAIT_START)      \
        FAP4KE_DECL(WAIT_END)        \
        FAP4KE_DECL(FAP4KE_TRACE_ALLOC_BUF_BEGIN)   \
        FAP4KE_DECL(FAP4KE_TRACE_ALLOC_BUF_END)     \
        FAP4KE_DECL(FAP4KE_TRACE_FREE_BUF_BEGIN)    \
        FAP4KE_DECL(FAP4KE_TRACE_FREE_BUF_END)      \
    }

typedef enum {
    FAP4KE_TRACE_RX_BEGIN,
    FAP4KE_TRACE_RX_PACKET,
    FAP4KE_TRACE_RX_END,

    FAP4KE_TRACE_TX_BEGIN,
    FAP4KE_TRACE_TX_FREE,
    FAP4KE_TRACE_TX_PACKET,
    FAP4KE_TRACE_TX_END,

    FAP4KE_TRACE_IRQ_BEGIN,
    FAP4KE_TRACE_IRQ_CALL_START,
    FAP4KE_TRACE_IRQ_CALL_END,
    FAP4KE_TRACE_IRQ_END,

    FAP4KE_TRACE_TASK,
    FAP4KE_TRACE_WAIT_START,
    FAP4KE_TRACE_WAIT_END,

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    FAP4KE_TRACE_ALLOC_BUF_BEGIN,
    FAP4KE_TRACE_ALLOC_BUF_END,
    FAP4KE_TRACE_FREE_BUF_BEGIN,
    FAP4KE_TRACE_FREE_BUF_END,
#endif
    FAP4KE_TRACE_MAX
} fap4keTrace_id_t;

typedef enum {
    FAP4KE_TRACE_TYPE_DEC,
    FAP4KE_TRACE_TYPE_HEX,
    FAP4KE_TRACE_TYPE_STR,
    FAP4KE_TRACE_TYPE_MAX
} fap4keTrace_type_t;

typedef struct {
    fap4keTrace_id_t id;
    uint32_t cycles;
    uint32_t arg;
    fap4keTrace_type_t type;
} fap4keTrace_record_t;

typedef struct {
    uint32_t write;
    uint32_t count;
    fap4keTrace_record_t record[FAP4KE_TRACE_HISTORY_SIZE];
} fap4keTrace_history_t;

/*
 * fap4kePsm_trace_t: 4ke Trace variables in PSM
 */
typedef struct {
    uint32_t enable;
    fap4keTrace_history_t history;
} fap4kePsm_trace_t;

#define p4keTrace ( &p4kePsmGbl->trace )
#endif /* CC_FAP4KE_TRACE */


#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
typedef struct {
    uint32 buf_thresh_lvl;  /* the current buffer level in global BPM */
    uint16 reqt;              
    uint16 resp;             

#if defined(CC_FAP_BPM_EXT_FREE)
    uint32 extRdInProg;
#endif

    /*
     * The local free cache is a temporary storage (cache) of buffer addresses.
     * When the addresses are recycled and there is no space in the RX ring, the
     * excess buffers are stored in the local free cache.
     * There are FAP_BPM_BULK_FREE_MAX_REQ number of local free caches, each
     * of size FAP_BPM_BULK_FREE_COUNT words.
     */
    uint32 freeCache[FAP_BPM_BULK_FREE_MAX_REQ][FAP_BPM_BULK_FREE_COUNT];
} fap4kePsm_bpm_t;
#define p4kePsmBpm ( &p4kePsmGbl->bpm )
#endif

#if defined(CONFIG_BCM_GMAC)
typedef struct {
#define FAP_GMAC_ACTIVE     0x01    /* GMAC is enabled & active right now */
#define FAP_CHAN_ENABLED    0x02    /* channel is enabled */
    uint32 rxFlags[ENET_RX_CHANNELS_MAX];
    uint32 txFlags[ENET_TX_CHANNELS_MAX];
} fap4kePsm_gmac_t;
#define p4kePsmGmac ( &p4kePsmGbl->gmac )
#endif

/*
 * fap4keGso_alloc_t: GSO variables
 */
#if defined(CC_FAP4KE_PKT_GSO)
typedef struct {
    fap4keGso_runtime_t runtime;
    fap4keGso_shared_t shared;
} fap4keGso_alloc_t;

#if defined(CONFIG_BCM96362)
#define p4keGso ( &p4kePsmGbl->gso )
#else
#define p4keGso ( &p4keQsmGbl->gso )
#endif
#endif /* CC_FAP4KE_PKT_GSO */

/*
 * fap4kePsm_global_t: contains all global variables stored in the PSM
 */

typedef struct {
    fap4kePsm_timers_t timers;
    uint32 scribble0;
#if defined(CC_FAP4KE_PMON) && defined(CONFIG_BCM96362)
    fap4keDbg_pmon_t pmon;
#endif
    uint32 scribble1;

#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
    /* Xtm Iudma */
    BcmPktDma_LocalXtmRxDma XtmRxDma[XTM_RX_CHANNELS_MAX];
    BcmPktDma_LocalXtmTxDma XtmTxDma[XTM_TX_CHANNELS_MAX];

    /* Global flag to coordinate XTM tx disable between 4ke and Host - May 2010 */
    uint8 XtmTxDownFlags[XTM_TX_CHANNELS_MAX];
#endif

    uint32 scribble2;

    /* ManagedMemory replaces TxKeys, txSources, txAddresses, enet & xtm rx/tx BDs - Apr 2010 */
    uint8   ManagedMemory[FAP_PSM_MANAGED_MEMORY_SIZE];
    uint32 scribble3;
    uint8 * pManagedMemory;
    uint32 scribble4;

    /*multicast log buffers */
    Mclog_t mclogPool[FAP_MAX_MCLOGS];
    /*data buffer refcounters */
    int8 dataBufRefCount[FAP_MAX_REFCOUNT_TBL_SIZE];
    uint32 dataRefcntIndex;

#if defined(CC_FAP4KE_TRACE)
    fap4kePsm_trace_t trace;
#endif

    int blockHalt;

#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    fap4kePsm_bpm_t bpm;
#endif
    fap4kePsm_packet_t  packet;
    uint16              mtuOverride;    
#if defined(CONFIG_BCM963268)
    XtmRtPtmBondInfo   ptmBondInfo;
    XtmRtPtmTxBondHeader   ptmBondHdr [XTMRT_PTM_BOND_MAX_FRAG_PER_PKT] ;
#endif    
    fap4keSdram_alloc_t *pHostSdram_p;

#if defined(CC_FAP4KE_TM)
    fap4keTm_storage_t tmStorage;
#endif

#if defined(CONFIG_BCM_GMAC)
    fap4kePsm_gmac_t    gmac;
#endif

#if defined(CC_FAP4KE_PKT_GSO) && defined(CONFIG_BCM96362)
    fap4keGso_alloc_t gso;
#endif

} fap4kePsm_global_t;

typedef union {
    uint8 u8[FAP_PSM_SIZE];
    uint32 u32[FAP_PSM_SIZE_32];

    fap4kePsm_global_t global;
} fap4kePsm_alloc_t;


/***************************************************
 * Queue Shared Memory (QSM) Mappings
 ***************************************************/
#define p4keQsm ( (fap4keQsm_alloc_t *)(FAP_4KE_QSM_BASE) )
#define p4keQsmGbl ( &p4keQsm->global )

/*
 * fap4keQsm_packet_t: Packet Manager Variables, including flow definitions
 */

#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
typedef struct {
    uint16 lo;
    uint16 hi;
    uint32 dropped;
} fap4ke_iq_thresh_t;

typedef struct {
    uint16_t port;          /* dest port */
    uint8_t  ent    :1;     /* static entry */
    uint8_t  unused :4;     /* unused */
    uint8_t  prio   :3;     /* prio */
    uint8_t  nextIx;        /* overflow bucket index */
} fap4ke_iq_hent_t;

typedef struct {
    uint8_t count;
    uint8_t nextIx;
} fap4ke_free_ovfl_ent_t;

#define FAP_IQ_HASHTBL_SIZE             64
#define FAP_IQ_OVFLTBL_SIZE             64

typedef enum {
    FAP_IQ_L4PROTO_TCP,
    FAP_IQ_L4PROTO_UDP,
    FAP_IQ_L4PROTO_MAX
} fap4ke_iq_L4proto_t;

typedef struct {
    fap4ke_iq_thresh_t enetDqmThresh[ENET_RX_CHANNELS_MAX];
#if defined(CONFIG_BCM_XTMCFG) || defined(CONFIG_BCM_XTMCFG_MODULE)
    fap4ke_iq_thresh_t xtmDqmThresh[XTM_RX_CHANNELS_MAX];
#endif

     /* Main Hash Table(s): for UDP and TCP */
    fap4ke_iq_hent_t  htbl[FAP_IQ_L4PROTO_MAX][FAP_IQ_HASHTBL_SIZE];

    /* Overflow Table(s): for UDP and TCP */
    fap4ke_iq_hent_t  ovfl_tbl[FAP_IQ_L4PROTO_MAX][FAP_IQ_OVFLTBL_SIZE];

    /* Free Overflow Entry List(s): for UDP and TCP */
    fap4ke_free_ovfl_ent_t free_ovfl_list[FAP_IQ_L4PROTO_MAX];
} fap4keQsm_iq_t;
#endif

typedef struct {
    uint32 fap2HostHostIf;
    uint32 host2FapHostIf;
} fap4keQsm_dqmStats_t;

typedef struct {
/* Stats for 4ke enet rx (iuDMA interrupt) */
    uint32 rxCount;             /* snapshot */
    uint32 rxHighWm;            /* peak value */
    int32 txCount;
    uint32 rxTotal;
    uint32 rxDropped;
    uint32 rxNoBd;
    uint32 rxAssignedBdsMin;    /* peak value */
    uint32 txFreeBdsMin;        /* peak value */
/* Stats for Host enet rx (Q7) */
    uint32 Q7budget;            /* snapshot */
    uint32 Q7rxCount;           /* snapshot */
    uint32 Q7rxHighWm;          /* peak value */
    uint32 Q7rxTotal;
/* Stats for 4ke enet rx free (Q12) */
    uint32 Q12rxCount;          /* snapshot */
    uint32 Q12rxHighWm;         /* peak value */
    uint32 Q12rxTotal;
/* Stats for 4ke enet tx (Q11) */
    uint32 Q11txCount;          /* snapshot */
    uint32 Q11txHighWm;         /* peak value */
    uint32 Q11txTotal;
/* Stats for Host enet tx free (Q13) */
    uint32 Q13txCount;          /* snapshot */
    uint32 Q13txHighWm;         /* peak value */
    uint32 Q13txTotal;
/* Stats for FAP 2 HOST packets */
    uint32 rxDroppedDqmLow;
    uint32 rxDroppedDqmHigh;
    uint32 rxDroppedIq;
} fap4keQsm_stats_t;

#define p4keEnetStats ( &p4keQsm->global.enetStats )
#define p4keXtmStats ( &p4keQsm->global.xtmStats )

typedef struct {
    uint32  rxDropped;
    uint32  txDropped;
} fap4keQsm_vportStats_t;

typedef struct {
    uint32 scribble0;
#if (defined(CONFIG_BCM_INGQOS) || defined(CONFIG_BCM_INGQOS_MODULE))
    fap4keQsm_iq_t iq;
#endif
    fap4keQsm_dqmStats_t dqmStats;

#if defined(CC_FAP4KE_PMON) && !defined(CONFIG_BCM96362)
    fap4keDbg_pmon_t pmon;
#endif
    uint8 dbgEnabled[FAP_MAILBOX_MSGID_MAX];

    fap4keQsm_stats_t enetStats;
    fap4keQsm_stats_t xtmStats;
    fap4keQsm_vportStats_t enet[ENET_MAX_VPORTS];
    fap4keQsm_vportStats_t xtm[XTM_MAX_VPORTS];

#if defined(CC_FAP4KE_PKT_GSO) && !defined(CONFIG_BCM96362)
    fap4keGso_alloc_t gso;
#endif

    uint32 scribble1;
} fap4keQsm_global_t;

typedef union {
    uint8 u8[FAP_QSM_SIZE];
    uint32 u32[FAP_QSM_SIZE / 4];
    fap4keQsm_global_t global;
} fap4keQsm_alloc_t;

/***************************************************
 * Miscellaneous
 ***************************************************/
#define g_dataBufRefCount p4kePsm->global.dataBufRefCount 
#define g_dataRefcntIndex p4kePsm->global.dataRefcntIndex 
#define g_mcHdrFreeList p4keDspramGbl->mchdrFreeList
#define g_mcCfglogPool p4keSdram->alloc.mcCfglogspool

#define FAP4KE_PSM_SCRIBBLE_0 sizeof(fap4kePsm_global_t)
#define FAP4KE_PSM_SCRIBBLE_1 0x12345678
#define FAP4KE_PSM_SCRIBBLE_2 0x12345678
#define FAP4KE_PSM_SCRIBBLE_3 0x12345678
#define FAP4KE_PSM_SCRIBBLE_4 0x12345678


#define FAP4KE_DSP_SCRIBBLE_0 sizeof(fap4keDspram_global_t)
#define FAP4KE_DSP_SCRIBBLE_1 0x12345678


#define FAP4KE_QSM_SCRIBBLE_0 sizeof(fap4keQsm_global_t)
#define FAP4KE_QSM_SCRIBBLE_1 0x12345678


fapRet fap4keHw_dspramCheck(int isFirstTime);
fapRet fap4keHw_psmCheck(int isFirstTime);
fapRet fap4keHw_qsmCheck(int isFirstTime);


#endif  /* defined(__FAP4KE_MEMORY_H_INCLUDED__) */
