/*
 * Decoding of Hotspot2.0 ANQP packets.
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
#include "pktDecodeHspotAnqp.h"

/* print decoded hotspot ANQP */
void printHspotAnqpDecode(pktDecodeHspotAnqpT *hspot)
{
	WL_P2PO(("decoded hotspot ANQP frame:\n"));

	if (hspot->queryListBuffer) {
		WL_PRPKT("   query list",
			hspot->queryListBuffer, hspot->queryListLength);
	}
	if (hspot->capabilityListBuffer) {
		WL_PRPKT("   capability list",
			hspot->capabilityListBuffer, hspot->capabilityListLength);
	}
	if (hspot->operatorFriendlyNameBuffer) {
		WL_PRPKT("   operator friendly name",
			hspot->operatorFriendlyNameBuffer,
			hspot->operatorFriendlyNameLength);
	}
	if (hspot->wanMetricsBuffer) {
		WL_PRPKT("   wan metrics",
			hspot->wanMetricsBuffer, hspot->wanMetricsLength);
	}
	if (hspot->connectionCapabilityBuffer) {
		WL_PRPKT("   connection capability",
			hspot->connectionCapabilityBuffer,
			hspot->connectionCapabilityLength);
	}
	if (hspot->naiHomeRealmQueryBuffer) {
		WL_PRPKT("   NAI home realm query",
			hspot->naiHomeRealmQueryBuffer,
			hspot->naiHomeRealmQueryLength);
	}
	if (hspot->opClassIndicationBuffer) {
		WL_PRPKT("   operating class indication",
			hspot->opClassIndicationBuffer,
			hspot->opClassIndicationLength);
	}
	if (hspot->onlineSignupProvidersBuffer) {
		WL_PRPKT("   online sign-up providers",
			hspot->onlineSignupProvidersBuffer,
			hspot->onlineSignupProvidersLength);
	}
	if (hspot->anonymousNaiBuffer) {
		WL_PRPKT("   anonymous NAI",
			hspot->anonymousNaiBuffer,
			hspot->anonymousNaiLength);
	}
	if (hspot->iconRequestBuffer) {
		WL_PRPKT("   icon request",
			hspot->iconRequestBuffer,
			hspot->iconRequestLength);
	}
	if (hspot->iconBinaryFileBuffer) {
		WL_PRPKT("   icon binary file",
			hspot->iconBinaryFileBuffer,
			hspot->iconBinaryFileLength);
	}
}

/* decode hotspot ANQP frame */
int pktDecodeHspotAnqp(pktDecodeT *pkt, int isReset, pktDecodeHspotAnqpT *hspot)
{
	int nextIeOffset = 0;
	int ieCount = 0;

	WL_PRPKT("packet for hotspot ANQP decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	if (isReset)
		memset(hspot, 0, sizeof(*hspot));

	while (nextIeOffset < pktDecodeBufLength(pkt)) {
		uint16 infoId;
		uint16 length;
		uint8 oui[WFA_OUI_LEN];
		uint8 type;
		uint8 subtype;
		uint8 reserved;
		int dataLength;
		uint8 *dataPtr;

		pktDecodeOffset(pkt) = nextIeOffset;
		WL_P2PO(("decoding offset 0x%x\n", pktDecodeOffset(pkt)));

		/* minimum ID and length */
		if (pktDecodeRemaining(pkt) < 4) {
			WL_P2PO(("ID and length too short\n"));
			break;
		}

		pktDecodeLe16(pkt, &infoId);
		pktDecodeLe16(pkt, &length);

		/* check length */
		if (length > pktDecodeRemaining(pkt)) {
			WL_P2PO(("length exceeds packet %d > %d\n",
				length, pktDecodeRemaining(pkt)));
			break;
		}
		nextIeOffset = pktDecodeOffset(pkt) + length;

		/* check ID */
		if (infoId != ANQP_ID_VENDOR_SPECIFIC_LIST) {
			WL_P2PO(("invalid ID 0x%04x\n", infoId));
			continue;
		}

		if (length < HSPOT_LENGTH_OVERHEAD) {
			WL_P2PO(("length too short %d < %d\n",
				length, HSPOT_LENGTH_OVERHEAD));
			continue;
		}

		/* check OUI */
		if (!pktDecodeBytes(pkt, WFA_OUI_LEN, oui) ||
			memcmp(oui, WFA_OUI, WFA_OUI_LEN) != 0)
			continue;

		/* check type */
		if (!pktDecodeByte(pkt, &type) || type != HSPOT_ANQP_OUI_TYPE)
			continue;

		if (!pktDecodeByte(pkt, &subtype))
			continue;

		if (!pktDecodeByte(pkt, &reserved))
			continue;

		/* remaining data */
		dataLength = length - HSPOT_LENGTH_OVERHEAD;
		dataPtr = pktDecodeCurrentPtr(pkt);

		switch (subtype)
		{
		case HSPOT_SUBTYPE_QUERY_LIST:
			hspot->queryListLength = dataLength;
			hspot->queryListBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_CAPABILITY_LIST:
			hspot->capabilityListLength = dataLength;
			hspot->capabilityListBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME:
			hspot->operatorFriendlyNameLength = dataLength;
			hspot->operatorFriendlyNameBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_WAN_METRICS:
			hspot->wanMetricsLength = dataLength;
			hspot->wanMetricsBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_CONNECTION_CAPABILITY:
			hspot->connectionCapabilityLength = dataLength;
			hspot->connectionCapabilityBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY:
			hspot->naiHomeRealmQueryLength = dataLength;
			hspot->naiHomeRealmQueryBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION:
			hspot->opClassIndicationLength = dataLength;
			hspot->opClassIndicationBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS:
			hspot->onlineSignupProvidersLength = dataLength;
			hspot->onlineSignupProvidersBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_ANONYMOUS_NAI:
			hspot->anonymousNaiLength = dataLength;
			hspot->anonymousNaiBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_ICON_REQUEST:
			hspot->iconRequestLength = dataLength;
			hspot->iconRequestBuffer = dataPtr;
			break;
		case HSPOT_SUBTYPE_ICON_BINARY_FILE:
			hspot->iconBinaryFileLength = dataLength;
			hspot->iconBinaryFileBuffer = dataPtr;
			break;
		default:
			WL_P2PO(("invalid subtype %d\n", subtype));
			continue;
			break;
		}

		/* count IEs decoded */
		ieCount++;
	}


	return ieCount;
}

/* decode query list */
int pktDecodeHspotAnqpQueryList(pktDecodeT *pkt, pktHspotAnqpQueryListT *queryList)
{
	int count, i;

	WL_PRPKT("packet for hotspot query list decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(queryList, 0, sizeof(*queryList));

	count =  pktDecodeRemaining(pkt);
	for (i = 0; i < count; i++) {
		if (i >= MAX_LIST_SIZE) {
			WL_ERROR(("truncating query list to %d\n",
				MAX_LIST_SIZE));
			return FALSE;
		}
		pktDecodeByte(pkt, &queryList->queryId[queryList->queryLen++]);
	}

	if (WL_P2PO_ON())
		pktDecodeHspotAnqpQueryListPrint(queryList);
	return TRUE;
}

/* print decoded query list */
void pktDecodeHspotAnqpQueryListPrint(pktHspotAnqpQueryListT *queryList)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot ANQP query list:\n"));
	WL_PRINT(("query count = %d\n", queryList->queryLen));

	for (i = 0; i < queryList->queryLen; i++) {
		char *queryStr;

		switch (queryList->queryId[i]) {
		case HSPOT_SUBTYPE_QUERY_LIST:
			queryStr = "HSPOT_SUBTYPE_QUERY_LIST";
			break;
		case HSPOT_SUBTYPE_CAPABILITY_LIST:
			queryStr = "HSPOT_SUBTYPE_CAPABILITY_LIST";
			break;
		case HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME:
			queryStr = "HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME";
			break;
		case HSPOT_SUBTYPE_WAN_METRICS:
			queryStr = "HSPOT_SUBTYPE_WAN_METRICS";
			break;
		case HSPOT_SUBTYPE_CONNECTION_CAPABILITY:
			queryStr = "HSPOT_SUBTYPE_CONNECTION_CAPABILITY";
			break;
		case HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY:
			queryStr = "HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY";
			break;
		case HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION:
			queryStr = "HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION";
			break;
		case HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS:
			queryStr = "HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS";
			break;
		case HSPOT_SUBTYPE_ANONYMOUS_NAI:
			queryStr = "HSPOT_SUBTYPE_ANONYMOUS_NAI";
			break;
		case HSPOT_SUBTYPE_ICON_REQUEST:
			queryStr = "HSPOT_SUBTYPE_ICON_REQUEST";
			break;
		case HSPOT_SUBTYPE_ICON_BINARY_FILE:
			queryStr = "HSPOT_SUBTYPE_ICON_BINARY_FILE";
			break;
		default:
			queryStr = "unknown";
			break;
		}

		WL_PRINT(("   %s (%d)\n", queryStr, queryList->queryId[i]));
	}
}

/* decode capability list */
int pktDecodeHspotAnqpCapabilityList(pktDecodeT *pkt, pktHspotAnqpCapabilityListT *capList)
{
	int count, i;

	WL_PRPKT("packet for hotspot capability list decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(capList, 0, sizeof(*capList));

	count =  pktDecodeRemaining(pkt);
	for (i = 0; i < count; i++) {
		if (i >= MAX_LIST_SIZE) {
			WL_ERROR(("truncating capability list to %d\n",
				MAX_LIST_SIZE));
			return FALSE;
		}
		pktDecodeByte(pkt, &capList->capId[capList->capLen++]);
	}

	if (WL_P2PO_ON())
		pktDecodeHspotAnqpCapabilityListPrint(capList);
	return TRUE;
}

/* print decoded capability list */
void pktDecodeHspotAnqpCapabilityListPrint(pktHspotAnqpCapabilityListT *capList)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot ANQP capability list:\n"));
	WL_PRINT(("capability count = %d\n", capList->capLen));

	for (i = 0; i < capList->capLen; i++) {
		char *capStr;

		switch (capList->capId[i]) {
		case HSPOT_SUBTYPE_QUERY_LIST:
			capStr = "HSPOT_SUBTYPE_QUERY_LIST";
			break;
		case HSPOT_SUBTYPE_CAPABILITY_LIST:
			capStr = "HSPOT_SUBTYPE_CAPABILITY_LIST";
			break;
		case HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME:
			capStr = "HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME";
			break;
		case HSPOT_SUBTYPE_WAN_METRICS:
			capStr = "HSPOT_SUBTYPE_WAN_METRICS";
			break;
		case HSPOT_SUBTYPE_CONNECTION_CAPABILITY:
			capStr = "HSPOT_SUBTYPE_CONNECTION_CAPABILITY";
			break;
		case HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY:
			capStr = "HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY";
			break;
		case HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION:
			capStr = "HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION";
			break;
		case HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS:
			capStr = "HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS";
			break;
		case HSPOT_SUBTYPE_ANONYMOUS_NAI:
			capStr = "HSPOT_SUBTYPE_ANONYMOUS_NAI";
			break;
		case HSPOT_SUBTYPE_ICON_REQUEST:
			capStr = "HSPOT_SUBTYPE_ICON_REQUEST";
			break;
		case HSPOT_SUBTYPE_ICON_BINARY_FILE:
			capStr = "HSPOT_SUBTYPE_ICON_BINARY_FILE";
			break;
		default:
			capStr = "unknown";
			break;
		}

		WL_PRINT(("   %s (%d)\n", capStr, capList->capId[i]));
	}
}

/* decode operator name duple */
static int pktDecodeHspotAnqpOperatorNameDuple(pktDecodeT *pkt, pktHspotAnqpNameDupleT *duple)
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
		WL_ERROR(("decode error\n"));
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

/* decode operator friendly name */
int pktDecodeHspotAnqpOperatorFriendlyName(pktDecodeT *pkt, pktHspotAnqpOperatorFriendlyNameT *op)
{
	WL_PRPKT("packet for hotspot operator friendly name decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(op, 0, sizeof(*op));

	while (pktDecodeRemaining(pkt) > 0 &&
		op->numName < MAX_OPERATOR_NAME) {
		if (!pktDecodeHspotAnqpOperatorNameDuple(pkt,
			&op->duple[op->numName])) {
			return FALSE;
		}
		else {
			op->numName++;
		}
	}

	if (WL_P2PO_ON())
		pktDecodeHspotAnqpOperatorFriendlyNamePrint(op);
	return TRUE;
}

/* print decoded operator friendly name */
void pktDecodeHspotAnqpOperatorFriendlyNamePrint(pktHspotAnqpOperatorFriendlyNameT *op)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot ANQP operator friendly name:\n"));
	for (i = 0; i < op->numName; i++) {
		WL_PRINT(("   language: %s\n", op->duple[i].lang));
		WL_PRPKT("   operator name",
			(uint8 *)op->duple[i].name, op->duple[i].nameLen);
	}
}

/* decode WAN metrics */
int pktDecodeHspotAnqpWanMetrics(pktDecodeT *pkt, pktHspotAnqpWanMetricsT *wanMetrics)
{
	uint8 wanInfo;

	WL_PRPKT("packet for hotspot WAN metrics decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(wanMetrics, 0, sizeof(*wanMetrics));

	if (!pktDecodeByte(pkt, &wanInfo)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	wanMetrics->linkStatus =
		(wanInfo & HSPOT_WAN_LINK_STATUS_MASK) >> HSPOT_WAN_LINK_STATUS_SHIFT;
	wanMetrics->symmetricLink =
		(wanInfo & HSPOT_WAN_SYMMETRIC_LINK_MASK) >> HSPOT_WAN_SYMMETRIC_LINK_SHIFT;
	wanMetrics->atCapacity =
		(wanInfo & HSPOT_WAN_AT_CAPACITY_MASK) >> HSPOT_WAN_AT_CAPACITY_SHIFT;

	if (!pktDecodeLe32(pkt, &wanMetrics->dlinkSpeed)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeLe32(pkt, &wanMetrics->ulinkSpeed)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &wanMetrics->dlinkLoad)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &wanMetrics->ulinkLoad)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeLe16(pkt, &wanMetrics->lmd)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (WL_P2PO_ON())
		pktDecodeHspotAnqpWanMetricsPrint(wanMetrics);
	return TRUE;
}

/* print decoded WAN metrics */
void pktDecodeHspotAnqpWanMetricsPrint(pktHspotAnqpWanMetricsT *wanMetrics)
{
	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot ANQP WAN metrics:\n"));
	WL_PRINT(("   link status              : %d\n", wanMetrics->linkStatus));
	WL_PRINT(("   symmetric link           : %d\n", wanMetrics->symmetricLink));
	WL_PRINT(("   at capacity              : %d\n", wanMetrics->atCapacity));
	WL_PRINT(("   downlink speed           : %d\n", wanMetrics->dlinkSpeed));
	WL_PRINT(("   up speed                 : %d\n", wanMetrics->ulinkSpeed));
	WL_PRINT(("   downlink load            : %d\n", wanMetrics->dlinkLoad));
	WL_PRINT(("   up load                  : %d\n", wanMetrics->ulinkLoad));
	WL_PRINT(("   load measurement duration: %d\n", wanMetrics->lmd));
}

/* decode protocol port tuple */
static int pktDecodeHspotAnqpProtoPortTuple(pktDecodeT *pkt, pktHspotAnqpProtoPortT *protoPort)
{

	if (!pktDecodeByte(pkt, &protoPort->ipProtocol)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeLe16(pkt, &protoPort->portNumber)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &protoPort->status)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* decode connection capability */
int pktDecodeHspotAnqpConnectionCapability(pktDecodeT *pkt, pktHspotAnqpConnectionCapabilityT *cap)
{
	WL_PRPKT("packet for hotspot connection capability decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(cap, 0, sizeof(*cap));

	while (pktDecodeRemaining(pkt) > 0 &&
		cap->numConnectCap < MAX_CONNECTION_CAPABILITY) {
		if (!pktDecodeHspotAnqpProtoPortTuple(pkt,
			&cap->tuple[cap->numConnectCap])) {
			return FALSE;
		}
		else {
			cap->numConnectCap++;
		}
	}

	if (WL_P2PO_ON())
		pktDecodeHspotAnqpConnectionCapabilityPrint(cap);
	return TRUE;
}

/* print decoded connection capability */
void pktDecodeHspotAnqpConnectionCapabilityPrint(pktHspotAnqpConnectionCapabilityT *cap)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot ANQP connection capability:\n"));
	WL_PRINT(("   count = %d\n", cap->numConnectCap));
	for (i = 0; i < cap->numConnectCap; i++) {
		WL_PRINT(("   IP protocol = %3d   port = %4d   status = %s\n",
			cap->tuple[i].ipProtocol, cap->tuple[i].portNumber,
			cap->tuple[i].status == 0 ? "closed" :
			cap->tuple[i].status == 1 ? "open" :
			cap->tuple[i].status == 2 ? "unknown" :
			"reserved"));
	}
}

static int pktDecodeHspotAnqpNaiHomeRealmQueryData(pktDecodeT *pkt,
	pktHspotAnqpNaiHomeRealmDataT *data)
{

	if (!pktDecodeByte(pkt, &data->encoding)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &data->nameLen)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (data->nameLen > pktDecodeRemaining(pkt)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (data->nameLen && !pktDecodeBytes(pkt, data->nameLen, (uint8 *)data->name)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	data->name[data->nameLen] = 0;

	return TRUE;
}

/* decode home realm query */
int pktDecodeHspotAnqpNaiHomeRealmQuery(pktDecodeT *pkt, pktHspotAnqpNaiHomeRealmQueryT *realm)
{
	int i;
	WL_PRPKT("packet for hotspot NAI home realm decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(realm, 0, sizeof(*realm));

	if (!pktDecodeByte(pkt, &realm->count)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (realm->count > MAX_HOME_REALM) {
		WL_ERROR(("home realm count %d > %d\n", realm->count, MAX_HOME_REALM));
		return FALSE;
	}

	for (i = 0; i < realm->count; i++) {
		if (!pktDecodeHspotAnqpNaiHomeRealmQueryData(pkt,
			&realm->data[i])) {
			return FALSE;
		}
	}

	if (WL_P2PO_ON())
		pktDecodeHspotAnqpNaiHomeRealmQueryPrint(realm);
	return TRUE;
}

/* print decoded home realm query */
void pktDecodeHspotAnqpNaiHomeRealmQueryPrint(pktHspotAnqpNaiHomeRealmQueryT *realm)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot ANQP home realm query:\n"));
	WL_PRINT(("   count = %d\n", realm->count));
	for (i = 0; i < realm->count; i++) {
		WL_PRINT(("   realm = %s\n", realm->data[i].name));
	}
}

/* decode operating class indication */
int pktDecodeHspotAnqpOperatingClassIndication(pktDecodeT *pkt,
	pktHspotAnqpOperatingClassIndicationT *opClassList)
{
	int count;

	WL_PRPKT("packet for hotspot operating class indication decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(opClassList, 0, sizeof(*opClassList));

	count = pktDecodeRemaining(pkt);
	if (count > MAX_OPCLASS_LIST_SIZE) {
		WL_ERROR(("operating class indication list size is too large %d\n",
			MAX_OPCLASS_LIST_SIZE));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, count, (uint8 *)opClassList->opClass)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}

	opClassList->opClassLen = count;
	if (WL_P2PO_ON())
		pktDecodeHspotAnqpOperatingClassIndicationPrint(opClassList);
	return TRUE;
}

/* print decoded operating class indication */
void pktDecodeHspotAnqpOperatingClassIndicationPrint(
	pktHspotAnqpOperatingClassIndicationT *opClassList)
{
	int i;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot operating class indication list:\n"));
	WL_PRPKT("operating class",
		opClassList->opClass, opClassList->opClassLen);

	for (i = 0; i < opClassList->opClassLen; i++)
		WL_PRINT(("   operating class %i: %d\n", i, opClassList->opClass[i]));
}

/* decode icon metadata */
static int pktDecodeHspotAnqpIconMetadata(pktDecodeT *pkt, pktHspotAnqpIconMetadataT *icon)
{

	if (!pktDecodeLe16(pkt, &icon->width)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeLe16(pkt, &icon->height)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeBytes(pkt, VENUE_LANGUAGE_CODE_SIZE, (uint8 *)icon->lang)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}
	icon->lang[VENUE_LANGUAGE_CODE_SIZE] = 0;
	if (!pktDecodeByte(pkt, &icon->typeLength)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (icon->typeLength > pktDecodeRemaining(pkt)) {
		WL_ERROR(("type length exceeds packet %d > %d\n",
			icon->typeLength, pktDecodeRemaining(pkt)));
		return FALSE;
	}
	if (icon->typeLength > MAX_ICON_TYPE_LENGTH) {
		WL_ERROR(("type exceeds buffer %d > %d\n",
			icon->typeLength, MAX_ICON_TYPE_LENGTH));
		return FALSE;
	}
	if (!pktDecodeBytes(pkt, icon->typeLength, (uint8 *)icon->type)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}
	icon->type[icon->typeLength] = 0;

	if (!pktDecodeByte(pkt, &icon->filenameLength)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (icon->filenameLength > pktDecodeRemaining(pkt)) {
		WL_ERROR(("filename length exceeds packet %d > %d\n",
			icon->filenameLength, pktDecodeRemaining(pkt)));
		return FALSE;
	}
	if (icon->filenameLength > MAX_ICON_FILENAME_LENGTH) {
		WL_ERROR(("filename exceeds buffer %d > %d\n",
			icon->filenameLength, MAX_ICON_FILENAME_LENGTH));
		return FALSE;
	}
	if (!pktDecodeBytes(pkt, icon->filenameLength, (uint8 *)icon->filename)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}
	icon->filename[icon->filenameLength] = 0;

	return TRUE;
}

/* decode OSU provider list */
int pktDecodeHspotAnqpOsuProviderList(pktDecodeT *pkt, pktHspotAnqpOsuProviderListT *list)
{
	int len;

	WL_PRPKT("packet for hotspot OSU provider list decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(list, 0, sizeof(*list));

	/* decode non-transmitted profile */
	if (!pktDecodeLe16(pkt, &list->nonTransProfileLength)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (list->nonTransProfileLength > MAX_NON_TRANS_PROFILE_LENGTH) {
		WL_ERROR(("length exceeds maximum %d > %d\n",
			list->nonTransProfileLength, MAX_NON_TRANS_PROFILE_LENGTH));
		return FALSE;
	}
	if (list->nonTransProfileLength > pktDecodeRemaining(pkt)) {
		WL_ERROR(("length exceeds packet %d > %d\n",
			list->nonTransProfileLength, pktDecodeRemaining(pkt)));
		return FALSE;
	}
	if (list->nonTransProfileLength > 0 &&
		!pktDecodeBytes(pkt, list->nonTransProfileLength, list->nonTransProfile)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}

	/* decode legacy OSU SSID */
	if (!pktDecodeByte(pkt, &list->legacyOsuSsidLength)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	len = list->legacyOsuSsidLength;
	if (len > MAX_LEGACY_OSU_SSID_LENGTH) {
		WL_ERROR(("length exceeds maximum %d > %d\n",
			list->legacyOsuSsidLength, MAX_LEGACY_OSU_SSID_LENGTH));
		return FALSE;
	}
	if (list->legacyOsuSsidLength > pktDecodeRemaining(pkt)) {
		WL_ERROR(("length exceeds packet %d > %d\n",
			list->legacyOsuSsidLength, pktDecodeRemaining(pkt)));
		return FALSE;
	}
	if (list->legacyOsuSsidLength > 0 &&
		!pktDecodeBytes(pkt, list->legacyOsuSsidLength, list->legacyOsuSsid)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}
	list->legacyOsuSsid[list->legacyOsuSsidLength] = 0;

	while (pktDecodeRemaining(pkt) > 0 &&
		list->osuProviderCount < MAX_OSU_PROVIDER) {
		pktHspotAnqpOsuProviderT *osu = &list->osuProvider[list->osuProviderCount];
		uint16 osuOffset;
		uint16 osuLength;
		uint16 nameLength;
		int remNameLength;
		uint16 iconLength;
		int remIconLength;
		int remDescLength;

		osuOffset = pktDecodeOffset(pkt);

		/* decode OSU provider length */
		if (!pktDecodeLe16(pkt, &osuLength)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (osuLength > pktDecodeRemaining(pkt)) {
			WL_ERROR(("OSU length exceeds packet %d > %d\n",
				osuLength, pktDecodeRemaining(pkt)));
			return FALSE;
		}

		if (pktDecodeRemaining(pkt) == 0) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		/* get length from friendly name duple */
		nameLength = *pktDecodeCurrentPtr(pkt) + 1;
		if (nameLength > pktDecodeRemaining(pkt)) {
			WL_ERROR(("name length exceeds packet %d > %d\n",
				nameLength, pktDecodeRemaining(pkt)));
			return FALSE;
		}
		remNameLength = nameLength;
		while (remNameLength > 0 &&
			osu->name.numName < MAX_OPERATOR_NAME) {
			int startOffset = pktDecodeOffset(pkt);

			if (!pktDecodeHspotAnqpOperatorNameDuple(pkt,
				&osu->name.duple[osu->name.numName])) {
				return FALSE;
			}
			else {
				osu->name.numName++;
			}

			/* update remaining name length */
			remNameLength -= pktDecodeOffset(pkt) - startOffset;
		}

		/* decode OSU server URI */
		if (!pktDecodeByte(pkt, &osu->uriLength)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (osu->uriLength > MAX_URI_LENGTH) {
			WL_ERROR(("URI exceeds buffer %d > %d\n",
				osu->uriLength, MAX_URI_LENGTH));
			return FALSE;
		}
		if (!pktDecodeBytes(pkt, osu->uriLength, (uint8 *)osu->uri)) {
			WL_ERROR(("pktDecodeBytes failed"));
			return FALSE;
		}
		osu->uri[osu->uriLength] = 0;

		/* decode OSU method */
		if (!pktDecodeByte(pkt, &osu->methodLength)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (osu->methodLength > MAX_METHOD_LENGTH) {
			WL_ERROR(("method exceeds buffer %d > %d\n",
				osu->methodLength, MAX_METHOD_LENGTH));
			return FALSE;
		}
		if (!pktDecodeBytes(pkt, osu->methodLength, (uint8 *)osu->method)) {
			WL_ERROR(("pktDecodeBytes failed"));
			return FALSE;
		}

		/* decode icon metadata */
		if (!pktDecodeLe16(pkt, &iconLength)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		remIconLength = iconLength;
		while (remIconLength > 0 &&
			osu->iconMetadataCount < MAX_ICON_METADATA_LENGTH) {
			int startOffset = pktDecodeOffset(pkt);

			if (!pktDecodeHspotAnqpIconMetadata(pkt,
				&osu->iconMetadata[osu->iconMetadataCount])) {
				return FALSE;
			}
			else {
				osu->iconMetadataCount++;
			}

			/* update remaining name length */
			remIconLength -= pktDecodeOffset(pkt) - startOffset;
		}

		/* decode OSU NAI */
		if (!pktDecodeByte(pkt, &osu->naiLength)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}
		if (osu->naiLength > MAX_NAI_LENGTH) {
			WL_ERROR(("NAI exceeds buffer %d > %d\n",
				osu->naiLength, MAX_NAI_LENGTH));
			return FALSE;
		}
		if (!pktDecodeBytes(pkt, osu->naiLength, (uint8 *)osu->nai)) {
			WL_ERROR(("pktDecodeBytes failed"));
			return FALSE;
		}
		osu->nai[osu->naiLength] = 0;

		/* decode OSU service description */
		remDescLength = osuLength - (pktDecodeOffset(pkt) - osuOffset);
		while (remDescLength > 0 &&
			osu->desc.numName < MAX_OPERATOR_NAME) {
			int startOffset = pktDecodeOffset(pkt);

			if (!pktDecodeHspotAnqpOperatorNameDuple(pkt,
				&osu->desc.duple[osu->desc.numName])) {
				return FALSE;
			}
			else {
				osu->desc.numName++;
			}

			/* update remaining name length */
			remDescLength -= pktDecodeOffset(pkt) - startOffset;
		}

		list->osuProviderCount++;
	}

	if (WL_P2PO_ON())
		pktDecodeHspotAnqpOsuProviderListPrint(list);
	return TRUE;
}

/* print decoded OSU provider list */
void pktDecodeHspotAnqpOsuProviderListPrint(pktHspotAnqpOsuProviderListT *list)
{
	int i, j;

	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot OSU provider list:\n"));
	WL_PRINT(("OSU provider count = %d\n", list->osuProviderCount));

	for (i = 0; i < list->osuProviderCount; i++) {
		pktHspotAnqpOsuProviderT *osu = &list->osuProvider[i];

		WL_PRINT(("\n"));
		for (j = 0; j < osu->name.numName; j++) {
			pktHspotAnqpNameDupleT *d = &osu->name.duple[j];
			WL_PRINT(("   language: %s\n", d->lang));
			WL_PRINT(("   provider name: %s\n", d->name));
		}
		WL_PRINT(("   URI: %s\n", osu->uri));
		for (j = 0; j < osu->methodLength; j++) {
			uint8 method = osu->method[j];
			WL_PRINT(("   method: %s\n",
				method == HSPOT_OSU_METHOD_OMA_DM ? "OMA-DM" :
				method == HSPOT_OSU_METHOD_SOAP_XML ? "SOAP-XML" :
				"invalid"));
		}
		WL_PRINT(("   icons: %d\n", osu->iconMetadataCount));
		for (j = 0; j < osu->iconMetadataCount; j++) {
			pktHspotAnqpIconMetadataT *icon = &osu->iconMetadata[j];
			WL_PRINT(("      width=%d height=%d type=%s filename=%s\n",
				icon->width, icon->height, icon->type, icon->filename));
		}
		WL_PRINT(("   NAI: %s\n", osu->nai));
		for (j = 0; j < osu->desc.numName; j++) {
			pktHspotAnqpNameDupleT *d = &osu->desc.duple[j];
			WL_PRINT(("   language: %s\n", d->lang));
			WL_PRINT(("   description: %s\n", d->name));
		}
	}
}

/* search decoded OSU provider list for specified provider */
pktHspotAnqpOsuProviderT *pktDecodeHspotAnqpFindOsuProvider(
	pktHspotAnqpOsuProviderListT *list, int length, char *provider)
{
	int i;

	for (i = 0; i < list->osuProviderCount; i++) {
		pktHspotAnqpOsuProviderT *p = &list->osuProvider[i];
		int j;

		for (j = 0; j < p->name.numName; j++) {
			pktHspotAnqpNameDupleT *d = &p->name.duple[j];
			if (d->nameLen == length &&
				memcmp(d->name, provider, d->nameLen) == 0) {
				return p;
			}
		}
	}

	return 0;
}

/* decode anonymous NAI */
int pktDecodeHspotAnqpAnonymousNai(pktDecodeT *pkt, pktHspotAnqpAnonymousNaiT *anonymous)
{
	WL_PRPKT("packet for hotspot anonymous NAI decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(anonymous, 0, sizeof(*anonymous));

	if (!pktDecodeLe16(pkt, &anonymous->naiLen)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (anonymous->naiLen > pktDecodeRemaining(pkt)) {
		WL_ERROR(("anonymous NAI length exceeds packet %d > %d\n",
			anonymous->naiLen, pktDecodeRemaining(pkt)));
		return FALSE;
	}

	if (anonymous->naiLen > MAX_NAI_SIZE) {
		WL_ERROR(("anonymous NAI exceeds buffer %d > %d\n",
			anonymous->naiLen, MAX_NAI_SIZE));
		return FALSE;
	}

	if (!pktDecodeBytes(pkt, anonymous->naiLen, (uint8 *)anonymous->nai)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}

	anonymous->nai[anonymous->naiLen] = 0;
	if (WL_P2PO_ON())
		pktDecodeHspotAnqpAnonymousNaiPrint(anonymous);
	return TRUE;
}

/* print decoded anonymous NAI */
void pktDecodeHspotAnqpAnonymousNaiPrint(pktHspotAnqpAnonymousNaiT *anonymous)
{
	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot anonymous NAI:\n"));
	WL_PRINT(("   %s\n", anonymous->nai));
}

/* decode icon binary file */
int pktDecodeHspotAnqpIconBinaryFile(pktDecodeT *pkt, pktHspotAnqpIconBinaryFileT *icon)
{
	WL_PRPKT("packet for hotspot icon binary file decoding",
		pktDecodeCurrentPtr(pkt), pktDecodeRemaining(pkt));

	memset(icon, 0, sizeof(*icon));

	if (!pktDecodeByte(pkt, &icon->status)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &icon->typeLength)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (icon->typeLength > pktDecodeRemaining(pkt)) {
		WL_ERROR(("icon type length exceeds packet %d > %d\n",
			icon->typeLength, pktDecodeRemaining(pkt)));
		return FALSE;
	}
	if (icon->typeLength > MAX_ICON_TYPE_LENGTH) {
		WL_ERROR(("icon type exceeds buffer %d > %d\n",
			icon->typeLength, MAX_ICON_TYPE_LENGTH));
		return FALSE;
	}
	if (!pktDecodeBytes(pkt, icon->typeLength, (uint8 *)icon->type)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}
	icon->type[icon->typeLength] = 0;

	if (!pktDecodeLe16(pkt, &icon->binaryLength)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (icon->binaryLength > pktDecodeRemaining(pkt)) {
		WL_ERROR(("icon binary length exceeds packet %d > %d\n",
			icon->binaryLength, pktDecodeRemaining(pkt)));
		return FALSE;
	}
	if (icon->binaryLength > MAX_ICON_BINARY_SIZE) {
		WL_ERROR(("icon binary exceeds buffer %d > %d\n",
			icon->binaryLength, MAX_ICON_BINARY_SIZE));
		return FALSE;
	}
	if (!pktDecodeBytes(pkt, icon->binaryLength, (uint8 *)icon->binary)) {
		WL_ERROR(("pktDecodeBytes failed"));
		return FALSE;
	}

	if (WL_P2PO_ON())
		pktDecodeHspotAnqpIconBinaryFilePrint(icon);
	return TRUE;
}

/* print decoded icon binary file */
void pktDecodeHspotAnqpIconBinaryFilePrint(pktHspotAnqpIconBinaryFileT *icon)
{
	WL_PRINT(("----------------------------------------\n"));
	WL_PRINT(("decoded hotspot icon binary file:\n"));
	WL_PRINT(("   status: %s\n",
		icon->status == HSPOT_ICON_STATUS_SUCCESS ? "success" :
		icon->status == HSPOT_ICON_STATUS_FILE_NOT_FOUND ? "file not found" :
		icon->status == HSPOT_ICON_STATUS_UNSPECIFIED_FILE_ERROR ?
		"unspecified file error" : "invalid"));
	WL_PRPKT("   type",
		(uint8 *)icon->type, icon->typeLength);
	WL_PRPKT("   binary",
		(uint8 *)icon->binary, icon->binaryLength);
}
