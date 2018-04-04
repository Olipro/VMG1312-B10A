/*
 * Encoding of WNM packets.
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
#include "pktEncodeWnm.h"

/* encode WNM-notification request for subscription remediation */
int pktEncodeWnmSubscriptionRemediation(pktEncodeT *pkt,
	uint8 dialogToken, uint16 urlLen, char *url)
{
	int initLen = pktEncodeLength(pkt);

	pktEncodeByte(pkt, DOT11_ACTION_CAT_WNM);
	pktEncodeByte(pkt, DOT11_WNM_ACTION_NOTFCTN_REQ);
	pktEncodeByte(pkt, dialogToken);
	pktEncodeByte(pkt, HSPOT_WNM_TYPE);
	pktEncodeByte(pkt, DOT11_MNG_VS_ID);
	pktEncodeByte(pkt, 6 + urlLen);
	pktEncodeBytes(pkt, WFA_OUI_LEN, (uint8 *)WFA_OUI);
	pktEncodeByte(pkt, HSPOT_WNM_OUI_TYPE);
	pktEncodeByte(pkt, HSPOT_SUBTYPE_SUBSCRIPTION_REMEDIATION);
	pktEncodeByte(pkt, urlLen);
	if (urlLen > 0) {
		pktEncodeBytes(pkt, urlLen, (uint8 *)url);
	}

	return pktEncodeLength(pkt) - initLen;
}
