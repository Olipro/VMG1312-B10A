/*
 * Decoding of WNM packets.
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
#include "pktHspot.h"
#include "pktDecodeWnm.h"

/* decode WNM-notification request for subscription remediation */
int pktDecodeWnmSubscriptionRemediation(pktDecodeT *pkt, pktWnmDecodeT *wnm)
{
	uint8 byte, len;
	uint8 oui[WFA_OUI_LEN];

	WL_PRPKT("packet for WNM subscription remediation decoding",
		pktDecodeBuf(pkt), pktDecodeBufLength(pkt));

	memset(wnm, 0, sizeof(*wnm));

	if (!pktDecodeByte(pkt, &byte) || byte != DOT11_ACTION_CAT_WNM) {
		WL_ERROR(("WNM action category\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &byte) || byte != DOT11_WNM_ACTION_NOTFCTN_REQ) {
		WL_ERROR(("WNM notifcation request\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &wnm->dialogToken)) {
		WL_ERROR(("dialog token\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &byte) || byte != HSPOT_WNM_TYPE) {
		WL_ERROR(("WNM type\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &byte) || byte != DOT11_MNG_VS_ID) {
		WL_ERROR(("vendor specific ID\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &len) || len < 6) {
		WL_ERROR(("length\n"));
		return FALSE;
	}
	if (len > pktDecodeRemaining(pkt)) {
		WL_ERROR(("length exceeds packet %d > %d\n",
			len, pktDecodeRemaining(pkt)));
		return FALSE;
	}
	if (!pktDecodeBytes(pkt, WFA_OUI_LEN, oui) ||
		memcmp(oui, WFA_OUI, WFA_OUI_LEN) != 0) {
		WL_ERROR(("WFA OUI\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &byte) || byte != HSPOT_WNM_OUI_TYPE) {
		WL_ERROR(("hotspot WNM OUI type\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &byte) || byte != HSPOT_SUBTYPE_SUBSCRIPTION_REMEDIATION) {
		WL_ERROR(("subscription remediation subtype\n"));
		return FALSE;
	}
	if (!pktDecodeByte(pkt, &wnm->urlLength) ||
		wnm->urlLength > pktDecodeRemaining(pkt)) {
		WL_ERROR(("URL length\n"));
		return FALSE;
	}
	if (wnm->urlLength > 0) {
		pktDecodeBytes(pkt, wnm->urlLength, (uint8 *)wnm->url);
	}
	wnm->url[wnm->urlLength] = 0;

	return TRUE;
}
