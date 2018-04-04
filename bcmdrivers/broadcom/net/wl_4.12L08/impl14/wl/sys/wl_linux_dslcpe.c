/*
 * DSLCPE specific functions
 *
 * Copyright 2007, Broadcom Corporation
 * All Rights Reserved.
 *
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id$
 */

#include <typedefs.h>
#include <linuxver.h>

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/netdevice.h>

#include <osl.h>

#include <wlioctl.h>

#include "bcm_map.h"
#include "bcm_intr.h"
#include "board.h"
#include "bcmnet.h"
#include "boardparms.h"
#include <wl_linux_dslcpe.h>

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
typedef struct wl_info wl_info_t;
extern int wl_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd);
extern int __devinit wl_pci_probe(struct pci_dev *pdev, const struct pci_device_id *ent);
extern void wl_free(wl_info_t *wl);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 20)
extern irqreturn_t wl_isr(int irq, void *dev_id);
#else
extern irqreturn_t wl_isr(int irq, void *dev_id, struct pt_regs *ptregs);
#endif

static struct net_device_ops wl_dslcpe_netdev_ops;
#endif
#include <bcmendian.h>
#include <bcmdevs.h>

#ifdef DSLCPE_SDIO
#include <bcmsdh.h>
extern struct wl_info *sdio_dev;
void *bcmsdh_osh = NULL;
#endif /* DSLCPE_SDIO */

unsigned char brcm_smp = 0;
/* USBAP */
#ifdef BCMDBUS
#include "dbus.h"
/* BMAC_NOTES: Remove, but just in case your Linux system has this defined */
#undef CONFIG_PCI
void *wl_dbus_probe_cb(void *arg, const char *desc, uint32 bustype, uint32 hdrlen);
void wl_dbus_disconnect_cb(void *arg);
#endif

#ifdef BCMDBG
extern int msglevel;
#endif
extern struct pci_device_id wl_id_table[];

extern bool
wlc_chipmatch(uint16 vendor, uint16 device);

#ifdef WMF
extern int32 emfc_module_init(void);
extern void emfc_module_exit(void);
#endif

struct net_device *
wl_netdev_get(struct wl_info *wl);
extern void wl_reset_cnt(struct net_device *dev);

#if defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE)
#include <linux/gbpm.h>
#endif

#define WL_FIFO_WMARK_POLICY
wl_wmark_t wl_wmark[2]; /* wmark structure */
int high_wmark_tot = 400;
#define WL_LOW_WMARK_DELTA  16
int wmark_min = 0;

#define WL_WMARK_DELTA	10
#define WL_WMARK_MIN		1

/* mark wlx is up */
void wl_wmark_up(int unit)
{
	wl_wmark[unit].exist = 1;
}

/* mark wlx is down or non-exist */
void wl_wmark_down(int unit)
{
	wl_wmark[unit].exist = 0;
}

#ifdef WL_FIFO_WMARK_POLICY
/* 
two adapter shares one skb buffer mark.
cnt0 = number of skb queueed in wl0 txq
cnt1 = number of skb queueed in wl0 txq
cnt0+cnt1 < wmark_tot; To make sure not over the watermark
*/
bool wl_pkt_drop_on_high_wmark_check(uint unit)
{
	int cnt0, cnt1;
	bool ret = TRUE;

	/* only wl0 and wl1 */
	ASSERT( unit==0 || unit ==1 );

	cnt0 = atomic_read(&(wl_wmark[0].pktbuffered));
	cnt1 = atomic_read(&(wl_wmark[1].pktbuffered));
	if (cnt0 + cnt1 < high_wmark_tot)
		ret = FALSE;

	/* enqueue pkt */
	return ret;
}

bool wl_pkt_drop_on_low_wmark_check(uint unit)
{
	int cnt0, cnt1;
	bool ret = TRUE;

	/* only wl0 and wl1 */
	ASSERT( unit==0 || unit ==1 );

	cnt0 = atomic_read(&(wl_wmark[0].pktbuffered));
	cnt1 = atomic_read(&(wl_wmark[1].pktbuffered));
	if (cnt0 + cnt1 < (high_wmark_tot-WL_LOW_WMARK_DELTA))
		ret = FALSE;

	/* enqueue pkt */
	return ret;
}
#endif /* WL_FIFO_WMARK_POLICY */

#ifdef WL_STATIC_WMARK_POLICY
/* 
each adapter allocate half of the skb buffers
cnt = number of skb queueed in wl0 txq
cnt < wmark_tot/2; To make sure not over the watermark
*/
bool wl_pkt_drop_on_high_wmark_check(uint unit)
{
	int cnt;
	bool ret = TRUE;

	/* only wl0 and wl1 */
	ASSERT( unit==0 || unit ==1 );

	cnt = atomic_read(&(wl_wmark[unit].pktbuffered));

	if (unlikely(wl_wmark[1-unit].exist)) {
		if (cnt < high_wmark_tot/2)
			ret = FALSE;
	} 
	else 
		if (cnt < high_wmark_tot)
			ret = FALSE;

	/* enqueue pkt */
	return ret;
}

bool wl_pkt_drop_on_low_wmark_check(uint unit)
{
	int cnt;
	bool ret = TRUE;

	/* only wl0 and wl1 */
	ASSERT( unit==0 || unit ==1 );

	cnt = atomic_read(&(wl_wmark[unit].pktbuffered));

	if (unlikely(wl_wmark[1-unit].exist)) {
		if (cnt < ((high_wmark_tot-WL_LOW_WMARK_DELTA)/2))
			ret = FALSE;
	} 
	else 
		if (cnt < (high_wmark_tot-WL_LOW_WMARK_DELTA))
			ret = FALSE;

	/* enqueue pkt */
	return ret;
}
#endif /* WL_STATIC_WMARK_POLICY */

/*
 * wl_dslcpe_open:
 * extended hook for device open for DSLCPE.
 */
int wl_dslcpe_open(struct net_device *dev)
{
	return 0;
}

/*
 * wl_dslcpe_close:
 * extended hook for device close for DSLCPE.
 */
int wl_dslcpe_close(struct net_device *dev)
{
	return 0;
}
/*
 * wlc_dslcpe_boardflags:
 * extended hook for modifying boardflags for DSLCPE.
 */
void wlc_dslcpe_boardflags(uint32 *boardflags, uint32 *boardflags2)
{
	return;
}

/*
 * wlc_dslcpe_led_attach:
 * extended hook for when led is to be initialized for DSLCPE.
 */

void wlc_dslcpe_led_attach(void *config, dslcpe_setup_wlan_led_t setup_dslcpe_wlan_led)
{
#if defined (CONFIG_BCM96816)
	setup_dslcpe_wlan_led(config, 0, 1, WL_LED_ACTIVITY, 0);
	setup_dslcpe_wlan_led(config, 1, 1, WL_LED_BRADIO, 0);
#else
	setup_dslcpe_wlan_led(config, 0, 0, WL_LED_ACTIVITY, 1);
	setup_dslcpe_wlan_led(config, 1, 1, WL_LED_BRADIO, 1);
#endif
	return;
}

/*
 * wlc_dslcpe_led_detach:
 * extended hook for when led is to be de-initialized for DSLCPE.
 */
void wlc_dslcpe_led_detach(void)
{
	return;
}
/*
 * wlc_dslcpe_timer_led_blink_timer:
 * extended hook for when periodical(10ms) led timer is called for DSLCPE when wlc is up.
 */
void wlc_dslcpe_timer_led_blink_timer(void)
{
	return;
}
/*
 * wlc_dslcpe_led_timer:
 * extended hook for when led blink timer(200ms) is called for DSLCPE when wlc is up.
 */
void wlc_dslcpe_led_timer(void)
{
	return;
}

/*
 * wl_dslcpe_ioctl:
 * extended ioctl support on BCM63XX.
 */
int
wl_dslcpe_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
	int isup = 0;
	int error = -1;

	if (cmd >= SIOCGLINKSTATE && cmd < SIOCLAST) {
		error = 0;
		/* we can add sub-command in ifr_data if we need to in the future */
		switch (cmd) {
			case SIOCGLINKSTATE:
				if (dev->flags&IFF_UP) isup = 1;
				if (copy_to_user((void*)(int*)ifr->ifr_data, (void*)&isup,
					sizeof(int))) {
					return -EFAULT;
				}
				break;
			case SIOCSCLEARMIBCNTR:
				wl_reset_cnt(dev);
				break;
		}
	} else {
		error = wl_ioctl(dev, ifr, cmd);
	}
	return error;
}

#if defined(DSLCPE_SDIO) || defined(CONFIG_PCI)
/* special deal for dslcpe */
int __devinit
wl_dslcpe_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
	struct wl_info *wl;
	struct net_device *dev;

#ifdef DSLCPE_SDIO
	wl = (struct wl_info *)sdio_dev;
#else
	if (wl_pci_probe(pdev, ent))
		return -ENODEV;

	wl = pci_get_drvdata(pdev);
#endif
	ASSERT(wl);

	/* hook ioctl */
	dev = wl_netdev_get(wl);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
    /* note -- this is sort of cheating, as we are changing
    * a pointer in a shared global structure, but... this should
    * work, as we are likely not to mix dslcpe wl's with non-dslcpe wl;s.
    * as well, it prevents us from having to export some symbols we don't
    * want to export.  A proper fix might be to add this to the
    * wlif structure, and point netdev ops there.
    */
    memcpy(&wl_dslcpe_netdev_ops, dev->netdev_ops, sizeof(struct net_device_ops));
    wl_dslcpe_netdev_ops.ndo_do_ioctl = wl_dslcpe_ioctl;
    dev->netdev_ops = &wl_dslcpe_netdev_ops;
#else
	ASSERT(dev);
	ASSERT(dev->do_ioctl);
	dev->do_ioctl = wl_dslcpe_ioctl;
#endif

#ifdef DSLCPE_DGASP
	kerSysRegisterDyingGaspHandler(dev->name, &wl_shutdown_handler, wl);
#endif

	return 0;
}

#ifndef DSLCPE_SDIO
void __devexit wl_remove(struct pci_dev *pdev);

static struct pci_driver wl_pci_driver = {
	name:		"wl",
	probe:		wl_dslcpe_probe,
	remove:		__devexit_p(wl_remove),
	id_table:	wl_id_table,
	};
#endif
#endif  /* defined(DSLCPE_SDIO) || defined(CONFIG_PCI) */

/* USBAP  Could combined with wl_dslcpe_probe */
#ifdef BCMDBUS
static void *wl_dslcpe_dbus_probe_cb(void *arg, const char *desc, uint32 bustype, uint32 hdrlen)
{
	struct net_device *dev;
	wl_info_t *wl = wl_dbus_probe_cb(arg, desc, bustype, hdrlen);
	int irq;

	ASSERT(wl);

	/* hook ioctl */
	dev = wl_netdev_get(wl);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 30)
    /* note -- this is sort of cheating, as we are changing
    * a pointer in a shared global structure, but... this should
    * work, as we are likely not to mix dslcpe wl's with non-dslcpe wl;s.
    * as well, it prevents us from having to export some symbols we don't
    * want to export.  A proper fix might be to add this to the
    * wlif structure, and point netdev ops there.
    */
    memcpy(&wl_dslcpe_netdev_ops, dev->netdev_ops, sizeof(struct net_device_ops));
    wl_dslcpe_netdev_ops.ndo_do_ioctl = wl_dslcpe_ioctl;
    dev->netdev_ops = &wl_dslcpe_netdev_ops;
#else
	ASSERT(dev);
	ASSERT(dev->do_ioctl);
	dev->do_ioctl = wl_dslcpe_ioctl;
#endif

#ifdef DSLCPE_DGASP
	kerSysRegisterDyingGaspHandler(dev->name, &wl_shutdown_handler, wl);
#endif
	return 0;
}

static void wl_dslcpe_dbus_disconnect_cb(void *arg)
{
	wl_dbus_disconnect_cb(arg);
}
#endif /* BCMDBUS */

#ifdef DSLCPE_CACHE_SMARTFLUSH
extern uint dsl_tx_pkt_flush_len;
#endif

static int __init
wl_module_init(void)
{

	int error;
	int i;
#ifdef CONFIG_SMP
       printk("--SMP support\n");
	brcm_smp = 1;
#endif
#ifdef CONFIG_BCM_WAPI
       printk("--WAPI support\n");
#endif

#if defined(DSLCPE_CACHE_SMARTFLUSH) && defined(PKTDIRTYPISPRESENT)
	printk("wl: dsl_tx_pkt_flush_len=%d\n", dsl_tx_pkt_flush_len);
#endif

#if defined(CONFIG_BCM_BPM) || defined(CONFIG_BCM_BPM_MODULE)
#ifdef CONFIG_GBPM_API_HAS_GET_TOTAL_BUFS
	high_wmark_tot = (int) (gbpm_get_total_bufs()*65/100);
#endif /* CONFIG_BPM_API_HAS_GET_TOTAL_BUFS */
#endif
	printk("wl: high_wmark_tot=%d\n", high_wmark_tot);

	if (wl_config_check())
		return -1;
#ifdef BCMDBG
	if (msglevel != 0xdeadbeef) {
		/* wl_msg_level = msglevel; */
		printf("%s: msglevel set to 0x%x\n", __FUNCTION__, msglevel);
	}
#endif /* BCMDBG */

#ifdef WMF
	if (emfc_module_init()) {
		return -1;
	}
#endif 

	for (i=0; i<2; i++) {
	 	atomic_set(&(wl_wmark[i].pktbuffered), 0);
		wl_wmark_down(i);
	}
	wmark_min = (high_wmark_tot /WL_WMARK_DELTA)*WL_WMARK_MIN;

#ifdef DSLCPE_SDIO
	bcmsdh_osh = osl_attach(NULL, SDIO_BUS, FALSE);
	
	if (!(error = wl_sdio_register(VENDOR_BROADCOM, BCM4318_CHIP_ID, (void *)0xfffe2300, bcmsdh_osh, INTERRUPT_ID_SDIO))) {
		if((error = wl_dslcpe_probe(0, 0)) != 0) {	/* to hookup entry points or misc */
			osl_detach(bcmsdh_osh);
			return error;
		}
	} else {
		osl_detach(bcmsdh_osh);
	}

#endif  /* DSLCPE_SDIO */

#ifdef CONFIG_PCI
	if (!(error = pci_module_init(&wl_pci_driver)))
		return (0);
#endif /* CONFIG_PCI */

#ifdef BCMDBUS
	/* BMAC_NOTE: define hardcode number, why NODEVICE is ok ? */
	error = dbus_register(BCM_DNGL_VID, BCM_DNGL_BDC_PID, wl_dslcpe_dbus_probe_cb,
		wl_dslcpe_dbus_disconnect_cb, NULL, NULL, NULL);
	if (error == DBUS_ERR_NODEVICE) {
		error = DBUS_OK;
	}
#endif /* BCMDBUS */
	return (error);
}

static void __exit
wl_module_exit(void)
{
#ifdef DSLCPE_SDIO
	wl_sdio_unregister();
	osl_detach(bcmsdh_osh);
#endif /* DSLCPE_SDIO */

#ifdef CONFIG_PCI
	pci_unregister_driver(&wl_pci_driver);
#endif	/* CONFIG_PCI */

#ifdef BCMDBUS
	dbus_deregister();
#endif /* BCMDBUS */
#ifdef WMF
	emfc_module_exit();
#endif /* WMF */
}

/* Turn 63xx GPIO LED On(1) or Off(0) */
void wl_dslcpe_led(unsigned char state)
{
/*if WLAN LED is from 63XX GPIO Line, define compiler flag GPIO_LED_FROM_63XX
#define GPIO_LED_FROM_63XX
*/

#ifdef GPIO_LED_FROM_63XX
	BOARD_LED_STATE led;
	led = state? kLedStateOn : kLedStateOff;

	kerSysLedCtrl(kLedSes, led);
#endif
}

module_init(wl_module_init);
module_exit(wl_module_exit);
MODULE_LICENSE("Proprietary");
