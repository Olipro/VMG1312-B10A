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

#ifndef _PKTENCODEHSPOTANQP_H_
#define _PKTENCODEHSPOTANQP_H_

#include "typedefs.h"
#include "pktEncode.h"
#include "pktHspot.h"

/* encode query list */
int pktEncodeHspotAnqpQueryList(pktEncodeT *pkt, uint16 queryLen, uint8 *query);

/* encode capability list */
int pktEncodeHspotAnqpCapabilityList(pktEncodeT *pkt, uint16 capLen, uint8 *cap);

/* encode operator friendly name */
int pktEncodeHspotAnqpOperatorNameDuple(pktEncodeT *pkt, uint8 langLen, char *lang,
	uint8 nameLen, char *name);
int pktEncodeHspotAnqpOperatorFriendlyName(pktEncodeT *pkt, uint16 nameLen, uint8 *name);

/* encode WAN metrics */
int pktEncodeHspotAnqpWanMetrics(pktEncodeT *pkt, uint8 linkStatus, uint8 symmetricLink,
	uint8 atCapacity, uint32 dlinkSpeed, uint32 ulinkSpeed,
	uint8 dlinkLoad, uint8 ulinkLoad, uint16 lmd);

/* encode connection capability */
int pktEncodeHspotAnqpProtoPortTuple(pktEncodeT *pkt,
	uint8 ipProtocol, uint16 portNumber, uint8 status);
int pktEncodeHspotAnqpConnectionCapability(pktEncodeT *pkt, uint16 capLen, uint8 *cap);

/* encode NAI home realm query */
int pktEncodeHspotAnqpNaiHomeRealmName(pktEncodeT *pkt, uint8 encoding,
	uint8 nameLen, char *name);
int pktEncodeHspotAnqpNaiHomeRealmQuery(pktEncodeT *pkt, uint8 count,
	uint16 nameLen, uint8 *name);

/* encode operating class indication */
int pktEncodeHspotAnqpOperatingClassIndication(pktEncodeT *pkt,
	uint16 opClassLen, uint8 *opClass);

/* encode icon metadata */
int pktEncodeHspotAnqpIconMetadata(pktEncodeT *pkt,
	uint16 width, uint16 height, char *lang,
	uint8 typeLength, uint8 *type, uint8 filenameLength, uint8 *filename);
/* encode OSU provider */
int pktEncodeHspotAnqpOsuProvider(pktEncodeT *pkt,
	uint16 nameLength, uint8 *name,	uint8 uriLength, uint8 *uri,
	uint8 methodLength, uint8 *method, uint16 iconLength, uint8 *icon,
	uint8 naiLength, uint8 *nai, uint16 descLength, uint8 *desc);
/* encode OSU provider list */
int pktEncodeHspotAnqpOsuProviderList(pktEncodeT *pkt,
	uint16 nonTransProfileLength, uint8 *nonTransProfile,
	uint8 legacyOsuSsidLength, uint8 *legacyOsuSsid,
	uint16 providerLength, uint8 *provider);


/* encode anonymous NAI */
int pktEncodeHspotAnqpAnonymousNai(pktEncodeT *pkt, uint16 length, uint8 *nai);

/* encode icon request */
int pktEncodeHspotAnqpIconRequest(pktEncodeT *pkt, uint16 length, uint8 *filename);

/* encode icon binary file */
int pktEncodeHspotAnqpIconBinaryFile(pktEncodeT *pkt,
	uint8 status, uint8 typeLength, uint8 *type, uint16 length, uint8 *data);

#endif /* _PKTENCODEHSPOTANQP_H_ */
