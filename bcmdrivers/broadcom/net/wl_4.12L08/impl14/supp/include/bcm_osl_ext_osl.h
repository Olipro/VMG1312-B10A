/*
 * bcm_osl_ext_osl.h -- osl for external supplicant
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: bcm_osl_ext_osl.h,v 1.1 2010-02-23 17:43:13 $
 */

#ifndef _bcm_osl_ext_osl_h_
#define _bcm_osl_ext_osl_h_

#include <osl.h>

#define OS_MALLOC(len) malloc(len)
#define OS_FREE(p) free(p)

#endif /* _bcm_osl_ext_osl_h_ */
