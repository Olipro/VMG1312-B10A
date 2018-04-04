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
 * $Id: wlc_vht.c 365329 2012-10-29 09:26:41Z $
 */

#include "wlc_cfg.h"
#include "osl.h"
#include "typedefs.h"
#include "proto/802.11.h"
#include "bcmwifi_channels.h"
#include "bcmutils.h"
#include "bcmendian.h"
#include "siutils.h"
#include "wlioctl.h"
#include "wlc_pub.h"
#include "wlc_key.h"
#include "d11.h"
#include "wlc_bsscfg.h"
#include "wlc_rate.h"
#include "wlc.h"
#include "wlc_scb.h"
#include "wlc_vht.h"
#include "wlc_csa.h"
#include <wlc_txbf.h>

#define TLVLEN(len) 		((len) + TLV_HDR_LEN)
#define VHT_IE_ENCAP_NONE	0x0
#define VHT_IE_ENCAP_HDR	0x1
#define VHT_IE_ENCAP_VHTIE	0x2
#define VHT_IE_ENCAP_ALL	(VHT_IE_ENCAP_HDR | VHT_IE_ENCAP_VHTIE)

#define VHT_IE_FEATURES_HDRLEN	(sizeof(vht_features_ie_hdr_t))

/*
 * How to add VHT IEs for automatic encapsulation
 *
 * Provide a length calculating routine for IE, return length --including-- the TLV header length
 *
 * Provide a formatting routine for the IE, at minimum,
 * it must take the buffer pointer, buffer length and return the modified buffer pointer
 *
 * Add length routine to wlc_vht_ie_len() for the desired frametypes
 *
 * Add formatting routine to wlc_vht_write_vht_elements() for the desired frametypes
 *
 * If the IE is not mandatory or can be suppressed for low mem applications, add a flag to
 * vlc_vht.h to the VHT_IE_XXX group of defines
 */
void
wlc_vht_init_defaults(wlc_info_t *wlc)
{
	uint i;
	uint32 cap;
#ifdef WL11N_STBC_RX_ENABLED
	uint32 rx_stbc_Nss;
#endif /* WL11N_STBC_RX_ENABLED */
	ASSERT(wlc);

	/* VHT Capabilites IE (802.11 sec 8.4.2.160) */
	cap = 0;


	/* AMPDU limit for AMSDU agg, support max 11k */
	cap |= VHT_CAP_MPDU_MAX_11K;

	/* Support 20/40/80 MHz */
	cap |= VHT_CAP_CHAN_WIDTH_SUPPORT_MANDATORY;

	cap |= VHT_CAP_INFO_SGI_80MHZ;
	cap |= VHT_CAP_INFO_LDPC;
#ifdef WL11N_STBC_RX_ENABLED
	if (wlc->stf->txstreams > 1)
		cap |= VHT_CAP_INFO_TX_STBC;
	/* calculate Rx STBC support as (rx_chain / 2)
	 *
	 * 1   Chain  -> No STBC
	 * 2-3 Chains -> STBC on Nss = 1 (Nsts = 2)
	 * 4-5 Chains -> STBC on Nss = 2 (Nsts = 4)
	 * 6-7 Chains -> STBC on Nss = 3 (Nsts = 6)
	 * 8   Chains -> STBC on Nss = 4 (Nsts = 8)
	 */
	rx_stbc_Nss = WLC_BITSCNT(wlc->stf->hw_rxchain) / 2;
	cap |= (rx_stbc_Nss << VHT_CAP_INFO_RX_STBC_SHIFT);
#endif /* WL11N_STBC_RX_ENABLED */

	/* AMPDU length limit, support max 1MB (2 ^ (13 + 7)) */
	cap |= (7 << VHT_CAP_INFO_AMPDU_MAXLEN_EXP_SHIFT);

#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub)) {
		if (wlc->txbf->bfr_capable)
			cap |= VHT_CAP_INFO_SU_BEAMFMR;

		if (wlc->txbf->bfe_capable)
			cap |= VHT_CAP_INFO_SU_BEAMFMEE;

		/*
		 * Beamformee's capability of max. beamformer's antenas it can support
		 * should be 2 for brcm AC device
		 */
		if (wlc->txbf->bfe_capable | wlc->txbf->bfr_capable) {
			cap |= (2 << VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT);
			cap |= ((wlc->stf->txstreams - 1) << VHT_CAP_INFO_NUM_SOUNDING_DIM_SHIFT);
			cap |= (3 << VHT_CAP_INFO_LINK_ADAPT_CAP_SHIFT); /* both */
		}
	}
#endif /* WL_BEAMFORMING */
	wlc->vht_cap.vht_cap_info = cap;

	/* Fill out the Rx and Tx MCS support */
	for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
		uint8 mcs;

		/* Declare MCS support for all tx chains configured */
		if (i <= WLC_BITSCNT(wlc->stf->hw_txchain)) {
			if (BCM256QAM_DSAB(wlc))
				mcs = VHT_CAP_MCS_MAP_0_7;
			else {
				mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i,
					wlc->default_bss->rateset.vht_mcsmap);
			}
		} else {
			mcs = VHT_CAP_MCS_MAP_NONE;
		}

		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, wlc->vht_cap.rx_mcs_map);
		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, wlc->vht_cap.tx_mcs_map);
	}

	WL_TRACE(("%s: cap info=%04x\n", __FUNCTION__, wlc->vht_cap.vht_cap_info));

	wlc->default_bss->vht_capabilities = wlc->vht_cap.vht_cap_info;
	wlc->default_bss->vht_rxmcsmap = wlc->vht_cap.rx_mcs_map;
	wlc->default_bss->vht_txmcsmap = wlc->vht_cap.tx_mcs_map;

	WL_RATE(("%s: rx mcsmap (IE) %04x, tx %04x (hwchain %d)\n", __FUNCTION__,
		wlc->default_bss->vht_rxmcsmap,
		wlc->default_bss->vht_rxmcsmap,
		wlc->stf->hw_txchain));
}

void
wlc_vht_update_cap(wlc_info_t *wlc)
{
	uint i;
	uint8 mcs;

	wlc->vht_cap.tx_mcs_map = wlc->vht_cap.rx_mcs_map = 0;

	for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {

		/* Set up tx mcs map according to number of tx chains */
		if (i <= WLC_BITSCNT(wlc->stf->txchain)) {
			mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, wlc->default_bss->rateset.vht_mcsmap);
		} else {
			mcs = VHT_CAP_MCS_MAP_NONE;
		}
		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, wlc->vht_cap.tx_mcs_map);

		/* Set up rx mcs map according to number of rx chains */
		if (i <= WLC_BITSCNT(wlc->stf->rxchain)) {
			mcs = VHT_MCS_MAP_GET_MCS_PER_SS(i, wlc->default_bss->rateset.vht_mcsmap);
		} else {
			mcs = VHT_CAP_MCS_MAP_NONE;
		}
		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, wlc->vht_cap.rx_mcs_map);
	}

	WL_RATE(("%s: defmap %04x rx_mcs_map %04x tx_mcs_map %04x\n",
		__FUNCTION__, wlc->default_bss->rateset.vht_mcsmap,
		wlc->vht_cap.rx_mcs_map, wlc->vht_cap.tx_mcs_map));

	/* update beacon/probe resp for AP */
	if (wlc->pub->up && AP_ENAB(wlc->pub) && wlc->pub->associated) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}

/* copy ie int passed in struct, then fix endianness  */
vht_cap_ie_t *
wlc_read_vht_cap_ie(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len, vht_cap_ie_t* cap_ie)
{
	bcm_tlv_t *cap_ie_tlv;

	ASSERT(cap_ie);

	cap_ie_tlv = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_VHT_CAP_ID);
	if (cap_ie_tlv) {
		if (cap_ie_tlv->len >= VHT_CAP_IE_LEN) {
			bcopy(&cap_ie_tlv->data, cap_ie, VHT_CAP_IE_LEN);
			cap_ie->vht_cap_info = htol32(cap_ie->vht_cap_info);

			cap_ie->rx_mcs_map = htol16(cap_ie->rx_mcs_map);
			cap_ie->rx_max_rate = htol16(cap_ie->rx_max_rate);
			cap_ie->tx_mcs_map = htol16(cap_ie->tx_mcs_map);
			cap_ie->tx_max_rate = htol16(cap_ie->tx_max_rate);

			return cap_ie;
		} else {
			WL_ERROR(("wl%d: %s: std len %d does not match %d\n",
			          wlc->pub->unit, __FUNCTION__, cap_ie_tlv->len, VHT_CAP_IE_LEN));
		}
	}

	return NULL;
}

vht_op_ie_t *
wlc_read_vht_op_ie(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len, vht_op_ie_t* op_ie)
{
	bcm_tlv_t *op_ie_tlv;

	op_ie_tlv = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_VHT_OPERATION_ID);
	if (op_ie_tlv) {
		if (op_ie_tlv->len >= VHT_OP_IE_LEN)
			return (vht_op_ie_t *)&op_ie_tlv->data;
		else
			WL_ERROR(("wl%d: %s: std len %d does not match %d\n",
			          wlc->pub->unit, __FUNCTION__, op_ie_tlv->len, VHT_OP_IE_LEN));
	}

	return NULL;
}

uint8 *wlc_read_vht_features_ie(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len,
	uint8 *rate_mask, int *prop_tlv_len)
{
	bcm_tlv_t *elt = NULL;
	vht_features_ie_hdr_t *features_ie = NULL;
	uint8 *prop_tlvs = NULL;
	uint8 ouitype = BRCM_VHT_FEATURES_OUITYPE;

	elt = wlc_find_vendor_ie(tlvs, tlvs_len, BRCM_PROP_OUI, &ouitype, sizeof(ouitype));
	if (elt) {
		features_ie = (vht_features_ie_hdr_t *)&elt->data;
		*rate_mask = features_ie->rate_mask;
		if (elt->len > (VHT_IE_FEATURES_HDRLEN + 2)) {
			prop_tlvs = (uint8 *)&features_ie[1];
			/* Note elt->len does not include the 2 byte IE header */
			*prop_tlv_len = elt->len - VHT_IE_FEATURES_HDRLEN;
		}
	}
	return prop_tlvs;

}


dot11_oper_mode_notif_ie_t *
wlc_read_oper_mode_notif_ie(wlc_info_t *wlc, uint8 *tlvs, int tlvs_len,
	dot11_oper_mode_notif_ie_t *ie)
{
	bcm_tlv_t *tlv;
	tlv = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_OPER_MODE_NOTIF_ID);
	if (tlv) {
		if (tlv->len >= DOT11_OPER_MODE_NOTIF_IE_LEN) {
			memcpy(ie, tlv->data, DOT11_OPER_MODE_NOTIF_IE_LEN);
			return ie;
		} else {
			WL_INFORM(("wl%d: %s: Operating Mode Notification IE len"
				" %d bytes, expected %d bytes\n",
				wlc->pub->unit, __FUNCTION__, tlv->len,
				DOT11_OPER_MODE_NOTIF_IE_LEN));
			WLCNTINCR(wlc->pub->_cnt->rxbadproto);
		}
	}
	return NULL;
}

/* see 802.11ac D3.0 - 8.4.1.50 */
void
wlc_write_oper_mode_notif_ie(wlc_info_t *wlc, wlc_bsscfg_t *cfg,
	dot11_oper_mode_notif_ie_t *ie)
{
	ie->mode = wlc_get_oper_mode(wlc, cfg);
}

uint8* wlc_read_ext_cap_ie(wlc_info_t *wlc, uint8* tlvs, int tlvs_len,
	uint8* ext_cap)
{
	bcm_tlv_t *tlv;
	tlv = bcm_parse_tlvs(tlvs, tlvs_len, DOT11_MNG_EXT_CAP_ID);
	if (tlv) {
		if (tlv->len != 0) {
			memset(ext_cap, 0, DOT11_EXTCAP_LEN_MAX);
			memcpy(ext_cap, tlv->data, MIN(tlv->len, DOT11_EXTCAP_LEN_MAX));
			return ext_cap;
		} else {
			WL_INFORM(("wl%d: %s: ext cap length is 0\n",
				wlc->pub->unit, __FUNCTION__));
			WLCNTINCR(wlc->pub->_cnt->rxbadproto);
		}
	}
	return NULL;
}

void
wlc_vht_update_sgi_rx(wlc_info_t *wlc, uint int_val)
{
	WL_TRACE(("wl%d: %s(%04x)\n", wlc->pub->unit, __FUNCTION__, int_val));
	wlc->vht_cap.vht_cap_info &= ~(VHT_CAP_INFO_SGI_80MHZ);
	wlc->vht_cap.vht_cap_info |= (int_val & WLC_VHT_SGI_80) ?
		VHT_CAP_INFO_SGI_80MHZ : 0;

	if (wlc->pub->up) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}

void
wlc_vht_update_bfr_bfe(wlc_info_t *wlc)
{
#ifdef WL_BEAMFORMING
	if (TXBF_ENAB(wlc->pub)) {
		wlc->vht_cap.vht_cap_info &= ~(VHT_CAP_INFO_SU_BEAMFMR);
		if (wlc->txbf->bfr_capable)
			wlc->vht_cap.vht_cap_info |= VHT_CAP_INFO_SU_BEAMFMR;

		wlc->vht_cap.vht_cap_info &= ~(VHT_CAP_INFO_SU_BEAMFMEE);
		if (wlc->txbf->bfe_capable)
			wlc->vht_cap.vht_cap_info |= VHT_CAP_INFO_SU_BEAMFMEE;

		/*
		 * Beamformee's capability of max. beamformer's antenas it can support
		 * should be 2 for brcm AC device
		 */
		if (wlc->txbf->bfe_capable | wlc->txbf->bfr_capable) {
			wlc->vht_cap.vht_cap_info &= (~VHT_CAP_INFO_NUM_BMFMR_ANT_MASK);
			wlc->vht_cap.vht_cap_info &= (~VHT_CAP_INFO_NUM_SOUNDING_DIM_MASK);
			wlc->vht_cap.vht_cap_info &= (~VHT_CAP_INFO_LINK_ADAPT_CAP_MASK);

			wlc->vht_cap.vht_cap_info |= (2 << VHT_CAP_INFO_NUM_BMFMR_ANT_SHIFT);
			wlc->vht_cap.vht_cap_info |=
				((wlc->stf->txstreams - 1) << VHT_CAP_INFO_NUM_SOUNDING_DIM_SHIFT);
			wlc->vht_cap.vht_cap_info |=
				(3 << VHT_CAP_INFO_LINK_ADAPT_CAP_SHIFT); /* both */
		}
	}
#endif /* WL_BEAMFORMING */

	if (wlc->pub->up) {
		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
	}
}

static void
wlc_write_vht_cap_ie(wlc_bsscfg_t *cfg, vht_cap_ie_t *cap_ie)
{
	uint32 cap;

	ASSERT(&cfg->wlc->vht_cap != cap_ie);

	cap = cfg->wlc->vht_cap.vht_cap_info;
	WL_TRACE(("%s: cap info=%04x\n", __FUNCTION__, cap));

	/* copy off what we can do - initted and filled out at attach */
	htol32_ua_store(cap, (uint8*)&cap_ie->vht_cap_info);

	htol16_ua_store(cfg->wlc->vht_cap.rx_mcs_map, (uint8*)&cap_ie->rx_mcs_map);
	htol16_ua_store(cfg->wlc->vht_cap.rx_max_rate, (uint8*)&cap_ie->rx_max_rate);
	htol16_ua_store(cfg->wlc->vht_cap.tx_mcs_map, (uint8*)&cap_ie->tx_mcs_map);
	htol16_ua_store(cfg->wlc->vht_cap.tx_max_rate, (uint8*)&cap_ie->tx_max_rate);

	if (AP_ENAB(cfg->wlc->pub) && cfg->current_bss) {
		cfg->current_bss->vht_rxmcsmap = cfg->wlc->vht_cap.rx_mcs_map;
		cfg->current_bss->vht_txmcsmap = cfg->wlc->vht_cap.tx_mcs_map;
	}
}

static void
wlc_write_vht_op_ie(wlc_bsscfg_t *cfg, vht_op_ie_t *op_ie)
{
	wlc_bss_info_t *current_bss = cfg->current_bss;
	chanspec_t chspec = current_bss->chanspec;
	uint8 width;
	uint8 chan1, chan2;

	if (CHSPEC_IS80(chspec)) {
		width = VHT_OP_CHAN_WIDTH_80;
	} else if (CHSPEC_IS160(chspec)) {
		width = VHT_OP_CHAN_WIDTH_160;
	} else if (CHSPEC_IS8080(chspec)) {
		width = VHT_OP_CHAN_WIDTH_80_80;
	} else {
		width = VHT_OP_CHAN_WIDTH_20_40;
	}

	if (cfg->oper_mode_enabled) {
		/* If Operating Mode enabled, update width accordingly (11ac 10.41) */
		if (DOT11_OPER_MODE_CHANNEL_WIDTH_20MHZ(cfg->oper_mode) ||
			DOT11_OPER_MODE_CHANNEL_WIDTH_40MHZ(cfg->oper_mode))
			width = VHT_OP_CHAN_WIDTH_20_40;
	}

	if (CHSPEC_IS8080(chspec)) {
		chan1 = CHSPEC_CHAN1(chspec);
		chan2 = CHSPEC_CHAN2(chspec);
	} else {
		chan1 = CHSPEC_CHANNEL(chspec);
		chan2 = 0;
	}

	op_ie->chan_width = width;
	op_ie->chan1 = chan1;
	op_ie->chan2 = chan2;
}

static uint8 *
wlc_write_local_maximum_transmit_pwr(uint8 min_pwr, uint8* cp, int buflen)
{
	*cp = min_pwr;
	cp += sizeof(uint8);

	return cp;
}

uint8 *
wlc_write_vht_transmit_power_envelope_ie(wlc_info_t *wlc,
	chanspec_t chspec, uint8 *cp, int buflen)
{
	wlcband_t *band;
	uint8 min_pwr;
	dot11_vht_transmit_power_envelope_ie_t *vht_transmit_power_ie;

	vht_transmit_power_ie = (dot11_vht_transmit_power_envelope_ie_t *)cp;
	vht_transmit_power_ie->id = DOT11_MNG_VHT_TRANSMIT_POWER_ENVELOPE_ID;

	cp += sizeof(dot11_vht_transmit_power_envelope_ie_t);

	band = wlc->bandstate[CHSPEC_IS2G(chspec) ? BAND_2G_INDEX : BAND_5G_INDEX];

	wlc_phy_txpower_sromlimit(band->pi, chspec, &min_pwr, NULL, 0);

	min_pwr /= (WLC_TXPWR_DB_FACTOR / 2);

	vht_transmit_power_ie->local_max_transmit_power_20 = min_pwr;

	if (CHSPEC_IS20(chspec)) {
		vht_transmit_power_ie->transmit_power_info = 0;
	}
	else if (CHSPEC_IS40(chspec)) {
		vht_transmit_power_ie->transmit_power_info = 1;
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
	}
	else if (CHSPEC_IS80(chspec)) {
		vht_transmit_power_ie->transmit_power_info = 2;
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
	}
	else if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		vht_transmit_power_ie->transmit_power_info = 3;
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
		cp = wlc_write_local_maximum_transmit_pwr(min_pwr, cp, buflen);
	}

	vht_transmit_power_ie->len = (sizeof(dot11_vht_transmit_power_envelope_ie_t) - TLV_HDR_LEN)
	+ vht_transmit_power_ie->transmit_power_info;

	return cp;
}

/*
 * Returns bitmask specifying the contents format of the  Proprietary Features IE.
 * Zero if IE is not required.
 */

static uint
wlc_vht_ie_encap(wlc_pub_t *pub, int band)
{

	if (BAND_5G(band)) {
		return (WLC_VHT_FEATURES_RATES_5G(pub) != 0) ?
			VHT_IE_ENCAP_HDR : VHT_IE_ENCAP_NONE;
	} else if (BAND_2G(band)) {
		/* On 2.4G encapsulate the VHT IEs and publish the Prop rates if enabled */
		return VHT_IE_ENCAP_ALL;
	} else {
		return VHT_IE_ENCAP_NONE;
	}
}

/*
 * Returns the number of bytes of the Power Envelope IE including TLV header.
 * Varies depending on chanspec
 * Zero if IE is not needed.
 */
static uint
wlc_vht_ie_len_power_envelope(wlc_bsscfg_t *cfg, uint32 flags)
{
	uint ielen = 0;
	chanspec_t chspec;

	if (!(flags & VHT_IE_PWR_ENVELOPE))
		return 0;

	chspec = cfg->current_bss->chanspec;

	if (CHSPEC_IS20(chspec)) {
		ielen = 0;
	}
	else if (CHSPEC_IS40(chspec)) {
		ielen = 1;
	}
	else if (CHSPEC_IS80(chspec)) {
		ielen = 2;
	}
	else if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		ielen = 3;
	}

	/* dot11_vht_transmit_power_envelope_ie_t includes the TLV header */
	ielen += (sizeof(dot11_vht_transmit_power_envelope_ie_t));

	return ielen;
}

/*
 * Returns the number of bytes of the Channel Switch Wrapper IE including TLV header.
 * Varies depending on chanspec
 * Zero if IE is not needed.
 */
static uint
wlc_vht_ie_len_csa_wrapper(wlc_bsscfg_t *cfg, uint32 flags)
{
	uint ielen = 0;
	chanspec_t chspec;

	if (!(flags & VHT_IE_CSA_WRAPPER))
		return 0;

	chspec = cfg->current_bss->chanspec;

	ielen = sizeof(dot11_vht_transmit_power_envelope_ie_t);

	/* wb_csa_ie not present in 20MHz channels */
	if (!CHSPEC_IS20(chspec)) {
		ielen += sizeof(dot11_wide_bw_chan_switch_ie_t);
	}

	/* vht transmit power envelope IE length depends on channel width,
	 * update channel wrapper IE length
	 */
	if (CHSPEC_IS40(chspec)) {
		ielen += 1;
	}
	else if (CHSPEC_IS80(chspec)) {
		ielen += 2;
	}
	else if (CHSPEC_IS8080(chspec) || CHSPEC_IS160(chspec)) {
		ielen += 3;
	}
	return ielen;
}

/*
 * Determine the length of the VHT IE components including any proprietary VHT IEs
 * Required by parts of the WLAN state machine thane wants to know the length
 * of the packet before writing it out for buffer allocation purposes
 */
uint
wlc_vht_ie_len(wlc_bsscfg_t *cfg, int band, uint fc, uint32 flags)
{
	uint ielen = 0;
	switch (fc) {
	case FC_BEACON:
	case FC_PROBE_RESP:
		/* Fixed part, always there */
		ielen = TLVLEN(VHT_CAP_IE_LEN) + TLVLEN(VHT_OP_IE_LEN);

		/* Protocol/state dependent part */

		/* Power Envelope IE */
		ielen += wlc_vht_ie_len_power_envelope(cfg, flags);

		/* CSA Wrapper IE */
		ielen += wlc_vht_ie_len_csa_wrapper(cfg, flags);

		break;

	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		ielen = TLVLEN(VHT_CAP_IE_LEN) + TLVLEN(VHT_OP_IE_LEN);
		break;

	case FC_PROBE_REQ:
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		ielen = TLVLEN(VHT_CAP_IE_LEN);
		break;
	default:
		ielen = 0;
	}

	/* Check for encapsulation header */
	if (wlc_vht_ie_encap(cfg->wlc->pub, band)) {
		ielen += TLVLEN(VHT_IE_FEATURES_HDRLEN);
	}

	return ielen;
}

/* Write out the IEs with the encap header */
static uint8*
wlc_vht_write_vht_elements(wlc_bsscfg_t *cfg, int band,
	uint fc, uint32 flags, uint8 *buf, uint16 len) {
	vht_cap_ie_t vht_cap;
	uint8 *bufend = buf + len;
	wlc_info_t *wlc = cfg->wlc;

	switch (fc) {
	case FC_BEACON:
	case FC_PROBE_RESP:
		/* VHT Capability IE */
		wlc_write_vht_cap_ie(cfg, &vht_cap);
		buf = wlc_write_info_elt_safe(buf, BUFLEN(buf, bufend),
			DOT11_MNG_VHT_CAP_ID, VHT_CAP_IE_LEN, &vht_cap);

		/* VHT Operation IE */
		wlc_write_vht_op_ie(cfg, &cfg->wlc->vht_op);
		buf = wlc_write_info_elt_safe(buf, BUFLEN(buf, bufend),
			DOT11_MNG_VHT_OPERATION_ID, VHT_OP_IE_LEN, &wlc->vht_op);

		/* Protocol and state dependent variable part */

		/* VHT Power envelope IE */
		if (flags & VHT_IE_PWR_ENVELOPE)
			buf = wlc_write_vht_transmit_power_envelope_ie(wlc,
				cfg->current_bss->chanspec, buf, BUFLEN(buf, bufend));

		/* VHT CSA Switch Wrapper */
		if (flags & VHT_IE_CSA_WRAPPER)
			buf = wlc_csa_write_chan_switch_wrapper_ie(wlc->csa,
				cfg, buf, BUFLEN(buf, bufend));
		break;

	case FC_ASSOC_RESP:
	case FC_REASSOC_RESP:
		/* VHT Capability IE */
		wlc_write_vht_cap_ie(cfg, &vht_cap);
		buf = wlc_write_info_elt_safe(buf, BUFLEN(buf, bufend),
			DOT11_MNG_VHT_CAP_ID, VHT_CAP_IE_LEN, &vht_cap);

		/* VHT Operation IE */
		wlc_write_vht_op_ie(cfg, &cfg->wlc->vht_op);
		buf = wlc_write_info_elt_safe(buf, BUFLEN(buf, bufend),
			DOT11_MNG_VHT_OPERATION_ID, VHT_OP_IE_LEN, &wlc->vht_op);
		break;

	case FC_PROBE_REQ:
	case FC_ASSOC_REQ:
	case FC_REASSOC_REQ:
		/* VHT Capability IE */
		wlc_write_vht_cap_ie(cfg, &vht_cap);
		buf = wlc_write_info_elt_safe(buf, BUFLEN(buf, bufend),
			DOT11_MNG_VHT_CAP_ID, VHT_CAP_IE_LEN, &vht_cap);
		break;

	default:
		break;
	}

	return buf;

}

uint8*
wlc_vht_write_vht_ies(wlc_bsscfg_t *cfg, int band, uint fc,
	uint32 flags, uint8 *buf, uint16 len) {

	/* Length check *BEFORE* we attempt the ucode template update */
	ASSERT(wlc_vht_ie_len(cfg, band, fc, flags) <= len);

	/* Make 2.4 operation check here */
	return (BAND_2G(band)) ?
		buf : wlc_vht_write_vht_elements(cfg, band, fc, flags, buf, len);
}

/* Write out Proprietary VHT features IE */
uint8*
wlc_vht_write_vht_brcm_ie(wlc_bsscfg_t *cfg, int band,
	uint fc, uint32 flags, uint8 *buf, uint16 len) {
	uint8 *bufend = buf + len;
	uint8 *payload_start;
	uint encap;
	vht_features_ie_hdr_t vht_features_ie;
	wlc_pub_t *pub = cfg->wlc->pub;

	encap = wlc_vht_ie_encap(pub, band);
	if (wlc_vht_ie_encap(pub, band) == VHT_IE_ENCAP_NONE)
		return buf;

	ASSERT(encap & VHT_IE_ENCAP_HDR);

	/* Length check *BEFORE* we attempt the ucode template update */
	ASSERT(wlc_vht_ie_len(cfg, band, fc, flags) <= len);

	bcopy(BRCM_PROP_OUI, (void *)&vht_features_ie.oui[0], DOT11_OUI_LEN);
	vht_features_ie.type = BRCM_VHT_FEATURES_OUITYPE;

	if (BAND_5G(band)) {
		vht_features_ie.rate_mask = WLC_VHT_FEATURES_RATES_5G(pub);
	} else if (BAND_2G(band)) {
		/* On 2.4G encapsulate the VHT IEs and publish the Prop rates if enabled */
		vht_features_ie.rate_mask = WLC_VHT_FEATURES_RATES_2G(pub);
	}

	/* Write out the Vendor specific  IE header */
	payload_start = wlc_write_info_elt_safe(buf, BUFLEN(buf, bufend),
		DOT11_MNG_PROPR_ID, VHT_IE_FEATURES_HDRLEN, &vht_features_ie);

	/* Write out IE payload */
	if (encap & VHT_IE_ENCAP_VHTIE) {
		int caplen;
		uint8 *payload_end;

		payload_end = wlc_vht_write_vht_elements(cfg, band,
			fc, flags, payload_start, BUFLEN(payload_start, bufend));

		/* Add length of payload into BRCM IE header */
		caplen = BUFLEN(payload_start, payload_end);

		((bcm_tlv_t*)buf)->len += (uint8) caplen;

		return payload_end;
	} else {
		return payload_start;
	}
}

chanspec_t
wlc_vht_chanspec(wlc_info_t *wlc, vht_op_ie_t *op_ie, chanspec_t ht_chanspec)
{
	static const uint16 sidebands[] = {
		WL_CHANSPEC_CTL_SB_LLL, WL_CHANSPEC_CTL_SB_LLU,
		WL_CHANSPEC_CTL_SB_LUL, WL_CHANSPEC_CTL_SB_LUU,
		WL_CHANSPEC_CTL_SB_ULL, WL_CHANSPEC_CTL_SB_ULU,
		WL_CHANSPEC_CTL_SB_UUL, WL_CHANSPEC_CTL_SB_UUU
	};
	chanspec_t chanspec = 0;
	vht_op_chan_width_t width = (vht_op_chan_width_t)op_ie->chan_width;
	uint8 chan1;
	uint8 ht_primary;
	uint16 sb = 0;
	int i;

	if (width == VHT_OP_CHAN_WIDTH_20_40) {
		chanspec = ht_chanspec;
	} else if (width == VHT_OP_CHAN_WIDTH_80) {
		chan1 = op_ie->chan1;
		ht_primary = wf_chspec_ctlchan(ht_chanspec);

		/* check which 20MHz sub-channel is the primary from the HT primary channel */
		if (CHSPEC_IS40_UNCOND(ht_chanspec)) {
			for (i = 0; i < 4; i++) {
				chanspec = CH80MHZ_CHSPEC(chan1, sidebands[i]);
				if (ht_primary == wf_chspec_ctlchan(chanspec)) {
					sb = sidebands[i];
					break;
				}
			}

			/* if the loop ended early, we are good, otherwise we did not
			 * find a primay channel for 80MHz chan1 center channel that
			 * matched the HT channel primary
			 */

			if (i == 4) {
				WL_INFORM(("wl%d: %s: unexpected HT 20MHz primary channel %d for "
				           "80MHz center channel %d\n",
				           wlc->pub->unit, __FUNCTION__,
				           ht_primary, chan1));
				/* will return INVCHANSPEC */
				chanspec = INVCHANSPEC;
			}
		} else {
			WL_INFORM(("wl%d: %s: unexpected HT 20MHz channel %d for "
			           "VHT 80MHz channel %d\n",
			           wlc->pub->unit, __FUNCTION__,
			           ht_primary, chan1));

			/* will return INVCHANSPEC */
			chanspec = INVCHANSPEC;
		}

		if (chanspec != INVCHANSPEC) {
			chanspec = CH80MHZ_CHSPEC(chan1, sb);
		}
	} else if (width == VHT_OP_CHAN_WIDTH_160) {
	} else if (width == VHT_OP_CHAN_WIDTH_80_80) {
	}

	return chanspec;
}

void
wlc_vht_parse_bcn_prb(wlc_info_t *wlc, wlc_bss_info_t *bi, uint8 *tlvs, uint tlvs_len)
{
	vht_cap_ie_t cap_ie_in;
	vht_cap_ie_t *cap_ie = NULL;
	vht_op_ie_t op_ie_in;
	vht_op_ie_t *op_ie;
	int beacon_band  = CHSPEC2WLC_BAND(bi->chanspec);
	uint8 *tlvdata = tlvs;
	uint tlvlen = tlvs_len;

	/* clean up flags */
	bi->flags2 &= ~(WLC_BSS_VHT | WLC_BSS_80MHZ | WLC_BSS_SGI_80);

	if (bi->flags & WLC_BSS_HT) {
		/*
		 * Encapsulated Prop VHT IE appears if we are running VHT in 2.4G or
		 * the extended rates are enabled
		 */
		if (BAND_2G(beacon_band) || WLC_VHT_FEATURES_RATES(wlc->pub)) {
			uint8 vht_ratemask = 0;
			int prop_tlv_len = 0;
			uint8 *prop_tlv = wlc_read_vht_features_ie(wlc,
				tlvs, tlvs_len, &vht_ratemask, &prop_tlv_len);
			if (prop_tlv) {
				tlvdata = prop_tlv;
				tlvlen = prop_tlv_len;
			}
		}

		cap_ie = wlc_read_vht_cap_ie(wlc, tlvdata, tlvlen, &cap_ie_in);
	}

	if (cap_ie) {
		uint32 vht_cap = ltoh16_ua(&cap_ie->vht_cap_info);
		bi->vht_capabilities = vht_cap;
		bi->vht_rxmcsmap = cap_ie->rx_mcs_map;
		bi->vht_txmcsmap = cap_ie->tx_mcs_map;

		/* Mark the BSS as VHT capable */
		bi->flags2 |= WLC_BSS_VHT;

		/* Set SGI flags */
		if (vht_cap & VHT_CAP_INFO_SGI_80MHZ) {
			bi->flags2 |= WLC_BSS_SGI_80;
		}

		/* copy the raw mcs set into the bss rateset struct */
		if (bi->flags2 & WLC_BSS_VHT) {
			bi->rateset.vht_mcsmap = bi->vht_rxmcsmap;
		}

		/* determine the chanspec from VHT Operational IE */
		op_ie = wlc_read_vht_op_ie(wlc, tlvdata, tlvlen, &op_ie_in);
		if (op_ie) {
			bi->chanspec = wlc_vht_chanspec(wlc, op_ie, bi->chanspec);
			/* Set 80MHZ employed bit on bss */
			if (CHSPEC_IS80(bi->chanspec)) {
					bi->flags2 |= WLC_BSS_80MHZ;
			}
		}
	}
}

void
wlc_vht_upd_rate_mcsmap(wlc_info_t *wlc, struct scb *scb, uint16 rxmcsmap)
{
	int i;
	uint16 txmcsmap = wlc->vht_cap.tx_mcs_map;
	uint16 mcsmap = VHT_CAP_MCS_MAP_NONE_ALL;
	int nss_max = VHT_CAP_MCS_MAP_NSS_MAX;

	if (scb->oper_mode_enabled && !DOT11_OPER_MODE_RXNSS_TYPE(scb->oper_mode))
		nss_max = DOT11_OPER_MODE_RXNSS(scb->oper_mode);

	/* initialize all mcs values over all the allowed number of
	 * operational streams to none.
	 */
	for (i = 1; i <= VHT_CAP_MCS_MAP_NSS_MAX; i++) {
		uint8 mcs_tx = VHT_MCS_MAP_GET_MCS_PER_SS(i, txmcsmap);
		uint8 mcs_rx = VHT_MCS_MAP_GET_MCS_PER_SS(i, rxmcsmap);
		uint8 mcs = VHT_CAP_MCS_MAP_NONE;

		if (i <= nss_max && mcs_tx != VHT_CAP_MCS_MAP_NONE &&
			mcs_rx != VHT_CAP_MCS_MAP_NONE)
			mcs = MIN(mcs_tx, mcs_rx);

		VHT_MCS_MAP_SET_MCS_PER_SS(i, mcs, mcsmap);
	}
	scb->rateset.vht_mcsmap = mcsmap;
}

uint8
wlc_get_oper_mode(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg)
{
	uint8 type = DOT11_OPER_MODE_RXNSS_TYPE(bsscfg->oper_mode);
	uint8 nss = DOT11_OPER_MODE_RXNSS(bsscfg->oper_mode);
	uint8 bw = DOT11_OPER_MODE_CHANNEL_WIDTH(bsscfg->oper_mode);

	/* adjust oper_mode according to our capabilities */
	if (!type) {
		nss = MIN(wlc->stf->rxstreams, nss);

		if (bw == DOT11_OPER_MODE_80MHZ && !WLC_80MHZ_CAP_PHY(wlc))
			bw = DOT11_OPER_MODE_40MHZ;
		if (bw == DOT11_OPER_MODE_40MHZ && !WLC_40MHZ_CAP_PHY(wlc))
			bw = DOT11_OPER_MODE_20MHZ;

		return DOT11_OPER_MODE(type, nss, bw);
	}

	/* we should not be here because we don't support nss type 1 for now */
	return bsscfg->oper_mode;
}

void
wlc_send_action_vht_oper_mode(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg,
	const struct ether_addr *ea) {
	struct scb *scb;
	void *p;
	uint8 *pbody;
	uint body_len;
	struct dot11_action_vht_oper_mode *ahdr;

	ASSERT(bsscfg != NULL);

	scb = wlc_scbfindband(wlc, bsscfg, ea,
		CHSPEC_WLCBANDUNIT(bsscfg->current_bss->chanspec));

	if (!(SCB_OPER_MODE_NOTIF_CAP(scb)))
		return;

	body_len = sizeof(struct dot11_action_vht_oper_mode);
	p = wlc_frame_get_mgmt(wlc, FC_ACTION, ea, &bsscfg->cur_etheraddr,
		&bsscfg->BSSID, body_len, &pbody);
	if (p == NULL)
		return;

	ahdr = (struct dot11_action_vht_oper_mode *)pbody;
	ahdr->category = DOT11_ACTION_CAT_VHT;
	ahdr->action = DOT11_VHT_ACTION_OPER_MODE_NOTIF;
	ahdr->mode = wlc_get_oper_mode(wlc, bsscfg);


	wlc_sendmgmt(wlc, p, bsscfg->wlcif->qi, scb);
}

void
wlc_frameaction_vht(wlc_info_t *wlc, uint action_id, struct scb *scb,
	struct dot11_management_header *hdr, uint8 *body, int body_len)
{
	uint8 oper_mode;

	if (scb == NULL)
		return;

	switch (action_id) {
	case DOT11_VHT_ACTION_OPER_MODE_NOTIF:
		if (body_len < sizeof(struct dot11_action_vht_oper_mode)) {
			WL_INFORM(("wl %d: VHT oper mode action frame too small",
				WLCWLUNIT(wlc)));
			WLCNTINCR(wlc->pub->_cnt->rxbadproto);
			break;
		}

		oper_mode = ((struct dot11_action_vht_oper_mode *)body)->mode;
		wlc_scb_update_oper_mode(wlc, scb, oper_mode);
		break;
	case DOT11_VHT_ACTION_CBF:
	case DOT11_VHT_ACTION_GID_MGMT:
	default:
		WL_INFORM(("wl %d: Ignoring unsupported VHT action, id: %d\n",
			WLCWLUNIT(wlc), action_id));
		break;
	}
}
