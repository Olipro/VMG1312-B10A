#ifndef __FAP_TM_H_INCLUDED__
#define __FAP_TM_H_INCLUDED__

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
 * File Name  : fap_tm.h
 *
 * Description: This file contains the FAP Traffic Manager API.
 *
 *******************************************************************************
 */

#if defined(CC_FAP4KE_TM)

#define FAP_TM_BUCKET_SIZE_MAX   65535
#define FAP_TM_KBPS_MAX          1000000

/* This MUST be kept in sync with fapIoctl_tmMode_t */
typedef enum {
    FAP_TM_MODE_AUTO=0,
    FAP_TM_MODE_MANUAL,
    FAP_TM_MODE_MAX
} fapTm_mode_t;

/* This MUST be kept in sync with fapIoctl_tmPortType_t */
typedef enum {
    FAP_TM_PORT_TYPE_LAN=0,
    FAP_TM_PORT_TYPE_WAN,
    FAP_TM_PORT_TYPE_MAX
} fapTm_portType_t;

typedef enum {
    FAP_TM_SHAPING_TYPE_DISABLED=0,
    FAP_TM_SHAPING_TYPE_RATE,
    FAP_TM_SHAPING_TYPE_RATIO,
    FAP_TM_SHAPING_TYPE_MAX
} fapTm_shapingType_t;

/* Management API */
void fapTm_masterConfig(int enable);
int fapTm_portConfig(uint8 port, fapTm_mode_t mode, int kbps, int mbs,
                     fapTm_shapingType_t shapingType);
int fapTm_getPortConfig(uint8 port, fapTm_mode_t mode, int *kbps_p, int *mbs_p,
                        fapTm_shapingType_t *shapingType_p);
int fapTm_setPortMode(uint8 port, fapTm_mode_t mode);
fapTm_mode_t fapTm_getPortMode(uint8 port);
int fapTm_modeReset(uint8 port, fapTm_mode_t mode);
int fapTm_portType(uint8 port, fapTm_portType_t portType);
int fapTm_portEnable(uint8 port, fapTm_mode_t mode, int enable);
int fapTm_apply(uint8 port);
int fapTm_queueConfig(uint8 port, fapTm_mode_t mode, uint8 queue,
                      uint8 shaperType, int kbps, int mbs);
int fapTm_arbiterConfig(uint8 port, fapTm_mode_t mode, uint8 arbiterType, uint8 arbiterArg);
int fapTm_setQueueWeight(uint8 port, fapTm_mode_t mode, uint8 queue, uint8 weight);
int fapTm_getQueueRate(uint8 port, uint8 queue);
int fapTm_mapTmQueueToSwQueue(uint8 port, uint8 queue, uint8 swQueue);

/* Stats/Debuggging */
void fapTm_status(void);
void fapTm_stats(int port);
void fapTm_dumpMaps(void);

/* Others */
void fapTm_setFlowInfo(fap4kePkt_flowInfo_t *flowInfo_p, uint32 virtDestPortMask);
int fapTm_ioctl(unsigned long arg);
void fapTm_init(void);

#endif /* CC_FAP4KE_TM */

#endif /* __FAP_TM_H_INCLUDED__ */
