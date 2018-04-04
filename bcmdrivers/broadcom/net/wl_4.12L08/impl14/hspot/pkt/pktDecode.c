/*
 * Decoding base functions.
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
#include "pktDecode.h"

static int isLengthValid(pktDecodeT *pkt, int length)
{
	if (pkt == 0)
		return FALSE;

	if (pkt->offset + length > pkt->maxLength) {
		WL_ERROR(("exceeding data buffer %d > %d\n",
			pkt->offset + length, pkt->maxLength));
		return FALSE;
	}

	return TRUE;
}

/* initialize pkt decode with decode buffer */
int pktDecodeInit(pktDecodeT *pkt, int maxLength, uint8 *data)
{
	pkt->maxLength = maxLength;
	pkt->offset = 0;
	pkt->buf = data;
	return TRUE;
}

/* decode byte */
int pktDecodeByte(pktDecodeT *pkt, uint8 *byte)
{
	if (!isLengthValid(pkt, 1))
		return 0;

	*byte = pkt->buf[pkt->offset++];
	return 1;
}

/* decode 16-bit big endian */
int pktDecodeBe16(pktDecodeT *pkt, uint16 *value)
{
	if (!isLengthValid(pkt, 2))
		return 0;

	*value =
		pkt->buf[pkt->offset] << 8 |
		pkt->buf[pkt->offset + 1];
	pkt->offset += 2;
	return 2;
}

/* decode 32-bit big endian */
int pktDecodeBe32(pktDecodeT *pkt, uint32 *value)
{
	if (!isLengthValid(pkt, 4))
		return 0;

	*value =
		pkt->buf[pkt->offset] << 24 |
		pkt->buf[pkt->offset + 1] << 16 |
		pkt->buf[pkt->offset + 2] << 8 |
		pkt->buf[pkt->offset + 3];
	pkt->offset += 4;
	return 4;
}

/* decode 16-bit little endian */
int pktDecodeLe16(pktDecodeT *pkt, uint16 *value)
{
	if (!isLengthValid(pkt, 2))
		return 0;

	*value =
		pkt->buf[pkt->offset] |
		pkt->buf[pkt->offset + 1] << 8;
	pkt->offset += 2;
	return 2;
}

/* decode 32-bit little endian */
int pktDecodeLe32(pktDecodeT *pkt, uint32 *value)
{
	if (!isLengthValid(pkt, 4))
		return 0;

	*value =
		pkt->buf[pkt->offset] |
		pkt->buf[pkt->offset + 1] << 8 |
		pkt->buf[pkt->offset + 2] << 16 |
		pkt->buf[pkt->offset + 3] << 24;
	pkt->offset += 4;
	return 4;
}

/* decode bytes */
int pktDecodeBytes(pktDecodeT *pkt, int length, uint8 *bytes)
{
	if (!isLengthValid(pkt, length))
		return 0;

	memcpy(bytes, &pkt->buf[pkt->offset], length);
	pkt->offset += length;
	return length;
}
