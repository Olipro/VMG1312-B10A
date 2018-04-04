/*
 * Wireless Ethernet (WET) tunnel
 *
 *   Copyright (C) 2012, Broadcom Corporation
 *   All Rights Reserved.
 *   
 *   This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 *   the contents of this file may not be disclosed to third parties, copied
 *   or duplicated in any form, in whole or in part, without the prior
 *   written permission of Broadcom Corporation.
 *
 *   $Id: wlc_wet_tunnel.h 330664 2012-05-02 06:55:45Z $
 */

#ifndef _wlc_wet_tunnel_h_
#define _wlc_wet_tunnel_h_


#ifdef WET_TUNNEL
#define WET_TUNNEL_ENAB(pub)	((pub)->wet_tunnel)
#define SCB_WET_TUNNEL(a)		((a) && ((a)->flags_brcm_syscap & SCBBS_WET_TUNNEL))
#else
#define WET_TUNNEL_ENAB(pub)		FALSE
#define SCB_WET_TUNNEL(a)	FALSE
#endif

/* forward declaration */
typedef struct wlc_wet_tunnel_info wlc_wet_tunnel_info_t;

/*
 * Initialize wet tunnel private context.It returns a pointer to the
 * wet tunnel private context if succeeded. Otherwise it returns NULL.
 */
extern wlc_wet_tunnel_info_t *wlc_wet_tunnel_attach(wlc_info_t *wlc);

/* Cleanup wet tunnel private context */
extern void wlc_wet_tunnel_detach(wlc_wet_tunnel_info_t *weth);

/* Process frames in transmit direction */
extern int wlc_wet_tunnel_send_proc(wlc_wet_tunnel_info_t *weth, void *sdu);

/* Process frames in receive direction */
extern int wlc_wet_tunnel_recv_proc(wlc_wet_tunnel_info_t *weth, void *sdu);

/* Process multicast frames in receive direction */
extern int wlc_wet_tunnel_multi_packet_forward(wlc_info_t *wlc, osl_t *osh,
	struct scb *scb, struct wlc_if *wlcif, void *sdu);

#ifdef BCMDBG
extern int wlc_wet_tunnel_dump(wlc_wet_tunnel_info_t *weth, struct bcmstrbuf *b);
#endif /* BCMDBG */

#endif	/* _wlc_wet_tunnel_h_ */
