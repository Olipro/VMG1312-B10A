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

#ifndef _PKTDECODEHSPOTANQP_H_
#define _PKTDECODEHSPOTANQP_H_

#include "proto/802.11.h"
#include "typedefs.h"
#include "pktDecode.h"
#include "pktHspot.h"

typedef struct {
	int queryListLength;
	uint8 *queryListBuffer;
	int capabilityListLength;
	uint8 *capabilityListBuffer;
	int operatorFriendlyNameLength;
	uint8 *operatorFriendlyNameBuffer;
	int wanMetricsLength;
	uint8 *wanMetricsBuffer;
	int connectionCapabilityLength;
	uint8 *connectionCapabilityBuffer;
	int naiHomeRealmQueryLength;
	uint8 *naiHomeRealmQueryBuffer;
	int opClassIndicationLength;
	uint8 *opClassIndicationBuffer;
	int onlineSignupProvidersLength;
	uint8 *onlineSignupProvidersBuffer;
	int anonymousNaiLength;
	uint8 *anonymousNaiBuffer;
	int iconRequestLength;
	uint8 *iconRequestBuffer;
	int iconBinaryFileLength;
	uint8 *iconBinaryFileBuffer;
} pktDecodeHspotAnqpT;

/* print decoded hotspot ANQP */
void printHspotAnqpDecode(pktDecodeHspotAnqpT *hspot);

/* decode hotspot ANQP frame */
int pktDecodeHspotAnqp(pktDecodeT *pkt, int isReset, pktDecodeHspotAnqpT *hspot);

#define MAX_LIST_SIZE	16
typedef struct
{
	uint16 queryLen;
	uint8 queryId[MAX_LIST_SIZE];
} pktHspotAnqpQueryListT;

/* decode query list */
int pktDecodeHspotAnqpQueryList(pktDecodeT *pkt, pktHspotAnqpQueryListT *queryList);

/* print decoded query list */
void pktDecodeHspotAnqpQueryListPrint(pktHspotAnqpQueryListT *queryList);

typedef struct
{
	uint16 capLen;
	uint8 capId[MAX_LIST_SIZE];
} pktHspotAnqpCapabilityListT;

/* decode capability list */
int pktDecodeHspotAnqpCapabilityList(pktDecodeT *pkt, pktHspotAnqpCapabilityListT *capList);

/* print decoded capability list */
void pktDecodeHspotAnqpCapabilityListPrint(pktHspotAnqpCapabilityListT *capList);

typedef struct {
	uint8 langLen;
	char lang[VENUE_LANGUAGE_CODE_SIZE + 1];	/* null terminated */
	uint8 nameLen;
	char name[VENUE_NAME_SIZE + 1];		/* null terminated */
} pktHspotAnqpNameDupleT;

#define MAX_OPERATOR_NAME	4
typedef struct {
	int numName;
	pktHspotAnqpNameDupleT duple[MAX_OPERATOR_NAME];
} pktHspotAnqpOperatorFriendlyNameT;

/* decode operator friendly name */
int pktDecodeHspotAnqpOperatorFriendlyName(pktDecodeT *pkt, pktHspotAnqpOperatorFriendlyNameT *op);

/* print decoded operator friendly name */
void pktDecodeHspotAnqpOperatorFriendlyNamePrint(pktHspotAnqpOperatorFriendlyNameT *op);

typedef struct {
	uint8 linkStatus;
	uint8 symmetricLink;
	uint8 atCapacity;
	uint32 dlinkSpeed;
	uint32 ulinkSpeed;
	uint8 dlinkLoad;
	uint8 ulinkLoad;
	uint16 lmd;
} pktHspotAnqpWanMetricsT;

/* decode WAN metrics */
int pktDecodeHspotAnqpWanMetrics(pktDecodeT *pkt, pktHspotAnqpWanMetricsT *wanMetrics);

/* print decoded WAN metrics */
void pktDecodeHspotAnqpWanMetricsPrint(pktHspotAnqpWanMetricsT *wanMetrics);

typedef struct {
	uint8 ipProtocol;
	uint16 portNumber;
	uint8 status;
} pktHspotAnqpProtoPortT;

#define MAX_CONNECTION_CAPABILITY	16
typedef struct {
	int numConnectCap;
	pktHspotAnqpProtoPortT tuple[MAX_CONNECTION_CAPABILITY];
} pktHspotAnqpConnectionCapabilityT;

/* decode connection capability */
int pktDecodeHspotAnqpConnectionCapability(pktDecodeT *pkt, pktHspotAnqpConnectionCapabilityT *cap);

/* print decoded connection capability */
void pktDecodeHspotAnqpConnectionCapabilityPrint(pktHspotAnqpConnectionCapabilityT *cap);

typedef struct {
	uint8 encoding;
	uint8 nameLen;
	char name[VENUE_NAME_SIZE + 1];		/* null terminated */
} pktHspotAnqpNaiHomeRealmDataT;

#define MAX_HOME_REALM	16
typedef struct {
	uint8 count;
	pktHspotAnqpNaiHomeRealmDataT data[MAX_HOME_REALM];
} pktHspotAnqpNaiHomeRealmQueryT;

/* decode NAI home realm query */
int pktDecodeHspotAnqpNaiHomeRealmQuery(pktDecodeT *pkt, pktHspotAnqpNaiHomeRealmQueryT *realm);

/* print decoded home realm query */
void pktDecodeHspotAnqpNaiHomeRealmQueryPrint(pktHspotAnqpNaiHomeRealmQueryT *realm);

#define MAX_OPCLASS_LIST_SIZE	255
typedef struct
{
	uint16 opClassLen;
	uint8 opClass[MAX_OPCLASS_LIST_SIZE];
} pktHspotAnqpOperatingClassIndicationT;

/* decode operating class indication */
int pktDecodeHspotAnqpOperatingClassIndication(pktDecodeT *pkt,
	pktHspotAnqpOperatingClassIndicationT *opClassList);

/* print decoded operating class indication */
void pktDecodeHspotAnqpOperatingClassIndicationPrint(
	pktHspotAnqpOperatingClassIndicationT *opClassList);

#define MAX_ICON_TYPE_LENGTH		128
#define MAX_ICON_FILENAME_LENGTH	128
typedef struct
{
	uint16 width;
	uint16 height;
	char lang[VENUE_LANGUAGE_CODE_SIZE + 1];	/* null terminated */
	uint8 typeLength;
	uint8 type[MAX_ICON_TYPE_LENGTH + 1];	/* null terminated */
	uint8 filenameLength;
	uint8 filename[MAX_ICON_FILENAME_LENGTH + 1];	/* null terminated */
} pktHspotAnqpIconMetadataT;

#define MAX_NAI_LENGTH				128
#define MAX_METHOD_LENGTH			2
#define MAX_ICON_METADATA_LENGTH	8
#define MAX_URI_LENGTH				128
typedef struct
{
	pktHspotAnqpOperatorFriendlyNameT name;
	uint8 uriLength;
	uint8 uri[MAX_URI_LENGTH + 1];	/* null terminated */
	uint8 methodLength;
	uint8 method[MAX_METHOD_LENGTH];
	int iconMetadataCount;
	pktHspotAnqpIconMetadataT iconMetadata[MAX_ICON_METADATA_LENGTH];
	uint8 naiLength;
	uint8 nai[MAX_NAI_LENGTH + 1];	/* null terminated */
	pktHspotAnqpOperatorFriendlyNameT desc;
} pktHspotAnqpOsuProviderT;

#define MAX_NON_TRANS_PROFILE_LENGTH	256
#define MAX_LEGACY_OSU_SSID_LENGTH		255
#define MAX_OSU_PROVIDER				16
typedef struct
{
	uint16 nonTransProfileLength;
	uint8 nonTransProfile[MAX_NON_TRANS_PROFILE_LENGTH];
	uint8 legacyOsuSsidLength;
	uint8 legacyOsuSsid[MAX_LEGACY_OSU_SSID_LENGTH + 1];	/* null terminated */
	uint16 osuProviderCount;
	pktHspotAnqpOsuProviderT osuProvider[MAX_OSU_PROVIDER];
} pktHspotAnqpOsuProviderListT;

/* decode OSU provider list */
int pktDecodeHspotAnqpOsuProviderList(pktDecodeT *pkt, pktHspotAnqpOsuProviderListT *list);

/* print decoded OSU provider list */
void pktDecodeHspotAnqpOsuProviderListPrint(pktHspotAnqpOsuProviderListT *list);

/* search decoded OSU provider list for specified provider */
pktHspotAnqpOsuProviderT *pktDecodeHspotAnqpFindOsuProvider(
	pktHspotAnqpOsuProviderListT *list, int length, char *provider);

#define MAX_NAI_SIZE	255
typedef struct {
	uint16 naiLen;
	char nai[MAX_NAI_SIZE + 1];		/* null terminated */
} pktHspotAnqpAnonymousNaiT;

/* decode anonymous NAI */
int pktDecodeHspotAnqpAnonymousNai(pktDecodeT *pkt, pktHspotAnqpAnonymousNaiT *anonymous);

/* print decoded anonymous NAI */
void pktDecodeHspotAnqpAnonymousNaiPrint(pktHspotAnqpAnonymousNaiT *anonymous);

#define MAX_ICON_BINARY_SIZE	4096
typedef struct {
	uint8 status;
	uint8 typeLength;
	uint8 type[MAX_ICON_TYPE_LENGTH + 1];	/* null terminated */
	uint16 binaryLength;
	char binary[MAX_ICON_BINARY_SIZE];
} pktHspotAnqpIconBinaryFileT;

/* decode icon binary file */
int pktDecodeHspotAnqpIconBinaryFile(pktDecodeT *pkt, pktHspotAnqpIconBinaryFileT *icon);

/* print decoded icon binary file */
void pktDecodeHspotAnqpIconBinaryFilePrint(pktHspotAnqpIconBinaryFileT *icon);

#endif /* _PKTDECODEHSPOTANQP_H_ */
