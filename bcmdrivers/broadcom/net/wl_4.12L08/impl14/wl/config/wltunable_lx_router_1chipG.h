/*
 * Broadcom 802.11abg Networking Device Driver Configuration file
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wltunable_lx_router_1chipG.h 241182 2011-02-17 21:50:03Z $
 *
 * wl driver tunables single-chip (4712/535x) G-only driver
 */

#define D11CONF		0x0080		/* D11 Core Rev 7 (4712/535x) */
#define D11CONF2	0		/* D11 Core Rev > 31 */
#define GCONF		0x0004		/* G-Phy Rev 2 (4712/535x) */
#define ACONF		0		/* No A-Phy */
#define HTCONF		0		/* No HT-Phy */
#define NCONF		0		/* No N-Phy */
#define LPCONF		0		/* No LP-Phy */
#define SSLPNCONF	0		/* No SSLPN-Phy */
#define LCNCONF		0		/* No LCN-Phy */
#define LCN40CONF	0		/* No LCN40-Phy */

#define WME_PER_AC_TX_PARAMS 1
#define WME_PER_AC_TUNING 1
