/*
 * Vendor IE list manipulation functions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_bsscfg.h,v 1.116.8.37.2.4 2010-11-22 11:32:54 $
 */

#ifndef _wlc_vndr_ie_list_h_
#define _wlc_vndr_ie_list_h_

#include <wlc_types.h>

/* Vendor IE definitions */
#define VNDR_IE_EL_HDR_LEN	(sizeof(void *))
#define VNDR_IE_MAX_TOTLEN	256	/* Space for vendor IEs in Beacons and Probe Responses */

struct vndr_ie_listel {
	struct vndr_ie_listel *next_el;
	vndr_ie_info_t vndr_ie_infoel;
};

int wlc_vndr_ie_buflen(const vndr_ie_buf_t *ie_buf, int len, int *bcn_ielen, int *prbrsp_ielen);

typedef bool (*vndr_ie_list_filter_fn_t)(void *arg, const vndr_ie_t *ie);
int wlc_vndr_ie_list_getlen_ext(const vndr_ie_listel_t *vndr_ie_listp,
	vndr_ie_list_filter_fn_t filter, void *arg, uint32 pktflag, int *totie);
#define wlc_vndr_ie_list_getlen(list, pktflag, totie) \
	wlc_vndr_ie_list_getlen_ext(list, NULL, NULL, pktflag, totie)
typedef bool (*vndr_ie_list_write_filter_fn_t)(void *arg, uint type, const vndr_ie_t *ie);
uint8 *wlc_vndr_ie_list_write_ext(const vndr_ie_listel_t *vndr_ie_listp,
	vndr_ie_list_write_filter_fn_t filter, void *arg, uint type, uint8 *cp,
	int buflen, uint32 pktflag);
#define wlc_vndr_ie_list_write(list, cp, buflen, pktflag) \
	wlc_vndr_ie_list_write_ext(list, NULL, NULL, -1, cp, buflen, pktflag)

vndr_ie_listel_t *wlc_vndr_ie_list_add_elem(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	uint32 pktflag, vndr_ie_t *vndr_iep);
int wlc_vndr_ie_list_add(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	const vndr_ie_buf_t *ie_buf, int len);
int wlc_vndr_ie_list_del(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	const vndr_ie_buf_t *ie_buf, int len);

void wlc_vndr_ie_list_free(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listpp);

int wlc_vndr_ie_list_set(wlc_info_t *wlc, const void *vndr_ie_setbuf,
	int vndr_ie_setbuf_len, vndr_ie_listel_t **vndr_ie_listp,
	bool *bcn_upd, bool *prbresp_upd);

int wlc_vndr_ie_list_get(wlc_info_t *wlc, const vndr_ie_listel_t *vndr_ie_listp,
	vndr_ie_buf_t *ie_buf, int len, uint32 pktflag);
vndr_ie_listel_t *wlc_vndr_ie_list_mod_elem(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	vndr_ie_listel_t *old_listel, uint32 pktflag, vndr_ie_t *vndr_iep);
int wlc_vndr_ie_list_mod_elem_by_type(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp,
	uint8 type, uint32 pktflag, vndr_ie_t *vndr_iep);
int wlc_vndr_ie_list_del_by_type(wlc_info_t *wlc, vndr_ie_listel_t **vndr_ie_listp, uint8 type);

uint8 *wlc_vndr_ie_list_find_by_type(vndr_ie_listel_t *vndr_ie_listp, uint8 type);

#endif /* _wlc_vndr_ie_list_h_ */
