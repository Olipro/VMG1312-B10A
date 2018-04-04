/*
 * Test harness for encoding and decoding Hotspot2.0 ANQP packets.
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
#include "pktEncodeHspotAnqp.h"
#include "pktDecodeHspotAnqp.h"

TEST_DECLARE();

#define BUFFER_SIZE		256
static uint8 buffer[BUFFER_SIZE * 4];
static pktEncodeT enc;

/* --------------------------------------------------------------- */

static void testEncode(void)
{
	uint8 data[8];
	int i;

	for (i = 0; i < 8; i++)
		data[i] = i;

	TEST(pktEncodeInit(&enc, sizeof(buffer), buffer), "pktEncodeInit failed");

	TEST(pktEncodeHspotAnqpQueryList(&enc, 8, data),
		"pktEncodeHspotAnqpQueryList failed");
	WL_PRPKT("hotspot query list",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));

	TEST(pktEncodeHspotAnqpCapabilityList(&enc, 8, data),
		"pktEncodeHspotAnqpCapabilityList failed");
	WL_PRPKT("hotspot capability list",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));

	{
		uint8 nameBuf[BUFFER_SIZE];
		pktEncodeT name;

		TEST(pktEncodeInit(&name, BUFFER_SIZE, nameBuf), "pktEncodeInit failed");

		TEST(pktEncodeHspotAnqpOperatorNameDuple(&name, 2, "EN", 6, "myname"),
			"pktEncodeHspotAnqpOperatorNameDuple failed");
		TEST(pktEncodeHspotAnqpOperatorNameDuple(&name, 2, "FR", 10, "helloworld"),
			"pktEncodeHspotAnqpOperatorNameDuple failed");
		TEST(pktEncodeHspotAnqpOperatorNameDuple(&name, 5, "JAPAN", 6, "yrname"),
			"pktEncodeHspotAnqpOperatorNameDuple failed");

		TEST(pktEncodeHspotAnqpOperatorFriendlyName(&enc,
			pktEncodeLength(&name), pktEncodeBuf(&name)),
			"pktEncodeHspotAnqpOperatorFriendlyName failed");
		WL_PRPKT("hotspot operator friendly name",
			pktEncodeBuf(&enc), pktEncodeLength(&enc));
	}

	TEST(pktEncodeHspotAnqpWanMetrics(&enc,
		HSPOT_WAN_LINK_TEST, HSPOT_WAN_SYMMETRIC_LINK, HSPOT_WAN_AT_CAPACITY,
		0x12345678, 0x11223344, 0xaa, 0xbb, 0xcdef),
		"pktEncodeHspotAnqpCapabilityList failed");
	WL_PRPKT("hotspot WAN metrics",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));

	{
		uint8 capBuf[BUFFER_SIZE];
		pktEncodeT cap;

		TEST(pktEncodeInit(&cap, BUFFER_SIZE, capBuf), "pktEncodeInit failed");

		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 1, 0, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 6, 20, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 6, 22, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 6, 80, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 6, 443, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 6, 1723, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 6, 5060, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 17, 500, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 17, 5060, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");
		TEST(pktEncodeHspotAnqpProtoPortTuple(&cap, 17, 4500, HSPOT_CC_STATUS_OPEN),
			"pktEncodeHspotAnqpProtoPortTuple failed");

		TEST(pktEncodeHspotAnqpConnectionCapability(&enc,
			pktEncodeLength(&cap), pktEncodeBuf(&cap)),
			"pktEncodeHspotAnqpConnectionCapability failed");
		WL_PRPKT("hotspot connection capability",
			pktEncodeBuf(&enc), pktEncodeLength(&enc));
	}

	{
		uint8 nameBuf[BUFFER_SIZE];
		pktEncodeT name;

		TEST(pktEncodeInit(&name, BUFFER_SIZE, nameBuf), "pktEncodeInit failed");

		TEST(pktEncodeHspotAnqpNaiHomeRealmName(&name, 0, 5, "hello"),
			"pktEncodeHspotAnqpNaiHomeRealmName failed");
		TEST(pktEncodeHspotAnqpNaiHomeRealmName(&name, 1, 5, "world"),
			"pktEncodeHspotAnqpNaiHomeRealmName failed");

		TEST(pktEncodeHspotAnqpNaiHomeRealmQuery(&enc, 2,
			pktEncodeLength(&name), pktEncodeBuf(&name)),
			"pktEncodeHspotAnqpNaiHomeRealmQuery failed");
		WL_PRPKT("hotspot NAI home realm query",
			pktEncodeBuf(&enc), pktEncodeLength(&enc));
	}

	{
		/* Testing range of operating classes */
		uint8 opClass [35] = {80, 81, 82, 83, 84, 94, 95, 96, 101, 102, 103, 104, 105, 106,
			107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119, 120, 121,
		122, 123, 124, 125, 126, 127};

		TEST(pktEncodeHspotAnqpOperatingClassIndication(&enc, 35, opClass),
			"pktEncodeHspotAnqpOperatingClassIndication failed");
		WL_PRPKT("hotspot operating class indication",
			pktEncodeBuf(&enc), pktEncodeLength(&enc));
	}

	{
		uint8 nameBuf[BUFFER_SIZE];
		pktEncodeT name;
		uint8 iconBuf[BUFFER_SIZE];
		pktEncodeT icon;
		uint8 osuBuf[BUFFER_SIZE];
		uint8 desc1Buf[BUFFER_SIZE];
		pktEncodeT desc1;
		uint8 desc2Buf[BUFFER_SIZE];
		pktEncodeT desc2;
		pktEncodeT osu;
		uint8 soap = HSPOT_OSU_METHOD_SOAP_XML;
		uint8 omadm = HSPOT_OSU_METHOD_OMA_DM;

		TEST(pktEncodeInit(&name, BUFFER_SIZE, nameBuf), "pktEncodeInit failed");
		TEST(pktEncodeHspotAnqpOperatorNameDuple(&name, 2, "EN", 6, "myname"),
			"pktEncodeHspotAnqpOperatorNameDuple failed");

		TEST(pktEncodeInit(&icon, BUFFER_SIZE, iconBuf), "pktEncodeInit failed");
		TEST(pktEncodeHspotAnqpIconMetadata(&icon, 1, 2, "EN",
			4, (uint8 *)"text", 13, (uint8 *)"iconfile1.txt"),
			"pktEncodeHspotAnqpIconMetadata failed");
		TEST(pktEncodeHspotAnqpIconMetadata(&icon, 3, 4, "CH",
			4, (uint8 *)"text", 13, (uint8 *)"iconfile2.txt"),
			"pktEncodeHspotAnqpIconMetadata failed");

		TEST(pktEncodeInit(&desc1, BUFFER_SIZE, desc1Buf), "pktEncodeInit failed");
		TEST(pktEncodeHspotAnqpOperatorNameDuple(&desc1, 2, "EN", 12, "SOAP-XML OSU"),
			"pktEncodeHspotAnqpOperatorNameDuple failed");

		TEST(pktEncodeInit(&desc2, BUFFER_SIZE, desc2Buf), "pktEncodeInit failed");
		TEST(pktEncodeHspotAnqpOperatorNameDuple(&desc2, 2, "EN", 10, "OMA-DM OSU"),
			"pktEncodeHspotAnqpOperatorNameDuple failed");

		TEST(pktEncodeInit(&osu, BUFFER_SIZE, osuBuf), "pktEncodeInit failed");
		TEST(pktEncodeHspotAnqpOsuProvider(&osu,
			pktEncodeLength(&name), pktEncodeBuf(&name),
			6, (uint8 *)"myuri1",
			1, &soap,
			pktEncodeLength(&icon), pktEncodeBuf(&icon),
			15, (uint8 *)"myprovider1.com",
			pktEncodeLength(&desc1), pktEncodeBuf(&desc1)),
			"pktEncodeHspotAnqpOsuProvider failed");
		TEST(pktEncodeHspotAnqpOsuProvider(&osu,
			pktEncodeLength(&name), pktEncodeBuf(&name),
			6, (uint8 *)"myuri2",
			1, &omadm,
			pktEncodeLength(&icon), pktEncodeBuf(&icon),
			15, (uint8 *)"myprovider2.com",
			pktEncodeLength(&desc2), pktEncodeBuf(&desc2)),
			"pktEncodeHspotAnqpOsuProvider failed");

		TEST(pktEncodeHspotAnqpOsuProviderList(&enc,
			23, (uint8 *)"non-transmitted profile",
			11, (uint8 *)"legacy SSID",
			pktEncodeLength(&osu), pktEncodeBuf(&osu)),
			"pktEncodeHspotAnqpOsuProviderList failed");
		WL_PRPKT("hotspot OSU provider list",
			pktEncodeBuf(&enc), pktEncodeLength(&enc));
	}

	{
		TEST(pktEncodeHspotAnqpAnonymousNai(&enc, 13, (uint8 *)"anonymous.com"),
			"pktEncodeHspotAnqpAnonymousNai failed");
		WL_PRPKT("hotspot anonymous NAI",
			pktEncodeBuf(&enc), pktEncodeLength(&enc));
	}

	{
		TEST(pktEncodeHspotAnqpIconRequest(&enc, 12, (uint8 *)"iconfile.txt"),
			"pktEncodeHspotAnqpIconRequest failed");
		WL_PRPKT("hotspot icon request",
			pktEncodeBuf(&enc), pktEncodeLength(&enc));
	}

	{
		TEST(pktEncodeHspotAnqpIconBinaryFile(&enc, HSPOT_ICON_STATUS_SUCCESS,
			4, (uint8 *)"text",	14, (uint8 *)"iconbinarydata"),
			"pktEncodeHspotAnqpIconBinaryFile failed");
		WL_PRPKT("hotspot icon binary file",
			pktEncodeBuf(&enc), pktEncodeLength(&enc));
	}
}

static void testDecode(void)
{
	pktDecodeT dec;
	pktDecodeHspotAnqpT hspot;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");
	WL_PRPKT("decode packet", pktDecodeBuf(&dec), pktDecodeBufLength(&dec));

	TEST(pktDecodeHspotAnqp(&dec, TRUE, &hspot) == 11, "pktDecodeHspotAnqp failed");

	{
		pktDecodeT ie;
		pktHspotAnqpQueryListT queryList;

		TEST(pktDecodeInit(&ie, hspot.queryListLength,
			hspot.queryListBuffer), "pktDecodeInit failed");
		TEST(pktDecodeHspotAnqpQueryList(&ie, &queryList),
			"pktDecodeHspotAnqpQueryList failed");
		TEST(queryList.queryLen == 8, "invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpCapabilityListT capList;

		TEST(pktDecodeInit(&ie, hspot.capabilityListLength,
			hspot.capabilityListBuffer), "pktDecodeInit failed");
		TEST(pktDecodeHspotAnqpCapabilityList(&ie, &capList),
			"pktDecodeHspotAnqpCapabilityList failed");
		TEST(capList.capLen == 8, "invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpWanMetricsT wanMetrics;

		TEST(pktDecodeInit(&ie, hspot.wanMetricsLength,
			hspot.wanMetricsBuffer), "pktDecodeInit failed");
		TEST(pktDecodeHspotAnqpWanMetrics(&ie, &wanMetrics),
			"pktHspotAnqpDecodeWanMetrics failed");
		TEST(wanMetrics.linkStatus == HSPOT_WAN_LINK_TEST, "invalid data");
		TEST(wanMetrics.symmetricLink == HSPOT_WAN_SYMMETRIC_LINK, "invalid data");
		TEST(wanMetrics.atCapacity == HSPOT_WAN_AT_CAPACITY, "invalid data");
		TEST(wanMetrics.dlinkSpeed == 0x12345678, "invalid data");
		TEST(wanMetrics.ulinkSpeed == 0x11223344, "invalid data");
		TEST(wanMetrics.dlinkLoad == 0xaa, "invalid data");
		TEST(wanMetrics.ulinkLoad == 0xbb, "invalid data");
		TEST(wanMetrics.lmd == 0xcdef, "invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpOperatorFriendlyNameT op;

		TEST(pktDecodeInit(&ie, hspot.operatorFriendlyNameLength,
			hspot.operatorFriendlyNameBuffer), "pktDecodeInit failed");

		TEST(pktDecodeHspotAnqpOperatorFriendlyName(&ie, &op),
			"pktDecodeHspotAnqpOperatorFriendlyName failed");
		TEST(op.numName == 3, "invalid data");

		TEST(op.duple[0].langLen == 2, "invalid data");
		TEST(strcmp(op.duple[0].lang, "EN") == 0, "invalid data");
		TEST(op.duple[0].nameLen == 6, "invalid data");
		TEST(strcmp(op.duple[0].name, "myname") == 0, "invalid data");

		TEST(op.duple[1].langLen == 2, "invalid data");
		TEST(strcmp(op.duple[1].lang, "FR") == 0, "invalid data");
		TEST(op.duple[1].nameLen == 10, "invalid data");
		TEST(strcmp(op.duple[1].name, "helloworld") == 0, "invalid data");

		TEST(op.duple[2].langLen == 3, "invalid data");
		TEST(strcmp(op.duple[2].lang, "JAP") == 0, "invalid data");
		TEST(op.duple[2].nameLen == 6, "invalid data");
		TEST(strcmp(op.duple[2].name, "yrname") == 0, "invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpConnectionCapabilityT cap;

		TEST(pktDecodeInit(&ie, hspot.connectionCapabilityLength,
			hspot.connectionCapabilityBuffer), "pktDecodeInit failed");

		TEST(pktDecodeHspotAnqpConnectionCapability(&ie, &cap),
			"pktDecodeHspotAnqpAnqpConnectionCapability failed");
		TEST(cap.numConnectCap == 10, "invalid data");

		TEST(cap.tuple[0].ipProtocol == 1, "invalid data");
		TEST(cap.tuple[0].portNumber == 0, "invalid data");
		TEST(cap.tuple[0].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[1].ipProtocol == 6, "invalid data");
		TEST(cap.tuple[1].portNumber == 20, "invalid data");
		TEST(cap.tuple[1].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[2].ipProtocol == 6, "invalid data");
		TEST(cap.tuple[2].portNumber == 22, "invalid data");
		TEST(cap.tuple[2].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[3].ipProtocol == 6, "invalid data");
		TEST(cap.tuple[3].portNumber == 80, "invalid data");
		TEST(cap.tuple[3].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[4].ipProtocol == 6, "invalid data");
		TEST(cap.tuple[4].portNumber == 443, "invalid data");
		TEST(cap.tuple[4].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[5].ipProtocol == 6, "invalid data");
		TEST(cap.tuple[5].portNumber == 1723, "invalid data");
		TEST(cap.tuple[5].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[6].ipProtocol == 6, "invalid data");
		TEST(cap.tuple[6].portNumber == 5060, "invalid data");
		TEST(cap.tuple[6].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[7].ipProtocol == 17, "invalid data");
		TEST(cap.tuple[7].portNumber == 500, "invalid data");
		TEST(cap.tuple[7].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[8].ipProtocol == 17, "invalid data");
		TEST(cap.tuple[8].portNumber == 5060, "invalid data");
		TEST(cap.tuple[8].status == HSPOT_CC_STATUS_OPEN, "invalid data");

		TEST(cap.tuple[9].ipProtocol == 17, "invalid data");
		TEST(cap.tuple[9].portNumber == 4500, "invalid data");
		TEST(cap.tuple[9].status == HSPOT_CC_STATUS_OPEN, "invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpNaiHomeRealmQueryT realm;

		TEST(pktDecodeInit(&ie, hspot.naiHomeRealmQueryLength,
			hspot.naiHomeRealmQueryBuffer), "pktDecodeInit failed");

		TEST(pktDecodeHspotAnqpNaiHomeRealmQuery(&ie, &realm),
			"pktDecodeHspotAnqpNaiHomeRealmQuery failed");
		TEST(realm.count == 2, "invalid data");

		TEST(realm.data[0].encoding == 0, "invalid data");
		TEST(realm.data[0].nameLen == 5, "invalid data");
		TEST(strcmp(realm.data[0].name, "hello") == 0, "invalid data");

		TEST(realm.data[1].encoding == 1, "invalid data");
		TEST(realm.data[1].nameLen == 5, "invalid data");
		TEST(strcmp(realm.data[1].name, "world") == 0, "invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpOperatingClassIndicationT opClassList;

		TEST(pktDecodeInit(&ie, hspot.opClassIndicationLength,
			hspot.opClassIndicationBuffer), "pktDecodeInit failed");
		TEST(pktDecodeHspotAnqpOperatingClassIndication(&ie, &opClassList),
			"pktDecodeHspotOperatingClassIndication failed");
		TEST(opClassList.opClassLen == 35, "invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpOsuProviderListT list;

		TEST(pktDecodeInit(&ie, hspot.onlineSignupProvidersLength,
			hspot.onlineSignupProvidersBuffer), "pktDecodeInit failed");
		TEST(pktDecodeHspotAnqpOsuProviderList(&ie, &list),
			"pktDecodeHspotAnqpOsuProviderList failed");

		TEST(list.osuProviderCount == 2, "invalid data");

		TEST(list.osuProvider[0].name.numName == 1, "invalid data");
		TEST(strcmp(list.osuProvider[0].name.duple[0].lang, "EN") == 0, "invalid data");
		TEST(strcmp(list.osuProvider[0].name.duple[0].name, "myname") == 0, "invalid data");
		TEST(strcmp((const char *)list.osuProvider[0].nai, "myprovider1.com") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[0].uri, "myuri1") == 0, "invalid data");
		TEST(list.osuProvider[0].methodLength == 1, "invalid data");
		TEST(list.osuProvider[0].method[0] == HSPOT_OSU_METHOD_SOAP_XML, "invalid data");
		TEST(list.osuProvider[0].iconMetadataCount == 2, "invalid data");
		TEST(list.osuProvider[0].iconMetadata[0].width == 1, "invalid data");
		TEST(list.osuProvider[0].iconMetadata[0].height == 2, "invalid data");
		TEST(strcmp((const char *)list.osuProvider[0].iconMetadata[0].lang, "EN") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[0].iconMetadata[0].type, "text") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[0].iconMetadata[0].filename,
			"iconfile1.txt") == 0, "invalid data");
		TEST(list.osuProvider[0].iconMetadata[1].width == 3, "invalid data");
		TEST(list.osuProvider[0].iconMetadata[1].height == 4, "invalid data");
		TEST(strcmp((const char *)list.osuProvider[0].iconMetadata[1].lang, "CH") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[0].iconMetadata[1].type, "text") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[0].iconMetadata[1].filename,
			"iconfile2.txt") == 0, "invalid data");
		TEST(list.osuProvider[0].desc.numName == 1, "invalid data");
		TEST(strcmp(list.osuProvider[0].desc.duple[0].lang, "EN") == 0,
			"invalid data");
		TEST(strcmp(list.osuProvider[0].desc.duple[0].name, "SOAP-XML OSU") == 0,
			"invalid data");

		TEST(list.osuProvider[1].name.numName == 1, "invalid data");
		TEST(strcmp(list.osuProvider[1].name.duple[0].lang, "EN") == 0, "invalid data");
		TEST(strcmp(list.osuProvider[1].name.duple[0].name, "myname") == 0, "invalid data");
		TEST(strcmp((const char *)list.osuProvider[1].nai, "myprovider2.com") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[1].uri, "myuri2") == 0, "invalid data");
		TEST(list.osuProvider[1].methodLength == 1, "invalid data");
		TEST(list.osuProvider[1].method[0] == HSPOT_OSU_METHOD_OMA_DM, "invalid data");
		TEST(list.osuProvider[1].iconMetadataCount == 2, "invalid data");
		TEST(list.osuProvider[1].iconMetadata[0].width == 1, "invalid data");
		TEST(list.osuProvider[1].iconMetadata[0].height == 2, "invalid data");
		TEST(strcmp((const char *)list.osuProvider[1].iconMetadata[0].lang, "EN") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[1].iconMetadata[0].type, "text") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[1].iconMetadata[0].filename,
			"iconfile1.txt") == 0, "invalid data");
		TEST(list.osuProvider[1].iconMetadata[1].width == 3, "invalid data");
		TEST(list.osuProvider[1].iconMetadata[1].height == 4, "invalid data");
		TEST(strcmp((const char *)list.osuProvider[1].iconMetadata[1].lang, "CH") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[1].iconMetadata[1].type, "text") == 0,
			"invalid data");
		TEST(strcmp((const char *)list.osuProvider[1].iconMetadata[1].filename,
			"iconfile2.txt") == 0, "invalid data");
		TEST(list.osuProvider[1].desc.numName == 1, "invalid data");
		TEST(strcmp(list.osuProvider[1].desc.duple[0].lang, "EN") == 0,
			"invalid data");
		TEST(strcmp(list.osuProvider[1].desc.duple[0].name, "OMA-DM OSU") == 0,
			"invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpAnonymousNaiT anonymous;

		TEST(pktDecodeInit(&ie, hspot.anonymousNaiLength,
			hspot.anonymousNaiBuffer), "pktDecodeInit failed");
		TEST(pktDecodeHspotAnqpAnonymousNai(&ie, &anonymous),
			"pktDecodeHspotAnqpAnonymousNai failed");
		TEST(strcmp(anonymous.nai, "anonymous.com") == 0, "invalid data");
		TEST(anonymous.naiLen == 13, "invalid data");
	}

	{
		pktDecodeT ie;
		pktHspotAnqpIconBinaryFileT icon;

		TEST(pktDecodeInit(&ie, hspot.iconBinaryFileLength,
			hspot.iconBinaryFileBuffer), "pktDecodeInit failed");
		TEST(pktDecodeHspotAnqpIconBinaryFile(&ie, &icon),
			"pktDecodeHspotAnqpIconBinaryFile failed");
		TEST(icon.status == HSPOT_ICON_STATUS_SUCCESS, "invalid data");
		TEST(strcmp((const char *)icon.type, "text") == 0, "invalid data");
		TEST(strcmp(icon.binary, "iconbinarydata") == 0, "invalid data");
	}
}

static void testDecodeCorruptLength(void)
{
	pktDecodeT dec;
	pktDecodeHspotAnqpT hspot;
	uint8 *lenPtr;
	uint8 save;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");
	WL_PRPKT("decode packet", pktDecodeBuf(&dec), pktDecodeBufLength(&dec));

	lenPtr = &pktDecodeBuf(&dec)[2];
	save = *lenPtr;
	*lenPtr += 1;
	TEST(pktDecodeHspotAnqp(&dec, TRUE, &hspot) == 1, "pktDecodeHspotAnqp failed");
	*lenPtr = 0x02;
	TEST(pktDecodeHspotAnqp(&dec, TRUE, &hspot) == 0, "pktDecodeHspotAnqp failed");
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
