/*
 * API to generic high resolution timer abstraction layer for multiplexing h/w timer
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

#ifndef _wlc_hrt_h_
#define _wlc_hrt_h_

/* module attach/detach */
extern wlc_hrt_info_t *wlc_hrt_attach(wlc_info_t *wlc);
extern void wlc_hrt_detach(wlc_hrt_info_t *hrti);

/* h/w timer access routine registration
 * get_time: returns the current value of a free running counter h/w
 * set_timer: programs a timeout to the count-up/count-down timer h/w
 *            the timer shall generate an interrupt when the timeout expires
 * ack_timer: deserts the timer interrupt
 * max_timer_val: maximum value of the the free running counter
 */
typedef uint (*wlc_hrt_get_time_fn)(void *ctx);
typedef void (*wlc_hrt_set_timer_fn)(void *ctx, uint timeout);
typedef void (*wlc_hrt_ack_timer_fn)(void *ctx);
extern int wlc_hrt_hai_register(wlc_hrt_info_t *hrti,
	wlc_hrt_get_time_fn get_time, wlc_hrt_set_timer_fn set_timer,
	wlc_hrt_ack_timer_fn ack_timer,
	void *ctx, uint max_timer_val);

/* isr/dpc */
extern void wlc_hrt_isr(wlc_hrt_info_t *hrti);

/* timer object creation/deletion */
extern wlc_hrt_to_t *wlc_hrt_alloc_timeout(wlc_hrt_info_t *hrti);
extern void wlc_hrt_free_timeout(wlc_hrt_to_t *to);

/* timer arming/canceling */
typedef void (*wlc_hrt_to_cb_fn)(void *arg);
extern bool wlc_hrt_add_timeout(wlc_hrt_to_t *to, uint timeout, wlc_hrt_to_cb_fn fun, void *arg);
extern void wlc_hrt_del_timeout(wlc_hrt_to_t *to);

/* time accessors */
extern uint wlc_hrt_gettime(wlc_hrt_info_t *hrti);
extern uint wlc_hrt_getdelta(wlc_hrt_info_t *hrti, uint *last);

/* deprecated */
extern void wlc_hrt_gptimer_abort(wlc_info_t *wlc);
extern void wlc_hrt_gptimer_cb(wlc_info_t *wlc);

#endif /* !_wlc_hrt_h_ */
