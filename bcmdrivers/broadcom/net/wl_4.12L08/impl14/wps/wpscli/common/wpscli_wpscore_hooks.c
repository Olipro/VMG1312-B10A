/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wpscore_hooks.c 241182 2011-02-17 21:50:03Z $
 *
 * Description: Implement APIs defined in wpscli_wps_core library and used in wps core common files
 *
 */
#include "wpscli_osl.h"
#include <proto/eapol.h>
#include <wps_wl.h>
#include <portability.h>
#include <wpserror.h>
#include <tutrace.h>
#include "wpscli_common.h"

extern BOOL wpscli_set_wps_ie(unsigned char *p_data, int length, unsigned int cmdtype);
extern BOOL wpscli_del_wps_ie(unsigned int frametype);
extern uint32 wpscli_sta_eap_send_data_down(char *dataBuffer, uint32 dataLen);

/* implement Portability.h */ 
uint32 WpsHtonl(uint32 intlong)
{
    return wpscli_htonl(intlong);
}

uint16 WpsHtons(uint16 intshort)
{
    return wpscli_htons(intshort);
}

uint16 WpsHtonsPtr(uint8 * in, uint8 * out)
{
	uint16 v;
	uint8 *c;
	c = (uint8 *)&v;
	c[0] = in[0]; c[1] = in[1];
	v = wpscli_htons(v);
	out[0] = c[0]; out[1] = c[1];
	return v;
} 

uint32 WpsHtonlPtr(uint8 * in, uint8 * out)
{
	uint32 v;
	uint8 *c;
	c = (uint8 *)&v;
	c[0] = in[0]; c[1] = in[1]; c[2] = in[2]; c[3] = in[3];
	v = wpscli_htonl(v);
	out[0] = c[0]; out[1] = c[1]; out[2] = c[2]; out[3] = c[3];
	return v;
} 


uint32 WpsNtohl(uint8 *a)
{
	uint32 v;

	memcpy(&v, a, sizeof(v));
	return wpscli_ntohl(v);
}

uint16 WpsNtohs(uint8 *a)
{
	uint16 v;

	memcpy(&v, a, sizeof(v));
	return wpscli_ntohs(v);
}


void WpsSleep(uint32 seconds)
{
    wpscli_sleep(seconds*1000);
}

void WpsSleepMs(uint32 ms)
{
    wpscli_sleep(ms);
}

void wps_setProcessStates(int state)
{
}
 
void wps_setStaDevName(unsigned char *str)
{
}

void upnp_device_uuid(unsigned char *uuid)
{
	char uuid_local[16] = {0x22, 0x21, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0xa, 0x0b, 0x0c, 0x0d, 0x0e, 0x0f};

    TUTRACE((TUTRACE_INFO, "upnp_device_uuid: Entered.\n"));

	if(uuid)
		memcpy(uuid, uuid_local, 16);

/*	// We should consider to generate uuid from following code which should work on non-windows platform

	unsigned char mac[6];
	char deviceType[] = "urn:schemas-wifialliance-org:device:WFADevice:1";

	MD5_CTX mdContext;

	get_lan_mac(mac);

	// Generate hash 
	MD5Init(&mdContext);
	MD5Update(&mdContext, mac, sizeof(mac));
	MD5Update(&mdContext, deviceType, strlen(deviceType));
	MD5Final(uuid, &mdContext);
*/
    TUTRACE((TUTRACE_INFO, "upnp_device_uuid: Exiting.\n"));
	return;
}

char* wpsap_osl_eapol_parse_msg(char *msg, int msg_len, int *len)
{
	/* just return msg and msg_len, the msg is a eapol raw date */
	*len = msg_len;
	return msg;
}

int get_instance_by_wlmac(unsigned char *mac)
{
	// To solve link error. No usage on SoftAP
	return 0;
}

int get_wlname_by_mac(unsigned char *mac, char *wlname)
{
	// To solve link error. No usage on SoftAP
	return 0;
}

// OS dependent UPNP APIs defined in wps_api.h. No implemenation on SoftAP side
void wpsap_osl_upnp_update_wlan_event(int if_instance, unsigned char *macaddr,
	char *databuf, int datalen, int init)
{
}

int wpsap_osl_upnp_subscriber_num(int if_instance)
{
	return 0;
}

void wpsap_osl_upnp_attach_wfa_dev(int if_instance)
{
}

void wpsap_osl_upnp_detach_wfa_dev(int if_instance)
{
}

uint32 wpsap_osl_upnp_send_data(int if_instance, char *dataBuffer, uint32 dataLen, int type)
{
	return 0;
}

char* wpsap_osl_upnp_parse_msg(char *msg, int msg_len, int *len, int *type)
{
	return NULL;
}

char *get_ifname_by_wlmac(unsigned char *mac, char *name)
{
	return NULL;
}

int wps_osl_set_ifname(char *ifname)
{
	return 0;
}

int wps_osl_init(char *bssid)
{
	wpscli_rand_init();

	return 0;
}

void wps_osl_cleanup()
{
}

int wps_deauthenticate(unsigned char *bssid, unsigned char *sta_mac, int reason)
{
	return 0;
}

uint32 wait_for_eapol_packet(char* eapol_msg, uint32* len)
{
	return 0;
}

// Used on STA side only and called from inside wps core common code. It should
// return wps common code type of error codes 
uint32 send_eapol_packet(char *packet, uint32 len)
{
	brcm_wpscli_status status;
	uint32 retVal;

	status = wpscli_sta_eap_send_data_down(packet, len);

	// Convert to error code set defined by wps common code
	retVal = (status==WPS_STATUS_SUCCESS? WPS_SUCCESS : WPS_SEND_MSG_ERROR);

	return retVal;
}


//
// WPS AP side common code hookup functions
//
// Take the raw eapol packet containing the dest and source mac
// address already and send it out. 
uint32 wpsap_osl_eapol_send_data(char *dataBuffer, uint32 dataLen)
{
	wpscli_pktdisp_send_packet(dataBuffer, dataLen);

	return 0;
}

// wps hookup function to common code on WPS ap side 
// return 0 - Success, -1 - Failure
int wps_set_wps_ie(void *bcmdev, unsigned char *p_data, int length, unsigned int cmdtype)
{
	BOOL bRel;

	bRel = wpscli_set_wps_ie(p_data, length, cmdtype);
	if(bRel)
		return 0;  // Success
	
	return -1;  // Failure
}
