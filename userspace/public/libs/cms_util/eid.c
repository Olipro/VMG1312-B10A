/***********************************************************************
 *
 *  Copyright (c) 2006-2007  Broadcom Corporation
 *  All Rights Reserved
 *
 * <:label-BRCM:2011:DUAL/GPL:standard
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2, as published by
 * the Free Software Foundation (the "GPL").
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * 
 * A copy of the GPL is available at http://www.broadcom.com/licenses/GPLv2.php, or by
 * writing to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 * 
:>
 *
 ************************************************************************/

#include "cms.h"
#include "cms_util.h"
#include "oal.h"


static CmsEntityInfo entityInfoArray[] = {

   {EID_TR69C,        /* eid */
    NDA_ACCESS_TR69C, /* TR69 attribute access bit */
    "tr69c",          /* name */
    "/bin/tr69c",     /* path */
    "",               /* run args */
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_LAUNCH_ON_BOOT|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE|EIF_IPV6,
#else
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_LAUNCH_ON_BOOT|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
#endif
    1,                /* backlog for TCP server port */
    TR69C_CONN_REQ_PORT,       /* TCP server port */
#ifdef SUPPORT_TR69C
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},         /* misc mem usage parameters */

   {EID_TR64C,
    NDA_ACCESS_TR64C,
    "tr64c",
    "/bin/tr64c",
    "",
#if 0 //__MSTC__, Justin, Support Enable and Disable by smd
    EIF_MDM|EIF_LAUNCH_ON_BOOT,
#else
    EIF_MDM|EIF_MESSAGING_CAPABLE,
#endif
    0,
    TR64C_HTTP_CONN_PORT,
#ifdef SUPPORT_TR64C
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_HTTPD,
    NDA_ACCESS_HTTPD,
    "httpd",
//__MSTC__, Nick Tseng, GUI Debug Mode with FTP
#ifdef SUPPORT_GUI_DEBUG_MODE
    "/var/httpd",
#else
    "/bin/httpd",
#endif
    "",
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE|EIF_IPV6,
#else
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
#endif
    3,                /* backlog for TCP server port */
    HTTPD_PORT,       /* TCP server port */
#ifdef SUPPORT_HTTPD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif

    0, 0, 0},

#ifdef SUPPORT_HTTPD_SSL // __MSTC__, Richard Huang
   {EID_HTTPD_SSL,
    NDA_ACCESS_HTTPD,
    "httpd_ssl",
    "/bin/httpd",
    " -s",
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE|EIF_IPV6,
#else
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
#endif
    3,                /* backlog for TCP server port */
    HTTPD_PORT_SSL,   /* TCP server port */
    TRUE,             /* isFeatureCompiledIn */

    0, 0, 0},
#endif //SUPPORT_HTTPD_SSL

   {EID_SNMPD,
    NDA_ACCESS_SNMPD,
    "snmpd",
    "/bin/snmpd",
    "",
    EIF_MDM|EIF_MESSAGING_CAPABLE,
    /*    EIF_MDM, */
    0,
    SNMPD_PORT,
#ifdef SUPPORT_SNMP
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_CONSOLED,
    NDA_ACCESS_CONSOLED,
    "consoled",
    "/bin/consoled",
    "",
    EIF_MDM|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
    0,
    0,
#ifdef SUPPORT_CONSOLED
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_TELNETD,
    NDA_ACCESS_TELNETD,
    "telnetd",
    "/bin/telnetd",
    "",
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE|EIF_IPV6,
#else
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
#endif
    3,             /* backlog for TCP server port */
    TELNETD_PORT,
#ifdef SUPPORT_TELNETD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_SSHD,
    NDA_ACCESS_SSHD,
    "sshd",
    "/bin/sshd",
    "",
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE|EIF_IPV6,
#else
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
#endif
    3,             /* backlog for TCP server port */
    SSHD_PORT,
#ifdef SUPPORT_SSHD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_UPNP,
    NDA_ACCESS_UPNP,
    "upnp",
    "/bin/upnp",
    "",
    EIF_MDM|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
    0,
    0,
#ifdef SUPPORT_UPNP
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},


   {EID_OMCID,
    0,
    "omcid",
    "/bin/omcid",
    "-v 0 start",
    EIF_LAUNCH_ON_BOOT|EIF_MDM|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
    0,
    0,
#ifdef DMP_X_ITU_ORG_GPON_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},


   /*
    */
   {EID_OMCIPMD,
    0,
    "omcipmd",
    "/bin/omcipmd",
    "",
    EIF_LAUNCH_ON_BOOT|EIF_MDM|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
    0,
    0,
#ifdef DMP_X_ITU_ORG_GPON_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

#ifndef DMP_X_ITU_ORG_GPON_1
    {EID_CVTPSI,
     0,
     "cvtpsi",
     "/bin/cvtpsi",
     NULL,
     0,
     0,
     0,
     FALSE,            /* isFeatureCompiledIn */
     0, 0, 0},
#endif

   {EID_VECTORINGD,
    NDA_ACCESS_VECTORINGD,
    "vectoringd",
    "/bin/vectoringd",
    NULL,
    EIF_MDM|EIF_MESSAGING_CAPABLE|EIF_LAUNCH_ON_BOOT,
    0,
    0,
#ifdef SUPPORT_VECTORINGD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_SMD,
    0,
    "smd",
    "/bin/smd",
    "",
    EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,   /* flags */
    0,
    0,
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_SSK,
    0,
    "ssk",
    "/bin/ssk",
    "",
    EIF_LAUNCH_IN_STAGE_1|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_PPP,
    0,
    "pppd",
    "/bin/pppd",
    "-c",
    EIF_MESSAGING_CAPABLE|EIF_MULTIPLE_INSTANCES,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_DHCPC,
    0,
    "dhcpc",
    "/bin/dhcpc",
    "-f",  /* stay in foreground, we don't want it to fork or daemonize */
    EIF_MESSAGING_CAPABLE|EIF_MULTIPLE_INSTANCES,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_DHCPD,
    0,
    "dhcpd",
    "/bin/dhcpd",
    "",
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef SUPPORT_UDHCP
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_FTPD,
    0,
    "bftpd",
    "/bin/bftpd",
    "",
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE|EIF_IPV6,
#else
    EIF_SERVER|EIF_SERVER_TCP|EIF_MESSAGING_CAPABLE,
#endif
    1, /* backlog for TCP server port */
    FTPD_PORT,
#ifdef SUPPORT_FTPD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_TFTPD,
    0,
    "tftpd",
    "/usr/bin/tftpd",
    "",
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    EIF_SERVER|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE|EIF_IPV6,
#else
    EIF_SERVER|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
#endif
    0,
    TFTPD_PORT,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

    {EID_TFTP,  /* this is the client */
    0,
    "tftp",
    "/usr/bin/tftp",
    "",
    EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_DNSPROBE,
    0,                  /* TR69 attribute access bit */
    "dnsprobe",
    "/bin/dnsprobe",
    "",                 /* run args */
    0,                  /* EIF_ flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

   {EID_DNSPROXY,
    0,
    "dnsproxy",
    "/bin/dnsproxy",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE|EIF_AUTO_RELAUNCH,  /* EIF_ flags */
    0,
    0,
#ifdef DMP_X_5067F0_DNSPROXY_1 /* aka SUPPORT_DNSPROXY */
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_SYSLOGD,
    0,                  /* TR69 attribute access bit */
    "syslogd",
    "/sbin/syslogd",
    "-n",               /* run args (don't daemonize) */
    0,                  /* flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

   {EID_KLOGD,
    0,                  /* TR69 attribute access bit */
    "klogd",
    "/sbin/klogd",
    "-n",               /* run args (don't daemonize) */
    0,                  /* flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

   {EID_VODSL,
    NDA_ACCESS_VODSL,   /* tr69 attribute access bit, not applicable to vodsl */
    "vodsl",
    "/bin/vodsl",
    "",           /* run args */
    EIF_MDM|EIF_MESSAGING_CAPABLE|EIF_AUTO_RELAUNCH,
    0,            /* backlog for TCP server port, if this is a tcp server */
    0,            /* TCP/UDP server port */
#ifdef SUPPORT_VOXXXLOAD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},     /* misc memory parameters */
#ifdef  SUPPORT_IPTK_SIP_IPV6 /* For Aricent IPTK ICF process, chchien */
 {EID_ICF,
    0,                  /* Attribute access bit */
    "icf",
    "/bin/icf.exe",
    "",                 /* run args */
    EIF_LAUNCH_ON_BOOT,                  /* EIF_ flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */
#endif

#ifdef ISKON_CUSTOMIZATION
   {EID_CROND,
    0,
    "crond",
    "/usr/sbin/crond",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},
#endif

#if 1	/* MSTC_VOICE */
   {EID_ZYIMS,
    NDA_ACCESS_ZYIMS,   /* tr69 attribute access bit, not applicable to zyims */
    "zyims",
    "/bin/voiceApp",
    "",           /* run args */
    EIF_LAUNCH_ON_BOOT|EIF_MDM|EIF_MESSAGING_CAPABLE,
    0,            /* backlog for TCP server port, if this is a tcp server */
    0,            /* TCP/UDP server port */
#ifdef VOXXXLOAD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},     /* misc memory parameters */
#endif

   {EID_DECTDBGD,
    NDA_ACCESS_DECTDBGD,   /* tr69 attribute access bit, not applicable to vodsl */
    "dectdbgd_NEW",
    "/etc/dectdbgd",
    "",           /* run args */
    EIF_MDM|EIF_MESSAGING_CAPABLE,
    0,            /* backlog for TCP server port, if this is a tcp server */
    0,            /* TCP/UDP server port */
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},     /* misc memory parameters */

#if 1 //__MSTC__, Kenny, SUPPORT_DNSMASQ
	  {EID_DNSMASQ,
	   0, 
	   "dnsmasq",
	   "/bin/dnsmasq",
	   #if 0 //__MSTC__, DNSRoute,  Eason, enable debug
	   "-d -q",
	   #else
	   "",				   /* run args */
	   #endif
	   EIF_MESSAGING_CAPABLE,				   /* EIF_ flags */
	   0, 
	   0, 
#ifdef SUPPORT_DNSMASQ /* SUPPORT_DNSMASQ */
	   TRUE,			 /* isFeatureCompiledIn */
#else
	   FALSE,			 /* isFeatureCompiledIn */
#endif
	   0, 0, 0}, 
#endif
   {EID_DDNSD,
    0,                  /* TR69 attribute access bit */
#ifdef MSTC_DDNS_INADYN /* Support DDNS with inadyn, __FTTB7-CHT__, MitraStar chiahsin, 20120703 */
    "inadyn",
    "/bin/inadyn",
#else
    "ddnsd",
    "/bin/ddnsd",
#endif
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* flags (later make it desktop capable) */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

    {EID_DDNSD2,        /* Support DDNS with ez-ipupdate, __MSTC__, yic */
    0,                  /* TR69 attribute access bit */
    "ez-ipupdate",
    "/bin/ez-ipupdate",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* flags (later make it desktop capable) */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

   {EID_ZEBRA,
    0,                  /* TR69 attribute access bit */
    "zebra",
    "/bin/zebra",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* flags (later make it desktop capable) */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

   {EID_RIPD,
    0,                  /* TR69 attribute access bit */
    "ripd",
    "/bin/ripd",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* flags (later make it desktop capable) */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

   {EID_SNTP,
    0,                  /* TR69 attribute access bit */
    "sntp",
    "/bin/sntp",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* flags (later make it desktop capable) */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */
#if 1 // __MSTC__,Amy, Parental Control
   {EID_WEBIPQ,
    0,                       /* TR69 attribute access bit */
    "webipqd",
    "/bin/webipqd",
    "",                      /* run args (don't daemonize) */
    EIF_MESSAGING_CAPABLE,   /* flags */
    0,                       /* backlog for TCP server port */
    0,                       /* TCP/UDP server port */
    TRUE,                    /* isFeatureCompiledIn */
    0, 0, 0},                /* misc memory parameters */
#endif
#if 1  /* __MSTC__, Ailsa, Support nslookup, 20110607*/
	   {EID_NSLOOKUP,
		0,
		"nslookup",
		"/usr/bin/nslookup",
		"", 				/* run args */
		EIF_MESSAGING_CAPABLE,	/* EIF_ flags */
		0,					/* backlog for TCP server port */
		0,					/* TCP/UDP server port */
		TRUE,				/* isFeatureCompiledIn */
		0, 0, 0},			/* misc memory parameters */
#endif  /* __MSTC__, Ailsa, Support nslookup*/
   {EID_URLFILTERD,
    0,
    "urlfilterd",
    "/bin/urlfilterd",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* flags (later make it desktop capable) */
    0,
    0,
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_DHCP6C,
    0,
    "dhcp6c",
    "/bin/dhcp6c",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE|EIF_MULTIPLE_INSTANCES,
    0,
    0,
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_DHCP6S,
    0,
    "dhcp6s",
    "/bin/dhcp6s",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_RADVD,
    0,
    "radvd",
    "/bin/radvd",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_RASTATUS6,
    0,
    "rastatus6",
    "/bin/rastatus6",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef DMP_X_5067F0_IPV6_1 /* aka SUPPORT_IPV6 */
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_IPPD,
    0,
    "ippd",
    "/bin/ippd",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef SUPPORT_IPP
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_DSLDIAGD,
    0,
    "dsldiagd",
    "/bin/dsldiagd",
    "",                 /* run args */
    EIF_LAUNCH_ON_BOOT,
    0,
    0,
#ifdef SUPPORT_DSLDIAGD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_SOAPSERVER,
    0,
    "soapserver",
    "/bin/soapserver",
    "-f",                /* run args */
    EIF_LAUNCH_ON_BOOT,
    0,
    0,
#ifdef SUPPORT_SOAP
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

    {EID_FTP,   /* this is FTP the client */
    0,
    "ftp",
    "/usr/bin/ftp",
    "",
    EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_UNITTEST,
    0,
    "ut",
    "/",
    "",
    0,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_PING,
    0,
    "ping",
    "/bin/ping",
    "",                 /* run args */
#ifdef DMP_X_ITU_ORG_GPON_1
    EIF_MESSAGING_CAPABLE | EIF_MULTIPLE_INSTANCES,  /* EIF_ flags */
#else
    EIF_MESSAGING_CAPABLE ,  /* EIF_ flags */
#endif
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

   {EID_TRACERT,
    0,
    "traceroute",
    "/usr/bin/traceroute",
    "",                 /* run args */
#ifdef DMP_X_ITU_ORG_GPON_1
    EIF_MESSAGING_CAPABLE | EIF_MULTIPLE_INSTANCES,  /* EIF_ flags */
#else
    EIF_MESSAGING_CAPABLE ,  /* EIF_ flags */
#endif
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
#ifdef DMP_X_ITU_ORG_GPON_1
    TRUE,               /* isFeatureCompiledIn */
#else
    FALSE,
#endif
    0, 0, 0},           /* misc memory parameters */

   {EID_PWRCTL,
    0,
    "pwrctl",
    "/bin/pwrctl",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */

   {EID_HOTPLUG,
    0,
    "hotplug",
    "/bin/hotplug",
    "",
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef DMP_STORAGESERVICE_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_SAMBA,
    0,
    "smbd",
    "/bin/smbd",
#if 1 //__MSTC__, Kenny, Samba build in enable
      "-D -s /var/samba/smb.conf -l=/var/samba --lockdir=/var/samba --piddir=/var/samba --private-dir=/var/samba",                 /* run args */
#else
    "-D",
#endif
    0,
    0,
    0,
#ifdef SUPPORT_SAMBA
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},
#if 1 // __MSTC__, Richard Huang, for CPU usg
   {EID_CPULOAD,
    0,
    "",
    "/bin/cpuload",
    "",                /* run args */
    EIF_MESSAGING_CAPABLE, /* EIF_ flags */
    0,               /* backlog for TCP server port */
    0,               /* TCP/UDP server port */
    TRUE,            /* isFeatureCompiledIn */
    0, 0, 0},        /* misc memory parameters */ 
#endif
#ifdef MSTC_IPV6 // __MSTC__, DingRuei, for DHCPv6 relay
   {EID_DHCP6RELAY,
    0,
    "dhcp6relay",
    "/bin/dhcp6relay",
    "",                /* run args */
    EIF_MESSAGING_CAPABLE, /* EIF_ flags */
    0,               /* backlog for TCP server port */
    0,               /* TCP/UDP server port */
#ifdef DMP_X_5067F0_IPV6_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},        /* misc memory parameters */ 
#endif
   {EID_MISC,
    0,
    "misc",
    "/",
    "",
    0,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_WLMNGR,
    NDA_ACCESS_WLAN,
    "WLMNGR-DAEMON",
    "/bin/wlmngr",
    "",
    EIF_MDM|EIF_MESSAGING_CAPABLE, //|EIF_DESKTOP_LINUX_CAPABLE |EIF_LAUNCH_ON_BOOT,
    0,
    0,
#ifdef BRCM_WLAN
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,             /* isFeatureCompiledIn */
#endif
    0, 0, 0},

 {EID_WLNVRAM,
    NDA_ACCESS_WLAN,
    "WLNVRAM",
    "/bin/nvram",
    "",
    EIF_MDM|EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef BRCM_WLAN
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,             /* isFeatureCompiledIn */
#endif
    0, 0, 0},
    
   {EID_WLEVENT,
    NDA_ACCESS_WLAN,
    "WLEVENT",
    "/bin/wlevt",
    "",
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef BRCM_WLAN
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,             /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_WLWPS,
    NDA_ACCESS_WLAN,
    "WLWPS",
    "/bin/wps_monitor",
    "",
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef BRCM_WLAN
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,             /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_WLWAPID,
    NDA_ACCESS_WLAN,
    "WLWAPID",
    "/bin/wapid",
    "",
    EIF_MESSAGING_CAPABLE, //|EIF_DESKTOP_LINUX_CAPABLE |EIF_LAUNCH_ON_BOOT,
    0,
    0,
#ifdef BCMWAPI_WAI
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,             /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_CMFD,
    0,
    "cmfd",
    "/bin/cmfd",
    "",
    EIF_LAUNCH_ON_BOOT|EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef SUPPORT_CMFD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_MCPD,
    0,
    "mcpd",
    "/bin/mcpd",
    "",                 /* run args */
    EIF_MDM | EIF_MESSAGING_CAPABLE,  /* flags (later make it desktop capable) */
    0,
    0,
#if defined(SUPPORT_IGMP) || defined (SUPPORT_MLD)
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_MCPCTL,
    0,
    "mcpctl",
    "/bin/mcpctl",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* flags (later make it desktop capable) */
    0,
    0,
#if defined(SUPPORT_IGMP) || defined (SUPPORT_MLD)
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_L2TPD,
    0,
    "l2tpd",
    "/bin/openl2tpd",
    "",                 /* run args */
    0,
    0,
    0,
#ifdef DMP_X_5067F0_L2TPAC_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_PPTPD,
    0,
    "pptp",
    "/bin/pptp",
    "",                 /* run args */
    0,
    0,
    0,
#ifdef DMP_X_5067F0_PPTPAC_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_MOCAD,
    0,
    "mocad",
    "/bin/mocad",
    "",
    EIF_MESSAGING_CAPABLE|EIF_MULTIPLE_INSTANCES,
    0,
    0,
#ifdef BRCM_MOCA_DAEMON
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_RNGD,
    0,
    "rngd",
    "/bin/rngd",
    "",                 /* run args */
    EIF_LAUNCH_ON_BOOT,
    0,
    0,
#ifdef SUPPORT_HWRANDOM
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},
#ifdef MSTC_VCAUTOHUNT //__MSTC__, Eric, VCAUTOHUNT
   {EID_VCAUTOHUNT,
    NDA_ACCESS_HTTPD,
    "vcautohunt",
    "/bin/vcautohunt",
    "",                 /* run args */
    EIF_MDM|EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */
#endif //MSTC_VCAUTOHUNT
#ifdef MSTC_VLANAUTOHUNT //__MSTC__, Jeff
   {EID_VLANAUTOHUNT,
    NDA_ACCESS_HTTPD,
    "vlanautohunt",
    "/bin/vlanautohunt",
    "",                 /* run args */
    EIF_MDM|EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */
#endif
#ifdef  MSTC_AUTO_SCHEDULING
	  {EID_SCHEDULE,
	   NDA_ACCESS_CVTPSI,
	   "schedule",
	   "/bin/autoscheduling",
	   "",				   /* run args */
	   EIF_MESSAGING_CAPABLE| EIF_MDM, /* EIF_ flags */
	   0,				 /* backlog for TCP server port */
	   0,				 /* TCP/UDP server port */
	   TRUE,			 /* isFeatureCompiledIn */
	   0, 0, 0},		 /* misc memory parameters */
   
#endif
   {EID_DMSD,
    0,
    "bcmmserver",
    "/bin/bcmmserver",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef DMP_X_5067F0_DLNA_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_SWMDK,
    //NDA_ACCESS_SWMDK,
    0,
    "swmdk",
    "/bin/swmdk",
    "",                 /* run args */
    EIF_LAUNCH_ON_BOOT,
    0,
    0,
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},

   {EID_EPON_APP,
    NDA_ACCESS_EPONAPP,
    "eponapp",
    "/bin/eponapp",
    "",                 /* run args */
    EIF_MDM | EIF_LAUNCH_ON_BOOT | EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef DMP_X_5067F0_EPON_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,             /* isFeatureCompiledIn */
#endif
    0, 0, 0},

#if 1 //__MSTC__, Justin, Support TR143
   {EID_UDPECHOD,     /*eid*/
   	NDA_ACCESS_UDPECHOD,   /* TR69 attribute access bit */
    "udpechod",         /*name*/
    "/bin/udpechod",    /*path*/
    "",                 /* run args (don't daemonize) */
    EIF_MDM|EIF_MESSAGING_CAPABLE, /* flags */
    0,                  /* backlog for TCP server port */
    2345,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */
    
   {EID_UPLOADD,       /*eid*/
	NDA_ACCESS_UPLOADD,   /* TR69 attribute access bit */
    "uploadd",         /*name*/
    "/bin/uploadd",    /*path*/
    "",                 /* run args (don't daemonize) */
    EIF_MDM|EIF_MESSAGING_CAPABLE, /* flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},            /* misc memory parameters */
    
   {EID_WGET,
    0,
    "wget",
    "/usr/bin/wget",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */
#endif

#ifdef SUPPORT_CONFIGURATION_FILTER
	{EID_ZYCFGFILTER,
    0,
    "zycfgfilter",
    "/bin/zycfgfilter",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
    TRUE,               /* isFeatureCompiledIn */
    0, 0, 0},           /* misc memory parameters */
#endif   

#if 1 /* Jennifer, software watchdog*/
{EID_SW_WATCHDOG,
 0,
 "watchdog",
 "/bin/watchdog",
 "",				 /* run args */
 EIF_MDM|EIF_LAUNCH_ON_BOOT|EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
 0, 				 /* backlog for TCP server port */
 0, 				 /* TCP/UDP server port */
#ifdef MSTC_SW_WATCHDOG /* Jennifer, software watchdog*/
 TRUE,				 /* isFeatureCompiledIn */
#endif
 0, 0, 0},			 /* misc memory parameters */
#endif

{EID_OSGID,        /* eid */
    NDA_ACCESS_TR69C, /* attribute access bit */
    "osgid",          /* name */
    "/bin/osgid",     /* path */
    "",               /* run args */
    EIF_MDM|EIF_SERVER|EIF_SERVER_TCP|EIF_LAUNCH_ON_BOOT|EIF_MESSAGING_CAPABLE|EIF_DESKTOP_LINUX_CAPABLE,
    0,                /* backlog for TCP server port: TO_DO, should be NULL? */
    0,       /* TCP server port: TO_DO; should be NULL? unless felix is sending somethign up*/
#ifdef DMP_X_5067F0_OSGI_JVM_1
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},         /* misc mem usage parameters */

   {EID_DECT,
    NDA_ACCESS_VODSL,   /* tr69 attribute access bit, not applicable to vodsl */
    "dectd",
    "/bin/dectd",
    "",           /* run args */
    EIF_MDM|EIF_MESSAGING_CAPABLE,
    0,            /* backlog for TCP server port, if this is a tcp server */
    0,            /* TCP/UDP server port */
#ifdef SUPPORT_VOXXXLOAD
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},     /* misc memory parameters */

   {EID_BMUD,
    0,
    "bmud",
    "/bin/bmud",
    "",                 /* run args */
    EIF_LAUNCH_ON_BOOT|EIF_MESSAGING_CAPABLE,
    0,
    0,
#ifdef SUPPORT_BMU
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},

   {EID_BMUCTL,
    0,
    "bmuctl",
    "/bin/bmuctl",
    "",                 /* run args */
    EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
    0,                  /* backlog for TCP server port */
    0,                  /* TCP/UDP server port */
#ifdef SUPPORT_BMU
    TRUE,             /* isFeatureCompiledIn */
#else
    FALSE,            /* isFeatureCompiledIn */
#endif
    0, 0, 0},           /* misc memory parameters */

    {EID_MODUPDTD,
     0,
     "modupdtd",
     "/bin/modupdtd",
     "",                 /* run args */
     EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
     0,                  /* backlog for TCP server port */
     0,                  /* TCP/UDP server port */
 #ifdef SUPPORT_MOD_SW_UPDATE
     TRUE,             /* isFeatureCompiledIn */
 #else
     FALSE,            /* isFeatureCompiledIn */
 #endif
     0, 0, 0},           /* misc memory parameters */

    {EID_MODUPDTV,
      0,
      "modupdtv",
      "",                 /* this app is never launched by smd */
      "",                 /* run args */
      EIF_MESSAGING_CAPABLE,  /* EIF_ flags */
      0,                  /* backlog for TCP server port */
      0,                  /* TCP/UDP server port */
  #ifdef SUPPORT_MOD_SW_UPDATE
      TRUE,             /* isFeatureCompiledIn */
  #else
      FALSE,            /* isFeatureCompiledIn */
  #endif
      0, 0, 0},           /* misc memory parameters */
#if DMP_X_5067F0_MSTC_WWAN_1 // __MSTC__, Richard Huang, For 3G WAN, __TELEFONICA__, MitraStar, Chehuai */      
   {EID_LINKUPDOWN,
      0,
      "link_updown",
      "/bin/link_updown",
      "",                       /* run args */
      EIF_LAUNCH_ON_BOOT,
      /* EIF_ flags */
      0,              /* backlog for TCP server port */
      0,              /* TCP/UDP server port */
#ifdef DMP_X_5067F0_MSTC_WWAN_1
      TRUE,           /* isFeatureCompiledIn */
#else
      FALSE,          /* isFeatureCompiledIn */
#endif
      0, 0, 0},  
          
   {EID_CELLD,
      NDA_ACCESS_CELLD, 
      "celld",
      "/bin/celld",
      "",             /* run args */
      EIF_MESSAGING_CAPABLE|EIF_MDM |EIF_LAUNCH_ON_BOOT,//   0,
      /* EIF_ flags */
      0,              /* backlog for TCP server port */
      0,              /* TCP/UDP server port */
#ifdef DMP_X_5067F0_MSTC_WWAN_1
      TRUE,           /* isFeatureCompiledIn */
#else
      FALSE,          /* isFeatureCompiledIn */
#endif
      0, 0, 0},
#endif // #if 1 // __MSTC__, Richard Huang

#if 1	//__MSTC__, Amy, 802.1x
   {EID_WPASUPPLICANT,
    0,  
    "wpa_supplicant",
    "/bin/wpa_supplicant",
    "-wW -c /etc/wpa_supplicant.conf",           /* run args */
    EIF_MESSAGING_CAPABLE,
    //EIF_MESSAGING_CAPABLE|EIF_MULTIPLE_INSTANCES,
    0,            /* backlog for TCP server port, if this is a tcp server */
    0,            /* TCP/UDP server port */
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},     /* misc memory parameters */

	{EID_LINKSTATUS,/* __MSTC__, chiayi */
	0, 
	"linkstatus",
	"/bin/linkstatus",
	"",            /* run args */
	EIF_LAUNCH_ON_BOOT,//   0,
	/* EIF_ flags */
	0,              /* backlog for TCP server port */
	0,              /* TCP/UDP server port */
	TRUE,
	0, 0, 0}, 
#endif

#if 1	// __ZyXEL__, Albert, 20140402, [Feature Enhancement] Add Mobile App Manager
   {EID_MOBILEAPP,
    NDA_ACCESS_TR69C,  
    "EmappS",
    "/bin/EmappS",
    "",           /* run args */
    EIF_MDM | EIF_LAUNCH_ON_BOOT | EIF_MESSAGING_CAPABLE,
    //EIF_MESSAGING_CAPABLE|EIF_MULTIPLE_INSTANCES,
    3,            /* backlog for TCP server port, if this is a tcp server */
    263,            /* TCP/UDP server port */
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},     /* misc memory parameters */

#endif
#if 1	// __ZyXEL__, Ryan, for PPTP
   {EID_VPPPD,
    0,  
    "pppd2.4.4",
    "/usr/bin/pppd2.4.4",
    "",           /* run args */
    EIF_MDM | EIF_LAUNCH_ON_BOOT | EIF_MESSAGING_CAPABLE,
    //EIF_MESSAGING_CAPABLE|EIF_MULTIPLE_INSTANCES,
    0,            /* backlog for TCP server port, if this is a tcp server */
    0,            /* TCP/UDP server port */
    TRUE,             /* isFeatureCompiledIn */
    0, 0, 0},     /* misc memory parameters */

#endif


};


#define NUM_ENTITY_INFO_ENTRIES (sizeof(entityInfoArray)/sizeof(CmsEntityInfo))


UINT32 cmsEid_getNumberOfEntityInfoEntries(void)
{
   return NUM_ENTITY_INFO_ENTRIES;
}


const CmsEntityInfo *cmsEid_getFirstEntityInfo(void)
{
   return &(entityInfoArray[0]);
}

#if 1 //__MSTC__, Dennis
UBOOL8 cmsEid_setEntityCompiledIn(CmsEntityId eid, UBOOL8 flag)
{
   UINT32 i=0;

   while (i < NUM_ENTITY_INFO_ENTRIES)
   {
      if (entityInfoArray[i].eid == eid)
      {
         entityInfoArray[i].isFeatureCompiledIn = flag;
         return TRUE;
      }

      i++;
   }

   return FALSE;
}

UBOOL8 cmsEid_setEntityPort(CmsEntityId eid, int port)
{
   UINT32 i=0;

   while (i < NUM_ENTITY_INFO_ENTRIES)
   {
      if (entityInfoArray[i].eid == eid)
      {
         entityInfoArray[i].port = port;
         return TRUE;
      }

      i++;
   }

   return FALSE;
}

CmsEntityId cmsEid_getEidByPort(int port, CmsEntityId exceptEid)
{
   UINT32 i=0;

   while (i < NUM_ENTITY_INFO_ENTRIES)
   {
      if (entityInfoArray[i].port == port 
	  	&& (exceptEid == EID_INVALID || entityInfoArray[i].eid != exceptEid) )
      {
         return (entityInfoArray[i].eid);
      }

      i++;
   }

   return EID_INVALID;
}

#endif


const CmsEntityInfo *cmsEid_getEntityInfo(CmsEntityId eid)
{
   UINT32 i=0;

   while (i < NUM_ENTITY_INFO_ENTRIES)
   {
      if (entityInfoArray[i].eid == eid)
      {
         return (&entityInfoArray[i]);
      }

      i++;
   }

   return NULL;
}


const CmsEntityInfo *cmsEid_getEntityInfoByAccessBit(UINT16 bit)
{
   UINT32 i=0;

   if (bit == 0)
   {
      return NULL;
   }

   while (i < NUM_ENTITY_INFO_ENTRIES)
   {
      if (entityInfoArray[i].accessBit == bit)
      {
         return (&entityInfoArray[i]);
      }

      i++;
   }

   return NULL;
}


const CmsEntityInfo *cmsEid_getEntityInfoByStringName(const char *name)
{
   UINT32 i=0;

   if (name == NULL)
   {
      return NULL;
   }

   while (i < NUM_ENTITY_INFO_ENTRIES)
   {
      if (!strcmp(entityInfoArray[i].name, name))
      {
         return (&entityInfoArray[i]);
      }

      i++;
   }

   return NULL;
}


CmsRet cmsEid_getStringNamesFromBitMask(UINT16 bitMask, char **buf)
{
   UINT32 i, mask;
   UINT32 len=1; /* for empty mask, return a buffer with an empty string. */
   UINT32 idx=0;
   UINT32 numBits = sizeof(bitMask) * 8;
   const CmsEntityInfo *info;
   UBOOL8 firstName=TRUE;
   CmsRet ret = CMSRET_SUCCESS;

   /* first loop calculates how much space needed for string names */
   for (mask=1, i=0; i < numBits; i++)
   {
      info = cmsEid_getEntityInfoByAccessBit(bitMask & mask);
      if (info == NULL)
      {
         if (bitMask & mask)
         {
            cmsLog_error("unrecognized bitmask 0x%x", (bitMask & mask));
            ret = CMSRET_SUCCESS_UNRECOGNIZED_DATA_IGNORED;
         }
      }
      else
      {
         len += strlen(info->name) + 1;
      }

      mask = mask << 1;
   }

   if (((*buf) = cmsMem_alloc(len, ALLOC_ZEROIZE)) == NULL)
   {
      return CMSRET_RESOURCE_EXCEEDED;
   }

   /* this loop copies string names into buffer */
   for (mask=1, i=0; i < numBits; i++)
   {
      info = cmsEid_getEntityInfoByAccessBit(bitMask & mask);
      if (info != NULL)
      {
         if (firstName)
         {
            idx = sprintf((*buf), "%s", info->name);
            firstName = FALSE;
         }
         else
         {
            idx += sprintf(&((*buf)[idx]), ",%s", info->name);
         }
      }

      mask = mask << 1;
   }

   cmsAst_assert(idx < len);

   return ret;
}



CmsRet cmsEid_getBitMaskFromStringNames(const char *buf, UINT16 *bitMask)
{
   const char *start;
   const char *end;
   const CmsEntityInfo *info;
   CmsRet ret = CMSRET_SUCCESS;

   if (bitMask == NULL)
   {
      return CMSRET_INVALID_ARGUMENTS;
   }

   *bitMask = 0;

   if ((buf == NULL) || (strlen(buf) == 0))
   {
      /* null or empty string means no bits are set */
      return CMSRET_SUCCESS;
   }

   start = buf;
   end = strchr(start, ',');
   while (end != NULL)
   {
      char name[BUFLEN_256]; /* this should be long enough to hold string names */

      cmsAst_assert((end - start + 1) < (SINT32) sizeof(name));
      snprintf(name, end - start + 1, "%s", start);
      info = cmsEid_getEntityInfoByStringName(name);
      if (info == NULL)
      {
         cmsLog_debug("ignoring name %s", name);
         ret = CMSRET_SUCCESS_UNRECOGNIZED_DATA_IGNORED;
      }
      else
      {
         (*bitMask) |= info->accessBit;
      }

      start = end+1;
      while ((*start == ' ') && (*start != 0))
      {
         start++;
      }

      if (*start != 0)
      {
         end = strchr(start, ',');
      }
      else
      {
         end = NULL;
      }
   }

   /* there is one more name at the end (with no trailing comma) */
   info = cmsEid_getEntityInfoByStringName(start);
   if (info == NULL)
   {
      cmsLog_debug("ignoring name %s", start);
      ret = CMSRET_SUCCESS_UNRECOGNIZED_DATA_IGNORED;
   }
   else
   {
      (*bitMask) |= info->accessBit;
   }



   return ret;
}


SINT32 cmsEid_getPid(void)
{
   return (oal_getPid());
}
