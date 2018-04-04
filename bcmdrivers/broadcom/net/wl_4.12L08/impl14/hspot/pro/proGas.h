/*
 * 802.11u GAS state machine.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
 */

#ifndef _PROGAS_H_
#define _PROGAS_H_

#include "typedefs.h"
#include "proto/ethernet.h"

typedef struct proGasStruct proGasT;

/* Opaque driver handle type. In dongle this is struct wlc_info_t, representing
 * the driver. On linux host this is struct ifreq, representing the primary OS
 * interface for a driver instance. To specify a virtual interface this should
 * be used together with a bsscfg index.
 */
struct wl_drv_hdl;

/* maximum GAS instances */
#define MAX_GAS_INSTANCE	16

/* event notification type */
typedef enum
{
	PRO_GAS_EVENT_QUERY_REQUEST,	 /* query request to the advertisement server */
	PRO_GAS_EVENT_TX,				 /* GAS packet transmited */
	PRO_GAS_EVENT_RX,				 /* GAS packet received */
	PRO_GAS_EVENT_RESPONSE_FRAGMENT, /* response fragment received */
	PRO_GAS_EVENT_STATUS,			 /* status at the completion of GAS exchange */
} proGasEventTypeT;

#define MAX_QUERY_REQUEST_LENGTH	128

typedef struct
{
	uint16 len;
	uint8 data[MAX_QUERY_REQUEST_LENGTH];
} proGasQueryRequestT;

typedef struct
{
	int gasActionFrame;		/* GAS action frame type (eg. GAS_REQUEST_ACTION_FRAME) */
	int length;				/* packet length */
	uint8 fragmentId;		/* fragment ID and more bit (0x80) */
} proGasPacketT;

typedef struct
{
	int length;				/* fragment length */
	uint8 fragmentId;		/* fragment ID and more bit (0x80) */
} proGasFragmentT;

typedef struct
{
	uint16 statusCode;
} proGasStatusT;

typedef struct
{
	proGasT *gas;
	struct ether_addr peer;
	uint8 dialogToken;
	proGasEventTypeT type;
	union {
		proGasQueryRequestT queryReq;
		proGasPacketT tx;
		proGasPacketT rx;
		proGasFragmentT rspFragment;
		proGasStatusT status;
	};
} proGasEventT;

/* enable/disable incoming GAS request (default is enable) */
void proGasIncomingRequest(int enable);

/* configure dot11GASPauseForServerResponse (default is FALSE) */
void proGasPauseForServerResponse(int isPause);

/* override comeback delay timeout (default is 0) */
void proGasComebackDelayOverride(int msec);

/* set comeback delay in GAS response for unpause */
void proGasSetComebackDelayResponseUnpause(int msec);

/* set comeback delay in GAS response for pause */
void proGasSetComebackDelayResponsePause(int msec);

/* enable/disable fragment response (default is disable) */
void proGasFragmentResponse(int enable);

/* initialize GAS protocol */
int proGasInitialize(void);
void proGasInitDsp(void);
void proGasInitWlanHandler(void);

/* deinitialize GAS protocol */
int proGasDeinitialize(void);

/* create GAS protocol instance to destination */
proGasT *proGasCreate(struct wl_drv_hdl *drv, int bsscfg_idx,
	uint16 channel, struct ether_addr *dst);

/* destroy GAS protocol instance */
int proGasDestroy(proGasT *gas);

/* reset GAS protocol instance */
int proGasReset(proGasT *gas);

/* set maximum retransmit on no ACK from peer */
int proGasSetMaxRetransmit(proGasT *gas, uint16 count);

/* set transmit response timeout */
int proGasSetResponseTimeout(proGasT *gas, uint16 msec);

/* set maximum comeback delay expected for a response */
int proGasSetMaxComebackDelay(proGasT *gas, uint16 msec);

/* start GAS protocol instance - send initial GAS request configued by proGasSetQueryRequest */
int proGasStart(proGasT *gas);

/* subscribe for GAS event notification callback */
int proGasSubscribeEvent(void *context,
	void (*fn)(void *context, proGasT *gas, proGasEventT *event));

/* unsubscribe for GAS event notification callback */
int proGasUnsubscribeEvent(void (*fn)(void *context,
	proGasT *gas, proGasEventT *event));

/* set query request */
int proGasSetQueryRequest(proGasT *gas, int len, uint8 *data);

/* set query response data in response to a PRO_GAS_EVENT_QUERY_REQUEST event */
int proGasSetQueryResponse(proGasT *gas, int len, uint8 *data);

/* retrieve query response length after protocol completes */
/* retrieve fragment response length in response to PRO_GAS_EVENT_RESPONSE_FRAGMENT */
int proGasGetQueryResponseLength(proGasT *gas);

/* retrieve query response after protocol completes */
/* retrieve fragment response in response to PRO_GAS_EVENT_RESPONSE_FRAGMENT */
int proGasGetQueryResponse(proGasT *gas, int dataLen, int *len, uint8 *data);

/* set bsscfg index */
int proGasSetBsscfgIndex(proGasT *gas, int index);

/* configure dot11GASPauseForServerResponse per interface */
void proGasSetIfGASPause(int isPause, struct wl_drv_hdl *drv);

/* set comeback delay in GAS response for unpause per interface */
void proGasSetIfCBDelayUnpause(int msec, struct wl_drv_hdl *drv);

/* get driver handle */
struct wl_drv_hdl *proGasGetDrv(proGasT *gas);

/* wlan event handler */
void proGasProcessWlanEvent(void *context, uint32 eventType,
	wl_event_msg_t *wlEvent, uint8 *data, uint32 length);

#endif /* _PROGAS_H_ */
