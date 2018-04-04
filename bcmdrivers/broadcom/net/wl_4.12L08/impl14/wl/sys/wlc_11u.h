/*
 * 802.11u module header file (interworking protocol)
 *
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

#ifndef _wlc_11u_h_
#define _wlc_11u_h_

#include <bcmutils.h>
/* APIs */
#ifdef WL11U
/* 802.11u Interworking(IW) IE size */
#define IW_IE_MAX_SIZE 				11
/* 802.11u IW Advertisement Protocol IE size */
#define IWAP_IE_MAX_SIZE 			4
/* Access network type offset in IW IE */
#define IW_ANT_OFFSET				0
#define IW_LEN						9
#define IW_HESSID_OFFSET			3
#define SHORT_IW_HESSID_OFFSET	1
#define IWAP_QUERY_INFO_SIZE		1

/* module */
extern bool wlc_11u_check_probe_req_iw(void *handle, wlc_bsscfg_t *cfg,
	wlc_d11rxhdr_t *wrxh, uint8 *plcp, struct dot11_management_header *hdr,
	uint8 *body, int body_len, bool *psendProbeResp);

extern wlc_11u_info_t *wlc_11u_attach(wlc_info_t *wlc);
extern void wlc_11u_detach(wlc_11u_info_t *m11u);
extern bool wlc_11u_iw_activated(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg);
extern uint8 *wlc_11u_get_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 ie_type);
extern int wlc_11u_set_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 *ie_data,
	bool *bcn_upd, bool *prbresp_upd);
extern uint8 *wlc_11u_write_ie_beacon(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg,
	uint8 *cp, uint8 *bufend);
extern uint8 *wlc_11u_write_iw_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 *pbody);
extern int wlc_11u_iw_ie_len(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg);
extern bool wlc_11u_is_11u_ie(wlc_11u_info_t *m11u, uint8 ie_type);
#define wlc_11u_write_ie_prbreq wlc_11u_write_iw_ie
#define wlc_11u_write_ie_assocreq wlc_11u_write_iw_ie
#define wlc_11u_ie_len_prbreq wlc_11u_iw_ie_len
#define wlc_11u_ie_len_assocreq wlc_11u_iw_ie_len
#else /* !WL11U */
static INLINE wlc_11u_info_t *wlc_11u_attach(wlc_info_t *wlc)
{
	return NULL;
}
static INLINE void wlc_11u_detach(wlc_11u_info_t *m11u)
{
	return;
}
static INLINE bool wlc_11u_iw_activated(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg)
{
	return FALSE;
}
static INLINE uint8 *wlc_11u_get_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 ie_type)
{
	return NULL;
}
static INLINE int wlc_11u_set_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 *ie_data,
	bool *bcn_upd, bool *prbresp_upd)
{
	return BCME_UNSUPPORTED;
}
static INLINE uint8 *wlc_11u_write_ie_beacon(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg,
	uint8 *cp, uint8 *bufend)
{
	return cp;
}
static INLINE uint8 *wlc_11u_write_iw_ie(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 *pbody)
{
	return pbody;
}
static INLINE uint8 *wlc_11u_write_ie_prbreq(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg, uint8 *pbody)
{
	return pbody;
}
static INLINE uint8 *wlc_11u_write_ie_assocreq(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg,
	uint8 *pbody)
{
	return pbody;
}
static INLINE int wlc_11u_iw_ie_len(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg)
{
	return 0;
}
static INLINE int wlc_11u_ie_len_prbreq(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg)
{
	return 0;
}
static INLINE int wlc_11u_ie_len_assocreq(wlc_11u_info_t *m11u, wlc_bsscfg_t *cfg)
{
	return 0;
}
static INLINE bool wlc_11u_is_11u_ie(wlc_11u_info_t *m11u, uint8 ie_type)
{
	return FALSE;
}
#endif /* !WL11U */

#endif /* _wlc_11u_h_ */
