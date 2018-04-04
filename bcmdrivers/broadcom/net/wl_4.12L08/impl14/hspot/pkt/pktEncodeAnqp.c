/*
 * Encoding of 802.11u ANQP packets.
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
#include "pktEncodeAnqp.h"

/* encode ANQP query list */
int pktEncodeAnqpQueryList(pktEncodeT *pkt, uint16 queryLen, uint16 *queryId)
{
	int initLen = pktEncodeLength(pkt);
	int i;

	pktEncodeLe16(pkt, ANQP_ID_QUERY_LIST);
	pktEncodeLe16(pkt, queryLen * sizeof(uint16));
	for (i = 0; i < queryLen; i++)
		pktEncodeLe16(pkt, queryId[i]);

	return pktEncodeLength(pkt) - initLen;
}

/* encode ANQP capability list */
int pktEncodeAnqpCapabilityList(pktEncodeT *pkt, uint16 capLen, uint16 *capList,
	uint16 vendorLen, uint8 *vendorList)
{
	int initLen = pktEncodeLength(pkt);
	int i;

	pktEncodeLe16(pkt, ANQP_ID_CAPABILITY_LIST);
	pktEncodeLe16(pkt, capLen * sizeof(uint16) + vendorLen);
	for (i = 0; i < capLen; i++)
		pktEncodeLe16(pkt, capList[i]);
	if (vendorLen > 0)
		pktEncodeBytes(pkt, vendorLen, vendorList);

	return pktEncodeLength(pkt) - initLen;
}

/* encode venue name duple */
int pktEncodeAnqpVenueDuple(pktEncodeT *pkt, uint8 langLen, char *lang,
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

/* encode venue name */
int pktEncodeAnqpVenueName(pktEncodeT *pkt, uint8 group, uint8 type,
	uint16 nameLen, uint8 *name)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, ANQP_ID_VENUE_NAME_INFO);
	pktEncodeLe16(pkt, nameLen + 2);
	pktEncodeByte(pkt, group);
	pktEncodeByte(pkt, type);
	pktEncodeBytes(pkt, nameLen, name);

	return pktEncodeLength(pkt) - initLen;
}

/* encode network authentication unit */
int pktEncodeAnqpNetworkAuthenticationUnit(pktEncodeT *pkt, uint8 type,
	uint16 urlLen, char *url)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, type);
	pktEncodeLe16(pkt, urlLen);
	pktEncodeBytes(pkt, urlLen, (uint8 *)url);

	return pktEncodeLength(pkt) - initLen;
}

/* encode network authentication type */
int pktEncodeAnqpNetworkAuthenticationType(pktEncodeT *pkt, uint16 len, uint8 *data)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO);
	pktEncodeLe16(pkt, len);
	pktEncodeBytes(pkt, len, data);

	return pktEncodeLength(pkt) - initLen;
}

/* encode OI duple */
int pktEncodeAnqpOiDuple(pktEncodeT *pkt, uint8 oiLen, uint8 *oi)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, oiLen);
	pktEncodeBytes(pkt, oiLen, oi);

	return pktEncodeLength(pkt) - initLen;
}

/* encode roaming consortium */
int pktEncodeAnqpRoamingConsortium(pktEncodeT *pkt, uint16 len, uint8 *data)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, ANQP_ID_ROAMING_CONSORTIUM_LIST);
	pktEncodeLe16(pkt, len);
	pktEncodeBytes(pkt, len, data);

	return pktEncodeLength(pkt) - initLen;
}

/* encode IP address type availability */
int pktEncodeAnqpIpTypeAvailability(pktEncodeT *pkt, uint8 ipv6, uint8 ipv4)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO);
	pktEncodeLe16(pkt, 1);
	pktEncodeByte(pkt, ((ipv4 << IPA_IPV4_SHIFT) & IPA_IPV4_MASK) |
		((ipv6 << IPA_IPV6_SHIFT) & IPA_IPV6_MASK));

	return pktEncodeLength(pkt) - initLen;
}

/* encode authentication parameter subfield */
int pktEncodeAnqpAuthenticationSubfield(pktEncodeT *pkt,
	uint8 id, uint8 authLen, uint8 *auth)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, id);
	pktEncodeByte(pkt, authLen);
	pktEncodeBytes(pkt, authLen, auth);

	return pktEncodeLength(pkt) - initLen;
}

/* encode EAP method subfield */
int pktEncodeAnqpEapMethodSubfield(pktEncodeT *pkt, uint8 method,
	uint8 authCount, uint8 authLen, uint8 *auth)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, authLen + 2);
	pktEncodeByte(pkt, method);
	pktEncodeByte(pkt, authCount);
	pktEncodeBytes(pkt, authLen, auth);

	return pktEncodeLength(pkt) - initLen;
}

/* encode EAP method */
int pktEncodeAnqpEapMethod(pktEncodeT *pkt, uint8 method,
	uint8 authCount, uint16 authLen, uint8 *auth)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, method);
	pktEncodeByte(pkt, authCount);
	pktEncodeBytes(pkt, authLen, auth);

	return pktEncodeLength(pkt) - initLen;
}

/* encode NAI realm data */
int pktEncodeAnqpNaiRealmData(pktEncodeT *pkt, uint8 encoding,
	uint8 realmLen, uint8 *realm,
	uint8 eapCount, uint16 eapLen, uint8 *eap)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, 3 + realmLen + eapLen);
	pktEncodeByte(pkt, encoding);
	pktEncodeByte(pkt, realmLen);
	pktEncodeBytes(pkt, realmLen, realm);
	pktEncodeByte(pkt, eapCount);
	pktEncodeBytes(pkt, eapLen, eap);

	return pktEncodeLength(pkt) - initLen;
}

/* encode NAI realm */
int pktEncodeAnqpNaiRealm(pktEncodeT *pkt, uint16 realmCount,
	uint16 len, uint8 *data)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, ANQP_ID_NAI_REALM_LIST);
	pktEncodeLe16(pkt, len + 2);
	pktEncodeLe16(pkt, realmCount);
	pktEncodeBytes(pkt, len, data);

	return pktEncodeLength(pkt) - initLen;
}

/* encode PLMN */
int pktEncodeAnqpPlmn(pktEncodeT *pkt, char *mcc, char *mnc)
{
	int initLen = pktEncodeLength(pkt);
	int i;

	if (!(strlen(mcc) == 3 && (strlen(mnc) == 3 || strlen(mnc) == 2)))
		return 0;

	for (i = 0; i < (int)strlen(mcc); i++) {
		if (!isdigit(mcc[i]))
			return 0;
	}

	for (i = 0; i < (int)strlen(mnc); i++) {
		if (!isdigit(mnc[i]))
			return 0;
	}

	/* mcc digit 2 | mcc digit 1 */
	pktEncodeByte(pkt, (mcc[1] - '0') << 4 | (mcc[0] - '0'));
	if (strlen(mnc) == 2)
		/* mnc digit 3 | mcc digit 3 */
		pktEncodeByte(pkt, 0x0f << 4 | (mcc[2] - '0'));
	else
		/* mnc digit 3 | mcc digit 3 */
		pktEncodeByte(pkt, (mnc[2] - '0') << 4 | (mcc[2] - '0'));
	/* mnc digit 2 | mnc digit 1 */
	pktEncodeByte(pkt, (mnc[1] - '0') << 4 | (mnc[0] - '0'));

	return pktEncodeLength(pkt) - initLen;
}

/* encode 3GPP cellular network */
int pktEncodeAnqp3GppCellularNetwork(pktEncodeT *pkt,
	uint8 plmnCount, uint16 plmnLen, uint8 *plmnData)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, ANQP_ID_G3PP_CELLULAR_NETWORK_INFO);
	pktEncodeLe16(pkt, 5 + plmnLen);
	pktEncodeByte(pkt, G3PP_GUD_VERSION);
	pktEncodeByte(pkt, 3 + plmnLen);
	pktEncodeByte(pkt, G3PP_PLMN_LIST_IE);
	pktEncodeByte(pkt, 1 + plmnLen);
	pktEncodeByte(pkt, plmnCount);
	pktEncodeBytes(pkt, plmnLen, plmnData);

	return pktEncodeLength(pkt) - initLen;
}

/* encode domain name */
int pktEncodeAnqpDomainName(pktEncodeT *pkt, uint8 nameLen, char *name)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, nameLen);
	pktEncodeBytes(pkt, nameLen, (uint8 *)name);

	return pktEncodeLength(pkt) - initLen;
}

/* encode domain name list */
int pktEncodeAnqpDomainNameList(pktEncodeT *pkt, uint16 domainLen, uint8 *domain)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, ANQP_ID_DOMAIN_NAME_LIST);
	pktEncodeLe16(pkt, domainLen);
	pktEncodeBytes(pkt, domainLen, domain);

	return pktEncodeLength(pkt) - initLen;
}

/* encode ANQP query vendor specific */
int pktEncodeAnqpQueryVendorSpecific(pktEncodeT *pkt,
	uint16 serviceUpdateIndicator, uint16 dataLen, uint8 *data)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, ANQP_ID_VENDOR_SPECIFIC_LIST);
	pktEncodeLe16(pkt, WFA_OUI_LEN + 1 + 2 + dataLen);
	pktEncodeBytes(pkt, WFA_OUI_LEN, (uint8 *)WFA_OUI);
	pktEncodeByte(pkt, ANQP_OUI_SUBTYPE);
	pktEncodeLe16(pkt, serviceUpdateIndicator);
	pktEncodeBytes(pkt, dataLen, data);

	return pktEncodeLength(pkt) - initLen;
}

/* encode ANQP query vendor specific */
static int pktEncodeAnqpQueryVendorSpecificTlv(pktEncodeT *pkt,
	uint8 serviceProtocolType, uint8 serviceTransactionId,
	int isStatusCode, uint8 statusCode,
	uint16 queryLen, uint8 *queryData)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeLe16(pkt, 2 + (isStatusCode ? 1 : 0) + queryLen);
	pktEncodeByte(pkt, serviceProtocolType);
	pktEncodeByte(pkt, serviceTransactionId);
	if (isStatusCode)
		pktEncodeByte(pkt, statusCode);
	pktEncodeBytes(pkt, queryLen, queryData);

	return pktEncodeLength(pkt) - initLen;
}

/* encode ANQP query request vendor specific TLV */
int pktEncodeAnqpQueryRequestVendorSpecificTlv(pktEncodeT *pkt,
	uint8 serviceProtocolType, uint8 serviceTransactionId,
	uint16 queryLen, uint8 *queryData)
{
	return pktEncodeAnqpQueryVendorSpecificTlv(pkt,
		serviceProtocolType, serviceTransactionId,
		FALSE, 0, queryLen, queryData);
}

/* encode ANQP query response vendor specific TLV */
int pktEncodeAnqpQueryResponseVendorSpecificTlv(pktEncodeT *pkt,
	uint8 serviceProtocolType, uint8 serviceTransactionId, uint8 statusCode,
	uint16 queryLen, uint8 *queryData)
{
	return pktEncodeAnqpQueryVendorSpecificTlv(pkt,
		serviceProtocolType, serviceTransactionId,
		TRUE, statusCode, queryLen, queryData);
}
