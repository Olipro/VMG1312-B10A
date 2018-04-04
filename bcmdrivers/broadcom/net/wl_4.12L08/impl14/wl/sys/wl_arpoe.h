/*
 * ARP Offload interface
 *
 *   Copyright (C) 2012, Broadcom Corporation
 *   All Rights Reserved.
 *   
 *   This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 *   the contents of this file may not be disclosed to third parties, copied
 *   or duplicated in any form, in whole or in part, without the prior
 *   written permission of Broadcom Corporation.
 *
 *   $Id: wl_arpoe.h 288820 2011-10-10 15:16:39Z $
 */

#ifndef _wl_arpoe_h_
#define _wl_arpoe_h_

/* Forward declaration */
typedef struct wl_arp_info wl_arp_info_t;

/* Return values */
#define ARP_REPLY_PEER 		0x1	/* Reply was sent to service ARP request from peer */
#define ARP_REPLY_HOST		0x2	/* Reply was sent to service ARP request from host */
#define ARP_REQ_SINK		0x4	/* Input packet should be discarded */
#define ARP_FORCE_FORWARD       0X5     /* ARP req should be forwarded to host,
					 * bypassing pktfilter
					 */

#ifdef ARPOE

/*
 * Initialize ARP private context.
 * Returns a pointer to the ARP private context, NULL on failure.
 */
extern wl_arp_info_t *wl_arp_attach(wlc_info_t *wlc);

/* Cleanup ARP private context */
extern void wl_arp_detach(wl_arp_info_t *arpi);

/* Process frames in transmit direction */
extern int wl_arp_send_proc(wl_arp_info_t *arpi, void *sdu);

/* Process frames in receive direction */
extern int wl_arp_recv_proc(wl_arp_info_t *arpi, void *sdu);

/* called when a new virtual IF is created.
 *	i/p: primary ARPIIF [arpi_p] and the new wlcif,
 *	o/p: new arpi structure populated with inputs and
 *		the global parameters duplicated from arpi_p
 *	side-effects: arpi for a new IF will inherit properties of arpi_p till
 *		the point new arpi is created. After that, for any change in
 *		arpi_p will NOT change the arpi corr to new IF. To change property
 *		of new IF, wl -i wl0.x has to be used.
*/
extern wl_arp_info_t *wl_arp_alloc_ifarpi(wl_arp_info_t *arpi_p,
	wlc_if_t *wlcif);

extern void wl_arp_free_ifarpi(wl_arp_info_t *arpi);

#else	/* stubs */

#define wl_arp_attach(a)		(wl_arp_info_t *)0x0dadbeef
#define	wl_arp_detach(a)		do {} while (0)
#define wl_arp_send_proc(a, b)		(-1)
#define wl_arp_recv_proc(a, b)		(-1)
#define wl_arp_alloc_ifarpi(a, b)	(0)
#define wl_arp_free_ifarpi(a)		(0)

#endif /* ARPOE */

#endif	/* _wl_arpoe_h_ */
