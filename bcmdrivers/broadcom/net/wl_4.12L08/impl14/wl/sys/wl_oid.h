
/*
 * NDIS OID_802_11 handler for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Portions
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wl_oid.h 354428 2012-08-31 02:01:48Z $
 */
#ifndef _wl_oid_h_
#define _wl_oid_h_

/* forward declare */
typedef struct wl_oid wl_oid_t;


#define WL_IOCTL_OID(oid) ((oid >= WL_OID_BASE) && (oid < (WL_OID_BASE + WLC_LAST)))


#define wl_oid_attach(a)		(wl_oid_t *)0x0dadbeef
#define wl_oid_detach(a)		do {} while (0)
#define wl_oid_reclaim(oid)		1
#define wl_freebsstable(a)		do {} while (0)
#define wl_agebsstable(a)		do {} while (0)
#define wl_set_infra(a, b)		do {} while (0)
#define wl_scan_complete(a, b, c)	do {} while (0)
#define wl_oid_event(a, b)		do {} while (0)
#define wl_fill_bsstable(a, b)		do {} while (0)

#endif /* _wl_oid_h_ */
