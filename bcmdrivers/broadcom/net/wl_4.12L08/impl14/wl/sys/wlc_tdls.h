/*
 * TDLS(Tunnel Direct Link Setup) related header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_tdls.h,v 1.1.2.4 2011-02-04 17:33:49 $
*/

#ifndef _wlc_tdls_h_
#define _wlc_tdls_h_

#define TDLS_PAYLOAD_TYPE		2
#define TDLS_PAYLOAD_TYPE_LEN	1

/* TDLS Action Category code */
#define TDLS_ACTION_CATEGORY_CODE		12
/* Wi-Fi Display (WFD) Vendor Specific Category */
/* used for WFD Tunneled Probe Request and Response */
#define TDLS_VENDOR_SPECIFIC				127

/* TDLS Action Field Values */
#define TDLS_SETUP_REQ					0
#define TDLS_SETUP_RESP					1
#define TDLS_SETUP_CONFIRM				2
#define TDLS_TEARDOWN					3
#define TDLS_PEER_TRAFFIC_IND			4
#define TDLS_CHANNEL_SWITCH_REQ			5
#define TDLS_CHANNEL_SWITCH_RESP		6
#define TDLS_PEER_PSM_REQ				7
#define TDLS_PEER_PSM_RESP				8
#define TDLS_PEER_TRAFFIC_RESP			9
#define TDLS_DISCOVERY_REQ				10

/* 802.11z TDLS Public Action Frame action field */
#define TDLS_DISCOVERY_RESP				14

/* This marks the start of a packed structure section. */
#include <packed_section_start.h>

/* 802.11z TDLS Public Action Frame */
BWL_PRE_PACKED_STRUCT struct tdls_pub_act_frame {
	uint8	category;	/* DOT11_ACTION_CAT_PUBLIC */
	uint8	action;		/* TDLS_DISCOVERY_RESP */
	uint8	dialog_token;
	uint16	cap;		/* TDLS capabilities */
	uint8	elts[1];	/* Variable length information elements.  Max size =
				* ACTION_FRAME_SIZE - sizeof(this structure) - 1
				*/
} BWL_POST_PACKED_STRUCT;
typedef struct tdls_pub_act_frame tdls_pub_act_frame_t;
#define TDLS_PUB_AF_FIXED_LEN	4
/* This marks the end of a packed structure section. */
#include <packed_section_end.h>

#ifdef WLTDLS
#define TDLS_PRHEX(m, b, n)	do {if (WL_TDLS_ON()) prhex(m, b, n);} while (0)

extern tdls_info_t *wlc_tdls_attach(wlc_info_t *wlc);
extern void wlc_tdls_detach(tdls_info_t *tdls);
extern bool wlc_tdls_cap(tdls_info_t *tdls);

extern bool wlc_tdls_buffer_sta_enable(tdls_info_t *tdls);
extern bool wlc_tdls_sleep_sta_enable(tdls_info_t *tdls);
extern void wlc_tdls_update_tid_seq(tdls_info_t *tdls, struct scb *scb, uint8 tid, uint16 seq);
extern void wlc_tdls_return_to_base_ch_on_eosp(tdls_info_t *tdls, struct scb *scb);
extern void wlc_tdls_rcv_data_frame(tdls_info_t *tdls, struct scb *scb, d11rxhdr_t *rxhdr);
extern void wlc_tdls_rcv_action_frame(tdls_info_t *tdls, struct scb *scb, struct wlc_frminfo *f,
	uint pdata_offset);
extern bool wlc_tdls_recvfilter(tdls_info_t *tdls, struct scb *scb);
extern void wlc_tdls_process_discovery_resp(tdls_info_t *tdls, struct dot11_management_header *hdr,
	uint8 *body, int body_len, int8 rssi);
extern int wlc_tdls_set(tdls_info_t *tdls, bool on);
extern void wlc_tdls_cleanup(tdls_info_t *tdls, wlc_bsscfg_t *parent);
extern void wlc_tdls_free_scb(tdls_info_t *tdls, struct scb *scb);
extern struct scb *wlc_tdls_query(tdls_info_t *tdls, wlc_bsscfg_t *parent, void *p,
	struct ether_addr *ea);
extern void wlc_tdls_port_open(tdls_info_t *tdls, struct ether_addr *ea);
extern wlc_bsscfg_t *wlc_tdls_get_parent_bsscfg(wlc_info_t *wlc, struct scb *scb);
extern void wlc_tdls_update_pm(tdls_info_t *tdls, wlc_bsscfg_t *bsscfg, uint txstatus);
extern void wlc_tdls_notify_pm_state(tdls_info_t *tdls, wlc_bsscfg_t *parent, bool state);
extern int wlc_tdls_down(void *hdl);
extern int wlc_tdls_up(void *hdl);
extern void wlc_tdls_send_pti(tdls_info_t *tdls, struct scb *scb);
extern void wlc_tdls_apsd_usp_end(tdls_info_t *tdls, struct scb *scb);
extern uint wlc_tdls_apsd_usp_interval(tdls_info_t *tdls, struct scb *scb);
extern bool wlc_tdls_stay_awake(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_tdls_do_chsw(tdls_info_t *tdls, wlc_bsscfg_t *bsscfg, bool off_ch);
extern uint16 wlc_tdls_get_pretbtt_time(tdls_info_t *tdls);
extern bool wlc_tdls_quiet_down(tdls_info_t *tdls);
#else	/* stubs */
#define TDLS_PRHEX(m, b, n)	do {} while (0)

#define wlc_tdls_attach(a) (dpt_info_t *)0x0dadbeef
#define	wlc_tdls_detach(a) do {} while (0)
#define	wlc_tdls_cap(a) FALSE
#define wlc_tdls_buffer_sta_enable(a)	FALSE
#define wlc_tdls_sleep_sta_enable(a)	FALSE
#define wlc_tdls_update_tid_seq(a, b, c, d) do {} while (0)
#define wlc_tdls_return_to_base_ch_on_eosp(a, b) do {} while (0)
#define wlc_tdls_rcv_data_frame(a, b, c) do {} while (0)
#define wlc_tdls_recvfilter(a, b) FALSE
#define	wlc_tdls_rcv_action_frame(a, b, c, d) do {} while (0)
#define wlc_tdls_process_discovery_resp(a, b, c, d, e) do {} while (0)
#define wlc_tdls_set(a, b) do {} while (0)
#define wlc_tdls_cleanup(a, b) do {} while (0)
#define wlc_tdls_free_scb(a, b) do {} while (0)
#define wlc_tdls_query(a, b, c, d) NULL
#define wlc_tdls_port_open(a, b) NULL
#define wlc_tdls_get_parent_bsscfg(a, b) NULL
#define wlc_tdls_update_pm(a, b, c) do {} while (0)
#define wlc_tdls_notify_pm_state(a, b, c) do {} while (0)
#define wlc_tdls_down(a) do {} while (0)
#define wlc_tdls_on(a) do {} while (0)
#define wlc_tdls_send_pti(a, b) do {} while (0)
#define wlc_tdls_apsd_usp_end(a, b) do {} while (0)
#define wlc_tdls_apsd_usp_interval(a, b) 0
#define wlc_tdls_stay_awake(a, b) FALSE
#define wlc_tdls_do_chsw(a, b, c) do {} while (0)
#define wlc_tdls_get_pretbtt_time(a) 0
#define wlc_tdls_quiet_down(a) FALSE
#endif /* WLTDLS */

#endif /* _wlc_tdls_h_ */
