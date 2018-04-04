/**
 * Frame information structure
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wlc_frmutil.h 261155 2011-05-23 23:51:32Z $
*/
#ifndef _WLC_FRMUTIL_H_
#define _WLC_FRMUTIL_H_

struct wlc_frminfo {
	struct dot11_header *h;		/* pointer to d11 header */
	uchar *pbody;			/* pointer to body of frame */
	uint body_len;			/* length of body after d11 hdr */
	uint len;			/* length of first pkt in chain */
	uint totlen;			/* length of entire pkt chain */
	void *p;			/* pointer to pkt */
	d11rxhdr_t *rxh;		/* pointer to rxhdr */
	uint16 fc;			/* frame control field */
	uint16 type;			/* frame type */
	uint16 subtype;			/* frame subtype */
	uint8 prio;			/* frame priority */
	int ac;				/* access category of frame */
	bool apsd_eosp;			/* TRUE if apsd eosp set */
	bool wds;			/* TRUE for wds frame */
	bool qos;			/* TRUE for qos frame */
	bool ht;			/* TRUE for frame with HT control field */
	bool ismulti;			/* TRUE for multicast frame */
	bool isamsdu;			/* TRUE for amsdu frame */
	bool htma;			/* TRUE for ht frame with embedded mgmt act */
	bool istdls;			/* TRUE for frame recd thru dpt link */
	bool bssid_match;		/* TRUE if bssid match */
	int rx_wep;			/* wep frame */
	wsec_key_t *key;		/* key data */
	int key_index;			/* key index */
#ifndef LINUX_CRYPTO
	uint16 phase1[TKHASH_P1_KEY_SIZE/2];	/* phase1 data */
	uint8 phase2[TKHASH_P2_KEY_SIZE];	/* phase2 data */
#endif
	uint16 *pp1;			/* phase1 pointer */
	uint8 ividx;			/* IV index */
	uint16 iv16;			/* 16 bit IV */
	uint32 iv32;			/* 32 bit IV */
	uint16 WPA_auth;			/* WPA auth enabled */
	struct ether_header *eh;	/* pointer to ether header */
	struct ether_addr *sa;		/* pointer to source address */
	struct ether_addr *da;		/* pointer to dest address */
	uint8 plcp[D11_PHY_HDR_LEN];
	uint16 seq;			/* sequence number in host endian */
	wlc_d11rxhdr_t *wrxh;		/* pointer to rxhdr */
};


#endif /* _WLC_FRMUTIL_H_ */
