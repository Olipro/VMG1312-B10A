/*
 * Broadcom UPnP module linux OS dependent implementation
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: linux_osl.c 241192 2011-02-17 21:52:25Z gmo $
 */
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <ctype.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if_arp.h>

#define __KERNEL__
#include <asm/types.h>
#include <linux/sockios.h>
#include <linux/ethtool.h>

#include <shutils.h>
#include <upnp.h>


/*
 * The following functions are required by the
 * upnp engine, which the OSL has to implement.
 */
int
upnp_osl_ifaddr(const char *ifname, struct in_addr *inaddr)
{
	int sockfd;
	struct ifreq ifreq;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(sockfd, SIOCGIFADDR, &ifreq) < 0) {
		close(sockfd);
		return -1;
	}
	else {
		memcpy(inaddr, &(((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr),
			sizeof(struct in_addr));
	}

	close(sockfd);
	return 0;
}

int
upnp_osl_netmask(const char *ifname, struct in_addr *inaddr)
{
	int sockfd;
	struct ifreq ifreq;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(sockfd, SIOCGIFNETMASK, &ifreq) < 0) {
		close(sockfd);
		return -1;
	}
	else {
		memcpy(inaddr, &(((struct sockaddr_in *)&ifreq.ifr_addr)->sin_addr),
			sizeof(struct in_addr));
	}

	close(sockfd);
	return 0;
}

int
upnp_osl_hwaddr(const char *ifname, char *mac)
{
	int sockfd;
	struct ifreq  ifreq;

	if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	strncpy(ifreq.ifr_name, ifname, IFNAMSIZ);
	if (ioctl(sockfd, SIOCGIFHWADDR, &ifreq) < 0) {
		close(sockfd);
		return -1;
	}
	else {
		memcpy(mac, ifreq.ifr_hwaddr.sa_data, 6);
	}

	close(sockfd);
	return 0;
}

/* Create a udp socket with a specific ip and port */
int
upnp_open_udp_socket(struct in_addr addr, unsigned short port)
{
	int s;
	int reuse = 1;
	struct sockaddr_in sin;

	/* create UDP socket */
	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0)
		return -1;

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		upnp_syslog(LOG_ERR, "Cannot set socket option (SO_REUSEPORT)");
	}

	/* bind socket to recive discovery */
	memset((char *)&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr = addr;

	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		upnp_syslog(LOG_ERR, "bind failed!");
		close(s);
		return -1;
	}

	return s;
}

/* Create a tcp socket with a specific ip and port */
int
upnp_open_tcp_socket(struct in_addr addr, unsigned short port)
{
	int s;
	int reuse = 1;
	struct sockaddr_in sin;

	s = socket(AF_INET, SOCK_STREAM, 0);
	if (s < 0)
		return -1;

	if (setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
		upnp_syslog(LOG_ERR, "Cannot set socket option (SO_REUSEPORT)");
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	sin.sin_port = htons(port);
	sin.sin_addr = addr;

	if (bind(s, (struct sockaddr *)&sin, sizeof(sin)) < 0 ||
	    listen(s, MAX_WAITS) < 0) {
		close(s);
		return -1;
	}

	return s;
}

#ifdef __CONFIG_NAT__
#ifdef __BCMIGD__

#define _PATH_PROCNET_DEV           "/proc/net/dev"

#include <InternetGatewayDevice.h>

/*
 * The functions below are required by the
 * upnp device, for example, InternetGatewayDevice.
 */
char *
get_name(char *name, char *p)
{
	while (isspace(*p))
		p++;

	while (*p) {
		/* Eat white space */
		if (isspace(*p))
			break;

		/* could be an alias */
		if (*p == ':') {
			char *dot = p, *dotname = name;
			*name++ = *p++;
			while (isdigit(*p))
				*name++ = *p++;

			/* it wasn't, backup */
			if (*p != ':') {
				p = dot;
				name = dotname;
			}
			if (*p == '\0')
				return NULL;

			p++;
			break;
		}

		*name++ = *p++;
	}

	*name++ = '\0';
	return p;
}

int
procnetdev_version(char *buf)
{
	if (strstr(buf, "compressed"))
		return 3;

	if (strstr(buf, "bytes"))
		return 2;

	return 1;
}

int
get_dev_fields(char *bp, int versioninfo, if_stats_t *pstats)
{
	switch (versioninfo) {
	case 3:
		sscanf(bp, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
			&pstats->rx_bytes,
			&pstats->rx_packets,
			&pstats->rx_errors,
			&pstats->rx_dropped,
			&pstats->rx_fifo_errors,
			&pstats->rx_frame_errors,
			&pstats->rx_compressed,
			&pstats->rx_multicast,
			&pstats->tx_bytes,
			&pstats->tx_packets,
			&pstats->tx_errors,
			&pstats->tx_dropped,
			&pstats->tx_fifo_errors,
			&pstats->collisions,
			&pstats->tx_carrier_errors,
			&pstats->tx_compressed);
		break;

	case 2:
		sscanf(bp, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
			&pstats->rx_bytes,
			&pstats->rx_packets,
			&pstats->rx_errors,
			&pstats->rx_dropped,
			&pstats->rx_fifo_errors,
			&pstats->rx_frame_errors,
			&pstats->tx_bytes,
			&pstats->tx_packets,
			&pstats->tx_errors,
			&pstats->tx_dropped,
			&pstats->tx_fifo_errors,
			&pstats->collisions,
			&pstats->tx_carrier_errors);

		pstats->rx_multicast = 0;
		break;

	case 1:
		sscanf(bp, "%lu %lu %lu %lu %lu %lu %lu %lu %lu %lu %lu",
			&pstats->rx_packets,
			&pstats->rx_errors,
			&pstats->rx_dropped,
			&pstats->rx_fifo_errors,
			&pstats->rx_frame_errors,
			&pstats->tx_packets,
			&pstats->tx_errors,
			&pstats->tx_dropped,
			&pstats->tx_fifo_errors,
			&pstats->collisions,
			&pstats->tx_carrier_errors);

		pstats->rx_bytes = 0;
		pstats->tx_bytes = 0;
		pstats->rx_multicast = 0;
		break;
	}

	return 0;
}

int
upnp_osl_wan_ifstats(char *wan_ifname, if_stats_t *pstats)
{
	extern int get_dev_fields(char *, int, if_stats_t *);
	extern int procnetdev_version(char *);
	extern char *get_name(char *, char *);

	FILE *fh;
	char buf[512];
	int err;
	int procnetdev_vsn;  /* version information */

	memset(pstats, 0, sizeof(*pstats));

	fh = fopen(_PATH_PROCNET_DEV, "r");
	if (!fh) {
		fprintf(stderr, "Warning: cannot open %s (%s). Limited output.\n",
			_PATH_PROCNET_DEV, strerror(errno));
		return 0;
	}

	fgets(buf, sizeof(buf), fh);	/* eat line */
	fgets(buf, sizeof(buf), fh);

	procnetdev_vsn = procnetdev_version(buf);

	err = 0;
	while (fgets(buf, sizeof(buf), fh)) {
		char *s;
		char name[50];

		s = get_name(name, buf);
		if (strcmp(name, wan_ifname) == 0) {
			get_dev_fields(s, procnetdev_vsn, pstats);
			break;
		}
	}
	if (ferror(fh)) {
		perror(_PATH_PROCNET_DEV);
		err = -1;
	}
	fclose(fh);

	return err;
}

int
upnp_osl_wan_link_status(char *wan_devname)
{
	struct ifreq ifr;
	int fd, err;
	uint if_up = 0;
	struct ethtool_cmd ecmd;

	/* Setup our control structures. */
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, wan_devname);

	/* Open control socket. */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd >= 0) {
		ecmd.cmd = ETHTOOL_GSET;
		ifr.ifr_data = (caddr_t)&ecmd;

		err = ioctl(fd, SIOCETHTOOL, &ifr);
		if (err >= 0) {
			switch (ecmd.speed) {
			case SPEED_10:
			case SPEED_100:
			case SPEED_1000:
				if_up = 1;
				break;
			}
		}

		/* close the control socket */
		close(fd);
	}

	return if_up;
}

unsigned int
upnp_osl_wan_max_bitrates(char *wan_devname, unsigned long *rx, unsigned long *tx)
{
	struct ethtool_cmd ecmd;
	struct ifreq ifr;
	int fd, err;
	long speed = 0;

	/* This would have problem of pppoe */

	/* Setup our control structures. */
	memset(&ifr, 0, sizeof(ifr));
	strcpy(ifr.ifr_name, wan_devname);

	/* Open control socket. */
	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (fd >= 0) {
		ecmd.cmd = ETHTOOL_GSET;
		ifr.ifr_data = (caddr_t)&ecmd;

		err = ioctl(fd, SIOCETHTOOL, &ifr);
		if (err >= 0) {

			unsigned int mask = ecmd.supported;

			/* dump_ecmd(&ecmd); */
			if ((mask & (ADVERTISED_1000baseT_Half|ADVERTISED_1000baseT_Full))) {
				speed = (1000 * 1000000);
			}
			else if ((mask & (ADVERTISED_100baseT_Half|ADVERTISED_100baseT_Full))) {
				speed = (100 * 1000000);
			}
			else if ((mask & (ADVERTISED_10baseT_Half|ADVERTISED_10baseT_Full))) {
				speed = (10 * 1000000);
			}
			else {
				speed = 0;
			}
		}
		else {
			upnp_syslog(LOG_INFO, "ioctl(SIOCETHTOOL) failed");
		}

		/* close the control socket */
		close(fd);
	}
	else {
		upnp_syslog(LOG_INFO, "cannot open socket");
	}

	*rx = *tx = speed;
	return TRUE;
}

int
upnp_osl_wan_ip(char *wan_ifname, struct in_addr *inaddr)
{
	inaddr->s_addr = 0;

	if (upnp_osl_ifaddr(wan_ifname, inaddr) == 0)
		return 1;

	return 0;
}

int
upnp_osl_wan_isup(char *wan_ifname)
{
	struct in_addr inaddr = {0};

	if (upnp_osl_ifaddr(wan_ifname, &inaddr) == 0) {
		/* Check ip address */
		if (inaddr.s_addr != 0)
			return 1;
	}

	return 0;
}

void
upnp_osl_nat_config(char *wan_ifname, UPNP_PORTMAP *map)
{
	void upnpnat(int argc, char **argv);

	char cmd[256];
	char *argv[32] = {0};
	char *name, *p, *next;
	char *log_level;
	int i;

	sprintf(cmd, "upnpnat -i %s -eport %d -iport %d -en %d",
		wan_ifname,
		map->external_port,
		map->internal_port,
		map->enable);

	if (strlen(map->remote_host)) {
		strcat(cmd, " -remote ");
		strcat(cmd, map->remote_host);
	}

	if (strlen(map->protocol)) {
		strcat(cmd, " -proto ");
		strcat(cmd, map->protocol);
	}

	if (strlen(map->internal_client)) {
		strcat(cmd, " -client ");
		strcat(cmd, map->internal_client);
	}

	if ((log_level = upnp_get_config("log_level")) != NULL) {
		strcat(cmd, " -log_level ");
		strcat(cmd, log_level);
	}

	/* Seperate into argv[] */
	for (i = 0, name = cmd, p = name;
		name && name[0];
		name = next, p = 0, i++) {
		/* Get next token */
		strtok_r(p, " ", &next);
		argv[i] = name;
	}

	/* Run the upnpnat command */
	_eval(argv, ">/dev/console", 0, NULL);

	return;
}

#endif /* __BCMIGD__ */
#endif /* __CONFIG_NAT__ */
