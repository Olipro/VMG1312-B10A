/*
 * DSLCPE exported functions and definitions
 *
 * Copyright 2007, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 */

#ifndef _wl_linux_dslcpe_h_
#define _wl_linux_dslcpe_h_

#ifndef DSL_VERSION_MAJOR_CODE
#define DSL_VERSION_MAJOR_CODE ((DSL_LINUX_VERSION_CODE>>8)<<8)
#endif

#ifndef DSL_VERSION
#define DSL_VERSION(a,b,c) (((a) << 16) + ((b) << 8) + (c))
#endif

#ifndef DSL_VERSION_MAJOR
#define DSL_VERSION_MAJOR(a,b) (((a) << 16) + ((b) << 8))
#endif

struct wl_info;

typedef void (*dslcpe_setup_wlan_led_t)(void *config, int led_idx, int pin, int func, int act_hi);

extern int __devinit wl_dslcpe_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
extern int wl_dslcpe_open(struct net_device *dev);
extern int wl_dslcpe_close(struct net_device *dev);
extern void wlc_dslcpe_boardflags(uint32 *boardflags, uint32 *boardflags2);
extern void wlc_dslcpe_led_attach(void *config, dslcpe_setup_wlan_led_t setup);
extern void wlc_dslcpe_led_detach(void);
extern void wlc_dslcpe_timer_led_blink_timer(void);
extern void wlc_dslcpe_led_timer(void);
extern void wl_dslcpe_led(unsigned char state);
extern void wl_shutdown_handler(struct wl_info *wl);
extern void wl_reset_cnt(struct net_device *dev);

extern int wl_config_check(void);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 11)
extern int wl_suspend(struct pci_dev *pdev, DRV_SUSPEND_STATE_TYPE  state);
#else
extern int wl_suspend(struct pci_dev *pdev, u32 state);
#endif

#ifdef DSLCPE_SDIO
extern void * wl_sdh_get(struct wl_info *wl);
extern int wl_sdio_register(uint16 venid, uint16 devid, void *regsva, void *param, int irq);
extern int wl_sdio_unregister(void);
#endif

typedef struct {
	atomic_t	pktbuffered; /* Buffered prealloced pkt*/
	int		band; /* which band this interafce is working on. Not used now.*/
	int 		exist; /* whether the interafce is up?*/
} wl_wmark_t;

extern wl_wmark_t wl_wmark[2];
extern void wl_wmark_up(int unit);
extern void wl_wmark_down(int unit);
extern bool wl_pkt_drop_on_high_wmark_check(uint unit);
extern bool wl_pkt_drop_on_low_wmark_check(uint unit);

#endif /* _wl_linux_dslcpe_h_ */
