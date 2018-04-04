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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "trace.h"
#include "pktEncode.h"

static int isLengthValid(pktEncodeT *pkt, int length)
{
	if (pkt == 0)
		return FALSE;

	if (pkt->buf == 0)
		return FALSE;

	if (pkt->length + length > pkt->maxLength) {
		WL_ERROR(("length %d exceeds remaining buffer %d\n",
			length, pkt->maxLength - pkt->length));
		return FALSE;
	}

	return TRUE;
}

/* initialize pkt encode buffer */
int pktEncodeInit(pktEncodeT *pkt, int maxLength, uint8 *buf)
{
	if (buf == 0)
		return FALSE;

	pkt->maxLength = maxLength;
	pkt->length = 0;
	pkt->buf = buf;

	return TRUE;
}

/* encode byte */
int pktEncodeByte(pktEncodeT *pkt, uint8 byte)
{
	if (!isLengthValid(pkt, 1))
		return 0;

	pkt->buf[pkt->length++] = byte;
	return 1;
}

/* encode 16-bit big endian */
int pktEncodeBe16(pktEncodeT *pkt, uint16 value)
{
	if (!isLengthValid(pkt, 2))
		return 0;

	pkt->buf[pkt->length++] = value >> 8;
	pkt->buf[pkt->length++] = value;
	return 2;
}

/* encode 32-bit big endian */
int pktEncodeBe32(pktEncodeT *pkt, uint32 value)
{
	if (!isLengthValid(pkt, 4))
		return 0;

	pkt->buf[pkt->length++] = value >> 24;
	pkt->buf[pkt->length++] = value >> 16;
	pkt->buf[pkt->length++] = value >> 8;
	pkt->buf[pkt->length++] = value;
	return 4;
}

/* encode 16-bit little endian */
int pktEncodeLe16(pktEncodeT *pkt, uint16 value)
{
	if (!isLengthValid(pkt, 2))
		return 0;

	pkt->buf[pkt->length++] = value;
	pkt->buf[pkt->length++] = value >> 8;
	return 2;
}

/* encode 32-bit little endian */
int pktEncodeLe32(pktEncodeT *pkt, uint32 value)
{
	if (!isLengthValid(pkt, 4))
		return 0;

	pkt->buf[pkt->length++] = value;
	pkt->buf[pkt->length++] = value >> 8;
	pkt->buf[pkt->length++] = value >> 16;
	pkt->buf[pkt->length++] = value >> 24;
	return 4;
}

/* encode bytes */
int pktEncodeBytes(pktEncodeT *pkt, int length, uint8 *bytes)
{
	if (!isLengthValid(pkt, length))
		return 0;

	memcpy(&pkt->buf[pkt->length], bytes, length);
	pkt->length += length;
	return length;
}
