/*
 * Broadcom UPnP module main entry of linux platform
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: linux_main.c 241192 2011-02-17 21:52:25Z gmo $
 */
#include <errno.h>
#include <error.h>
#include <signal.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <netinet/in.h>
#include <wait.h>
#include <ctype.h>
#include <shutils.h>
#include <upnp.h>
#include <bcmnvram.h>

#ifdef DSLCPE
#define BCMUPMP_PID_FILE_PATH		"/var/bcmupnp.pid"
#else
#define BCMUPMP_PID_FILE_PATH	"/tmp/bcmupnp.pid"
#endif
static int upnp_argc;
static char *upnp_argv[64];
static char upnp_argbuf[512];
static char *upnp_ptr;

/*
 * Will be change to use message passing
 */
void
upnp_osl_update_wfa_subc_num(int if_instance, int num)
{
	char *upnp_subc_num, prefix[32];
	char num_string[8];

	if (if_instance == 0)
		snprintf(prefix, sizeof(prefix), "lan_upnp_wfa_subc_num");
	else
		snprintf(prefix, sizeof(prefix), "lan%d_upnp_wfa_subc_num", if_instance);

	upnp_subc_num = nvram_get(prefix);
	if (!upnp_subc_num || (num != atoi(upnp_subc_num))) {
		snprintf(num_string, sizeof(num_string), "%d", num);
		nvram_set(prefix, num_string);
	}

	return;
}

#ifdef __CONFIG_NAT__
#ifdef __BCMIGD__
/* Get primary wan ifname */
static void
upnp_wan_info(char *ifname, char *devname)
{
	int unit;
	char name[100];
	char prefix[32];
	char *value;
	char *proto;
	char *wan_devname = "";
	char *wan_ifname = "";

	/* Get primary wan config index */
	for (unit = 0; unit < 10; unit ++) {
		sprintf(name, "wan%d_primary", unit);
		value = nvram_safe_get(name);
		if (strcmp(value, "1") == 0)
			break;
	}
	if (unit == 10)
		unit = 0;

	sprintf(prefix, "wan%d_", unit);

	/* Get wan physical devname */
	wan_devname = nvram_safe_get(strcat_r(prefix, "ifname", name));

	/* Get wan interface name */
	proto = nvram_safe_get(strcat_r(prefix, "proto", name));
	if (strcmp(proto, "pppoe") == 0) {
		wan_ifname = nvram_safe_get(strcat_r(prefix, "pppoe_ifname", name));
	}
	else if (strcmp(proto, "disabled") != 0) {
		wan_ifname = nvram_safe_get(strcat_r(prefix, "ifname", name));
	}

	/* Return to caller */
	strcpy(ifname, wan_ifname);
	strcpy(devname, wan_devname);
	return;
}
#endif	/* __BCMIGD__ */
#endif	/* __CONFIG_NAT__ */

/* Set to upnp argument */
static void
upnp_set_arg(char *name, char *value)
{
	if (name == NULL || value == NULL)
		return;

	upnp_ptr += sprintf(upnp_ptr, "%s=%s\n", name, value);
	return;
}

static void
upnp_build_args()
{
	int i;
	char name[32];
	char *value;
	char *var, *p, *next;


	upnp_ptr = upnp_argbuf;

	/* Set common variables, all of them have default values */
	upnp_set_arg("os_name", nvram_get("os_name"));
	upnp_set_arg("os_version", nvram_get("os_version"));
	upnp_set_arg("upnp_port", nvram_get("upnp_port"));
	upnp_set_arg("upnp_ad_time", nvram_get("upnp_ad_time"));
	upnp_set_arg("upnp_sub_timeout", nvram_get("upnp_sub_timeout"));
	upnp_set_arg("upnp_conn_retries", nvram_get("upnp_conn_retries"));
	upnp_set_arg("log_level", nvram_get("log_level"));

	/* Primary lan mac for uuid */
	upnp_set_arg("lan_hwaddr", nvram_get("lan_hwaddr"));

	/* Set interface names */	
	/* Cat all lan interface together */
	for (i = 0; i < 255; i++) {
		if (i == 0)
			strcpy(name, "lan_ifname");
		else
			sprintf(name, "lan%d_ifname", i);

		value = nvram_get(name);
		if (value) {
			sprintf(name, "lan%d_ifname", i);
			upnp_set_arg(name, value);
		}
	}

#ifdef __CONFIG_NAT__
#ifdef __BCMIGD__
	/* Set IGD variables */
	value = nvram_get("router_disable");
	if (value && strcmp(value, "1") == 0) {
		/* Let IGD be unable to attach */
		upnp_set_arg("igd_status", "disabled");
	}
	else {
		char wan_ifname[IFNAMSIZ] = {0};
		char wan_devname[IFNAMSIZ] = {0};

		/* Set wan names to each UPNP_INTERFACE */
		upnp_wan_info(wan_ifname, wan_devname);

		for (i = 0; i < 255; i++) {
			if (i == 0)
				strcpy(name, "lan_ifname");
			else
				sprintf(name, "lan%d_ifname", i);

			value = nvram_get(name);
			if (value) {
				/* Set per wan ifname */
				sprintf(name, "wan%d_ifname", i);
				upnp_set_arg(name, wan_ifname);

				/* Set per wan devname */
				sprintf(name, "wan%d_devname", i);
				upnp_set_arg(name, wan_devname);
			}
		}
	}
#endif	/* __BCMIGD__ */
#endif	/* __CONFIG_NAT__ */

	/* Break the token */
	upnp_argc = 0;
	for (var = upnp_argbuf, p = var;
		var && var[0];
		var = next, p = 0) {
		/* Break to next */
		strtok_r(p, "\n", &next);
		upnp_argv[upnp_argc++] = var;
	}

	return;
}

static void
reap(int sig)
{
	pid_t pid;

	if (sig == SIGPIPE)
		return;

	while ((pid = waitpid(-1, NULL, WNOHANG)) > 0)
#ifdef DSLCPE
		;
#else
		printf("Reaped %d\n", (int)pid);
#endif /* DSLCPE */
}

int
main(int argc, char *argv[])
{
	char **argp = &argv[1];
	int daemonize = 0;
	int usage = 0;

	FILE *pidfile;
	int flag;

	/*
	 * Check whether this process is running
	 */
	if ((pidfile = fopen(BCMUPMP_PID_FILE_PATH, "r"))) {
		fprintf(stderr, "%s: UPnP has been started\n", __FILE__);

		fclose(pidfile);
		return -1;
	}

	/* Create pid file */
	if ((pidfile = fopen(BCMUPMP_PID_FILE_PATH, "w"))) {
		fprintf(pidfile, "%d\n", getpid());
		fclose(pidfile);
	}
	else {
		perror("pidfile");
		exit(errno);
	}

	/*
	 * Process arguments
	 */
	while (argp < &argv[argc]) {
		/* Compatible to old way */
		if (strcasecmp(*argp, "-W") == 0) {
			++argp;
		}
		else if (strcasecmp(*argp, "-D") == 0) {
			daemonize = 1;
		}
		else {
			usage = 1;
		}
		argp++;
	}

	/* Warn if the some arguments are not supported */
	if (usage)
		fprintf(stderr, "usage: %s -D\n", argv[0]);

	/*
	 * We need to have a reaper for child processes we may create.
	 * That happens when we send signals to the dhcp process to
	 * release an renew a lease on the external interface.
	 */
	signal(SIGCHLD, reap);

	/* Handle the TCP -EPIPE error */
	signal(SIGPIPE, reap);

	/*
	 * For some reason that I do not understand, this process gets
	 * a SIGTERM after sending SIGUSR1 to the dhcp process (to
	 * renew a lease).  Ignore SIGTERM to avoid being killed when
	 * this happens.
	 */
	/* signal(SIGTERM, SIG_IGN); */
	signal(SIGUSR1, upnp_restart_handler);
	fflush(stdout);

	signal(SIGINT, upnp_stop_handler);
	signal(SIGTERM, upnp_stop_handler);

	/*
	 * Enter mainloop
	 */
	if (daemonize && daemon(1, 1) == -1) {
		/* Destroy pid file */
		unlink(BCMUPMP_PID_FILE_PATH);

		perror("daemon");
		exit(errno);
	}

	/* Replace pid file with daemon pid */
	pidfile = fopen(BCMUPMP_PID_FILE_PATH, "w");
	fprintf(pidfile, "%d\n", getpid());
	fclose(pidfile);

	/* Read configuration to structure and call mainloop */
	while (1) {
		upnp_build_args();

		/* Reload config if user want to restart */
		flag = upnp_mainloop(upnp_argc, upnp_argv);
		if ((flag & UPNP_FLAG_RESTART) == 0)
			break;
	}

	/* Destroy pid file */
	unlink(BCMUPMP_PID_FILE_PATH);

	return 0;
}
