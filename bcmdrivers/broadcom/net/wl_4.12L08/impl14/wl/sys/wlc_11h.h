/*
 * 802.11h module header file (top level and spectrum management)
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_11h.h 285399 2011-09-21 19:02:10Z $
*/

#ifndef _wlc_11h_h_
#define _wlc_11h_h_

/* spect_state */
#define NEED_TO_UPDATE_BCN	(1 << 0)	/* Need to decrement counter in outgoing beacon */
#define NEED_TO_SWITCH_CHANNEL	(1 << 1)	/* A channel switch is pending */
#define RADAR_SIM		(1 << 2)	/* Simulate radar detection...for testing */

/* APIs */
#ifdef WL11H

/* module */
extern wlc_11h_info_t *wlc_11h_attach(wlc_info_t *wlc);
extern void wlc_11h_detach(wlc_11h_info_t *tpc);

/* beacon proc */
extern void wlc_11h_process_beacon(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg,
	uint8 *params, int len, struct dot11_bcn_prb *bcn);

/* TBTT proc */
extern void wlc_11h_tbtt(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg);

/* IE build/parse */
extern uint8 *wlc_11h_write_sup_chan_ie(wlc_11h_info_t* m11h, wlc_bsscfg_t *cfg,
	uint8 *cp, int buflen);

/* spectrum management */
extern void wlc_recv_frameaction_specmgmt(wlc_11h_info_t *m11h, struct dot11_management_header *hdr,
	uint8 *body, int body_len, int8 rssi, ratespec_t rspec);
extern bool wlc_validate_measure_req(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg,
	struct dot11_management_header *hdr);
extern int wlc_11h_set_spect(wlc_11h_info_t *m11h, uint spect);

/* accessors */
extern uint wlc_11h_get_spect(wlc_11h_info_t *m11h);
extern void wlc_11h_set_spect_state(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg, uint mask, uint val);
extern uint wlc_11h_get_spect_state(wlc_11h_info_t *m11h, wlc_bsscfg_t *cfg);

#else /* !WL11H */

#define wlc_11h_attach(wlc) NULL
#define wlc_11h_detach(m11h) do {} while (0)

#define wlc_11h_process_beacon(m11h, cfg, params, len, bcn) do {} while (0)

#define wlc_11h_tbtt(m11h, cfg) do {} while (0)

#define wlc_11h_write_sup_chan_ie(m11h, cfg, cp, buflen) cp

#define wlc_recv_frameaction_specmgmt(m11h, hdr, body, body_len, rssi, rspec) do {} while (0)
#define wlc_validate_measure_req(m11h, cfg, hdr) FALSE
#define wlc_11h_set_spect(m11h, spect) BCME_ERROR

#define wlc_11h_get_spect(m11h) 0
#define wlc_11h_set_spect_state(m11h, cfg, mask, val) do {} while (0)
#define wlc_11h_get_spect_state(m11h, cfg) 0

#endif /* !WL11H */

#endif /* _wlc_11h_h_ */
