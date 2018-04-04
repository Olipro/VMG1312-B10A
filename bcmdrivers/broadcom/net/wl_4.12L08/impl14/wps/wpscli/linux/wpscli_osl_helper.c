/*
 * Copyright (C) 2011, Broadcom Corporation
 * All Rights Reserved.
 * 
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Broadcom Corporation;
 * the contents of this file may not be disclosed to third parties, copied
 * or duplicated in any form, in whole or in part, without the prior
 * written permission of Broadcom Corporation.
 *
 * $Id: wpscli_osl_helper.c 273043 2011-07-21 18:25:56Z $
 *
 * Description: Implement Linux OSL helper functions
 *
 */
#include <string.h>
#include <wpscli_osl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdarg.h>
#include <tutrace.h>

extern void RAND_linux_init();

void wpscli_rand_init()
{
	TUTRACE((TUTRACE_INFO, "wpscli_rand_init: Entered.\n"));

	RAND_linux_init();

	TUTRACE((TUTRACE_INFO, "wpscli_rand_init: Exiting.\n"));
}

uint16 wpscli_htons(uint16 v)
{
	return htons(v);
}

uint32 wpscli_htonl(uint32 v)
{
	return htonl(v);
}

uint16 wpscli_ntohs(uint16 v)
{
	return ntohs(v);
}

uint32 wpscli_ntohl(uint32 v)
{
	return ntohl(v);
}

#ifdef _TUDEBUGTRACE
void wpscli_print_buf(char *text, unsigned char *buff, int buflen)
{
	char str[512];
	int i;

	/* print to both stdio and log file */
	sprintf(str, "\n%s : %d\n", text, buflen);
	printf("%s", str);
	TUTRACE((TUTRACE_INFO, "%s", str));
	str[0] = '\0';
	for (i = 0; i < buflen; i++) {
		char str1[32];
		sprintf(str1, "%02X ", buff[i]);
		strcat(str, str1);
		if (!((i+1)%16)) {
			strcat(str, "\n");
			printf("%s", str);
			TUTRACE((TUTRACE_INFO, "%s", str));
			str[0] = '\0';
		}
	}
	strcat(str, "\n");
	printf("%s", str);
	TUTRACE((TUTRACE_INFO, "%s", str));
}
#endif
