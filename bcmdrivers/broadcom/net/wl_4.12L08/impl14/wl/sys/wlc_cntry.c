/*
 * 802.11h/11d Country and wl Country module source file
 * Broadcom 802.11abgn Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cntry.c 380365 2013-01-22 18:49:30Z $
 */

#include <wlc_cfg.h>

#ifdef WLCNTRY

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmendian.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc.h>
#include <wlc_alloc.h>
#ifdef WLP2P
#include <bcmwpa.h>
#endif
#include <wlc_11h.h>
#include <wlc_tpc.h>
#include <wlc_11d.h>
#include <wlc_cntry.h>

/* IOVar table */
/* No ordering is imposed */
enum {
	IOV_COUNTRY,
	IOV_COUNTRY_ABBREV_OVERRIDE,
	IOV_COUNTRY_IE_OVERRIDE,
	IOV_COUNTRY_REV,
	IOV_LAST
};

static const bcm_iovar_t wlc_cntry_iovars[] = {
	{"country", IOV_COUNTRY, (0), IOVT_BUFFER, WLC_CNTRY_BUF_SZ},
	{"country_abbrev_override", IOV_COUNTRY_ABBREV_OVERRIDE, (0), IOVT_BUFFER, 0},
#ifdef BCMDBG
	{"country_ie_override", IOV_COUNTRY_IE_OVERRIDE, (0), IOVT_BUFFER, 5},
#endif /* BCMDBG */
	{"country_rev", IOV_COUNTRY_REV, (0), IOVT_BUFFER, (sizeof(uint32)*(WL_NUMCHANSPECS+1))},
	{NULL, 0, 0, 0, 0}
};

/* Country module info */
struct wlc_cntry_info {
	wlc_info_t *wlc;
	/* country management */
	char country_default[WLC_CNTRY_BUF_SZ];	/* saved country for leaving 802.11d
						 * auto-country mode
						 */
#ifdef BCMDBG
	bcm_tlv_t *country_ie_override;		/* debug override of announced Country IE */
#endif
	char country_abbrev_override[WLC_CNTRY_BUF_SZ];	/* country abbrev override */
};

/* local functions */
/* module */
#ifndef OPENSRC_IOV_IOCTL
static int wlc_cntry_external_to_internal(char *buf, int buflen);
#endif /* OPENSRC_IOV_IOCTL */
static int wlc_cntry_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif);
#ifdef BCMDBG
static int wlc_cntry_dump(void *ctx, struct bcmstrbuf *b);
#endif

/* module */
wlc_cntry_info_t *
BCMATTACHFN(wlc_cntry_attach)(wlc_info_t *wlc)
{
	wlc_cntry_info_t *cm;

	if ((cm = wlc_calloc(wlc->osh, wlc->pub->unit, sizeof(wlc_cntry_info_t))) == NULL) {
		WL_ERROR(("wl%d: %s: out of mem, malloced %d bytes\n",
			wlc->pub->unit, __FUNCTION__, MALLOCED(wlc->osh)));
		goto fail;
	}
	cm->wlc = wlc;

#ifdef BCMDBG
	if (wlc_dump_register(wlc->pub, "cntry", wlc_cntry_dump, cm) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_dumpe_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	}
#endif

	/* keep the module registration the last other add module unregistratin
	 * in the error handling code below...
	 */
	if (wlc_module_register(wlc->pub, wlc_cntry_iovars, "cntry", (void *)cm, wlc_cntry_doiovar,
	                        NULL, NULL, NULL) != BCME_OK) {
		WL_ERROR(("wl%d: %s: wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		goto fail;
	};

	return cm;

	/* error handling */
fail:
	if (cm != NULL)
		MFREE(wlc->osh, cm, sizeof(wlc_cntry_info_t));
	return NULL;
}

void
BCMATTACHFN(wlc_cntry_detach)(wlc_cntry_info_t *cm)
{
	wlc_info_t *wlc = cm->wlc;

	wlc_module_unregister(wlc->pub, "cntry", cm);

#ifdef BCMDBG
	if (cm->country_ie_override != NULL) {
		MFREE(wlc->osh, cm->country_ie_override,
		      cm->country_ie_override->len + TLV_HDR_LEN);
		cm->country_ie_override = NULL;
	}
#endif	/* BCMDBG */

	MFREE(wlc->osh, cm, sizeof(wlc_cntry_info_t));
}

/*
 * Converts external country code representation to internal format, for debug builds or
 * disallows for production builds.
 * eg,
 *	ALL -> #a
 *	RDR -> #r
 */
#ifndef OPENSRC_IOV_IOCTL
static int
wlc_cntry_external_to_internal(char *buf, int buflen)
{
	int err = BCME_OK;

	/* Translate ALL or RDR to internal 2 char country codes. */
	if (!strncmp(buf, "ALL", sizeof("ALL") - 1)) {
		strncpy(buf, "#a", buflen);
	} else if (!strncmp(buf, "RDR", sizeof("RDR") - 1)) {
		strncpy(buf, "#r", buflen);
	}
#if !defined(BCMDBG) && !defined(WLTEST)
	/* Don't allow ALL or RDR in production. */
	if ((!strncmp(buf, "#a", sizeof("#a") - 1)) || (!strncmp(buf, "#r", sizeof("#r") - 1))) {
		err = BCME_BADARG;
	}

#endif /* !defined(BCMDBG) && !defined(WLTEST) */
	return err;
}
#endif /* OPENSRC_IOV_IOCTL */

static int
wlc_cntry_doiovar(void *ctx, const bcm_iovar_t *vi, uint32 actionid, const char *name,
	void *params, uint p_len, void *arg, int len, int val_size, struct wlc_if *wlcif)
{
	wlc_cntry_info_t *cm = (wlc_cntry_info_t *)ctx;
	wlc_info_t *wlc = cm->wlc;
	int err = 0;
	int32 int_val = 0;
	int32 int_val2 = 0;
	int32 *ret_int_ptr;
	bool bool_val;
	bool bool_val2;

	/* convenience int and bool vals for first 8 bytes of buffer */
	if (p_len >= (int)sizeof(int_val))
		bcopy(params, &int_val, sizeof(int_val));

	if (p_len >= (int)sizeof(int_val) * 2)
		bcopy((void*)((uintptr)params + sizeof(int_val)), &int_val2, sizeof(int_val));

	/* convenience int ptr for 4-byte gets (requires int aligned arg) */
	ret_int_ptr = (int32 *)arg;
	BCM_REFERENCE(ret_int_ptr);

	bool_val = (int_val != 0) ? TRUE : FALSE;
	bool_val2 = (int_val2 != 0) ? TRUE : FALSE;
	BCM_REFERENCE(bool_val);
	BCM_REFERENCE(bool_val2);

	/* Do the actual parameter implementation */
	switch (actionid) {
	case IOV_GVAL(IOV_COUNTRY): {
		wl_country_t *io_country = (wl_country_t*)arg;
		size_t ccode_buflen = (size_t)(len - OFFSETOF(wl_country_t, ccode));

		if (ccode_buflen < strlen(wlc_channel_ccode(wlc->cmi)) + 1) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		strncpy(io_country->country_abbrev,
			wlc_channel_country_abbrev(wlc->cmi),
			WLC_CNTRY_BUF_SZ-1);
		io_country->country_abbrev[WLC_CNTRY_BUF_SZ-1] = '\0';
		io_country->rev = wlc_channel_regrev(wlc->cmi);
		strncpy(io_country->ccode,
			wlc_channel_ccode(wlc->cmi),
			strlen(wlc_channel_ccode(wlc->cmi)) + 1);
		break;
	}

#ifndef OPENSRC_IOV_IOCTL
	case IOV_SVAL(IOV_COUNTRY): {
		char country_abbrev[WLC_CNTRY_BUF_SZ];
		char ccode[WLC_CNTRY_BUF_SZ];
		int32 rev = -1;

		WL_REGULATORY(("wl%d:%s(): set IOV_COUNTRY.\n", wlc->pub->unit, __FUNCTION__));
		if (len < WLC_CNTRY_BUF_SZ) {
			strncpy(country_abbrev, (char*)arg, len);
		} else {
			strncpy(country_abbrev, (char*)arg, WLC_CNTRY_BUF_SZ-1);
		}

		err = wlc_cntry_external_to_internal(country_abbrev, sizeof(country_abbrev));
		if (err)
			break;

		if (len >= (int)sizeof(wl_country_t)) {
			rev = load32_ua((uint8*)&((wl_country_t*)arg)->rev);
			strncpy(ccode, ((wl_country_t*)arg)->ccode, WLC_CNTRY_BUF_SZ-1);
		}
		else {
			ccode[0] = '\0';
		}

		err = wlc_cntry_external_to_internal(ccode, sizeof(ccode));
		if (err)
			break;

		if (ccode[0] == '\0')
			err = wlc_set_countrycode(wlc->cmi, country_abbrev);
		else
			err = wlc_set_countrycode_rev(wlc->cmi, ccode, rev);
		if (err)
			break;

		/* the country setting may have changed our radio state */
		wlc_radio_upd(wlc);

		/* save default country for exiting 11d regulatory mode */
		strncpy(cm->country_default, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
#ifdef STA
		/* setting the country ends the search for country info */
		if (wlc->m11d)
			wlc_11d_reset_all(wlc->m11d);
#endif
		break;
	}
#endif /* OPENSRC_IOV_IOCTL */

	case IOV_GVAL(IOV_COUNTRY_ABBREV_OVERRIDE): {
		char *country_abbrev = (char*)arg;

		if (len < WLC_CNTRY_BUF_SZ) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		strncpy(country_abbrev, cm->country_abbrev_override,
			WLC_CNTRY_BUF_SZ-1);
		country_abbrev[WLC_CNTRY_BUF_SZ-1] = '\0';
		break;
	}

#ifndef OPENSRC_IOV_IOCTL
	case IOV_SVAL(IOV_COUNTRY_ABBREV_OVERRIDE): {
		char country_abbrev[WLC_CNTRY_BUF_SZ];

		WL_REGULATORY(("wl%d:%s(): set IOV_COUNTRY_ABBREV_OVERRIDE\n",
		               wlc->pub->unit, __FUNCTION__));

		if (len < WLC_CNTRY_BUF_SZ) {
			strncpy(country_abbrev, (char*)arg, len);
		} else {
			strncpy(country_abbrev, (char*)arg, WLC_CNTRY_BUF_SZ-1);
		}

		err = wlc_cntry_external_to_internal(country_abbrev, sizeof(country_abbrev));
		if (err)
			break;

		strncpy(cm->country_abbrev_override, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
		cm->country_abbrev_override[WLC_CNTRY_BUF_SZ-1] = '\0';
		break;
	}
#endif /* OPENSRC_IOV_IOCTL */

#ifdef BCMDBG
	case IOV_GVAL(IOV_COUNTRY_IE_OVERRIDE): {
		bcm_tlv_t *ie = (bcm_tlv_t*)arg;

		if (cm->country_ie_override == NULL) {
			ie->id = DOT11_MNG_COUNTRY_ID;
			ie->len = 0;
			break;
		} else if (len < (cm->country_ie_override->len + TLV_HDR_LEN)) {
			err = BCME_BUFTOOSHORT;
			break;
		}

		wlc_copy_info_elt((uint8*)ie, cm->country_ie_override);
		break;
	}

	case IOV_SVAL(IOV_COUNTRY_IE_OVERRIDE): {
		bcm_tlv_t *ie = (bcm_tlv_t*)arg;

		if (ie->id != DOT11_MNG_COUNTRY_ID || len < (ie->len + TLV_HDR_LEN)) {
			err = BCME_BADARG;
			break;
		}

		/* free any existing override */
		if (cm->country_ie_override != NULL) {
			MFREE(wlc->osh, cm->country_ie_override,
			      cm->country_ie_override->len + TLV_HDR_LEN);
			cm->country_ie_override = NULL;
		}

		/* save a copy of the Country IE override */
		cm->country_ie_override = MALLOC(wlc->osh, ie->len + TLV_HDR_LEN);
		if (cm->country_ie_override == NULL) {
			err = BCME_NORESOURCE;
			break;
		}
		wlc_copy_info_elt((uint8*)cm->country_ie_override, ie);

		wlc_update_beacon(wlc);
		wlc_update_probe_resp(wlc, TRUE);
		break;
	}
#endif /* BCMDBG */

	case IOV_GVAL(IOV_COUNTRY_REV): {
		wl_uint32_list_t *list = (wl_uint32_list_t *)arg;
		char abbrev[WLC_CNTRY_BUF_SZ];
		clm_country_t iter;
		ccode_t cc;
		unsigned int rev;

		bzero(abbrev, WLC_CNTRY_BUF_SZ);
		strncpy(abbrev, ((char*)params), WLC_CNTRY_BUF_SZ - 1);
		list->count = 0;

		for (clm_iter_init(&iter); clm_country_iter(&iter, cc, &rev) == CLM_RESULT_OK;) {
			/* If not CC we are looking for - going to next region */
			if (strncmp(cc, abbrev, sizeof(ccode_t))) {
				continue;
			}
			/* Adding revision to buffer */
			if (list->count < ((uint32)val_size-1)) {
				list->element[list->count] = rev;
				list->count++;
			}
		}

		break;
	}

	default:
		err = BCME_UNSUPPORTED;
		break;
	}

	return err;
}

#ifdef BCMDBG
static int
wlc_cntry_dump(void *ctx, struct bcmstrbuf *b)
{
	wlc_cntry_info_t *cm = (wlc_cntry_info_t *)ctx;

	bcm_bprintf(b, "country_default:%s\n", cm->country_default);

	if (cm->country_ie_override != NULL) {
		wlc_print_ies(cm->wlc, (uint8 *)cm->country_ie_override,
		              cm->country_ie_override->len + TLV_HDR_LEN);
	}

	return BCME_OK;
}
#endif /* BCMDBG */

static uint8 *
wlc_write_country_ie(wlc_info_t *wlc, wlc_cntry_info_t *cm, uint8 *cp, int buflen)
{
	uint bandunit;
	uint8 cur_chan, first_chan, group_pwr, chan_pwr;
	int valid_channel;
	char seen_valid;
	const char *country_str;
	bcm_tlv_t *country_ie = (bcm_tlv_t*)cp;
	uint8 *orig_cp = cp;

	/* What bandunit is the current BSS?  We want to only process a single band. */
	bandunit = CHSPEC_WLCBANDUNIT(wlc->home_chanspec);

	/* make sure we have enough for fixed fields of counrty ie */
	/* if not, return original buffer pointer */
	BUFLEN_CHECK_AND_RETURN(5, buflen, orig_cp);

	country_ie->id = DOT11_MNG_COUNTRY_ID;

	/* Override if country abbrev override is set */
	country_str = (cm->country_abbrev_override[0] != '\0') ? cm->country_abbrev_override :
	                                                 wlc_channel_country_abbrev(wlc->cmi);
	country_ie->data[0] = country_str[0];
	country_ie->data[1] = country_str[1];
	country_ie->data[2] = ' '; /* handling both indoor and outdoor */

	cp += 5;
	/* update buflen */
	buflen -= 5;

	/* Fill in channels & txpwr */
	seen_valid = 0;
	group_pwr = 0;
	for (first_chan = cur_chan = 0; cur_chan <= MAXCHANNEL; cur_chan++) {
		valid_channel = (cur_chan < MAXCHANNEL) &&
			VALID_CHANNEL20_IN_BAND(wlc, bandunit, cur_chan);
		if (valid_channel)
			chan_pwr = wlc_get_reg_max_power_for_channel(wlc->cmi, cur_chan, TRUE);
		else
			chan_pwr = 0;

		if (!valid_channel || chan_pwr != group_pwr) {
			if (seen_valid) {
				/* make sure buffer big enough for 3 byte data */
				/* if not, return original buffer pointer */
				BUFLEN_CHECK_AND_RETURN(3, buflen, orig_cp);
				*cp++ = first_chan;
				*cp++ = cur_chan - first_chan;
				*cp++ = group_pwr;
				/* update buflen */
				buflen -= 3;
				seen_valid = 0;
			}
		}

		if (valid_channel) {
			if (!seen_valid) {
				group_pwr = chan_pwr;
				first_chan = cur_chan;
				seen_valid = 1;
			}
		}
	}

	/* Pad if odd length.  Len excludes ID and len itself. */
	country_ie->len = (uint8)(cp - country_ie->data);
	if (country_ie->len & 1) {
		/* make sure buffer big enough for 1 byte pad */
		/* if not, return original buffer pointer */
		BUFLEN_CHECK_AND_RETURN(1, buflen, orig_cp);
		country_ie->len++;
		*cp++ = 0;
		buflen--;
	}

	return cp;
}

uint8 *
wlc_cntry_write_country_ie(wlc_cntry_info_t *cm, uint8 *cp, int buflen)
{
	wlc_info_t *wlc = cm->wlc;

	(void)wlc;

/* #ifdef BCMDBG */
	if (!WLC_AUTOCOUNTRY_ENAB(wlc) ||
	    wlc_11d_autocountry_adopted(wlc->m11d)) {
		cp = wlc_write_country_ie(wlc, cm, cp, buflen);
	}

	return cp;
}

#ifdef STA
int
wlc_cntry_parse_country_ie(wlc_cntry_info_t *cm, const bcm_tlv_t *ie,
	char *country, chanvec_t *valid_channels, int8 *tx_pwr)
{
	wlc_info_t *wlc = cm->wlc;
	const uint8 *cp;
	char ie_len = ie->len;
	int8 channel_txpwr;
	uchar channel, channel_len;
	uint8 ch_sep;

	(void)wlc;

	ASSERT(ie->id == DOT11_MNG_COUNTRY_ID);

	bzero(country, 4);
	bzero(valid_channels, sizeof(chanvec_t));

	if (ie_len >= WLC_MIN_CNTRY_ELT_SZ) {
		/* parse country string */
		cp = ie->data;
		country[0] = *(cp++);
		country[1] = *(cp++);
		country[2] = *(cp++);
		country[3] = 0;	/* terminate */
		ie_len -= 3;

		/* Parse all first channel/num channels/tx power triples */
		while (ie_len >= 3) {
			channel = cp[0];
			channel_len = cp[1];
			channel_txpwr = (int8)cp[2];

			ch_sep = (channel <= CH_MAX_2G_CHANNEL)? CH_5MHZ_APART: CH_20MHZ_APART;
			for (; channel_len && channel < MAXCHANNEL; channel += ch_sep,
				channel_len--) {
				setbit(valid_channels->vec, channel);
				tx_pwr[(int)channel] = channel_txpwr;
			}
			ie_len -= 3;
			cp += 3;
		}
	} else {
		WL_REGULATORY(("wl%d: %s: malformed country ie: len %d\n",
			wlc->pub->unit, __FUNCTION__, ie_len));
		return BCME_ERROR;
	}

	return BCME_OK;
}

/* Country IE adopting rules:
 *	1. If no Country IE has ever been adopted, adopt it.
 *	2. If associted AP is an normal AP, adopt it.
 *	3. If associated AP is a GO, adopt ONLY if it has NOT adopted an
 *        Country IE from a normal associated AP.
*/

void
wlc_cntry_adopt_country_ie(wlc_cntry_info_t *cm, wlc_bsscfg_t *cfg, uint8 *tags, int tags_len)
{
	wlc_info_t *wlc = cm->wlc;
	bcm_tlv_t *country_ie;
	bool adopt_country;

	country_ie = bcm_parse_tlvs(tags, tags_len, DOT11_MNG_COUNTRY_ID);
#ifdef WLP2P
	adopt_country = bcm_find_p2pie(tags, tags_len) == NULL;
#else
	adopt_country = TRUE;
#endif

	WL_REGULATORY(("wl%d: %s: is_normal_ap = %d, country_ie %s\n",
	               wlc->pub->unit, __FUNCTION__,
	               adopt_country, ((country_ie) ? "PRESENT" : "ABSENT")));

	if (country_ie != NULL) {
		char country_str[WLC_CNTRY_BUF_SZ];
		int8 ie_tx_pwr[MAXCHANNEL];
		chanvec_t channels;

		/* Init the array to max value */
		memset(ie_tx_pwr, WLC_TXPWR_MAX, sizeof(ie_tx_pwr));

		bzero(channels.vec, sizeof(channels.vec));

		if (wlc_cntry_parse_country_ie(cm, country_ie, country_str, &channels, ie_tx_pwr)) {
			WL_REGULATORY(("wl%d: %s: malformed Country IE\n",
			               wlc->pub->unit, __FUNCTION__));
			return;
		}

		wlc_11d_adopt_country(wlc->m11d, country_str, adopt_country);

		/* Do not update txpwr_local_max if home_chanspec is not in the country
		 * IE (due to bug in AP?)
		 */
		if (WL11H_ENAB(wlc) && wlc->pub->associated &&
		    isset(channels.vec, wf_chspec_ctlchan(wlc->home_chanspec))) {
			uint8 txpwr;
			uint8 constraint;

			txpwr = ie_tx_pwr[wf_chspec_ctlchan(wlc->home_chanspec)];
			wlc_tpc_set_local_max(wlc->tpc, txpwr);
			WL_REGULATORY(("wl%d: Adopting Country IE \"%s\" "
			               "channel %d txpwr local max %d dBm\n", wlc->pub->unit,
			               country_str, wf_chspec_ctlchan(wlc->home_chanspec),
			               txpwr));
			constraint = wlc_tpc_get_local_constraint_qdbm(wlc->tpc);
			wlc_channel_set_txpower_limit(wlc->cmi, constraint);
		}
	}
}
#endif /* STA */

int
wlc_cntry_use_default(wlc_cntry_info_t *cm)
{
	wlc_info_t *wlc = cm->wlc;

	WL_INFORM(("wl%d:%s(): restore country_default %s.\n",
	           wlc->pub->unit, __FUNCTION__, cm->country_default));

	return wlc_set_countrycode(wlc->cmi, cm->country_default);
}

/* accessors */
void
wlc_cntry_set_default(wlc_cntry_info_t *cm, const char *country_abbrev)
{
	strncpy(cm->country_default, country_abbrev, WLC_CNTRY_BUF_SZ - 1);
}

#endif /* WLCNTRY */
