/*
 * Test harness for encoding and decoding WNM packets
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
#include "pktEncodeWnm.h"
#include "pktDecodeWnm.h"

TEST_DECLARE();

#define BUFFER_SIZE		512
static uint8 buffer[BUFFER_SIZE];
static pktEncodeT enc;

/* --------------------------------------------------------------- */

static void testEncode(void)
{
	TEST(pktEncodeInit(&enc, sizeof(buffer), buffer), "pktEncodeInit failed");
	TEST(pktEncodeWnmSubscriptionRemediation(&enc, 1, 10, "helloworld"),
		"pktEncodeWnmSubscriptionRemediation failed");
	WL_PRPKT("encoded packet", pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecode(void)
{
	pktDecodeT dec;
	pktWnmDecodeT wnm;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");
	WL_PRPKT("decode packet", pktDecodeBuf(&dec), pktDecodeBufLength(&dec));

	TEST(pktDecodeWnmSubscriptionRemediation(&dec, &wnm),
		"pktDecodeWnmSubscriptionRemediation failed");
	TEST(wnm.dialogToken == 1, "invalid data");
	TEST(wnm.urlLength == 10, "invalid data");
	TEST(strcmp(wnm.url, "helloworld") == 0, "invalid data");
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	TRACE_LEVEL_SET(TRACE_ALL);
	TEST_INITIALIZE();

	testEncode();
	testDecode();

	TEST_FINALIZE();
	return 0;
}
