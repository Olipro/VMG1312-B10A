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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "proto/802.11.h"
#include "proto/p2p.h"
#include "wlioctl.h"
#include "trace.h"
#include "pktDecodeWfd.h"
#include "pktDecodeIe.h"

static void printIes(pktIeT *ie)
{
#if !defined(BCMDBG)
	(void)ie;
#endif
	WL_TRACE(("decoded IEs:\n"));

	WL_PRUSR("   DS",
		ie->ds, ie->dsLength);
	WL_PRUSR("   BSS load",
		ie->bssLoad, ie->bssLoadLength);
	WL_PRUSR("   time advertise",
		ie->timeAdvertisement, ie->timeAdvertisementLength);
	WL_PRUSR("   time zone",
		ie->timeZone, ie->timeZoneLength);
	WL_PRUSR("   interworking",
		ie->interworking, ie->interworkingLength);
	WL_PRUSR("   advertisement protocol",
		ie->advertisementProtocol, ie->advertisementProtocolLength);
	WL_PRUSR("   expedited bandwidth request",
		ie->expeditedBandwidthRequest, ie->expeditedBandwidthRequestLength);
	WL_PRUSR("   QOS map set",
		ie->qosMapSet, ie->qosMapSetLength);
	WL_PRUSR("   roaming consortium",
		ie->roamingConsortium, ie->roamingConsortiumLength);
	WL_PRUSR("   emergency alert",
		ie->emergencyAlert, ie->emergencyAlertLength);
	WL_PRUSR("   extended capability",
		ie->extendedCapability, ie->extendedCapabilityLength);
	WL_PRUSR("   hotspot indication",
		ie->hotspotIndication, ie->hotspotIndicationLength);
	WL_PRUSR("   WPS vendor specific",
		ie->wpsIe, ie->wpsIeLength);
	WL_PRUSR("   WFD vendor specific",
		ie->wfdIe, ie->wfdIeLength);
}

/* decode IE */
int pktDecodeIe(pktDecodeT *pkt, pktIeT *ie)
{
	int nextIeOffset = 0;
	int ieCount = 0;

	WL_PRUSR("packet for IE decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(ie, 0, sizeof(*ie));

	while (nextIeOffset < pktDecodeBufLength(pkt)) {
		uint8 id;
		uint8 length;
		int dataLength;
		uint8 *dataPtr;

		pktDecodeOffset(pkt) = nextIeOffset;
		WL_TRACE(("decoding offset 0x%x\n", pktDecodeOffset(pkt)));

		/* minimum ID and length */
		if (pktDecodeRemaining(pkt) < 2) {
			WL_P2PO(("ID and length too short\n"));
			break;
		}

		pktDecodeByte(pkt, &id);
		pktDecodeByte(pkt, &length);

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
		case DOT11_MNG_DS_PARMS_ID:
			ie->dsLength = dataLength;
			ie->ds = dataPtr;
			break;
		case DOT11_MNG_QBSS_LOAD_ID:
			ie->bssLoadLength = dataLength;
			ie->bssLoad = dataPtr;
			break;
		case DOT11_MNG_TIME_ADVERTISE_ID:
			ie->timeAdvertisementLength = dataLength;
			ie->timeAdvertisement = dataPtr;
			break;
		case DOT11_MNG_TIME_ZONE_ID:
			ie->timeZoneLength = dataLength;
			ie->timeZone = dataPtr;
			break;
		case DOT11_MNG_INTERWORKING_ID:
			ie->interworkingLength = dataLength;
			ie->interworking = dataPtr;
			break;
		case DOT11_MNG_ADVERTISEMENT_ID:
			ie->advertisementProtocolLength = dataLength;
			ie->advertisementProtocol = dataPtr;
			break;
		case DOT11_MNG_EXP_BW_REQ_ID:
			ie->expeditedBandwidthRequestLength = dataLength;
			ie->expeditedBandwidthRequest = dataPtr;
			break;
		case DOT11_MNG_QOS_MAP_ID:
			ie->qosMapSetLength = dataLength;
			ie->qosMapSet = dataPtr;
			break;
		case DOT11_MNG_ROAM_CONSORT_ID:
			ie->roamingConsortiumLength = dataLength;
			ie->roamingConsortium = dataPtr;
			break;
		case DOT11_MNG_EMERGCY_ALERT_ID:
			ie->emergencyAlertLength = dataLength;
			ie->emergencyAlert = dataPtr;
			break;
		case DOT11_MNG_EXT_CAP_ID:
			ie->extendedCapabilityLength = dataLength;
			ie->extendedCapability = dataPtr;
			break;
		case DOT11_MNG_VS_ID:
			if (dataLength >= WFA_OUI_LEN + 1 &&
				memcmp(dataPtr, WFA_OUI, WFA_OUI_LEN) == 0 &&
				dataPtr[WFA_OUI_LEN] == HSPOT_IE_OUI_TYPE) {
				ie->hotspotIndicationLength = dataLength;
				ie->hotspotIndication = dataPtr;
			}
			else if (dataLength >= WPS_OUI_LEN + 1 &&
				memcmp(dataPtr, WPS_OUI, WPS_OUI_LEN) == 0 &&
				dataPtr[WPS_OUI_LEN] == WPS_OUI_TYPE) {
				ie->wpsIeLength = dataLength;
				ie->wpsIe = dataPtr;
			}
			else if (dataLength >= WFA_OUI_LEN + 1 &&
				memcmp(dataPtr, WFA_OUI, WFA_OUI_LEN) == 0 &&
				dataPtr[WFA_OUI_LEN] == WFA_OUI_TYPE_P2P) {
				ie->wfdIeLength = dataLength;
				ie->wfdIe = dataPtr;
			}
			break;
		default:
			continue;
			break;
		}

		/* count IEs decoded */
		ieCount++;
	}

	if (ieCount > 0)
		printIes(ie);

	return ieCount;
}

/* decode hotspot 2.0 indication */
int pktDecodeIeHotspotIndication(pktDecodeT *pkt, uint8 *hotspotConfig)
{
	uint8 oui[WFA_OUI_LEN];
	uint8 type;

	WL_PRUSR("packet for hotspot indication decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	*hotspotConfig = 0;

	/* check OUI */
	if (!pktDecodeBytes(pkt, WFA_OUI_LEN, oui) ||
		memcmp(oui, WFA_OUI, WFA_OUI_LEN) != 0) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	/* check type */
	if (!pktDecodeByte(pkt, &type) || type != HSPOT_IE_OUI_TYPE) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	/* hotspot configuration */
	if (!pktDecodeByte(pkt, hotspotConfig)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* decode hotspot 2.0 indication release2 */
int pktDecodeIeHotspotIndication2(pktDecodeT *pkt, pktHotspotIndicationT *hotspot)
{
	uint8 config;

	if (!pktDecodeIeHotspotIndication(pkt, &config))
		return FALSE;

	memset(hotspot, 0, sizeof(*hotspot));
	if (config & HSPOT_DGAF_DISABLED_MASK)
		hotspot->isDgafDisabled = TRUE;
	if (config & HSPOT_OSU_BSSID_MASK)
		hotspot->isOsuBssid = TRUE;
	hotspot->releaseNumber =
		(config & HSPOT_RELEASE_MASK) >> HSPOT_RELEASE_SHIFT;
	return TRUE;
}

/* decode interworking */
int pktDecodeIeInterworking(pktDecodeT *pkt, pktInterworkingT *interworking)
{
	uint8 options;

	WL_PRUSR("packet for interworking decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(interworking, 0, sizeof(*interworking));

	if (!pktDecodeByte(pkt, &options)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	interworking->accessNetworkType = options & IW_ANT_MASK;
	if (options & IW_INTERNET_MASK)
		interworking->isInternet = TRUE;
	if (options & IW_ASRA_MASK)
		interworking->isAsra = TRUE;
	if (options & IW_ESR_MASK)
		interworking->isEsr = TRUE;
	if (options & IW_UESA_MASK)
		interworking->isUesa = TRUE;

	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (!pktDecodeByte(pkt, &interworking->venueGroup)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &interworking->venueType)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	interworking->isVenue = TRUE;

	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (!pktDecodeBytes(pkt, sizeof(interworking->hessid), (uint8 *)&interworking->hessid)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	interworking->isHessid = TRUE;

	return TRUE;
}

/* decode advertisement protocol tuple */
int pktDecodeIeAdvertisementProtocolTuple(pktDecodeT *pkt,
	pktAdvertisementProtocolTupleT *tuple)
{
	uint8 info;

	memset(tuple, 0, sizeof(*tuple));

	if (!pktDecodeByte(pkt, &info)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	tuple->queryResponseLimit = info & ADVP_QRL_MASK;
	if (info & ADVP_PAME_BI_MASK)
		tuple->isPamebi = TRUE;

	if (!pktDecodeByte(pkt, &tuple->protocolId)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* decode advertisement protocol */
int pktDecodeIeAdvertisementProtocol(pktDecodeT *pkt,
	pktAdvertisementProtocolT *advertise)
{
	WL_PRUSR("packet for advertisement protocol decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(advertise, 0, sizeof(*advertise));

	while (pktDecodeRemaining(pkt) > 0 &&
		advertise->count < MAX_ADVERTISEMENT_PROTOCOL) {
		pktAdvertisementProtocolTupleT *tuple =
			&advertise->protocol[advertise->count];

		if (!pktDecodeIeAdvertisementProtocolTuple(pkt, tuple)) {
			WL_ERROR(("decode error\n"));
			return FALSE;
		}

		advertise->count++;
	}

	return TRUE;
}

/* decode roaming consortium */
int pktDecodeIeRoamingConsortium(pktDecodeT *pkt, pktRoamingConsortiumT *roam)
{
	uint8 oiLengths;
	uint8 oiLen1, oiLen2;
	pktOiT *oi;

	WL_PRUSR("packet for roaming consortium decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(roam, 0, sizeof(*roam));

	if (!pktDecodeByte(pkt, &roam->anqpOiCount)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &oiLengths)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	oiLen1 = oiLengths & 0x0f;
	oiLen2 = oiLengths >> 4;

	if (oiLen1 == 0 && oiLen2 == 0)
		return TRUE;

	if (pktDecodeRemaining(pkt) < oiLen1 + oiLen2) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	oi = &roam->oi[0];
	oi->length = oiLen1;
	if (!pktDecodeBytes(pkt, oi->length, oi->data)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	roam->count++;

	if (oiLen2 == 0)
		return TRUE;

	oi = &roam->oi[1];
	oi->length = oiLen2;
	if (!pktDecodeBytes(pkt, oi->length, oi->data)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	roam->count++;

	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (pktDecodeRemaining(pkt) > MAX_IE_OI_LENGTH) {
		WL_ERROR(("OI #3 length %d > %d\n",
			pktDecodeRemaining(pkt), MAX_IE_OI_LENGTH));
		return FALSE;
	}

	oi = &roam->oi[3];
	oi->length = pktDecodeRemaining(pkt);
	if (!pktDecodeBytes(pkt, oi->length, oi->data)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}
	roam->count++;

	return TRUE;
}

/* decode extended capabilities */
int pktDecodeIeExtendedCapabilities(pktDecodeT *pkt, uint32 *cap)
{
	WL_PRUSR("packet for extended capabilities decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(cap, 0, sizeof(*cap));

	if (!pktDecodeLe32(pkt, cap)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* decode time advertisement */
int pktDecodeIeTimeAdvertisement(pktDecodeT *pkt, pktTimeAdvertisementT *time)
{
	WL_PRUSR("packet for time advertisement decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(time, 0, sizeof(*time));

	if (!pktDecodeByte(pkt, &time->capabilities)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (!pktDecodeLe16(pkt, &time->timeValue.year)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &time->timeValue.month)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &time->timeValue.day)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &time->timeValue.hours)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &time->timeValue.minutes)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &time->timeValue.seconds)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeLe16(pkt, &time->timeValue.milliseconds)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &time->timeValue.reserved)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (!pktDecodeBytes(pkt, sizeof(time->timeError), time->timeError)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (pktDecodeRemaining(pkt) == 0)
		return TRUE;

	if (!pktDecodeBytes(pkt, sizeof(time->timeUpdate), time->timeUpdate)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* decode time zone */
int pktDecodeIeTimeZone(pktDecodeT *pkt, pktTimeZoneT *zone)
{
	WL_PRUSR("packet for time zone decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(zone, 0, sizeof(*zone));
	pktDecodeBytes(pkt, pktDecodeBufLength(pkt), (uint8 *)zone);
	return TRUE;
}

/* decode BSS load */
int pktDecodeIeBssLoad(pktDecodeT *pkt, pktBssLoadT *load)
{
	WL_PRUSR("packet for BSS load decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(load, 0, sizeof(*load));

	if (!pktDecodeLe16(pkt, &load->stationCount)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeByte(pkt, &load->channelUtilization)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	if (!pktDecodeLe16(pkt, &load->availableAdmissionCapacity)) {
		WL_ERROR(("decode error\n"));
		return FALSE;
	}

	return TRUE;
}

/* decode probe response from wl_bss_info_t */
int pktDecodeIeProbeResponse(wl_bss_info_t *bi, pktProbeResponseT *pr)
{
	memset(pr, 0, sizeof(*pr));

	/* deocde probe responses only */
	if ((bi->flags & WL_BSS_FLAGS_FROM_BEACON) == 0) {
		uint8 *biData = (uint8 *)bi;
		pktDecodeT pkt;
		pktIeT ies;

		pktDecodeInit(&pkt, bi->ie_length, &biData[bi->ie_offset]);
		pktDecodeIe(&pkt, &ies);

		/* channel from DS IE if available */
		pr->channel = ies.dsLength == 1 ? *ies.ds :
			wf_chspec_ctlchan(bi->chanspec);

		if (ies.wfdIe != 0) {
			pktDecodeT dec1;
			pktDecodeWfdT wfd;

			pr->isWfd = TRUE;
			pktDecodeInit(&dec1, ies.wfdIeLength, ies.wfdIe);
			pktDecodeWfd(&dec1, &wfd);

			if (wfd.deviceInfoBuffer) {
				pktDecodeT dec2;

				pktDecodeInit(&dec2, wfd.deviceInfoLength,
					wfd.deviceInfoBuffer);
				pr->isWfdDeviceInfoDecoded =
					pktDecodeWfdDeviceInfo(&dec2, &pr->wfdDeviceInfo);
			}

			if (wfd.capabilityBuffer) {
				pktDecodeT dec3;

				pktDecodeInit(&dec3, wfd.capabilityLength,
					wfd.capabilityBuffer);
				pr->isWfdCapabilityDecoded =
					pktDecodeWfdCapability(&dec3, &pr->wfdCapability);
			}
		}

		if (ies.hotspotIndication != 0) {
			pktDecodeT dec4;

			pr->isWfd = TRUE;
			pktDecodeInit(&dec4, ies.hotspotIndicationLength,
				ies.hotspotIndication);
			pr->isHotspotDecoded =
				pktDecodeIeHotspotIndication(&dec4, &pr->hotspotConfig);
		}

		return TRUE;
	}
	return FALSE;
}
