/*
 * Decoding of WiFi-Direct attributes.
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

#ifndef _PKTDECODEWFD_H_
#define _PKTDECODEWFD_H_

#include "typedefs.h"
#include "pktDecode.h"

typedef struct {
	int statusLength;
	uint8 *statusBuffer;
	int minorReasonCodeLength;
	uint8 *minorReasonCodeBuffer;
	int capabilityLength;
	uint8 *capabilityBuffer;
	int deviceIdLength;
	uint8 *deviceIdBuffer;
	int groupOwnerIntentLength;
	uint8 *groupOwnerIntentBuffer;
	int configurationTimeoutLength;
	uint8 *configurationTimeoutBuffer;
	int listenChannelLength;
	uint8 *listenChannelBuffer;
	int groupBssidLength;
	uint8 *groupBssidBuffer;
	int extendedListenTimingLength;
	uint8 *extendedListenTimingBuffer;
	int intendedInterfaceAddressLength;
	uint8 *intendedInterfaceAddressBuffer;
	int manageabilityLength;
	uint8 *manageabilityBuffer;
	int channelListLength;
	uint8 *channelListBuffer;
	int noticeOfAbsenceLength;
	uint8 *noticeOfAbsenseBuffer;
	int deviceInfoLength;
	uint8 *deviceInfoBuffer;
	int groupInfoLength;
	uint8 *groupInfoBuffer;
	int groupIdLength;
	uint8 *groupIdBuffer;
	int interfaceLength;
	uint8 *interfaceBuffer;
	int operatingChannelLength;
	uint8 *operatingChannelBuffer;
	int invitationFlagsLength;
	uint8 *invitationFlagsBuffer;
} pktDecodeWfdT;

/* decode WFD */
int pktDecodeWfd(pktDecodeT *pkt, pktDecodeWfdT *wfd);

typedef uint8 deviceTypeT[8];
#define MAX_SECONDARY_DEVICE_TYPE	4
#define MAX_DEVICE_NAME	32

typedef struct
{
	struct ether_addr deviceAddress;
	uint16 configMethods;
	deviceTypeT primaryType;
	uint8 numSecondaryType;
	deviceTypeT secondaryType[MAX_SECONDARY_DEVICE_TYPE];
	uint8 deviceName[MAX_DEVICE_NAME + 1];
} pktWfdDeviceInfoT;

/* decode device info */
int pktDecodeWfdDeviceInfo(pktDecodeT *pkt, pktWfdDeviceInfoT *device);

/* print decoded device info */
void pktDecodeWfdDeviceInfoPrint(pktWfdDeviceInfoT *device);

typedef struct
{
	uint8 device;
	uint8 group;
} pktWfdCapabilityT;

/* decode capability */
int pktDecodeWfdCapability(pktDecodeT *pkt, pktWfdCapabilityT *capability);

/* print decoded capability */
void pktDecodeWfdCapabilityPrint(pktWfdCapabilityT *capability);

#endif /* _PKTDECODEWFD_H_ */
