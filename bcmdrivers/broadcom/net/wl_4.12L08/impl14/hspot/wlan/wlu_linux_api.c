/*
 * Linux specific functions.
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
 */

#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <ctype.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/sockios.h>
#ifdef HOTSPOT_AP
typedef u_int64_t u64;
typedef u_int32_t u32;
typedef u_int16_t u16;
typedef u_int8_t u8;
#endif
#include <linux/ethtool.h>
#include <wlioctl.h>
#include <bcmutils.h>
#include "wlu.h"
#include "wlu_remote.h"
#include "wlu_api.h"

#define DEV_TYPE_LEN					3 /* length for devtype 'wl'/'et' */
#define NO_ERROR						0
#ifdef DSLCPE
#include <bcmnvram.h>
#include <shutils.h>
#define MAX_NVPARSE 16 
#endif
#define IOCTL_ERROR  -2		/* Error code for Ioctl failure */

int remote_type = NO_REMOTE;

static void *gWl = 0;

static struct ifreq *wlifreq[MAX_WLIF_NUM];
static int wlif_num = 0;

static void
syserr(char *s)
{
	perror(s);
	exit(errno);
}

int
wl_ioctl(void *wl, int cmd, void *buf, int len, bool set)
{
	struct ifreq *ifr = (struct ifreq *) wl;
	wl_ioctl_t ioc;
	int ret = 0;
	int s;

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		syserr("socket");

	/* do it */
	ioc.cmd = cmd;
	ioc.buf = buf;
	ioc.len = len;
	ioc.set = set;
	ifr->ifr_data = (caddr_t) &ioc;
	if ((ret = ioctl(s, SIOCDEVPRIVATE, ifr)) < 0) {
		if (cmd != WLC_GET_MAGIC) {
			ret = IOCTL_ERROR;
		}
	}

	/* cleanup */
	close(s);
	return ret;
}

static int
wl_get_dev_type(char *name, void *buf, int len)
{
	int s;
	int ret;
	struct ifreq ifr;
	struct ethtool_drvinfo info;

	/* open socket to kernel */
	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
		syserr("socket");

	/* get device type */
	memset(&info, 0, sizeof(info));
	info.cmd = ETHTOOL_GDRVINFO;
	ifr.ifr_data = (caddr_t)&info;
	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	if ((ret = ioctl(s, SIOCETHTOOL, &ifr)) < 0) {

		/* print a good diagnostic if not superuser */
		if (errno == EPERM)
			syserr("wl_get_dev_type");

		*(char *)buf = '\0';
	} else {
		strncpy(buf, info.driver, len);
	}

	close(s);
	return ret;
}

#ifdef DSLCPE
static bool check_Hspot_Req(void *intf ) {

	char *ptr;
	int pri, sec;
	bool find;
	char prefix[16];
	char *osifname;
	 char varname[NVRAM_MAX_PARAM_LEN];
	struct ifreq *ifr=(struct ifreq *)intf;
	osifname = wl_ifname(ifr);
	find = FALSE;
	/* look for interface name on the primary interfaces first */
	for (pri = 0; pri < MAX_NVPARSE; pri++) {
		snprintf(varname, sizeof(varname),
				"wl%d_ifname", pri);
		if (nvram_match(varname, osifname)) {
			find = TRUE;
			snprintf(prefix, sizeof(prefix), "wl%d_", pri);
			break;
		}
	}

	if (!find) {
		/* look for interface name on the multi-instance interfaces */
		for (pri = 0; pri < MAX_NVPARSE; pri++) {
			for (sec = 0; sec < MAX_NVPARSE; sec++) {
				snprintf(varname, sizeof(varname),
						"wl%d.%d_ifname", pri, sec);
				if (nvram_match(varname, osifname)) {
					find = TRUE;
					snprintf(prefix, sizeof(prefix),
							"wl%d.%d_", pri, sec);
					break;
				}
			}
		}
	}

	if (find) {
		ptr = nvram_get(strcat_r(prefix, "hspot", varname));
		if(ptr) {
			sec=atoi(ptr);	
			if(sec==1) {
				/* now we will check if wpa2 is enabled */
				ptr = nvram_get(strcat_r(prefix, "akm", varname));
				if(ptr && (!strcmp(ptr,"wpa2"))) 
					return TRUE;
			}
		}
	}
	return FALSE;
}

#endif

static int
wl_find(void)
{
	char proc_net_dev[] = "/proc/net/dev";
	FILE *fp;
	char buf[1000], *c, *name;
	char dev_type[DEV_TYPE_LEN];
	int status;

	if (!(fp = fopen(proc_net_dev, "r")))
		return BCME_ERROR;

	/* eat first two lines */
	if (!fgets(buf, sizeof(buf), fp) ||
	    !fgets(buf, sizeof(buf), fp)) {
		fclose(fp);
		return BCME_ERROR;
	}

	while (fgets(buf, sizeof(buf), fp)) {
		c = buf;
		while (isspace(*c))
			c++;
		if (!(name = strsep(&c, ":")))
			continue;
		if (wl_get_dev_type(name, dev_type, DEV_TYPE_LEN) >= 0 &&
			!strncmp(dev_type, "wl", 2)) {
			struct ifreq *ifr;
			ifr = malloc(sizeof(struct ifreq));
			if (!ifr) {
				break;
			}
			memset(ifr, 0, sizeof(struct ifreq));
			strncpy(ifr->ifr_name, name, IFNAMSIZ);
#ifdef DSLCPE
			if ((wl_check((void *) ifr) == 0)&&(check_Hspot_Req(ifr)!=FALSE)) {
#else
			if (wl_check((void *) ifr) == 0) {
#endif
				wlifreq[wlif_num] = ifr;
				wlif_num++;
				if (wlif_num >= MAX_WLIF_NUM)
					break;
			} else {
				free(ifr);
			}
		}
	}
	if (wlif_num == 0)
		status = BCME_ERROR;
	else
		status = BCME_OK;

	fclose(fp);
	return status;
}


static int
ioctl_queryinformation_fe(void *wl, int cmd, void* input_buf, int *input_len)
{
	int error = NO_ERROR;

	if (remote_type == NO_REMOTE) {
		error = wl_ioctl(wl, cmd, input_buf, *input_len, FALSE);
	} else {
	}
	return error;
}

static int
ioctl_setinformation_fe(void *wl, int cmd, void* buf, int *len)
{
	int error = 0;

	if (remote_type == NO_REMOTE) {
		error = wl_ioctl(wl,  cmd, buf, *len, TRUE);
	} else {
	}

	return error;
}

int
wl_get(void *wl, int cmd, void *buf, int len)
{
	int error = 0;
	/* For RWL: When interfacing to a Windows client, need t add in OID_BASE */
	error = (int)ioctl_queryinformation_fe(wl, cmd, buf, &len);

	if (error == SERIAL_PORT_ERR)
		return SERIAL_PORT_ERR;
	else if (error == BCME_NODEVICE)
		return BCME_NODEVICE;
	else if (error != 0)
		return IOCTL_ERROR;

	return 0;
}

int
wl_set(void *wl, int cmd, void *buf, int len)
{
	int error = 0;

	/* For RWL: When interfacing to a Windows client, need t add in OID_BASE */
	error = (int)ioctl_setinformation_fe(wl, cmd, buf, &len);

	if (error == SERIAL_PORT_ERR)
		return SERIAL_PORT_ERR;
	else if (error == BCME_NODEVICE)
		return BCME_NODEVICE;
	else if (error != 0)
		return IOCTL_ERROR;

	return 0;
}

void *wl(void)
{
	if (gWl == 0)
		wl_open(&gWl);

	return gWl;
}

void *wlif(int index)
{
	if (index >= wlif_num)
		return NULL;

	return wlifreq[index];
}

void *wl_getifbyname(char *ifname)
{
	int i;
	for (i = 0; i < wlif_num; i++) {
		if (strcmp(wlifreq[i]->ifr_name, ifname) == 0)
			return wlifreq[i];
	}
	return NULL;
}

void wlFree(void)
{
	if (gWl != 0) {
		wl_close();
		gWl = 0;
	}
}

int wl_open(void **wl)
{
	int error = -1;
	wl_find();
	if (wlif_num) {
		*wl = wlifreq[0];
		error = 0;
	}
	return error;
}

void wl_close(void)
{
	int i;
	for (i = 0; i < wlif_num; i++) {
		free(wlifreq[i]);
	}
	wlif_num = 0;
	gWl = 0;
}

char *wl_ifname(void *wl)
{
	struct ifreq *ifr = wl;
	return ifr->ifr_name;
}
