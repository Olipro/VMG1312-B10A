
#if 1 //__MSTC__,Peter, wps lockdown

/*Since the new version of WPS lockdown don't meet our require
  we will using the old version which meet customer requirements.
: Peter.Lee.MitraStar 2012-07-18*/

/*
 * WPS aplockdown
 *
 * Copyright (C) 2010, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_aplockdown.c 241376 2011-02-18 03:19:15Z stakita $
 */

#include <stdio.h>
#include <time.h>
#include <tutrace.h>
#include <wps.h>
#include <wps_wl.h>
#include <wps_aplockdown.h>
#include <wps_ui.h>
#include <wps_ie.h>
#if 1 // __MSTC__, Hong-Yu
#include "cms_log.h"
#endif

typedef struct wps_aplockdown_node {
	struct wps_aplockdown_node *next;
	unsigned long access_time;
} WPS_APLOCKDOWN_NODE;

typedef struct {
	WPS_APLOCKDOWN_NODE *head;
	WPS_APLOCKDOWN_NODE *tail;
	unsigned int force_on;
	unsigned int enabled;
	unsigned int locked;
	unsigned int living_cnt;
	unsigned int limit_cnt;
	unsigned int duration;
	unsigned int ageout;
#if 1 // __NSTC__, Hong-Yu, Not clean the lock
	unsigned int locked_forever;
#endif
} WPS_APLOCKDOWN;

/* How many failed PIN authentication attempts */
#if 1 // __MSTC__, Hong-Yu, not clean the lock
#define WPS_APLOCKDOWN_FOREVER		1
#endif
#define WPS_APLOCKDOWN_LIMIT		30
#define WPS_V2_APLOCKDOWN_LIMIT		3

/* Within how many seconds */
#define WPS_APLOCKDOWN_DURATION		300
#define WPS_V2_APLOCKDOWN_DURATION	60

/* How many seconds AP must stay in the lock-down state */
#define WPS_APLOCKDOWN_AGEOUT		300
#define WPS_V2_APLOCKDOWN_AGEOUT	60

static WPS_APLOCKDOWN wps_aplockdown;

int
wps_aplockdown_init()
{
	char *p = NULL;
	uint32 limit = 0;
	uint32 duration = 0;
	uint32 ageout = 0;
#if 1 // __MSTC__, Hong-Yu, Not clean the lock
	uint32 locked_forever = 0;
#endif

	memset(&wps_aplockdown, 0, sizeof(wps_aplockdown));

	if (!strcmp(wps_safe_get_conf("wps_aplockdown_forceon"), "1"))
		wps_aplockdown.force_on = 1;

	/* wps_aplockdown_cap not equal to 1 */
	if (strcmp(wps_safe_get_conf("wps_aplockdown_cap"), "1") != 0) {
		TUTRACE((TUTRACE_INFO, "WPS AP lock down capability is disabling!\n"));
		return 0;
	}

	wps_aplockdown.enabled = 1;

	/* WSC 2.0 */
	if (strcmp(wps_safe_get_conf("wps_version2"), "enabled") == 0) {
		limit = WPS_V2_APLOCKDOWN_LIMIT;
		duration = WPS_V2_APLOCKDOWN_DURATION;
		ageout = WPS_V2_APLOCKDOWN_AGEOUT;
	}
	else {
		limit = WPS_APLOCKDOWN_LIMIT;
		duration = WPS_APLOCKDOWN_DURATION;
		ageout = WPS_APLOCKDOWN_AGEOUT;
	}
#if 1 // __MSTC__, Hong-Yu, Not clean the lock
	locked_forever = WPS_APLOCKDOWN_FOREVER;
#endif

	if ((p = wps_get_conf("wps_aplockdown_count")))
		limit = atoi(p);

	if ((p = wps_get_conf("wps_aplockdown_duration")))
		duration = atoi(p);

	if ((p = wps_get_conf("wps_aplockdown_ageout")))
		ageout = atoi(p);

#if 1 // __MSTC__, Hong-Yu, Not clean the lock
	if ((p = wps_get_conf("wps_aplockdown_lockforever")))
		locked_forever = atoi(p);
#endif

	wps_aplockdown.limit_cnt = limit;
	wps_aplockdown.duration = duration;
	wps_aplockdown.ageout = ageout;
#if 1 // __Telus__, Hong-Yu, Not clean the lock
	wps_aplockdown.locked_forever = locked_forever;
#endif

	TUTRACE((TUTRACE_INFO, "WPS aplockdown init: limit = %d, duration = %d, ageout = %d!,lockforever=%d \n",
		limit, duration, ageout,locked_forever));

	return 0;
}

int
wps_aplockdown_add(void)
{
	unsigned long now;
	WPS_APLOCKDOWN_NODE *curr = NULL;
	WPS_APLOCKDOWN_NODE *prev = NULL;
	WPS_APLOCKDOWN_NODE *next = NULL;

	if (wps_aplockdown.enabled == 0)
		return 0;

	if (wps_aplockdown.locked == 1)
		return 0;

	time(&now);

	TUTRACE((TUTRACE_INFO, "Error of config AP pin fail in %d!\n", now));

	curr = (WPS_APLOCKDOWN_NODE *)malloc(sizeof(*curr));
	if (!curr) {
		TUTRACE((TUTRACE_INFO, "Memory allocation for WPS aplockdown service fail!!\n"));
		return -1;
	}

	curr->access_time = now;
	curr->next = wps_aplockdown.head;

	/*
	 * Add an WPS_APLOCKDOWN_NODE to list
	 */
	wps_aplockdown.head = curr;
	if (wps_aplockdown.tail == NULL)
		wps_aplockdown.tail = curr;

	wps_aplockdown.living_cnt++;
#if 1 // __MSTC__, Hong-Yu
	fprintf(stderr, "#### current living trial count = %d!\n", wps_aplockdown.living_cnt);
#endif

	/*
	 * If hit AP setup lock criteria,
	 * check duration
	 */
	if (wps_aplockdown.living_cnt >= wps_aplockdown.limit_cnt) {
		if ((curr->access_time - wps_aplockdown.tail->access_time)
		    <= wps_aplockdown.duration) {
			/* set apLockDown mode indicator as true */

			wps_aplockdown.locked = 1;

			wps_ui_set_env("wps_aplockdown", "1");
			wps_set_conf("wps_aplockdown", "1");

			/* reset the IE */
			wps_ie_set(NULL, NULL);

			TUTRACE((TUTRACE_ERR, "AP is lock down now\n"));
#if 1 // __MSTC__, Hong-Yu
			zyLog_cfg(LOG_FAC_WIFI, "WPS AP PIN is lock down now");
			fprintf(stderr, "AP is lock down now\n\n");
#endif
			return 1;
		}
	}

	/*
	 * Age out nodes in list
	 */
	wps_aplockdown.living_cnt = 0;
	curr = prev = wps_aplockdown.head;
	while (curr) {
		/*
		 * Hit the age out criteria
		 */
		if ((unsigned long)(now - curr->access_time) > wps_aplockdown.duration) {
			prev->next = NULL;
			wps_aplockdown.tail = prev;
			while (curr) {
				next = curr->next;
				free(curr);
				curr = next;
			}
			break;
		}

		wps_aplockdown.living_cnt++;
		prev = curr;
		curr = curr->next;
	}

	TUTRACE((TUTRACE_INFO, "Fail AP pin trial count = %d!\n", wps_aplockdown.living_cnt));

	return wps_aplockdown.locked;
}

int
wps_aplockdown_check(void)
{
	unsigned long now;

	if (wps_aplockdown.enabled == 0 || wps_aplockdown.living_cnt == 0)
		return 0;

#if 1 // __MSTC__, Hong-Yu
	if (wps_aplockdown.locked == 1 && wps_aplockdown.locked_forever == 1)
		return 1;
#endif

	/* get current time and oldest time */
	(void) time(&now);

	/* check if latest pinfail trial is ageout */
	if ((unsigned long)(now - wps_aplockdown.head->access_time)
		> wps_aplockdown.ageout)
	{
		/* clean up nodes within ap_lockdown_log */
		wps_aplockdown_cleanup();

		if (wps_aplockdown.locked) {
			/* unset apLockDown indicator */
			wps_aplockdown.locked = 0;

			wps_ui_set_env("wps_aplockdown", "0");
			wps_set_conf("wps_aplockdown", "0");

			/* reset the IE */
			wps_ie_set(NULL, NULL);

			TUTRACE((TUTRACE_INFO, "Unlock AP lock down\n"));
		}
	}

	return wps_aplockdown.locked;
}

int
wps_aplockdown_islocked()
{
	return wps_aplockdown.locked | wps_aplockdown.force_on;
}

int
wps_aplockdown_cleanup()
{
	WPS_APLOCKDOWN_NODE *curr = NULL;
	WPS_APLOCKDOWN_NODE *next = NULL;

	if (wps_aplockdown.enabled == 0)
		return 0;

	curr = wps_aplockdown.head;
	while (curr) {
		next = curr->next;
		free(curr);
		curr = next;
	}

	/* clear old info in ap_lockdown_log */
	wps_aplockdown.head = NULL;
	wps_aplockdown.tail = NULL;
	wps_aplockdown.living_cnt = 0;

	return 0;
}

#else
/*
 * WPS aplockdown
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wps_aplockdown.c 316112 2012-02-21 03:10:17Z $
 */

#include <stdio.h>
#include <time.h>
#include <tutrace.h>
#include <wps.h>
#include <wps_wl.h>
#include <wps_aplockdown.h>
#include <wps_ui.h>
#include <wps_ie.h>

typedef struct {
	unsigned int force_on;
#ifdef DSLCPE_NG
	unsigned int disabled;	
#endif
	unsigned int locked;
	unsigned int time;
	unsigned int start_cnt;
	unsigned int forever_cnt;
	int failed_cnt;
} WPS_APLOCKDOWN;

#define WPS_APLOCKDOWN_START_CNT	3
#define	WPS_APLOCKDOWN_INIT_AGEOUT	10
#define	WPS_APLOCKDOWN_FOREVER_CNT	10

#ifdef DSLCPE_NG
#define WPS_APLOCKDOWN_ACCUMULATED
#ifdef WPS_APLOCKDOWN_ACCUMULATED 
#define	WPS_PIN_FAILED_CNT_FILE_PATH	"/tmp/wps_pin_failed_cnt"  
#endif
#endif

static WPS_APLOCKDOWN wps_aplockdown;

int
wps_aplockdown_init()
{
	char *p;
	int start_cnt = 0;
	int forever_cnt = 0;
#ifdef DSLCPE_NG
#ifdef WPS_APLOCKDOWN_ACCUMULATED	
	FILE *fp; 
#endif
#endif
	memset(&wps_aplockdown, 0, sizeof(wps_aplockdown));

	if (!strcmp(wps_safe_get_conf("wps_aplockdown_forceon"), "1")) {
		wps_aplockdown.force_on = 1;

		wps_ui_set_env("wps_aplockdown", "1");
		wps_set_conf("wps_aplockdown", "1");
	}
	else {
		wps_ui_set_env("wps_aplockdown", "0");
		wps_set_conf("wps_aplockdown", "0");
	}
#ifdef DSLCPE_NG
	/* wps_aplockdown_disable equal to 1 */
	if (!strcmp(wps_safe_get_conf("wps_aplockdown_disable"), "1")) {
		TUTRACE((TUTRACE_INFO, "WPS AP lock down capability is disabling!\n"));
		wps_aplockdown.disabled = 1;
		return 0;
	}
	else
		wps_aplockdown.disabled = 0;
#endif
	/* check lock start count */
	if ((p = wps_get_conf("wps_lock_start_cnt")))
		start_cnt = atoi(p);

	if (start_cnt < WPS_APLOCKDOWN_START_CNT ||
	    start_cnt > WPS_APLOCKDOWN_FOREVER_CNT) {
		/* Default start count */
		start_cnt = WPS_APLOCKDOWN_START_CNT;
	}

	/* check lock forever count */
	if ((p = wps_get_conf("wps_lock_forever_cnt")))
		forever_cnt = atoi(p);

	if (forever_cnt < WPS_APLOCKDOWN_START_CNT ||
	    forever_cnt > WPS_APLOCKDOWN_FOREVER_CNT) {
		/* Default forever lock count */
		forever_cnt = WPS_APLOCKDOWN_FOREVER_CNT;
	}

	if (start_cnt > forever_cnt)
		start_cnt = forever_cnt;

	/* Save to structure */
	wps_aplockdown.start_cnt = start_cnt;
	wps_aplockdown.forever_cnt = forever_cnt;

	TUTRACE((TUTRACE_INFO,
		"WPS aplockdown init: force_on = %d, start_cnt = %d, forever_cnt = %d!\n",
		wps_aplockdown.force_on,
		wps_aplockdown.start_cnt,
		wps_aplockdown.forever_cnt));
#ifdef DSLCPE_NG
#ifdef WPS_APLOCKDOWN_ACCUMULATED
	if ((fp = fopen(WPS_PIN_FAILED_CNT_FILE_PATH, "r"))) {
		TUTRACE((TUTRACE_INFO, "%s: PIN failed count file already exist\n", __FUNCTION__));
		fscanf(fp, "%d", &wps_aplockdown.failed_cnt);
		if (wps_aplockdown.failed_cnt >= wps_aplockdown.start_cnt) {
				wps_aplockdown.locked = 1;
				wps_ui_set_env("wps_aplockdown", "1");
				wps_set_conf("wps_aplockdown", "1");
				/* reset the IE */
				wps_ie_set(NULL, NULL);
				TUTRACE((TUTRACE_ERR, "%s: AP is lock down now\n", __FUNCTION__));
		}		
		fclose(fp);
	}
	else {
		if ((fp = fopen(WPS_PIN_FAILED_CNT_FILE_PATH, "w"))) {
			fprintf(fp, "%d\n", 0);
			fclose(fp);
			TUTRACE((TUTRACE_INFO, "%s: set PIN failed count to 0 \n", __FUNCTION__));
		}
		else 
			TUTRACE((TUTRACE_INFO, "%s: fopen file!\n", __FUNCTION__));
	}
#endif
#endif
	return 0;
}

int
wps_aplockdown_add(void)
{
	unsigned long now;
#ifdef DSLCPE_NG
#ifdef WPS_APLOCKDOWN_ACCUMULATED
	FILE *fp;
#endif
#endif
	time((time_t *)&now);

#ifdef DSLCPE_NG
	if (wps_aplockdown.disabled == 1)
		return 0;
#endif

	TUTRACE((TUTRACE_INFO, "Error of config AP pin fail in %d!\n", now));

#ifdef DSLCPE_NG
#ifdef WPS_APLOCKDOWN_ACCUMULATED
	fp = fopen(WPS_PIN_FAILED_CNT_FILE_PATH, "r");
	if (fp) {
		fscanf(fp, "%d", &wps_aplockdown.failed_cnt);
		fclose(fp);
	}
	TUTRACE((TUTRACE_INFO, "current PIN failed count = %d!\n", wps_aplockdown.failed_cnt));
#endif
#endif

	/*
	 * Add PIN failed count
	 */
	if (wps_aplockdown.failed_cnt < wps_aplockdown.forever_cnt) {
		wps_aplockdown.failed_cnt++;
#ifdef DSLCPE_NG
#ifdef WPS_APLOCKDOWN_ACCUMULATED/
		fp = fopen(WPS_PIN_FAILED_CNT_FILE_PATH, "w");
		if (fp) {
			fprintf(fp, "%d\n", wps_aplockdown.failed_cnt);
			fclose(fp);
		}
		TUTRACE((TUTRACE_INFO, "add PIN failed count to %d!\n", wps_aplockdown.failed_cnt));
#endif
#endif
	}
	/*
	 * Lock it if reach start count.
	 */
	if (wps_aplockdown.failed_cnt >= wps_aplockdown.start_cnt) {
		wps_aplockdown.locked = 1;
		wps_aplockdown.time = now;

		wps_ui_set_env("wps_aplockdown", "1");
		wps_set_conf("wps_aplockdown", "1");

		/* reset the IE */
		wps_ie_set(NULL, NULL);

		TUTRACE((TUTRACE_ERR, "AP is lock down now\n"));
	}

	TUTRACE((TUTRACE_INFO, "Fail AP pin trial count = %d!\n", wps_aplockdown.failed_cnt));

	return wps_aplockdown.locked;
}

int
wps_aplockdown_check(void)
{
	unsigned long now;
	int ageout;
#ifdef DSLCPE_NG
	if (wps_aplockdown.disabled == 1)
		return 0;
#endif
	if (wps_aplockdown.locked == 0)
		return 0;

	/* check lock forever */
	if (wps_aplockdown.force_on ||
	    wps_aplockdown.failed_cnt >= wps_aplockdown.forever_cnt)
		return 1;

	/* wps_aplockdown.failed_cnt will always >= wps_aplockdown.start_cnt,
	 * so, ageout start from 1 minutes.
	 */
	ageout = (1 << (wps_aplockdown.failed_cnt - wps_aplockdown.start_cnt)) * 60;

	time((time_t *)&now);

	/* Lock release check */ 
	if ((unsigned long)(now - wps_aplockdown.time) > ageout) {
		/* unset apLockDown indicator */
		wps_aplockdown.locked = 0;

		wps_ui_set_env("wps_aplockdown", "0");
		wps_set_conf("wps_aplockdown", "0");

		/* reset the IE */
		wps_ie_set(NULL, NULL);

		TUTRACE((TUTRACE_INFO, "Unlock AP lock down\n"));
	}

	return wps_aplockdown.locked;
}

int
wps_aplockdown_islocked()
{
	return wps_aplockdown.locked | wps_aplockdown.force_on;
}

int
wps_aplockdown_cleanup()
{
#ifdef DSLCPE_NG
	if (wps_aplockdown.disabled == 1)
		return 0;
//	memset(&wps_aplockdown, 0, sizeof(wps_aplockdown));
	/* Clean dynamic variables */
	wps_aplockdown.locked = 0;
	wps_aplockdown.time = 0;
	wps_aplockdown.failed_cnt = 0;
#else
	memset(&wps_aplockdown, 0, sizeof(wps_aplockdown));
#endif
	return 0;
}
#endif //__MSTC__, WPS lockdwon