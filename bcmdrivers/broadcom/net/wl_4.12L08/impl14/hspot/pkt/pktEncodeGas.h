/*
 * Encoding of 802.11u GAS packets.
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

#ifndef _PKTENCODEGAS_H_
#define _PKTENCODEGAS_H_

#include "typedefs.h"
#include "pktEncode.h"

/* encode GAS request */
int pktEncodeGasRequest(pktEncodeT *pkt, uint8 dialogToken,
	uint8 apieLen, uint8 *apie, uint16 reqLen, uint8 *req);

/* encode GAS response */
int pktEncodeGasResponse(pktEncodeT *pkt, uint8 dialogToken,
	uint16 statusCode, uint16 comebackDelay, uint8 apieLen, uint8 *apie,
	uint16 rspLen, uint8 *rsp);

/* encode GAS comeback request */
int pktEncodeGasComebackRequest(pktEncodeT *pkt, uint8 dialogToken);

/* encode GAS response */
int pktEncodeGasComebackResponse(pktEncodeT *pkt, uint8 dialogToken,
	uint16 statusCode, uint8 fragmentId, uint16 comebackDelay,
	uint8 apieLen, uint8 *apie, uint16 rspLen, uint8 *rsp);

#endif /* _PKTENCODEGAS_H_ */
