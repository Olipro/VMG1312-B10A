/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_osl.h 277453 2011-08-15 18:52:06Z $
 *
 * Description: Define the OSL APIs to be implemented by OSL components
 *
 */
#ifndef __WPSCLI_HOOKS_H__
#define __WPSCLI_HOOKS_H__

#ifdef WIN32
#include <winsock2.h>
#endif

#include <wpscli_api.h>
#include <wlioctl.h>
#include <stdio.h>

#ifdef TARGETOS_nucleus
#define printf printf_bsp
#endif

#define WPS_DUMP_BUF_LEN			(127 * 1024)
#define WLAN_SCAN_TIMEOUT			10		// in seconds
#define WLAN_CONNECTION_TIMEOUT		10		// in seconds
#define LOG_BUFFER_SIZE				4096
// 
// definitions related to packet dispatcher
//
// open/init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_open(const uint8 *peer_addr);				

// close/un-init packet dispatcher
brcm_wpscli_status wpscli_pktdisp_close(void);				

brcm_wpscli_status wpscli_set_peer_addr(const uint8 *peer_addr);

// waiting for packet. return immediately. received packet will come via the callback function
brcm_wpscli_status wpscli_pktdisp_wait_for_packet(char* buf, uint32* len, uint32 timeout, bool b_raw);

// send a packet
brcm_wpscli_status wpscli_pktdisp_send_packet(char *dataBuffer, uint32 dataLen);

//
// definitions related to timer
//
unsigned long wpscli_current_time(void);

void wpscli_sleep(unsigned long milli_seconds);


//
// definitions related to wl handler
//
brcm_wpscli_status wpscli_wlan_open(void);

brcm_wpscli_status wpscli_wlan_close(void);

// make a wlan connection. 
brcm_wpscli_status wpscli_wlan_connect(const char* ssid, uint32 wsec, const char *bssid,
	int num_chanspec, chanspec_t *chanspec);

// disconnect wlan connection
brcm_wpscli_status wpscli_wlan_disconnect(void);

brcm_wpscli_status wpscli_wlan_scan(wl_scan_results_t *ap_list, uint32 buf_size);

brcm_wpscli_status wpscli_wlh_open(const char *adapter_name, int is_virutal);

brcm_wpscli_status wpscli_wlh_close(void);

brcm_wpscli_status wpscli_wlh_get_adapter_mac(uint8 *adapter_mac);

brcm_wpscli_status wpscli_wlh_ioctl_set(int cmd, const char *data, ulong data_len);

brcm_wpscli_status wpscli_wlh_ioctl_get(int cmd, char *buf, ulong buf_len);

// 
// Define other OSL helper functions
//
// RAND initialization for brcmcrypto
void wpscli_rand_init(void);

uint16 wpscli_htons(uint16 v);

uint32 wpscli_htonl(uint32 v);

uint16 wpscli_ntohs(uint16 v);

uint32 wpscli_ntohl(uint32 v);

#ifdef _TUDEBUGTRACE
extern void wpscli_print_buf(char *text, unsigned char *buff, int buflen);
#endif


#endif  // __WPSCLI_HOOKS_H__
