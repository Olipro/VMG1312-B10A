/*
 * Copyright (C) 2012, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id:$
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "epivers.h"
#include "trace.h"
#include "dsp.h"
#include "wlu_api.h"
#include "proGas.h"
#include "pktEncodeIe.h"
#include "pktEncodeAnqp.h"
#include "pktEncodeHspotAnqp.h"
#include "pktEncodeWnm.h"
#include "pktDecodeAnqp.h"
#include "pktDecodeHspotAnqp.h"
#include "pktDecodeIe.h"
#include "tcp_srv.h"
#include "proto/bcmeth.h"
#include "proto/bcmevent.h"
#include "proto/802.11.h"
#include <bcmnvram.h>
#include <shutils.h>
#ifdef DSLCPE
#define printf(...) 
#endif

#define MAX_NVPARSE 16

#define BUILT_IN_FILTER

#define MAX_OI_LEN		15
#define MAX_RC_OI_NUM	16
#define MIH_PROTOCOL_ID	1
#define MAX_PLMN_NUM	16

typedef struct {
	uint32 pktflag;
	int ieLength;
	uint8 ieData[VNDR_IE_MAX_LEN];
} vendorIeT;

typedef struct {
	uint8 len;
	uint8 data[MAX_OI_LEN];
} OIDupleT;

/* enable testing mode */
#define TESTING_MODE	0

static pktAnqpPlmnT gTMobilePlmn = {"310", "026"};

/* home realm */
#define HOME_REALM	"example.com"

/* local buffer size */
#define BUFFER_SIZE				256

/* query request buffer size */
#define QUERY_REQUEST_BUFFER	1024

typedef struct
{
	/* wl interface */
	void *ifr;

	char prefix[16];

	/* DGAF disabled flag */
	bool isDgafDisabled;

	int gas_cb_delay;

	/* realm disabled flag */
	int isRealmDisabled;

	/* 3GPP disabled flag */
	int is3GppDisabled;

	/* hotspot vendor IEs added */
	vendorIeT vendorIeHSI;
	/* P2P vendor IEs added */
	vendorIeT vendorIeP2P;
    
    uint8 dialogToken;

	/* for testing */
	int isGasPauseForServerResponse;

	/* for testing */
	int testResponseSize;

	/* BSS transition request */
	uint8 *url;		/* session information URL */
	uint8 url_len;		/* session information URL length */
	uint8 req_token;

	/* interworking IE */
	bool iw_enabled;
	bool iw_isInternet;
	uint8 iw_ANT;
	uint8 iw_VG;
	uint8 iw_VT;
	bool iw_isHESSIDPresent;
	struct ether_addr iw_HESSID;

	/* advertisement protocol IE */
	bool ap_ANQPenabled;
	bool ap_MIHenabled;

	/* Roaming Consortium IE */
	OIDupleT rc_oi[MAX_RC_OI_NUM];
	uint8 rc_oiNum;

	/* 3G Cellular Network Information */
	pktAnqpPlmnT Plmn[MAX_PLMN_NUM];
	uint8 numPlmn;

	/* domain list */
	pktAnqpDomainNameT domain[MAX_DOMAIN];
	uint8 numDomain;

	/* add or remove hotspot 2.0 indication element */
	bool hs_ie_enabled;

	/* enable or disable p2p cross connect */
	bool p2p_ie_enabled;
	bool p2p_cross_enabled;

	uint8 numNaiRealm;

	uint8 ipv6_addr_type;
	uint8 ipv4_addr_type;
	bool l2_traffic_inspect;
	bool icmpv4_echo;

	uint8 nai_id;
	uint8 oper_id;
	uint8 venue_id;
	uint8 wanm_id;
	uint8 conn_id;
	uint8 ipa_id;
	uint8 opclass_id;
	uint8 home_id;
	uint8 nat_id;

	bool useDefaultANQPValue;
	bool useDefaultPLMNValue;
	bool emptyANQPInfo;
	bool useSim;
	bool disable_ANQP_response;
} hspotApT;

static hspotApT *hspotaps[MAX_WLIF_NUM];
static int hspotap_num = 0;
static hspotApT *current_hspotap = NULL;

/* tcp server for remote control */
static int tcpServerEnabled = 0;
static int tcpServerPort;

static int update_iw_ie(hspotApT *hspotap, bool disable);
static int update_rc_ie(hspotApT *hspotap);
static int update_ap_ie(hspotApT *hspotap);
/* --------------------------------------------------------------- */
int hspot_send_BTM_Req_frame(hspotApT *hspotap, struct ether_addr *da)
{
	int err = 0;
	dot11_bss_trans_req_t *transreq;
	wnm_url_t *url;
	uint16 len;

	len = DOT11_BSS_TRANS_REQ_LEN + hspotap->url_len + 1;
	transreq = (dot11_bss_trans_req_t *)malloc(len);
	if (transreq == NULL) {
		printf("malloc failed\n");
		return -1;
	}
	transreq->category = DOT11_ACTION_CAT_WNM;
	transreq->action = DOT11_WNM_ACTION_BSS_TRANS_REQ;
	transreq->token = hspotap->req_token;
	transreq->reqmode = DOT11_BSS_TRNS_REQMODE_ESS_DISASSOC_IMNT;
	transreq->disassoc_tmr = 0;
	transreq->validity_intrvl = 0;
	url = (wnm_url_t *)&transreq->data[0];
	url->len = hspotap->url_len;
	if (hspotap->url_len) {
		memcpy(url->data, hspotap->url, hspotap->url_len);
	}

	if (wl_wifiaction(hspotap->ifr, (uint32)hspotap, da, len, (uint8 *)transreq) < 0) {
		err = -1;
		TRACE(TRACE_ERROR, "wl_wifiaction failed\n");
	}

	hspotap->req_token++;
	if (hspotap->req_token == 0)
		hspotap->req_token = 1;

	free(transreq);
	return err;
}

static char *afStr(char *buf, int af, int length, uint8 fragmentId)
{
	switch (af)
	{
	case GAS_REQUEST_ACTION_FRAME:
		sprintf(buf, "request(%d)", length);
		break;
	case GAS_RESPONSE_ACTION_FRAME:
		sprintf(buf, "response(%d)", length);
		break;
	case GAS_COMEBACK_REQUEST_ACTION_FRAME:
		sprintf(buf, "comeback request(%d)", length);
		break;
	case GAS_COMEBACK_RESPONSE_ACTION_FRAME:
		sprintf(buf, "comeback response(%d, 0x%02x)", length, fragmentId);
		break;
	default:
		strcpy(buf, "unknown");
		break;
	}

	return buf;
}

void hspotPrintGasEvent(proGasEventT *event)
{
	char buf[32];

	if ((event->type == PRO_GAS_EVENT_TX &&
		event->tx.gasActionFrame == GAS_REQUEST_ACTION_FRAME) ||
		(event->type == PRO_GAS_EVENT_RX &&
		event->rx.gasActionFrame == GAS_REQUEST_ACTION_FRAME)) {
		printf("\npeer MAC     : %02X:%02X:%02X:%02X:%02X:%02X\n",
			event->peer.octet[0], event->peer.octet[1], event->peer.octet[2],
			event->peer.octet[3], event->peer.octet[4], event->peer.octet[5]);
		printf("dialog token : %d\n\n", event->dialogToken);
	}

	if (event->type == PRO_GAS_EVENT_QUERY_REQUEST) {
		TRACE(TRACE_DEBUG, "   PRO_GAS_EVENT_QUERY_REQUEST\n");
	}
	else if (event->type == PRO_GAS_EVENT_TX) {
		printf("%30s  ----->\n",
			afStr(buf, event->tx.gasActionFrame,
			event->tx.length, event->tx.fragmentId));
	}
	else if (event->type == PRO_GAS_EVENT_RX) {
		printf("%30s  <-----  %s\n", "",
			afStr(buf, event->rx.gasActionFrame,
			event->rx.length, event->rx.fragmentId));
	}
	else if (event->type == PRO_GAS_EVENT_STATUS) {
		char *str;

		switch (event->status.statusCode)
		{
		case DOT11_SC_SUCCESS:
			str = "SUCCESS";
			break;
		case DOT11_SC_FAILURE:
			str = "UNSPECIFIED";
			break;
		case DOT11_SC_ADV_PROTO_NOT_SUPPORTED:
			str = "ADVERTISEMENT_PROTOCOL_NOT_SUPPORTED";
			break;
		case DOT11_SC_NO_OUTSTAND_REQ:
			str = "NO_OUTSTANDING_REQUEST";
			break;
		case DOT11_SC_RSP_NOT_RX_FROM_SERVER:
			str = "RESPONSE_NOT_RECEIVED_FROM_SERVER";
			break;
		case DOT11_SC_TIMEOUT:
			str = "TIMEOUT";
			break;
		case DOT11_SC_QUERY_RSP_TOO_LARGE:
			str = "QUERY_RESPONSE_TOO_LARGE";
			break;
		case DOT11_SC_SERVER_UNREACHABLE:
			str = "SERVER_UNREACHABLE";
			break;
		case DOT11_SC_TRANSMIT_FAILURE:
			str = "TRANSMISSION_FAILURE";
			break;
		default:
			str = "unknown GAS status";
			break;
		}

		printf("\n\nstatus = %s\n", str);
	}
	else {
		TRACE(TRACE_DEBUG, "   unknown event type %d\n", event->type);
	}
}

static hspotApT *getHspotApByWlif(void *ifr)
{
	int i;

	for (i = 0; i < hspotap_num; i++) {
		if (hspotaps[i]->ifr == ifr)
			return hspotaps[i];
	}
	return NULL;
}

static hspotApT *getHspotApByIfname(char *ifname)
{
	int i;

	if (ifname == NULL)
		return hspotaps[0];

	for (i = 0; i < hspotap_num; i++) {
		if (strcmp(wl_ifname(hspotaps[i]->ifr), ifname) == 0)
			return hspotaps[i];
	}
	return NULL;
}

static void encodeAnqpCapabilityList(hspotApT *hspotap, pktEncodeT *pkt)
{
	uint8 buffer[BUFFER_SIZE];
	pktEncodeT vendor;
	uint8 vendorCap[] = {
		HSPOT_SUBTYPE_QUERY_LIST,
		HSPOT_SUBTYPE_CAPABILITY_LIST,
		HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME,
		HSPOT_SUBTYPE_WAN_METRICS,
		HSPOT_SUBTYPE_CONNECTION_CAPABILITY,
		HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY,
        HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION,
        HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS, 
        HSPOT_SUBTYPE_ANONYMOUS_NAI, 
        HSPOT_SUBTYPE_ICON_REQUEST, 
        HSPOT_SUBTYPE_ICON_BINARY_FILE };
        
    uint16 cap[] = {
		ANQP_ID_QUERY_LIST,
		ANQP_ID_CAPABILITY_LIST,
		ANQP_ID_VENUE_NAME_INFO,
		ANQP_ID_EMERGENCY_CALL_NUMBER_INFO,
		ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO,
		ANQP_ID_ROAMING_CONSORTIUM_LIST,
		ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO,
		ANQP_ID_NAI_REALM_LIST,
		ANQP_ID_G3PP_CELLULAR_NETWORK_INFO,
		ANQP_ID_AP_GEOSPATIAL_LOCATION,
		ANQP_ID_AP_CIVIC_LOCATION,
		ANQP_ID_AP_LOCATION_PUBLIC_ID_URI,
		ANQP_ID_DOMAIN_NAME_LIST,
		ANQP_ID_EMERGENCY_ALERT_ID_URI,
		ANQP_ID_EMERGENCY_NAI };

	if (hspotap->emptyANQPInfo) {
		pktEncodeAnqpCapabilityList(pkt, 0, 0, 0, 0);
		return;
	}

	/* encode vendor specific capability */
	pktEncodeInit(&vendor, sizeof(buffer), buffer);
	pktEncodeHspotAnqpCapabilityList(&vendor, sizeof(vendorCap) / sizeof(uint8), vendorCap);

	/* encode capability with vendor specific appended */
	pktEncodeAnqpCapabilityList(pkt, sizeof(cap) / sizeof(uint16), cap,
	pktEncodeLength(&vendor), pktEncodeBuf(&vendor));
}

static void encodeVenueName(hspotApT *hspotap, pktEncodeT *pkt)
{
#define WIFI_VENUE 	"Wi-Fi Alliance\n2989 Copper Road\nSanta Clara, CA 95051, USA"
#define ENGLISH	"eng"
#define CHINESE "chi"
	uint8 buffer[BUFFER_SIZE];
	pktEncodeT duple;
	uint8 chinese_venue_name[] = {0x57, 0x69, 0x2d, 0x46, 0x69, 0xe8, 0x81, 0x94,
		0xe7, 0x9b, 0x9f, 0xe5, 0xae, 0x9e, 0xe9, 0xaa,
		0x8c, 0xe5, 0xae, 0xa4, 0x0a, 0xe4, 0xba, 0x8c,
		0xe4, 0xb9, 0x9d, 0xe5, 0x85, 0xab, 0xe4, 0xb9,
		0x9d, 0xe5, 0xb9, 0xb4, 0xe5, 0xba, 0x93, 0xe6,
		0x9f, 0x8f, 0xe8, 0xb7, 0xaf, 0x0a, 0xe5, 0x9c,
		0xa3, 0xe5, 0x85, 0x8b, 0xe6, 0x8b, 0x89, 0xe6,
		0x8b, 0x89, 0x2c, 0x20, 0xe5, 0x8a, 0xa0, 0xe5,
		0x88, 0xa9, 0xe7, 0xa6, 0x8f, 0xe5, 0xb0, 0xbc,
		0xe4, 0xba, 0x9a, 0x39, 0x35, 0x30, 0x35, 0x31,
		0x2c, 0x20, 0xe7, 0xbe, 0x8e, 0xe5, 0x9b, 0xbd};

	pktEncodeInit(&duple, sizeof(buffer), buffer);

	if (hspotap->emptyANQPInfo) {
		pktEncodeAnqpVenueName(pkt, 0, 0,
			pktEncodeLength(&duple), pktEncodeBuf(&duple));
		return;
	}

	if (hspotap->venue_id == 1) {
		pktEncodeAnqpVenueDuple(&duple, strlen(ENGLISH), ENGLISH,
			strlen(WIFI_VENUE), WIFI_VENUE);
		pktEncodeAnqpVenueDuple(&duple, strlen(CHINESE), CHINESE,
			sizeof(chinese_venue_name), (char *)chinese_venue_name);
		pktEncodeAnqpVenueName(pkt, VENUE_BUSINESS, 8,
			pktEncodeLength(&duple), pktEncodeBuf(&duple));
	} else {
		pktEncodeAnqpVenueName(pkt, 0, 0,
			pktEncodeLength(&duple), pktEncodeBuf(&duple));
	}
}

static void encodeNetworkAuthenticationType(hspotApT *hspotap, pktEncodeT *pkt)
{
	#define URL 	"https://tandc-server.wi-fi.org"
	uint8 buffer[BUFFER_SIZE];
	pktEncodeT network;

	pktEncodeInit(&network, sizeof(buffer), buffer);
	if (!hspotap->emptyANQPInfo) {
		if (hspotap->nat_id == 1) {
			pktEncodeAnqpNetworkAuthenticationUnit(&network,
				NATI_ACCEPTANCE_OF_TERMS_CONDITIONS, 0, 0);
			pktEncodeAnqpNetworkAuthenticationUnit(&network,
				NATI_HTTP_HTTPS_REDIRECTION, strlen(URL), URL);
            pktEncodeAnqpNetworkAuthenticationUnit(&network,
                    NATI_ONLINE_ENROLLMENT_SUPPORTED, 0, 0);
		}
	}
	pktEncodeAnqpNetworkAuthenticationType(pkt,
		pktEncodeLength(&network), pktEncodeBuf(&network));
}

static void encodeRoamingConsortium(hspotApT *hspotap, pktEncodeT *pkt)
{
	uint8 buffer[BUFFER_SIZE];
	pktEncodeT oi;

	pktEncodeInit(&oi, sizeof(buffer), buffer);
	if (!hspotap->emptyANQPInfo) {
		if (hspotap->useDefaultANQPValue) {
			pktEncodeAnqpOiDuple(&oi, strlen(WFA_OUI), (uint8 *)WFA_OUI);
			pktEncodeAnqpOiDuple(&oi, 3, (uint8 *)"\x00\x50\xf2");
		}
		else {
			int i;
			for (i = 0; i < hspotap->rc_oiNum; i++) {
				pktEncodeAnqpOiDuple(&oi, hspotap->rc_oi[i].len,
					hspotap->rc_oi[i].data);
			}
		}
	}
	pktEncodeAnqpRoamingConsortium(pkt,
		pktEncodeLength(&oi), pktEncodeBuf(&oi));
}

static void encodeIpAddressType(hspotApT *hspotap, pktEncodeT *pkt)
{
	if (hspotap->emptyANQPInfo)
		pktEncodeAnqpIpTypeAvailability(pkt,
			IPA_IPV6_NOT_AVAILABLE, IPA_IPV4_NOT_AVAILABLE);
	else if (hspotap->ipa_id == 1)
		pktEncodeAnqpIpTypeAvailability(pkt,
			IPA_IPV6_NOT_AVAILABLE, IPA_IPV4_SINGLE_NAT);
	else
		pktEncodeAnqpIpTypeAvailability(pkt,
			hspotap->ipv6_addr_type, hspotap->ipv4_addr_type);
}

static void encodeNaiRealmList(hspotApT *hspotap, pktEncodeT *pkt)
{
	#define MAIL		"mail.example.com"
	#define CISCO		"cisco.com"
	#define WIFI		"wi-fi.org"
	#define EXAMPLE4	"example.com"
	uint8 ttlsAuthBuf[BUFFER_SIZE];
	pktEncodeT ttlsAuth;
	uint8 ttlsEapBuf[BUFFER_SIZE];
	pktEncodeT ttlsEap;
	uint8 tlsAuthBuf[BUFFER_SIZE];
	pktEncodeT tlsAuth;
	uint8 tlsEapBuf[BUFFER_SIZE];
	pktEncodeT tlsEap;
	uint8 simAuthBuf[BUFFER_SIZE];
	pktEncodeT simAuth;
	uint8 simEapBuf[BUFFER_SIZE];
	pktEncodeT simEap;
	uint8 realmBuf[BUFFER_SIZE];
	pktEncodeT realm;
	uint8 inner;
	uint8 credential;
	int numRealm;

	pktEncodeInit(&realm, sizeof(realmBuf), realmBuf);
	numRealm = 0;

	if ((!hspotap->isRealmDisabled) && (!hspotap->emptyANQPInfo)) {
		if (hspotap->nai_id == 1) {
			/* TTLS - MSCHAPv2, username & password */
			pktEncodeInit(&ttlsAuth, sizeof(ttlsAuthBuf), ttlsAuthBuf);
			inner = REALM_MSCHAPV2;
			credential = REALM_USERNAME_PASSWORD;
			pktEncodeAnqpAuthenticationSubfield(&ttlsAuth,
				REALM_NON_EAP_INNER_AUTHENTICATION, sizeof(inner), &inner);
			pktEncodeAnqpAuthenticationSubfield(&ttlsAuth,
				REALM_CREDENTIAL, sizeof(credential), &credential);
			pktEncodeInit(&ttlsEap, sizeof(ttlsEapBuf), ttlsEapBuf);
			pktEncodeAnqpEapMethodSubfield(&ttlsEap, REALM_EAP_TTLS,
				2, pktEncodeLength(&ttlsAuth), pktEncodeBuf(&ttlsAuth));

			/* TLS - certificate */
			pktEncodeInit(&tlsAuth, sizeof(tlsAuthBuf), tlsAuthBuf);
			credential = REALM_CERTIFICATE;
			pktEncodeAnqpAuthenticationSubfield(&tlsAuth,
				REALM_CREDENTIAL, sizeof(credential), &credential);
			pktEncodeInit(&tlsEap, sizeof(tlsEapBuf), tlsEapBuf);
			pktEncodeAnqpEapMethodSubfield(&tlsEap, REALM_EAP_TLS,
				1, pktEncodeLength(&tlsAuth), pktEncodeBuf(&tlsAuth));

			/* SIM */
			pktEncodeInit(&simAuth, sizeof(simAuthBuf), simAuthBuf);
			credential = REALM_SIM;
			pktEncodeAnqpAuthenticationSubfield(&simAuth,
				REALM_CREDENTIAL, sizeof(credential), &credential);
			pktEncodeInit(&simEap, sizeof(simEapBuf), simEapBuf);
			pktEncodeAnqpEapMethodSubfield(&simEap, REALM_EAP_SIM,
				1, pktEncodeLength(&simAuth), pktEncodeBuf(&simAuth));

			/* mail */
			if (!hspotap->useSim) {
				pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
					strlen(MAIL), (uint8 *)MAIL, 1,
					pktEncodeLength(&ttlsEap), pktEncodeBuf(&ttlsEap));
				numRealm++;
			}

			/* cisco */
			pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
				strlen(CISCO), (uint8 *)CISCO, 1,
				pktEncodeLength(&ttlsEap), pktEncodeBuf(&ttlsEap));
			numRealm++;

			/* append EAP-TLS to EAP_TTLS buffer */
			pktEncodeAnqpEapMethodSubfield(&ttlsEap, REALM_EAP_TLS,
				1, pktEncodeLength(&tlsAuth), pktEncodeBuf(&tlsAuth));

			/* wifi */
			pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
				strlen(WIFI), (uint8 *)WIFI, 2,
				pktEncodeLength(&ttlsEap), pktEncodeBuf(&ttlsEap));
			numRealm++;

			/* example4 */
			pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
				strlen(EXAMPLE4), (uint8 *)EXAMPLE4, 1,
				pktEncodeLength(&tlsEap), pktEncodeBuf(&tlsEap));
			numRealm++;

			/* sim */
			if (hspotap->useSim) {
				pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
					strlen(MAIL), (uint8 *)MAIL, 1,
					pktEncodeLength(&simEap), pktEncodeBuf(&simEap));
				numRealm++;
			}
		} else if (hspotap->nai_id == 2) {
			/* TTLS - MSCHAPv2, username & password */
			pktEncodeInit(&ttlsAuth, sizeof(ttlsAuthBuf), ttlsAuthBuf);
			inner = REALM_MSCHAPV2;
			credential = REALM_USERNAME_PASSWORD;
			pktEncodeAnqpAuthenticationSubfield(&ttlsAuth,
				REALM_NON_EAP_INNER_AUTHENTICATION, sizeof(inner), &inner);
			pktEncodeAnqpAuthenticationSubfield(&ttlsAuth,
				REALM_CREDENTIAL, sizeof(credential), &credential);
			pktEncodeInit(&ttlsEap, sizeof(ttlsEapBuf), ttlsEapBuf);
			pktEncodeAnqpEapMethodSubfield(&ttlsEap, REALM_EAP_TTLS,
				2, pktEncodeLength(&ttlsAuth), pktEncodeBuf(&ttlsAuth));

			/* wifi */
			pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
				strlen(WIFI), (uint8 *)WIFI, 1,
				pktEncodeLength(&ttlsEap), pktEncodeBuf(&ttlsEap));
			numRealm++;
		} else if (hspotap->nai_id == 3) {
			/* TTLS - MSCHAPv2, username & password */
			pktEncodeInit(&ttlsAuth, sizeof(ttlsAuthBuf), ttlsAuthBuf);
			inner = REALM_MSCHAPV2;
			credential = REALM_USERNAME_PASSWORD;
			pktEncodeAnqpAuthenticationSubfield(&ttlsAuth,
				REALM_NON_EAP_INNER_AUTHENTICATION, sizeof(inner), &inner);
			pktEncodeAnqpAuthenticationSubfield(&ttlsAuth,
				REALM_CREDENTIAL, sizeof(credential), &credential);
			pktEncodeInit(&ttlsEap, sizeof(ttlsEapBuf), ttlsEapBuf);
			pktEncodeAnqpEapMethodSubfield(&ttlsEap, REALM_EAP_TTLS,
				2, pktEncodeLength(&ttlsAuth), pktEncodeBuf(&ttlsAuth));

			/* TLS - certificate */
			pktEncodeInit(&tlsAuth, sizeof(tlsAuthBuf), tlsAuthBuf);
			credential = REALM_CERTIFICATE;
			pktEncodeAnqpAuthenticationSubfield(&tlsAuth,
				REALM_CREDENTIAL, sizeof(credential), &credential);
			pktEncodeInit(&tlsEap, sizeof(tlsEapBuf), tlsEapBuf);
			pktEncodeAnqpEapMethodSubfield(&tlsEap, REALM_EAP_TLS,
				1, pktEncodeLength(&tlsAuth), pktEncodeBuf(&tlsAuth));

			/* cisco */
			pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
				strlen(CISCO), (uint8 *)CISCO, 1,
				pktEncodeLength(&ttlsEap), pktEncodeBuf(&ttlsEap));
			numRealm++;

			/* append EAP-TLS to EAP_TTLS buffer */
			pktEncodeAnqpEapMethodSubfield(&ttlsEap, REALM_EAP_TLS,
				1, pktEncodeLength(&tlsAuth), pktEncodeBuf(&tlsAuth));

			/* wifi */
			pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
				strlen(WIFI), (uint8 *)WIFI, 2,
				pktEncodeLength(&ttlsEap), pktEncodeBuf(&ttlsEap));
			numRealm++;

			/* example4 */
			pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
				strlen(EXAMPLE4), (uint8 *)EXAMPLE4, 1,
				pktEncodeLength(&tlsEap), pktEncodeBuf(&tlsEap));
			numRealm++;
		} else if (hspotap->nai_id == 4) {
			/* TTLS - MSCHAPv2, username & password */
			pktEncodeInit(&ttlsAuth, sizeof(ttlsAuthBuf), ttlsAuthBuf);
			inner = REALM_MSCHAPV2;
			credential = REALM_USERNAME_PASSWORD;
			pktEncodeAnqpAuthenticationSubfield(&ttlsAuth,
				REALM_NON_EAP_INNER_AUTHENTICATION, sizeof(inner), &inner);
			pktEncodeAnqpAuthenticationSubfield(&ttlsAuth,
				REALM_CREDENTIAL, sizeof(credential), &credential);
			pktEncodeInit(&ttlsEap, sizeof(ttlsEapBuf), ttlsEapBuf);
			pktEncodeAnqpEapMethodSubfield(&ttlsEap, REALM_EAP_TTLS,
				2, pktEncodeLength(&ttlsAuth), pktEncodeBuf(&ttlsAuth));

			/* TLS - certificate */
			pktEncodeInit(&tlsAuth, sizeof(tlsAuthBuf), tlsAuthBuf);
			credential = REALM_CERTIFICATE;
			pktEncodeAnqpAuthenticationSubfield(&tlsAuth,
				REALM_CREDENTIAL, sizeof(credential), &credential);

			/* append EAP-TLS to EAP_TTLS buffer */
			pktEncodeAnqpEapMethodSubfield(&ttlsEap, REALM_EAP_TLS,
				1, pktEncodeLength(&tlsAuth), pktEncodeBuf(&tlsAuth));

			/* mail */
			pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
				strlen(MAIL), (uint8 *)MAIL, 2,
				pktEncodeLength(&ttlsEap), pktEncodeBuf(&ttlsEap));

			numRealm++;
		}
		else if (hspotap->numNaiRealm) {

		}
	}

	pktEncodeAnqpNaiRealm(pkt, numRealm,
		pktEncodeLength(&realm), pktEncodeBuf(&realm));
}

static void encode3GppCellularNetwork(hspotApT *hspotap, pktEncodeT *pkt)
{
	uint8 plmnBuf[BUFFER_SIZE];
	pktEncodeT plmn;
	int plmnCount = 0;

	pktEncodeInit(&plmn, BUFFER_SIZE, plmnBuf);
	if ((!hspotap->is3GppDisabled) && (!hspotap->emptyANQPInfo)) {
		if (hspotap->useDefaultPLMNValue) {
			plmnCount++;
			pktEncodeAnqpPlmn(&plmn, gTMobilePlmn.mcc, gTMobilePlmn.mnc);
			plmnCount++;
			pktEncodeAnqpPlmn(&plmn, "208", "00");
			plmnCount++;
			pktEncodeAnqpPlmn(&plmn, "208", "01");
			plmnCount++;
			pktEncodeAnqpPlmn(&plmn, "208", "02");
			plmnCount++;
			pktEncodeAnqpPlmn(&plmn, "450", "02");
			plmnCount++;
			pktEncodeAnqpPlmn(&plmn, "450", "04");
		}
		else {
			int i;
			for (i = 0; i < hspotap->numPlmn; i++) {
				if (pktEncodeAnqpPlmn(&plmn, hspotap->Plmn[i].mcc,
					hspotap->Plmn[i].mnc))
					plmnCount++;
			}
		}
	}
	printf("numPlmn %d, %d, %d, %d\n", hspotap->numPlmn,
		plmnCount, hspotap->is3GppDisabled, hspotap->emptyANQPInfo);
	pktEncodeAnqp3GppCellularNetwork(pkt,
		plmnCount, pktEncodeLength(&plmn), pktEncodeBuf(&plmn));
}

static void encodeDomainNameList(hspotApT *hspotap, pktEncodeT *pkt)
{
	#define WIFI		"wi-fi.org"
	uint8 buffer[BUFFER_SIZE];
	pktEncodeT name;

	pktEncodeInit(&name, sizeof(buffer), buffer);
	if (!hspotap->emptyANQPInfo) {
		if (hspotap->useDefaultANQPValue) {
			pktEncodeAnqpDomainName(&name, strlen(WIFI), WIFI);
		}
		else {
			if (hspotap->numDomain) {
				int i;
				for (i = 0; i < hspotap->numDomain; i++) {
					pktEncodeAnqpDomainName(&name, hspotap->domain[i].len,
						hspotap->domain[i].name);
				}
			}
		}
	}
	pktEncodeAnqpDomainNameList(pkt,
		pktEncodeLength(&name), pktEncodeBuf(&name));
}

static void encodeHspotCapabilityList(hspotApT *hspotap, pktEncodeT *pkt)
{
	uint8 cap[] = {
		HSPOT_SUBTYPE_QUERY_LIST,
		HSPOT_SUBTYPE_CAPABILITY_LIST,
		HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME,
		HSPOT_SUBTYPE_WAN_METRICS,
		HSPOT_SUBTYPE_CONNECTION_CAPABILITY,
		HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY,
        HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION,
        HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS, 
        HSPOT_SUBTYPE_ANONYMOUS_NAI, 
        HSPOT_SUBTYPE_ICON_REQUEST, 
        HSPOT_SUBTYPE_ICON_BINARY_FILE };
	if (hspotap->emptyANQPInfo)
		pktEncodeHspotAnqpCapabilityList(pkt, 0, cap);
	else
		pktEncodeHspotAnqpCapabilityList(pkt, sizeof(cap) / sizeof(uint8), cap);
}

static void encodeHspotOperatingClassIndication(hspotApT *hspotap, pktEncodeT *pkt)
{
	uint8 opClass [2] = {81, 115};
	uint8 opClass1 [1] = {81};
	uint8 opClass2 [1] = {115};

	if (hspotap->emptyANQPInfo)
		pktEncodeHspotAnqpOperatingClassIndication(pkt, 0, opClass);
	else if (hspotap->opclass_id == 3)
		pktEncodeHspotAnqpOperatingClassIndication(pkt, sizeof(opClass), opClass);
	else if (hspotap->opclass_id == 2)
		pktEncodeHspotAnqpOperatingClassIndication(pkt, sizeof(opClass2), opClass2);
	else if (hspotap->opclass_id == 1)
		pktEncodeHspotAnqpOperatingClassIndication(pkt, sizeof(opClass1), opClass1);
	else
		pktEncodeHspotAnqpOperatingClassIndication(pkt, 0, opClass);
}

static void encodeOperatorFriendlyName(hspotApT *hspotap, pktEncodeT *pkt)
{
	#define ENGLISH					"eng"
	#define ENGLISH_FRIENDLY_NAME 	"Wi-Fi Alliance"
	#define CHINESE					"chi"
	#define CHINESE_FRIENDLY_NAME 	"\x57\x69\x2d\x46\x69\xe8\x81\x94\xe7\x9b\x9f"

	uint8 buffer[BUFFER_SIZE];
	pktEncodeT name;

	pktEncodeInit(&name, sizeof(buffer), buffer);
	if (!hspotap->emptyANQPInfo) {
		if (hspotap->oper_id == 1) {
			pktEncodeHspotAnqpOperatorNameDuple(&name,
				strlen(ENGLISH), ENGLISH, strlen(ENGLISH_FRIENDLY_NAME),
				ENGLISH_FRIENDLY_NAME);
			pktEncodeHspotAnqpOperatorNameDuple(&name,
				strlen(CHINESE), CHINESE, strlen(CHINESE_FRIENDLY_NAME),
				CHINESE_FRIENDLY_NAME);
		}
	}
	pktEncodeHspotAnqpOperatorFriendlyName(pkt,
		pktEncodeLength(&name), pktEncodeBuf(&name));
}

static void encodeHspotOsuProviders(hspotApT *hspotap, pktEncodeT *pkt) 
{
	uint8 nameBuf1[BUFFER_SIZE];
	pktEncodeT name1;
	uint8 nameBuf2[BUFFER_SIZE];
	pktEncodeT name2;
	uint8 iconBuf[BUFFER_SIZE];
	pktEncodeT icon;
	uint8 osuBuf[BUFFER_SIZE];
	uint8 descBuf1[BUFFER_SIZE];
	pktEncodeT desc1;
	uint8 descBuf2[BUFFER_SIZE];
	pktEncodeT desc2;
	pktEncodeT osu;
	uint8 soap = HSPOT_OSU_METHOD_SOAP_XML;
	uint8 omadm = HSPOT_OSU_METHOD_OMA_DM;

    pktEncodeInit(&name1, BUFFER_SIZE, nameBuf1); 
    pktEncodeHspotAnqpOperatorNameDuple(&name1, 2, "EN", 9, "provider1"); 

	pktEncodeInit(&name2, BUFFER_SIZE, nameBuf2);
	pktEncodeHspotAnqpOperatorNameDuple(&name2, 2, "EN", 9, "provider2");
    
	pktEncodeInit(&icon, BUFFER_SIZE, iconBuf);
	pktEncodeHspotAnqpIconMetadata(&icon, 1, 2, "EN",
		5, (uint8 *)"image", 13, (uint8 *)"iconfile1.txt");
	pktEncodeHspotAnqpIconMetadata(&icon, 3, 4, "EN",
		5, (uint8 *)"image", 13, (uint8 *)"iconfile2.txt");

	pktEncodeInit(&desc1, BUFFER_SIZE, descBuf1);
	pktEncodeHspotAnqpOperatorNameDuple(&desc1, 2, "EN", 12, "SOAP-XML OSU");
	pktEncodeInit(&desc2, BUFFER_SIZE, descBuf2);
	pktEncodeHspotAnqpOperatorNameDuple(&desc2, 2, "EN", 10, "OMA-DM OSU");
	
	pktEncodeInit(&osu, BUFFER_SIZE, osuBuf);
	pktEncodeHspotAnqpOsuProvider(&osu,
		pktEncodeLength(&name1), pktEncodeBuf(&name1),
		6, (uint8 *)"myuri1", 1, &soap,
		pktEncodeLength(&icon), pktEncodeBuf(&icon),
		strlen(HOME_REALM), (uint8 *)HOME_REALM,
		pktEncodeLength(&desc1), pktEncodeBuf(&desc1));
	pktEncodeHspotAnqpOsuProvider(&osu,
		pktEncodeLength(&name2), pktEncodeBuf(&name2),
		6, (uint8 *)"myuri2", 1, &omadm,
		pktEncodeLength(&icon), pktEncodeBuf(&icon),
		strlen(HOME_REALM), (uint8 *)HOME_REALM,
		pktEncodeLength(&desc2), pktEncodeBuf(&desc2));

	pktEncodeHspotAnqpOsuProviderList(pkt, 0, 0, 0, 0,
		pktEncodeLength(&osu), pktEncodeBuf(&osu));
}

static void encodeHspotAnonymousNai(hspotApT *hspotap, pktEncodeT *pkt) 
{ 
    char *nai = "anonymous.com"; 
    pktEncodeHspotAnqpAnonymousNai(pkt, strlen(nai), (uint8 *)nai); 
}

static void encodeWanMetrics(hspotApT *hspotap, pktEncodeT *pkt)
{
	if (hspotap->emptyANQPInfo)
		pktEncodeHspotAnqpWanMetrics(pkt,
			0, 0, 0, 0, 0, 0, 0, 0);
	else if (hspotap->wanm_id == 1)
		pktEncodeHspotAnqpWanMetrics(pkt,
			HSPOT_WAN_LINK_UP, HSPOT_WAN_NOT_SYMMETRIC_LINK, HSPOT_WAN_NOT_AT_CAPACITY,
			2500, 384, 0, 0, 0);
	else
		pktEncodeHspotAnqpWanMetrics(pkt,
			0, 0, 0, 0, 0, 0, 0, 0);
}

static void encodeConnectionCapability(hspotApT *hspotap, pktEncodeT *pkt)
{
	uint8 buffer[BUFFER_SIZE];
	pktEncodeT cap;

	pktEncodeInit(&cap, sizeof(buffer), buffer);
	if (!hspotap->emptyANQPInfo) {
		if (hspotap->conn_id == 1) {
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x1, 0x0, HSPOT_CC_STATUS_CLOSED);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x6, 0x14, HSPOT_CC_STATUS_OPEN);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x6, 0x16, HSPOT_CC_STATUS_CLOSED);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x6, 0x50, HSPOT_CC_STATUS_OPEN);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x6, 0x1bb, HSPOT_CC_STATUS_OPEN);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x6, 0x6bb, HSPOT_CC_STATUS_CLOSED);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x6, 0x13c4, HSPOT_CC_STATUS_CLOSED);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x11, 0x1f4, HSPOT_CC_STATUS_OPEN);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x11, 0x13c4, HSPOT_CC_STATUS_CLOSED);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x11, 0x1194, HSPOT_CC_STATUS_OPEN);
			pktEncodeHspotAnqpProtoPortTuple(&cap,
				0x32, 0x0, HSPOT_CC_STATUS_OPEN);
		}
	}
	pktEncodeHspotAnqpConnectionCapability(pkt,
		pktEncodeLength(&cap), pktEncodeBuf(&cap));
}

static void encodeNaiHomeRealmQuery(hspotApT *hspotap, pktEncodeT *pkt)
{
	uint8 buffer[BUFFER_SIZE];
	pktEncodeT name;
	int count = 0;

	pktEncodeInit(&name, sizeof(buffer), buffer);
	if (!hspotap->emptyANQPInfo) {
		if (hspotap->home_id == 1) {
			pktEncodeHspotAnqpNaiHomeRealmName(&name, REALM_ENCODING_RFC4282,
				strlen(HOME_REALM), HOME_REALM);
			count++;
		}
	}
	pktEncodeHspotAnqpNaiHomeRealmQuery(pkt, count,
		pktEncodeLength(&name), pktEncodeBuf(&name));
}

static void encodeHomeRealm(hspotApT *hspotap, pktEncodeT *pkt)
{
	uint8 tlsAuthBuf[BUFFER_SIZE];
	pktEncodeT tlsAuth;
	uint8 tlsEapBuf[BUFFER_SIZE];
	pktEncodeT tlsEap;
	uint8 realmBuf[BUFFER_SIZE];
	pktEncodeT realm;
	uint8 credential;

	if (hspotap->emptyANQPInfo) {
		pktEncodeAnqpNaiRealm(pkt, 0, 0, realmBuf);
		return;
	}

	/* TLS - certificate */
	pktEncodeInit(&tlsAuth, sizeof(tlsAuthBuf), tlsAuthBuf);
	credential = REALM_CERTIFICATE;
	pktEncodeAnqpAuthenticationSubfield(&tlsAuth,
		REALM_CREDENTIAL, sizeof(credential), &credential);
	pktEncodeInit(&tlsEap, sizeof(tlsEapBuf), tlsEapBuf);
	pktEncodeAnqpEapMethodSubfield(&tlsEap, REALM_EAP_TLS,
		1, pktEncodeLength(&tlsAuth), pktEncodeBuf(&tlsAuth));

	pktEncodeInit(&realm, sizeof(realmBuf), realmBuf);

	/* example */
	pktEncodeAnqpNaiRealmData(&realm, REALM_ENCODING_RFC4282,
		strlen(HOME_REALM), (uint8 *)HOME_REALM, 1,
		pktEncodeLength(&tlsEap), pktEncodeBuf(&tlsEap));
	pktEncodeAnqpNaiRealm(pkt, 1,
		pktEncodeLength(&realm), pktEncodeBuf(&realm));
}

static void processQueryRequest(hspotApT *hspotap,
	proGasT *gas, int len, uint8 *data)
{
	int bufferSize = QUERY_REQUEST_BUFFER;
	uint8 *buffer;
	pktDecodeT pkt;
	pktDecodeAnqpT anqp;
	pktEncodeT rsp;
	int responseSize;

	TRACE_HEX_DUMP(TRACE_DEBUG, "query request", len, data);

	if (hspotap->testResponseSize > QUERY_REQUEST_BUFFER) {
		bufferSize = hspotap->testResponseSize;
	}

	buffer = malloc(bufferSize);
	if (buffer == 0)
		return;

	memset(buffer, 0, bufferSize);

	pktEncodeInit(&rsp, bufferSize, buffer);

	/* decode ANQP */
	pktDecodeInit(&pkt, len, data);
	pktDecodeAnqp(&pkt, &anqp);

	/* decode query list and encode response */
	if (anqp.anqpQueryListLength > 0) {
		pktDecodeT ie;
		pktAnqpQueryListT queryList;
		int i;

		pktDecodeInit(&ie, anqp.anqpQueryListLength, anqp.anqpQueryListBuffer);
		if (pktDecodeAnqpQueryList(&ie, &queryList))
			pktDecodeAnqpQueryListPrint(&queryList);
		else
			printf("failed to decode query list\n");

		for (i = 0; i < queryList.queryLen; i++) {
			switch (queryList.queryId[i])
			{
			case ANQP_ID_QUERY_LIST:
				break;
			case ANQP_ID_CAPABILITY_LIST:
				encodeAnqpCapabilityList(hspotap, &rsp);
				break;
			case ANQP_ID_VENUE_NAME_INFO:
				encodeVenueName(hspotap, &rsp);
				break;
			case ANQP_ID_EMERGENCY_CALL_NUMBER_INFO:
				break;
			case ANQP_ID_NETWORK_AUTHENTICATION_TYPE_INFO:
				encodeNetworkAuthenticationType(hspotap, &rsp);
				break;
			case ANQP_ID_ROAMING_CONSORTIUM_LIST:
				encodeRoamingConsortium(hspotap, &rsp);
				break;
			case ANQP_ID_IP_ADDRESS_TYPE_AVAILABILITY_INFO:
				encodeIpAddressType(hspotap, &rsp);
				break;
			case ANQP_ID_NAI_REALM_LIST:
				encodeNaiRealmList(hspotap, &rsp);
				break;
			case ANQP_ID_G3PP_CELLULAR_NETWORK_INFO:
				encode3GppCellularNetwork(hspotap, &rsp);
				break;
			case ANQP_ID_AP_GEOSPATIAL_LOCATION:
				break;
			case ANQP_ID_AP_CIVIC_LOCATION:
				break;
			case ANQP_ID_AP_LOCATION_PUBLIC_ID_URI:
				break;
			case ANQP_ID_DOMAIN_NAME_LIST:
				encodeDomainNameList(hspotap, &rsp);
				break;
			case ANQP_ID_EMERGENCY_ALERT_ID_URI:
				break;
			case ANQP_ID_EMERGENCY_NAI:
				break;
			case ANQP_ID_VENDOR_SPECIFIC_LIST:
				break;
			default:
				break;
			}
		}
	}

	if (anqp.hspot.queryListLength > 0) {
		pktDecodeT ie;
		pktHspotAnqpQueryListT queryList;
		int i;

		pktDecodeInit(&ie, anqp.hspot.queryListLength, anqp.hspot.queryListBuffer);
		if (pktDecodeHspotAnqpQueryList(&ie, &queryList))
			pktDecodeHspotAnqpQueryListPrint(&queryList);
		else
			printf("failed to decode hotspot query list\n");

		for (i = 0; i < queryList.queryLen; i++) {
			switch (queryList.queryId[i])
			{
			case HSPOT_SUBTYPE_QUERY_LIST:
				break;
			case HSPOT_SUBTYPE_CAPABILITY_LIST:
				encodeHspotCapabilityList(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_OPERATOR_FRIENDLY_NAME:
				encodeOperatorFriendlyName(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_WAN_METRICS:
				encodeWanMetrics(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_CONNECTION_CAPABILITY:
				encodeConnectionCapability(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_NAI_HOME_REALM_QUERY:
				encodeNaiHomeRealmQuery(hspotap, &rsp);
				break;
			case HSPOT_SUBTYPE_OPERATING_CLASS_INDICATION:
				encodeHspotOperatingClassIndication(hspotap, &rsp);
				break;
            case HSPOT_SUBTYPE_ONLINE_SIGNUP_PROVIDERS:
                encodeHspotOsuProviders(hspotap, &rsp); 
                break; 
            case HSPOT_SUBTYPE_ANONYMOUS_NAI:
                encodeHspotAnonymousNai(hspotap, &rsp); 
                break;
			default:
				break;
			}
		}
	}

	if (anqp.hspot.naiHomeRealmQueryLength > 0) {
		if (!hspotap->isRealmDisabled) {
			pktDecodeT ie;
			pktHspotAnqpNaiHomeRealmQueryT realm;
			int i;
			int isMatch = FALSE;

			pktDecodeInit(&ie, anqp.hspot.naiHomeRealmQueryLength,
				anqp.hspot.naiHomeRealmQueryBuffer);
			if (pktDecodeHspotAnqpNaiHomeRealmQuery(&ie, &realm))
				pktDecodeHspotAnqpNaiHomeRealmQueryPrint(&realm);
			else
				printf("failed to decode hotspot home realm query\n");

			for (i = 0; i < realm.count; i++) {
				if (strcmp(realm.data[i].name, HOME_REALM) == 0)
					isMatch = TRUE;
			}

			if (isMatch)
				encodeHomeRealm(hspotap, &rsp);
			else
				pktEncodeAnqpNaiRealm(&rsp, 0, 0, 0);
		}
		else {
			pktEncodeAnqpNaiRealm(&rsp, 0, 0, 0);
		}
	}
    
    if (anqp.hspot.iconRequestLength > 0)
    { 
        uint8 iconData[256]; 
        int i;  
        TRACE_HEX_DUMP(TRACE_PRINTF, "icon request", 
                                    anqp.hspot.iconRequestLength,
                                    anqp.hspot.iconRequestBuffer); 
    
        for (i = 0; i < (int)sizeof(iconData); i++) 
        iconData[i] = i; 
        pktEncodeHspotAnqpIconBinaryFile(&rsp, HSPOT_ICON_STATUS_SUCCESS, 
                                         4, (uint8 *)"type",
                                         sizeof(iconData), iconData); 
    }
	responseSize = pktEncodeLength(&rsp);

	/* pad response to testResponseSize */
	if (hspotap->testResponseSize > responseSize) {
		responseSize = hspotap->testResponseSize;
	}

	printf("%30s  <-----  query response %d bytes %d\n", "",
		responseSize, hspotap->disable_ANQP_response);

	if (!hspotap->disable_ANQP_response)
		proGasSetQueryResponse(gas, responseSize, pktEncodeBuf(&rsp));

	free(buffer);
}

static int gasEventHandler(hspotApT *hspotap, proGasEventT *event, uint16 *status)
{
	hspotPrintGasEvent(event);

	if (event->type == PRO_GAS_EVENT_QUERY_REQUEST) {
		processQueryRequest(hspotap, event->gas,
			event->queryReq.len, event->queryReq.data);
	}
	else if (event->type == PRO_GAS_EVENT_STATUS) {
#if TESTING_MODE
		/* toggle setting */
		hspotap->isGasPauseForServerResponse =
			hspotap->isGasPauseForServerResponse ? FALSE : TRUE;
		TRACE(TRACE_DEBUG, "pause for server response: %s\n",
			hspotap->isGasPauseForServerResponse ? "TRUE" : "FALSE");
		proGasSetIfGASPause(hspotap->isGasPauseForServerResponse, hspotap->ifr);
#endif
		if (status != 0)
			*status = event->status.statusCode;
		return TRUE;
	}

	return FALSE;
}

static void gasEventCallback(void *context, proGasT *gas, proGasEventT *event)
{
	(void)context;
	hspotApT *hspotap;
	hspotap = getHspotApByWlif(proGasGetDrv(gas));
	if (hspotap == NULL) {
		printf("can't find matched hspotap\n");
		return;
	}

	if (gasEventHandler(hspotap, event, 0))
	{
		printf("GAS_DONE\n");
	}
}

static void addIes(hspotApT *hspotap)
{
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;
	pktEncodeT ie;
	/* encode hotspot vendor IE */
	pktEncodeInit(&ie, sizeof(vendorIeHSI->ieData), vendorIeHSI->ieData);
	pktEncodeIeHotspotIndication2(&ie, !hspotap->isDgafDisabled, FALSE, HSPOT_RELEASE_2);
	vendorIeHSI->ieLength = pktEncodeLength(&ie);

	/* add to beacon and probe response */
	vendorIeHSI->pktflag = VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG;

	/* delete IEs first if not a clean shutdown */
	wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
		vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

	pktEncodeInit(&ie, sizeof(vendorIeHSI->ieData), vendorIeHSI->ieData);
	pktEncodeIeHotspotIndication2(&ie, hspotap->isDgafDisabled, FALSE, HSPOT_RELEASE_2);

	/* delete IEs first if not a clean shutdown */
	wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
		vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

	if (hspotap->hs_ie_enabled) {
		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
			vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2) < 0)
			TRACE(TRACE_ERROR, "failed to add vendor IE\n");
	}

	/* encode P2P vendor IE */
	/* P2P Manageability attribute with P2P Device Management bit (B0) set to 1 and */
	/* the Cross Connection Permitted bit (B1) set to zero */
	vendorIeP2P->ieLength = 10;
	vendorIeP2P->ieData[0] = 0xDD;
	vendorIeP2P->ieData[1] = 0x08;
	vendorIeP2P->ieData[2] = 0x50;
	vendorIeP2P->ieData[3] = 0x6f;
	vendorIeP2P->ieData[4] = 0x9a;
	vendorIeP2P->ieData[5] = 0x09;
	vendorIeP2P->ieData[6] = 0x0a;
	vendorIeP2P->ieData[7] = 0x01;
	vendorIeP2P->ieData[8] = 0x00;
	vendorIeP2P->ieData[9] = 0x03;

	/* add to beacon and probe response */
	vendorIeP2P->pktflag = VNDR_IE_BEACON_FLAG | VNDR_IE_PRBRSP_FLAG;

	/* delete IEs first if not a clean shutdown */
	wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
		vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);

	vendorIeP2P->ieData[9] = 0x01;
	/* delete IEs first if not a clean shutdown */
	wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
		vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);
	if (hspotap->p2p_cross_enabled)
		vendorIeP2P->ieData[9] = 0x03;
	/* don't need first 2 bytes (0xdd + len) */
	if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
		vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2) < 0)
		TRACE(TRACE_ERROR, "failed to add vendor IE\n");

	update_iw_ie(hspotap, TRUE);

	update_rc_ie(hspotap);

	hspotap->ap_ANQPenabled = TRUE;
	hspotap->ap_MIHenabled = FALSE;
	update_ap_ie(hspotap);

}

static void deleteIes(hspotApT *hspotap)
{
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;

	/* delete hotspot vendor IE */
	if (hspotap->hs_ie_enabled)
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
			vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

	/* delete P2P vendor IE */
	if (hspotap->p2p_ie_enabled)
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);

	/* delete interworking IE */
	if (wl_ie(hspotap->ifr, DOT11_MNG_INTERWORKING_ID, 0, 0) < 0)
		TRACE(TRACE_ERROR, "failed delete IW IE\n");

	/* delete advertisement protocol IE */
	if (wl_ie(hspotap->ifr, DOT11_MNG_ADVERTISEMENT_ID, 0, 0) < 0)
		TRACE(TRACE_ERROR, "failed delete IWAP IE\n");

	/* delete roaming consortium IE */
	if (wl_ie(hspotap->ifr, DOT11_MNG_ROAM_CONSORT_ID, 0, 0) < 0)
		TRACE(TRACE_ERROR, "failed delete IWRC IE\n");
}

static int update_dgaf_disable(hspotApT *hspotap)
{
	int err = 0;
	if (hspotap->isDgafDisabled) {
		if (wl_dhcp_unicast(hspotap->ifr, 1) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_dhcp_unicast failed\n");
		}
		if (wl_block_multicast(hspotap->ifr, 1) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_block_multicast failed\n");
		}
		if (wl_gtk_per_sta(hspotap->ifr, 1) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_gtk_per_sta failed\n");
		}
	} else {
		if (wl_gtk_per_sta(hspotap->ifr, 0) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_gtk_per_sta failed\n");
		}
		if (wl_block_multicast(hspotap->ifr, 0) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_block_multicast failed\n");
		}
		if (wl_dhcp_unicast(hspotap->ifr, 0) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_dhcp_unicast failed\n");
		}
	}

	return err;
}

static int update_l2_traffic_inspect(hspotApT *hspotap)
{
	int err = 0;
	if (hspotap->l2_traffic_inspect) {
		if (wl_block_ping(hspotap->ifr, !hspotap->icmpv4_echo) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_block_ping failed\n");
		}
		if (wl_block_sta(hspotap->ifr, 0) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_block_sta failed\n");
		}
		if (wl_ap_isolate(hspotap->ifr, 0) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_ap_isolate failed\n");
		}
	}
	else {
		if (wl_block_ping(hspotap->ifr, 0) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_block_ping failed\n");
		}
		if (wl_block_sta(hspotap->ifr, 1) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_block_sta failed\n");
		}
		if (wl_ap_isolate(hspotap->ifr, 1) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_ap_isolate failed\n");
		}
	}
	return err;
}

static void init_wlan_hspot(hspotApT *hspotap)
{
	/* delete interworking IE */
	if (wl_bssload(hspotap->ifr, 1) < 0)
		TRACE(TRACE_ERROR, "wl_bssload failed\n");

	if (wl_dls(hspotap->ifr, 1) < 0)
		TRACE(TRACE_ERROR, "wl_dls failed\n");

	if (wl_wnm(hspotap->ifr, WL_WNM_BSSTRANS | WL_WNM_NOTIF) < 0)
		TRACE(TRACE_ERROR, "wl_wnm failed\n");
	if (wl_interworking(hspotap->ifr, 1) < 0)
		TRACE(TRACE_ERROR, "wl_interworking failed\n");

	if (wl_probresp_sw(hspotap->ifr, 1) < 0)
		TRACE(TRACE_ERROR, "wl_probresp_sw failed\n");

	if (wl_proxy_arp(hspotap->ifr, 1) < 0)
		TRACE(TRACE_ERROR, "wl_proxy_arp failed\n");

	if (wl_grat_arp(hspotap->ifr, 1) < 0)
		TRACE(TRACE_ERROR, "wl_grat_arp failed\n");

	if (wl_block_tdls(hspotap->ifr, 1) < 0)
		TRACE(TRACE_ERROR, "wl_block_tdls failed\n");
	if (wl_dls_reject(hspotap->ifr, 1) < 0)
		TRACE(TRACE_ERROR, "wl_dls_reject failed\n");

	update_dgaf_disable(hspotap);
	update_l2_traffic_inspect(hspotap);
}

static int update_iw_ie(hspotApT *hspotap, bool disable)
{
	int err = 0;
	if (hspotap->iw_enabled) {
		pktEncodeT ie;
		uint8 buffer[BUFFER_SIZE];
		/* encode interworking IE */
		pktEncodeInit(&ie, sizeof(buffer), buffer);
		pktEncodeIeInterworking(&ie, hspotap->iw_ANT, hspotap->iw_isInternet,
			FALSE, FALSE, FALSE,
			TRUE, hspotap->iw_VG, hspotap->iw_VT,
			hspotap->iw_isHESSIDPresent ? &hspotap->iw_HESSID : 0);

		/* add interworking IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_INTERWORKING_ID,
		pktEncodeLength(&ie) - 2, pktEncodeBuf(&ie) + 2);
		if (err)
			TRACE(TRACE_ERROR, "failed add IW IE\n");
	}
	else if (disable) {
		/* delete interworking IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_INTERWORKING_ID, 0, 0);
		if (err)
			TRACE(TRACE_ERROR, "failed delete IW IE\n");
	}
	return err;
}

static int update_rc_ie(hspotApT *hspotap)
{
	int err = 0;
	if (hspotap->rc_oiNum) {
		pktEncodeT ie;
		uint8 buffer[BUFFER_SIZE];
		/* encode roaming consortium IE */
		pktEncodeInit(&ie, sizeof(buffer), buffer);
		pktEncodeIeRoamingConsortium(&ie,
			hspotap->rc_oiNum > 3 ? (hspotap->rc_oiNum - 3) : 0,
			hspotap->rc_oiNum > 0 ? hspotap->rc_oi[0].len : 0, hspotap->rc_oi[0].data,
			hspotap->rc_oiNum > 1 ? hspotap->rc_oi[1].len : 0, hspotap->rc_oi[1].data,
			hspotap->rc_oiNum > 2 ? hspotap->rc_oi[2].len : 0, hspotap->rc_oi[2].data);

		/* add roaming consortium IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_ROAM_CONSORT_ID,
		pktEncodeLength(&ie) - 2, pktEncodeBuf(&ie) + 2);
		if (err)
			TRACE(TRACE_ERROR, "failed add RC IE\n");
	}
	else {
		/* delete roaming consortium IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_ROAM_CONSORT_ID, 0, 0);
		if (err)
			TRACE(TRACE_ERROR, "failed delete RC IE\n");
	}
	return err;
}

static int update_ap_ie(hspotApT *hspotap)
{
	int err = 0;
	if (hspotap->ap_ANQPenabled || hspotap->ap_MIHenabled) {
		pktEncodeT ie;
		uint8 buffer[BUFFER_SIZE];
		uint8 adBuffer[BUFFER_SIZE];
		pktEncodeT ad;

		/* encode advertisement protocol IE */
		pktEncodeInit(&ie, sizeof(buffer), buffer);
		pktEncodeInit(&ad, sizeof(adBuffer), adBuffer);
		pktEncodeIeAdvertisementProtocolTuple(&ad, 0x7f, FALSE,
			hspotap->ap_ANQPenabled ? ADVP_ANQP_PROTOCOL_ID : MIH_PROTOCOL_ID);
		pktEncodeIeAdvertiseProtocol(&ie, pktEncodeLength(&ad), pktEncodeBuf(&ad));

		/* add advertisement protocol IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_ADVERTISEMENT_ID,
		pktEncodeLength(&ie) - 2, pktEncodeBuf(&ie) + 2);
		if (err)
			TRACE(TRACE_ERROR, "failed add AP IE\n");
	}
	else {
		/* delete advertisement protocol IE */
		err = wl_ie(hspotap->ifr, DOT11_MNG_ADVERTISEMENT_ID, 0, 0);
		if (err)
			TRACE(TRACE_ERROR, "failed delete AP IE\n");
	}
	return err;
}

static bool strToEther(char *str, struct ether_addr *bssid)
{
	int hex[ETHER_ADDR_LEN];
	int i;

	if (sscanf(str, "%02x:%02x:%02x:%02x:%02x:%02x",
		&hex[0], &hex[1], &hex[2], &hex[3], &hex[4], &hex[5]) != 6)
		return FALSE;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		bssid->octet[i] = hex[i];

	return TRUE;
}

static int
get_hex_data(uchar *data_str, uchar *hex_data, int len)
{
	uchar *src, *dest;
	uchar val;
	int idx;
	char hexstr[3];

	src = data_str;
	dest = hex_data;

	for (idx = 0; idx < len; idx++) {
		hexstr[0] = src[0];
		hexstr[1] = src[1];
		hexstr[2] = '\0';

		val = (uchar) strtoul(hexstr, NULL, 16);

		*dest++ = val;
		src += 2;
	}

	return 0;
}

static int hspot_cmd_interworking_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command interworking\n");
		return -1;
	}

	hspotap->iw_enabled = (atoi(argv[0]) != 0);
	printf("iw_enabled %d\n", hspotap->iw_enabled);

	err = update_iw_ie(hspotap, TRUE);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_enabled);
	ret = nvram_set(strcat_r(hspotap->prefix, "interworking", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_accs_net_type_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command accs_net_type\n");
		return -1;
	}

	hspotap->iw_ANT = atoi(argv[0]);
	printf("iw_ANT %d\n", hspotap->iw_ANT);

	err = update_iw_ie(hspotap, FALSE);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_ANT);
	ret = nvram_set(strcat_r(hspotap->prefix, "accs_net_type", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_internet_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command internet\n");
		return -1;
	}

	hspotap->iw_isInternet = (atoi(argv[0]) != 0);
	printf("iw_isInternet %d\n", hspotap->iw_isInternet);

	err = update_iw_ie(hspotap, FALSE);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_isInternet);
	ret = nvram_set(strcat_r(hspotap->prefix, "internet", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_venue_grp_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command venue_grp\n");
		return -1;
	}

	hspotap->iw_VG = atoi(argv[0]);
	printf("iw_VG %d\n", hspotap->iw_VG);

	err = update_iw_ie(hspotap, FALSE);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_VG);
	ret = nvram_set(strcat_r(hspotap->prefix, "venue_grp", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_venue_type_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command venue_type\n");
		return -1;
	}

	hspotap->iw_VT = atoi(argv[0]);
	printf("iw_VT %d\n", hspotap->iw_VT);

	err = update_iw_ie(hspotap, FALSE);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_VT);
	ret = nvram_set(strcat_r(hspotap->prefix, "venue_type", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_hessid_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		hspotap->iw_isHESSIDPresent = FALSE;
		printf("HESSID is not present\n");
	}
	else {
		hspotap->iw_isHESSIDPresent = TRUE;
		if (!strToEther(argv[0], &hspotap->iw_HESSID)) {
			printf("wrong format parameter in command hessid\n");
			return -1;
		}

		ret = nvram_set(strcat_r(hspotap->prefix, "hessid", varname), argv[0]);
		if (ret) {
			printf("nvram_set %s=%s failure\n", varname, argv[0]);
		}
		nvram_commit();

		printf("HESSID 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", hspotap->iw_HESSID.octet[0],
			hspotap->iw_HESSID.octet[1], hspotap->iw_HESSID.octet[2],
			hspotap->iw_HESSID.octet[3], hspotap->iw_HESSID.octet[4],
			hspotap->iw_HESSID.octet[5]);
	}

	err = update_iw_ie(hspotap, FALSE);
	return err;
}

static int hspot_cmd_roaming_cons_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	int data_len;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;
	int total_len = 0;
	char *c;
	hspotap->rc_oiNum = 0;
	c = varvalue;
	while (*argv) {
		if (strcmp(argv[0], "disabled") == 0) break;
		if (strcmp(argv[0], "Disabled") == 0) break;
		data_len = strlen(argv[0]) / 2;
		if (data_len && (data_len <= MAX_OI_LEN)) {
			get_hex_data((uchar *)argv[0],
				hspotap->rc_oi[hspotap->rc_oiNum].data, data_len);
			hspotap->rc_oi[hspotap->rc_oiNum].len = data_len;
			printf("OI %d:0x%x 0x%x 0x%x\n", hspotap->rc_oiNum,
				hspotap->rc_oi[hspotap->rc_oiNum].data[0],
				hspotap->rc_oi[hspotap->rc_oiNum].data[1],
				hspotap->rc_oi[hspotap->rc_oiNum].data[2]);

			total_len += (strlen(argv[0]) + 1);
			if (total_len <= NVRAM_MAX_PARAM_LEN) {
				if (hspotap->rc_oiNum) {
					c += sprintf(c, ";%s", argv[0]);
				} else {
					c += sprintf(c, "%s", argv[0]);
				}
			}

			hspotap->rc_oiNum++;
			if (hspotap->rc_oiNum >= MAX_RC_OI_NUM)
				break;
		}
		++argv;
	}
	*c = 0;
	if (hspotap->rc_oiNum == 0) {
		printf("Roaming consortium OI is not present\n");
		ret = nvram_unset(strcat_r(hspotap->prefix, "roaming_cons", varname));
		if (ret) {
			printf("nvram_unset %s failure\n", varname);
		}
	} else {
		ret = nvram_set(strcat_r(hspotap->prefix, "roaming_cons", varname), varvalue);
		if (ret) {
			printf("nvram_set %s=%s failure\n", varname, varvalue);
		}
	}
	nvram_commit();
	err = update_rc_ie(hspotap);
	return err;
}

static int hspot_cmd_anqp_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command anqp\n");
		return -1;
	}

	hspotap->ap_ANQPenabled = (atoi(argv[0]) != 0);
	printf("ap_ANQPenabled %d\n", hspotap->ap_ANQPenabled);

	err = update_ap_ie(hspotap);
	return err;
}

static int hspot_cmd_mih_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command mih\n");
		return -1;
	}

	hspotap->ap_MIHenabled = (atoi(argv[0]) != 0);
	printf("ap_MIHenabled %d\n", hspotap->ap_MIHenabled);

	err = update_ap_ie(hspotap);
	return err;
}

static int hspot_cmd_dgaf_disable_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	pktEncodeT ie;
	bool isDgafDisabled;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command dgaf_disable\n");
		return -1;
	}

	isDgafDisabled = (atoi(argv[0]) != 0);
	printf("isDgafDisabled %d\n", isDgafDisabled);

	if (hspotap->isDgafDisabled == isDgafDisabled)
		return err;

	hspotap->isDgafDisabled = isDgafDisabled;

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->isDgafDisabled);
	ret = nvram_set(strcat_r(hspotap->prefix, "dgaf_disabled", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	/* delete hotspot vendor IE */
	if (hspotap->hs_ie_enabled)
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
			vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

	/* encode hotspot vendor IE */
	pktEncodeInit(&ie, sizeof(vendorIeHSI->ieData), vendorIeHSI->ieData);
	pktEncodeIeHotspotIndication2(&ie, hspotap->isDgafDisabled, FALSE, HSPOT_RELEASE_2);

	if (hspotap->hs_ie_enabled) {
		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
			vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2) < 0) {
			TRACE(TRACE_ERROR, "failed to add vendor IE\n");
		}
	}

	err = update_dgaf_disable(hspotap);
	return err;
}

static int hspot_cmd_l2_traffic_inspect_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command l2_traffic_inspect\n");
		return -1;
	}

	hspotap->l2_traffic_inspect = (atoi(argv[0]) != 0);
	printf("l2_traffic_inspect %d\n", hspotap->l2_traffic_inspect);
	err = update_l2_traffic_inspect(hspotap);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->l2_traffic_inspect);
	ret = nvram_set(strcat_r(hspotap->prefix, "l2_traffic_inspect", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_icmpv4_echo_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command icmpv4_echo\n");
		return -1;
	}

	hspotap->icmpv4_echo = (atoi(argv[0]) != 0);
	printf("icmpv4_echo %d\n", hspotap->icmpv4_echo);

	if (hspotap->l2_traffic_inspect) {
		if (wl_block_ping(hspotap->ifr, !hspotap->icmpv4_echo) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "wl_block_ping failed\n");
		}
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->icmpv4_echo);
	ret = nvram_set(strcat_r(hspotap->prefix, "icmpv4_echo", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_plmn_mcc_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	hspotap->numPlmn = 0;
	while (*argv) {
		if (strlen(argv[0]) > MCC_LENGTH) {
			printf("wrong MCC length %d\n", strlen(argv[0]));
			hspotap->numPlmn = 0;
			return -1;
		}

		strcpy(hspotap->Plmn[hspotap->numPlmn].mcc, argv[0]);
		printf("plmn_mcc %d: %s\n", hspotap->numPlmn, hspotap->Plmn[hspotap->numPlmn].mcc);
		hspotap->numPlmn++;
		if (hspotap->numPlmn >= MAX_PLMN_NUM)
			break;
		++argv;
	}

	if (hspotap->numPlmn == 0)
		printf("plmn_mcc is not present\n");

	return err;
}

static int hspot_cmd_plmn_mnc_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	hspotap->numPlmn = 0;
	while (*argv) {
		if (strlen(argv[0]) > MNC_LENGTH) {
			printf("wrong MNC length %d\n", strlen(argv[0]));
			hspotap->numPlmn = 0;
			return -1;
		}

		strcpy(hspotap->Plmn[hspotap->numPlmn].mnc, argv[0]);
		printf("plmn_mnc %d: %s\n", hspotap->numPlmn, hspotap->Plmn[hspotap->numPlmn].mnc);
		hspotap->numPlmn++;
		if (hspotap->numPlmn >= MAX_PLMN_NUM)
			break;
		++argv;
	}

	if (hspotap->numPlmn == 0)
		printf("plmn_mnc is not present\n");

	return err;
}

static int hspot_cmd_proxy_arp_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	bool proxy_arp;
	if (argv[0] == NULL) {
		printf("missing parameter in proxy_arp\n");
		return -1;
	}

	proxy_arp = (atoi(argv[0]) != 0);
	printf("proxy_arp %d\n", proxy_arp);

	if (wl_proxy_arp(hspotap->ifr, proxy_arp) < 0) {
		err = -1;
		TRACE(TRACE_ERROR, "wl_proxy_arp failed\n");
	}
	if (wl_grat_arp(hspotap->ifr, proxy_arp) < 0) {
		err = -1;
		TRACE(TRACE_ERROR, "wl_grat_arp failed\n");
	}
	return err;
}

static int hspot_cmd_bcst_uncst_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	bool bcst_uncst;
	if (argv[0] == NULL) {
		printf("missing parameter in bcst_uncst\n");
		return -1;
	}

	bcst_uncst = (atoi(argv[0]) != 0);
	printf("bcst_uncst %d\n", bcst_uncst);

	if (wl_dhcp_unicast(hspotap->ifr, bcst_uncst) < 0) {
		err = -1;
		TRACE(TRACE_ERROR, "wl_dhcp_unicast failed\n");
	}
	return err;
}

static int hspot_cmd_gas_cb_delay_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	int gas_cb_delay;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in gas_cb_delay\n");
		return -1;
	}

	gas_cb_delay = atoi(argv[0]);
	printf("gas_cb_delay %d\n", gas_cb_delay);
	if (gas_cb_delay) {
		hspotap->isGasPauseForServerResponse = FALSE;
		proGasSetIfCBDelayUnpause(gas_cb_delay, hspotap->ifr);
	}
	else {
		hspotap->isGasPauseForServerResponse = TRUE;
	}
	proGasSetIfGASPause(hspotap->isGasPauseForServerResponse, hspotap->ifr);

	hspotap->gas_cb_delay = gas_cb_delay;

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->gas_cb_delay);
	ret = nvram_set(strcat_r(hspotap->prefix, "gas_cb_delay", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_4_frame_gas_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in 4_frame_gas\n");
		return -1;
	}

	hspotap->isGasPauseForServerResponse = (atoi(argv[0]) == 0);
	printf("4_frame_gas %d\n", !(hspotap->isGasPauseForServerResponse));
	proGasSetIfGASPause(hspotap->isGasPauseForServerResponse, hspotap->ifr);

	snprintf(varvalue, sizeof(varvalue), "%d", (!(hspotap->isGasPauseForServerResponse)));
	ret = nvram_set(strcat_r(hspotap->prefix, "4_frame_gas", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_domain_list_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	int i = 0;
	char *token;

	if (argv[0] == NULL) {
		printf("missing parameter in command domain_list\n");
		return -1;
	}
	while ((i < MAX_DOMAIN) &&
	       ((token = argv[i]) != NULL)) {
		strcpy(hspotap->domain[i].name, token);
		hspotap->domain[i].len = strlen(token);
		printf("Domain %d: %s, len %d\n", i, hspotap->domain[i].name,
			hspotap->domain[i].len);
		i++;
	}
	hspotap->numDomain = i;
	return err;
}

static int hspot_cmd_sess_info_url_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command sess_info_url\n");
		return -1;
	}

	if (hspotap->url_len)
		free(hspotap->url);

	hspotap->url_len = strlen(argv[0]);
	if (hspotap->url_len == 0) {
		printf("sess_info_url: length is zero\n");
		wl_wnm_url(hspotap->ifr, 0, 0);
		return err;
	}

	hspotap->url = malloc(hspotap->url_len + 1);
	if (hspotap->url == NULL) {
		hspotap->url_len = 0;
		wl_wnm_url(hspotap->ifr, 0, 0);
		printf("sess_info_url: malloc failed\n");
		return -1;
	}

	strcpy((char *)hspotap->url, argv[0]);

	printf("sess_info_url: %s, len %d\n", hspotap->url, hspotap->url_len);

	err = wl_wnm_url(hspotap->ifr, hspotap->url_len, hspotap->url);
	return err;
}

static int hspot_cmd_dest_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	struct ether_addr da;

	if (argv[0] == NULL) {
		printf("missing parameter in command dest\n");
		return -1;
	}

	if (!strToEther(argv[0], &da)) {
		printf("wrong format parameter in command dest\n");
		return -1;
	}

	printf("dest 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", da.octet[0],
		da.octet[1], da.octet[2],
		da.octet[3], da.octet[4],
		da.octet[5]);

	err = hspot_send_BTM_Req_frame(hspotap, &da);
	return err;
}

static int hspot_cmd_hs2_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	int err = 0;
	bool enabled = TRUE;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command hs2\n");
		return -1;
	}

	enabled = (atoi(argv[0]) != 0);

	printf("hs2 enabled %d\n", enabled);
	if (hspotap->hs_ie_enabled != enabled) {
		if (hspotap->hs_ie_enabled)
			/* delete hotspot vendor IE */
			wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
				vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);

		if (enabled) {
			/* don't need first 2 bytes (0xdd + len) */
			if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
				vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2) < 0) {
				err = -1;
				TRACE(TRACE_ERROR, "failed to add vendor IE\n");
			}
		}
		hspotap->hs_ie_enabled = enabled;
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->hs_ie_enabled);
	ret = nvram_set(strcat_r(hspotap->prefix, "hs2_ie", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_p2p_cross_connect_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;
	int err = 0;
	bool enabled = TRUE;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	if (argv[0] == NULL) {
		printf("missing parameter in command p2p_cross_connect\n");
		return -1;
	}

	enabled = (atoi(argv[0]) != 0);

	printf("p2p_cross_connect enabled %d\n", enabled);
	if (hspotap->p2p_cross_enabled != enabled) {
		/* delete P2P vendor IE */
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);

		vendorIeP2P->ieData[9] = enabled ? 0x03 : 0x01;

		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
		    vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2) < 0) {
			err = -1;
			TRACE(TRACE_ERROR, "failed to add vendor IE\n");
		}
		hspotap->p2p_cross_enabled = enabled;
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->p2p_cross_enabled);
	ret = nvram_set(strcat_r(hspotap->prefix, "p2p_cross", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}
	nvram_commit();

	return err;
}

static int hspot_cmd_ip_add_type_avail_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	if (argv[0] == NULL) {
		printf("missing parameter in command ip_add_type_avail\n");
		return -1;
	}
	hspotap->ipa_id = atoi(argv[0]);
	printf("ip_add_type_avail id %d\n", hspotap->ipa_id);
	return err;
}

static int hspot_cmd_hs_reset_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	vendorIeT *vendorIeHSI = &hspotap->vendorIeHSI;
	vendorIeT *vendorIeP2P = &hspotap->vendorIeP2P;
	pktEncodeT ie;
	int err = 0;
	char varvalue[NVRAM_MAX_PARAM_LEN];
	char varname[NVRAM_MAX_PARAM_LEN];
	int ret;

	printf("hs_reset\n");
	hspotap->iw_enabled = TRUE;
	hspotap->iw_isHESSIDPresent = TRUE;
	if (wl_cur_etheraddr(hspotap->ifr, DEFAULT_BSSCFG_INDEX, &hspotap->iw_HESSID) < 0)
		TRACE(TRACE_ERROR, "wl_cur_etheraddr failed\n");
	hspotap->iw_ANT = 2;
	hspotap->iw_isInternet = 0;
	hspotap->iw_VT = 8;
	hspotap->iw_VG = 2;
	update_iw_ie(hspotap, TRUE);

	hspotap->rc_oiNum = 2;
	memcpy(hspotap->rc_oi[0].data, WFA_OUI, strlen(WFA_OUI));
	hspotap->rc_oi[0].len = strlen(WFA_OUI);
	hspotap->rc_oi[1].data[0] = 0x00;
	hspotap->rc_oi[1].data[1] = 0x1B;
	hspotap->rc_oi[1].data[2] = 0xC5;
	hspotap->rc_oi[1].data[3] = 0x04;
	hspotap->rc_oi[1].data[4] = 0xBD;
	hspotap->rc_oi[1].len = 5;

	update_rc_ie(hspotap);

	hspotap->ap_ANQPenabled = TRUE;
	hspotap->ap_MIHenabled = FALSE;
	update_ap_ie(hspotap);

	if (hspotap->isDgafDisabled) {
		hspotap->isDgafDisabled = FALSE;
		/* delete hotspot vendor IE */
		if (hspotap->hs_ie_enabled)
			wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
				vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2);
		/* encode hotspot vendor IE */
		pktEncodeInit(&ie, sizeof(vendorIeHSI->ieData), vendorIeHSI->ieData);
		pktEncodeIeHotspotIndication2(&ie,
			hspotap->isDgafDisabled, FALSE, HSPOT_RELEASE_2);
		hspotap->hs_ie_enabled = FALSE;
		update_dgaf_disable(hspotap);
	}

	if (hspotap->hs_ie_enabled == FALSE) {
		hspotap->hs_ie_enabled = TRUE;
		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeHSI->pktflag,
			vendorIeHSI->ieLength - 2, vendorIeHSI->ieData + 2) < 0) {
			TRACE(TRACE_ERROR, "failed to add vendor IE\n");
		}
	}

	hspotap->numPlmn = 0;
	wl_proxy_arp(hspotap->ifr, 0);
	wl_grat_arp(hspotap->ifr, 0);
	proGasSetIfCBDelayUnpause(1000, hspotap->ifr);
	proGasSetComebackDelayResponsePause(1);
	hspotap->isGasPauseForServerResponse = TRUE;
	proGasSetIfGASPause(hspotap->isGasPauseForServerResponse, hspotap->ifr);
	hspotap->gas_cb_delay = 0;
	hspotap->numDomain = 0;
	wl_wnm_url(hspotap->ifr, 0, 0);
	if (hspotap->url_len)
		free(hspotap->url);
	hspotap->url_len = 0;

	if (hspotap->p2p_cross_enabled) {
		/* delete P2P vendor IE */
		wl_del_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
			vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2);

		vendorIeP2P->ieData[9] = 0x01;

		/* don't need first 2 bytes (0xdd + len) */
		if (wl_add_vndr_ie(hspotap->ifr, DEFAULT_BSSCFG_INDEX, vendorIeP2P->pktflag,
		    vendorIeP2P->ieLength - 2, vendorIeP2P->ieData + 2) < 0) {
			TRACE(TRACE_ERROR, "failed to add vendor IE\n");
		}
		hspotap->p2p_cross_enabled = FALSE;
	}

	hspotap->icmpv4_echo = TRUE;
	hspotap->l2_traffic_inspect = TRUE;
	update_l2_traffic_inspect(hspotap);

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->isDgafDisabled);
	ret = nvram_set(strcat_r(hspotap->prefix, "dgaf_disabled", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->gas_cb_delay);
	ret = nvram_set(strcat_r(hspotap->prefix, "gas_cb_delay", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", (!(hspotap->isGasPauseForServerResponse)));
	ret = nvram_set(strcat_r(hspotap->prefix, "4_frame_gas", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->l2_traffic_inspect);
	ret = nvram_set(strcat_r(hspotap->prefix, "l2_traffic_inspect", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->icmpv4_echo);
	ret = nvram_set(strcat_r(hspotap->prefix, "icmpv4_echo", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_ANT);
	ret = nvram_set(strcat_r(hspotap->prefix, "accs_net_type", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_enabled);
	ret = nvram_set(strcat_r(hspotap->prefix, "interworking", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_isInternet);
	ret = nvram_set(strcat_r(hspotap->prefix, "internet", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_VT);
	ret = nvram_set(strcat_r(hspotap->prefix, "venue_type", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->iw_VG);
	ret = nvram_set(strcat_r(hspotap->prefix, "venue_grp", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%02x:%02x:%02x:%02x:%02x:%02x",
		hspotap->iw_HESSID.octet[0], hspotap->iw_HESSID.octet[1],
		hspotap->iw_HESSID.octet[2], hspotap->iw_HESSID.octet[3],
		hspotap->iw_HESSID.octet[4], hspotap->iw_HESSID.octet[5]);
	ret = nvram_set(strcat_r(hspotap->prefix, "hessid", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	ret = nvram_unset(strcat_r(hspotap->prefix, "roaming_cons", varname));
	if (ret) {
		printf("nvram_unset %s failure\n", varname);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->hs_ie_enabled);
	ret = nvram_set(strcat_r(hspotap->prefix, "hs2_ie", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	snprintf(varvalue, sizeof(varvalue), "%d", hspotap->p2p_cross_enabled);
	ret = nvram_set(strcat_r(hspotap->prefix, "p2p_cross", varname), varvalue);
	if (ret) {
		printf("nvram_set %s=%s failure\n", varname, varvalue);
	}

	nvram_commit();

	return err;
}

static int hspot_cmd_nai_realm_list_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command nai_realm_list\n");
		return -1;
	}

	hspotap->nai_id = atoi(argv[0]);
	printf("nai_realm_list id %d\n", hspotap->nai_id);

	return err;
}

static int hspot_cmd_oper_name_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command oper_name\n");
		return -1;
	}

	hspotap->oper_id = atoi(argv[0]);
	printf("oper_name id %d\n", hspotap->oper_id);

	return err;
}

static int hspot_cmd_venue_name_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command venue_name\n");
		return -1;
	}

	hspotap->venue_id = atoi(argv[0]);
	printf("venue_name id %d\n", hspotap->venue_id);

	return err;
}

static int hspot_cmd_wan_metrics_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command wan_metrics\n");
		return -1;
	}

	hspotap->wanm_id = atoi(argv[0]);
	printf("wan_metrics id %d\n", hspotap->wanm_id);

	return err;
}

static int hspot_cmd_conn_cap_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command conn_cap\n");
		return -1;
	}

	hspotap->conn_id = atoi(argv[0]);
	printf("conn_cap id %d\n", hspotap->conn_id);

	return err;
}

static int hspot_cmd_net_auth_type_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command net_auth_type\n");
		return -1;
	}

	hspotap->nat_id = atoi(argv[0]);
	printf("net_auth_type id %d\n", hspotap->nat_id);

	return err;
}

static int hspot_cmd_plmn_default_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command plmn_default\n");
		return -1;
	}

	hspotap->useDefaultPLMNValue = (atoi(argv[0]) != 0);
	printf("useDefaultPLMNValue %d\n", hspotap->useDefaultPLMNValue);

	return err;
}

static int hspot_cmd_anqp_default_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command anqp_default\n");
		return -1;
	}

	hspotap->useDefaultANQPValue = (atoi(argv[0]) != 0);
	printf("useDefaultANQPValue %d\n", hspotap->useDefaultANQPValue);

	return err;
}

static int hspot_cmd_anqp_null_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command anqp_null\n");
		return -1;
	}

	hspotap->emptyANQPInfo = (atoi(argv[0]) != 0);
	printf("emptyANQPInfo %d\n", hspotap->emptyANQPInfo);

	return err;
}

static int hspot_cmd_nai_disabled_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command nai_disabled\n");
		return -1;
	}

	hspotap->isRealmDisabled = (atoi(argv[0]) != 0);
	printf("isRealmDisabled %d\n", hspotap->isRealmDisabled);

	return err;
}

static int hspot_cmd_3gpp_disabled_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command 3gpp_disabled\n");
		return -1;
	}

	hspotap->is3GppDisabled = (atoi(argv[0]) != 0);
	printf("is3GppDisabled %d\n", hspotap->is3GppDisabled);

	return err;
}

static int hspot_cmd_pause_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command pause\n");
		return -1;
	}

	hspotap->isGasPauseForServerResponse = (atoi(argv[0]) != 0);
	proGasSetIfGASPause(hspotap->isGasPauseForServerResponse, hspotap->ifr);
	printf("isGasPauseForServerResponse %d\n", hspotap->isGasPauseForServerResponse);

	return err;
}

static int hspot_cmd_dis_anqp_response_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command dis_anqp_response\n");
		return -1;
	}

	hspotap->disable_ANQP_response = (atoi(argv[0]) != 0);
	printf("disable_ANQP_response %d\n", hspotap->disable_ANQP_response);

	return err;
}

static int hspot_cmd_sim_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command sim\n");
		return -1;
	}

	hspotap->useSim = (atoi(argv[0]) != 0);
	printf("useSim %d\n", hspotap->useSim);

	return err;
}

static int hspot_cmd_response_size_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;

	if (argv[0] == NULL) {
		printf("missing parameter in command response_size\n");
		return -1;
	}

	hspotap->testResponseSize = atoi(argv[0]);
	printf("response_size %d\n", hspotap->testResponseSize);

	return err;
}

static int hspot_cmd_pause_cb_delay_handler(hspotApT *hspotap,
	char **argv, char *txData, bool *set_tx_data)
{
	int err = 0;
	int pause_cb_delay;

	if (argv[0] == NULL) {
		printf("missing parameter in command pause_cb_delay\n");
		return -1;
	}

	pause_cb_delay = atoi(argv[0]);
	printf("pause_cb_delay %d\n", pause_cb_delay);
	proGasSetComebackDelayResponsePause(pause_cb_delay);
	return err;
}

static int processCommand(hspotApT *hspotap, char **argv, char *txData)
{
	int err = 0;
	bool set_tx_data = TRUE;

	if (argv[0] == NULL) {
		printf("NULL command\n");
		err = -1;
	}
	else if (strcmp(argv[0], "interface") == 0) {
		hspotApT *new_hspotap;
		new_hspotap = getHspotApByIfname(argv[1]);
		if (new_hspotap == NULL) {
			printf("wrong interface name %s\n", argv[1]);
			err = -1;
		} else {
			current_hspotap = new_hspotap;
			err = 0;
			printf("change interface to %s\n", wl_ifname(current_hspotap->ifr));
		}
	}
	else if (strcmp(argv[0], "interworking") == 0) {
		argv++;
		err = hspot_cmd_interworking_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "accs_net_type") == 0) {
		argv++;
		err = hspot_cmd_accs_net_type_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "internet") == 0) {
		argv++;
		err = hspot_cmd_internet_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "venue_grp") == 0) {
		argv++;
		err = hspot_cmd_venue_grp_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "venue_type") == 0) {
		argv++;
		err = hspot_cmd_venue_type_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "hessid") == 0) {
		argv++;
		err = hspot_cmd_hessid_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "roaming_cons") == 0) {
		argv++;
		err = hspot_cmd_roaming_cons_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "anqp") == 0) {
		argv++;
		err = hspot_cmd_anqp_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "mih") == 0) {
		argv++;
		err = hspot_cmd_mih_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "dgaf_disable") == 0) {
		argv++;
		err = hspot_cmd_dgaf_disable_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "l2_traffic_inspect") == 0) {
		argv++;
		err = hspot_cmd_l2_traffic_inspect_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "icmpv4_echo") == 0) {
		argv++;
		err = hspot_cmd_icmpv4_echo_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "plmn_mcc") == 0) {
		argv++;
		err = hspot_cmd_plmn_mcc_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "plmn_mnc") == 0) {
		argv++;
		err = hspot_cmd_plmn_mnc_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "proxy_arp") == 0) {
		argv++;
		err = hspot_cmd_proxy_arp_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "bcst_uncst") == 0) {
		argv++;
		err = hspot_cmd_bcst_uncst_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "gas_cb_delay") == 0) {
		argv++;
		err = hspot_cmd_gas_cb_delay_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "4_frame_gas") == 0) {
		argv++;
		err = hspot_cmd_4_frame_gas_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "domain_list") == 0) {
		argv++;
		err = hspot_cmd_domain_list_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "sess_info_url") == 0) {
		argv++;
		err = hspot_cmd_sess_info_url_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "dest") == 0) {
		argv++;
		err = hspot_cmd_dest_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "hs2") == 0) {
		argv++;
		err = hspot_cmd_hs2_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "p2p_cross_connect") == 0) {
		argv++;
		err = hspot_cmd_p2p_cross_connect_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "ip_add_type_avail") == 0) {
		argv++;
		err = hspot_cmd_ip_add_type_avail_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "hs_reset") == 0) {
		argv++;
		err = hspot_cmd_hs_reset_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "nai_realm_list") == 0) {
		argv++;
		err = hspot_cmd_nai_realm_list_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "oper_name") == 0) {
		argv++;
		err = hspot_cmd_oper_name_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "venue_name") == 0) {
		argv++;
		err = hspot_cmd_venue_name_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "wan_metrics") == 0) {
		argv++;
		err = hspot_cmd_wan_metrics_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "conn_cap") == 0) {
		argv++;
		err = hspot_cmd_conn_cap_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "net_auth_type") == 0) {
		argv++;
		err = hspot_cmd_net_auth_type_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "plmn_default") == 0) {
		argv++;
		err = hspot_cmd_plmn_default_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "anqp_default") == 0) {
		argv++;
		err = hspot_cmd_anqp_default_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "anqp_null") == 0) {
		argv++;
		err = hspot_cmd_anqp_null_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "nai_disabled") == 0) {
		argv++;
		err = hspot_cmd_nai_disabled_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "3gpp_disabled") == 0) {
		argv++;
		err = hspot_cmd_3gpp_disabled_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "pause") == 0) {
		argv++;
		err = hspot_cmd_pause_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "dis_anqp_response") == 0) {
		argv++;
		err = hspot_cmd_dis_anqp_response_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "sim") == 0) {
		argv++;
		err = hspot_cmd_sim_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "response_size") == 0) {
		argv++;
		err = hspot_cmd_response_size_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "pause_cb_delay") == 0) {
		argv++;
		err = hspot_cmd_pause_cb_delay_handler(hspotap, argv, txData, &set_tx_data);
	}
	else if (strcmp(argv[0], "sr") == 0)
	{
		struct ether_addr addr, *bssid;
		char *url;
		int urlLength;
		pktEncodeT enc;
		uint8 buffer[BUFFER_SIZE];

		if ((argv[1] == NULL) || (argv[2] == NULL)) {
			printf("Invalid Number of Parameters\n");
			return err;
		}

		if (!strToEther(argv[1], &addr)) {
			printf("<addr> format is 00:11:22:33:44:55\n");
			return err;
		}

		url = argv[2];
		urlLength = strlen(url);
		if (urlLength > 255) {
			printf("<url> too long");
			return err;
		}

		pktEncodeInit(&enc, sizeof(buffer), buffer);
		pktEncodeWnmSubscriptionRemediation(&enc,
			hspotap->dialogToken++, urlLength, url);

		/* get bssid */
		wl_cur_etheraddr(hspotap->ifr, DEFAULT_BSSCFG_INDEX, &bssid);
		/* send action frame */
		wl_actframe(hspotap->ifr, DEFAULT_BSSCFG_INDEX, (uint32)pktEncodeBuf(&enc), 0, 250,
			&bssid, &addr, pktEncodeLength(&enc), pktEncodeBuf(&enc));
	}
	else
	{
		printf("unknown command %s\n", argv[0]);
		err = -1;
	}

	if (set_tx_data) {
		if (err) {
			strcpy(txData, "ERROR");
			/* sprintf(txData, "ERROR %d", err); */
		} else {
			strcpy(txData, "OK");
		}
	}

	return err;
}

static void tcpReceiveHandler(char *rxData, char *txData)
{
	/* receive and send back with OK
	   test with test.tcl test ap_set_hs2 to see what strings are being passed
	 */

	int argc = 0;
	char *argv[64], *token;
	int status;

	printf("received %s\n", rxData);
	/* sprintf(txData, "OK %s", rxData); */

	/* convert input to argc/argv format */
	while ((argc < (int)(sizeof(argv) / sizeof(char *) - 1)) &&
	       ((token = strtok(argc ? NULL : rxData, " \t\n")) != NULL)) {
		argv[argc++] = token;
	}
	argv[argc] = NULL;

	status = processCommand(current_hspotap, argv, txData);
}

static void setTestParameters(char *testName, hspotApT *hspotap)
{
	printf("test case --%s--\n", testName);

	if (!strcmp(testName, "test5.2-AP1:NAI")) {
		printf("3GPP enabled, NAI realm disabled\n");
		hspotap->isRealmDisabled = TRUE;
		hspotap->is3GppDisabled = FALSE;
		hspotap->useDefaultANQPValue = TRUE;
		hspotap->nai_id = 1;
		hspotap->ipa_id = 1;
	}
	else if (!strcmp(testName, "test5.2-AP2:NAI")) {
		printf("NAI realm enabled, 3GPP disabled\n");
		hspotap->isRealmDisabled = FALSE;
		hspotap->is3GppDisabled = TRUE;
		hspotap->useDefaultANQPValue = TRUE;
		hspotap->nai_id = 1;
		hspotap->ipa_id = 1;
	}
	else if (!strcmp(testName, "test5.2-AP3:NAI")) {
		printf("NAI realm disabled, 3GPP disabled\n");
		hspotap->isRealmDisabled = TRUE;
		hspotap->is3GppDisabled = TRUE;
		hspotap->emptyANQPInfo = TRUE;
	}
}

static void hspotapFree(void)
{
	int i;

	for (i = 0; i < hspotap_num; i++) {
		/* delete IEs */
		deleteIes(hspotaps[i]);

		wl_wnm_url(hspotaps[i]->ifr, 0, 0);

		if (hspotaps[i]->url_len)
			free(hspotaps[i]->url);

		free(hspotaps[i]);
	}

	hspotap_num = 0;

	wlFree();
}

int main(int argc, char **argv)
{
	int i;
	void *ifr;

	TRACE_LEVEL_SET(TRACE_ERROR);

	printf("\n");
	printf("Hotspot2.0 - version %s\n", EPI_VERSION_STR);
	printf("Copyright Broadcom Corporation\n");

	if (wl() == NULL) {
#ifdef DSLCPE
		printf("can't find hspot wl interface\n");
#else
		printf("can't find wl interface\n");
#endif
		exit(1);
	}

	while ((ifr = wlif(hspotap_num)) != NULL) {
		char *osifname;
		char varname[NVRAM_MAX_PARAM_LEN];
		char *ptr;
		int pri, sec;
		bool find;
		char prefix[16];
		hspotApT *hspotap;
		hspotap = malloc(sizeof(hspotApT));
		if (!hspotap) {
			printf("malloc failure\n");
			hspotapFree();
			exit(1);
		}
		memset(hspotap, 0, sizeof(hspotApT));
		hspotaps[hspotap_num] = hspotap;
		hspotap_num ++;

		hspotap->isGasPauseForServerResponse = TRUE;
		/* token start from 1 */
		hspotap->req_token = 1;
		hspotap->ifr = ifr;
		hspotap->useDefaultANQPValue = FALSE;
		hspotap->useDefaultPLMNValue = FALSE;
		hspotap->emptyANQPInfo = FALSE;
		hspotap->useSim = FALSE;
		hspotap->disable_ANQP_response = FALSE;

		hspotap->l2_traffic_inspect = TRUE;
		hspotap->icmpv4_echo = TRUE;

		hspotap->nai_id = 1;
		hspotap->oper_id = 1;
		hspotap->venue_id = 1;
		hspotap->wanm_id = 1;
		hspotap->conn_id = 1;
		hspotap->ipa_id = 1;

		hspotap->opclass_id = 3;
		hspotap->home_id = 1;
		hspotap->nat_id = 1;

		hspotap->hs_ie_enabled = TRUE;
		hspotap->p2p_cross_enabled = FALSE;
		hspotap->p2p_ie_enabled = TRUE;

		/* interworking IE default value */
		hspotap->iw_enabled = TRUE;
		hspotap->iw_ANT = 2;
		hspotap->iw_isInternet = 0;
		hspotap->iw_VT = 8;
		hspotap->iw_VG = 2;
		hspotap->iw_isHESSIDPresent = TRUE;
		if (wl_cur_etheraddr(ifr, DEFAULT_BSSCFG_INDEX, &hspotap->iw_HESSID) < 0)
			TRACE(TRACE_ERROR, "wl_cur_etheraddr failed\n");

		printf("HESSID %d: 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", hspotap_num - 1,
			hspotap->iw_HESSID.octet[0], hspotap->iw_HESSID.octet[1],
			hspotap->iw_HESSID.octet[2], hspotap->iw_HESSID.octet[3],
			hspotap->iw_HESSID.octet[4], hspotap->iw_HESSID.octet[5]);

		hspotap->rc_oiNum = 2;
		memcpy(hspotap->rc_oi[0].data, WFA_OUI, strlen(WFA_OUI));
		hspotap->rc_oi[0].len = strlen(WFA_OUI);
		hspotap->rc_oi[1].data[0] = 0x00;
		hspotap->rc_oi[1].data[1] = 0x1B;
		hspotap->rc_oi[1].data[2] = 0xC5;
		hspotap->rc_oi[1].data[3] = 0x04;
		hspotap->rc_oi[1].data[4] = 0xBD;
		hspotap->rc_oi[1].len = 5;

		osifname = wl_ifname(hspotap->ifr);
		find = FALSE;
		/* look for interface name on the primary interfaces first */
		for (pri = 0; pri < MAX_NVPARSE; pri++) {
			snprintf(varname, sizeof(varname),
				"wl%d_ifname", pri);
			if (nvram_match(varname, osifname)) {
				find = TRUE;
				snprintf(prefix, sizeof(prefix), "wl%d_", pri);
				break;
			}
		}

		if (!find) {
			/* look for interface name on the multi-instance interfaces */
			for (pri = 0; pri < MAX_NVPARSE; pri++) {
				for (sec = 0; sec < MAX_NVPARSE; sec++) {
					snprintf(varname, sizeof(varname),
						"wl%d.%d_ifname", pri, sec);
					if (nvram_match(varname, osifname)) {
						find = TRUE;
						snprintf(prefix, sizeof(prefix),
							"wl%d.%d_", pri, sec);
						break;
					}
				}
			}
		}

		if (find) {
			ptr = nvram_get(strcat_r(prefix, "dgaf_disabled", varname));
			if (ptr) {
				hspotap->isDgafDisabled = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->isDgafDisabled);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "gas_cb_delay", varname));
			if (ptr) {
				hspotap->gas_cb_delay = atoi(ptr);
				if (hspotap->gas_cb_delay) {
					hspotap->isGasPauseForServerResponse = FALSE;
				}
				printf("%s: %d\n", varname, hspotap->gas_cb_delay);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "4_frame_gas", varname));
			if (ptr) {
				hspotap->isGasPauseForServerResponse = (atoi(ptr) == 0);
				printf("%s: %d\n", varname,
					!(hspotap->isGasPauseForServerResponse));
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "l2_traffic_inspect", varname));
			if (ptr) {
				hspotap->l2_traffic_inspect = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->l2_traffic_inspect);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "icmpv4_echo", varname));
			if (ptr) {
				hspotap->icmpv4_echo = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->icmpv4_echo);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "accs_net_type", varname));
			if (ptr) {
				hspotap->iw_ANT = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->iw_ANT);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "interworking", varname));
			if (ptr) {
				hspotap->iw_enabled = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->iw_enabled);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "internet", varname));
			if (ptr) {
				hspotap->iw_isInternet = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->iw_isInternet);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "venue_type", varname));
			if (ptr) {
				hspotap->iw_VT = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->iw_VT);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "venue_grp", varname));
			if (ptr) {
				hspotap->iw_VG = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->iw_VG);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "hessid", varname));
			if (ptr) {
				if (!strToEther(ptr, &hspotap->iw_HESSID)) {
					printf("wrong format hessid in NVRAM\n");
				}
				else if (ETHER_ISNULLADDR(hspotap->iw_HESSID.octet)) {
					hspotap->iw_isHESSIDPresent = FALSE;
				}
				else {
					hspotap->iw_isHESSIDPresent = TRUE;
					printf("HESSID 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
						hspotap->iw_HESSID.octet[0],
						hspotap->iw_HESSID.octet[1],
						hspotap->iw_HESSID.octet[2],
						hspotap->iw_HESSID.octet[3],
						hspotap->iw_HESSID.octet[4],
						hspotap->iw_HESSID.octet[5]);
				}
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "roaming_cons", varname));
			if (ptr) {
				int data_len;
				int j;
				char *token;
				char *oi[MAX_RC_OI_NUM];
				i = 0;
				while ((i < MAX_RC_OI_NUM) &&
				  ((token = strtok(i ? NULL : ptr, ";")) != NULL)) {
					oi[i++] = token;
				}
				hspotap->rc_oiNum = 0;
				for (j = 0; j < i; j++) {
					data_len = strlen(oi[j]) / 2;
					if (data_len && (data_len <= MAX_OI_LEN)) {
						get_hex_data((uchar *)oi[j],
						  hspotap->rc_oi[hspotap->rc_oiNum].data, data_len);
						hspotap->rc_oi[hspotap->rc_oiNum].len = data_len;
						printf("OI %d:0x%x 0x%x 0x%x\n", hspotap->rc_oiNum,
						  hspotap->rc_oi[hspotap->rc_oiNum].data[0],
						  hspotap->rc_oi[hspotap->rc_oiNum].data[1],
						  hspotap->rc_oi[hspotap->rc_oiNum].data[2]);
						hspotap->rc_oiNum++;
					}
				}
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "hs2_ie", varname));
			if (ptr) {
				hspotap->hs_ie_enabled = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->hs_ie_enabled);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			ptr = nvram_get(strcat_r(prefix, "p2p_cross", varname));
			if (ptr) {
				hspotap->p2p_cross_enabled = atoi(ptr);
				printf("%s: %d\n", varname, hspotap->p2p_cross_enabled);
			} else {
				printf("%s is not defined in NVRAM\n", varname);
			}

			strcpy(hspotap->prefix, prefix);
		} else {
			printf("can't find NVRAM ifname for %s\n", osifname);
		}

		if (hspotap_num == 1) {
			for (i = 1; i < argc; i++) {
				if (strcmp(argv[i], "-help") == 0) {
					printf("\n");
					printf(" -debug      enable debug output\n");
					printf(" -verbose    enable verbose output\n");
					printf(" -help       print this menu\n");
					printf(" -dgaf       disable DGAF\n");
					printf(" sr <addr>   send subscription remediation action frame (use tcp port option)\n");
					printf("\n");
					printf("To redirect to file use 'tee' "
						"(eg. %s -d | tee log.txt).\n", argv[0]);
					printf("\n");
					hspotapFree();
					exit(1);
				}
				else if (strcmp(argv[i], "-debug") == 0) {
					TRACE_LEVEL_SET(TRACE_ERROR | TRACE_DEBUG | TRACE_PACKET);
				}
				else if (strcmp(argv[i], "-verbose") == 0) {
					TRACE_LEVEL_SET(TRACE_ALL);
				}
				else if (strcmp(argv[i], "-dgaf") == 0) {
					hspotap->isDgafDisabled = TRUE;
				}
				else if (strcmp(argv[i], "-tcp_port") == 0) {
					if (i == (argc - 1)) {
						printf("Not enough args for tcp port option\n");
						hspotapFree();
						exit(1);
					}
					tcpServerPort = atol(argv[i+1]);
					tcpServerEnabled = 1;
					i++;
				}
				else if (strcmp(argv[i], "-test") == 0) {
					if (i == (argc - 1)) {
						printf("Not enough arguments for test option\n");
						hspotapFree();
						exit(1);
					}
					setTestParameters(argv[i+1], hspotap);
					i++;
				}
				else if (strcmp(argv[i], "-respSize") == 0) {
					if (i == (argc - 1)) {
						printf("Not enough args for respSize option\n");
						hspotapFree();
						exit(1);
					}
					hspotap->testResponseSize = atoi(argv[i+1]);
					i++;
				}
				else if (strcmp(argv[i], "-gas4FramesOn") == 0) {
					printf("GAS 4 Frames is ON\n");
					hspotap->testResponseSize = 20000;
					hspotap->isGasPauseForServerResponse = FALSE;
					hspotap->gas_cb_delay = 1000;
					printf("testResponseSize %d\n", hspotap->testResponseSize);
				}
				else {
					printf("%s invalid\n", argv[i]);
					hspotapFree();
					exit(1);
				}
			}
		}

		if (wl_disable_event_msg(ifr, WLC_E_P2P_PROBREQ_MSG) < 0)
			TRACE(TRACE_ERROR, "failed to disable event msg %d\n",
				WLC_E_P2P_PROBREQ_MSG);

		/* add IEs */
		addIes(hspotap);

		init_wlan_hspot(hspotap);

		wl_wnm_url(ifr, 0, 0);

		if (hspotap_num >= MAX_WLIF_NUM)
			break;
	}

	current_hspotap = hspotaps[0];

	/* initialize GAS protocol */
	proGasSubscribeEvent(0, gasEventCallback);
	proGasInitDsp();
	proGasInitWlanHandler();

	if (tcpServerEnabled) {
		tcpSubscribeTcpHandler(tcpReceiveHandler);
		tcpSrvCreate(tcpServerPort);
	}

	for (i = 0; i < hspotap_num; i++) {
		if (hspotaps[i]->gas_cb_delay) {
			proGasSetIfCBDelayUnpause(hspotaps[i]->gas_cb_delay, hspotaps[i]->ifr);
		}
		proGasSetIfGASPause(hspotaps[i]->isGasPauseForServerResponse, hspotaps[i]->ifr);
	}
	dspStart(dsp());

	/* deinitialize GAS protocol */
	proGasDeinitialize();
	proGasUnsubscribeEvent(gasEventCallback);

	/* terminate dispatcher */
	dspFree();

	if (tcpServerEnabled) {
		tcpSubscribeTcpHandler(NULL);
		tcpSrvDestroy();
		tcpServerEnabled = 0;
	}

	hspotapFree();
	return 0;
}
