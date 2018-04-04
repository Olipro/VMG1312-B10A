/*
 * Encoding of Hotspot2.0 ANQP packets.
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
#include "pktEncodeHspotAnqp.h"

static void encodeHspotAnqpHeader(pktEncodeT *pkt, uint16 len, uint8 subtype)
{
	pktEncodeLe16(pkt, ANQP_ID_VENDOR_SPECIFIC_LIST);
	pktEncodeLe16(pkt, HSPOT_LENGTH_OVERHEAD + len);
	pktEncodeBytes(pkt, WFA_OUI_LEN, (uint8 *)WFA_OUI);
	pktEncodeByte(pkt, HSPOT_ANQP_OUI_TYPE);
	pktEncodeByte(pkt, subtype);
	pktEncodeByte(pkt, 0);		/* reserved */
}

/* encode query list */
int pktEncodeHspotAnqpQueryList(pktEncodeT *pkt, uint16 queryLen, uint8 *query)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, queryLen, HSPOT_SUBTYPE_QUERY_LIST);
	pktEncodeBytes(pkt, queryLen, query);

	return pktEncodeLength(pkt) - initLen;
}

/* encode capability list */
int pktEncodeHspotAnqpCapabilityList(pktEncodeT *pkt, uint16 capLen, uint8 *cap)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, capLen, HSPOT_SUBTYPE_CAPABILITY_LIST);
	pktEncodeBytes(pkt, capLen, cap);

	return pktEncodeLength(pkt) - initLen;
}

/* encode operator friendly name */
int pktEncodeHspotAnqpOperatorNameDuple(pktEncodeT *pkt, uint8 langLen, char *lang,
	uint8 nameLen, char *name)
{
	int initLen = pktEncodeLength(pkt);
	int len = langLen <= VENUE_LANGUAGE_CODE_SIZE ? langLen : VENUE_LANGUAGE_CODE_SIZE;

	pktEncodeByte(pkt, VENUE_LANGUAGE_CODE_SIZE + nameLen);
	pktEncodeBytes(pkt, len, (uint8 *)lang);
	while (pktEncodeLength(pkt) - initLen < VENUE_LANGUAGE_CODE_SIZE + 1)
		pktEncodeByte(pkt, 0);
	pktEncodeBytes(pkt, nameLen, (uint8 *)name);

	return pktEncodeLength(pkt) - initLen;
}

int pktEncodeHspotAnqpOperatorFriendlyName(pktEncodeT *pkt,
	uint16 nameLen, uint8 *name)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, nameLen, HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME);
	pktEncodeBytes(pkt, nameLen, name);

	return pktEncodeLength(pkt) - initLen;
}

/* encode WAN metrics */
int pktEncodeHspotAnqpWanMetrics(pktEncodeT *pkt,
	uint8 linkStatus, uint8 symmetricLink,
	uint8 atCapacity, uint32 dlinkSpeed, uint32 ulinkSpeed,
	uint8 dlinkLoad, uint8 ulinkLoad, uint16 lmd)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, 13, HSPOT_SUBTYPE_WAN_METRICS);
	pktEncodeByte(pkt,
		linkStatus << HSPOT_WAN_LINK_STATUS_SHIFT |
		symmetricLink << HSPOT_WAN_SYMMETRIC_LINK_SHIFT |
		atCapacity << HSPOT_WAN_AT_CAPACITY_SHIFT);
	pktEncodeLe32(pkt, dlinkSpeed);
	pktEncodeLe32(pkt, ulinkSpeed);
	pktEncodeByte(pkt, dlinkLoad);
	pktEncodeByte(pkt, ulinkLoad);
	pktEncodeLe16(pkt, lmd);

	return pktEncodeLength(pkt) - initLen;
}

/* encode connection capability */
int pktEncodeHspotAnqpProtoPortTuple(pktEncodeT *pkt,
	uint8 ipProtocol, uint16 portNumber, uint8 status)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, ipProtocol);
	pktEncodeLe16(pkt, portNumber);
	pktEncodeByte(pkt, status);

	return pktEncodeLength(pkt) - initLen;
}

int pktEncodeHspotAnqpConnectionCapability(pktEncodeT *pkt, uint16 capLen, uint8 *cap)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, capLen, HSPOT_SUBTYPE_CONNECTION_CAPABILITY);
	pktEncodeBytes(pkt, capLen, cap);

	return pktEncodeLength(pkt) - initLen;
}

/* encode NAI home realm query */
int pktEncodeHspotAnqpNaiHomeRealmName(pktEncodeT *pkt, uint8 encoding,
	uint8 nameLen, char *name)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, encoding);
	pktEncodeByte(pkt, nameLen);
	pktEncodeBytes(pkt, nameLen, (uint8 *)name);

	return pktEncodeLength(pkt) - initLen;
}

int pktEncodeHspotAnqpNaiHomeRealmQuery(pktEncodeT *pkt, uint8 count,
	uint16 nameLen, uint8 *name)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, nameLen + 1, HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY);
	pktEncodeByte(pkt, count);
	pktEncodeBytes(pkt, nameLen, name);

	return pktEncodeLength(pkt) - initLen;
}

/* encode operating class indication */
int pktEncodeHspotAnqpOperatingClassIndication(pktEncodeT *pkt,
	uint16 opClassLen, uint8 *opClass)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, opClassLen, HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION);
	pktEncodeBytes(pkt, opClassLen, opClass);

	return pktEncodeLength(pkt) - initLen;
}

/* encode icon metadata */
int pktEncodeHspotAnqpIconMetadata(pktEncodeT *pkt,
	uint16 width, uint16 height, char *lang,
	uint8 typeLength, uint8 *type, uint8 filenameLength, uint8 *filename)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, width);
	pktEncodeLe16(pkt, height);
	pktEncodeBytes(pkt, VENUE_LANGUAGE_CODE_SIZE, (uint8 *)lang);
	pktEncodeByte(pkt, typeLength);
	pktEncodeBytes(pkt, typeLength, type);
	pktEncodeByte(pkt, filenameLength);
	pktEncodeBytes(pkt, filenameLength, filename);

	return pktEncodeLength(pkt) - initLen;
}

/* encode OSU provider */
int pktEncodeHspotAnqpOsuProvider(pktEncodeT *pkt,
	uint16 nameLength, uint8 *name,	uint8 uriLength, uint8 *uri,
	uint8 methodLength, uint8 *method, uint16 iconLength, uint8 *icon,
	uint8 naiLength, uint8 *nai, uint16 descLength, uint8 *desc)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, 5 + nameLength + uriLength +
		methodLength + iconLength + naiLength + descLength);
	pktEncodeBytes(pkt, nameLength, name);
	pktEncodeByte(pkt, uriLength);
	pktEncodeBytes(pkt, uriLength, uri);
	pktEncodeByte(pkt, methodLength);
	pktEncodeBytes(pkt, methodLength, method);
	pktEncodeLe16(pkt, iconLength);
	pktEncodeBytes(pkt, iconLength, icon);
	pktEncodeByte(pkt, naiLength);
	pktEncodeBytes(pkt, naiLength, nai);
	pktEncodeBytes(pkt, descLength, desc);

	return pktEncodeLength(pkt) - initLen;
}

/* encode OSU provider list */
int pktEncodeHspotAnqpOsuProviderList(pktEncodeT *pkt,
	uint16 nonTransProfileLength, uint8 *nonTransProfile,
	uint8 legacyOsuSsidLength, uint8 *legacyOsuSsid,
	uint16 providerLength, uint8 *provider)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, 3 + nonTransProfileLength + legacyOsuSsidLength +
		providerLength, HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS);
	pktEncodeLe16(pkt, nonTransProfileLength);
	if (nonTransProfileLength > 0)
		pktEncodeBytes(pkt, nonTransProfileLength, nonTransProfile);
	pktEncodeByte(pkt, legacyOsuSsidLength);
	if (legacyOsuSsidLength > 0)
		pktEncodeBytes(pkt, legacyOsuSsidLength, legacyOsuSsid);
	pktEncodeBytes(pkt, providerLength, provider);

	return pktEncodeLength(pkt) - initLen;
}

/* encode anonymous NAI */
int pktEncodeHspotAnqpAnonymousNai(pktEncodeT *pkt, uint16 length, uint8 *nai)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, 2 + length, HSPOT_SUBTYPE_ANONYMOUS_NAI);
	pktEncodeLe16(pkt, length);
	pktEncodeBytes(pkt, length, nai);

	return pktEncodeLength(pkt) - initLen;
}

/* encode icon request */
int pktEncodeHspotAnqpIconRequest(pktEncodeT *pkt, uint16 length, uint8 *filename)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, length, HSPOT_SUBTYPE_ICON_REQUEST);
	pktEncodeBytes(pkt, length, filename);

	return pktEncodeLength(pkt) - initLen;
}

/* encode icon binary file */
int pktEncodeHspotAnqpIconBinaryFile(pktEncodeT *pkt,
	uint8 status, uint8 typeLength, uint8 *type, uint16 length, uint8 *data)
{
	int initLen = pktEncodeLength(pkt);

	encodeHspotAnqpHeader(pkt, 2 + typeLength + 2 + length, HSPOT_SUBTYPE_ICON_BINARY_FILE);
	pktEncodeByte(pkt, status);
	pktEncodeByte(pkt, typeLength);
	pktEncodeBytes(pkt, typeLength, type);
	pktEncodeLe16(pkt, length);
	pktEncodeBytes(pkt, length, data);

	return pktEncodeLength(pkt) - initLen;
}
