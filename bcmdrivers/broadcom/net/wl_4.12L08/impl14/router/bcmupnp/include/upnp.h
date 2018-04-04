/*
 * Broadcom UPnP module include file
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: upnp.h 241192 2011-02-17 21:52:25Z gmo $
 */

#ifndef __UPNP_H__
#define __UPNP_H__

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

#include <upnp_osl.h>

#include <upnp_type.h>
#include <soap.h>
#include <gena.h>
#include <ssdp.h>
#include <upnp_http.h>
#include <upnp_description.h>
#include <upnp_device.h>
#include <upnp_req.h>
#include <upnp_util.h>

/*
 * Declaration
 */
#define	UPNP_FLAG_SHUTDOWN	1
#define	UPNP_FLAG_RESTART	2

/*
 * Functions
 */
char *upnp_get_config(char *name);
char *upnp_safe_get_config(char *name);

int upnp_mainloop(int argc, char **argv);
void upnp_stop_handler();
void upnp_restart_handler();

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __UPNP_H__ */
