/*
 * Encoding of information elememts.
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
#include "pktHspot.h"
#include "pktEncodeIe.h"

/* encode hotspot 2.0 indication */
int pktEncodeIeHotspotIndication(pktEncodeT *pkt, uint8 hotspotConfig)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_MNG_VS_ID);
	pktEncodeByte(pkt, 5);
	pktEncodeBytes(pkt, WFA_OUI_LEN, (uint8 *)WFA_OUI);
	pktEncodeByte(pkt, HSPOT_IE_OUI_TYPE);
	pktEncodeByte(pkt, hotspotConfig);

	return pktEncodeLength(pkt) - initLen;
}

/* encode hotspot 2.0 indication release2 */
int pktEncodeIeHotspotIndication2(pktEncodeT *pkt,
	int isDgafDisabled, int isOsuBssid, uint8 releaseNumber)
{
	int initLen = pktEncodeLength(pkt);
	uint8 config = 0;

	pktEncodeByte(pkt, DOT11_MNG_VS_ID);
	pktEncodeByte(pkt, 5);
	pktEncodeBytes(pkt, WFA_OUI_LEN, (uint8 *)WFA_OUI);
	pktEncodeByte(pkt, HSPOT_IE_OUI_TYPE);
	if (isDgafDisabled)
		config |= HSPOT_DGAF_DISABLED_MASK;
	if (isOsuBssid)
		config |= HSPOT_OSU_BSSID_MASK;
	config |= (releaseNumber << HSPOT_RELEASE_SHIFT) & HSPOT_RELEASE_MASK;
	pktEncodeByte(pkt, config);

	return pktEncodeLength(pkt) - initLen;
}

/* encode interworking */
int pktEncodeIeInterworking(pktEncodeT *pkt, uint8 accessNetworkType,
	int isInternet, int isAsra, int isEsr, int isUesa,
	int isVenue, uint8 venueGroup, uint8 venueType, struct ether_addr *hessid)
{
	int initLen = pktEncodeLength(pkt);
	int len = 1;
	uint8 options = 0;

	pktEncodeByte(pkt, DOT11_MNG_INTERWORKING_ID);

	if (isVenue)
		len += 2;
	if (hessid != 0)
		len += sizeof(*hessid);
	pktEncodeByte(pkt, len);

	options = accessNetworkType & IW_ANT_MASK;
	if (isInternet)
		options |= IW_INTERNET_MASK;
	if (isAsra)
		options |= IW_ASRA_MASK;
	if (isEsr)
		options |= IW_ESR_MASK;
	if (isUesa)
		options |= IW_UESA_MASK;
	pktEncodeByte(pkt, options);

	if (isVenue) {
		pktEncodeByte(pkt, venueGroup);
		pktEncodeByte(pkt, venueType);
	}

	if (hessid != 0)
		pktEncodeBytes(pkt, sizeof(*hessid), hessid->octet);

	return pktEncodeLength(pkt) - initLen;
}

/* encode advertisement protocol tuple */
int pktEncodeIeAdvertisementProtocolTuple(pktEncodeT *pkt,
	int isPamebi, uint8 qResponseLimit, uint8 protocolId)
{
	int initLen = pktEncodeLength(pkt);
	uint8 info;

	info = qResponseLimit & ADVP_QRL_MASK;
	if (isPamebi)
		info |= ADVP_PAME_BI_MASK;
	pktEncodeByte(pkt, info);
	pktEncodeByte(pkt, protocolId);

	return pktEncodeLength(pkt) - initLen;
}

/* encode advertisement protocol */
int pktEncodeIeAdvertiseProtocol(pktEncodeT *pkt, uint8 len, uint8 *data)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_MNG_ADVERTISEMENT_ID);
	pktEncodeByte(pkt, len);
	pktEncodeBytes(pkt, len, data);

	return pktEncodeLength(pkt) - initLen;
}

/* encode roaming consortium */
int pktEncodeIeRoamingConsortium(pktEncodeT *pkt, uint8 numAnqpOi,
	uint8 oi1Len, uint8 *oi1, uint8 oi2Len, uint8 *oi2,
	uint8 oi3Len, uint8 *oi3)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_MNG_ROAM_CONSORT_ID);
	pktEncodeByte(pkt, 2 + oi1Len + oi2Len + oi3Len);
	pktEncodeByte(pkt, numAnqpOi);
	pktEncodeByte(pkt, oi2Len << 4 | (oi1Len & 0xf));
	if (oi1Len > 0)
		pktEncodeBytes(pkt, oi1Len, oi1);
	if (oi2Len > 0)
		pktEncodeBytes(pkt, oi2Len, oi2);
	if (oi3Len > 0)
		pktEncodeBytes(pkt, oi3Len, oi3);

	return pktEncodeLength(pkt) - initLen;
}

/* encode extended capabilities */
int pktEncodeIeExtendedCapabilities(pktEncodeT *pkt, uint32 cap)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_MNG_EXT_CAP_ID);
	pktEncodeByte(pkt, 4);
	pktEncodeLe32(pkt, cap);

	return pktEncodeLength(pkt) - initLen;
}

/* encode advertisement protocol */
int pktEncodeIeAdvertisementProtocol(pktEncodeT *pkt,
	uint8 pamebi, uint8 qRspLimit, uint8 id)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_MNG_ADVERTISEMENT_ID);
	pktEncodeByte(pkt, 2);
	pktEncodeByte(pkt, (pamebi << 7) | (qRspLimit & 0x7f));
	pktEncodeByte(pkt, id);

	return pktEncodeLength(pkt) - initLen;
}
