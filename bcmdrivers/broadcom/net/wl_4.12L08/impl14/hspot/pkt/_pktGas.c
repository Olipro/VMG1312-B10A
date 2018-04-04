/*
 * Test harness for encoding and decoding 802.11u GAS packets.
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
#include "test.h"
#include "trace.h"
#include "pktEncodeGas.h"
#include "pktDecodeGas.h"

TEST_DECLARE();

#define BUFFER_SIZE		256
static uint8 buffer[BUFFER_SIZE];
static pktEncodeT enc;

/* --------------------------------------------------------------- */

static void testPktGasRequest(void)
{
	pktDecodeT dec;
	uint8 apie[4] = {108, 2, 0, 0};
	uint8 req[128];
	int i;
	pktGasDecodeT gasDecode;

	for (i = 0; i < 128; i++)
		req[i] = i;

	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");

	TEST(pktEncodeGasRequest(&enc, 0x11, 4, apie, 128, req),
		"pktEncodeGasRequest failed");

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeGas(&dec, &gasDecode), "pktDecodeGas failed");
	TEST(gasDecode.category == DOT11_ACTION_CAT_PUBLIC, "decode failed");
	TEST(gasDecode.action == GAS_REQUEST_ACTION_FRAME, "decode failed");
	TEST(gasDecode.dialogToken == 0x11, "decode failed");
	TEST(gasDecode.request.apie.protocolId == 0, "decode failed");
	TEST(gasDecode.request.reqLen == 128, "decode failed");
}

static void testPktGasResponse(void)
{
	pktDecodeT dec;
	uint8 apie[4] = {108, 2, 0, 0};
	uint8 rsp[128];
	int i;
	pktGasDecodeT gasDecode;

	for (i = 0; i < 128; i++)
		rsp[i] = i;

	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");

	TEST(pktEncodeGasResponse(&enc, 0x11, 0x1234, 0x5678, 4, apie, 128, rsp),
		"pktEncodeGasResponse failed");

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeGas(&dec, &gasDecode), "pktDecodeGas failed");
	TEST(gasDecode.category == DOT11_ACTION_CAT_PUBLIC, "decode failed");
	TEST(gasDecode.action == GAS_RESPONSE_ACTION_FRAME, "decode failed");
	TEST(gasDecode.dialogToken == 0x11, "decode failed");
	TEST(gasDecode.response.statusCode == 0x1234, "decode failed");
	TEST(gasDecode.response.comebackDelay == 0x5678, "decode failed");
	TEST(gasDecode.response.apie.protocolId == 0, "decode failed");
	TEST(gasDecode.response.rspLen == 128, "decode failed");
}

static void testPktGasComebackRequest(void)
{
	pktDecodeT dec;
	pktGasDecodeT gasDecode;

	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");

	TEST(pktEncodeGasComebackRequest(&enc, 0x11),
		"pktEncodeGasComebackRequest failed");

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeGas(&dec, &gasDecode), "pktDecodeGas failed");
	TEST(gasDecode.category == DOT11_ACTION_CAT_PUBLIC, "decode failed");
	TEST(gasDecode.action == GAS_COMEBACK_REQUEST_ACTION_FRAME, "decode failed");
	TEST(gasDecode.dialogToken == 0x11, "decode failed");
}

static void testPktGasComebackResponse(void)
{
	pktDecodeT dec;
	uint8 apie[4] = {108, 2, 0, 0};
	uint8 rsp[128];
	int i;
	pktGasDecodeT gasDecode;

	for (i = 0; i < 128; i++)
		rsp[i] = i;

	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");

	TEST(pktEncodeGasComebackResponse(&enc, 0x11, 0x1234, 0xaa, 0x5678, 4, apie, 128, rsp),
		"pktEncodeGasComebackResponse failed");

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeGas(&dec, &gasDecode), "pktDecodeGas failed");
	TEST(gasDecode.category == DOT11_ACTION_CAT_PUBLIC, "decode failed");
	TEST(gasDecode.action == GAS_COMEBACK_RESPONSE_ACTION_FRAME, "decode failed");
	TEST(gasDecode.dialogToken == 0x11, "decode failed");
	TEST(gasDecode.comebackResponse.statusCode == 0x1234, "decode failed");
	TEST(gasDecode.comebackResponse.fragmentId == 0xaa, "decode failed");
	TEST(gasDecode.comebackResponse.comebackDelay == 0x5678, "decode failed");
	TEST(gasDecode.comebackResponse.apie.protocolId == 0, "decode failed");
	TEST(gasDecode.comebackResponse.rspLen == 128, "decode failed");
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	TRACE_LEVEL_SET(TRACE_ALL);
	TEST_INITIALIZE();

	testPktGasRequest();
	testPktGasResponse();
	testPktGasComebackRequest();
	testPktGasComebackResponse();

	TEST_FINALIZE();
	return 0;
}
