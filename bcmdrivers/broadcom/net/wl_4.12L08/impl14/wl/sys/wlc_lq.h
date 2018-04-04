/*
 * Code that controls the link quality
 * Broadcom 802.11bang Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_lq.h 281543 2011-09-02 17:34:54Z $
 */

#ifndef _wlc_lq_h_
#define _wlc_lq_h_

extern int wlc_lq_attach(wlc_info_t* wlc);
extern void wlc_lq_detach(wlc_info_t* wlc);

#ifdef WLCHANIM
extern void wlc_lq_chanim_update(wlc_info_t *wlc, chanspec_t chanspec, uint32 flags);
extern void wlc_lq_chanim_acc_reset(wlc_info_t *wlc);
extern bool wlc_lq_chanim_interfered(wlc_info_t *wlc, chanspec_t chanspec);
extern void wlc_lq_chanim_upd_act(wlc_info_t *wlc);
extern void wlc_lq_chanim_upd_acs_record(chanim_info_t *c_info, chanspec_t home_chspc,
	chanspec_t selected, uint8 trigger);
extern void wlc_lq_chanim_action(wlc_info_t *wlc);
#else
#define wlc_lq_chanim_update(a, b, c)	do {} while (0)
#define wlc_lq_chanim_acc_reset(a)	do {} while (0)
#define wlc_lq_chanim_interfered(a, b)	0
#define wlc_lq_chanim_upd_act(a)	do {} while (0)
#define wlc_lq_chanim_upd_acs_record(a, b, c, d) do {} while (0)
#define wlc_lq_chanim_action(a)		do {} while (0)
#endif /* WLCHANIM */


#ifdef WLCQ
extern int wlc_lq_channel_qa_start(wlc_info_t *wlc);
#else
#define wlc_lq_channel_qa_start(a)	(0)
#endif

#endif	/* _wlc_lq_h */
