/*
 * Encoding base functions.
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

#ifndef _PKTENCODE_H_
#define _PKTENCODE_H_

#include "typedefs.h"

typedef struct
{
	int maxLength;
	int length;
	uint8 *buf;
} pktEncodeT;

#define pktEncodeLength(pkt)	((pkt)->length)
#define pktEncodeBuf(pkt)		((pkt)->buf)

/* initialize pkt encode buffer */
int pktEncodeInit(pktEncodeT *pkt, int maxLength, uint8 *buf);

/* encode byte */
int pktEncodeByte(pktEncodeT *pkt, uint8 byte);

/* encode 16-bit big endian */
int pktEncodeBe16(pktEncodeT *pkt, uint16 value);

/* encode 32-bit big endian */
int pktEncodeBe32(pktEncodeT *pkt, uint32 value);

/* encode 16-bit little endian */
int pktEncodeLe16(pktEncodeT *pkt, uint16 value);

/* encode 32-bit little endian */
int pktEncodeLe32(pktEncodeT *pkt, uint32 value);

/* encode bytes */
int pktEncodeBytes(pktEncodeT *pkt, int length, uint8 *bytes);

#endif /* _PKTENCODE_H_ */
