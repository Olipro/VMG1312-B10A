/*
 * Wireless Ethernet (WET) interface
 *
 *   Copyright (C) 2012, Broadcom Corporation
 *   All Rights Reserved.
 *   
 *   This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 *   the contents of this file may not be disclosed to third parties, copied
 *   or duplicated in any form, in whole or in part, without the prior
 *   written permission of Broadcom Corporation.
 *
 *   $Id: wlc_wet.h 353863 2012-08-29 03:22:47Z $
 */

#ifndef _wlc_wet_h_
#define _wlc_wet_h_

/* forward declaration */
typedef struct wlc_wet_info wlc_wet_info_t;

/*
 * Initialize wet private context.It returns a pointer to the
 * wet private context if succeeded. Otherwise it returns NULL.
 */
extern wlc_wet_info_t *wlc_wet_attach(wlc_info_t *wlc);

/* Cleanup wet private context */
extern void wlc_wet_detach(wlc_wet_info_t *weth);

/* Process frames in transmit direction */
extern int wlc_wet_send_proc(wlc_wet_info_t *weth, void *sdu, void **new);

/* Process frames in receive direction */
extern int wlc_wet_recv_proc(wlc_wet_info_t *weth, void *sdu);

#ifdef BCMDBG
extern int wlc_wet_dump(wlc_wet_info_t *weth, struct bcmstrbuf *b);
#endif /* BCMDBG */

#ifdef PLC_WET
extern void wlc_wet_bssid_upd(wlc_wet_info_t *weth, wlc_bsscfg_t *cfg);
#endif /* PLC_WET */

#endif	/* _wlc_wet_h_ */
