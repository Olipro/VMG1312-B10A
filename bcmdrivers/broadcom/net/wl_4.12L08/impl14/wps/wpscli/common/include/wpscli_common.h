/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_common.h 289441 2011-10-12 22:16:41Z $
 *
 * Description: Common OSI definition
 *
 */
#ifndef  __WPSCLI_SM_H__
#define  __WPSCLI_SM_H__

#include <time.h>
#include "wpstypes.h"
#include <wps_wl.h>
#include <tutrace.h>
#include <typedefs.h>

// Constant definition
#define WPSCLI_GENERIC_POLL_TIMEOUT		1000  	// in milli-seconds
#define WPSCLI_MONITORING_POLL_TIMEOUT	100		// in milli-seconds
#define WPS_DEFAULT_PIN				"00000000"

// define keyMgmt string values inherited from enrollee engine
#define KEY_MGMT_OPEN				"OPEN"
#define KEY_MGMT_SHARED				"SHARED"
#define KEY_MGMT_WPAPSK				"WPA-PSK"
#define KEY_MGMT_WPA2PSK			"WPA2-PSK"
#define KEY_MGMT_WPAPSK_WPA2PSK		"WPA-PSK WPA2-PSK"

typedef struct td_wpscli_osi_context {
	uint8 sta_mac[6];				// STA's mac address
	uint8 bssid[6];					// AP's bssid
	uint8 sta_type;					// Enrollee or registrar
	char ssid[SIZE_SSID_LENGTH+1];	// ssid string with string terminator
	brcm_wpscli_wps_mode mode;
	brcm_wpscli_role role;			// SoftAP or Station
	brcm_wpscli_pwd_type pwd_type;
	char pin[10]; /* this device's pin for enrollee mode  */
	char peer_pin[10]; /* peer's pin for registrar mode  */
	brcm_wpscli_nw_settings *nw_conf;
	uint8 window_open;
	uint8 eap_state;					// Current state of eap state machine registration
	long start_time; 
	long time_out;				// WPS timout period in seconds
	uint8 authorizedMacs[6 * 5];			//WSC 2.0
	uint32 authorizedMacs_len;			//WSC 2.0
} wpscli_osi_context;

extern brcm_wpscli_request_ctx g_ctxWpscliReq;  // WPS library caller request context
extern brcm_wpscli_status_cb g_cbWpscliStatus;  // WPS caller client's callback function

extern wpscli_osi_context g_ctxWpscliOsi;
extern int g_bRequestAbort;
extern BOOL b_wps_version2;

extern void print_buf(unsigned char *buff, int buflen);

#endif  //  __WPSCLI_SM_H__
