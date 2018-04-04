/*
 * Required functions exported by the wlc_plt.c
 * to common (os-independent) driver code.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_plt.h 241182 2011-02-17 21:50:03Z $:
 */

#ifndef _wlc_plt_h_
#define _wlc_plt_h_

struct wlc_plt_info {
	bool plt_in_progress;
};

#ifdef WLPLT
extern struct wlc_plt_info *wlc_plt_attach(wlc_info_t *wlc);
extern int wlc_plt_detach(struct wlc_plt_info *plth);
#define WLC_PLT_IN_PROGRESS(wlc) (wlc->plt->plt_in_progress ? TRUE : FALSE)
#else
#define wlc_plt_attach(a) (struct wlc_plt_info *)0x0dadbeef
static INLINE int wlc_plt_detach(struct wlc_plt_info *plth) { return 0; }
#define WLC_PLT_IN_PROGRESS(wlc) FALSE
#endif /* WLPLT */

#endif	/* _wlc_plt_h_ */
