/*
 * Linux-specific portion of EAPD
 * (OS dependent file)
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: eapd_linux.c 307774 2012-01-12 15:59:38Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <linux/if_packet.h>
#include <typedefs.h>
#include <bcmutils.h>
#include <proto/ethernet.h>
#include <proto/eapol.h>
#include <proto/eap.h>
#include <wlutils.h>
#include <bcmnvram.h>
#include <eapd.h>
#include <wlif_utils.h>
#include <UdpLib.h>

static eapd_wksp_t *eapd_nwksp = NULL;

static void
eapd_hup_hdlr(int sig)
{
	if (eapd_nwksp)
		eapd_nwksp->flags |= EAPD_WKSP_FLAG_SHUTDOWN;

	return;
}

static void
eapd_chld_hdlr(int sig)
{
	pid_t pid;

	while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
		EAPD_INFO("Reaped %d\n", pid);
}

#ifdef EAPDDUMP
static void
eapd_dump_hdlr(int sig)
{
	if (eapd_nwksp)
		eapd_nwksp->flags |= EAPD_WKSP_FLAG_DUMP;

	return;
}
#endif

static int
eapd_send(eapd_wksp_t *nwksp, int drvSocket, struct iovec *frags, int nfrags)
{
	struct msghdr mh;

	memset(&mh, 0, sizeof(mh));
	mh.msg_name = (caddr_t) NULL;
	mh.msg_namelen = 0;
	mh.msg_iov = frags;
	mh.msg_iovlen = nfrags;

	if (sendmsg(drvSocket, &mh, 0) < 0) {
		EAPD_ERROR("send error %d to drvSocket %d\n", errno, drvSocket);
		return errno;
	}
	else {
		EAPD_INFO("send successful on drvSocket %d\n", drvSocket);
	}

	return 0;
}

/* Send a canned EAPOL packet */
void
eapd_eapol_canned_send(eapd_wksp_t *nwksp, struct eapd_socket *Socket, eapd_sta_t *sta,
                                                    unsigned char code, unsigned char type)
{
	eapol_header_t eapol;
	eap_header_t eap;
	struct iovec frags[2];

	memcpy(&eapol.eth.ether_dhost, &sta->ea, ETHER_ADDR_LEN);
	memcpy(&eapol.eth.ether_shost, &sta->bssid, ETHER_ADDR_LEN);

	eapol.eth.ether_type = htons(ETHER_TYPE_802_1X);
	eapol.version = sta->eapol_version;
	eapol.type = EAP_PACKET;
	eapol.length = htons(type ? (EAP_HEADER_LEN + 1) : EAP_HEADER_LEN);

	eap.code = code;
	eap.id = sta->pae_id;
	eap.length = eapol.length;
	eap.type = type;

	frags[0].iov_base = (caddr_t) &eapol;
	frags[0].iov_len = EAPOL_HEADER_LEN;
	frags[1].iov_base = (caddr_t) &eap;
	frags[1].iov_len = ntohs(eapol.length);

	eapd_send(nwksp, Socket->drvSocket, frags, 2);
}

void
eapd_message_send(eapd_wksp_t *nwksp, struct eapd_socket *Socket, uint8 *pData, int pLen)
{
	struct iovec frags[1];

	frags[0].iov_base = (caddr_t) pData;
	frags[0].iov_len = pLen;

	eapd_send(nwksp, Socket->drvSocket, frags, 1);
}

int
eapd_brcm_open(eapd_wksp_t *nwksp, eapd_brcm_socket_t *sock)
{
	struct ifreq ifr;
	struct sockaddr_ll ll;

	if (nwksp == NULL || sock == NULL) {
		EAPD_ERROR("Wrong arguments...\n");
		return -1;
	}

	sock->drvSocket = socket(PF_PACKET, SOCK_RAW, htons(ETHER_TYPE_BRCM));
	if (sock->drvSocket < 0) {
		EAPD_ERROR("open socket error!!\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));

	strcpy(ifr.ifr_name, sock->ifname);
	if (ioctl(sock->drvSocket, SIOCGIFINDEX, &ifr) != 0) {
		EAPD_ERROR("%s, ioctl(SIOCGIFINDEX), close drvSocket %d\n",
			sock->ifname, sock->drvSocket);
		close(sock->drvSocket);
		sock->drvSocket = -1;
		return -1;
	}

	sock->ifindex = ifr.ifr_ifindex;
	memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_protocol = htons(ETHER_TYPE_BRCM);
	ll.sll_ifindex = sock->ifindex;

	if (bind(sock->drvSocket, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		EAPD_ERROR("%s, bind fail, close drvSocket %d!!\n",
			sock->ifname, sock->drvSocket);
		close(sock->drvSocket);
		sock->drvSocket = -1;
		return -1;
	}
	/* at least one use it */
	sock->inuseCount = 1;

	EAPD_INFO("%s: BRCM socket %d opened\n", ifr.ifr_name, sock->drvSocket);

	return 0;
}

int
eapd_brcm_close(int drvSocket)
{
	close(drvSocket);
	return 0;
}

int
eapd_preauth_open(eapd_wksp_t *nwksp, eapd_preauth_socket_t *sock)
{
	struct ifreq ifr;
	struct sockaddr_ll ll;

	if (nwksp == NULL || sock == NULL) {
		EAPD_ERROR("Wrong arguments...\n");
		return -1;
	}

	sock->drvSocket = socket(PF_PACKET, SOCK_RAW, htons(ETHER_TYPE_802_1X_PREAUTH));
	if (sock->drvSocket < 0) {
		EAPD_ERROR("open socket error!!\n");
		return -1;
	}

	memset(&ifr, 0, sizeof(ifr));

	strcpy(ifr.ifr_name, sock->ifname);
	if (ioctl(sock->drvSocket, SIOCGIFINDEX, &ifr) != 0) {
		EAPD_ERROR("%s, ioctl(SIOCGIFINDEX), close drvSocket %d\n",
			sock->ifname, sock->drvSocket);
		close(sock->drvSocket);
		sock->drvSocket = -1;
		return -1;
	}

	sock->ifindex = ifr.ifr_ifindex;
	memset(&ll, 0, sizeof(ll));
	ll.sll_family = AF_PACKET;
	ll.sll_protocol = htons(ETHER_TYPE_802_1X_PREAUTH);
	ll.sll_ifindex = sock->ifindex;

	if (bind(sock->drvSocket, (struct sockaddr *) &ll, sizeof(ll)) < 0) {
		EAPD_ERROR("%s, bind fail, close drvSocket %d!!\n", sock->ifname, sock->drvSocket);
		close(sock->drvSocket);
		sock->drvSocket = -1;
		return -1;
	}
	/* at least one use it */
	sock->inuseCount = 1;

	EAPD_INFO("%s: preauth socket %d opened\n", ifr.ifr_name, sock->drvSocket);

	return 0;
}

int
eapd_preauth_close(int drvSocket)
{
	close(drvSocket);
	return 0;
}

/*
 * Configuration APIs
 */
int
eapd_safe_get_conf(char *outval, int outval_size, char *name)
{
	char *val;

	if (name == NULL || outval == NULL) {
		if (outval)
			memset(outval, 0, outval_size);
		return -1;
	}

	val = nvram_safe_get(name);
	if (!strcmp(val, ""))
		memset(outval, 0, outval_size);
	else
		snprintf(outval, outval_size, "%s", val);
	return 0;
}

int main(int argc, char* argv[])
{
#ifdef BCMDBG
	char *dbg;
#endif

#ifdef BCMDBG
	/* display usage if nothing is specified */
	if (argc == 2 &&
		(!strncmp(argv[1], "-h", 2) ||
		!strncmp(argv[1], "-H", 2))) {
		eapd_wksp_display_usage();
		return 0;
	}

	/* get eapd_msg_level from nvram */
	if ((dbg = nvram_get("eapd_dbg"))) {
		eapd_msg_level = (uint)strtoul(dbg, NULL, 0);
	}

#endif
	EAPD_INFO("EAP Dispatch Start...\n");
	/* alloc eapd work space */
	if (!(eapd_nwksp = eapd_wksp_alloc_workspace())) {
		EAPD_ERROR("Unable to allocate wksp memory. Quitting...\n");
		return -1;
	}

#if EAPD_WKSP_AUTO_CONFIG
	/* auto config */
	if (argc == 1) {
		if (eapd_wksp_auto_config(eapd_nwksp)) {
			EAPD_ERROR("Unable to auto config. Quitting...\n");
			eapd_wksp_cleanup(eapd_nwksp);
			return -1;
		}
	}
	else if (eapd_wksp_parse_cmd(argc, argv, eapd_nwksp)) {
		EAPD_ERROR("Command line parsing error. Quitting...\n");
		eapd_wksp_cleanup(eapd_nwksp);
		return -1;
	}
#else	/* EAPD_WKSP_AUTO_CONFIG */
	/* parse arguments in parse mode */
	if (eapd_wksp_parse_cmd(argc, argv, eapd_nwksp)) {
		EAPD_ERROR("Command line parsing error. Quitting...\n");
		eapd_wksp_cleanup(eapd_nwksp);
		return -1;
	}
#endif	/* EAPD_WKSP_AUTO_CONFIG */

	/* establish a handler to handle SIGTERM. */
	signal(SIGTERM, eapd_hup_hdlr);

#ifdef EAPDDUMP
	signal(SIGUSR1, eapd_dump_hdlr);
#endif

	signal(SIGCHLD, eapd_chld_hdlr);

	/* run main loop to dispatch messages */
	eapd_wksp_main_loop(eapd_nwksp);

	EAPD_INFO("EAP Dispatcher Stopped...\n");

	return 0;
}

size_t
eapd_message_read(int fd, void *buf, size_t nbytes)
{
	return (read(fd,buf,nbytes));
}
