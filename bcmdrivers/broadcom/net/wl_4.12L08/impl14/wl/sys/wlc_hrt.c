/*
 * Generic high resolution timer abstraction layer for multiplexing h/w timer
 * Broadcom 802.11abgn Networking Device Driver
 *
 * It also includes an implementation using d11 tsf/gptimer h/w.
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

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_alloc.h>
#include <wlc_hrt.h>

/* timeout structure, storage provided by the user */
struct wlc_hrt_to {
	wlc_hrt_to_t *next;		/* pointer to next timeout object */
	wlc_hrt_info_t *hrti;
	uint timeout;			/* amount of time remaining till timeout */
	wlc_hrt_to_cb_fn fun;		/* function to call when timed out */
	void *arg;			/* argument passed to function above */
	bool expired;			/* indicates timeout object has expired */
};

/* high res. timer module private states */
struct wlc_hrt_info {
	wlc_info_t *wlc;		/* back pointer to wlc */
	wlc_hrt_to_t timer_list;	/* delta sorted timeout list */
	wlc_hrt_get_time_fn get_time;	/* fn to get current time */
	wlc_hrt_set_timer_fn set_timer;	/* fn to set the hw timer */
	wlc_hrt_ack_timer_fn ack_timer;	/* fn to ack the hw timer, can be NULL */
	void *ctx;
	uint max_timer_val;		/* max timer value for this hw timer */
	uint last_time;			/* stores the last time gettime was called */
	bool timer_allowed;		/* disables timeout */
	bool timer_armed;		/* indicates hw timer is set */
};

/* local functions for multiplexed hw gptimer use */

/* module */

/* debug */
#if defined(BCMDBG)
static int wlc_hrt_dump(void *ctx, struct bcmstrbuf *b);
#endif

/* timeout */
static void wlc_hrt_run_timeouts(wlc_hrt_info_t *hrti);

/* tsf/gptimer as high res. timer h/w */
static uint wlc_hrt_gptimer_gettime(void *ctx);
static void wlc_hrt_gptimer_settimer(void *ctx, uint timeout);
static void wlc_hrt_gptimer_ack(void *ctx);

/* module */
wlc_hrt_info_t *
wlc_hrt_attach(wlc_info_t *wlc)
{
	wlc_hrt_info_t *hrti;

	if ((hrti = (wlc_hrt_info_t *)
	     wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_hrt_info_t))) == NULL) {
		WL_ERROR(("%s: failed to allocate memory for hw timer\n", __FUNCTION__));
		goto fail;
	}
	hrti->wlc = wlc;

	/* TODO: mode this registration out when a separate module is created
	 * for gptimer h/w timer.
	 */
	(void)wlc_hrt_hai_register(hrti,
	                           wlc_hrt_gptimer_gettime,
	                           wlc_hrt_gptimer_settimer,
	                           wlc_hrt_gptimer_ack,
	                           wlc,
	                           (uint)(uint32)(~0));

#if defined(BCMDBG)
	wlc_dump_register(wlc->pub, "hrt", wlc_hrt_dump, hrti);
#endif

	return hrti;

fail:
	wlc_hrt_detach(hrti);
	return NULL;
}

void
wlc_hrt_detach(wlc_hrt_info_t *hrti)
{
	wlc_info_t *wlc;

	if (hrti == NULL)
		return;

	wlc = hrti->wlc;

	MFREE(wlc->osh, hrti, sizeof(wlc_hrt_info_t));
}

int
wlc_hrt_hai_register(wlc_hrt_info_t *hrti,
	wlc_hrt_get_time_fn get_time,
	wlc_hrt_set_timer_fn set_timer,
	wlc_hrt_ack_timer_fn ack_timer,
	void *ctx,
	uint max_timer_val)
{
	if (hrti->timer_allowed)
		return BCME_BUSY;

	hrti->get_time = get_time;
	hrti->set_timer = set_timer;
	hrti->ack_timer = ack_timer;
	hrti->ctx = ctx;
	hrti->max_timer_val = max_timer_val;

	hrti->timer_allowed = TRUE;

	return BCME_OK;
}

/* timer isr */
void
wlc_hrt_isr(wlc_hrt_info_t *hrti)
{
	wlc_hrt_to_t *head;

	hrti->timer_armed = FALSE;
	wlc_hrt_run_timeouts(hrti);
	if (hrti->timer_armed)
		return;
	head = &hrti->timer_list;
	if (head->next != NULL) {
		hrti->set_timer(hrti->ctx, head->next->timeout);
		hrti->timer_armed = TRUE;
	}
	else if (hrti->ack_timer != NULL) {
		hrti->ack_timer(hrti->ctx);
	}
}

/* timer multiplexing layer */

/* Update expiration times in timeout list */
static void
wlc_hrt_update_timeout_list(wlc_hrt_info_t *hrti)
{
	uint delta;
	wlc_hrt_to_t *head = &hrti->timer_list;
	wlc_hrt_to_t *this = head->next;

	delta = wlc_hrt_getdelta(hrti, &hrti->last_time);
	if (this == NULL)
		return;
	if (delta == 0 && this->timeout != 0)
		return;
	while (this != NULL) {
		if (this->timeout <= delta) {
			/* timer has expired */
			delta -= this->timeout;
			this->timeout = 0;
			this->expired = TRUE;
			this = this->next;
		} else {
			this->timeout -= delta;
			break;
		}
	}
}

/* Remove expired from timeout list and invoke their callbacks */
static void
wlc_hrt_run_timeouts(wlc_hrt_info_t *hrti)
{
	wlc_hrt_to_t *this;
	wlc_hrt_to_t *head = &hrti->timer_list;
	wlc_hrt_to_cb_fn fun;
	bool expired;

	if ((this = head->next) == NULL)
		return;

	do {
		expired = FALSE;

		wlc_hrt_update_timeout_list(hrti);

		/* Always start from the head */
		while (((this = head->next) != NULL) && (this->expired == TRUE)) {
			head->next = this->next;
			fun = this->fun;
			this->fun = NULL;
			this->expired = FALSE;
			fun(this->arg);
			/* This flag tells us we've run timeout functions */
			expired = TRUE;
		}
		/* if we're out of timeout elements, then no more updates necessary
		 * even if we've run timeout functions
		 */
		if (head->next == NULL) {
			expired = FALSE;
		}
		/* if we've run timeout functions and timer not restarted, need to
		 * update the timeouts again in case one of the timeout functions
		 * we just ran took a long time.
		 * We should do this anyway just to take into account the times used up
		 * by the timeout functions.
		 */
	} while ((expired == TRUE) && (hrti->timer_armed == FALSE));
}

#if defined(BCMDBG)
static int
wlc_hrt_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_hrt_info_t *hrti = (wlc_hrt_info_t *)ctx;
	wlc_hrt_to_t *this;

	bcm_bprintf(b, "get_time %p set_timer %p act_timer %p context %p max_time 0x%x\n",
	            hrti->get_time, hrti->set_timer, hrti->ack_timer,
	            hrti->ctx, hrti->max_timer_val);
	bcm_bprintf(b, "timer_allowed %d timer_armed %d last_time 0x%x\n",
	            hrti->timer_allowed, hrti->timer_armed, hrti->last_time);

	for (this = hrti->timer_list.next; this != NULL; this = this->next) {
		bcm_bprintf(b, "timer %p, fun %p, arg %p, %d TO units\n",
		            this, this->fun, this->arg, this->timeout);
	}

	return BCME_OK;
}
#endif 

/* Add timeout to the list */
bool
wlc_hrt_add_timeout(wlc_hrt_to_t *to, uint timeout, wlc_hrt_to_cb_fn fun, void *arg)
{
	wlc_hrt_info_t *hrti = to->hrti;
	wlc_hrt_to_t *prev, *this;
	uint delta_to = timeout;

	if (to->fun != NULL) {
		WL_INFORM(("fun not null in 0x%p, timer already in list?\n", to));
		return FALSE;
	}

	if (timeout == 0) {
		WL_INFORM(("timeout is 0\n"));
		return FALSE;
	}

	if (!hrti->timer_allowed) {
		WL_INFORM(("no timer is allowed\n"));
		return FALSE;
	}

	wlc_hrt_update_timeout_list(hrti);

	/* find a proper location for the new timeout and update the timer value */
	prev = &hrti->timer_list;
	while (((this = prev->next) != NULL) && (this->timeout <= delta_to)) {
		delta_to -= this->timeout;
		prev = this;
	}
	if (this != NULL)
		this->timeout -= delta_to;

	to->fun = fun;
	to->arg = arg;
	to->timeout = delta_to;

	/* insert the new timeout */
	to->next = this;
	prev->next = to;

	if (to == hrti->timer_list.next) {
		hrti->set_timer(hrti->ctx, to->timeout);
		hrti->timer_armed = TRUE;
	}

	return TRUE;
}

/* Remove specified timeout from the list */
void
wlc_hrt_del_timeout(wlc_hrt_to_t *to)
{
	wlc_hrt_info_t *hrti = to->hrti;
	wlc_hrt_to_t *this;
	wlc_hrt_to_t *prev = &hrti->timer_list;

	while (((this = prev->next) != NULL)) {
		if (this == to) {
			if (this->next != NULL)
				this->next->timeout += this->timeout;
			prev->next = this->next;
			this->fun = NULL;
			break;
		}
		prev = this;
	}
}

/* s/w timer object alloc/free */
wlc_hrt_to_t *
wlc_hrt_alloc_timeout(wlc_hrt_info_t *hrti)
{
	wlc_info_t *wlc = hrti->wlc;
	wlc_hrt_to_t *to;

	if ((to = (wlc_hrt_to_t *)
	     wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_hrt_to_t))) == NULL) {
		WL_ERROR(("%s: failed to allocate memory for hrtime timeout obj\n",
		          __FUNCTION__));
		return NULL;
	}
	to->hrti = hrti;

	return to;
}

void
wlc_hrt_free_timeout(wlc_hrt_to_t *to)
{
	wlc_hrt_info_t *hrti = to->hrti;
	wlc_info_t *wlc = hrti->wlc;

	MFREE(wlc->osh, to, sizeof(wlc_hrt_to_t));
}

/* accessors */
uint
wlc_hrt_gettime(wlc_hrt_info_t *hrti)
{
	ASSERT(hrti->get_time != NULL);

	return hrti->get_time(hrti->ctx);
}

uint
wlc_hrt_getdelta(wlc_hrt_info_t *hrti, uint *last)
{
	uint curr_time, last_time;
	uint delta;

	ASSERT(last != NULL);

	curr_time = wlc_hrt_gettime(hrti);
	last_time = *last;
	/* put in condition to check for wrap around */
	if (curr_time < last_time) {
		delta = hrti->max_timer_val - last_time + curr_time + 1;
	}
	else {
		delta = curr_time - last_time;
	}
	*last = curr_time;

	return delta;
}

/* gptimer specific helper functions for setting the
 * get_time, set_timer, ack_timer functions in the
 * wlc_hrt_info_t object for wlc->hrti.
 */
/* GP timer is a freerunning 32 bit counter, decrements at 1 us rate */
static void
wlc_hrt_gptimer_set(wlc_info_t *wlc, uint us)
{
	ASSERT(wlc->pub->corerev >= 3); /* no gptimer in earlier revs */

	W_REG(wlc->osh, &wlc->regs->gptimer, us);
}

static uint
wlc_hrt_gptimer_gettime(void *ctx)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	return (uint)R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
}

static void
wlc_hrt_gptimer_settimer(void *ctx, uint timeout)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_hrt_gptimer_set(wlc, timeout);
}

static void
wlc_hrt_gptimer_ack(void *ctx)
{
	wlc_info_t *wlc = (wlc_info_t *)ctx;
	wlc_hrt_gptimer_set(wlc, 0);
}

void wlc_hrt_gptimer_abort(wlc_info_t *wlc) {}
void wlc_hrt_gptimer_cb(wlc_info_t *wlc) {}
