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

#ifndef _PKTIE_H_
#define _PKTIE_H_

/* hotspot OUI type */
#define HOTSPOT_TYPE	0x10

/* hostspot config */
#define DGAF_DISABLED	0x01	/* downstream group-addressed forward */

/* access network options */
#define ACCESS_NETWORK_TYPE_MASK	0x0f
#define INTERNET_MASK				0x10
#define ASRA_MASK					0x20
#define ESR_MASK					0x40
#define UESA_MASK					0x80

/* access network type */
#define PRIVATE_NETWORK				0
#define PRIVATE_NETWORK_WITH_GUEST	1
#define CHARGEABLE_PUBLIC_NETWORK	2
#define FREE_PUBLIC_NETWORK			3
#define PERSONAL_DEVICE_NETWORK		4
#define EMERGENCY_SERVICES_NETWORK	5
#define TEST_NETWORK				14
#define WILDCARD_NETWORK			15

/* HESSID */
typedef struct ether_addr hessidT;

/* ANQP protocol */
#define ANQP_PROTOCOL_ID	0

/* advertisement protocol */
#define QUERY_RESPONSE_LIMIT_MASK	0x7f
#define PAME_BI_MASK				0x80

#endif /* _PKTIE_H_ */
