/*
 * A-MPDU Tx (with extended Block Ack) related header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_ampdu.h 363425 2012-10-17 20:55:39Z $
*/


#ifndef _wlc_ampdu_tx_h_
#define _wlc_ampdu_tx_h_

typedef struct scb_ampdu_tx scb_ampdu_tx_t;
typedef struct scb_ampdu_tid_ini scb_ampdu_tid_ini_t;

extern ampdu_tx_info_t *wlc_ampdu_tx_attach(wlc_info_t *wlc);
extern void wlc_ampdu_tx_detach(ampdu_tx_info_t *ampdu_tx);
extern int wlc_sendampdu(ampdu_tx_info_t *ampdu_tx, wlc_txq_info_t *qi, void **aggp, int prec);
extern bool wlc_ampdu_dotxstatus(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
	tx_status_t *txs, wlc_txh_info_t *txh_info);
extern void wlc_ampdu_dotxstatus_regmpdu(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p,
	tx_status_t *txs);
extern void wlc_ampdu_tx_reset(ampdu_tx_info_t *ampdu_tx);
extern void wlc_ampdu_macaddr_upd(wlc_info_t *wlc);

extern uint8 wlc_ampdu_null_delim_cnt(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	ratespec_t rspec, int phylen, uint16* minbytes);
extern bool wlc_ampdu_frameburst_override(ampdu_tx_info_t *ampdu_tx);
#ifdef WLC_HIGH_ONLY
extern void wlc_ampdu_txstatus_complete(ampdu_tx_info_t *ampdu_tx, uint32 s1, uint32 s2);
#endif

extern void wlc_ampdu_agg_state_update_tx(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, bool txaggr);
extern void wlc_ampdu_mchan_ini_adjust(ampdu_tx_info_t *ampdu_tx, struct scb *scb, void *p);

extern bool wlc_ampdu_tx_cap(ampdu_tx_info_t *ampdu_tx);
extern int wlc_ampdumac_set(ampdu_tx_info_t *ampdu_tx, uint8 on);

#ifdef WLAMPDU_MAC
#define AMU_EPOCH_CHG_PLCP		0	/* epoch change due to plcp */
#define AMU_EPOCH_CHG_FID		1	/* epoch change due to rate flag in frameid */
#define AMU_EPOCH_CHG_NAGG		2	/* epoch change due to ampdu off */
#define AMU_EPOCH_CHG_MPDU		3	/* epoch change due to mpdu */
#define AMU_EPOCH_CHG_DSTTID		4	/* epoch change due to dst/tid */
#define AMU_EPOCH_CHG_SEQ		5	/* epoch change due to discontinuous seq no */

extern void wlc_ampdu_change_epoch(ampdu_tx_info_t *ampdu_tx, int fifo, int reason_code);
extern uint8 wlc_ampdu_chgnsav_epoch(ampdu_tx_info_t *, int fifo,
	int reason_code, struct scb *, uint8 tid, wlc_txh_info_t*);
extern bool wlc_ampdu_was_ampdu(ampdu_tx_info_t *, int fifo);
extern void wlc_dump_aggfifo(wlc_info_t *wlc, struct bcmstrbuf *b);
#endif /* WLAMPDU_MAC */
extern void wlc_sidechannel_init(ampdu_tx_info_t *ampdu_tx);

extern void ampdu_cleanup_tid_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	uint8 tid, bool force);

extern void scb_ampdu_tx_flush(ampdu_tx_info_t *ampdu_tx, struct scb *scb);

extern void BCMATTACHFN(wlc_ampdu_agg_state_txaggr_override)(ampdu_tx_info_t *ampdu_tx,
	int8 txaggr);

extern void wlc_ampdu_clear_tx_dump(ampdu_tx_info_t *ampdu_tx);

extern void wlc_ampdu_recv_ba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 *body,
	int body_len);
extern void wlc_ampdu_recv_addba_req_ini(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	dot11_addba_req_t *addba_req, int body_len);
extern void wlc_ampdu_recv_addba_resp(ampdu_tx_info_t *ampdu_tx, struct scb *scb,
	uint8 *body, int body_len);

#if defined(BCMDBG) || defined(WLTEST) || defined(BCMDBG_AMPDU)
extern int wlc_ampdu_tx_dump(ampdu_tx_info_t *ampdu_tx, struct bcmstrbuf *b);
#endif /* BCMDBG || WLTEST */

extern void wlc_ampdu_tx_set_mpdu_density(ampdu_tx_info_t *ampdu_tx, uint8 mpdu_density);
extern void wlc_ampdu_tx_set_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx, uint8 wsize);
extern uint8 wlc_ampdu_tx_get_ba_tx_wsize(ampdu_tx_info_t *ampdu_tx);
extern uint8 wlc_ampdu_tx_get_ba_max_tx_wsize(ampdu_tx_info_t *ampdu_tx);
extern void wlc_ampdu_tx_recv_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	uint8 category, uint16 initiator, uint16 reason);
extern void wlc_ampdu_tx_send_delba(ampdu_tx_info_t *ampdu_tx, struct scb *scb, uint8 tid,
	uint16 initiator, uint16 reason);

extern int wlc_ampdu_tx_set(ampdu_tx_info_t *ampdu_tx, bool on);

extern uint wlc_ampdu_tx_get_tcp_ack_ratio(ampdu_tx_info_t *ampdu_tx);

#ifdef MACOSX
extern uint wlc_ampdu_txpktcnt_prio(ampdu_tx_info_t *ampdu, uint prio);
#endif

extern void wlc_mchan_flush_queue(wlc_info_t *wlc, struct pktq *q);

#if defined(WLVSDB) && defined(PROP_TXSTATUS)
extern void wlc_ampdu_send_bar_cfg(ampdu_tx_info_t * ampdu, struct scb *scb);
extern void wlc_ampdu_flush_ampdu_q(ampdu_tx_info_t *ampdu, wlc_bsscfg_t *cfg);
#endif

#if defined(WLNAR)
extern uint8 BCMFASTPATH wlc_ampdu_ba_on_tidmask(const struct scb *scb);
#endif

#endif /* _wlc_ampdu_tx_h_ */
