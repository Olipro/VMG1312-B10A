/*
 * MBSS Feature portion of
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
 * $Id: wlc_mbss.c,v 1.6839 2011/01/17 22:33:04 Exp $
 */

#include <wlc_cfg.h>

#ifdef MBSS

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>

#include <siutils.h>

/* for htolxx() */
#include <bcmendian.h>

#include <wlioctl.h>
#include <epivers.h>

/* for dma32regs_t used in hnddma.h below */
#include <sbhnddma.h>
/* for dmatxfast to send tx status case */
#include <hnddma.h>

#include <wlc_pub.h>

/* for wsec/key related structures */
#include <wlc_key.h>

#include <wlc.h>
#include <wlc_bmac.h>
#include <wlc_apps.h>
#include <wlc_scb.h>

/* for wlc_stf_txcore_get() used in wlc_prb_resp_plcp_hdrs() */
#include <wlc_stf.h>

#include <wlc_security.h>
#include <wlc_ap.h>

#ifdef WLC_HIGH
/* to check scan_in_progress */
#include <wlc_scan.h>
#endif

/* for calloc */
#include <wlc_alloc.h>

#include <wlc_mbss.h>
#include <wlc_mbss_priv.h>

#if defined(WLC_HIGH) && defined(WLC_LOW)
#if defined(BCMDBG)
static void wlc_mbss_shm_ssid_get(wlc_info_t *wlc, int idx, wlc_ssid_t *ssid);
static void wlc_mbss_prq_entry_dump(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	uint16 rd_ptr, shm_mbss_prq_entry_t *entry);
static void wlc_mbss_prq_info_dump(wlc_info_t *wlc, wlc_prq_info_t *info);

#endif 
#endif /* defined(WLC_HIGH) && defined(WLC_LOW) */

#ifdef WLC_HIGH

/* IOVar table */
enum {
	IOV_MBSS,
	IOV_BSS_MAXASSOC,
	IOV_BCN_ROTATE, /* enable/disable beacon rotation */
	IOV_LAST		/* In case of a need to check max ID number */
};

/* MBSS IO Vars */
static const bcm_iovar_t wlc_mbss_iovars[] = {
	{"mbss", IOV_MBSS,
	(IOVF_SET_DOWN), IOVT_BOOL, 0,
	},
	{"bss_maxassoc", IOV_BSS_MAXASSOC,
	(IOVF_NTRL), IOVT_UINT32, 0
	},
	{"bcn_rotate", IOV_BCN_ROTATE,
	(0), IOVT_BOOL, 0
	},
	{NULL, 0, 0, 0, 0 }
};

static void wlc_mbss16_write_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
static void wlc_mbss16_setup(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

static void wlc_mbss_ssid_len_set(wlc_info_t *wlc, int idx, uint8 in_val);
static void wlc_mbss16_updssid(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

static int wlc_mbss_spt_init(struct wlc_info *wlc, wlc_spt_t *spt, int count, int len);
static void wlc_mbss_spt_deinit(struct wlc_info *wlc, wlc_spt_t *spt, int pkt_free_force);

static void wlc_mbss_ucode_set(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

static int wlc_bsscfg_macgen(wlc_info_t *wlc, wlc_bsscfg_t *cfg);

static int wlc_mbss_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg);

static void wlc_mbss_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg);

void wlc_mbss_update_bss_bcmc_fid(wlc_info_t *wlc);

static int
wlc_mbss_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);

#endif /* WLC_HIGH */

/* MBSSCAT_1************* module attach/detach related & iovar *************** */

/* module attach/detach */
struct wlc_mbss_info_t *
BCMATTACHFN(wlc_mbss_attach)(wlc_info_t *wlc)
{
	struct wlc_mbss_info_t *mbssinfo;

	if ((mbssinfo = (struct wlc_mbss_info_t *)
		MALLOC(wlc->osh, sizeof(struct wlc_mbss_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	bzero((char *)mbssinfo, sizeof(struct wlc_mbss_info_t));

	mbssinfo->wlc = wlc;

#ifdef WLC_HIGH
	/* register module */
	if (wlc_module_register(wlc->pub, wlc_mbss_iovars, "mbss", mbssinfo,
		wlc_mbss_doiovar, NULL, (up_fn_t)wlc_mbss_wlc_up, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	/* reserve cubby in the bsscfg container for per-bsscfg private data */
	if ((mbssinfo->cfgh = wlc_bsscfg_cubby_reserve(wlc, sizeof(wlc_mbss_bsscfg_cubby_t),
		wlc_mbss_bsscfg_init, wlc_mbss_bsscfg_deinit,
#ifdef BCMDBG
		(bsscfg_cubby_dump_t)wlc_mbss_per_bsscfg_dump,
#else
		NULL,
#endif
	    mbssinfo)) < 0) {
		WL_ERROR(("wl%d: %s: wlc_bsscfg_cubby_reserve() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}

	/* MBSS4 requires SW PRQ processing, which is not supported by wlc bmac driver */
	if (D11REV_ISMBSS4(wlc->pub->corerev)) {
		WL_ERROR(("bmac driver doesn't support MBSS4"));
		goto fail;
	}
#endif /* WLC_HIGH */

	/* This needs to be the same with the one in wlc_bmac.c */
	if (D11REV_ISMBSS16(wlc->pub->corerev)) {
		mbssinfo->max_ap_bss = wlc->pub->tunables->maxucodebss;

		/* 4313 has total fifo space of 128 blocks. if we enable
		 * all 16 MBSSs we will not be left with enough fifo space to
		 * support max thru'put. so we only allow configuring/enabling
		 * max of 4 BSSs. Rest of the space is distributed acorss
		 * the tx fifos.
		 */
#ifdef WLLPRS
		/* To support legacy prs of size > 256bytes, reduce the no. of
		 * bss supported to 8.
		 */
		if (D11REV_IS(wlc->pub->corerev, 16) || D11REV_IS(wlc->pub->corerev, 17) ||
			D11REV_IS(wlc->pub->corerev, 22)) {

			mbssinfo->max_ap_bss = 8;
			wlc->pub->tunables->maxucodebss = 8;
		}
#endif /* WLLPRS */

		if (D11REV_IS(wlc->pub->corerev, 25)) {
			mbssinfo->max_ap_bss = 4;
			wlc->pub->tunables->maxucodebss = 4;
		}

		mbssinfo->mbss_ucidx_mask = mbssinfo->max_ap_bss - 1;
	}

	return mbssinfo;

fail:
	/* error handling */
	wlc_mbss_detach(mbssinfo);

	return NULL;
}

void
BCMATTACHFN(wlc_mbss_detach)(struct wlc_mbss_info_t *mbssinfo)
{
	wlc_info_t *wlc;

	if (mbssinfo == NULL) {
		return;
	}

	wlc = mbssinfo->wlc;

	/* unregister module */
	wlc_module_unregister(wlc->pub, "mbss", mbssinfo);

	MFREE(wlc->osh, mbssinfo, sizeof(struct wlc_mbss_info_t));
}

#ifdef WLC_HIGH

static int
wlc_mbss_doiovar(void *context, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	struct wlc_mbss_info_t *mbss = (struct wlc_mbss_info_t *)context;
	wlc_info_t *wlc = mbss->wlc;
	wlc_pub_t *pub = wlc->pub;
	int32 *ret_int_ptr;
	int32 int_val = 0;
	bool bool_val;
	wlc_bsscfg_t *bsscfg;
	int err = 0;

	/* update bsscfg w/provided interface context */
	bsscfg = wlc_bsscfg_find_by_wlcif(wlc, wlcif);
	ASSERT(bsscfg != NULL);

	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;

	bool_val = (int_val != 0) ? TRUE : FALSE;

	switch (actionid) {
		case IOV_GVAL(IOV_MBSS):
			*ret_int_ptr = pub->_mbss ? TRUE : FALSE;
			break;

		case IOV_SVAL(IOV_MBSS): {
			bool curstat = (pub->_mbss != 0);
			bool rev16 = D11REV_ISMBSS16(pub->corerev) ? TRUE : FALSE;
			bool rev4 = D11REV_ISMBSS4(pub->corerev) ? TRUE : FALSE;

			/* No change requested */
			if (curstat == bool_val)
				break;

			if (!rev4 && !rev16) {
				break;
			}

			/* Reject if insufficient template memory */
			if (rev16 && (wlc_mbss_ucode_hwcap(wlc) == FALSE)) {
				uint blocks = 0;
				int stat = 0;
				WL_ERROR(("MBSS:insuff_res, trying to reduce BE tx fifo..\n"));

				/* get BE fifo, reduce some blocks */
				stat = wlc_bmac_xmtfifo_sz_get(wlc->hw, TX_AC_BE_FIFO, &blocks);
				if (stat != 0) {
					err = BCME_NORESOURCE;
					break;
				}
				blocks -= WLC_MBSS_BLKS_FOR_TEMPLATE;

				stat = wlc_bmac_xmtfifo_sz_set(wlc->hw, TX_AC_BE_FIFO,
					(uint16)blocks);
				if (stat != 0) {
					WL_ERROR(("MBSS:resources..NOTOK2\n"));
					err = BCME_NORESOURCE;
					break;
				}
				/* check if sufficient. If again NOT sufficient, return error */
				if (wlc_mbss_ucode_hwcap(wlc) == FALSE) {
					WL_ERROR(("MBSS:resources..NOTOK\n"));
					err = BCME_NORESOURCE;
					break;
				} else {
					WL_ERROR(("MBSS:resources..OK\n"));
				}
			}

			if (curstat) {
				/* if turning off mbss, disable extra bss configs */
				wlc_bsscfg_disablemulti(wlc);
				wlc_bmac_set_defmacintmask(wlc->hw, MI_PRQ, ~MI_PRQ);
				wlc_bmac_set_defmacintmask(wlc->hw, MI_DTIM_TBTT, ~MI_DTIM_TBTT);
				pub->_mbss = 0;
			}
			else {
				if (!rev16)
					wlc_bmac_set_defmacintmask(wlc->hw, MI_PRQ, MI_PRQ);
				wlc_bmac_set_defmacintmask(wlc->hw, MI_DTIM_TBTT, MI_DTIM_TBTT);
				pub->_mbss = rev4 ? MBSS4_ENABLED : MBSS16_ENABLED;
			}
#ifdef WLLPRS
			/* Enable/disable legacy prs support in ucode based on mbss
			 * state.
			 */
			wlc_mhf(wlc, MHF5, MHF5_LEGACY_PRS, (pub->_mbss ? MHF5_LEGACY_PRS : 0),
				WLC_BAND_ALL);
#endif /* WLLPRS */
			break;
		}
		case IOV_GVAL(IOV_BSS_MAXASSOC):
			*(uint32*)arg = bsscfg->maxassoc;
			break;

		case IOV_SVAL(IOV_BSS_MAXASSOC):
			if (int_val > wlc->pub->tunables->maxscb) {
				err = BCME_RANGE;
				goto exit;
			}
			bsscfg->maxassoc = int_val;
			break;
		case IOV_GVAL(IOV_BCN_ROTATE):
			if (mbss->beacon_bssidx < 0)
				*ret_int_ptr = 0;
			else
				*ret_int_ptr = 1;
			break;

		case IOV_SVAL(IOV_BCN_ROTATE):
			if (bool_val)
				mbss->beacon_bssidx = 0;
			else
				mbss->beacon_bssidx = -1;
			break;
		default:
			err = BCME_UNSUPPORTED;

	}
exit:
	return err;
}

/* MBSSCAT_1*************END module attach/detach related & iovar *** */

/* MBSSCAT_2************* module up/down related ********************* */

int
wlc_mbss_wlc_up(struct wlc_mbss_info_t *mbssinfo)
{
	int i = 0;

	if (MBSS_ENAB(mbssinfo->wlc->pub)) {
		/* Initialize the HW to SW BSS configuration index map */
		for (i = 0; i < WLC_MAXBSSCFG; i++) {
			mbssinfo->hw2sw_idx[i] = WLC_BSSCFG_IDX_INVALID;
		}

#if defined(WLC_HIGH) && defined(WLC_LOW)
		/* Initialize the cached PRQ base pointer */
			mbssinfo->prq_base = wlc_read_shm(mbssinfo->wlc, SHM_MBSS_PRQ_BASE);
			mbssinfo->prq_rd_ptr = mbssinfo->prq_base;
#endif /* WLC_HIGH && WLC_LOW */
	}
	return 0;
}
/* MBSSCAT_2************* module up/down related END ************************ */
/* MBSSCAT_3************* WLC related general ********************************************** */

void wlc_mbss_reset_macaddrs(struct wlc_mbss_info_t *mbssinfo)
{
	wlc_info_t *wlc = mbssinfo->wlc;
	wlc_bsscfg_t *bsscfg;
	int ii;

	if (MBSS_ENAB(wlc->pub)) {
		/* regardless of a clash, every time the user sets
		 * the primary config's cur_etheraddr, we will clear all
		 * all of the secondary config ethernet addresses.	If we
		 * don't do this, we'll have to prevent the user from
		 * configuring a MAC for the primary that collides(ucidx)
		 * with secondary configs.	this is way easier and is
		 * documented this way in the IOCTL/IOVAR manual.
		 */
		FOREACH_BSS(wlc, ii, bsscfg) {
			if (BSSCFG_AP(bsscfg))
				bcopy(&ether_null, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);
		}
	}

	/* also clear the base address for MBSS */
	bcopy(&ether_null, &mbssinfo->vether_base, ETHER_ADDR_LEN);
}

int wlc_mbss_validate_mac(struct wlc_mbss_info_t *mbssinfo,
	wlc_bsscfg_t *cfg, struct ether_addr *addr)
{
	wlc_info_t *wlc = mbssinfo->wlc;
	struct ether_addr temp;
	wlc_bsscfg_t *bsscfg;
	int ii;
	int ucidx;

	if (MBSS_ENAB(wlc->pub) && BSSCFG_AP(cfg)) {

		/* Has the primary config's address been set? */
		if (ETHER_ISNULLADDR(&wlc->cfg->cur_etheraddr))
			return BCME_BADADDR;

		if (ETHER_ISNULLADDR(&mbssinfo->vether_base)) {
			/* setting first VIF addr, start by checking
			 * for collision with primary config
			 */
			if (EADDR_TO_UC_IDX(*addr, mbssinfo->mbss_ucidx_mask) ==
			    EADDR_TO_UC_IDX(wlc->cfg->cur_etheraddr, mbssinfo->mbss_ucidx_mask))
				return BCME_BADADDR;

			/* Apply mask and save the base */
			bcopy(addr, &temp, ETHER_ADDR_LEN);
			temp.octet[5] &= ~(mbssinfo->mbss_ucidx_mask);
			bcopy(&temp, &mbssinfo->vether_base, ETHER_ADDR_LEN);
		}
		else {
			/* verify that the upper bits of the address
			 * match our base
			 */
			bcopy(addr, &temp, ETHER_ADDR_LEN);
			temp.octet[5] &= ~(mbssinfo->mbss_ucidx_mask);
			if (bcmp(&temp, &mbssinfo->vether_base, ETHER_ADDR_LEN))
				return BCME_BADADDR;

			/* verify that there isn't a
			 * collision with any other configs.
			 */
			ucidx = EADDR_TO_UC_IDX(*addr, mbssinfo->mbss_ucidx_mask);

			FOREACH_BSS(wlc, ii, bsscfg) {
				if ((bsscfg == cfg) ||
				    (ETHER_ISNULLADDR(&bsscfg->cur_etheraddr)))
					continue;
				if (ucidx == EADDR_TO_UC_IDX(bsscfg->cur_etheraddr,
					mbssinfo->mbss_ucidx_mask))
					return BCME_BADADDR;
			}

			/* make sure the index is in bound */
			if (MBSS_ENAB16(wlc->pub) &&
			    ((uint32)AP_BSS_UP_COUNT(wlc) >= mbssinfo->max_ap_bss))
				return BCME_BADADDR;
		}
	}
	return BCME_OK;
}

/*
 * Return true if packet got enqueued in BCMC PS packet queue.
 * This happens when the BSS is in transition from ON to OFF.
 * Called in prep_pdu and prep_sdu.
 */
bool
wlc_mbss_bcmc_pkt_q_check(wlc_info_t *wlc, struct scb *bcmc_scb, wlc_pkt_t pkt)
{
	if (!MBSS_ENAB(wlc->pub) || !SCB_PS(bcmc_scb) ||
		!(bcmc_scb->bsscfg->flags & WLC_BSSCFG_PS_OFF_TRANS)) {
		/* No need to enqueue pkt to PS queue */
		return FALSE;
	}

	/* BSS is in PS transition from ON to OFF; Enqueue frame on SCB's PSQ */
	if (wlc_apps_bcmc_ps_enqueue(wlc, bcmc_scb, pkt) < 0) {
		WL_PS(("wl%d: Failed to enqueue BC/MC pkt for BSS %d\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bcmc_scb->bsscfg)));
		PKTFREE(wlc->osh, pkt, TRUE);
	}
	/* Force caller to give up packet and not tx */
	return TRUE;
}

/* Under MBSS, this routine handles all TX dma done packets from the ATIM fifo. */
void
wlc_mbss_dotxstatus(wlc_info_t *wlc, tx_status_t *txs, void *pkt, uint16 fc,
	wlc_pkttag_t *pkttag, uint supr_status)
{
	wlc_bsscfg_t *bsscfg = NULL;
	int bss_idx;
	bool free_pkt = FALSE;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby;

	bss_idx = (int)(WLPKTTAG_BSSIDX_GET(pkttag));
#if defined(BCMDBG)     /* Verify it's a reasonable index */
	if ((bss_idx < 0) || (bss_idx >= WLC_MAXBSSCFG) ||
	    (wlc->bsscfg[bss_idx] == NULL)) {
		WL_ERROR(("%s: bad BSS idx\n", __FUNCTION__));
		ASSERT(!"MBSS dotxstatus: bad BSS idx\n");
	}
#endif /* BCMDBG */

	/* For probe resp, this is really only used for counters */
	bsscfg = wlc->bsscfg[bss_idx];
	ASSERT(bsscfg != NULL);
	pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	/* Being in the ATIM fifo, it must be a beacon or probe response */
	switch (fc & FC_KIND_MASK) {
	case FC_PROBE_RESP:
		/* Requeue suppressed probe response if due to TBTT */
		if (supr_status == TX_STATUS_SUPR_TBTT) {
			int txerr;
			WLCNTINCR(pcfgcubby->cnt->prb_resp_retrx);
#ifdef WLC_HIGH_ONLY
			if (RPCTX_ENAB(wlc->pub))
				txerr = wlc_rpctx_tx(wlc->rpctx, TX_ATIM_FIFO, pkt, TRUE, -1, 1);
			else
#endif /* WLC_HIGH_ONLY */
				txerr = dma_txfast(WLC_HW_DI(wlc, TX_ATIM_FIFO), pkt, TRUE);

			if (txerr < 0) {
				WL_MBSS(("Failed to retransmit suppressed probe resp for bss %d\n",
					WLC_BSSCFG_IDX(bsscfg)));
				WLCNTINCR(pcfgcubby->cnt->prb_resp_retrx_fail);
				free_pkt = TRUE;
			}
		} else {
			free_pkt = TRUE;
			if (supr_status == TX_STATUS_SUPR_EXPTIME) {
				WLCNTINCR(pcfgcubby->cnt->prb_resp_ttl_expy);
			}
		}
		break;
	case FC_BEACON:
		if (supr_status)
			WL_ERROR(("%s: Suppressed Beacon frame = 0x%x\n", __FUNCTION__,
			          supr_status));

		if (WLPKTFLAG_BSS_DOWN_GET(pkttag)) { /* Free the pkt since BSS is gone */
			WL_MBSS(("BSSCFG down on bcn done\n"));
			WL_ERROR(("%s: in_use_bitmap = 0x%x pkt: %p\n", __FUNCTION__,
			          pcfgcubby->bcn_template->in_use_bitmap, pkt));
			free_pkt = TRUE;
			break; /* All done */
		}
		ASSERT(bsscfg->up);
		/* Assume only one beacon in use at a time */
		pcfgcubby->bcn_template->in_use_bitmap = 0;
#if defined(WLC_SPT_DEBUG) && defined(BCMDBG)
		if (supr_status != 0) {
			pcfgcubby->bcn_template->suppressed++;
		}
#endif /* WLC_STP_DEBUG && BCMDBG */
		break;
	default: /* Bad packet type for ATIM fifo */
		ASSERT(!"TX done ATIM packet neither BCN or PRB");
		break;
	}

	if (supr_status != 0) {
		WLCNTINCR(wlc->pub->_cnt->atim_suppress_count);
	}

	if (free_pkt) {
		PKTFREE(wlc->osh, pkt, TRUE);
	}
}

void
wlc_mbss_dotxstatus_bcmc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint16 frameid)
{
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	pcfgcubby->mc_fifo_pkts--; /* Decrement mc fifo counter */
	/* Check if this was last frame uCode knew about */
	if (bsscfg->bcmc_fid_shm == frameid) {
		bsscfg->bcmc_fid_shm = INVALIDFID;
		if ((bsscfg->flags & WLC_BSSCFG_PS_OFF_TRANS) &&
			(bsscfg->bcmc_fid == INVALIDFID)) {
		/* Mark transition complete as pkts out of BCMC fifo */
			wlc_mbss_apps_bss_ps_off_done(wlc, bsscfg);
		}
	}
}

/* Use to set a specific SSID length */
static void
wlc_mbss_ssid_len_set(wlc_info_t *wlc, int idx, uint8 in_val)
{
	uint16 tmp_val;
	tmp_val = wlc_read_shm(wlc, _MBSS_SSID_LEN_SELECT(wlc, idx));
	_MBSS_SSID_LEN_SET(idx, tmp_val, in_val);
	wlc_write_shm(wlc, _MBSS_SSID_LEN_SELECT(wlc, idx), tmp_val);
}

int
wlc_mbss_shm_ssid_upd(struct wlc_mbss_info_t *mbss, wlc_bsscfg_t *cfg, uint16 *base)
{
	wlc_info_t *wlc = mbss->wlc;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);

	if (MBSS_ENAB(wlc->pub)) { /* Update based on uCode index of BSS */
		int uc_idx;

		if (MBSS_ENAB16(wlc->pub)) {
			wlc_mbss16_updssid(wlc, cfg);
			/* tell ucode where to find the probe responses */
			if (D11REV_GE(wlc->pub->corerev, 16))
				/* for corerev >= 16 the value is in multiple of 4 */
				wlc_write_shm(wlc, SHM_MBSS_PRS_TPLPTR,
					(MBSS_PRS_BLKS_START(mbss->max_ap_bss) >> 2));
			else
				wlc_write_shm(wlc, SHM_MBSS_PRS_TPLPTR,
					MBSS_PRS_BLKS_START(mbss->max_ap_bss));

			wlc_write_shm(wlc, SHM_MBSS_BC_FID1, mbss->mbss_ucidx_mask);
			return 0;
		}
		uc_idx = WLC_MBSS_BSSCFG_UCIDX(pcfgcubby);
		*base = SHM_MBSS_SSID_ADDR(uc_idx); /* Update base addr for ssid */
		wlc_mbss_ssid_len_set(wlc, uc_idx, cfg->SSID_len);
	}
	return 1;
}

/* MBSS4 MI_TBTT and MI_DTIM_TBTT handler */
int
wlc_mbss4_tbtt(wlc_info_t *wlc, uint32 macintstatus)
{
	int i, idx;
	wlc_bsscfg_t *cfg;
	uint16 beacon_count = 0;
	wlc_pkt_t pkt;
	bool dtim;
	uint32 delay;
#ifdef RADIO_PWRSAVE
	wlc_radio_pwrsave_t* radio_pwrsave = wlc_radio_pwrsave_get_rps_handle(wlc->ap);
#endif /* RADIO_PWRSAVE */
	struct wlc_mbss_info_t *mbss = wlc->mbss;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby;

	dtim = ((macintstatus & MI_DTIM_TBTT) != 0);
	/* Update our dtim count and determine which BSS config will beacon first */
	if (dtim) {
		mbss->cur_dtim_count = 0x00;
		/* disabled beacon rotate when beacon_bssidx < 0  */
		if (mbss->beacon_bssidx >= 0) {
			/* Bump the "starting" bss index up for bss rotation */
			for (i = 0; i < WLC_MAXBSSCFG; i++) {
				if (++mbss->beacon_bssidx >= WLC_MAXBSSCFG) {
					mbss->beacon_bssidx = 0;
				}
				if (BSS_BEACONING(wlc->bsscfg[mbss->beacon_bssidx])) {
					/* Found the next beaconing BSS index; break */
					break;
				}
			}
		}
	}
	else  {
		if (mbss->cur_dtim_count)
			mbss->cur_dtim_count--;
		else
			mbss->cur_dtim_count = wlc->default_bss->dtim_period - 1;
	}

	/* If we've taken too long to get beacons ready, don't bother queuing them */
	delay = (R_REG(wlc->osh, &wlc->regs->tsf_timerlow) -
	         mbss->last_tbtt_us);
	if (delay > wlc->ap->pre_tbtt_max_lat_us) {
		WLCNTINCR(wlc->pub->_cnt->late_tbtt_dpc);
		WL_MBSS(("wl%d: ERROR: TBTT latency: %u; skipping bcn\n", wlc->pub->unit, delay));
		return 0;
	}

#ifdef RADIO_PWRSAVE
	/* just out of pwr save with a need to skip bcn in level 1/2. */
	if (!dtim && radio_pwrsave->cncl_bcn) {
		WL_INFORM(("wl%d: radio pwrsave skipping bcn.\n", wlc->pub->unit));
		return 0;
	}
#endif /* RADIO_PWRSAVE */

	if (SCAN_IN_PROGRESS(wlc->scan)) {
		WL_MBSS(("wl%d: WARNING: MBSS Not beaconing due to scan in progress.\n",
		         wlc->pub->unit));
		return 0;
	}

	for (i = 0; i < WLC_MAXBSSCFG; i++) {
		idx = i;
		if (mbss->beacon_bssidx >= 0)
			idx = (i + mbss->beacon_bssidx) % WLC_MAXBSSCFG;
		cfg = wlc->bsscfg[idx];
		if (!BSS_BEACONING(cfg)) {
			continue; /* Skip cfgs not present or not AP or not up */
		}
		pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);

		ASSERT(pcfgcubby->bcn_template->latest_idx >= 0);
		ASSERT(pcfgcubby->bcn_template->latest_idx < WLC_SPT_COUNT_MAX);
		if (pcfgcubby->bcn_template->in_use_bitmap != 0) {
			WLCNTINCR(wlc->pub->_cnt->bcn_template_not_ready);
#if defined(BCMDBG_MBSS_PROFILE)
			if (pcfgcubby->bcn_tx_done) {
				WLCNTINCR(wlc->pub->_cnt->bcn_template_not_ready_done);
			}
#endif
			continue;
		}

		pkt = SPT_LATEST_PKT(pcfgcubby->bcn_template);
		ASSERT(pkt != NULL);

		/* Update DTIM count */
		BCN_TIM_DTIM_COUNT_SET(pcfgcubby->bcn_template->tim_ie, mbss->cur_dtim_count);

		/*
		 * Update BCMC flag in the beacon.
		 * At this point, the driver has not yet written the last FID SHM locations;
		 * so either bcmc_fid or bcmc_fid_shm may be indicate pkts in the BC/MC fifo
		 */
		if (BCMC_PKTS_QUEUED(cfg)) {
			BCN_TIM_BCMC_FLAG_SET(pcfgcubby->bcn_template->tim_ie);
		} else {
			BCN_TIM_BCMC_FLAG_RESET(pcfgcubby->bcn_template->tim_ie);
		}

		if (dma_txfast(WLC_HW_DI(wlc, TX_ATIM_FIFO), pkt, TRUE) < 0) {
			WLCNTINCR(pcfgcubby->cnt->bcn_tx_failed);
		} else {
			++beacon_count;
			pcfgcubby->bcn_template->in_use_bitmap |=
				(1 << pcfgcubby->bcn_template->latest_idx);
#if defined(BCMDBG_MBSS_PROFILE)
			pcfgcubby->bcn_tx_done = FALSE;
#endif /* BCMDBG_MBSS_PROFILE */
#if defined(WLC_SPT_DEBUG)
			pcfgcubby->bcn_template->tx_count++;
#endif /* WLC_SPT_DEBUG */
		}
	}
	wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, beacon_count);

	if (dtim) {
		wlc_mbss_update_bss_bcmc_fid(wlc);
	}

	return 0;
}

/* MBSS16 MI_TBTT and MI_DTIM_TBTT handler */
int
wlc_mbss16_tbtt(wlc_info_t *wlc, uint32 macintstatus)
{
	bool dtim;
	int cfgidx;
	int ucidx;
	wlc_bsscfg_t *cfg = NULL;
	uint16 beacon_count = 0;
	uint16 dtim_map = 0;
#ifdef RADIO_PWRSAVE
	wlc_radio_pwrsave_t* radio_pwrsave = wlc_radio_pwrsave_get_rps_handle(wlc->ap);
#endif /* RADIO_PWRSAVE */
	wlc_mbss_bsscfg_cubby_t *pcfgcubby;

#ifndef WLC_HIGH_ONLY
	{
		uint32 delay;
		struct wlc_mbss_info_t *mbss = wlc->mbss;

		/* If we've taken too long to get ready, skip */
		delay = (R_REG(wlc->osh, &wlc->regs->tsf_timerlow) - mbss->last_tbtt_us);
		if (delay > wlc->ap->pre_tbtt_max_lat_us) {
			WLCNTINCR(wlc->pub->_cnt->late_tbtt_dpc);
			WL_MBSS(("wl%d: ERROR: TBTT latency: %u; skipping bcn update\n",
			         wlc->pub->unit, delay));
			/* No beacons: didn't update */
			wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, 0);
			return 0;
		}
	}
#endif /* WLC_HIGH_ONLY */
	dtim = ((macintstatus & MI_DTIM_TBTT) != 0);

#ifdef RADIO_PWRSAVE
	if (!dtim && radio_pwrsave->cncl_bcn) {
		wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, 0);
		WL_INFORM(("wl%d: radio pwrsave skipping bcn.\n", wlc->pub->unit));
		return 0;
	}
#endif /* RADIO_PWRSAVE */

	/* Traverse the bsscfg's
	 * create a count of "active" bss's
	 *
	 * if we're at a DTIM:
	 * create a DTIM map,  push "last" bc/mc fid's to shm
	 *
	 * if a beacon has been modified push to shm
	 */
	for (cfgidx = 0; cfgidx < WLC_MAXBSSCFG; cfgidx++) {
		cfg = wlc->bsscfg[cfgidx];
		if (!BSS_BEACONING(cfg))
			continue;
		pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, cfg);

		ASSERT(pcfgcubby->bcn_template->latest_idx >= 0);
		ASSERT(pcfgcubby->bcn_template->latest_idx < WLC_SPT_COUNT_MAX);

		++beacon_count;

		ucidx = WLC_MBSS_BSSCFG_UCIDX(pcfgcubby);
		ASSERT(ucidx != WLC_BSSCFG_IDX_INVALID);
		/* Update BCMC flag in the beacon. */
		if (dtim && (cfg->bcmc_fid != INVALIDFID)) {
			uint fid_addr;

			dtim_map |= NBITVAL(ucidx);
			fid_addr = SHM_MBSS_BC_FID_ADDR16(ucidx);
			wlc_write_shm((wlc), fid_addr, cfg->bcmc_fid);
			BCMC_FID_SHM_COMMIT(cfg);
		}
		/* Update the HW beacon template */
		wlc_mbss16_write_beacon(wlc, cfg);

	} /* cfgidx loop */

	wlc_write_shm(wlc, SHM_MBSS_BCN_COUNT, beacon_count);
	wlc_write_shm(wlc, SHM_MBSS_BSSID_NUM, beacon_count);

	if (dtim)
		wlc_write_shm(wlc, SHM_MBSS_BC_BITMAP, dtim_map);

	return 0;
}

/* Write the BSSCFG's probe response template into HW, suspend MAC if requested */
void
wlc_mbss16_write_prbrsp(wlc_info_t *wlc, wlc_bsscfg_t *cfg, bool suspend)
{
	wlc_pkt_t pkt;
	uint32 ucidx;
	int start;
	uint16 len;
	uint8 * pt;
	struct wlc_mbss_info_t *mbss = wlc->mbss;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);

	ucidx = WLC_MBSS_BSSCFG_UCIDX(pcfgcubby);
	ASSERT(ucidx != (uint32)WLC_BSSCFG_IDX_INVALID);

	pkt = pcfgcubby->probe_template;
	ASSERT(pkt != NULL);

	WL_MBSS(("%s: wl%d.%d %smodified %d\n", __FUNCTION__, wlc->pub->unit, WLC_BSSCFG_IDX(cfg),
	         suspend ? "w/suspend " : "", pcfgcubby->prb_modified));

	/* probe response */
	if (pcfgcubby->prb_modified == TRUE) {
		if (suspend)
			wlc_suspend_mac_and_wait(wlc);

		start = MBSS_PRS_BLKS_START(mbss->max_ap_bss) + (ucidx * BCN_TMPL_LEN);

		pt = ((uint8 *)(PKTDATA(wlc->osh, pkt)) + D11_TXH_LEN);
		len = PKTLEN(wlc->osh, pkt) - D11_TXH_LEN;

		ASSERT(len <= BCN_TMPL_LEN);

		wlc_write_template_ram(wlc, start, (len + 3) & (~3), pt);
		/* probe response len */
		wlc_write_shm(wlc, SHM_MBSS_PRSLEN0 + (2 * ucidx), len);

#ifdef WLLPRS
		if (N_ENAB(wlc->pub)) {
			wlc_pkt_t prspkt;
			uint16 lgcyprs_len_ptr;

			prspkt = pcfgcubby->lprs_template;
			ASSERT(prspkt != NULL);

			/* 11g probe resp, which follows the ht probe resp */
			start = MBSS_PRS_BLKS_START(mbss->max_ap_bss) +
				(mbss->max_ap_bss * BCN_TMPL_LEN) + (ucidx * LPRS_TMPL_LEN);

			pt = ((uint8 *)(PKTDATA(wlc->osh, prspkt)) + D11_TXH_LEN);
			len = PKTLEN(wlc->osh, prspkt) - D11_TXH_LEN;

			ASSERT(len <= LPRS_TMPL_LEN);

			wlc_write_template_ram(wlc, start, (len + 3) & (~3), pt);

			lgcyprs_len_ptr = wlc_read_shm(wlc, SHM_MBSS_BC_FID3);

			wlc_write_shm(wlc, ((lgcyprs_len_ptr + ucidx) * 2), len);
		}
#endif /* WLLPRS */

		pcfgcubby->prb_modified = FALSE;

		if (suspend)
			wlc_enable_mac(wlc);
	}
}

/* Write the BSSCFG's beacon template into HW */
static void
wlc_mbss16_write_beacon(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	wlc_pkt_t pkt;
	uint32 ucidx;
	int start;
	uint16 len;
	uint8 * pt;
	uint shm_bcn0_tpl_base;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, cfg);

	ucidx = WLC_MBSS_BSSCFG_UCIDX(pcfgcubby);
	ASSERT(ucidx != (uint32)WLC_BSSCFG_IDX_INVALID);

	ASSERT(pcfgcubby->bcn_template->latest_idx >= 0);
	ASSERT(pcfgcubby->bcn_template->latest_idx < WLC_SPT_COUNT_MAX);

	pkt = SPT_LATEST_PKT(pcfgcubby->bcn_template);
	ASSERT(pkt != NULL);

	if (D11REV_GE(wlc->pub->corerev, 40)) {
		shm_bcn0_tpl_base = D11AC_T_BCN0_TPL_BASE;
	else
		shm_bcn0_tpl_base = D11_T_BCN0_TPL_BASE;

	/* beacon */
	if (pcfgcubby->bcn_template->bcn_modified == TRUE) {

		WL_MBSS(("%s: wl%d.%d\n", __FUNCTION__, wlc->pub->unit, WLC_BSSCFG_IDX(cfg)));

		start = shm_bcn0_tpl_base + (ucidx * BCN_TMPL_LEN);
		pt = ((uint8 *)(PKTDATA(wlc->osh, pkt)) + D11_TXH_LEN);
		len = PKTLEN(wlc->osh, pkt) - D11_TXH_LEN;

		ASSERT(len <= BCN_TMPL_LEN);

		wlc_write_template_ram(wlc, start, (len + 3) & (~3), pt);

		/* bcn len */
		wlc_write_shm(wlc, SHM_MBSS_BCNLEN0 + (2 * ucidx), len);
		wlc_mbss16_setup(wlc, cfg);
		pcfgcubby->bcn_template->bcn_modified = FALSE;
	}
}


static void
wlc_mbss16_updssid(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint32 start;
	uint16 val;
	uint32 ssidlen = cfg->SSID_len;
	uint32 swplen;
	uint8 ssidbuf[DOT11_MAX_SSID_LEN];
	struct wlc_mbss_info_t *mbss = wlc->mbss;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, cfg);
	int8 ucidx = WLC_MBSS_BSSCFG_UCIDX(pcfgcubby);

	ASSERT((ucidx >= 0) && (ucidx <= mbss->mbss_ucidx_mask));

	UNUSED_PARAMETER(mbss);

	/* push ssid, ssidlen out to ucode Search Engine */
	start = SHM_MBSS_SSIDSE_BASE_ADDR + (ucidx * SHM_MBSS_SSIDSE_BLKSZ);
	/* search mem length field is always little endian */
	swplen = htol32(ssidlen);
	/* invent new function like wlc_write_shm using OBJADDR_SRCHM_SEL */
	wlc_bmac_copyto_objmem(wlc->hw, start, &swplen, SHM_MBSS_SSIDLEN_BLKSZ, OBJADDR_SRCHM_SEL);

	bzero(ssidbuf, DOT11_MAX_SSID_LEN);
	bcopy(cfg->SSID, ssidbuf, cfg->SSID_len);

	start += SHM_MBSS_SSIDLEN_BLKSZ;
	wlc_bmac_copyto_objmem(wlc->hw, start, ssidbuf, SHM_MBSS_SSID_BLKSZ, OBJADDR_SRCHM_SEL);


	start = SHM_MBSS_SSIDLEN0 + (ucidx & 0xFE);
	val = wlc_read_shm(wlc, start);
	/* set bit indicating closed net if appropriate */
	if (cfg->closednet_nobcnssid)
		ssidlen |= SHM_MBSS_CLOSED_NET;

	if (ucidx & 0x01) {
		val &= 0xff;
		val |= ((uint8)ssidlen << 8);

	} else {
		val &= 0xff00;
		val |= (uint8)ssidlen;
	}

	wlc_write_shm(wlc, start, val);
}

static void
wlc_mbss16_setup(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	uint8 *bcn;
	void *pkt;
	uint16 tim_offset;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, cfg);

	/* find the TIM elt offset in the bcn template, push to shm */
	pkt = SPT_LATEST_PKT(pcfgcubby->bcn_template);
	bcn = (uint8 *)(PKTDATA(wlc->osh, pkt));
	tim_offset = (uint16)(pcfgcubby->bcn_template->tim_ie - bcn);
	/* we want it less the actual ssid length */
	tim_offset -= cfg->SSID_len;
	/* and less the D11_TXH_LEN too */
	tim_offset -= D11_TXH_LEN;

	wlc_write_shm(wlc, M_TIMBPOS_INBEACON, tim_offset);
}


/* BCMC_FID_SHM_COMMIT - Committing FID to SHM; move driver's value to bcmc_fid_shm */
void
wlc_mbss_bcmc_fid_shm_commit(wlc_bsscfg_t *bsscfg)
{
	bsscfg->bcmc_fid_shm = bsscfg->bcmc_fid;
	bsscfg->bcmc_fid = INVALIDFID;
}

/* Write the base MAC/BSSID into shared memory.  For MBSS, the MAC and BSSID
 * are required to be the same.
 */
int
wlc_write_mbss_basemac(struct wlc_mbss_info_t *mbss)
{
	uint16 mac_l;
	uint16 mac_m;
	uint16 mac_h;
	const struct ether_addr *addr = &mbss->vether_base;
	wlc_info_t *wlc = mbss->wlc;

	mac_l = addr->octet[0] | (addr->octet[1] << 8);
	mac_m = addr->octet[2] | (addr->octet[3] << 8);
	/* Mask low bits of BSSID base */
	mac_h = addr->octet[4] | ((addr->octet[5] & ~(mbss->mbss_ucidx_mask)) << 8);

	wlc_write_shm(wlc, SHM_MBSS_BSSID0, mac_l);
	wlc_write_shm(wlc, SHM_MBSS_BSSID1, mac_m);
	wlc_write_shm(wlc, SHM_MBSS_BSSID2, mac_h);

	return BCME_OK;
}

bool
wlc_mbss_ucode_hwcap(wlc_info_t *wlc)
{
	/* add up template space here */
	int templ_ram_sz, fifo_mem_used, i, stat;
	uint blocks = 0;
	struct wlc_mbss_info_t *mbss = wlc->mbss;

	for (fifo_mem_used = 0, i = 0; i < NFIFO; i++) {
		stat = wlc_bmac_xmtfifo_sz_get(wlc->hw, i, &blocks);
		if (stat != 0) return FALSE;
		fifo_mem_used += blocks;
	}

#ifdef WLC_HIGH
	templ_ram_sz = ((wlc->machwcap & MCAP_TXFSZ_MASK) >> MCAP_TXFSZ_SHIFT) * 2;
#else
	templ_ram_sz = ((wlc->hw->machwcap & MCAP_TXFSZ_MASK) >> MCAP_TXFSZ_SHIFT) * 2;
#endif
#ifdef WLC_HIGH
	if ((templ_ram_sz - fifo_mem_used) < (int)MBSS_TPLBLKS(mbss->max_ap_bss))
#else
	if ((templ_ram_sz - fifo_mem_used) < (int)MBSS_TPLBLKS(wlc->pub->tunables->maxucodebss))
#endif
	{
		WL_ERROR(("wl%d: %s: Insuff mem for MBSS: templ memblks %d fifo memblks %d\n",
			wlc->pub->unit, __FUNCTION__, templ_ram_sz, fifo_mem_used));
		return FALSE;
	}

	return TRUE;
}


uint16
wlc_mbss_get_fifostartblk(wlc_info_t *wlc)
{
	struct wlc_mbss_info_t *mbss = wlc->mbss;
	ASSERT(mbss->max_ap_bss > 0);
	return MBSS_TXFIFO_START_BLK(mbss->max_ap_bss);
}


/* Record latest TBTT/DTIM interrupt time for latency calc */
void
wlc_mbss_update_last_tbtt(wlc_info_t *wlc, uint32 tbttval)
{
	struct wlc_mbss_info_t *mbss = wlc->mbss;
	mbss->last_tbtt_us = tbttval;
}


/* MBSSCAT_3************* WLC related general END ********************************************** */


/* MBSSCAT_4************* bsscfg related ********************************************** */


int
wlc_mbss_bsscfg_up(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int result = BCME_OK;
	int idx;
	wlc_bsscfg_t *bsscfg;
	struct wlc_mbss_info_t *mbss = wlc->mbss;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);

	/* Assumes MBSS is enabled for this BSS config herein */

	/* Set pre TBTT interrupt timer to 10 ms for now; will be shorter */
	wlc_write_shm(wlc, SHM_MBSS_PRE_TBTT, wlc->ap->pre_tbtt_us);

	/* if the BSS configs hasn't been given a user defined address or
	 * the address is duplicated, we'll generate our own.
	 */
	FOREACH_BSS(wlc, idx, bsscfg) {
		if (bsscfg == cfg)
			continue;
		if (bcmp(&bsscfg->cur_etheraddr, &cfg->cur_etheraddr, ETHER_ADDR_LEN) == 0)
			break;
	}
	if (ETHER_ISNULLADDR(&cfg->cur_etheraddr) || idx < WLC_MAXBSSCFG) {
		result = wlc_bsscfg_macgen(wlc, cfg);
		if (result) {
			WL_ERROR(("wl%d.%d: %s: unable to generate MAC address\n",
				wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
			goto end;
		}
	}

	/* Set the uCode index of this config */
	pcfgcubby->_ucidx = EADDR_TO_UC_IDX(cfg->cur_etheraddr, mbss->mbss_ucidx_mask);
	ASSERT(pcfgcubby->_ucidx <= mbss->mbss_ucidx_mask);
	mbss->hw2sw_idx[pcfgcubby->_ucidx] = WLC_BSSCFG_IDX(cfg);

	/* Allocate DMA space for beacon software template */
	result = wlc_mbss_spt_init(wlc, pcfgcubby->bcn_template, BCN_TEMPLATE_COUNT, BCN_TMPL_LEN);
	if (result < 0) {
		WL_ERROR(("wl%d.%d: %s: unable to allocate beacon templates",
			wlc->pub->unit, WLC_BSSCFG_IDX(cfg), __FUNCTION__));
		goto end;
	}
	/* Set the BSSCFG index in the packet tag for beacons */
	for (idx = 0; idx < BCN_TEMPLATE_COUNT; idx++) {
		WLPKTTAGBSSCFGSET(pcfgcubby->bcn_template->pkts[idx], WLC_BSSCFG_IDX(cfg));
	}

	/* Make sure that our SSID is in the correct uCode
	 * SSID slot in shared memory
	 */
	wlc_shm_ssid_upd(wlc, cfg);

	BCMC_FID_INIT(cfg);

	if (!MBSS_ENAB16(wlc->pub)) {
		cfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB);
		cfg->flags |= (WLC_BSSCFG_SW_BCN | WLC_BSSCFG_SW_PRB);
	} else {
		cfg->flags &= ~(WLC_BSSCFG_HW_BCN | WLC_BSSCFG_HW_PRB |
			WLC_BSSCFG_SW_BCN | WLC_BSSCFG_SW_PRB);
		cfg->flags |= (WLC_BSSCFG_MBSS16);
	}

	wlc_write_mbss_basemac(wlc->mbss);

	wlc_mbss_ucode_set(wlc, cfg);

	/* replace updated ether addr for BSSID */
	bcopy((char*)&cfg->cur_etheraddr, (char*)&cfg->target_bss->BSSID, ETHER_ADDR_LEN);

end:
	return result;
}

void wlc_mbss_bsscfg_down(struct wlc_mbss_info_t *mbss, wlc_bsscfg_t *cfg)
{
	wlc_info_t *wlc = mbss->wlc;
	uint i, clear_len = FALSE;
	wlc_bsscfg_t *bsscfg;
	uint8 ssidlen = cfg->SSID_len;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);

	wlc_mbss_spt_deinit(wlc, pcfgcubby->bcn_template, FALSE);

	if (pcfgcubby->probe_template != NULL) {
		PKTFREE(wlc->osh, pcfgcubby->probe_template, TRUE);
		pcfgcubby->probe_template = NULL;
	}

#ifdef WLLPRS
	if (pcfgcubby->lprs_template != NULL) {
		PKTFREE(wlc->osh, pcfgcubby->lprs_template, TRUE);
		pcfgcubby->lprs_template = NULL;
	}
#endif /* WLLPRS */

	/* If we clear ssid length of all bsscfgs while doing
	 * a wl down the ucode can get into a state where it
	 * will keep searching	for non-zero ssid length thereby
	 * causing mac_suspend_and_wait messages. To avoid that
	 * we will prevent clearing the ssid len of at least one BSS
	 */
	FOREACH_BSS(wlc, i, bsscfg) {
		if (bsscfg->up) {
			clear_len = TRUE;
			break;
		}
	}
	if (clear_len) {
		cfg->SSID_len = 0;

		/* update uCode shared memory */
		wlc_shm_ssid_upd(wlc, cfg);
		cfg->SSID_len = ssidlen;

		/* Clear the HW index */
		mbss->hw2sw_idx[pcfgcubby->_ucidx] = WLC_BSSCFG_IDX_INVALID;
	}

}


int
wlc_mbss_bsscfg_enable(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	int ret = BCME_OK;

	/* make sure we don't exceed max */
	if ((uint32)AP_BSS_UP_COUNT(wlc) >= wlc->mbss->max_ap_bss) {
		bsscfg->enable = FALSE;
		WL_ERROR(("wl%d: max %d ap bss allowed\n",
			wlc->pub->unit, wlc->mbss->max_ap_bss));
		ret = BCME_ERROR;
	}
	return ret;
}

static int
wlc_mbss_bsscfg_init(void *ctx, wlc_bsscfg_t *cfg)
{
	struct wlc_mbss_info_t *mbss = (struct wlc_mbss_info_t *)ctx;
	wlc_info_t *wlc = mbss->wlc;
	uint unit;
	osl_t *osh;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);
	int status = BCME_OK;

	osh = wlc->osh;
	unit = wlc->pub->unit;

	if ((pcfgcubby->bcn_template = (wlc_spt_t *)
	     wlc_calloc(osh, unit, sizeof(wlc_spt_t))) == NULL) {
		status = BCME_NORESOURCE;
		goto fail;
	}

#if defined(WLCNT)
	if ((pcfgcubby->cnt = (wlc_mbss_cnt_t *)
		 wlc_calloc(osh, unit, sizeof(wlc_mbss_cnt_t))) == NULL) {
		status = BCME_NORESOURCE;
		goto fail;
		}
#endif

fail:
	return status;
}

static void
wlc_mbss_bsscfg_deinit(void *ctx, wlc_bsscfg_t *cfg)
{
	struct wlc_mbss_info_t *mbss = (struct wlc_mbss_info_t *)ctx;
	osl_t *osh;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);

	osh = mbss->wlc->osh;

	if (pcfgcubby->bcn_template) {
		MFREE(osh, pcfgcubby->bcn_template, sizeof(wlc_spt_t));
		pcfgcubby->bcn_template = NULL;
	}

#if defined(WLCNT)
	if (pcfgcubby->cnt) {
		MFREE(osh, pcfgcubby->cnt, sizeof(wlc_mbss_cnt_t));
		pcfgcubby->cnt = NULL;
	}
#endif
}


/*
 * Allocate and set up a software packet template
 * @param count The number of packets to use; must be <= WLC_SPT_COUNT_MAX
 * @param len The length of the packets to be allocated
 *
 * Returns 0 on success, < 0 on error.
 */

static int
wlc_mbss_spt_init(wlc_info_t *wlc, wlc_spt_t *spt, int count, int len)
{
	int idx;

	if (count > WLC_SPT_COUNT_MAX) {
		return -1;
	}

	ASSERT(spt != NULL);
	bzero(spt, sizeof(*spt));

	for (idx = 0; idx < count; idx++) {
		if ((spt->pkts[idx] = PKTGET(wlc->osh, len, TRUE)) == NULL) {
			wlc_mbss_spt_deinit(wlc, spt, TRUE);
			return -1;
		}
	}

	spt->latest_idx = -1;

	return 0;
}

/*
 * Clean up a software template object;
 * if pkt_free_force is TRUE, will not check if the pkt is in use before freeing.
 * Note that if "in use", the assumption is that some other routine owns
 * the packet and will free appropriately.
 */

static void
wlc_mbss_spt_deinit(wlc_info_t *wlc, wlc_spt_t *spt, int pkt_free_force)
{
	int idx;

	if (spt != NULL) {
		for (idx = 0; idx < WLC_SPT_COUNT_MAX; idx++) {
			if (spt->pkts[idx] != NULL) {
				if (pkt_free_force || !SPT_IN_USE(spt, idx)) {
					PKTFREE(wlc->osh, spt->pkts[idx], TRUE);
				} else {
					WLPKTFLAG_BSS_DOWN_SET(WLPKTTAG(spt->pkts[idx]), TRUE);
				}
			}
		}
		bzero(spt, sizeof(*spt));
	}
}


static void
wlc_mbss_ucode_set(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	bool cur_val, new_val;

	/* Assumes MBSS_EN has same value in all cores */
	cur_val = ((wlc_mhf_get(wlc, MHF1, WLC_BAND_AUTO) & MHF1_MBSS_EN) != 0);
	new_val = (MBSS_ENAB(wlc->pub) != 0);

	if (cur_val != new_val) {
		wlc_suspend_mac_and_wait(wlc);
		/* enable MBSS in uCode */
		WL_MBSS(("%s MBSS mode\n", new_val ? "Enabling" : "Disabling"));
		(void)wlc_mhf(wlc, MHF1, MHF1_MBSS_EN, new_val ? MHF1_MBSS_EN : 0, WLC_BAND_ALL);
		wlc_enable_mac(wlc);
	}
}

/* Generate a MAC address for the MBSS AP BSS config */
static int
wlc_bsscfg_macgen(wlc_info_t *wlc, wlc_bsscfg_t *cfg)
{
	int ii, jj;
	bool collision = TRUE;
	int cfg_idx = WLC_BSSCFG_IDX(cfg);
	struct ether_addr newmac;
	struct wlc_mbss_info_t *mbss = wlc->mbss;
#ifdef BCMDBG
	char eabuf[ETHER_ADDR_STR_LEN];
#endif /* BCMDBG */

	if (ETHER_ISNULLADDR(&mbss->vether_base)) {
		/* initialize virtual MAC base for MBSS
		 * the base should come from an external source,
		 * this initialization is in case one isn't provided
		 */
		bcopy(&wlc->pub->cur_etheraddr, &mbss->vether_base, ETHER_ADDR_LEN);
		/* avoid collision */
		mbss->vether_base.octet[5] += 1;

		/* force locally administered address */
		ETHER_SET_LOCALADDR(&mbss->vether_base);
	}

	bcopy(&mbss->vether_base, &newmac, ETHER_ADDR_LEN);

	/* brute force attempt to make a MAC for this interface,
	 * the user didn't provide one.
	 * outside loop limits the # of times we increment the low byte of
	 * the MAC address we're attempting to create, and the inner loop
	 * checks for collisions with other configs.
	 */
	for (ii = 0; (ii < WLC_MAXBSSCFG) && (collision == TRUE); ii++) {
		collision = FALSE;
		for (jj = 0; jj < WLC_MAXBSSCFG; jj++) {
			/* don't compare with the bss config we're updating */
			if (jj == cfg_idx || (!wlc->bsscfg[jj]))
				continue;
			if (EADDR_TO_UC_IDX(wlc->bsscfg[jj]->cur_etheraddr, mbss->mbss_ucidx_mask)
				== EADDR_TO_UC_IDX(newmac, mbss->mbss_ucidx_mask)) {
				collision = TRUE;
				break;
			}
		}
		if (collision == TRUE) /* increment and try again */
			newmac.octet[5] = (newmac.octet[5] & ~(mbss->mbss_ucidx_mask))
			        | (mbss->mbss_ucidx_mask & (newmac.octet[5]+1));
		else
			bcopy(&newmac, &cfg->cur_etheraddr, ETHER_ADDR_LEN);
	}

	if (ETHER_ISNULLADDR(&cfg->cur_etheraddr)) {
#ifdef BCMDBG
		WL_MBSS(("%s: wl%d.%d: wlc_bsscfg_macgen couldn't generate MAC address\n",
		         __FUNCTION__, wlc->pub->unit, cfg_idx));

#endif
		return BCME_BADADDR;
	}
	else {
#ifdef BCMDBG
		WL_MBSS(("%s: wl%d.%d: wlc_bsscfg_macgen assigned MAC %s\n",
		         __FUNCTION__, wlc->pub->unit, cfg_idx,
		         bcm_ether_ntoa(&cfg->cur_etheraddr, eabuf)));
#endif
		return BCME_OK;
	}
}

/* MBSSCAT_4************* bsscfg related END ************************** */

#endif /* WLC_HIGH */

#if defined(WLC_HIGH) && defined(WLC_LOW)

uint16
wlc_mbss_upd_fifo_and_get_start_blk(wlc_info_t *wlc, uint corerev,
	uint startrev, uint16 xmtfifo_sz[][NFIFO])
{
	uint16 txfifo_startblk = TXFIFO_START_BLK;

	/* 4313 has total fifo space of 128 blocks. if we enable
	 * all 16 MBSSs we will not be left with enough fifo space to
	 * support max thru'put. so we only allow configuring/enabling
	 * max of 4 BSSs. Rest of the space is distributed acorss
	 * the tx fifos.
	 */
	if (D11REV_IS(corerev, 24)) {
#ifdef WLLPRS
		uint16 xmtsz[] = { 9, 39, 22, 14, 14, 5 };
#else
		uint16 xmtsz[] = { 9, 47, 22, 14, 14, 5 };
#endif
		memcpy(xmtfifo_sz[(corerev - startrev)],
		       xmtsz, sizeof(xmtsz));
	}
#ifdef WLLPRS
	/* tell ucode the lprs size is 0x80 * 4bytes. */
	wlc_write_shm(wlc, SHM_MBSS_BC_FID2, 0x80);
#endif /* WLLPRS */

	if (MBSS_ENAB(wlc->pub) && wlc_bmac_ucodembss_hwcap(wlc->hw)) {
		ASSERT(wlc->mbss->max_ap_bss > 0);
		txfifo_startblk = MBSS_TXFIFO_START_BLK(wlc->mbss->max_ap_bss);
	}

	return txfifo_startblk;
}

/* MBSSCAT_5************* Software Probe Responses [ONLY for MBSS4]  ****** */
/* ******* Probe Request Fifo Handling: Generate Software Probe Responses ******* */

/* Is the given config up and responding to probe responses in SW? */
#define CFG_SOFT_PRB_RESP(cfg) \
	(((cfg) != NULL) && ((cfg)->up) && ((cfg)->enable) && SOFTPRB_ENAB(cfg))

/*
 * After some investigation, it looks like there's never more than 1 PRQ
 * entry to be serviced at a time.  So the bound here is probably inconsequential.
 */
#define PRQBND 5


/* Given a PRQ entry info structure, generate a PLCP header for a probe response and fixup the
 * txheader of the probe response template
 */
static void
wlc_mbss_prb_resp_plcp_hdrs(wlc_info_t *wlc, wlc_prq_info_t *info, int length, uint8 *plcp,
                       d11txh_t *txh, uint8 *d11_hdr)
{
	uint32 tmp;

	/* generate the PLCP header */
	switch (info->frame_type) {
	case SHM_MBSS_PRQ_FT_OFDM:
		bzero(plcp, D11_PHY_HDR_LEN);
		/* Low order 4 bits preserved from plcp0 */
		plcp[0] = (info->plcp0 & 0xf);
		/* The rest is the length, shifted over 5 bits, little endian */
		tmp = (length & 0xfff) << 5;
		plcp[2] |= (tmp >> 16) & 0xff;
		plcp[1] |= (tmp >> 8) & 0xff;
		plcp[0] |= tmp & 0xff;
		break;
	case SHM_MBSS_PRQ_FT_MIMO:
		plcp[0] = info->plcp0;
		WLC_SET_MIMO_PLCP_LEN(plcp, length);
		break;
	case SHM_MBSS_PRQ_FT_CCK:
		wlc_cck_plcp_set(info->plcp0 / 5, length, plcp);
		break;
	default:
		WL_ERROR(("Received illegal frame type in PRQ\n"));
		break;
	}

	/* for OFDM, MIMO and CCK fixup txheader, d11hdr */
	if (info->frame_type == SHM_MBSS_PRQ_FT_OFDM || info->frame_type == SHM_MBSS_PRQ_FT_MIMO ||
		info->frame_type == SHM_MBSS_PRQ_FT_CCK) {
		uint16 phyctl = 0;
		uint16 mainrates;
		uint16 xfts;
		uint16 durid;
		ratespec_t rspec;
		struct dot11_header *h = (struct dot11_header *)d11_hdr;

		/* plcp0 low 4 bits have incoming rate, we'll respond at same rate */
		if (info->frame_type == SHM_MBSS_PRQ_FT_OFDM) {
			rspec = wlc_ofdm_plcp_to_rspec(plcp[0]);
			phyctl = FT_OFDM;
		}
		else if (info->frame_type == SHM_MBSS_PRQ_FT_MIMO) {
			rspec = plcp[0] | RSPEC_MIMORATE;
			phyctl = FT_HT;
		}
		else {
			rspec = plcp[0]/5;
			phyctl = FT_CCK;
		}

#ifdef WL11N
		if (WLCISHTPHY(wlc->band)) {
			uint16 phytxant;
			phytxant = wlc_stf_txcore_get(wlc, rspec) << PHY_TXC_ANT_SHIFT;
			phyctl |= phytxant & PHY_TXC_HTCORE_MASK;
		} else
#endif /* WL11N */
			phyctl |= wlc->stf->phytxant & PHY_TXC_ANT_MASK;

		txh->PhyTxControlWord = htol16(phyctl);

		mainrates = D11A_PHY_HDR_GRATE((ofdm_phy_hdr_t *)plcp);
		txh->MainRates = htol16(mainrates);

		/* leave "most" of existing XtraFrameTypes, but make sure Fallback Frame Type
		 * is set to FT_OFDM.
		 */
		xfts = ltoh16(txh->XtraFrameTypes);
		xfts &= 0xFFFC;
		if (info->frame_type == SHM_MBSS_PRQ_FT_OFDM)
			xfts |= FT_OFDM;
		else if (info->frame_type == SHM_MBSS_PRQ_FT_MIMO)
			xfts |= FT_HT;
		else
			xfts |= FT_CCK;

		txh->XtraFrameTypes = htol16(xfts);

		/* dup plcp as fragplcp, same fallback rate etc */
		bcopy(plcp, (char*)&txh->FragPLCPFallback, sizeof(txh->FragPLCPFallback));

		/* Possibly fixup some more fields */
		if (WLCISNPHY(wlc->band) || WLCISLPPHY(wlc->band) || WLCISSSLPNPHY(wlc->band)) {
			uint16 phyctl1 = 0;

			/* When uCode handles probe responses, they use same rate for fallback
			 * as the main rate, so we'll do the same.
			 */

			/* the following code expects the BW setting in the ratespec */
			rspec &= ~RSPEC_BW_MASK;
			rspec |= RSPEC_BW_20MHZ;

			phyctl1 = wlc_phytxctl1_calc(wlc, rspec);
			txh->PhyTxControlWord_1 = htol16(phyctl1);
			txh->PhyTxControlWord_1_Fbr = htol16(phyctl1);
		}

		/* fixup dur based on our tx rate */
		durid = wlc_compute_frame_dur(wlc, rspec,
		                              ((RSPEC_ISHT(rspec) || RSPEC_ISVHT(rspec)) ?
		                               WLC_MM_PREAMBLE : WLC_LONG_PREAMBLE),
		                              0);
		h->durid = htol16(durid);
		txh->FragDurFallback = h->durid;
	}
}


/*
 * Convert raw PRQ entry to info structure
 * Returns error if bsscfg not found in wlc structure
 */

static int
wlc_mbss_prq_entry_convert(wlc_info_t *wlc, shm_mbss_prq_entry_t *in, wlc_prq_info_t *out)
{
	int uc_idx, sw_idx;
	struct wlc_mbss_info_t *mbss = wlc->mbss;

	bzero(out, sizeof(*out));
	bcopy(in, &out->source, sizeof(shm_mbss_prq_entry_t));
	if (ETHER_ISNULLADDR(&out->source.ta)) {
		WL_ERROR(("%s: PRQ Entry for Transmitter Address is NULL\n", __FUNCTION__));
		return -1;
	}

	out->directed_ssid = SHM_MBSS_PRQ_ENT_DIR_SSID(in);
	out->directed_bssid = SHM_MBSS_PRQ_ENT_DIR_BSSID(in);
	out->is_directed = (out->directed_ssid || out->directed_bssid);
	out->frame_type = SHM_MBSS_PRQ_ENT_FRAMETYPE(in);
	out->up_band = SHM_MBSS_PRQ_ENT_UPBAND(in);
	out->plcp0 = SHM_MBSS_PRQ_ENT_PLCP0(in);
#ifdef WLLPRS
	out->is_htsta = SHM_MBSS_PRQ_ENT_HTSTA(in);
#endif /* WLLPRS */

	if (out->is_directed) {
		uc_idx = SHM_MBSS_PRQ_ENT_UC_BSS_IDX(in);
		sw_idx = WLC_BSSCFG_HW2SW_IDX(mbss, uc_idx);
		if (sw_idx < 0) {
			return -1;
		}
		out->bsscfg = wlc->bsscfg[sw_idx];
		ASSERT(out->bsscfg != NULL);
	}

	return 0;
}

/*
 * PRQ FIFO Processing
 */

static void
wlc_mbss_prb_pkt_final_setup(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_prq_info_t *info,
	uint8 *pkt_start, int len)
{
	d11txh_t *txh;
	uint8 *plcp_hdr;
	uint8 *d11_hdr;
	uint8 *d11_da;
	int plcp_len;
	uint32 exptime;
	uint16 exptime_low;
	uint16 mcl;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	txh = (d11txh_t *)pkt_start;
	plcp_hdr = &pkt_start[D11_TXH_LEN];
	d11_hdr = &pkt_start[D11_TXH_LEN + D11_PHY_HDR_LEN];
	d11_da = &d11_hdr[2 * sizeof(uint16)]; /* skip fc and duration/id */

	/* Set up the PHY header */
	plcp_len = len - D11_TXH_LEN - D11_PHY_HDR_LEN + DOT11_FCS_LEN;
	wlc_mbss_prb_resp_plcp_hdrs(wlc, info, plcp_len, plcp_hdr, txh, d11_hdr);

	if (pcfgcubby->prb_ttl_us > 0) { /* Set the packet expiry time */
		exptime = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
		exptime_low = (uint16)exptime;
		exptime = (exptime & 0xffff0000) | info->source.time_stamp;
		if (exptime_low < info->source.time_stamp) { /* Rollover occurred */
			exptime -= 0x10000; /* Decrement upper 16 bits. */
		}
		exptime += pcfgcubby->prb_ttl_us;

		txh->TstampLow = htol16(exptime & 0xffff);
		txh->TstampHigh = htol16((exptime >> 16) & 0xffff);
		mcl = ltoh16(txh->MacTxControlLow);
		mcl |= TXC_LIFETIME;
		txh->MacTxControlLow = htol16(mcl);
	}

	/* Set up the dest addr */
	bcopy(&info->source.ta, d11_da, sizeof(struct ether_addr));
}

#if defined(WLLPRS)
/* This function does a selective copy of the PRQ template to the actual probe resp
 * packet. The idea is to intelligently form the "best" probe resp based on the
 * prq_info passed in by uCODE, based on parsing of the probe request. This is a change
 * to the existing "universal" probe response model.
 */
static void
wlc_mbss_prq_resp_form(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_prq_info_t *info,
                  uint8 *pkt_start, int *len)
{
	wlc_pkt_t template;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	template = pcfgcubby->probe_template;

	if (template == NULL)
		return;

	/* We need to change from the template */
	if (!info->is_htsta && PRB_HTIE(bsscfg).present) {
		uint8 *src_ptr = PKTDATA(wlc->osh, template);
		uint8 *dst_ptr = pkt_start;
		int copy_len = D11_PHY_HDR_LEN + D11_TXH_LEN + DOT11_MGMT_HDR_LEN +
		               PRB_HTIE(bsscfg).offset;

		/* Exclude ANA HT IEs while copying */
		bcopy(src_ptr, dst_ptr, copy_len);

		*len -= PRB_HTIE(bsscfg).length;
		src_ptr = PKTDATA(wlc->osh, template) + copy_len +
		          PRB_HTIE(bsscfg).length;
		dst_ptr = pkt_start + copy_len;
		copy_len = *len - copy_len;

		bcopy(src_ptr, dst_ptr, copy_len);
	} else
		bcopy(PKTDATA(wlc->osh, template), pkt_start, *len);

	return;
}
#endif /* WLLPRS */

/* Respond to the given PRQ entry on the given bss cfg */
static void
wlc_mbss_prq_directed(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, wlc_prq_info_t *info)
{
	wlc_pkt_t pkt, template;
	uint8 *pkt_start;
	int len;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	template = pcfgcubby->probe_template;
	if (template == NULL) {
		return;
	}
	len = PKTLEN(wlc->osh, template);

	/* Allocate a new pkt for DMA; copy from template; set up hdrs */
	pkt = PKTGET(wlc->osh, BCN_TMPL_LEN, TRUE);
	if (pkt == NULL) {
		WLCNTINCR(pcfgcubby->cnt->prb_resp_alloc_fail);
		return;
	}
	/* Template includes TX header, PLCP header and D11 header */
	pkt_start = PKTDATA(wlc->osh, pkt);
#if defined(WLLPRS)
	if (N_ENAB(wlc->pub))
		wlc_mbss_prq_resp_form(wlc, bsscfg, info, pkt_start, &len);
#else /* WLLPRS */
	bcopy(PKTDATA(wlc->osh, template), pkt_start, len);
#endif /* WLLPRS */
	PKTSETLEN(wlc->osh, pkt, len);
	WLPKTTAGBSSCFGSET(pkt, WLC_BSSCFG_IDX(bsscfg));

	wlc_mbss_prb_pkt_final_setup(wlc, bsscfg, info, pkt_start, len);

	if (dma_txfast(WLC_HW_DI(wlc, TX_ATIM_FIFO), pkt, TRUE) < 0) {
		WL_MBSS(("Failed to transmit probe resp for bss %d\n", WLC_BSSCFG_IDX(bsscfg)));
		WLCNTINCR(pcfgcubby->cnt->prb_resp_tx_fail);
	}
}

/* Process a PRQ entry, whether broadcast or directed, generating probe response(s) */


static void
wlc_mbss_prq_response(wlc_info_t *wlc, wlc_prq_info_t *info)
{
	int idx;
	wlc_bsscfg_t *cfg;
	struct wlc_mbss_info_t *mbss = wlc->mbss;

#ifdef WLPROBRESP_SW
	if (WLPROBRESP_SW_ENAB(wlc))
		return;
#endif /* WLPROBRESP_SW */

	if (info->is_directed) {
		ASSERT(info->bsscfg != NULL);
		wlc_mbss_prq_directed(wlc, info->bsscfg, info);
	} else { /* Broadcast probe response */
		for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
			cfg = wlc->bsscfg[(idx + mbss->bcast_next_start) % WLC_MAXBSSCFG];
			if (CFG_SOFT_PRB_RESP(cfg) && !cfg->closednet_nobcprbresp) {
				wlc_mbss_prq_directed(wlc, cfg, info);
			}
		}
		/* Move "next start" up to next BSS skipping inactive BSSes */

		for (idx = 0; idx < WLC_MAXBSSCFG; idx++) {
			if (++mbss->bcast_next_start == WLC_MAXBSSCFG) {
				mbss->bcast_next_start = 0;
			}
			if (CFG_SOFT_PRB_RESP(wlc->bsscfg[mbss->bcast_next_start])) {
				break;
			}
		}
	}
}

/*
 * Process the PRQ Fifo.
 * Note that read and write pointers are (uint16 *) in the ucode
 * Return TRUE if more entries to process.
 */
bool
wlc_mbss_prq_process(wlc_info_t *wlc, bool bounded)
{
	uint16 rd_ptr, wr_ptr, prq_base, prq_top;
	shm_mbss_prq_entry_t entry;
	wlc_prq_info_t info;
	int count = 0;
	bool rv = FALSE;  /* Default, no more to be done */
	bool set_rd_ptr = FALSE;
	struct wlc_mbss_info_t *mbss = wlc->mbss;
#if defined(BCMDBG)
	wlc_mbss_bsscfg_cubby_t *pcfgcubby;
#endif

	if (!MBSS_ENAB(wlc->pub)) {
		return FALSE;
	}

	prq_base = mbss->prq_base;
	prq_top = prq_base + (SHM_MBSS_PRQ_TOT_BYTES / 2);

	rd_ptr = mbss->prq_rd_ptr;
	wr_ptr = wlc_read_shm(wlc, SHM_MBSS_PRQ_WRITE_PTR);

#if defined(BCMDBG)
	/* Debug checks for rd and wr ptrs */
	if (wr_ptr < prq_base || wr_ptr >= prq_top) {
		WL_ERROR(("Error: PRQ fifo write pointer 0x%x out of bounds (%d, %d)\n",
			wr_ptr, prq_base, prq_top));
		return FALSE;
	}
	if (rd_ptr < prq_base || rd_ptr >= prq_top) {
		WL_ERROR(("Error: PRQ read pointer 0x%x out of bounds; clearing fifo\n", rd_ptr));
		/* Reset read pointer to write pointer, emptying the fifo */
		rd_ptr = wr_ptr;
		set_rd_ptr = TRUE;
	}
#endif /* BCMDBG */

	while (rd_ptr != wr_ptr) {
		WLCNTINCR(wlc->pub->_cnt->prq_entries_handled);
		set_rd_ptr = TRUE;

		/* Copy entry from PRQ; convert and respond; update rd ptr */
		wlc_copyfrom_shm(wlc, rd_ptr * 2, &entry, sizeof(entry));
		if (wlc_mbss_prq_entry_convert(wlc, &entry, &info) < 0) {
			WL_ERROR(("Error reading prq fifo at offset 0x%x\n", rd_ptr));
#if defined(BCMDBG)
			wlc_mbss_prq_entry_dump(wlc, NULL, rd_ptr, &entry);
#endif
			WLCNTINCR(wlc->pub->_cnt->prq_bad_entries);
		} else if (info.is_directed && !(info.bsscfg->up)) { /* Ignore rqst */
			WL_MBSS(("MBSS: Received PRQ entry on down BSS (%d)\n",
				WLC_BSSCFG_IDX(info.bsscfg)));
		} else {
#if defined(BCMDBG)
			if (info.is_directed && info.bsscfg != NULL) {
				if (0) { /* Extra dump for directed requests */
					wlc_mbss_prq_info_dump(wlc, &info);
				}
				pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, info.bsscfg);
				WLCNTINCR(pcfgcubby->cnt->prq_directed_entries);
			} else {
				WLCNTINCR(wlc->pub->_cnt->prq_undirected_entries);
			}
#endif
			wlc_mbss_prq_response(wlc, &info);
		}

		/* Update the read pointer */
		rd_ptr += sizeof(entry) / 2;
		if (rd_ptr >= prq_top) {
			rd_ptr = prq_base;
		}

		if (bounded && (count++ >= PRQBND)) {
			rv = TRUE; /* Possibly more to do */
			break;
		}
	}

	if (set_rd_ptr) { /* Write the value back when done processing */
		wlc_write_shm(wlc, SHM_MBSS_PRQ_READ_PTR, rd_ptr);
		mbss->prq_rd_ptr = rd_ptr;
	}

	return rv;
}

#if defined(BCMDBG)
static void
wlc_mbss_prq_entry_dump(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint16 rd_ptr,
	shm_mbss_prq_entry_t *entry)
{
	uint8 *ptr;
	char ssidbuf[SSID_FMT_BUF_LEN];

	ptr = (uint8 *)entry;
	if (rd_ptr != 0) {
		WL_MBSS(("Dump of raw PRQ entry from offset 0x%x (word offset 0x%x)\n",
			rd_ptr * 2, rd_ptr));
	} else {
		WL_MBSS(("    Dump of raw PRQ entry\n"));
	}
	WL_MBSS(("    %02x%02x %02x%02x %02x%02x %02x%02x %04x\n",
		ptr[0], ptr[1], ptr[2], ptr[3], ptr[4], ptr[5],
		entry->prq_info[0], entry->prq_info[1], entry->time_stamp));
	WL_MBSS(("    %sdirected SSID. %sdirected BSSID. uc_idx: %d. type %d. upband %d.\n",
		SHM_MBSS_PRQ_ENT_DIR_SSID(entry) ? "" : "not ",
		SHM_MBSS_PRQ_ENT_DIR_BSSID(entry) ? "" : "not ",
		SHM_MBSS_PRQ_ENT_UC_BSS_IDX(entry),
		SHM_MBSS_PRQ_ENT_FRAMETYPE(entry),
		SHM_MBSS_PRQ_ENT_UPBAND(entry)));
	if (bsscfg != NULL) {
		wlc_format_ssid(ssidbuf, bsscfg->SSID, bsscfg->SSID_len);
		WL_MBSS(("    Entry mapped to bss %d, ssid %s\n", WLC_BSSCFG_IDX(bsscfg), ssidbuf));
	}
}
static void
wlc_mbss_prq_info_dump(wlc_info_t *wlc, wlc_prq_info_t *info)
{
	WL_MBSS(("Dump of PRQ info: dir %d. dir ssid %d. dir bss %d. bss cfg idx %d\n",
		info->is_directed, info->directed_ssid, info->directed_bssid,
		WLC_BSSCFG_IDX(info->bsscfg)));
	WL_MBSS(("    frame type %d, up_band %d, plcp0 0x%x\n",
		info->frame_type, info->up_band, info->plcp0));
	wlc_mbss_prq_entry_dump(wlc, info->bsscfg, 0, &info->source);
}
#else
#define wlc_mbss_prq_entry_dump(a, b, c, d)
#define wlc_mbss_prq_info_dump(a, b)
#endif /* BCMDBG */

/* MBSSCAT_5************* Software Probe Responses [ONLY for MBSS4]  END ************* */

#endif /* defined(WLC_HIGH) && defined(WLC_LOW) */


/* MBSSCAT_6************* misc  ********************************************** */

/* DTIM pre-TBTT interrupt has occurred: Update SHM last_fid registers. */
void
wlc_mbss_update_bss_bcmc_fid(wlc_info_t *wlc)
{
	int i = 0;
	uint fid_addr = SHM_MBSS_BC_FID0;
	wlc_bsscfg_t *bsscfg;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby;

	if (!MBSS_ENAB(wlc->pub)) {
		return;
	}

	FOREACH_UP_AP(wlc, i, bsscfg) {
		pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);
		fid_addr = SHM_MBSS_BC_FID_ADDR(WLC_MBSS_BSSCFG_UCIDX(pcfgcubby));

		/* If BC/MC packets have been written, update shared mem */
		if (bsscfg->bcmc_fid != INVALIDFID) {
			wlc_write_shm((wlc), fid_addr, bsscfg->bcmc_fid);
			BCMC_FID_SHM_COMMIT(bsscfg);
		}
	}
}


void
wlc_mbss_upd_bcmc(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint16 *frameid)
{
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	bsscfg->bcmc_fid = *frameid;
	pcfgcubby->mc_fifo_pkts++;
#if defined(WLCNT)
	if (pcfgcubby->mc_fifo_pkts > pcfgcubby->cnt->mc_fifo_max) {
		pcfgcubby->cnt->mc_fifo_max = pcfgcubby->mc_fifo_pkts;
	}
	pcfgcubby->cnt->bcmc_count++;
#endif
	/* invalidate frameid so wlc_bmac_txfifo() does not commit to
	 * the non-MBSS BCMCFID location
	 */
	*frameid = INVALIDFID;
}

wlc_spt_t *wlc_mbss_get_bcm_template(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	return pcfgcubby->bcn_template;
}


wlc_pkt_t
wlc_mbss_prepare_prb_template(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, uint8 **pbody)
{
	wlc_pkt_t pkt = NULL;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	/* Probe response template includes everything from the PLCP header on */
	if ((pkt = pcfgcubby->probe_template) == NULL) {

		pkt = wlc_frame_get_mgmt(wlc, FC_PROBE_RESP, &ether_null,
			&bsscfg->cur_etheraddr, &bsscfg->BSSID, BCN_TMPL_LEN, pbody);
		if (pkt == NULL) {
			WL_ERROR(("Could not allocate SW probe template\n"));
		}
		pcfgcubby->probe_template = pkt;
	} else {
		/* Pull back PLCP and TX headers since wlc_d11hdrs puts them back */
		PKTPULL(wlc->osh, pkt, D11_PHY_HDR_LEN + D11_TXH_LEN);
		/* PKTDATA is now at start of D11 hdr; find packet body */
		*pbody = (uint8 *)PKTDATA(wlc->osh, pkt) + DOT11_MGMT_HDR_LEN;
	}

	return pkt;
}

void
wlc_mbss_bsscfg_set_prb_modif(struct wlc_mbss_info_t *mbss, wlc_bsscfg_t *cfg, bool flag)
{
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);

	pcfgcubby->prb_modified = flag;
}

uint32
wlc_mbss_bsscfg_get_mc_fifo_pkts(struct wlc_mbss_info_t *mbss, wlc_bsscfg_t *cfg)
{
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);
	return pcfgcubby->mc_fifo_pkts;
}

void
wlc_mbss_bsscfg_set_prb_ttl_us(struct wlc_mbss_info_t *mbss,
	wlc_bsscfg_t *bsscfg, uint32 prb_ttl_us)
{
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, bsscfg);
	pcfgcubby->prb_ttl_us = prb_ttl_us;
}


/* MBSSCAT_6************* misc  END********************************************** */


/* MBSSCAT_6.1************* apps related********************************************** */

/* Last STA has gone out of PS.  Check state of its BSS */

void
wlc_mbss_apps_bss_ps_off_start(wlc_info_t *wlc, struct scb *bcmc_scb)
{
	wlc_bsscfg_t *bsscfg;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby;

	bsscfg = bcmc_scb->bsscfg;
	ASSERT(bsscfg != NULL);

	pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);
	if (!BCMC_PKTS_QUEUED(bsscfg)) {
		/* No pkts in BCMC fifo */
		wlc_mbss_apps_bss_ps_off_done(wlc, bsscfg);
	} else { /* Mark in transition */
		ASSERT(bcmc_scb->PS); /* Should only have BCMC pkts if in PS */
		bsscfg->flags |= WLC_BSSCFG_PS_OFF_TRANS;
		WL_PS(("wl%d.%d: START PS-OFF. last fid 0x%x. shm fid 0x%x\n",
			wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg), bsscfg->bcmc_fid,
			bsscfg->bcmc_fid_shm));
#if defined(BCMDBG_MBSS_PROFILE)     /* Start transition timing */
		if (pcfgcubby->ps_start_us == 0) {
			pcfgcubby->ps_start_us = R_REG(wlc->osh, &wlc->regs->tsf_timerlow);
		}
#endif /* BCMDBG_MBSS_PROFILE */
	}
}

/*
 * Due to a STA transitioning to PS on, all packets have been drained from the
 * data fifos.  Update PS state of all BSSs (if not in PS-OFF transition).
 *
 * Note that it's possible that a STA has come out of PS mode during the
 * transition, so we may return to PS-OFF (abort the transition).  Since we
 * don't keep state of which STA and which BSS started the transition, we
 * simply check them all.
 */

void
wlc_mbss_apps_bss_ps_on_done(wlc_info_t *wlc)
{
	wlc_bsscfg_t *bsscfg;
	struct scb *bcmc_scb;
	int i;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby;

	FOREACH_UP_AP(wlc, i, bsscfg) {
		if (!(bsscfg->flags & WLC_BSSCFG_PS_OFF_TRANS)) { /* Ignore BSS in PS-OFF trans */
			bcmc_scb = WLC_BCMCSCB_GET(wlc, bsscfg);
			pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);
			if (wlc_apps_BSS_PS_NODES(wlc,  bsscfg) != 0) {
				if (!SCB_PS(bcmc_scb)) {
#if defined(MBSS)
					/* PS off, MC pkts to data fifo should be cleared */
					ASSERT(pcfgcubby->mc_fifo_pkts == 0);
#endif
					WLCNTINCR(pcfgcubby->cnt->ps_trans);
					WL_NONE(("wl%d.%d: DONE PS-ON\n",
						wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
				}
				bcmc_scb->PS = TRUE;
			} else { /* Unaffected BSS or transition aborted for this BSS */
				bcmc_scb->PS = FALSE;
			}
		}
	}
}

/*
 * Last STA for a BSS exitted PS; BSS has no pkts in BC/MC fifo.
 * Check whether other stations have entered PS since and update
 * state accordingly.
 *
 * That is, it is possible that the BSS state will remain PS
 * TRUE (PS delivery mode enabled) if a STA has changed to PS-ON
 * since the start of the PS-OFF transition.
 */

void
wlc_mbss_apps_bss_ps_off_done(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	struct scb *bcmc_scb;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, bsscfg);

	ASSERT(bsscfg->bcmc_fid_shm == INVALIDFID);
	ASSERT(bsscfg->bcmc_fid == INVALIDFID);

	bcmc_scb = WLC_BCMCSCB_GET(wlc, bsscfg);
	ASSERT(SCB_PS(bcmc_scb));

	if (wlc_apps_BSS_PS_NODES(wlc, bsscfg) != 0) {
		/* Aborted transtion:  Set PS delivery mode */
		bcmc_scb->PS = TRUE;
	} else { /* Completed transition: Clear PS delivery mode */
		bcmc_scb->PS = FALSE;
		WLCNTINCR(pcfgcubby->cnt->ps_trans);
		if (bsscfg->flags & WLC_BSSCFG_PS_OFF_TRANS) {
			WL_PS(("wl%d.%d: DONE PS-OFF.\n", wlc->pub->unit, WLC_BSSCFG_IDX(bsscfg)));
		}
	}

	bsscfg->flags &= ~WLC_BSSCFG_PS_OFF_TRANS; /* Clear transition flag */

	/* Forward any packets in MC-PSQ according to new state */
	while (wlc_apps_ps_send(wlc, bcmc_scb, WLC_PREC_BMP_ALL, 0))
		/* Repeat until queue empty */
		;

#if defined(BCMDBG_MBSS_PROFILE)
	if (pcfgcubby->ps_start_us != 0) {
		uint32 diff_us;

		diff_us = R_REG(wlc->osh, &wlc->regs->tsf_timerlow) - pcfgcubby->ps_start_us;
		if (diff_us > pcfgcubby->max_ps_off_us) pcfgcubby->max_ps_off_us = diff_us;
		pcfgcubby->tot_ps_off_us += diff_us;
		pcfgcubby->ps_off_count++;
		pcfgcubby->ps_start_us = 0;
	}
#endif /* BCMDBG_MBSS_PROFILE */
}

/* MBSSCAT_6.1************* apps related END************************************ */

/* MBSSCAT_7************* dbg  ********************************************** */

#if defined(BCMDBG)

/* Get the SSID for the indicated (idx) bsscfg from SHM Return the length
 */
static void
wlc_mbss_shm_ssid_get(wlc_info_t *wlc, int idx, wlc_ssid_t *ssid)
{
	int i;
	int base;
	uint16 tmpval;
	int ucode_idx;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(wlc->mbss, wlc->bsscfg[idx]);

	ucode_idx = WLC_MBSS_BSSCFG_UCIDX(pcfgcubby);

	if (MBSS_ENAB16(wlc->pub)) {
		base = SHM_MBSS_SSIDSE_BASE_ADDR + (ucode_idx * SHM_MBSS_SSIDSE_BLKSZ);
		wlc_bmac_copyfrom_objmem(wlc->hw, base, &ssid->SSID_len,
		                       SHM_MBSS_SSIDLEN_BLKSZ, OBJADDR_SRCHM_SEL);
		/* search mem length field is always little endian */
		ssid->SSID_len = ltoh32(ssid->SSID_len);
		base += SHM_MBSS_SSIDLEN_BLKSZ;
		wlc_bmac_copyfrom_objmem(wlc->hw, base, ssid->SSID,
		                       SHM_MBSS_SSID_BLKSZ, OBJADDR_SRCHM_SEL);
		return;
	}

	WLC_MBSS_SSID_LEN_GET(wlc, ucode_idx, ssid->SSID_len);
	base = SHM_MBSS_SSID_ADDR(ucode_idx);
	for (i = 0; i < DOT11_MAX_SSID_LEN; i += 2) {
		tmpval = wlc_read_shm(wlc, base + i);
		ssid->SSID[i] = tmpval & 0xFF;
		ssid->SSID[i + 1] = tmpval >> 8;
	}
}


int
wlc_mbss_gen_bsscfg_dump(wlc_info_t *wlc, struct bcmstrbuf *b)
{
	bcm_bprintf(b, "MBSS Build.  MBSS is %s. SW MBSS MHF band 0: %s; band 1: %s\n",
		MBSS_ENAB(wlc->pub) ? "enabled" : "disabled",
		(wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_2G) & MHF1_MBSS_EN) ? "set" : "clear",
		(wlc_bmac_mhf_get(wlc->hw, MHF1, WLC_BAND_5G) & MHF1_MBSS_EN) ? "set" : "clear");
	bcm_bprintf(b, "Pkts suppressed from ATIM:	%d. Bcn Tmpl not ready/done %d/%d\n",
		WLCNTVAL(wlc->pub->_cnt->atim_suppress_count),
		WLCNTVAL(wlc->pub->_cnt->bcn_template_not_ready),
		WLCNTVAL(wlc->pub->_cnt->bcn_template_not_ready_done));
#if defined(WLC_HIGH) && defined(WLC_LOW)
		bcm_bprintf(b, "WLC: cached prq base 0x%x, current prq rd 0x%x\n",
			wlc->mbss->prq_base,
			wlc->mbss->prq_rd_ptr);
#endif /* WLC_HIGH && WLC_LOW */
	bcm_bprintf(b, "Late TBTT counter %d\n",
		WLCNTVAL(wlc->pub->_cnt->late_tbtt_dpc));
	if (BSSCFG_EXTRA_VERBOSE && wlc->clk) {
		bcm_bprintf(b, "MBSS shared memory offsets and values:\n");
		SHOW_SHM(wlc, b, SHM_MBSS_BSSID0, "BSSID0");
		SHOW_SHM(wlc, b, SHM_MBSS_BSSID1, "BSSID1");
		SHOW_SHM(wlc, b, SHM_MBSS_BSSID2, "BSSID2");
		SHOW_SHM(wlc, b, SHM_MBSS_BCN_COUNT, "BCN_COUNT");
		SHOW_SHM(wlc, b, SHM_MBSS_PRQ_BASE, "PRQ_BASE");
		SHOW_SHM(wlc, b, SHM_MBSS_BC_FID0, "BC_FID0");
		SHOW_SHM(wlc, b, SHM_MBSS_BC_FID1, "BC_FID1");
		SHOW_SHM(wlc, b, SHM_MBSS_BC_FID2, "BC_FID2");
		SHOW_SHM(wlc, b, SHM_MBSS_BC_FID3, "BC_FID3");
		SHOW_SHM(wlc, b, SHM_MBSS_PRE_TBTT, "PRE_TBTT");
		SHOW_SHM(wlc, b, SHM_MBSS_SSID_LEN0, "SSID_LEN0");
		SHOW_SHM(wlc, b, SHM_MBSS_SSID_LEN1, "SSID_LEN1");
		SHOW_SHM(wlc, b, SHM_MBSS_PRQ_READ_PTR, "PRQ_RD");
		SHOW_SHM(wlc, b, SHM_MBSS_PRQ_WRITE_PTR, "PRQ_WR");
		SHOW_SHM(wlc, b, M_HOST_FLAGS1, "M_HOST1");
		SHOW_SHM(wlc, b, M_HOST_FLAGS2, "M_HOST2");
	}
	/* Dump out data at current PRQ ptrs */
	bcm_bprintf(b, "PRQ entries handled %d. Undirected %d. Bad %d\n",
		WLCNTVAL(wlc->pub->_cnt->prq_entries_handled),
		WLCNTVAL(wlc->pub->_cnt->prq_undirected_entries),
		WLCNTVAL(wlc->pub->_cnt->prq_bad_entries));

	if (BSSCFG_EXTRA_VERBOSE && wlc->clk) {
		uint16 rdptr, wrptr, base, totbytes, offset;
		int j;
		shm_mbss_prq_entry_t entry;
		char ea_buf[ETHER_ADDR_STR_LEN];

		base = wlc_read_shm(wlc, SHM_MBSS_PRQ_BASE);
		rdptr = wlc_read_shm(wlc, SHM_MBSS_PRQ_READ_PTR);
		wrptr = wlc_read_shm(wlc, SHM_MBSS_PRQ_WRITE_PTR);
		totbytes = SHM_MBSS_PRQ_ENTRY_BYTES * SHM_MBSS_PRQ_ENTRY_COUNT;
		if (rdptr < base || (rdptr >= base + totbytes)) {
			bcm_bprintf(b, "WARNING: PRQ read pointer out of range\n");
		}
		if (wrptr < base || (wrptr >= base + totbytes)) {
			bcm_bprintf(b, "WARNING: PRQ write pointer out of range\n");
		}

		bcm_bprintf(b, "PRQ data at %8s %25s\n", "TA", "PLCP0  Time");
		for (offset = base * 2, j = 0; j < SHM_MBSS_PRQ_ENTRY_COUNT;
			j++, offset += SHM_MBSS_PRQ_ENTRY_BYTES) {
			wlc_copyfrom_shm(wlc, offset, &entry, sizeof(entry));
			bcm_bprintf(b, "  0x%04x:", offset);
			bcm_bprintf(b, "  %s ", bcm_ether_ntoa(&entry.ta, ea_buf));
			bcm_bprintf(b, " 0x%0x 0x%02x 0x%04x", entry.prq_info[0],
				entry.prq_info[1], entry.time_stamp);
			if (SHM_MBSS_PRQ_ENT_DIR_SSID(&entry) ||
				SHM_MBSS_PRQ_ENT_DIR_BSSID(&entry)) {
				int uc, sw;

				uc = SHM_MBSS_PRQ_ENT_UC_BSS_IDX(&entry);
				sw = WLC_BSSCFG_HW2SW_IDX(wlc->mbss, uc);
				bcm_bprintf(b, "  (bss uc %d/sw %d)", uc, sw);
			}
			bcm_bprintf(b, "\n");
		}
	}

	return 0;
}

int
wlc_mbss_per_bsscfg_dump(struct wlc_mbss_info_t *mbss,
	wlc_bsscfg_t *cfg, struct bcmstrbuf *b)
{
	char ssidbuf[SSID_FMT_BUF_LEN];
	int bsscfg_idx = WLC_BSSCFG_IDX(cfg);
	wlc_info_t *wlc = mbss->wlc;
	wlc_mbss_bsscfg_cubby_t *pcfgcubby = MBSS_BSSCFG_CUBBY(mbss, cfg);

	bcm_bprintf(b, "PS trans %u.\n", WLCNTVAL(pcfgcubby->cnt->ps_trans));
#if defined(WLC_SPT_DEBUG)
	bcm_bprintf(b, "BCN: bcn tx cnt %u. bcn suppressed %u\n",
		pcfgcubby->bcn_template->tx_count, pcfgcubby->bcn_template->suppressed);
#endif /* WLC_SPT_DEBUG */
	bcm_bprintf(b, "PrbResp: soft-prb-resp %s. directed req %d, alloc_fail %d, tx_fail %d\n",
		SOFTPRB_ENAB(cfg) ? "enabled" : "disabled",
		WLCNTVAL(pcfgcubby->cnt->prq_directed_entries),
		WLCNTVAL(pcfgcubby->cnt->prb_resp_alloc_fail),
		WLCNTVAL(pcfgcubby->cnt->prb_resp_tx_fail));
	bcm_bprintf(b, "PrbResp: TBTT suppressions %d. TTL expires %d. retrx fail %d.\n",
		WLCNTVAL(pcfgcubby->cnt->prb_resp_retrx),
		WLCNTVAL(pcfgcubby->cnt->prb_resp_ttl_expy),
		WLCNTVAL(pcfgcubby->cnt->prb_resp_retrx_fail));
	bcm_bprintf(b, "BCN: soft-bcn %s. bcn in use bmap 0x%x. bcn fail %u\n",
		SOFTBCN_ENAB(cfg) ? "enabled" : "disabled",
		pcfgcubby->bcn_template->in_use_bitmap, WLCNTVAL(pcfgcubby->cnt->bcn_tx_failed));
	bcm_bprintf(b, "BCN: HW MBSS %s. bcn in use bmap 0x%x. bcn fail %u\n",
		UCTPL_MBSS_ENAB(cfg) ? "enabled" : "disabled",
		pcfgcubby->bcn_template->in_use_bitmap, WLCNTVAL(pcfgcubby->cnt->bcn_tx_failed));
	bcm_bprintf(b, "PRB: HW MBSS %s.\n",
		UCTPL_MBSS_ENAB(cfg) ? "enabled" : "disabled");
	bcm_bprintf(b, "MC pkts in fifo %u. Max %u\n", pcfgcubby->mc_fifo_pkts,
		WLCNTVAL(pcfgcubby->cnt->mc_fifo_max));
	if (wlc->clk) {
		wlc_ssid_t ssid;
		uint16 shm_fid;

		shm_fid = wlc_read_shm((wlc),
			SHM_MBSS_WORD_OFFSET_TO_ADDR(5 + pcfgcubby->_ucidx));
		bcm_bprintf(b, "bcmc_fid 0x%x. bcmc_fid_shm 0x%x. shm last fid 0x%x. "
			"bcmc TX pkts %u\n", cfg->bcmc_fid, cfg->bcmc_fid_shm, shm_fid,
			WLCNTVAL(pcfgcubby->cnt->bcmc_count));
		wlc_mbss_shm_ssid_get(wlc, bsscfg_idx, &ssid);
		if (ssid.SSID_len > DOT11_MAX_SSID_LEN) {
			WL_ERROR(("Warning: Invalid MBSS ssid length %d for BSS %d\n",
				ssid.SSID_len, bsscfg_idx));
			ssid.SSID_len = DOT11_MAX_SSID_LEN;
		}
		wlc_format_ssid(ssidbuf, ssid.SSID, ssid.SSID_len);
		bcm_bprintf(b, "MBSS: ucode idx %d; shm ssid >%s< of len %d\n",
			WLC_MBSS_BSSCFG_UCIDX(pcfgcubby), ssidbuf, ssid.SSID_len);
	} else {
		bcm_bprintf(b, "Core clock disabled, not dumping SHM info\n");
	}
	return 0;

}
#endif 

/* MBSSCAT_7************* dbg  END********************************************** */

#endif /* MBSS */
