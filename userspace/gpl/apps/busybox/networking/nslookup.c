/* vi: set sw=4 ts=4: */
/*
 * Mini nslookup implementation for busybox
 *
 * Copyright (C) 1999,2000 by Lineo, inc. and John Beppu
 * Copyright (C) 1999,2000,2001 by John Beppu <beppu@codepoet.org>
 *
 * Correct default name server display and explicit name server option
 * added by Ben Zeckel <bzeckel@hmc.edu> June 2001
 *
 * Licensed under GPLv2 or later, see file LICENSE in this tarball for details.
 */

#include <resolv.h>
#include "libbb.h"
#if 1 /* __MSTC__, Ailsa, Support nslookup, 20110607*/
#include <stdint.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <resolv.h>
#include <arpa/inet.h>
#include "busybox.h"
#include <net/if.h>
#include "cms_msg.h"
#include "cms_util.h"
#include "cms_log.h"
static void *msgHandle=NULL;
static CmsEntityId requesterId =0;
int writeFile = 0;
#define NSLOOKUP_FINISHED 	0
#define NSLOOKUP_ERROR 		-1
#include <unistd.h>
extern char *optarg;
extern int optind, optopt, opterr;
typedef struct _DNSResult{
	char serverName[BUFLEN_512]; 	/* 	ServerName */
	char serverAddress[BUFLEN_24];	/* 	ServerAddress 	*/
	char DNSName[BUFLEN_512];	 	/* 	DNSName */
	char DNSAddress[BUFLEN_512];	/* 	DNSAddress */
}DNSResult;
int status = NSLOOKUP_FINISHED;
DNSResult *r;
extern int h_errno;
#endif /* __MSTC__, Ailsa, Support nslookup*/

/*
 * I'm only implementing non-interactive mode;
 * I totally forgot nslookup even had an interactive mode.
 *
 * This applet is the only user of res_init(). Without it,
 * you may avoid pulling in _res global from libc.
 */

/* Examples of 'standard' nslookup output
 * $ nslookup yahoo.com
 * Server:         128.193.0.10
 * Address:        128.193.0.10#53
 *
 * Non-authoritative answer:
 * Name:   yahoo.com
 * Address: 216.109.112.135
 * Name:   yahoo.com
 * Address: 66.94.234.13
 *
 * $ nslookup 204.152.191.37
 * Server:         128.193.4.20
 * Address:        128.193.4.20#53
 *
 * Non-authoritative answer:
 * 37.191.152.204.in-addr.arpa     canonical name = 37.32-27.191.152.204.in-addr.arpa.
 * 37.32-27.191.152.204.in-addr.arpa       name = zeus-pub2.kernel.org.
 *
 * Authoritative answers can be found from:
 * 32-27.191.152.204.in-addr.arpa  nameserver = ns1.kernel.org.
 * 32-27.191.152.204.in-addr.arpa  nameserver = ns2.kernel.org.
 * 32-27.191.152.204.in-addr.arpa  nameserver = ns3.kernel.org.
 * ns1.kernel.org  internet address = 140.211.167.34
 * ns2.kernel.org  internet address = 204.152.191.4
 * ns3.kernel.org  internet address = 204.152.191.36
 */
#if 1 /* __MSTC__, Ailsa, Support nslookup, 20110607*/
	void usage()
	{
		fprintf(stderr, "\nUsage: nslookup -h [HOST] <-s [DNS server]>\n");
		fprintf(stderr, "Queries the nameserver for the IP address of the given HOST\n");
		fprintf(stderr, "optionally using a specified DNS server\n");
	
	}
	static void sendEventMessage(void)
	{
			char buf[sizeof(CmsMsgHeader) + sizeof(NslookupDataMsgBody)];
			CmsMsgHeader *msg=(CmsMsgHeader *) buf;
			NslookupDataMsgBody *nslookupData = (NslookupDataMsgBody*) (msg+1);
			CmsRet ret = CMSRET_SUCCESS;
		
			cmsLog_debug("Status %d, Server Name/IP %s,%s, DNS Name/IP %s/%s",
					status,r->serverName,r->serverAddress,r->DNSName,r->DNSAddress);
	
			memset(buf,0,sizeof(CmsMsgHeader) + sizeof(NslookupDataMsgBody));
			msg->type = CMS_MSG_NSLOOKUP_STATE_CHANGED;
			msg->src = EID_NSLOOKUP;
			msg->dst = EID_SSK;
			msg->flags_event = 1;
			msg->dataLength = sizeof(NslookupDataMsgBody);
		
			if (status == NSLOOKUP_FINISHED)
				sprintf(nslookupData->diagnosticsState,MDMVS_COMPLETE);
			else
				sprintf(nslookupData->diagnosticsState,MDMVS_ERROR_CANNOTRESOLVEHOSTNAME);
	
			sprintf(nslookupData->serverName,r->serverName);
			sprintf(nslookupData->serverAddress,r->serverAddress);
			sprintf(nslookupData->DNSName,r->DNSName);
			sprintf(nslookupData->DNSAddress,r->DNSAddress);
			nslookupData->requesterId = requesterId;
	
			if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
				cmsLog_error("could not send out CMS_MSG_NSLOOKUP_STATE_CHANGED to SSK, ret=%d", ret);
			else
				cmsLog_notice("sent out CMS_MSG_NSLOOKUP_STATE_CHANGED (finish=%d) to SSK", status);
			
			if (requesterId != 0)
			{
				msg->dst = requesterId;
				if ((ret = cmsMsg_send(msgHandle, msg)) != CMSRET_SUCCESS)
					cmsLog_error("could not send out CMS_MSG_NSLOOKUP_STATE_CHANGED to requestId %d, ret=%d", 
							(int)requesterId, ret);
				else
					cmsLog_notice("sent out CMS_MSG_NSLOOKUP_STATE_CHANGED (finish=%d) to requesterId %d",
							status,(int)requesterId);
			}
	
	}
#endif /* __MSTC__, Ailsa, Support nslookup*/

static int print_host(const char *hostname, const char *header)
{
	/* We can't use xhost2sockaddr() - we want to get ALL addresses,
	 * not just one */
	struct addrinfo *result = NULL;
	int rc;
	struct addrinfo hint;

	memset(&hint, 0 , sizeof(hint));
	/* hint.ai_family = AF_UNSPEC; - zero anyway */
	/* Needed. Or else we will get each address thrice (or more)
	 * for each possible socket type (tcp,udp,raw...): */
	hint.ai_socktype = SOCK_STREAM;
	// hint.ai_flags = AI_CANONNAME;
	rc = getaddrinfo(hostname, NULL /*service*/, &hint, &result);

	if (!rc) {
		struct addrinfo *cur = result;
		unsigned cnt = 0;
		#if 1//__MSTC__,Ailsa,nslookup,20110607
		char *revhostname = NULL;
		revhostname=(INET_rresolve((struct sockaddr_in*)cur->ai_addr, 0x4000, 0xffffffff));	
		if(revhostname != NULL)
			printf("%-10s %s\n", header, revhostname);
		else
		printf("%-10s %s\n", header, hostname);
		#endif
		// puts(cur->ai_canonname); ?
		while (cur) {
			char *dotted, *revhost;
			dotted = xmalloc_sockaddr2dotted_noport(cur->ai_addr);
			revhost = xmalloc_sockaddr2hostonly_noport(cur->ai_addr);

			printf("Address %u: %s%c", ++cnt, dotted, revhost ? ' ' : '\n');
		#if 1 //__MSTC__,Ailsa, nslookup, 20110607
	        if(cmsUtl_strcmp(header,"Server:") == 0)
			{
				if(revhost != NULL)
					strcpy(r->serverName,revhost);
				else
					strcpy(r->serverName,hostname);
			strcpy(r->serverAddress,dotted);
			}else{
				if(revhost != NULL)
					strcpy(r->DNSName,revhost);
				else
					strcpy(r->DNSName,hostname);
			strcat(r->DNSAddress,dotted);
			strcat(r->DNSAddress,",");
			}
         #endif
			if (revhost) {
				puts(revhost);
				if ((writeFile==1) && !strcmp(header,"Name:")) {
					char* cmd[128]; 
					sprintf(cmd, "echo DNSName %s %s >> /var/DnsInfo", dotted, revhost );
					system(cmd);
				}

				if (ENABLE_FEATURE_CLEAN_UP)
					free(revhost);
			}
			if (ENABLE_FEATURE_CLEAN_UP)
				free(dotted);
			cur = cur->ai_next;
		}
		
#if 1//__MSTC__, Ailsa, nslookup, 20110607
		if(r->DNSAddress[0] != '\0')
		{
			r->DNSAddress[strlen(r->DNSAddress)-1] = 0;
		}	
	if (ENABLE_FEATURE_CLEAN_UP)
			freeaddrinfo(revhostname);
#endif	
	} else {
#if 1 //__MSTC__,Ailsa, nslookup, 20110607
	if(cmsUtl_strcmp(header,"Server:") == 0)
			{
				strcpy(r->serverName,"Unknown host");
				strcpy(r->serverAddress,"");
	} else {
				strcpy(r->DNSName,"Unknown host");
				strcpy(r->DNSAddress,"");
			}
			status = NSLOOKUP_ERROR;
#endif
#if ENABLE_VERBOSE_RESOLUTION_ERRORS
		bb_error_msg("can't resolve '%s': %s", hostname, gai_strerror(rc));
#else
		bb_error_msg("can't resolve '%s'", hostname);
#endif
	}
	if (ENABLE_FEATURE_CLEAN_UP)
		freeaddrinfo(result);
	return (rc != 0);
}

/* lookup the default nameserver and display it */
static void server_print(void)
{
	char *server;
	struct sockaddr *sa;

#if ENABLE_FEATURE_IPV6
	sa = (struct sockaddr*)_res._u._ext.nsaddrs[0];
	if (!sa)
#endif
		sa = (struct sockaddr*)&_res.nsaddr_list[0];
	server = xmalloc_sockaddr2dotted_noport(sa);

	print_host(server, "Server:");
	if (ENABLE_FEATURE_CLEAN_UP)
		free(server);
	bb_putchar('\n');
}

/* alter the global _res nameserver structure to use
   an explicit dns server instead of what is in /etc/resolv.conf */
static void set_default_dns(const char *server)
{
	len_and_sockaddr *lsa;

	/* NB: this works even with, say, "[::1]:5353"! :) */
	lsa = xhost2sockaddr(server, 53);

	if (lsa->u.sa.sa_family == AF_INET) {
		_res.nscount = 1;
		/* struct copy */
		_res.nsaddr_list[0] = lsa->u.sin;
	}
#if ENABLE_FEATURE_IPV6
	/* Hoped libc can cope with IPv4 address there too.
	 * No such luck, glibc 2.4 segfaults even with IPv6,
	 * maybe I misunderstand how to make glibc use IPv6 addr?
	 * (uclibc 0.9.31+ should work) */
	if (lsa->u.sa.sa_family == AF_INET6) {
		// glibc neither SEGVs nor sends any dgrams with this
		// (strace shows no socket ops):
		//_res.nscount = 0;
		_res._u._ext.nscount = 1;
		/* store a pointer to part of malloc'ed lsa */
		_res._u._ext.nsaddrs[0] = &lsa->u.sin6;
		/* must not free(lsa)! */
	}
#endif
}

int nslookup_main(int argc, char **argv) MAIN_EXTERNALLY_VISIBLE;
int nslookup_main(int argc, char **argv)
{
	/* We allow 1 or 2 arguments.
	 * The first is the name to be looked up and the second is an
	 * optional DNS server with which to do the lookup.
	 * More than 3 arguments is an error to follow the pattern of the
	 * standard nslookup */
	#if 0 //__MSTC__,Ailsa,nslookup,20110607
	if (!argv[1] || argv[1][0] == '-' || argc > 3)
		bb_show_usage();
    #endif
#if 1//__MSTC__,Ailsa,nslookup,20110607
	int host;
	char hostname[128]={0};
	r = (DNSResult *) malloc(sizeof(DNSResult));
	int c;
	cmsLog_init(EID_NSLOOKUP);
	cmsLog_setLevel(DEFAULT_LOG_LEVEL);
	cmsMsg_init(EID_NSLOOKUP, &msgHandle);
	int i;
#endif
	writeFile = 0;

	/* initialize DNS structure _res used in printing the default
	 * name server and in the explicit name server option feature. */
	res_init();
	#if 1//__MSTC__, Ailsa, nslookup,20110607
	if(argc < 2)
	{
		usage();
		return EXIT_FAILURE;
	}
	while ((c = getopt(argc, argv, "s:h:f:c")) != -1) {
		switch(c) {
			case 's':
				set_default_dns(optarg);
			break;
			case 'h':
				strcpy(hostname,optarg);
			break;
			case 'f':
				requesterId = atoi(optarg);
			break;
			case 'c':
				writeFile = 1;
			break;			
			default:
				usage();
				return EXIT_FAILURE;
			break;
		}
	}

		struct stat st;
		stat("/var/DnsInfo", &st);
		unsigned long size = st.st_size;
		if (size > 1000) {
			system("rm /var/DnsInfo &");
		}
	
		server_print();
		print_host(hostname, "Name:");
		
		printf("Nslookup_end\n");
		
		sendEventMessage();

	free(r);
	return EXIT_SUCCESS;
        #else
	/* rfc2133 says this enables IPv6 lookups */
	/* (but it also says "may be enabled in /etc/resolv.conf") */
	/*_res.options |= RES_USE_INET6;*/

	if (argv[2])
		set_default_dns(argv[2]);

	server_print();
	return print_host(argv[1], "Name:");
	#endif
}
