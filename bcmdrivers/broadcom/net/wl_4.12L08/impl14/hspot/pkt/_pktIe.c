/*
 * Test harness for encoding and decoding information elements.
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
#include "pktEncodeIe.h"
#include "pktDecodeIe.h"

TEST_DECLARE();

#define BUFFER_SIZE		256
static uint8 buffer[BUFFER_SIZE];
static pktEncodeT enc;

/* --------------------------------------------------------------- */

static void testEncode(void)
{
	uint8 adBuffer[BUFFER_SIZE];
	pktEncodeT ad;

	TEST(pktEncodeInit(&enc, sizeof(buffer), buffer), "pktEncodeInit failed");

	TEST(pktEncodeIeHotspotIndication2(&enc, TRUE, TRUE, HSPOT_RELEASE_2),
		"pktEncodeIeHotspotIndication2 failed");

	TEST(pktEncodeIeInterworking(&enc,
		IW_ANT_WILDCARD_NETWORK, FALSE, FALSE, FALSE, FALSE,
		TRUE, 0, 0, (struct ether_addr *)"\xff\xff\xff\xff\xff\xff"),
		"pktEncodeIeInterworking failed");

	TEST(pktEncodeInit(&ad, sizeof(adBuffer), adBuffer), "pktEncodeInit failed");
	TEST(pktEncodeIeAdvertisementProtocolTuple(&ad,
		ADVP_PAME_BI_DEPENDENT, 0xff, ADVP_ANQP_PROTOCOL_ID),
		"pktEncodeIeAdvertisementProtocolTuple failed");
	TEST(pktEncodeIeAdvertiseProtocol(&enc, pktEncodeLength(&ad), pktEncodeBuf(&ad)),
		"pktEncodeIeAdvertiseProtocol failed");

	TEST(pktEncodeIeRoamingConsortium(&enc, 0xff, 3, (uint8 *)"\x00\x11\x22",
		4, (uint8 *)"\x55\x66\x77\x88", 4, (uint8 *)"\xaa\xbb\xcc\xdd"),
		"pktEncodeIeRoamingConsortium failed");

	TEST(pktEncodeIeExtendedCapabilities(&enc, 0x80000000),
		"pktEncodeIeExtendedCapabilities failed");

	WL_PRPKT("encoded IEs",	pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecode(void)
{
	pktDecodeT dec;
	pktIeT ie;
	pktDecodeT dec1;
	pktHotspotIndicationT hotspot;
	pktInterworkingT interworking;
	pktAdvertisementProtocolT advertise;
	pktRoamingConsortiumT roam;
	uint32 cap;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");
	WL_PRPKT("decode packet", pktDecodeBuf(&dec), pktDecodeBufLength(&dec));

	TEST(pktDecodeIe(&dec, &ie) == 5, "pktDecodeIe failed");

	TEST(pktDecodeInit(&dec1, ie.hotspotIndicationLength,
		ie.hotspotIndication), "pktDecodeInit failed");
	TEST(pktDecodeIeHotspotIndication2(&dec1, &hotspot),
		"pktDecodeIeHotspotIndication failed");
	TEST(hotspot.isDgafDisabled == TRUE, "invalid data");
	TEST(hotspot.isOsuBssid == TRUE, "invalid data");
	TEST(hotspot.releaseNumber == HSPOT_RELEASE_2, "invalid data");

	TEST(pktDecodeInit(&dec1, ie.interworkingLength,
		ie.interworking), "pktDecodeInit failed");
	TEST(pktDecodeIeInterworking(&dec1, &interworking), "pktDecodeIeInterworking failed");
	TEST(interworking.accessNetworkType == IW_ANT_WILDCARD_NETWORK, "invalid data");
	TEST(interworking.isInternet == FALSE, "invalid data");
	TEST(interworking.isAsra == FALSE, "invalid data");
	TEST(interworking.isEsr == FALSE, "invalid data");
	TEST(interworking.isUesa == FALSE, "invalid data");
	TEST(interworking.isVenue == TRUE, "invalid data");
	TEST(interworking.venueGroup == 0, "invalid data");
	TEST(interworking.venueType == 0, "invalid data");
	TEST(interworking.isHessid == TRUE, "invalid data");
	TEST(memcmp(&interworking.hessid, "\xff\xff\xff\xff\xff\xff",
		sizeof(interworking.hessid)) == 0, "invalid data");

	TEST(pktDecodeInit(&dec1, ie.advertisementProtocolLength,
		ie.advertisementProtocol), "pktDecodeInit failed");
	TEST(pktDecodeIeAdvertisementProtocol(&dec1, &advertise),
		"pktDecodeIeAdvertisementProtocol failed");
	TEST(advertise.count == 1, "invalid data");
	TEST(advertise.protocol[0].queryResponseLimit == 0x7f, "invalid data");
	TEST(advertise.protocol[0].isPamebi == FALSE, "invalid data");
	TEST(advertise.protocol[0].protocolId == ADVP_ANQP_PROTOCOL_ID, "invalid data");

	TEST(pktDecodeInit(&dec1, ie.roamingConsortiumLength,
		ie.roamingConsortium), "pktDecodeInit failed");
	TEST(pktDecodeIeRoamingConsortium(&dec1, &roam),
		"pktDecodeRoamingConsortium failed");
	TEST(roam.anqpOiCount == 0xff, "invalid data");
	TEST(roam.count == 3, "invalid data");
	TEST(memcmp(roam.oi[0].data, "\x00\x11\x22", roam.oi[0].length) == 0,
		"invalid data");
	TEST(memcmp(roam.oi[1].data, "\x55\x66\x77\x88", roam.oi[1].length) == 0,
		"invalid data");
	TEST(memcmp(roam.oi[2].data, "\xaa\xbb\xcc\xdd", roam.oi[2].length) == 0,
		"invalid data");

	TEST(pktDecodeInit(&dec1, ie.extendedCapabilityLength,
		ie.extendedCapability), "pktDecodeInit failed");
	TEST(pktDecodeIeExtendedCapabilities(&dec1, &cap),
		"pktDecodeIeExtendedCapabilities failed");
	TEST(cap == 0x80000000, "invalid data");
}

static void testDecodeCorruptLength(void)
{
	pktDecodeT dec;
	pktIeT ie;
	uint8 *lenPtr;
	uint8 save;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");
	WL_PRPKT("decode packet", pktDecodeBuf(&dec), pktDecodeBufLength(&dec));

	lenPtr = &pktDecodeBuf(&dec)[1];
	save = *lenPtr;
	*lenPtr += 1;
	TEST(pktDecodeIe(&dec, &ie) == 1, "pktDecodeIe failed");
	*lenPtr = 0xff;
	TEST(pktDecodeIe(&dec, &ie) == 0, "pktDecodeIe failed");
	*lenPtr = save;
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	TRACE_LEVEL_SET(TRACE_ALL);
	TEST_INITIALIZE();

	testEncode();
	testDecode();
	testDecodeCorruptLength();

	TEST_FINALIZE();
	return 0;
}
