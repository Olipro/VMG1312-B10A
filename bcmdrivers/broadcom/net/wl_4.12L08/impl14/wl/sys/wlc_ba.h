/*
 * Block Ack related header file
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_ba.h,v 1.10 2007-11-01 01:31:33 Exp $
*/


#ifndef _wlc_ba_h_
#define _wlc_ba_h_

#ifdef WLBA
extern ba_info_t *wlc_ba_attach(wlc_info_t *wlc);
extern void wlc_ba_detach(ba_info_t *ba);
extern bool wlc_ba_cap(ba_info_t *ba);
extern int wlc_ba(ba_info_t *ba, bool on);
extern void wlc_frameaction_ba(ba_info_t *ba, struct scb *scb,
	struct dot11_management_header *hdr, uint8 *body, int body_len);
extern void wlc_ba_process_data_pkt(ba_info_t *ba, struct scb *scb, uint8 tid,
	void **pkts, uint nfrags);
extern void wlc_ba_recv_ctl(ba_info_t *ba, struct scb *scb, uint8 *body, int body_len,
	uint16 fk);
extern void wlc_ba_recvdata(ba_info_t *ba, struct scb *scb, struct wlc_frminfo *f);
extern void wlc_ba_dotxstatus(ba_info_t *ba, struct scb *scb, void *p, uint16 seq,
	bool *free_pdu);
#else	/* stubs */
#define wlc_ba_attach(a) (ba_info_t *)0x0dadbeef
#define	wlc_ba_detach(a) do {} while (0)
#define	wlc_ba_cap(a) FALSE
#define wlc_ba(a, b) do {} while (0)
#define wlc_frameaction_ba(a, b, c, d, e) do {} while (0)
#define wlc_ba_process_data_pkt(a, b, c, d, e) do {} while (0)
#define wlc_ba_recv_ctl(a, b, c, d, e) do {} while (0)
#define wlc_ba_recvdata(a, b, c) do {} while (0)
#define wlc_ba_dotxstatus(a, b, c, d, e) do {} while (0)
#endif /* WLBA */


#endif /* _wlc_ba_h_ */
