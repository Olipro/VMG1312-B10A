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

#ifndef _PKTENCODEANQP_H_
#define _PKTENCODEANQP_H_

#include "typedefs.h"
#include "pktEncode.h"

/* encode ANQP query list */
int pktEncodeAnqpQueryList(pktEncodeT *pkt, uint16 queryLen, uint16 *queryId);

/* encode ANQP capability list */
int pktEncodeAnqpCapabilityList(pktEncodeT *pkt, uint16 capLen, uint16 *capList,
	uint16 vendorLen, uint8 *vendorList);

/* encode venue name duple */
int pktEncodeAnqpVenueDuple(pktEncodeT *pkt, uint8 langLen, char *lang,
	uint8 nameLen, char *name);

/* encode venue name */
int pktEncodeAnqpVenueName(pktEncodeT *pkt, uint8 group, uint8 type,
	uint16 nameLen, uint8 *name);

/* encode network authentication unit */
int pktEncodeAnqpNetworkAuthenticationUnit(pktEncodeT *pkt, uint8 type,
	uint16 urlLen, char *url);

/* encode network authentication type */
int pktEncodeAnqpNetworkAuthenticationType(pktEncodeT *pkt, uint16 len, uint8 *data);

/* encode OI duple */
int pktEncodeAnqpOiDuple(pktEncodeT *pkt, uint8 oiLen, uint8 *oi);

/* encode roaming consortium */
int pktEncodeAnqpRoamingConsortium(pktEncodeT *pkt, uint16 len, uint8 *data);

/* encode IP address type availability */
int pktEncodeAnqpIpTypeAvailability(pktEncodeT *pkt, uint8 ipv6, uint8 ipv4);

/* encode authentication parameter subfield */
int pktEncodeAnqpAuthenticationSubfield(pktEncodeT *pkt,
	uint8 id, uint8 authLen, uint8 *auth);

/* encode EAP method subfield */
int pktEncodeAnqpEapMethodSubfield(pktEncodeT *pkt, uint8 method,
	uint8 authCount, uint8 authLen, uint8 *auth);

/* encode EAP method */
int pktEncodeAnqpEapMethod(pktEncodeT *pkt, uint8 method,
	uint8 authCount, uint16 authLen, uint8 *auth);

/* encode NAI realm data */
int pktEncodeAnqpNaiRealmData(pktEncodeT *pkt, uint8 encoding,
	uint8 realmLen, uint8 *realm,
	uint8 eapCount, uint16 eapLen, uint8 *eap);

/* encode NAI realm */
int pktEncodeAnqpNaiRealm(pktEncodeT *pkt,
	uint16 realmCount, uint16 len, uint8 *data);

/* encode PLMN */
int pktEncodeAnqpPlmn(pktEncodeT *pkt, char *mcc, char *mnc);

/* encode 3GPP cellular network */
int pktEncodeAnqp3GppCellularNetwork(pktEncodeT *pkt,
	uint8 plmnCount, uint16 plmnLen, uint8 *plmnData);

/* encode domain name */
int pktEncodeAnqpDomainName(pktEncodeT *pkt, uint8 nameLen, char *name);

/* encode domain name list */
int pktEncodeAnqpDomainNameList(pktEncodeT *pkt, uint16 domainLen, uint8 *domain);

/* encode ANQP query vendor specific */
int pktEncodeAnqpQueryVendorSpecific(pktEncodeT *pkt,
	uint16 serviceUpdateIndicator, uint16 dataLen, uint8 *data);

/* encode ANQP query request vendor specific TLV */
int pktEncodeAnqpQueryRequestVendorSpecificTlv(pktEncodeT *pkt,
	uint8 serviceProtocolType, uint8 serviceTransactionId,
	uint16 queryLen, uint8 *queryData);

/* encode ANQP query response vendor specific TLV */
int pktEncodeAnqpQueryResponseVendorSpecificTlv(pktEncodeT *pkt,
	uint8 serviceProtocolType, uint8 serviceTransactionId, uint8 statusCode,
	uint16 queryLen, uint8 *queryData);

#endif /* _PKTENCODEANQP_H_ */
