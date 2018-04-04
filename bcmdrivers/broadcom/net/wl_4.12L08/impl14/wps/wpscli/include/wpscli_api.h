/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_api.h 296251 2011-11-15 00:55:08Z $
 *
 * Description: WPSCLI Library API Definition
 *
 */
#ifndef __WPSCLI_API_H__
#define __WPSCLI_API_H__

#ifdef __cplusplus
extern "C" {
#endif

/* define WPS 2.0 */
#ifndef WPSCLI_WSCV2
#define WPSCLI_WSCV2
#endif

#define BRCM_WPS_MAX_AP_NUMBER		100
#define BRCM_WPS_PIN_SIZE			8

#undef SIZE_20_BYTES
#undef SIZE_32_BYTES
#undef SIZE_64_BYTES
#define SIZE_20_BYTES	20
#define SIZE_32_BYTES	32
#define SIZE_64_BYTES	64

#define TYPEDEF_UINT32
#define TYPEDEF_INT32
#define TYPEDEF_UINT8
#define TYPEDEF_UINT16

#ifndef _TYPEDEFS_H_
typedef unsigned int	uint32;
typedef int				int32;
typedef unsigned char	uint8;
typedef unsigned short	uint16;
#endif

typedef void *brcm_wpscli_status_cb_data;
typedef void *brcm_wpscli_request_ctx;

// Definition of wpscli library callback and funtion return status codes
typedef enum BRCM_WPS_STATUS
{
	// Generic WPS library errors
	WPS_STATUS_SUCCESS,
	WPS_STATUS_SYSTEM_ERR,						// generic error not belonging to any other definition
	WPS_STATUS_OPEN_ADAPTER_FAIL,				// failed to open/init wps adapter
	WPS_STATUS_ABORTED,							// user cancels the connection
	WPS_STATUS_INVALID_NULL_PARAM,				// invalid NULL parameter passed in
	WPS_STATUS_NOT_ENOUGH_MEMORY,				// more memory is required to retrieve data
	WPS_STATUS_INVALID_NW_SETTINGS,				// Invalid network settings
	WPS_STATUS_WINDOW_NOT_OPEN,					// WPS windows is not going on now
	WPS_STATUS_INVALID_PIN,						// WPS PIN code is invalid

	// WPS protocol related errors
	WPS_STATUS_PROTOCOL_SUCCESS,
	WPS_STATUS_PROTOCOL_INIT_FAIL,
	WPS_STATUS_PROTOCOL_INIT_SUCCESS,
	WPS_STATUS_PROTOCOL_START_EXCHANGE,
	WPS_STATUS_PROTOCOL_END_EXCHANGE,
	WPS_STATUS_PROTOCOL_CONTINUE,
	WPS_STATUS_PROTOCOL_SEND_MEG,
	WPS_STATUS_PROTOCOL_WAIT_MSG,
	WPS_STATUS_PROTOCOL_RECV_MSG,
	WPS_STATUS_PROTOCOL_FAIL_TIMEOUT,			// timeout and fails in M1-M8 negotiation
	WPS_STATUS_PROTOCOL_FAIL_MAX_EAP_RETRY,		// don't retry any more because of EAP timeout as AP gives up already
	WPS_STATUS_PROTOCOL_FAIL_OVERLAP,			// PBC session overlap
	WPS_STATUS_PROTOCOL_FAIL_WRONG_PIN,			// fails in protocol processing stage because of unmatched pin number
	WPS_STATUS_PROTOCOL_FAIL_EAP,				// fails because of EAP failure
	WPS_STATUS_PROTOCOL_FAIL_UNEXPECTED_NW_CRED,// after wps negotiation, unexpected network credentials are received
	WPS_STATUS_PROTOCOL_FAIL_PROCESSING_MSG,	// after wps negotiation, unexpected network credentials are received
	
	// WL handler related status code
	WPS_STATUS_SET_WPS_IE_FAIL,
	WPS_STATUS_DEL_WPS_IE_FAIL,
	WPS_STATUS_IOCTL_SET_FAIL,					// failed to set iovar
	WPS_STATUS_IOCTL_GET_FAIL,					// failed to get iovar

	// WLAN related status code
	WPS_STATUS_WLAN_INIT_FAIL,
	WPS_STATUS_WLAN_SCAN_START,
	WPS_STATUS_WLAN_NO_ANY_AP_FOUND,
	WPS_STATUS_WLAN_NO_WPS_AP_FOUND,
	WPS_STATUS_WLAN_CONNECTION_START,			// preliminary association failed
	WPS_STATUS_WLAN_CONNECTION_ATTEMPT_FAIL,	// preliminary association failed
	WPS_STATUS_WLAN_CONNECTION_LOST,			// preliminary association lost during registration
	WPS_STATUS_WLAN_CONNECTION_DISCONNECT,

	// Packet dispatcher related errors
	WPS_STATUS_PKTD_INIT_FAIL,
	WPS_STATUS_PKTD_SYSTEM_FAIL,				// Generic packet dispatcher related errors not belonging any other definition
	WPS_STATUS_PKTD_SEND_PKT_FAIL,				// failed to send eapol packet
	WPS_STATUS_PKTD_NO_PKT,						// no packet is received (wait timeout)
	WPS_STATUS_PKTD_NOT_EAPOL_PKT,			// received packet is not eapol packet
} brcm_wpscli_status;

// Encryption type
typedef enum {
	BRCM_WPS_ENCRTYPE_NONE,
	BRCM_WPS_ENCRTYPE_WEP,
	BRCM_WPS_ENCRTYPE_TKIP,
	BRCM_WPS_ENCRTYPE_AES,
	BRCM_WPS_ENCRTYPE_TKIP_AES
} brcm_wpscli_encrtype;

// Authentication type
typedef enum {
	BRCM_WPS_AUTHTYPE_OPEN,
	BRCM_WPS_AUTHTYPE_SHARED,
	BRCM_WPS_AUTHTYPE_WPAPSK,
	BRCM_WPS_AUTHTYPE_WPA2PSK,
	BRCM_WPS_AUTHTYPE_WPAPSK_WPA2PSK
} brcm_wpscli_authtype;

// WPS message types
typedef enum {
	BRCM_WPS_MSGTYPE_EAPOL_START,		// NONE/EAPOL-START
	BRCM_WPS_MSGTYPE_BEACON,			// BEACON
	BRCM_WPS_MSGTYPE_PROBE_REQ,			// PROBE_REQ
	BRCM_WPS_MSGTYPE_PROBE_RESP,		// PROBE_RESP
	BRCM_WPS_MSGTYPE_M1,				// M1
	BRCM_WPS_MSGTYPE_M2,				// M2
	BRCM_WPS_MSGTYPE_M2D,				// M2D
	BRCM_WPS_MSGTYPE_M3,				// M3
	BRCM_WPS_MSGTYPE_M4,				// M4
	BRCM_WPS_MSGTYPE_M5,				// M5
	BRCM_WPS_MSGTYPE_M6,				// M6
	BRCM_WPS_MSGTYPE_M7,				// M7
	BRCM_WPS_MSGTYPE_M8,				// M8
	BRCM_WPS_MSGTYPE_ACK,				// ACK
	BRCM_WPS_MSGTYPE_NACK,				// NACK
	BRCM_WPS_MSGTYPE_DONE,				// DONE
	BRCM_WPS_MSGTYPE_IDENTITY,			// EAP Identity
	BRCM_WPS_MSGTYPE_EAP_START,			// EAP-Request (Start)
	BRCM_WPS_MSGTYPE_EAP_FAIL			// EAP-Fail
} brcm_wpscli_wps_msgtype;

// Define the role of SoftAP or Station side
typedef enum {
	BRCM_WPSCLI_ROLE_SOFTAP,
	BRCM_WPSCLI_ROLE_STA
} brcm_wpscli_role;

// WPS mode
typedef enum {
	BRCM_WPS_MODE_STA_ENR_JOIN_NW = 0,		// STA joins network. AP adds enrollee
	BRCM_WPS_MODE_STA_ER_JOIN_NW,			// STA external registrar joins network.
	BRCM_WPS_MODE_STA_ER_CONFIG_NW			// STA eternal registrar configures network.
} brcm_wpscli_wps_mode;

// WPS password type
typedef enum {
	BRCM_WPS_PWD_TYPE_PBC,
	BRCM_WPS_PWD_TYPE_PIN
} brcm_wpscli_pwd_type;

// Simple Config state
typedef enum {
	BRCM_WPS_SCSTATE_UNKNOWN = 0,
	BRCM_WPS_SCSTATE_UNCONFIGURED,
	BRCM_WPS_SCSTATE_CONFIGURED
} brcm_wpscli_scstate;

// Network settings
typedef struct td_brcm_wpscli_nw_settings {
    char ssid[SIZE_32_BYTES+1];			// network name/ssid
    char nwKey[SIZE_64_BYTES+1];		// authentication passphrase
	brcm_wpscli_authtype authType;		// authentication type
    brcm_wpscli_encrtype encrType;		// encryption type
	uint16 wepIndex;					// wep key index when WEP mode is used
	uint8 nwKeyShareable;			// WSC 2.0 network key shareable
} brcm_wpscli_nw_settings;

// Include information of the Softap
typedef struct td_brcm_wpscli_ap_entry {
	char ssid[SIZE_32_BYTES+1];			// ap ssid with string terminator character
	uint8 bssid[6];						// ap bssid (mac address)
	uint16 band;						// band
	uint8 wsec;							// the dot11 privacy capability
	uint8 activated;					// ap is wps-activated or not
	brcm_wpscli_scstate scstate;		// simple-config configuration state
	brcm_wpscli_pwd_type pwd_type;		// wps password type
	uint8 version2;				// WSC 2.0 support
	uint8 authorizedMACs[6 * 5];		// WSC 2.0 authorizedMACS
} brcm_wpscli_ap_entry;

// List of APs
typedef struct td_brcm_wpscli_ap_list {
    uint32 total_items;
    brcm_wpscli_ap_entry ap_entries[1];
} brcm_wpscli_ap_list;

// WPS client library callback procedure
typedef brcm_wpscli_status (*brcm_wpscli_status_cb)(brcm_wpscli_request_ctx cb_context,
													brcm_wpscli_status status,
													brcm_wpscli_status_cb_data cb_data);

// WPS log output function
typedef void (*brcm_wpscli_log_output_fn)(int is_err, char *traceMsg);


//
// Define APIs which are generic to both STA and SoftAP side of WPS
//

// Generate secure network settings
brcm_wpscli_status brcm_wpscli_generate_nw_settings(brcm_wpscli_nw_settings *nw_settings);

// Need to call once at the beginning of the library to do initialization work
brcm_wpscli_status brcm_wpscli_open(const char *dev_if_name,
								    brcm_wpscli_role cli_role,
								    brcm_wpscli_request_ctx request_ctx,
								    brcm_wpscli_status_cb status_cb);

// Need to call once before the end of using the library
brcm_wpscli_status brcm_wpscli_close(void);

// Request to abort current WPS procedure
brcm_wpscli_status brcm_wpscli_abort(void);

// Get password type currently being used
brcm_wpscli_status brcm_wpscli_get_pwd_type(void);

// Randomly generate an 8-digit numeric PIN with valid checksum of 8th digit
brcm_wpscli_status brcm_wpscli_generate_pin(char *pin, int bufLen);

// Validate WPS PIN string in 4 or 8 digit format
brcm_wpscli_status brcm_wpscli_validate_pin(const char *pin);

// Generate network settings
brcm_wpscli_status brcm_wpscli_generate_cred(brcm_wpscli_nw_settings *nw_settings);

// Redirect WPS debug logs to the given log function
brcm_wpscli_status brcm_wpscli_redirect_logs(brcm_wpscli_log_output_fn fn);

// Switch WPS version 1 and Version 2
brcm_wpscli_status brcm_wpscli_switch_wps_version(int is_use_version1);


//
// Define API set on SoftAP side
//

// Enable WPS on SoftAP
brcm_wpscli_status brcm_wpscli_softap_enable_wps(const char *deviceName, const uint16 configMethods,
	uint8 *authorizedMacs, uint32 authorizedMacs_len);

// Disable WPS on SoftAP
brcm_wpscli_status brcm_wpscli_softap_disable_wps(void);

// SoftAP starts WPS registration to enrollee STA device.
brcm_wpscli_status brcm_wpscli_softap_start_wps(brcm_wpscli_wps_mode wps_mode,
												brcm_wpscli_pwd_type pwd_type,
												const char *pin_code,
												brcm_wpscli_nw_settings *nw_settings,
												uint32 time_out,
												uint8 *peer_mac,
												uint8 *authorizedMacs,
												uint32 authorizedMacs_len);

// Wait and process a single EAPOL mesg and return current status 
#ifdef WPSCLI_NO_WPS_LOOP
	brcm_wpscli_status brcm_wpscli_softap_process_eapwps(char *buf, uint32 buf_len, uint8 *src_mac,uint8 *peer_addr);
#else
brcm_wpscli_status brcm_wpscli_softap_process_eapwps(uint8 *peer_addr);
#endif


// Close the current WPS session
brcm_wpscli_status brcm_wpscli_softap_close_session(void);

// Tell whether the current WPS window is open or not
int brcm_wpscli_softap_is_wps_window_open(void);

// Called when a STA's probe request is received. We will analyze the probe request to see if there is a WPS IE
brcm_wpscli_status brcm_wpscli_softap_on_sta_probreq_wpsie(const unsigned char *mac,
														   const uint8 *databuf,
														   uint32 datalen);

  // used to pass down context parameters. That could include sta PIN, ap PIN , mode ... etc  We should use a common structure.
 
brcm_wpscli_status brcm_wpscli_softap_set_wps_context(brcm_wpscli_nw_settings *nw_conf, const char* dev_pin,
	uint8 *authorizedMacs, uint32 authorizedMacs_len) ;

// Encode WPS IE WPS IE in beacon
//int wpscli_softap_encode_beacon_wps_ie(char *buf, int maxLength);
int wpscli_softap_encode_beacon_wps_ie(char *buf, int maxLength, char *deviceName,
                                       uint16 primDevCategory, uint16 primDevSubCategory);

// Encode WPS IE in probe request
int wpscli_softap_encode_probe_request_wps_ie(char *buf,
											  int maxLength,
											  uint16 cfgMethods,
											  uint16 primDevCategory, 
											  uint16 primDevSubCategory,
											  uint16 passwId,
											  char *deviceName,
											  uint16 reqCategoryId,
											  uint16 reqSubCategoryId);

// Encode WPS IE in probe response
int wpscli_softap_encode_probe_response_wps_ie(char *buf, int maxLength,
	uint16 cfgMeths, uint16 primDevCategory, uint16 primDevSubCategory,
	uint16 passwdId, char *deviceName);

//
// Define API set available on station side only
//
// brcm_wpscli_status brcm_wpscli_sta_search_wps_ap();
brcm_wpscli_status brcm_wpscli_sta_search_wps_ap(uint32 *wps_ap_total);

// Return the list of scanned WPS AP following the call to brcm_wpscli_sta_search_wps_ap
brcm_wpscli_status brcm_wpscli_sta_get_wps_ap_list(brcm_wpscli_ap_list *wps_ap_list,
												   uint32 buf_size,
												   uint32 *wps_ap_total);

// STA starts WPS registration to enroll to network. The intermediate status and
// result can be retrieved via the brcm_wpscli_status_cb callback procedure
// Save network settings to "nw_settings"
brcm_wpscli_status brcm_wpscli_sta_start_wps(const char *ssid,
											 uint32 wsec,
											 const uint8 *bssid,
											 int num_chanspec,
											 uint16 *chanspec,
											 brcm_wpscli_wps_mode wps_mode,
											 brcm_wpscli_pwd_type pwd_type,
											 const char *pin_code,
											 uint32 time_out,
											 brcm_wpscli_nw_settings *nw_settings);

// If brcm_wpscli_softap_start_wps() returns WPS_STATUS_PROTOCOL_FAIL_OVERLAP,
// this function can be called immediately to get the overlap device's MAC
// address. Returns WPS_STATUS_SUCCESS if an overlap MAC address was found.
brcm_wpscli_status brcm_wpscli_get_overlap_mac(unsigned char *mac);


brcm_wpscli_status wpscli_softap_init(const char *bssid);
brcm_wpscli_status wpscli_softap_construct_def_devinfo();
brcm_wpscli_status brcm_wpscli_softap_rem_wps_ie(void);
brcm_wpscli_status wpscli_sta_init(const char *mac);
brcm_wpscli_status wpscli_sta_construct_def_devinfo();
brcm_wpscli_status brcm_wpscli_sta_rem_wps_ie();

#ifdef __cplusplus
}  // extern "C" {
#endif

#endif // __WPSCLI_API_H__
