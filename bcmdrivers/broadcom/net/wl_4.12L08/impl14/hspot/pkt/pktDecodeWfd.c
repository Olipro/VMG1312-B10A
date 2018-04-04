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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proto/p2p.h"
#include "proto/wps.h"
#include "trace.h"
#include "pktDecodeWfd.h"

static void printWfdDecode(pktDecodeWfdT *wfd)
{
	WL_P2PO(("decoded WFD IEs:\n"));

	if (wfd->statusBuffer) {
		WL_PRPKT("   P2P_SEID_STATUS",
			wfd->statusBuffer, wfd->statusLength);
	}
	if (wfd->minorReasonCodeBuffer) {
		WL_PRPKT("   P2P_SEID_MINOR_RC",
			wfd->minorReasonCodeBuffer, wfd->minorReasonCodeLength);
	}
	if (wfd->capabilityBuffer) {
		WL_PRPKT("   P2P_SEID_P2P_INFO",
			wfd->capabilityBuffer, wfd->capabilityLength);
	}
	if (wfd->deviceIdBuffer) {
		WL_PRPKT("   P2P_SEID_DEV_ID",
			wfd->deviceIdBuffer, wfd->deviceIdLength);
	}
	if (wfd->groupOwnerIntentBuffer) {
		WL_PRPKT("   P2P_SEID_INTENT",
			wfd->groupOwnerIntentBuffer, wfd->groupOwnerIntentLength);
	}
	if (wfd->configurationTimeoutBuffer) {
		WL_PRPKT("   P2P_SEID_CFG_TIMEOUT",
			wfd->configurationTimeoutBuffer, wfd->configurationTimeoutLength);
	}
	if (wfd->listenChannelBuffer) {
		WL_PRPKT("   P2P_SEID_CHANNEL",
			wfd->listenChannelBuffer, wfd->listenChannelLength);
	}
	if (wfd->groupBssidBuffer) {
		WL_PRPKT("   P2P_SEID_GRP_BSSID",
			wfd->groupBssidBuffer, wfd->groupBssidLength);
	}
	if (wfd->extendedListenTimingBuffer) {
		WL_PRPKT("   P2P_SEID_XT_TIMING",
			wfd->extendedListenTimingBuffer, wfd->extendedListenTimingLength);
	}
	if (wfd->intendedInterfaceAddressBuffer) {
		WL_PRPKT("   P2P_SEID_INTINTADDR",
			wfd->intendedInterfaceAddressBuffer, wfd->intendedInterfaceAddressLength);
	}
	if (wfd->manageabilityBuffer) {
		WL_PRPKT("   P2P_SEID_P2P_MGBTY",
			wfd->manageabilityBuffer, wfd->manageabilityLength);
	}
	if (wfd->channelListBuffer) {
		WL_PRPKT("   P2P_SEID_CHAN_LIST",
			wfd->channelListBuffer, wfd->channelListLength);
	}
	if (wfd->noticeOfAbsenseBuffer) {
		WL_PRPKT("   P2P_SEID_ABSENCE",
			wfd->noticeOfAbsenseBuffer, wfd->noticeOfAbsenceLength);
	}
	if (wfd->deviceInfoBuffer) {
		WL_PRPKT("   P2P_SEID_DEV_INFO",
			wfd->deviceInfoBuffer, wfd->deviceInfoLength);
	}
	if (wfd->groupInfoBuffer) {
		WL_PRPKT("   P2P_SEID_GROUP_INFO",
			wfd->groupInfoBuffer, wfd->groupInfoLength);
	}
	if (wfd->groupIdBuffer) {
		WL_PRPKT("   P2P_SEID_GROUP_ID",
			wfd->groupIdBuffer, wfd->groupIdLength);
	}
	if (wfd->interfaceBuffer) {
		WL_PRPKT("   P2P_SEID_P2P_IF",
			wfd->interfaceBuffer, wfd->interfaceLength);
	}
	if (wfd->operatingChannelBuffer) {
		WL_PRPKT("   P2P_SEID_OP_CHANNEL",
			wfd->operatingChannelBuffer, wfd->operatingChannelLength);
	}
	if (wfd->invitationFlagsBuffer) {
		WL_PRPKT("   P2P_SEID_INVITE_FLAGS",
			wfd->invitationFlagsBuffer, wfd->invitationFlagsLength);
	}
}

/* decode WFD */
int pktDecodeWfd(pktDecodeT *pkt, pktDecodeWfdT *wfd)
{
	uint8 oui[WFA_OUI_LEN];
	uint8 type;
	int nextIeOffset = 0;
	int ieCount = 0;

	WL_PRPKT("packet for WFD decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(wfd, 0, sizeof(*wfd));

	/* check OUI */
	if (!pktDecodeBytes(pkt, WFA_OUI_LEN, oui) ||
		memcmp(oui, WFA_OUI, WFA_OUI_LEN) != 0) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	/* check type */
	if (!pktDecodeByte(pkt, &type) || type != WFA_OUI_TYPE_P2P) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	nextIeOffset = pktDecodeOffset(pkt);

	while (nextIeOffset < pktDecodeBufLength(pkt)) {
		uint8 id;
		uint16 length;
		int dataLength;
		uint8 *dataPtr;

		pktDecodeOffset(pkt) = nextIeOffset;
		WL_TRACE(("decoding offset 0x%x\n", pktDecodeOffset(pkt)));

		/* minimum ID and length */
		if (pktDecodeRemaining(pkt) < 3) {
			WL_P2PO(("ID and length too short\n"));
			break;
		}

		pktDecodeByte(pkt, &id);
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
		case P2P_SEID_STATUS:
			wfd->statusLength = dataLength;
			wfd->statusBuffer = dataPtr;
			break;
		case P2P_SEID_MINOR_RC:
			wfd->minorReasonCodeLength = dataLength;
			wfd->minorReasonCodeBuffer = dataPtr;
			break;
		case P2P_SEID_P2P_INFO:
			wfd->capabilityLength = dataLength;
			wfd->capabilityBuffer = dataPtr;
			break;
		case P2P_SEID_DEV_ID:
			wfd->deviceIdLength = dataLength;
			wfd->deviceIdBuffer = dataPtr;
			break;
		case P2P_SEID_INTENT:
			wfd->groupOwnerIntentLength = dataLength;
			wfd->groupOwnerIntentBuffer = dataPtr;
			break;
		case P2P_SEID_CFG_TIMEOUT:
			wfd->configurationTimeoutLength = dataLength;
			wfd->configurationTimeoutBuffer = dataPtr;
			break;
		case P2P_SEID_CHANNEL:
			wfd->listenChannelLength = dataLength;
			wfd->listenChannelBuffer = dataPtr;
			break;
		case P2P_SEID_GRP_BSSID:
			wfd->groupBssidLength = dataLength;
			wfd->groupBssidBuffer = dataPtr;
			break;
		case P2P_SEID_XT_TIMING:
			wfd->extendedListenTimingLength = dataLength;
			wfd->extendedListenTimingBuffer = dataPtr;
			break;
		case P2P_SEID_INTINTADDR:
			wfd->intendedInterfaceAddressLength = dataLength;
			wfd->intendedInterfaceAddressBuffer = dataPtr;
			break;
		case P2P_SEID_P2P_MGBTY:
			wfd->manageabilityLength = dataLength;
			wfd->manageabilityBuffer = dataPtr;
			break;
		case P2P_SEID_CHAN_LIST:
			wfd->channelListLength = dataLength;
			wfd->channelListBuffer = dataPtr;
			break;
		case P2P_SEID_ABSENCE:
			wfd->noticeOfAbsenceLength = dataLength;
			wfd->noticeOfAbsenseBuffer = dataPtr;
			break;
		case P2P_SEID_DEV_INFO:
			wfd->deviceInfoLength = dataLength;
			wfd->deviceInfoBuffer = dataPtr;
			break;
		case P2P_SEID_GROUP_INFO:
			wfd->groupInfoLength = dataLength;
			wfd->groupInfoBuffer = dataPtr;
			break;
		case P2P_SEID_GROUP_ID:
			wfd->groupIdLength = dataLength;
			wfd->groupIdBuffer = dataPtr;
			break;
		case P2P_SEID_P2P_IF:
			wfd->interfaceLength = dataLength;
			wfd->interfaceBuffer = dataPtr;
			break;
		case P2P_SEID_OP_CHANNEL:
			wfd->operatingChannelLength = dataLength;
			wfd->operatingChannelBuffer = dataPtr;
			break;
		case P2P_SEID_INVITE_FLAGS:
			wfd->invitationFlagsLength = dataLength;
			wfd->invitationFlagsBuffer = dataPtr;
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
		printWfdDecode(wfd);

	return ieCount;
}

/* decode device info */
int pktDecodeWfdDeviceInfo(pktDecodeT *pkt, pktWfdDeviceInfoT *device)
{
	int i;
	uint16 type;
	uint16 length;

	WL_PRPKT("packet for WFD device info decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(device, 0, sizeof(*device));

	/* allow zero length */
	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (!pktDecodeBytes(pkt, 6, (uint8 *)&device->deviceAddress))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeBe16(pkt, &device->configMethods)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, 8, (uint8 *)&device->primaryType))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &device->numSecondaryType))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (device->numSecondaryType > MAX_SECONDARY_DEVICE_TYPE)
	{
		WL_ERROR(("num secondary device type %d > %d\n",
			device->numSecondaryType, MAX_SECONDARY_DEVICE_TYPE));
		return FALSE;
	}

	for (i = 0; i < device->numSecondaryType; i++)
	{
		if (!pktDecodeBytes(pkt, 8, (uint8 *)&device->secondaryType[i]))
		{
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
	}

	if (!pktDecodeBe16(pkt, &type) || type != WPS_ID_DEVICE_NAME) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeBe16(pkt, &length)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (length > MAX_DEVICE_NAME)
	{
		WL_ERROR(("device name length %d > %d\n",
			length, MAX_DEVICE_NAME));
		return FALSE;
	}
	if (pktDecodeBytes(pkt, length, device->deviceName) != length)
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	device->deviceName[length] = 0;

	if (WL_P2PO_ON())
		pktDecodeWfdDeviceInfoPrint(device);
	return TRUE;
}

/* print decoded device info */
void pktDecodeWfdDeviceInfoPrint(pktWfdDeviceInfoT *device)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded WFD device info:\n"));
	WL_PRPKT("   device address",
		(uint8 *)&device->deviceAddress, sizeof(device->deviceAddress));
	WL_PRINT(("   config methods = 0x%04x\n", device->configMethods));
	WL_PRPKT("   primary device type",
		(uint8 *)&device->primaryType, sizeof(device->primaryType));
	WL_PRINT(("   num secondary device type = %d\n", device->numSecondaryType));
	for (i = 0; i < device->numSecondaryType; i++) {
		WL_PRPKT("   secondary device type",
			(uint8 *)&device->secondaryType[i], sizeof(device->secondaryType[i]));
	}
	WL_PRPKT("   device name",
		(uint8 *)&device->deviceName, strlen((char *)device->deviceName));
}

/* decode capability */
int pktDecodeWfdCapability(pktDecodeT *pkt, pktWfdCapabilityT *capability)
{
	WL_PRPKT("packet for WFD capability decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(capability, 0, sizeof(*capability));

	if (pktDecodeRemaining(pkt) != 2)
		return FALSE;

	if (!pktDecodeByte(pkt, &capability->device))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &capability->group))
	{
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (WL_P2PO_ON())
		pktDecodeWfdCapabilityPrint(capability);
	return TRUE;
}

/* print decoded capability */
void pktDecodeWfdCapabilityPrint(pktWfdCapabilityT *capability)
{
	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded WFD capability:\n"));
	WL_PRINT(("   device = 0x%02x\n", capability->device));
	WL_PRINT(("   group  = 0x%02x\n", capability->group));
}
