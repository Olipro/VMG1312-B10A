/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_wl.c 294321 2011-11-05 10:36:20Z $
 *
 * Description: Implement wl related functionalities to communicated with driver
 *
 */
#include <ctype.h>
#include "wpscli_osl.h"
#include "wlioctl.h"
#include "wps_wl.h"
#include "tutrace.h"
#include "wps_devinfo.h"
#include "wpscli_common.h"
#include <ie_utils.h>

/* enable structure packing */
#if defined(__GNUC__)
#define	PACKED	__attribute__((packed))
#else
#pragma pack(1)
#define	PACKED
#endif

#define XSUPPD_MAX_PACKET			4096
#define XSUPPD_ETHADDR_LENGTH		6
#define EAPOL_ETHER_TYPE			0x888e
#define ether_set_type(p, t)		((p)[0] = (uint8)((t) >> 8), (p)[1] = (uint8)(t))

BOOL b_wps_version2 = TRUE;

/* ethernet header */
typedef struct
{
	uint8 dest[XSUPPD_ETHADDR_LENGTH];
	uint8 src[XSUPPD_ETHADDR_LENGTH];
	uint8 type[2];
} PACKED ether_header;

typedef struct
{
	/* outgoing packet */
	uint8 out[XSUPPD_MAX_PACKET];
	int out_size;
} sendpacket;

typedef uint32 (*IE_UTILS_BUILD_FUNC)(void *params, uint8 *buf, int *buflen);
static IE_UTILS_BUILD_FUNC wpscli_ie_utils_build_func[IE_UTILS_BUILD_WPS_IE_FUNC_NUM + 1] = {
	NULL,				/* 0 */
	ie_utils_build_beacon_IE,	/* WPS_IE_TYPE_SET_BEACON_IE */
	ie_utils_build_probereq_IE,	/* WPS_IE_TYPE_SET_PROBE_REQUEST_IE */
	ie_utils_build_proberesp_IE,	/* WPS_IE_TYPE_SET_PROBE_RESPONSE_IE */
	ie_utils_build_assocreq_IE,	/* WPS_IE_TYPE_SET_ASSOC_REQUEST_IE */
	ie_utils_build_assocresp_IE	/* WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE */
};

BOOL wpscli_del_wps_ie(unsigned int frametype);

static uint wpscli_iovar_mkbuf(const char *name, const char *data, uint datalen, char *iovar_buf, uint buflen)
{
	uint iovar_len;
	char *p;

	iovar_len = (uint) strlen(name) + 1;

	/* check for overflow */
	if ((iovar_len + datalen) > buflen) 
		return 0;

	/* copy data to the buffer past the end of the iovar name string */
	if (datalen > 0)
		memmove(&iovar_buf[iovar_len], data, datalen);

	/* copy the name to the beginning of the buffer */
	strcpy(iovar_buf, name);

	/* wl command line automatically converts iovar names to lower case for
	 * ease of use
	 */
	p = iovar_buf;
	while (*p != '\0') {
		*p = tolower((int)*p);
		p++;
	}

	return (iovar_len + datalen);
}

BOOL wpscli_iovar_get(const char *iovar, void *buf, int buf_len)
{
	BOOL bRet;
	brcm_wpscli_status status;

	memcpy(buf, iovar, strlen(iovar));

	status = wpscli_wlh_ioctl_get(WLC_GET_VAR, buf, buf_len);
	bRet = (status == WPS_STATUS_SUCCESS);

	return bRet;
}

BOOL wpscli_iovar_set(const char *iovar, void *param, uint paramlen)
{
	BOOL bRet = FALSE;
	char smbuf[WLC_IOCTL_SMLEN];
	uint iolen;
	brcm_wpscli_status status;
	
	memset(smbuf, 0, sizeof(smbuf));
	iolen = wpscli_iovar_mkbuf(iovar, param, paramlen, smbuf, sizeof(smbuf));
	if (iolen == 0)
	{
		TUTRACE((TUTRACE_ERR, "wpscli_iovar_set(%s, paramlen=%d): wpscli_iovar_mkbuf() failed\n"));
		return bRet;
	}

	status = wpscli_wlh_ioctl_set(WLC_SET_VAR, smbuf, iolen);
	if(status == WPS_STATUS_SUCCESS)
		bRet = TRUE;

	return bRet;
}

#ifdef _TUDEBUGTRACE
/* print bytes formatted as hex to a log file */
void wpscli_log_hexdata(char *heading, unsigned char *data, int dataLen)
{
	int i;
	char dispBuf[4096];		/* debug-output display buffer */
	int dispBufSizeLeft;
	char *dispBufNewLine = "\n                              ";

	if (strlen(heading) >= sizeof(dispBuf))
		return;

	sprintf(dispBuf, "%s", heading);
	dispBufSizeLeft = sizeof(dispBuf) - strlen(dispBuf) - 1;
	for (i = 0; i < dataLen; i++) {
		/* show 20-byte in one row */
		if ((i % 20 == 0) && (dispBufSizeLeft > (int)strlen(dispBufNewLine)))
		{
			strcat(dispBuf, dispBufNewLine);
			dispBufSizeLeft -= strlen(dispBufNewLine);
		}

		/* make sure buffer is large enough, if not, abort it */
		if (dispBufSizeLeft < 3)
			break;
		sprintf(&dispBuf[strlen(dispBuf)], "%02x ", data[i]);
		dispBufSizeLeft -= 3;
	}
	TUTRACE((TUTRACE_INFO,"%s\n", dispBuf));
}

static char *
wpscli_pktflag_name(unsigned int pktflag)
{
	if (pktflag == VNDR_IE_BEACON_FLAG)
		return "Beacon";
	else if (pktflag == VNDR_IE_PRBRSP_FLAG)
		return "Probe Resp";
	else if (pktflag == VNDR_IE_ASSOCRSP_FLAG)
		return "Assoc Resp";
	else if (pktflag == VNDR_IE_AUTHRSP_FLAG)
		return "Auth Resp";
	else if (pktflag == VNDR_IE_PRBREQ_FLAG)
		return "Probe Req";
	else if (pktflag == VNDR_IE_ASSOCREQ_FLAG)
		return "Assoc Req";
	else if (pktflag == VNDR_IE_CUSTOM_FLAG)
		return "Custom";
	else
		return "Unknown";
}
#endif /* _TUDEBUGTRACE */

static BOOL
wpscli_del_vndr_ie(char *bufaddr, int buflen, uint32 frametype)
{
	BOOL bRet = FALSE;
	vndr_ie_setbuf_t *ie_setbuf;
	int iecount, iebuf_len;

#ifdef _TUDEBUGTRACE
	int frag_len = buflen - 6;
	unsigned char *frag = (unsigned char *)(bufaddr + 6);
#endif

	iebuf_len = buflen + sizeof(vndr_ie_setbuf_t) - sizeof(vndr_ie_t);
	ie_setbuf = (vndr_ie_setbuf_t *) malloc(iebuf_len);
	if (!ie_setbuf) {
		TUTRACE((TUTRACE_ERR, "memory alloc failure\n"));
		return FALSE;
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "del");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy((void*)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));
	memcpy((void*)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &frametype, sizeof(uint32));
	memcpy((void*)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data, bufaddr, buflen);

#ifdef _TUDEBUGTRACE
	TUTRACE((TUTRACE_INFO, "wpscli_del_vndr_ie for %s\n", wpscli_pktflag_name(frametype)));
	wpscli_log_hexdata("wpscli_del_vndr_ie (fragment):", frag, frag_len);
#endif

	bRet = wpscli_iovar_set("vndr_ie", ie_setbuf, iebuf_len);

	free(ie_setbuf);

	return bRet;
}

static BOOL
wpscli_set_vndr_ie(unsigned char *frag, int frag_len, unsigned char ouitype, unsigned int pktflag)
{
	BOOL bRet = FALSE;
	vndr_ie_setbuf_t *ie_setbuf;
	int buflen, iecount, i;


	buflen = sizeof(vndr_ie_setbuf_t) + frag_len;
	ie_setbuf = (vndr_ie_setbuf_t *) malloc(buflen);
	if (!ie_setbuf) {
		TUTRACE((TUTRACE_ERR, "memory alloc failure\n"));
		return FALSE;
	}

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "add");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy((void*)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/* 
	 * The packet flag bit field indicates the packets that will
	 * contain this IE
	 */
	memcpy((void*)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer, +1: one byte OUI_TYPE */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uint8) frag_len +
		VNDR_IE_MIN_LEN + 1;

	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0] = 0x00;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[1] = 0x50;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[2] = 0xf2;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0] = ouitype;

	for (i = 0; i < frag_len; i++) {
		ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[i+1] = frag[i];
	}

#ifdef _TUDEBUGTRACE
	TUTRACE((TUTRACE_INFO, "wpscli_set_vndr_ie for %s\n", wpscli_pktflag_name(pktflag)));
	wpscli_log_hexdata("wpscli_set_vndr_ie (fragment):", frag, frag_len);
#endif

	bRet = wpscli_iovar_set("vndr_ie", ie_setbuf, buflen);

	free(ie_setbuf);

	return bRet;
}

/* Parsing TLV format WPS IE */
static unsigned char *
wpscli_get_frag_wps_ie(unsigned char *p_data, int length, int *frag_len, int max_frag_len)
{
	int next_tlv_len, total_len = 0;
	uint16 type;
	unsigned char *next;

	if (!p_data || !frag_len || max_frag_len < 4)
		return NULL;

	if (length <= max_frag_len) {
		*frag_len = length;
		return p_data;
	}

	next = p_data;
	while (1) {
		type = WpsNtohs(next);
		next += 2; /* Move to L */
		next_tlv_len = WpsNtohs(next) + 4; /* Include Type and Value 4 bytes */
		next += 2; /* Move to V */
		if (next_tlv_len > max_frag_len) {
			TUTRACE((TUTRACE_ERR, "Error, there is a TLV length %d bigger than "
				"Max fragment length %d. Unable to fragment it.\n",
				next_tlv_len, max_frag_len));
			return NULL;
		}

		/* Abnormal IE check */
		if ((total_len + next_tlv_len) > length) {
			TUTRACE((TUTRACE_ERR, "Error, Abnormal WPS IE.\n"));
			*frag_len = length;
			return p_data;
		}

		/* Fragment point check */
		if ((total_len + next_tlv_len) > max_frag_len) {
			*frag_len = total_len;
			return p_data;
		}

		/* Get this TLV length */
		total_len += next_tlv_len;
		next += (next_tlv_len - 4); /* Move to next TLV */
	}
}

BOOL wpscli_del_wl_prov_svc_ie(unsigned int cmdtype)
{
	BOOL bRet = FALSE;
	int iebuf_len = 0;
	vndr_ie_setbuf_t *ie_setbuf;
	int iecount, i;

	char setbuf[256] = {0};
	char getbuf[WLC_IOCTL_MEDLEN] = {0};
	vndr_ie_buf_t *iebuf;
	vndr_ie_info_t *ieinfo;
	char wps_oui[4] = {0x00, 0x50, 0xf2, 0x05};
	char *bufaddr;
	int buflen = 0;
	int found = 0;
	uint32 pktflag;
	uint32 frametype;

	TUTRACE((TUTRACE_INFO, "wpscli_del_wl_prov_svc_ie: Entered.\n"));

	if (cmdtype == WPS_IE_TYPE_SET_BEACON_IE)
		frametype = VNDR_IE_BEACON_FLAG;
	else if (cmdtype == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		frametype = VNDR_IE_PRBRSP_FLAG;
	else {
		TUTRACE((TUTRACE_ERR, "wpscli_del_wl_prov_svc_ie: unknown frame type\n"));
		return FALSE;
	}

	if(!wpscli_iovar_get("vndr_ie", getbuf, WLC_IOCTL_MEDLEN)) {
		TUTRACE((TUTRACE_INFO, "wpscli_del_wl_prov_svc_ie: Exiting. Failed to get vndr_ie\n"));
		return FALSE;
	}

	iebuf = (vndr_ie_buf_t *)getbuf;

	bufaddr = (char*) iebuf->vndr_ie_list;

	for (i = 0; i < iebuf->iecount; i++) {
		ieinfo = (vndr_ie_info_t*)bufaddr;
		memmove((char*)&pktflag, (char*)&ieinfo->pktflag, (int)sizeof(uint32));
		if (pktflag == frametype) {
			if (!memcmp(ieinfo->vndr_ie_data.oui, wps_oui, 4))
			{
				found = 1;
				bufaddr = (char*)&ieinfo->vndr_ie_data;
				buflen = (int)ieinfo->vndr_ie_data.len + VNDR_IE_HDR_LEN;
				break;
			}
		}

		/* ieinfo->vndr_ie_data.len represents the together size (number of bytes) of OUI + IE data */ 
		bufaddr = (char *)ieinfo->vndr_ie_data.oui + ieinfo->vndr_ie_data.len;
	}

	if (!found) {
		TUTRACE((TUTRACE_INFO, "wpscli_del_wl_prov_svc_ie: Exiting. No Wireless Provisioning Service IE found.\n"));
		return FALSE;
	}

	iebuf_len = buflen + sizeof(vndr_ie_setbuf_t) - sizeof(vndr_ie_t);
	ie_setbuf = (vndr_ie_setbuf_t *)setbuf;

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "del");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	memcpy((void *)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &frametype, sizeof(uint32));

	memcpy(&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data, bufaddr, buflen);

	bRet = wpscli_iovar_set("vndr_ie", ie_setbuf, iebuf_len);

	TUTRACE((TUTRACE_INFO, "wpscli_del_wl_prov_svc_ie: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

BOOL wpscli_set_wl_prov_svc_ie(unsigned char *p_data, int length, unsigned int cmdtype)
{
	BOOL bRet = FALSE;
	unsigned int pktflag;
	int buflen, iecount, i;
	char ie_buf[256] = { 0 };
	vndr_ie_setbuf_t *ie_setbuf = (vndr_ie_setbuf_t *)ie_buf;

	TUTRACE((TUTRACE_INFO, "wpscli_set_wl_prov_svc_ie: Entered.\n"));

	if (cmdtype == WPS_IE_TYPE_SET_BEACON_IE)
		pktflag = VNDR_IE_BEACON_FLAG;
	else if (cmdtype == WPS_IE_TYPE_SET_PROBE_RESPONSE_IE)
		pktflag = VNDR_IE_PRBRSP_FLAG;
	else {
		TUTRACE((TUTRACE_ERR, "wpscli_set_wl_prov_svc_ie: unknown frame type\n"));
		return FALSE;
	}

	/* Delete wireless provisioning service IE first if it is already existed */
	wpscli_del_wl_prov_svc_ie(cmdtype);

	/* Copy the vndr_ie SET command ("add"/"del") to the buffer */
	strcpy(ie_setbuf->cmd, "add");

	/* Buffer contains only 1 IE */
	iecount = 1;
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.iecount, &iecount, sizeof(int));

	/* 
	 * The packet flag bit field indicates the packets that will
	 * contain this IE
	 */
	memcpy((void *)&ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].pktflag, &pktflag, sizeof(uint32));

	/* Now, add the IE to the buffer */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len = (uint8)length + VNDR_IE_MIN_LEN + 1;

	/* Wireless Provisioning Service vendor IE with OUI 00 50 F2 05 */
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[0] = 0x00;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[1] = 0x50;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.oui[2] = 0xf2;
	ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[0] = 0x05;

	for (i = 0; i < length; i++) {
		ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.data[i+1] = p_data[i];
	}

	buflen = ie_setbuf->vndr_ie_buffer.vndr_ie_list[0].vndr_ie_data.len -
		VNDR_IE_MIN_LEN + sizeof(vndr_ie_setbuf_t) - 1;

	bRet = wpscli_iovar_set("vndr_ie", ie_setbuf, buflen);
	if(!bRet)
		TUTRACE((TUTRACE_ERR, "wpscli_set_wl_prov_svc_ie: wpscli_iovar_set of vndir_ie failed; buflen=%d\n", buflen));
	
	TUTRACE((TUTRACE_INFO, "wpscli_set_wl_prov_svc_ie: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

BOOL wpscli_set_wps_ie(unsigned char *p_data, int length, unsigned int cmdtype)
{
	BOOL bRet = TRUE, ret;
	unsigned int pktflag;
	unsigned char *frag;
	int frag_len;
	int frag_max = WLC_IOCTL_SMLEN - sizeof(vndr_ie_setbuf_t) - strlen("vndr_ie") - 1;


	TUTRACE((TUTRACE_INFO, "wpscli_set_wps_ie: Entered.\n"));
	TUTRACE((TUTRACE_INFO, "\nwpscli_set_wps_ie: iebuf (len=%d):\n", length));

#ifdef _TUDEBUGTRACE
	wpscli_log_hexdata("wpscli_set_wps_ie:", p_data, length);
#endif

	switch (cmdtype) {
	case WPS_IE_TYPE_SET_BEACON_IE:
		pktflag = VNDR_IE_BEACON_FLAG;
		break;
	case WPS_IE_TYPE_SET_PROBE_REQUEST_IE:
		pktflag = VNDR_IE_PRBREQ_FLAG;
		break;
	case WPS_IE_TYPE_SET_PROBE_RESPONSE_IE:
		pktflag = VNDR_IE_PRBRSP_FLAG;
		break;
	case WPS_IE_TYPE_SET_ASSOC_REQUEST_IE:
		pktflag = VNDR_IE_ASSOCREQ_FLAG;
		break;
	case WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE:
		pktflag = VNDR_IE_ASSOCRSP_FLAG;
		break;
	default:
		TUTRACE((TUTRACE_ERR, "wpscli_set_wps_ie: unknown frame type\n"));
		return FALSE;
	}

	/* Always try to delete existing WPS IE first */
	wpscli_del_wps_ie(cmdtype);

	/* Separate a big IE to fragment IEs */
	frag = p_data;
	frag_len = length;
	while (length > 0) {
		if (length > frag_max)
			/* Find a appropriate fragment point */
			frag = wpscli_get_frag_wps_ie(frag, length, &frag_len, frag_max);

		if (!frag) {
			bRet = FALSE;
			goto error;
	}
	
		/* Set fragment WPS IE */
		ret = wpscli_set_vndr_ie(frag, frag_len, 0x4, pktflag);
		if (ret == FALSE)
			bRet = FALSE;

		/* Move to next */
		length -= frag_len;
		frag += frag_len;
		frag_len = length;
	}

error:
	if (bRet == FALSE) {
		TUTRACE((TUTRACE_ERR, "wpscli_set_wps_ie failed\n"));
	}

	TUTRACE((TUTRACE_INFO, "wpscli_set_wps_ie: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

BOOL wpscli_del_wps_ie(unsigned int cmdtype)
{
	BOOL bRet = FALSE, ret;
	char getbuf[WLC_IOCTL_MEDLEN] = {0};
	vndr_ie_buf_t *iebuf;
	vndr_ie_info_t *ieinfo;
	char wps_oui[4] = {0x00, 0x50, 0xf2, 0x04};
	char *bufaddr;
	int buflen = 0;
	int found = 0;
	int i;
	uint32 pktflag;
	uint32 ieinfo_pktflag;

	TUTRACE((TUTRACE_INFO, "wpscli_del_wps_ie: Entered.\n"));

	switch (cmdtype) {
	case WPS_IE_TYPE_SET_BEACON_IE:
		pktflag = VNDR_IE_BEACON_FLAG;
		break;
	case WPS_IE_TYPE_SET_PROBE_REQUEST_IE:
		pktflag = VNDR_IE_PRBREQ_FLAG;
		break;
	case WPS_IE_TYPE_SET_PROBE_RESPONSE_IE:
		pktflag = VNDR_IE_PRBRSP_FLAG;
		break;
	case WPS_IE_TYPE_SET_ASSOC_REQUEST_IE:
		pktflag = VNDR_IE_ASSOCREQ_FLAG;
		break;
	case WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE:
		pktflag = VNDR_IE_ASSOCRSP_FLAG;
		break;
	default:
		TUTRACE((TUTRACE_ERR, "wpscli_set_wps_ie: unknown frame type\n"));
		return FALSE;
	}

	if(!wpscli_iovar_get("vndr_ie", getbuf, WLC_IOCTL_MEDLEN)) {
		TUTRACE((TUTRACE_INFO, "wpscli_del_wps_ie: Exiting. Failed to get vndr_ie\n"));
		return FALSE;
	}

	iebuf = (vndr_ie_buf_t *)getbuf;
	bufaddr = (char*) iebuf->vndr_ie_list;

	/* Delete ALL specified ouitype IEs */
	for (i = 0; i < iebuf->iecount; i++) {
		ieinfo = (vndr_ie_info_t*) bufaddr;
		memmove((char*)&ieinfo_pktflag, (char*)&ieinfo->pktflag, sizeof(uint32));
		if (ieinfo_pktflag == pktflag) {
			if (!memcmp(ieinfo->vndr_ie_data.oui, wps_oui, 4)) {
				found = 1;
				bufaddr = (char*) &ieinfo->vndr_ie_data;
				buflen = (int)ieinfo->vndr_ie_data.len + VNDR_IE_HDR_LEN;
				/* Delete one vendor IE */
				ret = wpscli_del_vndr_ie(bufaddr, buflen, pktflag);
				if (!ret)
					bRet = FALSE;
			}
		}
		bufaddr = (char*)(ieinfo->vndr_ie_data.oui + ieinfo->vndr_ie_data.len);
	}

	if (!found) {
		TUTRACE((TUTRACE_INFO, "wpscli_del_wps_ie: Exiting. No WPS IE found.\n"));
		return FALSE;
	}

	TUTRACE((TUTRACE_INFO, "wpscli_del_wps_ie: Exiting. bRet=%d.\n", bRet));
	return bRet;
}

static void * wpscli_construct_params(DevInfo *devinfo, uint32 pktflag)
{
	void *params = NULL;
	IE_UTILS_BEACON_PARAMS *beacon;
	IE_UTILS_PROBEREQ_PARAMS *probereq;
	IE_UTILS_PROBERESP_PARAMS *proberesp;
	IE_UTILS_ASSOCREQ_PARAMS *assocreq;
	IE_UTILS_ASSOCRESP_PARAMS *assocresp;


	if (devinfo == NULL)
		return NULL;

	/* Allocate parameter memory */
	switch (pktflag) {
	case WPS_IE_TYPE_SET_BEACON_IE:
		beacon = params = malloc(sizeof(IE_UTILS_BEACON_PARAMS));
		break;
	case WPS_IE_TYPE_SET_PROBE_REQUEST_IE:
		probereq = params = malloc(sizeof(IE_UTILS_PROBEREQ_PARAMS));
		break;
	case WPS_IE_TYPE_SET_PROBE_RESPONSE_IE:
		proberesp = params = malloc(sizeof(IE_UTILS_PROBERESP_PARAMS));
		break;
	case WPS_IE_TYPE_SET_ASSOC_REQUEST_IE:
		assocreq = params = malloc(sizeof(IE_UTILS_ASSOCREQ_PARAMS));
		break;
	case WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE:
		assocresp = params = malloc(sizeof(IE_UTILS_ASSOCRESP_PARAMS));
		break;
	default:
		TUTRACE((TUTRACE_INFO, "wpscli_construct_params: Unsupported pktflag 0x%x\n",
			pktflag));
		return NULL;
	}

	if (params == NULL) {
		TUTRACE((TUTRACE_INFO, "wpscli_construct_params: malloc memory failed\n"));
		return NULL;
	}

	/* Set parameters */
	switch (pktflag) {
	case WPS_IE_TYPE_SET_BEACON_IE:
		memset(beacon, 0, sizeof(IE_UTILS_BEACON_PARAMS));

		beacon->version = devinfo->version;
		beacon->scState = devinfo->scState;
		beacon->apLockdown = 0;
		beacon->selReg = devinfo->selRegistrar;
		beacon->devPwdId = devinfo->devPwdId;
		beacon->selRegCfgMethods = devinfo->configMethods;
		memcpy(beacon->uuid_e, devinfo->uuid, SIZE_UUID);
		beacon->rfBand = devinfo->rfBand;
		beacon->primDeviceCategory = devinfo->primDeviceCategory;
		beacon->primDeviceOui = devinfo->primDeviceOui;
		beacon->primDeviceSubCategory = devinfo->primDeviceSubCategory;
		strcpy(beacon->deviceName, devinfo->deviceName);
		if (b_wps_version2) {
			beacon->version2 = devinfo->version2;
			if (devinfo->authorizedMacs_len) {
				beacon->authorizedMacs.len = devinfo->authorizedMacs_len;
				memcpy(beacon->authorizedMacs.macs, devinfo->authorizedMacs,
					devinfo->authorizedMacs_len);
			}
		}
		break;

	case WPS_IE_TYPE_SET_PROBE_REQUEST_IE:
		memset(probereq, 0, sizeof(IE_UTILS_PROBEREQ_PARAMS));

		probereq->version = devinfo->version;
		probereq->reqType = WPS_MSGTYPE_ENROLLEE_INFO_ONLY;
		probereq->configMethods = devinfo->configMethods;
		memcpy(probereq->uuid, devinfo->uuid, SIZE_UUID);
		probereq->primDeviceCategory = devinfo->primDeviceCategory;
		probereq->primDeviceOui = devinfo->primDeviceOui;
		probereq->primDeviceSubCategory = devinfo->primDeviceSubCategory;
		probereq->rfBand = devinfo->rfBand;
		probereq->assocState = devinfo->assocState;
		probereq->configError = devinfo->configError;
		probereq->devPwdId = devinfo->devPwdId;
		probereq->reqDeviceCategory = devinfo->reqDeviceCategory;
		probereq->reqDeviceOui = devinfo->reqDeviceOui;
		probereq->reqDeviceSubCategory = devinfo->reqDeviceSubCategory;
		if (b_wps_version2) {
			probereq->version2 = devinfo->version2;
			strcpy(probereq->manufacturer, devinfo->manufacturer);
			strcpy(probereq->modelName, devinfo->modelName);
			strcpy(probereq->modelNumber, devinfo->modelNumber);
			strcpy(probereq->deviceName, devinfo->deviceName);
			probereq->reqToEnroll = devinfo->b_reqToEnroll;
		}
		break;

	case WPS_IE_TYPE_SET_PROBE_RESPONSE_IE:
		memset(proberesp, 0, sizeof(IE_UTILS_PROBERESP_PARAMS));

		proberesp->version = devinfo->version;
		proberesp->scState = devinfo->scState;
		proberesp->apLockdown = 0;
		proberesp->selReg = devinfo->selRegistrar;
		proberesp->devPwdId = devinfo->devPwdId;
		proberesp->selRegCfgMethods = devinfo->configMethods;
		proberesp->respType = WPS_MSGTYPE_AP_WLAN_MGR;
		memcpy(proberesp->uuid_e, devinfo->uuid, SIZE_UUID);
		strcpy(proberesp->manufacturer, devinfo->manufacturer);
		strcpy(proberesp->modelName, devinfo->modelName);
		strcpy(proberesp->modelNumber, devinfo->modelNumber);
		strcpy(proberesp->serialNumber, devinfo->serialNumber);
		proberesp->primDeviceCategory = devinfo->primDeviceCategory;
		proberesp->primDeviceOui = devinfo->primDeviceOui;
		proberesp->primDeviceSubCategory = devinfo->primDeviceSubCategory;
		strcpy(proberesp->deviceName, devinfo->deviceName);
		proberesp->configMethods = devinfo->configMethods;
		proberesp->rfBand = devinfo->rfBand;
		if (b_wps_version2) {
			proberesp->version2 = devinfo->version2;
			if (devinfo->authorizedMacs_len) {
				proberesp->authorizedMacs.len = devinfo->authorizedMacs_len;
				memcpy(proberesp->authorizedMacs.macs, devinfo->authorizedMacs,
					devinfo->authorizedMacs_len);
			}
		}
		break;

	case WPS_IE_TYPE_SET_ASSOC_REQUEST_IE:
		memset(assocreq, 0, sizeof(IE_UTILS_ASSOCREQ_PARAMS));

		assocreq->version = devinfo->version;
		assocreq->reqType = WPS_MSGTYPE_ENROLLEE_INFO_ONLY;
		if (b_wps_version2)
			assocreq->version2 = devinfo->version2;
		break;

	case WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE:
		memset(assocresp, 0, sizeof(IE_UTILS_ASSOCRESP_PARAMS));

		assocresp->version = devinfo->version;
		assocresp->respType = WPS_MSGTYPE_AP_WLAN_MGR;
		if (b_wps_version2)
			assocresp->version2 = devinfo->version2;
		break;
	}

	return params;
}

BOOL wpscli_build_wps_ie(DevInfo *devinfo, uint8 pbc, uint32 pktflag,
	uint8 *buf, int *buflen)
{
	BOOL bRet = TRUE;
	uint32 ret;
	void *params = NULL;


	/* Sanity check */
	if (devinfo == NULL || buf == NULL || buflen == NULL) {
		TUTRACE((TUTRACE_INFO, "wpscli_add_wps_ie: Invaild arguments\n"));
		bRet = FALSE;
		goto error;
	}

	switch (pktflag) {
	case WPS_IE_TYPE_SET_BEACON_IE:
	case WPS_IE_TYPE_SET_PROBE_REQUEST_IE:
	case WPS_IE_TYPE_SET_PROBE_RESPONSE_IE:
	case WPS_IE_TYPE_SET_ASSOC_REQUEST_IE:
	case WPS_IE_TYPE_SET_ASSOC_RESPONSE_IE:
		break;
	default:
		TUTRACE((TUTRACE_INFO, "wpscli_add_wps_ie: Unsupported pktflag 0x%x\n", pktflag));
		bRet = FALSE;
		goto error;
	}

	/* Get parameter buffer */
	if ((params = wpscli_construct_params(devinfo, pktflag)) == NULL) {
		TUTRACE((TUTRACE_INFO, "wpscli_add_wps_ie: Construct parameters failed\n"));
		bRet = FALSE;
		goto error;
	}

	/* Generate WPS IE data */	
	if (pktflag > IE_UTILS_BUILD_WPS_IE_FUNC_NUM ||
	    wpscli_ie_utils_build_func[pktflag] == NULL) {
		TUTRACE((TUTRACE_INFO, "wpscli_add_wps_ie: No avaliable WPS IE build function\n"));
		bRet = FALSE;
		goto error;
	}

	ret = wpscli_ie_utils_build_func[pktflag](params, buf, buflen);
	if (ret != WPS_SUCCESS) {
		bRet = FALSE;
		goto error;
	}

error:
	if (params)
		free(params);

	return bRet;
}

BOOL wpscli_add_wps_ie(DevInfo *devinfo, uint8 pbc, uint32 pktflag)
{
	BOOL bRet = FALSE;
	uint8 buf[WLC_IOCTL_MEDLEN];
	int buflen = sizeof(buf);


	bRet = wpscli_build_wps_ie(devinfo, pbc, pktflag, buf, &buflen);
	if (bRet == FALSE) {
		TUTRACE((TUTRACE_INFO, "wpscli_add_wps_ie: Build WPS IE failed\n"));
	return bRet;
}

	/* Apply it */
	return wpscli_set_wps_ie(buf, buflen, pktflag);
}
