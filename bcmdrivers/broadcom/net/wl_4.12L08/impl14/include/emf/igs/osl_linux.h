/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: osl_linux.h 347400 2012-07-26 17:29:23Z $
 */

#ifndef _OSL_LINUX_H_
#define _OSL_LINUX_H_

#include <linux/types.h>
#include <linux/timer.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>


#ifdef DSLCPE
#include <siutils.h>
#include <bcmutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_cfg.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wl_linux.h>
#include <wl_export.h>
#include <linux/pci.h>
#if defined(CONFIG_MIPS_BRCM) && defined(CONFIG_BR_MLD_SNOOP)
#include <net/ipv6.h>
#endif
#endif

struct osl_lock
{
	spinlock_t slock;       /* Spin lock */
	uint8      name[16];    /* Name of the lock */
};

typedef struct osl_lock *osl_lock_t;

#ifdef DSLCPE
extern osl_lock_t OSL_LOCK_CREATE(uint8 *name);
extern void OSL_LOCK_DESTROY(osl_lock_t lock);
extern void OSL_LOCK(osl_lock_t lock);
extern void OSL_UNLOCK(osl_lock_t lock);
extern char *DEV_IFNAME(void *dev) ;
#else

static inline osl_lock_t
OSL_LOCK_CREATE(uint8 *name)
{
	osl_lock_t lock;

	lock = MALLOC(NULL, sizeof(struct osl_lock));

	if (lock == NULL)
	{
		printf("Memory alloc for lock object failed\n");
		return (NULL);
	}

	strncpy(lock->name, name, sizeof(lock->name)-1);
	lock->name[ sizeof(lock->name)-1 ] = '\0';
	spin_lock_init(&lock->slock);

	return (lock);
}

static inline void
OSL_LOCK_DESTROY(osl_lock_t lock)
{
	MFREE(NULL, lock, sizeof(struct osl_lock));
	return;
}

#define OSL_LOCK(lock)          spin_lock_bh(&((lock)->slock))
#define OSL_UNLOCK(lock)        spin_unlock_bh(&((lock)->slock))

#define DEV_IFNAME(dev)         (((struct net_device *)dev)->name)
#endif /* DSLCPE */

#ifdef DSLCPE
typedef struct wl_timer osl_timer_t;
#else
typedef struct osl_timer {
	struct timer_list timer;
	void   (*fn)(void *);
	void   *arg;
	uint   ms;
	bool   periodic;
	bool   set;
#ifdef BCMDBG
	char    *name;          /* Desription of the timer */
#endif
} osl_timer_t;
#endif /* DSLCPE */

extern osl_timer_t *osl_timer_init(const char *name, void (*fn)(void *arg), void *arg);
extern void osl_timer_add(osl_timer_t *t, uint32 ms, bool periodic);
extern void osl_timer_update(osl_timer_t *t, uint32 ms, bool periodic);
extern bool osl_timer_del(osl_timer_t *t);
extern osl_lock_t OSL_LOCK_CREATE(uint8 *name);
extern void OSL_LOCK_DESTROY(osl_lock_t lock);

#ifdef DSLCPE
extern osl_timer_t *igsc_osl_timer_init(wl_info_t *wl, const char *name, void (*fn)(void *arg), void *arg);
extern void igsc_osl_timer_add(wl_info_t *wl, osl_timer_t *t, uint32 ms, bool periodic);
extern void igsc_osl_timer_update(wl_info_t *wl, osl_timer_t *t, uint32 ms, bool periodic);
extern bool igsc_osl_timer_del(wl_info_t *wl, osl_timer_t *t);
#define osl_timer_init(_x1, _x2, _x3)   igsc_osl_timer_init(igsc_info->wl, _x1, _x2, _x3)
#define osl_timer_add(_x1, _x2, _x3)    igsc_osl_timer_add(igsc_info->wl, _x1, _x2, _x3)
#define osl_timer_update(_x1, _x2, _x3) igsc_osl_timer_update(igsc_info->wl, _x1, _x2, _x3);
#define osl_timer_del(_x1) 				igsc_osl_timer_del(igsc_info->wl, _x1);

extern void *osl_pci_get_drvdata(struct pci_dev *pdev);
#endif

#endif /* _OSL_LINUX_H_ */
