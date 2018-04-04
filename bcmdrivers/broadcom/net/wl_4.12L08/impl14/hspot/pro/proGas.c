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

#include <stdlib.h>
#include <string.h>
#include "proto/802.11.h"
#include "bcmendian.h"
#ifndef BCMDRIVER
#include "dsp.h"
#include "tmr.h"
#include "wlu_api.h"
#else
#include "wlc_gas.h"
#endif /* BCMDRIVER */
#include "trace.h"
#include "pktEncodeGas.h"
#include "pktDecodeGas.h"
#include "pktEncodeIe.h"
#include "pktDecodeIe.h"
#include "proGas.h"

#ifdef BCMDRIVER
#define BCM_GAS_NO_MULTIPLE_INTERFACES	/* single wlan chip */
#define BCM_GAS_NO_DISPATCHER			/* direct function call */
#define BCM_GAS_NO_REASSEMBLY			/* host to reassemble GAS comeback */
#endif /* BCMDRIVER */

#define QUERY_RESPONSE_TIMEOUT				1000	/* milliseconds */
#define COMEBACK_DELAY_RESPONSE_UNPAUSE		QUERY_RESPONSE_TIMEOUT
#define COMEBACK_DELAY_RESPONSE_PAUSE		1		/* milliseconds */
#define DEFAULT_MAX_COMEBACK_DELAY			0xffff
#define DEFAULT_RESPONSE_TIMEOUT			2000	/* milliseconds */

#define DEFAULT_MAX_RETRANSMIT				4

#define MAX_FRAGMENT_SIZE				(ACTION_FRAME_SIZE - 256)
#define MORE_BIT	0x80

#define NUM_FRAGMENT_PER_REALLOC		4

typedef struct {
	struct wl_drv_hdl *drv;
	int pause;
} IfGASPauseForServerResponseT;

typedef struct {
	struct wl_drv_hdl *drv;
	int CBDelay;
} IfCBDelayT;

#ifndef BCM_GAS_NO_MULTIPLE_INTERFACES
static IfGASPauseForServerResponseT gIfGASPause[MAX_WLIF_NUM];
static int gIfGASPause_num = 0;

static IfCBDelayT gIfCBDelayUnpause[MAX_WLIF_NUM];
static int gIfCBDelayUnpause_num = 0;
#endif	/* BCM_GAS_NO_MULTIPLE_INTERFACES */

/* support incoming request */
static int gIncomingRequest = TRUE;

/* implements pause/no pause for server response */
static int gDot11GASPauseForServerResponse = TRUE;

/* override comeback delay */
static int gComebackDelayOverride = 0;

/* comeback delay in GAS response with false gDot11GASPauseForServerResponse */
static int gComebackDelayResponseUnpause = COMEBACK_DELAY_RESPONSE_UNPAUSE;

/* comeback delay in GAS response with true gDot11GASPauseForServerResponse */
static int gComebackDelayResponsePause = COMEBACK_DELAY_RESPONSE_PAUSE;

/* fragment response */
static int gFragmentResponse =
#ifdef BCM_GAS_NO_REASSEMBLY
	TRUE;
#else
	FALSE;
#endif	/* BCM_GAS_NO_REASSEMBLY */

/* dialog token */
static uint8 gNextDialogToken = 0;

/* table of instances for initiated and incoming */
static proGasT *gGasInstance[MAX_GAS_INSTANCE];
static proGasT *gGasInstanceIncoming[MAX_GAS_INSTANCE];

static struct {
	void (*fn)(void *context, proGasT *gas, proGasEventT *event);
	void *context;
} gEventCallback;

/* state machine states */
typedef enum {
	/* initial idle state - pending START or RX_REQUEST events */
	STATE_IDLE,
	/* request tx'ed - pending RX_RESPONSE event */
	STATE_TX_REQUEST,
	/* response tx'ed - pending COMEBACK_REQUEST event (if more data) */
	STATE_TX_RESPONSE,
	/* comeback request tx'ed - pending COMEBACK_RESPONSE event */
	STATE_TX_COMEBACK_REQUEST,
	/* comeback response tx'ed - pending COMEBACK_REQUEST event (if more data) */
	STATE_TX_COMEBACK_RESPONSE,
	/* request tx'ed, query request posted - pending COMEBACK_REQUEST event, query response */
	STATE_TX_RESPONSE_AND_QUERY_REQUEST,
	/* query request posted - pending query response */
	STATE_WAIT_FOR_QUERY_RESPONSE,
	/* response rx'ed - waiting comeback delay */
	STATE_WAIT_COMEBACK_DELAY,
	/* error response tx'ed */
	STATE_ERROR
} stateT;

#if defined(BCMDBG)
/* state to string */
static char *state_str[] = {
	"STATE_IDLE",
	"STATE_TX_REQUEST",
	"STATE_TX_RESPONSE",
	"STATE_TX_COMEBACK_REQUEST",
	"STATE_TX_COMEBACK_RESPONSE",
	"STATE_TX_RESPONSE_AND_QUERY_REQUEST",
	"STATE_WAIT_FOR_QUERY_RESPONSE",
	"STATE_WAIT_COMEBACK_DELAY",
	"STATE_ERROR"
};

/* event to string */
static char *event_str[] = {
	"EVENT_RESET",
	"EVENT_CONFIGURE",
	"EVENT_START",
	"EVENT_RX_REQUEST",
	"EVENT_RX_RESPONSE",
	"EVENT_RX_COMEBACK_REQUEST",
	"EVENT_RX_COMEBACK_RESPONSE",
	"EVENT_RX_QUERY_RESPONSE",
	"EVENT_RESPONSE_TIMEOUT",
	"EVENT_COMEBACK_DELAY_TIMEOUT",
	"EVENT_QUERY_RESPONSE_TIMEOUT",
	"EVENT_ACTION_FRAME_TX_SUCCESS",
	"EVENT_ACTION_FRAME_TX_FAILED",
};
#endif	/* BCMDBG */

/* state machine events */
typedef enum {
	EVENT_RESET,					/* reset state machine */
	EVENT_CONFIGURE,				/* configure state machine */
	EVENT_START,					/* transmit GAS request */
	EVENT_RX_REQUEST,				/* receive GAS request */
	EVENT_RX_RESPONSE,				/* receive GAS response */
	EVENT_RX_COMEBACK_REQUEST,		/* receive GAS comeback request */
	EVENT_RX_COMEBACK_RESPONSE,		/* receive GAS comeback response */
	EVENT_RX_QUERY_RESPONSE,		/* receive query resoponse */
	EVENT_RESPONSE_TIMEOUT,			/* no response timeout */
	EVENT_COMEBACK_DELAY_TIMEOUT,	/* comeback delay timeout */
	EVENT_QUERY_RESPONSE_TIMEOUT,	/* no query response timeout */
	EVENT_ACTION_FRAME_TX_SUCCESS,	/* ACK received for action frame transmit */
	EVENT_ACTION_FRAME_TX_FAILED	/* no ACK received for action frame transmit */
} eventT;

struct proGasStruct
{
	/* wl driver handle */
	struct wl_drv_hdl *drv;

	/* bsscfg index */
	int bsscfgIndex;

	/* incoming request */
	int isIncoming;

	/* channel */
	uint16 channel;

	/* mac address */
	struct ether_addr mac;

	/* peer address */
	struct ether_addr peer;

	/* advertisement protocol */
	uint8 advertisementProtocol;

	/* maximum retransmit */
	uint16 maxRetransmit;

	/* response timeout */
	uint16 responseTimeout;

	/* maximum comeback delay */
	uint16 maxComebackDelay;

	/* timers */
	tmrT *responseTimer;
	tmrT *comebackDelayTimer;
	tmrT *queryResponseTimer;

	/* FSM state */
	stateT state;
	stateT nextState;

	/* current dialog token */
	uint8 dialogToken;

	/* current status code */
	uint16 statusCode;

	/* pending status notification */
	int isPendingStatusNotification;

	/* retransmit count */
	int retransmit;

	/* action frame tx parameters */
	struct {
		uint32 packetId;
		uint32 channel;
		uint16 responseTimeout;
		struct ether_addr bssid;
		struct ether_addr dst;
		int gasActionFrame;
		int length;
		uint8 data[ACTION_FRAME_SIZE];
	} tx;

	/* tx request data */
	struct {
		uint32 length;
		uint8 *data;
	} txRequest;

	/* tx response data */
	struct {
		uint8 fragmentId;
		uint8 lastFragmentId;
		uint32 length;
		uint8 *data;
	} txResponse;

	/* rx response data */
	struct {
		int isValid;
		uint8 fragmentId;
		uint32 maxLength;
		uint32 length;
		uint8 *data;
	} rxResponse;
};

typedef struct proGasReq proGasReqT;

/* request handler */
typedef void (*requestHandlerT)(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspData);

typedef struct {
	int isIncoming;
	struct wl_drv_hdl *drv;
	int bsscfgIndex;
	uint16 channel;
	struct ether_addr peer;
} proGasCreateReqT;

typedef struct {
	proGasT *gas;
} proGasCreateRspT;

typedef struct {
	uint16 value;
} proGasSetParamReqT;

typedef struct {
	int len;
	uint8 *data;
} proGasSetQueryRequestReqT;

typedef struct {
	int len;
	uint8 *data;
} proGasSetQueryResponseReqT;

typedef struct {
	int index;
} proGasSetBsscfgIndexReqT;

typedef struct {
	int len;
} proGasGetQueryResponseLengthRspT;

typedef struct {
	int maxLen;
	int *len;
	uint8 *data;
} proGasGetQueryResponseRspT;

struct proGasReq {
	requestHandlerT handler;
	union {
		proGasCreateReqT create;
		proGasSetParamReqT setParam;
		proGasSetQueryRequestReqT setQueryRequest;
		proGasSetQueryResponseReqT setQueryResponse;
		proGasSetBsscfgIndexReqT setBsscfgIndex;
	};
};

/* forward declarations */
static void deleteResponseData(proGasT *gas);
static void rxQueryResponse(proGasT *gas, int dataLen, uint8 *data);
static void stateWaitQueryResponseProcessQueryResponse(proGasT *gas);
static void rxNotification(proGasT *gas, pktGasDecodeT *gasDecode, int length);
static void fsm(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len);

/* ----------------------------------------------------------- */

static int addEntry(proGasT *gas)
{
	proGasT **instance;
	int i;

	instance = gas->isIncoming ? gGasInstanceIncoming : gGasInstance;

	for (i = 0; i < MAX_GAS_INSTANCE; i++) {
		if (instance[i] != 0) {
			if ((memcmp(&instance[i]->peer, &gas->peer,
				sizeof(instance[i]->peer)) == 0) &&
				(instance[i]->drv == gas->drv)) {
				WL_PRMAC("already existing mac",
					&instance[i]->peer);
			}
		}
	}

	for (i = 0; i < MAX_GAS_INSTANCE; i++) {
		if (instance[i] == 0) {
			instance[i] = gas;
			return TRUE;
		}
	}

	return FALSE;
}

static int delEntry(proGasT *gas)
{
	proGasT **instance;
	int i;

	instance = gas->isIncoming ? gGasInstanceIncoming : gGasInstance;

	for (i = 0; i < MAX_GAS_INSTANCE; i++) {
		if (instance[i] == gas) {
			instance[i] = 0;
			return TRUE;
		}
	}

	return FALSE;
}

static proGasT *findEntry(struct ether_addr *mac, int isIncoming)
{
	proGasT **instance;
	int i;

	instance = isIncoming ? gGasInstanceIncoming : gGasInstance;

	for (i = 0; i < MAX_GAS_INSTANCE; i++) {
		if (instance[i] != 0) {
			if (memcmp(&instance[i]->peer, mac,
				sizeof(instance[i]->peer)) == 0) {
				return instance[i];
			}
		}
	}

	return 0;
}

static int isEntryValid(proGasT *gas)
{
	proGasT **instance;
	int i;

	instance = gGasInstanceIncoming;

	for (i = 0; i < MAX_GAS_INSTANCE; i++) {
		if (instance[i] == gas) {
			return TRUE;
		}
	}

	instance = gGasInstance;

	for (i = 0; i < MAX_GAS_INSTANCE; i++) {
		if (instance[i] == gas) {
			return TRUE;
		}
	}

	return FALSE;
}

/* ----------------------------------------------------------- */

static void responseTimeout(void *arg)
{
	proGasT *gas = (proGasT *)arg;

	WL_TRACE(("responseTimeout callback\n"));
	fsm(gas, EVENT_RESPONSE_TIMEOUT, 0, 0, 0, 0);
}

static void comebackDelayTimeout(void *arg)
{
	proGasT *gas = (proGasT *)arg;

	WL_TRACE(("comebackDelayTimeout callback\n"));
	fsm(gas, EVENT_COMEBACK_DELAY_TIMEOUT, 0, 0, 0, 0);
}

static void queryResponseTimeout(void *arg)
{
	proGasT *gas = (proGasT *)arg;

	WL_TRACE(("queryResponseTimer callback\n"));
	fsm(gas, EVENT_QUERY_RESPONSE_TIMEOUT, 0, 0, 0, 0);
}

/* ----------------------------------------------------------- */

/* enable/disable incoming GAS request (default is enable) */
void proGasIncomingRequest(int enable)
{
	gIncomingRequest = enable;
}

/* ----------------------------------------------------------- */

/* configure dot11GASPauseForServerResponse (default is FALSE) */
void proGasPauseForServerResponse(int isPause)
{
	gDot11GASPauseForServerResponse = isPause;
}

#ifndef BCM_GAS_NO_MULTIPLE_INTERFACES
/* configure dot11GASPauseForServerResponse per interface */
void proGasSetIfGASPause(int isPause, struct wl_drv_hdl *drv)
{
	int i;
	for (i = 0; i < gIfGASPause_num; i++) {
		if (gIfGASPause[i].drv == drv) {
			gIfGASPause[i].pause = isPause;
			return;
		}
	}

	if (gIfGASPause_num < MAX_WLIF_NUM) {
		gIfGASPause[gIfGASPause_num].pause = isPause;
		gIfGASPause[gIfGASPause_num].drv = drv;
		gIfGASPause_num ++;
	}
}
#endif	/* BCM_GAS_NO_MULTIPLE_INTERFACES */

static int getGasPauseForServerResponse(struct wl_drv_hdl *drv)
{
#ifndef BCM_GAS_NO_MULTIPLE_INTERFACES
	int i;
	for (i = 0; i < gIfGASPause_num; i++) {
		if (gIfGASPause[i].drv == drv) {
			return gIfGASPause[i].pause;
		}
	}
#else
	(void)drv;
#endif	/* BCM_GAS_NO_MULTIPLE_INTERFACES */
	return gDot11GASPauseForServerResponse;
}

/* ----------------------------------------------------------- */

/* override comeback delay timeout (default is 0) */
void proGasComebackDelayOverride(int msec)
{
	gComebackDelayOverride = msec;
}

/* set comeback delay in GAS response for unpause */
void proGasSetComebackDelayResponseUnpause(int msec)
{
	gComebackDelayResponseUnpause = msec;
}

#ifndef BCM_GAS_NO_MULTIPLE_INTERFACES
/* set comeback delay in GAS response for unpause per interface */
void proGasSetIfCBDelayUnpause(int msec, struct wl_drv_hdl *drv)
{
	int i;
	for (i = 0; i < gIfCBDelayUnpause_num; i++) {
		if (gIfCBDelayUnpause[i].drv == drv) {
			gIfCBDelayUnpause[i].CBDelay = msec;
			return;
		}
	}

	if (gIfCBDelayUnpause_num < MAX_WLIF_NUM) {
		gIfCBDelayUnpause[gIfCBDelayUnpause_num].CBDelay = msec;
		gIfCBDelayUnpause[gIfCBDelayUnpause_num].drv = drv;
		gIfCBDelayUnpause_num ++;
	}
}
#endif	/* BCM_GAS_NO_MULTIPLE_INTERFACES */

static int getComebackDelayResponseUnpause(void *drv)
{
#ifndef BCM_GAS_NO_MULTIPLE_INTERFACES
	int i;
	for (i = 0; i < gIfCBDelayUnpause_num; i++) {
		if (gIfCBDelayUnpause[i].drv == drv) {
			return gIfCBDelayUnpause[i].CBDelay;
		}
	}
#else
	(void)drv;
#endif	/* BCM_GAS_NO_MULTIPLE_INTERFACES */
	return gComebackDelayResponseUnpause;
}

/* set comeback delay in GAS response for pause */
void proGasSetComebackDelayResponsePause(int msec)
{
	gComebackDelayResponsePause = msec;
}

/* enable/disable fragment response (default is disable) */
void proGasFragmentResponse(int enable)
{
	gFragmentResponse = enable;
}

/* get driver handle */
struct wl_drv_hdl *proGasGetDrv(proGasT *gas)
{
	return gas->drv;
}

/* ----------------------------------------------------------- */

#ifndef BCMDRIVER
void proGasInitDsp(void)
{
	/* attach handler to dispatcher */
	dspSubscribe(dsp(), 0, proGasProcessWlanEvent);
}

void proGasInitWlanHandler(void)
{
	void *ifr;
	int i = 0;

	WL_TRACE(("proGasInitWlanHandler\n"));

	for (ifr = wl(); ifr; ifr = wlif(++i)) {
		/* enable action frame rx event */
		if (wl_enable_event_msg(ifr, WLC_E_ACTION_FRAME_COMPLETE) < 0) {
			WL_ERROR(("failed to enable action frame complete event\n"));
		}
		if (wl_enable_event_msg(ifr, WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE) < 0) {
			WL_ERROR(("failed to enable act frame off chan complete event\n"));
		}
		if (wl_enable_event_msg(ifr, WLC_E_ACTION_FRAME_RX) < 0) {
			WL_ERROR(("failed to enable action frame rx event\n"));
		}
	}
}

static void proGasInitHandler(proGasT *gasNull,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)gasNull;
	(void)rspNull;
	if (reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasInitHandler\n"));
	proGasInitWlanHandler();
}

/* initialize GAS protocol */
int proGasInitialize(void)
{
	proGasReqT req;

	WL_TRACE(("proGasInitialize\n"));

	/* attach handler to dispatcher */
	dspSubscribe(dsp(), 0, proGasProcessWlanEvent);

	req.handler = (requestHandlerT)proGasInitHandler;
	return dspRequestSynch(dsp(), 0, sizeof(req), (uint8 *)&req, 0);
}

/* ----------------------------------------------------------- */

/* deinitialize GAS protocol */
int proGasDeinitialize(void)
{
	/* detach handler */
	dspUnsubscribe(dsp(), proGasProcessWlanEvent);
	return TRUE;
}
#endif /* BCMDRIVER */

/* ----------------------------------------------------------- */

static void proGasCreateHandler(proGasT *gasNull,
	int reqLength, proGasReqT *req, proGasCreateRspT *rsp)
{
	proGasT *gas;

	(void)gasNull;
	if (reqLength != sizeof(proGasReqT) || req == 0 || rsp == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasCreateHandler\n"));

	rsp->gas = 0;

	gas = malloc(sizeof(*gas));
	if (gas == 0)
		return;

	memset(gas, 0, sizeof(*gas));
	gas->isIncoming = req->create.isIncoming;
	gas->drv = req->create.drv;
	gas->bsscfgIndex = req->create.bsscfgIndex;
	gas->channel = req->create.channel;
	memcpy(&gas->peer, &req->create.peer, sizeof(gas->peer));
	gas->advertisementProtocol = ADVP_ANQP_PROTOCOL_ID;
	gas->maxRetransmit = DEFAULT_MAX_RETRANSMIT;
	gas->responseTimeout = DEFAULT_RESPONSE_TIMEOUT;
	gas->maxComebackDelay = DEFAULT_MAX_COMEBACK_DELAY;
	WL_PRMAC("creating instance for peer mac", &gas->peer);

	/* save new entry */
	delEntry(gas);
	if (!addEntry(gas)) {
		WL_ERROR(("failed to add new entry\n"));
		goto fail;
	}

	/* retrieve MAC address */
	if (wl_cur_etheraddr(gas->drv, gas->bsscfgIndex, &gas->mac) < 0) {
		WL_ERROR(("failed to get mac address\n"));
	}
	WL_PRMAC("MAC addr", &gas->mac);

	/* create timers */
	gas->responseTimer = tmrCreate(
#ifndef BCMDRIVER
		dsp(),
#else
		gas->drv,
#endif	/* BCMDRIVER */
		responseTimeout, gas, "responseTimer");
	if (gas->responseTimer == 0) {
		WL_ERROR(("failed to create timer\n"));
		goto fail;
	}
	gas->comebackDelayTimer = tmrCreate(
#ifndef BCMDRIVER
		dsp(),
#else
		gas->drv,
#endif	/* BCMDRIVER */
		comebackDelayTimeout, gas, "comebackDelayTimer");
	if (gas->comebackDelayTimer == 0) {
		WL_ERROR(("failed to create timer\n"));
		goto fail;
	}
	gas->queryResponseTimer = tmrCreate(
#ifndef BCMDRIVER
		dsp(),
#else
		gas->drv,
#endif	/* BCMDRIVER */
		queryResponseTimeout, gas, "queryResponseTimer");
	if (gas->queryResponseTimer == 0) {
		WL_ERROR(("failed to create timer\n"));
		goto fail;
	}

	/* reset state machine */
	fsm(gas, EVENT_RESET, 0, 0, 0, 0);

	/* return created instance */
	rsp->gas = gas;
	return;

fail:
	free(gas);
}

/* create GAS protocol instance to destination */
proGasT *proGasCreate(struct wl_drv_hdl *drv, int bsscfg_idx,
	uint16 channel, struct ether_addr *dst)
{
	proGasReqT req;
	proGasCreateRspT rsp;

	WL_TRACE(("proGasCreate\n"));

	req.handler = (requestHandlerT)proGasCreateHandler;
	req.create.isIncoming = FALSE;
	req.create.drv = drv;
	req.create.bsscfgIndex = bsscfg_idx;
	req.create.channel = channel;
	memcpy(&req.create.peer, dst, sizeof(req.create.peer));
#ifdef BCM_GAS_NO_DISPATCHER
	proGasCreateHandler(0, sizeof(req), &req, &rsp);
#else
	if (!dspRequestSynch(dsp(), 0, sizeof(req), (uint8 *)&req,
		(uint8 *)&rsp))
	{
		return 0;
	}
#endif	/* BCM_GAS_NO_DISPATCHER */

	return rsp.gas;
}

/* ----------------------------------------------------------- */

static void proGasDestroyHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasDestroyHandler\n"));

	/* stop timers */
	tmrStop(gas->responseTimer);
	tmrStop(gas->comebackDelayTimer);
	tmrStop(gas->queryResponseTimer);

	/* destroy timers */
	tmrDestroy(gas->responseTimer);
	tmrDestroy(gas->comebackDelayTimer);
	tmrDestroy(gas->queryResponseTimer);

	delEntry(gas);

	if (gas->txRequest.data)
		free(gas->txRequest.data);
	if (gas->txResponse.data)
		free(gas->txResponse.data);
	if (gas->rxResponse.data)
		free(gas->rxResponse.data);

	free(gas);
}

/* destroy GAS protocol instance */
int proGasDestroy(proGasT *gas)
{
	proGasReqT req;

	WL_TRACE(("proGasDestroy\n"));

	req.handler = proGasDestroyHandler;
#ifdef BCM_GAS_NO_DISPATCHER
	proGasDestroyHandler(gas, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequestSynch(dsp(), gas, sizeof(req), (uint8 *)&req, 0);
#endif /* BCM_GAS_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proGasResetHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasResetHandler\n"));
	fsm(gas, EVENT_RESET, 0, 0, 0, 0);
}

/* reset GAS protocol instance */
int proGasReset(proGasT *gas)
{
	proGasReqT req;

	WL_TRACE(("proGasReset\n"));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proGasResetHandler;
#ifdef BCM_GAS_NO_DISPATCHER
	proGasResetHandler(gas, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), gas, sizeof(req), (uint8 *)&req);
#endif /* BCM_GAS_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proGasSetMaxRetransmitHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasSetMaxRetransmitHandler\n"));
	if (gas->state == STATE_IDLE) {
		gas->maxRetransmit = req->setParam.value;
	}
}

/* set maximum retransmit on no ACK from peer */
int proGasSetMaxRetransmit(proGasT *gas, uint16 count)
{
	proGasReqT req;

	WL_TRACE(("proGasSetMaxRetransmit\n"));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proGasSetMaxRetransmitHandler;
	req.setParam.value = count;
#ifdef BCM_GAS_NO_DISPATCHER
	proGasSetMaxRetransmitHandler(gas, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), gas, sizeof(req), (uint8 *)&req);
#endif /* BCM_GAS_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proGasSetResponseTimeoutHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasSetResponseTimeoutHandler\n"));
	if (gas->state == STATE_IDLE) {
		gas->responseTimeout = req->setParam.value;
	}
}

/* set transmit response timeout */
int proGasSetResponseTimeout(proGasT *gas, uint16 msec)
{
	proGasReqT req;

	WL_TRACE(("proGasSetResponseTimeout\n"));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proGasSetResponseTimeoutHandler;
	req.setParam.value = msec;
#ifdef BCM_GAS_NO_DISPATCHER
	proGasSetResponseTimeoutHandler(gas, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), gas, sizeof(req), (uint8 *)&req);
#endif /* BCM_GAS_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proGasSetMaxComebackDelayHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasSetMaxComebackDelayHandler\n"));
	if (gas->state == STATE_IDLE) {
		gas->maxComebackDelay = req->setParam.value;
	}
}

/* set maximum comeback delay expected for a response */
int proGasSetMaxComebackDelay(proGasT *gas, uint16 msec)
{
	proGasReqT req;

	WL_TRACE(("proGasSetMaxComebackDelay\n"));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proGasSetMaxComebackDelayHandler;
	req.setParam.value = msec;
#ifdef BCM_GAS_NO_DISPATCHER
	proGasSetMaxComebackDelayHandler(gas, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), gas, sizeof(req), (uint8 *)&req);
#endif /* BCM_GAS_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proGasStartHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasStartHandler\n"));
	fsm(gas, EVENT_START, 0, 0, 0, 0);
}

/* start GAS protocol instance - send initial GAS request */
int proGasStart(proGasT *gas)
{
	proGasReqT req;

	WL_TRACE(("proGasStart\n"));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proGasStartHandler;
#ifdef BCM_GAS_NO_DISPATCHER
	proGasStartHandler(gas, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), gas, sizeof(req), (uint8 *)&req);
#endif /* BCM_GAS_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

/* subscribe for GAS event notification callback */
int proGasSubscribeEvent(void *context,
	void (*fn)(void *context, proGasT *gas, proGasEventT *event))
{
	gEventCallback.fn = fn;
	gEventCallback.context = context;
	return TRUE;
}

/* ----------------------------------------------------------- */

/* unsubscribe for GAS event notification callback */
int proGasUnsubscribeEvent(void (*fn)(void *context,
	proGasT *gas, proGasEventT *event))
{
	(void)fn;
	memset(&gEventCallback, 0, sizeof(gEventCallback));
	return TRUE;
}

/* ----------------------------------------------------------- */

static void proGasSetQueryRequestHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasSetQueryRequestHandler\n"));
	if (gas->state == STATE_IDLE) {
		if (gas->txRequest.data != 0) {
			free(gas->txRequest.data);
			gas->txRequest.data = 0;
		}
		gas->txRequest.data = malloc(req->setQueryRequest.len);
		if (gas->txRequest.data == 0)
			return;
		gas->txRequest.length = req->setQueryRequest.len;
		memcpy(gas->txRequest.data, req->setQueryRequest.data,
			gas->txRequest.length);
	}
}

/* set query request */
int proGasSetQueryRequest(proGasT *gas, int len, uint8 *data)
{
	proGasReqT req;
	int ret;

	WL_TRACE(("proGasQueryRequest\n"));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proGasSetQueryRequestHandler;
	req.setQueryRequest.len = len;
	req.setQueryRequest.data = malloc(req.setQueryRequest.len);
	if (req.setQueryRequest.data == 0)
		return 0;
	memcpy(req.setQueryRequest.data, data, req.setQueryRequest.len);
#ifdef BCM_GAS_NO_DISPATCHER
	proGasSetQueryRequestHandler(gas, sizeof(req), &req, 0);
	ret = 1;
#else
	ret = dspRequestSynch(dsp(), gas, sizeof(req), (uint8 *)&req, 0);
#endif /* BCM_GAS_NO_DISPATCHER */
	free(req.setQueryRequest.data);
	return ret;
}

/* ----------------------------------------------------------- */

static void proGasSetQueryResponseHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasSetQueryResponseHandler\n"));

	if (!isEntryValid(gas)) {
		/* timeout will cause entry to be no longer valid */
		free(req->setQueryResponse.data);
		return;
	}

#ifdef BCM_GAS_NO_DISPATCHER
	/* only valid in idle state */
	if (gas->state != STATE_IDLE) {
		return;
	}
	rxQueryResponse(gas, req->setQueryResponse.len, req->setQueryResponse.data);
#else
	fsm(gas, EVENT_RX_QUERY_RESPONSE, 0,
		req->setQueryResponse.len, req->setQueryResponse.data, 0);
#endif
	free(req->setQueryResponse.data);
}


/* set query response data in response to a PRO_GAS_EVENT_QUERY_REQUEST */
int proGasSetQueryResponse(proGasT *gas, int len, uint8 *data)
{
	proGasReqT req;
	int ret;

	WL_TRACE(("proGasQueryResponse len=%d\n", len));

	if ((gas == 0) || (len == 0) || (data == 0)) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proGasSetQueryResponseHandler;
	req.setQueryResponse.len = len;
	req.setQueryResponse.data = malloc(req.setQueryResponse.len);
	if (req.setQueryResponse.data == 0)
		return 0;
	memcpy(req.setQueryResponse.data, data, req.setQueryResponse.len);
#ifdef BCM_GAS_NO_DISPATCHER
	proGasSetQueryResponseHandler(gas, sizeof(req), &req, 0);
	ret = 1;
#else
	ret = dspRequestSynch(dsp(), gas, sizeof(req), (uint8 *)&req, 0);
#endif /* BCM_GAS_NO_DISPATCHER */
	if (ret == 0)
		free(req.setQueryResponse.data);
	return ret;
}

/* ----------------------------------------------------------- */

static void proGasGetQueryResponseLengthHandler(proGasT *gas,
	int reqLength, proGasReqT *req, proGasGetQueryResponseLengthRspT *rsp)
{
	int length;

	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasGetQueryResponseLengthHandler\n"));

	if (gFragmentResponse || gas->rxResponse.isValid)
		length = gas->rxResponse.length;
	else
		length = 0;

	rsp->len = length;
}

/* retrieve query response length after protocol completes */
/* retrieve fragment response length in response to PRO_GAS_EVENT_RESPONSE_FRAGMENT */
int proGasGetQueryResponseLength(proGasT *gas)
{
	proGasReqT req;
	proGasGetQueryResponseLengthRspT rsp;
	int ret;

	WL_TRACE(("proGasGetQueryResponseLength\n"));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

#ifdef BCM_GAS_NO_DISPATCHER
	proGasGetQueryResponseLengthHandler(gas, sizeof(req), &req, &rsp);
	ret = 1;
#else
	req.handler = (requestHandlerT)proGasGetQueryResponseLengthHandler;
	ret = dspRequestSynch(dsp(), gas,
		sizeof(req), (uint8 *)&req, (uint8 *)&rsp);
#endif /* BCM_GAS_NO_DISPATCHER */
	if (ret == 0)
		return 0;
	else
		return rsp.len;
}

/* ----------------------------------------------------------- */

static void proGasGetQueryResponseHandler(proGasT *gas,
	int reqLength, proGasReqT *req, proGasGetQueryResponseRspT *rsp)
{
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasGetQueryResponseHandler\n"));

	if ((gFragmentResponse || gas->rxResponse.isValid) &&
		(int)gas->rxResponse.length <= rsp->maxLen) {
		*rsp->len = gas->rxResponse.length;
		memcpy(rsp->data, gas->rxResponse.data, *rsp->len);
	}
	else {
		*rsp->len = 0;
	}

	if (gFragmentResponse)
		deleteResponseData(gas);
}

/* retrieve query response after protocol completes */
/* retrieve fragment response in response to PRO_GAS_EVENT_RESPONSE_FRAGMENT */
int proGasGetQueryResponse(proGasT *gas, int dataLen, int *len, uint8 *data)
{
	proGasReqT req;
	proGasGetQueryResponseRspT rsp;

	WL_TRACE(("proGasGetQueryResponse\n"));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = (requestHandlerT)proGasGetQueryResponseHandler;
	rsp.maxLen = dataLen;
	rsp.len = len;
	rsp.data = data;
#ifdef BCM_GAS_NO_DISPATCHER
	proGasGetQueryResponseHandler(gas, sizeof(req), &req, &rsp);
	return 1;
#else
	return dspRequestSynch(dsp(), gas,
		sizeof(req), (uint8 *)&req, (uint8 *)&rsp);
#endif /* BCM_GAS_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proGasSetBsscfgIndexHandler(proGasT *gas,
	int reqLength, proGasReqT *req, void *rspNull)
{
	(void)rspNull;
	if (gas == 0 || reqLength != sizeof(proGasReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proGasSetBsscfgIndexHandler\n"));

	gas->bsscfgIndex = req->setBsscfgIndex.index;
}

/* set bsscfg index */
int proGasSetBsscfgIndex(proGasT *gas, int index)
{
	proGasReqT req;

	WL_TRACE(("proGasSetBsscfgIndex index=%d\n", index));

	if (gas == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proGasSetBsscfgIndexHandler;
	req.setBsscfgIndex.index = index;
#ifdef BCM_GAS_NO_DISPATCHER
	proGasSetBsscfgIndexHandler(gas, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), gas, sizeof(req), (uint8 *)&req);
#endif /* BCM_GAS_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static int isValidAdvertisementProtocol(pktAdvertisementProtocolTupleT *apie)
{
	if (apie == 0 || apie->protocolId != ADVP_ANQP_PROTOCOL_ID) {
		WL_P2PO(("invalid protocol ID %d\n", apie->protocolId));
		return FALSE;
	}

	return TRUE;
}

void proGasProcessWlanEvent(void *context, uint32 eventType,
	wl_event_msg_t *wlEvent, uint8 *data, uint32 length)
{
	eventT event;
	struct wl_drv_hdl *drv = context;

	switch (eventType) {
	case WLC_E_ACTION_FRAME_COMPLETE:
	{
		uint32 *packetId = (uint32 *)data;
		proGasT *gas = (proGasT *)*packetId;

		WL_TRACE(("WLC_E_ACTION_FRAME_COMPLETE packetId=0x%x\n", *packetId));

		if (wlEvent->status == WLC_E_STATUS_SUCCESS) {
			WL_P2PO(("WLC_E_ACTION_FRAME_COMPLETE - WLC_E_STATUS_SUCCESS\n"));
			event = EVENT_ACTION_FRAME_TX_SUCCESS;
		}
		else if (wlEvent->status == WLC_E_STATUS_NO_ACK) {
			WL_P2PO(("WLC_E_ACTION_FRAME_COMPLETE - WLC_E_STATUS_NO_ACK\n"));
			event = EVENT_ACTION_FRAME_TX_FAILED;
		}
		else {
			return;
		}

		if (isEntryValid(gas)) {
			fsm(gas, event, 0, 0, 0, 0);
		}
	}
		break;
	case WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE:
		WL_P2PO(("WLC_E_ACTION_FRAME_OFF_CHAN_COMPLETE\n"));
		break;
	case WLC_E_ACTION_FRAME_RX:
	{
		struct ether_addr* src = &wlEvent->addr;
		wl_event_rx_frame_data_t *frameInfo =
			(wl_event_rx_frame_data_t*) data;
		uint16 channel = wf_chspec_ctlchan(ntoh16(frameInfo->channel));
		uint8 *actFrame = (uint8 *) (frameInfo + 1);
		uint32 actFrameLen = length - sizeof(wl_event_rx_frame_data_t);
		pktDecodeT dec;
		pktGasDecodeT gasDecode;

		WL_P2PO(("WLC_E_ACTION_FRAME_RX on channel %d\n", channel));
		WL_PRMAC("action frame src MAC", src);
		WL_PRPKT("RX action frame", actFrame, actFrameLen);

		/* decode GAS packet */
		if (pktDecodeInit(&dec, actFrameLen, actFrame) &&
			pktDecodeGas(&dec, &gasDecode)) {
			proGasT *gas;
			int isIncoming;

			switch (gasDecode.action) {
			case GAS_REQUEST_ACTION_FRAME:
				event = EVENT_RX_REQUEST;
				isIncoming = TRUE;
				break;
			case GAS_RESPONSE_ACTION_FRAME:
				event = EVENT_RX_RESPONSE;
				isIncoming = FALSE;
				break;
			case GAS_COMEBACK_REQUEST_ACTION_FRAME:
				event = EVENT_RX_COMEBACK_REQUEST;
				isIncoming = TRUE;
				break;
			case GAS_COMEBACK_RESPONSE_ACTION_FRAME:
				event = EVENT_RX_COMEBACK_RESPONSE;
				isIncoming = FALSE;
				break;
			default:
				WL_P2PO(("invalid GAS action %d\n", gasDecode.action));
				return;
				break;
			}

			/* find instance else create it */
			gas = findEntry(src, isIncoming);
			if (gas != 0 &&
				gasDecode.dialogToken != gas->dialogToken &&
				gasDecode.action == GAS_REQUEST_ACTION_FRAME) {
					proGasReqT req;

					/* new incoming request - destroy previous instance */
					req.handler = (requestHandlerT)0;
					proGasDestroyHandler(gas, sizeof(req), &req, 0);
					gas = 0;
			}
			if (gas == 0) {
				proGasReqT req;
				proGasCreateRspT rsp;

				if (gasDecode.action !=
					GAS_REQUEST_ACTION_FRAME &&
					gasDecode.action !=
					GAS_COMEBACK_REQUEST_ACTION_FRAME) {
					WL_PRMAC("no instance found", src);
					return;
				}

				if (!gIncomingRequest) {
					WL_ERROR(("incoming GAS request support disabled\n"));
					return;
				}

#ifndef BCMDRIVER
				/* router supports multiple physical interfaces */
				drv = wl_getifbyname(wlEvent->ifname);
				if (!drv) {
					WL_ERROR(("failed to find ifname %s\n",
						wlEvent->ifname));
					return;
				}
#endif /* BCMDRIVER */
				WL_P2PO(("creating incoming instance\n"));
				req.handler = (requestHandlerT)0;
				req.create.isIncoming = TRUE;
				req.create.drv = drv;
				req.create.bsscfgIndex = wlEvent->bsscfgidx;
				req.create.channel = channel;
				memcpy(&req.create.peer, src->octet,
					sizeof(req.create.peer));
				proGasCreateHandler(0, sizeof(req), &req, &rsp);
				gas = rsp.gas;
				if (gas == 0) {
					WL_ERROR(("failed to create instance\n"));
					return;
				}
			}

			rxNotification(gas, &gasDecode, actFrameLen);
			fsm(gas, event, &gasDecode, 0, 0, 0);
		}

		break;
	}
	default:
	{
	}
		break;
	}
}

/* reset all variables going into idle state */
static void idleReset(proGasT *gas)
{
	/* free previous query response */
	if (gas->txResponse.data) {
		free(gas->txResponse.data);
		gas->txResponse.data = 0;
	}

	/* stop timers */
	tmrStop(gas->responseTimer);
	tmrStop(gas->comebackDelayTimer);
	tmrStop(gas->queryResponseTimer);

	/* reset variables */
	gas->nextState = 0;
	gas->statusCode = DOT11_SC_SUCCESS;
	gas->isPendingStatusNotification = FALSE;
	gas->retransmit = 0;
	memset(&gas->tx, 0, sizeof(gas->tx));
	gas->txResponse.fragmentId = 0;
	gas->rxResponse.fragmentId = 0;
}

static void changeState(proGasT *gas, stateT next)
{
	/* state transition occurs at the end of fsm() processing */
	gas->nextState = next;
}

static void queryRequestNotification(proGasT *gas, uint16 reqLen, uint8 *req)
{
	if (gEventCallback.fn) {
		proGasEventT event;

		WL_PRPKT("query request notification", req, reqLen);
		event.gas = gas;
		memcpy(&event.peer, &gas->peer, sizeof(event.peer));
		event.dialogToken = gas->dialogToken;
		event.type = PRO_GAS_EVENT_QUERY_REQUEST;
		event.queryReq.len = reqLen;
		if (event.queryReq.len > MAX_QUERY_REQUEST_LENGTH) {
			WL_ERROR(("truncating query request to %d\n",
				MAX_QUERY_REQUEST_LENGTH));
			event.queryReq.len = MAX_QUERY_REQUEST_LENGTH;
		}
		memcpy(event.queryReq.data, req, event.queryReq.len);
		gEventCallback.fn(gEventCallback.context, gas, &event);
	}
}

static void txNotification(proGasT *gas, int gasActionFrame)
{
	if (gEventCallback.fn) {
		proGasEventT event;

		WL_TRACE(("tx notification = %d\n", gasActionFrame));
		event.gas = gas;
		memcpy(&event.peer, &gas->peer, sizeof(event.peer));
		event.dialogToken = gas->dialogToken;
		event.type = PRO_GAS_EVENT_TX;
		event.tx.gasActionFrame = gasActionFrame;
		event.tx.length = gas->tx.length;
		event.tx.fragmentId =
			gas->txResponse.fragmentId == gas->txResponse.lastFragmentId ?
			gas->txResponse.fragmentId :
			gas->txResponse.fragmentId | MORE_BIT;
		gEventCallback.fn(gEventCallback.context, gas, &event);
	}
}

static void rxNotification(proGasT *gas, pktGasDecodeT *gasDecode, int length)
{
	if (gEventCallback.fn) {
		proGasEventT event;
		uint8 fragmentId = 0;

		WL_TRACE(("rx notification = %d\n", gasDecode->action));

		if (gasDecode->action == GAS_COMEBACK_RESPONSE_ACTION_FRAME)
			fragmentId = gasDecode->comebackResponse.fragmentId;

		event.gas = gas;
		memcpy(&event.peer, &gas->peer, sizeof(event.peer));
		event.dialogToken = gasDecode->dialogToken;
		event.type = PRO_GAS_EVENT_RX;
		event.rx.gasActionFrame = gasDecode->action;
		event.rx.length = length;
		event.rx.fragmentId = fragmentId;
		gEventCallback.fn(gEventCallback.context, gas, &event);
	}
}

static void responseFragmentNotification(proGasT *gas, int length, uint8 fragmentId)
{
	if (gEventCallback.fn) {
		proGasEventT event;

		WL_TRACE(("response fragment notification = %d\n", fragmentId));
		event.gas = gas;
		memcpy(&event.peer, &gas->peer, sizeof(event.peer));
		event.dialogToken = gas->dialogToken;
		event.type = PRO_GAS_EVENT_RESPONSE_FRAGMENT;
		event.rspFragment.length = length;
		event.rspFragment.fragmentId = fragmentId;
		gEventCallback.fn(gEventCallback.context, gas, &event);
	}
}

static void statusNotification(proGasT *gas)
{
	if (gEventCallback.fn) {
		proGasEventT event;

		WL_TRACE(("status notification = %d\n", gas->statusCode));
		event.gas = gas;
		memcpy(&event.peer, &gas->peer, sizeof(event.peer));
		event.dialogToken = gas->dialogToken;
		event.type = PRO_GAS_EVENT_STATUS;
		event.status.statusCode = gas->statusCode;
		gas->isPendingStatusNotification = FALSE;
		gEventCallback.fn(gEventCallback.context, gas, &event);
	}
}

static void setStatusNotification(proGasT *gas, uint16 statusCode)
{
	gas->statusCode = statusCode;
	gas->isPendingStatusNotification = TRUE;
}

static void success(proGasT *gas)
{
	WL_TRACE(("SUCCESS - returning to IDLE\n"));
	idleReset(gas);
	changeState(gas, STATE_IDLE);
	setStatusNotification(gas, DOT11_SC_SUCCESS);
}

static void fail(proGasT *gas, uint16 statusCode)
{
	WL_TRACE(("FAILED - returning to IDLE\n"));
	idleReset(gas);
	changeState(gas, STATE_IDLE);
	setStatusNotification(gas, statusCode);
}

static void deleteResponseData(proGasT *gas)
{
	WL_TRACE(("deleting previous reponse data\n"));
	gas->rxResponse.isValid = FALSE;
	if (gas->rxResponse.data) {
		free(gas->rxResponse.data);
		gas->rxResponse.data = 0;
	}
	gas->rxResponse.maxLength = 0;
	gas->rxResponse.length = 0;
}

static void saveResponseData(proGasT *gas, uint32 length, uint8 *data)
{
	WL_TRACE(("save response data %d bytes\n", length));

	if (gFragmentResponse) {
		if (gas->rxResponse.length != 0) {
			WL_ERROR(("previous fragment response not retrieved\n"));
			return;
		}
	}

	/* alloc memory if doesn't fit in buffer */
	if (length > gas->rxResponse.maxLength - gas->rxResponse.length) {
		/* allow some reassembly headroom to reduce alloc overhead */
		gas->rxResponse.maxLength += gFragmentResponse ?
			length : NUM_FRAGMENT_PER_REALLOC * length;
#ifdef BCM_GAS_NO_REASSEMBLY
		WL_TRACE(("malloc %d bytes\n", gas->rxResponse.maxLength));
		gas->rxResponse.data = malloc(gas->rxResponse.maxLength);
#else
		WL_TRACE(("realloc %d bytes\n", gas->rxResponse.maxLength));
		gas->rxResponse.data = realloc(gas->rxResponse.data,
			gas->rxResponse.maxLength);
#endif
		if (gas->rxResponse.data == 0) {
			WL_ERROR(("realloc failed\n"));
			return;
		}
	}

	memcpy(&gas->rxResponse.data[gas->rxResponse.length], data, length);
	gas->rxResponse.length += length;
	WL_TRACE(("response data %d bytes\n", gas->rxResponse.length));
}

/* common event processing for all states */
static void defaultEventProcessing(proGasT *gas, eventT event, pktGasDecodeT *gasDecode)
{
	(void)gasDecode;

	switch (event) {
	case EVENT_RESET:
		WL_TRACE(("defaultEventProcessing: EVENT_RESET\n"));
		/* reset dialog token and variables */
		gas->state = 0;
		idleReset(gas);
		changeState(gas, STATE_IDLE);
		break;
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		break;
	case EVENT_RX_QUERY_RESPONSE:
		break;
	case EVENT_RESPONSE_TIMEOUT:
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		break;
	default:
		WL_ERROR(("invalid event %d\n", event));
		break;
	}
}

static void encodeAdvertisementProtocol(pktEncodeT *pkt,
	int isPamebi, uint8 qResponseLimit, uint8 advertisementProtocol)
{
	uint8 buffer[32];
	pktEncodeT ap;

	pktEncodeInit(&ap, sizeof(buffer), buffer);
	pktEncodeIeAdvertisementProtocolTuple(&ap, isPamebi,
		qResponseLimit, advertisementProtocol);
	pktEncodeIeAdvertiseProtocol(pkt, pktEncodeLength(&ap), pktEncodeBuf(&ap));
}

static int encodeGasRequest(int maxLength, uint8 *buf,
	uint8 dialogToken, uint8 advertisementProtocol,
	uint16 reqLen, uint8 *req)
{
	uint8 buffer[32];
	pktEncodeT apie;
	pktEncodeT enc;
	int length;

	/* encode advertisement protocol IE */
	pktEncodeInit(&apie, sizeof(buffer), buffer);
	encodeAdvertisementProtocol(&apie, ADVP_PAME_BI_DEPENDENT,
		ADVP_QRL_REQUEST, advertisementProtocol);

	/* encode GAS request */
	pktEncodeInit(&enc, maxLength, buf);
	length = pktEncodeGasRequest(&enc, dialogToken,
		pktEncodeLength(&apie), pktEncodeBuf(&apie), reqLen, req);

	return length;
}

static int encodeGasResponse(int maxLength, uint8 *buf,
	uint8 dialogToken, uint16 statusCode, uint16 comebackDelay,
	uint8 advertisementProtocol, uint16 rspLen, uint8 *rsp)
{
	uint8 buffer[32];
	pktEncodeT apie;
	pktEncodeT enc;
	int length;

	/* encode advertisement protocol IE */
	pktEncodeInit(&apie, sizeof(buffer), buffer);
	encodeAdvertisementProtocol(&apie, ADVP_PAME_BI_DEPENDENT,
		ADVP_QRL_RESPONSE, advertisementProtocol);

	/* encode GAS response */
	pktEncodeInit(&enc, maxLength, buf);
	length = pktEncodeGasResponse(&enc, dialogToken, statusCode, comebackDelay,
		pktEncodeLength(&apie), pktEncodeBuf(&apie), rspLen, rsp);

	return length;
}

static int encodeGasResponseFail(int maxLength, uint8 *buf,
	uint8 dialogToken, uint16 statusCode, uint8 id)
{
	uint8 buffer[32];
	pktEncodeT apie;
	pktEncodeT enc;
	int length;

	/* encode advertisement protocol IE */
	pktEncodeInit(&apie, sizeof(buffer), buffer);
	encodeAdvertisementProtocol(&apie, ADVP_PAME_BI_DEPENDENT,
		ADVP_QRL_RESPONSE, id);

	/* encode GAS response */
	pktEncodeInit(&enc, maxLength, buf);
	length = pktEncodeGasResponse(&enc, dialogToken, statusCode, 0,
		pktEncodeLength(&apie), pktEncodeBuf(&apie), 0, 0);

	return length;
}

static int encodeGasComebackRequest(int maxLength, uint8 *buf,
	uint8 dialogToken)
{
	pktEncodeT enc;
	int length;

	/* encode GAS comeback request */
	pktEncodeInit(&enc, maxLength, buf);
	length = pktEncodeGasComebackRequest(&enc, dialogToken);

	return length;
}

static int encodeGasComebackResponse(int maxLength, uint8 *buf,
	uint8 dialogToken, uint16 statusCode, uint8 fragmentId, uint16 comebackDelay,
	uint8 advertisementProtocol, uint16 rspLen, uint8 *rsp)
{
	uint8 buffer[32];
	pktEncodeT apie;
	pktEncodeT enc;
	int length;

	/* encode advertisement protocol IE */
	pktEncodeInit(&apie, sizeof(buffer), buffer);
	encodeAdvertisementProtocol(&apie, ADVP_PAME_BI_DEPENDENT,
		ADVP_QRL_RESPONSE, advertisementProtocol);

	/* encode GAS comeback response */
	pktEncodeInit(&enc, maxLength, buf);
	length = pktEncodeGasComebackResponse(&enc, dialogToken,
		statusCode, fragmentId, comebackDelay,
		pktEncodeLength(&apie), pktEncodeBuf(&apie), rspLen, rsp);

	return length;
}

static void txActionFrame(proGasT *gas,	uint32 channel, uint32 responseTimeout,
	struct ether_addr *bssid, struct ether_addr *dst, int gasActionFrame,
	int length, uint8 *buffer)
{
	/* reset retransmit count */
	gas->retransmit = 0;

	/* save tx parameters for retransmit */
	gas->tx.channel = channel;
	gas->tx.responseTimeout = responseTimeout;
	memcpy(&gas->tx.bssid, bssid, sizeof(gas->tx.bssid));
	memcpy(&gas->tx.dst, dst, sizeof(gas->tx.dst));
	gas->tx.gasActionFrame = gasActionFrame;

	WL_PRPKT("TX action frame",	gas->tx.data, gas->tx.length);

	if (wl_actframe(gas->drv, gas->bsscfgIndex,
		(uint32)gas, gas->tx.channel, gas->tx.responseTimeout,
		&gas->tx.bssid, &gas->tx.dst, length, buffer) != 0) {
		WL_ERROR(("wl_actframe failed\n"));
	}
	txNotification(gas, gas->tx.gasActionFrame);
	if (gas->tx.responseTimeout > 0) {
		tmrStart(gas->responseTimer, gas->tx.responseTimeout, FALSE);
	}
}

static void retransmit(proGasT *gas)
{
	if (gas->retransmit < gas->maxRetransmit) {
		WL_PRPKT("TX action frame (retransmit)", gas->tx.data, gas->tx.length);

		if (wl_actframe(gas->drv, gas->bsscfgIndex,
			(uint32)gas, gas->tx.channel, gas->tx.responseTimeout,
			&gas->tx.bssid, &gas->tx.dst,
			gas->tx.length, gas->tx.data) != 0) {
			WL_ERROR(("wl_actframe failed\n"));
		}
		gas->retransmit++;
		txNotification(gas, gas->tx.gasActionFrame);
		if (gas->tx.responseTimeout > 0) {
			tmrStart(gas->responseTimer, gas->tx.responseTimeout, FALSE);
		}
	}
	else {
		WL_P2PO(("max retransmits %d\n", gas->maxRetransmit));
		fail(gas, DOT11_SC_TRANSMIT_FAILURE);
	}
}

static void stateIdle(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		deleteResponseData(gas);
		/* assign dialog token */
		gas->dialogToken = gNextDialogToken++;
		/* transmit GAS request */
		gas->tx.length = encodeGasRequest(sizeof(gas->tx.data), gas->tx.data,
			gas->dialogToken, gas->advertisementProtocol,
			gas->txRequest.length, gas->txRequest.data);
		txActionFrame(gas, gas->channel, gas->responseTimeout, &gas->peer,
			&gas->peer, GAS_REQUEST_ACTION_FRAME, gas->tx.length, gas->tx.data);
		changeState(gas, STATE_TX_REQUEST);
		break;
	case EVENT_RX_REQUEST:
	{
		/* save dialog token from request */
		gas->dialogToken = gasDecode->dialogToken;

		if (!isValidAdvertisementProtocol(&gasDecode->request.apie)) {
			/* transmit failed GAS response */
			gas->statusCode = DOT11_SC_ADV_PROTO_NOT_SUPPORTED;
			gas->tx.length = encodeGasResponseFail(sizeof(gas->tx.data),
				gas->tx.data, gas->dialogToken, gas->statusCode,
				gasDecode->request.apie.protocolId);
			txActionFrame(gas, gas->channel, 0, &gas->mac,
				&gas->peer, GAS_RESPONSE_ACTION_FRAME,
				gas->tx.length, gas->tx.data);
			changeState(gas, STATE_ERROR);
		}
		else {
			/* send request to server */
			queryRequestNotification(gas,
				gasDecode->request.reqLen, gasDecode->request.req);
#ifndef BCM_GAS_NO_DISPATCHER
			tmrStart(gas->queryResponseTimer, QUERY_RESPONSE_TIMEOUT, FALSE);
#endif	/* BCM_GAS_NO_DISPATCHER */

			if (getGasPauseForServerResponse(gas->drv)) {
#ifndef BCM_GAS_NO_DISPATCHER
				changeState(gas, STATE_WAIT_FOR_QUERY_RESPONSE);
#else
				stateWaitQueryResponseProcessQueryResponse(gas);
#endif	/* BCM_GAS_NO_DISPATCHER */
			}
			else {
				int comebackDelay = getComebackDelayResponseUnpause(gas->drv);
				/* transmit GAS response */
				gas->tx.length = encodeGasResponse(sizeof(gas->tx.data),
					gas->tx.data, gas->dialogToken, DOT11_SC_SUCCESS,
					comebackDelay, gas->advertisementProtocol, 0, 0);
				txActionFrame(gas, gas->channel,
					comebackDelay + gas->responseTimeout,
					&gas->mac, &gas->peer, GAS_RESPONSE_ACTION_FRAME,
					gas->tx.length, gas->tx.data);
				changeState(gas, STATE_TX_RESPONSE_AND_QUERY_REQUEST);
			}
		}
	}
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		/* save dialog token from request */
		gas->dialogToken = gasDecode->dialogToken;
		/* transmit failed GAS comeback response */
		gas->statusCode = DOT11_SC_NO_OUTSTAND_REQ;
		gas->tx.length = encodeGasComebackResponse(
			sizeof(gas->tx.data), gas->tx.data, gas->dialogToken,
			gas->statusCode, 0, 0, gas->advertisementProtocol, 0, 0);
		txActionFrame(gas, gas->channel, 0, &gas->mac, &gas->peer,
			GAS_COMEBACK_RESPONSE_ACTION_FRAME, gas->tx.length, gas->tx.data);
		changeState(gas, STATE_ERROR);
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		break;
	case EVENT_RX_QUERY_RESPONSE:
		break;
	case EVENT_RESPONSE_TIMEOUT:
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

static void stateTxRequest(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		tmrStop(gas->responseTimer);
		if (gasDecode->dialogToken != gas->dialogToken) {
			WL_P2PO(("dialog token mismatch %d != %d\n",
				gasDecode->dialogToken, gas->dialogToken));
			fail(gas, DOT11_SC_NO_OUTSTAND_REQ);
		}
		else {
			if (gasDecode->response.statusCode != DOT11_SC_SUCCESS) {
				fail(gas, gasDecode->response.statusCode);
			}
			else {
				if (gasDecode->response.comebackDelay == 0) {
					/* data in response */
					saveResponseData(gas,
						gasDecode->response.rspLen,
						gasDecode->response.rsp);
					gas->rxResponse.isValid = TRUE;
					success(gas);
				}
				else if (gasDecode->response.comebackDelay <=
					gas->maxComebackDelay) {
					/* data fragmented */
					tmrStart(gas->comebackDelayTimer,
						gComebackDelayOverride == 0 ?
						gasDecode->response.comebackDelay :
						gComebackDelayOverride, FALSE);
					changeState(gas, STATE_WAIT_COMEBACK_DELAY);
				}
				else {
					/* fail due to comeback delay exceeded */
					fail(gas, DOT11_SC_FAILURE);
				}
			}
		}
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		break;
	case EVENT_RX_QUERY_RESPONSE:
		break;
	case EVENT_RESPONSE_TIMEOUT:
		fail(gas, DOT11_SC_TIMEOUT);
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		retransmit(gas);
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

static void rxComebackRequest(proGasT *gas, pktGasDecodeT *gasDecode)
{
	tmrStop(gas->responseTimer);
	if (gasDecode->dialogToken != gas->dialogToken) {
		WL_P2PO(("dialog token mismatch %d != %d\n",
			gasDecode->dialogToken, gas->dialogToken));
		fail(gas, DOT11_SC_NO_OUTSTAND_REQ);
	}
	else {
		uint32 sent;
		int more;
		uint16 statusCode = 0;
		uint8 fragmentId;
		uint16 comebackDelay = 0;
		uint16 rspLen;
		uint8 *rsp;

		/* determine query data size */
		sent = gas->txResponse.fragmentId * MAX_FRAGMENT_SIZE;
		if (gas->txResponse.length - sent > MAX_FRAGMENT_SIZE) {
			more = TRUE;
			rspLen = MAX_FRAGMENT_SIZE;
		}
		else {
			more = FALSE;
			rspLen = gas->txResponse.length - sent;
		}
		rsp = &gas->txResponse.data[sent];

		if (more)
			fragmentId = gas->txResponse.fragmentId | MORE_BIT;
		else
			fragmentId = gas->txResponse.fragmentId & ~MORE_BIT;

		/* transmit GAS comeback response */
		gas->tx.length = encodeGasComebackResponse(
			sizeof(gas->tx.data), gas->tx.data, gas->dialogToken,
			statusCode, fragmentId, comebackDelay, gas->advertisementProtocol,
			rspLen, rsp);
		txActionFrame(gas, gas->channel, 0, &gas->mac, &gas->peer,
			GAS_COMEBACK_RESPONSE_ACTION_FRAME,
			gas->tx.length, gas->tx.data);
		changeState(gas, STATE_TX_COMEBACK_RESPONSE);
	}
}

static void stateTxResponse(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		rxComebackRequest(gas, gasDecode);
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		break;
	case EVENT_RX_QUERY_RESPONSE:
		break;
	case EVENT_RESPONSE_TIMEOUT:
		fail(gas, DOT11_SC_TIMEOUT);
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		if (gas->txResponse.lastFragmentId == 0) {
			tmrStop(gas->responseTimer);
			success(gas);
		}
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		retransmit(gas);
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

static void rxComebackResponse(proGasT *gas, pktGasDecodeT *gasDecode)
{
	tmrStop(gas->responseTimer);
	if (gasDecode->dialogToken != gas->dialogToken) {
		WL_P2PO(("dialog token mismatch %d != %d\n",
			gasDecode->dialogToken, gas->dialogToken));
		fail(gas, DOT11_SC_NO_OUTSTAND_REQ);
	}
	else if (gasDecode->comebackResponse.statusCode != DOT11_SC_SUCCESS) {
		fail(gas, gasDecode->comebackResponse.statusCode);
	}
	else {
		int isMore = gasDecode->comebackResponse.fragmentId & MORE_BIT ? TRUE : FALSE;
		uint8 fragmentId = gasDecode->comebackResponse.fragmentId & ~MORE_BIT;

		if (isMore) {
			if (gas->rxResponse.fragmentId == fragmentId) {

				/* save response data */
				saveResponseData(gas,
					gasDecode->comebackResponse.rspLen,
					gasDecode->comebackResponse.rsp);

				if (gFragmentResponse) {
					responseFragmentNotification(gas,
					gasDecode->comebackResponse.rspLen,
					gasDecode->comebackResponse.fragmentId);
				}

				if (gasDecode->comebackResponse.comebackDelay == 0) {
					/* transmit GAS comeback request */
					gas->tx.length = encodeGasComebackRequest(
						sizeof(gas->tx.data), gas->tx.data,
						gas->dialogToken);
					txActionFrame(gas, gas->channel,
						gas->responseTimeout,
						&gas->peer, &gas->peer,
						GAS_COMEBACK_REQUEST_ACTION_FRAME,
						gas->tx.length, gas->tx.data);
					changeState(gas, STATE_TX_COMEBACK_REQUEST);
				}
				else if (gasDecode->comebackResponse.comebackDelay <=
					gas->maxComebackDelay) {
					/* wait the comeback delay */
					tmrStart(gas->comebackDelayTimer,
						gComebackDelayOverride == 0 ?
						gasDecode->comebackResponse.comebackDelay :
						gComebackDelayOverride,
						FALSE);
					changeState(gas, STATE_WAIT_COMEBACK_DELAY);
				}
				else {
					/* fail due to comeback delay exceeded */
					fail(gas, DOT11_SC_FAILURE);
				}

				/* next expected rx fragment id */
				gas->rxResponse.fragmentId++;
			}
			else if (gas->rxResponse.fragmentId < fragmentId) {
				WL_P2PO(("expecting fragment id %d, receive %d\n",
					gas->rxResponse.fragmentId, fragmentId));
				fail(gas, DOT11_SC_NO_OUTSTAND_REQ);
			}
			else {
				/* already received fragment ID */
				WL_P2PO(("expecting fragment id %d, receive %d\n",
					gas->rxResponse.fragmentId, fragmentId));

				/* transmit GAS comeback request */
				gas->tx.length = encodeGasComebackRequest(
					sizeof(gas->tx.data), gas->tx.data,
					gas->dialogToken);
				txActionFrame(gas, gas->channel,
					gas->responseTimeout,
					&gas->peer, &gas->peer,
					GAS_COMEBACK_REQUEST_ACTION_FRAME,
					gas->tx.length, gas->tx.data);
				changeState(gas, STATE_TX_COMEBACK_REQUEST);
			}
		}
		else {
			/* save last response data */
			saveResponseData(gas,
				gasDecode->comebackResponse.rspLen,
				gasDecode->comebackResponse.rsp);
			gas->rxResponse.isValid = TRUE;
			success(gas);
		}
	}
}

static void stateTxComebackRequest(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		rxComebackResponse(gas, gasDecode);
		break;
	case EVENT_RX_QUERY_RESPONSE:
		break;
	case EVENT_RESPONSE_TIMEOUT:
		fail(gas, DOT11_SC_TIMEOUT);
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		retransmit(gas);
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

static void stateTxComebackResponse(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		rxComebackRequest(gas, gasDecode);
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		break;
	case EVENT_RX_QUERY_RESPONSE:
		break;
	case EVENT_RESPONSE_TIMEOUT:
		fail(gas, DOT11_SC_TIMEOUT);
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		WL_P2PO(("fragment ID=%d last=%d\n",
			gas->txResponse.fragmentId, gas->txResponse.lastFragmentId));
		if (gas->txResponse.fragmentId == gas->txResponse.lastFragmentId) {
			tmrStop(gas->responseTimer);
			success(gas);
		}
		else {
			gas->txResponse.fragmentId++;
		}
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		retransmit(gas);
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

static void rxQueryResponse(proGasT *gas, int dataLen, uint8 *data)
{
	/* free previous query response */
	if (gas->txResponse.data) {
		free(gas->txResponse.data);
		gas->txResponse.data = 0;
	}

	/* copy query response */
	gas->txResponse.fragmentId = 0;
	gas->txResponse.length = dataLen;
	gas->txResponse.lastFragmentId =
		gas->txResponse.length / MAX_FRAGMENT_SIZE +
		(gas->txResponse.length % MAX_FRAGMENT_SIZE == 0 ? 0 : 1) - 1;
	gas->txResponse.data = malloc(gas->txResponse.length);
	if (gas->txResponse.data == 0)
		gas->txResponse.length = 0;
	else
		memcpy(gas->txResponse.data, data, gas->txResponse.length);
}

static void stateTxResponseAndQueryRequest(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		tmrStop(gas->responseTimer);
		if (gas->txResponse.length > 0) {
			rxComebackRequest(gas, gasDecode);
		}
		else {
			/* transmit GAS comeback response with no server response status */
			gas->statusCode = DOT11_SC_RSP_NOT_RX_FROM_SERVER;
			gas->tx.length = encodeGasComebackResponse(sizeof(gas->tx.data),
				gas->tx.data, gas->dialogToken,	gas->statusCode, 0, 0,
				gas->advertisementProtocol, 0, 0);
			txActionFrame(gas, gas->channel, 0, &gas->mac,
				&gas->peer, GAS_COMEBACK_RESPONSE_ACTION_FRAME,
				gas->tx.length, gas->tx.data);
			changeState(gas, STATE_ERROR);
		}
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		break;
	case EVENT_RX_QUERY_RESPONSE:
		tmrStop(gas->queryResponseTimer);
		rxQueryResponse(gas, dataLen, data);
		break;
	case EVENT_RESPONSE_TIMEOUT:
		fail(gas, DOT11_SC_TIMEOUT);
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		tmrStop(gas->responseTimer);
		/* transmit GAS comeback response with timeout status */
		gas->statusCode = DOT11_SC_RSP_NOT_RX_FROM_SERVER;
		gas->tx.length = encodeGasComebackResponse(sizeof(gas->tx.data), gas->tx.data,
			gas->dialogToken, gas->statusCode, 0, 0, gas->advertisementProtocol, 0, 0);
		txActionFrame(gas, gas->channel, 0, &gas->mac,
			&gas->peer, GAS_COMEBACK_RESPONSE_ACTION_FRAME,
			gas->tx.length, gas->tx.data);
		changeState(gas, STATE_ERROR);
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		/* check if query response is available */
		if (gas->txResponse.length > 0) {
			changeState(gas, STATE_TX_RESPONSE);
		}
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		retransmit(gas);
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

static void stateWaitQueryResponseProcessQueryResponse(proGasT *gas)
{
	uint16 statusCode = DOT11_SC_SUCCESS;
	uint16 comebackDelay;
	uint16 rspLen;
	uint8 *rsp;

	/* determine query data size */
	if (gas->txResponse.length <= MAX_FRAGMENT_SIZE) {
		comebackDelay = 0;
		rspLen = gas->txResponse.length;
		rsp = gas->txResponse.data;
	}
	else {
		comebackDelay = gComebackDelayResponsePause;
		rspLen = 0;
		rsp = 0;
		gas->txResponse.fragmentId = 0;
	}

	/* transmit GAS response */
	gas->tx.length = encodeGasResponse(sizeof(gas->tx.data), gas->tx.data,
		gas->dialogToken, statusCode, comebackDelay, gas->advertisementProtocol,
		rspLen, rsp);
	txActionFrame(gas, gas->channel,
		comebackDelay == 0 ? 0 : comebackDelay + gas->responseTimeout,
		&gas->mac, &gas->peer, GAS_RESPONSE_ACTION_FRAME,
		gas->tx.length, gas->tx.data);
	changeState(gas, STATE_TX_RESPONSE);
}

static void stateWaitQueryResponse(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)gasDecode;
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		break;
	case EVENT_RX_QUERY_RESPONSE:
		tmrStop(gas->queryResponseTimer);
		rxQueryResponse(gas, dataLen, data);
		stateWaitQueryResponseProcessQueryResponse(gas);
		break;
	case EVENT_RESPONSE_TIMEOUT:
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		/* transmit GAS response with timeout status */
		gas->statusCode = DOT11_SC_RSP_NOT_RX_FROM_SERVER;
		gas->tx.length = encodeGasResponse(sizeof(gas->tx.data), gas->tx.data,
			gas->dialogToken, gas->statusCode, 0, gas->advertisementProtocol, 0, 0);
		txActionFrame(gas, gas->channel, 0, &gas->mac,
			&gas->peer, GAS_RESPONSE_ACTION_FRAME,
			gas->tx.length, gas->tx.data);
		changeState(gas, STATE_ERROR);
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

static void stateWaitComebackDelay(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)gasDecode;
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		/* due to comeback request retransmits - possible to receive event in this state */
		tmrStop(gas->comebackDelayTimer);
		rxComebackResponse(gas, gasDecode);
		break;
	case EVENT_RX_QUERY_RESPONSE:
		break;
	case EVENT_RESPONSE_TIMEOUT:
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		/* transmit GAS comeback request */
		gas->tx.length = encodeGasComebackRequest(
			sizeof(gas->tx.data), gas->tx.data, gas->dialogToken);
		txActionFrame(gas, gas->channel, gas->responseTimeout, &gas->peer,
			&gas->peer, GAS_COMEBACK_REQUEST_ACTION_FRAME,
			gas->tx.length, gas->tx.data);
		changeState(gas, STATE_TX_COMEBACK_REQUEST);
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

static void stateError(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	(void)dataLen;
	(void)data;
	(void)len;

	switch (event) {
	case EVENT_CONFIGURE:
		break;
	case EVENT_START:
		break;
	case EVENT_RX_REQUEST:
		break;
	case EVENT_RX_RESPONSE:
		break;
	case EVENT_RX_COMEBACK_REQUEST:
		break;
	case EVENT_RX_COMEBACK_RESPONSE:
		break;
	case EVENT_RX_QUERY_RESPONSE:
		break;
	case EVENT_RESPONSE_TIMEOUT:
		break;
	case EVENT_COMEBACK_DELAY_TIMEOUT:
		break;
	case EVENT_QUERY_RESPONSE_TIMEOUT:
		break;
	case EVENT_ACTION_FRAME_TX_SUCCESS:
		fail(gas, gas->statusCode);
		break;
	case EVENT_ACTION_FRAME_TX_FAILED:
		retransmit(gas);
		break;
	default:
		defaultEventProcessing(gas, event, gasDecode);
		break;
	}
}

/* GAS protocol finite state machine */
static void fsm(proGasT *gas, eventT event,
	pktGasDecodeT *gasDecode, int dataLen, uint8 *data, int *len)
{
	WL_P2PO(("--------------------------------------------------------\n"));
	WL_P2PO(("current state=%s event=%s\n",
		state_str[gas->state], event_str[event]));

	switch (gas->state) {
	case STATE_IDLE:
		stateIdle(gas, event, gasDecode, dataLen, data, len);
		break;
	case STATE_TX_REQUEST:
		stateTxRequest(gas, event, gasDecode, dataLen, data, len);
		break;
	case STATE_TX_RESPONSE:
		stateTxResponse(gas, event, gasDecode, dataLen, data, len);
		break;
	case STATE_TX_COMEBACK_REQUEST:
		stateTxComebackRequest(gas, event, gasDecode, dataLen, data, len);
		break;
	case STATE_TX_COMEBACK_RESPONSE:
		stateTxComebackResponse(gas, event, gasDecode, dataLen, data, len);
		break;
	case STATE_TX_RESPONSE_AND_QUERY_REQUEST:
		stateTxResponseAndQueryRequest(gas, event, gasDecode, dataLen, data, len);
		break;
	case STATE_WAIT_FOR_QUERY_RESPONSE:
		stateWaitQueryResponse(gas, event, gasDecode, dataLen, data, len);
		break;
	case STATE_WAIT_COMEBACK_DELAY:
		stateWaitComebackDelay(gas, event, gasDecode, dataLen, data, len);
		break;
	case STATE_ERROR:
		stateError(gas, event, gasDecode, dataLen, data, len);
		break;
	default:
		WL_ERROR(("invalid state %d\n", gas->state));
		break;
	}

	/* transition to next state */
	if (gas->state != gas->nextState) {
		int isIncoming = gas->isIncoming;

		WL_P2PO(("state change %s -> %s\n",
			state_str[gas->state], state_str[gas->nextState]));
		gas->state = gas->nextState;

		if (gas->isPendingStatusNotification && gas->state == STATE_IDLE) {
			/* gas instance may be destroyed during
			 * the status notification callback
			 */
			statusNotification(gas);
		}

		/* destroy incoming on return to idle */
		if (isIncoming && gas->state == STATE_IDLE) {
			proGasReqT req;

			WL_P2PO(("destroying incoming instance\n"));
			req.handler = (requestHandlerT)0;
			proGasDestroyHandler(gas, sizeof(req), &req, 0);
		}
	}

	WL_P2PO(("--------------------------------------------------------\n"));
}
