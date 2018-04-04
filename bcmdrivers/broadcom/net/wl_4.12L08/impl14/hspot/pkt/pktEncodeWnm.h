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

#ifndef _PKTENCODEWNM_H_
#define _PKTENCODEWNM_H_

#include "typedefs.h"
#include "pktEncode.h"

/* encode WNM-notification request for subscription remediation */
int pktEncodeWnmSubscriptionRemediation(pktEncodeT *pkt,
	uint8 dialogToken, uint16 urlLen, char *url);

#endif /* _PKTENCODEWNM_H_ */
