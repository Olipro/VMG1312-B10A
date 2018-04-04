/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_softap.c 296937 2011-11-17 06:35:35Z $
 *
 * Description: Implement SoftAP side APIs in wpscli_api.h
 *
 */
// Include wpscli library OSL header file
#include "wpscli_osl.h"

// Include common OSI header file
#include "wpscli_common.h"

// Include WPS core common header files
// This set of wps core common #include header files need be in this order because of their inter-dependency
#include <typedefs.h>
#include <wpstypes.h>
#include <tutrace.h>
#include <wpstlvbase.h>
#include <wps_devinfo.h>
#include <bcmcrypto/bn.h>
#include <bcmcrypto/dh.h>
#include <sminfo.h>
#include <wps_devinfo.h>
#include <reg_prototlv.h>
#include <reg_proto.h>
#include <proto/eapol.h>
#include <wps_wl.h>
#include <statemachine.h>
#include <wpsapi.h>
#include <wps_ap.h>
#include <wps_apapi.h>
#if defined(UNDER_CE)
#include <proto/eap.h>
#else
#include <eap.h>
#endif
#include <ap_eap_sm.h>
#include <ap_ssr.h>
#include <wps_enr_osl.h>
#include <ethernet.h>

static brcm_wpscli_nw_settings g_nw_settings;

// Variable length of AP device information in WPS IE fields. Try to shortern
// string length as device driver needs WPS IE size to be as small as possible
#define AP_DEVINFO_DEVICENAME		"Broadcom"
#define AP_DEVINFO_MANUFACTURER		"Broadcom"
#define AP_DEVINFO_MODEL_NAME		"SoftAP"
#define AP_DEVINFO_MODEL_NO			"0"
#define AP_DEVINFO_SERIAL_NO		"0"

// External function declarations
extern BOOL wpscli_set_wl_prov_svc_ie(unsigned char *p_data, int length, unsigned int cmdtype);
extern BOOL wpscli_del_wl_prov_svc_ie(unsigned int cmdtype);
//extern BOOL wpscli_set_wps_ie(unsigned char *p_data, int length, unsigned int cmdtype);
extern BOOL wpscli_build_wps_ie(DevInfo *devinfo, uint8 pbc, uint32 pktflag, uint8 *buf, int *buflen);
extern BOOL wpscli_add_wps_ie(DevInfo *devinfo, uint8 pbc, uint32 pktflag);
extern BOOL wpscli_del_wps_ie(unsigned int frametype);
extern void wpscli_update_status(brcm_wpscli_status status, brcm_wpscli_status_cb_data cb_data);
extern void upnp_device_uuid(unsigned char *uuid);

// Local function declarations
static brcm_wpscli_status wpscli_softap_add_wps_ie(void);
static int wpscli_softap_readConfigure(wpsap_wksp_t *bcmwps, DevInfo *ap_devinfo, WpsEnrCred *credential);
static void wpscli_softap_cleanup_wps(void);
static void wpscli_softap_session_end(int opcode);
static brcm_wpscli_status wpscli_convert_nw_settings(const brcm_wpscli_nw_settings *nw_settings, WpsEnrCred *wps_cred);
static brcm_wpscli_status wpscli_softap_sm_init(WpsEnrCred *pNwCred, WPS_SCMODE ap_wps_mode, const uint8 *sta_mac);
static brcm_wpscli_status wpscli_softap_init_wps(brcm_wpscli_wps_mode wps_mode,
	 										     brcm_wpscli_pwd_type pwd_type,
												 const char *pin_code,
												 const brcm_wpscli_nw_settings *nw_settings,
												 uint32 time_out,
												 uint8 *authorizedMacs,
												 uint32 authorizedMacs_len);
static int wpscli_softap_process_eap_msg(char *eapol_msg, uint32 len);
static int wpscli_softap_process_sta_eapol_start_msg(char *sta_buf, uint8 *sta_mac);
static void wpscli_softap_bcmwps_add_pktcnt(wpsap_wksp_t *bcmwps);
static void wps_pb_update_pushtime(char *mac);
static int wps_pb_check_pushtime(unsigned long time);
static void wps_pb_remove_sta_from_mon_checker(const uint8 *mac);

// Local variables
static WPSAPI_T *g_mc = NULL;
static wpsap_wksp_t *g_bcmwps = NULL;
//static char gDeviceName[SIZE_32_BYTES+1];

//static uint16 gConfigMethods = WPS_CONFMET_LABEL | WPS_CONFMET_PBC;
#ifdef BCM_WPS_2_0
static uint16 gConfigMethodsV2 = WPS_CONFMET_LABEL | WPS_CONFMET_PBC;
#endif
static time_t g_tStartTime;
static BOOL g_bWpsEnabled = FALSE;
static BOOL g_bWpsSessionOpen;
static DevInfo softap_devinfo;

// eap states
#define IDLE 0
#define REQ_ID_SENT 1
#define REGISTRATION_STARTED 2


// Constant definitions
const char ZERO_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
const char *WpsEapEnrIdentity =  "WFA-SimpleConfig-Enrollee-1-0";
const char *WpsEapRegIdentity =  "WFA-SimpleConfig-Registrar-1-0";

enum { WPS_EAP_ID_ENROLLEE = 0,	WPS_EAP_ID_REGISTRAR, WPS_EAP_ID_NONE };

// PBC Monitoring Checker
#define PBC_OVERLAP_CNT			2
#define PBC_MONITOR_TIME		120		/* in seconds */

typedef struct {
	unsigned char	mac[6];
	unsigned int	last_time;
} PBC_STA_INFO;


static PBC_STA_INFO pbc_info[PBC_OVERLAP_CNT];

brcm_wpscli_status wpscli_softap_set_wps_version_1(bool b_use_wps_ver_1)
{
	b_wps_version2 = !b_use_wps_ver_1;
	TUTRACE((TUTRACE_INFO, "wpscli_softap_set_wps_version_1: b_wps_version2 is %d.\n",b_wps_version2));
	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_softap_open()
{
	TUTRACE((TUTRACE_INFO, "wpscli_softap_open: Entered.\n"));

	TUTRACE((TUTRACE_INFO, "wpscli_softap_open: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
};

brcm_wpscli_status wpscli_softap_init(const char *bssid)
{
	memcpy(g_ctxWpscliOsi.bssid, bssid, SIZE_6_BYTES);
	return wpscli_softap_construct_def_devinfo();
}

brcm_wpscli_status wpscli_softap_construct_def_devinfo()
{
	memset((char *)(&softap_devinfo), 0, sizeof(softap_devinfo));

	/* Version */
	softap_devinfo.version = 0x10;

	/* generate UUID base on the MAC addr */
	upnp_device_uuid(softap_devinfo.uuid);
	memcpy(softap_devinfo.uuid + 10, g_ctxWpscliOsi.bssid, SIZE_6_BYTES);

	/* Mac address */
	memcpy(softap_devinfo.macAddr, g_ctxWpscliOsi.bssid, SIZE_6_BYTES);

	/* Device Name */
	strcpy(softap_devinfo.deviceName, AP_DEVINFO_MANUFACTURER);

	/* Device category */
	softap_devinfo.primDeviceCategory = 6;

	/* Device OUI */
	softap_devinfo.primDeviceOui = 0x0050F204;

	/* Device sub category */
	softap_devinfo.primDeviceSubCategory = 1;

	/* WSC 2.0, WPS-PSK and SHARED are deprecated.
	 * When both the Registrar and the Enrollee are using protocol version 2.0
	 * or newer, this variable can use the value 0x0022 to indicate mixed mode
	 * operation (both WPA-Personal and WPA2-Personal enabled)
	 * NOTE: BCMWPA2 compile option MUST enabled
	 */
	if (b_wps_version2) {
		softap_devinfo.authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_WPA2PSK);
	} else {
		softap_devinfo.authTypeFlags = (uint16)(WPS_AUTHTYPE_OPEN | WPS_AUTHTYPE_WPAPSK |
			WPS_AUTHTYPE_SHARED | WPS_AUTHTYPE_WPA2PSK);
	}

	/* ENCR_TYPE_FLAGS */
	/*
	 * WSC 2.0, deprecated WEP. TKIP can only be advertised on the AP when
	 * Mixed Mode is enabled (Encryption Type is 0x000c)
	 */
	if (b_wps_version2) {
		softap_devinfo.encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_TKIP |
			WPS_ENCRTYPE_AES);
	} else {
		softap_devinfo.encrTypeFlags = (uint16)(WPS_ENCRTYPE_NONE | WPS_ENCRTYPE_WEP |
			WPS_ENCRTYPE_TKIP | WPS_ENCRTYPE_AES);
	}

	/* CONN_TYPE_FLAGS */
	softap_devinfo.connTypeFlags = 0x01;

	/* CONFIG_METHODS */
	/*
	 * WSC 2.0, Default standalone STA.
	 * 0x0004 Label | 0x0280 Virtual Push Button |
	 * 0x2008 Virtual Display PIN
	 */
	if (b_wps_version2) {
		softap_devinfo.configMethods = (WPS_CONFMET_LABEL | WPS_CONFMET_PBC |
			WPS_CONFMET_VIRT_PBC);
	} else {
		softap_devinfo.configMethods = WPS_CONFMET_LABEL | WPS_CONFMET_PBC;
	}

	/* Configure Mode */
	softap_devinfo.scState = WPS_SCSTATE_CONFIGURED;

	/* not selected by default */
	softap_devinfo.selRegistrar = FALSE;
	/* MANUFACTURER */
	strcpy(softap_devinfo.manufacturer, AP_DEVINFO_MANUFACTURER);

	/* MODEL_NAME */
	strcpy(softap_devinfo.modelName, AP_DEVINFO_MODEL_NAME);

	/* MODEL_NUMBER */
	strcpy(softap_devinfo.modelNumber, AP_DEVINFO_MODEL_NO);

	/* SERIAL_NUMBER */
	strcpy(softap_devinfo.serialNumber, AP_DEVINFO_SERIAL_NO);

	/* RF Band */
	softap_devinfo.rfBand = 1;

	/* OS_VER */
	softap_devinfo.osVersion = 0x80000000;

	/* FEATURE_ID */
	softap_devinfo.featureId = 0x80000000;

	softap_devinfo.b_ap = true;

	/* SSID */
	strcpy(softap_devinfo.ssid, "vista_softap");

	/* KeyMgmt */
	strcpy(softap_devinfo.keyMgmt, KEY_MGMT_OPEN);

	/* Auth */
	softap_devinfo.auth = 0;

	/* Wep */
	softap_devinfo.wep = 0;

	/* Crypto */
	softap_devinfo.crypto = WPS_ENCRTYPE_NONE;

	/* Request Device category, default set to zero */
	softap_devinfo.primDeviceCategory = 0;

	/* Request Device OUI */
	softap_devinfo.primDeviceOui = 0x0050F204;

	/* Request Device sub category */
	softap_devinfo.primDeviceSubCategory = 0;

	/* WSC 2.0 */
	if (b_wps_version2) {
		/* Version 2 */
		softap_devinfo.version2 = 0x20;

		/* Setting Delay Time */
		softap_devinfo.settingsDelayTime = 0x10;

		/* Request to Enroll */
		softap_devinfo.b_reqToEnroll = FALSE;

		/* Network Key shareable */
		softap_devinfo.b_nwKeyShareable = FALSE;

		/* AuthorizedMACs */
		softap_devinfo.authorizedMacs_len = 0;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_softap_construct_def_devinfo: cfMethods=0x%0x, devPwdId=0x%0x.\n",
		softap_devinfo.configMethods, softap_devinfo.devPwdId));

	return WPS_STATUS_SUCCESS;
}

brcm_wpscli_status wpscli_softap_advertise_no_8021x()
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	unsigned char ie_no8021x[1] = { 0x00 };

	TUTRACE((TUTRACE_INFO, "wpscli_softap_advertise_no_8021x: Entered.\n"));

	wpscli_set_wl_prov_svc_ie(ie_no8021x, 1, WPS_IE_TYPE_SET_BEACON_IE);
	wpscli_set_wl_prov_svc_ie(ie_no8021x, 1, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);

	TUTRACE((TUTRACE_INFO, "wpscli_softap_advertise_no_8021x: Exiting.\n"));
	return status;
}

brcm_wpscli_status wpscli_softap_cleanup_wl_prov_svc()
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "wpscli_softap_cleanup_wl_prov_svc: Entered.\n"));

	wpscli_del_wl_prov_svc_ie(WPS_IE_TYPE_SET_BEACON_IE);
	wpscli_del_wl_prov_svc_ie(WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);

	TUTRACE((TUTRACE_INFO, "wpscli_softap_cleanup_wl_prov_svc: Exiting.\n"));
	return status;
}

// Enable WPS on SoftAP
brcm_wpscli_status brcm_wpscli_softap_enable_wps(const char *deviceName, const uint16 configMethods,
	uint8 *authorizedMacs, uint32 authorizedMacs_len)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_enable_wps: Entered (devName=%s, configMethods=0x%x)\n", 
		deviceName, configMethods));

	//strncpy(gDeviceName, deviceName, sizeof(gDeviceName));
	//gConfigMethods = configMethods;
	strncpy(softap_devinfo.deviceName, deviceName, sizeof(softap_devinfo.deviceName));
	softap_devinfo.configMethods = configMethods;

	softap_devinfo.authorizedMacs_len = 0;
		/* clear pbc */
		softap_devinfo.devPwdId = WPS_DEVICEPWDID_DEFAULT;

	wpscli_softap_advertise_no_8021x();

	// Add and broadcast WPS IE
	status = wpscli_softap_add_wps_ie();
	if(status != WPS_STATUS_SUCCESS) {
		 TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_enable_wps: Failed to set WPS IE.\n"));
	}

	g_bWpsEnabled = TRUE;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_enable_wps: Exiting. status=%d, "
		"softap_devinfo.deviceName=%s\n", status, softap_devinfo.deviceName));
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_enable_wps: Start packet dispatchers\n") );
	wpscli_pktdisp_open((const uint8 *)ZERO_MAC);
	return status;
}

// Disable WPS on SoftAP
brcm_wpscli_status brcm_wpscli_softap_disable_wps()
{
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_disable_wps: Entered.\n"));

	if (g_ctxWpscliOsi.window_open ) {
		brcm_wpscli_softap_close_session();
	}

	if(g_bWpsEnabled) {
		wpscli_softap_cleanup_wl_prov_svc();

		// Remove wps IEs
		wpscli_softap_cleanup_wps();

		wpscli_pktdisp_close();
		g_bWpsEnabled = FALSE;

	}
	else {
		TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_disable_wps: WPS was not enabled.\n"));
	}

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_disable_wps: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

void wpscli_deinit_wps_engine()
{
	TUTRACE((TUTRACE_INFO, "wpscli_deinit_wps_engine: Entered.\n"));

	// De-initialize wps engine
	if(g_mc) {
		wps_deinit(g_mc);
		g_mc = NULL;
	}
	else {
		TUTRACE((TUTRACE_INFO, "wpscli_deinit_wps_engine: NULL g_mc.\n"));
	}

	// Free g_bcmwps
	if(g_bcmwps) {
		free(g_bcmwps);
		g_bcmwps = NULL;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_deinit_wps_engine: Exiting.\n"));
}

brcm_wpscli_status brcm_wpscli_softap_close_session()
{
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_close_session: Entered.\n"));

	g_bWpsSessionOpen = FALSE;
	g_ctxWpscliOsi.window_open=FALSE;
	g_ctxWpscliOsi.eap_state=IDLE;
	g_bWpsEnabled = TRUE;

	wpscli_deinit_wps_engine();

	if(g_bWpsEnabled) {
		/* clear pbc */
		softap_devinfo.devPwdId = WPS_DEVICEPWDID_DEFAULT;
		/* clear selected registrar */
		softap_devinfo.selRegistrar = FALSE;
		/* clear authorized mac */
		softap_devinfo.authorizedMacs_len = 0;

		TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_close_session: reset softap_devinfo.devPwdId=0x%0x\n",
			softap_devinfo.devPwdId));
		
		// Restore WPS IE as it will be modified during wps negotiation
		wpscli_softap_add_wps_ie();
	}

	g_ctxWpscliOsi.eap_state=IDLE;
	g_ctxWpscliOsi.window_open=FALSE;
	g_bWpsSessionOpen = FALSE;

	memcpy(g_ctxWpscliOsi.sta_mac, ZERO_MAC, 6);

	// Send a status update to indicate the end of a WPS handshake */
	wpscli_update_status(WPS_STATUS_PROTOCOL_END_EXCHANGE, NULL);

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_close_session: Exiting.\n"));
	return WPS_STATUS_SUCCESS;
}

int brcm_wpscli_softap_is_wps_window_open()
{
	return g_bWpsSessionOpen;
}

static uint32 wpscli_get_eap_wps_id(char *buf)
{
	uint32 eap_id = WPS_EAP_ID_NONE;
	eapol_header_t *eapol = (eapol_header_t *)buf;
	eap_header_t *eap = (eap_header_t *)eapol->body;

	TUTRACE((TUTRACE_INFO, "wpscli_get_eap_wps_id: Entered.\n"));

	if (eap->type != EAP_IDENTITY) {
		TUTRACE((TUTRACE_INFO, "wpscli_get_eap_wps_id: Exiting. Not an EAP Idenity message.\n"));
		return WPS_EAP_ID_NONE;
	}

	if(memcmp((char *)eap->data, REGISTRAR_ID_STRING, strlen(REGISTRAR_ID_STRING)) == 0)
		eap_id = WPS_EAP_ID_REGISTRAR;
	else if(memcmp((char *)eap->data, ENROLLEE_ID_STRING, strlen(ENROLLEE_ID_STRING)) == 0)
		eap_id = WPS_EAP_ID_ENROLLEE;
	else
		eap_id = WPS_EAP_ID_NONE;

	TUTRACE((TUTRACE_INFO, "wpscli_get_eap_wps_id: Exiting. eap_id=%d\n", eap_id));
	return eap_id;
}


brcm_wpscli_status brcm_wpscli_softap_set_wps_context(brcm_wpscli_nw_settings *nw_conf, const char* dev_pin,
	uint8 *authorizedMacs, uint32 authorizedMacs_len)
{
  strncpy(g_ctxWpscliOsi.pin, dev_pin, 8);
  g_ctxWpscliOsi.nw_conf = nw_conf;

	/* WSC 2.0, AuthorizedMacs */
	if (b_wps_version2) {
		if (authorizedMacs == NULL || authorizedMacs_len == 0) {
			/* WSC 2.0 r44, add wildcard MAC when authorized mac not specified */
			g_ctxWpscliOsi.authorizedMacs_len = SIZE_MAC_ADDR;
			memcpy(g_ctxWpscliOsi.authorizedMacs, wildcard_authorizedMacs, SIZE_MAC_ADDR);
		}
		else {
			g_ctxWpscliOsi.authorizedMacs_len = authorizedMacs_len;
			memcpy(g_ctxWpscliOsi.authorizedMacs, authorizedMacs, authorizedMacs_len);
		}
	}
	else {
		g_ctxWpscliOsi.authorizedMacs_len = 0;
	}

  return 0;
}

// SoftAP starts WPS registration negotiation to enroll STA device.

brcm_wpscli_status brcm_wpscli_softap_start_wps (
						 brcm_wpscli_wps_mode wps_mode,
						 brcm_wpscli_pwd_type pwd_type,
						 const char *pin_code,
						 brcm_wpscli_nw_settings *nw_settings,
						 uint32 time_out,
						 uint8 *peer_addr,
						 uint8 *authorizedMacs,
						 uint32 authorizedMacs_len)
{
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	// save a global copy
	memcpy((void *)&g_nw_settings, nw_settings, sizeof(g_nw_settings));

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_wps: Entered(ssid=%s).\n", nw_settings->ssid));

	// Reset library context and global veriables
	g_tStartTime = wpscli_current_time();  // Get the start time
	g_ctxWpscliOsi.time_out = time_out;  // Fill time out value

	/* save peer's pin */
	if (pin_code)
	  //memcpy(g_ctxWpscliOsi.peer_pin, pin_code, 8);
	  strcpy(g_ctxWpscliOsi.peer_pin,pin_code);

	g_bRequestAbort = FALSE;
	//strncpy(gDeviceName, nw_settings->ssid, sizeof(gDeviceName));
	strncpy(softap_devinfo.deviceName, nw_settings->ssid, sizeof(softap_devinfo.deviceName));
	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_wps: set softap_devinfo.deviceName=%s "
		"pwd_type=%d\n", softap_devinfo.deviceName, pwd_type));

	// Open packet dispatcher
	//status = wpscli_pktdisp_open((const uint8 *)ZERO_MAC);
	if(status != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_wps: Exiting. Failed to open packet dispatcher\n"));
		return WPS_STATUS_PKTD_INIT_FAIL;
	}

	// Initialize wps session
	if(g_mc)
		wpscli_deinit_wps_engine();

	/* AuthorizedMACs */
	if (b_wps_version2) {
		if (authorizedMacs == NULL || authorizedMacs_len == 0) {
			/* WSC 2.0 r44, add wildcard MAC when authorized mac not specified */
			authorizedMacs_len = SIZE_MAC_ADDR;
			authorizedMacs = (uint8 *)wildcard_authorizedMacs;
		}
	}
	else {
		authorizedMacs = NULL;
		authorizedMacs_len = 0;
	}
	/* just set the IE here. We don't know what mode will be activated yet */


	/*  if in PBC mode, set selRegistrar */
	/*  add the ie. This all that is needed to make the device discoverable
	    in PBC mode.
	 */
	if (pwd_type == BRCM_WPS_PWD_TYPE_PBC)
	  softap_devinfo.devPwdId = WPS_DEVICEPWDID_PUSH_BTN;
	else
	  softap_devinfo.devPwdId = WPS_DEVICEPWDID_DEFAULT;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_wps: set softap_devinfo.devPwdId to 0x%0x\n",
		softap_devinfo.devPwdId));

  	softap_devinfo.selRegistrar = TRUE;
  	wpscli_softap_add_wps_ie();

	status = wpscli_softap_init_wps(wps_mode, pwd_type, pin_code, nw_settings, time_out, authorizedMacs, authorizedMacs_len);
	g_bWpsSessionOpen = TRUE;

	TUTRACE((TUTRACE_INFO, "brcm_wpscli_softap_start_wps: Exiting. status=%d\n", status));
	return status;
}

#ifdef WPSCLI_NO_WPS_LOOP
brcm_wpscli_status brcm_wpscli_softap_process_eapwps(char *buf, uint32 buf_len, uint8 *src_mac,uint8 *peer_addr)
#else
brcm_wpscli_status brcm_wpscli_softap_process_eapwps(uint8 *peer_addr)
#endif
{

 #define RESP_TIMEOUT 1

  uint32 wps_id;
  int retVal = WPS_SUCCESS;

  BOOL window_is_opening = FALSE;

#ifndef WPSCLI_NO_WPS_LOOP
  static char buf[WPS_EAP_DATA_MAX_LENGTH];
  static uint32 buf_len = WPS_EAP_DATA_MAX_LENGTH;
  eapol_header_t *eapol = (eapol_header_t *)buf;
#else
  // put back space for the eth header
  eapol_header_t *eapol = (eapol_header_t *)(buf  -  sizeof(struct ether_header));
#endif

  eap_header_t *eap;
  brcm_wpscli_status status;
  brcm_wpscli_wps_mode wps_mode = 0;
  time_t current_time;
  static time_t last_time=0;

  current_time = wpscli_current_time();
  if (last_time == 0)
  	last_time = current_time;

  if (g_bWpsSessionOpen && (current_time - g_tStartTime) >= g_ctxWpscliOsi.time_out ) {
    status = WPS_STATUS_PROTOCOL_FAIL_TIMEOUT;
    goto FAIL;
  }

  if (g_bWpsSessionOpen && ! g_ctxWpscliOsi.window_open) {
    g_ctxWpscliOsi.window_open = TRUE;
    window_is_opening = TRUE;
    last_time = current_time;
  }

  // Exit if there is abort request
  if(g_bRequestAbort) {
    g_bRequestAbort = FALSE;
    status = WPS_STATUS_ABORTED;
    goto FAIL;
  }

#ifdef WPSCLI_NO_WPS_LOOP
/* timeout  */
  if ( buf == NULL) {
    status = WPS_STATUS_PKTD_NO_PKT;
    //TUTRACE((TUTRACE_INFO, "Timeout .has current time %d last time %d  : %d\n", wpscli_current_time(), last_time_recv ));
  }
  else {
    status = WPS_STATUS_SUCCESS;
    /* these packets were encapsulated into a brcm event ... reconstruct */
    /* this has to be cleaned up */
    memmove(eapol->eth.ether_shost, src_mac, 6);
    /* Set the EAPOL packet type correctly */
    eapol->eth.ether_type = wpscli_htons(ETHER_TYPE_802_1X);
  }
#else
  // Receive EAPOL
  buf_len = WPS_EAP_DATA_MAX_LENGTH;  // Reset buffer length
  status = wpscli_pktdisp_wait_for_packet(buf, &buf_len, WPSCLI_GENERIC_POLL_TIMEOUT, TRUE);
#endif

  if (  status == WPS_STATUS_PKTD_NOT_EAPOL_PKT) {
    return WPS_STATUS_PROTOCOL_CONTINUE;
  }

  if (status == WPS_STATUS_PKTD_SYSTEM_FAIL)
    goto FAIL;

	if (buf != NULL) {
		if (!((eapol->type == EAPOL_START) || (eapol->type == EAP_PACKET))) {
			return WPS_STATUS_PROTOCOL_CONTINUE;
		}
	}


#ifdef WPSCLI_NO_WPS_LOOP
#ifdef _TUDEBUGTRACE
	if (buf != NULL) {
		wpscli_print_buf("WPS rx", (uint8*)buf, buf_len);
	}
#endif
#endif


  // Timeout processing

  if (  status == WPS_STATUS_PKTD_NO_PKT ) {
	  if ( g_ctxWpscliOsi.eap_state  != IDLE ) {
		  TUTRACE((TUTRACE_INFO, "process eapwps  :  timeout.  State %d \n", g_ctxWpscliOsi.eap_state));
	  }
    if ((current_time - last_time) > RESP_TIMEOUT) {

      /* timeout, let the eap state machine decide to retransmit or not   */
      if ( g_ctxWpscliOsi.eap_state== REGISTRATION_STARTED) {

	if (ap_eap_sm_process_timeout() != WPS_CONT) {
	  status = WPS_STATUS_PKTD_SYSTEM_FAIL;
	  goto FAIL;
	}
	else
	  // msg has been resent
	  last_time = current_time;
      }
      /* TODO : if  not started and peer is pending : resend request Identity
	 ... needs an API ... for now we rely on peer re-try :-(
	 Best : the eap state machine should have created and it would fold into the previous case.
      */
      else
	if (g_ctxWpscliOsi.eap_state == REQ_ID_SENT){

	}
    }
    return WPS_STATUS_PROTOCOL_CONTINUE;
  }

  if(status == WPS_STATUS_SUCCESS)  {
	  // an eapol  packet has been received

	  status = WPS_STATUS_PROTOCOL_CONTINUE;
	  eap = (eap_header_t *)eapol->body;

	  //in general, if we have already sent a request ID, the eapol src mac must match the current peer
	  //not sure what to do here. For now wait for the right peer to answer
	  if (g_ctxWpscliOsi.eap_state >= REQ_ID_SENT && memcmp(g_ctxWpscliOsi.sta_mac, eapol->eth.ether_shost, 6) != 0)
		  return WPS_STATUS_PROTOCOL_CONTINUE;

	  switch (g_ctxWpscliOsi.eap_state) {
    case IDLE:
      if(eapol->type == EAPOL_START)
	{
	  //set peer mac since  we have not already sent an REQ ID
	    memcpy(g_ctxWpscliOsi.sta_mac, eapol->eth.ether_shost, 6);
	    wpscli_set_peer_addr(eapol->eth.ether_shost);  // Set peer mac to osl
	    // send req ID
	    wpscli_softap_process_sta_eapol_start_msg((char*)eapol, eapol->eth.ether_shost);
	    last_time = current_time;
	    g_ctxWpscliOsi.eap_state = REQ_ID_SENT;
	    return WPS_STATUS_PROTOCOL_CONTINUE;
	}
      //ignore anything else, it coujld be coming from a previous session re-transmission

      break;
          case REQ_ID_SENT:


      if(eapol->type == EAPOL_START)
	{
	  // we already sent a request id. The peer didn't receive it. Try again (I hope this works !)
	  // in general, we need an API to send a req ID, or we need an already initialized epa state machine
	  // We also need to decide if we override the previous peer mac address in case it went away before
	  // starting the process.

	  if ((current_time - last_time) > RESP_TIMEOUT) {
	    memcpy(peer_addr, eapol->eth.ether_shost, 6);
	    wpscli_set_peer_addr(eapol->eth.ether_shost);  // Set peer mac to osl
	  }

	  if(memcmp(g_ctxWpscliOsi.sta_mac, eapol->eth.ether_shost, 6) != 0)
	    //what should we do here ? For now ignore and let the peer re-try
	    // in case the current peer goes away
	    return WPS_STATUS_PROTOCOL_CONTINUE;

	  wpscli_softap_process_sta_eapol_start_msg((char*)eapol, eapol->eth.ether_shost);
	  last_time = current_time;
	  return WPS_STATUS_PROTOCOL_CONTINUE;
	}
      else if (eap->type == EAP_IDENTITY) {

	      int pwdType = softap_devinfo.devPwdId;

	    // First Identity Response, process this way.
	    // The Request Identty will be sent out from inside ap_eap_sm_startWPSReg. This is different way from that of processing other
	    // EAP packet, where we need to call ap_eap_sm_get_msg_to_send and ap_eap_sm_sendMsg to send out a packet.

	    //PHIL : it would be great if that was not the case and everything could be processed the same way.

	    // Initialize WPS session and advertize WPS modes
	    /* check if the peer is a registrar or an enrollee */
	    // Get EAP Identity ID

	    wps_id = wpscli_get_eap_wps_id((char *)eapol);

	    switch (wps_id) {

	    case WPS_EAP_ID_ENROLLEE :

	      TUTRACE((TUTRACE_INFO, "Rx EAP Identity Response : Enrollee \n"));
	      wps_mode =BRCM_WPS_MODE_STA_ENR_JOIN_NW;
	      /* registrar mode */
	      /* don't proceed if the window is not open on our side.
		 This means the enrollee started before us, but we are not ready (this is where
		 we should send an M2D ....)
		 mark the enrollee as pending. In that case we will not clear the peer mac address
		 when starting enrollement on this side.
	      */
	      if (! g_ctxWpscliOsi.window_open) {
		TUTRACE((TUTRACE_INFO, "Window not open on our side. Waiting for trigger ...\n"));
		return WPS_STATUS_PROTOCOL_CONTINUE;
	      }
	      break;

	    case WPS_EAP_ID_REGISTRAR :

	      /* Enrollee mode  */
	      /* do not start in enrollee mode if a window has just been opened */
	      TUTRACE((TUTRACE_INFO, "Rx EAP Identity Response : Registrar\n"));
	      if (! window_is_opening) {
		wps_mode = BRCM_WPS_MODE_STA_ER_CONFIG_NW;
                    /* only use PIN in enrollee mode */
		      pwdType =  BRCM_WPS_PWD_TYPE_PIN;

	      }
	      else {
		TUTRACE((TUTRACE_INFO, "Enrollee mode : window is open. Ignore external registrar\n"));
		return WPS_STATUS_PROTOCOL_CONTINUE;
	      }
	    }

	    status = wpscli_softap_init_wps(wps_mode, pwdType, g_ctxWpscliOsi.pin, g_ctxWpscliOsi.nw_conf,
	    								g_ctxWpscliOsi.time_out,
	                                    g_ctxWpscliOsi.authorizedMacs, g_ctxWpscliOsi.authorizedMacs_len);
	    if(status != WPS_STATUS_SUCCESS) {
		    TUTRACE((TUTRACE_ERR, "brcm_wpscli_softap_start_wps: Failed to initialize wps session.\n"));
		    goto FAIL;
	    }
	    // Start WPS state machine is now done in wpscli_softap_init_wps
	    //retVal = ap_eap_sm_startWPSReg(g_ctxWpscliOsi.sta_mac, g_ctxWpscliOsi.bssid);
	    last_time = current_time;
	    g_ctxWpscliOsi.eap_state = REGISTRATION_STARTED;
      }
      //ignore anything else
      return WPS_STATUS_PROTOCOL_CONTINUE;
      break;

    case REGISTRATION_STARTED:
      if (eap->type == EAPOL_START) {
	// now, that would be weird
	    return WPS_STATUS_PROTOCOL_CONTINUE;
	  }

      TUTRACE((TUTRACE_INFO, "Rx EAP packet type %d\n", eap->type));
      retVal = wpscli_softap_process_eap_msg((char*)eapol, buf_len);
      last_time = current_time;

      break;
    }
  }

  // check the return code and close this session if needed
  if((retVal == WPS_SUCCESS) ||
     (retVal == WPS_MESSAGE_PROCESSING_ERROR) ||
     (retVal == WPS_ERR_REGISTRATION_PINFAIL) ||
     (retVal == WPS_ERR_ENROLLMENT_PINFAIL) ||
     (retVal == WPS_ERR_GENERIC))
    {
      // Map wps engine error code to wpscli status code
      // Why do we need this ? Can't we have ONE set of codes ??
      switch(retVal)
	{
	case WPS_SUCCESS:
	  status = WPS_STATUS_SUCCESS;

	  if(g_ctxWpscliOsi.pwd_type == BRCM_WPS_PWD_TYPE_PBC) {
	    // Remove the station from PBC monitor checker
	    wps_pb_remove_sta_from_mon_checker(g_ctxWpscliOsi.sta_mac);
	  }
	  break;
	case WPS_MESSAGE_PROCESSING_ERROR:
	  status = WPS_STATUS_PROTOCOL_FAIL_PROCESSING_MSG;
	  break;
	case WPS_ERR_REGISTRATION_PINFAIL:
	case WPS_ERR_ENROLLMENT_PINFAIL:
	  status = WPS_STATUS_PROTOCOL_FAIL_WRONG_PIN;
	  break;
	case WPS_ERR_GENERIC:
	default:
	  status = WPS_STATUS_SYSTEM_ERR;
	}
      goto FAIL;
    }
  else {
    status = WPS_STATUS_PROTOCOL_CONTINUE;
  }
  return status;

 FAIL :
  TUTRACE((TUTRACE_INFO, " Close session with status %d\n", status));
  wpscli_softap_session_end(retVal);
  brcm_wpscli_softap_close_session();
  last_time = current_time;
  return status;
}

static brcm_wpscli_status wpscli_softap_init_wps(brcm_wpscli_wps_mode wps_mode,
						 brcm_wpscli_pwd_type pwd_type,
						 const char *pin_code,
						 const brcm_wpscli_nw_settings *nw_settings,
						 uint32 time_out,
						 uint8 *authorizedMacs,
						 uint32 authorizedMacs_len)
{

	brcm_wpscli_status status = WPS_STATUS_SUCCESS;
	WpsEnrCred credAP;
	uint32 uRet = WPS_SUCCESS;
	WPS_SCMODE e_mode;
	unsigned long now;
	int send_len;
	char *sendBuf;
	int retVal;

	pwd_type = softap_devinfo.devPwdId;
	TUTRACE((TUTRACE_INFO, "wpscli_softap_init_wps: Entered. wps_mode=%d pwd_type=%d\n", wps_mode, pwd_type));

	//
	// Validate input parameters
	//
	if(nw_settings == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: Exiting. network setting is NULL.\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	// PBC session overlap check
	now = wpscli_current_time();
	if((int)pwd_type == WPS_DEVICEPWDID_PUSH_BTN) {
	//if(pwd_type == BRCM_WPS_PWD_TYPE_PBC) {
		if(wps_pb_check_pushtime(now) > 1) {
			// Session overlap detected. Don't start registrar at all
			TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: Exiting. STA PBC session overlap detected.\n"));
			return WPS_STATUS_PROTOCOL_FAIL_OVERLAP;
		}
	}

	// Convert wps mode from wpscli format to wps engine format
	if(wps_mode == BRCM_WPS_MODE_STA_ER_JOIN_NW || wps_mode == BRCM_WPS_MODE_STA_ER_CONFIG_NW) {
		// SCMODE_AP_ENROLLEE really means AP is running as Enrollee. SCMODE_AP_ENROLLEE does NOT necessarily
		// mean AP is in "Unconfigured" state
		e_mode = SCMODE_AP_ENROLLEE;
	}
	else if(wps_mode == BRCM_WPS_MODE_STA_ENR_JOIN_NW) {
		e_mode = SCMODE_AP_REGISTRAR;
	}
	else {
		// No support to other modes, e.g. AP Proxy mode
		e_mode = SCMODE_UNKNOWN;
		return WPS_STATUS_INVALID_NW_SETTINGS;
	}

	// Convert network setting format to be used by wps engine
	memset(&credAP, 0, sizeof(credAP));
	if((status = wpscli_convert_nw_settings(&g_nw_settings, &credAP)) != WPS_STATUS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: Exiting. network setting has wrong format.\n"));
		return WPS_STATUS_INVALID_NW_SETTINGS;
	}

	// Set authorizedMacs for AP registrar */
	if (authorizedMacs_len) {
		memcpy(softap_devinfo.authorizedMacs, authorizedMacs, authorizedMacs_len);
	}

	// Initialize WPS on SoftAP side and will prepare the data for wps_start_ap_registration
	status = wpscli_softap_sm_init(&credAP, e_mode, g_ctxWpscliOsi.sta_mac);
	//status = wpscli_softap_sm_init(&credAP, e_mode, (const uint8 *)ZERO_MAC);

	if(status != WPS_STATUS_SUCCESS || g_mc == NULL) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: Exiting. wps_softap_init failed.\n"));
		return WPS_STATUS_PROTOCOL_INIT_FAIL;
	}

	// Remove beacon IE set previously as wps_start_ap_registration will reset beacon IE
	// as well as probe response IE internally as well
	wpscli_softap_cleanup_wps();

	// Start registration and set WPS Probe Resp IE in wps_start_ap_registration
	// Also via wps_start_ap_registration/wps_initReg, the "g_mc" used in many
	// places of AP code will be filled with data too

	// to used in ap enrollement, when the STA is a registrar.  So, check e_mode firstly. 
	// I am not sure the g_ctxWpscliOsi.pin has been intialized
	if (e_mode == SCMODE_AP_ENROLLEE) {
		uRet = wpsap_start_enrollment(g_mc, g_ctxWpscliOsi.pin);
	}
	else {
		// This must be the SCMODE_AP_REGISTRAR
		char *sta_pin;

		if ((int)pwd_type == WPS_DEVICEPWDID_PUSH_BTN)
			sta_pin = WPS_DEFAULT_PIN;
		else
			sta_pin = g_ctxWpscliOsi.peer_pin;

		uRet = wpsap_start_registration(g_mc, sta_pin);
	}

	if(uRet != WPS_SUCCESS) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_init_wps: wps_start_ap_registration failed. Return error %d.\n", uRet));
		return WPS_STATUS_PROTOCOL_INIT_FAIL;
	}

	// ap_eap_sm_init should be called after devcfg_new and wps_init to get all "bcmwps"
	// struct members initialized properly
	if(ap_eap_sm_init(g_mc, (char *)g_ctxWpscliOsi.sta_mac, wpsap_osl_eapol_parse_msg, wpsap_osl_eapol_send_data, 0) != WPS_SUCCESS) {
		wpscli_deinit_wps_engine();  // De-initialize the wps engine

		TUTRACE((TUTRACE_ERR, "wps_softap_init: Exiting. ap_eap_sm_init fail...\n"));
		return WPS_STATUS_PROTOCOL_INIT_FAIL;
	}

	/* at this point, the window must be open */
	retVal = ap_eap_sm_startWPSReg(g_ctxWpscliOsi.sta_mac, g_ctxWpscliOsi.bssid);  // Start eap state machine
	if (retVal == WPS_CONT || retVal == WPS_SEND_MSG_CONT) {
		wpscli_softap_bcmwps_add_pktcnt(g_bcmwps);
		/* check return code to do more things */
		if (retVal == WPS_SEND_MSG_CONT) {
			send_len = ap_eap_sm_get_msg_to_send(&sendBuf);
			if (send_len >= 0)
				ap_eap_sm_sendMsg(sendBuf, send_len);
		}

		//status = WPS_STATUS_PROTOCOL_CONTINUE;
		status = WPS_STATUS_SUCCESS;
	}
	else
		status = WPS_STATUS_SYSTEM_ERR;

	TUTRACE((TUTRACE_INFO, "wpscli_softap_init_wps: Exiting. Return status %d.\n", status));
	return status;
}

brcm_wpscli_status brcm_wpscli_get_overlap_mac(unsigned char *mac)
{
	int i;
	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if (pbc_info[i].last_time != 0) {
			TUTRACE((TUTRACE_INFO, "PBC overlap with %02x:%02x:%02x:%02x:%02x:%02x\n",
				pbc_info[i].mac[0], pbc_info[i].mac[1],
				pbc_info[i].mac[2], pbc_info[i].mac[3],
				pbc_info[i].mac[4], pbc_info[i].mac[5]));
			memcpy(mac, pbc_info[i].mac, sizeof(pbc_info[i].mac));
			return WPS_STATUS_SUCCESS;
		}
	}
	return WPS_STATUS_SYSTEM_ERR;
}

static brcm_wpscli_status wpscli_convert_nw_settings(const brcm_wpscli_nw_settings *nw_settings, WpsEnrCred *wps_cred)
{
	if(wps_cred == NULL || nw_settings == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	// ssid
	wps_cred->ssidLen = strlen(nw_settings->ssid);
	memcpy(wps_cred->ssid, nw_settings->ssid, wps_cred->ssidLen);


	// network passphrase/key
	wps_cred->nwKeyLen = strlen(nw_settings->nwKey);
	memcpy(wps_cred->nwKey, nw_settings->nwKey, wps_cred->nwKeyLen);


	// key management
	switch(nw_settings->authType)
	{
	case BRCM_WPS_AUTHTYPE_OPEN:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_OPEN);
		break;
	case BRCM_WPS_AUTHTYPE_SHARED:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_SHARED);
		break;
	case BRCM_WPS_AUTHTYPE_WPAPSK:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_WPAPSK);
		break;
	case BRCM_WPS_AUTHTYPE_WPA2PSK:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_WPA2PSK);
		break;
	case BRCM_WPS_AUTHTYPE_WPAPSK_WPA2PSK:
		strcpy(wps_cred->keyMgmt, KEY_MGMT_WPAPSK_WPA2PSK);
		break;
	default:
		return WPS_STATUS_INVALID_NW_SETTINGS;
	}

	// Wep key index
	wps_cred->wepIndex = nw_settings->wepIndex+1;

	// Convert encryption method in wps engine format
	switch(nw_settings->encrType)
	{
	case BRCM_WPS_ENCRTYPE_NONE:
		wps_cred->encrType = WPS_ENCRTYPE_NONE;
		break;
	case BRCM_WPS_ENCRTYPE_WEP:
		wps_cred->encrType = WPS_ENCRTYPE_WEP;
		break;
	case BRCM_WPS_ENCRTYPE_TKIP:
		wps_cred->encrType = WPS_ENCRTYPE_TKIP;
		break;
	case BRCM_WPS_ENCRTYPE_AES:
		wps_cred->encrType = WPS_ENCRTYPE_AES;
		break;
	case BRCM_WPS_ENCRTYPE_TKIP_AES:
		wps_cred->encrType = WPS_ENCRTYPE_TKIP | WPS_ENCRTYPE_AES;
		break;
	default:
		return WPS_STATUS_INVALID_NW_SETTINGS;
	}

	return WPS_STATUS_SUCCESS;
}

static void wpscli_softap_bcmwps_add_pktcnt(wpsap_wksp_t *bcmwps)
{
	if (bcmwps)
		bcmwps->pkt_count++;
}

static void wpscli_softap_session_end(int opcode)
{
	if (opcode == WPS_STATUS_SUCCESS) {
		wps_setProcessStates(WPS_OK);
	}
	else {
		wps_setProcessStates(WPS_MSG_ERR);
	}
}

// Take STA EAPOL-Start packet and send out EAP-Request/Identity packet
static int wpscli_softap_process_sta_eapol_start_msg(char *sta_buf, uint8 *sta_mac)
{
	int retVal;
	int size;
	uint32 data_len;
	char tx_buf[(sizeof(eapol_header_t) - 1) + EAP_HEADER_LEN + 1];

	//
	// Create AP EAP Request/Identity packet
	//
	eapol_header_t *eapol = (eapol_header_t *)tx_buf;
	eap_header_t *eap = (eap_header_t *)eapol->body;
	eapol_header_t *sta_eapol = (eapol_header_t *)sta_buf;

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_sta_eapol_start_msg: Entered.\n"));

	// Send a status update to indicate the start of a WPS handshake */
	wpscli_update_status(WPS_STATUS_PROTOCOL_START_EXCHANGE, NULL);

	// Fill eapol header
	memcpy(&eapol->eth.ether_dhost, sta_mac, ETHER_ADDR_LEN);
	memcpy(&eapol->eth.ether_shost, g_ctxWpscliOsi.bssid, ETHER_ADDR_LEN);

	eapol->eth.ether_type = wpscli_htons(ETHER_TYPE_802_1X);
	eapol->version = sta_eapol->version;
	eapol->type = EAP_PACKET;
	size = EAP_HEADER_LEN + 1;  // No eap data but eap type is needed
	eapol->length = wpscli_htons(size);

	// Fill eap header
	eap->code = EAP_REQUEST;  // 1-EAP Request  2-EAP Response
	eap->id = 0;  // Start from 0
	eap->length = eapol->length;
	eap->type = EAP_IDENTITY;  // Identity

	// Set eapol length
	data_len = (sizeof(*eapol) - 1) + size;

	TUTRACE((TUTRACE_INFO, "wpscli_pktdisp_send_packet: Tx EAP Identity Request\n"));
	retVal = wpsap_osl_eapol_send_data((char *)eapol, data_len);

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_sta_eapol_start_msg: Exiting. retVal=%d\n", retVal));
	return retVal;
}

static int wpscli_softap_process_eap_msg(char *eapol_msg, uint32 len)
{
	uint32 retVal;
	int send_len;
	char *sendBuf;

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_eap_msg: Entered.\n"));

	retVal = ap_eap_sm_process_sta_msg(eapol_msg, len);

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_eap_msg: ap_eap_sm_process_sta_msg returned %d.\n", retVal));

	/* update packet count */
	if (retVal == WPS_CONT || retVal == WPS_SEND_MSG_CONT)
		wpscli_softap_bcmwps_add_pktcnt(g_bcmwps);

	/* check return code to do more things */
	if (retVal == WPS_SEND_MSG_CONT ||
		retVal == WPS_SEND_MSG_SUCCESS ||
		retVal == WPS_SEND_MSG_ERROR ||
		retVal == WPS_ERR_REGISTRATION_PINFAIL ||
		retVal == WPS_ERR_ENROLLMENT_PINFAIL) {
		send_len = ap_eap_sm_get_msg_to_send(&sendBuf);
		if (send_len >= 0)
			ap_eap_sm_sendMsg(sendBuf, send_len);

		/* WPS_SUCCESS or WPS_MESSAGE_PROCESSING_ERROR case */
		if (retVal == WPS_SEND_MSG_SUCCESS ||
			retVal == WPS_SEND_MSG_ERROR ||
			retVal == WPS_ERR_REGISTRATION_PINFAIL ||
			retVal == WPS_ERR_ENROLLMENT_PINFAIL) {
			ap_eap_sm_Failure(0);
		}

		/* over-write retVal */
		switch(retVal)
		{
		case WPS_SEND_MSG_SUCCESS:
			retVal = WPS_SUCCESS;
			break;
		case WPS_SEND_MSG_ERROR:
			retVal = WPS_MESSAGE_PROCESSING_ERROR;
			break;
		case WPS_ERR_ENROLLMENT_PINFAIL:
		case WPS_ERR_REGISTRATION_PINFAIL:
			break;  // No need to overwrite
		default:
			retVal = WPS_CONT;  // For all other cases, continue wps negotiation
		}
	}
	/* other error case */
	else if (retVal != WPS_CONT) {
		ap_eap_sm_Failure(0);
	}

	TUTRACE((TUTRACE_INFO, "wpscli_softap_process_eap_msg: Exiting.\n"));
	return retVal;
}

void wpscli_softap_cleanup_wps(void)
{
	TUTRACE((TUTRACE_INFO, "wpscli_softap_cleanup_wps: Entered.\n"));

	wpscli_del_wps_ie(WPS_IE_TYPE_SET_BEACON_IE);
	wpscli_del_wps_ie(WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);

	if (b_wps_version2)
		wpscli_del_wps_ie(WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE);

	TUTRACE((TUTRACE_INFO, "wpscli_softap_cleanup_wps: Exiting.\n"));
}


int wpscli_softap_encode_beacon_wps_ie(char *buf, int maxLength, char *deviceName,
                                       uint16 primDevCategory, uint16 primDevSubCategory)
{
	BOOL bRet = FALSE;
	int buflen = maxLength;


	/* Set softap_devinfo */
	softap_devinfo.primDeviceCategory = primDevCategory;
	softap_devinfo.primDeviceOui = 0x0050F204;
	softap_devinfo.primDeviceSubCategory = primDevSubCategory;
	strcpy(softap_devinfo.deviceName, deviceName);
	if (b_wps_version2) {
		/* Version 2 */
		softap_devinfo.version2 = 0x20;
	}

	/* Build WPS IE */
	bRet = wpscli_build_wps_ie(&softap_devinfo, FALSE, WPS_IE_TYPE_SET_BEACON_IE,
		(uint8 *)buf, &buflen);

	if (bRet == FALSE)
		return 0;

	return buflen;
}


int wpscli_softap_encode_probe_request_wps_ie(char *buf, int maxLength,
	uint16 cfgMeths, uint16 primDevCategory, uint16 primDevSubCategory,
	uint16 passwdId, char *deviceName, uint16 reqCategoryId, uint16 reqSubCategoryId)
{
	BOOL bRet = FALSE;
	int buflen = maxLength;


	/* Set softap_devinfo */
	softap_devinfo.configMethods = cfgMeths;
	softap_devinfo.primDeviceCategory = primDevCategory;
	softap_devinfo.primDeviceOui = 0x0050F204;
	softap_devinfo.primDeviceSubCategory = primDevSubCategory;
	softap_devinfo.selRegistrar = TRUE;
	softap_devinfo.devPwdId = passwdId;
	strcpy(softap_devinfo.deviceName, deviceName);
	softap_devinfo.reqDeviceCategory = reqCategoryId;
	softap_devinfo.reqDeviceSubCategory = reqSubCategoryId;
	if (b_wps_version2) {
		/* Version 2 */
		softap_devinfo.version2 = 0x20;
	}

	/* Build WPS IE */
	bRet = wpscli_build_wps_ie(&softap_devinfo, FALSE, WPS_IE_TYPE_SET_PROBE_REQUEST_IE,
		(uint8 *)buf, &buflen);

	if (bRet == FALSE)
		return 0;

	return buflen;
}


int wpscli_softap_encode_probe_response_wps_ie(char *buf, int maxLength,
	uint16 cfgMeths, uint16 primDevCategory, uint16 primDevSubCategory,
	uint16 passwdId, char *deviceName)
{
	BOOL bRet = FALSE;
	int buflen = maxLength;


	/* Set softap_devinfo */
	softap_devinfo.configMethods = cfgMeths;
	softap_devinfo.primDeviceCategory = primDevCategory;
	softap_devinfo.primDeviceSubCategory = primDevSubCategory;
	softap_devinfo.selRegistrar = TRUE;
	softap_devinfo.devPwdId = passwdId;
	strcpy(softap_devinfo.deviceName, deviceName);

	/* Build WPS IE */
	bRet = wpscli_build_wps_ie(&softap_devinfo, FALSE, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE,
		(uint8 *)buf, &buflen);

	if (bRet == FALSE)
		return 0;

	return buflen;
}

static brcm_wpscli_status wpscli_softap_add_wps_ie(void)
{
	BOOL bRet_BE, bRet_PR, bRet_AR = FALSE;


	TUTRACE((TUTRACE_INFO, "wpscli_softap_add_wps_ie: Entered(softap_devinfo.deviceName=%s).\n",
		softap_devinfo.deviceName));

	/* Set softap_devinfo */
	softap_devinfo.primDeviceCategory = 6;
	softap_devinfo.primDeviceSubCategory = 1;
	/*  PHIL this should be eith a parameter  or not reset here */
	// softap_devinfo.devPwdId = 0; /* Default PIN */

	bRet_BE = wpscli_add_wps_ie(&softap_devinfo, FALSE, WPS_IE_TYPE_SET_BEACON_IE);
	if (bRet_BE == FALSE) {
		TUTRACE((TUTRACE_ERR, "wpscli_softap_add_wps_ie: Failed to add WPS IE to beacon.\n"));
	}

	bRet_PR = wpscli_add_wps_ie(&softap_devinfo, FALSE, WPS_IE_TYPE_SET_PROBE_RESPONSE_IE);
	if (bRet_BE == FALSE) {
		TUTRACE((TUTRACE_INFO, "wpscli_softap_add_wps_ie: Failed to add WPS IE to probe response.\n"));
	}

	if (b_wps_version2) {
		bRet_AR = wpscli_add_wps_ie(&softap_devinfo, FALSE, WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE);
		if (bRet_AR == FALSE) {
			TUTRACE((TUTRACE_INFO, "wpscli_softap_add_wps_ie: Failed to add WPS IE to associate response.\n"));
		}
	}

	TUTRACE((TUTRACE_INFO, "wpscli_softap_add_wps_ie: Exiting.\n"));

	return (bRet_BE & bRet_PR & bRet_AR) ? WPS_STATUS_SUCCESS : WPS_STATUS_SET_WPS_IE_FAIL;
}

static brcm_wpscli_status wpscli_softap_sm_init(WpsEnrCred *pNwCred,
											 WPS_SCMODE ap_wps_mode,
											 const uint8 *sta_mac)
{
	wpsap_wksp_t *bcmwps = NULL;
	DevInfo *ap_devinfo = NULL;
	brcm_wpscli_status status = WPS_STATUS_SUCCESS;

	TUTRACE((TUTRACE_INFO, "wps_softap_init: Entered.\n"));

	if(pNwCred == NULL) {
		TUTRACE((TUTRACE_ERR, "wps_softap_init: Invalid NULL pNwCred parameter passed\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}

	if(sta_mac == NULL) {
		TUTRACE((TUTRACE_ERR, "wps_softap_init: Invalid NULL sta_mac parameter passed\n"));
		return WPS_STATUS_INVALID_NULL_PARAM;
	}
	else {
		TUTRACE((TUTRACE_ERR, "wps_softap_init: init sta_mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
			sta_mac[0], sta_mac[1], sta_mac[2], sta_mac[3], sta_mac[4], sta_mac[5]));

	}

	// Initialize packet send/recv handlers which becomes to OS dependent osl.
	wps_osl_init(NULL);

	//
	// Fill in bcmwps information
	//
	bcmwps = (wpsap_wksp_t *)malloc(sizeof(wpsap_wksp_t));
	if (!bcmwps) {
		TUTRACE((TUTRACE_ERR, "wps_softap_init: Can not allocate memory for wps structer...\n"));
		return WPS_STATUS_SYSTEM_ERR;
	}
	memset(bcmwps, 0, sizeof(wpsap_wksp_t));

	// Set SoftAP mac address
	if(g_ctxWpscliOsi.bssid)
		memcpy(bcmwps->mac_ap, g_ctxWpscliOsi.bssid, 6);  // Fill SoftAP mac address

	// Set STA mac address? Not existed yet.
	memcpy(bcmwps->mac_sta, sta_mac, 6);  // Fill station mac address

	// Set WPS mode
	bcmwps->sc_mode = ap_wps_mode;

	// read device info here and pass to wps_init
	ap_devinfo = devinfo_new();
	if (ap_devinfo == NULL) {
		status = WPS_STATUS_PROTOCOL_INIT_FAIL;
		TUTRACE((TUTRACE_ERR, "wps_softap_inti: devinfo_new failed \n"));
		goto INIT_END;
	}

	if (wpscli_softap_readConfigure(bcmwps, ap_devinfo, pNwCred) != WPS_SUCCESS) {
		status = WPS_STATUS_PROTOCOL_INIT_FAIL;
		TUTRACE((TUTRACE_ERR, "wps_softap_init: wpsap_readConfigure fail...\n"));
		goto INIT_END;
	}

	ap_devinfo->sc_mode = ap_wps_mode;  // Set AP wps mode
	if ((bcmwps->mc = (void*)wps_init(bcmwps, ap_devinfo)) == NULL) {
		status = WPS_STATUS_PROTOCOL_INIT_FAIL;
		TUTRACE((TUTRACE_ERR, "wps_softap_init: wps_init fail...\n"));
		goto INIT_END;
	}
	g_mc = bcmwps->mc;

	g_bcmwps = bcmwps;

	TUTRACE((TUTRACE_INFO, "wps_softap_init: ap_eap_sm_init successful...\n"));

INIT_END:
	if (ap_devinfo)
		devinfo_delete(ap_devinfo);

	TUTRACE((TUTRACE_INFO, "wps_softap_init: Exiting.\n"));
	return status;
}

static int wpscli_softap_readConfigure(wpsap_wksp_t *bcmwps,
									   DevInfo *ap_devinfo,
									   WpsEnrCred *credential)
{
	int sc_mode = bcmwps->sc_mode;
	unsigned char *wl_mac = bcmwps->mac_ap;

		TUTRACE((TUTRACE_ERR, "Enter wpscli_softap_readConfigure\n"));

	if (!wl_mac) {
		TUTRACE((TUTRACE_ERR, "Error getting mac\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}

	if (sc_mode == SCMODE_UNKNOWN) {
		TUTRACE((TUTRACE_ERR, "Error getting wps config mode\n"));
		return MC_ERR_CFGFILE_CONTENT;
	}

	/* Update softap_devinfo */
	memcpy(softap_devinfo.ssid, credential->ssid, SIZE_SSID_LENGTH);
	memcpy(softap_devinfo.keyMgmt, credential->keyMgmt, SIZE_20_BYTES);
	if(credential->encrType & WPS_ENCRTYPE_WEP)
		softap_devinfo.wep = 1;
	else
		softap_devinfo.wep = 0;
	softap_devinfo.crypto = credential->encrType;

	/* Copy softap_devinfo object */
	memcpy(ap_devinfo, &softap_devinfo, sizeof(DevInfo));

	/* Fill ap_devcfg object */
	ap_devinfo->sc_mode = sc_mode;

	/* Network Key */
	wps_strncpy(ap_devinfo->nwKey, credential->nwKey, sizeof(ap_devinfo->nwKey));

	/* Wep Index */
	ap_devinfo->wepKeyIdx = credential->wepIndex;
	

	/* WSC 2.0, Set peer Mac address used for create M8ap/M8sta */
	memcpy(ap_devinfo->peerMacAddr, bcmwps->mac_sta, SIZE_6_BYTES);
	return WPS_SUCCESS;
}

// Update pbc monitor checker and return current number of requesting STAs within the
// 2-minute monitoring windows time
static int wps_pb_check_pushtime(unsigned long time)
{
	int i;
	int PBC_sta = PBC_OVERLAP_CNT;

	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if ((time < pbc_info[i].last_time) || ((time - pbc_info[i].last_time) > PBC_MONITOR_TIME)) {
			memset(&pbc_info[i], 0, sizeof(PBC_STA_INFO));
		}

		if (pbc_info[i].last_time == 0)
			PBC_sta--;
	}

	return PBC_sta;
}

static void wps_pb_update_pushtime(char *mac)
{
	int i;
	unsigned long now;

	if(mac == NULL)
		return;

	now = wpscli_current_time();

	wps_pb_check_pushtime(now);

	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if (memcmp(mac, pbc_info[i].mac, 6) == 0) {
			pbc_info[i].last_time = now;
			return;
		}
	}

	if (pbc_info[0].last_time <= pbc_info[1].last_time)
		i = 0;
	else
		i = 1;

	memcpy(pbc_info[i].mac, mac, 6);
	pbc_info[i].last_time = now;
}


static void wps_pb_remove_sta_from_mon_checker(const uint8 *mac)
{
	int i;

	// Remove the STA mac from monitor checker. We should remove the sta from PBC Monitor Checker
	// after the WPS session is completed successfully
	for (i = 0; i < PBC_OVERLAP_CNT; i++) {
		if (memcmp(mac, pbc_info[i].mac, 6) == 0) {
			// Remove the sta from monitor checker by zeroing out the element buffer
			memset(&pbc_info[i], 0, sizeof(PBC_STA_INFO));
		}
	}
}

static int wps_eap_parse_prob_reqIE(unsigned char *mac, unsigned char *p_data, uint32 len)
{
	int ret_val = 0;
	WpsProbreqIE prReqIE;
	BufferObj *bufObj = buffobj_new();

	/* De-serialize this IE to get to the data */
	buffobj_dserial(bufObj, p_data, len);

	ret_val += tlv_dserialize(&prReqIE.version, WPS_ID_VERSION, bufObj, 0, 0);
	ret_val += tlv_dserialize(&prReqIE.reqType, WPS_ID_REQ_TYPE, bufObj, 0, 0);
	ret_val +=  tlv_dserialize(&prReqIE.confMethods, WPS_ID_CONFIG_METHODS, bufObj, 0, 0);
	if (WPS_ID_UUID_E == buffobj_NextType(bufObj)) {
		ret_val += tlv_dserialize(&prReqIE.uuid, WPS_ID_UUID_E, bufObj, SIZE_16_BYTES, 0);
	}
	else if (WPS_ID_UUID_R == buffobj_NextType(bufObj)) {
		ret_val +=  tlv_dserialize(&prReqIE.uuid, WPS_ID_UUID_R, bufObj, SIZE_16_BYTES, 0);
	}

	/* Primary Device Type is a complex TLV - handle differently */
	ret_val += tlv_primDeviceTypeParse(&prReqIE.primDevType, bufObj);

	/* if any error, bail  */
	if (ret_val < 0) {
		buffobj_del(bufObj);
		return -1;
	}

	tlv_dserialize(&prReqIE.rfBand, WPS_ID_RF_BAND, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.assocState, WPS_ID_ASSOC_STATE, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.confErr, WPS_ID_CONFIG_ERROR, bufObj, 0, 0);
	tlv_dserialize(&prReqIE.pwdId, WPS_ID_DEVICE_PWD_ID, bufObj, 0, 0);

	// STA requesting PBC is detected. Update the pb_info monitor checker
	if (prReqIE.pwdId.m_data == WPS_DEVICEPWDID_PUSH_BTN) {
		TUTRACE((TUTRACE_INFO, "\n\nPush Button sta detect\n\n"));
		wps_pb_update_pushtime((char *)mac);
	}

	buffobj_del(bufObj);
	return 0;
}

// STA probe request is received
brcm_wpscli_status brcm_wpscli_softap_on_sta_probreq_wpsie(
	const unsigned char *mac, const uint8 *databuf, uint32 datalen)
{
	if (mac == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

	if(databuf == NULL)
		return WPS_STATUS_INVALID_NULL_PARAM;

		// Process WPS IE and update PBC checker from inside wps_eap_parse_prob_reqIE
	if (wps_eap_parse_prob_reqIE((unsigned char *)mac,
		(unsigned char *)databuf, datalen))
		return WPS_STATUS_SYSTEM_ERR;

	return WPS_STATUS_SUCCESS;
}
