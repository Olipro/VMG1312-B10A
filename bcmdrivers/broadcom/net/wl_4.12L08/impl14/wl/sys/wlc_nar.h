/*
 * wlc_nar.h
 *
 * This module contains the external definitions for the NAR transmit module.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id$
 *
 */
#if !defined(__WLC_NAR_H__)
#define __WLC_NAR_H__

/*
 * Module attach and detach functions.
 */
extern wlc_nar_info_t *wlc_nar_attach(wlc_info_t *);

extern int wlc_nar_detach(wlc_nar_info_t *);

extern void wlc_nar_dotxstatus(wlc_nar_info_t *, struct scb *scb, void *sdu, tx_status_t *txs);

#ifdef PKTQ_LOG
extern struct pktq *wlc_nar_prec_pktq(wlc_info_t* wlc, struct scb* scb);
#endif

#endif /* __WLC_NAR_H__ */
