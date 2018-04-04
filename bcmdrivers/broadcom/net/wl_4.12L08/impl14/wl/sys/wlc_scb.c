/*
 * Common interface to the 802.11 Station Control Block (scb) structure
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_scb.c 370836 2012-11-23 23:19:04Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmendian.h>
#include <proto/802.11.h>
#include <proto/wpa.h>
#include <sbconfig.h>
#include <pcicfg.h>
#include <bcmsrom.h>
#include <wlioctl.h>
#include <epivers.h>
#include <bcmwpa.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_phy_hal.h>
#include <wlc_antsel.h>
#include <wl_export.h>
#include <wlc_ap.h>
#include <wlc_scb_ratesel.h>
#include <wlc_assoc.h>
#ifdef PROP_TXSTATUS
#include <wlfc_proto.h>
#include <wl_wlfc.h>
#include <wlc_apps.h>
#endif
#ifdef TRAFFIC_MGMT
#include <wlc_traffic_mgmt.h>
#endif
#ifdef WLWNM
#include <wlc_wnm.h>
#endif
#ifdef WL_RELMCAST
#include "wlc_relmcast.h"
#endif

#define SCB_MAX_CUBBY		(pub->tunables->maxscbcubbies)
#define SCB_MAGIC 0x0505a5a5

#define INTERNAL_SCB		0x00000001
#define USER_SCB		0x00000002

#define	SCBHASHINDEX(hash, id)	((id[3] ^ id[4] ^ id[5]) % (hash))

#ifdef SCBFREELIST
#ifdef INT_SCB_OPT
#error "SCBFREELIST incompatible with INT_SCB_OPT"
/* To make it compatible, freelist needs to track internal vs external */
#endif /* INT_SCB_OPT */
#endif /* SCBFREELIST */
/* structure for storing per-cubby client info */
typedef struct cubby_info {
	scb_cubby_init_t	fn_init;	/* fn called during scb malloc */
	scb_cubby_deinit_t	fn_deinit;	/* fn called during scb free */
	scb_cubby_dump_t 	fn_dump;	/* fn called during scb dump */
	void			*context;	/* context to be passed to all cb fns */
} cubby_info_t;

/* structure for storing public and private global scb module state */
struct scb_module {
	wlc_info_t	*wlc;			/* global wlc info handle */
	wlc_pub_t	*pub;			/* public part of wlc */
	uint16		nscb;			/* total number of allocated scbs */
	uint		scbtotsize;		/* total scb size including container */
	uint 		ncubby;			/* current num of cubbies */
	cubby_info_t	*cubby_info;		/* cubby client info */
#ifdef SCBFREELIST
	struct scb      *free_list;		/* Free list of SCBs */
#endif
	int		cfgh;			/* scb bsscfg cubby handle */
};

/* station control block - one per remote MAC address */
struct scb_info {
	struct scb 	*scbpub;	/* public portion of scb */
	struct scb_info *hashnext;	/* pointer to next scb under same hash entry */
	struct scb_info	*next;		/* pointer to next allocated scb */
	struct wlcband	*band;		/* pointer to our associated band */
#ifdef MACOSX
	struct scb_info *hashnext_copy;
	struct scb_info *next_copy;
#endif
};

/* Helper macro for txpath in scb */
/* A feature in Tx path goes through following states:
 * Unregisterd -> Registered [Global state]
 * Registerd -> Configured -> Active -> Configured [Per-scb state]
 */

/* Set the next feature of given feature */
#define SCB_TXMOD_SET(scb, fid, _next_fid) { \
	scb->tx_path[fid].next_tx_fn = wlc->txmod_fns[_next_fid].tx_fn; \
	scb->tx_path[fid].next_handle = wlc->txmod_fns[_next_fid].ctx; \
	scb->tx_path[fid].next_fid = _next_fid; \
}
static void wlc_scb_hash_add(wlc_info_t *wlc, struct scb *scb, int bandunit,
	wlc_bsscfg_t *bsscfg);
static void wlc_scb_hash_del(wlc_info_t *wlc, struct scb *scbd, int bandunit,
	wlc_bsscfg_t *bsscfg);
static void wlc_scb_list_add(wlc_info_t *wlc, struct scb_info *scbinfo,
	wlc_bsscfg_t *bsscfg);
static void wlc_scb_list_del(wlc_info_t *wlc, struct scb *scbd,
	wlc_bsscfg_t *bsscfg);

static struct scb *wlc_scbvictim(wlc_info_t *wlc);
static struct scb *wlc_scb_getnext(struct scb *scb);
static struct wlc_bsscfg *wlc_scb_next_bss(scb_module_t *scbstate, int idx);
static int wlc_scbinit(wlc_info_t *wlc, struct wlcband *band, struct scb_info *scbinfo,
	uint32 scbflags);
static void wlc_scb_reset(scb_module_t *scbstate, struct scb_info *scbinfo);
static struct scb_info *wlc_scb_allocmem(scb_module_t *scbstate);
static void wlc_scb_freemem(scb_module_t *scbstate, struct scb_info *scbinfo);

#if defined(BCMDBG)
static int wlc_dump_scb(wlc_info_t *wlc, struct bcmstrbuf *b);
/* Dump the active txpath for the current SCB */
static int wlc_scb_txpath_dump(wlc_info_t *wlc, struct scb *scb, struct bcmstrbuf *b);
/* SCB Flags Names Initialization */
static const bcm_bit_desc_t scb_flags[] =
{
	{SCB_NONERP, "NonERP"},
	{SCB_LONGSLOT, "LgSlot"},
	{SCB_SHORTPREAMBLE, "ShPre"},
	{SCB_8021XHDR, "1X"},
	{SCB_WPA_SUP, "WPASup"},
	{SCB_DEAUTH, "DeA"},
	{SCB_WMECAP, "WME"},
	{SCB_BRCM, "BRCM"},
	{SCB_WDS_LINKUP, "WDSLinkUP"},
	{SCB_LEGACY_AES, "LegacyAES"},
	{SCB_MYAP, "MyAP"},
	{SCB_PENDING_PROBE, "PendingProbe"},
	{SCB_AMSDUCAP, "AMSDUCAP"},
	{SCB_USEME, "XXX"},
	{SCB_HTCAP, "HT"},
	{SCB_RECV_PM, "RECV_PM"},
	{SCB_AMPDUCAP, "AMPDUCAP"},
	{SCB_IS40, "40MHz"},
	{SCB_NONGF, "NONGFCAP"},
	{SCB_APSDCAP, "APSDCAP"},
	{SCB_PENDING_FREE, "PendingFree"},
	{SCB_PENDING_PSPOLL, "PendingPSPoll"},
	{SCB_RIFSCAP, "RIFSCAP"},
	{SCB_HT40INTOLERANT, "40INTOL"},
	{SCB_WMEPS, "WMEPSOK"},
	{SCB_COEX_MGMT, "OBSSCoex"},
	{SCB_IBSS_PEER, "IBSS Peer"},
	{SCB_STBCCAP, "STBC"},
	{0, NULL}
};
static const bcm_bit_desc_t scb_flags2[] =
{
	{SCB2_SGI20_CAP, "SGI20"},
	{SCB2_SGI40_CAP, "SGI40"},
	{SCB2_RX_LARGE_AGG, "LGAGG"},
#ifdef BCMWAPI_WAI
	{SCB2_WAIHDR, "WAI"},
#endif /* BCMWAPI_WAI */
	{SCB2_LDPCCAP, "LDPC"},
	{SCB2_VHTCAP, "VHT"},
	{SCB2_AMSDU_IN_AMPDU_CAP, "AGG^2"},
	{SCB2_P2P, "P2P"},
	{SCB2_DWDS, "DWDS"},
	{0, NULL}
};
static const bcm_bit_desc_t scb_flags3[] =
{
	{SCB3_A4_DATA, "A4_DATA"},
	{SCB3_A4_NULLDATA, "A4_NULLDATA"},
	{SCB3_A4_8021X, "A4_8021X"},
	{0, NULL}
};
static const bcm_bit_desc_t scb_states[] =
{
	{AUTHENTICATED, "AUTH"},
	{ASSOCIATED, "ASSOC"},
	{PENDING_AUTH, "AUTH_PEND"},
	{PENDING_ASSOC, "ASSOC_PEND"},
	{AUTHORIZED, "AUTH_8021X"},
	{TAKEN4IBSS, "IBSS"},
	{0, NULL}
};
static const bcm_bit_desc_t scb_vht_flags[] =
{
	{SCB_VHT_LDPCCAP, "LDPC"},
	{SCB_SGI80, "SGI80"},
	{SCB_SGI160, "SGI160"},
	{SCB_VHT_TX_STBCCAP, "TX_STBC"},
	{SCB_VHT_RX_STBCCAP, "RX_STBC"},
	{SCB_SU_BEAMFORMER, "SU_BEAMFORMER"},
	{SCB_SU_BEAMFORMEE, "SU_BEANFORMEE"},
	{SCB_MU_BEAMFORMER, "MU_BEAMFORMER"},
	{SCB_MU_BEAMFORMEE, "MU_BEANFORMEE"},
	{SCB_VHT_TXOP_PS, "TXOP_PS"},
	{SCB_HTC_VHT_CAP, "HTC_VHT"},
	{0, NULL}
};


#endif 

#if defined(PKTC) || defined(PKTC_DONGLE)
static void wlc_scb_pktc_enable(struct scb *scb);
static void wlc_scb_pktc_disable(struct scb *scb);
#endif

#ifdef SCBFREELIST
static void wlc_scbfreelist_free(scb_module_t *scbstate);
#if defined(BCMDBG)
static void wlc_scbfreelist_dump(scb_module_t *scbstate, struct bcmstrbuf *b);
#endif 
#endif /* SCBFREELIST */

#define SCBINFO(_scb) (_scb ? (struct scb_info *)((_scb)->scb_priv) : NULL)

#ifdef MACOSX

#ifdef panic
#undef panic
void panic(const char *str, ...);
#endif

#define SCBSANITYCHECK(_scb)  { \
		if (((_scb) != NULL) &&				\
		    ((((_scb))->magic != SCB_MAGIC) ||	\
		     (SCBINFO(_scb)->hashnext != SCBINFO(_scb)->hashnext_copy) || \
		     (SCBINFO(_scb)->next != SCBINFO(_scb)->next_copy)))	\
			panic("scbinfo corrupted: magic: 0x%x hn: %p hnc: %p n: %p nc: %p\n", \
			      ((_scb))->magic, SCBINFO(_scb)->hashnext, \
			      SCBINFO(_scb)->hashnext_copy,		\
			      SCBINFO(_scb)->next, SCBINFO(_scb)->next_copy);	\
	}

#define SCBFREESANITYCHECK(_scb)  { \
		if (((_scb) != NULL) &&				\
		    ((((_scb))->magic != ~SCB_MAGIC) || \
		     (SCBINFO(_scb)->next != SCBINFO(_scb)->next_copy)))	\
			panic("scbinfo corrupted: magic: 0x%x hn: %p hnc: %p n: %p nc: %p\n", \
			      ((_scb))->magic, SCBINFO(_scb)->hashnext, \
			      SCBINFO(_scb)->hashnext_copy,		\
			      SCBINFO(_scb)->next, SCBINFO(_scb)->next_copy);	\
	}

#else

#define SCBSANITYCHECK(_scbinfo)	do {} while (0)
#define SCBFREESANITYCHECK(_scbinfo)	do {} while (0)

#endif /* MACOSX */

/* bsscfg cubby */
typedef struct scb_bsscfg_cubby {
	struct scb	**scbhash[MAXBANDS];	/* scb hash table */
	uint8		nscbhash;		/* scb hash table size */
	struct scb	*scb;			/* station control block link list */
} scb_bsscfg_cubby_t;

#define SCB_BSSCFG_CUBBY(ss, cfg) ((scb_bsscfg_cubby_t *)BSSCFG_CUBBY(cfg, (ss)->cfgh))

static int wlc_scb_bsscfg_init(void *context, wlc_bsscfg_t *cfg);
static void wlc_scb_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg);
#if defined(BCMDBG)
static void wlc_scb_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b);
#else
#define wlc_scb_bsscfg_dump NULL
#endif

static int
wlc_scb_bsscfg_init(void *context, wlc_bsscfg_t *cfg)
{
	scb_module_t *scbstate = (scb_module_t *)context;
	scb_bsscfg_cubby_t *scb_cfg = SCB_BSSCFG_CUBBY(scbstate, cfg);
	uint8 nscbhash, *scbhash;
	wlc_pub_t *pub = scbstate->pub;
	uint32 i, len;

	nscbhash = ((pub->tunables->maxscb + 7)/8); /* # scb hash buckets */

	len = (sizeof(struct scb *) * MAXBANDS * nscbhash);
	scbhash = MALLOC(pub->osh, len);
	if (scbhash == NULL)
		return BCME_NOMEM;

	bzero((char *)scbhash, len);

	scb_cfg->nscbhash = nscbhash;
	for (i = 0; i < MAXBANDS; i++) {
		scb_cfg->scbhash[i] = (struct scb **)((uintptr)scbhash +
		                      (i * scb_cfg->nscbhash * sizeof(struct scb *)));
	}

	return BCME_OK;
}

static void
wlc_scb_bsscfg_deinit(void *context, wlc_bsscfg_t *cfg)
{
	scb_module_t *scbstate = (scb_module_t *)context;
	scb_bsscfg_cubby_t *scb_cfg = SCB_BSSCFG_CUBBY(scbstate, cfg);
	uint32 len;

	/* clear all scbs */
	wlc_scb_bsscfg_scbclear(cfg->wlc, cfg, TRUE);

	len = (sizeof(struct scb *) * MAXBANDS * scb_cfg->nscbhash);
	MFREE(scbstate->pub->osh, scb_cfg->scbhash[0], len);
}

scb_module_t *
BCMATTACHFN(wlc_scb_attach)(wlc_info_t *wlc)
{
	scb_module_t *scbstate;
	int len;
	wlc_pub_t *pub = wlc->pub;

	len = sizeof(scb_module_t) + (sizeof(cubby_info_t) * SCB_MAX_CUBBY);
	if ((scbstate = MALLOC(pub->osh, len)) == NULL)
		return NULL;
	bzero((char *)scbstate, len);

	scbstate->wlc = wlc;
	scbstate->pub = pub;

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((scbstate->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(scb_bsscfg_cubby_t),
		wlc_scb_bsscfg_init, wlc_scb_bsscfg_deinit, wlc_scb_bsscfg_dump,
		(void *)scbstate)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve failed\n",
			wlc->pub->unit, __FUNCTION__));
		MFREE(pub->osh, scbstate, len);
		return NULL;
	}

	scbstate->cubby_info = (cubby_info_t *)((uintptr)scbstate + sizeof(scb_module_t));

	scbstate->scbtotsize = sizeof(struct scb);
	scbstate->scbtotsize += sizeof(int) * MA_WINDOW_SZ; /* sizeof rssi_window */
	scbstate->scbtotsize += sizeof(struct tx_path_node) * TXMOD_LAST;

#if defined(BCMDBG)
	wlc_dump_register(pub, "scb", (dump_fn_t)wlc_dump_scb, (void *)wlc);
#endif

	return scbstate;
}

void
BCMATTACHFN(wlc_scb_detach)(scb_module_t *scbstate)
{
	wlc_pub_t *pub;
	int len;

	if (!scbstate)
		return;

	pub = scbstate->pub;

#ifdef SCBFREELIST
	wlc_scbfreelist_free(scbstate);
#endif

	ASSERT(scbstate->nscb == 0);

	len = sizeof(scb_module_t) + (sizeof(cubby_info_t) * SCB_MAX_CUBBY);
	MFREE(scbstate->pub->osh, scbstate, len);
}

/* Methods for iterating along a list of scb */

/* Direct access to the next */
static struct scb *
wlc_scb_getnext(struct scb *scb)
{
	if (scb) {
		SCBSANITYCHECK(scb);
		return (SCBINFO(scb)->next ? SCBINFO(scb)->next->scbpub : NULL);
	}
	return NULL;
}
static struct wlc_bsscfg *
wlc_scb_next_bss(scb_module_t *scbstate, int idx)
{
	wlc_bsscfg_t	*next_bss = NULL;

	/* get next bss walking over hole */
	while (idx < WLC_MAXBSSCFG) {
		next_bss = WLC_BSSCFG(scbstate->wlc, idx);
		if (next_bss != NULL)
			break;
		idx++;
	}
	return next_bss;
}

/* Initialize an iterator keeping memory of the next scb as it moves along the list */
void
wlc_scb_iterinit(scb_module_t *scbstate, struct scb_iter *scbiter, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;
	ASSERT(scbiter != NULL);

	if (bsscfg == NULL) {
		/* walk scbs of all bss */
		scbiter->all = TRUE;
		scbiter->next_bss = wlc_scb_next_bss(scbstate, 0);
		if (scbiter->next_bss == NULL) {
			/* init next scb pointer also to null */
			scbiter->next = NULL;
			return;
		}
	} else {
		/* walk scbs of specified bss */
		scbiter->all = FALSE;
		scbiter->next_bss = bsscfg;
	}

	ASSERT(scbiter->next_bss != NULL);
	scb_cfg = SCB_BSSCFG_CUBBY(scbstate, scbiter->next_bss);
	SCBSANITYCHECK(scb_cfg->scb);

	/* Prefetch next scb, so caller can free an scb before going on to the next */
	scbiter->next = scb_cfg->scb;
}

/* move the iterator */
struct scb *
wlc_scb_iternext(scb_module_t *scbstate, struct scb_iter *scbiter)
{
	scb_bsscfg_cubby_t *scb_cfg;
	struct scb *scb;

	ASSERT(scbiter != NULL);

	while (scbiter->next_bss) {

		/* get the next scb in the current bsscfg */
		if ((scb = scbiter->next) != NULL) {
			/* get next scb of bss */
			SCBSANITYCHECK(scb);
			scbiter->next = (SCBINFO(scb)->next ? SCBINFO(scb)->next->scbpub : NULL);
			return scb;
		}

		/* get the next bsscfg if we have run out of scbs in the current bsscfg */
		if (scbiter->all) {
			scbiter->next_bss =
			        wlc_scb_next_bss(scbstate, WLC_BSSCFG_IDX(scbiter->next_bss) + 1);
			if (scbiter->next_bss != NULL) {
				scb_cfg = SCB_BSSCFG_CUBBY(scbstate, scbiter->next_bss);
				scbiter->next = scb_cfg->scb;
			}
		} else {
			scbiter->next_bss = NULL;
		}
	}

	/* done with all bsscfgs and scbs */
	scbiter->next = NULL;

	return NULL;
}
/*
 * Accessors, nagative values are errors.
 */

int
BCMATTACHFN(wlc_scb_cubby_reserve)(wlc_info_t *wlc, uint size, scb_cubby_init_t fn_init,
	scb_cubby_deinit_t fn_deinit, scb_cubby_dump_t fn_dump, void *context)
{
	uint offset;
	scb_module_t *scbstate = wlc->scbstate;
	cubby_info_t *cubby_info;
	wlc_pub_t *pub = wlc->pub;

	ASSERT(scbstate->nscb == 0);
	ASSERT((scbstate->scbtotsize % PTRSZ) == 0);

	if (scbstate->ncubby >= (uint)SCB_MAX_CUBBY) {
		ASSERT(scbstate->ncubby < (uint)SCB_MAX_CUBBY);
		return -1;
	}

	/* housekeeping info is stored in scb_module struct */
	cubby_info = &scbstate->cubby_info[scbstate->ncubby++];
	cubby_info->fn_init = fn_init;
	cubby_info->fn_deinit = fn_deinit;
	cubby_info->fn_dump = fn_dump;
	cubby_info->context = context;

	/* actual cubby data is stored at the end of scb's */
	offset = scbstate->scbtotsize;

	/* roundup to pointer boundary */
	scbstate->scbtotsize = ROUNDUP(scbstate->scbtotsize + size, PTRSZ);

	return offset;
}

struct wlcband *
wlc_scbband(struct scb *scb)
{
	return SCBINFO(scb)->band;
}


#ifdef SCBFREELIST
static
struct scb_info *wlc_scbget_free(scb_module_t *scbstate)
{
	struct scb_info *ret = NULL;
	if (scbstate->free_list == NULL)
		return NULL;
	ret = SCBINFO(scbstate->free_list);
	SCBFREESANITYCHECK(ret->scbpub);
	scbstate->free_list = (ret->next ? ret->next->scbpub : NULL);
#ifdef MACOSX
	ret->next_copy = NULL;
#endif
	ret->next = NULL;
	wlc_scb_reset(scbstate, ret);
	return ret;
}

static
void wlc_scbadd_free(scb_module_t *scbstate, struct scb_info *ret)
{
	SCBFREESANITYCHECK(scbstate->free_list);
	ret->next = SCBINFO(scbstate->free_list);
	scbstate->free_list = ret->scbpub;
#ifdef MACOSX
	ret->scbpub->magic = ~SCB_MAGIC;
	ret->next_copy = ret->next;
#endif
}

static
void wlc_scbfreelist_free(scb_module_t *scbstate)
{
	struct scb_info *ret = NULL;
	ret = SCBINFO(scbstate->free_list);
	while (ret) {
#ifdef MACOSX
		SCBFREESANITYCHECK(ret->scbpub);
#endif
		scbstate->free_list = (ret->next ? ret->next->scbpub : NULL);
		wlc_scb_freemem(scbstate, ret);
		ret = scbstate->free_list ? SCBINFO(scbstate->free_list) : NULL;
	}
}

#if defined(BCMDBG)
static
void wlc_scbfreelist_dump(scb_module_t *scbstate, struct bcmstrbuf *b)
{
	struct scb_info *entry = NULL;
	int i = 1;

	bcm_bprintf(b, "scbfreelist:\n");
	entry = SCBINFO(scbstate->free_list);
	while (entry) {
#ifdef MACOSX
		SCBFREESANITYCHECK(entry->scbpub);
#endif
		bcm_bprintf(b, "%d: 0x%x\n", i, entry);
		entry = entry->next ? SCBINFO(entry->next->scbpub) : NULL;
		i++;
	}
}
#endif 
#endif /* SCBFREELIST */

void
wlc_internalscb_free(wlc_info_t *wlc, struct scb *scb)
{
	scb->permanent = FALSE;
	wlc_scbfree(wlc, scb);
}

static void
wlc_scb_reset(scb_module_t *scbstate, struct scb_info *scbinfo)
{
	struct scb *scbpub = scbinfo->scbpub;

	bzero((char*)scbinfo, sizeof(struct scb_info));
	scbinfo->scbpub = scbpub;
	bzero(scbpub, scbstate->scbtotsize);
	scbpub->scb_priv = (void *) scbinfo;
	/* init substructure pointers */
	scbpub->rssi_window = (int *)((char *)scbpub + sizeof(struct scb));
	scbpub->tx_path = (struct tx_path_node *)
	                ((char *)scbpub->rssi_window + (sizeof(int)*MA_WINDOW_SZ));
}

static struct scb_info *
wlc_scb_allocmem(scb_module_t *scbstate)
{
	struct scb_info *scbinfo = NULL;
	struct scb *scbpub;

	scbinfo = MALLOC(scbstate->pub->osh, sizeof(struct scb_info));
	if (!scbinfo) {
		WL_ERROR(("wl%d: %s: Internalscb alloc failure\n", scbstate->pub->unit,
		          __FUNCTION__));
		return NULL;
	}
	scbpub = MALLOC(scbstate->pub->osh, scbstate->scbtotsize);
	scbinfo->scbpub = scbpub;
	if (!scbpub) {
		/* set field to null so freeing mem does */
		/* not cause exception by freeing bad ptr */
		scbinfo->scbpub = NULL;
		wlc_scb_freemem(scbstate, scbinfo);
		WL_ERROR(("wl%d: %s: Internalscb alloc failure\n", scbstate->pub->unit,
		          __FUNCTION__));
		return NULL;
	}


	wlc_scb_reset(scbstate, scbinfo);

	return scbinfo;
}

struct scb *
wlc_internalscb_alloc(wlc_info_t *wlc, const struct ether_addr *ea, struct wlcband *band)
{
	struct scb_info *scbinfo = NULL;
	scb_module_t *scbstate = wlc->scbstate;
	int bcmerror = 0;

#ifdef SCBFREELIST
	/* If not found on freelist then allocate a new one */
	if ((scbinfo = wlc_scbget_free(scbstate)) == NULL)
#endif
	{
		scbinfo = wlc_scb_allocmem(scbstate);
		if (!scbinfo)
			return NULL;
	}

	bcmerror = wlc_scbinit(wlc, band, scbinfo, INTERNAL_SCB);
	if (bcmerror) {
		wlc_internalscb_free(wlc, scbinfo->scbpub);
		WL_ERROR(("wl%d: %s failed with err %d\n", wlc->pub->unit, __FUNCTION__, bcmerror));
		return NULL;
	}
	scbinfo->scbpub->permanent = TRUE;

	bcopy(ea, &scbinfo->scbpub->ea, sizeof(struct ether_addr));

	return (scbinfo->scbpub);
}

static struct scb *
wlc_userscb_alloc(wlc_info_t *wlc, const struct ether_addr *ea,
                  struct wlcband *band, wlc_bsscfg_t *bsscfg)
{
	scb_module_t *scbstate = wlc->scbstate;
	struct scb_info *scbinfo = NULL;
	struct scb *oldscb;
	int bcmerror;

	if (scbstate->nscb < wlc->pub->tunables->maxscb) {
#ifdef SCBFREELIST
		/* If not found on freelist then allocate a new one */
		if ((scbinfo = wlc_scbget_free(scbstate)) == NULL)
#endif
		{
			scbinfo = wlc_scb_allocmem(scbstate);
			if (!scbinfo) {
				WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
					wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			}
		}
	}
	if (!scbinfo) {
		/* free the oldest entry */
		if (!(oldscb = wlc_scbvictim(wlc))) {
			WL_ERROR(("wl%d: %s: no SCBs available to reclaim\n",
			          wlc->pub->unit, __FUNCTION__));
			return NULL;
		}
		if (!wlc_scbfree(wlc, oldscb)) {
			WL_ERROR(("wl%d: %s: Couldn't free a victimized scb\n",
			          wlc->pub->unit, __FUNCTION__));
			return NULL;
		}
		ASSERT(scbstate->nscb < wlc->pub->tunables->maxscb);
		/* allocate memory for scb */
		if (!(scbinfo = wlc_scb_allocmem(scbstate))) {
			WL_ERROR(("wl%d: %s: out of memory, malloced %d bytes\n",
			          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
			return NULL;
		}
	}

	scbstate->nscb++;

	/* add it to the link list */
	wlc_scb_list_add(wlc, scbinfo, bsscfg);

	bcmerror = wlc_scbinit(wlc, band, scbinfo, USER_SCB);
	if (bcmerror) {
		WL_ERROR(("wl%d: %s failed with err %d\n", wlc->pub->unit, __FUNCTION__, bcmerror));
		wlc_scb_list_del(wlc, scbinfo->scbpub, bsscfg);
		wlc_scbfree(wlc, scbinfo->scbpub);
		return NULL;
	}

	scbinfo->scbpub->ea = *ea;

	/* install it in the cache */
	wlc_scb_hash_add(wlc, scbinfo->scbpub, band->bandunit, bsscfg);

	return (scbinfo->scbpub);
}

static int
wlc_scbinit(wlc_info_t *wlc, struct wlcband *band, struct scb_info *scbinfo, uint32 scbflags)
{
	struct scb *scb = NULL;
	scb_module_t *scbstate = wlc->scbstate;
	cubby_info_t *cubby_info;
	uint i;
	int bcmerror = 0;

	scb = scbinfo->scbpub;
	ASSERT(scb != NULL);

	scb->used = wlc->pub->now;
	scb->bandunit = band->bandunit;
	scbinfo->band = band;
#if defined(AP) && defined(WLWNM)
	scb->rx_tstamp = wlc_wnm_timestamp(wlc->wnm_info);
#endif /* AP && WLWNM */

	for (i = 0; i < NUMPRIO; i++)
		scb->seqctl[i] = 0xFFFF;
	scb->seqctl_nonqos = 0xFFFF;

#ifdef MACOSX
	scb->magic = SCB_MAGIC;
#endif

	/* no other inits are needed for internal scb */
	if (scbflags & INTERNAL_SCB) {
		scb->flags2 |= SCB2_INTERNAL;
#ifdef INT_SCB_OPT
		return BCME_OK;
#endif
	}

	for (i = 0; i < scbstate->ncubby; i++) {
		cubby_info = &scbstate->cubby_info[i];
		if (cubby_info->fn_init) {
			bcmerror = cubby_info->fn_init(cubby_info->context, scb);
			if (bcmerror) {
				WL_ERROR(("wl%d: %s: Cubby failed\n",
				          wlc->pub->unit, __FUNCTION__));
				return bcmerror;
			}
		}
	}

#if defined(AP)
	wlc_scb_rssi_init(scb, WLC_RSSI_INVALID);
#endif

#ifdef WL11AC     /* hack for oper mode */
	scb->oper_mode = 0;
	scb->oper_mode_enabled = FALSE;
#endif

#ifdef AP
	scb->PS_pretend = PS_PRETEND_NOT_ACTIVE;
	scb->ps_pretend_failed_ack_count = 0;
#endif
	return bcmerror;
}

static void
wlc_scb_freemem(scb_module_t *scbstate, struct scb_info *scbinfo)
{

	if (scbinfo->scbpub)
		MFREE(scbstate->pub->osh, scbinfo->scbpub, scbstate->scbtotsize);
	MFREE(scbstate->pub->osh, scbinfo, sizeof(struct scb_info));
}

#ifdef PROP_TXSTATUS
void
wlc_scb_update_available_traffic_info(wlc_info_t *wlc, uint8 mac_handle, uint8 ta_bmp)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->mac_address_handle == mac_handle) {
			SCB_PROPTXTSTATUS_SETTIM(scb, ta_bmp);
			if (AP_ENAB(wlc->pub))
				wlc_apps_pvb_update_from_host(wlc, scb);
			break;
		}
	}
}
#endif /* PROP_TXSTATUS */

bool
wlc_scbfree(wlc_info_t *wlc, struct scb *scbd)
{
	struct scb_info *remove = SCBINFO(scbd);
	scb_module_t *scbstate = wlc->scbstate;
	cubby_info_t *cubby_info;
	uint i;
	uint8 prio;

#if defined(BCMDBG) || defined(WLMSG_WSEC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_WSEC */

	if (scbd->permanent)
		return FALSE;

	/* Return if SCB is already being deleted else mark it */
	if (scbd->flags & SCB_PENDING_FREE)
		return FALSE;

	scbd->flags |= SCB_PENDING_FREE;

#ifdef INT_SCB_OPT
	/* no other cleanups are needed for internal scb */
	if (SCB_INTERNAL(scbd)) {
		goto free;
	}
#endif

	/* free the per station key if one exists */
	if (scbd->key) {
		WL_WSEC(("wl%d: %s: deleting pairwise key for %s\n", wlc->pub->unit,
		        __FUNCTION__, bcm_ether_ntoa(&scbd->ea, eabuf)));
		ASSERT(!bcmp((char*)&scbd->key->ea, (char*)&scbd->ea, ETHER_ADDR_LEN));
		wlc_key_scb_delete(wlc, scbd);

	}

	for (i = 0; i < scbstate->ncubby; i++) {
		cubby_info = &scbstate->cubby_info[i];
		if (cubby_info->fn_deinit)
			cubby_info->fn_deinit(cubby_info->context, scbd);
	}

#ifdef PROP_TXSTATUS
	if (PROP_TXSTATUS_ENAB(wlc->pub)) {
		/* release MAC handle back to the pool, if applicable */
		if (scbd->mac_address_handle) {
			wlfc_MAC_table_update(wlc->wl, &scbd->ea.octet[0],
				WLFC_CTL_TYPE_MACDESC_DEL,
				scbd->mac_address_handle, ((scbd->bsscfg->wlcif == NULL) ?
				0 : scbd->bsscfg->wlcif->index));
			wlfc_release_MAC_descriptor_handle(wlc->wlfc_data,
				scbd->mac_address_handle);
			WLFC_DBGMESG(("STA: MAC-DEL for [%02x:%02x:%02x:%02x:%02x:%02x], "
				"handle: [%d], if:%d, t_idx:%d..\n",
				scbd->ea.octet[0], scbd->ea.octet[1], scbd->ea.octet[2],
				scbd->ea.octet[3], scbd->ea.octet[4], scbd->ea.octet[5],
				scbd->mac_address_handle,
				((scbd->bsscfg->wlcif == NULL) ? 0 : scbd->bsscfg->wlcif->index),
				WLFC_MAC_DESC_GET_LOOKUP_INDEX(scbd->mac_address_handle)));
		}
	}
#endif /* PROP_TXSTATUS */

#ifdef AP
	/* free any leftover authentication state */
	if (scbd->challenge) {
		MFREE(wlc->osh, scbd->challenge, 2 + scbd->challenge[1]);
		scbd->challenge = NULL;
	}
	/* free WDS state */
	if (scbd->wds != NULL) {
		if (scbd->wds->wlif) {
			wlc_if_event(wlc, WLC_E_IF_DEL, scbd->wds);
			wl_del_if(wlc->wl, scbd->wds->wlif);
			scbd->wds->wlif = NULL;
		}
		wlc_wlcif_free(wlc, wlc->osh, scbd->wds);
		scbd->wds = NULL;
	}
	/* free wpaie if stored */
	if (scbd->wpaie) {
		MFREE(wlc->osh, scbd->wpaie, scbd->wpaie_len);
		scbd->wpaie_len = 0;
		scbd->wpaie = NULL;
	}
#endif /* AP */

	/* free any frame reassembly buffer */
	for (prio = 0; prio < NUMPRIO; prio++) {
		if (scbd->fragbuf[prio]) {
			PKTFREE(wlc->osh, scbd->fragbuf[prio], FALSE);
			scbd->fragbuf[prio] = NULL;
			scbd->fragresid[prio] = 0;
		}
	}

#ifdef AP
	/* mark the aid unused */
	if (scbd->aid) {
		ASSERT(AID2AIDMAP(scbd->aid) < wlc->pub->tunables->maxscb);
		clrbit(scbd->bsscfg->aidmap, AID2AIDMAP(scbd->aid));
	}
#endif /* AP */

	scbd->state = 0;

#if defined(PKTC) || defined(PKTC_DONGLE)
	/* Clear scb pointer in rfc */
	wlc_scb_pktc_disable(scbd);
#endif

#ifndef INT_SCB_OPT
	if (SCB_INTERNAL(scbd)) {
		goto free;
	}
#endif
	if (!ETHER_ISMULTI(scbd->ea.octet)) {
		wlc_scb_hash_del(wlc, scbd, remove->band->bandunit, SCB_BSSCFG(scbd));
	}

	/* delete it from the link list */
	wlc_scb_list_del(wlc, scbd, SCB_BSSCFG(scbd));

	/* update total allocated scb number */
	scbstate->nscb--;

free:
#ifdef WL_RELMCAST
	wlc_relmcast_check(scbd);
#endif
#ifdef SCBFREELIST
	wlc_scbadd_free(scbstate, remove);
#else
	/* free scb memory */
	wlc_scb_freemem(scbstate, remove);
#endif

	return TRUE;
}

static void
wlc_scb_list_add(wlc_info_t *wlc, struct scb_info *scbinfo, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;

	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);

	SCBSANITYCHECK((scb_cfg)->scb);

	/* update scb link list */
	scbinfo->next = SCBINFO(scb_cfg->scb);
#ifdef MACOSX
	scbinfo->next_copy = scbinfo->next;
#endif
	scb_cfg->scb = scbinfo->scbpub;
}

static void
wlc_scb_list_del(wlc_info_t *wlc, struct scb *scbd, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;
	struct scb_info *scbinfo;
	struct scb_info *remove = SCBINFO(scbd);

	/* delete it from the link list */

	if (!bsscfg)
		return;

	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);
	scbinfo = SCBINFO(scb_cfg->scb);
	if (scbinfo == remove) {
		scb_cfg->scb = wlc_scb_getnext(scbd);
	} else {
		while (scbinfo) {
			SCBSANITYCHECK(scbinfo->scbpub);
			if (scbinfo->next == remove) {
				scbinfo->next = remove->next;
#ifdef MACOSX
				scbinfo->next_copy = scbinfo->next;
#endif
				break;
			}
			scbinfo = scbinfo->next;
		}
		ASSERT(scbinfo != NULL);
	}
}

/* free all scbs, unless permanent. Force indicates reclaim permanent as well */

void
wlc_scbclear(struct wlc_info *wlc, bool force)
{
	struct scb_iter scbiter;
	struct scb *scb;

	if (wlc->scbstate == NULL)
		return;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (force)
			scb->permanent = FALSE;
		wlc_scbfree(wlc, scb);
	}
	if (force)
		ASSERT(wlc->scbstate->nscb == 0);
}

/* free all scbs of a bsscfg */
void
wlc_scb_bsscfg_scbclear(struct wlc_info *wlc, wlc_bsscfg_t *bsscfg, bool perm)
{
	struct scb_iter scbiter;
	struct scb *scb;

	if (wlc->scbstate == NULL)
		return;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (scb->permanent) {
			if (!perm)
				continue;
			scb->permanent = FALSE;
		}
		wlc_scbfree(wlc, scb);
	}
}


static struct scb *
wlc_scbvictim(wlc_info_t *wlc)
{
	uint oldest;
	struct scb *scb;
	struct scb *oldscb;
	uint now, age;
	struct scb_iter scbiter;
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	wlc_bsscfg_t *bsscfg = NULL;

#ifdef AP
	/* search for an unauthenticated scb */
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (!scb->permanent && (scb->state == UNAUTHENTICATED))
			return scb;
	}
#endif /* AP */

	/* free the oldest scb */
	now = wlc->pub->now;
	oldest = 0;
	oldscb = NULL;
	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		bsscfg = SCB_BSSCFG(scb);
		ASSERT(bsscfg != NULL);
		if (BSSCFG_STA(bsscfg) && bsscfg->BSS && SCB_ASSOCIATED(scb))
			continue;
		if (!scb->permanent && ((age = (now - scb->used)) >= oldest)) {
			oldest = age;
			oldscb = scb;
		}
	}
	/* handle extreme case(s): all are permanent ... or there are no scb's at all */
	if (oldscb == NULL)
		return NULL;

#ifdef AP
	bsscfg = SCB_BSSCFG(oldscb);

	if (BSSCFG_AP(bsscfg)) {
		/* if the oldest authenticated SCB has only been idle a short time then
		 * it is not a candidate to reclaim
		 */
		if (oldest < SCB_SHORT_TIMEOUT)
			return NULL;

		/* notify the station that we are deauthenticating it */
		(void)wlc_senddeauth(wlc, &oldscb->ea, &bsscfg->BSSID,
		                     &bsscfg->cur_etheraddr,
		                     oldscb, DOT11_RC_INACTIVITY);
		wlc_deauth_complete(wlc, bsscfg, WLC_E_STATUS_SUCCESS, &oldscb->ea,
		              DOT11_RC_INACTIVITY, 0);
	}
#endif /* AP */

	WL_ASSOC(("wl%d: %s: relcaim scb %s, idle %d sec\n",  wlc->pub->unit, __FUNCTION__,
	          bcm_ether_ntoa(&oldscb->ea, eabuf), oldest));

	return oldscb;
}

#if defined(PKTC) || defined(PKTC_DONGLE)
static void
wlc_scb_pktc_enable(struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);
	wlc_info_t *wlc = bsscfg->wlc;

	SCB_PKTC_DISABLE(scb);

	if (wlc->wet && BSSCFG_STA(bsscfg))
		return;

	/* No chaining for wds, non qos, non ampdu stas */
	if (SCB_A4_DATA(scb) || !SCB_QOS(scb) || !SCB_WME(scb) || !SCB_AMPDU(scb))
		return;

	if (!SCB_ASSOCIATED(scb) && !SCB_AUTHORIZED(scb))
		return;

#ifdef PKTC_DONGLE
	if (BSSCFG_AP(scb->bsscfg) || BSS_TDLS_BUFFER_STA(scb->bsscfg))
		return;
#endif

	WL_NONE(("wl%d: auth %d openshared %d WPA_auth %d wsec 0x%x "
	         "scb wsec 0x%x scb key %p algo %d\n",
	         wlc->pub->unit, bsscfg->auth, bsscfg->openshared,
	         bsscfg->WPA_auth, bsscfg->wsec, scb->wsec, scb->key,
	         scb->key ? scb->key->algo : 0));

	/* Enable packet chaining for open auth or wpa2 aes only for now */
	if (((bsscfg->WPA_auth == WPA_AUTH_DISABLED) && !WSEC_WEP_ENABLED(bsscfg->wsec)) ||
	    (WSEC_ENABLED(bsscfg->wsec) && WSEC_AES_ENABLED(scb->wsec) &&
	    (scb->key != NULL) && (scb->key->algo == CRYPTO_ALGO_AES_CCM) &&
	    !WLC_SW_KEYS(bsscfg->wlc, bsscfg)))
		SCB_PKTC_ENABLE(scb);

	return;
}

static void
wlc_scb_pktc_disable(struct scb *scb)
{
	wlc_bsscfg_t *bsscfg = SCB_BSSCFG(scb);

	if (bsscfg && !SCB_AUTHORIZED(scb) && !SCB_ASSOCIATED(scb)) {
		int32 cidx;
		wlc_info_t *wlc = bsscfg->wlc;
		/* Invalidate rfc entry if scb is in it */
		cidx = BSSCFG_AP(bsscfg) ? 1 : 0;
		if (wlc->pktc_info->rfc[cidx].scb == scb) {
			WL_NONE(("wl%d: %s: Invalidate rfc %d before freeing scb %p\n",
			         wlc->pub->unit, __FUNCTION__, cidx, scb));
			wlc->pktc_info->rfc[cidx].scb = NULL;
		}
		SCB_PKTC_DISABLE(scb);
	}
}
#endif /* PKTC */

/* "|" operation. */
void
wlc_scb_setstatebit(struct scb *scb, uint8 state)
{

	WL_NONE(("set state %x\n", state));
	ASSERT(scb != NULL);

	if (state & AUTHENTICATED)
	{
		scb->state &= ~PENDING_AUTH;
	}
	if (state & ASSOCIATED)
	{
		ASSERT((scb->state | state) & AUTHENTICATED);
		scb->state &= ~PENDING_ASSOC;
	}

#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	if (state & AUTHORIZED)
	{
		if (!((scb->state | state) & ASSOCIATED) && !SCB_LEGACY_WDS(scb) &&
		    !SCB_IS_IBSS_PEER(scb)) {
			char eabuf[ETHER_ADDR_STR_LEN];
			WL_ASSOC(("wlc_scb : authorized %s is not a associated station, "
				"state = %x\n", bcm_ether_ntoa(&scb->ea, eabuf),
				scb->state));
		}
	}
#endif /* BCMDBG || WLMSG_ASSOC */

	scb->state |= state;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));

#if defined(PKTC) || defined(PKTC_DONGLE)
	/* When transitioning to ASSOCIATED/AUTHORIZED state try if we can
	 * enable packet chaining for this SCB.
	 */
	if (SCB_BSSCFG(scb)) {
		wlc_scb_pktc_enable(scb);
	}
#endif
#ifdef WL_RELMCAST
	wlc_relmcast_check(scb);
#endif
}

/* "& ~" operation */
void
wlc_scb_clearstatebit(struct scb *scb, uint8 state)
{
	ASSERT(scb != NULL);
	WL_NONE(("clear state %x\n", state));
	scb->state &= ~state;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));
#if defined(PKTC) || defined(PKTC_DONGLE)
	/* Clear scb pointer in rfc */
	wlc_scb_pktc_disable(scb);
#endif
#ifdef WLWNM
	if (state & ASSOCIATED)
		wlc_wnm_clear_scbstate(scb);
#endif /* WLWNM */
#ifdef WL_RELMCAST
	wlc_relmcast_check(scb);
#endif
}


/* "|" operation . idx = position of the bsscfg in the wlc array of multi ssids.
*/
void
wlc_scb_setstatebit_bsscfg(struct scb *scb, uint8 state, int idx)
{
	ASSERT(scb != NULL);
	WL_NONE(("set state : %x   bsscfg idx : %d\n", state, idx));
	if (state & ASSOCIATED)
	{

		ASSERT(SCB_AUTHENTICATED_BSSCFG(scb, idx));
		/* clear all bits (idx is set below) */
		memset(&scb->auth_bsscfg, 0, SCB_BSSCFG_BITSIZE);
		scb->state &= ~PENDING_ASSOC;
	}

	if (state & AUTHORIZED)
	{
		ASSERT(SCB_ASSOCIATED_BSSCFG(scb, idx));
	}
	setbit(scb->auth_bsscfg, idx);
	scb->state |= state;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));
#ifdef WL_RELMCAST
	wlc_relmcast_check(scb);
#endif
}


/*
 * "& ~" operation .
 * idx = position of the bsscfg in the wlc array of multi ssids.
 */
void
wlc_scb_clearstatebit_bsscfg(struct scb *scb, uint8 state, int idx)

{
	int i;
	ASSERT(scb != NULL);
	WL_NONE(("clear state : %x   bsscfg idx : %d\n", state, idx));
	/*
	   any clear of a stable state should lead to clear a bit
	   Warning though : this implies that, if we want to switch from
	   associated to authenticated, the clear happens before the set
	   otherwise this bit will be clear in authenticated state.
	*/
	if ((state & AUTHENTICATED) || (state & ASSOCIATED) || (state & AUTHORIZED))
	{
		clrbit(scb->auth_bsscfg, idx);
	}
	/* quik hack .. clear first ... */
	scb->state &= ~state;
	for (i = 0; i < SCB_BSSCFG_BITSIZE; i++)
	{
		/* reset if needed */
		if (scb->auth_bsscfg[i])
		{
			scb->state |= state;
			break;
		}
	}
#ifdef WL_RELMCAST
	wlc_relmcast_check(scb);
#endif
}

/* reset all state. */
void
wlc_scb_resetstate(struct scb *scb)
{
	WL_NONE(("reset state\n"));
	ASSERT(scb != NULL);
	memset(&scb->auth_bsscfg, 0, SCB_BSSCFG_BITSIZE);
	scb->state = 0;
	WL_NONE(("wlc_scb : state = %x\n", scb->state));
#ifdef WL_RELMCAST
	wlc_relmcast_check(scb);
#endif
}

/* set/change bsscfg */
void
wlc_scb_set_bsscfg(struct scb *scb, wlc_bsscfg_t *cfg)
{
	wlc_bsscfg_t *oldcfg = SCB_BSSCFG(scb);
	wlc_info_t *wlc;

	ASSERT(cfg != NULL);

	wlc = cfg->wlc;

	scb->bsscfg = cfg;

	/* when assigning the owner the first time or when assigning a different owner */
	if (oldcfg == NULL || oldcfg != cfg) {
		wlcband_t *band = wlc_scbband(scb);
		wlc_rateset_t *rs;

		/* changing bsscfg */
		if ((oldcfg != NULL) && !SCB_INTERNAL(scb)) {
			/* delete scb from hash table and scb list of old bsscfg */
			wlc_scb_hash_del(wlc, scb, band->bandunit, oldcfg);
			wlc_scb_list_del(wlc, scb, oldcfg);
			/* add scb to hash table and scb list of new bsscfg */
			wlc_scb_hash_add(wlc, scb, band->bandunit, cfg);
			wlc_scb_list_add(wlc, SCBINFO(scb), cfg);
		}

		/* flag the scb is used by IBSS */
		if (cfg->BSS)
			wlc_scb_clearstatebit(scb, TAKEN4IBSS);
		else {
			wlc_scb_resetstate(scb);
			if (!BSSCFG_IS_DPT(cfg) && !BSSCFG_IS_TDLS(cfg))
				wlc_scb_setstatebit(scb, TAKEN4IBSS);
		}

		/* invalidate txc */
		wlc_txc_invalidate(scb);

		/* use current, target, or per-band default rateset? */
		if (wlc->pub->up && wlc_valid_chanspec(wlc->cmi, cfg->target_bss->chanspec))
			if (cfg->associated)
				rs = &cfg->current_bss->rateset;
			else
				rs = &cfg->target_bss->rateset;
		else
			rs = &band->defrateset;

		/*
		 * Initialize the per-scb rateset:
		 * - if we are AP, start with only the basic subset of the
		 *	network rates.  It will be updated when receive the next
		 *	probe request or association request.
		 * - if we are IBSS and gmode, special case:
		 *	start with B-only subset of network rates and probe for ofdm rates
		 * - else start with the network rates.
		 *	It will be updated on join attempts.
		 */
		/* initialize the scb rateset */
		if (BSSCFG_AP(cfg)) {
			uint8 mcsallow = 0;
#ifdef WLP2P
			if (BSS_P2P_ENAB(wlc, cfg))
				wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_OFDM,
				                       RATE_MASK, wlc_get_mcsallow(wlc, cfg));
			else
#endif
			if (BSS_N_ENAB(wlc, cfg))
				mcsallow = WLC_MCS_ALLOW;
			wlc_rateset_filter(rs, &scb->rateset, TRUE, WLC_RATES_CCK_OFDM,
			                   RATE_MASK, mcsallow);
		}
		else if (!cfg->BSS && band->gmode) {
			wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_CCK,
			                   RATE_MASK, 0);
			/* if resulting set is empty, then take all network rates instead */
			if (scb->rateset.count == 0)
				wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_CCK_OFDM,
				                   RATE_MASK, 0);
		}
		else {
#ifdef WLP2P
			if (BSS_P2P_ENAB(wlc, cfg))
				wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_OFDM,
				                   RATE_MASK, wlc_get_mcsallow(wlc, cfg));
			else
#endif
			wlc_rateset_filter(rs, &scb->rateset, FALSE, WLC_RATES_CCK_OFDM,
			                   RATE_MASK, 0);
		}

		if (!SCB_INTERNAL(scb)) {
			wlc_scb_ratesel_init(wlc, scb);
#ifdef STA
			/* send ofdm rate probe */
			if (BSSCFG_STA(cfg) && !cfg->BSS &&
			    band->gmode && wlc->pub->up)
				wlc_rateprobe(wlc, cfg, &scb->ea, WLC_RATEPROBE_RATE);
#endif
		}
	}
}

static void
wlc_scb_bsscfg_reinit(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	uint prev_count;
	const wlc_rateset_t *rs;
	wlcband_t *band;
	struct scb *scb;
	struct scb_iter scbiter;
	bool cck_only;
	bool reinit_forced;

	WL_INFORM(("wl%d: %s: bandunit 0x%x phy_type 0x%x gmode 0x%x\n", wlc->pub->unit,
		__FUNCTION__, wlc->band->bandunit, wlc->band->phytype, wlc->band->gmode));

	/* sanitize any existing scb rates against the current hardware rates */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		prev_count = scb->rateset.count;
		/* Keep only CCK if gmode == GMODE_LEGACY_B */
		band = SCBINFO(scb)->band;
		if (BAND_2G(band->bandtype) && (band->gmode == GMODE_LEGACY_B)) {
			rs = &cck_rates;
			cck_only = TRUE;
		} else {
			rs = &band->hw_rateset;
			cck_only = FALSE;
		}
		if (!wlc_rate_hwrs_filter_sort_validate(&scb->rateset, rs, FALSE,
			wlc->stf->txstreams)) {
			/* continue with default rateset.
			 * since scb rateset does not carry basic rate indication,
			 * clear basic rate bit.
			 */
			WL_RATE(("wl%d: %s: invalid rateset in scb 0x%p bandunit 0x%x "
				"phy_type 0x%x gmode 0x%x\n", wlc->pub->unit, __FUNCTION__,
				scb, band->bandunit, band->phytype, band->gmode));
#ifdef BCMDBG
			wlc_rateset_show(wlc, &scb->rateset, &scb->ea);
#endif

			wlc_rateset_default(&scb->rateset, &band->hw_rateset,
			                    band->phytype, band->bandtype, cck_only, RATE_MASK,
			                    wlc_get_mcsallow(wlc, scb->bsscfg),
			                    CHSPEC_WLC_BW(scb->bsscfg->current_bss->chanspec),
			                    wlc->stf->txstreams, WLC_VHT_FEATURES_RATES(wlc->pub));
			reinit_forced = TRUE;
		}
		else
			reinit_forced = FALSE;

		/* if the count of rates is different, then the rate state
		 * needs to be reinitialized
		 */
		if (reinit_forced || (scb->rateset.count != prev_count))
			wlc_scb_ratesel_init(wlc, scb);

		WL_RATE(("wl%d: %s: bandunit 0x%x, phy_type 0x%x gmode 0x%x. final rateset is\n",
			wlc->pub->unit, __FUNCTION__,
			band->bandunit, band->phytype, band->gmode));
#ifdef BCMDBG
		wlc_rateset_show(wlc, &scb->rateset, &scb->ea);
#endif
	}
}

void
wlc_scb_reinit(wlc_info_t *wlc)
{
	int32 idx;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, idx, bsscfg) {
		wlc_scb_bsscfg_reinit(wlc, bsscfg);
	}
}

static INLINE struct scb* BCMFASTPATH
_wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, int bandunit)
{
	int indx;
	struct scb_info *scbinfo;
	scb_bsscfg_cubby_t *scb_cfg;

	ASSERT(bsscfg != NULL);

	/* All callers of wlc_scbfind() should first be checking to see
	 * if the SCB they're looking for is a BC/MC address.  Because we're
	 * using per bsscfg BCMC SCBs, we can't "find" BCMC SCBs without
	 * knowing which bsscfg.
	 */
	ASSERT(ea && !ETHER_ISMULTI(ea));


	/* search for the scb which corresponds to the remote station ea */
	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);
	indx = SCBHASHINDEX(scb_cfg->nscbhash, ea->octet);
	scbinfo = (scb_cfg->scbhash[bandunit][indx] ?
	           SCBINFO(scb_cfg->scbhash[bandunit][indx]) :
	           NULL);
	for (; scbinfo; scbinfo = scbinfo->hashnext) {
		SCBSANITYCHECK(scbinfo->scbpub);

		if (eacmp((const char*)ea, (const char*)&(scbinfo->scbpub->ea)) == 0)
			break;
	}

	return (scbinfo ? scbinfo->scbpub : NULL);
}

/* Find station control block corresponding to the remote id */
struct scb * BCMFASTPATH
wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea)
{
	return _wlc_scbfind(wlc, bsscfg, ea, wlc->band->bandunit);
}

/*
 * Lookup station control block corresponding to the remote id.
 * If not found, create a new entry.
 */
static INLINE struct scb *
_wlc_scblookup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, int bandunit)
{
	struct scb *scb;
	struct wlcband *band;

	/* Don't allocate/find a BC/MC SCB this way. */
	ASSERT(!ETHER_ISMULTI(ea));
	if (ETHER_ISMULTI(ea))
		return NULL;

	if ((scb = _wlc_scbfind(wlc, bsscfg, ea, bandunit)))
		return (scb);

	/* no scb match, allocate one for the desired bandunit */
	band = wlc->bandstate[bandunit];
	scb = wlc_userscb_alloc(wlc, ea, band, bsscfg);
	if (!scb)
		return (NULL);

	return (scb);
}

struct scb *
wlc_scblookup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea)
{
	return (_wlc_scblookup(wlc, bsscfg, ea, wlc->band->bandunit));
}

struct scb *
wlc_scblookupband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, int bandunit)
{
	/* assert that the band is the current band, or we are dual band and it is the other band */
	ASSERT((bandunit == (int)wlc->band->bandunit) ||
	       (NBANDS(wlc) > 1 && bandunit == (int)OTHERBANDUNIT(wlc)));

	return (_wlc_scblookup(wlc, bsscfg, ea, bandunit));
}

/* Get scb from band */
struct scb * BCMFASTPATH
wlc_scbfindband(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea, int bandunit)
{
	/* assert that the band is the current band, or we are dual band and it is the other band */
	ASSERT((bandunit == (int)wlc->band->bandunit) ||
	       (NBANDS(wlc) > 1 && bandunit == (int)OTHERBANDUNIT(wlc)));

	return (_wlc_scbfind(wlc, bsscfg, ea, bandunit));
}

/* Determine if any SCB associated to ap cfg
 * cfg specifies a specific ap cfg to compare to.
 * If cfg is NULL, then compare to any ap cfg.
 */
bool
wlc_scb_associated_to_ap(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	struct scb_iter scbiter;
	struct scb *scb;
	bool associated = FALSE;

	ASSERT((cfg == NULL) || BSSCFG_AP(cfg));

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (SCB_ASSOCIATED(scb) && BSSCFG_AP(scb->bsscfg)) {
			if ((cfg == NULL) || (cfg == scb->bsscfg)) {
				associated = TRUE;
			}
		}
	}

	return (associated);
}

void wlc_scb_switch_band(wlc_info_t *wlc, struct scb *scb, int new_bandunit,
	wlc_bsscfg_t *bsscfg)
{
	struct scb_info *scbinfo = SCBINFO(scb);

	/* first, del scb from hash table in old band */
	wlc_scb_hash_del(wlc, scb, scb->bandunit, bsscfg);
	/* next add scb to hash table in new band */
	wlc_scb_hash_add(wlc, scb, new_bandunit, bsscfg);
	/* update the scb's band */
	scb->bandunit = (uint)new_bandunit;
	scbinfo->band = wlc->bandstate[new_bandunit];

	return;
}

/* Move the scb's band info.
 * Parameter description:
 *
 * wlc - global wlc_info structure
 * bsscfg - the bsscfg that is about to move to a new chanspec
 * chanspec - the new chanspec the bsscfg is moving to
 *
 */
void
wlc_scb_update_band_for_cfg(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, chanspec_t chanspec)
{
	struct scb_iter scbiter;
	struct scb *scb, *stale_scb;
	int bandunit;
	bool reinit = FALSE;

	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (SCB_ASSOCIATED(scb)) {
			bandunit = CHSPEC_WLCBANDUNIT(chanspec);
			if (scb->bandunit != (uint)bandunit) {
				/* We're about to move our scb to the new band.
				 * Check to make sure there isn't an scb entry for us there.
				 * If there is one for us, delete it first.
				 */
				if ((stale_scb = _wlc_scbfind(wlc, bsscfg,
				                      &bsscfg->BSSID, bandunit)) &&
				    (stale_scb->permanent == FALSE)) {
					WL_ASSOC(("wl%d.%d: %s: found stale scb %p on %s band, "
					          "remove it\n",
					          wlc->pub->unit, bsscfg->_idx, __FUNCTION__,
					          stale_scb,
					          (bandunit == BAND_5G_INDEX) ? "5G" : "2G"));
					/* mark the scb for removal */
					stale_scb->stale_remove = TRUE;
				}
				/* Now perform the move of our scb to the new band */
				wlc_scb_switch_band(wlc, scb, bandunit, bsscfg);
				reinit = TRUE;
			}
		}
	}
	/* remove stale scb's marked for removal */
	FOREACH_BSS_SCB(wlc->scbstate, &scbiter, bsscfg, scb) {
		if (scb->stale_remove == TRUE) {
			WL_ASSOC(("remove stale scb %p\n", scb));
			scb->stale_remove = FALSE;
			wlc_scbfree(wlc, scb);
		}
	}

	if (reinit) {
		wlc_scb_reinit(wlc);
	}
}

struct scb *
wlc_scbibssfindband(wlc_info_t *wlc, const struct ether_addr *ea, int bandunit,
                    wlc_bsscfg_t **bsscfg)
{
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	/* assert that the band is the current band, or we are dual band
	 * and it is the other band.
	 */
	ASSERT((bandunit == (int)wlc->band->bandunit) ||
	       (NBANDS(wlc) > 1 && bandunit == (int)OTHERBANDUNIT(wlc)));

	*bsscfg = NULL;

	FOREACH_IBSS(wlc, idx, cfg) {
		/* Find the bsscfg and scb matching specified peer mac */
		scb = _wlc_scbfind(wlc, cfg, ea, bandunit);
		if (scb != NULL) {
			*bsscfg = cfg;
			break;
		}
	}

	return scb;
}

struct scb * BCMFASTPATH
wlc_scbbssfindband(wlc_info_t *wlc, const struct ether_addr *hwaddr,
                   const struct ether_addr *ea, int bandunit, wlc_bsscfg_t **bsscfg)
{
	int idx;
	wlc_bsscfg_t *cfg;
	struct scb *scb = NULL;

	/* assert that the band is the current band, or we are dual band
	 * and it is the other band.
	 */
	ASSERT((bandunit == (int)wlc->band->bandunit) ||
	       (NBANDS(wlc) > 1 && bandunit == (int)OTHERBANDUNIT(wlc)));

	*bsscfg = NULL;

	FOREACH_BSS(wlc, idx, cfg) {
		/* Find the bsscfg and scb matching specified hwaddr and peer mac */
		if (eacmp(cfg->cur_etheraddr.octet, hwaddr->octet) == 0) {
			scb = _wlc_scbfind(wlc, cfg, ea, bandunit);
			if (scb != NULL) {
				*bsscfg = cfg;
				break;
			}
		}
	}

	return scb;
}

/* (de)authorize/(de)authenticate single station
 * 'enable' TRUE means authorize, FLASE means deauthorize/deauthenticate
 * 'flag' is AUTHORIZED or AUTHENICATED for the type of operation
 * 'rc' is the reason code for a deauthenticate packet
 */
void
wlc_scb_set_auth(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, struct scb *scb, bool enable, uint32 flag,
                 int rc)
{
#if defined(BCMDBG) || defined(WLMSG_ASSOC)
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG || WLMSG_ASSOC */
	void *pkt = NULL;

	if (enable) {
		if (flag == AUTHORIZED) {
			wlc_scb_setstatebit(scb, AUTHORIZED);
			scb->flags &= ~SCB_DEAUTH;
#ifdef WL11N
			if (SCB_MFP(scb) && N_ENAB(wlc->pub) && SCB_AMPDU(scb) &&
				(scb->wsec == AES_ENABLED))
				wlc_txmod_config(wlc, scb, TXMOD_AMPDU);
#endif /* WL11N */
#ifdef TRAFFIC_MGMT
		if (BSSCFG_AP(bsscfg)) {
			wlc_scb_trf_mgmt(wlc, bsscfg, scb);
		}
#endif
		} else {
			wlc_scb_setstatebit(scb, AUTHENTICATED);
		}
	} else {
		if (flag == AUTHORIZED) {

			wlc_scb_clearstatebit(scb, AUTHORIZED);
		} else {

			if (wlc->pub->up && (SCB_AUTHENTICATED(scb) || SCB_LEGACY_WDS(scb))) {
				pkt = wlc_senddeauth(wlc, &scb->ea, &bsscfg->BSSID,
				                     &bsscfg->cur_etheraddr,
				                     scb, (uint16)rc);
			}
			if (pkt != NULL) {
				wlc_deauth_send_cbargs_t *args;

				args = MALLOC(wlc->osh, sizeof(wlc_deauth_send_cbargs_t));
				bcopy(&scb->ea, &args->ea, sizeof(struct ether_addr));
				args->_idx = WLC_BSSCFG_IDX(bsscfg);
				if (wlc_pkt_callback_register(wlc,
					wlc_deauth_sendcomplete, (void *)args, pkt))
					WL_ERROR(("wl%d: wlc_scb_set_auth: could not "
					          "register callback\n", wlc->pub->unit));
			}
		}
	}
	WL_ASSOC(("wl%d: %s: %s %s%s\n", wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf),
		(enable ? "" : "de"),
		((flag == AUTHORIZED) ? "authorized" : "authenticated")));
}

static void
wlc_scb_hash_add(wlc_info_t *wlc, struct scb *scb, int bandunit, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);
	int indx = SCBHASHINDEX(scb_cfg->nscbhash, scb->ea.octet);
	struct scb_info *scbinfo = (scb_cfg->scbhash[bandunit][indx] ?
	                            SCBINFO(scb_cfg->scbhash[bandunit][indx]) : NULL);

	SCBINFO(scb)->hashnext = scbinfo;
#ifdef MACOSX
	SCBINFO(scb)->hashnext_copy = SCBINFO(scb)->hashnext;
#endif

	scb_cfg->scbhash[bandunit][indx] = scb;
}

static void
wlc_scb_hash_del(wlc_info_t *wlc, struct scb *scbd, int bandunit, wlc_bsscfg_t *bsscfg)
{
	scb_bsscfg_cubby_t *scb_cfg;
	int indx;
	struct scb_info *scbinfo;
	struct scb_info *remove = SCBINFO(scbd);

	if (!bsscfg) {
		WL_ERROR(("wl%d: Null bsscfg in %s \n", wlc->pub->unit, __FUNCTION__));
		return;
	}

	scb_cfg = SCB_BSSCFG_CUBBY(wlc->scbstate, bsscfg);
	indx = SCBHASHINDEX(scb_cfg->nscbhash, scbd->ea.octet);

	/* delete it from the hash */
	scbinfo = (scb_cfg->scbhash[bandunit][indx] ?
	           SCBINFO(scb_cfg->scbhash[bandunit][indx]) : NULL);
	ASSERT(scbinfo != NULL);
	SCBSANITYCHECK(scbinfo->scbpub);
	/* special case for the first */
	if (scbinfo == remove) {
		if (scbinfo->hashnext)
		    SCBSANITYCHECK(scbinfo->hashnext->scbpub);
		scb_cfg->scbhash[bandunit][indx] =
		        (scbinfo->hashnext ? scbinfo->hashnext->scbpub : NULL);
	} else {
		for (; scbinfo; scbinfo = scbinfo->hashnext) {
			SCBSANITYCHECK(scbinfo->hashnext->scbpub);
			if (scbinfo->hashnext == remove) {
				scbinfo->hashnext = remove->hashnext;
#ifdef MACOSX
				scbinfo->hashnext_copy = scbinfo->hashnext;
#endif
				break;
			}
		}
		ASSERT(scbinfo != NULL);
	}
}

#ifdef AP
void
wlc_scb_wds_free(struct wlc_info *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		if (scb->wds) {
			scb->permanent = FALSE;
			wlc_scbfree(wlc, scb);
		}
	}
}
#endif /* AP */


#if defined(BCMDBG)
static void
wlc_scb_bsscfg_dump(void *context, wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	uint k, i;
	struct scb *scb;
	char eabuf[ETHER_ADDR_STR_LEN];
	char flagstr[64];
	char flagstr2[64];
	char flagstr3[64];
#ifdef WL11AC
	char vhtflagsstr[64];
#endif
	char statestr[64];
	struct scb_iter scbiter;
	cubby_info_t *cubby_info;
	scb_module_t *scbstate = (scb_module_t *)context;
#ifdef AP
	char ssidbuf[SSID_FMT_BUF_LEN] = "";
#endif /* AP */

	bcm_bprintf(b, "idx  ether_addr\n");
	k = 0;
	FOREACH_BSS_SCB(scbstate, &scbiter, cfg, scb) {
		bcm_format_flags(scb_flags, scb->flags, flagstr, 64);
		bcm_format_flags(scb_flags2, scb->flags2, flagstr2, 64);
		bcm_format_flags(scb_flags3, scb->flags3, flagstr3, 64);
		bcm_format_flags(scb_states, scb->state, statestr, 64);
#ifdef WL11AC
		bcm_format_flags(scb_vht_flags, scb->vht_flags, vhtflagsstr, 64);
#endif

		bcm_bprintf(b, "%3d%s %s\n", k, (scb->permanent? "*":" "),
			bcm_ether_ntoa(&scb->ea, eabuf));

		bcm_bprintf(b, "     State:0x%02x (%s) Used:%d(%d)\n",
		            scb->state, statestr, scb->used,
		            (int)(scb->used - scbstate->pub->now));
		bcm_bprintf(b, "     Band:%s",
		            ((scb->bandunit == BAND_2G_INDEX) ? BAND_2G_NAME :
		             BAND_5G_NAME));
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Flags:0x%x", scb->flags);
		if (flagstr[0] != '\0')
			bcm_bprintf(b, " (%s)", flagstr);
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Flags2:0x%x", scb->flags2);
		if (flagstr2[0] != '\0')
			bcm_bprintf(b, " (%s)", flagstr2);
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Flags3:0x%x", scb->flags3);
		if (flagstr3[0] != '\0')
			bcm_bprintf(b, " (%s)", flagstr3);
		bcm_bprintf(b, "\n");
#ifdef WL11AC
		if (SCB_VHT_CAP(scb)) {
			bcm_bprintf(b, " VHT Flags:0x%x", scb->vht_flags);
			bcm_bprintf(b, " VHT rxmcsmap:0x%x", scb->vht_rxmcsmap);
			if (vhtflagsstr[0] != '\0')
				bcm_bprintf(b, " (%s)", vhtflagsstr);
		}
		if (scb->stbc_num > 0) {
			bcm_bprintf(b, " stbc num = %d", scb->stbc_num);
		}
#endif /* WL11AC */
		bcm_bprintf(b, "\n");
		bcm_bprintf(b, "     Cfg:%d(%p)", WLC_BSSCFG_IDX(cfg), cfg);
		if (scb->flags & SCB_AMSDUCAP)
			bcm_bprintf(b, " AMSDU-MTU ht:%d vht:%d", scb->amsdu_ht_mtu_pref,
				scb->amsdu_vht_mtu_pref);
		bcm_bprintf(b, "\n");

		if (scb->key) {
			bcm_bprintf(b, "     Key:%d", scb->key->idx);
			bcm_bprintf(b, " Key ID:%d algo:%s length:%d data:",
				scb->key->id,
			        bcm_crypto_algo_name(scb->key->algo),
				scb->key->idx, scb->key->len);
			if (scb->key->len)
				bcm_bprintf(b, "0x");
			for (i = 0; i < scb->key->len; i++)
				bcm_bprintf(b, "%02X", scb->key->data[i]);
			for (i = 0; i < scb->key->len; i++)
				if (!bcm_isprint(scb->key->data[i]))
					break;
			if (i == scb->key->len)
				bcm_bprintf(b, " (%.*s)", scb->key->len, scb->key->data);
			bcm_bprintf(b, "\n");
		}

		wlc_dump_rateset("     rates", &scb->rateset, b);
		bcm_bprintf(b, "\n");

		if (scb->rateset.htphy_membership) {
			bcm_bprintf(b, "     membership %d(b)",
				(scb->rateset.htphy_membership & RATE_MASK));
			bcm_bprintf(b, "\n");
		}
#ifdef AP
		if (BSSCFG_AP(cfg)) {
			bcm_bprintf(b, "     AID:0x%x PS:%d Listen:%d WDS:%d(%p) RSSI:%d",
			               scb->aid, scb->PS, scb->listen, (scb->wds ? 1 : 0),
			               scb->wds, wlc_scb_rssi(scb));
			wlc_format_ssid(ssidbuf, cfg->SSID, cfg->SSID_len);
			bcm_bprintf(b, " BSS %d \"%s\"\n",
			            WLC_BSSCFG_IDX(cfg), ssidbuf);
		}
#endif
#ifdef STA
		if (BSSCFG_STA(cfg)) {
			bcm_bprintf(b, "     MAXSP:%u DEFL:0x%x TRIG:0x%x DELV:0x%x\n",
			            scb->apsd.maxsplen, scb->apsd.ac_defl,
			            scb->apsd.ac_trig, scb->apsd.ac_delv);
		}
#endif

#ifdef WL11N
		if (N_ENAB(scbstate->pub) && SCB_HT_CAP(scb)) {
			wlc_dump_mcsset("     HT mcsset :", &scb->rateset.mcs[0], b);
			bcm_bprintf(b,  "\n     HT capabilites 0x%04x ampdu_params 0x%02x "
			    "mimops_enabled %d mimops_rtsmode %d",
			    scb->ht_capabilities, scb->ht_ampdu_params, scb->ht_mimops_enabled,
			    scb->ht_mimops_rtsmode);
			bcm_bprintf(b,  "\n     wsec 0x%x", scb->wsec);
			bcm_bprintf(b, "\n");
		}
		wlc_dump_rclist("     rclist", scb->rclist, scb->rclen, b);
#endif  /* WL11N */
#if defined(WL11AC)
		if (VHT_ENAB_BAND(scbstate->pub, scbstate->wlc->band->bandtype) &&
		SCB_VHT_CAP(scb)) {
			bcm_bprintf(b, "     VHT ratemask: 0x%x\n", scb->vht_ratemask);
			wlc_dump_vht_mcsmap("     VHT mcsmap:", scb->rateset.vht_mcsmap, b);
		}
#endif /* WL11AC */
		wlc_dump_rspec(context, wlc_scb_ratesel_get_primary(cfg->wlc, scb, NULL), b);

#if defined(STA) && defined(DBG_BCN_LOSS)
		bcm_bprintf(b,	"	  last_rx:%d last_rx_rssi:%d last_bcn_rssi: "
			"%d last_tx: %d\n",
			scb->dbg_bcn.last_rx, scb->dbg_bcn.last_rx_rssi, scb->dbg_bcn.last_bcn_rssi,
			scb->dbg_bcn.last_tx);
#endif

		for (i = 0; i < scbstate->ncubby; i++) {
			cubby_info = &scbstate->cubby_info[i];
			if (cubby_info->fn_dump)
				cubby_info->fn_dump(cubby_info->context, scb, b);
		}

		wlc_scb_txpath_dump(cfg->wlc, scb, b);

		k++;
	}

	return;
}

static int
wlc_dump_scb(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	int32 idx;
	wlc_bsscfg_t *bsscfg;

	FOREACH_BSS(wlc, idx, bsscfg) {
		wlc_scb_bsscfg_dump(wlc->scbstate, bsscfg, b);
	}

#ifdef SCBFREELIST
	wlc_scbfreelist_dump(wlc->scbstate, b);
#endif /* SCBFREELIST */
	return 0;
}
#endif 

void
wlc_scb_sortrates(wlc_info_t *wlc, struct scb *scb)
{
	struct scb_info *scbinfo = SCBINFO(scb);
	wlc_rate_hwrs_filter_sort_validate(&scb->rateset, &scbinfo->band->hw_rateset, FALSE,
		wlc->stf->txstreams);
}

void
BCMINITFN(wlc_scblist_validaterates)(wlc_info_t *wlc)
{
	struct scb *scb;
	struct scb_iter scbiter;

	FOREACHSCB(wlc->scbstate, &scbiter, scb) {
		wlc_scb_sortrates(wlc, scb);
		if (scb->rateset.count == 0)
			wlc_scbfree(wlc, scb);
	}
}

int
wlc_scb_rssi(struct scb *scb)
{
	int rssi = 0, cnt;
	int i;

	for (i = 0, cnt = 0; i < MA_WINDOW_SZ; i++)
		if (scb->rssi_window[i] != WLC_RSSI_INVALID)
		{
			rssi += scb->rssi_window[i];
			cnt++;
		}
	if (cnt > 1) rssi /= cnt;

	return (rssi);
}

void
wlc_scb_rssi_init(struct scb *scb, int rssi)
{
	int i;
	scb->rssi_enabled = 1;

	for (i = 0; i < MA_WINDOW_SZ; i++)
		scb->rssi_window[i] = rssi;

	scb->rssi_index = 0;
}

/* Enable or disable RSSI update for a particular requestor module */
bool
wlc_scb_rssi_update_enable(struct scb *scb, bool enable, scb_rssi_requestor_t rid)
{
	if (enable) {
		scb->rssi_upd |= (1<<rid);
	} else {
		scb->rssi_upd &= ~(1<<rid);
	}
	return (scb->rssi_upd != 0);
}


/* Give then tx_fn, return the feature id from txmod_fns array.
 * If tx_fn is NULL, 0 will be returned
 * If entry is not found, it's an ERROR!
 */
static INLINE scb_txmod_t
wlc_scb_txmod_fid(wlc_info_t *wlc, txmod_tx_fn_t tx_fn)
{
	scb_txmod_t txmod;

	for (txmod = TXMOD_START; txmod < TXMOD_LAST; txmod++)
		if (tx_fn == wlc->txmod_fns[txmod].tx_fn)
			return txmod;

	/* Should not reach here */
	ASSERT(txmod < TXMOD_LAST);
	return txmod;
}

#if defined(BCMDBG)
static int
wlc_scb_txpath_dump(wlc_info_t *wlc, struct scb *scb, struct bcmstrbuf *b)
{
	static const char *txmod_names[TXMOD_LAST] = {
		"Start",
		"DPT",
		"TDLS",
		"APPS",
		"Traffic Mgmt",
		"NAR",
		"A-MSDU",
		"A-MPDU",
		"Transmit"
	};
	scb_txmod_t fid, next_fid;

	bcm_bprintf(b, "     Tx Path: ");
	fid = TXMOD_START;
	do {
		next_fid = wlc_scb_txmod_fid(wlc, scb->tx_path[fid].next_tx_fn);
		/* for each txmod print out name and # total pkts held fr all scbs */
		bcm_bprintf(b, "-> %s (allscb pkts=%u)",
			txmod_names[next_fid],
			(wlc->txmod_fns[next_fid].pktcnt_fn) ?
			wlc_txmod_get_pkts_pending(wlc, next_fid) : -1);
		fid = next_fid;
	} while (fid != TXMOD_TRANSMIT && fid != 0);
	bcm_bprintf(b, "\n");
	return 0;
}
#endif 

/* Add a feature to the path. It should not be already on the path and should be configured
 * Does not take care of evicting anybody
 */
void
wlc_scb_txmod_activate(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid)
{
	/* Numeric value designating this feature's position in tx_path */
	static const uint8 txmod_position[TXMOD_LAST] = {
		0, /* TXMOD_START */
		1, /* TXMOD_DPT */
		1, /* TXMOD_TDLS */
		6, /* TXMOD_APPS */
		2, /* TXMOD_TRF_MGMT */
		3, /* TXMOD_NAR */
		4, /* TXMOD_AMSDU */
		5, /* TXMOD_AMPDU */
		7, /* TXMOD_TRANSMIT */
	};

	uint curr_mod_position;
	scb_txmod_t prev, next;
	txmod_info_t curr_mod_info = wlc->txmod_fns[fid];

	ASSERT(SCB_TXMOD_CONFIGURED(scb, fid) &&
	       !SCB_TXMOD_ACTIVE(scb, fid));

	curr_mod_position = txmod_position[fid];

	prev = TXMOD_START;

	while ((next = wlc_scb_txmod_fid(wlc, scb->tx_path[prev].next_tx_fn)) != 0 &&
	       txmod_position[next] < curr_mod_position)
		prev = next;

	/* next == 0 indicate this is the first addition to the path
	 * it HAS to be TXMOD_TRANSMIT as it's the one that puts the packet in
	 * txq. If this changes, then assert will need to be removed.
	 */
	ASSERT(next != 0 || fid == TXMOD_TRANSMIT);
	ASSERT(txmod_position[next] != curr_mod_position);

	SCB_TXMOD_SET(scb, prev, fid);
	SCB_TXMOD_SET(scb, fid, next);

	/* invoke any activate notify functions now that it's in the path */
	if (curr_mod_info.activate_notify_fn)
		curr_mod_info.activate_notify_fn(curr_mod_info.ctx, scb);
}

/* Remove a fid from the path. It should be already on the path
 * Does not take care of replacing it with any other feature.
 */
void
wlc_scb_txmod_deactivate(wlc_info_t *wlc, struct scb *scb, scb_txmod_t fid)
{
	scb_txmod_t prev, next;
	txmod_info_t curr_mod_info = wlc->txmod_fns[fid];

	/* If not active, do nothing */
	if (!SCB_TXMOD_ACTIVE(scb, fid))
		return;

	/* if deactivate notify function is present, call it */
	if (curr_mod_info.deactivate_notify_fn)
		curr_mod_info.deactivate_notify_fn(curr_mod_info.ctx, scb);

	prev = TXMOD_START;

	while ((next = wlc_scb_txmod_fid(wlc, scb->tx_path[prev].next_tx_fn))
	       != fid)
		prev = next;

	SCB_TXMOD_SET(scb, prev, wlc_scb_txmod_fid(wlc, scb->tx_path[fid].next_tx_fn));
	scb->tx_path[fid].next_tx_fn = NULL;
}

#ifdef WDS
int
wlc_wds_create(wlc_info_t *wlc, struct scb *scb, uint flags)
{
	ASSERT(scb != NULL);

	/* honor the existing WDS link */
	if (scb->wds != NULL) {
		if (!(flags & WDS_DYNAMIC))
			scb->permanent = TRUE;
		return BCME_OK;
	}

	if (!(flags & WDS_INFRA_BSS) && SCB_ISMYAP(scb)) {
#ifdef BCMDBG_ERR
		char eabuf[ETHER_ADDR_STR_LEN];
		WL_ERROR(("wl%d: rejecting WDS %s, associated to it as our AP\n",
		          wlc->pub->unit, bcm_ether_ntoa(&scb->ea, eabuf)));
#endif /* BCMDBG_ERR */
		return BCME_ERROR;
	}

	/* allocate a wlc_if_t for the wds interface and fill it out */
	scb->wds = wlc_wlcif_alloc(wlc, wlc->osh, WLC_IFTYPE_WDS, wlc->active_queue);
	if (scb->wds == NULL) {
		WL_ERROR(("wl%d: wlc_wds_create: failed to alloc wlcif\n",
		          wlc->pub->unit));
		return BCME_NOMEM;
	}
	scb->wds->u.scb = scb;

#ifdef AP
	/* create an upper-edge interface */
	if (!(flags & WDS_INFRA_BSS)) {
		/* a WDS scb has an AID for a unique WDS interface unit number */
		if (scb->aid == 0)
			scb->aid = wlc_bsscfg_newaid(scb->bsscfg);
		scb->wds->wlif = wl_add_if(wlc->wl, scb->wds, AID2PVBMAP(scb->aid), &scb->ea);
		if (scb->wds->wlif == NULL) {
			MFREE(wlc->osh, scb->wds, sizeof(wlc_if_t));
			scb->wds = NULL;
			return BCME_NOMEM;
		}
		scb->bsscfg->wlcif->flags |= WLC_IF_LINKED;
		wlc_if_event(wlc, WLC_E_IF_ADD, scb->wds);
	}

	wlc_wds_wpa_role_set(wlc->ap, scb, WL_WDS_WPA_ROLE_AUTO);
#endif /* AP */

	/* Dont do this for DWDS. */
	if (!(flags & WDS_DYNAMIC)) {
		/* override WDS nodes rates to the full hw rate set */
		wlc_rateset_filter(&wlc->band->hw_rateset, &scb->rateset, FALSE,
			WLC_RATES_CCK_OFDM, RATE_MASK, wlc_get_mcsallow(wlc, scb->bsscfg));
		wlc_scb_ratesel_init(wlc, scb);

		scb->permanent = TRUE;
		scb->flags &= ~SCB_MYAP;

		/* legacy WDS does 4-addr nulldata and 8021X frames */
		scb->flags3 |= SCB3_A4_NULLDATA;
		scb->flags3 |= SCB3_A4_8021X;
	}

	SCB_A4_DATA_ENABLE(scb);

	return BCME_OK;
}
#endif /* WDS */

#ifdef WL11AC
void
wlc_scb_update_oper_mode(wlc_info_t *wlc, struct scb *scb, uint8 oper_mode)
{
	uint8 type;

#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	if (oper_mode == scb->oper_mode && scb->oper_mode_enabled)
		return;

	type = DOT11_OPER_MODE_RXNSS_TYPE(oper_mode);

	if (type != 0)
		return;

	WL_INFORM(("wl%d: updating oper mode: old=%d, new=%d, for scb addr %s\n",
		WLCWLUNIT(wlc), scb->oper_mode, oper_mode,
		bcm_ether_ntoa(&scb->ea, eabuf)));

	scb->oper_mode = oper_mode;
	scb->oper_mode_enabled = TRUE;
	wlc_scb_ratesel_init(wlc, scb);
}
#endif /* WL11AC */
