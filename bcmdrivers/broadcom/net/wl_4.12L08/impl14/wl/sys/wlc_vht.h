/*
 * Common (OS-independent) portion of
 * Broadcom 802.11 Networking Device Driver
 *
 * VHT support
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_vht.h 365329 2012-10-29 09:26:41Z $
 */
#ifndef _wlc_vht_h_
#define _wlc_vht_h_

#include "osl.h"
#include "typedefs.h"
#include "bcmwifi_channels.h"
#include "proto/802.11.h"
#include "wlc_types.h"
#include "wlc_bsscfg.h"

/* 2.4G VHT operation is not specified
 * BRCM supports this mode in 2.4 so these IEs have to be wrapped in a proprietary IE
 * when operating in 2.4G.
 * These IEs are dependent on protocol state, so they are not always present
 * These flags tell the IE code to include this in the management frame being built
 * It takes case of the length computation needed by some of the IE processing
 * code in the state machine
 */
#define VHT_IE_DEFAULT		0x0
#define VHT_IE_PWR_ENVELOPE	0x1
#define VHT_IE_CSA_WRAPPER	0x2

extern void wlc_vht_init_defaults(wlc_info_t *wlc);
extern void wlc_vht_update_cap(wlc_info_t *wlc);
extern vht_cap_ie_t * wlc_read_vht_cap_ie(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len,
	vht_cap_ie_t* cap_ie);
extern vht_op_ie_t * wlc_read_vht_op_ie(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len,
	vht_op_ie_t* op_ie);
extern uint8 *wlc_read_vht_features_ie(wlc_info_t *wlc,  uint8 *tlvs,
	int tlvs_len, uint8 *rate_mask, int *prop_tlv_len);

extern uint wlc_vht_ie_len(wlc_bsscfg_t *cfg, int band, uint fc, uint32 flags);
extern uint8* wlc_read_ext_cap_ie(wlc_info_t *wlc, uint8* tlvs, int tlvs_len,
	uint8* ext_cap);

/* ht/vht operating mode (11ac) */
extern dot11_oper_mode_notif_ie_t * wlc_read_oper_mode_notif_ie(
	wlc_info_t *wlc, uint8 *tlvs, int tlvs_len,
	dot11_oper_mode_notif_ie_t *ie);
extern void wlc_write_oper_mode_notif_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	dot11_oper_mode_notif_ie_t *ie);
extern uint8 wlc_get_oper_mode(wlc_info_t *wlc, wlc_bsscfg_t *cfg);
extern void wlc_frameaction_vht(wlc_info_t *wlc, uint action_id,
    struct scb *scb, struct dot11_management_header *hdr,
    uint8 *body, int body_len);
void wlc_send_action_vht_oper_mode(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *ea);

extern uint8 *wlc_vht_write_vht_ies(wlc_bsscfg_t *cfg,
	int band, uint fc, uint32 flags, uint8 *start, uint16 len);

extern uint8 *wlc_vht_write_vht_brcm_ie(wlc_bsscfg_t *cfg,
	int band, uint fc, uint32 flags, uint8 *buf, uint16 len);

extern chanspec_t wlc_vht_chanspec(wlc_info_t *wlc, vht_op_ie_t *op_ie, chanspec_t ht_chanspec);
extern void wlc_vht_parse_bcn_prb(wlc_info_t *wlc, wlc_bss_info_t *bi, uint8 *tlvs, uint tlvs_len);
extern void wlc_vht_upd_rate_mcsmap(wlc_info_t *wlc, struct scb *scb, uint16 rxmcsmap);
extern void wlc_vht_update_sgi_rx(wlc_info_t *wlc, uint int_val);
extern void wlc_vht_update_bfr_bfe(wlc_info_t *wlc);
extern int wlc_beamformer_init_link(wlc_info_t *wlc, struct scb *scb);
extern void wlc_beamformer_delete_link(wlc_info_t *wlc, struct scb *scb);
extern uint8 *wlc_write_vht_transmit_power_envelope_ie(wlc_info_t *wlc,
	chanspec_t chspec, uint8 *cp, int buflen);

#endif /* _wlc_vht_h_ */
