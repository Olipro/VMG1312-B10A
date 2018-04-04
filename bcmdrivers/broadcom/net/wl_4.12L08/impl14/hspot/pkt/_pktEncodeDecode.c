/*
 * Test harness for encoding and decoding base functions.
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
#include "test.h"
#include "trace.h"
#include "pktEncode.h"
#include "pktDecode.h"

TEST_DECLARE();

#define BUFFER_SIZE		256
static uint8 buffer[BUFFER_SIZE];
static pktEncodeT enc;

/* --------------------------------------------------------------- */

static void testEncode(void)
{
	uint8 data[BUFFER_SIZE];

	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");

	TEST(pktEncodeBe16(&enc, 0x1122) == 2, "pktEncodeBe16 failed");
	TEST(pktEncodeBe32(&enc, 0x11223344) == 4, "pktEncodeBe32 failed");
	TEST(pktEncodeLe16(&enc, 0xaabb) == 2, "pktEncodeLe16 failed");
	TEST(pktEncodeLe32(&enc, 0xaabbccdd) == 4, "pktEncodeLe32 failed");

	/* packet full */
	TEST(pktEncodeBytes(&enc, BUFFER_SIZE, data) == 0, "pktEncodeBytes failed");
	TEST(pktEncodeLength(&enc) == 12, "pktEncodeLength failed");
}

static void testDecode(void)
{
	pktDecodeT dec;
	uint16 data16;
	uint32 data32;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");
	WL_PRPKT("decode packet", pktDecodeBuf(&dec), pktDecodeBufLength(&dec));

	data16 = 0;
	TEST(pktDecodeBe16(&dec, &data16) == 2, "pktDecodeBe16 failed");
	TEST(data16 == 0x1122, "invalid data");
	data32 = 0;
	TEST(pktDecodeBe32(&dec, &data32) == 4, "pktDecodeBe32 failed");
	TEST(data32 == 0x11223344, "invalid data");
	data16 = 0;
	TEST(pktDecodeLe16(&dec, &data16) == 2, "pktDecodeLe16 failed");
	TEST(data16 == 0xaabb, "invalid data");
	data32 = 0;
	TEST(pktDecodeLe32(&dec, &data32) == 4, "pktDecodeLe32 failed");
	TEST(data32 == 0xaabbccdd, "invalid data");

	/* decode beyond buffer */
	TEST(pktDecodeBe16(&dec, &data16) == 0, "pktDecodeBe16 failed");
	TEST(pktDecodeBe32(&dec, &data32) == 0, "pktDecodeBe32 failed");
	TEST(pktDecodeLe16(&dec, &data16) == 0, "pktDecodeLe16 failed");
	TEST(pktDecodeLe32(&dec, &data32) == 0, "pktDecodeLe32 failed");
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
