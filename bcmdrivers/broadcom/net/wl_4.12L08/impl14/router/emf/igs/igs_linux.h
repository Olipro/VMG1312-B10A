/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igs_linux.h 344201 2012-07-11 20:09:44Z $
 */

#ifndef _IGS_LINUX_H_
#define _IGS_LINUX_H_

#define IGS_MAX_INST  16

typedef struct igs_info
{
	struct igs_info    *next;          /* Next pointer */
	int8               inst_id[IFNAMSIZ];/* IGS instance identifier */
	osl_t              *osh;           /* OS layer handle */
	void               *igsc_info;     /* IGSC Global data handle */
	struct net_device  *br_dev;        /* Bridge device for the instance */
} igs_info_t;

typedef struct igs_struct
{
	struct sock        *nl_sk;         /* Netlink socket */
	igs_info_t         *list_head;     /* IGS instance list head */
	osl_lock_t         lock;           /* IGS locking */
	int32              inst_count;     /* IGS instance count */
} igs_struct_t;

#define MCPD_MAX_IFS            10
typedef struct mcpd_wan_info
{
} t_MCPD_WAN_INFO;

typedef struct mcpd_mld_snoop_entry
{
    char                      br_name[IFNAMSIZ];
    char                      port_no;
    struct                    inv6_addr grp;
    struct                    inv6_addr src;
    struct                    inv6_addr rep;
    int                       mode;
    int                       code;
    unsigned char		 srcMac[6];
    __u16                     tci;/* vlan id */
    t_MCPD_WAN_INFO           wan_info[MCPD_MAX_IFS];
} t_MCPD_MLD_SNOOP_ENTRY;

#define SNOOPING_ADD_ENTRY 0
#define SNOOPING_DEL_ENTRY 1
#define SNOOPING_FLUSH_ENTRY 2
#define SNOOPING_FLUSH_ENTRY_ALL 3

#endif /* _IGS_LINUX_H_ */
