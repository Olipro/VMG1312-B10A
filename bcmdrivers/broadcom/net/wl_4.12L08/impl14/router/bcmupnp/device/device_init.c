/*
 * Broadcom UPnP module, device_init.c
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: device_init.c 241192 2011-02-17 21:52:25Z gmo $
 */
#include <upnp.h>

/* << TABLE BEGIN */
/*
 * WARNNING: DON'T MODIFY THE FOLLOWING TABLES
 * AND DON'T REMOVE TAG :
 *          "<< TABLE BEGIN"
 *          ">> TABLE END"
 */

#ifdef __CONFIG_NAT__
#ifdef __BCMIGD__
extern UPNP_DEVICE	InternetGatewayDevice;
#endif /* __BCMIGD__ */
#endif /* __CONFIG_NAT__ */

extern UPNP_DEVICE	WFADevice;

UPNP_DEVICE *upnp_device_table[] =
{
#ifdef __CONFIG_NAT__
#ifdef __BCMIGD__
	&InternetGatewayDevice,
#endif /* __BCMIGD__ */
#endif /* __CONFIG_NAT__ */

	&WFADevice,
	0
};
/* >> TABLE END */
