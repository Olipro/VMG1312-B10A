/*
 * Timer functions used by EMFL. These Functions can be moved to
 * shared/linux_osl.c, include/linux_osl.h
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: osl_linux.c 241182 2011-02-17 21:50:03Z $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include "osl_linux.h"


#ifdef DSLCPE
#include <siutils.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_pub.h>
#include <wl_export.h>
#include <wl_linux.h>
#endif

#ifdef DSLCPE
osl_timer_t *
igsc_osl_timer_init(wl_info_t *wl, const char *name, void (*fn)(void *arg), void *arg)
{
	ASSERT(wl);
	return (osl_timer_t *) wl_init_timer(wl, fn, arg,name);
}

void
igsc_osl_timer_add(wl_info_t *wl, osl_timer_t *t, uint32 ms, bool periodic)
{
	ASSERT(wl);
	wl_add_timer(wl, t,  ms, periodic);
}

void
igsc_osl_timer_update(wl_info_t *wl, osl_timer_t *t, uint32 ms, bool periodic)
{
	ASSERT(wl);
	wl_del_timer(wl, t);
	wl_add_timer(wl, t,  ms, periodic);
}
bool
igsc_osl_timer_del(wl_info_t *wl, osl_timer_t *t) 
{  	
	ASSERT(wl);
	return (wl_del_timer(wl, t));
}

#else

static void
osl_timer(ulong data)
{
	osl_timer_t *t;

	t = (osl_timer_t *)data;

	ASSERT(t->set);

	if (t->periodic) {
#if defined(BCMJTAG) || defined(BCMSLTGT)
		t->timer.expires = jiffies + t->ms*HZ/1000*htclkratio;
#else
		t->timer.expires = jiffies + t->ms*HZ/1000;
#endif /* defined(BCMJTAG) || defined(BCMSLTGT) */
		add_timer(&t->timer);
		t->set = TRUE;
		t->fn(t->arg);
	} else {
		t->set = FALSE;
		t->fn(t->arg);
#ifdef BCMDBG
		if (t->name) {
			MFREE(NULL, t->name, strlen(t->name) + 1);
		}
#endif
		MFREE(NULL, t, sizeof(osl_timer_t));
	}

	return;
}

osl_timer_t *
osl_timer_init(const char *name, void (*fn)(void *arg), void *arg)
{
	osl_timer_t *t;

	if ((t = MALLOC(NULL, sizeof(osl_timer_t))) == NULL) {
		printk(KERN_ERR "osl_timer_init: out of memory, malloced %d bytes\n",
		       sizeof(osl_timer_t));
		return (NULL);
	}

	bzero(t, sizeof(osl_timer_t));

	t->fn = fn;
	t->arg = arg;
	t->timer.data = (ulong)t;
	t->timer.function = osl_timer;
#ifdef BCMDBG
	if ((t->name = MALLOC(NULL, strlen(name) + 1)) != NULL) {
		strcpy(t->name, name);
	}
#endif

	init_timer(&t->timer);

	return (t);
}

void
osl_timer_add(osl_timer_t *t, uint32 ms, bool periodic)
{
	ASSERT(!t->set);

	t->set = TRUE;
	t->ms = ms;
	t->periodic = periodic;
#if defined(BCMJTAG) || defined(BCMSLTGT)
	t->timer.expires = jiffies + ms*HZ/1000*htclkratio;
#else
	t->timer.expires = jiffies + ms*HZ/1000;
#endif /* defined(BCMJTAG) || defined(BCMSLTGT) */

	add_timer(&t->timer);

	return;
}

void
osl_timer_update(osl_timer_t *t, uint32 ms, bool periodic)
{
	ASSERT(t->set);

	t->ms = ms;
	t->periodic = periodic;
	t->set = TRUE;
#if defined(BCMJTAG) || defined(BCMSLTGT)
	t->timer.expires = jiffies + ms*HZ/1000*htclkratio;
#else
	t->timer.expires = jiffies + ms*HZ/1000;
#endif /* defined(BCMJTAG) || defined(BCMSLTGT) */

	mod_timer(&t->timer, t->timer.expires);

	return;
}

/*
 * Return TRUE if timer successfully deleted, FALSE if still pending
 */
bool
osl_timer_del(osl_timer_t *t)
{
	if (t->set) {
		t->set = FALSE;
		if (!del_timer(&t->timer)) {
			printk(KERN_INFO "osl_timer_del: Failed to delete timer\n");
			return (FALSE);
		}
#ifdef BCMDBG
		if (t->name) {
			MFREE(NULL, t->name, strlen(t->name) + 1);
		}
#endif
		MFREE(NULL, t, sizeof(osl_timer_t));
	}

	return (TRUE);
}
#endif /* DSLCPE */

#ifdef DSLCPE
osl_lock_t
OSL_LOCK_CREATE(uint8 *name)
{
	osl_lock_t lock;

	lock = MALLOC(NULL, sizeof(osl_lock_t));

	if (lock == NULL)
	{
		printf("Memory alloc for lock object failed\n");
		return (NULL);
	}

	spin_lock_init(&lock->slock);
	strncpy(lock->name, name, 16);

	return (lock);
}

void
OSL_LOCK_DESTROY(osl_lock_t lock)
{
	MFREE(NULL, lock, sizeof(osl_lock_t));
	return;
}

void OSL_LOCK(osl_lock_t lock)
{
	return;
}
void OSL_UNLOCK(osl_lock_t lock)
{
	return;
}
char *DEV_IFNAME(void *dev) 
{
	return (((struct net_device *)dev)->name);
}

void *osl_pci_get_drvdata(struct pci_dev *pdev)
{
        return (pci_get_drvdata(pdev));
}

#endif /* DSLCPE */
