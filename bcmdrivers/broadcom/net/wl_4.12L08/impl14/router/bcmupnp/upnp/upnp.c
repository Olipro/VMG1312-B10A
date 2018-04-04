/*
 * Broadcom UPnP module main loop
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: upnp.c 241192 2011-02-17 21:52:25Z gmo $
 */

#include <upnp.h>

/*
 * Variables
 */
int upnp_flag;
UPNP_CONTEXT upnp_context;

static int upnp_config_num;
static char **upnp_config_list;

/* Get environment variable from argv */
char *
upnp_get_config(char *name)
{
	int i;
	char **pp = upnp_config_list;
	char *ptr;

	if (name == NULL)
		return 0;
	
	for (i = 0; i < upnp_config_num; i++, pp++) {
		ptr = strchr(*pp, '=');
		if (ptr && strncmp(name, *pp, strlen(name)) == 0)
			return ptr+1;
	}

	return 0;
}

char *
upnp_safe_get_config(char *name)
{
	char *value = upnp_get_config(name);

	if (value == 0)
		value = "";

	return value;
}

/* Shutdown all the UPnP interfaces */
void
upnp_shutdown(UPNP_CONTEXT *context)
{
	UPNP_INTERFACE	*ifp;

	while ((ifp = context->iflist) != 0) {

		context->focus_ifp = ifp;

		/* Unhook device database */
		while (ifp->device_chain) {

			ifp->focus_devchain = ifp->device_chain;

			upnp_device_detach(context, ifp->device_chain->device);
		}

		/* shutdown interface protocol */
		upnp_http_shutdown(context);

		ssdp_del_multi(context);

		upnp_request_shutdown(context);

		/* Drop this link and free */
		context->iflist = ifp->next;

		free(ifp);
	}

	/* Shutdown the mutlicast receive socket */
	ssdp_shutdown(context);

	upnp_syslog(LOG_INFO, "UPnP daemon stopped");
}

/*
 * Get interface IP addresses and netmasks from interface names
 */
static int
get_if_ipaddr(UPNP_INTERFACE *ifp)
{
	char mac[6];

	if (oslib_ifaddr(ifp->ifname, &ifp->ipaddr) != 0)
		return -1;

	if (oslib_netmask(ifp->ifname, &ifp->netmask) != 0)
		return -1;

	if ((oslib_hwaddr(ifp->ifname, mac) != 0) ||
		memcmp(mac, "\0\0\0\0\0\0", 6) == 0)
		return -1;

	return 0;
}

/* Do UPnP interface initialization */
static UPNP_INTERFACE *
upnp_ifinit(UPNP_CONTEXT *context, int instance)
{
	UPNP_INTERFACE	*ifp;
	char name[32];
	char *ifname;

	/* Get ifname */
	sprintf(name, "lan%d_ifname", instance);
	ifname = upnp_get_config(name);
	if (ifname == NULL)
		return 0;

	/* Allocate interface space */
	ifp = (UPNP_INTERFACE *)malloc(sizeof(*ifp));
	if (ifp == 0)
		return 0;

	memset(ifp, 0, sizeof(*ifp));

	/* Setup context */
	strcpy(ifp->ifname, ifname);
	ifp->if_instance = instance;

	ifp->http_sock = -1;
	ifp->req_sock = -1;

	if (get_if_ipaddr(ifp) != 0) {
		free(ifp);
		return 0;
	}

	/* Do prepend */
	ifp->next = context->iflist;
	context->iflist = ifp;

	return ifp;
}

static int
upnp_read_config(UPNP_CONFIG *config)
{
	char *value;

	memset(config, 0, sizeof(*config));

	/* Get OS/VER */
	if ((value = upnp_get_config("os_version")))
		strcpy(config->os_name, value );
	else
		strcpy(config->os_name, "UPnP Stack");

	if ((value = upnp_get_config("os_version")))
		strcpy(config->os_ver, value);
	else
		strcpy(config->os_ver, "estimation 1.00");

	/* initialize upnp_config to default values */
	if ((value = upnp_get_config("upnp_port")))
		config->http_port = atoi(value);
	else
		config->http_port = 1780;

	if ((value = upnp_get_config("upnp_ad_time")))
		config->adv_time = atoi(value);
	else
		config->adv_time = 300;

	if ((value = upnp_get_config("upnp_sub_timeout")))
		config->sub_time = atoi(value);
	else
		config->sub_time = 300;

	return 0;
}

/* UPnP module initialization */
int
upnp_init(UPNP_CONTEXT *context)
{
	UPNP_INTERFACE *ifp = 0;
	UPNP_DEVICE *device;
	int i;
	int instance;

	/* Clean up */
	memset(context, 0, sizeof(*context));

	upnp_read_config(&context->config);

	/* Do context common initialization */
	context->adv_seconds = time(0);
	context->gena_last_check = time(0);
	context->upnp_last_time = time(0);
	context->ssdp_sock = -1;

	for (i = 0; (device = upnp_device_table[i]) != 0; i++) {
		(*device->common_init)(context);

		/*
		 * Give a unique uuid to this router,
		 * create same uuid when restart
		 */
		upnp_device_renew_uuid(context, device);
	}

	/* Create the mutlicast receive socket for all interfaces */
	if (ssdp_init(context) != 0)
		goto error_out;

	/* Do per interface initial */
	for (instance = 0; instance < 255; instance++) {
		/* Init interface */		
		ifp = upnp_ifinit(context, instance);
		if (ifp == 0)
			continue;

		context->focus_ifp = ifp;

		/* Perform per interface protocol initialization */
		if (upnp_http_init(context) != 0) {
			upnp_syslog(LOG_ERR, "upnp_http_init::%s init error!", ifp->ifname);
			goto error_out;
		}

		if (ssdp_add_multi(context) == -1) {
			upnp_syslog(LOG_ERR, "ssdp_init::%s error!", ifp->ifname);
			goto error_out;
		}

		if (upnp_request_init(context) != 0) {
			upnp_syslog(LOG_ERR, "upnp_request_init::init error!");
			goto error_out;
		}

		/*
		 * Hook device table to each interface.
		 * The init function of each device
		 * intialize the event variables, and send SSDP ALIVE to each
		 * interface
		 */
		for (i = 0; (device = upnp_device_table[i]) != 0; i++) {
			if (device->attach_mode == DEVICE_ATTACH_ALWAYS)
				upnp_device_attach(context, device);
		}
	}

	/* No interface found, return directly */
	if (context->iflist == 0) {
		upnp_syslog(LOG_ERR, "No UPnP interface specified, bye!");
		goto error_out;
	}

	upnp_printf("UPnP daemon is ready to run\n");
	return 0;

error_out:
	upnp_printf("UPnP init failed\n");
	upnp_flag = UPNP_FLAG_SHUTDOWN;
	return -1;
}

/* Time out handler of SSDP, GEAN and all the devices */
void
upnp_timeout(UPNP_CONTEXT *context)
{
	UPNP_INTERFACE	*ifp;
	UPNP_DEVCHAIN	*chain;

	time_t now = time(0);
	int update_ssdp;
	int update_gena;
	int delta;

	/* Special patch for NTP */
	if ((now - context->upnp_last_time) > 631123200) {
		/* Update for ssdp */
		delta = context->upnp_last_time - context->adv_seconds;
		context->adv_seconds = (now - delta) - 1;
	}

	/* Okay, it's safe to check */
	update_ssdp = ((u_long)(now - context->adv_seconds) >= context->config.adv_time - 1);
	update_gena = ((u_long)(now - context->gena_last_check) >= GENA_TIMEOUT);

	/* Add device timer here */
	for (ifp = context->iflist;
		 ifp;
		 ifp = ifp->next) {

		/* Set the focus inteface for further reference */
		context->focus_ifp = ifp;

		/* loop for each device to check timeout */
		for (chain = ifp->device_chain;
			 chain;
			 chain = chain->next) {

			ifp->focus_devchain = chain;

			/* check for advertisement interval */
			if (update_ssdp)
				ssdp_timeout(context);

			/* check for subscription expirations every 30 seconds */
			if (update_gena)
				gena_timeout(context);

			/* Check device timeout */
			if (chain->device->timeout)
				(*chain->device->timeout)(context, now);
		}
	}

	/* Update ssdp timer, gena timer, and current system time */
	if (update_ssdp)
		context->adv_seconds = now;

	if (update_gena)
		context->gena_last_check = now;

	context->upnp_last_time = now;
	return;
}

/*
 * Accept a connection and to handle the new http connection.
 */
static void
upnp_http_accept(UPNP_CONTEXT *context)
{
	struct sockaddr_in addr;
	socklen_t addr_len;
	int ns;

	/* accept new upnp_http socket */
	addr_len = sizeof(struct sockaddr_in);
	ns = accept(context->focus_ifp->http_sock, (struct sockaddr *)&addr, &addr_len);
	if (ns == -1)
		return;

	context->dst_addr = addr;
	context->dst = &context->dst_addr;
	upnp_http_process(context, ns);
	return;
}

/* Dispatch UPnP incoming messages. */
int
upnp_dispatch(UPNP_CONTEXT *context)
{
	UPNP_INTERFACE	*ifp;
	struct timeval tv = {1, 0};    /* timed out every second */
	int n;
	fd_set  fds;

	FD_ZERO(&fds);

	/* Set select sockets */
	FD_SET(context->ssdp_sock, &fds);

	for (ifp = context->iflist; ifp; ifp = ifp->next) {
		FD_SET(ifp->http_sock, &fds);
		FD_SET(ifp->req_sock, &fds);
	}

	/* select sockets */
	n = select(FD_SETSIZE, &fds, (fd_set *)NULL, (fd_set *)NULL, &tv);
	if (n > 0) {
		/* process ssdp multicast packet */
		if (FD_ISSET(context->ssdp_sock, &fds)) {
			ssdp_process(context, context->ssdp_sock);
		}

		/* process upnp_http */
		for (ifp = context->iflist; ifp; ifp = ifp->next) {
			context->focus_ifp = ifp;

			/* process http */
			if (FD_ISSET(ifp->http_sock, &fds)) {
				upnp_http_accept(context);
			}

			/* process message */
			if (FD_ISSET(ifp->req_sock, &fds)) {
				upnp_request_handler(context);
			}
		}
	}

	upnp_timeout(context);

	return 0;
}

/* 
 * UPnP portable main loop.
 * It initializes the UPnP protocol and event variables.
 * And loop handler the UPnP incoming requests.
 */
int
upnp_mainloop(int argc, char **argv)
{
	UPNP_CONTEXT *context = &upnp_context;

	/* initialize upnp */
	upnp_flag = 0;
	upnp_config_num = argc;
	upnp_config_list = argv;

	/* init context */
	upnp_init(context);

	/* main loop */
	while (1) {
		switch (upnp_flag) {
		case UPNP_FLAG_SHUTDOWN:
			upnp_shutdown(context);
			upnp_syslog(LOG_INFO, "UPnP shutdown!");
			return UPNP_FLAG_SHUTDOWN;

		case UPNP_FLAG_RESTART:
			upnp_shutdown(context);
			upnp_syslog(LOG_INFO, "UPnP restart!");
			return UPNP_FLAG_RESTART;

		case 0:
		default:
			upnp_dispatch(context);
			break;
		}
	}

	return upnp_flag;
}


void
upnp_stop_handler()
{
	upnp_flag = UPNP_FLAG_SHUTDOWN;
}


void
upnp_restart_handler()
{
	upnp_flag = UPNP_FLAG_RESTART;
}
