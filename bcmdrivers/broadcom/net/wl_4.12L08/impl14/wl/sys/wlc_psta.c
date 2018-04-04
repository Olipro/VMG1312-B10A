/*
 * Proxy STA
 *
 * This module implements Proxy STA as well as the Wireless Repeater
 * features.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_psta.c 355514 2012-09-07 01:25:51Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <proto/802.3.h>
#include <proto/vlan.h>
#include <proto/bcmip.h>
#include <proto/bcmicmp.h>
#include <proto/bcmarp.h>
#include <proto/bcmudp.h>
#include <proto/bcmdhcp.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_assoc.h>
#include <wlc_scb.h>
#include <bcmwpa.h>

#include <wlc_psta.h>
#ifdef DPSTA
#include <dpsta.h>
#endif

/* PSTA private info structure */
struct wlc_psta_info {
	wlc_info_t	*wlc;		/* Pointer to wlc info struct */
	wlc_pub_t	*pub;		/* Pointer to wlc public data */
	uint8		rcmap[CEIL(RCMTA_SIZE, NBBY)];
	uint8		rcmta_idx;	/* RCMTA index */
	int32		inactivity;	/* Inactivity counter configured */
	bool		mrpt;		/* Support multi repeating */
#ifdef DPSTA
	psta_if_t	*pstaif;	/* PSTA interface id, used in DPSTA mode */
#endif
	int32		cfgh;		/* PSTA bsscfg cubby handle */
#ifdef WLCNT
	uint32		pstatxdhcpc;	/* DHCP client to server frames in tx dir */
	uint32		pstarxdhcpc;	/* DHCP client to server frames in rx dir */
	uint32		pstatxdhcps;	/* DHCP server to client frames in tx dir */
	uint32		pstarxdhcps;	/* DHCP server to client frames in rx dir */
	uint32		pstatxdhcpc6;	/* DHCP client to server frames in tx dir */
	uint32		pstarxdhcpc6;	/* DHCP client to server frames in rx dir */
	uint32		pstatxdhcps6;	/* DHCP server to client frames in tx dir */
	uint32		pstarxdhcps6;	/* DHCP server to client frames in rx dir */
	uint32		pstadupdetect;	/* Duplicate alias detected count */
#endif
};

/* Proxy STA Association info */
struct wlc_psa {
	bool		primary;	/* True if psta instance is primary */
	bool		ds_sta;		/* Indicates if it is wired/wireless client */
	wlc_bsscfg_t	*pcfg;		/* Pointer to primary psta bsscfg */
	wl_if_t		*pwlif;		/* Pointer to primary wlif */
	uint32		inactivity;	/* Num of sec the psta link is inactive */
	uint8		rcmta_idx;	/* RCMTA index */
	bool		allow_join;	/* Flag set to TRUE after config is done */
	struct ether_addr ds_ea;	/* MAC address of the client we are proxying */
#ifdef WLCNT
	uint32		txucast;	/* Frames sent on psta assoc */
	uint32		txnoassoc;	/* Frames dropped due to no assoc */
	uint32		rxucast;	/* Frames received on psta assoc */
	uint32		txbcmc;		/* BCAST/MCAST frames sent on all psta assoc */
	uint32		rxbcmc;		/* BCAST/MCAST frames recvd on all psta assoc */
#endif /* WLCNT */
};

/* Proxy STA bsscfg cubby */
typedef struct psta_bsscfg_cubby {
	wlc_psa_t	*psa;		/* PSTA assoc info */
} psta_bsscfg_cubby_t;

#define PSTA_BSSCFG_CUBBY(ps, cfg) ((psta_bsscfg_cubby_t *)BSSCFG_CUBBY((cfg), (ps)->cfgh))
#define PSTA_IS_PRIMARY(cfg)	((cfg) == wlc_bsscfg_primary((cfg)->wlc))

#define	PSTA_IS_DS_STA(s)	((s)->ds_sta)
#define	PSTA_JOIN_ALLOWED(s)	((s)->allow_join)

#define PSTA_MAX_INACT		600	/* PSTA inactivity timeout in seconds */

#define EA_CMP(e1, e2) \
	(!((((uint16 *)(e1))[0] ^ ((uint16 *)(e2))[0]) | \
	   (((uint16 *)(e1))[1] ^ ((uint16 *)(e2))[1]) | \
	   (((uint16 *)(e1))[2] ^ ((uint16 *)(e2))[2])))

#define	PSTA_SET_ALIAS(psta, psa, bc, ea) \
do { \
	ASSERT(EA_CMP((psa)->ds_ea.octet, (ea))); \
	*((struct ether_addr *)(ea)) = (bc)->cur_etheraddr; \
} while (0)

#define	PSTA_CLR_ALIAS(psta, psa, bc, ea) \
do { \
	ASSERT(EA_CMP((bc)->cur_etheraddr.octet, (ea))); \
	*((struct ether_addr *)(ea)) = (psa)->ds_ea; \
} while (0)

#define	PSTA_IS_ALIAS(psta, bc, ea)	EA_CMP((bc)->cur_etheraddr.octet, (ea))

/* IOVar table */
enum {
	IOV_PSTA,
	IOV_IS_PSTA_IF,
	IOV_PSTA_INACT,
	IOV_PSTA_ALLOW_JOIN,
	IOV_PSTA_MRPT
};

static const bcm_iovar_t psta_iovars[] = {
	{"psta", IOV_PSTA, (IOVF_SET_DOWN), IOVT_INT8, 0 },
	{"psta_if", IOV_IS_PSTA_IF, (IOVF_GET_UP), IOVT_BOOL, 0 },
	{"psta_inact", IOV_PSTA_INACT, (0), IOVT_UINT32, 0 },
	{"psta_allow_join", IOV_PSTA_ALLOW_JOIN, (0), IOVT_UINT32, 0 },
	{"psta_mrpt", IOV_PSTA_MRPT, (IOVF_SET_DOWN), IOVT_BOOL, 0 },
	{NULL, 0, 0, 0, 0 }
};

/* Forward declaration of local functions */
static int32 wlc_psta_rcmta_alloc(wlc_psta_info_t *psta, uint8 *idx);
static void wlc_psta_rcmta_free(wlc_psta_info_t *psta, uint8 idx);
static int32 wlc_psta_watchdog(void *arg);
static int32 wlc_psta_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid,
                              const char *name, void *p, uint plen, void *a,
                              int32 alen, int vsize, struct wlc_if *wlcif);
static void csum_fixup_16(uint8 *chksum, uint8 *optr, int olen, uint8 *nptr, int32 nlen);
static int32 wlc_psta_icmp6_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, void **p,
                                 uint8 *ih, uint8 *icmph, uint16 icmplen, uint8 **shost,
                                 bool tx, bool is_bcmc);
static int32 wlc_psta_udp_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, void **p,
                               uint8 *ih, uint8 *uh, uint8 **shost, bool tx, bool is_bcmc);
static int32 wlc_psta_dhcp_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, void **p,
                                uint8 *uh, uint8 *dhcph, uint16 dhcplen,
                                uint16 port, uint8 **shost, bool tx, bool is_bcmc);
static int32 wlc_psta_dhcpc_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, uint8 *udph,
                                 uint8 *dhcp, uint16 dhcplen, bool tx, bool is_bcmc);
static int32 wlc_psta_dhcps_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, uint8 *udph,
                                 uint8 *dhcp, uint16 dhcplen, bool tx, bool is_bcmc);
static int32 wlc_psta_dhcpc6_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, uint8 *udph,
                                  uint8 *dhcp, uint16 dhcplen, bool tx, bool is_bcmc);
static int32 wlc_psta_dhcps6_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, uint8 *udph,
                                  uint8 *dhcp, uint16 dhcplen, bool tx, bool is_bcmc);
static int32 wlc_psta_arp_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, void **p,
                               uint8 *ah, uint8 **shost, bool tx, bool is_bcast);
static int32 wlc_psta_proto_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *pcfg, void **p,
                                 uint8 *eh, wlc_bsscfg_t **cfg, uint8 **shost, bool tx);
static int32 wlc_psta_create(wlc_psta_info_t *psta, wlc_info_t *wlc, struct ether_addr *ea,
                             wlc_bsscfg_t *pcfg);
static wlc_bsscfg_t *wlc_psta_find_by_ds_ea(wlc_psta_info_t *psta, uint8 *mac);
static struct scb *wlc_psta_client_is_ds_sta(wlc_psta_info_t *psta,
                                             struct ether_addr *mac);
static void *wlc_psta_pkt_alloc_copy(wlc_pub_t *pub, void *p);
static void wlc_psta_alias_create(wlc_psta_info_t *psta, wlc_bsscfg_t *pcfg,
                                  struct ether_addr *shost);
static int wlc_psta_bsscfg_init(void *context, wlc_bsscfg_t *cfg);
static void wlc_psta_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg);
static int32 wlc_psta_up(void *ctx);
#ifdef DPSTA
static bool wlc_psta_is_ds_sta(wlc_psta_info_t *psta, struct ether_addr *mac);
static bool wlc_psta_authorized(wlc_bsscfg_t *cfg);
#endif

/*
 * Initialize psta private context. It returns a pointer to the
 * psta private context if succeeded. Otherwise it returns NULL.
 */
wlc_psta_info_t *
BCMATTACHFN(wlc_psta_attach)(wlc_info_t *wlc)
{
	wlc_psta_info_t *psta;

	/* Allocate psta private info struct */
	psta = MALLOC(wlc->pub->osh, sizeof(wlc_psta_info_t));
	if (!psta) {
		WL_ERROR(("wl%d: %s: MALLOC failed, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->pub->osh)));
		return NULL;
	}

	/* Init psta private info struct */
	bzero(psta, sizeof(wlc_psta_info_t));
	psta->wlc = wlc;
	psta->pub = wlc->pub;
	psta->inactivity = PSTA_MAX_INACT;
	psta->mrpt = FALSE;

	/* Register module */
	wlc_module_register(wlc->pub, psta_iovars, "psta", psta, wlc_psta_doiovar,
	                    wlc_psta_watchdog, wlc_psta_up, NULL);
#ifdef BCMDBG
	wlc_dump_register(wlc->pub, "psta", (dump_fn_t)wlc_psta_dump, (void *)psta);
#endif

	/* Reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((psta->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(psta_bsscfg_cubby_t),
		wlc_psta_bsscfg_init, wlc_psta_bsscfg_deinit,
		NULL, (void *)psta)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve failed\n",
			wlc->pub->unit, __FUNCTION__));
		MFREE(wlc->pub->osh, psta, sizeof(wlc_psta_info_t));
		return NULL;
	}

	WL_PSTA(("wl%d: psta attach done\n", psta->pub->unit));

	return psta;
}

static int
wlc_psta_bsscfg_init(void *context, wlc_bsscfg_t *cfg)
{
	wlc_psta_info_t *psta = (wlc_psta_info_t *)context;
	psta_bsscfg_cubby_t *psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
	wlc_pub_t *pub = psta->pub;

	psta_cfg->psa = MALLOC(pub->osh, sizeof(wlc_psa_t));
	if (psta_cfg->psa == NULL)
		return BCME_NOMEM;

	bzero((char *)psta_cfg->psa, sizeof(wlc_psa_t));

	return BCME_OK;
}

static void
wlc_psta_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg)
{
	wlc_psta_info_t *psta = (wlc_psta_info_t *)context;
	psta_bsscfg_cubby_t *psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);

	MFREE(psta->pub->osh, psta_cfg->psa, sizeof(wlc_psa_t));
}

static int32
wlc_psta_up(void *context)
{
	wlc_psta_info_t *psta = (wlc_psta_info_t *)context;

	if (PSTA_ENAB(psta->pub)) {
		/* Initialize and enable PSTA */
		wlc_psta_init(psta, psta->wlc->cfg);
		wlc_mhf(psta->wlc, MHF4, MHF4_PROXY_STA, MHF4_PROXY_STA, WLC_BAND_AUTO);
	} else
		/* Disable PSTA */
		wlc_mhf(psta->wlc, MHF4, MHF4_PROXY_STA, 0, WLC_BAND_AUTO);

	return BCME_OK;
}

/* Alloc rcmta slot. First half of the table is assigned to TAs and the
 * second half to RAs.
 */
static int32
wlc_psta_rcmta_alloc(wlc_psta_info_t *psta, uint8 *idx)
{
	uint8 i, start_off;

	start_off = PSTA_RA_STRT_INDX;

	/* Find the first available slot and assign it */
	for (i = 0; i < PSTA_MAX_ASSOC(psta->wlc); i++) {
		if (isset(&psta->rcmap[0], start_off + i))
			continue;
		*idx = start_off + i;
		setbit(&psta->rcmap[0], *idx);
		WL_PSTA(("wl%d: Allocing RCMTA index %d\n", psta->pub->unit, *idx));
		ASSERT(*idx < RCMTA_SIZE);
		return BCME_OK;
	}

	return BCME_NORESOURCE;
}

/* Free rcmta slot */
static void
wlc_psta_rcmta_free(wlc_psta_info_t *psta, uint8 idx)
{
	if (idx >= RCMTA_SIZE)
		return;

	clrbit(&psta->rcmap[0], idx);

	WL_PSTA(("wl%d: Freed RCMTA index %d\n", psta->pub->unit, idx));

	return;
}

/* Cleanup psta private context */
void
BCMATTACHFN(wlc_psta_detach)(wlc_psta_info_t *psta)
{
	if (!psta)
		return;

	/* Clear primary bss rmcta entry */
	wlc_psta_rcmta_free(psta, psta->rcmta_idx);

	/* Clear psta rcmta entries */

	wlc_module_unregister(psta->pub, "psta", psta);
	MFREE(psta->pub->osh, psta, sizeof(wlc_psta_info_t));
}

uint8
wlc_psta_rcmta_idx(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg)
{
	psta_bsscfg_cubby_t *psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
	return psta_cfg->psa->rcmta_idx;
}

void
wlc_psta_deauth_client(wlc_psta_info_t *psta, struct ether_addr *addr)
{
	wlc_bsscfg_t *cfg;
	struct scb *scb;
	struct ether_addr psta_ha;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	WL_PSTA(("wl%d: Rcvd deauth from client %s\n",
	         psta->pub->unit, bcm_ether_ntoa(addr, eabuf)));

	/* See if the mac address belongs to a downstream client */
	scb = wlc_psta_client_is_ds_sta(psta, addr);
	if (scb == NULL)
		return;

	if (SCB_PS(scb)) {
		WL_ERROR(("wl%d: Proxy STA %s ignoring bsscfg_free while in PS\n",
			psta->pub->unit, bcm_ether_ntoa(addr, eabuf)));
		return;
	}

	WL_PSTA(("wl%d: Sending deauth for ds client %s\n",
	         psta->pub->unit, bcm_ether_ntoa(addr, eabuf)));

	/* Find the psta using the client's original address */
	psta_ha = *addr;
	cfg = wlc_psta_find_by_ds_ea(psta, (uint8 *)&psta_ha);
	if (cfg == NULL) {
		WL_ERROR(("wl%d: Proxy STA %s link is already gone !!??\n",
		          psta->pub->unit, bcm_ether_ntoa(&psta_ha, eabuf)));
		return;
	}

	/* Cleanup the proxy client state */
	wlc_disassociate_client(cfg, FALSE, NULL, NULL);

	/* Send deauth to our AP with unspecified reason code */
	scb = wlc_scbfind(psta->wlc, cfg, &cfg->BSSID);
	(void)wlc_senddeauth(psta->wlc, &cfg->BSSID, &cfg->BSSID, &cfg->cur_etheraddr,
	                     scb, DOT11_RC_UNSPECIFIED);

	/* Down the bss */
	wlc_bsscfg_disable(psta->wlc, cfg);
	wlc_bsscfg_free(psta->wlc, cfg);

	WL_PSTA(("wl%d: Deauth for client %s complete\n",
	         psta->pub->unit, bcm_ether_ntoa(addr, eabuf)));
}

/*
 * Create the primary proxy sta association. Primry proxy sta connection is
 * initiated using the hardware address of the interface.
 */
int32
BCMINITFN(wlc_psta_init)(wlc_psta_info_t *psta, wlc_bsscfg_t *pcfg)
{
	int32 i;
	psta_bsscfg_cubby_t *psta_cfg;
#ifdef DPSTA
	psta_if_api_t api;
#endif

	if (!BSSCFG_STA(pcfg))
		return BCME_OK;

	WL_PSTA(("wl%d: %s: PSTA init\n", psta->pub->unit, __FUNCTION__));

	/* Clear RCMTA entries */
	for (i = 0; i < RCMTA_SIZE; i++)
		wlc_set_rcmta(psta->wlc, i, &ether_null);

	/* Set the the primary bss mac */
	psta->rcmta_idx = PSTA_RA_PRIM_INDX;
	wlc_set_rcmta(psta->wlc, psta->rcmta_idx, &pcfg->cur_etheraddr);

	/* Mark as primary */
	psta_cfg = PSTA_BSSCFG_CUBBY(psta, pcfg);
	psta_cfg->psa->primary = TRUE;
	psta_cfg->psa->rcmta_idx = PSTA_RA_PRIM_INDX;

	/* Initialize the mac address of primary */
	bcopy(&pcfg->cur_etheraddr, &psta_cfg->psa->ds_ea, ETHER_ADDR_LEN);

	WLCNTSET(psta->pstatxdhcpc, 0);
	WLCNTSET(psta->pstarxdhcpc, 0);
	WLCNTSET(psta->pstatxdhcps, 0);
	WLCNTSET(psta->pstarxdhcps, 0);
	WLCNTSET(psta->pstatxdhcpc6, 0);
	WLCNTSET(psta->pstarxdhcpc6, 0);
	WLCNTSET(psta->pstatxdhcps6, 0);
	WLCNTSET(psta->pstarxdhcps6, 0);
	WLCNTSET(psta->pstadupdetect, 0);

#ifdef DPSTA
	/* Register proxy sta APIs with DPSTA module */
	api.is_ds_sta = (bool (*)(void *, struct ether_addr *))wlc_psta_is_ds_sta;
	api.psta_find = (void *(*)(void *, uint8 *))wlc_psta_find;
	api.bss_auth = (bool (*)(void *))wlc_psta_authorized;
	api.psta = psta;
	api.bsscfg = pcfg;
	psta->pstaif = dpsta_register(psta->pub->unit, &api);
#endif

	return BCME_OK;
}

static int32
wlc_psta_watchdog(void *arg)
{
	wlc_psta_info_t *psta = arg;
	int32 idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb;
	psta_bsscfg_cubby_t *psta_cfg;

	if (!PSTA_ENAB(psta->pub))
		return BCME_OK;

	/* Cleanup the active proxy stas, and those that never came up too */
	FOREACH_PSTA(psta->wlc, idx, cfg) {
		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
		if (psta_cfg->psa->inactivity++ <= psta->inactivity)
			continue;

		/* Cleanup the proxy client state */
		wlc_disassociate_client(cfg, FALSE, NULL, NULL);

		/* Send deauth to our AP with unspecified reason code */
		scb = wlc_scbfind(psta->wlc, cfg, &cfg->BSSID);
		(void)wlc_senddeauth(psta->wlc, &cfg->BSSID, &cfg->BSSID,
		                     &cfg->cur_etheraddr,
		                     scb, DOT11_RC_UNSPECIFIED);

		wlc_bsscfg_disable(psta->wlc, cfg);
		wlc_bsscfg_free(psta->wlc, cfg);
	}

	return BCME_OK;
}

/* Handling psta related iovars */
static int32
wlc_psta_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
                 void *p, uint plen, void *a, int32 alen, int vsize, struct wlc_if *wlcif)
{
	wlc_psta_info_t *psta = hdl;
	wlc_info_t *wlc;
	int32 err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	wlc_bsscfg_t *cfg;
	bool bool_val;
	psta_bsscfg_cubby_t *psta_cfg;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	wlc = psta->wlc;

	/* Convenience int and bool vals for first 8 bytes of buffer */
	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	if (plen >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)p + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* Convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)a;

	/* bool conversion to avoid duplication below */
	bool_val = int_val != 0;

	cfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(cfg != NULL);

	switch (actionid) {
	case IOV_GVAL(IOV_PSTA):
		*ret_int_ptr = (int32)wlc->pub->_psta;
		break;

	case IOV_SVAL(IOV_PSTA):
		if ((int_val < PSTA_MODE_DISABLED) || (int_val > PSTA_MODE_REPEATER))
			err = BCME_RANGE;
		else
			wlc->pub->_psta = (int8)int_val;
		break;

	case IOV_GVAL(IOV_IS_PSTA_IF):
		*ret_int_ptr = (int32)cfg->_psta;
		break;

	case IOV_SVAL(IOV_PSTA_INACT):
		psta->inactivity = int_val;
		break;

	case IOV_GVAL(IOV_PSTA_INACT):
		*ret_int_ptr = psta->inactivity;
		break;

	case IOV_SVAL(IOV_PSTA_ALLOW_JOIN):
		/* Only valid for dynamic proxy sta instances. It doesn't apply for
		 * sta and ap because they are configured before hand.
		 */
		if (!BSSCFG_PSTA(cfg))
			err = BCME_BADARG;

		if (int_val) {
			WL_PSTA(("wl%d: Allow the join for %s now\n",
			         psta->pub->unit,
			         bcm_ether_ntoa(&cfg->cur_etheraddr, eabuf)));
		}

		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
		psta_cfg->psa->allow_join = int_val;

		/* Disable bss when joins are denied */
		if (!PSTA_JOIN_ALLOWED(psta_cfg->psa))
			wlc_bsscfg_disable(wlc, cfg);
		break;

	case IOV_GVAL(IOV_PSTA_ALLOW_JOIN):
		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
		*ret_int_ptr = psta_cfg->psa->allow_join;
		break;

	case IOV_SVAL(IOV_PSTA_MRPT):
		/* Silently ignore duplicate requests */
		if (psta->mrpt == bool_val)
			break;

		/* Multi repeater mode supported only when we are repeater */
		if (PSTA_IS_PROXY(wlc)) {
			err = BCME_UNSUPPORTED;
			break;
		}

		psta->mrpt = bool_val;
		break;

	case IOV_GVAL(IOV_PSTA_MRPT):
		*ret_int_ptr = (int32)psta->mrpt;
		break;

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

/* Called when the bss is disabled to clear the psta state of the bss */
void
wlc_psta_disable(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg)
{
	psta_bsscfg_cubby_t *psta_cfg;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	/* Nothing to do if the bss is not sta */
	if (!BSSCFG_STA(cfg))
		return;

	/* Turn roam off */
	cfg->roam->off = FALSE;

	/* Clear the rmcta entry of psta bss mac */
	psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
	wlc_set_rcmta(psta->wlc, psta_cfg->psa->rcmta_idx, &ether_null);

	WL_PSTA(("wl%d: Freeing RCMTA idx %d for etheraddr %s\n", psta->pub->unit,
	         psta_cfg->psa->rcmta_idx, bcm_ether_ntoa(&cfg->cur_etheraddr, eabuf)));

	/* Free the rcmta index */
	wlc_psta_rcmta_free(psta, psta_cfg->psa->rcmta_idx);
	psta_cfg->psa->rcmta_idx = 0;
	psta_cfg->psa->ds_sta = FALSE;
	psta_cfg->psa->allow_join = FALSE;
	psta_cfg->psa->pcfg = NULL;
	psta_cfg->psa->pwlif = NULL;

	/* Clear the counters */
	psta_cfg->psa->txucast = 0;
	psta_cfg->psa->txnoassoc = 0;
	psta_cfg->psa->rxucast = 0;
	psta_cfg->psa->txbcmc = 0;
	psta_cfg->psa->rxbcmc = 0;

	return;
}

void
wlc_psta_disable_all(wlc_psta_info_t *psta)
{
	int32 idx;
	wlc_bsscfg_t *bsscfg;

	WL_PSTA(("wl%d: Disabling all PSTAs\n", psta->pub->unit));

	/* Cleanup all active proxy stas */
	FOREACH_PSTA(psta->wlc, idx, bsscfg) {
		wlc_psta_disable(psta, bsscfg);
		wlc_bsscfg_free(psta->wlc, bsscfg);
	}

	return;
}

void
wlc_psta_build_ie(wlc_info_t *wlc, member_of_brcm_prop_ie_t *member_of_brcm_prop_ie)
{
	member_of_brcm_prop_ie->id = DOT11_MNG_PROPR_ID;
	member_of_brcm_prop_ie->len = MEMBER_OF_BRCM_PROP_IE_LEN;
	bcopy(BRCM_PROP_OUI, &member_of_brcm_prop_ie->oui[0], DOT11_OUI_LEN);
	member_of_brcm_prop_ie->type = MEMBER_OF_BRCM_PROP_IE_TYPE;
	bcopy(&wlc->cfg->cur_etheraddr, &member_of_brcm_prop_ie->ea, sizeof(struct ether_addr));
}

static int32
wlc_psta_create(wlc_psta_info_t *psta, wlc_info_t *wlc, struct ether_addr *ea,
                wlc_bsscfg_t *pcfg)
{
	int32 err, idx;
	wl_assoc_params_t assoc_params;
	wlc_bsscfg_t *cfg;
	struct ether_addr ds_ea;
	chanspec_t chanspec;
	psta_bsscfg_cubby_t *psta_cfg;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	/* Avoid overlapping joins */
	if (AS_IN_PROGRESS(wlc))
		return BCME_NOTASSOCIATED;

	/* Do the bss lookup using the psta's mac address */
	cfg = wlc_bsscfg_find_by_hwaddr(wlc, ea);

	WL_PSTA(("wl%d: Checking PSTA assoc count is %d/%d, PSTA cfg %p\n",
		psta->pub->unit, PSTA_BSS_COUNT(wlc), PSTA_MAX_ASSOC(wlc), cfg));

	if (cfg == NULL) {
		/* Disallow new assocs after reaching the max psta connection limit */
		if (PSTA_BSS_COUNT(wlc) >= PSTA_MAX_ASSOC(wlc)) {
			WL_PSTA(("wl%d: PSTA assoc limit %d reached.\n",
				psta->pub->unit, PSTA_MAX_ASSOC(wlc)));
			return BCME_NOTASSOCIATED;
		}

		/* Get the first available bss index */
		idx = wlc_bsscfg_get_free_idx(wlc);
		if (idx < 0) {
			WL_ERROR(("wl%d: PSTA connections limit exceeded\n",
			          psta->pub->unit));
			return BCME_NORESOURCE;
		}

		/* Allocate bsscfg */
		cfg = wlc_bsscfg_alloc(wlc, idx, 0, NULL, FALSE);
		if (cfg == NULL)
			return BCME_NOMEM;
		else if ((err = wlc_bsscfg_init(wlc, cfg))) {
			WL_ERROR(("wl%d: wlc_bsscfg_init failed, err %d\n",
			          psta->pub->unit, err));
			wlc_bsscfg_free(wlc, cfg);
			return err;
		}

		WL_PSTA(("wl%d: Allocated bsscfg for idx %d\n", psta->pub->unit, idx));

		ds_ea = *ea;

		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);

		/* Mark the client as ds sta if we are creating proxy assoc for a
		 * downstream wireless sta.
		 */
		psta_cfg->psa->ds_sta = (wlc_psta_client_is_ds_sta(psta, &ds_ea) != NULL);

		/* Save the original address of downstream client */
		bcopy(&ds_ea, &psta_cfg->psa->ds_ea, ETHER_ADDR_LEN);

		/* Create an alias for this client. Use this alias while talking
		 * to the upstream ap/rptr. Hopefully we generate random enough
		 * numbers so that there won't be duplicate mac addresses.
		 */
		if (PSTA_IS_REPEATER(psta->wlc)) {
			if (psta->mrpt)
				wlc_psta_alias_create(psta, pcfg, ea);
			else
				ETHER_SET_LOCALADDR(ea);
		}
	} else {
		ASSERT(!PSTA_IS_PRIMARY(cfg));

		/* Nothing to do if bss is already enabled and assoc state is
		 * not idle.
		 */
		if (cfg->enable && (cfg->assoc->state != AS_IDLE))
			return BCME_NOTASSOCIATED;

		wlc_bsscfg_enable(wlc, cfg);

		WL_PSTA(("wl%d: Enabled bsscfg for idx %d\n", psta->pub->unit,
		         WLC_BSSCFG_IDX(cfg)));

		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
	}

	/* Turn psta on */
	cfg->_psta = TRUE;
#ifdef MCAST_REGEN
	cfg->mcast_regen_enable = pcfg->mcast_regen_enable;
#endif /* MCAST_REGEN */

	/* Turn roam off */
	cfg->roam->off = TRUE;

	bcopy(ea, &cfg->cur_etheraddr, ETHER_ADDR_LEN);

	/* Link the primary cfg */
	psta_cfg->psa->pcfg = pcfg;

	/* Save the wlif pointer of the primary */
	psta_cfg->psa->pwlif = pcfg->wlcif->wlif;

	/* Allocate rcmta entry for the psta bss mac */
	if ((psta_cfg->psa->rcmta_idx == 0) &&
	    (err = wlc_psta_rcmta_alloc(psta, &psta_cfg->psa->rcmta_idx)) != BCME_OK)
		return err;

	WL_PSTA(("wl%d: Use RCMTA idx %d for etheraddr %s\n", psta->pub->unit,
		psta_cfg->psa->rcmta_idx, bcm_ether_ntoa(ea, eabuf)));

	/* Set the psta bss mac */
	wlc_set_rcmta(psta->wlc, psta_cfg->psa->rcmta_idx, &cfg->cur_etheraddr);

	/* When security is enabled we want to make sure that the psta is
	 * configured before initiating the joins. The configuration layer
	 * is reponsible for signaling to the psta driver.
	 */
	if (!PSTA_JOIN_ALLOWED(psta_cfg->psa)) {
		WL_PSTA(("wl%d: Hold the join for %s until config is done\n",
		         psta->pub->unit, bcm_ether_ntoa(ea, eabuf)));
		return BCME_NOTASSOCIATED;
	}

	/* Join the UAP */
	bcopy(&pcfg->BSSID, &assoc_params.bssid, ETHER_ADDR_LEN);
	chanspec = pcfg->current_bss->chanspec;
	assoc_params.chanspec_list[0] = CH20MHZ_CHSPEC(wf_chspec_ctlchan(chanspec));
	assoc_params.chanspec_num = 1;
	wlc_join(wlc, cfg, pcfg->SSID, pcfg->SSID_len, NULL,
	         &assoc_params, sizeof(wl_assoc_params_t));

	return BCME_NOTASSOCIATED;
}

/*
 * Adjust 16 bit checksum - taken from RFC 3022.
 *
 *   The algorithm below is applicable only for even offsets (i.e., optr
 *   below must be at an even offset from start of header) and even lengths
 *   (i.e., olen and nlen below must be even).
 */
static void
csum_fixup_16(uint8 *chksum, uint8 *optr, int olen, uint8 *nptr, int nlen)
{
	long x, old, new;

	ASSERT(!((int)optr&1) && !(olen&1));
	ASSERT(!((int)nptr&1) && !(nlen&1));

	x = (chksum[0]<< 8)+chksum[1];
	if (!x)
		return;
	x = ~x & 0xffff;
	while (olen)
	{
		old = (optr[0]<< 8)+optr[1]; optr += 2;
		x -= old & 0xffff;
		if (x <= 0) { x--; x &= 0xffff; }
		olen -= 2;
	}
	while (nlen)
	{
		new = (nptr[0]<< 8)+nptr[1]; nptr += 2;
		x += new & 0xffff;
		if (x & 0x10000) { x++; x &= 0xffff; }
		nlen -= 2;
	}
	x = ~x & 0xffff;
	chksum[0] = (uint8)(x >> 8); chksum[1] = (uint8)x;
}

/* Search the specified dhcp option and return the offset of it */
static int32
wlc_psta_dhcp_option_find(uint8 *dhcp, uint16 dhcplen, uint16 option_code)
{
	bool found = FALSE;
	uint16 optlen, offset;

	if (dhcplen <= DHCP_OPT_OFFSET) {
		WL_PSTA(("%s: no options, dhcplen %d less than 236\n",
		         __FUNCTION__, dhcplen));
		return -1;
	}

	offset = DHCP_OPT_OFFSET;

	/* First option must be magic cookie */
	if ((dhcp[offset + 0] != 0x63) || (dhcp[offset + 1] != 0x82) ||
	    (dhcp[offset + 2] != 0x53) || (dhcp[offset + 3] != 0x63)) {
		WL_PSTA(("%s: magic cookie 0x%x mismatch\n",
		         __FUNCTION__, *(uint32 *)(dhcp + offset)));
		return -1;
	}

	offset += 4;

	while (offset < dhcplen) {
		/* End of options */
		if (*(uint8 *)(dhcp + offset) == 255) {
			WL_PSTA(("%s: opt %d not found\n", __FUNCTION__, option_code));
			break;
		}

		if (*(uint8 *)(dhcp + offset + DHCP_OPT_CODE_OFFSET) == option_code) {
			found = TRUE;
			break;
		}

		optlen = *(uint8 *)(dhcp + offset + DHCP_OPT_LEN_OFFSET) + 2;
		offset += optlen;
	}

	return found ? offset : -1;
}

/* Process DHCP client frame (client to server) */
static int32
wlc_psta_dhcpc_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, uint8 *udph,
                    uint8 *dhcp, uint16 dhcplen, bool tx, bool is_bcmc)
{
	uint8 chaddr[ETHER_ADDR_LEN];
	int32 opt_offset;
#ifdef BCMDBG
	psta_bsscfg_cubby_t *psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
	int8 eabuf[ETHER_ADDR_STR_LEN];
	BCM_REFERENCE(psta_cfg);
#endif /* BCMDBG */

	/* Only interested in requests when sending to server in tx dir */
	if (!tx) {
		WLCNTINCR(psta->pstarxdhcpc);
		return BCME_OK;
	} else {
		WLCNTINCR(psta->pstatxdhcpc);
		if (*(dhcp + DHCP_TYPE_OFFSET) != DHCP_TYPE_REQUEST)
			return BCME_OK;
	}

	bcopy(dhcp + DHCP_CHADDR_OFFSET, chaddr, ETHER_ADDR_LEN);

	ASSERT(tx && !PSTA_IS_ALIAS(psta, cfg, chaddr));

	WL_PSTA(("wl%d: %s: tx %d dhcp chaddr %s\n", psta->pub->unit,
	         __FUNCTION__, (int32)tx,
	         bcm_ether_ntoa((struct ether_addr *)chaddr, eabuf)));

	/* Only modify if chaddr is one of the clients */
	if (wlc_psta_find_by_ds_ea(psta, chaddr) != NULL) {
		PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, chaddr);

		/* For DHCP requests, replace chaddr with host's MAC */
		csum_fixup_16(udph + UDP_CHKSUM_OFFSET,
		              dhcp + DHCP_CHADDR_OFFSET, ETHER_ADDR_LEN,
		              chaddr, ETHER_ADDR_LEN);
		PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, dhcp + DHCP_CHADDR_OFFSET);

		WL_PSTA(("wl%d: %s: dhcp chaddr modified as %s\n",
		         psta->pub->unit, __FUNCTION__,
		         bcm_ether_ntoa((struct ether_addr *)
		         (dhcp + DHCP_CHADDR_OFFSET), eabuf)));
	}

	/* If client identifier option is not present then no processing required */
	opt_offset = wlc_psta_dhcp_option_find(dhcp, dhcplen, DHCP_OPT_CODE_CLIENTID);
	if (opt_offset == -1) {
		WL_PSTA(("wl%d: %s: dhcp clientid option not found\n",
		         psta->pub->unit, __FUNCTION__));
		return BCME_OK;
	}

	/* First octet indicates type of data. If type is 1, it indicates
	 * that next 6 octets contain mac address.
	 */
	if (*(dhcp + opt_offset + DHCP_OPT_DATA_OFFSET) == 0x01) {
		uint32 offset, mac_offset = opt_offset + DHCP_OPT_DATA_OFFSET + 1;
		uint8 cli_addr[ETHER_ADDR_LEN];

		bcopy(dhcp + mac_offset, chaddr, ETHER_ADDR_LEN);

		/* Only modify if chaddr is one of the clients */
		if (wlc_psta_find_by_ds_ea(psta, chaddr) == NULL)
			return BCME_OK;

		PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, chaddr);

		/* For DHCP requests, replace chaddr with host's MAC */
		if (mac_offset & 1) {
			cli_addr[0] = dhcp[mac_offset - 1];
			bcopy(chaddr, cli_addr + 1, ETHER_ADDR_LEN - 1);
			offset = mac_offset - 1;
		} else {
			bcopy(chaddr, cli_addr, ETHER_ADDR_LEN);
			offset = mac_offset;
		}

		csum_fixup_16(udph + UDP_CHKSUM_OFFSET,
		              dhcp + offset, ETHER_ADDR_LEN,
		              cli_addr, ETHER_ADDR_LEN);
		PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, dhcp + mac_offset);

		WL_PSTA(("wl%d: %s: dhcp client id modified as %s\n",
		         psta->pub->unit, __FUNCTION__,
		         bcm_ether_ntoa((struct ether_addr *)(dhcp + mac_offset), eabuf)));
	}

	return BCME_OK;
}

/* Process DHCP server frame (server to client) */
static int32
wlc_psta_dhcps_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, uint8 *udph,
                    uint8 *dhcp, uint16 dhcplen, bool tx, bool is_bcmc)
{
	uint8 chaddr[ETHER_ADDR_LEN];
	int32 opt_offset;
	psta_bsscfg_cubby_t *psta_cfg;
#ifdef BCMDBG
	int8 eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	/* Only interested in replies when receiving from server in rx dir */
	if (tx) {
		WLCNTINCR(psta->pstatxdhcps);
		return BCME_OK;
	} else {
		WLCNTINCR(psta->pstarxdhcps);
		if (*(dhcp + DHCP_TYPE_OFFSET) != DHCP_TYPE_REPLY)
			return BCME_OK;
	}

	bcopy(dhcp + DHCP_CHADDR_OFFSET, chaddr, ETHER_ADDR_LEN);

	WL_PSTA(("wl%d: %s: tx %d dhcp chaddr %s\n", psta->pub->unit,
	         __FUNCTION__, (int32)tx,
	         bcm_ether_ntoa((struct ether_addr *)chaddr, eabuf)));

	if (!is_bcmc)
		ASSERT(!tx && PSTA_IS_ALIAS(psta, cfg, chaddr));

	/* Only modify if chaddr is one of the proxy clients */
	if ((cfg = wlc_psta_find(psta, chaddr)) != NULL) {
		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
		PSTA_CLR_ALIAS(psta, psta_cfg->psa, cfg, chaddr);

		csum_fixup_16(udph + UDP_CHKSUM_OFFSET,
		              dhcp + DHCP_CHADDR_OFFSET, ETHER_ADDR_LEN,
		              chaddr, ETHER_ADDR_LEN);
		PSTA_CLR_ALIAS(psta, psta_cfg->psa, cfg, dhcp + DHCP_CHADDR_OFFSET);

		WL_PSTA(("wl%d: %s: dhcp chaddr modified as %s\n",
		         psta->pub->unit, __FUNCTION__,
		         bcm_ether_ntoa((struct ether_addr *)
		         (dhcp + DHCP_CHADDR_OFFSET), eabuf)));
	}

	/* If client identifier option is not present then
	 * no processing required.
	 */
	opt_offset = wlc_psta_dhcp_option_find(dhcp, dhcplen, DHCP_OPT_CODE_CLIENTID);
	if (opt_offset == -1) {
		WL_PSTA(("wl%d: %s: dhcp clientid option not found\n",
		         psta->pub->unit, __FUNCTION__));
		return BCME_OK;
	}

	/* First octet indicates type of data. If type is 1, it indicates
	 * that next 6 octets contain mac address.
	 */
	if (*(dhcp + opt_offset + DHCP_OPT_DATA_OFFSET) == 0x01) {
		uint32 offset, mac_offset = opt_offset + DHCP_OPT_DATA_OFFSET + 1;
		uint8 cli_addr[ETHER_ADDR_LEN];

		bcopy(dhcp + mac_offset, chaddr, ETHER_ADDR_LEN);

		/* Only modify if chaddr is one of the proxy clients */
		if ((cfg = wlc_psta_find(psta, chaddr)) == NULL)
			return BCME_OK;

		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
		PSTA_CLR_ALIAS(psta, psta_cfg->psa, cfg, chaddr);

		/* For DHCP replies, replace chaddr with host's MAC */
		if (mac_offset & 1) {
			cli_addr[0] = dhcp[mac_offset - 1];
			bcopy(chaddr, cli_addr + 1, ETHER_ADDR_LEN - 1);
			offset = mac_offset - 1;
		} else {
			bcopy(chaddr, cli_addr, ETHER_ADDR_LEN);
			offset = mac_offset;
		}

		csum_fixup_16(udph + UDP_CHKSUM_OFFSET,
		              dhcp + offset, ETHER_ADDR_LEN,
		              cli_addr, ETHER_ADDR_LEN);
		PSTA_CLR_ALIAS(psta, psta_cfg->psa, cfg, dhcp + mac_offset);

		WL_PSTA(("wl%d: %s: dhcp client id restored to %s\n",
		         psta->pub->unit, __FUNCTION__,
		         bcm_ether_ntoa((struct ether_addr *)(dhcp + mac_offset), eabuf)));
	}

	return BCME_OK;
}

/* Search the specified dhcp6 option and return the offset of it */
static int32
wlc_psta_dhcp6_option_find(uint8 *dhcp, uint16 dhcplen, uint8 msg_type, uint16 option_code)
{
	bool found = FALSE;
	uint16 len, optlen, offset;

	/* Get the pointer to options */
	if (msg_type == DHCP6_TYPE_RELAYFWD)
		offset = DHCP6_RELAY_OPT_OFFSET;
	else
		offset = DHCP6_MSG_OPT_OFFSET;

	len = offset;
	while (len < dhcplen) {
		if (*(uint16 *)(dhcp + offset) == HTON16(option_code)) {
			found = TRUE;
			break;
		}
		optlen = NTOH16(*(uint16 *)(dhcp + offset + DHCP6_OPT_LEN_OFFSET)) + 4;
		offset += optlen;
		len += optlen;
	}

	return found ? offset : -1;
}

/* Process DHCP client frame (client to server) */
static int32
wlc_psta_dhcpc6_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, uint8 *udph,
                     uint8 *dhcp, uint16 dhcplen, bool tx, bool is_bcmc)
{
	int32 opt_offset, duid_offset;
	uint8 chaddr[ETHER_ADDR_LEN];
#ifdef BCMDBG
	psta_bsscfg_cubby_t *psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
	int8 eabuf[ETHER_ADDR_STR_LEN];
	BCM_REFERENCE(psta_cfg);
#endif /* BCMDBG */

	/* Only interested in requests when sending to server in tx dir */
	if (!tx) {
		WLCNTINCR(psta->pstarxdhcpc6);
		return BCME_OK;
	} else {
		uint8 msg_type;

		WLCNTINCR(psta->pstatxdhcpc6);
		msg_type = *(dhcp + DHCP6_TYPE_OFFSET);
		ASSERT((msg_type == DHCP6_TYPE_SOLICIT) ||
		       (msg_type == DHCP6_TYPE_REQUEST) ||
		       (msg_type == DHCP6_TYPE_RENEW) ||
		       (msg_type == DHCP6_TYPE_REBIND) ||
		       (msg_type == DHCP6_TYPE_RELEASE) ||
		       (msg_type == DHCP6_TYPE_DECLINE) ||
		       (msg_type == DHCP6_TYPE_CONFIRM) ||
		       (msg_type == DHCP6_TYPE_INFOREQ) ||
		       (msg_type == DHCP6_TYPE_RELAYFWD));

		/* If client identifier option is not present then
		 * no processing required.
		 */
		opt_offset = wlc_psta_dhcp6_option_find(dhcp, dhcplen, msg_type,
		                                        DHCP6_OPT_CODE_CLIENTID);
		if (opt_offset == -1) {
			WL_PSTA(("wl%d: %s: dhcp6 clientid option not found\n",
			         psta->pub->unit, __FUNCTION__));
			return BCME_OK;
		}
	}

	duid_offset = (opt_offset + DHCP6_OPT_DATA_OFFSET);

	/* Look for DUID-LLT or DUID-LL */
	if (*(uint16 *)(dhcp + duid_offset) == HTON16(1))
		duid_offset += 8;
	else if (*(uint16 *)(dhcp + duid_offset) == HTON16(3))
		duid_offset += 4;
	else
		return BCME_OK;

	bcopy(dhcp + duid_offset, chaddr, ETHER_ADDR_LEN);

	ASSERT(tx && !PSTA_IS_ALIAS(psta, cfg, chaddr));

	WL_PSTA(("wl%d: %s: tx %d dhcp chaddr %s\n", psta->pub->unit,
	         __FUNCTION__, (int32)tx,
	         bcm_ether_ntoa((struct ether_addr *)chaddr, eabuf)));

	/* Only modify if chaddr is one of the clients */
	if (wlc_psta_find_by_ds_ea(psta, chaddr) == NULL)
		return BCME_OK;

	PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, chaddr);

	/* For DHCP requests, replace chaddr with host's MAC */
	csum_fixup_16(udph + UDP_CHKSUM_OFFSET,
	              dhcp + duid_offset, ETHER_ADDR_LEN,
	              chaddr, ETHER_ADDR_LEN);
	PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, dhcp + duid_offset);

	WL_PSTA(("wl%d: %s: dhcp chaddr modified as %s\n", psta->pub->unit, __FUNCTION__,
	         bcm_ether_ntoa((struct ether_addr *)(dhcp + duid_offset), eabuf)));

	return BCME_OK;
}

/* Process DHCP server frame (server to client) */
static int32
wlc_psta_dhcps6_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, uint8 *udph,
                     uint8 *dhcp, uint16 dhcplen, bool tx, bool is_bcmc)
{
	int32 opt_offset, duid_offset;
	uint8 chaddr[ETHER_ADDR_LEN];
	psta_bsscfg_cubby_t *psta_cfg;
#ifdef BCMDBG
	int8 eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	/* Only interested in replies received from server in rx dir */
	if (tx) {
		WLCNTINCR(psta->pstatxdhcps6);
		return BCME_OK;
	} else {
		uint8 msg_type;

		WLCNTINCR(psta->pstarxdhcps6);
		msg_type = *(dhcp + DHCP6_TYPE_OFFSET);
		ASSERT((msg_type == DHCP6_TYPE_ADVERTISE) ||
		       (msg_type == DHCP6_TYPE_REPLY) ||
		       (msg_type == DHCP6_TYPE_RECONFIGURE) ||
		       (msg_type == DHCP6_TYPE_RELAYREPLY));

		/* If server identifier option is not present then
		 * no processing required.
		 */
		opt_offset = wlc_psta_dhcp6_option_find(dhcp, dhcplen, msg_type,
		                                        DHCP6_OPT_CODE_CLIENTID);
		if (opt_offset == -1) {
			WL_PSTA(("wl%d: %s: dhcp6 clientid option not found\n",
			         psta->pub->unit, __FUNCTION__));
			return BCME_OK;
		}
	}

	duid_offset = (opt_offset + DHCP6_OPT_DATA_OFFSET);

	/* Look for DUID-LLT or DUID-LL */
	if (*(uint16 *)(dhcp + duid_offset) == HTON16(1))
		duid_offset += 8;
	else if (*(uint16 *)(dhcp + duid_offset) == HTON16(3))
		duid_offset += 4;
	else
		return BCME_OK;

	bcopy(dhcp + duid_offset, chaddr, ETHER_ADDR_LEN);

	WL_PSTA(("wl%d: %s: tx %d dhcp chaddr %s\n", psta->pub->unit,
	         __FUNCTION__, (int32)tx,
	         bcm_ether_ntoa((struct ether_addr *)chaddr, eabuf)));

	if (!is_bcmc)
		ASSERT(!tx && PSTA_IS_ALIAS(psta, cfg, chaddr));

	/* Only modify if chaddr is one of the proxy clients */
	if ((cfg = wlc_psta_find(psta, chaddr)) == NULL)
		return BCME_OK;

	psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
	PSTA_CLR_ALIAS(psta, psta_cfg->psa, cfg, chaddr);

	csum_fixup_16(udph + UDP_CHKSUM_OFFSET,
	              dhcp + duid_offset, ETHER_ADDR_LEN,
	              chaddr, ETHER_ADDR_LEN);
	PSTA_CLR_ALIAS(psta, psta_cfg->psa, cfg, dhcp + duid_offset);

	WL_PSTA(("wl%d: %s: dhcp chaddr modified as %s\n",
	         psta->pub->unit, __FUNCTION__,
	         bcm_ether_ntoa((struct ether_addr *)(dhcp + duid_offset), eabuf)));

	return BCME_OK;
}

static void *
wlc_psta_pkt_alloc_copy(wlc_pub_t *pub, void *p)
{
	void *n;
	uint32 totlen;

	totlen = pkttotlen(pub->osh, p);

	WL_PSTA(("wl%d: %s: Copying %d bytes\n", pub->unit, __FUNCTION__, totlen));

	if ((n = PKTGET(pub->osh, TXOFF + totlen, TRUE)) == NULL) {
		WL_ERROR(("wl%d: %s: PKTGET of length %d failed\n",
		          pub->unit, __FUNCTION__, TXOFF + PKTLEN(pub->osh, p)));
		return NULL;
	}
	PKTPULL(pub->osh, n, TXOFF);

	wlc_pkttag_info_move(pub, p, n);
	PKTSETPRIO(n, PKTPRIO(p));

	/* Copy packet data to new buffer */
	pktcopy(pub->osh, p, 0, totlen, PKTDATA(osh, n));

	return n;
}

/* Process ARP request and replies in tx and rx directions */
static int32
wlc_psta_arp_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, void **p, uint8 *ah,
                  uint8 **shost, bool tx, bool is_bcast)
{
	uint8 chaddr[ETHER_ADDR_LEN];
	psta_bsscfg_cubby_t *psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	/* Modify the source mac address in ARP header */
	if (tx) {
		/* Since we are going to modify the address in arp header
		 * let's make a copy of the whole packet. Otherwise we will
		 * end up modifying arp header of the frame that is being
		 * broadcast to other bridged interfaces.
		 */
		if (is_bcast) {
			void *n;

			if ((n = wlc_psta_pkt_alloc_copy(psta->pub, *p)) == NULL) {
				WLCNTINCR(psta->pub->_cnt->txnobuf);
				return BCME_NOMEM;
			}

			/* First buffer contains only l2 header */
			ah = PKTDATA(psta->pub->osh, n) + PKTLEN(psta->pub->osh, *p);
			PKTFREE(psta->pub->osh, *p, TRUE);
			*p = n;
			*shost = PKTDATA(psta->pub->osh, n) + ETHER_SRC_OFFSET;
		}

		/* Modify the src eth in arp header only if it matches any of the
		 * client's mac address.
		 */
		bcopy(ah + ARP_SRC_ETH_OFFSET, chaddr, ETHER_ADDR_LEN);
		ASSERT(!PSTA_IS_ALIAS(psta, cfg, chaddr));
		PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, chaddr);
		if (wlc_psta_find(psta, chaddr) == NULL)
			return BCME_OK;

		/* ARP requests or replies */
		WL_PSTA(("wl%d: Munge src eth in arp header %s\n", psta->pub->unit,
		         bcm_ether_ntoa((struct ether_addr *)(ah+8), eabuf)));

		PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, ah + ARP_SRC_ETH_OFFSET);
	} else {
		/* Restore the host/client's mac address in ARP reply */
		if (*(uint16 *)(ah + ARP_OPC_OFFSET) == HTON16(ARP_OPC_REPLY)) {
			bcopy(ah + ARP_TGT_ETH_OFFSET, chaddr, ETHER_ADDR_LEN);
			ASSERT(PSTA_IS_ALIAS(psta, cfg, chaddr));
			if (wlc_psta_find(psta, chaddr) == NULL)
				return BCME_OK;

			PSTA_CLR_ALIAS(psta, psta_cfg->psa, cfg, ah + ARP_TGT_ETH_OFFSET);
			WL_PSTA(("wl%d: Restored tgt eth to %s\n", psta->pub->unit,
			         bcm_ether_ntoa((struct ether_addr *)(ah+18), eabuf)));
		}
	}

	return BCME_OK;
}

static int32
wlc_psta_dhcp_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, void **p,
                   uint8 *uh, uint8 *dhcph, uint16 dhcplen, uint16 port,
                   uint8 **shost, bool tx, bool is_bcmc)
{
	/* Since we are going to modify the address in dhcp header
	 * let's make a copy of the whole packet. Otherwise we will
	 * end up modifying dhcp header of the frame that is being
	 * broadcast to other bridged interfaces.
	 */
	if (tx && is_bcmc) {
		void *n;
		uint8 *ih, proto;
		int32 hlen = 0;

		if ((n = wlc_psta_pkt_alloc_copy(psta->pub, *p)) == NULL) {
			WLCNTINCR(psta->pub->_cnt->txnobuf);
			return BCME_NOMEM;
		}

		/* First buffer contains only l2 header */
		ih = PKTDATA(psta->pub->osh, n) + PKTLEN(psta->pub->osh, *p);

		if (IP_VER(ih) == IP_VER_6) {
			proto = ih[IPV6_NEXT_HDR_OFFSET];
			if (IPV6_EXTHDR(proto)) {
				hlen = ipv6_exthdr_len(ih, &proto);
				if (hlen < 0)
					return BCME_OK;
			}
			hlen += IPV6_MIN_HLEN;
		} else
			hlen = IPV4_HLEN(ih);

		uh = ih + hlen;
		dhcph = uh + UDP_HDR_LEN;
		PKTFREE(psta->pub->osh, *p, TRUE);
		*p = n;
		*shost = PKTDATA(psta->pub->osh, n) + ETHER_SRC_OFFSET;
	}

	switch (port) {
	case DHCP_PORT_SERVER:
		return wlc_psta_dhcpc_proc(psta, cfg, uh, dhcph, dhcplen, tx, is_bcmc);
	case DHCP6_PORT_SERVER:
		return wlc_psta_dhcpc6_proc(psta, cfg, uh, dhcph, dhcplen, tx, is_bcmc);
	case DHCP_PORT_CLIENT:
		return wlc_psta_dhcps_proc(psta, cfg, uh, dhcph, dhcplen, tx, is_bcmc);
	case DHCP6_PORT_CLIENT:
		return wlc_psta_dhcps6_proc(psta, cfg, uh, dhcph, dhcplen, tx, is_bcmc);
	default:
		break;
	}

	return BCME_OK;
}

static int32
wlc_psta_udp_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, void **p, uint8 *ih, uint8 *uh,
                  uint8 **shost, bool tx, bool is_bcmc)
{
	uint16 port, dhcplen;
	bool is_dhcp;

	port = NTOH16(*(uint16 *)(uh + UDP_DEST_PORT_OFFSET));

	is_dhcp = (((IP_VER(ih) == IP_VER_4) &&
	            ((port == DHCP_PORT_SERVER) || (port == DHCP_PORT_CLIENT))) ||
	           ((IP_VER(ih) == IP_VER_6) &&
	            ((port == DHCP6_PORT_SERVER) || (port == DHCP6_PORT_CLIENT))));

	if (is_dhcp) {
		dhcplen = NTOH16(*(uint16 *)(uh + UDP_LEN_OFFSET)) - UDP_HDR_LEN;
		return wlc_psta_dhcp_proc(psta, cfg, p, uh,
		                          uh + UDP_HDR_LEN, dhcplen,
		                          port, shost, tx, is_bcmc);
	}

	return BCME_OK;
}

static int32
wlc_psta_icmp6_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg, void **p, uint8 *ih,
                    uint8 *icmph, uint16 icmplen, uint8 **shost, bool tx, bool is_bcmc)
{
	struct icmp6_opt *opt;
	uint8 chaddr[ETHER_ADDR_LEN];
	uint8 *src_haddr = NULL;
#ifdef BCMDBG
	psta_bsscfg_cubby_t *psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
	char eabuf[ETHER_ADDR_STR_LEN];
	BCM_REFERENCE(psta_cfg);
#endif /* BCMDBG */

	/* Since we are going to modify the address in icmp option
	 * let's make a copy of the whole packet.
	 */
	if (tx && is_bcmc) {
		void *n;
		uint8 proto;
		int32 hlen = 0;

		if ((n = wlc_psta_pkt_alloc_copy(psta->pub, *p)) == NULL) {
			WLCNTINCR(psta->pub->_cnt->txnobuf);
			return BCME_NOMEM;
		}

		/* First buffer contains only l2 header */
		ih = PKTDATA(psta->pub->osh, n) + PKTLEN(psta->pub->osh, *p);

		if (IP_VER(ih) == IP_VER_6) {
			proto = ih[IPV6_NEXT_HDR_OFFSET];
			if (IPV6_EXTHDR(proto)) {
				hlen = ipv6_exthdr_len(ih, &proto);
				if (hlen < 0)
					return BCME_OK;
			}
			hlen += IPV6_MIN_HLEN;
		} else
			hlen = IPV4_HLEN(ih);

		icmph = ih + hlen;
		PKTFREE(psta->pub->osh, *p, TRUE);
		*p = n;
		*shost = PKTDATA(psta->pub->osh, n) + ETHER_SRC_OFFSET;
	}

	if (icmph[0] == ICMP6_RTR_SOLICITATION) {
		opt = (struct icmp6_opt *)&icmph[ICMP6_RTRSOL_OPT_OFFSET];
		if (opt->type == ICMP6_OPT_TYPE_SRC_LINK_LAYER)
			src_haddr = opt->data;
	} else if (icmph[0] == ICMP6_RTR_ADVERTISEMENT) {
		opt = (struct icmp6_opt *)&icmph[ICMP6_RTRADV_OPT_OFFSET];
		if (opt->type == ICMP6_OPT_TYPE_SRC_LINK_LAYER)
			src_haddr = opt->data;
	} else if (icmph[0] == ICMP6_NEIGH_SOLICITATION) {
		uint8 unspec[IPV6_ADDR_LEN] = { 0 };

		/* Option is not present when the src ip address is
		 * unspecified address.
		 */
		if (bcmp(unspec, ih + IPV6_SRC_IP_OFFSET, IPV6_ADDR_LEN) == 0)
			return BCME_OK;

		opt = (struct icmp6_opt *)&icmph[ICMP6_NEIGHSOL_OPT_OFFSET];
		if (opt->type == ICMP6_OPT_TYPE_SRC_LINK_LAYER)
			src_haddr = opt->data;
	} else if (icmph[0] == ICMP6_NEIGH_ADVERTISEMENT) {
		/* When responding to unicast neighbor soliciation this
		 * option may or may not be present.
		 */
		if ((icmplen - ICMP6_NEIGHADV_OPT_OFFSET) == 0)
			return BCME_OK;

		opt = (struct icmp6_opt *)&icmph[ICMP6_NEIGHADV_OPT_OFFSET];
		if (opt->type == ICMP6_OPT_TYPE_TGT_LINK_LAYER)
			src_haddr = opt->data;
	} else if (icmph[0] == ICMP6_REDIRECT) {
		opt = (struct icmp6_opt *)&icmph[ICMP6_REDIRECT_OPT_OFFSET];
		if (opt->type == ICMP6_OPT_TYPE_TGT_LINK_LAYER)
			src_haddr = opt->data;
	}

	if (tx) {
		if (src_haddr != NULL)
			bcopy(src_haddr, chaddr, ETHER_ADDR_LEN);
		else
			return BCME_OK;

		/* Modify the src link layer address only if it matches
		 * client's mac address.
		 */
		ASSERT(!PSTA_IS_ALIAS(psta, cfg, chaddr));
		PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, chaddr);
		if (wlc_psta_find(psta, chaddr) == NULL)
			return BCME_OK;

		WL_PSTA(("wl%d: Munge src mac in icmp6 opt %s\n", psta->pub->unit,
		         bcm_ether_ntoa((struct ether_addr *)src_haddr, eabuf)));

		csum_fixup_16(icmph + ICMP_CHKSUM_OFFSET,
		              src_haddr, ETHER_ADDR_LEN,
		              chaddr, ETHER_ADDR_LEN);

		PSTA_SET_ALIAS(psta, psta_cfg->psa, cfg, src_haddr);
	}

	return BCME_OK;
}

static void
wlc_psta_alias_create(wlc_psta_info_t *psta, wlc_bsscfg_t *pcfg, struct ether_addr *shost)
{
	uint32 m, b;
	uint8 oui[3];
#ifdef BCMDBG
	char alias_ea[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	ASSERT(pcfg != NULL);

	bcopy(&shost->octet[0], &oui[0], 3);
	ETHER_CLR_LOCALADDR(oui);

	/* If our oui is different from that of client then we can directly
	 * use the lower 24 bits from client mac addr.
	 */
	if (bcmp(&oui[0], &pcfg->cur_etheraddr.octet[0], 3)) {
		/* Use the oui from primary interface's hardware address */
		shost->octet[0] = pcfg->cur_etheraddr.octet[0];
		shost->octet[1] = pcfg->cur_etheraddr.octet[1];
		shost->octet[2] = pcfg->cur_etheraddr.octet[2];

		/* Set the locally administered bit */
		ETHER_SET_LOCALADDR(shost);

		WL_PSTA(("wl%d: Generated locally unique fixed alias %s\n",
		         psta->pub->unit, bcm_ether_ntoa(shost, alias_ea)));
		return;
	}

	WLCNTINCR(psta->pstadupdetect);

	/* Set the locally administered bit */
	ETHER_SET_LOCALADDR(shost);

	/* Right rotate the octets[1:3] of the mac address. This will make
	 * sure we generate an unique fixed alias for each mac address. If two
	 * client mac addresses have the same octets[1:3] then we will have
	 * a collision. If this happens then generate a random number for the
	 * mac address.
	 */
	m = shost->octet[1] << 16 | shost->octet[2] << 8 | shost->octet[3];

	b = m & 1;
	m >>= 1;
	m |= (b << 23);

	shost->octet[1] = m >> 16;
	shost->octet[2] = (m >> 8) & 0xff;
	shost->octet[3] = m & 0xff;

	/* Generate random value for octets[1:3] of mac address. Make sure
	 * there is no collision locally with already generated ones.
	 */
	while ((wlc_psta_client_is_ds_sta(psta, shost) != NULL) ||
	       (wlc_bsscfg_find_by_hwaddr(psta->wlc, shost) != NULL)) {
		/* We are making sure that our upstream and downstream
		 * instances don't use this address. If there is a match
		 * then try again.
		 */
		WLCNTINCR(psta->pstadupdetect);

		wlc_getrand(psta->wlc, &shost->octet[1], 3);
	}

	WL_PSTA(("wl%d: Generated an alias %s\n", psta->pub->unit,
	         bcm_ether_ntoa(shost, alias_ea)));

	return;
}

static int32 BCMFASTPATH
wlc_psta_ether_type(wlc_psta_info_t *psta, uint8 *eh, uint16 *et, uint16 *ip_off, bool *is_1x)
{
	uint16 ether_type, pull = ETHER_HDR_LEN;

	ether_type = NTOH16(*(uint16 *)(eh + ETHER_TYPE_OFFSET));

	/* LLC/SNAP frame */
	if ((ether_type <= ETHER_MAX_DATA) &&
	    (ether_type >= ETHER_HDR_LEN + SNAP_HDR_LEN + ETHER_TYPE_LEN)) {
		uint8 llc_snap_hdr[SNAP_HDR_LEN] = {0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00};

		/* Unknown llc-snap header */
		if (bcmp(llc_snap_hdr, eh + ETHER_HDR_LEN, SNAP_HDR_LEN)) {
			WL_PSTA(("wl%d: %s: unknown llc snap hdr\n",
			         psta->pub->unit, __FUNCTION__));
			return BCME_ERROR;
		}
		pull += (SNAP_HDR_LEN + ETHER_TYPE_LEN);
		ether_type = NTOH16(*(uint16 *)(eh + ETHER_HDR_LEN + SNAP_HDR_LEN));
	}

	/* Get the ether type from VLAN frame */
	if (ether_type == ETHER_TYPE_8021Q) {
		ether_type = NTOH16(*(uint16 *)(eh + ETHER_HDR_LEN));
		pull += VLAN_TAG_LEN;
	}

	if (ip_off != NULL)
		*ip_off = pull;

	/* 802.1x frames need not go thru' protocol processing here */
	if (is_1x != NULL) {
		*is_1x = ((ether_type == ETHER_TYPE_802_1X) ||
#ifdef BCMWAPI_WAI
		          (ether_type == ETHER_TYPE_WAI) ||
#endif /* BCMWAPI_WAI */
		          (ether_type == ETHER_TYPE_802_1X_PREAUTH));
	}

	if (et != NULL)
		*et = ether_type;

	return BCME_OK;
}

/*
 * Process the tx and rx frames based on the protocol type. ARP and DHCP packets are
 * processed by respective handlers.
 */
static int32 BCMFASTPATH
wlc_psta_proto_proc(wlc_psta_info_t *psta, wlc_bsscfg_t *pcfg, void **p, uint8 *eh,
                    wlc_bsscfg_t **cfg, uint8 **shost, bool tx)
{
	uint8 *ih;
	struct ether_addr *ea = NULL;
	uint16 ether_type, pull;
	int32 ea_off = -1;
	bool bcmc, fr_is_1x;
	psta_bsscfg_cubby_t *psta_cfg = NULL;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	/* Ignore unknown frames */
	if (wlc_psta_ether_type(psta, eh, &ether_type, &pull, &fr_is_1x) != BCME_OK)
		return BCME_OK;

	/* Send 1x frames as is. If primary is not authorized yet then wait for it
	 * before starting wpa handshake for secondary associations.
	 */
	if (fr_is_1x)
		return BCME_OK;

	ih = eh + pull;
	bcmc = ETHER_ISMULTI(eh + ETHER_DEST_OFFSET);

	if (tx) {
		/* Since we are going to modify the header below and the packet
		 * may be shared we allocate a header buffer and prepend it to
		 * the original sdu.
		 */
		if (bcmc) {
			void *n;

			if ((n = PKTGET(psta->pub->osh, TXOFF + pull, TRUE)) == NULL) {
				WL_ERROR(("wl%d: %s: PKTGET headroom %d failed\n",
				          psta->pub->unit, __FUNCTION__, TXOFF));
				WLCNTINCR(psta->pub->_cnt->txnobuf);
				return BCME_NOMEM;
			}
			PKTPULL(psta->pub->osh, n, TXOFF);

			wlc_pkttag_info_move(psta->pub, *p, n);
			PKTSETPRIO(n, PKTPRIO(*p));

			/* Copy ether header from data buffer to header buffer */
			memcpy(PKTDATA(psta->pub->osh, n),
			       PKTDATA(psta->pub->osh, *p), pull);
			PKTPULL(psta->pub->osh, *p, pull);

			/* Chain original sdu onto newly allocated header */
			PKTSETNEXT(psta->pub->osh, n, *p);

			eh = PKTDATA(psta->pub->osh, n);
			ih = PKTDATA(psta->pub->osh, *p);
			*p = n;
			*shost = eh + ETHER_SRC_OFFSET;
		}

		/* Find the proxy sta using client's original mac address */
		*cfg = wlc_psta_find_by_ds_ea(psta, *shost);
		if (*cfg == NULL) {
			wlc_psta_create(psta, psta->wlc,
			                (struct ether_addr *)*shost, pcfg);
			WLCNTINCR(psta->pub->_cnt->pstatxnoassoc);
			WL_PSTA(("wl%d: Creating psta for client %s\n",
			         psta->pub->unit,
			         bcm_ether_ntoa((struct ether_addr *)*shost, eabuf)));
			return BCME_NOTASSOCIATED;
		}

		/* Replace the src mac address in the ethernet header with
		 * newly created alias.
		 */
		psta_cfg = PSTA_BSSCFG_CUBBY(psta, *cfg);
		PSTA_SET_ALIAS(psta,  psta_cfg->psa, *cfg, *shost);
		ea = &((*cfg)->cur_etheraddr);
		ea_off = ETHER_SRC_OFFSET;

		WL_PSTA(("wl%d.%d: Translate addr to %s\n", psta->pub->unit,
		         WLC_BSSCFG_IDX(*cfg),
		         bcm_ether_ntoa(&((*cfg)->cur_etheraddr), eabuf)));
	} else {
		/* Restore proxy client address in the ether header */
		if (!bcmc) {
			ASSERT(*cfg != NULL);
#ifdef BCMDBG
			/* If local admin bit is not set then ignore the frame,
			 * do not proxy such address.
			 */
			if (!PSTA_IS_ALIAS(psta, *cfg, eh)) {
				WL_INFORM(("wl%d: Not proxying frame\n",
				           psta->pub->unit));
				return BCME_NOTASSOCIATED;
			}
#endif /* BCMDBG */
			psta_cfg = PSTA_BSSCFG_CUBBY(psta, *cfg);
			PSTA_CLR_ALIAS(psta, psta_cfg->psa, *cfg, eh + ETHER_DEST_OFFSET);
			ea = &psta_cfg->psa->ds_ea;
			ea_off = ETHER_DEST_OFFSET;

			WL_PSTA(("wl%d.%d: Restore addr to %s\n", psta->pub->unit,
			         WLC_BSSCFG_IDX(*cfg),
			         bcm_ether_ntoa(&psta_cfg->psa->ds_ea, eabuf)));
		}
	}

	switch (ether_type) {
	case ETHER_TYPE_IP:
	case ETHER_TYPE_IPV6:
		break;
	case ETHER_TYPE_ARP:
		return wlc_psta_arp_proc(psta, *cfg, p, ih, shost, tx, bcmc);
	default:
		WL_PSTA(("wl%d: Unhandled ether type 0x%x\n",
		         psta->pub->unit, ether_type));
		return BCME_OK;
	}

	if (IP_VER(ih) == IP_VER_4) {
		if (IPV4_PROT(ih) == IP_PROT_UDP)
			return wlc_psta_udp_proc(psta, *cfg, p, ih, ih + IPV4_HLEN(ih),
			                         shost, tx, bcmc);
	} else if (IP_VER(ih) == IP_VER_6) {
		uint8 proto = ih[IPV6_NEXT_HDR_OFFSET];
		int32 exthlen = 0;

		if (IPV6_EXTHDR(proto)) {
			exthlen = ipv6_exthdr_len(ih, &proto);
			if (exthlen < 0)
				return BCME_OK;
		}

		WL_PSTA(("wl%d: IP exthlen %d proto %d\n", psta->pub->unit,
		         exthlen, proto));

		if (proto == IP_PROT_UDP)
			return wlc_psta_udp_proc(psta, *cfg, p, ih,
			                         ih + IPV6_MIN_HLEN + exthlen,
			                         shost, tx, bcmc);
		else if (proto == IP_PROT_ICMP6)
			return wlc_psta_icmp6_proc(psta, *cfg, p, ih,
			                           ih + IPV6_MIN_HLEN + exthlen,
			                           IPV6_PAYLOAD_LEN(ih) - exthlen,
			                           shost, tx, bcmc);
	}

#ifdef PKTC
	if (PKTISCHAINED(*p)) {
		void *t;
		int32 ccnt;

		ASSERT((ea != NULL) && (ea_off != -1));
		for (t = PKTCLINK(*p); t != NULL; t = PKTCLINK(t)) {
			eh = PKTDATA(psta->pub->osh, t);
			eacopy(ea, eh + ea_off);
		}
		ASSERT(psta_cfg != NULL);
		ccnt = PKTCCNT(*p) - 1;
		if (tx) {
			WLCNTADD(psta->pub->_cnt->pstatxucast, ccnt);
			WLCNTADD(psta_cfg->psa->txucast, ccnt);
		} else {
			WLCNTADD(psta_cfg->psa->rxucast, ccnt);
			WLCNTADD(psta->pub->_cnt->pstarxucast, ccnt);
		}
	}
#endif /* PSTA */

	return BCME_OK;
}

/* Proxy assoc lookup */
wlc_bsscfg_t * BCMFASTPATH
wlc_psta_find(wlc_psta_info_t *psta, uint8 *mac)
{
	uint32 idx;
	wlc_bsscfg_t *cfg;

	FOREACH_UP_PSTA(psta->wlc, idx, cfg) {
		if (EA_CMP(mac, cfg->cur_etheraddr.octet))
			return cfg;
	}

	return NULL;
}

/* Proxy assoc lookup based on the ds client address */
wlc_bsscfg_t * BCMFASTPATH
wlc_psta_find_by_ds_ea(wlc_psta_info_t *psta, uint8 *mac)
{
	uint32 idx;
	wlc_bsscfg_t *cfg;
	psta_bsscfg_cubby_t *psta_cfg;

	FOREACH_PSTA(psta->wlc, idx, cfg) {
		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
		if (EA_CMP(mac, psta_cfg->psa->ds_ea.octet))
			return cfg;
	}

	return NULL;
}

/* Downstream client lookup */
static struct scb *
wlc_psta_client_is_ds_sta(wlc_psta_info_t *psta, struct ether_addr *mac)
{
	uint32 idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	FOREACH_UP_AP(psta->wlc, idx, cfg) {
		scb = wlc_scbfind(psta->wlc, cfg, mac);
		if (scb != NULL)
			break;
	}

	return scb;
}

#ifdef DPSTA
/* See if the downstream client is associated and authorized */
static bool
wlc_psta_is_ds_sta(wlc_psta_info_t *psta, struct ether_addr *mac)
{
	struct scb *scb;
	wlc_bsscfg_t *cfg;

	scb = wlc_psta_client_is_ds_sta(psta, mac);
	if (scb == NULL)
		return FALSE;

	cfg = SCB_BSSCFG(scb);
	ASSERT(cfg != NULL);

	/* AP is down */
	if (!wlc_bss_connected(cfg))
		return FALSE;

	return (cfg->WPA_auth != WPA_AUTH_DISABLED &&
	        WSEC_ENABLED(cfg->wsec)) ? SCB_AUTHORIZED(scb) : SCB_ASSOCIATED(scb);
}

/* Check if bss is authorized */
static bool
wlc_psta_authorized(wlc_bsscfg_t *cfg)
{
	return wlc_bss_connected(cfg) && WLC_PORTOPEN(cfg);
}
#endif /* DPSTA */

/*
 * Process frames in transmit direction by replacing source MAC with
 * the locally generated alias address.
 */
int32 BCMFASTPATH
wlc_psta_send_proc(wlc_psta_info_t *psta, void **p, wlc_bsscfg_t **cfg)
{
	uint8 *eh = PKTDATA(psta->pub->osh, *p);
	struct ether_addr *shost;
	wlc_bsscfg_t *pcfg = *cfg;
	psta_bsscfg_cubby_t *psta_cfg;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	ASSERT(pcfg != NULL);

	if (PKTLEN(psta->pub->osh, *p) < ETHER_HDR_LEN) {
		WL_ERROR(("wl%d: %s: unable to process short frame\n",
		          psta->pub->unit, __FUNCTION__));
		return BCME_BADLEN;
	}

	shost = (struct ether_addr *)(eh + ETHER_SRC_OFFSET);

	WL_PSTA(("wl%d: Try to transmit frame w/ ether type 0x%04x from client %s\n",
	         psta->pub->unit, NTOH16(*(uint16 *)(eh + ETHER_TYPE_OFFSET)),
	         bcm_ether_ntoa(shost, eabuf)));

	/* Make sure the primary is connected before sending data or
	 * before initiating secondary proxy assocs.
	 */
	psta_cfg = PSTA_BSSCFG_CUBBY(psta, pcfg);
	if (!wlc_bss_connected(pcfg)) {
		WL_PSTA(("wl%d: Primary not connected (%d, %d)\n",
		         psta->pub->unit, pcfg->associated, pcfg->wsec_portopen));
		WLCNTINCR(psta->pub->_cnt->pstatxnoassoc);
		WLCNTINCR(psta_cfg->psa->txnoassoc);
		return BCME_NOTASSOCIATED;
	}

	/* Frame belongs to primary bss */
	if (EA_CMP(shost, &pcfg->cur_etheraddr)) {
		WL_PSTA(("wl%d: Sending frame with %s on primary\n",
		         psta->pub->unit, bcm_ether_ntoa(shost, eabuf)));
		return BCME_OK;
	}

	WL_PSTA(("wl%d: See if %s is one of the proxy clients\n", psta->pub->unit,
	         bcm_ether_ntoa(shost, eabuf)));

	/* Wait for primary to authorize before secondary assoc attempts
	 * wpa handshake.
	 */
	if (!WLC_PORTOPEN(pcfg)) {
		WL_PSTA(("wl%d: Primary not authorized\n", psta->pub->unit));
		return BCME_NOTASSOCIATED;
	}

	if (PSTA_IS_REPEATER(psta->wlc)) {
		int32 err;

		/* Modify the source mac address in proto headers */
		err = wlc_psta_proto_proc(psta, pcfg, p, eh, cfg, (uint8 **)&shost, TRUE);
		if (err != BCME_OK)
			return err;
	} else {
		/* See if a secondary assoc exists to send the frame */
		*cfg = wlc_psta_find(psta, (uint8 *)shost);

		if (*cfg == NULL) {
			WLCNTINCR(psta->pub->_cnt->pstatxnoassoc);
			WL_PSTA(("wl%d: Creating psta for client %s\n",
			         psta->pub->unit, bcm_ether_ntoa(shost, eabuf)));
			return wlc_psta_create(psta, psta->wlc, shost, pcfg);
		}
	}

	/* Send the frame on to secondary proxy bss if allowed */
	psta_cfg = PSTA_BSSCFG_CUBBY(psta, *cfg);
	if (wlc_bss_connected(*cfg)) {
		/* Clear inactivity counter */
		psta_cfg->psa->inactivity = 0;

		if (!ETHER_ISMULTI(eh + ETHER_DEST_OFFSET)) {
			WLCNTINCR(psta->pub->_cnt->pstatxucast);
			WLCNTINCR(psta_cfg->psa->txucast);
		} else {
			WLCNTINCR(psta_cfg->psa->txbcmc);
			WLCNTINCR(psta->pub->_cnt->pstatxbcmc);
		}

		WL_PSTA(("wl%d: Tx for PSTA %s\n", psta->pub->unit,
		         bcm_ether_ntoa(shost, eabuf)));

		return BCME_OK;
	}

	WL_PSTA(("wl%d: PSTA not connected, associated %d portopen %d BSSID %s\n",
	         psta->pub->unit, (*cfg)->associated, WLC_PORTOPEN(*cfg),
	         bcm_ether_ntoa(&((*cfg)->BSSID), eabuf)));

	if ((!(*cfg)->enable) || ((*cfg)->assoc->state == AS_IDLE))
		wlc_psta_create(psta, psta->wlc, shost, pcfg);

	WLCNTINCR(psta->pub->_cnt->pstatxnoassoc);
	WLCNTINCR(psta_cfg->psa->txnoassoc);

	/* Before sending data wait for secondary proxy assoc to
	 * complete.
	 */
	return BCME_NOTASSOCIATED;
}

/*
 * Process frames in receive direction. Restore the original mac and mux
 * the frames on to primary.
 */
void BCMFASTPATH
wlc_psta_recv_proc(wlc_psta_info_t *psta, void *p, struct ether_header *eh,
                   wlc_bsscfg_t **bsscfg, wl_if_t **wlif)
{
	psta_bsscfg_cubby_t *psta_cfg;
	wlc_bsscfg_t *cfg = *bsscfg;
#ifdef BCMDBG
	char s_eabuf[ETHER_ADDR_STR_LEN];
	char d_eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	ASSERT(ETHER_ISMULTI(eh->ether_dhost) || BSSCFG_PSTA(cfg));

	WL_PSTA(("wl%d.%d: Frame of ether type %04x received for %s with src %s\n",
	         psta->pub->unit, WLC_BSSCFG_IDX(cfg), NTOH16(eh->ether_type),
	         bcm_ether_ntoa((struct ether_addr *)eh->ether_dhost, s_eabuf),
	         bcm_ether_ntoa((struct ether_addr *)eh->ether_shost, d_eabuf)));

	/* Restore original mac address for psta repeater assocs */
	if (PSTA_IS_REPEATER(psta->wlc)) {
		int32 err;
		/* Do the rx processing needed to restore the ds client
		 * address in the payload.
		 */
		err = wlc_psta_proto_proc(psta, NULL, &p, (uint8 *)eh,
		                          &cfg, NULL, FALSE);
		if (err != BCME_OK)
			return;
	}

	if (!ETHER_ISMULTI(eh->ether_dhost)) {
		/* Update the rx counters */
		ASSERT(wlc_bss_connected(cfg));

		if (PSTA_IS_PRIMARY(cfg))
			return;

		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);

		WLCNTINCR(psta_cfg->psa->rxucast);
		WLCNTINCR(psta->pub->_cnt->pstarxucast);

		/* Clear inactivity counter */
		psta_cfg->psa->inactivity = 0;

		/* Send up the frames on primary */
		if ((eh->ether_type != HTON16(ETHER_TYPE_802_1X)) &&
#ifdef BCMWAPI_WAI
		    (eh->ether_type != HTON16(ETHER_TYPE_WAI)) &&
#endif /* BCMWAPI_WAI */
		    (eh->ether_type != HTON16(ETHER_TYPE_802_1X_PREAUTH))) {
			*wlif = psta_cfg->psa->pwlif;
			*bsscfg = psta_cfg->psa->pcfg;
			ASSERT(*wlif == psta_cfg->psa->pcfg->wlcif->wlif);
		}
	} else {
		WLCNTINCR(psta->pub->_cnt->pstarxbcmc);

		/* Broadcast Multicast frames go up on primary */
		*wlif = cfg->wlcif->wlif;
	}

	WL_PSTA(("wl%d.%d: Sending up the frame to wlif 0x%p\n",
	         psta->pub->unit, WLC_BSSCFG_IDX(cfg), *wlif));

	return;
}

/* Disassociate and cleanup all the proxy client's state. This is
 * called for instance when primary roams.
 */
void
wlc_psta_disassoc_all(wlc_psta_info_t *psta)
{
	int32 idx;
	wlc_bsscfg_t *cfg;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	/* Cleanup all active proxy stas */
	FOREACH_PSTA(psta->wlc, idx, cfg) {
		WL_PSTA(("wl%d: %s: Disassoc for PSTA %s\n", psta->pub->unit,
		         __FUNCTION__, bcm_ether_ntoa(&cfg->cur_etheraddr, eabuf)));

		/* Cleanup the proxy client state */
		wlc_disassociate_client(cfg, FALSE, NULL, NULL);
	}

	return;
}

#ifdef BCMDBG
int32
wlc_psta_dump(wlc_psta_info_t *psta, struct bcmstrbuf *b)
{
	int32 idx;
	wlc_bsscfg_t *cfg;
	psta_bsscfg_cubby_t *psta_cfg;
	wlc_psa_t *psa;
	char eabuf[ETHER_ADDR_STR_LEN], ds_eabuf[ETHER_ADDR_STR_LEN];

	bcm_bprintf(b, "psta mode: %s\n", PSTA_IS_PROXY(psta->wlc) ? "proxy" :
	            PSTA_IS_REPEATER(psta->wlc) ? "repeater" : "disabled");

	/* Dump the global counters */
	bcm_bprintf(b, "pstatxucast %d pstatxnoassoc %d pstatxbcmc %d\n",
	            WLCNTVAL(psta->pub->_cnt->pstatxucast),
	            WLCNTVAL(psta->pub->_cnt->pstatxnoassoc),
	            WLCNTVAL(psta->pub->_cnt->pstatxbcmc));
	bcm_bprintf(b, "pstarxucast %d pstarxbcmc %d\n",
	            WLCNTVAL(psta->pub->_cnt->pstarxucast),
	            WLCNTVAL(psta->pub->_cnt->pstarxbcmc));
	bcm_bprintf(b, "pstatxdhcpc %d pstarxdhcpc %d pstatxdhcps %d pstarxdhcps %d\n",
	            WLCNTVAL(psta->pstatxdhcpc), WLCNTVAL(psta->pstarxdhcpc),
	            WLCNTVAL(psta->pstatxdhcps), WLCNTVAL(psta->pstarxdhcps));
	bcm_bprintf(b, "pstatxdhcpc6 %d pstarxdhcpc6 %d pstatxdhcps6 %d pstarxdhcps6 %d\n",
	            WLCNTVAL(psta->pstatxdhcpc6), WLCNTVAL(psta->pstarxdhcpc6),
	            WLCNTVAL(psta->pstatxdhcps6), WLCNTVAL(psta->pstarxdhcps6));
	bcm_bprintf(b, "pstadupdetect %d\n", WLCNTVAL(psta->pstadupdetect));

	bcm_bprintf(b, "  MAC\t\t\tAlias\t\t\tRCMTA\tAS\tTxBCMC\t\tTxUcast\t\t"
	            "RxUcast\t\tTxNoAssoc\n");

	/* Dump the proxy links */
	FOREACH_PSTA(psta->wlc, idx, cfg) {
		psta_cfg = PSTA_BSSCFG_CUBBY(psta, cfg);
		psa = psta_cfg->psa;
		bcm_bprintf(b, "%c %s\t%s\t%d\t%d\t%d\t\t%d\t\t%d\t\t%d\n",
		            cfg->up ? '*' : '-',
		            bcm_ether_ntoa(&psa->ds_ea, ds_eabuf),
		            bcm_ether_ntoa(&cfg->cur_etheraddr, eabuf),
		            psa->rcmta_idx, cfg->assoc->state,
		            WLCNTVAL(psa->txbcmc), WLCNTVAL(psa->txucast),
		            WLCNTVAL(psa->rxucast), WLCNTVAL(psa->txnoassoc));
	}

	return 0;
}
#endif	/* BCMDBG */
