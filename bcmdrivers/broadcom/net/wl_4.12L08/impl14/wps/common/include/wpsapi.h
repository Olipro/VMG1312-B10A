/*
 * WPS API
 *
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpsapi.h 323637 2012-03-26 10:31:40Z $
 */

#ifdef MSTC_WLAN_WPS_FOR_TR69 //__MSTC__, Paul Ho, TR069 for wps
#define WPS_LOCKED_STATE_UNLOCKED 0
#define WPS_LOCKED_STATE_LOCKED_BY_LOCAL_MANAGEMENT 1
#define WPS_LOCKED_STATE_LOCKED_BY_REMOTE_MANAGEMENT 2
#define WPS_LOCKED_STATE_PIN_RETRY_LIMIT_REACHED 3

#define WPS_CONFIG_ERROR_NO_ERROR 0 
#define WPS_CONFIG_ERROR_DECRYPTION_CRC_FAILURE 2
#define WPS_CONFIG_ERROR_SIGNAL_TOO_WEAK 5
#define WPS_CONFIG_ERROR_COULDNT_CONNECT_TO_REGISTRAR 11
#define WPS_CONFIG_ERROR_ROUGE_ACTIVITY_SUSPECTED 13
#define WPS_CONFIG_ERROR_DEVICE_BUSY 14
#define WPS_CONFIG_ERROR_SETUP_LOCKED 15
#define WPS_CONFIG_ERROR_MESSAGE_TIMEOUT 16
#define WPS_CONFIG_ERROR_REGISTRATION_SESSION_TIMEOUT 17
#define WPS_CONFIG_ERROR_DEVICE_PASSWORD_AUTH_FAILURE 18

#endif /* MSTC_WLAN_WPS_FOR_TR69 */

#ifndef _WPSAPI_
#define _WPSAPI_

#ifdef __cplusplus
extern "C" {
#endif

#include <reg_protomsg.h>
#include <wps_devinfo.h>

typedef struct {
	void *bcmwps;

	RegSM *mp_regSM;
	EnrSM *mp_enrSM;

	bool mb_initialized;
	bool mb_stackStarted;

	DevInfo *dev_info;

	bool b_UpnpDevGetDeviceInfo;
} WPSAPI_T;

void * wps_init(void *bcmwps, DevInfo *ap_devinfo);
uint32 wps_deinit(WPSAPI_T *g_mc);

uint32 wps_ProcessBeaconIE(char *ssid, uint8 *macAddr, uint8 *p_data, uint32 len);
uint32 wps_ProcessProbeReqIE(uint8 *macAddr, uint8 *p_data, uint32 len);
uint32 wps_ProcessProbeRespIE(uint8 *macAddr, uint8 *p_data, uint32 len);

int wps_getenrState(void *mc_dev);
int wps_getregState(void *mc_dev);

uint32 wps_sendStartMessage(void *bcmdev, TRANSPORT_TYPE trType);
int wps_get_upnpDevSSR(WPSAPI_T *g_mc, void *p_cbData, uint32 length, CTlvSsrIE *ssrmsg);
int wps_upnpDevSSR(WPSAPI_T *g_mc, CTlvSsrIE *ssrmsg);

int wps_getProcessStates();
void wps_setProcessStates(int state);
void wps_setStaDevName(unsigned char *str);
void wps_setPinFailInfo(uint8 *mac, char *name, char *state);

#ifdef __cplusplus
}
#endif


#endif /* _WPSAPI_ */
