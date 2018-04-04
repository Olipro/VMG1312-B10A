/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wlan.c 277453 2011-08-15 18:52:06Z $
 *
 * Description: Implement functions handling WLAN activities
 *
 */

#include <stdlib.h>
#include <string.h>
#ifndef OFFSETOF
#define	OFFSETOF(type, member)	((uint)(uintptr)&((type *)0)->member)
#endif /* OFFSETOF */
#include <wpscli_osl.h>
#include <bcmutils.h>
#include <tutrace.h>

extern int wpscli_iovar_set(const char *iovar, void *param, uint paramlen);

#define WLAN_JOIN_ATTEMPTS	3
#define WLAN_POLLING_JOIN_COMPLETE_ATTEMPTS	20
#define WLAN_POLLING_JOIN_COMPLETE_SLEEP	100
#define WLAN_JOIN_SCAN_DEFAULT_ACTIVE_TIME 20
#define WLAN_JOIN_SCAN_ACTIVE_TIME 60
#define WLAN_JOIN_SCAN_PASSIVE_TIME 150
#define WLAN_JOIN_SCAN_PASSIVE_TIME_LONG 2000

#if !defined(WL_ASSOC_PARAMS_FIXED_SIZE) || !defined(WL_JOIN_PARAMS_FIXED_SIZE)
static int join_network(char* ssid, uint32 wsec);
#endif /* !defined(WL_ASSOC_PARAMS_FIXED_SIZE) || !defined(WL_JOIN_PARAMS_FIXED_SIZE) */
static int join_network_with_bssid_active(const char* ssid, uint32 wsec, const char *bssid,
	int num_chanspec, chanspec_t *chanspec);
static int join_network_with_bssid(const char* ssid, uint32 wsec, const char *bssid,
	int num_chanspec, chanspec_t *chanspec);
static int leave_network(void);
extern int brcm_wpscli_ioctl_err;

brcm_wpscli_status wpscli_wlan_open(void)
{
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlan_close(void)
{
	return WPS_STATUS_SUCCESS;
}

/* make a wlan connection. */
brcm_wpscli_status wpscli_wlan_connect(const char* ssid, uint32 wsec, const char *bssid,
	int num_chanspec, chanspec_t *chanspec)
{
	int ret = 0;
	int auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;

	/*
	 * If wep bit is on,
	 * pick any WPA encryption type to allow association.
	 * Registration traffic itself will be done in clear (eapol).
	*/
	if (wsec)
		wsec = 2; /* TKIP */

	/* set infrastructure mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_INFRA,
		(const char *)&infra, sizeof(int))) < 0)
		return ret;

	/* set authentication mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_AUTH,
		(const char *)&auth, sizeof(int))) < 0)
		return ret;

	/* set wsec mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WSEC,
		(const char *)&wsec, sizeof(int))) < 0)
		return ret;

	/* set WPA_auth mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WPA_AUTH,
		(const char *)&wpa_auth, sizeof(wpa_auth))) < 0)
		return ret;

#if !defined(WL_ASSOC_PARAMS_FIXED_SIZE) || !defined(WL_JOIN_PARAMS_FIXED_SIZE)
	if (join_network(ssid, wsec) == 0)
		return WPS_STATUS_SUCCESS;
#else
	/* attempt join with first channel */
	if (join_network_with_bssid_active(ssid, wsec, bssid, num_chanspec ? 1 : 0, chanspec) == 0)
		return WPS_STATUS_SUCCESS;
	if (join_network_with_bssid(ssid, wsec, bssid, num_chanspec ? 1 : 0, chanspec) == 0)
		return WPS_STATUS_SUCCESS;

	if (num_chanspec > 1 && chanspec != NULL) {
		/* attempt join with remaining channels */
		if (join_network_with_bssid_active(ssid, wsec, bssid, num_chanspec - 1, &chanspec[1]) == 0)
			return WPS_STATUS_SUCCESS;
		if (join_network_with_bssid(ssid, wsec, bssid, num_chanspec - 1, &chanspec[1]) == 0)
			return WPS_STATUS_SUCCESS;
	}
#endif /* !defined(WL_ASSOC_PARAMS_FIXED_SIZE) || !defined(WL_JOIN_PARAMS_FIXED_SIZE) */

	return WPS_STATUS_WLAN_CONNECTION_ATTEMPT_FAIL;
}

/* disconnect wlan connection */
brcm_wpscli_status wpscli_wlan_disconnect(void)
{
	leave_network();

	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_wlan_scan(wl_scan_results_t *ap_list, uint32 buf_size)
{
	brcm_wpscli_status status = WPS_STATUS_WLAN_NO_ANY_AP_FOUND;
	int retry;
	wl_scan_params_t* params;
	int params_size = WL_SCAN_PARAMS_FIXED_SIZE + WL_NUMCHANNELS * sizeof(uint16);

	TUTRACE((TUTRACE_INFO, "Entered: wpscli_wlan_scan\n"));

	if (ap_list == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	params = (wl_scan_params_t*)malloc(params_size);

	if (params == NULL) {
		printf("Error allocating %d bytes for scan params\n", params_size);
		return WPS_STATUS_SYSTEM_ERR;
	}

	memset(params, 0, params_size);
	params->bss_type = DOT11_BSSTYPE_ANY;
	memcpy(&params->bssid, &ether_bcast, ETHER_ADDR_LEN);
	params->scan_type = -1;
	params->nprobes = -1;
	params->active_time = -1;
	params->passive_time = -1;
	params->home_time = -1;
	params->channel_num = 0;

	wpscli_wlh_ioctl_set(WLC_SCAN, (const char *)params, params_size);

	/* Poll for the results once a second until the scan is done */
	for (retry = 0; retry < WLAN_SCAN_TIMEOUT; retry++) {
		wpscli_sleep(1000);

		ap_list->buflen = WPS_DUMP_BUF_LEN;

		status = wpscli_wlh_ioctl_get(WLC_SCAN_RESULTS, (char *)ap_list, buf_size);

		/* break out if the scan result is ready */
		if (status == WPS_STATUS_SUCCESS)
			break;
	}

	free(params);

	TUTRACE((TUTRACE_INFO, "Exit: wpscli_wlan_scan. status=%d\n", status));
	return status;
}

#if !defined(WL_ASSOC_PARAMS_FIXED_SIZE) || !defined(WL_JOIN_PARAMS_FIXED_SIZE)
static int join_network(char* ssid, uint32 wsec)
{
	int ret = 0;
	wlc_ssid_t ssid_t;
	char associated_bssid[6];
	int auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;
	int i, j;

	TUTRACE((TUTRACE_INFO, "Entered: join_network. ssid=[%s] wsec=%d\n", ssid, wsec));

	printf("Joining network %s - %d\n", ssid, wsec);

	/*
	 * If wep bit is on,
	 * pick any WPA encryption type to allow association.
	 * Registration traffic itself will be done in clear (eapol).
	*/
	if (wsec)
		wsec = 2; /* TKIP */
	ssid_t.SSID_len = strlen(ssid);
	strncpy((char *)ssid_t.SSID, ssid, ssid_t.SSID_len);

	/* set infrastructure mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_INFRA,
		(const char *)&infra, sizeof(int))) < 0)
		return ret;

	/* set authentication mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_AUTH,
		(const char *)&auth, sizeof(int))) < 0)
		return ret;

	/* set wsec mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WSEC,
		(const char *)&wsec, sizeof(int))) < 0)
		return ret;

	/* set WPA_auth mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WPA_AUTH,
		(const char *)&wpa_auth, sizeof(wpa_auth))) < 0)
		return ret;

	for (i = 0; i < WLAN_JOIN_ATTEMPTS; i++) {
		TUTRACE((TUTRACE_INFO, "join_network: WLC_SET_SSID %d\n", i + 1));

		if ((ret = wpscli_wlh_ioctl_set(WLC_SET_SSID,
			(const char *)&ssid_t, sizeof(wlc_ssid_t))) < 0) {
			TUTRACE((TUTRACE_INFO,
				"join_network: WLC_SET_SSID ret=%d\n", ret));
			break;
		}

		/* poll for the results until we got BSSID */
		for (j = 0; j < WLAN_POLLING_JOIN_COMPLETE_ATTEMPTS; j++) {

			/* join time */
			wpscli_sleep(WLAN_POLLING_JOIN_COMPLETE_SLEEP);

			ret = wpscli_wlh_ioctl_get(WLC_GET_BSSID, associated_bssid, 6);

			/* exit if associated */
			if (ret == 0)
				goto exit;
		}
	}

exit:
	TUTRACE((TUTRACE_INFO, "join_network: Exiting. ret=%d\n", ret));
	return ret;
}
#endif /* !defined(WL_ASSOC_PARAMS_FIXED_SIZE) || !defined(WL_JOIN_PARAMS_FIXED_SIZE) */


/* Join a BSSID using the WLC_SET_SSID ioctl */
static int join_network_with_bssid_ioctl(const char* ssid, uint32 wsec, const char *bssid,
	int num_chanspec, chanspec_t *chanspec)
{
	int ret = 0;
	int auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;
	char associated_bssid[6];
	int join_params_size;
	wl_join_params_t *join_params;
	wlc_ssid_t *ssid_t;
	wl_assoc_params_t *params_t;
	int i, j;

	TUTRACE((TUTRACE_INFO,
		"Entered: join_network_with_bssid_ioctl. ssid=[%s] wsec=%d #ch=%d\n", ssid, wsec, num_chanspec));

	printf("Joining network %s - wsec %d\n", ssid, wsec);
	printf("BSSID: %02x-%02x-%02x-%02x-%02x-%02x\n",
		(unsigned char)bssid[0], (unsigned char)bssid[1], (unsigned char)bssid[2],
		(unsigned char)bssid[3], (unsigned char)bssid[4], (unsigned char)bssid[5]);

	join_params_size = WL_JOIN_PARAMS_FIXED_SIZE + num_chanspec * sizeof(chanspec_t);
	if ((join_params = malloc(join_params_size)) == NULL) {
		TUTRACE((TUTRACE_INFO, "Exit: join_network_with_bssid_ioctl: malloc failed"));
		return -1;
	}
	memset(join_params, 0, join_params_size);
	ssid_t = &join_params->ssid;
	params_t = &join_params->params;

	/*
	 * If wep bit is on,
	 * pick any WPA encryption type to allow association.
	 * Registration traffic itself will be done in clear (eapol).
	*/
	if (wsec)
		wsec = 2; /* TKIP */

	/* ssid */
	ssid_t->SSID_len = strlen(ssid);
	strncpy((char *)ssid_t->SSID, ssid, ssid_t->SSID_len);

	/* bssid (if any) */
	if (bssid)
		memcpy(&params_t->bssid, bssid, ETHER_ADDR_LEN);
	else
		memcpy(&params_t->bssid, &ether_bcast, ETHER_ADDR_LEN);

	/* channel spec */
	params_t->chanspec_num = num_chanspec;
	for (i = 0; i < params_t->chanspec_num; i++) {
		params_t->chanspec_list[i] = chanspec[i];
	}

	/* set infrastructure mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_INFRA,
		(const char *)&infra, sizeof(int))) < 0)
		goto exit;

	/* set authentication mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_AUTH,
		(const char *)&auth, sizeof(int))) < 0)
		goto exit;

	/* set wsec mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WSEC,
		(const char *)&wsec, sizeof(int))) < 0)
		goto exit;

	/* set WPA_auth mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WPA_AUTH,
		(const char *)&wpa_auth, sizeof(wpa_auth))) < 0)
		goto exit;

	/* set ssid */
	for (i = 0; i < WLAN_JOIN_ATTEMPTS; i++) {
		TUTRACE((TUTRACE_INFO, "join_network_with_bssid_ioctl: WLC_SET_SSID %d\n", i + 1));

		if ((ret = wpscli_wlh_ioctl_set(WLC_SET_SSID,
			(const char *)join_params, join_params_size)) < 0) {
			TUTRACE((TUTRACE_INFO,
				"join_network_with_bssid_ioctl: WLC_SET_SSID ret=%d\n", ret));
			goto exit;
		}

		/* join scan time */
		TUTRACE((TUTRACE_INFO,
			"join_network_with_bssid_ioctl: sleep %d ms\n", 40 * num_chanspec));
		wpscli_sleep(40 * num_chanspec);

		/* poll for the results until we got BSSID */
		for (j = 0; j < WLAN_POLLING_JOIN_COMPLETE_ATTEMPTS; j++) {

			/* join time */
			wpscli_sleep(100);

			ret = wpscli_wlh_ioctl_get(WLC_GET_BSSID, associated_bssid, 6);

			/* exit if associated */
			if (ret == 0)
				goto exit;
		}
	}

exit:
	TUTRACE((TUTRACE_INFO, "Exit: join_network_with_bssid_ioctl: ret=%d\n", ret));
	free(join_params);
	return ret;
}

/* Applies security settings and join a BSSID using a passive join scan.
 * First tries using the "join" iovar.  If that is unsupported by the driver
 * then use the WLC_SET_SSID ioctl.
 */
static int join_network_with_bssid(const char* ssid, uint32 wsec, const char *bssid,
	int num_chanspec, chanspec_t *chanspec)
{
#ifdef WL_EXTJOIN_PARAMS_FIXED_SIZE    /* if driver has "join" iovar */
	int ret = 0;
	int auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;
	char associated_bssid[6];
	int join_params_size;
	wl_extjoin_params_t *join_params;
	wlc_ssid_t *ssid_t;
	wl_join_scan_params_t *scan_t;
	wl_join_assoc_params_t *params_t;
	int i, j;
	int join_scan_time;

	TUTRACE((TUTRACE_INFO,
		"Entered: join_network_with_bssid. ssid=[%s] wsec=%d #ch=%d\n", ssid, wsec, num_chanspec));

	printf("Joining network %s - wsec %d (passive scan)\n", ssid, wsec);
	printf("BSSID: %02x-%02x-%02x-%02x-%02x-%02x\n",
		(unsigned char)bssid[0], (unsigned char)bssid[1], (unsigned char)bssid[2],
		(unsigned char)bssid[3], (unsigned char)bssid[4], (unsigned char)bssid[5]);
	printf("chanspec[%d] =", num_chanspec);
	for (i = 0; i < num_chanspec; i++)
		printf(" 0x%04x", chanspec[i]);
	printf("\n");

	join_params_size = WL_EXTJOIN_PARAMS_FIXED_SIZE + num_chanspec * sizeof(chanspec_t);
	if ((join_params = malloc(join_params_size)) == NULL) {
		TUTRACE((TUTRACE_INFO, "Exit: join_network_with_bssid: malloc failed"));
		return -1;
	}
	memset(join_params, 0, join_params_size);
	ssid_t = &join_params->ssid;
	scan_t = &join_params->scan;
	params_t = &join_params->assoc;

	/*
	 * If wep bit is on,
	 * pick any WPA encryption type to allow association.
	 * Registration traffic itself will be done in clear (eapol).
	*/
	if (wsec)
		wsec = 2; /* TKIP */

	/* ssid */
	ssid_t->SSID_len = strlen(ssid);
	strncpy((char *)ssid_t->SSID, ssid, ssid_t->SSID_len);

	/* join scan params */
	scan_t->scan_type = 1;
	scan_t->nprobes = -1;
	scan_t->active_time = -1;
	if (num_chanspec == 1)
		scan_t->passive_time = WLAN_JOIN_SCAN_PASSIVE_TIME_LONG;
	else
		scan_t->passive_time = WLAN_JOIN_SCAN_PASSIVE_TIME;
	scan_t->home_time = -1;
	join_scan_time = num_chanspec *
		(scan_t->passive_time + WLAN_JOIN_SCAN_DEFAULT_ACTIVE_TIME);

	/* bssid (if any) */
	if (bssid)
		memcpy(&params_t->bssid, bssid, ETHER_ADDR_LEN);
	else
		memcpy(&params_t->bssid, &ether_bcast, ETHER_ADDR_LEN);

	/* channel spec */
	params_t->chanspec_num = num_chanspec;
	for (i = 0; i < params_t->chanspec_num; i++) {
		params_t->chanspec_list[i] = chanspec[i];
	}

	/* set infrastructure mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_INFRA,
		(const char *)&infra, sizeof(int))) < 0)
		goto exit;

	/* set authentication mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_AUTH,
		(const char *)&auth, sizeof(int))) < 0)
		goto exit;

	/* set wsec mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WSEC,
		(const char *)&wsec, sizeof(int))) < 0)
		goto exit;

	/* set WPA_auth mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WPA_AUTH,
		(const char *)&wpa_auth, sizeof(wpa_auth))) < 0)
		goto exit;

	/* do join */
	for (i = 0; i < WLAN_JOIN_ATTEMPTS; i++) {

		/* Start the join */
		TUTRACE((TUTRACE_INFO, "join_network_with_bssid: join iovar %d\n", i + 1));
		if (!wpscli_iovar_set("join", join_params, join_params_size)) {
			TUTRACE((TUTRACE_INFO, "join_network_with_bssid: 'join' iovar ret=%d\n",
				brcm_wpscli_ioctl_err));
			/* If the "join" iovar is unsupported by the driver
			 *     Retry the join using the WLC_SET_SSID ioctl.
			 */
			if (brcm_wpscli_ioctl_err == BCME_UNSUPPORTED) {
				return join_network_with_bssid_ioctl(ssid, wsec, bssid,
					num_chanspec, chanspec);
			}
			goto exit;
		}

		/* wait for the join scan time */
		TUTRACE((TUTRACE_INFO,
			"join_network_with_bssid: sleep %d ms\n", join_scan_time));
		wpscli_sleep(join_scan_time);

		/* poll for the results until we got BSSID */
		for (j = 0; j < WLAN_POLLING_JOIN_COMPLETE_ATTEMPTS; j++) {

			/* join time */
			wpscli_sleep(WLAN_POLLING_JOIN_COMPLETE_SLEEP);

			ret = wpscli_wlh_ioctl_get(WLC_GET_BSSID, associated_bssid, 6);

			/* exit if associated */
			if (ret == 0)
				goto exit;
		}
	}

exit:
	TUTRACE((TUTRACE_INFO, "Exit: join_network_with_bssid: ret=%d\n", ret));
	free(join_params);
	return ret;
#else /* no "join" iovar */
	return join_network_with_bssid_ioctl(ssid, wsec, bssid, num_channels,
		channels);
#endif /* WL_EXTJOIN_PARAMS_FIXED_SIZE */
}

/* Applies security settings and join a BSSID using an active join scan.
 * First tries using the "join" iovar.  If that is unsupported by the driver
 * then use the WLC_SET_SSID ioctl.
 */
/* TODO: factor out common code between join_network_with_bssid() and
 * join_network_with_bssid_active()
 */
static int join_network_with_bssid_active(const char* ssid, uint32 wsec,
	const char *bssid, int num_chanspec, chanspec_t *chanspec)
{
#ifdef WL_EXTJOIN_PARAMS_FIXED_SIZE    /* if driver has "join" iovar */
	int ret = 0;
	int auth = 0, infra = 1;
	int wpa_auth = WPA_AUTH_DISABLED;
	char associated_bssid[6];
	int join_params_size;
	wl_extjoin_params_t *join_params;
	wlc_ssid_t *ssid_t;
	wl_join_scan_params_t *scan_t;
	wl_join_assoc_params_t *params_t;
	int i, j;
	int join_scan_time;

	TUTRACE((TUTRACE_INFO,
		"Entered: join_network_with_bssid_active. ssid=[%s] wsec=%d #ch=%d\n", ssid, wsec, num_chanspec));

	printf("Joining network %s - wsec %d (active scan)\n", ssid, wsec);
	printf("BSSID: %02x-%02x-%02x-%02x-%02x-%02x\n",
		(unsigned char)bssid[0], (unsigned char)bssid[1], (unsigned char)bssid[2],
		(unsigned char)bssid[3], (unsigned char)bssid[4], (unsigned char)bssid[5]);
	printf("chanspec[%d] =", num_chanspec);
	for (i = 0; i < num_chanspec; i++)
		printf(" 0x%04x", chanspec[i]);
	printf("\n");

	join_params_size = WL_EXTJOIN_PARAMS_FIXED_SIZE + num_chanspec * sizeof(chanspec_t);
	if ((join_params = malloc(join_params_size)) == NULL) {
		TUTRACE((TUTRACE_INFO, "Exit: join_network_with_bssid_active: malloc failed"));
		return -1;
	}
	memset(join_params, 0, join_params_size);
	ssid_t = &join_params->ssid;
	scan_t = &join_params->scan;
	params_t = &join_params->assoc;

	/*
	 * If wep bit is on,
	 * pick any WPA encryption type to allow association.
	 * Registration traffic itself will be done in clear (eapol).
	*/
	if (wsec)
		wsec = 2; /* TKIP */

	/* ssid */
	ssid_t->SSID_len = strlen(ssid);
	strncpy((char *)ssid_t->SSID, ssid, ssid_t->SSID_len);

	/* join scan params */
	scan_t->scan_type = DOT11_SCANTYPE_ACTIVE;
	scan_t->nprobes = -1;
	scan_t->active_time = WLAN_JOIN_SCAN_ACTIVE_TIME;
	scan_t->home_time = -1;
	join_scan_time = num_chanspec *
		(scan_t->active_time + WLAN_JOIN_SCAN_DEFAULT_ACTIVE_TIME);

	/* bssid (if any) */
	if (bssid)
		memcpy(&params_t->bssid, bssid, ETHER_ADDR_LEN);
	else
		memcpy(&params_t->bssid, &ether_bcast, ETHER_ADDR_LEN);

	/* channel spec */
	params_t->chanspec_num = num_chanspec;
	for (i = 0; i < params_t->chanspec_num; i++) {
		params_t->chanspec_list[i] = chanspec[i];
	}

	/* set infrastructure mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_INFRA,
		(const char *)&infra, sizeof(int))) < 0)
		goto exit;

	/* set authentication mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_AUTH,
		(const char *)&auth, sizeof(int))) < 0)
		goto exit;

	/* set wsec mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WSEC,
		(const char *)&wsec, sizeof(int))) < 0)
		goto exit;

	/* set WPA_auth mode */
	if ((ret = wpscli_wlh_ioctl_set(WLC_SET_WPA_AUTH,
		(const char *)&wpa_auth, sizeof(wpa_auth))) < 0)
		goto exit;

	/* do join */
	for (i = 0; i < WLAN_JOIN_ATTEMPTS; i++) {

		/* Start the join */
		TUTRACE((TUTRACE_INFO, "join_network_with_bssid_active: join iovar %d\n", i + 1));
		if (!wpscli_iovar_set("join", join_params, join_params_size)) {
			TUTRACE((TUTRACE_INFO, "join_network_with_bssid_active: 'join' iovar ret=%d\n",
				brcm_wpscli_ioctl_err));
			/* If the "join" iovar is unsupported by the driver
			 *     Retry the join using the WLC_SET_SSID ioctl.
			 */
			if (brcm_wpscli_ioctl_err == BCME_UNSUPPORTED) {
				return join_network_with_bssid_ioctl(ssid, wsec, bssid,
					num_chanspec, chanspec);
			}
			goto exit;
		}

		/* wait for the join scan time */
		TUTRACE((TUTRACE_INFO,
			"join_network_with_bssid_active: sleep %d ms\n", join_scan_time));
		wpscli_sleep(join_scan_time);

		/* poll for the results until we got BSSID */
		for (j = 0; j < WLAN_POLLING_JOIN_COMPLETE_ATTEMPTS; j++) {

			/* join time */
			wpscli_sleep(WLAN_POLLING_JOIN_COMPLETE_SLEEP);

			ret = wpscli_wlh_ioctl_get(WLC_GET_BSSID, associated_bssid, 6);

			/* exit if associated */
			if (ret == 0)
				goto exit;
		}
	}

exit:
	TUTRACE((TUTRACE_INFO, "Exit: join_network_with_bssid_active: ret=%d\n", ret));
	free(join_params);
	return ret;
#else /* no "join" iovar */
	return join_network_with_bssid_ioctl(ssid, wsec, bssid, num_chanspec,
		chanspec);
#endif /* WL_EXTJOIN_PARAMS_FIXED_SIZE */
}

static int leave_network(void)
{
	return wpscli_wlh_ioctl_set(WLC_DISASSOC, NULL, 0);
}
