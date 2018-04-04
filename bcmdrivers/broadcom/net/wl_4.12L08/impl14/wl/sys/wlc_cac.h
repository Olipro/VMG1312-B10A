/*
 * Call Admission Control header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cac.h 365823 2012-10-31 04:24:30Z $
 */

#ifndef _wlc_cac_h_
#define _wlc_cac_h_

/* if WLCAC is defined, function prototype are use, otherwise define NULL
 * Macro for all external functions.
 * When adding function prototype, makesure add to both places.
 */
#ifdef WLCAC
extern wlc_cac_t *wlc_cac_attach(wlc_info_t *wlc);
extern void wlc_cac_detach(wlc_cac_t *cac);
extern void wlc_cac_tspec_state_reset(wlc_cac_t *cac);
extern void wlc_cac_param_reset_all(wlc_cac_t *wlc, struct scb *scb);
extern int  wlc_cac_watchdog(void *hdl);
extern bool wlc_cac_update_used_time(wlc_cac_t *cac, int ac, int dur, struct scb *scb);
extern void wlc_cac_action_frame(wlc_cac_t *wlc, uint action_id,
	struct dot11_management_header *hdr, uint8 *body, int body_len, struct scb *scb);
extern uint32 wlc_cac_medium_time_total(wlc_cac_t *cac, struct scb *scb);
#ifdef BCMDBG
extern int wlc_dump_cac(wlc_cac_t *cac, struct bcmstrbuf *b);
#endif /* BCMDBG */
void wlc_cac_on_join_bss(wlc_cac_t *cac, wlc_bsscfg_t *cfg, struct ether_addr *bssid, bool roam);
extern bool wlc_cac_is_traffic_admitted(wlc_cac_t *cac, int ac, struct scb *scb);
extern void wlc_cac_reset_inactivity_interval(wlc_cac_t *cac, int ac, struct scb *scb);
extern void wlc_cac_handle_inactivity(wlc_cac_t *cac, int ac, struct scb *scb);
extern bool wlc_cac_is_ac_downgrade_admitted(wlc_cac_t *cac);
extern void wlc_cac_on_leave_bss(wlc_cac_t *cac);
void wlc_frameaction_cac(wlc_bsscfg_t *bsscfg, uint action_id, wlc_cac_t *cac,
	struct dot11_management_header *hdr, uint8 *body, int body_len);
#else	/* WLCAC */
#define wlc_cac_attach(a)			(wlc_cac_t *)0x0dadbeef
#define wlc_cac_detach(a)			do {} while (0)
#define wlc_cac_addts_timeout(a)		do {} while (0)
#define wlc_cac_tspec_state_reset(a)		do {} while (0)
#define wlc_cac_param_reset_all(a, b)		do {} while (0)
#define wlc_cac_watchdog(a)			do {} while (0)
#define wlc_cac_update_used_time(a, b, c, d)	(0)
#define wlc_cac_assoc_tspec(a, b, c, d, e)		do {} while (0)
#define wlc_cac_assoc_status(a, b)		(b)
#define wlc_cac_tspec_append(a, b, c, d)		(c)
#define wlc_cac_action_frame(a, b, c, d, e, f)	do {} while (0)
#define wlc_cac_medium_time_total(a, b)		(0)
#define wlc_cac_update_curr_bssid(a)	do {} while (0)
#define wlc_cac_is_traffic_admitted(a, b, c) (0)
#define wlc_cac_reset_inactivity_interval(a, b, c) do {} while (0)
#define wlc_cac_handle_inactivity(a, b, c) do {} while (0)
#define wlc_cac_is_ac_downgrade_admitted(a) do {} while (0)
#define wlc_cac_on_leave_bss(a)	do {} while (0)
#define wlc_frameaction_cac(a, b, c, d, e, f) do {} while (0)
#endif  /* WLCAC */

#ifdef WLFBT
extern uint8 *wlc_cac_write_ricreq(wlc_info_t *wlc, int *bufsize, int *ric_ie_count);
extern void wlc_cac_copy_state(wlc_cac_t *cac, struct scb *prev_scb, struct scb *scb);
#endif /* WLFBT */

#endif /* _wlc_cac_h_ */
