/*
<:copyright-gpl
 Copyright 2010 Broadcom Corp. All Rights Reserved.

 This program is free software; you can distribute it and/or modify it
 under the terms of the GNU General Public License (Version 2) as
 published by the Free Software Foundation.

 This program is distributed in the hope it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 for more details.

 You should have received a copy of the GNU General Public License along
 with this program; if not, write to the Free Software Foundation, Inc.,
 59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
:>
 */

/*
 *--------------------------------------------------------------------------
 * Color encodings for console printing:
 *
 * This feature is controlled from top level make menuconfig, under
 * Debug Selection>Enable Colorized Prints
 *
 * You may select a color specific to your subsystem by:
 *  #define CLRsys CLRg
 *
 * Usage:  PRINT(CLRr "format" CLRNL);
 *--------------------------------------------------------------------------
 */

#ifndef __BCM_COLORS_H__
#define __BCM_COLORS_H__

#ifdef CONFIG_BCM_COLORIZE_PRINTS
#define BCMCOLOR(clr_code)     clr_code
#else
#define BCMCOLOR(clr_code)
#endif

/* White background */
#define CLRr             BCMCOLOR("\e[0;31m")       /* red              */
#define CLRg             BCMCOLOR("\e[0;32m")       /* green            */
#define CLRy             BCMCOLOR("\e[0;33m")       /* yellow           */
#define CLRb             BCMCOLOR("\e[0;34m")       /* blue             */
#define CLRm             BCMCOLOR("\e[0;35m")       /* magenta          */
#define CLRc             BCMCOLOR("\e[0;36m")       /* cyan             */

/* blacK "inverted" background */
#define CLRrk            BCMCOLOR("\e[0;31;40m")    /* red     on blacK */
#define CLRgk            BCMCOLOR("\e[0;32;40m")    /* green   on blacK */
#define CLRyk            BCMCOLOR("\e[0;33;40m")    /* yellow  on blacK */
#define CLRmk            BCMCOLOR("\e[0;35;40m")    /* magenta on blacK */
#define CLRck            BCMCOLOR("\e[0;36;40m")    /* cyan    on blacK */
#define CLRwk            BCMCOLOR("\e[0;37;40m")    /* whilte  on blacK */

/* Colored background */
#define CLRcb            BCMCOLOR("\e[0;36;44m")    /* cyan    on blue  */
#define CLRyr            BCMCOLOR("\e[0;33;41m")    /* yellow  on red   */
#define CLRym            BCMCOLOR("\e[0;33;45m")    /* yellow  on magen */

/* Generic foreground colors */
#define CLRhigh          CLRm                    /* Highlight color  */
#define CLRbold          CLRcb                   /* Bold      color  */
#define CLRbold2         CLRym                   /* Bold2     color  */
#define CLRerr           CLRyr                   /* Error     color  */
#define CLRnorm          BCMCOLOR("\e[0m")       /* Normal    color  */
#define CLRnl            CLRnorm "\n"            /* Normal + newline */

/* Each subsystem may define CLRsys */

#endif /* __BCM_COLORS_H__ */
