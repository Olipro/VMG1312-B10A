/*
 * Configuration-related definitions for
 * Broadcom 802.11abg Networking Device Driver
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_cfg.h 380701 2013-01-23 16:36:13Z $
 */

#ifndef _wlc_cfg_h_
#define _wlc_cfg_h_

#if defined(__NetBSD__) || defined(__FreeBSD__)
#if !defined(__ECOS)
#include <opt_bcm.h>
#include <opt_bwl.h>
#endif /* !defined(__ECOS) */
#endif /* defined(__NetBSD__) || defined(__FreeBSD__) */

/**************************************************
 * Get customized tunables to override the default*
 * ************************************************
 */
#ifndef LINUX_HYBRID
#include "wlconf.h"
#endif

/***********************************************
 * Feature-related macros to optimize out code *
 * *********************************************
 */

/* WL_ENAB_RUNTIME_CHECK may be set based upon the #define below (for ROM builds). It may also
 * be defined via makefiles (e.g. ROM auto abandon unoptimized compiles).
 */
#if defined(BCMROMBUILD)
	#ifndef WL_ENAB_RUNTIME_CHECK
		#define WL_ENAB_RUNTIME_CHECK
	#endif
#endif /* BCMROMBUILD */

/* DUALBAND Support */
#ifdef DBAND
#define NBANDS(wlc) ((wlc)->pub->_nbands)
#define NBANDS_PUB(pub) ((pub)->_nbands)
#define NBANDS_HW(hw) ((hw)->_nbands)
#else
#define NBANDS(wlc) 1
#define NBANDS_PUB(wlc) 1
#define NBANDS_HW(hw) 1
#endif /* DBAND */

#define IS_SINGLEBAND_5G(device) \
	((device == BCM4306_D11A_ID) || \
	 (device == BCM4311_D11A_ID) || \
	 (device == BCM4318_D11A_ID) || \
	 (device == BCM4321_D11N5G_ID) || \
	 (device == BCM4328_D11A_ID) || \
	 (device == BCM4325_D11A_ID) || \
	 (device == BCM4322_D11N5G_ID) || \
	 (device == BCM43222_D11N5G_ID) || \
	 (device == BCM43228_D11N5G_ID) || \
	 (device == BCM43236_D11N5G_ID) || \
	 (device == BCM4331_D11N5G_ID) || \
	 (device == BCM4360_D11AC5G_ID) || \
	 (device == BCM4335_D11AC5G_ID) || \
	 (device == BCM4352_D11AC5G_ID) || \
	 (device == BCM6362_D11N5G_ID) || \
	0)

	#define WLNINTENDO_ENAB(wlc)	0
	#define WLNITRO_ENAB(wlc)	0

#if defined(WLC_HIGH) && !defined(WLC_LOW)
#define WLC_HIGH_ONLY
#endif

#if !defined(WLC_HIGH) && defined(WLC_LOW)
#define WLC_LOW_ONLY
#endif

#if !defined(WLC_HIGH) || !defined(WLC_LOW)
#define WLC_SPLIT
#endif

/* **** Core type/rev defaults **** */
#define D11_DEFAULT	0xffffffb0	/* Supported  D11 revs: 4, 5, 7-31 */
#define D11_DEFAULT2	0x00003f07	/* Supported  D11 revs: 32-34, 40-45 */

#define APHY_DEFAULT	0x000001ec	/* Supported aphy revs:
					 *	2	4306b0
					 *	3	4306c0, 4712a0/a1/a2/b0
					 *	5	4320a2
					 *	6	4318b0
					 *	7	5352a0, 4311a0
					 *	8	4311b0
					 */
#define GPHY_DEFAULT	0x000003c6	/* Supported gphy revs:
					 *	1	4306b0
					 *	2	4306c0, 4712a0/a1/a2/b0
					 *	6	4320a2
					 *	7	4318b0, 5352a0
					 *	8	4311a0
					 *	9	4311b0
					 */
#define NPHY_DEFAULT	0x001f07ff	/* Supported nphy revs:
					 *	0	4321a0
					 *	1	4321a1
					 *	2	4321b0/b1/c0/c1
					 *	3	4322a0
					 *	4	4322a1
					 *	5	4716a0
					 *	6	43222a0, 43224a0
					 *	7	43226a0
					 *	8	5357a0, 43236a0
					 *	9	5357b0, 43236b0
					 *      10      43237a0
					 *	16	43228a0
					 *	17	53572a0
					 *	18	43239a0
					 *	19	4324a0
					 *	20	4324b0
					 */
#define HTPHY_DEFAULT	0x00000003	/* Supported htphy revs:
					 *	0	4331a0
					 *	1	4331a1
					 */
#define LPPHY_DEFAULT	0x0000001f	/* Supported lpphy revs:
					 *	0	4328a0, 5354a0, 5354a1
					 *	1	4312a0/a1
					 *	2	4325a0
					 *	3	4325b0
					 *	4	4315a0
					 */

#define SSLPNPHY_DEFAULT 0x0000000f	/* Supported sslpnphy revs:
					 *	0	4329a0/k0
					 *	1	4329b0/4329C0
					 *	2	4319a0
					 *	3	5356a0
					 */

#define LCNPHY_DEFAULT	0x0000000f	/* Supported lcnphy revs:
					 *	0	4313a0, 4336a0, 4330a0
					 *	1
					 *	2 	4330a0
					 *	3 	4330b0
					 */

#define LCN40PHY_DEFAULT	0x0000004f	/* Supported lcn40phy revs:
						 *	0	4334a0
						 *	1	4314a0
						 *
						 *	3	4324a0
						 *
						 *
						 *	6	43143a0
						 *
						 */
#define ACPHY_DEFAULT		0x000003f

#define D11CONF1_BASE		0
#define D11CONF2_BASE		32
#define D11CONF3_BASE		64
#define D11CONF_WIDTH		32
#define D11CONF_CHK(val, base)	(((val) >= (base)) && ((val) < ((base) + D11CONF_WIDTH)))

/* Windows needs D11CONF_REV():
 *   "shift count negative or too big, undefined behavior"
 */
#define D11CONF_REV(val, base)	(D11CONF_CHK(val, base) ? ((val) - (base)) : (0))

/* For undefined values, use defaults */
#ifndef D11CONF
#define D11CONF	D11_DEFAULT
#endif
#ifndef D11CONF2
#define D11CONF2 D11_DEFAULT2
#endif
#ifndef ACONF
#define ACONF	APHY_DEFAULT
#endif
#ifndef GCONF
#define GCONF	GPHY_DEFAULT
#endif
#ifndef NCONF
#define NCONF	NPHY_DEFAULT
#endif
#ifndef LPCONF
#define LPCONF	LPPHY_DEFAULT
#endif
#ifndef SSLPNCONF
#define SSLPNCONF	SSLPNPHY_DEFAULT
#endif
#ifndef LCNCONF
#define LCNCONF	LCNPHY_DEFAULT
#endif
#ifndef LCN40CONF
#define LCN40CONF	LCN40PHY_DEFAULT
#endif
#ifndef HTCONF
#define HTCONF	HTPHY_DEFAULT
#endif
#ifndef ACCONF
#define ACCONF ACPHY_DEFAULT
#endif

#if ACCONF
#ifndef ENABLE_ACPHY
#define ENABLE_ACPHY
#endif
#endif	/* ACCONF */


/* support 2G band */
#if GCONF || NCONF || LPCONF || SSLPNCONF || LCNCONF || HTCONF || LCN40CONF || ACCONF
#define BAND2G
#endif

/* support 5G band */
#if ACONF || defined(DBAND)
#define BAND5G
#endif

#ifdef WL11N
#if NCONF || HTCONF || ACCONF
#define WLANTSEL	1
#endif
#if defined(WLAMSDU) && !defined(DONGLEBUILD)
#define WLAMSDU_TX      1
#endif
#endif /* WL11N */

/***********************************
 * Some feature consistency checks *
 * *********************************
 */
#if (defined(BCMSUP_PSK) && defined(LINUX_CRYPTO))
#error	"Only one supplicant can be defined; BCMSUP_PSK or LINUX_CRYPTO"
#endif

#if defined(BCMSUP_PSK) && !defined(STA)
#error	"STA must be defined when BCMSUP_PSK and/or BCMCCX is defined"
#endif


#if defined(CCX_SDK)
#error "BCMCCX, EXT_STA, NDIS6x0, BCMCCMP and WLCNT must be defined for CCX_SDK"
#endif /* CCX_SDK */


#if defined(WET) && !defined(STA)
#error	"STA must be defined when WET is defined"
#endif

#if (!defined(AP) || !defined(STA)) && defined(APSTA)
#error "APSTA feature requested without both AP and STA defined"
#endif


#if defined(BCMAUTH_PSK) && !defined(BCMSUP_PSK)
#error "BCMSUP_PSK should be defined for BCMAUTH_PSK(Temporary Restriction)"
#endif

#if defined(WOWL) && !defined(STA)
#error "STA should be defined for WOWL"
#endif


#if !defined(WLPLT)

#if defined(WLC_HIGH)
#if !defined(AP) && !defined(STA)
#error	"Neither AP nor STA were defined"
#endif
#endif /* defined(WLC_HIGH) */
#if defined(BAND5G) && !defined(WL11H)
#error	"WL11H is required when 5G band support is configured"
#endif

#endif /* !WLPLT */

#if !defined(WME) && defined(WLCAC)
#error	"WME support required"
#endif

/* RXCHAIN_PWRSAVE/WL11N consistency check */
#if defined(RXCHAIN_PWRSAVE) && !defined(WL11N)
#error "WL11N should be defined for RXCHAIN_PWRSAVE"
#endif

/* AP TPC and 11h consistency check */
#if defined(WL_AP_TPC) && !defined(WL11H)
#error "WL11H should be defined for WL_AP_TPC"
#endif

/* AMPDU/WL11N consistency check */
#if defined(WLAMPDU) && !defined(WL11N)
#error "WL11N should be defined for WLAMPDU"
#endif

/* AMSDU/WL11N consistency check */
#if defined(WLAMSDU) && !defined(WL11N)
#error "WL11N should be defined for WLAMSDU"
#endif

/* WL11N/WL11AC consistency check */
#if defined(WL11AC) && !defined(WL11N)
#error "WL11N should be defined for WL11AC"
#endif


#if defined(DWDS) && !defined(WDS)
#error "WDS should be defined for DWDS"
#endif
/* AMPDU Host reorder config checks */
#ifdef WLAMPDU_HOSTREORDER
#if !defined(BRCMAPIVTW)
#error "WLAMPDU_HOSTREORDER feature depends on BRCMAPIVTW feature"
#endif
#if !defined(DONGLEBUILD)
#error "WLAMPDU_HOSTREORDER feature is a valid feature only for full dongle builds"
#endif
#endif /* WLAMPDU_HOSTREORDER */


/* MBSS Utility Macros */
#if defined(MBSS)
/* Test if chip supports 4 or 16 MBSS */
#define D11REV_ISMBSS4(rev)  (D11REV_GE(rev, 9) && D11REV_LE(rev, 14) && !D11REV_IS(rev, 13))
#define D11REV_ISMBSS16(rev)  (D11REV_GE(rev, 13) && !D11REV_IS(rev, 14))
#else
#define D11REV_ISMBSS4(rev)    (0)
#define D11REV_ISMBSS16(rev)    (0)
#endif /* MBSS */

/********************************************************************
 * Phy/Core Configuration.  Defines macros to to check core phy/rev *
 * compile-time configuration.  Defines default core support.       *
 * ******************************************************************
 */

/* Basic macros to check a configuration bitmask */

#define CONF_HAS(config, val)	((config) & (1U << (val)))
#define CONF_MSK(config, mask)	((config) & (mask))
#define MSK_RANGE(low, hi)	((1U << ((hi) + 1)) - (1U << (low)))
#define CONF_RANGE(config, low, hi) (CONF_MSK(config, MSK_RANGE(low, high)))

#define CONF_IS(config, val)	((config) == (uint)(1U << (val)))
#define CONF_GE(config, val)	((config) & (0 - (1U << (val))))
#define CONF_GT(config, val)	((config) & (0 - 2 * (1U << (val))))
#define CONF_LT(config, val)	((config) & ((1U << (val)) - 1))
#define CONF_LE(config, val)	((config) & (2 * (1U << (val)) - 1))

/* Wrappers for some of the above, specific to config constants */

#define ACONF_HAS(val)	CONF_HAS(ACONF, val)
#define ACONF_MSK(mask)	CONF_MSK(ACONF, mask)
#define ACONF_IS(val)	CONF_IS(ACONF, val)
#define ACONF_GE(val)	CONF_GE(ACONF, val)
#define ACONF_GT(val)	CONF_GT(ACONF, val)
#define ACONF_LT(val)	CONF_LT(ACONF, val)
#define ACONF_LE(val)	CONF_LE(ACONF, val)

#define GCONF_HAS(val)	CONF_HAS(GCONF, val)
#define GCONF_MSK(mask)	CONF_MSK(GCONF, mask)
#define GCONF_IS(val)	CONF_IS(GCONF, val)
#define GCONF_GE(val)	CONF_GE(GCONF, val)
#define GCONF_GT(val)	CONF_GT(GCONF, val)
#define GCONF_LT(val)	CONF_LT(GCONF, val)
#define GCONF_LE(val)	CONF_LE(GCONF, val)

#define NCONF_HAS(val)	CONF_HAS(NCONF, val)
#define NCONF_MSK(mask)	CONF_MSK(NCONF, mask)
#define NCONF_IS(val)	CONF_IS(NCONF, val)
#define NCONF_GE(val)	CONF_GE(NCONF, val)
#define NCONF_GT(val)	CONF_GT(NCONF, val)
#define NCONF_LT(val)	CONF_LT(NCONF, val)
#define NCONF_LE(val)	CONF_LE(NCONF, val)

#define LPCONF_HAS(val)	CONF_HAS(LPCONF, val)
#define LPCONF_MSK(mask) CONF_MSK(LPCONF, mask)
#define LPCONF_IS(val)	CONF_IS(LPCONF, val)
#define LPCONF_GE(val)	CONF_GE(LPCONF, val)
#define LPCONF_GT(val)	CONF_GT(LPCONF, val)
#define LPCONF_LT(val)	CONF_LT(LPCONF, val)
#define LPCONF_LE(val)	CONF_LE(LPCONF, val)

#define SSLPNCONF_HAS(val)	CONF_HAS(SSLPNCONF, val)
#define SSLPNCONF_MSK(mask)	CONF_MSK(SSLPNCONF, mask)
#define SSLPNCONF_IS(val)	CONF_IS(SSLPNCONF, val)
#define SSLPNCONF_GE(val)	CONF_GE(SSLPNCONF, val)
#define SSLPNCONF_GT(val)	CONF_GT(SSLPNCONF, val)
#define SSLPNCONF_LT(val)	CONF_LT(SSLPNCONF, val)
#define SSLPNCONF_LE(val)	CONF_LE(SSLPNCONF, val)

#define LCNCONF_HAS(val)	CONF_HAS(LCNCONF, val)
#define LCNCONF_MSK(mask)	CONF_MSK(LCNCONF, mask)
#define LCNCONF_IS(val)		CONF_IS(LCNCONF, val)
#define LCNCONF_GE(val)		CONF_GE(LCNCONF, val)
#define LCNCONF_GT(val)		CONF_GT(LCNCONF, val)
#define LCNCONF_LT(val)		CONF_LT(LCNCONF, val)
#define LCNCONF_LE(val)		CONF_LE(LCNCONF, val)

#define LCN40CONF_HAS(val)	CONF_HAS(LCN40CONF, val)
#define LCN40CONF_MSK(mask)	CONF_MSK(LCN40CONF, mask)
#define LCN40CONF_IS(val)	CONF_IS(LCN40CONF, val)
#define LCN40CONF_GE(val)	CONF_GE(LCN40CONF, val)
#define LCN40CONF_GT(val)	CONF_GT(LCN40CONF, val)
#define LCN40CONF_LT(val)	CONF_LT(LCN40CONF, val)
#define LCN40CONF_LE(val)	CONF_LE(LCN40CONF, val)

#define HTCONF_HAS(val)		CONF_HAS(HTCONF, val)
#define HTCONF_MSK(mask)	CONF_MSK(HTCONF, mask)
#define HTCONF_IS(val)		CONF_IS(HTCONF, val)
#define HTCONF_GE(val)		CONF_GE(HTCONF, val)
#define HTCONF_GT(val)		CONF_GT(HTCONF, val)
#define HTCONF_LT(val)		CONF_LT(HTCONF, val)
#define HTCONF_LE(val)		CONF_LE(HTCONF, val)

#define ACCONF_HAS(val)		CONF_HAS(ACCONF, val)
#define ACCONF_MSK(mask)	CONF_MSK(ACCONF, mask)
#define ACCONF_IS(val)		CONF_IS(ACCONF, val)
#define ACCONF_GE(val)		CONF_GE(ACCONF, val)
#define ACCONF_GT(val)		CONF_GT(ACCONF, val)
#define ACCONF_LT(val)		CONF_LT(ACCONF, val)
#define ACCONF_LE(val)		CONF_LE(ACCONF, val)

#define D11CONF_MSK(mask) CONF_MSK(D11CONF, mask)
#define D11CONF_MSK2(lo, hi) (\
	(((D11CONF_CHK(lo, D11CONF1_BASE)) && (D11CONF_CHK(hi, D11CONF1_BASE))) && \
		CONF_MSK(D11CONF, MSK_RANGE(\
			D11CONF_REV(lo, D11CONF1_BASE), D11CONF_REV(hi, D11CONF1_BASE)))) || \
	(((D11CONF_CHK(lo, D11CONF2_BASE)) && (D11CONF_CHK(hi, D11CONF2_BASE))) && \
		CONF_MSK(D11CONF2, MSK_RANGE(\
			D11CONF_REV(lo, D11CONF2_BASE), D11CONF_REV(hi, D11CONF2_BASE)))) || \
	(((D11CONF_CHK(lo, D11CONF1_BASE)) && (D11CONF_CHK(hi, D11CONF2_BASE))) && \
		(CONF_MSK(D11CONF, MSK_RANGE(\
			D11CONF_REV(lo, D11CONF1_BASE), (D11CONF_WIDTH - 1))) || \
		CONF_MSK(D11CONF2, MSK_RANGE(0, D11CONF_REV(hi, D11CONF2_BASE))))))
#define D11CONF_HAS(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_HAS(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_HAS(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))))
#define D11CONF_IS(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) &&	\
	 !D11CONF2 && CONF_IS(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) ||	\
	(D11CONF_CHK(val, D11CONF2_BASE) &&	\
	 !D11CONF && CONF_IS(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))))
#define D11CONF_GE(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_GE(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF1_BASE) && D11CONF2) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_GE(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))))
#define D11CONF_GT(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_GT(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF1_BASE) && D11CONF2) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_GT(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))))
#define D11CONF_LT(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_LT(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_LT(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && D11CONF))
#define D11CONF_LE(val) (\
	(D11CONF_CHK(val, D11CONF1_BASE) && CONF_LE(D11CONF, D11CONF_REV(val, D11CONF1_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && CONF_LE(D11CONF2, D11CONF_REV(val, D11CONF2_BASE))) || \
	(D11CONF_CHK(val, D11CONF2_BASE) && D11CONF))

#define PHYCONF_HAS(val) CONF_HAS(PHYTYPE, val)
#define PHYCONF_IS(val)	CONF_IS(PHYTYPE, val)

/* Macros to check (but override) a run-time value; compile-time
 * override allows unconfigured code to be optimized out.
 *
 * NOTE: includes compile-time check for forced 0 AND forced 1
 * NOTE: single bit/value arg works for small zero-based enums only
 */

#define AREV_IS(var, val)	(ACONF_HAS(val) && (ACONF_IS(val) || ((var) == (val))))
#define AREV_GE(var, val)	(ACONF_GE(val) && (!ACONF_LT(val) || ((var) >= (val))))
#define AREV_GT(var, val)	(ACONF_GT(val) && (!ACONF_LE(val) || ((var) > (val))))
#define AREV_LT(var, val)	(ACONF_LT(val) && (!ACONF_GE(val) || ((var) < (val))))
#define AREV_LE(var, val)	(ACONF_LE(val) && (!ACONF_GT(val) || ((var) <= (val))))

#define GREV_IS(var, val)	(GCONF_HAS(val) && (GCONF_IS(val) || ((var) == (val))))
#define GREV_GE(var, val)	(GCONF_GE(val) && (!GCONF_LT(val) || ((var) >= (val))))
#define GREV_GT(var, val)	(GCONF_GT(val) && (!GCONF_LE(val) || ((var) > (val))))
#define GREV_LT(var, val)	(GCONF_LT(val) && (!GCONF_GE(val) || ((var) < (val))))
#define GREV_LE(var, val)	(GCONF_LE(val) && (!GCONF_GT(val) || ((var) <= (val))))

#define NREV_IS(var, val)	(NCONF_HAS(val) && (NCONF_IS(val) || ((var) == (val))))
#define NREV_GE(var, val)	(NCONF_GE(val) && (!NCONF_LT(val) || ((var) >= (val))))
#define NREV_GT(var, val)	(NCONF_GT(val) && (!NCONF_LE(val) || ((var) > (val))))
#define NREV_LT(var, val)	(NCONF_LT(val) && (!NCONF_GE(val) || ((var) < (val))))
#define NREV_LE(var, val)	(NCONF_LE(val) && (!NCONF_GT(val) || ((var) <= (val))))

#define HTREV_IS(var, val)	(HTCONF_HAS(val) && (HTCONF_IS(val) || ((var) == (val))))
#define HTREV_GE(var, val)	(HTCONF_GE(val) && (!HTCONF_LT(val) || ((var) >= (val))))
#define HTREV_GT(var, val)	(HTCONF_GT(val) && (!HTCONF_LE(val) || ((var) > (val))))
#define HTREV_LT(var, val)	(HTCONF_LT(val) && (!HTCONF_GE(val) || ((var) < (val))))
#define HTREV_LE(var, val)	(HTCONF_LE(val) && (!HTCONF_GT(val) || ((var) <= (val))))

#define LPREV_IS(var, val)	(LPCONF_HAS(val) && (LPCONF_IS(val) || ((var) == (val))))
#define LPREV_GE(var, val)	(LPCONF_GE(val) && (!LPCONF_LT(val) || ((var) >= (val))))
#define LPREV_GT(var, val)	(LPCONF_GT(val) && (!LPCONF_LE(val) || ((var) > (val))))
#define LPREV_LT(var, val)	(LPCONF_LT(val) && (!LPCONF_GE(val) || ((var) < (val))))
#define LPREV_LE(var, val)	(LPCONF_LE(val) && (!LPCONF_GT(val) || ((var) <= (val))))

#define SSLPNREV_IS(var, val)	(SSLPNCONF_HAS(val) && (SSLPNCONF_IS(val) || ((var) == (val))))
#define SSLPNREV_GE(var, val)	(SSLPNCONF_GE(val) && (!SSLPNCONF_LT(val) || ((var) >= (val))))
#define SSLPNREV_GT(var, val)	(SSLPNCONF_GT(val) && (!SSLPNCONF_LE(val) || ((var) > (val))))
#define SSLPNREV_LT(var, val)	(SSLPNCONF_LT(val) && (!SSLPNCONF_GE(val) || ((var) < (val))))
#define SSLPNREV_LE(var, val)	(SSLPNCONF_LE(val) && (!SSLPNCONF_GT(val) || ((var) <= (val))))

#define LCNREV_IS(var, val)	(LCNCONF_HAS(val) && (LCNCONF_IS(val) || ((var) == (val))))
#define LCNREV_GE(var, val)	(LCNCONF_GE(val) && (!LCNCONF_LT(val) || ((var) >= (val))))
#define LCNREV_GT(var, val)	(LCNCONF_GT(val) && (!LCNCONF_LE(val) || ((var) > (val))))
#define LCNREV_LT(var, val)	(LCNCONF_LT(val) && (!LCNCONF_GE(val) || ((var) < (val))))
#define LCNREV_LE(var, val)	(LCNCONF_LE(val) && (!LCNCONF_GT(val) || ((var) <= (val))))

#define LCN40REV_IS(var, val)	(LCN40CONF_HAS(val) && (LCN40CONF_IS(val) || ((var) == (val))))
#define LCN40REV_GE(var, val)	(LCN40CONF_GE(val) && (!LCN40CONF_LT(val) || ((var) >= (val))))
#define LCN40REV_GT(var, val)	(LCN40CONF_GT(val) && (!LCN40CONF_LE(val) || ((var) > (val))))
#define LCN40REV_LT(var, val)	(LCN40CONF_LT(val) && (!LCN40CONF_GE(val) || ((var) < (val))))
#define LCN40REV_LE(var, val)	(LCN40CONF_LE(val) && (!LCN40CONF_GT(val) || ((var) <= (val))))

#define ACREV_IS(var, val)	(ACCONF_HAS(val) && (ACCONF_IS(val) || ((var) == (val))))
#define ACREV_GE(var, val)	(ACCONF_GE(val) && (!ACCONF_LT(val) || ((var) >= (val))))
#define ACREV_GT(var, val)	(ACCONF_GT(val) && (!ACCONF_LE(val) || ((var) > (val))))
#define ACREV_LT(var, val)	(ACCONF_LT(val) && (!ACCONF_GE(val) || ((var) < (val))))
#define ACREV_LE(var, val)	(ACCONF_LE(val) && (!ACCONF_GT(val) || ((var) <= (val))))

#define D11REV_IS(var, val)	(D11CONF_HAS(val) && (D11CONF_IS(val) || ((var) == (val))))
#define D11REV_GE(var, val)	(D11CONF_GE(val) && (!D11CONF_LT(val) || ((var) >= (val))))
#define D11REV_GT(var, val)	(D11CONF_GT(val) && (!D11CONF_LE(val) || ((var) > (val))))
#define D11REV_LT(var, val)	(D11CONF_LT(val) && (!D11CONF_GE(val) || ((var) < (val))))
#define D11REV_LE(var, val)	(D11CONF_LE(val) && (!D11CONF_GT(val) || ((var) <= (val))))

#define PHYTYPE_IS(var, val)	(PHYCONF_HAS(val) && (PHYCONF_IS(val) || ((var) == (val))))

/* Finally, early-exit from switch case if anyone wants it... */

#define CASECHECK(config, val)	if (!(CONF_HAS(config, val))) break
#define CASEMSK(config, mask)	if (!(CONF_MSK(config, mask))) break

#if (D11CONF ^ (D11CONF & D11_DEFAULT))
#error "Unsupported MAC revision configured"
#endif
#if (ACONF ^ (ACONF & APHY_DEFAULT))
#error "Unsupported APHY revision configured"
#endif
#if (GCONF ^ (GCONF & GPHY_DEFAULT))
#error "Unsupported GPHY revision configured"
#endif
#if (NCONF ^ (NCONF & NPHY_DEFAULT))
#error "Unsupported NPHY revision configured"
#endif
#if (LPCONF ^ (LPCONF & LPPHY_DEFAULT))
#error "Unsupported LPPHY revision configured"
#endif
#if (LCNCONF ^ (LCNCONF & LCNPHY_DEFAULT))
#error "Unsupported LCNPHY revision configured"
#endif
#if (HTCONF ^ (HTCONF & HTPHY_DEFAULT))
#error "Unsupported HTPHY revision configured"
#endif
#if (LCN40CONF ^ (LCN40CONF & LCN40PHY_DEFAULT))
#error "Unsupported LCN40PHY revision configured"
#endif
#if (ACCONF ^ (ACCONF & ACPHY_DEFAULT))
#error "Unsupported ACPHY revision configured"
#endif

/* *** Consistency checks *** */
#if !D11CONF && !D11CONF2
#error "No MAC revisions configured!"
#endif

#if !ACONF && !GCONF && !NCONF && !LPCONF && !SSLPNCONF && !LCNCONF && !HTCONF && \
	!LCN40CONF && !ACCONF
#error "No PHY configured!"
#endif

/* Set up PHYTYPE automatically: (depends on PHY_TYPE_X, from d11.h) */
#if ACONF
#define _PHYCONF_A (1U << PHY_TYPE_A)
#else
#define _PHYCONF_A 0
#endif /* ACONF */

#if GCONF
#define _PHYCONF_G (1U << PHY_TYPE_G)
#else
#define _PHYCONF_G 0
#endif /* GCONF */

#if NCONF
#define _PHYCONF_N (1U << PHY_TYPE_N)
#else
#define _PHYCONF_N 0
#endif /* NCONF */

#if LPCONF
#define _PHYCONF_LP (1U << PHY_TYPE_LP)
#else
#define _PHYCONF_LP 0
#endif /* LPCONF */

#if SSLPNCONF
#define _PHYCONF_SSLPN (1U << PHY_TYPE_SSN)
#else
#define _PHYCONF_SSLPN 0
#endif /* SSLPNCONF */

#if LCNCONF
#define _PHYCONF_LCN (1U << PHY_TYPE_LCN)
#else
#define _PHYCONF_LCN 0
#endif /* LCNCONF */

#if LCN40CONF
#define _PHYCONF_LCN40 (1U << PHY_TYPE_LCN40)
#else
#define _PHYCONF_LCN40 0
#endif /* LCN40CONF */

#if HTCONF
#define _PHYCONF_HT (1U << PHY_TYPE_HT)
#else
#define _PHYCONF_HT 0
#endif /* HTCONF */

#if ACCONF
#define _PHYCONF_AC (1U << PHY_TYPE_AC)
#else
#define _PHYCONF_AC 0
#endif /* ACCONF */

#define PHYTYPE (_PHYCONF_A | _PHYCONF_G | _PHYCONF_N | _PHYCONF_LP | \
	_PHYCONF_SSLPN | _PHYCONF_LCN | _PHYCONF_HT | _PHYCONF_LCN40 |  \
	_PHYCONF_AC)

/* Utility macro to identify 802.11n (HT) capable PHYs */
#define PHYTYPE_11N_CAP(phytype) \
	(PHYTYPE_IS(phytype, PHY_TYPE_N) ||	\
	 PHYTYPE_IS(phytype, PHY_TYPE_SSN) ||	\
	 PHYTYPE_IS(phytype, PHY_TYPE_LCN) ||	\
	 PHYTYPE_IS(phytype, PHY_TYPE_LCN40) ||	\
	 PHYTYPE_IS(phytype, PHY_TYPE_AC) ||	\
	 PHYTYPE_IS(phytype, PHY_TYPE_HT))

/* Utility macro to identify MIMO capable PHYs */
#define PHYTYPE_MIMO_CAP(phytype) \
	(PHYTYPE_11N_CAP(phytype) && \
	 !PHYTYPE_IS(phytype, PHY_TYPE_SSN) && \
	 !PHYTYPE_IS(phytype, PHY_TYPE_LCN) && \
	 !PHYTYPE_IS(phytype, PHY_TYPE_LCN40))

/* Utility macro to identify 802.11ac (VHT) capable PHYs */
#define PHYTYPE_VHT_CAP(phytype) \
	(PHYTYPE_IS(phytype, PHY_TYPE_AC))

#define PHYTYPE_HT_CAP(band) \
	(PHYTYPE_IS((band)->phytype, PHY_TYPE_HT) ||	\
	 PHYTYPE_IS((band)->phytype, PHY_TYPE_AC))

/* Last but not least: shorter wlc-specific var checks */
#define WLCISAPHY(band)		PHYTYPE_IS((band)->phytype, PHY_TYPE_A)
#define WLCISGPHY(band)		PHYTYPE_IS((band)->phytype, PHY_TYPE_G)
#define WLCISNPHY(band)		PHYTYPE_IS((band)->phytype, PHY_TYPE_N)
#define WLCISLPPHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_LP)
#define WLCISSSLPNPHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_SSN)
#define WLCISLCNPHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_LCN)
#define WLCISHTPHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_HT)
#define WLCISLCN40PHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_LCN40)
#define WLCISACPHY(band)	PHYTYPE_IS((band)->phytype, PHY_TYPE_AC)

#define WLC_PHY_11N_CAP(band)	PHYTYPE_11N_CAP((band)->phytype)
#define WLC_PHY_VHT_CAP(band)	PHYTYPE_VHT_CAP((band)->phytype)

/**********************************************************************
 * ------------- End of Core phy/rev configuration. ----------------- *
 * ********************************************************************
 */

/*************************************************
 * Defaults for tunables (e.g. sizing constants)
 *
 * For each new tunable, add a member to the end
 * of wlc_tunables_t in wlc_pub.h to enable
 * runtime checks of tunable values. (Directly
 * using the macros in code invalidates ROM code)
 *
 * ***********************************************
 */
#ifndef NTXD
#define NTXD		512   /* Max # of entries in Tx FIFO. 512 maximum */
#endif /* NTXD */
#ifndef NRXD
#define NRXD		256   /* Max # of entries in Rx FIFO. 512 maximum */
#endif /* NRXD */

/* Separate tunable for DMA descriptor ring size for cores
 * with large descriptor ring support (4k descriptors)
 */
#ifndef NTXD_LARGE
#define NTXD_LARGE		NTXD		/* TX descriptor ring */
#endif

#ifndef NRXD_LARGE
#define NRXD_LARGE		NRXD		/* RX descriptor ring */
#endif

#ifndef NRXBUFPOST
#define	NRXBUFPOST	64		/* try to keep this # rbufs posted to the chip */
#endif /* NRXBUFPOST */

#ifndef TXMR
#define TXMR			2	/* number of outstanding reads */
#endif

#ifndef TXPREFTHRESH
#define TXPREFTHRESH		8	/* prefetch threshold */
#endif

#ifndef TXPREFCTL
#define TXPREFCTL		16	/* max descr allowed in prefetch request */
#endif

#ifndef TXBURSTLEN
#define TXBURSTLEN		1024	/* burst length for dma reads */
#endif

#ifndef RXPREFTHRESH
#define RXPREFTHRESH		8	/* prefetch threshold */
#endif

#ifndef RXPREFCTL
#define RXPREFCTL		16	/* max descr allowed in prefetch request */
#endif

#ifndef RXBURSTLEN
#define RXBURSTLEN		128	/* burst length for dma writes */
#endif

#ifndef MRRS
#define MRRS			AUTO	/* Max read request size */
#endif

#ifndef MAXSCB				    /* station control blocks in cache */
#ifdef AP
#define	MAXSCB		128		/* Maximum SCBs in cache for AP */
#else
#define MAXSCB		32		/* Maximum SCBs in cache for STA */
#endif /* AP */
#endif /* MAXSCB */

#ifndef MAXSCBCUBBIES
#define MAXSCBCUBBIES		24	/* max number of cubbies in scb container */
#endif

#ifndef MAXBSSCFGCUBBIES
#define MAXBSSCFGCUBBIES	16	/* max number of cubbies in bsscfg container */
#endif

#ifdef AMPDU_NUM_MPDU			    /* Used for the memory limited dongles. */
#define AMPDU_NUM_MPDU_2SS_D11LEGACY	AMPDU_NUM_MPDU /* Small fifo D11 Maccore < 16 */
#define AMPDU_NUM_MPDU_3SS_D11LEGACY	AMPDU_NUM_MPDU /* Small fifo D11 Maccore < 16 */
#define AMPDU_NUM_MPDU_2SS_D11HT	AMPDU_NUM_MPDU /* Medium fifo 16 < D11 Maccore < 40 */
#define AMPDU_NUM_MPDU_3SS_D11HT	AMPDU_NUM_MPDU /* Medium fifo 16 < D11 Maccore < 40 */
#define AMPDU_NUM_MPDU_2SS_D11AQM	AMPDU_NUM_MPDU /* Large fifo D11 Maccore > 40 */
#define AMPDU_NUM_MPDU_3SS_D11AQM	AMPDU_NUM_MPDU /* Large fifo D11 Maccore > 40 */
#define AMPDU_NUM_MPDU_3STREAMS		AMPDU_NUM_MPDU
#define AMPDU_MAX_MPDU			AMPDU_NUM_MPDU
#define AMPDU_NUM_MPDU_LEGACY		AMPDU_NUM_MPDU
#else
#ifndef AMPDU_NUM_MPDU_2SS_D11LEGACY
#define AMPDU_NUM_MPDU_2SS_D11LEGACY	16 /* Small fifo D11 Maccore < 16 */
#endif

#ifndef AMPDU_NUM_MPDU_3SS_D11LEGACY
#define AMPDU_NUM_MPDU_3SS_D11LEGACY	16 /* Small fifo D11 Maccore < 16 */
#endif

#ifndef AMPDU_NUM_MPDU_2SS_D11HT
#define AMPDU_NUM_MPDU_2SS_D11HT	32 /* Medium fifo 16 < D11 Maccore < 40 */
#endif

#ifndef AMPDU_NUM_MPDU_3SS_D11HT
#define AMPDU_NUM_MPDU_3SS_D11HT	32 /* Medium fifo 16 < D11 Maccore < 40 */
#endif

#ifndef AMPDU_NUM_MPDU_2SS_D11AQM
#define AMPDU_NUM_MPDU_2SS_D11AQM	32 /* Large fifo D11 Maccore > 40 */
#endif

#ifndef AMPDU_NUM_MPDU_3SS_D11AQM
#define AMPDU_NUM_MPDU_3SS_D11AQM	64 /* Large fifo D11 Maccore > 40 */
#endif

#ifndef AMPDU_NUM_MPDU_3STREAMS
#define AMPDU_NUM_MPDU_3STREAMS		AMPDU_NUM_MPDU_3SS_D11LEGACY
#endif

#ifndef AMPDU_MAX_MPDU
#if D11CONF_GE(40)
#define AMPDU_MAX_MPDU		64 /* max number of mpdus in an ampdu */
#else
#define AMPDU_MAX_MPDU		32 /* max number of mpdus in an ampdu */
#endif
#endif /* AMPDU_MAX_MPDU */

#ifndef AMPDU_NUM_MPDU_LEGACY
#define AMPDU_NUM_MPDU_LEGACY	16
#endif

#define AMPDU_NUM_MPDU			AMPDU_NUM_MPDU_2SS_D11LEGACY

#endif /* AMPDU_NUM_MPDU */

#ifndef AMPDU_PKTQ_LEN
#define AMPDU_PKTQ_LEN		1024
#endif

#ifndef AMPDU_PKTQ_FAVORED_LEN
#define AMPDU_PKTQ_FAVORED_LEN	1024
#endif

#define D11HT_CORE	16
#define D11AQM_CORE	40 /* MAC supports hardware qggregation */

/* Count of packet callback structures. either of following
 * 1. Set to the number of SCBs since a STA
 * can queue up a rate callback for each IBSS STA it knows about, and an AP can
 * queue up an "are you there?" Null Data callback for each associated STA
 * 2. controlled by tunable config file
 */
#ifndef MAXPKTCB
#define MAXPKTCB	MAXSCB	/* Max number of packet callbacks */
#endif /* MAXPKTCB */

#ifndef WLC_MAXDPT
#define WLC_MAXDPT	0		/* Max # of DPT links */
#endif /* WLC_MAXDPT */

#ifndef WLC_MAXTDLS
#ifdef WLTDLS
#define WLC_MAXTDLS	4		/* Max # of TDLS links */
#define WLC_MAXDLS_TIMERS	10	/* TDLS_LOOKASIDE_SIZE */
#else
#define WLC_MAXTDLS	0		/* Max # of TDLS links */
#define WLC_MAXDLS_TIMERS	0
#endif /* WLTDLS */
#endif /* WLC_MAXTDLS */

#ifndef WLC_MAXMFPS
#ifdef MFP
#ifdef NDIS630
#define WLC_MAXMFPS	1	/* Win8 only supports 802.11w on primary port */
#else /* !NDIS630 */
#define WLC_MAXMFPS	8	/* in general max times for MFP */
#endif /* NDIS630 */
#else /* !MFP */
#define WLC_MAXMFPS	0
#endif /* MFP */
#endif /* WLC_MAXMFPS */

#ifndef CTFPOOLSZ
#define CTFPOOLSZ       128
#endif /* CTFPOOLSZ */

/* NetBSD also needs to keep track of this */
#define WLC_MAX_UCODE_BSS	(16)		/* Number of BSS handled in ucode bcn/prb */
#define WLC_MAX_UCODE_BSS4	(4)		/* Number of BSS handled in sw bcn/prb */
#ifndef WLC_MAXBSSCFG
#ifdef PSTA
#define WLC_MAXBSSCFG		(1 + 50)		/* max # BSS configs */
#else /* PSTA */
#ifdef AP
#define WLC_MAXBSSCFG		(WLC_MAX_UCODE_BSS + WLC_MAXDPT)	/* max # BSS configs */
#else
#define WLC_MAXBSSCFG		(1 + WLC_MAXDPT)	/* max # BSS configs */
#endif /* AP */
#endif /* PSTA */
#endif /* WLC_MAXBSSCFG */

#ifndef MAXBSS
#if defined(NDIS620) || defined(NDIS630)
#define MAXBSS		128	/* max # available networks */
#else
#define MAXBSS		64	/* max # available networks */
#endif /* NDIS620 || NDIS630 */
#endif /* MAXBSS */

#ifndef WLC_DATAHIWAT
#define WLC_DATAHIWAT		50	/* data msg txq hiwat mark */
#endif /* WLC_DATAHIWAT */

#ifndef WLC_AMPDUDATAHIWAT
#define WLC_AMPDUDATAHIWAT 255
#endif /* WLC_AMPDUDATAHIWAT */

/* bounded rx loops */
#ifndef RXBND
#define RXBND		8	/* max # frames to process in wlc_recv() */
#endif	/* RXBND */
#ifdef WL_ALL_PASSIVE	 //DSLCPE 
#if defined(DSLCPE) && defined(PKTC)
#define TXSBND		128
#else
#define TXSBND		32	/* max # frames to process in wl_start_txqwork(), derived from NETDEV_WEIGHT=32 in bcmenet.c */
#endif //PKTC
#endif
#ifndef TXSBND
#define TXSBND		8	/* max # tx status to process in wlc_txstatus() */
#endif	/* TXSBND */

#ifndef PKTCBND
#define PKTCBND		8	/* max # frames to chain in wlc_recv() */
#endif	/* PKTCBND */

/* VHT 3x3 tunable value defaults */
#ifndef NTXD_AC3X3
#define NTXD_AC3X3		NTXD	/* TX descriptor ring */
#endif

#ifndef NRXD_AC3X3
#define NRXD_AC3X3		NRXD	/* RX descriptor ring */
#endif

#ifndef NTXD_LARGE_AC3X3
#define NTXD_LARGE_AC3X3	NTXD_LARGE
#endif

#ifndef NRXD_LARGE_AC3X3
#define NRXD_LARGE_AC3X3	NRXD_LARGE
#endif

#ifndef NRXBUFPOST_AC3X3
#define NRXBUFPOST_AC3X3	NRXBUFPOST	/* # rx buffers posted */
#endif

#ifndef RXBND_AC3X3
#define RXBND_AC3X3		RXBND	/* max # rx frames to process */
#endif

#ifndef PKTCBND_AC3X3
#define PKTCBND_AC3X3		PKTCBND	/* max # frames to chain in wlc_recv() */
#endif

#ifndef CTFPOOLSZ_AC3X3
#define CTFPOOLSZ_AC3X3		CTFPOOLSZ	/* max buffers in ctfpool */
#endif

/* Maximum number of notification servers. */
#ifndef MAX_NOTIF_SERVERS
#define MAX_NOTIF_SERVERS	5
#endif

/* Maximum number of notification clients. */
#ifndef MAX_NOTIF_CLIENTS
#define MAX_NOTIF_CLIENTS	10
#endif

/* Maximum number of memory pools. */
#ifndef MAX_MEMPOOLS
#define MAX_MEMPOOLS	5
#endif

#ifdef PROP_TXSTATUS
/* FIFO credit values given to host */
#ifndef WLFCFIFOCREDITAC0
#define WLFCFIFOCREDITAC0 2
#endif

#ifndef WLFCFIFOCREDITAC1
#define WLFCFIFOCREDITAC1 8
#endif /* WLFCFIFOCREDITAC1 */

#ifndef WLFCFIFOCREDITAC2
#define WLFCFIFOCREDITAC2 2
#endif

#ifndef WLFCFIFOCREDITAC3
#define WLFCFIFOCREDITAC3 2
#endif

#ifndef WLFCFIFOCREDITBCMC
#define WLFCFIFOCREDITBCMC 2
#endif

#ifndef WLFCFIFOCREDITOTHER
#define WLFCFIFOCREDITOTHER 2
#endif

/* FIFO Credit Pending Thresholds */
#ifndef WLFC_FIFO_CR_PENDING_THRESH_AC_BK
#define WLFC_FIFO_CR_PENDING_THRESH_AC_BK 2
#endif

#ifndef WLFC_FIFO_CR_PENDING_THRESH_AC_BE
#define WLFC_FIFO_CR_PENDING_THRESH_AC_BE 4
#endif

#ifndef WLFC_FIFO_CR_PENDING_THRESH_AC_VI
#define WLFC_FIFO_CR_PENDING_THRESH_AC_VI 3
#endif

#ifndef WLFC_FIFO_CR_PENDING_THRESH_AC_VO
#define WLFC_FIFO_CR_PENDING_THRESH_AC_VO 2
#endif
#endif /* PROP_TXSTATUS */

/* Radar support */
#if defined(WL11H) && defined(BAND5G)
#define RADAR
#endif /* WL11H && BAND5G */
/* DFS */
#if defined(AP) && defined(RADAR)
#define WLDFS
#endif /* AP && RADAR */

#if defined(BAND5G)
#define BAND_5G(bt)	((bt) == WLC_BAND_5G)
#else
#define BAND_5G(bt)	((void)(bt), 0)
#endif

#if defined(BAND2G)
#define BAND_2G(bt)	((bt) == WLC_BAND_2G)
#else
#define BAND_2G(bt)	((void)(bt), 0)
#endif

/* Some phy initialization code/data can't be reclaimed in dualband mode */
#if defined(DBAND) || LPCONF_HAS(0)
#define WLBANDINITDATA(_data)	_data
#define WLBANDINITFN(_fn)	_fn
#else
#define WLBANDINITDATA(_data)	BCMINITDATA(_data)
#define WLBANDINITFN(_fn)	BCMINITFN(_fn)
#endif

/* FIPS support */
#define FIPS_ENAB(wlc) 0

#ifdef DSLCPE
#ifdef WLLMAC
#  ifdef WLLMAC_ONLY
#    define LMAC_ENAB(wlc_pub)			1
#    define LMACPROTO_ENAB(wlc_pub)		1
#  else /* WLLMAC_ONLY */
#    define LMAC_ENAB(wlc_pub)			(wlc_pub->_lmac)
#    ifdef WLLMACPROTO
#      define LMACPROTO_ENAB(wlc_pub)		(wlc_pub->_lmacproto)
#    else
#      define LMACPROTO_ENAB(wlc_pub)		0
#    endif
#  endif /* WLLMAC_ONLY */
#else /* WLLMAC */
#  define LMAC_ENAB(wlc_pub)			0
#  define WLNOKIA_ENAB(wlc_pub)			0
#endif /* WLLMAC */
#endif /* DSLCPE */
#ifdef WLPLT
	#if defined(WL_ENAB_RUNTIME_CHECK) || !defined(DONGLEBUILD)
		#define WLPLT_ENAB(wlc_pub)     (wlc_pub->_plt)
	#elif defined(WLPLT_ENABLED)
		#define WLPLT_ENAB(wlc_pub)     1
	#else
		#define WLPLT_ENAB(wlc_pub)     0
	#endif
#else
	#define WLPLT_ENAB(wlc_pub)     0
#endif /* WLPLT */

#ifdef WLANTSEL
#define WLANTSEL_ENAB(wlc)	1
#else
#define WLANTSEL_ENAB(wlc)	0
#endif /* WLANTSEL */

#ifdef WLAMSDU_TX
#define WLAMSDU_TX_ENAB(wlc)    1
#else
#define WLAMSDU_TX_ENAB(wlc)    0
#endif

#define WLC_TXC_ENAB(wlc)	1
#define WLC_NTXC		2

#ifdef BCMWAPI_WPI
#define WAPI_HW_WPI_CAP(wlc)       ((D11REV_GE((wlc)->pub->corerev, 24)) ? 1 : 0)
#else
#define WAPI_HW_WPI_CAP(wlc)       0
#endif

/* sdio-bmac */

#if ACCONF && D11CONF_GE(40)
#define D11AC_TXD
#endif

/* TOE is always on for 4360, 4335, 4350 */
#if ACCONF && ((D11CONF_GE(40) && D11CONF_LE(42)) || D11CONF_HAS(44) || \
	D11CONF_HAS(46))
#define WLTOEHW
#endif

#ifdef BAND5G
#define WL_NUMCHANSPECS_5G_20	29
#ifdef WL11N
#define WL_NUMCHANSPECS_5G_40	24
#else
#define WL_NUMCHANSPECS_5G_40	0
#endif /* WL11N */
#ifdef WL11AC
#define WL_NUMCHANSPECS_5G_80	24
#else
#define WL_NUMCHANSPECS_5G_80	0
#endif /* WL11AC */
#else
#define WL_NUMCHANSPECS_5G_20	0
#define WL_NUMCHANSPECS_5G_40	0
#define WL_NUMCHANSPECS_5G_80	0
#endif /* band5g */

#ifdef BAND2G
#define WL_NUMCHANSPECS_2G_20	14
#ifdef WL11N
#define WL_NUMCHANSPECS_2G_40	18
#else
#define WL_NUMCHANSPECS_2G_40	0
#endif /* WL11N */
#else
#define WL_NUMCHANSPECS_2G_20	0
#define WL_NUMCHANSPECS_2G_40	0
#endif /* band 2g */
#define WL_NUMCHANSPECS_2G (WL_NUMCHANSPECS_2G_20 + WL_NUMCHANSPECS_2G_40)

#define WL_NUMCHANSPECS_5G (WL_NUMCHANSPECS_5G_20 + WL_NUMCHANSPECS_5G_40 +\
		WL_NUMCHANSPECS_5G_80)

#define IS_DEV_AC3X3(d) (((d) == BCM4360_D11AC_ID) || \
	                 ((d) == BCM4360_D11AC2G_ID) || \
	                 ((d) == BCM4360_D11AC5G_ID))

#define IS_DEV_AC2X2(d) (((d) == BCM4352_D11AC_ID) ||	\
	                 ((d) == BCM4352_D11AC2G_ID) || \
	                 ((d) == BCM4352_D11AC5G_ID))

#define PS_PRETEND_ENABLED(wlc)  (((wlc)->ps_pretend_retry_limit) > 0)
#define PS_PRETEND_THRESHOLD_ENABLED(wlc)  (((wlc)->ps_pretend_threshold) > 0)

#endif /* _wlc_cfg_h_ */
