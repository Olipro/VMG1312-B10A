/*
 * WiFi-Direct discovery state machine.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
 */

#ifndef _PROWFDDISC_H_
#define _PROWFDDISC_H_

typedef struct proWfdDiscStruct proWfdDiscT;

/* Opaque driver handle type. In dongle this is struct wlc_info_t, representing
 * the driver. On linux host this is struct ifreq, representing the primary OS
 * interface for a driver instance. To specify a virtual interface this should
 * be used together with a bsscfg index.
 */
struct wl_drv_hdl;

/* initialize WFD discovery */
int proWfdDiscInitialize(void);

/* deinitialize WFD discovery */
int proWfdDiscDeinitialize(void);

/* create WFD discovery */
proWfdDiscT *proWfdDiscCreate(struct wl_drv_hdl *drv, uint16 listenChannel);

/* destroy WFD discovery */
int proWfdDiscDestroy(proWfdDiscT *disc);

/* reset WFD discovery */
int proWfdDiscReset(proWfdDiscT *disc);

/* start WFD discovery */
int proWfdDiscStartDiscovery(proWfdDiscT *disc);

/* start WFD extended listen */
/* for continuous listen set on=non-zero (e.g. 5000), off=0 */
int proWfdDiscStartExtListen(proWfdDiscT *disc,
	uint16 listenOnTimeout, uint16 listenOffTimeout);

/* get bsscfg index of WFD discovery interface */
/* bsscfg index is valid only after started */
int proWfdDiscGetBsscfgIndex(proWfdDiscT *disc);

/* wlan event handler */
void proWfdDiscProcessWlanEvent(void *context, uint32 eventType,
	wl_event_msg_t *wlEvent, uint8 *data, uint32 length);

#endif /* _PROWFDDISC_H_ */
