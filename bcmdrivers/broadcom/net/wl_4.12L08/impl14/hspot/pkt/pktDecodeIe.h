/*
 * Decoding of information elememts.
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

#ifndef _PKTDECODEIE_H_
#define _PKTDECODEIE_H_

#include "typedefs.h"
#include "wlioctl.h"
#include "pktDecode.h"
#include "pktDecodeWfd.h"
#include "pktHspot.h"

typedef struct {
	int dsLength;
	uint8 *ds;
	int bssLoadLength;
	uint8 *bssLoad;
	int timeAdvertisementLength;
	uint8 *timeAdvertisement;
	int timeZoneLength;
	uint8 *timeZone;
	int interworkingLength;
	uint8 *interworking;
	int advertisementProtocolLength;
	uint8 *advertisementProtocol;
	int expeditedBandwidthRequestLength;
	uint8 *expeditedBandwidthRequest;
	int qosMapSetLength;
	uint8 *qosMapSet;
	int roamingConsortiumLength;
	uint8 *roamingConsortium;
	int emergencyAlertLength;
	uint8 *emergencyAlert;
	int extendedCapabilityLength;
	uint8 *extendedCapability;

	/* vendor specific */
	int hotspotIndicationLength;
	uint8 *hotspotIndication;
	int wpsIeLength;
	uint8 *wpsIe;
	int wfdIeLength;
	uint8 *wfdIe;

} pktIeT;

/* decode vendor IE */
int pktDecodeIe(pktDecodeT *pkt, pktIeT *ie);

/* decode hotspot 2.0 indication */
int pktDecodeIeHotspotIndication(pktDecodeT *pkt, uint8 *hotspotConfig);

typedef struct
{
	int isDgafDisabled;
	int isOsuBssid;
	uint8 releaseNumber;
} pktHotspotIndicationT;

/* decode hotspot 2.0 indication release2 */
int pktDecodeIeHotspotIndication2(pktDecodeT *pkt, pktHotspotIndicationT *hotspot);

typedef struct
{
	uint8 accessNetworkType;
	int isInternet;
	int isAsra;
	int isEsr;
	int isUesa;
	int isVenue;
	uint8 venueGroup;
	uint8 venueType;
	int isHessid;
	struct ether_addr hessid;
} pktInterworkingT;

/* decode interworking */
int pktDecodeIeInterworking(pktDecodeT *pkt, pktInterworkingT *interworking);

typedef struct
{
	uint8 queryResponseLimit;
	int isPamebi;
	uint8 protocolId;
} pktAdvertisementProtocolTupleT;

/* decode advertisement protocol tuple */
int pktDecodeIeAdvertisementProtocolTuple(pktDecodeT *pkt,
	pktAdvertisementProtocolTupleT *tuple);

#define MAX_ADVERTISEMENT_PROTOCOL	8
typedef struct
{
	int count;
	pktAdvertisementProtocolTupleT protocol[MAX_ADVERTISEMENT_PROTOCOL];
} pktAdvertisementProtocolT;

/* decode advertisement protocol */
int pktDecodeIeAdvertisementProtocol(pktDecodeT *pkt,
	pktAdvertisementProtocolT *advertise);

#define MAX_IE_OI_LENGTH	15
typedef struct
{
	uint8 length;
	uint8 data[MAX_IE_OI_LENGTH];
} pktOiT;

#define MAX_IE_OI	3
typedef struct
{
	uint8 anqpOiCount;
	uint8 count;
	pktOiT oi[MAX_IE_OI];
} pktRoamingConsortiumT;

/* decode roaming consortium */
int pktDecodeIeRoamingConsortium(pktDecodeT *pkt, pktRoamingConsortiumT *roam);

/* decode extended capabilities */
int pktDecodeIeExtendedCapabilities(pktDecodeT *pkt, uint32 *cap);

typedef struct
{
	uint16 year;
	uint8 month;
	uint8 day;
	uint8 hours;
	uint8 minutes;
	uint8 seconds;
	uint16 milliseconds;
	uint8 reserved;
} pktTimeValueT;

#define TIME_ERROR_LENGTH	5
#define TIME_UPDATE_LENGTH	1

typedef struct
{
	uint8 capabilities;
	pktTimeValueT timeValue;
	uint8 timeError[TIME_ERROR_LENGTH];
	uint8 timeUpdate[TIME_UPDATE_LENGTH];
} pktTimeAdvertisementT;

/* decode time advertisement */
int pktDecodeIeTimeAdvertisement(pktDecodeT *pkt, pktTimeAdvertisementT *time);

#define TIME_ZONE_LENGTH	255
typedef char pktTimeZoneT[TIME_ZONE_LENGTH + 1];	/* null terminated */

/* decode time zone */
int pktDecodeIeTimeZone(pktDecodeT *pkt, pktTimeZoneT *zone);

typedef struct
{
	uint16 stationCount;
	uint8 channelUtilization;
	uint16 availableAdmissionCapacity;
} pktBssLoadT;

/* decode BSS load */
int pktDecodeIeBssLoad(pktDecodeT *pkt, pktBssLoadT *load);

typedef struct
{
	uint16 channel;
	int isWfd;
	int isWfdDeviceInfoDecoded;
	pktWfdDeviceInfoT wfdDeviceInfo;
	int isWfdCapabilityDecoded;
	pktWfdCapabilityT wfdCapability;
	int isHotspotDecoded;
	uint8 hotspotConfig;
} pktProbeResponseT;

/* decode probe response from wl_bss_info_t */
int pktDecodeIeProbeResponse(wl_bss_info_t *bi, pktProbeResponseT *pr);

#endif /* _PKTDECODEIE_H_ */
