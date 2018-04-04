/*
 * DPT (Direct Packet Transfer) related header file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_dpt.h 241182 2011-02-17 21:50:03Z $
*/

#ifndef _wlc_dpt_h_
#define _wlc_dpt_h_

#define wlc_dpt_attach(a) (dpt_info_t *)0x0dadbeef
#define	wlc_dpt_detach(a) do {} while (0)
#define	wlc_dpt_cap(a) FALSE
#define	wlc_dpt_update_pm_all(a, b, c) do {} while (0)
#define wlc_dpt_pm_pending(a, b) FALSE
#define wlc_dpt_recv_process_prbreq(a, b, c, d, e, f) do {} while (0)
#define wlc_dpt_recv_process_prbresp(a, b, c, d, e, f) do {} while (0)
#define wlc_dpt_query(a, b, c, d) NULL
#define wlc_dpt_used(a, b) do {} while (0)
#define wlc_dpt_rcv_pkt(a, b, c, d) do {} (FALSE)
#define wlc_dpt_set(a, b) do {} while (0)
#define wlc_dpt_cleanup(a, b) do {} while (0)
#define wlc_dpt_free_scb(a, b) do {} while (0)
#define wlc_dpt_wpa_passhash_done(a, b) do {} while (0)
#define wlc_dpt_port_open(a, b) do {} while (0)
#define wlc_dpt_write_ie(a, b, c, d) (0)
#define wlc_dpt_get_parent_bsscfg(a, b) NULL

#endif /* _wlc_dpt_h_ */
