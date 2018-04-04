/*
<:copyright-BRCM:2007:DUAL/GPL:standard

   Copyright (c) 2007 Broadcom Corporation
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

/***************************************************************************
 * File Name  : bcmxtmrt.h
 *
 * Description: This file contains the definitions, structures and function
 *              prototypes for the Broadcom Asynchronous/Packet Transfer Mode
 *              (XTM) Runtime driver.
 ***************************************************************************/

#if !defined(_BCMXTMRT_H_)
#define _BCMXTMRT_H_

#include "bcmxtmcfg.h"

#define CNI_HW_ADD_HEADER                   0x01
#define CNI_HW_REMOVE_HEADER                0x02
#define CNI_USE_ALT_FSTAT                   0x04
#define CNI_HW_REMOVE_TRAILER               0x08
#define LSC_RAW_ENET_MODE                   0x80000000
#define CELL_PAYLOAD_SIZE                   48
#define CELL_SIZE                           (5 + CELL_PAYLOAD_SIZE)
#define MAX_VCIDS                           16

#define XTMRT_DEV_OPENED                    1
#define XTMRT_DEV_CLOSED                    2

#define CELL_HDLR_OAM                       1
#define CELL_HDLR_ASM                       2

#define XTMRT_CMD_GLOBAL_INITIALIZATION     1
#define XTMRT_CMD_CREATE_DEVICE             2
#define XTMRT_CMD_GET_DEVICE_STATE          3
#define XTMRT_CMD_SET_ADMIN_STATUS          4
#define XTMRT_CMD_REGISTER_CELL_HANDLER     5
#define XTMRT_CMD_UNREGISTER_CELL_HANDLER   6
#define XTMRT_CMD_LINK_STATUS_CHANGED       7
#define XTMRT_CMD_SEND_CELL                 8
#define XTMRT_CMD_DELETE_DEVICE             9
#define XTMRT_CMD_SET_TX_QUEUE              10
#define XTMRT_CMD_UNSET_TX_QUEUE            11
#define XTMRT_CMD_GET_NETDEV_TXCHANNEL      12
#define XTMRT_CMD_GLOBAL_UNINITIALIZATION   13
#define XTMRT_CMD_TOGGLE_PORT_DATA_STATUS_CHANGE	  14
#define XTMRT_CMD_SET_TEQ_DEVCTX            15
#define XTMRT_CMD_SET_DS_SEQ_DEVIATION 	    16
#if 1 //__MSTC__, Eric, VCAUTOHUNT
#define XTMRT_CMD_SEND_VCHUNT_PATTERN		17
#define XTMRT_CMD_GET_VCHUNT_TEST_STATUS 	18
#define XTMRT_CMD_SET_VCAUTOHUNT_INFO		19
#endif //__MSTC__, Eric, VCAUTOHUNT
#if 1 //__MSTC__, FuChia
#define XTMRT_CMD_GET_TXRATE                20
#endif //__MSTC__, FuChia
#define XTMRT_CMD_GLOBAL_REINITIALIZATION   21

#define XTMRT_CMD_PORT_DATA_STATUS_DISABLED  0
#define XTMRT_CMD_PORT_DATA_STATUS_ENABLED   1

#define XTMRTCB_CMD_CELL_RECEIVED           3

typedef UINT32 XTMRT_HANDLE;

typedef int (*FN_XTMRT_REQ) (XTMRT_HANDLE hDev, UINT32 ulCmd, void *pParm);

typedef struct XtmrtGlobalInitParms
{
    UINT32 ulReceiveQueueSizes[MAX_RECEIVE_QUEUES];
    XtmBondConfig bondConfig ;

    UINT32 *pulMibTxOctetCountBase;
    UINT32 ulMibRxClrOnRead;
    UINT32 *pulMibRxCtrl;
    UINT32 *pulMibRxMatch;
    UINT32 *pulMibRxOctetCount;
    UINT32 *pulMibRxPacketCount;
    UINT32 *pulRxCamBase ;

} XTMRT_GLOBAL_INIT_PARMS, *PXTMRT_GLOBAL_INIT_PARMS;

typedef struct XtmrtCreateNetworkDevice
{
    XTM_ADDR ConnAddr;
    char szNetworkDeviceName[NETWORK_DEVICE_NAME_SIZE];
    UINT32 ulHeaderType;
    UINT32 ulFlags;
    XTMRT_HANDLE hDev;
} XTMRT_CREATE_NETWORK_DEVICE, *PXTMRT_CREATE_NETWORK_DEVICE;

typedef struct XtmrtCell
{
    XTM_ADDR ConnAddr;
    UINT8 ucCircuitType;
    UINT8 ucData[CELL_SIZE];
} XTMRT_CELL, *PXTMRT_CELL;

#if 1 //__MSTC__, Eric, VCAUTOHUNT
typedef struct XtmrtVcHuntTest
{ 
  XTM_ADDR ConnAddr;
  UINT32 ulTestServiceType;
  UINT32 ulTestHeaderType;
} XTMRT_VCHUNT_TEST, *PXTMRT_VCHUNT_TEST ;

typedef struct XtmrtVcHuntTestStatus
{ 
  	XTM_ADDR ConnAddr;
	UINT32  ulVcHuntStatus ;  		/*read only*/
  	UINT32  ulHuntedService ; 		/*read only*/
	UINT32	ulHeaderType ;			/*read only*/
} XTMRT_VCHUNT_TEST_STATUS, *PXTMRT_VCHUNT_TEST_STATUS ;

typedef struct XtmrtVcAutoHuntInfo
{ 
	UINT8  ucVcAutoHuntWork ; 		
	UINT8  ucIsVcAutoHuntWork ;  	/*read only*/
} XTMRT_VCAUTOHUNT_INFO, *PXTMRT_VCAUTOHUNT_INFO ;

#endif //__MSTC__, Eric, VCAUTOHUNT

typedef int (*XTMRT_CALLBACK) (XTMRT_HANDLE hDev, UINT32 ulCommand, void *pParam,
    void *pContext);

typedef struct XtmrtCellHdlr
{
    UINT32 ulCellHandlerType;
    XTMRT_CALLBACK pfnCellHandler;
    void *pContext;
} XTMRT_CELL_HDLR, *PXTMRT_CELL_HDLR;

#define XTM_HIGH_SUB_PRIORITY       MAX_SUB_PRIORITIES-1
#define XTM_LOW_SUB_PRIORITY        0

typedef struct XtmrtTransmitQueueId
{
    UINT32 ulPortId;
    UINT32 ulPtmPriority;
    UINT32 ulWeightAlg;
    UINT32 ulWeightValue;
    UINT32 ulSubPriority;
    UINT32 ulQueueSize;
    UINT32 ulMinBitRate;            /* 0 indicates no shaping */
    UINT32 ulShapingRate;           /* 0 indicates no shaping */
    UINT16 usShapingBurstSize;
    UINT16 usQosQId;
    UINT32 ulQueueIndex;
    UINT32 ulBondingPortId;      /* Read-only */
#if (defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE))
    UINT32 ulLoThresh;
    UINT32 ulHiThresh;
    UINT32 ulDropped;
#endif
} XTMRT_TRANSMIT_QUEUE_ID, *PXTMRT_TRANSMIT_QUEUE_ID;

typedef struct XtmrtLinkStatusChange
{
    UINT32 ulLinkState;
    UINT32 ulLinkUsRate;
    UINT32 ulLinkDsRate;
    UINT32 ulLinkDataMask ;
    UINT32 ulOtherLinkUsRate ;
    UINT32 ulOtherLinkDsRate ;
    UINT32 ulDsSeqDeviation ;   /* Only for bonding */
    UINT32 ulTrafficType ;
    UINT32 ulTransmitQueueIdsSize;
    XTMRT_TRANSMIT_QUEUE_ID TransmitQueueIds[MAX_TRANSMIT_QUEUES];
    UINT8 ucTxVcid ;
    UINT32 ulRxVcidsSize ;
    UINT8 ucRxVcids [MAX_VCIDS];
} XTMRT_LINK_STATUS_CHANGE, *PXTMRT_LINK_STATUS_CHANGE;

typedef struct XtmrtNetdevTxchannel
{
    UINT32 txChannel;
    void * pDev;
} XTMRT_NETDEV_TXCHANNEL, *PXTMRT_NETDEV_TXCHANNEL;

typedef struct XtmrtTogglePortDataStatusChange
{
    UINT32 ulPortId;
    UINT32 ulPortDataUsStatus ;
    UINT32 ulPortDataDsStatus ;
} XTMRT_TOGGLE_PORT_DATA_STATUS_CHANGE, *PXTMRT_TOGGLE_PORT_DATA_STATUS_CHANGE;

#if defined(__cplusplus)
extern "C" {
#endif

int bcmxtmrt_request(XTMRT_HANDLE hDev, UINT32 ulCmd, void *pParm);

#if defined(__cplusplus)
}
#endif

#endif /* _BCMXTMRT_H_ */

