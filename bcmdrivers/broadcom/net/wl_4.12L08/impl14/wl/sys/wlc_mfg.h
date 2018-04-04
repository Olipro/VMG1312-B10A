/*
 * Manufacturing Test Module
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_mfg.h,v 1.3 2009-04-19 01:29:31 Exp $
 */

#ifndef __wlc_mfg_h__
#define __wlc_mfg_h__

#ifdef WLMFG
extern mfg_info_t *wlc_mfg_attach(wlc_pub_t *pub, wlc_info_t *wlc);
extern int wlc_mfg_detach(mfg_info_t *info);
extern void wlc_mfg_init(mfg_info_t *info);
extern void wlc_mfg_deinit(mfg_info_t *info);
extern void wlc_mfg_up(mfg_info_t *info);
extern int wlc_mfg_down(mfg_info_t *info);

#if (defined(BCMROMMFG) && defined(BCMROMBUILD))
extern int wlc_mfg_ioctl_filter(wlc_pub_t *pub, int cmd);
extern int wlc_mfg_iovar_filter(wlc_pub_t *pub, const bcm_iovar_t *vi);
#else
#define wlc_mfg_ioctl_filter(a, b)	BCME_OK
#define wlc_mfg_iovar_filter(a, b)	BCME_OK
#endif 

#else /* WLMFG */

#define wlc_mfg_attach(a, b)		(mfg_info_t *)0x0dadbeef
#define wlc_mfg_detach(a)		0
#define wlc_mfg_init(a)			do {} while (0)
#define wlc_mfg_deinit(a)		do {} while (0)
#define wlc_mfg_up(a)			do {} while (0)
#define wlc_mfg_down(a)			0
#define wlc_mfg_ioctl_filter(a, b)	BCME_OK
#define wlc_mfg_iovar_filter(a, b)	BCME_OK
#endif /* WLMFG */

#endif /* __wlc_mfg_h__ */
