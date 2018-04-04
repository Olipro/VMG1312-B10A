/*
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

#ifndef _PKTGAS_H_
#define _PKTGAS_H_

/* public action frame */
#define PUBLIC_ACTION_FRAME						4

/* GAS action frames */
#define GAS_REQUEST_ACTION_FRAME				10
#define GAS_RESPONSE_ACTION_FRAME				11
#define GAS_COMEBACK_REQUEST_ACTION_FRAME		12
#define GAS_COMEBACK_RESPONSE_ACTION_FRAME		13

/* information elements */
#define INTERWORKING_IE							107
#define ADVERTISEMENT_PROTOCOL_IE				108
#define EXPEDITED_BANDWIDTH_REQUEST_IE			109
#define QOS_MAP_SET_IE							110
#define ROAMING_CONSORTIUM_IE					111
#define EMERGENCY_ALERT_IDENTIFIER_IE			112

#endif /* _PKTGAS_H_ */
