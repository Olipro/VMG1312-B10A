/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * beamforming support
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: $
 */

#include <wlc_cfg.h>
#include <osl.h>
#include <typedefs.h>
#include <proto/802.11.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <wlc_cfg.h>
#include <wlc_key.h>
#include <wlc_pub.h>
#include <d11.h>
#include <wlc_bsscfg.h>
#include <wlc_rate.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wlc_vht.h>
#include <wlc_bmac.h>
#include <wlc_txbf.h>
#include <wlc_stf.h>

#ifdef WL_BEAMFORMING

/* iovar table */
enum {
	IOV_TXBF_ENAB,
	IOV_TXBF_BFR_CAP,
	IOV_TXBF_BFE_CAP,
	IOV_TXBF_TIMER,
	IOV_TXBF_TRIGGER,
	IOV_TXBF_RATESET,
	IOV_TXBF_BF_LAST
};

struct txbf_scb_info {
	struct scb *scb;
	wlc_txbf_info_t *txbf;
	uint32  cap;
	bool	enable;
	uint8	shm_index; /* index for template & SHM blocks */
	uint8	amt_index;
};

struct txbf_scb_cubby {
	struct txbf_scb_info *txbf_scb_info;
};

#define TXBF_SCB_CUBBY(txbf, scb) (struct txbf_scb_cubby *)SCB_CUBBY(scb, (txbf->scb_handle))
#define TXBF_SCB_INFO(txbf, scb) (TXBF_SCB_CUBBY(txbf, scb))->txbf_scb_info

static const bcm_iovar_t txbf_iovars[] = {
	{"txbf", IOV_TXBF_ENAB,
	(0), IOVT_BOOL, 0
	},
	{"txbf_bfr_cap", IOV_TXBF_BFR_CAP,
	(IOVF_SET_DOWN), IOVT_BOOL, 0
	},
	{"txbf_bfe_cap", IOV_TXBF_BFE_CAP,
	(IOVF_SET_DOWN), IOVT_BOOL, 0
	},
	{"txbf_timer", IOV_TXBF_TIMER,
	(IOVF_SET_UP), IOVT_INT32, 0
	},
	{"txbf_rateset", IOV_TXBF_RATESET,
	(0), IOVT_BUFFER, sizeof(wl_txbf_rateset_t)
	},
	{NULL, 0, 0, 0, 0}
};

#define BF_SOUND_PERIOD_DFT	(25 * 1000/4)	/* 25 ms, in 4us unit */
#define BF_SOUND_PERIOD_DISABLED 0xffff
#define BF_SOUND_PERIOD_MIN	5	/* 5ms */
#define BF_SOUND_PERIOD_MAX	128	/* 128ms */

#define BF_NDPA_TYPE_CWRTS	0x1d
#define BF_NDPA_TYPE_VHT	0x15
#define BF_FB_VALID		0x100	/* Sounding successful, Phy cache is valid */

#define BF_AMT_MASK	0xF000	/* bit 12L bfm enabled, bit 13:15 idx to M_BFIx_BLK */
#define BF_AMT_BFM_ENABLED	(1 << 12)
#define BF_AMT_BLK_IDX_SHIFT	13

static int wlc_set_amt_ta(wlc_info_t *wlc, const struct ether_addr *ea, uint8 bf_idx,
	uint8 *amt_idx);

static int
wlc_txbf_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif);

static int wlc_txbf_up(void *context);
static int wlc_txbf_down(void *context);

static void wlc_txbf_dump(wlc_txbf_info_t *txbf, struct bcmstrbuf *b);
static bool wlc_txbf_check_ofdm_rate(uint8 rate, uint8 *supported_rates, uint8 num_of_rates);

static int wlc_txbf_init_sta(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info);
static void wlc_txbf_bfr_init(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info);

#if defined(BCMDBG) || defined(WLTEST)
static void wlc_txbf_shm_dump(wlc_txbf_info_t *txbf, struct bcmstrbuf *b);
#endif /*  defined(BCMDBG) || defined(WLTEST) */

static int scb_txbf_init(void *context, struct scb *scb);
static void scb_txbf_deinit(void *context, struct scb *scb);

static int
scb_txbf_init(void *context, struct scb *scb)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	struct txbf_scb_cubby *txbf_scb_cubby = (struct txbf_scb_cubby *)TXBF_SCB_CUBBY(txbf, scb);
	struct txbf_scb_info *txbf_scb_info;

	ASSERT(txbf_scb_cubby);

	txbf_scb_info = (struct txbf_scb_info *)MALLOC(txbf->osh, sizeof(struct txbf_scb_info));
	if (!txbf_scb_info)
		return BCME_ERROR;

	bzero(txbf_scb_info, sizeof(struct txbf_scb_info));

	txbf_scb_info->txbf = txbf;
	txbf_scb_info->scb = scb;
	txbf_scb_cubby->txbf_scb_info = txbf_scb_info;
	return BCME_OK;
}


static void
scb_txbf_deinit(void *context, struct scb *scb)
{

	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	struct txbf_scb_cubby *txbf_scb_cubby;
	struct txbf_scb_info *txbf_scb_info;

	ASSERT(txbf);
	ASSERT(scb);

	txbf_scb_cubby = (struct txbf_scb_cubby *)TXBF_SCB_CUBBY(txbf, scb);
	ASSERT(txbf_scb_cubby);
	if (!txbf_scb_cubby)
		return;

	txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);
	ASSERT(txbf_scb_info);
	if (!txbf_scb_info)
		return;

	MFREE(txbf->osh, txbf_scb_info, sizeof(struct txbf_scb_info));
	txbf_scb_cubby->txbf_scb_info = NULL;
	return;
}

const wl_txbf_rateset_t rs = { 	{0xff, 0xff,    0, 0},   /* mcs */
				{0xff, 0xff, 0x7e, 0},   /* Broadcom-to-Broadcom mcs */
				{0x3ff, 0x3ff,    0, 0}, /* vht */
				{0x3ff, 0x3ff, 0x7e, 0}, /* Broadcom-to-Broadcom vht */
				{0,0,0,0,0,0,0,0},       /* ofdm */
				{0,0,0,0,0,0,0,0},       /* Broadcom-to-Broadcom ofdm */
				0,                       /* ofdm count */
				0,                       /* Broadcom-to-Broadcom ofdm count */
			     };
wlc_txbf_info_t *
BCMATTACHFN(wlc_txbf_attach)(wlc_info_t *wlc)
{
	wlc_txbf_info_t *txbf;
	int i;

	if (!(txbf = (wlc_txbf_info_t *)MALLOC(wlc->osh, sizeof(wlc_txbf_info_t)))) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		wlc->pub->_txbf = FALSE;
		return NULL;
	}
	bzero((void *)txbf, sizeof(wlc_txbf_info_t));
	txbf->wlc = wlc;
	txbf->pub = wlc->pub;
	txbf->osh = wlc->osh;

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		wlc->pub->_txbf = FALSE;
	} else {
		wlc->pub->_txbf = TRUE;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, txbf_iovars, "txbf",
		txbf, wlc_txbf_doiovar, NULL, wlc_txbf_up, wlc_txbf_down)) {
		WL_ERROR(("wl%d: txbf wlc_module_register() failed\n", wlc->pub->unit));
		MFREE(wlc->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
		return NULL;
	}
	wlc_dump_register(wlc->pub, "txbf", (dump_fn_t)wlc_txbf_dump, (void *)txbf);

	if (!wlc->pub->_txbf)
		return txbf;

	txbf->scb_handle = wlc_scb_cubby_reserve(wlc, sizeof(struct txbf_scb_cubby),
		scb_txbf_init, scb_txbf_deinit, NULL, (void *)txbf);
	if (txbf->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_scb_cubby_reserve() failed\n", wlc->pub->unit));
		MFREE(wlc->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
		return NULL;
	}

	/* Copy MCS rateset */
	bcopy(rs.txbf_rate_mcs, txbf->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
	bcopy(rs.txbf_rate_mcs_bcm, txbf->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
	/* Copy VHT rateset */
	for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
		txbf->txbf_rate_vht[i] = rs.txbf_rate_vht[i];
		txbf->txbf_rate_vht_bcm[i] = rs.txbf_rate_vht_bcm[i];
	}
	/* Copy OFDM rateset */
	txbf->txbf_rate_ofdm_cnt = rs.txbf_rate_ofdm_cnt;
	bcopy(rs.txbf_rate_ofdm, txbf->txbf_rate_ofdm, rs.txbf_rate_ofdm_cnt);
	txbf->txbf_rate_ofdm_cnt_bcm = rs.txbf_rate_ofdm_cnt_bcm;
	bcopy(rs.txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_bcm,
		rs.txbf_rate_ofdm_cnt_bcm);

#ifdef WLP2P
	txbf->amt_max_idx = AMT_MAXIDX_P2P_USE -  M_ADDR_BMP_BLK_SZ + 1;
#else
	txbf->amt_max_idx = AMT_MAXIDX_P2P_USE + 1;
#endif /* WLP2P */

#ifdef AP
	/* PSTA AWARE AP: Max PSTA tx beamforming entry */
	txbf->amt_start_idx = txbf->amt_max_idx - (AMT_MAX_TXBF_ENTRIES +
		AMT_MAX_TXBF_PSTA_ENTRIES);
#else
	txbf->amt_start_idx = txbf->amt_max_idx - AMT_MAX_TXBF_ENTRIES;
#endif

	wlc_txbf_init(txbf);
	return txbf;
}

void
BCMATTACHFN(wlc_txbf_detach)(wlc_txbf_info_t *txbf)
{
	if (!txbf)
		return;
	txbf->pub->_txbf = FALSE;
	wlc_module_unregister(txbf->pub, "txbf", txbf);
	MFREE(txbf->osh, (void *)txbf, sizeof(wlc_txbf_info_t));
	return;
}

static int
wlc_txbf_up(void *context)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	wlc_info_t *wlc = txbf->wlc;
	int txchains = WLC_BITSCNT(wlc->stf->txchain);
	int rxchains = WLC_BITSCNT(wlc->stf->rxchain);
	uint16 val, i;

	uint16	txbf_MLBF_LUT[] = {0x3475, 0x3475, 0x3475, 0x217c,
		0x237b, 0x217c, 0x217c, 0x217c,
		0x3276, 0x3276, 0x3475, 0x187e,
		0x217c, 0x167e, 0x1d7d, 0x1f7c};

	txbf->shm_base = wlc_read_shm(wlc, M_BFI_BLK_PTR);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_REFRESH_THR_OFFSET) * 2,
		txbf->sounding_period);

	if (txchains == 1) {
		txbf->active = 0;
		WL_TXBF(("wl%d: %s beamforming deactived!(txchains < 2)\n",
			wlc->pub->unit, __FUNCTION__));
	} if (txchains == 2) {
		uint16 ndp2s_phyctl0, antmask;

		antmask = ((wlc->stf->txchain << D11AC_PHY_TXC_CORE_SHIFT)
			& D11AC_PHY_TXC_ANT_MASK);

		ndp2s_phyctl0 = (0x8003 | antmask);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_VHTNDP2S_PHYCTL_OFFSET) * 2,
			ndp2s_phyctl0);

		ndp2s_phyctl0 = (0x8002 | antmask);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP2S_PHYCTL_OFFSET) * 2,
			ndp2s_phyctl0);
	}

	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_BFI_NRXC_OFFSET) * 2, (rxchains - 1));

	/*
	 * treat 2ss spatial expansion as special form of beamfomring
	 * use fixed index 5
	*/
	val = wlc_read_shm(wlc, (txbf->shm_base + 5 * M_BFI_BLK_PTR  + C_BFI_BFRIDX_POS) * 2);
	val &= (~(1 << 8));
	wlc_write_shm(wlc, (txbf->shm_base +  5 * M_BFI_BLK_PTR + C_BFI_BFRIDX_POS) * 2, val);

	/* initial MLBF_LUT */
	for (i = 0; i <  ARRAYSIZE(txbf_MLBF_LUT); i++) {
		wlc_write_shm(wlc, (txbf->shm_base +  M_BFI_MLBF_LUT + i) * 2, txbf_MLBF_LUT[i]);
	}

	WL_TXBF(("wl%d: %s bfr capable %d bfe capable %d beamforming enable %d\n",
		wlc->pub->unit, __FUNCTION__,
		txbf->bfr_capable, txbf->bfe_capable, txbf->enable));
	return BCME_OK;
}

void
wlc_txbf_init(wlc_txbf_info_t *txbf)
{
	wlc_info_t *wlc;

	ASSERT(txbf);
	if (!txbf) {
		return;
	}
	wlc = txbf->wlc;

	txbf->bfr_shm_index_bitmap = 0;
	if (D11REV_GE(wlc->pub->corerev, 40)) {
		if (WLC_BITSCNT(wlc->stf->hw_txchain) >= 1) {
			txbf->bfr_capable = TRUE;
		} else {
			txbf->bfr_capable = FALSE;
		}
		txbf->bfe_capable = FALSE;
		txbf->enable = FALSE;
		txbf->active = FALSE;
		txbf->sounding_period = BF_SOUND_PERIOD_DFT;
	} else {
		txbf->bfr_capable = FALSE;
		txbf->bfe_capable = FALSE;
		txbf->enable = FALSE;
		txbf->active = FALSE;
	}
	WL_TXBF(("wl%d: %s bfr capable %d bfe capable %d beamforming enable %d"
		"beamforming active %d\n", wlc->pub->unit, __FUNCTION__,
		txbf->bfr_capable, txbf->bfe_capable, txbf->enable, txbf->active));

}

int
wlc_txbf_init_link(wlc_txbf_info_t *txbf, struct scb *scb)
{
	wlc_info_t *wlc;
	uint8 amt_idx;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint16 val;
	int status;
	bool	bfe = FALSE;
	bool	bfr = FALSE;
	int idx = -1;
	struct txbf_scb_info *txbf_scb_info;

	ASSERT(txbf);
	wlc = txbf->wlc;

	ASSERT(wlc);
	BCM_REFERENCE(eabuf);

	if (!(wlc->txbf->bfr_capable || wlc->txbf->bfe_capable))
		return BCME_OK;

	if (SCB_VHT_CAP(scb) && (scb->vht_flags & (SCB_SU_BEAMFORMEE | SCB_MU_BEAMFORMEE))) {
		WL_TXBF(("wl%d: %s: sta %s has BFE cap\n",
			wlc->pub->unit, __FUNCTION__,
			bcm_ether_ntoa(&scb->ea, eabuf)));
		bfe = TRUE;
	}

	if (SCB_VHT_CAP(scb) && (scb->vht_flags & (SCB_SU_BEAMFORMER | SCB_MU_BEAMFORMER))) {
		WL_TXBF(("wl%d: %s: sta %s has BFR cap\n",
			wlc->pub->unit, __FUNCTION__,
			bcm_ether_ntoa(&scb->ea, eabuf)));
		bfr = TRUE;
	}

	if (!((wlc->txbf->bfr_capable && bfe) || (wlc->txbf->bfe_capable && bfr)))
		return BCME_OK;

	txbf_scb_info = TXBF_SCB_INFO(txbf, scb);
	ASSERT(txbf_scb_info);

	if (!txbf_scb_info) {
		WL_ERROR(("wl:%d %s failed for %s\n",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
		return BCME_ERROR;
	}

	idx = wlc_txbf_init_sta(txbf, txbf_scb_info);
	if ((idx < 0) || (idx >= WLC_BEAMFORMING_MAX_LINK)) {
		WL_ERROR(("wl%d: %s fail to add bfr blk\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	if (wlc->txbf->bfr_capable && bfe) {
		ASSERT(wlc->pub->up);
		wlc_txbf_bfr_init(txbf, txbf_scb_info);
		txbf_scb_info->enable = TRUE;
	}

	if (!(wlc->txbf->bfe_capable && bfr)) {
		return BCME_OK;
	}

	/*
	 * if wsec is enable, amt entry will be created after key exchange.
	 * TxBf will use the same amt entry
	*/
	if (scb->wsec) {
		/* PSTA don't create a seperate amt entry for keys */
		if (!(PSTA_ENAB(wlc->pub) && BSSCFG_PSTA(SCB_BSSCFG(scb)))) {
			return BCME_OK;
		}
	}

	status = wlc_set_amt_ta(wlc, &scb->ea, (uint8)idx, &amt_idx);
	if (status != BCME_OK) {
		return status;
	}
	ASSERT(amt_idx < AMT_SIZE);
	if (amt_idx >= AMT_SIZE)
		return BCME_ERROR;

	val = wlc_read_shm(wlc, (M_AMT_INFO_BLK + amt_idx * 2));
	val &= (~BF_AMT_MASK);
	val |= (BF_AMT_BFM_ENABLED | idx << BF_AMT_BLK_IDX_SHIFT);

	txbf_scb_info->amt_index = amt_idx;
	wlc_write_shm(wlc, (M_AMT_INFO_BLK + amt_idx * 2), val);

	WL_TXBF(("wl%d: %s sta %s, enabled for txBF. amt %d, idx %d\n",
		wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf), amt_idx, idx));
	return BCME_OK;

}

static int
wlc_txbf_init_sta(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info)
{
	wlc_info_t *wlc;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint8 bf_shm_idx = 0xff, i;
	bool found = 0;
	struct scb *scb = txbf_scb_info->scb;

	ASSERT(txbf);
	wlc = txbf->wlc;
	BCM_REFERENCE(eabuf);

	if (txbf_scb_info->enable) {
		WL_ERROR(("wl%d: %s: scb aleady has user index %d\n", wlc->pub->unit, __FUNCTION__,
			txbf_scb_info->shm_index));
	}

	if (!txbf->active && (WLC_BITSCNT(wlc->stf->txchain) == 1)) {
		WL_TXBF(("wl%d: %s: Can not active beamforming no. of txchains %d\n",
			wlc->pub->unit, __FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
	} else if (txbf->enable) {
		WL_TXBF(("wl%d: %s: beamforming actived! txchains %d\n", wlc->pub->unit,
			__FUNCTION__, WLC_BITSCNT(wlc->stf->txchain)));
		txbf->active = TRUE;
	}

	/* find a free index */
	for (i = 0; i < WLC_BEAMFORMING_MAX_LINK; i++) {
		if ((txbf->bfr_shm_index_bitmap & (1 << i)) == 0) {
			if (!found) {
				bf_shm_idx = i;
				found = 1;
			}
		} else {
			/* check if scb match to any exist entrys */
			if (eacmp(&txbf->bfe_scbs[i]->ea, &scb->ea) == 0) {
				WL_TXBF(("wl%d: %s, TxBF link for  %s alreay exist\n",
					wlc->pub->unit, __FUNCTION__,
					bcm_ether_ntoa(&scb->ea, eabuf)));
					txbf_scb_info->shm_index = i;
				/* all PSTA connection use same txBF link */
				if (!(PSTA_ENAB(wlc->pub) && BSSCFG_PSTA(SCB_BSSCFG(scb)))) {
					txbf->bfe_scbs[i] = scb;
				}
				return i;
			} else if (txbf->bfe_scbs[i] ==  scb->psta_prim) {
				/*
				* PSTA Aware AP:STA's belong to same PSTA share a single
				* TxBF link.
				*/
				txbf_scb_info->shm_index = i;
				WL_TXBF(("wl%d: %s, TxBF link for  ProxySTA %s shm_index %d\n",
				wlc->pub->unit, __FUNCTION__,
				bcm_ether_ntoa(&scb->ea, eabuf), txbf_scb_info->shm_index));
				return txbf_scb_info->shm_index;
			}
		}
	}
	if (!found) {
		WL_ERROR(("%d: %s fail to find a free user index\n", wlc->pub->unit, __FUNCTION__));
		return BCME_ERROR;
	}

	txbf_scb_info->shm_index = bf_shm_idx;
	txbf_scb_info->amt_index = AMT_SIZE;
	txbf->bfr_shm_index_bitmap |= (uint8)((1 << bf_shm_idx));
	txbf->bfe_scbs[bf_shm_idx] = scb;

	WL_TXBF(("%s add  0x%p %s index %d map %x\n", __FUNCTION__, scb,
		bcm_ether_ntoa(&scb->ea, eabuf), bf_shm_idx, txbf->bfr_shm_index_bitmap));

	return bf_shm_idx;
}

void
wlc_txbf_delete_link(wlc_txbf_info_t *txbf, struct scb *scb)
{
	char eabuf[ETHER_ADDR_STR_LEN];
	int i;
	wlc_info_t *wlc;
	struct txbf_scb_info *txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

	BCM_REFERENCE(eabuf);

	ASSERT(txbf);
	wlc = txbf->wlc;
	ASSERT(wlc);

	ASSERT(txbf_scb_info);
	WL_TXBF(("wl:%d %s %s\n",
		wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
	if (!txbf_scb_info) {
		WL_ERROR(("wl:%d %s failed for %s\n",
			wlc->pub->unit, __FUNCTION__, bcm_ether_ntoa(&scb->ea, eabuf)));
		return;
	}

	for (i = 0; i < WLC_BEAMFORMING_MAX_LINK; i++) {
		if ((txbf->bfr_shm_index_bitmap & (1 << i)) == 0)
			continue;
		if ((txbf->bfe_scbs[i] == scb) || (txbf->bfe_scbs[i] == scb->psta_prim)) {
			break;
		}
	}

	if (i == WLC_BEAMFORMING_MAX_LINK) {
		return;
	}

	WL_TXBF(("wl%d: %s delete beamforming link %s shm_index %d amt_index %d\n",
		wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf), txbf_scb_info->shm_index,
		txbf_scb_info->amt_index));

	if (!(SCB_VHT_CAP(scb) && (scb->vht_flags & (SCB_SU_BEAMFORMEE | SCB_MU_BEAMFORMEE
		| SCB_SU_BEAMFORMER | SCB_MU_BEAMFORMER)))) {
		WL_ERROR(("%d: %s STA %s don't have TxBF cap %x\n", wlc->pub->unit, __FUNCTION__,
			bcm_ether_ntoa(&scb->ea, eabuf), scb->vht_flags));
		return;
	}

	if (!txbf_scb_info->enable) {
		/* maybe it was disable due to txchain change, but link is still there */
		WL_ERROR(("%d: %s %s not enabled!\n", wlc->pub->unit, __FUNCTION__,
			bcm_ether_ntoa(&scb->ea, eabuf)));
	}

	txbf_scb_info->enable = FALSE;
	if ((PSTA_ENAB(wlc->pub) && BSSCFG_PSTA(SCB_BSSCFG(scb))))
		return;

	WL_TXBF(("%d: %s %s deleted amt %d!\n", wlc->pub->unit, __FUNCTION__,
		bcm_ether_ntoa(&scb->ea, eabuf), txbf_scb_info->amt_index));

	ASSERT(txbf->bfr_shm_index_bitmap & (1 << txbf_scb_info->shm_index));

	if (txbf->bfe_scbs[i] == scb)
		txbf->bfr_shm_index_bitmap &= (~((1 << txbf_scb_info->shm_index)));
	if (wlc->pub->up) {
		uint16 val;
		if ((txbf_scb_info->amt_index >=  txbf->amt_start_idx)&&
			(txbf_scb_info->amt_index < txbf->amt_max_idx)) {
			wlc_bmac_write_amt(wlc->hw, txbf_scb_info->amt_index, &ether_null, 0);
		}
		if ((txbf_scb_info->amt_index < AMT_SIZE) && (txbf->bfe_scbs[i] == scb)) {
			val = wlc_read_shm(wlc, (M_AMT_INFO_BLK + txbf_scb_info->amt_index * 2));
			val &= (~BF_AMT_MASK);
			wlc_write_shm(wlc, (M_AMT_INFO_BLK + txbf_scb_info->amt_index * 2), val);
		}
	}

	if (txbf->bfe_scbs[i] == scb)
		txbf->bfe_scbs[txbf_scb_info->shm_index] = NULL;
	if (txbf->bfr_shm_index_bitmap) {
		txbf->active = FALSE;
		WL_TXBF(("wl%d: %s beamforming deactived!\n", wlc->pub->unit, __FUNCTION__));
	}
}

static
int wlc_txbf_down(void *context)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)context;
	uint8 i;

	ASSERT(txbf);
	for (i = 0; i < WLC_BEAMFORMING_MAX_LINK; i++) {
		txbf->bfe_scbs[i] = NULL;
	}
	txbf->bfr_shm_index_bitmap = 0;
	txbf->active = FALSE;
	WL_TXBF(("wl%d: %s beamforming deactived!\n", txbf->wlc->pub->unit, __FUNCTION__));
	return BCME_OK;
}


static void
wlc_txbf_bfr_init(wlc_txbf_info_t *txbf, struct txbf_scb_info *txbf_scb_info)
{
	uint16 bfe_config0;
	uint16 bfe_mimoctl;
	uint16 bfr_config0;
	wlc_info_t *wlc;
	struct scb *scb = txbf_scb_info->scb;

	uint16	bf_shm_blk_base, bfrctl = 0;
	uint32 cap;
	uint8 idx;
	bool isVHT = FALSE;

	ASSERT(scb);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (SCB_VHT_CAP(scb))
		isVHT = TRUE;

	cap = txbf_scb_info->cap;
	idx = txbf_scb_info->shm_index;
	bf_shm_blk_base = txbf->shm_base + idx * M_BFI_BLK_SIZE;


	if (isVHT)
		bfe_config0 = 0x1001;
	else
		bfe_config0 = (1 | ((((cap >> HT_CAP_TX_BF_CAP_EXPLICIT_CSI_FB_SHIFT) & 1)
			+ 1) << 11));

	bfe_mimoctl = (0x8410 | (wlc->stf->txstreams - 1));

	if (CHSPEC_IS40(wlc->chanspec)) {
		bfe_mimoctl = bfe_mimoctl | (0x1 << 6);
	} else if  (CHSPEC_IS80(wlc->chanspec)) {
		bfe_mimoctl = bfe_mimoctl | (0x2 << 6);
	}

	bfr_config0 = (0x1 | (isVHT << 3) | (idx << 5));
	if  (isVHT || (((cap) >> HT_CAP_TX_BF_CAP_EXPLICIT_COMPRESSED_FB_SHIFT) & 0x3))
		bfr_config0 = bfr_config0 | 0x4;
	else
		bfr_config0 = bfr_config0 | 0x2;

	if (CHSPEC_IS40(wlc->chanspec)) {
		bfr_config0 = (bfr_config0 | (0x1 << 13));
	} else if  (CHSPEC_IS80(wlc->chanspec)) {
		bfr_config0 = (bfr_config0 | (0x2 << 13));
	}

	wlc_suspend_mac_and_wait(wlc);
	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_NDPA_TYPE_POS) * 2,
		BF_NDPA_TYPE_VHT);

	/* NDP streams and VHT/HT */
	if ((WLC_BITSCNT(wlc->stf->txchain) == 3) && (((cap & VHT_CAP_INFO_NUM_BMFMR_ANT_MASK)
		>> VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT) == 2)) {
		/* 3 streams */
		bfrctl |= (1 << C_BFI_BFRCTL_POS_NSTS_SHIFT);
	}
	if (isVHT) {
		bfrctl |= (1 << C_BFI_BFRCTL_POS_NDP_TYPE_SHIFT);
	}
	if (txbf_scb_info->scb->flags & SCB_BRCM) {
		bfrctl |= (1 << C_BFI_BFRCTL_POS_MLBF_SHIFT);
	}

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFRCTL_POS) * 2, bfrctl);

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFR_CONFIG0_POS) * 2,
		bfr_config0);
	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_CONFIG0_POS) * 2,
		bfe_config0);
	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_MIMOCTL_POS) * 2,
		bfe_mimoctl);

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_BSSID0_POS) * 2,
		((scb->bsscfg->BSSID.octet[1] << 8) | scb->bsscfg->BSSID.octet[0]));

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_BSSID1_POS) * 2,
		((scb->bsscfg->BSSID.octet[3] << 8) | scb->bsscfg->BSSID.octet[2]));

	wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFE_BSSID2_POS) * 2,
		((scb->bsscfg->BSSID.octet[5] << 8) | scb->bsscfg->BSSID.octet[4]));

	wlc_enable_mac(wlc);
	return;
}


void wlc_txbf_update_vht_cap(wlc_txbf_info_t *txbf, struct scb *scb, uint32 vht_cap_info)
{
	struct txbf_scb_info *txbf_scb_info =
		(struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("wl:%d %s failed\n", txbf->wlc->pub->unit, __FUNCTION__));
		return;
	}
	txbf_scb_info->cap = vht_cap_info;
}

void wlc_txbf_rxchain_upd(wlc_txbf_info_t *txbf)
{
	int rxchains;
	wlc_info_t * wlc;

	ASSERT(txbf);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (!txbf->enable)
		return;

	if (!wlc->clk)
		return;

	rxchains = WLC_BITSCNT(wlc->stf->rxchain);
	wlc_write_shm(wlc, (txbf->shm_base + M_BFI_BFI_NRXC_OFFSET) * 2, (rxchains - 1));
}

void wlc_txbf_txchain_upd(wlc_txbf_info_t *txbf)
{
	int txchains = WLC_BITSCNT(txbf->wlc->stf->txchain);
	int i;
	wlc_info_t * wlc;

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (!txbf->enable)
		return;

	if (txchains < 2) {
		txbf->active = FALSE;
		WL_TXBF(("wl%d: %s beamforming deactived!(txchains < 2)\n",
			wlc->pub->unit, __FUNCTION__));
		return;
	}

	if ((!txbf->active) && (txbf->bfr_shm_index_bitmap)) {
		txbf->active = TRUE;
		WL_TXBF(("wl%d: %s beamforming reactived! txchain %d\n", wlc->pub->unit,
			__FUNCTION__, wlc->stf->txchain));
	}

	if ((!wlc->pub->up) || (txbf->bfr_shm_index_bitmap == 0))
		return;

	if (txchains == 2) {
		uint16 ndp2s_phyctl0, antmask;

		antmask = ((wlc->stf->txchain << D11AC_PHY_TXC_CORE_SHIFT)
			& D11AC_PHY_TXC_ANT_MASK);

		ndp2s_phyctl0 = (0x8003 | antmask);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_VHTNDP2S_PHYCTL_OFFSET) * 2,
			ndp2s_phyctl0);

		ndp2s_phyctl0 = (0x8002 | antmask);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_HTNDP2S_PHYCTL_OFFSET) * 2,
			ndp2s_phyctl0);
	}

	wlc_suspend_mac_and_wait(wlc);
	for (i = 0; i < WLC_BEAMFORMING_MAX_LINK; i++) {
		uint16 val, shm_blk_base, bfrctl = 0;
		struct scb *scb;
		struct txbf_scb_info *txbf_scb_info;

		if ((txbf->bfr_shm_index_bitmap & (1 << i)) == 0)
			continue;

		shm_blk_base = txbf->shm_base + i * M_BFI_BLK_SIZE;

		scb = txbf->bfe_scbs[i];
		txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

		/* NDP streams and VHT/HT */
		if ((txchains == 3) && (((txbf_scb_info->cap & VHT_CAP_INFO_NUM_BMFMR_ANT_MASK)
			>> VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT) == 2)) {
			/* 3 streams */
			bfrctl = (1 << C_BFI_BFRCTL_POS_NSTS_SHIFT);
		}
		if (SCB_VHT_CAP(scb)) {
			bfrctl |= (1 << C_BFI_BFRCTL_POS_NDP_TYPE_SHIFT);
		}

		wlc_write_shm(wlc, (shm_blk_base + C_BFI_BFRCTL_POS) * 2, bfrctl);

		/* clean up phy cache */
		val = wlc_read_shm(wlc, (shm_blk_base + C_BFI_BFRIDX_POS) * 2);
		val &= (~(1 << 8));
		wlc_write_shm(wlc, (shm_blk_base + C_BFI_BFRIDX_POS) * 2, val);
	}
	wlc_enable_mac(wlc);

	/* invalid tx header cache */
	if (WLC_TXC_ENAB(wlc))
		wlc->txcgen++;
}

void
wlc_txbf_sounding_clean_cache(wlc_txbf_info_t *txbf)
{
	uint16 val;
	uint16 bf_shm_blk_base, bf_shm_base;
	int i;
	wlc_info_t * wlc;

	wlc = txbf->wlc;
	ASSERT(wlc);

	bf_shm_base = txbf->shm_base;
	for (i = 0; i < WLC_BEAMFORMING_MAX_LINK; i++) {
		if (!(txbf->bfr_shm_index_bitmap & (1 << i)))
			continue;
		bf_shm_blk_base = bf_shm_base + i * M_BFI_BLK_SIZE;
		val = wlc_read_shm(wlc, (bf_shm_blk_base + C_BFI_BFRIDX_POS) * 2);
		val &= (~(1 << 8));
		wlc_write_shm(wlc, (bf_shm_blk_base + C_BFI_BFRIDX_POS) * 2, val);
	}
}

static bool wlc_txbf_check_ofdm_rate(uint8 rate, uint8 *supported_rates, uint8 num_of_rates)
{
	int i;
	for (i = 0; i < num_of_rates; i++) {
		if (rate == (supported_rates[i] & RSPEC_RATE_MASK))
			return TRUE;
	}
	return FALSE;
}


bool wlc_txbf_check(wlc_txbf_info_t *txbf, ratespec_t rspec, struct scb *scb, uint8 *shm_index)
{
	uint nss, rate;
	struct txbf_scb_info *txbf_scb_info;
	int is_brcm_sta = (scb->flags & SCB_BRCM);

	txbf_scb_info = TXBF_SCB_INFO(txbf, scb);
	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("wl:%d %s failed\n", txbf->wlc->pub->unit, __FUNCTION__));
		return FALSE;
	}

	if (!txbf_scb_info->enable)
		return FALSE;

	*shm_index = txbf_scb_info->shm_index;
	if (IS_CCK(rspec))
		return FALSE;

	if (IS_OFDM(rspec)) {
		if (is_brcm_sta) {
			if (txbf->txbf_rate_ofdm_cnt_bcm == TXBF_RATE_OFDM_ALL)
				return TRUE;
			else if (txbf->txbf_rate_ofdm_cnt_bcm)
				return wlc_txbf_check_ofdm_rate((rspec & RSPEC_RATE_MASK),
					txbf->txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_cnt_bcm);
		} else {
			if (txbf->txbf_rate_ofdm_cnt == TXBF_RATE_OFDM_ALL)
				return TRUE;
			else if (txbf->txbf_rate_ofdm_cnt)
				return wlc_txbf_check_ofdm_rate((rspec & RSPEC_RATE_MASK),
					txbf->txbf_rate_ofdm, txbf->txbf_rate_ofdm_cnt);
		}
		return FALSE;
	}

	nss = wlc_ratespec_nss(rspec);
	ASSERT(nss >= 1);
	nss -= 1;
	if (RSPEC_ISVHT(rspec)) {
		rate = (rspec & RSPEC_VHT_MCS_MASK);
		if (is_brcm_sta)
			return (((1 << rate) & txbf->txbf_rate_vht_bcm[nss]) != 0);
		else
			return (((1 << rate) & txbf->txbf_rate_vht[nss]) != 0);
	} else if (RSPEC_ISHT(rspec)) {
		rate = (rspec & RSPEC_RATE_MASK) - (nss) * 8;

		if (is_brcm_sta)
			return (((1 << rate) & txbf->txbf_rate_mcs_bcm[nss]) != 0);
		else
			return (((1 << rate) & txbf->txbf_rate_mcs[nss]) != 0);
	}
	return 0;
}

static void wlc_txbf_dump(wlc_txbf_info_t *txbf, struct bcmstrbuf *b)
{
	int i;
	char eabuf[ETHER_ADDR_STR_LEN];
	struct scb *scb;
	struct txbf_scb_info *txbf_scb_info;
	wlc_info_t *wlc = txbf->wlc;

	BCM_REFERENCE(eabuf);

	if (!TXBF_ENAB(wlc->pub)) {
		bcm_bprintf(b, "Beamforming is not supported!\n");
	}

	bcm_bprintf(b, "TxBF bfr capable:%d bfe capable:%d, enable:%d, active:%d,"
		" Allowed by country code: %d\n",
		txbf->bfr_capable, txbf->bfe_capable,
		txbf->enable, txbf->active, wlc->stf->allow_txbf);
	bcm_bprintf(b, "%d links with beamforming enabled\n",
		WLC_BITSCNT(txbf->bfr_shm_index_bitmap));

	for (i = 0; i < WLC_BEAMFORMING_MAX_LINK; i++) {
		bool valid;
		if (!(txbf->bfr_shm_index_bitmap & (1 << i)))
			continue;
		scb = txbf->bfe_scbs[i];
		ASSERT(scb);
		if (!scb)
			continue;
		txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

		valid = (wlc_read_shm(wlc, (txbf->shm_base + i * M_BFI_BLK_SIZE) * 2)
				& BF_FB_VALID) ? TRUE : FALSE;

		bcm_bprintf(b, "%d:\t mac addr: %s\n\t VHT cap:0x%x, enable:%d, index:%d, amt:%d,"
			" Sounding successful:%d\n",
			(i + 1), bcm_ether_ntoa(&scb->ea, eabuf),
			txbf_scb_info->cap, txbf_scb_info->enable,
			txbf_scb_info->shm_index, txbf_scb_info->amt_index, valid);
	}
#if defined(BCMDBG) || defined(WLTEST)
	wlc_txbf_shm_dump(txbf, b);
#endif /* defined(BCMDBG) || defined(WLTEST) */
}

#if defined(BCMDBG) || defined(WLTEST)
static void wlc_txbf_shm_dump(wlc_txbf_info_t *txbf, struct bcmstrbuf *b)
{
	int i, offset;
	uint16 bf_shm_base, val;
	wlc_info_t *wlc = txbf->wlc;

	if (!wlc->clk)
		return;

	txbf->shm_base = wlc_read_shm(wlc, M_BFI_BLK_PTR);
	bf_shm_base = txbf->shm_base;

	bcm_bprintf(b, "TxBF shm common block: base %x\n", bf_shm_base);
	for (i = M_BFI_COMM_OFFSET; i < (M_BFI_LAST); i++) {
		bcm_bprintf(b, "offset %d, %x\n", i,
			wlc_read_shm(wlc, (bf_shm_base + i) * 2));
	}
	for (i = 0; i < WLC_BEAMFORMING_MAX_LINK; i++) {
		if (!(txbf->bfr_shm_index_bitmap & (1 << i)))
			continue;
		bcm_bprintf(b, "block %d\n", i);
		for (offset = 0; offset <= 12; offset++) {
			bcm_bprintf(b, "offset %d: %x\n",  offset,
			wlc_read_shm(wlc, (bf_shm_base + i * M_BFI_BLK_SIZE + offset) * 2));
		}
	}
	for (i = 0; i < AMT_SIZE; i++) {
		val = wlc_read_shm(wlc, (M_AMT_INFO_BLK + i * 2));
		if (val)
			bcm_bprintf(b, "AMT_INFO_BLK[%d] = %x\n", i, val);
	}
}
#endif 
/*
 * if there is already an entry in amt, just return the index
 * otherwise create a new enry
 */
static int
wlc_set_amt_ta(wlc_info_t *wlc, const struct ether_addr *ea, uint8 bf_shm_idx, uint8 *amt_idx)
{
	uint16 attr;
	char eabuf[ETHER_ADDR_STR_LEN];
	uint8 i, free_idx = 0;
	struct ether_addr tmp;
	bool free_idx_found = FALSE;

	BCM_REFERENCE(eabuf);
	WL_TXBF(("%s idx %d ea: %s\n", __FUNCTION__, bf_shm_idx, bcm_ether_ntoa(ea, eabuf)));

	ASSERT(ea != NULL);

	if (D11REV_LT(wlc->pub->corerev, 40)) {
		return BCME_UNSUPPORTED;
	}

	ASSERT(bf_shm_idx <= AMT_MAX_TXBF_ENTRIES);
	if (!wlc->clk)
		return BCME_NOCLK;
	/* check if there is already an entry  */
	for (i = 0; i < AMT_SIZE; i++) {
		wlc_read_amt(wlc, i, &tmp, &attr);
		if (ETHER_ISNULLADDR(&tmp) || (attr == 0)) {
			if (!free_idx_found && ((i >= wlc->txbf->amt_start_idx) &&
				(i < wlc->txbf->amt_max_idx))) {
				free_idx_found = TRUE;
				free_idx = i;
			}
			continue;
		}
		if ((memcmp((void *)&tmp, (void *)ea, sizeof(struct ether_addr)) == 0)) {
			WL_TXBF(("%s, amt entry found at idx%d\n", __FUNCTION__, i));
			*amt_idx = i;
			if ((attr & ((AMT_ATTR_VALID) | (AMT_ATTR_A2)))
				== ((AMT_ATTR_VALID) | (AMT_ATTR_A2))) {
				return BCME_OK;
			}
			if (!(attr & AMT_ATTR_VALID)) {
				attr = (AMT_ATTR_VALID | AMT_ATTR_A2);
				wlc_bmac_write_amt(wlc->hw, i, ea, attr);
				return BCME_OK;
			}

			if (!(attr & AMT_ATTR_A2)) {
				attr |= AMT_ATTR_A2;
				wlc_bmac_write_amt(wlc->hw, i, ea, attr);
				return BCME_OK;
			}
		}
	}

		/* no amt entry exist for this mac address, create a new one */
	*amt_idx = free_idx;
	ASSERT((*amt_idx) < wlc->txbf->amt_max_idx);

	attr = (AMT_ATTR_VALID | AMT_ATTR_A2);
	wlc_bmac_write_amt(wlc->hw, *amt_idx, ea, attr);
	WL_TXBF(("%s, Add amt entry %d\n", __FUNCTION__, *amt_idx));

	return BCME_OK;
}

void wlc_txfbf_update_amt_idx(wlc_txbf_info_t *txbf, int amt_idx, const struct ether_addr *addr)
{
	uint8 i, shm_idx;
	uint16 val;
	wlc_info_t *wlc;

	uint16 attr;
	char eabuf[ETHER_ADDR_STR_LEN];
	struct ether_addr tmp;
	struct scb *scb;
	struct txbf_scb_info *txbf_scb_info;
	int32 idx;
	wlc_bsscfg_t *cfg;
	struct scb *psta_scb = NULL;

	BCM_REFERENCE(eabuf);
	ASSERT(txbf);

	wlc = txbf->wlc;
	ASSERT(wlc);

	if (!TXBF_ENAB(wlc->pub)) {
		return;
	}

	if (txbf->bfr_shm_index_bitmap == 0)
		return;

	wlc_read_amt(wlc, amt_idx, &tmp, &attr);
	if ((attr & ((AMT_ATTR_VALID) | (AMT_ATTR_A2)))
		!= ((AMT_ATTR_VALID) | (AMT_ATTR_A2))) {
		return;
	}

	/* PSTA AWARE AP: Look for PSTA SCB */
	FOREACH_BSS(wlc, idx, cfg) {
		psta_scb = wlc_scbfind(wlc, cfg, addr);
		if (psta_scb != NULL)
			break;
	}

	for (i = 0; i < WLC_BEAMFORMING_MAX_LINK; i++) {
		if ((txbf->bfr_shm_index_bitmap & (1 << i)) == 0)
			continue;

		scb = txbf->bfe_scbs[i];
		ASSERT(scb);
		if (!scb)
			continue;
		txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);
		ASSERT(txbf_scb_info);
		if (!txbf_scb_info) {
			WL_ERROR(("wl:%d %s update amt %x for %s failed\n",
				wlc->pub->unit, __FUNCTION__, amt_idx,
				bcm_ether_ntoa(addr, eabuf)));
			return;
		}

		if ((eacmp(&txbf->bfe_scbs[i]->ea, addr) == 0) ||
			((psta_scb != NULL) && (txbf->bfe_scbs[i] == psta_scb->psta_prim))) {
			shm_idx = txbf_scb_info->shm_index;
			val = wlc_read_shm(wlc, (M_AMT_INFO_BLK + amt_idx * 2));
			val &= (~BF_AMT_MASK);
			val |= (BF_AMT_BFM_ENABLED | (shm_idx << BF_AMT_BLK_IDX_SHIFT));
			wlc_write_shm(wlc, (M_AMT_INFO_BLK + amt_idx * 2), val);
			txbf_scb_info->enable = TRUE;
			WL_TXBF(("update amt idx %d %s for shm idx %d, %x\n", amt_idx,
				bcm_ether_ntoa(addr, eabuf), shm_idx, val));
			return;
		}
	}
}

static int
wlc_txbf_doiovar(void *hdl, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *p, uint plen, void *a, int alen, int vsize, struct wlc_if *wlcif)
{
	wlc_txbf_info_t *txbf = (wlc_txbf_info_t *)hdl;
	wlc_info_t *wlc = txbf->wlc;

	int32 int_val = 0;
	bool bool_val;
	uint32 *ret_uint_ptr;
	int32 *ret_int_ptr;
	int err = 0;

	if (plen >= (int)sizeof(int_val))
		bcopy(p, &int_val, sizeof(int_val));

	bool_val = (int_val != 0) ? TRUE : FALSE;
	ret_uint_ptr = (uint32 *)a;
	ret_int_ptr = (int32 *)a;

	if (!TXBF_ENAB(wlc->pub))
		return BCME_UNSUPPORTED;

	switch (actionid) {
	case IOV_GVAL(IOV_TXBF_ENAB):
		*ret_int_ptr = (int32)(txbf->bfr_capable & txbf->enable);
		break;

	case IOV_SVAL(IOV_TXBF_ENAB):
		if (txbf->bfr_capable) {
			/* check if spatial policy is turned on for all subbands, if
			 * any subband has spatial policy turned off,  don't enable TxBF.
			 */
			if (wlc_stf_spatial_policy_ismin(wlc) && bool_val) {
				WL_ERROR(("%s: Cannot enable TXBF, spatial policy is turned off\n",
					__FUNCTION__));
				return BCME_EPERM;
			}
			txbf->enable = bool_val;
			if (txbf->enable == FALSE) {
				txbf->active = FALSE;
				WL_TXBF(("%s:TxBF deactived\n", __FUNCTION__));
			} else if (txbf->enable && (WLC_BITSCNT(wlc->stf->txchain) > 1) &&
				txbf->bfr_shm_index_bitmap) {
				txbf->active = TRUE;
				WL_TXBF(("TxBF actived \n"));
			}

			/* invalid tx header cache */
			if (WLC_TXC_ENAB(wlc))
				wlc->txcgen++;

		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_GVAL(IOV_TXBF_BFR_CAP):
		*ret_int_ptr = (int32)txbf->bfr_capable;
		break;

	case IOV_SVAL(IOV_TXBF_BFR_CAP):
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			txbf->bfr_capable = bool_val;
#ifdef WL11AC
			wlc_vht_update_bfr_bfe(wlc);
#endif
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;

	case IOV_GVAL(IOV_TXBF_BFE_CAP):
		*ret_int_ptr = (int32)txbf->bfe_capable;
		break;

	case IOV_SVAL(IOV_TXBF_BFE_CAP):
		if (D11REV_GE(wlc->pub->corerev, 40)) {
			txbf->bfe_capable = bool_val;
#ifdef WL11AC
			wlc_vht_update_bfr_bfe(wlc);
#endif
		} else {
			err = BCME_UNSUPPORTED;
		}
		break;


	case IOV_GVAL(IOV_TXBF_TIMER):
		if (txbf->sounding_period == 0)
			*ret_uint_ptr = (uint32) -1; /* -1 auto */
		else if (txbf->sounding_period == BF_SOUND_PERIOD_DISABLED)
			*ret_uint_ptr = 0; /* 0: disabled */
		else
			*ret_uint_ptr = (uint32)txbf->sounding_period * 4/ 1000;
		break;

	case IOV_SVAL(IOV_TXBF_TIMER):
		if (int_val == -1) /* -1 auto */
			txbf->sounding_period = 0;
		else if (int_val == 0) /* 0: disabled */
			txbf->sounding_period = BF_SOUND_PERIOD_DISABLED;
		else if ((int_val < BF_SOUND_PERIOD_MIN) || (int_val > BF_SOUND_PERIOD_MAX))
			return BCME_BADARG;
		else
			txbf->sounding_period = (uint16)int_val * (1000 / 4);
		wlc_write_shm(wlc, (txbf->shm_base + M_BFI_REFRESH_THR_OFFSET) * 2,
			txbf->sounding_period);
		break;

	case IOV_GVAL(IOV_TXBF_RATESET): {
		int i;
		wl_txbf_rateset_t *ret_rs = (wl_txbf_rateset_t *)a;

		/* Copy MCS rateset */
		bcopy(txbf->txbf_rate_mcs, ret_rs->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
		bcopy(txbf->txbf_rate_mcs_bcm, ret_rs->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
		/* Copy VHT rateset */
		for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
			ret_rs->txbf_rate_vht[i] = txbf->txbf_rate_vht[i];
			ret_rs->txbf_rate_vht_bcm[i] = txbf->txbf_rate_vht_bcm[i];
		}
		/* Copy OFDM rateset */
		ret_rs->txbf_rate_ofdm_cnt = txbf->txbf_rate_ofdm_cnt;
		bcopy(txbf->txbf_rate_ofdm, ret_rs->txbf_rate_ofdm, txbf->txbf_rate_ofdm_cnt);
		ret_rs->txbf_rate_ofdm_cnt_bcm = txbf->txbf_rate_ofdm_cnt_bcm;
		bcopy(txbf->txbf_rate_ofdm_bcm, ret_rs->txbf_rate_ofdm_bcm,
			txbf->txbf_rate_ofdm_cnt_bcm);
		break;
	}

	case IOV_SVAL(IOV_TXBF_RATESET): {
		int i;
		wl_txbf_rateset_t *in_rs = (wl_txbf_rateset_t *)a;

		/* Copy MCS rateset */
		bcopy(in_rs->txbf_rate_mcs, txbf->txbf_rate_mcs, TXBF_RATE_MCS_ALL);
		bcopy(in_rs->txbf_rate_mcs_bcm, txbf->txbf_rate_mcs_bcm, TXBF_RATE_MCS_ALL);
		/* Copy VHT rateset */
		for (i = 0; i < TXBF_RATE_VHT_ALL; i++) {
			txbf->txbf_rate_vht[i] = in_rs->txbf_rate_vht[i];
			txbf->txbf_rate_vht_bcm[i] = in_rs->txbf_rate_vht_bcm[i];
		}
		/* Copy OFDM rateset */
		txbf->txbf_rate_ofdm_cnt = in_rs->txbf_rate_ofdm_cnt;
		bcopy(in_rs->txbf_rate_ofdm, txbf->txbf_rate_ofdm, in_rs->txbf_rate_ofdm_cnt);
		txbf->txbf_rate_ofdm_cnt_bcm = in_rs->txbf_rate_ofdm_cnt_bcm;
		bcopy(in_rs->txbf_rate_ofdm_bcm, txbf->txbf_rate_ofdm_bcm,
			in_rs->txbf_rate_ofdm_cnt_bcm);
		break;
	}

	default:
		WL_ERROR(("wl%d %s %x not supported\n", wlc->pub->unit, __FUNCTION__, actionid));
		return BCME_UNSUPPORTED;
	}
	return err;
}

void wlc_txbf_pkteng_tx_start(wlc_txbf_info_t *txbf, struct scb *scb)
{

	uint16 val;
	wlc_info_t * wlc;
	struct txbf_scb_info *txbf_scb_info;

	wlc = txbf->wlc;
	ASSERT(wlc);
	txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("%s failed!\n", __FUNCTION__));
		return;
	}

	wlc_suspend_mac_and_wait(wlc);
	/* borrow shm block 0 for pkteng */
	val = wlc_read_shm(wlc, (txbf->shm_base + C_BFI_BFRIDX_POS) * 2);
	/* fake valid bit */
	val |= (1 << 8);
	wlc_write_shm(wlc, (txbf->shm_base + C_BFI_BFRIDX_POS) * 2, val);
	wlc_enable_mac(wlc);

	txbf_scb_info->shm_index = 0;
	txbf_scb_info->enable = TRUE;

	if (!txbf->active)
		txbf->active  = 1;

}

void wlc_txbf_pkteng_tx_stop(wlc_txbf_info_t *txbf, struct scb *scb)
{
	wlc_info_t * wlc;
	uint16 val;
	struct txbf_scb_info *txbf_scb_info;

	wlc = txbf->wlc;
	ASSERT(wlc);

	txbf_scb_info = (struct txbf_scb_info *)TXBF_SCB_INFO(txbf, scb);

	ASSERT(txbf_scb_info);
	if (!txbf_scb_info) {
		WL_ERROR(("%s failed!\n", __FUNCTION__));
		return;
	}
	txbf_scb_info->enable = FALSE;

	/* clear the valid bit */
	wlc_suspend_mac_and_wait(wlc);
	val = wlc_read_shm(wlc, (txbf->shm_base + C_BFI_BFRIDX_POS) * 2);
	val &= (~(1 << 8));
	wlc_write_shm(wlc, (txbf->shm_base + C_BFI_BFRIDX_POS) * 2, val);
	wlc_enable_mac(wlc);

	if ((txbf->bfr_shm_index_bitmap == 0) && txbf->active)
		txbf->active  = 0;
}

#endif /*  WL_BEAMFORMING */
