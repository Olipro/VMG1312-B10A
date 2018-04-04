/*
 * 802.11h/11d Country module header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cntry.h 241182 2011-02-17 21:50:03Z $
*/

#ifndef _wlc_cntry_h_
#define _wlc_cntry_h_

#ifdef WLCNTRY

/* module */
extern wlc_cntry_info_t *wlc_cntry_attach(wlc_info_t *wlc);
extern void wlc_cntry_detach(wlc_cntry_info_t *cm);

/* IE build/parse/proc */
extern int wlc_cntry_parse_country_ie(wlc_cntry_info_t *cm, const bcm_tlv_t *ie,
	char *country, chanvec_t *valid_channels, int8 *tx_pwr);
extern uint8 *wlc_cntry_write_country_ie(wlc_cntry_info_t *cm, uint8 *cp, int buflen);
extern void wlc_cntry_adopt_country_ie(wlc_cntry_info_t *cm, wlc_bsscfg_t *cfg,
	uint8 *tags, int tags_len);

/* others */
extern int wlc_cntry_use_default(wlc_cntry_info_t *cm);

/* accessors */
extern void wlc_cntry_set_default(wlc_cntry_info_t *cm, const char *country_abbrev);

#else /* !WLCNTRY */

#define wlc_cntry_attach(wlc) NULL
#define wlc_cntry_detach(cm) do {} while (0)

#define wlc_cntry_parse_country_ie(cm, ie, country, valid_channels, tx_pwr) BCME_ERROR
#define wlc_cntry_write_country_ie(cm, cp, buflen) cp
#define wlc_cntry_adopt_country_ie(cm, cfg, tags, tags_len) do {} while (0)

#define wlc_cntry_use_default(cm) BCME_OK

#define wlc_cntry_set_default(cm, country_abbrev) do {} while (0)

#endif /* !WLCNTRY */

#endif /* _wlc_cntry_h_ */
