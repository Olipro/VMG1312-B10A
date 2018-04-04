/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_api.c 291340 2011-10-21 23:24:31Z $
 *
 * Description: Implement APIs generic to both SoftAP and STA sides 
 *
 */
#include <stdlib.h>
#include <string.h>
#include "wpscli_osl.h"
#include "wpscli_api.h"
#include <typedefs.h>
#include <wpserror.h>
#include "wpscli_common.h"

brcm_wpscli_request_ctx g_ctxWpscliReq = NULL;  // WPS library caller request context
brcm_wpscli_status_cb g_cbWpscliStatus = NULL;		// Callback function
int g_bRequestAbort;

// Global wpscli OSI layer context
wpscli_osi_context g_ctxWpscliOsi;

extern brcm_wpscli_status wpscli_sta_open(void);
extern brcm_wpscli_status wpscli_softap_open(void);
extern brcm_wpscli_status wpscli_softap_close_wps(void);
extern uint32 wps_generatePin(char *c_devPwd, int buf_len, IN bool b_display);
extern bool wps_validateChecksum(IN unsigned long int PIN);
extern brcm_wpscli_status wpscli_softap_set_wps_version_1(bool b_use_wps_ver_1);

brcm_wpscli_status brcm_wpscli_open(const char *dev_if_name,
								    brcm_wpscli_role cli_role, 
								    brcm_wpscli_request_ctx request_ctx, 
								    brcm_wpscli_status_cb status_cb)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	uint8 adapter_mac[6];

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_open: Entered (dev_if_name=%s).\n",
			dev_if_name == (const char *)NULL ? "" : dev_if_name));

	if(dev_if_name == NULL)
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_open: Exiting. Invalid NULL device interface name passed in.\n"));

	// Initialize variables
	g_ctxWpscliReq = request_ctx;
	g_cbWpscliStatus = status_cb;
	g_bRequestAbort = FALSE;
	g_ctxWpscliOsi.role = cli_role;

	// Initialize wl handler. Call this first as it will open adapter which is required for other initilization
	status = wpscli_wlh_open(dev_if_name, cli_role == BRCM_WPSCLI_ROLE_SOFTAP);

	if(status != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_open: Exiting. Failed to osl open is failed.\n"));
		return status;
	}

	// Fill bssid or sta_mac
	wpscli_wlh_get_adapter_mac(adapter_mac);  // Get client adapter mac address

	if(cli_role == BRCM_WPSCLI_ROLE_STA)
		memcpy(g_ctxWpscliOsi.sta_mac, adapter_mac, 6);
	else
		memcpy(g_ctxWpscliOsi.bssid, adapter_mac, 6);

	if(cli_role == BRCM_WPSCLI_ROLE_STA)
		wpscli_sta_open();
	else
		wpscli_softap_open();

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_open: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status brcm_wpscli_close()
{
	// Close wlan osl
	wpscli_wlan_close();

	// Close wl handler osl
	wpscli_wlh_close();

	g_ctxWpscliReq = NULL;
	g_cbWpscliStatus = NULL;

	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status brcm_wpscli_abort()
{
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_abort: requesting abort\n"));
	g_bRequestAbort = TRUE;

	return WPS_STATUS_SUCCESS;
}

void wpscli_update_status(brcm_wpscli_status status, brcm_wpscli_status_cb_data cb_data)
{
	if(g_cbWpscliStatus == NULL)
		return;

	g_cbWpscliStatus(g_ctxWpscliReq, status, cb_data);
}

void print_buf(unsigned char *buff, int buflen)
{
	int i;
	printf("\n print buf : \n");
	for (i = 0; i < buflen; i++) {
		printf("%02X ", buff[i]);
		if (!((i+1)%16))
			printf("\n");
	}
	printf("\n");
}

brcm_wpscli_status brcm_wpscli_redirect_logs(brcm_wpscli_log_output_fn fn)
{
	wps_set_traceMsg_output_fn(fn);
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status brcm_wpscli_generate_pin(char *pin, int bufLen)
{
	brcm_wpscli_status status;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_generate_pin: Entered.\n"));

	if(pin == NULL) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_generate_pin: Exiting. Invalid NULL pin parameter passed in.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}
	
	if(wps_generatePin(pin, bufLen, FALSE) == WPS_SUCCESS)
		status = WPS_STATUS_SUCCESS;
	else {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_generate_pin: Failed to generate a valid WPS pin number.\n"));
		status = WPS_STATUS_SYSTEM_ERR;
	}

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_generate_pin: Exiting.\n"));
	return status;
}

brcm_wpscli_status brcm_wpscli_validate_pin(const char *pin)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	unsigned int len;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_validate_pin: Entered.\n"));

	if (pin == NULL) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_validate_pin: Exiting. Invalid NULL parameter passed in!\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	// Validate length of PIN which should be either 4 or 8 digits
	len = strlen(pin);
	if (len == 4 || len == 8) {
		unsigned long int pin_l;

		pin_l = atol(pin);
		if (len == 8) {
			// Validate checksum if the PIN is 8 digits
			if (!wps_validateChecksum(pin_l)) {
				TUTRACE((TUTRACE_ERR, "brcm_wpscli_validate_pin: Invalid checksum digit!\n", len));
				status = WPS_STATUS_INVALID_PIN;
			}
		}
	}
	else {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_validate_pin: Invalid PIN length %d!\n", len));
		status = WPS_STATUS_INVALID_PIN;
	}

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_validate_pin: Exiting. status=%d\n", status));
	return status;
}

brcm_wpscli_status brcm_wpscli_switch_wps_version(int is_use_version1)
{
	// Set the value into both Server and Client
	wpscli_softap_set_wps_version_1(is_use_version1);

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_switch_wps_version: is use version 1 =%d\n", is_use_version1));
	return WPS_STATUS_SUCCESS;
}
