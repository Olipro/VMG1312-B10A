/*
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wl_rte.c 364344 2012-10-23 19:47:13Z $
 */


#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <epivers.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <bcmdevs.h>
#include <wlioctl.h>

#include <proto/802.11.h>
#include <sbhndpio.h>
#include <sbhnddma.h>
#include <hnddma.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_channel.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_hw.h>
#include <wlc_hw_priv.h>
#include <wlc_bmac.h>
#include <bcmsrom_fmt.h>
#include <bcmsrom.h>
#ifdef MSGTRACE
#include <msgtrace.h>
#endif

#include <wl_export.h>

#include <wl_oid.h>
#include <wlc_led.h>

#ifdef WLPFN
#include <wl_pfn.h>
#endif 	/* WLPFN */
#include <wl_toe.h>
#include <wl_arpoe.h>
#include <wl_keep_alive.h>
#include <wlc_pkt_filter.h>

#if defined(CONFIG_WLU) || defined(ATE_BUILD)
#include "../exe/wlu_cmd.h"
#endif  /* CONFIG_WLU || ATE_BUILD */

#ifdef WLC_LOW_ONLY
#include <bcm_xdr.h>
#include <bcm_rpc_tp.h>
#include <bcm_rpc.h>
#include <wlc_rpc.h>

#include <wlc_channel.h>
#endif

#ifdef BCMDBG
#include <bcmutils.h>
#endif

#include <dngl_bus.h>
#include <dngl_wlhdr.h>

#define WL_IFTYPE_BSS	1
#define WL_IFTYPE_WDS	2


#if defined(PROP_TXSTATUS)
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#include <wlc_scb.h>
#endif
#if defined(WLNDOE)
#include <wl_ndoe.h>
#endif

#if defined(NWOE)
#include <wl_nwoe.h>
#endif

typedef struct hndrte_timer wl_timer;

struct wl_if {
	struct wlc_if *wlcif;
	hndrte_dev_t *dev;		/* virtual device */
	wl_arp_info_t   *arpi;      /* pointer to arp agent offload info */
#ifdef WLNDOE
	wl_nd_info_t	*ndi;
#endif
};

typedef struct wl_info {
	uint		unit;		/* device instance number */
	wlc_pub_t	*pub;		/* pointer to public wlc state */
	void		*wlc;		/* pointer to private common os-independent data */
	wlc_hw_info_t	*wlc_hw;
	hndrte_dev_t	*dev;		/* primary device */
	bool		link;		/* link state */
	uint8		hwfflags;	/* host wake up filter flags */
	hndrte_stats_t	stats;
	wl_oid_t	*oid;		/* oid handler state */
	hndrte_timer_t  dpcTimer;	/* 0 delay timer used to schedule dpc */
#ifdef WLPFN
	wl_pfn_info_t	*pfn;		/* pointer to prefered network data */
#endif /* WLPFN */
	wl_toe_info_t	*toei;		/* pointer to toe specific information */
	wl_arp_info_t	*arpi;		/* pointer to arp agent offload info */
	wl_keep_alive_info_t	*keep_alive_info;	/* pointer to keep-alive offload info */
	wlc_pkt_filter_info_t	*pkt_filter_info;	/* pointer to packet filter info */
#if defined(PROP_TXSTATUS)
	wlfc_info_state_t*	wlfc_info;
#endif
#ifdef WLC_LOW_ONLY
	rpc_info_t 	*rpc;		/* RPC handle */
	rpc_tp_info_t	*rpc_th;	/* RPC transport handle */
	wlc_rpc_ctx_t	rpc_dispatch_ctx;
	bool dpc_stopped;	/* stop wlc_dpc() flag */
	bool dpc_requested;	/* request to wlc_dpc() */
#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
	hndrte_lowmem_free_t lowmem_free_info;
#endif
#endif /* WLC_LOW_ONLY */
#ifdef WLNDOE
	wl_nd_info_t	*ndi; 	/* Neighbor Advertisement Offload for IPv6 */
#endif
#ifdef NWOE
	wl_nwoe_info_t  *nwoei;		/* pointer to the network offload engine info */
#endif /* NWOE */
} wl_info_t;


#define WL_IF(wl, dev)	(((hndrte_dev_t *)(dev) == ((wl_info_t *)(wl))->dev) ? \
			 NULL : \
			 *(wl_if_t **)((hndrte_dev_t *)(dev) + 1))

#ifdef WLC_LOW_ONLY

/* Minimal memory requirement to do wlc_dpc. This is critical for BMAC as it cannot
 * lose frames
 * This value is based on perceived DPC need. Note that it accounts for possible
 * fragmentation  where sufficient memory does not mean getting contiguous allocation
 */

#define MIN_DPC_MEM	((RXBND + 6)* 2048)

#endif /* WLC_LOW_ONLY */

/* host wakeup filter flags */
#define HWFFLAG_UCAST	1		/* unicast */
#define HWFFLAG_BCAST	2		/* broadcast */

/* iovar table */
enum {
	IOV_HWFILTER,		/* host wakeup filter */
	IOV_DEEPSLEEP,		/* Deep sleep mode */
	IOV_LAST 		/* In case of a need to check max ID number */
};

static const bcm_iovar_t wl_iovars[] = {
	{"hwfilter", IOV_HWFILTER, 0, IOVT_BUFFER, 0},
	{"deepsleep", IOV_DEEPSLEEP, 0, IOVT_BOOL, 0},
	{NULL, 0, 0, 0, 0}
};

#ifdef PROP_TXSTATUS
static void wlfc_sendup_timer(void* arg);
int wlfc_initialize(wl_info_t *wl, wlc_info_t *wlc);
#endif /* PROP_TXSTATUS */

#if defined(CONFIG_WLU) || defined(ATE_BUILD)
/* forward prototype */
static void do_wl_cmd(uint32 arg, uint argc, char *argv[]);
#endif /* CONFIG_WLU || ATE_BUILD */
#ifdef BCMDBG
static void do_wlmsg_cmd(uint32 arg, uint argc, char *argv[]);
#endif
#ifdef WLC_HIGH
static void wl_statsupd(wl_info_t *wl);
#endif
static void wl_timer_main(hndrte_timer_t *t);
#ifdef WLC_HIGH
static int wl_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
                      void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);
#endif

/* Driver entry points */
void *wl_probe(hndrte_dev_t *dev, void *regs, uint bus,
	uint16 devid, uint coreid, uint unit);
static void wl_free(wl_info_t *wl, osl_t *osh);
static void wl_isr(hndrte_dev_t *dev);
static void _wl_dpc(hndrte_timer_t *timer);
static void wl_dpc(wl_info_t *wl);
static int wl_open(hndrte_dev_t *dev);
static int wl_send(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb);
static int wl_close(hndrte_dev_t *dev);

#ifndef WLC_LOW_ONLY
static int wl_ioctl(hndrte_dev_t *dev, uint32 cmd, void *buf, int len, int *used, int *needed,
	int set);
static bool _wl_rte_oid_check(wl_info_t *wl, uint32 cmd, void *buf, int len, int *used,
	int *needed, bool set, int *status);
#else
static void wl_rpc_down(void *wlh);
static void wl_rpc_resync(void *wlh);

static void wl_rpc_tp_txflowctl(hndrte_dev_t *dev, bool state, int prio);
static void wl_rpc_txflowctl(void *wlh, bool on);

#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
static void wl_lowmem_free(void *wlh);
#endif
static void wl_rpc_bmac_dispatch(void *ctx, struct rpc_buf* buf);

static void do_wlhist_cmd(uint32 arg, uint argc, char *argv[]);
static void do_wldpcdump_cmd(uint32 arg, uint argc, char *argv[]);
#endif /* WLC_LOW_ONLY */

#ifdef WLC_HIGH
static void _wl_toe_send_proc(wl_info_t *wl, void *p);
static int _wl_toe_recv_proc(wl_info_t *wl, void *p);
#endif /* WLC_HIGH */

static hndrte_devfuncs_t wl_funcs = {
#if defined(BCMROMSYMGEN_BUILD) || !defined(BCMROMBUILD)
	probe:		wl_probe,
#endif
	open:		wl_open,
	close:		wl_close,
	xmit:		wl_send,
#ifdef WLC_LOW_ONLY
	txflowcontrol:	wl_rpc_tp_txflowctl,
#else
	ioctl:		wl_ioctl,
#endif /* WLC_LOW_ONLY */
	poll:		wl_isr
};

hndrte_dev_t bcmwl = {
	name:		"wl",
	funcs:		&wl_funcs
};

#ifdef WLC_LOW_ONLY
#endif /* WLC_LOW_ONLY */

#ifdef ATE_BUILD
#define ATE_CMD_STR_LEN_MAX	100
#define WLC_ATE_CMD_PARAMS_MAX 10
#define ATE_CMDS_NUM_MAX 100
extern hndrte_dev_t *dev_list;
static int wl_ate_cmd_preproc(char **argv, char *cmd_str);
static void wl_ate_gpio_interrupt(uint32 stat, void *arg);
static void wl_ate_wlup(wl_info_t *wl);
void wl_ate_cmd_proc(void);
void wl_ate_init(si_t *sih);
void wlc_stf_phy_txant_upd(wlc_info_t *wlc);
ate_params_t ate_params;
#endif /* ATE_BUILD */

#ifdef WLC_HIGH
static int
wl_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
           void *p, uint plen, void *arg, int alen, int vsize, struct wlc_if *wlcif)
{

	wl_info_t *wl = (wl_info_t *)hdl;
	wlc_info_t *wlc = wl->wlc;
	int32 int_val = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	int err = 0;
	int radio;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));
	bool_val = (int_val != 0) ? TRUE : FALSE;

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	switch (actionid) {
	case IOV_GVAL(IOV_HWFILTER):
		*ret_int_ptr = wl->hwfflags;
		break;

	case IOV_SVAL(IOV_HWFILTER):
		wl->hwfflags = (uint8)int_val;
		break;

	case IOV_GVAL(IOV_DEEPSLEEP):
		if ((err = wlc_get(wlc, WLC_GET_RADIO, &radio)))
			break;
		*ret_int_ptr = (radio & WL_RADIO_SW_DISABLE) ? TRUE : FALSE;
		break;

	case IOV_SVAL(IOV_DEEPSLEEP):
		wlc_set(wlc, WLC_SET_RADIO, (WL_RADIO_SW_DISABLE << 16)
		        | (bool_val ? WL_RADIO_SW_DISABLE : 0));
		/* suspend or resume timers */
		if (bool_val)
			hndrte_suspend_timer();
		else
			hndrte_resume_timer();
		break;

	default:
		err = BCME_UNSUPPORTED;
	}

	return err;
}

static void
BCMINITFN(_wl_init)(wl_info_t *wl)
{
	wl_reset(wl);

	wlc_init(wl->wlc);
}

void
wl_init(wl_info_t *wl)
{
	WL_TRACE(("wl%d: wl_init\n", wl->unit));

		_wl_init(wl);
}
#endif /* WLC_HIGH */

uint
BCMINITFN(wl_reset)(wl_info_t *wl)
{
	WL_TRACE(("wl%d: wl_reset\n", wl->unit));

	wlc_reset(wl->wlc);

	return 0;
}

bool
wl_alloc_dma_resources(wl_info_t *wl, uint addrwidth)
{
	return TRUE;
}

/*
 * These are interrupt on/off enter points.
 * Since wl_isr is serialized with other drive rentries using spinlock,
 * They are SMP safe, just call common routine directly,
 */
void
wl_intrson(wl_info_t *wl)
{
	wlc_intrson(wl->wlc);
}

uint32
wl_intrsoff(wl_info_t *wl)
{
	return wlc_intrsoff(wl->wlc);
}

void
wl_intrsrestore(wl_info_t *wl, uint32 macintmask)
{
	wlc_intrsrestore(wl->wlc, macintmask);
}

#ifdef ATE_BUILD
void
wl_ate_init(si_t *sih)
{
	void *handle;
	hndrte_dev_t *dev = dev_list;
	wl_info_t *wl = NULL;
	wlc_info_t *wlc = NULL;
	wlc_hw_info_t *wlc_hw = NULL;
	char ate_cmd_gpio_ip[12] = "ate_gpio_ip";

	/* Validate chip - ATE commands supported for 4336b1, 4330b1 and 4334a0 only */
	while (dev) {
		if ((dev->devid == BCM4336_D11N_ID) || (dev->devid == BCM4330_D11N_ID) ||
			(dev->devid == BCM4330_D11N2G_ID) || (dev->devid == BCM4334_D11N_ID) ||
			(dev->devid == BCM4334_D11N2G_ID) || (dev->devid == BCM4334_D11N5G_ID))
			break;
		dev = dev->next;
	}
	if (!dev) {
		printf("This chip is NOT supported for ATE operations!!!\n");
		ASSERT(FALSE);
	}
	wl = ate_params.wl = dev->softc;
	wlc = wl->wlc;
	wlc_hw = wlc->hw;

	/* Init ATE params */
	ate_params.cmd_proceed = TRUE;
	ate_params.ate_cmd_done = FALSE;
	ate_params.cmd_idx = 0;
	ate_params.gpio_input = 0xFF;
	ate_params.gpio_output = 0xFF;

	/* Configure the GPIOs IN */
	if (getvar(wlc->pub->vars, ate_cmd_gpio_ip)) {
		ate_params.gpio_input = (uint8)getintvar(NULL, ate_cmd_gpio_ip);

		printf("ate_params.gpio_input = %d\n", ate_params.gpio_input);
		/* Take over gpio control from cc */
		si_gpiocontrol(sih, (1 << ate_params.gpio_input),
			(1 << ate_params.gpio_input), GPIO_DRV_PRIORITY);

		/* Register the GPIO interrupt handler (FALSE = edge-detect). */
		handle = si_gpio_handler_register(sih, (1 << ate_params.gpio_input), FALSE,
			wl_ate_gpio_interrupt, (void *) &ate_params);

		/* make polarity opposite of the current value */
		si_gpiointpolarity(sih, (1 << ate_params.gpio_input),
			(si_gpioin(sih) & (1 << ate_params.gpio_input)), 0);

		/* always enabled */
		si_gpiointmask(sih, (1 << ate_params.gpio_input), (1 << ate_params.gpio_input), 0);
	}


	/* Bring up the wl */
	//if (0) /* Currently not operational */
		wl_ate_wlup(wl);

	/* Done with init */
	printf("ATE Init Done!!!\n");
}

static void
wl_ate_gpio_interrupt(uint32 stat, void *arg)
{
	ate_params_t *ap = (ate_params_t *)arg;
	ap->cmd_proceed = TRUE;
}

static int
wl_ate_cmd_preproc(char **argv, char *cmd_str)
{
	int param_count;
	char *array_ptr = cmd_str;

	ASSERT(strlen(cmd_str) <= ATE_CMD_STR_LEN_MAX);

	for (param_count = 0; param_count < WLC_ATE_CMD_PARAMS_MAX; param_count++)
		argv[param_count] = 0;

	for (param_count = 0; *array_ptr != '\0'; param_count++) {
		argv[param_count] = array_ptr;
		for (; (*array_ptr != '\0') && (*array_ptr != ' '); array_ptr++);
			if (*array_ptr == '\0')
				break;
			*array_ptr = '\0';
			array_ptr++;
	}

	return param_count + 1;
}

void
wlc$wlc_radio_hwdisable_upd(wlc_info_t* wlc);

static void
wl_ate_wlup(wl_info_t *wl)
{
	wlc_info_t *wlc = wl->wlc;

	wlc->down_override = FALSE;
	wlc->mpc_out = FALSE;
	wlc->mpc = FALSE;

	/* wl MPC 0 */
	wlc_radio_mpc_upd(wlc);

	/* wl up */
	wlc->clk = TRUE;
	wlc_radio_monitor_stop(wlc);
	wl_init(wlc->wl);
	wlc->pub->up = TRUE;
	wlc_bmac_up_finish(wlc->hw);


	/* ensure antenna config is up to date */
	wlc_stf_phy_txant_upd(wlc);
}

void
wl_ate_cmd_proc(void)
{
	char *ate_str = NULL;
	char *argv[WLC_ATE_CMD_PARAMS_MAX];
	uint8 argc = 0;
	char ate_cmd_str[10] = "ate_cmd";
	uint8 ate_cmd_str_len = strlen(ate_cmd_str);
	char ate_cmd_num[3];
	wl_info_t *wl = NULL;
	wlc_info_t *wlc = NULL;

	ASSERT(ate_params.wl);

	if ((ate_params.ate_cmd_done == TRUE) ||	/* All commands executed */
		(ate_params.cmd_proceed == FALSE))		/* Waiting for GPIO trigger */
		return;

	wl = ate_params.wl;
	wlc = wl->wlc;

	if (ate_params.cmd_idx == 0) {
		printf("\nATE CMD : START!!!\n");
		/* Be prepared for a Wait for INT ATE command */
		ate_params.cmd_proceed = FALSE;
	}

	do {
		sprintf(ate_cmd_num, "%02d", ate_params.cmd_idx);
		ate_cmd_str[ate_cmd_str_len] = '\0';
		strcat(ate_cmd_str, ate_cmd_num);
		ate_str = getvar(wlc->pub->vars, ate_cmd_str);

		if (ate_str) {
			printf("ATE CMD%02d: %s : ", ate_params.cmd_idx, ate_str);
			argc = wl_ate_cmd_preproc(argv, ate_str);

			if (strcmp(argv[0], "ate_cmd_wait_gpio_rising_edge") == 0) {
				/* Execute the ATE command */
				if (ate_params.cmd_proceed == TRUE) {
					ate_params.cmd_proceed = FALSE;
					/* Proceed with the next ATE command */
					ate_params.cmd_idx++;
					printf("\n");
					continue;
				}
				else {
					printf("Waiting for INT\n");
					return;
				}
			} else {
				if (argc > 1) {
					/* Execute the ATE wl command */
					do_wl_cmd((uint32)wlc->wl, argc, argv);
				} else {
					printf("ATE Command: Invalid command : %s. "
						"Num of params : %d\n", ate_str, argc);
				}
				ate_params.cmd_idx++;
			}
		} else {
			ate_params.cmd_idx++;
		}
	} while (ate_params.cmd_idx < ATE_CMDS_NUM_MAX);

	printf("ATE CMD : END!!!\n");

	/* All ATE commands done, update the variables accordingly */
	ate_params.cmd_idx = 0;
	ate_params.ate_cmd_done = TRUE;

	return;
}
#endif /* ATE_BUILD */

#ifdef WLC_HIGH
/* BMAC driver has alternative up/down etc. */
int
wl_up(wl_info_t *wl)
{
	int ret;
	wlc_info_t *wlc = (wlc_info_t *) wl->wlc;

	WL_TRACE(("wl%d: wl_up\n", wl->unit));

	if (wl->pub->up)
		return 0;


	if (wl->pub->up)
		return 0;

	/* Reset the hw to known state */
	ret = wlc_up(wlc);

	if (ret == 0)
		ret = wl_keep_alive_up(wl->keep_alive_info);

#ifndef RSOCK
	if (wl_oid_reclaim(wl->oid))
		hndrte_reclaim();

	if (POOL_ENAB(wl->pub->pktpool))
		pktpool_fill(hndrte_osh, wl->pub->pktpool, FALSE);
#endif

	return ret;
}

void
wl_down(wl_info_t *wl)
{
	WL_TRACE(("wl%d: wl_down\n", wl->unit));
	if (!wl->pub->up)
		return;

	wl_keep_alive_down(wl->keep_alive_info);
	wlc_down(wl->wlc);
	wl->pub->hw_up = FALSE;
}

void
wl_dump_ver(wl_info_t *wl, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "wl%d: %s %s version %s\n", wl->unit,
		__DATE__, __TIME__, EPI_VERSION_STR);
}

#if defined(BCMDBG) || defined(WLDUMP)
static int
wl_dump(wl_info_t *wl, struct bcmstrbuf *b)
{
	wl_dump_ver(wl, b);

	return 0;
}
#endif /* BCMDBG || WLDUMP */
#endif /* WLC_HIGH */

void
wl_monitor(wl_info_t *wl, wl_rxsts_t *rxsts, void *p)
{
#ifdef WL_MONITOR
	uint len;
	struct lbuf *mon_pkt;

	len = PKTLEN(wl->pub->osh, p) - D11_PHY_HDR_LEN + sizeof(rx_ctxt_t);

	if ((mon_pkt = PKTGET(wl->pub->osh, len, FALSE)) == NULL)
		return;

	PKTSETLEN(wl->pub->osh, mon_pkt, len - sizeof(rx_ctxt_t));

	bcopy(PKTDATA(wl->pub->osh, p) + D11_PHY_HDR_LEN,
		PKTDATA(wl->pub->osh, mon_pkt),
		len);
	wl_sendup(wl, NULL, mon_pkt, 1);
#endif /* WL_MONITOR */
}

void
wl_set_monitor(wl_info_t *wl, int val)
{
}

char *
wl_ifname(wl_info_t *wl, struct wl_if *wlif)
{
	if (wlif == NULL)
		return wl->dev->name;
	else
		return wlif->dev->name;
}

#if defined(AP) && defined(WLC_HIGH)
static hndrte_devfuncs_t*
get_wl_funcs(void)
{
	return &wl_funcs;
}

/* Allocate wl_if_t, hndrte_dev_t, and wl_if_t * all together */
static wl_if_t *
wl_alloc_if(wl_info_t *wl, int iftype, uint subunit, struct wlc_if *wlcif)
{
	hndrte_dev_t *dev;
	wl_if_t *wlif;
	osl_t *osh = wl->pub->osh;
	hndrte_dev_t *bus = wl->dev->chained;
	uint len;
	int ifindex;
	wl_if_t **priv;

	/* the primary device must be binded to the bus */
	if (bus == NULL) {
		WL_ERROR(("wl%d: %s: device not binded\n", wl->pub->unit, __FUNCTION__));
		return NULL;
	}

	/* allocate wlif struct + dev struct + priv pointer */
	len = sizeof(wl_if_t) + sizeof(hndrte_dev_t) + sizeof(wl_if_t **);
	if ((wlif = MALLOC(osh, len)) == NULL) {
		WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
		          (wl->pub)?wl->pub->unit:subunit, __FUNCTION__, MALLOCED(wl->pub->osh)));
		goto err;
	}
	bzero(wlif, len);

	dev = (hndrte_dev_t *)(wlif + 1);
	priv = (wl_if_t **)(dev + 1);

	wlif->dev = dev;
	wlif->wlcif = wlcif;

	dev->funcs = get_wl_funcs();
	dev->softc = wl;
	snprintf(dev->name, HNDRTE_DEV_NAME_MAX, "wl%d.%d", wl->pub->unit, subunit);

	*priv = wlif;

	/* use the return value as the i/f no. in the event to the host */
#ifdef DONGLEBUILD
	if ((ifindex = bus_ops->binddev(bus, dev)) < 1) {
		WL_ERROR(("wl%d: %s: bus_binddev failed\n", wl->pub->unit, __FUNCTION__));
		goto err;
	}
#else
	ifindex = subunit;
#endif
	wlcif->index = (uint8)ifindex;

	/* create and populate arpi for this IF */
	if (ARPOE_ENAB(wl->pub))
		wlif->arpi = wl_arp_alloc_ifarpi(wl->arpi, wlcif);

#ifdef WLNDOE
	if (NDOE_ENAB(wl->pub))
		wlif->ndi = wl_nd_alloc_ifndi(wl->ndi, wlcif);
#endif

	return wlif;

err:
	if (wlif != NULL)
		MFREE(osh, wlif, len);
	return NULL;
}

static void
wl_free_if(wl_info_t *wl, wl_if_t *wlif)
{

#ifdef ARPOE
	/* free arpi for this IF */
	if (ARPOE_ENAB(wl->pub))
		wl_arp_free_ifarpi(wlif->arpi);
#endif /* ARPOE */

#ifdef WLNDOE
	/* free ndi for this IF */
	if (NDOE_ENAB(wl->pub)) {
		wl_nd_free_ifndi(wlif->ndi);
	}
#endif
	MFREE(wl->pub->osh, wlif, sizeof(wl_if_t) + sizeof(hndrte_dev_t) + sizeof(wl_if_t *));
}

struct wl_if *
wl_add_if(wl_info_t *wl, struct wlc_if *wlcif, uint unit, struct ether_addr *remote)
{
	wl_if_t *wlif;

	wlif = wl_alloc_if(wl, remote != NULL ? WL_IFTYPE_WDS : WL_IFTYPE_BSS, unit, wlcif);

	if (wlif == NULL) {
		WL_ERROR(("wl%d: %s: failed to create %s interface %d\n", wl->pub->unit,
			__FUNCTION__, (remote)?"WDS":"BSS", unit));
		return NULL;
	}

	return wlif;
}

void
wl_del_if(wl_info_t *wl, struct wl_if *wlif)
{
#ifdef DONGLEBUILD
	hndrte_dev_t *bus = wl->dev->chained;

	if (bus_ops->unbinddev(bus, wlif->dev) < 1)
		WL_ERROR(("wl%d: %s: bus_unbinddev failed\n", wl->pub->unit, __FUNCTION__));
#endif
	WL_TRACE(("wl%d: %s: bus_unbinddev idx %d\n", wl->pub->unit, __FUNCTION__,
		wlif->wlcif->index));
	wl_free_if(wl, wlif);
}
#endif /* AP */

static void
wl_timer_main(hndrte_timer_t *t)
{
	ASSERT(t->context); ASSERT(t->auxfn);

	t->auxfn(t->data);
}

#undef wl_init_timer

struct wl_timer *
wl_init_timer(wl_info_t *wl, void (*fn)(void* arg), void *arg, const char *name)
{
	return (struct wl_timer *)hndrte_init_timer(wl, arg, wl_timer_main, fn);
}

void
wl_free_timer(wl_info_t *wl, struct wl_timer *t)
{
	hndrte_free_timer((hndrte_timer_t *)t);
}

void
wl_add_timer(wl_info_t *wl, struct wl_timer *t, uint ms, int periodic)
{
	ASSERT(t != NULL);
	hndrte_add_timer((hndrte_timer_t *)t, ms, periodic);
}

bool
wl_del_timer(wl_info_t *wl, struct wl_timer *t)
{
	if (t == NULL)
		return TRUE;
	return hndrte_del_timer((hndrte_timer_t *)t);
}

#ifdef PROP_TXSTATUS
#ifdef PROP_TXSTATUS_DEBUG
static void
hndrte_wlfc_info_dump(uint32 arg, uint argc, char *argv[])
{
	extern void wlfc_display_debug_info(void* _wlc, int hi, int lo);
	wlfc_info_state_t* wlfc = wlfc_state_get(((wlc_info_t*)arg)->wl);
	int hi = 0;
	int lo = 0;
	int i;

	if (argc > 2) {
		hi = atoi(argv[1]);
		lo = atoi(argv[2]);
	}
	printf("packets: (from_host,status_back,stats_other, credit_back, creditin) = "
		"(%d,%d,%d,%d,%d)\n",
		wlfc->stats.packets_from_host,
		wlfc->stats.txstatus_count,
		wlfc->stats.txstats_other,
		wlfc->stats.creditupdates,
		wlfc->stats.creditin);
	printf("credits for fifo: fifo[0-5] = (");
	for (i = 0; i < NFIFO; i++)
		printf("%d,", wlfc->stats.credits[i]);
	printf(")\n");
	printf("stats: (header_only_alloc, realloc_in_sendup): (%d,%d)\n",
		wlfc->stats.nullpktallocated,
		wlfc->stats.realloc_in_sendup);
	printf("wlc_toss, wlc_sup = (%d, %d)\n",
		wlfc->stats.wlfc_wlfc_toss,
		wlfc->stats.wlfc_wlfc_sup);
	printf("debug counts:for_D11,to_D11,free_exceptions:(%d,%d,%d)\n",
		(wlfc->stats.packets_from_host - (wlfc->stats.wlfc_wlfc_toss +
		wlfc->stats.wlfc_wlfc_sup)
		+ wlfc->stats.txstats_other),
		wlfc->stats.wlfc_to_D11,
		wlfc->stats.wlfc_pktfree_except);
#ifdef AP
	wlfc_display_debug_info((void*)arg, hi, lo);
#endif
	return;
}
#endif /* PROP_TXSTATUS_DEBUG */

int
BCMATTACHFN(wlfc_initialize)(wl_info_t *wl, wlc_info_t *wlc)
{
	wlc_tunables_t *tunables = wlc->pub->tunables;
	wl->wlfc_info = (wlfc_info_state_t*)MALLOC(wl->pub->osh, sizeof(wlfc_info_state_t));
	if (wl->wlfc_info == NULL) {
		WL_ERROR(("MALLOC() failed in %s() for wlfc_info", __FUNCTION__));
		goto fail;
	}
	memset(wl->wlfc_info, 0, sizeof(wlfc_info_state_t));

	wl->wlfc_info->fifo_credit_threshold[TX_AC_BE_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_be;
	wl->wlfc_info->fifo_credit_threshold[TX_AC_BK_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_bk;
	wl->wlfc_info->fifo_credit_threshold[TX_AC_VI_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_vi;
	wl->wlfc_info->fifo_credit_threshold[TX_AC_VO_FIFO] =
		tunables->wlfc_fifo_cr_pending_thresh_ac_vo;

	wlc->wlfc_data = MALLOC(wl->pub->osh, sizeof(wlfc_mac_desc_handle_map_t));
	if (wlc->wlfc_data == NULL) {
		WL_ERROR(("MALLOC() failed in %s() for wlfc_mac_desc_handle_map_t", __FUNCTION__));
		goto fail;
	}
	memset(wlc->wlfc_data, 0, sizeof(wlfc_mac_desc_handle_map_t));

	wlc->wlfc_vqdepth = WLFC_DEFAULT_FWQ_DEPTH;

	/* init and add a timer for periodic wlfc signal sendup */
	wl->wlfc_info->wl_info = wl;
	if (!(((wlfc_info_state_t*)wl->wlfc_info)->fctimer = wl_init_timer(wl,
		wlfc_sendup_timer, wl->wlfc_info, "wlfctimer"))) {
		WL_ERROR(("wl%d: wl_init_timer for wlfc timer failed\n", wl->pub->unit));
		goto fail;
	}
	wlc_eventq_set_ind(wlc->eventq, WLC_E_FIFO_CREDIT_MAP, TRUE);

#ifdef PROP_TXSTATUS_DEBUG
	wlc->wlfc_flags = WLFC_FLAGS_RSSI_SIGNALS | WLFC_FLAGS_XONXOFF_SIGNALS |
		WLFC_FLAGS_CREDIT_STATUS_SIGNALS;
	hndrte_cons_addcmd("np", (cons_fun_t)hndrte_wlfc_info_dump, (uint32)wlc);
#else
	/* All TLV s are turned off by default */
	wlc->wlfc_flags = 0;
#endif
	return BCME_OK;

fail:
	return BCME_ERROR;
}

static void
wlfc_sendup_timer(void* arg)
{
	wlfc_info_state_t* wlfc = (wlfc_info_state_t*)arg;

	wlfc->timer_started = 0;
	if (wlfc->pending_datalen) {
		wlfc_sendup_ctl_info_now(wlfc->wl_info);
	}
	return;
}

#ifdef PROP_TXSTATUS_DEBUG
static void
wlfc_hostpkt_callback(wlc_info_t *wlc, uint txstatus, void *p)
{
	/* space for type(1), length(1) and value */
	uint8 results[1+1+WLFC_CTL_VALUE_LEN_TXSTATUS];
	wlc_pkttag_t *pkttag;
	uint32 statusdata = 0;

	ASSERT(p != NULL);
	pkttag = WLPKTTAG(p);
	if (WL_TXSTATUS_GET_FLAGS(pkttag->wl_hdr_information) & WLFC_PKTFLAG_PKTFROMHOST) {
		WLFC_DBGMESG(("wlfc_info:[%08x], p:%p,(flag,hslot,seq)=(%02x,%04x,%02x)\n",
			pkttag->wl_hdr_information, p, (pkttag->wl_hdr_information >> 24),
			((pkttag->wl_hdr_information >> 8) & 0xffff),
			(pkttag->wl_hdr_information & 0xff)));
		results[0] = WLFC_CTL_TYPE_TXSTATUS;
		results[1] = WLFC_CTL_VALUE_LEN_TXSTATUS;

		WL_TXSTATUS_SET_PKTID(statusdata,
			WL_TXSTATUS_GET_PKTID(pkttag->wl_hdr_information));
		WL_TXSTATUS_SET_FLAGS(statusdata, WLFC_CTL_PKTFLAG_DISCARD);
		memcpy(&results[2], &statusdata, sizeof(uint32));
		wlfc_push_signal_data(wlc->wl, results, sizeof(results), FALSE);
		((wlfc_info_state_t *)((struct wl_info *)
			(wlc->wl))->wlfc_info)->stats.wlfc_pktfree_except++;
	}
}
#endif /* PROP_TXSTATUS_DEBUG */

uint8
wlfc_allocate_MAC_descriptor_handle(struct wlfc_mac_desc_handle_map* map)
{
	int i;

	for (i = 0; i < NBITS(uint32); i++) {
		if (!(map->bitmap & (1 << i))) {
			map->bitmap |= 1 << i;
			/* we would use 3 bits only */
			map->replay_counter++;
			/* ensure a non-zero replay counter value */
			if (!(map->replay_counter & 7))
				map->replay_counter = 1;
			return i | (map->replay_counter << 5);
		}
	}
	return WLFC_MAC_DESC_ID_INVALID;
}

void
wlfc_release_MAC_descriptor_handle(struct wlfc_mac_desc_handle_map* map, uint8 handle)
{

	if (handle < WLFC_MAC_DESC_ID_INVALID) {
		/* unset the allocation flag in bitmap */
		map->bitmap &= ~(1 << WLFC_MAC_DESC_GET_LOOKUP_INDEX(handle));
	}
	else {
	}
	return;
}

#endif /* PROP_TXSTATUS */

#if (BCMCHIPID > 0x9999)
static const char BCMATTACHDATA(rstr_fmt_hello)[] =
	"wl%d: Broadcom BCM%d 802.11 Wireless Controller %s\n";
#else
static const char BCMATTACHDATA(rstr_fmt_hello)[] =
	"wl%d: Broadcom BCM%04x 802.11 Wireless Controller %s\n";
#endif

void *
BCMATTACHFN(wl_probe)(hndrte_dev_t *dev, void *regs, uint bus, uint16 devid,
                      uint coreid, uint unit)
{
	wl_info_t *wl;
	wlc_info_t *wlc;
	osl_t *osh;
	uint err;

	/* allocate private info */
	if (!(wl = (wl_info_t *)MALLOC(NULL, sizeof(wl_info_t)))) {
		WL_ERROR(("wl%d: MALLOC failed\n", unit));
		return NULL;
	}
	bzero(wl, sizeof(wl_info_t));

	wl->unit = unit;

	osh = osl_attach(dev);

#ifdef WLC_LOW_ONLY
	wl->rpc_th = bcm_rpc_tp_attach(osh, dev);
	if (wl->rpc_th == NULL) {
		WL_ERROR(("wl%d: bcm_rpc_tp_attach failed\n", unit));
		goto fail;
	}
	bcm_rpc_tp_txflowctlcb_init(wl->rpc_th, wl, wl_rpc_txflowctl);

	wl->rpc = bcm_rpc_attach(NULL, NULL, wl->rpc_th, NULL);
	if (wl->rpc == NULL) {
		WL_ERROR(("wl%d: bcm_rpc_attach failed\n", unit));
		goto fail;
	}
#endif /* WLC_LOW_ONLY */

	/* common load-time initialization */
	if (!(wlc = wlc_attach(wl,			/* wl */
	                       VENDOR_BROADCOM,		/* vendor */
	                       devid,			/* device */
	                       unit,			/* unit */
	                       FALSE,			/* piomode */
	                       osh,			/* osh */
	                       regs,			/* regsva */
	                       bus,			/* bustype */
#ifdef WLC_LOW_ONLY
	                       wl->rpc,			/* BMAC, overloading, to change */
#else
			       wl,			/* sdh */
#endif
	                       &err))) {		/* perr */
		WL_ERROR(("wl%d: wlc_attach failed with error %d\n", unit, err));
		goto fail;
	}
	wl->wlc = (void *)wlc;
	wl->pub = wlc_pub((void *)wlc);
	wl->wlc_hw = wlc->hw;
	wl->dev = dev;
	wl->dpcTimer.mainfn = _wl_dpc;
	wl->dpcTimer.data = wl;

	snprintf(dev->name, HNDRTE_DEV_NAME_MAX, "wl%d", unit);

	/* print hello string */
	printf(rstr_fmt_hello, unit, wlc->hw->sih->chip, EPI_VERSION_STR);

#ifndef HNDRTE_POLLING
	if (hndrte_add_isr(0, coreid, unit, (isr_fun_t)wl_isr, dev, bus)) {
		WL_ERROR(("wl%d: hndrte_add_isr failed\n", unit));
		goto fail;
	}
#endif	/* HNDRTE_POLLING */

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		if (BCME_OK != wlfc_initialize(wl, wlc)) {
			WL_ERROR(("wl%d: wlfc_initialize failed\n", unit));
			goto fail;
		}
	}
#endif

#ifdef WLC_LOW_ONLY
	wl->rpc_dispatch_ctx.rpc = wl->rpc;
	wl->rpc_dispatch_ctx.wlc = wlc;
	wl->rpc_dispatch_ctx.wlc_hw = wlc->hw;
	bcm_rpc_rxcb_init(wl->rpc, &wl->rpc_dispatch_ctx, wl_rpc_bmac_dispatch, wl,
	                  wl_rpc_down, wl_rpc_resync, NULL);

	hndrte_cons_addcmd("wlhist", do_wlhist_cmd, (uint32)wl);
	hndrte_cons_addcmd("dpcdump", do_wldpcdump_cmd, (uint32)wl);

#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
	wl->lowmem_free_info.free_fn = wl_lowmem_free;
	wl->lowmem_free_info.free_arg = wl;

	hndrte_pt_lowmem_register(&wl->lowmem_free_info);
#endif /* HNDRTE_PT_GIANT && DMA_TX_FREE */

#else /* WLC_LOW_ONLY */

#ifdef STA
	/* algin watchdog with tbtt indication handling in PS mode */
	wl->pub->align_wd_tbtt = TRUE;

	/* Enable TBTT Interrupt */
	wlc_bmac_enable_tbtt(wlc->hw, TBTT_WD_MASK, TBTT_WD_MASK);
#endif

	wlc_eventq_set_ind(wlc->eventq, WLC_E_IF, TRUE);

	/* initialize OID handler state */
	if ((wl->oid = wl_oid_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_oid_attach failed\n", unit));
		goto fail;
	}

#if defined(WLPFN) && !defined(WLPFN_DISABLED)
	/* initialize PFN handler state */
	if ((wl->pfn = wl_pfn_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_pfn_attach failed\n", unit));
		goto fail;
	}
	wl->pub->_wlpfn = TRUE;
#endif /* WLPFN */

	/* allocate the toe info struct */
	if ((wl->toei = wl_toe_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_toe_attach failed\n", unit));
		goto fail;
	}

	/* allocate the arp info struct */
	if ((wl->arpi = wl_arp_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_arp_attach failed\n", unit));
		goto fail;
	}

	/* allocate the keep-alive info struct */
	if ((wl->keep_alive_info = wl_keep_alive_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_keep_alive_attach failed\n", unit));
		goto fail;
	}

	/* allocate the packet filter info struct */
	if ((wl->pkt_filter_info = wlc_pkt_filter_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wlc_pkt_filter_attach failed\n", unit));
		goto fail;
	}
#ifdef NWOE
	/* allocate the nwoe info struct */
	if ((wl->nwoei = wl_nwoe_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_nwoe_attach failed\n", unit));
		goto fail;
	}
#endif /* NWOE */
#if defined(WLNDOE) && !defined(WLNDOE_DISABLED)
	/* allocate the arp info struct */
	if ((wl->ndi = wl_nd_attach(wlc)) == NULL) {
		WL_ERROR(("wl%d: wl_nd_attach failed\n", unit));
		goto fail;
	}
#endif /* WLNDOE  && !defined(WLNDOE_DISABLED) */
#endif /* WLC_HIGH */

#ifdef	CONFIG_WLU
	hndrte_cons_addcmd("wl", do_wl_cmd, (uint32)wl);
#endif /* CONFIG_WLU */

#ifdef BCMDBG
	hndrte_cons_addcmd("wlmsg", do_wlmsg_cmd, (uint32)wl);
#endif
#ifdef WLC_HIGH

	/* register module */
	if (wlc_module_register(wlc->pub, wl_iovars, "wl", wl, wl_doiovar, NULL, NULL, NULL)) {
		WL_ERROR(("wl%d: wlc_module_register() failed\n", unit));
		goto fail;
	}

#if defined(BCMDBG) || defined(WLDUMP)
	wlc_dump_register(wl->pub, "wl", (dump_fn_t)wl_dump, (void *)wl);
#endif

#endif /* WLC_HIGH */

#ifdef MSGTRACE
	msgtrace_init(wlc, wl->dev, (msgtrace_func_send_t)wlc_event_sendup_trace);
#endif


	return (wl);

fail:
	wl_free(wl, osh);
	return NULL;
}

static void
BCMATTACHFN(wl_free)(wl_info_t *wl, osl_t *osh)
{

#ifdef WLC_HIGH
	if (wl->pkt_filter_info)
		wlc_pkt_filter_detach(wl->pkt_filter_info);
	if (wl->keep_alive_info)
		wl_keep_alive_detach(wl->keep_alive_info);
	if (wl->arpi)
		wl_arp_detach(wl->arpi);
	if (wl->toei)
		wl_toe_detach(wl->toei);
#ifdef NWOE
	if (wl->nwoei)
		wl_nwoe_detach(wl->nwoei);
#endif /* NWOE */
#ifdef WLPFN
	if (WLPFN_ENAB(wl->pub) && wl->pfn)
		wl_pfn_detach(wl->pfn);
#endif
	if (wl->oid)
		wl_oid_detach(wl->oid);
#endif /* WLC_HIGH */

#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
	hndrte_pt_lowmem_unregister(&wl->lowmem_free_info);
#endif

	/* common code detach */
	if (wl->wlc)
		wlc_detach(wl->wlc);

#ifdef WLC_LOW_ONLY
	/* rpc, rpc_transport detach */
	if (wl->rpc)
		bcm_rpc_detach(wl->rpc);
	if (wl->rpc_th)
		bcm_rpc_tp_detach(wl->rpc_th);
#endif /* WLC_LOG_ONLY */

#if defined(WLNDOE) && !defined(WLNDOE_DISABLED)
	if (wl->ndi)
		wl_nd_detach(wl->ndi);
#endif /* defined(WLNDOE) && !defined(WLNDOE_DISABLED) */
	MFREE(osh, wl, sizeof(wl_info_t));
}

static void
wl_isr(hndrte_dev_t *dev)
{
	wl_info_t *wl = dev->softc;
	bool dpc;

	WL_TRACE(("wl%d: wl_isr\n", wl->unit));

	/* call common first level interrupt handler */
	if (wlc_isr(wl->wlc, &dpc)) {
		/* if more to do... */
		if (dpc) {
			wl_dpc(wl);
		}
	}
}

static void
wl_dpc(wl_info_t *wl)
{
	bool resched = 0;
	bool bounded = TRUE;

	/* call the common second level interrupt handler if we have enough memory */
	if (wl->wlc_hw->up) {
		wlc_dpc_info_t dpci = {0};
#ifdef WLC_LOW_ONLY
		if (!wl->dpc_stopped) {
			if (wl->wlc_hw->rpc_dngl_agg & BCM_RPC_TP_DNGL_AGG_DPC) {
				bcm_rpc_tp_agg_set(wl->rpc_th, BCM_RPC_TP_DNGL_AGG_DPC, TRUE);
			}

			resched = wlc_dpc(wl->wlc, bounded, &dpci);

			if (wl->wlc_hw->rpc_dngl_agg & BCM_RPC_TP_DNGL_AGG_DPC) {
				bcm_rpc_tp_agg_set(wl->rpc_th, BCM_RPC_TP_DNGL_AGG_DPC, FALSE);
			}
		} else {
			WL_TRACE(("dpc_stop is set!\n"));
			wl->dpc_requested = TRUE;
			return;
		}
#else
		resched = wlc_dpc(wl->wlc, bounded, &dpci);
#endif /* WLC_LOW_ONLY */
	}

	/* wlc_dpc() may bring the driver down */
	if (!wl->wlc_hw->up)
		return;

	/* re-schedule dpc or re-enable interrupts */
	if (resched) {
		if (!hndrte_add_timer(&wl->dpcTimer, 0, FALSE))
			ASSERT(FALSE);
	} else
		wlc_intrson(wl->wlc);
}

static void
_wl_dpc(hndrte_timer_t *timer)
{
	wl_info_t *wl = (wl_info_t *) timer->data;

	if (wl->wlc_hw->up) {
		wlc_intrsupd(wl->wlc);
		wl_dpc(wl);
	}
}

static int
wl_open(hndrte_dev_t *dev)
{
	wl_info_t *wl = dev->softc;
	int ret;

	WL_TRACE(("wl%d: wl_open\n", wl->unit));

	if ((ret = wlc_ioctl(wl->wlc, WLC_UP, NULL, 0, NULL)))
		return ret;

#ifdef HNDRTE_JOIN_SSID
	/*
	 * Feature useful for repetitious testing: if Make defines HNDRTE_JOIN_SSID
	 * to an SSID string, automatically join that SSID at driver startup.
	 */
	{
		wlc_info_t *wlc = wl->wlc;
		int infra = 1;
		int auth = 0;
		char *ss = HNDRTE_JOIN_SSID;
		wlc_ssid_t ssid;

		printf("Joining %s:\n", ss);
		/* set infrastructure mode */
		printf("  Set Infra\n");
		wlc_ioctl(wlc, WLC_SET_INFRA, &infra, sizeof(int), NULL);
		printf("  Set Auth\n");
		wlc_ioctl(wlc, WLC_SET_AUTH, &auth, sizeof(int), NULL);
		printf("  Set SSID %s\n", ss);
		ssid.SSID_len = strlen(ss);
		bcopy(ss, ssid.SSID, ssid.SSID_len);
		wlc_ioctl(wlc, WLC_SET_SSID, &ssid, sizeof(wlc_ssid_t), NULL);
	}
#endif /* HNDRTE_JOIN_SSID */

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub))
	{
		wlc_bsscfg_t* bsscfg;
		wlc_tunables_t *tunables = (((wlc_info_t *)(wl->wlc))->pub)->tunables;

		uint8 credit_map[] = {
			tunables->wlfcfifocreditac0,
			tunables->wlfcfifocreditac1,
			tunables->wlfcfifocreditac2,
			tunables->wlfcfifocreditac3,
			tunables->wlfcfifocreditbcmc,
			tunables->wlfcfifocreditother,
		};

		wlc_mac_event(wl->wlc,
			WLC_E_FIFO_CREDIT_MAP, NULL, 0, 0, 0, credit_map, sizeof(credit_map));

		bsscfg = wlc_bsscfg_find_by_wlcif(wl->wlc, NULL);
		wlc_if_event(wl->wlc, WLC_E_IF_ADD, bsscfg->wlcif);
	}
#endif /* PROP_TXSTATUS */

	return (ret);
}

#ifdef WLC_HIGH

static int
_wl_toe_recv_proc(wl_info_t *wl, void *p)
{
	if (TOE_ENAB(wl->pub))
		(void)wl_toe_recv_proc(wl->toei, p);
	return 0;
}
static bool
wl_hwfilter(wl_info_t *wl, void *p)
{
	struct ether_header *eh = (struct ether_header *)PKTDATA(wl->pub->osh, p);

	if (((wl->hwfflags & HWFFLAG_UCAST) && !ETHER_ISMULTI(eh->ether_dhost)) ||
	    ((wl->hwfflags & HWFFLAG_BCAST) && ETHER_ISBCAST(eh->ether_dhost)))
		return TRUE;

	return FALSE;
}

#ifdef PROP_TXSTATUS
int
wlfc_MAC_table_update(struct wl_info *wl, uint8* ea,
	uint8 add_del, uint8 mac_handle, uint8 ifidx)
{
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_MACDESC];

	results[0] = add_del;
	results[1] = WLFC_CTL_VALUE_LEN_MACDESC;
	results[2] = mac_handle;
	results[3] = ifidx;
	memcpy(&results[4], ea, ETHER_ADDR_LEN);
	return wlfc_push_signal_data(wl, results, sizeof(results), FALSE);
}

wlfc_info_state_t*
wlfc_state_get(struct wl_info *wl)
{
	if (wl != NULL)
		return wl->wlfc_info;
	return NULL;
}

int
wlfc_psmode_request(struct wl_info *wl, uint8 mac_handle, uint8 count,
	uint8 precedence_bitmap, uint8 request_type)
{
	/* space for type(1), length(1) and value */
	uint8	results[1+1+WLFC_CTL_VALUE_LEN_REQUEST_CREDIT];
	int ret;

	results[0] = request_type;
	if (request_type == WLFC_CTL_TYPE_MAC_REQUEST_PACKET)
		results[1] = WLFC_CTL_VALUE_LEN_REQUEST_PACKET;
	else
		results[1] = WLFC_CTL_VALUE_LEN_REQUEST_CREDIT;
	results[2] = count;
	results[3] = mac_handle;
	results[4] = precedence_bitmap;
	ret = wlfc_push_signal_data(wl, results, sizeof(results), FALSE);
	if (ret == BCME_OK)
		ret = wlfc_sendup_ctl_info_now(wl);
	return ret;
}

int
wlfc_sendup_ctl_info_now(struct wl_info *wl)
{
	int header_overhead = BCMDONGLEOVERHEAD*3;
	struct lbuf *wlfc_pkt;

	ASSERT(wl != NULL);
	ASSERT(wl->wlfc_info != NULL);

	if ((wlfc_pkt = PKTGET(wl->pub->osh,
		(wl->wlfc_info->pending_datalen + header_overhead),
		TRUE)) == NULL) {
		/* what can be done to deal with this?? */
		/* set flag and try later again? */
		WL_ERROR(("PKTGET pkt size %d failed\n", wl->wlfc_info->pending_datalen));
		return BCME_NOMEM;
	}
	PKTPULL(wl->pub->osh, wlfc_pkt, header_overhead + wl->wlfc_info->pending_datalen);
	PKTSETLEN(wl->pub->osh, wlfc_pkt, 0);
	PKTSETTYPEEVENT(wl->pub->osh, wlfc_pkt);
	wl_sendup(wl, NULL, wlfc_pkt, 1);
#ifdef PROP_TXSTATUS_DEBUG
	wl->wlfc_info->stats.nullpktallocated++;
#endif
	return BCME_OK;
}

int
wlfc_push_credit_data(struct wl_info *wl, void* p)
{
	uint8 ac;

	ac = WL_TXSTATUS_GET_FIFO(WLPKTTAG(p)->wl_hdr_information);
	WLPKTTAG(p)->flags |= WLF_CREDITED;

#ifdef PROP_TXSTATUS_DEBUG
	wl->wlfc_info->stats.creditupdates++;
	wl->wlfc_info->stats.credits[ac]++;
#endif
	wl->wlfc_info->fifo_credit_back[ac]++;
	wl->wlfc_info->fifo_credit_back_pending = 1;
	/*
	monitor how much credit is being gathered here. If credit pending is
	larger than a preset threshold, send_it_now(). The idea is to keep
	the host busy pushing packets to keep the pipeline filled.
	*/
	if ((wl->wlfc_info->fifo_credit_back[TX_AC_BE_FIFO] >=
		wl->wlfc_info->fifo_credit_threshold[TX_AC_BE_FIFO]) ||
		(wl->wlfc_info->fifo_credit_back[TX_AC_BK_FIFO] >=
		wl->wlfc_info->fifo_credit_threshold[TX_AC_BK_FIFO]) ||
		(wl->wlfc_info->fifo_credit_back[TX_AC_VI_FIFO] >=
		wl->wlfc_info->fifo_credit_threshold[TX_AC_VI_FIFO]) ||
		(wl->wlfc_info->fifo_credit_back[TX_AC_VO_FIFO] >=
		wl->wlfc_info->fifo_credit_threshold[TX_AC_VO_FIFO])) {
		wlfc_sendup_ctl_info_now(wl);
	}
	return BCME_OK;
}

int
wlfc_push_signal_data(struct wl_info *wl, void* data, uint8 len, bool hold)
{
	int rc = BCME_OK;
	uint8 type = ((uint8*)data)[0];
	uint8 tlv_flag;
	uint32 tlv_mask;
	bool skip_cp = FALSE;

	ASSERT(wl != NULL);
	ASSERT(wl->wlfc_info != NULL);

	tlv_flag = ((wlc_info_t *)(wl->wlc))->wlfc_flags;

	tlv_mask = (((tlv_flag & WLFC_FLAGS_XONXOFF_SIGNALS) ? 1 : 0) ?
		WLFC_FLAGS_XONXOFF_MASK : 0) |
		(((tlv_flag & WLFC_FLAGS_CREDIT_STATUS_SIGNALS) ? 1 : 0) ?
		WLFC_FLAGS_CREDIT_STATUS_MASK : 0);

	/* if the host does not want these TLV signals, drop it */
	if (!(tlv_mask & (1 << type))) {
		WLFC_DBGMESG(("%s() Dropping signal, type:%d, mask:%08x, flag:%d\n", __FUNCTION__,
			type, tlv_mask, tlv_flag));
		return BCME_OK;
	}

	if ((wl->wlfc_info->pending_datalen + len) > WLFC_MAX_PENDING_DATALEN) {
		if (BCME_OK != (rc = wlfc_sendup_ctl_info_now(wl)))
			/* at least the caller knows we have failed */
			return rc;
	}

	if (((wlc_info_t *)wl->wlc)->comp_stat_enab) {
		if (wl->wlfc_info->pending_datalen && (type == WLFC_CTL_TYPE_TXSTATUS)) {
			uint8 cur_pos = 0;
			while ((cur_pos + wl->wlfc_info->data[cur_pos+1]+2) <
				wl->wlfc_info->pending_datalen) {
				cur_pos = cur_pos + wl->wlfc_info->data[cur_pos+1] + 2;
			}
			if ((wl->wlfc_info->data[cur_pos] == WLFC_CTL_TYPE_TXSTATUS)) {
				if ((((uint8 *)data)[5] == wl->wlfc_info->data[cur_pos + 5]) &&
					(((uint8 *)data)[2] ==
					(wl->wlfc_info->data[cur_pos + 2] + 1)) &&
					(((uint8 *)data)[3] ==
					(wl->wlfc_info->data[cur_pos + 3] + 1)) &&
					(((uint8 *)data)[4] ==
					(wl->wlfc_info->data[cur_pos + 4]))) {
					wl->wlfc_info->data[cur_pos + 1]++;
					wl->wlfc_info->data[cur_pos + 6] = 2;
					wl->wlfc_info->pending_datalen++;
					wl->wlfc_info->data[cur_pos] = WLFC_CTL_TYPE_COMP_TXSTATUS;
					skip_cp = TRUE;
					wl->wlfc_info->compressed_stat_cnt++;
				}
			} else  if (wl->wlfc_info->data[cur_pos] == WLFC_CTL_TYPE_COMP_TXSTATUS) {
				if ((((uint8 *)data)[5] == wl->wlfc_info->data[cur_pos + 5]) &&
					(((uint8 *)data)[2] ==	(wl->wlfc_info->data[cur_pos + 2] +
					wl->wlfc_info->data[cur_pos + 6])) &&
					(((uint8 *)data)[3] == (wl->wlfc_info->data[cur_pos + 3] +
					wl->wlfc_info->data[cur_pos + 6])) &&
					(((uint8 *)data)[4] ==
					(wl->wlfc_info->data[cur_pos + 4]))) {
					wl->wlfc_info->data[cur_pos + 6]++;
					wl->wlfc_info->compressed_stat_cnt++;
					skip_cp = TRUE;
				}
			}
		}
	} else {
		hold = FALSE;
	}

	if (!skip_cp) {
	memcpy(&wl->wlfc_info->data[wl->wlfc_info->pending_datalen], data, len);
	wl->wlfc_info->pending_datalen += len;
	}

	if ((wl->wlfc_info->pending_datalen > WLFC_PENDING_TRIGGER_WATERMARK) ||
		(!hold &&
		(wl->wlfc_info->compressed_stat_cnt > WLFC_PENDING_TRIGGER_WATERMARK/6))) {
		rc = wlfc_sendup_ctl_info_now(wl);
	}

	if (!wl->wlfc_info->timer_started) {
		wl_add_timer(wl, wl->wlfc_info->fctimer,
			WLFC_SENDUP_TIMER_INTERVAL, 0);
		wl->wlfc_info->timer_started = 1;
	}
	return rc;
}

static int
wl_sendup_txstatus(wl_info_t *wl, void **pp)
{
	wlfc_info_state_t* wlfc = (wlfc_info_state_t*)wl->wlfc_info;
	uint8* wlfchp;
	uint8 required_headroom;
	uint8 wl_hdr_words = 0;
	uint8 fillers = 0;
	uint8 rssi_space = 0;
	uint8 seqnumber_space = 0;
	uint8 fcr_tlv_space = 0;
	uint8* fcr_tlv;
	uint8 ampdu_reorder_info_space = 0;
	void *p = *pp;
	uint32 datalen;
	wl->wlfc_info->compressed_stat_cnt = 0;

	/* For DATA packets: plugin a RSSI value that belongs to this packet.
	   RSSI TLV = 1 + 1 + WLFC_CTL_VALUE_LEN_RSSI
	 */
	 if (!PKTTYPEEVENT(wl->pub->osh, p)) {
		/* is the RSSI TLV reporting enabled? */
		if (((wlc_info_t *)(wl->wlc))->wlfc_flags & WLFC_FLAGS_RSSI_SIGNALS) {
			rssi_space = 1 + 1 + WLFC_CTL_VALUE_LEN_RSSI;
			wlfc->pending_datalen += rssi_space;
		}
#ifdef WLAMPDU_HOSTREORDER
		/* check if the host roredering info needs to be added from pkttag */
		{
			wlc_pkttag_t *pkttag;
			pkttag = WLPKTTAG(p);
			if (pkttag->flags2 & WLF2_HOSTREORDERAMPDU_INFO) {
				ampdu_reorder_info_space = WLHOST_REORDERDATA_LEN + TLV_HDR_LEN;
			}
		}
#endif /* WLAMPDU_HOSTREORDER */
	 }
#ifdef WLFCHOST_TRANSACTION_ID
	 seqnumber_space = TLV_HDR_LEN + WLFC_TYPE_TRANS_ID_LEN;
#endif

	if (wlfc->fifo_credit_back_pending) {
		fcr_tlv_space = TLV_HDR_LEN + WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK;
		wlfc->pending_datalen += fcr_tlv_space;
	}

	datalen = wlfc->pending_datalen + ampdu_reorder_info_space + seqnumber_space;

	fillers = ROUNDUP(datalen, 4) - datalen;
	required_headroom = datalen + fillers;
	wl_hdr_words = required_headroom >> 2;

	if (PKTHEADROOM(wl->pub->osh, p) < required_headroom) {
		void *p1;
		int plen = PKTLEN(wl->pub->osh, p);

		/* Allocate a packet that will fit all the data */
		if ((p1 = PKTGET(wl->pub->osh, (plen + required_headroom), TRUE)) == NULL) {
			WL_ERROR(("PKTGET pkt size %d failed\n", plen));
			PKTFREE(wl->pub->osh, p, TRUE);
			return TRUE;
		}
		/* Transfer other fields */
		PKTSETPRIO(p1, PKTPRIO(p));
		PKTSETSUMGOOD(p1, PKTSUMGOOD(p));
		bcopy(PKTDATA(wl->pub->osh, p),
			(PKTDATA(wl->pub->osh, p1) + required_headroom), plen);
		wlc_pkttag_info_move(wl->pub, p, p1);
		PKTFREE(wl->pub->osh, p, TRUE);
		p = p1;
		*pp = p1;
#ifdef PROP_TXSTATUS_DEBUG
		wlfc->stats.realloc_in_sendup++;
#endif
	} else
		PKTPUSH(wl->pub->osh, p, required_headroom);

	wlfchp = PKTDATA(wl->pub->osh, p);

#ifdef WLFCHOST_TRANSACTION_ID
	if (seqnumber_space) {
		uint32 timestamp;

		/* bute 0: ver, byte 1: seqnumber, byte2:byte6 timestamps */
		wlfchp[0] = WLFC_CTL_TYPE_TRANS_ID;
		wlfchp[1] = WLFC_TYPE_TRANS_ID_LEN;
		wlfchp += TLV_HDR_LEN;

		wlfchp[0] = 0;
		wlfchp[1] = wlfc->txseqtohost++;

		/* time stamp of the packet */
		timestamp = OSL_SYSUPTIME();
		bcopy(&timestamp, &wlfchp[2], sizeof(uint32));

		wlfchp += WLFC_TYPE_TRANS_ID_LEN;
	}
#endif /* WLHOST_DBG_SEQNUMBER */

#ifdef WLAMPDU_HOSTREORDER
	if (ampdu_reorder_info_space) {

		wlc_pkttag_t *pkttag = WLPKTTAG(p);
		PKTSETNODROP(wl->pub->osh, p);


		wlfchp[0] = WLFC_CTL_TYPE_HOST_REORDER_RXPKTS;
		wlfchp[1] = WLHOST_REORDERDATA_LEN;
		wlfchp += TLV_HDR_LEN;

		/* zero out the tag value */
		bzero(wlfchp, WLHOST_REORDERDATA_LEN);

		wlfchp[WLHOST_REORDERDATA_FLOWID_OFFSET] =
			pkttag->u.ampdu_info_to_host.ampdu_flow_id;
		wlfchp[WLHOST_REORDERDATA_MAXIDX_OFFSET] =
			pkttag->u.ampdu_info_to_host.max_idx;
		wlfchp[WLHOST_REORDERDATA_FLAGS_OFFSET] =
			pkttag->u.ampdu_info_to_host.flags;
		wlfchp[WLHOST_REORDERDATA_CURIDX_OFFSET] =
			pkttag->shared.ampdu_seqs_to_host.cur_idx;
		wlfchp[WLHOST_REORDERDATA_EXPIDX_OFFSET] =
			pkttag->shared.ampdu_seqs_to_host.exp_idx;

		WL_INFORM(("flow:%d idx(%d, %d, %d), flags 0x%02x\n",
			wlfchp[WLHOST_REORDERDATA_FLOWID_OFFSET],
			wlfchp[WLHOST_REORDERDATA_CURIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_EXPIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_MAXIDX_OFFSET],
			wlfchp[WLHOST_REORDERDATA_FLAGS_OFFSET]));
		wlfchp += WLHOST_REORDERDATA_LEN;
	}
#endif /* WLAMPDU_HOSTREORDER */

	/* if there're any fifo credit pending, append it */
	if (fcr_tlv_space) {
		fcr_tlv = &wlfc->data[wlfc->pending_datalen - fcr_tlv_space - rssi_space];
		fcr_tlv[0] = WLFC_CTL_TYPE_FIFO_CREDITBACK;
		fcr_tlv[1] = WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK;
		memcpy(&fcr_tlv[2], wlfc->fifo_credit_back,
			WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK);

		/* reset current credit map */
		memset(wlfc->fifo_credit_back, 0, WLFC_CTL_VALUE_LEN_FIFO_CREDITBACK);
		wlfc->fifo_credit_back_pending = 0;
	}

	if (rssi_space) {
		wlfchp[0] = WLFC_CTL_TYPE_RSSI;
		wlfchp[1] = WLFC_CTL_VALUE_LEN_RSSI;
		wlfchp[2] = ((wlc_pkttag_t*)WLPKTTAG(p))->rssi;
	}
	if (wlfc->pending_datalen > rssi_space) {
		/* this packet is carrying signals */
		PKTSETNODROP(wl->pub->osh, p);
		memcpy(&wlfchp[rssi_space], wlfc->data,
			(wlfc->pending_datalen - rssi_space));
	}
	if (fillers)
		memset(&wlfchp[wlfc->pending_datalen], WLFC_CTL_TYPE_FILLER, fillers);

	PKTSETDATAOFFSET(p, wl_hdr_words);
	wlfc->pending_datalen = 0;

	if (wlfc->timer_started) {
		/* cancel timer */
		wl_del_timer(wl, wlfc->fctimer);
		wlfc->timer_started = 0;
	}
	return FALSE;
}
#endif /* PROP_TXSTATUS */


static void *
wl_pkt_header_push(wl_info_t *wl, void *p, uint8 *wl_hdr_words)
{
	wl_header_t *h;
	osl_t *osh = wl->pub->osh;
	wlc_pkttag_t *pkttag = WLPKTTAG(p);
	int8 rssi = pkttag->rssi;

	if (PKTHEADROOM(osh, p) < WL_HEADER_LEN) {
		void *p1;
		int plen = PKTLEN(osh, p);

		/* Alloc a packet that will fit all the data; chaining the header won't work */
		if ((p1 = PKTGET(osh, plen + WL_HEADER_LEN, TRUE)) == NULL) {
			WL_ERROR(("PKTGET pkt size %d failed\n", plen));
			PKTFREE(osh, p, TRUE);
			return NULL;
		}

		/* Transfer other fields */
		PKTSETPRIO(p1, PKTPRIO(p));
		PKTSETSUMGOOD(p1, PKTSUMGOOD(p));

		bcopy(PKTDATA(osh, p), PKTDATA(osh, p1) + WL_HEADER_LEN, plen);
		PKTFREE(osh, p, TRUE);

		p = p1;
	} else
		PKTPUSH(osh, p, WL_HEADER_LEN);

	h = (wl_header_t *)PKTDATA(osh, p);
	h->type = WL_HEADER_TYPE;
	h->version = WL_HEADER_VER;
	h->rssi = rssi;
	h->pad = 0;
	/* Return header length in words */
	*wl_hdr_words = WL_HEADER_LEN/4;

	return p;
}

static void
wl_pkt_header_pull(wl_info_t *wl, void *p)
{
	/* Currently this is a placeholder function. We don't process wl header
	   on Tx side as no meaningful fields defined for tx currently.
	 */
	PKTPULL(wl->pub->osh, p, PKTDATAOFFSET(p));
	return;
}

/* Return the proper arpi pointer for either corr to an IF or
*	default. For IF case, Check if arpi is present. It is possible that, upon a
*	down->arpoe_en->up scenario, interfaces are not reallocated, and
*	so, wl->arpi could be NULL. If so, allocate it and use.
*/
static wl_arp_info_t *
wl_get_arpi(wl_info_t *wl, struct wl_if *wlif)
{
	if (wlif != NULL) {
		if (wlif->arpi == NULL)
			wlif->arpi = wl_arp_alloc_ifarpi(wl->arpi, wlif->wlcif);
		/* note: this could be null if the above wl_arp_alloc_ifarpi fails */
		return wlif->arpi;
	} else
		return wl->arpi;
}

void *
wl_get_ifctx(wl_info_t *wl, int ctx_id, wl_if_t *wlif)
{
	if (ctx_id == IFCTX_ARPI)
		return (void *)wlif->arpi;

#if defined(WLNDOE) && !defined(WLNDOE_DISABLED)
	if (ctx_id == IFCTX_NDI)
		return (void *)wlif->ndi;
#endif /* defined(WLNDOE) && !defined(WLNDOE_DISABLED) */
	return NULL;
}
#ifdef WLNDOE
/* Return the proper ndi pointer for either corr to an IF or
*	default. For IF case, Check if arpi is present. It is possible that, upon a
*	down->ndoe_en->up scenario, interfaces are not reallocated, and
*	so, wl->ndi could be NULL. If so, allocate it and use.
*/
static wl_nd_info_t *
wl_get_ndi(wl_info_t *wl, struct wl_if *wlif)
{
	if (wlif != NULL) {
		if (wlif->ndi == NULL)
			wlif->ndi = wl_nd_alloc_ifndi(wl->ndi, wlif->wlcif);
		/* note: this could be null if the above wl_arp_alloc_ifarpi fails */
		return wlif->ndi;
	} else
		return wl->ndi;
}
#endif /* WLNDOE */

/*
 * The last parameter was added for the build. Caller of
 * this function should pass 1 for now.
 */
void
wl_sendup(wl_info_t *wl, struct wl_if *wlif, void *p, int numpkt)
{
	struct lbuf *lb;
	hndrte_dev_t *dev;
	hndrte_dev_t *chained;
	int ret_val;
	int no_filter;
	uint8 *buf;
	bool brcm_specialpkt;
	uint8 wl_hdr_words = 0;

	WL_TRACE(("wl%d: wl_sendup: %d bytes\n", wl->unit, PKTLEN(NULL, p)));

	no_filter = 0;
	if (wlif == NULL)
		dev = wl->dev;
	else
		dev = wlif->dev;
	chained = dev->chained;

	buf = PKTDATA(wl->pub->osh, p);
#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub))
		brcm_specialpkt = !!PKTTYPEEVENT(wl->pub->osh, p);
	else
#endif
	brcm_specialpkt = ntoh16_ua(buf + ETHER_TYPE_OFFSET) == ETHER_TYPE_BRCM;

	if (!brcm_specialpkt) {
		_wl_toe_recv_proc(wl, p);

#ifdef NWOE
		if (NWOE_ENAB(wl->pub))
		{
			ret_val = wl_nwoe_recv_proc(wl->nwoei, wl->pub->osh, p);
			if (ret_val == NWOE_PKT_CONSUMED)
				return;
		}
#endif /* NWOE */

		/* Apply ARP offload */
		if (ARPOE_ENAB(wl->pub)) {
			wl_arp_info_t *arpi = wl_get_arpi(wl, wlif);
			if (arpi) {
				ret_val = wl_arp_recv_proc(arpi, p);
				if ((ret_val == ARP_REQ_SINK) || (ret_val == ARP_REPLY_PEER)) {
					PKTFREE(wl->pub->osh, p, FALSE);
					return;
				}
				if (ret_val == ARP_FORCE_FORWARD)
					no_filter = 1;
			}
		}
#ifdef WLNDOE
		/* Apply NS offload */
		if (NDOE_ENAB(wl->pub)) {
			wl_nd_info_t *ndi = wl_get_ndi(wl, wlif);
			if (ndi) {
				ret_val = wl_nd_recv_proc(ndi, p);
				if ((ret_val == ND_REQ_SINK) || (ret_val == ND_REPLY_PEER)) {
					PKTFREE(wl->pub->osh, p, FALSE);
					return;
				}
				if (ret_val == ND_FORCE_FORWARD) {
					no_filter = 1;
				}
			}
		}
#endif
	}


	if (chained) {

		/* Internally generated events have the special ether-type of
		 * ETHER_TYPE_BRCM; do not run these events through data packet filters.
		 */
		if (!brcm_specialpkt) {
			/* Apply packet filter */
			if ((chained->flags & RTEDEVFLAG_HOSTASLEEP) &&
			    wl->hwfflags && !wl_hwfilter(wl, p)) {
				PKTFREE(wl->pub->osh, p, FALSE);
				return;
			}

			/* Apply packet filtering. */
			if (!no_filter && PKT_FILTER_ENAB(wl->pub)) {
				if (!wlc_pkt_filter_recv_proc(wl->pkt_filter_info, p)) {
					/* Discard received packet. */
					PKTFREE(wl->pub->osh, p, FALSE);
					return;
				}
			}
		}

#ifdef PROP_TXSTATUS
		if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub)) {
			if (wl_sendup_txstatus(wl, &p)) {
				return;
			}
		} else
#endif /* PROP_TXSTATUS */
		{
		if ((p = wl_pkt_header_push(wl, p, &wl_hdr_words)) == NULL) {
			return;
		}

		PKTSETDATAOFFSET(p, wl_hdr_words);
		}
		lb = PKTTONATIVE(wl->pub->osh, p);
		if (chained->funcs->xmit(dev, chained, lb) != 0) {
			WL_ERROR(("%s: xmit failed; free pkt 0x%p\n", __FUNCTION__, lb));
			lb_free(lb);
		}
	} else {
		/* only AP mode can be non chained */
		ASSERT(AP_ENAB(wl->pub));
		PKTFREE(wl->pub->osh, p, FALSE);
	}
}
#endif /* WLC_HIGH */

/* buffer received from BUS driver(e.g USB, SDIO) in dongle framework
 *   For normal driver, push it to common driver sendpkt
 *   For BMAC driver, forward to RPC layer to process
 */
#ifdef WLC_HIGH

static void
_wl_toe_send_proc(wl_info_t *wl, void *p)
{
	if (TOE_ENAB(wl->pub))
		wl_toe_send_proc(wl->toei, p);
}

#ifdef PROP_TXSTATUS
static int
wl_send_txstatus(wl_info_t *wl, void *p)
{
	uint8* wlhdrtodev;
	wlc_pkttag_t *pkttag;
	uint8 wlhdrlen;
	uint8 processed = 0;

	ASSERT(wl != NULL);

	pkttag = WLPKTTAG(p);
	pkttag->wl_hdr_information = 0;

	wlhdrlen = PKTDATAOFFSET(p) << 2;
	if (wlhdrlen != 0) {
		wlhdrtodev = (uint8*)PKTDATA(wl->pub->osh, p);

		while (processed < wlhdrlen) {
			if (wlhdrtodev[processed] == WLFC_CTL_TYPE_PKTTAG) {
				pkttag->wl_hdr_information =
					ltoh32_ua(&wlhdrtodev[processed + 2]);
			}
			else if (wlhdrtodev[processed] ==
				WLFC_CTL_TYPE_PENDING_TRAFFIC_BMP) {
				wlc_scb_update_available_traffic_info(wl->wlc,
					wlhdrtodev[processed+2], wlhdrtodev[processed+3]);
			}
			if (wlhdrtodev[processed] == WLFC_CTL_TYPE_FILLER) {
				/* skip ahead - 1 */
				processed += 1;
			}
			else {
				/* skip ahead - type[1], len[1], value_len */
				processed += 1 + 1 + wlhdrtodev[processed + 1];
			}
		}
		PKTPULL(wl->pub->osh, p, wlhdrlen);
	}
	else
		WL_INFORM(("No pkttag from host.\n"));

	if (wl->wlfc_info != NULL) {
		((wlfc_info_state_t*)wl->wlfc_info)->stats.packets_from_host++;
	}

	if (PKTLEN(wl->pub->osh, p) == 0) {
		/* a signal-only packet from host */
		PKTFREE(wl->pub->osh, p, TRUE);
		return TRUE;
	}
#ifdef PROP_TXSTATUS_DEBUG
	if ((WL_TXSTATUS_GET_FLAGS(pkttag->wl_hdr_information) &
		WLFC_PKTFLAG_PKTFROMHOST) &&
		(!(WL_TXSTATUS_GET_FLAGS(pkttag->wl_hdr_information) &
		WLFC_PKTFLAG_PKT_REQUESTED))) {
		((wlfc_info_state_t*)wl->wlfc_info)->stats.creditin++;
	}
	if (wlc_pkt_callback_register(wl->wlc, wlfc_hostpkt_callback, p, p) < 0) {
		WLFC_DBGMESG(("Error:%s():%d, wlc_pkt_callback_register() failed\n",
			__FUNCTION__, __LINE__));
		wlfc_hostpkt_callback(wl->wlc, 0, p);
		/* serious problem, can't even register callback */
		PKTFREE(wl->pub->osh, p, TRUE);
		return TRUE;
	}
#endif
	return FALSE;
}
#endif /* PROP_TXSTATUS */

static int
wl_send(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb)
{
	wl_info_t *wl = dev->softc;
	wl_if_t *wlif = WL_IF(wl, dev);
	struct wlc_if *wlcif = wlif != NULL ? wlif->wlcif : NULL;
	void *p;

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(((wlc_info_t *)(wl->wlc))->pub)) {
		p = PKTFRMNATIVE(wl->pub->osh, lb);
		if (wl_send_txstatus(wl, p)) {
			return TRUE;
		}
	} else
#endif /* PROP_TXSTATUS */
	{
		/* Pull wl header. Currently no information is included on transmit side */
		wl_pkt_header_pull(wl, lb);

		p = PKTFRMNATIVE(wl->pub->osh, lb);
	}

	WL_TRACE(("wl%d: wl_send: len %d\n", wl->unit, PKTLEN(wl->pub->osh, p)));

	/* Apply ARP offload */
	if (ARPOE_ENAB(wl->pub)) {
		wl_arp_info_t *arpi = wl_get_arpi(wl, wlif);
		if (arpi) {
			if (wl_arp_send_proc(arpi, p) ==
				ARP_REPLY_HOST) {
				PKTFREE(wl->pub->osh, p, TRUE);
				return TRUE;
			}
		}
	}

#ifdef WLNDOE
	/* Apply NS offload */
	if (NDOE_ENAB(wl->pub)) {
		wl_nd_info_t *ndi = wl_get_ndi(wl, wlif);
		if (ndi) {
			wl_nd_send_proc(ndi, p);
		}
	}
#endif

	_wl_toe_send_proc(wl, p);

	if (wlc_sendpkt(wl->wlc, p, wlcif))
		return TRUE;


	return FALSE;
}
#else
static int
wl_send(hndrte_dev_t *src, hndrte_dev_t *dev, struct lbuf *lb)
{
	wl_info_t *wl = dev->softc;

	WL_TRACE(("wl%d: wl_send: len %d\n", wl->unit, lb->len));

	bcm_rpc_tp_rx_from_dnglbus(wl->rpc_th, lb);

	return FALSE;
}
#endif /* WLC_HIGH */

#ifdef WLC_HIGH
void
wl_txflowcontrol(wl_info_t *wl, struct wl_if *wlif, bool state, int prio)
{
	hndrte_dev_t *chained = wl->dev->chained;

	/* sta mode must be chained */
	if (chained && chained->funcs->txflowcontrol)
		chained->funcs->txflowcontrol(chained, state, prio);
	else
		ASSERT(AP_ENAB(wl->pub));
}

void
wl_event(wl_info_t *wl, char *ifname, wlc_event_t *e)
{
	wl_oid_event(wl->oid, e);

#ifdef WLPFN
	/* Tunnel events into PFN for analysis */
	if (WLPFN_ENAB(wl->pub))
		wl_pfn_event(wl->pfn, e);
#endif /* WLPFN */

	switch (e->event.event_type) {
	case WLC_E_LINK:
	case WLC_E_NDIS_LINK:
		wl->link = e->event.flags&WLC_EVENT_MSG_LINK;
		if (wl->link)
			WL_ERROR(("wl%d: link up (%s)\n", wl->unit, ifname));
/* Getting too many */
		else
			WL_ERROR(("wl%d: link down (%s)\n", wl->unit, ifname));
		break;
#if defined(BCMSUP_PSK) && defined(STA)
	case WLC_E_MIC_ERROR: {
		wlc_bsscfg_t *cfg = wlc_bsscfg_find_by_wlcif(wl->wlc, NULL);
		if (cfg == NULL || e->event.bsscfgidx != WLC_BSSCFG_IDX(cfg))
			break;
		wlc_sup_mic_error(cfg, (e->event.flags&WLC_EVENT_MSG_GROUP) == WLC_EVENT_MSG_GROUP);
		break;
	}
#endif
	}
}
#endif /* WLC_HIGH */

void
wl_event_sync(wl_info_t *wl, char *ifname, wlc_event_t *e)
{
}

void
wl_event_sendup(wl_info_t *wl, const wlc_event_t *e, uint8 *data, uint32 len)
{
}

#ifndef WLC_LOW_ONLY
static int
wl_ioctl(hndrte_dev_t *dev, uint32 cmd, void *buf, int len, int *used, int *needed, int set)
{
	wl_info_t *wl = dev->softc;
	wl_if_t *wlif = WL_IF(wl, dev);
	struct wlc_if *wlcif = wlif != NULL ? wlif->wlcif : NULL;
	wlc_bsscfg_t *cfg = NULL;
	int ret = 0;
	int origcmd = cmd;
	int status = 0;
	uint32 *ret_int_ptr = (uint32 *)buf;

	WL_TRACE(("wl%d: wl_ioctl: cmd 0x%x\n", wl->unit, cmd));

	cfg = wlc_bsscfg_find_by_wlcif(wl->wlc, wlcif);
	ASSERT(cfg != NULL);
	switch (cmd) {
	case RTEGHWADDR:
		ret = wlc_iovar_op(wl->wlc, "cur_etheraddr", NULL, 0, buf, len, IOV_GET, wlcif);
		break;
	case RTESHWADDR:
		ret = wlc_iovar_op(wl->wlc, "cur_etheraddr", NULL, 0, buf, len, IOV_SET, wlcif);
		break;
	case RTEGPERMADDR:
		ret = wlc_iovar_op(wl->wlc, "perm_etheraddr", NULL, 0, buf, len, IOV_GET, wlcif);
		break;
	case RTEGMTU:
		*ret_int_ptr = ETHER_MAX_DATA;
		break;
#ifdef WLC_HIGH
	case RTEGSTATS:
		wl_statsupd(wl);
		bcopy(&wl->stats, buf, MIN(len, sizeof(wl->stats)));
		break;

	case RTEGALLMULTI:
		*ret_int_ptr = cfg->allmulti;
		break;
	case RTESALLMULTI:
		cfg->allmulti = *((uint32 *) buf);
		break;
#endif /* WLC_HIGH */
	case RTEGPROMISC:
		cmd = WLC_GET_PROMISC;
		break;
	case RTESPROMISC:
		cmd = WLC_SET_PROMISC;
		break;
#ifdef WLC_HIGH
	case RTESMULTILIST: {
		int i;

		/* copy the list of multicasts into our private table */
		cfg->nmulticast = len / ETHER_ADDR_LEN;
		for (i = 0; i < cfg->nmulticast; i++)
			cfg->multicast[i] = ((struct ether_addr *)buf)[i];
		break;
	}
#endif /* WLC_HIGH */
	case RTEGUP:
		cmd = WLC_GET_UP;
		break;
	default:
		/* force call to wlc ioctl handler */
		origcmd = -1;
		break;
	}

	if (cmd != origcmd) {
		if (!_wl_rte_oid_check(wl, cmd, buf, len, used, needed, set, &status))
			ret = wlc_ioctl(wl->wlc, cmd, buf, len, wlcif);
	}

	if (status)
		return status;

	return (ret);
}
#endif /* WLC_LOW_ONLY */

static int
BCMUNINITFN(wl_close)(hndrte_dev_t *dev)
{
	wl_info_t *wl = dev->softc;
	BCM_REFERENCE(wl);

	WL_TRACE(("wl%d: wl_close\n", wl->unit));

#ifdef WLC_HIGH
	/* BMAC_NOTE: ? */
	wl_down(wl);
#endif

	return 0;
}

#ifdef WLC_HIGH
static void
wl_statsupd(wl_info_t *wl)
{
	hndrte_stats_t *stats;

	WL_TRACE(("wl%d: wl_get_stats\n", wl->unit));

	stats = &wl->stats;

	/* refresh stats */
	if (wl->pub->up)
		wlc_statsupd(wl->wlc);

	stats->rx_packets = WLCNTVAL(wl->pub->_cnt->rxframe);
	stats->tx_packets = WLCNTVAL(wl->pub->_cnt->txframe);
	stats->rx_bytes = WLCNTVAL(wl->pub->_cnt->rxbyte);
	stats->tx_bytes = WLCNTVAL(wl->pub->_cnt->txbyte);
	stats->rx_errors = WLCNTVAL(wl->pub->_cnt->rxerror);
	stats->tx_errors = WLCNTVAL(wl->pub->_cnt->txerror);
	stats->rx_dropped = 0;
	stats->tx_dropped = 0;
	stats->multicast = WLCNTVAL(wl->pub->_cnt->rxmulti);
}
#endif /* WLC_HIGH */

void
BCMATTACHFN(wl_reclaim)(void)
{
#ifdef DONGLEBUILD
#ifdef BCMRECLAIM
	bcmreclaimed = TRUE;
#endif /* BCMRECLAIM */
	attach_part_reclaimed = TRUE;
	hndrte_reclaim();
#endif /* DONGLEBUILD */
}

#if defined(CONFIG_WLU) || defined(ATE_BUILD)
int
wl_get(void *wlc, int cmd, void *buf, int len)
{
	return wlc_ioctl(wlc, cmd, buf, len, NULL);
}

int
wl_set(void *wlc, int cmd, void *buf, int len)
{
	return wlc_ioctl(wlc, cmd, buf, len, NULL);
}

static void
do_wl_cmd(uint32 arg, uint argc, char *argv[])
{
	wl_info_t *wl = (wl_info_t *)arg;
	cmd_t *cmd;
	int ret = 0;

	if (argc < 2)
		printf("missing subcmd\n");
	else {
		/* search for command */
		for (cmd = wl_cmds; cmd->name && strcmp(cmd->name, argv[1]); cmd++);

		/* defaults to using the set_var and get_var commands */
		if (cmd->name == NULL)
			cmd = &wl_varcmd;

#ifdef ATE_BUILD
		if (cmd->name == NULL)
			printf("ATE: Command not supported!!!\n");
#endif
		/* do command */
		ret = (*cmd->func)(wl->wlc, cmd, argv + 1);
		printf("ret=%d (%s)\n", ret, bcmerrorstr(ret));
	}
}

#endif  /* CONFIG_WLU || ATE_BUILD */

#ifdef WLC_LOW_ONLY
static void
do_wlhist_cmd(uint32 arg, uint argc, char *argv[])
{
	wl_info_t *wl = (wl_info_t *)arg;

	if (strcmp(argv[1], "clear") == 0) {
		wlc_rpc_bmac_dump_txfifohist(wl->wlc_hw, FALSE);
		return;
	}

	wlc_rpc_bmac_dump_txfifohist(wl->wlc_hw, TRUE);
}

static void
do_wldpcdump_cmd(uint32 arg, uint argc, char *argv[])
{
	wl_info_t *wl = (wl_info_t *)arg;

	printf("wlc_dpc(): stopped = %d, requested = %d\n", wl->dpc_stopped, wl->dpc_requested);
	printf("\n");
}
#endif /* WLC_LOW_ONLY */
#ifdef BCMDBG
/* Mini command to control msglevel for BCMDBG builds */
static void
do_wlmsg_cmd(uint32 arg, uint argc, char *argv[])
{
	switch (argc) {
	case 3:
		/* Set both msglevel and msglevel2 */
		wl_msg_level2 = strtoul(argv[2], 0, 0);
		/* fall through */
	case 2:
		/* Set msglevel */
		wl_msg_level = strtoul(argv[1], 0, 0);
		break;
	case 1:
		/* Display msglevel and msglevel2 */
		printf("msglvl1=0x%x msglvl2=0x%x\n", wl_msg_level, wl_msg_level2);
		break;
	}
}
#endif /* BCMDBG */

#ifdef NOT_YET
static int
BCMATTACHFN(wl_module_init)(si_t *sih)
{
	uint16 id;

	WL_TRACE(("wl_module_init: add WL device\n"));

	if ((id = si_d11_devid(sih)) == 0xffff)
		id = BCM4318_D11G_ID;

	return hndrte_add_device(&bcmwl, D11_CORE_ID, id);
}

HNDRTE_MODULE_INIT(wl_module_init);

#endif /* NOT_YET */

#ifdef WLC_LOW_ONLY


#if defined(HNDRTE_PT_GIANT) && defined(DMA_TX_FREE)
static void
wl_lowmem_free(void *wlh)
{
	wl_info_t *wl = (wl_info_t*)wlh;
	wlc_info_t *wlc = wl->wlc;
	int i;

	/* process any tx reclaims */
	for (i = 0; i < NFIFO; i++) {
		hnddma_t *di = WLC_HW_DI(wlc, i);
		if (di == NULL)
			continue;
		dma_txreclaim(di, HNDDMA_RANGE_TRANSFERED);
	}
}
#endif /* HNDRTE_PT_GIANT && DMA_TX_FREE */

static void
wl_rpc_tp_txflowctl(hndrte_dev_t *dev, bool state, int prio)
{
	wl_info_t *wl = dev->softc;

	bcm_rpc_tp_txflowctl(wl->rpc_th, state, prio);
}

static void
wl_rpc_down(void *wlh)
{
	wl_info_t *wl = (wl_info_t*)(wlh);

	(void)wl;

	if (wlc_bmac_down_prep(wl->wlc_hw) == 0)
		(void)wlc_bmac_down_finish(wl->wlc_hw);
}

static void
wl_rpc_resync(void *wlh)
{
	wl_info_t *wl = (wl_info_t*)(wlh);

	/* reinit to all the default values */
	wlc_bmac_info_init(wl->wlc_hw);

	/* reload original  macaddr */
	wlc_bmac_reload_mac(wl->wlc_hw);
}

/* CLIENT dongle driver RPC dispatch routine, called by bcm_rpc_buf_recv()
 *  Based on request, push to common driver or send back result
 */
static void
wl_rpc_bmac_dispatch(void *ctx, struct rpc_buf* buf)
{
	wlc_rpc_ctx_t *rpc_ctx = (wlc_rpc_ctx_t *)ctx;

	wlc_rpc_bmac_dispatch(rpc_ctx, buf);
}

static void
wl_rpc_txflowctl(void *wlh, bool on)
{
	wl_info_t *wl = (wl_info_t *)(wlh);

	if (!wl->wlc_hw->up) {
		wl->dpc_stopped = FALSE;
		wl->dpc_requested = FALSE;
		return;
	}

	if (on) {	/* flowcontrol activated */
		if (!wl->dpc_stopped) {
			WL_TRACE(("dpc_stopped set!\n"));
			wl->dpc_stopped = TRUE;
		}
	} else {	/* flowcontrol released */

		if (!wl->dpc_stopped)
			return;

		WL_TRACE(("dpc_stopped cleared!\n"));
		wl->dpc_stopped = FALSE;

		/* if there is dpc requeset pending, run it */
		if (wl->dpc_requested) {
			wl->dpc_requested = FALSE;
			wl_dpc(wl);
		}
	}
}
#endif /* WLC_LOW_ONLY */


#ifndef WLC_LOW_ONLY
static bool
_wl_rte_oid_check(wl_info_t *wl, uint32 cmd, void *buf, int len, int *used, int *needed,
	bool set, int *status)
{
	return FALSE;
}
#endif /* WLC_LOW_ONLY */


#ifdef WL_WOWL_MEDIA
void wl_wowl_dngldown(struct wl_info *wl)
{
	hndrte_dev_t *chained = NULL;
	hndrte_dev_t *dev = NULL;

	dev = wl->dev;
	if (dev)
	  chained = dev->chained;

	if (chained && chained->funcs->wowldown) {
		chained->funcs->wowldown(chained);
	}
}
#endif
