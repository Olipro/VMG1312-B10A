/*
<:copyright-gpl
 Copyright 2004 Broadcom Corp. All Rights Reserved.

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

/***************************************************************************
 * File Name  : bcmspudrv.h
 *
 * Description: This file contains the definitions and structures for the
 *              Linux IOCTL interface that used between the user mode SPU
 *              API library and the kernel SPU API driver.
 *
 * Updates    : 11/26/2007  Pavan Kumar.  Created.
 ***************************************************************************/

#if !defined(_BCMSPUDRV_H_)
#define _BCMSPUDRV_H_

/* Includes. */
#include <bcmspucfg.h>

#if defined(__cplusplus)
extern "C" {
#endif

#define IPSECSPUDRV_MAJOR       233

#define SPUDDIOCTL_CHECK \
    _IOR(IPSECSPUDRV_MAJOR, 0, SPUDDDRV_STATUS_ONLY)
#define SPUDDIOCTL_INITIALIZE \
    _IOWR(IPSECSPUDRV_MAJOR, 1, SPUDDDRV_INITIALIZE)
#define SPUDDIOCTL_UNINITIALIZE \
    _IOR(IPSECSPUDRV_MAJOR, 2, SPUDDDRV_STATUS_ONLY)
#define SPUDDIOCTL_ENCRYPT_DECRYPT \
    _IOR(IPSECSPUDRV_MAJOR, 3, SPUDDDRV_ENCRYPT_DECRYPT)
#define SPUDDIOCTL_KEY_SETUP \
    _IOR(IPSECSPUDRV_MAJOR, 4, SPUDDDRV_KEY_SETUP)
#define SPUDDIOCTL_TEST \
    _IOR(IPSECSPUDRV_MAJOR, 5, SPUDDDRV_TEST)
#define SPUDDIOCTL_SPU_SHOW\
    _IOR(IPSECSPUDRV_MAJOR, 6, SPUDDDRV_SPU_SHOW)

#define MAX_SPUDDDRV_IOCTL_COMMANDS 7

/* Enumerations */
typedef enum BcmSpuddStatus
{
    BCMSPUDD_STATUS_SUCCESS = 0,
    BCMSPUDD_STATUS_ERROR
} BCMSPUDD_STATUS;

/* Globals */
typedef void (*SPUDD_FN_NOTIFY_CB)(void *pArg, BCMSPUDD_STATUS status);

/* Typedefs. */
typedef struct
{
    BCMSPUDD_STATUS bvStatus;
} SPUDDDRV_STATUS_ONLY, *PSPUDDDRV_STATUS_ONLY;

typedef struct
{
    SPUDD_FN_NOTIFY_CB pFnNotifyCb;
    UINT32 ulParm;
    BCMSPUDD_STATUS bvStatus;
} SPUDDDRV_INITIALIZE, *PSPUDDDRV_INITIALIZE;

typedef struct
{
    SPUDD_FN_NOTIFY_CB pFnNotifyCb;
    //UINT32 ulTestId;
    UINT32 ulPktId;
    UINT32 ulNumPkts;
    BCMSPUDD_STATUS bvStatus;
} SPUDDDRV_ENCRYPT_DECRYPT, *PSPUDDDRV_ENCRYPT_DECRYPT;

typedef struct
{
    SPUDD_FN_NOTIFY_CB pFnNotifyCb;
    //UINT32 ulTestId;
    UINT32 ulPktId;
    UINT32 ulNumPkts;
    BCMSPUDD_STATUS bvStatus;
} SPUDDDRV_ENCRYPT_TEST, *PSPUDDDRV_ENCRYPT_TEST;

typedef struct
{
    SPUDD_FN_NOTIFY_CB pFnNotifyCb;
    SPU_STAT_PARMS stats;
    BCMSPUDD_STATUS bvStatus;
} SPUDDDRV_SPU_SHOW, *PSPUDDDRV_SPU_SHOW;

typedef struct
{
    SPUDD_FN_NOTIFY_CB pFnNotifyCb;
    SPU_TEST_PARMS testParams;
    BCMSPUDD_STATUS bvStatus;
} SPUDDDRV_TEST, *PSPUDDDRV_TEST;

#if defined(__cplusplus)
}
#endif
#endif // _BCMSPUDRV_H_
