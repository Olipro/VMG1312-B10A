/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * Beamforming support
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
 */
#ifndef _wlc_txbf_h_
#define _wlc_txbf_h_


#define TXBF_RATE_OFDM_DFT	0	/* disable txbf for all OFDM rate */
#define TXBF_RATE_MCS_DFT	0xFF	/* enable mcs 0-7 */
#define TXBF_RATE_VHT_DFT	0x3FF	/* enable vht 0-9 */

struct wlc_txbf_info {
	wlc_info_t		*wlc;		/* pointer to main wlc structure */
	wlc_pub_t		*pub;		/* public common code handler */
	osl_t			*osh;		/* OSL handler */
	int	scb_handle; 	/* scb cubby handle to retrieve data from scb */
	uint8 bfr_capable;
	uint8 bfe_capable;
	uint8 enable;
	uint8 active;
	uint8 bfr_shm_index_bitmap;
	struct scb *bfe_scbs[WLC_BEAMFORMING_MAX_LINK];
	uint16	shm_base;
	uint16	sounding_period;
	uint16	amt_max_idx;
	uint16	amt_start_idx;
	uint8	txbf_rate_mcs[TXBF_RATE_MCS_ALL];	/* one for each stream */
	uint8	txbf_rate_mcs_bcm[TXBF_RATE_MCS_ALL];	/* one for each stream */
	uint16	txbf_rate_vht[TXBF_RATE_VHT_ALL];	/* one for each stream */
	uint16	txbf_rate_vht_bcm[TXBF_RATE_VHT_ALL];	/* one for each stream */
	uint8	txbf_rate_ofdm[TXBF_RATE_OFDM_ALL];	/* bitmap of ofdm rates that enables txbf */
	uint8	txbf_rate_ofdm_bcm[TXBF_RATE_OFDM_ALL]; /* bitmap of ofdm rates that enables txbf */
	uint8	txbf_rate_ofdm_cnt;
	uint8	txbf_rate_ofdm_cnt_bcm;
};

#ifdef WL_BEAMFORMING
#define TXBF_ACTIVE(wlc) (TXBF_ENAB((wlc)->pub) && ((wlc)->txbf->enable) &&((wlc)->txbf->active))
#else
#define TXBF_ACTIVE(wlc) (0)
#endif /* WL_BEAMFORMING */

extern wlc_txbf_info_t *wlc_txbf_attach(wlc_info_t *wlc);
extern void wlc_txbf_detach(wlc_txbf_info_t *txbf);
extern void wlc_txbf_init(wlc_txbf_info_t *txbf);
extern void wlc_txbf_delete_link(wlc_txbf_info_t *txbf, struct scb *scb);
extern int wlc_txbf_init_link(wlc_txbf_info_t *txbf, struct scb *scb);
extern void wlc_txbf_sounding_clean_cache(wlc_txbf_info_t *txbf);
extern bool wlc_txbf_check(wlc_txbf_info_t *txbf, ratespec_t rspec, struct scb *scb,
	uint8 *shm_index);
extern void wlc_txbf_txchain_set(wlc_txbf_info_t *txbf);
extern void wlc_txbf_txchain_upd(wlc_txbf_info_t *txbf);
extern void wlc_txbf_rxchain_upd(wlc_txbf_info_t *txbf);
extern void wlc_txfbf_update_amt_idx(wlc_txbf_info_t *txbf, int idx, const struct ether_addr *addr);
extern void wlc_txbf_pkteng_tx_start(wlc_txbf_info_t *txbf, struct scb *scb);
extern void wlc_txbf_pkteng_tx_stop(wlc_txbf_info_t *txbf, struct scb *scb);
extern void wlc_txbf_update_vht_cap(wlc_txbf_info_t *txbf, struct scb *scb, uint32 vht_cap_info);
#endif /* _wlc_txbf_h_ */
