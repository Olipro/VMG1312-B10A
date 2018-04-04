/*
 * Broadcom Home Gateway Reference Design
 * Web Page Configuration Support Routines for EZConfig
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 * $Id: ezc.c 241182 2011-02-17 21:50:03Z $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <httpd.h>

#include <typedefs.h>
#include <proto/ethernet.h>
#include <bcmnvram.h>
#include <bcmutils.h>
#include <shutils.h>
#include <netconf.h>
#include <nvparse.h>
#include <wlutils.h>
#include <ezc.h>
#include <bcmcvar.h>

extern struct variable variables[];
extern char post_buf[];
extern char ezc_version[];
extern char no_cache[];

static int crypt_enable = 1;
static int ezc_error = 0;
static int seed = 41;
static ezc_cb_fn_t ezc_cb = NULL;

enum {
	NOTHING,
	REBOOT,
	RESTART,
	};

#define EZC_CRYPT_KEYLEN		32

extern char *webs_buf;
extern int webs_buf_offset;

#define websBufferCrypt(wp) if (crypt_enable) { gen_key(key, sizeof(key)); \
	crypt(webs_buf, webs_buf_offset, key, sizeof(key));}

#if defined(linux)

#include <signal.h>
#define sys_restart() kill(1, SIGHUP)
#define sys_reboot() kill(1, SIGTERM)

#endif

/* debug stuff */
#ifdef BCMDBG
int debug_ezc = 1;
#define EZCDBG(fmt, arg...)     { \
	if (debug_ezc) { \
		printf("%s: "fmt, __FUNCTION__ , ##arg); \
	} \
}
#else   /* #if BCMDBG */
#define EZCDBG(fmt, arg...)
#endif  /* #if BCMDBG */

/* This Crypt routine uses an algorithm created by Rodney Thayer.
 * This algorithm can use keys of various sizes.
 * It is compatible with RSA's RC4 algorithm.
 *
 * This routine is constructed from a draft taken from Internet Engineering
 * Task Force (IETF) at http://www.ietf.cnri.reston.va.us/home.htm
 */

static void
crypt(char *buffer, int len, char *key, int keylen)
{
	char sbox[128];
	uint i, j, t, x;
	char temp;

	if ((buffer == NULL) || (len == 0) || (key == NULL) || (keylen == 0))
		return;

	for (i = 0; i < sizeof(sbox); i++)
		sbox[i] = i;

	/* scramble sbox with key */
	for (i = 0, j = 0; i < sizeof(sbox); i++) {
		j = (j + sbox[i] + key[i%keylen]) % sizeof(sbox);
		temp = sbox[i];
		sbox[i] = sbox[j];
		sbox[j] = temp;
	}

	for (i = j = x = 0; x < len; x++) {
		i = (i + 1) % sizeof(sbox);
		j = (j + sbox[i]) % sizeof(sbox);

		/* create pseudo random byte for encryption key */
		t = (sbox[i] + sbox[j]) % sizeof(sbox);

		/* xor with the random byte and done */
		buffer[x] ^= (sbox[t] ^ 0x80);
	}
}

static void
gen_key(char *key, int keylen)
{
	int i;

	key[0] = seed;
	for (i = 1; i < keylen; i++) {
		key[i] = (key[i-1] + seed) % 128;
	}
}


static void
escape_it(char *to, char *from)
{
	while (*from) {
		if ((*from == '=') || (*from == '&') || (*from == '%'))
			to += sprintf(to, "%%%02x", *from);
		else
			*to++ = *from;
		from++;
	}
	*to = 0;
}

void
ezc_nv_deprecate(webs_t wp, int unit, char *name, char *value)
{
	char tmp[NVRAM_BUFSIZE];

	if (strcmp(name, "wl_auth_mode"))
		return;

	/* sta expects wl_auth_mode to be open/shared/wpa/psk/radius */
	if (!strcmp(value, "none")) {
		snprintf(tmp, sizeof(tmp), "wl%d_akm", unit);
		if (strstr(nvram_safe_get(tmp), "psk"))
			strcpy(value, "psk");
		else if (strstr(nvram_safe_get(tmp), "wpa"))
			strcpy(value, "wpa");
		else
			strcpy(value, "open");
	}
}

void
ezc_nv_upgrade(char *name, char *value)
{
	if (strcmp(name, "wl_auth_mode"))
		return;

	/* sta sends wl_auth_mode to be open/psk; convert to none/wl_akm */
	if (!strcmp(value, "psk")) {
		EZCDBG("setting wl_akm to psk\n");
		nvram_set("wl_akm", "psk");

		if (ezc_cb)
			ezc_cb("wl_akm", "psk");
	}

	if (strcmp(value, "radius"))
		strcpy(value, "none");
}

static int
apply_ezconfig(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
               char_t *url, char_t *path, char_t *query)
{
	int action = NOTHING;
	char *value, *unit_str = NULL;
	struct variable *v;
	char tmp[NVRAM_BUFSIZE], tname[NVRAM_BUFSIZE], tvalue[NVRAM_BUFSIZE];
	int count;
	int unit_needed = 0;
	int wan_present = 0;
	char key[EZC_CRYPT_KEYLEN];
	int asize = variables_arraysize();

	EZCDBG("Received POST request\n");

	value = websGetVar(wp, "action", "");
	if (!strcmp(value, "Apply")) {

		count = count_cgi();

		/* subtract for action */
		count -= 1;

		EZCDBG("Received apply request with %d nv pairs\n", count);

		/* validate correct args */
		for (v = variables; v < &variables[asize]; v++) {
			if ((v->ezc_flags & EZC_FLAGS_WRITE) == 0)
				continue;
			if (websGetVar(wp, v->name, NULL)) {
				count--;
				if (!strncmp(v->name, "wl_", 3))
					unit_needed = TRUE;
				if (!strncmp(v->name, "wan_", 4))
					wan_present = TRUE;
			}
		}

		if (unit_needed) {
			if (!(unit_str = websGetVar(wp, "wl_unit", NULL))) {
				websBufferWrite(wp, "status=%d&", EZC_ERR_INVALID_DATA);
				EZCDBG("ERROR: wl_unit not set\n");
				goto done;
			}
			snprintf(tmp, sizeof(tmp), "wl%s_unit", unit_str);
			if (!nvram_get(tmp)) {
				websBufferWrite(wp, "status=%d&", EZC_ERR_INVALID_DATA);
				EZCDBG("ERROR: invalid unit\n");
				goto done;
			}
			EZCDBG("recd wl_unit as %s\n", unit_str);
			if (ezc_cb)
				ezc_cb("wl_unit", unit_str);

			count--;
		}

		if (count) {
			websBufferWrite(wp, "status=%d&", EZC_ERR_INVALID_DATA);
			EZCDBG("ERROR: invalid count %d(extra)\n", count);
			goto done;
		}

		/* copy all wan0_* to wan_* */
		for (v = variables; (v < &variables[asize]) && wan_present; v++) {
			if (!strncmp(v->name, "wan_", 4)) {
				snprintf(tmp, sizeof(tmp), "wan0_%s", &v->name[4]);
				nvram_set(v->name, nvram_safe_get(tmp));
			}
		}

		/* copy all wl<wl_unit>_* to wl_* */
		for (v = variables; (v < &variables[asize]) && unit_needed; v++) {
			if (!strncmp(v->name, "wl_", 3)) {
				snprintf(tmp, sizeof(tmp), "wl%s_%s", unit_str, &v->name[3]);
				nvram_set(v->name, nvram_safe_get(tmp));
			}
		}

		/* retrieve all variables */
		for (v = variables; v < &variables[asize]; v++) {
			if ((v->ezc_flags & EZC_FLAGS_WRITE) == 0)
				continue;
			if (!(value = websGetVar(wp, v->name, NULL)))
				continue;

			strcpy(tname, v->name);
			strcpy(tvalue, value);

			ezc_nv_upgrade(tname, tvalue);

			EZCDBG("setting %s to %s\n", tname, tvalue);

			if ((!*value && v->nullok) || !v->validate)
				nvram_set(tname, tvalue);
			else
				v->validate(wp, tvalue, v, NULL);

			if (ezc_cb)
				ezc_cb(tname, tvalue);
		}

		/* copy all wl_* to wl<unit>_* */
		for (v = variables; (v < &variables[asize]) && unit_needed; v++) {
			if (!strncmp(v->name, "wl_", 3)) {
				snprintf(tmp, sizeof(tmp), "wl%s_%s", unit_str, &v->name[3]);
				nvram_set(tmp, nvram_safe_get(v->name));
			}
		}

		/* copy all wan_* to wan0_* */
		for (v = variables; (v < &variables[asize]) && wan_present; v++) {
			if (!strncmp(v->name, "wan_", 4)) {
				snprintf(tmp, sizeof(tmp), "wan0_%s", &v->name[4]);
				nvram_set(tmp, nvram_safe_get(v->name));
			}
		}

		/* Set currently selected unit */
		nvram_set("wl_unit", unit_str);

		if (ezc_cb)
			ezc_cb("action", "Apply");

		nvram_set("is_modified", "1");
		nvram_set("is_default", "0");
		nvram_commit();
		action = RESTART;
	}
	/* Restore defaults */
	else if (!strcmp(value, "Restore")) {
		if (count_cgi() != 1) {
			websBufferWrite(wp, "status=%d&", EZC_ERR_INVALID_DATA);
			EZCDBG("ERROR: invalid count for Restore\n");
			goto done;
		}
		if (ezc_cb)
			ezc_cb("action", "Restore");
		nvram_set("sdram_ncdl", "0");
		nvram_set("restore_defaults", "1");
		nvram_commit();
		action = REBOOT;
	} else {
		websBufferWrite(wp, "status=%d&", EZC_ERR_INVALID_DATA);
		goto done;
	}

	websBufferWrite(wp, "status=%d&", EZC_SUCCESS);

done:

	websBufferCrypt(wp);
	websBufferFlush(wp);

	if (action == RESTART)
		sys_restart();
	else if (action == REBOOT)
		sys_reboot();

	return 1;
}

static int
ezconfig_asp(webs_t wp, char_t *urlPrefix, char_t *webDir, int arg,
             char_t *url, char_t *path, char_t *query)
{
	struct variable *v;
	char name[IFNAMSIZ], *next;
	int unit = 0;
	char tmp[NVRAM_BUFSIZE];
	char esc[NVRAM_BUFSIZE];
	char ifnames[256];
	char key[EZC_CRYPT_KEYLEN];
	int asize = variables_arraysize();

	/* EZCDBG("Received ezconfig request\n"); */

	websBufferInit(wp);

	if (!webs_buf) {
		websWrite(wp, "out of memory\n");
		websDone(wp, 0);
		EZCDBG("ezconfig_asp: out of memory\n");
		return 0;
	}

	if (strcmp(nvram_safe_get("ezc_enable"), "1")) {
		websBufferWrite(wp, "status=%d&", EZC_ERR_NOT_ENABLED);
		EZCDBG("ezc_enable is not set\n");
		goto done;
	}

	/* proceed if only a single wan profile at unit 0 */
	for (unit = 1; unit < MAX_NVPARSE; unit ++) {
		snprintf(tmp, sizeof(tmp), "wan%d_unit", unit);
		if (nvram_get(tmp)) {
			websBufferWrite(wp, "status=%d&", EZC_ERR_INVALID_STATE);
			EZCDBG("ERROR: multiple wan profiles not supported\n");
			goto done;
		}
	}

	if (ezc_error) {
		websBufferWrite(wp, "status=%d&", EZC_ERR_INVALID_DATA);
		EZCDBG("ERROR: ezc_error is set\n");
		goto done;
	}

	/* Can enter this function through GET or POST */
	if (websGetVar(wp, "action", NULL)) {
		return apply_ezconfig(wp, urlPrefix, webDir, arg, url, path, query);
	}

	websBufferWrite(wp, "status=%d&is_default=%s&is_modified=%s&",
		EZC_SUCCESS, nvram_safe_get("is_default"), nvram_safe_get("is_modified"));

#if defined(linux)
	{
	char *str = file2str("/proc/uptime");
	if (str) {
		websBufferWrite(wp, "sys_uptime=%s&", str);
		free(str);
	}
	}
#endif


	snprintf(ifnames, sizeof(ifnames), "%s %s",
		nvram_safe_get("lan_ifnames"),
		nvram_safe_get("wan_ifnames"));

	/* retrieve non wl_xxx variables */
	for (v = variables; v < &variables[asize]; v++) {

		if ((v->ezc_flags & EZC_FLAGS_READ) == 0)
			continue;

		if (strncmp(v->name, "wl_", 3)) {
			escape_it(esc, nvram_safe_get(v->name));
			websBufferWrite(wp, "%s=%s&", v->name, esc);
		}
	}

	websBufferWrite(wp, "lan_hwaddr=%s&", nvram_safe_get("lan_hwaddr"));

	/* retrieve all wl_xxxx variables */
	foreach(name, ifnames, next) {
		if (wl_probe(name) == 0 &&
		    wl_ioctl(name, WLC_GET_INSTANCE, &unit, sizeof(unit)) == 0) {

			/* add wl_hwaddr */
			snprintf(tmp, sizeof(tmp), "wl%d_hwaddr", unit);
			websBufferWrite(wp, "%s=%s&", tmp, nvram_safe_get(tmp));

			for (v = variables; v < &variables[asize]; v++) {

				if ((v->ezc_flags & EZC_FLAGS_READ) == 0)
					continue;

				if (!strncmp(v->name, "wl_", 3)) {
					snprintf(tmp, sizeof(tmp), "wl%d_%s", unit, &v->name[3]);
					escape_it(esc, nvram_safe_get(tmp));
					ezc_nv_deprecate(wp, unit, tmp, esc);
					websBufferWrite(wp, "%s=%s&", tmp, esc);
				}
			}
		}
	}

	/* EZCDBG("processed GET request\n"); */

done:
	websBufferCrypt(wp);
	websBufferFlush(wp);
	return 0;
}

void
do_apply_ezconfig_post(char *url, FILE *stream, int len, char *boundary)
{
	char key[EZC_CRYPT_KEYLEN];

	/* ready the output header */
	sprintf(ezc_version, "%s\r\nEZC-Version: %s", no_cache, nvram_safe_get("ezc_version"));

	ezc_error = 0;

	/* Get query */
	if (!fgets(post_buf, MIN(len + 1, POST_BUF_SIZE), stream))
		return;
	len -= strlen(post_buf);

	if (crypt_enable) {
		gen_key(key, sizeof(key));
		crypt(post_buf, strlen(post_buf), key, sizeof(key));
	}

	/* Initialize CGI */
	init_cgi(post_buf);

	/* validate we got something useful */
	if (count_cgi() == 0)
		ezc_error = 1;

	/* Slurp anything remaining in the request */
	while (len--)
		(void) fgetc(stream);
}

void
do_ezconfig_asp(char *url, FILE *stream)
{
	char *path, *query;

	/* Parse path */
	query = url;
	path = strsep(&query, "?") ? : url;

	ezconfig_asp(stream, NULL, NULL, 0, url, path, query);

	/* Reset CGI */
	init_cgi(NULL);
}

int
ezc_register_cb(ezc_cb_fn_t fn)
{
	EZCDBG("Registering callback function\n");
	ezc_cb = fn;
	return EZC_SUCCESS;
}
