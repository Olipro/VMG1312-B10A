/*
 * Decoding of 802.11u GAS packets.
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proto/802.11.h"
#include "trace.h"
#include "pktDecodeIe.h"
#include "pktDecodeGas.h"

static void printGasFrame(pktGasDecodeT *gasDecode)
{
	WL_P2PO(("decoded GAS frame:\n"));
	WL_P2PO(("   category = %d\n", gasDecode->category));
	WL_P2PO(("   action = %d (%s)\n", gasDecode->action,
		gasDecode->action == GAS_REQUEST_ACTION_FRAME ? "GAS REQUEST" :
		gasDecode->action == GAS_RESPONSE_ACTION_FRAME ? "GAS RESPONSE" :
		gasDecode->action == GAS_COMEBACK_REQUEST_ACTION_FRAME ?
			"GAS COMEBACK REQUEST" :
		gasDecode->action == GAS_COMEBACK_RESPONSE_ACTION_FRAME ?
			"GAS COMEBACK RESPONSE" :
		"?"));
	WL_P2PO(("   dialog token = %d\n", gasDecode->dialogToken));
	switch (gasDecode->action)
	{
	case GAS_REQUEST_ACTION_FRAME:
		WL_P2PO(("   advertisement protocol ID = %d\n",
			gasDecode->request.apie.protocolId));
		WL_PRPKT("   request",
			gasDecode->request.req, gasDecode->request.reqLen);
		break;
	case GAS_RESPONSE_ACTION_FRAME:
		WL_P2PO(("   status code = %d\n",
			gasDecode->response.statusCode));
		WL_P2PO(("   comeback delay = 0x%04x\n",
			gasDecode->response.comebackDelay));
		WL_P2PO(("   advertisement protocol ID = %d\n",
			gasDecode->response.apie.protocolId));
		WL_PRPKT("   response",
			gasDecode->response.rsp, gasDecode->response.rspLen);
		break;
	case GAS_COMEBACK_REQUEST_ACTION_FRAME:
		/* nothing */
		break;
	case GAS_COMEBACK_RESPONSE_ACTION_FRAME:
		WL_P2PO(("   status code = %d\n",
			gasDecode->comebackResponse.statusCode));
		WL_P2PO(("   fragment ID = 0x%02x\n",
			gasDecode->comebackResponse.fragmentId));
		WL_P2PO(("   comeback delay = 0x%04x\n",
			gasDecode->comebackResponse.comebackDelay));
		WL_P2PO(("   advertisement protocol ID = %d\n",
			gasDecode->comebackResponse.apie.protocolId));
		WL_PRPKT("   response",
			gasDecode->comebackResponse.rsp, gasDecode->comebackResponse.rspLen);
		break;
	default:
		break;
	}
}

static int decodeAdvertisementProtocol(pktDecodeT *pkt,
	pktAdvertisementProtocolTupleT *ap)
{
	uint8 ie, len;

	if (!pktDecodeByte(pkt, &ie) || ie != DOT11_MNG_ADVERTISEMENT_ID) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &len) || len != 2) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeIeAdvertisementProtocolTuple(pkt, ap)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

static int decodeReqRsp(pktDecodeT *pkt, uint16 *rLen, uint8 **r)
{
	if (!pktDecodeLe16(pkt, rLen)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (pktDecodeRemaining(pkt) < *rLen) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	*r = pktDecodeCurrentPtr(pkt);
	pktDecodeOffset(pkt) += *rLen;

	return TRUE;
}

int pktDecodeGas(pktDecodeT *pkt, pktGasDecodeT *gasDecode)
{

	memset(gasDecode, 0, sizeof(*gasDecode));

	if (pktDecodeRemaining(pkt) < 3) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	pktDecodeByte(pkt, &gasDecode->category);
	pktDecodeByte(pkt, &gasDecode->action);
	pktDecodeByte(pkt, &gasDecode->dialogToken);

	if (gasDecode->category != DOT11_ACTION_CAT_PUBLIC) {
		WL_P2PO(("invalid category %d\n", gasDecode->category));
		return FALSE;
	}

	switch (gasDecode->action)
	{
	case GAS_REQUEST_ACTION_FRAME:
		if (!decodeAdvertisementProtocol(pkt, &gasDecode->request.apie)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (!decodeReqRsp(pkt, &gasDecode->request.reqLen, &gasDecode->request.req)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		break;
	case GAS_RESPONSE_ACTION_FRAME:
		if (!pktDecodeLe16(pkt, &gasDecode->response.statusCode)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (!pktDecodeLe16(pkt, &gasDecode->response.comebackDelay)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (!decodeAdvertisementProtocol(pkt, &gasDecode->response.apie)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (!decodeReqRsp(pkt, &gasDecode->response.rspLen, &gasDecode->response.rsp)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		break;
	case GAS_COMEBACK_REQUEST_ACTION_FRAME:
		/* nothing */
		break;
	case GAS_COMEBACK_RESPONSE_ACTION_FRAME:
		if (!pktDecodeLe16(pkt, &gasDecode->comebackResponse.statusCode)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (!pktDecodeByte(pkt, &gasDecode->comebackResponse.fragmentId)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (!pktDecodeLe16(pkt, &gasDecode->comebackResponse.comebackDelay)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (!decodeAdvertisementProtocol(pkt, &gasDecode->comebackResponse.apie)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (!decodeReqRsp(pkt, &gasDecode->comebackResponse.rspLen,
			&gasDecode->comebackResponse.rsp)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		break;
	default:
		WL_P2PO(("invalid action %d\n", gasDecode->action));
		return FALSE;
		break;
	}

	printGasFrame(gasDecode);
	return TRUE;
}
