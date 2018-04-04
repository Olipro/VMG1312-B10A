/*
 * Wireless Multicast Forwarding (WMF)
 *
 * WMF is forwarding multicast packets as unicast packets to
 * multicast group members in a BSS
 *
 * Supported protocol families: IPV4
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_wmf.c 355758 2012-09-08 01:00:02Z $
 */

#include <wlc_cfg.h>
#include <typedefs.h>
#include <bcmdefs.h>
#include <osl.h>
#include <bcmutils.h>
#include <siutils.h>
#include <wlioctl.h>
#include <proto/802.11.h>
#include <proto/ethernet.h>
#include <proto/vlan.h>
#include <proto/802.3.h>
#include <proto/bcmip.h>
#include <proto/bcmarp.h>
#include <proto/bcmudp.h>
#include <proto/bcmdhcp.h>
#include <bcmendian.h>

#include <d11.h>
#include <wlc_rate.h>
#include <wlc_pub.h>
#include <wlc_key.h>
#include <wlc_bsscfg.h>
#include <wlc.h>
#include <wlc_scb.h>
#include <wl_export.h>
#include <wlc_wmf.h>
#include <emf/emf/emf_cfg.h>
#include <emf/emf/emfc_export.h>
#include <emf/igs/igs_cfg.h>
#include <emf/igs/igsc_export.h>

#ifdef WMF_DEBUG
#define WMF_DBG(args) printf args
#else
#define WMF_DBG(args)
#endif
#ifdef DSLCPE  
extern unsigned char wmf_ipv6_enabled;
#endif
struct wmf_info {
	wlc_info_t *wlc;
	int	scb_handle;	/* scb cubby handle */
};

/* forward declarations */
static void wlc_wmf_scb_free_notify(void *context, struct scb *scb);
#ifdef PLC_WET
int32 wmf_forward_plc(void *wrapper, void *p, uint32 mgrp_ip,
            void *txif, bool rt_port);
#endif
int32 wmf_forward(void *wrapper, void *p, uint32 mgrp_ip,
            void *txif, bool rt_port);
void wmf_sendup(void *wrapper, void *p);
int32 wmf_hooks_register(void *wrapper);
int32 wmf_hooks_unregister(void *wrapper);
int32 wmf_igs_broadcast(void *wrapper, uint8 *ip, uint32 length, uint32 mgrp_ip);
static int wlc_wmf_down(void *hdl);

/*
 * WMF module attach function
 */
wmf_info_t *
BCMATTACHFN(wlc_wmf_attach)(wlc_info_t *wlc)
{
	wmf_info_t *wmf;

	if (!(wmf = (wmf_info_t *)MALLOC(wlc->osh, sizeof(wmf_info_t)))) {
		WL_ERROR(("wl%d: wlc_wmf_attach: out of mem, malloced %d bytes\n",
		          wlc->pub->unit, MALLOCED(wlc->osh)));
		return NULL;
	}

	bzero((char *)wmf, sizeof(wmf_info_t));
	wmf->wlc = wlc;

	/* reserve cubby in the scb container for per-scb private data */
	wmf->scb_handle = wlc_scb_cubby_reserve(wlc, 0, NULL, wlc_wmf_scb_free_notify,
		NULL, (void *)wmf);
	if (wmf->scb_handle < 0) {
		WL_ERROR(("wl%d: wlc_wmf_attach: wlc_scb_cubby_reserve failed\n", wlc->pub->unit));
		MFREE(wlc->osh, wmf, sizeof(wmf_info_t));
		return NULL;
	}

	/* register module */
	if (wlc_module_register(wlc->pub, NULL, "wmf", wmf, NULL, NULL, NULL, wlc_wmf_down)) {
		WL_ERROR(("wl%d: %s wlc_module_register() failed\n",
		          wlc->pub->unit, __FUNCTION__));
		return NULL;
	}

	return wmf;
}

/*
 * WMF module detach function
 */
void
BCMATTACHFN(wlc_wmf_detach)(wmf_info_t *wmf)
{
	if (!wmf)
		return;

	wlc_module_unregister(wmf->wlc->pub, "wmf", wmf);
	MFREE(wmf->wlc->osh, wmf, sizeof(wmf_info_t));
}

/*
 * SCB free notification
 * We call the WMF specific interface delete function
 */

static void
wlc_wmf_scb_free_notify(void *context, struct scb *scb)
{
	if (!scb->bsscfg->wmf_instance)
		return;

	/* Delete the station from  WMF list */
	wlc_wmf_sta_del(scb->bsscfg, scb);
}

/*
 * Description: This function is called to instantiate emf
 *		and igs instance on enabling WMF on a
 *              bsscfg.
 *
 * Input:       wlc - pointer to the wlc_info_t structure
 *              bsscfg - pointer to the bss configuration
 */
int32
wlc_wmf_instance_add(wlc_info_t *wlc, struct wlc_bsscfg *bsscfg)
{
	wlc_wmf_instance_t *wmf_inst;
	emfc_wrapper_t wmf_emfc;
	igsc_wrapper_t wmf_igsc;
	char inst_id[10];

	wmf_inst = MALLOC(wlc->osh, sizeof(wlc_wmf_instance_t));
	if (!wmf_inst) {
		WL_ERROR(("%s: MALLOC failed, malloced %d bytes\n",
		          __FUNCTION__, MALLOCED(wlc->osh)));
		return BCME_ERROR;
	}

	/* Fill in the wmf efmc wrapper functions */
#ifdef PLC_WET
	wmf_emfc.forward_fn = PLC_ENAB(wlc->pub) ? wmf_forward_plc : wmf_forward;
#else
	wmf_emfc.forward_fn = wmf_forward;
#endif
	wmf_emfc.sendup_fn = wmf_sendup;
	wmf_emfc.hooks_register_fn = wmf_hooks_register;
	wmf_emfc.hooks_unregister_fn = wmf_hooks_unregister;

	/* Create Instance ID */
	sprintf(inst_id, "wmf%d", bsscfg->_idx);

	/* Create EMFC instance for WMF */
	wmf_inst->emfci = emfc_init((int8 *)inst_id, (void *)bsscfg, wlc->osh, &wmf_emfc);
	if (wmf_inst->emfci == NULL) {
		WL_ERROR(("%s: WMF EMFC init failed\n", __FUNCTION__));
		MFREE(wlc->osh, wmf_inst, sizeof(wlc_wmf_instance_t));
		return BCME_ERROR;
	}

	WMF_DBG(("Created EMFC instance\n"));

	/* Fill in the wmf igsc wrapper functions */
	wmf_igsc.igs_broadcast = wmf_igs_broadcast;

	/* Create IGSC instance */
	wmf_inst->igsci = igsc_init((int8 *)inst_id, (void *)bsscfg, wlc->osh, &wmf_igsc);
	if (wmf_inst->igsci == NULL) {
		WL_ERROR(("%s: WMF IGSC init failed\n", __FUNCTION__));
		/* Free the earlier allocated resources */
		emfc_exit(wmf_inst->emfci);
		MFREE(wlc->osh, wmf_inst, sizeof(wlc_wmf_instance_t));
		return BCME_ERROR;
	}

	WMF_DBG(("Created IGSC instance\n"));

	/* Set the wlc pointer in the wmf instance */
	wmf_inst->wlc = wlc;

	/* Set the wmf instance pointer inside the bsscfg */
	bsscfg->wmf_instance = wmf_inst;

	WMF_DBG(("Addding WLC wmf instance\n"));

	return BCME_OK;
}

/*
 * Description: This function is called to destroy emf
 *		and igs instances on disabling WMF on
 *              a bsscfg.
 *
 * Input:       bsscfg - pointer to the bss configuration
 */
void
wlc_wmf_instance_del(wlc_bsscfg_t *bsscfg)
{
	WMF_DBG(("Deleteing WLC wmf instance\n"));

	if (!bsscfg->wmf_instance)
		return;

	/* Free the EMFC instance */
	emfc_exit(bsscfg->wmf_instance->emfci);

	/* Free the IGSC instance */
	igsc_exit(bsscfg->wmf_instance->igsci);

	/* Free the WMF instance */
	MFREE(bsscfg->wlc->osh, bsscfg->wmf_instance, sizeof(wlc_wmf_instance_t));

	/* Make the pointer NULL */
	bsscfg->wmf_instance = NULL;

	return;
}

/*
 * Description: This function is called to start wmf operation
 *              when a bsscfg is up
 *
 * Input:       bsscfg - pointer to the bss configuration
 */
int
wlc_wmf_start(wlc_bsscfg_t *bsscfg)
{
	emf_cfg_request_t req;
	char inst_id[10];

	WMF_DBG(("Starting WLC wmf instance\n"));

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	sprintf(inst_id, "wmf%d", bsscfg->_idx);
	strcpy((char *)req.inst_id, inst_id);
	req.command_id = EMFCFG_CMD_EMF_ENABLE;
	req.size = sizeof(bool);
	req.oper_type = EMFCFG_OPER_TYPE_SET;
	*(bool *)req.arg = TRUE;

	emfc_cfg_request_process(bsscfg->wmf_instance->emfci, &req);
	if (req.status != EMFCFG_STATUS_SUCCESS) {
		WL_ERROR(("%s failed\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return BCME_OK;
}

/*
 * Description: This function is called to stop wmf
 *		operation when bsscfg is down
 *
 * Input:       bsscfg - pointer to the bss configuration
 */
void
wlc_wmf_stop(wlc_bsscfg_t *bsscfg)
{
	emf_cfg_request_t req;
	char inst_id[10];

	if (!bsscfg->wmf_instance) {
		return;
	}

	WMF_DBG(("Stopping WLC wmf instance\n"));
	bzero((char *)&req, sizeof(emf_cfg_request_t));

	sprintf(inst_id, "wmf%d", bsscfg->_idx);
	strcpy((char *)req.inst_id, inst_id);
	req.command_id = EMFCFG_CMD_EMF_ENABLE;
	req.size = sizeof(bool);
	req.oper_type = EMFCFG_OPER_TYPE_SET;
	*(bool *)req.arg = FALSE;

	emfc_cfg_request_process(bsscfg->wmf_instance->emfci, &req);
	if (req.status != EMFCFG_STATUS_SUCCESS) {
		WL_ERROR(("%s failed\n", __FUNCTION__));
		return;
	}
}

/*
 * Description: This function is called to delete a station
 *		to emfc interface list when it is
 *              disassociated to the BSS
 *
 * Input:       bsscfg - pointer to the bss configuration
 *              scb - pointer to the scb
 */
int
wlc_wmf_sta_del(wlc_bsscfg_t *bsscfg, struct scb *scb)
{

	WMF_DBG(("Deleting station from WLC wmf instance\n"));

	if (igsc_sdb_interface_del(bsscfg->wmf_instance->igsci, scb) != SUCCESS) {
		WL_ERROR(("%s failed\n", __FUNCTION__));
		return BCME_ERROR;
	}

	if (igsc_interface_rtport_del(bsscfg->wmf_instance->igsci, scb) != SUCCESS) {
		WL_ERROR(("%s failed\n", __FUNCTION__));
		return BCME_ERROR;
	}

	return BCME_OK;
}

#ifdef PLC_WET
int32
wmf_forward_plc(void *wrapper, void *p, uint32 mgrp_ip, void *node, bool rt_port)
{
	struct ether_header *eh;
	wlc_bsscfg_t *bsscfg;
	osl_t *osh;
	wlc_pub_t *pub;

	WMF_DBG(("Calling WMF forward\n"));

	bsscfg = (wlc_bsscfg_t *)wrapper;

	osh = bsscfg->wlc->osh;
	pub = bsscfg->wlc->pub;

	/* For now force a copy assuming that shared attribute in packet
	 * is not common case. Packet will be shared only when two end
	 * clients are streaming the same video.
	 */
	if (PKTSHARED(p)) {
		void *pkt;
		uint32 totlen;

		totlen = pkttotlen(pub->osh, p);

		WMF_DBG(("wl%d: %s: Copying %d bytes\n", pub->unit, __FUNCTION__, totlen));

		if ((pkt = PKTGET(pub->osh, TXOFF + totlen, TRUE)) == NULL) {
			WL_ERROR(("wl%d: %s: PKTGET of length %d failed\n",
			          pub->unit, __FUNCTION__, TXOFF + PKTLEN(pub->osh, p)));
			PKTFREE(osh, p, TRUE);
			WLCNTINCR(pub->_cnt->txnobuf);
			return FAILURE;
		}
		PKTPULL(pub->osh, pkt, TXOFF);

		wlc_pkttag_info_move(pub, p, pkt);
		PKTSETPRIO(pkt, PKTPRIO(p));

		/* Copy packet data to new buffer */
		pktcopy(pub->osh, p, 0, totlen, PKTDATA(osh, pkt));

		/* Free original packet */
		PKTFREE(osh, p, TRUE);

		p = pkt;
	}

	eh = (struct ether_header *) PKTDATA(osh, p);
	WMF_DBG(("Group Addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
	         eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
	         eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]));

	/* Fill in the unicast address of the station into the ether dest */
	wl_plc_node_mac(bsscfg->wlc->wl, node, eh->ether_dhost);

	WMF_DBG(("Unicast Addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
	         eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
	         eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]));

	/* If node is wifi only then skip plc. Send it over plc either when
	 * global plc path is enabled or destination node is plc only or
	 * if it is configured with plc affinity.
	 */
	if (PLC_ENAB(pub) && wl_plc_node_pref_plc(bsscfg->wlc->wl, node)) {
		wl_plc_loop(bsscfg->wlc->wl, p, bsscfg->wlcif->wlif);
		return SUCCESS;
	}

	/* Send the packet using bsscfg wlcif */
	wlc_sendpkt(bsscfg->wlc, p, bsscfg->wlcif);

	return SUCCESS;
}
#endif /* PLC_WET */

/*
 * Description: This function is called by EMFC layer to
 *		forward a frame on an interface
 *
 * Input:       wrapper - pointer to the bss configuration
 *              p     - Pointer to the packet buffer.
 *              mgrp_ip - Multicast destination address.
 *              txif    - Interface to send the frame on.
 *              rt_port    - router port or not
 */
int32
wmf_forward(void *wrapper, void *p, uint32 mgrp_ip,
            void *txif, bool rt_port)
{
	struct ether_header *eh;
	wlc_bsscfg_t *bsscfg;
	struct scb *scb;
	osl_t *osh;

	WMF_DBG(("Calling WMF forward\n"));

	bsscfg = (wlc_bsscfg_t *)wrapper;
	osh = bsscfg->wlc->osh;
	scb = (struct scb *)txif;
	WMF_DBG(("scb %p associated %d\n", scb, scb ? SCB_ASSOCIATED(scb) : 0));
	if (!scb || !SCB_ASSOCIATED(scb)) {
		if (p != NULL)
			PKTFREE(osh, p, FALSE);
		return FAILURE;
	}

	/* Since we are going to modify the header below and the
	 * packet may be shared we allocate a header buffer and
	 * prepend it to the original sdu.
	 */
	if (PKTSHARED(p)) {
		void *pkt;

		if ((pkt = PKTGET(osh, TXOFF + ETHER_HDR_LEN, TRUE)) == NULL) {
			WL_ERROR(("wl%d: %s: PKTGET headroom %d failed\n",
			          bsscfg->wlc->pub->unit, __FUNCTION__, TXOFF));
			WLCNTINCR(bsscfg->wlc->pub->_cnt->txnobuf);
			WLCIFCNTINCR(scb, txnobuf);
			WLCNTSCBINCR(scb->scb_stats.tx_failures);
#ifdef DSLCPE
			PKTFREE(osh, p, FALSE);
#endif
			return FAILURE;
		}
		PKTPULL(osh, pkt, TXOFF);

		wlc_pkttag_info_move(bsscfg->wlc->pub, p, pkt);
		PKTSETPRIO(pkt, PKTPRIO(p));

		/* Copy ether header from data buffer to header buffer */
		memcpy(PKTDATA(osh, pkt), PKTDATA(osh, p), ETHER_HDR_LEN);
		PKTPULL(osh, p, ETHER_HDR_LEN);

		/* Chain original sdu onto newly allocated header */
		PKTSETNEXT(osh, pkt, p);
		p = pkt;
	}

	/* Fill in the unicast address of the station into the ether dest */
	eh = (struct ether_header *) PKTDATA(osh, p);
	memcpy(eh->ether_dhost, &scb->ea, ETHER_ADDR_LEN);

	WMF_DBG(("Group Addr: %02x:%02x:%02x:%02x:%02x:%02x\n",
	         eh->ether_dhost[0], eh->ether_dhost[1], eh->ether_dhost[2],
	         eh->ether_dhost[3], eh->ether_dhost[4], eh->ether_dhost[5]));


	/* Send the packet using bsscfg wlcif */
	wlc_sendpkt(bsscfg->wlc, p, bsscfg->wlcif);

	return SUCCESS;
}

/*
 * Description: This function is called by EMFC layer to
 *		send up a frame
 *
 * Input:       wrapper - pointer to the bss configuration
 *              p     - Pointer to the packet buffer.
 */
void
wmf_sendup(void *wrapper, void *p)
{
	wlc_bsscfg_t *bsscfg;

	WMF_DBG(("Calling WMF Sendup\n"));

	bsscfg = (wlc_bsscfg_t *)wrapper;
	if (!bsscfg->wmf_instance) {
#ifdef DSLCPE
		if (p != NULL)
			PKTFREE(bsscfg->wlc->osh, p, FALSE);
#endif
		WL_ERROR(("%s: Cannot send packet up becasue WMF instance does not exist\n",
		          __FUNCTION__));
		return;
	}


	/* Send the packet up */
	wl_sendup(bsscfg->wmf_instance->wlc->wl, bsscfg->wlcif->wlif, p, 1);
}

/*
 * Description: This function is called to broadcast an IGMP query
 *		to the BSS
 *
 * Input:       wrapper - pointer to the bss configuration
 *		ip      - pointer to the ip header
 *		length  - length of the packet
 *		mgrp_ip - multicast group ip
 */
int32
wmf_igs_broadcast(void *wrapper, uint8 *ip, uint32 length, uint32 mgrp_ip)
{
	void *pkt;
	wlc_bsscfg_t *bsscfg;
	struct ether_header *eh;

	WMF_DBG(("Calling WMF IGS broadcast\n"));

	bsscfg = (wlc_bsscfg_t *)wrapper;

	if (!bsscfg->wmf_instance) {
		WL_ERROR(("%s: Cannot send IGMP query because WMF instance does not exist\n",
		          __FUNCTION__));
		return FAILURE;
	}

	/* Allocate the packet, copy the ip part */
	pkt = PKTGET(bsscfg->wlc->osh, length + ETHER_HDR_LEN, TRUE);
	if (pkt == NULL) {
		WL_ERROR(("%s: Out of memory allocating IGMP Query packet\n", __FUNCTION__));
		return FAILURE;
	}

	/* Add the ethernet header */
	eh = (struct ether_header *)PKTDATA(bsscfg->wlc->pub->osh, pkt);
	eh->ether_type = hton16(ETHER_TYPE_IP);
	ETHER_FILL_MCAST_ADDR_FROM_IP(eh->ether_dhost, mgrp_ip);

	/* Add bsscfg address as the source ether address */
	memcpy(eh->ether_shost, &bsscfg->cur_etheraddr, ETHER_ADDR_LEN);

	/* Copy the IP part */
	memcpy((uint8 *)eh + ETHER_HDR_LEN, ip, length);

	/* Send the frame on to the bss */
	wlc_sendpkt(bsscfg->wmf_instance->wlc, pkt, bsscfg->wlcif);

	return SUCCESS;
}

/*
 * Description: This function is called to register hooks
 *		into wl for packet reception
 *
 * Input:       wrapper  - pointer to the bsscfg
 */
int32
wmf_hooks_register(void *wrapper)
{
	WMF_DBG(("Calling WMF hooks register\n"));

	/*
	 * We dont need to do anything here. WMF enable status will be checked
	 * in the wl before handing off packets to WMF
	 */

	return BCME_OK;
}

/*
 * Description: This function is called to unregister hooks
 *		into wl for packet reception
 *
 * Input:       wrapper  - pointer to the bsscfg
 */
int32
wmf_hooks_unregister(void *wrapper)
{
	WMF_DBG(("Calling WMF hooks unregister\n"));

	/*
	 *  We dont need to do anything here. WMF enable status will be checked
	 * in the wl before handing off packets to WMF
	 */

	return BCME_OK;
}

/*
 * Description: This function is called to do the packet handling by
 *		WMF
 *
 * Input:       bsscfg - pointer to the bss configuration
 *              scb - pointer to the station scb
 *              p - pointer to the packet
 *		frombss - packet from BSS or DS
 */
int
wlc_wmf_packets_handle(wlc_bsscfg_t *bsscfg, struct scb *scb, void *p, bool frombss)
{
	uint8 *iph;
	struct ether_header *eh;
#ifdef WL_IGMP_UCQUERY
	struct scb *scb_ptr;
	struct scb_iter scbiter;
	void *sdu_clone;
#ifdef WMF_DEBUG
	char da[ETHER_ADDR_STR_LEN];
#endif /* WMF_DEBUG */
#endif /* WL_IGMP_UCQUERY */

	/* If the WMF instance is not yet created return */
	if (!bsscfg->wmf_instance)
		return WMF_NOP;

	eh = (struct ether_header *)PKTDATA(bsscfg->wlc->pub->osh, p);
	iph = (uint8 *)eh + ETHER_HDR_LEN;

	/* Only IP packets are handled */
	if ((ntoh16(eh->ether_type) != ETHER_TYPE_IP)&&(ntoh16(eh->ether_type) != ETHER_TYPE_IPV6))
		return WMF_NOP;

	/* Change interface to primary interface of proxySTA */
	if (frombss && scb && scb->psta_prim) {
		scb = scb->psta_prim;
	}

#ifdef WL_IGMP_UCQUERY
	/* check if igmp query pkt? */
	if (WL_IGMP_UCQUERY_ENAB(bsscfg) && (!frombss)) {
		if ((IP_VER(iph) == IP_VER_4) && (IPV4_PROT(iph) == IP_PROT_IGMP) &&
			(*(iph + IPV4_HLEN(iph)) == IGMPV2_HOST_MEMBERSHIP_QUERY)) {
			/* Convert igmp query to unicast for each assoc STA */
			FOREACH_BSS_SCB(bsscfg->wlc->scbstate, &scbiter, bsscfg, scb_ptr) {
				/* Skip sending to proxy interfaces of proxySTA */
				if (scb_ptr->psta_prim != NULL) {
					continue;
				}
				WMF_DBG(("wl%d: %s: send Query to to %s\n",
					bsscfg->wlc->pub->unit, __FUNCTION__,
					bcm_ether_ntoa(&scb_ptr->ea, da)));
				if ((sdu_clone = PKTDUP(bsscfg->wlc->osh, p)) == NULL)
					return (WMF_NOP);
				wmf_forward(bsscfg, sdu_clone, 0, scb_ptr, !frombss);
			}
			PKTFREE(bsscfg->wlc->osh, p, TRUE);
			return WMF_TAKEN;
		}
	}
#endif /* WL_IGMP_UCQUERY */
	
	//WMF_DBG("JXUJXU:%s:%d wmfenabled:   \r\n",__FUNCTION__,__LINE__);
#ifdef DSLCPE  
	if (wmf_ipv6_enabled && (ntoh16(eh->ether_type) == ETHER_TYPE_IPV6))
		return (emfc_ipv6_input(bsscfg->wmf_instance->emfci, p, scb, iph, !frombss));
	/* Only IP packets are handled */
	if (ntoh16(eh->ether_type) == ETHER_TYPE_IP)
		return (emfc_input(bsscfg->wmf_instance->emfci, p, scb, iph, !frombss));
	return WMF_NOP;
#else
	return (emfc_input(bsscfg->wmf_instance->emfci, p, scb, iph, !frombss));
#endif
}

/*
 * WMF module down function
 * Does not do anything now
 */
static
int wlc_wmf_down(void *hdl)
{
	return 0;
}

/* Enable/Disable  sending multicast packets to host  for WMF instance */
int wlc_wmf_mcast_data_sendup(wlc_bsscfg_t *bsscfg, bool set, bool enable)
{
	emf_cfg_request_t req;

	if (!bsscfg->wmf_instance) {
		WL_ERROR(("%s failed: WMF not enabled\n", __FUNCTION__));
		return BCME_ERROR;
	}

	bzero((char *)&req, sizeof(emf_cfg_request_t));

	snprintf((char *)req.inst_id, sizeof(req.inst_id), "wmf%d", bsscfg->_idx);
	req.command_id = EMFCFG_CMD_MC_DATA_IND;
	req.size = sizeof(bool);
	req.oper_type = EMFCFG_OPER_TYPE_GET;
	if (set) {
		req.oper_type = EMFCFG_OPER_TYPE_SET;
		*(bool *)req.arg = enable;
	}

	emfc_cfg_request_process(bsscfg->wmf_instance->emfci, &req);
	if (req.status != EMFCFG_STATUS_SUCCESS) {
		WL_ERROR(("%s failed\n", __FUNCTION__));
		return BCME_ERROR;
	}
	if (set) {
		return BCME_OK;
	}
	return ((int)(*((bool *)req.arg)));
}
