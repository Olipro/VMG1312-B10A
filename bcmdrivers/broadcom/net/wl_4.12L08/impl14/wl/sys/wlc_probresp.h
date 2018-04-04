/*
 * SW probe response module header file
 * disable ucode sending probe response,
 * driver will decide whether send probe response,
 * after check the received probe request.
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
*/

#ifndef _wlc_probresp_h_
#define _wlc_probresp_h_

typedef bool (*probreq_filter_fn_t)(void *handle, wlc_bsscfg_t *cfg,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, struct dot11_management_header *hdr,
	uint8 *body, int body_len, bool *psendProbeResp);

/* APIs */
#ifdef WLPROBRESP_SW
/* module */
extern wlc_probresp_info_t *wlc_probresp_attach(wlc_info_t *wlc);
extern void wlc_probresp_detach(wlc_probresp_info_t *mprobresp);
extern int wlc_probresp_register(wlc_probresp_info_t *mprobresp, void *hdl,
	probreq_filter_fn_t filter_fn, bool p2p);
extern int wlc_probresp_unregister(wlc_probresp_info_t *mprobresp, void *hdl);
extern void wlc_probresp_recv_process_prbreq(wlc_probresp_info_t *mprobresp,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, struct dot11_management_header *hdr,
	uint8 *body, int body_len);
#else /* !WLPROBRESP_SW */
static INLINE wlc_probresp_info_t *wlc_probresp_attach(wlc_info_t *wlc)
{
	return NULL;
}
static INLINE void wlc_probresp_detach(wlc_probresp_info_t *mprobresp)
{
	return;
}
static INLINE int wlc_probresp_register(wlc_probresp_info_t *mprobresp, void *hdl,
	probreq_filter_fn_t filter_fn, bool p2p)
{
	return BCME_OK;
}
static INLINE int wlc_probresp_unregister(wlc_probresp_info_t *mprobresp, void *hdl)
{
	return BCME_OK;
}
static INLINE void wlc_probresp_recv_process_prbreq(wlc_probresp_info_t *mprobresp,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, struct dot11_management_header *hdr,
	uint8 *body, int body_len)
{
	return;
}
#endif /* !WLPROBRESP_SW */

#endif /* _wlc_probresp_h_ */
