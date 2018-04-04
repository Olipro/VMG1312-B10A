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

#ifndef _PKTDECODEWNM_H_
#define _PKTDECODEWNM_H_

#include "typedefs.h"
#include "pktDecode.h"

#define MAX_SERVER_URL_LENGTH	255
typedef struct
{
	uint8 dialogToken;
	uint8 urlLength;
	char url[MAX_SERVER_URL_LENGTH + 1];	/* null terminated */
} pktWnmDecodeT;

/* decode WNM-notification request for subscription remediation */
int pktDecodeWnmSubscriptionRemediation(pktDecodeT *pkt, pktWnmDecodeT *wnm);

#endif /* _PKTDECODEWNM_H_ */
