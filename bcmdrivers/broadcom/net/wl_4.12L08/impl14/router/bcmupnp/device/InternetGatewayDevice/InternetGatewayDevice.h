/*
 * Broadcom UPnP module, InternetGatewayDevice.h
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: InternetGatewayDevice.h 241192 2011-02-17 21:52:25Z gmo $
 */
#ifndef __INTERNETGATEWAYDEVICE_H__
#define __INTERNETGATEWAYDEVICE_H__

#ifdef __cplusplus
extern "C" {
#endif	/* __cplusplus */

/* Control structure */
#define UPNP_PM_SIZE        32
#define UPNP_PM_DESC_SIZE   256

typedef struct upnp_portmap {
	char remote_host[sizeof("255.255.255.255")];
	unsigned short external_port;
	char protocol[8];
	unsigned short internal_port;
	char internal_client[sizeof("255.255.255.255")];
	unsigned int enable;
	char description[UPNP_PM_DESC_SIZE];
	unsigned long duration;
	unsigned long book_time;
} UPNP_PORTMAP;


#define UPNP_IGD_WAN_LINK_LOCKS		5

typedef	struct upnp_igd_ctrl {
	unsigned int igd_seconds;
	char wan_ifname[IFNAMSIZ];
	char wan_devname[IFNAMSIZ];
	struct in_addr wan_ip;
	int wan_up;
	unsigned int wan_up_time;
	int wan_link;
	int wan_link_locks;
	int pm_num;
	int pm_limit;
	UPNP_PORTMAP pmlist[1];
} UPNP_IGD_CTRL;
#define UPNP_IGD_CTRL_SIZE (sizeof(UPNP_IGD_CTRL) - sizeof(UPNP_PORTMAP)) 

/* Functions */
int upnp_portmap_add
(
	UPNP_CONTEXT *context,
	char *remote_host,
	unsigned short external_port,
	char *protocol,
	unsigned short internal_port,
	char *internal_client,
	unsigned int enable,
	char *description,
	unsigned long duration
);

int upnp_portmap_del
(
	UPNP_CONTEXT *context,
	char *remote_host,
	unsigned short external_port,
	char *protocol
);

UPNP_PORTMAP *upnp_portmap_find
(
	UPNP_CONTEXT *context,
	char *remote_host,
	unsigned short external_port,
	char *protocol
);

unsigned short upnp_portmap_num(UPNP_CONTEXT *context);
UPNP_PORTMAP *upnp_portmap_with_index(UPNP_CONTEXT *context, int index);

/* OSL dependent function */
struct _if_stats;

extern int upnp_osl_wan_ifstats(char *wan_ifname, struct _if_stats *);
extern int upnp_osl_wan_link_status(char *wan_devname);
extern unsigned int upnp_osl_wan_max_bitrates(char *wan_devname, unsigned long *rx, unsigned long *tx);
extern int upnp_osl_wan_ip(char *wan_ifname, struct in_addr *inaddr);
extern int upnp_osl_wan_isup(char *wan_ifname);
extern void upnp_osl_nat_config(char *wan_ifname, UPNP_PORTMAP *map);

/* << TABLE BEGIN */
/*
 * WARNNING: DON'T MODIFY THE FOLLOWING TABLES
 * AND DON'T REMOVE TAG :
 *          "<< TABLE BEGIN"
 *          ">> TABLE END"
 */

extern UPNP_DEVICE InternetGatewayDevice;

int InternetGatewayDevice_common_init(UPNP_CONTEXT *context);
int InternetGatewayDevice_open(UPNP_CONTEXT *context);
int InternetGatewayDevice_close(UPNP_CONTEXT *context);
int InternetGatewayDevice_request(UPNP_CONTEXT *context, void *cmd);
int InternetGatewayDevice_timeout(UPNP_CONTEXT *context, time_t now);
int InternetGatewayDevice_notify(UPNP_CONTEXT *context, UPNP_SERVICE *service);
/* >> TABLE END */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __INTERNETGATEWAYDEVICE_H__ */
