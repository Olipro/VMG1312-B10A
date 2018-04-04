/*
 * BSS load IE header file
 *
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

#ifndef _WLC_BSSLOAD_H_
#define _WLC_BSSLOAD_H_

#ifdef WLBSSLOAD
extern wlc_bssload_info_t *wlc_bssload_attach(wlc_info_t *wlc);
extern void wlc_bssload_detach(wlc_bssload_info_t *mbssload);
extern uint8 *wlc_bssload_write_ie_beacon(wlc_bssload_info_t *mbssload,
	wlc_bsscfg_t *cfg, uint8 *cp, uint8 *bufend);
#else	/* stubs */
#define wlc_bssload_attach(wlc) NULL
#define wlc_bssload_detach(mbssload) do {} while (0)
#define wlc_bssload_write_ie_beacon(mbssload, cfg, cp, bufend) (cp)
#endif /* WLBSSLOAD */

#endif /* _WLC_BSSLOAD_H_ */
