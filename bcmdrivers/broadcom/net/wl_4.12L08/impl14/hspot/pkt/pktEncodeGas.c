/*
 * Encoding of 802.11u GAS packets.
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
#include "pktEncodeGas.h"

/* encode GAS request */
int pktEncodeGasRequest(pktEncodeT *pkt, uint8 dialogToken,
	uint8 apieLen, uint8 *apie, uint16 reqLen, uint8 *req)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_ACTION_CAT_PUBLIC);
	pktEncodeByte(pkt, GAS_REQUEST_ACTION_FRAME);
	pktEncodeByte(pkt, dialogToken);
	if (apieLen > 0) {
		pktEncodeBytes(pkt, apieLen, apie);
	}
	pktEncodeLe16(pkt, reqLen);
	if (reqLen > 0) {
		pktEncodeBytes(pkt, reqLen, req);
	}

	return pktEncodeLength(pkt) - initLen;
}

/* encode GAS response */
int pktEncodeGasResponse(pktEncodeT *pkt, uint8 dialogToken,
	uint16 statusCode, uint16 comebackDelay, uint8 apieLen, uint8 *apie,
	uint16 rspLen, uint8 *rsp)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_ACTION_CAT_PUBLIC);
	pktEncodeByte(pkt, GAS_RESPONSE_ACTION_FRAME);
	pktEncodeByte(pkt, dialogToken);
	pktEncodeLe16(pkt, statusCode);
	pktEncodeLe16(pkt, comebackDelay);
	if (apieLen > 0) {
		pktEncodeBytes(pkt, apieLen, apie);
	}
	pktEncodeLe16(pkt, rspLen);
	if (rspLen > 0) {
		pktEncodeBytes(pkt, rspLen, rsp);
	}

	return pktEncodeLength(pkt) - initLen;
}

/* encode GAS comeback request */
int pktEncodeGasComebackRequest(pktEncodeT *pkt, uint8 dialogToken)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_ACTION_CAT_PUBLIC);
	pktEncodeByte(pkt, GAS_COMEBACK_REQUEST_ACTION_FRAME);
	pktEncodeByte(pkt, dialogToken);

	return pktEncodeLength(pkt) - initLen;
}

/* encode GAS response */
int pktEncodeGasComebackResponse(pktEncodeT *pkt, uint8 dialogToken,
	uint16 statusCode, uint8 fragmentId, uint16 comebackDelay,
	uint8 apieLen, uint8 *apie, uint16 rspLen, uint8 *rsp)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_ACTION_CAT_PUBLIC);
	pktEncodeByte(pkt, GAS_COMEBACK_RESPONSE_ACTION_FRAME);
	pktEncodeByte(pkt, dialogToken);
	pktEncodeLe16(pkt, statusCode);
	pktEncodeByte(pkt, fragmentId);
	pktEncodeLe16(pkt, comebackDelay);
	if (apieLen > 0) {
		pktEncodeBytes(pkt, apieLen, apie);
	}
	pktEncodeLe16(pkt, rspLen);
	if (rspLen > 0) {
		pktEncodeBytes(pkt, rspLen, rsp);
	}

	return pktEncodeLength(pkt) - initLen;
}
