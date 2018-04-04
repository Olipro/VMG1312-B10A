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

#ifndef _PKTANQP_H_
#define _PKTANQP_H_

#define ADVERTISEMENT_PROTOCOL_IE	108

#define PAMEBI			0

/* query response limit */
#define QRL_REQUEST		0x00
#define QRL_RESPONSE	0x7f

#define ADVERTISEMENT_PROTOCOL_ID	0

#define ANQP_OUI_SUBTYPE	9

/* ANQP information ID */
#define ANQP_QUERY_LIST						256
#define ANQP_CAPABILITY_LIST				257
#define VENUE_NAME_INFO						258
#define EMERGENCY_CALL_NUMBER_INFO			259
#define NETWORK_AUTHENTICATION_TYPE_INFO	260
#define ROAMING_CONSORTIUM_LIST				261
#define IP_ADDRESS_TYPE_AVAILABILITY_INFO	262
#define NAI_REALM_LIST						263
#define G3PP_CELLULAR_NETWORK_INFO			264
#define AP_GEOSPATIAL_LOCATION				265
#define AP_CIVIC_LOCATION					266
#define AP_LOCATION_PUBLIC_ID_URI			267
#define DOMAIN_NAME_LIST					268
#define EMERGENCY_ALERT_ID_URI				269
#define EMERGENCY_NAI						271
#define ANQP_VENDOR_SPECIFIC_LIST			56797

/* venue group */
#define UNSPECIFIED				0
#define ASSEMBLY				1
#define BUSINESS				2
#define EDUCATIONAL				3
#define FACTORY					4
#define INSTITUTIONAL			5
#define MERCANTILE				6
#define RESIDENTIAL				7
#define STORAGE					8
#define UTILITY					9
#define VEHICULAR				10
#define OUTDOOR					11

/* venue name */
#define MAX_LANGAUGE_SIZE		3
#define MAX_NAME_SIZE			255

/* network authentication type indicator */
#define ACCEPTANCE_OF_TERMS_CONDITIONS		0
#define ONLINE_ENROLLMENT_SUPPORTED			1
#define HTTP_HTTPS_REDIRECTION				2
#define DNS_REDIRECTION						3

/* IP address type availability - IPv6 */
#define IPV6_SHIFT						0
#define IPV6_MASK						(0x03 << IPV6_SHIFT)
#define	IPV6_NOT_AVAILABLE				0x00
#define IPV6_AVAILABLE					0x01
#define IPV6_UNKNOWN_AVAILABILITY		0x02

/* IP address type availability - IPv4 */
#define IPV4_SHIFT						2
#define IPV4_MASK						(0x3f << IPV4_SHIFT)
#define	IPV4_NOT_AVAILABLE				0x00
#define IPV4_PUBLIC						0x01
#define IPV4_PORT_RESTRICT				0x02
#define IPV4_SINGLE_NAT					0x03
#define IPV4_DOUBLE_NAT					0x04
#define IPV4_PORT_RESTRICT_SINGLE_NAT	0x05
#define IPV4_PORT_RESTRICT_DOUBLE_NAT	0x06
#define IPV4_UNKNOWN_AVAILABILITY		0x07

/* NAI realm encoding */
#define REALM_ENCODING_RFC4282	0
#define REALM_ENCODING_UTF8		1

/* IANA EAP method type numbers */
#define EAP_TLS					13
#define EAP_SIM					18
#define EAP_TTLS				21
#define EAP_AKA					23
#define EAP_PSK					47
#define EAP_AKAP				50

/* authentication ID */
#define EXPANDED_EAP						1
#define NON_EAP_INNER_AUTHENTICATION		2
#define INNER_AUTHENTICATION_EAP			3
#define EXPANDED_INNER_EAP					4
#define CREDENTIAL							5
#define TUNNELED_EAP_CREDENTIAL				6
#define VENDOR_SPECIFIC_EAP					221

/* non-EAP inner authentication type */
#define PAP						1
#define CHAP					2
#define MSCHAP					3
#define MSCHAPV2				4

/* credential type */
#define SIM						1
#define USIM					2
#define NFC						3
#define HARDWARE_TOKEN			4
#define SOFTOKEN				5
#define CERTIFICATE				6
#define USERNAME_PASSWORD		7
#define SERVER_SIDE				8

/* PLMN */
#define GUD_VERSION		0
#define PLMN_LIST_IE	0

/* service protocol type */
#define SERVICE_PROTOCOL_ALL			0
#define SERVICE_PROTOCOL_BONJOUR		1
#define SERVICE_PROTOCOL_UPNP			2
#define SERVICE_PROTOCOL_WS_DISCOVERY	3
#define SERVICE_PROTOCOL_WIFI_DISPLAY	4

#endif /* _PKTANQP_H_ */
