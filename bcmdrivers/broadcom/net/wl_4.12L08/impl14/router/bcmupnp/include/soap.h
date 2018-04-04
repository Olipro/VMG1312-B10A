/*
 * Broadcom UPnP module SOAP include file
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: soap.h 241192 2011-02-17 21:52:25Z gmo $
 */

#ifndef __SOAP_H__
#define __SOAP_H__

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

#include <upnp_type.h>

#define SOAP_MAX_ERRMSG		256
#define SOAP_MAX_BUF		2048

enum SOAP_ERROR_E {
	SOAP_INVALID_ACTION = 401,
	SOAP_INVALID_ARGS,
	SOAP_OUT_OF_SYNC,
	SOAP_INVALID_VARIABLE,
	SOAP_DEVICE_INTERNAL_ERROR = 501
};

/*
 * Functions
 */
int soap_process(UPNP_CONTEXT *context);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __SOAP_H__ */
