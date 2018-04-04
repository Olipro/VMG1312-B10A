/*
 * Broadcom UPnP module message passing by using loopback socket
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: upnp_req.c 241192 2011-02-17 21:52:25Z gmo $
 */

#include <upnp.h>
#include <security_ipc.h>

#ifdef __CONFIG_NAT__
#ifdef __BCMIGD__
#include <InternetGatewayDevice.h>
#endif /* __BCMIGD__ */
#endif /* __CONFIG_NAT__ */

/* Close the UPnP request socket */
void
upnp_request_shutdown(UPNP_CONTEXT *context)
{
	UPNP_INTERFACE	*ifp = context->focus_ifp;

	if (ifp->req_sock != -1) {
		close(ifp->req_sock);
		ifp->req_sock = -1;
	}

	return;
}

/* Open the UPnP request socket */
int
upnp_request_init(UPNP_CONTEXT *context)
{
	UPNP_INTERFACE	*ifp = context->focus_ifp;
	struct in_addr addr;

	addr.s_addr = inet_addr(UPNP_IPC_ADDR);
	ifp->req_sock = oslib_udp_socket(addr, UPNP_IPC_PORT + ifp->if_instance);
	if (ifp->req_sock < 0)
		return -1;

	return 0;
}

/* Read request message */
static int
read_request(UPNP_CONTEXT *context)
{
	socklen_t size = sizeof(struct sockaddr);
	int len;

	len = recvfrom(context->focus_ifp->req_sock, context->buf, sizeof(context->buf),
		0, (struct sockaddr *)&context->dst_addr, &size);

	/* sizeof message */
	context->end = len;
	return len;
}

/* Send request response to peer */
static int
upnp_request_response(UPNP_CONTEXT *context, void *buf, int len)
{
	/* Send the response out */
	sendto(context->focus_ifp->req_sock, buf, len, 0,
		(struct sockaddr *)&context->dst_addr, sizeof(context->dst_addr));

	return 0;
}

static UPNP_DEVICE *
get_root_device(UPNP_CONTEXT *context, char *url)
{
	UPNP_DEVICE *device = 0;
	int i;

	for (i = 0; (device = upnp_device_table[i]) != 0; i++) {
		if (strcmp(device->root_device_xml, url) == 0)
			break;
	}

	return device;
}

/* IPC command handlers */
static void
upnp_ipc_attach(UPNP_CONTEXT *context)
{
	UPNP_DEVICE *device = 0;
	char *uri;

	/* Skip "attach " command */
	uri = context->buf + strlen("attach ");
	strtok(uri, "\n");

	/* Find the root device */
	device = get_root_device(context, uri);
	if (!device) {
		upnp_syslog("cannot find root device %s\n", uri);
		return;
	}

	/* Attach device to interface */
	upnp_syslog(LOG_INFO, "attach %s", uri);
	upnp_device_attach(context, device);
	return;
}

static void
upnp_ipc_detach(UPNP_CONTEXT *context)
{
	UPNP_DEVICE *device = 0;
	char *uri;

	/* Skip "detach " command */
	uri = context->buf + strlen("detach ");
	strtok(uri, "\n");

	/* Find the root device */
	device = get_root_device(context, uri);
	if (!device) {
		upnp_syslog("cannot find root device %s\n", uri);
		return;
	}

	/* Attach device to interface */
	upnp_syslog(LOG_INFO, "detach %s", uri);
	upnp_device_detach(context, device);
	return;
}

static void
upnp_ipc_notify(UPNP_CONTEXT *context)
{
	char *ipaddr;
	char *service_name;
	char *p, *next;
	int num = 0;
	char *headers[UPNP_TOTAL_EVENTED_VAR+1];

	/* Get ipaddr to notify */
	ipaddr = context->buf + strlen("notify ");
	strtok_r(ipaddr, " ", &next);

	/* Get service name */
	if ((service_name = next) == 0)
		return;

	strtok_r(service_name, "\n", &next);

	/* Fill headers */
	if (next) {
		memset(headers, 0, sizeof(headers));

		for (p = next; p && p[0]; p = next) {
			/* Get next token */
			strtok_r(NULL, "\n", &next);
			headers[num++] = p;
		}
	}

	/* Do gena alarm */
	if (strcmp(ipaddr, "*") == 0)
		ipaddr = 0;

 	if (num)
		gena_event_alarm(context, service_name, num, headers, ipaddr);

	return;
}

/* Process the UPnP request message */
void
upnp_request_handler(UPNP_CONTEXT *context)
{
	int len;
	char *buf;

	/* Read message */
	len = read_request(context);
	if (len <= 0)
		return;

	/* Perform text style IPC commands */
	buf = context->buf;

	if (memcmp(buf, "attach ", 7) == 0) {
		upnp_ipc_attach(context);
		return;
	}
	else if (memcmp(buf, "detach ", 7) == 0) {
		upnp_ipc_detach(context);
		return;
	}
	else if (memcmp(buf, "notify ", 7) == 0) {
		upnp_ipc_notify(context);
		return;
	}

	return;
}
