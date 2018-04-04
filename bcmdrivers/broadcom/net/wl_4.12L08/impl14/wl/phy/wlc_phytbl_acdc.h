/*
 * Declarations for Broadcom PHY core tables,
 * Networking Adapter Device Driver.
 *
 * THE CONTENTS OF THIS FILE IS TEMPORARY.
 * Eventually it'll be auto-generated.
 *
 * Copyright(c) 2012 Broadcom Corp.
 * All Rights Reserved.
 *
 * $Id$
 */

#ifndef _WLC_PHYTBL_ACDH_H_
#define _WLC_PHYTBL_ACDH_H_

#include <typedefs.h>
#include <wlc_cfg.h>

#include "wlc_phy_int.h"

/*
 * Channel Info table for the 2069 rev 1 (4345 A0).
 */

typedef struct _chan_info_radio20691 {
	uint16 chan;            /* channel number */
	uint16 freq;            /* in Mhz */

} chan_info_radio20691_t;

extern chan_info_radio20691_t chan_tuning_20691rev_1[77];

#if defined(BCMDBG)
#if defined(DBG_PHY_IOV)
extern radio_20xx_dumpregs_t dumpregs_20691_rev1[];
#endif	
#endif	

extern radio_20xx_prefregs_t prefregs_20691_rev1[];
extern uint16 acphy_txgain_epa_2g_20691rev1[];
extern uint16 acphy_txgain_epa_5g_20691rev1[];
extern uint16 acphy_txgain_ipa_2g_20691rev1[];
extern uint16 acphy_txgain_ipa_5g_20691rev1[];

#endif	/* _WLC_PHYTBL_ACDH_H_ */
