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

#ifndef _PKTENCODEIE_H_
#define _PKTENCODEIE_H_

#include "typedefs.h"
#include "pktEncode.h"
#include "pktHspot.h"

/* encode hotspot 2.0 indication */
int pktEncodeIeHotspotIndication(pktEncodeT *pkt, uint8 hotspotConfig);

/* encode hotspot 2.0 indication release2 */
int pktEncodeIeHotspotIndication2(pktEncodeT *pkt,
	int isDgafDisabled, int isOsuBssid, uint8 releaseNumber);

/* encode interworking */
int pktEncodeIeInterworking(pktEncodeT *pkt, uint8 accessNetworkType,
	int isInternet, int isAsra, int isEsr, int isUesa,
	int isVenue, uint8 venueGroup, uint8 venueType, struct ether_addr *hessid);

/* encode advertisement protocol tuple */
int pktEncodeIeAdvertisementProtocolTuple(pktEncodeT *pkt,
	int isPamebi, uint8 qResponseLimit, uint8 protocolId);

/* encode advertisement protocol */
int pktEncodeIeAdvertiseProtocol(pktEncodeT *pkt, uint8 len, uint8 *data);

/* encode roaming consortium */
int pktEncodeIeRoamingConsortium(pktEncodeT *pkt, uint8 numAnqpOi,
	uint8 oi1Len, uint8 *oi1, uint8 oi2Len, uint8 *oi2,
	uint8 oi3Len, uint8 *oi3);

/* encode extended capabilities */
int pktEncodeIeExtendedCapabilities(pktEncodeT *pkt, uint32 cap);

/* encode advertisement protocol */
int pktEncodeIeAdvertisementProtocol(pktEncodeT *pkt,
	uint8 pamebi, uint8 qRspLimit, uint8 id);

#endif /* _PKTENCODEIE_H_ */
