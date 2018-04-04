/*
 * Decoding of 802.11u GAS packets.
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

#ifndef _PKTDECODEGAS_H_
#define _PKTDECODEGAS_H_

#include "typedefs.h"
#include "pktDecode.h"
#include "pktDecodeIe.h"

typedef struct {
	pktAdvertisementProtocolTupleT apie;
	uint16 reqLen;
	uint8 *req;
} pktGasRequestT;

typedef struct {
	uint16 statusCode;
	uint16 comebackDelay;
	pktAdvertisementProtocolTupleT apie;
	uint16 rspLen;
	uint8 *rsp;
} pktGasResponseT;

typedef struct {
	uint16 statusCode;
	uint8 fragmentId;
	uint16 comebackDelay;
	pktAdvertisementProtocolTupleT apie;
	uint16 rspLen;
	uint8 *rsp;
} pktGasComebackResponseT;

typedef struct {
	uint8 category;
	uint8 action;
	uint8 dialogToken;
	union {
		pktGasRequestT request;
		pktGasResponseT response;
		/* none for comeback request */
		pktGasComebackResponseT comebackResponse;
	};
} pktGasDecodeT;

/* decode GAS frame */
int pktDecodeGas(pktDecodeT *pkt, pktGasDecodeT *gasDecode);

#endif /* _PKTDECODEGAS_H_ */
