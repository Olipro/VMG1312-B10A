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

#ifndef _PKTDECODEANQP_H_
#define _PKTDECODEANQP_H_

#include "typedefs.h"
#include "pktDecode.h"
#include "pktDecodeHspotAnqp.h"

typedef struct {
	int anqpQueryListLength;
	uint8 *anqpQueryListBuffer;
	int anqpCapabilityListLength;
	uint8 *anqpCapabilityListBuffer;
	int venueNameInfoLength;
	uint8 *venueNameInfoBuffer;
	int emergencyCallNumberInfoLength;
	uint8 *emergencyCallNumberInfoBuffer;
	int networkAuthenticationTypeInfoLength;
	uint8 *networkAuthenticationTypeInfoBuffer;
	int roamingConsortiumListLength;
	uint8 *roamingConsortiumListBuffer;
	int ipAddressTypeAvailabilityInfoLength;
	uint8 *ipAddressTypeAvailabilityInfoBuffer;
	int naiRealmListLength;
	uint8 *naiRealmListBuffer;
	int g3ppCellularNetworkInfoLength;
	uint8 *g3ppCellularNetworkInfoBuffer;
	int apGeospatialLocationLength;
	uint8 *apGeospatialLocationBuffer;
	int apCivicLocationLength;
	uint8 *apCivicLocationBuffer;
	int apLocationPublicIdUriLength;
	uint8 *apLocationPublicIdUriBuffer;
	int domainNameListLength;
	uint8 *domainNameListBuffer;
	int emergencyAlertIdUriLength;
	uint8 *emergencyAlertIdUriBuffer;
	int emergencyNaiLength;
	uint8 *emergencyNaiBuffer;
	int anqpVendorSpecificListLength;
	uint8 *anqpVendorSpecificListBuffer;

	/* hotspot specific */
	pktDecodeHspotAnqpT hspot;
} pktDecodeAnqpT;

/* decode ANQP */
int pktDecodeAnqp(pktDecodeT *pkt, pktDecodeAnqpT *anqp);

#define MAX_LIST_SIZE	16
typedef struct
{
	uint16 queryLen;
	uint16 queryId[MAX_LIST_SIZE];
} pktAnqpQueryListT;

/* decode ANQP query list */
int pktDecodeAnqpQueryList(pktDecodeT *pkt, pktAnqpQueryListT *queryList);

/* print decoded ANQP query list */
void pktDecodeAnqpQueryListPrint(pktAnqpQueryListT *queryList);

typedef struct
{
	uint16 vendorLen;
	uint8 vendorData[MAX_LIST_SIZE];
} pktAnqpVendorListT;

/* decode ANQP vendor specific list */
int pktDecodeAnqpVendorSpecificList(pktDecodeT *pkt, pktAnqpVendorListT *vendorList);

typedef struct
{
	uint16 capLen;
	uint16 capId[MAX_LIST_SIZE];
	pktHspotAnqpCapabilityListT hspotCapList;
} pktAnqpCapabilityListT;

/* decode ANQP capability list */
int pktDecodeAnqpCapabilityList(pktDecodeT *pkt, pktAnqpCapabilityListT *capList);

/* print decoded ANQP capability list */
void pktDecodeAnqpCapabilityListPrint(pktAnqpCapabilityListT *capList);

typedef struct
{
	uint8 langLen;
	char lang[VENUE_LANGUAGE_CODE_SIZE + 1];	/* null terminated */
	uint8 nameLen;
	char name[VENUE_NAME_SIZE + 1];		/* null terminated */
} pktAnqpVenueNameDupleT;

#define MAX_VENUE_NAME	4
typedef struct
{
	uint8 group;
	uint8 type;
	int numVenueName;
	pktAnqpVenueNameDupleT venueName[MAX_VENUE_NAME];
} pktAnqpVenueNameT;

/* decode venue name */
int pktDecodeAnqpVenueName(pktDecodeT *pkt, pktAnqpVenueNameT *venueName);

/* print decoded venue name */
void pktDecodeAnqpVenueNamePrint(pktAnqpVenueNameT *venueName);

#define MAX_URL_LENGTH	128
typedef struct
{
	uint8 type;
	uint16 urlLen;
	uint8 url[MAX_URL_LENGTH + 1];		/* null terminated */
} pktAnqpNetworkAuthenticationUnitT;

#define MAX_AUTHENTICATION_UNIT	8
typedef struct
{
	int numAuthenticationType;
	pktAnqpNetworkAuthenticationUnitT unit[MAX_AUTHENTICATION_UNIT];
} pktAnqpNetworkAuthenticationTypeT;

/* decode network authentication type */
int pktDecodeAnqpNetworkAuthenticationType(pktDecodeT *pkt,
	pktAnqpNetworkAuthenticationTypeT *auth);

/* print decoded network authentication type */
void pktDecodeAnqpNetworkAuthenticationTypePrint(
	pktAnqpNetworkAuthenticationTypeT *auth);

/* search decoded network authentication type for online enrollment support */
int pktDecodeAnqpIsOnlineEnrollmentSupport(pktAnqpNetworkAuthenticationTypeT *auth);

#define MAX_OI_LENGTH	8
typedef struct
{
	uint8 oiLen;
	uint8 oi[MAX_OI_LENGTH];
} pktAnqpOiDupleT;

#define MAX_OI	16
typedef struct
{
	int numOi;
	pktAnqpOiDupleT oi[MAX_OI];
} pktAnqpRoamingConsortiumT;

/* decode roaming consortium */
int pktDecodeAnqpRoamingConsortium(pktDecodeT *pkt,
	pktAnqpRoamingConsortiumT *roam);

/* print decoded roaming consortium */
void pktDecodeAnqpRoamingConsortiumPrint(pktAnqpRoamingConsortiumT *roam);

/* search decoded roaming consortium for a match */
int pktDecodeAnqpIsRoamingConsortium(pktAnqpRoamingConsortiumT *roam,
	pktAnqpOiDupleT *oi);

typedef struct
{
	uint8 ipv6;
	uint8 ipv4;
} pktAnqpIpTypeT;

/* decode IP address type availability */
int pktDecodeAnqpIpTypeAvailability(pktDecodeT *pkt, pktAnqpIpTypeT *ip);

/* print decoded IP address type availability */
void pktDecodeAnqpIpTypeAvailabilityPrint(pktAnqpIpTypeT *ip);

#define MAX_AUTH_PARAM		16
typedef struct
{
	uint8 id;
	uint8 len;
	uint8 value[MAX_AUTH_PARAM];
} pktAnqpAuthT;

#define MAX_AUTH			4
typedef struct
{
	uint8 eapMethod;
	uint8 authCount;
	pktAnqpAuthT auth[MAX_AUTH];
} pktAnqpEapMethodT;

#define MAX_REALM_LENGTH	64
#define MAX_EAP_METHOD		4
typedef struct
{
	uint8 encoding;
	uint8 realmLen;
	uint8 realm[MAX_REALM_LENGTH + 1];	/* null terminated */
	uint8 eapCount;
	pktAnqpEapMethodT eap[MAX_EAP_METHOD];
} pktAnqpNaiRealmDataT;

#define MAX_REALM			16
typedef struct
{
	uint16 realmCount;
	pktAnqpNaiRealmDataT realm[MAX_REALM];
} pktAnqpNaiRealmListT;

/* decode NAI realm */
int pktDecodeAnqpNaiRealm(pktDecodeT *pkt, pktAnqpNaiRealmListT *realm);

/* print decoded NAI realm */
void pktDecodeAnqpNaiRealmPrint(pktAnqpNaiRealmListT *realm);

/* search the decoded NAI realm for a match */
int pktDecodeAnqpIsRealm(pktAnqpNaiRealmListT *realmList,
	char *realmName, uint8 eapMethod, uint8 credential);

#define MCC_LENGTH	3
#define MNC_LENGTH	3
typedef struct
{
	char mcc[MCC_LENGTH + 1];
	char mnc[MNC_LENGTH + 1];
} pktAnqpPlmnT;

#define MAX_PLMN	16
typedef struct
{
	uint8 plmnCount;
	pktAnqpPlmnT plmn[MAX_PLMN];
} pktAnqp3GppCellularNetworkT;

/* decode 3GPP cellular network */
int pktDecodeAnqp3GppCellularNetwork(pktDecodeT *pkt, pktAnqp3GppCellularNetworkT *g3pp);

/* print decoded 3GPP cellular network */
void pktDecodeAnqp3GppCellularNetworkPrint(pktAnqp3GppCellularNetworkT *g3pp);

/* search the decoded 3GPP cellular network for a match */
int pktDecodeAnqpIs3Gpp(pktAnqp3GppCellularNetworkT *g3pp, pktAnqpPlmnT *plmn);

#define MAX_DOMAIN_NAME_SIZE 128
typedef struct
{
	uint8 len;
	char name[MAX_DOMAIN_NAME_SIZE + 1];	/* null terminated */
} pktAnqpDomainNameT;

#define MAX_DOMAIN 	16
typedef struct
{
	int numDomain;
	pktAnqpDomainNameT domain[MAX_DOMAIN];
} pktAnqpDomainNameListT;

/* decode domain name list */
int pktDecodeAnqpDomainNameList(pktDecodeT *pkt, pktAnqpDomainNameListT *list);

/* print decoded domain name list */
void pktDecodeAnqpDomainNameListPrint(pktAnqpDomainNameListT *list);

/* search the decoded domain name list for a match */
int pktDecodeAnqpIsDomainName(pktAnqpDomainNameListT *list, char *domain);

/* decode query vendor specific */
int pktDecodeAnqpQueryVendorSpecific(pktDecodeT *pkt, uint16 *serviceUpdateIndicator);

#define ANQP_QUERY_REQUEST_TLV_MIN_LENGTH	4

typedef struct
{
	uint8 serviceProtocolType;
	uint8 serviceTransactionId;
	uint16 queryLen;
	uint8 *queryData;
} pktAnqpQueryRequestVendorSpecificTlvT;

/* decode query request vendor specific TLV */
int pktDecodeAnqpQueryRequestVendorSpecificTlv(pktDecodeT *pkt,
	pktAnqpQueryRequestVendorSpecificTlvT *request);

#define ANQP_QUERY_RESPONSE_TLV_MIN_LENGTH	5

typedef struct
{
	uint8 serviceProtocolType;
	uint8 serviceTransactionId;
	uint8 statusCode;
	uint16 queryLen;
	uint8 *queryData;
} pktAnqpQueryResponseVendorSpecificTlvT;

/* decode query response vendor specific TLV */
int pktDecodeAnqpQueryResponseVendorSpecificTlv(pktDecodeT *pkt,
	pktAnqpQueryResponseVendorSpecificTlvT *response);

#endif /* _PKTDECODEANQP_H_ */
