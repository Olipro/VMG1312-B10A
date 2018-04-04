/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igsc_sdb.h 241182 2011-02-17 21:50:03Z $
 */

#ifndef _IGSC_SDB_H_
#define _IGSC_SDB_H_

#ifdef DSLCPE  
#include <proto/bcmip.h>
#include <clist.h>
#include <typedefs.h>
#define IGSDB_MGRP_HASH_IPV6(m)       ((((m).s6_addr32[0])+((m).s6_addr32[1])+((m).s6_addr32[2])+((m).s6_addr32[3])) &7)
#endif
#define IGSDB_MGRP_HASH(m)     ((((m) >> 24) + ((m) >> 16) + \
				 ((m) >> 8) + ((m) & 0xff)) & 7)

/*
 * Group entry of IGSDB
 */
typedef struct igsc_mgrp
{
	clist_head_t   mgrp_hlist;   /* Multicast Groups hash list */
#ifdef DSLCPE  
	struct inv6_addr mgrp_ipv6;     /* NMulticast Group IPv6 address */
#endif
	uint32         mgrp_ip;      /* Multicast Group IP Address */
	clist_head_t   mh_head;      /* List head of group members */
	clist_head_t   mi_head;      /* List head of interfaces */
	igsc_info_t    *igsc_info;   /* IGSC instance data */
} igsc_mgrp_t;

/*
 * Interface entry of IGSDB
 */
typedef struct igsc_mi
{
	clist_head_t   mi_list;      /* Multicast i/f list prev and next */
	void           *mi_ifp;      /* Interface pointer */
	int32          mi_ref;       /* Ref count of hosts on the i/f */
} igsc_mi_t;

/*
 * Host entry of IGSDB
 */
typedef struct igsc_mh
{
	clist_head_t   mh_list;      /* Group members list prev and next */
#ifdef DSLCPE  
	struct inv6_addr mh_ipv6;
#endif
	uint32         mh_ip;        /* Unicast IP address of host */
	igsc_mgrp_t    *mh_mgrp;     /* Multicast forwarding entry for the
	                              * group
	                              */
	osl_timer_t    *mgrp_timer;  /* Group Membership Interval timer */
	igsc_mi_t      *mh_mi;       /* Interface connected to host */
} igsc_mh_t;

/*
 * Prototypes
 */
int32 igsc_sdb_member_add(igsc_info_t *igsc_info, void *ifp, uint32 mgrp_ip,
                          uint32 mh_ip);
int32 igsc_sdb_member_del(igsc_info_t *igsc_info, void *ifp, uint32 mgrp_ip,
                          uint32 mh_ip);
void igsc_sdb_init(igsc_info_t *igsc_info);

#ifdef DSLCPE  
void igsc_sdb_clear_ipv6_wlc(wlc_info_t *wlc);
void igsc_sdb_clear_ipv6( igsc_info_t *igsc_info);
int32 igsc_sdb_clear_group_ipv6(wlc_info_t *wlc,struct inv6_addr *grp);
int32 igsc_sdb_member_add_ipv6(igsc_info_t *igsc_info, void *ifp, struct inv6_addr mgrp_ip, struct inv6_addr mh_ip);
int32 igsc_sdb_member_del_ipv6(igsc_info_t *igsc_info, void *ifp, struct inv6_addr mgrp_ip, struct inv6_addr mh_ip);
#endif
#endif /* _IGSC_SDB_H_ */
