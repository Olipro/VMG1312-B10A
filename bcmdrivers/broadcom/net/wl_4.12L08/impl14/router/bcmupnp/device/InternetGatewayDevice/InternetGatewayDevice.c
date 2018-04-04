/*
 * Broadcom UPnP module, InternetGatewayDevice.c
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: InternetGatewayDevice.c 241192 2011-02-17 21:52:25Z gmo $
 */
#include <upnp.h>
#include <InternetGatewayDevice.h>

void
upnp_portmap_reload(UPNP_CONTEXT *context)
{
	UPNP_IGD_CTRL *igd_ctrl;
	UPNP_PORTMAP *map;

	igd_ctrl = (UPNP_IGD_CTRL *)(context->focus_ifp->focus_devchain->devctrl);
	for (map = igd_ctrl->pmlist;
		map < igd_ctrl->pmlist + igd_ctrl->pm_num;
		map++)
	{
		/* Set to NAT kernel */
		if (map->enable)
			upnp_osl_nat_config(igd_ctrl->wan_ifname, map);
	}

	return;
}

/* Find a port mapping entry with index */
UPNP_PORTMAP *
upnp_portmap_with_index(UPNP_CONTEXT *context, int index)
{
	UPNP_IGD_CTRL *igd_ctrl;

	igd_ctrl = (UPNP_IGD_CTRL *)(context->focus_ifp->focus_devchain->devctrl);
	if (index >= igd_ctrl->pm_num)
		return 0;

	return (igd_ctrl->pmlist+index);
}

/* Get number of port mapping entries */
unsigned short
upnp_portmap_num(UPNP_CONTEXT *context)
{
	UPNP_IGD_CTRL *igd_ctrl;

	igd_ctrl = (UPNP_IGD_CTRL *)(context->focus_ifp->focus_devchain->devctrl);
	return igd_ctrl->pm_num;
}

/* Find a port mapping entry */
UPNP_PORTMAP *
upnp_portmap_find(UPNP_CONTEXT *context, char *remote_host,
	unsigned short external_port, char *protocol)
{
	UPNP_IGD_CTRL *igd_ctrl;
	UPNP_PORTMAP *map;

	igd_ctrl = (UPNP_IGD_CTRL *)(context->focus_ifp->focus_devchain->devctrl);
	for (map = igd_ctrl->pmlist;
		map < igd_ctrl->pmlist + igd_ctrl->pm_num;
		map++) {

		/* Find the entry fits for the required paramters */
		if (strcmp(map->remote_host, remote_host) == 0 &&
			map->external_port == external_port &&
			strcmp(map->protocol, protocol) == 0) {

			return map;
		}
	}

	return 0;
}

/* Description: Delete a port mapping entry */
static void
upnp_portmap_purge(UPNP_CONTEXT *context, UPNP_PORTMAP *map)
{
	UPNP_IGD_CTRL *igd_ctrl;
	int index;
	int remainder;

	igd_ctrl = (UPNP_IGD_CTRL *)(context->focus_ifp->focus_devchain->devctrl);

	if (map->enable) {
		map->enable = 0;
		upnp_osl_nat_config(igd_ctrl->wan_ifname, map);
	}

	/* Pull up remainder */
	index = map - igd_ctrl->pmlist;
	remainder = igd_ctrl->pm_num - (index+1);
	if (remainder)
		memcpy(map, map+1, sizeof(*map)*remainder);

	igd_ctrl->pm_num--;
	return;
}

int
upnp_portmap_del(UPNP_CONTEXT *context, char *remote_host,
	unsigned short external_port, char *protocol)
{
	UPNP_PORTMAP *map;

	map = upnp_portmap_find(context, remote_host, external_port, protocol);
	if (map == 0)
		return -1;

	/* Purge this entry */
	upnp_portmap_purge(context, map);
	return 0;
}

/* Add a new port mapping entry */
int
upnp_portmap_add
(
	UPNP_CONTEXT	*context,
	char            *remote_host,
	unsigned short  external_port,
	char            *protocol,
	unsigned short  internal_port,
	char            *internal_client,
	unsigned int    enable,
	char            *description,
	unsigned long   duration
)
{
	UPNP_IGD_CTRL *igd_ctrl;
	UPNP_PORTMAP *map;

	/* Get control body */
	igd_ctrl = (UPNP_IGD_CTRL *)(context->focus_ifp->focus_devchain->devctrl);

	/* data validation */
	if (strcasecmp(protocol, "TCP") != 0 &&
		strcasecmp(protocol, "UDP") != 0) {
		upnp_syslog(LOG_ERR, "add_portmap:: Invalid protocol");
		return -1;
	}

	/* check duplication */
	map = upnp_portmap_find(context, remote_host, external_port, protocol);
	if (map) {
		if (strcmp(internal_client, map->internal_client) != 0)
			return -1;

		/* Argus, make it looked like shutdown */
		if (enable != map->enable ||
			internal_port != map->internal_port) {

			if (map->enable) {
				map->enable = 0;
				upnp_osl_nat_config(igd_ctrl->wan_ifname, map);
			}
		}
	}
	else {
		if (igd_ctrl->pm_num == igd_ctrl->pm_limit) {

			UPNP_IGD_CTRL *new_igd_ctrl;

			int old_limit = igd_ctrl->pm_limit;
			int old_size = UPNP_IGD_CTRL_SIZE + old_limit * sizeof(UPNP_PORTMAP);

			int new_limit = old_limit * 2;
			int new_size = UPNP_IGD_CTRL_SIZE + new_limit * sizeof(UPNP_PORTMAP);

			/*
			 * malloc a new one for twice the size,
			 * the reason we don't use realloc is when realloc failed,
			 * the old memory will be gone!
			 */
			new_igd_ctrl = (UPNP_IGD_CTRL *)malloc(new_size);
			if (new_igd_ctrl == 0)
				return -1;

			/* Copy the old to the new one, and free it */
			memcpy(new_igd_ctrl, igd_ctrl, old_size);

			free(igd_ctrl);

			/* Assign the new one as the igd_ctrl */
			igd_ctrl = new_igd_ctrl;
			context->focus_ifp->focus_devchain->devctrl = new_igd_ctrl;

			igd_ctrl->pm_limit = new_limit;
		}

		/* Locate the map and advance the total number */
		map = igd_ctrl->pmlist + igd_ctrl->pm_num;
		igd_ctrl->pm_num++;
	}

	/* Update database */
	map->external_port = external_port;
	map->internal_port = internal_port;
	map->enable = enable;
	map->duration = duration;
	map->book_time = time(0);

	strcpy(map->remote_host, remote_host);
	strcpy(map->protocol, protocol);
	strcpy(map->internal_client, internal_client);
	strcpy(map->description, description);

	/* Set to NAT kernel */
	if (map->enable)
		upnp_osl_nat_config(igd_ctrl->wan_ifname, map);

	return 0;
}

/* Timed-out a port mapping entry */
static void
upnp_igd_timeout(UPNP_CONTEXT *context, time_t now)
{
	UPNP_IGD_CTRL *igd_ctrl;
	UPNP_PORTMAP *map;
	unsigned int past;
	struct in_addr wan_ip;
	int wan_up;
	int wan_link;
	int notify_flag = 0;

	igd_ctrl = (UPNP_IGD_CTRL *)(context->focus_ifp->focus_devchain->devctrl);

	/* Make sure reach check point */
	past = now - igd_ctrl->igd_seconds;
	if (past == 0)
		return;

	/* Check wan link */
	wan_link = upnp_osl_wan_link_status(igd_ctrl->wan_devname);
	if (igd_ctrl->wan_link != wan_link) {
		if (++igd_ctrl->wan_link_locks >= UPNP_IGD_WAN_LINK_LOCKS) {
			igd_ctrl->wan_link = wan_link;
			igd_ctrl->wan_link_locks = -1;
		}
	}
	else {
		igd_ctrl->wan_link_locks = -1;
	}

	/* Check wan up */
	if (igd_ctrl->wan_link == 0)
		wan_up = 0;
	else
		wan_up = upnp_osl_wan_isup(igd_ctrl->wan_ifname);

	if (igd_ctrl->wan_up != wan_up) {
		igd_ctrl->wan_up = wan_up;
		igd_ctrl->wan_up_time = 0;
		notify_flag = 1;
	}
	else if (wan_up) {
		/* Increment uptime every one second */
		igd_ctrl->wan_up_time++;
	}

	/* Check wan ip */
	upnp_osl_wan_ip(igd_ctrl->wan_ifname, &wan_ip);
	if (wan_ip.s_addr != igd_ctrl->wan_ip.s_addr) {
		igd_ctrl->wan_ip = wan_ip;
		notify_flag = 1;
	}

	/* Send notification */
	if (notify_flag == 1)
		gena_event_alarm(context, "urn:schemas-upnp-org:service:WANIPConnection", 0, 0, 0);

	/* Check portmap timeout */
	map = igd_ctrl->pmlist;
	while (map < igd_ctrl->pmlist + igd_ctrl->pm_num) {
		/* Purge the expired one */
		if (map->duration != 0 && now >= map->book_time) {
			upnp_portmap_purge(context, map);

			/*
			 * Keep the map pointer because after purging,
			 * the remainders will be pulled up
			 */
		}
		else {
			map++;
		}
	}

	igd_ctrl->igd_seconds = now;
	return;
}

/* Free port mapping list */
static void
upnp_igd_free(UPNP_CONTEXT *context)
{
	UPNP_IGD_CTRL *igd_ctrl;
	UPNP_PORTMAP *map;
	int i;

	igd_ctrl = (UPNP_IGD_CTRL *)(context->focus_ifp->focus_devchain->devctrl);
	if (igd_ctrl == 0)
		return;

	for (i = 0, map = igd_ctrl->pmlist; i < igd_ctrl->pm_num; i++, map++) {
		/* Delete this one from NAT kernel */
		if (map->enable) {
			map->enable = 0;
			upnp_osl_nat_config(igd_ctrl->wan_ifname, map);
		}
	}

	/* Free the control table */
	context->focus_ifp->focus_devchain->devctrl = 0;
	free(igd_ctrl);
	return;
}

/* Initialize port mapping list */
static int
upnp_igd_init(UPNP_CONTEXT *context)
{
	UPNP_IGD_CTRL *igd_ctrl;
	int size = UPNP_IGD_CTRL_SIZE + UPNP_PM_SIZE * sizeof(UPNP_PORTMAP);
	char name[32];

	/* allocate memory */
	igd_ctrl = (UPNP_IGD_CTRL *)malloc(size);
	if (igd_ctrl == 0) {
		upnp_syslog(LOG_ERR, "Cannot allocate igd ctrl block");
		return -1;
	}

	memset(igd_ctrl, 0, size);

	/* Do initialization */
	igd_ctrl->pm_limit = UPNP_PM_SIZE;

	/* Hook to devchain */
	context->focus_ifp->focus_devchain->devctrl = igd_ctrl;

	/* Setup wan information */
	sprintf(name, "wan%d_ifname", context->focus_ifp->if_instance);
	strcpy(igd_ctrl->wan_ifname, upnp_safe_get_config(name));

	sprintf(name, "wan%d_devname", context->focus_ifp->if_instance);
	strcpy(igd_ctrl->wan_devname, upnp_safe_get_config(name));

	return 0;
}

/*
 * WARNNING: PLEASE IMPLEMENT YOUR CODES AFTER
 *          "<< USER CODE START >>"
 * AND DON'T REMOVE TAG :
 *          "<< AUTO GENERATED FUNCTION: "
 *          ">> AUTO GENERATED FUNCTION"
 *          "<< USER CODE START >>"
 */

/* << AUTO GENERATED FUNCTION: InternetGatewayDevice_common_init() */
int
InternetGatewayDevice_common_init(UPNP_CONTEXT *context)
{
	/* << USER CODE START >> */
	char *value;
	int retries;

	value = upnp_safe_get_config("upnp_conn_retries");
	retries = atoi(value);
	if (retries < 1 || retries > 20)
		retries = 20;

	InternetGatewayDevice.gena_connect_retries = retries;
	return 0;
}
/* >> AUTO GENERATED FUNCTION */

/* << AUTO GENERATED FUNCTION: InternetGatewayDevice_open() */
int
InternetGatewayDevice_open(UPNP_CONTEXT *context)
{
	/* << USER CODE START >> */
	/* Check whether the IGD is okay */
	char *value;

	value = upnp_safe_get_config("igd_status");
	if (strcmp(value, "disabled") == 0)
		return -1;

	/* IGD control block initialization */
	if (upnp_igd_init(context) != 0)
		return -1;

	return 0;
}
/* >> AUTO GENERATED FUNCTION */


/* << AUTO GENERATED FUNCTION: InternetGatewayDevice_close() */
int
InternetGatewayDevice_close(UPNP_CONTEXT *context)
{
	/* << USER CODE START >> */
	/* cleanup NAT traversal structures */
	upnp_igd_free(context);
	return 0;
}
/* >> AUTO GENERATED FUNCTION */

/* << AUTO GENERATED FUNCTION: InternetGatewayDevice_request() */
int
InternetGatewayDevice_request(UPNP_CONTEXT *context, void *cmd)
{
	/* << USER CODE START >> */
	return 0;
}
/* >> AUTO GENERATED FUNCTION */

/* << AUTO GENERATED FUNCTION: InternetGatewayDevice_timeout() */
int
InternetGatewayDevice_timeout(UPNP_CONTEXT *context, time_t now)
{
	/* << USER CODE START >> */
	upnp_igd_timeout(context, now);
	return 0;
}
/* >> AUTO GENERATED FUNCTION */

/* << AUTO GENERATED FUNCTION: InternetGatewayDevice_notify() */
int
InternetGatewayDevice_notify(UPNP_CONTEXT *context, UPNP_SERVICE *service)
{
	/* << USER CODE START >> */
	return 0;
}
/* >> AUTO GENERATED FUNCTION */
