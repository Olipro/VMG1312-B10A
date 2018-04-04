/*
 * Proxy STA interface
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_psta.h 355514 2012-09-07 01:25:51Z $
 */

#ifndef _WLC_PSTA_H_
#define _WLC_PSTA_H_

#define	PSTA_IS_PROXY(wlc)	((wlc)->pub->_psta == PSTA_MODE_PROXY)
#define	PSTA_IS_REPEATER(wlc)	((wlc)->pub->_psta == PSTA_MODE_REPEATER)

/* Max PSTA assoc limits exclude the default primary assoc. One entry in
 * bottom half of the skl and key tables is accounted for the primary's
 * pairwise keys and one in top half is used for storing the broadcast key.
 */
#define	PSTA_PROXY_MAX_ASSOC	((RCMTA_SIZE / 2) - 1)
#define	PSTA_RPT_MAX_ASSOC	((RCMTA_SIZE / 2) - 1)

#define PSTA_MAX_ASSOC(wlc)	(PSTA_IS_REPEATER(wlc) ? PSTA_RPT_MAX_ASSOC : \
				                         PSTA_PROXY_MAX_ASSOC)

/* RCMTA engine can have a maximum of 50 entries. Reserve slot 25 for the
 * primary. Slots 26 to 49 are available for use by PSTA RAs. Slots 0 to 24
 * are available for use for the downstream STAs TAs.
 */
#define PSTA_TA_STRT_INDX	0
#define PSTA_RA_PRIM_INDX	(RCMTA_SIZE / 2)
#define PSTA_RA_STRT_INDX	((RCMTA_SIZE / 2) + 1)

/* Initialize psta private context.It returns a pointer to the
 * psta private context if succeeded. Otherwise it returns NULL.
 */
extern wlc_psta_info_t *wlc_psta_attach(wlc_info_t *wlc);

extern int32 wlc_psta_init(wlc_psta_info_t *psta, wlc_bsscfg_t *pcfg);

/* Cleanup psta private context */
extern void wlc_psta_detach(wlc_psta_info_t *psta);

/* Process frames in transmit direction */
extern int32 wlc_psta_send_proc(wlc_psta_info_t *psta, void **p, wlc_bsscfg_t **cfg);

/* Process frames in receive direction */
extern void wlc_psta_recv_proc(wlc_psta_info_t *psta, void *p, struct ether_header *eh,
                               wlc_bsscfg_t **bsscfg, wl_if_t **wlif);

extern wlc_bsscfg_t *wlc_psta_find(wlc_psta_info_t *psta, uint8 *mac);

/* Disassociate all Proxy STAs */
extern void wlc_psta_disassoc_all(wlc_psta_info_t *psta);

extern void wlc_psta_disable(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg);

extern void wlc_psta_disable_all(wlc_psta_info_t *psta);

extern void wlc_psta_deauth_client(wlc_psta_info_t *psta, struct ether_addr *addr);
extern void wlc_psta_build_ie(wlc_info_t *wlc, member_of_brcm_prop_ie_t *member_of_brcm_prop_ie);

#ifdef BCMDBG
extern int32 wlc_psta_dump(wlc_psta_info_t *psta, struct bcmstrbuf *b);
#endif /* BCMDBG */

extern uint8 wlc_psta_rcmta_idx(wlc_psta_info_t *psta, wlc_bsscfg_t *cfg);

#endif	/* _WLC_PSTA_H_ */
