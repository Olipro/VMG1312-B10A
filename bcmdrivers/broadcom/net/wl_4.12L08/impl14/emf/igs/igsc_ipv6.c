/*
 * IGMP Snooping Layer: IGMP Snooping module runs at layer 2. IGMP
 * Snooping layer uses the multicast information in the IGMP messages
 * exchanged between the participating hosts and multicast routers to
 * update the multicast forwarding database. This file contains the
 * common code routines of IGS module.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: igsc.c 247842 2011-03-22 02:10:23Z jihuac $
 */

#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <proto/bcmip.h>
#include <osl.h>
#include <bcmnvram.h>
#include <clist.h>
#if defined(linux)
#include <osl_linux.h>
#else /* defined(osl_xx) */
#error "Unsupported osl"
#endif /* defined(osl_xx) */
#include "igs_cfg.h"
#include "emfc_export.h"
#include "igs_export.h"
#include "igsc_export.h"
#include "igsc.h"
#include "igs_linux.h"
#include "igsc_sdb.h"
#include "wlc_wmf.h"
#include <wlc.h>

/* This becomes netdev->priv and is the link between netdev and wlif struct */
typedef struct priv_link {
	wl_if_t *wlif;
} priv_link_t;

extern int register_mcast_snooping_notifier(struct notifier_block *nb);
extern int unregister_mcast_snooping_notifier(struct notifier_block *nb);
extern struct scb *wlc_scbfind(wlc_info_t *wlc, wlc_bsscfg_t *bsscfg, const struct ether_addr *ea);
extern struct notifier_block mcast_snooping_notifer; 
extern void *getprivInf(char *name,int port_no); 
int igsc_mcast_snooping_event(unsigned long event, void *ptr) {

	t_MCPD_MLD_SNOOP_ENTRY *snoop_entry = (t_MCPD_MLD_SNOOP_ENTRY *)ptr;
	struct wlc_info *wlc;
	struct scb *scb=NULL;
	struct wl_info *wlcif;
	wlc_bsscfg_t	*bsscfg;			/* the primary bsscfg (can be AP or STA) */
	struct wlc_wmf_instance	*wmf; /* WMF instance handle */
	/*int prIndex=0,vrIndex=0;*/
	igsc_info_t *igsc_inf;
	priv_link_t *prl=NULL;
	//struct ether_addr scbaddr;

	prl= (priv_link_t *)getprivInf(snoop_entry->br_name,snoop_entry->port_no);
	if(prl)
	{
		wl_if_t *wlif= prl->wlif;

		if(wlif==NULL||wlif->wl==NULL) {
			printk("JXUJXU:%s:%d   How come wlcif or wlif is NULL? \r\n",__FUNCTION__,__LINE__ );
			return 1;		
		}
		wlcif=wlif->wl;
		wlc=(wlc_info_t *)wlcif->wlc;
		if(wlc) {

			bsscfg=(wlc_bsscfg_t *)wlc->cfg;
			if(unlikely(!bsscfg->wmf_enable))
				return (FAILURE);
			/*sscanf(ifname,"wl%d.%d",&prIndex,&vrIndex);
			//we may need  to find wMF instance differently when it supports multiple 
			IGS_DEBUG(":%s:%d  prIndex:%d,vrIndex:%d \r\n",__FUNCTION__,__LINE__,prIndex,vrIndex);
			IGS_DEBUG(":%s:%d  primary bsscfg:%p,index[0] bsscfg:%08x \r\n",__FUNCTION__,__LINE__,wlc->cfg,wlc->bsscfg[0]);
			*/
			wmf=bsscfg->wmf_instance;
			if(unlikely(wmf==NULL))  
				return (FAILURE);
			igsc_inf=wmf->igsci;
			if(event==SNOOPING_FLUSH_ENTRY_ALL) {
				igsc_sdb_clear_ipv6_wlc(wlc);
				IGS_IGSDB(":%s:%d  Flush all successful \r\n",__FUNCTION__,__LINE__ );
			}
			else if(event==SNOOPING_FLUSH_ENTRY) {
				IGS_IGSDB(":%s:%d Group:%08x:%08x:%08x:%08x \r\n",__FUNCTION__,__LINE__ ,
						snoop_entry->grp.s6_addr32[0],
						snoop_entry->grp.s6_addr32[1],
						snoop_entry->grp.s6_addr32[2],
						snoop_entry->grp.s6_addr32[3]);
				if(!igsc_sdb_clear_group_ipv6(wlc,&snoop_entry->grp))
					IGS_IGSDB(":%s:%d  Flush successful \r\n",__FUNCTION__,__LINE__ );
				else
					IGS_IGSDB(":%s:%d  Flush unsuccessful \r\n",__FUNCTION__,__LINE__ );
			}
			else if((scb=wlc_scbfind(wlc,bsscfg,(const struct ether_addr *)(&snoop_entry->srcMac[0]))))
			{
				//memcpy(scbaddr.octet,snoop_entry->srcMac,6);
				//if(scb=wlc_scbfind(wlc,bsscfg,&scbaddr)) {
				IGS_IGSDB(":%s:%d  FIND ASSOCIATEDES SCB, this is right case \r\n",__FUNCTION__,__LINE__ );
				switch(event) {
					case SNOOPING_ADD_ENTRY:
						if(!igsc_sdb_member_add_ipv6(igsc_inf,scb,snoop_entry->grp,snoop_entry->rep))
							IGS_IGSDB(":%s:%d  Added entry successful \r\n",__FUNCTION__,__LINE__ );
						else
							IGS_IGSDB(":%s:%d  Failed to add entry \r\n",__FUNCTION__,__LINE__ );
						break;
					case SNOOPING_DEL_ENTRY:
						if(!igsc_sdb_member_del_ipv6(igsc_inf,scb,snoop_entry->grp,snoop_entry->rep))
							IGS_IGSDB(":%s:%d  Del entry successful \r\n",__FUNCTION__,__LINE__ );
						else
							IGS_IGSDB(":%s:%d  Failed to Del entry \r\n",__FUNCTION__,__LINE__ );
						break;
				}
				IGS_IGSDB(":%s:%d From station:%08x:%08x:%08x:%08x \r\n",__FUNCTION__,__LINE__ ,
						snoop_entry->rep.s6_addr32[0],
						snoop_entry->rep.s6_addr32[1],
						snoop_entry->rep.s6_addr32[2],
						snoop_entry->rep.s6_addr32[3]);
				IGS_IGSDB("Src Mac:0x:%02x:%02x:%02x:%02x:%02x:%02x\n",
						snoop_entry->srcMac[0],
						snoop_entry->srcMac[1],
						snoop_entry->srcMac[2],
						snoop_entry->srcMac[3],
						snoop_entry->srcMac[4],
						snoop_entry->srcMac[5]);
						//}
			}
		}
	}
	else 
		IGS_IGSDB("%s:%d  NULL POINTER!!!!!!!!!!!!!!!!!!!!!! \r\n",__FUNCTION__,__LINE__ );
	return 0;
}

/*for the notifier function, we have only one instance for all,so maintain instance 
  count for igsc and do unregister only when last igsc to be removed. */
uint8 igsc_instance_count=0;
void *
igsc_init_ipv6(igsc_info_t *igsc_info, osl_t *osh)
{

	igsc_info->sdb_lock_ipv6 = OSL_LOCK_CREATE("SDB6 Lock");
	if (igsc_info->sdb_lock_ipv6 == NULL)
	{
		igsc_sdb_clear_ipv6(igsc_info);
		MFREE(osh, igsc_info, sizeof(igsc_info_t));
		return (NULL);
	}
	if(++igsc_instance_count==1)
		register_mcast_snooping_notifier(&mcast_snooping_notifer);
	IGS_IGSDB("Initialized IGSDB\n");

	return (igsc_info);
}


void
igsc_exit_ipv6(igsc_info_t *igsc_info)
{
	if(--igsc_instance_count==0)
		unregister_mcast_snooping_notifier(&mcast_snooping_notifer);
	igsc_sdb_clear_ipv6(igsc_info);
	OSL_LOCK_DESTROY(igsc_info->sdb_lock_ipv6);
	return;
}

