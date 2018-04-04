/*
 * Decoding of 802.11u ANQP packets.
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
#include "pktDecodeAnqp.h"

static void printAnqpDecode(pktDecodeAnqpT *anqp)
{
	WL_P2PO(("decoded ANQP IEs:\n"));

	if (anqp->anqpQueryListBuffer) {
		WL_PRPKT("   ANQP_QUERY_LIST",
			anqp->anqpQueryListBuffer, anqp->anqpQueryListLength);
	}
	if (anqp->anqpCapabilityListBuffer) {
		WL_PRPKT("   ANQP_CAPABILITY_LIST",
			anqp->anqpCapabilityListBuffer, anqp->anqpCapabilityListLength);
	}
	if (anqp->venueNameInfoBuffer) {
		WL_PRPKT("   VENUE_NAME_INFO",
			anqp->venueNameInfoBuffer, anqp->venueNameInfoLength);
	}
	if (anqp->emergencyCallNumberInfoBuffer) {
		WL_PRPKT("   EMERGENCY_CALL_NUMBER_INFO",
			anqp->emergencyCallNumberInfoBuffer,
			anqp->emergencyCallNumberInfoLength);
	}
	if (anqp->networkAuthenticationTypeInfoBuffer) {
		WL_PRPKT("   NETWORK_AUTHENTICATION_TYPE_INFO",
			anqp->networkAuthenticationTypeInfoBuffer,
			anqp->networkAuthenticationTypeInfoLength);
	}
	if (anqp->roamingConsortiumListBuffer) {
		WL_PRPKT("   ROAMING_CONSORTIUM_LIST",
			anqp->roamingConsortiumListBuffer,
			anqp->roamingConsortiumListLength);
	}
	if (anqp->ipAddressTypeAvailabilityInfoBuffer) {
		WL_PRPKT("   IP_ADDRESS_TYPE_AVAILABILITY_INFO",
			anqp->ipAddressTypeAvailabilityInfoBuffer,
			anqp->ipAddressTypeAvailabilityInfoLength);
	}
	if (anqp->naiRealmListBuffer) {
		WL_PRPKT("   NAI_REALM_LIST",
			anqp->naiRealmListBuffer, anqp->naiRealmListLength);
	}
	if (anqp->g3ppCellularNetworkInfoBuffer) {
		WL_PRPKT("   G3PP_CELLULAR_NETWORK_INFO",
			anqp->g3ppCellularNetworkInfoBuffer,
			anqp->g3ppCellularNetworkInfoLength);
	}
	if (anqp->apGeospatialLocationBuffer) {
		WL_PRPKT("   AP_GEOSPATIAL_LOCATION",
			anqp->apGeospatialLocationBuffer, anqp->apGeospatialLocationLength);
	}
	if (anqp->apCivicLocationBuffer) {
		WL_PRPKT("   AP_CIVIC_LOCATION",
			anqp->apCivicLocationBuffer, anqp->apCivicLocationLength);
	}
	if (anqp->apLocationPublicIdUriBuffer) {
		WL_PRPKT("   AP_LOCATION_PUBLIC_ID_URI",
			anqp->apLocationPublicIdUriBuffer,
			anqp->apLocationPublicIdUriLength);
	}
	if (anqp->domainNameListBuffer) {
		WL_PRPKT("   DOMAIN_NAME_LIST",
			anqp->domainNameListBuffer, anqp->domainNameListLength);
	}
	if (anqp->emergencyAlertIdUriBuffer) {
		WL_PRPKT("   EMERGENCY_ALERT_ID_URI",
			anqp->emergencyAlertIdUriBuffer, anqp->emergencyAlertIdUriLength);
	}
	if (anqp->emergencyNaiBuffer) {
		WL_PRPKT("   EMERGENCY_NAI",
			anqp->emergencyNaiBuffer, anqp->emergencyNaiLength);
	}
	if (anqp->anqpVendorSpecificListBuffer) {
		WL_PRPKT("   ANQP_VENDOR_SPECIFIC_LIST",
			anqp->anqpVendorSpecificListBuffer,
			anqp->anqpVendorSpecificListLength);
	}

	printHspotAnqpDecode(&anqp->hspot);
}

/* decode ANQP */
int pktDecodeAnqp(pktDecodeT *pkt, pktDecodeAnqpT *anqp)
{
	int nextIeOffset = 0;
	int ieCount = 0;

	WL_PRPKT("packet for ANQP decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(anqp, 0, sizeof(*anqp));

	while (nextIeOffset < pktDecodeBufLength(pkt)) {
		uint16 id;
		uint16 length;
		int dataLength;
		uint8 *dataPtr;

		pktDecodeOffset(pkt) = nextIeOffset;
		WL_TRACE(("decoding offset 0x%x\n", pktDecodeOffset(pkt)));

		/* minimum ID and length */
		if (pktDecodeRemaining(pkt) < 4) {
			WL_P2PO(("ID and length too short\n"));
			break;
		}

		pktDecodeLe16(pkt, &id);
		pktDecodeLe16(pkt, &length);

		/* check length */
		if (length > pktDecodeRemaining(pkt)) {
			WL_P2PO(("length exceeds packet %d > %d\n",
				length, pktDecodeRemaining(pkt)));
			break;
		}
		nextIeOffset = pktDecodeOffset(pkt) + length;

		/* data */
		dataLength = length;
		dataPtr = pktDecodeCurrentPtr(pkt);

		switch (id)
		{
		case ANQP_ID_QUERY_LIST:
			anqp->anqpQueryListLength = dataLength;
			anqp->anqpQueryListBuffer = dataPtr;
			break;
		case ANQP_ID_CAPABILITY_LIST:
			anqp->anqpCapabilityListLength = dataLength;
			anqp->anqpCapabilityListBuffer = dataPtr;
			break;
		case ANQP_ID_VENUE_NAME_INFO:
			anqp->venueNameInfoLength = dataLength;
			anqp->venueNameInfoBuffer = dataPtr;
			break;
		case ANQP_ID_EMERGENCY_CALL_NUMBER_INFO:
			anqp->emergencyCallNumberInfoLength = dataLength;
			anqp->emergencyCallNumberInfoBuffer = dataPtr;
			break;
		case ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO:
			anqp->networkAuthenticationTypeInfoLength = dataLength;
			anqp->networkAuthenticationTypeInfoBuffer = dataPtr;
			break;
		case ANQP_ID_ROAMING_CONSORTIUM_LIST:
			anqp->roamingConsortiumListLength = dataLength;
			anqp->roamingConsortiumListBuffer = dataPtr;
			break;
		case ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO:
			anqp->ipAddressTypeAvailabilityInfoLength = dataLength;
			anqp->ipAddressTypeAvailabilityInfoBuffer = dataPtr;
			break;
		case ANQP_ID_NAI_REALM_LIST:
			anqp->naiRealmListLength = dataLength;
			anqp->naiRealmListBuffer = dataPtr;
			break;
		case ANQP_ID_G3PP_CELLULAR_NETWORK_INFO:
			anqp->g3ppCellularNetworkInfoLength = dataLength;
			anqp->g3ppCellularNetworkInfoBuffer = dataPtr;
			break;
		case ANQP_ID_AP_GEOSPATIAL_LOCATION:
			anqp->apGeospatialLocationLength = dataLength;
			anqp->apGeospatialLocationBuffer = dataPtr;
			break;
		case ANQP_ID_AP_CIVIC_LOCATION:
			anqp->apCivicLocationLength = dataLength;
			anqp->apCivicLocationBuffer = dataPtr;
			break;
		case ANQP_ID_AP_LOCATION_PUBLIC_ID_URI:
			anqp->apLocationPublicIdUriLength = dataLength;
			anqp->apLocationPublicIdUriBuffer = dataPtr;
			break;
		case ANQP_ID_DOMAIN_NAME_LIST:
			anqp->domainNameListLength = dataLength;
			anqp->domainNameListBuffer = dataPtr;
			break;
		case ANQP_ID_EMERGENCY_ALERT_ID_URI:
			anqp->emergencyAlertIdUriLength = dataLength;
			anqp->emergencyAlertIdUriBuffer = dataPtr;
			break;
		case ANQP_ID_EMERGENCY_NAI:
			anqp->emergencyNaiLength = dataLength;
			anqp->emergencyNaiBuffer = dataPtr;
			break;
		case ANQP_ID_VENDOR_SPECIFIC_LIST:
		{
			pktDecodeT vs;

			/* include ID and length */
			pktDecodeInit(&vs, dataLength + 4, dataPtr - 4);
			/* hotspot decode */
			if (pktDecodeHspotAnqp(&vs, FALSE, &anqp->hspot) == 0) {
				/* not decoded */
				anqp->anqpVendorSpecificListLength = dataLength;
				anqp->anqpVendorSpecificListBuffer = dataPtr;
			}
		}
			break;
		default:
			WL_P2PO(("invalid ID %d\n", id));
			continue;
			break;
		}

		/* count IEs decoded */
		ieCount++;
	}

	if (ieCount > 0)
		printAnqpDecode(anqp);

	return ieCount;
}

static char *ieName(uint16 id)
{
	char *str;

	switch (id) {
	case ANQP_ID_QUERY_LIST:
		str = "ANQP_ID_QUERY_LIST";
		break;
	case ANQP_ID_CAPABILITY_LIST:
		str = "ANQP_ID_CAPABILITY_LIST";
		break;
	case ANQP_ID_VENUE_NAME_INFO:
		str = "ANQP_ID_VENUE_NAME_INFO";
		break;
	case ANQP_ID_EMERGENCY_CALL_NUMBER_INFO:
		str = "ANQP_ID_EMERGENCY_CALL_NUMBER_INFO";
		break;
	case ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO:
		str = "ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO";
		break;
	case ANQP_ID_ROAMING_CONSORTIUM_LIST:
		str = "ANQP_ID_ROAMING_CONSORTIUM_LIST";
		break;
	case ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO:
		str = "ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO";
		break;
	case ANQP_ID_NAI_REALM_LIST:
		str = "ANQP_ID_NAI_REALM_LIST";
		break;
	case ANQP_ID_G3PP_CELLULAR_NETWORK_INFO:
		str = "ANQP_ID_G3PP_CELLULAR_NETWORK_INFO";
		break;
	case ANQP_ID_AP_GEOSPATIAL_LOCATION:
		str = "ANQP_ID_AP_GEOSPATIAL_LOCATION";
		break;
	case ANQP_ID_AP_CIVIC_LOCATION:
		str = "ANQP_ID_AP_CIVIC_LOCATION";
		break;
	case ANQP_ID_AP_LOCATION_PUBLIC_ID_URI:
		str = "ANQP_ID_AP_LOCATION_PUBLIC_ID_URI";
		break;
	case ANQP_ID_DOMAIN_NAME_LIST:
		str = "ANQP_ID_DOMAIN_NAME_LIST";
		break;
	case ANQP_ID_EMERGENCY_ALERT_ID_URI:
		str = "ANQP_ID_EMERGENCY_ALERT_ID_URI";
		break;
	case ANQP_ID_EMERGENCY_NAI:
		str = "ANQP_ID_EMERGENCY_NAI";
		break;
	case ANQP_ID_VENDOR_SPECIFIC_LIST:
		str = "ANQP_ID_VENDOR_SPECIFIC_LIST";
		break;
	default:
		str = "unknown";
		break;
	}

	return str;
}

/* decode ANQP query list */
int pktDecodeAnqpQueryList(pktDecodeT *pkt, pktAnqpQueryListT *queryList)
{
	int count, i;

	WL_PRPKT("packet for ANQP query list decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(queryList, 0, sizeof(*queryList));

	count =  pktDecodeRemaining(pkt) / 2;
	for (i = 0; i < count; i++) {
		if (i >= MAX_LIST_SIZE) {
			WL_ERROR(("truncating query list to %d\n",
				MAX_LIST_SIZE));
			return FALSE;
		}
		pktDecodeLe16(pkt, &queryList->queryId[queryList->queryLen++]);
	}

	if (WL_P2PO_ON())
		pktDecodeAnqpQueryListPrint(queryList);
	return TRUE;
}

/* print decoded ANQP query list */
void pktDecodeAnqpQueryListPrint(pktAnqpQueryListT *queryList)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP query list:\n"));
	WL_PRINT(("query count = %d\n", queryList->queryLen));

	for (i = 0; i < queryList->queryLen; i++) {
		WL_PRINT(("   %s (%d)\n",
			ieName(queryList->queryId[i]), queryList->queryId[i]));
	}
}

/* decode ANQP vendor specific list */
int pktDecodeAnqpVendorSpecificList(pktDecodeT *pkt, pktAnqpVendorListT *vendorList)
{
	WL_PRPKT("packet for ANQP vendor list decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(vendorList, 0, sizeof(*vendorList));

	if (pktDecodeRemaining(pkt) > MAX_LIST_SIZE) {
		WL_ERROR(("list size %d > %d\n",
			pktDecodeRemaining(pkt), MAX_LIST_SIZE));
		return FALSE;
	}

	vendorList->vendorLen = pktDecodeRemaining(pkt);
	pktDecodeBytes(pkt, vendorList->vendorLen, vendorList->vendorData);

	return TRUE;
}

/* decode ANQP capability list */
int pktDecodeAnqpCapabilityList(pktDecodeT *pkt, pktAnqpCapabilityListT *capList)
{
	WL_PRPKT("packet for ANQP capability list decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(capList, 0, sizeof(*capList));

	while (pktDecodeRemaining(pkt) >= 2) {
		uint16 id;

		if (capList->capLen >= MAX_LIST_SIZE) {
			WL_ERROR(("truncating capability list to %d\n",
				MAX_LIST_SIZE));
			return FALSE;
		}
		pktDecodeLe16(pkt, &id);

		if (id != ANQP_ID_VENDOR_SPECIFIC_LIST) {
			capList->capId[capList->capLen++] = id;
		}
		else {
			uint16 len;
			pktDecodeT vs;
			pktDecodeHspotAnqpT hspot;
			pktDecodeT cap;

			if (!pktDecodeLe16(pkt, &len)) {
				WL_ERROR(("decode error\n"));
				return FALSE;
			}

			if (len > pktDecodeRemaining(pkt)) {
				WL_ERROR(("decode error\n"));
				return FALSE;
			}

			/* include ID and length */
			pktDecodeInit(&vs, len + 4,
				pktDecodeCurrentPtr(pkt) - 4);
			/* decode hotspot capability */
			if (pktDecodeHspotAnqp(&vs, TRUE, &hspot)) {
				if (hspot.capabilityListLength > MAX_LIST_SIZE) {
					WL_ERROR(("list size %d > %d\n",
						hspot.capabilityListLength,
						MAX_LIST_SIZE));
					return FALSE;
				}
				pktDecodeInit(&cap, hspot.capabilityListLength,
					hspot.capabilityListBuffer);
				if (!pktDecodeHspotAnqpCapabilityList(
					&cap, &capList->hspotCapList)) {
					return FALSE;
				}
			}

			/* advance packet pointer */
			pktDecodeOffset(pkt) += len;
		}
	}

	if (WL_P2PO_ON())
		pktDecodeAnqpCapabilityListPrint(capList);
	return TRUE;
}

/* print decoded ANQP capability list */
void pktDecodeAnqpCapabilityListPrint(pktAnqpCapabilityListT *capList)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP capability list:\n"));
	WL_PRINT(("capability count = %d\n", capList->capLen));

	for (i = 0; i < capList->capLen; i++) {
		WL_PRINT(("   %s (%d)\n",
			ieName(capList->capId[i]), capList->capId[i]));
	}
}

/* decode venue name duple */
static int pktDecodeAnqpVenueNameDuple(pktDecodeT *pkt, pktAnqpVenueNameDupleT *duple)
{
	uint8 len;


	if (!pktDecodeByte(pkt, &len)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, VENUE_LANGUAGE_CODE_SIZE, (uint8 *)duple->lang)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	duple->lang[VENUE_LANGUAGE_CODE_SIZE] = 0;
	duple->langLen = strlen(duple->lang);

	if (len - VENUE_LANGUAGE_CODE_SIZE > pktDecodeRemaining(pkt)) {
		WL_ERROR(("packet too short %d > %d\n",
		len - VENUE_LANGUAGE_CODE_SIZE, len - VENUE_LANGUAGE_CODE_SIZE));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, len - VENUE_LANGUAGE_CODE_SIZE, (uint8 *)duple->name)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	duple->name[len - VENUE_LANGUAGE_CODE_SIZE] = 0;
	duple->nameLen = strlen(duple->name);

	return TRUE;
}

int pktDecodeAnqpVenueName(pktDecodeT *pkt, pktAnqpVenueNameT *venueName)
{
	WL_PRPKT("packet for ANQP venue name decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(venueName, 0, sizeof(*venueName));

	if (!pktDecodeByte(pkt, &venueName->group)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &venueName->type)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	while (pktDecodeRemaining(pkt) > 0 &&
		venueName->numVenueName < MAX_VENUE_NAME) {
		if (!pktDecodeAnqpVenueNameDuple(pkt,
			&venueName->venueName[venueName->numVenueName])) {
			return FALSE;
		}
		else {
			venueName->numVenueName++;
		}
	}

	if (WL_P2PO_ON())
		pktDecodeAnqpVenueNamePrint(venueName);
	return TRUE;
}

/* print decoded venue name */
void pktDecodeAnqpVenueNamePrint(pktAnqpVenueNameT *venueName)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP venue name:\n"));
	WL_PRINT(("   group: %d\n", venueName->group));
	WL_PRINT(("   type: %d\n", venueName->type));
	for (i = 0; i < venueName->numVenueName; i++) {
		WL_PRINT(("   language: %s\n", venueName->venueName[i].lang));
		WL_PRPKT("   venue name",
			(uint8 *)venueName->venueName[i].name, venueName->venueName[i].nameLen);
	}
}

/* decode network authentication unit */
static int pktDecodeAnqpNetworkAuthenticationUnit(pktDecodeT *pkt,
	pktAnqpNetworkAuthenticationUnitT *unit)
{

	if (!pktDecodeByte(pkt, &unit->type)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeLe16(pkt, &unit->urlLen)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (unit->urlLen > MAX_URL_LENGTH) {
		WL_ERROR(("URL length %d > %d\n", unit->urlLen, MAX_URL_LENGTH));
		return FALSE;
	}

	if (unit->urlLen && !pktDecodeBytes(pkt, unit->urlLen, unit->url)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	unit->url[unit->urlLen] = 0;

	return TRUE;
}

/* decode network authentication type */
int pktDecodeAnqpNetworkAuthenticationType(pktDecodeT *pkt,
	pktAnqpNetworkAuthenticationTypeT *auth)
{
	WL_PRPKT("packet for ANQP network authentication type decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(auth, 0, sizeof(*auth));

	while (pktDecodeRemaining(pkt) > 0 &&
		auth->numAuthenticationType < MAX_AUTHENTICATION_UNIT) {
		if (!pktDecodeAnqpNetworkAuthenticationUnit(pkt,
			&auth->unit[auth->numAuthenticationType])) {
			return FALSE;
		}
		else {
			auth->numAuthenticationType++;
		}
	}

	if (WL_P2PO_ON())
		pktDecodeAnqpNetworkAuthenticationTypePrint(auth);
	return TRUE;
}

/* print decoded network authentication type */
void pktDecodeAnqpNetworkAuthenticationTypePrint(
	pktAnqpNetworkAuthenticationTypeT *auth)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP network authentication type:\n"));
	WL_PRINT(("   count: %d\n", auth->numAuthenticationType));
	for (i = 0; i < auth->numAuthenticationType; i++) {
		WL_PRINT(("   type: %d  url:%s\n",
			auth->unit[i].type, auth->unit[i].url));
	}
}

/* search decoded network authentication type for online enrollment support */
int pktDecodeAnqpIsOnlineEnrollmentSupport(pktAnqpNetworkAuthenticationTypeT *auth)
{
	int i;

	for (i = 0; i < auth->numAuthenticationType; i++) {
		if (auth->unit[i].type == NATI_ONLINE_ENROLLMENT_SUPPORTED) {
			return TRUE;
		}
	}

	return FALSE;
}

/* decode OI duple */
static int pktDecodeAnqpOiDuple(pktDecodeT *pkt, pktAnqpOiDupleT *oi)
{

	if (!pktDecodeByte(pkt, &oi->oiLen)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (oi->oiLen > MAX_OI_LENGTH) {
		WL_ERROR(("OI length %d > %d\n", oi->oiLen, MAX_OI_LENGTH));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, oi->oiLen, oi->oi)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* decode roaming consortium */
int pktDecodeAnqpRoamingConsortium(pktDecodeT *pkt,
	pktAnqpRoamingConsortiumT *roam)
{
	WL_PRPKT("packet for ANQP roaming consortium decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(roam, 0, sizeof(*roam));

	while (pktDecodeRemaining(pkt) > 0 &&
		roam->numOi < MAX_OI) {
		if (!pktDecodeAnqpOiDuple(pkt,
			&roam->oi[roam->numOi])) {
			return FALSE;
		}
		else {
			roam->numOi++;
		}
	}

	if (WL_P2PO_ON())
		pktDecodeAnqpRoamingConsortiumPrint(roam);
	return TRUE;
}

/* print decoded roaming consortium */
void pktDecodeAnqpRoamingConsortiumPrint(pktAnqpRoamingConsortiumT *roam)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP roaming consortium:\n"));
	WL_PRINT(("   count: %d\n", roam->numOi));
	for (i = 0; i < roam->numOi; i++) {
		WL_PRPKT("   OI",
			roam->oi[i].oi, roam->oi[i].oiLen);
	}
}

/* search decoded roaming consortium for a match */
int pktDecodeAnqpIsRoamingConsortium(pktAnqpRoamingConsortiumT *roam,
	pktAnqpOiDupleT *oi)
{
	int i;

	for (i = 0; i < roam->numOi; i++) {
		pktAnqpOiDupleT *r = &roam->oi[i];
		if (oi->oiLen == r->oiLen) {
			if (memcmp(oi->oi, r->oi, oi->oiLen) == 0)
				return TRUE;
		}
	}

	return FALSE;
}

/* decode IP address type availability */
int pktDecodeAnqpIpTypeAvailability(pktDecodeT *pkt, pktAnqpIpTypeT *ip)
{
	uint8 type;

	WL_PRPKT("packet for IP type availability decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(ip, 0, sizeof(*ip));

	if (!pktDecodeByte(pkt, &type)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	ip->ipv6 = (type & IPA_IPV6_MASK) >> IPA_IPV6_SHIFT;
	ip->ipv4 = (type & IPA_IPV4_MASK) >> IPA_IPV4_SHIFT;

	if (WL_P2PO_ON())
		pktDecodeAnqpIpTypeAvailabilityPrint(ip);
	return TRUE;
}

/* print decoded IP address type availability */
void pktDecodeAnqpIpTypeAvailabilityPrint(pktAnqpIpTypeT *ip)
{
	char *str;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP IP type availability:\n"));

	switch (ip->ipv6)
	{
		case IPA_IPV6_NOT_AVAILABLE:
			str = "address type not available";
			break;
		case IPA_IPV6_AVAILABLE:
			str = "address type available";
			break;
		case IPA_IPV6_UNKNOWN_AVAILABILITY:
			str = "availability of the address type not known";
			break;
		default:
			str = "unknown";
			break;
	}
	WL_PRINT(("IPv6: %d   %s\n", ip->ipv6, str));

	switch (ip->ipv4)
	{
		case IPA_IPV4_NOT_AVAILABLE:
			str = "address type not available";
			break;
		case IPA_IPV4_PUBLIC:
			str = "public IPv4 adress available";
			break;
		case IPA_IPV4_PORT_RESTRICT:
			str = "port-restricted IPv4 address available";
			break;
		case IPA_IPV4_SINGLE_NAT:
			str = "single NATed private IPv4 address available";
			break;
		case IPA_IPV4_DOUBLE_NAT:
			str = "double NATed private IPv4 address available";
			break;
		case IPA_IPV4_PORT_RESTRICT_SINGLE_NAT:
			str = "port-restricted IPv4 address and single "
			"NATed IPv4 address available";
			break;
		case IPA_IPV4_PORT_RESTRICT_DOUBLE_NAT:
			str = "port-restricted IPv4 address and double "
			"NATed IPv4 address available";
			break;
		case IPA_IPV4_UNKNOWN_AVAILABILITY:
			str = "availability of the address type is not known";
			break;
		default:
			str = "unknown";
			break;
	}
	WL_PRINT(("IPv4: %d   %s\n", ip->ipv4, str));
}

/* decode authentication parameter */
static int pktDecodeAnqpAuthentication(pktDecodeT *pkt, pktAnqpAuthT *auth)
{

	if (!pktDecodeByte(pkt, &auth->id)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &auth->len)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (auth->len > MAX_AUTH_PARAM) {
		WL_ERROR(("authentication parameter %d > %d\n",
			auth->len, MAX_AUTH_PARAM));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, auth->len, auth->value)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* decode EAP method */
static int pktDecodeAnqpEapMethod(pktDecodeT *pkt, pktAnqpEapMethodT *eap)
{
	uint8 len;
	int i;


	if (!pktDecodeByte(pkt, &len)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (len > pktDecodeRemaining(pkt)) {
		WL_ERROR(("packet too short %d > %d\n",
			len, pktDecodeRemaining(pkt)));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &eap->eapMethod)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &eap->authCount)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (eap->authCount > MAX_AUTH) {
		WL_ERROR(("auth count %d > %d\n",
			eap->authCount, MAX_AUTH));
		return FALSE;
	}

	for (i = 0; i < eap->authCount; i++) {
		if (!pktDecodeAnqpAuthentication(pkt, &eap->auth[i]))
			return FALSE;
	}

	return TRUE;
}

/* decode NAI realm data */
static int pktDecodeAnqpNaiRealmData(pktDecodeT *pkt, pktAnqpNaiRealmDataT *data)
{
	uint16 len;
	int i;


	if (!pktDecodeLe16(pkt, &len)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (len > pktDecodeRemaining(pkt)) {
		WL_ERROR(("packet too short %d > %d\n",
			len, pktDecodeRemaining(pkt)));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &data->encoding)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &data->realmLen)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (data->realmLen > MAX_REALM_LENGTH) {
		WL_ERROR(("realm length %d > %d\n",
			data->realmLen, MAX_REALM_LENGTH));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, data->realmLen, data->realm)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	data->realm[data->realmLen] = 0;

	if (!pktDecodeByte(pkt, &data->eapCount)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (data->eapCount > MAX_EAP_METHOD) {
		WL_ERROR(("EAP count %d > %d\n",
			data->eapCount, MAX_EAP_METHOD));
		return FALSE;
	}

	for (i = 0; i < data->eapCount; i++) {
		if (!pktDecodeAnqpEapMethod(pkt, &data->eap[i]))
			return FALSE;
	}

	return TRUE;
}

/* decode NAI realm */
int pktDecodeAnqpNaiRealm(pktDecodeT *pkt, pktAnqpNaiRealmListT *realm)
{
	int i;

	WL_PRPKT("packet for NAI realm decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(realm, 0, sizeof(*realm));

	/* allow zero length */
	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (!pktDecodeLe16(pkt, &realm->realmCount)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
}

	if (realm->realmCount > MAX_REALM) {
		WL_ERROR(("realm count %d > %d\n",
			realm->realmCount, MAX_REALM));
		return FALSE;
	}

	for (i = 0; i < realm->realmCount; i++) {
		if (!pktDecodeAnqpNaiRealmData(pkt, &realm->realm[i]))
			return FALSE;
	}

	if (WL_P2PO_ON())
		pktDecodeAnqpNaiRealmPrint(realm);
	return TRUE;
}

/* print decoded NAI realm */
void pktDecodeAnqpNaiRealmPrint(pktAnqpNaiRealmListT *realm)
{
	int i, j, k;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP NAI realm list:\n"));
	WL_PRINT(("realm count = %d\n", realm->realmCount));

	for (i = 0; i < realm->realmCount; i++) {
		pktAnqpNaiRealmDataT *data = &realm->realm[i];
		WL_PRINT(("\n"));
		WL_PRINT(("%s:\n", data->realm));
		WL_PRINT(("encoding = %d\n", data->encoding));
		WL_PRINT(("EAP count = %d\n", data->eapCount));

		for (j = 0; j < data->eapCount; j++) {
			pktAnqpEapMethodT *eap = &data->eap[j];
			char *eapStr;

			switch (eap->eapMethod) {
			case REALM_EAP_TLS:
				eapStr = "EAP-TLS";
				break;
			case REALM_EAP_SIM:
				eapStr = "EAP-SIM";
				break;
			case REALM_EAP_TTLS:
				eapStr = "EAP-TTLS";
				break;
			case REALM_EAP_AKA:
				eapStr = "EAP-AKA";
				break;
			case REALM_EAP_AKAP:
				eapStr = "EAP-AKA'";
				break;
			default:
				eapStr = "unknown";
				break;
			}
			WL_PRINT(("   EAP method = %s (%d)\n", eapStr, eap->eapMethod));
			WL_PRINT(("   authentication count = %d\n", eap->authCount));

			for (k = 0; k < eap->authCount; k++) {
				pktAnqpAuthT *auth = &eap->auth[k];
				char *authStr;
				char *paramStr;

				switch (auth->id) {
				case REALM_EXPANDED_EAP:
					authStr = "expanded EAP method";
					break;
				case REALM_NON_EAP_INNER_AUTHENTICATION:
					authStr = "non-EAP inner authentication";
					break;
				case REALM_INNER_AUTHENTICATION_EAP:
					authStr = "inner authentication EAP method";
					break;
				case REALM_EXPANDED_INNER_EAP:
					authStr = "expanded inner EAP method";
					break;
				case REALM_CREDENTIAL:
					authStr = "credential";
					break;
				case REALM_TUNNELED_EAP_CREDENTIAL:
					authStr = "tunneled EAP method credential";
					break;
				case REALM_VENDOR_SPECIFIC_EAP:
					authStr = "vendor specific";
					break;
				default:
					authStr = "unknown";
					break;
				}
				WL_PRINT(("      authentication = %s (%d)\n",
					authStr, auth->id));

				if (auth->id == REALM_NON_EAP_INNER_AUTHENTICATION) {
					switch (auth->value[0]) {
					case REALM_PAP:
						paramStr = "PAP";
						break;
					case REALM_CHAP:
						paramStr = "CHAP";
						break;
					case REALM_MSCHAP:
						paramStr = "MSCHAP";
						break;
					case REALM_MSCHAPV2:
						paramStr = "MSCHAPV2";
						break;
					default:
						paramStr = "unknown";
						break;
					}
					WL_PRINT(("      %s (%d)\n",
						paramStr, auth->value[0]));
				}
				else if (auth->id == REALM_CREDENTIAL ||
					auth->id == REALM_TUNNELED_EAP_CREDENTIAL) {
					switch (auth->value[0]) {
					case REALM_SIM:
						paramStr = "SIM";
						break;
					case REALM_USIM:
						paramStr = "USIM";
						break;
					case REALM_NFC:
						paramStr = "NFC";
						break;
					case REALM_HARDWARE_TOKEN:
						paramStr = "hardware token";
						break;
					case REALM_SOFTOKEN:
						paramStr = "softoken";
						break;
					case REALM_CERTIFICATE:
						paramStr = "certificate";
						break;
					case REALM_USERNAME_PASSWORD:
						paramStr = "username/password";
						break;
					case REALM_SERVER_SIDE:
						paramStr = "server side";
						break;
					default:
						paramStr = "unknown";
						break;
					}
					WL_PRINT(("      %s (%d)\n",
						paramStr, auth->value[0]));
				}
				else {
					if (auth->len > 0) {
						int i;

						WL_PRINT(("      "));
						for (i = 0; i < auth->len; i++)
							WL_PRINT(("%02x ",
								auth->value[i]));
					}
				}
			}
		}
	}
}

/* search the decoded NAI realm for a match */
int pktDecodeAnqpIsRealm(pktAnqpNaiRealmListT *realmList,
	char *realmName, uint8 eapMethod, uint8 credential)
{
	int i, j, k;
	pktAnqpNaiRealmDataT *data;
	pktAnqpEapMethodT *eap;

	/* search for realm name */
	for (i = 0; i < realmList->realmCount; i++) {
		uint8 realm[MAX_REALM_LENGTH + 1];
		int argc = 0;
		char *argv[16], *token;
		int isRealmFound = FALSE;

		data = &realmList->realm[i];

		/* the realm may have multiple entries delimited by ';' */
		/* copy and convert realm to argc/argv format */
		strncpy((char *)realm, (char *)data->realm, sizeof(realm));
#ifndef BCMDRIVER
		while ((argc < (int)(sizeof(argv) / sizeof(char *) - 1)) &&
		     ((token = strtok(argc ? NULL : (char *)realm, ";")) != NULL)) {
#else
		while ((argc < (int)(sizeof(argv) / sizeof(char *) - 1)) &&
		     ((token = bcmstrtok((char **)&realm, ";", 0)) != NULL)) {
#endif
			argv[argc++] = token;
		}
		argv[argc] = NULL;

		/* compare each realm entry */
		for (j = 0; j < argc; j++) {
			if (realmName != 0 &&
				strcmp(argv[j], realmName) == 0) {
				isRealmFound = TRUE;
				break;
			}
		}

		if (!isRealmFound)
			continue;

		/* search for EAP method */
		for (j = 0; j < data->eapCount; j++) {
			eap = &data->eap[j];
			if (eap->eapMethod == eapMethod) {
				/* seach for credential */
				for (k = 0; k < eap->authCount; k++) {
					pktAnqpAuthT *auth = &eap->auth[k];
					if (auth->id == REALM_CREDENTIAL &&
						auth->value[0] == credential) {
						return TRUE;
					}
				}
			}
		}
	}

	return FALSE;
}

/* decode PLMN */
static int pktDecodeAnqpPlmn(pktDecodeT *pkt, pktAnqpPlmnT *plmn)
{
	uint8 octet1, octet2, octet3;


	if (!pktDecodeByte(pkt, &octet1)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &octet2)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &octet3)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	plmn->mcc[0] = (octet1 & 0x0f) + '0';
	plmn->mcc[1] = (octet1 >> 4) + '0';
	plmn->mcc[2] = (octet2 & 0x0f) + '0';
	plmn->mcc[3] = 0;

	plmn->mnc[0] = (octet3 & 0x0f) + '0';
	plmn->mnc[1] = (octet3 >> 4) + '0';
	if ((octet2 & 0xf0) == 0xf0)
		plmn->mnc[2] = 0;
	else
		plmn->mnc[2] = (octet2 >> 4) + '0';
	plmn->mnc[3] = 0;

	return TRUE;
}

/* decode 3GPP cellular network */
int pktDecodeAnqp3GppCellularNetwork(pktDecodeT *pkt, pktAnqp3GppCellularNetworkT *g3pp)
{
	uint8 byte;
	uint8 count;
	int i;

	WL_PRPKT("packet for 3GPP cellular network",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(g3pp, 0, sizeof(*g3pp));

	/* allow zero length */
	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (!pktDecodeByte(pkt, &byte)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (byte != G3PP_GUD_VERSION) {
		WL_ERROR(("3GPP PLMN GUD version %d != %d\n",
			byte, G3PP_GUD_VERSION));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &byte)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (byte > pktDecodeRemaining(pkt)) {
		WL_ERROR(("UDHL too short %d > %d\n",
			byte, pktDecodeRemaining(pkt)));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &byte)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (byte != G3PP_PLMN_LIST_IE) {
		WL_ERROR(("3GPP PLMN IE %d != %d\n",
			byte, G3PP_PLMN_LIST_IE));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &byte)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (byte > pktDecodeRemaining(pkt)) {
		WL_ERROR(("PLMN length too short %d > %d\n",
			byte, pktDecodeRemaining(pkt)));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &count)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (count > MAX_PLMN) {
		WL_ERROR(("PLMN count %d > %d\n",
			count, MAX_PLMN));
		return FALSE;
	}

	if (count * 3 > pktDecodeRemaining(pkt)) {
		WL_ERROR(("packet too short %d > %d\n",
			count * 3, pktDecodeRemaining(pkt)));
		return FALSE;
	}

	for (i = 0; i < count; i++)
		pktDecodeAnqpPlmn(pkt, &g3pp->plmn[i]);

	g3pp->plmnCount = count;

	if (WL_P2PO_ON())
		pktDecodeAnqp3GppCellularNetworkPrint(g3pp);
	return TRUE;
}

/* print decoded 3GPP cellular network */
void pktDecodeAnqp3GppCellularNetworkPrint(pktAnqp3GppCellularNetworkT *g3pp)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP 3GPP cellular network:\n"));
	WL_PRINT(("   PLMN count = %d\n", g3pp->plmnCount));
	for (i = 0; i < g3pp->plmnCount; i++)
		WL_PRINT(("   MCC=%s MNC=%s\n", g3pp->plmn[i].mcc, g3pp->plmn[i].mnc));
}

/* search the decoded 3GPP cellular network for a match */
int pktDecodeAnqpIs3Gpp(pktAnqp3GppCellularNetworkT *g3pp, pktAnqpPlmnT *plmn)
{
	int i;

	for (i = 0; i < g3pp->plmnCount; i++) {
		if (strcmp(plmn->mcc, g3pp->plmn[i].mcc) == 0 &&
			strcmp(plmn->mnc, g3pp->plmn[i].mnc) == 0)
			return TRUE;
	}

	return FALSE;
}

/* decode domain name */
static int pktDecodeAnqpDomainName(pktDecodeT *pkt, pktAnqpDomainNameT *domain)
{

	if (!pktDecodeByte(pkt, &domain->len)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (domain->len > pktDecodeRemaining(pkt)) {
		WL_ERROR(("packet too short %d > %d\n",
		domain->len, pktDecodeRemaining(pkt)));
		return FALSE;
	}

	if (domain->len > MAX_DOMAIN_NAME_SIZE) {
		WL_ERROR(("domain name size %d > %d\n",
		domain->len, MAX_DOMAIN_NAME_SIZE));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, domain->len, (uint8 *)domain->name)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	domain->name[domain->len] = 0;

	return TRUE;
}

/* decode domain name list */
int pktDecodeAnqpDomainNameList(pktDecodeT *pkt, pktAnqpDomainNameListT *list)
{
	WL_PRPKT("packet for ANQP domain name list",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(list, 0, sizeof(*list));

	while (pktDecodeRemaining(pkt) > 0 &&
		list->numDomain < MAX_DOMAIN) {
		if (!pktDecodeAnqpDomainName(pkt,
			&list->domain[list->numDomain])) {
			return FALSE;
		}
		else {
			list->numDomain++;
		}
	}

	if (WL_P2PO_ON())
		pktDecodeAnqpDomainNameListPrint(list);
	return TRUE;
}

/* print decoded domain name list */
void pktDecodeAnqpDomainNameListPrint(pktAnqpDomainNameListT *list)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded ANQP domain name:\n"));
	WL_PRINT(("   count = %d\n", list->numDomain));
	for (i = 0; i < list->numDomain; i++)
		WL_PRINT(("   %s\n", list->domain[i].name));
}

/* search the decoded domain name list for a match */
int pktDecodeAnqpIsDomainName(pktAnqpDomainNameListT *list, char *domain)
{
	int i;

	if (domain != 0) {
		for (i = 0; i < list->numDomain; i++) {
			if (strcmp(domain, list->domain[i].name) == 0)
				return TRUE;
		}
	}

	return FALSE;
}

/* decode query vendor specific */
int pktDecodeAnqpQueryVendorSpecific(pktDecodeT *pkt, uint16 *serviceUpdateIndicator)
{
	uint8 oui[WFA_OUI_LEN];
	uint8 ouiSubtype;

	WL_PRPKT("packet for ANQP query vendor specific",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	/* check OUI */
	if (!pktDecodeBytes(pkt, WFA_OUI_LEN, oui) ||
		memcmp(oui, WFA_OUI, WFA_OUI_LEN) != 0) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &ouiSubtype) || ouiSubtype != ANQP_OUI_SUBTYPE) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeLe16(pkt, serviceUpdateIndicator)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	return TRUE;
}

static int pktDecodeAnqpQueryVendorSpecificTlv(pktDecodeT *pkt,
	uint8 *serviceProtocolType,	uint8 *serviceTransactionId,
	uint8 *statusCode, uint16 *queryLen, uint8 **queryData)
{
	uint16 length;

	if (!pktDecodeLe16(pkt, &length)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (length > pktDecodeRemaining(pkt)) {
		WL_ERROR(("length exceeds packet %d > %d\n",
			length, pktDecodeRemaining(pkt)));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, serviceProtocolType)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, serviceTransactionId)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (statusCode != 0) {
		if (!pktDecodeByte(pkt, statusCode)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
	}
	*queryLen = length - (2 + (statusCode != 0 ? 1 : 0));
	*queryData = pktDecodeCurrentPtr(pkt);
	pktDecodeOffset(pkt) += *queryLen;
	return TRUE;
}

/* decode query request vendor specific TLV */
int pktDecodeAnqpQueryRequestVendorSpecificTlv(pktDecodeT *pkt,
	pktAnqpQueryRequestVendorSpecificTlvT *request)
{
	WL_PRPKT("packet for ANQP query request vendor specific TLV",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(request, 0, sizeof(*request));
	return pktDecodeAnqpQueryVendorSpecificTlv(pkt,
		&request->serviceProtocolType,
		&request->serviceTransactionId, 0,
		&request->queryLen, &request->queryData);
}

/* decode query response vendor specific TLV */
int pktDecodeAnqpQueryResponseVendorSpecificTlv(pktDecodeT *pkt,
	pktAnqpQueryResponseVendorSpecificTlvT *response)
{
	WL_PRPKT("packet for ANQP query response vendor specific TLV",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(response, 0, sizeof(*response));
	return pktDecodeAnqpQueryVendorSpecificTlv(pkt,
		&response->serviceProtocolType,
		&response->serviceTransactionId, &response->statusCode,
		&response->queryLen, &response->queryData);
}
