/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_pktdisp.c 241182 2011-02-17 21:50:03Z $
 *
 * Description: Implement Linux packet dispatcher
 *
 */
#include <wpscli_osl.h>
#include <tutrace.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/time.h>
#include <errno.h>
#include <proto/eapol.h>

#define ETH_8021X_PROT	0x888e

extern char* ether_ntoa(const struct ether_addr *addr);
extern char *wpscli_get_interface_name();

#ifdef _TUDEBUGTRACE
extern void wpscli_print_buf(char *text, unsigned char *buff, int buflen);
#endif

static char if_name[20] = {0};
static int eap_fd = -1; /* descriptor to raw socket  */
static int ifindex = -1; /* interface index */
static uint8 peer_mac[6] = { 0 };

// 
// definitions related to packet dispatcher
//
// open/init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_open(const uint8 *peer_addr)
{
	struct ifreq ifr;
	struct sockaddr_ll ll;
	int err;

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Entered.\n"));

	// Get interface name
	strcpy(if_name, wpscli_get_interface_name());

	if(peer_addr == NULL) {
		printf("Peer MAC address is not specified.\n");
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if (!if_name[0]) {
		printf("Wireless Interface not specified.\n");
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open. if_name=%s peer_addr=[%02X:%02X:%02X:%02X:%02X:%02X]\n", 
		if_name, peer_addr[0], peer_addr[1], peer_addr[2], peer_addr[3], peer_addr[4], peer_addr[5]));

	eap_fd = socket(PF_PACKET, SOCK_RAW, htons(ETH_8021X_PROT));
	if (eap_fd == -1) {
		TUTRACE((TUTRACE_ERR, "UDP Open failed.\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	// Set peer mac address
	if(peer_addr)
		memcpy(peer_mac, peer_addr, 6);

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

	err = ioctl(eap_fd, SIOCGIFINDEX, &ifr);
	if (err < 0) {
		close(eap_fd);
		eap_fd = -1;
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_open: Exiting. Get interface index failed\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	memset(&ll, 0, sizeof(ll));

	ll.sll_family = PF_PACKET;
	ll.sll_ifindex = ifr.ifr_ifindex;
	ifindex = ifr.ifr_ifindex;
	ll.sll_protocol = htons(ETH_8021X_PROT);
	if (bind(eap_fd, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		close(eap_fd);
		eap_fd = -1;
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_open: Exiting. Bind interface failed\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_open: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

// close/un-init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_close()
{
	// Close the socket
	if (eap_fd != -1)
		close(eap_fd);
        return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_set_peer_addr(const uint8 *peer_addr)
{
	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Entered.\n"));

	if(peer_addr == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_set_peer_mac: Exiting. NULL peer_addr is passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	memcpy(peer_mac, peer_addr, 6);

	TUTRACE((TUTRACE_INFO, "wpscli_set_peer_mac: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

// waiting for eap data
brcm_wpscli_status wpscli_pktdisp_wait_for_packet(char* buf, uint32* len, uint32 timeout, bool b_raw)
{
	int recvBytes = 0;
	int fromlen;
	struct sockaddr_ll ll;
	fd_set fdvar;
	struct timeval tv_timeout;


	//TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Entered.\n"));

	if (buf == NULL || len == NULL) {
		TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_INVALID_NULL_PARAM\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	tv_timeout.tv_sec = timeout/1000;  // Convert milli-second to second 
	tv_timeout.tv_usec = (timeout%1000)*1000; // Convert millisecond to microsecond

	FD_ZERO(&fdvar);
	FD_SET(eap_fd, &fdvar);
	if (select(eap_fd + 1, &fdvar, NULL, NULL, &tv_timeout) < 0) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. l2 select recv failed\n"));
		return WPS_STATUS_PKTD_SYSTEM_FAIL;
	}
	
	if (FD_ISSET(eap_fd, &fdvar)) {
		memset(&ll, 0, sizeof(ll));
		fromlen = sizeof(ll);

		recvBytes = recvfrom(eap_fd, buf, *len, 0, (struct sockaddr *)&ll, (socklen_t *)&fromlen);
		if (recvBytes == -1) {
			printf("l2 recv failed; recvBytes = %d\n", recvBytes);
			TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_PKTD_NO_PKT\n"));
			return WPS_STATUS_PKTD_NO_PKT;
		}

		if(b_raw) {
			// Raw packet is expected which is same as we read from socket
			*len = recvBytes;
		}
		else {
			// Otherwise trim out ether header
			eapol_header_t *eapol = (eapol_header_t *)buf;
			*len = recvBytes - sizeof(eapol->eth);
			memmove(buf, &eapol->version, *len);
		}

		TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_wait_for_packet: EAPOL packet received. *len=%d\n", *len));
#ifdef _TUDEBUGTRACE
		wpscli_print_buf("WPS rx", (unsigned char *)buf, *len);
#endif
		//TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_SUCCESS\n"));
		return WPS_STATUS_SUCCESS;
	}

//	TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_wait_for_packet: Exiting. status=WPS_STATUS_PKTD_NO_PKT. FD_ISSET failed.\n"));
	return WPS_STATUS_PKTD_NO_PKT;
}

// send a packet
brcm_wpscli_status wpscli_pktdisp_send_packet(char *dataBuffer, uint32 dataLen)
{
	int sentBytes = 0;
	struct sockaddr_ll ll;

	TUTRACE((TUTRACE_INFO, "Entered: wpscli_pktdisp_send_packet. dataLen=%d peer_addr=[%02X:%02X:%02X:%02X:%02X:%02X]\n", 
		dataLen, peer_mac[0], peer_mac[1], peer_mac[2], peer_mac[3], peer_mac[4], peer_mac[5]));

#ifdef _TUDEBUGTRACE
		wpscli_print_buf("WPS tx", (unsigned char *)dataBuffer, dataLen);
#endif

	if ((!dataBuffer) || (!dataLen)) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_send_packet: Exiting. Invalid Parameters\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_ifindex = ifindex;
	ll.sll_protocol = htons(ETH_8021X_PROT);
	ll.sll_halen = 6;
	memcpy(ll.sll_addr, peer_mac, 6);
	sentBytes = sendto(eap_fd, dataBuffer, dataLen, 0, (struct sockaddr *) &ll, sizeof(ll));

	if (sentBytes != (int32) dataLen) {
		TUTRACE((TUTRACE_ERR, "wpscli_pktdisp_send_packet: Exiting. L2 send failed; sentBytes = %d\n", sentBytes));
		return WPS_STATUS_PKTD_SEND_PKT_FAIL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_send_packet: Exiting. Succeeded.\n"));
	return WPS_STATUS_SUCCESS;
}
