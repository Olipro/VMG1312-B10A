#ifndef __FAP_HW_H_INCLUDED__
#define __FAP_HW_H_INCLUDED__

/*
 <:copyright-BRCM:2007:DUAL/GPL:standard
 
    Copyright (c) 2007 Broadcom Corporation
    All Rights Reserved
 
 This program is free software; you can redistribute it and/or modify
 it under the terms of the GNU General Public License, version 2, as published by
 the Free Software Foundation (the "GPL").
 
 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 
 A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 Boston, MA 02111-1307, USA.
 
:>
*/

/*
 *******************************************************************************
 * File Name  : fap_hw.h
 *
 * Description: This file contains ...
 *
 *******************************************************************************
 */

#if defined(CONFIG_BCM96362)
#include "fap_hw_6362.h"
#elif defined(CONFIG_BCM963268)
#include "fap_hw_63268.h"
#elif defined(CONFIG_BCM96828)
#include "fap_hw_6828.h"
#elif defined(CONFIG_BCM96818)
#include "fap_hw_6818.h"
#else
#error "FAP BEING INCLUDED FOR NON SUPPORTED CHIP"
#endif


#endif /* __FAP_HW_H_INCLUDED__ */

