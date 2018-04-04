#ifndef __PKTDMA_HOOKS_H_INCLUDED__
#define __PKTDMA_HOOKS_H_INCLUDED__

/*
<:copyright-BRCM:2009:DUAL/GPL:standard

   Copyright (c) 2009 Broadcom Corporation
   All Rights Reserved

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
 * File Name  : bcmPktDmaHooks.h
 *
 * Description: This file contains the Packet DMA Host Hook macros and definitions.
 *              It may be included in Kernel space only.
 *
 *******************************************************************************
 */

#include "fap4ke_timers.h"
#include "fap4ke_packet.h"
#include "fap4ke_msg.h"
#include "fap_hw.h"
#include "fap_tm.h"

//#define CC_PKTDMA_HOOKS_DEBUG

/* Hook for: fapDrv_Xmit2Fap */
#define bcmPktDma_xmit2Fap(fapIdx, _msgType, _pMsg)                     \
    ({                                                                  \
        int __ret;                                                      \
        if(bcmPktDma_hostHooks_g.xmit2Fap != NULL) {                    \
            bcmPktDma_hostHooks_g.xmit2Fap(fapIdx, _msgType, _pMsg);    \
            __ret = FAP_SUCCESS;                                        \
        } else {                                                        \
            __ret = FAP_ERROR;                                          \
        }                                                               \
        __ret;                                                          \
    })

/* Hook for: fapDrv_psmAlloc */
/* FAP PSM Memory Allocation added Apr 2010 */
#define bcmPktDma_psmAlloc(fapIdx, _size)                                       \
    ({                                                                  \
        uint8 * __ret;                                                  \
        if(bcmPktDma_hostHooks_g.psmAlloc != NULL)                      \
            __ret = bcmPktDma_hostHooks_g.psmAlloc(fapIdx, _size);      \
        else                                                            \
            __ret = FAP4KE_OUT_OF_PSM;                                  \
        __ret;                                                          \
    })

/* Hook for: dqmXmitMsgHost */
#if defined(CC_PKTDMA_HOOKS_DEBUG)
#define bcmPktDma_dqmXmitMsgHost(fapIdx, _queue, _tokenSize, _t)        \
    ({                                                                  \
        if(bcmPktDma_hostHooks_g.dqmXmitMsgHost != NULL)                \
          bcmPktDma_hostHooks_g.dqmXmitMsgHost(fapIdx, _queue, _tokenSize, _t); \
    })
#else
#define bcmPktDma_dqmXmitMsgHost(fapIdx, _queue, _tokenSize, _t)        \
    bcmPktDma_hostHooks_g.dqmXmitMsgHost(fapIdx, _queue, _tokenSize, _t)
#endif

/* Hook for: dqmRecvMsgHost */
#if defined(CC_PKTDMA_HOOKS_DEBUG)
#define bcmPktDma_dqmRecvMsgHost(fapIdx, _queue, _tokenSize, _t)        \
    ({                                                                  \
        if(bcmPktDma_hostHooks_g.dqmRecvMsgHost != NULL)                \
          bcmPktDma_hostHooks_g.dqmRecvMsgHost(fapIdx, _queue, _tokenSize, _t); \
    })
#else
#define bcmPktDma_dqmRecvMsgHost(fapIdx, _queue, _tokenSize, _t)        \
    bcmPktDma_hostHooks_g.dqmRecvMsgHost(fapIdx, _queue, _tokenSize, _t)
#endif

/* Hook for: dqmXmitAvailableHost */
#if defined(CC_PKTDMA_HOOKS_DEBUG)
#define bcmPktDma_isDqmXmitAvailableHost(fapIdx, _queue)                \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.isDqmXmitAvailableHost != NULL)        \
            __ret = bcmPktDma_hostHooks_g.isDqmXmitAvailableHost(fapIdx, _queue); \
        __ret;                                                          \
    })
#else
#define bcmPktDma_isDqmXmitAvailableHost(fapIdx, _queue)                \
    bcmPktDma_hostHooks_g.isDqmXmitAvailableHost(fapIdx, _queue)
#endif

/* Hook for: dqmRecvAvailableHost */
#if defined(CC_PKTDMA_HOOKS_DEBUG)
#define bcmPktDma_isDqmRecvAvailableHost(fapIdx, _queue)                \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.isDqmRecvAvailableHost != NULL)        \
            __ret = bcmPktDma_hostHooks_g.isDqmRecvAvailableHost(fapIdx, _queue); \
        __ret;                                                          \
    })
#else
#define bcmPktDma_isDqmRecvAvailableHost(fapIdx, _queue)                \
    bcmPktDma_hostHooks_g.isDqmRecvAvailableHost(fapIdx, _queue)
#endif

/* Hooks for: dqmHandlerEnableHost, dqmHandlerDisableHost */
#define __bcmPktDma_dqmHandlerEnableHost(_mask, _enable)                \
    ({                                                                  \
        int __ret;                                                      \
        if(bcmPktDma_hostHooks_g.dqmEnableHost != NULL)                 \
            __ret = bcmPktDma_hostHooks_g.dqmEnableHost(_mask, _enable); \
        else                                                            \
            __ret = FAP_ERROR;                                          \
        __ret;                                                          \
    })
#define bcmPktDma_dqmHandlerEnableHost(_mask)  __bcmPktDma_dqmHandlerEnableHost(_mask, TRUE)
#define bcmPktDma_dqmHandlerDisableHost(_mask) __bcmPktDma_dqmHandlerEnableHost(_mask, FALSE)

#if defined(CC_FAP4KE_TM)
#define bcmPktDma_tmMasterConfig(_enable)                               \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmMasterConfig != NULL)                \
            __ret = bcmPktDma_hostHooks_g.tmMasterConfig(_enable);      \
        __ret;                                                          \
    })

#define bcmPktDma_tmPortConfig(_port, _mode, _kbps, _mbs, _shapingType) \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmPortConfig != NULL)                  \
            __ret = bcmPktDma_hostHooks_g.tmPortConfig(_port, _mode, _kbps, _mbs, _shapingType); \
        __ret;                                                          \
    })

#define bcmPktDma_tmPortMode(_port, _mode)                              \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmPortMode != NULL)                    \
            __ret = bcmPktDma_hostHooks_g.tmPortMode(_port, _mode);     \
        __ret;                                                          \
    })

#define bcmPktDma_tmGetPortMode(_port)                                  \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmGetPortMode != NULL)                 \
            __ret = bcmPktDma_hostHooks_g.tmGetPortMode(_port);         \
        __ret;                                                          \
    })

#define bcmPktDma_tmIsPortEnabled(_port)                                \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmIsPortEnabled != NULL)               \
            __ret = bcmPktDma_hostHooks_g.tmIsPortEnabled(_port);       \
        __ret;                                                          \
    })

#define bcmPktDma_tmPortType(_port, _portType)                          \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmPortType != NULL)                    \
            __ret = bcmPktDma_hostHooks_g.tmPortType(_port, _portType); \
        __ret;                                                          \
    })
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
#define bcmPktDma_tmPortEnable(_port, _mode, _enable)                   \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmPortEnable != NULL)                  \
            __ret = bcmPktDma_hostHooks_g.tmPortEnable(_port, _mode, _enable); \
        __ret;                                                          \
    })
#define bcmPktDma_tmApply(_port)                                        \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmApply != NULL)                       \
            __ret = bcmPktDma_hostHooks_g.tmApply(_port);               \
        __ret;                                                          \
    })
#else
#define bcmPktDma_tmApply(_port, _enable)                               \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmApply != NULL)                       \
            __ret = bcmPktDma_hostHooks_g.tmApply(_port, _enable);      \
        __ret;                                                          \
    })
#endif
#define bcmPktDma_tmPortApply(_port, _enable)                           \
    ({                                                                  \
        int __ret = 0;                                                  \
        if(bcmPktDma_hostHooks_g.tmPortApply != NULL)                   \
            __ret = bcmPktDma_hostHooks_g.tmPortApply(_port, _enable);  \
        __ret;                                                          \
    })
#endif /* CC_FAP4KE_TM */

#if defined(CONFIG_BCM_FAP_LAYER2) && defined(CONFIG_11ac_throughput_patch_from_412L08)
/*
 * bcmPktDma_arlNotify
 *    Context     : Called in *Interrupt* context, by the FAP interrupt
 *                  handler. User may want to defer work to a Tasklet.
 *    _op         : HOST_MSG_ARL_ADD or HOST_MSG_ARL_REMOVE only
 *    _arlEntry_p : Pointer to fapMsg_Arl_t, containing the ARL entry
 */
#define bcmPktDma_arlNotify(_op, _arlEntry_p)                           \
    do {                                                                \
        if(bcmPktDma_arlNotifyHandlerFuncP_g != NULL)                   \
            (bcmPktDma_arlNotifyHandlerFuncP_g(_op, _arlEntry_p));      \
    } while(0)
#else
#define bcmPktDma_arlNotify(_op, _arlEntry_p)
#endif /* CONFIG_BCM_FAP_LAYER2 */

/* The following are MASKS which describe why the FAP was asked to go to sleep.
   The fap will not wake up unless all triggers are cleared */
#define FAP_SLEEP_TRIGGER_UNPLUGGED_ETH     0x1


/* FAP Driver Hooks */
typedef struct {
    void  (* xmit2Fap)(uint32 fapIdx, fapMsgGroups_t msgType, xmit2FapMsg_t *pMsg);
    uint8 * (* psmAlloc)(uint32 fapIdx, int size);
    void (* dqmXmitMsgHost)(uint32 fapIdx, uint32 queue, uint32 tokenSize, DQMQueueDataReg_S *t);
    void (* dqmRecvMsgHost)(uint32 fapIdx, uint32 queue, uint32 tokenSize, DQMQueueDataReg_S *t);
    int  (* isDqmXmitAvailableHost)(uint32 fapIdx, uint32 queue);
    int  (* isDqmRecvAvailableHost)(uint32 fapIdx, uint32 queue);
    int  (* dqmEnableHost)(uint32 mask, bool enable);
#if defined(CC_FAP4KE_TM)
    void (*tmMasterConfig)(int enable);
    int (*tmPortConfig)(uint8 port, fapTm_mode_t mode, int kbps, int mbs, fapTm_shapingType_t shapingType);
    int (*tmSetPortMode)(uint8 port, fapTm_mode_t mode);
    fapTm_mode_t (*tmGetPortMode)(uint8 port);
#if defined(CONFIG_11ac_throughput_patch_from_412L08)
    int (*tmPortType)(uint8 port, fapTm_portType_t portType);
    int (*tmPortEnable)(uint8 port, fapTm_mode_t mode, int enable);
    int (*tmApply)(uint8 port);
#else
    int (*tmIsPortEnabled)(uint8 port);
    int (*tmPortType)(uint8 port, fapTm_portType_t portType);
    int (*tmApply)(uint8 port, int enable);
#endif
#endif
} bcmPktDma_hostHooks_t;

extern bcmPktDma_hostHooks_t bcmPktDma_hostHooks_g;

int bcmPktDma_bind(bcmPktDma_hostHooks_t *hooks);
void bcmPktDma_unbind(void);

#if defined(CONFIG_BCM_FAP_LAYER2) && defined(CONFIG_11ac_throughput_patch_from_412L08)
typedef void (*bcmPktDma_arlNotifyHandlerFuncP)(hostMsgGroups_t op, fapMsg_arlEntry_t *arlEntry_p);
extern bcmPktDma_arlNotifyHandlerFuncP bcmPktDma_arlNotifyHandlerFuncP_g;

void bcmPktDma_registerArlNotifyHandler(bcmPktDma_arlNotifyHandlerFuncP arlNotifyHandlerFuncP);
void bcmPktDma_unregisterArlNotifyHandler(void);
#endif /* CONFIG_BCM_FAP_LAYER2 */

#endif  /* defined(__PKTDMA_HOOKS_H_INCLUDED__) */
