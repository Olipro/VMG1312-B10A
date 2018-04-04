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

#ifndef _PKTHSPOTANQP_H_
#define _PKTHSPOTANQP_H_

/* length includes OUI + type + subtype + reserved */
#define HSPOT_LENGTH_OVERHEAD	(WFA_OUI_LEN + 1 + 1 + 1)

/* OUI and type */
#define WFA_OUI			"\x50\x6F\x9A"
#define HSPOT_OI_TYPE	0x11
#define HSPOT_OUI		"\x50\x6F\x9A\x11"

/* subtype */
#define RESERVED_SUBTYPE					0
#define HSPOT_QUERY_LIST_SUBTYPE			1
#define HSPOT_CAPABILITY_LIST_SUBTYPE		2
#define OPERATOR_FRIENDLY_NAME_SUBTYPE		3
#define WAN_METRICS_SUBTYPE					4
#define CONNECTION_CAPABILITY_SUBTYPE		5
#define NAI_HOME_REALM_QUERY_SUBTYPE		6
#define OPERATING_CLASS_INDICATION_SUBTYPE	7

/* operator friendly name */
#define MAX_LANGAUGE_SIZE		3
#define MAX_NAME_SIZE			255

/* WAN info - link status */
#define LINK_STATUS_SHIFT		0
#define LINK_STATUS_MASK		(0x03 << LINK_STATUS_SHIFT)
#define	LINK_UP					0x01
#define LINK_DOWN				0x02
#define LINK_TEST				0x03

/* WAN info - symmetric link */
#define SYMMETRIC_LINK_SHIFT	2
#define SYMMETRIC_LINK_MASK		(0x01 << SYMMETRIC_LINK_SHIFT)
#define SYMMETRIC_LINK			0x01
#define NOT_SYMMETRIC_LINK		0x00

/* WAN info - at capacity */
#define AT_CAPACITY_SHIFT		3
#define AT_CAPACITY_MASK		(0x01 << AT_CAPACITY_SHIFT)
#define AT_CAPACITY				0x01
#define NOT_AT_CAPACITY			0x00

/* connection capability */
#define STATUS_CLOSED			0
#define STATUS_OPEN				1
#define STATUS_UNKNOWN			2

#endif /* _PKTHSPOTANQP_H_ */
