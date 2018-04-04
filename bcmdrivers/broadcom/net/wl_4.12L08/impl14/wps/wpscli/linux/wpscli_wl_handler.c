/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wl_handler.c 241182 2011-02-17 21:50:03Z $
 *
 * Description: Implement functions handling OID and device
 *
 */
#include <string.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <netpacket/packet.h>
#include <net/if.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <asm/types.h>
#include <unistd.h>
#include <ctype.h>

#ifdef TARGETENV_android
#include <sys/types.h>
#include <sys/socket.h>
#else
#include <linux/types.h>
#include <linux/sockios.h>
#endif

#include "stdio.h"
#include "stdlib.h"

#include <wpscli_osl.h>
#include <tutrace.h>

#define DEV_TYPE_LEN	4
#define ETHTOOL_GDRVINFO	0x00000003 /* Get driver info. */
#define ETHTOOL_BUSINFO_LEN     32

struct ethtool_drvinfo {
	__u32	cmd;
	char	driver[32];	/* driver short name, "tulip", "eepro100" */
	char	version[32];	/* driver version string */
	char	fw_version[32];	/* firmware version string, if applicable */
	char	bus_info[ETHTOOL_BUSINFO_LEN];	/* Bus info for this IF. */
				/* For PCI devices, use pci_dev->slot_name. */
	char	reserved1[32];
	char	reserved2[16];
	__u32	n_stats;	/* number of u64's from ETHTOOL_GSTATS */
	__u32	testinfo_len;
	__u32	eedump_len;	/* Size of data from ETHTOOL_GEEPROM (bytes) */
	__u32	regdump_len;	/* Size of data from ETHTOOL_GREGS (bytes) */
};

char if_name[20] = { 0 };

/* Global containing the return value from the last WL ioctl call */
int brcm_wpscli_ioctl_err = 0;

static int get_interface_name(struct ifreq* ifr);
static int wps_wl_ioctl(int cmd, void *buf, int len, int set);

brcm_wpscli_status wpscli_wlh_open(const char *adapter_name, int is_virutal)
{
	struct ifreq ifr;

	TUTRACE((TUTRACE_INFO, "Entered : wpscli_wlh_open\n"));

	if (NULL == adapter_name) {
		if( 0 != get_interface_name(&ifr)) {
			TUTRACE((TUTRACE_ERR, "Exit: wpscli_wlh_open : Failed to discover Wi-Fi interface.\n"));
			return WPS_STATUS_OPEN_ADAPTER_FAIL;
		}
		strcpy(if_name, ifr.ifr_name);
	} 
	else {
		strcpy(if_name, adapter_name);;
	}

	TUTRACE((TUTRACE_INFO, "Exit : wpscli_wlh_open. \n"));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlh_close()
{
	return WPS_STATUS_SUCCESS;
}

char *wpscli_get_interface_name()
{
	return if_name;
}

brcm_wpscli_status wpscli_wlh_get_adapter_mac(uint8 *adapter_mac)
{
	struct ifreq ifr;
	int ret = 0;
	int s;
	char szDebug[256] = { 0 };

	TUTRACE((TUTRACE_INFO, "Entered : wpscli_wlh_get_adapter_mac. if_name=[%s]\n", if_name));

	if(adapter_mac == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_wlh_get_adapter_mac : Invalid NULL adapter_mac\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if (!if_name[0]) {
		printf("Wireless Interface not specified.\n");
		return WPS_STATUS_SYSTEM_ERR;
	}

	/* Open a raw socket */
	if ((s = socket (AF_INET, SOCK_DGRAM, 0)) < 0) {
		printf("socket open failed\n");
		return WPS_STATUS_SYSTEM_ERR;
	}

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, IFNAMSIZ);
	if ((ret = ioctl(s, SIOCGIFHWADDR, &ifr)) < 0) {
		printf("ioctl to get hwaddr failed.\n");
		close(s);
		return WPS_STATUS_SYSTEM_ERR;
	}

	/* Copy the result back */
	memcpy(adapter_mac, ifr.ifr_hwaddr.sa_data, 6);

	sprintf(szDebug, "%2.2X:%2.2X:%2.2X:%2.2X:%2.2X:%2.2X", adapter_mac[0],adapter_mac[1],
		adapter_mac[2],adapter_mac[3],adapter_mac[4],adapter_mac[5]);

	close(s);

	TUTRACE((TUTRACE_INFO, "Exit : wpscli_wlh_get_adapter_mac. adapter_mac [%s]\n", szDebug));
	return WPS_STATUS_SUCCESS;
}

int get_dev_type(char *name, void *buf, int len)
{
	int s;
	int ret = 0;
	struct ifreq ifr;
	struct ethtool_drvinfo info;

	TUTRACE((TUTRACE_INFO, "Entered : get_dev_type\n"));

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) >= 0)
	{
		/* get device type */
		memset(&info, 0, sizeof(info));
		info.cmd = ETHTOOL_GDRVINFO;
		ifr.ifr_data = (caddr_t)&info;
		strncpy(ifr.ifr_name, name, IFNAMSIZ);
		if ((ret = ioctl(s, SIOCETHTOOL, &ifr)) < 0) {
			*(char *)buf = '\0';
		}
		else
			strncpy(buf, info.driver, len);

		close(s);
	}
	return ret;
}

brcm_wpscli_status wpscli_wlh_ioctl_set(int cmd, const char *data, ulong data_len)
{
	brcm_wpscli_status status;
	int err;

	TUTRACE((TUTRACE_INFO, "wpscli_wlh_ioctl_set: Entered.\n"));

	err = wps_wl_ioctl(cmd, (void *)data, data_len, TRUE);  // Set
	brcm_wpscli_ioctl_err = err;
	status = (err==0 ? WPS_STATUS_SUCCESS : WPS_STATUS_IOCTL_SET_FAIL);

	TUTRACE((TUTRACE_INFO, "wpscli_wlh_ioctl_set: Exiting. status=%d\n", status));
	return status;
}

brcm_wpscli_status wpscli_wlh_ioctl_get(int cmd, char *buf, ulong buf_len)
{
	brcm_wpscli_status status;
	int err;

	
	TUTRACE((TUTRACE_INFO, "wpscli_wlh_ioctl_get: Entered.\n"));

	err = wps_wl_ioctl((uint)cmd, buf, buf_len, FALSE);  // Get

	status = (err==0 ? WPS_STATUS_SUCCESS : WPS_STATUS_IOCTL_GET_FAIL);

	TUTRACE((TUTRACE_INFO, "wpscli_wlh_ioctl_get: Exiting. status=%d\n", status));
	return status;
}

int get_interface_name(struct ifreq* ifr)
{
	char proc_net_dev[] = "/proc/net/dev";
	FILE *fp;
	char buf[1000], *c, *name;
	char dev_type[DEV_TYPE_LEN];
	int ret = -1;

	TUTRACE((TUTRACE_INFO, "Entered : get_interface_name\n"));

	ifr->ifr_name[0] = '\0';

	if (!(fp = fopen(proc_net_dev, "r")))
		return ret;

	/* eat first two lines */
	if (!fgets(buf, sizeof(buf), fp) ||
	    !fgets(buf, sizeof(buf), fp)) {
		fclose(fp);
		return ret;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		c = buf;
		while (isspace(*c))
			c++;
		if (!(name = strsep(&c, ":")))
			continue;
		strncpy(ifr->ifr_name, name, IFNAMSIZ);
		if (get_dev_type(name, dev_type, DEV_TYPE_LEN) >= 0 &&
			(!strncmp(dev_type, "wl", 2) || !strncmp(dev_type, "dhd", 3)))
		{
				ret = 0;
				break;
		}
		ifr->ifr_name[0] = '\0';
	}

	fclose(fp);

	TUTRACE((TUTRACE_INFO, "Exit : get_interface_name\n"));
	return ret;
}

int wps_wl_ioctl(int cmd, void *buf, int len, int set)
{
	struct ifreq ifr;
	wl_ioctl_t ioc;
	int ret = 0;
	int s;

	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));

//	TUTRACE((TUTRACE_INFO, "wps_wl_ioctl: ifr_name is [%s]\n", if_name));

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		ret = -1;
		return ret;
	}

	/* do it */
	ioc.cmd = cmd;
	ioc.buf = buf;
	ioc.len = len;
	ioc.set = set;
	ifr.ifr_data = (caddr_t) &ioc;
	if ((ret = ioctl(s, SIOCDEVPRIVATE, &ifr)) < 0) {
		if (cmd != WLC_GET_MAGIC) {
			ret = -2;
		}
	}

	/* cleanup */
	close(s);
	return ret;
}
