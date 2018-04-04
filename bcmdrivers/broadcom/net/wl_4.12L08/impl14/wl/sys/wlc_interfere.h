/*
 * 802.11 interference stats module header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cca.h 253231 2011-04-14 06:59:23Z $
*/

#ifndef _wlc_interfere_h_
#define _wlc_interfere_h_
extern itfr_info_t *wlc_itfr_attach(wlc_info_t *wlc);
extern void wlc_itfr_detach(itfr_info_t *itfr);
#endif /* _wlc_interfere_h_ */
