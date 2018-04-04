/*
 * Broadcom security module ipc ports file
 *
 * Copyright (C) 2012, Broadcom Corporation. All Rights Reserved.
 * 
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * $Id: security_ipc.h 300516 2011-12-04 17:39:44Z $
 */

#ifndef __SECURITY_IPC_H__
#define __SECURITY_IPC_H__

/*
 * WAI module
 */
#define WAI_UI_ADDR			"127.0.0.1"
#define WAP_UI_PORT			9002

/*
 * AS module
 */
#define AS_UI_ADDR			"127.0.0.1"
#define AS_UI_PORT			9001
#define AS_WAI_PORT			3810

/*
 * EAP module
 */
#define EAPD_WKSP_UDP_ADDR		"127.0.0.1"

/* get_ifname_unit() index is << 4 */
#define EAPD_WKSP_PORT_INDEX_SHIFT	4
#define EAPD_WKSP_SPORT_OFFSET		(1 << 5)
#define EAPD_WKSP_MPORT_OFFSET		(1 << 6)
#define EAPD_WKSP_VX_PORT_OFFSET	(1 << 7)

#define EAPD_WKSP_WPS_UDP_PORT		37000
#define EAPD_WKSP_WPS_UDP_RPORT		EAPD_WKSP_WPS_UDP_PORT
#define EAPD_WKSP_WPS_UDP_SPORT		EAPD_WKSP_WPS_UDP_PORT + EAPD_WKSP_SPORT_OFFSET
#define EAPD_WKSP_WPS_UDP_MPORT		EAPD_WKSP_WPS_UDP_PORT + EAPD_WKSP_MPORT_OFFSET

#define EAPD_WKSP_NAS_UDP_PORT		38000
#define EAPD_WKSP_NAS_UDP_RPORT		EAPD_WKSP_NAS_UDP_PORT
#define EAPD_WKSP_NAS_UDP_SPORT		EAPD_WKSP_NAS_UDP_PORT + EAPD_WKSP_SPORT_OFFSET

#define EAPD_WKSP_SES_UDP_PORT		39000
#define EAPD_WKSP_SES_UDP_RPORT		EAPD_WKSP_SES_UDP_PORT
#define EAPD_WKSP_SES_UDP_SPORT		EAPD_WKSP_SES_UDP_PORT + EAPD_WKSP_SPORT_OFFSET

#define EAPD_WKSP_WAI_UDP_PORT		41000
#define EAPD_WKSP_WAI_UDP_RPORT 	EAPD_WKSP_WAI_UDP_PORT
#define EAPD_WKSP_WAI_UDP_SPORT 	EAPD_WKSP_WAI_UDP_PORT + EAPD_WKSP_SPORT_OFFSET

#define EAPD_WKSP_DCS_UDP_PORT		42000
#define EAPD_WKSP_DCS_UDP_RPORT 	EAPD_WKSP_DCS_UDP_PORT
#define EAPD_WKSP_DCS_UDP_SPORT 	EAPD_WKSP_DCS_UDP_PORT + EAPD_WKSP_SPORT_OFFSET

#define EAPD_WKSP_DIF_UDP_PORT		43000
#ifdef DSLCPE
#define EAPD_WKSP_EVT_UDP_PORT		50000
#define EAPD_WKSP_EVT_UDP_RPORT		EAPD_WKSP_EVT_UDP_PORT
#define EAPD_WKSP_EVT_UDP_SPORT		EAPD_WKSP_EVT_UDP_PORT + EAPD_WKSP_SPORT_OFFSET
#endif /* DSLCPE */

/*
 * UPNP module
 */
#define	UPNP_IPC_ADDR			"127.0.0.1"
#define UPNP_WFA_ADDR			"127.0.0.1"

#define UPNP_IPC_PORT			40100
#define UPNP_WFA_PORT			40040		/* WFA wlan receive port */

/* WPS UPNP definitions */
#define UPNP_WPS_TYPE_SSR		1		/* Set Selected Registrar */
#define UPNP_WPS_TYPE_PMR		2		/* Wait For Put Message Resp */
#define UPNP_WPS_TYPE_GDIR		3		/* Wait For Get DevInfo Resp */
#define UPNP_WPS_TYPE_PWR		4		/* Put WLAN Response */
#define UPNP_WPS_TYPE_WE		5		/* WLAN Event */
#define UPNP_WPS_TYPE_QWFAS		6		/* Query WFAWLANConfig Subscribers */
#define UPNP_WPS_TYPE_DISCONNECT	7		/* Subscriber unreachable */
#define UPNP_WPS_TYPE_MAX		8

typedef struct {
	unsigned int type;
	unsigned char dst_addr[16];
	unsigned int length;
	unsigned char data[1];
} UPNP_WPS_CMD;

#define UPNP_WPS_CMD_SIZE		24

/*
 * WPS module
 */
#define WPS_EAP_ADDR			"127.0.0.1"
#define WPS_UPNPDEV_ADDR		"127.0.0.1"

#define WPS_UPNPDEV_PORT		40000

#endif	/* __SECURITY_IPC_H__ */
