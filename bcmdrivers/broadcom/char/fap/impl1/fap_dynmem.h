/*
<:copyright-BRCM:2012:DUAL/GPL:standard

   Copyright (c) 2012 Broadcom Corporation
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
 * File Name  : fap_dynmem.h
 *
 * Description: This file contains the base interface into the FAP dynamic
 *              memory.
 *******************************************************************************
 */

#ifndef _FAP_DYNMEM_H_INCLUDED_
#define _FAP_DYNMEM_H_INCLUDED_

#ifndef DYN_MEM_TEST_APP
#include "fap_hw.h"
#endif


typedef enum    {
    FAP_DM_REGION_DSP = 0,
    FAP_DM_REGION_PSM,
    FAP_DM_REGION_QSM,
    FAP_DM_REGION_HP,       // high priority - may be in QSM or PSM
    FAP_DM_REGION_MAX        
} fapDm_RegionIdx;

typedef enum    {
    FAP_DM_REGION_ORDER_DSP_PSM_QSM = 0,
    FAP_DM_REGION_ORDER_DSP_PSM_QSM_HP,
    FAP_DM_REGION_ORDER_QSM_PSM,
    FAP_DM_REGION_ORDER_QSM_PSM_HP,
    FAP_DM_REGION_ORDER_QSM,
    FAP_DM_REGION_ORDER_MAX
} fapDm_RegionOrder;

typedef uint32 fapDm_BlockId;

typedef union {
    struct  {
        unsigned              regionIdx : 3;
        unsigned              blockIdx  : 13;
        unsigned              offset : 16;
    };
    fapDm_BlockId           id;
} fapDm_BlockInfo ;


/* Note: FAP_DM_INVALID_BLOCK_ID may not be 0 (as 0 is valid), and must be the same byte repeated four times, due to
   the use of memset to make all blocks invalid by default */

#define FAP_DM_INVALID_BLOCK_ID        ((fapDm_BlockId)0xFFFFFFFF)

#define FAP_DM_RSVD_HP_FLOW_CNT     12
/* The following assumes that only flow info is stored in the hp flow types -- not command lists.
   This is true for multicast.  If we want to do other types of high priority flows, this will
   need to be adjusted: */
#define FAP_DM_HP_SIZE              (FAP_DM_RSVD_HP_FLOW_CNT*sizeof(fap4kePkt_flow_t))

#endif
