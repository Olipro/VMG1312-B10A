/*
 * WiFi-Direct discovery state machine.
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
#include <time.h>
#include "bcmendian.h"
#ifndef BCMDRIVER
#include "dsp.h"
#include "tmr.h"
#include "wlu_api.h"
#else
#include "wlc_p2po_disc.h"
#endif /* BCMDRIVER */
#ifdef BCMDBG_ESCAN
#include "pktDecodeIe.h"
#endif /* BCMDBG_ESCAN */
#include "trace.h"
#include "proWfdDisc.h"

#ifdef BCMDRIVER
#define BCM_WFD_DISC_NO_DISPATCHER			/* direct function call */
#else
/* create discovery bsscfg (else host to create) */
#define BCM_WFD_DISC_CREATE_DISCOVERY_BSSCFG

/* add and delete IEs (else host to add/del) */
#define BCM_WFD_DISC_ADD_DELETE_IES
#endif

/* 802.11 scan at the start of discovery (else scan after search) */
#undef BCM_WFD_DISC_SCAN_AT_START_OF_DISCOVERY

/* number of searches before a full scan */
#define SEARCHES_PER_SCAN	32

/* state machine states */
typedef enum {
	STATE_IDLE,
	STATE_SCAN,
	STATE_LISTEN,
	STATE_SEARCH,
	STATE_EXT_LISTEN_ON,
	STATE_EXT_LISTEN_OFF
} stateT;

#ifdef BCMDBG
/* state to string */
static char *state_str[] = {
	"STATE_IDLE",
	"STATE_SCAN",
	"STATE_LISTEN",
	"STATE_SEARCH",
	"STATE_EXT_LISTEN_ON",
	"STATE_EXT_LISTEN_OFF"
};
#endif	/* BCMDBG */

/* state machine events */
typedef enum {
	EVENT_RESET,
	EVENT_START_DISCOVERY,
	EVENT_START_EXT_LISTEN,
	EVENT_SCAN_COMPLETE,
	EVENT_LISTEN_TIMEOUT
} eventT;

#ifdef BCMDBG
/* event to string */
static char *event_str[] = {
	"EVENT_RESET",
	"EVENT_START_DISCOVERY",
	"EVENT_START_EXT_LISTEN",
	"EVENT_SCAN_COMPLETE",
	"EVENT_LISTEN_TIMEOUT"
};
#endif	/* BCMDBG */

#define NUM_SOCIAL_CHANNEL	3

struct proWfdDiscStruct
{
	/* wl driver handle */
	struct wl_drv_hdl *drv;

	/* bsscfg index */
	int bsscfgIndex;

#ifdef BCM_WFD_DISC_ADD_DELETE_IES
	/* discovery ethernet address */
	struct ether_addr addr;
#endif	/* BCM_WFD_DISC_ADD_DELETE_IES */

	/* channel */
	uint16 listenChannel;

	/* exteneded listen */
	uint16 extListenOnTimeout;
	uint16 extListenOffTimeout;

	/* social channels */
	uint16 socialChannel[NUM_SOCIAL_CHANNEL];

	/* timers */
	tmrT *listenTimer;

	/* escan sync id */
	uint16 sync_id;

	/* FSM state */
	stateT state;
	stateT nextState;

	/* search counter */
	uint32 searchCount;
};

/* single instance */
static proWfdDiscT gDisc;

typedef struct proWfdDiscReq proWfdDiscReqT;

/* request handler */
typedef void (*requestHandlerT)(proWfdDiscT *disc,
	int reqLength, proWfdDiscReqT *req, void *rspData);

typedef struct {
	struct wl_drv_hdl *drv;
	uint16 listenChannel;
} proWfdDiscCreateReqT;

typedef struct {
	proWfdDiscT *disc;
} proWfdDiscCreateRspT;

typedef struct {
	uint16 listenOnTimeout;
	uint16 listenOffTimeout;
} proWfdDiscStartExtListenReqT;

struct proWfdDiscReq {
	requestHandlerT handler;
	union {
		proWfdDiscCreateReqT create;
		proWfdDiscStartExtListenReqT startExtListen;
	};
};

/* forward declarations */
static void fsm(proWfdDiscT *disc, eventT event);

/* ----------------------------------------------------------- */

#ifdef BCM_WFD_DISC_ADD_DELETE_IES
static uchar prbrspIe[] = {
	0x50, 0x6f, 0x9a, 0x09, 0x02, 0x02, 0x00, 0x27,
	0x0c, 0x0d, 0x1d, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x01, 0x88, 0x00, 0x01, 0x00, 0x50,
	0xf2, 0x04, 0x00, 0x05, 0x00, 0x10, 0x11, 0x00,
	0x08, 0x42, 0x72, 0x6f, 0x61, 0x64, 0x63, 0x6f,
	0x6d
};

static void addIes(proWfdDiscT *disc)
{
	int i;

	/* update IE with the discovery address */
	for (i = 0; i < (int)sizeof(disc->addr); i++) {
		prbrspIe[12 + i] = disc->addr.octet[i];
	}

	if (wl_add_vndr_ie(disc->drv, disc->bsscfgIndex,
		VNDR_IE_PRBRSP_FLAG, 133, (uchar *)
		"\x00\x50\xf2\x04\x10\x4a\x00\x01\x10\x10\x44\x00\x01\x02\x10\x41"
		"\x00\x01\x01\x10\x12\x00\x02\x00\x00\x10\x53\x00\x02\x01\x88\x10"
		"\x3b\x00\x01\x03\x10\x47\x00\x10\x22\x21\x02\x03\x04\x05\x06\x07"
		"\x08\x09\x7a\xe4\x00\x4a\xf8\x5e\x10\x21\x00\x08\x42\x72\x6f\x61"
		"\x64\x63\x6f\x6d\x10\x23\x00\x06\x53\x6f\x66\x74\x41\x50\x10\x24"
		"\x00\x01\x30\x10\x42\x00\x01\x30\x10\x54\x00\x08\x00\x01\x00\x50"
		"\xf2\x04\x00\x05\x10\x11\x00\x08\x42\x72\x6f\x61\x64\x63\x6f\x6d"
		"\x10\x08\x00\x02\x01\x08\x10\x3c\x00\x01\x01\x10\x49\x00\x06\x00"
		"\x37\x2a\x00\x01\x20") < 0) {
		WL_ERROR(("failed to add vendor IE\n"));
	}
	if (wl_add_vndr_ie(disc->drv, disc->bsscfgIndex,
		VNDR_IE_PRBRSP_FLAG, sizeof(prbrspIe), prbrspIe) < 0) {
		WL_ERROR(("failed to add vendor IE\n"));
	}
	if (wl_add_vndr_ie(disc->drv, disc->bsscfgIndex,
		VNDR_IE_PRBREQ_FLAG, 124, (uchar *)
		"\x00\x50\xf2\x04\x10\x4a\x00\x01\x10\x10\x3a\x00\x01\x00\x10\x08"
		"\x00\x02\x01\x88\x10\x47\x00\x10\x22\x21\x02\x03\x04\x05\x06\x07"
		"\x08\x09\x7a\xe4\x00\x4a\xf8\x5e\x10\x54\x00\x08\x00\x01\x00\x50"
		"\xf2\x04\x00\x05\x10\x3c\x00\x01\x01\x10\x02\x00\x02\x00\x00\x10"
		"\x09\x00\x02\x00\x00\x10\x12\x00\x02\x00\x00\x10\x21\x00\x08\x42"
		"\x72\x6f\x61\x64\x63\x6f\x6d\x10\x23\x00\x06\x53\x6f\x66\x74\x41"
		"\x50\x10\x24\x00\x01\x30\x10\x11\x00\x08\x42\x72\x6f\x61\x64\x63"
		"\x6f\x6d\x10\x49\x00\x06\x00\x37\x2a\x00\x01\x20") < 0) {
		WL_ERROR(("failed to add vendor IE\n"));
	}
	if (wl_add_vndr_ie(disc->drv, disc->bsscfgIndex,
		VNDR_IE_PRBREQ_FLAG, 25, (uchar *)
		"\x50\x6f\x9a\x09\x02\x02\x00\x27\x0c\x06\x05\x00\x55\x53\x04\x51"
		"\x0b\x11\x05\x00\x55\x53\x04\x51\x0b") < 0) {
		WL_ERROR(("failed to add vendor IE\n"));
	}
}

static void deleteIes(proWfdDiscT *disc)
{
	if (wl_del_vndr_ie(disc->drv, disc->bsscfgIndex,
		VNDR_IE_PRBRSP_FLAG, 133, (uchar *)
		"\x00\x50\xf2\x04\x10\x4a\x00\x01\x10\x10\x44\x00\x01\x02\x10\x41"
		"\x00\x01\x01\x10\x12\x00\x02\x00\x00\x10\x53\x00\x02\x01\x88\x10"
		"\x3b\x00\x01\x03\x10\x47\x00\x10\x22\x21\x02\x03\x04\x05\x06\x07"
		"\x08\x09\x7a\xe4\x00\x4a\xf8\x5e\x10\x21\x00\x08\x42\x72\x6f\x61"
		"\x64\x63\x6f\x6d\x10\x23\x00\x06\x53\x6f\x66\x74\x41\x50\x10\x24"
		"\x00\x01\x30\x10\x42\x00\x01\x30\x10\x54\x00\x08\x00\x01\x00\x50"
		"\xf2\x04\x00\x05\x10\x11\x00\x08\x42\x72\x6f\x61\x64\x63\x6f\x6d"
		"\x10\x08\x00\x02\x01\x08\x10\x3c\x00\x01\x01\x10\x49\x00\x06\x00"
		"\x37\x2a\x00\x01\x20") < 0) {
		WL_P2PO(("failed to delete vendor IE\n"));
	}
	if (wl_del_vndr_ie(disc->drv, disc->bsscfgIndex,
		VNDR_IE_PRBRSP_FLAG, sizeof(prbrspIe), prbrspIe) < 0) {
		WL_P2PO(("failed to delete vendor IE\n"));
	}
	if (wl_del_vndr_ie(disc->drv, disc->bsscfgIndex,
		VNDR_IE_PRBREQ_FLAG, 124, (uchar *)
		"\x00\x50\xf2\x04\x10\x4a\x00\x01\x10\x10\x3a\x00\x01\x00\x10\x08"
		"\x00\x02\x01\x88\x10\x47\x00\x10\x22\x21\x02\x03\x04\x05\x06\x07"
		"\x08\x09\x7a\xe4\x00\x4a\xf8\x5e\x10\x54\x00\x08\x00\x01\x00\x50"
		"\xf2\x04\x00\x05\x10\x3c\x00\x01\x01\x10\x02\x00\x02\x00\x00\x10"
		"\x09\x00\x02\x00\x00\x10\x12\x00\x02\x00\x00\x10\x21\x00\x08\x42"
		"\x72\x6f\x61\x64\x63\x6f\x6d\x10\x23\x00\x06\x53\x6f\x66\x74\x41"
		"\x50\x10\x24\x00\x01\x30\x10\x11\x00\x08\x42\x72\x6f\x61\x64\x63"
		"\x6f\x6d\x10\x49\x00\x06\x00\x37\x2a\x00\x01\x20") < 0) {
		WL_P2PO(("failed to delete vendor IE\n"));
	}
	if (wl_del_vndr_ie(disc->drv, disc->bsscfgIndex,
		VNDR_IE_PRBREQ_FLAG, 25, (uchar *)
		"\x50\x6f\x9a\x09\x02\x02\x00\x27\x0c\x06\x05\x00\x55\x53\x04\x51"
		"\x0b\x11\x05\x00\x55\x53\x04\x51\x0b") < 0) {
		WL_P2PO(("failed to delete vendor IE\n"));
	}
}
#endif	/* BCM_WFD_DISC_ADD_DELETE_IES */

/* ----------------------------------------------------------- */

static void listenTimeout(void *arg)
{
	proWfdDiscT *disc = (proWfdDiscT *)arg;

	WL_TRACE(("listenTimeout callback\n"));
	fsm(disc, EVENT_LISTEN_TIMEOUT);
}

/* ----------------------------------------------------------- */

#ifndef BCMDRIVER
static void proWfdDiscInitHandler(proWfdDiscT *discNull,
	int reqLength, proWfdDiscReqT *req, void *rspNull)
{
	(void)discNull;
	(void)rspNull;
	if (reqLength != sizeof(proWfdDiscReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proWfdDiscInitHandler\n"));

	if (wl_enable_event_msg(wl(), WLC_E_ESCAN_RESULT) < 0) {
		WL_ERROR(("failed to enable escan result event\n"));
	}
}

/* initialize WFD discovery */
int proWfdDiscInitialize(void)
{
	proWfdDiscReqT req;

	WL_TRACE(("proWfdDiscInitialize\n"));

	/* attach handler to dispatcher */
	dspSubscribe(dsp(), 0, proWfdDiscProcessWlanEvent);

	req.handler = proWfdDiscInitHandler;
	return dspRequestSynch(dsp(), 0, sizeof(req), (uint8 *)&req, 0);
}

/* ----------------------------------------------------------- */

/* deinitialize WFD discovery */
int proWfdDiscDeinitialize(void)
{
	/* detach handler */
	dspUnsubscribe(dsp(), proWfdDiscProcessWlanEvent);
	return TRUE;
}
#endif /* BCMDRIVER */

/* ----------------------------------------------------------- */

static void proWfdDiscCreateHandler(proWfdDiscT *discNull,
	int reqLength, proWfdDiscReqT *req, void *rspData)
{
	proWfdDiscT *disc;
	proWfdDiscCreateRspT *rsp = (proWfdDiscCreateRspT *)rspData;

	(void)discNull;
	if (reqLength != sizeof(proWfdDiscReqT) || req == 0 || rspData == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proWfdDiscCreateHandler\n"));

#ifndef BCMDRIVER
	/* seed the random generator */
	srand((unsigned)time(NULL));
#endif /* BCMDRIVER */

	rsp->disc = 0;
	disc = &gDisc;
	memset(disc, 0, sizeof(*disc));

	disc->drv = req->create.drv;

	disc->listenChannel = req->create.listenChannel;

	/* initialize social channels */
	disc->socialChannel[0] = 1;
	disc->socialChannel[1] = 6;
	disc->socialChannel[2] = 11;

#ifdef BCM_WFD_DISC_CREATE_DISCOVERY_BSSCFG
	/* disable P2P discovery - to ensure bsscfg does not exist */
	wl_p2p_disc(disc->drv, FALSE);
	/* enable P2P discovery */
	wl_p2p_disc(disc->drv, TRUE);
#endif	/* BCM_WFD_DISC_CREATE_DISCOVERY_BSSCFG */

	if (wl_p2p_dev(disc->drv, &disc->bsscfgIndex) < 0) {
		WL_ERROR(("failed to get bsscfg index\n"));
	}
	WL_P2PO(("bsscfg index=%d\n", disc->bsscfgIndex));
#ifdef BCM_WFD_DISC_ADD_DELETE_IES
	wl_cur_etheraddr(disc->drv, disc->bsscfgIndex, &disc->addr);
	WL_PRMAC("discovery MAC address", &disc->addr);
#endif /* BCM_WFD_DISC_ADD_DELETE_IES */

#ifdef BCM_WFD_DISC_ADD_DELETE_IES
	addIes(disc);
#endif	/* BCM_WFD_DISC_ADD_DELETE_IES */

	/* create timers */
	disc->listenTimer = tmrCreate(
#ifndef BCMDRIVER
		dsp(),
#else
		disc->drv,
#endif	/* BCMDRIVER */
		listenTimeout, disc, "listenTimer");
	if (disc->listenTimer == 0) {
		WL_ERROR(("failed to create timer\n"));
		goto fail;
	}

	/* reset state machine */
	fsm(disc, EVENT_RESET);

	/* return created instance */
	rsp->disc = disc;
	return;

fail:
	memset(disc, 0, sizeof(*disc));
}

/* create WFD discovery */
proWfdDiscT *proWfdDiscCreate(struct wl_drv_hdl *drv, uint16 listenChannel)
{
	proWfdDiscReqT req;
	proWfdDiscCreateRspT rsp;

	WL_TRACE(("proWfdDiscCreate\n"));

	req.handler = proWfdDiscCreateHandler;
	req.create.drv = drv;
	req.create.listenChannel = listenChannel;
#ifdef BCM_WFD_DISC_NO_DISPATCHER
	proWfdDiscCreateHandler(0, sizeof(req), &req, &rsp);
#else
	if (!dspRequestSynch(dsp(), 0, sizeof(req), (uint8 *)&req, (uint8 *)&rsp))
	{
		return 0;
	}
#endif /* BCM_WFD_DISC_NO_DISPATCHER */

	return rsp.disc;
}

/* ----------------------------------------------------------- */

static void proWfdDiscDestroyHandler(proWfdDiscT *disc,
	int reqLength, proWfdDiscReqT *req, void *rspNull)
{
	(void)rspNull;
	if (disc == 0 || reqLength != sizeof(proWfdDiscReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proWfdDiscDestroyHandler\n"));

#ifdef BCM_WFD_DISC_ADD_DELETE_IES
	deleteIes(disc);
#endif	/* BCM_WFD_DISC_ADD_DELETE_IES */

#ifdef BCM_WFD_DISC_CREATE_DISCOVERY_BSSCFG
	/* disable P2P discovery */
	wl_p2p_disc(disc->drv, FALSE);
#endif	/* BCM_WFD_DISC_CREATE_DISCOVERY_BSSCFG */

	disc->bsscfgIndex = 0;

	/* stop timers */
	tmrStop(disc->listenTimer);

	/* destroy timers */
	tmrDestroy(disc->listenTimer);

	memset(disc, 0, sizeof(*disc));
}

/* destroy WFD discovery */
int proWfdDiscDestroy(proWfdDiscT *disc)
{
	proWfdDiscReqT req;

	WL_TRACE(("proWfdDiscDestroy\n"));

	if (disc == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proWfdDiscDestroyHandler;
#ifdef BCM_WFD_DISC_NO_DISPATCHER
	proWfdDiscDestroyHandler(disc, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequestSynch(dsp(), disc, sizeof(req), (uint8 *)&req, 0);
#endif /* BCM_WFD_DISC_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proWfdDiscResetHandler(proWfdDiscT *disc,
	int reqLength, proWfdDiscReqT *req, void *rspNull)
{
	(void)rspNull;
	if (disc == 0 || reqLength != sizeof(proWfdDiscReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proWfdDiscResetHandler\n"));
	fsm(disc, EVENT_RESET);
}

/* reset WFD discovery */
int proWfdDiscReset(proWfdDiscT *disc)
{
	proWfdDiscReqT req;

	WL_TRACE(("proWfdDiscReset\n"));

	if (disc == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proWfdDiscResetHandler;
#ifdef BCM_WFD_DISC_NO_DISPATCHER
	proWfdDiscResetHandler(disc, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), disc, sizeof(req), (uint8 *)&req);
#endif /* BCM_WFD_DISC_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proWfdDiscStartDiscoveryHandler(proWfdDiscT *disc,
	int reqLength, proWfdDiscReqT *req, void *rspNull)
{
	(void)rspNull;
	if (disc == 0 || reqLength != sizeof(proWfdDiscReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proWfdDiscStartDiscoveryHandler\n"));
	fsm(disc, EVENT_START_DISCOVERY);
}

/* start WFD discovery */
int proWfdDiscStartDiscovery(proWfdDiscT *disc)
{
	proWfdDiscReqT req;

	WL_TRACE(("proWfdDiscStartDiscovery\n"));

	if (disc == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proWfdDiscStartDiscoveryHandler;
#ifdef BCM_WFD_DISC_NO_DISPATCHER
	proWfdDiscStartDiscoveryHandler(disc, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), disc, sizeof(req), (uint8 *)&req);
#endif /* BCM_WFD_DISC_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

static void proWfdDiscStartExtListenHandler(proWfdDiscT *disc,
	int reqLength, proWfdDiscReqT *req, void *rspNull)
{
	(void)rspNull;
	if (disc == 0 || reqLength != sizeof(proWfdDiscReqT) || req == 0) {
		WL_ERROR(("invalid parameter\n"));
		return;
	}
	WL_TRACE(("proWfdDiscStartExtListenHandler %d %d\n",
		req->startExtListen.listenOnTimeout,
		req->startExtListen.listenOffTimeout));
	disc->extListenOnTimeout = req->startExtListen.listenOnTimeout;
	disc->extListenOffTimeout = req->startExtListen.listenOffTimeout;
	fsm(disc, EVENT_START_EXT_LISTEN);
}

/* start WFD extended listen */
int proWfdDiscStartExtListen(proWfdDiscT *disc,
	uint16 listenOnTimeout, uint16 listenOffTimeout)
{
	proWfdDiscReqT req;

	WL_TRACE(("proWfdDiscStartExtListen\n"));

	if (disc == 0) {
		WL_ERROR(("invalid parameter\n"));
		return 0;
	}

	req.handler = proWfdDiscStartExtListenHandler;
	req.startExtListen.listenOnTimeout = listenOnTimeout;
	req.startExtListen.listenOffTimeout = listenOffTimeout;
#ifdef BCM_WFD_DISC_NO_DISPATCHER
	proWfdDiscStartExtListenHandler(disc, sizeof(req), &req, 0);
	return 1;
#else
	return dspRequest(dsp(), disc, sizeof(req), (uint8 *)&req);
#endif /* BCM_WFD_DISC_NO_DISPATCHER */
}

/* ----------------------------------------------------------- */

/* get bsscfg index of WFD discovery interface */
/* bsscfg index is valid only after started */
int proWfdDiscGetBsscfgIndex(proWfdDiscT *disc)
{
	return disc->bsscfgIndex;
}

/* ----------------------------------------------------------- */

void proWfdDiscProcessWlanEvent(void * context, uint32 eventType,
	wl_event_msg_t *wlEvent, uint8 *data, uint32 length)
{
	proWfdDiscT *disc = &gDisc;
	(void)context;
#ifndef BCMDBG_ESCAN
	(void)data;
	(void)length;
#endif

#ifdef BCMDBG
	{
		int i;
		char *event_name = "UNKNOWN";

		for (i = 0; i < bcmevent_names_size; i++)
			if (bcmevent_names[i].event == eventType)
				event_name = (char *)bcmevent_names[i].name;

		WL_P2PO(("WLAN event %s (%d)\n", event_name, eventType));
	}
#endif	/* BCMDBG */

	if (eventType == WLC_E_ESCAN_RESULT) {
		if (wlEvent->status == WLC_E_STATUS_PARTIAL) {
#ifdef BCMDBG_ESCAN
			wl_escan_result_t *escan_data = (wl_escan_result_t *)data;

			if (length >= sizeof(*escan_data)) {
				wl_bss_info_t *bi = &escan_data->bss_info[0];
				pktProbeResponseT pr;
				struct ether_addr *addr;

				if (!pktDecodeIeProbeResponse(bi, &pr)) {
					return;
				}

				/* default address */
				addr = &bi->BSSID;

				/* WFD not supported */
				if (!pr.isWfd) {
					char ssidbuf[4*32+1];
					wl_format_ssid(ssidbuf, bi->SSID, bi->SSID_len);
					printf(" AP   %-20.20s   %s   %d\n", ssidbuf,
						wl_ether_etoa(addr), pr.channel);
					return;
				}

				if (pr.isWfdDeviceInfoDecoded) {
					/* use device address */
					addr = &pr.wfdDeviceInfo.deviceAddress;
					printf("WFD   %-20.20s   %s   %d\n",
						pr.wfdDeviceInfo.deviceName,
						wl_ether_etoa(addr), pr.channel);
				}
			}
#endif /* BCMDBG_ESCAN */
		}
		else if (wlEvent->status == WLC_E_STATUS_SUCCESS) {
			WL_P2PO(("WLC_E_ESCAN_RESULT status=WLC_E_STATUS_SUCCESS\n"));
			fsm(disc, EVENT_SCAN_COMPLETE);
		}
		else {
			WL_P2PO(("WLC_E_ESCAN_RESULT status=%d\n", wlEvent->status));
			/* escan may have failed/restarted but keep state machine running */
			fsm(disc, EVENT_SCAN_COMPLETE);
		}
	}
}

static void changeState(proWfdDiscT *disc, stateT next)
{
	/* state transition occurs at the end of fsm() processing */
	disc->nextState = next;
}

/* reset all variables returning to idle state */
static void idleReset(proWfdDiscT *disc)
{
	/* default P2P state */
	wl_p2p_state(disc->drv, WL_P2P_DISC_ST_SCAN, 0, 0);

	/* stop timers */
	tmrStop(disc->listenTimer);

	disc->searchCount = 0;
	changeState(disc, STATE_IDLE);
}

/* 802.11 scan all channels */
static void scan(proWfdDiscT *disc)
{
	wl_p2p_state(disc->drv, WL_P2P_DISC_ST_SCAN, 0, 0);
	wl_p2p_scan(disc->drv, ++disc->sync_id, TRUE, -1, -1, -1, 0, 0);
	changeState(disc, STATE_SCAN);
}

/* random timeout for listen */
static uint16 randomListenTimeout(void)
{
	/* 100, 200, or 300 msec */
	return ((rand() % 3) + 1) * 100;
}

/* listen mode */
static void listen(proWfdDiscT *disc, uint16 timeout)
{
	wl_p2p_state(disc->drv, WL_P2P_DISC_ST_LISTEN,
		CH20MHZ_CHSPEC(disc->listenChannel), timeout);
	WL_P2PO(("listen timer started %d msec\n", timeout));
	tmrStart(disc->listenTimer, timeout, FALSE);
	changeState(disc, STATE_LISTEN);
}

/* extended listen on mode */
static void extendedListenOn(proWfdDiscT *disc, uint16 timeout)
{
	wl_p2p_state(disc->drv, WL_P2P_DISC_ST_LISTEN,
		CH20MHZ_CHSPEC(disc->listenChannel), timeout);
	if (timeout > 0) {
		WL_P2PO(("listen timer started %d msec\n", timeout));
		tmrStart(disc->listenTimer, timeout, FALSE);
	}
	changeState(disc, STATE_EXT_LISTEN_ON);
}

/* extended listen off mode */
static void extendedListenOff(proWfdDiscT *disc, uint16 timeout)
{
	wl_p2p_state(disc->drv, WL_P2P_DISC_ST_SCAN, 0, 0);
	if (timeout > 0) {
		WL_P2PO(("listen timer started %d msec\n", timeout));
		tmrStart(disc->listenTimer, timeout, FALSE);
	}
	changeState(disc, STATE_EXT_LISTEN_OFF);
}

/* search mode */
static void search(proWfdDiscT *disc)
{
	wl_p2p_state(disc->drv, WL_P2P_DISC_ST_SEARCH, 0, 0);
	wl_p2p_scan(disc->drv, ++disc->sync_id, TRUE, -1, -1, -1,
		NUM_SOCIAL_CHANNEL, disc->socialChannel);
	disc->searchCount++;
	changeState(disc, STATE_SEARCH);
}

/* common event processing for all states */
static void defaultEventProcessing(proWfdDiscT *disc, eventT event)
{
	switch (event) {
	case EVENT_RESET:
		WL_TRACE(("defaultEventProcessing: EVENT_RESET\n"));
		idleReset(disc);
		break;
	case EVENT_START_DISCOVERY:
		break;
	case EVENT_START_EXT_LISTEN:
		break;
	case EVENT_SCAN_COMPLETE:
		break;
	case EVENT_LISTEN_TIMEOUT:
		break;
	default:
		WL_ERROR(("invalid event %d\n", event));
		break;
	}
}

static void stateIdle(proWfdDiscT *disc, eventT event)
{
	switch (event) {
	case EVENT_START_DISCOVERY:
	case EVENT_START_EXT_LISTEN:
		if (event == EVENT_START_DISCOVERY) {
#ifdef BCM_WFD_DISC_SCAN_AT_START_OF_DISCOVERY
			scan(disc);
#else
			search(disc);
#endif	/* BCM_WFD_DISC_SCAN_AT_START_OF_DISCOVERY */
		}
		else {
			extendedListenOn(disc, disc->extListenOnTimeout);
		}
		break;
	case EVENT_SCAN_COMPLETE:
		break;
	case EVENT_LISTEN_TIMEOUT:
		break;
	default:
		defaultEventProcessing(disc, event);
		break;
	}
}

static void stateScan(proWfdDiscT *disc, eventT event)
{
	switch (event) {
	case EVENT_START_DISCOVERY:
		break;
	case EVENT_START_EXT_LISTEN:
		break;
	case EVENT_SCAN_COMPLETE:
		listen(disc, randomListenTimeout());
		break;
	case EVENT_LISTEN_TIMEOUT:
		break;
	default:
		defaultEventProcessing(disc, event);
		break;
	}
}

static void stateListen(proWfdDiscT *disc, eventT event)
{
	switch (event) {
	case EVENT_START_DISCOVERY:
		break;
	case EVENT_SCAN_COMPLETE:
		break;
	case EVENT_LISTEN_TIMEOUT:
		search(disc);
		break;
	default:
		defaultEventProcessing(disc, event);
		break;
	}
}

static void stateSearch(proWfdDiscT *disc, eventT event)
{
	switch (event) {
	case EVENT_START_DISCOVERY:
		break;
	case EVENT_START_EXT_LISTEN:
		break;
	case EVENT_SCAN_COMPLETE:
		WL_P2PO(("search count=%d\n", disc->searchCount));
		if ((disc->searchCount % SEARCHES_PER_SCAN) == 0)
			scan(disc);
		else
			listen(disc, randomListenTimeout());
		break;
	case EVENT_LISTEN_TIMEOUT:
		break;
	default:
		defaultEventProcessing(disc, event);
		break;
	}
}

static void stateExtListenOn(proWfdDiscT *disc, eventT event)
{
	switch (event) {
	case EVENT_START_DISCOVERY:
		break;
	case EVENT_START_EXT_LISTEN:
		break;
	case EVENT_SCAN_COMPLETE:
		break;
	case EVENT_LISTEN_TIMEOUT:
		if (disc->extListenOffTimeout > 0)
			extendedListenOff(disc, disc->extListenOffTimeout);
		else
			extendedListenOn(disc, disc->extListenOnTimeout);
		break;
	default:
		defaultEventProcessing(disc, event);
		break;
	}
}

static void stateExtListenOff(proWfdDiscT *disc, eventT event)
{
	switch (event) {
	case EVENT_START_DISCOVERY:
		break;
	case EVENT_START_EXT_LISTEN:
		break;
	case EVENT_SCAN_COMPLETE:
		break;
	case EVENT_LISTEN_TIMEOUT:
		extendedListenOn(disc, disc->extListenOnTimeout);
		break;
	default:
		defaultEventProcessing(disc, event);
		break;
	}
}

/* WFD discovery finite state machine */
static void fsm(proWfdDiscT *disc, eventT event)
{
	WL_P2PO(("--------------------------------------------------------\n"));
	WL_P2PO(("current state=%s event=%s\n",
		state_str[disc->state], event_str[event]));

	switch (disc->state) {
	case STATE_IDLE:
		stateIdle(disc, event);
		break;
	case STATE_SCAN:
		stateScan(disc, event);
		break;
	case STATE_LISTEN:
		stateListen(disc, event);
		break;
	case STATE_SEARCH:
		stateSearch(disc, event);
		break;
	case STATE_EXT_LISTEN_ON:
		stateExtListenOn(disc, event);
		break;
	case STATE_EXT_LISTEN_OFF:
		stateExtListenOff(disc, event);
		break;
	default:
		WL_ERROR(("invalid state %d\n", disc->state));
		break;
	}

	/* transition to next state */
	if (disc->state != disc->nextState) {
		WL_P2PO(("state change %s -> %s\n",
			state_str[disc->state], state_str[disc->nextState]));
		disc->state = disc->nextState;
	}

	WL_P2PO(("--------------------------------------------------------\n"));
}
