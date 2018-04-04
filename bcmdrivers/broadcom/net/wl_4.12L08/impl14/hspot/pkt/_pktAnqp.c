/*
 * Test harness for encoding and decoding 802.11u ANQP packets.
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
#include "proto/p2p.h"
#include "test.h"
#include "trace.h"
#include "pktEncodeAnqp.h"
#include "pktDecodeAnqp.h"
#include "pktEncodeHspotAnqp.h"
#include "pktDecodeHspotAnqp.h"

TEST_DECLARE();

#define NO_IE_APPEND	0

#define BUFFER_SIZE		1024
static uint8 buffer[BUFFER_SIZE];
static pktEncodeT enc;

/* --------------------------------------------------------------- */

static void testEncodeQueryList(void)
{
	uint16 query[] = {
		ANQP_ID_VENUE_NAME_INFO,
		ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO,
		ANQP_ID_ROAMING_CONSORTIUM_LIST,
		ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO,
		ANQP_ID_NAI_REALM_LIST,
		ANQP_ID_G3PP_CELLULAR_NETWORK_INFO,
		ANQP_ID_DOMAIN_NAME_LIST,
	};

	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
	TEST(pktEncodeAnqpQueryList(&enc, sizeof(query) / sizeof(uint16), query),
	"pktEncodeAnqpQueryList failed");

	WL_PRPKT("testEncodeQueryList",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeQueryList(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqpQueryListT queryList;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 1, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.anqpQueryListLength,
		anqp.anqpQueryListBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpQueryList(&ie, &queryList), "pktDecodeAnqpQueryList failed");
	TEST(queryList.queryLen == 7, "invalid data");
	TEST(queryList.queryId[0] == ANQP_ID_VENUE_NAME_INFO, "invalid data");
	TEST(queryList.queryId[1] == ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO, "invalid data");
	TEST(queryList.queryId[2] == ANQP_ID_ROAMING_CONSORTIUM_LIST, "invalid data");
	TEST(queryList.queryId[3] == ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO, "invalid data");
	TEST(queryList.queryId[4] == ANQP_ID_NAI_REALM_LIST, "invalid data");
	TEST(queryList.queryId[5] == ANQP_ID_G3PP_CELLULAR_NETWORK_INFO, "invalid data");
	TEST(queryList.queryId[6] == ANQP_ID_DOMAIN_NAME_LIST, "invalid data");
}

static void testEncodeCapabilityList(void)
{
	uint8 buf[BUFFER_SIZE];
	pktEncodeT vendor;
	uint8 vendorQuery[] = {1, 2, 3};
	uint16 query[] = {
		ANQP_ID_VENUE_NAME_INFO,
		ANQP_ID_ROAMING_CONSORTIUM_LIST,
		ANQP_ID_NAI_REALM_LIST,
		ANQP_ID_G3PP_CELLULAR_NETWORK_INFO,
		ANQP_ID_DOMAIN_NAME_LIST,
		ANQP_ID_EMERGENCY_NAI};

	TEST(pktEncodeInit(&vendor, BUFFER_SIZE, buf), "pktEncodeInit failed");
	TEST(pktEncodeHspotAnqpCapabilityList(&vendor,
	sizeof(vendorQuery) / sizeof(uint8), vendorQuery),
	"pktEncodeHspotAnqpCapabilityList failed");

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif
	TEST(pktEncodeAnqpCapabilityList(&enc, sizeof(query) / sizeof(uint16), query,
	pktEncodeLength(&vendor), pktEncodeBuf(&vendor)),
	"pktEncodeAnqpCapabilityList failed");

	WL_PRPKT("testEncodeCapabilityList",
	pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeCapabilityList(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqpCapabilityListT capList;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 2, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.anqpCapabilityListLength,
		anqp.anqpCapabilityListBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpCapabilityList(&ie, &capList),
		"pktDecodeAnqpCapabilityList failed");
	TEST(capList.capLen == 6, "invalid data");
	TEST(capList.capId[0] == ANQP_ID_VENUE_NAME_INFO, "invalid data");
	TEST(capList.capId[1] == ANQP_ID_ROAMING_CONSORTIUM_LIST, "invalid data");
	TEST(capList.capId[2] == ANQP_ID_NAI_REALM_LIST, "invalid data");
	TEST(capList.capId[3] == ANQP_ID_G3PP_CELLULAR_NETWORK_INFO, "invalid data");
	TEST(capList.capId[4] == ANQP_ID_DOMAIN_NAME_LIST, "invalid data");
	TEST(capList.capId[5] == ANQP_ID_EMERGENCY_NAI, "invalid data");
	TEST(capList.hspotCapList.capLen == 3, "invalid data");
	TEST(capList.hspotCapList.capId[0] == 1, "invalid data");
	TEST(capList.hspotCapList.capId[1] == 2, "invalid data");
	TEST(capList.hspotCapList.capId[2] == 3, "invalid data");
}

static void testEncodeVenueName(void)
{
	uint8 buf[BUFFER_SIZE];
	pktEncodeT duple;

	TEST(pktEncodeInit(&duple, BUFFER_SIZE, buf), "pktEncodeInit failed");
	TEST(pktEncodeAnqpVenueDuple(&duple, 2, "EN", 6, "myname"),
		"pktEncodeAnqpVenueDuple failed");
	TEST(pktEncodeAnqpVenueDuple(&duple, 2, "FR", 10, "helloworld"),
		"pktEncodeAnqpVenueDuple failed");
	TEST(pktEncodeAnqpVenueDuple(&duple, 5, "JAPAN", 6, "yrname"),
		"pktEncodeAnqpOperatorNameDuple failed");

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif
	TEST(pktEncodeAnqpVenueName(&enc, VENUE_BUSINESS, 7,
		pktEncodeLength(&duple), pktEncodeBuf(&duple)),
		"pktEncodeAnqpVenueName failed");

	WL_PRPKT("testEncodeVenueName",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeVenueName(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqpVenueNameT venueName;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 3, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.venueNameInfoLength,
		anqp.venueNameInfoBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpVenueName(&ie, &venueName),
		"pktDecodeAnqpVenueName failed");
	TEST(venueName.group == VENUE_BUSINESS, "invalid data");
	TEST(venueName.type == 7, "invalid data");
	TEST(venueName.numVenueName == 3, "invalid data");
	TEST(strcmp(venueName.venueName[0].lang, "EN") == 0, "invalid data");
	TEST(strcmp(venueName.venueName[0].name, "myname") == 0, "invalid data");
	TEST(strcmp(venueName.venueName[1].lang, "FR") == 0, "invalid data");
	TEST(strcmp(venueName.venueName[1].name, "helloworld") == 0, "invalid data");
	TEST(strcmp(venueName.venueName[2].lang, "JAP") == 0, "invalid data");
	TEST(strcmp(venueName.venueName[2].name, "yrname") == 0, "invalid data");
}

static void testEncodeNetworkAuthenticationType(void)
{
	uint8 buf[BUFFER_SIZE];
	pktEncodeT network;

	TEST(pktEncodeInit(&network, BUFFER_SIZE, buf), "pktEncodeInit failed");
	TEST(pktEncodeAnqpNetworkAuthenticationUnit(&network,
		NATI_ONLINE_ENROLLMENT_SUPPORTED, 5, "myurl"),
		"pktEncodeAnqpNetworkAuthenticationUnit failed");
	TEST(pktEncodeAnqpNetworkAuthenticationUnit(&network,
		NATI_ONLINE_ENROLLMENT_SUPPORTED, 5, "yrurl"),
		"pktEncodeAnqpNetworkAuthenticationUnit failed");


#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif
	TEST(pktEncodeAnqpNetworkAuthenticationType(&enc,
		pktEncodeLength(&network), pktEncodeBuf(&network)),
		"pktEncodeAnqpNetworkAuthenticationType failed");

	WL_PRPKT("testEncodeNetworkAuthenticationType",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeNetworkAuthenticationType(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqpNetworkAuthenticationTypeT auth;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 4, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.networkAuthenticationTypeInfoLength,
		anqp.networkAuthenticationTypeInfoBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpNetworkAuthenticationType(&ie, &auth),
		"pktDecodeAnqpNetworkAuthenticationType failed");
	TEST(auth.numAuthenticationType == 2, "invalid data");
	TEST(auth.unit[0].type == NATI_ONLINE_ENROLLMENT_SUPPORTED, "invalid data");
	TEST(strcmp((char *)auth.unit[0].url, "myurl") == 0, "invalid data");
	TEST(auth.unit[1].type == NATI_ONLINE_ENROLLMENT_SUPPORTED, "invalid data");
	TEST(strcmp((char *)auth.unit[1].url, "yrurl") == 0, "invalid data");
}

static void testEncodeRoamingConsortium(void)
{
	uint8 buf[BUFFER_SIZE];
	pktEncodeT oi;

	TEST(pktEncodeInit(&oi, BUFFER_SIZE, buf), "pktEncodeInit failed");
	TEST(pktEncodeAnqpOiDuple(&oi, 4, (uint8 *)"\x00\x11\x22\x33"),
		"pktEncodeAnqpOiDuple failed");
	TEST(pktEncodeAnqpOiDuple(&oi, 3, (uint8 *)"\x12\x34\x56"),
		"pktEncodeAnqpOiDuple failed");

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif
	TEST(pktEncodeAnqpRoamingConsortium(&enc,
		pktEncodeLength(&oi), pktEncodeBuf(&oi)),
		"pktEncodeAnqpRoamingConsortium failed");

	WL_PRPKT("testEncodeRoamingConsortium",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeRoamingConsortium(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqpRoamingConsortiumT roam;
	pktAnqpOiDupleT oi1 = {3, "\x11\x22\x33"};
	pktAnqpOiDupleT oi2 = {3, "\x12\x34\x56"};

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 5, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.roamingConsortiumListLength,
		anqp.roamingConsortiumListBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpRoamingConsortium(&ie, &roam),
		"pktDecodeAnqpRoamingConsortium failed");
	TEST(roam.numOi == 2, "invalid data");
	TEST(memcmp((char *)roam.oi[0].oi, "\x00\x11\x22\x33",
		roam.oi[0].oiLen) == 0, "invalid data");
	TEST(memcmp((char *)roam.oi[1].oi, "\x12\x34\x56",
		roam.oi[1].oiLen) == 0, "invalid data");

	TEST(pktDecodeAnqpIsRoamingConsortium(&roam, &oi1) == FALSE,
		"pktDecodeAnqpIsRoamingConsortium failed");
	TEST(pktDecodeAnqpIsRoamingConsortium(&roam, &oi2) == TRUE,
		"pktDecodeAnqpIsRoamingConsortium failed");
}

static void testEncodeIpAddressType(void)
{
#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif
	TEST(pktEncodeAnqpIpTypeAvailability(&enc,
		IPA_IPV6_AVAILABLE, IPA_IPV4_PORT_RESTRICT_SINGLE_NAT),
		"pktEncodeAnqpIpTypeAvailability failed");

	WL_PRPKT("testEncodeIpAddressType",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeIpAddressType(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqpIpTypeT type;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 6, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.ipAddressTypeAvailabilityInfoLength,
		anqp.ipAddressTypeAvailabilityInfoBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpIpTypeAvailability(&ie, &type),
		"pktDecodeAnqpIpTypeAvailability failed");
	TEST(type.ipv6 == IPA_IPV6_AVAILABLE, "invalid data");
	TEST(type.ipv4 == IPA_IPV4_PORT_RESTRICT_SINGLE_NAT, "invalid data");
}

static void testEncodeNaiRealm(void)
{
	uint8 credential = REALM_SIM;
	int numAuth = 2;
	uint8 authBuf[BUFFER_SIZE];
	pktEncodeT auth;
	int numEap = 3;
	uint8 eapBuf[BUFFER_SIZE];
	pktEncodeT eap;
	int numRealm = 2;
	uint8 realmBuf[BUFFER_SIZE];
	pktEncodeT realm;
	int i;

	TEST(pktEncodeInit(&auth, BUFFER_SIZE, authBuf), "pktEncodeInit failed");
	for (i = 0; i < numAuth; i++) {
		TEST(pktEncodeAnqpAuthenticationSubfield(&auth,
			REALM_CREDENTIAL, sizeof(credential), &credential),
			"pktEncodeAnqpAuthenticationSubfield failed");
	}

	TEST(pktEncodeInit(&eap, BUFFER_SIZE, eapBuf), "pktEncodeInit failed");
	for (i = 0; i < numEap; i++) {
		TEST(pktEncodeAnqpEapMethodSubfield(&eap, REALM_EAP_SIM,
			numAuth, pktEncodeLength(&auth), pktEncodeBuf(&auth)),
			"pktEncodeAnqpEapMethodSubfield failed");
	}

	TEST(pktEncodeInit(&realm, BUFFER_SIZE, realmBuf), "pktEncodeInit failed");
	TEST(pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
		31, (uint8 *)"helloworld;myworld.com;test.com", numEap,
		pktEncodeLength(&eap), pktEncodeBuf(&eap)),
		"pktEncodeAnqpNaiRealmData failed");
	TEST(pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
		11, (uint8 *)"hotspot.com", numEap,
		pktEncodeLength(&eap), pktEncodeBuf(&eap)),
		"pktEncodeAnqpNaiRealmData failed");

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif
	TEST(pktEncodeAnqpNaiRealm(&enc, numRealm,
		pktEncodeLength(&realm), pktEncodeBuf(&realm)),
		"pktEncodeAnqpNaiRealm failed");

	WL_PRPKT("testEncodeNaiRealm",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeNaiRealm(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqpNaiRealmListT realm;
	int i, j, k;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 7, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.naiRealmListLength,
		anqp.naiRealmListBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpNaiRealm(&ie, &realm),
		"pktDecodeAnqpNaiRealm failed");

	TEST(realm.realmCount == 2, "invalid data");

	for (i = 0; i < realm.realmCount; i++) {
		TEST(realm.realm[i].encoding == 0, "invalid data");
		if (i == 0)
			TEST(strcmp((char *)realm.realm[i].realm,
				"helloworld;myworld.com;test.com") == 0, "invalid data");
		else
			TEST(strcmp((char *)realm.realm[i].realm,
				"hotspot.com") == 0, "invalid data");
		TEST(realm.realm[i].eapCount == 3, "invalid data");
		for (j = 0; j < realm.realm[i].eapCount; j++) {
			TEST(realm.realm[i].eap[j].eapMethod == REALM_EAP_SIM, "invalid data");
			TEST(realm.realm[i].eap[j].authCount == 2, "invalid data");
			for (k = 0; k < realm.realm[i].eap[j].authCount; k++) {
				TEST(realm.realm[i].eap[j].auth[k].id ==
					REALM_CREDENTIAL, "invalid data");
				TEST(realm.realm[i].eap[j].auth[k].value[0] ==
					REALM_SIM, "invalid data");
			}
		}
	}

	TEST(pktDecodeAnqpIsRealm(&realm, "hotspot.com", REALM_EAP_SIM, REALM_SIM),
		"pktDecodeAnqpIsRealm failed");
	TEST(pktDecodeAnqpIsRealm(&realm, "helloworld", REALM_EAP_SIM, REALM_SIM),
		"pktDecodeAnqpIsRealm failed");
	TEST(pktDecodeAnqpIsRealm(&realm, "myworld.com", REALM_EAP_SIM, REALM_SIM),
		"pktDecodeAnqpIsRealm failed");
	TEST(pktDecodeAnqpIsRealm(&realm, "test.com", REALM_EAP_SIM, REALM_SIM),
		"pktDecodeAnqpIsRealm failed");
	TEST(!pktDecodeAnqpIsRealm(&realm, "missing.com", REALM_EAP_SIM, REALM_SIM),
		"pktDecodeAnqpIsRealm failed");
}

static void testEncode3GppCellularNetwork(void)
{
	uint8 plmnBuf[BUFFER_SIZE];
	pktEncodeT plmn;

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif

	TEST(pktEncodeInit(&plmn, BUFFER_SIZE, plmnBuf), "pktEncodeInit failed");
	TEST(pktEncodeAnqpPlmn(&plmn, "310", "026"), "pktEncodeAnqpPlmn failed");
	TEST(pktEncodeAnqpPlmn(&plmn, "208", "00"), "pktEncodeAnqpPlmn failed");
	TEST(pktEncodeAnqpPlmn(&plmn, "208", "01"), "pktEncodeAnqpPlmn failed");
	TEST(pktEncodeAnqpPlmn(&plmn, "208", "02"), "pktEncodeAnqpPlmn failed");
	TEST(pktEncodeAnqpPlmn(&plmn, "450", "02"), "pktEncodeAnqpPlmn failed");
	TEST(pktEncodeAnqpPlmn(&plmn, "450", "04"), "pktEncodeAnqpPlmn failed");

	TEST(pktEncodeAnqp3GppCellularNetwork(&enc, 6, pktEncodeLength(&plmn), pktEncodeBuf(&plmn)),
		"pktEncodeAnqp3GppCellularNetwork failed");

	WL_PRPKT("testEncode3GppCellularNetwork",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecode3GppCellularNetwork(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqp3GppCellularNetworkT g3pp;
	pktAnqpPlmnT plmn;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 8, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.g3ppCellularNetworkInfoLength,
		anqp.g3ppCellularNetworkInfoBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqp3GppCellularNetwork(&ie, &g3pp),
		"pktDecodeAnqp3GppCellularNetwork failed");
	TEST(g3pp.plmnCount == 6, "invalid data");
	TEST(strcmp(g3pp.plmn[0].mcc, "310") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[0].mnc, "026") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[1].mcc, "208") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[1].mnc, "00") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[2].mcc, "208") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[2].mnc, "01") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[3].mcc, "208") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[3].mnc, "02") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[4].mcc, "450") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[4].mnc, "02") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[5].mcc, "450") == 0, "invalid data");
	TEST(strcmp(g3pp.plmn[5].mnc, "04") == 0, "invalid data");

	strncpy(plmn.mcc, "310", sizeof(plmn.mcc));
	strncpy(plmn.mnc, "026", sizeof(plmn.mnc));
	TEST(pktDecodeAnqpIs3Gpp(&g3pp, &plmn), "pktDecodeAnqpIs3Gpp failed");

	strncpy(plmn.mcc, "208", sizeof(plmn.mcc));
	strncpy(plmn.mnc, "02", sizeof(plmn.mnc));
	TEST(pktDecodeAnqpIs3Gpp(&g3pp, &plmn), "pktDecodeAnqpIs3Gpp failed");

	strncpy(plmn.mnc, "03", sizeof(plmn.mnc));
	TEST(pktDecodeAnqpIs3Gpp(&g3pp, &plmn) == FALSE, "pktDecodeAnqpIs3Gpp failed");
}

static void testEncodeDomainNameList(void)
{
	uint8 nameBuf[BUFFER_SIZE];
	pktEncodeT name;

	TEST(pktEncodeInit(&name, BUFFER_SIZE, nameBuf), "pktEncodeInit failed");
	TEST(pktEncodeAnqpDomainName(&name, 10, "helloworld"),
		"pktEncodeAnqpDomainName failed");

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif
	TEST(pktEncodeAnqpDomainNameList(&enc,
		pktEncodeLength(&name), pktEncodeBuf(&name)),
		"pktEncodeAnqpDomainNameList failed");

	WL_PRPKT("testEncodeDomainNameList",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeDomainNameList(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqpDomainNameListT list;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 9, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.domainNameListLength,
		anqp.domainNameListBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpDomainNameList(&ie, &list),
		"pktDecodeAnqpDomainNameList failed");
	TEST(list.numDomain == 1, "invalid data");
	TEST(strcmp(list.domain[0].name, "helloworld") == 0, "invalid data");

	TEST(pktDecodeAnqpIsDomainName(&list, "helloworld"),
		"pktDecodeAnqpIsDomainName failed");
}

static void testEncodeQueryVendorSpecific(void)
{
#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif
	TEST(pktEncodeAnqpQueryVendorSpecific(&enc, 3,
		10, (uint8 *)"helloworld"),
		"pktEncodeAnqpQueryVendorSpecific failed");

	WL_PRPKT("testEncodeQueryVendorSpecific",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeQueryVendorSpecific(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	uint16 serviceUpdateIndicator;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 10, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.anqpVendorSpecificListLength,
		anqp.anqpVendorSpecificListBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpQueryVendorSpecific(&ie, &serviceUpdateIndicator),
		"pktDecodeAnqpQueryVendorSpecific failed");
	TEST(serviceUpdateIndicator == 3, "invalid data");
	TEST(pktDecodeRemaining(&ie) == 10, "invalid data");
	TEST(memcmp(pktDecodeCurrentPtr(&ie), "helloworld", 10) == 0, "invalid data");
}

static void testEncodeQueryRequestVendorSpecific(void)
{
	uint8 queryBuf[BUFFER_SIZE];
	pktEncodeT query;

	TEST(pktEncodeInit(&query, BUFFER_SIZE, queryBuf), "pktEncodeInit failed");
	TEST(pktEncodeAnqpQueryRequestVendorSpecificTlv(&query,
		SVC_RPOTYPE_UPNP, 1, 12, (uint8 *)"queryrequest"),
		"pktEncodeAnqpQueryRequestVendorSpecificTlv failed");
	TEST(pktEncodeAnqpQueryRequestVendorSpecificTlv(&query,
		SVC_RPOTYPE_BONJOUR, 2, 12, (uint8 *)"queryrequest"),
		"pktEncodeAnqpQueryRequestVendorSpecificTlv failed");

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif

	TEST(pktEncodeAnqpQueryVendorSpecific(&enc, 0,
		pktEncodeLength(&query), pktEncodeBuf(&query)),
		"pktEncodeAnqpQueryVendorSpecific failed");

	WL_PRPKT("testEncodeQueryRequestVendorSpecific",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeQueryRequestVendorSpecific(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	uint16 serviceUpdateIndicator;
	pktAnqpQueryRequestVendorSpecificTlvT request;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 11, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.anqpVendorSpecificListLength,
		anqp.anqpVendorSpecificListBuffer), "pktDecodeInit failed");
	TEST(pktDecodeAnqpQueryVendorSpecific(&ie, &serviceUpdateIndicator),
		"pktDecodeAnqpQueryVendorSpecific failed");
	TEST(serviceUpdateIndicator == 0, "invalid data");

	TEST(pktDecodeAnqpQueryRequestVendorSpecificTlv(&ie, &request),
		"pktDecodeAnqpQueryRequestVendorSpecificTlv failed");
	TEST(request.serviceProtocolType == SVC_RPOTYPE_UPNP, "invalid data");
	TEST(request.serviceTransactionId == 1, "invalid data");
	TEST(request.queryLen == 12, "invalid data");
	TEST(memcmp(request.queryData, "queryrequest", 12) == 0, "invalid data");

	TEST(pktDecodeAnqpQueryRequestVendorSpecificTlv(&ie, &request),
		"pktDecodeAnqpQueryRequestVendorSpecificTlv failed");
	TEST(request.serviceProtocolType == SVC_RPOTYPE_BONJOUR, "invalid data");
	TEST(request.serviceTransactionId == 2, "invalid data");
	TEST(request.queryLen == 12, "invalid data");
	TEST(memcmp(request.queryData, "queryrequest", 12) == 0, "invalid data");

}

static void testEncodeQueryResponseVendorSpecific(void)
{
	uint8 queryBuf[BUFFER_SIZE];
	pktEncodeT query;

	TEST(pktEncodeInit(&query, BUFFER_SIZE, queryBuf), "pktEncodeInit failed");
	TEST(pktEncodeAnqpQueryResponseVendorSpecificTlv(&query,
		SVC_RPOTYPE_UPNP, 1, 0, 13, (uint8 *)"queryresponse"),
		"pktEncodeAnqpQueryResponseVendorSpecificTlv failed");
	TEST(pktEncodeAnqpQueryResponseVendorSpecificTlv(&query,
		SVC_RPOTYPE_BONJOUR, 2, 0, 13, (uint8 *)"queryresponse"),
		"pktEncodeAnqpQueryResponseVendorSpecificTlv failed");

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif

	TEST(pktEncodeAnqpQueryVendorSpecific(&enc, 0,
		pktEncodeLength(&query), pktEncodeBuf(&query)),
		"pktEncodeAnqpQueryVendorSpecific failed");

	WL_PRPKT("testEncodeQueryResponseVendorSpecific",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));
}

static void testDecodeQueryResponseVendorSpecific(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	uint16 serviceUpdateIndicator;
	pktAnqpQueryResponseVendorSpecificTlvT response;

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");

	TEST(pktDecodeAnqp(&dec, &anqp) == 12, "pktDecodeAnqp failed");

	TEST(pktDecodeInit(&ie, anqp.anqpVendorSpecificListLength,
		anqp.anqpVendorSpecificListBuffer), "pktDecodeInit failed");

	TEST(pktDecodeAnqpQueryVendorSpecific(&ie, &serviceUpdateIndicator),
		"pktDecodeAnqpQueryVendorSpecific failed");
	TEST(serviceUpdateIndicator == 0, "invalid data");

	TEST(pktDecodeAnqpQueryResponseVendorSpecificTlv(&ie, &response),
		"pktDecodeAnqpQueryResponseVendorSpecificTlv failed");
	TEST(response.serviceProtocolType == SVC_RPOTYPE_UPNP, "invalid data");
	TEST(response.serviceTransactionId == 1, "invalid data");
	TEST(response.statusCode == 0, "invalid data");
	TEST(response.queryLen == 13, "invalid data");
	TEST(memcmp(response.queryData, "queryresponse", 13) == 0, "invalid data");

	TEST(pktDecodeAnqpQueryResponseVendorSpecificTlv(&ie, &response),
		"pktDecodeAnqpQueryResponseVendorSpecificTlv failed");
	TEST(response.serviceProtocolType == SVC_RPOTYPE_BONJOUR, "invalid data");
	TEST(response.serviceTransactionId == 2, "invalid data");
	TEST(response.statusCode == 0, "invalid data");
	TEST(response.queryLen == 13, "invalid data");
	TEST(memcmp(response.queryData, "queryresponse", 13) == 0, "invalid data");
}

static void testEncodeHspotAnqp(void)
{
	uint8 data[8];
	int i;

	for (i = 0; i < 8; i++)
		data[i] = i;

#if NO_IE_APPEND
	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
#endif

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
}

static void testEmpty3GppCellularNetwork(void)
{
	pktDecodeT dec;
	pktDecodeAnqpT anqp;
	pktDecodeT ie;
	pktAnqp3GppCellularNetworkT g3pp;

	TEST(pktEncodeInit(&enc, BUFFER_SIZE, buffer), "pktEncodeInit failed");
	TEST(pktEncodeAnqp3GppCellularNetwork(&enc, 0, 0, 0),
		"pktEncodeAnqp3GppCellularNetwork failed");
	WL_PRPKT("testEncode3GppCellularNetwork",
		pktEncodeBuf(&enc), pktEncodeLength(&enc));

	TEST(pktDecodeInit(&dec, pktEncodeLength(&enc),
		pktEncodeBuf(&enc)), "pktDecodeInit failed");
	TEST(pktDecodeAnqp(&dec, &anqp) == 1, "pktDecodeAnqp failed");
	TEST(pktDecodeInit(&ie, anqp.g3ppCellularNetworkInfoLength,
		anqp.g3ppCellularNetworkInfoBuffer), "pktDecodeInit failed");
	TEST(pktDecodeAnqp3GppCellularNetwork(&ie, &g3pp),
		"pktDecodeAnqp3GppCellularNetwork failed");
}

int main(int argc, char **argv)
{
	(void) argc;
	(void) argv;

	TRACE_LEVEL_SET(TRACE_DEBUG | TRACE_PACKET);
	TEST_INITIALIZE();

	testEncodeQueryList();
	testDecodeQueryList();

	testEncodeCapabilityList();
	testDecodeCapabilityList();

	testEncodeVenueName();
	testDecodeVenueName();

	testEncodeNetworkAuthenticationType();
	testDecodeNetworkAuthenticationType();

	testEncodeRoamingConsortium();
	testDecodeRoamingConsortium();

	testEncodeIpAddressType();
	testDecodeIpAddressType();

	testEncodeNaiRealm();
	testDecodeNaiRealm();

	testEncode3GppCellularNetwork();
	testDecode3GppCellularNetwork();

	testEncodeDomainNameList();
	testDecodeDomainNameList();

	testEncodeQueryVendorSpecific();
	testDecodeQueryVendorSpecific();

	testEncodeQueryRequestVendorSpecific();
	testDecodeQueryRequestVendorSpecific();

	testEncodeQueryResponseVendorSpecific();
	testDecodeQueryResponseVendorSpecific();

	testEncodeHspotAnqp();

	testEmpty3GppCellularNetwork();

	TEST_FINALIZE();
	return 0;
}
