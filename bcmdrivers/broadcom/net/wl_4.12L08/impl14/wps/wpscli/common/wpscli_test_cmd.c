/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_test_cmd.c 282714 2011-09-09 01:15:50Z $
 *
 * Description: WPSCLI library test console program
 *
 */
#include <stdio.h>
#include <string.h>
#include <wpscli_api.h>
#include <ctype.h>
#include <reg_prototlv.h>


#ifdef WPSCLI_WSCV2
static BOOL b_wps_version2 = TRUE;
#else
static BOOL b_wps_version2 = FALSE;
#endif

static char def_pin[9] = "12345670\0";

#define REMOVE_NEWLINE(buf)	{ \
	int i; \
	for (i = 0; i < sizeof(buf); i++) { \
		if (buf[i] == '\n') \
		buf[i] = '\0'; \
	} \
}

#define WPS_APLIST_BUF_SIZE \
	(BRCM_WPS_MAX_AP_NUMBER*sizeof(brcm_wpscli_ap_entry) + sizeof(brcm_wpscli_ap_list))

const static char ZERO_MAC[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

#if defined(WIN32) || defined(_WIN32_WCE)
#ifdef WIN32
/* Compile Win32 Big Window */
/* Desktop PCI multi-band wlan adapter */
#define SOFTAP_IF_NAME			"{C87FCF3B-B2AE-4D06-9704-D12A7D5568DC}"

/* Desktop PCI Multi-band wlan adapter */
#define WL_ADAPTER_IF_NAME		"{EE502175-BAB0-4BBE-A398-627A8FF3ED91}"
#else /* _WIN32_WCE */
/* Compile WinMobile */
#define WL_ADAPTER_IF_NAME		"BCMSDDHD1"
/* BCMSDDHD1 is the primary interface */
#define SOFTAP_IF_NAME			"BCMSDDHD1"
#endif /* WIN32 */
#else /* !WIN32 && !_WIN32_WCE */
/* Compile Linux */
#define SOFTAP_IF_NAME			"wl0.1"
#define WL_ADAPTER_IF_NAME		NULL
#endif /* WIN32 || _WIN32_WCE */


const char *ENCR_STR[] = {"None", "WEP", "TKIP", "AES"};
const char *AUTH_STR[] = {"OPEN", "SHARED", "WPA-PSK", "WPA2-PSK"};
const char *STATUS_STR[] = {
	/* Generic WPS library errors */
	"WPS_STATUS_SUCCESS",
	/* generic error not belonging to any other definition */
	"WPS_STATUS_SYSTEM_ERR",
	/* failed to open/init wps adapter */
	"WPS_STATUS_OPEN_ADAPTER_FAIL",
	/* user cancels the connection */
	"WPS_STATUS_ABORTED",
	/* invalid NULL parameter passed in */
	"WPS_STATUS_INVALID_NULL_PARAM",
	/* more memory is required to retrieve data */
	"WPS_STATUS_NOT_ENOUGH_MEMORY",
	/* Invalid network settings */
	"WPS_STATUS_INVALID_NW_SETTINGS",
	"WPS_STATUS_WINDOW_NOT_OPEN",

	/* WPS protocol related errors */
	"WPS_STATUS_PROTOCOL_SUCCESS",
	"WPS_STATUS_PROTOCOL_INIT_FAIL",
	"WPS_STATUS_PROTOCOL_INIT_SUCCESS",
	"WPS_STATUS_PROTOCOL_START_EXCHANGE",
	"WPS_STATUS_PROTOCOL_CONTINUE",
	"WPS_STATUS_PROTOCOL_SEND_MEG",
	"WPS_STATUS_PROTOCOL_WAIT_MSG",
	"WPS_STATUS_PROTOCOL_RECV_MSG",
	/* timeout and fails in M1-M8 negotiation */
	"WPS_STATUS_PROTOCOL_FAIL_TIMEOUT",
	/* don't retry any more because of EAP timeout as AP gives up already. */
	"WPS_STATUS_PROTOCOL_FAIL_MAX_EAP_RETRY",
	/* PBC session overlap */
	"WPS_STATUS_PROTOCOL_FAIL_OVERLAP",
	/* fails in protocol processing stage because of unmatched pin number */
	"WPS_STATUS_PROTOCOL_FAIL_WRONG_PIN",
	/* fails because of EAP failure */
	"WPS_STATUS_PROTOCOL_FAIL_EAP",
	/* after wps negotiation, unexpected network credentials are received */
	"WPS_STATUS_PROTOCOL_FAIL_UNEXPECTED_NW_CRED",
	/* after wps negotiation, unexpected network credentials are received */
	"WPS_STATUS_PROTOCOL_FAIL_PROCESSING_MSG",

	/* WL handler related status code */
	"WPS_STATUS_SET_BEACON_IE_FAIL",
	"WPS_STATUS_DEL_BEACON_IE_FAIL",
	/* failed to set iovar */
	"WPS_STATUS_IOCTL_SET_FAIL",
	/* failed to get iovar */
	"WPS_STATUS_IOCTL_GET_FAIL",

	/* WLAN related status code */
	"WPS_STATUS_WLAN_INIT_FAIL",
	"WPS_STATUS_WLAN_SCAN_START",
	"WPS_STATUS_WLAN_NO_ANY_AP_FOUND",
	"WPS_STATUS_WLAN_NO_WPS_AP_FOUND",
	/* preliminary association failed */
	"WPS_STATUS_WLAN_CONNECTION_START",
	/* preliminary association failed */
	"WPS_STATUS_WLAN_CONNECTION_ATTEMPT_FAIL",
	/* preliminary association lost during registration */
	"WPS_STATUS_WLAN_CONNECTION_LOST",
	"WPS_STATUS_WLAN_CONNECTION_DISCONNECT",

	/* Packet dispatcher related erros */
	"WPS_STATUS_PKTD_INIT_FAIL",
	/* Generic packet dispatcher related errors not belonging any other definition */
	"WPS_STATUS_PKTD_SYSTEM_FAIL",
	/* failed to send eapol packet */
	"WPS_STATUS_PKTD_SEND_PKT_FAIL",
	"WPS_STATUS_PKTD_NO_PKT",
	/* received packet is not eap packet */
	"WPS_STATUS_PKTD_NOT_EAP_PKT"
};

unsigned char* ConverMacAddressStringIntoByte(const char *pszMACAddress, unsigned char* pbyAddress)
{
	const char cSep = '-';
	int iConunter;

	for (iConunter = 0; iConunter < 6; ++iConunter)
	{
		unsigned int iNumber = 0;
		char ch;

		/* Convert letter into lower case. */
		ch = tolower(*pszMACAddress++);

		if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
		{
			return NULL;
		}

		iNumber = isdigit(ch) ? (ch - '0') : (ch - 'a' + 10);
		ch = tolower(*pszMACAddress);

		if ((iConunter < 5 && ch != cSep) ||
			(iConunter == 5 && ch != '\0' && !isspace(ch)))
		{
			++pszMACAddress;

			if ((ch < '0' || ch > '9') && (ch < 'a' || ch > 'f'))
			{
				return NULL;
			}

			iNumber <<= 4;
			iNumber += isdigit(ch) ? (ch - '0') : (ch - 'a' + 10);
			ch = *pszMACAddress;

			if (iConunter < 5 && ch != cSep)
			{
				return NULL;
			}
		}
		/* Store result.  */
		pbyAddress[iConunter] = (unsigned char) iNumber;
		/* Skip cSep.  */
		++pszMACAddress;
	}
	return pbyAddress;
}

void test_softap_side(const char *pin, char *softap_if_name, brcm_wpscli_nw_settings *nw_settings)
{
	brcm_wpscli_status status;
	char inp[3];
	uint8 sta_mac[6] = { 0 };

	printf("Calling brcm_wpscli_open\n");

	if (softap_if_name[0] == '\0')  /* Use default */
		strcpy(softap_if_name, SOFTAP_IF_NAME);

	status = brcm_wpscli_open(softap_if_name, BRCM_WPSCLI_ROLE_SOFTAP, NULL, NULL);
	if (status != WPS_STATUS_SUCCESS) {
		printf("brcm_wpscli_open failed. System error!\n");
		goto SOFTAP_END;
	}

	/* Prepare configuration */
	status = wpscli_softap_construct_def_devinfo();
	if (status != WPS_STATUS_SUCCESS) {
		printf("Failed to construct device informations. status=%s\n", STATUS_STR[status]);
		goto SOFTAP_END;
	}

	status = brcm_wpscli_softap_enable_wps("Broadcom",
		b_wps_version2 ?
		(WPS_CONFMET_LABEL | WPS_CONFMET_PBC | WPS_CONFMET_VIRT_PBC) :
		(WPS_CONFMET_LABEL | WPS_CONFMET_PBC),
		NULL, 0); /* For now, empty AuthorizedMACs */
	if (status != WPS_STATUS_SUCCESS) {
		printf("Failed to enable softap wps. status=%s\n", STATUS_STR[status]);
		goto SOFTAP_END;
	}

	/*
	 * Set initial context for enrollee mode (could set more default values).
	 * In general, it could be nice to define a structure to pass down context
	 * parameters to the wps library instead of passing many parameters.
	 */
	brcm_wpscli_softap_set_wps_context(nw_settings, pin ? pin : "00000000",
		NULL, 0); /* use wildcard AuthorizedMacs */

SOFTAP_ESTART:
	printf("Adding WPS enrollee and waiting for enrollee to connect...\n");

	if (pin == NULL || strlen(pin) == 0) {
		/* use wildcard AuthorizedMacs */
		status = brcm_wpscli_softap_start_wps(BRCM_WPS_MODE_STA_ENR_JOIN_NW,
			BRCM_WPS_PWD_TYPE_PBC, NULL, nw_settings, 60, sta_mac, NULL, 0);
	}
	else {
		/* use wildcard AuthorizedMacs */
		status = brcm_wpscli_softap_start_wps(BRCM_WPS_MODE_STA_ENR_JOIN_NW,
			BRCM_WPS_PWD_TYPE_PIN, pin, nw_settings, 60, sta_mac, NULL, 0);
	}

	if (status != WPS_STATUS_SUCCESS) {
		printf("brcm_wpscli_softap_start_wps failed. System error!\n");
		brcm_wpscli_softap_close_session();
		goto SOFTAP_END;
	}

	while (1) {
#ifdef WPSCLI_NO_WPS_LOOP
		/*
		 * For wpscli test command line, we don't support WPSCLI_NO_WPS_LOOP,
		 * Here the code is just make compile happy when WPSCLI_NO_WPS_LOOP specified.
		 * For example the dongle builds which WPS used by P2P.
		 */
		status = brcm_wpscli_softap_process_eapwps(NULL, 0, NULL, sta_mac);
#else
		/* wait for next packet and process depending on the state of the window */
		status = brcm_wpscli_softap_process_eapwps(sta_mac);
#endif

		/* if the window was open and the registration finished, close the window  */
		if (status == WPS_STATUS_SUCCESS) {
			printf("\n\nWPS negotiation is successful!\n");
			printf("\n\n");
			break;
		}
		else if (status == WPS_STATUS_PROTOCOL_CONTINUE) {
			continue;
		}
		else {
			brcm_wpscli_softap_close_session();
			printf("\n\nWPS negotiation is failed. status=%d!\n", status);
			printf("\n\n");
			goto SOFTAP_END;
		}
	}

	/* Close wps windows session */
	brcm_wpscli_softap_close_session();

	printf("\n\nDo you want to process another enrollment request (y/n)?: ");
	fflush(stdin);
	fgets(inp, sizeof(inp), stdin);
	fflush(stdin);
	if (inp[0] == 'y' || inp[0] == 'Y')
		goto SOFTAP_ESTART;

SOFTAP_END:
	brcm_wpscli_softap_disable_wps();
	brcm_wpscli_close();

	printf("\n\nWPS registrar is exitting....\n");
}

static int
display_aplist(brcm_wpscli_ap_entry *ap, uint32 ap_total)
{
	int i, j;
	uint8 *mac;
	uint8 empty_mac[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

	if (!ap)
		return 0;

	printf("-------------------------------------------------------\n");
	for (i = 0; i < ap_total; i++) {
		printf(" %d :  ", i+1);
		printf("SSID:%s  ", ap->ssid);
		printf("BSSID:%02x:%02x:%02x:%02x:%02x:%02x  ",
			ap->bssid[0], ap->bssid[1], ap->bssid[2],
			ap->bssid[3], ap->bssid[4], ap->bssid[5]);
		if (ap->wsec)
			printf("WEP  ");
		if (ap->scstate == BRCM_WPS_SCSTATE_CONFIGURED)
			printf("Configured  ");
		printf("%s  ", ap->pwd_type == BRCM_WPS_PWD_TYPE_PBC ? "PBC" : "PIN");
		if (b_wps_version2 && ap->version2 >= 0x20) {
			printf("V2(0x%02X)  ", ap->version2);

			mac = ap->authorizedMACs;
			printf("AuthorizedMACs:");
			for (j = 0; j < 5; j++) {
				if (memcmp(mac, empty_mac, SIZE_MAC_ADDR) == 0)
					break;

				printf(" %02x:%02x:%02x:%02x:%02x:%02x",
					mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
				mac += SIZE_MAC_ADDR;
			}
		}
		printf("\n");
		ap++;
	}

	printf("-------------------------------------------------------\n");
	return 0;
}

void test_sta_side(const char *pin)
{
	brcm_wpscli_status status;
	uint32 nAP = 0;
	char buf[WPS_APLIST_BUF_SIZE] = { 0 };
	uint32 ap_total;
	brcm_wpscli_ap_entry *ap;
	brcm_wpscli_nw_settings nw_cred;
	char inp[3];

	status = brcm_wpscli_open(WL_ADAPTER_IF_NAME, BRCM_WPSCLI_ROLE_STA, NULL, NULL);
	if (status != WPS_STATUS_SUCCESS) {
		printf("Failed to initialize wps library. status=%s\n", STATUS_STR[status]);
		goto END;
	}

	/* Prepare configuration */
	status = wpscli_sta_construct_def_devinfo();
	if (status != WPS_STATUS_SUCCESS) {
		printf("Failed to construct device informations. status=%s\n", STATUS_STR[status]);
		goto END;
	}

	printf("Searching WPS APs...\n");
	status = brcm_wpscli_sta_search_wps_ap(&nAP);

	if (status == WPS_STATUS_SUCCESS) {
		printf("WPS APs are found and there are %d of them:\n", nAP);

		/* Get the list of wps APs */
		status = brcm_wpscli_sta_get_wps_ap_list((brcm_wpscli_ap_list *)buf,
			sizeof(brcm_wpscli_ap_entry)*BRCM_WPS_MAX_AP_NUMBER,
			&ap_total);
		if (status == WPS_STATUS_SUCCESS)
		{
			display_aplist(((brcm_wpscli_ap_list *)buf)->ap_entries, ap_total);
			printf("\n");
SEL:
			printf("Please enter the AP number you wish to "
				"connect to or [x] to quit: ");
			fflush(stdin);
			fgets(inp, sizeof(inp), stdin);
			fflush(stdin);

			if (inp[0] == 'x' || inp[0] == 'X')
				goto END;

			if (inp[0] > '9' || inp[0] < '1')
				goto SEL;

			ap = &(((brcm_wpscli_ap_list *)buf)->ap_entries[inp[0]-'1']);

			printf("\nConnecting to WPS AP [%s] "
				"and negotiating WPS protocol in %s mode\n",
				ap->ssid,
				ap->pwd_type == BRCM_WPS_PWD_TYPE_PBC? "PBC" : "PIN");

			if (ap->pwd_type == BRCM_WPS_PWD_TYPE_PBC)
			{
				/* Continue to make connection */
				status = brcm_wpscli_sta_start_wps(ap->ssid,
					ap->wsec,
					ap->bssid,
					0, 0,
					BRCM_WPS_MODE_STA_ENR_JOIN_NW,
					BRCM_WPS_PWD_TYPE_PBC,
					NULL,
					120,
					&nw_cred);
			}
			else
			{
				/* Replace if no user specified PIN */
				if (pin == NULL || strlen(pin) == 0)
					pin = def_pin;

				status = brcm_wpscli_sta_start_wps(ap->ssid,
					ap->wsec,
					ap->bssid,
					0, 0,
					BRCM_WPS_MODE_STA_ENR_JOIN_NW,
					BRCM_WPS_PWD_TYPE_PIN,
					pin,
					120,
					&nw_cred);
			}

			/* Print out the result */
			if (status == WPS_STATUS_SUCCESS)
			{
				printf("\nWPS negotiation is successful!\n");
				printf("\nWPS AP Credentials:\n");
				printf("  SSID: %s\n", nw_cred.ssid);
				printf("  Key Mgmt type: %s\n", AUTH_STR[nw_cred.authType]);
				printf("  Encryption type: %s\n", ENCR_STR[nw_cred.encrType]);
				printf("  Network key: %s\n", nw_cred.nwKey);
				if (b_wps_version2)
					printf("  Network Key Shareable :  %s\n",
						nw_cred.nwKeyShareable ? "TRUE" : "FALSE");
				printf("\n\n");
			}
			else
				printf("WPS negotiation is failed. status=%s\n",
					STATUS_STR[status]);
		}
		else
		{
			printf("Failed to get wps ap list. status=%s\n", STATUS_STR[status]);
		}
	}
	else {
		printf("No WPS AP is found. status=%s\n", STATUS_STR[status]);
	}

END:
	/* Remove WPS IE */
	brcm_wpscli_sta_rem_wps_ie();

	brcm_wpscli_close();
}

static int
print_usage()
{
	printf("Usage : \n\n");
	printf("    wpscli_test_cmd <-if eth_name> <-ssid ssid_name> <-pin pin> <-pb> <-ap> \n\n");
	printf("    Default values :\n");
	printf("       WPS Enrollee \n");
	printf("       interface_name :  wl0.1\n");
	printf("       ssid_name : softap_wps \n");
	printf("       pin : 12345670\n");
	return 0;
}

int main(int argc, char* argv[])
{
	char inp[3], pin[80] = { 0 };
	char if_name[80] = { 0 };
	int index;
	char *cmd, *val;
	int pwd_type = -1;
	char ssid[SIZE_32_BYTES+1] = "softap_wps";
	brcm_wpscli_nw_settings nw_settings;
	brcm_wpscli_role role = BRCM_WPSCLI_ROLE_STA;  /* STA mode by default */

	/* parse arguments */
	argc--;
	index = 1;
	while (argc) {
		cmd = argv[index++]; argc--;
		if (!strcmp(cmd, "-help")) {
			print_usage();
			return 0;
		}
		else if (!strcmp(cmd, "-ap")) {
			role = BRCM_WPSCLI_ROLE_SOFTAP;
		}
		else if (!strcmp(cmd, "-if")) {
			val = argv[index++]; argc--;
			strcpy(if_name, val);
		}
		else if (!strcmp(cmd, "-ssid")) {
			val = argv[index++]; argc--;
			strcpy((char *)ssid, val);
		}
		else if (!strcmp(cmd, "-pin")) {
			val = argv[index++]; argc--;
			strcpy(pin, val);
			/* Validate user entered PIN */
			if (brcm_wpscli_validate_pin(pin) != WPS_STATUS_SUCCESS) {
				printf("\tInvalid PIN number parameter: %s\n", pin);
				print_usage();
				return 0;
			}
			pwd_type = BRCM_WPS_PWD_TYPE_PIN;
		}
		else if (!strcmp(cmd, "-pb")) {
			pin[0] = '\0';
			pwd_type = BRCM_WPS_PWD_TYPE_PBC;
		}
		else {
			printf("Invalid parameter : %s\n", cmd);
			print_usage();
			return 0;
		}
	}

	/* If no "-ap" argument, use old user selection */
	if (role != BRCM_WPSCLI_ROLE_SOFTAP) {
		printf("*********** WPSCLI Test (if_name=%s)****************\n", if_name);
		printf("1. Run WPS for SoftAP side as Registrar.\n");
		printf("2. Run WPS for STA side as Enrollee.\n");
		printf("e. Exit the test.\n");

		printf("\n");
		while (1) {
			printf("Please make selection: ");

			fflush(stdin);
			fgets(inp, sizeof(inp), stdin);
			fflush(stdin);

			if (inp[0] == '1') {
				role = BRCM_WPSCLI_ROLE_SOFTAP;
				break;
			}
			else if (inp[0] == '2') {
				role = BRCM_WPSCLI_ROLE_STA;
				break;
			}
			else if (inp[0] == 'e' || inp[0] == 'E')
				return 0;
		}
		printf("\n*********************************************\n");
	}

	if (role == BRCM_WPSCLI_ROLE_STA)
		printf("WPS - STA Enrollee APP Broadcom Corp.\n");
	else
		printf("WPS - SoftAP Registrar APP Broadcom Corp.\n");
	printf("*********************************************\n");

	if (pwd_type == -1) {
get_pin:
		printf("\nIf you have a pin, enter it now, otherwise press ENTER: ");
		fflush(stdin);
		fgets(pin, 80, stdin);
		fflush(stdin);
		if (pin[0] != '\n') {
			REMOVE_NEWLINE(pin);
			if (brcm_wpscli_validate_pin(pin) != WPS_STATUS_SUCCESS) {
				printf("\tInvalid PIN number parameter: %s", pin);
				goto get_pin;
			}
		}
	}

	if (pin[0] == '\n') {
		printf("pbc mode \n");
		pwd_type = BRCM_WPS_PWD_TYPE_PBC;
		pin[0] = '\0';
	} else {
		printf("pin = %s\n", pin);
		pwd_type = BRCM_WPS_PWD_TYPE_PIN;
	}

	/* Going to start STA or SOFAP WPS */
	if (role == BRCM_WPSCLI_ROLE_STA)
		test_sta_side(pin);
	else {
		/* Start nw_settings configuration */
		printf("\nEnter pre-shared key or press ENTER for open mode: ");
		fflush(stdin);
		fgets(nw_settings.nwKey, 80, stdin);
		fflush(stdin);
		if (nw_settings.nwKey[0] == '\n') {
			nw_settings.nwKey[0] = '\0';
			nw_settings.encrType = BRCM_WPS_ENCRTYPE_NONE;
			nw_settings.authType = BRCM_WPS_AUTHTYPE_OPEN;
			printf("open mode\n");
		} else {
			REMOVE_NEWLINE(nw_settings.nwKey);
			printf("ap pmk = %s\n", nw_settings.nwKey);
			nw_settings.nwKey[strlen(nw_settings.nwKey)] = '\0';

			/* Select Authentication Type */
			printf("1. Shared.\n");
			printf("2. WPAPSK.\n");
			printf("3. WPA2PSK.\n");
			printf("4. WPAPSK_WPA2PSK.\n");
			printf("Default: WPA2PSK\n");
			printf("Select authentication type: ");
			fflush(stdin);
			fgets(inp, sizeof(inp), stdin);
			fflush(stdin);

			switch (inp[0]) {
			case '1':
				nw_settings.authType = BRCM_WPS_AUTHTYPE_SHARED;
				break;
			case '2':
				nw_settings.authType = BRCM_WPS_AUTHTYPE_WPAPSK;
				break;
			case '3':
				nw_settings.authType = BRCM_WPS_AUTHTYPE_WPA2PSK;
				break;
			case '4':
				nw_settings.authType = BRCM_WPS_AUTHTYPE_WPAPSK_WPA2PSK;
				break;
			default:
				nw_settings.authType = BRCM_WPS_AUTHTYPE_WPA2PSK;
				break;
			}

			/* Select encription type */
			printf("1. WEP.\n");
			printf("2. TKIP.\n");
			printf("3. AES.\n");
			printf("4. TKIP_AES.\n");
			printf("Default: AES\n");
			printf("Select encription type: ");
			fflush(stdin);
			fgets(inp, sizeof(inp), stdin);
			fflush(stdin);

			switch (inp[0]) {
			case '1':
				nw_settings.encrType = BRCM_WPS_ENCRTYPE_WEP;
				break;
			case '2':
				nw_settings.encrType = BRCM_WPS_ENCRTYPE_TKIP;
				break;
			case '3':
				nw_settings.encrType = BRCM_WPS_ENCRTYPE_AES;
				break;
			case '4':
				nw_settings.encrType = BRCM_WPS_ENCRTYPE_TKIP_AES;
				break;
			default:
				nw_settings.encrType = BRCM_WPS_ENCRTYPE_AES;
				break;
			}
		}

		strcpy(nw_settings.ssid, ssid);
		nw_settings.wepIndex = 0;

		printf("if_name = %s\n", if_name);
		printf("pin = %s\n", pin);
		printf("ssid = %s\n", nw_settings.ssid);
		printf("pmk = %s\n", nw_settings.nwKey);
		printf("auth = %d\n", nw_settings.authType);
		printf("enr = %d\n", nw_settings.encrType);
		printf("\nPress any key to start...");
		fflush(stdin);
		getchar();
		test_softap_side(pin, if_name, &nw_settings);
	}
	fflush(stdin);
	printf("\nPress ENTER to exit...");
	fflush(stdin);
	getchar();

	return 0;
}
