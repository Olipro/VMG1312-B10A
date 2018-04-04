/*
 * Efficient Multicast Forwarding Layer: This module does the efficient
 * layer 2 forwarding of multicast streams, i.e., forward the streams
 * only on to the ports that have corresponding group members there by
 * reducing the bandwidth utilization and latency. It uses the information
 * updated by IGMP Snooping layer to do the optimal forwarding. This file
 * contains the common code routines of EMFL.
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: emfc.c 241182 2011-02-17 21:50:03Z gmo $
 */
#include <typedefs.h>
#include <bcmdefs.h>
#include <bcmendian.h>
#include <proto/ethernet.h>
#include <proto/bcmip.h>
#include <osl.h>
#if defined(linux)
#include <osl_linux.h>
#else /* defined(osl_xx) */
#error "Unsupported osl"
#endif /* defined(osl_xx) */
#include <bcmutils.h>
#include "clist.h"
#include "emf_cfg.h"
#include "emfc_export.h"
#include "emfc.h"
#include "emf_linux.h"
#include <wlc.h>
#include <wlc_scb.h>

extern int br_mld_filter(const unsigned char *dest, void *ipv6);
extern int br_mld_snooping_enabled(void *dev);
extern int ipv6_type(const struct inv6_addr *addr) ;
/*
 * MFDB Listing Function
 */
int32
emfc_mfdb_list_ipv6(emfc_info_t *emfc, emf_cfg_mfdb_list_t *list, uint32 size)
{
    clist_head_t *ptr1, *ptr2;
    emfc_mi_t *mi;
    emfc_mgrp_t *mgrp;
    int32 i, index = 0;

    if (emfc == NULL)
    {
        EMF_ERROR("Invalid EMFC handle passed\n");
        return (FAILURE);
    }

    if (list == NULL)
    {
        EMF_ERROR("Invalid buffer input\n");
        return (FAILURE);
    }

    for (i = 0; i < MFDB_HASHT_SIZE; i++)
    {
        for (ptr1 = emfc->mgrp_fdb_ipv6[i].next;
                ptr1 != &emfc->mgrp_fdb_ipv6[i]; ptr1 = ptr1->next)
        {
            mgrp = clist_entry(ptr1, emfc_mgrp_t, mgrp_hlist);
            EMF_DEBUG("emfc:%p,mgrp:%p,mgrp->mi_head:%p,mgrp->mi_head.next:%p\n", emfc, mgrp,&mgrp->mi_head,mgrp->mi_head.next);
            EMF_DEBUG("Multicast Group entry %08x:%08x:%08x:%08x found\n",
                      mgrp->mgrp_ipv6.s6_addr32[0],
                      mgrp->mgrp_ipv6.s6_addr32[1],
                      mgrp->mgrp_ipv6.s6_addr32[2],
                      mgrp->mgrp_ipv6.s6_addr32[3]);
            for (ptr2 = mgrp->mi_head.next;
                    ptr2 != &mgrp->mi_head; ptr2 = ptr2->next)
            {
                mi = clist_entry(ptr2, emfc_mi_t, mi_list);
                EMF_DEBUG("SCB index:%d, ptr2:%p:\n",index,ptr2);

                /*
                				memcpy(&list->mfdb_entry[index].mgrp_ipv6,&mgrp->mgrp_ipv6,sizeof(inv6_addr));
                				strncpy(list->mfdb_entry[index].if_name,
                				        (uint8 *)((struct scb *)(mi->mi_mhif->mhif_ifp))->ea, 16);
                				list->mfdb_entry[index].pkts_fwd = mi->mi_data_fwd;
                				*/
                index++;
            }
        }
    }

    /* Update the total number of entries 
    list->num_entries = index; 
    */
    return (SUCCESS);
}



static emfc_mi_t *
emfc_mfdb_mi_entry_ipv6_find(emfc_info_t *emfc, emfc_mgrp_t *mgrp, void *ifp)
{
    emfc_mi_t *mi;
    clist_head_t *ptr;

    ASSERT(mgrp);

    for (ptr = mgrp->mi_head.next;
            ptr != &mgrp->mi_head; ptr = ptr->next)
    {
        mi = clist_entry(ptr, emfc_mi_t, mi_list);
        if (ifp == mi->mi_mhif->mhif_ifp)
        {
            return (mi);
        }
    }

    return (NULL);
}



/*
 * Description: This function does the MFDB lookup to locate an ipv6 multicast
 *              group entry.
 *
 * Input:       emfc    - EMFL Common code global data handle
 *              mgrp_ipv6 - Multicast group address of the entry.
 *
 * Return:      Returns NULL is no group entry is found. Otherwise
 *              returns pointer to the MFDB group entry.
 */

static emfc_mgrp_t * emfc_mfdb_group_ipv6_lookup(emfc_info_t *emfc, struct inv6_addr mgrp_ipv6)
{
    uint32 hash;
    emfc_mgrp_t *mgrp;
    clist_head_t *ptr;
    
    /* Do the cache lookup first. Since the multicast video traffic
     * is bursty in nature there is a good chance that the cache
     * hit ratio will be good. If during the testing we find that
     * the hit ratio is not as good then this single entry cache
     * mechanism will be removed.
     */
    if(ipv6_is_same(mgrp_ipv6,emfc->mgrp_cache_ipv6_addr))
    {
        EMFC_STATS_INCR_IPV6(emfc, mfdb_cache_hits);
        return (emfc->mgrp_cache_ipv6_grp);
    }


    EMFC_STATS_INCR_IPV6(emfc, mfdb_cache_misses);

    hash = MFDB_MGRP_HASH_IPV6(mgrp_ipv6);
    EMF_DEBUG(":%s:%d  Hash values is:%d \r\n",__FUNCTION__,__LINE__,hash );

    for (ptr = emfc->mgrp_fdb_ipv6[hash].next;
            ptr != &emfc->mgrp_fdb_ipv6[hash];
            ptr = ptr->next)
    {
        mgrp = clist_entry(ptr, emfc_mgrp_t, mgrp_hlist);
        EMF_DEBUG(":%s:%d  compare entry in Table with coming in \r\n",__FUNCTION__,__LINE__ );
        EMF_DEBUG(":%s:%d  compare entry in Table with coming in \r\n",__FUNCTION__,__LINE__ );
        /* Compare group address */
        if (ipv6_is_same(mgrp_ipv6,mgrp->mgrp_ipv6))
        {
            EMF_DEBUG("Multicast Group entry %08x:%08x:%08x:%08x found\n",
                      mgrp_ipv6.s6_addr32[0],
                      mgrp_ipv6.s6_addr32[1],
                      mgrp_ipv6.s6_addr32[2],
                      mgrp_ipv6.s6_addr32[3]);
            emfc->mgrp_cache_ipv6_grp = mgrp;
            memcpy(&emfc->mgrp_cache_ipv6_addr,&mgrp_ipv6,sizeof(struct inv6_addr));
            return (mgrp);
        }
    }

    return (NULL);
}

uint32 emfc_ipv6_input(emfc_info_t *emfc,void *sdu, void *ifp, uint8 *iph, bool rt_port)
{

    wlc_bsscfg_t *bsscfg;
    wlc_info_t *wlc;
    struct ipv6_hdr *hdr=(struct ipv6_hdr *)iph;
    int dst_type;
    uint32 dest_ip;
    u8 *nextHdr = NULL;
    u8 snooping_enabled=0;
    void *dev;
    bsscfg=(wlc_bsscfg_t *)emfc->emfi;
    wlc=bsscfg->wlc;
    dev=wlc->wl->dev;
    if(likely(dev!=NULL))
    {
        snooping_enabled=br_mld_snooping_enabled(dev);
        EMF_DEBUG("this device is%s, mld is enabled:%d\n",dev->name,snooping_enabled);
    }
    dst_type=ipv6_type(&hdr->daddr);
    if (!(dst_type&IPV6_ADDR_MULTICAST) || (!emfc->emf_enable))
    {
        EMF_DEBUG("IPv6 Unicast frame recevied/EMF disabled\n");
        return (EMF_NOP);
    }
    EMF_DEBUG("Received frame with dest ip %08x:%08x:%08x:%08x\n",
              hdr->daddr.s6_addr32[0],
              hdr->daddr.s6_addr32[1],
              hdr->daddr.s6_addr32[2],
              hdr->daddr.s6_addr32[3]);

    dest_ip = ntoh32(*((uint32 *)(iph + IPV4_DEST_IP_OFFSET)));

    /* Non-IPv6 multicast packets are not handled here*/
    if (hdr->version != IP_VER_6)
    {
        EMF_INFO("Non-IPv6 multicast packets will be flooded\n");
        return (EMF_NOP);
    }

    nextHdr = (u8 *)((u8*)hdr + sizeof(struct ipv6_hdr));
    /* Check the protocol type of multicast frame */
    if ( (hdr->nexthdr == IPPROTO_HOPOPTS) &&
            (*nextHdr == IPPROTO_ICMPV6) &&
            (emfc->snooper != NULL))
    {
        EMF_DEBUG("Received MLD frame type %d\n", *(iph + IPV4_HLEN(iph)));
        EMFC_STATS_INCR_IPV6(emfc, igmp_frames);
#if defined(DSLCPE) && defined(DSLCPE_CACHE_SMARTFLUSH)
        /* snooper func will read data in pkt, so disable flush optimiz */
        PKTSETDIRTYP(emfc->osh, sdu, NULL);
#endif
        //for now, all IPV6 control packets,we send over to stack for MCPD handling.
        //	if(debugcontrol) {
        if(!rt_port)
        {
            ASSERT(emfc->wrapper.sendup_fn);
            emfc->wrapper.sendup_fn(emfc->emfi, sdu);
            return EMF_TAKEN;
        }
        else return EMF_NOP;

    }
    else
    {
        clist_head_t *ptr;
        emfc_mgrp_t *mgrp;
        emfc_mi_t *mi;
        void *sdu_clone;
        int scbcount=0;
        //UPNPfor ipv6???
        if(!br_mld_filter(NULL,(void *)&hdr->daddr))
        {
            //   (BCM_IN6_IS_ADDR_MC_SCOPE0(ipv6)) ||
            //    (BCM_IN6_IS_ADDR_MC_NODELOCAL(ipv6)) ||
            //    (BCM_IN6_IS_ADDR_MC_LINKLOCAL(ipv6)))

            EMF_DEBUG("LinkLocal-nodelocal etc to be flooded\n");
            return EMF_NOP;
        }
        else if(!snooping_enabled)
        {
            //snooping is not enabled for this bridge,but WMF
            //is endabled, we will unicast it to all
            //the assoicated
            struct scb *lscb;
            struct scb_iter scbiter;
            EMF_DEBUG("assocaited SCB count is:%d\n",wlc->pub->assoc_count);
	    /* if there is no association station and from router, this packet should be dropped */
	    if(wlc->pub->assoc_count==0 && rt_port) return EMF_DROP;
            FOREACHSCB(wlc->scbstate, &scbiter, lscb)
            {
                if (SCB_ASSOCIATED(lscb)&&(lscb!=ifp))
                {
                    if((++scbcount==wlc->pub->assoc_count)&&rt_port)
                    {
                        EMF_DEBUG("Send to last associated SCB with original SDU\n");
                        emfc->wrapper.forward_fn(emfc->emfi, sdu,0,lscb,rt_port);
			            return (EMF_TAKEN);
                    }
                    else
                    {
                        if((sdu_clone = PKTDUP(emfc->osh, sdu)) == NULL)
                        {
                            EMFC_STATS_INCR_IPV6(emfc, mcast_data_dropped);
                            return (EMF_DROP);
                        }
                        EMF_DEBUG("Clone skb and send\n");
                        emfc->wrapper.forward_fn(emfc->emfi,sdu_clone,0,lscb,rt_port);
                    }

                }
            }
        }
        else
        {
            //for now,we will unicast the packet to all other STAs
            if((mgrp=emfc_mfdb_group_ipv6_lookup(emfc,hdr->daddr))!=NULL)
            {
                EMF_DEBUG("emfc:%p,mgrp at :%p and mgrp_mi_head.next:%p,mgrp->mi_head:%p\n",
                          emfc,mgrp,mgrp->mi_head.next,&mgrp->mi_head);
                for (ptr = mgrp->mi_head.next; ptr != &mgrp->mi_head; ptr = ptr->next)
                {
                    if(!ptr) break;
                    mi = clist_entry(ptr, emfc_mi_t, mi_list);
                    if ((ifp!=NULL)&&(ifp == mi->mi_mhif->mhif_ifp))
                    {
                        EMF_DEBUG("%s:%d  Received from same STA, from router:%d \r\n",__FUNCTION__,__LINE__,rt_port );
                        continue;
                    }
                    else
                    {
                        if((ptr->next==&mgrp->mi_head)&&rt_port)
                        {
                            EMF_DEBUG("send last one with original skb\n");
                            emfc->wrapper.forward_fn(emfc->emfi, sdu,0,mi->mi_mhif->mhif_ifp,rt_port);
			    return (EMF_TAKEN);
                        }
                        else
                        {
                            if((sdu_clone = PKTDUP(emfc->osh, sdu)) == NULL)
                            {
                                EMFC_STATS_INCR_IPV6(emfc, mcast_data_dropped);
                                return (EMF_DROP);
                            }
                            EMF_DEBUG("Clone skb and send\n");
                            emfc->wrapper.forward_fn(emfc->emfi, sdu_clone,0,mi->mi_mhif->mhif_ifp,rt_port);
                        }
                        EMF_DEBUG(":%s:%d  staring to broadcount this ipv6 MM to SCB index:%d \r\n",__FUNCTION__,__LINE__ ,scbcount++);
                    }

                }
            } else if(rt_port) 
		return EMF_DROP;
        }
	if(!rt_port)
	{
		emfc->wrapper.sendup_fn(emfc->emfi, sdu);
		return (EMF_TAKEN);
	}
    }
    return EMF_NOP;
}


/*
 * Add the entry if not present otherwise return the pointer to
 * the entry.
 */
emfc_mhif_t * emfc_mfdb_mhif_add_ipv6(emfc_info_t *emfc, void *ifp)
{
    emfc_mhif_t *ptr, *mhif;

    for (ptr = emfc->mhif_head_ipv6; ptr != NULL; ptr = ptr->next)
    {
        if (ptr->mhif_ifp == ifp)
        {
            return (ptr);
        }
    }

    /* Add new entry */
    mhif = MALLOC(emfc->osh, sizeof(emfc_mhif_t));
    if (mhif == NULL)
    {
        EMF_ERROR("Failed to alloc mem size %d for mhif entry\n",
                  sizeof(emfc_mhif_t));
        return (NULL);
    }

    mhif->mhif_ifp = ifp;
    mhif->mhif_data_fwd = 0;
    mhif->next = emfc->mhif_head_ipv6;
    emfc->mhif_head_ipv6 = mhif;

    return (mhif);
}


int32 emfc_mfdb_ipv6_membership_add(emfc_info_t *emfc,void *mgrp_ip6, void *ifp)
{
    uint32 hash;
    emfc_mgrp_t *mgrp;
    emfc_mi_t *mi;
    struct inv6_addr *mgrp_ipv6=(struct inv6_addr*)mgrp_ip6;

    OSL_LOCK(emfc->fdb_lock_ipv6);

    /* If the group entry doesn't exist, add a new entry and update
     * the member/interface information.
     */
    mgrp = emfc_mfdb_group_ipv6_lookup (emfc,*mgrp_ipv6);

    if (mgrp == NULL)
    {
        /* Allocate and initialize multicast group entry */
        mgrp = MALLOC(emfc->osh, sizeof(emfc_mgrp_t));
        if (mgrp == NULL)
        {
            EMF_ERROR("Failed to alloc mem size %d for group entry\n",
                      sizeof(emfc_mgrp_t));
            OSL_UNLOCK(emfc->fdb_lock_ipv6);
            return (FAILURE);
        }

        memcpy(&mgrp->mgrp_ipv6,mgrp_ipv6,sizeof(struct inv6_addr));
        clist_init_head(&mgrp->mi_head);

        EMF_DEBUG("Adding ipv6 group entry %08x.%08x.%08x.%08x\n",
                  (mgrp_ipv6->s6_addr32[0]),
                  (mgrp_ipv6->s6_addr32[1]),
                  (mgrp_ipv6->s6_addr32[2]),
                  (mgrp_ipv6->s6_addr32[3]));

        /* Add the group entry to hash table */
        hash = MFDB_MGRP_HASH_IPV6(*mgrp_ipv6);
        clist_add_head(&emfc->mgrp_fdb_ipv6[hash], &mgrp->mgrp_hlist);
        EMF_DEBUG(":%s:%d  Hash values is:%d and it is added to table:%p \r\n",__FUNCTION__,__LINE__,hash,&mgrp->mgrp_hlist);
    }
    else
    {
        mi = emfc_mfdb_mi_entry_ipv6_find(emfc, mgrp, ifp);
        EMF_DEBUG(":%s:%d  find mi \r\n",__FUNCTION__,__LINE__ );

        /* Update the ref count */
        if (mi != NULL)
        {
            mi->mi_ref++;
            OSL_UNLOCK(emfc->fdb_lock_ipv6);
            return (SUCCESS);
        }
    }

    EMF_MFDB("Adding interface entry for interface %p\n", ifp);

    /* Allocate and initialize multicast interface entry */
    mi = MALLOC(emfc->osh, sizeof(emfc_mi_t));
    if (mi == NULL)
    {
        EMF_ERROR("Failed to allocated memory %d for interface entry\n",
                  sizeof(emfc_mi_t));
        if (clist_empty(&mgrp->mi_head))
        {
            clist_delete(&mgrp->mgrp_hlist);
            if( emfc->mgrp_cache_ipv6_grp!=NULL && (emfc->mgrp_cache_ipv6_grp == mgrp))
            {
                emfc->mgrp_cache_ipv6_grp=NULL;
                memset(&emfc->mgrp_cache_ipv6_addr,0,sizeof(struct inv6_addr));
            }
            MFREE(emfc->osh, mgrp, sizeof(emfc_mgrp_t));
        }
        OSL_UNLOCK(emfc->fdb_lock_ipv6);
        return (FAILURE);
    }
#ifdef DSLCPE
    OSL_UNLOCK(emfc->fdb_lock_ipv6);
#endif  /* DSLCPE */

    /* Initialize the multicast interface list entry */
    mi->mi_ref = 1;
    mi->mi_mhif = emfc_mfdb_mhif_add_ipv6(emfc, ifp);
    mi->mi_data_fwd = 0;

#ifdef DSLCPE
    OSL_LOCK(emfc->fdb_lock_ipv6);
#endif  /* DSLCPE */

    /* Add the multicast interface entry */
    clist_add_head(&mgrp->mi_head, &mi->mi_list);

    OSL_UNLOCK(emfc->fdb_lock_ipv6);
    //emfc_mfdb_list_ipv6(emfc,emfc,3);
    return (SUCCESS);
}

/*
 * add Multicast group number to STB MAC to  the table
 * ifp --  sbc
 * grp --  inv6_addr of the group
 * dev --  wireless network device.
 *
 */
int32 emfc_mfdb_ipv6_membership_dev_add(void *dev,void *grp, void *ifp)
{

// FOr now, there seems only have one EMF instance for all the wireless lan, I think this need to be fixed
// TODO:: try to fix EMF instance issue.

    emfc_info_t *emfc=emfc_instance_find("wmf0");
    if(emfc)
        return emfc_mfdb_ipv6_membership_add(emfc,(struct inv6_addr*)grp, ifp);
    else
        return (FAILURE);
}

int32
emfc_mfdb_ipv6_membership_del(emfc_info_t *emfc,void *mgrp_ip6, void *ifp)
{
    emfc_mi_t *mi;
    emfc_mgrp_t *mgrp;
    struct inv6_addr *mgrp_ipv6=(struct  inv6_addr*)mgrp_ip6;
    

    OSL_LOCK(emfc->fdb_lock_ipv6);

    /* Find group entry */
    mgrp = emfc_mfdb_group_ipv6_lookup(emfc, *mgrp_ipv6);

    if (mgrp == NULL)
    {
        OSL_UNLOCK(emfc->fdb_lock);
        return (FAILURE);
    }

    /* Find interface entry */
    mi = emfc_mfdb_mi_entry_ipv6_find(emfc, mgrp, ifp);

    if (mi == NULL)
    {
        OSL_UNLOCK(emfc->fdb_lock);
        return (FAILURE);
    }

    EMF_MFDB("Deleting MFDB interface entry for interface %p\n", ifp);

    /* Delete the interface entry when ref count reaches zero */
    mi->mi_ref--;
    EMF_DEBUG(":%s:%d  mi->ref:%d \r\n",__FUNCTION__,__LINE__,mi->mi_ref);
    EMF_MFDB("Deleting interface entry %p\n", mi->mi_mhif->mhif_ifp);
    clist_delete(&mi->mi_list);

    /* If the member being deleted is last node in the interface list,
     * delete the group entry also.
     */
    if (clist_empty(&mgrp->mi_head))
    {
        EMF_DEBUG("Deleting group entry \n");
        clist_delete(&mgrp->mgrp_hlist);
        memset(&emfc->mgrp_cache_ipv6_addr,0,sizeof(struct inv6_addr));
        MFREE(emfc->osh, mgrp, sizeof(emfc_mgrp_t));
    }

    MFREE(emfc->osh, mi, sizeof(emfc_mi_t));

    OSL_UNLOCK(emfc->fdb_lock_ipv6);

    return (SUCCESS);
}

int32 emfc_mfdb_ipv6_membership_dev_del(void *dev,void *grp, void *ifp)
{

/* FOr now, there seems only have one EMF instance for all the wireless lan, I think this need to be fixed TODO:: try to fix EMF instance issue.
*/
	emfc_info_t *emfc=emfc_instance_find("wmf0");
	if(emfc)
		return emfc_mfdb_ipv6_membership_del(emfc,grp,ifp);
	else
		return (FAILURE);
}

