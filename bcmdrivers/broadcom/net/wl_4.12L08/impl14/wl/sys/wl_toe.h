/*
 * TCP Offload Engine (TOE) components interface
 *
 *   Copyright (C) 2012, Broadcom Corporation
 *   All Rights Reserved.
 *   
 *   This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 *   the contents of this file may not be disclosed to third parties, copied
 *   or duplicated in any form, in whole or in part, without the prior
 *   written permission of Broadcom Corporation.
 *
 *   $Id: wl_toe.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _wl_toe_h_
#define _wl_toe_h_

/* Forward declaration */
typedef struct wl_toe_info wl_toe_info_t;

#ifdef TOE

/*
 * Initialize toe private context.It returns a pointer to the
 * toe private context if succeeded. Otherwise it returns NULL.
 */
extern wl_toe_info_t *wl_toe_attach(wlc_info_t *wlc);

/* Cleanup toe private context */
extern void wl_toe_detach(wl_toe_info_t *toei);

/* Process frames in transmit direction */
extern void wl_toe_send_proc(wl_toe_info_t *toei, void *sdu);

/* Process frames in receive direction */
extern int wl_toe_recv_proc(wl_toe_info_t *toei, void *sdu);

extern void wl_toe_set_olcmpnts(wl_toe_info_t *toei, int ol_cmpnts);

#else	/* stubs */

#define wl_toe_attach(a)		(wl_toe_info_t *)0x0dadbeef
#define	wl_toe_detach(a)		do {} while (0)
#define wl_toe_send_proc(a, b)		do {} while (0)
#define wl_toe_recv_proc(a, b)		(-1)
#define wl_toe_set_olcmpnts(a, b)	do {} while (0)

#endif /* TOE */

#endif	/* _wl_toe_h_ */
