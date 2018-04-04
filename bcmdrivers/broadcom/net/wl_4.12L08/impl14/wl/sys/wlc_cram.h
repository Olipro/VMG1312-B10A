/*
 * CRAM related header file
 * CRAM is a BRCM proprietary small TCP frame aggregator
 * protocol. Receiver software de-aggregates the frames
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cram.h,v 1.11 2009-04-15 18:47:39 Exp $
*/


#ifndef _wlc_cram_h_
#define _wlc_cram_h_

/* Sending side of CRAM is enabled for AP only */
#if defined(AP) && defined(CRAM)
extern cram_info_t *wlc_cram_attach(wlc_info_t *wlc);
extern void wlc_cram_detach(cram_info_t *crami);
extern void wlc_cram_stop(cram_info_t *crami);
extern void wlc_cram_close(void *handle, struct scb *scb);
#else	/* stubs */
#define wlc_cram_attach(a) (cram_info_t *)0x0dadbeef
#define	wlc_cram_detach(a) do {} while (0)
#define wlc_cram_stop(a) do {} while (0)
#define	wlc_cram_close(a, b) do {} while (0)
#endif /* AP && CRAM */

/* Receive side of CRAM is enabled for STA only */
#if defined(STA) && defined(CRAM)
extern void wlc_uncram(wlc_info_t *wlc, struct scb *scb, bool wds,
	struct ether_addr *da, void *p,	char *prx_ctxt, int len_rx_ctxt);
#else
#define	wlc_uncram(wlc, b, c, d, orig, ctxt, len_ctxt) \
		(PKTFREE(((wlc_info_t *)wlc)->pub->osh, (void *)orig, FALSE))
#endif /* STA && CRAM */

#endif /* _wlc_cram_h_ */
