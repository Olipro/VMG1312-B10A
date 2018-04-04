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

#ifndef _PKTDECODE_H_
#define _PKTDECODE_H_

#include "typedefs.h"

typedef struct
{
	int maxLength;
	int offset;
	uint8 *buf;
} pktDecodeT;

#define pktDecodeBufLength(pkt)		((pkt)->maxLength)
#define pktDecodeBuf(pkt)			((pkt)->buf)
#define pktDecodeOffset(pkt)		((pkt)->offset)
#define pktDecodeRemaining(pkt)		\
	((pkt)->maxLength > (pkt)->offset ? (pkt)->maxLength - (pkt)->offset : 0)
#define pktDecodeCurrentPtr(pkt)	(&(pkt)->buf[(pkt)->offset])

/* initialize pkt decode with decode buffer */
int pktDecodeInit(pktDecodeT *pkt, int maxLength, uint8 *data);

/* decode byte */
int pktDecodeByte(pktDecodeT *pkt, uint8 *byte);

/* decode 16-bit big endian */
int pktDecodeBe16(pktDecodeT *pkt, uint16 *value);

/* decode 32-bit big endian */
int pktDecodeBe32(pktDecodeT *pkt, uint32 *value);

/* decode 16-bit little endian */
int pktDecodeLe16(pktDecodeT *pkt, uint16 *value);

/* decode 32-bit little endian */
int pktDecodeLe32(pktDecodeT *pkt, uint32 *value);

/* decode bytes */
int pktDecodeBytes(pktDecodeT *pkt, int length, uint8 *bytes);

#endif /* _PKTDECODE_H_ */
