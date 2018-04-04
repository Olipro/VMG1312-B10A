/*
 * 802.11h Quiet module header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_quiet.h 253231 2011-04-14 06:59:23Z $
*/

#ifndef _wlc_quiet_h_
#define _wlc_quiet_h_

#ifdef WLQUIET
#define BSS_QUIET_STATE(qm, cfg) (qm != NULL ? wlc_quiet_get_quiet_state(qm, cfg) : 0)
#else
#define BSS_QUIET_STATE(qm, cfg) 0
#endif

/* quiet state */
#define SILENCE		(1<<0)

/* APIs */
#ifdef WLQUIET

/* module */
extern wlc_quiet_info_t *wlc_quiet_attach(wlc_info_t *wlc);
extern void wlc_quiet_detach(wlc_quiet_info_t *qm);

/* recv/send */
#ifdef STA
extern void wlc_11h_quiet(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg,
	dot11_quiet_t *tag, struct dot11_bcn_prb *bcn);
#else
#define wlc_11h_quiet(qm, cfg, tag, bcn) do {} while (0)
#endif

/* IE build/parse */
extern uint8 *wlc_quiet_write_ie(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg,
	uint8 *cp, int len);

/* actions */
extern void wlc_quiet_reset_all(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg);
extern void wlc_quiet_do_quiet(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg, dot11_quiet_t *qie);

extern void wlc_quiet_txfifo_suspend_complete(wlc_quiet_info_t *qm);

/* accessors */
extern uint wlc_quiet_get_quiet_state(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg);
extern uint wlc_quiet_get_quiet_count(wlc_quiet_info_t *qm, wlc_bsscfg_t *cfg);

#else /* !WLQUIET */

#define wlc_quiet_attach(wlc) NULL
#define wlc_quiet_detach(qm) do {} while (0)

/* recv/send */
#define wlc_11h_quiet(qm, cfg, tag, bcn) do {} while (0)

/* IE build/parse */
#define wlc_quiet_write_ie(qm, cfg, cp, len) (cp)

/* actions */
#define wlc_quiet_reset_all(qm, cfg) do {} while (0)
#define wlc_quiet_do_quiet(qm, cfg, qie) do {} while (0)

#define wlc_quiet_txfifo_suspend_complete(qm) do {} while (0)

/* accessors */
#define wlc_quiet_get_quiet_state(qm, cfg) 0
#define wlc_quiet_get_quiet_count(qm, cfg) 0

#endif /* !WLQUIET */

#endif /* _wlc_quiet_h_ */
