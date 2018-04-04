
/*
 * <:copyright-BRCM:2012:DUAL/GPL:standard
 * 
 *    Copyright (c) 2012 Broadcom Corporation
 *    All Rights Reserved
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
 * :>
 */

#include "shared_utils.h"

#ifdef _CFE_                                                
#include "lib_types.h"
#include "lib_printf.h"
#include "lib_string.h"
#include "bcm_map.h"
#define printk  printf
#else // Linux
#include <linux/kernel.h>
#include <bcm_map_part.h>
#include <linux/string.h>
#endif

unsigned int UtilGetChipRev(void)
{
    unsigned int revId;
    revId = PERF->RevID & REV_ID_MASK;

    return  revId;
}

char *UtilGetChipName(char *buf, int len) {

    unsigned int chipId = (PERF->RevID & CHIP_ID_MASK) >> CHIP_ID_SHIFT;
    unsigned int revId;
    char *mktname = NULL;
    revId = (int) (PERF->RevID & REV_ID_MASK);

#if  defined (_BCM96818_) || defined(CONFIG_BCM96818)
   unsigned int var = (BRCM_VARIANT_REG & BRCM_VARIANT_REG_MASK) >> BRCM_VARIANT_REG_SHIFT;

    switch ((chipId << 8) | var) {
	case(0x681100):
		mktname = "6812B";
		break;
	case(0x681101):
		mktname = "6812R";
		break;
	case(0x681503):
		mktname = "6812GU";
		break;
	case(0x681500):
		mktname = "6818SR";
		break;
	case(0x681700):
		mktname = "6818G";
		break;
	case(0x681701):
		mktname = "6818GS";
		break;
	case(0x681501):
		mktname = "6818GR";
		break;
	case(0x681502):
		mktname = "6820IAD";
		break;
	default:
		mktname = NULL;
    }

#elif  defined (_BCM96828_) || defined(CONFIG_BCM96828)
#if defined(CHIP_VAR_MASK)
        unsigned int var = (PERF->RevID & CHIP_VAR_MASK) >> CHIP_VAR_SHIFT;
#endif
    switch ((chipId << 8) | var) {
	case(0x682100):
		mktname = "6821F";
		break;
	case(0x682101):
		mktname = "6821G";
		break;
	case(0x682200):
		mktname = "6822F";
		break;
	case(0x682201):
		mktname = "6822G";
		break;
	case(0x682800):
		mktname = "6828F";
		break;
	case(0x682801):
		mktname = "6828G";
		break;
	default:
		mktname = NULL;
		break;
    }
#endif

    if (mktname == NULL) {
	sprintf(buf,"%X%X",chipId,revId);
    } else {
        sprintf(buf,"%s_%X",mktname,revId);
    }
    return(buf);
}

int UtilGetChipIsPinCompatible(void) 
{

    int ret = 0;
#if  defined (_BCM96818_) || defined(CONFIG_BCM96818)
    unsigned int chipId = (PERF->RevID & CHIP_ID_MASK) >> CHIP_ID_SHIFT;
    unsigned int var = (BRCM_VARIANT_REG & BRCM_VARIANT_REG_MASK) >> BRCM_VARIANT_REG_SHIFT;
    unsigned int sw;
    sw =  ((chipId << 8) | var);
    switch (sw) {
	case(0x681503): //  "6812GU";
	case(0x681500): //  "6818SR";
	case(0x681501): //  "6818GR";
		ret = 1;
		break;
	default:
		ret = 0;
    }
#endif

    return(ret);
}
