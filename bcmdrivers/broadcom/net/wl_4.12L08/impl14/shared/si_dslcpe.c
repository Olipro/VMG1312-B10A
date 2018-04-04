/*
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * THIS SOFTWARE IS OFFERED "AS IS", AND BROADCOM GRANTS NO WARRANTIES OF ANY
 * KIND, EXPRESS OR IMPLIED, BY STATUTE, COMMUNICATION OR OTHERWISE. BROADCOM
 * SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A SPECIFIC PURPOSE OR NONINFRINGEMENT CONCERNING THIS SOFTWARE.
 *
 * $Id:$
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <bcmdevs.h>
#include <hndsoc.h>
#include <sbchipc.h>
#include <pci_core.h>
#include <pcie_core.h>
#include <nicpci.h>
#include <bcmnvram.h>
#include <bcmsrom.h>
#include <hndtcam.h>
#include <pcicfg.h>
#include <sbpcmcia.h>
#include <sbsocram.h>
#ifdef BCMECICOEX
#include <bcmotp.h>
#endif /* BCMECICOEX */
#include <hndpmu.h>
#ifdef BCMSPI
#include <spid.h>
#endif /* BCMSPI */

#include "siutils_priv.h"

/* DSLCPE headers */
#include <board.h>
#include <boardparms.h>

#ifdef SI_ENUM_BASE_VARIABLE
void si_enum_base_init(si_t *sih, uint bustype)
{
	si_info_t *sii = SI_INFO(sih);
	int devid; 

	if (bustype == PCI_BUS) {
		devid = OSL_PCI_READ_CONFIG(sii->osh, PCI_CFG_VID, sizeof(uint32)) >> 16;
		if ((devid == BCM6362_D11N_ID) || (devid == BCM6362_D11N2G_ID) || (devid == BCM6362_D11N5G_ID)) {
			SI_ENUM_BASE = 0x10004000; //sii->pub.si_enum_base = 0x10004000;
		} else {
			SI_ENUM_BASE = 0x18000000; //sii->pub.si_enum_base = 0x18000000;
		}
	}
}
#endif /* SI_ENUM_BASE_VARIABLE */

