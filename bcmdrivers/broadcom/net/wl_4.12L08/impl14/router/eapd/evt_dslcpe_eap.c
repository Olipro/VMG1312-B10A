/*
 * Application-specific portion of EAPD
 * (Wlan Driver Event-Specific Handling)
 *
 * Copyright (C) 2008, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <wlutils.h>
#include <eapd.h>
#include <shutils.h>
#include <UdpLib.h>
#include <security_ipc.h>

/* Receive message from evt module (Located in wldaemon)  */
void evt_app_recv_handler(eapd_wksp_t *nwksp, char *wlifname, eapd_cb_t *from,
	uint8 *pData, int *pLen, struct ether_addr *ap_ea)	
{
	/* This is a stub function for further usage */
	return;
}

/* Set Event Mask to allow wlmngr to receive wlan Event from Driver */
void evt_app_set_eventmask(eapd_app_t *app)
{

	memset(app->bitvec, 0, sizeof(app->bitvec));

	setbit(app->bitvec, WLC_E_AUTH_IND);
	setbit(app->bitvec, WLC_E_DEAUTH_IND);
	setbit(app->bitvec, WLC_E_ASSOC_IND);
	setbit(app->bitvec, WLC_E_REASSOC_IND);
	setbit(app->bitvec, WLC_E_DISASSOC_IND);
#ifdef DSLCPE_EVT
	/* add desired event map here */
	setbit(app->bitvec, WLC_E_PSK_SUP);
#endif /* DSLCPE_EVT */
	return;
}

/* Initialize the parameters */
int evt_app_init(eapd_wksp_t *nwksp)
{
	int ret, reuse = 1;
	eapd_evt_t *evt;
	eapd_cb_t *cb;

	struct sockaddr_in sockaddr;


	if (nwksp == NULL)
		return -1;

	evt = &nwksp->evt;
	evt->appSocket = -1;

	cb = evt->cb;
	if (cb == NULL) {
		EAPD_INFO("No any interface is running EVT !\n");
		return 0;
	}

	while (cb) {
		EAPD_INFO("init brcm interface %s \n", cb->ifname);
		cb->brcmSocket = eapd_add_brcm(nwksp, cb->ifname);
		if (!cb->brcmSocket)
			return -1;

		cb = cb->next;
	}

	evt->appSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	
	if (evt->appSocket < 0) {
		EAPD_ERROR("UDP Open failed.\n");
		return -1;
	}


	if (setsockopt(evt->appSocket, SOL_SOCKET, SO_REUSEADDR, (char*)&reuse,
		sizeof(reuse)) < 0) {
		EAPD_ERROR("UDP setsockopt failed.\n");
		close(evt->appSocket);
		evt->appSocket = -1;
		return -1;
	}

	memset(&sockaddr, 0, sizeof(sockaddr));
	sockaddr.sin_family = AF_INET;
	sockaddr.sin_addr.s_addr = htonl(INADDR_ANY); //htonl(INADDR_LOOPBACK);
	sockaddr.sin_port = htons(EAPD_WKSP_EVT_UDP_RPORT);
	ret = bind(evt->appSocket, (struct sockaddr *)&sockaddr, sizeof(sockaddr));
		
	if (ret < 0) {
		EAPD_ERROR("UDP Bind failed, close evt appSocket %d\n", evt->appSocket);
		close(evt->appSocket);
		evt->appSocket = -1;
		return -1;
	}
	EAPD_INFO("EVT appSocket %d opened\n", evt->appSocket);

	return 0;
}

int evt_app_deinit(eapd_wksp_t *nwksp)
{
	eapd_evt_t *evt;
	eapd_cb_t *cb, *tmp_cb;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}
	evt = &nwksp->evt;
	cb = evt->cb;
	while (cb) {
		/* brcm drvSocket delete */
		if (cb->brcmSocket) {
			EAPD_INFO("close evt brcmSocket %d\n", cb->brcmSocket->drvSocket);
			eapd_del_brcm(nwksp, cb->brcmSocket);
		}

		tmp_cb = cb;
		cb = cb->next;
		free(tmp_cb);
	}

	/* close appSocket for evt */
	if (evt->appSocket >= 0) {
		EAPD_INFO("close  evt appSocket %d\n", evt->appSocket);
		close(evt->appSocket);
		evt->appSocket = -1;
	}

	return 0;
}

/* Send WLAN Event to Wlmngr */
static int evt_app_sendup(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from)
{
	eapd_evt_t *evt;

	if (nwksp == NULL) {
		EAPD_ERROR("Wrong argument...\n");
		return -1;
	}

	evt = &nwksp->evt;
	if (evt->appSocket >= 0) {
		/* send to evt */
		int sentBytes = 0;
		struct sockaddr_in to;

		to.sin_addr.s_addr = inet_addr(EAPD_WKSP_UDP_ADDR);
		to.sin_family = AF_INET;
		to.sin_port = htons(EAPD_WKSP_EVT_UDP_SPORT);

		EAPD_INFO("%s@%d evt->appSocket=%d\n", __FUNCTION__, __LINE__, evt->appSocket );

		/* Send Event Data to wlmngr */
		sentBytes = sendto(evt->appSocket, pData, Len, 0, (struct sockaddr *)&to, sizeof(struct sockaddr_in));

		if (sentBytes != Len) {
			EAPD_ERROR("UDP send to evt on %s failed; sentBytes = %d\n",
				from, sentBytes);
		}
		else {
			EAPD_INFO("send %d bytes to evt on %s: port=%d\n", sentBytes, from, to.sin_port);
		}
	}
	else {
		EAPD_ERROR("evt appSocket not created\n");
	}

	return 0;
}

/* First handling WLAN Event */
int evt_app_handle_event(eapd_wksp_t *nwksp, uint8 *pData, int Len, char *from)
{
	int type;
	eapd_evt_t *evt;
	eapd_cb_t *cb;
	bcm_event_t *dpkt = (bcm_event_t *) pData;
	wl_event_msg_t *event;

	event = &(dpkt->event);
	type = ntohl(event->event_type);

	evt = &nwksp->evt;
	cb = evt->cb;
	while (cb) {
		if (isset(evt->bitvec, type) &&
			!strcmp(cb->ifname, from)) {

			EAPD_INFO("Send from intf: %s\n", cb->ifname );
			evt_app_sendup(nwksp, pData, Len, cb->ifname);
			break;
		}
		cb = cb->next;
	}
	return 0;
}

